#
#  Copyright (c) 2014-2017, Linaro Limited. All rights reserved.
#  Copyright (c) 2017, Rockchip Inc. All rights reserved.
#
#  This program and the accompanying materials
#  are licensed and made available under the terms and conditions of the BSD License
#  which accompanies this distribution.  The full text of the license may be found at
#  http://opensource.org/licenses/bsd-license.php
#
#  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
#  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
#
#

#include <AsmMacroLib.h>
#include <Library/ArmLib.h>

ASM_FUNC(ArmPlatformPeiBootAction)
  // Early UART marker before stack/C runtime is available. This distinguishes
  // "SEC entry reached" from "firmware never entered".
  MOV64 (x0, 0xFF77E028)
  MOV32 (w1, 0x03C00140)
  str   w1, [x0]

  MOV64 (x0, 0xFF1A0000)
  MOV32 (w2, 0x10000)
1:
  ldr   w1, [x0, #0x14]
  tbnz  w1, #5, 2f
  subs  w2, w2, #1
  b.ne  1b
2:
  mov   w1, #'S'
  strb  w1, [x0]
  mov   w1, #'\r'
  strb  w1, [x0]
  mov   w1, #'\n'
  strb  w1, [x0]
  ret

//UINTN
//ArmPlatformGetCorePosition (
//  IN UINTN MpId
//  );
// With this function: CorePos = (ClusterId * 4) + CoreId
ASM_FUNC(ArmPlatformGetCorePosition)
  and   x1, x0, #ARM_CORE_MASK
  and   x0, x0, #ARM_CLUSTER_MASK
  add   x0, x1, x0, LSR #6
  ret

//UINTN
//ArmPlatformGetPrimaryCoreMpId (
//  VOID
//  );
ASM_FUNC(ArmPlatformGetPrimaryCoreMpId)
  MOV32 (w0, FixedPcdGet32(PcdArmPrimaryCore))
  ret

//UINTN
//ArmPlatformIsPrimaryCore (
//  IN UINTN MpId
//  );
ASM_FUNC(ArmPlatformIsPrimaryCore)
  MOV32 (w1, FixedPcdGet32(PcdArmPrimaryCoreMask))
  and   x0, x0, x1
  MOV32 (w1, FixedPcdGet32(PcdArmPrimaryCore))
  cmp   w0, w1
  cset  x0, eq
  ret
