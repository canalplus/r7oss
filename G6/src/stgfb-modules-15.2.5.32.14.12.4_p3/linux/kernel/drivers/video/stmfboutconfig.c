/***********************************************************************
 *
 * File: linux/kernel/drivers/video/stmfboutconfig.c
 * Copyright (c) 2007 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <linux/version.h>
#include <linux/fb.h>
#include <asm/uaccess.h>

#include <stm_display.h>

#include "stmfb.h"
#include "stmfbinfo.h"
#include "linux/kernel/drivers/stm/hdmi/stmhdmi.h"

void
stmfb_initialise_output_config (stm_display_output_h                        out,
                                struct stmfbio_output_configuration * const config)
{
  uint32_t caps;
  uint32_t tmp;
  uint32_t powerdowndacs=0;

  config->activate = STMFBIO_ACTIVATE_ON_NEXT_CHANGE;

  if(stm_display_output_get_capabilities(out,&caps)<0)
  {
    DPRINTK("Failed to get output caps\n");
    return;
  }

  if(caps & OUTPUT_CAPS_MIXER_BACKGROUND)
  {
    config->caps |= STMFBIO_OUTPUT_CAPS_MIXER_BACKGROUND;
    stm_display_output_get_control(out, OUTPUT_CTRL_BACKGROUND_ARGB, &tmp);
    config->mixer_background = tmp;
  }

  if(stm_display_output_get_control(out, OUTPUT_CTRL_DAC_POWER_DOWN, &powerdowndacs)<0)
  {
    printk(KERN_WARNING "Failed to get video dacs power status !!\n");
    return;
  }

  if(powerdowndacs)
  {
    DPRINTK("Video DACs are powered down !!\n");
    return;
  }

  config->caps = STMFBIO_OUTPUT_CAPS_ANALOGUE_CONFIG;

  if((caps & (OUTPUT_CAPS_SD_ANALOG | OUTPUT_CAPS_DISPLAY_TIMING_MASTER)) == (OUTPUT_CAPS_SD_ANALOG | OUTPUT_CAPS_DISPLAY_TIMING_MASTER))
  {
    config->caps |= STMFBIO_OUTPUT_CAPS_SDTV_ENCODING;
  }

  config->analogue_config = STMFBIO_OUTPUT_ANALOGUE_CLIP_VIDEORANGE;

  if(stm_display_output_get_control(out, OUTPUT_CTRL_VIDEO_OUT_SELECT, &tmp)<0)
  {
    DPRINTK("Failed to get video output select control\n");
    return;
  }

  /*
   * The video out select defines are the same as the output signals
   * configuration define for the analogue outputs.
   */
  config->analogue_config = (tmp & STMFBIO_OUTPUT_ANALOGUE_MASK);

  if(caps & OUTPUT_CAPS_SD_ANALOG)
  {
    config->caps |= (STMFBIO_OUTPUT_CAPS_BRIGHTNESS |
                     STMFBIO_OUTPUT_CAPS_SATURATION |
                     STMFBIO_OUTPUT_CAPS_CONTRAST   |
                     STMFBIO_OUTPUT_CAPS_HUE);

    stm_display_output_get_control(out, OUTPUT_CTRL_BRIGHTNESS, &tmp);
    config->brightness = tmp;
    stm_display_output_get_control(out, OUTPUT_CTRL_SATURATION, &tmp);
    config->saturation = tmp;
    stm_display_output_get_control(out, OUTPUT_CTRL_CONTRAST, &tmp);
    config->contrast = tmp;
    stm_display_output_get_control(out, OUTPUT_CTRL_HUE, &tmp);
    config->hue = tmp;
  }

  /*
   * Set the default analogue output to be limited to the standard video range
   * where the hardware can enfore that.
   */
  stm_display_output_set_control(out, OUTPUT_CTRL_CLIP_SIGNAL_RANGE, STM_SIGNAL_VIDEO_RANGE);
}


