#include "memory.h"
#include "dtb.h"
#include "uart1.h"

extern char _heap_top;
static char *htop_ptr = &_heap_top; // declare it "static" so it'll always
                                    // remember the last position

void *simple_malloc(unsigned int size) {
    // -> heap top ptr
    // heap top ptr + 0x02: heap_block size
    // heap top ptr + 0x10 ~ heap top ptr + 0x10 * k:
    //              { heap_block }
    // -> heap top ptr

    // header
    char *r = htop_ptr + 0x10;

    // size padding align to heap_block header
    size = 0x10 + size - size % 0x10;
    // recode the size of space allocated at 0x8 byte before content(heap_block)
    *(unsigned int *)(r - 0x8) = size;
    // mov heap top
    htop_ptr += size;

    return r;
}

void free(void *ptr) {
}

/* lab 4 */
extern char _start; // defined in linker
extern char _end;
extern char *CPIO_DEFAULT_START; // define in dtb.h
extern char *CPIO_DEFAULT_END;

static frame *frame_array;     // store all "frame" status
static frame *free_frame_list; // store continuous free frame info
static frame_slot *free_slot_list;

void insert_frame_node(frame *node, frame *it) {
    node->next = it->next;
    node->prev = it;
    node->prev->next = node;
    node->next->prev = node;
}

// Should be called after CPIO being traverse
void allocator_init() {
    // init frame array
    frame_array = (frame *)simple_malloc(MAX_PAGE * sizeof(frame));
    for (int i = 0; i < MAX_PAGE; i++) {
        frame_array[i].next = &frame_array[i];
        frame_array[i].prev = &frame_array[i];
        frame_array[i].val = 0;
        frame_array[i].idx = i;
        frame_array[i].used = FRAME_FREE_FLAG;
        frame_array[i].slot_level = NOT_A_SLOT;
    }

    // init frame freelist, heads are empty
    free_frame_list = (frame *)simple_malloc((MAX_PAGE_EXP + 1) * sizeof(frame));
    for (int i = 0; i < MAX_PAGE_EXP + 1; i++) {
        free_frame_list[i].idx = -1; // debug usage
        free_frame_list[i].next = &free_frame_list[i];
        free_frame_list[i].prev = &free_frame_list[i];
    }
    // insert every biggest chunk of mem into list
    for (int i = 0; i < MAX_PAGE; i += (1 << MAX_PAGE_EXP)) {
        frame_array[i].val = MAX_PAGE_EXP;
        insert_frame_node(&frame_array[i], &free_frame_list[MAX_PAGE_EXP]);
    }

    // init dynamic mem allocator head (head is empty)
    free_slot_list = simple_malloc((MAX_SLOT_EXP + 1) * sizeof(frame_slot));
    for (int i = 0; i < MAX_SLOT_EXP + 1; i++) {
        free_slot_list[i].next = &free_slot_list[i];
        free_slot_list[i].prev = &free_slot_list[i];
    }

    // dump_free_frame_list();

    // start up allocation
    // uart_sendline("Buddy system usable memory from 0x%x to 0x%x\n", ALLOCATION_BASE, ALLOCATION_END);
    // uart_sendline("Page size: %d Byte, total available frame: %d\n", PAGE_SIZE, MAX_PAGE);
    // uart_sendline("[Start up allocation]\n");
    // uart_sendline("\nSpin tables & DTB:");
    // dtb_get_reserved_memory(); // spin tables and dtb itself
    // uart_sendline("\n_start ~ _end:");
    // memory_reserve((unsigned long long)&_start, (unsigned long long)&_end); // kernel image (stack, heap included)
    // uart_sendline("\nCPIO_DEFAULT_START ~ CPIO_DEFAULT_END:");
    // memory_reserve((unsigned long long)CPIO_DEFAULT_START, (unsigned long long)CPIO_DEFAULT_END); // initramfs
    // dump_free_frame_list();
}

// size: Byte
void *page_malloc(unsigned int size) {
    // calculate least frame size needed
    int needed_frame = (size >> PAGE_LEVEL) + ((size & (0xFFF)) > 0);
    // uart_sendline("Request size: %d B (%d frames)\n", size, needed_frame);

    size = needed_frame; // turn size into frame (4KB = 2^12 bytes)
    void *allocated_addr = (void *)0;

    // traverse and find available continuous frame
    for (int i = 0;; i++) {
        // failed
        if (i > MAX_PAGE_EXP) {
            uart_sendline("page malloc failed, no space left\n");
            break;
        }

        // find smallest block to store
        if ((1 << i) >= size && free_frame_list[i].next != &free_frame_list[i]) {
            // calculate address by frame index
            frame *node = free_frame_list[i].next;
            allocated_addr = (void *)(((uint64_t)(node->idx) << PAGE_LEVEL) + ALLOCATION_BASE);
            // void *end = (void *)(((uint64_t)(node->idx + (1 << i)) << (PAGE_LEVEL)) + ALLOCATION_BASE);

            // uart_sendline("[allocte] From index %d, allocated %d frame with total size "
            //               "%d KB\n",
            //               node->idx, (1 << i), (1 << (i + PAGE_LEVEL - 10)));

            // update frame array head (set val to 2^i continuous frame)
            node->val = i;
            node->used = FRAME_OCCUPIED_FLAG;

            // remove node from freelist
            node->prev->next = node->next;
            node->next->prev = node->prev;
            node->next = node;
            node->prev = node;

            // release redundant frame
            release_redundant(node, size);

            // uart_sendline("allocated address from %x ~ %x\n", allocated_addr, end);
            break;
        }
    }
    // dump_free_frame_list();
    return allocated_addr;
}

