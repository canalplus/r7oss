/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/pixel_capture/stiH416/stiH416.c
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <linux/platform_device.h>
#include <linux/stm/platform.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/stm/stih416.h>

#ifdef CONFIG_SUPERH
#include <asm/irq-ilc.h>
#endif

#ifdef CONFIG_ARM
#include <asm/mach-types.h>
#endif

#include <device_features.h>
#include <debug.h>


static void release(struct device *dev) { }

static struct platform_device capture_h416[] = {
  [0] = {
    .name = "stm-capture",
    .id = 0,
    .num_resources = 1, /* type of resources= IO, MEM, IRQ, DMA */
    .resource = (struct resource[])
    {
      STM_PLAT_RESOURCE_MEM_NAMED("capture-io",  0xfd340e00, 0x1000),
    },
    .dev.platform_data = &(struct _stm_capture_hw_features) {
      .name                     = "COMPO-CAPTURE_0",
      .type                     = STM_PIXEL_CAPTURE_COMPO,
      .hw_features              = {
        .chroma_hsrc            = 0,
      },
    },
    .dev.release = release,
  },
/*
  [1] = {
    .name = "stm-capture",
    .id = 1,
    .dev.platform_data = &(struct _stm_capture_hw_features) {
      .owner                    = THIS_MODULE,
      .name                     = "FVDP-CAPTURE_0",
      .id                       = 0,
      .type                     = STM_PIXEL_CAPTURE_FVDP,
      .hw_features              = {
        .chroma_hsrc            = 0,
      },
    },
    .dev.release = release,
  },
*/
};

static struct platform_device *h416_devices[] __initdata = {
  &capture_h416[0],
/*  &capture_h416[1],*/
};

static int __init stm_cap_stih416_init(void)
{
  int ret;

  stm_cap_printi("%s\n", __func__);

  ret = platform_add_devices(h416_devices,
           ARRAY_SIZE(h416_devices));
  if (ret)
    return ret;

  return 0;
}

static void stm_cap_stih416_exit(void)
{
  unsigned int i;

  stm_cap_printi("%s\n", __func__);

  for (i = 0; i < ARRAY_SIZE(capture_h416); ++i)
    platform_device_unregister(&capture_h416[i]);
}

int __init stm_capture_probe_devices(void)
{
  return stm_cap_stih416_init();
}

void stm_capture_cleanup_devices(void)
{
  stm_cap_stih416_exit();
}
