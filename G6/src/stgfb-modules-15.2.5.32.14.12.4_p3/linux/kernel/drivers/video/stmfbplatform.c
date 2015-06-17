/***********************************************************************
 *
 * File: linux/kernel/drivers/video/stmfbplatform.c
 * Copyright (c) 2000-2010 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/fb.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/kthread.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/console.h>
#include <linux/string.h>
#include <linux/dma-mapping.h>

#if defined(CONFIG_BPA2)
#include <linux/bpa2.h>
#else
#error Kernel must have the BPA2 memory allocator configured
#endif

#include <asm/irq.h>
#include <asm/uaccess.h>
#include <linux/semaphore.h>

#include <stm_display.h>
#include <linux/stm/stmcoredisplay.h>

#include "stmfb.h"
#include "stmfbinfo.h"
#include "linux/kernel/drivers/stm/hdmi/stmhdmi.h"

#include <linux/of.h>
#include <linux/of_platform.h>

/*
 * Define the maximum number of framebuffers/display pipelines the
 * driver can support.
 */
#define NR_FRAMEBUFFERS 4

/* See stmfbinfo.h for the device structure */
static struct stmfb_info stmfb_info[NR_FRAMEBUFFERS];

static char *display0;
static char *display1;
static char *display2;
static char *display3;

module_param(display0, charp, 0444);
MODULE_PARM_DESC(display0, "mode:memory:auxmem:default TV encoding:analogue colour:digital colour");

module_param(display1, charp, 0444);
MODULE_PARM_DESC(display1, "mode:memory:auxmem:default TV encoding:analogue colour:digital colour");

module_param(display2, charp, 0444);
MODULE_PARM_DESC(display2, "mode:memory:auxmem:default TV encoding:analogue colour:digital colour");

module_param(display3, charp, 0444);
MODULE_PARM_DESC(display3, "mode:memory:auxmem:default TV encoding:analogue colour:digital colour");

/*
    Panel related paramters for custom modes
*/

static char *panel0=0;
module_param(panel0, charp, 0444);
MODULE_PARM_DESC(panel0, "panel related configuration");

/*
 * We will register as many framebuffer devices as available display
 * piplines.
 * Each framebuffer device will need to store display platform data.
 */
static struct stmcore_display_pipeline_data platform_data[NR_FRAMEBUFFERS];


/************************************************************************
 *  Initialization and cleanup code
 */

static void stmfb_encode_videomode(const stm_display_mode_t *mode, struct fb_videomode *vm);

static int conv_atoi(const char *name)
{
  int val = 0;

  for (;; name++)
  {
    switch (*name)
    {
      case '0' ... '9':
        val = 10*val+(*name-'0');
      break;
      default:
      return val;
    }
  }
}

static int stmfb_parse_sd_standard_parameter(char *tvstandard, struct stmfb_info *i, int display)
{
  switch(*tvstandard)
  {
    case 'N':
    case 'n':
    {
      if(!strcmp(tvstandard,"NTSC-J"))
      {
        DPRINTK("Selecting NTSC-J output\n");
        i->default_sd_encoding = STM_OUTPUT_STD_NTSC_J;
      }
      else if(!strcmp(tvstandard,"NTSC-443"))
      {
        DPRINTK("Selecting NTSC-443 output\n");
        i->default_sd_encoding = STM_OUTPUT_STD_NTSC_443;
      }
      else
      {
        DPRINTK("Selecting NTSC (US) output\n");
        i->default_sd_encoding = STM_OUTPUT_STD_NTSC_M;
      }
      break;
    }
    case 'S':
    case 's':
    {
      DPRINTK("Selecting SECAM output\n");
      i->default_sd_encoding = STM_OUTPUT_STD_SECAM;
      break;
    }
    case 'P':
    case 'p':
    {
      if(!strcmp(tvstandard,"PAL-M"))
      {
        DPRINTK("Selecting PAL-M output\n");
        i->default_sd_encoding = STM_OUTPUT_STD_PAL_M;
      }
      else if(!strcmp(tvstandard,"PAL-N"))
      {
        DPRINTK("Selecting PAL-N output\n");
        i->default_sd_encoding = STM_OUTPUT_STD_PAL_N;
      }
      else if(!strcmp(tvstandard,"PAL-Nc"))
      {
        DPRINTK("Selecting PAL-Nc output\n");
        i->default_sd_encoding = STM_OUTPUT_STD_PAL_Nc;
      }
      else if(!strcmp(tvstandard, "PAL-60"))
      {
        DPRINTK("Selecting PAL-60 output\n");
        i->default_sd_encoding = STM_OUTPUT_STD_PAL_60;
      }
      else
      {
        DPRINTK("Selecting PAL-BDGHI output\n");
        i->default_sd_encoding = STM_OUTPUT_STD_PAL_BDGHI;
      }

      break;
    }
    default:
    {
      DPRINTK("Invalid default SD output standard, falling back to NTSC\n");
    }
  }

  return 0;
}


static int stmfb_parse_analog_output_parameters(char *analog_params, struct stmfb_info *i, int display)
{
  int      param_count=0;
  char    *output_param = NULL;
  uint32_t outputcaps;
  int      ret;

  if((ret = stm_display_output_get_capabilities(i->hFBMainOutput,&outputcaps))<0)
  {
    DPRINTK("Cannot get output capabilities error return = %d\n",ret);
    return ret;
  }

  DPRINTK("Analog Video params for display%d : '%s'\n", display, analog_params);
  output_param = strsep(&analog_params,"+");

  while(output_param)
  {
    param_count++;
    DPRINTK("  -- output_param%d='%s'\n", param_count, output_param);
    switch(*output_param)
    {
      case 'C':
      case 'c':
        if(outputcaps & (OUTPUT_CAPS_CVBS_YC_EXCLUSIVE |
                         OUTPUT_CAPS_SD_RGB_CVBS_YC    |
                         OUTPUT_CAPS_SD_YPbPr_CVBS_YC))
        {
          DPRINTK("Selecting CVBS on display %d\n",display);
          i->main_config.analogue_config |= STMFBIO_OUTPUT_ANALOGUE_CVBS;
        }
        else
        {
          printk(KERN_WARNING "Cannot select CVBS on display %d, ignoring option\n",display);
        }
        break;
      case 'S':
      case 's':
        if(outputcaps & (OUTPUT_CAPS_CVBS_YC_EXCLUSIVE |
                         OUTPUT_CAPS_SD_RGB_CVBS_YC    |
                         OUTPUT_CAPS_SD_YPbPr_CVBS_YC))
        {
          DPRINTK("Selecting S-Video (Y/C) on display %d\n",display);
          i->main_config.analogue_config |= STMFBIO_OUTPUT_ANALOGUE_YC;
        }
        else
        {
          printk(KERN_WARNING "Cannot select S-Video (Y/C) on display %d, ignoring option\n",display);
        }
        break;
      case 'R':
      case 'r':
        if(outputcaps & (OUTPUT_CAPS_RGB_EXCLUSIVE |
                         OUTPUT_CAPS_SD_RGB_CVBS_YC))
        {
          DPRINTK("Selecting RGB on display %d\n",display);
          i->main_config.analogue_config |= STMFBIO_OUTPUT_ANALOGUE_RGB;
        }
        else
        {
          printk(KERN_WARNING "Cannot select RGB on display %d, ignoring option\n",display);
        }
        break;
      case 'Y':
      case 'y':
        if(outputcaps & (OUTPUT_CAPS_YPbPr_EXCLUSIVE |
                         OUTPUT_CAPS_SD_YPbPr_CVBS_YC))
        {
          DPRINTK("Selecting YPrPb on display %d\n",display);
          i->main_config.analogue_config |= STMFBIO_OUTPUT_ANALOGUE_YPrPb;
        }
        else
        {
          printk(KERN_WARNING "Cannot select YPrPb on display %d, ignoring option\n",display);
        }
        break;
      default:
        printk(KERN_WARNING "Unknown analog video output option : '%s'\n", output_param);
        break;
    }
    output_param = strsep(&analog_params,"+");
  }

  return 0;
}


static int stmfb_parse_hdmi_format_parameters(char *hdmiformat, struct stmfb_info *i, int display)
{
  switch(*hdmiformat)
  {
    case 'Y':
    case 'y':
      i->main_config.hdmi_config |= STMFBIO_OUTPUT_HDMI_YUV;
      break;
    default:
      DPRINTK("Unknown hdmi video format option - falling back to RGB\n");
      /* no break */
    case 'R':
    case 'r':
      i->main_config.hdmi_config &= ~STMFBIO_OUTPUT_HDMI_YUV;
      break;
  }

  return 0;
}


