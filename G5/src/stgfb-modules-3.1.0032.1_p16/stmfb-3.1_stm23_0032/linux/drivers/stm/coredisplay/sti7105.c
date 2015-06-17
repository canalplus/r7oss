/***********************************************************************
 *
 * File: linux/drivers/stm/coredisplay/sti7105.c
 * Copyright (c) 2008,2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

/*
 * Note that the STi7105 is only supported on 2.6.23 and above kernels
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

#ifdef CONFIG_SH_DORADE
#include <asm/board_info.h>
#endif

#include <stmdisplay.h>
#include <linux/stm/stmcoredisplay.h>

#include <soc/sti7105/sti7105reg.h>
#include <soc/sti7105/sti7105device.h>
#include <soc/sti7111/sti7111reg.h>
#include <stmdisplayoutput.h>
#include <STMCommon/stmhdmiregs.h>

static const unsigned long whitelist[] = {
    STi7111_REGISTER_BASE + STi7111_DENC_BASE,
    STi7111_REGISTER_BASE + STi7111_DENC_BASE+PAGE_SIZE,
    STi7111_REGISTER_BASE + STi7111_DENC_BASE+(PAGE_SIZE*2),
    STi7111_REGISTER_BASE + STi7111_HDMI_BASE,
    _ALIGN_DOWN(STi7111_REGISTER_BASE + STi7111_BLITTER_BASE, PAGE_SIZE),
};


/* BDisp IRQs on 7105: all aq sharing 0x1220, cq1 ???, cq2 ??? */
static struct stmcore_display_pipeline_data platform_data[] = {
  {
    .owner                    = THIS_MODULE,
    .name                     = "STi7105-main",
    .device                   = 0,
    .vtg_irq                  = evt2irq(0x1540),
    .blitter_irq              = evt2irq(0x1220),
    .blitter_irq_kernel       = evt2irq(0x1220),
    .hdmi_irq                 = evt2irq(0x15C0),
#if defined(CONFIG_SH_ST_MB680)
    .hdmi_i2c_adapter_id      = 2,
#elif defined(CONFIG_SH_ST_PDK7105)
    .hdmi_i2c_adapter_id      = 0,
#elif defined(CONFIG_SH_ST_IPTV7105)
    .hdmi_i2c_adapter_id      = 1,
#else
    .hdmi_i2c_adapter_id      = HDMI_I2C_ADAPTER_ID,
#endif
    .main_output_id           = STi7111_OUTPUT_IDX_VDP0_MAIN,
    .hdmi_output_id           = STi7111_OUTPUT_IDX_VDP0_HDMI,
    .dvo_output_id            = STi7111_OUTPUT_IDX_DVO0,

    .blitter_id               = STi7111_BLITTER_IDX_VDP0_MAIN,
    .blitter_id_kernel        = STi7111_BLITTER_IDX_KERNEL,
    .blitter_type             = STMCORE_BLITTER_BDISPII,

    .preferred_graphics_plane = OUTPUT_GDP1,
    .preferred_video_plane    = OUTPUT_VID1,
    .planes                   = {
    	{ OUTPUT_GDP1, STMCORE_PLANE_GFX | STMCORE_PLANE_PREFERRED | STMCORE_PLANE_MEM_SYS },
    	{ OUTPUT_GDP2, STMCORE_PLANE_GFX | STMCORE_PLANE_MEM_SYS },
    	{ OUTPUT_GDP3, STMCORE_PLANE_GFX | STMCORE_PLANE_SHARED | STMCORE_PLANE_MEM_SYS },
    	{ OUTPUT_VID1, STMCORE_PLANE_VIDEO | STMCORE_PLANE_PREFERRED | STMCORE_PLANE_MEM_SYS },
    	{ OUTPUT_VID2, STMCORE_PLANE_VIDEO | STMCORE_PLANE_SHARED | STMCORE_PLANE_MEM_SYS },
    	{ OUTPUT_CUR , STMCORE_PLANE_GFX | STMCORE_PLANE_MEM_SYS }
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
    .name                     = "STi7105-aux",
    .device                   = 0,
    .vtg_irq                  = evt2irq(0x1560),
    .blitter_irq              = evt2irq(0x1220),
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
    	{ OUTPUT_GDP3, STMCORE_PLANE_GFX | STMCORE_PLANE_SHARED | STMCORE_PLANE_PREFERRED | STMCORE_PLANE_MEM_SYS },
    	{ OUTPUT_VID2, STMCORE_PLANE_VIDEO | STMCORE_PLANE_SHARED | STMCORE_PLANE_PREFERRED | STMCORE_PLANE_MEM_SYS }
    },
    .whitelist                = whitelist,
    .whitelist_size           = ARRAY_SIZE(whitelist),
    .io_offset                = 0,

    /* this is later mmap()ed, so it must be page aligned! */
    .mmio                     = _ALIGN_DOWN (STi7111_REGISTER_BASE + STi7111_BLITTER_BASE, PAGE_SIZE),
    .mmio_len                 = PAGE_SIZE,
  }
};


