#ifndef __STP_ERROR_H__
#define __STP_ERROR_H__

#define STP_NO_ERROR  0
#define STP_INDEX_OPEN_ERROR  1
#define STP_META_OPEN_ERROR   2	
#define STP_INDEX_READ_ERROR  3
#define STP_META_READ_ERROR   4
#define STP_INDEX_WRITE_ERROR  5
#define STP_META_WRITE_ERROR   6
#define STP_BAD_MAGIC_NUMBER   7
#define STP_META_CANT_BE_WRITER     8
#define STP_INDEX_CANT_BE_WRITER    9
#define STP_INDEX_CANT_BE_READER     10
#define STP_META_CANT_BE_READER      11
#define STP_INDEX_READER_CANT_STORE  12
#define STP_META_READER_CANT_STORE   13
#define STP_INDEX_READER_CANT_DELETE 14
#define STP_META_READER_CANT_DELETE 15
#define STP_INDEX_READER_CANT_COMPACTION  16
#define STP_META_READER_CANT_COMPACTION   17
#define STP_INDEX_READER_CANT_UPDATE   18
#define STP_META_READER_CANT_UPDATE  19
#define STP_INDEX_ITEM_NO_FOUND      20
#define STP_META_ITEM_NO_FOUND       21
#define STP_INDEX_ILLEAGAL_DATA      22
#define STP_META_ILLEAGAL_DATA        23
#define STP_INDEX_NO_SPACE          24
#define STP_META_NO_SPACE           25    
#define STP_MALLOC_ERROR           26
#define STP_INDEX_CREAT_ERROR      27
#define STP_META_CREAT_ERROR       28
#define STP_INDEX_FILE_CHECK_ERROR  29
#define STP_META_FILE_CHECK_ERROR  30
#define STP_INODE_MALLOC_ERROR     31
#define STP_BNODE_MALLOC_ERROR     32
#define STP_INVALID_ARGUMENT       33
#define STP_NO_SYSCALL             34
#define STP_INDEX_EXIST 	       35
#define STP_INDEX_MAX_LEVEL	       36
#define STP_INDEX_NOT_EXIST        37
#define STP_FS_ENTRY_EXIST 	       38
#define STP_FS_ENTRY_NOEXIST       39
#define STP_FS_ENTRY_FULL          40
#define STP_FS_UNKNOWN_ERROR       41
#define STP_FS_INO_NOEXIST         42
#define STP_FS_DIR_NOEMPTY	       43
#define STP_FS_NO_DIR			   44
#define STP_FS_ROOT                45
#define STP_FS_NODIR			   46

#define STP_MIN_ERRNO             0
#define STP_MAX_ERRNO             46

#define N_(s)  (s)
#define _(s)  ((const char *)s)

#endif