static int stmfb_parse_module_parameters(struct stmfb_info *i, int display)
{
  char *paramstring       = NULL;
  char *copy              = NULL;
  char *tmpstr            = NULL;
  char *mode              = NULL;
  char *memsize           = NULL;
  char *auxmemsize        = NULL;
  char *tvstandard        = NULL;
  char *analogvideo       = NULL;
  char *hdmivideo         = NULL;
  unsigned long fbsize = 0;
  int stride;
  int ret = 0;


  /*
   * Note: slightly messy due to module parameter limitations and not wanting to
   * use parameter arrays.
   */
  switch(display)
  {
    case 0:
      paramstring = display0;
      break;
    case 1:
      paramstring = display1;
      break;
    case 2:
      paramstring = display2;
      break;
    case 3:
      paramstring = display3;
      break;
    default:
      break;
  }

  printk("stmfb: fb%d parameters = \"%s\"\n",display,(paramstring==NULL)?"<NULL>":paramstring);

  if(paramstring == NULL || *paramstring == '\0')
    return -ENODEV;

  copy = kstrdup(paramstring, GFP_KERNEL);
  if(!copy)
    return -ENOMEM;

  tmpstr         = copy;
  mode           = strsep(&tmpstr,":,");
  memsize        = strsep(&tmpstr,":,");
  auxmemsize     = strsep(&tmpstr,":,");
  tvstandard     = strsep(&tmpstr,":,");
  analogvideo    = strsep(&tmpstr,":,");
  hdmivideo      = strsep(&tmpstr,":,");

  if(!mode || *mode == '\0')
  {
    DPRINTK("No mode string found\n");
    ret = -ENODEV;
    goto free_copy_and_exit;
  }

  if(fb_find_mode(&i->info.var,&i->info,mode,i->videomodedb,ARRAY_SIZE(i->videomodedb),NULL,16) == 0)
  {
    DPRINTK("No suitable mode found (not even a default!)\n");
    ret = -ENODEV;
    goto free_copy_and_exit;
  }

  if(memsize)
  {
    fbsize = (unsigned long)memparse(memsize,&memsize);
    DPRINTK("requested memory = %lu\n",fbsize);
  }

  stride = i->info.var.xres * (i->info.var.bits_per_pixel/8);
  i->info.var.yres_virtual  = fbsize / stride;

  if(i->info.var.yres_virtual < i->info.var.yres)
  {
    DPRINTK("Requested memory is too small for default video mode, fixing it\n");
    fbsize = i->info.var.yres * stride;
  }

  /*
   * Having determined size, set the virtual yres to equal yres for the
   * default mode.
   */
  i->info.var.yres_virtual = i->info.var.yres;

  i->ulFBSize = fbsize;

  if(auxmemsize)
  {
    i->AuxSize[0] = (unsigned long)memparse(auxmemsize,&auxmemsize);
    DPRINTK("requested %lu bytes of auxiliary memory\n",i->AuxSize[0]);
  }
  else
    i->AuxSize[0] = 0;

  i->default_sd_encoding = STM_OUTPUT_STD_NTSC_M;

  if (tvstandard && (strlen(tvstandard) > 0))
  {
    if((ret = stmfb_parse_sd_standard_parameter(tvstandard, i, display))<0)
      goto free_copy_and_exit;
  }

  /* Reset SD TV Encoding */
  i->main_config.sdtv_encoding = 0;

  if(i->main_config.caps & STMFBIO_OUTPUT_CAPS_SDTV_ENCODING)
    i->main_config.sdtv_encoding = i->default_sd_encoding;

  /* Reset Analog Output Formats */
  i->main_config.analogue_config &= ~STMFBIO_OUTPUT_ANALOGUE_MASK;

  /*
   * Set the output format
   */
  if(analogvideo && (strlen(analogvideo) > 0))
  {
    if(i->main_config.caps & STMFBIO_OUTPUT_CAPS_ANALOGUE_CONFIG)
    {
      if((ret = stmfb_parse_analog_output_parameters(analogvideo, i, display))<0)
        goto free_copy_and_exit;
    }
  }

  if(i->hFBHDMI && hdmivideo && (strlen(hdmivideo) > 0))
  {
    if((ret = stmfb_parse_hdmi_format_parameters(hdmivideo, i, display))<0)
      goto free_copy_and_exit;
  }

free_copy_and_exit:
  kfree(copy);
  return ret;
}

static int stmfb_parse_module_panel_parameters(struct stmfb_info *i, int display)
{
  int stride;
  int idx;
  unsigned int namelen;
  int ret = 0;
  unsigned long fbsize = 0;
  //uint16_t bpp = 0;
  unsigned int factor = 1;
  struct fb_videomode vm;
  stm_display_panel_config_t panel_config;
  stm_display_mode_t panel_mode = {
    STM_TIMING_MODE_RESERVED,
    { 60000, STM_PROGRESSIVE_SCAN, 1920, 1080, 24, 41,
    STM_OUTPUT_STD_UNDEFINED, {{0,0},{0,0}}, {0,0}, STM_MODE_FLAGS_NONE },
    { 2200, 1125, 148500000, STM_SYNC_NEGATIVE, 2, STM_SYNC_NEGATIVE, 6}};
  /* string params */
  char *copy              = NULL;
  char *paramstring       = NULL;
  char *tmpstr            = NULL;
  char *mode              = NULL;
  char *memsize           = NULL;
  char *auxmemsize        = NULL;
  char *htotal            = NULL;
  char *vtotal            = NULL;
  char *hstart            = NULL;
  char *vstart            = NULL;
  char *hsyncwidth        = NULL;
  char *vsyncwidth        = NULL;
  char *pixel_freq        = NULL;
  char *pwr_on_t1         = NULL;
  char *pwr_on_t2         = NULL;
  char *pwr_dn_t1         = NULL;
  char *pwr_dn_t2         = NULL;

  paramstring = panel0;

  printk("stmfb: fb%d parameters = \"%s\"\n",display,(paramstring==NULL)?"<NULL>":paramstring);

  if(paramstring == NULL || *paramstring == '\0')
    return -ENODEV;

  copy = kstrdup(paramstring, GFP_KERNEL);
  if(!copy)
    return -ENOMEM;

  memset(&panel_config, 0, sizeof(panel_config));

  tmpstr         = copy;

  /* Parse panel related configurations */
  mode           = strsep(&tmpstr,":,");
  memsize        = strsep(&tmpstr,":,");
  auxmemsize     = strsep(&tmpstr,":,");
  htotal         = strsep(&tmpstr,":,");
  vtotal         = strsep(&tmpstr,":,");
  hstart         = strsep(&tmpstr,":,");
  vstart         = strsep(&tmpstr,":,");
  hsyncwidth     = strsep(&tmpstr,":,");
  vsyncwidth     = strsep(&tmpstr,":,");
  pixel_freq     = strsep(&tmpstr,":,");
  pwr_on_t1      = strsep(&tmpstr,":,");
  pwr_on_t2      = strsep(&tmpstr,":,");
  pwr_dn_t1      = strsep(&tmpstr,":,");
  pwr_dn_t2      = strsep(&tmpstr,":,");

  if(!mode || *mode == '\0')
  {
    DPRINTK("No mode string found\n");
    ret = -ENODEV;
    goto panel_free_copy_and_exit;
  }

  /* Parse mode string params */
  namelen = strlen(mode);
  for (idx = namelen-1; idx >= 0; idx--)
  {
    switch (mode[idx])
    {
      case '@':
        namelen = idx;
        panel_mode.mode_params.vertical_refresh_rate = conv_atoi(&mode[idx+1])*1000;
      break;
      case '-':
        namelen = idx;
        //bpp = conv_atoi(&mode[idx+1]);
      break;
      case 'x':
        panel_mode.mode_params.active_area_height = conv_atoi(&mode[idx+1]);
      break;
      case '0' ... '9':
      break;
      default:
      break;
     }
   }
   panel_mode.mode_params.active_area_width = conv_atoi(mode);

   /* Parse pixel frequency */
  namelen = strlen(pixel_freq);
  for (idx = namelen-1; idx >= 0; idx--)
  {
    switch (pixel_freq[idx])
    {
      case 'K':
        namelen = idx;
        factor = 1000;
      break;
      case 'k':
        namelen = idx;
        factor = 1000;
      break;
      case '0' ... '9':
      break;
      default:
      break;
    }
  }
  panel_mode.mode_timing.pixel_clock_freq = conv_atoi(pixel_freq)*factor;

   /* Parse total/start/sync width */
  panel_mode.mode_timing.pixels_per_line = conv_atoi(htotal);
  panel_mode.mode_timing.lines_per_frame = conv_atoi(vtotal);
  panel_mode.mode_params.active_area_start_pixel = conv_atoi(hstart);
  panel_mode.mode_params.active_area_start_line = conv_atoi(vstart);
  panel_mode.mode_timing.hsync_width = conv_atoi(hsyncwidth);
  panel_mode.mode_timing.vsync_width = conv_atoi(vsyncwidth);

    /* Parse panel power power timing */
  panel_config.panel_power_up_timing.pwr_to_de_delay_during_power_on = \
  conv_atoi(pwr_on_t1);
  panel_config.panel_power_up_timing.de_to_bklt_on_delay_during_power_on = \
  conv_atoi(pwr_on_t2);
  panel_config.panel_power_up_timing.bklt_to_de_off_delay_during_power_off = \
  conv_atoi(pwr_dn_t1);
  panel_config.panel_power_up_timing.de_to_pwr_delay_during_power_off= \
  conv_atoi(pwr_dn_t2);

  INIT_LIST_HEAD(&i->info.modelist);

  stmfb_encode_videomode(&panel_mode, &vm);

  /*
   * Create a state mode database of just the supported modes for module
   * parameter parsing.
   */
  i->videomodedb[0] = vm;

  /*
   * Add to the framework's mode list for sysfs
   */
  fb_add_videomode(&vm, &i->info.modelist);

  if(fb_find_mode(&i->info.var,&i->info,mode,i->videomodedb,ARRAY_SIZE(i->videomodedb),NULL,16) == 0)
  {
    DPRINTK("No suitable mode found (not even a default!)\n");
    ret = -ENODEV;
    goto panel_free_copy_and_exit;
  }

  /* Setting panel configuration for panel power timing */
  stm_display_output_set_compound_control(i->hFBMainOutput, \
  OUTPUT_CTRL_PANEL_CONFIGURE, &panel_config);

  /* Setting custom dislay timing mode for panel */
  stm_display_output_start(i->hFBMainOutput, (const stm_display_mode_t *)&panel_mode);

  if(memsize)
  {
    fbsize = (unsigned long)memparse(memsize,&memsize);
    DPRINTK("requested memory = %lu\n",fbsize);
  }

  stride = i->info.var.xres * (i->info.var.bits_per_pixel/8);
  i->info.var.yres_virtual  = fbsize / stride;

  if(i->info.var.yres_virtual < i->info.var.yres)
  {
    DPRINTK("Requested memory is too small for default video mode, fixing it\n");
    fbsize = i->info.var.yres * stride;
  }

  /*
   * Having determined size, set the virtual yres to equal yres for the
   * default mode.
   */
  i->info.var.yres_virtual = i->info.var.yres;

  i->ulFBSize = fbsize;

  if(auxmemsize)
  {
    i->AuxSize[0] = (unsigned long)memparse(auxmemsize,&auxmemsize);
    DPRINTK("requested %lu bytes of auxiliary memory\n",i->AuxSize[0]);
  }
  else
    i->AuxSize[0] = 0;

panel_free_copy_and_exit:
  kfree(copy);
  return ret;
}

