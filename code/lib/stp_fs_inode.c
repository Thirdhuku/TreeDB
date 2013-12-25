#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <assert.h>

#include "stp_fs.h"
#include "stp_error.h"
#include "list.h"
#include "rb_tree.h"

static struct stp_fs_entry *__get_fs_entry(struct stp_fs_info *sb,struct stp_inode *inode,struct stp_fs_entry *);
static inline void __set_fs_header(struct stp_header *,const struct stp_header *);
static inline void __set_inode_dirty(struct stp_fs_info *sb,struct stp_inode *inode);
static inline void __set_entry_dirty(struct stp_fs_info *sb,struct stp_fs_entry *entry);

static inline int __empty_location(const struct stp_header *location);
static inline void __debug_entry(const struct stp_fs_dirent * ent);
static inline void __debug_indir(const struct stp_inode *,const struct stp_fs_indir *);
static int __rm_inode_entry(struct stp_inode *,struct stp_header *,struct stp_fs_entry *);
static int __rm_inode_indir(struct stp_inode *,struct stp_header *,struct stp_fs_entry *);
static struct stp_fs_entry* __do_read_entry(struct stp_inode *,const struct stp_header *,u32,struct stp_fs_entry *);
static inline int __equal_location(const struct stp_header *l1,const struct stp_header *l2);
static int __copy_inode_entry(struct stp_inode *inode,const struct stp_header *location,struct stp_dir_item *item,struct stp_fs_entry *pentry,size_t *len);
static int __copy_inode_indir(struct stp_inode *inode,const struct stp_header *location,struct stp_dir_item *item,struct stp_fs_entry *pentry,size_t *len);

static int __remove_inode_dir_item(struct stp_inode *,struct stp_fs_entry *,const struct stp_dir_item *);
static int __remove_inode_indir_item(struct stp_inode *,struct stp_fs_entry *,const struct stp_dir_item *);
static int __binary_search(const struct stp_dir_item*,u32 ,const char *);

static int do_fs_inode_setattr(struct stp_inode *inode)
{
    return -1;
}

static int do_fs_inode_init(struct stp_inode *inode)
{
    
    inode->item->size = 0;
    inode->item->nlink = 1;
    inode->item->uid = getuid();
    inode->item->gid = getgid();
    inode->item->mode = S_IRWXU|S_IRGRP|S_IROTH;
    inode->item->atime = time(NULL);
    inode->item->ctime = inode->item->atime;
    inode->item->mtime = inode->item->atime;
    inode->item->nritem = 0;

    return 0;
}

static int dir_item_cmp(const void *s1,const void *s2)
{
    struct stp_dir_item *it1,*it2;
    
    it1 = (struct stp_dir_item *)s1;
    it2 = (struct stp_dir_item *)s2;
    
    return strcmp(it1->name, it2->name);
}

static inline void __set_inode_dirty(struct stp_fs_info *sb,struct stp_inode *inode)
{
    if(!(inode->flags & STP_FS_INODE_DIRTY)) {
            inode->flags |= STP_FS_INODE_DIRTY;
            list_move(&sb->dirty_list,&inode->dirty);
        }
}


static int __location_entry_exist(const struct stp_header *location,struct stp_inode *parent,\
                                  const struct stp_dir_item *key,struct stp_fs_entry **_h,struct stp_fs_entry *pentry)
{
    struct stp_fs_info *sb = parent->fs;
    struct stp_fs_entry *entry;
    struct rb_node *node;
    struct stp_dir_item *origin;
    
    origin = NULL;
    
    if(!location)
        return -1;
    //search in rb tree
    if((node = rb_tree_find(&parent->root,location->offset))) {
        entry = rb_entry(node,struct stp_fs_entry,node);
    } else {
        if(!(entry = sb->ops->alloc_entry(sb,parent,location->offset,location->count,pentry))) {
            fprintf(stderr,"[ERROR]:cann't allocate memory.\n");
            return -1;
        }
    
        if(entry->ops->read(sb,entry) < 0) return -1;
        entry->flags |= STP_FS_ENTRY_DIRECT;
    }
    //search in entry
    struct stp_fs_dirent *ent;
    
    ent = (struct stp_fs_dirent *)entry->entry;
    
    //search in entry
    if((origin = bsearch(key,ent->item,ent->location.nritems,sizeof(struct stp_dir_item),dir_item_cmp))) {
        if(_h) *_h = entry;
        return 0;
    }
    
    stp_errno = STP_FS_ENTRY_NOEXIST;
    
    return -1;
}

