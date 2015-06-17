/*
 *   STMicrolectronics STx7200 SoC description & audio glue driver
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
#include <linux/platform_device.h>
#include <linux/stm/clk.h>
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
static char *id = "STx7200"; /* Default card ID */

/* CUT 2+ ONLY! As PCM Reader #1 shares pins with MII1 it may receive
 * unwanted traffic if MII1 is actually used to networking,
 * or when PCM Player #1 is configured to use these pins. In such
 * case one may disable the reader input using this module parameter. */
static int pcm_reader_1_enabled = 1;

module_param(index, int, 0444);
MODULE_PARM_DESC(index, "Index value for STx7200 audio subsystem card.");
module_param(id, charp, 0444);
MODULE_PARM_DESC(id, "ID string for STx7200 audio subsystem card.");
module_param(pcm_reader_1_enabled, int, 0444);
MODULE_PARM_DESC(id, "PCM Reader #1 control (not valid for STx7200 cut 1).");



/*
 * Audio subsystem components & platform devices
 */

/* Audio subsystem glue */

static struct platform_device stx7200_glue = {
	.name = "snd_stx7200_glue",
	.id = -1,
	.num_resources = 1,
	.resource = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd601200,
			.end = 0xfd60120b,
		},
	}
};

/* Internal DACs */

static struct platform_device stx7200_conv_dac_mem_0 = {
	.name = "snd_conv_dac_mem",
	.id = 0,
	.num_resources = 1,
	.resource = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd601400,
			.end = 0xfd601403,
		},
	},
	.dev.platform_data = &(struct snd_stm_conv_dac_mem_info) {
		/* .ver = see snd_stm_stx7200_init() */
		.source_bus_id = "snd_pcm_player.0",
		.channel_from = 0,
		.channel_to = 1,
	},
};

static struct platform_device stx7200_conv_dac_mem_1 = {
	.name = "snd_conv_dac_mem",
	.id = 1,
	.num_resources = 1,
	.resource = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd601500,
			.end = 0xfd601503,
		},
	},
	.dev.platform_data = &(struct snd_stm_conv_dac_mem_info) {
		/* .ver = see snd_stm_stx7200_init() */
		.source_bus_id = "snd_pcm_player.1",
		.channel_from = 0,
		.channel_to = 1,
	},
};

/* PCM players connected to internal DACs */

static struct platform_device stx7200_pcm_player_0 = {
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
			.start = ILC_IRQ(39),
			.end = ILC_IRQ(39),
		},
	},
	.dev.platform_data = &(struct snd_stm_pcm_player_info) {
		.name = "PCM player #0",
		/* .ver = see snd_stm_stx7200_init() */
		.card_device = 0,
		.clock_name = "pcm0_clk",
		.channels = 2,
		.fdma_initiator = 0,
		.fdma_request_line = 33,
	},
};

static struct platform_device stx7200_pcm_player_1 = {
	.name = "snd_pcm_player",
	.id = 1,
	.num_resources = 2,
	.resource = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd102000,
			.end = 0xfd102027,
		}, {
			.flags = IORESOURCE_IRQ,
			.start = ILC_IRQ(40),
			.end = ILC_IRQ(40),
		},
	},
	.dev.platform_data = &(struct snd_stm_pcm_player_info) {
		.name = "PCM player #1",
		/* .ver = see snd_stm_stx7200_init() */
		.card_device = 1,
		.clock_name = "pcm1_clk",
		.channels = 6,
		.fdma_initiator = 0,
		.fdma_request_line = 34,
	},
};

/* PCM players with digital outputs */

static struct platform_device stx7200_pcm_player_2 = {
	.name = "snd_pcm_player",
	.id = 2,
	.num_resources = 2,
	.resource = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd103000,
			.end = 0xfd103027,
		}, {
			.flags = IORESOURCE_IRQ,
			.start = ILC_IRQ(41),
			.end = ILC_IRQ(41),
		},
	},
	.dev.platform_data = &(struct snd_stm_pcm_player_info) {
		.name = "PCM player #2",
		/* .ver = see snd_stm_stx7200_init() */
		.card_device = 2,
		.clock_name = "pcm2_clk",
		.channels = 8,
		.fdma_initiator = 0,
		.fdma_request_line = 35,
	},
};

