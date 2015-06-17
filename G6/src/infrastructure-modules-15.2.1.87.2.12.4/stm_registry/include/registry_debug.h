/*****************************************************************************/
/* COPYRIGHT (C) 2011 STMicroelectronics - All Rights Reserved               */
/* ST makes no warranty express or implied including but not limited to,     */
/* any warranty of                                                           */
/*                                                                           */
/*   (i)  merchantability or fitness for a particular purpose and/or         */
/*   (ii) requirements, for a particular purpose in relation to the LICENSED */
/*        MATERIALS, which is provided “AS IS”, WITH ALL FAULTS. ST does not */
/*        represent or warrant that the LICENSED MATERIALS provided here     */
/*        under is free of infringement of any third party patents,          */
/*        copyrights,trade secrets or other intellectual property rights.    */
/*        ALL WARRANTIES, CONDITIONS OR OTHER TERMS IMPLIED BY LAW ARE       */
/*        EXCLUDED TO THE FULLEST EXTENT PERMITTED BY LAW                    */
/*                                                                           */
/*****************************************************************************/
/**
 @File   registry_debug.h
 @brief
*/

#ifndef REGISTRY_DEBUG_H_
#define REGISTRY_DEBUG_H_

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/list.h>
#include "infra_os_wrapper.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Enable/disable debug-level macros.
 *
 * This really should be off by default if you
 * *really* need your message to hit the console every time then that is what REGISTRY_TRACE()
 * is for.
 */

/* Comment in release version */
/*#define DEBUG_VER	1 */


#ifdef DEBUG_VER
#define ENABLE_REGISTRY_DEBUG      1
#else
#define ENABLE_REGISTRY_DEBUG      0
#endif

#define REG_STKPI		1
#define REG_RBTREE		1
#define REG_SYSFS		1
#define REG_DEBUGFS		1
#define REG_CORE		1


#define REGISTRY_DEBUG_MSG(fmt, args...)	 ((void) (ENABLE_REGISTRY_DEBUG && \
						 (pr_info("%s: " fmt, __FUNCTION__, ##args), 0)))

/* Output trace information off the critical path */
#define REGISTRY_TRACE_MSG(fmt, args...)         (pr_notice("%s: " fmt, __FUNCTION__, ##args))

/* Output errors, should never be output in 'normal' operation */
#define REGISTRY_ERROR_MSG(fmt, args...)         pr_crit("ERROR in %s %d: " fmt, __FUNCTION__, __LINE__, ##args)

#define REGISTRY_ASSERT(x) do { \
					if (!(x)) \
						pr_crit("%s: Assertion '%s' failed at %s:%d\n", \
						__FUNCTION__, #x, __FILE__, __LINE__); \
				} while (0)


#define RED   "\033[0;31m"
#define CYAN  "\033[1;36m"
#define GREEN "\033[4;32m"
#define BLUE  "\033[9;34m"
#define NONE   "\033[0m"
#define BROWN  "\033[0;33m"
#define MAGENTA  "\033[0;35m"

#ifdef __cplusplus
}
#endif

#endif /* __REGISTRY_DEBUG_H */