int
stmfb_get_output_configuration (struct stmfbio_output_configuration * const c,
                                struct stmfb_info                   * const i)
{
  switch(c->outputid)
  {
    case STMFBIO_OUTPUTID_MAIN:
    {
      /*
       * First, update the current HDMI config from the HDMI device if we do
       * not have a pending update.
       */
      if(i->hdmi_fops && (i->main_config.activate != STMFBIO_ACTIVATE_ON_NEXT_CHANGE))
      {
        int ret;
        __u32 disabled;
        mm_segment_t oldfs = get_fs();
        set_fs(KERNEL_DS);
        ret = i->hdmi_fops->unlocked_ioctl(&i->hdmi_dev, STMHDMIIO_GET_DISABLED, (unsigned long)&disabled);
        set_fs(oldfs);

        if(ret < 0)
          return (ret == -EINTR && signal_pending(current))?-ERESTARTSYS:ret;

        i->main_config.hdmi_config &= ~(STMFBIO_OUTPUT_HDMI_ENABLED | STMFBIO_OUTPUT_HDMI_DISABLED);
        i->main_config.hdmi_config |= (disabled == 0)?STMFBIO_OUTPUT_HDMI_ENABLED:STMFBIO_OUTPUT_HDMI_DISABLED;
      }
      DPRINTK("returning main configuration (address = %p)\n",&i->main_config);
      *c = i->main_config;
      break;
    }
    default:
      DPRINTK("invalid output id = %d\n",(int)c->outputid);
      return -EINVAL;
  }

  return 0;
}


static void stmfbio_set_sdtv_encoding(struct stmfb_info *i,
                                      stm_display_output_h o,
                                      uint32_t outputcaps,
                                      struct stmfbio_output_configuration *c,
                                      struct stmfbio_output_configuration *newconfig)
{
  __u32 failed = 0;

  DPRINTK("\n");

  switch(c->sdtv_encoding)
  {
    case STMFBIO_OUTPUT_STD_PAL_BDGHI:
    case STMFBIO_OUTPUT_STD_PAL_M:
    case STMFBIO_OUTPUT_STD_PAL_N:
    case STMFBIO_OUTPUT_STD_PAL_Nc:
    case STMFBIO_OUTPUT_STD_NTSC_M:
    case STMFBIO_OUTPUT_STD_NTSC_J:
    case STMFBIO_OUTPUT_STD_NTSC_443:
    case STMFBIO_OUTPUT_STD_SECAM:
    case STMFBIO_OUTPUT_STD_PAL_60:
      break;
    default:
      failed = STMFBIO_OUTPUT_CAPS_SDTV_ENCODING;
      DPRINTK("Invalid SD TV Encoding\n");
  }

  if(!(outputcaps & OUTPUT_CAPS_SD_ANALOG))
  {
    failed = STMFBIO_OUTPUT_CAPS_SDTV_ENCODING;
    DPRINTK("Output doesn't support changing SD analogue standard\n");
  }

  if((failed == 0) && (c->activate == STMFBIO_ACTIVATE_ON_NEXT_CHANGE))
    newconfig->sdtv_encoding = c->sdtv_encoding;

  c->failed |= failed;

  if((failed != 0) || c->activate != STMFBIO_ACTIVATE_IMMEDIATE)
    return;

  if(i->current_videomode_valid)
  {
    stm_display_mode_t new_mode = i->current_videomode;
    stm_display_mode_t mode;

    /* We must make sure to not have an invalid standard description
       in the mode we are setting. This could happen, since
       sdtv_encoding might e.g. still contain the previous SD standard,
       which could be incompatible with the new requested mode. */
    if (stm_display_output_get_display_mode(o,new_mode.mode_id,
                                            &mode)==0)
    {
      uint32_t newstandard = c->sdtv_encoding;

      if(new_mode.mode_params.output_standards & STM_OUTPUT_STD_SMPTE293M)
        newstandard |= STM_OUTPUT_STD_SMPTE293M;

      newstandard &= mode.mode_params.output_standards;
      new_mode.mode_params.output_standards = newstandard;

      if(stm_display_output_start(o,&new_mode)<0)
        failed = STMFBIO_OUTPUT_CAPS_SDTV_ENCODING;
      else
        i->current_videomode = new_mode;
    }
    else
      failed = STMFBIO_OUTPUT_CAPS_SDTV_ENCODING;
  }

  if(failed == 0)
    newconfig->sdtv_encoding = c->sdtv_encoding;
  else
    c->failed |= failed;
}


static void stmfbio_set_analogue_config(struct stmfb_info *i,
                                       stm_display_output_h o,
                                       uint32_t outputcaps,
                                       struct stmfbio_output_configuration *c,
                                       struct stmfbio_output_configuration *newconfig)
{
  __u32 failed = 0;
  __u32 tmp;
  stm_display_signal_range_t range;
  stm_ycbcr_colorspace_t colorspace;

