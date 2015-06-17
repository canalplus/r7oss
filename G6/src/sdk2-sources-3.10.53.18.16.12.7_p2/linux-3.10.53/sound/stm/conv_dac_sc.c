/*
 *   STMicroelectronics System-on-Chips' internal (sysconf controlled)
 *   audio DAC driver
 *
 *   Copyright (c) 2005-2013 STMicroelectronics Limited
 *
 *   Author: John Boddie <john.boddie@st.com>
 *           Pawel Moll <pawel.moll@st.com>
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
#include <linux/io.h>
#include <linux/list.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>
#include <sound/core.h>
#include <sound/info.h>
#include "common.h"
#include <linux/reset.h>

static int snd_stm_debug_level;
module_param_named(debug, snd_stm_debug_level, int, S_IRUGO | S_IWUSR);

/*
 * Hardware-related definitions
 */

#define FORMAT (SND_STM_FORMAT__I2S | SND_STM_FORMAT__SUBFRAME_32_BITS)
#define OVERSAMPLING 256

static struct of_device_id conv_dac_sc_match[];

/* Regfield IDs */
enum {
	CONVDACSC_NOT_STANDBY,
	CONVDACSC_STANDBY,
	CONVDACSC_SOFTMUTE,
	CONVDACSC_MUTE_L,
	CONVDACSC_MUTE_R,
	CONVDACSC_MODE,
	CONVDACSC_ANALOG_NOT_PWR_DW,
	CONVDACSC_ANALOG_STANDBY,
	CONVDACSC_ANALOG_PWR_DW,
	CONVDACSC_ANALOG_NOT_PWR_DW_BG,
	/* keep last */
	MAX_REGFIELDS
};

/*
 * Internal DAC instance structure
 */

struct conv_dac_sc_sysconf_regfields {
	bool avail;
	struct reg_field regfield;
};

struct conv_dac_sc {
	struct device *dev;

	/* System informations */
	struct snd_stm_conv_converter *converter;
	const char *bus_id;
	struct device_node *source_bus_id;
	const char *source_bus_id_description;

	int channel_to;
	int channel_from;

	unsigned int format;
	int oversampling;
	struct reset_control *nrst;

	/* sysconf registers bank map */
	struct regmap *sysconf_regmap;
	/* sysconf register bit fields */
	const struct conv_dac_sc_sysconf_regfields *sysconf_regfields;
	/* Control bits */
	struct regmap_field *mode;
	struct regmap_field *nsb;
	struct regmap_field *sb;
	struct regmap_field *softmute;
	struct regmap_field *mute_l;
	struct regmap_field *mute_r;
	struct regmap_field *sbana;
	struct regmap_field *npdana;
	struct regmap_field *pdana;
	struct regmap_field *pndbg;

	snd_stm_magic_field;
};

/*
 * sysconf registers configuration of given platform
 */

static const struct conv_dac_sc_sysconf_regfields
	conv_dac_sc_sysconf_regfields_stih416[MAX_REGFIELDS] = {
	[CONVDACSC_NOT_STANDBY] = {true, REG_FIELD(STIH416_AUDIO_DAC_CTRL, 3, 3)},
	[CONVDACSC_SOFTMUTE] = {true, REG_FIELD(STIH416_AUDIO_DAC_CTRL, 4, 4)},
	[CONVDACSC_MODE] = {true, REG_FIELD(STIH416_AUDIO_DAC_CTRL, 1, 2)},
	[CONVDACSC_ANALOG_PWR_DW] = {true, REG_FIELD(STIH416_AUDIO_DAC_CTRL, 5, 5)},
	[CONVDACSC_ANALOG_NOT_PWR_DW_BG] = {true, REG_FIELD(STIH416_AUDIO_DAC_CTRL, 6, 6)},
};

