#
# arch/s390/setjmp.S
#
# setjmp/longjmp for the s390 architecture
#

	.text
	.align 4
	.globl setjmp
	.type setjmp, @function

#ifndef __s390x__

setjmp:
	stm	%r6,%r15,0(%r2)		# save all general registers
	std	%f4,40(%r2)		# save fp registers f4 and f6
	std	%f6,48(%r2)
	lhi	%r2,0			# return 0
	br	%r14

	.size setjmp,.-setjmp

	.text
	.align 4
	.globl longjmp
	.type longjmp, @function
longjmp:
	lr	%r1,%r2			# jmp_buf
	lr	%r2,%r3			# return value
	ld	%f6,48(%r1)		# restore all saved registers
	ld	%f4,40(%r1)
	lm	%r6,%r15,0(%r1)
	br	%r14			# return to restored address

	.size longjmp,.-longjmp

#else

setjmp:
	stmg	%r6,%r15,0(%r2)		# save all general registers
	std	%f1,80(%r2)		# save fp registers f4 and f6
	std	%f3,88(%r2)
	std	%f5,96(%r2)
	std	%f7,104(%r2)
	lghi	%r2,0			# return 0
	br	%r14

	.size setjmp,.-setjmp

	.text
	.align 4
	.globl longjmp
	.type longjmp, @function
longjmp:
	lgr	%r1,%r2			# jmp_buf
	lgr	%r2,%r3			# return value
	ld	%f7,104(%r1)		# restore all saved registers
	ld	%f5,96(%r1)
	ld	%f3,88(%r1)
	ld	%f1,80(%r1)
	lmg	%r6,%r15,0(%r1)
	br	%r14			# return to restored address

	.size longjmp,.-longjmp

#endif
