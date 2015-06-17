/*
 *   STMicrolectronics Freeman 510/520/530/540 (FLI7510/FLI7520/FLI7530/
 *   FLI7540) SoC description & audio glue driver
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
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
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
static char *id = "FLI7510"; /* Default card ID */

module_param(index, int, 0444);
MODULE_PARM_DESC(index, "Index value for Freeman 510/520/530/540 audio "
		"subsystem card.");
module_param(id, charp, 0444);
MODULE_PARM_DESC(id, "ID string for Freeman 510/520/530/540 audio "
		"subsystem card.");



/*
 * Audio subsystem components & platform devices
 */

/* Audio configuration */

static struct platform_device fli7510_config = {
	.name = "snd_fli7510_config",
	.id = -1,
	.num_resources = 1,
	.resource = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd670000,
			.end = 0xfd67001b,
		},
	}
};

/* PCM players */

static struct platform_device fli7510_pcm_player_0 = {
	.name = "snd_pcm_player",
	.id = 0,
	.num_resources = 2,
	.resource = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd618000,
			.end = 0xfd618027,
		}, {
			.flags = IORESOURCE_IRQ,
			.start = ILC_IRQ(10),
			.end = ILC_IRQ(10),
		},
	},
	.dev.platform_data = &(struct snd_stm_pcm_player_info) {
		.name = "PCM player LS",
		.ver = 6,
		.card_device = 0,
		.clock_name = "clk_256fs_dec_1",
		.channels = 8,
		.fdma_initiator = 0,
		.fdma_request_line = 41,
	},
};

static struct platform_device fli7510_pcm_player_1 = {
	.name = "snd_pcm_player",
	.id = 1,
	.num_resources = 2,
	.resource = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd61c000,
			.end = 0xfd61c027,
		}, {
			.flags = IORESOURCE_IRQ,
			.start = ILC_IRQ(11),
			.end = ILC_IRQ(11),
		},
	},
	.dev.platform_data = &(struct snd_stm_pcm_player_info) {
		.name = "PCM player AUX/Encoder",
		.ver = 6,
		.card_device = 1,
		.clock_name = "clk_256fs_dec_2",
		.channels = 2,
		.fdma_initiator = 0,
		.fdma_request_line = 42,
	},
};

static struct platform_device fli7510_pcm_player_2 = {
	.name = "snd_pcm_player",
	.id = 2,
	.num_resources = 2,
	.resource = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd620000,
			.end = 0xfd620027,
		}, {
			.flags = IORESOURCE_IRQ,
			.start = ILC_IRQ(12),
			.end = ILC_IRQ(12),
		},
	},
	.dev.platform_data = &(struct snd_stm_pcm_player_info) {
		.name = "PCM player HP",
		.ver = 6,
		.card_device = 2,
		.clock_name = "clk_256fs_dec_2",
		.channels = 2,
		.fdma_initiator = 0,
		.fdma_request_line = 43,
	},
};

static struct platform_device fli7510_pcm_player_3 = {
	.name = "snd_pcm_player",
	.id = 3,
	.num_resources = 2,
	.resource = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd628000,
			.end = 0xfd628027,
		}, {
			.flags = IORESOURCE_IRQ,
			.start = ILC_IRQ(13),
			.end = ILC_IRQ(13),
		},
	},
	.dev.platform_data = &(struct snd_stm_pcm_player_info) {
		.name = "PCM player Monitor",
		.ver = 6,
		.card_device = 3,
		.clock_name = "clk_256fs_dec_2",
		.channels = 2,
		.fdma_initiator = 0,
		.fdma_request_line = 44,
	},
};

static struct platform_device fli7520_pcm_player_3 = {
	.name = "snd_pcm_player",
	.id = 3,
	.num_resources = 2,
	.resource = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd630000,
			.end = 0xfd630027,
		}, {
			.flags = IORESOURCE_IRQ,
			.start = ILC_IRQ(14),
			.end = ILC_IRQ(14),
		},
	},
	.dev.platform_data = &(struct snd_stm_pcm_player_info) {
		.name = "PCM player AV out",
		.ver = 6,
		.card_device = 3,
		.clock_name = "clk_256fs_dec_2",
		.channels = 2,
		.fdma_initiator = 0,
		.fdma_request_line = 44,
	},
};