static const struct conv_dac_sc_sysconf_regfields
	conv_dac_sc_sysconf_regfields_stih407[MAX_REGFIELDS] = {
	[CONVDACSC_STANDBY] = {true, REG_FIELD(STIH407_AUDIO_DAC_CTRL, 2, 2)},
	[CONVDACSC_SOFTMUTE] = {true, REG_FIELD(STIH407_AUDIO_DAC_CTRL, 0, 0)},
	[CONVDACSC_ANALOG_STANDBY] = {true, REG_FIELD(STIH407_AUDIO_DAC_CTRL, 1, 1)},
};

/*
 * Converter interface implementation
 */

static unsigned int conv_dac_sc_get_format(void *priv)
{
	struct conv_dac_sc *conv = priv;

	BUG_ON(!conv);
	BUG_ON(!snd_stm_magic_valid(conv));

	dev_dbg(conv->dev, "%s(priv=%p)", __func__, priv);

	return conv->format;
}

static int conv_dac_sc_get_oversampling(void *priv)
{
	struct conv_dac_sc *conv = priv;

	BUG_ON(!conv);
	BUG_ON(!snd_stm_magic_valid(conv));

	dev_dbg(conv->dev, "%s(priv=%p)", __func__, priv);

	return conv->oversampling;
}

static int conv_dac_sc_set_enabled(int enabled, void *priv)
{
	struct conv_dac_sc *conv = priv;

	BUG_ON(!conv);
	BUG_ON(!snd_stm_magic_valid(conv));

	dev_dbg(conv->dev, "%s(enabled=%d, priv=%p)", __func__, enabled, priv);

	dev_dbg(conv->dev, "%sabling DAC for %s",
			enabled ? "En" : "Dis", conv->bus_id);

	if (enabled) {
		/* Take the DAC out of standby */
		if (conv->nsb)
			regmap_field_write(conv->nsb, 1);
		if (conv->sb)
			regmap_field_write(conv->sb, 0);

		/* Take the DAC out of reset */
		if (conv->nrst)
			reset_control_deassert(conv->nrst);
	} else {
		/* Put the DAC into reset */
		if (conv->nrst)
			reset_control_assert(conv->nrst);

		/* Put the DAC into standby */
		if (conv->nsb)
			regmap_field_write(conv->nsb, 0);
		if (conv->sb)
			regmap_field_write(conv->sb, 1);
	}

	return 0;
}

static int conv_dac_sc_set_muted(int muted, void *priv)
{
	struct conv_dac_sc *conv = priv;

	BUG_ON(!conv);
	BUG_ON(!snd_stm_magic_valid(conv));

	dev_dbg(conv->dev, "%s(muted=%d, priv=%p)", __func__, muted, priv);

	dev_dbg(conv->dev, "%suting DAC for %s", muted ? "M" : "Unm",
			conv->bus_id);

	if (conv->softmute)
		regmap_field_write(conv->softmute, muted ? 1 : 0);
	if (conv->mute_l)
		regmap_field_write(conv->mute_l, muted ? 1 : 0);
	if (conv->mute_r)
		regmap_field_write(conv->mute_r, muted ? 1 : 0);

	return 0;
}

static struct snd_stm_conv_ops conv_dac_sc_ops = {
	.get_format	  = conv_dac_sc_get_format,
	.get_oversampling = conv_dac_sc_get_oversampling,
	.set_enabled	  = conv_dac_sc_set_enabled,
	.set_muted	  = conv_dac_sc_set_muted,
};

/*
 * ALSA low-level device implementation
 */

static int conv_dac_sc_register(struct snd_device *snd_device)
{
	struct conv_dac_sc *conv = snd_device->device_data;

	BUG_ON(!conv);
	BUG_ON(!snd_stm_magic_valid(conv));

	dev_dbg(conv->dev, "%s(snd_device=%p)", __func__, snd_device);

	/* Put the DAC into reset */
	if (conv->nrst)
		reset_control_assert(conv->nrst);

	/* Put the DAC into standby */
	if (conv->nsb)
		regmap_field_write(conv->nsb, 0);
	if (conv->sb)
		regmap_field_write(conv->sb, 1);

	/* Mute the DAC */
	if (conv->softmute)
		regmap_field_write(conv->softmute, 1);
	if (conv->mute_l)
		regmap_field_write(conv->mute_l, 1);
	if (conv->mute_r)
		regmap_field_write(conv->mute_r, 1);

	/* Take the DAC analog bits out of standby */
	if (conv->mode)
		regmap_field_write(conv->mode, 0);
	if (conv->npdana)
		regmap_field_write(conv->npdana, 0);
	if (conv->sbana)
		regmap_field_write(conv->sbana, 0);
	if (conv->pdana)
		regmap_field_write(conv->pdana, 1);
	if (conv->pndbg)
		regmap_field_write(conv->pndbg, 1);

	return 0;
}

