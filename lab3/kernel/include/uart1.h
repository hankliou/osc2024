#ifndef _UART1_H_
#define _UART1_H_

void uart_init();
char uart_getc();
char uart_recv();
void uart_send(char c);
int uart_sendline(char *fmt, ...);
char uart_async_getc();
void uart_async_putc(char c);
int uart_puts(char *fmt, ...);
void uart_2hex(unsigned int d);

void uart_rx_irq_handler();
void uart_tx_irq_handler();

void uart_rx_irq_enable();
void uart_rx_irq_disable();
void uart_tx_irq_enable();
void uart_tx_irq_disable();

#endif /*_UART1_H_*/