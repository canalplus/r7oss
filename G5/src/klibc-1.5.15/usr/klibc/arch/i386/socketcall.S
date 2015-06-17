#
# socketcall.S
#
# Socketcalls use the following convention:
# %eax = __NR_socketcall
# %ebx = socketcall number
# %ecx = pointer to arguments (up to 6)
#

#include <asm/unistd.h>

#ifdef __i386__

	.text
	.align 4
	.globl __socketcall_common
	.type __socketcall_common, @function

__socketcall_common:
	xchgl	%ebx,(%esp)	# The stub passes the socketcall # on stack

#ifdef	_REGPARM
	pushl	16(%esp)	# Arg 6
	pushl	16(%esp)	# Arg 5
	pushl	16(%esp)	# Arg 4
	pushl	%ecx
	pushl	%edx
	pushl	%eax
	movl	%esp,%ecx
#else
	leal	8(%esp),%ecx	# Arguments already contiguous on-stack
#endif

	movl	$__NR_socketcall,%eax
	int	$0x80

#ifdef	_REGPARM
	addl	$6*4, %esp
#endif

	cmpl	$-4095,%eax	# Error return?

	popl	%ebx

	jb	1f

	negl	%eax
	movl	%eax,errno
	orl	$-1,%eax	# Return -1
1:
	ret

	.size __socketcall_common,.-__socketcall_common

#endif
