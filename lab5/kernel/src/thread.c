#include "thread.h"
#include "exception.h"
#include "memory.h"
#include "signal.h"
#include "timer.h"
#include "u_string.h"
#include "uart1.h"

thread *run_queue;
thread thread_list[PID_MAX + 1];

int pid_cnt = 0;

void init_thread() {
    lock();
    // init 'thread_list' & 'run_queue'
    run_queue = kmalloc(sizeof(thread));
    run_queue->next = run_queue;
    run_queue->prev = run_queue;
    run_queue->pid = -1; // debug usage

    for (int i = 0; i <= PID_MAX; i++) {
        thread_list[i].isused = 0;
        thread_list[i].iszombie = 0;
        thread_list[i].pid = i;
        thread_list[i].signal_inProcess = 0;
    }
    thread *cur_thread = thread_create(idle);
    asm volatile("msr tpidr_el1, %0" ::"r"(cur_thread));

    unlock();
}

thread *thread_create(void *funcion_start_point) {
    lock();
    thread *the_thread;
    if (pid_cnt > PID_MAX)
        return 0;

    // check the pid in thread_list, if not use yet, take it
    // TODO: pid_cnt need to mod max size
    if (!thread_list[pid_cnt].isused)
        the_thread = &thread_list[pid_cnt++];
    else
        return 0;

    // init property of 'the_thread'
    the_thread->iszombie = 0;
    the_thread->isused = 1;
    the_thread->user_stack_ptr = kmalloc(USTACK_SIZE);
    // uart_sendline("ustack allocate: 0x%x\n", the_thread->user_stack_ptr);
    the_thread->kernel_stack_ptr = kmalloc(KSTACK_SIZE);
    // uart_sendline("kstack allocate: 0x%x\n", the_thread->kernel_stack_ptr);
    the_thread->context.lr = (unsigned long)funcion_start_point;
    // sp init to the top of allocated stack area
    the_thread->context.sp = (unsigned long)the_thread->user_stack_ptr + USTACK_SIZE;
    the_thread->context.fp = the_thread->context.sp; // fp is the base addr, sp won't upper than fp

    // signal
    the_thread->signal_inProcess = 0;
    for (int i = 0; i <= SIGNAL_MAX; i++) {
        the_thread->sigcount[i] = 0;
        the_thread->signal_handler[i] = signal_default_handler;
    }

    // add it into run_queue tail
    the_thread->prev = run_queue->prev;
    the_thread->next = run_queue;
    the_thread->prev->next = the_thread;
    the_thread->next->prev = the_thread;

    unlock();
    return the_thread;
}

void from_el1_to_el0(thread *t) {
    asm volatile("msr tpidr_el1, %0" ::"r"(&t->context)); // hold the kernel(el1) thread structure info
    asm volatile("msr elr_el1, lr");                      // get back to caller function
    asm volatile("msr spsr_el1, %0" ::"r"(0x340));        // disable E A I F
    asm volatile("msr sp_el0, %0" ::"r"(t->context.sp));
    asm volatile("mov sp, %0" ::"r"(t->kernel_stack_ptr + KSTACK_SIZE));
    asm volatile("eret");
}

// find a job to schedule, otherwise spinning til found
void schedule() {
    lock();

    thread *cur_thread = get_current();
    do {
        cur_thread = cur_thread->next;
    } while (cur_thread == run_queue);
    unlock();

    // context switch (defined in asm)
    // pass both thread's addr as base addr, to load/store the registers
    switch_to(get_current(), cur_thread);
}

void idle() {
    while (1) {
        kill_zombie();
        schedule();
    };
}

void thread_exit() {
    lock();
    get_current()->iszombie = 1;
    unlock();
    schedule();
}

void kill_zombie() {
    lock();
    for (thread *cur = run_queue->next; cur != run_queue; cur = cur->next) {
        if (cur->iszombie) {
            // remove from list
            cur->next->prev = cur->prev;
            cur->prev->next = cur->next;
            // release memory
            kfree(cur->kernel_stack_ptr);
            kfree(cur->user_stack_ptr);
            // update thread status
            cur->isused = 0;
            cur->iszombie = 0;
        }
    }
    unlock();
}

// for video player
void thread_exec(char *code, unsigned int codesize) {
    thread *t = thread_create(code);
    t->codesize = codesize;
    t->code = kmalloc(codesize);
    memcpy(t->code, code, codesize);
    t->context.lr = (unsigned long)t->code;
    asm volatile("msr tpidr_el1, %0;" ::"r"(&t->context));                // hold the "kernel(el1)" thread structure info
    asm volatile("msr elr_el1, %0;" ::"r"(t->context.lr));                // set exception return addr to 'c_filedata'
    asm volatile("msr spsr_el1, %0;" ::"r"(0x0));                         // set state to user mode, and enable interrupt
    asm volatile("msr sp_el0, %0;" ::"r"(t->context.sp));                 //
    asm volatile("mov sp, %0;" ::"r"(t->kernel_stack_ptr + KSTACK_SIZE)); // set el0's sp to top of new stack
    add_timer(schedule_timer, "", getTimerFreq());
    asm volatile("eret;"); // switch EL to 0
}

void schedule_timer() {
    add_timer(schedule_timer, "re-schedule", getTimerFreq() >> 5);
}

void foo() {
    for (int i = 0; i < 10; ++i) {
        uart_sendline("Thread id: %d %d\n", get_current()->pid, i);
        for (int j = 0; j < 1000000; j++)
            asm volatile("nop\n\t");
        schedule();
    }
    thread_exit();
}