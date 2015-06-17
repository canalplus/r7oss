/*
 * Driver for
 *    Samsung S5H1420 and
 *    PnpNetwork PN1010 QPSK Demodulator
 *
 * Copyright (C) 2005 Andrew de Quincey <adq_dvb@lidskialf.net>
 * Copyright (C) 2005 Patrick Boettcher <pb@linuxtv.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the
 *
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 675 Mass
 * Ave, Cambridge, MA 02139, USA.
 */
#ifndef S5H1420_PRIV
#define S5H1420_PRIV

#include <asm/types.h>

enum s5h1420_register {
	ID01      = 0x00,
	CON_0     = 0x01,
	CON_1     = 0x02,
	PLL01     = 0x03,
	PLL02     = 0x04,
	QPSK01    = 0x05,
	QPSK02    = 0x06,
	Pre01     = 0x07,
	Post01    = 0x08,
	Loop01    = 0x09,
	Loop02    = 0x0a,
	Loop03    = 0x0b,
	Loop04    = 0x0c,
	Loop05    = 0x0d,
	Pnco01    = 0x0e,
	Pnco02    = 0x0f,
	Pnco03    = 0x10,
	Tnco01    = 0x11,
	Tnco02    = 0x12,
	Tnco03    = 0x13,
	Monitor01 = 0x14,
	Monitor02 = 0x15,
	Monitor03 = 0x16,
	Monitor04 = 0x17,
	Monitor05 = 0x18,
	Monitor06 = 0x19,
	Monitor07 = 0x1a,
	Monitor12 = 0x1f,

	FEC01     = 0x22,
	Soft01    = 0x23,
	Soft02    = 0x24,
	Soft03    = 0x25,
	Soft04    = 0x26,
	Soft05    = 0x27,
	Soft06    = 0x28,
	Vit01     = 0x29,
	Vit02     = 0x2a,
	Vit03     = 0x2b,
	Vit04     = 0x2c,
	Vit05     = 0x2d,
	Vit06     = 0x2e,
	Vit07     = 0x2f,
	Vit08     = 0x30,
	Vit09     = 0x31,
	Vit10     = 0x32,
	Vit11     = 0x33,
	Vit12     = 0x34,
	Sync01    = 0x35,
	Sync02    = 0x36,
	Rs01      = 0x37,
	Mpeg01    = 0x38,
	Mpeg02    = 0x39,
	DiS01     = 0x3a,
	DiS02     = 0x3b,
	DiS03     = 0x3c,
	DiS04     = 0x3d,
	DiS05     = 0x3e,
	DiS06     = 0x3f,
	DiS07     = 0x40,
	DiS08     = 0x41,
	DiS09     = 0x42,
	DiS10     = 0x43,
	DiS11     = 0x44,
	Rf01      = 0x45,
	Err01     = 0x46,
	Err02     = 0x47,
	Err03     = 0x48,
	Err04     = 0x49,
};

#if 0

regsiter mapping follows

/* ID01  */
#define PN1010_ID 7, 0

/* CON_0 */
#define QPSK_ONLY 4, 4
#define SOFT_RST  3, 3 /* System soft reset mode (active high) [1] Enable [0] Disable */
#define CLK_MODE  2, 2
#define BPSK_QPSK 1, 1
#define DSS_DVB   0, 0 /* DSS/DVB mode selection [1] DSS [0] DVB */

/* CON_1 */
#define ADC_SPD   6, 6
#define PLL_OEN   5, 5
#define SER_SEL   4, 4
#define XTAL_OEN  3, 3
#define Q_TEST    2, 2
#define PWR_DN    1, 1 /* Power down mode [1] Power down enable,  [0] Power down disable */
#define I2C_RPT   0, 0 /* I2C repeater control [1] I2C repeater enable,  [0] I2C repeater disable.
					   * Note: The master should be set this bit to "1" in order to interface with the tuner.
					   * When the master is not communicated with the tuner,  this bit should be set to "0" */

