/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/coredisplay/stiH407/stiH407.c
 * Copyright (c) 2011-2012 STMicroelectronics Limited.
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
#include <linux/of.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#ifndef CONFIG_ARCH_STI
#include <linux/stm/soc.h>
#include <linux/stm/pad.h>
#include <linux/stm/stih407.h>
#endif

#include <asm/mach-types.h>
#include <asm/system.h>

#include <stm_display.h>
#include <linux/stm/stmcoredisplay.h>
#include <linux/stm/hdmiplatform.h>
#include <linux/stm/linkplatform.h>

#include <display/soc/stiH407/stiH407reg.h>
#include <display/soc/stiH407/stiH407device.h>

extern void stmcore_display_dt_get_pdata(
        struct platform_device *pdev,
        struct stmcore_display_pipeline_data **pp,
        int *n);

static stm_display_filter_setup_t sd_luma_filter = {
  DENC_COEFF_LUMA,
  { .denc = { STM_FLT_DIV_1024,
              { -3, -4, -7, -1, 35, 5, -96, -15, 327, 542 }
            }
  }
};

static stm_display_filter_setup_t sd_chroma_filter = {
  DENC_COEFF_CHROMA,
  { .denc = { STM_FLT_DIV_512,
              {10, 6, -9, -25, -23, 13, 73, 134, 154 }
            }
  }
};

#ifdef CONFIG_ARCH_STI
static int stmcore_display_init_clocks(struct device *dev)
{
  return stmcore_display_set_clocks(dev);
}

#else
enum _clocks {
  /* VIDEO Clocks we are going to use internally */
  CLOCK_NONE = 0,
  /* Clockgen C_0 (main system clocks) */
  CLK_MAIN_DISP,
  CLK_AUX_DISP,
  CLK_COMPO_DVP,
  /* Clockgen D_2 (video display clocks) */
  CLK_TMDSOUT_HDMI,
  CLK_TMDS_HDMI,
};

struct coredisplay_clk {
  struct clk  *clk;
  const char  *name1;
  const char  *name2;
  bool         using_2nd;
  enum _clocks parent;
  u32          default_rate;
};

static struct coredisplay_clk coredisplay_clks[] = {
  /* Clockgen C_0 (main system clocks) */
  [CLK_MAIN_DISP]     = { .name1 = "CLK_MAIN_DISP",     .parent = CLOCK_NONE, .default_rate = 400000000 },
  [CLK_AUX_DISP]      = { .name1 = "CLK_AUX_DISP",      .parent = CLOCK_NONE, .default_rate = 400000000 },
  [CLK_COMPO_DVP]     = { .name1 = "CLK_COMPO_DVP",     .parent = CLOCK_NONE, .default_rate = 400000000 },
  /* Clockgen D_2 (video display clocks) */
  [CLK_TMDSOUT_HDMI]  = { .name1 = "CLK_TMDSOUT_HDMI",  .parent = CLOCK_NONE,       .default_rate = 0 },
  [CLK_TMDS_HDMI]     = { .name1 = "CLK_TMDS_HDMI",     .parent = CLK_TMDSOUT_HDMI, .default_rate = 0 },
};


#define CLK_SET_DEFAULT_RATE(CLK) { if(clk_set_rate(coredisplay_clks[CLK].clk,coredisplay_clks[CLK].default_rate)) printk(KERN_WARNING "Failed to set clock %s rate to %d\n",coredisplay_clks[CLK].using_2nd ? coredisplay_clks[CLK].name2 : coredisplay_clks[CLK].name1,coredisplay_clks[CLK].default_rate); }
#define CLK_SET_PARENT(CLK) { if(clk_set_parent(coredisplay_clks[CLK].clk,coredisplay_clks[coredisplay_clks[CLK].parent].clk)) printk(KERN_WARNING "Failed to set clock %s parent to %s\n",coredisplay_clks[CLK].using_2nd ? coredisplay_clks[CLK].name2 : coredisplay_clks[CLK].name1,coredisplay_clks[coredisplay_clks[CLK].parent].using_2nd ? coredisplay_clks[coredisplay_clks[CLK].parent].name2 : coredisplay_clks[coredisplay_clks[CLK].parent].name1); }

