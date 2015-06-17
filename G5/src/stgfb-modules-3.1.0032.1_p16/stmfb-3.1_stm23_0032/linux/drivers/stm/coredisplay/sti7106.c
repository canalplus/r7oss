/***********************************************************************
 *
 * File: linux/drivers/stm/coredisplay/sti7106.c
 * Copyright (c) 2009 STMicroelectronics Limited.
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
#include <linux/stm/pio.h>
#include <linux/stm/sysconf.h>

#include <asm/io.h>
#include <asm/semaphore.h>
#include <asm/irq.h>

#include <stmdisplay.h>
#include <linux/stm/stmcoredisplay.h>

#include <soc/sti7106/sti7106reg.h>
#include <soc/sti7106/sti7106device.h>
#include <STMCommon/stmhdmiregs.h>

/*
 * Note: STi7105 and STi7106 are almost identical from a display point of
 * view, including the system infrastructure (IRQ,PIO,SysCfg) wrapped around it.
 * However it does use a new HDMI cell and that needs a different implementation
 * of InfroFrame management and they have swapped around the video plugs and
 * changed which GPD is the shared plane between the main and aux pipelines.
 */
static const unsigned long whitelist[] = {
    STi7111_REGISTER_BASE + STi7111_DENC_BASE,
    STi7111_REGISTER_BASE + STi7111_DENC_BASE+PAGE_SIZE,
    STi7111_REGISTER_BASE + STi7111_DENC_BASE+(PAGE_SIZE*2),
    STi7111_REGISTER_BASE + STi7111_HDMI_BASE,
    _ALIGN_DOWN(STi7111_REGISTER_BASE + STi7111_BLITTER_BASE, PAGE_SIZE),
};



static struct stmcore_display_pipeline_data platform_data[] = {
  {
    .owner                    = THIS_MODULE,
    .name                     = "STi7106-main",
    .device                   = 0,
    .vtg_irq                  = evt2irq(0x1540),
    .blitter_irq              = evt2irq(0x1220),
    .blitter_irq_kernel       = evt2irq(0x1220),
    .hdmi_irq                 = evt2irq(0x15C0),
#if defined(CONFIG_SH_ST_MB680)
    .hdmi_i2c_adapter_id      = 2,
#else
    .hdmi_i2c_adapter_id      = 0,
#endif
    .main_output_id           = STi7111_OUTPUT_IDX_VDP0_MAIN,
    .hdmi_output_id           = STi7111_OUTPUT_IDX_VDP0_HDMI,
    .dvo_output_id            = STi7111_OUTPUT_IDX_DVO0,

    .blitter_id               = STi7111_BLITTER_IDX_VDP0_MAIN,
    .blitter_id_kernel        = STi7111_BLITTER_IDX_KERNEL,
    .blitter_type             = STMCORE_BLITTER_BDISPII,

