/*
 * LTT core in-kernel infrastructure.
 *
 * Copyright 2006 - Mathieu Desnoyers mathieu.desnoyers@polymtl.ca
 *
 * Distributed under the GPL license
 */

#include <linux/ltt-core.h>
#include <linux/module.h>

/* Traces structures */
struct ltt_traces ltt_traces = {
	.head = LIST_HEAD_INIT(ltt_traces.head),
};
EXPORT_SYMBOL(ltt_traces);

/* Traces list writer locking */
static DEFINE_MUTEX(ltt_traces_mutex);

void ltt_lock_traces(void)
{
	mutex_lock(&ltt_traces_mutex);
}
EXPORT_SYMBOL_GPL(ltt_lock_traces);

void ltt_unlock_traces(void)
{
	mutex_unlock(&ltt_traces_mutex);
}
EXPORT_SYMBOL_GPL(ltt_unlock_traces);

unsigned int ltt_nesting[NR_CPUS];
EXPORT_SYMBOL(ltt_nesting);

int ltt_run_filter_default(void *trace, uint16_t eID)
{
	return 1;
}

/* This function pointer is protected by a trace activation check */
ltt_run_filter_functor ltt_run_filter = ltt_run_filter_default;
EXPORT_SYMBOL_GPL(ltt_run_filter);

void ltt_filter_register(ltt_run_filter_functor func)
{
	ltt_run_filter = func;
}
EXPORT_SYMBOL_GPL(ltt_filter_register);

void ltt_filter_unregister(void)
{
	ltt_run_filter = ltt_run_filter_default;
}
EXPORT_SYMBOL_GPL(ltt_filter_unregister);
