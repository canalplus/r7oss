/*
 * "memcpy" implementation of SuperH
 * SH4 specific version
 *
 * Copyright (C) 1999  Niibe Yutaka
 * Copyright (c) 2002  STMicroelectronics Ltd
 *   Modified from memcpy.S and micro-optimised for SH4
 *   Stuart Menefy (stuart.menefy@st.com)
 *
 * Copyright (c) 2009-2011  STMicroelectronics Ltd
 *   Optimised using prefetching and 64bit data transfer via FPU
 *   Author: Giuseppe Cavallaro <peppe.cavallaro@st.com>
 *   Improved: Christian Bruel <christian.bruel@st.com>
 */

/*
 * void *memcpy(void *dst, const void *src, size_t n);
 *
 * It is assumed that there is no overlap between src and dst.
 * If there is an overlap, then the results are undefined.
 */

#include <sysdep.h>
#include <endian.h>

! Number of prefetch buffers. 2 for SH4, 6 for SH4-300.
#ifndef PREFETCH
#ifdef 	__SH4_300__
#define PREFETCH 6
#else
#define PREFETCH 2
#endif
#endif /* PREFETCH */

#ifdef __SH_FPU_DOUBLE__

! Use paired single precision load/store mode for 64-bit transfers.
! In entry FPSCR.SZ=0, FPSCR.PR=1
! that FPSCR.SZ=1, FPSCR.PR=1 is well defined only for SH4-300 cores.
!      FPSCR.SZ=1, FPSCR.PR=0 is well defined for SH4 cores.
!

#ifdef 	__SH4_300__

#define SAVE_AND_SET_FPSCR
#define RESTORE_FPSCR

#else /* __SH4_300__ */

.macro SAVE_AND_SET_FPSCR
	sts.l	fpscr, @-r15
	mov	#0x0, r0	! PR=0 SZ=0
	lds	r0, fpscr
.endm

.macro RESTORE_FPSCR
	lds.l	@r15+, fpscr
.endm

#endif /* ! __SH4_300__ */

#endif /* __SH_FPU_DOUBLE__ */

	!
	!	GHIJ KLMN OPQR -->  ...G HIJK LMNO PQR.
	!

	! Size is 16 or greater, and may have trailing bytes

	.balign	32
LOCAL(case1):
	! Read a long word and write a long word at once
	! At the start of each iteration, r7 contains last long load
	mov.l	@(r0,r5),r7	!  21 LS (2 cycles latency)
	add	#-4,r5		!  50 EX

	!
	! 6 cycles, 4 bytes per iteration
#ifdef __LITTLE_ENDIAN__
3:	mov.l	@(r0,r5),r1	!  21 LS (latency=2)	! NMLK
	mov	r7, r3		!   5 MT (latency=0)	! RQPO

	cmp/hi	r2,r0		!  57 MT
	shll16	r3		! 103 EX

	mov	r1,r6		!   5 MT (latency=0)
	shll8	r3		! 102 EX		! Oxxx

	shlr8	r6		! 106 EX		! xNML
	mov	r1, r7		!   5 MT (latency=0)

	or	r6,r3		!  82 EX		! ONML
	bt/s	3b		! 109 BR

	 mov.l	r3,@-r0		!  30 LS
#else /* ! __LITTLE_ENDIAN__ */
3:	mov.l	@(r0,r5),r1	!  21 LS (latency=2)	! KLMN
	mov	r7,r3		!   5 MT (latency=0)	! OPQR

	cmp/hi	r2,r0		!  57 MT
	shlr16	r3		! 107 EX

	shlr8	r3		! 106 EX		! xxxO
	mov	r1,r6		!   5 MT (latency=0)

	shll8	r6		! 102 EX		! LMNx
	mov	r1,r7		!   5 MT (latency=0)

	or	r6,r3		!  82 EX		! LMNO
	bt/s	3b		! 109 BR

	 mov.l	r3,@-r0		!  30 LS
#endif /* ! __LITTLE_ENDIAN__ */
	! Finally, copy a byte at once, if necessary

	add	#4,r5		!  50 EX
	cmp/eq	r4,r0		!  54 MT

	bt/s	9f		! 109 BR
	 add	#-6,r2		!  50 EX

8:	cmp/hi	r2,r0		!  57 MT
	mov.b	@(r0,r5),r1	!  20 LS (latency=2)

	bt/s	8b		! 109 BR

	 mov.b	r1,@-r0		!  29 LS

