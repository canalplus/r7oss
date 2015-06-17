/*
 * arch/sh/syscall.S
 *
 * On sh, r3 contains the syscall number (set by generated stub);
 * r4..r7 contain arguments 0-3 per the standard calling convention,
 * and arguments 4-5 are passed in r0 and r1.
 *
 * The return value is in r3 rather than standard r0.
 */

	.section ".text.syscall","ax"
	.align	2
	.globl	__syscall_common
	.type	__syscall_common,@function
__syscall_common:
	mov.l	@(0,sp),r0
	mov.l	@(4,sp),r1
	trapa	#0x15
	mov	#-125,r1
	cmp/hi	r1,r0
	bf.s	3f
	neg	r0,r2
	mov.l	2f,r5
	mov.l	r2,@r5
	rts
	mov	#-1,r0

3:
	rts
	nop

	.align 2
1:	.long	-4096		/* Errno limit */
2:	.long	errno

	.size	__syscall_common,.-__syscall_common
