#include "tmpfs.h"
#include "memory.h"
#include "u_string.h"
#include "uart1.h"
#include "vfs.h"

file_operations tmpfs_file_operations = {
    tmpfs_write,   //
    tmpfs_read,    //
    tmpfs_open,    //
    tmpfs_close,   //
    tmpfs_lseek64, //
    tmpfs_getsize  //
};

vnode_operations tmpfs_vnode_operations = {
    tmpfs_lookup, //
    tmpfs_create, //
    tmpfs_mkdir   //
};

int register_tmpfs() {
    filesystem fs;
    fs.name = "tmpfs";
    fs.setup_mount = tmpfs_setup_mount; // point to customized mount func
    return register_filesystem(&fs);
}

int tmpfs_setup_mount(filesystem *fs, mount *mnt) {
    mnt->fs = fs;
    mnt->root = tmpfs_create_vnode(0, dir_t);
    return 0;
}

/* -------------------------------------------------------------- */
/*                        file operations                         */
/* -------------------------------------------------------------- */
int tmpfs_write(file *file, const void *buf, size_t len) {
    tmpfs_inode *inode = file->vnode->internal;
    // write from f_pos
    memcpy(inode->data + file->f_pos, buf, len);
    // update f_pos and size
    file->f_pos += len;
    if (inode->datasize < file->f_pos) inode->datasize = file->f_pos;
    return len;
}

int tmpfs_read(file *file, void *buf, size_t len) {
    tmpfs_inode *inode = file->vnode->internal;
    // if buffer overflow, shrink the request read length
    if (len + file->f_pos > inode->datasize) len = inode->datasize - file->f_pos;

    // read from f_pos
    memcpy(buf, inode->data + file->f_pos, len);
    file->f_pos += inode->datasize - file->f_pos;
    return len;
}

// open a file, initailze everything
int tmpfs_open(vnode *file_node, file **target) {
    (*target)->vnode = file_node;
    (*target)->f_ops = file_node->f_ops;
    (*target)->f_pos = 0;
    return 0;
}

int tmpfs_close(file *file) {
    kfree(file);
    return 0;
}

// long seek 64-bit // TODO 搞清楚這FUNC再幹嘛(用不到?)
long tmpfs_lseek64(file *file, long offset, int whence) {
    if (whence == SEEK_SET) {
        file->f_pos = offset;
        return file->f_pos;
    }
    return -1;
}

long tmpfs_getsize(vnode *vn) {
    tmpfs_inode *inode = vn->internal;
    return inode->datasize;
}

/* -------------------------------------------------------------- */
/*                        vnode operations                        */
/* -------------------------------------------------------------- */
// examine if is a file named 'component_name' under 'dir_node', 0:success, !!target will be move on next node!!
int tmpfs_lookup(vnode *dir_node, vnode **target, const char *component_name) {
    tmpfs_inode *dir_inode = dir_node->internal;
    int          child_idx = 0;

    // BFS search tree
    for (; child_idx <= MAX_DIR_ENTRY; child_idx++) {
        vnode *vnode_it = dir_inode->entry[child_idx]; // entry stores all sub folder/file
        if (!vnode_it) break;
        tmpfs_inode *inode_it = vnode_it->internal;
        if (strcmp(component_name, inode_it->name) == 0) {
            *target = vnode_it;
            return 0;
        }
    }
    return -1;
}

int tmpfs_create(vnode *dir_node, vnode **target, const char *component_name) {
    tmpfs_inode *inode = dir_node->internal;
    if (inode->type != dir_t) {
        uart_sendline("[tmpfs create] not dir\n");
        return -1;
    }

    int child_idx = 0;
    for (; child_idx <= MAX_DIR_ENTRY; child_idx++) {
        if (!inode->entry[child_idx]) break; // entry stores all sub folder/file
        tmpfs_inode *child_inode = inode->entry[child_idx]->internal;
        if (strcmp(child_inode->name, component_name) == 0) {
            uart_sendline("[tmpfs create] file exists\n");
            return -1;
        }
    }

    if (child_idx > MAX_DIR_ENTRY) {
        uart_sendline("[tmpfs create] dir entry full\n");
        return -1;
    }

    if (strlen(component_name) > MAX_FILE_NAME) {
        uart_sendline("[tmpfs create] too long file name\n");
        return -1;
    }

    vnode *vn = tmpfs_create_vnode(0, file_t);
    inode->entry[child_idx] = vn;

    tmpfs_inode *new_inode = vn->internal;
    strcpy(new_inode->name, component_name);

    *target = vn;
    return 0;
}

int tmpfs_mkdir(vnode *dir_node, vnode **target, const char *component_name) {
    tmpfs_inode *inode = dir_node->internal;
    if (inode->type != dir_t) {
        uart_sendline("[tmpfs mkdir] not a directory\n");
        return -1;
    }

    int child_idx = 0;
    for (; child_idx < MAX_DIR_ENTRY; child_idx++) {
        if (!inode->entry[child_idx]) break;
    }

    if (child_idx > MAX_DIR_ENTRY) {
        uart_sendline("[tmpfs mkdir] dir entry full\n");
        return -1;
    }

    if (strlen(component_name) > MAX_FILE_NAME) {
        uart_sendline("[tmpfs mkdir] too long file name\n");
        return -1;
    }

    vnode *vn = tmpfs_create_vnode(0, dir_t);
    inode->entry[child_idx] = vn;

    tmpfs_inode *new_inode = vn->internal;
    strcpy(new_inode->name, component_name);

    *target = vn;
    return 0;
}

// TODO mnt沒用到，要移除
vnode *tmpfs_create_vnode(mount *mnt, fsnode_type type) {
    vnode *v = kmalloc(sizeof(vnode));
    v->f_ops = &tmpfs_file_operations;
    v->v_ops = &tmpfs_vnode_operations;
    v->mount = 0;

    tmpfs_inode *inode = kmalloc(sizeof(tmpfs_inode));
    memset(inode, 0, sizeof(tmpfs_inode));
    inode->type = type;
    inode->data = kmalloc(4096);
    v->internal = inode;

    return v;
}