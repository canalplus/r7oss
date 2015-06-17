/***********************************************************************
 *
 * File: linux/drivers/video/stmfboutconfig.c
 * Copyright (c) 2007 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <linux/version.h>
#include <linux/fb.h>

#include <stmdisplay.h>

#include "stmfb.h"
#include "stmfbinfo.h"

void
stmfb_initialise_output_config (stm_display_output_t                * const out,
                                struct stmfbio_output_configuration * const config)
{
  ULONG caps;
  ULONG ctrlcaps;
  ULONG tmp;

  config->activate = STMFBIO_ACTIVATE_ON_NEXT_CHANGE;
  config->analogue_config = STMFBIO_OUTPUT_ANALOGUE_CLIP_VIDEORANGE;

  if(stm_display_output_get_capabilities(out,&caps)<0)
  {
    DPRINTK("Failed to get output caps\n");
    return;
  }

  if(stm_display_output_get_control_capabilities(out,&ctrlcaps)<0)
  {
    DPRINTK("Failed to get output control caps\n");
    return;
  }

  config->caps = STMFBIO_OUTPUT_CAPS_ANALOGUE_CONFIG;

  if((caps & (STM_OUTPUT_CAPS_SD_ANALOGUE | STM_OUTPUT_CAPS_MODE_CHANGE)) == (STM_OUTPUT_CAPS_SD_ANALOGUE | STM_OUTPUT_CAPS_MODE_CHANGE))
  {
    config->caps |= STMFBIO_OUTPUT_CAPS_SDTV_ENCODING;
  }

  if(stm_display_output_get_control(out, STM_CTRL_VIDEO_OUT_SELECT, &tmp)<0)
  {
    DPRINTK("Failed to get video output select control\n");
    return;
  }

  /*
   * The video out select defines are the same as the output signals
   * configuration define for the analogue outputs.
   */
  config->analogue_config = (tmp & STMFBIO_OUTPUT_ANALOGUE_MASK);

  if(ctrlcaps & STM_CTRL_CAPS_BACKGROUND)
  {
    config->caps |= STMFBIO_OUTPUT_CAPS_MIXER_BACKGROUND;
    stm_display_output_get_control(out, STM_CTRL_BACKGROUND_ARGB, &tmp);
    config->mixer_background = tmp;
  }

  if(ctrlcaps & STM_CTRL_CAPS_BRIGHTNESS)
  {
    config->caps |= STMFBIO_OUTPUT_CAPS_BRIGHTNESS;
    stm_display_output_get_control(out, STM_CTRL_BRIGHTNESS, &tmp);
    config->brightness = tmp;
  }

  if(ctrlcaps & STM_CTRL_CAPS_SATURATION)
  {
    config->caps |= STMFBIO_OUTPUT_CAPS_SATURATION;
    stm_display_output_get_control(out, STM_CTRL_SATURATION, &tmp);
    config->saturation = tmp;
  }

  if(ctrlcaps & STM_CTRL_CAPS_CONTRAST)
  {
    config->caps |= STMFBIO_OUTPUT_CAPS_CONTRAST;
    stm_display_output_get_control(out, STM_CTRL_CONTRAST, &tmp);
    config->contrast = tmp;
  }

  if(ctrlcaps & STM_CTRL_CAPS_HUE)
  {
    config->caps |= STMFBIO_OUTPUT_CAPS_HUE;
    stm_display_output_get_control(out, STM_CTRL_HUE, &tmp);
    config->hue = tmp;
  }

  /*
   * Set the default analogue output to be limited to the standard video range
   * where the hardware can enfore that.
   */
  stm_display_output_set_control(out, STM_CTRL_SIGNAL_RANGE, STM_SIGNAL_VIDEO_RANGE);
}


