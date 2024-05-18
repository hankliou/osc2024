#include "mmu.h"

// when called, x0 may be DTB_ptr, restore it after func done
void *set_2M_kernel_mmu(void *x0) {
    // Turn
    //   Two-level Translation (1GB) - in boot.S
    // to
    //   Three-level Translation (2MB) - set PUD point to new table

    // PGD -> PUD -> PTE
    unsigned long *pud_table = (unsigned long *)MMU_PGD_ADDR;
    unsigned long *pte_table1 = (unsigned long *)MMU_PTE_ADDR;
    unsigned long *pte_table2 = (unsigned long *)(MMU_PTE_ADDR + 0x1000L);

    for (int i = 0; i < 512; i++) {
        unsigned long addr = 0x200000L * i; // 0x20_0000 = 2MB
        if (addr >= PERIPHERAL_END) {
            pte_table1[i] = (0x0 + addr) + BOOT_PTE_ATTR_nGnRnE;
            continue;
        }
        pte_table1[i] = (0x00000000 + addr) | BOOT_PTE_ATTR_NOCACHE; // 0*2MB
        pte_table2[i] = (0x40000000 + addr) | BOOT_PTE_ATTR_NOCACHE; // 512*2MB
    }

    // set PUD
    pud_table[0] = (unsigned long)pte_table1 | BOOT_PUD_ATTR;
    pud_table[1] = (unsigned long)pte_table2 | BOOT_PUD_ATTR;

    return x0;
}