  DPRINTK(": outcaps = 0x%08x analogue config = 0x%08x\n",outputcaps,c->analogue_config);

  /*
   * Component YPrPb and RGB (SCART) are mutually exclusive
   */
  tmp = (STMFBIO_OUTPUT_ANALOGUE_YPrPb|STMFBIO_OUTPUT_ANALOGUE_RGB);
  if((c->analogue_config & tmp) == tmp)
  {
    DPRINTK(": RGB and YPrPb are mutually exclusive\n");
    failed = STMFBIO_OUTPUT_CAPS_ANALOGUE_CONFIG;
  }

  if(c->analogue_config & STMFBIO_OUTPUT_ANALOGUE_RGB)
  {
    if(!(outputcaps & (OUTPUT_CAPS_SD_RGB_CVBS_YC | OUTPUT_CAPS_RGB_EXCLUSIVE)))
    {
      DPRINTK(": RGB not supported\n");
      failed = STMFBIO_OUTPUT_CAPS_ANALOGUE_CONFIG;
    }
    else if(((c->analogue_config & STMFBIO_OUTPUT_ANALOGUE_MASK) != STMFBIO_OUTPUT_ANALOGUE_RGB) &&
            !(outputcaps & OUTPUT_CAPS_SD_RGB_CVBS_YC))
    {
      DPRINTK(": RGB+CVBS/YC (SCART) not supported\n");
      failed = STMFBIO_OUTPUT_CAPS_ANALOGUE_CONFIG;
    }
  }

  if(c->analogue_config & STMFBIO_OUTPUT_ANALOGUE_YPrPb)
  {
    if(!(outputcaps & (OUTPUT_CAPS_SD_YPbPr_CVBS_YC | OUTPUT_CAPS_YPbPr_EXCLUSIVE)))
    {
      DPRINTK(": YPrPb not supported\n");
      failed = STMFBIO_OUTPUT_CAPS_ANALOGUE_CONFIG;
    }
    else if(((c->analogue_config & STMFBIO_OUTPUT_ANALOGUE_MASK) != STMFBIO_OUTPUT_ANALOGUE_YPrPb) &&
            !(outputcaps & OUTPUT_CAPS_SD_YPbPr_CVBS_YC))
    {
      DPRINTK(": YPrPb+CVBS/YC not supported\n");
      failed = STMFBIO_OUTPUT_CAPS_ANALOGUE_CONFIG;
    }
  }

  tmp = (OUTPUT_CAPS_CVBS_YC_EXCLUSIVE |
         OUTPUT_CAPS_SD_RGB_CVBS_YC    |
         OUTPUT_CAPS_SD_YPbPr_CVBS_YC);

  if((c->analogue_config & (STMFBIO_OUTPUT_ANALOGUE_CVBS|STMFBIO_OUTPUT_ANALOGUE_YC)) && !(outputcaps & tmp))
  {
    DPRINTK(": CVBS and Y/C unavailable\n");
    failed = STMFBIO_OUTPUT_CAPS_ANALOGUE_CONFIG;
  }

  if((failed == 0) && (c->activate == STMFBIO_ACTIVATE_ON_NEXT_CHANGE))
    newconfig->analogue_config = c->analogue_config;

  c->failed |= failed;

  if((failed != 0) || c->activate != STMFBIO_ACTIVATE_IMMEDIATE)
    return;

  /*
   * Note that the STMFBIO_OUTPUT_SIGNAL_ defines have been deliberately made
   * the same as the STM_VIDEO_OUT_ defines for analogue signals.
   */
  tmp = c->analogue_config & STMFBIO_OUTPUT_ANALOGUE_MASK;

  if(stm_display_output_set_control(o, OUTPUT_CTRL_VIDEO_OUT_SELECT, tmp)<0)
    failed = STMFBIO_OUTPUT_CAPS_ANALOGUE_CONFIG;

  range = (c->analogue_config & STMFBIO_OUTPUT_ANALOGUE_CLIP_FULLRANGE)?STM_SIGNAL_FULL_RANGE:STM_SIGNAL_VIDEO_RANGE;
  if(stm_display_output_set_control(o, OUTPUT_CTRL_CLIP_SIGNAL_RANGE, range)<0)
    failed = STMFBIO_OUTPUT_CAPS_ANALOGUE_CONFIG;