int
stmfb_get_output_configuration (struct stmfbio_output_configuration * const c,
                                struct stmfb_info                   * const i)
{
  switch(c->outputid)
  {
    case STMFBIO_OUTPUTID_MAIN:
      DPRINTK("returning main configuration (address = %p)\n",&i->main_config);
      *c = i->main_config;
      break;
    default:
      DPRINTK("invalid output id = %d\n",(int)c->outputid);
      return -EINVAL;
  }

  return 0;
}


static void stmfbio_set_sdtv_encoding(struct stmfb_info *i,
                                      stm_display_output_t *o,
                                      ULONG outputcaps,
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

  if(!(outputcaps & STM_OUTPUT_CAPS_SD_ANALOGUE))
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
    ULONG newstandard = c->sdtv_encoding;
    if(i->current_videomode.pModeLine->ModeParams.OutputStandards & newstandard)
    {
      if(i->current_videomode.pModeLine->ModeParams.OutputStandards & STM_OUTPUT_STD_SMPTE293M)
        newstandard |= STM_OUTPUT_STD_SMPTE293M;

      if(stm_display_output_start(o,i->current_videomode.pModeLine,newstandard)<0)
        failed = STMFBIO_OUTPUT_CAPS_SDTV_ENCODING;
      else
        i->current_videomode.ulTVStandard = newstandard;
    }
    else
    {
      failed = STMFBIO_OUTPUT_CAPS_SDTV_ENCODING;
      DPRINTK("SD TV Encoding not supported by current mode, cannot do an immediate change\n");
    }
  }

  if(failed == 0)
    newconfig->sdtv_encoding = c->sdtv_encoding;
  else
    c->failed |= failed;
}


static void stmfbio_set_analogue_config(struct stmfb_info *i,
                                       stm_display_output_t *o,
                                       ULONG outputcaps,
                                       struct stmfbio_output_configuration *c,
                                       struct stmfbio_output_configuration *newconfig)
{
  __u32 failed = 0;
  __u32 tmp;
  stm_display_signal_range_t range;
  stm_ycbcr_colorspace_t colorspace;

