#define _XOPEN_SOURCE 700

#include "fs.h"

/******************************************************************************
* SECTION: 宏定义
*******************************************************************************/
#define OPTION(t, p)        { t, offsetof(struct custom_options, p), 1 }

/******************************************************************************
* SECTION: 全局变量
*******************************************************************************/
static const struct fuse_opt option_spec[] = {		/* 用于FUSE文件系统解析参数 */
	OPTION("--device=%s", device),
	FUSE_OPT_END
};

struct custom_options fs_options;			 /* 全局选项 */
struct fs_super super; 
/******************************************************************************
* SECTION: FUSE操作定义
*******************************************************************************/
static struct fuse_operations operations = {
	.init = fs_init,						 /* mount文件系统 */		
	.destroy = fs_destroy,				 /* umount文件系统 */
	.mkdir = fs_mkdir,					 /* 建目录，mkdir */
	.getattr = fs_getattr,				 /* 获取文件属性，类似stat，必须完成 */
	.readdir = fs_readdir,				 /* 填充dentrys */
	.mknod = fs_mknod,					 /* 创建文件，touch相关 */
	.write = fs_write,								  	 /* 写入文件 */
	.read = fs_read,								  	 /* 读文件 */
	.utimens = fs_utimens,				 /* 修改时间，忽略，避免touch报错 */
	.truncate = fs_truncate,						  		 /* 改变文件大小 */
	.unlink = fs_unlink,							  		 /* 删除文件 */
	.rmdir	= fs_rmdir,							  		 /* 删除目录， rm -r */
	.rename = fs_rename,							  		 /* 重命名，mv */

	.open = fs_open,							
	.opendir = fs_opendir,
	.access = fs_access
};
/******************************************************************************
* SECTION: 必做函数实现
*******************************************************************************/
/**
 * @brief 挂载（mount）文件系统
 * 
 * @param conn_info 可忽略，一些建立连接相关的信息 
 * @return void*
 */
void* fs_init(struct fuse_conn_info * conn_info) {
	return disk_mount();
}

/**
 * @brief 卸载（umount）文件系统
 * 
 * @param p 可忽略
 * @return void
 */
void fs_destroy(void* p) {
	disk_umount();
}

/**
 * @brief 创建目录
 * 
 * @param path 相对于挂载点的路径
 * @param mode 创建模式（只读？只写？），可忽略
 * @return int 0成功，否则返回对应错误号
 */
int fs_mkdir(const char* path, mode_t mode) {
	struct fs_dentry* parent;
	if (dentry_lookup(path, &parent) == 0) {
		return ERROR_EXISTS;
	}
	if (parent->ftype != FT_DIR) {
		return ERROR_NOTFOUND;
	}
	struct fs_dentry* dir = dentry_create(get_fname(path), FT_DIR);
	struct fs_inode* inode = inode_create();
	inode->ino = bitmap_alloc(super.imap, super.params.max_ino);
	dentry_bind(dir, inode);

	inode_alloc(parent->self);
	dentry_register(dir, parent);
	parent->self->dir_cnt++;
	
	return ERROR_NONE;
}

/**
 * @brief 获取文件或目录的属性，该函数非常重要
 * 
 * @param path 相对于挂载点的路径
 * @param fs_stat 返回状态
 * @return int 0成功，否则返回对应错误号
 */
int fs_getattr(const char* path, struct stat * fs_stat) {
	struct fs_dentry* dentry;
	if (dentry_lookup(path, &dentry) != 0) {
		return ERROR_NOTFOUND;
	}
	if (dentry->ftype == FT_DIR) {
		fs_stat->st_mode = S_IFDIR | FS_DEFAULT_PERM;
		fs_stat->st_size = dentry->self->dir_cnt * sizeof(struct fs_dentry_d);
	}
	if (dentry->ftype == FT_REG) {
		fs_stat->st_mode = S_IFREG | FS_DEFAULT_PERM;
		fs_stat->st_size = dentry->self->size;
	}

	fs_stat->st_nlink = 1;
	fs_stat->st_uid 	 = getuid();
	fs_stat->st_gid 	 = getgid();
	fs_stat->st_atime   = time(NULL);
	fs_stat->st_mtime   = time(NULL);
	fs_stat->st_blksize = super.params.size_block;

	if (strcmp(path, "/") == 0) {
		fs_stat->st_size	= super.params.size_usage;
		fs_stat->st_blocks = super.params.size_disk / super.params.size_block;
		fs_stat->st_nlink  = 2;		/* !特殊，根目录link数为2 */
	}
	return 0;
}

