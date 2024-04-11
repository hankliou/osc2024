#include "dtb.h"
#include "exception.h"
#include "heap.h"
#include "shell.h"
#include "timer.h"
#include "u_string.h"
#include "uart1.h"

extern char *dtb_ptr;
char input_buffer[CMD_MAX_LEN];

void main(char *arg) {
    // debug: check EL(exception level) by watch reg
    unsigned long el;
    asm volatile("mrs %0, CurrentEL" : "=r"(el));
    uart_sendline("Current EL is: ");
    uart_2hex((el >> 2) & 3);
    uart_sendline("\n");

    dtb_ptr = arg;
    traverse_device_tree(dtb_ptr, dtb_callback_initramfs);

    timer_init_interrupt(); // basic 2, core timer interrupt
    timer_list_init();      // advanced 1, timer multiplexing
    irqtask_list_init();    // advanced 2, concurrent IO handling

    uart_init();
    uart_sendline("Loading dtb from: 0x%x\n", arg);
    cli_print_banner();

    while (1) {
        cli_cmd_clear(input_buffer, CMD_MAX_LEN);
        uart_sendline("\n# ");
        cli_cmd_read(input_buffer);
        cli_cmd_exec(input_buffer);
    }
}