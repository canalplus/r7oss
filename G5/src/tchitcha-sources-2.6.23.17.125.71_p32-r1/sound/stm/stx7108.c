/*
 *   STMicrolectronics STx7108 SoC description & audio config driver
 *
 *   Copyright (c) 2010 STMicroelectronics Limited
 *
 *   Author: Pawel Moll <pawel.moll@st.com>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/stm/sysconf.h>
#include <sound/driver.h>
#include <sound/core.h>
#include <asm/irq-ilc.h>

#include "common.h"



static int snd_stm_debug_level;
module_param_named(debug, snd_stm_debug_level, int, S_IRUGO | S_IWUSR);



/*
 * ALSA module parameters
 */

static int index = -1; /* First available index */
static char *id = "STx7108"; /* Default card ID */

module_param(index, int, 0444);
MODULE_PARM_DESC(index, "Index value for STx7108 audio subsystem "
		"card.");
module_param(id, charp, 0444);
MODULE_PARM_DESC(id, "ID string for STx7108 audio subsystem card.");



/*
 * Audio subsystem components & platform devices
 */

/* Internal DAC */

static struct platform_device stx7108_conv_dac = {
	.name = "snd_conv_dac_sc",
	.id = -1,
	.dev.platform_data = &(struct snd_stm_conv_dac_sc_info) {
		.source_bus_id = "snd_pcm_player.1",
		.channel_from = 0,
		.channel_to = 1,
		.nrst = { SYS_CFG_BANK4, 41, 0, 0 },
		.mode = { SYS_CFG_BANK4, 41, 1, 2 },
		.nsb = { SYS_CFG_BANK4, 41, 3, 3 },
		.softmute = { SYS_CFG_BANK4, 41, 4, 4 },
		.pdana = { SYS_CFG_BANK4, 41, 5, 5 },
		.pndbg = { SYS_CFG_BANK4, 41, 6, 6 },
	},
};

/* PCM players  */

static struct platform_device stx7108_pcm_player_0 = {
	.name = "snd_pcm_player",
	.id = 0,
	.num_resources = 2,
	.resource = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd000d00,
			.end = 0xfd000d27,
		}, {
			.flags = IORESOURCE_IRQ,
			.start = ILC_IRQ(105),
			.end = ILC_IRQ(105),
		},
	},
	.dev.platform_data = &(struct snd_stm_pcm_player_info) {
		.name = "PCM player #0 (HDMI)",
		.ver = 6,
		.card_device = 0,
		.clock_name = "CLKC_FS0_CH1",
		.channels = 8,
		.fdma_initiator = 0,
		.fdma_request_line = 23,
	},
};

static struct platform_device stx7108_pcm_player_1 = {
	.name = "snd_pcm_player",
	.id = 1,
	.num_resources = 2,
	.resource = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd002000,
			.end = 0xfd002027,
		}, {
			.flags = IORESOURCE_IRQ,
			.start = ILC_IRQ(70),
			.end = ILC_IRQ(70),
		},
	},
	.dev.platform_data = &(struct snd_stm_pcm_player_info) {
		.name = "PCM player #1 (analog)",
		.ver = 6,
		.card_device = 1,
		.clock_name = "CLKC_FS0_CH2",
		.channels = 2,
		.fdma_initiator = 0,
		.fdma_request_line = 24,
	},
};

static struct platform_device stx7108_pcm_player_2 = {
	.name = "snd_pcm_player",
	.id = 2,
	.num_resources = 2,
	.resource = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd003000,
			.end = 0xfd003027,
		}, {
			.flags = IORESOURCE_IRQ,
			.start = ILC_IRQ(71),
			.end = ILC_IRQ(71),
		},
	},
	.dev.platform_data = &(struct snd_stm_pcm_player_info) {
		.name = "PCM player #2",
		.ver = 6,
		.card_device = 2,
		.clock_name = "CLKC_FS0_CH4",
		.channels = 8,
		.fdma_initiator = 0,
		.fdma_request_line = 25,
	},
};

