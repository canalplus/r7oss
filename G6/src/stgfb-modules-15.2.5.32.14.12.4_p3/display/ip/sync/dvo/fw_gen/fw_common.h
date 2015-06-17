/***********************************************************************
 *
 * File: display/ip/sync/dvo/fw_gen/fw_common.h
 * Copyright (c) 2013 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef __FW_COMMON__
#define __FW_COMMON__

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __cplusplus
/*
 * We cannot include <linux/types.h> here because its definition
 * of bool clashes with C++.
 */
#if !defined(_STM_OS21_PLATFORM_H)
typedef unsigned char  bool;
typedef int            int32_t;
typedef short          int16_t;
typedef char           int8_t;
typedef unsigned int   uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char  uint8_t;
__extension__ typedef unsigned long long uint64_t;

/*
 * Some good old style C definitions for where we have imported code
 * from elsewhere.
 */
#define FALSE (1==2)
#define TRUE  (!FALSE)

#undef NULL
#if defined(__cplusplus)
#define NULL 0
#else
#define NULL ((void *)0)
#endif
#endif  // !defined(_STM_OS21_PLATFORM_H)

#endif

#ifdef __cplusplus
}
#endif

#endif /* __FW_COMMON__ */