  DPRINTK(": outcaps = 0x%08lx analogue config = 0x%08x\n",outputcaps,c->analogue_config);

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
    if(!(outputcaps & (STM_OUTPUT_CAPS_SD_RGB_CVBS_YC | STM_OUTPUT_CAPS_RGB_EXCLUSIVE)))
    {
      DPRINTK(": RGB not supported\n");
      failed = STMFBIO_OUTPUT_CAPS_ANALOGUE_CONFIG;
    }
    else if(((c->analogue_config & STMFBIO_OUTPUT_ANALOGUE_MASK) != STMFBIO_OUTPUT_ANALOGUE_RGB) &&
            !(outputcaps & STM_OUTPUT_CAPS_SD_RGB_CVBS_YC))
    {
      DPRINTK(": RGB+CVBS/YC (SCART) not supported\n");
      failed = STMFBIO_OUTPUT_CAPS_ANALOGUE_CONFIG;
    }
  }

  if(c->analogue_config & STMFBIO_OUTPUT_ANALOGUE_YPrPb)
  {
    if(!(outputcaps & (STM_OUTPUT_CAPS_SD_YPrPb_CVBS_YC | STM_OUTPUT_CAPS_YPrPb_EXCLUSIVE)))
    {
      DPRINTK(": YPrPb not supported\n");
      failed = STMFBIO_OUTPUT_CAPS_ANALOGUE_CONFIG;
    }
    else if(((c->analogue_config & STMFBIO_OUTPUT_ANALOGUE_MASK) != STMFBIO_OUTPUT_ANALOGUE_YPrPb) &&
            !(outputcaps & STM_OUTPUT_CAPS_SD_YPrPb_CVBS_YC))
    {
      DPRINTK(": YPrPb+CVBS/YC not supported\n");
      failed = STMFBIO_OUTPUT_CAPS_ANALOGUE_CONFIG;
    }
  }

  tmp = (STM_OUTPUT_CAPS_CVBS_YC_EXCLUSIVE |
         STM_OUTPUT_CAPS_SD_RGB_CVBS_YC    |
         STM_OUTPUT_CAPS_SD_YPrPb_CVBS_YC);

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

  if(stm_display_output_set_control(o, STM_CTRL_VIDEO_OUT_SELECT, tmp)<0)
    failed = STMFBIO_OUTPUT_CAPS_ANALOGUE_CONFIG;

  range = (c->analogue_config & STMFBIO_OUTPUT_ANALOGUE_CLIP_FULLRANGE)?STM_SIGNAL_FILTER_SAV_EAV:STM_SIGNAL_VIDEO_RANGE;
  if(stm_display_output_set_control(o, STM_CTRL_SIGNAL_RANGE, range)<0)
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
    if(stm_display_output_set_control(o, STM_CTRL_YCBCR_COLORSPACE, colorspace)<0)
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
  __u32 filter = 0;

  DPRINTK(": dvo_config = 0x%08x\n",c->dvo_config);

  if(!i->pFBDVO)
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
      ret = stm_display_output_start(i->pFBDVO, i->current_videomode.pModeLine, i->current_videomode.ulTVStandard);
    else
      ret = stm_display_output_stop(i->pFBDVO);

    if(ret<0 && signal_pending(current))
      failed = STMFBIO_OUTPUT_CAPS_DVO_CONFIG;
  }

  switch(c->dvo_config & STMFBIO_OUTPUT_DVO_MODE_MASK)
  {
    case STMFBIO_OUTPUT_DVO_YUV_444_16BIT:
      format = STM_VIDEO_OUT_YUV;
      break;
    case STMFBIO_OUTPUT_DVO_YUV_444_24BIT:
      format = STM_VIDEO_OUT_YUV_24BIT;
      break;
    case STMFBIO_OUTPUT_DVO_YUV_422_16BIT:
      format = STM_VIDEO_OUT_422;
      break;
    case STMFBIO_OUTPUT_DVO_ITUR656:
      format = STM_VIDEO_OUT_ITUR656;
      break;
    case STMFBIO_OUTPUT_DVO_RGB_24BIT:
      format = STM_VIDEO_OUT_RGB;
      break;
  }

  if(stm_display_output_set_control(i->pFBDVO, STM_CTRL_VIDEO_OUT_SELECT,format)<0)
    failed = STMFBIO_OUTPUT_CAPS_DVO_CONFIG;

  range = (c->dvo_config & STMFBIO_OUTPUT_DVO_CLIP_FULLRANGE)?STM_SIGNAL_FILTER_SAV_EAV:STM_SIGNAL_VIDEO_RANGE;
  if(stm_display_output_set_control(i->pFBDVO, STM_CTRL_SIGNAL_RANGE, range)<0)
    failed = STMFBIO_OUTPUT_CAPS_DVO_CONFIG;

  filter = (c->dvo_config & STMFBIO_OUTPUT_DVO_CHROMA_FILTER_ENABLED)?1:0;
  if(stm_display_output_set_control(i->pFBDVO, STM_CTRL_422_CHROMA_FILTER, filter)<0)
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

  DPRINTK(": hdmi_config = 0x%08x\n",c->hdmi_config);

  if(!i->hdmi)
    failed = STMFBIO_OUTPUT_CAPS_HDMI_CONFIG;
  else if(((c->hdmi_config & STMFBIO_OUTPUT_HDMI_COLOURDEPTH_MASK) != 0) &&
          !(i->hdmi->capabilities & STM_OUTPUT_CAPS_TMDS_DEEPCOLOUR))
  {
    DPRINTK(": No deepcolour support available\n");
    failed = STMFBIO_OUTPUT_CAPS_HDMI_CONFIG;
  }

  if((c->hdmi_config & STMFBIO_OUTPUT_HDMI_YUV) &&
     (c->hdmi_config & STMFBIO_OUTPUT_HDMI_422) &&
     (c->hdmi_config & STMFBIO_OUTPUT_HDMI_COLOURDEPTH_MASK) != STM_VIDEO_OUT_HDMI_COLOURDEPTH_24BIT)
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
    video |= (c->hdmi_config & STMFBIO_OUTPUT_HDMI_422)?STM_VIDEO_OUT_422:STM_VIDEO_OUT_YUV;
  else
    video |= STM_VIDEO_OUT_RGB;

  switch(c->hdmi_config & STMFBIO_OUTPUT_HDMI_COLOURDEPTH_MASK)
  {
    case STMFBIO_OUTPUT_HDMI_COLOURDEPTH_30BIT:
      video |= STM_VIDEO_OUT_HDMI_COLOURDEPTH_30BIT;
      break;
    case STMFBIO_OUTPUT_HDMI_COLOURDEPTH_36BIT:
      video |= STM_VIDEO_OUT_HDMI_COLOURDEPTH_36BIT;
      break;
    case STMFBIO_OUTPUT_HDMI_COLOURDEPTH_48BIT:
      video |= STM_VIDEO_OUT_HDMI_COLOURDEPTH_48BIT;
      break;
    default:
      video |= STM_VIDEO_OUT_HDMI_COLOURDEPTH_24BIT;
      break;
  }

  if(mutex_lock_interruptible(&i->hdmi->lock))
  {
    c->failed |= STMFBIO_OUTPUT_CAPS_HDMI_CONFIG;
    return;
  }

  if(i->hdmi->video_type != video)
  {
    i->hdmi->video_type = video;

    if((i->hdmi->status != STM_DISPLAY_DISCONNECTED) && (i->hdmi->edid_info.display_type == STM_DISPLAY_HDMI))
    {
      if(i->hdmi->status == STM_DISPLAY_NEEDS_RESTART)
        i->hdmi->status_changed = 1; /* Give the management thread a kick to try again with the new config */
      else
        stm_display_output_stop(i->hdmi->hdmi_output);
    }
  }

  hdmidisable = (c->hdmi_config & STMFBIO_OUTPUT_HDMI_DISABLED)?1:0;
  if(hdmidisable != i->hdmi->disable)
  {
    i->hdmi->disable = hdmidisable;
    i->hdmi->status_changed = 1;
  }

  range = (c->hdmi_config & STMFBIO_OUTPUT_HDMI_CLIP_FULLRANGE)?STM_SIGNAL_FILTER_SAV_EAV:STM_SIGNAL_VIDEO_RANGE;
  if(stm_display_output_set_control(i->hdmi->hdmi_output, STM_CTRL_SIGNAL_RANGE, range)<0)
    failed = STMFBIO_OUTPUT_CAPS_HDMI_CONFIG;

  if(i->hdmi->status_changed)
    wake_up(&i->hdmi->status_wait_queue);

  mutex_unlock(&i->hdmi->lock);

  if(failed == 0)
    newconfig->hdmi_config = c->hdmi_config;
  else
    c->failed |= failed;

}