/**
 * @brief 遍历目录项，填充至buf，并交给FUSE输出
 * 
 * @param path 相对于挂载点的路径
 * @param buf 输出buffer
 * @param filler 参数讲解:
 * 
 * typedef int (*fuse_fill_dir_t) (void *buf, const char *name,
 *				const struct stat *stbuf, off_t off)
 * buf: name会被复制到buf中
 * name: dentry名字
 * stbuf: 文件状态，可忽略
 * off: 下一次offset从哪里开始，这里可以理解为第几个dentry
 * 
 * @param offset 第几个目录项？
 * @param fi 可忽略
 * @return int 0成功，否则返回对应错误号
 */
int fs_readdir(const char * path, void * buf, fuse_fill_dir_t filler, off_t offset,
			    		 struct fuse_file_info * fi)
{
	struct fs_dentry* dentry;
	if (dentry_lookup(path, &dentry) != 0) {
		return ERROR_NOTFOUND;
	}
	struct fs_dentry* dentrys = dentry->self->childs;
	struct fs_dentry* cur = dentry_get(dentrys, offset);
	if (cur == NULL) {
		return ERROR_NOTFOUND;
	}
	filler(buf, cur->name, NULL, offset + 1);		
    return ERROR_NONE;
}

/**
 * @brief 创建文件
 * 
 * @param path 相对于挂载点的路径
 * @param mode 创建文件的模式，可忽略
 * @param dev 设备类型，可忽略
 * @return int 0成功，否则返回对应错误号
 */
int fs_mknod(const char* path, mode_t mode, dev_t dev) {

	if (S_ISDIR(mode)) {
		return fs_mkdir(path, mode);
	}

	struct fs_dentry* parent;
	if (dentry_lookup(path, &parent) == 0) {
		return ERROR_EXISTS;
	}
	if (parent->ftype != FT_DIR) {
		return ERROR_NOTFOUND;
	}
	struct fs_dentry* new_file = dentry_create(get_fname(path), FT_REG);
	struct fs_inode* inode = inode_create();
	inode->ino = bitmap_alloc(super.imap, super.params.max_ino);
	dentry_bind(new_file, inode);

	inode_alloc(parent->self);
	dentry_register(new_file, parent);

	parent->self->dir_cnt++;
	return ERROR_NONE;
}

/**
 * @brief 修改时间，为了不让touch报错 
 * 
 * @param path 相对于挂载点的路径
 * @param tv 实践
 * @return int 0成功，否则返回对应错误号
 */
int fs_utimens(const char* path, const struct timespec tv[2]) {
	(void)path;
	return 0;
}
/******************************************************************************
* SECTION: 选做函数实现
*******************************************************************************/
/**
 * @brief 写入文件
 * 
 * @param path 相对于挂载点的路径
 * @param buf 写入的内容
 * @param size 写入的字节数
 * @param offset 相对文件的偏移
 * @param fi 可忽略
 * @return int 写入大小
 */
int fs_write(const char* path, const char* buf, size_t size, off_t offset,
		        struct fuse_file_info* fi) {
	struct fs_dentry* file;
	if (dentry_lookup(path, &file) != 0) {
		return ERROR_NOTFOUND;
	}
	if (file->ftype != FT_REG) {
		return ERROR_ISDIR;
	}
	struct fs_inode* inode = file->self;

	if (inode->size < offset) {
		return ERROR_SEEK;
	}

	file_write(inode, offset, buf, size);
	
	inode->size = offset + size > inode->size ? offset + size : inode->size;
	return size;
}

/**
 * @brief 读取文件
 * 
 * @param path 相对于挂载点的路径
 * @param buf 读取的内容
 * @param size 读取的字节数
 * @param offset 相对文件的偏移
 * @param fi 可忽略
 * @return int 读取大小
 */
int fs_read(const char* path, char* buf, size_t size, off_t offset,
		       struct fuse_file_info* fi) {
	struct fs_dentry* file;
	if (dentry_lookup(path, &file) != 0) {
		return ERROR_NOTFOUND;
	}
	if (file->ftype != FT_REG) {
		return ERROR_ISDIR;
	}
	struct fs_inode* inode = file->self;
	file_read(inode, offset, buf, size);	
	return size;			   
}

