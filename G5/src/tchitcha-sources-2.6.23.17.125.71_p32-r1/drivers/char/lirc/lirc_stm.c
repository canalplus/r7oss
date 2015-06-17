/*
 * LIRC plugin for the STMicroelectronics IRDA devices
 *
 * Copyright (C) 2004-2008 STMicroelectronics
 *
 * June 2004:  first implementation for a 2.4 Linux kernel
 *             Giuseppe Cavallaro  <peppe.cavallaro@st.com>
 * Marc 2005:  review to support pure raw mode and to adapt to Linux 2.6
 *             Giuseppe Cavallaro  <peppe.cavallaro@st.com>
 * June 2005:  Change to a MODE2 receive driver and made into a generic
 *             ST driver.
 *             Carl Shaw <carl.shaw@st.com>
 * July 2005:  fix STB7100 MODE2 implementation and improve performance
 *             of STm8000 version. <carl.shaw@st.com>
 * Aug  2005:  Added clock autoconfiguration support.  Fixed module exit code.
 * 	       Added UHF support (kernel build only).
 * 	       Carl Shaw <carl.shaw@st.com>
 * Sep  2005:  Added first transmit support
 *             Added ability to set rxpolarity register
 * 	       Angelo Castello <angelo.castello@st.com>
 * 	       and Carl Shaw <carl.shaw@st.com>
 * Oct  2005:  Added 7100 transmit
 *             Added carrier width configuration
 * 	       Carl Shaw <carl.shaw@st.com>
 * Sept 2006:  Update:
 * 		fix timing issues (bugzilla 764)
 * 		Thomas Betker <thomas.betker@siemens.com>
 * 		allocate PIO pins in driver
 * 		update transmit
 * 		improve fault handling on init
 * 		Carl Shaw <carl.shaw@st.com>
 * Oct  2007:  Added both lirc-0.8.2 common interface and integrated out IRB driver  
 *             to be working for linux-2.6.23-rc7. Removed old platform support...
 *             Sti5528 STb8000. Added new IR rx intq mechanism to reduce the amount 
 *             intq needed to identify one button. Fix TX transmission loop setting up 
 *             correctly the irq clean register.
 * 	       Angelo Castello <angelo.castello@st.com>
 * Nov  2007:  Moved here all the platform
 *             dependences leaving clear of this task the common interface. (lirc_dev.c)
 *	       Code cleaning and optimization.
 * 	       Angelo Castello <angelo.castello@st.com>
 * Dec  2007:  Added device resource management support.
 * 	       Angelo Castello <angelo.castello@st.com>
 * Mar  2008:  Fix UHF support and general tidy up
 *             Carl Shaw <carl.shaw@st.com>
 * Mar  2008:  Fix insmod/rmmod actions. Added new PIO allocate mechanism
 *	       based on platform PIOs dependencies values (LIRC_PIO_ON,
 *	       LIRC_IR_RX, LIRC_UHF_RX so on )
 * 	       Angelo Castello <angelo.castello@st.com>
 * Apr  2008:  Added SCD support
 * 	       Angelo Castello <angelo.castello@st.com>
 *	       Carl Shaw <carl.shaw@st.com>
 * Feb  2009:  Added PM capability
 * 	       Angelo Castello <angelo.castello@st.com>
 *	       Francesco Virlinzi <francesco.virlinzi@st.com>
 * Mar	2010:  Fixed the following SCD issues:
 * 	       Q1: to manage rightly border-line symbols
 * 	       Q2: to add others constrains for safe initialization.
 * 	       Q3: to manage potential contiguous Pulse/Space when the SCD
 * 	           programming is not aligned to leading/trailing edge
 * 	       Q4: to rebuild rightly the incoming signals train filtered by SCD
 * 	           when detected from SCD_CODE or from SCD_ALT_CODE.
 * 	       Angelo Castello <angelo.castello at st.com>
 *
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <asm/uaccess.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/stm/pio.h>
#include <linux/stm/clk.h>
#include <linux/stm/soc.h>
#include <linux/time.h>
#include <linux/lirc.h>
#include "lirc_dev.h"

#define LIRC_STM_NAME "lirc_stm"

/* General debugging */
#ifdef CONFIG_LIRC_STM_DEBUG
#define DPRINTK(fmt, args...) printk(KERN_INFO LIRC_STM_NAME ": %s: " fmt, __FUNCTION__ , ## args)
#else
#define DPRINTK(fmt, args...)
#endif

/*
 * Infra Red: hardware register map
 */
#if defined(CONFIG_LIRC_STM_UHF_RX)
static int uhf_switch = 1;
#else
static int uhf_switch = 0;
#endif
static int ir_or_uhf_offset;
static void *irb_base_address;	/* IR block register base address */

/* IR transmitter registers */
#define IRB_TX_REG(x)		(irb_base_address + x)
#define IRB_TX_PRESCALAR	IRB_TX_REG(0x00)	/* clock prescalar       */
#define IRB_TX_SUBCARRIER	IRB_TX_REG(0x04)	/* subcarrier frequency  */
#define IRB_TX_SYMPERIOD	IRB_TX_REG(0x08)	/* symbol period         */
#define IRB_TX_ONTIME		IRB_TX_REG(0x0c)	/* symbol pulse time     */
#define IRB_TX_INT_ENABLE	IRB_TX_REG(0x10)	/* TX irq enable         */
#define IRB_TX_INT_STATUS	IRB_TX_REG(0x14)	/* TX irq status         */
#define IRB_TX_ENABLE		IRB_TX_REG(0x18)	/* TX enable             */
#define IRB_TX_INT_CLEAR	IRB_TX_REG(0x1c)	/* TX interrupt clear    */
#define IRB_TX_SUBCARRIER_WIDTH	IRB_TX_REG(0x20)	/* subcarrier freq width */
#define IRB_TX_STATUS		IRB_TX_REG(0x24)	/* TX status */

#define TX_INT_PENDING			0x01
#define TX_INT_UNDERRUN			0x02

#define TX_FIFO_DEPTH			7
#define TX_FIFO_USED			((readl(IRB_TX_STATUS) >> 8) & 0x07)

/* IR receiver registers */
#define IRB_RX_REG(x)		(irb_base_address + x + ir_or_uhf_offset)
#define IRB_RX_ON	    	IRB_RX_REG(0x40)	/* RX pulse time capture */
#define IRB_RX_SYS          	IRB_RX_REG(0X44)	/* RX sym period capture */
#define IRB_RX_INT_EN	    	IRB_RX_REG(0x48)	/* RX IRQ enable (R/W)   */
#define IRB_RX_INT_STATUS      	IRB_RX_REG(0x4C)	/* RX IRQ status (R/W)   */
#define IRB_RX_EN	    	IRB_RX_REG(0x50)	/* Receive enable (R/W)  */
#define IRB_MAX_SYM_PERIOD  	IRB_RX_REG(0x54)	/* end of sym. max value */
#define IRB_RX_INT_CLEAR 	IRB_RX_REG(0x58)	/* overrun status (W)    */
#define IRB_RX_STATUS	    	IRB_RX_REG(0x6C)	/* receive status        */
#define IRB_RX_NOISE_SUPPR  	IRB_RX_REG(0x5C)	/* noise suppression     */
#define IRB_RX_POLARITY_INV 	IRB_RX_REG(0x68)	/* polarity inverter     */

/* IRB and UHF common registers */
#define IRB_CM_REG(x)		(irb_base_address + x)
#define IRB_RX_RATE_COMMON   	IRB_CM_REG(0x64)	/* sample frequency divisor */
#define IRB_RX_CLOCK_SEL  	IRB_CM_REG(0x70)	/* clock select (LP mode)   */
#define IRB_RX_CLOCK_SEL_STATUS IRB_CM_REG(0x74)	/* clock selection status   */
#define IRB_RX_NOISE_SUPP_WIDTH IRB_CM_REG(0x9C)

