#import "../template.typ": *

= 实验详细设计

#quote[图文并茂地描述实验实现的所有功能和详细的设计方案及实验过程中的特色部分。]

hitFuse 是一个基于 FUSE 的文件系统，支持以下功能：

- 创建、删除、重命名文件和目录 `mkdir, touch, ls, rm, mv ...`
- 读取和写入文件内容 `cat, echo ...`
- 查看文件和目录的属性 `ls ...`

该系统在设计上尽可能采用了模块化设计，于 `Simplefs` 的基础上更加明确地划分了具体的功能模块，并实现了对 FUSE 常用功能的封装，使得文件系统的实现更加简洁和高效。

== 总体设计方案
#quote[详细阐述文件系统的总体设计思路，包括系统架构图和关键组件的说明]

hitFuse 的整体架构图如下：

#figure(
  image("../assets/arch.png"),
  caption: "hitFuse 整体架构",
)

hitFuse 的整体架构可以分为以下几个部分：
- *Disk Manager*: 负责管理磁盘空间，包括磁盘读写、内存与磁盘的信息同步、文件读写等。
- *Memory Manager*: 负责管理文件系统的元数据
  - 通过 `inode` 与 `dentry` 维护文件系统的层次结构
  - 通过 `Inode Bitmap` 与 `Data Bitmap` 维护磁盘空间的使用情况

所有来自用户空间的操作都会经过 `Memory Manager`，由 `Memory Manager` 进行元数据操作后再进行必要的磁盘操作，从而保证文件系统的正确性和一致性。

== 数据结构说明

本系统中涉及到的数据结构对象如下：
#grid(
	columns: 3,
	rows: 2, row-gutter: 1em,
	DS(
		name: "Inode Bitmap",
		description: [磁盘上 inode 的使用情况，用于维护 inode 的分配和释放。],
		funcs: [
			- bitmap_init 
			- bitmap_alloc 
			- bitmap_clear 
		]
	), 
	DS(
		name: "Data Bitmap",
		description: [磁盘上数据块的使用情况，用于维护数据块的分配和释放。],
		funcs: [
		-	 bitmap_init  
		-	 bitmap_alloc 
		-	 bitmap_clear 
		]
	),
	DS(
		name: "Dentry",
		description: [目录项，包括文件名、文件类型等。],
		funcs: [
		-	dentry_create 
		-	dentry_bind
		-	dentry_register
		-	dentry_unregister
		-	dentry_get(index)
		-	dentry_delete
		-	dentry_lookup
		]
	),
	DS(
		name: "Inode",
		description: [文件或目录的元数据，包括其使用的数据块、大小、子目录等。],
		funcs: [
		-	inode_create
		-	inode_alloc
		]
	),
	DS(
		name: "Super Block",
		description: [全局的元数据，包括文件系统的总大小、已使用空间、根目录等。],
		funcs: [
		]
	),
	DS(
		name: "Disk",
		description: [磁盘，用于实际存储文件系统的数据。],
		funcs: [
		-	disk_read
		-	disk_write
		-	disk_mount
		-	disk_umount
		-	inode_sync
		-	dentry_restore
		]
	)
)
=== Bitmap
Bitmap 是一个位图，用于表示磁盘空间的使用情况。每个位表示一个磁盘块的使用情况，0 表示未使用，1 表示已使用。本系统中的数据位图包括 `Inode Bitmap` 和 `Data Bitmap`。

任何对磁盘存储空间的操作（除`Super Block`外）都需要先对相应的位图进行操作，以维护磁盘空间的使用情况。

Bitmap 提供以下方法：
- `bitmap_init`: 初始化位图，将所有位设置为 0。
- `bitmap_alloc`: 分配一个未使用的磁盘块，并返回其索引。
- `bitmap_clear`: 将位图中指定索引的位设置为 0，表示释放该磁盘块。

#design_choice[
  在 alloc 和 clear 时，可以对磁盘相应的数据块进行初始化或清除，以防止隐私泄露。

  在本实现中，仅通过对位图进行操作，标记相应的数据块可用。
]

相应方法的实现均为 bit-level 操作，以 `bitmap_clear` 为例：
```c
void bitmap_clear(uint8_t *bitmap, uint32_t index) {
    uint32_t byte_index = index / 8;
    uint32_t bit_index = index % 8;
    bitmap[byte_index] &= ~(1 << bit_index);
}
```
通过将 `imap` 或 `dmap` 传入该函数，可以实现对相应位图的修改。

