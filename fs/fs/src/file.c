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

void dentry_regiter(struct fs_dentry* dentry, struct fs_dentry* parent)
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