  switch(c->analogue_config & STMFBIO_OUTPUT_ANALOGUE_COLORSPACE_MASK)
  {
    case STMFBIO_OUTPUT_ANALOGUE_COLORSPACE_AUTO:
      colorspace = STM_YCBCR_COLORSPACE_AUTO_SELECT;
      break;
    case STMFBIO_OUTPUT_ANALOGUE_COLORSPACE_601:
      colorspace = STM_YCBCR_COLORSPACE_601;
      break;
    case STMFBIO_OUTPUT_ANALOGUE_COLORSPACE_709:
      colorspace = STM_YCBCR_COLORSPACE_709;
      break;
    default:
      failed = STMFBIO_OUTPUT_CAPS_ANALOGUE_CONFIG;
      break;
  }

  if(failed == 0)
  {
    if(stm_display_output_set_control(o, OUTPUT_CTRL_YCBCR_COLORSPACE, colorspace)<0)
      failed = STMFBIO_OUTPUT_CAPS_ANALOGUE_CONFIG;
  }

  if(failed == 0)
    newconfig->analogue_config = c->analogue_config;
  else
    c->failed |= failed;
}


static void stmfbio_set_dvo_config(struct stmfb_info *i,
                                    struct stmfbio_output_configuration *c,
                                    struct stmfbio_output_configuration *newconfig)
{
  __u32 failed = 0;
  __u32 format = 0;
  stm_display_signal_range_t range;
  __u32 sync   = 0;
  __u32 filter = 0;
  __u32 inv_clk = 0;

  DPRINTK(": dvo_config = 0x%08x\n",c->dvo_config);

  if(!i->hFBDVO)
    failed = STMFBIO_OUTPUT_CAPS_DVO_CONFIG;

  if((failed == 0) && (c->activate == STMFBIO_ACTIVATE_ON_NEXT_CHANGE))
    newconfig->dvo_config = c->dvo_config;

  c->failed |= failed;

  if((failed != 0) || (c->activate != STMFBIO_ACTIVATE_IMMEDIATE))
    return;

  if(i->current_videomode_valid)
  {
    int ret;
    /*
     * If the display is running, just do a start or stop. It doesn't matter
     * if they are called when already in the correct state. We only take note
     * of the error return if a signal is pending.
     */
    if(!(c->dvo_config & STMFBIO_OUTPUT_DVO_DISABLED))
      ret = stm_display_output_start(i->hFBDVO, &i->current_videomode);
    else
      ret = stm_display_output_stop(i->hFBDVO);

    if(ret<0 && signal_pending(current))
      failed = STMFBIO_OUTPUT_CAPS_DVO_CONFIG;
  }

  switch(c->dvo_config & STMFBIO_OUTPUT_DVO_MODE_MASK)
  {
    case STMFBIO_OUTPUT_DVO_YUV_444_16BIT:
      format = (STM_VIDEO_OUT_YUV | STM_VIDEO_OUT_444 | STM_VIDEO_OUT_16BIT);
      break;
    case STMFBIO_OUTPUT_DVO_YUV_444_24BIT:
      format = (STM_VIDEO_OUT_YUV | STM_VIDEO_OUT_444 | STM_VIDEO_OUT_24BIT);
      break;
    case STMFBIO_OUTPUT_DVO_YUV_422_16BIT:
      format = (STM_VIDEO_OUT_YUV | STM_VIDEO_OUT_422 | STM_VIDEO_OUT_16BIT);
      break;
    case STMFBIO_OUTPUT_DVO_ITUR656:
      format = (STM_VIDEO_OUT_YUV | STM_VIDEO_OUT_ITUR656);
      break;
    case STMFBIO_OUTPUT_DVO_RGB_24BIT:
      format = (STM_VIDEO_OUT_RGB | STM_VIDEO_OUT_24BIT);
      break;
  }

  if(stm_display_output_set_control(i->hFBDVO, OUTPUT_CTRL_VIDEO_OUT_SELECT,format)<0)
    failed = STMFBIO_OUTPUT_CAPS_DVO_CONFIG;

