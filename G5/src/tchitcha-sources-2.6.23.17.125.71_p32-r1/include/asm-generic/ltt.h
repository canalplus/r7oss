#ifndef _ASM_GENERIC_LTT_H
#define _ASM_GENERIC_LTT_H

/*
 * linux/include/asm-generic/ltt.h
 *
 * Copyright (C) 2007 - Mathieu Desnoyers (mathieu.desnoyers@polymtl.ca)
 *
 * Generic definitions for LTT
 * Architectures without TSC
 */

#include <linux/param.h>	/* For HZ */
#include <asm/atomic.h>

#define LTT_GENERIC_CLOCK_SHIFT 13

u64 ltt_read_synthetic_tsc(void);

extern atomic_t lttng_generic_clock;

static inline u32 ltt_get_timestamp32_generic(void)
{
	return atomic_add_return(1, &lttng_generic_clock);
}

static inline u64 ltt_get_timestamp64_generic(void)
{
	return ltt_read_synthetic_tsc();
}

static inline void ltt_add_timestamp_generic(unsigned long ticks)
{
	int old_clock, new_clock;

	do {
		old_clock = atomic_read(&lttng_generic_clock);
		new_clock = (old_clock + (ticks << LTT_GENERIC_CLOCK_SHIFT))
			& (~((1 << LTT_GENERIC_CLOCK_SHIFT) - 1));
	} while (atomic_cmpxchg(&lttng_generic_clock, old_clock, new_clock)
			!= old_clock);
}

static inline unsigned int ltt_frequency_generic(void)
{
	return HZ << LTT_GENERIC_CLOCK_SHIFT;
}

static inline u32 ltt_freq_scale_generic(void)
{
	return 1;
}
#endif /* _ASM_GENERIC_LTT_H */
