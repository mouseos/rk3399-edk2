/** @file
  This file implement the MMC Host Protocol for the DesignWare eMMC.

  Copyright (c) 2014-2017, Linaro Limited. All rights reserved.
  Copyright (c) 2017, Rockchip Inc. All rights reserved.
  Copyright (c) 2019, Andrey Warkentin <andrey.warkentin@gmail.com>

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <Library/BaseMemoryLib.h>
#include <Library/CacheMaintenanceLib.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/IoLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/TimerLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/CRULib.h>
#include <Rk3399/Rk3399Cru.h>

#include <Protocol/MmcHost.h>

#include "DwEmmc.h"
#include <Rk3399/Rk3399.h>
#include <Rk3399/Rk3399Grf.h>
#include <Rk3399/Rk3399PmuGrf.h>

#define DW_DBG		DEBUG_BLKIO

#define DWEMMC_DESC_PAGE                1
#define DWEMMC_BLOCK_SIZE               512
#define DWEMMC_DMA_BUF_SIZE             (512 * 8)
#define DWEMMC_MAX_DESC_PAGES           512
#define DWEMMC_CMD_SETTLE_US            100
#define DWEMMC_SD_SWITCH_SETTLE_US      15000
#define DWEMMC_CMD_POLL_US              10
#define DWEMMC_CMD_TIMEOUT_US           1000000
#define DWEMMC_DMA_POLL_US              10
#define DWEMMC_DMA_TIMEOUT_US           10000000
#define DWEMMC_INT_HTO                  (1 << 10)
#define DWEMMC_DATA_ERROR_FLAGS         (DWEMMC_INT_DRT | DWEMMC_INT_DCRC | DWEMMC_INT_FRUN | \
                                         DWEMMC_INT_HLE | DWEMMC_INT_HTO | DWEMMC_INT_SBE  | \
                                         DWEMMC_INT_EBE)
#define FIFO_RESET                      (1 << 1)
#define FIFO_EMPTY                      (1 << 2)
#define DWEMMC_MSHCI_FIFO               ((UINT32)PcdGet32 (PcdDwEmmcDxeBaseAddress) + 0x200)

typedef struct {
  UINT32                        Des0;
  UINT32                        Des1;
  UINT32                        Des2;
  UINT32                        Des3;
} DWEMMC_IDMAC_DESCRIPTOR;

#define DWEMMC_MAX_DESC_COUNT           ((EFI_PAGE_SIZE * DWEMMC_MAX_DESC_PAGES) / sizeof (DWEMMC_IDMAC_DESCRIPTOR))

EFI_MMC_HOST_PROTOCOL     *gpMmcHost;
DWEMMC_IDMAC_DESCRIPTOR   *gpIdmacDesc;
EFI_GUID mDwEmmcDevicePathGuid = EFI_CALLER_ID_GUID;
STATIC UINT32 mDwEmmcCommand;
STATIC UINT32 mDwEmmcArgument;

STATIC
UINTN
DwEmmcCommandSettleDelay (
  IN MMC_CMD                    MmcCmd
  )
{
  switch (MMC_GET_INDX (MmcCmd)) {
  case MMC_INDX (6):
  case MMC_INDX (51):
    return DWEMMC_SD_SWITCH_SETTLE_US;
  default:
    return DWEMMC_CMD_SETTLE_US;
  }
}

EFI_STATUS
DwEmmcReadBlockData (
  IN EFI_MMC_HOST_PROTOCOL     *This,
  IN EFI_LBA                    Lba,
  IN UINTN                      Length,
  IN UINT32*                    Buffer
  );

BOOLEAN
DwEmmcIsPowerOn (
  VOID
  )
{
    return TRUE;
}

EFI_STATUS
DwEmmcInitialize (
  VOID
  )
{
    DEBUG ((DEBUG_BLKIO, "DwEmmcInitialize()"));
    return EFI_SUCCESS;
}

BOOLEAN
DwEmmcIsCardPresent (
  IN EFI_MMC_HOST_PROTOCOL     *This
  )
{
  return TRUE;
}

BOOLEAN
DwEmmcIsReadOnly (
  IN EFI_MMC_HOST_PROTOCOL     *This
  )
{
  return FALSE;
}

BOOLEAN
DwEmmcIsDmaSupported (
  IN EFI_MMC_HOST_PROTOCOL     *This
  )
{
  return TRUE;
}

EFI_STATUS
DwEmmcBuildDevicePath (
  IN EFI_MMC_HOST_PROTOCOL      *This,
  IN EFI_DEVICE_PATH_PROTOCOL   **DevicePath
  )
{
  EFI_DEVICE_PATH_PROTOCOL *NewDevicePathNode;

  NewDevicePathNode = CreateDeviceNode (HARDWARE_DEVICE_PATH, HW_VENDOR_DP, sizeof (VENDOR_DEVICE_PATH));
  CopyGuid (& ((VENDOR_DEVICE_PATH*)NewDevicePathNode)->Guid, &mDwEmmcDevicePathGuid);

  *DevicePath = NewDevicePathNode;
  return EFI_SUCCESS;
}

EFI_STATUS
DwEmmcUpdateClock (
  VOID
  )
{
  UINT32 Data;

  /* CMD_UPDATE_CLK */
  Data = BIT_CMD_WAIT_PRVDATA_COMPLETE | BIT_CMD_UPDATE_CLOCK_ONLY |
         BIT_CMD_START;
  MmioWrite32 (DWEMMC_CMD, Data);
  while (1) {
    Data = MmioRead32 (DWEMMC_CMD);
    if (!(Data & CMD_START_BIT)) {
      break;
    }
    Data = MmioRead32 (DWEMMC_RINTSTS);
    if (Data & DWEMMC_INT_HLE) {
      Print (L"failed to update mmc clock frequency\n");
      return EFI_DEVICE_ERROR;
    }
  }
  return EFI_SUCCESS;
}

