/************************************************************************
Copyright (C) 2003-2014 STMicroelectronics. All Rights Reserved.

This file is part of the Streaming Engine.

Streaming Engine is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Streaming Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Streaming Engine; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
USA.

The Streaming Engine Library may alternatively be licensed under a
proprietary license from ST.
************************************************************************/

#include <linux/module.h>
#include <linux/platform_device.h>

#include "osinline.h"
#include "stm_se.h"
#include "mixer_transformer.h"

mixerTransformerDesc_t mixerTransformerArray[] = {
	{ .mixerId = 0, .transformerName = AUDIOMIXER_MT_NAME"_a0"},
	{ .mixerId = 1, .transformerName = AUDIOMIXER_MT_NAME"_a0"},
	{ .mixerId = 2, .transformerName = AUDIOMIXER_MT_NAME"_a0"},
	{ .mixerId = 3, .transformerName = AUDIOMIXER_MT_NAME"_a0"},
};

MODULE_PARM_DESC(mixer0Transformer, "Name of MME Transformer to use for Mixer0");
module_param_string(mixer0Transformer, mixerTransformerArray[0].transformerName,
                    MME_MAX_TRANSFORMER_NAME, 0);

MODULE_PARM_DESC(mixer1Transformer, "Name of MME Transformer to use for Mixer1");
module_param_string(mixer1Transformer, mixerTransformerArray[1].transformerName,
                    MME_MAX_TRANSFORMER_NAME, 0);

MODULE_PARM_DESC(mixer2Transformer, "Name of MME Transformer to use for Mixer2");
module_param_string(mixer2Transformer, mixerTransformerArray[2].transformerName,
                    MME_MAX_TRANSFORMER_NAME, 0);

MODULE_PARM_DESC(mixer3Transformer, "Name of MME Transformer to use for Mixer3");
module_param_string(mixer3Transformer, mixerTransformerArray[3].transformerName,
                    MME_MAX_TRANSFORMER_NAME, 0);

// sysfs statistics are considered as debug info:
// not compiled if CONFIG_DEBUG_FS not set..
#if defined(CONFIG_DEBUG_FS) || defined(SDK2_ENABLE_ATTRIBUTES)
#define SE_ENABLE_ATTRIBUTES
#endif

#ifdef SE_ENABLE_ATTRIBUTES

/* Mixer selection sysfs store callback */
static ssize_t store_mixerSelection(struct device *dev, struct device_attribute *attr,
                                    const char *buf, size_t count)
{
	struct dev_ext_attribute *ext_attr;
	mixerTransformerDesc_t *mixerDesc;
	char *str;
	char *charPos;

	ext_attr = container_of(attr, struct dev_ext_attribute, attr);
	mixerDesc = (mixerTransformerDesc_t *) ext_attr->var;

	if (count < 1) {
		return 0;
	}

	str = (char *) OS_Malloc(count);
	if (str == NULL) {
		pr_err("Error: %s Failed to alloc %d bytes\n", __func__, count);
		return count;
	}

	strncpy(str, buf, count);
	str[count - 1] = '\0';

	// By default, the string passed through sysfs includes a '\n'
	// Replace this '\n' by '\0'
	charPos = strrchr(str, '\n');
	if (charPos != NULL) {
		*charPos = '\0';
	}

	if (__stm_se_audio_mixer_update_transformer_id(mixerDesc->mixerId, str) == -ENODEV) {
		// Requested Mixer ID has not been created (yet)
		// Store the Requested Mixer transformerName
		strncpy(mixerDesc->transformerName, str,
		        min(strlen(str) + 1, sizeof(mixerDesc->transformerName)));
		mixerDesc->transformerName[sizeof(mixerDesc->transformerName) - 1] = '\0';
	}

	OS_Free(str);
	// Always return full write size even if we didn't consume all
	return count;
}

/* Mixer selection sysfs show callback */
static ssize_t show_mixerSelection(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct dev_ext_attribute *ext_attr;
	mixerTransformerDesc_t *mixerDesc;

	ext_attr = container_of(attr, struct dev_ext_attribute, attr);
	mixerDesc = (mixerTransformerDesc_t *) ext_attr->var;

	return snprintf(buf, PAGE_SIZE, "%s\n", mixerDesc->transformerName);
}

static struct dev_ext_attribute dev_attr_mixer0Transformer = {
	__ATTR(mixer0Transformer, S_IWUSR | S_IRUGO, show_mixerSelection, store_mixerSelection),
	&mixerTransformerArray[0]
};

static struct dev_ext_attribute dev_attr_mixer1Transformer = {
	__ATTR(mixer1Transformer, S_IWUSR | S_IRUGO, show_mixerSelection, store_mixerSelection),
	&mixerTransformerArray[1]
};

static struct dev_ext_attribute dev_attr_mixer2Transformer = {
	__ATTR(mixer2Transformer, S_IWUSR | S_IRUGO, show_mixerSelection, store_mixerSelection),
	&mixerTransformerArray[2]
};

static struct dev_ext_attribute dev_attr_mixer3Transformer = {
	__ATTR(mixer3Transformer, S_IWUSR | S_IRUGO, show_mixerSelection, store_mixerSelection),
	&mixerTransformerArray[3]
};

static struct device_attribute *mixerSelection_attrs[] = {
	&dev_attr_mixer0Transformer.attr,
	&dev_attr_mixer1Transformer.attr,
	&dev_attr_mixer2Transformer.attr,
	&dev_attr_mixer3Transformer.attr,
};

void create_sysfs_mixer_selection(struct device *dev)
{
	int i, error;

	for (i = 0; i < ARRAY_SIZE(mixerSelection_attrs); i++) {
		error = device_create_file(dev, mixerSelection_attrs[i]);
		if (error) {
			pr_err("Error: %s failed device create for %d\n", __func__, i);
			return;
		}
	}
}

void remove_sysfs_mixer_selection(struct device *dev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mixerSelection_attrs); i++) {
		device_remove_file(dev, mixerSelection_attrs[i]);
	}
}

#else

void create_sysfs_mixer_selection(struct device *dev)
{
}

void remove_sysfs_mixer_selection(struct device *dev)
{
}

#endif  // SE_ENABLE_ATTRIBUTES