  sync = (c->dvo_config & STMFBIO_OUTPUT_DVO_EMBEDDED_SYNC)?1:0;
  if(stm_display_output_set_control(i->hFBDVO, OUTPUT_CTRL_DVO_ALLOW_EMBEDDED_SYNCS, sync)<0)
    failed = STMFBIO_OUTPUT_CAPS_DVO_CONFIG;

  inv_clk = (c->dvo_config & STMFBIO_OUTPUT_DVO_INVERT_DATA_CLOCK)?1:0;
  if(stm_display_output_set_control(i->hFBDVO, OUTPUT_CTRL_DVO_INVERT_DATA_CLOCK, inv_clk)<0)
    failed = STMFBIO_OUTPUT_CAPS_DVO_CONFIG;

  range = (c->dvo_config & STMFBIO_OUTPUT_DVO_CLIP_FULLRANGE)?STM_SIGNAL_FILTER_SAV_EAV:STM_SIGNAL_VIDEO_RANGE;
  if(stm_display_output_set_control(i->hFBDVO, OUTPUT_CTRL_CLIP_SIGNAL_RANGE, range)<0)
    failed = STMFBIO_OUTPUT_CAPS_DVO_CONFIG;

  filter = (c->dvo_config & STMFBIO_OUTPUT_DVO_CHROMA_FILTER_ENABLED)?1:0;
  if(stm_display_output_set_control(i->hFBDVO, OUTPUT_CTRL_422_CHROMA_FILTER, filter)<0)
    failed = STMFBIO_OUTPUT_CAPS_DVO_CONFIG;

  if(failed == 0)
    newconfig->dvo_config = c->dvo_config;
  else
    c->failed |= failed;
}


static void stmfbio_set_hdmi_config(struct stmfb_info *i,
                                    struct stmfbio_output_configuration *c,
                                    struct stmfbio_output_configuration *newconfig)
{
  __u32 failed = 0;
  __u32 video  = 0;
  int hdmidisable;
  stm_display_signal_range_t range;
  mm_segment_t oldfs = get_fs();

  DPRINTK(": hdmi_config = 0x%08x\n",c->hdmi_config);

  if((i->hFBHDMI == NULL) || (i->hdmi_fops == NULL))
    failed = STMFBIO_OUTPUT_CAPS_HDMI_CONFIG;
  else if(((c->hdmi_config & STMFBIO_OUTPUT_HDMI_COLOURDEPTH_MASK) != STMFBIO_OUTPUT_HDMI_COLOURDEPTH_24BIT))
  {
    uint32_t caps;

    if((stm_display_output_get_capabilities(i->hFBHDMI, &caps)<0) ||
       ((caps & OUTPUT_CAPS_HDMI_DEEPCOLOR) == 0))
    {
      DPRINTK(": No deepcolour support available\n");
      failed = STMFBIO_OUTPUT_CAPS_HDMI_CONFIG;
    }
  }

  if((c->hdmi_config & STMFBIO_OUTPUT_HDMI_YUV) &&
     (c->hdmi_config & STMFBIO_OUTPUT_HDMI_422) &&
     (c->hdmi_config & STMFBIO_OUTPUT_HDMI_COLOURDEPTH_MASK) != STMFBIO_OUTPUT_HDMI_COLOURDEPTH_24BIT)
  {
    DPRINTK(": Deepcolour not valid in YUV 422 output mode\n");
    failed = STMFBIO_OUTPUT_CAPS_HDMI_CONFIG;
  }

  if((failed == 0) && (c->activate == STMFBIO_ACTIVATE_ON_NEXT_CHANGE))
    newconfig->hdmi_config = c->hdmi_config;

  c->failed |= failed;

  if((failed != 0) || (c->activate != STMFBIO_ACTIVATE_IMMEDIATE))
    return;

  if(c->hdmi_config & STMFBIO_OUTPUT_HDMI_YUV)
  {
    video |= STM_VIDEO_OUT_YUV;
    video |= (c->hdmi_config & STMFBIO_OUTPUT_HDMI_422)?STM_VIDEO_OUT_422:STM_VIDEO_OUT_444;
  }
  else
  {
    video |= STM_VIDEO_OUT_RGB;
  }

