/*
 * -------------------------------------------------------------------------
 * <linux_root>/arch/sh/kernel/cpu/sh4/suspend-core.S
 * -------------------------------------------------------------------------
 * Copyright (C) 2008  STMicroelectronics
 * Author: Francesco M. Virlinzi  <francesco.virlinzi@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License V.2 ONLY.  See linux/COPYING for more information.
 *
 * ------------------------------------------------------------------------- */

#include <linux/linkage.h>
#include <asm/cpu-sh4/mmu_context.h>
#include <asm/cpu-sh4/timer.h>
#include <asm/cpu-sh4/suspend.h>

#if defined(CONFIG_WY_WAKEUP) && (defined(CONFIG_SH_ST_C275) || defined(CONFIG_SH_ST_CUSTOM002012))
# define USE_SUSPEND_EPILOG 1
#endif

#define INT_CODE_INTC_IR	0x11a0
#define INT_CODE_INTC_TMU0	0x0400	/* 14-bit interrupt code from INTEVT */

#define _TMU0_TCR_DEFAULT	(TMU_TCR_TPSC_PMCDIV1024 << TMU_TCR_SFT_TPSC)
#define _TMU0_FREQ		976	/* Pø/1024 =~ 976 Hz */

/*
 * Cells in stx7105_wrt_table[]
 */
#define _TIME_SEC		14	/* duplicates suspend-stx7105.c */
#define _TIME_CNT		15	/* duplicates suspend-stx7105.c */
#define _WAKE_UP_SEC		16	/* duplicates suspend-stx7105.c */

/*
 * Some register are dedicated for special purpose
 */
#define IOREGS_BASE		r14
#define ITABLE_ADDRESS		r13
#define DELAY_REG		r12
#define RTC_CNT			r11
#define RTC_SEC			r10
#define TCNT_LAST		r9
#define WAKE_UP_SEC		r8
#define TCNT_MEM		r6	/* resume phase only */

#define OFFSET_IOBASE		0x0
#define OFFSET_LPJ		0x4
#define OFFSET_DTABLE		0x8
#define OFFSET_DTABLE_SIZE	0xc
#define REG_INSTR		r5	/* init phase only */
#define REG_INSTR_END		r6	/* init phase only */


#define JUMPER()		bra 201f;	\
				 nop;		\
			200:	bra 200f;	\
				 nop;	;	\
			201:

#define JUMP_ISLAND()	200:	bra 200f;	\
				 nop;	;

#undef ENTRY
#define ENTRY(name, align)	\
  .balign align;		\
  .globl name;			\
  name:

.text
ENTRY(sh4_suspend, 32)		! to be icache aligned
	bra 200f		! start the jump sequence
	 nop
sh4_really_suspend:
	mov.l   r14, @-r15
	mov.l   r13, @-r15
	mov.l   r12, @-r15
	mov.l   r11, @-r15
	mov.l   r10, @-r15
	mov.l   r9,  @-r15
	mov.l   r8,  @-r15
	sts.l	pr,  @-r15	! save the pr (we can call other function)
	stc.l	sr,  @-r15
	stc	vbr, r0

	mov.l	r0,  @-r15	! save the original vbr on the stack
	stc	gbr, r0
	mov.l	r0,  @-r15		! save the original gbr on the stack

	mov	#0,  r0			! use null gbr
	ldc	r0,  gbr

	mov.l	@(OFFSET_IOBASE, r4), IOREGS_BASE	! save ioregs address
	mov.l	@(OFFSET_LPJ, r4),    DELAY_REG
	mov	REG_INSTR,  	      ITABLE_ADDRESS	! the instruction table!

#ifdef CONFIG_SH_LPRTC
	mov	#0xffffffff, TCNT_LAST

	!	RTC_SEC = stx7105_wrt_table[_TIME_SEC]
	mov.l	@IOREGS_BASE, r0	! r0 = stx7105_wrt_table
	mov.l	@(_TIME_SEC * 4, r0), RTC_SEC
	!	RTC_CNT = stx7105_wrt_table[_TIME_CNT]
	mov.l	@(_TIME_CNT * 4, r0), RTC_CNT
	!	WAKE_UP_SEC = stx7105_wrt_table[_WAKE_UP_SEC]
	add	#(_WAKE_UP_SEC * 4), r0
	mov.l	@r0, WAKE_UP_SEC

	mov.l	kintevt, r0		! Load the long word there in cache
	mov.l	ktmu_tstr, r0
