#include "thread.h"
#include "exception.h"
#include "memory.h"
#include "timer.h"
#include "u_string.h"
#include "uart1.h"

extern unsigned long switch_to();
extern unsigned long get_current();

int pid_cnt = 0;
thread *run_queue;
thread thread_list[MAXPID + 1];
thread *cur_thread;

void init_thread() {
    lock();
    // init 'thread_list' & 'run_queue'
    run_queue = kmalloc(sizeof(thread));
    run_queue->next = run_queue;
    run_queue->prev = run_queue;

    for (int i = 0; i <= MAXPID; i++) {
        // thread_list[i].isused = 0;
        thread_list[i].iszombie = 0;
        thread_list[i].pid = i;
    }
    // BUG: Don't let thread structure NULL as we enable the functionality
    cur_thread = thread_create(idle);
    asm volatile("msr tpidr_el1, %0" ::"r"(&cur_thread->context));

    unlock();
}

thread *thread_create(void *funcion_start_point) {
    lock();
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
    the_thread->private_stack_ptr = kmalloc(USTACK_SIZE);
    the_thread->kernel_stack_ptr = kmalloc(KSTACK_SIZE);
    the_thread->context.lr = (unsigned long long)funcion_start_point;
    // sp init to the top of allocated stack area
    the_thread->context.sp = (unsigned long long)the_thread->private_stack_ptr + USTACK_SIZE;
    the_thread->context.fp = the_thread->context.sp; // fp is the base addr, sp won't upper than fp

    // add it into run_queue tail
    the_thread->prev = run_queue->prev;
    the_thread->next = run_queue;
    the_thread->prev->next = the_thread;
    the_thread->next->prev = the_thread;

    unlock();
    return the_thread;
}

void from_el1_to_el0(thread *t) {
    asm volatile("msr elr_el1, lr");
    asm volatile("msr spsr_el1, %0" ::"r"(0x3c0));
    asm volatile("msr sp_el0, %0" ::"r"(t->kernel_stack_ptr + KSTACK_SIZE));
    asm volatile("eret");
}

// find a job to schedule, otherwise spinning til found
void schedule() {
    // uart_sendline("scheduling...\n");
    lock();
    // let program keep execute after 'eret'
    // thread *tmp = cur_thread; // tmp is current process, cur_thread will be next process
    do {
        cur_thread = cur_thread->next;
    } while (cur_thread == run_queue);

    // context switch (defined in asm)
    // pass both thread's addr as base addr, to load/store the registers
    switch_to(get_current(), &cur_thread->context);
    unlock();
}

void idle() {
    // TODO: program will stuck in idle
    // uart_sendline("idle...\n");
    // for (int i = 0; i < 10000000; i++)
    //     asm volatile("nop");
    kill_zombie();
    schedule();
}

void thread_exit() {
    lock();
    cur_thread->iszombie = 1;
    uart_sendline("exiting\n");
    unlock();
    schedule();
}

void kill_zombie() {
    // lock();
    for (thread *cur = run_queue->next; cur != run_queue; cur = cur->next) {
        if (cur->iszombie) {
            // remove from list
            cur->next->prev = cur->prev;
            cur->prev->next = cur->next;
            // release memory
            kfree(cur->kernel_stack_ptr);
            kfree(cur->private_stack_ptr);
            // update thread status
            cur->isused = 0;
            cur->iszombie = 0;
        }
    }
    // unlock();
}

// for video player
void thread_exec(char *code, char codesize) {
    // TODO
    // thread *t = thread_create(code);
    // t->code = kmalloc(codesize);
    // t->codesize = codesize;
    // t->context.lr = (unsigned long)t->code;

    // memcpy(t->code, code, codesize);

    // cur_thread = t;
}

void schedule_timer() {
    unsigned long long timeout;
    asm volatile("mrs %0, cntfrq_el0;" : "=r"(timeout));
    add_timer(schedule_timer, "re-schedule", timeout >> 5);
}

void foo() {
    for (int i = 0; i < 10; ++i) {
        uart_sendline("Thread id: %d %d\n", cur_thread->pid, i);
        for (int j = 0; j < 1000000; j++)
            asm volatile("nop\n\t");
        schedule();
    }
    thread_exit();
}