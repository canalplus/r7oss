#include <linux/time.h>

/*
 * wywakeup_get_wakeup_time:
 *
 * Fills 'ts' with the currently set wake up time
 * Returns 0 if the wakeup by timer is enabled, -EAGAIN otherwise
 */
int wywakeup_get_wakeup_time(struct timespec *ts);
