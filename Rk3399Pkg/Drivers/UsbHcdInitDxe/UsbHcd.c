/** @file
 *
 * Copyright (C) 2010-2011 Texas Instruments Incorporated - http://www.ti.com
 * Copyright 2015 Freescale Semiconductor, Inc.
 * Copyright (C) 2016 Rockchip, Inc.
 * Copyright 2017 NXP
 * Copyright (c) 2019, Andrey Warkentin <andrey.warkentin@gmail.com>
 *
 * SPDX-License-Identifier:     GPL-2.0
 */

#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/NonDiscoverableDeviceRegistrationLib.h>
#include <Library/UefiLib.h>
#include <Library/TimerLib.h>
#include <Rk3399/Rk3399.h>
#include <Rk3399/Rk3399Cru.h>
#include <Rk3399/Rk3399Grf.h>
#include <Library/I2CLib.h>

/* Global Configuration Register */
#define DWC3_GCTL_PWRDNSCALE(n)                 ((n) << 19)
#define DWC3_GCTL_U2RSTECN                      (1 << 16)
#define DWC3_GCTL_RAMCLKSEL(x)                  \
                (((x) & DWC3_GCTL_CLK_MASK) << 6)
#define DWC3_GCTL_CLK_BUS                       (0)
#define DWC3_GCTL_CLK_PIPE                      (1)
#define DWC3_GCTL_CLK_PIPEHALF                  (2)
#define DWC3_GCTL_CLK_MASK                      (3)
#define DWC3_GCTL_PRTCAP(n)                     (((n) & (3 << 12)) >> 12)
#define DWC3_GCTL_PRTCAPDIR(n)                  ((n) << 12)
#define DWC3_GCTL_PRTCAP_HOST                   1
#define DWC3_GCTL_PRTCAP_DEVICE                 2
#define DWC3_GCTL_PRTCAP_OTG                    3
#define DWC3_GCTL_CORESOFTRESET                 (1 << 11)
#define DWC3_GCTL_SCALEDOWN(n)                  ((n) << 4)
#define DWC3_GCTL_SCALEDOWN_MASK                DWC3_GCTL_SCALEDOWN(3)
#define DWC3_GCTL_DISSCRAMBLE                   (1 << 3)
#define DWC3_GCTL_DSBLCLKGTNG                   (1 << 0)

/* Global HWPARAMS1 Register */
#define DWC3_GHWPARAMS1_EN_PWROPT(n)            (((n) & (3 << 24)) >> 24)
#define DWC3_GHWPARAMS1_EN_PWROPT_NO            0
#define DWC3_GHWPARAMS1_EN_PWROPT_CLK           1

/* Global USB2 PHY Configuration Register */
#define DWC3_GUSB2PHYCFG_PHYSOFTRST             (1 << 31)
#define DWC3_GUSB2PHYCFG_U2_FREECLK_EXISTS      (1 << 30)
#define DWC3_GUSB2PHYCFG_ENBLSLPM               (1 << 8)
#define DWC3_GUSB2PHYCFG_SUSPHY                 (1 << 6)
#define DWC3_GUSB2PHYCFG_PHYIF                  (1 << 3)

/* Global USB2 PHY Configuration Mask */
#define DWC3_GUSB2PHYCFG_USBTRDTIM_MASK         (0xf << 10)

/* Global USB2 PHY Configuration Offset */
#define DWC3_GUSB2PHYCFG_USBTRDTIM_OFFSET       10

#define DWC3_GUSB2PHYCFG_USBTRDTIM_16BIT (0x5 << \
                DWC3_GUSB2PHYCFG_USBTRDTIM_OFFSET)
#define DWC3_GUSB2PHYCFG_USBTRDTIM_8BIT (0x9 << \
                DWC3_GUSB2PHYCFG_USBTRDTIM_OFFSET)

/* Global USB3 PIPE Control Register */
#define DWC3_GUSB3PIPECTL_PHYSOFTRST            (1 << 31)
#define DWC3_GUSB3PIPECTL_DISRXDETP3            (1 << 28)
#define DWC3_GUSB3PIPECTL_SUSPHY                (1 << 17)

