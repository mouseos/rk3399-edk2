#!/usr/bin/env bash
set -euo pipefail

# Build EDK2 FD and pack as Android boot.img for U-Boot chainloading
# Usage: ./build-bootimg.sh [--skip-build]

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKSPACE="$(dirname "$SCRIPT_DIR")"
FD_FILE="$WORKSPACE/Build/Rk3399-SDK/DEBUG_GCC5/FV/RK3399_SDK_UEFI.fd"
OUT_DIR="$SCRIPT_DIR/out"
MKBOOTIMG="$HOME/work/Android/zmooth/zmooth_Source/kernel/scripts/mkbootimg"
BOOT_IMG="$OUT_DIR/edk2-boot.img"

# EDK2 FD load address (must match PcdFdBaseAddress in .fdf)
KERNEL_ADDR=0x00200000

mkdir -p "$OUT_DIR"

if [[ "${1:-}" != "--skip-build" ]]; then
    echo "[bootimg] Building EDK2..."
    "$SCRIPT_DIR/build-rk3399-uefi.sh" --workspace "$WORKSPACE" --skip-fetch
fi

if [[ ! -f "$FD_FILE" ]]; then
    echo "[bootimg] ERROR: FD not found: $FD_FILE"
    exit 1
fi

echo "[bootimg] FD: $FD_FILE ($(stat -c%s "$FD_FILE") bytes)"

# Create empty ramdisk
RAMDISK=$(mktemp)
echo -n > "$RAMDISK"

# Pack as Android boot.img
# kernel = EDK2 FD
# ramdisk = empty
# base = 0 (kernel_addr is absolute)
python3 "$MKBOOTIMG" \
    --kernel "$FD_FILE" \
    --ramdisk "$RAMDISK" \
    --base 0x0 \
    --kernel_offset "$KERNEL_ADDR" \
    --ramdisk_offset 0x0 \
    --second_offset 0x0 \
    --tags_offset 0x0 \
    --pagesize 2048 \
    --cmdline "" \
    --output "$BOOT_IMG"

rm -f "$RAMDISK"

echo "[bootimg] Created: $BOOT_IMG ($(stat -c%s "$BOOT_IMG") bytes)"
echo "[bootimg] Flash with: rkdeveloptool write-partition boot $BOOT_IMG"
echo "[bootimg] Or copy to SD card boot partition"
