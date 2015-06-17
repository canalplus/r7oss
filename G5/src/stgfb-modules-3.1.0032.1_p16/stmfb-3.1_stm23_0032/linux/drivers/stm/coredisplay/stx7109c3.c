/***********************************************************************
 *
 * File: linux/drivers/stm/coredisplay/stx7109c2.c
 * Copyright (c) 2007 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fb.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,23)
#  include <linux/stpio.h>
#else
#  include <linux/stm/pio.h>
#endif


#include <asm/io.h>
#include <asm/semaphore.h>

#include <stmdisplay.h>
#include <linux/stm/stmcoredisplay.h>

#include "soc/stb7100/stb7100reg.h"

static const unsigned long whitelist[] = {
    0x19002000, /* HD display */
    0x19003000, /* SD display */
    0x19004000, /* LMU */
    0x19005000,	/* VOS */
    0x1920c000, /* DENC */
    _ALIGN_DOWN(STb7100_REGISTER_BASE + STb7109_BDISP_BASE, PAGE_SIZE)
};


static struct stmcore_display_pipeline_data platform_data[] = {
  {
    .owner                    = THIS_MODULE,
    .name                     = "STx7109c3-main",
    .device                   = 0,
    .vtg_irq                  = 154,
    .blitter_irq              = 156,
    .blitter_irq_kernel       = -1, /* handled by main blitter */
    .hdmi_irq                 = 158, 
#if defined(CONFIG_SH_STB7100_REF) || defined(CONFIG_SH_ST_MB442) || defined(CONFIG_SH_ST_HMP7100)
    .hdmi_i2c_adapter_id      = 1,
#elif defined(CONFIG_SH_STB7109E_REF) || defined(CONFIG_SH_ST_MB448)
    .hdmi_i2c_adapter_id      = 0,
#elif defined(CONFIG_SH_STB7100_MBOARD) || defined(CONFIG_SH_ST_MB411)
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,23)
    .hdmi_i2c_adapter_id      = 2,
#else
    .hdmi_i2c_adapter_id      = 1,
#endif
#elif defined(CONFIG_SH_HMS1) || defined(CONFIG_SH_ST_HMS1)
    .hdmi_i2c_adapter_id      = 2,
#else
    .hdmi_i2c_adapter_id      = HDMI_I2C_ADAPTER_ID, /* Add your board definition here */
#endif
    .main_output_id           = 0,
    .hdmi_output_id           = 3,
    .dvo_output_id            = 2,

    .blitter_id               = 0,
    .blitter_id_kernel        = -1, /* just use main blitter */
    .blitter_type             = STMCORE_BLITTER_BDISPII,

    .preferred_graphics_plane = OUTPUT_GDP1,
    .preferred_video_plane    = OUTPUT_VID1,
    .planes                   = {
    	{ OUTPUT_GDP1, STMCORE_PLANE_GFX | STMCORE_PLANE_PREFERRED | STMCORE_PLANE_MEM_SYS},
    	{ OUTPUT_GDP2, STMCORE_PLANE_GFX | STMCORE_PLANE_MEM_SYS},
    	{ OUTPUT_GDP3, STMCORE_PLANE_GFX | STMCORE_PLANE_SHARED | STMCORE_PLANE_MEM_SYS},
    	{ OUTPUT_VID1, STMCORE_PLANE_VIDEO | STMCORE_PLANE_PREFERRED | STMCORE_PLANE_MEM_VIDEO},
    	{ OUTPUT_VID2, STMCORE_PLANE_VIDEO | STMCORE_PLANE_SHARED | STMCORE_PLANE_MEM_VIDEO},
    	{ OUTPUT_CUR , STMCORE_PLANE_GFX | STMCORE_PLANE_MEM_SYS}
    },
    .whitelist                = whitelist,
    .whitelist_size           = ARRAY_SIZE(whitelist),
    .io_offset                = 0,

