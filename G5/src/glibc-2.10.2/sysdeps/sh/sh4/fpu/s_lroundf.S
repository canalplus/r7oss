/* Round argument toward 0. SH4 version.
   According to ISO/IEC 9899:1999. This version doesn't handle range error.
   If arg is not finite or if the result cannot be represented into a long,
   return an unspecified value. No exception raised.

   Copyright (C) 1997, 2006 Free Software Foundation, Inc.
   Copyright (C) 2010 STMicroelectronics Ltd.
   Author: Christian Bruel <christian.bruel@st.com>

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

#include <sysdep.h>

ENTRY(__lroundf)
	mov	#0,r0
	sts	fpscr,r3
	lds	r0,fpscr
	flds	fr5,fpul
	mov.l	LOCAL(mask),r1
	sts	fpul,r2
	and	r2,r1
	mov.l	LOCAL(midway),r2
	or	r1,r2
	lds	r2,fpul
	fsts	fpul,fr2
	fadd	fr2,fr5
	ftrc	fr5,fpul
	sts	fpul,r0
	rts
	lds	r3,fpscr

	.align 2
LOCAL(mask):
	.long	0x80000000
LOCAL(midway):
	.long	1056964608

END(__lroundf)
weak_alias (__lroundf, lroundf)