// if allocated frames / 2 > needed frame, take it back
void release_redundant(frame *node, unsigned int size) {
    // loop until < size
    while (node->val > 0 && size <= (1 << (node->val - 1))) {
        // cut half
        node->val -= 1;

        // get the half been cut
        frame *buddy = &frame_array[node->idx ^ (1 << node->val)];
        buddy->val = node->val; // mark level to coalesce

        // add buddy to free frame list
        insert_frame_node(buddy, &free_frame_list[node->val]);

        // uart_sendline("[release] From index %d, released %d frame with total size %d "
        //               "KB\n",
        //               buddy->idx, (1 << node->val), (1 << (node->val + PAGE_LEVEL - 10)));
    }
}

// val = frame's level (2^val)
void page_free(void *ptr) {
    // get the target in array: (addr - base) / 8(byte) / 4096(frame size)
    frame *page_frame_ptr = &frame_array[((unsigned long long)ptr - ALLOCATION_BASE) >> PAGE_LEVEL];

    // uart_sendline("[Free] From index %d, released %d frame with total size %d KB\n", page_frame_ptr->idx, (1 << page_frame_ptr->val),
    //               (1 << (page_frame_ptr->val + PAGE_LEVEL - 10)));

    // update tag to 'un-used'
    page_frame_ptr->used = FRAME_FREE_FLAG;

    // coalesce iteratively
    while (coalesce(&page_frame_ptr) == 0) {};

    // add whole coalesced frames into free frame list
    insert_frame_node(page_frame_ptr, &free_frame_list[page_frame_ptr->val]);

    // dump_free_frame_list();
}

// return 0 if success, return -1 otherwise
int coalesce(frame **ptr) {
    frame *frame_ptr = *ptr;
    // find buddy's index
    frame *buddy = &frame_array[frame_ptr->idx ^ (1 << frame_ptr->val)];

    // frame must in same level (varify if buddy is free)
    if (frame_ptr->val != buddy->val)
        return -1;

    // buddy is in used
    if (buddy->used != FRAME_FREE_FLAG)
        return -1;

    // uart_sendline("[Coalesce] Merge frame %d and frame %d\n", frame_ptr->idx, buddy->idx);

    // remove buddy from freelist
    buddy->prev->next = buddy->next;
    buddy->next->prev = buddy->prev;

    // ptr point to head of merged frames
    if (buddy->idx < frame_ptr->idx)
        *ptr = buddy;

    // since merge, upgrade level
    (*ptr)->val++;

    return 0;
}

void dump_free_frame_list() {
    uart_sendline("\n===========================================\n");
    uart_sendline("[free frame list]\n");
    for (int i = 0; i <= MAX_PAGE_EXP; i++) {
        frame *it = free_frame_list[i].next;
        int len = 0;
        while (it != &free_frame_list[i]) {
            // uart_sendline("Frame sequence index %d, sized %d KB\n", it->idx, (1 << (i + 2))); // ((1<<i) * 4)KB
            len++;
            it = it->next;
        }
        uart_sendline("Frame size %d KB:\t%d left\n", (1 << (i + PAGE_LEVEL - 10)), len);
    }
    uart_sendline("===========================================\n\n");
}

// get a page from buddy system
void cut_page_to_slot(int expo) {
    // (char*) to index it Byte by Byte
    char *addr = page_malloc(PAGE_SIZE);
    frame *page = &frame_array[((unsigned long long)addr - ALLOCATION_BASE) >> PAGE_LEVEL];
    page->slot_level = expo;

    // insert_frame_node(page, &dynamic_mem_allocator_head[expo]);
    int slot_size = SLOT_SIZE << expo;
    for (int i = 0; i < PAGE_SIZE; i += slot_size) {
        // temporary use the space to store next, prev ptr
        // it will be overwrite after allocated
        frame_slot *slot = (frame_slot *)(addr + i);
        slot->next = free_slot_list[expo].next;
        slot->prev = &free_slot_list[expo];
        slot->next->prev = slot;
        slot->prev->next = slot;
    }
    // dump_free_slot_list();
}