/* SCD registers */
#define IRB_SCD_CFG		IRB_CM_REG(0x200)	/* SCD config */
#define IRB_SCD_STA		IRB_CM_REG(0x204)	/* SCD status */
#define IRB_SCD_CODE		IRB_CM_REG(0x208)	/* SCD code to be detected */
#define IRB_SCD_CODE_LEN	IRB_CM_REG(0x20c)	/* SCD num code symbols */
#define IRB_SCD_SYMB_MIN_TIME	IRB_CM_REG(0x210)	/* SCD min symbol time */
#define IRB_SCD_SYMB_MAX_TIME	IRB_CM_REG(0x214)	/* SCD max symbol time */
#define IRB_SCD_SYMB_NOM_TIME	IRB_CM_REG(0x218)	/* SCD nominal symbol time */
#define IRB_SCD_PRESCALAR	IRB_CM_REG(0x21c)	/* SCD prescalar */
#define IRB_SCD_INT_EN		IRB_CM_REG(0x220)	/* SCD interrupt enable */
#define IRB_SCD_INT_CLR		IRB_CM_REG(0x224)	/* SCD interrupt clear */
#define IRB_SCD_INT_STA		IRB_CM_REG(0x22c)	/* SCD interrupt status */
#define IRB_SCD_NOISE_RECOV	IRB_CM_REG(0x228)	/* SCD noise recovery */
#define IRB_SCD_ALT_CODE	IRB_CM_REG(0x230)	/* SCD alternative start code */

#define RX_CLEAR_IRQ(x) 		writel((x), IRB_RX_INT_CLEAR)
#define RX_WORDS_IN_FIFO() 		(readl(IRB_RX_STATUS) & 0xff00)

#define LIRC_STM_MINOR			0
#define LIRC_STM_MAX_SYMBOLS		100
#define LIRC_STM_BUFSIZE		(LIRC_STM_MAX_SYMBOLS*sizeof(lirc_t))

#define LIRC_STM_SCD_MAX_SYMBOLS	32
#define LIRC_STM_SCD_BUFSIZE		((LIRC_STM_SCD_MAX_SYMBOLS/2+1)*sizeof(lirc_t))
#define LIRC_STM_SCD_TOLERANCE		25
/* rx.scd_flags fields:
 *	scd normal			1 -> bit 0
 *	scd altenative			1 -> bit 1
 *	scd normal = alternative	1 -> bit 2
 *	scd enabled			1 -> bit 3
 */
#define SCD_NORMAL		0x01
#define SCD_ALTERNATIVE		0x02
#define SCD_NOR_EQ_ALT		0x04
#define SCD_ENABLED		0x08
#define SCD_ALT_MASK		(SCD_NORMAL|SCD_ALTERNATIVE|SCD_ENABLED)

/* Bit settings */
#define LIRC_STM_IS_OVERRUN	 	0x04
#define LIRC_STM_CLEAR_IRQ	 	0x38
#define LIRC_STM_CLEAR_OVERRUN	 	0x04
/* IRQ set: Enable full FIFO                 1  -> bit  3;
 *          Enable overrun IRQ               1  -> bit  2;
 *          Enable last symbol IRQ           1  -> bit  1:
 *          Enable RX interrupt              1  -> bit  0;
 */
#define LIRC_STM_ENABLE_IRQ		0x0f

/* RX graphical example to better understand the difference between ST IR block
 * output and standard definition used by LIRC (and most of the world!)
 *
 *           mark                                     mark
 *      |-IRB_RX_ON-|                            |-IRB_RX_ON-|
 *      ___  ___  ___                            ___  ___  ___             _
 *      | |  | |  | |                            | |  | |  | |             |
 *      | |  | |  | |         space 0            | |  | |  | |   space 1   |
 * _____| |__| |__| |____________________________| |__| |__| |_____________|
 *
 *      |--------------- IRB_RX_SYS -------------|------ IRB_RX_SYS -------|
 *
 *      |------------- encoding bit 0 -----------|---- encoding bit 1 -----|
 *
 * ST hardware returns mark (IRB_RX_ON) and total symbol time (IRB_RX_SYS), so
 * convert to standard mark/space we have to calculate space=(IRB_RX_SYS - mark)
 * The mark time represents the amount of time the carrier (usually 36-40kHz)
 * is detected.
 *
 * TX is the same but with opposite calculation.
 *
 * The above examples shows Pulse Width Modulation encoding where bit 0 is
 * represented by space>mark.
 */

/* Start Code Detect (SCD) graphical example to understand how to configure
 * propely the code, code length and nominal time values based on Remote Control
 * signals train example.
 *
 *      __________________        ________      _______  ___  ___  ___
 *      |                |        |      |      |     |  | |  | |  | |
 *      |                |        |      |      |     |  | |  | |  | |
 * _____|                |________|      |______|     |__| |__| |__| |___......
 *
 *      |---- 1000us ----|- 429 -|- 521 -|- 500-|....
 *      |-- 500 -|- 500 -|- 500 -|- 500 -|  units in us for SCD code.
 *      |--  1  -|-  1  -|-  0  -|-  1  -|  SCD code Ob1101.
 *
 * The nominal symbol duration is 500us, code length 4 and code Ob1101.
 */

/* SOC dependent section - these values are set in the appropriate 
 * arch/sh/kernel/cpu/sh4/setup-* files and
 * transfered when the lirc device is opened
 */

typedef struct lirc_stm_tx_data_s {
	wait_queue_head_t waitq;
	/* timing fine control */
	unsigned int mult;
	unsigned int div;
	unsigned int carrier_freq;
	/* transmit buffer */
	lirc_t *wbuf;
	unsigned int off_wbuf;
} lirc_stm_tx_data_t;

typedef struct lirc_stm_rx_data_s {
	/* timing fine control */
	int symbol_mult;
	int symbol_div;
	int pulse_mult;
	int pulse_div;
	/* data configuration */
	unsigned int sampling_freq_div;
	lirc_t *rbuf;
	unsigned int off_rbuf;
	unsigned int sumUs;
	int error;
	struct timeval sync;
	/* SCD support */
	short scd_flags;
	lirc_t *rscd_code;
	lirc_t *rscd_altcode;
	unsigned int off_rscd_code;
	unsigned int off_rscd_altcode;
	struct timeval last_written;
} lirc_stm_rx_data_t;

typedef struct lirc_stm_plugin_data_s {
	int open_count;
	struct plat_lirc_data *p_lirc_d;
#ifdef CONFIG_PM
	pm_message_t prev_state;
#endif
} lirc_stm_plugin_data_t;

static lirc_stm_plugin_data_t pd;	/* IR data config */
static lirc_stm_rx_data_t rx;	/* RX data config */
static lirc_stm_tx_data_t tx;	/* TX data config */

/* LIRC subsytem symbol buffer. managed only via common lirc routines
 * user process read symbols from here  */
struct lirc_buffer lirc_stm_rbuf;

static inline void lirc_stm_reset_rx_data(void)
{
	rx.error = 0;
	rx.off_rbuf = 0;
	rx.sumUs = 0;
	memset(rx.rbuf, 0, LIRC_STM_BUFSIZE);
}

static inline unsigned int lirc_stm_time_to_cycles(unsigned int microsecondtime)
{
	/* convert a microsecond time to the nearest number of subcarrier clock
	 * cycles
	 */
	microsecondtime *= tx.mult;
	microsecondtime /= tx.div;
	return (microsecondtime * tx.carrier_freq / 1000000);
}

