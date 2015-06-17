/*
 *   STMicrolectronics STx7100/STx7109 SoC description & audio glue driver
 *
 *   Copyright (c) 2005-2007 STMicroelectronics Limited
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
#include <linux/stm/clk.h>
#include <sound/driver.h>
#include <sound/core.h>

#include "common.h"



static int snd_stm_debug_level;
module_param_named(debug, snd_stm_debug_level, int, S_IRUGO | S_IWUSR);



/*
 * ALSA module parameters
 */

static int index = -1; /* First available index */
static char *id = "STx7100"; /* Default card ID */

module_param(index, int, 0444);
MODULE_PARM_DESC(index, "Index value for STx7100/STx7109 audio subsystem "
		"card.");
module_param(id, charp, 0444);
MODULE_PARM_DESC(id, "ID string for STx7100/STx7109 audio subsystem card.");



/*
 * Audio subsystem components & platform devices
 */

/* Audio subsystem glue */

static struct platform_device stx7100_glue = {
	.name = "snd_stx7100_glue",
	.id = -1,
	.num_resources = 1,
	.resource = (struct resource[]) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0x19210200,
			.end = 0x19210203,
		},
	},
};

/* Internal DAC */

static struct snd_stm_conv_dac_mem_info conv_dac_mem_info = {
	/* .ver = see snd_stm_stx7100_init() */
	.source_bus_id = "snd_pcm_player.1",
	.channel_from = 0,
	.channel_to = 1,
};

static struct platform_device stx7100_conv_dac_mem = {
	.name = "snd_conv_dac_mem",
	.id = -1,
	.num_resources = 1,
	.resource = (struct resource[]) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0x19210100,
			.end = 0x19210103,
		},
	},
	.dev.platform_data = &conv_dac_mem_info,
};

/* PCM players */

struct snd_stm_pcm_player_info stx7100_pcm_player_0_info = {
	.name = "PCM player #0 (HDMI)",
	/* .ver = see snd_stm_stx7100_init() */
	.card_device = 0,
	.clock_name = "pcm0_clk",
	.channels = 10,
	.fdma_initiator = 1,
	/* .fdma_request_line = see snd_stm_stx7100_init() */
};

static struct platform_device stx7100_pcm_player_0 = {
	.name = "snd_pcm_player",
	.id = 0,
	.num_resources = 2,
	.resource = (struct resource[]) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0x18101000,
			.end = 0x18101027,
		}, {
			.flags = IORESOURCE_IRQ,
			.start = 144,
			.end = 144,
		},
	},
	.dev.platform_data = &stx7100_pcm_player_0_info,
};

struct snd_stm_pcm_player_info stx7100_pcm_player_1_info = {
	.name = "PCM player #1",
	/* .ver = see snd_stm_stx7100_init() */
	.card_device = 1,
	.clock_name = "pcm1_clk",
	.channels = 2,
	.fdma_initiator = 1,
	/* .fdma_request_line = see snd_stm_stx7100_init() */
};

static struct platform_device stx7100_pcm_player_1 = {
	.name = "snd_pcm_player",
	.id = 1,
	.num_resources = 2,
	.resource = (struct resource[]) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0x18101800,
			.end = 0x18101827,
		}, {
			.flags = IORESOURCE_IRQ,
			.start = 145,
			.end = 145,
		},
	},
	.dev.platform_data = &stx7100_pcm_player_1_info,
};

/* SPDIF player */

struct snd_stm_spdif_player_info stx7100_spdif_player_info = {
	.name = "SPDIF player (HDMI)",
	/* .ver = see snd_stm_stx7100_init() */
	.card_device = 2,
	.clock_name = "spdif_clk",
	.fdma_initiator = 1,
	/* .fdma_request_line = see snd_stm_stx7100_init() */
};

static struct platform_device stx7100_spdif_player = {
	.name = "snd_spdif_player",
	.id = -1,
	.num_resources = 2,
	.resource = (struct resource[]) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0x18103000,
			.end = 0x1810303f,
		}, {
			.flags = IORESOURCE_IRQ,
			.start = 147,
			.end = 147,
		},
	},
	.dev.platform_data = &stx7100_spdif_player_info,
};

/* HDMI-connected I2S to SPDIF converter */

static struct snd_stm_conv_i2sspdif_info stx7100_conv_i2sspdif_info = {
	/* .ver = see stx7100_configure_audio() */
	.source_bus_id = "snd_pcm_player.0",
	.channel_from = 0,
	.channel_to = 1,
};

static struct platform_device stx7100_conv_i2sspdif = {
	.name = "snd_conv_i2sspdif",
	.id = -1,
	.num_resources = 2,
	.resource = (struct resource[]) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0x18103800,
			.end = 0x18103a23,
		}, {
			.flags = IORESOURCE_IRQ,
			.start = 142,
			.end = 142,
		},
	},
	.dev.platform_data = &stx7100_conv_i2sspdif_info,
};

