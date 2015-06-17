/*
 * arch/sh/boards/st/common/mb588.c
 *
 * Copyright (C) 2007 STMicroelectronics Limited
 * Author: Pawel Moll <pawel.moll@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics NAND Flash STEM board
 *
 * This code assumes that STEM_notCS0 line is used (J1 = 1-2).
 *
 * J2 may be left totally unfitted.
 *
 * If J3 is closed NAND chip is write protected, so if you wish to modify
 * its content...
 *
 * Some additional main board setup may be required to use proper CS signal
 * signal - see "include/asm-sh/<board>/stem.h" for more information.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/nand.h>
#include <linux/stm/nand.h>
#include <linux/stm/soc.h>
#include <linux/stm/soc_init.h>
#include <linux/stm/emi.h>
#include <asm/mach/stem.h>

static struct mtd_partition nand_parts[] = {
	{
		.name	= "NAND root",
		.offset	= 0,
		.size 	= 0x00800000
	}, {
		.name	= "NAND home",
		.offset	= MTDPART_OFS_APPEND,
		.size	= MTDPART_SIZ_FULL
	},
};

static struct plat_stmnand_data nand_config = {
	.emi_withinbankoffset	= STEM_CS0_OFFSET,
	.rbn_port		= -1,
	.rbn_pin		= -1,

	/* Timing data for SoCs using STM_NAND_EMI/FLEX/AFM drivers */
	.timing_data = &(struct nand_timing_data) {
		.sig_setup	= 50,		/* times in ns */
		.sig_hold	= 50,
		.CE_deassert	= 0,
		.WE_to_RBn	= 100,
		.wr_on		= 10,
		.wr_off		= 40,
		.rd_on		= 10,
		.rd_off		= 40,
		.chip_delay	= 30,		/* in us */
	},
	.flex_rbn_connected	= 0,
};

/* For SoCs migrated to STM_NAND_EMI/FLEX/AFM drivers, setup template platform
 * device structure.  SoC setup will configure SoC specific data.  Use
 * 'stm-nand-emi/flex/afm.x' as ID for specifying MTD partitions on the kernel
 * command line.
 */
static struct platform_device nand_device =
	STM_NAND_DEVICE("stm-nand-emi", STEM_CS0_BANK, &nand_config,
			nand_parts, ARRAY_SIZE(nand_parts), 0);

static int __init mb588_init(void)
{
#if defined(CONFIG_CPU_SUBTYPE_STX7105)
	stx7105_configure_nand(&nand_device);
#elif defined(CONFIG_CPU_SUBTYPE_STX7111)
	stx7111_configure_nand(&nand_device);
#elif defined(CONFIG_CPU_SUBTYPE_STX7200)
	stx7200_configure_nand(&nand_device);
#else
#	error Unsupported SOC.
#endif
	return 0;
}
arch_initcall(mb588_init);

