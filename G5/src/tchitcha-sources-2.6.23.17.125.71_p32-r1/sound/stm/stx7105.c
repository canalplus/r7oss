/*
 *   STMicrolectronics STx7105/STx7106 SoC description & audio glue driver
 *
 *   Copyright (c) 2005-2009 STMicroelectronics Limited
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
#include <sound/driver.h>
#include <sound/core.h>

#include "common.h"



static int snd_stm_debug_level;
module_param_named(debug, snd_stm_debug_level, int, S_IRUGO | S_IWUSR);



/*
 * ALSA module parameters
 */

static int index = -1; /* First available index */
static char *id = "STx7105"; /* Default card ID */

module_param(index, int, 0444);
MODULE_PARM_DESC(index, "Index value for STx7105/STx7106 audio subsystem "
		"card.");
module_param(id, charp, 0444);
MODULE_PARM_DESC(id, "ID string for STx7105/STx7106 audio subsystem card.");



/*
 * Audio subsystem components & platform devices
 */

/* Audio subsystem glue */

static struct platform_device stx7105_glue = {
	.name = "snd_stx7105_glue",
	.id = -1,
	.num_resources = 1,
	.resource = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfe210200,
			.end = 0xfe21020b,
		},
	}
};

/* Internal DAC */

static struct platform_device stx7105_conv_dac_mem = {
	.name = "snd_conv_dac_mem",
	.id = -1,
	.num_resources = 1,
	.resource = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfe210100,
			.end = 0xfe210103,
		},
	},
	.dev.platform_data = &(struct snd_stm_conv_dac_mem_info) {
		.source_bus_id = "snd_pcm_player.1",
		.channel_from = 0,
		.channel_to = 1,
	},
};

/* PCM players  */

static struct platform_device stx7105_pcm_player_0 = {
	.name = "snd_pcm_player",
	.id = 0,
	.num_resources = 2,
	.resource = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd104d00,
			.end = 0xfd104d27,
		}, {
			.flags = IORESOURCE_IRQ,
			.start = evt2irq(0x1400),
			.end = evt2irq(0x1400),
		},
	},
	.dev.platform_data = &(struct snd_stm_pcm_player_info) {
		.name = "PCM player #0 (HDMI)",
		.ver = 6,
		.card_device = 0,
		.clock_name = "CLKC_FS0_CH1",
		.channels = 8,
		.fdma_initiator = 0,
		.fdma_request_line = 39,
	},
};

static struct platform_device stx7105_pcm_player_1 = {
	.name = "snd_pcm_player",
	.id = 1,
	.num_resources = 2,
	.resource = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd101800,
			.end = 0xfd101827,
		}, {
			.flags = IORESOURCE_IRQ,
			.start = evt2irq(0x1420),
			.end = evt2irq(0x1420),
		},
	},
	.dev.platform_data = &(struct snd_stm_pcm_player_info) {
		.name = "PCM player #1",
		.ver = 6,
		.card_device = 1,
		.clock_name = "CLKC_FS0_CH2",
		.channels = 2,
		.fdma_initiator = 0,
		.fdma_request_line = 34,
	},
};

/*
 * SPDIF player
 */

static struct platform_device stx7105_spdif_player = {
	.name = "snd_spdif_player",
	.id = -1,
	.num_resources = 2,
	.resource = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd104c00,
			.end = 0xfd104c43,
		}, {
			.flags = IORESOURCE_IRQ,
			.start = evt2irq(0x1460),
			.end = evt2irq(0x1460),
		},
	},
	.dev.platform_data = &(struct snd_stm_spdif_player_info) {
		.name = "SPDIF player (HDMI)",
		.ver = 4,
		.card_device = 2,
		.clock_name = "CLKC_FS0_CH3",
		.fdma_initiator = 0,
		.fdma_request_line = 40,
	},
};

/* I2S to SPDIF converters */

static struct platform_device stx7105_conv_i2sspdif_0 = {
	.name = "snd_conv_i2sspdif",
	.id = 0,
	.num_resources = 2,
	.resource = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd105000,
			.end = 0xfd105223,
		}, {
			.flags = IORESOURCE_IRQ,
			.start = evt2irq(0x0a00),
			.end = evt2irq(0x0a00),
		}
	},
	.dev.platform_data = &(struct snd_stm_conv_i2sspdif_info) {
		.ver = 4,
		.source_bus_id = "snd_pcm_player.0",
		.channel_from = 0,
		.channel_to = 1,
	},
};