static int conv_dac_sc_disconnect(struct snd_device *snd_device)
{
	struct conv_dac_sc *conv = snd_device->device_data;

	BUG_ON(!conv);
	BUG_ON(!snd_stm_magic_valid(conv));

	dev_dbg(conv->dev, "%s(snd_device=%p)", __func__, snd_device);

	/* Put the DAC into reset */
	if (conv->nrst)
		reset_control_assert(conv->nrst);

	/* Put the DAC into standby */
	if (conv->nsb)
		regmap_field_write(conv->nsb, 0);
	if (conv->sb)
		regmap_field_write(conv->sb, 1);

	/* Mute the DAC */
	if (conv->softmute)
		regmap_field_write(conv->softmute, 1);
	if (conv->mute_l)
		regmap_field_write(conv->mute_l, 1);
	if (conv->mute_r)
		regmap_field_write(conv->mute_r, 1);

	/* Put the DAC analog bits into standby */
	if (conv->mode)
		regmap_field_write(conv->mode, 0);
	if (conv->npdana)
		regmap_field_write(conv->npdana, 1);
	if (conv->sbana)
		regmap_field_write(conv->sbana, 1);
	if (conv->pdana)
		regmap_field_write(conv->pdana, 0);
	if (conv->pndbg)
		regmap_field_write(conv->pndbg, 0);

	return 0;
}

static struct snd_device_ops conv_dac_sc_snd_device_ops = {
	.dev_register	= conv_dac_sc_register,
	.dev_disconnect	= conv_dac_sc_disconnect,
};

/*
 * Platform driver routines
 */

static int conv_dac_sc_parse_dt(struct platform_device *pdev,
		struct conv_dac_sc *conv)
{
	struct device_node *pnode = pdev->dev.of_node;
	int val;

	/* get source_bus_id node */
	val = get_property_hdl(&pdev->dev, pnode, "source-bus-id", 0);
	if (!val) {
		dev_err(&pdev->dev, "unable to find DT source-bus-id node");
		return -EINVAL;
	}
	conv->source_bus_id = of_find_node_by_phandle(val);
	of_property_read_string(conv->source_bus_id, "description",
				&conv->source_bus_id_description);

	/* Read the device and channel properties */
	of_property_read_u32(pnode, "channel-to", &conv->channel_to);
	of_property_read_u32(pnode, "channel-from", &conv->channel_from);
	of_property_read_u32(pnode, "format", &conv->format);
	of_property_read_u32(pnode, "oversampling", &conv->oversampling);

	conv->sysconf_regmap =
		syscon_regmap_lookup_by_phandle(pnode, "st,syscfg");
	if (!conv->sysconf_regmap) {
		dev_err(conv->dev, "No syscfg phandle specified\n");
		return -EINVAL;
	}

	conv->sysconf_regfields =
		of_match_node(conv_dac_sc_match, pnode)->data;
	if (!conv->sysconf_regfields) {
		dev_err(conv->dev, "compatible data not provided\n");
		return -EINVAL;
	}

	return 0;
}

