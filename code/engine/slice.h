/*
 * @file slice.h
 * @brief 
 * 
 * @version 1.0
 * @date Mon May 20 10:15:38 2013
 * 
 * @copyright Copyright (C) 2013 UESTC
 * @author lpc<lvpengcheng6300@gmail.com>
 */

#ifndef ENGINE_SLICE_H_
#define ENGINE_SLICE_H_

#include <cassert>
#include <cstring>

#include <string>

class Slice {
 public:
  Slice() : data_(""), size_(0) {}
  Slice(const char* d, size_t n) : data_(d), size_(n) {}
  Slice(const std::string& s) : data_(s.data()), size_(s.size()) {}
  Slice(const char* s) : data_(s), size_(strlen(s)) {}

  ~Slice() { data_ = ""; size_ = 0; }

  const char* data() const { return data_ };
  size_t size() const { return size_; }
  bool IsEmpty() const { return size_ == 0; }
  void Clear() { data_ = ""; size_ = 0; }

  std::string ToString() const { return std::string(data_, size_); }

  char oerator[](size_t n) const {
      assert(n < size_);
      return data_[n];
  }

 private:
  const char* data_;
  size_t size_;
};

inline bool operator==(const Slice& x, const Slice& y) const {
    return (x.size() == y.size() 
            && (memcmp(x.data(), y.data()) == 0));
}

inline bool operator!=(const Slice& x, const Slice& y) const {
    return !(x == y);
}

#endif  // ENGINE_SLICE_H_

