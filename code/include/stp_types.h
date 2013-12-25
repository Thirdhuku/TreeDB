#ifndef __STP_TYPE_H__
#define __STP_TYPE_H__

#include <stdint.h>

typedef uint32_t u32;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint64_t u64;

typedef int32_t s32;
typedef int8_t s8;
typedef int16_t s16;
typedef int64_t s64;

typedef unsigned long ptr_t;

#define BITS_PER_U32 (sizeof(u32)*8)

#define U32_MAX  ((u32)-1)

#define BITMAP_LAST_WORD_MASK(nbits)            \
  (                                             \
   ((nbits % BITS_PER_U32)) ?                  \
   (1UL << (nbits % BITS_PER_U32)) -1 : ~0UL   \
  )

#define BITMAP_LAST_WORD_ZERO(nbits)            \
    (                                           \
     ((nbits % BITS_PER_U32))?                      \
     (~0UL >> (nbits % BITS_PER_U32)):0UL           \
                                                )

#define BITS_PER_BYTE 8

#define DIV_ROUND_UP(n,d)  (((n) + (d) -1) / (d))

#define BITS_TO_U32(nr)  DIV_ROUND_UP(nr,BITS_PER_BYTE * sizeof(u32))


#endif