static void conv_dac_sc_sysconf_claim(struct conv_dac_sc *conv)
{

	/* Read the device standby sysconf properties */
	if (conv->sysconf_regfields[CONVDACSC_NOT_STANDBY].avail == true)
		conv->nsb = regmap_field_alloc(conv->sysconf_regmap,
			conv->sysconf_regfields[CONVDACSC_NOT_STANDBY].regfield);
	if (conv->sysconf_regfields[CONVDACSC_STANDBY].avail == true)
		conv->sb = regmap_field_alloc(conv->sysconf_regmap,
			conv->sysconf_regfields[CONVDACSC_STANDBY].regfield);


	/* Read the device mute sysconf properties */
	if (conv->sysconf_regfields[CONVDACSC_SOFTMUTE].avail == true)
		conv->softmute = regmap_field_alloc(conv->sysconf_regmap,
			conv->sysconf_regfields[CONVDACSC_SOFTMUTE].regfield);
	if (conv->sysconf_regfields[CONVDACSC_MUTE_L].avail == true)
		conv->mute_l = regmap_field_alloc(conv->sysconf_regmap,
			conv->sysconf_regfields[CONVDACSC_MUTE_L].regfield);
	if (conv->sysconf_regfields[CONVDACSC_MUTE_R].avail == true)
		conv->mute_r = regmap_field_alloc(conv->sysconf_regmap,
			conv->sysconf_regfields[CONVDACSC_MUTE_R].regfield);

	/* Read the device mode sysconf property */
	if (conv->sysconf_regfields[CONVDACSC_MODE].avail == true)
		conv->mode = regmap_field_alloc(conv->sysconf_regmap,
			conv->sysconf_regfields[CONVDACSC_MODE].regfield);

	/* Read the device analog sysconf properties */
	if (conv->sysconf_regfields[CONVDACSC_ANALOG_NOT_PWR_DW].avail == true)
		conv->npdana = regmap_field_alloc(conv->sysconf_regmap,
			conv->sysconf_regfields[CONVDACSC_ANALOG_NOT_PWR_DW].regfield);
	if (conv->sysconf_regfields[CONVDACSC_ANALOG_STANDBY].avail == true)
		conv->sbana = regmap_field_alloc(conv->sysconf_regmap,
			conv->sysconf_regfields[CONVDACSC_ANALOG_STANDBY].regfield);
	if (conv->sysconf_regfields[CONVDACSC_ANALOG_PWR_DW].avail == true)
		conv->pdana = regmap_field_alloc(conv->sysconf_regmap,
			conv->sysconf_regfields[CONVDACSC_ANALOG_PWR_DW].regfield);
	if (conv->sysconf_regfields[CONVDACSC_ANALOG_NOT_PWR_DW_BG].avail == true)
		conv->pndbg = regmap_field_alloc(conv->sysconf_regmap,
			conv->sysconf_regfields[CONVDACSC_ANALOG_NOT_PWR_DW_BG].regfield);
}

