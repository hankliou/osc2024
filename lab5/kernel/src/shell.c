#include "shell.h"
#include "cpio.h"
#include "dtb.h"
#include "mbox.h"
#include "memory.h"
#include "power.h"
#include "thread.h"
#include "timer.h"
#include "u_string.h"
#include "uart1.h"

extern char *dtb_ptr;
void *CPIO_DEFAULT_START; // root of ramfs
void *CPIO_DEFAULT_END;   // end addressl of ramfs

struct CLI_CMDS cmd_list[CLI_MAX_CMD] = {
    {.command = "cat", .help = "see the file content"},
    {.command = "dtb", .help = "show device tree"},
    {.command = "exec", .help = "load usr proc"},
    {.command = "hello", .help = "print Hello World!"},
    {.command = "help", .help = "print all available commands"},
    {.command = "info", .help = "get device information via mailbox"},
    {.command = "ls", .help = "list directory contents"},
    {.command = "simple_malloc", .help = "simple allocator in heap session"},
    {.command = "reboot", .help = "reboot the device"},
    {.command = "setTimer", .help = "setTimer [msg] [second]"},
    {.command = "set2sAlert", .help = "set a 2s timer"},
    {.command = "testAsyncUart", .help = "will echo your input by async UART"},
    {.command = "mem_test", .help = "lazy testing kmalloc and kfree"},
    {.command = "thread_test", .help = "test threads interleaving"},
};

void do_cmd_exec(char *filepath) {
    char *c_filepath;
    char *c_filedata;
    unsigned int c_filesize;
    struct cpio_newc_header *header_ptr = CPIO_DEFAULT_START;

    // traverse the whole ramdisk, check filename one by one
    while (header_ptr != 0) {
        // function return -1 when error
        int error = cpio_newc_parse_header(header_ptr, &c_filepath, &c_filesize, &c_filedata, &header_ptr);
        if (error) {
            uart_sendline("cpio parse error");
            break;
        }

        // if match
        if (strcmp(c_filepath, filepath) == 0) {
            // exec c_filedata
            char *ustack = simple_malloc(256);
            asm volatile("msr spsr_el1, %0" ::"r"(0x3c0));       // set state to user mode, and enable interrupt
            asm volatile("msr elr_el1, %0;" ::"r"(c_filedata));  // set exception return addr to 'c_filedata'(any
                                                                 // addr may be ok)
            asm volatile("msr sp_el0, %0;" ::"r"(ustack + 256)); // set el0's sp to top of new stack
            asm volatile("eret;");                               // switch EL to 0

            break;
        }

        // if meet TRAILER!!! (last file)
        if (header_ptr == 0)
            uart_sendline("cat: %s: No such file or directory.\n", filepath);
    }
}

int cli_cmd_strcmp(const char *p1, const char *p2) {
    const unsigned char *s1 = (const unsigned char *)p1;
    const unsigned char *s2 = (const unsigned char *)p2;
    unsigned char c1, c2;

    do {
        c1 = (unsigned char)*s1++;
        c2 = (unsigned char)*s2++;
        if (c1 == '\0')
            return c1 - c2;
    } while (c1 == c2);
    return c1 - c2;
}

void cli_cmd_clear(char *buffer, int length) {
    for (int i = 0; i < length; i++) {
        buffer[i] = '\0';
    }
};

void cli_cmd_read(char *buffer) {
    char c = '\0';
    int idx = 0;
    while (1) {
        if (idx >= CMD_MAX_LEN)
            break;

        c = uart_async_getc();
        // c = uart_recv();
        if (c == '\n') {
            uart_sendline("\r\n");
            break;
        }
        if (c > 16 && c < 32)
            continue;
        if (c > 127)
            continue;
        buffer[idx++] = c;
    }
}

