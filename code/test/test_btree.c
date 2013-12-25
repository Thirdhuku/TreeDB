/*
 * @file test_btree.c
 * @brief
 *
 * @version 1.0
 * @date Sun Apr 28 11:47:14 2013
 *
 * @copyright Copyright (C) 2013 UESTC
 * @author lpc<lvpengcheng6300@gmail.com>
 */

#include "bitmap.h"
#include "stp_types.h"
#include "stp.h"

#include <assert.h>
#include <errno.h>
#include <string.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>



struct timeval g_begin;
struct timeval g_end;
void begin_timer()
{
	gettimeofday(&g_begin, NULL);
}

void end_timer()
{
	gettimeofday(&g_end, NULL);
}

int Diff_timer()
{
	// in miniseconds
	return (g_end.tv_sec * 1000 + g_end.tv_usec / 1000) - 
		(g_begin.tv_sec * 1000 + g_begin.tv_usec / 1000);
}



struct stp_btree_info* stp_btree_open(const char *btree_file, unsigned int mode);
int read_btree_info(int bfd,struct stp_btree_info ** _btree,unsigned int mode);
int insert_into_btree(struct stp_btree_info *btree, u64 ino, u64 offset);
int search_in_btree(struct stp_btree_info *btree, u64 ino, struct stp_bnode_off *off);
int delete_from_btree(struct stp_btree_info *btree, u64 ino);
int sync_btree(struct stp_btree_info *btree);
void stp_btree_close(struct stp_btree_info *btree);

struct stp_btree_info* stp_btree_open(const char *btree_file, unsigned int mode)
{
    int bfd;
    mode_t m = O_RDWR;
    struct stat st;
    unsigned int flags = 0;
    
    mode &= ~STP_FS_CREAT;

    if(stat(btree_file, &st) < 0) {
        mode |= STP_FS_CREAT;
        fprintf(stderr,"[WARNING] can't find the index file.\n");
    }
    
    if(mode & STP_FS_CREAT) {   
        m |= O_CREAT;
        mode |= STP_FS_RDWR;
    }
    
    if((bfd = open(btree_file, m, S_IRWXU | S_IRGRP | S_IROTH)) < 0) {
        return NULL;
    }                                                          
     
    struct stp_btree_info *tree = NULL;
    
    if(read_btree_info(bfd, &tree, mode) < 0) {
        return NULL;
    }

    tree->filename = btree_file;
    
    return tree;
}

int read_btree_info(int bfd,struct stp_btree_info ** _btree,unsigned int mode)
{
    struct stp_btree_info *btree = NULL;
    void *addr;
    
    if(!(btree = (struct stp_btree_info *)calloc(1,sizeof(struct stp_btree_info)))) {
        stp_errno = STP_MALLOC_ERROR;
        return -1;
    }

    //memset(btree,0,sizeof(struct stp_btree_info));

    btree->ops = &stp_btree_super_operations;
    
    if((mode & STP_FS_CREAT) && ftruncate(bfd,BTREE_SUPER_SIZE) < 0) {
        stp_errno = STP_INDEX_CREAT_ERROR;
        return -1;
    }
    
    if((addr = mmap(NULL,BTREE_SUPER_SIZE,PROT_READ|PROT_WRITE,\
        MAP_SHARED|MAP_LOCKED,bfd,0)) == MAP_FAILED) {
        stp_errno = STP_MALLOC_ERROR;
        free(btree);
        return -1;
    }

    if(mode & STP_FS_CREAT)
        memset(addr,0,BTREE_SUPER_SIZE);

    btree->super = (struct stp_btree_super *)addr;

    #ifdef DEBUG
    if(!(mode & STP_FS_CREAT))
    	printf("OPEN:%s:btree->super:%p,addr:%p,flags:%d\n",
                __FUNCTION__,btree->super,addr,btree->super->root.flags);
    else
      	printf("CREAT:%s:btree->super:%p,addr:%p,root:%p,flags:%p\n",
                __FUNCTION__,btree->super,addr,&btree->super->root,&btree->super->root.flags);
    #endif

    btree->mode = mode;
    btree->fd = bfd;
    
    if(btree->ops->init(btree) < 0) {
        munmap(btree->super,BTREE_SUPER_SIZE);
        stp_errno = STP_MALLOC_ERROR;
        free(btree);
        return -1;
    }

    *_btree = btree;
    return 0;
}

