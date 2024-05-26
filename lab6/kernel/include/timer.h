#ifndef _TIMER_H_
#define _TIMER_H_

typedef struct timer_node {
    struct timer_node *next;           // next node
    struct timer_node *prev;           // prev node
    unsigned long long interrupt_time; // store as tick time after cpu start
    void *callback;                    // interrupt -> timer_callback -> callback(args)
    char *msg;                         // msg
} timer_node;

void timer_init_interrupt();
// BUG: lack of masking timer control bit (0x4000 0040)
void timer_enable_interrupt();
void timer_disable_interrupt();

void timer_list_init();
void timer_list_insert_front(timer_node *node, timer_node *it);

void add_timer(void *callback, char *msg, unsigned long long timeout); // timeout: set next [timeout] cycles for times up
void timer_handler();
void set_timer_interrupt(unsigned long long tick);
void set_timer_interrupt_by_tick(unsigned long long time);

void timer_print_msg(char *msg);
long long int getTimerFreq(); // get the cycle from booting

#endif /* _TIMER_H_ */