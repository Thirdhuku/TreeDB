#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <dirent.h>

#include "stp_fs.h"
#include "stp_error.h"
#include "stp.h"
#include "rb_tree.h"
#include "stp_internal.h"

static int read_fs_info(int ffd,struct stp_fs_info **,unsigned int mode);
static int read_btree_info(int bfd,struct stp_btree_info **,unsigned int mode);
static int __fs_info_insert(struct stp_fs_info *,u64 pino,struct stp_dir_item *,struct stp_bnode_off *,mode_t);
static int __btree_info_insert(struct stp_btree_info *,const struct stp_bnode_off *);
static int __btree_info_unlink(struct stp_btree_info *,const struct stp_bnode_off *);
static int __fs_info_unlink(struct stp_fs_info *,struct stp_inode *inode,const char *name,struct stp_bnode_off *off,int *flag);
static int __fs_info_mkdir(struct stp_fs_info *,u64 pino,struct stp_dir_item *,struct stp_bnode_off *,mode_t );
//static int __fs_read_inode(struct stp_fs_info *,u64 pino,struct stp_btree_info *,struct stp_inode **);

static int stp_check(const struct stp_fs_info *fs,const struct stp_btree_info *btree)
{
    if(fs->super->magic != btree->super->magic )
    {
        stp_errno = STP_BAD_MAGIC_NUMBER;
        return -1;
    }
    
    struct stat stbuf;
    
    //check meta file size
    if((fstat(fs->fd,&stbuf) < 0) || (fs->super->total_bytes != stbuf.st_size)) {
        fprintf(stderr,"total_bytes:%llu,size:%lu\n",fs->super->total_bytes,stbuf.st_size);
        stp_errno = STP_META_FILE_CHECK_ERROR;
        return -1;
    }
    
    //check index file size
    
    if((fstat(btree->fd,&stbuf) < 0)) {
        fprintf(stderr,"btree total_bytes:%llu,size:%lu\n",btree->super->total_bytes,stbuf.st_size);
        stp_errno = STP_INDEX_FILE_CHECK_ERROR;
        return -1;
    }
    
    return 0;
}


STP_FILE stp_open(const char *ffile,const char *bfile,unsigned int mode)
{
    int ffd,bfd;
    mode_t m = O_RDWR;
    struct stat stf,stb;
    unsigned int flags = 0;
    
    mode &= ~STP_FS_CREAT;

    if((stat(ffile,&stf) < 0) || (stat(bfile,&stb) < 0)) {
        mode |= STP_FS_CREAT;
        fprintf(stderr,"[WARNING] can't find the index or fs file.\n");
    }
    
    if(mode & STP_FS_CREAT) {   
        m |= O_CREAT;
        mode |= STP_FS_RDWR;
    }
    
    if((ffd = open(ffile,m,S_IRWXU|S_IRGRP|S_IROTH)) < 0) {
        stp_errno = STP_META_OPEN_ERROR;
        return NULL;
    }
    
    if((bfd = open(bfile,m,S_IRWXU|S_IRGRP|S_IROTH)) < 0) {
        stp_errno = STP_INDEX_OPEN_ERROR;
        return NULL;
    }                                                          
     
    STP_FILE_INFO *pfile = (STP_FILE_INFO *)calloc(1,sizeof(STP_FILE_INFO));
    if(!pfile) {
        stp_errno = STP_MALLOC_ERROR;
        return NULL;
    }
    
    struct stp_fs_info *fs = NULL;
    struct stp_btree_info *tree = NULL;
    
    
    if(read_fs_info(ffd,&fs,mode) < 0) {
        free(pfile);
        return NULL;
    }
    pfile->fs = fs;
    pfile->fs->filename = ffile;
    
    if(read_btree_info(bfd,&tree,mode) < 0) {
        if((void *)&fs->super) 
            munmap(&fs->super,FS_SUPER_SIZE);
        free(fs);
        free(pfile);
        return NULL;
    }

    pfile->tree = tree;
    pfile->tree->filename = bfile;
    
    if(stp_check(pfile->fs,pfile->tree))
        return NULL;
    
    return pfile;
}

