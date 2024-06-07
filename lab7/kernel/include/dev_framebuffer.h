// #ifndef _DEV_FRAMEBUFFER_H_
// #define _DEV_FRAMEBUFFER_H_

// #include "stddef.h"
// #include "vfs.h"

// typedef struct framebuffer_info {
//     size_t width;
//     size_t height;
//     size_t pitch;
//     size_t isrgb;
// } framebuffer_info;

// int init_dev_framebuffer();

// int dev_framebuffer_write(file *f, const void *buf, size_t len);
// int dev_framebuffer_read(file *f, void *buf, size_t len);
// int dev_framebuffer_open(vnode *file_node, file **target);
// int dev_framebuffer_close(file *f);
// long dev_framebuffer_lseek64(file *f, long offset, int whence);
// int dev_framebuffer_op_deny();

// #endif /*_DEV_FRAMEBUFFER_H_*/