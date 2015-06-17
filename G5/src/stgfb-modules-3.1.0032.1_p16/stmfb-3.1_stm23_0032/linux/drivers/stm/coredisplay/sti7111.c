/***********************************************************************
 *
 * File: linux/drivers/stm/coredisplay/sti7111.c
 * Copyright (c) 2008,2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

/*
 * Note that the STi7111 is only supported on 2.6.23 and above kernels
 */
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fb.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/stm/pio.h>
#include <linux/stm/sysconf.h>

#include <asm/io.h>
#include <asm/semaphore.h>
#include <asm/irq.h>

#include <stmdisplay.h>
#include <linux/stm/stmcoredisplay.h>

#include "soc/sti7111/sti7111reg.h"
#include "soc/sti7111/sti7111device.h"

#if defined(CONFIG_SH_ST_MB618)
#define HAS_DSUB 1
#else
#define HAS_DSUB 0
#endif


static const unsigned long whitelist[] = {
    STi7111_REGISTER_BASE + STi7111_DENC_BASE,
    STi7111_REGISTER_BASE + STi7111_DENC_BASE+PAGE_SIZE,
    STi7111_REGISTER_BASE + STi7111_DENC_BASE+(PAGE_SIZE*2),
    STi7111_REGISTER_BASE + STi7111_HDMI_BASE,
    _ALIGN_DOWN(STi7111_REGISTER_BASE + STi7111_BLITTER_BASE, PAGE_SIZE),
};


/* BDisp IRQs on 7111: all aq sharing 0x1580, cq1 ???, cq2 ??? */
static struct stmcore_display_pipeline_data platform_data[] = {
  {
    .owner                    = THIS_MODULE,
    .name                     = "STi7111-main",
    .device                   = 0,
    .vtg_irq                  = evt2irq(0x1540),
    .blitter_irq              = evt2irq(0x1580),
    .blitter_irq_kernel       = evt2irq(0x1580),
    .hdmi_irq                 = evt2irq(0x15C0),
    .hdmi_i2c_adapter_id      = 0,
    .main_output_id           = STi7111_OUTPUT_IDX_VDP0_MAIN,
    .hdmi_output_id           = STi7111_OUTPUT_IDX_VDP0_HDMI,
    .dvo_output_id            = STi7111_OUTPUT_IDX_DVO0,

    .blitter_id               = STi7111_BLITTER_IDX_VDP0_MAIN,
    .blitter_id_kernel        = STi7111_BLITTER_IDX_KERNEL,
    .blitter_type             = STMCORE_BLITTER_BDISPII,

    .preferred_graphics_plane = OUTPUT_GDP1,
    .preferred_video_plane    = OUTPUT_VID1,
    .planes                   = {
    	{ OUTPUT_GDP1, STMCORE_PLANE_GFX | STMCORE_PLANE_PREFERRED | STMCORE_PLANE_MEM_SYS},
    	{ OUTPUT_GDP2, STMCORE_PLANE_GFX | STMCORE_PLANE_MEM_SYS},
    	{ OUTPUT_GDP3, STMCORE_PLANE_GFX | STMCORE_PLANE_SHARED | STMCORE_PLANE_MEM_SYS},
    	{ OUTPUT_VID1, STMCORE_PLANE_VIDEO | STMCORE_PLANE_PREFERRED | STMCORE_PLANE_MEM_SYS},
    	{ OUTPUT_VID2, STMCORE_PLANE_VIDEO | STMCORE_PLANE_SHARED | STMCORE_PLANE_MEM_SYS},
    	{ OUTPUT_CUR , STMCORE_PLANE_GFX | STMCORE_PLANE_MEM_SYS}
    },
    .whitelist                = whitelist,
    .whitelist_size           = ARRAY_SIZE(whitelist),
    .io_offset                = 0,