static void stmfb_encode_videomode(const stm_display_mode_t *mode, struct fb_videomode *vm)
{
  u64 timingtmp;

  /* TODO: this really ought to be merged into stmfb_encode_var() since the conversions
   *       absolutely must not disagree (otherwise we end up with spurious entries in
   *       the modelist).
   */

  vm->name = NULL;
  vm->refresh = mode->mode_params.vertical_refresh_rate / 1000;
  vm->xres = mode->mode_params.active_area_width;
  vm->yres = mode->mode_params.active_area_height;

  timingtmp = 1000000000000ULL + (mode->mode_timing.pixel_clock_freq/2);
  do_div(timingtmp, mode->mode_timing.pixel_clock_freq);
  vm->pixclock = (u32) timingtmp;

  vm->left_margin  = (mode->mode_params.active_area_start_pixel -
                      mode->mode_timing.hsync_width);

  vm->right_margin = (mode->mode_timing.pixels_per_line -
                      mode->mode_params.active_area_start_pixel -
                      mode->mode_params.active_area_width);

  vm->hsync_len    = mode->mode_timing.hsync_width;

  vm->vsync_len    = mode->mode_timing.vsync_width;

  vm->upper_margin = (mode->mode_params.active_area_start_line - 1);

  if (STM_INTERLACED_SCAN == mode->mode_params.scan_type) {
    vm->vsync_len   *= 2;
    vm->upper_margin = ((mode->mode_params.active_area_start_line - 1) *2)
                        - vm->vsync_len;
  } else {
    vm->upper_margin = (mode->mode_params.active_area_start_line - 1)
                        - vm->vsync_len;
  }

  vm->lower_margin = (mode->mode_timing.lines_per_frame -
                      mode->mode_params.active_area_height-
                      vm->vsync_len -
                      vm->upper_margin);

  /* set all the bitfields to zero and fill them in programatically */
  vm->sync = 0;
  vm->vmode = 0;
  vm->flag = 0;

  if (mode->mode_timing.hsync_polarity)
    vm->sync|=FB_SYNC_HOR_HIGH_ACT;

  if (mode->mode_timing.vsync_polarity)
    vm->sync|=FB_SYNC_VERT_HIGH_ACT;


  if (STM_INTERLACED_SCAN == mode->mode_params.scan_type) {
    vm->vmode |= FB_VMODE_INTERLACED;
  } else {
    vm->vmode |= FB_VMODE_NONINTERLACED;
  }
  /* TODO: FB_VMODE_DOUBLE */

  if (mode->mode_params.hdmi_vic_codes[STM_AR_INDEX_4_3] ||
      mode->mode_params.hdmi_vic_codes[STM_AR_INDEX_16_9]) {
    vm->flag |= FB_MODE_IS_STANDARD; /* meaning, in this case, CEA-861 */
  }
}


static void stmfb_enumerate_modes(struct stmfb_info *i)
{
  int n;
  stm_display_mode_t  mode;
  struct fb_videomode vm;

  INIT_LIST_HEAD(&i->info.modelist);

  for (n=0; n<STM_TIMING_MODE_COUNT; n++) {
    if (stm_display_output_get_display_mode(i->hFBMainOutput, n, &mode)<0)
      continue;

    stmfb_encode_videomode(&mode, &vm);

    /*
     * Create a state mode database of just the supported modes for module
     * parameter parsing.
     */
    i->videomodedb[n] = vm;

    /*
     * Add to the framework's mode list for sysfs
     */
    fb_add_videomode(&vm, &i->info.modelist);
  }

  /* the modelist is automatically destroyed by unregister_framebuffer() so
   * we only have to worry about leaks on error paths before we are successfully
   * registered.
   */
}

/*
 *
 */
static void stmfb_destroy_auxmem(struct stmfb_info *i)
{
  unsigned int idx;

  for (idx = 0; idx < ARRAY_SIZE (i->AuxPart); ++idx)
  {
    if (i->AuxPart[idx])
    {
      bpa2_free_pages (i->AuxPart[idx], i->AuxBase[idx]);
      i->AuxPart[idx] = NULL;
      i->AuxBase[idx] = 0;
      i->AuxSize[idx] = 0;
    }
  }
}

/*
 * stmfb_destroyfb
 * Unregister a framebuffer, clear it off the display and release
 * its resources.
 */
