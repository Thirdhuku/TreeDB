//
// @file block_db.h
// @brief 
// 
// @version 1.0
// @date Thu May 23 18:41:58 2013
// 
// @copyright Copyright (C) 2013
// @author lpc<lvpengcheng6300@gmail.com>
//

#include "engine/db.h"

#include <string>

#include "engine/data_format.h"
#include "engine/data_file.h"
#include "engine/index_file.h"

class BlockDb : public Db {
 public:
  BlockDb();
  ~BlockDb();

  virtual Status Open(const std::string& db_path);
  virtual Status Close();
  virtual Status Put(uint64_t key, const std::string& value);
  virtual Status Get(uint64_t key, std::string* value) const;
  virtual Status Delete(uint64_t key);

 private:
  std::string db_path_;
  DataFile* data_file_;
  IndexFile* index_file_;
};

