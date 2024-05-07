#include "syscall.h"
#include "bcm2837/rpi_mbox.h"
#include "cpio.h"
#include "exception.h"
#include "mbox.h"
#include "thread.h"
#include "u_string.h"
#include "uart1.h"

extern thread *cur_thread;
extern thread *run_queue;
extern thread thread_list[MAXPID + 1];

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
    tpf->x0 = cur_thread->pid;
    return cur_thread->pid;
}

// user have to allocate spaces to x0(buf) themself
size_t uart_read(trap_frame *tpf, char buf[], size_t size) {
    int i = 0;
    while (i < size)
        buf[i++] = uart_async_getc();
    tpf->x0 = i;
    return i;
}

// user have to allocate spaces to x0(buf) themself
size_t uart_write(trap_frame *tpf, const char buf[], size_t size) {
    int i = 0;
    while (i < size)
        uart_async_putc(buf[i++]);
    tpf->x0 = i;
    return i;
}

// Note: In this lab, you won’t have to deal with argument passing, but you can still use it.
int exec(trap_frame *tpf, const char *name, char *const argv[]) {
    cur_thread->codesize = get_file_size((char *)name);
    memcpy(cur_thread->code, get_file_start((char *)name), cur_thread->codesize);
    tpf->elr_el1 = (unsigned long)cur_thread->code;
    tpf->sp_el0 = (unsigned long)cur_thread->user_stack_ptr + USTACK_SIZE;
    tpf->x0 = 0;
    return 0;
}

// return child's id to parent, return 0 to child
int fork(trap_frame *tpf) {
    lock();

    thread *child = thread_create(cur_thread->code);                        // create new thread
    thread *parent = cur_thread;                                            // backup parent thread
    int parent_id = cur_thread->pid;                                        // record original pid
    child->codesize = cur_thread->codesize;                                 // assign size
    memcpy(child->kernel_stack_ptr, parent->kernel_stack_ptr, KSTACK_SIZE); // copy kernel stack (deep copy)
    memcpy(child->user_stack_ptr, parent->user_stack_ptr, USTACK_SIZE);     // copy user stack (deep copy)

    // before coping context, update parent's context first!!!
    store_context(get_current());

    // child
    if (cur_thread->pid != parent_id) {
        // TODO: explain why (parent's tpf + offset) = child's tpf
        // because the copied trap_frame still point to parent's trap_frame, so need to give it an offset
        unsigned long long offset = (unsigned long long)(child->kernel_stack_ptr) - (unsigned long long)(parent->kernel_stack_ptr);
        tpf = (trap_frame *)((char *)tpf + offset);
        tpf->sp_el0 += offset;
        tpf->x0 = 0;
        unlock();
        return 0; // jump to link register
    }
    // copy parent's context
    child->context = cur_thread->context;
    // fix stacks's offset
    // when create new thread, it will get a private/kernel stack space
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

    unsigned int r = (*mbox & ~(0xF)) | (*mbox | ch);
    while ((*MBOX_STATUS & BCM_ARM_VC_MS_FULL) != 0) {}
    *MBOX_WRITE = r;

    while (1) {
        while (*MBOX_STATUS & BCM_ARM_VC_MS_EMPTY) {}
        if (*MBOX_READ == r) {
            tpf->x0 = (mbox[1] == MBOX_REQUEST_SUCCEED);
            unlock();
            return tpf->x0;
        }
    }
    tpf->x0 = 0;
    unlock();
    return 0;
}

void kill(trap_frame *tpf, int pid) {
    if (pid < 0 || pid > MAXPID || !thread_list[pid].isused)
        return;
    lock();
    thread_list[pid].iszombie = 1;
    unlock();
    schedule();
}

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