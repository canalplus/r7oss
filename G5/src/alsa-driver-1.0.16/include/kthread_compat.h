#ifndef __KTHREAD_COMPAT_H
#define __KTHREAD_COMPAT_H

/* TODO: actually implement this ... */

#define kthread_create(fn, data, namefmt, ...) NULL
#define kthread_run(fn, data, namefmt, ...) NULL
#define kthread_stop(kt) 0
#define kthread_should_stop() 1

#endif
