/*
 * @file data_format.h
 * @brief 
 * 
 * @version 1.0
 * @date Tue May 21 11:34:00 2013
 * 
 * @copyright Copyright (C) 2013 UESTC
 * @author lpc<lvpengcheng6300@gmail.com>
 */

#ifndef ENGINE_DATA_FORMAT_H_
#define ENGINE_DATA_FORMAT_H_

#include <sys/types.h>
#include <inttypes.h>

struct FileHeader {
  uint8_t version;
  uint8_t file_type;
  uint8_t block_size;
  uint64_t scn;
} __attribute__((packed));

struct SuperBlock {
  FileHeader header;
  uint32_t block_nr;
  uint32_t index_nr;
  uint32_t indices[1018]; // make it a 4KB block
} __attribute__((packed));

struct Record {
  uint64_t key;
  char data[92];
} __attribute__((packed));  // 100 bytes

struct DataBlock {
  uint16_t items;
  char padding[94];
  Record records[40];
} __attribute__((packed));  // 4KB block

struct IndexRecord {
  uint64_t key;
  uint64_t block_id;
} __attribute__((packed));  // 16 bytes index record

#endif  // ENGINE_DATA_FORMAT_H_