static int __exit stmfb_destroyfb(struct stmfb_info *i)
{
  DPRINTK("\n");

#if defined(SDK2_ENABLE_STMFB_ATTRIBUTES)
  stmfb_cleanup_class_device(i);
#endif

  unregister_framebuffer(&i->info);

  if ( i->info.apertures )
    {
      kfree( i->info.apertures );
      i->info.apertures = 0;
    }

  if(i->hFBSource)
  {

    if(i->hQueueInterface != NULL)
    {
      // Unlock this queue for our exclusive usage
      stm_display_source_queue_unlock(i->hQueueInterface);
      stm_display_source_queue_release(i->hQueueInterface);
      i->hQueueInterface = NULL;
    }

    stm_display_plane_disconnect_from_source(i->hFBPlane, i->hFBSource);
    stm_display_source_close(i->hFBSource);

    i->hFBSource  = NULL;
  }

  if(i->hFBPlane && i->hFBMainOutput)
  {
    stm_display_plane_disconnect_from_output(i->hFBPlane, i->hFBMainOutput);
    stm_display_output_stop(i->hFBMainOutput);
    stm_display_plane_close(i->hFBPlane);

    i->hFBPlane  = NULL;
  }

  if(i->ulPFBBase)
  {
    if (!bpa2_low_part(i->FBPart))
      iounmap(i->info.screen_base);
    i->info.screen_base = NULL;

    bpa2_free_pages (i->FBPart, i->ulPFBBase);
    i->ulPFBBase = 0;
    i->FBPart = NULL;
  }

  fb_dealloc_cmap (&i->info.cmap);

  stmfb_destroy_auxmem (i);

  return 0;
}

/*
 *  stmfb_createfb
 *  Create framebuffer related state and register with upper layers
 */