static int read_fs_info(int ffd,struct stp_fs_info ** _fs,unsigned int mode)
{
    struct stp_fs_info *fs = NULL;
    void *addr;
    
    if(!(fs = (struct stp_fs_info *)calloc(1,sizeof(struct stp_fs_info)))) {
        stp_errno = STP_MALLOC_ERROR;
        return -1;
    }
    
    fs->ops = &stp_fs_super_operations;
    
    if((mode & STP_FS_CREAT) && ftruncate(ffd,FS_SUPER_SIZE) < 0)
    {       
        stp_errno = STP_META_CREAT_ERROR;
        return -1;
    }
    
    //read from file
    if((addr = mmap(NULL,FS_SUPER_SIZE,PROT_READ|PROT_WRITE,MAP_SHARED|MAP_LOCKED,ffd,0)) == MAP_FAILED) {
        stp_errno = STP_MALLOC_ERROR;
        free(fs);
        return -1;
    }

    if(mode & STP_FS_CREAT)
        memset(addr,0,FS_SUPER_SIZE);
    
    fs->super = (struct stp_fs_super *)addr;
    fs->mode = mode;
    fs->fd = ffd;
    
    if(fs->ops->init(fs) < 0) {
        munmap(fs->super,FS_SUPER_SIZE);
        stp_errno = STP_MALLOC_ERROR;
        free(fs);
        return -1;
    }
    *_fs = fs;
    return 0;
}

static int read_btree_info(int bfd,struct stp_btree_info ** _btree,unsigned int mode)
{
    struct stp_btree_info *btree = NULL;
    void *addr;

    //申请一个内存btree空间
    if(!(btree = (struct stp_btree_info *)calloc(1,sizeof(struct stp_btree_info)))) {
        stp_errno = STP_MALLOC_ERROR;
        return -1;
    }

    //memset(btree,0,sizeof(struct stp_btree_info));

	//设置好btree的操作函数集
    btree->ops = &stp_btree_super_operations;

    //如果是新创建db则截断到superblock大小
    if((mode & STP_FS_CREAT) && ftruncate(bfd,BTREE_SUPER_SIZE) < 0) {
        stp_errno = STP_INDEX_CREAT_ERROR;
        return -1;
    }

    //将db文件的超级块部分映射到内存中
    if((addr = mmap(NULL,BTREE_SUPER_SIZE,PROT_READ|PROT_WRITE,\
        MAP_SHARED|MAP_LOCKED,bfd,0)) == MAP_FAILED) {
        stp_errno = STP_MALLOC_ERROR;
        free(btree);
        return -1;
    }

	//如果新创建，则清空超级块部分
    if(mode & STP_FS_CREAT)
        memset(addr,0,BTREE_SUPER_SIZE);

	//内存btree设置superblock指针
    btree->super = (struct stp_btree_super *)addr;

    #ifdef DEBUG
    if(!(mode & STP_FS_CREAT))
    	printf("OPEN:%s:btree->super:%p,addr:%p,flags:%d\n",__FUNCTION__,btree->super,addr,btree->super->root.flags);
    else
      	printf("CREAT:%s:btree->super:%p,addr:%p,root:%p,flags:%p\n",__FUNCTION__,btree->super,addr,&btree->super->root,&btree->super->root.flags);
    #endif

	//设置内存btree的打开mode和文件fd
    btree->mode = mode;
    btree->fd = bfd;

    //初始化内存btree
    if(btree->ops->init(btree) < 0) {
        munmap(btree->super,BTREE_SUPER_SIZE);
        stp_errno = STP_MALLOC_ERROR;
        free(btree);
        return -1;
    }

	//回传输出参数
    *_btree = btree;
    return 0;
}



int stp_close(STP_FILE pfile)
{
    struct stp_fs_info * fs = pfile->fs;
    struct stp_btree_info *btree = pfile->tree;
    
    #ifdef DEBUG
    printf("%s:%d b+ tree:\n",__FUNCTION__,__LINE__);
    #endif
    //btree->ops->debug_btree(btree);
    

    fs->ops->destroy(fs);
    fsync(fs->fd);
    munmap(fs->super,FS_SUPER_SIZE);
    close(fs->fd);
    free(fs);
    //btree->ops->debug_btree(btree);
    btree->ops->destroy(btree);

    #ifdef DEBUG
    printf("__function__:%s,flags:%d,nrkeys:%d\n",__FUNCTION__,btree->super->root.flags,btree->super->nritems);
    #endif

    fsync(btree->fd);
    msync(btree->super,BTREE_SUPER_SIZE,MS_SYNC);
    munmap(btree->super,BTREE_SUPER_SIZE);
    close(btree->fd);
    free(btree);
    
    free(pfile);
    
    return 0;
}