static struct platform_device stx7200_pcm_player_3 = {
	.name = "snd_pcm_player",
	.id = 3,
	.num_resources = 2,
	.resource = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd104000,
			.end = 0xfd104027,
		}, {
			.flags = IORESOURCE_IRQ,
			.start = ILC_IRQ(42),
			.end = ILC_IRQ(42),
		},
	},
	.dev.platform_data = &(struct snd_stm_pcm_player_info) {
		.name = "PCM player #3",
		/* .ver = see snd_stm_stx7200_init() */
		.card_device = 3,
		.clock_name = "pcm3_clk",
		.channels = 10,
		.fdma_initiator = 0,
		.fdma_request_line = 36,
	},
};

/* SPDIF player */

static struct platform_device stx7200_spdif_player = {
	.name = "snd_spdif_player",
	.id = 0,
	.num_resources = 2,
	.resource = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd105000,
			/* .end = see snd_stm_stx7200_init() */
		}, {
			.flags = IORESOURCE_IRQ,
			.start = ILC_IRQ(37),
			.end = ILC_IRQ(37),
		},
	},
	.dev.platform_data = &(struct snd_stm_spdif_player_info) {
		.name = "SPDIF player",
		/* .ver = see snd_stm_stx7200_init() */
		.card_device = 5,
		.clock_name = "spdif_clk",
		.fdma_initiator = 0,
		.fdma_request_line = 38,
	},
};

/* HDMI output devices
 * Cut 1.0: Please note that "HDTVOutBaseAddress" (0xFD10C000) from page 54
 * of "7200 Programming Manual, Volume 2" is wrong. The correct HDMI players
 * subsystem base address is "HDMIPlayerBaseAddress" (0xFD106000) from
 * page 488 of the manual.
 * Cut 2.0: HDTVout IP is identical to STx7111's one and the base address
 * is 0xFD112000. */

static struct platform_device stx7200_hdmi_pcm_player = {
	.name = "snd_pcm_player",
	.id = 4, /* HDMI PCM player is no. 4 */
	.num_resources = 2,
	.resource = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			/* .start = see snd_stm_stx7200_init() */
			/* .end = see snd_stm_stx7200_init() */
		}, {
			.flags = IORESOURCE_IRQ,
			.start = ILC_IRQ(62),
			.end = ILC_IRQ(62),
		},
	},
	.dev.platform_data = &(struct snd_stm_pcm_player_info) {
		.name = "PCM player HDMI",
		/* .ver = see snd_stm_stx7200_init() */
		.card_device = 4,
		.clock_name = "hdmi_clk",
		.channels = 8,
		.fdma_initiator = 0,
		.fdma_request_line = 39,
	},
};

static struct platform_device stx7200_hdmi_spdif_player = {
	.name = "snd_spdif_player",
	.id = 1, /* HDMI SPDIF player is no. 1 */
	.num_resources = 2,
	.resource = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			/* .start = see snd_stm_stx7200_init() */
			/* .end = see snd_stm_stx7200_init() */
		}, {
			.flags = IORESOURCE_IRQ,
			.start = ILC_IRQ(63),
			.end = ILC_IRQ(63),
		},
	},
	.dev.platform_data = &(struct snd_stm_spdif_player_info) {
		.name = "SPDIF player HDMI",
		/* .ver = see snd_stm_stx7200_init() */
		.card_device = 6,
		.clock_name = "hdmi_clk",
		.fdma_initiator = 0,
		.fdma_request_line = 40,
	},
};

/* Not available in cut 1.0! */
static struct platform_device stx7200_conv_i2sspdif_0 = {
	.name = "snd_conv_i2sspdif",
	.id = 0,
	.num_resources = 2,
	.resource = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd113000,
			.end = 0xfd113223,
		}, {
			.flags = IORESOURCE_IRQ,
			.start = ILC_IRQ(64),
			.end = ILC_IRQ(64),
		}
	},
	.dev.platform_data = &(struct snd_stm_conv_i2sspdif_info) {
		.ver = 4,
		.source_bus_id = "snd_pcm_player.4",
		.channel_from = 0,
		.channel_to = 1,
	},
};

/* Not available in cut 1.0! */
static struct platform_device stx7200_conv_i2sspdif_1 = {
	.name = "snd_conv_i2sspdif",
	.id = 1,
	.num_resources = 2,
	.resource = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd113400,
			.end = 0xfd113623,
		}, {
			.flags = IORESOURCE_IRQ,
			.start = ILC_IRQ(65),
			.end = ILC_IRQ(65),
		}
	},
	.dev.platform_data = &(struct snd_stm_conv_i2sspdif_info) {
		.ver = 4,
		.source_bus_id = "snd_pcm_player.4",
		.channel_from = 2,
		.channel_to = 3,
	},
};

