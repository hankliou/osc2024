#include "exception.h"
#include "bcm2837/rpi_irq.h"
#include "bcm2837/rpi_uart1.h"
#include "memory.h"
#include "signal.h"
#include "syscall.h"
#include "thread.h"
#include "timer.h"
#include "uart1.h"

irq_node *irq_head; // head is empty, every node come after head
extern thread *run_queue;

static unsigned long long lock_count = 0;
void lock() {
    el1_interrupt_disable(); // enter critical section
    lock_count++;
}

void unlock() {
    lock_count--;
    if (lock_count <= 0)
        el1_interrupt_enable(); // leave critical section
}

void el0_sync_router(trap_frame *tpf) {
    // unsigned long long spsrel1;
    // asm volatile("mrs %0, spsr_el1" : "=r"(spsrel1));
    // unsigned long long elrel1;
    // asm volatile("mrs %0, elr_el1" : "=r"(elrel1));
    // unsigned long long esrel1;
    // asm volatile("mrs %0, esr_el1" : "=r"(esrel1));
    // uart_sendline("spsr_el1: %x, elr_el1: %x, esr_el1: %x\n", spsrel1, elrel1, esrel1);
    // return;

    el1_interrupt_enable();
    int syscall_no = tpf->x8;
    // uart_sendline("syscall: %d\n", syscall_no);
    // uart_sendline("tpf: %x\n", tpf);
    switch (syscall_no) {
    case 0:
        getpid(tpf);
        break;
    case 1:
        // user have to allocate spaces to x0(buf) theirself
        uart_read(tpf, (char *)tpf->x0, tpf->x1);
        break;
    case 2:
        uart_write(tpf, (char *)tpf->x0, tpf->x1);
        break;
    case 3:
        exec(tpf, (char *)tpf->x0, (char **)tpf->x1);
        break;
    case 4:
        fork(tpf);
        break;
    case 5:
        exit(tpf, tpf->x0);
        break;
    case 6:
        mbox_call(tpf, (unsigned char)tpf->x0, (unsigned int *)tpf->x1);
        break;
    case 7:
        kill(tpf, tpf->x0);
        break;
    case 8:
        signal_register(tpf->x0, (void (*)())(tpf->x1));
        break;
    case 9:
        signal_kill(tpf->x0, tpf->x1);
        break;
    case 99:
        signal_return(tpf);
        break;
    default:
        uart_sendline("Invalid System Call Number\n");
    }
    // uart_sendline("syscall end\n");
}

void el1h_irq_router(trap_frame *tpf) {
    // uart
    if (*IRQ_PENDING_1 & (1 << 29)) {
        switch (*AUX_MU_IIR_REG & 0x6) {
        case 0x2: // transmit interrupt
            uart_tx_irq_disable();
            add_irq_task(uart_tx_irq_handler, UART_IRQ_PRIORITY);
            break;
        case 0x4: // receive interrupt
            uart_rx_irq_disable();
            add_irq_task(uart_rx_irq_handler, UART_IRQ_PRIORITY);
            break;
        }
    }
    // timer
    else if (*CORE0_INTERRUPT_SOURCE & 0x2) {
        timer_disable_interrupt();
        add_irq_task(timer_handler, TIMER_IRQ_PRIORITY);
        timer_enable_interrupt();
        // at least two thread running -> schedule for any timer irq
        if (run_queue && run_queue->next && run_queue->next->next != run_queue)
            schedule();
    }

    // TODO
    // M[3:0], 0: User, 1:FIQ, 2:IRQ, 3:Supervisor
    if ((tpf->spsr_el1 & 0b1100) == 0) {
        check_signal(tpf);
    }
}

void invalid_exception_router(int no) {
    uart_sendline("invalid exception [%d]\n", no);
    unsigned long esr, elr;
    asm volatile("mrs %0, esr_el1" : "=r"(esr));
    asm volatile("mrs %0, elr_el1" : "=r"(elr));
    uart_sendline("esr_el1: %x, elr_el1: %x\n", esr, elr);
    while (1) {};
}

/* implement preemption */
void irqtask_list_init() {
    irq_head = simple_malloc(sizeof(irq_node));
    irq_head->next = irq_head;
    irq_head->prev = irq_head;
}

void irq_list_insert_front(irq_node *node, irq_node *it) {
    node->prev = it->prev;
    node->next = it;
    it->prev->next = node;
    it->prev = node;
}

void el1_interrupt_enable() {
    asm volatile("msr daifclr, 0xf"); // enable all interrupt, 0 is default enable
}

void el1_interrupt_disable() {
    asm volatile("msr daifset, 0xf"); // disable all interrupt, 1 is disable
}

void add_irq_task(void *callback, unsigned priority) {
    // init node
    irq_node *node = simple_malloc(sizeof(irq_node));
    node->priority = priority;
    node->task_function = callback;

    // mask interrupt line
    el1_interrupt_disable();

    // insert node into list
    irq_node *it = irq_head->next;
    for (; it != irq_head; it = it->next) {
        if (node->priority < it->priority) {
            irq_list_insert_front(node, it);
            break;
        }
    }
    // if not inserted, add at last pos
    if (it == irq_head)
        irq_list_insert_front(node, it);

    // unmask interrupt line
    el1_interrupt_enable();

    // do the task
    while (irq_head->next != irq_head) {

        // ((void (*)())irq_head->next->task_function)(); // run
        /*
            it will corrupt if run above line
            cause it may be compile into more then one line of asm
            if interrupt occur, boom
            !!!!! be careful using pointer !!!!!
        */

        el1_interrupt_disable();                    // disable interrupt
        void *task = irq_head->next->task_function; // copy task function
        // remove node
        irq_node *n = irq_head->next;
        irq_head->next = n->next;
        n->next->prev = irq_head;
        free(n);
        el1_interrupt_enable(); // enable interrupt

        // execute event with interrupt enabled
        ((void (*)())task)();
    }
}

int getCurrentEL() {
    // debug: check EL(exception level) by watch reg
    unsigned long el;
    asm volatile("mrs %0, CurrentEL" : "=r"(el));
    uart_sendline("Current EL is: ");
    uart_2hex((el >> 2) & 3);
    uart_sendline("\n");
    return ((el >> 2) & 3);
}