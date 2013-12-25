#ifndef __STP_FS_H__
#define __STP_FS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "list.h"
#include "stp_types.h"
#include "rb_tree.h"

#include <pthread.h>
#include <semaphore.h>

#define STP_HEADER_DELETE (1<<0)

/*
 * every object(meta_item or btree_item) has a header to indicate where the node is.
 */
struct stp_header {
    u64 offset;
    u64 count;
    u8 flags;
    u32 nritems;
}__attribute__((__packed__));

/*
 * directory item(256Byte) in directory
 */
#define DIR_LEN 127

struct stp_dir_item {
    u64 ino;
    u32 name_len;
    char name[DIR_LEN + 1];
    u8 flags;
    u8 padding[115];
}__attribute__((__packed__));

/*
 * inode item in disk 256 byte
 * 
 */
struct stp_inode_item {
    struct stp_header location;
    u64 ino;
    u64 size;
    u8 nlink;
    u32 uid;
    u32 gid;
    u32 mode;
    u16 flags;
    u64 atime;
    u64 ctime;
    u64 mtime;
    u64 otime;
    u64 transid;
    u32 nritem;//file-item number dir_item 
    union //dir entry or extend attribution 
    {
        //direct(8KB 31 entries),indirect(8KB-8KB),2-indirect
        struct stp_header entry[3]; //for dir record
        struct extend_attribution {
            char ip[32];
            u8 padding[64];
        } extend;   
    };
    char checksum[32];
    char address[32];//meta location
} __attribute__((__packed__));

/*
 * meta file super block
 * 4KB --metadata
 *
 */
#define FS_SUPER_SIZE  (4*1024)
#define STP_FS_MAGIC (0x1357ef77)
struct stp_fs_super {
    u32 magic;
    u32 flags;
    u64 total_bytes;
    u64 bytes_used;
    u64 bytes_hole;
    u64 ino;
    u32 nritems;//number item
    u32 nrdelete;//delete item number
    struct stp_inode_item root;
} __attribute__((__packed__));

/*
 *
 * maybe add some operations later
 */

struct stp_inode;
    
struct stp_inode_operations {
    int (*init)(struct stp_inode *);
    int (*setattr)(struct stp_inode *);
    int (*mkdir)(struct stp_inode *,const char *,size_t,struct stp_inode *);
    int (*lookup)(struct stp_inode *,const char *,size_t,u64);
    int (*rm)(struct stp_inode *,const char *name,size_t len,u64 *ino);
    int (*creat)(struct stp_inode *,const char *,size_t,struct stp_inode *,mode_t);
    int (*readdir)(struct stp_inode *,struct stp_dir_item *);
    int (*destroy)(struct stp_inode *);
    int (*sync)(struct stp_inode *);
    int (*free)(struct stp_inode *);
    int (*unlink)(struct stp_inode *);
    struct stp_dir_item* (*find_entry)(struct stp_inode *parent,const char *filename,size_t len);
};

#define STP_FS_INODE_CREAT  (1<<0)
#define STP_FS_INODE_DIRTY  (1<<1)
#define STP_FS_INODE_DIRTY_MM (1<<2)

#define STP_FS_DIR_NUM (31)
/*
 * dirent format in disk
 */
struct stp_fs_dirent {
    struct stp_header location;
    u8 padding[235];
    struct stp_dir_item item[STP_FS_DIR_NUM];
}__attribute__((__packed__));

#define STP_FS_DIRENT_MAX  (389)
/*
 * dirent indirect format
 */
struct stp_fs_indir {
    struct stp_header location;
    u8 padding[2];
    struct stp_header index[STP_FS_DIRENT_MAX]; 
}__attribute__((__packed__));        

#define STP_FS_ENTRY_DIRTY (1<<0)
#define STP_FS_ENTRY_DIRECT (1<<1)
#define STP_FS_ENTRY_INDIR1 (1<<2)
#define STP_FS_ENTRY_INDIR2 (1<<3)
#define STP_FS_ENTRY_RB (1<<4)
struct stp_fs_entry_operations;    

struct stp_fs_entry {
    u32 flags;
    struct stp_inode *inode;
    struct stp_fs_entry *parent;
    void * entry;
    size_t size;
    off_t offset;
    struct rb_node node;
    struct list list;
    struct list lru;
    struct list dirty;
    /* tree entry
     * direct NULL
     * indirect 
     *    |    \
     * direct--direct ....
     */
    struct list sibling;//link all brother entry 
    struct list child;
    const struct stp_fs_entry_operations *ops;
};