static int conv_dac_sc_probe(struct platform_device *pdev)
{
	int result = 0;
	struct conv_dac_sc *conv;
	struct snd_card *card = snd_stm_card_get(SND_STM_CARD_TYPE_AUDIO);

	BUG_ON(!card);

	snd_stm_printd(0, "%s('%s')\n", __func__, dev_name(&pdev->dev));

	/* Allocate driver structure */
	conv = devm_kzalloc(&pdev->dev, sizeof(*conv), GFP_KERNEL);
	if (!conv) {
		dev_err(&pdev->dev, "Failed to allocate driver structure");
		return -ENOMEM;
	}

	snd_stm_magic_set(conv);
	conv->dev = &pdev->dev;
	conv->bus_id = dev_name(&pdev->dev);

	/* Get resources */
	if (!pdev->dev.of_node) {
		dev_err(conv->dev, "device not in dt");
		return -EINVAL;
	}

	/* Get soft reset control */
	conv->nrst = devm_reset_control_get(&pdev->dev, "dac_nrst");
	if (IS_ERR(conv->nrst)) {
		dev_info(conv->dev, "audio dac soft reset control not defined\n");
		conv->nrst = NULL;
	}

	if (conv_dac_sc_parse_dt(pdev, conv)) {
		dev_err(conv->dev, "invalid dt");
		return -EINVAL;
	}

	/* Ensure valid bus id */
	if (!conv->source_bus_id) {
		dev_err(conv->dev, "Invalid source bus id!");
		return -EINVAL;
	}

	/* Check if we need to use the default format */
	if (conv->format == 0)
		conv->format = FORMAT;

	/* Check if we need to use the default oversampling */
	if (conv->oversampling == 0)
		conv->oversampling = OVERSAMPLING;

	/* claim sysconf */
	conv_dac_sc_sysconf_claim(conv);

	/* SoC must have either 'not-standby' or 'standby' sysconf */
	if (!conv->nsb && !conv->sb) {
		dev_err(conv->dev, "Failed to claim any standby sysconf!");
		return -EINVAL;
	}

	/* SoC must have either 'softmute' or 'mute-left' & 'mute-right' */
	if (!conv->softmute && !conv->mute_l && !conv->mute_r) {
		dev_err(conv->dev, "Failed to claim any mute sysconf!");
		return -EINVAL;
	}

	dev_dbg(&pdev->dev, "Attached to %s", conv->source_bus_id_description);

	/* Register the converter */
	conv->converter = snd_stm_conv_register_converter(
			"DAC SC", &conv_dac_sc_ops, conv,
			&platform_bus_type, conv->source_bus_id,
			conv->channel_from, conv->channel_to, NULL);
	if (!conv->converter) {
		dev_err(conv->dev, "Can't attach to player!");
		result = -ENODEV;
		goto error_attach;
	}

	/* Create ALSA low-level device */
	result = snd_device_new(card, SNDRV_DEV_LOWLEVEL, conv,
			&conv_dac_sc_snd_device_ops);
	if (result < 0) {
		dev_err(conv->dev, "ALSA low-level device creation failed!");
		goto error_device;
	}

	/* Save converter in driver data */
	platform_set_drvdata(pdev, conv);

	return 0;

error_device:
	snd_stm_conv_unregister_converter(conv->converter);
error_attach:
	snd_stm_magic_clear(conv);
	return result;
}

static int conv_dac_sc_remove(struct platform_device *pdev)
{
	struct conv_dac_sc *conv = platform_get_drvdata(pdev);

	BUG_ON(!conv);
	BUG_ON(!snd_stm_magic_valid(conv));

	snd_stm_conv_unregister_converter(conv->converter);

	if (conv->nsb)
		regmap_field_free(conv->nsb);
	if (conv->sb)
		regmap_field_free(conv->sb);
	if (conv->softmute)
		regmap_field_free(conv->softmute);
	if (conv->mute_l)
		regmap_field_free(conv->mute_l);
	if (conv->mute_r)
		regmap_field_free(conv->mute_r);
	if (conv->mode)
		regmap_field_free(conv->mode);
	if (conv->sbana)
		regmap_field_free(conv->sbana);
	if (conv->npdana)
		regmap_field_free(conv->npdana);
	if (conv->pdana)
		regmap_field_free(conv->pdana);
	if (conv->pndbg)
		regmap_field_free(conv->pndbg);

	snd_stm_magic_clear(conv);

	return 0;
}

/*
 * Module initialization.
 */

static struct of_device_id conv_dac_sc_match[] = {
	{
		.compatible = "st,snd_conv_dac_sc_stih416",
		.data = conv_dac_sc_sysconf_regfields_stih416
	},
	{
		.compatible = "st,snd_conv_dac_sc_stih407",
		.data = conv_dac_sc_sysconf_regfields_stih407
	},
	{},
};

MODULE_DEVICE_TABLE(of, conv_dac_sc_match);

static struct platform_driver conv_dac_sc_platform_driver = {
	.driver.name	= "snd_conv_dac_sc",
	.driver.of_match_table = conv_dac_sc_match,
	.probe		= conv_dac_sc_probe,
	.remove		= conv_dac_sc_remove,
};

module_platform_driver(conv_dac_sc_platform_driver);

MODULE_AUTHOR("John Boddie <john.boddie@st.com>");
MODULE_DESCRIPTION("STMicroelectronics sysconf-based DAC converter driver");
MODULE_LICENSE("GPL");
