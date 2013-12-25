#include <stdlib.h>
#include <semaphore.h>
#include <pthread.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <assert.h>
#include <sys/mman.h>

#include "stp_fs.h"
#include "stp_error.h"
#include "slab.h"
#include "list.h"
#include "stp.h"
#include "rb_tree.h"

static umem_cache_t *fs_inode_slab = NULL; //size 32
static umem_cache_t *fs_inode_item_slab = NULL; //size 256
static umem_cache_t *fs_entry_slab = NULL;//for stp_fs_dir size 36

static struct stp_inode * __get_stp_inode(struct stp_fs_info *sb);

#define FS_ITEM_ENTRY (1UL <<0)

#define PAGE_SHIFT (12)
#define PAGE_MASK (~((1UL << PAGE_SHIFT) - 1))
#define PAGE_ALIGN(x) (((x) + PAGE_MASK) & ~PAGE_MASK)
#define PAGE_IS_ALIGN(x) ((PAGE_ALIGN(x) == x))

#define STP_FS_ENTRY_MMAP (1UL << 0)

static void __set_fs_header(struct stp_header *dest,const struct stp_header *src);
static void __set_entry_dirty(struct stp_fs_info *,struct stp_fs_entry *);

static void init_root(struct stp_fs_info *sb,struct stp_inode_item *_inode)
{
    struct stp_inode *inode;
    
    if(!(inode = __get_stp_inode(sb))) return;
    
    inode->item = _inode;
    
    if(!inode->item->ino) {
        inode->item->ino = 1;
        inode->ops->init(inode);
    }
    
    //location for root inode
    inode->item->location.offset = sizeof(struct stp_fs_super) - sizeof(struct stp_inode_item);
    inode->item->location.count = sizeof(struct stp_inode_item);
    
    init_rb_node(&inode->node,inode->item->ino);
    inode->item->mode |= S_IFDIR;
    rb_tree_insert(&sb->root,&inode->node);
    list_move(&sb->inode_list,&inode->list);
}


static int do_fs_super_init(struct stp_fs_info * super) 
{
    super->transid = 0;
    super->active = 0;
    sem_init(&super->sem,0,1);
    pthread_mutex_init(&super->mutex,NULL);
    list_init(&super->inode_list);
    list_init(&super->inode_lru);
    list_init(&super->dirty_list);
    list_init(&super->inode_list);
    list_init(&super->entry_dirty_list);
    list_init(&super->entry_list);
    init_rb_root(&super->root,NULL);

    if((fs_inode_slab = umem_cache_create("stp_inode_slab",\
        sizeof(struct stp_inode),ALIGN4,SLAB_NOSLEEP,NULL,NULL)) == NULL)    
        goto fail;
    
    if((fs_inode_item_slab = umem_cache_create("stp_inode_item_slab",\
                                               sizeof(struct stp_inode_item),ALIGN4,SLAB_NOSLEEP,NULL,NULL)) == NULL) {
        umem_cache_destroy(fs_inode_slab);
        goto fail;
    }

    if((fs_entry_slab = umem_cache_create("stp_entry_slab",\
                                               sizeof(struct stp_fs_entry),ALIGN4,SLAB_NOSLEEP,NULL,NULL)) == NULL) {
        umem_cache_destroy(fs_inode_slab);
        umem_cache_destroy(fs_inode_item_slab);
        goto fail;
    }

    if(super->mode & STP_FS_CREAT) {
        super->super->magic = STP_FS_MAGIC;
        super->super->flags = 0;
        super->super->total_bytes = FS_SUPER_SIZE;
        super->super->bytes_used = FS_SUPER_SIZE;
        super->super->bytes_hole  = 0;
        super->super->nritems = 1;
        super->super->ino = 1;
        super->super->nrdelete = 0;
        memset(&super->super->root,0,sizeof(struct stp_inode_item));
    }

    init_root(super,&super->super->root);
    
    super->offset = lseek(super->fd,0,SEEK_END);
    
    #ifdef FS_DEBUG
    printf("magic:%x,stp_inode size:%u,stp_inode_item size:%u,dir_item:%u,fs_entry:%u,dirent:%u,indirent:%u\n",\
           super->super->magic,sizeof(struct stp_inode),sizeof(struct stp_inode_item),sizeof(struct stp_dir_item),sizeof(struct stp_fs_entry),\
           sizeof(struct stp_fs_dirent),sizeof(struct stp_fs_indir));
    #endif

    return 0;
 fail:
    {
        stp_errno = STP_MALLOC_ERROR;
        sem_destroy(&super->sem);
        pthread_mutex_destroy(&super->mutex);
        return -1;
    }

}

