/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/pixel_capture/debug.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef __CAPTURE_DEBUG_H__
#define __CAPTURE_DEBUG_H__

#include <linux/kernel.h>
#include <linux/bug.h>
#include <linux/slab.h>

/* C++ support */
#ifdef __cplusplus
extern "C" {
#endif

#define STM_PIX_CAP_DEBUG_DOMAIN(domain,str,desc) static int __attribute__((unused)) domain

#define THIS_FILE ((strrchr(__FILE__, '/') ?: __FILE__ - 1) + 1)

#ifndef STM_PIX_CAP_ASSERT
#  define STM_PIX_CAP_ASSERT(cond) BUG_ON(!(cond))
#endif

#define STM_PIX_CAP_ASSUME(cond) WARN_ON(!(cond))
#define stm_cap_printw(format, args...) \
    printk (KERN_WARNING "%s:%d: " format, THIS_FILE, __LINE__, ##args)
#define stm_cap_printe(format, args...) \
    printk (KERN_ERR "%s:%d: " format, THIS_FILE, __LINE__, ##args)
#define stm_cap_printi(format, args...) \
    printk (KERN_INFO "%s:%d: " format, THIS_FILE, __LINE__, ##args)
#ifdef DEBUG
#define stm_cap_printd(domain,format, args...) \
    printk (KERN_INFO "%s:%d: " format, THIS_FILE, __LINE__, ##args)
#else
#define stm_cap_printd(domain,format, args...) do { } while(0)
#endif /* DEBUG */

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* __CAPTURE_DEBUG_H__ */
