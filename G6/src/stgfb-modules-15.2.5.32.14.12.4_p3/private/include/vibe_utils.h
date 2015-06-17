/***********************************************************************
 *
 * File: private/include/vibe_utils.h
 * Copyright (c) 2007-2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/
#ifndef VIBE_UTILS_H
#define VIBE_UTILS_H

#if defined(__cplusplus)

/*
 * Internal build type definitions and macros.
 */

__extension__ typedef unsigned long long stm_utime64_t;


#define OFFSETOF(X,Y) ((uint32_t)&(((X*)0)->Y))

#define LIKELY(x)     __builtin_expect(!!(x), 1)
#define UNLIKELY(x)   __builtin_expect(!!(x), 0)

// first one doesn't work with g++ :-(
#if 0
/* Force a compilation error if condition is true, but also produce a
   result (of value 0 and type size_t), so the expression can be used
   e.g. in a structure initializer (or where-ever else comma expressions
   aren't permitted). */
#define BUILD_BUG_ON_ZERO(e) (sizeof(struct { int:-!!(e); }))

/* &a[0] degrades to a pointer: a different type from an array */
#define __must_be_array(a) \
  BUILD_BUG_ON_ZERO(__builtin_types_compatible_p(typeof(a), typeof(&a[0])))

#define N_ELEMENTS(array) (sizeof (array) / sizeof ((array)[0]) + __must_be_array (array))
#else
#define N_ELEMENTS(array) (sizeof (array) / sizeof ((array)[0]))
#endif

#define MIN(a,b)    ((a) < (b) ? (a) : (b))
#define MAX(a,b)    ((a) > (b) ? (a) : (b))

/* Division of 2 integers with result rounded up or down */
#define DIV_ROUNDED_UP(num, den)        ( ( (num) + (den) - 1) / (den))
#define DIV_ROUNDED(num, den)           ( ( (num) + (den/2) - 1) / (den))
#define DIV_ROUNDED_DOWN(num, den)      ( (num) / (den) )

/* align addr on a size boundary - adjust address up/down if needed */
#define _ALIGN_UP(addr,size)    (((addr)+((size)-1))&(~((size)-1)))
#define _ALIGN_DOWN(addr,size)  ((addr)&(~((size)-1)))

/* align addr on a size boundary - adjust address up if needed */
#define _ALIGN(addr,size)     _ALIGN_UP(addr,size)


#endif /* __cplusplus */

#endif /* VIBE_UTILS_H */
