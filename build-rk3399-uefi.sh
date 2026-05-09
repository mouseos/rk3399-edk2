#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKSPACE="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_TYPE="DEBUG"
TOOLCHAIN="GCC5"
GCC5_AARCH64_PREFIX="${GCC5_AARCH64_PREFIX:-aarch64-linux-gnu-}"
SKIP_FETCH=0

EDK2_COMMIT="46f4c9677c615d862649459392f8f55b3e6567c2"
EDK2_NON_OSI_COMMIT="1e2ca640be54d7a4d5d804c4f33894d099432de3"
EDK2_PLATFORMS_COMMIT="861c200cda1417539d46fe3b1eba2b582fa72cbb"

usage() {
  cat <<'EOF'
Usage:
  ./build-rk3399-uefi.sh [options]

Fetch the README-pinned EDK2 trees, place rk3399-edk2 at
edk2-platforms/Platform/Rockchip, build BaseTools, build RK3399 UEFI, and pack
Build/Rk3399-SDK/<BUILD>_GCC5/FV/RK3399_SDK_UEFI.fd into RK3399_SDK_UEFI.img.

Options:
      --workspace PATH    Workspace root, default: parent of this repository
      --build-type NAME   DEBUG or RELEASE, default: DEBUG
      --toolchain NAME    EDK2 toolchain, default: GCC5
      --prefix PREFIX     Cross compiler prefix, default: aarch64-linux-gnu-
      --skip-fetch        Do not clone/fetch/check out EDK2 repositories
  -h, --help              Show this help

Required Debian/Ubuntu packages:
  build-essential acpica-tools nasm uuid-dev gcc-aarch64-linux-gnu git
EOF
}

log() {
  printf '[rk3399-build] %s\n' "$*"
}

die() {
  printf '[rk3399-build] error: %s\n' "$*" >&2
  exit 1
}

abs_path() {
  local path="$1"
  if [[ "$path" = /* ]]; then
    printf '%s\n' "$path"
  else
    printf '%s/%s\n' "$PWD" "$path"
  fi
}

need_tool() {
  command -v "$1" >/dev/null 2>&1 || return 1
}

check_host_tools() {
  local missing=()
  local cross_gcc="${GCC5_AARCH64_PREFIX}gcc"

  for tool in git make gcc uuidgen "$cross_gcc" nasm iasl; do
    if ! need_tool "$tool"; then
      missing+=("$tool")
    fi
  done

  if (( ${#missing[@]} > 0 )); then
    printf '[rk3399-build] missing host tools: %s\n' "${missing[*]}" >&2
    printf '[rk3399-build] install on Debian/Ubuntu with:\n' >&2
    printf '  sudo apt-get install build-essential acpica-tools nasm uuid-dev gcc-aarch64-linux-gnu git\n' >&2
    exit 1
  fi
}

clone_or_update() {
  local url="$1"
  local dir="$2"
  local commit="$3"

  if [[ ! -d "$dir/.git" ]]; then
    log "cloning $(basename "$dir")"
    git clone "$url" "$dir"
  else
    log "fetching $(basename "$dir")"
    git -C "$dir" fetch --tags origin
  fi

  log "checking out $(basename "$dir") @ $commit"
  git -C "$dir" checkout --detach "$commit"
}

prepare_repositories() {
  mkdir -p "$WORKSPACE"

  if [[ "$SKIP_FETCH" -eq 0 ]]; then
    clone_or_update https://github.com/tianocore/edk2.git \
      "$WORKSPACE/edk2" "$EDK2_COMMIT"
    clone_or_update https://github.com/tianocore/edk2-non-osi.git \
      "$WORKSPACE/edk2-non-osi" "$EDK2_NON_OSI_COMMIT"
    clone_or_update https://github.com/tianocore/edk2-platforms.git \
      "$WORKSPACE/edk2-platforms" "$EDK2_PLATFORMS_COMMIT"
  fi

  [[ -f "$WORKSPACE/edk2/edksetup.sh" ]] || die "edk2 tree not found: $WORKSPACE/edk2"
  [[ -d "$WORKSPACE/edk2-platforms/Platform" ]] || die "edk2-platforms tree not found: $WORKSPACE/edk2-platforms"

  if [[ -e "$WORKSPACE/edk2-platforms/Platform/Rockchip" && ! -L "$WORKSPACE/edk2-platforms/Platform/Rockchip" ]]; then
    die "$WORKSPACE/edk2-platforms/Platform/Rockchip exists and is not a symlink"
  fi

  ln -sfn "$SCRIPT_DIR" "$WORKSPACE/edk2-platforms/Platform/Rockchip"
}

build_firmware() {
  export WORKSPACE
  export GCC5_AARCH64_PREFIX
  export PACKAGES_PATH="$WORKSPACE/edk2:$WORKSPACE/edk2-platforms:$WORKSPACE/edk2-non-osi"

  log "WORKSPACE=$WORKSPACE"
  log "PACKAGES_PATH=$PACKAGES_PATH"

  # Older edksetup.sh reads unset variables; keep the main script strict and
  # relax nounset only while importing the EDK2 environment.
  set +u
  # shellcheck source=/dev/null
  source "$WORKSPACE/edk2/edksetup.sh"
  set -u
  cd "$WORKSPACE"

  local tools_def="$WORKSPACE/edk2/Conf/tools_def.txt"
  sed -i 's/\r//g' "$tools_def"
  sed -i '/^[[:space:]]*-Wno-stringop-overflow$/d' "$tools_def"
  sed -i 's#$(WORKSPACE)/ArmPkg/Library/GccLto#$(EDK_TOOLS_PATH)/Bin/GccLto#g' "$tools_def"
  if ! grep -q -- "-Wno-stringop-overflow" "$tools_def"; then
    log "patching tools_def.txt for modern GCC warnings"
    sed -i \
      's/\(DEFINE GCC_ALL_CC_FLAGS *=.*\)$/\1 -Wno-stringop-overflow/' \
      "$tools_def"
  fi

  log "building BaseTools"
  make -C edk2/BaseTools EXTRA_OPTFLAGS="-Wno-vla-parameter -Wno-use-after-free -Wno-dangling-pointer"

  log "building RK3399 UEFI ($BUILD_TYPE/$TOOLCHAIN)"
  build -a AARCH64 -t "$TOOLCHAIN" \
    -p edk2-platforms/Platform/Rockchip/Rk3399Pkg/Rk3399-SDK.dsc \
    -b "$BUILD_TYPE"

  local fd="$WORKSPACE/Build/Rk3399-SDK/${BUILD_TYPE}_${TOOLCHAIN}/FV/RK3399_SDK_UEFI.fd"
  local img="$WORKSPACE/RK3399_SDK_UEFI.img"
  [[ -f "$fd" ]] || die "firmware fd not found: $fd"

  log "packing $img"
  "$SCRIPT_DIR/Rk3399Pkg/Tools/loaderimage" --pack --uboot "$fd" "$img"

  log "firmware fd: $fd"
  log "packed image: $img"
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --workspace)
      WORKSPACE="$(abs_path "${2:?missing value for $1}")"
      shift 2
      ;;
    --build-type)
      BUILD_TYPE="${2:?missing value for $1}"
      shift 2
      ;;
    --toolchain)
      TOOLCHAIN="${2:?missing value for $1}"
      shift 2
      ;;
    --prefix)
      GCC5_AARCH64_PREFIX="${2:?missing value for $1}"
      shift 2
      ;;
    --skip-fetch)
      SKIP_FETCH=1
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      die "unknown option: $1"
      ;;
  esac
done

check_host_tools
prepare_repositories
build_firmware
