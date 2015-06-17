/*
 * include/asm-sh/cpu-sh4/timer.h
 *
 * Copyright (C) 2004 Lineo Solutions, Inc.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#ifndef __ASM_CPU_SH4_TIMER_H
#define __ASM_CPU_SH4_TIMER_H

/*
 * ---------------------------------------------------------------------------
 * TMU Common definitions for SH4 processors
 *	SH7750S/SH7750R
 *	SH7751/SH7751R
 *	SH7760
 *	SH-X3
 * ---------------------------------------------------------------------------
 */
#ifdef CONFIG_CPU_SUBTYPE_SHX3
#define TMU_012_BASE	0xffc10000
#define TMU_345_BASE	0xffc20000
#else
#define TMU_012_BASE	0xffd80000
#define TMU_345_BASE	0xfe100000
#endif

#define TMU_TOCR	TMU_012_BASE	/* Not supported on all CPUs */

#define TMU_012_TSTR	(TMU_012_BASE + 0x04)
#define TMU_345_TSTR	(TMU_345_BASE + 0x04)

#define TMU0_TCOR	(TMU_012_BASE + 0x08)
#define TMU0_TCNT	(TMU_012_BASE + 0x0c)
#define TMU0_TCR	(TMU_012_BASE + 0x10)

#define TMU1_TCOR       (TMU_012_BASE + 0x14)
#define TMU1_TCNT       (TMU_012_BASE + 0x18)
#define TMU1_TCR        (TMU_012_BASE + 0x1c)

#define TMU2_TCOR       (TMU_012_BASE + 0x20)
#define TMU2_TCNT       (TMU_012_BASE + 0x24)
#define TMU2_TCR	(TMU_012_BASE + 0x28)
#define TMU2_TCPR	(TMU_012_BASE + 0x2c)
#define TMU2_IRQ	18

#define TMU3_TCOR	(TMU_345_BASE + 0x08)
#define TMU3_TCNT	(TMU_345_BASE + 0x0c)
#define TMU3_TCR	(TMU_345_BASE + 0x10)

#define TMU4_TCOR	(TMU_345_BASE + 0x14)
#define TMU4_TCNT	(TMU_345_BASE + 0x18)
#define TMU4_TCR	(TMU_345_BASE + 0x1c)

#define TMU5_TCOR	(TMU_345_BASE + 0x20)
#define TMU5_TCNT	(TMU_345_BASE + 0x24)
#define TMU5_TCR	(TMU_345_BASE + 0x28)

#define TMU_TCR_SFT_TPSC	0	/* TMU.TCR[n].TPSC: timer prescaler */
#define TMU_TCR_MSK_TPSC	0x7
#define TMU_TCR_TPSC_PMCDIV4	0x0	/* count clock: Pø/4 */
#define TMU_TCR_TPSC_PMCDIV16	0x1	/* count clock: Pø/16 */
#define TMU_TCR_TPSC_PMCDIV64	0x3	/* count clock: Pø/64 */
#define TMU_TCR_TPSC_PMCDIV1024	0x4	/* count clock: Pø/1024 */
#define TMU_TCR_TPSC_RTC	0x6	/* count clock: on-chip RTC out clk */
#define TMU_TCR_TPSC_EXT	0x7	/* count clock: external clock */

#define TMU_TCR_SFT_UNIE	5	/* TMU.TCR[n].UNIE: underflow IRQ */
#define TMU_TCR_MSK_UNIE	0x1
#define TMU_TCR_UNIE_OFF	0x0	/* underflow interrupt: disabled */
#define TMU_TCR_UNIE_ON		0x1	/* underflow interrupt: enabled */

#endif /* __ASM_CPU_SH4_TIMER_H */
