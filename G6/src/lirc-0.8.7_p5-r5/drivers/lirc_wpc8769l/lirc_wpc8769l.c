/*      $Id: lirc_wpc8769l.c,v 1.9 2010/03/17 14:16:16 jarodwilson Exp $      */

/****************************************************************************
 ** lirc_wpc8769l.c ****************************************************
 ****************************************************************************
 *
 * lirc_wpc8769l - Device driver for the integrated CIR receiver found in
 *                 Acer Aspire 6530G (and probably other models), based on
 *                 the Winbond 8769L embedded controller.
 *                 (Written using the lirc_serial driver as a guide).
 *
 * Copyright (C) 2008, 2009 Juan J. Garcia de Soria <skandalfo@gmail.com>
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 2, 18)
#error "**********************************************************"
#error " Sorry, this driver needs kernel version 2.2.18 or higher "
#error "**********************************************************"
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 33)
#include <linux/autoconf.h>
#endif

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/types.h>
#include <linux/poll.h>

#include <linux/bitops.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 16)
#include <asm/io.h>
#else
#include <linux/io.h>
#endif
#include <linux/irq.h>

#include <linux/acpi.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 18)
#include <linux/platform_device.h>
#endif

#include "drivers/lirc.h"
#include "drivers/kcompat.h"
#include "drivers/lirc_dev/lirc_dev.h"

#include "lirc_wpc8769l.h"

/* Name of the lirc device. */
#define LIRC_DRIVER_NAME "lirc_wpc8769l"

