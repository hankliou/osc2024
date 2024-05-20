#include "syscall.h"
#include "bcm2837/rpi_mbox.h"
#include "cpio.h"
#include "exception.h"
#include "mbox.h"
#include "memory.h"
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
    while (i < size)
        buf[i++] = uart_getc();
    // buf[i++] = uart_async_getc();
    tpf->x0 = i;
    return i;
}

// user have to allocate spaces to x0(buf) themself
size_t uart_write(trap_frame *tpf, const char buf[], size_t size) {
    int i = 0;
    while (i < size)
        uart_send(buf[i++]);
    // uart_async_putc(buf[i++]);
    tpf->x0 = i;
    return i;
}

// Note: In this lab, you won’t have to deal with argument passing, but you can still use it.
int exec(trap_frame *tpf, const char *name, char *const argv[]) {
    thread *cur_thread = get_current();
    cur_thread->codesize = get_file_size((char *)name);
    memcpy(cur_thread->code, get_file_start((char *)name), cur_thread->codesize);

    // init signal handler
    for (int i = 0; i <= SIGNAL_MAX; i++)
        cur_thread->signal_handler[i] = signal_default_handler;

    tpf->elr_el1 = (unsigned long)cur_thread->code;
    tpf->sp_el0 = (unsigned long)cur_thread->user_stack_ptr + USTACK_SIZE;
    tpf->x0 = 0;
    return 0;
}

// return child's id to parent, return 0 to child
int fork(trap_frame *tpf) {
    lock();

    thread *child = thread_create(get_current()->code);                     // create new thread
    thread *parent = get_current();                                         // backup parent thread
    int parent_id = parent->pid;                                            // record original pid
    child->codesize = parent->codesize;                                     // assign size
    memcpy(child->user_stack_ptr, parent->user_stack_ptr, USTACK_SIZE);     // copy user stack (deep copy)
    memcpy(child->kernel_stack_ptr, parent->kernel_stack_ptr, KSTACK_SIZE); // copy kernel stack (deep copy)

    // copy signal handler
    for (int i = 0; i <= SIGNAL_MAX; i++)
        child->signal_handler[i] = parent->signal_handler[i];

    // before coping context, update parent's context first!!!
    store_context((thread_context *)get_current()); // mainly storing lr and sp

    // child
    if (get_current()->pid != parent_id) {
        // child move it's "tpf" pointer to it's trap_frame
        tpf = (trap_frame *)((char *)tpf + (child->kernel_stack_ptr - parent->kernel_stack_ptr)); // trap frame in kernel stack
        // child move it's "user sp" with same offset of it's parent
        tpf->sp_el0 += child->user_stack_ptr - parent->user_stack_ptr;
        tpf->x0 = 0;
        return 0; // jump to link register
    }

    // copy parent's context to child
    child->context = get_current()->context;
    // update an offset to child's sp, so after jump with "lr", "sp" will be correct
    unsigned long long offset = (unsigned long long)(child->kernel_stack_ptr) - (unsigned long long)(parent->kernel_stack_ptr);
    child->context.fp += offset;
    child->context.sp += offset;

    tpf->x0 = child->pid; // return child's pid
    unlock();
    return child->pid; // return child's pid
}

void exit(trap_frame *tpf, int status) {
    thread_exit();
}

int mbox_call(trap_frame *tpf, unsigned char ch, unsigned int *mbox) {
    lock();
    unsigned int r = ((unsigned long)mbox & ~0xF) | (ch & 0xF);
    // Wait until we can write to the mailbox
    while (*MBOX_STATUS & BCM_ARM_VC_MS_FULL)
        ;
    *MBOX_WRITE = r; // Write the request
    while (1) {
        // Wait for the response
        while (*MBOX_STATUS & BCM_ARM_VC_MS_EMPTY)
            ;
        if (r == *MBOX_READ) {
            tpf->x0 = (mbox[1] == MBOX_REQUEST_SUCCEED);
            unlock();
            return mbox[1] == MBOX_REQUEST_SUCCEED;
        }
    }
    tpf->x0 = 0;
    unlock();
    return 0;
}

void kill(int pid) {
    if (pid < 0 || pid > PID_MAX || !thread_list[pid].isused)
        return;
    lock();
    thread_list[pid].iszombie = 1;
    unlock();
    schedule();
}

/* signal related functions */
void signal_register(int signal, void (*handler)()) {
    if (signal < 0 || signal > SIGNAL_MAX)
        return;
    get_current()->signal_handler[signal] = handler;
}

// trigger (call) signal handler
void signal_kill(int pid, int signal) {
    if (pid < 0 || pid > PID_MAX || !thread_list[pid].isused)
        return;
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
        if (strcmp(filepath, path) == 0)
            return filesize;

        // if TRAILER!!!
        if (head == 0)
            uart_sendline("execfile: %s: No such file or directory\n", path);
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
        if (strcmp(filepath, path) == 0)
            return filedata;

        // if TRAILER!!!
        if (head == 0)
            uart_sendline("execfile: %s: No such file or directory\n", path);
    }
    return 0;
}