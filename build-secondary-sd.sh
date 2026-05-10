#!/usr/bin/env bash
set -euo pipefail

# Create SD card image for secondary bootloader mode
# EDK2 FD is placed raw at sector 64 of the SD card
# U-Boot (on eMMC) loads it with: mmc dev 1; mmc read 0x200000 40 800; go 0x200000

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WORKSPACE="$(dirname "$SCRIPT_DIR")"
FD_FILE="$WORKSPACE/Build/Rk3399-SDK/DEBUG_GCC5/FV/RK3399_SDK_UEFI.fd"
OUT_DIR="$SCRIPT_DIR/out"
OUT_IMG="$OUT_DIR/edk2-secondary-sd.img"

FD_START_SECTOR=64
FD_SIZE_BYTES=983040
FD_SIZE_SECTORS=$(( (FD_SIZE_BYTES + 511) / 512 ))
IMG_SECTORS=$(( FD_START_SECTOR + FD_SIZE_SECTORS + 64 ))

mkdir -p "$OUT_DIR"

if [[ ! -f "$FD_FILE" ]]; then
    echo "[secondary-sd] ERROR: FD not found. Run build first."
    echo "[secondary-sd] Expected: $FD_FILE"
    exit 1
fi

echo "[secondary-sd] FD: $FD_FILE ($FD_SIZE_BYTES bytes = $FD_SIZE_SECTORS sectors)"
echo "[secondary-sd] Writing at sector $FD_START_SECTOR (offset $((FD_START_SECTOR * 512)))"

# Create image
rm -f "$OUT_IMG"
truncate -s $(( IMG_SECTORS * 512 )) "$OUT_IMG"

# Write FD raw at sector 64
dd if="$FD_FILE" of="$OUT_IMG" bs=512 seek=$FD_START_SECTOR conv=notrunc status=none

echo "[secondary-sd] Created: $OUT_IMG"
echo ""
echo "=== Usage ==="
echo "1. Write to SD card: dd if=$OUT_IMG of=/dev/sdX bs=4M"
echo "2. Boot device from eMMC (U-Boot with LCD)"
echo "3. In U-Boot shell, run:"
echo "   mmc dev 1"
echo "   mmc read 0x200000 40 $(printf '%x' $FD_SIZE_SECTORS)"
echo "   go 0x200000"
echo ""
echo "To automate, add to U-Boot env:"
echo "   setenv bootcmd_edk2 'mmc dev 1; mmc read 0x200000 40 $(printf '%x' $FD_SIZE_SECTORS); go 0x200000'"
echo "   setenv bootcmd 'run bootcmd_edk2'"
echo "   saveenv"
