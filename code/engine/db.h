/*
 * @file db.h
 * @brief 
 * 
 * @version 1.0
 * @date Mon May 20 10:50:18 2013
 * 
 * @copyright Copyright (C) 2013 UESTC
 * @author lpc<lvpengcheng6300@gmail.com>
 */

#ifndef ENGINE_DB_H_
#define ENGINE_DB_H_

#include "status.h"

class Db {
 public:
  Db() {}
  virtual ~Db() {};

  virtual Status Open(const std::string& db_path);
  virtual Status Close();
  virtual Status Put(const std::string& key, const std::string& value) = 0;
  virtual Status Get(const std::string& key, std::string* value) const = 0;
  virtual Status Delete(const std::string& key) = 0;
};

#endif  // ENGINE_DB_H_

