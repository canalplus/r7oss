
#ifndef __MSS_DEBUG_H
#define __MSS_DEBUG_H

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/list.h>
#include <asm/page.h>
#include <linux/spinlock.h>

#include "memutil.h"
#include "infra_os_wrapper.h"

/* Enable/disable debug-level macros.
 *
 * This really should be off by default if you
 * *really* need your message to hit the console
 * every time then that is what MSS_TRACE()
 * is for.
 */

#define MEM_SRC			0
#define MEM_SINK		0
#define MEM_UTILS		0



#define MSS_DEBUG_MSG(enable, fmt, args...)	   ((void) (enable && \
		(pr_info("%s: " fmt, __func__, ##args), 0)))

/* Output trace information off the critical path */
#define MSS_TRACE_MSG(fmt, args...)	    \
		(pr_notice("%s: " fmt, __func__, ##args))

/* Output errors, should never be output in 'normal' operation */
#define MSS_ERROR_MSG(fmt, args...)	   \
	pr_err("ERROR in %s: " fmt, __func__, ##args)

/*
#define MSS_ASSERT(x) do if(!(x)) \
		pr_err("%s: Assertion '%s' failed at %s:%d\n", \
			__func__, #x, __FILE__, __LINE__); while(0)
*/

#endif /* __MSS_DEBUG_H */
