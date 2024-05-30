#include "mmu.h"
#include "bcm2837/rpi_mmu.h"
#include "memory.h"
#include "mmu_constant.h"
#include "stddef.h"
#include "thread.h"
#include "u_string.h"
#include "uart1.h"

// when called, x0 may be DTB_ptr, restore it after func done
void *set_2M_kernel_mmu(void *x0) {
    // Turn
    //   Two-level Translation (1GB) - in boot.S
    // to
    //   Three-level Translation (2MB) - set PUD point to new table

    // PGD -> PUD -> PTE
    unsigned long *pud_table = (unsigned long *)MMU_PUD_ADDR;
    unsigned long *pte_table1 = (unsigned long *)MMU_PTE_ADDR;
    unsigned long *pte_table2 = (unsigned long *)(MMU_PTE_ADDR + 0x1000L);

    for (int i = 0; i < 512; i++) {
        unsigned long offset = 0x200000L * i; // 0x20_0000 = 2MB

        // if addr exceed peripheral end, it belongs to device memory
        if (offset >= PERIPHERAL_END) {
            pte_table1[i] = (0x0 + offset) + BOOT_PTE_ATTR_nGnRnE;
            continue;
        }
        pte_table1[i] = (0x00000000 + offset) | BOOT_PTE_ATTR_NOCACHE; // 0*2MB
        pte_table2[i] = (0x40000000 + offset) | BOOT_PTE_ATTR_NOCACHE; // 512*2MB
    }

    // setup PUD entry(1GB), make it point to PTE entry(2MB)
    pud_table[0] = (unsigned long)pte_table1 | BOOT_PUD_ATTR;
    pud_table[1] = (unsigned long)pte_table2 | BOOT_PUD_ATTR;

    return x0;
}

void map_page(size_t *user_pgd_ptr, size_t va, size_t pa, size_t flag) {
    size_t *table = user_pgd_ptr;
    for (int level = 0; level < 4; level++) {
        // get the address chunk
        unsigned int idx = (va >> (39 - 9 * level)) & 0x1ff;
        // pte will return physical addr
        if (level == 3) {
            table[idx] = pa;
            table[idx] |= PD_ACCESS | PD_TABLE | (MAIR_IDX_NORMAL_NOCACHE << 2) | PD_PXN | flag; // el0 only
            return;
        }

        // table doesn't exist -> malloc new one
        if (!table[idx]) {
            size_t *new_table = kmalloc(0x1000);                                 // create table, kmalloc will return virt addr
            memset(new_table, 0, 0x1000);                                        // clear bits
            table[idx] = VIRT2PHYS((size_t)new_table);                           // pointer to next table
            table[idx] |= PD_ACCESS | PD_TABLE | (MAIR_IDX_NORMAL_NOCACHE << 2); // flags, bits[4:2] is the index to MAIR
        }

        /*
            Entry of PGD, PUD, PMD which point to a page table
            +-----+------------------------------+---------+--+
            |     | next level table's phys addr | ignored |11|
            +-----+------------------------------+---------+--+
                47                             12         2  0

            Entry of PUD, PMD which point to a block
            +-----+------------------------------+---------+--+
            |     |  block's physical address    |attribute|01|
            +-----+------------------------------+---------+--+
                47                              n         2  0

            Entry of PTE which points to a page
            +-----+------------------------------+---------+--+
            |     |  page's physical address     |attribute|11|
            +-----+------------------------------+---------+--+
                47                             12         2  0

            Invalid entry
            +-----+------------------------------+---------+--+
            |     |  page's physical address     |attribute|*0|
            +-----+------------------------------+---------+--+
                47                             12         2  0
        */
        // bits[47:n] is the physical address the entry point to.
        // By above table, flags bits will not affect 'table' to iter to next level and looking up
        table = (size_t *)PHYS2VIRT((size_t)(table[idx] & ENTRY_ADDR_MASK)); // iter to next table
    }
}

