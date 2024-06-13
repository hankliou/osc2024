#ifndef _EXPTION_H_
#define _EXPTION_H_

#include "bcm2837/rpi_irq.h"

#define CORE_INTERRUPT_SOURCE      ((volatile unsigned int *)(PHYS2VIRT(0X40000060)))
#define IRQ_PENDING_1_AUX_INT      (1 << 29)
#define INTERRUPT_SOURCE_GPU       (1 << 8)
#define INTERRUPT_SOURCE_CNTPNSIRQ (1 << 1)

/*
ARM Peripherals interrupts table
    0: arm timer
    1: arm mailbox
    2: arm doorbell 0
    3: arm doorbell 1
    4: gpu0 halted (or gpu1 halt fi bit 10 of fontrol reg 1 is set)
    5: gpu1 halted
    6: illegal access type 1
    7: illegal access type 0
*/
#define UART_IRQ_PRIORITY  1
#define TIMER_IRQ_PRIORITY 0

typedef struct irq_node {
    struct irq_node *prev;
    struct irq_node *next;
    unsigned long long priority; // store priority (smaller number is more preemptive)
    void *task_function;         // task function ptr
} irq_node;

void irqtask_list_init();
void irq_list_insert_front(irq_node *node, irq_node *it);
void add_irq_task(void *callback, unsigned priority);

// https://github.com/Tekki/raspberrypi-documentation/blob/master/hardware/raspberrypi/bcm2836/QA7_rev3.4.pdf
// p16
#define CORE0_INTERRUPT_SOURCE ((volatile unsigned int *)(PHYS2VIRT(0x40000060)))

#define INTERRUPT_SOURCE_CNTPNSIRQ                                                                                                                   \
    (1 << 1) // physical non-secure, interrupt, counter. often use for physical
             // counter
#define INTERRUPT_SOURCE_GPU (1 << 8)

#define IRQ_PENDING_1_AUX_INT (1 << 29)

void el1_interrupt_enable();
void el1_interrupt_disable();

void el1h_irq_router();
void el0_sync_router();

void invalid_exception_router(); // exception_handler.S

void lock();
void unlock();
int getCurrentEL();

#endif