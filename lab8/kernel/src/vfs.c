#include "vfs.h"
#include "dev_framebuffer.h"
#include "dev_uart.h"
#include "initramfs.h"
#include "memory.h"
#include "tmpfs.h"
#include "u_string.h"
#include "uart1.h"

mount          *rootfs;
filesystem      reg_fs[MAX_FS_REG];
file_operations reg_dev[MAX_DEV_REG];

void init_rootfs() {
    // tmpfs
    int idx = register_tmpfs();
    rootfs = kmalloc(sizeof(mount));
    reg_fs[idx].setup_mount(&reg_fs[idx], rootfs);

    // initramfs
    vfs_mkdir("/initramfs");
    idx = register_initramfs();
    vfs_mount("/initramfs", "initramfs");

    // dev fs
    vfs_mkdir("/dev");
    int uart_id = init_dev_uart();
    vfs_mknod("/dev/uart", uart_id);
    int framebuffer_id = init_dev_framebuffer();
    vfs_mknod("/dev/framebuffer", framebuffer_id);
}

// copy param 'fs' into table
int register_filesystem(filesystem *fs) {
    for (int i = 0; i < MAX_FS_REG; i++) {
        if (!reg_fs[i].name) {
            reg_fs[i].name = fs->name;               // init name
            reg_fs[i].setup_mount = fs->setup_mount; // init func ptr
            return i;
        }
    }
    return -1;
}

// copy param 'fo' into table
int register_dev(file_operations *fo) {
    for (int i = 0; i < MAX_FS_REG; i++) {
        if (!reg_dev[i].open) {
            // return unique id for the assigned device
            reg_dev[i] = *fo;
            return i;
        }
    }
    return -1;
}

int vfs_mknod(char *pathname, int id) {
    file *f;
    // create leaf and its file opeations
    vfs_open(pathname, O_CREAT, &f);
    f->vnode->f_ops = &reg_dev[id];
    vfs_close(f); // free the struct 'file', but struct 'vnode' still exist
    return 0;
}

filesystem *find_filesystem(const char *fs_name) {
    for (int i = 0; i < MAX_FS_REG; i++) {
        if (strcmp(reg_fs[i].name, fs_name) == 0) { return &reg_fs[i]; }
    }
    return 0;
}

/* -------------------------------------------------------------- */
/*                        file operations                         */
/* -------------------------------------------------------------- */

// write len byte from buf to the opened file, return written size or error code
int vfs_write(file *file, const void *buf, size_t len) {
    return file->f_ops->write(file, buf, len);
}

// read min(len, readable_size) byte to buf from opened file, block if nothing to read for FIFO type, and return read size
int vfs_read(file *file, void *buf, size_t len) {
    return file->f_ops->read(file, buf, len);
}

// success: return 0, failed: return -1
int vfs_open(const char *pathname, int flags, file **target) {
    // 1. Lookup pathname (create new if vnode not found and 'O_CREAT')
    vnode *node;
    if (vfs_lookup(pathname, &node) != 0 && (flags & O_CREAT)) {
        // get all directory path
        int last_slash_idx = 0;
        for (int i = 0; i < strlen(pathname); i++) {
            if (pathname[i] == '/') last_slash_idx = i;
        }

        char dirname[MAX_PATH_NAME + 1];
        strcpy(dirname, pathname);
        dirname[last_slash_idx] = 0;

        // update dirname to node
        if (vfs_lookup(dirname, &node) != 0) {
            uart_sendline("[vfs open] Cannot o_create under non-exist dir\n");
            return -1;
        }

        // create a new file node on node, &node is new file, 3rd arg is filename
        node->v_ops->create(node, &node, pathname + last_slash_idx + 1);
    }
    // 2. Create a new file handler for this vnode if found
    // attach opened file on the node
    *target = kmalloc(sizeof(file));
    // attach opened file on the new node
    node->f_ops->open(node, target);
    (*target)->flags = flags;
    return 0;
}

// return 0 if success
int vfs_close(file *file) {
    file->f_ops->close(file); // release the file handler
    return 0;                 // return error code if fails
}

/* -------------------------------------------------------------- */
/*                        Multi-level VFS                         */
/* -------------------------------------------------------------- */