void cli_cmd_exec(char *buffer) {
    if (!buffer)
        return;

    char *cmd = buffer;
    char *argvs; // get the first param after cmd

    while (1) {
        if (*buffer == '\0') {
            argvs = buffer;
            break;
        }
        if (*buffer == ' ') {
            *buffer = '\0';
            argvs = buffer + 1;
            break;
        }
        buffer++;
    }

    if (strcmp(cmd, "cat") == 0) {
        do_cmd_cat(argvs);
    } else if (strcmp(cmd, "dtb") == 0) {
        do_cmd_dtb();
    } else if (strcmp(cmd, "hello") == 0) {
        do_cmd_hello();
    } else if (strcmp(cmd, "help") == 0) {
        do_cmd_help();
    } else if (strcmp(cmd, "info") == 0) {
        do_cmd_info();
    } else if (strcmp(cmd, "simple_malloc") == 0) {
        do_cmd_simple_malloc();
    } else if (strcmp(cmd, "ls") == 0) {
        do_cmd_ls(argvs);
    } else if (strcmp(cmd, "exec") == 0) {
        do_cmd_exec(argvs);
    } else if (strcmp(cmd, "reboot") == 0) {
        do_cmd_reboot();
    } else if (strcmp(cmd, "set2sAlert") == 0) {
        do_cmd_set2sTimer("2s time's up");
    } else if (strcmp(cmd, "setTimer") == 0) {
        char *sec = str_SepbySpace(argvs);
        do_cmd_setTimer(argvs, atoi(sec));
    } else if (strcmp(cmd, "testAsyncUart") == 0) {
        do_cmd_testAsyncUart();
    } else if (strcmp(cmd, "mem_test") == 0) {
        do_cmd_mem_test();
    } else if (strcmp(cmd, "thread_test") == 0) {
        do_cmd_thread_test();
    } else {
        uart_sendline("%s : command not found\n", cmd);
    }
}

void do_cmd_testAsyncUart() {
    while (1) {
        uart_sendline("type 'q' to quit\n");
        char buffer[CMD_MAX_LEN];
        int idx = 0;
        while (idx < CMD_MAX_LEN) {
            char c = uart_async_getc();
            buffer[idx++] = c;
            if (c == '\n')
                break;
        }
        if (buffer[0] == 'q' && buffer[1] == '\n')
            break;
        for (int i = 0; i < idx; i++) {
            uart_async_putc(buffer[i]);
        }
        uart_async_putc('\r');
        uart_async_putc('\n');
    }
}

void cli_print_banner() {
    uart_sendline("=======================================\r\n");
    uart_sendline("  Welcome to NYCU-OSC 2024 Lab5 Shell  \r\n");
    uart_sendline("=======================================\r\n");
}

void do_cmd_cat(char *filepath) {
    char *c_filepath;
    char *c_filedata;
    unsigned int c_filesize;
    struct cpio_newc_header *header_ptr = CPIO_DEFAULT_START;

    // traverse the whole ramdisk, check filename one by one
    while (header_ptr != 0) {
        // func return -1 when error
        int error = cpio_newc_parse_header(header_ptr, &c_filepath, &c_filesize, &c_filedata, &header_ptr);
        if (error) {
            uart_sendline("cpio parse error");
            break;
        }

        if (strcmp(c_filepath, filepath) == 0) {
            uart_sendline("%s", c_filedata);
            break;
        }

        // if is TRAILER!!!
        if (header_ptr == 0) {
            uart_sendline("cat: %s: No such file or directory", filepath);
        }
    }
    uart_sendline("\n");
}

void do_cmd_dtb() {
    traverse_device_tree(dtb_ptr, dtb_callback_show_tree);
}

void do_cmd_help() {
    for (int i = 0; i < CLI_MAX_CMD; i++) {
        uart_sendline(cmd_list[i].command);
        uart_sendline("\t\t: ");
        uart_sendline(cmd_list[i].help);
        uart_sendline("\r\n");
    }
}

void do_cmd_hello() { // hello
    uart_sendline("Hello World!\r\n");
}

void do_cmd_info() {
    // print hw revision
    pt[0] = 8 * 4;
    pt[1] = MBOX_REQUEST_PROCESS;
    pt[2] = MBOX_TAG_GET_BOARD_REVISION;
    pt[3] = 4;
    pt[4] = MBOX_TAG_REQUEST_CODE;
    pt[5] = 0;
    pt[6] = 0;
    pt[7] = MBOX_TAG_LAST_BYTE;

    if (mbox_call(MBOX_TAGS_ARM_TO_VC, (unsigned int)((unsigned long)&pt))) {
        uart_sendline("Hardware Revision\t: ");
        uart_2hex(pt[6]);
        uart_2hex(pt[5]);
        uart_sendline("\r\n");
    }
    // print arm memory
    pt[0] = 8 * 4;
    pt[1] = MBOX_REQUEST_PROCESS;
    pt[2] = MBOX_TAG_GET_ARM_MEMORY;
    pt[3] = 8;
    pt[4] = MBOX_TAG_REQUEST_CODE;
    pt[5] = 0;
    pt[6] = 0;
    pt[7] = MBOX_TAG_LAST_BYTE;

    if (mbox_call(MBOX_TAGS_ARM_TO_VC, (unsigned int)((unsigned long)&pt))) {
        uart_sendline("ARM Memory Base Address\t: ");
        uart_2hex(pt[5]);
        uart_sendline("\r\n");
        uart_sendline("ARM Memory Size\t\t: ");
        uart_2hex(pt[6]);
        uart_sendline("\r\n");
    }
}

