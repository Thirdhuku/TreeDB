#include "stp_types.h"
#include "bitops.h"

#include <stdio.h>
#include <assert.h>
#include <sys/types.h>

static int __get_remain(off_t start,size_t len)
{
    len = len - (BITS_PER_U32 - start);
    while(len  > BITS_PER_U32) 
        len -= BITS_PER_U32;
    return len;
}

u32  __bitmap_alloc(u32 *bitmap,off_t start,size_t len)
{
    int k,lim,s1,s2,pos;
    u32 b,e;
    
    k = start / BITS_PER_U32;
    s1 = start % BITS_PER_U32;
    
    lim = (len >= BITS_PER_U32 ? BITS_PER_U32:len);
    
#ifdef DEBUG
    printf("begin index:%d,offset:%d,start:%ld,len:%u\n",k,s1,start,len);
#endif

    b = bitmap[k] | (~0UL <<(BITS_PER_U32 - s1));
    if((pos = find_first_zero_bit(&b,lim)) != lim) 
    {
        #ifdef DEBUG
        printf("find in first u32,pos:%d\n",pos);
        #endif
        return BITS_PER_U32 - (pos+1) + k * BITS_PER_U32;
    }
    
    lim = (start + len) / BITS_PER_U32;
    
    #ifdef DEBUG
    printf("k:%d,lim:%d\n",k,lim);
    #endif
    
    for(k=k+1;k < lim;k ++) 
    {
        if((pos = find_first_zero_bit(&bitmap[k],BITS_PER_U32)) != BITS_PER_U32)
        {     
			#ifdef DEBUG
            printf("find in internl u32,pos:%d\n",pos);
            #endif
            return k*BITS_PER_U32 + BITS_PER_U32 - pos - 1;
        }
        //        else printf("pos:%d,bitmap[%d]:%x\n",pos,k,bitmap[k]);
    }
    
    s2 = __get_remain(start,len) + 1;
    
	#ifdef DEBUG
    printf("find s2:%d\n",s2);
    #endif
    assert(s2 != 0);
    if(!s2) return 0;
    
    b = bitmap[k] | (~0UL << s2);
    if((pos = find_first_zero_bit(&b,BITS_PER_U32)) != BITS_PER_U32)
    {
        #ifdef DEBUG
        printf("find in last u32,pos:%d\n",pos);
        #endif
        return (k+1) * BITS_PER_U32 - pos - 1;
    }
    //printf("don't find the last one,pos:%d\n",pos);
    return 0;
}