# ifdef CONFIG_STM_LPFP
	mov.l	ksrlpfp, r0
# endif
#endif
# ifdef CONFIG_STM_LPIR
	mov.l ksrlpir, r0
#endif
#if defined(USE_SUSPEND_EPILOG)
	mov.l ksrlep, r0
#endif

/*
 *	runs the suspend iteration tables
 */
	bsr	do_decode		! Caution RAM and LMI powered down here
	 nop
	JUMPER()
	cmp/eq  #1, r0		! check if we have to sleep or not
	bt	__resume	! it depends if we complete the table
				! with END or END_NO_SLEEP

#ifdef CONFIG_SH_LPRTC
	bsr	get_seconds		! r2 = seconds = RTC_SEC % 60
	 nop
# ifdef CONFIG_STM_LPFP
	!	if (seconds <= 30) then ensure the display is up-to-date
	mov	#30, r1
	cmp/hs	r2, r1			! if r2 <= 30
	bf	2f
	mov.l	ksrlpfp, r0
	mov	RTC_SEC, r4		! r4: unsigned int timestamp
	JUMPER()
	jsr	@r0
	 nop
2:
# endif
	!	nap_sec = 60 - seconds
	mov	#60, r3
	bra	schedule_alarm
	 sub	r2, r3			! r3: nap_sec

	JUMP_ISLAND()

snooze:
	bsr	get_seconds		! r2 = seconds = RTC_SEC % 60
	 nop
	JUMPER()
	!	if (seconds > 30) then nap_sec = 120 - seconds
	!	                  else nap_sec =  60 - seconds
	mov	#60, r3
	mov	#30, r1
	cmp/hi	r1, r2			! if r2 > 30
	bf	2f
	JUMPER()
	shll	r3			! r3 = 120
2:
	sub	r2, r3			! r3: nap_sec

schedule_alarm:
	!	TMU_ticks = nap_sec * frequency of TMU0 counter
	mov	#(_TMU0_FREQ >> 4), r1	! r1 = _TMU0_FREQ
	shll2	r1
	shll2	r1
	mul.l	r1, r3
	sts	macl, r1		! r1: TMU_ticks
	JUMPER()
	!	Prerequisite: TMU0 is stopped, and underflow flag is cleared
	!	TMU.TCNT0 = TMU_ticks
	mov.l	ktmu_tstr, r0
	mov.l	r1, @(TMU0_TCNT - TMU_012_TSTR, r0)
	!	Reconfigure TMU0 rate and enable the underflow interrupt
	mov	#(TMU_TCR_UNIE_ON << TMU_TCR_SFT_UNIE | _TMU0_TCR_DEFAULT), r1
	mov	#(TMU0_TCR - TMU_012_TSTR), r2
	mov.w	r1, @(r0, r2)
	or.b	#0x01, @(r0, gbr)	! _tmu_start(TMU0): TMU.TSTR.STR0 = 1
	JUMPER()
#endif	/* CONFIG_SH_LPRTC */

go_back_to_sleep:
	mova	vbr_base_suspend, r0	! this mova isn't a problem
					! because vbr_base_suspend is
					! 4 bytes alligned
	ldc	r0, vbr			! install the wakeup_interrupt
	mov	#0x3c, r1		! SR.IMASK = 0x0
	shll2	r1
	not	r1,   r1
	stc	sr,   r0
	and	r1,   r0
	ldc	r0,   sr		! enable the interrups

	sleep				! SLEEP!!!

__resume:
	nop
	JUMPER()

#if CONFIG_STM_LPIR
	mov.l	kintevt, r0		! r1 = *INTEVT = 14-bit interrupt code
	mov.l	@r0, r1

	!	r2 = INT_CODE_INTC_IR
	mov.l	irbevent, r2
	cmp/eq	r1, r2
	bf	not_irb
	JUMPER()

	! call sub-routine to check IR key code
	mov.l   ksrlpir, r0
	jsr     @r0
	nop

	! if keycode is valid wakeup otherwise go back to suspend
	cmp/eq  #1, r0
	bt      resume_and_exit

	bra     go_back_to_sleep
	nop
	JUMPER()