=== Dentry && Inode <dentry_inode>
Dentry 是目录项，主要用于维护文件系统的层次结构。其结构如下：

```c
struct fs_dentry {
    FileType ftype;
    char     name[MAX_NAME_LEN];

    uint32_t ino;
    struct fs_inode *self;

    struct fs_dentry *parent;
    struct fs_dentry *next;
};
```

该结构主要包括三个部分：
- 文件类型 `ftype` 与文件名 `name`
- 文件对应的 `inode` 索引 `ino` 与 `inode` 结构体指针 `self`。该部分将 `inode` 与 `dentry` 关联起来，使得我们可以通过 `dentry` 快速找到对应的 `inode`
- 父目录 `parent` 与兄弟节点 `next`。该部分为维护文件系统层次结构的链表节点，使得我们可以通过 `dentry` 快速找到其父目录和兄弟节点

Inode 用于维护文件或目录的元数据，包括其使用的数据块、大小、子目录等。其结构如下：

```c
struct fs_inode {
    uint32_t ino;
    struct fs_dentry *self;

    // * Directory Structure *
    int dir_cnt; // number of sub dentries
    struct fs_dentry *childs; // linked list of sub dentries
    uint32_t dno_dir; // data block number

    // * Regular File Structure *
    int size;
    uint32_t dno_reg[MAX_BLOCK_PER_INODE];
};
```
该结构主要包括三个部分：
- 文件索引 `ino` 与 `dentry` 结构体指针 `self`。该部分将 `inode` 与 `dentry` 关联起来，使得我们可以通过 `inode` 快速找到对应的 `dentry`
- 目录结构 `Directory Structure`。该部分用于维护目录的子目录，包括子目录的数量 `dir_cnt`、子目录的链表 `childs` 以及子目录的数据块索引 `dno_dir`
- 文件结构 `Regular File Structure`。该部分用于维护文件的元数据，包括文件大小 `size` 以及文件的数据块索引 `dno_reg`

#design_choice[
  本实现中，每个文件最多只能包含 MAX_BLOCK_PER_INODE 个数据块。

  事实上，如果将 dno_reg 改为指针数组，则可以作进一步扩展，如支持间接数据块索引，从而支持更大的文件。
]

以上两个数据结构共同实现了以下接口：
- `dentry_create` 与 `inode_create`: 创建新的 `dentry` 与 `inode`，未分配磁盘空间
- `dentry_bind(inode)`: 将 `inode` 与 `dentry` 绑定
- `dentry_register/unregister`: 将 `dentry` 注册到父目录中，或从父目录中移除
- `dentry_get(index)`: 获取指定索引的子目录项
- `dentry_delete`: 删除 `dentry`，递归清除位图上的对应位，标记为未使用
- `inode_alloc`: 在磁盘上分配新的数据块用于存放 inode 的子目录项

其中值得一提的是 `dentry_register` 与 `dentry_unregister`：
```c 
void dentry_register(struct fs_dentry* dentry, struct fs_dentry* parent)
{
    struct fs_inode* inode = parent->self;

    if (inode->childs == NULL) {
        inode->childs = dentry;
    }
    else {
        dentry->next = inode->childs;
        inode->childs = dentry;
    }

    dentry->parent = parent;
}
```
可以看到，此处仅进行了简单的注册，将 `dentry` 添加到 `inode->childs` 链表的头部。

该函数既可在创建新目录时调用，也可在从磁盘中重建 `dentry` 时调用。

```c 
void dentry_unregister(struct fs_dentry* dentry)
{
    struct fs_dentry* parent = dentry->parent;
    struct fs_inode* inode = parent->self;

    if (inode->childs == dentry) {
        inode->childs = dentry->next;
    }
    else {
        struct fs_dentry* ptr = inode->childs;
        while (ptr->next != dentry) {
            ptr = ptr->next;
        }
        ptr->next = dentry->next;
    }

    dentry->parent = NULL;
    dentry->next = NULL;
    
    inode->dir_cnt--;
}
```

该函数用于从父目录中移除 `dentry`，并递减 `inode->dir_cnt`。

