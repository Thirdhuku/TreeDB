//
// @file data_file.h
// @brief Abstration of the data file.
// 
// @version 1.0
// @date Mon Jul  1 10:29:39 2013
// 
// @copyright Copyright (C) 2013
// @author weishih<lvpengcheng6300@gmail.com>
//

#include <string>

#include "engine/status.h"
#include "engine/data_format.h"

/// @brief See the file comment.
class DataFile {
 public:
  DataFile(const std::string& file_name);
  ~DataFile();

  Status Open();
  Status Close();
  Status Sync();

  int AllocateFreeBlock();
  Status GetBlock(int block_id, DataBlock* block);
  Status PutBlock(int block_id, DataBlock* block);
  Status DeleteBlock(int block_id);

 private:
  Status CreateDataFile();
  Status OpenDataFile();
  Status ReadBlock(int block_id, DataBlock* block);
  Status WriteBlock(int block_id, DataBlock* block);
  Status WriteSuperBlock();

  std::string data_file_;
  int data_fd_;
  SuperBlock super_block_;
};