#define dprintk(fmt, args...)					\
	do {							\
		if (debug)					\
			printk(KERN_DEBUG LIRC_DRIVER_NAME ": "	\
			       fmt, ## args);			\
	} while (0)

#define wprintk(fmt, args...)					\
	do {							\
			printk(KERN_WARN LIRC_DRIVER_NAME ": "	\
			       fmt, ## args);			\
	} while (0)

#define eprintk(fmt, args...)					\
	do {							\
			printk(KERN_ERR LIRC_DRIVER_NAME ": "	\
			       fmt, ## args);			\
	} while (0)

#define iprintk(fmt, args...)					\
	do {							\
			printk(KERN_INFO LIRC_DRIVER_NAME ": "	\
			       fmt, ## args);			\
	} while (0)

/* Number of driver->lirc-dev buffer elements. */
#define RBUF_LEN 256

/* Number of 0xff bytes received in a row. */
static unsigned int wpc8769l_ff_bytes_in_a_row;

/* Hardware resource parameters. */
static unsigned int baseport1;
static unsigned int baseport2;
static unsigned int irq;

/* Debugging flag. */
static int debug;

/* If true, we skip ACPI autodetection and use the parameter-supplied I/O and
 * IRQ. */
static int skip_probe;

/* Whether the device is open or not. */
static int lirc_wpc8769l_is_open;

/* Code disabled since it didn't seem to work with the test hardware. */
/*#define LIRC_WPC8769L_WAKEUP*/
#ifdef LIRC_WPC8769L_WAKEUP
/* These parameters are taken from the driver for MS Windows Vista.
 * The specific values used for your hardware may be found at this registry
 * key:
 *
 * HKEY_LOCAL_MACHINE/CurrentControlSet/Services/Winbond CIR/PowerKey
 */
static int protocol_select = 2;
static int max_info_bits = 24;
static unsigned int rc_wakeup_code = 0x7ffffbf3;
static unsigned int rc_wakeup_mask = 0xff000fff;
#endif

/* Resource allocation pointers. */
static struct resource *wpc8769l_portblock1_resource;
static struct resource *wpc8769l_portblock2_resource;

/* Hardware related spinlock. */
static DEFINE_SPINLOCK(wpc8769l_hw_spinlock);

/* The buffer for ISR to bottom half data transfer. */
static struct lirc_buffer rbuf;

/* Bit-to-MODE2 coalescing helper variables. */
static int last_was_pulse;
static lirc_t last_counter;

/* Microseconds after a timeout-triggered pulse. */
static s64 lastus;

/* Microseconds when the timer was started. */
static s64 timerstartus;

/* Put another pulse/space to the queue, checking for overruns. */
static void put_item(lirc_t data)
{
	if (lirc_buffer_full(&rbuf)) {
		if (printk_ratelimit())
			eprintk("RX buffer overrun.\n");
		return;
	}
	lirc_buffer_write(&rbuf, (void *) &data);
}

/* Put any accumulated pulse/space to userspace. */
static void put_span(void)
{
	lirc_t data;
	if (last_counter) {
		/* Take the usecs length. */
		data = last_counter;

		/* Mark pulse or space. */
		if (last_was_pulse)
			data |= PULSE_BIT;

		/* Put the span to the buffer. */
		put_item(data);

		/* Reset counter, in order to avoid emitting duplicate data. */
		last_counter = 0;
	}
}

/* Aggregate pulse time. */
static void put_pulse_bit(lirc_t n)
{
	if (last_was_pulse) {
		last_counter += n;
		if (last_counter > PULSE_MASK)
			last_counter = PULSE_MASK;
	} else {
		put_span();
		last_was_pulse = 1;
		last_counter = n;
		if (last_counter > PULSE_MASK)
			last_counter = PULSE_MASK;
	}
}

/* Aggregate space time. */
static void put_space_bit(lirc_t n)
{
	if (!last_was_pulse) {
		last_counter += n;
		if (last_counter > PULSE_MASK)
			last_counter = PULSE_MASK;
	} else {
		put_span();
		last_was_pulse = 0;
		last_counter = n;
		if (last_counter > PULSE_MASK)
			last_counter = PULSE_MASK;
	}
}

/* Timeout function for last pulse part. */
static void wpc8769l_last_timeout(unsigned long l)
{
	struct timeval currenttv;

	unsigned long flags;
	spin_lock_irqsave(&wpc8769l_hw_spinlock, flags);

	/* Mark the time at which we inserted the timeout span. */
	do_gettimeofday(&currenttv);
	lastus = ((s64) currenttv.tv_sec) * 1000000ll + currenttv.tv_usec;

	/* Emit the timeout as a space. */
	put_space_bit(lastus - timerstartus);

	/* Signal the bottom half wait queue
	 * that there's data available. */
	wake_up_interruptible(&rbuf.wait_poll);

	spin_unlock_irqrestore(&wpc8769l_hw_spinlock, flags);
}

/* Timer for end-of-code pulse timeout. */
static struct timer_list last_span_timer =
	TIMER_INITIALIZER(wpc8769l_last_timeout, 0, 0);

/* Interrupt handler, doing the bit sample to mode2 conversion.
 * Perhaps this work should be taken outside of the ISR... */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19)
static irqreturn_t irq_handler(int irqno, void *blah)
#else
static irqreturn_t irq_handler(int irqno, void *blah, struct pt_regs *regs)
#endif
{
	unsigned int data;
	int handled = 0;
	int count, more;
	struct timeval currenttv;
	s64 currentus, span;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 25)
	unsigned char data_buf[WPC8769L_BYTE_BUFFER_SIZE];
	unsigned char *data_ptr;
	unsigned long *ldata;
	unsigned int next_one, next_zero, size;
#else
	unsigned int mask;
#endif

	unsigned long flags;
	spin_lock_irqsave(&wpc8769l_hw_spinlock, flags);

	/* Check whether there's any data available. */
	outb(WPC8769L_BANK_00, baseport1 + WPC8769L_SELECT_REG);
	data = inb(baseport1 + WPC8769L_DATA_STATUS_REG);

	if (data & WPC8769L_DATA_READY_MASK) {
		/* Get current timestamp. */
		do_gettimeofday(&currenttv);
		currentus = ((s64) currenttv.tv_sec) * 1000000ll +
			currenttv.tv_usec;

		/* If we had a timeout before we might need to fill
		 * in additional space time. */
		if (lastus) {
			/* Calculate the difference, compensating
			 * the time for the data successfully
			 * received (estimated to be
			 * WPC8769L_BYTES_PER_BURST bytes). */
			span = currentus - lastus
					- WPC8769L_BYTES_PER_BURST
					* WPC8769L_USECS_PER_BYTE;

			/* Only insert positive spans. */
			if (span > 0) {
				/* Emit the extended gap as a space. */
				put_space_bit(span);
			}

			/* Mark that we had the last timeout into account. */
			lastus = 0;
		}

		count = 0;
		more = 1;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 25)
		data_ptr = data_buf;
#endif
		do {
			/* Read the next byte of data. */
			outb(WPC8769L_BANK_00, baseport1 + WPC8769L_SELECT_REG);
			data = inb(baseport1 + WPC8769L_DATA_REG);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 25)
			*data_ptr++ = data;
#else
			for (mask = 0x01 ; mask < 0x100; mask <<= 1) {
				if (data & mask)
					put_space_bit(WPC8769L_USECS_PER_BIT);
				else
					put_pulse_bit(WPC8769L_USECS_PER_BIT);
			}
#endif

			/* Check for 0xff in a row. */
			if (data == 0xff)
				wpc8769l_ff_bytes_in_a_row++;
			else
				wpc8769l_ff_bytes_in_a_row = 0;

			outb(WPC8769L_BANK_00, baseport1 + WPC8769L_SELECT_REG);
			data = inb(baseport1 + WPC8769L_DATA_ACK_REG);
			if (data & WPC8769L_DATA_ACK_REG) {
				outb(WPC8769L_BANK_E0,
					baseport1 + WPC8769L_SELECT_REG);
				data = inb(baseport1 +
					WPC8769L_REMAINING_RX_DATA_REG);
				if (!data)
					more = 0;
			} else
				more = 0;

			count++;
		} while (more && count < WPC8769L_BYTES_PER_BURST);

		if (wpc8769l_ff_bytes_in_a_row
			>= WPC8769L_FF_BYTES_BEFORE_RESET) {

			/* Put in another 0xff byte. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 25)
			*data_ptr++ = 0xff;
			count++;
#else
			put_space_bit(8 * WPC8769L_USECS_PER_BIT);
#endif

			/* Reset the hardware in the case of too many
			 * 0xff bytes in a row. */
			outb(WPC8769L_BANK_00, baseport1 + WPC8769L_SELECT_REG);
			outb(WPC8769L_TIMEOUT_RESET_MASK,
				baseport1 + WPC8769L_TIMEOUT_RESET_REG);
		}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 25)
		/* Emit the data. */
		size = count << 3;

		ldata = (unsigned long *) data_buf;
		next_one = generic_find_next_le_bit(ldata, size, 0);

		if (next_one > 0)
			put_pulse_bit(next_one
				* WPC8769L_USECS_PER_BIT);

		while (next_one < size) {
			next_zero = generic_find_next_zero_le_bit(ldata,
				size, next_one + 1);

			put_space_bit(
				(next_zero - next_one)
				* WPC8769L_USECS_PER_BIT);

			if (next_zero < size) {
				next_one = generic_find_next_le_bit(ldata,
					size, next_zero + 1);

				put_pulse_bit(
					(next_one - next_zero)
					* WPC8769L_USECS_PER_BIT);
			} else {
				next_one = size;
			}
		}
#endif

		/* Mark the IRQ as handled. */
		handled = 1;

		/* Signal the bottom half wait queue
		 * that there's data available. */
		wake_up_interruptible(&rbuf.wait_poll);

		/* Set up timeout handling. */
		mod_timer(&last_span_timer,
			jiffies + WPC8769L_LAST_TIMEOUT_JIFFIES);

		/* Set up last timer us mark. */
		timerstartus = currentus;
	}

	spin_unlock_irqrestore(&wpc8769l_hw_spinlock, flags);
	return IRQ_RETVAL(handled);
}

