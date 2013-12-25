#ifndef __BITOPS64_H__
#define __BITOPS64_H__

/*
 *
 * reference by 
 *				linux kernel source: /include/asm-x86/bitops_64.h
 * 				http://datao0907.blog.chinaunix.net
 *				http://www.groad.net/bbs/simple/?t3154.html
 */

#include <limits.h>

#include "stp_types.h"

#ifdef BITS_PER_LONG
#undef BITS_PER_LONG
#endif

#define BITS_PER_LONG 32

#define LOCK_PREFIX_64 \
  ".section .smp_locks,\"a\"\n"                \
  ".align 8\n"                                \
  ".long 661f\n" /* offset */                  \
  ".previous\n"                                \
  "661:\n\tlock;"                              \
  
#define LOCK_PREFIX_32 \
  ".section .smp_locks,\"a\"\n"                \
  ".align 4\n"                                \
  ".long 661f\n" /* offset */                  \
  ".previous\n"                                \
  "661:\n\tlock;"                              \

#if __WORDSIZE == 64
	#define LOCK_PREFIX  LOCK_PREFIX_64
#else
	#define LOCK_PREFIX LOCK_PREFIX_32
#endif

#if __GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 1)
	#define ADDR  "=m"  (*(volatile long *)addr)
#else
	#define ADDR  "+m" (*(volatile long *)addr)
#endif

static inline void set_bit(int nr,volatile void *addr) 
{
  __asm__ volatile( LOCK_PREFIX 
                      "btsl %1,%0"
                      :ADDR
                      :"dIr" (nr):"memory");
}

static inline void __set_bit(int nr,volatile void *addr) 
{
  __asm__ volatile( 
                      "btsl %1,%0"
                      :ADDR
                      :"dIr" (nr):"memory");

}

static inline void clear_bit(int nr,volatile void *addr)
{
  __asm__ volatile( LOCK_PREFIX 
                      "btr %1,%0"
                      :ADDR
                      :"Ir" (nr));
  /*
  __asm__ volatile( LOCK_PREFIX 
                    "btrl 1%,0%"
                    :ADDR
                    :"dIr" (nr):"memory");
  */
}

static inline int change_bit(int nr,volatile void *addr)
{
  int oldbit;
  
  __asm__ volatile(LOCK_PREFIX
                       "btsl %2,%1\n\tsbbl %0,%0"
                       :"=r" (oldbit),ADDR
                       :"dIr"(nr)
                       :"memory");
  return oldbit;
}


static inline u32 __scanbit(u32 val,u32 max)
{
  asm("bsf %1,%0 ; cmovz %2,%0" :"=&r" (val) :"r"(val),"r"(max));
  return val;
}

#define find_first_bit(addr,size)                           \
  ((__builtin_constant_p(size) && (size) <= BITS_PER_LONG?  \
    (__scan_bit(*(unsigned long *)addr,(size))):            \
	BITS_PER_LONG))

#define find_next_bit(addr,size,off)                        \
  ((__builtin_constant_p(size) && (size) <= BITS_PER_LONG?  \
    ((off) + (__scanbit((*addr) >> (off),(size)-(off)))): \
    BITS_PER_LONG))

#define find_first_zero_bit(addr,size)                                \
  ((__builtin_constant_p(size) && (size) <= BITS_PER_LONG?            \
    (__scanbit(~*addr,(size))):                       \
    BITS_PER_LONG))

#define find_next_zero_bit(addr,size,off)                               \
  ((__builtin_constant_p(size) && (size) <= BITS_PER_LONG?              \
    ((off) + (__scanbit((~*(unsigned long *)addr) >> (off),(size)-(off)))):\
    BITS_PER_LONG))

static inline void set_bit_string(u32 *bitmap,unsigned long i,int len)
{
  unsigned long end = i + len;
  while(i < end) {
    __set_bit(i,bitmap);
    i++;
  }
}


static inline void __clear_bit_string(u32 *bitmap,unsigned long i,int len)
{
  unsigned long end = i + end;
  
  while(i < end) {
    __clear_bit(i,bitmap);
    i++;
  }
  
}

//same as __scanbit ,but in different word size
static inline int __ffs(int x)
{
  int r;
  
  __asm__("bsfl %1,%0\n\t"
          "cmovzl %2,%0"
          :"=r" (r)
          :"rm"(x),"r"(-1));
  
  return r+1;
}

//find 1 from low bit
static inline int __fls(int x)
{
  int r;
  
  __asm__("bsrl %1,%0\n\t"
          "cmovz %2,%0"
          :"=&r"(r):"rm"(x),"rm"(-1));
  return r+1;
}



#endif
