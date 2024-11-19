#include "../include/nfs.h"

extern struct nfs_super super;

struct nfs_dentry* alloc_dentry(char * fname, FILE_TYPE ftype)
{
    struct nfs_dentry * dentry = (struct nfs_dentry *)malloc(sizeof(struct nfs_dentry));
    memset(dentry, 0, sizeof(struct nfs_dentry));
    memcpy(dentry->name, fname, strlen(fname));
    dentry->ftype   = ftype;
    dentry->ino     = -1;
    dentry->inode   = NULL;
    dentry->parent  = NULL;
    dentry->brother = NULL;  
    return dentry;                                          
}

// Register a dentry under a given inode, inode should be a directory
// ! For now, assume that number of subdirectories are
// !  limited in the First block
int register_dentry(struct nfs_dentry *dentry, struct nfs_inode *inode)
{
    // Check if inode is a directory
    if (!IS_DIR(inode)) {
        printf("Error: Register a dentry to a Regular file\n");
        return -1;
    }

    if (inode->dentrys == NULL) {
        inode->dentrys = dentry;

        int dno = bitmap_find_first_zero(super.data_map);
        bitmap_reverse_bit(super.data_map, dno); // mark the data block as used
        // ! You should check if the dno is valid
        inode->dnos[0] = dno;
    }
    else {
        dentry->brother = inode->dentrys;
        inode->dentrys = dentry;
    }
    inode->dir_cnt++;
    return inode->dir_cnt;
}

// Alloc inode for a given dentry, which marks the dentry as a valid file
struct nfs_inode* alloc_inode(struct nfs_dentry * dentry)
{
    int ino = bitmap_find_first_zero(super.inode_map);
    // ! You should check if the ino is valid
    bitmap_reverse_bit(super.inode_map, ino); // mark the inode as used

    struct nfs_inode *inode = (struct nfs_inode *)malloc(sizeof(struct nfs_inode));
    inode->ino = ino;
    inode->ftype = dentry->ftype;
    inode->dentrys = NULL;
    inode->dir_cnt = 0;
    inode->size = 0;
    // ! dnos and data are not necessary set because of size is 0
    return inode;
}


// Sync inode to disk
int sync_inode(struct nfs_inode * inode)
{
    //TODO: Write the inode itself

    //TODO: If the inode is a directory, write the dentrys

    //TODO: Then recursively write the subdirectories

    //TODO: If the inode is a regular file, write the data

    return 0;
}

// Read inode from disk
struct nfs_inode* read_inode(uint32_t ino)
{
    // TODO: If the inode is a directory, read the names of the dentrys

    // TODO: If the inode is a regular file, read the data
}
