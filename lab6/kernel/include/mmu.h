#ifndef _MMU_H_
#define _MMU_H_

#include "stddef.h"

#define TRANS_FAULT_LEVEL0 0b000100 // esr_el1, iss IFSC, bits[5:0]
#define TRANS_FAULT_LEVEL1 0b000101
#define TRANS_FAULT_LEVEL2 0b000110
#define TRANS_FAULT_LEVEL3 0b000111

#define MEMFAIL_DATA_ABORT_LOWER 0b100100 // esr_el1, EC[31:26]
#define MEMFAIL_INST_ABORT_LOWER 0b100000 // (in lower EL)

typedef struct esr_el1 {
    size_t        // 32 bit field
        iss : 25, // Instruction specific syndrome
        il : 1,   // Instruction length bit
        ec : 6;   // Exception class
} esr_el1;

typedef struct vm_area_struct {
    struct vm_area_struct *next;
    struct vm_area_struct *prev;
    unsigned long virt_addr;
    unsigned long phys_addr;
    unsigned long area_size;
    unsigned long xwr; // 1, 2, 4
    int is_allocated;
} vm_area_struct;

void *set_2M_kernel_mmu(void *x0);
void map_page(size_t *virt_pgd_p, size_t va, size_t pa, size_t flag);

typedef struct thread thread;
void mmu_add_vma(thread *t, size_t va, size_t size, size_t pa, size_t xwr, int is_allocated);
void mmu_del_vma(thread *t);
void mmu_map_pages(size_t *pgd_ptr, size_t va, size_t size, size_t pa, size_t flag);
void mmu_free_page_tables(size_t *page_table, int level);

void mmu_memfail_abort_handler(esr_el1 *esr);

#endif /* _MMU_H_ */