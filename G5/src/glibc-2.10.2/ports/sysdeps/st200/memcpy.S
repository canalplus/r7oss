/* Optimised memcpy() for st231. More micro optimisations are possible with this,
 * in particular the single word copies could be changed to predicated load/stores
 * by using the 4 word stack scratch area to dump out unwanted load/stores. The
 * unaligned initial stores could also be pipelined that way.
 *
 * There is also no reason why this code cannot be used for copy_to_user() in the
 * kernel as well. This will give a substantial performance boost. Revisit this code
 * when I optimise copy_to_user()
 */

#ifdef __ST240__
.assume st240
#elif defined __ST231__
.assume st231
#else
#  error Unknown target.
#endif

#include <sysdep.h>
.nonopinsertion

/* memcpy $r16 = dst, $r17 = src $r18 = len */

/* Global variables used, we are so spoilt for choice on the ST200 for registers it is
 * almost embarrassing!
 *
 * $r16	- Original dst address - DO NOT MODIFY, used to return
 * $r17 - Current src address for reading
 * $r18 - Number of bytes left to copy
 * $r19 - Current dst address
 * $r20 - Current pft address - basically 256 bytes above current src
 * $r21 - Address to prefetch from.
 * $r22 - End address, start + len, if >= out of range
 * $r23	- Word aligned end address.
 * $r24 - Dest alignment (dest & 0x3)
 * $r25 - Shift for previous word in unaligned transfers
 * $r26 - Shift for current word in unaligned transfers
 *
 */


/* This code does "perfect prefetching", it issues exactly the correct number
 * of prefetches required (well, except in the 0 length case). When the
 * prefetch address is > the end address, prefetching is switched off by
 * pointing the register at a control address. That's where the -228 comes from
 * as 224 + -228 is still a control register. Initially I was using address
 * zero, but that incurs a penalty on prefetch because the instruction will
 * miss on the DTLB and will have to check the UTLB which incurs a 5 cycle
 * penalty
 */


ENTRY(memcpy):
	pft	0[$r17]			/* We know we are going to look at these bytes */
	cmpeq	$b0 = $r18, $r0	 	/* $b0 true if nothing to do */
	or	$r42 = $r16, $r17
	cmpltu	$b7 = $r18, 4		/* Less than a word, have to do byte at a time */
	;;
	mov	$r19 = $r16		/* $r19 contains dst */
	and	$r42 = $r42, 0x3	/* $r42 true if src or dst not aligned */
	cmpltu	$b2 = $r18, 32		/* Less than 32 bytes, do word at a time */
	xor	$r24 = $r16, $r17	/* Check for same alignement */
	;;
	convib	$b1 =  $r42		/* $b1 true if src or dst not aligned */
	cmpltu	$b3 = $r18, 64		/* < 64 bytes, don't issue next prefetch */
	slctf	$r21 = $b2, $r17, -228
	and	$r24 = $r24, 0x3	/* Zero if same alignement */
	;;
	br	$b0, .Ldone		/* Exit on len = 0, zero latency on $b0 */
	pft	32[$r21]
	slctf 	$r21 = $b3, $r17, -228
	cmpltu	$b4 = $r18, 96
	;;
	slctf 	$r21 = $b4, $r17, -228
	cmpltu	$b5 = $r18, 128
	pft	64[$r21]
	br	$b7, .Lbyte_at_a_time 	/* len < 4, so just copy byte at a time. Zero latency on $b7 */
	;;
	br	$b1, .Lstrange_align /* Byte at a time, zero latency on $b1 */
	slctf	$r21 = $b5, $r17, -228
	cmpltu  $b6 = $r18, 160
	pft	96[$r21]
	;;
	brf	$b2, .Lpft_precharge_loop	/* Zero latency on $b2 */
	pft	128[$r21]
	cmpltu  $b7 = $r18, 160
	slctf 	$r21 = $b6, $r17, -228
	;;

/* Must be at least one word to copy */
.Lword_at_a_time:
	ldw	$r50 = 0[$r17]
	cmpeq	$b0 = $r18, 4
	cmpltu	$b1 = $r18, 8
	;;
	add	$r17 = $r17, 4
	;;
	add	$r18 = $r18, -4
	;;
	stw	0[$r19] = $r50 /* 2 cycle stall for load */
	add	$r19 = $r19, 4
	brf	$b1 , .Lword_at_a_time
	;;
	br 	$b0, .Ldone
	;;

.Lbyte_at_a_time:
	ldb	$r50 = 0[$r17]
	cmpeq	$b0 = $r18, 1
	;;
	add	$r17 = $r17, 1
	;;
	add	$r18 = $r18, -1
	;;
	stb	0[$r19] = $r50
	add	$r19 = $r19, 1
	brf	$b0, .Lbyte_at_a_time
	;;
