/***********************************************************************
 *
 * File: linux/kernel/drivers/video/stmfbsysfs.c
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
#include <linux/semaphore.h>

#include <stm_display.h>

#include "stmfb.h"
#include "stmfbinfo.h"

static ssize_t __show_cea861(const stm_display_mode_t *display_modes, int num_modes,
                             stm_wss_aspect_ratio_t aspect, char *buf)
{
        unsigned long long modes;
        unsigned int i, sz;

        modes = 0;
        for (i=0; i<num_modes; i++) {
                const stm_display_mode_t *mode = &display_modes[i];

                if (STM_WSS_ASPECT_RATIO_16_9 != aspect)
                        modes |= 1ull << mode->mode_params.hdmi_vic_codes[STM_AR_INDEX_4_3];
                if (STM_WSS_ASPECT_RATIO_4_3 != aspect)
                        modes |= 1ull << mode->mode_params.hdmi_vic_codes[STM_AR_INDEX_16_9];
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

static ssize_t show_cea861(struct device           *device,
                           struct device_attribute *attr,
                           char                    *buf)
{
        struct stmfb_info *info = dev_get_drvdata(device);

        stm_display_mode_t *display_modes;
        ssize_t sz;
        int num_modes;
        int i;

        display_modes = kmalloc(sizeof(stm_display_mode_t)*STM_TIMING_MODE_COUNT, GFP_KERNEL);
        if(!display_modes)
          return 0;

        num_modes = 0;
        for (i=0; i<STM_TIMING_MODE_COUNT; i++) {
                if (stm_display_output_get_display_mode(info->hFBMainOutput, i, &display_modes[num_modes])<0)
                        continue;

                num_modes++;
        }

        sz = __show_cea861(display_modes, num_modes, STM_WSS_ASPECT_RATIO_UNKNOWN, buf);

        kfree(display_modes);

        return sz;
}

static struct device_attribute stmfb_device_attrs[] = {
        /*__ATTR(bits_per_pixel, S_IRUGO|S_IWUSR, show_bpp, store_bpp), (R/W EXAMPLE) */
        __ATTR(_ST_cea861, S_IRUGO, show_cea861, NULL),
};


void
stmfb_init_class_device (struct stmfb_info * const fb_info)
{
        unsigned int i, ret;

        for (i = 0; i < ARRAY_SIZE(stmfb_device_attrs); i++) {
                ret = device_create_file(fb_info->info.dev,
                                         &stmfb_device_attrs[i]);
                if (ret)
                        printk(KERN_ERR
                               "stmfb: failed registering device attributes\n");
        }

}

void __exit
stmfb_cleanup_class_device (struct stmfb_info * const fb_info)
{
        unsigned int i;

        for (i = 0; i < ARRAY_SIZE(stmfb_device_attrs); i++)
                device_remove_file(fb_info->info.dev,
                                   &stmfb_device_attrs[i]);
}