// size: byte
void *dynamic_malloc(unsigned int size) {
    // calculate least frame size needed
    int slot_needed = (size >> SLOT_LEVEL) + ((size & (0x1F)) > 0);
    // uart_sendline("Request size: %d Byte (%d slots) < 4KB\n", size, slot_needed);
    size = slot_needed;

    // find the level needed
    int expo = 0;
    while ((1 << expo) < size)
        expo++;

    // if no free slot
    if (free_slot_list[expo].next == &free_slot_list[expo]) {
        // uart_sendline("[Dynamic malloc] no free slot\n");
        cut_page_to_slot(expo);
    }

    void *slot_addr = free_slot_list[expo].next; // allocated space
    // remove slot from free_slot_list
    frame_slot *it = free_slot_list[expo].next;
    it->prev->next = it->next;
    it->next->prev = it->prev;

    // uart_sendline("[Dynamic malloc] start from %x, sized %d Byte\n", slot_addr, (1 << expo));
    // dump_free_slot_list();

    return slot_addr;
}

void dynamic_free(void *ptr) {
    // get the list to insert
    int expo = frame_array[((unsigned long long)ptr - ALLOCATION_BASE) >> 12].slot_level;
    // type trans to re-get 'next', 'prev' field
    frame_slot *slot = (frame_slot *)ptr;

    // uart_sendline("[Dynamic free] start from %x, sized %d Byte\n", ptr, (1 << expo) * SLOT_SIZE);

    // insert
    slot->next = free_slot_list[expo].next;
    slot->prev = &free_slot_list[expo];
    slot->next->prev = slot;
    slot->prev->next = slot;

    // dump_free_slot_list();
}

void dump_free_slot_list() {
    uart_sendline("\n===========================================\n");
    uart_sendline("[free slot list]\n");
    for (int i = 0; i <= MAX_SLOT_EXP; i++) {
        frame_slot *it = free_slot_list[i].next;
        int len = 0;
        while (it != &free_slot_list[i]) {
            len++;
            it = it->next;
        }
        uart_sendline("Slot size %d Byte:\t%d left\n", (SLOT_SIZE << i), len);
    }
    uart_sendline("===========================================\n\n");
}

void *kmalloc(unsigned int size) {
    void *addr;
    if (size > PAGE_SIZE / 2)
        addr = page_malloc(size);
    else
        addr = dynamic_malloc(size);
    return addr;
}

void kfree(void *ptr) {
    // if ptr's space in frame array has 'slot_level' attribute, use dynamic free
    frame *the_frame = &frame_array[((unsigned long long)ptr - ALLOCATION_BASE) >> 12];
    if (the_frame->slot_level == NOT_A_SLOT)
        page_free(ptr);
    else
        dynamic_free(ptr);
}

// start, end: address (e.g. 0x1234 5678), a frame contains 0x8000 bits !!!
void memory_reserve(unsigned long long start, unsigned long long end) {
    // turn start, end into frame idx
    // uart_sendline("\nReserve Memory from 0x%8x to 0x%8x", start, end);
    start = ((start - ALLOCATION_BASE) >> PAGE_LEVEL);
    end = (end % (1 << PAGE_LEVEL)) == 0 ? ((end - ALLOCATION_BASE) >> PAGE_LEVEL) : ((end - ALLOCATION_BASE) >> PAGE_LEVEL) + 1;
    // uart_sendline(" (frame %d ~ %d)\n", start, end);

    for (int expo = MAX_PAGE_EXP; expo >= 0; expo--) {

        for (frame *it = free_frame_list[expo].next; it != &free_frame_list[expo];) {

            int frame_start = it->idx, frame_end = it->idx + (1 << it->val); // get the range of frames
            // reserve
            if (start <= frame_start && frame_end <= end) {
                // uart_sendline("[Reserve] 0x%8x ~ 0x%8x\n", ((it->idx << PAGE_LEVEL) + ALLOCATION_BASE),
                //               (((it->idx + (1 << it->val)) << PAGE_LEVEL) + ALLOCATION_BASE));
                it->used = FRAME_OCCUPIED_FLAG;
                it->prev->next = it->next;
                it->next->prev = it->prev;
                it = it->next;
            }
            // free
            else if (frame_start >= end || frame_end <= start) {
                it = it->next;
            }
            // remove it, cut half, go next iteration
            else {
                it->prev->next = it->next; // remove
                it->next->prev = it->prev;

                frame *buddy = &frame_array[it->idx ^ (1 << (expo - 1))]; // get its buddy (another half)
                it->val--;
                buddy->val = it->val;

                frame *tmp = it->next; // backup it.next

                insert_frame_node(buddy, &free_frame_list[expo - 1]); // add to lower level list
                insert_frame_node(it, &free_frame_list[expo - 1]);

                it = tmp; // restore
            }
        }
    }
}