static int __location_indir_exist(const struct stp_header *location,struct stp_inode *parent,const struct stp_dir_item *key,\
                                  struct stp_fs_entry **_h,struct stp_fs_entry *pentry)
{
    struct stp_fs_info *sb = parent->fs;
    struct stp_fs_entry *entry;
    struct rb_node *node;
    struct stp_fs_indir *in;
    int i,flags;
    
    if((node = rb_tree_find(&parent->root,location->offset))) {
        entry = rb_entry(node,struct stp_fs_entry,node);
    } else {
        if(!(entry = sb->ops->alloc_entry(sb,parent,location->offset,location->count,pentry))) {
            fprintf(stderr,"[ERROR]:cann't allocate memory for indirect entry.\n");
            return -1;
        }
        if(entry->ops->read(sb,entry) < 0) {
            fprintf(stderr,"[ERROR]:read pages for indirect entry.\n");
            return -1;
        }
        entry->flags |= STP_FS_ENTRY_INDIR1;
    }
    
    in = (struct stp_fs_indir *)entry->entry;
    for(i = 0;i < in->location.nritems;i++)
    {
        flags = __location_entry_exist(&in->index[i],parent,key,_h,entry);
        if(!flags) 
            return 0;
        
        if(flags < 0 && stp_errno != STP_FS_ENTRY_NOEXIST)
            return -1;
    }
    
    stp_errno = STP_FS_ENTRY_NOEXIST;
    
    return -1;
}


static int _do_fs_inode_exist(struct stp_inode *parent,u64 ino,const char *filename,size_t len,int *found,\
                              struct stp_fs_entry **_h)
{
    struct stp_fs_info *sb = parent->fs;
    struct stp_fs_entry *entry;
    struct rb_node *node;
    struct stp_header *location;
    struct stp_dir_item key;
    struct stp_fs_indir *in;
    int i,flags;
    
    assert(parent->item->mode & S_IFDIR);    
    *found = 0;
    
    memset(&key,0,sizeof(key));    
    key.ino = ino;
    key.name_len = len;
    strncpy(key.name,filename,len);

    //search in direct entry
    location = &parent->item->entry[0];
    
    if(!location || __empty_location(location)) return 0;

    if(!(flags = __location_entry_exist(location,parent,&key,_h,NULL))) {
        *found = 1;
        return 0;
    }

    if(flags < 0 && stp_errno != STP_FS_ENTRY_NOEXIST)
        return -1;

    //search in indirect entry
    location = &parent->item->entry[1];
    if(!location || __empty_location(location)) return 0;
    
    if(!__location_indir_exist(location,parent,&key,_h,NULL)) {
        *found = 1;
        return 0;
    }
    
    //search in 3-indirect entry entry
    location = &parent->item->entry[2];
    if(!location || __empty_location(location)) return 0;
 
   if((node = rb_tree_find(&parent->root,location->offset))) {
        entry = rb_entry(node,struct stp_fs_entry,node);
    } else {
       if(!(entry = sb->ops->alloc_entry(sb,parent,location->offset,location->count,NULL))) {
            fprintf(stderr,"[ERROR]:cann't allocate memory for indirect entry.\n");
            return -1;
        }
        if(entry->ops->read(sb,entry) < 0) {
            fprintf(stderr,"[ERROR]:read pages for indirect entry.\n");
            return -1;
        }  
        entry->flags |= STP_FS_ENTRY_INDIR2;
    }
 
   in = (struct stp_fs_indir *)entry->entry;
   for(i = 0;i< in->location.nritems;i++)
   {
       if(!__location_indir_exist(&in->index[i],parent,&key,_h,entry))
       {
           *found = 1;
           return 0;
       }
   }
   
   return 0;
}                 

