/* longjmp for ST200.
   Copyright (C) 2003 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the GNU C Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#include <sysdep.h>
#define _SETJMP_H
#include <bits/setjmp.h>

/* __longjmp(jmpbuf, val) */

ENTRY(__longjmp)
	ldw	$r63 = _JB_LINK[$r16]
	;;
#ifdef PTR_DEMANGLE
	PTR_DEMANGLE ($r63, $r1)
	/* Do not restore mangled SP value directly to r12,
	   it may have lowest bit set, and inadvertently
	   activate rollback mechanism in kernel interrupt handler.
	*/
	ldw	$r2 = _JB_STACK_PTR[$r16]
	;;
	PTR_DEMANGLE2 ($r2, $r1)
	;;
	mov     $r12 = $r2
#else
	ldw	$r12 = _JB_STACK_PTR[$r16]
	;;
#endif
	ldw	$r1 = _JB_PRE(0)[$r16]
	;;
	ldw	$r2 = _JB_PRE(1)[$r16]
	;;
	ldw	$r3 = _JB_PRE(2)[$r16]
	;;
	ldw	$r4 = _JB_PRE(3)[$r16]
	;;
	ldw	$r5 = _JB_PRE(4)[$r16]
	;;
	ldw	$r6 = _JB_PRE(5)[$r16]
	;;
	ldw	$r7 = _JB_PRE(6)[$r16]
	;;
	ldw	$r13 = _JB_PRE(7)[$r16]
	;;
	ldw	$r14 = _JB_PRE(8)[$r16]
	;;
	mov	$r16 = $r17
	goto	$r63
	;;
END(__longjmp)


