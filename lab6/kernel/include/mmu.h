#ifndef _MMU_H_
#define _MMU_H_

#include "stddef.h"

typedef struct vm_area_struct {
    struct vm_area_struct *next;
    struct vm_area_struct *prev;
    unsigned long virt_addr;
    unsigned long phys_addr;
    unsigned long area_size;
    unsigned long rwx; // 1, 2, 4
    int is_allocated;
} vm_area_struct;

void *set_2M_kernel_mmu(void *x0);
void map_page(size_t *virt_pgd_p, size_t va, size_t pa, size_t flag);

typedef struct thread thread;
void mmu_add_vma(thread *t, size_t va, size_t size, size_t pa, size_t rwx, int is_allocated);
void mmu_del_vma(thread *t);
void mmu_map_pages(size_t *pgd_ptr, size_t va, size_t size, size_t pa, size_t flag);
void mmu_free_page_tables(size_t *page_table, int level);

#endif /* _MMU_H_ */