#design_choice[
	在 register 时，我们并没有将父目录的 `dir_cnt` 增加，这是因为在从磁盘重建 `dentry` 时，我们用到了 `dir_cnt` 属性，为避免冲突，我们选择将该操作交由调用者处理。
]

=== Path Resolution
Path Resolution 是文件系统的一个重要功能，用于将用户输入的路径解析为对应的 `dentry`。通过前面所定义的数据结构，再加上以下的辅助函数：
- `get_fname`: 获取路径中的文件名

我们可以提供以下接口：
- `dentries_find(fname)`: 在当前目录的子目录中查找文件名对应的 `dentry`
  - 遍历当前目录项 `dentry->self->childs`，即存储在 `inode->childs` 中的链表，查找文件名对应的 `dentry`。
- `dentry_lookup(path)`：从根目录开始，查找路径对应的 `dentry`
  - 逐层解析路径，调用 `dentries_find` 查找对应的 `dentry`

`dentry_lookup` 的实现如下：

```c
int dentry_lookup(char* path, struct fs_dentry** dentry)
{
    struct fs_dentry *ptr = super.root; // * start from root
    *dentry = ptr;

    char *fname;

    char* path_bak = strdup(path);

    int levels = get_path_level(path_bak);
    fname = strtok(path_bak, "/");

    for (int i = 0; i < levels; i++) {
        // Find fname in ptr's subdirecties
        ptr = dentry_find(ptr->self->childs, fname);
        if (ptr == NULL) {
            return ERROR_NOTFOUND;
        }
        *dentry = ptr;
        fname = strtok(NULL, "/");
    }
    if (ptr->self == NULL) {
        dentry_restore(ptr, ptr->ino);
    }
    return 0;
}
```
其中，`dentry_restore` 函数从磁盘中恢复 `dentry`，在后文会详细介绍。

=== Disk 
Disk 为连续的数据块，每个数据块的大小为 BLOCK_SIZE。为了实现磁盘的读写操作，定义了以下接口：
- `disk_read` 与 `disk_write`: 读写磁盘数据块
- `file_read` 与 `file_write`: 直接同步文件数据到磁盘
- `inode_sync`: 同步 `inode` 数据到磁盘
- `dentry_restore`: 从磁盘数据重建 `dentry`
- `disk_mount`: 挂载磁盘，初始化 `super` 结构体
- `disk_umount`: 卸载磁盘，同步必要的数据，释放资源

各接口的实现思路如下：

==== `disk_read` && `disk_write` 

主要参考 Simplefs 中的实现，通过设置暂存区 `buf`，将对齐的数据块读入暂存区，再从暂存区写入目标地址。

#design_choice[
  本实现中，暂存区 `buf` 的大小和读写大小相同，在磁盘读写过程中可能会导致性能下降。 

  事实上，不对齐的数据块仅有首尾两块，可以只对首尾两块进行对齐操作，从而提高性能。
]

==== `file_read` && `file_write` 
通过调用 `disk_read` 与 `disk_write`，将文件数据同步到磁盘。

`inode` 结构体中存储了文件的数据块索引 `dno_reg[]`，因此，`file_read` 与 `file_write` 函数可以直接通过 `dno_reg[]` 找到文件的数据块，进行读写操作。

读写逻辑如下图所示：
#figure(
	image("../assets/filerw.png", width: 80%),
	caption: [
		`file_read` 与 `file_write` 的读写逻辑，inode 中的 dno 与 dmap 中已分配的位图位一一对应，buffer 与已分配的数据块直接交互。
	]
)
`file_write` 的具体实现如下：
```c
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
```

==== `inode_sync` && `dentry_restore`
`inode_sync` 和 `dentry_restore` 主要用于同步文件系统的层次结构到磁盘。

- `inode_sync` 将 `inode` 写入磁盘的索引节点区，同时，将 `inode` 的子目录项递归写入磁盘的数据块中。
- `dentry_restore` 从磁盘的数据块中读取子目录项，并通过 `dentry_register` 将子目录项注册到父目录中。

值得一提的是由于我们的文件读写采取直接与磁盘交互的方式，因此 `inode_sync` 不需要对文件进行同步，只需要同步 `inode` 即可。

```c
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
```