static void stmfbio_set_mixer_background(stm_display_output_t *o,
                                         ULONG  ctrlcaps,
                                         struct stmfbio_output_configuration *c,
                                         struct stmfbio_output_configuration *newconfig)
{
  __u32 failed = 0;

  DPRINTK("\n");

  if(!(ctrlcaps & STM_CTRL_CAPS_BACKGROUND))
    failed = STMFBIO_OUTPUT_CAPS_MIXER_BACKGROUND;

  if((failed == 0) && (c->activate == STMFBIO_ACTIVATE_ON_NEXT_CHANGE))
    newconfig->mixer_background = c->mixer_background;

  c->failed |= failed;

  if((failed != 0) || (c->activate != STMFBIO_ACTIVATE_IMMEDIATE))
    return;

  if(stm_display_output_set_control(o, STM_CTRL_BACKGROUND_ARGB, c->mixer_background)<0)
  {
    failed = STMFBIO_OUTPUT_CAPS_MIXER_BACKGROUND;
  }

  if(failed == 0)
    newconfig->mixer_background = c->mixer_background;
  else
    c->failed |= failed;

}


static void stmfbio_set_psi(stm_display_output_t *o,
                            ULONG  ctrlcaps,
                            struct stmfbio_output_configuration *c,
                            struct stmfbio_output_configuration *newconfig)
{
  __u32 failed = 0;

