#include "dtb.h"
#include "exception.h"
#include "memory.h"
#include "shell.h"
#include "thread.h"
#include "timer.h"
#include "u_string.h"
#include "uart1.h"

extern char *dtb_ptr;
char input_buffer[CMD_MAX_LEN];

void main(char *arg) {
    // debug: check EL(exception level) by watch reg
    checkEL();

    dtb_ptr = arg;
    traverse_device_tree(dtb_ptr, dtb_callback_initramfs);

    timer_init_interrupt(); // basic 2, core timer interrupt
    timer_list_init();      // advanced 1, timer multiplexing
    irqtask_list_init();    // advanced 2, concurrent IO handling

    uart_init();
    uart_sendline("Loading dtb from: 0x%x\n", arg);
    cli_print_banner();

    allocator_init();
    init_thread();

    // debug: trying switch to el0
    // checkEL();
    // lock();
    // asm volatile("mov x1, %0" ::"r"(0x345)); // EL1h (SPSel = 1) with interrupt disabled
    // void (*ptr)() = fork_test;
    // asm volatile("msr elr_el1, %0" : "=r"(ptr));
    // asm volatile("msr spsr_el1, %0" ::"r"(0x0));
    // // asm volatile("msr sp_el0, %0" : "=r"(t->context.sp));
    // asm volatile("eret");

    while (1) {
        cli_cmd_clear(input_buffer, CMD_MAX_LEN);
        uart_sendline("# ");
        cli_cmd_read(input_buffer);
        cli_cmd_exec(input_buffer);
    }
}