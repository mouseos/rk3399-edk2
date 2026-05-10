/* SimpleFbDxe: Simple FrameBuffer with RGB565 hardware + 32bpp GOP */
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

#define FB_BITS_PER_PIXEL  (32)
#define HW_BPP             (16)

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
STATIC UINT32 *mShadowFb;        // 32bpp shadow buffer (for FrameBufferBltLib)
STATIC UINT16 *mHwFb;            // Hardware RGB565 framebuffer
STATIC UINT32 mWidth, mHeight;

STATIC VOID __attribute__((unused)) ConvertAndFlush(VOID)
{
  UINTN total = mWidth * mHeight;
  UINTN i;
  for (i = 0; i < total; i++) {
    UINT32 pixel = mShadowFb[i];
    UINT8 r = (pixel >> 16) & 0xFF;
    UINT8 g = (pixel >> 8) & 0xFF;
    UINT8 b = pixel & 0xFF;
    mHwFb[i] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
  }
  WriteBackInvalidateDataCacheRange((void*)mHwFb, total * 2);
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
    // Convert affected region from 32bpp shadow to RGB565 hardware FB
    UINTN y;
    for (y = DestinationY; y < DestinationY + Height && y < mHeight; y++) {
      UINTN x;
      UINTN offset = y * mWidth + DestinationX;
      for (x = 0; x < Width && (DestinationX + x) < mWidth; x++) {
        UINT32 pixel = mShadowFb[offset + x];
        UINT8 r = (pixel >> 16) & 0xFF;
        UINT8 g = (pixel >> 8) & 0xFF;
        UINT8 b = pixel & 0xFF;
        // VOP has RB_SWAP=1, so swap R and B in RGB565 output
        mHwFb[offset + x] = ((b >> 3) << 11) | ((g >> 2) << 5) | (r >> 3);
      }
    }
    WriteBackInvalidateDataCacheRange(
      (void*)(mHwFb + DestinationY * mWidth),
      Height * mWidth * 2);
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

  // NOTE: VOP has RB_SWAP=1, so R and B are swapped in RGB565 output.
  // We account for this in the conversion (swap R and B).

  // Allocate 32bpp shadow framebuffer
  UINTN ShadowSize = mWidth * mHeight * 4;
  mShadowFb = AllocateZeroPool(ShadowSize);
  if (mShadowFb == NULL) {
    DEBUG((DEBUG_ERROR, "SimpleFbDxe: Failed to allocate shadow FB\n"));
    return EFI_OUT_OF_RESOURCES;
  }

  // Clear hardware FB to black
  ZeroMem((void*)mHwFb, mWidth * mHeight * 2);
  WriteBackInvalidateDataCacheRange((void*)mHwFb, mWidth * mHeight * 2);

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
  mDisplay.Mode->Info->PixelFormat = PixelBlueGreenRedReserved8BitPerColor;
  mDisplay.Mode->Info->PixelsPerScanLine = mWidth;
  mDisplay.Mode->SizeOfInfo = sizeof(EFI_GRAPHICS_OUTPUT_MODE_INFORMATION);
  mDisplay.Mode->FrameBufferBase = (EFI_PHYSICAL_ADDRESS)(UINTN)mShadowFb;
  mDisplay.Mode->FrameBufferSize = ShadowSize;

  // Configure FrameBufferBltLib with shadow buffer
  mFrameBufferBltLibConfigureSize = 0;
  Status = FrameBufferBltConfigure(
      (VOID*)mShadowFb, mDisplay.Mode->Info,
      mFrameBufferBltLibConfigure, &mFrameBufferBltLibConfigureSize);
  if (Status == RETURN_BUFFER_TOO_SMALL) {
    mFrameBufferBltLibConfigure = AllocatePool(mFrameBufferBltLibConfigureSize);
    if (mFrameBufferBltLibConfigure != NULL) {
      Status = FrameBufferBltConfigure(
          (VOID*)mShadowFb, mDisplay.Mode->Info,
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

  DEBUG((DEBUG_ERROR, "SimpleFbDxe: GOP installed %ux%u (shadow@%p hw@%p)\n",
         mWidth, mHeight, mShadowFb, mHwFb));

  return Status;
}
