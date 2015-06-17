/*
 *   STMicroelectronics System-on-Chips' Bi-phase converter driver
 *
 *   Copyright (c) 2011-2013 STMicroelectronics Limited
 *
 *   Author: John Boddie <john.boddie@st.com>
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
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>
#include "common.h"


/*
 * Hardware related definitions
 */

#define FORMAT (SND_STM_FORMAT__SPDIF | SND_STM_FORMAT__SUBFRAME_32_BITS)
#define OVERSAMPLING 128

static struct of_device_id conv_biphase_match[];

/* Regfield IDs */
enum {
	CONVBIPHASE_ENABLE,
	CONVBIPHASE_IDLE,
	/* keep last */
	MAX_REGFIELDS
};

/*
 * Internal instance structure
 */

struct conv_biphase_sysconf_regfields {
	bool avail;
	struct reg_field regfield;
};

struct conv_biphase {
	struct device *dev;

	/* System information */
	struct snd_stm_conv_converter *converter;
	const char *bus_id;
	struct device_node *source_bus_id;
	const char *source_bus_id_description;

	int channel_to;
	int channel_from;

	unsigned int format;
	int oversampling;

	/* sysconf registers bank map */
	struct regmap *sysconf_regmap;
	/* sysconf register bit fields */
	const struct conv_biphase_sysconf_regfields *sysconf_regfields;
	/* Enable bit */
	struct regmap_field *enable;
	struct regmap_field *idle;

	unsigned int idle_value;

	snd_stm_magic_field;
};

/*
 * sysconf registers configuration of given platform
 */

static const struct conv_biphase_sysconf_regfields
	conv_biphase_sysconf_regfields_stih416[MAX_REGFIELDS] = {
	[CONVBIPHASE_ENABLE] = {true, REG_FIELD(STIH416_AUDIO_GLUE_CONFIG, 6, 6)},
	[CONVBIPHASE_IDLE] = {true, REG_FIELD(STIH416_AUDIO_GLUE_CONFIG, 7, 7)},
};

static const struct conv_biphase_sysconf_regfields
	conv_biphase_sysconf_regfields_stih407[MAX_REGFIELDS] = {
	[CONVBIPHASE_ENABLE] = {true, REG_FIELD(STIH407_AUDIO_GLUE_CONFIG, 6, 6)},
	[CONVBIPHASE_IDLE] = {true, REG_FIELD(STIH407_AUDIO_GLUE_CONFIG, 7, 7)},
};

/*
 * Converter interface implementation
 */

static unsigned int conv_biphase_get_format(void *priv)
{
	struct conv_biphase *conv = priv;

	BUG_ON(!conv);
	BUG_ON(!snd_stm_magic_valid(conv));

	dev_dbg(conv->dev, "%s(priv=%p)", __func__, priv);

	return conv->format;
}

static int conv_biphase_get_oversampling(void *priv)
{
	struct conv_biphase *conv = priv;

	BUG_ON(!conv);
	BUG_ON(!snd_stm_magic_valid(conv));

	dev_dbg(conv->dev, "%s(priv=%p)", __func__, priv);

	return conv->oversampling;
}

static int conv_biphase_set_enabled(int enabled, void *priv)
{
	struct conv_biphase *conv = priv;

	BUG_ON(!conv);
	BUG_ON(!snd_stm_magic_valid(conv));

	dev_dbg(conv->dev, "%s(enabled=%d, priv=%p)", __func__, enabled, priv);

	dev_dbg(conv->dev, "%sabling bi-phase formatter for %s",
			enabled ? "En" : "Dis", conv->bus_id);

	regmap_field_write(conv->enable, enabled ? 1 : 0);

	return 0;
}

static int conv_biphase_set_muted(int muted, void *priv)
{
	struct conv_biphase *conv = priv;

	BUG_ON(!conv);
	BUG_ON(!snd_stm_magic_valid(conv));

	dev_dbg(conv->dev, "%s(muted=%d, priv=%p)", __func__, muted, priv);

	/* The bi-phase formatter does not have mute functionality */

	return 0;
}

static struct snd_stm_conv_ops conv_biphase_ops = {
	.get_format	  = conv_biphase_get_format,
	.get_oversampling = conv_biphase_get_oversampling,
	.set_enabled	  = conv_biphase_set_enabled,
	.set_muted	  = conv_biphase_set_muted,
};


/*
 * ALSA low-level device implementation
 */

static int conv_biphase_register(struct snd_device *snd_device)
{
	struct conv_biphase *conv = snd_device->device_data;

	BUG_ON(!conv);
	BUG_ON(!snd_stm_magic_valid(conv));

	dev_dbg(conv->dev, "%s(snd_device=%p)", __func__, snd_device);

	/* Initialise bi-phase formatter to disabled */
	regmap_field_write(conv->enable, 0);

	/* Initialise bi-phase formatter idle value */
	if (conv->idle)
		regmap_field_write(conv->idle, conv->idle_value);

	return 0;
}

