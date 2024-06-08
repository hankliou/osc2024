#include "dev_uart.h"
#include "memory.h"
#include "uart1.h"

file_operations dev_file_operations = {
    dev_uart_write,           //
    dev_uart_read,            //
    dev_uart_open,            //
    dev_uart_close,           //
    (void *)dev_uart_op_deny, //
    (void *)dev_uart_op_deny, //
};

int init_dev_uart() {
    return register_dev(&dev_file_operations);
}

int dev_uart_write(file *file, const void *buf, size_t len) {
    // uart_sendline("dev uart writing\n"); // FIXME
    const char *char_buf = buf;
    for (int i = 0; i < len; i++) uart_async_putc(char_buf[i]);
    return len;
}

int dev_uart_read(file *file, void *buf, size_t len) {
    // uart_sendline("dev uart reading\n"); // FIXME
    char *char_buf = buf;
    for (int i = 0; i < len; i++) char_buf[i] = uart_async_getc();
    return len;
}

int dev_uart_open(vnode *file_node, file **target) {
    (*target)->vnode = file_node;
    (*target)->f_ops = &dev_file_operations;
    return 0;
}

int dev_uart_close(file *file) {
    kfree(file);
    return 0;
}

int dev_uart_op_deny() {
    return -1;
}