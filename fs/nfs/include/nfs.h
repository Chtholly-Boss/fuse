#ifndef _NFS_H_
#define _NFS_H_

#define FUSE_USE_VERSION 26
#include "stdio.h"
#include "stdlib.h"
#include <unistd.h>
#include "fcntl.h"
#include "string.h"
#include "fuse.h"
#include <stddef.h>
#include "ddriver.h"
#include "errno.h"
#include "types.h"
#include "stdint.h"

#define MAX_NAME_LEN    128     
#define NFS_MAGIC           0x20220915 
#define NFS_DEFAULT_PERM    0777   /* 全权限打开 */

#define BLOCK_SIZE 1024
#define SIZE_OF(blks) (blks * BLOCK_SIZE)

typedef enum file_type{
    F_REG,
    F_DIR,
} FILE_TYPE;

// * Macro functions
#define UPPER_BOUND(off, blks) ((off) + (blks) * BLOCK_SIZE)

#define ROUND_DOWN(value, round)    ((value) % (round) == 0 ? (value) : ((value) / (round)) * (round))
#define ROUND_UP(value, round)      ((value) % (round) == 0 ? (value) : ((value) / (round) + 1) * (round))

#define IS_REG(inode) ((inode->ftype) == F_REG)
#define IS_DIR(inode) ((inode->ftype) == F_DIR)

// * disk.c
#define DISK_IO_SIZE 512
#define MAX_DNOS_PER_INODE 4
#define DISK_ROUND_DOWN(x) ROUND_DOWN(x, DISK_IO_SIZE) // 512 is the block size
#define DISK_ROUND_UP(x) ROUND_UP(x, DISK_IO_SIZE) // 512 is the block size
// layout of the disk
#define SUPER_OFF 0
#define SUPER_BLOCKS 1

#define INODE_BITMAP_OFF UPPER_BOUND(SUPER_OFF, SUPER_BLOCKS)
#define INODE_BITMAP_BLOCKS 1

#define DATA_BITMAP_OFF UPPER_BOUND(INODE_BITMAP_OFF, INODE_BITMAP_BLOCKS)
#define DATA_BITMAP_BLOCKS 1

#define INODES_OFF UPPER_BOUND(DATA_BITMAP_OFF, DATA_BITMAP_BLOCKS)
#define INODES_BLOCKS 64

#define DATA_BLOCKS_OFF UPPER_BOUND(INODES_OFF, INODES_BLOCKS)
#define DATA_BLOCKS_BLOCKS 4000

typedef struct {
	uint32_t offset;
	uint32_t blks;
} DiskUnit;

struct nfs_super_d {
    uint32_t magic;

	DiskUnit super_block;

	DiskUnit inode_bitmap;
	DiskUnit data_bitmap;

	DiskUnit inodes;
	DiskUnit data_blocks;
};

struct nfs_dentry_d {
    char     name[MAX_NAME_LEN];
    uint32_t ino;
    FILE_TYPE ftype;
};

struct nfs_inode_d {
    uint32_t ino;

    FILE_TYPE ftype;

    int dnos[MAX_DNOS_PER_INODE];

    int size;
	// * If is a directory, store the number of entries in the directory
    int dir_cnt;
};

int disk_read(int offset, void *out_content, int size);
int disk_write(int offset, void *in_content, int size);
int disk_mount();
int disk_unmount();

// * bitmap.c
int bitmap_get_bit(uint8_t *bitmap, int index);
int bitmap_reverse_bit(uint8_t *bitmap, int index);
int bitmap_find_first_zero(uint8_t *bitmap);

// * nfs.c

struct custom_options {
	const char*        device;
};

struct nfs_super {
    int      fd;

	uint8_t *inode_map;
	uint8_t *data_map;

	struct nfs_dentry* root;
};

struct nfs_inode {
    uint32_t ino;

	FILE_TYPE ftype;
	struct nfs_dentry* dentry;                          // 指向该inode的dentry 
	
	// * dno is necessary because we need to read the data from the disk
	int dnos[MAX_DNOS_PER_INODE]; 
	uint8_t *data[MAX_DNOS_PER_INODE];

    int size;

	// * If is a directory:
    int dir_cnt;
    struct nfs_dentry* dentrys;                         // 所有目录项 

};

struct nfs_dentry {
    char     name[MAX_NAME_LEN];
    uint32_t ino;
	FILE_TYPE ftype;
    struct nfs_inode*       inode;                      // 文件对应的inode

	// * If is a directory:
	struct nfs_dentry*      parent;                     // 父目录的dentry
    struct nfs_dentry*      brother;                    // 兄弟的dentry
};

void* 			   nfs_init(struct fuse_conn_info *);
void  			   nfs_destroy(void *);
int   			   nfs_mkdir(const char *, mode_t);
int   			   nfs_getattr(const char *, struct stat *);
int   			   nfs_readdir(const char *, void *, fuse_fill_dir_t, off_t,
						                struct fuse_file_info *);
int   			   nfs_mknod(const char *, mode_t, dev_t);
int   			   nfs_write(const char *, const char *, size_t, off_t,
					                  struct fuse_file_info *);
int   			   nfs_read(const char *, char *, size_t, off_t,
					                 struct fuse_file_info *);
int   			   nfs_access(const char *, int);
int   			   nfs_unlink(const char *);
int   			   nfs_rmdir(const char *);
int   			   nfs_rename(const char *, const char *);
int   			   nfs_utimens(const char *, const struct timespec tv[2]);
int   			   nfs_truncate(const char *, off_t);
			
int   			   nfs_open(const char *, struct fuse_file_info *);
int   			   nfs_opendir(const char *, struct fuse_file_info *);

// * file.c
struct nfs_dentry* alloc_dentry(char * fname, FILE_TYPE ftype);
int register_dentry(struct nfs_dentry *dentry, struct nfs_inode *inode);
struct nfs_inode* alloc_inode(struct nfs_dentry * dentry);
int sync_inode(struct nfs_inode * inode);
struct nfs_inode* read_inode(uint32_t ino);
#endif  /* _nfs_H_ */