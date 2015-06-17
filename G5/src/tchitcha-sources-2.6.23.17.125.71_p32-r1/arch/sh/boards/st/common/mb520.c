/*
 * arch/sh/boards/st/common/mb520.c
 *
 * Copyright (C) 2007 STMicroelectronics Limited
 * Author: Pawel Moll <pawel.moll@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * STMicroelectronics STB peripherals board support.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/i2c.h>
#include <linux/i2c/pcf857x.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/pio.h>
#include <linux/bug.h>
#include <asm/processor.h>
#include <sound/stm.h>



/* I2C PIO extender (IC23) */
#define EXTENDER_BASE 200
static struct i2c_board_info pio_extender = {
	I2C_BOARD_INFO("pcf857x", 0x27),
	.type = "pcf8575",
	.platform_data = &(struct pcf857x_platform_data) {
		.gpio_base = EXTENDER_BASE,
	},
};
#define EXTENDER_GPIO(port, pin) (EXTENDER_BASE + (port * 8) + (pin))



/* Heartbeat led (LD12-T) */
static struct platform_device hb_led = {
	.name = "leds-gpio",
	.id = -1,
	.dev.platform_data = &(struct gpio_led_platform_data) {
		.num_leds = 1,
		.leds = (struct gpio_led[]) {
			{
				.name = "HB",
				.default_trigger = "heartbeat",
				.gpio = stpio_to_gpio(4, 7),
				.active_low = 1,
			},
		},
	},
};



#ifdef CONFIG_SND
/* Audio peripherals
 *
 * The recommended audio setup of MB520 is as follows:
 * 1. Remove R93 (located near SW4 and IC18)
 * 2. Set SW3[1..4] to [OFF, OFF, ON, ON]
 * 3. Set SW5[1..4] to [OFF, ON, ON, OFF]
 * 4. Update EPLD version to revision 7
 *
 * With such settings PCM Player #0 stereo output is available on
 * "INT DACS AUD-0" connectors, PCM Player #1 (first two channels)
 * on "INT DACS AUD-1", and first two channels of PCM Player #3
 * on "EXT DAC". PCM Reader is getting data from "SPDIF PCM IN"
 * connector. */

/* AK4388 DAC (IC18) */
static struct platform_device conv_external_dac = {
	.name = "snd_conv_gpio",
	.id = 0,
	.dev.platform_data = &(struct snd_stm_conv_gpio_info) {
		.group = "External DAC",

		.source_bus_id = "snd_pcm_player.3",
		.channel_from = 0,
		.channel_to = 1,
		.format = SND_STM_FORMAT__I2S |
				SND_STM_FORMAT__SUBFRAME_32_BITS,
		.oversampling = 256,

		.enable_supported = 1,
		.enable_gpio = EXTENDER_GPIO(0, 7),
		.enable_value = 1,

		.mute_supported = 1,
		.mute_gpio = EXTENDER_GPIO(1, 7),
		.mute_value = 1,
	},
};

/* CS8416 SPDIF to I2S converter (IC14) */
static struct platform_device conv_spdif_to_i2s = {
	.name = "snd_conv_gpio",
	.id = 1,
	.dev.platform_data = &(struct snd_stm_conv_gpio_info) {
		.group = "SPDIF Input",

		.source_bus_id = "snd_pcm_reader.0",
		.channel_from = 0,
		.channel_to = 1,
		.format = SND_STM_FORMAT__I2S |
				SND_STM_FORMAT__SUBFRAME_32_BITS,

		.enable_supported = 1,
		.enable_gpio = EXTENDER_GPIO(1, 6),
		.enable_value = 1,

		.mute_supported = 0,
	},
};
#endif



static struct platform_device *mb520_devices[] __initdata = {
	&hb_led,
#ifdef CONFIG_SND
	&conv_external_dac,
	&conv_spdif_to_i2s,
#endif
};

static int __init device_init(void)
{
	int result;
	struct sysconf_field *sc;

	/* This file is (so far) valid only for 7200 processor board! */
	BUG_ON(cpu_data->type != CPU_STX7200);

	/* Sysconf bits */

#ifdef CONFIG_SND
	/* CONF_PAD_AUD[0] = 1
	 * AUDDIG* are connected PCMOUT3_* - 10-channels PCM player #3
	 * ("scenario 1", but only one channel is available) */
	sc = sysconf_claim(SYS_CFG, 20, 0, 0, "pcm_player.3");
	sysconf_write(sc, 1);
#endif

	/* I2C devices */

	/* PIO extender is connected do SSC4 (I2C device no. '2'
	 * in case of MB519...) */
	result = i2c_register_board_info(2, &pio_extender, 1);

	/* And now platform devices... */

	result |= platform_add_devices(mb520_devices,
			ARRAY_SIZE(mb520_devices));

	return result;
}
arch_initcall(device_init);
