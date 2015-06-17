#ifndef _LINUX_LTT_H
#define _LINUX_LTT_H

/*
 * Generic LTT clock.
 *
 * Chooses between an architecture specific clock or an atomic logical clock.
 *
 * Copyright (C) 2007 Mathieu Desnoyers (mathieu.desnoyers@polymtl.ca)
 */

#ifdef CONFIG_LTT_TIMESTAMP
#ifdef CONFIG_ARCH_SUPPORTS_LTT_CLOCK
#include <asm/ltt.h>
#else
#include <asm-generic/ltt.h>

#define ltt_get_timestamp32	ltt_get_timestamp32_generic
#define ltt_get_timestamp64	ltt_get_timestamp64_generic
#define ltt_add_timestamp	ltt_add_timestamp_generic
#define ltt_frequency		ltt_frequency_generic
#define ltt_freq_scale		ltt_freq_scale_generic
#endif /* CONFIG_ARCH_SUPPORTS_LTT_CLOCK */
#else
#define ltt_add_timestamp(ticks)
#endif /* CONFIG_LTT_TIMESTAMP */
#endif /* _LINUX_LTT_H */