EFI_STATUS
DwEmmcSetClock (
  IN UINTN                     ClockFreq
  )
{
  UINT32 Divider, Rate, Data;
  EFI_STATUS Status;
  BOOLEAN Found = FALSE;

  DEBUG ((DW_DBG, "%a():ClockFreq:%d\n", __func__, ClockFreq));

  for (Divider = 1; Divider < 256; Divider++) {
    Rate = PcdGet32 (PcdDwEmmcDxeClockFrequencyInHz)/2;
    if ((Rate / (2 * Divider)) <= ClockFreq) {
      Found = TRUE;
      break;
    }
  }
  if (Found == FALSE) {
    return EFI_NOT_FOUND;
  }

  // Wait until MMC is idle
  do {
    Data = MmioRead32 (DWEMMC_STATUS);
  } while (Data & DWEMMC_STS_DATA_BUSY);

  // Disable MMC clock first
  MmioWrite32 (DWEMMC_CLKENA, 0);
  Status = DwEmmcUpdateClock ();
  ASSERT (!EFI_ERROR (Status));

  MmioWrite32 (DWEMMC_CLKDIV, Divider);
  Status = DwEmmcUpdateClock ();
  ASSERT (!EFI_ERROR (Status));

  // Enable MMC clock
  MmioWrite32 (DWEMMC_CLKENA, 1);
  MmioWrite32 (DWEMMC_CLKSRC, 0);
  Status = DwEmmcUpdateClock ();
  ASSERT (!EFI_ERROR (Status));
  return EFI_SUCCESS;
}

EFI_STATUS
DwEmmcNotifyState (
  IN EFI_MMC_HOST_PROTOCOL     *This,
  IN MMC_STATE                 State
  )
{
  UINT32      Data;
  EFI_STATUS  Status;

  switch (State) {
  case MmcInvalidState:
    return EFI_INVALID_PARAMETER;
  case MmcHwInitializationState:
    MmioWrite32 (DWEMMC_PWREN, 1);

    // If device already turn on then restart it
    Data = DWEMMC_CTRL_RESET_ALL;
    MmioWrite32 (DWEMMC_CTRL, Data);
    do {
      // Wait until reset operation finished
      Data = MmioRead32 (DWEMMC_CTRL);
    } while (Data & DWEMMC_CTRL_RESET_ALL);

    // Setup clock that could not be higher than 400KHz.
    Status = DwEmmcSetClock (400000);
    ASSERT (!EFI_ERROR (Status));
    // Wait clock stable
    MicroSecondDelay (100);

    MmioWrite32 (DWEMMC_RINTSTS, ~0);
    MmioWrite32 (DWEMMC_INTMASK, 0);
    MmioWrite32 (DWEMMC_TMOUT, ~0);
    MmioWrite32 (DWEMMC_IDINTEN, 0);
    MmioWrite32 (DWEMMC_BMOD, DWEMMC_IDMAC_SWRESET);

    MmioWrite32 (DWEMMC_BLKSIZ, DWEMMC_BLOCK_SIZE);
    do {
      Data = MmioRead32 (DWEMMC_BMOD);
    } while (Data & DWEMMC_IDMAC_SWRESET);
    break;
  case MmcIdleState:
    break;
  case MmcReadyState:
    break;
  case MmcIdentificationState:
    break;
  case MmcStandByState:
    break;
  case MmcTransferState:
    break;
  case MmcSendingDataState:
    break;
  case MmcReceiveDataState:
    break;
  case MmcProgrammingState:
    break;
  case MmcDisconnectState:
    break;
  default:
    return EFI_INVALID_PARAMETER;
  }
  return EFI_SUCCESS;
}

// Need to prepare DMA buffer first before sending commands to MMC card
BOOLEAN
IsPendingReadCommand (
  IN MMC_CMD                    MmcCmd
  )
{
  UINTN  Mask;

  Mask = BIT_CMD_DATA_EXPECTED | BIT_CMD_READ;
  if ((MmcCmd & Mask) == Mask) {
    return TRUE;
  }
  return FALSE;
}