static int do_fs_inode_lookup(struct stp_inode *inode,const char *filename,size_t len,u64 ino)
{
    int found;
    struct stp_dir_item *origin;
    
    if(_do_fs_inode_exist(inode,ino,filename,len,&found,NULL) < 0)
        return -1;
    
    if(!found) {
        stp_errno = STP_FS_ENTRY_NOEXIST;
        return -1;
    }
    
    return 0;
}

static void __copy_dir_item(struct stp_dir_item *dest,const struct stp_dir_item *src)
{
    memset(dest,0,sizeof(*dest));
    
    dest->ino = src->ino;
    dest->name_len = src->name_len;
    dest->flags = src->flags;
    strncpy(dest->name,src->name,src->name_len);
}


static int __do_fs_entry_insert(struct stp_inode *parent,const struct stp_dir_item *item,struct stp_header *location,struct stp_fs_entry *pentry)
{
    
    struct stp_fs_info *sb = parent->fs;
    struct stp_fs_entry *entry;
    //search in entry
    struct stp_fs_dirent *ent;
    
    if(!(entry = __do_read_entry(parent,location,STP_FS_ENTRY_DIRECT,pentry)))
        return -1;
    
    ent = (struct stp_fs_dirent *)entry->entry;
    
    if(ent->location.nritems == STP_FS_DIR_NUM) 
    {
        stp_errno = STP_FS_ENTRY_FULL;
        return -1;
    }
    
    __copy_dir_item(&ent->item[ent->location.nritems++],item);
    parent->item->nritem ++;
    if((&ent->location) != location)
        location->nritems ++;
    __set_inode_dirty(sb,parent);
    
    //must be sorted(sorted by ino)
    qsort(ent->item,ent->location.nritems,sizeof(*item),dir_item_cmp);
    __set_entry_dirty(sb,entry);
    
    #ifdef FS_DEBUG
    __debug_entry(ent);
    #endif

    return 0;
}

static inline int __ent_empty(const struct stp_header *location)
{
    if(!location) return 1;
    
    return location->offset == 0 && location->count == 0;
}


static int __do_fs_indir_insert(struct stp_inode *parent,const struct stp_dir_item *item,struct stp_header *header,struct stp_fs_entry *pentry)
{
    struct stp_fs_info *sb = parent->fs;
    struct stp_fs_entry *entry;
    struct stp_header *location;
    
    //search in entry
    struct stp_fs_indir *ent;

    if(!(entry = __do_read_entry(parent,header,STP_FS_ENTRY_INDIR1,pentry))) 
        return -1;

    ent = (struct stp_fs_indir *)entry->entry;

    int i = 0;
    
    if(ent->location.nritems == STP_FS_DIRENT_MAX && \
        ent->index[STP_FS_DIRENT_MAX - 1].nritems == STP_FS_DIR_NUM) {
        stp_errno = STP_FS_ENTRY_FULL;
        return -1;
    }
    
    while(i < ent->location.nritems && ent->index[i].nritems == STP_FS_DIR_NUM) {
        i++;
    }
    
    location = &ent->index[i];
    struct stp_fs_entry *it;
    //allocate direct entry,then insert into it
    if(__ent_empty(location)) {
        if(!(it = __get_fs_entry(sb,parent,entry)))
            return -1;
        __set_fs_header(location,(struct stp_header *)it->entry);
        __set_entry_dirty(sb,it);
        header->nritems ++;
        ent->location.nritems ++;
        it->flags |= STP_FS_ENTRY_DIRECT;
    } else {
        //find the entry
        struct rb_node *node;
        if(!(node = rb_tree_find(&parent->root,location->offset))) {
            fprintf(stderr,"[ERROR]:%s,%d,fail to lookup entry(%p)\n",__FUNCTION__,__LINE__,node);
            stp_errno = STP_FS_UNKNOWN_ERROR;
            return -1;
        }
        
        it = rb_entry(node,struct stp_fs_entry,node);
        if(!it->entry) {
            fprintf(stderr,"[ERROR]:%s,%d fail to get entry\n",__FUNCTION__,__LINE__);
            stp_errno = STP_FS_UNKNOWN_ERROR;
            return -1;
        }
    }
    
    
    int flags =  __do_fs_entry_insert(parent,item,(struct stp_header *)it->entry,it);
    if(!flags) {
        location->nritems ++;
        __set_entry_dirty(sb,it);
    }
    
    __set_entry_dirty(sb,entry);

    #ifdef FS_DEBUG
    __debug_indir(parent,ent);
    #endif
    
    return flags;
}

