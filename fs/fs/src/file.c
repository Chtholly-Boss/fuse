#include "../include/fs.h"

extern struct fs_super super;

#define BLK_ROUND_DOWN(off)     ROUND_DOWN(off,(super.params.size_block))
#define BLK_ROUND_UP(off)      ROUND_UP(off,(super.params.size_block))

/**
 * @brief Create an In-Memory empty dentry, no inode binded
 */
struct fs_dentry* dentry_create(const char* name, FileType ftype)
{
    struct fs_dentry* dentry = (struct fs_dentry*)malloc(sizeof(struct fs_dentry));
    memset(dentry, 0, sizeof(struct fs_dentry));
    memcpy(dentry->name, name, strlen(name));
    dentry->ftype = ftype;

    dentry->ino = -1;
    dentry->self = NULL;

    dentry->parent = NULL;
    dentry->next= NULL;

    return dentry;    
}

/**
 * @brief Create an In-Memory empty inode, no ino assigned
 */
struct fs_inode* inode_create()
{
    struct fs_inode* inode = (struct fs_inode*)malloc(sizeof(struct fs_inode));
    memset(inode, 0, sizeof(struct fs_inode));

    inode->ino = -1;

    inode->dir_cnt = 0;
    inode->self = NULL;
    inode->childs = NULL;
    inode->dno_dir = -1;

    inode->size = 0;
    for (int i = 0; i < MAX_BLOCK_PER_INODE; i++) {
        inode->dno_reg[i] = -1;
    }

    return inode;
}

/**
 * @brief Bind dentry and inode, assign inode::ino to dentry::ino
 */
void dentry_bind(struct fs_dentry* dentry, struct fs_inode* inode)
{
    dentry->self = inode;
    inode->self = dentry;

    if (inode->ino == -1) {
        inode->ino = dentry->ino;
    } else {
        dentry->ino = inode->ino;
    }
}

/**
 * @brief Register a dentry to its parent dentry
 * @attention Caller should allocate data blocks for the parent dentry
 */
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
/**
 * @brief Get the file name from path
 * @example / -> /
 * @example /home/user -> user
 */
char* get_fname(char* path)
{
    if (strcmp(path, "/") == 0) {
        return "/";
    }
    char *fname = strrchr(path, '/') + 1;
    return fname;
}

/**
 * @brief Get the level of path
 * @example / -> 0
 * @example /home -> 1
 * @example /home/user -> 2
 */
int get_path_level(char* path)
{
    if (strcmp(path, "/") == 0) {
        return 0;
    }

    int level = 0;
    char *p = path;
    while (*p != '\0') {
        if (*p == '/') {
            level++;
        }
        p++;
    }
    return level;
}

/**
 * @brief Find dentry from dentries that match the given name
 */
struct fs_dentry *dentry_find(struct fs_dentry *dentries, char *fname)
{
    struct fs_dentry *dentry = dentries;
    while (dentry != NULL) {
        if (strcmp(dentry->name, fname) == 0) {
            return dentry;
        }
        dentry = dentry->next;
    }
    return NULL;
}

/**
 * @brief Get dentry from dentries by index
 */
struct fs_dentry *dentry_get(struct fs_dentry* dentries, int index)
{
    struct fs_dentry *dentry = dentries;
    while (dentry != NULL) {
        if (index == 0) {
            return dentry;
        }
        dentry = dentry->next;
        index--;
    }
    return NULL;
}

/**
 * @brief Resolve path to dentry
 * @return 0 if found, and put dentry to *dentry, else put parent dentry to *dentry
 */
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
 * @brief Allocate data block for inode if needed
 * @attention should be called whenever new data is written to inode
 */
void inode_alloc(struct fs_inode* inode)
{
    struct fs_dentry* dentry = inode->self;
    if (dentry->ftype == FT_DIR) {
        if (inode->childs == NULL && inode->dno_dir == -1) {
            inode->dno_dir = bitmap_alloc(super.dmap, super.params.max_dno);
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

int dentry_delete(struct fs_dentry* dentry)
{
    dentry_unregister(dentry);
    if (dentry->ftype == FT_REG) {
        for (int i = 0; i < MAX_BLOCK_PER_INODE; i++) {
            if (dentry->self->dno_reg[i] != -1) {
                bitmap_clear(super.dmap, dentry->self->dno_reg[i]);
            }
        }
    }
    if (dentry->ftype == FT_DIR) {
        for (int i = 0; i < dentry->self->dir_cnt; i++){
            dentry_delete(dentry->self->childs);
        }
        if (dentry->self->dno_dir != -1){
            bitmap_clear(super.dmap, dentry->self->dno_dir);
        }
    }

    bitmap_clear(super.imap, dentry->self->ino);
    free(dentry->self);
    free(dentry);
    return ERROR_NONE;
}
