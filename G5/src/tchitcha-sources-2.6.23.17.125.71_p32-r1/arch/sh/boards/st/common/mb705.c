/*
 * arch/sh/boards/st/common/mb705.c
 *
 * Copyright (C) 2008 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics STB peripherals board support.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/pio.h>
#include <linux/stm/soc.h>
#include <linux/stm/emi.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/stm/nand.h>
#include <linux/stm/soc_init.h>
#include <linux/mtd/physmap.h>
#include <linux/mtd/partitions.h>
#include <linux/spi/spi.h>
#include <linux/spi/flash.h>
#include <linux/bug.h>
#include <asm/processor.h>
#include <asm/irq-ilc.h>
#include "common.h"
#include "mb705-epld.h"

static DEFINE_SPINLOCK(misc_lock);
char mb705_rev = '?';

static struct platform_device mb705_gpio_led = {
	.name = "leds-gpio",
	.id = 1,
	.dev.platform_data = &(struct gpio_led_platform_data) {
		.num_leds = 1,
		.leds = (struct gpio_led[]) {
			{
				.name = "HB",
				.default_trigger = "heartbeat",
				.gpio = stpio_to_gpio(2, 0),
				.active_low = 1,
			},
		},
	},
};

static struct platform_device epld_device = {
	.name		= "epld",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.start	= 0x04800000,
			.end	= 0x048002ff,
			.flags	= IORESOURCE_MEM,
		}
	},
	.dev.platform_data = &(struct plat_epld_data) {
		 .opsize = 16,
	},
};

static struct platform_device mb705_display_device = {
	.name = "mb705-display",
	.id = -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.start	= 0x04800140,
			.end	= 0x048001bf,
			.flags	= IORESOURCE_MEM,
		}
	},
};

#ifndef CONFIG_CPU_SUBTYPE_STX7108
static struct platform_device mb705_fpbutton_device = {
	.name = "mb705-fpbutton",
	.id = -1,
	.num_resources	= 2,
	.resource	= (struct resource[]) {
		{
			.start	= ILC_EXT_IRQ(0),
			.end	= ILC_EXT_IRQ(0),
			.flags	= IORESOURCE_IRQ,
		}, {
			/* Mask for the EPLD status register */
			.name	= "mask",
			.start	= 1<<9,
			.end	= 1<<9,
			.flags	= IORESOURCE_IRQ,
		}
	},
};
#endif

static void set_vpp(struct map_info * info, int enable)
{
	u16 reg;

	spin_lock(&misc_lock);

	reg = epld_read(EPLD_EMI_MISC);
	if (enable)
		reg |= EPLD_EMI_MISC_NORFLASHVPPEN;
	else
		reg &= ~EPLD_EMI_MISC_NORFLASHVPPEN;
	epld_write(reg, EPLD_EMI_MISC);

	spin_unlock(&misc_lock);
}

static struct mtd_partition mtd_parts_table[3] = {
	{
		.name = "Boot firmware",
		.size = 0x00040000,
		.offset = 0x00000000,
	}, {
		.name = "Kernel",
		.size = 0x00100000,
		.offset = 0x00040000,
	}, {
		.name = "Root FS",
		.size = MTDPART_SIZ_FULL,
		.offset = 0x00140000,
	}
};

static struct physmap_flash_data physmap_flash_data = {
	.width		= 2,
	.set_vpp	= set_vpp,
	.nr_parts	= ARRAY_SIZE(mtd_parts_table),
	.parts		= mtd_parts_table
};

static struct platform_device physmap_flash = {
	.name		= "physmap-flash",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.start		= 0x00000000,	/* Can be overridden */
			.end		= 32*1024*1024 - 1,
			.flags		= IORESOURCE_MEM,
		}
	},
	.dev		= {
		.platform_data	= &physmap_flash_data,
	},
};

static struct mtd_partition mb705_serial_flash_mtd_parts[] = {
	{
		.name = "sflash_1",
		.size = 0x00080000,
		.offset = 0,
	}, {
		.name = "sflash_2",
		.size = MTDPART_SIZ_FULL,
		.offset = MTDPART_OFS_NXTBLK,
	},
};

static struct flash_platform_data mb705_serial_flash_data = {
	.name = "m25p80",
	.parts = mb705_serial_flash_mtd_parts,
	.nr_parts = ARRAY_SIZE(mb705_serial_flash_mtd_parts),
	.type = "m25p32",
};

static struct spi_board_info mb705_serial_flash[] =  {
	{
		/* .bus_num and .chip_select set by processor board */
		.modalias	= "m25p80",
		.max_speed_hz	= 500000,
		.platform_data	= &mb705_serial_flash_data,
		.mode		= SPI_MODE_3,
	},
};

static struct platform_device *mb705_devices[] __initdata = {
	&epld_device,
	&mb705_gpio_led,
	&mb705_display_device,
#ifndef CONFIG_CPU_SUBTYPE_STX7108
	&mb705_fpbutton_device,
#endif
	&physmap_flash,
};