9:	rts
	 nop


	!
	!	GHIJ KLMN OPQR -->  .GHI JKLM NOPQ R...
	!

	! Size is 16 or greater, and may have trailing bytes

	.balign	32
LOCAL(case3):
	! Read a long word and write a long word at once
	! At the start of each iteration, r7 contains last long load
	mov.l	@(r0,r5),r7	! 21 LS (2 cycles latency)
	add	#-4,r5		! 50 EX

	!
	! 6 cycles, 4 bytes per iteration
#ifdef __LITTLE_ENDIAN__
3:	mov.l	@(r0,r5),r1	!  21 LS (latency=2)	! NMLK
	mov	r7, r3		!   5 MT (latency=0)	! RQPO

	cmp/hi	r2,r0		!  57 MT
	shll8	r3		! 102 EX		! QPOx

	mov	r1,r6		!   5 MT (latency=0)
	shlr16	r6		! 107 EX

	shlr8	r6		! 106 EX		! xxxN
	mov	r1, r7		!   5 MT (latency=0)

	or	r6,r3		!  82 EX		! QPON
	bt/s	3b		! 109 BR

	 mov.l	r3,@-r0		!  30 LS
#else /* ! __LITTLE_ENDIAN__ */
3:	mov	r7,r3		! OPQR
	shlr8	r3		! xOPQ
	mov.l	@(r0,r5),r7	! KLMN
	mov	r7,r6
	shll16	r6
	shll8	r6		! Nxxx
	or	r6,r3		! NOPQ
	cmp/hi	r2,r0
	bt/s	3b
	 mov.l	r3,@-r0
#endif /* ! __LITTLE_ENDIAN__ */

	! Finally, copy a byte at once, if necessary

	add	#6,r5		!  50 EX
	cmp/eq	r4,r0		!  54 MT

	bt/s	9f		! 109 BR
	 add	#-6,r2		!  50 EX

8:	cmp/hi	r2,r0		!  57 MT
	mov.b	@(r0,r5),r1	!  20 LS (latency=2)

	bt/s	8b		! 109 BR

	 mov.b	r1,@-r0		!  29 LS

9:	rts
	 nop


ENTRY(memcpy)

	! Calculate the invariants which will be used in the remainder
	! of the code:
	!
	!      r4   -->  [ ...  ] DST             [ ...  ] SRC
	!	         [ ...  ]                 [ ...  ]
	!	           :                        :
	!      r0   -->  [ ...  ]       r0+r5 --> [ ...  ]
	!
	!

	! Short circuit the common case of src, dst and len being 32 bit aligned
	! and test for zero length move

	mov	r6, r0		!   5 MT (0 cycle latency)
	or	r4, r0		!  82 EX

	or	r5, r0		!  82 EX
	tst	r6, r6		!  86 MT

	bt/s	99f		! 111 BR		(zero len)
	 tst	#3, r0		!  87 MT

	mov	r4, r0		!   5 MT (0 cycle latency)
	add	r6, r0		!  49 EX

	bt/s	LOCAL(case00)	! 111 BR		(aligned)
	 sub	r4, r5		!  75 EX

	! Arguments are not nicely long word aligned or zero len.
	! Check for small copies, and if so do a simple byte at a time copy.
	!
	! Deciding on an exact value of 'small' is not easy, as the point at which
	! using the optimised routines become worthwhile varies (these are the
	! cycle counts for differnet sizes using byte-at-a-time vs. optimised):
	!	size	byte-at-time	long	word	byte
	!	16	42		39-40	46-50	50-55
	!	24	58		43-44	54-58	62-67
	!	36	82		49-50	66-70	80-85
	! However the penalty for getting it 'wrong' is much higher for long word
	! aligned data (and this is more common), so use a value of 16.

	mov	#16, r1		!   6 EX
	cmp/gt	r6,r1		!  56 MT

	add	#-1,r5		!  50 EX
	bf/s	6f		! 108 BR		(not small)

	 mov	r5, r3		!   5 MT (latency=0)
	shlr	r6		! 104 EX

	mov.b	@(r0,r5),r1	!  20 LS (latency=2)
	bf/s	4f		! 111 BR

	 add	#-1,r3		!  50 EX
	tst	r6, r6		!  86 MT

	bt/s	98f		! 110 BR
	 mov.b	r1,@-r0		!  29 LS

	! 4 cycles, 2 bytes per iteration
3:	mov.b	@(r0,r5),r1	!  20 LS (latency=2)

