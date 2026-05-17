/* SimpleFbDxe: RGB565 GOP backed directly by the U-Boot configured VOP FB. */
#include <PiDxe.h>
#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Protocol/GraphicsOutput.h>
#include <Library/BaseLib.h>
#include <Library/FrameBufferBltLib.h>
#include <Library/CacheMaintenanceLib.h>
#include <Library/IoLib.h>

#define FB_BITS_PER_PIXEL   (16)

#define RK3399_VOP0_BIG_BASE        0xFF900000
#define VOP_REG_CFG_DONE_OFFSET     0x000
#define VOP_WIN0_CTRL0_OFFSET       0x030
#define VOP_WIN0_VIR_OFFSET         0x03C
#define VOP_WIN0_YRGB_MST_OFFSET    0x040

#define VOP_WIN0_EN                 BIT0
#define VOP_WIN0_DATA_FMT_MASK      (7U << 1)
#define VOP_WIN0_DATA_FMT_RGB565    (2U << 1)
#define VOP_WIN0_RB_SWAP            BIT12

typedef struct {
  VENDOR_DEVICE_PATH DisplayDevicePath;
  EFI_DEVICE_PATH EndDevicePath;
} DISPLAY_DEVICE_PATH;

DISPLAY_DEVICE_PATH mDisplayDevicePath = {
  {{HARDWARE_DEVICE_PATH, HW_VENDOR_DP,
    {(UINT8)(sizeof(VENDOR_DEVICE_PATH)), (UINT8)((sizeof(VENDOR_DEVICE_PATH)) >> 8)}},
   EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID},
  {END_DEVICE_PATH_TYPE, END_ENTIRE_DEVICE_PATH_SUBTYPE, {sizeof(EFI_DEVICE_PATH_PROTOCOL), 0}}
};

STATIC FRAME_BUFFER_CONFIGURE *mFrameBufferBltLibConfigure;
STATIC UINTN mFrameBufferBltLibConfigureSize;
STATIC UINT16 *mHwFb;
STATIC UINT32 mWidth, mHeight;
STATIC UINTN mFrameBufferSize;

STATIC
VOID
ConfigureVopForRgb565 (
  IN UINT32 HwFbAddr
  )
{
  UINT32 Ctrl0;
  UINT32 NewCtrl0;

  Ctrl0 = MmioRead32 (RK3399_VOP0_BIG_BASE + VOP_WIN0_CTRL0_OFFSET);
  NewCtrl0 = Ctrl0;
  NewCtrl0 &= ~(VOP_WIN0_DATA_FMT_MASK | VOP_WIN0_RB_SWAP);
  NewCtrl0 |= VOP_WIN0_DATA_FMT_RGB565 | VOP_WIN0_EN;

  MmioWrite32 (RK3399_VOP0_BIG_BASE + VOP_WIN0_CTRL0_OFFSET, NewCtrl0);
  MmioWrite32 (RK3399_VOP0_BIG_BASE + VOP_WIN0_VIR_OFFSET, (mWidth / 2) & 0x3FFF);
  MmioWrite32 (RK3399_VOP0_BIG_BASE + VOP_WIN0_YRGB_MST_OFFSET, HwFbAddr);
  MmioWrite32 (RK3399_VOP0_BIG_BASE + VOP_REG_CFG_DONE_OFFSET, 1);

  DEBUG ((DEBUG_ERROR,
    "SimpleFbDxe: VOP RGB565 fb=0x%08x width=%u ctrl0 0x%08x->0x%08x\n",
    HwFbAddr, mWidth, Ctrl0, NewCtrl0));
}

STATIC EFI_STATUS EFIAPI DisplayQueryMode(
    IN EFI_GRAPHICS_OUTPUT_PROTOCOL *This, IN UINT32 ModeNumber,
    OUT UINTN *SizeOfInfo, OUT EFI_GRAPHICS_OUTPUT_MODE_INFORMATION **Info)
{
  EFI_STATUS Status;
  Status = gBS->AllocatePool(EfiBootServicesData,
      sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION), (VOID**)Info);
  ASSERT_EFI_ERROR(Status);
  *SizeOfInfo = sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION);
  CopyMem(*Info, This->Mode->Info, sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION));
  return EFI_SUCCESS;
}

STATIC EFI_STATUS EFIAPI DisplaySetMode(
    IN EFI_GRAPHICS_OUTPUT_PROTOCOL *This, IN UINT32 ModeNumber)
{
  return EFI_SUCCESS;
}

