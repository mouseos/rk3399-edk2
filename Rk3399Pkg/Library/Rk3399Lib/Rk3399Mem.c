/** @file
*
*  Copyright (c) 2014-2017, Linaro Limited. All rights reserved.
*  Copyright (c) 2017, Rockchip Inc. All rights reserved.
*
*  This program and the accompanying materials
*  are licensed and made available under the terms and conditions of the BSD License
*  which accompanies this distribution.  The full text of the license may be found at
*  http://opensource.org/licenses/bsd-license.php
*
*  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
*  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
*
**/

#include <Library/ArmPlatformLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/IoLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>

#include <Rk3399/Rk3399.h>
#include <Rk3399/Rk3399PmuGrf.h>
#include "Rk3399Mem.h"

// The total number of descriptors, including the final "end-of-table" descriptor.
#define MAX_VIRTUAL_MEMORY_MAP_DESCRIPTORS 12

// DDR attributes
#define DDR_ATTRIBUTES_CACHED           ARM_MEMORY_REGION_ATTRIBUTE_WRITE_BACK
#define DDR_ATTRIBUTES_UNCACHED         ARM_MEMORY_REGION_ATTRIBUTE_UNCACHED_UNBUFFERED

#define EXTRA_SYSTEM_MEMORY_BASE  0x40000000
#define RK3399_VENDOR_DRAM_HOLE_BASE  0x08400000
#define RK3399_VENDOR_DRAM_HOLE_SIZE  0x01E00000
#define RK3399_FRAMEBUFFER_RESERVED_INDEX  2

STATIC struct Rk3399ReservedMemory {
  EFI_PHYSICAL_ADDRESS         Offset;
  EFI_PHYSICAL_ADDRESS         Size;
} Rk3399ReservedMemoryBuffer [] = {
  { 0x00000000, 0x200000 },   // Reserved for ATF
  // Matches the vendor boot memory banks: 0x08400000-0x0A200000 is not RAM.
  { RK3399_VENDOR_DRAM_HOLE_BASE, RK3399_VENDOR_DRAM_HOLE_SIZE },
  { 0x00000000, 0x000000 }    // Filled with the active framebuffer range
};

STATIC
VOID
BuildMemoryRegionHobs (
  IN EFI_PHYSICAL_ADDRESS        RegionBase,
  IN UINT64                      RegionSize,
  IN EFI_RESOURCE_ATTRIBUTE_TYPE ResourceAttributes
  )
{
  EFI_PHYSICAL_ADDRESS RegionEnd;
  EFI_PHYSICAL_ADDRESS CurrentBase;
  EFI_PHYSICAL_ADDRESS ReservedBase;
  EFI_PHYSICAL_ADDRESS ReservedEnd;
  EFI_PHYSICAL_ADDRESS OverlapBase;
  EFI_PHYSICAL_ADDRESS OverlapEnd;
  UINTN                Index;
  UINTN                Count;

  if (RegionSize == 0) {
    return;
  }

  RegionEnd = RegionBase + RegionSize;
  CurrentBase = RegionBase;
  Count = sizeof (Rk3399ReservedMemoryBuffer) / sizeof (struct Rk3399ReservedMemory);

  for (Index = 0; Index < Count; Index++) {
    ReservedBase = Rk3399ReservedMemoryBuffer[Index].Offset;
    ReservedEnd = ReservedBase + Rk3399ReservedMemoryBuffer[Index].Size;

    if ((ReservedEnd <= CurrentBase) || (ReservedBase >= RegionEnd)) {
      continue;
    }

    OverlapBase = (ReservedBase > CurrentBase) ? ReservedBase : CurrentBase;
    OverlapEnd = (ReservedEnd < RegionEnd) ? ReservedEnd : RegionEnd;

    if (CurrentBase < OverlapBase) {
      BuildResourceDescriptorHob (
        EFI_RESOURCE_SYSTEM_MEMORY,
        ResourceAttributes,
        CurrentBase,
        OverlapBase - CurrentBase);
    }

    BuildResourceDescriptorHob (
      EFI_RESOURCE_MEMORY_RESERVED,
      EFI_RESOURCE_ATTRIBUTE_PRESENT,
      OverlapBase,
      OverlapEnd - OverlapBase);

    CurrentBase = OverlapEnd;
  }

  if (CurrentBase < RegionEnd) {
    BuildResourceDescriptorHob (
      EFI_RESOURCE_SYSTEM_MEMORY,
      ResourceAttributes,
      CurrentBase,
      RegionEnd - CurrentBase);
  }
}

