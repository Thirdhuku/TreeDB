#ifndef __STP_FILE_H__
#define __STP_FILE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "list.h"
#include "stp_types.h"

#include <pthread.h>
#include <semaphore.h>

/*
 * every object(meta_item or btree_item) has a header to indicate where the node is.
 */
struct stp_header {
    u64 start;
    u64 offset;
    u8 flags;
    u32 nritems;
}__attribute__((__packed__));

/*
 * directory item in directory
 */
#define DIR_LEN 128

struct stp_dir_item {
    u64 ino;
    u32 name_len;
    char name[DIR_LEN];
}__attribute__((__packed__));

/*
 * inode item in disk
 * 
 */
struct stp_inode_item {
    struct stp_header location;
    u64 ino;
    u64 size;
    u8 nlink;
    u32 uid;
    u32 gid;
    u8 mode;
    u16 flags;
    u64 atime;
    u64 ctime;
    u64 mtime;
    u64 otime;
    u64 transid;
    u32 nritem;//file-item number dir_item
} __attribute__((__packed__));

/*
 * meta file super block
 *
 */
#define FS_SUPER_SIZE  (4*1024)
struct stp_fs_super {
    u32 magic;
    u32 flags;
    u64 total_bytes;
    u64 bytes_used;
    u64 bytes_hole;
    u32 nritems;//number item
    u32 nrdelete;//delete item number
    struct stp_inode_item root;
} __attribute__((__packed__));

/*
 *
 * may be add some operations later
 */

struct stp_inode;
    
struct stp_inode_operations {
    int (*setattr)(struct stp_inode *);
    int (*mkdir)(struct stp_inode *);
    int (*rm)(struct stp_inode *,u64 ino);
    int (*create)(struct stp_inode *,u64 ino);
};

struct stp_inode {
    u8 flags;
    u8 ref;
    struct list lru;
    struct list dirty;
    struct stp_fs_info *fs;
    struct stp_inode_item item;
    struct stp_inode_operations *ops;
};

struct stp_fs_info;
    
struct stp_fs_operations {
    int (*init)(struct stp_fs_info *);
    struct stp_inode* (*allocate)(struct stp_fs_info *);
    int (*free)(struct stp_fs_info *,struct stp_inode *);
    struct stp_inode * (*read)(struct stp_fs_info *,off_t offset);
    int (*sync)(struct stp_fs_info *);
    int (*write)(struct stp_fs_info *,struct stp_inode *);
};

extern const struct stp_fs_operations stp_super_operations;

/* 4KB metadata*/
struct stp_fs_info {
    const char *filename;
    int fd;
    u8 mode;
    u64 transid;
    sem_t sem;
    pthread_mutex_t mutex;
    struct stp_fs_super super;
    struct list inode_list;
    struct list inode_lru;
    struct list dirty_list;
    struct stp_fs_operations *ops;//inode read/write/sync operations
};

/*
 *
 * b+ tree
 */
struct stp_bnode_off {
    u64 ino;
    u8 flags;
    u64 start;
    u64 offset;
} __attribute__((__packed__));
    
struct stp_bnode_key {
    u64 ino;
} __attribute__((__packed__));

#define BTREE_DEGREE 32      
#define KEY(t)  (2*(t) - 1)
#define MIN_KEY(t)  ((t) - 1)
#define MIN_CHILD(t)  (t)
#define CHILD(t) (t)

struct stp_bnode_item {
    struct stp_header location;
    struct stp_bnode_key key[KEY(BTREE_DEGREE)];
    u32 nrkeys;
    struct stp_bnode_off ptrs[CHILD(BTREE_DEGREE)];
    u32 nrptrs;
    u32 level;
}__attribute__((__packed__));


struct stp_bnode;
    
struct stp_bnode_operations {
    int (*init)(struct stp_bnode * node,void *arg);
    int (*insert)(struct stp_bnode * node,struct stp_bnode_key *key);
    int (*update)(struct stp_bnode * node,struct stp_bnode_key *key);
    int (*delete)(struct stp_bnode * node,struct stp_bnode_key *key);
    int (*search)(struct stp_bnode * node,struct stp_bnode_key *key);
};
    
        

struct stp_bnode {
    u8 flags;
    u8 ref;
    struct list lru;
    struct list dirty;
    struct stp_btree_info *tree;
    struct stp_bnode_item item;
    struct stp_bnode_operations *ops;
};
    
        

/*
 * btree index file layout:
 * superinfo:4KB--4KB--4KB btree-node
 */
#define BITMAP_ENTRY  (1024)

struct stp_btree_super {
    u32 magic;
    u32 flags;
    u64 total_bytes;
    u32 nritems;//btree node number
    u32 bitmap[BITMAP_ENTRY];
    struct stp_bnode_item root;
} __attribute__((__packed__));

#define BTREE_MAX_NODE (BITMAP_ENTRY * 32)
    
struct stp_btree_info;
    

struct stp_btree_operations {
    int (*init)(struct stp_btree_info *);
    int (*read)(struct stp_btree_info *,off_t offset);
    int (*sync)(struct stp_btree_info *);
    int (*write)(struct stp_btree_info *,struct stp_bnode *);
    int (*search)(struct stp_btree_info *,struct stp_bnode_key *key);
    int (*insert)(struct stp_btree_info *,struct stp_bnode_key *key);
    int (*rm)(struct stp_btree_info *,struct stp_bnode_key *key);
};  

struct stp_btree_info {
    const char *filename;
    int fd;
    u8 mode;
    u64 transid;
    sem_t sem;
    pthread_mutex_t mutex;
    struct stp_btree_super super;
    struct list node_list;
    struct list node_lru;
    struct list dirty_list;
    struct stp_fs_operations *ops;//node operations
};

typedef struct {
    struct stp_fs_info *fs;
    struct stp_btree_info *tree;
}STP_FILE_INFO;

#define STP_FS_READ  (1<<0)
#define STP_FS_WRITE (1<<1)
#define STP_FS_RDWR  (1<<2)
#define STP_FS_CREAT (1<<3)

typedef unsigned int stp_error;

extern stp_error stp_errno;

typedef STP_FILE_INFO* STP_FILE;


const char * stp_strerror(stp_error error);
    
STP_FILE stp_open(const char *filename,int mode);

int stp_close(STP_FILE file);
  

#ifdef __cplusplus
}
#endif

#endif
