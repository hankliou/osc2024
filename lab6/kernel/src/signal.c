#include "signal.h"
#include "exception.h"
#include "memory.h"
#include "syscall.h"
#include "thread.h"
#include "uart1.h"

extern void store_context(thread_context *addr);
static void (*cur_signal_handler)() = signal_default_handler;

void signal_default_handler() {
    uart_sendline("get current: %d\n", get_current());
    kill(get_current()->pid);
}

void check_signal(trap_frame *tpf) {
    // no need to handle nested signal
    // if (!get_current()) {
    //     uart_sendline("error\n");
    //     return;
    // }
    // uart_sendline("%d, ", get_current()->pid);
    // uart_sendline("%d\n", get_current()->signal_inProcess);
    if (get_current()->signal_inProcess)
        return;
    lock();
    get_current()->signal_inProcess = 1;
    unlock();

    // uart_sendline("c ");
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
    char *signal_user_stack = kmalloc(USTACK_SIZE);
    cur_signal_handler = get_current()->curr_signal_handler;
    asm volatile("msr elr_el1, %0" ::"r"(signal_handler_wrapper));
    asm volatile("msr sp_el0, %0" ::"r"(signal_user_stack + USTACK_SIZE));
    asm volatile("msr spsr_el1, %0" ::"r"(0));
    asm volatile("eret");
}

void signal_handler_wrapper() {
    if (cur_signal_handler)
        cur_signal_handler();
    // [signal_return] system call
    asm volatile("mov x8, 99");
    asm volatile("svc 0");
}