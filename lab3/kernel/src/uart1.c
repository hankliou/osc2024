#include "uart1.h"
#include "bcm2837/rpi_gpio.h"
#include "bcm2837/rpi_irq.h"
#include "bcm2837/rpi_uart1.h"
#include "u_string.h"

char uart_tx_buffer[VSPRINT_MAX_BUF_SIZE] = {};
char uart_rx_buffer[VSPRINT_MAX_BUF_SIZE] = {};
unsigned int uart_tx_read = 0;
unsigned int uart_tx_write = 0;
unsigned int uart_rx_read = 0;
unsigned int uart_rx_write = 0;

void uart_init() {
    register unsigned int r;

    /* initialize UART */
    *AUX_ENABLES |= 1;    // enable UART1
    *AUX_MU_CNTL_REG = 0; // disable TX/RX

    /* configure UART */
    *AUX_MU_IER_REG = 0;    // disable interrupt
    *AUX_MU_LCR_REG = 3;    // 8 bit data size
    *AUX_MU_MCR_REG = 0;    // disable flow control
    *AUX_MU_BAUD_REG = 270; // 115200 baud rate
    *AUX_MU_IIR_REG = 0x6;  // disable FIFO

    /* map UART1 to GPIO pins */
    r = *GPFSEL1;    // load GPFSEL1(register) to r
    r &= ~(7 << 12); // clean gpio14
    r |= 2 << 12;    // set gpio14 to alt5
    r &= ~(7 << 15); // clean gpio15
    r |= 2 << 15;    // set gpio15 to alt5
    *GPFSEL1 = r;

    /* enable pin 14, 15 - ref: Page 101 */
    *GPPUD = 0; // gpio pull-up/down, enable/disable gpio 上拉/下拉電阻
    r = 150;    // wait
    while (r--) {
        asm volatile("nop");
    }
    *GPPUDCLK0 = (1 << 14) | (1 << 15); // 啟用 pull-up/down 電阻
    r = 150;
    while (r--) {
        asm volatile("nop");
    }
    *GPPUDCLK0 = 0; // clear register

    *AUX_MU_CNTL_REG = 3;      // Enable transmitter and receiver
    *ENABLE_IRQS_1 |= 1 << 29; // Enable uart interrupt
}

// read big file usage(no output when read)
char uart_getc() {
    char r;
    while (!(*AUX_MU_LSR_REG & 0x01)) {};
    r = (char)(*AUX_MU_IO_REG);
    return r;
}

char uart_recv() {
    char r;
    // wait until LSR(fifo receiver) receive data
    while (!(*AUX_MU_LSR_REG & 0x01)) {};
    r = (char)(*AUX_MU_IO_REG); // save data in 'r'
    uart_send(r);
    if (r == '\r') {
        uart_send('\n');
    }
    return r == '\r' ? '\n' : r;
}

void uart_send(char c) {
    while (!(*AUX_MU_LSR_REG & 0x20)) {};
    *AUX_MU_IO_REG = c;
}

int uart_puts(char *fmt, ...) {
    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    char buf[VSPRINT_MAX_BUF_SIZE];

    char *str = (char *)buf;
    int count = vsprintf(str, fmt, args);

    while (*str) {
        if (*str == '\n')
            uart_send('\r');
        uart_send(*str++);
    }

    __builtin_va_end(args);
    return count;
}

void uart_2hex(unsigned int d) {
    unsigned int n;
    int c;
    for (c = 28; c >= 0; c -= 4) {
        n = (d >> c) & 0xF;       // get 4 bits
        n += n > 9 ? 0x37 : 0x30; // if > 10, trans it to 'A'
        uart_send(n);
    }
}

int uart_sendline(char *fmt, ...) {
    __builtin_va_list args;
    __builtin_va_start(args, fmt);
    char buf[VSPRINT_MAX_BUF_SIZE]; // buf to get formatted string

    char *str = (char *)buf; // trans it to a ptr
    int count = vsprintf(str, fmt, args);

    while (*str) {
        if (*str == '\n') {
            uart_send('\r');
        }
        uart_send(*str++);
    }
    __builtin_va_end(args);
    return count;
}

void uart_tx_irq_handler() {
    // buffer empty
    if (uart_tx_write == uart_tx_read) {
        uart_tx_irq_disable(); // disable wtite interrupt
        return;
    }
    // critical
    uart_tx_irq_disable(); // disable write interrupt
    // print if buffer not empty
    while (uart_tx_read != uart_tx_write) {
        *AUX_MU_IO_REG = uart_tx_buffer[uart_tx_read++]; // print
        uart_tx_read %= VSPRINT_MAX_BUF_SIZE;
    }
    // end critical (tx irq will be enable again in next 'async putc' call)
}

void uart_rx_irq_handler() {
    // Check if buffer is full
    if ((uart_rx_write + 1) % VSPRINT_MAX_BUF_SIZE == uart_rx_read) {
        uart_rx_irq_disable();
        return;
    }

    // Disable read interrupt
    uart_rx_irq_disable();

    // Store data in buffer
    while ((uart_rx_read + 1) % VSPRINT_MAX_BUF_SIZE != uart_rx_write) {
        uart_rx_buffer[uart_rx_write++] = uart_recv(); // Save data in buffer
        uart_rx_write %= VSPRINT_MAX_BUF_SIZE;
    }
    // end critical (rx irq will be enable again in next 'async getc' call)
}

void uart_async_putc(char c) {
    // hold when buffer full
    while (((uart_tx_write + 1) % VSPRINT_MAX_BUF_SIZE) == uart_tx_read) {};
    uart_tx_irq_disable(); // disable write interrupt
    uart_tx_buffer[uart_tx_write++] = c;
    uart_tx_write %= VSPRINT_MAX_BUF_SIZE;
    uart_tx_irq_enable(); // enable write interrupt
}

char uart_async_getc() {
    // empty
    uart_rx_irq_enable();
    while (uart_rx_read == uart_rx_write) {};
    // delay here?
    uart_rx_irq_disable(); // disable read interrupt
    char c = uart_rx_buffer[uart_rx_read++];
    uart_rx_read %= VSPRINT_MAX_BUF_SIZE;
    return c;
}

void uart_rx_irq_enable() {
    *AUX_MU_IER_REG |= 1; // enable read interrupt
}

void uart_rx_irq_disable() {
    *AUX_MU_IER_REG &= ~(1); // disable read interrupt
}

void uart_tx_irq_enable() {
    *AUX_MU_IER_REG |= 2; // enable write interrupt
}

void uart_tx_irq_disable() {
    *AUX_MU_IER_REG &= ~(2); // disable write interrupt
}