.Ldone:
	goto	$r63
	;;


.Lpft_precharge_loop:
	pft	160[$r21]
	slctf	$r21 = $b7, $r17, -228
	cmpltu	$b0 = $r18, 224
	;;
	pft	192[$r21]
	slctf	$r21 = $b0, $r17, -228
	mov	$r20 = $r17
	add	$r22 = $r17, $r18	/* End address */
	;;
	pft	224[$r21]
	add	$r20 = $r17, 256 	/* We should now have all 8 prefetch buffers running */
	;;


/* Pft in middle of loop. This is a hard one to call. If you put the pft at the
 * end of the loop, you can do an extra check to see if the pft is still doing
 * anything usefull and not execute it So this saves 1 cycle round the loop in
 * the last 8 loops. However, if you issue the pft before the stores, you then
 * gain an extra 9 cycles of latency on the prefetch. Which is best depends on
 * what you are doing. You could have two loops, but it's not worth it. I've
 * elected to gain the latency on the prefetch
 */

.Lcache_line_loop:
	ldw	$r50	= 0[$r17]
	cmpltu  $b2 = $r20, $r22 /* Prefetch next line ? */
	;;
	ldw	$r51 	= 4[$r17]
	slct	$r21 = $b2, $r20, -228 /* Switch off prefetch */
	;;
	ldw	$r52	= 8[$r17]
	;;
	ldw	$r53	= 12[$r17]
	;;
	ldw	$r54	= 16[$r17]
	;;
	ldw	$r55	= 20[$r17]
	;;
	ldw	$r56	= 24[$r17]
	;;
	ldw	$r57	= 28[$r17]	/* Cache line consumed, prefetch buffer is empty */
	add	$r17 = $r17, 32
	add	$r18 = $r18, -32
	;;
	pft	0[$r21]		/* Issue next prefetch */
	;;
	stw	0[$r19]	= $r50
	cmpltu	$b1 = $r18, 32 /* Terminate loop ? */
	;;
	stw	4[$r19]	= $r51
	;;
	stw	8[$r19]	= $r52
	cmpeq	$b3 = $r18, 0  /* $b3 if memcpy complete */
	cmpltu	$b4 = $r18, 4 /* $b4 if byte remainder */
	;;
	stw	12[$r19]= $r53
	add	$r20 = $r20, 32	/* Prefetch address */
	;;
	stw	16[$r19]= $r54
	;;
	stw	20[$r19]= $r55
	;;
	stw	24[$r19]= $r56
	;;
	stw	28[$r19]= $r57
	add	$r19 = $r19, 32
	brf	$b1, .Lcache_line_loop	/* Branch if !done */
	;;
	br 	$b3, .Ldone
	;;
	br	$b4, .Lbyte_at_a_time
	;;
	goto 	.Lword_at_a_time
	;;

/* Copy 1-3  bytes from src to dst */
.Lalign_src_dst_to_word:
	ldb	$r50 = 0[$r17]
	cmpeq	$b0 = $r24, 1
	cmpltu	$b1 = $r25, 32		/* True if we have < 32 bytes at the end of this loop */
	cmpltu	$b2 = $r25, 4		/* True if only bytes left after this loop */
	;;
	add	$r17 = $r17, 1
	cmpne	$b3 = $r25, 0 		/* True if we are done! */
	;;
	add	$r24 = $r24, -1
	add	$r18, $r18, -1
	;;
	stb	0[$r19] = $r50
	add	$r19 = $r19, 1
	brf	$b0, .Lalign_src_dst_to_word
	;;
	/* We are now word aligned. If > 32 bytes to
  	 * copy, goto precharge_pft, otherwise go to
	 * word at a time copy
	 */
	brf	$b1, .Lpft_precharge_loop
	;;
	br	$b2, .Lbyte_at_a_time
	;;
	br	$b3, .Lword_at_a_time
	;;
	goto	$r63
	;;

/* We get here if
 *	- src and dst are not WORD ALIGNED!
 *  	We check for if src and dst have the SAME alignement,
 * 	in which case we just copy the correct number of bytes
 * 	to word align, and then jump back into main loop. We have
 * 	to do the missing prefetch or else we will pessimise everything
 *	badly.
 * 	- $r24 contains xor of src,dst and 0x3
 *	- $b2 true if < 32, no latency
 * 	we assume that we are going to to be aligned on the same
 * 	boundary, so keep the prefetches going, then jump to the
 * 	correct loop
 *
 */

/* We can probably shift the pft_precharge loop here as well, so
 * that we can jump straight into the word at a time case
 */