static void lirc_stm_tx_interrupt(int irq, void *dev_id)
{
	unsigned int symbol, mark, done = 0;
	unsigned int tx_irq_status = readl(IRB_TX_INT_STATUS);

	if ((tx_irq_status & TX_INT_PENDING) != TX_INT_PENDING)
		return;

	while (done == 0) {
		if (unlikely((readl(IRB_TX_INT_STATUS) & TX_INT_UNDERRUN) ==
			     TX_INT_UNDERRUN)) {
			/* There has been an underrun - clear flag, switch
			 * off transmitter and signal possible exit
			 */
			printk(KERN_ERR LIRC_STM_NAME ": transmit underrun!\n");
			writel(0x02, IRB_TX_INT_CLEAR);
			writel(0x00, IRB_TX_INT_ENABLE);
			writel(0x00, IRB_TX_ENABLE);
			done = 1;
			DPRINTK("disabled TX\n");
			wake_up_interruptible(&tx.waitq);
		} else {
			int fifoslots = TX_FIFO_USED;

			while (fifoslots < TX_FIFO_DEPTH) {
				mark = tx.wbuf[(tx.off_wbuf * 2)];
				symbol = mark + tx.wbuf[(tx.off_wbuf * 2) + 1];
				DPRINTK("TX raw m %d s %d ", mark, symbol);

				mark = lirc_stm_time_to_cycles(mark) + 1;
				symbol = lirc_stm_time_to_cycles(symbol) + 2;
				DPRINTK("cal m %d s %d\n", mark, symbol);

				if ((tx.wbuf[(tx.off_wbuf * 2)] == 0xFFFF) ||
				    (tx.wbuf[(tx.off_wbuf * 2) + 1] == 0xFFFF))
				{
					/* Dump out last symbol */
					writel(mark * 2, IRB_TX_SYMPERIOD);
					writel(mark, IRB_TX_ONTIME);

					DPRINTK("TX end m %d s %d\n",
						mark, mark * 2);

					/* flush transmit fifo */
					while (TX_FIFO_USED != 0) {
					};
					writel(0, IRB_TX_SYMPERIOD);
					writel(0, IRB_TX_ONTIME);
					/* spin until TX fifo empty */
					while (TX_FIFO_USED != 0) {
					};
					/* disable tx interrupts and
					 * transmitter */
					writel(0x07, IRB_TX_INT_CLEAR);
					writel(0x00, IRB_TX_INT_ENABLE);
					writel(0x00, IRB_TX_ENABLE);
					DPRINTK("TX disabled\n");
					tx.off_wbuf = 0;
					fifoslots = 999;
					done = 1;
				} else {
					writel(symbol, IRB_TX_SYMPERIOD);
					writel(mark, IRB_TX_ONTIME);

					DPRINTK("Nm %d s %d\n", mark, symbol);

					tx.off_wbuf++;
					fifoslots = TX_FIFO_USED;
				}
			}
		}
	}
}

static void lirc_stm_prescd_symbols(void)
{
	lirc_t *lscd;
	unsigned int *offscd;

	DPRINTK("SCD_STA(0x%x)\n", rx.scd_flags & 0x3);
	/* Here need to take care the following SCD constrain:
	 * If there is only one start code to be detected, the registers for
	 * normal and alternative start codes has been programmed with
	 * identical values. The low significate bits of scd_flags means:
	 * 	1    : SCD_ENABLED
	 * 	 1   : SCD_NOR_EQ_ALT
	 * 	  1  : SCD_ALTERNATIVE
	 * 	   1 : SCD_NORMAL
	 *	11xx : normal and alternative are the same due to constrain.
	 *	1001 : normal detected
	 *	1011 : alternative detected
	 */
	if ((rx.scd_flags & SCD_NOR_EQ_ALT) ||
	    (rx.scd_flags ^ SCD_ALT_MASK)) {
		/* normal code detected */
		lscd = rx.rscd_code;
		offscd = &rx.off_rscd_code;
	} else {
		/* alternative code detected */
		lscd = rx.rscd_altcode;
		offscd = &rx.off_rscd_altcode;
	}

	/* manage potential contiguous Pulse/Space when the SCD programming
	 * is not aligned to leading/trailing edge
	*/
	if ((lscd[*offscd-1] & PULSE_BIT) == (rx.rbuf[0] & PULSE_BIT)) {
		rx.rbuf[0] += (lscd[*offscd-1] & ~PULSE_BIT);
		(*offscd)--;
	}

	lirc_buffer_write_n(&lirc_stm_rbuf, (unsigned char *)lscd, *offscd);
	return;
}

static void lirc_stm_rx_interrupt(int irq, void *dev_id)
{
	unsigned int symbol, mark = 0;
	int lastSymbol = 0, clear_irq = 1;
#ifdef CONFIG_LIRC_LCD_NOISE_RUWIDO
	int sum = 0;
#endif

	if (rx.scd_flags) {
		/* SCD status catched only the first time */
		if (!(rx.scd_flags & SCD_NORMAL)) {
			rx.scd_flags |= readb(IRB_SCD_STA);
			writel(0x01, IRB_SCD_INT_CLR);
			writel(0x00, IRB_SCD_INT_EN);
		}
	}

	while (RX_WORDS_IN_FIFO()) {
		/* discard the entire collection in case of errors!  */
		if (unlikely(readl(IRB_RX_INT_STATUS) & LIRC_STM_IS_OVERRUN)) {
			printk(KERN_INFO LIRC_STM_NAME ": IR RX overrun\n");
			writel(LIRC_STM_CLEAR_OVERRUN, IRB_RX_INT_CLEAR);
			rx.error = 1;
		}

		/* get the symbol times from FIFO */
		symbol = (readl(IRB_RX_SYS));
		mark = (readl(IRB_RX_ON));

		if (clear_irq) {
			/*  Clear the interrupt
			 * and leave only the overrun irq enabled */
			RX_CLEAR_IRQ(LIRC_STM_CLEAR_IRQ);
			writel(0x07, IRB_RX_INT_EN);
			clear_irq = 0;
		}

#ifdef CONFIG_LIRC_LCD_NOISE_RUWIDO
		/* let's count the total period of the symbols
		 * read in this interrupt, but we don't take into
		 * account noise at the beginning of a frame
		 */
		if (rx.off_rbuf > 0)
			sum += symbol;

		if (symbol != 0xFFFF) {
			/* invalid symbols (< 2*250) or marks
			 * (<250 or >2*250) should be considered
			 * as noise and ignored */
			if ((symbol < 400) || (mark < 200) ||
				(mark > 650))
				continue;

			/* with lcd noise, timeout is different
			 * than 0xFFFF but usually higher than
			 * 1200 (higher than valid period for a
			 * normal symbol = 4 * 250)
			 */
			if (symbol >= 1200) {
				if (rx.rbuf == 0)
					continue;
				else
					symbol = 0xFFFF;
			}
		}
#endif

		if (rx.off_rbuf >= LIRC_STM_MAX_SYMBOLS) {
			printk(KERN_INFO LIRC_STM_NAME
			       ": IR too many symbols (max %d)\n",
			       LIRC_STM_MAX_SYMBOLS);
			rx.error = 1;
		}

		/* now handle the data depending on error condition */
		if (rx.error) {
			/*  Try again */
			lirc_stm_reset_rx_data();
			continue;
		}

		if (symbol == 0xFFFF)
			lastSymbol = 1;
		else
			lastSymbol = 0;

		/* A sequence seems to start with a constant time symbol (1us)
		 * pulse and symbol time length, both of 1us. We ignore this.
		 */
		if ((mark > 2) && (symbol > 1)) {
			/* Make fine adjustments to timings */
			symbol -= mark;	/* to get space timing */
			symbol *= rx.symbol_mult;
			symbol /= rx.symbol_div;
			mark *= rx.pulse_mult;
			mark /= rx.pulse_div;

			/* The ST hardware returns the pulse time and the
			 * period, which is the pulse time + space time, so
			 * we need to subtract the pulse time from the period
			 * to get the space time.
			 * For a pulse in LIRC MODE2, we need to set the
			 * PULSE_BIT ON
			 */
			rx.rbuf[(rx.off_rbuf * 2)] = mark | PULSE_BIT;
			rx.rbuf[(rx.off_rbuf * 2) + 1] = symbol;
			rx.sumUs += mark + symbol;
			rx.off_rbuf++;

			if (lastSymbol) {
				/* move the entire collection into user
				 * buffer if enough space, drop otherwise
				 * (perhaps too crude a recovery?)
				 */
				if (likely(lirc_buffer_available
					   (&lirc_stm_rbuf) >=
					   ((2 * rx.off_rbuf) +
						LIRC_STM_SCD_MAX_SYMBOLS))) {
					struct timeval now;
					lirc_t syncSpace;

					DPRINTK("W symbols = %d\n",
						rx.off_rbuf);

					/*  Calculate and write the leading
					 *  space. All spaces and pulses
					 *  together sum up to the
					 *  microseconds elapsed since we
					 *  sent the previous block of data
					 */

					do_gettimeofday(&now);
					if (now.tv_sec - rx.sync.tv_sec < 0)
						syncSpace = 0;
					else if (now.tv_sec -
						 rx.sync.tv_sec >
						 PULSE_MASK / 1000000)
						syncSpace = PULSE_MASK;
					else {
						syncSpace =
						    (now.tv_sec -
						     rx.sync.tv_sec) *
						    1000000 +
						    (now.tv_usec -
						     rx.sync.tv_usec);
						syncSpace -=
						    (rx.sumUs -
						     rx.
						     rbuf[((rx.
							    off_rbuf -
							    1) * 2) + 1]);
						if (syncSpace < 0)
							syncSpace = 0;
						else if (syncSpace > PULSE_MASK)
							syncSpace = PULSE_MASK;
					}

					lirc_buffer_write_1
					    (&lirc_stm_rbuf, (unsigned char *)
					     &syncSpace);
					rx.sync = now;

					/* Now write the SCD filtered-out
					 * pulse / space pairs
					 */
					if (rx.scd_flags)
						lirc_stm_prescd_symbols();

					/*  Now write the pulse / space pairs
					 *  EXCEPT FOR THE LAST SPACE
					 *  The last space value should be
					 *  0xFFFF to denote a timeout
					 */
					lirc_buffer_write_n
					    (&lirc_stm_rbuf,
					     (unsigned char *)rx.rbuf,
					     (2 * rx.off_rbuf) - 1);
					wake_up_interruptible
					    (&lirc_stm_rbuf.wait_poll);
				} else
					printk(KERN_ERR LIRC_STM_NAME
					       ": not enough space "
					       "in user buffer\n");
				lirc_stm_reset_rx_data();
			}
		}
	}			/* while */

#ifdef CONFIG_LIRC_LCD_NOISE_RUWIDO
	/* if we timed out but did not receive a 0xFFFF then
	 * the last symbol written to rbuf should be considered as
	 * the terminating one and the buffer should be written now.*/
	if ((!lastSymbol) && (sum >= 12000) && (rx.off_rbuf > 0)) {
		rx.rbuf[rx.off_rbuf * 2] = 0xFFFF;

		lirc_buffer_write_n
		(&lirc_stm_rbuf,
		(unsigned char *)rx.rbuf,
		(2 * rx.off_rbuf) - 1);

		lirc_stm_reset_rx_data();
	}
#endif

	RX_CLEAR_IRQ(LIRC_STM_CLEAR_IRQ | 0x02);
	writel(LIRC_STM_ENABLE_IRQ, IRB_RX_INT_EN);

	if (rx.scd_flags && lastSymbol) {
		/* reset the SCD flags */
		rx.scd_flags &= (SCD_ENABLED | SCD_NOR_EQ_ALT);
		writel(0x01, IRB_SCD_INT_EN);
		writel(0x01, IRB_SCD_CFG);
	}
}