/* Prepare the hardware on module load. */
static void wpc8769l_prepare_hardware(void)
{
	unsigned long flags;
	spin_lock_irqsave(&wpc8769l_hw_spinlock, flags);

	/* I don't know why this needs reading. */
	outb(WPC8769L_BANK_E4, baseport1 + WPC8769L_SELECT_REG);
	inb(baseport1 + WPC8769L_READ_ON_STARTUP_REG);

	spin_unlock_irqrestore(&wpc8769l_hw_spinlock, flags);
}


/* Wake up device from power down and check whether it was the
 * device that woke us up.
 */
static int wpc8769l_power_up_and_check_if_we_woke_us_up(void)
{
	unsigned int data;
	int res;

	unsigned long flags;
	spin_lock_irqsave(&wpc8769l_hw_spinlock, flags);

	if (baseport2) {
		data = inb(baseport2 + WPC8769L_BANK2_CLOCK_REG);
		data &= ~WPC8769L_CLOCK_OFF_MASK;
		data |= WPC8769L_CLOCK_ON_MASK;
		outb(data, baseport2 + WPC8769L_BANK2_CLOCK_REG);

		res = inb(baseport2 + WPC8769L_WAKEUP_STATUS_REG)
			& WPC8769L_WAKEUP_WOKE_UP_MASK;

		data = inb(baseport2 + WPC8769L_BANK2_CLOCK_REG);
		data &= ~WPC8769L_CLOCK_OFF_MASK;
		data |= WPC8769L_CLOCK_ON_MASK;
		outb(data, baseport2 + WPC8769L_BANK2_CLOCK_REG);

		outb(WPC8769L_WAKEUP_WOKE_UP_MASK,
			baseport2 + WPC8769L_WAKEUP_STATUS_REG);

		outb(WPC8769L_WAKEUP_ACK_MASK,
			baseport2 + WPC8769L_WAKEUP_ACK_REG);
	} else {
		outb(WPC8769L_BANK_F0, baseport1 + WPC8769L_SELECT_REG);
		res = (inb(baseport1 + WPC8769L_WAKEUP_STATUS_LEG_REG)
			& WPC8769L_WAKEUP_STATUS_LEG_MASK) ? 1 : 0;
	}

	spin_unlock_irqrestore(&wpc8769l_hw_spinlock, flags);

	return res;
}

/* Disable interrupts from device. */
static void wpc8769l_disable_interrupts(void)
{
	unsigned long flags;
	spin_lock_irqsave(&wpc8769l_hw_spinlock, flags);

	outb(WPC8769L_BANK_00, baseport1 + WPC8769L_SELECT_REG);
	outb(WPC8769L_BANK_00, baseport1 + WPC8769L_SELECT_REG);
	outb(inb(baseport1 + WPC8769L_INTERRUPT_REG)
		& ~WPC8769L_INTERRUPT_1_MASK,
		baseport1 + WPC8769L_INTERRUPT_REG);
	outb(WPC8769L_BANK_00, baseport1 + WPC8769L_SELECT_REG);
	outb(WPC8769L_BANK_00, baseport1 + WPC8769L_SELECT_REG);
	outb(inb(baseport1 + WPC8769L_INTERRUPT_REG)
		& ~WPC8769L_INTERRUPT_1_MASK,
		baseport1 + WPC8769L_INTERRUPT_REG);

	spin_unlock_irqrestore(&wpc8769l_hw_spinlock, flags);
}

#ifdef LIRC_WPC8769L_WAKEUP
/* Expand value nibble for configuration of wake up parameters.
 * This seems to manchester-encode a nibble into a byte. */