static int stmfb_createfb(struct stmfb_info *i, int display, const char *name)
{
  int           ret=0;
  unsigned long nPages;
  int           fb_registered = 0;
  uint32_t      status;
  unsigned int  idx = 0;        /* index into i->AuxPart[] */

  DPRINTK("\n");

  /*
   * Need to set the framebuffer operation table and get hold of the display
   * plane before parsing the module parameters so fb_find_mode can work.
   */
  i->info.fbops = &stmfb_ops;

  if(panel0 && display==0)
  {
    if((ret = stmfb_parse_module_panel_parameters(i, display)) < 0)
      goto failed0;
  }
  else
  {
    /*
     * Copy all available display modes into the modelist, before we parse
     * the module parameters to get the default mode.
     */
    stmfb_enumerate_modes(i);
    if((ret = stmfb_parse_module_parameters(i, display)) < 0)
      goto failed0;
  }

  nPages = (i->ulFBSize + PAGE_SIZE - 1) / PAGE_SIZE;

  i->FBPart = bpa2_find_part ("bigphysarea");
  i->ulPFBBase = i->FBPart ? bpa2_alloc_pages (i->FBPart, nPages, 0, GFP_KERNEL)
                           : 0;
  if (!i->ulPFBBase)
  {
    printk(KERN_WARNING "Failed to allocate fb%d memory, requested size = %lu\n",display, i->ulFBSize);
    ret = -ENOMEM;
    goto failed0;
  }


  /* try to allocate memory from BPA2 as additional memory for graphics
     operations. Basically, this is not vital, but driver loading will still
     fail if an auxsize has been specified in the module parameters which can
     not be satisfied, either because no BPA2 partition 'gfx-memory' exists,
     or if it's not large enough for the given auxsize. */
  /* Please note that due to hardware limitations, this can actually be a bit
     complex (2 & 3):
     1) we look for partitions labelled 'gfx-memory-[0...x]', each of which
        should be 64MB in size and be aligned to a 64MB bank. This makes the
        process quite easy for us.
     2) Failing that, we try to use a partition labelled 'gfx-memory'. Now we
        have to make sure ourselves that each allocation does not cross a 64MB
        bank boundary.
     3) Failing that, too, or in case the 'gfx-memory' partition not being
        large enough to accomodate for the amount of gfx memory requested in
        the module options, we will do the same but this time use the
        'bigphysarea' partition. */
  /* So, either one can configure several 'gfx-memory-%d' partitions
     (preferred, to have maximum control over placement of gfx memory), just
     use one large 'gfx-memory' partition, or configure nothing at all and be
     limited to 'bigphysarea' memory.
     The combined memory of all the allocations will be made available to
     DirectFB. */
  /* FIXME: this code is way too complex! */
#define MEMORY64MB_SIZE      (1<<26)
  if (i->AuxSize[0])
  {
    /* aux memory was requested */
    /* order of partnames is important here! */
    static const char *partnames[] = { "gfx-memory-%d", "gfx-memory", "bigphysarea" };
    unsigned int       partnameidx /* index into partnames[] */,
                       partidx     /* index in case of 'gfx-memory-%d' */;
    char               partname[14]; /* real name in case of 'gfx-memory-%d' */
    struct bpa2_part  *part;
    unsigned long      still_needed = i->AuxSize[0];

    unsigned long      this_alloc;
    int                alignment;

    partidx = 0;
    /* find a partition suitable for us, in the preferred order, as outlined
       above */
    for (partnameidx = 0; partnameidx < ARRAY_SIZE (partnames); ++partnameidx)
    {
      sprintf (partname, partnames[partnameidx], 0);
      part = bpa2_find_part (partname);
      if (part)
        break;
    }

    if (!part)
    {
      printk (KERN_ERR "no BPA2 partitions found for auxmem!\n");
      goto failed_bigphys;
    }

    idx = 0;

restart:
    if (still_needed >= MEMORY64MB_SIZE)
    {
      /* we first try to satisfy from a 64MB aligned region */
      this_alloc = MEMORY64MB_SIZE;
      /* don't request a specific alignment if individual 'gfx-memory-%d'
         partitions are configured in the kernel */
      alignment  = (partnameidx == 0) ? 0 : MEMORY64MB_SIZE / PAGE_SIZE;
    }
    else
    {
      /* if requested size is < 64MB, we are optimistic and hope it will
         fit into one 64MB bank without alignment restrictions. This
         doesn't mean we will be happy with a region crossing that
         boundary, i.e. we will still make sure all restrictions are met.
         We are really being just optimistic here. */
      this_alloc = still_needed;
      alignment  = 0;
    }

    while (part && idx < ARRAY_SIZE (i->AuxPart) && still_needed)
    {
      int           this_pages;
      unsigned long base;

      printk (KERN_INFO "trying to alloc %lu bytes (align: %.4x, still needed:"
                        " %lu) from '%s' for GFX%d auxmem\n",
              this_alloc, alignment, still_needed, partname, display);

      this_pages = (this_alloc + PAGE_SIZE - 1) / PAGE_SIZE;
      base = bpa2_alloc_pages (part, this_pages, alignment, GFP_KERNEL);
      if (base)
      {
        /* make sure it doesn't cross a 64MB boundary. At some point we will
           be using the new BPA2 allocator API... */
        if ((base & ~(MEMORY64MB_SIZE - 1)) != ((base + this_alloc - 1) & ~(MEMORY64MB_SIZE - 1)))
        {
          unsigned long topAddress;

          printk ("%s: %8.lx + %lu (%d pages) crosses a 64MB boundary, retrying!\n",
                  __FUNCTION__, base, this_alloc, this_pages);

          /* free and try a new reservation with different attributes,
             hoping nobody requests bpa2 memory in between... */
          bpa2_free_pages (part, base);

          if (partnameidx == 0)
          {
            /* if we have 'gfx-memory-%d' partitions, try the next one */
            /* Getting here doesn't neccessarily mean there is an error in
               the BPA2 partition definition. This can happen if e.g. two
               framebuffers are configured and the auxmem size requested is
               larger but not a multiple of 64MB (since we are optimistic and
               try to re-use partially used partitions), so don't be tempted
               to put a WARN_ON() here! */
            sprintf (partname, partnames[partnameidx], ++partidx);
            part = bpa2_find_part (partname);
            continue;
          }

          if (still_needed == this_alloc && alignment == 0)
          {
            /* this can only happen on the last chunk of memory needed. So
               we first try to fit it into its own region.
               I.e. we first try to put the last chunk into a partly used
               bank. If we succeed, but the chunk now spans a boundary
               (which gets us here), we try to put it into its own
               bank. If that fails, too, we will come back again (this
               time with alignment == 1) and split the chunk into two. */
            alignment = MEMORY64MB_SIZE / PAGE_SIZE;
            continue;
          }

          /* standard case, allocate up to the end of current 64MB bank,
             effectively splitting the current chunk into two. */
#define _ALIGN_UP(addr,size)    (((addr)+((size)-1))&(~((size)-1)))
          topAddress = _ALIGN_UP (base, MEMORY64MB_SIZE);
          this_alloc = topAddress - base;
          this_pages = (this_alloc + PAGE_SIZE - 1) / PAGE_SIZE;

          /* alignment should be 0 or 1 here */
          WARN_ON (alignment != 0
                   && alignment != 1);
          base = bpa2_alloc_pages (part, this_pages, alignment, GFP_KERNEL);
          if (!base)
          {
            /* shouldn't happen here, since we just moments ago suceeded in
               allocating even more memory than now, so we don't know what
               to do and kindof panic ...*/
            break;
          }
        }

        /* we now have found a more or less nice place to chill. */
        i->AuxPart[idx] = part;
        i->AuxBase[idx] = base;
        i->AuxSize[idx] = this_alloc;
        ++idx;

        still_needed -= this_alloc;

        printk (KERN_INFO "success: base: %.8lx, still needed: %lu\n",
                base, still_needed);

        if (still_needed >= MEMORY64MB_SIZE)
        {
          /* we first try to satisfy from a 64MB aligned region */
          this_alloc = MEMORY64MB_SIZE;
          alignment  = (partnameidx == 0) ? 0 : MEMORY64MB_SIZE / PAGE_SIZE;
        }
        else
        {
          /* if requested size is < 64MB we are on the last chunk of memory
             blocks. We are otimistic and hope it will fit into a possibly
             already partly used 64MB bank without alignment restrictions,
             i.e. we hope it will not span a 64MB boundary. If that's not
             the case, we will again use an alignment of 64MB, so as to
             reduce scattering of the auxmem chunks. */
          this_alloc = still_needed;
          alignment  = 0;

          /* if using 'gfx-memory-%d' partitions, try to share the last
             chunk with another chunk (in a possibly partly used
             partition). */
          if (partnameidx == 0)
            partidx = -1;
        }

        if (partnameidx == 0)
        {
          /* in case we are using 'gfx-memory-%d' as partition name, make
             sure we advance to the next partition */
          sprintf (partname, partnames[partnameidx], ++partidx);
          part = bpa2_find_part (partname);
        }
      }
      else
      {
        if (alignment > 1)
        {
          if (still_needed == this_alloc)
            /* make sure not to get into an endless loop which would be
               switching around the alignment all the time. */
            alignment = 1;
          else
            /* try again with different alignment rules */
            alignment = 0;
        }
        else
        {
          /* failed: couldn't allocate any memory at all, even without any
             alignment restrictions.
             1) In case there are 'gfx-memory-%d' partitions, we advance
                to the next one.
             2) In case we had been using 'gfx-memory' as partition name,
                we now try 'bigphysarea'*/
          if (partnameidx == 0)
          {
            /* use next 'gfx-memory-%d' partition */
            BUG_ON (strcmp (partnames[partnameidx], "gfx-memory-%d"));
            sprintf (partname, partnames[partnameidx], ++partidx);
            part = bpa2_find_part (partname);
          }
          else if (partnameidx == 1)
          {
            /* advance from 'gfx-memory' to 'bigphysarea' partition name */
            BUG_ON (strcmp (partnames[partnameidx], "gfx-memory"));
            sprintf (partname, partnames[++partnameidx], 0);
            BUG_ON (strcmp (partnames[partnameidx], "bigphysarea"));
            part = bpa2_find_part (partname);
            goto restart;
          }
          else
            /* that's it, there are no partitions left to check for auxmem */
            break;
        }
      }
    }

    if (still_needed)
    {
      /* not enough chill space configured for BPA2 */
      printk (KERN_WARNING "Failed to allocate enough fb%d auxmem "
                           "(still need %lu bytes)\n",
              display, still_needed);
      ret = -ENOMEM;
      goto failed_auxmem;
    }
  }

  if (idx) {
    struct apertures_struct *ap;
    struct aperture* ranges;
    int c;

    i->info.apertures = alloc_apertures(idx);

    if (!i->info.apertures) {
      printk (KERN_WARNING "Failed to allocate memory for auxmem2 ranges\n");
      ret = -ENOMEM;
      goto failed_auxmem;
    }

    ap = i->info.apertures;

    ranges = &ap->ranges[0];

    for (c = 0; c < idx; c++) {
      ranges[c].base = i->AuxBase[c];
      ranges[c].size = i->AuxSize[c];
    }
  }

  if(stm_display_plane_connect_to_output(i->hFBPlane, i->hFBMainOutput) < 0)
  {
    printk(KERN_ERR "fb%d display cannot be connected to display output pipeline, this is very bad!\n",display);
    ret = -EIO;
    goto failed_auxmem;
  }

  /*
   * Lock the QueueBuffer Interface for our exclusive queue_buffer usage
   */
  if(i->hQueueInterface == NULL)
  {
    printk(KERN_WARNING "fb%d No interface registered\n",display);
    ret = -EIO;
    goto failed_auxmem;
  }

  ret = stm_display_source_get_status(i->hFBSource, &status);
  if ( (ret == 0) && (!(status & STM_STATUS_QUEUE_LOCKED)) )
  {
    // Lock this queue for our exclusive usage
    ret = stm_display_source_queue_lock(i->hQueueInterface, false);
  }
  else
  {
    ret = -EBUSY;
  }

  if (ret<0)
  {
    printk(KERN_WARNING "fb%d display source interface may already be in use by another driver\n",display);
    goto failed_qinterface;
  }

  i->main_config.activate = STMFBIO_ACTIVATE_IMMEDIATE;
  if(stmfb_set_output_configuration(&i->main_config,i)<0)
  {
    printk(KERN_WARNING "fb%d main output configuration is unsupported\n",display);
    ret = -EINVAL;
    goto failed_qinterface;
  }

  /*
   * Get the framebuffer layer default extended var state and capabilities
   */
  i->current_var_ex.layerid = 0;

  if((ret = stmfb_encode_var_ex(&i->current_var_ex, i)) < 0)
  {
    printk(KERN_WARNING "fb%d failed to get display plane's extended capabilities\n",display);
    goto failed_qinterface;
  }

  /* Setup the framebuffer info for registration */
  if (bpa2_low_part(i->FBPart))
  {
    i->info.screen_base = phys_to_virt(i->ulPFBBase);
  }
  else
  {
    i->info.screen_base = ioremap_nocache(i->ulPFBBase,i->ulFBSize);
  }
  i->info.flags       = FBINFO_DEFAULT |
                        FBINFO_PARTIAL_PAN_OK |
                        FBINFO_HWACCEL_YPAN;

  strncpy(i->info.fix.id, name, sizeof(i->info.fix.id)-1);  /* identification string */
  i->info.fix.id[sizeof(i->info.fix.id)-1] = '\0';

  i->info.fix.smem_start  = i->ulPFBBase;                /* Start of frame buffer mem (physical address)               */
  i->info.fix.smem_len    = i->ulFBSize;                 /* Length of frame buffer mem as the device sees it           */
  i->info.fix.type        = FB_TYPE_PACKED_PIXELS;       /* see FB_TYPE_*                     */
  i->info.fix.type_aux    = 0;                           /* Interleave for interleaved Planes */
  i->info.fix.xpanstep    = 0;                           /* zero if no hardware panning       */
  i->info.fix.ypanstep    = 1;                           /* zero if no hardware panning       */
  i->info.fix.ywrapstep   = 0;                           /* zero if no hardware ywrap         */

  i->info.pseudo_palette = &i->pseudo_palette;

  if(fb_alloc_cmap(&i->info.cmap, 256, 1)<0)
  {
    printk(KERN_ERR "fb%d unable to allocate colour map\n",display);
    ret = -ENOMEM;
    goto failed_ioremap;
  }

  if(i->info.cmap.len != 256)
  {
    printk(KERN_ERR "fb%d WTF colour map length is wrong????\n",display);
  }

  if (register_framebuffer(&i->info) < 0)
  {
    printk(KERN_ERR "fb%d register =_framebuffer failed!\n",display);
    ret = -ENODEV;
    goto failed_cmap;
  }

  /*
   * If there was no console activated on the registered framebuffer, we have
   * to force the display mode be updated. This sequence is cribbed from the
   * matroxfb driver.
   */
  if(!i->current_videomode_valid)
  {
    i->info.var.activate |= FB_ACTIVATE_FORCE;
    if((ret = fb_set_var(&i->info, &i->info.var))<0)
    {
      printk(KERN_WARNING "fb%d failed to set default display mode, display pipeline may already be in use\n",display);
      goto failed_register;
    }
  }

#if defined(SDK2_ENABLE_STMFB_ATTRIBUTES)
  stmfb_init_class_device(i);
#endif

  DPRINTK("out\n");

  return 0;

failed_register:
  fb_registered = 1;
  unregister_framebuffer (&i->info);

failed_cmap:
  fb_dealloc_cmap (&i->info.cmap);

failed_ioremap:
  if (!bpa2_low_part(i->FBPart))
    iounmap (i->info.screen_base);
  i->info.screen_base = NULL;

failed_qinterface:
  stm_display_source_queue_unlock(i->hQueueInterface);

failed_auxmem:
  stmfb_destroy_auxmem (i);

failed_bigphys:
  bpa2_free_pages (i->FBPart, i->ulPFBBase);
  i->ulPFBBase = 0;
  i->FBPart = NULL;

failed0:
  if (!fb_registered)
    /* unregister_framebuffer() does fb_destroy_modelist() for us, so don't
       destroy it twice! */
    fb_destroy_modelist (&i->info.modelist);

  return ret;
}


