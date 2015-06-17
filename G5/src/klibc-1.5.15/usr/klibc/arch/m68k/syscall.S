/*
 * arch/m68k/syscall.S
 *
 * Common tail-handling code for system calls.
 *
 * The arguments are on the stack; the system call number in %d0.
 */

	.text
	.align	2
	.globl	__syscall_common
	.type	__syscall_common, @function
__syscall_common:
	movem.l %d2-%d6, -(%sp)	/* 5 registers saved */
	movem.l	24(%sp), %d1-%d6
	trap	#0
	cmpi.l	#-4095, %d0
	blt.l	1f
	neg.l	%d0
	move.l	%d0, (errno)
	moveq	#-1, %d0
1:
	movea.l	%d0, %a0	/* Redundant return */
	movem.l (%sp)+, %d2-%d6 /* Restore registers */
	rts

	.size	__syscall_common,.-__syscall_common