STATIC EFI_STATUS EFIAPI DisplayBlt(
    IN EFI_GRAPHICS_OUTPUT_PROTOCOL *This,
    IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL *BltBuffer OPTIONAL,
    IN EFI_GRAPHICS_OUTPUT_BLT_OPERATION BltOperation,
    IN UINTN SourceX, IN UINTN SourceY,
    IN UINTN DestinationX, IN UINTN DestinationY,
    IN UINTN Width, IN UINTN Height,
    IN UINTN Delta OPTIONAL)
{
  RETURN_STATUS Status;
  EFI_TPL Tpl;

  Tpl = gBS->RaiseTPL(TPL_NOTIFY);
  Status = FrameBufferBlt(mFrameBufferBltLibConfigure, BltBuffer, BltOperation,
             SourceX, SourceY, DestinationX, DestinationY, Width, Height, Delta);
  gBS->RestoreTPL(Tpl);

  if (!RETURN_ERROR(Status)) {
    WriteBackInvalidateDataCacheRange((VOID*)mHwFb, mFrameBufferSize);
  }

  return RETURN_ERROR(Status) ? EFI_INVALID_PARAMETER : EFI_SUCCESS;
}

STATIC EFI_GRAPHICS_OUTPUT_PROTOCOL mDisplay = {
  DisplayQueryMode, DisplaySetMode, DisplayBlt, NULL
};

EFI_STATUS EFIAPI SimpleFbDxeInitialize(
    IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable)
{
  EFI_STATUS Status = EFI_SUCCESS;
  EFI_HANDLE hUEFIDisplayHandle = NULL;

  DEBUG((DEBUG_ERROR, "SimpleFbDxe: Initialize\n"));

  UINT32 HwFbAddr = FixedPcdGet32(PcdMipiFrameBufferAddress);
  mWidth = FixedPcdGet32(PcdMipiFrameBufferWidth);
  mHeight = FixedPcdGet32(PcdMipiFrameBufferHeight);

  if (HwFbAddr == 0 || mWidth == 0 || mHeight == 0) {
    DEBUG((DEBUG_ERROR, "SimpleFbDxe: Invalid PCD parameters\n"));
    return EFI_DEVICE_ERROR;
  }

  mHwFb = (UINT16*)(UINTN)HwFbAddr;
  mFrameBufferSize = mWidth * mHeight * (FB_BITS_PER_PIXEL / 8);

  ConfigureVopForRgb565 (HwFbAddr);
  ZeroMem((VOID*)mHwFb, mFrameBufferSize);
  WriteBackInvalidateDataCacheRange((VOID*)mHwFb, mFrameBufferSize);

  // Setup GOP mode info
  Status = gBS->AllocatePool(EfiBootServicesData,
      sizeof(EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE), (VOID**)&mDisplay.Mode);
  ASSERT_EFI_ERROR(Status);
  ZeroMem(mDisplay.Mode, sizeof(EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE));

  Status = gBS->AllocatePool(EfiBootServicesData,
      sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION), (VOID**)&mDisplay.Mode->Info);
  ASSERT_EFI_ERROR(Status);
  ZeroMem(mDisplay.Mode->Info, sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION));

  mDisplay.Mode->MaxMode = 1;
  mDisplay.Mode->Mode = 0;
  mDisplay.Mode->Info->Version = 0;
  mDisplay.Mode->Info->HorizontalResolution = mWidth;
  mDisplay.Mode->Info->VerticalResolution = mHeight;
  mDisplay.Mode->Info->PixelFormat = PixelBitMask;
  mDisplay.Mode->Info->PixelInformation.RedMask = 0xF800;
  mDisplay.Mode->Info->PixelInformation.GreenMask = 0x07E0;
  mDisplay.Mode->Info->PixelInformation.BlueMask = 0x001F;
  mDisplay.Mode->Info->PixelInformation.ReservedMask = 0;
  mDisplay.Mode->Info->PixelsPerScanLine = mWidth;
  mDisplay.Mode->SizeOfInfo = sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION);
  mDisplay.Mode->FrameBufferBase = (EFI_PHYSICAL_ADDRESS)(UINTN)mHwFb;
  mDisplay.Mode->FrameBufferSize = mFrameBufferSize;

  mFrameBufferBltLibConfigureSize = 0;
  Status = FrameBufferBltConfigure(
      (VOID*)mHwFb, mDisplay.Mode->Info,
      mFrameBufferBltLibConfigure, &mFrameBufferBltLibConfigureSize);
  if (Status == RETURN_BUFFER_TOO_SMALL) {
    mFrameBufferBltLibConfigure = AllocatePool(mFrameBufferBltLibConfigureSize);
    if (mFrameBufferBltLibConfigure != NULL) {
      Status = FrameBufferBltConfigure(
          (VOID*)mHwFb, mDisplay.Mode->Info,
          mFrameBufferBltLibConfigure, &mFrameBufferBltLibConfigureSize);
    }
  }
  ASSERT_EFI_ERROR(Status);

  // Install GOP
  Status = gBS->InstallMultipleProtocolInterfaces(
      &hUEFIDisplayHandle,
      &gEfiDevicePathProtocolGuid, &mDisplayDevicePath,
      &gEfiGraphicsOutputProtocolGuid, &mDisplay,
      NULL);
  ASSERT_EFI_ERROR(Status);

  DEBUG((DEBUG_ERROR, "SimpleFbDxe: GOP installed %ux%u RGB565 fb@%p\n",
         mWidth, mHeight, mHwFb));

  return Status;
}
