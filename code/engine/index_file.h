//
// @file index_file.h
// @brief Abstraction of index file.
// 
// @version 1.0
// @date Tue Jul  2 14:32:14 2013
// 
// @copyright Copyright (C) 2013
// @author weishih<lvpengcheng6300@gmail.com>
//

#include <string>

#include "engine/status.h"
#include "engine/data_format.h"

class IndexFile {
 public:
  IndexFile(const std::string& file_name);
  ~IndexFile();

  Status Open();
  Status Close();
  Status Sync();

  Status GetIndex(uint64_t key, IndexRecord* index);
  Status PutIndex(const IndexRecord& index);
  Status DeleteIndex(uint64_t key);

 protected:
  std::string index_file_;
};

