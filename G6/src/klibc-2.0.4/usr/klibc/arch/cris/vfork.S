/*
 * arch/cris/vfork.S
 *
 * On cris, r9 contains the syscall number (set by generated stub);
 * r10..r13 contain arguments 0-3 per the standard calling convention.
 * The return address is in $srp; so we just need to avoid the stack
 * usage of the normal syscall stubs.
 */

#include <asm/unistd.h>

	.section ".text","ax"
	.balign	4
	.globl	vfork
	.type	vfork,@function
vfork:
	move.d	__NR_vfork, $r9
	break	13

	cmps.w	-4096,$r10
	blo	1f
	neg.d	$r10,$r11
	move.d	$r11,[errno]
	moveq	-1,$r10
1:
	ret
	nop

	.size	vfork,.-vfork