static int __do_fs_inode_insert(struct stp_inode *parent,const struct stp_dir_item *item)
{
    struct stp_fs_info  *sb = parent->fs;
    struct stp_fs_entry *entry;
    struct stp_header *location;
    s64 max;
    
    max = parent->item->nritem;

    if(max == U32_MAX) {
        stp_errno = STP_FS_ENTRY_FULL;
        return -1;
    }
    
    //insert in the direct dir
    max -= STP_FS_DIR_NUM;
    location = &parent->item->entry[0];
    if(__ent_empty(location)) {
        //allocate entry
        if(!(entry = __get_fs_entry(sb,parent,NULL))) 
            return -1;
        __set_fs_header(location,(struct stp_header *)entry->entry);
        __set_inode_dirty(sb,parent);
        entry->flags |= STP_FS_ENTRY_DIRECT;
    } else {
        //find the entry
        struct rb_node *node;
        if(!(node = rb_tree_find(&parent->root,location->offset))) {
            fprintf(stderr,"[ERROR]:%s,%d,fail to lookup entry(%p)\n",__FUNCTION__,__LINE__,node);
            stp_errno = STP_FS_UNKNOWN_ERROR;
            return -1;
        }
        
        entry = rb_entry(node,struct stp_fs_entry,node);
        if(!entry->entry) {
            fprintf(stderr,"[ERROR]:%s,%d fail to get entry\n",__FUNCTION__,__LINE__);
            stp_errno = STP_FS_UNKNOWN_ERROR;
            return -1;
        }
    }
    
    #ifdef FS_DEBUG
    printf("%s:%d,nritems:%d\n",__FUNCTION__,__LINE__,location->nritems);
    #endif

    if(max < 0 && location->nritems < STP_FS_DIR_NUM) {
        __set_entry_dirty(sb,entry);
        return __do_fs_entry_insert(parent,item,location,NULL);
    }
    
    
    //insert in the indirect dir
    location = &parent->item->entry[1];
    if(__ent_empty(location)) {
        //allocate entry
        if(!(entry = __get_fs_entry(sb,parent,NULL))) 
            return -1;
        __set_fs_header(location,(struct stp_header *)entry->entry);
        __set_inode_dirty(sb,parent);
        entry->flags |= STP_FS_ENTRY_INDIR1;
    }
    
    max -= STP_FS_DIR_NUM * STP_FS_DIRENT_MAX;
    
    if(max < 0 && location->nritems <= STP_FS_DIRENT_MAX) {
        __set_entry_dirty(sb,entry);
        return __do_fs_indir_insert(parent,item,location,NULL);
    }
    
    //insert in the in-indirect entry
    location = &parent->item->entry[2];
    if(__ent_empty(location)) {
        //allocate entry
        if(!(entry = __get_fs_entry(sb,parent,NULL))) 
            return -1;
        __set_fs_header(location,(struct stp_header *)entry->entry);
        __set_inode_dirty(sb,parent);
        entry->flags |= STP_FS_ENTRY_INDIR2;
    }
    if(location->nritems == STP_FS_DIRENT_MAX) {
        stp_errno = STP_FS_ENTRY_FULL;
        return -1;
    }
    
    int i = 0;
    struct stp_fs_indir *ent;
    
    ent = (struct stp_fs_indir *)entry->entry;

    int flags;
    while(i < location->nritems) {
        struct stp_header *l;
        
        l = &ent->index[i];
        if(__ent_empty(l)) {
        struct stp_fs_entry *item;
        if(!(item = __get_fs_entry(sb,parent,entry)))
            return -1;
        __set_fs_header(&ent->index[i],(struct stp_header *)item->entry);
        __set_entry_dirty(sb,entry);
        location->nritems ++;
        }
    
        __set_entry_dirty(sb,entry);
        flags  = __do_fs_indir_insert(parent,item,l,entry);
        if(!flags || (flags < 0 && stp_errno != STP_FS_ENTRY_FULL))
            return flags;
    }
    
}

