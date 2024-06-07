#ifndef _INITRAMFS_H_
#define _INITFAMFS_H_

#include "stddef.h"
#include "vfs.h"

#define INITRAMFS_MAX_DIR_ENTRY 100

typedef struct initramfs_inode {
    fsnode_type type;
    char *name;
    vnode *entry[INITRAMFS_MAX_DIR_ENTRY];
    char *data;
    size_t datasize;
} initramfs_inode;

int register_initramfs();
int initramfs_setup_mount(filesystem *fs, mount *mnt);

int initramfs_write(file *file, const void *buf, size_t len);
int initramfs_read(file *file, void *buf, size_t len);
int initramfs_open(vnode *file_node, file **target);
int initramfs_close(file *file);
long initramfs_lseek64(file *file, long offset, int whence);
long initramfs_getsize(vnode *vn);

int initramfs_lookup(vnode *dir_node, vnode **target, const char *component_name);
int initramfs_create(vnode *dir_node, vnode **target, const char *component_name);
int initramfs_mkdir(vnode *dir_node, vnode **target, const char *component_name);

vnode *initramfs_create_vnode(mount *mnt, fsnode_type type);

#endif /* _INITRAMFS_H_ */