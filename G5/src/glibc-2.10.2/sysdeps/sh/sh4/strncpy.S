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

/*
	char *strncpy(char *dest, const char *src, size_t n);
 */

#include <sysdep.h>

ENTRY(strncpy)
	mov	#0,r0
	bra	2f
	 mov	r4,r2
1:
	mov.b	r1,@(r0,r2)
	add	#1,r0
2:
	cmp/hs	r6,r0
	bt	5f
	mov.b	@(r0,r5),r1
	tst	r1,r1
	bf/s	1b
	 cmp/hs	r6,r0
	bra	4f
	 nop
3:
	mov.b	r1,@(r0,r2)
	add	#1,r0
	cmp/hs	r6,r0
4:
	bf/s	3b
	 mov	#0,r1
5:
	rts
	 mov     r2,r0
END(strncpy)
libc_hidden_builtin_def(strncpy)
