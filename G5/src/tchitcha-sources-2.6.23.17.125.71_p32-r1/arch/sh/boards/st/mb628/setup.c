/*
 * arch/sh/boards/st/mb628/setup.c
 *
 * Copyright (C) 2008 STMicroelectronics Limited
 * Author: Stuart Menefy (stuart.menefy@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics STx7141 Mboard support.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/stm/pio.h>
#include <linux/stm/soc.h>
#include <linux/workqueue.h>
#include <linux/stm/emi.h>
#include <linux/spi/spi.h>
#include <linux/spi/spi_bitbang.h>
#include <linux/spi/flash.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/physmap.h>
#include <linux/mtd/partitions.h>
#include <linux/bpa2.h>
#include <linux/phy.h>
#include <linux/lirc.h>
#include <linux/gpio_keys.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <asm/irq-ilc.h>
#include <asm/irl.h>
#include <asm/mb628/epld.h>
#include <sound/stm.h>
#include "../common/common.h"

const char *LMI_VID_partalias[] = { "BPA2_Region1", "coredisplay-video", "gfx-memory", "v4l2-video-buffers", NULL };
const char *LMI_SYS_partalias[] = { "BPA2_Region0", "bigphysarea", "v4l2-coded-video-buffers", NULL };

static struct bpa2_partition_desc bpa2_parts_table[] = {
	{
		.name  = "LMI_VID",
		.start = 0x80800000,
		.size  = 0x07800000,
		.flags = 0,
		.aka   = LMI_VID_partalias
	},
	{
		.name  = "LMI_SYS",
		.start = 0,
		.size  = 0x05000000,
		.flags = 0,
		.aka   = LMI_SYS_partalias
	}
};

#define FLASH_NOR
#ifdef CONFIG_STMMAC_DUAL_MAC
#define ENABLE_GMAC0
#endif
static struct platform_device epld_device;

static int ascs[] __initdata = {
	1 | (ASC1_PIO10 << 8),	/* PIO10 muxed with GMAC0 & DVO */
	2 | (ASC2_PIO6  << 8)	/* PIO6 muxed with TS(NIM) and OOB_??? */
};

static void __init mb628_setup(char **cmdline_p)
{
	u8 test;

	printk(KERN_INFO "STMicroelectronics STx7141 Mboard initialisation\n");

	stx7141_early_device_init();
	stx7141_configure_asc(ascs, ARRAY_SIZE(ascs), 0);

	epld_early_init(&epld_device);

	epld_write(0xab, EPLD_TEST);
	test = epld_read(EPLD_TEST);
	printk(KERN_INFO "mb628 EPLD version %ld, test %s\n",
	       epld_read(EPLD_IDENT),
	       (test == (u8)(~0xab)) ? "passed" : "failed");

	bpa2_init(bpa2_parts_table, ARRAY_SIZE(bpa2_parts_table));
}

static struct plat_stm_pwm_data pwm_private_info = {
	.flags		= PLAT_STM_PWM_OUT1,
};

/* Chip-select for SSC1-SPI bus.  Serial FLASH is only device on this bus */
static void spi_chipselect_ssc1(void *_spi, int value)
{
	struct spi_device *spi = _spi;
	u8 reg;

	/* Serial FLASH is on chip_select '1' */
	if (spi->chip_select == 1) {

		reg = epld_read(EPLD_ENABLE);

		if (value == BITBANG_CS_ACTIVE)
			if (spi->mode & SPI_CS_HIGH)
				reg |= EPLD_ENABLE_SPI_NOTCS;
			else
				reg &= ~EPLD_ENABLE_SPI_NOTCS;
		else
			if (spi->mode & SPI_CS_HIGH)
				reg &= ~EPLD_ENABLE_SPI_NOTCS;
			else
				reg |= EPLD_ENABLE_SPI_NOTCS;
		epld_write(reg, EPLD_ENABLE);
	}
}

