/*
 * arch/sh/boards/st/common/db641.c
 *
 * Copyright (C) 2007 STMicroelectronics Limited
 * Author: Pawel Moll <pawel.moll@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics STEM board with double Ethernet interface support
 *
 * Only first port (CN1) is supported. Required DB641 jumpers settings are:
 * - J1 and J5 = open
 * - J2, J3, J4 = 1-2
 *
 * STEM notCS0/notINTR0 lines are expected to be used - see
 * "include/asm-sh/<board>/stem.h" for more information about
 * main board configuration.
 */

#ifdef CONFIG_SH_ST_MB705
/*
 * The EPLD on the mb705 which sub-decodes EMI bank 4 into EPLD registers
 * and STEM CS0 uses bits 17-16 of the address bus, where A[17:16] = "01"
 * selects STEM CS0. However the db641 uses A16 for the FIFO_SEL pin.
 * This means that it is impossible to use STEM CS0 with this card, and
 * we have to use STEM CS1.
 *
 * STEM CS1 is driven by EMI CSD. The validation team target pack
 * configures this in PC card mode, so we need to reconfigure this as part
 * of the EMI programming.
 */
#define CS_BANK STEM_CS1_BANK
#define CS_OFFSET STEM_CS1_OFFSET
#define STEM_INTR_IRQ STEM_INTR1_IRQ
#define pcmode_configure(a, b) emi_config_pcmode(a, b)
#else
#define CS_BANK STEM_CS0_BANK
#define CS_OFFSET STEM_CS0_OFFSET
#define STEM_INTR_IRQ STEM_INTR0_IRQ
#define pcmode_configure(a, b) do { } while (0)
#endif

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/stm/emi.h>
#include <asm/mach/stem.h>

static struct platform_device smsc_lan9117 = {
	.name		= "smc911x",
	.id		= -1,
	.num_resources	= 4,
	.resource	= (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			/* .start & .end - see db641_init() */
		},
		{
			.flags = IORESOURCE_IRQ,
			/* .start & .end - see db641_init() */
		},
		/* See end of "drivers/net/smsc_911x/smsc9118.c" file
		 * for description of two following resources. */
		{
			.flags = IORESOURCE_IRQ,
			.name  = "polarity",
			.start = 0,
			.end   = 0,
		},
		{
			.flags = IORESOURCE_IRQ,
			.name  = "type",
			.start = 0,
			.end   = 0,
		},
	},
};

static int __init db641_init(void)
{
	smsc_lan9117.resource[0].start = emi_bank_base(CS_BANK) + CS_OFFSET;
	smsc_lan9117.resource[0].end = smsc_lan9117.resource[0].start + 0xff;

	smsc_lan9117.resource[1].start = STEM_INTR_IRQ;
	smsc_lan9117.resource[1].end = smsc_lan9117.resource[1].start;

	pcmode_configure(CS_BANK, 0);
	emi_bank_configure(CS_BANK, (unsigned long[4]) { 0x041086f1,
			0x0e024400, 0x0e024400, 0 });

	return platform_device_register(&smsc_lan9117);
}
arch_initcall(db641_init);

