#include "../include/fs.h"

extern struct fs_super super;


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
