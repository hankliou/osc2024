#include "syscall.h"
#include "bcm2837/rpi_mbox.h"
#include "bcm2837/rpi_mmu.h"
#include "cpio.h"
#include "exception.h"
#include "mbox.h"
#include "memory.h"
#include "mmu_constant.h"
#include "signal.h"
#include "thread.h"
#include "u_string.h"
#include "uart1.h"

extern thread *run_queue;
extern thread thread_list[PID_MAX + 1];

extern void *CPIO_DEFAULT_START;

/*
    When it comes to system calls, the user software expects the kernel to take care of it.
    The program uses the general-purpose registers to set the arguments and receive the return value, just like conventional function calls.
    The kernel can then read the trap frame to acquire the user’s parameters and write it to set the return value and error code.

    !!!!!!!!!!!!!!!!
    When calling the svc function:
        The arguments would be stored in x0, x1, x2, …
        Return value would be stored in x0
        The system call numbers given would be stored in x8
    !!!!!!!!!!!!!!!!
*/
int getpid(trap_frame *tpf) {
    tpf->x0 = get_current()->pid;
    return get_current()->pid;
}

// user have to allocate spaces to x0(buf) themself
size_t uart_read(trap_frame *tpf, char buf[], size_t size) {
    int i = 0;
    while (i < size) buf[i++] = uart_getc();
    // buf[i++] = uart_async_getc();
    tpf->x0 = i;
    return i;
}

// user have to allocate spaces to x0(buf) themself
size_t uart_write(trap_frame *tpf, const char buf[], size_t size) {
    int i = 0;
    while (i < size) uart_send(buf[i++]);
    // uart_async_putc(buf[i++]);
    tpf->x0 = i;
    return i;
}

// Note: In this lab, you won’t have to deal with argument passing, but you can still use it.
int exec(trap_frame *tpf, const char *name, char *const argv[]) {
    thread *cur_thread = get_current();

    // init virtual memory list
    mmu_del_vma(cur_thread); // empty whole vma_list
    cur_thread->vma_list.next = &(cur_thread->vma_list);
    cur_thread->vma_list.prev = &(cur_thread->vma_list);

    cur_thread->codesize = get_file_size((char *)name);
    char *new_data = get_file_start((char *)name);
    cur_thread->code = kmalloc(cur_thread->codesize);
    cur_thread->user_stack_ptr = kmalloc(USTACK_SIZE);

    asm("dsb ish\n\t"); // ensure write has completed
    mmu_free_page_tables(cur_thread->context.pgd, 0);
    memset(PHYS2VIRT(cur_thread->context.pgd), 0, 0x1000);
    asm("tlbi vmalle1is\n\t" // invalidate all TLB entries
        "dsb ish\n\t"        // ensure completion of TLB invalidatation
        "isb\n\t");          // clear pipeline

    mmu_add_vma(cur_thread, USER_KERNEL_BASE, cur_thread->codesize, (size_t)VIRT2PHYS(cur_thread->code), 0b111, 1);
    mmu_add_vma(cur_thread, USER_STACK_TOP - USTACK_SIZE, USTACK_SIZE, (size_t)VIRT2PHYS(cur_thread->user_stack_ptr), 0b111, 1);
    mmu_add_vma(cur_thread, PERIPHERAL_START, PERIPHERAL_END - PERIPHERAL_START, PERIPHERAL_START, 0b011, 0);
    mmu_add_vma(cur_thread, USER_SIGNAL_WRAPPER_VA, 0x2000, (size_t)VIRT2PHYS(signal_handler_wrapper), 0b101, 0);

    memcpy(cur_thread->code, new_data, cur_thread->codesize);
    for (int i = 0; i <= SIGNAL_MAX; i++) { cur_thread->signal_handler[i] = signal_default_handler; }

    tpf->elr_el1 = USER_KERNEL_BASE;
    tpf->sp_el0 = USER_STACK_TOP;
    tpf->x0 = 0;
    return 0;
}