static unsigned int wpc8769l_expand_value_nibble(unsigned int nibble)
{
	int i;
	unsigned int tmp, tmp2, res;

	res = 0;

	for (i = 0; i < 4; i += 2) {
		tmp = (nibble >> i) & 0x3;
		switch (tmp) {
		case 3:
			tmp2 = 5;
			break;
		case 2:
			tmp2 = 6;
			break;
		case 1:
			tmp2 = 9;
			break;
		case 0:
			tmp2 = 0x0a;
			break;
		default:
			return 0;
			break;
		}
		res |= ((tmp2 << i) << i);
	}

	return res;
}

/* Expand mask nibble for configuration of wake up parameters. */
static unsigned int wpc8769l_expand_mask_nibble(unsigned int nibble)
{
	int i;
	unsigned int tmp, tmp2, res;

	res = 0;

	for (i = 0; i < 4; i += 2) {
		tmp = (nibble >> i) & 0x3;
		switch (tmp) {
		case 0:
			tmp2 = 0;
			break;
		case 1:
			tmp2 = 3;
			break;
		case 2:
			tmp2 = 0x0c;
			break;
		case 3:
			tmp2 = 0x0f;
			break;
		default:
			return 0;
			break;
		}
		res |= ((tmp2 << i) << i);
	}

	return res;
}

/* Configure wake up triggers for the hardware that supports it.
 * THE CALLER MUST HAVE ACQUIRED wpc8769l_hw_spinlock BEFORE CALLING.
 */
static void wpc8769l_configure_wakeup_triggers(void)
{
	unsigned int x;
	unsigned int data, data2;

	int i, j;

	x = inb(baseport2 + WPC8769L_WAKEUP_ENABLE_REG)
		& WPC8769L_WAKEUP_ENABLE_MASK;
	outb(inb(baseport2 + WPC8769L_WAKEUP_ENABLE_REG)
		& ~WPC8769L_WAKEUP_ENABLE_MASK,
		baseport2 + WPC8769L_WAKEUP_ENABLE_REG);

	data = inb(baseport2 + WPC8769L_BANK2_CLOCK_REG);
	data &= ~WPC8769L_CLOCK_OFF_MASK;
	data |= WPC8769L_CLOCK_ON_MASK;
	outb(data, baseport2 + WPC8769L_BANK2_CLOCK_REG);

	outb(WPC8769L_WAKEUP_CONFIGURING_MASK,
		baseport2 + WPC8769L_WAKEUP_STATUS_REG);
	outb(WPC8769L_WAKEUP_ACK_MASK,
		baseport2 + WPC8769L_WAKEUP_ACK_REG);

	data = inb(baseport2 + WPC8769L_BANK2_CLOCK_REG);
	data &= ~WPC8769L_CLOCK_OFF_MASK;
	data |= WPC8769L_CLOCK_ON_MASK;
	outb(data, baseport2 + WPC8769L_BANK2_CLOCK_REG);

	data = inb(baseport2 + WPC8769L_BANK2_CLOCK_REG);
	data &= ~WPC8769L_CLOCK_OFF_MASK;
	data |= WPC8769L_CLOCK_ON_MASK;
	outb(data, baseport2 + WPC8769L_BANK2_CLOCK_REG);

	data = inb(baseport2 + WPC8769L_WAKEUP_CONFIG_REG);
	data &= WPC8769L_WAKEUP_CONFIG_PRE_MASK;
	data |= (max_info_bits + WPC8769L_MAX_INFO_BITS_BIAS)
		<< WPC8769L_MAX_INFO_BITS_SHIFT;
	outb(data, baseport2 + WPC8769L_WAKEUP_CONFIG_REG);

	i = j = 0;

	/* Program values. */
	while (j < WPC8769L_WAKEUP_DATA_BITS) {
		data = inb(baseport2 + WPC8769L_BANK2_CLOCK_REG);
		data &= ~WPC8769L_CLOCK_OFF_MASK;
		data |= WPC8769L_CLOCK_ON_MASK;
		outb(data, baseport2 + WPC8769L_BANK2_CLOCK_REG);

		outb(i + WPC8769L_WAKEUP_DATA_BASE,
			baseport2 + WPC8769L_WAKEUP_DATA_PTR_REG);

		data = inb(baseport2 + WPC8769L_BANK2_CLOCK_REG);
		data &= ~WPC8769L_CLOCK_OFF_MASK;
		data |= WPC8769L_CLOCK_ON_MASK;
		outb(data, baseport2 + WPC8769L_BANK2_CLOCK_REG);

		data = (rc_wakeup_code >> j) & 0x0f;
		data = wpc8769l_expand_value_nibble(data);
		outb(data, baseport2 + WPC8769L_WAKEUP_DATA_REG);

		i++;
		j += 4;
	}

	/* Program masks. */
	while (j < WPC8769L_WAKEUP_DATA_BITS) {
		data = inb(baseport2 + WPC8769L_BANK2_CLOCK_REG);
		data &= ~WPC8769L_CLOCK_OFF_MASK;
		data |= WPC8769L_CLOCK_ON_MASK;
		outb(data, baseport2 + WPC8769L_BANK2_CLOCK_REG);

		outb(i + WPC8769L_WAKEUP_MASK_BASE,
			baseport2 + WPC8769L_WAKEUP_DATA_PTR_REG);

		data = inb(baseport2 + WPC8769L_BANK2_CLOCK_REG);
		data &= ~WPC8769L_CLOCK_OFF_MASK;
		data |= WPC8769L_CLOCK_ON_MASK;
		outb(data, baseport2 + WPC8769L_BANK2_CLOCK_REG);

		data = (rc_wakeup_mask >> j) & 0x0f;
		data = wpc8769l_expand_mask_nibble(data);
		outb(data, baseport2 + WPC8769L_WAKEUP_DATA_REG);

		i++;
		j += 4;
	}

	data = inb(baseport2 + WPC8769L_BANK2_CLOCK_REG);
	data &= ~WPC8769L_CLOCK_OFF_MASK;
	data |= WPC8769L_CLOCK_ON_MASK;
	outb(data, baseport2 + WPC8769L_BANK2_CLOCK_REG);

	data2 = inb(baseport2 + WPC8769L_WAKEUP_CONFIG2_REG);
	data2 &= WPC8769L_WAKEUP_CONFIG2_AND_MASK;
	data2 |= WPC8769L_WAKEUP_CONFIG2_OR_MASK;

	data = inb(baseport2 + WPC8769L_BANK2_CLOCK_REG);
	data &= ~WPC8769L_CLOCK_OFF_MASK;
	data |= WPC8769L_CLOCK_ON_MASK;
	outb(data, baseport2 + WPC8769L_BANK2_CLOCK_REG);

	outb(data2, baseport2 + WPC8769L_WAKEUP_CONFIG2_REG);

	if (x != WPC8769L_WAKEUP_ENABLE_MASK)
		outb(inb(baseport2 + WPC8769L_WAKEUP_ENABLE_REG)
		| WPC8769L_WAKEUP_ENABLE_MASK,
		baseport2 + WPC8769L_WAKEUP_ENABLE_REG);
}
#endif

