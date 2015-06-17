/*
 * arch/sh/boards/st/hdk7108/setup.c
 *
 * Copyright (C) 2009 STMicroelectronics Limited
 * Author: Pawel Moll (pawel.moll@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/phy.h>
#include <linux/phy_fixed.h>
#include <linux/gpio.h>
#include <linux/leds.h>
#include <linux/tm1668.h>
#include <linux/spi/spi.h>
#include <linux/stm/nand.h>
#include <linux/stm/emi.h>
#include <linux/stm/soc.h>
#include <linux/stm/soc_init.h>
#include <linux/stm/pio.h>
#include <linux/stm/sysconf.h>
#include "../common/common.h"
#include <linux/bpa2.h>

#define HDK7108_GPIO_POWER_ON stpio_to_gpio(5, 0)
#define HDK7108_GPIO_PCI_RESET stpio_to_gpio(6, 4)
#define HDK7108_GPIO_POWER_ON_ETHERNET stpio_to_gpio(15, 4)

const char *player_buffers[] = { "BPA2_Region0", "BPA2_Region1", "v4l2-video-buffers",
                                 "bigphysarea", "v4l2-coded-video-buffers", "coredisplay-video", NULL };
//const char *bigphys[]        = { "bigphysarea", "v4l2-coded-video-buffers", "coredisplay-video", NULL };

static struct bpa2_partition_desc bpa2_parts_table[] = {
        
        {
                .name  = "Player_Buffers", //96MB
                .start = 0,
                .size  = 0x07000000,
                .flags = 0,
                .aka   = player_buffers
        },
        /*
        {
                .name  = "Bigphys_Buffers", //64MB
                .start = 0,
                .size  = 0x04000000,
                .flags = 0,
                .aka   = bigphys
        },
        */
        {
                .name  = "gfx-memory",  //64MB
                .start = 0x00000000,
                .size  = 0x04000000,
                .flags = 0,
                .aka   = NULL
        }        
};

static void __init hdk7108_setup(char **cmdline_p)
{
	printk(KERN_INFO "STMicroelectronics HDK7108 board initialisation\n");

	stx7108_early_device_init();

	stx7108_configure_asc(3, &(struct stx7108_asc_config) {
			.routing.asc3.txd = stx7108_asc3_txd_pio24_4,
			.routing.asc3.rxd = stx7108_asc3_rxd_pio24_5,
			.hw_flow_control = 0,
			.is_console = 1, });
	stx7108_configure_asc(1, &(struct stx7108_asc_config) {
			.hw_flow_control = 1, });

	bpa2_init(bpa2_parts_table, ARRAY_SIZE(bpa2_parts_table));
}



static struct platform_device hdk7108_leds = {
	.name = "leds-gpio",
	.id = -1,
	.dev.platform_data = &(struct gpio_led_platform_data) {
		.num_leds = 2,
		.leds = (struct gpio_led[]) {
			{
				.name = "GREEN",
				.default_trigger = "heartbeat",
				.gpio = stpio_to_gpio(3, 0),
			}, {
				.name = "RED",
				.gpio = stpio_to_gpio(3, 1),
			},
		},
	},
};

static struct tm1668_key hdk7108_front_panel_keys[] = {
	{ 0x00001000, KEY_UP, "Up (SWF2)" },
	{ 0x00800000, KEY_DOWN, "Down (SWF7)" },
	{ 0x00008000, KEY_LEFT, "Left (SWF6)" },
	{ 0x00000010, KEY_RIGHT, "Right (SWF5)" },
	{ 0x00000080, KEY_ENTER, "Enter (SWF1)" },
	{ 0x00100000, KEY_ESC, "Escape (SWF4)" },
};

static struct tm1668_character hdk7108_front_panel_characters[] = {
	TM1668_7_SEG_HEX_DIGITS,
	TM1668_7_SEG_HEX_DIGITS_WITH_DOT,
	TM1668_7_SEG_SEGMENTS,
};

static struct platform_device hdk7108_front_panel = {
	.name = "tm1668",
	.id = -1,
	.dev.platform_data = &(struct tm1668_platform_data) {
		.gpio_dio = stpio_to_gpio(2, 5),
		.gpio_sclk = stpio_to_gpio(2, 4),
		.gpio_stb = stpio_to_gpio(2, 6),
		.config = tm1668_config_6_digits_12_segments,

		.keys_num = ARRAY_SIZE(hdk7108_front_panel_keys),
		.keys = hdk7108_front_panel_keys,
		.keys_poll_period = DIV_ROUND_UP(HZ, 5),

		.brightness = 8,
		.characters_num = ARRAY_SIZE(hdk7108_front_panel_characters),
		.characters = hdk7108_front_panel_characters,
		.text = "7108",
	},
};



static int hdk7108_phy_reset(void *bus)
{
	static int done;

	/* This line is shared between both MII interfaces */
	if (!done) {
		gpio_set_value(HDK7108_GPIO_POWER_ON_ETHERNET, 0);
		udelay(10000); /* 10 miliseconds is enough for everyone ;-) */
		gpio_set_value(HDK7108_GPIO_POWER_ON_ETHERNET, 1);
		done = 1;
	}

	return 1;
}

static struct platform_device hdk7108_phy_devices[] = {
	{ /* MII connector JP2 */
		.name = "stmmacphy",
		.id = 0,
		.dev.platform_data = &(struct  plat_stmmacphy_data) {
			.bus_id = 1,
			.phy_addr = -1,
			.phy_mask = 0,
			.interface = PHY_INTERFACE_MODE_GMII,
			.phy_reset = &hdk7108_phy_reset,
		},
	}, { /* On-board ICplus IP1001 */
		.name = "stmmacphy",
		.id = 1,
		.dev.platform_data = &(struct  plat_stmmacphy_data) {
			.bus_id = 2,
			.phy_addr = 1,
			.phy_mask = 0,
			.interface = PHY_INTERFACE_MODE_GMII,
			.phy_reset = &hdk7108_phy_reset,
		},
	},
};