static void stmfb_cleanup_info(struct stmfb_info *i)
{
  if(i->hdmi_dev.private_data != NULL)
  {
    struct stm_hdmi *hdmi = (struct stm_hdmi *)i->hdmi_dev.private_data;
    module_put(hdmi->cdev.owner);
    i->hdmi_dev.private_data = NULL;
    i->hdmi_fops = NULL;
  }

  stm_display_output_close(i->hFBMainOutput);
  i->hFBMainOutput = NULL;

  stm_display_output_close(i->hFBDVO);
  i->hFBDVO = NULL;

  stm_display_output_close(i->hFBHDMI);
  i->hFBHDMI = NULL;

  if(i->hFBPlane && i->hFBSource)
    stm_display_plane_disconnect_from_source(i->hFBPlane, i->hFBSource);

  if(i->hFBPlane)
  {
    stm_display_plane_close(i->hFBPlane);
    i->hFBPlane = NULL;
  }

  if (i->hQueueInterface != NULL)
  {
    stm_display_source_queue_unlock(i->hQueueInterface);
    stm_display_source_queue_release(i->hQueueInterface);
    i->hQueueInterface = NULL;
  }

  if(i->hFBSource)
  {
    stm_display_source_close(i->hFBSource);
    i->hFBSource = NULL;
  }

  if(i->pFBCLUT)
  {
    dma_free_coherent(&i->platformDevice->dev, sizeof(unsigned long)*256, i->pFBCLUT, i->dmaFBCLUT);
    i->pFBCLUT = NULL;
  }

  stm_display_device_close(i->display_device);
  i->display_device = NULL;
}


static int stmfb_find_hdmi_device(struct stmfb_info *i, int hdmi_output_id)
{
struct device *dev = NULL;
struct stm_hdmi *hdmi = NULL;


    /*
     * This matches up the display platform device with the HDMI
     * platform device and its driver if one is loaded. This is a replacement
     * for the HDMI device driver context being stored directly in the display
     * platform device structure and being manipulated directly by the
     * framebuffer driver. Why do we need to do this?
     *
     * 1. When the framebuffer changes the display mode we need to "kick" the
     *    HDMI device driver state machine into trying to restart the HDMI
     *    output if it isn't currently active (i.e. say the previous framebuffer
     *    mode was not supported on HDMI. This is like issuing fake "hotplug"
     *    event.
     * 2. We want, for the moment, to continue supporting HDMI output
     *    configuration through STMFBIO_SET_OUTPUT_CONFIG, which is the
     *    current basis for the DirectFB screen/encoder interfaces. This may
     *    be reviewed in the future.
     */

    struct platform_device *pdev = NULL;
    struct device_node *hdmitx_root_node_p = NULL;
    struct device_node *hdmirepeater_root_node_p = NULL;

    hdmitx_root_node_p = of_find_compatible_node(NULL, NULL, "st,hdmi");
    hdmirepeater_root_node_p = of_find_compatible_node(NULL, NULL, "st,hdmi-repeater");

    if ((!hdmitx_root_node_p)&& (!hdmirepeater_root_node_p)) {
        printk(KERN_ERR "Failed to get hdmi(tx)/(repeater)_root_node_p \n");
        return -ENODEV;
    }

    if (hdmitx_root_node_p){
        pdev= of_find_device_by_node (hdmitx_root_node_p);
    } else {
        pdev= of_find_device_by_node (hdmirepeater_root_node_p);
    }
    if(pdev)
      dev = &(pdev->dev);
    if(dev == NULL)
    {
      printk(KERN_WARNING "No HDMI platform device found\n");
      return -ENODEV;
    }

    hdmi = (struct stm_hdmi *)dev_get_drvdata(dev);
    DPRINTK("Found HDMI platform device drvdata = %p\n",hdmi);
    if(!hdmi || (hdmi->hdmi_output_id != hdmi_output_id))
    {
      printk(KERN_WARNING "No HDMI platform driver loaded or it is not for the required HDMI output (%d)\n",hdmi_output_id);
      return -ENODEV;
    }

    if(!try_module_get(hdmi->cdev.owner))
    {
      printk(KERN_ERR "Cannot get HDMI driver module reference\n");
      return -ENODEV;
    }

    i->hdmi_dev.private_data = hdmi;
    i->hdmi_fops = hdmi->cdev.ops;

    return 0;
}


static int stmfb_probe_get_outputs(struct stmfb_info *i,struct stmcore_display_pipeline_data *pd)
{
  int ret;

  if((ret = stm_display_device_open_output(i->display_device, pd->main_output_id, &(i->hFBMainOutput))) != 0)
    return ret;

  i->main_config.outputid = STMFBIO_OUTPUTID_MAIN;
  stmfb_initialise_output_config(i->hFBMainOutput,&i->main_config);

  if(pd->dvo_output_id >= 0)
  {
    if((ret = stm_display_device_open_output(i->display_device, pd->dvo_output_id, &(i->hFBDVO))) != 0)
      return ret;
  }

  if(pd->hdmi_output_id >= 0)
  {
    if((ret = stm_display_device_open_output(i->display_device, pd->hdmi_output_id, &(i->hFBHDMI))) != 0)
      return ret;
  }

  if(i->hFBDVO)
  {
    i->main_config.caps |= STMFBIO_OUTPUT_CAPS_DVO_CONFIG;
    i->main_config.dvo_config = (STMFBIO_OUTPUT_DVO_CLIP_VIDEORANGE |
                                 STMFBIO_OUTPUT_DVO_YUV_444_16BIT   |
                                 STMFBIO_OUTPUT_DVO_DISABLED);

    stm_display_output_set_control(i->hFBDVO, OUTPUT_CTRL_CLIP_SIGNAL_RANGE, STM_SIGNAL_VIDEO_RANGE);
  }

  if(i->hFBHDMI)
  {
    if(stmfb_find_hdmi_device(i, pd->hdmi_output_id) == 0)
    {
      __u32 hdmi_disabled = 1;
      mm_segment_t oldfs = get_fs();
      i->main_config.caps |= STMFBIO_OUTPUT_CAPS_HDMI_CONFIG;

      set_fs(KERNEL_DS);
      i->hdmi_fops->unlocked_ioctl(&i->hdmi_dev, STMHDMIIO_GET_DISABLED, (unsigned long)&hdmi_disabled);
      set_fs(oldfs);

      i->main_config.hdmi_config = (hdmi_disabled==0)?STMFBIO_OUTPUT_HDMI_ENABLED:STMFBIO_OUTPUT_HDMI_DISABLED;
      i->main_config.hdmi_config |= (STMFBIO_OUTPUT_HDMI_RGB | STMFBIO_OUTPUT_HDMI_CLIP_VIDEORANGE);

      stm_display_output_set_control(i->hFBHDMI, OUTPUT_CTRL_CLIP_SIGNAL_RANGE, STM_SIGNAL_VIDEO_RANGE);
    }
    else
    {
      /*
       * If we haven't got a HDMI device driver loaded for the HDMI output
       * associated with this framebuffer's display pipeline then the
       * framebuffer driver will not interact at all with that HDMI output.
       */
      stm_display_output_close(i->hFBHDMI);
      i->hFBHDMI = NULL;
    }
  }

  return 0;
}