static int do_fs_inode_mkdir(struct stp_inode *parent,const char *filename,size_t len,struct stp_inode *inode)
{
    struct stp_fs_info *sb = parent->fs;
    struct stp_dir_item item;
    
    if(!parent || !inode || len==0 || len > DIR_LEN) {
        stp_errno = STP_INVALID_ARGUMENT;
        return -1;
    }
    
    memset(&item,0,sizeof(item));
    item.ino = inode->item->ino;
    item.name_len = len;
    strncpy(item.name,filename,len);
    
    if(!(inode->item->mode & S_IFDIR)) {
        inode->item->mode |= S_IFDIR;
        __set_inode_dirty(sb,inode);
    }
    
    return  __do_fs_inode_insert(parent,&item);

}

//remove entry from  parent inode
static int do_fs_inode_rm(struct stp_inode *parent,const char *filename,size_t len,u64 *ino)
{
    struct stp_fs_info *sb = parent->fs;
    struct stp_fs_entry *entry,*pentry;
    int found,i;

    if(_do_fs_inode_exist(parent,0,filename,len,&found,&entry) < 0)
        return -1;
    if(!found) 
        return -1;
    if(!(entry->flags & STP_FS_ENTRY_DIRECT)) {
        stp_errno = STP_INVALID_ARGUMENT;
        return -1;
    }
    
    struct stp_fs_dirent *ent = (struct stp_fs_dirent *)entry->entry;
    
    //delete the corresponding entry
    if((i = __binary_search(ent->item,ent->location.nritems,filename)) < 0) {
        stp_errno = STP_FS_ENTRY_NOEXIST;
        return -1;
    }
    
    assert(i != ent->location.nritems);
    
    *ino = ent->item[i].ino;
    
    //move the entry forward
    while(i < ent->location.nritems - 1) {
        __copy_dir_item(&ent->item[i],&ent->item[i+1]);
        i++;
    }
    
    ent->location.nritems --;
    parent->item->nritem --;
    
    __set_entry_dirty(sb,entry);
    __set_inode_dirty(sb,parent);
    /*
     * remove the correspoding entry 
     * from parent
     */
    pentry = entry->parent;
    
    if(!pentry) return 0;
    /*
    if(!__equal_location(&parent->item->entry[0],(struct stp_header *)pentry->entry)) {
        stp_errno = STP_FS_UNKNOWN_ERROR;
        return -1;
    }
    */
    
    struct stp_fs_indir* fi = (struct stp_fs_indir *)pentry->entry;
    fi->location.nritems --;
    __set_entry_dirty(sb,pentry);

    return 0;
}

static int do_fs_inode_create(struct stp_inode *parent,const char *filename,size_t len,struct stp_inode *inode,mode_t mode)
{
    struct stp_fs_info *sb = parent->fs;
    struct stp_dir_item item;
    int flags;
    
    if(!parent || !inode || len ==0 || len > DIR_LEN) {
        stp_errno = STP_INVALID_ARGUMENT;
        return -1;
    }

    flags = parent->ops->lookup(parent,filename,len,inode->item->ino);
    
    if(!flags) {
        stp_errno = STP_FS_ENTRY_EXIST;
        return -1;
    }
    
    if((flags<0) && (stp_errno != STP_FS_ENTRY_NOEXIST) )
        return -1;

    memset(&item,0,sizeof(item));
    item.ino = inode->item->ino;
    item.name_len = len;
    strncpy(item.name,filename,len);
    
    inode->item->mode = mode;
    __set_inode_dirty(sb,inode);
    

    return __do_fs_inode_insert(parent,&item);
}

static int do_fs_inode_destroy(struct stp_inode *inode)
{
    struct stp_fs_info *sb = inode->fs;
    struct stp_fs_entry *dir,*ndir;
    
    //destroy all entry and then itself
    list_for_each_entry_del(dir,ndir,&inode->entry_list,list) {
        dir->ops->destroy(sb,dir);
        list_del_element(&dir->list);        
    }
    
    //if dirty must be flush
    if(inode->flags & STP_FS_INODE_DIRTY) {
        sb->ops->write(sb,inode);
        inode->flags &= ~STP_FS_INODE_DIRTY;
    }
    
    list_del_element(&inode->lru);
    list_del_element(&inode->dirty);
    list_del_element(&inode->list);
    list_del_element(&inode->sibling);
    list_del_element(&inode->child);
    
    rb_tree_erase(&sb->root,&inode->node);
    pthread_mutex_destroy(&inode->lock);
    
    return 0;
}

