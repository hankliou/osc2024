#ifndef _TMPFS_H_
#define _TMPFS_H_

#include "stddef.h"
#include "vfs.h"

// For tmpfs, you can assume that component name won’t excced 15 characters,
// and at most 16 entries for a directory. and at most 4096 bytes for a file.
#define MAX_FILE_NAME 15
#define MAX_DIR_ENTRY 16
#define MAX_FILE_SIZE 4096

typedef struct tmpfs_inode {
    fsnode_type type;
    char        name[MAX_FILE_NAME];
    vnode      *entry[MAX_DIR_ENTRY];
    char       *data;
    size_t      datasize;
} tmpfs_inode;

int register_tmpfs();
int tmpfs_setup_mount(filesystem *fs, mount *mnt);

int  tmpfs_write(file *file, const void *buf, size_t len);
int  tmpfs_read(file *file, void *buf, size_t len);
int  tmpfs_open(vnode *file_node, file **target);
int  tmpfs_close(file *file);
long tmpfs_lseek64(file *file, long offset, int whence);
long tmpfs_getsize(vnode *vn);

int tmpfs_lookup(vnode *dir_node, vnode **target, const char *component_name);
int tmpfs_create(vnode *dir_node, vnode **target, const char *component_name);
int tmpfs_mkdir(vnode *dir_node, vnode **target, const char *component_name);
int tmpfs_sync(filesystem *fs);

vnode *tmpfs_create_vnode(fsnode_type type);

#endif