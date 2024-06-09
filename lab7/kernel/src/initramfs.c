#include "initramfs.h"
#include "cpio.h"
#include "memory.h"
#include "stddef.h"
#include "u_string.h"
#include "uart1.h"
#include "vfs.h"

extern void *CPIO_DEFAULT_START; // root of ramfs
extern void *CPIO_DEFAULT_END;   // end addressl of ramfs

file_operations initramfs_file_operations = {
    initramfs_write,   //
    initramfs_read,    //
    initramfs_open,    //
    initramfs_close,   //
    initramfs_lseek64, //
    initramfs_getsize  //
};
vnode_operations initramfs_vnode_operations = {
    initramfs_lookup, //
    initramfs_create, //
    initramfs_mkdir   //
};

int register_initramfs() {
    filesystem fs;
    fs.name = "initramfs";                  // init name
    fs.setup_mount = initramfs_setup_mount; // init func ptr
    return register_filesystem(&fs);
}

int initramfs_setup_mount(filesystem *fs, mount *mnt) {
    mnt->fs = fs;
    mnt->root = initramfs_create_vnode(0, dir_t);

    // create entry under mnt, attach cpio files on it
    initramfs_inode *ramfs_inode = mnt->root->internal;

    // add all file in initramfs to filesystem
    char             *filepath, *filedata;
    unsigned int      filesize;
    cpio_newc_header *header_ptr = CPIO_DEFAULT_START;
    int               idx = 0;

    while (header_ptr != 0) {
        int error = cpio_newc_parse_header(header_ptr, &filepath, &filesize, &filedata, &header_ptr);
        if (error) {
            uart_sendline("[initramfs setup mount] error\n");
            break;
        }

        // if not TRAILER!!!
        if (header_ptr != 0) {
            vnode           *file_vnode = initramfs_create_vnode(0, file_t);
            initramfs_inode *file_inode = file_vnode->internal; // record content in inode
            file_inode->data = filedata;
            file_inode->datasize = filesize;
            file_inode->name = filepath;
            ramfs_inode->entry[idx++] = file_vnode; // add vnode under dir
        }
    }
    return 0;
}

/* -------------------------------------------------------------- */
/*                        file operations                         */
/* -------------------------------------------------------------- */
int initramfs_write(file *file, const void *buf, size_t len) {
    return -1; // read only
}

// return number of byte being read
int initramfs_read(file *file, void *buf, size_t len) {
    initramfs_inode *inode = file->vnode->internal;

    // if overflow, shrink size // BUG: diff with sample
    if (len + file->f_pos > inode->datasize) len = inode->datasize - file->f_pos;

    memcpy(buf, inode->data, len);
    file->f_pos += len;
    return len;
}

int initramfs_open(vnode *file_node, file **target) {
    (*target)->vnode = file_node;
    (*target)->f_ops = file_node->f_ops;
    (*target)->f_pos = 0;
    return 0;
}

int initramfs_close(file *file) {
    kfree(file);
    return 0;
}

long initramfs_lseek64(file *file, long offset, int whence) {
    return 0;
}

long initramfs_getsize(vnode *vn) {
    return 0;
}

/* -------------------------------------------------------------- */
/*                        vnode operations                        */
/* -------------------------------------------------------------- */
int initramfs_lookup(vnode *dir_node, vnode **target, const char *component_name) {
    initramfs_inode *dir_inode = dir_node->internal;
    int              child_idx = 0;
    for (; child_idx < INITRAMFS_MAX_DIR_ENTRY; child_idx++) {
        vnode *vn = dir_inode->entry[child_idx];
        if (!vn) break;
        initramfs_inode *inode = vn->internal;
        if (strcmp(component_name, inode->name) == 0) {
            *target = vn;
            return 0;
        }
    }
    return -1;
}

int initramfs_create(vnode *dir_node, vnode **target, const char *component_name) {
    return -1; // read only
}

int initramfs_mkdir(vnode *dir_node, vnode **target, const char *component_name) {
    return -1; // read only
}

vnode *initramfs_create_vnode(mount *mnt, fsnode_type type) {
    vnode *v = kmalloc(sizeof(vnode));
    v->f_ops = &initramfs_file_operations;
    v->v_ops = &initramfs_vnode_operations;
    v->mount = mnt;

    initramfs_inode *inode = kmalloc(sizeof(initramfs_inode));
    memset(inode, 0, sizeof(initramfs_inode));
    inode->type = type;
    inode->data = kmalloc(0x1000);

    v->internal = inode;
    return v;
}