==== `disk_mount` && `disk_umount` <mount>

`disk_mount` 和 `disk_umount` 主要用于挂载和卸载磁盘。

- `disk_mount` 的主要流程如下：
  - 读取磁盘的超级块 `super`，根据 *Magic Number* 判断是否需要格式化磁盘。
  - 同步磁盘的位图到内存，以便进行磁盘空间的分配和回收。
  - 重建根目录
- `disk_umount` 的主要流程如下：
  - 同步超级块 `super` 到磁盘
  - 同步位图到磁盘
  - 递归同步根目录到磁盘

== 功能详细说明
#quote[每个功能点的详细说明（关键的数据结构、核心代码、流程等）]

建立在上述数据结构的基础上，各功能的实现如下：

=== 挂载与卸载
挂载与卸载磁盘是文件系统的重要功能，用于初始化文件系统，以及同步文件系统的层次结构到磁盘。

该功能的 hook 函数为 `fs_init` 与 `fs_destroy`，具体实现如下：
```c
void* fs_init(struct fuse_conn_info * conn_info) {
	return disk_mount();
}

void fs_destroy(void* p) {
	disk_umount();
}
```
=== 查看文件信息
查看文件信息的功能对应的 hook 函数为 `fs_getattr`，参考 Simplefs 中的实现，具体实现如下：
```c
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
```

#clues.warning[
  `mkdir` 与 `mknod` 均会经过该函数，必须先实现该函数。
]

=== 创建文件/文件夹
创建文件夹的功能对应的 hook 函数为 `fs_mkdir`，实现思路为：
- 创建 dentry 与 inode，并将它们绑定
- 在 imap 中分配新的 inode 索引
- 将新建的 dentry 注册到父目录中

具体实现如下：
```c
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
```

创建文件功能对应的 hook 函数为 `fs_mknod`，其实现思路与创建文件夹类似，具体实现如下：
```c
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
```

#clues.warning[
  - 创建文件时不需要 malloc，所有数据都直接与磁盘进行交互
  - 创建文件时不需要分配数据块，写入数据时会进行动态分配
]

=== 查看文件夹内容
查看文件夹内容的功能对应的 hook 函数为 `fs_readdir`，具体实现如下：
```c
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
```

以上为本实验必做部分的实现，附加功能在实验特色中进行介绍。

== 实验特色
#quote[实验中你认为自己实现的比较有特色的部分，包括设计思路、实现方法和预期效果。]

本系统的特色在于一定程度的模块化，在设计上适当采用了 OOP 思想。与 Simplefs 中笼统将所有操作都放在 `sfs.utils` 中不同，本系统以各操作的对象为主体，在其上抽象出了一系列通用操作，使得代码更加清晰、简洁、易扩展。

下面以实验附加题的实现为例进行说明：

=== 预备工作
按指导书要求，我们首先完成 `fs_open`, `fs_opendir`, `fs_access` 和 `fs_truncate` 四个函数，参考 Simplefs 中的实现即可。

```c
int fs_open(const char* path, struct fuse_file_info* fi) {
	return ERROR_NONE;
}

int fs_opendir(const char* path, struct fuse_file_info* fi) {
	return ERROR_NONE;
}

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
```

=== 读写文件 
读写文件对应的 hook 函数为 `fs_read` 和 `fs_write`，调用 `file_read` 和 `file_write` 与磁盘进行直接交互即可，具体实现如下：

```c
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
```

=== 删除文件/目录
删除文件对应的 hook 函数为 `fs_unlink`，调用 `dentry_delete` 即可具体实现如下：
```c
int fs_unlink(const char* path) {
	struct fs_dentry* file;
	if (dentry_lookup(path, &file) != 0) {
		return ERROR_NOTFOUND;
	}
	dentry_delete(file);

	return ERROR_NONE;
}
```

=== MV 命令
MV 命令对应的 hook 函数为 `fs_rename`，实现思路如下：
- 通过 `dentry_unregister` 从父目录中移除
- 通过 `dentry_register` 将其注册到新的父目录中
- 修改 `dentry` 中的 `name` 字段

具体实现如下：
```c
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
```

相对于指导书中给出的实现，该实现更加符合直觉，易于理解。

由上述实现可以看出，对于附加功能的实现都可按直觉进行，代码简洁且易扩展。