static int hdmi_148MHZ_pre_emphasis = HDMI_148MHZ_PRE_EMPHASIS, \
  hdmi_148MHZ_pre_emphasis_on = HDMI_148MHZ_PRE_EMPHASIS_ON;

stm_display_hdmi_phy_config_t default_phy_config[] = {
    { 0,        40000000,  {0, SYS_CFG2_HDMIPHY_BUFFER_SPEED_4 }},
    { 40000000, 72000000,  {0, SYS_CFG2_HDMIPHY_BUFFER_SPEED_8 }},
    { 72000000, 80000000,  {0, SYS_CFG2_HDMIPHY_BUFFER_SPEED_8 }},
    { 80000000, 150000000, {0, 0}},
    { 0, 0 }
};

/*
 * The theoretical DAC voltage range is 1.398v:
 * 1024 * 0.0624 * (1.2214 / Rref) * Rload
 * 1023 * 0.0624 * (1.2214 / 7810) *   140
 *
 * But different board designs effect this.
 */
#if defined(CONFIG_SH_ST_MB680)
static int maxDAC123Voltage = 1360;
static int maxDAC456Voltage = 1360;
#elif defined(CONFIG_SH_ST_PDK7105)
static int maxDAC123Voltage = 1320;
static int maxDAC456Voltage = 1320;
#else
static int maxDAC123Voltage = STMFB_MAX_DAC123_VOLTAGE;
static int maxDAC456Voltage = STMFB_MAX_DAC456_VOLTAGE;
#endif

static const int DAC123SaturationPoint; // Use Default (1023 for a 10bit DAC)
static const int DAC456SaturationPoint;

static struct stpio_pin     *hotplug_pio;
static struct sysconf_field *syscfg2_27;

int __init stmcore_probe_device(struct stmcore_display_pipeline_data **pd, int *nr_platform_devices)
{

  if(boot_cpu_data.type == CPU_STX7105)
  {
    *pd = platform_data;
    *nr_platform_devices = N_ELEMENTS (platform_data);

    /*
     * Set up pre emphasis values for DORADE boards.
     */
#if defined(CONFIG_SH_DORADE)
    /* For SAGEM */
    if (is_dorade_manufacturer_sagem()) {
      /* Pre-emphasis */
      hdmi_148MHZ_pre_emphasis = 1;
      hdmi_148MHZ_pre_emphasis_on = 1;

      /* DAC values */
      maxDAC123Voltage = 1486;
      maxDAC456Voltage = 1487;      
    }

    /* For CISCO */
    if (is_dorade_manufacturer_cisco()) {
      /* Pre-emphasis */
      hdmi_148MHZ_pre_emphasis = 0;
      hdmi_148MHZ_pre_emphasis_on = 1;
      
      /* 
       * DAC values.
       * These values are not good. We are waiting
       * for CISCO's register values.
       */
      maxDAC123Voltage = 1398;
      maxDAC456Voltage = 1398;
    }
#else
#endif

    /* Update default_phy_config table */
    default_phy_config[3].config[1] = (SYS_CFG2_HDMIPHY_BUFFER_SPEED_16 | \
				       (hdmi_148MHZ_pre_emphasis_on * SYS_CFG2_HDMIPHY_PREEMP_ON)       | \
				       (hdmi_148MHZ_pre_emphasis << SYS_CFG2_HDMIPHY_PREEMP_STR_SHIFT));

    /*
     * Setup HDMI hotplug
     */
    /* default should be 9,7,"HDMI Hotplug",STPIO_IN */
    hotplug_pio = stpio_request_pin(HDMI_HOTPLUG_PIO_PORTNO, HDMI_HOTPLUG_PIO_PINNO, HDMI_HOTPLUG_PIO_NAME, HDMI_HOTPLUG_PIO_DIRECTION);
    if (hotplug_pio)
    {
      /* enable hotplug pio in syscfg */
      syscfg2_27 = sysconf_claim (SYS_CFG, 2, 27, 27, "HDMI Hotplug PIO enable");
      sysconf_write (syscfg2_27, 1);
      printk (KERN_INFO "stmcore-display: using HDMI hotplug\n");
    }
    else
      printk (KERN_INFO "stmcore-display: HDMI hotplug not available\n");

    printk(KERN_WARNING "stmcore-display: STi7105 display: probed\n");
    return 0;
  }

  printk(KERN_WARNING "stmcore-display: STi7105 display: platform unknown\n");

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

#ifdef STMFB_CLOCK_REF_30MHZ_ERROR
  stm_display_output_setup_clock_reference(p->main_output, STM_CLOCK_REF_30MHZ, STMFB_CLOCK_REF_30MHZ_ERROR);
#endif

  return 0;
}


void stmcore_cleanup_device(void)
{
  if(hotplug_pio)
    stpio_free_pin(hotplug_pio);

  if (syscfg2_27)
    sysconf_release (syscfg2_27);
}


EXPORT_SYMBOL(default_phy_config);