static int do_fs_inode_free(struct stp_inode *inode)
{
    struct stp_fs_info *sb = inode->fs;
    struct stp_fs_entry *entry;

    if(!(inode->item->mode & S_IFDIR))
        return 0;
    /*
     * free all data associated with inode
     */
    struct stp_header *location;
    
    //free direct entry
    location = &inode->item->entry[0];
    if(!__ent_empty(location)) {
        if(__rm_inode_entry(inode,location,NULL) < 0)
            return -1;
    }
    
    //free indirect entry
    location = &inode->item->entry[1];
    if(!__ent_empty(location)) {
        if(__rm_inode_indir(inode,location,NULL) < 0)
            return -1;
        //free the last entry
        if(__rm_inode_entry(inode,location,NULL) < 0)
            return -1;
    }
    
    //free in-indirect entry
    location = &inode->item->entry[2];
    if(__ent_empty(location))
        goto _last;
    
    
    int i = 0;
    struct stp_fs_indir *ent;
    
    if(!(entry = __do_read_entry(inode,location,\
                                 STP_FS_ENTRY_INDIR2,NULL)))
        return -1;
    ent = (struct stp_fs_indir *)entry->entry;
    while(i < location->nritems) {
        struct stp_header *l;
        
        l = &ent->index[i];
        if(!__ent_empty(l)) {
            if(__rm_inode_indir(inode,l,entry) < 0)
                return -1;
        }
        //free the last entry
        if(__rm_inode_entry(inode,l,entry) < 0)
            return -1;
        i++;
    }
    
    //free the last entry
    if(__rm_inode_entry(inode,location,NULL) < 0)
        return -1;

 _last:
    //return inode->ops->destroy(inode);
    return 0;
}

static int do_fs_inode_unlink(struct stp_inode *inode)
{
    struct stp_fs_info *sb = inode->fs;
    
    assert(inode->item);
    
    if(inode->item->nlink > 1) {
        inode->item->nlink --;
        __set_inode_dirty(sb,inode);
        return 0;
    }
    
    return sb->ops->free_inode(sb,inode);
}

static int do_fs_inode_readdir(struct stp_inode *inode,struct stp_dir_item *item)
{
    struct stp_fs_entry *entry;
    struct stp_fs_dirent *ent;
    struct stp_header *location;    
    int i;
    size_t len,num;
    
    i = 0;
    num = 0;
    len = 0;
    
    //copy direct dir entry
    location = &inode->item->entry[0];
    if(__copy_inode_entry(inode,location,item,NULL,&len) < 0)
        return -1;
    
    num += len;
    len = 0;
    //copy indirect dir entry
    location = &inode->item->entry[1];
    if(__copy_inode_indir(inode,location,item+num,NULL,&len) < 0)
        return -1;
    
    //copy in-indirect dir entry
    location = &inode->item->entry[2];
    if(!location || __ent_empty(location))
        return 0;
    
    struct stp_fs_indir *in;
    
    if(!(entry = __do_read_entry(inode,location,STP_FS_ENTRY_INDIR2,NULL)) < 0) 
        return -1;
    
    in = (struct stp_fs_indir *)entry->entry;
    
    for(i = 0;i < in->location.nritems;i++) {
        len = 0;
        if(__copy_inode_indir(inode,&in->index[i],\
                              item+num,entry,&len) < 0)
            return -1;
        num += len;
    }
    
    return 0;
}

static struct stp_dir_item*  do_fs_inode_find_entry(struct stp_inode *parent,const char *filename,size_t len)
{
    struct stp_fs_entry *entry;
    int found,i;
    
    if(_do_fs_inode_exist(parent,0,filename,len,&found,&entry) < 0)
        return NULL;
    if(!found)
        return NULL;
    if(!(entry->flags & STP_FS_ENTRY_DIRECT)) {
        stp_errno = STP_INVALID_ARGUMENT;
        return NULL;
    }
    
    struct stp_fs_dirent *ent = (struct stp_fs_dirent *)entry->entry;
    