static irqreturn_t lirc_stm_interrupt(int irq, void *dev_id)
{
	lirc_stm_tx_interrupt(irq, dev_id);
	lirc_stm_rx_interrupt(irq, dev_id);

	return IRQ_HANDLED;
}

static int lirc_stm_scd_set(char enable)
{
	if (!rx.scd_flags)
		return -1;

	if (enable) {
		writel(0x01, IRB_SCD_INT_EN);
		writel(0x01, IRB_SCD_INT_CLR);
		writel(0x01, IRB_SCD_CFG);
	} else {
		writel(0x00, IRB_SCD_INT_EN);
		writel(0x00, IRB_SCD_CFG);
	}
	DPRINTK("SCD %s\n", (enable ? "enabled" : "disabled"));
	return 0;
}

static int lirc_stm_scd_config(lirc_scd_t * scd, unsigned long clk)
{
	unsigned int nrec, ival, scwidth;
	unsigned int prescalar, tolerance;
	unsigned int space, mark;
	int i, j, k;
	lirc_t *lscd;
	unsigned int *offscd;
	unsigned int code, codelen, alt_codelen, curr_codelen;

	rx.scd_flags = 0;
	rx.off_rscd_code = 0;
	rx.off_rscd_altcode = 0;
        if (!uhf_switch) {
            printk(KERN_ERR LIRC_STM_NAME
                   ": SCD not available in IR-RX mode. Not armed\n");
            return -ENOTSUPP;
        }
        if (!(scd)) {
            printk(KERN_ERR LIRC_STM_NAME
                   ": SCD bad configuration. Not armed\n");
            return -EIO;
        }

	codelen = fls(scd->code);
	if ((scd->code == 0) ||
	    (codelen > LIRC_STM_SCD_MAX_SYMBOLS) ||
	    (codelen < 1)) {
		printk(KERN_ERR LIRC_STM_NAME
			": SCD invalid start code. Not armed\n");
		return -EINVAL;
	}

	alt_codelen = fls(scd->alt_code);
	if (alt_codelen > 0) {
		if ((scd->alt_code == 0) ||
		    (alt_codelen > LIRC_STM_SCD_MAX_SYMBOLS) ||
		    (alt_codelen < (codelen / 2))) {
			printk(KERN_ERR LIRC_STM_NAME
			": SCD invalid alternative code. Not armed\n");
			return -EINVAL;
		}
	}

	/* SCD disable */
	writel(0x00, IRB_SCD_CFG);

	/* Configure pre-scalar clock first to give 1MHz sampling */
	prescalar = clk / 1000000;
	writel(prescalar, IRB_SCD_PRESCALAR);

	/* pre-loading of not filtered SCD codes and
	 * preparing data for tolerance calculation.
	 */
	lscd = rx.rscd_code;
	offscd = &rx.off_rscd_code;
	code = scd->code;
	curr_codelen = codelen;
	for (k = 0; k < 2; k++) {
		space = 0;
		mark = 0;
		j = 0;
		for (i = (curr_codelen - 1); i >= 0; i--) {
			j = 1 << (i - 1);
			if (code & (1 << i)) {
				mark += scd->nomtime;
				if (!(code & j) || (i == 0)) {
					lscd[*offscd] = mark | PULSE_BIT;
					DPRINTK("SCD mark rscd[%d](%d)\n",
						*offscd,
						lscd[*offscd] & ~PULSE_BIT);
					(*offscd)++;
					mark = 0;
				}
			} else {
				space += scd->nomtime;
				if ((code & j) || (i == 0)) {
					lscd[*offscd] = space;
					DPRINTK("SCD space rscd[%d](%d)\n",
						*offscd, lscd[*offscd]);
					(*offscd)++;
					space = 0;
				}
			}
		}
		lscd = rx.rscd_altcode;
		offscd = &rx.off_rscd_altcode;
		code = scd->alt_code;
		curr_codelen = alt_codelen;
	}

	/* normaly 20% of nomtine is enough as tolerance */
	tolerance = scd->nomtime * LIRC_STM_SCD_TOLERANCE / 100;

	DPRINTK("SCD prescalar %d nominal %d tolerance %d\n",
		prescalar, scd->nomtime, tolerance);

	/* Sanity check to garantee all hw constrains must be satisfied */
	if ((tolerance > ((scd->nomtime >> 1) - scd->nomtime)) ||
	    (scd->nomtime < ((scd->nomtime >> 1) + tolerance))) {
		tolerance = scd->nomtime * LIRC_STM_SCD_TOLERANCE / 100;
		DPRINTK("SCD tolerance out of range. default %d\n", tolerance);
	}
	if (tolerance < 4) {
		tolerance = scd->nomtime * LIRC_STM_SCD_TOLERANCE / 100;
		DPRINTK("SCD tolerance too close. default %d\n", tolerance);
	}

	/* Program in scd codes and lengths */
	writel(scd->code, IRB_SCD_CODE);

	/* Some cuts of chips have broken SCD, so check... */
	i = readl(IRB_SCD_CODE);
	if (i != scd->code) {
		printk(KERN_ERR LIRC_STM_NAME
		       ": SCD hardware fault.  Broken silicon?\n");
		printk(KERN_ERR LIRC_STM_NAME
		       ": SCD wrote code 0x%08x read 0x%08x. Not armed\n",
			scd->code, i);
		return -ENODEV;
	}

	/* Program scd min time, max time and nominal time */
	writel(scd->nomtime - tolerance, IRB_SCD_SYMB_MIN_TIME);
	writel(scd->nomtime, IRB_SCD_SYMB_NOM_TIME);
	writel(scd->nomtime + tolerance, IRB_SCD_SYMB_MAX_TIME);

	/* Program in noise recovery (if required) */
	if (scd->noiserecov) {
		nrec = 1 | (1 << 16);	/* primary and alt code enable */

		i = 1 << (codelen - 1);
		ival = scd->code & i;
		if (ival)
			nrec |= 2;

		scwidth = 0;
		while (i > 0 && ((scd->code & i) == ival)) {
			scwidth++;
			i >>= 1;
		}

		nrec |= (scwidth << 8);

		i = 1 << (alt_codelen - 1);
		ival = scd->alt_code & i;
		if (ival)
			nrec |= 1 << 17;

		scwidth = 0;
		while (i > 0 && ((scd->alt_code & i) == ival)) {
			scwidth++;
			i >>= 1;
		}

		nrec |= (scwidth << 24);

		DPRINTK("SCD noise recovery 0x%08x\n", nrec);
		writel(nrec, IRB_SCD_NOISE_RECOV);
	}

	/* Set supported flag */
        printk(KERN_INFO LIRC_STM_NAME
		": SCD normal code 0x%x codelen 0x%x nomtime 0x%x armed.\n",
		scd->code, codelen, scd->nomtime);

	if (alt_codelen > 0) {
		printk(KERN_INFO LIRC_STM_NAME
			": SCD alternative code 0x%x codelen 0x%x armed.\n",
		scd->alt_code, alt_codelen);
		writel(scd->alt_code, IRB_SCD_ALT_CODE);
	} else {
		writel(scd->code, IRB_SCD_ALT_CODE);
		alt_codelen = codelen;
		rx.scd_flags |= SCD_NOR_EQ_ALT;
	}

	/* SCD codelen range [00001,...,11111,00000] where 00000 = 32 symbols */
	writel(((alt_codelen & 0x1f) << 8)
	       | (codelen & 0x1f), IRB_SCD_CODE_LEN);

	rx.scd_flags |= SCD_ENABLED;
	return 0;
}