BOOLEAN
IsPendingWriteCommand (
  IN MMC_CMD                    MmcCmd
  )
{
  UINTN  Mask;

  Mask = BIT_CMD_DATA_EXPECTED | BIT_CMD_WRITE;
  if ((MmcCmd & Mask) == Mask) {
    return TRUE;
  }
  return FALSE;
}

EFI_STATUS
SendCommand (
  IN MMC_CMD                    MmcCmd,
  IN UINT32                     Argument
  )
{
  UINT32      Data, ErrMask;
  UINTN       TimeOut;

  DEBUG ((DW_DBG, "%a(): MmcCmd 0x%x(%d),Argument 0x%x \n", __func__, MmcCmd, MmcCmd&0x3f, Argument));

  MicroSecondDelay (DwEmmcCommandSettleDelay (MmcCmd));

  // Wait until MMC is idle
  do {
    Data = MmioRead32 (DWEMMC_STATUS);
  } while (Data & DWEMMC_STS_DATA_BUSY);

  MmioWrite32 (DWEMMC_RINTSTS, ~0);
  MmioWrite32 (DWEMMC_CMDARG, Argument);
  MmioWrite32 (DWEMMC_CMD, MmcCmd);

  ErrMask = DWEMMC_INT_EBE | DWEMMC_INT_HLE | DWEMMC_INT_RTO |
            DWEMMC_INT_RCRC | DWEMMC_INT_RE;
  ErrMask |= DWEMMC_INT_DCRC | DWEMMC_INT_DRT | DWEMMC_INT_SBE;
  TimeOut = DWEMMC_CMD_TIMEOUT_US / DWEMMC_CMD_POLL_US;
  do {
    Data = MmioRead32 (DWEMMC_RINTSTS);

    if (Data & ErrMask) {
      DEBUG ((DW_DBG, "%a(): EFI_DEVICE_ERROR DWEMMC_RINTSTS=0x%x MmcCmd 0x%x(%d),Argument 0x%x\n",
          __func__, Data, MmcCmd, MmcCmd&0x3f, Argument));
      return EFI_DEVICE_ERROR;
    }
    if (Data & DWEMMC_INT_DTO) {     // Transfer Done
      break;
    }

    if (Data & DWEMMC_INT_CMD_DONE) {
      break;
    }

    MicroSecondDelay (DWEMMC_CMD_POLL_US);
  } while (--TimeOut > 0);

  if (TimeOut == 0) {
    DEBUG ((DEBUG_ERROR, "%a(): EFI_TIMEOUT MmcCmd 0x%x(%d),Argument 0x%x RINTSTS=0x%x\n",
      __func__, MmcCmd, MmcCmd&0x3f, Argument, Data));
    return EFI_TIMEOUT;
  }

  DEBUG ((DW_DBG, "%a(): EFI_SUCCESS\n", __func__));
  return EFI_SUCCESS;
}