/**
  * create a file.
  */
int stp_creat(STP_FILE file,const char *filename,mode_t mode)
{
  struct stp_fs_info *fs;
  struct stp_btree_info *tree;
  struct stp_bnode_off off;
  struct stp_dir_item item;
  int flags;
  u64 pino = 1;

  if(!file || !filename || strlen(filename) > DIR_LEN) {
      stp_errno = STP_INVALID_ARGUMENT;
      return -1;
  }

  
  fs = file->fs;
  tree = file->tree;

  if(!(tree->mode & STP_FS_RDWR)) {
      stp_errno =  STP_INDEX_CANT_BE_WRITER;
      return -1;
  }

  //read parent inode
  if(__fs_read_inode(fs,pino,tree,NULL) < 0) 
      return -1;


  memset(&item,0,sizeof(item));
  
  item.name_len = strlen(filename);
  strncpy(item.name,filename,item.name_len);
  
  memset(&off,0,sizeof(off));
  
  flags = __fs_info_insert(fs,pino,&item,&off,mode);
  if(flags < 0) return -1;
  flags =  __btree_info_insert(tree,&off);

  return flags;
}

int stp_unlink(STP_FILE file,u64 pino,const char *filename)
{
    struct stp_fs_info *fs;
    struct stp_btree_info *tree;
    struct stp_bnode_off off;
    struct stp_inode *inode;    

    static u64 num = 1;
    int flags;
    
    if(!file) {
      stp_errno = STP_INVALID_ARGUMENT;
      return -1;
  }

  fs = file->fs;
  tree = file->tree;
  
  if(!(tree->mode & STP_FS_RDWR)) {
      stp_errno =  STP_INDEX_CANT_BE_WRITER;
      return -1;
  }

  /*
   * unlink entry
   */
  //read parent inode
  if(__fs_read_inode(fs,pino,tree,&inode) < 0) 
      return -1;
  
  memset(&off,0,sizeof(off));

  //unlink the filename entry of parent and corresponding inode
  if(__fs_info_unlink(fs,inode,filename,&off,&flags) < 0) 
      return -1;
  
  //unlink the corresponding position
  if(flags)
      return __btree_info_unlink(tree,&off);
  
  return 0;

}

int stp_mkdir(STP_FILE file,u64 pino,const char *name,mode_t mode)
{
    struct stp_fs_info *tree;
    struct stp_btree_info *btree;
    struct stp_bnode_off off;
    struct stp_dir_item item;
    int flags;

    if(!file || !name || strlen(name) > DIR_LEN) {
        stp_errno = STP_INVALID_ARGUMENT;
        return -1;
    }
    
    tree = file->fs;
    btree = file->tree;
    
    if(!(tree->mode & STP_FS_RDWR)) {
        stp_errno = STP_INDEX_CANT_BE_WRITER;
        return -1;
    }

    //read parent inode
    if(__fs_read_inode(tree,pino,btree,NULL) < 0) 
        return -1;

    memset(&item,0,sizeof(item));
    
    item.name_len = strlen(name);
    strncpy(item.name,name,item.name_len);
    
    memset(&off,0,sizeof(off));
    
    mode |= S_IFDIR;
    
    flags = __fs_info_mkdir(tree,pino,&item,&off,mode);
    if(flags < 0) return -1;
    flags = __btree_info_insert(btree,&off);
    
    return 0;
}


