#ifndef __BITOPS_H__
#define __BITOPS_H__

#ifdef __cplusplus

extern "C" 
{

#endif    

#include <limits.h>

#if __WORDSIZE == 64
	#include "bitops_64.h"
#else
	#include "bitops_32.h"
#endif

#ifdef __cplusplus
}
#endif

#endif
