/*
 * -------------------------------------------------------------------------
 * <linux_root>/arch/sh/kernel/cpu/sh4/swsusp.S
 * -------------------------------------------------------------------------
 * Copyright (C) 2009  STMicroelectronics
 * Author: Francesco M. Virlinzi  <francesco.virlinzi@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License V.2 ONLY.  See linux/COPYING for more information.
 *
 * This may not use any stack, nor any variable that is not "NoSave"
 * ------------------------------------------------------------------------- */

#include <linux/linkage.h>

#define OFF_R8		( 0 )
#define OFF_R9		(4 + OFF_R8  )
#define OFF_R10		(4 + OFF_R9  )
#define OFF_R11		(4 + OFF_R10 )
#define OFF_R12		(4 + OFF_R11 )
#define OFF_R13		(4 + OFF_R12 )
#define OFF_R14		(4 + OFF_R13 )
#define OFF_R15		(4 + OFF_R14 )
#define OFF_PR		(4 + OFF_R15 )
#define OFF_SR		(4 + OFF_PR  )
#define OFF_R6_BK	(4 + OFF_SR  )
#define OFF_R7_BK	(4 + OFF_R6_BK )

.text		;
.balign 32 	;	! to be icache aligned

ENTRY(swsusp_arch_suspend) ;
/*
 *	Here I have to save all the registers
 */
    mov.l 1f, r0
    mov.l r8, @(OFF_R8, r0)
    mov.l r9, @(OFF_R9, r0)
    mov.l r10, @(OFF_R10, r0)
    mov.l r11, @(OFF_R11, r0)
    mov.l r12, @(OFF_R12, r0)
    mov.l r13, @(OFF_R13, r0)
    mov.l r14, @(OFF_R14, r0)
    mov.l r15, @(OFF_R15, r0)
    stc   r6_bank, r1
    mov.l r1, @(OFF_R6_BK, r0)
    stc   r7_bank, r1
    mov.l r1, @(OFF_R7_BK, r0)

    sts   pr, r1
    mov.l r1, @(OFF_PR, r0)
    stc   sr, r1
    mov.l r1, @(OFF_SR, r0)

! Now call the swsusp_save
    mov.l 2f, r0
    jsr   @r0
     nop
    ! Now in r0 there is the returned value of swsusp_save
    mov.l 1f, r4
    ! Reload the right pr
    mov.l @(OFF_PR, r4), r1
    lds   r1, pr
    rts
     nop

.balign 4;
1:.long saved_context_reg;
2:.long swsusp_save;

#define JUMPER()	        bra 201f;	\
				 nop;		\
			200:	bra 200f;	\
				 nop;	;	\
			201:

.balign 32 ;		! icache aligned
ENTRY(swsusp_arch_resume)
    bra 200f 		! start the jump sequence
     nop
really_swsusp_arch_resume:
    mov.l 10f, r6 	! load the restore_pblist
    mov.l @r6, r6
0:  tst r6, r6 		! check if there are other 'pages'(pbe)
    bt/s  20f
     mov #64, r7	! r7 = 64
    shll r7 		! r7 = 128 dcache line(128 * 32) = 4096...

    JUMPER()

    mov.l @(0, r6), r5 	! the source address
    mov.l @(4, r6), r4 	! the destination address
    mov.l @(8, r6), r6 	! the next pbe address
    mov r5, r3
    add #32, r3		! the next icache line source address
1:
    mov.l @r5+, r0 	! 1.
    pref  @r3 		! prefect the next icache line
    movca.l r0, @r4
    mov.l @r5+, r0	! 2.
    mov.l r0,   @(4, r4)
    mov.l @r5+, r0 	! 3.
    JUMPER()

    mov.l r0, @(8, r4)
    mov.l @r5+, r0 	! 4.
    mov.l r0, @(12, r4)
    mov.l @r5+, r0 	! 5.
    mov.l r0, @(16, r4)
    mov.l @r5+, r0 	! 6
    mov.l r0, @(20, r4)

    JUMPER()

    mov.l @r5 +, r0 	! 7
    mov.l r0, @(24, r4)
    mov.l @r5+,  r0 	! 8
    mov.l r0, @(28, r4)
    ocbp @r4
    add  #32,	r4
    add  #32,	r3
    dt   r7
    bf   1b 	! check if the	page is complete

    JUMPER()

    bra 0b
     nop

 20:

    !now restore the previous value...
    mov.l 11f, r0
    mov.l @(OFF_R8, r0), r8
    mov.l @(OFF_R9, r0), r9
    mov.l @(OFF_R10, r0), r10

    JUMPER()

    mov.l @(OFF_R11, r0), r11
    mov.l @(OFF_R12, r0), r12
    mov.l @(OFF_R13, r0), r13
    mov.l @(OFF_R14, r0), r14
    mov.l @(OFF_R15, r0), r15
    mov.l @(OFF_R6_BK, r0), r1
    ldc r1, r6_bank
    mov.l @(OFF_R7_BK, r0), r1
    ldc r1, r7_bank

    JUMPER()

    mov.l 12f, r1
    jsr @r1
     nop
    mov.l 11f, r0
    mov.l @(OFF_PR, r0), r1
    lds r1, pr
    mov.l @(OFF_SR, r0), r1
    ldc r1, sr
    rts
     mov #0, r0

200: bra really_swsusp_arch_resume
      nop

.balign 4			;
10:.long restore_pblist		;
11:.long saved_context_reg	;
12:.long flush_cache_all	;
