#include "signal.h"
#include "exception.h"
#include "memory.h"
#include "syscall.h"
#include "thread.h"

extern thread *cur_thread;
extern void *store_context(void *cur_context);

void signal_default_handler() {
    kill(0, cur_thread->pid);
}

void check_signal(trap_frame *tpf) {
    // no need to handle nested signal handling
    if (cur_thread->signal_inProcess)
        return;
    lock();
    cur_thread->signal_inProcess = 1;
    unlock();

    for (int i = 0; i <= SIGNAL_MAX; i++) {
        store_context(&cur_thread->signal_savedContext);
        if (cur_thread->sigcount[i] > 0) {
            lock();
            cur_thread->sigcount[i]--;
            unlock();
            run_signal(tpf, i);
        }
    }

    lock();
    cur_thread->signal_inProcess = 0;
    unlock();
}

void run_signal(trap_frame *tpf, int idx) {
    cur_thread->curr_signal_handler = cur_thread->signal_handler[idx];

    // run default handler in kernel mode
    if (cur_thread->curr_signal_handler == signal_default_handler) {
        signal_default_handler();
        return;
    }

    // run registered handler in user mode
    char *signal_user_stack = kmalloc(USTACK_SIZE);
    asm volatile("msr elr_el1, %0" ::"r"(signal_handler_wrapper));
    asm volatile("msr sp_el0, %0" ::"r"(signal_user_stack + USTACK_SIZE));
    asm volatile("msr spsr_el1, %0" ::"r"(tpf->spsr_el1));
    asm volatile("eret");
}

void signal_handler_wrapper() {
    (cur_thread->curr_signal_handler)();
    // [signal_return] system call
    asm volatile("mov x8, 99");
    asm volatile("svc 0");
}