4:	mov.b	@(r0,r3),r2	!  20 LS (latency=2)
	dt	r6		!  67 EX

	mov.b	r1,@-r0		!  29 LS
	bf/s	3b		! 111 BR

	 mov.b	r2,@-r0		!  29 LS
98:
	rts
	 nop

99:	rts
	 mov	r4, r0

	! Size is not small, so its worthwhile looking for optimisations.
	! First align destination to a long word boundary.
	!
	! r5 = normal value -1

6:	tst	#3, r0		!  87 MT
	mov	#3, r3		!   6 EX

	bt/s	2f		! 111 BR
	 and	r0,r3		!  78 EX

	sub	r3,r6

	! 3 cycles, 1 byte per iteration
1:	dt	r3		!  67 EX
	mov.b	@(r0,r5),r1	!  19 LS (latency=2)

	bf/s	1b		! 109 BR

	 mov.b	r1,@-r0		!  28 LS

2:
	! Now select the appropriate bulk transfer code based on relative
	! alignment of src and dst.

	mov	r0, r3		!   5 MT (latency=0)
	mov	r5, r0		!   5 MT (latency=0)

	tst	#1, r0		!  87 MT
	 mov	#64, r7		!   6 EX

	mov	r4, r2		!   5 MT (latency=0)
	bt/s	1f		! 111 BR

	 tst	#2, r0

	! bit 0 clear

	cmp/ge	r7, r6		!  55 MT

	add	#-3, r5
	bt/s	2f		! 111 BR
	 tst	#2, r0		!  87 MT
	add	#7,r2

	! small
	bf/s	LOCAL(case0)
	 mov	r3, r0

	bra	LOCAL(case2)
	 add	#2,r5

	! big
2:	bf/s	LOCAL(case0b)
	 mov	r3, r0

	bra	LOCAL(case2b)
	 add	#2,r5

	! bit 0 set
1:	add	#7,r2

	bt/s	LOCAL(case1)
	 mov	r3, r0

	bra	LOCAL(case3)
	  add	#-2,r5		! 79 EX

	!
	!	GHIJ KLMN OPQR -->  GHIJ KLMN OPQR
	!

	! src, dst and size are all long word aligned
	! size is non-zero

	.balign	32
LOCAL(case00):
	mov	#64, r1		!   6 EX
	mov	r5, r3		!   5 MT (latency=0)

	cmp/gt	r6, r1		!  56 MT
	add	#-4, r5		!  50 EX

	bf	LOCAL(case0b)	! 108 BR		(big loop)
	shlr2	r6		! 105 EX

	shlr	r6		! 104 EX
	mov.l	@(r0, r5), r1	!  21 LS (latency=2)

	bf/s	4f		! 111 BR
	 add	#-8, r3		!  50 EX

	tst	r6, r6		!  86 MT
	bt/s	5f		! 110 BR

	 mov.l	r1,@-r0		!  30 LS

	! 4 cycles, 2 long words per iteration
3:	mov.l	@(r0, r5), r1	!  21 LS (latency=2)

4:	mov.l	@(r0, r3), r2	!  21 LS (latency=2)
	dt	r6		!  67 EX

	mov.l	r1, @-r0	!  30 LS
	bf/s	3b		! 109 BR

	 mov.l	r2, @-r0	!  30 LS

5:	rts
	 nop


	! Size is 16 or greater and less than 64, but may have trailing bytes

	.balign	32
LOCAL(case0):
	mov	r4, r7		!   5 MT (latency=0)
	mov.l	@(r0,r5),r1	!  20 LS

	mov	#4, r2
	add	#11, r7		!  50 E

	tst	r2, r6		!  86 MT
	mov	r5, r3		!   5 MT (latency=0)

	bt/s	4f		! 111 BR
	 add	#-4, r3		!  50 EX

	mov.l	r1,@-r0		!  30 LS

	! 4 cycles, 2 long words per iteration
3:	mov.l	@(r0, r5), r1	!  21 LS (latency=2)

4:	mov.l	@(r0, r3), r2	!  21 LS (latency=2)
	cmp/hi	r7, r0

	mov.l	r1, @-r0	!  30 LS
	bt/s	3b		! 109 BR

	 mov.l	r2, @-r0	!  30 LS
	mov	r4,r2		!   5 MT (latency=0)

	add	#1,r2		!  50 EX
	cmp/hi	r2, r0		!  57 MT

	bf/s	1f		! 109 BR
	 add	#2, r5

	mov.w	@(r0,r5),r1
	mov.w	r1,@-r0

	!
	! Finally, copy the last byte if necessary