STATIC
UINT64
EFIAPI
Rk3399InitMemorySize (
  IN VOID
  )
{
  UINT32 Rank, Col, Bank, Cs0Row, Cs1Row, Bw, Row34;
  UINT32 ChipSizeMb = 0;
  UINT64 SizeMb = 0;
  UINT32 Ch;
  UINT64 ret;

  UINT32 SysReg = MmioRead32(RK3399_PMU_GRF_BASE + PMU_GRF_OS_REG2);
  UINT32 ChNum = 1 + ((SysReg >> SYS_REG_NUM_CH_SHIFT) &
                      SYS_REG_NUM_CH_MASK);

  for (Ch = 0; Ch < ChNum; Ch++) {
    Rank = 1 + (SysReg >> SYS_REG_RANK_SHIFT(Ch) &
                SYS_REG_RANK_MASK);
    Col = 9 + (SysReg >> SYS_REG_COL_SHIFT(Ch) & SYS_REG_COL_MASK);
    Bank = 3 - ((SysReg >> SYS_REG_BK_SHIFT(Ch)) & SYS_REG_BK_MASK);
    Cs0Row = 13 + (SysReg >> SYS_REG_CS0_ROW_SHIFT(Ch) &
                   SYS_REG_CS0_ROW_MASK);
    Cs1Row = 13 + (SysReg >> SYS_REG_CS1_ROW_SHIFT(Ch) &
                   SYS_REG_CS1_ROW_MASK);
    Bw = (2 >> ((SysReg >> SYS_REG_BW_SHIFT(Ch)) &
                SYS_REG_BW_MASK));
    Row34 = SysReg >> SYS_REG_ROW_3_4_SHIFT(Ch) &
            SYS_REG_ROW_3_4_MASK;

    ChipSizeMb = (1 << (Cs0Row + Col + Bank + Bw - 20));

    if (Rank > 1)
      ChipSizeMb += ChipSizeMb >> (Cs0Row - Cs1Row);
    if (Row34)
      ChipSizeMb = ChipSizeMb * 3 / 4;
    SizeMb += ChipSizeMb;

    DEBUG((DEBUG_INFO, "Rank %d Col %d Bank %d Cs0Row %d Bw %d Row34 %d\n",
          Rank, Col, Bank, Cs0Row, Bw, Row34));
  }

  /*
   * Support maximum DDR capacity is 4GB size, less the MMIO hole
   * at 0xf8000000, where the SoC registers are.
   */
  ret = SizeMb << 20;

  if (ret >= RK3399_PERIPH_BASE) {
    ret = RK3399_PERIPH_BASE;
  }

  DEBUG((DEBUG_INFO, "memory size=%dMB 0x%x\n", SizeMb, ret));

  return ret;
}

