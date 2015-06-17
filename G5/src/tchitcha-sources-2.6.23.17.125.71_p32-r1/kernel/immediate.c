/*
 * Copyright (C) 2007 Mathieu Desnoyers
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/immediate.h>
#include <linux/memory.h>

extern const struct __immediate __start___immediate[];
extern const struct __immediate __stop___immediate[];

/*
 * immediate_mutex nests inside module_mutex. immediate_mutex protects builtin
 * immediates and module immediates.
 */
static DEFINE_MUTEX(immediate_mutex);

/**
 * immediate_update_range - Update immediate values in a range
 * @begin: pointer to the beginning of the range
 * @end: pointer to the end of the range
 *
 * Sets a range of immediates to a enabled state : set the enable bit.
 */
void immediate_update_range(const struct __immediate *begin,
		const struct __immediate *end)
{
	const struct __immediate *iter;
	int ret;

	for (iter = begin; iter < end; iter++) {
		mutex_lock(&immediate_mutex);
		kernel_text_lock();
		ret = arch_immediate_update(iter);
		kernel_text_unlock();
		if (ret)
			printk(KERN_WARNING "Invalid immediate value. "
					    "Variable at %p, "
					    "instruction at %p, size %lu\n",
					    (void *)iter->immediate,
					    (void *)iter->var, iter->size);
		mutex_unlock(&immediate_mutex);
	}
}
EXPORT_SYMBOL_GPL(immediate_update_range);

/**
 * immediate_update - update all immediate values in the kernel
 * @lock: should a module_mutex be taken ?
 *
 * Iterate on the kernel core and modules to update the immediate values.
 */
void core_immediate_update(void)
{
	/* Core kernel immediates */
	immediate_update_range(__start___immediate, __stop___immediate);
}
EXPORT_SYMBOL_GPL(core_immediate_update);

static void __init immediate_update_early_range(const struct __immediate *begin,
		const struct __immediate *end)
{
	const struct __immediate *iter;

	for (iter = begin; iter < end; iter++)
		arch_immediate_update_early(iter);
}

/**
 * immediate_update_early - Update immediate values at boot time
 *
 * Update the immediate values to the state of the variables they refer to. It
 * is done before SMP is active, at the very beginning of start_kernel().
 */
void __init immediate_update_early(void)
{
	immediate_update_early_range(__start___immediate, __stop___immediate);
}