    if((i = __binary_search(ent->item,ent->location.nritems,filename)) < 0) {
        stp_errno = STP_FS_ENTRY_NOEXIST;
        return NULL;
    }
    
    
    return &ent->item[i];
}


static struct stp_fs_entry* __get_fs_entry(struct stp_fs_info *sb,struct stp_inode *inode,struct stp_fs_entry *pentry)
{
    struct stp_fs_entry *entry;
    
    if(!(entry = sb->ops->alloc_entry(sb,inode,0,0,pentry))) return NULL;
    if(entry->ops->alloc(sb,inode,entry) < 0) {
        sb->ops->free_entry(sb,entry);
        return NULL;
    }
    
    return entry;
}


static void __set_fs_header(struct stp_header *dest,const struct stp_header *src)
{
    dest->offset = src->offset;
    dest->count = src->count;
    dest->flags = src->flags;
    dest->nritems = src->nritems;
}

static inline void __set_entry_dirty(struct stp_fs_info *sb,struct stp_fs_entry *entry)
{
    if(entry->flags & STP_FS_ENTRY_DIRTY) return;
    
    entry->flags |= STP_FS_ENTRY_DIRTY;
    list_move(&sb->entry_dirty_list,&entry->dirty);    
}

static inline int __empty_location(const struct stp_header *location)
{
    return __ent_empty(location);
    
}

static inline void __debug_indir(const struct stp_inode *parent,const struct stp_fs_indir *ent)
{
    int i;
    struct stp_fs_entry *entry;
    struct rb_node *node;
    
    printf("debug indirectory offset:%llu,count:%llu,%p\n",ent->location.offset,ent->location.count,ent);
    
    if(!ent) {
        printf("[ERROR]:%s,%d ent is null!\n",__FUNCTION__,__LINE__);
        return;
    }
    
    for(i = 0;i < ent->location.nritems;i++)
    {
        if(!(node = rb_tree_find(&parent->root,ent->index[i].offset))) {
            printf("[ERROR]:%s,%d indir offset:%llu(%d) is null\n",__FUNCTION__,__LINE__,ent->index[i].offset,i);
            return;
        }
        
        entry = rb_entry(node,struct stp_fs_entry,node);
        if(!entry->entry) {
            printf("[ERROR]:%s,%d,entry(%p) entry is null\n",__FUNCTION__,__LINE__,entry);
            return;
        }
        
        __debug_entry((struct stp_fs_dirent*)entry->entry);
    }   
}


static inline void __debug_entry(const struct stp_fs_dirent * ent)
{
    int i;
    printf("%s:%d,ent(%p),nritems:%d,offset:%llu\n",__FUNCTION__,\
           __LINE__,ent,ent->location.nritems,ent->location.offset);
    
    for(i = 0;i<ent->location.nritems;i++)
    {
        printf("ent[%d],ino:%llu,name:%s\n",i,ent->item[i].ino,\
               ent->item[i].name);
    }
    
}

/*
 * mark the entry delete.
 */
static int __rm_inode_entry(struct stp_inode *inode,struct stp_header *location,struct stp_fs_entry *pentry)
{   
    struct stp_fs_entry *entry;
    struct stp_fs_dirent *ent;
    struct stp_fs_info *sb = inode->fs;
    
    if(!(entry = __do_read_entry(inode,location,STP_FS_ENTRY_DIRECT,pentry)))
        return -1;
    
    ent = (struct stp_fs_dirent *)entry->entry;
    if(ent->location.nritems != 0) {
        stp_errno = STP_FS_DIR_NOEMPTY;
        return -1;
    }
    
    return entry->ops->rm(sb,entry);
}

static int __rm_inode_indir(struct stp_inode *inode,struct stp_header *location,struct stp_fs_entry *pentry)
{
    struct stp_fs_entry *entry;
    struct stp_fs_indir *ent;
    struct stp_fs_info *sb = inode->fs;
    
    if(!(entry = __do_read_entry(inode,location,STP_FS_ENTRY_INDIR1,pentry)))
        return -1;
    
    ent = (struct stp_fs_indir *)entry->entry;
    int i = 0;
    
    for(i = 0;i < ent->location.nritems;i++) {
        if(ent->index[i].nritems != 0) {
            stp_errno = STP_FS_DIR_NOEMPTY;
            return -1;
        }
        
        if(__rm_inode_entry(inode,&ent->index[i],entry) < 0)
            return -1;
     
        location->nritems --;
    }
}

