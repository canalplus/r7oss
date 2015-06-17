/*
 * linux/arch/sh/kernel/io.c
 *
 * Copyright (C) 2000  Stuart Menefy
 * Copyright (C) 2005  Paul Mundt
 *
 * Also definitions of machine independent IO functions.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <linux/module.h>
#include <asm/machvec.h>
#include <asm/io.h>

/*
 * Copy data from IO memory space to "real" memory space.
 */
void memcpy_fromio(void *to, volatile void __iomem *from, unsigned long count)
{
	/*
	 * Would it be worthwhile doing byte and long transfers first
	 * to try and get aligned?
	 */
#ifdef CONFIG_CPU_SH4
	if ( (count >= 0x20) &&
	     (((u32)to & 0x1f) == 0) && (((u32)from & 0x3) == 0) ) {
		int tmp2, tmp3, tmp4, tmp5, tmp6;

		__asm__ __volatile__(
			"1:			\n\t"
			"mov.l	@%7+, r0	\n\t"
			"mov.l	@%7+, %2	\n\t"
			"movca.l r0, @%0	\n\t"
			"mov.l	@%7+, %3	\n\t"
			"mov.l	@%7+, %4	\n\t"
			"mov.l	@%7+, %5	\n\t"
			"mov.l	@%7+, %6	\n\t"
			"mov.l	@%7+, r7	\n\t"
			"mov.l	@%7+, r0	\n\t"
			"mov.l	%2, @(0x04,%0)	\n\t"
			"mov	#0x20, %2	\n\t"
			"mov.l	%3, @(0x08,%0)	\n\t"
			"sub	%2, %1		\n\t"
			"mov.l	%4, @(0x0c,%0)	\n\t"
			"cmp/hi	%1, %2		! T if 32 > count	\n\t"
			"mov.l	%5, @(0x10,%0)	\n\t"
			"mov.l	%6, @(0x14,%0)	\n\t"
			"mov.l	r7, @(0x18,%0)	\n\t"
			"mov.l	r0, @(0x1c,%0)	\n\t"
			"bf.s	1b		\n\t"
			" add	#0x20, %0	\n\t"
			: "=&r" (to), "=&r" (count),
			  "=&r" (tmp2), "=&r" (tmp3), "=&r" (tmp4),
			  "=&r" (tmp5), "=&r" (tmp6), "=&r" (from)
			: "7"(from), "0" (to), "1" (count)
			: "r0", "r7", "t", "memory");
	}
#endif

	if ((((u32)to | (u32)from) & 0x3) == 0) {
		for ( ; count > 3; count -= 4) {
			*(u32*)to = *(volatile u32*)from;
			to += 4;
			from += 4;
		}
	}

	for ( ; count > 0; count--) {
		*(u8*)to = *(volatile u8*)from;
		to++;
		from++;
	}

	mb();
}
EXPORT_SYMBOL(memcpy_fromio);

/*
 * Copy data from "real" memory space to IO memory space.
 */
void memcpy_toio(volatile void __iomem *to, const void *from, unsigned long count)
{
	if ((((u32)to | (u32)from) & 0x3) == 0) {
		for ( ; count > 3; count -= 4) {
			*(volatile u32*)to = *(u32*)from;
			to += 4;
			from += 4;
		}
	}

	for ( ; count > 0; count--) {
		*(volatile u8*)to = *(u8*)from;
		to++;
		from++;
	}

	mb();
}
EXPORT_SYMBOL(memcpy_toio);

/*
 * "memset" on IO memory space.
 * This needs to be optimized.
 */
void memset_io(volatile void __iomem *dst, int c, unsigned long count)
{
        while (count) {
                count--;
                writeb(c, (void __iomem *)dst);
                dst++;
        }
}
EXPORT_SYMBOL(memset_io);

void __raw_readsb(const void __iomem *addrp, void *datap, int len)
{
	u8 *data;

	for (data = datap; len != 0; len--)
		*data++ = ctrl_inb(addrp);

}
EXPORT_SYMBOL(__raw_readsb);

void __raw_readsw(const void __iomem *addrp, void *datap, int len)
{
	u16 *data;

	for (data = datap; len != 0; len--)
		*data++ = ctrl_inw((unsigned long) addrp);

}
EXPORT_SYMBOL(__raw_readsw);