static struct stp_inode * __get_stp_inode(struct stp_fs_info *super)
{
    struct stp_inode * inode = NULL;
    
    if(!(inode = umem_cache_alloc(fs_inode_slab)))
    {    
        stp_errno = STP_MALLOC_ERROR;
        return NULL;
    }
    
    memset(inode,0,sizeof(*inode));
    
    inode->flags = 0;
    inode->ref = 0;
    pthread_mutex_init(&inode->lock,NULL);
    list_init(&inode->lru);
    list_init(&inode->dirty);
    list_init(&inode->list);
    list_init(&inode->entry_list);
    list_init(&inode->child);
    list_init(&inode->sibling);
    inode->parent = NULL;
    //init_rb_node(&inode->node,1);
    inode->fs = super;
    inode->ops = &inode_operations;

    return inode;
}


static struct stp_inode * do_fs_super_allocate(struct stp_fs_info * super,off_t offset)
{
    struct stp_inode * inode = NULL;

    assert(offset >= 0);

    if((inode = __get_stp_inode(super)) == NULL) 
        return NULL;

    if(offset)
    {    
        if(super->ops->read(super,inode,offset)) {   
            umem_cache_free(fs_inode_slab,inode);
            return NULL;
        }
    }
    else {
    if(!(inode->item = umem_cache_alloc(fs_inode_item_slab))) {
        umem_cache_free(fs_inode_slab,inode);
        stp_errno = STP_INODE_MALLOC_ERROR;
        return NULL;
    }
    inode->flags = STP_FS_INODE_DIRTY | STP_FS_INODE_CREAT;
    
    memset(inode->item,0,sizeof(struct stp_inode_item));
    
    pthread_mutex_lock(&super->mutex);
    list_move(&super->dirty_list,&inode->dirty);
    inode->item->ino = ++super->super->ino;
    super->super->total_bytes += sizeof(struct stp_inode_item);
    super->super->bytes_used += sizeof(struct stp_inode_item);
    super->super->nritems ++;
    offset = super->offset;
    super->offset += sizeof(struct stp_inode_item);
    pthread_mutex_unlock(&super->mutex);

    inode->item->location.offset = offset;
    inode->item->location.count = sizeof(struct stp_inode_item);
    inode->item->location.flags = 0;
    inode->item->location.nritems = 0;

    #ifdef FS_DEBUG
    printf("%s:%d,inode:%p,flag:%d\n",__FUNCTION__,__LINE__,inode,\
           inode->flags);
    #endif

    inode->ops->init(inode);
    }

    init_rb_node(&inode->node,inode->item->ino);
    init_rb_root(&inode->root,NULL);

    pthread_mutex_lock(&super->mutex);

    rb_tree_insert(&super->root,&inode->node);
    super->active++;
    list_move(&super->inode_list,&inode->list);
    //lru replacement in here
    
    pthread_mutex_unlock(&super->mutex);
    
    return inode;
}

/*
 * allocate two pages for dentry of inode
 */
static int do_fs_super_alloc_pages(struct stp_fs_info *sb,struct stp_inode *inode,struct stp_fs_entry *entry)
{
    struct stp_header *location;

    assert(!entry->offset && !entry->size);
    assert(!(entry->flags & STP_FS_ENTRY_RB));

    pthread_mutex_lock(&sb->mutex);
    
    entry->offset = sb->offset;

    entry->size = 8*1024;

    sb->offset += entry->size;
    sb->super->bytes_used += entry->size;
    sb->super->total_bytes += entry->size;
    
    pthread_mutex_unlock(&sb->mutex);

    /*
    if(PAGE_IS_ALIGN(entry->offset)) {
        if((entry->entry = mmap(NULL,entry->size,PROT_READ|PROT_WRITE,MAP_SHARED,sb->fd,entry->offset)) == MAP_FAILED) 
            goto __fail;
        
    } else 
    */
        if((entry->entry = mmap(NULL,entry->size,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0)) == MAP_FAILED) 
            goto __fail;
    
    location = (struct stp_header *)entry->entry;
    location->offset = entry->offset;
    location->count = entry->size;
    location->flags = 0;
    location->nritems = 0;
    
    init_rb_node(&entry->node,entry->offset);
    rb_tree_insert(&inode->root,&entry->node);
    
    if(!(entry->flags & STP_FS_ENTRY_DIRTY)) {
        entry->flags |= STP_FS_ENTRY_DIRTY;
        list_move(&sb->entry_dirty_list,&entry->dirty);
    }

    //list_move(&sb->entry_list,&entry->list);
    
    return 0;
 __fail:
    entry->entry = NULL;
    stp_errno = STP_MALLOC_ERROR;

    pthread_mutex_lock(&sb->mutex);
    sb->offset -= entry->size;
    sb->super->bytes_used -= entry->size;
    sb->super->total_bytes -= entry->size;
    pthread_mutex_unlock(&sb->mutex);

    return -1;

}