    /* this is later mmap()ed, so it must be page aligned! */
    .mmio                     = _ALIGN_DOWN (STb7100_REGISTER_BASE + STb7109_BDISP_BASE, PAGE_SIZE),
    .mmio_len                 = PAGE_SIZE,
  },
  {
    .owner                    = THIS_MODULE,
    .name                     = "STx7109c3-aux",
    .device                   = 0,
    .vtg_irq                  = 155,
    .blitter_irq              = -1, /* Handled by the main pipeline instance */
    .blitter_irq_kernel       = -1, /* Handled by the main pipeline instance */
    .hdmi_irq                 = -1, 
    .hdmi_i2c_adapter_id      = -1,
    .main_output_id           = 1,
    .hdmi_output_id           = -1,
    .dvo_output_id            = -1,

    .blitter_id               = 0,
    .blitter_id_kernel        = -1, /* just use main blitter */
    .blitter_type             = STMCORE_BLITTER_BDISPII,

    .preferred_graphics_plane = OUTPUT_GDP3,
    .preferred_video_plane    = OUTPUT_VID2,
    .planes                   = {
    	{ OUTPUT_GDP3, STMCORE_PLANE_GFX | STMCORE_PLANE_SHARED | STMCORE_PLANE_PREFERRED | STMCORE_PLANE_MEM_SYS},
    	/* Note on STx7109c3 VID2 requires memory from LMI_VID not LMI_SYS */
    	{ OUTPUT_VID2, STMCORE_PLANE_VIDEO | STMCORE_PLANE_SHARED | STMCORE_PLANE_PREFERRED | STMCORE_PLANE_MEM_VIDEO}
    },
    .whitelist                = whitelist,
    .whitelist_size           = ARRAY_SIZE(whitelist),
    .io_offset                = 0,

    /* this is later mmap()ed, so it must be page aligned! */
    .mmio                     = _ALIGN_DOWN (STb7100_REGISTER_BASE + STb7109_BDISP_BASE, PAGE_SIZE),
    .mmio_len                 = PAGE_SIZE,
  }
};

#if defined(CONFIG_SH_STB7100_MBOARD) || defined(CONFIG_SH_ST_MB411)
static const int refClockError = 359;
#else
static const int refClockError = 0;
#endif


/*
 * For ST boards, we assume the video DAC load resistor network is the same,
 * customers will need to tune this for their board designs.
 */
static const int maxDAC123Voltage = 1409;   // Rref=7.68Kohm Rload=140ohm, Vmax=(77.312/7680)*140
static const int maxDAC456Voltage = 1409;

static const int DAC123SaturationPoint = 0; // Use Default (1023 for a 10bit DAC)
static const int DAC456SaturationPoint = 0;

static struct stpio_pin *hotplug_pio = 0;

#if defined(CONFIG_SH_STB7100_MBOARD) || defined(CONFIG_SH_STB7100_REF) || \
    defined(CONFIG_SH_STB7109E_REF)   || defined(CONFIG_SH_ST_MB411)    || \
    defined(CONFIG_SH_ST_MB442)       || defined(CONFIG_SH_ST_MB448)    || \
    defined(CONFIG_SH_HMS1)           || defined(CONFIG_SH_ST_HMS1)     || \
    defined(CONFIG_SH_ST_HMP7100)

#define SYSCONF_DEVICEID 0x19001000
#else
#define SYSCONF_DEVICEID 0x19001000
#endif


int __init stmcore_probe_device(struct stmcore_display_pipeline_data **pd, int *nr_platform_devices)
{

  if(SYSCONF_DEVICEID != 0)
  {
    unsigned long *devid = ioremap(SYSCONF_DEVICEID, sizeof(unsigned long));
    unsigned long chipid = readl(devid);

    int is7109      = (((chipid>>12)&0x3ff) == 0x02c);
    int chipVersion = (chipid>>28)+1;
    iounmap(devid);

    if(is7109 && chipVersion == 3)
    {
      *pd = platform_data;
      *nr_platform_devices = N_ELEMENTS (platform_data);

      /* default should be 2,2,"HDMI Hotplug",STPIO_BIDIR_Z1 */
      hotplug_pio = stpio_request_pin(HDMI_HOTPLUG_PIO_PORTNO, HDMI_HOTPLUG_PIO_PINNO, HDMI_HOTPLUG_PIO_NAME, HDMI_HOTPLUG_PIO_DIRECTION);

      /*
       * Use the following for boards where hdmi hotplug is connected to the
       * hdmi block interrupt, via PIO4(7) alternate input function on
       * 7109C3.
       *
       * hotplug_pio = stpio_request_pin(4,7,"HDMI Hotplug",STPIO_ALT_BIDIR);
       */

      if(!hotplug_pio)
      {
        printk(KERN_WARNING "stmcore-display: Hotplug PIO already in use (by SSC driver?)\n");
        printk(KERN_WARNING "stmcore-display: HDMI will not work in this board configuration\n");
      }

      return 0;
    }
  }

  printk(KERN_WARNING "stmcore-display: STx7109C3 display not found\n");

  return -ENODEV;
}