1:	cmp/eq	r4,r0		!  54 MT
	bt/s	98b
	 add	#1,r5
	mov.b	@(r0,r5),r1
	rts
	 mov.b	r1,@-r0

	! Size is at least 64 bytes, so will be going round the big loop at least once.
	!
	!   r2 = rounded up r4
	!   r3 = rounded down r0

	.balign	32
LOCAL(case0b):
	mov	r0, r3		!   5 MT (latency=0)
	mov	#(~0x1f), r1	!   6 EX

	and	r1, r3		!  78 EX
	mov	r4, r2		!   5 MT (latency=0)

	cmp/eq	r3, r0		!  54 MT
	add	#0x1f, r2	!  50 EX

	bt/s	1f		! 110 BR
	 and	r1, r2		!  78 EX

	! copy initial words until cache line aligned

	mov.l	@(r0, r5), r1	!  21 LS (latency=2)
	tst	#4, r0		!  87 MT

	mov	r5, r6		!   5 MT (latency=0)
	add	#-4, r6		!  50 EX

	bt/s	4f		! 111 BR
	 add	#8, r3		!  50 EX

	tst	#0x18, r0	!  87 MT
	bt/s	1f		! 109 BR

	 mov.l	r1,@-r0		!  30 LS

	! 4 cycles, 2 long words per iteration
3:	mov.l	@(r0, r5), r1	!  21 LS (latency=2)

4:	mov.l	@(r0, r6), r7	!  21 LS (latency=2)
	cmp/eq	r3, r0		!  54 MT

	mov.l	r1, @-r0	!  30 LS
	bf/s	3b		! 109 BR

	 mov.l	r7, @-r0	!  30 LS

1:	mov	r2, r7
	add	r5, r7

	add	r0, r5
	add	#-0x1c, r5

	mov	r0, r6
	mov	r6, r3		! MT

	sub	r2, r3		! EX (r3 - r2 -> r3)a
	mov	#-5, r0

	shld	r0, r3		! number of cache lines

#ifdef __SH_FPU_DOUBLE__
	! Copy the cache line aligned blocks by using the FPU registers.
	! If src and dst are well aligned adopt 64-bit data transfers.
	!
	! In use: r0, r2, r4, r5
	! Scratch: r1, r3, r6, r7
	!
	! We also need r0 as a temporary (for movca), so 'undo' the invariant:
	!   r5:	 src (was r0+r5)
	!   r1:	 dest (was r0)
	! this can be reversed at the end, so we don't need to save any extra
	! state.
	!
	mov	#8, r0
	cmp/ge	r0, r3		! Check if there are many cache lines to copy.

	bt/s	1f
	 mov	r5, r1

#ifdef PREFETCH
	add	#-0x20, r1
	pref	@r1
#endif

	add	#4, r2
	bra	45f		! Copy cache line aligned blocks.
	 add	#0x24, r7	! Prefetch limit (src + 1 cache lines)

1:	fmov.s	fr12, @-r15
	fmov.s	fr13, @-r15
	add	#0x44, r7	! Prefetch limit (src + 1 cache lines)
	fmov.s	fr14, @-r15
#if PREFETCH == 6
	add	#-0x20, r1
#elif PREFETCH == 2
	add	#-0x60, r1
#endif
	fmov.s	fr15, @-r15

	SAVE_AND_SET_FPSCR

	mov	r5, r0

#ifdef PREFETCH
	pref	@r1		! first line

	add	#-0x20, r1
	pref	@r1		! 2nd line
#endif

#if PREFETCH == 6
	add	#-0x20, r1
	pref	@r1

	add	#-0x20, r1
	pref	@r1

	add	#-0x20, r1
	pref	@r1

	add	#-0x20, r1
	pref	@r1
#endif

1:	tst	#7, r0		! src is 64 bit aligned
	bf/s	89f

	  mov	#4, r0
	fschg

	.balign 4
67:	fmov.d	@r5+, dr0
	fmov.d	@r5+, dr2
	fmov.d	@r5+, dr4
	fmov.d	@r5+, dr6
	add	#-0x40,r5
	fmov.d	@r5+, dr8
	fmov.d	@r5+, dr10
	cmp/ge	r7, r1
	fmov.d	@r5+, dr12
	bf/s	1f
	 fmov.d	@r5+, dr14

#ifdef PREFETCH
	add	#-0x20, r1
	pref	@r1
	add	#-0x20, r1
	pref	@r1
#endif