static struct stp_fs_entry * __do_read_entry(struct stp_inode *parent,const struct stp_header *location,u32 flags,struct stp_fs_entry *pentry)
{
    struct stp_fs_info *sb = parent->fs;
    struct stp_fs_entry *entry;
    struct rb_node *node;
    
    if((node = rb_tree_find(&parent->root,location->offset))) {
        entry = rb_entry(node,struct stp_fs_entry,node);
    } else {
        if(!(entry = sb->ops->alloc_entry(sb,parent,location->offset,location->count,pentry))) {
            fprintf(stderr,"[ERROR]:cann't allocate  memory in %s\n",__FUNCTION__);
            return NULL;
        }
        
        if(entry->ops->read(sb,entry) < 0) return NULL;
        entry->flags |= flags;
    }

    return entry;
}

static inline int __equal_location(const struct stp_header *l1,const struct stp_header *l2)
{
    return (l1->offset == l2->offset) && (l1->count == l2->count);
}

static int __copy_inode_entry(struct stp_inode *inode,const struct stp_header *location,struct stp_dir_item *item,struct stp_fs_entry *pentry,size_t *num)
{
    struct stp_fs_entry *entry;
    struct stp_fs_dirent *ent;
    int i;

    *num = 0;
    if(!location || __ent_empty(location))
        return 0;
    
    if(!(entry = __do_read_entry(inode,location,STP_FS_ENTRY_DIRECT,pentry)))
        return -1;
        
    ent = (struct stp_fs_dirent *)entry->entry;
    i = 0;
    while(i < ent->location.nritems) {
        memcpy(&item[i],&ent->item[i],sizeof(struct stp_dir_item));
        i++;
    }
    
    *num = i;
    
    return 0;
}

static int __copy_inode_indir(struct stp_inode *inode,const struct stp_header *location,struct stp_dir_item *item,struct stp_fs_entry *pentry,size_t *len)
{
    struct stp_fs_entry *entry;
    struct stp_fs_dirent *ent;
    struct stp_fs_indir *in;
    int i;
    size_t num = 0,count = 0;
    
    *len = 0;
    
    if(!location || __ent_empty(location))
        return 0;
    
    if(!(entry = __do_read_entry(inode,location,STP_FS_ENTRY_INDIR1,pentry)))
        return -1;
    
    in = (struct stp_fs_indir *)entry->entry;
    for(i = 0;i < in->location.nritems;i++) {
        count = 0;
        if(__copy_inode_entry(inode,&in->index[i],item + num,entry,&count) < 0)
            return -1;
        num += count;
    }

    return 0;
}

static int __remove_inode_dir_item(struct stp_inode *inode,struct stp_fs_entry *entry,const struct stp_dir_item *item)
{
    stp_errno = STP_NO_SYSCALL;
    return -1;
}

static int __remove_inode_indir_item(struct stp_inode *inode,struct stp_fs_entry *entry,const struct stp_dir_item *item)
{
 
    stp_errno = STP_NO_SYSCALL;

    return -1;
}


static int __binary_search(const struct stp_dir_item *v,u32 len,const char *item)
{
    int mid,left,right,flag;
    
    left = 0;
    right = len - 1;
    
    while(left <= right) {
        mid = (left+right)/2;
        if((flag = strcmp(v[mid].name,item)) < 0)
            left = mid + 1;
        else if(flag == 0) return mid;
        else right = mid - 1;
    }
    
    return -1;
}


const struct stp_inode_operations inode_operations = {
    .init = do_fs_inode_init,
    .setattr = do_fs_inode_setattr,
    .mkdir = do_fs_inode_mkdir,
    .rm = do_fs_inode_rm,
    .creat = do_fs_inode_create,
    .readdir = do_fs_inode_readdir,
    .destroy = do_fs_inode_destroy,
    .free = do_fs_inode_free,
    .lookup = do_fs_inode_lookup,
    .unlink = do_fs_inode_unlink,
    .find_entry = do_fs_inode_find_entry,
};
