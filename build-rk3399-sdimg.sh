#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEFAULT_OUT_DIR="$SCRIPT_DIR/out"

WORKSPACE="${WORKSPACE:-}"
EDK2_DIR="${EDK2_DIR:-}"
EDK2_PLATFORMS_DIR="${EDK2_PLATFORMS_DIR:-}"
EDK2_NON_OSI_DIR="${EDK2_NON_OSI_DIR:-}"
GCC5_AARCH64_PREFIX="${GCC5_AARCH64_PREFIX:-aarch64-linux-gnu-}"
TOOLCHAIN="GCC5"
BUILD_TYPE="DEBUG"
OUT_IMG="$DEFAULT_OUT_DIR/rk3399-uefi-sd.img"
UEFI_IMG=""
ESP_DIR=""
NO_BUILD=0
IMG_SECTORS=264192
ROCKCHIP_FW_TYPE_GUID="6975A537-1745-4736-9F0F-A621F1CB4378"

usage() {
  cat <<'EOF'
Usage:
  ./build-rk3399-sdimg.sh [options]

Build RK3399 EDK2 firmware and create a raw SD-card image matching the
README "Flashing the UEFI firmware" partition layout.

Options:
  -o, --output PATH        Output raw disk image path
                            default: ./out/rk3399-uefi-sd.img
      --workspace PATH     EDK2 workspace root
      --edk2 PATH          edk2 source tree, default: $WORKSPACE/edk2
      --edk2-platforms PATH
                            edk2-platforms source tree
      --edk2-non-osi PATH  edk2-non-osi source tree, if used
      --toolchain NAME     EDK2 toolchain, default: GCC5
      --build-type NAME    DEBUG or RELEASE, default: DEBUG
      --uefi-img PATH      Prepacked RK3399_SDK_UEFI.img to place in loader2
      --no-build           Skip EDK2 build and require --uefi-img
      --esp-dir PATH       Directory to copy into the FAT ESP partition
      --image-sectors N    Total sectors for the image, default: 264192
  -h, --help               Show this help

Required host tools for image generation: sgdisk, dd, truncate.
Optional ESP formatting tools: mkfs.vfat or mkfs.fat, and mcopy for --esp-dir.
EOF
}

log() {
  printf '[rk3399-sdimg] %s\n' "$*"
}

die() {
  printf '[rk3399-sdimg] error: %s\n' "$*" >&2
  exit 1
}

need_tool() {
  command -v "$1" >/dev/null 2>&1 || die "required tool not found: $1"
}