struct stp_fs_info;

struct stp_fs_entry_operations {
    int (*alloc)(struct stp_fs_info *,struct stp_inode *,struct stp_fs_entry *);
    int (*read)(struct stp_fs_info *,struct stp_fs_entry *);
    int (*rm)(struct stp_fs_info *,struct stp_fs_entry *);
    int (*release)(struct stp_fs_info *,struct stp_fs_entry *);
    int (*sync)(struct stp_fs_info *,struct stp_fs_entry *);
    int (*destroy)(struct stp_fs_info *,struct stp_fs_entry *);
    int (*free_entry)(struct stp_fs_info *,struct stp_fs_entry *);
    int (*free)(struct stp_fs_info *,struct stp_fs_entry *);
};     

extern const struct stp_fs_entry_operations fs_entry_operations;
    
struct stp_inode {
    u8 flags;
    u32 ref;
    pthread_mutex_t lock;
    struct rb_node node;//for search
    struct list lru;
    struct list dirty;
    struct list list;
    struct list entry_list;//for dir cache
    struct stp_fs_info *fs;
    struct stp_inode_item *item;
    struct stp_inode *parent;
    struct rb_root root; //key:offset + size
    struct list child;//unused
    struct list sibling;//unused
    const struct stp_inode_operations *ops;
};

extern const struct stp_inode_operations inode_operations;
    
struct stp_fs_operations {
    int (*init)(struct stp_fs_info *);
    struct stp_inode* (*allocate)(struct stp_fs_info *,off_t);
    int (*free_inode)(struct stp_fs_info *,struct stp_inode *);
    int (*destroy_inode)(struct stp_fs_info *,struct stp_inode *);
    struct stp_fs_entry * (*alloc_entry)(struct stp_fs_info *,struct stp_inode *,off_t,size_t,struct stp_fs_entry *);
    int (*free_entry)(struct stp_fs_info *,struct stp_fs_entry *);
    int (*destroy_entry)(struct stp_fs_info *,struct stp_fs_entry *);
    int (*lookup)(struct stp_fs_info *sb,struct stp_inode **inode,u64 ino,off_t offset);
    int (*find)(struct stp_fs_info *sb,struct stp_inode **inode,u64 ino);
    int (*free)(struct stp_fs_info *);
    int (*read)(struct stp_fs_info *,struct stp_inode *,off_t offset);
    int (*sync)(struct stp_fs_info *);
    int (*write)(struct stp_fs_info *,struct stp_inode *);
    int (*destroy)(struct stp_fs_info *);
};

extern const struct stp_fs_operations stp_super_operations;

/* 4KB metadata*/
struct stp_fs_info {
    const char *filename;
    int fd;
    u32 mode;
    u32 magic;
    u32 active;
    u64 transid;
    off_t offset;//current fs file offset
    sem_t sem;
    pthread_mutex_t mutex;
    struct stp_fs_super *super;
    struct rb_root root;//rb root for read/search in memory
    struct list inode_list;
    struct list inode_lru;
    //    struct list inode_mm;//inode from mmap
    struct list dirty_list;//inode dirty list
    struct list entry_dirty_list;
    struct list entry_list;//all inode entry(data or dir) list
    const struct stp_fs_operations *ops;//inode read/write/sync operations
};

extern const struct stp_fs_operations stp_fs_super_operations;

/*
 *
 * b+ tree
 */
struct stp_bnode_off {
    u64 ino;
    u8 flags;
    u64 len;
    u64 offset;
} __attribute__((__packed__));

#define BTREE_KEY_DELETE  (1<<0)

    
struct stp_bnode_key {
    u64 ino;
    u8 flags;
} __attribute__((__packed__));

#define BTREE_DEGREE 59      
#define MAX ((u32)-1)

#define KEY(t)  (2*(t) - 1)
#define MIN_KEY(t)  ((t) - 1)
#define MIN_CHILD(t)  (t)
#define CHILD(t) (2*(t))


#define BTREE_CHILD_MAX (CHILD(BTREE_DEGREE))
#define BTREE_CHILD_MIN (MIN_CHILD(BTREE_DEGREE))
#define BTREE_KEY_MIN (MIN_KEY(BTREE_DEGREE))
#define BTREE_KEY_MAX (KEY(BTREE_DEGREE))