.Lstrange_align:
	cmpeq	$b0  = $r24, 0
	cmpltu	$b1  = $r18, 16			/* True if < 16 bytes */
	pft	128[$r21]			/* Assume same alignment, have to keep pipeline prefetch going */
	and	$r24 = $r17, 0x3		/* Alignement of src */
	;;
	cmpeq	$b2 = $r24, 0			/* True if  src is aligned */
	cmpltu  $b7 = $r18, 160
	slctf 	$r21 = $b6, $r17, -228
	sub	$r24 = 4, $r24			/* Number of bytes to copy */
	;;
	sub	$r25 = $r18, $r24		/* We cannot be here for memcpy of < 4, so safe */
	br	$b0, .Lalign_src_dst_to_word	/* Two cycle stall on $b0 - can we re-pipeline? */
	add	$r22 = $r17, $r18		/* Last address */
	;;
	/* Curses. We are not aligned on the same boundary. That unfortunately means we
	 * have to do the horrible case of load, shift, and or.  Yuck
	 */
	br	$b1,	.Lbyte_at_a_time		/* < 16 bytes, so just do a simple byte loop */
	and	$r24 = $r19, 0x3		/* dst alignment */
	and	$r25 = $r17, 0x3		/* Alignement of src */
	and	$r23 = $r22, ~0x3		/* Last word aligned address */
	;;
	br	$b2,	.Lsrc_aligned
	sub	$r25 = 4, $r25
	;;
/* Store out n bytes, so that src is aligned  */
.Lalign_src_to_word:
	ldb	$r50 = 0[$r17]
	add	$r17 = $r17, 1
	cmpeq	$b0 = $r25, 1
	add	$r25 = $r25, -1
	;;
	stb	0[$r19] = $r50
	add	$r19 = $r19, 1
	add	$r18 = $r18, -1		/* Change remaining bytes */
	;;
	brf	$b0, .Lalign_src_to_word;
	and	$r24 = $r19, 0x3	/* New destination alignement */
	;;

/* OK, at this point the src is aligned correctly. We now can load the word up,
   and then store it in the dst until it is aligned on a word boundary as well.
   We know that the dst must be misaligned here because we have already removed
   the case where the src and dst have the same alignment. Therefore as src is
   aligned dst can no longer be even if it was to start.
 */

.Lsrc_aligned:
	ldw	$r50 = 0[$r17]		/* Aligned */
	sub	$r25 = 4, $r24	/* Number of bytes to align dst */
	add	$r17 = $r17, 4
	shru	$r47 = $r18, 2	/* Number of complete words we can copy in upcoming loop */
	;;
	mov	$r53 = $r50
	add	$r47 = $r47, -1 /* We have already read one word */
	;;

.Lalign_dst_to_word:
	stb	0[$r19] = $r50
	shru	$r50 = $r50, 8
	cmpeq	$b0 = $r25, 1
	;;
	add	$r25 = $r25, -1
	add	$r19 = $r19, 1
	add	$r18 = $r18, -1 	/* $r18 always holds the number of bytes remaining */
	brf	$b0, .Lalign_dst_to_word
	;;

/* Now src and dst are both aligned, we can now start the main loop.

/* $r24 contains dest alignment, this controls how much shifting we have to
 * do in order to get everything to work out right
 */
.Lshifter_prep:
	shru	$r48 = $r47, 3	/* number of big loops */
	shl	$r26 = $r24, 3	/* $r26 is shift for the new word */
	sub 	$r25 = 4, $r24
	;;
	shl	$r25 = $r25, 3 /* $r25 is shift for the previous word */
	mov	$r50 = $r53
	cmpgtu	$b0 = $r48, 0 /* Do we go round the big loops or not?  */
	;;
	br 	$b0, .Lshifter_pft_precharge_loop # Stall reshcedule
	;;

/* This could be pipelined a bit better */
.Lshifter_word_at_a_time:
	ldw	$r51 = 0[$r17]
	shru	$r50 = $r50, $r25
	add	$r17 = $r17, 4
	;;
	shl	$r52 = $r51, $r26
	cmpltu	$b0 = $r17, $r23
	;;
	or	$r52 = $r52, $r50
	;;
	stw	0[$r19] = $r52
	mov	$r50 = $r51 /* new word is now the old word */
	add	$r19 = $r19, 4
	add	$r18 = $r18, -4
	;;
	br	$b0, .Lshifter_word_at_a_time
	shru	$r53 = $r51, $r25	/* For tail operation */
	;;

/* Ok, we are now out of the main byte loop. We need to now dump out
 * n bytes from the last word read, byte at a time. There will always
 * be something to dump out here, so no test is needed.
 */
