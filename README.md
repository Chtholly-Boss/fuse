# user-land-filesystem
The repository is mainly for course project, aiming at file system teaching process.

## Disk Read/Write
To manage complexity, we need to encapsulate the details of accessing some data structures.

We will first abstract `read/write` operations on disk. 

To get started, we need to define some macros first.

!!! warning Error Code
    Error codes are necessary for this lab because of the black-box nature of some operations in the lab. If you just return -1, you will get stuck at some point.

```c
// error.h
#define ERROR_NONE          0
#define ERROR_ACCESS        EACCES
#define ERROR_SEEK          ESPIPE     
#define ERROR_ISDIR         EISDIR
#define ERROR_NOSPACE       ENOSPC
#define ERROR_EXISTS        EEXIST
#define ERROR_NOTFOUND      ENOENT
#define ERROR_UNSUPPORTED   ENXIO
#define ERROR_IO            EIO     /* Error Input/Output */
#define ERROR_INVAL         EINVAL  /* Invalid Args */
```

To keep our naming convention consistent, we will use `object_operator` to represent the operations on a data structure.

We will keep some necessary parameters in `super` block, so let's add some field to it.

```c
// define some parameters
typedef struct disk_parameters {
    int size_io;
    int size_block; // ! Logic block size
    int size_usage;

    int max_ino;    // ! Max number of inodes
    ...
} DiskParam;

// add it to super
struct fs_super {
    ...
    DiskParam params;
};
```

Then we can implement disk read/write operations. You can find the implementation in `disk.c`.

To get some feedback, you can add some disk read/write operations in `fs.c::fs_init()`, which will be called when the file system is mounted.

We skip the experiment and turn to the next section.

## mkdir/ls
In this section, we focus on the implementation of `mkdir` and `ls` commands.

We should accomplish the following tasks:

- Implement `mkdir` command
- Implement `ls` command
- when unmounted, write the dentries back to disk
- when mounted, restore the dentries when `ls` is called

However, this section will be the most complex one, so we will break it down into several parts.

Along the way, always keep in mind that our disk layout is:
```
| super | inode Bitmap | data Bitmap | dentries | data |
```

### Bitmap
We can get started by encapsulating the bitmap operations.

A Bitmap maintains a mapping from logical block number to whether it is used or not. So we think the following
operations are necessary:

- `bitmap_init`: initialize the bitmap, set all blocks to unused
- `bitmap_set`: set a block to used
- `bitmap_clear`: set a block to unused

And always, we want to find the first unused block. So we can add the following operations:

- `bitmap_get_first_unused`: get the first unused block

We get to implement the bitmap operations. You can find the implementation in `bitmap.c`.

### Dentry & Inode
First, let's consider how to organize our file system in memory.
We will get to On-Disk structures only when we can suceessfully `mkdir` and `ls` directories in memory.

Inode, aka, index node, is a data structure that stores information about a file or directory. It is used to locate the data blocks that store the file or directory contents.

For directories, an inode can index the subdirectories it has, which means we should add a list-like structure to store the subdirectories.

However, we couldn't know about the number of subdirectories in advance, so we can't allocate a fixed-size array to store the subdirectories. Instead, we can use a linked list to store the subdirectories.

You can treat `struct fs_dentry` as a node wrapper for `struct fs_inode`. And in inode, we can add a pointer to the first node of the linked list.

First, define `FILE_TYPE` as a type to represent the type of a file or directory.

```c
typedef enum file_type {
    FT_REG,
    FT_DIR,
} FileType;
```

Then define `struct fs_dentry` and `struct fs_inode`:
```c
struct fs_inode
{
    int                ino;                           /* 在inode位图中的下标 */
    int                dir_cnt;
    struct fs_dentry* dentry;                        /* 指向该inode的dentry */
    struct fs_dentry* childs;                       /* 所有子目录项 */
};  

struct fs_dentry
{
    char               fname[MAX_FILE_NAME];
    struct fs_dentry* parent;                        /* 父亲Inode的dentry */
    struct fs_dentry* brother;                       /* 兄弟 */
    struct fs_inode*  inode;                         /* 指向inode */
    FILE_TYPE      ftype;
};
```

Now we get to implement the inode and dentry operations. You can find the implementation in `file.c`.

When we create a new file, a common process is:
- allocate a new dentry
- allocate a new inode
- bind the dentry and inode

So we can first implement constructor functions for dentry and inode:
- `dentry_create`: create a new dentry
- `inode_create`: create a new inode
- `dentry_bind`: bind a dentry to an inode

We need register a dentry to another:
- `dentry_register`: register a dentry to its parent

Now we can create directories!

To put our effort into practice, we create the `root` directory and store it to `super`, which is needed for **Path Resolution**.

This process should be done in `fs_init`, for convenience, we put it in `disk_mount`.

To make things alive, we need to implement a simple `mount`.

To put it easy, `mount` is just a function that initializes the in-memory structures, and when it is the first time to mount, it will also initialize the disk.

We use `magic number` to check whether the disk is mounted or not. If the magic number is not correct, we can initialize the disk.

### Path Resolution
Path resolution is the process of breaking down a file path into its components and finding the corresponding file or directory.