static int stmfb_probe_get_source(struct stmfb_info *i,struct stmcore_display_pipeline_data *pd)
{
  uint32_t              sourceID;
  int                   ret;
  uint32_t              status;
  stm_display_source_interface_params_t  iface_params;

  for(sourceID=0 ; ; sourceID++)  // Iterate available sources
  {
    if(stm_display_device_open_source(i->display_device, sourceID,&(i->hFBSource)) != 0)
    { // if we have tested all sources
      DPRINTK("Failed to get a source for plane \n");
      return -ENODEV;
    }

    /* Check that this source is not already used by someone else */
    ret = stm_display_source_get_status(i->hFBSource, &status);

    if ( (ret == 0) && (!(status & STM_STATUS_SOURCE_CONNECTED)) )
    {
      /* try to connect the source on the prefered plane */
      ret = stm_display_plane_connect_to_source(i->hFBPlane, i->hFBSource);
      if (ret == 0)
      {
        DPRINTK("Plane %p successfully connected to Source %p\n",i->hFBPlane, i->hFBSource);

        // Allocate QueueInterface handle
        iface_params.interface_type = STM_SOURCE_QUEUE_IFACE;
        ret = stm_display_source_get_interface(i->hFBSource, iface_params, (void**)&(i->hQueueInterface));
        if(ret == 0)
        {
          break; /* break as it is ok */
        }

        DPRINTK("%s: get queue interface failed\n", __PRETTY_FUNCTION__);
      }
    }

    stm_display_source_close(i->hFBSource);
    i->hFBSource = 0;
   }

  // we have found a source
  DPRINTK("Get Source %p success\n",i->hFBSource);
  return 0;
}


static int stmfb_probe_get_plane(struct stmfb_info *i,struct stmcore_display_pipeline_data *pd)
{
  /*
   * Get framebuffer plane, we do this here instead of in stmfb_createfb
   * as we need the device handle.
   *
   * The plane which we are going to use should use memory from the
   * system partition. At the moment we are not supporting framebuffer
   * allocation from secondary LMI.
   *
   * We are going to get a preferred graphical plane for the framebuffer
   * using stm_display_device_find_planes_with_capabilities() API.
   * To setup the appropriate plane capabilities we have to get the
   * output controls then check to which mixer/compositor the main
   * output is begin connected.
   *
   * Finally we have to ensure that the selected plane for main mixer
   * should not be sharable; mostly we will need to get a shared plane
   * for the Aux output while some platforms did have only one shared
   * plane.
   */
  int                            ret;
  uint32_t                       plane_id;
  uint32_t                       ctrlVal = 0;
  stm_plane_capabilities_t       plane_caps_value, plane_caps_mask;

  /*
   * Capabilites value for plane cannot depends on framebuffer id.
   * We cannot assume here that fb0 is for main display and fb1 is for
   * aux display since we could start ONLY the Aux display. The only way
   * to get ride of this is to check on the main output controls.
   */
  if(stm_display_output_get_control(i->hFBMainOutput, OUTPUT_CTRL_VIDEO_SOURCE_SELECT, &ctrlVal) < 0)
    return -ENODEV;

  switch(ctrlVal){
    case STM_VIDEO_SOURCE_MAIN_COMPOSITOR:
    case STM_VIDEO_SOURCE_MAIN_COMPOSITOR_BYPASS:
      /* Main Output (YPbPb or/and HDMI or/and DVO) */
      plane_caps_value = (stm_plane_capabilities_t)(PLANE_CAPS_GRAPHICS|PLANE_CAPS_GRAPHICS_BEST_QUALITY|PLANE_CAPS_PRIMARY_OUTPUT);
      /*
       * Mask for plane. Dont get a sharable GDP for Main display as
       * it would be possible that we need it for Aux output.
       */
      plane_caps_mask = (stm_plane_capabilities_t)(PLANE_CAPS_GRAPHICS|PLANE_CAPS_GRAPHICS_BEST_QUALITY|PLANE_CAPS_PRIMARY_OUTPUT|PLANE_CAPS_SECONDARY_OUTPUT);
      break;
    case STM_VIDEO_SOURCE_AUX_COMPOSITOR:
    case STM_VIDEO_SOURCE_AUX_COMPOSITOR_BYPASS:
      /* Aux Output */
      plane_caps_value = (stm_plane_capabilities_t)(PLANE_CAPS_GRAPHICS|PLANE_CAPS_SECONDARY_OUTPUT);
      /*
       * Try to get a dedicated or sharable GDP-Lite for Aux display.
       */
      plane_caps_mask = (stm_plane_capabilities_t)(PLANE_CAPS_GRAPHICS|PLANE_CAPS_GRAPHICS_BEST_QUALITY|PLANE_CAPS_SECONDARY_OUTPUT);
      break;
    default:
      DPRINTK( KERN_ERR "%s: failed to find an appropriate plane capabilities\n",__func__);
      return -ENODEV;
  }

  ret = stm_display_device_find_planes_with_capabilities(i->display_device, plane_caps_value, plane_caps_mask, &plane_id, 1);
  if(ret == 0)
  {
    /* No GDP-Lite available on this platform ?
     * Try to get GDP-Full for this fb device
     */
    plane_caps_value |= PLANE_CAPS_GRAPHICS_BEST_QUALITY;
    ret = stm_display_device_find_planes_with_capabilities(i->display_device, plane_caps_value, plane_caps_mask, &plane_id, 1);
  }

  if(ret != 1) {
    DPRINTK( KERN_ERR "%s: failed to find an appropriate plane\n",__func__);
    return -ENODEV;
  }

  ret = stm_display_device_open_plane(i->display_device, plane_id, &(i->hFBPlane));
  if(ret !=0) {
    DPRINTK( KERN_ERR "%s: failed to open the appropriate plane\n",__func__);
    return -ENODEV;
  }

  /*
   * Setup the plane z-position to be on top of all created planes.
   * Set depth to 255 in order to ensure this. If the provided depth value is
   * great then the mixer crossbar size then internally coredisplay will set
   * the depth value to the the mixer crossbar size.
   */
  ret = stm_display_plane_set_depth(i->hFBPlane, i->hFBMainOutput, 255, 1);
  if(ret<0)
  {
    DPRINTK( KERN_ERR "%s: failed to set the plane depth\n",__func__);
    return ret;
  }

  i->pFBCLUT = dma_alloc_coherent(&i->platformDevice->dev, sizeof(unsigned long)*256, &i->dmaFBCLUT, GFP_KERNEL);
  if(!i->pFBCLUT)
    return -ENOMEM;

  return 0;
}


/****************************************************************************
 * Platform device implementation
 */
static int __init stmfb_platformdev_init( void )
{
  int i;
  int res;
  int nvaliddevices=0;
  int nr_platform_devices=0;

  DPRINTK("\n");

  for (i = 0; i < NR_FRAMEBUFFERS; i++)
  {
    if(stmcore_get_display_pipeline(i, &platform_data[nr_platform_devices]) != 0)
      continue;
    nr_platform_devices++;
  }

  DPRINTK("stmfb device probe found %d display pipelines\n",nr_platform_devices);

  for(i=0;i<nr_platform_devices;i++)
  {
    struct platform_device               *platformDevice = NULL;
    struct stmcore_display_pipeline_data *pd   = &platform_data[i];

    /*
     * Create a platform device for this display pipeline
     */
    platformDevice = platform_device_alloc("stm-fb",i);
    if(IS_ERR_OR_NULL(platformDevice))
      continue;

    /*
     * Set the DMA address coherency mask for this device.
     */
    platformDevice->dev.coherent_dma_mask = DMA_BIT_MASK(32);

    /*
     * Note that adding the platform device data makes a shallow copy of
     * the _pointer_ to the platform structure in this case.
     */
    res = platform_device_add_data(platformDevice, &pd, sizeof(struct stmcore_display_pipeline_data *));
    if(res<0)
    {
      //platform_device_put(platformDevice);
      platformDevice = NULL;
      continue;
    }

    res = platform_device_add(platformDevice);
    if(res<0)
    {
       printk(KERN_ERR "Failed to add platform device %d\n",i);
       //platform_device_put(platformDevice);
       platformDevice = NULL;
       continue;
    }

    DPRINTK("device platform %d registred\n",nvaliddevices);
    nvaliddevices++;
  }

  if(nvaliddevices == 0)
  {
    printk(KERN_ERR "No valid display pipelines available\n");
    return -ENODEV;
  }

  return 0;
}

