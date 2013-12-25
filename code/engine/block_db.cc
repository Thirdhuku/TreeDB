//
// @file block_db.cc
// @brief 
// 
// @version 1.0
// @date Thu May 23 20:03:30 2013
// 
// @copyright Copyright (C) 2013
// @author lpc<lvpengcheng6300@gmail.com>
//

#include "block_db.h"

#include <string>
#include <memory>

#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "status.h"
#include "engine/index_file.h"
#include "engine/data_file.h"
#include "engine/data_format.h"

BlockDb::BlockDb() {
  data_file_ = NULL;
  index_file_ = NULL;
}

Status BlockDb::Open(const std::string& db_path) {
  db_path_ = db_path;
  std::string data_file_path = db_path + "/data";
  std::string index_file_path = db_path + "/index";

  data_file_ = new DataFile(data_file_path);
  index_file_ = new IndexFile(index_file_path);

  struct stat st_db;
  if (stat(db_path_.c_str(), &st_db) < 0) {
    int rc = mkdir(db_path_.c_str(), S_IRWXU);
    if (rc < 0) {
      fprintf(stderr, "mkdir error, %s, %s\n", 
          db_path_.c_str(), strerror(errno));
      return Status::Error("mkdir for db error");
    }
  }

  Status st = data_file_->Open();
  if (!st.Ok()) {
    fprintf(stderr, "open data file %s error\n", data_file_path.c_str());
    return Status::Error("open data file error");
  }

  st = index_file_->Open();
  if (!st.Ok()) {
    fprintf(stderr, "open index file %s error\n", index_file_path.c_str());
    return Status::Error("open index file error");
  }

  return Status::OK();
}

Status BlockDb::Close() {
  Status st = data_file_->Close();
  if (!st.Ok()) {
    fprintf(stderr, "close data file error\n");
    return Status::Error("close data file error");
  }

  st = index_file_->Close();
  if (!st.Ok()) {
    fprintf(stderr, "close index file error\n");
    return Status::Error("close index file error");
  }

  return Status::OK();
}

Status BlockDb::Put(uint64_t key, const std::string& value) {
  IndexRecord ir;
  Status st = index_file_->GetIndex(key, &ir);
  if (!st.Ok()) {
    return Status::Error("get index error");
  }

  std::auto_ptr<DataBlock> block = std::auto_ptr<DataBlock>(new DataBlock());
  st = data_file_->GetBlock(ir.block_id, block.get());
  if (!st.Ok()) {
    fprintf(stderr, "get block error\n");
    return Status::Error("get block error");
  }

  Record* recs = block->records;
  int items = block->items;
  int found = -1;
  int left = 0;
  int right = items - 1;
  int center = 0;
  
  while (left < right) {
    center = (left + right) / 2;
    if (recs[center].key == key) {
      found = center;
      break;
    }
    if (recs[center].key > key) {
      right = center - 1;
    } else {
      left = center + 1;
    }
  }

  // found, update
  if (-1 != found) {
    memset(recs[found].data, 0, 92);
    memcpy(recs[found].data, value.c_str(), value.size());
    return data_file_->PutBlock(ir.block_id, block.get());
  }
 
  // not found, insert

  // not full
  if (items < 40) {
    int insert_pos = -1;
    for (int i = 0; i < items; ++i) {
      if (recs[i].key > key) {
        insert_pos = i;
        break;
      }
    }
    if (insert_pos == -1) {
      // append to end
      insert_pos = items;
    } else {
      // insert
      memmove(recs + insert_pos + 1, recs + insert_pos, 
          sizeof(Record) * (items - insert_pos));
    }
    recs[insert_pos].key = key;
    memset(recs[insert_pos].data, 0, 92);
    memcpy(recs[insert_pos].data, value.c_str(), value.size());
    ++block->items;

    st = data_file_->PutBlock(ir.block_id, block.get());
    if (!st.Ok()) {
      return st;
    }

    if (insert_pos == 0) {
      // update index
      st = index_file_->DeleteIndex(ir.key);
      if (!st.Ok()) {
        return st;
      }
      IndexRecord nir;
      nir.key = key;
      nir.block_id = ir.block_id;
      st = index_file_->PutIndex(nir);
      return st;
    }
    return Status::OK();
  }

  // full, split
  int split_pos = items / 2;
  std::auto_ptr<DataBlock> nblock = std::auto_ptr<DataBlock>(new DataBlock());
  int nb_cnt = 0;
  for (int i = split_pos; i < items; ++i) {
    nblock->records[nb_cnt++] = recs[i];
  }
  nblock->items = nb_cnt;
  block->items -= nb_cnt;
  st = data_file_->PutBlock(ir.block_id, block.get());
  if (!st.Ok()) {
    return st;
  }
  int nblock_id = data_file_->AllocateFreeBlock();
  st = data_file_->PutBlock(nblock_id, nblock.get());
  if (!st.Ok()) {
    return st;
  }
  // insert new index
  IndexRecord nir;
  nir.key = nblock->records[0].key;
  nir.block_id = nblock_id;
  st = index_file_->PutIndex(nir);
  if (!st.Ok()) {
    return st;
  }

  // insert recursively after split
  return Put(key, value);
}