  DPRINTK("\n");

  if(!(ctrlcaps & STM_CTRL_CAPS_BRIGHTNESS) && (c->caps & STMFBIO_OUTPUT_CAPS_BRIGHTNESS))
    failed |= STMFBIO_OUTPUT_CAPS_BRIGHTNESS;

  if(!(ctrlcaps & STM_CTRL_CAPS_SATURATION) && (c->caps & STMFBIO_OUTPUT_CAPS_SATURATION))
    failed |= STMFBIO_OUTPUT_CAPS_SATURATION;

  if(!(ctrlcaps & STM_CTRL_CAPS_CONTRAST) && (c->caps & STMFBIO_OUTPUT_CAPS_CONTRAST))
    failed |= STMFBIO_OUTPUT_CAPS_CONTRAST;

  if(!(ctrlcaps & STM_CTRL_CAPS_HUE) && (c->caps & STMFBIO_OUTPUT_CAPS_HUE))
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

    if(stm_display_output_set_control(o, STM_CTRL_BRIGHTNESS, c->brightness)<0)
      failed |= STMFBIO_OUTPUT_CAPS_BRIGHTNESS;
    else
      newconfig->brightness = c->brightness;
  }

  if(c->caps & STMFBIO_OUTPUT_CAPS_SATURATION)
  {
    DPRINTK("saturation = %u\n",(unsigned)c->saturation);

    if(stm_display_output_set_control(o, STM_CTRL_SATURATION, c->saturation)<0)
      failed |= STMFBIO_OUTPUT_CAPS_SATURATION;
    else
      newconfig->saturation = c->saturation;
  }

  if(c->caps & STMFBIO_OUTPUT_CAPS_CONTRAST)
  {
    DPRINTK("contrast = %u\n",(unsigned)c->contrast);

    if(stm_display_output_set_control(o, STM_CTRL_CONTRAST, c->contrast)<0)
      failed |= STMFBIO_OUTPUT_CAPS_CONTRAST;
    else
      newconfig->contrast = c->contrast;
  }

  if(c->caps & STMFBIO_OUTPUT_CAPS_HUE)
  {
    DPRINTK("hue = %u\n",(unsigned)c->hue);

    if(stm_display_output_set_control(o, STM_CTRL_HUE, c->hue)<0)
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
  stm_display_output_t *o = NULL;
  struct stmfbio_output_configuration *newconfig = NULL;
  ULONG outcaps,ctrlcaps;
  c->failed = 0;

  switch(c->outputid)
  {
    case STMFBIO_OUTPUTID_MAIN:
      DPRINTK("Updating main output configuration\n");

      o = i->pFBMainOutput;
      newconfig = &i->main_config;
      break;
    default:
      DPRINTK("Invalid output configuration id %d\n",(int)c->outputid);
      break;
  }

  if(!o)
    return -EINVAL;


  if(stm_display_output_get_capabilities(o,&outcaps)<0)
  {
    DPRINTK("Cannot get output capabilities\n");
    return -EIO;
  }

  if(stm_display_output_get_control_capabilities(o,&ctrlcaps)<0)
  {
    DPRINTK("Cannot get output control capabilities\n");
    return -EIO;
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
    stmfbio_set_mixer_background(o, ctrlcaps, c, newconfig);

  if(c->caps & STMFBIO_OUTPUT_CAPS_PSI_MASK)
    stmfbio_set_psi(o, ctrlcaps, c, newconfig);

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