static struct plat_ssc_data ssc_private_info = {
	.capability  =
		ssc0_has(SSC_SPI_CAPABILITY)	/* SSC1 */	|
		ssc1_has(SSC_SPI_CAPABILITY)	/* SSC2 */	|
		ssc2_has(SSC_I2C_CAPABILITY)	/* SSC3 */	|
		ssc3_has(SSC_I2C_CAPABILITY)	/* SSC4 */	|
		ssc4_has(SSC_I2C_CAPABILITY)	/* SSC5 */	|
		ssc5_has(SSC_I2C_CAPABILITY)	/* SSC6 */	|
		ssc6_has(SSC_I2C_CAPABILITY),	/* SSC7 */
	.spi_chipselects = {
		[0] = spi_chipselect_ssc1,
	},
};

/* MTD partitions for Serial FLASH device */
static struct mtd_partition serialflash_partitions[] = {
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

/* Serial FLASH is type 'm25p32', handled by 'm25p80' SPI Protocol driver */
static struct flash_platform_data serialflash_data = {
	.name = "m25p80",
	.parts = serialflash_partitions,
	.nr_parts = ARRAY_SIZE(serialflash_partitions),
	.type = "m25p32",
};

/* SPI 'board_info' to register serial FLASH protocol driver */
static struct spi_board_info spi_serialflash[] =  {
	{
		.modalias	= "m25p80",
		.bus_num	= 0,
		.chip_select	= 1,
		.max_speed_hz	= 5000000,
		.platform_data	= &serialflash_data,
		.mode		= SPI_MODE_3,
	},
};


#ifdef FLASH_NOR
/* J69 must be in position 2-3 to enable the on-board Flash devices (both
 * NOR and NAND) rather than STEM). */
/* J89 and J84 must be both in position 1-2 to avoid shorting A15 */
/* J70 must be in the 2-3 position to enable NOR Flash */

static void set_vpp(struct map_info *info, int enable)
{
	epld_write((enable ? EPLD_FLASH_NOTWP : 0) | EPLD_FLASH_NOTRESET,
		   EPLD_FLASH);
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
			.start		= 0x00000000,
			.end		= 32*1024*1024 - 1,
			.flags		= IORESOURCE_MEM,
		}
	},
	.dev		= {
		.platform_data	= &physmap_flash_data,
	},
};

#else

/* J70 must be in the 1-2 position to enable NAND Flash */
static struct mtd_partition nand_partitions[] = {
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

static struct plat_stmnand_data mb628_nand_config = {
	.emi_bank		= 0,
	.emi_withinbankoffset	= 0,

	/* Timings for NAND512W3A */
	.emi_timing_data = &(struct emi_timing_data) {
		.rd_cycle_time	 = 40,		 /* times in ns */
		.rd_oee_start	 = 0,
		.rd_oee_end	 = 10,
		.rd_latchpoint	 = 10,
		.busreleasetime  = 0,

		.wr_cycle_time	 = 40,
		.wr_oee_start	 = 0,
		.wr_oee_end	 = 10,

		.wait_active_low = 0,
	},