void do_cmd_simple_malloc() {
    char *test1 = simple_malloc(0x18);
    memcpy(test1, "test malloc1", sizeof("test amlloc1"));
    uart_sendline("%s\n", test1);

    char *test2 = simple_malloc(0x20);
    memcpy(test2, "test malloc2", sizeof("test amlloc2"));
    uart_sendline("%s\n", test2);

    char *test3 = simple_malloc(0x28);
    memcpy(test3, "test malloc3", sizeof("test amlloc3"));
    uart_sendline("%s\n", test3);
}

void do_cmd_ls(char *dir) {
    char *c_filepath;
    char *c_filedata;
    unsigned int c_filesize;
    struct cpio_newc_header *header_ptr = CPIO_DEFAULT_START;

    while (header_ptr != 0) {
        int error = cpio_newc_parse_header(header_ptr, &c_filepath, &c_filesize, &c_filedata, &header_ptr);
        if (error) {
            uart_sendline("cpio parse error");
            break;
        }

        if (header_ptr != 0) {
            uart_sendline("%s\n", c_filepath);
        }
    }
}

void do_cmd_reboot() {
    uart_sendline("Reboot in 5 seconds ...\r\n\r\n");
    volatile unsigned int *rst_addr = (unsigned int *)PM_RSTC;
    *rst_addr = PM_PASSWORD | 0x20;
    volatile unsigned int *wdg_addr = (unsigned int *)PM_WDOG;
    *wdg_addr = PM_PASSWORD | 5;
}

void do_cmd_set2sTimer(char *msg) {
    // get '1 sec' length
    long long int cntpct_el0, cntfrq_el0;
    asm volatile("mrs %0, cntpct_el0\n\t" : "=r"(cntpct_el0));
    asm volatile("mrs %0, cntfrq_el0\n\t" : "=r"(cntfrq_el0));
    uart_sendline("[Interrupt][el1_irq][%s] %d seconds after booting\n", msg, cntpct_el0 / cntfrq_el0);

    add_timer(do_cmd_set2sTimer, msg, 2);
}

void do_cmd_setTimer(char *msg, int sec) {
    add_timer(timer_print_msg, msg, sec);
}

void do_cmd_mem_test() {
    char *p1 = kmalloc(0x820);
    char *p2 = kmalloc(0x900);
    char *p3 = kmalloc(0x2000);
    char *p4 = kmalloc(0x3900);
    kfree(p3);
    kfree(p4);
    kfree(p1);
    kfree(p2);
    char *a = kmalloc(0x10);
    char *b = kmalloc(0x100);
    char *c = kmalloc(0x1000);

    kfree(a);
    kfree(b);
    kfree(c);

    a = kmalloc(32);
    char *aa = kmalloc(50);
    b = kmalloc(64);
    char *bb = kmalloc(64);
    c = kmalloc(128);
    char *cc = kmalloc(129);
    char *d = kmalloc(256);
    char *dd = kmalloc(256);
    char *e = kmalloc(512);
    char *ee = kmalloc(999);

    char *f = kmalloc(0x2000);
    char *ff = kmalloc(0x2000);
    char *g = kmalloc(0x2000);
    char *gg = kmalloc(0x2000);
    char *h = kmalloc(0x2000);
    char *hh = kmalloc(0x2000);

    kfree(a);
    kfree(aa);
    kfree(b);
    kfree(bb);
    kfree(c);
    kfree(cc);
    kfree(dd);
    kfree(d);
    kfree(e);
    kfree(ee);

    kfree(f);
    kfree(ff);
    kfree(g);
    kfree(gg);
    kfree(h);
    kfree(hh);
}

void do_cmd_thread_test() {
    for (int i = 0; i < 5; i++) {
        uart_sendline("testing: %d\n", thread_create(foo)->pid);
    }
    idle();
}