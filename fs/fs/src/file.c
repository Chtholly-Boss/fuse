#include "../include/fs.h"

extern struct fs_super super;

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

struct fs_inode* inode_create()
{
    int ino = bitmap_alloc(super.imap, super.params.max_ino);
    if (ino < 0) {
        return NULL;
    }
    struct fs_inode* inode = (struct fs_inode*)malloc(sizeof(struct fs_inode));
    memset(inode, 0, sizeof(struct fs_inode));

    inode->ino = ino;

    inode->dir_cnt = 0;
    inode->self = NULL;
    inode->childs = NULL;

    return inode;
}

void dentry_bind(struct fs_dentry* dentry, struct fs_inode* inode)
{
    dentry->self = inode;
    inode->self = dentry;

    dentry->ino = inode->ino;
}

void dentry_register(struct fs_dentry* dentry, struct fs_dentry* parent)
{
    struct fs_inode* inode = parent->self;

    int is_init = (inode->childs == NULL);
    if (is_init) {
        inode->dno = bitmap_alloc(super.dmap, super.params.max_dno);
    }

    if (is_init) {
        inode->childs = dentry;
    }
    else {
        dentry->next = inode->childs;
        inode->childs = dentry;
    }

    dentry->parent = parent;

    inode->dir_cnt++;
}

// Get file name from path
// e.g. /home/user/file.txt -> file.txt
char* get_fname(const char* path)
{
    if (strcmp(path, "/") == 0) {
        return "/";
    }
    char *fname = strrchr(path, '/') + 1;
    return fname;
}

// Get path level from path
// e.g. / -> 0
// e.g. /home/user/file.txt -> 2
int get_path_level(const char* path)
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

struct fs_dentry *dentries_find(struct fs_dentry *dentries, char *fname)
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
// Get the dentry at index
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
int dentry_lookup(const char* path, struct fs_dentry** dentry)
{
    struct fs_dentry *ptr = super.root; // * start from root
    *dentry = ptr;

    char *fname;

    int levels = get_path_level(path);
    fname = strtok(path, "/");

    for (int i = 0; i < levels; i++) {
        // Find fname in ptr's subdirecties
        ptr = dentries_find(ptr->self->childs, fname);
        if (ptr == NULL) {
            return -1;
        }
        *dentry = ptr;
        fname = strtok(NULL, "/");
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
    inode_d.dno = inode->dno;

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
        int offset = super.data_off + inode->dno * super.params.size_block;
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
    inode->dno = inode_d.dno;
    inode->childs = NULL;

    dentry->self = inode;
    dentry->ino = inode_d.ino;
    
    if (dentry->ftype == FT_DIR) {
        int offset = super.data_off + inode->dno * super.params.size_block;

        struct fs_dentry_d child_d;
        struct fs_dentry* child;
        struct fs_inode* child_inode;

        for (int i = 0; i < inode->dir_cnt; i++) {
            disk_read(offset + i * sizeof(struct fs_dentry_d), &child_d, sizeof(struct fs_dentry_d));
            child = dentry_create(child_d.name, child_d.ftype);
            child->ino = child_d.ino;

            child_inode = (struct fs_inode*)malloc(sizeof(struct fs_inode));
            child->self = child_inode;
            child_inode->self = child;

            // ! directly call this will cause duplicate dno
            // dentry_regiter(child, dentry); // register child to parent
            if (inode->childs == NULL) {
                inode->childs = child;
            }
            else {
                child->next = inode->childs;
                inode->childs = child;
            }
            child->parent = dentry;
            inode->dir_cnt++;
        }

    }
}