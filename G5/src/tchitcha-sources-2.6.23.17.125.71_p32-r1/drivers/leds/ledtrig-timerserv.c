/*
 * LED Kernel Timerserv Trigger
 *
 * Copyright 2005-2006 Openedhand Ltd.
 *
 * Author: Richard Purdie <rpurdie@openedhand.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/device.h>
#include <linux/sysdev.h>
#include <linux/timer.h>
#include <linux/ctype.h>
#include <linux/leds.h>
#include "leds.h"

struct timerserv_trig_data {
	unsigned long delay_on;		/* milliseconds on */
	unsigned long delay_off;	/* milliseconds off */
	unsigned int state;		/* LED_FLASH_X */
	int brightness_still;		/* the level requested externally */
	int brightness_blink;		/* the flash level */
	struct timer_list timer;
};

enum led_flash {
	LED_FLASH_OFF,
	LED_FLASH_ON,
};

struct serv_tag {
	char *name;
	int val;
};

static struct serv_tag ledflashstate[] = {
	{ "off",	LED_FLASH_OFF },
	{ "on",		LED_FLASH_ON },
	{ NULL,		0 }
};

static int serv_str2int(const char *state, struct serv_tag *array)
{
	struct serv_tag *pt = NULL;
	size_t count;

	if (state == NULL)
		return -1;
	count = strlen(state);
	while (count > 0) {
		if (isspace(state[count - 1]))
			count--;
		else
			break;
	}

	for (pt = array; pt->name != NULL; pt++) {
		if ( strlen(pt->name) == count &&
		     !strncmp(state, pt->name, count) )
			return pt->val;
	}
	return -1;
}

static char *serv_int2str(int val, struct serv_tag *array)
{
	struct serv_tag *pt = NULL;

	for (pt = array; pt->name != NULL; pt++) {
		if (pt->val == val)
			return pt->name;
	}
	return NULL;
}

static void led_timerserv_function(unsigned long data)
{
	struct led_classdev *led_cdev = (struct led_classdev *) data;
	struct timerserv_trig_data *timerserv_data = led_cdev->trigger_data;
	int brightness = led_cdev->brightness;
	unsigned long delay;

	if (timerserv_data->state != LED_FLASH_ON)
		return;

	if (!timerserv_data->delay_on || !timerserv_data->delay_off) {
		timerserv_data->state = LED_FLASH_OFF;
		led_set_brightness(led_cdev, timerserv_data->brightness_still);
		return;
	}

	if (brightness == LED_OFF) {
		brightness = timerserv_data->brightness_blink;
		delay = timerserv_data->delay_on;
	}
	else {
		if (brightness != timerserv_data->brightness_blink) {
			/* brightness parameter has been modified,
			 * as per an external request */
			timerserv_data->brightness_still = brightness;
			timerserv_data->brightness_blink = brightness;
		}
		brightness = LED_OFF;
		delay = timerserv_data->delay_off;
	}

	led_set_brightness(led_cdev, brightness);

	mod_timer(&timerserv_data->timer, jiffies + msecs_to_jiffies(delay));
}

static ssize_t led_delay_on_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct timerserv_trig_data *timerserv_data = led_cdev->trigger_data;

	sprintf(buf, "%lu\n", timerserv_data->delay_on);

	return strlen(buf) + 1;
}

static ssize_t led_delay_on_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct timerserv_trig_data *timerserv_data = led_cdev->trigger_data;
	int ret = -EINVAL;
	char *after;
	unsigned long state = simple_strtoul(buf, &after, 10);
	size_t count = after - buf;

	if (*after && isspace(*after))
		count++;

	if (count == size) {
		timerserv_data->delay_on = state;
		ret = count;
	}

	return ret;
}

static ssize_t led_delay_off_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct timerserv_trig_data *timerserv_data = led_cdev->trigger_data;

	sprintf(buf, "%lu\n", timerserv_data->delay_off);

	return strlen(buf) + 1;
}

static ssize_t led_delay_off_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct timerserv_trig_data *timerserv_data = led_cdev->trigger_data;
	int ret = -EINVAL;
	char *after;
	unsigned long state = simple_strtoul(buf, &after, 10);
	size_t count = after - buf;

	if (*after && isspace(*after))
		count++;

	if (count == size) {
		timerserv_data->delay_off = state;
		ret = count;
	}

	return ret;
}

