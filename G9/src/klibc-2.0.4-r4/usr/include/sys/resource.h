/*
 * sys/resource.h
 */

#ifndef _SYS_RESOURCE_H
#define _SYS_RESOURCE_H

#include <klibc/extern.h>
#include <sys/types.h>		/* MUST be included before linux/resource.h */
#include <linux/resource.h>

__extern int getpriority(int, int);
__extern int setpriority(int, int, int);

__extern int getrusage(int, struct rusage *);

#endif				/* _SYS_RESOURCE_H */
