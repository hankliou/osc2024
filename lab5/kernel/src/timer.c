#include "timer.h"
#include "memory.h"
#include "u_string.h"
#include "uart1.h"

timer_node *timer_head; // head is empty, every node come after head

void timer_list_init() {
    timer_head = simple_malloc(sizeof(timer_node));
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
    asm volatile("mov x0, 1");
    asm volatile("msr cntp_ctl_el0, x0");  // Enable
    asm volatile("mrs x0, cntfrq_el0");    // get the timer frequency
    asm volatile("asr x0, x0, 5");         // lab5: shift 5 bits right(asr: Arithmetic Shift Right)
    asm volatile("msr cntp_tval_el0, x0"); // Set expired time
    asm volatile("mov x0, 2");             // w0 = lower half of x0
    asm volatile("ldr x1, =0x40000040");   // CORE_TIMER_IRQ_CTRL
    asm volatile("str w0, [x1]");          // Unmask timer interrupt
    unsigned long tmp;
    asm volatile("mrs %0, cntkctl_el1" : "=r"(tmp));
    tmp |= 1;
    asm volatile("msr cntkctl_el1, %0" : : "r"(tmp));
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
    // if list is empty, postpond the timer
    if (timer_head->next != timer_head)
        set_timer_interrupt_by_tick(timer_head->next->interrupt_time);
    else
        set_timer_interrupt(99999);
}

// timeout: set next [timeout] cycles for times up
void add_timer(void *callback, char *msg, unsigned long long timeout) {
    // init node
    timer_node *node = simple_malloc(sizeof(timer_node));
    node->next = timer_head;
    node->prev = timer_head;
    node->callback = callback;
    unsigned long long tick;
    asm volatile("mrs %0, cntpct_el0" : "=r"(tick));
    // asm volatile("mrs %0, cntfrq_el0" : "=r"(freq));
    node->interrupt_time = timeout + tick;
    node->msg = simple_malloc(strlen(msg) + 1); // need to free when times up(in timer_handler)
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

// set timer [expire_time] from now (abs)
void set_timer_interrupt(unsigned long long expire_time) {
    asm volatile("mrs x1, cntfrq_el0\n\t");
    asm volatile("mul x1, x1, %0\n\t" ::"r"(expire_time));
    asm volatile("msr cntp_cval_el0, x1\n\t");
}

void set_timer_interrupt_by_tick(unsigned long long time) {
    asm volatile("msr cntp_cval_el0, %0" ::"r"(time));
}

long long int getTimerFreq() {
    long long int tick;
    asm volatile("mrs %0, cntfrq_el0" : "=r"(tick));
    return tick;
}