/**
 * @brief 删除文件
 * 
 * @param path 相对于挂载点的路径
 * @return int 0成功，否则返回对应错误号
 */
int fs_unlink(const char* path) {
	struct fs_dentry* file;
	if (dentry_lookup(path, &file) != 0) {
		return ERROR_NOTFOUND;
	}
	dentry_delete(file);

	return ERROR_NONE;
}

/**
 * @brief 删除目录
 * 
 * 一个可能的删除目录操作如下：
 * rm ./tests/mnt/j/ -r
 *  1) Step 1. rm ./tests/mnt/j/j
 *  2) Step 2. rm ./tests/mnt/j
 * 即，先删除最深层的文件，再删除目录文件本身
 * 
 * @param path 相对于挂载点的路径
 * @return int 0成功，否则返回对应错误号
 */
int fs_rmdir(const char* path) {

	struct fs_dentry* file;
	if (dentry_lookup(path, &file) != 0) {
		return ERROR_NOTFOUND;
	}
	dentry_delete(file);

	return 0;
}

/**
 * @brief 重命名文件 
 * 
 * @param from 源文件路径
 * @param to 目标文件路径
 * @return int 0成功，否则返回对应错误号
 */
int fs_rename(const char* from, const char* to) {
	struct fs_dentry* from_file;
	if (dentry_lookup(from, &from_file) != 0) {
		return ERROR_NOTFOUND;
	}

	if (strcmp(from, to) == 0) {
		return ERROR_NONE;
	}

	struct fs_dentry* parent;
	if (dentry_lookup(to, &parent) == 0) {
		return ERROR_EXISTS;
	}

	dentry_unregister(from_file);

	inode_alloc(parent->self);
	dentry_register(from_file, parent);

	parent->self->dir_cnt++;

	char* fname = get_fname(to);

	memcpy(from_file->name, fname, strlen(fname) + 1);

	return ERROR_NONE;
}

/**
 * @brief 打开文件，可以在这里维护fi的信息，例如，fi->fh可以理解为一个64位指针，可以把自己想保存的数据结构
 * 保存在fh中
 * 
 * @param path 相对于挂载点的路径
 * @param fi 文件信息
 * @return int 0成功，否则返回对应错误号
 */
int fs_open(const char* path, struct fuse_file_info* fi) {
	return ERROR_NONE;
}

/**
 * @brief 打开目录文件
 * 
 * @param path 相对于挂载点的路径
 * @param fi 文件信息
 * @return int 0成功，否则返回对应错误号
 */
int fs_opendir(const char* path, struct fuse_file_info* fi) {
	return ERROR_NONE;
}

/**
 * @brief 改变文件大小
 * 
 * @param path 相对于挂载点的路径
 * @param offset 改变后文件大小
 * @return int 0成功，否则返回对应错误号
 */
int fs_truncate(const char* path, off_t offset) {
	struct fs_dentry* file;
	if (dentry_lookup(path, &file) != 0) {
		return ERROR_NOTFOUND;
	}
	if (file->ftype != FT_REG) {
		return ERROR_ISDIR;
	}
	struct fs_inode* inode = file->self;
	inode->size = offset;
	return ERROR_NONE;
}


/**
 * @brief 访问文件，因为读写文件时需要查看权限
 * 
 * @param path 相对于挂载点的路径
 * @param type 访问类别
 * R_OK: Test for read permission. 
 * W_OK: Test for write permission.
 * X_OK: Test for execute permission.
 * F_OK: Test for existence. 
 * 
 * @return int 0成功，否则返回对应错误号
 */
int fs_access(const char* path, int type) {
	struct fs_dentry* dentry;
	int fail = 0;
	switch (type)
	{
		case R_OK:
		case W_OK:
		case X_OK:
			fail = 0; break;
		case F_OK:
			fail = dentry_lookup(path, &dentry); break;
	}

	return fail ? ERROR_ACCESS : ERROR_NONE;
}	
/******************************************************************************
* SECTION: FUSE入口
*******************************************************************************/
int main(int argc, char **argv)
{
    int ret;
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

	fs_options.device = strdup("/home/cauchy/ddriver");

	if (fuse_opt_parse(&args, &fs_options, option_spec, NULL) == -1)
		return -1;
	
	ret = fuse_main(args.argc, args.argv, &operations, NULL);
	fuse_opt_free_args(&args);
	return ret;
}