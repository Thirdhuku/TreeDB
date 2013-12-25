//
// @file map_index_file.h
// @brief Abstraction of index file.
// 
// @version 1.0
// @date Tue Jul  2 14:32:14 2013
// 
// @copyright Copyright (C) 2013
// @author weishih<lvpengcheng6300@gmail.com>
//

#include "engine/index_file.h"

#include <string>
#include <map>

#include "engine/status.h"
#include "engine/data_format.h"

class MapIndexFile : public IndexFile {
 public:
  MapIndexFile(const std::string& file_name);
  ~MapIndexFile();

  Status Open();
  Status Close();
  Status Sync();

  Status GetIndex(uint64_t key, IndexRecord* index);
  Status PutIndex(const IndexRecord& index);
  Status DeleteIndex(uint64_t key);

 private:
  Status CreateIndexFile();
  Status OpenIndexFile();

  int index_fd_;
  std::map<uint64_t, uint64_t> index_;
};

