/***********************************************************************
 *
 * File: include/stmdisplaytypes.h
 * Copyright (c) 2000, 2004, 2005, 2007 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef STM_DISPLAY_TYPES_H
#define STM_DISPLAY_TYPES_H

#if defined(__cplusplus)
extern "C" {
#endif

typedef long  LONG;
typedef short SHORT;
typedef char  CHAR;
typedef unsigned long  ULONG;
typedef unsigned short USHORT;
typedef unsigned char  UCHAR;
typedef int BOOL;

__extension__ typedef long long TIME64;
__extension__ typedef unsigned long long UTIME64;
__extension__ typedef unsigned long long LONGLONG;
__extension__ typedef unsigned long long ULONGLONG;

#define OFFSETOF(X,Y) ((ULONG)&(((X*)0)->Y))


#define LIKELY(x)     __builtin_expect(!!(x), 1)
#define UNLIKELY(x)   __builtin_expect(!!(x), 0)

// first one doesn't work with g++ :-(
#ifndef __cplusplus
/* Force a compilation error if condition is true, but also produce a
   result (of value 0 and type size_t), so the expression can be used
   e.g. in a structure initializer (or where-ever else comma expressions
   aren't permitted). */
#define BUILD_BUG_ON_ZERO(e) (sizeof(char[1 - 2 * !!(e)]) - 1)

/* &a[0] degrades to a pointer: a different type from an array */
#define __must_be_array(a) \
  BUILD_BUG_ON_ZERO(__builtin_types_compatible_p(typeof(a), typeof(&a[0])))

#define N_ELEMENTS(array) (sizeof (array) / sizeof ((array)[0]) + __must_be_array (array))
#else
#define N_ELEMENTS(array) (sizeof (array) / sizeof ((array)[0]))
#endif



/* align addr on a size boundary - adjust address up/down if needed */
#define _ALIGN_UP(addr,size)	(((addr)+((size)-1))&(~((size)-1)))
#define _ALIGN_DOWN(addr,size)	((addr)&(~((size)-1)))

/* align addr on a size boundary - adjust address up if needed */
#define _ALIGN(addr,size)     _ALIGN_UP(addr,size)


/*
 * simple representation of a rational number, used for aspect ratios.
 */
typedef struct {
  long  numerator;
  long  denominator;
} stm_rational_t;


#if defined(__cplusplus)
}
#endif

#endif /* STM_DISPLAY_TYPES_H */
