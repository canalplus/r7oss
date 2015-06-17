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
	char *strcpy(char *dest, const char *src);
 */

#include <sysdep.h>

ENTRY(strcpy)
	mov	r4,r2
1:
	mov.b	@r5+,r1
	tst	r1,r1
	mov.b	r1,@r2
	bf/s	1b
	 add	#1,r2

	rts
	 mov	r4,r0
END(strcpy)
libc_hidden_builtin_def (strcpy)
