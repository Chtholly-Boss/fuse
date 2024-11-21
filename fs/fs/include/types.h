#ifndef _TYPES_H_
#define _TYPES_H_

#include "disk.h"

#define MAX_NAME_LEN    128     
#define MAX_BLOCK_PER_INODE  4
typedef enum file_type {
    FT_REG,
    FT_DIR,
} FileType;

struct custom_options {
	const char*        device;
};

struct fs_super {
    int      fd;
    /* TODO: Define yourself */
    DiskParam params;

    uint32_t super_off;
    uint32_t imap_off;
    uint32_t dmap_off;
    uint32_t inodes_off;
    uint32_t data_off;

    uint8_t* imap;
    uint8_t* dmap;

    struct fs_dentry *root;
};

struct fs_inode {
    uint32_t ino;

    // * Directory Structure *
    int dir_cnt; // number of sub dentries
    struct fs_dentry *self; 
    struct fs_dentry *childs; // linked list of sub dentries
    uint32_t dno_dir; // data block number

    // * Regular File Structure *
    int size;
    uint8_t *data[MAX_BLOCK_PER_INODE];
    uint32_t dno_reg[MAX_BLOCK_PER_INODE];
};

struct fs_dentry {
    FileType ftype;
    char     name[MAX_NAME_LEN];
    uint32_t ino;

    struct fs_dentry *parent;
    struct fs_dentry *next;

    struct fs_inode *self;
};

struct fs_inode_d {
    uint32_t ino;
    // * Directory Structure *
    int dir_cnt; // number of sub dentries
    uint32_t dno_dir; // data block number
    
    // * Regular File Structure *
    int size;
    uint32_t dno_reg[MAX_BLOCK_PER_INODE];
};

struct fs_dentry_d {
    FileType ftype;
    char     name[MAX_NAME_LEN];
    uint32_t ino;
};
#endif /* _TYPES_H_ */
