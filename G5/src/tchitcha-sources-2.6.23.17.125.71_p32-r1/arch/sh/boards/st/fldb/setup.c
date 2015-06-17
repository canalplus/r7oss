/*
 * arch/sh/boards/st/fldb/setup.c
 *
 * Copyright (C) 2009 STMicroelectronics Limited
 * Author: Pawel Moll (pawel.moll@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/leds.h>
#include <linux/lirc.h>
#include <linux/phy.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/physmap.h>
#include <linux/mtd/partitions.h>
#include <linux/spi/spi.h>
#include <linux/spi/flash.h>
#include <linux/stm/emi.h>
#include <linux/stm/pio.h>
#include <linux/stm/nand.h>
#include <linux/stm/soc.h>
#include <linux/stm/soc_init.h>
#include <linux/stm/sysconf.h>
#include <sound/stm.h>
#include <asm/irq-ilc.h>
#include "../common/common.h"



#define FLDB_GPIO_RESET_OUTN stpio_to_gpio(11, 6)
#define FLDB_GPIO_PCI_IDSEL stpio_to_gpio(16, 2)



static int ascs[2] __initdata = { 1, 2 | (STASC_FLAG_NORTSCTS << 8) };

static void __init fldb_setup(char **cmdline_p)
{
	printk(KERN_INFO "STMicroelectronics Freeman 510 Development Board "
			"initialisation\n");

	fli7510_early_device_init();
	fli7510_configure_asc(ascs, 2, 0);
}



static struct plat_ssc_data fldb_ssc_private_info = {
	.capability  =
		ssc0_has(SSC_I2C_CAPABILITY) |
		ssc1_has(SSC_I2C_CAPABILITY) |
		ssc2_has(SSC_I2C_CAPABILITY) |
		ssc3_has(SSC_I2C_CAPABILITY) |
		ssc4_has(SSC_SPI_CAPABILITY),
};



static struct plat_stm_pwm_data fldb_pwm_private_info = {
	.flags = PLAT_STM_PWM_OUT0 |
#if 0
		/* Connected to DF1 LED, currently used as a
		 * GPIO-controlled one (see below) */
		PLAT_STM_PWM_OUT1 |
#endif
		PLAT_STM_PWM_OUT2 |
		PLAT_STM_PWM_OUT3,
};



static struct platform_device fldb_led_df1 = {
	.name = "leds-gpio",
	.id = -1,
	.dev.platform_data = &(struct gpio_led_platform_data) {
		.num_leds = 1,
		.leds = (struct gpio_led[]) {
			{
				.name = "DF1 orange",
				.default_trigger = "heartbeat",
				.gpio = stpio_to_gpio(8, 5),
			},
		},
	},
};



/* UM4 = STA333ML */
static struct platform_device fldb_speakers = {
	.name = "snd_conv_gpio",
	.id = 0,
	.dev.platform_data = &(struct snd_stm_conv_gpio_info) {
		.group = "Speakers",

		.source_bus_id = "snd_pcm_player.0",
		.channel_from = 0,
		.channel_to = 1,
		.format = SND_STM_FORMAT__LEFT_JUSTIFIED |
				SND_STM_FORMAT__SUBFRAME_32_BITS,
		.oversampling = 256,

		.mute_supported = 1,
		.mute_gpio = stpio_to_gpio(26, 0),
		.mute_value = 0,
	},
};



static struct plat_stmmacphy_data fldb_phy_private_data = {
	.bus_id = 0,
	.phy_addr = 1,
	.phy_mask = 0,
	.interface = PHY_INTERFACE_MODE_MII,
};

static struct platform_device fldb_phy_device = {
	.name = "stmmacphy",
	.id = 0,
	.num_resources = 1,
	.resource = (struct resource[]) {
		{
			.name = "phyirq",
			.start = -1, /* FIXME */
			.end = -1,
			.flags = IORESOURCE_IRQ,
		},
	},
	.dev.platform_data = &fldb_phy_private_data,
};



