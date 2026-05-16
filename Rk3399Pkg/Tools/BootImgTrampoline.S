/*
 * RK3399 Android boot.img trampoline for EDK2 secondary boot.
 *
 * Rockchip U-Boot loads the boot.img kernel payload at kernel_addr_r
 * (0x00280000 on this platform), while the RK3399 EDK2 FD is linked to run
 * from PcdFdBaseAddress (0x00200000). This trampoline is the boot.img kernel
 * payload: it copies the appended FD to its linked address and jumps there.
 */

#ifndef EDK2_FD_LOAD_ADDR
#error "EDK2_FD_LOAD_ADDR must be defined"
#endif

#ifndef EDK2_FD_SIZE
#error "EDK2_FD_SIZE must be defined"
#endif

#ifndef PAYLOAD_OFFSET
#error "PAYLOAD_OFFSET must be defined"
#endif

#ifndef TRAMPOLINE_RELOC_ADDR
#error "TRAMPOLINE_RELOC_ADDR must be defined"
#endif

#define CACHELINE_SIZE 64

  .section .text, "ax"
  .balign 16
  .global _start

  .macro mov64, reg, value
    movz  \reg, #((\value) & 0xffff)
    movk  \reg, #(((\value) >> 16) & 0xffff), lsl #16
    movk  \reg, #(((\value) >> 32) & 0xffff), lsl #32
    movk  \reg, #(((\value) >> 48) & 0xffff), lsl #48
  .endm

_start:
  /*
   * The FD destination range overlaps this trampoline's load address, so copy
   * the real copy/jump routine to scratch RAM before overwriting 0x00280000.
   */
  mov64 x0, TRAMPOLINE_RELOC_ADDR
  adr   x1, Lreloc_start
  adr   x2, Lreloc_end
  sub   x2, x2, x1

  mov   x3, x0
  mov   x4, x1
  mov   x5, x2

Lcopy_relocator:
  cbz   x5, Lrelocator_copied
  ldr   x6, [x4], #8
  str   x6, [x3], #8
  sub   x5, x5, #8
  b     Lcopy_relocator

Lrelocator_copied:
  /* Make the copied routine visible to instruction fetch. */
  mov   x3, x0
  mov   x4, x2
  bl    Lclean_invalidate_range

  mov64 x0, EDK2_FD_LOAD_ADDR
  adr   x1, _start
  mov64 x2, PAYLOAD_OFFSET
  add   x1, x1, x2
  mov64 x2, EDK2_FD_SIZE
  mov64 x3, EDK2_FD_LOAD_ADDR
  mov64 x4, TRAMPOLINE_RELOC_ADDR
  br    x4

/*
 * Input:
 *   x0 = destination address for FD
 *   x1 = source address of appended FD
 *   x2 = FD size
 *   x3 = EDK2 entry address
 */
  .balign 16
Lreloc_start:
  mov   x9, x0
  mov   x10, x2
  cbz   x10, Lcopy_done

Lcopy_fd_16:
  cmp   x10, #16
  b.lo  Lcopy_fd_tail
  ldp   x11, x12, [x1], #16
  stp   x11, x12, [x0], #16
  sub   x10, x10, #16
  b     Lcopy_fd_16

Lcopy_fd_tail:
  cbz   x10, Lcopy_done
  ldrb  w11, [x1], #1
  strb  w11, [x0], #1
  sub   x10, x10, #1
  b     Lcopy_fd_tail

Lcopy_done:
  mov   x0, x9
  mov   x1, x2
  bl    Lreloc_clean_invalidate_range

  mov   x0, xzr
  mov   x1, xzr
  mov   x2, xzr
  mov   x4, x3
  br    x4

Lreloc_clean_invalidate_range:
  add   x2, x0, x1
  bic   x5, x0, #(CACHELINE_SIZE - 1)
  mov   x6, x5
1:
  cmp   x5, x2
  b.hs  2f
  dc    cvau, x5
  add   x5, x5, #CACHELINE_SIZE
  b     1b
2:
  dsb   ish
  mov   x5, x6
3:
  cmp   x5, x2
  b.hs  4f
  ic    ivau, x5
  add   x5, x5, #CACHELINE_SIZE
  b     3b
4:
  dsb   ish
  isb
  ret

  .balign 16
Lreloc_end:

Lclean_invalidate_range:
  add   x2, x3, x4
  bic   x5, x3, #(CACHELINE_SIZE - 1)
  mov   x6, x5
1:
  cmp   x5, x2
  b.hs  2f
  dc    cvau, x5
  add   x5, x5, #CACHELINE_SIZE
  b     1b
2:
  dsb   ish
  mov   x5, x6
3:
  cmp   x5, x2
  b.hs  4f
  ic    ivau, x5
  add   x5, x5, #CACHELINE_SIZE
  b     3b
4:
  dsb   ish
  isb
  ret