/* Enable interrupts from device. */
static void wpc8769l_enable_interrupts(void)
{
	unsigned int data, data2, data_save;

	unsigned int a, b;

	unsigned long flags;
	spin_lock_irqsave(&wpc8769l_hw_spinlock, flags);

	outb(WPC8769L_BANK_F0, baseport1 + WPC8769L_SELECT_REG);
	data_save = inb(baseport1 + WPC8769L_WAKEUP_STATUS_LEG_REG);

	outb(WPC8769L_BANK_E0, baseport1 + WPC8769L_SELECT_REG);
	outb(0, baseport1 + WPC8769L_HARDWARE_ENABLE1_REG);

	outb(WPC8769L_BANK_E0, baseport1 + WPC8769L_SELECT_REG);
	outb(WPC8769L_BANK_E0, baseport1 + WPC8769L_SELECT_REG);
	outb(inb(baseport1 + WPC8769L_HARDWARE_ENABLE1_REG)
		| WPC8769L_HARDWARE_ENABLE1_MASK,
		baseport1 + WPC8769L_HARDWARE_ENABLE1_REG);

	outb(WPC8769L_BANK_00, baseport1 + WPC8769L_SELECT_REG);
	outb(0, baseport1 + WPC8769L_CONFIG_REG);

	outb(WPC8769L_BANK_00, baseport1 + WPC8769L_SELECT_REG);
	outb(WPC8769L_BANK_00, baseport1 + WPC8769L_SELECT_REG);
	data = inb(baseport1 + WPC8769L_CONFIG_REG);
	data &= ~WPC8769L_CONFIG_OFF_MASK;
	data |= WPC8769L_CONFIG_ON_MASK;
	outb(data, baseport1 + WPC8769L_CONFIG_REG);

	outb(WPC8769L_BANK_00, baseport1 + WPC8769L_SELECT_REG);
	outb(WPC8769L_DATA_STATUS_MASK_1, baseport1 + WPC8769L_DATA_STATUS_REG);

	outb(WPC8769L_BANK_00, baseport1 + WPC8769L_SELECT_REG);
	outb(WPC8769L_DATA_STATUS_MASK_2, baseport1 + WPC8769L_DATA_STATUS_REG);

	outb(WPC8769L_BANK_F4, baseport1 + WPC8769L_SELECT_REG);
	outb(WPC8769L_BANK_F4, baseport1 + WPC8769L_SELECT_REG);
	outb(inb(baseport1 + WPC8769L_CONFIG2_REG)
		& ~WPC8769L_CONFIG2_OFF_MASK,
		baseport1 + WPC8769L_CONFIG2_REG);

	outb(WPC8769L_BANK_EC, baseport1 + WPC8769L_SELECT_REG);
	outb(WPC8769L_BANK_EC, baseport1 + WPC8769L_SELECT_REG);
	outb(inb(baseport1 + WPC8769L_CONFIG3_REG)
		| WPC8769L_CONFIG3_ON_MASK,
		baseport1 + WPC8769L_CONFIG3_REG);

	outb(WPC8769L_BANK_F4, baseport1 + WPC8769L_SELECT_REG);
	data = inb(baseport1 + WPC8769L_CONFIG4_REG);
	data &= WPC8769L_CONFIG4_AND_MASK;
	data |= WPC8769L_CONFIG4_ON_MASK;

	outb(WPC8769L_BANK_F4, baseport1 + WPC8769L_SELECT_REG);
	outb(data, baseport1 + WPC8769L_CONFIG4_REG);

	outb(WPC8769L_BANK_E0, baseport1 + WPC8769L_SELECT_REG);
	outb(WPC8769L_BANK_E0, baseport1 + WPC8769L_SELECT_REG);
	outb(inb(baseport1 + WPC8769L_CONFIG5_REG)
		| WPC8769L_CONFIG5_ON_MASK,
		baseport1 + WPC8769L_CONFIG5_REG);

	outb(WPC8769L_BANK_E0, baseport1 + WPC8769L_SELECT_REG);
	outb(WPC8769L_CONFIG6_MASK, baseport1 + WPC8769L_CONFIG6_REG);

	outb(WPC8769L_BANK_E0, baseport1 + WPC8769L_SELECT_REG);
	outb(0, baseport1 + WPC8769L_CONFIG7_REG);

	if (baseport2) {
		/*
		 * This has to do with wake-up support, which is
		 * disabled when the second I/O range doesn't
		 * exist.
		 */
		/* -- internal subroutine -- */
		data = inb(baseport2 + WPC8769L_BANK2_CLOCK_REG);
		data &= ~WPC8769L_CLOCK_OFF_MASK;
		data |= WPC8769L_CLOCK_ON_MASK;
		outb(data, baseport2 + WPC8769L_BANK2_CLOCK_REG);

		data2 = inb(baseport2 + WPC8769L_WAKEUP_CONFIG3_REG);
		a = (data2 >> WPC8769L_WAKEUP_CONFIG3_A_SHIFT)
			& WPC8769L_WAKEUP_CONFIG3_A_MASK;
		b = (data2 >> WPC8769L_WAKEUP_CONFIG3_B_SHIFT)
			& WPC8769L_WAKEUP_CONFIG3_B_MASK;

		data = inb(baseport2 + WPC8769L_BANK2_CLOCK_REG);
		data &= ~WPC8769L_CLOCK_OFF_MASK;
		data |= WPC8769L_CLOCK_ON_MASK;
		outb(data, baseport2 + WPC8769L_BANK2_CLOCK_REG);

		data2 &= ~WPC8769L_WAKEUP_CONFIG3_OFF_MASK;
		data2 |= WPC8769L_WAKEUP_CONFIG3_ON_MASK;
		outb(data2, baseport2 + WPC8769L_WAKEUP_CONFIG3_REG);
		/* -- end internal subroutine -- */

#ifdef LIRC_WPC8769L_WAKEUP
		/* Call for setting wake up filters */
		wpc8769l_configure_wakeup_triggers();
#endif
	} else {
		/* No second port range. Take these defaults. */
		a = (data_save & WPC8769L_WAKEUP_STATUS_LEG_MASK_A)
			? 0 : 1;
		b = (data_save & WPC8769L_WAKEUP_STATUS_LEG_MASK_B)
			? 1 : 0;
	}

	outb(WPC8769L_BANK_EC, baseport1 + WPC8769L_SELECT_REG);
	outb(WPC8769L_BANK_EC, baseport1 + WPC8769L_SELECT_REG);

	data = inb(baseport1 + WPC8769L_CONFIG3_REG);
	data = (a == 1)
		? (data & ~WPC8769L_CONFIG3_MASK_1)
		: (data | WPC8769L_CONFIG3_MASK_1);
	outb(data, baseport1 + WPC8769L_CONFIG3_REG);

	outb(WPC8769L_BANK_F4, baseport1 + WPC8769L_SELECT_REG);
	outb(WPC8769L_BANK_F4, baseport1 + WPC8769L_SELECT_REG);

	data = inb(baseport1 + WPC8769L_CONFIG2_REG);
	data = (b == 0)
		? (data & ~WPC8769L_CONFIG2_MASK_1)
		: (data | WPC8769L_CONFIG2_MASK_1);
	outb(data, baseport1 + WPC8769L_CONFIG2_REG);

	outb(0, baseport1 + WPC8769L_CONFIG8_REG);

	outb(0, baseport1 + WPC8769L_CONFIG9_REG);

	outb(WPC8769L_BANK_00, baseport1 + WPC8769L_SELECT_REG);
	outb(WPC8769L_BANK_00, baseport1 + WPC8769L_SELECT_REG);
	outb(inb(baseport1 + WPC8769L_INTERRUPT_REG)
		| WPC8769L_INTERRUPT_1_MASK,
		baseport1 + WPC8769L_INTERRUPT_REG);

	spin_unlock_irqrestore(&wpc8769l_hw_spinlock, flags);
}