static int lirc_stm_open_inc(void *data)
{
	lirc_stm_plugin_data_t *lpd = (lirc_stm_plugin_data_t *) data;
	DPRINTK("entering open\n");

	if (lpd->open_count++ == 0) {
		unsigned long flags;
		DPRINTK("plugin enabled\n");
		local_irq_save(flags);

		/* enable interrupts and receiver */
		writel(LIRC_STM_ENABLE_IRQ, IRB_RX_INT_EN);
		writel(0x01, IRB_RX_EN);
		lirc_stm_reset_rx_data();
		local_irq_restore(flags);

		if (rx.scd_flags & SCD_ENABLED)
			return lirc_stm_scd_set(1);
	} else
		DPRINTK("plugin already open\n");

	return 0;
}

static void lirc_stm_flush_rx(void)
{
	if (rx.scd_flags)
		lirc_stm_scd_set(0);
	/* Disable receiver */
	writel(0x00, IRB_RX_EN);
	/* Disable interrupt */
	writel(0x20, IRB_RX_INT_EN);
	/* clean the buffer */
	lirc_stm_reset_rx_data();
}

/*
** Called by lirc_dev as a last action on a real close
*/
static void lirc_stm_close_dec(void *data)
{
	lirc_stm_plugin_data_t *lpd = (lirc_stm_plugin_data_t *) data;
	DPRINTK("entering close\n");

	/* The last close disable the receiver */
	if (--lpd->open_count == 0)
		lirc_stm_flush_rx();
}

static int lirc_stm_ioctl(struct inode *node, struct file *filep,
			  unsigned int cmd, unsigned long arg)
{
	int retval = 0;
	unsigned long value = 0;
	lirc_scd_t scd;
	char *msg = "";

	switch (cmd) {
	case LIRC_GET_FEATURES:
		/*
		 * Our driver can receive in mode2 and send in pulse mode.
		 * TODO: We can generate our own carrier freq
		 *      (LIRC_CAN_SET_SEND_CARRIER) and also change duty
		 *      cycle (LIRC_CAN_SET_SEND_DUTY_CYCLE)
		 */
		DPRINTK("LIRC_GET_FEATURES return REC_MODE2|SEND_PULSE\n");
		retval =
		    put_user(LIRC_CAN_REC_MODE2 | LIRC_CAN_SEND_PULSE,
			     (unsigned long *)arg);
		break;

	case LIRC_GET_REC_MODE:
		DPRINTK("LIRC_GET_REC_MODE return LIRC_MODE_MODE2\n");
		retval = put_user(LIRC_MODE_MODE2, (unsigned long *)arg);
		break;

	case LIRC_SET_REC_MODE:
		retval = get_user(value, (unsigned long *)arg);
		DPRINTK("LIRC_SET_REC_MODE to 0x%lx\n", value);
		if (value != LIRC_MODE_MODE2)
			retval = -ENOSYS;
		break;

	case LIRC_GET_SEND_MODE:
		DPRINTK("LIRC_GET_SEND_MODE return LIRC_MODE_PULSE\n");
		retval = put_user(LIRC_MODE_PULSE, (unsigned long *)arg);
		break;

	case LIRC_SET_SEND_MODE:
		retval = get_user(value, (unsigned long *)arg);
		DPRINTK("LIRC_SET_SEND_MODE to 0x%lx\n", value);
		/* only LIRC_MODE_PULSE supported */
		if (value != LIRC_MODE_PULSE)
			return (-ENOSYS);
		break;

	case LIRC_SCD_CONFIGURE:
		if (copy_from_user(&scd, arg, sizeof(scd)))
			return -EFAULT;

		retval = lirc_stm_scd_config(&scd, 
			clk_get_rate(clk_get(NULL, "comms_clk")));
		break;

	case LIRC_SCD_ENABLE:
	case LIRC_SCD_DISABLE:
		retval = lirc_stm_scd_set(cmd == LIRC_SCD_ENABLE);
		break;

	case LIRC_SCD_STATUS:
		retval = put_user((rx.scd_flags & SCD_ENABLED),
				(unsigned long *)arg);
		break;

	case LIRC_GET_REC_RESOLUTION:
		msg = "LIRC_GET_REC_RESOLUTION";
		goto _not_supported;

	case LIRC_GET_REC_CARRIER:
		msg = "LIRC_GET_REC_CARRIER";
		goto _not_supported;

	case LIRC_SET_REC_CARRIER:
		msg = "LIRC_SET_REC_CARRIER";
		goto _not_supported;

	case LIRC_GET_SEND_CARRIER:
		msg = "LIRC_GET_SEND_CARRIER";
		goto _not_supported;

	case LIRC_SET_SEND_CARRIER:
		msg = "LIRC_SET_SEND_CARRIER";
		goto _not_supported;

	case LIRC_GET_REC_DUTY_CYCLE:
		msg = "LIRC_GET_REC_DUTY_CYCLE";
		goto _not_supported;

	case LIRC_SET_REC_DUTY_CYCLE:
		msg = "LIRC_SET_REC_DUTY_CYCLE";
		goto _not_supported;

	case LIRC_GET_SEND_DUTY_CYCLE:
		msg = "LIRC_GET_SEND_DUTY_CYCLE";
		goto _not_supported;

	case LIRC_SET_SEND_DUTY_CYCLE:
		msg = "LIRC_SET_SEND_DUTY_CYCLE";
		goto _not_supported;

	case LIRC_GET_LENGTH:
		msg = "LIRC_GET_LENGTH";
		goto _not_supported;

	default:
		msg = "???";
	       _not_supported:
		DPRINTK("command %s (0x%x) not supported\n", msg, cmd);
		retval = -ENOIOCTLCMD;
	}

	return retval;
}

