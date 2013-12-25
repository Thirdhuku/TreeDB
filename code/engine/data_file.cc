//
// @file data_file.cc
// @brief Implementation of data file operations.
// 
// @version 1.0
// @date Mon Jul  1 11:31:10 2013
// 
// @copyright Copyright (C) 2013
// @author weishih<lvpengcheng6300@gmail.com>
//

#include "engine/data_file.h"

#include <string>

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "engine/status.h"
#include "engine/data_format.h"

DataFile::DataFile(const std::string& file_name) {
  data_file_ = file_name;
  data_fd_ = -1;
  memset(&super_block_, 0, sizeof(super_block_));
}

DataFile::~DataFile() {
}

Status DataFile::Open() {
  struct stat st_df;
  int rv = stat(data_file_.c_str(), &st_df);
  if (rv < 0) {
    return CreateDataFile();
  }
  return OpenDataFile();
}

Status DataFile::Close() {
  int rv = close(data_fd_);
  if (rv < 0) {
    fprintf(stderr, "Close data file %s error, %s\n", data_file_.c_str(), 
        strerror(errno));
    return Status::Error("Close data file error.");
  }
  return Status::OK();
}

Status DataFile::Sync() {
  WriteSuperBlock();  // write super block to disk

  int rv = fsync(data_fd_);
  if (rv < 0) {
    fprintf(stderr, "Sync data file %s to disk error, %s\n", 
        data_file_.c_str(), strerror(errno));
    return Status::Error("Sync data file to disk error.");
  }
  return Status::OK();
}

int DataFile::AllocateFreeBlock() {
  if (0 != super_block_.index_nr) {
    return super_block_.indices[super_block_.index_nr-- - 1];
  } else {
    return super_block_.block_nr++;
  }
}

Status DataFile::GetBlock(int block_id, DataBlock* block) {
  assert(NULL != block);
  return ReadBlock(block_id, block);
}

Status DataFile::PutBlock(int block_id, DataBlock* block) {
  assert(NULL != block);
  return WriteBlock(block_id, block);
}

Status DataFile::DeleteBlock(int block_id) {
  assert(super_block_.index_nr <= 1018);
  super_block_.indices[super_block_.index_nr++] = block_id;
  return Status::OK();
}

Status DataFile::CreateDataFile() {
  data_fd_ = open(data_file_.c_str(), O_RDWR | O_CREAT | O_EXCL, 
      S_IRUSR | S_IWUSR);
  if (data_fd_ < 0) {
    fprintf(stderr, "Create data file %s error, %s\n", data_file_.c_str(),
        strerror(errno));
    return Status::Error("Create data file error.");
  }
  super_block_.header.block_size = 4;  // 4KB block
  return Status::OK();
}

Status DataFile::OpenDataFile() {
  data_fd_ = open(data_file_.c_str(), O_RDWR);
  if (data_fd_ < 0) {
    fprintf(stderr, "Open data file %s error, %s\n", data_file_.c_str(),
        strerror(errno));
    return Status::Error("Open data file error.");
  }

  int rv = read(data_fd_, &super_block_, sizeof(super_block_));
  if (rv < 0) {
    fprintf(stderr, "Read super block from data file %s error %s\n", 
        data_file_.c_str(), strerror(errno));
    close(data_fd_);
    return Status::Error("Read super block from data file error.");
  }

  return Status::OK();
}

Status DataFile::ReadBlock(int block_id, DataBlock* block) {
  assert(NULL != block);
  int rv = pread(data_fd_, block, sizeof(*block), 
      sizeof(super_block_) + block_id * sizeof(DataBlock));
  if (rv < 0) {
    fprintf(stderr, "Read data block %d from data file %s error, %s\n",
        block_id, data_file_.c_str(), strerror(errno));
    return Status::Error("Read data block from data file error.");
  }
  return Status::OK();
}

Status DataFile::WriteBlock(int block_id, DataBlock* block) {
  assert(NULL != block);
  int rv = pwrite(data_fd_, block, sizeof(*block),
      sizeof(super_block_) + block_id * sizeof(DataBlock));
  if (rv < 0) {
    fprintf(stderr, "Write data block %d to data file %s error, %s\n", 
        block_id, data_file_.c_str(), strerror(errno));
    return Status::Error("Write data block to data file error.");
  }
  return Status::OK();
}

Status DataFile::WriteSuperBlock() {
  int rv = write(data_fd_, &super_block_, sizeof(super_block_));
  if (rv < 0) {
    fprintf(stderr, "Write super block of data file %s error, %s\n.",
        data_file_.c_str(), strerror(errno));
    return Status::Error("Write super block of data file error.");
  }
  return Status::OK();
}