static struct fixed_phy_status fixed_phy_status[2] = {
	{
		.link = 1,
		.speed = 100,
	}, {
		.link = 1,
		.speed = 100,
	}
};



static struct platform_device *hdk7108_devices[] __initdata = {
	&hdk7108_leds,
	&hdk7108_front_panel,
	&hdk7108_phy_devices[0],
	&hdk7108_phy_devices[1],
};



/* PCI configuration */
static struct pci_config_data hdk7108_pci_config = {
	.pci_irq = {
		[0] = PCI_PIN_DEFAULT,
		[1] = PCI_PIN_DEFAULT,
		[2] = PCI_PIN_UNUSED,
		[3] = PCI_PIN_UNUSED
	},
	.serr_irq = PCI_PIN_DEFAULT,
	.idsel_lo = 30,
	.idsel_hi = 30,
	.req_gnt = {
		[0] = PCI_PIN_DEFAULT,
		[1] = PCI_PIN_UNUSED,
		[2] = PCI_PIN_UNUSED,
		[3] = PCI_PIN_UNUSED
	},
	.pci_clk = 33333333,
	.pci_reset_gpio = -EINVAL,
};

int pcibios_map_platform_irq(struct pci_dev *dev, u8 slot, u8 pin)
{
	/* We can use the standard function on this board */
	return stx7108_pcibios_map_platform_irq(&hdk7108_pci_config, pin);
}



static int __init hdk7108_device_init(void)
{
	int phy_bus;

	stx7108_configure_pci(&hdk7108_pci_config);

	/* The "POWER_ON_ETH" line should be rather called "PHY_RESET",
	 * but it isn't... ;-) */
	gpio_request(HDK7108_GPIO_POWER_ON_ETHERNET, "POWER_ON_ETHERNET");
	gpio_direction_output(HDK7108_GPIO_POWER_ON_ETHERNET, 0);

	/* Some of the peripherals are powered by regulators
	 * triggered by the following PIO line... */
	gpio_request(HDK7108_GPIO_POWER_ON, "POWER_ON");
	gpio_direction_output(HDK7108_GPIO_POWER_ON, 1);

	/* Serial flash */
	stx7108_configure_ssc_spi(0, NULL);
	/* NIM */
	stx7108_configure_ssc_i2c(1, NULL);
	/* AV */
	stx7108_configure_ssc_i2c(2, &(struct stx7108_ssc_config) {
			.routing.ssc2.sclk = stx7108_ssc2_sclk_pio14_4,
			.routing.ssc2.mtsr = stx7108_ssc2_mtsr_pio14_5, });
	/* SYS - EEPROM & MII0 */
	stx7108_configure_ssc_i2c(5, NULL);
	/* HDMI */
	stx7108_configure_ssc_i2c(6, NULL);

	stx7108_configure_lirc(&(struct stx7108_lirc_config) {
			.rx_mode = stx7108_lirc_rx_mode_ir, });

	stx7108_configure_usb(0);
	stx7108_configure_usb(1);
	stx7108_configure_usb(2);

	stx7108_configure_sata(0);
	stx7108_configure_sata(1);

	stx7108_configure_audio(&(struct stx7108_audio_config) {
			.pcm_output_mode = stx7108_audio_pcm_output_8_channels,
			.pcm_input_enabled = 1,
			.spdif_output_enabled = 1, });

#ifdef CONFIG_SH_ST_HDK7108_STMMAC0
#ifdef CONFIG_SH_ST_HDK7108_STMMAC0_FIXED_PHY
	/* Use fixed phy bus (bus 0) phy 1 */
	phy_bus = STMMAC_PACK_BUS_ID(0, 1);
	BUG_ON(fixed_phy_add(PHY_POLL, 1, &fixed_phy_status[0]));
#else
	/* Use stmmacphy bus 1 defined above */
	phy_bus = 1;
#endif
	stx7108_configure_ethernet(0, &(struct stx7108_ethernet_config) {
			.mode = stx7108_ethernet_mode_mii,
			.ext_clk = 1,
			.phy_bus = phy_bus, });
#endif
	/*
	 * HW changes needed to use the GMII mode (GTX CLK)
	 *		RP18	RP42	RP14	Rp17
	 *	GMII	NC	 51R	NC	51R
	 *	MII	NC	 NC	51R	51R
	 */
	stx7108_configure_ethernet(1, &(struct stx7108_ethernet_config) {
			.mode = stx7108_ethernet_mode_gmii_gtx,
			.ext_clk = 0,
			.phy_bus = 2, });

	stx7108_configure_mmc();

	return platform_add_devices(hdk7108_devices,
			ARRAY_SIZE(hdk7108_devices));
}
arch_initcall(hdk7108_device_init);


static void __iomem *hdk7108_ioport_map(unsigned long port, unsigned int size)
{
	/* Shouldn't be here! */
	BUG();
	return NULL;
}

struct sh_machine_vector mv_hdk7108 __initmv = {
	.mv_name = "hdk7108",
	.mv_setup = hdk7108_setup,
	.mv_nr_irqs = NR_IRQS,
	.mv_ioport_map = hdk7108_ioport_map,
	STM_PCI_IO_MACHINE_VEC
};