static struct platform_device fli7510_pcm_player_4 = {
	.name = "snd_pcm_player",
	.id = 4,
	.num_resources = 2,
	.resource = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd630000,
			.end = 0xfd630027,
		}, {
			.flags = IORESOURCE_IRQ,
			.start = ILC_IRQ(14),
			.end = ILC_IRQ(14),
		},
	},
	.dev.platform_data = &(struct snd_stm_pcm_player_info) {
		.name = "PCM player AV out",
		.ver = 6,
		.card_device = 4,
		.clock_name = "clk_256fs_dec_2",
		.channels = 2,
		.fdma_initiator = 0,
		.fdma_request_line = 45,
	},
};

/* SPDIF player */

static struct platform_device fli7510_spdif_player = {
	.name = "snd_spdif_player",
	.id = -1,
	.num_resources = 2,
	.resource = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd638000,
			.end = 0xfd638043,
		}, {
			.flags = IORESOURCE_IRQ,
			.start = ILC_IRQ(4),
			.end = ILC_IRQ(4),
		},
	},
	.dev.platform_data = &(struct snd_stm_spdif_player_info) {
		.name = "SPDIF player",
		.ver = 4,
		.card_device = 5,
		.clock_name = "clk_256fs_free_run",
		.fdma_initiator = 0,
		.fdma_request_line = 40,
	},
};

/* PCM readers */

static struct platform_device fli7510_pcm_reader_0 = {
	.name = "snd_pcm_reader",
	.id = 0,
	.num_resources = 2,
	.resource = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd604000,
			.end = 0xfd604027,
		}, {
			.flags = IORESOURCE_IRQ,
			.start = ILC_IRQ(5),
			.end = ILC_IRQ(5),
		},
	},
	.dev.platform_data = &(struct snd_stm_pcm_reader_info) {
		.name = "PCM Reader SPDIF",
		.ver = 5,
		.card_device = 6,
		.channels = 2,
		.fdma_initiator = 0,
		.fdma_request_line = 46,
	},
};

static struct platform_device fli7510_pcm_reader_1 = {
	.name = "snd_pcm_reader",
	.id = 1,
	.num_resources = 2,
	.resource = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd608000,
			.end = 0xfd608027,
		}, {
			.flags = IORESOURCE_IRQ,
			.start = ILC_IRQ(6),
			.end = ILC_IRQ(6),
		},
	},
	.dev.platform_data = &(struct snd_stm_pcm_reader_info) {
		.name = "PCM Reader HDMI",
		.ver = 5,
		.card_device = 7,
		.channels = 8,
		.fdma_initiator = 0,
		.fdma_request_line = 47,
	},
};

static struct platform_device fli7510_pcm_reader_2 = {
	.name = "snd_pcm_reader",
	.id = 2,
	.num_resources = 2,
	.resource = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd60c000,
			.end = 0xfd60c027,
		}, {
			.flags = IORESOURCE_IRQ,
			.start = ILC_IRQ(7),
			.end = ILC_IRQ(7),
		},
	},
	.dev.platform_data = &(struct snd_stm_pcm_reader_info) {
		.name = "PCM Reader I2S",
		.ver = 5,
		.card_device = 8,
		.channels = 2,
		.fdma_initiator = 0,
		.fdma_request_line = 48,
	},
};

static struct platform_device fli7510_pcm_reader_3 = {
	.name = "snd_pcm_reader",
	.id = 3,
	.num_resources = 2,
	.resource = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd610000,
			.end = 0xfd610027,
		}, {
			.flags = IORESOURCE_IRQ,
			.start = ILC_IRQ(8),
			.end = ILC_IRQ(8),
		},
	},
	.dev.platform_data = &(struct snd_stm_pcm_reader_info) {
		.name = "PCM Reader Demod",
		.ver = 5,
		.card_device = 9,
		.channels = 2,
		.fdma_initiator = 0,
		.fdma_request_line = 49,
	},
};