void mmu_add_vma(thread *t, size_t va, size_t size, size_t pa, size_t xwr, int is_allocated) {
    // aligment
    size = size % 0x1000 ? size + (0x1000 - (size % 0x1000)) : size;
    // init node
    vm_area_struct *mem = kmalloc(sizeof(vm_area_struct));
    mem->xwr = xwr;
    mem->phys_addr = pa;
    mem->virt_addr = va;
    mem->area_size = size;
    mem->is_allocated = is_allocated;

    // insert into vma list
    mem->next = &(t->vma_list);
    mem->prev = t->vma_list.prev;
    mem->next->prev = mem;
    mem->prev->next = mem;
    uart_sendline("mmu add vma paddr: %x\n", mem->phys_addr);
}

// remove thread's private vma list
void mmu_del_vma(thread *t) {
    vm_area_struct *it = t->vma_list.next;
    while (it != &t->vma_list) {
        if (it->is_allocated) kfree((void *)PHYS2VIRT(it->phys_addr));
        it = it->next;   // iter first
        kfree(it->prev); // del current node
    }
    // diff with sample, re-init list
    t->vma_list.next = &t->vma_list;
    t->vma_list.prev = &t->vma_list;
}

// recursively remove entry in thread's private pgd
void mmu_free_page_tables(size_t *page_table, int level) {
    size_t *table_virt = (size_t *)PHYS2VIRT((char *)page_table);

    // traverse all entry in table
    for (int i = 0; i < 512; i++) {
        if (table_virt[i] != 0) {                                             // if entry points to table or page
            size_t *next_table = (size_t *)(table_virt[i] & ENTRY_ADDR_MASK); // addr points to next level table
            if (table_virt[i] & PD_TABLE) {                                   // if current entry points to a table, not a phys addr

                if (level < 2)                                   // if is upper table of PMD
                    mmu_free_page_tables(next_table, level + 1); // free it !
                table_virt[i] = 0L;                              // reset val
                kfree((void *)PHYS2VIRT((char *)next_table));
            }
        }
    }
}

void mmu_memfail_abort_handler(esr_el1 *esr) {
    // get fault address
    // far_el1 store faulting 'virtual addr', data abort, PC alignment fault, watch point exception taken to EL1
    unsigned long long far_el1; // fault address register
    asm volatile("mrs %0, far_el1" : "=r"(far_el1));
    // uart_sendline("far_el1: %x\n", far_el1);

    // search virt addr in va_list
    thread *cur_thread = get_current();
    vm_area_struct *memory = 0;
    for (vm_area_struct *it = cur_thread->vma_list.next; it != &cur_thread->vma_list; it = it->next) {
        // far_el1 is inside any node in vma_list
        if (it->virt_addr <= far_el1 && far_el1 <= it->virt_addr + it->area_size) {
            memory = it;
            break;
        }
    }

    // not found in vma_list
    if (!memory) {
        uart_sendline("[Segmentation Fault] killing process\n");
        thread_exit();
        return;
    }

    // for translation fault(check last 6 bits), map 1 frame to the addr
    if ((esr->iss & 0x3f) == TRANS_FAULT_LEVEL0 || // lv0
        (esr->iss & 0x3f) == TRANS_FAULT_LEVEL1 || // lv1
        (esr->iss & 0x3f) == TRANS_FAULT_LEVEL2 || // lv2
        (esr->iss & 0x3f) == TRANS_FAULT_LEVEL3) { // lv3

        uart_sendline("[Translation Fault] 0x%x\n", far_el1);

        // get offset and align to 0x1000 (phys addr usage)
        size_t offset = far_el1 - (memory->virt_addr);
        if (offset % 0x1000) offset -= (offset % 0x1000);

        // set the privilege
        size_t xwr_flag = 0;
        if ((memory->xwr & 0b100) == 0) xwr_flag |= PD_UXN;       // non executable
        if ((memory->xwr & 0b010) == 0) xwr_flag |= PD_RDONLY;    // non writeable
        if ((memory->xwr & 0x001) == 1) xwr_flag |= PD_UK_ACCESS; // non redable

        map_page(PHYS2VIRT(cur_thread->context.pgd), memory->virt_addr + offset, memory->phys_addr + offset, xwr_flag);
    } else {
        uart_sendline("[Segmentation Fault] killing process (else)\n");
        thread_exit();
    }
    return;
}