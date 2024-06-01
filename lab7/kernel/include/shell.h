#ifndef _SHELL_H_
#define _SHELL_H_

#define CLI_MAX_CMD 15
#define CMD_MAX_LEN 128
#define MSG_MAX_LEN 128

typedef struct CLI_CMDS {
    char command[CMD_MAX_LEN];
    char help[MSG_MAX_LEN];
} CLI_CMDS;

int cli_cmd_strcmp(const char *, const char *);
void cli_cmd_clear(char *, int);
void cli_cmd_read(char *);
void cli_cmd_exec(char *);
void cli_print_banner();

void do_cmd_cat(char *);
void do_cmd_dtb();
void do_cmd_help();
void do_cmd_hello();
void do_cmd_info();
void do_cmd_ls(char *);
void do_cmd_simple_malloc();
void do_cmd_reboot();
void do_cmd_set2sTimer(char *msg);
void do_cmd_setTimer(char *msg, int sec);
void do_cmd_testAsyncUart();
void do_cmd_mem_test();
void do_cmd_thread_test();
void do_cmd_syscall_test();

void fork_test();

#endif /* _SHELL_H_ */