  switch(c->hdmi_config & STMFBIO_OUTPUT_HDMI_COLOURDEPTH_MASK)
  {
    case STMFBIO_OUTPUT_HDMI_COLOURDEPTH_30BIT:
      video |= STM_VIDEO_OUT_30BIT;
      break;
    case STMFBIO_OUTPUT_HDMI_COLOURDEPTH_36BIT:
      video |= STM_VIDEO_OUT_36BIT;
      break;
    case STMFBIO_OUTPUT_HDMI_COLOURDEPTH_48BIT:
      video |= STM_VIDEO_OUT_48BIT;
      break;
    default:
      video |= STM_VIDEO_OUT_24BIT;
      break;
  }

  set_fs(KERNEL_DS);
  if(i->hdmi_fops->unlocked_ioctl(&i->hdmi_dev, STMHDMIIO_SET_VIDEO_FORMAT, video)<0)
    failed = STMFBIO_OUTPUT_CAPS_HDMI_CONFIG;

  hdmidisable = (c->hdmi_config & STMFBIO_OUTPUT_HDMI_DISABLED)?1:0;
  if(i->hdmi_fops->unlocked_ioctl(&i->hdmi_dev, STMHDMIIO_SET_DISABLED, hdmidisable)<0)
    failed = STMFBIO_OUTPUT_CAPS_HDMI_CONFIG;
  set_fs(oldfs);

  range = (c->hdmi_config & STMFBIO_OUTPUT_HDMI_CLIP_FULLRANGE)?STM_SIGNAL_FULL_RANGE:STM_SIGNAL_VIDEO_RANGE;
  if(stm_display_output_set_control(i->hFBHDMI, OUTPUT_CTRL_CLIP_SIGNAL_RANGE, range)<0)
    failed = STMFBIO_OUTPUT_CAPS_HDMI_CONFIG;

  if(failed == 0)
    newconfig->hdmi_config = c->hdmi_config;
  else
    c->failed |= failed;

}


static void stmfbio_set_mixer_background(stm_display_output_h o,
                                         uint32_t  outcaps,
                                         struct stmfbio_output_configuration *c,
                                         struct stmfbio_output_configuration *newconfig)
{
  __u32 failed = 0;

  DPRINTK("\n");

  if(!(outcaps & OUTPUT_CAPS_MIXER_BACKGROUND))
    failed = STMFBIO_OUTPUT_CAPS_MIXER_BACKGROUND;

  if((failed == 0) && (c->activate == STMFBIO_ACTIVATE_ON_NEXT_CHANGE))
    newconfig->mixer_background = c->mixer_background;

  c->failed |= failed;

  if((failed != 0) || (c->activate != STMFBIO_ACTIVATE_IMMEDIATE))
    return;

  if(stm_display_output_set_control(o, OUTPUT_CTRL_BACKGROUND_ARGB, c->mixer_background)<0)
  {
    failed = STMFBIO_OUTPUT_CAPS_MIXER_BACKGROUND;
  }

  if(failed == 0)
    newconfig->mixer_background = c->mixer_background;
  else
    c->failed |= failed;

}


static void stmfbio_set_psi(stm_display_output_h o,
                            uint32_t  outcaps,
                            struct stmfbio_output_configuration *c,
                            struct stmfbio_output_configuration *newconfig)
{
  __u32 failed = 0;

  DPRINTK("\n");

  if(!(outcaps & OUTPUT_CAPS_SD_ANALOG) && (c->caps & STMFBIO_OUTPUT_CAPS_BRIGHTNESS))
    failed |= STMFBIO_OUTPUT_CAPS_BRIGHTNESS;

  if(!(outcaps & OUTPUT_CAPS_SD_ANALOG) && (c->caps & STMFBIO_OUTPUT_CAPS_SATURATION))
    failed |= STMFBIO_OUTPUT_CAPS_SATURATION;

  if(!(outcaps & OUTPUT_CAPS_SD_ANALOG) && (c->caps & STMFBIO_OUTPUT_CAPS_CONTRAST))
    failed |= STMFBIO_OUTPUT_CAPS_CONTRAST;

  if(!(outcaps & OUTPUT_CAPS_SD_ANALOG) && (c->caps & STMFBIO_OUTPUT_CAPS_HUE))
    failed |= STMFBIO_OUTPUT_CAPS_HUE;

