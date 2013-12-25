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

#include "engine/index_file.h"

#include <string>

#include "engine/status.h"
#include "engine/data_format.h"

#include "bitmap.h"
#include "stp_types.h"
#include "stp.h"

class BTreeIndexFile : public IndexFile {
 public:
  BTreeIndexFile(const std::string& file_name);
  ~BTreeIndexFile();

 private:
  Status OpenBtree();
  Status ReadBtreeInfo(int index_fd, int mode);

  struct stp_btree_info* btree_;
};

