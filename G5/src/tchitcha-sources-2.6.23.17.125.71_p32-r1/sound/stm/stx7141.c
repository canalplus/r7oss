/*
 *   STMicrolectronics STx7141 SoC description & audio glue driver
 *
 *   Copyright (c) 2005-2007 STMicroelectronics Limited
 *
 *   Author: Stephen Gallimore <stephen.gallimore@st.com>
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
#include <asm/irq-ilc.h>
#include <sound/driver.h>
#include <sound/core.h>

#include "common.h"



static int snd_stm_debug_level;
module_param_named(debug, snd_stm_debug_level, int, S_IRUGO | S_IWUSR);



/*
 * ALSA module parameters
 */

static int index = -1; /* First available index */
static char *id = "STx7141"; /* Default card ID */

module_param(index, int, 0444);
MODULE_PARM_DESC(index, "Index value for STx7141 audio subsystem card.");
module_param(id, charp, 0444);
MODULE_PARM_DESC(id, "ID string for STx7141 audio subsystem card.");



/*
 * Audio subsystem components & platform devices
 */

/* Audio subsystem glue */

static struct platform_device stx7141_glue = {
	.name = "snd_stx7141_glue",
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

static struct platform_device stx7141_conv_dac_mem = {
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

static struct platform_device stx7141_pcm_player_0 = {
	.name = "snd_pcm_player",
	.id = 0,
	.num_resources = 2,
	.resource = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd101000,
			.end = 0xfd101027,
		}, {
			.flags = IORESOURCE_IRQ,
			.start = ILC_IRQ(101),
			.end = ILC_IRQ(101),
		},
	},
	.dev.platform_data = &(struct snd_stm_pcm_player_info) {
		.name = "PCM player #0",
		.ver = 6,
		.card_device = 0,
		.clock_name = "CLKC_FS0_CH4",
		.channels = 10,
		.fdma_initiator = 0,
		.fdma_request_line = 39,
	},
};

static struct platform_device stx7141_pcm_player_1 = {
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
			.start = ILC_IRQ(102),
			.end = ILC_IRQ(102),
		},
	},
	.dev.platform_data = &(struct snd_stm_pcm_player_info) {
		.name = "PCM player #1",
		.ver = 6,
		.card_device = 1,
		.clock_name = "CLKC_FS0_CH2",
		.channels = 2,
		.fdma_initiator = 0,
		.fdma_request_line = 40,
	},
};

static struct platform_device stx7141_pcm_player_2 = {
	.name = "snd_pcm_player",
	.id = 2,
	.num_resources = 2,
	.resource = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd104d00,
			.end = 0xfd104d27,
		}, {
			.flags = IORESOURCE_IRQ,
			.start = ILC_IRQ(137),
			.end = ILC_IRQ(137),
		},
	},
	.dev.platform_data = &(struct snd_stm_pcm_player_info) {
		.name = "PCM player HDMI",
		.ver = 6,
		.card_device = 2,
		.clock_name = "CLKC_FS0_CH1",
		.channels = 8,
		.fdma_initiator = 0,
		.fdma_request_line = 47,
	},
};


/*
 * SPDIF player
 */

static struct platform_device stx7141_spdif_player = {
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
			.start = ILC_IRQ(136),
			.end = ILC_IRQ(136),
		},
	},
	.dev.platform_data = &(struct snd_stm_spdif_player_info) {
		.name = "SPDIF player (HDMI)",
		.ver = 4,
		.card_device = 3,
		.clock_name = "CLKC_FS0_CH3",
		.fdma_initiator = 0,
		.fdma_request_line = 48,
	},
};

/* I2S to SPDIF converters */

static struct platform_device stx7141_conv_i2sspdif_0 = {
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
			.start = ILC_IRQ(135),
			.end = ILC_IRQ(135),
		}
	},
	.dev.platform_data = &(struct snd_stm_conv_i2sspdif_info) {
		.ver = 4,
		.source_bus_id = "snd_pcm_player.2",
		.channel_from = 0,
		.channel_to = 1,
	},
};

static struct platform_device stx7141_conv_i2sspdif_1 = {
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
			.start = ILC_IRQ(134),
			.end = ILC_IRQ(134),
		}
	},
	.dev.platform_data = &(struct snd_stm_conv_i2sspdif_info) {
		.ver = 4,
		.source_bus_id = "snd_pcm_player.2",
		.channel_from = 2,
		.channel_to = 3,
	},
};

static struct platform_device stx7141_conv_i2sspdif_2 = {
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
			.start = ILC_IRQ(133),
			.end = ILC_IRQ(133),
		}
	},
	.dev.platform_data = &(struct snd_stm_conv_i2sspdif_info) {
		.ver = 4,
		.source_bus_id = "snd_pcm_player.2",
		.channel_from = 4,
		.channel_to = 5,
	},
};