EFI_STATUS
DwEmmcSendCommand (
  IN EFI_MMC_HOST_PROTOCOL     *This,
  IN MMC_CMD                    MmcCmd,
  IN UINT32                     Argument
  )
{
  UINT32       Cmd = 0;
  EFI_STATUS   Status = EFI_SUCCESS;

  DEBUG ((DW_DBG, "%a(): MmcCmd 0x%x(%d),Argument 0x%x \n", __func__, MmcCmd, MmcCmd&0x3f, Argument));

  switch (MMC_GET_INDX(MmcCmd)) {
  case MMC_INDX(0):
    Cmd = BIT_CMD_SEND_INIT;
    break;
  case MMC_INDX(1):
    Cmd = BIT_CMD_RESPONSE_EXPECT;
    break;
  case MMC_INDX(2):
    Cmd = BIT_CMD_RESPONSE_EXPECT | BIT_CMD_LONG_RESPONSE |
           BIT_CMD_CHECK_RESPONSE_CRC | BIT_CMD_SEND_INIT;
    break;
  case MMC_INDX(3):
    Cmd = BIT_CMD_RESPONSE_EXPECT | BIT_CMD_CHECK_RESPONSE_CRC |
           BIT_CMD_SEND_INIT;
    break;
  case MMC_INDX(6):
    if (((Argument >> 31) & 0x1) == 0x1)
      Cmd = BIT_CMD_RESPONSE_EXPECT |BIT_CMD_CHECK_RESPONSE_CRC |
           BIT_CMD_DATA_EXPECTED | BIT_CMD_READ |
           BIT_CMD_WAIT_PRVDATA_COMPLETE;
    else
      Cmd = BIT_CMD_RESPONSE_EXPECT | BIT_CMD_CHECK_RESPONSE_CRC;
    break;
  case MMC_INDX(7):
    if (Argument)
        Cmd = BIT_CMD_RESPONSE_EXPECT | BIT_CMD_CHECK_RESPONSE_CRC;
    else
        Cmd = 0;
    break;
  case MMC_INDX(8):
    Cmd = BIT_CMD_RESPONSE_EXPECT | BIT_CMD_CHECK_RESPONSE_CRC |
           /*BIT_CMD_DATA_EXPECTED | BIT_CMD_READ |*/
           BIT_CMD_WAIT_PRVDATA_COMPLETE;
    break;
  case MMC_INDX(9):
    Cmd = BIT_CMD_RESPONSE_EXPECT | BIT_CMD_CHECK_RESPONSE_CRC |
           BIT_CMD_LONG_RESPONSE;
    break;
  case MMC_INDX(12):
    Cmd = BIT_CMD_RESPONSE_EXPECT | BIT_CMD_CHECK_RESPONSE_CRC |
           BIT_CMD_STOP_ABORT_CMD;
    break;
  case MMC_INDX(13):
    Cmd = BIT_CMD_RESPONSE_EXPECT | BIT_CMD_CHECK_RESPONSE_CRC |
           BIT_CMD_WAIT_PRVDATA_COMPLETE;
    break;
  case MMC_INDX(16):
    Cmd = BIT_CMD_RESPONSE_EXPECT | BIT_CMD_CHECK_RESPONSE_CRC;
    break;
  case MMC_INDX(17):
  case MMC_INDX(18):
    Cmd = BIT_CMD_RESPONSE_EXPECT | BIT_CMD_CHECK_RESPONSE_CRC |
           BIT_CMD_DATA_EXPECTED | BIT_CMD_READ |
           BIT_CMD_WAIT_PRVDATA_COMPLETE;
    break;
  case MMC_INDX(24):
  case MMC_INDX(25):
    Cmd = BIT_CMD_RESPONSE_EXPECT | BIT_CMD_CHECK_RESPONSE_CRC |
           BIT_CMD_DATA_EXPECTED | BIT_CMD_WRITE |
           BIT_CMD_WAIT_PRVDATA_COMPLETE;
    break;
  case MMC_INDX(30):
    Cmd = BIT_CMD_RESPONSE_EXPECT | BIT_CMD_CHECK_RESPONSE_CRC |
           BIT_CMD_DATA_EXPECTED;
    break;
  case MMC_INDX(41):  //MMC_CMD41
    Cmd = BIT_CMD_RESPONSE_EXPECT;
    break;
  case MMC_INDX(51):	//MMC_CMD51
    Cmd = BIT_CMD_RESPONSE_EXPECT |BIT_CMD_CHECK_RESPONSE_CRC |
           BIT_CMD_DATA_EXPECTED | BIT_CMD_READ |
           BIT_CMD_WAIT_PRVDATA_COMPLETE;
    break;
  default:
    Cmd = BIT_CMD_RESPONSE_EXPECT | BIT_CMD_CHECK_RESPONSE_CRC;
    break;
  }

  Cmd |= MMC_GET_INDX(MmcCmd) | BIT_CMD_USE_HOLD_REG | BIT_CMD_START;

  if (IsPendingReadCommand (Cmd) || IsPendingWriteCommand (Cmd)) {
    mDwEmmcCommand = Cmd;
    mDwEmmcArgument = Argument;
  } else {
    Status = SendCommand (Cmd, Argument);
  }
  return Status;
}

