#ifndef _THREAD_H_
#define _THREAD_H_

#define MAXPID 32678 // Linux kernel defined limit 2^15
#define USTACK_SIZE 0x10000
#define KSTACK_SIZE 0x10000

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
    char *code;              // something static like .text .data blablabla
    unsigned int codesize;   // size of static data (code)
    int iszombie;            // zombie flag
    int pid;                 // process id
    int isused;              // flag, check if thread is in used
    char *private_stack_ptr; // stack segment
    char *kernel_stack_ptr;  // store register when enter kernel mode
    thread_context context;  // context(registers) need to store
} thread;

void init_thread();
thread *thread_create(void *funcion_start_point); // runable function
void schedule();                                  // switch to next job
void idle();                                      // keep scheduling
void thread_exit();                               // mark thread 'zombie'
void kill_zombie();                               // remove zombie process
void foo();                                       // test function

#endif /* _THREAD_H_ */