/* Called when the device is opened. */
static int set_use_inc(void *data)
{
	int result;

	/* Reset pulse values. */
	last_was_pulse = 0;
	last_counter = 0;

	/* Reset last timeout value. */
	lastus = 0;

	/* Init the read buffer. */
	if (lirc_buffer_init(&rbuf, sizeof(lirc_t), RBUF_LEN) < 0)
		return -ENOMEM;

	/* Acquire the IRQ. */
	result = request_irq(irq, irq_handler,
			   IRQF_DISABLED | IRQF_SHARED,
			   LIRC_DRIVER_NAME, THIS_MODULE);

	switch (result) {
	case -EBUSY:
		eprintk("IRQ %d busy\n", irq);
		lirc_buffer_free(&rbuf);
		return -EBUSY;
	case -EINVAL:
		eprintk("Bad irq number or handler\n");
		lirc_buffer_free(&rbuf);
		return -EINVAL;
	default:
		dprintk("IRQ %d obtained.\n", irq);
		break;
	};

	/* Mark the device as open. */
	lirc_wpc8769l_is_open = 1;

	/* Enable hardware interrupts. */
	wpc8769l_enable_interrupts();

	MOD_INC_USE_COUNT;
	return 0;
}

/* Called when the device is released. */
static void set_use_dec(void *data)
{
	/* Mark the device as closed. */
	lirc_wpc8769l_is_open = 0;

	/* Cancel the timeout if pending. */
	del_timer_sync(&last_span_timer);

	/* Disable the hardware interrupts. */
	wpc8769l_disable_interrupts();

	/* Free the IRQ. */
	free_irq(irq, THIS_MODULE);
	dprintk("Freed IRQ %d\n", irq);

	/* Free the RX buffer. */
	lirc_buffer_free(&rbuf);

	MOD_DEC_USE_COUNT;
}

