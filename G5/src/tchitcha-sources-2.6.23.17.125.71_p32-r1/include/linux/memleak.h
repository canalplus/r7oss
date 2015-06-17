/*
 * include/linux/memleak.h
 *
 * Copyright (C) 2006 ARM Limited
 * Written by Catalin Marinas <catalin.marinas@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#ifndef __MEMLEAK_H
#define __MEMLEAK_H

#include <linux/stddef.h>

struct memleak_offset {
	unsigned long type_id;
	unsigned long member_type_id;
	unsigned long offset;
};

extern struct memleak_offset __memleak_offsets_start[];
extern struct memleak_offset __memleak_offsets_end[];

/* type id approximation */
#define ml_guess_typeid(size)	((unsigned long)(size))
#define ml_typeid(type)		ml_guess_typeid(sizeof(type))
#define ml_sizeof(typeid)	((size_t)(typeid))

#ifdef CONFIG_DEBUG_MEMLEAK

/* if offsetof(type, member) is not a constant known at compile time,
 * just use 0 instead since we cannot add it to the
 * .init.memleak_offsets section
 */
#define memleak_offsetof(type, member)				\
	(__builtin_constant_p(offsetof(type, member)) ?		\
	 offsetof(type, member) : 0)

#define DECLARE_MEMLEAK_OFFSET(name, type, member)		\
	static const struct memleak_offset			\
	__attribute__ ((__section__ (".init.memleak_offsets")))	\
	__attribute_used__ __memleak_offset__##name = {		\
		ml_typeid(type),				\
		ml_typeid(typeof(((type *)0)->member)),		\
		memleak_offsetof(type, member)			\
	}

extern void memleak_init(void);
extern void memleak_alloc(const void *ptr, size_t size, int ref_count);
extern void memleak_free(const void *ptr);
extern void memleak_padding(const void *ptr, unsigned long offset, size_t size);
extern void memleak_not_leak(const void *ptr);
extern void memleak_ignore(const void *ptr);
extern void memleak_scan_area(const void *ptr, unsigned long offset,
			      size_t length);
extern void memleak_insert_aliases(struct memleak_offset *ml_off_start,
				   struct memleak_offset *ml_off_end);

static inline void memleak_erase(void **ptr)
{
	*ptr = NULL;
}

#define memleak_container(type, member)	(\
	DECLARE_MEMLEAK_OFFSET(container_of, type, member);	\
)

extern void memleak_typeid_raw(const void *ptr, unsigned long type_id);
#define memleak_typeid(ptr, type) \
	memleak_typeid_raw(ptr, ml_typeid(type))

#else

#define DECLARE_MEMLEAK_OFFSET(name, type, member)

static inline void memleak_init(void)
{
}
static inline void memleak_alloc(const void *ptr, size_t size, int ref_count)
{
}
static inline void memleak_free(const void *ptr)
{
}
static inline void memleak_padding(const void *ptr, unsigned long offset,
				   size_t size)
{
}
static inline void memleak_not_leak(const void *ptr)
{
}
static inline void memleak_ignore(const void *ptr)
{
}
static inline void memleak_scan_area(const void *ptr, unsigned long offset,
				     size_t length)
{
}
static inline void memleak_insert_aliases(struct memleak_offset *ml_off_start,
					  struct memleak_offset *ml_off_end)
{
}
static inline void memleak_erase(void **ptr)
{
}

#define memleak_container(type, member)

static inline void memleak_typeid_raw(const void *ptr, unsigned long type_id)
{
}

#define memleak_typeid(ptr, type)

#endif /* CONFIG_DEBUG_MEMLEAK */

#endif /* __MEMLEAK_H */
