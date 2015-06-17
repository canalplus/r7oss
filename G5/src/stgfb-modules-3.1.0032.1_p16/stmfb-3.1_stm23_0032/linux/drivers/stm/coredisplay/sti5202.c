/***********************************************************************
 *
 * File: linux/drivers/stm/coredisplay/sti5202.c
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

static const unsigned long whitelist[] = {
    0x19002000, /* HD display */
    0x19003000, /* SD display */
    0x19004000, /* LMU */
    0x19005000,	/* VOS */
    0x1920c000, /* DENC */
};


#if defined(CONFIG_SH_ST_MB602)
#define PROBED 1
#define HDMI_I2C_BUS_ID 1
#else
#define PROBED 0
#define HDMI_I2C_BUS_ID 0
#endif


/*
 * The STi5202 only has a single LMI and is limited to SD/ED display modes
 */
static struct stmcore_display_pipeline_data platform_data[] = {
  {
    .owner                    = THIS_MODULE,
    .name                     = "STi5202-main",
    .device                   = 0,
    .vtg_irq                  = 154,
    .blitter_irq              = 156,
    .blitter_irq_kernel       = -1, /* handled by main blitter */
    .hdmi_irq                 = 158, 
    .hdmi_i2c_adapter_id      = HDMI_I2C_BUS_ID,
    .main_output_id           = 0,
    .hdmi_output_id           = 3,
    .dvo_output_id            = 2,

    .blitter_id               = 0,
    .blitter_id_kernel        = -1, /* just use main blitter */
    .blitter_type             = STMCORE_BLITTER_BDISPII,

    .preferred_graphics_plane = OUTPUT_GDP1,
    .preferred_video_plane    = OUTPUT_VID1,
    .planes                   = {
        { OUTPUT_GDP1, STMCORE_PLANE_GFX   | STMCORE_PLANE_PREFERRED | STMCORE_PLANE_MEM_SYS},
        { OUTPUT_GDP2, STMCORE_PLANE_GFX   | STMCORE_PLANE_MEM_SYS},
        { OUTPUT_GDP3, STMCORE_PLANE_GFX   | STMCORE_PLANE_SHARED    | STMCORE_PLANE_MEM_SYS},
        { OUTPUT_VID1, STMCORE_PLANE_VIDEO | STMCORE_PLANE_PREFERRED | STMCORE_PLANE_MEM_SYS},
        { OUTPUT_VID2, STMCORE_PLANE_VIDEO | STMCORE_PLANE_SHARED    | STMCORE_PLANE_MEM_SYS},
        { OUTPUT_CUR , STMCORE_PLANE_GFX   | STMCORE_PLANE_MEM_SYS}
    },
    .whitelist                = whitelist,
    .whitelist_size           = ARRAY_SIZE(whitelist),
    .io_offset                = 0
  },
  {
    .owner                    = THIS_MODULE,
    .name                     = "STi5202-aux",
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
        { OUTPUT_VID2, STMCORE_PLANE_VIDEO | STMCORE_PLANE_SHARED | STMCORE_PLANE_PREFERRED | STMCORE_PLANE_MEM_SYS}
    },
    .whitelist                = whitelist,
    .whitelist_size           = ARRAY_SIZE(whitelist),
    .io_offset                = 0
  }
};


static const int maxDAC123Voltage = 1402;   // Rref=7.72Kohm Rload=140ohm, Vmax=(77.312/7720)*140
static const int maxDAC456Voltage = 1402;

static const int DAC123SaturationPoint = 0; // Use Default (1023 for a 10bit DAC)
static const int DAC456SaturationPoint = 0;

static struct stpio_pin *hotplug_pio = 0;

static const int refClockError = 0;

int __init stmcore_probe_device(struct stmcore_display_pipeline_data **pd, int *nr_platform_devices)
{

  if(PROBED != 0)
  {
    *pd = platform_data;
    *nr_platform_devices = N_ELEMENTS (platform_data);

    hotplug_pio = stpio_request_pin(2,2,"HDMI Hotplug",STPIO_BIDIR_Z1);

    /*
     * Use the following for boards where hdmi hotplug is connected to the
     * hdmi block interrupt, via PIO4(7) alternate input function.
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

  printk(KERN_WARNING "stmcore-display: STi5202 display not found\n");
  
  return -ENODEV;
}


int __init stmcore_display_postinit(struct stmcore_display *p)
{

  stm_display_output_setup_clock_reference(p->main_output, STM_CLOCK_REF_30MHZ, refClockError);
  stm_display_output_set_control(p->main_output, STM_CTRL_MAX_PIXEL_CLOCK, 27027000);

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
   * hdmi block interrupt, via PIO4(7) alternate input function.
   *
   * if(p->hdmi_data)
   *   stm_display_output_set_control(p->main_output, STM_CTRL_HDMI_USE_HOTPLUG_INTERRUPT, 1);
   */

  return 0;
}


void stmcore_cleanup_device(void)
{
  if(hotplug_pio)
    stpio_free_pin(hotplug_pio);
}