not_irb:
#endif
#ifdef CONFIG_SH_LPRTC
	mov.l	ktmu_tstr, r0		! _tmu_stop(TMU0): TMU.TSTR.STR0 = 0
	and.b	#0xfe, @(r0, gbr)
	!	Clear underflow flag and disable interrupt
	mov	#(_TMU0_TCR_DEFAULT), r1
	mov	#(TMU0_TCR - TMU_012_TSTR), r2
	mov.w	r1, @(r0, r2)

	JUMPER()
	mov.l	kintevt, r0		! r1 = *INTEVT = 14-bit interrupt code
	mov.l	@r0, r1
	!	r2 = INT_CODE_INTC_TMU0
	mov	#(INT_CODE_INTC_TMU0 >> 8), r2
	shll8	r2
	cmp/eq	r1, r2			! if r1 != r2 then bra resume_and_exit
	bf	not_rtc
	JUMPER()

	bsr	sample_tmu		! read TMU1, update TCNT_LAST
	 nop
	bsr	add_to_rtc		! add last delta to { RTC_SEC, RTC_CNT }
	 nop

# ifdef CONFIG_STM_LPFP
	!	Now call the sub-routine which updates the display
	mov.l	ksrlpfp, r0
	mov	RTC_SEC, r4		! r4: unsigned int timestamp
	jsr	@r0
	 nop
	JUMPER()
# endif

	!	if RTC_SEC < wake-up time then bra snooze
	cmp/hi	RTC_SEC, WAKE_UP_SEC
	bt	snooze
not_rtc:
#endif

/*
 *	runs the resume instruction tables
 */
resume_and_exit:
#if defined(USE_SUSPEND_EPILOG)
	mov.l	kintevt, r0		! read interrupt event id
	mov.l	@r0, r4			! pass as arg0
	mov.l	ksrlep, r0		! load epilog function address
	jmp	@r0			! jump into LPM epilog code - NO RETURN
	nop
	JUMPER()
#else
	bsr     do_decode		! RAM and LMI powered up here
	 nop
	JUMPER()
	!	Tests show that the following instruction shall be cached too
	mov.l	@r15+, r0
	ldc	r0,    gbr		! Restore the original gbr
	mov.l	@r15+, r0
	ldc	r0,    vbr		! Restore the original vbr
	mov.l	@r15+, r0		! Original sr (on interrupts disabled)
	lds.l	@r15+, pr
	mov.l   @r15+, r8
	mov.l   @r15+, r9
	mov.l   @r15+, r10
	mov.l   @r15+, r11
	mov.l   @r15+, r12
	mov.l   @r15+, r13
	JUMPER()			! Caution: this jump is a long one
	mov.l	@r15+, r14
	mov.l	kintevt, r1
	mov.l   @r1, r1			! who waked up
	ldc	r0, sr			! Restore the original sr
	rts
	 mov	r1, r0			! who waked up
	JUMPER()			! Caution: this jump is a long one
#endif

.balign 4
kintevt:	.long	INTEVT		! Interrupt event register
ktmu_tstr:	.long	TMU_012_TSTR	! TMU timer start register
#ifdef CONFIG_STM_LPFP
ksrlpfp:	.long	lpfp_display_time	! From lowpower_frontpanel.c
#endif
#ifdef CONFIG_STM_LPIR
ksrlpir:	.long	lpir_check_keys	! From lowpower_ir_wakeup.c
irbevent:	.long	INT_CODE_INTC_IR
#endif
#if defined(USE_SUSPEND_EPILOG)
ksrlep:		.long	lpm_epilog
#endif

.balign       	1024,	0,	1024
/* Interrupt handling vector address: VBR + 0x600 */
vbr_base_suspend:
	.long   0
.balign         1024,   0,      1024

	.long 0
