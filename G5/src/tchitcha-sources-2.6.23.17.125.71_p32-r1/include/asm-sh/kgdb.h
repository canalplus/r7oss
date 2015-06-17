/*
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Based on original code by Glenn Engel, Jim Kingdon,
 * David Grothe <dave@gcom.com>, Tigran Aivazian, <tigran@sco.com> and
 * Amit S. Kale <akale@veritas.com>
 * 
 * Super-H port based on sh-stub.c (Ben Lee and Steve Chamberlain) by
 * Henry Bell <henry.bell@st.com>
 * 
 * Header file for low-level support for remote debug using GDB. 
 *
 */

#ifndef __KGDB_H
#define __KGDB_H

#include <asm/ptrace.h>


#include <asm-generic/kgdb.h>

/* Based on sh-gdb.c from gdb-6.1, Glenn
Engel at HP  Ben Lee and Steve Chamberlain */
#define NUMREGBYTES	112	/* 92 */
#define NUMCRITREGBYTES	(9 << 2)
#define BUFMAX		400

#ifndef __ASSEMBLY__
struct kgdb_regs {
	unsigned long regs[16];
	unsigned long pc;
	unsigned long pr;
	unsigned long gbr;
	unsigned long vbr;
	unsigned long mach;
	unsigned long macl;
	unsigned long sr;
};

#define BREAKPOINT()		asm("trapa #0x3c");
#define BREAK_INSTR_SIZE	2
#define CACHE_FLUSH_IS_SAFE	1

/* KGDB should be able to flush all kernel text space */
#if defined(CONFIG_CPU_SH4)
#define kgdb_flush_icache_range(start, end) \
{									\
	extern void __flush_purge_region(void *, int);			\
	extern void flush_icache_range(unsigned long , unsigned long);	\
	__flush_purge_region((void*)(start), (int)(end) - (int)(start));\
	flush_icache_range((start), (end));				\
}
#else
#define kgdb_flush_icache_range(start, end)	do { } while (0)
#endif

#endif				/* !__ASSEMBLY__ */
#endif
