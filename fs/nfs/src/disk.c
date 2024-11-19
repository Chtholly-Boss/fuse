#include "../include/nfs.h"

#define DRIVER (super.fd)

extern struct nfs_super      super; 
extern struct custom_options nfs_options;
/**
 * @brief Read data from disk
 */
int disk_read(int offset, void *out_content, int size) {
    int offset_rounded = DISK_ROUND_DOWN(offset);
    int size_rounded = DISK_ROUND_UP(size);

    uint8_t *buffer = (uint8_t*) malloc(size_rounded);
    uint8_t *cur = buffer;

    ddriver_seek(DRIVER, offset_rounded, SEEK_SET);

    while (size_rounded > 0) {
        ddriver_read(DRIVER, cur, DISK_IO_SIZE);
        size_rounded -= DISK_IO_SIZE;
        cur += DISK_IO_SIZE;
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
    int offset_rounded = DISK_ROUND_DOWN(offset);
    int size_rounded = DISK_ROUND_UP(size);

    uint8_t *buffer = (uint8_t*) malloc(size_rounded);
    // ! to avoid overwriting, we need to read the data first
    disk_read(offset_rounded, buffer, size_rounded);

    int bias = offset - offset_rounded;
    memcpy(buffer + bias, in_content, size);

    ddriver_seek(DRIVER, offset_rounded, SEEK_SET);
    uint8_t *cur = buffer;

    while (size_rounded > 0) {
        ddriver_write(DRIVER, cur, DISK_IO_SIZE);
        size_rounded -= DISK_IO_SIZE;
        cur += DISK_IO_SIZE;
    }

    free(buffer);
    return 0;
}

int disk_mount() {
    int driver_fd = ddriver_open(nfs_options.device);
    if (driver_fd < 0) {
        printf("Error: Failed to open driver\n");
        return -1;
    }
    super.fd = driver_fd;

    struct nfs_super_d super_d;

    disk_read(SUPER_OFF, &super_d, sizeof(struct nfs_super_d));

    struct nfs_dentry *root_dentry = alloc_dentry("/", F_DIR);
    if (super_d.magic != NFS_MAGIC) {
        // TODO: create super block
        super_d.magic = NFS_MAGIC;

        super_d.super_block.offset = SUPER_OFF;
        super_d.super_block.blks = SUPER_BLOCKS;

        super_d.inode_bitmap.offset = INODE_BITMAP_OFF;
        super_d.inode_bitmap.blks = INODE_BITMAP_BLOCKS;

        super_d.data_bitmap.offset = DATA_BITMAP_OFF;
        super_d.data_bitmap.blks = DATA_BITMAP_BLOCKS;

        super_d.inodes.offset = INODES_OFF;
        super_d.inodes.blks = INODES_BLOCKS;

        super_d.data_blocks.offset = DATA_BLOCKS_OFF;
        super_d.data_blocks.blks = DATA_BLOCKS_BLOCKS;
        // * Write super block to disk
        disk_write(SUPER_OFF, &super_d, sizeof(struct nfs_super_d));
        // * Create root inode
        struct nfs_inode* root_inode = alloc_inode(root_dentry);
        sync_inode(root_inode);
    }

    // TODO: Construct In-Memory Super Block
    super.inode_map = (uint8_t*) malloc(SIZE_OF(INODE_BITMAP_BLOCKS)); // TODO: malloc a block
    memset(super.inode_map, 0, SIZE_OF(INODE_BITMAP_BLOCKS));
    super.data_map = (uint8_t*) malloc(SIZE_OF(DATA_BITMAP_BLOCKS)); // TODO: malloc a block
    memset(super.data_map, 0, SIZE_OF(DATA_BITMAP_BLOCKS));
    //TODO: Get inode_map and data_map from disk

    //TODO: Create root dentry and bind it to the root inode on disk
    super.root = root_dentry;
    super.root->inode = read_inode(0);

    return 0;
}

int disk_unmount() {
    // * Sync inodes and datas
    sync_inode(super.root->inode);
    // * Sync Super Block
    // ! Already done in the initialization of super block
    // * Sync Inode Map and Data Map
    disk_write(INODE_BITMAP_OFF, super.inode_map, SIZE_OF(INODE_BITMAP_BLOCKS));
    disk_write(DATA_BITMAP_OFF, super.data_map, SIZE_OF(DATA_BITMAP_BLOCKS));
    // * free memory
    free(super.inode_map);
    free(super.data_map);
    // * Close Driver
    ddriver_close(DRIVER);
    return 0;
}