// return 0 if success, -1 if failed
int vfs_mkdir(const char *pathname) {
    char dirname[MAX_PATH_NAME] = {0};    // copy of pathname
    char newdirname[MAX_PATH_NAME] = {0}; // last split of pathname

    // search for last directory
    int last_slash_idx = 0;
    for (int i = 0; i < strlen(pathname); i++) {
        if (pathname[i] == '/') last_slash_idx = i;
    }
    memcpy(dirname, pathname, last_slash_idx);         // dirname = '/path/given/by/user'
    strcpy(newdirname, pathname + last_slash_idx + 1); // newdirname = 'user'

    // create new directory if upper directory is found
    vnode *node;
    if (vfs_lookup(dirname, &node) == 0) {
        // node is the old dir, &node is new dir
        node->v_ops->mkdir(node, &node, newdirname);
        return 0;
    }

    uart_sendline("[vfs mkdir] cannot find pathname.\n");
    return -1;
}

int vfs_mount(const char *target, const char *file_sys) {
    vnode *dirnode;
    // search for the target filesystem
    filesystem *fs = find_filesystem(file_sys);

    // if target fs not found (not registered)
    if (!fs) {
        uart_sendline("[vfs mount] cannot find filesystem\n");
        return -1;
    }

    // if mounting dir not found (path doesn't exist)
    if (vfs_lookup(target, &dirnode) == -1) {
        uart_sendline("[vfs mount] cannot find dir\n");
        return -1;
    }

    // mount fs on dirnode
    dirnode->mount = kmalloc(sizeof(mount));
    fs->setup_mount(fs, dirnode->mount);
    return 0;
}

// if found, target will point to vnode. if failed, return -1
int vfs_lookup(const char *pathname, vnode **target) {
    // if no path input, return root
    if (strlen(pathname) == 0) {
        *target = rootfs->root;
        return 0;
    }

    vnode *dirnode = rootfs->root;
    char   component_name[MAX_FILE_NAME + 1] = {0};
    int    c_idx = 0;

    // deal with directory, lookup every spilt of path('component_name' will represent a path split)
    for (int i = 1; i < strlen(pathname); i++) {
        if (pathname[i] == '/') {
            component_name[c_idx++] = 0;
            // if fs's v_ops error, return -1, !!! tmpfs_lookup will move dirnode to next node !!!
            if (dirnode->v_ops->lookup(dirnode, &dirnode, component_name) != 0) return -1; // BFS search subnode
            // redirect to mounted filesystem(if mount point exist, go to the mounted fs)
            while (dirnode->mount) dirnode = dirnode->mount->root;
            c_idx = 0;
        } else component_name[c_idx++] = pathname[i];
    }

    // deal with file
    component_name[c_idx++] = 0;
    // if fs's v_ops error, return -1
    if (dirnode->v_ops->lookup(dirnode, &dirnode, component_name) != 0) return -1;
    // redirect to mounted filesystem
    while (dirnode->mount) dirnode = dirnode->mount->root;

    // return file's vnode
    *target = dirnode;
    return 0;
}

// 'path' will be written the abs_path, and will be returned
char *get_absolute_path(char *path, char *curr_working_dir) {
    // if relative path -> add '/' at the beginning of 'path'
    if (path[0] != '/') {
        char tmp[MAX_PATH_NAME];
        strcpy(tmp, curr_working_dir);
        if (strcmp(curr_working_dir, "/") != 0) strcat(tmp, "/");
        strcat(tmp, path);
        strcpy(path, tmp);
    }

    char absolute_path[MAX_PATH_NAME + 1] = {0};
    int  idx = 0; // index for 'absolute_path'
    for (int i = 0; i < strlen(path); i++) {
        // meet '/..'
        if (path[i] == '/' && path[i + 1] == '.' && path[i + 2] == '.') {
            for (int j = idx; j >= 0; j--) {
                if (absolute_path[j] == '/') {
                    absolute_path[j] = 0;
                    idx = j;
                }
            }
            i += 2;
            continue;
        }

        // ignore '/.'
        if (path[i] == '/' && path[i + 1] == '.') {
            i++;
            continue;
        }

        absolute_path[idx++] = path[i];
    }
    absolute_path[idx] = 0;

    return strcpy(path, absolute_path);
}