/* Global TX Fifo Size Register */
#define DWC3_GTXFIFOSIZ_TXFDEF(n)               ((n) & 0xffff)
#define DWC3_GTXFIFOSIZ_TXFSTADDR(n)            ((n) & 0xffff0000)

/* Device Control Register */
#define DWC3_DCTL_RUN_STOP                      (1 << 31)
#define DWC3_DCTL_CSFTRST                       (1 << 30)
#define DWC3_DCTL_LSFTRST                       (1 << 29)

/* Global Frame Length Adjustment Register */
#define GFLADJ_30MHZ_REG_SEL                    (1 << 7)
#define GFLADJ_30MHZ(n)                         ((n) & 0x3f)
#define GFLADJ_30MHZ_DEFAULT                    0x20

#define DWC3_GSNPSID_MASK                       0xffff0000
#define DWC3_REVISION_MASK                      0xffff

#pragma pack(push, 1)
struct rockchip_usb_dwc3 {
        UINT32 sbuscfg0;
        UINT32 sbuscfg1;
        UINT32 txthrcfg;
        UINT32 rxthrcfg;
        UINT32 ctl;
        UINT32 evten;
        UINT32 sts;
        UINT8 reserved0[4];
        UINT32 snpsid;
        UINT32 gpio;
        UINT32 uid;
        UINT32 uctl;
        UINT64 buserraddr;
        UINT64 prtbimap;
        UINT32 hwparams0;
        UINT32 hwparams1;
        UINT32 hwparams2;
        UINT32 hwparams3;
        UINT32 hwparams4;
        UINT32 hwparams5;
        UINT32 hwparams6;
        UINT32 hwparams7;
        UINT32 dbgfifospace;
        UINT32 dbgltssm;
        UINT32 dbglnmcc;
        UINT32 dbgbmu;
        UINT32 dbglspmux;
        UINT32 dbglsp;
        UINT32 dbgepinfo0;
        UINT32 dbgepinfo1;
        UINT64 prtbimap_hs;
        UINT64 prtbimap_fs;
        UINT8 reserved2[112];
        UINT32 usb2phycfg;
        UINT8 reserved3[60];
        UINT32 usb2i2cctl;
        UINT8 reserved4[60];
        UINT32 usb2phyacc;
        UINT8 reserved5[60];
        UINT32 usb3pipectl;
        UINT8 reserved6[60];
} *otg0 = (VOID *) RK3399_OTG0_DWC3_BASE,
  *otg1 = (VOID *) RK3399_OTG1_DWC3_BASE;
#pragma pack(pop)

STATIC VOID
rockchip_dwc3_phy_setup(
  IN  struct rockchip_usb_dwc3 *dwc3
  )
{
  UINT32 reg;
  // UINT32 utmi_bits;

  /* Set dwc3 usb2 phy config */
  reg = MmioRead32((UINTN) &dwc3->usb2phycfg);

  // if (dev_read_bool(dev, "snps,dis-enblslpm-quirk"))
  reg &= ~DWC3_GUSB2PHYCFG_ENBLSLPM;

  // utmi_bits = dev_read_u32_default(dev, "snps,phyif-utmi-bits", -1);
  // if (utmi_bits == 16) {
  reg |= DWC3_GUSB2PHYCFG_PHYIF;
  reg &= ~DWC3_GUSB2PHYCFG_USBTRDTIM_MASK;
  reg |= DWC3_GUSB2PHYCFG_USBTRDTIM_16BIT;
  // } else if (utmi_bits == 8) {
  // reg &= ~DWC3_GUSB2PHYCFG_PHYIF;
  // reg &= ~DWC3_GUSB2PHYCFG_USBTRDTIM_MASK;
  // reg |= DWC3_GUSB2PHYCFG_USBTRDTIM_8BIT;
  // }

  // if (dev_read_bool(dev, "snps,dis-u2-freeclk-exists-quirk"))
  reg &= ~DWC3_GUSB2PHYCFG_U2_FREECLK_EXISTS;

  // if (dev_read_bool(dev, "snps,dis-u2-susphy-quirk"))
  reg &= ~DWC3_GUSB2PHYCFG_SUSPHY;

  MmioWrite32((UINTN) &dwc3->usb2phycfg, reg);
}