1:	add	#-0x40,r5
	fmov.d	@r5+, xd0
	fmov.d	@r5+, xd2
	fmov.d	@r5+, xd4
	fmov.d	@r5+, xd6
	add	#-0x40,r5
	fmov.d	@r5+, xd8
	add	#-8, r6
	fmov.d	@r5+, xd10
	cmp/ge	r7, r1
	fmov.d	@r5+, xd12
	bf/s	1f
	 fmov.d	@r5+, xd14

#ifdef PREFETCH
	add	#-0x20, r1
	pref	@r1
	add	#-0x20, r1
	pref	@r1
#endif

1:	add	#-0x40,r5
	movca.l	r0, @r6
	fmov.d	dr6, @r6
	fmov.d	dr4, @-r6
	fmov.d	dr2, @-r6
	fmov.d	dr0, @-r6

	add	#-8, r6
	movca.l	r0, @r6
	fmov.d	dr14, @r6
	fmov.d	dr12, @-r6
	fmov.d	dr10, @-r6
	fmov.d	dr8, @-r6

	add	#-8, r6
	movca.l	r0, @r6
	fmov.d	xd6, @r6
	fmov.d	xd4, @-r6
	fmov.d	xd2, @-r6
	fmov.d	xd0, @-r6

	add	#-8, r6
	movca.l	r0, @r6
	fmov.d	xd14, @r6
	add	#-4, r3
	fmov.d	xd12, @-r6
	cmp/ge	r0, r3
	fmov.d	xd10, @-r6
	bt/s	67b
	 fmov.d	xd8, @-r6

	bf/s 2f
	 fschg

	.balign 4
89:	fmov.s	@r5+, fr0
	fmov.s	@r5+, fr1
	fmov.s	@r5+, fr2
	fmov.s	@r5+, fr3
	fmov.s	@r5+, fr4
	fmov.s	@r5+, fr5
	fmov.s	@r5+, fr6
	fmov.s	@r5+, fr7
	add	#-0x40,r5

	fmov.s	@r5+, fr8
	fmov.s	@r5+, fr9
	fmov.s	@r5+, fr10
	fmov.s	@r5+, fr11
	fmov.s	@r5+, fr12
	fmov.s	@r5+, fr13
	cmp/ge	r7, r1
	fmov.s	@r5+, fr14
	bf/s	1f
	 fmov.s	@r5+, fr15

#ifdef PREFETCH
	add	#-0x20, r1
	pref	@r1
	add	#-0x20, r1
	pref	@r1
#endif

1:	frchg	! bank 1

	 add	#-0x40, r5
	fmov.s	@r5+, fr0
	fmov.s	@r5+, fr1
	fmov.s	@r5+, fr2
	fmov.s	@r5+, fr3
	fmov.s	@r5+, fr4
	fmov.s	@r5+, fr5
	fmov.s	@r5+, fr6
	fmov.s	@r5+, fr7
	 add	#-0x40, r5

	fmov.s	@r5+, fr8
	fmov.s	@r5+, fr9
	fmov.s	@r5+, fr10
	fmov.s	@r5+, fr11
	fmov.s	@r5+, fr12
	add	#-4, r6
	fmov.s	@r5+, fr13
	cmp/ge	r7, r1
	fmov.s	@r5+, fr14
	bf/s	1f
	 fmov.s	@r5+, fr15

#ifdef PREFETCH
	add	#-0x20, r1
	pref	@r1
	add	#-0x20, r1
	pref	@r1
#endif

1:	add	#-0x40, r5
	frchg	! bank 0

	movca.l	r0, @r6
	fmov.s	fr7, @r6
	fmov.s	fr6, @-r6
	fmov.s	fr5, @-r6
	fmov.s	fr4, @-r6
	fmov.s	fr3, @-r6
	fmov.s	fr2, @-r6
	fmov.s	fr1, @-r6
	fmov.s	fr0, @-r6

	add	#-4, r6
	movca.l	r0, @r6
	fmov.s	fr15, @r6
	fmov.s	fr14, @-r6
	fmov.s	fr13, @-r6
	fmov.s	fr12, @-r6
	fmov.s	fr11, @-r6
	fmov.s	fr10, @-r6
	fmov.s	fr9, @-r6
	fmov.s	fr8, @-r6

	frchg	! bank 1

	add	#-4, r6
	movca.l	r0, @r6
	fmov.s	fr7, @r6
	fmov.s	fr6, @-r6
	fmov.s	fr5, @-r6
	fmov.s	fr4, @-r6
	fmov.s	fr3, @-r6
	fmov.s	fr2, @-r6
	fmov.s	fr1, @-r6
	fmov.s	fr0, @-r6

	add	#-4, r6
	movca.l	r0, @r6
	fmov.s	fr15, @r6
	fmov.s	fr14, @-r6
	fmov.s	fr13, @-r6
	fmov.s	fr12, @-r6
	fmov.s	fr11, @-r6
	fmov.s	fr10, @-r6
	add	#-4, r3
	fmov.s	fr9, @-r6
	cmp/ge	r0, r3
	fmov.s	fr8, @-r6

	bt/s	89b
	 frchg	! bank 0

	.balign 4
