/***********************************************************************
 *
 * File: linux/drivers/video/stmfbsysfs.c
 * Copyright (c) 2006 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <linux/version.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/fb.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>

#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/semaphore.h>

#include <stmdisplay.h>

#include "stmfb.h"
#include "stmfbinfo.h"

static ssize_t __show_cea861(const stm_mode_line_t **display_modes, int num_modes,
                                  stm_wss_t aspect, char *buf)
{
	unsigned long long modes;
	unsigned int i, sz;

	modes = 0;
	for (i=0; i<num_modes; i++) {
		const stm_mode_line_t *mode = display_modes[i];

		if (STM_WSS_16_9 != aspect)
			modes |= 1ull << mode->ModeParams.HDMIVideoCodes[AR_INDEX_4_3];
		if (STM_WSS_4_3 != aspect)
			modes |= 1ull << mode->ModeParams.HDMIVideoCodes[AR_INDEX_16_9];
	}

	modes >>= 1;
	sz = 0;
	for (i=1; modes; i++, modes>>=1) {
		if (modes & 1) {
			sz += snprintf(&buf[sz], PAGE_SIZE - sz, "%d\n", i);
		}
	}

	return sz;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
static ssize_t show_cea861(struct class_device *class_device, char *buf)
#else
static ssize_t show_cea861(struct device           *device,
			   struct device_attribute *attr,
			   char                    *buf)
#endif
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
	struct stmfb_info *info = class_get_devdata(class_device);
#else
	struct stmfb_info *info = dev_get_drvdata(device);
#endif

	const stm_mode_line_t *display_modes[STVTG_TIMING_MODE_COUNT];
	const stm_mode_line_t *mode;
	int num_modes;
	int i;

	num_modes = 0;
	for (i=0; i<STVTG_TIMING_MODE_COUNT; i++) {
		if (!(mode = stm_display_output_get_display_mode(info->pFBMainOutput, i)))
			continue;

		display_modes[num_modes++] = mode;
	}

	return __show_cea861(display_modes, num_modes, STM_WSS_OFF, buf);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
static struct class_device_attribute stmfb_device_attrs[] = {
#else
static struct device_attribute stmfb_device_attrs[] = {
#endif
	/*__ATTR(bits_per_pixel, S_IRUGO|S_IWUSR, show_bpp, store_bpp), (R/W EXAMPLE) */
	__ATTR(_ST_cea861, S_IRUGO, show_cea861, NULL),
};


void
stmfb_init_class_device (struct stmfb_info * const fb_info)
{
	unsigned int i, ret;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
	for (i = 0; i < ARRAY_SIZE(stmfb_device_attrs); i++) {
		ret = class_device_create_file(fb_info->info.class_device,
					       &stmfb_device_attrs[i]);
#else
	for (i = 0; i < ARRAY_SIZE(stmfb_device_attrs); i++) {
		ret = device_create_file(fb_info->info.dev,
					 &stmfb_device_attrs[i]);
#endif
		if (ret)
			printk(KERN_ERR
			       "stmfb: failed registering device attributes\n");
	}

}

void __exit
stmfb_cleanup_class_device (struct stmfb_info * const fb_info)
{
	unsigned int i;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
	for (i = 0; i < ARRAY_SIZE(stmfb_device_attrs); i++)
		class_device_remove_file(fb_info->info.class_device,
					 &stmfb_device_attrs[i]);
#else
	for (i = 0; i < ARRAY_SIZE(stmfb_device_attrs); i++)
		device_remove_file(fb_info->info.dev,
				   &stmfb_device_attrs[i]);
#endif

}