static struct platform_device stx7141_conv_i2sspdif_3 = {
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
			.start = ILC_IRQ(132),
			.end = ILC_IRQ(132),
		}
	},
	.dev.platform_data = &(struct snd_stm_conv_i2sspdif_info) {
		.ver = 4,
		.source_bus_id = "snd_pcm_player.2",
		.channel_from = 6,
		.channel_to = 7,
	},
};

/* PCM reader */

static struct platform_device stx7141_pcm_reader_0 = {
	.name = "snd_pcm_reader",
	.id = 0,
	.num_resources = 2,
	.resource = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd102000,
			.end = 0xfd102027,
		}, {
			.flags = IORESOURCE_IRQ,
			.start = ILC_IRQ(103),
			.end = ILC_IRQ(103),
		},
	},
	.dev.platform_data = &(struct snd_stm_pcm_reader_info) {
		.name = "PCM Reader #0",
		.ver = 4,
		.card_device = 4,
		.channels = 2,
		.fdma_initiator = 0,
		.fdma_request_line = 41,
	},
};

static struct platform_device stx7141_pcm_reader_1 = {
	.name = "snd_pcm_reader",
	.id = 1,
	.num_resources = 2,
	.resource = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd103000,
			.end = 0xfd103027,
		}, {
			.flags = IORESOURCE_IRQ,
			.start = ILC_IRQ(104),
			.end = ILC_IRQ(104),
		},
	},
	.dev.platform_data = &(struct snd_stm_pcm_reader_info) {
		.name = "PCM Reader #1",
		.ver = 4,
		.card_device = 5,
		.channels = 2,
		.fdma_initiator = 0,
		.fdma_request_line = 42,
	},
};

static struct platform_device *snd_stm_stx7141_devices[] = {
	&stx7141_glue,
	&stx7141_conv_dac_mem,
	&stx7141_pcm_player_0,
	&stx7141_pcm_player_1,
	&stx7141_pcm_player_2,
	&stx7141_spdif_player,
	&stx7141_conv_i2sspdif_0,
	&stx7141_conv_i2sspdif_1,
	&stx7141_conv_i2sspdif_2,
	&stx7141_conv_i2sspdif_3,
	&stx7141_pcm_reader_0,
	&stx7141_pcm_reader_1,
};



/*
 * Audio glue driver implementation
 */

#define IO_CTRL(base)				((base) + 0x00)
#define CLKREC_SEL				9
#define CLKREC_SEL__PCMPLHDMI			(0 << CLKREC_SEL)
#define CLKREC_SEL__SPDIFHDMI			(1 << CLKREC_SEL)
#define CLKREC_SEL__PCMPL1			(2 << CLKREC_SEL)
#define CLKREC_SEL__PCMPL0			(3 << CLKREC_SEL)
#define PCMR1_SCLK_INV_SEL			12
#define PCMR1_SCLK_INV_SEL__NO_INVERSION	(0 << PCMR1_SCLK_INV_SEL)
#define PCMR1_SCLK_INV_SEL__INVERSION		(1 << PCMR1_SCLK_INV_SEL)
#define PCMR1_LRCLK_RET_SEL			13
#define PCMR1_LRCLK_RET_SEL__NO_RETIMIMG	(0 << PCMR1_LRCLK_RET_SEL)
#define PCMR1_LRCLK_RET_SEL__RETIME_BY_1_CYCLE	(1 << PCMR1_LRCLK_RET_SEL)
#define PCMR1_SCLK_SEL				14
#define PCMR1_SCLK_SEL__FROM_PAD		(0 << PCMR1_SCLK_SEL)
#define PCMR1_SCLK_SEL__FROM_PCM_PLAYER_0	(1 << PCMR1_SCLK_SEL)
#define PCMR1_LRCLK_SEL				15
#define PCMR1_LRCLK_SEL__FROM_PAD		(0 << PCMR1_LRCLK_SEL)
#define PCMR1_LRCLK_SEL__FROM_PCM_PLAYER_0	(1 << PCMR1_LRCLK_SEL)

struct snd_stm_stx7141_glue {
	struct resource *mem_region;
	void *base;

	struct snd_info_entry *proc_entry;

	snd_stm_magic_field;
};

static void snd_stm_stx7141_glue_dump_registers(struct snd_info_entry *entry,
		struct snd_info_buffer *buffer)
{
	struct snd_stm_stx7141_glue *stx7141_glue = entry->private_data;

	BUG_ON(!stx7141_glue);
	BUG_ON(!snd_stm_magic_valid(stx7141_glue));