static struct mtd_partition mb705_nand_flash_mtd_parts[] = {
	{
		.name	= "nand_1",
		.offset	= 0,
		.size 	= 0x00800000
	}, {
		.name	= "nand_2",
		.offset	= MTDPART_OFS_APPEND,
		.size	= MTDPART_SIZ_FULL
	},
};

static struct plat_stmnand_data mb705_nand_config = {
	.emi_withinbankoffset	= 0,
	.rbn_port		= -1,
	.rbn_pin		= -1,

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
	.flex_rbn_connected	= 1,
};

/* Platform data for STM_NAND_EMI/FLEX/AFM. (bank# may be updated later) */
static struct platform_device mb705_nand_flash =
	STM_NAND_DEVICE("stm-nand-flex", 1, &mb705_nand_config,
			mb705_nand_flash_mtd_parts,
			ARRAY_SIZE(mb705_nand_flash_mtd_parts),
			NAND_USE_FLASH_BBT);


#include <linux/delay.h>

static DEFINE_SPINLOCK(mb705_reset_lock);

void mb705_reset(int mask, unsigned long usdelay)
{
	u16 reg;

	spin_lock(&mb705_reset_lock);
	reg = epld_read(EPLD_EMI_RESET);
	reg |= mask;
	epld_write(reg, EPLD_EMI_RESET);
	spin_unlock(&mb705_reset_lock);

	udelay(usdelay);

	spin_lock(&mb705_reset_lock);
	reg = epld_read(EPLD_EMI_RESET);
	reg &= ~mask;
	epld_write(reg, EPLD_EMI_RESET);
	spin_unlock(&mb705_reset_lock);
}

static int __init mb705_init(void)
{
	int i;

	/* Valid for 7105, 7106, 7108 */
	BUG_ON(cpu_data->type != CPU_STX7105 &&
	       cpu_data->type != CPU_STX7106 &&
	       cpu_data->type != CPU_STX7108);

	/* We are't actually doing this early here... */
	epld_early_init(&epld_device);

	/* Check out the EPLDs */
	for (i=0; i<3; i++) {
		int ident_offset = (0x100 * i) + 0;
		int test_offset = (0x100 * i) + 2;
		u16 ident;
		u16 test;
		u16 mask = (i==0) ? 0xffff : 0xff;

		ident = epld_read(ident_offset);
		epld_write(0xab12+i, test_offset);
		test = epld_read(test_offset);

		printk(KERN_INFO
		       "mb705 %s_EPLD: board rev %c, EPLD rev %d, test %s\n",
		       (char*[3]){"EMI", "TS", "AUD" }[i],
		       ((ident >> 4) & 0xf) - 1 + 'A', ident & 0xf,
		       (((test ^ (0xab12+i)) & mask) == mask) ?
		        "passed" : "failed");
	}

	mb705_rev = ((epld_read(EPLD_EMI_IDENT) >> 4) & 0xf) - 1 + 'A';

	/* Determine whether NOR and NAND devices are swapped. */
	i = epld_read(EPLD_EMI_SWITCH);
	if (i & EPLD_EMI_SWITCH_BOOTFROMNOR) {
		u32 bank1_start = emi_bank_base(1);
		u32 bank2_start = emi_bank_base(2);
		physmap_flash.resource[0].start = bank1_start;
		physmap_flash.resource[0].end = bank2_start - 1;
		mb705_nand_flash.id = 0;
	}

	/*
	 * The MTD NAND code doesn't understand the concept of VPP,
	 * (or hardware write protect) so permanently enable it.
	 * Also disable NOR VPP enable just in case.
	 */
	i = epld_read(EPLD_EMI_MISC);
	i &= ~EPLD_EMI_MISC_NORFLASHVPPEN;
	i |= EPLD_EMI_MISC_NOTNANDFLASHWP;
	epld_write(i, EPLD_EMI_MISC);

	/* Interrupt routing.
	 * At the moment we only care about a small number of
	 * interrupts, so simply set up a static one-to-one routing.
	 *
	 * Interrupt sources:
	 *  0 : MAFE
	 *  1 : VOIP
	 *  2 : SPDIF out
	 *  3 : STRec status
	 *  4 : STEM0 (-> SysIrq(2) this matches the mb680 but active high)
	 *  5 : STEM1 (-> SysIrq(1) this matches the mb680 but active high))
	 *  6 : DVB
	 *  7 : DVB CD1
	 *  8 : DVB CD2
	 *  9 : FButton (-> SysIrq(0))
	 * 10 : EPLD intr in
	 */
	epld_write(0, EPLD_EMI_INT_PRI(9));
	epld_write(1, EPLD_EMI_INT_PRI(5));
	epld_write(2, EPLD_EMI_INT_PRI(4));
	epld_write((1<<4)|(1<<5)|(1<<9), EPLD_EMI_INT_MASK);

	/* Defined in procesor board, which knows the SoC and PIOs... */
	mbxxx_configure_nand_flash(&mb705_nand_flash);
	mbxxx_configure_serial_flash(mb705_serial_flash,
				     ARRAY_SIZE(mb705_serial_flash));

	return platform_add_devices(mb705_devices, ARRAY_SIZE(mb705_devices));
}
arch_initcall(mb705_init);
