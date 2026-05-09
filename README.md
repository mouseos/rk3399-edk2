# Rockchip RK3399 UEFI Firmware

Now with ACPI...and (almost working) USB!

## Notes

This is a version of the Rockchip 3399 UEFI firmware from https://github.com/jeffchenfz/Rockchip
updated to build with the latest [edk2](https://github.com/tianocore/edk2)/[edk2-platforms](https://github.com/tianocore/edk2-platforms)
(as of 2018.10.28).

## Building on vanilla Debian GNU/Linux 9.1

The same pinned revisions can be fetched and built with:

```
apt-get install build-essential acpica-tools nasm uuid-dev gcc-aarch64-linux-gnu git
./build-rk3399-uefi.sh
```

The script uses the parent directory of this repository as `WORKSPACE`, creates
or updates `edk2`, `edk2-non-osi`, and `edk2-platforms`, symlinks this checkout
to `edk2-platforms/Platform/Rockchip`, then writes `RK3399_SDK_UEFI.img` to the
workspace root.

To create a Win32DiskImager-friendly SD-card image after the UEFI build:

```
./build-rk3399-sdimg.sh --no-build --uefi-img ../RK3399_SDK_UEFI.img
```

The output is `out/rk3399-uefi-sd.img`. It uses the exact sector layout shown in
the flashing section below.

```
apt-get install build-essential acpica-tools nasm uuid-dev gcc-aarch64-linux-gnu
# If you want to use a different workspace, change the target directory below
export WORKSPACE=~/workspace
mkdir $WORKSPACE
cd $WORKSPACE
git clone https://github.com/tianocore/edk2.git
pushd edk2
git checkout b7a715f7c03c45c6b4575bf88596bfd79658b8ce
popd
git clone https://github.com/tianocore/edk2-non-osi.git
pushd edk2-non-osi
git checkout 7ac12d81e02b323bffdf1ef3c188ea33c2185c91
popd
git clone https://github.com/tianocore/edk2-platforms.git
pushd edk2-platforms
git checkout b5e92aa284c59a22e7e38f79125a20f774ab7027
popd
git clone https://github.com/andreiw/rk3399-edk2.git edk2-platforms/Platform/Rockchip
export GCC5_AARCH64_PREFIX=aarch64-linux-gnu-
export PACKAGES_PATH=$WORKSPACE/edk2:$WORKSPACE/edk2-platforms:$WORKSPACE/edk2-non-osi
. edk2/edksetup.sh
make -C edk2/BaseTools
build -a AARCH64 -t GCC5 -p edk2-platforms/Platform/Rockchip/Rk3399Pkg/Rk3399-SDK.dsc -b DEBUG
edk2-platforms/Platform/Rockchip/Rk3399Pkg/Tools/loaderimage --pack --uboot Build/Rk3399-SDK/DEBUG_GCC5/FV/RK3399_SDK_UEFI.fd RK3399_SDK_UEFI.img
```

## Flashing the UEFI firmware

  How to put this together? You need a GPT disk. SD card will be fine, or
  you can use rkdeveloptool to modify the eMMC.

  The start/end sectors are important!

    Number  Start (sector)    End (sector)  Size       Code  Name
       1              64            8063   3.9 MiB     FFFF  loader1   <-- Rk3399Pkg/Tools/Bin/idbloader.bin
       2            8064            8191   64.0 KiB    FFFF  reserved1
       3            8192           16383   4.0 MiB     FFFF  reserved2
       4           16384           24575   4.0 MiB     FFFF  loader2   <-- RK3399_SDK_UEFI.img
       5           24576           32767   4.0 MiB     FFFF  atf       <-- Rk3399Pkg/Tools/Bin/trust.img
       6           32768          262143   112.0 MiB   EF00  efi esp
