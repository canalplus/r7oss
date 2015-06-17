/*      $Id: lirc_wpc8769l.h,v 1.5 2009/06/15 15:11:39 jarodwilson Exp $      */

/****************************************************************************
 ** lirc_wpc8769l.h ****************************************************
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

#include <linux/types.h>

/* Name of the ACPI resource used to autodetect the receiver. */
#define WPC8769L_ACPI_HID "WEC1020"

/* Number of microseconds for a whole byte of samples. */
/* This is assuming 20 kHz bit sampling frequency.     */
#define WPC8769L_USECS_PER_BYTE 400

/* Number of microseconds for a bit sample. */
#define WPC8769L_USECS_PER_BIT (WPC8769L_USECS_PER_BYTE >> 3)

/* Number of bytes in each data burst. */
#define WPC8769L_BYTES_PER_BURST 14

/* Number of 0xff bytes before reset. */
#define WPC8769L_FF_BYTES_BEFORE_RESET 250

/* Microseconds timeout for last part of code. */
#define WPC8769L_LAST_TIMEOUT_JIFFIES (HZ / 20)

/* Microseconds timeout for last part of code. */
#define WPC8769L_LAST_TIMEOUT_JIFFIES (HZ / 20)

/* Size of I/O region 1. */
#define WPC8769L_IO_REGION_1_SIZE 0x08

/* Size of I/O region 2. */
#define WPC8769L_IO_REGION_2_SIZE 0x20

/* Size of a byte array for a complete burst, rounded
 * up to an integral number of unsigned longs. */
#define WPC8769L_BYTE_BUFFER_SIZE \
	(((WPC8769L_BYTES_PER_BURST + 1 + BITS_PER_LONG / 8 - 1) \
	/ (BITS_PER_LONG / 8)) * (BITS_PER_LONG / 8))



/* WPC8769L register set definitions. Note that these are all wild guesses.*/

/* Registers for I/O range 1. */
#define WPC8769L_SELECT_REG			0x03

/*------------*/
#define WPC8769L_BANK_00			0x00

#define WPC8769L_DATA_REG			0x00

#define WPC8769L_INTERRUPT_REG			0x01
#define WPC8769L_INTERRUPT_1_MASK		0x01
#define WPC8769L_INTERRUPT_2_MASK		0x01

#define WPC8769L_DATA_STATUS_REG		0x02
#define WPC8769L_DATA_READY_MASK		0x01
#define WPC8769L_DATA_STATUS_MASK_1		0x02
#define WPC8769L_DATA_STATUS_MASK_2		0xd0

#define WPC8769L_CONFIG_REG			0x04
#define WPC8769L_CONFIG_OFF_MASK		0xe0
#define WPC8769L_CONFIG_ON_MASK			0xc0

#define WPC8769L_DATA_ACK_REG			0x05
#define WPC8769L_DATA_ACK_MASK			0x01

#define WPC8769L_TIMEOUT_RESET_REG		0x07
#define WPC8769L_TIMEOUT_RESET_MASK		0x20

/*------------*/
#define WPC8769L_BANK_E0			0xe0

#define WPC8769L_CONFIG6_REG			0x00
#define WPC8769L_CONFIG6_MASK			0x4b

#define WPC8769L_CONFIG7_REG			0x01

#define WPC8769L_HARDWARE_ENABLE1_REG		0x02
#define WPC8769L_HARDWARE_ENABLE1_MASK		0x01

#define WPC8769L_CONFIG5_REG			0x04
#define WPC8769L_CONFIG5_ON_MASK		0x30

#define WPC8769L_REMAINING_RX_DATA_REG		0x07

/*------------*/
#define WPC8769L_BANK_E4			0xe4

#define WPC8769L_READ_ON_STARTUP_REG		0x00

/*------------*/
#define WPC8769L_BANK_EC			0xec

#define WPC8769L_CONFIG3_REG			0x04
#define WPC8769L_CONFIG3_ON_MASK		0x01
#define WPC8769L_CONFIG3_MASK_1			0x10

/*------------*/
#define WPC8769L_BANK_F0			0xf0

#define WPC8769L_WAKEUP_STATUS_LEG_REG		0x02
#define WPC8769L_WAKEUP_STATUS_LEG_MASK		0x04
#define WPC8769L_WAKEUP_STATUS_LEG_MASK_A	0x02
#define WPC8769L_WAKEUP_STATUS_LEG_MASK_B	0x08

/*------------*/
#define WPC8769L_BANK_F4			0xf4

#define WPC8769L_CONFIG9_REG			0x01

#define WPC8769L_CONFIG4_REG			0x02
#define WPC8769L_CONFIG4_AND_MASK		0x0f
#define WPC8769L_CONFIG4_ON_MASK		0x50

#define WPC8769L_CONFIG8_REG			0x04

#define WPC8769L_CONFIG2_REG			0x07
#define WPC8769L_CONFIG2_OFF_MASK		0x20
#define WPC8769L_CONFIG2_MASK_1			0x10


/* Registers for I/O range 2. */
#define WPC8769L_WAKEUP_ACK_REG			0x00
#define WPC8769L_WAKEUP_ACK_MASK		0x10

#define WPC8769L_WAKEUP_ENABLE_REG		0x02
#define WPC8769L_WAKEUP_ENABLE_MASK		0x10

#define WPC8769L_BANK2_CLOCK_REG		0x04
#define WPC8769L_CLOCK_OFF_MASK			0x02
#define WPC8769L_CLOCK_ON_MASK			0x01

#define WPC8769L_WAKEUP_CONFIG_REG		0x1a
#define WPC8769L_WAKEUP_CONFIG_PRE_MASK		0x80
#define WPC8769L_MAX_INFO_BITS_BIAS		0x0e
#define WPC8769L_MAX_INFO_BITS_SHIFT		0x01

#define WPC8769L_WAKEUP_CONFIG3_REG		0x13
#define WPC8769L_WAKEUP_CONFIG3_OFF_MASK	0x10
#define WPC8769L_WAKEUP_CONFIG3_ON_MASK		0x21
#define WPC8769L_WAKEUP_CONFIG3_A_SHIFT		0x01
#define WPC8769L_WAKEUP_CONFIG3_A_MASK		0x03
#define WPC8769L_WAKEUP_CONFIG3_B_SHIFT		0x03
#define WPC8769L_WAKEUP_CONFIG3_B_MASK		0x01

#define WPC8769L_WAKEUP_STATUS_REG		0x14
#define WPC8769L_WAKEUP_WOKE_UP_MASK		0x01
#define WPC8769L_WAKEUP_CONFIGURING_MASK	0x17

#define WPC8769L_WAKEUP_CONFIG2_REG		0x15
#define WPC8769L_WAKEUP_CONFIG2_AND_MASK	0xf9
#define WPC8769L_WAKEUP_CONFIG2_OR_MASK		0x01

#define WPC8769L_WAKEUP_DATA_PTR_REG		0x18
#define WPC8769L_WAKEUP_DATA_BITS		0x20
#define WPC8769L_WAKEUP_DATA_BASE		0x10
#define WPC8769L_WAKEUP_MASK_BASE		0x20

#define WPC8769L_WAKEUP_DATA_REG		0x19