/* PLL programming information
 * Fout = ((M+8)*Fin)/((P+2)*2^S),   Fin = 4 MHz */
/* PLL01 */
#define M 7, 0

/* PLL02 */
#define P 5, 0
#define S 7, 6

/* QPSK01 */
#define KICK_EN    7, 7 /* [1] PLL Kicker enable [0] Disable */
#define ROLL_OFF   6, 6
#define SCALE_EN   5, 5
#define DC_EN      4, 4 /* DC offset remove [1] Enable [0] Disable */
#define VMODE      3, 3
#define TGL_MSB    2, 2
#define MODE_QPSK  1, 1 /* QPSK operation mode [1] 1 sampling/1 symbol [0] 2 sampling/1 symbol */
#define Q_SRESET_N 0, 0 /* QPSK start signal [1] Start [0] Idle */

/* QPSK02 */
#define DUMP_ACC   3, 3 /* Dump phase loop filter & timing loop filter accumulator [0 and then 1]
					    * The read operation enabled,  when user set DUMP_ACC "0" and then "1". */
#define DC_WIN     2, 0 /* Window position from MSB (0 <= DC_WIN <= 7) */

/* AGC Controls */
/* Pre01 */
#define PRE_TH    4, 0 /* PRE-AGC threshold */
#define INV_PULSE 7, 7 /* PWM signal is reversed [1] PWM signal active low [0] PWM signal active high */

/* Post01 */
#define RRC_SCALE 7, 6
#define POST_TH   5, 0 /* POST-AGC threshold */

/* Loop Controls */
/* Loop01 */
#define WT_TNCO 7, 7 /* Write TNCO center frequency [0 and then 1] The write operation enabled,  when user set WT_TNCO "0" and then "1" */
#define WT_PNCO 6, 6 /* Write PNCO center frequency [0 and then 1] The write operation enabled,  when user set WT_PNCO "0" and then "1" */

/* Loop02 */
#define LOOP_OUT 7, 7 /* Loop filter monitoring selection [1] Loop filter accumulator + NCO [0] Loop filter accumulator */
#define KICK_VAL 6, 4 /* The value that gets injected into the accumulator when a "kick" is needed. */
#define KICK_MUL 3, 0 /* The number of bits KICK_VAL is up-shifted (2^N) before it is injected into the accumulator. */

/* Loop03 */
#define IGA_PLF 7, 4 /* Phase loop,  integral gain (2^IGA_PLF) in the acquisition mode */
#define PGA_PLF 3, 0 /* Phase loop,  proportional gain (2^PGA_PLF) in the acquisition mode (default +8 added) */

/* Loop04 */
#define IGT_PLF 7, 4 /* Integral gain in the tracking mode */
#define PGT_PLF 3, 0 /* Phase loop,  proportional gain in the tracking mode (default +8 added) */

/* Loop 05 */
#define IG_TLF 7, 4 /* Timing loop,  integral gain */
#define PG_TLF 3, 0 /* Timing loop,  proportional gain (default +8 added) */

/* NCO controls
 * LOOP_OUT [1] Read PLF accumulator + PNCO,  LOOP_OUT [0] Read PLF accumulator */
/* Pnco01 */
#define PNCO0 7, 0
/* Pnco02 */
#define PNCO1 7, 0
/* Pnco03 */
#define PNCO2 7, 0

/* LOOP_OUT [1],  Read TLF accumulator + TNCO,  LOOP_OUT [0],  Read TLF accumulator */
/* Tnco01 */
#define TNCO0 7, 0
/* Tnco02 */
#define TNCO1 7, 0
/* Tnco03 */
#define TNCO2 7, 0

/* Monitor01 */
#define A_SCL 3, 3
#define TLOCK 1, 1 /* Timing loop lock (Symbol sync) [1] Timing loop has locked [0] Timing loop has not locked */
#define PLOCK 0, 0 /* Phase loop lock (Carrier sync) [1] Phase loop has locked [0] Phase loop has not locked */

