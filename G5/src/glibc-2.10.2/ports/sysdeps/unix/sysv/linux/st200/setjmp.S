/* setjmp for ST200.
   Copyright (C) 2003, 2009 Free Software Foundation, Inc.
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

#define _SETJMP_H
#include <sysdep.h>
#include <bits/setjmp.h>

ENTRY(__sigsetjmp)
.Lsigsetjmp_local:
#ifdef PTR_MANGLE
	mov	$r9 = $r12
	;;
	PTR_MANGLE ($r9, $r8)
	;;
	stw	_JB_STACK_PTR[$r16] = $r9
	mov     $r9 = $r63
	;;
	PTR_MANGLE2 ($r9, $r8)
	;;
	stw	_JB_LINK[$r16] = $r9
	;;
#else
	stw	_JB_STACK_PTR[$r16] = $r12
	;;
	stw	_JB_LINK[$r16] = $r63
	;;
#endif
	stw	_JB_PRE(0)[$r16] = $r1
	;;
	stw	_JB_PRE(1)[$r16] = $r2
	;;
	stw	_JB_PRE(2)[$r16] = $r3
	;;
	stw	_JB_PRE(3)[$r16] = $r4
	;;
	stw	_JB_PRE(4)[$r16] = $r5
	;;
	stw	_JB_PRE(5)[$r16] = $r6
	;;
	stw	_JB_PRE(6)[$r16] = $r7
	;;
	stw	_JB_PRE(7)[$r16] = $r13
	;;
	stw	_JB_PRE(8)[$r16] = $r14
	;;
#if defined NOT_IN_libc && defined IS_IN_rtld
	/* In ld.so we never save the signal mask.  */
	mov	$r16 = 0
	goto	$r63
#else
	goto	__sigjmp_save
#endif
	;;
	END(__sigsetjmp)

/* Put these traditional entry points in the same file so that we can
   elide much of the nonsense in trying to jmp to the real function.  */

ENTRY(_setjmp)
	mov	$r17 = 0
	goto	.Lsigsetjmp_local
	;;
END(_setjmp)
libc_hidden_def (_setjmp)

ENTRY(setjmp)
	mov	$r17 = 1
	goto	.Lsigsetjmp_local
	;;
END(setjmp)

weak_extern(_setjmp)
weak_extern(setjmp)