	.chip_delay		= 40,		/* time in us */
	.mtd_parts		= nand_partitions,
	.nr_parts		= ARRAY_SIZE(nand_partitions),
};
#endif

static int mb628_phy_reset(void *bus)
{
	u8 reg;
	static int first = 1;

	/* Both PHYs share the same reset signal, only act on the first. */
	if (!first)
		return 1;
	first = 0;

	reg = epld_read(EPLD_RESET);
	reg &= ~EPLD_RESET_MII;
	epld_write(reg, EPLD_RESET);
	udelay(150);
	reg |= EPLD_RESET_MII;
	epld_write(reg, EPLD_RESET);

	/* DP83865 (PHY chip) has a looong initialization
	 * procedure... Let's give him some time to settle down... */
	udelay(1000);

	/*
	 * The SMSC LAN8700 requires a 21mS delay after reset. This
	 * matches the power on reset signal period, which should only
	 * be applied after power on, but experimentally appears to be
	 * applied post reset as well.
	 */
	mdelay(25);

	return 1;
}

/*
 * Several things need to be configured to use the GMAC0 with the
 * mb539 - SMSC LAN8700 PHY board:
 *
 * - normally the PHY's internal 1V8 regulator is used, which is
 *   is enabled at PHY power up (not reset) by sampling RXCLK/REGOFF.
 *   It appears that the STx7141's internal pull up resistor on this
 *   signal is enabled at power on, defeating the internal pull down
 *   in the SMSC device. Thus it is necessary to fix an external
 *   pull down resistor to RXCLK/REGOFF. 10K appears to be sufficient.
 *
 *   Alternativly fitting J2 on the mb539 supplies power from an
 *   off-chip regulator, working around this problem.
 *
 * - various signals are muxed with the MII pins (as well as DVO_DATA).
 *   + ASC1_RXD and ASC1_RTS, so make sure J101 is set to 2-3. This
 *     allows the EPLD to disable the level converter.
 *   + PCIREQ1 and PCIREQ2 need to be disabled by removing J104 and J98
 *     (near the PCI slot).
 *   + SYSITRQ1 needs to be disabled, which requires removing R232
 *     (near CN17). See DDTS INSbl29196 for details.
 *   + PCIGNT2 needs to be disabled. This can be done either by removing
 *     R241, or by ensuring that jumper J89 is not in position 1-2 (by
 *     either removing it completely or putting it in position 2-3).
 *
 * - other jumper and switch settings for the mb539:
 *   + J1 fit 1-2 (use on board crystal)
 *   + SW1: 1:on, 2:off, 3:off, 4:off
 *   + SW2: 1:off, 2:off, 3:off, 4:off
 *
 * - For reliable SMI signalling it is necessary to have a
 *   pull up resistor on the MDIO signal. This can be done by
 *   installing R3 on the mb539 which is normally a DNF.
 *
 * - to use the MDINT signal, R148 needs to be in position 1-2.
 *   To disable this, replace the irq with -1 in the data below.
 */

static struct plat_stmmacphy_data phy_private_data[2] = {
{
	/* GMAC0: MII connector CN17. We assume a mb539 (SMSC 8700). */
	.bus_id = 0,
	.phy_addr = -1,
	.phy_mask = 0,
	.interface = PHY_INTERFACE_MODE_MII,
	.phy_reset = mb628_phy_reset,
}, {
	/* GMAC1: on board NatSemi PHY */
	.bus_id = 1,
	.phy_addr = 1,
	.phy_mask = 0,
	.interface = PHY_INTERFACE_MODE_GMII,
	.phy_reset = mb628_phy_reset,
} };

static struct platform_device phy_devices[2] = {
{
	.name		= "stmmacphy",
	.id		= 0,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.name	= "phyirq",
			.start	= ILC_IRQ(43), /* See MDINT above */
			.end	= ILC_IRQ(43),
			.flags	= IORESOURCE_IRQ,
		},
	},
	.dev = {
		.platform_data = &phy_private_data[0],
	}
}, {
	.name		= "stmmacphy",
	.id		= 1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.name	= "phyirq",
			.start	= -1,/* ILC_IRQ(42) but MODE pin clash*/
			.end	= -1,
			.flags	= IORESOURCE_IRQ,
		},
	},
	.dev = {
		.platform_data = &phy_private_data[1],
	}
} };

static struct platform_device epld_device = {
	.name		= "epld",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.start	= 0x05000000,
			/* Minimum size to ensure mapped by PMB */
			.end	= 0x05000000+(8*1024*1024)-1,
			.flags	= IORESOURCE_MEM,
		}
	},
	.dev.platform_data = &(struct plat_epld_data) {
		 .opsize = 8,
	},
};

