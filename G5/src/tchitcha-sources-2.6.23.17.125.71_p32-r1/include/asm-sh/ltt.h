/*
 * Copyright (C) 2007, Giuseppe Cavallaro <peppe.cavallaro@st.com>
 *                     Mathieu Desnoyers <mathieu.desnoyers@polymtl.ca>
 *
 * SuperH definitions for tracing system
 */

#ifndef _ASM_SH_LTT_H
#define _ASM_SH_LTT_H

#include <linux/ltt-core.h>
#include <linux/timer.h>
#include <linux/clk.h>

#define LTT_HAS_TSC

u64 ltt_read_synthetic_tsc(void);

static inline u32 ltt_get_timestamp32(void)
{
	return get_cycles();
}

static inline u64 ltt_get_timestamp64(void)
{
	return ltt_read_synthetic_tsc();
}

static inline void ltt_add_timestamp(unsigned long ticks)
{ }

static inline unsigned int ltt_frequency(void)
{
	unsigned long rate;
	struct clk *tmu1_clk;

	tmu1_clk = clk_get(NULL, "tmu1_clk");
	rate = (clk_get_rate(tmu1_clk));

	return (unsigned int)(rate);
}

static inline u32 ltt_freq_scale(void)
{
	return 1;
}

#endif /* _ASM_SH_LTT_H */
