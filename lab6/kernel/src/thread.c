#include "thread.h"
#include "exception.h"
#include "memory.h"
#include "mmu_constant.h"
#include "signal.h"
#include "timer.h"
#include "u_string.h"
#include "uart1.h"

thread thread_list[PID_MAX + 1];
thread *run_queue = thread_list;

int pid_cnt = 0;

void init_thread() {
    lock();
    // init 'thread_list' & 'run_queue'
    run_queue = kmalloc(sizeof(thread));
    run_queue->next = run_queue;
    run_queue->prev = run_queue;
    run_queue->pid = -1; // debug usage

    for (int i = 0; i <= PID_MAX; i++) {
        // lab5 thread's init
        thread_list[i].isused = 0;
        thread_list[i].iszombie = 0;
        thread_list[i].pid = i;
    }
    thread *cur_thread = thread_create(idle, 0x1000);
    asm volatile("msr tpidr_el1, %0" ::"r"(cur_thread));

    unlock();
}

thread *thread_create(void *func, size_t codesize) {
    lock();
    thread *the_thread;
    if (pid_cnt > PID_MAX) return 0;

    // pick a pid
    // TODO: pid_cnt need to mod max size
    if (!thread_list[pid_cnt].isused)
        the_thread = &thread_list[pid_cnt++];
    else
        return 0;

    // init property of 'the_thread'
    the_thread->iszombie = 0;
    the_thread->isused = 1;
    the_thread->context.lr = (unsigned long long)func;
    the_thread->user_stack_ptr = kmalloc(USTACK_SIZE);
    the_thread->kernel_stack_ptr = kmalloc(KSTACK_SIZE);
    the_thread->codesize = codesize;
    the_thread->code = kmalloc(codesize);
    // sp init to the top of allocated stack area
    the_thread->context.sp = (unsigned long long)the_thread->user_stack_ptr + USTACK_SIZE;
    the_thread->context.fp = the_thread->context.sp; // fp is the base addr, sp won't upper than fp

    // signal
    the_thread->signal_inProcess = 0;
    for (int i = 0; i <= SIGNAL_MAX; i++) {
        the_thread->sigcount[i] = 0;
        the_thread->signal_handler[i] = signal_default_handler;
    }

    // lab6 vm's init
    the_thread->vma_list.next = &the_thread->vma_list;
    the_thread->vma_list.prev = &the_thread->vma_list;
    the_thread->context.pgd = kmalloc(0x1000); // BUG: diff with sample, modified back, page table should left in kernel space
    memset(the_thread->context.pgd, 0, 0x1000);

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
    asm volatile("msr spsr_el1, %0" ::"r"(0x0));          // enable INT
    asm volatile("msr sp_el0, %0" ::"r"(t->context.sp));
    asm volatile("mov sp, %0" ::"r"(t->kernel_stack_ptr + KSTACK_SIZE));
    asm volatile("eret");
}

// find a job to schedule, otherwise spinning til found
void schedule() {
    lock();
    thread *cur_thread = get_current();
    do { cur_thread = cur_thread->next; } while (cur_thread == run_queue || cur_thread->iszombie);
    unlock();

    // context switch (defined in asm)
    // pass both thread's addr as base addr, to load/store the registers
    // uart_sendline("curr pid: %d, next pid: %d\n", get_current()->pid, cur_thread->pid); // FIXME
    switch_to(get_current(), cur_thread);
    // uart_sendline("scheduling\n"); // FIXME
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

            // lab6
            mmu_free_page_tables(cur->context.pgd, 0); // remove vm tables
            mmu_del_vma(cur);                          // remove vma_list

            kfree(cur->kernel_stack_ptr); // release memory
            kfree(cur->user_stack_ptr);

            cur->isused = 0; // update thread status
            cur->iszombie = 0;
        }
    }
    unlock();
}

// for video player
void thread_exec(char *code, unsigned int codesize) {
    thread *t = thread_create(code, codesize);
    memcpy(t->code, code, codesize); // copy code content to thread's mem in user space

    // VM approach (using 'VIRT2PHYS' to make the addr in user memory)
    mmu_add_vma(t, USER_KERNEL_BASE, codesize, (size_t)VIRT2PHYS(t->code), 0b111, 1);                          // space for store code
    mmu_add_vma(t, USER_STACK_TOP - USTACK_SIZE, USTACK_SIZE, (size_t)VIRT2PHYS(t->user_stack_ptr), 0b111, 1); // space for user stack
    mmu_add_vma(t, PERIPHERAL_START, PERIPHERAL_END - PERIPHERAL_START, PERIPHERAL_START, 0b011, 0);           // space for peripheral memory
    mmu_add_vma(t, USER_SIGNAL_WRAPPER_VA, 0x2000, (size_t)VIRT2PHYS(signal_handler_wrapper), 0b101, 0);       // kernel code is directly mapped

    // FIXME: print debug
    // for (vm_area_struct *it = t->vma_list.next; it != &t->vma_list; it = it->next) {
    //     uart_sendline("node at %x:\n", it);
    //     uart_sendline("next: %x\n", it->next);
    //     uart_sendline("prev: %x\n", it->prev);
    //     uart_sendline("virt: %x\n", it->virt_addr);
    //     uart_sendline("phys: %x\n", it->phys_addr);
    //     uart_sendline("size: %x\n", it->area_size);
    //     uart_sendline("xwr: %d\n", it->xwr);
    //     uart_sendline("is allocated: %d\n\n", it->is_allocated);
    // }

    t->context.pgd = VIRT2PHYS(t->context.pgd);
    t->context.sp = USER_STACK_TOP;
    t->context.fp = USER_STACK_TOP;
    t->context.lr = USER_KERNEL_BASE;

    // uart_sendline("pid: %d\n", t->pid);               // FIXME
    // uart_sendline("elr: %x\n", t->context.lr);        // FIXME
    // uart_sendline("ttbr0_el1: %x\n", t->context.pgd); // FIXME

    // vm related setup
    asm volatile("dsb ish");                                 // memory barrier
    asm volatile("msr ttbr0_el1, %0" ::"r"(t->context.pgd)); // load thread's pgd
    asm volatile("tlbi vmalle1is");                          // flush all TLB (?)
    asm volatile("dsb ish");                                 // memory barrier
    asm volatile("isb");                                     // clear pipeline

    // basic EL switch setup
    asm volatile("msr tpidr_el1, %0;" ::"r"(&t->context));                // hold the "kernel(el1)" thread structure info
    asm volatile("msr elr_el1, %0;" ::"r"(t->context.lr));                // set exception return addr to 'c_filedata'
    asm volatile("msr spsr_el1, %0;" ::"r"(0x0));                         // set state to user mode, and enable interrupt
    asm volatile("msr sp_el0, %0;" ::"r"(t->context.sp));                 //
    asm volatile("mov sp, %0;" ::"r"(t->kernel_stack_ptr + KSTACK_SIZE)); // set el0's sp to top of new stack

    // switch !!
    add_timer(schedule_timer, "", getTimerFreq());
    asm volatile("eret"); // switch EL to 0
}

void schedule_timer() { add_timer(schedule_timer, "re-schedule", getTimerFreq() >> 5); }

void foo() {
    for (int i = 0; i < 10; ++i) {
        uart_sendline("Thread id: %d %d\n", get_current()->pid, i);
        for (int j = 0; j < 1000000; j++) asm volatile("nop\n\t");
        schedule();
    }
    thread_exit();
}