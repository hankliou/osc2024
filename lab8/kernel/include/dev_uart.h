#ifndef _DEV_UART_H_
#define _DEV_UART_H_

#include "stddef.h"
#include "vfs.h"

int init_dev_uart();

int dev_uart_write(file *file, const void *buf, size_t len);
int dev_uart_read(file *file, void *buf, size_t len);
int dev_uart_open(vnode *file_node, file **target);
int dev_uart_close(file *file);
int dev_uart_op_deny();

#endif