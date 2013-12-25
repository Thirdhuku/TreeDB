#include <stdlib.h>
#include <string.h>
#include <stdio.h>


#include "stp_fs.h"
#include "stp_error.h"
#include "list.h"

static int do_bnode_init(struct stp_bnode *bnode) 
{
    return -1;
}

static int do_bnode_insert(struct stp_bnode * node,u64 ino,size_t start,off_t offset)
{
    return -1;
}

static int do_bnode_update(struct stp_bnode * node,u64 ino,size_t start,off_t offset)
{
    return -1;
}

static int do_bnode_delete(struct stp_bnode *node,u64 ino)
{
    return -1;
}

static int do_bnode_destroy(struct stp_bnode * node)
{
    return -1;
}

static struct stp_bnode* do_bnode_search(struct stp_bnode *node,u64 ino,struct stp_bnode_off *off)
{
    return NULL;
}


const struct stp_bnode_operations bnode_operations = {
    .init = do_bnode_init,
    .insert = do_bnode_insert,
    .update = do_bnode_update,
    .delete = do_bnode_delete,
    .search = do_bnode_search,
    .destroy = do_bnode_destroy,
};

    