/* Monitor02 */
#define PRE_LEVEL 7, 0 /* PRE-AGC gain level */

/* Monitor03 */
#define POST_LEVEL 7, 0 /* POST-AGC gain level */

/* Monitor04 */
#define DC_I_LEVEL 7, 0 /* DC offset of I samples */

/* Monitor05 */
#define DC_Q_LEVEL 7, 0 /* DC offset of Q samples */

/* Monitor06 */
#define PLOCK_CNT 7, 0

/* Monitor07 */
#define TLOCK_CNT 7, 0

/* Monitor12 */
#define QPSK_OUT  6, 1 /* QPSK output monitoring */
#define DC_FREEZE 0, 0 /* [1] Do not update DC_OFFSET */

/* FEC Block */
/* FEC01 */
#define DERAND_BPAS   6, 6
#define RS_BPAS       5, 5
#define FEC_SRST_CNTL 4, 4
#define FEC_SRESET    3, 3
#define MPEG_CLK_INTL 2, 0
	/*  Tmp=(FMClk/FSR) x (1 / (2xCR)); FMClk: System Clock Frequency,  FSR: Symbol Rate,  CR: Code Rate
	 *  0: 1 < Tmp <= 2,   4: 13 < Tmp <= 17,
	 *  1: 2 < Tmp <= 5,   5: 17 < Tmp <= 25,
	 *  2: 5 < Tmp <= 9,   6: 25 < Tmp <= 33,
	 *  3: 9 < Tmp <= 13,  7: 33 < Tmp */

/* Soft01 */
#define ST_VAL_R12 2, 0
#define STEP_R12   5, 3
#define STEP_R23   5, 3
#define STEP_R34   5, 3
#define STEP_R56   5, 3
#define STEP_R67   5, 3
#define STEP_R78   5, 3
/* Soft02 */
#define ST_VAL_R23 7, 0
/* Soft03 */
#define ST_VAL_R34 7, 0
/* Soft04 */
#define ST_VAL_R56 7, 0
/* Soft05 */
#define ST_VAL_R67 7, 0
/* Soft06 */
#define ST_VAL_R78 7, 0

/* Vit01 */
#define RENORM_R12 7, 0
/* Vit02 */
#define RENORM_R23 7, 0
/* Vit03 */
#define RENORM_R34 7, 0
/* Vit04 */
#define RENORM_R56 7, 0
/* Vit05 */
#define RENORM_R67 7, 0
/* Vit06 */
#define RENORM_R78 7, 0
/* Vit07 */
#define RENORM_PRD 7, 0

/* Vit08 */
#define VIT_IN_SR78 7, 7
#define VIT_IN_SR34 6, 6
#define VIT_SR78    5, 5
#define VIT_SR67    4, 4
#define VIT_SR56    3, 3
#define VIT_SR34    2, 2
#define VIT_SR23    1, 1 /* .... */
#define VIT_SR12    0, 0 /* Include CR 1/2 in search */

/* Vit09 */
#define PARM_FIX             4, 4 /* Parameter fix mode: [1] Known parameter [0] Unknown parameter */
#define VIT_INV_SPECPARM_FIX 3, 3 /* Initial spectrum information */
#define VIT_FR               2, 0 /* Start synchronization search at code rate as follows */
	#define VIT_CR_12 0x00
	#define VIT_CR_23 0x01
	#define VIT_CR_34 0x02
	#define VIT_CR_56 0x03
	#define VIT_CR_67 0x04
	#define VIT_CR_78 0x05

/* Vit10 */
#define INV_SPEC_STS 3, 3 /* Spectrum information monitoring [1] Inv spectrum [0] Not inv spectrum */
#define VIT_CR       2, 0 /* Viterbi decoder current code rate [0] R=1/2 [1] R=2/3 [2] R=3/4 [3] R=5/6 [4] R=6/7 [5] R=7/8 */

/* Vit11 */
#define VIT_CER 7, 0

/* Vit12 */
#define RENORM_RAT 7, 0