.balign         512,	0,	512
wakeup_interrupt:
	JUMPER()
	!	Disable the interrupts in the ssr
	!	and returns to the context (asap)....
	stc	ssr,   r0	! SSR.IMASK = 0xf
	or	#0xf0, r0
	ldc	r0, ssr		! to avoit recursive irq...
				! this means the context will be resumed
				! with interrupt disabled!!!
/*
 * Here we could have a problem (a sleep with interrupt disabled!!!)
 * It could happen if we detect an interrupt between
 * the enabled irq and the sleep!!!
 * Restoring the (raw) spc we will go to execute a sleep with the
 * interrupt disabled !!!!
 * To avoid that in any case we will return on the resume_address
 * label
 */
	mov.l	resume_address, r0
	ldc	r0, spc
	rte
	 nop

200:
/*
 *	load the instruction datas
 */
	mov.l	resume_address, r0	! Load the long word there in cache
#ifdef CONFIG_SH_LPRTC
	mov.l	ktmu1tcnt, r0
	mov.l	kdiv60, r0
	mov.l	kdiv100, r0
	mov.l	kdiv250k, r0
	mov.l	k250k, r0
#endif

	mov	REG_INSTR, r0		/* start address I-table */
	mov	REG_INSTR_END, r1	/* I-table size */
	tst	r1, r1
2:
	mov.l   @r0, r2			/* Load the I-tables in cache */
	add	#32, r0
        bf/s	2b
         dt	r1
/*
 *      load the writeable datas
 */
	mov.l	@(OFFSET_DTABLE, r4), r0
	mov.l	@(OFFSET_DTABLE_SIZE, r4),   r1
	tst	r1, r1
2:
	mov.l   @r0, r2			/* Load the d-tables in cache */
	add	#32, r0
	bf/s	2b
	 dt	r1
	bra	200f
	 nop

.balign 4
resume_address:		.long __resume

#define SRC0		r1
#define SRC1		r2
#define SRC2		r3
#define DEST		r4
#define TMP		r5

.balign 2
	JUMP_ISLAND()

ENTRY(do_decode, 2)
	mova	suspend_jmp_table, r0
	mov	r0, TMP
	mov.l	@ITABLE_ADDRESS+, r0	! opcode
	mov.l	@(r0, TMP), TMP
	jmp	@TMP
	 nop

l_end:	! OP_END
	rts				! Return point
	 mov	#0, r0			! r0 = 0 to say return and sleep

	JUMP_ISLAND()

l_end_no_sleep:	! OP_END_NO_SLEEP
	rts				! Return point
	 mov	#1, r0			! r0 = 1 to say return and Don't sleep

l_source: ! OP_SOURCE
	mov.l	@ITABLE_ADDRESS+, r0	! load the source reg base
	mov.l	@(r0, IOREGS_BASE), TMP	! load ioreg in r5
	mov.l	@ITABLE_ADDRESS+, SRC0	! load the offset
	bra     do_decode
	 add	TMP, SRC0		! r2 = the iomem address of source

	JUMP_ISLAND()

	/* Load a @SRC0 in Dest*/
l_load: ! #OP_LOAD
	bra	do_decode
	 mov.l	@SRC0, DEST		! load the value

	/* Load a value from table in SRC0 */
l_iload_scr0: ! OP_ILOAD_SRC0
	bra	do_decode
	 mov.l	@ITABLE_ADDRESS+, SRC0	! the value is in SRC0 !!!

	/* Load a value from table in SRC1 */
l_iload_src1: ! OP_ILOAD_SRC1
	bra	do_decode
	 mov.l	@ITABLE_ADDRESS+, SRC1	! the value is in SRC1 !!!

	/* Load a value from table in SRC2 */
l_iload_src2: ! OP_ILOAD_SRC2
	bra	do_decode
	 mov.l @ITABLE_ADDRESS+, SRC2	! the value is in SRC2 !!!

	/* Load a value from table in the DEST */
l_iload_dest: ! OP_ILOAD_DEST
	bra	do_decode
	 mov.l @ITABLE_ADDRESS+, DEST

	JUMP_ISLAND()

	/* Store DEST value in @SRC0 */