2:
	! Restore FPU callee save registers
	RESTORE_FPSCR
	fmov.s	@r15+, fr15
	fmov.s	@r15+, fr14
	tst	 r3, r3
	fmov.s	@r15+, fr13

	! Other cache lines could be copied: so use the FPU in single
	! precision. No check for alignment is necessary.

	bt/s	5f
	 fmov.s	@r15+, fr12

	add	#-0x20, r7	! Prefetch limit (src + 1 cache lines)

	cmp/ge	r7, r1
	bf/s	45f
	  add	#4, r2

#else /* __SH_FPU_DOUBLE__ */

	! Those instructions could be better scheduled with the NOFPU
	! block bellow but factorize them here form simplicity.
	add	#0x24, r7	! Prefetch limit (src + 1 cache lines)
	mov	r5, r1
	add	#4, r2

#endif /* __SH_FPU_DOUBLE__ */

#ifdef __SH4_NOFPU__
	mov.l	r8, @-r15	!  30 LS
	mov.l	r9, @-r15	!  30 LS
	mov.l	r10, @-r15	!  30 LS
	mov.l	r11, @-r15	!  30 LS
	mov.l	r12, @-r15	!  30 LS
	add	#-0x20, r1
	mov.l	r13, @-r15	!  30 LS
#ifdef PREFETCH
	pref	@r1
#endif

4:	mov.l	@r5+, r13
	mov.l	@r5+, r12
	mov.l	@r5+, r11
	mov.l	@r5+, r10
	mov.l	@r5+, r9
	add	#-4, r6
	mov.l	@r5+, r8
	cmp/ge	r7, r1
	mov.l	@r5+, r3
	bf/s	1f
	 mov.l	@r5+, r0

#ifdef PREFETCH
	add	#-0x20, r1
	pref	@r1
#endif

1:	movca.l	r0, @r6
	mov.l	r3,@-r6
	mov.l	r8,@-r6
	mov.l	r9,@-r6
	mov.l	r10,@-r6
	mov.l	r11,@-r6
	mov.l	r12,@-r6
	cmp/eq	r2, r6
	mov.l	r13,@-r6

	bf/s	4b
	 add	#-0x40,r5

	mov.l	 @r15+,r13	!  30 LS
	mov.l	 @r15+,r12	!  30 LS
	mov.l	 @r15+,r11	!  30 LS
	mov.l	 @r15+,r10	!  30 LS
	mov.l	 @r15+,r9	!  30 LS
	mov.l	 @r15+,r8	!  30 LS
#else /* __SH4_NOFPU_ */

#ifdef PREFETCH
	add	#-0x20, r1
	pref	@r1
#endif

	! This block is used for SH_FPU_SINGLE and __SH_FPU_DOUBLE__ modes.
45:	fmov.s	@r5+, fr0
	fmov.s	@r5+, fr1
	fmov.s	@r5+, fr2
	fmov.s	@r5+, fr3
	fmov.s	@r5+, fr4
	add	#-4,r6
	fmov.s	@r5+, fr5
	cmp/ge	r7, r1
	mov.l	@r5+, r3
	bf/s	1f
	 mov.l	@r5+, r0

#ifdef PREFETCH
	add	#-0x20, r1
	pref	@r1
#endif

1:	movca.l	r0, @r6
	mov.l	r3, @-r6
	fmov.s	fr5, @-r6
	fmov.s	fr4, @-r6
	fmov.s	fr3, @-r6
	fmov.s	fr2, @-r6
	fmov.s	fr1, @-r6
	cmp/eq	r2, r6
	fmov.s	fr0, @-r6

	bf/s	45b
	 add	#-0x40,r5
#endif /* ! __SH4_NOFPU__ */

5:	mov	r6, r0
	sub	r4, r6

	shlr2	r6
	tst	r6,r6

	bt/s	4f
	 add	#28, r5

