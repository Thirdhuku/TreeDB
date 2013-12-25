#ifndef __BITOPS32_H__
#define __BITOPS32_h__

#include "stp_types.h"

#define LOCK_PREFIX \
  ".section .smp_locks,\"a\"\n"                \
  ".align 4\n"                                \
  ".long 661f\n" /* offset */                  \
  ".previous\n"                                \
  "661:\n\tlock;"                              \

#define ADDR  (*(volatile long *)addr)

static inline void set_bit(int nr,volatile void *addr)
{
    __asm__ __volatile__( LOCK_PREFIX
                           "btsl %1,%0"
                           :"+m" (ADDR)
                           :"Ir" (nr):"memory");
}

static inline void __set_bit(int nr,volatile void *addr)
{
    __asm__ __volatile__(
                    "btsl %1,%0"
                    :"+m"(ADDR)
                    :"Ir" (nr):"memory");

}

static inline void clear_bit(int nr,volatile void *addr)
{
    __asm__ volatile(LOCK_PREFIX
                   "btrl %1,%0"
                   :"+m"(ADDR)
                   :"dIr"(nr));
}

static inline void change_bit(int nr,u32 *addr)
{
    __asm__ volatile(LOCK_PREFIX
                   "btcl %1,%0"
                   :"+m"(ADDR)
                   :"Ir"(nr));
}

static inline int test_and_set_bit(int nr,volatile u32 *addr)
{
    int oldbit;
    
    __asm__ __volatile__(LOCK_PREFIX
                         "btsl %2,%1\n\tsbbl %0,%0"
                         :"=r" (oldbit),"+m"(ADDR)
                         :"Ir"(nr):"memory");
    return oldbit;  
}

static inline int test_and_clear_bit(int nr,volatile u32 *addr)
{
    int oldbit;
    
    __asm__ __volatile__(LOCK_PREFIX
                         "btrl %2,%1\n\tsbbl %0,%0"
                         :"=r" (oldbit),"+m"(ADDR)
                         :"Ir"(nr):"memory");
}

/**
 *如果查找失败,返回最后越界的第一个字符位置,如32
 */
static inline int find_first_zero_bit(const u32 *addr,unsigned size)
{
    int d0,d1,d2;
    int res;
    
    if(!size)
        return 0;
    
    __asm__ __volatile__(
		"movl $-1,%%eax\n\t"
        "xorl %%edx,%%edx\n\t"
        "repe; scasl\n\t"
        "je 1f\n\t"
        "xorl -4(%%edi),%%eax\n\t"
        "subl $4,%%edi\n\t"
        "bsfl %%eax,%%edx\n"
        "1:\tsubl %%ebx,%%edi\n\t"
        "shll $3,%%edi\n\t"
        "addl %%edi,%%edx"
        :"=d"(res),"=&c" (d0),"=&D" (d1),"=&a"(d2)
        :"1" ((size + 31) >> 5),"2"(addr),"b"(addr):"memory");
    return res;
}

static inline int __find_first_zero_bit(const u32 addr,unsigned size)
{
    int d0,d1,d2;
    int res;
    
    if(!size)
        return 0;
    
    __asm__ __volatile__(
		"movl $-1,%%eax\n\t"
        "xorl %%edx,%%edx\n\t"
        "repe; scasl\n\t"
        "je 1f\n\t"
        "xorl -4(%%edi),%%eax\n\t"
        "subl $4,%%edi\n\t"
        "bsfl %%eax,%%edx\n"
        "1:\tsubl %%ebx,%%edi\n\t"
        "shll $3,%%edi\n\t"
        "addl %%edi,%%edx"
        :"=d"(res),"=&c" (d0),"=&D" (d1),"=&a"(d2)
        :"1" ((size + 31) >> 5),"2"(&addr),"b"(&addr):"memory");
    return res;
}


#define find_next_zero_bit(value,size,off)  ((off) + (__find_first_zero_bit(((value)>>(off)),((size) - (off)))))


#endif
