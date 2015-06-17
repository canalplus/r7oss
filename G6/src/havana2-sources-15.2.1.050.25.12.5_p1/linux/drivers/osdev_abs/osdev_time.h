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

#ifndef H_OSDEV_TIME
#define H_OSDEV_TIME

#ifdef __cplusplus
#error "included from player cpp code"
#endif

#include <linux/delay.h>
#include <linux/hrtimer.h>
#include <linux/ktime.h>
#include <linux/ptrace.h>

static inline ktime_t OSDEV_GetKtime(void)
{
	// was return ktime_get(); => but now use CLOCK_MONOTONIC_RAW : cf bz47980
	struct timespec ts;
	getrawmonotonic(&ts);
	return timespec_to_ktime(ts);
}

static inline unsigned int OSDEV_GetTimeInMilliSeconds(void)
{
	return ktime_to_ms(OSDEV_GetKtime());
}

static inline unsigned long long OSDEV_GetTimeInMicroSeconds(void)
{
	return ktime_to_us(OSDEV_GetKtime());
}

static inline void OSDEV_SleepMilliSeconds(unsigned int Value)
{
	sigset_t allset, oldset;

	/* Block all signals during the sleep (i.e. work correctly when called via ^C) */
	sigfillset(&allset);
	sigprocmask(SIG_BLOCK, &allset, &oldset);

	/* Sleep */
	if (Value >= 20) {
		msleep_interruptible(Value);
	} else {
		usleep_range(Value * 900, Value * 1100);
	}

	/* Restore the original signal mask */
	sigprocmask(SIG_SETMASK, &oldset, NULL);
}

#endif // H_OSDEV_TIME