	snd_iprintf(buffer, "--- snd_stx7141_glue ---\n");
	snd_iprintf(buffer, "IO_CTRL (0x%p) = 0x%08x\n",
			IO_CTRL(stx7141_glue->base),
			readl(IO_CTRL(stx7141_glue->base)));

	snd_iprintf(buffer, "\n");
}

static int __init snd_stm_stx7141_glue_probe(struct platform_device *pdev)
{
	int result = 0;
	struct snd_stm_stx7141_glue *stx7141_glue;

	snd_stm_printd(0, "%s()\n", __func__);

	stx7141_glue = kzalloc(sizeof(*stx7141_glue), GFP_KERNEL);
	if (!stx7141_glue) {
		snd_stm_printe("Can't allocate memory "
				"for a device description!\n");
		result = -ENOMEM;
		goto error_alloc;
	}
	snd_stm_magic_set(stx7141_glue);

	result = snd_stm_memory_request(pdev, &stx7141_glue->mem_region,
			&stx7141_glue->base);
	if (result < 0) {
		snd_stm_printe("Memory region request failed!\n");
		goto error_memory_request;
	}

	/* Additional procfs info */
	snd_stm_info_register(&stx7141_glue->proc_entry, "stx7141_glue",
			snd_stm_stx7141_glue_dump_registers, stx7141_glue);

	platform_set_drvdata(pdev, stx7141_glue);

	return result;

error_memory_request:
	snd_stm_magic_clear(stx7141_glue);
	kfree(stx7141_glue);
error_alloc:
	return result;
}

static int __exit snd_stm_stx7141_glue_remove(struct platform_device *pdev)
{
	struct snd_stm_stx7141_glue *stx7141_glue = platform_get_drvdata(pdev);

	BUG_ON(!stx7141_glue);
	BUG_ON(!snd_stm_magic_valid(stx7141_glue));

	snd_stm_info_unregister(stx7141_glue->proc_entry);

	snd_stm_memory_release(stx7141_glue->mem_region, stx7141_glue->base);

	snd_stm_magic_clear(stx7141_glue);
	kfree(stx7141_glue);

	return 0;
}

static struct platform_driver snd_stm_stx7141_glue_driver = {
	.driver.name = "snd_stx7141_glue",
	.probe = snd_stm_stx7141_glue_probe,
	.remove = snd_stm_stx7141_glue_remove,
};



/*
 * Audio initialization
 */

static int __init snd_stm_stx7141_init(void)
{
	int result;
	struct snd_card *card;

	snd_stm_printd(0, "snd_stm_stx7141_init()\n");

	if (cpu_data->type != CPU_STX7141) {
		snd_stm_printe("Not supported (other than STx7141) SOC "
				"detected!\n");
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

	result = platform_driver_register(&snd_stm_stx7141_glue_driver);
	if (result != 0) {
		snd_stm_printe("Failed to register audio glue driver!\n");
		goto error_driver_register;
	}

	card = snd_stm_card_new(index, id, THIS_MODULE);
	if (card == NULL) {
		snd_stm_printe("ALSA card creation failed!\n");
		result = -ENOMEM;
		goto error_card_new;
	}
	strcpy(card->driver, "STx7141");
	strcpy(card->shortname, "STx7141 audio subsystem");
	snprintf(card->longname, 79, "STMicroelectronics STx7141 cut %d "
			"SOC audio subsystem", cpu_data->cut_major);

	result = snd_stm_add_platform_devices(snd_stm_stx7141_devices,
			ARRAY_SIZE(snd_stm_stx7141_devices));
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
	snd_stm_remove_platform_devices(snd_stm_stx7141_devices,
			ARRAY_SIZE(snd_stm_stx7141_devices));
error_add_devices:
	snd_stm_card_free();
error_card_new:
	platform_driver_unregister(&snd_stm_stx7141_glue_driver);
error_driver_register:
error_soc_type:
	return result;
}

static void __exit snd_stm_stx7141_exit(void)
{
	snd_stm_printd(0, "snd_stm_stx7141_exit()\n");

	snd_stm_card_free();

	snd_stm_remove_platform_devices(snd_stm_stx7141_devices,
			ARRAY_SIZE(snd_stm_stx7141_devices));

	platform_driver_unregister(&snd_stm_stx7141_glue_driver);
}

MODULE_AUTHOR("Stephen Gallimore <stephen.gallimore@st.com>");
MODULE_DESCRIPTION("STMicroelectronics STx7141 audio driver");
MODULE_LICENSE("GPL");

module_init(snd_stm_stx7141_init);
module_exit(snd_stm_stx7141_exit);
