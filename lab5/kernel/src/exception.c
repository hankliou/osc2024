#include "exception.h"
#include "bcm2837/rpi_irq.h"
#include "bcm2837/rpi_uart1.h"
#include "memory.h"
#include "syscall.h"
#include "thread.h"
#include "timer.h"
#include "uart1.h"

irq_node *irq_head; // head is empty, every node come after head

void lock() {
    el1_interrupt_disable();
}

void unlock() {
    el1_interrupt_enable();
}

void el0_sync_router(trap_frame *tpf) {
    int syscall_no = tpf->x8;
    switch (syscall_no) {
    case 0:
        getpid(tpf);
        break;
    case 1:
        // user have to allocate spaces to x0(buf) themself
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
    default:
        uart_sendline("Invalid System Call Number\n");
    }
}

void el1h_irq_router() {
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
        // schedule();
    }
}

void invalid_exception_router() {
    uart_sendline("invalid exception !\n");
}

/* implement preemption */
void irqtask_list_init() {
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
    node->next = irq_head;
    node->prev = irq_head;
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