#ifdef CONFIG_SND
/* CS8416 SPDIF to I2S converter (IC14) */
static struct platform_device mb628_snd_spdif_input = {
	.name = "snd_conv_dummy",
	.id = -1,
	.dev.platform_data = &(struct snd_stm_conv_dummy_info) {
		.group = "SPDIF Input",

		.source_bus_id = "snd_pcm_reader.0",
		.channel_from = 0,
		.channel_to = 1,
		.format = SND_STM_FORMAT__I2S |
				SND_STM_FORMAT__SUBFRAME_32_BITS,
	},
};

static struct platform_device mb628_snd_external_dacs = {
	.name = "snd_conv_epld",
	.id = -1,
	.dev.platform_data = &(struct snd_stm_conv_epld_info) {
		.group = "External DACs",

		.source_bus_id = "snd_pcm_player.0",
		.channel_from = 0,
		.channel_to = 9,
		.format = SND_STM_FORMAT__I2S |
				SND_STM_FORMAT__SUBFRAME_32_BITS,
		.oversampling = 256,

		.mute_supported = 1,
		.mute_offset = EPLD_AUDIO,
		.mute_mask = EPLD_AUDIO_PCMDAC1_SMUTE |
				EPLD_AUDIO_PCMDAC2_SMUTE,
		.mute_value = EPLD_AUDIO_PCMDAC1_SMUTE |
				EPLD_AUDIO_PCMDAC2_SMUTE,
		.unmute_value = 0,
	},
};
#endif

static struct platform_device *mb628_devices[] __initdata = {
	&epld_device,
	&physmap_flash,
	&phy_devices[0],
	&phy_devices[1],
#ifdef CONFIG_SND
	&mb628_snd_spdif_input,
	&mb628_snd_external_dacs,
#endif
};

/* Configuration based on Futarque-RC signals train. */
lirc_scd_t lirc_scd = {
	.code = 0x3FFFC028,
	.nomtime = 0x1f4,
	.noiserecov = 0,
};