/* SPDIF player */

static struct platform_device stx7108_spdif_player = {
	.name = "snd_spdif_player",
	.id = -1,
	.num_resources = 2,
	.resource = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd000c00,
			.end = 0xfd000c43,
		}, {
			.flags = IORESOURCE_IRQ,
			.start = ILC_IRQ(104),
			.end = ILC_IRQ(104),
		},
	},
	.dev.platform_data = &(struct snd_stm_spdif_player_info) {
		.name = "SPDIF player",
		.ver = 4,
		.card_device = 3,
		.clock_name = "CLKC_FS0_CH3",
		.fdma_initiator = 0,
		.fdma_request_line = 27,
	},
};

/* I2S to SPDIF converters */

static struct platform_device stx7108_conv_i2sspdif_0 = {
	.name = "snd_conv_i2sspdif",
	.id = 0,
	.num_resources = 2,
	.resource = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd001000,
			.end = 0xfd001223,
		}, {
			.flags = IORESOURCE_IRQ,
			.start = ILC_IRQ(103),
			.end = ILC_IRQ(103),
		}
	},
	.dev.platform_data = &(struct snd_stm_conv_i2sspdif_info) {
		.ver = 4,
		.source_bus_id = "snd_pcm_player.0",
		.channel_from = 0,
		.channel_to = 1,
	},
};

static struct platform_device stx7108_conv_i2sspdif_1 = {
	.name = "snd_conv_i2sspdif",
	.id = 1,
	.num_resources = 2,
	.resource = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd001400,
			.end = 0xfd001623,
		}, {
			.flags = IORESOURCE_IRQ,
			.start = ILC_IRQ(102),
			.end = ILC_IRQ(102),
		}
	},
	.dev.platform_data = &(struct snd_stm_conv_i2sspdif_info) {
		.ver = 4,
		.source_bus_id = "snd_pcm_player.0",
		.channel_from = 2,
		.channel_to = 3,
	},
};

static struct platform_device stx7108_conv_i2sspdif_2 = {
	.name = "snd_conv_i2sspdif",
	.id = 2,
	.num_resources = 2,
	.resource = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd001800,
			.end = 0xfd001a23,
		}, {
			.flags = IORESOURCE_IRQ,
			.start = ILC_IRQ(101),
			.end = ILC_IRQ(101),
		}
	},
	.dev.platform_data = &(struct snd_stm_conv_i2sspdif_info) {
		.ver = 4,
		.source_bus_id = "snd_pcm_player.0",
		.channel_from = 4,
		.channel_to = 5,
	},
};

static struct platform_device stx7108_conv_i2sspdif_3 = {
	.name = "snd_conv_i2sspdif",
	.id = 3,
	.num_resources = 2,
	.resource = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd001c00,
			.end = 0xfd001e23,
		}, {
			.flags = IORESOURCE_IRQ,
			.start = ILC_IRQ(100),
			.end = ILC_IRQ(100),
		}
	},
	.dev.platform_data = &(struct snd_stm_conv_i2sspdif_info) {
		.ver = 4,
		.source_bus_id = "snd_pcm_player.0",
		.channel_from = 6,
		.channel_to = 7,
	},
};

/* PCM reader */

static struct platform_device stx7108_pcm_reader = {
	.name = "snd_pcm_reader",
	.id = -1,
	.num_resources = 2,
	.resource = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd004000,
			.end = 0xfd004027,
		}, {
			.flags = IORESOURCE_IRQ,
			.start = ILC_IRQ(72),
			.end = ILC_IRQ(72),
		},
	},
	.dev.platform_data = &(struct snd_stm_pcm_reader_info) {
		.name = "PCM Reader",
		.ver = 5,
		.card_device = 4,
		.channels = 2,
		.fdma_initiator = 0,
		.fdma_request_line = 26,
	},
};

