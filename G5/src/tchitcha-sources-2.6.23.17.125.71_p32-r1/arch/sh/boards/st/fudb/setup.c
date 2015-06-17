/*
 * arch/sh/boards/st/fudb/setup.c
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
#include <linux/phy_fixed.h>
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



#define FUDB_GPIO_RESET_OUTN stpio_to_gpio(11, 5)
#define FUDB_GPIO_SPI_WPN stpio_to_gpio(18, 2)



static int ascs[2] __initdata = { 1, 2 | (STASC_FLAG_NORTSCTS << 8) };

static void __init fudb_setup(char **cmdline_p)
{
	printk(KERN_INFO "STMicroelectronics Freeman 540 "
			"Development Board initialisation\n");

	fli7510_early_device_init();
	fli7510_configure_asc(ascs, 2, 0);
}



static struct plat_ssc_data fudb_ssc_private_info = {
	.capability  =
		ssc0_has(SSC_I2C_CAPABILITY) |
		ssc1_has(SSC_I2C_CAPABILITY) |
		ssc2_has(SSC_I2C_CAPABILITY) |
		ssc3_has(SSC_I2C_CAPABILITY) |
		ssc4_has(SSC_SPI_CAPABILITY),
};



static struct plat_stm_pwm_data fudb_pwm_private_info = {
	.flags = PLAT_STM_PWM_OUT2 | PLAT_STM_PWM_OUT3,
};



static struct platform_device fudb_led_df1 = {
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
static struct platform_device fudb_speakers = {
	.name = "snd_conv_dummy",
	.id = 0,
	.dev.platform_data = &(struct snd_stm_conv_dummy_info) {
		.group = "Speakers",

		.source_bus_id = "snd_pcm_player.0",
		.channel_from = 0,
		.channel_to = 1,
		.format = SND_STM_FORMAT__LEFT_JUSTIFIED |
				SND_STM_FORMAT__SUBFRAME_32_BITS,
		.oversampling = 256,
	},
};



static struct platform_device fudb_phy_device = {
	.name = "stmmacphy",
	.id = -1,
	.dev.platform_data = &(struct plat_stmmacphy_data) {
		.bus_id = 0,
		.phy_addr = 1,
		.phy_mask = 0,
		.interface = PHY_INTERFACE_MODE_MII,
	},
};



static struct spi_board_info fudb_serial_flash =  {
	.modalias = "m25p80",
	.bus_num = 0,
	.chip_select = spi_set_cs(20, 2),
	.max_speed_hz = 5000000,
	.mode = SPI_MODE_3,
	.platform_data = &(struct flash_platform_data) {
		.name = "m25p80",
		.type = "m25p64",
		.nr_parts = 2,
		.parts = (struct mtd_partition []) {
			{
				.name = "SerialFlash_1",
				.size = 0x00080000,
				.offset = 0,
			}, {
				.name = "SerialFlash_2",
				.size = MTDPART_SIZ_FULL,
				.offset = MTDPART_OFS_NXTBLK,
			},
		},
	},
};



static struct platform_device *fudb_devices[] __initdata = {
	&fudb_led_df1,
	&fudb_speakers,
	&fudb_phy_device,
};



static int __init fudb_device_init(void)
{
	/* This is a board-level reset line, which goes to the
	 * Ethernet PHY, audio amps & number of extension connectors */
	if (gpio_request(FUDB_GPIO_RESET_OUTN, "RESET_OUTN") == 0) {
		gpio_direction_output(FUDB_GPIO_RESET_OUTN, 0);
		udelay(10000); /* 10ms is the Ethernet PHY requirement */
		gpio_set_value(FUDB_GPIO_RESET_OUTN, 1);
	} else {
		printk(KERN_ERR "fudb: Failed to claim RESET_OUTN PIO!\n");
	}

	fli7510_configure_ethernet(fli7510_ethernet_rmii, 0, 0);

	fli7510_configure_pwm(&fudb_pwm_private_info);
	fli7510_configure_ssc(&fudb_ssc_private_info);
	fli7510_configure_usb(0, fli7510_usb_ovrcur_active_low);
	fli7510_configure_usb(1, fli7510_usb_ovrcur_active_low);
	fli7510_configure_lirc(NULL);
	fli7510_configure_audio(&(struct fli7510_audio_config) {
			.i2sa_output_mode =
					fli7510_audio_i2sa_output_8_channels,
			.spdif_output_enabled = 1, });

	spi_register_board_info(&fudb_serial_flash, 1);

	return platform_add_devices(fudb_devices,
			ARRAY_SIZE(fudb_devices));
}
arch_initcall(fudb_device_init);



struct sh_machine_vector fudb_machine_vector __initmv = {
	.mv_name = "fudb",
	.mv_setup = fudb_setup,
	.mv_nr_irqs = NR_IRQS,
};
