#include "signal.h"
#include "exception.h"
#include "memory.h"
#include "mmu_constant.h"
#include "syscall.h"
#include "thread.h"
#include "uart1.h"

extern void store_context(thread_context *addr);
static void (*cur_signal_handler)() = signal_default_handler;

void signal_default_handler() { kill(get_current()->pid); }

void check_signal(trap_frame *tpf) {
    // no need to handle nested signal
    if (get_current()->signal_inProcess) return;
    lock();
    get_current()->signal_inProcess = 1;
    unlock();

    for (int i = 0; i <= SIGNAL_MAX; i++) {
        store_context(&(get_current()->signal_savedContext));
        if (get_current()->sigcount[i] > 0) {
            lock();
            get_current()->sigcount[i]--;
            unlock();
            run_signal(tpf, i);
        }
    }

    lock();
    get_current()->signal_inProcess = 0;
    unlock();
}

void run_signal(trap_frame *tpf, int idx) {
    get_current()->curr_signal_handler = get_current()->signal_handler[idx];

    // run default handler in kernel mode
    if (get_current()->curr_signal_handler == signal_default_handler) {
        signal_default_handler();
        return;
    }

    // run registered handler in user mode
    cur_signal_handler = get_current()->curr_signal_handler;
    asm volatile("msr elr_el1, %0" ::"r"(USER_SIGNAL_WRAPPER_VA + ((size_t)signal_handler_wrapper % 0x1000)));
    asm volatile("msr sp_el0, %0" ::"r"(tpf->sp_el0));
    asm volatile("msr spsr_el1, %0" ::"r"(tpf->spsr_el1));
    asm volatile("mov x0, %0" ::"r"(get_current()->curr_signal_handler)); // pass handler's va
    asm volatile("eret");
}

void signal_handler_wrapper() {
    // [signal_return] system call
    asm volatile("blr x0");
    asm volatile("mov x8, 99");
    asm volatile("svc 0");
}