static struct mtd_partition fldb_nor_flash_mtd_parts[] = {
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

static struct platform_device fldb_nor_flash = {
	.name = "physmap-flash",
	.id = -1,
	.num_resources = 1,
	.resource = (struct resource[]) {
		{
			.start = 0x00000000, /* May be overridden */
			.end = 32 * 1024 * 1024 - 1,
			.flags = IORESOURCE_MEM,
		}
	},
	.dev.platform_data = &(struct physmap_flash_data) {
		.width = 2,
		.nr_parts = ARRAY_SIZE(fldb_nor_flash_mtd_parts),
		.parts = fldb_nor_flash_mtd_parts,
	},
};

static struct mtd_partition fldb_serial_flash_mtd_parts[] = {
	{
		.name = "SerialFlash_1",
		.size = 0x00080000,
		.offset = 0,
	}, {
		.name = "SerialFlash_2",
		.size = MTDPART_SIZ_FULL,
		.offset = MTDPART_OFS_NXTBLK,
	},
};


static struct flash_platform_data fldb_serial_flash_data = {
	.name = "m25p80",
	.parts = fldb_serial_flash_mtd_parts,
	.nr_parts = ARRAY_SIZE(fldb_serial_flash_mtd_parts),
	.type = "m25p32",
};

/* SPI 'board_info' to register serial FLASH protocol driver */
static struct spi_board_info fldb_serial_flash[] =  {
	{
		.modalias	= "m25p80",
		.bus_num	= 0,
		.chip_select	= spi_set_cs(17, 4),
		.max_speed_hz	= 7000000,
		.platform_data	= &fldb_serial_flash_data,
		.mode		= SPI_MODE_3,
	},
};

static struct mtd_partition fldb_nand_flash_mtd_parts[] = {
	{
		.name   = "NANDFlash_1",
		.offset = 0,
		.size   = 0x00800000
	}, {
		.name   = "NANDFlash_2",
		.offset = MTDPART_OFS_APPEND,
		.size   = MTDPART_SIZ_FULL
	},
};

static struct plat_stmnand_data fldb_nand_config = {
	.emi_withinbankoffset   = 0,
	.rbn_port               = -1,
	.rbn_pin                = -1,