// return child's id to parent, return 0 to child
int fork(trap_frame *tpf) {
    lock();
    thread *child = thread_create(get_current()->code, get_current()->codesize);

    // copy signal handler
    for (int i = 0; i <= SIGNAL_MAX; i++) { child->signal_handler[i] = get_current()->signal_handler[i]; }

    for (vm_area_struct *vma = &get_current()->vma_list; vma->next != &get_current()->vma_list; vma = vma->next) {
        // ignore device and signal wrapper
        if (vma->virt_addr == USER_SIGNAL_WRAPPER_VA || vma->virt_addr == PERIPHERAL_START) { continue; }
        char *new_alloc = kmalloc(vma->area_size); // alloc a new memory to map VA
        mmu_add_vma(child, vma->virt_addr, vma->area_size, (size_t)VIRT2PHYS(new_alloc), vma->xwr, 1);
        memcpy(new_alloc, (void *)PHYS2VIRT(vma->phys_addr), vma->area_size);
    }
    mmu_add_vma(child, PERIPHERAL_START, PERIPHERAL_END - PERIPHERAL_START, PERIPHERAL_START, 0b011, 0);
    mmu_add_vma(child, USER_SIGNAL_WRAPPER_VA, 0x2000, (size_t)VIRT2PHYS(signal_handler_wrapper), 0b101, 0);

    int parent_pid = get_current()->pid;

    // copy stack into new process
    for (int i = 0; i < KSTACK_SIZE; i++) { child->kernel_stack_ptr[i] = get_current()->kernel_stack_ptr[i]; }

    store_context((thread_context *)get_current());
    // for child
    if (parent_pid != get_current()->pid) { goto child; }

    void *temp_pgd = child->context.pgd;
    child->context = get_current()->context;
    child->context.pgd = VIRT2PHYS(temp_pgd);
    child->context.fp += child->kernel_stack_ptr - get_current()->kernel_stack_ptr; // move fp
    child->context.sp += child->kernel_stack_ptr - get_current()->kernel_stack_ptr; // move kernel sp

    unlock();
    uart_sendline("fork finish\n");
    tpf->x0 = child->pid;
    return child->pid;

child:
    tpf->x0 = 0;
    return 0;
}

void exit(trap_frame *tpf, int status) { thread_exit(); }

int syscall_mbox_call(trap_frame *tpf, unsigned char ch, unsigned int *mbox_user) {
    lock();

    unsigned int size_of_mbox = mbox_user[0];
    memcpy((char *)pt, mbox_user, size_of_mbox);
    mbox_call(MBOX_TAGS_ARM_TO_VC, (unsigned int)((unsigned long)&pt));
    memcpy(mbox_user, (char *)pt, size_of_mbox);

    tpf->x0 = 8;
    unlock();
    return 0;
}

void kill(int pid) {
    lock();
    if (pid < 0 || pid > PID_MAX || !thread_list[pid].isused) {
        unlock();
        return;
    }
    thread_list[pid].iszombie = 1;
    unlock();
    schedule();
}

/* signal related functions */
void signal_register(int signal, void (*handler)()) {
    if (signal < 0 || signal > SIGNAL_MAX) return;
    get_current()->signal_handler[signal] = handler;
}

// trigger (call) signal handler
void signal_kill(int pid, int signal) {
    if (pid < 0 || pid > PID_MAX || !thread_list[pid].isused) return;
    lock();
    thread_list[pid].sigcount[signal]++;
    unlock();
}

void signal_return(trap_frame *tpf) {
    // free space (there may be some sys call in user-defined handler, when handling, tpf will be update, hance we can get it's sp from tpf)
    unsigned long long signal_user_stack = tpf->sp_el0 % USTACK_SIZE == 0 ? tpf->sp_el0 - USTACK_SIZE : tpf->sp_el0 & (~(USTACK_SIZE - 1));
    kfree((char *)signal_user_stack);

    // load context
    load_context(&get_current()->signal_savedContext);
}

/* components */
unsigned int get_file_size(char *path) {
    char *filepath, *filedata;
    unsigned int filesize;
    struct cpio_newc_header *head = CPIO_DEFAULT_START;

    // traverse the whole ramdisk, check filename one by one
    while (head != 0) {
        int error = cpio_newc_parse_header(head, &filepath, &filesize, &filedata, &head);
        if (error) {
            uart_sendline("cpio parse error\n");
            return -1;
        }
        if (strcmp(filepath, path) == 0) return filesize;

        // if TRAILER!!!
        if (head == 0) uart_sendline("execfile: %s: No such file or directory\n", path);
    }
    return 0;
}

char *get_file_start(char *path) {
    char *filepath, *filedata;
    unsigned int filesize;
    struct cpio_newc_header *head = CPIO_DEFAULT_START;

    // traverse the whole ramdisk, check filename one by one
    while (head != 0) {
        int error = cpio_newc_parse_header(head, &filepath, &filesize, &filedata, &head);
        if (error) {
            uart_sendline("cpio parse error\n");
            break;
        }
        if (strcmp(filepath, path) == 0) return filedata;

        // if TRAILER!!!
        if (head == 0) uart_sendline("execfile: %s: No such file or directory\n", path);
    }
    return 0;
}