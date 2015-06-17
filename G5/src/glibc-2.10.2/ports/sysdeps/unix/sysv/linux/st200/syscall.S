/* Copyright (C) 2003 Free Software Foundation, Inc.
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
#include <sys/syscall.h>

	.text
	.proc
	.global C_SYMBOL_NAME(__syscall)
C_LABEL(__syscall)
	CALL_MCOUNT
/* The syscall number is in R16, should be in r15 */
/* This simply shuffles the args, and then invokes the syscall */
	mov $r15 = $r16
	mov $r16 = $r17
	mov $r17 = $r18
	mov $r18 = $r19
	;;
	mov $r19 = $r20
	mov $r20 = $r21
	mov $r21 = $r22
	mov $r22 = $r23
	;;
	syscall 0
	;;
	br	$b0, 1f
	;;
	ret
1:
	SYSCALL_ERROR_HANDLER
	mov $r16 = -1
	ret
	.endp

weak_alias (__syscall, syscall)
