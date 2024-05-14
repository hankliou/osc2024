#ifndef _THREAD_H_
#define _THREAD_H_

#define PID_MAX 32678 // Linux kernel defined limit 2^15
#define USTACK_SIZE 0x1000
#define KSTACK_SIZE 0x1000
#define SIGNAL_MAX 64

typedef struct thread_context {
    // callee saved registers
    // https://developer.arm.com/documentation/102374/0101/Procedure-Call-Standard
    // since on 64bits arch, regs are 64 bits = sizeof(unsigned long)
    unsigned long x19;
    unsigned long x20;
    unsigned long x21;
    unsigned long x22;
    unsigned long x23;
    unsigned long x24;
    unsigned long x25;
    unsigned long x26;
    unsigned long x27;
    unsigned long x28;
    // note: sp & fp are the up/down vaild boundary of function's stack
    unsigned long fp; // frame pointer: base pointer for local variable in stack
    unsigned long lr; // link register: store return address
    unsigned long sp; // stack pointer:
} thread_context;

typedef struct thread {
    thread_context context; // context(registers) need to store
    struct thread *next;
    struct thread *prev;
    int pid;                // process id
    char *code;             // something static like .text .data blablabla
    unsigned int codesize;  // size of static data (code)
    int isused;             // flag, check if thread is in used
    int iszombie;           // zombie flag
    char *user_stack_ptr;   // stack segment
    char *kernel_stack_ptr; // store register when enter kernel mode

    int sigcount[SIGNAL_MAX + 1];             // Signal Pending buffer
    void (*signal_handler[SIGNAL_MAX + 1])(); // Signal handlers for different signal
    void (*curr_signal_handler)();            // Allow Signal handler overwritten by others
    int signal_inProcess;                     // Signal Processing Lock (flag)
    thread_context signal_savedContext;       // Store registers before signal handler involving
} thread;

extern void switch_to(void *curr_context, void *next_context);
extern thread *get_current();
extern void store_context(thread_context *addr);
extern void load_context(thread_context *addr);

void init_thread();
thread *thread_create(void *funcion_start_point);    // runable function
void schedule();                                     // switch to next job
void idle();                                         // keep scheduling
void thread_exit();                                  // mark thread 'zombie'
void kill_zombie();                                  // remove zombie process
void thread_exec(char *code, unsigned int codesize); // exec task in new thread
void schedule_timer();                               // basic 3, "Set the expired time as core timer frequency shift right 5 bits."
void foo();                                          // test function
void from_el1_to_el0(thread *t);                     // switch to user space

#endif /* _THREAD_H_ */