static int __init device_init(void)
{
	/*
	 * Can't enable PWM output without conflicting with either
	 * SSC6 (audio) or USB1A OC (which is disabled because it is broken,
	 * but would still result in contention).
	 *
	 * stx7141_configure_pwm(&pwm_private_info);
	 */
	stx7141_configure_ssc(&ssc_private_info);
	stx7141_configure_usb(0);

	/* This requires fitting jumpers J52A 1-2 and J52B 4-5 */
	stx7141_configure_usb(1);

	if (cpu_data->cut_major > 1) {
		stx7141_configure_usb(2);
		stx7141_configure_usb(3);

		stx7141_configure_sata();
	}

#ifdef ENABLE_GMAC0
	/* Must disable ASC1 if using GMII0 */
	epld_write(epld_read(EPLD_ENABLE) | EPLD_ASC1_EN | EPLD_ENABLE_MII0,
		   EPLD_ENABLE);

	/* Configure GMII0 MDINT for active low */
	set_irq_type(ILC_IRQ(43), IRQ_TYPE_LEVEL_LOW);

	stx7141_configure_ethernet(0, 0, 0, 0);
#endif

	epld_write(epld_read(EPLD_ENABLE) | EPLD_ENABLE_MII1, EPLD_ENABLE);
	stx7141_configure_ethernet(1, 0, 0, 1);

	/*
	 * This assumes the default jumper settings:
	 * J99  1-2 Enable IR input
	 * J100 2-3 Select IR PPM In
	 * J105 1-2 Select IR PPM Out
	 * Any other configuration will require moving these.
	 */
	stx7141_configure_lirc(&(struct stx7141_lirc_config){
		.rx_mode = stx7141_lirc_rx_mode_ir,
		.tx_enabled = 1,
		.scd = &lirc_scd, });

#ifndef FLASH_NOR
	stx7141_configure_nand(&mb628_nand_config);
	/* The MTD NAND code doesn't understand the concept of VPP,
	 * (or hardware write protect) so permanently enable it.
	 */
	epld_write(EPLD_FLASH_NOTWP | EPLD_FLASH_NOTRESET, EPLD_FLASH);
#endif

#ifdef CONFIG_SND
	/* Audio peripherals
	 *
	 * WARNING! Board rev. A has swapped silkscreen labels of J16 & J32!
	 *
	 * The recommended audio setup of MB628 is as follows:
	 * SW2[1..4] - [ON, OFF, OFF, ON]
	 * SW5[1..4] - [OFF, OFF, OFF, OFF]
	 * SW3[1..4] - [OFF, OFF, ON, OFF]
	 * SW12[1..4] - [OFF, OFF, OFF, OFF]
	 * SW13[1..4] - [OFF, OFF, OFF, OFF]
	 * J2 - 2-3
	 * J3 - 1-2
	 * J6 - 1-2
	 * J7 - 1-2
	 * J8 - 1-2
	 * J12 - 1-2
	 * J16-A - 1-2, J16-B - 1-2
	 * J23-A - 2-3, J23-B - 2-3
	 * J26-A - 1-2, J26-B - 2-3
	 * J34-A - 1-2, J34-B - 2-3
	 * J41-A - 3-2, J41-B - 3-2
	 *
	 * Additionally the audio EPLD should be updated to the latest
	 * available release.
	 *
	 * With such settings the audio outputs layout presents as follows:
	 *
	 * +--------------------------------------+
	 * |                                      |
	 * |  (S.I)   (1.R)  (1.L)  (0.4)  (0.3)  | TOP
	 * |                                      |
	 * |  (---)   (0.2)  (0.1)  (0.10) (0.9)  |
	 * |                                      |
	 * |  (S.O)   (0.6)  (0.5)  (0.8)  (0.7)  | BOTTOM
	 * |                                      |
	 * +--------------------------------------+
	 *     CN6     CN5    CN4    CN3     CN2
	 *
	 * where:
	 *   - S.I - SPDIF input - PCM Reader #0
	 *   - S.O - SPDIF output - SPDIF Player (HDMI)
	 *   - 1.R, 1.L - audio outputs - PCM Player #1, channel L(1)/R(2)
	 *   - 0.1-10 - audio outputs - PCM Player #0, channels 1 to 10
	 */

	/* As digital audio outputs are now GPIOs, we have to claim them... */
	stx7141_configure_audio_pins(5, 0, 1, 1, 1);

	/* We use both DACs to get full 10-channels output from
	 * PCM Player #0 (EPLD muxing mode #1) */
	{
		unsigned int value = epld_read(EPLD_AUDIO);

		value &= ~(EPLD_AUDIO_AUD_SW_CTRL_MASK <<
				EPLD_AUDIO_AUD_SW_CTRL_SHIFT);
		value |= 0x1 << EPLD_AUDIO_AUD_SW_CTRL_SHIFT;

		epld_write(value, EPLD_AUDIO);
	}
#endif
	spi_register_board_info(spi_serialflash, ARRAY_SIZE(spi_serialflash));

	return platform_add_devices(mb628_devices, ARRAY_SIZE(mb628_devices));
}
arch_initcall(device_init);

static void __iomem *mb628_ioport_map(unsigned long port, unsigned int size)
{
	/*
	 * No IO ports on this device, but to allow safe probing pick
	 * somewhere safe to redirect all reads and writes.
	 */
	return (void __iomem *)CCN_PVR;
}

static void __init mb628_init_irq(void)
{
}

struct sh_machine_vector mv_mb628 __initmv = {
	.mv_name		= "mb628",
	.mv_setup		= mb628_setup,
	.mv_nr_irqs		= NR_IRQS,
	.mv_init_irq		= mb628_init_irq,
	.mv_ioport_map		= mb628_ioport_map,
};
