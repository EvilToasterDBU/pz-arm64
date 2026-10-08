/* stubs.S - 4096 tiny entry points (8 bytes each) + the common trampoline.
 * A stub loads its index into w16 and branches to pzs_common, which saves the AAPCS64 argument registers
 * (x0-x7, d0-d7) and a pointer to the caller's stack arguments, then calls pzs_dispatch(idx, regs).
 * Registered via RegisterNatives, one stub per native method, so no machine code is generated at run time. */
    .text
    .balign 8
    .global pzs_stubs
    .hidden pzs_stubs
    .type pzs_stubs, %object
pzs_stubs:
    .set i, 0
    .rept 4096
    movz w16, #i
    b pzs_common
    .set i, i+1
    .endr

    .balign 16
    .global pzs_common
    .hidden pzs_common
    .type pzs_common, %function
pzs_common:
    sub  sp, sp, #176
    stp  x29, x30, [sp, #160]
    stp  x0,  x1,  [sp, #0]
    stp  x2,  x3,  [sp, #16]
    stp  x4,  x5,  [sp, #32]
    stp  x6,  x7,  [sp, #48]
    stp  d0,  d1,  [sp, #64]
    stp  d2,  d3,  [sp, #80]
    stp  d4,  d5,  [sp, #96]
    stp  d6,  d7,  [sp, #112]
    add  x9, sp, #176          /* caller's stack-passed arguments start at the old sp */
    str  x9, [sp, #144]
    mov  w0, w16
    mov  x1, sp
    bl   pzs_dispatch
    ldr  x0, [sp, #128]        /* integer/pointer result */
    ldr  d0, [sp, #136]        /* float/double result */
    ldp  x29, x30, [sp, #160]
    add  sp, sp, #176
    ret
    .size pzs_common, .-pzs_common
    .section .note.GNU-stack,"",%progbits