static int conv_biphase_disconnect(struct snd_device *snd_device)
{
	struct conv_biphase *conv = snd_device->device_data;

	BUG_ON(!conv);
	BUG_ON(!snd_stm_magic_valid(conv));

	dev_dbg(conv->dev, "%s(snd_device=%p)", __func__, snd_device);

	/* Set bi-phase formatter to disabled */
	regmap_field_write(conv->enable, 0);

	return 0;
}

static struct snd_device_ops conv_biphase_snd_device_ops = {
	.dev_register	= conv_biphase_register,
	.dev_disconnect = conv_biphase_disconnect,
};


/*
 * Platform driver routines
 */

static int conv_biphase_parse_dt(struct platform_device *pdev,
		struct conv_biphase *conv)
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

	/* Read the device properties */
	of_property_read_u32(pnode, "channel-to", &conv->channel_to);
	of_property_read_u32(pnode, "channel-from", &conv->channel_from);
	of_property_read_u32(pnode, "format", &conv->format);
	of_property_read_u32(pnode, "oversampling", &conv->oversampling);
	of_property_read_u32(pnode, "idle-value", &conv->idle_value);

	conv->sysconf_regmap =
		syscon_regmap_lookup_by_phandle(pnode, "st,syscfg");
	if (!conv->sysconf_regmap) {
		dev_err(&pdev->dev, "No syscfg phandle specified\n");
		return -EINVAL;
	}

	conv->sysconf_regfields =
		of_match_node(conv_biphase_match, pnode)->data;
	if (!conv->sysconf_regfields) {
		dev_err(&pdev->dev, "compatible data not provided\n");
		return -EINVAL;
	}

	/* Read the device sysconf property */
	if (conv->sysconf_regfields[CONVBIPHASE_ENABLE].avail == true)
		conv->enable = regmap_field_alloc(conv->sysconf_regmap,
			conv->sysconf_regfields[CONVBIPHASE_ENABLE].regfield);

	if (conv->sysconf_regfields[CONVBIPHASE_IDLE].avail == true)
		conv->idle = regmap_field_alloc(conv->sysconf_regmap,
			conv->sysconf_regfields[CONVBIPHASE_IDLE].regfield);

	return 0;
}


static int conv_biphase_probe(struct platform_device *pdev)
{
	int result = 0;
	struct conv_biphase *conv;
	struct snd_card *card = snd_stm_card_get(SND_STM_CARD_TYPE_AUDIO);

	BUG_ON(!card);

	dev_dbg(&pdev->dev, "%s(pdev=%p)", __func__, pdev);

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
	if (conv_biphase_parse_dt(pdev, conv)) {
		dev_err(conv->dev, "invalid dt");
		return -EINVAL;
	}

	/* Check if we need to use the default format */
	if (conv->format == 0)
		conv->format = FORMAT;

	/* Check if we need to use the default oversampling */
	if (conv->oversampling == 0)
		conv->oversampling = OVERSAMPLING;

	/* Ensure valid enable sysconf and bus id */
	if (!conv->enable) {
		dev_err(conv->dev, "Failed to claim enable sysconf!");
		return -EINVAL;
	}

	if (!conv->source_bus_id) {
		dev_err(conv->dev, "Invalid source bus id!");
		return -EINVAL;
	}

	dev_dbg(&pdev->dev, "Attached to %s", conv->source_bus_id_description);

	/* Register the converter */
	conv->converter = snd_stm_conv_register_converter(
			"Bi-phase formatter", &conv_biphase_ops,
			conv, &platform_bus_type, conv->source_bus_id,
			conv->channel_from, conv->channel_to, NULL);
	if (!conv->converter) {
		dev_err(conv->dev, "Can't attach to player!");
		result = -ENODEV;
		goto error_attach;
	}

	/* Create ALSA low-level device */
	result = snd_device_new(card, SNDRV_DEV_LOWLEVEL, conv,
			&conv_biphase_snd_device_ops);
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

static int conv_biphase_remove(struct platform_device *pdev)
{
	struct conv_biphase *conv = platform_get_drvdata(pdev);

	BUG_ON(!conv);
	BUG_ON(!snd_stm_magic_valid(conv));

	snd_stm_conv_unregister_converter(conv->converter);

	regmap_field_free(conv->enable);

	if (conv->idle)
		regmap_field_free(conv->idle);

	snd_stm_magic_clear(conv);

	return 0;
}


/*
 * Module initialization.
 */

static struct of_device_id conv_biphase_match[] = {
	{
		.compatible = "st,snd_conv_biphase_stih416",
		.data = conv_biphase_sysconf_regfields_stih416
	},
	{
		.compatible = "st,snd_conv_biphase_stih407",
		.data = conv_biphase_sysconf_regfields_stih407
	},
	{},
};

MODULE_DEVICE_TABLE(of, conv_biphase_match);

static struct platform_driver conv_biphase_platform_driver = {
	.driver.name	= "snd_conv_biphase",
	.driver.of_match_table = conv_biphase_match,
	.probe		= conv_biphase_probe,
	.remove		= conv_biphase_remove,
};

module_platform_driver(conv_biphase_platform_driver);

MODULE_AUTHOR("John Boddie <john.boddie@st.com>");
MODULE_DESCRIPTION("STMicroelectronics bi-phase converter driver");
MODULE_LICENSE("GPL");