/* Sync01 */
#define SYNC_MISS_TH 7, 4 /* Sync byte detector?s miss threshold */
#define SYNC_HIT_TH  3, 0 /* Sync byte detector?s hit threshold This value should be greater than 2 */

/* Sync02 */
#define BYTE_SYNC     5, 5 /* [1] Acquire byte sync [0] Not acquire byte sync */
#define SYNC_BYTE_STS 4, 2
#define VIT_SRCH_STS  1, 1
#define VIT_SYNC      0, 0 /* [1] Viterbi decoder is in sync [0] Viterbi decoder is out of sync */

/* Rs01 */
#define RS_ERR  3, 0
#define PKT_ERR 4, 4

/* Mpeg01 */
#define ERR_POL   3, 3 /* Packet error polarity [1] Active low [0] Active high */
#define SYNC_POL  2, 2 /* Sync polarity [1] Active low [0] Active high */
#define VALID_POL 1, 1 /* Data valid polarity [1] Active low [0] Active high */
#define CDCLK_POL 0, 0 /* CDCLK polarity [1] Falling edge event [0] Rising edge event */

/* Mpeg02 */
#define MPEG_OEN  6, 6
#define DOUT_CONT 5, 5
#define ERR_CONT  4, 4
#define CLK_CONT  3, 3 /* Clock continuous mode [1] Continuous clock,  [0] Clock is enable during payload data transfer */
#define ENVELOPE  2, 2
#define SER_PAR   1, 1 /* Serial / Parallel mode [1] Serial mode,  [0] Parallel mode */
#define DSS_SYNC  0, 0 /* DSS sync mode [1] Output sync,  [0] No output sync */

/* DiS01 */
#define TONE_FREQ 7, 0 /* Tone frequency ratio ftone = fclk / (TONE_FREQ x 32) */

/* DiS02 */
#define RCV_EN     7, 7 /* DiSEqC receive enable mode [1] Receive enable [0] Receive disable */
#define DIS_LENGTH 6, 4 /* Message length */
#define DIS_RDY    3, 3 /* Data Transfer ready / finish [1] Ready [0] Finish */
#define SWITCH_CON 2, 2 /* Satellite switch in tone burst mode [1] Satellite B [0] Satellite A */
#define LNB_CON    1, 0 /* LNB control mode,  [0] Continuous mode,  [1] Tone burst mode,  [2] DiSEqC mode,  [3] Reserved */

/* DiS03 */
#define OLF_N      2, 2 /* [1] Disable [0] OLF (active low) */
#define LNB_DN     1, 1 /* [1] LNB down [0] Disable (active high) */
#define V18_13V    0, 0 /* 13V/18V select register (0=13V, 1=18V) */

/* DiS04 .. DiS11*/
#define LNB_MESGE0 7, 0
#define LNB_MESGE1 7, 0
#define LNB_MESGE2 7, 0
#define LNB_MESGE3 7, 0
#define LNB_MESGE4 7, 0
#define LNB_MESGE5 7, 0
#define LNB_MESGE6 7, 0
#define LNB_MESGE7 7, 0

/* Rf01 */
#define SLAVE_ADDR 6, 0 /* RF tuner slave Address (SOC VERSION) */

/* Err01 */
#define ALARM_MODE  4, 4
#define ERR_CNT_PRD 3, 2
#define ERR_SRC     1, 0 /* Error monitoring source */
	#define QPSK_BIT_ERRORS 0x0
	#define VITERBI_BIT_ERRORS 0x1
	#define VITERBI_BYTE_ERRORS 0x2
	#define PACKET_ERRORS 0x3

/* Err02 */
#define ERR_CNT_L  7, 0 /* Error counter value register (LSB 8 bits) */

/* Err03 */
#define ERR_CNT_H  7, 0 /* Error counter value register (MSB 8 bits) */

/* Err04 */
#define PARITY_ERR 7, 0 /* Error flag for DiSEqC receive data */

#endif

#endif
