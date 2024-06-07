// #include "dev_framebuffer.h"
// #include "exception.h"
// #include "mbox.h"
// #include "u_string.h"
// #include "uart1.h"

// #define MBOX_CH_PROP 8

// // lab7: mailbox init
// size_t width, height, pitch, isrgb; // dimensions & channel order
// unsigned char *lfb;                 // raw frame buffer addess

// file_operations dev_framebuffer_operation = {
//     dev_framebuffer_write,           //
//     (void *)dev_framebuffer_op_deny, //
//     dev_framebuffer_open,            //
//     dev_framebuffer_close,           //
//     dev_framebuffer_lseek64,         //
//     dev_framebuffer_op_deny          //
// };

// int init_dev_framebuffer() {
//     //
// }

// int dev_framebuffer_write(file *f, const void *buf, size_t len);
// int dev_framebuffer_read(file *f, void *buf, size_t len);
// int dev_framebuffer_open(vnode *file_node, file **target);
// int dev_framebuffer_close(file *f);
// long dev_framebuffer_lseek64(file *f, long offset, int whence);
// int dev_framebuffer_op_deny();