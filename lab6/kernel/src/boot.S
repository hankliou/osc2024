#include "mmu.h"

.section ".text.boot"

.global _start

_start:
    bl from_el2_to_el1
    // b setup_exception_vector_table // debug usage

setup_mmu_config:
    // translation control register (TCR), paging is configured by TCR
    // set paging configuration (up : 0xffff000000000000 low : 0x0000000000000000)
    ldr     x4, = TCR_CONFIG_DEFAULT
    msr     tcr_el1, x4

    // Memory Attribute Indirection Register (MAIR)
    ldr     x4, =((MAIR_DEVICE_nGnRnE << (MAIR_IDX_DEVICE_nGnRnE * 8)) | (MAIR_NORMAL_NOCACHE << (MAIR_IDX_NORMAL_NOCACHE * 8)))
    msr     mair_el1, x4

    // Identity Paging
    // 2-level translation, PGD & PUD only. Each entry of PUD points to a 1GB block.
    // TODO: different with spec !!!!!
    ldr     x4, = MMU_PGD_ADDR // PGD's page frame at 0x1000
    ldr     x1, = MMU_PUD_ADDR // PUD's page frame at 0x2000

    ldr     x2, = BOOT_PGD_ATTR
    orr     x2, x1, x2              // combine the physical address of next level page with attribute.
    str     x2, [x4]                // [MMU_PGD_ADDR] = MMU_PUD_ADDR | BOOT_PGD_ATTR

    ldr     x2, = BOOT_PUD_ATTR
    // The first page of PUD maps 0x00000000 - 0x3fffffff (RAM and GPU peripherals)
    mov     x3, 0x00000000          
    orr     x3, x2, x3              // [MMU_PUD_ADDR] = 0x00000000 | BOOT_PUD_ATTR
    str     x3, [x1]                // 1st 1GB mapped by the 1st entry of PUD
    // The second page of PUD maps 0x40000000 - 0x7fffffff (ARM local peripherals).
    mov     x3, 0x40000000
    orr     x3, x2, x3              // [MMU_PUD_ADDR + 8] = 0x40000000 | BOOT_PUD_ATTR
    str     x3, [x1, 8]             // 2nd 1GB mapped by the 2nd entry of PUD

    // Map the Kernel Space
    // load the identity paging's PGD to 'ttbr1_el1', use indirect branch to virtual address, then run kernel in upper address space.
    msr     ttbr0_el1, x4           // load PGD to the bottom translation-based register.
    msr     ttbr1_el1, x4           // also load PGD to the upper translation based register.

    mov     sp, 0x3c000000          // temp stack
    bl      set_2M_kernel_mmu
    
    mrs     x2, sctlr_el1           // sctlr_el1: Provides top level control of the system, including its memory system, at EL1 and EL0.
    orr     x2, x2, 1               // sctlr_el1[0]: EL1&0 stage 1 address translation enabled/disabled.
    msr     sctlr_el1, x2           // enable MMU, cache remains disabled

    // indirect branch to the virtual address
    ldr     x2, = setup_exception_vector_table         
    br      x2


setup_exception_vector_table:
    adr     x1, exception_vector_table
    msr     vbar_el1, x1

setup_stack:
    ldr     x1, =_stack_top
    mov     sp, x1

setup_bss:
    ldr     x1, =_bss_top
    ldr     w2, =_bss_size

init_bss:
    cbz     w2, run_main
    str     xzr, [x1], #8
    sub     w2, w2, #1
    cbnz    w2, init_bss

run_main:
    bl      main

proc_hang:
    wfe
    b       proc_hang

from_el2_to_el1:
    mov     x1, (1 << 31)           // EL1 uses aarch64
    msr     hcr_el2, x1
    mov     x1, 0x345               // EL1h (SPSel = 1) with interrupt disabled (EAIF)
    msr     spsr_el2, x1
    msr     elr_el2, lr
    eret                            // return to EL1