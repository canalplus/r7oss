#
# usr/klibc/arch/m68k/vfork.S
#
# vfork is nasty - there must be nothing at all on the stack above
# the stack frame of the enclosing function.
#

#include <asm/unistd.h>

	.text
	.align	2
	.globl	vfork
	.type	vfork, @function
vfork:
	move.l	(%sp)+, %d1		/* Return address */
	move.l	# __NR_vfork, %d0
	trap	#0
	move.l	%d1, -(%sp)
	cmpi.l	#-4095, %d0
	blt.l	1f
	neg.l	%d0
	move.l	%d0, (errno)
	moveq	#-1, %d0
1:
	movea.l	%d0, %a0
	rts

	.size	vfork, .-vfork