3:	mov.l	@r5,r1
	dt	r6
	add	#-4,r5
	bf/s	3b		! 109 BR
	 mov.l	r1,@-r0		!  30 LS

4:	sub	r0, r5		!  75 EX
	mov	r4, r2		!   5 MT (latency=0)

	add	#2, r2		!  50 EX
	cmp/hs	r2, r0		!  57 MT

	bf/s	1f		! 109 BR
	 add	#2, r5

	mov.w	@(r0,r5),r1
	mov.w	r1,@-r0

	!
	! Finally, copy the last byte if necessary
1:	cmp/eq	r4, r0		!  54 MT
	bt/s	98f
	 add	#1, r5
	mov.b	@(r0,r5),r1
	rts
	 mov.b	r1,@-r0

98:	rts
	 nop

	!
	!	GHIJ KLMN OPQR -->  ..GH IJKL MNOP QR..
	!

	.balign	32
LOCAL(case2):
	! Size is 16 or greater and less then 64, but may have trailing bytes

	mov	r5, r6		!   5 MT (latency=0)
	add	#-2,r6		!  50 EX

3:	mov.w	@(r0,r5),r1	!  20 LS (latency=2)

	mov.w	@(r0,r6),r3	!  20 LS (latency=2)
	cmp/hi	r2,r0		!  57 MT

	mov.w	r1,@-r0		!  29 LS
	bt/s	3b		! 111 BR

	 mov.w	r3,@-r0		!  29 LS

	 mov	r4,r2		!   5 MT (latency=0)
	add	#1,r2		!  50 EX

	cmp/hi	r2, r0		!  57 MT
	bf	1f		! 109 BR

	mov.w	@(r0,r5),r1
	mov.w	r1,@-r0

	!
	! Finally, copy the last byte if necessary
1:	cmp/eq	r4,r0		!  54 MT
	bt/s	98f
	 add	#1,r5
	mov.b	@(r0,r5),r1
	rts
	 mov.b	r1,@-r0

98:	rts
	 nop

	.balign	32
LOCAL(case2b):
	! Size is at least 64 bytes, so will be going round the big loop at least once.
	!
	!   r2 = rounded up r4
	!   r3 = rounded down r0

	mov	#(~0x1f), r1	!   6 EX
	and	r1, r3		!  78 EX

	cmp/eq	r3, r0		!  54 MT
	add	#0x1f, r2	!  50 EX

	bt/s	1f		! 110 BR
	 and	r1, r2		!  78 EX

	! Copy a short word one at a time until we are cache line aligned
	!   Normal values: r0, r2, r3, r4
	!   Unused: r1, r6, r7
	!   Mod: r5 (=r5-2)
	!
	add	#2, r3		!  50 EX

2:	mov.w	@(r0,r5),r1	!  20 LS (latency=2)
	cmp/eq	r3,r0		!  54 MT

	bf/s	2b		! 111 BR

	 mov.w	r1,@-r0		!  29 LS

	! Copy the cache line aligned blocks
	!
	! In use: r0, r2, r4, r5 (=r5-2)
	! Scratch: r1, r3, r6, r7
	!
	! We could do this with the four scratch registers, but if src
	! and dest hit the same cache line, this will thrash, so make
	! use of additional registers.
	!
	! We also need r0 as a temporary (for movca), so 'undo' the invariant:
	!   r5:	 src (was r0+r5)
	!   r1:	 dest (was r0)
	! this can be reversed at the end, so we don't need to save any extra
	! state.
	!
1:	mov.l	r8, @-r15	!  30 LS
	add	r0, r5		!  49 EX

	mov.l	r9, @-r15	!  30 LS
	mov	r0, r1		!   5 MT (latency=0)

	mov.l	r10, @-r15	!  30 LS
	add	#-0x1e, r5	!  50 EX

	mov.l	r11, @-r15	!  30 LS

	mov.l	r12, @-r15	!  30 LS

	! 17 cycles, 32 bytes per iteration