static int stmcore_display_init_clocks(struct device *dev)
{
  int i, err=0;

  /*
   * Note: Loop starting at 1 as clock 0 is the "NULL" clock.
   */
  for(i = 1; i < ARRAY_SIZE (coredisplay_clks); ++i)
  {
    coredisplay_clks[i].clk = devm_clk_get(dev, coredisplay_clks[i].name1);
    if(IS_ERR_OR_NULL(coredisplay_clks[i].clk))
    {
      if (!coredisplay_clks[i].name2)
        printk(KERN_WARNING "stmcore-display: Unable to get clock %s\n",coredisplay_clks[i].name1);
      else
      {
        printk(KERN_WARNING "stmcore-display: Unable to get clock %s, trying %s instead\n",coredisplay_clks[i].name1,coredisplay_clks[i].name2);
        coredisplay_clks[i].clk = devm_clk_get(dev, coredisplay_clks[i].name2);
        coredisplay_clks[i].using_2nd = true;
      }
      if(IS_ERR_OR_NULL(coredisplay_clks[i].clk))
      {
        printk(KERN_WARNING "stmcore-display: clock not found\n");
        err = -ENODEV;
        continue;
      }
    }

    if(coredisplay_clks[i].parent != CLOCK_NONE)
      CLK_SET_PARENT(i);

    clk_prepare_enable (coredisplay_clks[i].clk);

    if(coredisplay_clks[i].default_rate)
      CLK_SET_DEFAULT_RATE(i);

#if defined(DEBUG)
    printk(KERN_WARNING "stmcore-display: Enabling clock: %s\n",
           coredisplay_clks[i].using_2nd ? coredisplay_clks[i].name2 : coredisplay_clks[i].name1);
#endif
  }

  return err;
}
#endif

#ifndef CONFIG_ARCH_STI
int __init stmcore_probe_device(struct platform_device *pdev,
                                struct stmcore_display_pipeline_data **pd,
                                int *nr_platform_devices)
#else
int stmcore_probe_device(struct platform_device *pdev,
                                struct stmcore_display_pipeline_data **pd,
                                int *nr_platform_devices)
#endif
{
  int i;

  *pd = NULL;
  *nr_platform_devices = 0;
  stmcore_display_dt_get_pdata(pdev, pd, nr_platform_devices);
  if ( !*pd ) {
      printk(KERN_ERR "error while reading Device tree definition for display driver.\n");
      return -ENODEV;
  }

  if((i = stmcore_display_init_clocks(&pdev->dev)) < 0)
  {
    return i;
  }

  /*
   * Setup HDMI platform device
   */
#ifdef CONFIG_STM_VIRTUAL_PLATFORM /* FIXME - VSOC BUG - stmdisplaylink: stmlink_probe: HDMI i2c bus (0) not available, check your kernel configuration and board setup */
  printk(KERN_WARNING "VSOC - stmcore-display: WARNING ***********************\n");
  printk(KERN_WARNING " HDMI Hotplug is unavailable due to missing Ethernet TLM model\n");
  printk(KERN_WARNING "*******************************************************\n");
#endif

  printk(KERN_INFO "stmcore-display: STIH407 display: probed\n");

  return 0;
}


#ifndef CONFIG_ARCH_STI
int __init stmcore_display_postinit(struct stmcore_display_pipeline_data *p)
#else
int stmcore_display_postinit(struct stmcore_display_pipeline_data *p)
#endif
{
    stm_display_output_set_compound_control(p->display_runtime->main_output, OUTPUT_CTRL_DAC_FILTER_COEFFICIENTS, &sd_luma_filter);
    stm_display_output_set_compound_control(p->display_runtime->main_output, OUTPUT_CTRL_DAC_FILTER_COEFFICIENTS, &sd_chroma_filter);
    return 0;
}


void stmcore_cleanup_device(void)
{
#ifndef CONFIG_ARCH_STI
  int i;
  for(i = 0; i < ARRAY_SIZE (coredisplay_clks); ++i)
  {
    if(!IS_ERR_OR_NULL(coredisplay_clks[i].clk))
    {
      clk_disable_unprepare (coredisplay_clks[i].clk);
      clk_put (coredisplay_clks[i].clk);
    }
  }
#endif
}

#ifndef CONFIG_ARCH_STI
void stmcore_display_device_set_pm(struct device *dev, int event)
{
  int i;
  int enabled = ((event == PM_EVENT_RESUME) || (event == PM_EVENT_RESTORE));

  if(event == PM_EVENT_RESTORE)
  {
    stmcore_display_init_clocks(dev);
  }

  for(i = 0; i < ARRAY_SIZE (coredisplay_clks); ++i)
  {
    if(coredisplay_clks[i].clk)
    {
#if defined(DEBUG)
      printk(KERN_WARNING "stmcore-display: %s clock: %s",
             enabled ? "Enabling" : "Disabling", coredisplay_clks[i].using_2nd ? coredisplay_clks[i].name2 : coredisplay_clks[i].name1);
#endif
      if (enabled)
        clk_prepare_enable (coredisplay_clks[i].clk);
      else
        clk_disable_unprepare (coredisplay_clks[i].clk);

      if(event == PM_EVENT_FREEZE)
      {
        clk_put (coredisplay_clks[i].clk);
      }
    }
  }
}
#endif
