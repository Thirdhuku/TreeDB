#ifndef __STP_INTERNAL_H_
#define __STP_INTERNAL_H_

#include "stp_types.h"

struct stp_fs_info;
struct stp_btree_info;
struct stp_inode;

int __fs_read_inode(struct stp_fs_info *,u64,struct stp_btree_info *,struct stp_inode **);


#endif