STATIC VOID
dwc3_set_mode(
  IN  struct rockchip_usb_dwc3 *dwc3,
  UINT32 mode
  )
{
  MmioAndThenOr32((UINTN) &dwc3->ctl,
                  ~DWC3_GCTL_PRTCAPDIR(DWC3_GCTL_PRTCAP_OTG),
                  DWC3_GCTL_PRTCAPDIR(mode));
}

STATIC VOID
dwc3_phy_reset(
  IN  struct rockchip_usb_dwc3 *dwc3
  )
{
  /* Assert USB3 PHY reset */
  MmioOr32((UINTN) &dwc3->usb3pipectl, DWC3_GUSB3PIPECTL_PHYSOFTRST);

  /* Assert USB2 PHY reset */
  MmioOr32((UINTN) &dwc3->usb2phycfg, DWC3_GUSB2PHYCFG_PHYSOFTRST);

  MicroSecondDelay(100000);

  /* Clear USB3 PHY reset */
  MmioAnd32((UINTN) &dwc3->usb3pipectl, ~DWC3_GUSB3PIPECTL_PHYSOFTRST);

  /* Clear USB2 PHY reset */
  MmioAnd32((UINTN) &dwc3->usb2phycfg, ~DWC3_GUSB2PHYCFG_PHYSOFTRST);
}

STATIC VOID
dwc3_core_soft_reset(
  IN  struct rockchip_usb_dwc3 *dwc3
  )
{
  /* Before Resetting PHY, put Core in Reset */
  MmioOr32((UINTN) &dwc3->ctl, DWC3_GCTL_CORESOFTRESET);

  /* reset USB3 phy - if required */
  dwc3_phy_reset(dwc3);

  MicroSecondDelay(100000);

  /* After PHYs are stable we can take Core out of reset state */
  MmioAnd32((UINTN) &dwc3->ctl, ~DWC3_GCTL_CORESOFTRESET);
}

STATIC EFI_STATUS
dwc3_core_init(
  IN  struct rockchip_usb_dwc3 *dwc3
  )
{
  UINT32 reg;
  UINT32 revision;
  unsigned int dwc3_hwparams1;

  revision = MmioRead32((UINTN) &dwc3->snpsid);
  /* This should read as U3 followed by revision number */
  if ((revision & DWC3_GSNPSID_MASK) != 0x55330000) {
    DEBUG((EFI_D_ERROR, "This is not a DesignWare USB3 DRD Core\n"));
    return EFI_UNSUPPORTED;
  }

  dwc3_core_soft_reset(dwc3);

  dwc3_hwparams1 = MmioRead32((UINTN) &dwc3->hwparams1);

  reg = MmioRead32((UINTN) &dwc3->ctl);
  reg &= ~DWC3_GCTL_SCALEDOWN_MASK;
  reg &= ~DWC3_GCTL_DISSCRAMBLE;
  switch (DWC3_GHWPARAMS1_EN_PWROPT(dwc3_hwparams1)) {
  case DWC3_GHWPARAMS1_EN_PWROPT_CLK:
    reg &= ~DWC3_GCTL_DSBLCLKGTNG;
    break;
  default:
    DEBUG((EFI_D_INFO, "No power optimization available\n"));
  }

  /*
   * WORKAROUND: DWC3 revisions <1.90a have a bug
   * where the device can fail to connect at SuperSpeed
   * and falls back to high-speed mode which causes
   * the device to enter a Connect/Disconnect loop
   */
  if ((revision & DWC3_REVISION_MASK) < 0x190a)
    reg |= DWC3_GCTL_U2RSTECN;

  MmioWrite32((UINTN) &dwc3->ctl, reg);
  return EFI_SUCCESS;
}

STATIC EFI_STATUS
rockchip_xhci_core_init(
  IN  struct rockchip_usb_dwc3 *dwc3
  )
{
  EFI_STATUS Status;

  Status = dwc3_core_init(dwc3);
  if (EFI_ERROR (Status)) {
    DEBUG((EFI_D_ERROR, "Failed to initialize dwc3 core: %r\n", Status));
    return Status;
  }

  rockchip_dwc3_phy_setup(dwc3);

  /* We are hard-coding DWC3 core to Host Mode */
  dwc3_set_mode(dwc3, DWC3_GCTL_PRTCAP_HOST);

  return EFI_SUCCESS;
}