/* Not available in cut 1.0! */
static struct platform_device stx7200_conv_i2sspdif_2 = {
	.name = "snd_conv_i2sspdif",
	.id = 2,
	.num_resources = 2,
	.resource = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd113800,
			.end = 0xfd113a23,
		}, {
			.flags = IORESOURCE_IRQ,
			.start = ILC_IRQ(66),
			.end = ILC_IRQ(66),
		}
	},
	.dev.platform_data = &(struct snd_stm_conv_i2sspdif_info) {
		.ver = 4,
		.source_bus_id = "snd_pcm_player.4",
		.channel_from = 4,
		.channel_to = 5,
	},
};

/* Not available in cut 1.0! */
static struct platform_device stx7200_conv_i2sspdif_3 = {
	.name = "snd_conv_i2sspdif",
	.id = 3,
	.num_resources = 2,
	.resource = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd113c00,
			.end = 0xfd113e23,
		}, {
			.flags = IORESOURCE_IRQ,
			.start = ILC_IRQ(67),
			.end = ILC_IRQ(67),
		}
	},
	.dev.platform_data = &(struct snd_stm_conv_i2sspdif_info) {
		.ver = 4,
		.source_bus_id = "snd_pcm_player.4",
		.channel_from = 6,
		.channel_to = 7,
	},
};

/* PCM readers */

static struct platform_device stx7200_pcm_reader_0 = {
	.name = "snd_pcm_reader",
	.id = 0,
	.num_resources = 2,
	.resource = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd100000,
			.end = 0xfd100027,
		}, {
			.flags = IORESOURCE_IRQ,
			.start = ILC_IRQ(38),
			.end = ILC_IRQ(38),
		},
	},
	.dev.platform_data = &(struct snd_stm_pcm_reader_info) {
		.name = "PCM Reader #0",
		/* .ver = see snd_stm_stx7200_init() */
		.card_device = 7,
		.channels = 2,
		.fdma_initiator = 0,
		.fdma_request_line = 37,
	},
};

/* Not available in cut 1.0! */
static struct platform_device stx7200_pcm_reader_1 = {
	.name = "snd_pcm_reader",
	.id = 1,
	.num_resources = 2,
	.resource = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd114000,
			.end = 0xfd114027,
		}, {
			.flags = IORESOURCE_IRQ,
			.start = ILC_IRQ(35),
			.end = ILC_IRQ(35),
		},
	},
	.dev.platform_data = &(struct snd_stm_pcm_reader_info) {
		.name = "PCM Reader #1",
		.ver = 5,
		.card_device = 8,
		.channels = 8,
		.fdma_initiator = 0,
		.fdma_request_line = 51,
	},
};



static struct platform_device *snd_stm_stx7200_devices[] = {
	&stx7200_glue,
	&stx7200_conv_dac_mem_0,
	&stx7200_conv_dac_mem_1,
	&stx7200_pcm_player_0,
	&stx7200_pcm_player_1,
	&stx7200_pcm_player_2,
	&stx7200_pcm_player_3,
	&stx7200_spdif_player,
	&stx7200_hdmi_pcm_player,
	&stx7200_hdmi_spdif_player,
	&stx7200_pcm_reader_0,
};

static struct platform_device *snd_stm_stx7200_i2sspdif_devices[] = {
	&stx7200_conv_i2sspdif_0,
	&stx7200_conv_i2sspdif_1,
	&stx7200_conv_i2sspdif_2,
	&stx7200_conv_i2sspdif_3,
};

static struct platform_device *snd_stm_stx7200_pcm_reader_1_device[] = {
	&stx7200_pcm_reader_1,
};



/*
 * Audio glue driver implementation
 */

