/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/pixel_capture/stiH407/stiH407.c
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
#include <linux/stm/stih407.h>

#ifdef CONFIG_SUPERH
#include <asm/irq-ilc.h>
#endif

#ifdef CONFIG_ARM
#include <asm/mach-types.h>
#endif

#include <device_features.h>
#include <debug.h>


static void release(struct device *dev) { }

static struct platform_device capture_H407[] = {
  [0] = {
    .name = "stm-capture",
    .id = 0,
    .num_resources = 1, /* type of resources= IO, MEM, IRQ, DMA */
    .resource = (struct resource[])
    {
      STM_PLAT_RESOURCE_MEM_NAMED("capture-io",  0x9d11E00, 0x1000),
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

  [1] = {
    .name = "stm-capture",
    .id = 0,
    .num_resources = 2, /* type of resources= IO, MEM, IRQ, DMA */
    .resource = (struct resource[])
    {
      STM_PLAT_RESOURCE_MEM_NAMED("capture-io",  0x9d12000, 0x1000),
      STIH407_RESOURCE_IRQ_NAMED("capture-int", 67),
    },
    .dev.platform_data = &(struct _stm_capture_hw_features) {
      .name                     = "DVP-CAPTURE_0",
      .type                     = STM_PIXEL_CAPTURE_DVP,
      .hw_features              = {
        .chroma_hsrc            = 0,
      },
    },
    .dev.release = release,
  },
};

static struct platform_device *stiH407_devices[] __initdata = {
  &capture_H407[0],
  &capture_H407[1],
};

static int __init stm_cap_stiH407_init(void)
{
  int ret;

  stm_cap_printi("%s\n", __func__);

  ret = platform_add_devices(stiH407_devices,
           ARRAY_SIZE(stiH407_devices));
  if (ret)
    return ret;

  /* setup clk for DVP ? */

  return 0;
}

static void stm_cap_stiH407_exit(void)
{
  unsigned int i;

  stm_cap_printi("%s\n", __func__);

  for (i = 0; i < ARRAY_SIZE(capture_H407); ++i)
    platform_device_unregister(&capture_H407[i]);
}

int __init stm_capture_probe_devices(void)
{
  return stm_cap_stiH407_init();
}

void stm_capture_cleanup_devices(void)
{
  stm_cap_stiH407_exit();
}
