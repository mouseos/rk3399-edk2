/** @file

  Differentiated System Description Table Fields (DSDT)

  Copyright (c) 2018, Linaro Ltd. All rights reserved.<BR>
  Copyright (c) 2019, Andrey Warkentin <andrey.warkentin@gmail.com>

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

DefinitionBlock ("DSDT.aml", "DSDT", 2, "RKCP  ", "RK3399  ", 3)
{
    Scope (_SB)
    {
        /*
         * ACPI namespace derived from:
         *   zmooth_Source/kernel/arch/arm64/boot/dts/rockchip/
         *   rk3399-evb-rev3-android-lp4.dts
         *
         * Keep the ACPI namespace conservative. Linux Rockchip drivers are
         * mostly DT-oriented, so incomplete PRP0001 devices can bind without
         * their required clocks, resets, phys, or syscon links and destabilize
         * boot. Enable devices here only after their ACPI description is enough.
         */
        Device (CPU0)
        {
            Name (_HID, "ACPI0007" /* Processor Device */)  // _HID: Hardware ID
            Name (_UID, 0x000)  // _UID: Unique ID
        }
        Device (CPU1)
        {
            Name (_HID, "ACPI0007" /* Processor Device */)  // _HID: Hardware ID
            Name (_UID, 0x001)  // _UID: Unique ID
        }
        Device (CPU2)
        {
            Name (_HID, "ACPI0007" /* Processor Device */)  // _HID: Hardware ID
            Name (_UID, 0x002)  // _UID: Unique ID
        }
        Device (CPU3)
        {
            Name (_HID, "ACPI0007" /* Processor Device */)  // _HID: Hardware ID
            Name (_UID, 0x003)  // _UID: Unique ID
        }
        Device (CPU4)
        {
            Name (_HID, "ACPI0007" /* Processor Device */)  // _HID: Hardware ID
            Name (_UID, 0x010)  // _UID: Unique ID
        }
        Device (CPU5)
        {
            Name (_HID, "ACPI0007" /* Processor Device */)  // _HID: Hardware ID
            Name (_UID, 0x011)  // _UID: Unique ID
        }

        Device (CRU0)
        {
            Name (_HID, "RKCP0001")
            Name (_CID, Package () { "PRP0001" })
            Name (_UID, 0x00)
            Method (_STA, 0, NotSerialized) { Return (0x00) }
            Name (_DSD, Package ()
            {
                ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
                Package ()
                {
                    Package () { "compatible", "rockchip,rk3399-cru" },
                }
            })
            Name (_CRS, ResourceTemplate ()
            {
                Memory32Fixed (ReadWrite, 0xFF760000, 0x00001000)
            })
        }

        Device (PCRU)
        {
            Name (_HID, "RKCP0002")
            Name (_CID, Package () { "PRP0001" })
            Name (_UID, 0x00)
            Method (_STA, 0, NotSerialized) { Return (0x00) }
            Name (_DSD, Package ()
            {
                ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
                Package ()
                {
                    Package () { "compatible", "rockchip,rk3399-pmucru" },
                }
            })
            Name (_CRS, ResourceTemplate ()
            {
                Memory32Fixed (ReadWrite, 0xFF750000, 0x00001000)
            })
        }

        Device (GRF0)
        {
            Name (_HID, "RKCP0003")
            Name (_CID, Package () { "PRP0001" })
            Name (_UID, 0x00)
            Method (_STA, 0, NotSerialized) { Return (0x00) }
            Name (_DSD, Package ()
            {
                ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
                Package ()
                {
                    Package () { "compatible", Package () { "rockchip,rk3399-grf", "syscon", "simple-mfd" } },
                }
            })
            Name (_CRS, ResourceTemplate ()
            {
                Memory32Fixed (ReadWrite, 0xFF770000, 0x00010000)
            })
        }

        Device (PGRF)
        {
            Name (_HID, "RKCP0004")
            Name (_CID, Package () { "PRP0001" })
            Name (_UID, 0x00)
            Method (_STA, 0, NotSerialized) { Return (0x00) }
            Name (_DSD, Package ()
            {
                ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
                Package ()
                {
                    Package () { "compatible", Package () { "rockchip,rk3399-pmugrf", "syscon", "simple-mfd" } },
                }
            })
            Name (_CRS, ResourceTemplate ()
            {
                Memory32Fixed (ReadWrite, 0xFF320000, 0x00001000)
            })
        }

        Device (GPI0)
        {
            Name (_HID, "RKCP0010")
            Name (_CID, Package () { "PRP0001" })
            Name (_UID, 0x00)
            Method (_STA, 0, NotSerialized) { Return (0x00) }
            Name (_DSD, Package ()
            {
                ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
                Package () { Package () { "compatible", "rockchip,gpio-bank" } }
            })
            Name (_CRS, ResourceTemplate ()
            {
                Memory32Fixed (ReadWrite, 0xFF720000, 0x00000100)
                Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive, ,, ) { 46 }
            })
        }

        Device (GPI1)
        {
            Name (_HID, "RKCP0010")
            Name (_CID, Package () { "PRP0001" })
            Name (_UID, 0x01)
            Method (_STA, 0, NotSerialized) { Return (0x00) }
            Name (_DSD, Package ()
            {
                ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
                Package () { Package () { "compatible", "rockchip,gpio-bank" } }
            })
            Name (_CRS, ResourceTemplate ()
            {
                Memory32Fixed (ReadWrite, 0xFF730000, 0x00000100)
                Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive, ,, ) { 47 }
            })
        }

        Device (GPI2)
        {
            Name (_HID, "RKCP0010")
            Name (_CID, Package () { "PRP0001" })
            Name (_UID, 0x02)
            Method (_STA, 0, NotSerialized) { Return (0x00) }
            Name (_DSD, Package ()
            {
                ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
                Package () { Package () { "compatible", "rockchip,gpio-bank" } }
            })
            Name (_CRS, ResourceTemplate ()
            {
                Memory32Fixed (ReadWrite, 0xFF780000, 0x00000100)
                Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive, ,, ) { 48 }
            })
        }

        Device (GPI3)
        {
            Name (_HID, "RKCP0010")
            Name (_CID, Package () { "PRP0001" })
            Name (_UID, 0x03)
            Method (_STA, 0, NotSerialized) { Return (0x00) }
            Name (_DSD, Package ()
            {
                ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
                Package () { Package () { "compatible", "rockchip,gpio-bank" } }
            })
            Name (_CRS, ResourceTemplate ()
            {
                Memory32Fixed (ReadWrite, 0xFF788000, 0x00000100)
                Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive, ,, ) { 49 }
            })
        }

        Device (GPI4)
        {
            Name (_HID, "RKCP0010")
            Name (_CID, Package () { "PRP0001" })
            Name (_UID, 0x04)
            Method (_STA, 0, NotSerialized) { Return (0x00) }
            Name (_DSD, Package ()
            {
                ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
                Package () { Package () { "compatible", "rockchip,gpio-bank" } }
            })
            Name (_CRS, ResourceTemplate ()
            {
                Memory32Fixed (ReadWrite, 0xFF790000, 0x00000100)
                Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive, ,, ) { 50 }
            })
        }

        Device (SDIO)
        {
            Name (_HID, "RKCP0020")
            Name (_CID, Package () { "PRP0001" })
            Name (_UID, 0x00)
            Name (_CCA, 0x00)
            Method (_STA, 0, NotSerialized) { Return (0x00) }
            Name (_DSD, Package ()
            {
                ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
                Package ()
                {
                    Package () { "compatible", Package () { "rockchip,rk3399-dw-mshc", "rockchip,rk3288-dw-mshc" } },
                    Package () { "bus-width", 4 },
                    Package () { "cap-sd-highspeed", 1 },
                    Package () { "cap-sdio-irq", 1 },
                    Package () { "keep-power-in-suspend", 1 },
                    Package () { "non-removable", 1 },
                    Package () { "supports-sdio", 1 },
                    Package () { "fifo-depth", 0x100 },
                }
            })
            Name (_CRS, ResourceTemplate ()
            {
                Memory32Fixed (ReadWrite, 0xFE310000, 0x00004000)
                Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive, ,, ) { 96 }
            })
        }

        Device (SDMC)
        {
            Name (_HID, "PRP0001")
            Name (_CID, Package () { "RKCP0020" })
            Name (_UID, 0x01)
            Name (_CCA, 0x00)
            Method (_STA, 0, NotSerialized) { Return (0x0F) }
            Name (_DSD, Package ()
            {
                ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
                Package ()
                {
                    Package () { "compatible", "snps,dw-mshc" },
                    Package () { "bus-width", 4 },
                    Package () { "clock-frequency", 50000000 },
                    Package () { "clock-freq-min-max", Package () { 400000, 50000000 } },
                    Package () { "cap-sd-highspeed", 1 },
                    Package () { "broken-cd", 1 },
                    Package () { "disable-wp", 1 },
                    Package () { "supports-sd", 1 },
                    Package () { "fifo-depth", 0x100 },
                }
            })
            Name (_CRS, ResourceTemplate ()
            {
                Memory32Fixed (ReadWrite, 0xFE320000, 0x00004000)
                Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive, ,, ) { 97 }
            })
        }

        Device (EMMC)
        {
            Name (_HID, "RKCP0021")
            Name (_CID, Package () { "PRP0001" })
            Name (_UID, 0x00)
            Name (_CCA, 0x00)
            Method (_STA, 0, NotSerialized) { Return (0x00) }
            Name (_DSD, Package ()
            {
                ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
                Package ()
                {
                    Package () { "compatible", Package () { "rockchip,rk3399-sdhci-5.1", "arasan,sdhci-5.1" } },
                    Package () { "bus-width", 8 },
                    Package () { "mmc-hs400-1_8v", 1 },
                    Package () { "mmc-hs400-enhanced-strobe", 1 },
                    Package () { "non-removable", 1 },
                    Package () { "supports-emmc", 1 },
                }
            })
            Name (_CRS, ResourceTemplate ()
            {
                Memory32Fixed (ReadWrite, 0xFE330000, 0x00010000)
                Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive, ,, ) { 43 }
            })
        }

        Device (I2C0)
        {
            Name (_HID, "RKCP0030")
            Name (_CID, Package () { "PRP0001" })
            Name (_UID, 0x00)
            // The Linux rk3x I2C driver is DT-oriented and currently crashes
            // when bound from this incomplete ACPI description.
            Method (_STA, 0, NotSerialized) { Return (0x00) }
            Name (_DSD, Package ()
            {
                ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
                Package ()
                {
                    Package () { "compatible", "rockchip,rk3399-i2c" },
                    Package () { "clock-frequency", 400000 },
                }
            })
            Name (_CRS, ResourceTemplate ()
            {
                Memory32Fixed (ReadWrite, 0xFF3C0000, 0x00001000)
                Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive, ,, ) { 89 }
            })

            Device (TCPC)
            {
                Name (_HID, "RKCP0040")
                Name (_CID, Package () { "PRP0001" })
                Name (_UID, 0x01)
                Method (_STA, 0, NotSerialized) { Return (0x00) }
                Name (_DSD, Package ()
                {
                    ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
                    Package () { Package () { "compatible", "fairchild,fusb302" } }
                })
                Name (_CRS, ResourceTemplate ()
                {
                    I2cSerialBusV2 (0x0022, ControllerInitiated, 400000,
                        AddressingMode7Bit, "\\_SB.I2C0",
                        0x00, ResourceConsumer,, Exclusive,)
                })
            }

            Device (PMIC)
            {
                Name (_HID, "RKCP0041")
                Name (_CID, Package () { "PRP0001" })
                Name (_UID, 0x00)
                Method (_STA, 0, NotSerialized) { Return (0x00) }
                Name (_DSD, Package ()
                {
                    ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
                    Package ()
                    {
                        Package () { "compatible", "rockchip,rk808" },
                        Package () { "rockchip,system-power-controller", 1 },
                        Package () { "wakeup-source", 1 },
                    }
                })
                Name (_CRS, ResourceTemplate ()
                {
                    I2cSerialBusV2 (0x001B, ControllerInitiated, 400000,
                        AddressingMode7Bit, "\\_SB.I2C0",
                        0x00, ResourceConsumer,, Exclusive,)
                })
            }
        }

        Device (I2C1)
        {
            Name (_HID, "RKCP0030")
            Name (_CID, Package () { "PRP0001" })
            Name (_UID, 0x01)
            Method (_STA, 0, NotSerialized) { Return (0x00) }
            Name (_DSD, Package ()
            {
                ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
                Package ()
                {
                    Package () { "compatible", "rockchip,rk3399-i2c" },
                    Package () { "clock-frequency", 400000 },
                }
            })
            Name (_CRS, ResourceTemplate ()
            {
                Memory32Fixed (ReadWrite, 0xFF110000, 0x00001000)
                Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive, ,, ) { 91 }
            })
        }

        Device (I2C4)
        {
            Name (_HID, "RKCP0030")
            Name (_CID, Package () { "PRP0001" })
            Name (_UID, 0x04)
            Method (_STA, 0, NotSerialized) { Return (0x00) }
            Name (_DSD, Package ()
            {
                ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
                Package ()
                {
                    Package () { "compatible", "rockchip,rk3399-i2c" },
                    Package () { "clock-frequency", 400000 },
                }
            })
            Name (_CRS, ResourceTemplate ()
            {
                Memory32Fixed (ReadWrite, 0xFF3D0000, 0x00001000)
                Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive, ,, ) { 88 }
            })

            Device (TSCR)
            {
                Name (_HID, "RKCP0042")
                Name (_CID, Package () { "PRP0001" })
                Name (_UID, 0x00)
                Method (_STA, 0, NotSerialized) { Return (0x00) }
                Name (_DSD, Package ()
                {
                    ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
                    Package ()
                    {
                        Package () { "compatible", "goodix,gt9xx" },
                        Package () { "max-x", 1200 },
                        Package () { "max-y", 1900 },
                        Package () { "tp-size", 911 },
                    }
                })
                Name (_CRS, ResourceTemplate ()
                {
                    I2cSerialBusV2 (0x0014, ControllerInitiated, 400000,
                        AddressingMode7Bit, "\\_SB.I2C4",
                        0x00, ResourceConsumer,, Exclusive,)
                })
            }
        }

        Device (I2C6)
        {
            Name (_HID, "RKCP0030")
            Name (_CID, Package () { "PRP0001" })
            Name (_UID, 0x06)
            Method (_STA, 0, NotSerialized) { Return (0x00) }
            Name (_DSD, Package ()
            {
                ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
                Package ()
                {
                    Package () { "compatible", "rockchip,rk3399-i2c" },
                    Package () { "clock-frequency", 400000 },
                }
            })
            Name (_CRS, ResourceTemplate ()
            {
                Memory32Fixed (ReadWrite, 0xFF150000, 0x00001000)
                Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive, ,, ) { 69 }
            })

            Device (TCP0)
            {
                Name (_HID, "RKCP0040")
                Name (_CID, Package () { "PRP0001" })
                Name (_UID, 0x00)
                Method (_STA, 0, NotSerialized) { Return (0x00) }
                Name (_DSD, Package ()
                {
                    ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
                    Package () { Package () { "compatible", "fairchild,fusb302" } }
                })
                Name (_CRS, ResourceTemplate ()
                {
                    I2cSerialBusV2 (0x0022, ControllerInitiated, 400000,
                        AddressingMode7Bit, "\\_SB.I2C6",
                        0x00, ResourceConsumer,, Exclusive,)
                })
            }
        }

        Device (XHC0)
        {
            Name (_HID, "PNP0D15")      // _HID: Hardware ID
            Name (_UID, 0x00)           // _UID: Unique ID
            Name (_CCA, 0x00)           // Not coherent!

            Name (_CRS, ResourceTemplate ()  // _CRS: Current Resource Settings
            {
                Memory32Fixed (ReadWrite,
                    0xfe800000,         // Address Base (MMIO)
                    0x00008000,         // Address Length
                    )
                Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive, ,, )
                {
                  137
                }
            })
        }

        Device (XHC1)
        {
            Name (_HID, "PNP0D15")      // _HID: Hardware ID
            Name (_UID, 0x01)           // _UID: Unique ID
            Name (_CCA, 0x00)           // Not coherent!

            Name (_CRS, ResourceTemplate ()  // _CRS: Current Resource Settings
            {
                Memory32Fixed (ReadWrite,
                    0xfe900000,         // Address Base (MMIO)
                    0x00008000,         // Address Length
                    )
                Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive, ,, )
                {
                  142
                }
            })
        }

        Device (EHC0)
        {
            Name (_HID, "PRP0001")
            Name (_UID, 0x00)
            Name (_CCA, 0x00)
            Method (_STA, 0, NotSerialized) { Return (0x0F) }
            Name (_DSD, Package ()
            {
                ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
                Package () { Package () { "compatible", "generic-ehci" } }
            })
            Name (_CRS, ResourceTemplate ()
            {
                Memory32Fixed (ReadWrite, 0xFE380000, 0x00020000)
                Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive, ,, ) { 58 }
            })
        }

        Device (OHC0)
        {
            Name (_HID, "PRP0001")
            Name (_UID, 0x00)
            Name (_CCA, 0x00)
            Method (_STA, 0, NotSerialized) { Return (0x0F) }
            Name (_DSD, Package ()
            {
                ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
                Package () { Package () { "compatible", "generic-ohci" } }
            })
            Name (_CRS, ResourceTemplate ()
            {
                Memory32Fixed (ReadWrite, 0xFE3A0000, 0x00020000)
                Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive, ,, ) { 60 }
            })
        }

        Device (EHC1)
        {
            Name (_HID, "PRP0001")
            Name (_UID, 0x01)
            Name (_CCA, 0x00)
            Method (_STA, 0, NotSerialized) { Return (0x0F) }
            Name (_DSD, Package ()
            {
                ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
                Package () { Package () { "compatible", "generic-ehci" } }
            })
            Name (_CRS, ResourceTemplate ()
            {
                Memory32Fixed (ReadWrite, 0xFE3C0000, 0x00020000)
                Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive, ,, ) { 62 }
            })
        }

        Device (OHC1)
        {
            Name (_HID, "PRP0001")
            Name (_UID, 0x01)
            Name (_CCA, 0x00)
            Method (_STA, 0, NotSerialized) { Return (0x0F) }
            Name (_DSD, Package ()
            {
                ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
                Package () { Package () { "compatible", "generic-ohci" } }
            })
            Name (_CRS, ResourceTemplate ()
            {
                Memory32Fixed (ReadWrite, 0xFE3E0000, 0x00020000)
                Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive, ,, ) { 64 }
            })
        }

        Device (COM0)
        {
            Name (_HID, "RKCP8250")
            Name (_CID, Package () { "HISI0031", "8250dw", "PRP0001" })
            Name (_UID, 0x00)
            Method (_STA, 0, NotSerialized) { Return (0x0F) }
            Name (_DSD, Package ()
            {
                ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
                Package ()
                {
                    Package () { "compatible", Package () { "rockchip,rk3399-uart", "snps,dw-apb-uart" } },
                    Package () { "reg-shift", 2 },
                    Package () { "reg-io-width", 4 },
                    Package () { "clock-frequency", 24000000 },
                }
            })
            Name (_CRS, ResourceTemplate ()
            {
                Memory32Fixed (ReadWrite, 0xFF180000, 0x00000100)
                Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive, ,, ) { 131 }
            })
        }

        Device (COM1)
        {
            Name (_HID, "RKCP8250")                             // _HID: Hardware ID
            Name (_CID, Package() {"HISI0031", "8250dw", "PRP0001"})       // _CID: Compatible ID
            Name (_UID, 0x02)
            Method (_STA, 0, NotSerialized) { Return (0x0F) }
            Name (_DSD, Package ()
            {
                ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
                Package ()
                {
                    Package () { "compatible", Package () { "rockchip,rk3399-uart", "snps,dw-apb-uart" } },
                    Package () { "reg-shift", 2 },
                    Package () { "reg-io-width", 4 },
                    Package () { "clock-frequency", 24000000 },
                }
            })
            Name (_CRS, ResourceTemplate ()                     // _CRS: Current Resource Settings
            {
                Memory32Fixed (ReadWrite,
                    FixedPcdGet64(PcdSerialRegisterBase),       // Address Base
                    0x00000100,                                 // Address Length
                    )
                Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive, ,, )
                {
                  132
                }
            })
        }

        Device (VOPL)
        {
            Name (_HID, "RKCP0050")
            Name (_CID, Package () { "PRP0001" })
            Name (_UID, 0x01)
            Name (_CCA, 0x00)
            // The upstream Rockchip DRM stack expects DT graph/clocks/resets.
            // Keep these nodes hidden until the ACPI display description is complete.
            Method (_STA, 0, NotSerialized) { Return (0x00) }
            Name (_DSD, Package ()
            {
                ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
                Package () { Package () { "compatible", "rockchip,rk3399-vop-lit" } }
            })
            Name (_CRS, ResourceTemplate ()
            {
                Memory32Fixed (ReadWrite, 0xFF8F0000, 0x00000600)
                Memory32Fixed (ReadWrite, 0xFF8F1C00, 0x00000200)
                Memory32Fixed (ReadWrite, 0xFF8F2000, 0x00000400)
                Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive, ,, ) { 151 }
            })
        }

        Device (VOPB)
        {
            Name (_HID, "RKCP0050")
            Name (_CID, Package () { "PRP0001" })
            Name (_UID, 0x00)
            Name (_CCA, 0x00)
            Method (_STA, 0, NotSerialized) { Return (0x00) }
            Name (_DSD, Package ()
            {
                ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
                Package () { Package () { "compatible", "rockchip,rk3399-vop-big" } }
            })
            Name (_CRS, ResourceTemplate ()
            {
                Memory32Fixed (ReadWrite, 0xFF900000, 0x00000600)
                Memory32Fixed (ReadWrite, 0xFF901C00, 0x00000200)
                Memory32Fixed (ReadWrite, 0xFF902000, 0x00001000)
                Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive, ,, ) { 150 }
            })
        }

        Device (DSI0)
        {
            Name (_HID, "RKCP0051")
            Name (_CID, Package () { "PRP0001" })
            Name (_UID, 0x00)
            Method (_STA, 0, NotSerialized) { Return (0x00) }
            Name (_DSD, Package ()
            {
                ToUUID ("daffd814-6eba-4d8c-8a91-bc9bbf4aa301"),
                Package ()
                {
                    Package () { "compatible", "rockchip,rk3399-mipi-dsi" },
                    Package () { "dsi,lanes", 4 },
                    Package () { "dsi,format", "rgb888" },
                    Package () { "panel-width", 1200 },
                    Package () { "panel-height", 1920 },
                    Package () { "clock-frequency", 160000000 },
                    Package () { "hback-porch", 21 },
                    Package () { "hfront-porch", 120 },
                    Package () { "vback-porch", 18 },
                    Package () { "vfront-porch", 21 },
                    Package () { "hsync-len", 20 },
                    Package () { "vsync-len", 3 },
                }
            })
            Name (_CRS, ResourceTemplate ()
            {
                Memory32Fixed (ReadWrite, 0xFF960000, 0x00008000)
                Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive, ,, ) { 77 }
            })
        }
    }
}