  if((failed == 0) && (c->activate == STMFBIO_ACTIVATE_ON_NEXT_CHANGE))
  {
    if(c->caps & STMFBIO_OUTPUT_CAPS_BRIGHTNESS)
      newconfig->brightness = c->brightness;

    if(c->caps & STMFBIO_OUTPUT_CAPS_SATURATION)
      newconfig->saturation = c->saturation;

    if(c->caps & STMFBIO_OUTPUT_CAPS_CONTRAST)
      newconfig->contrast = c->contrast;

    if(c->caps & STMFBIO_OUTPUT_CAPS_HUE)
      newconfig->hue = c->hue;
  }

  c->failed |= failed;

  if((failed != 0) || (c->activate != STMFBIO_ACTIVATE_IMMEDIATE))
    return;

  if(c->caps & STMFBIO_OUTPUT_CAPS_BRIGHTNESS)
  {
    DPRINTK("brightness = %u\n",(unsigned)c->brightness);

    if(stm_display_output_set_control(o, OUTPUT_CTRL_BRIGHTNESS, c->brightness)<0)
      failed |= STMFBIO_OUTPUT_CAPS_BRIGHTNESS;
    else
      newconfig->brightness = c->brightness;
  }

  if(c->caps & STMFBIO_OUTPUT_CAPS_SATURATION)
  {
    DPRINTK("saturation = %u\n",(unsigned)c->saturation);

    if(stm_display_output_set_control(o, OUTPUT_CTRL_SATURATION, c->saturation)<0)
      failed |= STMFBIO_OUTPUT_CAPS_SATURATION;
    else
      newconfig->saturation = c->saturation;
  }

  if(c->caps & STMFBIO_OUTPUT_CAPS_CONTRAST)
  {
    DPRINTK("contrast = %u\n",(unsigned)c->contrast);

    if(stm_display_output_set_control(o, OUTPUT_CTRL_CONTRAST, c->contrast)<0)
      failed |= STMFBIO_OUTPUT_CAPS_CONTRAST;
    else
      newconfig->contrast = c->contrast;
  }

  if(c->caps & STMFBIO_OUTPUT_CAPS_HUE)
  {
    DPRINTK("hue = %u\n",(unsigned)c->hue);

    if(stm_display_output_set_control(o, OUTPUT_CTRL_HUE, c->hue)<0)
      failed |= STMFBIO_OUTPUT_CAPS_HUE;
    else
      newconfig->hue = c->hue;
  }

  c->failed |= failed;

}


int
stmfb_set_output_configuration (struct stmfbio_output_configuration * const c,
                                struct stmfb_info                   * const i)
{
  stm_display_output_h o = NULL;
  struct stmfbio_output_configuration *newconfig = NULL;
  uint32_t outcaps;
  int ret = 0;

  c->failed = 0;

  switch(c->outputid)
  {
    case STMFBIO_OUTPUTID_MAIN:
      DPRINTK("Updating main output configuration\n");

      o = i->hFBMainOutput;
      newconfig = &i->main_config;
      break;
    default:
      DPRINTK("Invalid output configuration id %d\n",(int)c->outputid);
      break;
  }

  if(!o)
    return -EINVAL;


  if((ret = stm_display_output_get_capabilities(o,&outcaps))<0)
  {
    DPRINTK("Cannot get output capabilities\n");
    return ret;
  }

  if(c->caps & STMFBIO_OUTPUT_CAPS_SDTV_ENCODING)
    stmfbio_set_sdtv_encoding(i, o, outcaps, c, newconfig);

  if(c->caps & STMFBIO_OUTPUT_CAPS_ANALOGUE_CONFIG)
    stmfbio_set_analogue_config(i, o, outcaps, c, newconfig);

  if(c->caps & STMFBIO_OUTPUT_CAPS_HDMI_CONFIG)
    stmfbio_set_hdmi_config(i, c, newconfig);

  if(c->caps & STMFBIO_OUTPUT_CAPS_DVO_CONFIG)
    stmfbio_set_dvo_config(i, c, newconfig);

  if(c->caps & STMFBIO_OUTPUT_CAPS_MIXER_BACKGROUND)
    stmfbio_set_mixer_background(o, outcaps, c, newconfig);

  if(c->caps & STMFBIO_OUTPUT_CAPS_PSI_MASK)
    stmfbio_set_psi(o, outcaps, c, newconfig);

  if(c->failed)
  {
    DPRINTK("Failed mask = 0x%08x\n",c->failed);
    if(signal_pending(current))
      return -EINTR;
    else
      return -EINVAL;
  }

  return 0;
}
