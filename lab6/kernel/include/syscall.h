#include <stddef.h>
#ifndef _SYSCALL_H_
#define _SYSCALL_H_

// registers are saved at the top of the kernel stack when a user process throws an exception and enters kernel mode.
// The registers are loaded before returning to user mode.
typedef struct trap_frame {
    unsigned long x0;
    unsigned long x1;
    unsigned long x2;
    unsigned long x3;
    unsigned long x4;
    unsigned long x5;
    unsigned long x6;
    unsigned long x7;
    unsigned long x8;
    unsigned long x9;
    unsigned long x10;
    unsigned long x11;
    unsigned long x12;
    unsigned long x13;
    unsigned long x14;
    unsigned long x15;
    unsigned long x16;
    unsigned long x17;
    unsigned long x18;
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
    unsigned long x29;
    unsigned long x30;
    unsigned long spsr_el1;
    unsigned long elr_el1;
    unsigned long sp_el0;
} trap_frame;

// sys calls
int getpid(trap_frame *tpf);
size_t uart_read(trap_frame *tpf, char buf[], size_t size);
size_t uart_write(trap_frame *tpf, const char buf[], size_t size);
int exec(trap_frame *tpf, const char *name, char *const argv[]);
int fork(trap_frame *tpf);
void exit(trap_frame *tpf, int status);
int mbox_call(trap_frame *tpf, unsigned char ch, unsigned int *mbox);
void kill(int pid);

void signal_register(int signal, void (*handler)());
void signal_kill(int pid, int signal);
void signal_return(trap_frame *tpf);

// components
unsigned int get_file_size(char *path);
char *get_file_start(char *path);

#endif