#include "../include/fs.h"

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
 * @brief Sync inode to disk
 */
int inode_sync(struct fs_inode* inode)
{
    struct fs_inode_d inode_d;

    inode_d.ino = inode->ino;
    inode_d.dir_cnt = inode->dir_cnt;
    inode_d.dno_dir = inode->dno_dir;
    inode_d.size = inode->size;
    memcpy(inode_d.dno_reg, inode->dno_reg, sizeof(inode->dno_reg));
    // Write inode to disk
    disk_write(
        (super.inodes_off + inode->ino * sizeof(struct fs_inode_d)),
        &inode_d,
        sizeof(struct fs_inode_d)
    );
    if (inode->self->ftype == FT_DIR) {
        // Write dentry to disk
        struct fs_dentry* child = inode->childs;
        struct fs_dentry_d child_d;
        int offset = super.data_off + inode->dno_dir * super.params.size_block;
        while (child != NULL) {
            memcpy(child_d.name, child->name, MAX_NAME_LEN);
            child_d.ino = child->ino;
            child_d.ftype = child->ftype;

            disk_write(offset, &child_d, sizeof(struct fs_dentry_d));

            if (child->self != NULL) {
                inode_sync(child->self);
            }

            child = child->next;
            offset += sizeof(struct fs_dentry_d);
        }
    }
    return ERROR_NONE;
}

/**
 * @brief Restore dentry from disk
 */
int dentry_restore(struct fs_dentry* dentry, int ino)
{
    struct fs_inode_d inode_d;

    disk_read(
        (super.inodes_off + ino * sizeof(struct fs_inode_d)),
        &inode_d,
        sizeof(struct fs_inode_d)
    );

    struct fs_inode* inode = (struct fs_inode*)malloc(sizeof(struct fs_inode));
    inode->self = dentry;
    inode->ino = inode_d.ino;
    inode->dir_cnt = inode_d.dir_cnt;
    inode->childs = NULL;

    inode->size = inode_d.size;
    inode->dno_dir = inode_d.dno_dir;
    memcpy(inode->dno_reg, inode_d.dno_reg, sizeof(inode->dno_reg));

    dentry->self = inode;
    dentry->ino = inode_d.ino;
    
    if (dentry->ftype == FT_DIR) {
        int offset = super.data_off + inode->dno_dir * super.params.size_block;

        struct fs_dentry_d child_d;
        struct fs_dentry* child;

        for (int i = 0; i < inode->dir_cnt; i++) {
            disk_read(offset + i * sizeof(struct fs_dentry_d), &child_d, sizeof(struct fs_dentry_d));
            child = dentry_create(child_d.name, child_d.ftype);
            child->ino = child_d.ino;

            dentry_register(child, dentry); // register child to parent
        }
    }
}

/**
 * @brief Read data from file
 */
int file_read(struct fs_inode* file, int offset, void *buf, int size)
{
    int io_size = super.params.size_block;

    int offset_rounded = BLK_ROUND_DOWN(offset);
    int size_rounded = BLK_ROUND_UP(size);

    uint8_t* buffer = (uint8_t*)malloc(size_rounded);
    uint8_t* cur = buffer;

    int blk_ptr = offset_rounded / io_size;

    while (size_rounded > 0) {
        // ! Every block read from disk
        // ! may cause performance issue
        if (file->dno_reg[blk_ptr] == -1) {
            memset(cur, 0, io_size);
        } else {
            disk_read(
                super.data_off + file->dno_reg[blk_ptr] * super.params.size_block,
                cur,
                io_size
            );
        }
        cur += io_size;
        size_rounded -= io_size;
        blk_ptr++;
    }

    int bias = offset - offset_rounded;
    memcpy(buf, buffer + bias, size);
    free(buffer);

    return ERROR_NONE;
}

/**
 * @brief Write data to file
 */