static ssize_t lirc_stm_write(struct file *file, const char *buf,
			      size_t n, loff_t * ppos)
{
	int i;
	size_t rdn = n / sizeof(size_t);
	unsigned int symbol, mark;
	int fifosyms, num_pio_pins;
	struct lirc_pio *p;

	num_pio_pins = pd.p_lirc_d->num_pio_pins;
	while (num_pio_pins > 0) {
		p = &(pd.p_lirc_d->pio_pin_arr[num_pio_pins - 1]);
		if (!(p->pinof ^ (LIRC_IR_TX | LIRC_PIO_ON)))
			break;
		else
			num_pio_pins--;
	}
	if (!num_pio_pins) {
		printk(KERN_ERR LIRC_STM_NAME
		       ": write operation unsupported.\n");
		return -ENOTSUPP;
	}

	if (n % sizeof(lirc_t))
		return -EINVAL;

	if (tx.off_wbuf != 0 && (file->f_flags & O_NONBLOCK))
		return -EAGAIN;

	/* Wait for transmit to become free... */
	if (wait_event_interruptible(tx.waitq, tx.off_wbuf == 0))
		return -ERESTARTSYS;

	/* Prevent against buffer overflow... */
	if (rdn > LIRC_STM_MAX_SYMBOLS)
		rdn = LIRC_STM_MAX_SYMBOLS;

	n -= rdn * sizeof(size_t);

	if (copy_from_user((char *)tx.wbuf, buf, rdn * sizeof(size_t))) {
		return -EFAULT;
	}

	if (n == 0)
		tx.wbuf[rdn - 1] = 0xFFFF;

	/* load the first words into the FIFO */
	fifosyms = rdn;

	if (fifosyms > TX_FIFO_DEPTH)
		fifosyms = TX_FIFO_DEPTH;

	for (i = 0; i < fifosyms; i++) {
		mark = tx.wbuf[(i * 2)];
		symbol = mark + tx.wbuf[(i * 2) + 1];
		DPRINTK("TX raw m %d s %d ", mark, symbol);

		mark = lirc_stm_time_to_cycles(mark) + 1;
		symbol = lirc_stm_time_to_cycles(symbol) + 2;
		DPRINTK("cal m %d s %d\n", mark, symbol);

		tx.off_wbuf++;
		writel(mark, IRB_TX_ONTIME);
		writel(symbol, IRB_TX_SYMPERIOD);
	}

	/* enable the transmit */
	writel(0x07, IRB_TX_INT_ENABLE);
	writel(0x01, IRB_TX_ENABLE);
	DPRINTK("TX enabled\n");

	return n;
}

static void lirc_stm_calc_tx_clocks(unsigned int clockfreq,
				    unsigned int carrierfreq,
				    unsigned int subwidthpercent)
{
	/*  We know the system base clock and the required IR carrier frequency
	 *  We now want a divisor of the system base clock that gives the nearest
	 *  integer multiple of the carrier frequency
	 */

	const unsigned int clkratio = clockfreq / carrierfreq;
	unsigned int scalar, n;
	int delta;
	unsigned int diffbest = clockfreq, nbest = 0, scalarbest = 0;
	unsigned int nmin = clkratio / 255;

	if ((nmin & 0x01) == 1)
		nmin++;

	for (n = nmin; n < clkratio; n += 2) {
		scalar = clkratio / n;
		if ((scalar & 0x01) == 0 && scalar != 0) {
			delta = clockfreq - (scalar * carrierfreq * n);
			if (delta < 0)
				delta *= -1;

			if (delta < diffbest) {	/* better set of parameters ? */
				diffbest = delta;
				nbest = n;
				scalarbest = scalar;
			}
			if (delta == 0)	/* an exact multiple */
				break;
		}
	}

	scalarbest /= 2;
	nbest *= 2;

	DPRINTK("TX clock scalar = %d\n", scalarbest);
	DPRINTK("TX subcarrier scalar = %d\n", nbest);

	/*  Set the registers now  */
	writel(scalarbest, IRB_TX_PRESCALAR);
	writel(nbest, IRB_TX_SUBCARRIER);
	writel(nbest * subwidthpercent / 100, IRB_TX_SUBCARRIER_WIDTH);

	/*  Now calculate timing to subcarrier cycles factors which compensate
	 *  for any remaining difference between our clock ratios and real times
	 *  in microseconds
	 */

	if (diffbest == 0) {
		/* no adjustment required - our clock is running at the required
		 * speed */
		tx.mult = 1;
		tx.div = 1;
	} else {
		/* adjustment is required */
		delta = scalarbest * carrierfreq * nbest;
		tx.mult = delta / (clockfreq / 10000);

		if (delta < clockfreq) {	/* our clock is running too fast */
			DPRINTK("clock running slow at %d\n", delta);
			tx.div = tx.mult;
			tx.mult = 10000;
		} else {	/* our clock is running too slow */

			DPRINTK("clock running fast at %d\n", delta);
			tx.div = 10000;
		}
	}

	DPRINTK("TX fine adjustment mult = %d\n", tx.mult);
	DPRINTK("TX fine adjustment div  = %d\n", tx.div);
}

static void
lirc_stm_calc_rx_clocks(struct platform_device *pdev, unsigned long baseclock, unsigned int irbrxmaxperiod)
{
	struct plat_lirc_data *lirc_private_data = NULL;
	unsigned int rx_max_symbol_per;

	lirc_private_data = (struct plat_lirc_data *)pdev->dev.platform_data;

	if (lirc_private_data->irbclkdiv == 0) {
		/* Auto-calculate clock divisor */
		int freqdiff;
		rx.sampling_freq_div = baseclock / 10000000;
		/* Work out the timing adjustment factors */
		freqdiff = baseclock - (rx.sampling_freq_div * 10000000);
		/* freqdiff contains the difference between our clock and a
		 * true 10 MHz clock which the IR block wants
		 */
		if (freqdiff == 0) {
			/* no adjustment required - our clock is running at the
			 * required speed
			 */
			rx.symbol_mult = 1;
			rx.pulse_mult = 1;
			rx.symbol_div = 1;
			rx.pulse_div = 1;
		} else {
			/* adjustment is required */
			rx.symbol_mult =
			    baseclock / (10000 * rx.sampling_freq_div);

			if (freqdiff > 0) {
				/* our clock is running too fast */
				rx.pulse_mult = 1000;
				rx.pulse_div = rx.symbol_mult;
				rx.symbol_mult = rx.pulse_mult;
				rx.symbol_div = rx.pulse_div;
			} else {
				/* our clock is running too slow */
				rx.symbol_div = 1000;
				rx.pulse_mult = rx.symbol_mult;
				rx.pulse_div = 1000;
			}

		}

	} else {
		rx.sampling_freq_div = (lirc_private_data->irbclkdiv);
		rx.symbol_mult = (lirc_private_data->irbperiodmult);
		rx.symbol_div = (lirc_private_data->irbperioddiv);
	}

	writel(rx.sampling_freq_div, IRB_RX_RATE_COMMON);
	DPRINTK("IR clock is %lu\n", baseclock);
	DPRINTK("IR clock divisor is %d\n", rx.sampling_freq_div);
	DPRINTK("IR clock divisor readlack is %d\n", readl(IRB_RX_RATE_COMMON));
	DPRINTK("IR period mult factor is %d\n", rx.symbol_mult);
	DPRINTK("IR period divisor factor is %d\n", rx.symbol_div);
	DPRINTK("IR pulse mult factor is %d\n", rx.pulse_mult);
	DPRINTK("IR pulse divisor factor is %d\n", rx.pulse_div);
	/* maximum symbol period.
	 * Symbol periods longer than this will generate
	 * an interrupt and terminate a command
	 */
	if (irbrxmaxperiod != 0)
		rx_max_symbol_per =
		    irbrxmaxperiod *
		    rx.symbol_mult / rx.symbol_div;
	else
		rx_max_symbol_per = 0;

	DPRINTK("RX Maximum symbol period register 0x%x\n", rx_max_symbol_per);
	writel(rx_max_symbol_per, IRB_MAX_SYM_PERIOD);
}