Status BlockDb::Get(uint64_t key, std::string* value) const {
  IndexRecord ir;
  Status st = index_file_->GetIndex(key, &ir);
  if (!st.Ok()) {
    return st;
  }

  std::auto_ptr<DataBlock> block = std::auto_ptr<DataBlock>(new DataBlock());
  st = data_file_->GetBlock(ir.block_id, block.get());
  if (!st.Ok()) {
    fprintf(stderr, "get block error\n");
    return Status::Error("get block error");
  }

  Record* recs = block->records;
  int items = block->items;
  int left = 0;
  int right = items - 1;
  int center = 0;
  
  while (left < right) {
    center = (left + right) / 2;
    if (recs[center].key == key) {
      *value = recs[center].data;
      return Status::OK();
    }
    if (recs[center].key > key) {
      right = center - 1;
    } else {
      left = center + 1;
    }
  }
  
  return Status::NotFound("key not found");
}

Status BlockDb::Delete(uint64_t key) {
  IndexRecord ir;
  Status st = index_file_->GetIndex(key, &ir);
  if (!st.Ok()) {
    return st;
  }

  std::auto_ptr<DataBlock> block = std::auto_ptr<DataBlock>(new DataBlock());
  st = data_file_->GetBlock(ir.block_id, block.get());
  if (!st.Ok()) {
    fprintf(stderr, "get block error\n");
    return Status::Error("get block error");
  }

  Record* recs = block->records;
  int items = block->items;
  int found = -1;
  int left = 0;
  int right = items - 1;
  int center = 0;
  
  while (left < right) {
    center = (left + right) / 2;
    if (recs[center].key == key) {
      found = center;
      break;
    }
    if (recs[center].key > key) {
      right = center - 1;
    } else {
      left = center + 1;
    }
  }

  if (-1 == found) {
    return Status::NotFound("key not found while delete");
  }

  // delete
  void* dest = memmove((char*)(recs + found), (char*)(recs + found + 1),
      sizeof(Record) * (items - found - 1));
  if (NULL == dest) {
    fprintf(stderr, "memmove error, %s\n", strerror(errno));
    return Status::Error("memmove error while deleteing record");
  }

  if (--block->items == 0) {
    st = data_file_->DeleteBlock(ir.block_id);
    if (!st.Ok()) {
      return st;
    }
    st = index_file_->DeleteIndex(ir.key);
    if (!st.Ok()) {
      return st;
    }
  } else if(0 == found) {
    // first record deleted
    st = index_file_->DeleteIndex(ir.key);
    if (!st.Ok()) {
      return st;
    }
    IndexRecord nir;
    nir.key = recs[0].key;
    nir.block_id = ir.block_id;
    st = index_file_->PutIndex(nir);
    if (!st.Ok()) {
      return st;
    }
  }
  
  return data_file_->PutBlock(ir.block_id, block.get());
}

