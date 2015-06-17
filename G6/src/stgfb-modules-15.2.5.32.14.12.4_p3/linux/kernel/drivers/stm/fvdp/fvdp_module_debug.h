/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/fvdp/fvdp_module_debug.h
 * Copyright (c) 2000-2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef FVDP_DEBUG_H
#define FVDP_DEBUG_H

#include <linux/kernel.h>

#define THIS_FILE ((strrchr(__FILE__, '/') ?: __FILE__ - 1) + 1)

#define stm_fvdp_printw(format, args...) \
    printk (KERN_WARNING "%s:%d: " format, THIS_FILE, __LINE__, ##args)
#define stm_fvdp_printe(format, args...) \
    printk (KERN_ERR "%s:%d: " format, THIS_FILE, __LINE__, ##args)
#define stm_fvdp_printi(format, args...) \
    printk (KERN_INFO "%s:%d: " format, THIS_FILE, __LINE__, ##args)

#ifdef DEBUG
#define stm_fvdp_printd(format, args...) \
    printk (KERN_INFO "%s:%d: " format, THIS_FILE, __LINE__, ##args)
#else
#define stm_fvdp_printd(format, args...) do { } while(0)
#endif /* DEBUG */


#endif /* FVDP_DEBUG_H */