static struct platform_device fli7510_pcm_reader_4 = {
	.name = "snd_pcm_reader",
	.id = 4,
	.num_resources = 2,
	.resource = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd614000,
			.end = 0xfd614027,
		}, {
			.flags = IORESOURCE_IRQ,
			.start = ILC_IRQ(9),
			.end = ILC_IRQ(9),
		},
	},
	.dev.platform_data = &(struct snd_stm_pcm_reader_info) {
		.name = "PCM Reader AV in",
		.ver = 5,
		.card_device = 10,
		.channels = 2,
		.fdma_initiator = 0,
		.fdma_request_line = 50,
	},
};

static struct platform_device fli7520_pcm_reader_5 = {
	.name = "snd_pcm_reader",
	.id = 5,
	.num_resources = 2,
	.resource = (struct resource []) {
		{
			.flags = IORESOURCE_MEM,
			.start = 0xfd628000,
			.end = 0xfd628027,
		}, {
			.flags = IORESOURCE_IRQ,
			.start = ILC_IRQ(13),
			.end = ILC_IRQ(13),
		},
	},
	.dev.platform_data = &(struct snd_stm_pcm_reader_info) {
		.name = "PCM Reader HDMI #2",
		.ver = 5,
		.card_device = 11,
		.channels = 8,
		.fdma_initiator = 0,
		.fdma_request_line = 45,
	},
};



/*
 * Device sets
 */

static struct platform_device *snd_stm_fli7510_devices[] = {
	&fli7510_config,
	&fli7510_pcm_player_0,
	&fli7510_pcm_player_1,
	&fli7510_pcm_player_2,
	&fli7510_pcm_player_3,
	&fli7510_pcm_player_4,
	&fli7510_spdif_player,
	&fli7510_pcm_reader_0,
	&fli7510_pcm_reader_1,
	&fli7510_pcm_reader_2,
	&fli7510_pcm_reader_3,
	&fli7510_pcm_reader_4,
};

static struct platform_device *snd_stm_fli7520_devices[] = {
	&fli7510_config,
	&fli7510_pcm_player_0,
	&fli7510_pcm_player_1,
	&fli7510_pcm_player_2,
	&fli7520_pcm_player_3,
	&fli7510_spdif_player,
	&fli7510_pcm_reader_0,
	&fli7510_pcm_reader_1,
	&fli7510_pcm_reader_2,
	&fli7510_pcm_reader_3,
	&fli7510_pcm_reader_4,
	&fli7520_pcm_reader_5,
};



/*
 * Audio glue driver implementation
 */

