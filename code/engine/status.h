/*
 * @file status.h
 * @brief 
 * 
 * @version 1.0
 * @date Mon May 13 16:50:43 2013
 * 
 * @copyright Copyright (C) 2013 UESTC
 * @author lpc<lvpengcheng6300@gmail.com>
 */

#ifndef ENGINE_STATUS_H_
#define ENGINE_STATUS_H_

#include <string>

#include "util.h"

class Status
{
 public:
  Status() : code_(kOk) {}
  ~Status() {}

  static Status OK() { return Status(); }
  static Status Error(const std::string& msg) {
    return Status(kError, msg);
  }
  static Status NotFound(const std::string& msg) {
    return Status(kNotFound, msg);
  }

  bool Ok() const { return (kOk == code_) ? true : false; }
  bool IsNotFound() const {return (kNotFound == code_) ? true : false;};

  std::string ToString() const;

  Status(const Status &status);
  Status& operator=(const Status &status);
 
 private:
  enum Code {
    kOk = 0,
    kError = 1,
    kNotFound = 2
  };

  Status(Code code, const std::string& msg) : code_(code), message_(msg) {}

  static const char* code_messages_[];
  Code code_;
  std::string message_;
};

const char* Status::code_messages_[] = {
    "Success",
    "Error",
    "NotFound",
  };

inline std::string Status::ToString() const {
  return "Status code " + util::conv<std::string, Code>(code_)
      + ", " + code_messages_[code_]
      + ", " + message_;
}

inline Status::Status(const Status &status) {
  *this = status;
}

inline Status&  Status::operator=(const Status &status) {
  code_ = status.code_;
  message_ = status.message_;
  return *this;
}

#endif  // ENGINE_STATUS_H_

