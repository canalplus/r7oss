/*
 *   STMicrolectronics audio glue driver
 *
 *   Copyright (c) 2013 STMicroelectronics Limited
 *
 *   Authors: John Boddie <john.boddie@st.com>
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

#include <linux/of.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>
#include "common.h"

static struct of_device_id audio_glue_match[];

/* Regfield IDs */
enum {
	EXT_CLOCK,
	VOIP,
	EXT_DAC_OUTPUT,
	/* keep last */
	MAX_REGFIELDS
};

/* stm device power IDs */
enum {
	STM_DEVICE_POWER_OFF,
	STM_DEVICE_POWER_ON
};

/*
 * Driver structure.
 */

struct audio_glue_sysconf_regfields {
	bool avail;
	struct reg_field regfield;
	int power_off_value;
	int power_on_value;
};

struct audio_glue {
	/* sysconf registers bank map */
	struct regmap *sysconf_regmap;
	/* sysconf register bit fields */
	const struct audio_glue_sysconf_regfields *sysconf_regfields;
	/* Control bits */
	struct regmap_field *sysconf_regmap_field[MAX_REGFIELDS];
};

/*
 * sysconf registers configuration of given platform
 */

static struct audio_glue_sysconf_regfields
			audio_glue_sysconf_regfields_stih416[MAX_REGFIELDS] = {
	[EXT_CLOCK] = {true, REG_FIELD(STIH416_AUDIO_GLUE_CONFIG, 8, 11), 0, 15},
	[VOIP] = {true, REG_FIELD(STIH416_AUDIO_GLUE_CONFIG, 2, 5), 0, 0},
	[EXT_DAC_OUTPUT] = {true, REG_FIELD(STIH416_AUDIO_GLUE_CONFIG, 0, 1), 0, 1},
};

static struct audio_glue_sysconf_regfields
			audio_glue_sysconf_regfields_stih407[MAX_REGFIELDS] = {
	[EXT_CLOCK] = {true, REG_FIELD(STIH407_AUDIO_GLUE_CONFIG, 8, 11), 0, 15},
	[VOIP] = {true, REG_FIELD(STIH407_AUDIO_GLUE_CONFIG, 2, 5), 0, 0},
	[EXT_DAC_OUTPUT] = {true, REG_FIELD(STIH407_AUDIO_GLUE_CONFIG, 0, 1), 0, 1},
};

/*
 * Driver functions.
 */

static int audio_glue_parse_dt(struct platform_device *pdev,
					struct audio_glue *glue)
{
	struct device_node *pnode = pdev->dev.of_node;

	glue->sysconf_regmap =
		syscon_regmap_lookup_by_phandle(pnode, "st,syscfg");
	if (!glue->sysconf_regmap) {
		dev_err(&pdev->dev, "No syscfg phandle specified\n");
		return -EINVAL;
	}

	glue->sysconf_regfields =
		of_match_node(audio_glue_match, pnode)->data;
	if (!glue->sysconf_regfields) {
		dev_err(&pdev->dev, "compatible data not provided\n");
		return -EINVAL;
	}

	return 0;
}

static void audio_glue_sysconf_claim(struct audio_glue *glue)
{
	int i;

	for (i = 0; i < MAX_REGFIELDS; i++) {
		if (glue->sysconf_regfields[i].avail == true)
			glue->sysconf_regmap_field[i] =
				regmap_field_alloc(glue->sysconf_regmap,
					glue->sysconf_regfields[i].regfield);
	}
}

static void device_power(struct audio_glue *glue, int power)
{
	int i;

	for (i = 0; i < MAX_REGFIELDS; i++) {
		if (glue->sysconf_regmap_field[i]) {
			if (power == STM_DEVICE_POWER_ON)
				regmap_field_write(glue->sysconf_regmap_field[i],
					glue->sysconf_regfields[i].power_on_value);
			else
				regmap_field_write(glue->sysconf_regmap_field[i],
					glue->sysconf_regfields[i].power_off_value);
		}
	}
}

static int audio_glue_probe(struct platform_device *pdev)
{
	struct audio_glue *glue;
	int result;

	dev_dbg(&pdev->dev, "%s()", __func__);

	/* Allocate driver structure */
	glue = devm_kzalloc(&pdev->dev, sizeof(*glue), GFP_KERNEL);
	if (!glue) {
		dev_err(&pdev->dev, "Failed to allocate driver structure");
		return -ENOMEM;
	}

	/* Get the device configuration */
	if (!pdev->dev.of_node) {
		dev_err(&pdev->dev, "device not in dt");
		return -EINVAL;
	}

	if (audio_glue_parse_dt(pdev, glue)) {
		dev_err(&pdev->dev, "invalid dt");
		return -EINVAL;
	}

	/* claim sysconf */
	audio_glue_sysconf_claim(glue);

	/* Issue the device power on sequence */
	device_power(glue, STM_DEVICE_POWER_ON);

	/* Register the audio sound card */
	result = snd_stm_card_register(SND_STM_CARD_TYPE_AUDIO);
	if (result) {
		dev_err(&pdev->dev, "Failed to register ALSA audio card!");
		return result;
	}

	/* Save the dev config as driver data */
	dev_set_drvdata(&pdev->dev, glue);

	return 0;
}

static int audio_glue_remove(struct platform_device *pdev)
{
	struct audio_glue *glue = dev_get_drvdata(&pdev->dev);

	dev_dbg(&pdev->dev, "%s()", __func__);

	/* Issue the device power off sequence */
	device_power(glue, STM_DEVICE_POWER_OFF);

	return 0;
}

/*
 * Power management.
 */

#ifdef CONFIG_PM
static int audio_glue_suspend(struct device *dev)
{
	struct audio_glue *glue = dev_get_drvdata(dev);

	dev_dbg(dev, "%s()", __func__);

	/* Issue the device power off sequence */
	device_power(glue, STM_DEVICE_POWER_OFF);

	return 0;
}

static int audio_glue_resume(struct device *dev)
{
	struct audio_glue *glue = dev_get_drvdata(dev);

	dev_dbg(dev, "%s()", __func__);

	/* Issue the device power on sequence */
	device_power(glue, STM_DEVICE_POWER_ON);

	return 0;
}
#else
#define audio_glue_suspend NULL
#define audio_glue_resume NULL
#endif

static const struct dev_pm_ops audio_glue_pm_ops = {
	.suspend	= audio_glue_suspend,
	.resume		= audio_glue_resume,
	.freeze		= audio_glue_suspend,
	.thaw		= audio_glue_resume,
	.restore	= audio_glue_resume,
};

/*
 * Module initialistaion.
 */

static struct of_device_id audio_glue_match[] = {
	{
		.compatible = "st,snd_audio_glue_stih416",
		.data = audio_glue_sysconf_regfields_stih416
	},
	{
		.compatible = "st,snd_audio_glue_stih407",
		.data = audio_glue_sysconf_regfields_stih407
	},
	{},
};

MODULE_DEVICE_TABLE(of, audio_glue);

static struct platform_driver audio_glue_platform_driver = {
	.driver.name	= "snd_audio_glue",
	.driver.of_match_table = audio_glue_match,
	.driver.pm	= &audio_glue_pm_ops,
	.probe		= audio_glue_probe,
	.remove		= audio_glue_remove,
};

module_platform_driver(audio_glue_platform_driver);

MODULE_AUTHOR("John Boddie <john.boddie@st.com>");
MODULE_DESCRIPTION("STMicroelectronics audio glue driver");
MODULE_LICENSE("GPL");