static struct lirc_driver driver = {
	.name		= LIRC_DRIVER_NAME,
	.minor		= -1,
	.code_length	= 1,
	.sample_rate	= 0,
	.data		= NULL,
	.add_to_buf	= NULL,
	.get_queue	= NULL,
	.rbuf		= &rbuf,
	.set_use_inc	= set_use_inc,
	.set_use_dec	= set_use_dec,
	.fops		= NULL,
	.dev		= NULL,
	.owner		= THIS_MODULE,
};

static acpi_status wec_parse_resources(struct acpi_resource *resource,
	void *context)
{
	if (resource->type == ACPI_RESOURCE_TYPE_IO) {
		/* Read the two I/O ranges. */
		if (!baseport1)
			baseport1 = resource->data.io.minimum;
		else if (!baseport2)
			baseport2 = resource->data.io.minimum;
	} else if (resource->type == ACPI_RESOURCE_TYPE_IRQ) {
		/* Read the rx IRQ number. */
		if (!irq)
			irq = resource->data.irq.interrupts[0];
	}
	return AE_OK;
}

static acpi_status wec_parse_device(acpi_handle handle, u32 level,
	void *context, void **return_value)
{
	acpi_status status;
	iprintk("Found %s device via ACPI.\n", WPC8769L_ACPI_HID);

	status = acpi_walk_resources(handle, METHOD_NAME__CRS,
		wec_parse_resources, NULL);
	if (ACPI_FAILURE(status))
		return status;

	return AE_OK;
}

/* Find the device I/O ranges and IRQ number by searching for the
 * CIR ACPI entry. */
static int wpc8769l_acpi_detect(void)
{
	acpi_status status;
	status = acpi_get_devices(WPC8769L_ACPI_HID, wec_parse_device, NULL,
		NULL);
	if (ACPI_FAILURE(status))
		return -ENOENT;
	else
		return 0;
}

#ifdef MODULE
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 18)
static struct platform_device *lirc_wpc8769l_platform_dev;

static int __devinit lirc_wpc8769l_probe(struct platform_device *dev)
{
	return 0;
}

static int __devexit lirc_wpc8769l_remove(struct platform_device *dev)
{
	return 0;
}

static int lirc_wpc8769l_suspend(struct platform_device *dev,
			       pm_message_t state)
{
	if (lirc_wpc8769l_is_open)
		/* Disable all interrupts. */
		wpc8769l_disable_interrupts();
	return 0;
}

static int lirc_wpc8769l_resume(struct platform_device *dev)
{
	if (lirc_wpc8769l_is_open) {
		/* Check if we caused resuming; we still do nothing about it. */
		wpc8769l_power_up_and_check_if_we_woke_us_up();

		/* Enable interrupts again. */
		wpc8769l_enable_interrupts();
	}
	return 0;
}

static struct platform_driver lirc_wpc8769l_platform_driver = {
	.probe		= lirc_wpc8769l_probe,
	.remove		= __devexit_p(lirc_wpc8769l_remove),
	.suspend	= lirc_wpc8769l_suspend,
	.resume		= lirc_wpc8769l_resume,
	.driver		= {
		.name	= LIRC_DRIVER_NAME,
		.owner	= THIS_MODULE,
	},
};

static int __init lirc_wpc8769l_platform_init(void)
{
	int result;

	result = platform_driver_register(&lirc_wpc8769l_platform_driver);
	if (result) {
		eprintk("Platform driver register returned %d.\n", result);
		return result;
	}

	lirc_wpc8769l_platform_dev = platform_device_alloc(LIRC_DRIVER_NAME, 0);
	if (!lirc_wpc8769l_platform_dev) {
		result = -ENOMEM;
		goto exit_driver_unregister;
	}

	result = platform_device_add(lirc_wpc8769l_platform_dev);
	if (result)
		goto exit_device_put;

	return 0;

exit_device_put:
	platform_device_put(lirc_wpc8769l_platform_dev);

exit_driver_unregister:
	platform_driver_unregister(&lirc_wpc8769l_platform_driver);
	return result;
}

