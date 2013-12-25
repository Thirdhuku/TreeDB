//
// @file map_index_file.cc
// @brief Implementation of index file.
// 
// @version 1.0
// @date Tue Jul  2 15:19:30 2013
// 
// @copyright Copyright (C) 2013
// @author weishih<lvpengcheng6300@gmail.com>
//

#include "engine/map_index_file.h"

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

MapIndexFile::MapIndexFile(const std::string& file_name) :
    IndexFile(file_name) {
  index_fd_ = -1;
}

MapIndexFile::~MapIndexFile() {
  if (index_fd_ != -1) {
    Close();
  }
}

Status MapIndexFile::Open() {
  struct stat st_df;
  int rv = stat(index_file_.c_str(), &st_df);
  if (rv < 0) {
    return CreateIndexFile();
  }
  return OpenIndexFile();
}

Status MapIndexFile::Close() {
  Status st = Sync();
  if (!st.Ok()) {
    fprintf(stderr, "sync index file error, %s\n", strerror(errno));
    return Status::Error("sync index file error");
  }
  close(index_fd_);
  index_fd_ = -1;
  return Status::OK();
}

Status MapIndexFile::Sync() {
  lseek(index_fd_, 0, SEEK_SET);
  char buf[4096];
  int records = 0;
  uint64_t* int_ptr = (uint64_t*)buf;
  for (std::map<uint64_t, uint64_t>::iterator it = index_.begin();
      it != index_.end(); ++it) {

    if ((++records) % 256 == 0) {
      write(index_fd_, buf, 4096);
      int_ptr = (uint64_t*) buf;
    }

    *int_ptr = it->first;
    int_ptr += 8;
    *int_ptr = it->second;
    int_ptr += 8;
  }

  int len = (int_ptr - (uint64_t*) buf) * 8;
  write(index_fd_, buf, len);

  return Status::OK();
}

Status MapIndexFile::GetIndex(uint64_t key, IndexRecord* index) {
  if (index_.empty()) {
    index->key = 0;
    index->block_id = 0;
    return Status::OK();
  }

  std::map<uint64_t, uint64_t>::iterator it = index_.lower_bound(key);
  if (it == index_.end()) {
    std::map<uint64_t, uint64_t>::reverse_iterator rit = index_.rbegin();
    index->key = rit->first;
    index->block_id = rit->second;
    return Status::OK();
  }

  if (it->first == key) {
    index->key = key;
    index->block_id = it->second;
    return Status::OK();
  }

  --it;
  index->key = it->first;
  index->block_id = it->second;
  return Status::OK();
}

Status MapIndexFile::PutIndex(const IndexRecord& index) {
  index_.insert(std::make_pair(index.key, index.block_id));
  return Status::OK();
}

Status MapIndexFile::DeleteIndex(uint64_t key) {
  index_.erase(key);
  return Status::OK();
}

Status MapIndexFile::CreateIndexFile() {
  index_fd_ = open(index_file_.c_str(), O_RDWR | O_CREAT | O_EXCL, 
      S_IRUSR | S_IWUSR);
  if (index_fd_ < 0) {
    fprintf(stderr, "Create index file %s error, %s\n", index_file_.c_str(),
        strerror(errno));
    return Status::Error("Create index file error.");
  }
  return Status::OK();
}

Status MapIndexFile::OpenIndexFile() {
  index_fd_ = open(index_file_.c_str(), O_RDWR);
  if (index_fd_ < 0) {
    fprintf(stderr, "Open index file %s error, %s\n", index_file_.c_str(),
        strerror(errno));
    return Status::Error("Open index file error.");
  }

  char buf[4096];
  int readn = 0;
  while ( (readn = read(index_fd_, buf, 4096)) > 0) {
    int off = 0;
    while (off < readn) {
      int64_t key = *((uint64_t*)(buf + off));
      int64_t block_id = *((uint64_t*)(buf + off + 8));
      index_.insert(std::make_pair(key, block_id));
      off += 16;
    }
  }

  return Status::OK();
}