#define IOMUX_CTRL(base)	((base) + 0x00)
#define PCM_CLK_EN		0
#define PCM_CLK_EN__INPUT	(0 << PCM_CLK_EN)
#define PCM_CLK_EN__OUTPUT	(1 << PCM_CLK_EN)
#define DATA0_EN		1
#define DATA0_EN__INPUT		(0 << DATA0_EN)
#define DATA0_EN__OUTPUT	(1 << DATA0_EN)
#define DATA1_EN		2
#define DATA1_EN__INPUT		(0 << DATA1_EN)
#define DATA1_EN__OUTPUT	(1 << DATA1_EN)
#define DATA2_EN		3
#define DATA2_EN__INPUT		(0 << DATA2_EN)
#define DATA2_EN__OUTPUT	(1 << DATA2_EN)
#define SPDIF_EN		4
#define SPDIF_EN__DISABLE	(0 << SPDIF_EN)
#define SPDIF_EN__ENABLE	(1 << SPDIF_EN)
#define PCMRDR1_EN		5
#define PCMRDR1_EN__DISABLE	(0 << PCMRDR1_EN)
#define PCMRDR1_EN__ENABLE	(1 << PCMRDR1_EN)
#define HDMI_CTRL(base)		((base) + 0x04)
#define RECOVERY_CTRL(base)	((base) + 0x08)

struct snd_stm_stx7200_glue {
	struct resource *mem_region;
	void *base;

	struct snd_info_entry *proc_entry;

	snd_stm_magic_field;
};

static void snd_stm_stx7200_glue_dump_registers(struct snd_info_entry *entry,
		struct snd_info_buffer *buffer)
{
	struct snd_stm_stx7200_glue *stx7200_glue = entry->private_data;

	BUG_ON(!stx7200_glue);
	BUG_ON(!snd_stm_magic_valid(stx7200_glue));

	snd_iprintf(buffer, "--- snd_stx7200_glue ---\n");
	snd_iprintf(buffer, "IOMUX_CTRL (0x%p) = 0x%08x\n",
			IOMUX_CTRL(stx7200_glue->base),
			readl(IOMUX_CTRL(stx7200_glue->base)));
	snd_iprintf(buffer, "HDMI_CTRL (0x%p) = 0x%08x\n",
			HDMI_CTRL(stx7200_glue->base),
			readl(HDMI_CTRL(stx7200_glue->base)));
	snd_iprintf(buffer, "RECOVERY_CTRL (0x%p) = 0x%08x\n",
			RECOVERY_CTRL(stx7200_glue->base),
			readl(RECOVERY_CTRL(stx7200_glue->base)));

	snd_iprintf(buffer, "\n");
}

static int __init snd_stm_stx7200_glue_probe(struct platform_device *pdev)
{
	int result = 0;
	struct snd_stm_stx7200_glue *stx7200_glue;
	unsigned long value;

	snd_stm_printd(0, "%s()\n", __func__);

	stx7200_glue = kzalloc(sizeof(*stx7200_glue), GFP_KERNEL);
	if (!stx7200_glue) {
		snd_stm_printe("Can't allocate memory "
				"for a device description!\n");
		result = -ENOMEM;
		goto error_alloc;
	}
	snd_stm_magic_set(stx7200_glue);

	result = snd_stm_memory_request(pdev, &stx7200_glue->mem_region,
			&stx7200_glue->base);
	if (result < 0) {
		snd_stm_printe("Memory region request failed!\n");
		goto error_memory_request;
	}

	/* Enable audio outputs */
	value = SPDIF_EN__ENABLE | DATA2_EN__OUTPUT | DATA1_EN__OUTPUT |
			DATA0_EN__OUTPUT | PCM_CLK_EN__OUTPUT;

	/* Enable PCM Reader #1 (well, in some cases) */
	if (cpu_data->cut_major > 1 && pcm_reader_1_enabled)
		value |= PCMRDR1_EN__ENABLE;

	writel(value, IOMUX_CTRL(stx7200_glue->base));

	/* Additional procfs info */
	snd_stm_info_register(&stx7200_glue->proc_entry, "stx7200_glue",
			snd_stm_stx7200_glue_dump_registers, stx7200_glue);

	/* Done now */

	platform_set_drvdata(pdev, stx7200_glue);

	return result;

error_memory_request:
	snd_stm_magic_clear(stx7200_glue);
	kfree(stx7200_glue);
error_alloc:
	return result;
}

