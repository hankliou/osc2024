#include "thread.h"
#include "../include/memory.h"

static int pid_cnt = 0;
thread *run_queue;
thread thread_list[MAXPID + 1];

void init_thread() {
    // TODO: lock
    // init 'thread_list' & 'run_queue'
    run_queue = kmalloc(sizeof(thread));
    run_queue = run_queue->next = run_queue;
    run_queue->prev = run_queue;

    // TODO: unlock
}

thread *thread_create(void *funcion_start_point) {
    // TODO: lock
    thread *the_thread;
    if (pid_cnt > MAXPID)
        return 0;

    // check the pid in thread_list, if not use yet, take it
    if (!thread_list[pid_cnt].isused)
        the_thread = &thread_list[pid_cnt++];
    else
        return 0;

    // init property of 'the_thread'
    the_thread->iszombie = 0;
    the_thread->isused = 1;
    the_thread->context.lr = (unsigned long long)funcion_start_point;
    the_thread->private_stack_ptr = kmalloc(USTACK_SIZE);
    the_thread->kernel_stack_ptr = kmalloc(KSTACK_SIZE);
    // sp init to the top of allocated stack area
    the_thread->context.sp = (unsigned long long)the_thread->private_stack_ptr + USTACK_SIZE;
    the_thread->context.fp = the_thread->context.sp; // fp is the base addr, sp won't upper than fp

    // add it into run_queue tail
    the_thread->prev = run_queue->prev;
    the_thread->next = run_queue;
    the_thread->prev->next = the_thread;
    the_thread->next->prev = the_thread;

    // TODO: unlock
    return the_thread;
}