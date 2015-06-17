/*
 * arch/sh/boards/st/setup.c
 *
 * Copyright (C) 2008 STMicroelectronics Limited
 * Author: Stuart Menefy (stuart.menefy@st.com)
 *
 * Copyright (C) 2012 WyPlay SAS.
 * frederic mazuel <fmazuel@wyplay.com>
 * thomas griozel <tgriozel@wyplay.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Samsung SMT-S5210-SDK support.
 */

#include <linux/platform_device.h>
#include <linux/stm/soc.h>
#include <linux/stm/pio.h>
#include <linux/phy.h>
#include <linux/delay.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/physmap.h>
#include <linux/mtd/partitions.h>
#include <linux/i2c.h>

#define STV0900_6110_MAIN_BOARD_CONTROLLER     (0xD0>>1)
#define STV0900_6110_DEVICE_NAME "STV0900_6110"

static struct i2c_board_info __initdata STV0900_6110_board_info[] = {
	{
		I2C_BOARD_INFO(STV0900_6110_DEVICE_NAME , STV0900_6110_MAIN_BOARD_CONTROLLER),
	},
	{
		.driver_name = "bcm3445",
		.type = "bcm3445",
		.addr = 0x6c,
		.platform_data = NULL,
	},
};

static struct i2c_board_info __initdata STV6417_board_info[] = {
	{
		.driver_name = "stv6417",
		.type = "stv6417",
		.addr = 0x4B,
	},
};


static int ascs[1] __initdata = {
	2 | (STASC_FLAG_NORTSCTS << 8),		/* ASC2 = UART */
};

#ifdef CONFIG_BPA2

#include <linux/bpa2.h>

static const char *LMI_SYS_partalias_256[] = {
        "BPA2_Region0",
        "v4l2-coded-video-buffers",
        "BPA2_Region1",
        "v4l2-video-buffers",
        "coredisplay-video",
        NULL
};

static const char *bigphys_partalias_256[] = {
        "gfx-memory-1",
        NULL
};

static struct bpa2_partition_desc bpa2_parts_table[] = {
        {
                .name   = "LMI_SYS",            /* 75MB */
                .start  = 0,
                .size   = 69 * 1024 * 1024,
                .flags  = 0,
                .aka    = LMI_SYS_partalias_256
        },
        {
                .name   = "gfx-memory-0",       /* 16MB */
                .start  = 0x50000000 - 0x01000000,
                .size   = 0x01000000,
                .flags  = 0,
                .aka    = NULL
        },
        {
                .name   = "bigphysarea",        /* 19MB */
                .start  = 0x50000000 - 0x01000000 - 0x01300000,
                .size   = 0x01300000,
                .flags  = 0,
                .aka    = bigphys_partalias_256
        },
};

#endif  /* CONFIG_BPA2 */

#ifdef CONFIG_SH_G5_SPLASH
# define ZREPLAY_IN_KERNEL
# include "splash.h"
#endif

#define register_read32(addr) (*(volatile unsigned int *)(addr))
#define ST7105_PTI_BASE_ADDRESS	0xFE230000
#define PTI_OFFSET		0x9984
#define REBOOT_FLAG_OFFSET	12

static void __init s5210_setup(char** cmdline_p)
{
#ifdef CONFIG_SH_G5_SPLASH
	if (register_read32(ST7105_PTI_BASE_ADDRESS + PTI_OFFSET + REBOOT_FLAG_OFFSET) != 1)
		splash();
#endif

	printk("SMT-S5210 board initialization\n");
	stx7105_early_device_init();
	stx7105_configure_asc(ascs, 1, 0);

#ifdef CONFIG_BPA2
	bpa2_init(bpa2_parts_table, ARRAY_SIZE(bpa2_parts_table));
#endif
}

static struct plat_ssc_data ssc_private_info = {
        .capability =
                ssc0_has(SSC_I2C_CAPABILITY | SSC_I2C_PIO) |
                ssc1_has(SSC_I2C_CAPABILITY | SSC_I2C_PIO) |
                ssc2_has(SSC_I2C_CAPABILITY | SSC_I2C_PIO) |
                ssc3_has(SSC_UNCONFIGURED),
        .routing =
                SSC2_SCLK_PIO3_4 | SSC2_MTSR_PIO3_5,
};

static struct mtd_partition mtd_parts_table[] = {
        {
                .name = "whole",
                .offset = 0x0,
                .size = 0x400000,
        },
        {
                .name = "nor",
                .offset = 0x0,
                .size = 0x3E0000,
        },
        {
                .name = "eeprom",
                .offset = 0x3E0000,
                .size = 0x20000,
        }
};

static struct physmap_flash_data physmap_flash_data = {
	.width		= 2,
	.nr_parts	= ARRAY_SIZE(mtd_parts_table),
	.parts		= mtd_parts_table
};