#define BTREE_ITEM_HOLE (1<<0)
#define BTREE_ITEM_LEAF (1<<1)

struct stp_bnode_item {  //4096 bytes
    struct stp_header location;
    struct stp_bnode_key key[KEY(BTREE_DEGREE)];
    u32 nrkeys;
    struct stp_bnode_off ptrs[CHILD(BTREE_DEGREE)];
    u32 nrptrs;
    u32 level;
    u8 flags;
    struct stp_header parent;
    u8 padding[38];
}__attribute__((__packed__));


struct stp_bnode;
    
struct stp_bnode_operations {
    int (*init)(struct stp_bnode * node);
    int (*insert)(struct stp_bnode * node,u64 ino,size_t start,off_t offset);
    int (*update)(struct stp_bnode * node,u64 ino,size_t start,off_t offset);
    int (*delete)(struct stp_bnode * node,u64 ino);
    struct stp_bnode* (*search)(struct stp_bnode *node,u64 ino,struct stp_bnode_off *off);
    int (*destroy)(struct stp_bnode * node);
};
    
#define STP_INDEX_BNODE_CREAT (1<<0)
#define STP_INDEX_BNODE_DIRTY (1<<1)        

struct stp_bnode {
    u8 flags;
    u8 ref;
    pthread_mutex_t lock;
    struct list lru;
    struct list dirty;
    struct list list;
    struct stp_bnode *ptrs[CHILD(BTREE_DEGREE)];
  	struct stp_bnode *parent;
    struct stp_btree_info *tree;
    struct stp_bnode_item *item;
    const struct stp_bnode_operations *ops;
};

extern const struct stp_bnode_operations bnode_operations;
    

/*
 * btree index file layout:
 * superinfo:8KB btree-node
 * actually the super'size is 4.5KB,but it's 8KB for mmap function
 */
//#define BTREE_SUPER_SIZE (sizeof(struct stp_bnode_item) + 1024)
#define BTREE_SUPER_SIZE (1024*8) 
#define BITMAP_ENTRY  (512)
#define BITMAP_SIZE  (BITMAP_ENTRY * sizeof(u32) * 8) //2KB

struct stp_btree_super {
    u32 magic;
    u32 flags;
    u64 total_bytes;
    u32 nritems;//btree node number
    u64 nrkeys;//btree number keys
    u32 bitmap[BITMAP_ENTRY];
    //    struct stp_bnode_item root;
    struct stp_header root; // location of root
} __attribute__((__packed__));

#define BTREE_MAX_NODE (BITMAP_ENTRY * 32)
#define BTREE_TOTAL_SIZE (BTREE_MAX_NODE*(sizeof(struct stp_bnode_item)) + BTREE_SUPER_SIZE)   

struct stp_btree_info;
    

struct stp_btree_operations {
    int (*init)(struct stp_btree_info *);
    struct stp_bnode* (*allocate)(struct stp_btree_info *,off_t offset);
    int (*read)(struct stp_btree_info *,struct stp_bnode *,off_t offset);
    int (*sync)(struct stp_btree_info *);
    int (*write)(struct stp_btree_info *,struct stp_bnode *);
  	// ino must be unique
    int (*search)(struct stp_btree_info *,u64 ino,struct stp_bnode_off * );
  	void (*debug)(const struct stp_bnode *);
    void (*debug_btree)(const struct stp_btree_info *);
    int (*insert)(struct stp_btree_info *,const struct stp_bnode_off *,u8);
    int (*rm)(struct stp_btree_info *,u64 ino);
    int (*free)(struct stp_btree_info *,struct stp_bnode *);
    int (*destroy)(struct stp_btree_info *);
};

extern const struct stp_btree_operations stp_btree_super_operations;
    

struct stp_btree_info {
    const char *filename;
    int fd;
    u8 mode;
    u64 transid;
    sem_t sem;
    u32 active;//item in memory
    u64 off;//last location in bitmap
    pthread_mutex_t mutex;
    struct stp_btree_super *super;
    struct stp_bnode *root;
    struct list node_list;
    struct list node_lru;
    struct list dirty_list;
    const struct stp_btree_operations *ops;//node operations
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

/*
 * recore insert flags
 */
#define BTREE_OVERFLAP (1<<0)    

#ifdef __cplusplus
}
#endif

#endif