static int lirc_stm_hardware_init(struct platform_device *pdev)
{
	struct plat_lirc_data *lirc_private_data = NULL;
	struct clk *clk;
	unsigned int scwidth;
	int baseclock;

	/*  set up the hardware version dependent setup parameters */
	lirc_private_data = (struct plat_lirc_data *)pdev->dev.platform_data;

	tx.carrier_freq = 38000;	// in Hz

	/* Set the polarity inversion bit to the correct state */
	writel(lirc_private_data->rxpolarity, IRB_RX_POLARITY_INV);

	/*  Get or calculate the clock and timing adjustment values.
	 *  We can auto-calculate these in some cases
	 */
	if (lirc_private_data->irbclock == 0) {
		clk = clk_get(NULL, "comms_clk");
		baseclock = clk_get_rate(clk) / lirc_private_data->sysclkdiv;
	} else
		baseclock = lirc_private_data->irbclock;

	lirc_stm_calc_rx_clocks(pdev, baseclock, lirc_private_data->irbrxmaxperiod);
	/*  Set up the transmit timings  */
	if (lirc_private_data->subcarrwidth != 0)
		scwidth = lirc_private_data->subcarrwidth;
	else
		scwidth = 50;

	if (scwidth > 100)
		scwidth = 50;

	DPRINTK("subcarrier width set to %d %%\n", scwidth);
	lirc_stm_calc_tx_clocks(baseclock, tx.carrier_freq, scwidth);

	lirc_stm_scd_config(lirc_private_data->scd_info, baseclock);

	return 0;
}

static int lirc_stm_remove(struct platform_device *pdev)
{
	DPRINTK("lirc_stm_remove called\n");
	return 0;
}

static int lirc_stm_probe(struct platform_device *pdev)
{
	int ret = -EINVAL;
	int num_pio_pins;
	struct lirc_pio *p;
	struct device *dev = &pdev->dev;
	struct resource *res;
	int irb_irq = 0;

	if (pdev->name == NULL) {
		printk(KERN_ERR LIRC_STM_NAME
		       ": probe failed. Check kernel SoC config.\n");
		return -ENODEV;
	}

	printk(KERN_INFO LIRC_STM_NAME
	       ": probe found data for platform device %s\n", pdev->name);
	pd.p_lirc_d = (struct plat_lirc_data *)pdev->dev.platform_data;

	if ((irb_irq = platform_get_irq(pdev, 0)) == 0) {
		printk(KERN_ERR LIRC_STM_NAME
		       ": IRQ configuration not found\n");
		return -ENODEV;
	}

	DPRINTK("IRB irq is %d\n", irb_irq);

	if (devm_request_irq(dev, irb_irq, lirc_stm_interrupt, IRQF_DISABLED,
			     LIRC_STM_NAME, (void *)&pd) < 0) {
		printk(KERN_ERR LIRC_STM_NAME ": IRQ register failed\n");
		return -EIO;
	}

	/* Enable wakeup interrupt if any */
	irb_irq = platform_get_irq(pdev, 1);
	if (irb_irq > 0) {
		DPRINTK("IRB wake-up irq is %d\n", irb_irq);
		if (devm_request_irq(dev, irb_irq, lirc_stm_interrupt, IRQF_DISABLED,
					LIRC_STM_NAME, (void *)&pd) < 0) {
			printk(KERN_ERR LIRC_STM_NAME ": IRQ register failed\n");
			return -EIO;
			}
		disable_irq(irb_irq);
		enable_irq_wake(irb_irq);
		}
	/* Configure for ir or uhf. uhf_switch==1 is UHF */
	if (uhf_switch)
		ir_or_uhf_offset = 0x40;
	else
		ir_or_uhf_offset = 0x00;

	/* Hardware IR block setup - the PIO ports should already be set up
	 * in the board-dependent configuration.  We need to remap the
	 * IR registers into kernel space - we do this in one chunk
	 */

	if ((rx.rbuf = (lirc_t *) devm_kzalloc(dev,
					       LIRC_STM_BUFSIZE,
					       GFP_KERNEL)) == NULL)
		return -ENOMEM;

	if ((rx.rscd_code = (lirc_t *) devm_kzalloc(dev,
						    LIRC_STM_SCD_BUFSIZE,
						    GFP_KERNEL)) == NULL)
		return -ENOMEM;

	if ((rx.rscd_altcode = (lirc_t *) devm_kzalloc(dev,
						       LIRC_STM_SCD_BUFSIZE,
						       GFP_KERNEL)) == NULL)
		return -ENOMEM;

	if ((tx.wbuf = (lirc_t *) devm_kzalloc(dev,
					       LIRC_STM_BUFSIZE,
					       GFP_KERNEL)) == NULL)
		return -ENOMEM;

	memset(rx.rbuf, 0, LIRC_STM_BUFSIZE);
	memset(rx.rscd_code, 0, LIRC_STM_SCD_BUFSIZE);
	memset(rx.rscd_altcode, 0, LIRC_STM_SCD_BUFSIZE);
	memset(tx.wbuf, 0, LIRC_STM_BUFSIZE);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		printk(KERN_ERR LIRC_STM_NAME ": IO MEM not found\n");
		return -ENODEV;
	}

	if (!devm_request_mem_region(dev, res->start,
				     res->end - res->start, LIRC_STM_NAME)) {
		printk(KERN_ERR LIRC_STM_NAME ": request_mem_region failed\n");
		return -EBUSY;
	}

	irb_base_address =
	    devm_ioremap_nocache(dev, res->start, res->end - res->start);

	if (irb_base_address == NULL) {
		printk(KERN_ERR LIRC_STM_NAME ": ioremap failed\n");
		ret = -ENOMEM;
	} else {
		DPRINTK("ioremapped register block at 0x%x\n", res->start);
		DPRINTK("ioremapped to 0x%x\n", (unsigned int)irb_base_address);

		printk(KERN_INFO LIRC_STM_NAME
		       ": STM LIRC plugin has IRQ %d", irb_irq);

		/* Allocate the PIO pins */
		num_pio_pins = pd.p_lirc_d->num_pio_pins;
		while (num_pio_pins > 0) {
			ret = 0;
			p = &(pd.p_lirc_d->pio_pin_arr[num_pio_pins - 1]);
			/* Can I satisfy the IR-RX request ? */
			if ((ir_or_uhf_offset == 0x00)
			    && (p->pinof & LIRC_IR_RX)) {
				if (p->pinof & LIRC_PIO_ON)
					printk(" using IR-RX mode\n");
				else {
					printk
					    (" with IR-RX mode unsupported\n");
					ret = -1;
				}
			}

			/* Can I satisfy the UHF-RX request ? */
			if ((ir_or_uhf_offset == 0x40)
			    && (p->pinof & LIRC_UHF_RX)) {
				if (p->pinof & LIRC_PIO_ON)
					printk(" using UHF-RX mode\n");
				else {
					printk
					    (" with UHF-RX mode unsupported\n");
					ret = -1;
				}
			}

			/* Try to satisfy the request */
			if ((!ret) && (p->pinof & LIRC_PIO_ON))
				if (!(p->pinaddr = stpio_request_pin(p->bank,
								     p->pin,
								     LIRC_STM_NAME,
								     p->dir))) {
					printk(KERN_ERR
					       "\nlirc_stm: PIO[%d,%d] request failed\n",
					       p->bank, p->pin);
					ret = -1;

				}

			/* something bad is happened */
			if (ret) {
				while (num_pio_pins < pd.p_lirc_d->num_pio_pins) {
					stpio_free_pin(pd.p_lirc_d->
						       pio_pin_arr
						       [num_pio_pins].pinaddr);
					pd.p_lirc_d->pio_pin_arr[num_pio_pins].
					    pinaddr = NULL;
					num_pio_pins++;
				}
				return -EIO;
			}

			num_pio_pins--;
		}

		/* reset and then harware initialisation */
		init_waitqueue_head(&tx.waitq);
		/* enable signal detection */
		ret = lirc_stm_hardware_init(pdev);
	}
	return ret;
}