/* PCM reader */

struct snd_stm_pcm_reader_info stx7100_pcm_reader_info = {
	.name = "PCM Reader",
	/* .ver = see snd_stm_stx7100_init() */
	.card_device = 3,
	.channels = 2,
	.fdma_initiator = 1,
	/* .fdma_request_line = see snd_stm_stx7100_init() */
};

static struct platform_device stx7100_pcm_reader = {
	.name = "snd_pcm_reader",
	.id = -1,
	.num_resources = 2,
	.resource = (struct resource[]) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0x18102000,
			.end = 0x18102027,
		}, {
			.flags = IORESOURCE_IRQ,
			.start = 146,
			.end = 146,
		},
	},
	.dev.platform_data = &stx7100_pcm_reader_info,
};

static struct platform_device *snd_stm_stx7100_devices[] = {
	&stx7100_glue,
	&stx7100_pcm_player_0,
	&stx7100_pcm_player_1,
	&stx7100_conv_dac_mem,
	&stx7100_spdif_player,
	&stx7100_conv_i2sspdif,
	&stx7100_pcm_reader,
};



/*
 * Audio glue driver implementation
 */

#define IO_CTRL(base)		((base) + 0x00)
#define PCM_CLK_EN		0
#define PCM_CLK_EN__INPUT	(0 << PCM_CLK_EN)
#define PCM_CLK_EN__OUTPUT	(1 << PCM_CLK_EN)
#define DATA0_EN		1
#define DATA0_EN__INPUT		(0 << DATA0_EN)
#define DATA0_EN__OUTPUT	(1 << DATA0_EN)
#define DATA1_EN		2
#define DATA1_EN__INPUT		(0 << DATA1_EN)
#define DATA1_EN__OUTPUT	(1 << DATA1_EN)
#define SPDIN_EN		3
#define SPDIF_EN__DISABLE	(0 << SPDIN_EN)
#define SPDIF_EN__ENABLE	(1 << SPDIN_EN)

struct snd_stm_stx7100_glue {
	struct resource *mem_region;
	void *base;

	struct snd_info_entry *proc_entry;

	snd_stm_magic_field;
};

static void snd_stm_stx7100_glue_dump_registers(struct snd_info_entry *entry,
		struct snd_info_buffer *buffer)
{
	struct snd_stm_stx7100_glue *stx7100_glue = entry->private_data;

	BUG_ON(!stx7100_glue);
	BUG_ON(!snd_stm_magic_valid(stx7100_glue));

	snd_iprintf(buffer, "--- snd_stx7100_glue ---\n");
	snd_iprintf(buffer, "IO_CTRL (0x%p) = 0x%08x\n",
			IO_CTRL(stx7100_glue->base),
			readl(IO_CTRL(stx7100_glue->base)));

	snd_iprintf(buffer, "\n");
}

static int __init snd_stm_stx7100_glue_probe(struct platform_device *pdev)
{
	int result = 0;
	struct snd_stm_stx7100_glue *stx7100_glue;

	snd_stm_printd(0, "%s()\n", __func__);

	stx7100_glue = kzalloc(sizeof(*stx7100_glue), GFP_KERNEL);
	if (!stx7100_glue) {
		snd_stm_printe("Can't allocate memory "
				"for a device description!\n");
		result = -ENOMEM;
		goto error_alloc;
	}
	snd_stm_magic_set(stx7100_glue);

	result = snd_stm_memory_request(pdev, &stx7100_glue->mem_region,
			&stx7100_glue->base);
	if (result < 0) {
		snd_stm_printe("Memory region request failed!\n");
		goto error_memory_request;
	}

	/* Enable all audio outputs */
	writel(SPDIF_EN__ENABLE | DATA1_EN__OUTPUT | DATA0_EN__OUTPUT |
			PCM_CLK_EN__OUTPUT, IO_CTRL(stx7100_glue->base));

	/* Additional procfs info */
	snd_stm_info_register(&stx7100_glue->proc_entry, "stx7100_glue",
			snd_stm_stx7100_glue_dump_registers, stx7100_glue);

	platform_set_drvdata(pdev, stx7100_glue);

	return result;

error_memory_request:
	snd_stm_magic_clear(stx7100_glue);
	kfree(stx7100_glue);
error_alloc:
	return result;
}

static int __exit snd_stm_stx7100_glue_remove(struct platform_device *pdev)
{
	struct snd_stm_stx7100_glue *stx7100_glue =
			platform_get_drvdata(pdev);

	snd_stm_printd(0, "%s()\n", __func__);

	BUG_ON(!stx7100_glue);
	BUG_ON(!snd_stm_magic_valid(stx7100_glue));

	/* Remove procfs entry */
	snd_stm_info_unregister(stx7100_glue->proc_entry);

	/* Disable all audio outputs */
	writel(SPDIF_EN__DISABLE | DATA1_EN__INPUT | DATA0_EN__INPUT |
			PCM_CLK_EN__INPUT, IO_CTRL(stx7100_glue->base));

	snd_stm_memory_release(stx7100_glue->mem_region, stx7100_glue->base);

	snd_stm_magic_clear(stx7100_glue);
	kfree(stx7100_glue);

	return 0;
}

