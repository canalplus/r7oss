/*
 *  linux/include/asm-sh/ide.h
 *
 *  Copyright (C) 1994-1996  Linus Torvalds & authors
 */

/*
 *  This file contains the i386 architecture specific IDE code.
 *  In future, SuperH code.
 */

#ifndef __ASM_SH_IDE_H
#define __ASM_SH_IDE_H

#ifdef __KERNEL__


#define ide_default_io_ctl(base)	(0)

#include <asm-generic/ide_iops.h>

/* IDE code reads and writes directly to user space buffers via
 * kernel logical addresses, with no explicit cache synchronization.
 * Insert explicit cache flushes as required.
 * This uses the same technique as the Sparc64.
 */
#if defined(CONFIG_CPU_SH4) && !defined(CONFIG_SH_CACHE_DISABLE)

#ifdef insl
#undef insl
#endif
#ifdef outsl
#undef outsl
#endif
#ifdef insw
#undef insw
#endif
#ifdef outsw
#undef outsw
#endif

#define wrap_in(op)					\
static __inline__ void sh_ide_##op(unsigned long port,	\
				void *buffer,		\
				unsigned long count,	\
				unsigned long nbytes)	\
{							\
	__##op(port, buffer, count);			\
	dma_cache_wback(buffer, nbytes);		\
}

wrap_in(insw)
wrap_in(insl)
#define insw(port, buffer, count) sh_ide_insw((port), (buffer), (count), (count)<<1)
#define insl(port, buffer, count) sh_ide_insl((port), (buffer), (count), (count)<<2)
#undef wrap_in

#define wrap_out(op)					\
static __inline__ void sh_ide_##op(unsigned long port,	\
				void *buffer,		\
				unsigned long count,	\
				unsigned long nbytes)	\
{							\
	__##op(port, buffer, count);			\
	/* dma_cache_wback(buffer, nbytes); */		\
}

wrap_out(outsw)
wrap_out(outsl)
#define outsw(port, buffer, count) sh_ide_outsw((port), (buffer), (count), (count)<<1)
#define outsl(port, buffer, count) sh_ide_outsl((port), (buffer), (count), (count)<<2)
#undef wrap_out

#endif /* CONFIG_CPU_SH4 && !CONFIG_SH_CACHE_DISABLE */

#endif /* __KERNEL__ */

#endif /* __ASM_SH_IDE_H */