static struct platform_device stx7105_conv_i2sspdif_1 = {
	.name = "snd_conv_i2sspdif",
	.id = 1,
	.num_resources = 2,
	.resource = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd105400,
			.end = 0xfd105623,
		}, {
			.flags = IORESOURCE_IRQ,
			.start = evt2irq(0x0a20),
			.end = evt2irq(0x0a20),
		}
	},
	.dev.platform_data = &(struct snd_stm_conv_i2sspdif_info) {
		.ver = 4,
		.source_bus_id = "snd_pcm_player.0",
		.channel_from = 2,
		.channel_to = 3,
	},
};

static struct platform_device stx7105_conv_i2sspdif_2 = {
	.name = "snd_conv_i2sspdif",
	.id = 2,
	.num_resources = 2,
	.resource = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd105800,
			.end = 0xfd105a23,
		}, {
			.flags = IORESOURCE_IRQ,
			.start = evt2irq(0x0a40),
			.end = evt2irq(0x0a40),
		}
	},
	.dev.platform_data = &(struct snd_stm_conv_i2sspdif_info) {
		.ver = 4,
		.source_bus_id = "snd_pcm_player.0",
		.channel_from = 4,
		.channel_to = 5,
	},
};

static struct platform_device stx7105_conv_i2sspdif_3 = {
	.name = "snd_conv_i2sspdif",
	.id = 3,
	.num_resources = 2,
	.resource = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd105c00,
			.end = 0xfd105e23,
		}, {
			.flags = IORESOURCE_IRQ,
			.start = evt2irq(0x0a60),
			.end = evt2irq(0x0a60),
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

static struct platform_device stx7105_pcm_reader = {
	.name = "snd_pcm_reader",
	.id = -1,
	.num_resources = 2,
	.resource = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd102000,
			.end = 0xfd102027,
		}, {
			.flags = IORESOURCE_IRQ,
			.start = evt2irq(0x1440),
			.end = evt2irq(0x1440),
		},
	},
	.dev.platform_data = &(struct snd_stm_pcm_reader_info) {
		.name = "PCM Reader",
		.ver = 5,
		.card_device = 3,
		.channels = 2,
		.fdma_initiator = 0,
		.fdma_request_line = 37,
	},
};

static struct platform_device *snd_stm_stx7105_devices[] = {
	&stx7105_glue,
	&stx7105_conv_dac_mem,
	&stx7105_pcm_player_0,
	&stx7105_pcm_player_1,
	&stx7105_spdif_player,
	&stx7105_conv_i2sspdif_0,
	&stx7105_conv_i2sspdif_1,
	&stx7105_conv_i2sspdif_2,
	&stx7105_conv_i2sspdif_3,
	&stx7105_pcm_reader,
};



/*
 * Audio glue driver implementation
 */

#define IO_CTRL(base)		((base) + 0x00)
#define PCM_CLK_EN		0
#define PCM_CLK_EN__INPUT	(0 << PCM_CLK_EN)
#define PCM_CLK_EN__OUTPUT	(1 << PCM_CLK_EN)
#define SPDIFHDMI_EN		3
#define SPDIFHDMI_EN__INPUT	(0 << SPDIFHDMI_EN)
#define SPDIFHDMI_EN__OUTPUT	(1 << SPDIFHDMI_EN)
#define PCMPLHDMI_EN		5
#define PCMPLHDMI_EN__INPUT	(0 << PCMPLHDMI_EN)
#define PCMPLHDMI_EN__OUTPUT	(1 << PCMPLHDMI_EN)
#define CLKREC_SEL		9
#define CLKREC_SEL__PCMPLHDMI	(0 << CLKREC_SEL)
#define CLKREC_SEL__SPDIFHDMI	(1 << CLKREC_SEL)
#define CLKREC_SEL__PCMPL1	(2 << CLKREC_SEL)

struct snd_stm_stx7105_glue {
	struct resource *mem_region;
	void *base;

	struct snd_info_entry *proc_entry;

	snd_stm_magic_field;
};

static void snd_stm_stx7105_glue_dump_registers(struct snd_info_entry *entry,
		struct snd_info_buffer *buffer)
{
	struct snd_stm_stx7105_glue *stx7105_glue = entry->private_data;

	BUG_ON(!(stx7105_glue));
	BUG_ON(!snd_stm_magic_valid(stx7105_glue));

	snd_iprintf(buffer, "--- snd_stx7105_glue ---\n");
	snd_iprintf(buffer, "IO_CTRL (0x%p) = 0x%08x\n",
			IO_CTRL(stx7105_glue->base),
			readl(IO_CTRL(stx7105_glue->base)));

	snd_iprintf(buffer, "\n");
}