EFI_STATUS
DwEmmcReceiveResponse (
  IN EFI_MMC_HOST_PROTOCOL     *This,
  IN MMC_RESPONSE_TYPE          Type,
  IN UINT32*                    Buffer
  )
{
  if (Buffer == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (   (Type == MMC_RESPONSE_TYPE_R1)
      || (Type == MMC_RESPONSE_TYPE_R1b)
      || (Type == MMC_RESPONSE_TYPE_R3)
      || (Type == MMC_RESPONSE_TYPE_R6)
      || (Type == MMC_RESPONSE_TYPE_R7))
  {
    Buffer[0] = MmioRead32 (DWEMMC_RESP0);
  } else if (Type == MMC_RESPONSE_TYPE_R2) {
    Buffer[0] = MmioRead32 (DWEMMC_RESP0);
    Buffer[1] = MmioRead32 (DWEMMC_RESP1);
    Buffer[2] = MmioRead32 (DWEMMC_RESP2);
    Buffer[3] = MmioRead32 (DWEMMC_RESP3);
  }
  return EFI_SUCCESS;
}

EFI_STATUS
PrepareDmaData (
  IN DWEMMC_IDMAC_DESCRIPTOR*    IdmacDesc,
  IN UINTN                      Length,
  IN UINT32*                    Buffer,
  OUT UINTN                     *DescBytes
  )
{
  UINTN  Cnt, Blks, Idx, LastIdx;

  if ((Length == 0) || ((Length % DWEMMC_BLOCK_SIZE) != 0)) {
    return EFI_UNSUPPORTED;
  }

  Cnt = (Length + DWEMMC_DMA_BUF_SIZE - 1) / DWEMMC_DMA_BUF_SIZE;
  if (Cnt > DWEMMC_MAX_DESC_COUNT) {
    return EFI_BAD_BUFFER_SIZE;
  }

  if (((UINTN)Buffer > MAX_UINT32) ||
      (((UINTN)Buffer + Length - 1) > MAX_UINT32) ||
      ((UINTN)IdmacDesc > MAX_UINT32) ||
      (((UINTN)IdmacDesc + (Cnt * sizeof (DWEMMC_IDMAC_DESCRIPTOR)) - 1) > MAX_UINT32))
  {
    return EFI_UNSUPPORTED;
  }

  Blks = (Length + DWEMMC_BLOCK_SIZE - 1) / DWEMMC_BLOCK_SIZE;
  Length = DWEMMC_BLOCK_SIZE * Blks;

  ZeroMem (IdmacDesc, Cnt * sizeof (DWEMMC_IDMAC_DESCRIPTOR));
  for (Idx = 0; Idx < Cnt; Idx++) {
    (IdmacDesc + Idx)->Des0 = DWEMMC_IDMAC_DES0_OWN | DWEMMC_IDMAC_DES0_CH |
                              DWEMMC_IDMAC_DES0_DIC;
    (IdmacDesc + Idx)->Des1 = DWEMMC_IDMAC_DES1_BS1(DWEMMC_DMA_BUF_SIZE);
    /* Buffer Address */
    (IdmacDesc + Idx)->Des2 = (UINT32)((UINTN)Buffer + DWEMMC_DMA_BUF_SIZE * Idx);
    /* Next Descriptor Address */
    (IdmacDesc + Idx)->Des3 = (UINT32)((UINTN)IdmacDesc +
                                       (sizeof(DWEMMC_IDMAC_DESCRIPTOR) * (Idx + 1)));
  }
  /* First Descriptor */
  IdmacDesc->Des0 |= DWEMMC_IDMAC_DES0_FS;
  /* Last Descriptor */
  LastIdx = Cnt - 1;
  (IdmacDesc + LastIdx)->Des0 |= DWEMMC_IDMAC_DES0_LD;
  (IdmacDesc + LastIdx)->Des0 &= ~(DWEMMC_IDMAC_DES0_DIC | DWEMMC_IDMAC_DES0_CH);
  (IdmacDesc + LastIdx)->Des1 = DWEMMC_IDMAC_DES1_BS1(Length -
                                                      (LastIdx * DWEMMC_DMA_BUF_SIZE));
  /* Set the Next field of Last Descriptor */
  (IdmacDesc + LastIdx)->Des3 = 0;
  *DescBytes = Cnt * sizeof (DWEMMC_IDMAC_DESCRIPTOR);
  WriteBackDataCacheRange (IdmacDesc, *DescBytes);
  MmioWrite32 (DWEMMC_DBADDR, (UINT32)((UINTN)IdmacDesc));

  return EFI_SUCCESS;
}

VOID
StartDma (
  UINTN    Length
  )
{
  UINT32 Data;

  Data = MmioRead32 (DWEMMC_CTRL);
  Data |= DWEMMC_CTRL_INT_EN | DWEMMC_CTRL_DMA_EN | DWEMMC_CTRL_IDMAC_EN;
  MmioWrite32 (DWEMMC_CTRL, Data);
  Data = MmioRead32 (DWEMMC_BMOD);
  Data |= DWEMMC_IDMAC_ENABLE | DWEMMC_IDMAC_FB;
  MmioWrite32 (DWEMMC_BMOD, Data);

  MmioWrite32 (DWEMMC_BLKSIZ, DWEMMC_BLOCK_SIZE);
  MmioWrite32 (DWEMMC_BYTCNT, Length);
}

STATIC
VOID
StopDma (
  VOID
  )
{
  UINT32 Data;

  Data = MmioRead32 (DWEMMC_CTRL);
  Data &= ~(DWEMMC_CTRL_DMA_EN | DWEMMC_CTRL_IDMAC_EN);
  MmioWrite32 (DWEMMC_CTRL, Data);
}

STATIC
EFI_STATUS
DwEmmcResetFifo (
  VOID
  )
{
  UINT32 Data;
  UINT32 TimeOut;
  UINT32 Value;

  Data = MmioRead32 (DWEMMC_CTRL);
  Data |= FIFO_RESET;
  MmioWrite32 (DWEMMC_CTRL, Data);

  TimeOut = 100000;
  while (((Value = MmioRead32 (DWEMMC_CTRL)) & FIFO_RESET) && (TimeOut > 0)) {
    TimeOut--;
  }
  if (TimeOut == 0) {
    DEBUG ((DEBUG_ERROR, "%a(): CMD=%d FIFO reset timeout\n", __func__, mDwEmmcCommand & 0x3f));
    return EFI_DEVICE_ERROR;
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
DwEmmcPrepareDataPath (
  VOID
  )
{
  UINT32 Data;

  if (mDwEmmcCommand & BIT_CMD_WAIT_PRVDATA_COMPLETE) {
    do {
      Data = MmioRead32 (DWEMMC_STATUS);
    } while (Data & DWEMMC_STS_DATA_BUSY);
  }

  if ((mDwEmmcCommand & BIT_CMD_STOP_ABORT_CMD) || (mDwEmmcCommand & BIT_CMD_DATA_EXPECTED)) {
    if (!(MmioRead32 (DWEMMC_STATUS) & FIFO_EMPTY)) {
      return DwEmmcResetFifo ();
    }
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
DwEmmcWaitDmaTransfer (
  IN UINTN                     Length,
  IN UINT32                    *Buffer
  )
{
  UINT32  Mask;
  UINTN   TimeOut;

  TimeOut = DWEMMC_DMA_TIMEOUT_US / DWEMMC_DMA_POLL_US;
  do {
    Mask = MmioRead32 (DWEMMC_RINTSTS);
    if (Mask & DWEMMC_DATA_ERROR_FLAGS) {
      DEBUG ((DEBUG_ERROR, "%a(): DMA error RINTSTS=0x%x\n", __func__, Mask));
      return EFI_DEVICE_ERROR;
    }

    if (Mask & DWEMMC_INT_DTO) {
      InvalidateDataCacheRange (Buffer, Length);
      MmioWrite32 (DWEMMC_RINTSTS, Mask);
      return EFI_SUCCESS;
    }

    MicroSecondDelay (DWEMMC_DMA_POLL_US);
  } while (--TimeOut > 0);

  DEBUG ((DEBUG_ERROR, "%a(): DMA timeout RINTSTS=0x%x Length=%u\n", __func__, MmioRead32 (DWEMMC_RINTSTS), Length));
  return EFI_TIMEOUT;
}

STATIC
EFI_STATUS
DwEmmcReadBlockDataPio (
  IN UINTN                      Length,
  IN UINT32                    *Buffer
  )
{
  EFI_STATUS Status;
  UINT32     DataLen;
  EFI_STATUS Ret;
  UINT32     TimeOut;

  DataLen = Length >> 2;
  Ret = EFI_SUCCESS;

  Status = DwEmmcPrepareDataPath ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  MmioWrite32 (DWEMMC_BLKSIZ, 512);
  MmioWrite32 (DWEMMC_BYTCNT, Length);

  Status = SendCommand (mDwEmmcCommand, mDwEmmcArgument);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to read data, mDwEmmcCommand:%x, mDwEmmcArgument:%x, Status:%r\n", mDwEmmcCommand, mDwEmmcArgument, Status));
    return EFI_DEVICE_ERROR;
  }

  DEBUG ((DW_DBG, "Sdmmc::SdmmcReadBlockDataPio DataLen=%d\n", DataLen));
  TimeOut = 1000000;
  while (DataLen) {
    if (MmioRead32 (DWEMMC_RINTSTS) & DWEMMC_DATA_ERROR_FLAGS) {
      DEBUG ((DEBUG_ERROR, "%a(): EFI_DEVICE_ERROR DWEMMC_RINTSTS=0x%x DataLen=%d\n",
        __func__, MmioRead32 (DWEMMC_RINTSTS), DataLen));
      return EFI_DEVICE_ERROR;
    }

    while ((!(MmioRead32 (DWEMMC_STATUS) & FIFO_EMPTY)) && DataLen) {
      *Buffer++ = MmioRead32 (DWEMMC_MSHCI_FIFO);
      DataLen--;
      TimeOut = 1000000;
    }

    if (!DataLen) {
      Ret = (MmioRead32 (DWEMMC_RINTSTS) & DWEMMC_DATA_ERROR_FLAGS) ?
        EFI_DEVICE_ERROR : EFI_SUCCESS;
      DEBUG ((DW_DBG, "%a(): DataLen end :%d\n", __func__, Ret));
      break;
    }

    NanoSecondDelay (1);
    TimeOut--;
    if (TimeOut == 0) {
      Ret = EFI_DEVICE_ERROR;
      DEBUG ((DEBUG_ERROR, "%a(): TimeOut! DataLen=%d\n", __func__, DataLen));
      break;
    }
  }

  return Ret;
}

STATIC
EFI_STATUS
DwEmmcReadBlockDataDma (
  IN UINTN                      Length,
  IN UINT32                    *Buffer
  )
{
  EFI_STATUS Status;
  UINTN      DescBytes;

  Status = DwEmmcPrepareDataPath ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = PrepareDmaData (gpIdmacDesc, Length, Buffer, &DescBytes);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = DwEmmcResetFifo ();
  if (EFI_ERROR (Status)) {
    return Status;
  }

  WriteBackInvalidateDataCacheRange (Buffer, Length);
  MmioWrite32 (DWEMMC_IDSTS, ~0);
  StartDma (Length);

  Status = SendCommand (mDwEmmcCommand, mDwEmmcArgument);
  if (EFI_ERROR (Status)) {
    StopDma ();
    DEBUG ((DEBUG_ERROR, "Failed to read DMA data, mDwEmmcCommand:%x, mDwEmmcArgument:%x, Status:%r\n", mDwEmmcCommand, mDwEmmcArgument, Status));
    return EFI_DEVICE_ERROR;
  }

  Status = DwEmmcWaitDmaTransfer (Length, Buffer);
  StopDma ();
  InvalidateDataCacheRange (gpIdmacDesc, DescBytes);
  return Status;
}

EFI_STATUS
DwEmmcReadBlockData (
  IN EFI_MMC_HOST_PROTOCOL     *This,
  IN EFI_LBA                    Lba,
  IN UINTN                      Length,
  IN UINT32*                   Buffer
  )
{
  EFI_STATUS Status;

  DEBUG ((DW_DBG, "%a():\n", __func__));

  if ((gpIdmacDesc != NULL) && (Length >= DWEMMC_BLOCK_SIZE) && ((Length % DWEMMC_BLOCK_SIZE) == 0)) {
    Status = DwEmmcReadBlockDataDma (Length, Buffer);
    if (Status != EFI_UNSUPPORTED) {
      return Status;
    }
  }

  return DwEmmcReadBlockDataPio (Length, Buffer);
}

#define MMC_GET_FCNT(x)		        (((x)>>17) & 0x1FF)

EFI_STATUS
DwEmmcWriteBlockData (
  IN EFI_MMC_HOST_PROTOCOL     *This,
  IN EFI_LBA                    Lba,
  IN UINTN                      Length,
  IN UINT32*                    Buffer
  )
{
  UINT32 *DataBuffer = Buffer;
  UINTN Count=0;
  UINTN Size32 = Length / 4;
  UINT32 Mask;
  EFI_STATUS	Status;
  UINT32		Data;
  UINT32		TimeOut = 0;
  UINT32		value = 0;

  DEBUG ((DW_DBG, "%a():\n", __func__));

  if (mDwEmmcCommand & BIT_CMD_WAIT_PRVDATA_COMPLETE) {
    do {
      Data = MmioRead32 (DWEMMC_STATUS);
    } while (Data & DWEMMC_STS_DATA_BUSY);
  }

  if (!(((mDwEmmcCommand&0x3f) == 6) || ((mDwEmmcCommand&0x3f) == 51))) {
    if ((mDwEmmcCommand & BIT_CMD_STOP_ABORT_CMD) || (mDwEmmcCommand & BIT_CMD_DATA_EXPECTED)) {
      if (!(MmioRead32 (DWEMMC_STATUS) & FIFO_EMPTY)) {
        Data = MmioRead32 (DWEMMC_CTRL);
        Data |= FIFO_RESET;
        MmioWrite32 (DWEMMC_CTRL, Data);

        TimeOut = 100000;
        while (((value = MmioRead32 (DWEMMC_CTRL)) & (FIFO_RESET)) && (TimeOut > 0)) {
          TimeOut--;
        }
        if (TimeOut == 0) {
          DEBUG ((DEBUG_ERROR, "%a():  CMD=%d SDC_SDC_ERROR\n", __func__, mDwEmmcCommand&0x3f));
          return EFI_DEVICE_ERROR;
        }
      }
    }
  }

  MmioWrite32 (DWEMMC_BLKSIZ, 512);
  MmioWrite32 (DWEMMC_BYTCNT, Length);

  Status = SendCommand (mDwEmmcCommand, mDwEmmcArgument);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "Failed to write data, mDwEmmcCommand:%x, mDwEmmcArgument:%x, Status:%r\n", mDwEmmcCommand, mDwEmmcArgument, Status));
    return EFI_DEVICE_ERROR;
  }

  for (Count = 0; Count < Size32; Count++) {
    while(MMC_GET_FCNT(MmioRead32(DWEMMC_STATUS)) >32)
    MicroSecondDelay(1);
    MmioWrite32((DWEMMC_MSHCI_FIFO), *DataBuffer++);
  }

  do {
    Mask = MmioRead32(DWEMMC_RINTSTS);
    if (Mask & DWEMMC_DATA_ERROR_FLAGS) {
      DEBUG((DEBUG_ERROR, "SdmmcWriteData error, RINTSTS = 0x%08x\n", Mask));
      return EFI_DEVICE_ERROR;
    }	
  } while (!(Mask & DWEMMC_INT_DTO));

  return EFI_SUCCESS;
}

EFI_STATUS
DwEmmcSetIos (
  IN EFI_MMC_HOST_PROTOCOL      *This,
  IN  UINT32                    BusClockFreq,
  IN  UINT32                    BusWidth,
  IN  UINT32                    TimingMode
  )
{
  EFI_STATUS Status = EFI_SUCCESS;
  UINT32    Data;

  if (TimingMode != EMMCBACKWARD) {
    Data = MmioRead32 (DWEMMC_UHSREG);
    switch (TimingMode) {
    case EMMCHS52DDR1V2:
    case EMMCHS52DDR1V8:
      Data |= 1 << 16;
      break;
    case EMMCHS52:
    case EMMCHS26:
      Data &= ~(1 << 16);
      break;
    default:
      return EFI_UNSUPPORTED;
    }
    MmioWrite32 (DWEMMC_UHSREG, Data);
  }

  switch (BusWidth) {
  case 1:
    MmioWrite32 (DWEMMC_CTYPE, 0);
    break;
  case 4:
    MmioWrite32 (DWEMMC_CTYPE, 1);
    break;
  case 8:
    MmioWrite32 (DWEMMC_CTYPE, 1 << 16);
    break;
  default:
    return EFI_UNSUPPORTED;
  }
  if (BusClockFreq) {
    Status = DwEmmcSetClock (BusClockFreq);
  }
  return Status;
}

BOOLEAN
DwEmmcIsMultiBlock (
  IN EFI_MMC_HOST_PROTOCOL      *This
  )
{
  return TRUE;
}

EFI_MMC_HOST_PROTOCOL gMciHost = {
  MMC_HOST_PROTOCOL_REVISION,
  DwEmmcIsCardPresent,
  DwEmmcIsReadOnly,
  DwEmmcBuildDevicePath,
  DwEmmcNotifyState,
  DwEmmcSendCommand,
  DwEmmcReceiveResponse,
  DwEmmcReadBlockData,
  DwEmmcWriteBlockData,
  DwEmmcSetIos,
  DwEmmcIsMultiBlock
};

EFI_STATUS
DwEmmcIomux (
  void
  )
{
  UINT32 PllRate;

  DEBUG ((DW_DBG, "%a():\n", __func__));

  CruWritel((0x1 << ( 10 + 16)) | (1 << 10), CRU_SOFTRSTS_CON(7));
  MicroSecondDelay(5);
  CruWritel((0x1 << ( 10 + 16)) | (0 << 10), CRU_SOFTRSTS_CON(7));

  PllRate = rk3399_pll_get_rate(PLL_GPLL);
  DEBUG ((DW_DBG, "%a(): GPLL PllRate=%d\n", __func__, PllRate));

  if (PllRate == 800000000) {
    /*
     * Select GPLL for clk_sdmmc_pll_sel.
     */
    CruWritel((0x7 << ( 8 + 16)) | (0x1 << 8), CRU_CLKSELS_CON(16));
    /*
     * 100MHz = clk = clk_src / (clk_sddmmc_div_con + 1), clk_sddmmc_div_con == 3.
     */
    CruWritel((0x7f << ( 0 + 16)) | (0x7 << 0), CRU_CLKSELS_CON(16));
    /*
     * Ungate clk_sdmmc_src_en.
     */
    CruWritel((0x1 << ( 1 + 16)) | (0 << 1), CRU_CLKGATES_CON(6));
  } else {
    DEBUG ((DEBUG_ERROR, "%a(): GPLL != 800MHz, set sdmmc clock error!!!\n", __func__));
  }

  GrfWritel((0x3 << ( 8 + 16)) | (0x1 << 8) ,GRF_GPIO4B_IOMUX);
  GrfWritel((0x3 << ( 10 + 16)) | (0x1 << 10) ,GRF_GPIO4B_IOMUX);
  GrfWritel((0x3 << ( 0 + 16)) | (0x1 << 0) ,GRF_GPIO4B_IOMUX);
  GrfWritel((0x3 << ( 2 + 16)) | (0x1 << 2) ,GRF_GPIO4B_IOMUX);
  GrfWritel((0x3 << ( 4 + 16)) | (0x1 << 4) ,GRF_GPIO4B_IOMUX);
  GrfWritel((0x3 << ( 6 + 16)) | (0x1 << 6) ,GRF_GPIO4B_IOMUX);
  PmuGrfWritel((0x3  << ( 14 + 16)) | (0x1 << 14) ,PMU_GRF_GPIO0A_IOMUX);
  PmuGrfWritel((0x3  << ( 0 + 16)) | (0x1 << 0) ,PMU_GRF_GPIO0B_IOMUX);

  return EFI_SUCCESS;
}

EFI_STATUS
DwEmmcDxeInitialize (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
{
  EFI_STATUS    Status;
  EFI_HANDLE    Handle;

  DwEmmcIomux();

  Handle = NULL;

  gpIdmacDesc = (DWEMMC_IDMAC_DESCRIPTOR *)AllocatePages (DWEMMC_MAX_DESC_PAGES);
  if (gpIdmacDesc == NULL) {
    return EFI_BUFFER_TOO_SMALL;
  }

  DEBUG ((DEBUG_BLKIO, "DwEmmcDxeInitialize()\n"));

  //Publish Component Name, BlockIO protocol interfaces
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &Handle,
                  &gEmbeddedMmcHostProtocolGuid,         &gMciHost,
                  NULL
                  );
  ASSERT_EFI_ERROR (Status);

  return EFI_SUCCESS;
}