    /* this is later mmap()ed, so it must be page aligned! */
    .mmio                     = _ALIGN_DOWN (STi7111_REGISTER_BASE + STi7111_BLITTER_BASE, PAGE_SIZE),
    .mmio_len                 = PAGE_SIZE,
  },
  {
    .owner                    = THIS_MODULE,
    .name                     = "STi7111-aux",
    .device                   = 0,
    .vtg_irq                  = evt2irq(0x1560),
    .blitter_irq              = evt2irq(0x1580),
    .blitter_irq_kernel       = -1, /* only one instance of kernel blitter, handled above */
    .hdmi_irq                 = -1,
    .hdmi_i2c_adapter_id      = -1,
    .main_output_id           = STi7111_OUTPUT_IDX_VDP1_MAIN,
    .hdmi_output_id           = -1,
    .dvo_output_id            = -1,

    .blitter_id               = STi7111_BLITTER_IDX_VDP1_MAIN,
    .blitter_id_kernel        = STi7111_BLITTER_IDX_KERNEL,
    .blitter_type             = STMCORE_BLITTER_BDISPII,

    .preferred_graphics_plane = OUTPUT_GDP3,
    .preferred_video_plane    = OUTPUT_VID2,
    .planes                   = {
    	{ OUTPUT_GDP3, STMCORE_PLANE_GFX | STMCORE_PLANE_SHARED | STMCORE_PLANE_PREFERRED | STMCORE_PLANE_MEM_SYS},
    	{ OUTPUT_VID2, STMCORE_PLANE_VIDEO | STMCORE_PLANE_SHARED | STMCORE_PLANE_PREFERRED | STMCORE_PLANE_MEM_SYS}
    },
    .whitelist                = whitelist,
    .whitelist_size           = ARRAY_SIZE(whitelist),
    .io_offset                = 0,

    /* this is later mmap()ed, so it must be page aligned! */
    .mmio                     = _ALIGN_DOWN (STi7111_REGISTER_BASE + STi7111_BLITTER_BASE, PAGE_SIZE),
    .mmio_len                 = PAGE_SIZE,
  }
};


// 140 / 7720.0 * 77.31
static const int maxDAC123Voltage = 1360;
static const int maxDAC456Voltage = 1360;

static const int DAC123SaturationPoint = 0; // Use Default (1023 for a 10bit DAC)
static const int DAC456SaturationPoint = 0;

static struct stpio_pin *hotplug_pio = 0;
static struct stpio_pin *vsync_pio;
static struct stpio_pin *hsync_pio;


static struct sysconf_field *syscfg2_27 = NULL;
static struct sysconf_field *syscfg3_10 = NULL;
static struct sysconf_field *syscfg7_10_11 = NULL;


int __init stmcore_probe_device(struct stmcore_display_pipeline_data **pd, int *nr_platform_devices)
{

  if(boot_cpu_data.type == CPU_STX7111)
  {
    *pd = platform_data;
    *nr_platform_devices = N_ELEMENTS (platform_data);

    /*
     * Setup HDMI hotplug
     */
    hotplug_pio = stpio_request_pin(4,7,"HDMI Hotplug",STPIO_IN);
    /*
     * Enable hotplug pio in syscfg
     */
    syscfg2_27 = sysconf_claim(SYS_CFG,2,27,27,"HDMI Hotplug PIO Enable");
    sysconf_write(syscfg2_27,1);

    /*
     * Enable DVO->DVP loopback. The 7111 does not have external pads for
     * either, they are only there for loopback case and this bit should
     * always be set.
     */
    syscfg3_10 = sysconf_claim(SYS_CFG,3,10,10,"DVO->DVP Loopback");
    sysconf_write(syscfg3_10,1);

    if(HAS_DSUB != 0)
    {
      /*
       * Setup DSub connector H & V sync outputs on MB618.
       * NOTE: These signals use pio ports shared with SSC1. SSC1 I2S/SPI
       *       functionality will need to be disabled in the kernel board
       *       support before you can use this.
       *
       *       On MB618B/C J55 & J56 must be CLOSED to get the
       *       signals routed to the DSub connector. They are located between
       *       the big PIO jumper banks J15 and J25.
       */
      vsync_pio = stpio_request_pin(3,1,"DSub VSync",STPIO_ALT_OUT);
      hsync_pio = stpio_request_pin(3,2,"DSub HSync",STPIO_ALT_OUT);

      if(vsync_pio && hsync_pio)
      {
        unsigned long val;
        syscfg7_10_11 = sysconf_claim(SYS_CFG,7,10,11,"External H/V Sync Control");
        sysconf_write(syscfg7_10_11,1);
      }
      else
      {
        printk(KERN_WARNING "stmcore-display: External H&V sync signals already in use, probably as an I2C bus\n");
        printk(KERN_WARNING "stmcore-display: DSub connector output will not be functional.\n");
      }
    }

    printk(KERN_WARNING "stmcore-display: STi7111 display probed\n");
    return 0;
  }

  printk(KERN_WARNING "stmcore-display: STi7111 display not found\n");

  return -ENODEV;
}


int __init stmcore_display_postinit(struct stmcore_display *p)
{
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

  return 0;
}


void stmcore_cleanup_device(void)
{
  if(hotplug_pio)
    stpio_free_pin(hotplug_pio);

  if(vsync_pio)
    stpio_free_pin(vsync_pio);

  if(hsync_pio)
    stpio_free_pin(hsync_pio);

  if (syscfg2_27)
    sysconf_release (syscfg2_27);

  if (syscfg3_10)
    sysconf_release (syscfg3_10);

  if (syscfg7_10_11)
    sysconf_release (syscfg7_10_11);

}