static int __init snd_stm_stx7105_glue_probe(struct platform_device *pdev)
{
	int result = 0;
	struct snd_stm_stx7105_glue *stx7105_glue;

	snd_stm_printd(0, "%s()\n", __func__);

	stx7105_glue = kzalloc(sizeof(*stx7105_glue), GFP_KERNEL);
	if (!stx7105_glue) {
		snd_stm_printe("Can't allocate memory "
				"for a device description!\n");
		result = -ENOMEM;
		goto error_alloc;
	}
	snd_stm_magic_set(stx7105_glue);

	result = snd_stm_memory_request(pdev, &stx7105_glue->mem_region,
			&stx7105_glue->base);
	if (result < 0) {
		snd_stm_printe("Memory region request failed!\n");
		goto error_memory_request;
	}

	/* Enable audio outputs */
	writel(PCMPLHDMI_EN__OUTPUT | SPDIFHDMI_EN__OUTPUT |
			PCM_CLK_EN__OUTPUT, IO_CTRL(stx7105_glue->base));

	/* Additional procfs info */
	snd_stm_info_register(&stx7105_glue->proc_entry, "stx7105_glue",
			snd_stm_stx7105_glue_dump_registers, stx7105_glue);

	platform_set_drvdata(pdev, stx7105_glue);

	return result;

error_memory_request:
	snd_stm_magic_clear(stx7105_glue);
	kfree(stx7105_glue);
error_alloc:
	return result;
}

static int __exit snd_stm_stx7105_glue_remove(struct platform_device *pdev)
{
	struct snd_stm_stx7105_glue *stx7105_glue =
			platform_get_drvdata(pdev);

	BUG_ON(!(stx7105_glue));
	BUG_ON(!snd_stm_magic_valid(stx7105_glue));

	/* Remove procfs entry */
	snd_stm_info_unregister(stx7105_glue->proc_entry);

	/* Disable audio outputs */
	writel(PCMPLHDMI_EN__INPUT | SPDIFHDMI_EN__INPUT |
			PCM_CLK_EN__INPUT, IO_CTRL(stx7105_glue->base));

	snd_stm_memory_release(stx7105_glue->mem_region, stx7105_glue->base);

	snd_stm_magic_clear(stx7105_glue);
	kfree(stx7105_glue);

	return 0;
}

static struct platform_driver snd_stm_stx7105_glue_driver = {
	.driver.name = "snd_stx7105_glue",
	.probe = snd_stm_stx7105_glue_probe,
	.remove = snd_stm_stx7105_glue_remove,
};



/*
 * Audio initialization
 */

static int __init snd_stm_stx7105_init(void)
{
	int result;
	const char *soc_type;
	struct snd_card *card;

	snd_stm_printd(0, "snd_stm_stx7105_init()\n");

	switch (cpu_data->type) {
	case CPU_STX7105:
		soc_type = "STx7105";
		break;
	case CPU_STX7106:
		soc_type = "STx7106";
		break;
	default:
		snd_stm_printe("Not supported (other than STx7105 or STx7106)"
				" SOC detected!\n");
		result = -EINVAL;
		goto error_soc_type;
	}

	/* Ugly but quick hack to have SPDIF player & I2S to SPDIF
	 * converters enabled without loading STMFB...
	 * TODO: do this in some sane way! */
	{
		void *hdmi_gpout = ioremap(0xfd104020, 4);
		writel(readl(hdmi_gpout) | 0x3, hdmi_gpout);
		iounmap(hdmi_gpout);
	}

	result = platform_driver_register(&snd_stm_stx7105_glue_driver);
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
	snprintf(card->longname, 79, "STMicroelectronics %s cut %d "
			"SOC audio subsystem", soc_type, cpu_data->cut_major);

	result = snd_stm_add_platform_devices(snd_stm_stx7105_devices,
			ARRAY_SIZE(snd_stm_stx7105_devices));
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
	snd_stm_remove_platform_devices(snd_stm_stx7105_devices,
			ARRAY_SIZE(snd_stm_stx7105_devices));
error_add_devices:
	snd_stm_card_free();
error_card_new:
	platform_driver_unregister(&snd_stm_stx7105_glue_driver);
error_glue_driver_register:
error_soc_type:
	return result;
}

static void __exit snd_stm_stx7105_exit(void)
{
	snd_stm_printd(0, "snd_stm_stx7105_exit()\n");

	snd_stm_card_free();

	snd_stm_remove_platform_devices(snd_stm_stx7105_devices,
			ARRAY_SIZE(snd_stm_stx7105_devices));

	platform_driver_unregister(&snd_stm_stx7105_glue_driver);
}

MODULE_AUTHOR("Pawel Moll <pawel.moll@st.com>");
MODULE_DESCRIPTION("STMicroelectronics STx7105 audio driver");
MODULE_LICENSE("GPL");

module_init(snd_stm_stx7105_init);
module_exit(snd_stm_stx7105_exit);