.Ldump_tail:
	stb	0[$r19] = $r53
	shru	$r53 = $r53, 8
	add	$r24 = $r24, -1
	cmpeq	$b0 = $r24, 1
	;;
	add	$r18 = $r18, -1
	cmpeq	$b1 = $r18, 1		/* True if nothing to do after loop finished */
	add	$r19 = $r19, 1
	;;
	brf	$b0, .Ldump_tail
	;;
	brf	$b1, .Lbyte_at_a_time
	;;
	goto	$r63
	;;

/* This should die, it can be subsumed into the above logic
 * without any penalty. Here now for testing
 */
.Lshifter_pft_precharge_loop:
	pft	160[$r21]
	slctf	$r21 = $b7, $r17, -228
	cmpltu	$b0 = $r18, 224
	;;
	pft	192[$r21]
	slctf	$r21 = $b0, $r17, -228
	mov	$r20 = $r17
	;;
	pft	224[$r21]
	add	$r20 = $r17, 256 	/* We should now have all 8 prefetch buffers running */
	;;


/* Can assume at least 32 bytes to do on entry to this loop,
 * and that both src and dst are aligned
 * Ensure $r50 contains starting word when you come into this loop.
 */
.Lshifter_line_at_a_time:
	ldw	$r51 = 0[$r17]
	add	$r48 = $r48, -1 # Decrement count
	cmpltu	$b2 = $r20, $r22
	;;
	ldw	$r52 = 4[$r17]
	slct	$r21 = $b2, $r20, -228	/* Stop prefetching */
	cmpeq	$b0 = $r48, 0
	;;
	ldw	$r53 = 8[$r17]
	;;
	ldw	$r54 = 12[$r17]
	;;
	ldw	$r55 = 16[$r17] /* $r51 now available with no latency */
	shru	$r49 = $r50, $r25	/* $r50 contains the previous value from end of loop! */
	shl	$r31 = $r51, $r26
	;;
	ldw	$r56 = 20[$r17]		/* $r52 now available no latency */
	shru	$r49 = $r51, $r25
	shl	$r32 = $r52, $r26
	or	$r31 = $r31, $r49	/* $r31 now contains word to store */
	;;
	ldw	$r57 = 24[$r17]
	shru	$r49 = $r52, $r25
	shl	$r33 = $r53, $r26
	or	$r32 = $r32, $r49
	;;
	ldw	$r58 = 28[$r17]
	shru	$r49 = $r53, $r25
	shl	$r34 = $r54, $r26
	or	$r33 = $r33, $r49
	;;
	pft	0[$r21]		/* Issue next prefetch */
	shru	$r49 = $r54, $r25
	shl	$r35 = $r55, $r26
	or	$r34 = $r34, $r49
	;;
	stw	0[$r19] = $r31
	shru	$r49 = $r55, $r25
	shl	$r36 = $r56, $r26
	or	$r35 = $r35, $r49
	;;
	stw	4[$r19] = $r32
	shru	$r49 = $r56, $r25
	shl	$r37 = $r57, $r26
	or	$r36 = $r36, $r49
	;;
	stw	8[$r19] = $r33
	shru	$r49 = $r57, $r25
	shl	$r38 = $r58, $r26
	or	$r37 = $r37, $r49
	;;
	stw	12[$r19] = $r34
	or	$r38 = $r38, $r49	/* All done ! */
	mov	$r50 = $r58		/* $r50 now becomes the stub for the next loop */
	;;
	stw	16[$r19] = $r35
	add	$r17 	= $r17, 32
	;;
	stw	20[$r19] = $r36
	cmpltu	$b1 = $r17, $r23	# $r23 contains last word address
	;;
	stw	24[$r19] = $r37
	add	$r20	= $r20, 32	# Prefetch address
	;;
	stw	28[$r19] = $r38
	add	$r19	= $r19, 32	/* Onto next line */
	add	$r18	= $r18, -32
	brf	$b0, .Lshifter_line_at_a_time
	;;
	/* End of loop */
	/* Right, we are now out of the main unrolled loop,
	 * The rump data is now in $r50. So, if we have more than
	 * a word left to go, then have to jump to the word at a
	 * a time loop above. If not, then we have to jump to the dump_tail
	 * unfortunately , we have to also set up the loop counters for the above
 	 * loops. Would be better with invarients for these.
	 */
	br	$b1, .Lshifter_word_at_a_time;
	shru	$r53 = $r50, $r25	/* For tail operation */
	;;
	goto	.Ldump_tail	// There are always some bytes to dump out
	;;

END(memcpy)

libc_hidden_builtin_def (memcpy)

