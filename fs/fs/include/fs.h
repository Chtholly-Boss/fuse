#ifndef _FS_H_
#define _FS_H_

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
#include "error.h"

#define FS_MAGIC 0x20220915
#define FS_DEFAULT_PERM 0777 /* 全权限打开 */

#define ROUND_DOWN(value, round) ((value) % (round) == 0 ? (value) : ((value) / (round)) * (round))
#define ROUND_UP(value, round) ((value) % (round) == 0 ? (value) : ((value) / (round) + 1) * (round))

/******************************************************************************
 * SECTION: fs.c
 *******************************************************************************/
void *fs_init(struct fuse_conn_info *);
void fs_destroy(void *);
int fs_mkdir(const char *, mode_t);
int fs_getattr(const char *, struct stat *);
int fs_readdir(const char *, void *, fuse_fill_dir_t, off_t,
			   struct fuse_file_info *);
int fs_mknod(const char *, mode_t, dev_t);
int fs_write(const char *, const char *, size_t, off_t,
			 struct fuse_file_info *);
int fs_read(const char *, char *, size_t, off_t,
			struct fuse_file_info *);
int fs_access(const char *, int);
int fs_unlink(const char *);
int fs_rmdir(const char *);
int fs_rename(const char *, const char *);
int fs_utimens(const char *, const struct timespec tv[2]);
int fs_truncate(const char *, off_t);

int fs_open(const char *, struct fuse_file_info *);
int fs_opendir(const char *, struct fuse_file_info *);

// * bitmap.c
uint8_t *bitmap_init(uint32_t size);
int bitmap_alloc(uint8_t *bitmap, uint32_t size);

// * file.c
struct fs_dentry *dentry_create(const char *name, FileType ftype);
struct fs_inode *inode_create();
void dentry_bind(struct fs_dentry *dentry, struct fs_inode *inode);
void dentry_register(struct fs_dentry *dentry, struct fs_dentry *parent);
char *get_fname(char *path);

struct fs_dentry *dentries_find(struct fs_dentry *dentries, char *fname);
struct fs_dentry *dentry_get(struct fs_dentry *dentries, int index);
int dentry_lookup(char *path, struct fs_dentry **dentry);

int inode_sync(struct fs_inode *inode);
int dentry_restore(struct fs_dentry *dentry, int ino);
void inode_alloc(struct fs_inode *inode);

// * disk.c
int disk_read(int offset, void *out_content, int size);
int disk_write(int offset, void *in_content, int size);
int disk_mount();
int disk_umount();

#endif /* _fs_H_ */