l_store: ! OP_STORE
	bra	do_decode
	 mov.l DEST, @(0,SRC0)		! store the value

	/* Or operation: DEST |= SRC0 */
l_or:	! OP_OR
	bra	do_decode
	 or	SRC0, DEST

	/* And operation: DEST &= SRC0 */
l_and:	! OP_AND
	bra	do_decode
	 and	SRC0, DEST

	/* Not operation: SRC0 = ~SRC1*/
	/* It's a bit dirty that the NOT operation works on SRC1 instead of DEST or SRC0*/
l_not:	! OP_NOT
	bra	do_decode
	 not	SRC1, SRC0

	JUMP_ISLAND()

	/* While bits equal to value. This operation assumes:
		- SRC0: the iomemory address
		- SRC1: the bitmask
		- SRC2: the result
	*/
l_while_eq: !	OP_WHILE_EQ
	mov.l	@SRC0, TMP
2:	and     SRC1, TMP
	cmp/eq	SRC2, TMP			! (@SRC0 and SRC1) ?!= SRC2)
	bt/s	2b
	 mov.l   @SRC0, TMP
	bra	do_decode
	 nop

	JUMP_ISLAND()

	/* While bits not equal to value. This operation assumes:
		   - SRC0: the iomemory address
		   - SRC1: the bitmask
		   - SRC2: the result
	*/
l_while_neq: ! OP_WHILE_NEQ
	mov.l	@SRC0, TMP
2:	and	SRC1, TMP
	cmp/eq  SRC2, TMP		! (@SRC0 and SRC1) ?== SRC2)
	bf/s	2b
	 mov.l	@SRC0, TMP
	bra	do_decode
	 nop

	JUMP_ISLAND()

	/* Delay operation */
l_delay: ! OP_DELAY
	mov     DELAY_REG, TMP
	tst	TMP, TMP
2:
	bf/s   2b
	 dt	TMP
	bra	do_decode
	 nop

	/*  SCR0 = @SRC0 */
l_load_src0: ! OP_LOAD_SRC0
	mov.l	@SRC0, SRC0
	bra	do_decode
	 nop

	JUMP_ISLAND()

l_load_src1: ! OP_LOAD_SRC1	=> SRC1 = @SRC0
	mov.l  @SRC0, SRC1
	bra	do_decode
	 nop

l_load_src2: ! OP_LOAD_SRC2	=> SRC2 = @SRC0
	mov.l  @SRC0, SRC2
	bra	do_decode
	 nop

#ifdef CONFIG_SH_LPRTC
l_sav_tmu_ls_init: ! OP_SAVE_TMU_LS_INIT
	mov.l	ktmu1tcnt, r0		! TCNT_LAST = TMU1_TCNT
	bra	do_decode
	 mov.l	@r0, TCNT_LAST

	JUMP_ISLAND()

l_sav_tmu_hs: ! OP_SAVE_TMU_HS
	mov	TCNT_LAST, r1		! soustract TCNT_LAST - TMU1_TCNT
	mov.l	ktmu1tcnt, r0
	mov.l	@r0, r2
	sub	r2, r1
	mov.l	kdiv100, r0		! divide by 100
	dmulu.l	r0, r1
	sts	mach, r1
	mov	#-5, r2
	shld	r2, r1
	JUMPER()
	add	r1, RTC_CNT		! then add to RTC_CNT
	mov.l	k250k, r0		! test if RTC_CNT >= 250000
	cmp/hs	r0, RTC_CNT
	bf	2f
	sub	r0, RTC_CNT		! if true: RTC_SEC++; RTC_CNT-=250000
	add	#1, RTC_SEC
2:
	bra	do_decode
	 nop

	JUMP_ISLAND()

l_sav_tmu_ls_exit: ! OP_SAVE_TMU_LS_EXIT
	mov.l	ktmu1tcnt, r0		! TCNT_MEM = TMU1_TCNT
	bra	do_decode
	 mov.l	@r0, TCNT_MEM

	JUMP_ISLAND()