/**
  The Entry Point of module. It follows the standard UEFI driver model.

  @param[in] ImageHandle   The firmware allocated handle for the EFI image.
  @param[in] SystemTable   A pointer to the EFI System Table.

  @retval EFI_SUCCESS      The entry point is executed successfully.
  @retval other            Some error occurs when executing this entry point.

**/
EFI_STATUS
EFIAPI
InitializeUsbHcd (
  IN EFI_HANDLE            ImageHandle,
  IN EFI_SYSTEM_TABLE      *SystemTable
  )
{
  EFI_STATUS Status;

  /* Initialize fusb302 Type-C controller for USB Host (DFP) mode + VBUS */
  /* fusb302 on I2C4, address 0x22 */
  {
    #define FUSB302_ADDR         0x22
    #define FUSB_REG_DEVICEID    0x01
    #define FUSB_REG_SWITCHES0   0x02
    #define FUSB_REG_SWITCHES1   0x03
    #define FUSB_REG_MEASURE     0x04
    #define FUSB_REG_CONTROL0    0x06
    #define FUSB_REG_CONTROL2    0x08
    #define FUSB_REG_CONTROL3    0x09
    #define FUSB_REG_MASK        0x0A
    #define FUSB_REG_POWER       0x0B
    #define FUSB_REG_RESET       0x0C
    #define FUSB_REG_MASKA       0x0E
    #define FUSB_REG_MASKB       0x0F
    #define FUSB_REG_STATUS0     0x40
    #define FUSB_REG_STATUS1     0x41

    UINT8 val;
    EFI_STATUS I2cStatus;

    DEBUG((DEBUG_ERROR, "USB: Init fusb302 on I2C4 addr 0x%02x\n", FUSB302_ADDR));

    /* Enable I2C4 clock: PMU_CRU CLKGATE_CON1 bit 8 */
    {
      volatile UINT32 *pmu_cru_gate1 = (volatile UINT32*)(0xFF750000 + 0x100 + 1*4);
      *pmu_cru_gate1 = (1u << (8+16)); /* write-enable bit 8, clear gate */
    }
    /* Enable I2C4 source clock: PMU_CRU CLKSEL_CON3 for i2c4 */
    {
      volatile UINT32 *pmu_cru_sel3 = (volatile UINT32*)(0xFF750000 + 0x080 + 3*4);
      *pmu_cru_sel3 = (0x7Fu << (0+16)) | (0x03u << 0); /* div=4 from PPLL */
    }

    I2cStatus = I2CInit(I2C_CH4, 200000);
    if (EFI_ERROR(I2cStatus)) {
      DEBUG((DEBUG_ERROR, "USB: I2C4 init failed: %r\n", I2cStatus));
      goto fusb_done;
    }

    /* Read device ID to verify communication */
    I2cStatus = I2CRead(I2C_CH4, FUSB302_ADDR, FUSB_REG_DEVICEID, 1, &val, 1);
    if (EFI_ERROR(I2cStatus)) {
      DEBUG((DEBUG_ERROR, "USB: fusb302 read ID failed: %r\n", I2cStatus));
      goto fusb_done;
    }
    DEBUG((DEBUG_ERROR, "USB: fusb302 DeviceID=0x%02x\n", val));

    /* Software reset */
    val = 0x03; /* PD_RESET | SW_RES */
    I2CWrite(I2C_CH4, FUSB302_ADDR, FUSB_REG_RESET, 1, &val, 1);
    MicroSecondDelay(10000);

    /* Power on all internal blocks */
    val = 0x0F; /* PWR_ALL */
    I2CWrite(I2C_CH4, FUSB302_ADDR, FUSB_REG_POWER, 1, &val, 1);

    /* Mask all interrupts (we poll, don't use IRQ) */
    val = 0xFF;
    I2CWrite(I2C_CH4, FUSB302_ADDR, FUSB_REG_MASK, 1, &val, 1);
    I2CWrite(I2C_CH4, FUSB302_ADDR, FUSB_REG_MASKA, 1, &val, 1);
    I2CWrite(I2C_CH4, FUSB302_ADDR, FUSB_REG_MASKB, 1, &val, 1);

    /* SWITCHES0: Enable CC1 and CC2 pull-up (Rp) for host/source mode */
    val = 0xCC; /* PU_EN1 | PU_EN2 | MEAS_CC1 | MEAS_CC2 ... actually:
                   bit7=PU_EN2, bit6=PU_EN1, bit3=MEAS_CC2, bit2=MEAS_CC1
                   0xCC = 1100 1100 = PU_EN2 + PU_EN1 + MEAS_CC2 + MEAS_CC1 */
    val = 0xC4; /* PU_EN2(bit7) + PU_EN1(bit6) + MEAS_CC1(bit2) = 1100 0100 */
    I2CWrite(I2C_CH4, FUSB302_ADDR, FUSB_REG_SWITCHES0, 1, &val, 1);

    /* CONTROL2: Set DFP mode with toggle */
    val = 0x25; /* TOG_RD_ONLY(bit5) + MODE_DFP(bits[2:1]=10) + TOGGLE(bit0) */
    I2CWrite(I2C_CH4, FUSB302_ADDR, FUSB_REG_CONTROL2, 1, &val, 1);

    /* MEASURE: Set host current detection level (default Rp) */
    val = 0x31; /* MDAC for default USB current */
    I2CWrite(I2C_CH4, FUSB302_ADDR, FUSB_REG_MEASURE, 1, &val, 1);

    DEBUG((DEBUG_ERROR, "USB: fusb302 configured as DFP (host)\n"));

    /* Enable OTG 5V boost via RK818 PMIC (I2C0, addr 0x1C) */
    /* DCDC_BOOST enable: RK818_DCDC_EN_REG (0x23) bit 4 */
    {
      #define RK818_ADDR  0x1C
      UINT8 val;

      I2CInit(I2C_CH0, 200000);
      I2CRead(I2C_CH0, RK818_ADDR, 0x23, 1, &val, 1);
      DEBUG((DEBUG_ERROR, "USB: RK818 DCDC_EN(0x23)=0x%02x\n", val));
      val |= (1u << 4);  /* BOOST enable */
      I2CWrite(I2C_CH0, RK818_ADDR, 0x23, 1, &val, 1);
      I2CRead(I2C_CH0, RK818_ADDR, 0x23, 1, &val, 1);
      DEBUG((DEBUG_ERROR, "USB: RK818 DCDC_EN after=0x%02x (boost on)\n", val));
    }

    /* Wait for VBUS ramp + device enumeration */
    MicroSecondDelay(1000000); /* 1 second */

    /* Read status to check connection */
    I2CRead(I2C_CH4, FUSB302_ADDR, FUSB_REG_STATUS0, 1, &val, 1);
    DEBUG((DEBUG_ERROR, "USB: fusb302 STATUS0=0x%02x\n", val));
    I2CRead(I2C_CH4, FUSB302_ADDR, FUSB_REG_STATUS1, 1, &val, 1);
    DEBUG((DEBUG_ERROR, "USB: fusb302 STATUS1=0x%02x\n", val));

fusb_done:
    ;
  }

  rockchip_xhci_core_init(otg0);
  rockchip_xhci_core_init(otg1);

  Status = RegisterNonDiscoverableMmioDevice(
                                             NonDiscoverableDeviceTypeXhci,
                                             NonDiscoverableDeviceDmaTypeNonCoherent,
                                             NULL,
                                             NULL,
                                             1,
                                             RK3399_OTG0_DWC3_XHCI_BASE, RK3399_OTG_REGS_SIZE
                                             );
  
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR,
            "Failed to register USB device (0x%x): %r\n",
            RK3399_OTG0_DWC3_XHCI_BASE, Status));
  }

  Status = RegisterNonDiscoverableMmioDevice(
                                             NonDiscoverableDeviceTypeXhci,
                                             NonDiscoverableDeviceDmaTypeNonCoherent,
                                             NULL,
                                             NULL,
                                             1,
                                              RK3399_OTG1_DWC3_XHCI_BASE, RK3399_OTG_REGS_SIZE
                                             );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR,
            "Failed to register USB device (0x%x): %r\n",
            RK3399_OTG1_DWC3_XHCI_BASE, Status));
  }

  return Status;
}