    .preferred_graphics_plane = OUTPUT_GDP3,
    .preferred_video_plane    = OUTPUT_VID2,
    .planes                   = {
        { OUTPUT_GDP1, STMCORE_PLANE_GFX | STMCORE_PLANE_SHARED | STMCORE_PLANE_MEM_SYS },
        { OUTPUT_GDP2, STMCORE_PLANE_GFX | STMCORE_PLANE_MEM_SYS },
        { OUTPUT_GDP3, STMCORE_PLANE_GFX | STMCORE_PLANE_PREFERRED | STMCORE_PLANE_MEM_SYS },
        { OUTPUT_VID1, STMCORE_PLANE_VIDEO | STMCORE_PLANE_SHARED | STMCORE_PLANE_MEM_SYS },
        { OUTPUT_VID2, STMCORE_PLANE_VIDEO | STMCORE_PLANE_PREFERRED | STMCORE_PLANE_MEM_SYS },
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
    .name                     = "STi7106-aux",
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

    .preferred_graphics_plane = OUTPUT_GDP1,
    .preferred_video_plane    = OUTPUT_VID1,
    .planes                   = {
    	{ OUTPUT_GDP1, STMCORE_PLANE_GFX | STMCORE_PLANE_SHARED | STMCORE_PLANE_PREFERRED | STMCORE_PLANE_MEM_SYS },
    	{ OUTPUT_VID1, STMCORE_PLANE_VIDEO | STMCORE_PLANE_SHARED | STMCORE_PLANE_PREFERRED | STMCORE_PLANE_MEM_SYS }
    },
    .whitelist                = whitelist,
    .whitelist_size           = ARRAY_SIZE(whitelist),
    .io_offset                = 0,

    /* this is later mmap()ed, so it must be page aligned! */
    .mmio                     = _ALIGN_DOWN (STi7111_REGISTER_BASE + STi7111_BLITTER_BASE, PAGE_SIZE),
    .mmio_len                 = PAGE_SIZE,
  }
};


/*
 * The theoretical DAC voltage range is 1.398v:
 * 1024 * 0.0624 * (1.2214 / Rref) * Rload
 * 1023 * 0.0624 * (1.2214 / 7810) *   140
 *
 * But different board designs effect this.
 */
#if defined(CONFIG_SH_ST_MB680)
/*
 * 7106 is mostly pin compatible with 7105 so it can be used in an MB680
 */
static const int maxDAC123Voltage = 1360;
static const int maxDAC456Voltage = 1360;
/*
 * TODO: awaiting HDMI PHY pre-empasis setup for standard 1080p 50/60Hz modes
 * and both pre-emphasis and source termination for deepcolour modes with a
 * TMDS clock >165MHz.
 */
static stm_display_hdmi_phy_config_t hdmiphy_config[] = {
    { 148000000, 148250000, { 0, 0, 0, STM_HDMI_SRZ_CTRL_SRC_TERM(STM_HDMI_SRZ_CTRL_SRC_TERM_OFF) } },
    { 165000001, 225000000, { 0, 0, 0, STM_HDMI_SRZ_CTRL_SRC_TERM(STM_HDMI_SRZ_CTRL_SRC_TERM_65MV) } },
    { 0, 0 }
};

#elif defined(CONFIG_SH_ST_HDK7106)
/*
 * Note: Both HD and SD analogue video go through an STv6440 which needs to be
 * switched on via I2C separately from this driver. Also this board uses 7k87
 * as the reference resistor instead of 7k81.
 */
static const int maxDAC123Voltage = 1376;
static const int maxDAC456Voltage = 1376;

static stm_display_hdmi_phy_config_t hdmiphy_config[] = {
    { 148000000, 148250000, { 0, 0, 0, STM_HDMI_SRZ_CTRL_SRC_TERM(STM_HDMI_SRZ_CTRL_SRC_TERM_OFF) } },
    { 165000001, 225000000, { 0, 0, 0, STM_HDMI_SRZ_CTRL_SRC_TERM(STM_HDMI_SRZ_CTRL_SRC_TERM_65MV) } },
    { 0, 0 }
};
#else
static const int maxDAC123Voltage = 1398;
static const int maxDAC456Voltage = 1398;

static stm_display_hdmi_phy_config_t hdmiphy_config[] = {
    { 0, 0 }
};
#endif

static const int DAC123SaturationPoint; // Use Default (1023 for a 10bit DAC)
static const int DAC456SaturationPoint;

static struct stpio_pin     *hotplug_pio;
static struct sysconf_field *syscfg2_27;


int __init stmcore_probe_device(struct stmcore_display_pipeline_data **pd, int *nr_platform_devices)
{

  if(0/*boot_cpu_data.type == CPU_STX7106*/)
  {
    *pd = platform_data;
    *nr_platform_devices = N_ELEMENTS (platform_data);

    /*
     * Setup HDMI hotplug
     */
    hotplug_pio = stpio_request_pin(9,7,"HDMI Hotplug",STPIO_IN);
    if (hotplug_pio)
    {
      /* enable hotplug pio in syscfg */
      syscfg2_27 = sysconf_claim (SYS_CFG, 2, 27, 27, "HDMI Hotplug PIO enable");
      sysconf_write (syscfg2_27, 1);
      printk (KERN_INFO "stmcore-display: using HDMI hotplug\n");
    }
    else
      printk (KERN_INFO "stmcore-display: HDMI hotplug not available\n");

    printk(KERN_WARNING "stmcore-display: STi7106 display: probed\n");
    return 0;
  }

  printk(KERN_WARNING "stmcore-display: STi7106 display: platform unknown\n");

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

  if(p->hdmi_output)
    stm_display_output_set_control(p->hdmi_output, STM_CTRL_HDMI_PHY_CONF_TABLE, (ULONG)hdmiphy_config);

  return 0;
}


void stmcore_cleanup_device(void)
{
  if(hotplug_pio)
    stpio_free_pin(hotplug_pio);

  if (syscfg2_27)
    sysconf_release (syscfg2_27);
}