//²åÈë½Ó¿Ú
int insert_into_btree(struct stp_btree_info *btree, u64 ino, u64 offset)
{
    struct stp_bnode_off off;
    off.ino = ino;//?
    off.flags = 0;//?
    off.offset = offset;//?
    off.len = 10;//?

    return do_btree_super_insert(btree, &off, 0);
}

int search_in_btree(struct stp_btree_info *btree, u64 ino, struct stp_bnode_off *off)
{
    return do_btree_super_search(btree, ino, off);
}

int delete_from_btree(struct stp_btree_info *btree, u64 ino)
{
    return do_btree_super_rm(btree, ino);
}

int sync_btree(struct stp_btree_info *btree)
{
    struct stp_bnode *bnode,*next;

    /*destroy bnode and flush it into disk*/
    list_for_each_entry_del(bnode,next,&btree->dirty_list,dirty) {
        btree->ops->write(btree,bnode);
        list_del_element(&bnode->dirty);
    }

    fsync(btree->fd);
    msync(btree->super,BTREE_SUPER_SIZE,MS_SYNC);
} 

void stp_btree_close(struct stp_btree_info *btree)
{
    btree->ops->destroy(btree);

    fsync(btree->fd);
    msync(btree->super,BTREE_SUPER_SIZE,MS_SYNC);
    munmap(btree->super,BTREE_SUPER_SIZE);
    close(btree->fd);
    free(btree);
}

void test_insert(struct stp_btree_info *btree, int start, int num)
{
    int rc = 0;
    int i = 0;
    
    begin_timer();
    
    for (i = start; i <= start + num; ++i) {
        rc = insert_into_btree(btree, i, i);
        if (rc < 0) {
            fprintf(stderr, "insert error, %d\n", i);
            break;
        }

/* 
        if (i % 1000 == 0) {
            rc = sync_btree(btree);
            if (rc < 0) {
                fprintf(stderr, "sync error, %d\n", i);
                break;
            }
        }
   */   

        if(i % 10000 == 0){
	      end_timer();
	      printf("1ms insert %d\n", 10000 / Diff_timer());
	      begin_timer();
        }
    }
}

void test_delete(struct stp_btree_info *btree, int start, int num)
{
    int rc = 0;
    int i = 0;
    for (i = start; i < start + num; ++i) {
        rc = delete_from_btree(btree, i);
        if (rc < 0) {
            fprintf(stderr, "delete error, %d\n", i);
            break;
        }
    }
}

void test_search(struct stp_btree_info *btree, int start, int num)
{
    int rc = 0;
    int i = 0;
    struct stp_bnode_off off;
    for (i = start; i < start + num; ++i) {
        rc = search_in_btree(btree, i, &off);
        if (rc < 0) {
            fprintf(stderr, "search error, %d\n", i);
            break;
        }
        assert(i == off.ino && i == off.offset);
    }
}

void test_sync(struct stp_btree_info *btree)
{
    int rc = 0;

    rc = sync_btree(btree);
    if (rc < 0) {
        fprintf(stderr, "sync error\n");
    }
}



int main(int argc, char *argv[]) 
{
    struct stp_btree_info *btree = NULL;
    btree = stp_btree_open("sw.idx", 0);
    if (btree == NULL) {
        fprintf(stderr, "open index file error\n");
        return -1;
    }

    test_insert(btree, 1, 90000000);
    printf("1\n");
    test_sync(btree);
    printf("2\n");
    test_search(btree, 1, 900000);
    printf("3\n");
    test_delete(btree, 1, 10);
    printf("4\n");
    test_sync(btree);
    printf("5\n");
    test_search(btree, 100000, 800000);
    printf("6\n");
    test_search(btree, 12, 2);
    printf("7\n");
    test_search(btree, 1, 2);
    printf("8\n");

    stp_btree_close(btree);

    return 0;
}