static int __exit snd_stm_stx7200_glue_remove(struct platform_device *pdev)
{
	struct snd_stm_stx7200_glue *stx7200_glue = platform_get_drvdata(pdev);
	unsigned long value;

	BUG_ON(!stx7200_glue);
	BUG_ON(!snd_stm_magic_valid(stx7200_glue));

	/* Remove procfs entry */
	snd_stm_info_unregister(stx7200_glue->proc_entry);

	/* Disable audio outputs */
	value = SPDIF_EN__DISABLE | DATA2_EN__INPUT | DATA1_EN__INPUT |
			DATA0_EN__INPUT | PCM_CLK_EN__INPUT;

	/* Disable PCM Reader #1 (well, in some cases) */
	if (cpu_data->cut_major > 1 && pcm_reader_1_enabled)
		value |= PCMRDR1_EN__DISABLE;

	writel(value, IOMUX_CTRL(stx7200_glue->base));

	snd_stm_memory_release(stx7200_glue->mem_region, stx7200_glue->base);

	snd_stm_magic_clear(stx7200_glue);
	kfree(stx7200_glue);

	return 0;
}

static struct platform_driver snd_stm_stx7200_glue_driver = {
	.driver.name = "snd_stx7200_glue",
	.probe = snd_stm_stx7200_glue_probe,
	.remove = snd_stm_stx7200_glue_remove,
};



/*
 * Audio initialization
 */

#define SET_VER(_info_struct_, _device_, _ver_) \
		(((struct _info_struct_ *)_device_.dev.platform_data)->ver = \
		_ver_)