void __raw_readsl(const void __iomem *addrp, void *datap, int len)
{
	u32 *data;
	unsigned long addr = (unsigned long)addrp;

	for (data = datap; (len != 0) && (((u32)data & 0x1f) != 0); len--)
		*data++ = ctrl_inl(addr);

	if (likely(len >= (0x20 >> 2))) {
		int tmp2, tmp3, tmp4, tmp5, tmp6;

		__asm__ __volatile__(
			"1:			\n\t"
			"mov.l	@%7, r0		\n\t"
			"mov.l	@%7, %2		\n\t"
#ifdef CONFIG_CPU_SH4
			"movca.l r0, @%0	\n\t"
#else
			"mov.l	r0, @%0		\n\t"
#endif
			"mov.l	@%7, %3		\n\t"
			"mov.l	@%7, %4		\n\t"
			"mov.l	@%7, %5		\n\t"
			"mov.l	@%7, %6		\n\t"
			"mov.l	@%7, r7		\n\t"
			"mov.l	@%7, r0		\n\t"
			"mov.l	%2, @(0x04,%0)	\n\t"
			"mov	#0x20>>2, %2	\n\t"
			"mov.l	%3, @(0x08,%0)	\n\t"
			"sub	%2, %1		\n\t"
			"mov.l	%4, @(0x0c,%0)	\n\t"
			"cmp/hi	%1, %2		! T if 32 > len	\n\t"
			"mov.l	%5, @(0x10,%0)	\n\t"
			"mov.l	%6, @(0x14,%0)	\n\t"
			"mov.l	r7, @(0x18,%0)	\n\t"
			"mov.l	r0, @(0x1c,%0)	\n\t"
			"bf.s	1b		\n\t"
			" add	#0x20, %0	\n\t"
			: "=&r" (data), "=&r" (len),
			  "=&r" (tmp2), "=&r" (tmp3), "=&r" (tmp4),
			  "=&r" (tmp5), "=&r" (tmp6)
			: "r"(addr), "0" (data), "1" (len)
			: "r0", "r7", "t", "memory");
	}

	for (; len != 0; len--)
		*data++ = ctrl_inl(addr);
}
EXPORT_SYMBOL(__raw_readsl);


void __raw_writesb(void __iomem *addrp, const void *datap, int len)
{
	u8 *data;

	for (data = datap; len != 0; len--)
		ctrl_outb(*data++, addrp);

}
EXPORT_SYMBOL(__raw_writesb);

void __raw_writesw(void __iomem *addrp, const void *datap, int len)
{
	u16 *data;

	for (data = (u16 *) datap; len != 0; len--)
		ctrl_outw(*data++, (unsigned long) addrp);

}
EXPORT_SYMBOL(__raw_writesw);

void __raw_writesl(void __iomem *addrp, const void *data, int len)
{
	unsigned long addr = (unsigned long)addrp;

	if (likely(len != 0)) {
		int tmp1;

		__asm__ __volatile__ (
			"1:			\n\t"
			"mov.l	@%0+, %1	\n\t"
			"dt	%3		\n\t"
#ifdef CONFIG_CPU_ST40_300
			"mov.l	%1, @%4		\n\t"
			"bf	1b		\n\t"
			/*
			 * Note we cannot put the mov.l into the delay slot
			 * here, because of a bug in the SH4-300 (GNBvd67168).
			 */
#else
			"bf.s	1b		\n\t"
			" mov.l %1, @%4         \n\t"
#endif
			: "=&r" (data), "=&r" (tmp1)
			: "0" (data), "r" (len), "r"(addr)
			: "t", "memory");
	}
}
EXPORT_SYMBOL(__raw_writesl);

#ifndef CONFIG_GENERIC_IOMAP

void __iomem *ioport_map(unsigned long port, unsigned int nr)
{
	return sh_mv.mv_ioport_map(port, nr);
}
EXPORT_SYMBOL(ioport_map);

void ioport_unmap(void __iomem *addr)
{
	sh_mv.mv_ioport_unmap(addr);
}
EXPORT_SYMBOL(ioport_unmap);

#endif