static struct platform_device *snd_stm_stx7108_devices[] = {
	&stx7108_conv_dac,
	&stx7108_pcm_player_0,
	&stx7108_pcm_player_1,
	&stx7108_pcm_player_2,
	&stx7108_spdif_player,
	&stx7108_conv_i2sspdif_0,
	&stx7108_conv_i2sspdif_1,
	&stx7108_conv_i2sspdif_2,
	&stx7108_conv_i2sspdif_3,
	&stx7108_pcm_reader,
};



/*
 * Audio initialization
 */

static struct sysconf_field *snd_stm_stx7108_pcmp_valid_sel;

static int __init snd_stm_stx7108_init(void)
{
	int result;
	struct snd_card *card;

	snd_stm_printd(0, "snd_stm_stx7108_init()\n");

	if (cpu_data->type != CPU_STX7108) {
		snd_stm_printe("Not supported (other than STx7108) SOC "
				"detected!\n");
		result = -EINVAL;
		goto error_soc_type;
	}

	/* Ugly but quick hack to have SPDIF player & I2S to SPDIF
	 * converters enabled without loading STMFB...
	 * TODO: do this in some sane way! */
	{
		void *hdmi_gpout = ioremap(0xfd000020, 4);
		writel(readl(hdmi_gpout) | 0x3, hdmi_gpout);
		iounmap(hdmi_gpout);
	}

	/* Route PCM Player #2 to the digital outputs (PIOs) */
	snd_stm_stx7108_pcmp_valid_sel = sysconf_claim(SYS_CFG_BANK4,
			43, 0, 1, "PCMP_VALID_SEL");
	if (!snd_stm_stx7108_pcmp_valid_sel) {
		snd_stm_printe("Can't claim required sysconf field!\n");
		result = -EBUSY;
		goto error_sysconf_claim;
	}
	sysconf_write(snd_stm_stx7108_pcmp_valid_sel, 2);

	card = snd_stm_card_new(index, id, THIS_MODULE);
	if (card == NULL) {
		snd_stm_printe("ALSA card creation failed!\n");
		result = -ENOMEM;
		goto error_card_new;
	}
	strcpy(card->driver, "STx7108");
	snprintf(card->shortname, 31, "STx7108 audio subsystem");
	snprintf(card->longname, 79, "STMicroelectronics STx7108 cut %d "
			"SOC audio subsystem", cpu_data->cut_major);

	result = snd_stm_add_platform_devices(snd_stm_stx7108_devices,
			ARRAY_SIZE(snd_stm_stx7108_devices));
	if (result != 0) {
		snd_stm_printe("Failed to add platform devices!\n");
		goto error_add_devices;
	}

	result = snd_stm_card_register();
	if (result != 0) {
		snd_stm_printe("Failed to register ALSA cards!\n");
		goto error_card_register;
	}

	return 0;

error_card_register:
	snd_stm_remove_platform_devices(snd_stm_stx7108_devices,
			ARRAY_SIZE(snd_stm_stx7108_devices));
error_add_devices:
	snd_stm_card_free();
error_card_new:
error_sysconf_claim:
error_soc_type:
	return result;
}

static void __exit snd_stm_stx7108_exit(void)
{
	snd_stm_printd(0, "snd_stm_stx7108_exit()\n");

	snd_stm_card_free();

	snd_stm_remove_platform_devices(snd_stm_stx7108_devices,
			ARRAY_SIZE(snd_stm_stx7108_devices));

	sysconf_release(snd_stm_stx7108_pcmp_valid_sel);
}

MODULE_AUTHOR("Pawel Moll <pawel.moll@st.com>");
MODULE_DESCRIPTION("STMicroelectronics STx7108 audio driver");
MODULE_LICENSE("GPL");

module_init(snd_stm_stx7108_init);
module_exit(snd_stm_stx7108_exit);
