/************************************************************************
Copyright (C) 2003-2014 STMicroelectronics. All Rights Reserved.

This file is part of the Streaming Engine.

Streaming Engine is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Streaming Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Streaming Engine; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
USA.

The Streaming Engine Library may alternatively be licensed under a
proprietary license from ST.
************************************************************************/

#ifndef H_OSDEV_SCHED
#define H_OSDEV_SCHED

#ifdef __cplusplus
#error "included from player cpp code"
#endif

#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/ptrace.h>
#include <linux/kthread.h>

#include "osdev_status.h"

// -----------------------------------------------------------------------------------------------
//
// The function types and macro's used for thread creation
//

typedef void (*OSDEV_ThreadFn_t)(void   *Parameter);
#define OSDEV_ThreadEntrypoint(fn)      void fn( void   *Parameter )

#define OSDEV_HIGHEST_PRIORITY          99
#define OSDEV_MID_PRIORITY              50
#define OSDEV_LOWEST_PRIORITY           0

typedef struct task_struct             *OSDEV_Thread_t;
typedef void                           *OSDEV_ThreadParam_t;
typedef struct {
	const char *name;
	int policy;
	int priority;
} OSDEV_ThreadDesc_t;

struct ThreadInfo_s {
	OSDEV_ThreadFn_t thread;
	void            *args;
	int              policy;
	int              priority;
};

static int  OSDEV_CreateThreadHelper(void *p)
{
	struct ThreadInfo_s *t = (struct ThreadInfo_s *)p;

	/* switch to real time scheduling (if requested) */
	if (0 != t->priority || SCHED_NORMAL != t->policy) {
		struct sched_param Param;
		Param.sched_priority = t->priority;
		if (0 != sched_setscheduler(current, t->policy, &Param)) {
			pr_err("Error: %s failed to set scheduling parameters to policy %d priority %d\n", __func__,
			       t->policy, t->priority);
		}
	}

	/* enter the thread */
	t->thread(t->args);

	/* clean up and exit */
	kfree(t);

	return 0;
}

static inline OSDEV_Status_t   OSDEV_CreateThread(OSDEV_Thread_t           *Thread,
                                                  OSDEV_ThreadFn_t          Entrypoint,
                                                  OSDEV_ThreadParam_t       Parameter,
                                                  const OSDEV_ThreadDesc_t *ThDesc)
{
	struct ThreadInfo_s *t;
	struct task_struct *Taskp;

	if (NULL == ThDesc) {
		return OSDEV_Error;
	}
	t = (struct ThreadInfo_s *)kmalloc(sizeof(struct ThreadInfo_s), GFP_KERNEL);

	t->thread    = Entrypoint;
	t->args      = Parameter;
	t->policy    = ThDesc->policy;
	t->priority  = ThDesc->priority;

	/* create the task and leave it suspended */
	Taskp = kthread_create(OSDEV_CreateThreadHelper, t, "%s", ThDesc->name);
	if (IS_ERR(Taskp)) {
		kfree(t);
		return OSDEV_Error;
	}
	Taskp->flags |= PF_NOFREEZE;

	/* do the wake up */
	wake_up_process(Taskp);
	*Thread = Taskp;

	return OSDEV_NoError;
}

static inline OSDEV_Status_t  OSDEV_SetPriority(const OSDEV_ThreadDesc_t *ThDesc)
{
	int Result;
	struct sched_param Param;
	int policy;

	if (NULL == ThDesc) {
		return OSDEV_Error;
	}

	policy = ThDesc->policy;
	Param.sched_priority = ThDesc->priority;

	Result = sched_setscheduler(current, policy, &Param);
	if (0 != Result) {
		pr_err("Error: %s failed to set scheduling parameters to priority %d policy %d\n", __func__,
		       policy, Param.sched_priority);
		return OSDEV_Error;
	}

	return OSDEV_NoError;
}

static inline char *OSDEV_ThreadName(void)
{
	return current->comm;
}

// -----------------------------------------------------------------------------------------------

// Check if any signals are waiting on the current process.
// This is important to check when blocking userspace contexts.
static inline int OSDEV_SignalPending(void)
{
	return signal_pending(current);
}

#endif // H_OSDEV_SCHED