int stp_rmdir(STP_FILE file,u64 pino,const char *filename,size_t len)
{
    struct stp_fs_info *fs;
    struct stp_btree_info *tree;
    struct stp_bnode_off off;
    struct stp_inode *inode,*pinode;
    struct stp_dir_item *it;
    int flag;

    if(!file) {
        stp_errno = STP_INVALID_ARGUMENT;
        return -1;
    }
    
    fs = file->fs;
    tree = file->tree;
    
    if(!(fs->mode & STP_FS_RDWR)) {
        stp_errno = STP_META_CANT_BE_WRITER;
        return -1;
    }
    
    memset(&off,0,sizeof(off));
    if(!strcmp(filename, "/")) {
        stp_errno = STP_FS_ROOT;
        return -1;
    }    

    //get parent inode
    if(__fs_read_inode(fs,pino,tree,&pinode) < 0) 
        return -1;

    //get child inode position
    if(!(it = pinode->ops->find_entry(pinode,filename,len)))
        return -1;

    //get child inode
    struct stp_bnode_off;
    
    memset(&off,0,sizeof(off));
    
    if(tree->ops->search(tree,it->ino,&off) < 0)
        return -1;
    
    if(fs->ops->lookup(fs,&inode,off.ino,off.offset) < 0)
        return -1;
    
    if(!(inode->item->mode & S_IFDIR)) {
        stp_errno = STP_FS_NO_DIR;
        return -1;
    }

    if(inode->item->nritem != 0) {
        stp_errno = STP_FS_DIR_NOEMPTY;
        return -1;
    }
    
    if(__fs_info_unlink(fs,pinode,filename,&off,&flag) < 0)
        return -1;
    
    if(flag)
        return __btree_info_unlink(tree,&off);
    
    return 0;
}

dirent_t * stp_opendir(STP_FILE file,u64 ino)
{
    struct stp_fs_info *fs;
    struct stp_btree_info *tree;
    struct stp_bnode_off off;
    struct stp_inode *inode;
    dirent_t * pdir;
    int i;
    struct dirent *entry;
    
    if(!file) {
        stp_errno = STP_INVALID_ARGUMENT;
        return NULL;
    }
    
    fs = file->fs;
    tree = file->tree;
    
    if((!(tree->mode & STP_FS_RDWR)) && (!(tree->mode & STP_FS_READ))) {
        stp_errno = STP_INDEX_CANT_BE_READER;
        return NULL;
    }

    if(__fs_read_inode(fs,ino,tree,&inode)  < 0)
        return NULL;

    if(!(inode->item->mode & S_IFDIR)) {
        stp_errno = STP_FS_NO_DIR;
        return NULL;
    }

    if(!(pdir = calloc(1,sizeof(dirent_t)))) {
        stp_errno = STP_MALLOC_ERROR;
        return NULL;
    }
    
    if(!inode->item->nritem) 
        entry = NULL;
    else 
    {
        if(!(entry = calloc(inode->item->nritem,sizeof(struct dirent)))) {
            free(pdir);
            stp_errno = STP_MALLOC_ERROR;
            return NULL;
        }
        
        struct stp_dir_item *item;
        
        if(!(item = calloc(inode->item->nritem,sizeof(struct stp_dir_item)))) {
            stp_errno = STP_INODE_MALLOC_ERROR;
            return NULL;
        }

        if(inode->ops->readdir(inode,item) < 0) {
            free(item);
            return NULL;
        }
        
        //for loop copy
        for(i = 0;i < inode->item->nritem;i++) {
            entry[i].d_ino = item[i].ino;
            strncpy(entry[i].d_name,item[i].name,item[i].name_len);
            entry[i].d_off = 0;
            entry[i].d_reclen = 0;
            entry[i].d_type = 0;
        }
        
        free(item);
    }
    
    pdir->dir = entry;
    pdir->curr = 0;
    pdir->ino = ino;
    pdir->length = inode->item->nritem;
    
    return pdir;
}

int stp_closedir(dirent_t *pdir)
{
    if(pdir->dir)
        free(pdir->dir);
    free(pdir);
    
    return 0;
}

struct dirent* stp_readdir2(dirent_t *pdir)
{
    if(pdir->length == 0 || pdir->curr == pdir->length)
        return NULL;

    pdir->curr++;
    return (pdir->dir + pdir->curr - 1);
}