static int do_fs_super_read_pages(struct stp_fs_info *sb,struct stp_fs_entry *entry)
{
    assert(entry->offset && entry->size);
    
    if(PAGE_IS_ALIGN(entry->offset))
    {
        if((entry->entry = mmap(NULL,entry->size,PROT_READ|PROT_WRITE,MAP_SHARED,sb->fd,entry->offset)) == MAP_FAILED) {
            stp_errno = STP_MALLOC_ERROR;
            return -1;
        }
        
        entry->flags |= STP_FS_ENTRY_MMAP;
        
    } else {
        if((entry->entry = mmap(NULL,entry->size,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0)) == MAP_FAILED) {
            stp_errno = STP_MALLOC_ERROR;
            return -1;
        }
        if((pread(sb->fd,entry->entry,entry->size,entry->offset)) != entry->size) {
            stp_errno = STP_META_READ_ERROR;
            return -1;
        }
    }
    
    

    //list_move(&sb->entry_list,&entry->list);
    return 0;
}

static int do_fs_super_rm_pages(struct stp_fs_info *sb,struct stp_fs_entry *entry)
{
    assert(entry->size && entry->offset && entry->entry);
    
    struct stp_header *location;
    
    pthread_mutex_lock(&sb->mutex);
    /*
    if(sb->offset == (entry->offset + entry->size)) {
        sb->offset = entry->offset;
        sb->super->total_bytes -= entry->size;
    } else {
    */
    sb->super->bytes_hole += entry->size;
    sb->super->bytes_used -= entry->size;
    //}
    
    pthread_mutex_unlock(&sb->mutex);
    
    location = (struct stp_header *)entry->entry;
    location->flags |= STP_HEADER_DELETE;
    __set_entry_dirty(sb,entry);

    return entry->ops->release(sb,entry);
}


static int do_fs_super_release_pages(struct stp_fs_info *sb,struct stp_fs_entry *entry)
{

    if(!entry->entry || !(entry->offset && entry->size))
        return 0;
    
    struct stp_inode *inode = entry->inode;
    
    if(entry->flags & STP_FS_ENTRY_DIRTY) {
        entry->ops->sync(sb,entry);
    }
    entry->flags &= ~STP_FS_ENTRY_DIRTY;
    
    list_del_element(&entry->list);
    list_del_element(&entry->lru);
    list_del_element(&entry->dirty);
    rb_tree_erase(&inode->root,&entry->node);
    
    entry->offset = 0;
    entry->size = 0;
    entry->flags = 0;
    
    munmap(entry->entry,entry->size);
    
    entry->entry = NULL;
    
    return 0;
}

static int do_fs_super_sync_pages(struct stp_fs_info *sb,struct stp_fs_entry *entry)
{
    if(!(entry->flags & STP_FS_ENTRY_DIRTY))
        return 0;
    /*
    if(PAGE_IS_ALIGN(entry->offset)) 
    {
        if(msync(entry->entry,entry->size,MS_ASYNC) < 0) {
            stp_errno = STP_META_WRITE_ERROR;
            return -1;
        }
        
    } else
    */
    {
        if(pwrite(sb->fd,entry->entry,entry->size,entry->offset) != entry->size) {
        stp_errno = STP_META_WRITE_ERROR;
        return -1;
        }
    }
    
    entry->flags &= ~STP_FS_ENTRY_DIRTY;
    
    list_del_element(&entry->dirty);
    
    return 0;
}

static int do_fs_super_free(struct stp_fs_info *sb)
{
    return -1;
}


static int do_fs_super_read(struct stp_fs_info * sb,struct stp_inode *inode,off_t offset)
{
    int res;
    size_t size;
    
    assert(offset > 0);
    //allcate stp_inode_item,then pread
    if(!(inode->item = umem_cache_alloc(fs_inode_item_slab))) {
        stp_errno = STP_INODE_MALLOC_ERROR;
        return -1;
    }
        
    res = pread(sb->fd,inode->item,sizeof(struct stp_inode_item),offset);
    if(res != sizeof(struct stp_inode_item)) {
        stp_errno = STP_META_READ_ERROR;
        return -1;
    }
        
    inode->flags = STP_FS_INODE_CREAT;
        
    return 0;
}

