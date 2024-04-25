#include "syscall.h"
#include "exception.h"
#include "mbox.h"
#include "thread.h"
#include "u_string.h"
#include "uart1.h"

/*
    When it comes to system calls, the user software expects the kernel to take care of it.
    The program uses the general-purpose registers to set the arguments and receive the return value, just like conventional function calls.
    The kernel can then read the trap frame to acquire the user’s parameters and write it to set the return value and error code.
*/

extern thread *cur_thread;
extern thread thread_list[MAXPID + 1];

int getpid() {
    return cur_thread->pid;
}

size_t uart_read(char buf[], size_t size) {
    int i = 0;
    while (i < size)
        buf[i++] = uart_async_getc();
    return i;
}

size_t uart_write(const char buf[], size_t size) {
    int i = 0;
    while (i < size)
        uart_async_putc(buf[i++]);
    return i;
}

// Note: In this lab, you won’t have to deal with argument passing, but you can still use it.
int exec(const char *name, char *const argv[]) {
}

// return child's id to parent, return 0 to child
int fork() {
    lock();
    thread *child = thread_create(cur_thread->code);                              // create new thread
    int parent_id = cur_thread->pid;                                              // record original pid
    child->codesize = cur_thread->codesize;                                       // assign size
    memcpy(child->private_stack_ptr, cur_thread->private_stack_ptr, USTACK_SIZE); // copy user stack (deep copy)
    memcpy(child->kernel_stack_ptr, cur_thread->kernel_stack_ptr, KSTACK_SIZE);   // copy kernel stack

    // store parent's context

    // child
    if (cur_thread->pid != parent_id) {
        return 0;
    }

    // parent
    child->context = cur_thread->context;
    // fix stacks's offset
    // when create new thread, it will get a private/kernel stack space
    unsigned long long offset = (unsigned long long)(child->kernel_stack_ptr) - (unsigned long long)(cur_thread->kernel_stack_ptr);
    child->context.fp = child->context.fp + offset;
    child->context.sp = child->context.sp + offset;

    unlock();
    return child->pid; // return child's pid
}

void exit() {
    thread_exit();
}

int mbox_call(unsigned char ch, unsigned int *mbox) {
    lock();

    unlock();
}

void kill(int pid) {
    if (pid < 0 || pid > MAXPID || !thread_list[pid].isused)
        return;
    lock();
    thread_list[pid].iszombie = 1;
    unlock();
    schedule();
}