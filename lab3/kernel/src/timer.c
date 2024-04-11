#include "timer.h"
#include "heap.h"
#include "u_string.h"
#include "uart1.h"

timer_node *timer_head; // head is empty, every node come after head

void timer_list_init() {
    timer_head = malloc(sizeof(timer_node));
    timer_head->next = timer_head;
    timer_head->prev = timer_head;
}

void timer_list_insert_front(timer_node *node, timer_node *it) {
    node->prev = it->prev;
    node->next = it;
    it->prev->next = node;
    it->prev = node;
}

void timer_init_interrupt() {
    asm volatile("mov x0, 1;"
                 "msr cntp_ctl_el0, x0;" // Enable
                 "mrs x0, cntfrq_el0;"
                 "msr cntp_tval_el0, x0;" // Set expired time
                 "mov x0, 2;"
                 "ldr x1, =0x40000040;" //  CORE_TIMER_IRQ_CTRL
                 "str w0, [x1]\n\t");   // Unmask timer interrupt
}

void timer_enable_interrupt() {
    asm volatile("mov x0, 1\n\t");
    asm volatile("msr cntp_ctl_el0, x0\n\t");
}

void timer_disable_interrupt() {
    asm volatile("mov x0, 0\n\t");
    asm volatile("msr cntp_ctl_el0, x0\n\t");
}

void timer_handler() {
    timer_node *node = timer_head->next;
    // list not empyt
    if (timer_head->next != timer_head) {
        // ((ret_type (*)(arg_type)) fptr) (args);
        ((void (*)(char *))node->callback)(node->msg);

        // remove current node
        // connect front and back, then free the space
        node->prev->next = node->next;
        node->next->prev = node->prev;
        free(node->msg);
        free(node);
    }
    // if list is not empty re-new timer, else postpond the timer
    if (timer_head->next != timer_head)
        set_timer_interrupt_by_tick(timer_head->next->interrupt_time);
    else
        set_timer_interrupt(99999);
}

void add_timer(void *callback, char *msg, unsigned long long timeout) {
    // init node
    timer_node *node = malloc(sizeof(timer_node));
    node->next = timer_head;
    node->prev = timer_head;
    node->callback = callback;
    unsigned long long tick, freq;
    asm volatile("mrs %0, cntpct_el0" : "=r"(tick));
    asm volatile("mrs %0, cntfrq_el0" : "=r"(freq));
    node->interrupt_time = timeout * freq + tick;
    node->msg =
        malloc(strlen(msg) + 1); // need to free when times up(in timer_handler)
    strcpy(node->msg, msg);

    // insert node into list
    timer_node *it = timer_head->next;
    for (; it != timer_head; it = it->next) {
        if (node->interrupt_time < it->interrupt_time) {
            timer_list_insert_front(node, it);
            break;
        }
    }
    // if node time is longest, put it in last pos
    if (it == timer_head)
        timer_list_insert_front(node, timer_head);

    // set tick
    set_timer_interrupt_by_tick(timer_head->next->interrupt_time);
}

void timer_print_msg(char *msg) {
    uart_sendline("%s\n", msg);
}

// set timer [expire_time] from now (related)
void set_timer_interrupt(unsigned long long expire_time) {
    asm volatile("mrs x1, cntfrq_el0\n\t");
    asm volatile("mul x1, x1, %0\n\t" : "=r"(expire_time));
    asm volatile("msr cntp_cval_el0, x1\n\t");
}

// set timer [expire_time] from now (abs)
void set_timer_interrupt_by_tick(unsigned long long time) {
    asm volatile("msr cntp_cval_el0, %0" : "=r"(time));
}