static ssize_t flash_show(struct device *dev,
			  struct device_attribute *attr,
			  char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct timerserv_trig_data *timerserv_data = led_cdev->trigger_data;

	sprintf(buf, "%s\n", serv_int2str(timerserv_data->state,
					ledflashstate));

	return strlen(buf) + 1;
}

static ssize_t flash_store(struct device *dev,
			   struct device_attribute *attr,
			   const char *buf,
			   size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct timerserv_trig_data *timerserv_data = led_cdev->trigger_data;
	const char *name = NULL;
	int brightness = led_cdev->brightness;
	int ret;
	unsigned int state;

	state = serv_str2int(buf, ledflashstate);
	if (state < 0)
		return -EINVAL;
	name = serv_int2str(state, ledflashstate);
	if (name == NULL)
		return -ENOSYS;
	ret = strlen(name);
	if (state == timerserv_data->state)
		return ret;

	switch (state) {
	case LED_FLASH_OFF:
		timerserv_data->state = state;
		del_timer_sync(&timerserv_data->timer);
		led_set_brightness(led_cdev, timerserv_data->brightness_still);
		break;
	case LED_FLASH_ON:
		timerserv_data->brightness_still = brightness;
		timerserv_data->brightness_blink =
			brightness == LED_OFF ? LED_FULL : brightness;
		timerserv_data->state = state;
		mod_timer(&timerserv_data->timer, jiffies + 1);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static DEVICE_ATTR(delay_on, 0644, led_delay_on_show, led_delay_on_store);
static DEVICE_ATTR(delay_off, 0644, led_delay_off_show, led_delay_off_store);
static DEVICE_ATTR(flash_mode, 0644, flash_show, flash_store);

static void timerserv_trig_activate(struct led_classdev *led_cdev)
{
	struct timerserv_trig_data *timerserv_data;
	int rc;

	timerserv_data = kzalloc(sizeof(struct timerserv_trig_data), GFP_KERNEL);
	if (!timerserv_data)
		return;

	led_cdev->trigger_data = timerserv_data;

	init_timer(&timerserv_data->timer);
	timerserv_data->timer.function = led_timerserv_function;
	timerserv_data->timer.data = (unsigned long) led_cdev;

	rc = device_create_file(led_cdev->dev, &dev_attr_delay_on);
	if (rc)
		goto err_out;
	rc = device_create_file(led_cdev->dev, &dev_attr_delay_off);
	if (rc)
		goto err_out_delayon;
	rc = device_create_file(led_cdev->dev, &dev_attr_flash_mode);
	if (rc)
		goto err_out_delayoff;

	return;

err_out_delayoff:
	device_remove_file(led_cdev->dev, &dev_attr_delay_off);
err_out_delayon:
	device_remove_file(led_cdev->dev, &dev_attr_delay_on);
err_out:
	led_cdev->trigger_data = NULL;
	kfree(timerserv_data);
}

static void timerserv_trig_deactivate(struct led_classdev *led_cdev)
{
	struct timerserv_trig_data *timerserv_data = led_cdev->trigger_data;

	if (timerserv_data) {
		device_remove_file(led_cdev->dev, &dev_attr_delay_on);
		device_remove_file(led_cdev->dev, &dev_attr_delay_off);
		device_remove_file(led_cdev->dev, &dev_attr_flash_mode);
		del_timer_sync(&timerserv_data->timer);
		kfree(timerserv_data);
	}
}

static struct led_trigger timerserv_led_trigger = {
	.name     = "timerserv",
	.activate = timerserv_trig_activate,
	.deactivate = timerserv_trig_deactivate,
};

static int __init timerserv_trig_init(void)
{
	return led_trigger_register(&timerserv_led_trigger);
}

static void __exit timerserv_trig_exit(void)
{
	led_trigger_unregister(&timerserv_led_trigger);
}

module_init(timerserv_trig_init);
module_exit(timerserv_trig_exit);

MODULE_AUTHOR("Richard Purdie <rpurdie@openedhand.com>");
MODULE_DESCRIPTION("Timer serv LED trigger");
MODULE_LICENSE("GPL");
