#ifndef _THREAD_H_
#define _THREAD_H_

#define MAXPID 32678 // Linux kernel defined limit 2^15

typedef struct thread_context {
    // callee saved registers
    // https://developer.arm.com/documentation/102374/0101/Procedure-Call-Standard
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
    struct thread *next;
    struct thread *prev;
    char *data;                     // ??
    unsigned int datasize;          // ??
    int iszombie;                   // zombie flag
    int pid;                        // process id
    int isused;                     // ??
    char *stack_alloced_ptr;        // ??
    char *kernel_stack_alloced_ptr; // ??
    thread_context context;         // context(registers) need to store
} thread;

void init_thread();
thread *thread_create(void *func); // runable function
void foo();                        // test function

#endif /* _THREAD_H_ */