static int __fs_info_insert(struct stp_fs_info *sb,u64 pino,struct stp_dir_item *key,struct stp_bnode_off *off,mode_t mode)
{
    struct stp_inode *inode,*parent;
    int flags;
    
    if(sb->ops->find(sb,&parent,pino) < 0) {
        
        #ifdef DEBUG
        fprintf(stderr,"fail to find parent ino.\n");
        #endif
        
        return -1;
    }
    
    flags = parent->ops->lookup(parent,key->name,key->name_len,0);
    
    if(flags < 0 && stp_errno != STP_FS_ENTRY_NOEXIST) return -1;
    if(!flags) {
        stp_errno = STP_FS_ENTRY_EXIST;
        return -1;
    }
    
    if(!(inode = sb->ops->allocate(sb,0))) return -1;
    
    key->ino = inode->item->ino;
    off->ino = inode->item->ino;
    off->offset = inode->item->location.offset;
    off->len = inode->item->location.count;
    
	#ifdef DEBUG
    
    printf("ino:%llu,len:%llu,offset:%llu\n",off->ino,off->len,off->offset);
    #endif
    
    return inode->ops->creat(parent,key->name,key->name_len,inode,mode);
}

static int __fs_info_mkdir(struct stp_fs_info *sb,u64 pino,struct stp_dir_item *key,struct stp_bnode_off *off,mode_t mode)
{
    struct stp_inode *inode,*parent;
    int flags;
    
    if(!(mode & S_IFDIR)) {
        stp_errno = STP_FS_NODIR;
        return -1;
    }
    
    
    if(sb->ops->find(sb,&parent,pino) < 0) {
        #ifdef DEBUG
        fprintf(stderr,"fail to find parent inode.\n");
        #endif
        return -1;
    }

    if(!(parent->item->mode & S_IFDIR)) {
        stp_errno = STP_FS_NO_DIR;
        return -1;
    }
    
    flags = parent->ops->lookup(parent,key->name,key->name_len,0);
    if(flags < 0 && stp_errno != STP_FS_ENTRY_NOEXIST) return -1;
    if(!flags) {
        stp_errno = STP_FS_ENTRY_EXIST;
        return -1;
    }
    
    if(!(inode = sb->ops->allocate(sb,0))) return -1;
    
    key->ino = inode->item->ino;
    off->ino = inode->item->ino;
    off->offset = inode->item->location.offset;
    off->len = inode->item->location.count;
    inode->item->mode = mode;
    
    #ifdef DEBUG
    printf("ino:%llu,len:%llu,offset:%llu\n",off->ino,off->len,off->offset);
    #endif
    
    return inode->ops->mkdir(parent,key->name,key->name_len,inode);
}


static int __btree_info_insert(struct stp_btree_info *tree,const struct stp_bnode_off *off)
{
    return tree->ops->insert(tree,off,BTREE_OVERFLAP);
}

static int __fs_info_unlink(struct stp_fs_info *sb,struct stp_inode *inode,const char *name,struct stp_bnode_off *off,int *flags)
{
    struct stp_inode *_inode;
    u64 ino;
    size_t len = strlen(name);
    
    //rm entry
    if(inode->ops->rm(inode,name,len,&ino) < 0)
        return -1;
    off->ino = ino;
    //search corresponding_inode of name
    if(sb->ops->find(sb,&_inode,ino) < 0) 
        return -1;
    
    assert(_inode->item);
    if(off) 
    {
        off->offset = _inode->item->location.offset;
        off->len = _inode->item->location.count;
        off->flags = _inode->item->location.flags;
    }
    
    *flags = _inode->item->nlink;
    
    return _inode->ops->unlink(_inode);
}

static int __btree_info_unlink(struct stp_btree_info *sb,const struct stp_bnode_off *off)
{    
    #ifdef DEBUG
    printf("btree unlink ino:%llu\n",off->ino);
    #endif
    return sb->ops->rm(sb,off->ino);
}

int __fs_read_inode(struct stp_fs_info *fs,u64 ino,struct stp_btree_info *btree,struct stp_inode **_inode)
{
    struct stp_inode *inode;
    struct stp_bnode_off off;
    
    if(!fs->ops->find(fs,_inode,ino)) 
        return 0;
    
    memset(&off,0,sizeof(off));
    
    if(btree->ops->search(btree,ino,&off) < 0)
        return -1;
    if(fs->ops->lookup(fs,&inode,off.ino,off.offset) < 0)
        return -1;

    if(_inode)
        *_inode = inode;
    
    return 0;
}
