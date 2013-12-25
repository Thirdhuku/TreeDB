
/*
 * stp read/write function implementation
 *
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "stp_fs.h"
#include "stp_error.h"
#include "stp.h"
#include "stp_internal.h"

static inline void __copy_stat(struct stat *dest,const struct stp_inode_item *item);

int stp_stat(STP_FILE pfile,u64 ino,struct stat *buf)
{
    struct stp_fs_info * fs = pfile->fs;
    struct stp_btree_info *btree = pfile->tree;
    struct stp_bnode_off off;
    struct stp_inode *inode;

    int ret;
    
    if(!buf) {
        stp_errno = STP_INVALID_ARGUMENT;
        return -1;
    }
    
    memset(&off,0,sizeof(off));
    if(ino == 1) {
        __copy_stat(buf,&fs->super->root);
        return 0;
    } 
    ret = btree->ops->search(btree,ino,&off);
    if(ret < 0) return ret;
    /*
     * read from fs 
     *
         */
    if(fs->ops->lookup(fs,&inode,off.ino,off.offset) < 0)
        return -1;
    
    printf("%s:%d,ino:%llu(%llu),offset:%llu,size:%llu,ret:%d\n",__FUNCTION__,__LINE__,off.ino,ino,off.offset,off.len,ret);
    __copy_stat(buf,inode->item);
    return 0;
    
}

int stp_readdir(STP_FILE file,u64 ino,dir_t *items,u32 len)
{
    struct stp_fs_info *fs;
    struct stp_btree_info *tree;
    struct stp_inode *inode;
    int i;
    
    if(!file) {
        stp_errno = STP_INVALID_ARGUMENT;
        return -1;
    }
    
    fs = file->fs;
    tree = file->tree;
    inode = NULL;
    
    if((!(tree->mode & STP_FS_RDWR)) && (!(tree->mode & STP_FS_READ))) {
        stp_errno = STP_INDEX_CANT_BE_READER;
        return -1;
    }

    if(__fs_read_inode(fs,ino,tree,&inode) < 0)
        return -1;

    if(!(inode->item->mode & S_IFDIR)) {
        stp_errno = STP_FS_NO_DIR;
        return -1;
    }

    /*
      
    memset(&off,0,sizeof(off));
    
    if(ino != 1) {
        if(tree->ops->search(tree,ino,&off) < 0)
            return -1;
    } else {
        off.ino = 1;
        off.offset = sizeof(struct stp_fs_super) - sizeof(struct stp_inode_item);
    }
    
    
    if(fs->ops->lookup(fs,&inode,off.ino,off.offset) < 0)
        return -1;
    */

    struct stp_dir_item *item;
    
    if(!(item = calloc(inode->item->nritem,sizeof(struct stp_dir_item)))) {
        stp_errno = STP_INODE_MALLOC_ERROR;
        return -1;
    }    

    if(inode->ops->readdir(inode,item) < 0) 
    {
        free(item);
        return -1;
    }
    
    #ifdef DEBUG
    
    fprintf(stderr,"readdir in here %s\n",__FUNCTION__);
    
    for(i = 0;i < inode->item->nritem;++i) {
        fprintf(stderr,"ino:%llu,name:%s\n",item[i].ino,item[i].name);
    }
    
    #endif
    
    if(!item || !len) {
        free(item);
        return 0;
    }
    
    for(i = 0;i < inode->item->nritem && i < len;++i) {
        memcpy(&items[i],&item[i],sizeof(*items));
    }
    
    free(item);
    return 0;
}


static inline void __copy_stat(struct stat *dest,const struct stp_inode_item *item)
{
    dest->st_ino = item->ino;
    dest->st_size = item->size;
    dest->st_nlink = item->nlink;
    dest->st_uid = item->uid;
    dest->st_gid = item->gid;
    dest->st_mode = item->mode;
    dest->st_atime = item->atime;
    dest->st_ctime = item->ctime;
    dest->st_mtime = item->mtime;
    dest->st_dev = 0;
    dest->st_rdev = 0;
    dest->st_blocks = 0;
    dest->st_blksize = 8*1024;
}