#define AUD_CONFIG_REG1(base)		((base) + 0x00)
#define SPDIF_CLK			0
#define SPDIF_CLK__CLK_256FS_FREE_RUN	(0 << SPDIF_CLK)
#define SPDIF_CLK__M_CLK_SPDIF		(1 << SPDIF_CLK)
#define SPDIF_CLK__M_CLK_HDMI		(2 << SPDIF_CLK)
#define SPDIF_CLK__M_CLK_I2S		(3 << SPDIF_CLK)
#define SPDIF_CLK__CLK_256FS_DEC_1	(4 << SPDIF_CLK)
#define SPDIF_CLK__CLK_256FS_DEC_2	(5 << SPDIF_CLK)
#define MAIN_CLK			3
#define MAIN_CLK__CLK_256FS_FREE_RUN	(0 << MAIN_CLK)
#define MAIN_CLK__M_CLK_MAIN		(1 << MAIN_CLK)
#define MAIN_CLK__M_CLK_HDMI		(2 << MAIN_CLK)
#define MAIN_CLK__M_CLK_I2S		(3 << MAIN_CLK)
#define MAIN_CLK__CLK_256FS_DEC_1	(4 << MAIN_CLK)
#define MAIN_CLK__CLK_256FS_DEC_2	(5 << MAIN_CLK)
#define ENC_CLK				6
#define ENC_CLK__CLK_256FS_FREE_RUN	(0 << ENC_CLK)
#define ENC_CLK__M_CLK_ENC		(1 << ENC_CLK)
#define ENC_CLK__M_CLK_HDMI		(2 << ENC_CLK)
#define ENC_CLK__M_CLK_I2S		(3 << ENC_CLK)
#define ENC_CLK__CLK_256FS_DEC_1	(4 << ENC_CLK)
#define ENC_CLK__CLK_256FS_DEC_2	(5 << ENC_CLK)
#define DAC_CLK				9
#define DAC_CLK__CLK_256FS_FREE_RUN	(0 << DAC_CLK)
#define DAC_CLK__M_CLK_DAC		(1 << DAC_CLK)
#define DAC_CLK__M_CLK_HDMI		(2 << DAC_CLK)
#define DAC_CLK__M_CLK_I2S		(3 << DAC_CLK)
#define DAC_CLK__CLK_256FS_DEC_1	(4 << DAC_CLK)
#define DAC_CLK__CLK_256FS_DEC_2	(5 << DAC_CLK)
#define ADC_CLK				12
#define ADC_CLK__CLK_256FS_FREE_RUN	(0 << ADC_CLK)
#define ADC_CLK__M_CLK_ADC		(1 << ADC_CLK)
#define ADC_CLK__M_CLK_HDMI		(2 << ADC_CLK)
#define ADC_CLK__M_CLK_I2S		(3 << ADC_CLK)
#define ADC_CLK__CLK_256FS_DEC_1	(4 << ADC_CLK)
#define ADC_CLK__CLK_256FS_DEC_2	(5 << ADC_CLK)
#define SPDIF_CLK_DIV2_EN		30
#define SPDIF_CLK_DIV2_EN__DISABLED	(0 << SPDIF_CLK_DIV2_EN)
#define SPDIF_CLK_DIV2_EN__ENABLED	(1 << SPDIF_CLK_DIV2_EN)
#define SPDIF_IN_PAD_HYST_EN		31
#define SPDIF_IN_PAD_HYST_EN__DISABLED  (0 << SPDIF_IN_PAD_HYST_EN)
#define SPDIF_IN_PAD_HYST_EN__ENABLED	(1 << SPDIF_IN_PAD_HYST_EN)

#define AUD_CONFIG_REG2(base)		((base) + 0x04)
#define SPDIF				0
#define SPDIF__PLAYER			(0 << SPDIF)
#define SPDIF__READER			(1 << SPDIF)
#define FLI7510_MAIN_I2S		1
#define FLI7510_MAIN_I2S__PCM_PLAYER_0	(0 << FLI7510_MAIN_I2S)
#define FLI7510_MAIN_I2S__PCM_READER_1	(1 << FLI7510_MAIN_I2S)
#define FLI7510_SEC_I2S			2
#define FLI7510_SEC_I2S__PCM_PLAYER_1	(0 << FLI7510_SEC_I2S)
#define FLI7510_SEC_I2S__PCM_PLAYER_2	(1 << FLI7510_SEC_I2S)
#define FLI7510_SEC_I2S__PCM_PLAYER_3	(2 << FLI7510_SEC_I2S)
#define FLI7510_SEC_I2S__PCM_PLAYER_4	(3 << FLI7510_SEC_I2S)
#define FLI7510_SEC_I2S__PCM_READER_0	(4 << FLI7510_SEC_I2S)
#define FLI7510_SEC_I2S__PCM_READER_3	(5 << FLI7510_SEC_I2S)
#define FLI7510_SEC_I2S__PCM_READER_4	(6 << FLI7510_SEC_I2S)
#define FLI7510_SEC_I2S__AATV_I2S0	(7 << FLI7510_SEC_I2S)
#define FLI7520_MAIN_I2S		1
#define FLI7520_MAIN_I2S__PCM_READER_1	(0 << FLI7520_MAIN_I2S)
#define FLI7520_MAIN_I2S__PCM_READER_5	(1 << FLI7520_MAIN_I2S)
#define FLI7520_MAIN_I2S__PCM_PLAYER_0	(2 << FLI7520_MAIN_I2S)
#define FLI7520_MAIN_I2S__AATV_I2S0	(3 << FLI7520_MAIN_I2S)
#define FLI7520_SEC_I2S			3
#define FLI7520_SEC_I2S__PCM_PLAYER_1	(0 << FLI7520_SEC_I2S)
#define FLI7520_SEC_I2S__PCM_PLAYER_2	(1 << FLI7520_SEC_I2S)
#define FLI7520_SEC_I2S__PCM_PLAYER_4	(3 << FLI7520_SEC_I2S)
#define FLI7520_SEC_I2S__PCM_READER_0	(4 << FLI7520_SEC_I2S)
#define FLI7520_SEC_I2S__PCM_READER_3	(5 << FLI7520_SEC_I2S)
#define FLI7520_SEC_I2S__PCM_READER_4	(6 << FLI7520_SEC_I2S)
#define FLI7520_SEC_I2S__AATV_I2S0	(7 << FLI7520_SEC_I2S)
#define SPDIF_PLAYER_EN			31
#define SPDIF_PLAYER_EN__DISABLED	(0 << SPDIF_PLAYER_EN)
#define SPDIF_PLAYER_EN__ENABLED	(1 << SPDIF_PLAYER_EN)