static int do_fs_super_sync(struct stp_fs_info *super)
{
    struct stp_inode *inode;
    
    list_for_each_entry(inode,&super->dirty_list,dirty) {
        super->ops->write(super,inode);
    }

    struct stp_fs_entry *entry;
    list_for_each_entry(entry,&super->entry_dirty_list,dirty) {
        entry->ops->sync(super,entry);
    }

    return -1;
}

static int do_fs_super_write(struct stp_fs_info *super,struct stp_inode *inode)
{
    int res;

    assert(inode->item != NULL);
    assert(inode->item->location.count > 0);
    assert(inode->item->location.offset > 0);
    
    inode->item->atime = time(NULL);
    
    res = pwrite(super->fd,inode->item,inode->item->location.count,inode->item->location.offset);
    if(res < 0) 
        stp_errno = STP_META_WRITE_ERROR;
    else 
    {
        inode->flags &= ~STP_FS_INODE_DIRTY;
        list_del_element(&inode->dirty);
    }
    
    return res;    
}

static int do_fs_super_destroy(struct stp_fs_info *super)
{
    struct stp_inode *inode,*next;

    /* flush dirty inode to disk */
    list_for_each_entry_del(inode,next,&super->dirty_list,dirty) {
        if(inode->item->ino != 1)
            super->ops->write(super,inode);
        list_del_element(&inode->dirty);
    }
    
    /** destroy bnode and flush it into disk*/
    fsync(super->fd);
    /** flush dirty dirent to disk */
    struct stp_fs_entry *dir,*ndir;
    list_for_each_entry_del(dir,ndir,&super->entry_dirty_list,dirty) {
        dir->ops->sync(super,dir);
        list_del_element(&dir->dirty);
    }
    
    /**free all inode in inode_list*/
    list_for_each_entry_del(inode,next,&super->inode_list,list)
    {
        //destroy dir function here with inode destroy
        super->ops->destroy_inode(super,inode);
    }

    /**destroy cache*/
    umem_cache_destroy(fs_inode_slab);
    umem_cache_destroy(fs_inode_item_slab);
    umem_cache_destroy(fs_entry_slab);
    sem_destroy(&super->sem);
    pthread_mutex_destroy(&super->mutex);
    return 0;
}

static void __set_fs_header(struct stp_header *dest,const struct stp_header *src)
{
    dest->offset = src->offset;
    dest->count = src->offset;
    dest->flags = src->flags;
    dest->nritems = src->nritems;
}

static void __set_entry_dirty(struct stp_fs_info *sb,struct stp_fs_entry *entry)
{
    if(entry->flags & STP_FS_ENTRY_DIRTY) return;
    
    entry->flags |= STP_FS_ENTRY_DIRTY;
    list_move(&sb->entry_dirty_list,&entry->dirty);    
}

static int do_fs_super_lookup(struct stp_fs_info *sb,struct stp_inode **_inode,u64 ino,off_t offset)
{
    struct rb_node *node;
    struct stp_inode *inode;
    

    node = rb_tree_find(&sb->root,ino);
    if(node) {
        inode = rb_entry(node,struct stp_inode,node);
        *_inode = inode;
        return 0;
    }

    if(!offset) {
        stp_errno = STP_INVALID_ARGUMENT;
        return -1;
    }
    
    if(!(inode = sb->ops->allocate(sb,offset)))
        return -1;
    
    *_inode = inode;
    
    return 0;
}

static int do_fs_super_find(struct stp_fs_info *sb,struct stp_inode **inode,u64 ino)
{
    struct stp_inode *_inode;
    struct rb_node *node;
    
    node = rb_tree_find(&sb->root,ino);
    
    if(node) {
        _inode = rb_entry(node,struct stp_inode,node);
        if(inode)
            *inode = _inode;
        return 0;
    }
    
    stp_errno = STP_FS_INO_NOEXIST;
    return -1;
}

static struct stp_fs_entry * do_fs_super_alloc_entry(struct stp_fs_info *sb,struct stp_inode *inode,off_t offset,size_t size,
                                                     struct stp_fs_entry *parent)
{
    struct stp_fs_entry *entry;
    
    if(!(entry = umem_cache_alloc(fs_entry_slab))) {
        stp_errno = STP_MALLOC_ERROR;
        return NULL;
    }
    
    memset(entry,0,sizeof(*entry));

