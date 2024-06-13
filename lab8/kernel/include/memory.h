#ifndef _MEMORY_H_
#define _MEMORY_H_

#include "bcm2837/rpi_mmu.h"

/* lab 2 */
void *simple_malloc(unsigned int size);
void free(void *ptr);

/* lab 4 */
void *kmalloc(unsigned int size);
void kfree(void *ptr);

// page related
#define ALLOCATION_BASE (PHYS2VIRT(0x0)) // 0x1000 0000 ~ 0x2000 0000 (byte)
#define ALLOCATION_END  (PHYS2VIRT(0x3C000000))
// #define ALLOCATION_BASE 0x10000000 // 0x1000 0000 ~ 0x2000 0000 (byte)
// #define ALLOCATION_END 0x20000000
#define PAGE_SIZE    0x1000 // 4KB = 0x1000 byte
#define PAGE_LEVEL   12     // 4KB = (1 << 12)
#define MAX_PAGE     ((ALLOCATION_END - ALLOCATION_BASE) / PAGE_SIZE)
#define MAX_PAGE_EXP 10 // max 'val' field in frame (1024 continuous frames, 4096 KB)

#define FRAME_FREE_FLAG     -1
#define FRAME_OCCUPIED_FLAG -2
#define NOT_A_SLOT          -1

typedef struct frame {
    struct frame *next;
    struct frame *prev; // to del node in O(1)
    unsigned int idx;   // frame index
    int used;           // bool flag
    int val;            // page level
    int slot_level;     // slot level
} frame;

void insert_frame_node(frame *node, frame *it);
void allocator_init();

void *page_malloc(unsigned int size);
void release_redundant(frame *node, unsigned int size);
void page_free(void *ptr);
int coalesce(frame **ptr);
void dump_free_frame_list();

// dynamic memory allocator related
#define SLOT_SIZE    32 // Byte
#define SLOT_LEVEL   5  // log(DYNAMIC_ALLOCATOR_BLOCK_SIZE)
#define MAX_SLOT_EXP 7  // log(pagesize(32B) / blocksize(4KB)) = 7

typedef struct frame_slot {
    struct frame_slot *next;
    struct frame_slot *prev;
} frame_slot;

void *dynamic_malloc(unsigned int size);
void cut_page_to_slot(int expo);
void dynamic_free(void *ptr);
void dump_free_slot_list();

// Reserved Memory
void memory_reserve(unsigned long long start, unsigned long long end);

#endif /* _MEMORY_H_ */