static struct platform_device physmap_flash = {
	.name           = "physmap-flash",
	.id	            = -1,
	.num_resources  = 1,
	.resource       = (struct resource[]) {
		{
			.start	= 0x00000000,
			.end	= 32*1024*1024 - 1,
			.flags	= IORESOURCE_MEM,
		}
	},
	.dev            = {
		.platform_data	= &physmap_flash_data,
	},
};


static struct usb_init_data usb_init[2] __initdata = {
        {
                .oc_en = 0,
                .pwr_en = 0,
        }, {
                .oc_en = 0,
                .pwr_en = 0,
        }
};

static struct plat_stmmacphy_data phy_private_data = {
	.bus_id = 0,
	.phy_addr = -1,
	.phy_mask = 0,
	.interface = PHY_INTERFACE_MODE_MII,
};

static struct platform_device s5210_phy_device = {
	.name		= "stmmacphy",
	.id		= 0,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.name	= "phyirq",
			.start	= -1,/*FIXME, should be ILC_EXT_IRQ(6), */
			.end	= -1,
			.flags	= IORESOURCE_IRQ,
		},
	},
	.dev = {
		.platform_data = &phy_private_data,
	}
};

static struct platform_device *s5210_devices[] __initdata = {
	&physmap_flash,
	&s5210_phy_device,
};

int (*bcm3445_set_ant_bridge_mode)(u8 activate_bridge) = NULL;

int set_ant_bridge_mode(u8 activate_bridge)
{
	if (bcm3445_set_ant_bridge_mode)
		return bcm3445_set_ant_bridge_mode(activate_bridge);
	else
		return -ENXIO;
}

EXPORT_SYMBOL(bcm3445_set_ant_bridge_mode);
EXPORT_SYMBOL(set_ant_bridge_mode);

static int s5210_frontend_reset(void)
{
	struct stpio_pin *rstn_demod;

	rstn_demod = stpio_request_pin(1, 4, "RESET_DEMOD_", STPIO_OUT);
	/* STM STV0900: pin RESET_DEMOD_ must remain active (low) until at least 3 ms
	 * after the last power supply has stabilized, then transit from low to
	 * high.
	 * TODO check rise time of power supply line further to rising edge of
	 * LOW_PW_FEnot */
	if (rstn_demod) {
		stpio_set_pin(rstn_demod, 0);
		mdelay(5);
		stpio_set_pin(rstn_demod, 1);
		stpio_free_pin(rstn_demod);
		printk("STV0900 is now out of reset\n");
		return 1;
	}
	return 0;
}

static int __init device_init(void)
{
	stx7105_configure_ssc(&ssc_private_info);

	s5210_frontend_reset();

	stx7105_configure_usb(0, &usb_init[0]);
	stx7105_configure_usb(1, &usb_init[1]);

	stx7105_configure_lirc(NULL);
	stx7105_configure_tsin(0, 1);
	stx7105_configure_tsin(2, 1);

	stx7105_configure_ethernet(0, stx7105_ethernet_mii, 0, 0, 0, 0);

	printk("i2c_init\n");
	i2c_register_board_info(0, STV0900_6110_board_info, ARRAY_SIZE(STV0900_6110_board_info));
	i2c_register_board_info(1, STV6417_board_info, ARRAY_SIZE(STV6417_board_info));

	return platform_add_devices(s5210_devices, ARRAY_SIZE(s5210_devices));
}
arch_initcall(device_init);

static void __iomem *s5210_ioport_map(unsigned long port, unsigned int size)
{
	/* However picking somewhere safe isn't as easy as you might think.
	 * I used to use external ROM, but that can cause problems if you are
	 * in the middle of updating Flash. So I'm now using the processor core
	 * version register, which is guaranted to be available, and non-writable.
	 */
	return (void __iomem *)CCN_PVR;
}

static void __init s5210_init_irq(void)
{
}

static void s5210_reset(void)
{
	long jump2_flag = 0xfe239970;

#define DEFAULT_RESET_CHAIN     0xa8c
#define SYS_CONFIG_BASE         0xfe001000
#define SYS_CONFIG_9            (void __iomem *)(SYS_CONFIG_BASE + 0x124)

	/* Restore the reset chain. Note that we cannot use the sysconfig
         * API here, as it calls kmalloc(GFP_KERNEL) but we get here with
         * interrupts disabled.
         */
        writel(DEFAULT_RESET_CHAIN, SYS_CONFIG_9);

        *( (unsigned int *)(jump2_flag + 4) ) = 0;
}

struct sh_machine_vector mv_s5210 __initmv = {
	.mv_name		= "s5210",
	.mv_setup		= s5210_setup,
	.mv_nr_irqs		= NR_IRQS,
	.mv_init_irq		= s5210_init_irq,
	.mv_ioport_map		= s5210_ioport_map,
	.mv_reset       = s5210_reset,
};

