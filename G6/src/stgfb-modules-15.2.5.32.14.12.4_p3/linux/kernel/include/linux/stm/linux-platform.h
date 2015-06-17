/***********************************************************************
 *
 * File: linux/kernel/include/linux/stm/linux-platform.h
 * Copyright (c) 2000-2010 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STM_LINUX_PLATFORM_H
#define _STM_LINUX_PLATFORM_H

#if defined(__cplusplus)
extern "C" {
#endif

/*! \file linux-platform.h
 *  \brief BUG_ON Linux specific macro definition for Vibe
 */

/* Frederic Hunsinger: (Nov. 2013 - kernel 3.10 migration)
 * Include chain in 3.10 has been changed, <asm/bug.h> provoque
 * the inclusion of <linux/kernel.h> which contains C types which
 * conflicts with C++ types.
 * following line is a hack to NOP <linux/kernel.h>
 */
#define _LINUX_KERNEL_H

#include <asm/bug.h>

/*
 * The ARM specific portions of the BUG() macro in the latest kernels
 * (>= linux-3.2) perform some compile time invariant checking. We cannot
 * take the default implementation of the invariant test macros because these
 * are contained in kernel headers we cannot allow the C++ compiler to observe
 * (linux/kernel.h). We work around this by providing an ineffective
 * alternative. We justify this with the claim that the invariant has been
 * checked quite a few times already during the kernel build process.
 */
#ifndef BUILD_BUG_ON
#define BUILD_BUG_ON(condition)
#endif

/*
 * Specific BUG_ON macro definition for Vibe
 */
#define STM_VIBE_BUG_ON(cond) BUG_ON(cond)

/*
 * Also give access to errno for the API entrypoints
 */
#include <linux/errno.h>
#define ENOTSUP EOPNOTSUPP

/*
 * We cannot include <linux/types.h> here because its definition
 * of bool clashes with C++.
 */
typedef int            int32_t;
typedef short          int16_t;
typedef char           int8_t;
typedef unsigned int   uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char  uint8_t;
__extension__ typedef long long int64_t;
__extension__ typedef unsigned long long uint64_t;

/*
 * Some good old style C definitions for where we have imported code
 * from elsewhere.
 */
#define FALSE (1==2)
#define TRUE  (!FALSE)

#if defined(CONFIG_INFRASTRUCTURE)
#include <stm_registry.h>
#else
typedef void *stm_object_h;
#define STM_REGISTRY_MAX_TAG_SIZE 32
#define STM_REGISTRY_TYPES 0
#define STM_REGISTRY_INSTANCES 0
#define stm_registry_remove_object(a) (0)
#define stm_registry_add_object(a,b,c) (0)
#define stm_registry_add_instance(a,b,c,d) (0)
#define stm_registry_add_connection(a,b,c) (0)
#define stm_registry_remove_connection(a,b) (0)
#endif

#if defined(__cplusplus)
}
#endif

#if defined(__cplusplus)
#define NULL 0
#else
#define NULL ((void *)0)
#endif

#endif /* _STM_LINUX_PLATFORM_H */
