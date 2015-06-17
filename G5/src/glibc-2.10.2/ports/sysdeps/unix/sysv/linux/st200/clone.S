/* Copyright (C) 2003, 2009 Free Software Foundation, Inc.
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

/* clone() is even more special than fork() as it mucks with stacks
   and invokes a function in the right context after its all over.  */

/* 'cris' version allows (child_stack==NULL), which makes the kernel
   allocate stack. Implementing that version, requires the syscall
   returning saved registers to the new thread, since we have no
   stack to store them. */

#include <sysdep.h>
#define _ERRNO_H        1
#include <bits/errno.h>
#include <asm/unistd.h>

#define CLONE_VM      0x00000100
#define CLONE_THREAD  0x00010000

// int __clone(int (*fn)(void *), void *child_stack, int flags, void *arg,
//             pid_t *ptid, void *tls, pid_t *ctid)

	.text
	.proc
	.global C_SYMBOL_NAME(__clone)
C_LABEL(__clone)
	andl    $b0 = $r16, $r17   /* Test for NULL fn ptr or NULL stack ptr */
	and	$r17 = $r17, ~7    /* Ensure user stack is 8-byte aligned */
	;;
	brf	$b0, .Lerror_einval
	add	$r17 = $r17, -32	/* speculate */
	;;

#ifdef RESET_PID
	stw	28[$r17] = $r18	/* save flags ... */
	;;
#endif
	stw	24[$r17] = $r16	/* save user's function ... */
	;;
	// We have r18 = flags, r17 = usp, r20 = ptid, r22 = ctid, r21 = tls
        // For the clone syscall we want
	//  r16 = flags, r17 = usp, r18 = ptid, r19 = ctid, r20 = tls
	// so shuffle registers for the syscall
	stw	20[$r17] = $r19	/* ... and arguments in the new stack */
	mov	$r16 = $r18
	mov	$r18 = $r20
	mov	$r19 = $r22
	;;
	mov	$r20 = $r21
	DO_CALL(__NR_clone)
	;;
	br	$b0, .Lerror
	cmpeq	$b0 = $r16, 0
	;;
	br	$b0, .Lthread_start
	;;
	ret

/* New thread execution latency can be decreased by loading lr in the bundle
 * that follows syscall, no matter which is the sp value. But the code becomes
 * less readable. Do it after debug. */
/* Note: 16 byte scratch area already allocated by parent */
.Lthread_start:
#ifdef RESET_PID
	ldw     $r17 = 28[$r12] /* restore flags */
	;;
	and     $r16 = $r17, CLONE_THREAD
	and	$r17 = $r17, CLONE_VM
	;;
	cmpne   $b0  = $r16, 0  /* b0 = CLONE_THREAD */
	cmpne   $b1  = $r17, 0  /* b1 = CLONE_VM */
	;;
	brf     $b0, .Lrestorepid
	;;
.Ldonepid:
#endif
	ldw	$r63 = 24[$r12]	/* from the new thread's sp */
	;;
	ldw	$r16 = 20[$r12]
	;;
	call	$r63 = $r63		/* call the user's function */
	;;
1:
#ifdef PIC
	/* need to establish our GP so we can call _exit */
	call $r63 = 0f
	;;
0:	add  $r14 = $r63, @neggprel(0b)
	;;
#endif /* PIC */
	call	$r63 = _exit	/* r16 has the return value */
	;;
	mov	$r16 = 1
	goto	1b
	;;

#ifdef RESET_PID
.Lrestorepid:
	mov	$r16 = -1
	br	$b1, .Lgotpid
	;;
	DO_CALL(__NR_getpid)
	;;
.Lgotpid:
	stw 	PID[$r13] = $r16
	;;
	stw	TID[$r13] = $r16
	goto	.Ldonepid
	;;
#endif

.Lerror_einval:
	mov	$r16 = EINVAL
	;;

/* errno=r16; return -1; */
.Lerror:
	SYSCALL_ERROR_HANDLER
	mov $r16 = -1
	ret
	.endp

weak_alias (__clone, clone)
