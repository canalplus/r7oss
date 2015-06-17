/***********************************************************************
 *
 * File: os21/stglib/os21-platform.h
 * Copyright (c) 2000-2010 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef _STM_OS21_PLATFORM_H
#define _STM_OS21_PLATFORM_H

#if defined(__cplusplus)
extern "C" {
#endif

/*! \file os21-platform.h
 *  \brief BUG_ON OS21 specific macro definition for CoreDisplay
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <os21.h>

#define TRUE  OS21_TRUE
#define FALSE OS21_FALSE

/*
 * Specific OS21 BUG_ON macro definition for CoreDisplay
 */
#define STM_VIBE_BUG_ON(cond) \
   if (cond) \
     kernel_printf ("argh %s %d %s", __FILE__, __LINE__, #cond );
     /* should break execution ? */

/*
 * No use of SDK2 registry/infrastructure support on OS21
 */
typedef void *stm_object_h;
#define STM_REGISTRY_MAX_TAG_SIZE 32
#define STM_REGISTRY_TYPES 0
#define STM_REGISTRY_INSTANCES 0
#define stm_registry_remove_object(a) (0)
#define stm_registry_add_object(a,b,c) (0)
#define stm_registry_add_instance(a,b,c,d) (0)
#define stm_registry_add_connection(a,b,c) (0)
#define stm_registry_remove_connection(a,b) (0)
#define stm_registry_add_attribute(a,b,c,d,e) (0)

#if defined(__cplusplus)
}
#endif

#endif /* _STM_OS21_PLATFORM_H */