/*
 * The following FIR filter setups for the DENC luma filter have been derived
 * for the MB411; FIR2C is the default already used by the core driver. The
 * others give a variety of different frequency responses.
 *
 * We do not have specific filters for MB442 or HMS1; however, they both use the
 * same video output stage configuration as the MB411, so it is expected that
 * they will give very similar frequency responses.

static stm_display_filter_setup_t luma_filter_FIR2C = {
  DENC_COEFF_LUMA,
  { .denc = { STM_FLT_DIV_1024,
              {0x03, 0x01, 0xfffffff7, 0xfffffffe, 0x1E, 0x05, 0xffffffa4, 0xfffffff9, 0x144, 0x206}
            }
  }
};

static stm_display_filter_setup_t luma_filter_FIR2D = {
  DENC_COEFF_LUMA,
  { .denc = { STM_FLT_DIV_1024,
              {0x0f, 0xffffffe0, 0xffffffde, 0x3C, 0x2E, 0xffffffc5, 0xffffff95, 0x03F, 0x150, 0x1C0}
            }
  }
};

static stm_display_filter_setup_t luma_filter_FIR2E = {
  DENC_COEFF_LUMA,
  { .denc = { STM_FLT_DIV_1024,
              {0xfffffffd, 0xfffffffc, 0xfffffff9, 0xffffffff, 0x23, 0x05, 0xffffffa0, 0xfffffff1, 0x147, 0x21E}
            }
  }
};

static stm_display_filter_setup_t luma_filter_FIR2F = {
  DENC_COEFF_LUMA,
  { .denc = { STM_FLT_DIV_1024,
              {0x03, 0x00, 0xfffffff7, 0xffffffff, 0x1f, 0x03, 0xffffffa6, 0xfffffff4, 0x141, 0x214}
            }
  }
};
*/


int __init stmcore_display_postinit(struct stmcore_display *p)
{

  stm_display_output_setup_clock_reference(p->main_output, STM_CLOCK_REF_30MHZ, refClockError);

  /*
   * Setup internal configuration controls
   */
  if(maxDAC123Voltage != 0)
    stm_display_output_set_control(p->main_output, STM_CTRL_DAC123_MAX_VOLTAGE, maxDAC123Voltage);

  if(maxDAC456Voltage != 0)
    stm_display_output_set_control(p->main_output, STM_CTRL_DAC456_MAX_VOLTAGE, maxDAC456Voltage);

  if(DAC123SaturationPoint != 0)
    stm_display_output_set_control(p->main_output, STM_CTRL_DAC123_SATURATION_POINT, DAC123SaturationPoint);

  if(DAC456SaturationPoint != 0)
    stm_display_output_set_control(p->main_output, STM_CTRL_DAC456_SATURATION_POINT, DAC456SaturationPoint);

  /*
   * To poll a pio for hotplug changes in the primary vsync handler set the
   * following.
   */
  p->hotplug_poll_pio = hotplug_pio;

  /*
   * Use this instead for boards where hdmi hotplug is connected to the
   * hdmi block interrupt, via PIO4(7) alternate input function on
   * 7109C3.
   *
   * if(p->hdmi_data)
   *   stm_display_output_set_control(p->main_output, STM_CTRL_HDMI_USE_HOTPLUG_INTERRUPT, 1);
   */


  /*
   * Uncomment this if you want to change the default DENC luma filter

    stm_display_output_set_filter_coefficients(p->main_output, &luma_filter_FIR2C);
   */

  return 0;
}


void stmcore_cleanup_device(void)
{
  if(hotplug_pio)
    stpio_free_pin(hotplug_pio);
}