int file_write(struct fs_inode* file, int offset, void *buf, int size)
{
    int io_size = super.params.size_block;

    int offset_rounded = BLK_ROUND_DOWN(offset);
    int size_rounded = BLK_ROUND_UP(size);

    int blk_start = offset_rounded / io_size;
    int blk_ptr = blk_start;

    uint8_t* buffer = (uint8_t*)malloc(size_rounded);

    for (int i = 0; i < size_rounded; i += io_size) {
        if (file->dno_reg[blk_ptr] == -1) {
            memset(buffer + i, 0, io_size);
        } else {
            disk_read(
                super.data_off + file->dno_reg[blk_ptr] * super.params.size_block,
                buffer + i,
                io_size
            );
        }
        blk_ptr++;
    }

    memcpy(buffer + offset_rounded - offset, buf, size);

    blk_ptr = blk_start;
    for (int i = 0; i < size_rounded; i += io_size) {
        if (file->dno_reg[blk_ptr] == -1) {
            file->dno_reg[blk_ptr] = bitmap_alloc(super.dmap, super.params.max_dno);
        }
        disk_write(
            super.data_off + file->dno_reg[blk_ptr] * super.params.size_block,
            buffer + i,
            io_size
        );
        blk_ptr++;
    }
    return ERROR_NONE;
}

/**
 * @brief mount disk
 */
int disk_mount() {

    super.fd = ddriver_open(fs_options.device);

    ddriver_ioctl(super.fd, IOC_REQ_DEVICE_IO_SZ, &super.params.size_io);
    ddriver_ioctl(super.fd, IOC_REQ_DEVICE_SIZE, &super.params.size_disk);
    super.params.size_block = super.params.size_io * 2;

    struct fs_super_d super_d;
    disk_read(0, &super_d, sizeof(struct fs_super_d));

    int is_init = (super_d.magic != FS_MAGIC);

    // Superblock Initialization
    if (is_init) {
        // Initialize the disk
        super_d.magic = FS_MAGIC;

        super_d.param.size_io = super.params.size_io;
        super_d.param.size_disk = super.params.size_disk;
        super_d.param.size_block = super.params.size_block;
        super_d.param.size_usage = 0; 
        super_d.param.max_ino = 1000; 
        super_d.param.max_dno = 4000; 
        
        super_d.super.offset = 0;
        super_d.super.blocks = 1;

        super_d.imap.offset = super_d.super.offset + super_d.super.blocks * super_d.param.size_block;
        super_d.imap.blocks = 1;

        super_d.dmap.offset = super_d.imap.offset + super_d.imap.blocks * super_d.param.size_block;
        super_d.dmap.blocks = 1;

        super_d.inodes.offset = super_d.dmap.offset + super_d.dmap.blocks * super_d.param.size_block;
        super_d.inodes.blocks = 64; 
        
        super_d.data.offset = super_d.inodes.offset + super_d.inodes.blocks * super_d.param.size_block;
        super_d.data.blocks = 4096 - super_d.super.blocks - super_d.imap.blocks - super_d.dmap.blocks - super_d.inodes.blocks; 
    }

    memcpy(&super.params, &super_d.param, sizeof(DiskParam));
    super.super_off = super_d.super.offset;
    super.imap_off = super_d.imap.offset;
    super.dmap_off = super_d.dmap.offset;
    super.inodes_off = super_d.inodes.offset;
    super.data_off = super_d.data.offset;

    // Bitmap Initialization
    if (is_init) {
        super.imap = bitmap_init(super.params.size_block * super_d.imap.blocks * 8);
        super.dmap = bitmap_init(super.params.size_block * super_d.dmap.blocks * 8);
        disk_write(super.imap_off, super.imap, sizeof (super.imap));
        disk_write(super.dmap_off, super.dmap, sizeof (super.dmap));
        free(super.imap);
        free(super.dmap);
    }
    super.imap = bitmap_init(super.params.size_block * super_d.imap.blocks * 8);
    super.dmap = bitmap_init(super.params.size_block * super_d.dmap.blocks * 8);
    disk_read(super.imap_off, super.imap, sizeof(super.imap));
    disk_read(super.dmap_off, super.dmap, sizeof(super.dmap));

    // Root Entry Initialization
    struct fs_dentry *root = dentry_create("/", FT_DIR);
    super.root = root;
    if (is_init) {

        struct fs_inode *root_inode = inode_create();

        int ino = bitmap_alloc(super.imap,super.params.max_ino);
        root_inode->ino = ino;

        dentry_bind(root, root_inode);
        root_inode->dno_dir = bitmap_alloc(super.dmap, super.params.max_dno);
        inode_sync(root_inode);
    }
    dentry_restore(root, 0);

    return ERROR_NONE;
}

/**
 * @brief Unmount the disk
 */
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