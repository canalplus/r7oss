/* Copyright (C) 2009 STMicroelectronics Ltd.

   Author: Giuseppe Cavallaro <peppe.cavallaro@st.com>

   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#include <string.h>

/*
 * All this is so that bcopy.c can #include
 * this file after defining some things.
 */
#ifndef a1
#define a1  dest    /* First arg is DEST.  */
#define a1const
#define a2  src		/* Second arg is SRC.  */
#define a2const const
#undef memmove
#endif
#if !defined(RETURN) || !defined(rettype)
#define RETURN(s)   return (s)  /* Return DEST.  */
#define rettype     void *
#endif

static void fpu_opt_copy_fwd(void *dest, const void *src, size_t len);

rettype
memmove (a1, a2, len)
     a1const void *a1;
     a2const void *a2;
     size_t len;
{
	unsigned long int d = (long int)dest;
	unsigned long int s = (long int)src;
	unsigned long int res;

	if (d >= s)
		res = d - s;
	else
		res = s - d;
	/*
	 * 1) dest and src are not overlap  ==> memcpy (BWD/FDW)
	 * 2) dest and src are 100% overlap ==> memcpy (BWD/FDW)
	 * 3) left-to-right overlap ==>  Copy from the beginning to the end
	 * 4) right-to-left overlap ==>  Copy from the end to the beginning
	 */

	if (res == 0)		/* 100% overlap */
		memcpy(dest, src, len);	/* No overlap */
	else if (res >= len)
		memcpy(dest, src, len);
	else {
		if (d > s)	/* right-to-left overlap */
			memcpy(dest, src, len);	/* memcpy is BWD */
		else		/* cannot use SH4 memcpy for this case */
			fpu_opt_copy_fwd(dest, src, len);
	}
	RETURN (dest);
}
#ifndef memmove
libc_hidden_builtin_def (memmove)
#endif

#define FPSCR_SR	(1 << 20)
#define STORE_FPSCR(x)	__asm__ volatile("sts fpscr, %0" : "=r"(x))
#define LOAD_FPSCR(x)	__asm__ volatile("lds %0, fpscr" : : "r"(x))

static void fpu_opt_copy_fwd(void *dest, const void *src, size_t len)
{
	char *d = (char *)dest;
	char *s = (char *)src;

	if (len >= 64) {
		unsigned long fpscr;
		int *s1;
		int *d1;

		/* Align the dest to 4 byte boundary. */
		while ((unsigned)d & 0x7) {
			*d++ = *s++;
			len--;
		}

		s1 = (int *)s;
		d1 = (int *)d;

		/* check if s is well aligned to use FPU */
		if (!((unsigned)s1 & 0x7)) {

			/* Align the dest to cache-line boundary */
			while ((unsigned)d1 & 0x1c) {
				*d1++ = *s1++;
				len -= 4;
			}

			/* Use paired single precision load or store mode for
			 * 64-bit tranfering.*/
			STORE_FPSCR(fpscr);
			LOAD_FPSCR(FPSCR_SR);

			while (len >= 32) {
				__asm__ volatile ("fmov @%0+,dr0":"+r" (s1));
				__asm__ volatile ("fmov @%0+,dr2":"+r" (s1));
				__asm__ volatile ("fmov @%0+,dr4":"+r" (s1));
				__asm__ volatile ("fmov @%0+,dr6":"+r" (s1));
				__asm__
				    volatile ("fmov dr0,@%0"::"r"
					      (d1):"memory");
				d1 += 2;
				__asm__
				    volatile ("fmov dr2,@%0"::"r"
					      (d1):"memory");
				d1 += 2;
				__asm__
				    volatile ("fmov dr4,@%0"::"r"
					      (d1):"memory");
				d1 += 2;
				__asm__
				    volatile ("fmov dr6,@%0"::"r"
					      (d1):"memory");
				d1 += 2;
				len -= 32;
			}

			LOAD_FPSCR(fpscr);
		}
		s = (char *)s1;
		d = (char *)d1;
		/*TODO: other subcases could be covered here?!?*/
	}
	/* Go to per-byte copy */
	while (len > 0) {
		*d++ = *s++;
		len--;
	}
	return;
}
