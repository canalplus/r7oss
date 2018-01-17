/*
 *  plt_id.c - Driver that read Technicolor STB platform ID from GPIO
 *
 *  Author : Cyril TOSTIVINT <cyril.tostivint@technicolor.com>
 *
 *  Copyright (C) 2011-2016 Technicolor
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Device-tree:
 *    plt_id {
 *        compatible = "tch_plt_id";
 *        plt_name = "USX8001CNL";
 *        plt_name_alt = "USW4001NCP"; // Only when SW should support multiple platform
 *                                     // if set, the highest GPIO is for platform distinction
 *        gpio = <0x64 0x63 0x61 0x5f>;
 *    };
 */


#include <linux/of.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>

#include "plt_id.h"

char default_plt_name[] = "UNDEFINED";

char *hw_suffix[NUM_HW_SUFFIX] = {
	"LAB1",
	"LAB1a",
	"LAB2",
	"LAB2a",
	"LAB3",
	"LAB3a",
	"INDEX0",
	"INDEX1",
	"INDEX2",
	"INDEX3",
	"SPARE1",
	"SPARE2",
	"SPARE3",
	"SPARE4",
	"SPARE5",
	"SPARE6"
};

char *hw_suffix_alt[NUM_HW_SUFFIX/2] = {
	"LAB1",
	"LAB2",
	"INDEX0",
	"INDEX1",
	"SPARE1",
	"SPARE2",
	"SPARE3",
	"SPARE4"
};

static inline char *tch_plt_get_id_suffix(u8 id, int alt)
{
	if (alt) {
        if ((id & 0x07) >= NUM_HW_SUFFIX/2)
            return NULL;
		return hw_suffix_alt[id & 0x07]; // Highest GPIO for platform distinction
    }

	if (id >= NUM_HW_SUFFIX)
		return NULL;

    return hw_suffix[id];
}

// Read callback for /sys/plt/id
static ssize_t id_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	struct tch_plt_id_desc *desc = container_of(kobj,
		   struct tch_plt_id_desc, kobj);

	return sprintf(buf, "%d", desc->id);
}

// Read callback for /sys/plt/name
static ssize_t name_show(struct kobject *kobj,
		struct kobj_attribute *attr,
		char *buf)
{
	struct tch_plt_id_desc *desc = container_of(kobj,
		   struct tch_plt_id_desc, kobj);

	return sprintf(buf, "%s_%s\n", desc->name,
			tch_plt_get_id_suffix(desc->id, desc->multi_plt));
}

static u8 tch_plt_read_id(struct device *dev, u32 *gpios)
{
	int i;
	u8 id = 0;

	for (i=0; i < 4; i++) {
		if (gpio_request_one(gpios[i], GPIOF_IN, "plt_id") != 0) {
			dev_err(dev, "Failed to request gpio %d", gpios[i]);
			return 0xFF;
		}
		id |= gpio_get_value(gpios[i]) << i;
		gpio_free(gpios[i]);
	}

	return id;
}

/*---------------- SysFS structures ----------------*/
static struct kobj_attribute id_attr = __ATTR_RO(id);
static struct kobj_attribute name_attr = __ATTR_RO(name);

static struct attribute *plt_id_attrs[] = {
	&id_attr.attr,
	&name_attr.attr,
	NULL
};

static struct kobj_type plt_id_ktype = {
	.sysfs_ops = &kobj_sysfs_ops,
	.default_attrs = plt_id_attrs,
};


static int tch_plt_id_probe(struct platform_device *pdev)
{
	struct tch_plt_id_desc *desc;
	u32 gpio_list[4];
	int len, ret;

	desc = devm_kzalloc(&pdev->dev, sizeof(*desc), GFP_KERNEL);
	desc->name = default_plt_name;

	ret = of_property_read_u32_array(pdev->dev.of_node,
					"gpio",
					gpio_list,
					ARRAY_SIZE(gpio_list));
	if (ret) {
		dev_err(&pdev->dev, "gpio node isn't defined");
		return -EINVAL;
	}

	desc->id = tch_plt_read_id(&pdev->dev, gpio_list);

	if (of_find_property(pdev->dev.of_node, "plt_name_alt", &len))
		desc->multi_plt = 1;
	else
		desc->multi_plt = 0;

	if ( desc->multi_plt && (desc->id & 0x08) == 0x08)
		of_property_read_string(pdev->dev.of_node, "plt_name_alt", &desc->name);
	else
		of_property_read_string(pdev->dev.of_node, "plt_name", &desc->name);

	platform_set_drvdata(pdev, desc);

	printk("Platform (GPIOs detection): %s_%s\n", desc->name,
		   	tch_plt_get_id_suffix(desc->id, desc->multi_plt));

	ret = kobject_init_and_add(&desc->kobj,
			&plt_id_ktype, NULL, "plt");
	if (ret)
		dev_err(&pdev->dev, "Unable to create sysfs plt: %d", ret);
	kobject_uevent(&desc->kobj, KOBJ_ADD);

	plt_permdata_init(&pdev->dev, desc);

	return 0;
}

static const struct of_device_id tch_plt_id_of_match[] = {
	{ .compatible = "tch_plt_id" },
	{},
};

static struct platform_driver tch_plt_id_driver = {
	.driver = {
		.name	= "tch_plt_id",
		.of_match_table = tch_plt_id_of_match,
	},
	.probe = tch_plt_id_probe,
};

static int __init tch_plt_id_init(void)
{
	return platform_driver_register(&tch_plt_id_driver);
}
device_initcall(tch_plt_id_init);


static void __exit tch_plt_id_exit(void)
{
		platform_driver_unregister(&tch_plt_id_driver);
}
module_exit(tch_plt_id_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Driver for Technicolor Hardware platform id");
MODULE_AUTHOR("Cyril Tostivint <cyril.tostivint@technicolor.com");