#ifdef CONFIG_PM
static void lirc_stm_restore_rx(void)
{
	if (rx.scd_flags)
		lirc_stm_scd_set(1);
	/* enable interrupts and receiver */
	writel(LIRC_STM_ENABLE_IRQ, IRB_RX_INT_EN);
	writel(0x01, IRB_RX_EN);
	/* clean the buffer */
	lirc_stm_reset_rx_data();
}

static int lirc_stm_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct plat_lirc_data *lirc_private_data = NULL;
	lirc_private_data = (struct plat_lirc_data *)pdev->dev.platform_data;
	pd.prev_state = state;
	switch (state.event) {
	case PM_EVENT_SUSPEND:
		if (device_may_wakeup(&(pdev->dev))) {
			/* need for the resuming phase */
                        lirc_stm_flush_rx();
			lirc_stm_calc_rx_clocks(pdev, lirc_private_data->clk_on_low_power,
				lirc_private_data->maxperiod_on_low_power);
			lirc_stm_scd_config(lirc_private_data->scd_info,
				lirc_private_data->clk_on_low_power);
			lirc_stm_restore_rx();
#if defined(CONFIG_SH_ST_CUSTOM002007) || \
	defined(CONFIG_SH_ST_CUSTOM002012) || \
	defined(CONFIG_SH_ST_C275) || \
	defined(CONFIG_SH_ST_CUSTOM003019)
			DPRINTK("SET IRB_RX_NOISE_SUPPR\n");
			writel(3,IRB_RX_NOISE_SUPPR);
#endif
			writel(0x02, IRB_SCD_CFG);
			writel(0x01, IRB_SCD_CFG);
			return 0;
		}
	case PM_EVENT_FREEZE:
		/* disable IR RX/TX interrupts plus clear status*/
		writel(0x00, IRB_RX_EN);
		writel(0xff, IRB_RX_INT_CLEAR);
		writel(0x00, IRB_TX_ENABLE);
		writel(0x1e, IRB_TX_INT_CLEAR);
		/* disabling LIRC irq request */
		/* flush LIRC plugin data */
		lirc_stm_reset_rx_data();
		break;
	}

	return 0;
}

static int lirc_stm_resume(struct platform_device *pdev)
{
	switch (pd.prev_state.event) {
	case PM_EVENT_FREEZE:
		lirc_stm_hardware_init(pdev);
		/* there was I really open device ? */
		if (pd.open_count > 0) {
			/* enable interrupts and receiver */
			writel(LIRC_STM_ENABLE_IRQ, IRB_RX_INT_EN);
			writel(0x01, IRB_RX_EN);
		}
		break;
	case PM_EVENT_SUSPEND:
		lirc_stm_hardware_init(pdev);
		lirc_stm_restore_rx();
#if defined(CONFIG_SH_ST_CUSTOM002007) || \
	defined(CONFIG_SH_ST_CUSTOM002012) || \
	defined(CONFIG_SH_ST_C275) || \
	defined(CONFIG_SH_ST_CUSTOM003019)
		DPRINTK("DISABLE IRB_RX_NOISE_SUPPR\n");
		writel(0,IRB_RX_NOISE_SUPPR);
#endif
		writel(0x02, IRB_SCD_CFG);
		writel(0x01, IRB_SCD_CFG);
		break;
	}
	pd.prev_state = PMSG_ON;
	return 0;
}
#else
#define lirc_stm_suspend	NULL
#define lirc_stm_resume		NULL
#endif

static struct platform_driver lirc_device_driver = {
	.driver.name = "lirc",
	.probe = lirc_stm_probe,
	.remove = lirc_stm_remove,
	.suspend = lirc_stm_suspend,
	.resume = lirc_stm_resume,
};

static struct file_operations lirc_stm_fops = {
      write:lirc_stm_write,
};

static struct lirc_plugin lirc_stm_plugin = {
	.name = LIRC_STM_NAME,
	.minor = LIRC_STM_MINOR,
	.code_length = 1,
	.sample_rate = 0,
	/* plugin can receive raw pulse and space timings for each symbol */
	.features = LIRC_CAN_REC_MODE2,
	/* plugin private data  */
	.data = (void *)&pd,
	/* buffer handled by upper layer */
	.add_to_buf = NULL,
	.get_queue = NULL,
	.set_use_inc = lirc_stm_open_inc,
	.set_use_dec = lirc_stm_close_dec,
	.ioctl = lirc_stm_ioctl,
	.fops = &lirc_stm_fops,
	.rbuf = &lirc_stm_rbuf,
	.owner = THIS_MODULE,
};

static int __init lirc_stm_init(void)
{
	DPRINTK("initializing the IR receiver...\n");

	if (platform_driver_register(&lirc_device_driver)) {
		printk(KERN_ERR LIRC_STM_NAME
		       ": platform driver register failed\n");
		goto out_err;
	}

        if (!pd.p_lirc_d) {
                printk(KERN_ERR LIRC_STM_NAME 
                       ": missed out hardware probing. Check kernel SoC config.\n");
                goto out_err;
        }

	/* inform the top level driver that we use our own user buffer */
	if (lirc_buffer_init(&lirc_stm_rbuf, sizeof(lirc_t),
			     (2 * LIRC_STM_MAX_SYMBOLS))) {
		printk(KERN_ERR LIRC_STM_NAME ": buffer init failed\n");
                platform_driver_unregister(&lirc_device_driver);
		goto out_err;
	}

	request_module("lirc_dev");
	if (lirc_register_plugin(&lirc_stm_plugin) < 0) {
		printk(KERN_ERR LIRC_STM_NAME ": plugin registration failed\n");
		lirc_buffer_free(&lirc_stm_rbuf);
                platform_driver_unregister(&lirc_device_driver);
		goto out_err;
	}

	printk(KERN_INFO
	       "STMicroelectronics LIRC driver initialized.\n");
	return 0;
out_err:
	printk(KERN_ERR
	       "STMicroelectronics LIRC driver not initialized.\n");
	return -EINVAL;
}

void __exit lirc_stm_release(void)
{
	int num_pio_pins;

	DPRINTK("removing STM lirc plugin\n");

	/* unregister the plugin */
	lirc_stm_flush_rx();
	platform_driver_unregister(&lirc_device_driver);

	/* unplug the lirc stm driver */
	if (lirc_unregister_plugin(LIRC_STM_MINOR) < 0)
		printk(KERN_ERR LIRC_STM_NAME ": plugin unregister failed\n");
	/* free buffer */
	lirc_buffer_free(&lirc_stm_rbuf);

	/* deallocate the STPIO pins */
	num_pio_pins = pd.p_lirc_d->num_pio_pins;
	while (num_pio_pins > 0)
		if (pd.p_lirc_d->pio_pin_arr[num_pio_pins - 1].pinaddr)
			stpio_free_pin(pd.p_lirc_d->pio_pin_arr[--num_pio_pins].
				       pinaddr);
		else
			--num_pio_pins;

	printk(KERN_INFO "STMicroelectronics LIRC driver removed\n");
}

module_param(uhf_switch, bool, 0664);
MODULE_PARM_DESC(ir_or_uhf_offset, "Enable uhf mode");

module_init(lirc_stm_init);
module_exit(lirc_stm_release);
MODULE_DESCRIPTION
    ("Linux InfraRed receiver plugin for STMicroelectronics platforms");
MODULE_AUTHOR("Carl Shaw <carl.shaw@st.com>");
MODULE_AUTHOR("Angelo Castello <angelo.castello@st.com>");
MODULE_LICENSE("GPL");