/**
  Return the Virtual Memory Map of your platform

  This Virtual Memory Map is used by MemoryInitPei Module to initialize the MMU on your platform.

  @param[out]   VirtualMemoryMap    Array of ARM_MEMORY_REGION_DESCRIPTOR describing a Physical-to-
                                    Virtual Memory mapping. This array must be ended by a zero-filled
                                    entry

**/
VOID
ArmPlatformGetVirtualMemoryMap (
  IN ARM_MEMORY_REGION_DESCRIPTOR** VirtualMemoryMap
  )
{
  ARM_MEMORY_REGION_ATTRIBUTES  CacheAttributes;
  UINTN                         Index = 0;
  ARM_MEMORY_REGION_DESCRIPTOR  *VirtualMemoryTable;
  EFI_RESOURCE_ATTRIBUTE_TYPE   ResourceAttributes;
  UINT64                        MemorySize, AdditionalMemorySize;
  EFI_PHYSICAL_ADDRESS          FrameBufferBase;
  UINT64                        FrameBufferSize;

  MemorySize = Rk3399InitMemorySize ();
  if (MemorySize == 0) {
    MemorySize = PcdGet64 (PcdSystemMemorySize);
  }

  FrameBufferBase = FixedPcdGet32 (PcdMipiFrameBufferAddress);
  FrameBufferSize = (UINT64)FixedPcdGet32 (PcdMipiFrameBufferWidth) *
                    FixedPcdGet32 (PcdMipiFrameBufferHeight) * 2;
  if (FrameBufferSize != 0) {
    Rk3399ReservedMemoryBuffer[RK3399_FRAMEBUFFER_RESERVED_INDEX].Offset = FrameBufferBase;
    Rk3399ReservedMemoryBuffer[RK3399_FRAMEBUFFER_RESERVED_INDEX].Size =
      EFI_PAGES_TO_SIZE (EFI_SIZE_TO_PAGES (FrameBufferSize));
  }

  ResourceAttributes = (
    EFI_RESOURCE_ATTRIBUTE_PRESENT |
    EFI_RESOURCE_ATTRIBUTE_INITIALIZED |
    EFI_RESOURCE_ATTRIBUTE_WRITE_COMBINEABLE |
    EFI_RESOURCE_ATTRIBUTE_WRITE_THROUGH_CACHEABLE |
    EFI_RESOURCE_ATTRIBUTE_WRITE_BACK_CACHEABLE |
    EFI_RESOURCE_ATTRIBUTE_TESTED
  );

  // Create memory HOBs, excluding display framebuffer from normal RAM so
  // Linux cannot allocate over the active scanout buffer during early boot.
  BuildMemoryRegionHobs (
    PcdGet64 (PcdSystemMemoryBase),
    PcdGet64 (PcdSystemMemorySize),
    ResourceAttributes);

  AdditionalMemorySize = MemorySize - PcdGet64 (PcdSystemMemorySize);
  if (AdditionalMemorySize > 0) {
    // Declared the additional memory
    ResourceAttributes =
      EFI_RESOURCE_ATTRIBUTE_PRESENT |
      EFI_RESOURCE_ATTRIBUTE_INITIALIZED |
      EFI_RESOURCE_ATTRIBUTE_WRITE_COMBINEABLE |
      EFI_RESOURCE_ATTRIBUTE_WRITE_THROUGH_CACHEABLE |
      EFI_RESOURCE_ATTRIBUTE_WRITE_BACK_CACHEABLE |
      EFI_RESOURCE_ATTRIBUTE_TESTED;

    BuildMemoryRegionHobs (
      EXTRA_SYSTEM_MEMORY_BASE,
      AdditionalMemorySize,
      ResourceAttributes);
  }

  ASSERT (VirtualMemoryMap != NULL);

  VirtualMemoryTable = (ARM_MEMORY_REGION_DESCRIPTOR*)AllocatePages(EFI_SIZE_TO_PAGES (sizeof(ARM_MEMORY_REGION_DESCRIPTOR) * MAX_VIRTUAL_MEMORY_MAP_DESCRIPTORS));
  if (VirtualMemoryTable == NULL) {
    return;
  }

  CacheAttributes = DDR_ATTRIBUTES_CACHED;

  Index = 0;

  // RK3399 SOC peripherals
  VirtualMemoryTable[Index].PhysicalBase    = RK3399_PERIPH_BASE;
  VirtualMemoryTable[Index].VirtualBase     = RK3399_PERIPH_BASE;
  VirtualMemoryTable[Index].Length          = RK3399_PERIPH_SZ;
  VirtualMemoryTable[Index].Attributes      = ARM_MEMORY_REGION_ATTRIBUTE_DEVICE;

  // DDR - predefined 1GB size
  VirtualMemoryTable[++Index].PhysicalBase  = PcdGet64 (PcdSystemMemoryBase);
  VirtualMemoryTable[Index].VirtualBase     = PcdGet64 (PcdSystemMemoryBase);
  VirtualMemoryTable[Index].Length          = PcdGet64 (PcdSystemMemorySize);
  VirtualMemoryTable[Index].Attributes      = CacheAttributes;

  // If DDR capacity is over 1GB.
  if (AdditionalMemorySize > 0) {
    VirtualMemoryTable[++Index].PhysicalBase = EXTRA_SYSTEM_MEMORY_BASE;
    VirtualMemoryTable[Index].VirtualBase    = EXTRA_SYSTEM_MEMORY_BASE;
    VirtualMemoryTable[Index].Length         = AdditionalMemorySize;
    VirtualMemoryTable[Index].Attributes     = CacheAttributes;
  }

  // End of Table
  VirtualMemoryTable[++Index].PhysicalBase  = 0;
  VirtualMemoryTable[Index].VirtualBase     = 0;
  VirtualMemoryTable[Index].Length          = 0;
  VirtualMemoryTable[Index].Attributes      = (ARM_MEMORY_REGION_ATTRIBUTES)0;

  ASSERT((Index + 1) <= MAX_VIRTUAL_MEMORY_MAP_DESCRIPTORS);

  *VirtualMemoryMap = VirtualMemoryTable;
}
