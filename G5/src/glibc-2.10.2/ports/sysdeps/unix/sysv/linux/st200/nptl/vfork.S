/* Copyright (C) 2000, 2002, 2004 Free Software Foundation, Inc.
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
#define _SIGNAL_H
#include <bits/signum.h>
#include <tcb-offsets.h>

/* The following are defined in linux/sched.h, which unfortunately	*/
/* is not safe for inclusion in an assembly file.			*/
#define CLONE_VM        0x00000100      /* set if VM shared between processes */
#define CLONE_VFORK     0x00004000      /* set if the parent wants the child to wake it up on mm_release */

/* pid_t vfork(void); */
/* Implemented as __clone_syscall(CLONE_VFORK | CLONE_VM | SIGCHLD, 0)	*/

ENTRY(__vfork)
	/* Load the current cached pid value across the vfork.  */
	ldw 	$r8 = PID[$r13]
	;;
	/* If the cached value is initialized (nonzero), then write
	   back its negation, otherwise INT_MIN to indicate that the pid
	   value is uninitialized in the the child, and in the window
	   between here and the point at which we restore the value.  */
	sub 	$r9 = $r0,$r8
	cmpeq 	$b0 = $r8,$r0
	;;
	brf 	$b0, 1f
	;;
	mov 	$r9 = 0x80000000
	;;
1:
	/* Call syscall clone */
	stw	PID[$r13] = $r9
	;;
	mov 	$r16 = CLONE_VM+CLONE_VFORK+SIGCHLD
	mov	$r17 = $r0
	DO_CALL (SYS_ify(clone));
        ;;
	brf     $b0, 2f  /* There was an error... */
	;;
	SYSCALL_ERROR_HANDLER
        mov     $r16 = -1
	;;
	return	$r63
	;;
2:
	/* Check if we're in the child or parent */
	cmpeq	$b0 = $r16, $r0
	;;
	br	$b0, 4f
	;;
	/* If we're back in the parent, restore the saved pid.  */
	ldw 	$r8 = PID[$r13]
	;;
	sub	$r9 = $r0, $r8  /* re-invert the PID */
	and	$r10 = $r8, 0xeffffff
	;;
	cmpeq	$b0 = $r10, $r0
	;;
	brf	$b0, 3f
	;;
	mov	$r9 = $r10
	;;
3:
	stw     PID[$r13] = $r9
	;;
4:
	return  $r63
	;;
PSEUDO_END(__vfork)
libc_hidden_def (__vfork)
weak_alias (__vfork, vfork)