static void __exit stmfb_platformdev_exit( void )
{
  int i;

  DPRINTK("\n");

  for (i = 0; i < NR_FRAMEBUFFERS; i++)
  {
    struct stmfb_info *info = &stmfb_info[i];

    if(info->platformDevice)
    {
      platform_device_unregister(info->platformDevice);
      info->platformDevice = NULL;
    }
  }
}


static int stmfb_probe(struct platform_device *pdev)
{
  struct stmfb_info *i = (struct stmfb_info *)platform_get_drvdata(pdev);
  struct stmcore_display_pipeline_data *pd;
  int ret;

  DPRINTK("\n");

  if(i != NULL)
  {
    /*
     * If the device already has registered driver data, don't replace it
     * with the framebuffer.
     */
    return -EBUSY;
  }

  if(pdev->id < 0 || pdev->id >= NR_FRAMEBUFFERS)
    return -ENODEV;

  i  = &stmfb_info[pdev->id];
  pd = *((struct stmcore_display_pipeline_data **)pdev->dev.platform_data);

  if(!pd)
    return -ENODEV;

  if(!try_module_get(pd->owner))
    return -ENODEV;

  i->display_device_id = pd->device_id;

  if((ret = stm_display_open_device(i->display_device_id, &i->display_device))<0)
  {
    module_put(pd->owner);
    return ret;
  }

#ifdef CONFIG_PM_RUNTIME
  pm_runtime_set_active(&pdev->dev);
  //pm_suspend_ignore_children(&pdev->dev, 1);
  pm_runtime_enable(&pdev->dev);
#endif

  /* Setup framebuffer structure */
  sema_init(&i->framebufferLock,1);
  spin_lock_init(&i->framebufferSpinLock);
  init_waitqueue_head(&i->framebuffer_updated_wait_queue);
  i->opens = 0;
  i->fbdev_api_suspended = 0;
  i->platformDevice = pdev;

  if((ret = stmfb_probe_get_outputs(i,pd))<0)
  {
    stmfb_cleanup_info(i);
    module_put(pd->owner);
    return ret;
  }

  if((ret = stmfb_probe_get_plane(i,pd))<0)
  {
    stmfb_cleanup_info(i);
    module_put(pd->owner);
    return ret;
  }

  if((ret = stmfb_probe_get_source(i,pd))<0)
  {
    stmfb_cleanup_info(i);
    module_put(pd->owner);
    return ret;
  }

  if ((ret = stmfb_createfb(i, pdev->id, pd->name)) < 0)
  {
    stmfb_cleanup_info(i);
    module_put(pd->owner);

    /*
     * stmfb_createfb makes a lot of calls to the core driver which can be
     * interrupted by a signal, checking for this here keeps the code cleaner.
     */
    if(signal_pending(current))
      return -EINTR;
    else
      return ret;
  }

  /*
   * Hook info structure into the platform device's driver data.
   */
  platform_set_drvdata(pdev,i);

  DPRINTK("probe finished\n");

#ifdef CONFIG_PM_RUNTIME
  pm_runtime_suspend(&pdev->dev);
#endif

  return 0;
}


static int __exit stmfb_remove(struct platform_device *pdev)
{
  int res = 0;
  mm_segment_t oldfs;
  struct stmfb_info *i = (struct stmfb_info *)platform_get_drvdata(pdev);
  struct stmcore_display_pipeline_data *pd = *((struct stmcore_display_pipeline_data **)pdev->dev.platform_data);

  DPRINTK("\n");

  if(!i)
    return 0;

  if(!i->display_device)
  {
    if((res = stm_display_open_device(i->display_device_id, &i->display_device))<0)
    {
      printk(KERN_ERR "unable to open display device\n");
      return res;
    }
  }

  if(!signal_pending(current) && i->hdmi_fops && i->hdmi_fops->unlocked_ioctl)
  {
    oldfs = get_fs();

    /*
     * It is important to ignore errors other than when a signal was delivered,
     * as if the HDMI device isn't in a state to restart we do not actually
     * care in this code. We need to ensure that HDCP is disabled via this control
     * before stopping output.
     */
    set_fs(KERNEL_DS);
    if(i->hdmi_fops->unlocked_ioctl(&i->hdmi_dev, STMHDMIIO_SET_DISABLED, 1) == -ERESTARTSYS)
      res = -ERESTARTSYS;
    set_fs(oldfs);

  }

#ifdef CONFIG_PM_RUNTIME
  pm_runtime_disable(&pdev->dev);
#endif

  stmfb_destroyfb(i);
  stmfb_cleanup_info(i);

  platform_set_drvdata(pdev,NULL);

  module_put(pd->owner);

  return res;
}


static void stmfb_shutdown(struct platform_device *pdev)
{
  struct stmfb_info *i = (struct stmfb_info *)platform_get_drvdata(pdev);
  if(!i)
    return;

  return;
}


static int stmfb_set_power_state(struct platform_device *pdev, pm_message_t state)
{
  int retval = 0;
  struct stmfb_info *i = (struct stmfb_info *)platform_get_drvdata(pdev);

  switch(state.event) {
    case PM_EVENT_RESTORE:
    case PM_EVENT_RESUME:
      {
        DPRINTK(" framebuffer fb%d is %s ...\n", pdev->id, (state.event == PM_EVENT_RESUME) ? "Resuming" : "Restoring");

        /*
         * Kick the framebuffer into life
         *
         * We don't lock the framebuffer here as the fb_set_suspend callbacks
         * might call back into the driver.
        */
        if(!i->platformDevice)
          break;

        if(!i->display_device)
        {
          if((retval = stm_display_open_device(i->display_device_id, &i->display_device))<0)
            return retval;
        }

        fb_pan_display(&i->info, &i->info.var);
        fb_set_cmap(&i->info.cmap, &i->info);
        fb_set_suspend(&i->info, 0);

        DPRINTK( " 0K.\n");
      }
      break;
    case PM_EVENT_FREEZE:
    case PM_EVENT_SUSPEND:
      {
        DPRINTK(" framebuffer fb%d is %s ...\n", pdev->id, (state.event == PM_EVENT_SUSPEND) ? "Suspending" : "Freezing");

        /*
         * Kick off the framebuffer devices
         *
         * We don't lock the framebuffer here as the fb_set_suspend callbacks
         * might call back into the driver.
         */
        if(!i->platformDevice)
          break;

        fb_set_suspend(&i->info, 0);
        fb_set_suspend(&i->info, 1);

        if(i->display_device)
        {
          stm_display_device_close(i->display_device);
          i->display_device = NULL;
        }

        DPRINTK(" 0K.\n");
      }
      break;
    case PM_EVENT_ON:
    case PM_EVENT_THAW:
    default :
      DPRINTK(" Unsupported PM event!\n");
      retval = -EINVAL;
    break;
  }

  return retval;
}

static int stmfb_suspend(struct device *dev)
{
  struct platform_device *pdev = to_platform_device(dev);

  return stmfb_set_power_state(pdev,PMSG_SUSPEND);
}

static int stmfb_resume(struct device *dev)
{
  struct platform_device *pdev = to_platform_device(dev);

  return stmfb_set_power_state(pdev,PMSG_RESUME);
}

const struct dev_pm_ops stmfb_pm_ops = {
  .suspend         = stmfb_suspend,
  .resume          = stmfb_resume,
  .freeze          = stmfb_suspend,
  .thaw            = stmfb_resume,
  .poweroff        = stmfb_suspend,
  .restore         = stmfb_resume,
#ifdef CONFIG_PM_RUNTIME
  .runtime_suspend = stmfb_suspend,
  .runtime_resume  = stmfb_resume,
  .runtime_idle    = NULL,
#endif
};


static struct platform_driver stmfb_driver = {
   .probe    = stmfb_probe,
   .shutdown = stmfb_shutdown,
   .remove   = __exit_p(stmfb_remove),
   .driver   = {
       .pm       = &stmfb_pm_ops,
       .name     = "stm-fb",
       .owner    = THIS_MODULE
   }
};


static void __exit stmfb_exit(void)
{
  platform_driver_unregister(&stmfb_driver);
  stmfb_platformdev_exit();
}


static int __init stmfb_init(void)
{
  memset(stmfb_info, 0, sizeof(stmfb_info));
  memset(platform_data, 0, sizeof(platform_data));

  platform_driver_register(&stmfb_driver);
  stmfb_platformdev_init();

  return 0;
}


/******************************************************************************
 *  Modularization
 */

module_init(stmfb_init);
module_exit(stmfb_exit);

MODULE_LICENSE("GPL");