static int __init snd_stm_stx7200_init(void)
{
	int result;
	struct snd_card *card;
	int ver;

	snd_stm_printd(0, "snd_stm_stx7200_init()\n");

	if (cpu_data->type != CPU_STX7200) {
		snd_stm_printe("Not supported (other than STx7200) SOC "
				"detected!\n");
		result = -EINVAL;
		goto error_soc_type;
	}

	/* We assume below that MEM resource is first, lets check it... */
	BUG_ON(stx7200_spdif_player.resource[0].flags != IORESOURCE_MEM);
	BUG_ON(stx7200_hdmi_pcm_player.resource[0].flags != IORESOURCE_MEM);
	BUG_ON(stx7200_hdmi_spdif_player.resource[0].flags != IORESOURCE_MEM);

	if (cpu_data->cut_major < 2) {
		SET_VER(snd_stm_pcm_player_info, stx7200_pcm_player_0, 5);
		SET_VER(snd_stm_pcm_player_info, stx7200_pcm_player_1, 5);
		SET_VER(snd_stm_pcm_player_info, stx7200_pcm_player_2, 5);
		SET_VER(snd_stm_pcm_player_info, stx7200_pcm_player_3, 5);

		SET_VER(snd_stm_spdif_player_info, stx7200_spdif_player, 3);
		stx7200_spdif_player.resource[0].end = 0xfd10503f;

		SET_VER(snd_stm_pcm_player_info, stx7200_hdmi_pcm_player, 5);
		stx7200_hdmi_pcm_player.resource[0].start = 0xfd106d00;
		stx7200_hdmi_pcm_player.resource[0].end = 0xfd106d27;

		SET_VER(snd_stm_spdif_player_info,
				stx7200_hdmi_spdif_player, 3);
		stx7200_hdmi_spdif_player.resource[0].start = 0xfd106c00;
		stx7200_hdmi_spdif_player.resource[0].end = 0xfd106c3f;

		SET_VER(snd_stm_pcm_reader_info, stx7200_pcm_reader_0, 3);
	} else {
		SET_VER(snd_stm_pcm_player_info, stx7200_pcm_player_0, 6);
		SET_VER(snd_stm_pcm_player_info, stx7200_pcm_player_1, 6);
		SET_VER(snd_stm_pcm_player_info, stx7200_pcm_player_2, 6);
		SET_VER(snd_stm_pcm_player_info, stx7200_pcm_player_3, 6);

		SET_VER(snd_stm_spdif_player_info, stx7200_spdif_player, 4);
		stx7200_spdif_player.resource[0].end = 0xfd105043;

		SET_VER(snd_stm_pcm_player_info, stx7200_hdmi_pcm_player, 6);
		stx7200_hdmi_pcm_player.resource[0].start = 0xfd112d00;
		stx7200_hdmi_pcm_player.resource[0].end = 0xfd112d27;

		SET_VER(snd_stm_spdif_player_info,
				stx7200_hdmi_spdif_player, 4);
		stx7200_hdmi_spdif_player.resource[0].start = 0xfd112c00;
		stx7200_hdmi_spdif_player.resource[0].end = 0xfd112c43;

		ver = (cpu_data->cut_major == 2 ? 5 : 6);
		SET_VER(snd_stm_pcm_reader_info, stx7200_pcm_reader_0, ver);
		SET_VER(snd_stm_pcm_reader_info, stx7200_pcm_reader_1, ver);
	}

	/* Ugly but quick hack to have HDMI SPDIF player & I2S to SPDIF
	 * converters enabled without loading STMFB...
	 * TODO: do this in some sane way! */
	{
		void *hdmi_gpout;

		if (cpu_data->cut_major == 1)
			hdmi_gpout = ioremap(0xfd106020, 4);
		else
			hdmi_gpout = ioremap(0xfd112020, 4);

		writel(readl(hdmi_gpout) | 0x3, hdmi_gpout);
		iounmap(hdmi_gpout);
	}

	result = platform_driver_register(&snd_stm_stx7200_glue_driver);
	result = 0;
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
	strcpy(card->driver, "STx7200");
	strcpy(card->shortname, "STx7200 audio subsystem");
	snprintf(card->longname, 79, "STMicroelectronics STx7200 cut %d "
			"SOC audio subsystem", cpu_data->cut_major);

	result = snd_stm_add_platform_devices(snd_stm_stx7200_devices,
			ARRAY_SIZE(snd_stm_stx7200_devices));
	if (result != 0) {
		snd_stm_printe("Failed to add platform devices!\n");
		goto error_add_devices;
	}

	if (cpu_data->cut_major > 1) {
		result = snd_stm_add_platform_devices(
				snd_stm_stx7200_i2sspdif_devices,
				ARRAY_SIZE(snd_stm_stx7200_i2sspdif_devices));
		if (result != 0) {
			snd_stm_printe("Failed to add I2S-SPDIF converters "
					"platform devices!\n");
			goto error_add_i2sspdif_devices;
		}
	}

	if (cpu_data->cut_major > 1 && pcm_reader_1_enabled) {
		result = snd_stm_add_platform_devices(
				snd_stm_stx7200_pcm_reader_1_device, 1);
		if (result != 0) {
			snd_stm_printe("Failed to add PCM Reader #1 "
					"platform device!\n");
			goto error_add_pcm_reader_1_device;;
		}
	}

	result = snd_stm_card_register();
	if (result != 0) {
		snd_stm_printe("Failed to register ALSA cards (%d)!\n", result);
		goto error_card_register;
	}

	return 0;

error_card_register:
	if (cpu_data->cut_major > 1 && pcm_reader_1_enabled)
		snd_stm_remove_platform_devices(
				snd_stm_stx7200_pcm_reader_1_device, 1);
error_add_pcm_reader_1_device:
	if (cpu_data->cut_major > 1)
		snd_stm_remove_platform_devices(
				snd_stm_stx7200_i2sspdif_devices,
				ARRAY_SIZE(snd_stm_stx7200_i2sspdif_devices));
error_add_i2sspdif_devices:
	snd_stm_remove_platform_devices(snd_stm_stx7200_devices,
			ARRAY_SIZE(snd_stm_stx7200_devices));
error_add_devices:
	snd_stm_card_free();
error_card_new:
	platform_driver_unregister(&snd_stm_stx7200_glue_driver);
error_glue_driver_register:
error_soc_type:
	return result;
}

static void __exit snd_stm_stx7200_exit(void)
{
	snd_stm_printd(0, "snd_stm_stx7200_exit()\n");

	snd_stm_card_free();

	if (cpu_data->cut_major > 1 && pcm_reader_1_enabled)
		snd_stm_remove_platform_devices(
				snd_stm_stx7200_pcm_reader_1_device, 1);

	if (cpu_data->cut_major > 1)
		snd_stm_remove_platform_devices(
				snd_stm_stx7200_i2sspdif_devices,
				ARRAY_SIZE(snd_stm_stx7200_i2sspdif_devices));

	snd_stm_remove_platform_devices(snd_stm_stx7200_devices,
			ARRAY_SIZE(snd_stm_stx7200_devices));

	platform_driver_unregister(&snd_stm_stx7200_glue_driver);
}

MODULE_AUTHOR("Pawel Moll <pawel.moll@st.com>");
MODULE_DESCRIPTION("STMicroelectronics STx7200 audio driver");
MODULE_LICENSE("GPL");

module_init(snd_stm_stx7200_init);
module_exit(snd_stm_stx7200_exit);
