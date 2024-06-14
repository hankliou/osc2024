#include "dtb.h"
#include "exception.h"
#include "memory.h"
#include "shell.h"
#include "thread.h"
#include "timer.h"
#include "u_string.h"
#include "uart1.h"
#include "vfs.h"

extern char *dtb_ptr;
char         input_buffer[CMD_MAX_LEN];

void main(char *arg) {
    uart_init();

    dtb_ptr = PHYS2VIRT(arg);
    traverse_device_tree(dtb_ptr, dtb_callback_initramfs);
    uart_sendline("Loading dtb from: 0x%x\n", dtb_ptr);

    allocator_init();
    init_thread();
    init_rootfs();

    cli_print_banner();

    timer_init_interrupt(); // basic 2, core timer interrupt
    timer_list_init();      // advanced 1, timer multiplexing
    irqtask_list_init();    // advanced 2, concurrent IO handling

    while (1) {
        cli_cmd_clear(input_buffer, CMD_MAX_LEN);
        uart_sendline("# ");
        cli_cmd_read(input_buffer);
        cli_cmd_exec(input_buffer);
    }
}