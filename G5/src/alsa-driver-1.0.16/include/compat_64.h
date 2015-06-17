#ifndef __ALSA_COMPAT_64_H
#define __ALSA_COMPAT_64_H

#include <linux/types.h>

/* FIXME: it is valid for x86_64, check other archs */

typedef s32             compat_time_t;

struct compat_timespec {
	compat_time_t   tv_sec;
	s32             tv_nsec;
};

struct compat_timeval {
	compat_time_t   tv_sec;
	s32             tv_usec;
};

/* compat_to() macro */
#ifndef A
#ifdef CONFIG_PPC64
#include <asm/ppc32.h>
#else
/* x86-64, sparc64 */
#define A(__x) ((void *)(unsigned long)(__x))
#endif
#endif
#define compat_ptr(x)	A(x)

#endif /* __ALSA_COMPAT_64_H */
