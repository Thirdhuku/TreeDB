#ifndef __BITMAP_H__
#define __BITMAP_H__

#include "stp_types.h"
#include "bitops.h"
#include <string.h>
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif
/*
 * bitmap operations for bltmap-allocation
 *
 */

static inline int bitmap_clean(u32 *bitmap,unsigned long bits)
{

    int k,lim = bits/BITS_PER_U32;
    
    for(k = 0;k < lim;k++)
        bitmap[k] &= 0UL;

    #ifdef DEBUG
    #endif 

    bitmap[k] &= BITMAP_LAST_WORD_ZERO(bits);
    

    #ifdef DEBUG
    printf("bitmap[0]:%x\n",bitmap[0]);
    #endif
}
    
    


static inline int bitmap_empty(const u32 *bitmap,int bits) 
{
  int k,lim = bits / BITS_PER_U32;
  
  for(k = 0;k < lim;k++) 
    if(bitmap[k])
      	return 0;
  
  if(bits % BITS_PER_U32)
    if(bitmap[k] & BITMAP_LAST_WORD_MASK(bits))
      return 0;
  
  return 1;
}

    
static inline void bitmap_fill(u32 *bitmap,int bits)
{
    u32 nlongs = BITS_TO_U32(bits);
    
    if(nlongs > 1) {
        int len = (nlongs - 1) * sizeof(u32);
        memset(bitmap,0xff,len);
    }
    
    bitmap[nlongs - 1] = BITMAP_LAST_WORD_MASK(bits);
}


static inline u32 bitmap_find_first_zero_bit(u32 *bitmap,unsigned long start,int len)
{
    return __bitmap_alloc(bitmap,start,len);
}

static inline void bitmap_set(u32 *bitmap,unsigned long off)
{
    int k;
    
    k = off / BITS_PER_U32;
    off = off % BITS_PER_U32;
    
#ifdef DEBUG
    printf("before set bitmap[%d]:%lx,off:%ld\n",k,bitmap[k],off);
#endif

    set_bit((BITS_PER_U32 - off -1 ),&bitmap[k]);

#ifdef DEBUG
    printf("after set bitmap[%d]:%lx,off:%ld\n",k,bitmap[k],off);
#endif
}

static inline void bitmap_clear(u32 *bitmap,unsigned long off)    
{
    int k;
    
    k = off / BITS_PER_U32;
    off = off % BITS_PER_U32;
    
#ifdef DEBUG
    fprintf(stderr,"before clear bitmap[%u]:%lx,off:%ld\n",k,bitmap[k],off);
#endif
    clear_bit((BITS_PER_U32 - off - 1),&bitmap[k]);

#ifdef DEBUG
    fprintf(stderr,"after clear bitmap[%u]:%lx,off:%ld\n",k,bitmap[k],off);
#endif
    //clear_bit((BITS_PER_U32 - k),&bitmap[off]);
}
    

#ifdef __cplusplus
}
#endif

#endif