    entry->inode = inode;
    entry->size = size;
    entry->offset = offset;
    list_init(&entry->list);
    list_init(&entry->lru);
    list_init(&entry->dirty);
    list_init(&entry->sibling);
    list_init(&entry->child);
    entry->parent = NULL;
    entry->ops = &fs_entry_operations;
    if(offset && size) 
    {
        init_rb_node(&entry->node,entry->offset);
        entry->flags = STP_FS_ENTRY_RB;
        rb_tree_insert(&inode->root,&entry->node);
    }
    
    list_move(&inode->entry_list,&entry->list);
    entry->parent = parent;

    if(parent)
        list_add_tail(&parent->child,&entry->sibling);
    
    return entry;
}

static int do_fs_super_free_pages(struct stp_fs_info *sb,struct stp_fs_entry *entry)
{
    /*
    struct stp_header *location = (struct stp_header *)entry->entry;
    
    pthread_mutex_lock(&sb->mutex);
    
    sb->super->bytes_hole += location->count;
    sb->super->bytes_used -= location->count;
    location->flags |= STP_HEADER_DELETE;
    
    pthread_mutex_unlock(&sb->mutex);
    
    entry->flags |= STP_FS_ENTRY_DIRTY;

    return entry->ops->release(sb,entry);
    */
    if(!entry->entry)
        return 0;
    
    return entry->ops->rm(sb,entry);

}

static int do_fs_super_free_inode(struct stp_fs_info *sb,struct stp_inode *inode)
{
    
    struct stp_header *location;
    
    assert(inode->item);
    if((inode->item->mode & S_IFDIR) && (inode->item->nritem != 0)) {
        stp_errno = STP_FS_DIR_NOEMPTY;
        return -1;
    }
    
    pthread_mutex_lock(&sb->mutex);
    
    sb->super->nritems --;
    /*
    if(sb->offset == (inode->item->location.offset + sizeof(struct stp_inode_item))) {
        sb->offset = inode->item->location.offset;
        sb->super->total_bytes -= sizeof(struct stp_inode_item);
    } else {
    */
    sb->super->bytes_hole += sizeof(struct stp_inode_item);
    sb->super->bytes_used -= sizeof(struct stp_inode_item);
        //}
    sb->super->nrdelete ++;
    
    pthread_mutex_unlock(&sb->mutex);
    
    location = &inode->item->location;
    location->flags |= STP_HEADER_DELETE;
    inode->flags |= STP_FS_INODE_DIRTY;
    if(inode->ops->free(inode) < 0)
        return -1;

    return sb->ops->destroy_inode(sb,inode);
}

static int do_fs_super_destroy_inode(struct stp_fs_info *sb,struct stp_inode *inode)
{
    
    inode->ops->destroy(inode);

    if(inode->flags & STP_FS_INODE_CREAT)
        umem_cache_free(fs_inode_item_slab,inode->item);
    else 
    {
        if(inode->item->ino != 1)
            munmap(inode->item,sizeof(struct stp_inode_item));
    }
    
    umem_cache_free(fs_inode_slab,inode);
    
    return 0;
}

static int do_fs_super_destroy_entry(struct stp_fs_info *sb,struct stp_fs_entry *entry)
{
    
    entry->ops->release(sb,entry);
    
    umem_cache_free(fs_entry_slab,entry);
    
    return 0;
}

static int do_fs_super_free_entry(struct stp_fs_info *sb,struct stp_fs_entry *entry)
{
    
    entry->ops->free(sb,entry);
    
    return entry->ops->destroy(sb,entry);

}


const struct stp_fs_operations stp_fs_super_operations = {
    .init = do_fs_super_init,
    .allocate = do_fs_super_allocate,
    .free_inode = do_fs_super_free_inode,
    .destroy_inode = do_fs_super_destroy_inode,
    .alloc_entry = do_fs_super_alloc_entry,
    .free_entry = do_fs_super_free_entry,
    .destroy_entry = do_fs_super_destroy_entry,
    .free = do_fs_super_free,
    .read = do_fs_super_read,
    .sync = do_fs_super_sync,
    .write = do_fs_super_write,
    .destroy = do_fs_super_destroy,
    .lookup = do_fs_super_lookup,
    .find = do_fs_super_find,
};


const struct stp_fs_entry_operations fs_entry_operations = {
    .alloc = do_fs_super_alloc_pages,
    .read = do_fs_super_read_pages,
    .rm = do_fs_super_rm_pages,
    .release = do_fs_super_release_pages,
    .free = do_fs_super_free_pages,
    .sync = do_fs_super_sync_pages,
    .destroy = do_fs_super_destroy_entry,
    .free_entry = do_fs_super_free_entry,
};

    