l_close_rtc: ! OP_CLOSE_RTC
	mov.l	ktmutstr, r0		! _tmu_stop(TMU1): TMU.TSTR &= ~0x02
	and.b	#0xfd, @(r0, gbr)
	mov	#0xffffffff, r1		! TMU1_TCNT = 0xffffffff
	mov.l	r1, @(TMU1_TCNT - TMU_012_TSTR, r0)
	or.b	#0x02, @(r0, gbr)	! _tmu_start(TMU1): TMU_TSTR |= 0x02
	sub	TCNT_MEM, TCNT_LAST	! TCNT_LAST - OP_SAVE_TMU_LS_EXIT()
	sts.l	pr, @-r15		! save pr on stack (nested subroutine call)
	bsr	add_to_rtc		! add to { RTC_SEC, RTC_CNT }
	 mov	TCNT_LAST, r1
	lds.l	@r15+, pr		! restore pr from stack
	!	stx7105_wrt_table[_TIME_SEC] = RTC_SEC
	mov.l	@IOREGS_BASE, r0	! r0 = stx7105_wrt_table
	mov.l	RTC_SEC, @(_TIME_SEC * 4, r0)
	!	stx7105_wrt_table[_TIME_CNT] = RTC_CNT
	bra	do_decode
	 mov.l	RTC_CNT, @(_TIME_CNT * 4, r0)

	JUMP_ISLAND()

get_seconds:				! sub-routine(void)
	!	seconds = RTC_SEC % 60
	mov.l	kdiv60, r1		! r2 = RTC_SEC / 60
	dmulu.l	r1, RTC_SEC
	sts	mach, r2
	mov	#-5, r1
	shld	r1, r2
	mov	r2, r3			! r3 = r2 * 60 = (r2 * 16 - r2) * 4
	mov	#4, r1
	shld	r1, r3
	sub	r2, r3
	shll2	r3
	JUMPER()
	mov	RTC_SEC, r2		! r2 = RTC_SEC - r3
	rts
	 sub	r3, r2

sample_tmu:				! sub-routine(void)
	mov	TCNT_LAST, r1
	mov.l	ktmu1tcnt, r2		! TCNT_LAST = TMU1_TCNT
	mov.l	@r2, TCNT_LAST
	rts				! return the time delta in r1
	 sub	TCNT_LAST, r1

	JUMP_ISLAND()

add_to_rtc:				! sub-routine(r1 delta)
	add	r1, RTC_CNT		! add to RTC_CNT
	mov.l	kdiv250k, r1		! RTC_SEC += RTC_CNT / 250000
	dmulu.l	r1, RTC_CNT
	sts	mach, r1
	shlr16	r1
	add	r1, RTC_SEC
	mov.l	k250k, r2		! RTC_CNT %= 250000
	mul.l	r2, r1
	sts	macl, r1
	rts
	 sub	r1, RTC_CNT

	JUMP_ISLAND()

.balign 4
ktmutstr:	.long	TMU_012_TSTR	! TMU timer start register
ktmu1tcnt:	.long	TMU1_TCNT	! TMU timer counter 1
kdiv60:		.long	-2004318071	! constant for /60 division
kdiv100:	.long	1374389535	! constant for /100 division
kdiv250k:	.long	1125899907	! constant for /250000 division
k250k:		.long	250000

.balign 2
#endif	/* CONFIG_SH_LPRTC */

200:	! Preload the jump table
	mova	suspend_jmp_table, r0
	mov	r0, r1
	mov	#_OPCODE_TABLE_SIZE_, r0
	cmp/eq	#0, r0
load_jtable:
	mov.l	@r1, r2
	add	#32, r1
	bf/s	load_jtable
	 dt	r0

	bra sh4_really_suspend		! Now we jump on sh4_really_suspend
	 nop				! to really suspend (and resume... ;-)

.balign 32
suspend_jmp_table:
.long l_end, l_end_no_sleep, l_source, l_load, l_iload_scr0, l_iload_src1
.long l_iload_src2, l_iload_dest, l_store, l_or, l_and, l_not, l_while_eq
.long l_while_neq, l_delay, l_load_src0, l_load_src1, l_load_src2
#ifdef CONFIG_SH_LPRTC
.long l_sav_tmu_hs, l_sav_tmu_ls_init, l_sav_tmu_ls_exit, l_close_rtc
#endif