struct snd_stm_fli7510_config {
	struct resource *mem_region;
	void *base;

	struct snd_info_entry *proc_entry;

	snd_stm_magic_field;
};

static void snd_stm_fli7510_config_dump_registers(struct snd_info_entry *entry,
		struct snd_info_buffer *buffer)
{
	struct snd_stm_fli7510_config *fli7510_config = entry->private_data;

	BUG_ON(!fli7510_config);
	BUG_ON(!snd_stm_magic_valid(fli7510_config));

	snd_iprintf(buffer, "--- snd_fli7510_config ---\n");
	snd_iprintf(buffer, "AUD_CONFIG_REG1 (0x%p) = 0x%08x\n",
			AUD_CONFIG_REG1(fli7510_config->base),
			readl(AUD_CONFIG_REG1(fli7510_config->base)));
	snd_iprintf(buffer, "AUD_CONFIG_REG2 (0x%p) = 0x%08x\n",
			AUD_CONFIG_REG2(fli7510_config->base),
			readl(AUD_CONFIG_REG2(fli7510_config->base)));

	snd_iprintf(buffer, "\n");
}

static int __init snd_stm_fli7510_config_probe(struct platform_device *pdev)
{
	int result = 0;
	struct snd_stm_fli7510_config *fli7510_config;
	unsigned int value;

	snd_stm_printd(0, "%s()\n", __func__);

	fli7510_config = kzalloc(sizeof(*fli7510_config), GFP_KERNEL);
	if (!fli7510_config) {
		snd_stm_printe("Can't allocate memory "
				"for a device description!\n");
		result = -ENOMEM;
		goto error_alloc;
	}
	snd_stm_magic_set(fli7510_config);

	result = snd_stm_memory_request(pdev, &fli7510_config->mem_region,
			&fli7510_config->base);
	if (result < 0) {
		snd_stm_printe("Memory region request failed!\n");
		goto error_memory_request;
	}

	/* Clocking configuration:
	 *
	 * clk_256fs_free_run --> SPDIF clock
	 *
	 * clk_256fs_dec_1 -----> MAIN (LS) clock
	 *
	 *                    ,-> ENCODER clock
	 * clk_256fs_dec_2 --<
	 *                    `-> DAC (HP, AUX) clock
	 */
	value = SPDIF_CLK__CLK_256FS_FREE_RUN;
	value |= MAIN_CLK__CLK_256FS_DEC_1;
	value |= ENC_CLK__CLK_256FS_DEC_2;
	value |= DAC_CLK__CLK_256FS_DEC_2;
	value |= SPDIF_CLK_DIV2_EN__DISABLED;
	value |= SPDIF_IN_PAD_HYST_EN__DISABLED;
	writel(value, AUD_CONFIG_REG1(fli7510_config->base));

	value = SPDIF__PLAYER;
	if (cpu_data->type == CPU_FLI7510) {
		value |= FLI7510_MAIN_I2S__PCM_PLAYER_0;
		value |= FLI7510_SEC_I2S__PCM_PLAYER_1;
	} else {
		value |= FLI7520_MAIN_I2S__PCM_PLAYER_0;
		value |= FLI7520_SEC_I2S__PCM_PLAYER_1;
	}
	value |= SPDIF_PLAYER_EN__ENABLED;
	writel(value, AUD_CONFIG_REG2(fli7510_config->base));

	snd_stm_info_register(&fli7510_config->proc_entry, "fli7510_config",
			snd_stm_fli7510_config_dump_registers, fli7510_config);

	platform_set_drvdata(pdev, fli7510_config);

	return result;

error_memory_request:
	snd_stm_magic_clear(fli7510_config);
	kfree(fli7510_config);
error_alloc:
	return result;
}