abs_path() {
  local path="$1"
  if [[ "$path" = /* ]]; then
    printf '%s\n' "$path"
  else
    printf '%s/%s\n' "$PWD" "$path"
  fi
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    -o|--output)
      OUT_IMG="$(abs_path "${2:?missing value for $1}")"
      shift 2
      ;;
    --workspace)
      WORKSPACE="$(abs_path "${2:?missing value for $1}")"
      shift 2
      ;;
    --edk2)
      EDK2_DIR="$(abs_path "${2:?missing value for $1}")"
      shift 2
      ;;
    --edk2-platforms)
      EDK2_PLATFORMS_DIR="$(abs_path "${2:?missing value for $1}")"
      shift 2
      ;;
    --edk2-non-osi)
      EDK2_NON_OSI_DIR="$(abs_path "${2:?missing value for $1}")"
      shift 2
      ;;
    --toolchain)
      TOOLCHAIN="${2:?missing value for $1}"
      shift 2
      ;;
    --build-type)
      BUILD_TYPE="${2:?missing value for $1}"
      shift 2
      ;;
    --uefi-img)
      UEFI_IMG="$(abs_path "${2:?missing value for $1}")"
      shift 2
      ;;
    --no-build)
      NO_BUILD=1
      shift
      ;;
    --esp-dir)
      ESP_DIR="$(abs_path "${2:?missing value for $1}")"
      shift 2
      ;;
    --image-sectors)
      IMG_SECTORS="${2:?missing value for $1}"
      shift 2
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

TOOLS_DIR="$SCRIPT_DIR/Rk3399Pkg/Tools"
LOADERIMAGE="$TOOLS_DIR/loaderimage"
IDBLOADER="$TOOLS_DIR/Bin/idbloader.img"
TRUST_IMG="$TOOLS_DIR/Bin/trust.img"
OUT_DIR="$(dirname "$OUT_IMG")"
PACKED_UEFI_IMG="$OUT_DIR/RK3399_SDK_UEFI.img"

[[ -f "$IDBLOADER" ]] || IDBLOADER="$TOOLS_DIR/Bin/idbloader.bin"
[[ -f "$IDBLOADER" ]] || die "idbloader image not found in $TOOLS_DIR/Bin"
[[ -f "$TRUST_IMG" ]] || die "trust image not found: $TRUST_IMG"
[[ -x "$LOADERIMAGE" ]] || die "loaderimage tool not executable: $LOADERIMAGE"

build_firmware() {
  [[ "$NO_BUILD" -eq 0 ]] || return 0

  if [[ -z "$WORKSPACE" ]]; then
    if [[ -d "$SCRIPT_DIR/../../../edk2" && "$(basename "$SCRIPT_DIR")" == "Rockchip" ]]; then
      WORKSPACE="$(cd "$SCRIPT_DIR/../../.." && pwd)"
    elif [[ -d "$PWD/edk2" ]]; then
      WORKSPACE="$PWD"
    else
      WORKSPACE="$(cd "$SCRIPT_DIR/.." && pwd)"
    fi
  fi

  [[ -n "$EDK2_DIR" ]] || EDK2_DIR="$WORKSPACE/edk2"
  [[ -n "$EDK2_PLATFORMS_DIR" ]] || EDK2_PLATFORMS_DIR="$WORKSPACE/edk2-platforms"
  [[ -n "$EDK2_NON_OSI_DIR" ]] || EDK2_NON_OSI_DIR="$WORKSPACE/edk2-non-osi"

  [[ -d "$EDK2_DIR" ]] || die "edk2 tree not found: $EDK2_DIR"
  [[ -f "$EDK2_DIR/edksetup.sh" ]] || die "edksetup.sh not found in: $EDK2_DIR"

  local platform_pkg_root="$EDK2_PLATFORMS_DIR"
  local overlay_dir="$OUT_DIR/package-overlay"
  if [[ ! -d "$EDK2_PLATFORMS_DIR/Platform/Rockchip/Rk3399Pkg" ]]; then
    platform_pkg_root="$overlay_dir"
    mkdir -p "$overlay_dir/Platform"
    ln -sfn "$SCRIPT_DIR" "$overlay_dir/Platform/Rockchip"
  fi

  export WORKSPACE
  export GCC5_AARCH64_PREFIX
  if [[ -n "${PACKAGES_PATH:-}" ]]; then
    export PACKAGES_PATH
  elif [[ -d "$EDK2_NON_OSI_DIR" ]]; then
    export PACKAGES_PATH="$EDK2_DIR:$platform_pkg_root:$EDK2_NON_OSI_DIR"
  else
    export PACKAGES_PATH="$EDK2_DIR:$platform_pkg_root"
  fi

  log "WORKSPACE=$WORKSPACE"
  log "PACKAGES_PATH=$PACKAGES_PATH"

  # shellcheck source=/dev/null
  source "$EDK2_DIR/edksetup.sh"
  if [[ ! -x "$EDK2_DIR/BaseTools/Source/C/bin/GenFfs" ]]; then
    log "building BaseTools"
    make -C "$EDK2_DIR/BaseTools"
  fi

  log "building RK3399 UEFI firmware"
  build -a AARCH64 -t "$TOOLCHAIN" \
    -p Platform/Rockchip/Rk3399Pkg/Rk3399-SDK.dsc \
    -b "$BUILD_TYPE"

  local fd="$WORKSPACE/Build/Rk3399-SDK/${BUILD_TYPE}_${TOOLCHAIN}/FV/RK3399_SDK_UEFI.fd"
  [[ -f "$fd" ]] || die "built firmware not found: $fd"

  log "packing UEFI firmware"
  "$LOADERIMAGE" --pack --uboot "$fd" "$PACKED_UEFI_IMG"
  UEFI_IMG="$PACKED_UEFI_IMG"
}

check_size_fits() {
  local file="$1"
  local max_bytes="$2"
  local label="$3"
  local size
  size="$(stat -c '%s' "$file")"
  (( size <= max_bytes )) || die "$label is too large: $size bytes, max $max_bytes bytes"
}

make_esp_image() {
  local esp_img="$1"
  local esp_bytes=$(( (262143 - 32768 + 1) * 512 ))
  local mkfs_fat=""

  mkfs_fat="$(command -v mkfs.vfat || command -v mkfs.fat || true)"
  if [[ -z "$mkfs_fat" ]]; then
    log "mkfs.vfat/mkfs.fat not found; ESP partition will be left blank"
    return 1
  fi

  truncate -s "$esp_bytes" "$esp_img"
  "$mkfs_fat" -n EFI "$esp_img" >/dev/null

  if [[ -n "$ESP_DIR" ]]; then
    [[ -d "$ESP_DIR" ]] || die "ESP source directory not found: $ESP_DIR"
    command -v mcopy >/dev/null 2>&1 || die "mcopy is required when --esp-dir is used"
    shopt -s nullglob dotglob
    local entries=("$ESP_DIR"/*)
    shopt -u nullglob dotglob
    if (( ${#entries[@]} > 0 )); then
      mcopy -i "$esp_img" -s "${entries[@]}" ::
    fi
  fi

  return 0
}

create_disk_image() {
  need_tool sgdisk
  need_tool dd
  need_tool truncate

  [[ -n "$UEFI_IMG" ]] || die "UEFI image path is empty; build failed or pass --uefi-img with --no-build"
  [[ -f "$UEFI_IMG" ]] || die "UEFI image not found: $UEFI_IMG"
  [[ "$IMG_SECTORS" =~ ^[0-9]+$ ]] || die "--image-sectors must be an integer"
  (( IMG_SECTORS > 262177 )) || die "--image-sectors must be greater than 262177 to leave room for backup GPT"

  mkdir -p "$OUT_DIR"

  check_size_fits "$IDBLOADER" $(( (8063 - 64 + 1) * 512 )) loader1
  check_size_fits "$UEFI_IMG" $(( (24575 - 16384 + 1) * 512 )) loader2
  check_size_fits "$TRUST_IMG" $(( (32767 - 24576 + 1) * 512 )) atf

  log "creating GPT disk image: $OUT_IMG"
  rm -f "$OUT_IMG"
  truncate -s "$(( IMG_SECTORS * 512 ))" "$OUT_IMG"
  # gdisk/sgdisk prints unknown GPT type GUIDs as code FFFF, matching the
  # README Rockchip firmware partitions.
  sgdisk --clear --set-alignment=1 \
    --new=1:64:8063 --typecode=1:"$ROCKCHIP_FW_TYPE_GUID" --change-name=1:loader1 \
    --new=2:8064:8191 --typecode=2:"$ROCKCHIP_FW_TYPE_GUID" --change-name=2:reserved1 \
    --new=3:8192:16383 --typecode=3:"$ROCKCHIP_FW_TYPE_GUID" --change-name=3:reserved2 \
    --new=4:16384:24575 --typecode=4:"$ROCKCHIP_FW_TYPE_GUID" --change-name=4:loader2 \
    --new=5:24576:32767 --typecode=5:"$ROCKCHIP_FW_TYPE_GUID" --change-name=5:atf \
    --new=6:32768:262143 --typecode=6:EF00 --change-name=6:"efi esp" \
    "$OUT_IMG" >/dev/null

  dd if="$IDBLOADER" of="$OUT_IMG" bs=512 seek=64 conv=notrunc status=none
  dd if="$UEFI_IMG" of="$OUT_IMG" bs=512 seek=16384 conv=notrunc status=none
  dd if="$TRUST_IMG" of="$OUT_IMG" bs=512 seek=24576 conv=notrunc status=none

  local tmp_esp
  tmp_esp="$(mktemp)"
  if make_esp_image "$tmp_esp"; then
    dd if="$tmp_esp" of="$OUT_IMG" bs=512 seek=32768 conv=notrunc status=none
  fi
  rm -f "$tmp_esp"

  log "done: $OUT_IMG"
}

mkdir -p "$OUT_DIR"
if [[ "$NO_BUILD" -eq 1 ]]; then
  [[ -n "$UEFI_IMG" ]] || die "--no-build requires --uefi-img"
else
  build_firmware
fi
create_disk_image