	.timing_data = &(struct nand_timing_data) {
		.sig_setup      = 50,           /* times in ns */
		.sig_hold       = 50,
		.CE_deassert    = 0,
		.WE_to_RBn      = 100,
		.wr_on          = 10,
		.wr_off         = 40,
		.rd_on          = 10,
		.rd_off         = 40,
		.chip_delay     = 50,           /* in us */
	},
	.flex_rbn_connected     = 1,
};

static struct platform_device fldb_nand_flash =
STM_NAND_DEVICE("stm-nand-flex", 1, &fldb_nand_config,
		fldb_nand_flash_mtd_parts,
		ARRAY_SIZE(fldb_nand_flash_mtd_parts),
		NAND_USE_FLASH_BBT);

static struct pci_config_data fldb_pci_config = {
	.pci_irq = {
		PCI_PIN_DEFAULT,
		PCI_PIN_DEFAULT,
		PCI_PIN_UNUSED,
		PCI_PIN_UNUSED
	},
	.serr_irq = PCI_PIN_DEFAULT,
	.idsel_lo = 30,
	.idsel_hi = 30,
	.req_gnt = {
		PCI_PIN_DEFAULT,
		PCI_PIN_UNUSED,
		PCI_PIN_UNUSED,
		PCI_PIN_UNUSED
	},
	.pci_clk = 33333333,
	.pci_reset_gpio = stpio_to_gpio(16, 5),
};

int pcibios_map_platform_irq(struct pci_dev *dev, u8 slot, u8 pin)
{
       /* We can use the standard function on this board */
       return fli7510_pcibios_map_platform_irq(&fldb_pci_config, pin);
}




static struct platform_device *fldb_devices[] __initdata = {
	&fldb_led_df1,
	&fldb_speakers,
	&fldb_phy_device,
	&fldb_nor_flash,
};



static int __init fldb_device_init(void)
{
	struct sysconf_field *sc;
	unsigned long nor_flash_base;
	unsigned long emi_conf[4];

	/* If the mode pins are configured to boot from NOR (SW4-SW6 all
	 * off), the NOR flash CS is assigned to EMI_CSA, so it is placed
	 * in EMI Bank 0. Otherwise, it's in bank 1 (and the NAND chip
	 * is in the first one). */
	sc = sysconf_claim(CFG_MODE_PIN_STATUS, 4, 9, "SW4-SW6 state");
	switch (sysconf_read(sc)) {
	case 0x00:
	case 0x20:
		nor_flash_base = emi_bank_base(0);
		break;
	default:
		nor_flash_base = emi_bank_base(1);
		fldb_nand_flash.id = 0;

		/* Configure EMI Bank1 for NOR */
		emi_conf[0] = 0x001126d1;	/* Settings taken from Bank 0 */
		emi_conf[1] = 0x8d200000;	/* (Freeman targetpack 0.5)   */
		emi_conf[2] = 0x9a200000;
		emi_conf[3] = 0x0400021d;
		emi_bank_configure(1, emi_conf);

		break;
	}
	fldb_nor_flash.resource[0].start += nor_flash_base;
	fldb_nor_flash.resource[0].end += nor_flash_base;
	sysconf_release(sc);

	/* This is a board-level reset line, which goes to the
	 * Ethernet PHY, audio amps & number of extension connectors */
	if (gpio_request(FLDB_GPIO_RESET_OUTN, "RESET_OUTN") == 0) {
		gpio_direction_output(FLDB_GPIO_RESET_OUTN, 0);
		udelay(10000); /* 10ms is the Ethernet PHY requirement */
		gpio_set_value(FLDB_GPIO_RESET_OUTN, 1);
	} else {
		printk(KERN_ERR "fldb: Failed to claim RESET_OUTN PIO!\n");
	}

	/* The IDSEL line is connected to PIO16.2 only... Luckily
	 * there is just one slot, so we can just force 1... */
	if (gpio_request(FLDB_GPIO_PCI_IDSEL, "PCI_IDSEL") == 0)
		gpio_direction_output(FLDB_GPIO_PCI_IDSEL, 1);
	else
		printk(KERN_ERR "fldb: Failed to claim PCI_IDSEL PIO!\n");

	/* We're not using the SPIBoot controller, so disable here and release
	 * the pads. */
	sc = sysconf_claim(CFG_COMMS_CONFIG_2, 13, 13, "spi_enable");
	sysconf_write(sc, 0);
	sysconf_release(sc);

	/* And finally! */
	fli7510_configure_pci(&fldb_pci_config);

	fli7510_configure_pwm(&fldb_pwm_private_info);
	fli7510_configure_ssc(&fldb_ssc_private_info);
	fli7510_configure_usb(0, fli7510_usb_ovrcur_active_low);
	fli7510_configure_ethernet(fli7510_ethernet_mii, 0, 0);
	fli7510_configure_lirc(NULL);
	fli7510_configure_nand(&fldb_nand_flash);
	fli7510_configure_audio(&(struct fli7510_audio_config) {
			.i2sa_output_mode =
					fli7510_audio_i2sa_output_8_channels,
			.spdif_output_enabled = 1, });

	spi_register_board_info(fldb_serial_flash,
				ARRAY_SIZE(fldb_serial_flash));

	return platform_add_devices(fldb_devices,
			ARRAY_SIZE(fldb_devices));
}
arch_initcall(fldb_device_init);



struct sh_machine_vector fldb_machine_vector __initmv = {
	.mv_name = "fldb",
	.mv_setup = fldb_setup,
	.mv_nr_irqs = NR_IRQS,
	STM_PCI_IO_MACHINE_VEC
};