#ifdef __LITTLE_ENDIAN__
2:	mov.w	@r5+, r0	!  14 LS (latency=2)		..JI
	add	#-0x20, r1	!  50 EX

	mov.l	@r5+, r3	!  15 LS (latency=2)		NMLK

	mov.l	@r5+, r6	!  15 LS (latency=2)		RQPO
	shll16	r0		! 103 EX			JI..

	mov.l	@r5+, r7	!  15 LS (latency=2)
	xtrct	r3, r0		!  48 EX			LKJI

	mov.l	@r5+, r8	!  15 LS (latency=2)
	xtrct	r6, r3		!  48 EX			PONM

	mov.l	@r5+, r9	!  15 LS (latency=2)
	xtrct	r7, r6		!  48 EX

	mov.l	@r5+, r10	!  15 LS (latency=2)
	xtrct	r8, r7		!  48 EX

	mov.l	@r5+, r11	!  15 LS (latency=2)
	xtrct	r9, r8		!  48 EX

	mov.w	@r5+, r12	!  15 LS (latency=2)
	xtrct	r10, r9		!  48 EX

	movca.l	r0,@r1		!  40 LS (latency=3-7)
	xtrct	r11, r10	!  48 EX

	mov.l	r3, @(0x04,r1)	!  33 LS
	xtrct	r12, r11	!  48 EX

	mov.l	r6, @(0x08,r1)	!  33 LS

	mov.l	r7, @(0x0c,r1)	!  33 LS

	mov.l	r8, @(0x10,r1)	!  33 LS
	add	#-0x40, r5	!  50 EX

	mov.l	r9, @(0x14,r1)	!  33 LS
	cmp/eq	r2,r1		!  54 MT

	mov.l	r10, @(0x18,r1)	!  33 LS
	bf/s	2b		! 109 BR

	 mov.l	r11, @(0x1c,r1)	!  33 LS
#else /* ! __LITTLE_ENDIAN__ */
2:	mov.w	@(0x1e,r5), r0	!  17 LS (latency=2)
	add	#-2, r5		!  50 EX

	mov.l	@(0x1c,r5), r3	!  18 LS (latency=2)
	add	#-4, r1		!  50 EX

	mov.l	@(0x18,r5), r6	!  18 LS (latency=2)
	shll16	r0		! 103 EX

	mov.l	@(0x14,r5), r7	!  18 LS (latency=2)
	xtrct	r3, r0		!  48 EX

	mov.l	@(0x10,r5), r8	!  18 LS (latency=2)
	xtrct	r6, r3		!  48 EX

	mov.l	@(0x0c,r5), r9	!  18 LS (latency=2)
	xtrct	r7, r6		!  48 EX

	mov.l	@(0x08,r5), r10	!  18 LS (latency=2)
	xtrct	r8, r7		!  48 EX

	mov.l	@(0x04,r5), r11	!  18 LS (latency=2)
	xtrct	r9, r8		!  48 EX

	mov.l   @(0x00,r5), r12 !  18 LS (latency=2)
	xtrct	r10, r9		!  48 EX

	movca.l	r0,@r1		!  40 LS (latency=3-7)
	add	#-0x1c, r1	!  50 EX

	mov.l	r3, @(0x18,r1)	!  33 LS
	xtrct	r11, r10	!  48 EX

	mov.l	r6, @(0x14,r1)	!  33 LS
	xtrct	r12, r11	!  48 EX

	mov.l	r7, @(0x10,r1)	!  33 LS

	mov.l	r8, @(0x0c,r1)	!  33 LS
	add	#-0x1e, r5	!  50 EX

	mov.l	r9, @(0x08,r1)	!  33 LS
	cmp/eq	r2,r1		!  54 MT

	mov.l	r10, @(0x04,r1)	!  33 LS
	bf/s	2b		! 109 BR

	 mov.l	r11, @(0x00,r1)	!  33 LS
#endif /* ! __LITTLE_ENDIAN__ */

	mov.l	@r15+, r12
	mov	r1, r0		!   5 MT (latency=0)

	mov.l	@r15+, r11	!  15 LS
	sub	r1, r5		!  75 EX

	mov.l	@r15+, r10	!  15 LS
	sub	r4, r1

	 mov.l	 @r15+, r9	!  15 LS
	shlr	r1

	mov.l	@r15+, r8	!  15 LS
	tst	r1,r1

	bt/s	1f
	add	#0x1e, r5	!  50 EX

3:	mov.w	@(r0,r5),r6		!  20 LS (latency=2)
	dt	r1
	bf/s	3b		! 111 BR
	 mov.w	r6,@-r0		!  29 LS

	!
	! Finally, copy the last byte if necessary
1:	cmp/eq	r4,r0		!  54 MT
	bt/s	98f
	 add	#1,r5
	mov.b	@(r0,r5),r1
	rts
	 mov.b	r1,@-r0

98:	rts
	 nop

END(memcpy)
libc_hidden_builtin_def (memcpy)