static void __exit lirc_wpc8769l_platform_exit(void)
{
	platform_device_unregister(lirc_wpc8769l_platform_dev);
	platform_driver_unregister(&lirc_wpc8769l_platform_driver);
}
#endif

static int __init lirc_wpc8769l_module_init(void)
{
	int rc;

	/* If needed, read the resource information for the ACPI device
	 * description. */
	if (!skip_probe) {
		rc = wpc8769l_acpi_detect();
		if (rc) {
			eprintk("Error when looking for %s ACPI device.\n",
				WPC8769L_ACPI_HID);
			return rc;
		}
	}

	/* Check that we got some resource info to work with. */
	if (!baseport1 || !irq) {
		rc = -ENODEV;
		eprintk("Not all required resources found for %s device.\n",
			LIRC_DRIVER_NAME);
		return rc;
	}

	dprintk("%s device found to use 0x%04x, 0x%04x I/O bases, IRQ #%d.\n",
		LIRC_DRIVER_NAME, baseport1, baseport2, irq);

	/* Request the two I/O regions. */
	wpc8769l_portblock1_resource = request_region(baseport1,
		WPC8769L_IO_REGION_1_SIZE, LIRC_DRIVER_NAME);
	if (!wpc8769l_portblock1_resource) {
		rc = -EBUSY;
		eprintk("Could not allocate I/O range at 0x%04x", baseport1);
		return rc;
	}
	if (baseport2) {
		wpc8769l_portblock2_resource = request_region(baseport2,
			WPC8769L_IO_REGION_2_SIZE, LIRC_DRIVER_NAME);
		if (!wpc8769l_portblock2_resource) {
			rc = -EBUSY;
			printk(KERN_ERR "Could not allocate I/O range "
			       "at 0x%04x",
				baseport2);
			goto exit_release_region_1;
		}
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 18)
	/* Register the platform driver and device. */
	rc = lirc_wpc8769l_platform_init();
	if (rc)
		goto exit_release_region_2;
#endif

	/* Prepare the hardware. */
	wpc8769l_prepare_hardware();

	/* Do load-time checks. */
	wpc8769l_power_up_and_check_if_we_woke_us_up();

	/* Configure the driver hooks. */
	driver.features = LIRC_CAN_REC_MODE2;
	driver.minor = lirc_register_driver(&driver);
	if (driver.minor < 0) {
		eprintk("lirc_register_driver failed!\n");
		rc = -EIO;
		goto exit_platform_exit;
	}

	iprintk("Driver loaded.\n");

	return 0; /* Everything OK. */

exit_platform_exit:
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 18)
	lirc_wpc8769l_platform_exit();

exit_release_region_2:
#endif
	if (baseport2)
		release_region(baseport2, WPC8769L_IO_REGION_2_SIZE);

exit_release_region_1:
	release_region(baseport1, WPC8769L_IO_REGION_1_SIZE);

	return rc;
}

module_init(lirc_wpc8769l_module_init);

static void __exit lirc_wpc8769l_module_exit(void)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 18)
	/* Unregister the platform driver and device. */
	lirc_wpc8769l_platform_exit();
#endif

	/* Unregister the LIRC driver. */
	lirc_unregister_driver(driver.minor);

	/* Release the second range. */
	if (baseport2)
		release_region(baseport2, WPC8769L_IO_REGION_2_SIZE);

	/* Release the first range. */
	release_region(baseport1, WPC8769L_IO_REGION_1_SIZE);

	iprintk("Driver unloaded.\n");
}

module_exit(lirc_wpc8769l_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Juan J. Garcia de Soria");
MODULE_DESCRIPTION("Driver for the integrated Winbond WPC8769L-based IR\
 receiver found in Acer laptops.");
MODULE_VERSION("0.0");

module_param(debug, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(debug, "Enable debugging messages");

module_param(baseport1, uint, S_IRUGO);
MODULE_PARM_DESC(baseport1,
	"First I/O range base address (default: ACPI autodetect).");

module_param(baseport2, uint, S_IRUGO);
MODULE_PARM_DESC(baseport2,
	"Second I/O range base address (default: ACPI autodetect).");

module_param(irq, uint, S_IRUGO);
MODULE_PARM_DESC(irq, "IRQ number (default: ACPI autodetect).");

module_param(skip_probe, bool, S_IRUGO);
MODULE_PARM_DESC(skip_probe,
	"Skip ACPI-based device detection \
(default: false for ACPI autodetect).");

#ifdef LIRC_WPC8769L_WAKEUP
module_param(protocol_select, int, S_IRUGO);
MODULE_PARM_DESC(protocol_select,
	"Define the protocol for wake up functions (default: 2).");

module_param(max_info_bits, int, S_IRUGO);
MODULE_PARM_DESC(max_info_bits,
	"Define the maximum info bits for wake up functions (default: 24).");

module_param(rc_wakeup_code, uint, S_IRUGO);
MODULE_PARM_DESC(rc_wakeup_code,
	"Define the RC code value for wake up functions\
 (default: 0x7ffffbf3).");

module_param(rc_wakeup_mask, uint, S_IRUGO);
MODULE_PARM_DESC(rc_wakeup_mask,
	"Define the RC code mask for wake up functions (default: 0xff000fff).");
#endif

EXPORT_NO_SYMBOLS;

#endif /* MODULE */