static struct platform_driver snd_stm_stx7100_glue_driver = {
	.driver.name = "snd_stx7100_glue",
	.probe = snd_stm_stx7100_glue_probe,
	.remove = snd_stm_stx7100_glue_remove,
};



/*
 * Audio initialization
 */

static int __init snd_stm_stx7100_init(void)
{
	int result;
	const char *soc_type;
	struct snd_card *card;

	snd_stm_printd(0, "%s()\n", __func__);

	switch (cpu_data->type) {
	case CPU_STB7100:
		soc_type = "STx7100";

		/* FDMA request line configuration */
		stx7100_pcm_player_0_info.fdma_request_line = 26;
		stx7100_pcm_player_1_info.fdma_request_line = 27;
		stx7100_spdif_player_info.fdma_request_line = 29;
		stx7100_pcm_reader_info.fdma_request_line = 28;

		/* IP versions */
		stx7100_pcm_reader_info.ver = 1;
		if (cpu_data->cut_major < 3) {
			/* STx7100 cut < 3.0 */
			stx7100_pcm_player_0_info.ver = 1;
			stx7100_pcm_player_1_info.ver = 1;
		} else {
			/* STx7100 cut >= 3.0 */
			stx7100_pcm_player_0_info.ver = 2;
			stx7100_pcm_player_1_info.ver = 2;
		}
		stx7100_spdif_player_info.ver = 1;
		stx7100_conv_i2sspdif_info.ver = 1;

		break;

	case CPU_STB7109:
		soc_type = "STx7109";

		/* FDMA request line configuration */
		stx7100_pcm_player_0_info.fdma_request_line = 24;
		stx7100_pcm_player_1_info.fdma_request_line = 25;
		stx7100_spdif_player_info.fdma_request_line = 27;
		stx7100_pcm_reader_info.fdma_request_line = 26;

		/* IP versions */
		stx7100_pcm_reader_info.ver = 2;
		if (cpu_data->cut_major < 3) {
			/* STx7109 cut < 3.0 */
			stx7100_pcm_player_0_info.ver = 3;
			stx7100_pcm_player_1_info.ver = 3;
		} else {
			/* STx7109 cut >= 3.0 */
			stx7100_pcm_player_0_info.ver = 4;
			stx7100_pcm_player_1_info.ver = 4;
		}
		stx7100_spdif_player_info.ver = 2;
		stx7100_conv_i2sspdif_info.ver = 2;

		break;

	default:
		snd_stm_printe("Not supported (other than STx7100 or STx7109)"
				" SOC detected!\n");
		result = -EINVAL;
		goto error_soc_type;
	}

	result = platform_driver_register(&snd_stm_stx7100_glue_driver);
	if (result != 0) {
		snd_stm_printe("Failed to register audio glue driver!\n");
		goto error_glue_driver_register;
	}

	card = snd_stm_card_new(index, id, THIS_MODULE);
	if (card == NULL) {
		snd_stm_printe("ALSA card creation failed!\n");
		result = -ENOMEM;
		goto error_card_new;
	}
	strcpy(card->driver, soc_type);
	snprintf(card->shortname, 31, "%s audio subsystem", soc_type);
	snprintf(card->longname, 79, "STMicroelectronics %s cut %d.%d SOC "
			"audio subsystem", soc_type, cpu_data->cut_major,
			cpu_data->cut_minor);

	result = snd_stm_add_platform_devices(snd_stm_stx7100_devices,
			ARRAY_SIZE(snd_stm_stx7100_devices));
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
	snd_stm_remove_platform_devices(snd_stm_stx7100_devices,
			ARRAY_SIZE(snd_stm_stx7100_devices));
error_add_devices:
	snd_stm_card_free();
error_card_new:
	platform_driver_unregister(&snd_stm_stx7100_glue_driver);
error_glue_driver_register:
error_soc_type:
	return result;
}

static void __exit snd_stm_stx7100_exit(void)
{
	snd_stm_printd(0, "%s()\n", __func__);

	snd_stm_card_free();

	snd_stm_remove_platform_devices(snd_stm_stx7100_devices,
			ARRAY_SIZE(snd_stm_stx7100_devices));

	platform_driver_unregister(&snd_stm_stx7100_glue_driver);
}

MODULE_AUTHOR("Pawel Moll <pawel.moll@st.com>");
MODULE_DESCRIPTION("STMicroelectronics STx7100/STx7109 audio driver");
MODULE_LICENSE("GPL");

module_init(snd_stm_stx7100_init);
module_exit(snd_stm_stx7100_exit);
