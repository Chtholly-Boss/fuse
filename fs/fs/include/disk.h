typedef struct disk_parameters {
    int size_io;
    int size_block; // ! Logic block size
    int size_usage;
    int size_disk;

    int max_ino;    // Max number of inodes
    int max_dno;    // Max number of data blocks
} DiskParam;

typedef struct disk_unit {
    int offset; // offset on disk, start at 0
    int blocks; // number of blocks
} DiskUnit;

struct fs_super_d {
    uint32_t magic;

    DiskParam param;

    DiskUnit super;

    DiskUnit imap;
    DiskUnit dmap;

    DiskUnit inodes;
    DiskUnit data;
};

int disk_read(int offset, void *out_content, int size);
int disk_write(int offset, void *in_content, int size);
int disk_mount();
