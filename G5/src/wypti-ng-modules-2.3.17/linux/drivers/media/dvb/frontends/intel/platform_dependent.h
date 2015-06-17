#ifndef _PLATFORM_DEPENDENT_H
#define _PLATFORM_DEPENDENT_H

#include "mxl5007-ce6353.h"
#include "semstbcp.h"

TICK_T semosal_tick_now(void);
TICK_T semosal_tick_minus(TICK_T t1, TICK_T t2);
u32 semosal_elapsed_msec(TICK_T begin, TICK_T end);
void semosal_wait_msec(u32 msec);
int semosal_trace(u32 id, u32 param);
#endif