static int __exit snd_stm_fli7510_config_remove(struct platform_device *pdev)
{
	struct snd_stm_fli7510_config *fli7510_config =
			platform_get_drvdata(pdev);

	snd_stm_printd(0, "%s()\n", __func__);

	BUG_ON(!fli7510_config);
	BUG_ON(!snd_stm_magic_valid(fli7510_config));

	snd_stm_info_unregister(fli7510_config->proc_entry);

	snd_stm_memory_release(fli7510_config->mem_region,
			fli7510_config->base);

	snd_stm_magic_clear(fli7510_config);
	kfree(fli7510_config);

	return 0;
}

static struct platform_driver snd_stm_fli7510_config_driver = {
	.driver.name = "snd_fli7510_config",
	.probe = snd_stm_fli7510_config_probe,
	.remove = snd_stm_fli7510_config_remove,
};



/*
 * Audio initialization
 */

static int __init snd_stm_fli7510_init(void)
{
	int result;
	const char *soc_type;
	struct snd_card *card;

	snd_stm_printd(0, "snd_stm_fli7510_init()\n");

	switch (cpu_data->type) {
	case CPU_FLI7510:
		soc_type = "FLI7510";
		break;
	case CPU_FLI7520:
		soc_type = "FLI7520";
		break;
	case CPU_FLI7530:
		soc_type = "FLI7530";
		break;
	case CPU_FLI7540:
		soc_type = "FLI7540";
		break;
	default:
		snd_stm_printe("Not supported (non-Freeman) SOC detected!\n");
		result = -EINVAL;
		goto error_soc_type;
	}

	result = platform_driver_register(&snd_stm_fli7510_config_driver);
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

	if (cpu_data->type == CPU_FLI7510)
		result = snd_stm_add_platform_devices(snd_stm_fli7510_devices,
				ARRAY_SIZE(snd_stm_fli7510_devices));
	else
		result = snd_stm_add_platform_devices(snd_stm_fli7520_devices,
				ARRAY_SIZE(snd_stm_fli7520_devices));
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
	snd_stm_remove_platform_devices(snd_stm_fli7510_devices,
			ARRAY_SIZE(snd_stm_fli7510_devices));
error_add_devices:
	snd_stm_card_free();
error_card_new:
	platform_driver_unregister(&snd_stm_fli7510_config_driver);
error_glue_driver_register:
error_soc_type:
	return result;
}

static void __exit snd_stm_fli7510_exit(void)
{
	snd_stm_printd(0, "snd_stm_fli7510_exit()\n");

	snd_stm_card_free();

	if (cpu_data->type == CPU_FLI7510)
		snd_stm_remove_platform_devices(snd_stm_fli7510_devices,
				ARRAY_SIZE(snd_stm_fli7510_devices));
	else
		snd_stm_remove_platform_devices(snd_stm_fli7520_devices,
				ARRAY_SIZE(snd_stm_fli7520_devices));

	platform_driver_unregister(&snd_stm_fli7510_config_driver);
}

MODULE_AUTHOR("Pawel Moll <pawel.moll@st.com>");
MODULE_DESCRIPTION("STMicroelectronics Freeman 510/520/530/540 audio driver");
MODULE_LICENSE("GPL");

module_init(snd_stm_fli7510_init);
module_exit(snd_stm_fli7510_exit);
