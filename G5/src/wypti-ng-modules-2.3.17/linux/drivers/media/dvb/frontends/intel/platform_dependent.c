#include <linux/module.h>
#include <linux/rtc.h>
#include <linux/kdev_t.h>
#include <linux/idr.h>

#include "platform_dependent.h"
#include "mxl5007-ce6353.h"

TICK_T semosal_tick_now(void)
{
	struct timespec tm;
	TICK_T ret=0;

	tm = current_kernel_time();

	ret = tm.tv_sec*1000+(tm.tv_nsec/1000000);

	return ret;
}

TICK_T semosal_tick_minus(TICK_T t1, TICK_T t2)
{
	return t1-t2;
}

u32 semosal_elapsed_msec(TICK_T begin, TICK_T end)
{
	return end-begin;
}

void semosal_wait_msec(u32 msec)
{
	msleep(msec);
}

int semosal_trace(u32 id, u32 param)
{
	return 0;
}
