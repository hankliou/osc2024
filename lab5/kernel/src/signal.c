#include "signal.h"
#include "syscall.h"
#include "thread.h"

extern thread *cur_thread;

void signal_default_handler() {
    kill(0, cur_thread->pid);
}

void check_signal(trap_frame *tpf) {
}

void run_signal(trap_frame *tpf, int signal) {
}

void signal_handler_wrapper() {
}