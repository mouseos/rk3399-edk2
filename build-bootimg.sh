#!/usr/bin/env bash
set -euo pipefail

# Build EDK2 FD and pack it as Android boot.img for U-Boot chainloading.
#
# The Android boot flow loads the kernel payload at kernel_addr_r
# (0x00280000 on this RK3399 U-Boot). EDK2 is linked for 0x00200000, so the
# boot.img kernel is a small trampoline followed by the FD. The trampoline
# copies the FD to 0x00200000 and jumps there.
#
# Usage: ./build-bootimg.sh [--skip-build]

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKSPACE="${WORKSPACE:-$(dirname "$SCRIPT_DIR")}"
BUILD_TYPE="${BUILD_TYPE:-DEBUG}"
TOOLCHAIN="${TOOLCHAIN:-GCC5}"
FD_FILE="${FD_FILE:-$WORKSPACE/Build/Rk3399-SDK/${BUILD_TYPE}_${TOOLCHAIN}/FV/RK3399_SDK_UEFI.fd}"
OUT_DIR="${OUT_DIR:-$SCRIPT_DIR/out}"
MKBOOTIMG="${MKBOOTIMG:-$HOME/work/Android/zmooth/zmooth_Source/kernel/scripts/mkbootimg}"
BOOT_IMG="$OUT_DIR/edk2-boot.img"
BOOT_KERNEL="$OUT_DIR/edk2-boot-kernel.bin"
TRAMPOLINE_SRC="$SCRIPT_DIR/Rk3399Pkg/Tools/BootImgTrampoline.S"
TRAMPOLINE_ELF="$OUT_DIR/BootImgTrampoline.elf"
TRAMPOLINE_BIN="$OUT_DIR/BootImgTrampoline.bin"

# Android U-Boot payload address and EDK2 FD address.
ANDROID_KERNEL_ADDR=0x00280000
EDK2_FD_LOAD_ADDR=0x00200000
TRAMPOLINE_RELOC_ADDR=0x01000000
PAYLOAD_OFFSET=4096

AARCH64_PREFIX="${GCC5_AARCH64_PREFIX:-aarch64-linux-gnu-}"
TRAMPOLINE_CC="${TRAMPOLINE_CC:-${AARCH64_PREFIX}gcc}"
TRAMPOLINE_OBJCOPY="${TRAMPOLINE_OBJCOPY:-${AARCH64_PREFIX}objcopy}"

SKIP_BUILD=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --skip-build)
            SKIP_BUILD=1
            shift
            ;;
        *)
            echo "[bootimg] ERROR: unknown option: $1" >&2
            exit 1
            ;;
    esac
done

die() {
    echo "[bootimg] ERROR: $*" >&2
    exit 1
}

mkdir -p "$OUT_DIR"

if [[ "$SKIP_BUILD" -eq 0 ]]; then
    echo "[bootimg] Building EDK2..."
    "$SCRIPT_DIR/build-rk3399-uefi.sh" --workspace "$WORKSPACE" --skip-fetch
fi

if [[ ! -f "$FD_FILE" ]]; then
    die "FD not found: $FD_FILE"
fi
[[ -f "$TRAMPOLINE_SRC" ]] || die "trampoline source not found: $TRAMPOLINE_SRC"
[[ -f "$MKBOOTIMG" ]] || die "mkbootimg not found: $MKBOOTIMG"
command -v "$TRAMPOLINE_CC" >/dev/null 2>&1 || die "compiler not found: $TRAMPOLINE_CC"
command -v "$TRAMPOLINE_OBJCOPY" >/dev/null 2>&1 || die "objcopy not found: $TRAMPOLINE_OBJCOPY"

FD_SIZE="$(stat -c%s "$FD_FILE")"

echo "[bootimg] FD: $FD_FILE ($FD_SIZE bytes)"
echo "[bootimg] Building trampoline payload..."

"$TRAMPOLINE_CC" \
    -x assembler-with-cpp \
    -nostdlib \
    -static \
    -Wl,-Ttext=0 \
    -Wl,-e,_start \
    -Wl,--build-id=none \
    -DEDK2_FD_LOAD_ADDR="$EDK2_FD_LOAD_ADDR" \
    -DEDK2_FD_SIZE="$FD_SIZE" \
    -DPAYLOAD_OFFSET="$PAYLOAD_OFFSET" \
    -DTRAMPOLINE_RELOC_ADDR="$TRAMPOLINE_RELOC_ADDR" \
    "$TRAMPOLINE_SRC" \
    -o "$TRAMPOLINE_ELF"

"$TRAMPOLINE_OBJCOPY" -O binary -j .text "$TRAMPOLINE_ELF" "$TRAMPOLINE_BIN"

TRAMPOLINE_SIZE="$(stat -c%s "$TRAMPOLINE_BIN")"
if (( TRAMPOLINE_SIZE > PAYLOAD_OFFSET )); then
    die "trampoline too large: $TRAMPOLINE_SIZE bytes, max $PAYLOAD_OFFSET"
fi

rm -f "$BOOT_KERNEL"
cp "$TRAMPOLINE_BIN" "$BOOT_KERNEL"
truncate -s "$PAYLOAD_OFFSET" "$BOOT_KERNEL"
cat "$FD_FILE" >> "$BOOT_KERNEL"

echo "[bootimg] Trampoline: $TRAMPOLINE_SIZE bytes"
echo "[bootimg] Kernel payload: $BOOT_KERNEL ($(stat -c%s "$BOOT_KERNEL") bytes)"
echo "[bootimg] Runtime copy: $ANDROID_KERNEL_ADDR + $PAYLOAD_OFFSET -> $EDK2_FD_LOAD_ADDR ($FD_SIZE bytes)"

# Create empty ramdisk
RAMDISK=$(mktemp)
trap 'rm -f "$RAMDISK"' EXIT
: > "$RAMDISK"

# Pack as Android boot.img
# kernel = trampoline + EDK2 FD
# ramdisk = empty
# base = 0 (kernel_addr is absolute)
python3 "$MKBOOTIMG" \
    --kernel "$BOOT_KERNEL" \
    --ramdisk "$RAMDISK" \
    --base 0x0 \
    --kernel_offset "$ANDROID_KERNEL_ADDR" \
    --ramdisk_offset 0x0 \
    --second_offset 0x0 \
    --tags_offset 0x0 \
    --pagesize 2048 \
    --cmdline "" \
    --output "$BOOT_IMG"

echo "[bootimg] Created: $BOOT_IMG ($(stat -c%s "$BOOT_IMG") bytes)"
echo "[bootimg] Flash with: rkdeveloptool write-partition boot $BOOT_IMG"
echo "[bootimg] Or copy to SD card boot partition"
