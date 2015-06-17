#ifndef _LINUX_IMMEDIATE_H
#define _LINUX_IMMEDIATE_H

/*
 * Immediate values, can be updated at runtime and save cache lines.
 *
 * (C) Copyright 2007 Mathieu Desnoyers <mathieu.desnoyers@polymtl.ca>
 *
 * This file is released under the GPLv2.
 * See the file COPYING for more details.
 */

#ifdef CONFIG_IMMEDIATE

#include <asm/immediate.h>

/**
 * immediate_set - set immediate variable (with locking)
 * @name: immediate value name
 * @i: required value
 *
 * Sets the value of @name, taking the module_mutex if required by
 * the architecture.
 */
#define immediate_set(name, i)						\
	do {								\
		name##__immediate = (i);				\
		core_immediate_update();				\
		module_immediate_update();				\
	} while (0)


/**
 * _immediate_set - set immediate variable (without locking)
 * @name: immediate value name
 * @i: required value
 *
 * Sets the value of @name. Must be called with module_mutex held.
 */
#define _immediate_set(name, i)						\
	do {								\
		name##__immediate = (i);				\
		core_immediate_update();				\
		_module_immediate_update();				\
	} while (0)

/**
 * immediate_set_early - set immediate variable at early boot
 * @name: immediate value name
 * @i: required value
 *
 * Sets the value of @name. Should be used for updates at early boot, when only
 * one CPU is active and interrupts are disabled.
 */
#define immediate_set_early(name, i)					\
	do {								\
		name##__immediate = (i);				\
		immediate_update_early();				\
	} while (0)

/*
 * Internal update functions.
 */
extern void core_immediate_update(void);
extern void immediate_update_range(const struct __immediate *begin,
	const struct __immediate *end);
extern void immediate_update_early(void);

#else

/*
 * Generic immediate values: a simple, standard, memory load.
 */

/**
 * immediate_read - read immediate variable
 * @name: immediate value name
 *
 * Reads the value of @name.
 */
#define immediate_read(name)		_immediate_read(name)

/**
 * immediate_set - set immediate variable (with locking)
 * @name: immediate value name
 * @i: required value
 *
 * Sets the value of @name, taking the module_mutex if required by
 * the architecture.
 */
#define immediate_set(name, i)		(name##__immediate = (i))

/**
 * _immediate_set - set immediate variable (without locking)
 * @name: immediate value name
 * @i: required value
 *
 * Sets the value of @name. Must be called with module_mutex held.
 */
#define _immediate_set(name, i)		immediate_set(name, i)

/**
 * immediate_set_early - set immediate variable at early boot
 * @name: immediate value name
 * @i: required value
 *
 * Sets the value of @name. Should be used for early boot updates.
 */
#define immediate_set_early(name, i)	immediate_set(name, i)

/*
 * Internal update functions.
 */
static inline void immediate_update_early(void)
{ }
#endif

#define DECLARE_IMMEDIATE(type, name) extern __typeof__(type) name##__immediate
#define DEFINE_IMMEDIATE(type, name)  __typeof__(type) name##__immediate

#define EXPORT_IMMEDIATE_SYMBOL(name) EXPORT_SYMBOL(name##__immediate)
#define EXPORT_IMMEDIATE_SYMBOL_GPL(name) EXPORT_SYMBOL_GPL(name##__immediate)

/**
 * _immediate_read - Read immediate value with standard memory load.
 * @name: immediate value name
 *
 * Force a data read of the immediate value instead of the immediate value
 * based mechanism. Useful for __init and __exit section data read.
 */
#define _immediate_read(name)		(name##__immediate)

#endif
