//
// @file index_file.cc
// @brief Implementation of index file.
// 
// @version 1.0
// @date Tue Jul  2 15:19:30 2013
// 
// @copyright Copyright (C) 2013
// @author weishih<lvpengcheng6300@gmail.com>
//

#include "engine/btree_index_file.h"

#include <string>

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "engine/status.h"
#include "engine/data_format.h"

#include "bitmap.h"
#include "stp_types.h"
#include "stp.h"

int do_btree_super_search(struct stp_btree_info *sb,
    u64 ino, struct stp_bnode_off *off);
int do_btree_super_insert(struct stp_btree_info *sb, 
    const struct stp_bnode_off *off, u8 flags);
int do_btree_super_rm(struct stp_btree_info *sb, u64 ino);

BTreeIndexFile::BTreeIndexFile(const std::string& file_name) :
    IndexFile(file_name) {
  btree_ = NULL;
}

BTreeIndexFile::~BTreeIndexFile() {

}

Status BTreeIndexFile::Open() {
  return OpenBtree();
}

Status BTreeIndexFile::Close() {
  btree_->ops->destroy(btree_);

  int rv = fsync(btree_->fd);
  if (rv < 0) {
    fprintf(stderr, "fsync index file %s error, %s\n",
        index_file_.c_str(), strerror(errno));
    return Status::Error("FSync index file error.");
  }

  rv = msync(btree_->super, BTREE_SUPER_SIZE, MS_SYNC);
  if (rv < 0) {
    fprintf(stderr, "msync index file %s error, %s\n",
        index_file_.c_str(), strerror(errno));
    return Status::Error("MSync index file error.");
  }

  rv = munmap(btree_->super, BTREE_SUPER_SIZE);
  if (rv < 0) {
    fprintf(stderr, "munmap index file %s error, %s\n",
        index_file_.c_str(), strerror(errno));
    return Status::Error("Unmap index file error.");
  }

  close(btree_->fd);
  free(btree_);
  btree_ = NULL;
  return Status::OK();
}

Status BTreeIndexFile::Sync() {
  struct stp_bnode* bnode;
  struct stp_bnode* next;

  /*destroy bnode and flush it into disk*/
  list_for_each_entry_del(bnode, next, &btree_->dirty_list, dirty) {
    btree_->ops->write(btree_, bnode);
    list_del_element(&bnode->dirty);
  }

  int rv = fsync(btree_->fd);
  if (rv < 0) {
    fprintf(stderr, "fsync index file %s error, %s\n",
        index_file_.c_str(), strerror(errno));
    return Status::Error("FSync index file error.");
  }

  rv = msync(btree_->super, BTREE_SUPER_SIZE, MS_SYNC);
  if (rv < 0) {
    fprintf(stderr, "msync index file %s error, %s\n", 
        index_file_.c_str(), strerror(errno));
    return Status::Error("MSync index file error.");
  }

  return Status::OK();
}

Status BTreeIndexFile::GetIndex(uint64_t key, IndexRecord* index) {
  struct stp_bnode_off off;
  off.ino = key;
  int rc = do_btree_super_search(btree_, key, &off);
  if (rc < 0) {
    if (stp_errno = STP_INDEX_ITEM_NO_FOUND) {
      Status::NotFound("record with specified key not found "
          "in the index file\n");
    }
    fprintf(stderr, "search record in index file %s error\n", 
        index_file_.c_str());
    return Status::Error("Search index file error.");
  }

  index->key = key;
  index->offset = off.offset;
  index->len = off.len;
  return Status::OK();
}

Status BTreeIndexFile::PutIndex(const IndexRecord& index) {
  struct stp_bnode_off off;
  off.ino = index.key;
  off.flags = 0;
  off.offset = index.offset;
  off.len = index.len;

  int rc = do_btree_super_insert(btree_, &off, 0);
  if (rc != 0) {
    fprintf(stderr, "put record in index file %s error\n", 
        index_file_.c_str());
    return Status::Error("Put into index file error.");
  }

  return Status::OK();
}

Status BTreeIndexFile::DeleteIndex(uint64_t key) {
  int rc = do_btree_super_rm(btree_, key);
  if (rc != 0) {
    fprintf(stderr, "delete record from index file %s error\n", 
        index_file_.c_str());
    return Status::Error("Delete from index file error.");
  }

  return Status::OK();
}

Status BTreeIndexFile::OpenBtree() {
  int bfd = 0;
  int mode = 0;
  mode_t m = O_RDWR;
  struct stat st;
  unsigned int flags = 0;
  
  mode &= ~STP_FS_CREAT;

  if(stat(index_file_.c_str(), &st) < 0) {
    mode |= STP_FS_CREAT;
    fprintf(stderr,"Can't find the index file %s.\n", index_file_.c_str());
  }
  if(mode & STP_FS_CREAT) {   
    m |= O_CREAT;
    mode |= STP_FS_RDWR;
  }

  if((bfd = open(index_file_.c_str(), m, S_IRWXU | S_IRGRP | S_IROTH)) < 0) {
    fprintf(stderr, "Open index file %s error, %s\n", 
        index_file_.c_str(), strerror(errno));
    return Status::Error("Open index file error.");
  }                                                          

  Status status = ReadBtreeInfo(bfd, mode);
  if (!status.Ok()) {
    fprintf(stderr, "Read btree info from index file %s error.\n",
        index_file_.c_str());
    close(bfd);
    return Status::Error("Read btree info from idnex file error.");
  }
  btree_->filename = index_file_.c_str();
  return Status::OK();
}

Status BTreeIndexFile::ReadBtreeInfo(int index_fd, int mode) {
  if(!(btree_ = 
        (struct stp_btree_info *) calloc(1,sizeof(struct stp_btree_info)))) {
    stp_errno = STP_MALLOC_ERROR;
    return Status::Error("Allocate memory error.");
  }

  btree_->ops = &stp_btree_super_operations;
  if((mode & STP_FS_CREAT) && ftruncate(index_fd,BTREE_SUPER_SIZE) < 0) {
    stp_errno = STP_INDEX_CREAT_ERROR;
    fprintf(stderr, "Truncate index file %s error, %s\n",
        index_file_.c_str(), strerror(errno));
    return Status::Error("Truncate index file error.");
  }

  void* addr = NULL;
  if((addr = mmap(NULL,BTREE_SUPER_SIZE,PROT_READ|PROT_WRITE,\
          MAP_SHARED|MAP_LOCKED,index_fd,0)) == MAP_FAILED) {
    stp_errno = STP_MALLOC_ERROR;
    free(btree_);
    btree_ = NULL;
    fprintf(stderr, "mmap index file %s error, %s\n", 
        index_file_.c_str(), strerror(errno));
    return Status::Error("Map index file error.");
  }

  if(mode & STP_FS_CREAT)
    memset(addr,0,BTREE_SUPER_SIZE);

  btree_->super = (struct stp_btree_super *) addr;
  btree_->mode = mode;
  btree_->fd = index_fd; 

  if(btree_->ops->init(btree_) < 0) {
    munmap(btree_->super,BTREE_SUPER_SIZE);
    stp_errno = STP_MALLOC_ERROR;
    free(btree_);
    btree_ = NULL;
    fprintf(stderr, "munmap index file %s error, %s\n", 
        index_file_.c_str(), strerror(errno));
    return Status::Error("Unmap index file error.");
  }

  return Status::OK();
}

