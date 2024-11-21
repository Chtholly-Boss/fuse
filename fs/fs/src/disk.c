#include "../include/fs.h"

#define DISK_ROUND_DOWN(off) ROUND_DOWN(off, (super.params.size_io)) // 511 is the block size
#define DISK_ROUND_UP(off) ROUND_UP(off, (super.params.size_io)) // 511 is the block size

extern struct fs_super super;
extern struct custom_options fs_options;			 /* 全局选项 */

/**
 * @brief Read data from disk
 */
int disk_read(int offset, void *out_content, int size) {
    int io_size = super.params.size_io;

    int offset_rounded = DISK_ROUND_DOWN(offset);
    int size_rounded = DISK_ROUND_UP(size);

    uint8_t *buffer = (uint8_t*) malloc(size_rounded);
    uint8_t *cur = buffer;

    ddriver_seek(super.fd, offset_rounded, SEEK_SET);

    while (size_rounded > 0) {
        ddriver_read(super.fd, cur, io_size);
        size_rounded -= io_size;
        cur += io_size;
    }

    int bias = offset - offset_rounded;
    memcpy(out_content, buffer + bias, size);
    free(buffer);
    return 0;
}

/**
 * @brief Write data to disk
 */
int disk_write(int offset, void *in_content, int size) {
    int io_size = super.params.size_io;

    int offset_rounded = DISK_ROUND_DOWN(offset);
    int size_rounded = DISK_ROUND_UP(size);

    uint8_t *buffer = (uint8_t*) malloc(size_rounded);
    // ! to avoid overwriting, we need to read the data first
    disk_read(offset_rounded, buffer, size_rounded);

    int bias = offset - offset_rounded;
    memcpy(buffer + bias, in_content, size);

    ddriver_seek(super.fd, offset_rounded, SEEK_SET);
    uint8_t *cur = buffer;

    while (size_rounded > 0) {
        ddriver_write(super.fd, cur, io_size);
        size_rounded -= io_size;
        cur += io_size;
    }

    free(buffer);
    return 0;
}

/**
 * @brief Construct in-mem super correctly
 */
int disk_mount() {
    // root for path resolution
    // because root is not a subdirectory of anything, we should create a dentry every time we mount
    struct fs_dentry *root = dentry_create("/", FT_DIR);
    super.root = root;

    super.fd = ddriver_open(fs_options.device);

    ddriver_ioctl(super.fd, IOC_REQ_DEVICE_IO_SZ, &super.params.size_io);
    ddriver_ioctl(super.fd, IOC_REQ_DEVICE_SIZE, &super.params.size_disk);
    super.params.size_block = super.params.size_io * 2;

    struct fs_super_d super_d;
    disk_read(0, &super_d, sizeof(struct fs_super_d));

    int is_init = (super_d.magic != FS_MAGIC);
    if (is_init) {
        // Initialize the disk
        super_d.magic = FS_MAGIC;

        super_d.param.size_io = super.params.size_io;
        super_d.param.size_disk = super.params.size_disk;
        super_d.param.size_block = super.params.size_block;
        super_d.param.size_usage = 0; // TODO
        super_d.param.max_ino = 1000; // TODO
        super_d.param.max_dno = 4000; // TODO
        
        super_d.super.offset = 0;
        super_d.super.blocks = 1;

        super_d.imap.offset = super_d.super.offset + super_d.super.blocks * super_d.param.size_block;
        super_d.imap.blocks = 1;

        super_d.dmap.offset = super_d.imap.offset + super_d.imap.blocks * super_d.param.size_block;
        super_d.dmap.blocks = 1;

        super_d.inodes.offset = super_d.dmap.offset + super_d.dmap.blocks * super_d.param.size_block;
        super_d.inodes.blocks = 64; // TODO
        
        super_d.data.offset = super_d.inodes.offset + super_d.inodes.blocks * super_d.param.size_block;
        super_d.data.blocks = 4029; // TODO
    }

    memcpy(&super.params, &super_d.param, sizeof(DiskParam));
    super.super_off = super_d.super.offset;
    super.imap_off = super_d.imap.offset;
    super.dmap_off = super_d.dmap.offset;
    super.inodes_off = super_d.inodes.offset;
    super.data_off = super_d.data.offset;

    if (is_init) {
        super.imap = bitmap_init(super.params.size_block * super_d.imap.blocks);
        super.dmap = bitmap_init(super.params.size_block * super_d.dmap.blocks);
        disk_write(super.imap_off, super.imap, super.params.size_block * super_d.imap.blocks);
        disk_write(super.dmap_off, super.dmap, super.params.size_block * super_d.dmap.blocks);
        free(super.imap);
        free(super.dmap);
    }
    super.imap = bitmap_init(super.params.size_block * super_d.imap.blocks);
    super.dmap = bitmap_init(super.params.size_block * super_d.dmap.blocks);
    disk_read(super.imap_off, super.imap, super.params.size_block * super_d.imap.blocks);
    disk_read(super.dmap_off, super.dmap, super.params.size_block * super_d.dmap.blocks);

    if (is_init) {
        // ! allocate ino 0 to root
        struct fs_inode *root_inode = inode_create();
        // for now, just bind
        dentry_bind(root, root_inode);
        // inode_sync(root_inode);
    }

    // dentry_restore(root, 0);

    return ERROR_NONE;
}

int disk_umount() {
    inode_sync(super.root->self);

    struct fs_super_d super_d;
    memcpy(&super_d.param, &super.params, sizeof(DiskParam));
    super_d.magic = FS_MAGIC;
    super_d.super.offset = super.super_off;
    super_d.super.blocks = 1;
    super_d.imap.offset = super.imap_off;
    super_d.imap.blocks = 1;
    super_d.dmap.offset = super.dmap_off;
    super_d.dmap.blocks = 1;
    super_d.inodes.offset = super.inodes_off;
    super_d.inodes.blocks = 64; 
    super_d.data.offset = super.data_off;
    super_d.data.blocks = 4029; 

    disk_write(0, &super_d, sizeof(struct fs_super_d));

    // Write Bitmap
    disk_write(super.imap_off, super.imap, super.params.size_block * super_d.imap.blocks);
    disk_write(super.dmap_off, super.dmap, super.params.size_block * super_d.dmap.blocks);

    free(super.imap);
    free(super.dmap);

    ddriver_close(super.fd);
    return ERROR_NONE;
}