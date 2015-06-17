/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/hdmi/hdmitx/hdmitxkthread.c
 * Copyright (c) 2005-2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/
#include <linux/version.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/freezer.h>
#include <linux/slab.h>
#include <linux/fb.h>
#include <linux/i2c.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <stm_common.h>
#include <stm_event.h>
#include <event_subscriber.h>
#include <linux/gpio.h>
#include <asm/uaccess.h>
#include <linux/semaphore.h>

#include <stm_display.h>
#include <stm_display_link.h>
#include <linux/stm/stmcoredisplay.h>
#include <linux/stm/stmcorehdmi.h>
#include <linux/stm/stmcorelink.h>
#include "stmhdmi.h"

#define DISPLAY_LINK_EVT_NUMBER  (3)    /* Display link event Number: HPD, Rx sense and HDCP events */
#define HDCP_STATUS_HDMI_MODE    (0x1<<12)
#define GET_HDCP_STATUS_HDMI_MODE(x)            ((x&HDCP_STATUS_HDMI_MODE)==HDCP_STATUS_HDMI_MODE)
#define SINK_STATUS(x) (((unsigned int)(x)[0]) | (((unsigned int)(x)[1]) << 8))

#if defined(SDK2_ENABLE_HDMI_TX_ATTRIBUTES)
static inline void hdmi_sysfs_notify(struct kobject *kobj, const char *dir, const char *attr)
{
  sysfs_notify(kobj, dir, attr);
}
#else
static inline void hdmi_sysfs_notify(struct kobject *kobj, const char *dir, const char *attr) {}
#endif

static int hdmi_open_device(struct stm_hdmi *dev)
{
  #ifdef CONFIG_PM_RUNTIME
  /*
   * DPM : Open required devices
   */
  mutex_lock(&dev->hdmilock);
  dev->device_is_opened = 1;
  if(!dev->device)
  {
    if(stm_display_open_device(dev->display_device_id, &dev->device)<0)
    {
      printk(KERN_ERR "Unable to open the LINK %d\n",dev->display_device_id);
      dev->device_is_opened = 0;
      mutex_unlock(&dev->hdmilock);
      return -ENODEV;
    }
    dev->device_use_count ++;
  }
  mutex_unlock(&dev->hdmilock);
  #endif
  return 0;
}

static int hdmi_release_device(struct stm_hdmi *dev)
{
  #ifdef CONFIG_PM_RUNTIME
  /*
   * DPM : Close used devices
   */
  mutex_lock(&dev->hdmilock);
  if((dev->device_use_count > 0) && (dev->device_is_opened == 1))
    dev->device_use_count --;
  if(dev->device && (dev->device_use_count == 0))
  {
    stm_display_device_close(dev->device);
    dev->device = NULL;
  }
  dev->device_is_opened = 0;
  mutex_unlock(&dev->hdmilock);
  #endif
  return 0;
}

/******************************************************************************
 * Functions to check the EDID for a valid mode and enable the HDMI output
 */
static void stmhdmi_configure_hdmi_formatting(struct stm_hdmi *hdmi)
{
  uint32_t video_type;
  uint32_t colour_depth;
  uint32_t quantization = 0;
  uint32_t support_deepcolour;

  /*
   * We are connected to a CEA conformant HDMI device. In this case the spec
   * says we must do HDMI; if the device does not support YCrCb then force
   * RGB output.
   */
  if((hdmi->edid_info.cea_capabilities & (STM_CEA_CAPS_YUV|STM_CEA_CAPS_422|STM_CEA_CAPS_420))
   ||((hdmi->edid_info.display_type == STM_DISPLAY_HDMI) && (hdmi->edid_updated == false)))
  {
    video_type = hdmi->video_type & (STM_VIDEO_OUT_RGB |
                                     STM_VIDEO_OUT_YUV |
                                     STM_VIDEO_OUT_422 |
                                     STM_VIDEO_OUT_444 |
                                     STM_VIDEO_OUT_420);
  }
  else
  {
    video_type = STM_VIDEO_OUT_RGB;
  }

  DPRINTK("Setting HDMI output format %s\n",(video_type&STM_VIDEO_OUT_RGB)?"RGB":"YUV");

  support_deepcolour = ((hdmi->edid_info.hdmi_vsdb_flags & STM_HDMI_VSDB_DC_MASK)
                      ||(hdmi->edid_info.hdmi_hf_dc_flags & STM_HDMI_HF_VSDB_DC_MASK))?1:0;

  if(stm_display_output_set_control(hdmi->hdmi_output, OUTPUT_CTRL_SINK_SUPPORTS_DEEPCOLOR, support_deepcolour) < 0)
  {
    DPRINTK("Unable to set sink deepcolor state on HDMI output, HW probably doesn't support it\n");
    support_deepcolour = 0;
  }

  /*
   * Filter the requested colour depth based on the EDID and colour format,
   * falling back to standard colour if there is any mismatch.
   */
  if(!support_deepcolour               ||
     (video_type & STM_VIDEO_OUT_422)  ||
     ((video_type & STM_VIDEO_OUT_444) && !(hdmi->edid_info.hdmi_vsdb_flags & STM_HDMI_VSDB_DC_Y444))||
     ((video_type & STM_VIDEO_OUT_420) && !(hdmi->edid_info.hdmi_hf_dc_flags & STM_HDMI_HF_VSDB_DC_MASK)))
  {
    DPRINTK("Deepcolour not supported in requested colour format\n");
    colour_depth = STM_VIDEO_OUT_24BIT;
  }
  else
  {
    switch(hdmi->video_type & STM_VIDEO_OUT_DEPTH_MASK)
    {
      case STM_VIDEO_OUT_30BIT:
        if((hdmi->edid_info.hdmi_vsdb_flags & STM_HDMI_VSDB_DC_30BIT)||
          (hdmi->edid_info.hdmi_hf_dc_flags & STM_HDMI_HF_VSDB_DC_30BIT_420))
          colour_depth = STM_VIDEO_OUT_30BIT;
        else
          colour_depth = STM_VIDEO_OUT_24BIT;
        break;
      case STM_VIDEO_OUT_36BIT:
        if((hdmi->edid_info.hdmi_vsdb_flags & STM_HDMI_VSDB_DC_36BIT)||
          (hdmi->edid_info.hdmi_hf_dc_flags & STM_HDMI_HF_VSDB_DC_36BIT_420))
          colour_depth = STM_VIDEO_OUT_36BIT;
        else
          colour_depth = STM_VIDEO_OUT_24BIT;
        break;
      case STM_VIDEO_OUT_48BIT:
        if((hdmi->edid_info.hdmi_vsdb_flags & STM_HDMI_VSDB_DC_48BIT)||
          (hdmi->edid_info.hdmi_hf_dc_flags & STM_HDMI_HF_VSDB_DC_48BIT_420))
          colour_depth = STM_VIDEO_OUT_48BIT;
        else
          colour_depth = STM_VIDEO_OUT_24BIT;
        break;
      case STM_VIDEO_OUT_24BIT:
      default:
        colour_depth = STM_VIDEO_OUT_24BIT;
        break;
    }
  }

  switch(colour_depth)
  {
    case STM_VIDEO_OUT_24BIT:
      DPRINTK("Setting 24bit colour\n");
      break;
    case STM_VIDEO_OUT_30BIT:
      DPRINTK("Setting 30bit colour\n");
      break;
    case STM_VIDEO_OUT_36BIT:
      DPRINTK("Setting 36bit colour\n");
      break;
    case STM_VIDEO_OUT_48BIT:
      DPRINTK("Setting 48bit colour\n");
      break;
  }

  stm_display_output_set_control(hdmi->hdmi_output, OUTPUT_CTRL_VIDEO_OUT_SELECT, (STM_VIDEO_OUT_HDMI | video_type | colour_depth));

  if(hdmi->edid_info.cea_vcdb_flags & STM_CEA_VCDB_QS_RGB_QUANT)
    quantization |= (uint32_t)STM_VCDB_QUANTIZATION_RGB;

  if(hdmi->edid_info.cea_vcdb_flags & STM_CEA_VCDB_QY_YCC_QUANT)
    quantization |= (uint32_t)STM_VCDB_QUANTIZATION_YCC;

  stm_display_output_set_control(hdmi->hdmi_output, OUTPUT_CTRL_VCDB_QUANTIZATION_SUPPORT, quantization);

  if(hdmi->cea_mode_from_edid)
  {
    uint32_t tmp = 0;
    switch(hdmi->edid_info.tv_aspect)
    {
      case STM_WSS_ASPECT_RATIO_4_3:
        tmp = STM_AVI_VIC_4_3;
        break;
      case STM_WSS_ASPECT_RATIO_16_9:
        tmp = STM_AVI_VIC_16_9;
        break;
      case STM_WSS_ASPECT_RATIO_GT_16_9:
        tmp = STM_AVI_VIC_64_27;
        break;
    default:
      break;
    }
    stm_display_output_set_control(hdmi->hdmi_output, OUTPUT_CTRL_AVI_VIC_SELECT, tmp);
  }

  DPRINTK("Finished\n");
}

static int send_event(struct stm_hdmi * hdmi, unsigned int type)
{
  stm_event_t  event;
  event.event_id = type;
  event.object = (void *)hdmi->hdmi_output;
  return stm_event_signal(&event);
}

static int stmhdmi_determine_pixel_repetition(struct stm_hdmi    *hdmi,
                                              stm_display_mode_t *mode)

{
  int repeat = hdmi->max_pixel_repeat;
  uint32_t saved_flags = mode->mode_params.flags;
  stm_display_mode_t m;

  DPRINTK("\n");

  if((mode->mode_params.output_standards & STM_OUTPUT_STD_SD_MASK)||
     (mode->mode_params.output_standards & STM_OUTPUT_STD_ED_MASK))
  {
    if(!hdmi->non_strict_edid_semantics)
    {
      /*
       * Find the maximum pixel repeat we can use for SD/ED modes; this is so we
       * always have the maximum audio bandwidth available.
       */
      while(repeat>1)
      {
        DPRINTK("Trying pixel repeat = %d\n",repeat);
        if(!stm_display_output_find_display_mode(hdmi->main_output,
                                                 mode->mode_params.active_area_width*repeat,
                                                 mode->mode_params.active_area_height,
                                                 mode->mode_timing.lines_per_frame,
                                                 mode->mode_timing.pixels_per_line*repeat,
                                                 mode->mode_timing.pixel_clock_freq*repeat,
                                                 mode->mode_params.scan_type,
                                                 &m))
        {
          int n;
          DPRINTK("Found SoC timing mode for pixel repeated mode %d\n",m.mode_id);
          for(n=0;n<hdmi->edid_info.num_modes;n++)
          {
            const stm_display_mode_t *tmp = &hdmi->edid_info.display_modes[n];

            if(tmp->mode_params.hdmi_vic_codes[STM_AR_INDEX_4_3] == m.mode_params.hdmi_vic_codes[STM_AR_INDEX_4_3] &&
               tmp->mode_params.hdmi_vic_codes[STM_AR_INDEX_16_9] == m.mode_params.hdmi_vic_codes[STM_AR_INDEX_16_9] &&
               tmp->mode_params.hdmi_vic_codes[STM_AR_INDEX_64_27] == m.mode_params.hdmi_vic_codes[STM_AR_INDEX_64_27])
            {
              DPRINTK("Matched pixel repeated mode %d with display EDID supported modes\n",tmp->mode_id);
              *mode = m;
              break;
            }
          }

          if(n<hdmi->edid_info.num_modes)
          break;
        }

        repeat /= 2;
      }
      switch(repeat)
      {
        case 4:
          mode->mode_params.flags = (saved_flags | STM_MODE_FLAGS_HDMI_PIXELREP_4X);
          break;
        case 2:
          mode->mode_params.flags = (saved_flags | STM_MODE_FLAGS_HDMI_PIXELREP_2X);
          break;
        case 1:
          mode->mode_params.flags = saved_flags;
          break;
        default:
          DPRINTK("No pixel repeated mode supported with display EDID\n");
          return -EINVAL;
         break;
      }
    }
    else
    {
      repeat = hdmi->max_pixel_repeat;
      DPRINTK("No supported pixel repeated EDID mode found\n");
      DPRINTK("Trying to find %dx pixel repeat for non-strict EDID semantics\n", repeat);
      if(!stm_display_output_find_display_mode(hdmi->main_output,
                                               mode->mode_params.active_area_width*repeat,
                                               mode->mode_params.active_area_height,
                                               mode->mode_timing.lines_per_frame,
                                               mode->mode_timing.pixels_per_line*repeat,
                                               mode->mode_timing.pixel_clock_freq*repeat,
                                               mode->mode_params.scan_type,
                                               &m))
      {
        DPRINTK("Found SoC timing mode for %dx pixel repeated mode %d\n", repeat, m.mode_id);
        *mode = m;
        switch(repeat)
        {
          case 4:
            mode->mode_params.flags = (saved_flags | STM_MODE_FLAGS_HDMI_PIXELREP_4X);
            break;
          case 2:
            mode->mode_params.flags = (saved_flags | STM_MODE_FLAGS_HDMI_PIXELREP_2X);
            break;
          default:
            mode->mode_params.flags = saved_flags;
            break;
        }
      }
      else
      {
        DPRINTK("No pixel repeated mode supported for non-strict EDID semantics\n");
        return -EINVAL;
      }
    }
  }
  else
  {
    DPRINTK("Using no pixel repetition for HD mode\n");
  }
  return 0;
}

static int stmhdmi_output_start_hdcp(struct stm_hdmi *hdmi)
{
  int retVal=0;

  if ((hdmi->hdcp_enable) && (!hdmi->current_hdcp_status))
  {
    if ((retVal = stm_display_link_set_ctrl(hdmi->link, STM_DISPLAY_LINK_CTRL_HDCP_ENABLE,
       (hdmi->edid_info.display_type == STM_DISPLAY_HDMI)?1:2))<0)
          printk(KERN_ERR " Error in HDCP enabling\n");
    hdmi->current_hdcp_status =true;
  }
  else
  {
    DPRINTK(" HDCP is not started by user application\n");
  }
  return retVal;
}

static int stmhdmi_output_stop_hdcp(struct stm_hdmi *hdmi)
{
  int retVal=0;
  if ((hdmi->hdcp_enable) && (hdmi->current_hdcp_status))
  {
     if ((retVal = stm_display_link_set_ctrl(hdmi->link, STM_DISPLAY_LINK_CTRL_HDCP_ENABLE, 0)) <0)
          printk(KERN_ERR " Error in HDCP disabling\n");
     hdmi->current_hdcp_status =false;
  }
  else
  {
    DPRINTK(" HDCP is not already started or encrytion is not already enabled on desired output\n");
  }
  return 0;
}


static int stmhdmi_output_start(struct stm_hdmi    *hdmi,
                                stm_display_mode_t *mode)
{
  int retVal=0;
  stm_display_mode_t current_hdmi_mode;

  DPRINTK("\n");

  hdmi_open_device(hdmi);
  /*
   * Find out if the HDMI output is still running, we may be handling a deferred
   * disconnection hotplug event.
   */
  if(stm_display_output_get_current_display_mode(hdmi->hdmi_output, &current_hdmi_mode)<0)
  {
    current_hdmi_mode.mode_id = STM_TIMING_MODE_RESERVED;
  }
  DPRINTK("Current mode ID is %d\n",current_hdmi_mode.mode_id);

  if(hdmi->deferred_disconnect > 0)
  {
    /*
     * You might think, hold on how can the mode have changed in this case,
     * but if the EDID changed what pixel repeated modes are supported then
     * that might happen. Therefore we return an error and the output will
     * be disabled and the application notified that it needs to do something.
     */
    if((current_hdmi_mode.mode_id == mode->mode_id) && (current_hdmi_mode.mode_params.output_standards == mode->mode_params.output_standards))
      retVal = 0;
    else
      retVal = -EINVAL;

    goto exit;
  }

  /*
   * If we got a request to restart the output, without a disconnect first,
   * then really do a restart if the mode is different by stopping the
   * output first. If the mode is exactly the same just call start which
   * will reset the output's connection state to connected.
   */
  if((!hdmi->stop_output)&& ((current_hdmi_mode.mode_id != mode->mode_id) || (current_hdmi_mode.mode_params.output_standards != mode->mode_params.output_standards)))
  {
    if (stmhdmi_output_stop_hdcp(hdmi) != 0)
       DPRINTK("HDMI manager can not stop HDCP properly\n");

    stm_display_output_stop(hdmi->hdmi_output);
    hdmi->stop_output = true;

    if (stm_display_link_set_ctrl(hdmi->link, STM_DISPLAY_LINK_CTRL_DISPLAY_MODE, STM_DISPLAY_INVALID)<0)
    {
      retVal = -EINVAL;
      goto exit;
    }
  }

  DPRINTK("Starting Video Mode %ux%u%s\n",mode->mode_params.active_area_width,
                                          mode->mode_params.active_area_height,
                                          (mode->mode_params.scan_type == STM_INTERLACED_SCAN)?"i":"p");

  if(stm_display_output_start(hdmi->hdmi_output, mode)<0)
  {
    retVal = -EINVAL;
    goto exit;
  }

  if (stm_display_link_set_ctrl(hdmi->link, STM_DISPLAY_LINK_CTRL_DISPLAY_MODE,
     (hdmi->edid_info.display_type == STM_DISPLAY_HDMI)?DISPLAY_MODE_HDMI:DISPLAY_MODE_DVI)<0)
  {
    retVal = -EINVAL;
    goto exit;
  }

exit:
  return retVal;
}


static int stmhdmi_enable_mode_by_timing_limits(struct stm_hdmi    *hdmi,
                                                stm_display_mode_t *mode)
{
  uint32_t hfreq,vfreq;

  DPRINTK("Falling back refresh range limits for DVI monitor\n");

  if(mode->mode_timing.pixel_clock_freq > (hdmi->edid_info.max_clock*1000000))
  {
    DPRINTK("Pixel clock (%uHz) out of range\n",mode->mode_timing.pixel_clock_freq);
    return -EINVAL;
  }

  /*
   * Check H & V Refresh instead
   */
  hfreq = mode->mode_timing.pixel_clock_freq / mode->mode_timing.pixels_per_line;
  vfreq = mode->mode_params.vertical_refresh_rate / 1000;

  if((vfreq < hdmi->edid_info.min_vfreq) || (vfreq > hdmi->edid_info.max_vfreq))
  {
    DPRINTK("Vertical refresh (%uHz) out of range\n",vfreq);
    return -EINVAL;
  }

  if((hfreq < (hdmi->edid_info.min_hfreq*1000)) || (hfreq > (hdmi->edid_info.max_hfreq*1000)))
  {
    DPRINTK("Horizontal refresh (%uKHz) out of range\n",hfreq/1000);
    return -EINVAL;
  }

  DPRINTK("Starting DVI Video Mode %ux%u hfreq = %uKHz vfreq = %uHz\n",mode->mode_params.active_area_width,
                                                                       mode->mode_params.active_area_height,
                                                                       hfreq/1000,
                                                                       vfreq);

  return stmhdmi_output_start(hdmi, mode);
}


static int stmhdmi_enable_link(struct stm_hdmi *hdmi)
{
  int n, edid_4_3_3D_flag, edid_16_9_3D_flag, edid_64_27_3D_flag;
  int STM_MODE_FLAGS_3D;
  stm_display_mode_t mode;
  STM_MODE_FLAGS_3D =   STM_MODE_FLAGS_3D_SBS_HALF | STM_MODE_FLAGS_3D_TOP_BOTTOM | STM_MODE_FLAGS_3D_FRAME_PACKED;

  if(hdmi->edid_info.display_type == STM_DISPLAY_INVALID)
  {
    DPRINTK("Invalid EDID: Not enabling link\n");
    return -ENODEV;
  }

  /*
   * Get the master output's mode, this is what we want to set
   */
  if(stm_display_output_get_current_display_mode(hdmi->main_output, &mode)<0)
    return -EINVAL;

  BUG_ON(mode.mode_id == STM_TIMING_MODE_RESERVED);

  if(mode.mode_id < STM_TIMING_MODE_CUSTOM)
  {
    /*
     * If the master is in a known standard mode, replace the output standard
     * mask with the original template version so we can see if it is a CEA861
     * mode or not.
     */
    stm_display_mode_t tmp;
    if(stm_display_output_get_display_mode(hdmi->main_output, mode.mode_id, &tmp)<0)
      return -EINVAL;

    mode.mode_params.output_standards = tmp.mode_params.output_standards;
  }
  /*
   * Make sure pixel repetition mode flags are cleared
   */
  mode.mode_params.flags &= ~(STM_MODE_FLAGS_HDMI_PIXELREP_2X | STM_MODE_FLAGS_HDMI_PIXELREP_4X);

  if(hdmi->edid_info.display_type == STM_DISPLAY_DVI)
  {
    /*
     * We believe we now have a computer monitor or a TV with a DVI input not
     * a HDMI input. So set the output format to DVI and force RGB.
     */
    DPRINTK("Setting DVI/RGB output\n");
    stm_display_output_set_control(hdmi->hdmi_output, OUTPUT_CTRL_VIDEO_OUT_SELECT, (STM_VIDEO_OUT_DVI | STM_VIDEO_OUT_RGB | STM_VIDEO_OUT_24BIT));
    /*
     * DVI devices do not support deepcolor.
     */
    stm_display_output_set_control(hdmi->hdmi_output, OUTPUT_CTRL_SINK_SUPPORTS_DEEPCOLOR, 0);
  }
  else
  {
    stmhdmi_configure_hdmi_formatting(hdmi);
  }

  if(stmhdmi_determine_pixel_repetition(hdmi,&mode)!=0)
    return -EINVAL;

  /*
   * Allow pure VESA and NTG5 standards as well as CEA861, anything else is
   * not supported. For modes like VGA, which are defined for both CEA and VESA
   * then we prefer CEA so we get complete AVI infoframe behaviour.
   *
   * Note: do this _after_ looking for pixel repeated modes as that requires
   * and may modify the tvStandard.
   */
  if((mode.mode_params.output_standards & STM_OUTPUT_STD_CEA861) == 0)
  {
    DPRINTK("Non CEA861 mode\n");
    if(mode.mode_params.output_standards & STM_OUTPUT_STD_VESA_MASK)
      mode.mode_params.output_standards = (mode.mode_params.output_standards & STM_OUTPUT_STD_VESA_MASK);
    else if(mode.mode_params.output_standards & STM_OUTPUT_STD_NTG5)
      mode.mode_params.output_standards = STM_OUTPUT_STD_NTG5;
    else if(mode.mode_params.output_standards & STM_OUTPUT_STD_HDMI_LLC_EXT)
      mode.mode_params.output_standards = STM_OUTPUT_STD_HDMI_LLC_EXT;
    else
      return -EINVAL;
  }
  else
  {
    /*
     * Clear any standard flags other than for HDMI
     */
    mode.mode_params.output_standards = STM_OUTPUT_STD_CEA861;
    DPRINTK("Configuring CEA861 mode\n");
  }

  /*
   * If we have been asked not to check against the EDID, just start
   * the mode.
   */
  if(hdmi->non_strict_edid_semantics)
  {
    DPRINTK("Non strict EDID semantics selected, starting mode\n");
    return stmhdmi_output_start(hdmi, &mode);
  }

  for(n=0;n<hdmi->edid_info.num_modes;n++)
  {
    const stm_display_mode_t *tmp = &hdmi->edid_info.display_modes[n];

    /*
     * We match up the analogue display mode with the EDID mode by using the
     * HDMI video codes.
     */

    if((tmp->mode_params.hdmi_vic_codes[STM_AR_INDEX_4_3] == mode.mode_params.hdmi_vic_codes[STM_AR_INDEX_4_3]) &&
       (tmp->mode_params.hdmi_vic_codes[STM_AR_INDEX_16_9] == mode.mode_params.hdmi_vic_codes[STM_AR_INDEX_16_9])&&
       (tmp->mode_params.hdmi_vic_codes[STM_AR_INDEX_64_27] == mode.mode_params.hdmi_vic_codes[STM_AR_INDEX_64_27]))
    {
      edid_64_27_3D_flag = hdmi->edid_info.cea_codes[tmp->mode_params.hdmi_vic_codes[STM_AR_INDEX_64_27]].supported_3d_flags;
      edid_16_9_3D_flag = hdmi->edid_info.cea_codes[tmp->mode_params.hdmi_vic_codes[STM_AR_INDEX_16_9]].supported_3d_flags;
      edid_4_3_3D_flag = hdmi->edid_info.cea_codes[tmp->mode_params.hdmi_vic_codes[STM_AR_INDEX_4_3]].supported_3d_flags;
      if( (mode.mode_params.flags & STM_MODE_FLAGS_3D) != 0)
      {
        if ((mode.mode_params.hdmi_vic_codes[STM_AR_INDEX_64_27] &&
        (((mode.mode_params.flags & edid_64_27_3D_flag & STM_MODE_FLAGS_3D_SBS_HALF) == STM_MODE_FLAGS_3D_SBS_HALF) ||
        ((mode.mode_params.flags & edid_64_27_3D_flag & STM_MODE_FLAGS_3D_TOP_BOTTOM) == STM_MODE_FLAGS_3D_TOP_BOTTOM) ||
        ((mode.mode_params.flags & edid_64_27_3D_flag & STM_MODE_FLAGS_3D_FRAME_PACKED) == STM_MODE_FLAGS_3D_FRAME_PACKED))) ||
        (mode.mode_params.hdmi_vic_codes[STM_AR_INDEX_16_9] &&
        (((mode.mode_params.flags & edid_16_9_3D_flag & STM_MODE_FLAGS_3D_SBS_HALF) == STM_MODE_FLAGS_3D_SBS_HALF) ||
        ((mode.mode_params.flags & edid_16_9_3D_flag & STM_MODE_FLAGS_3D_TOP_BOTTOM) == STM_MODE_FLAGS_3D_TOP_BOTTOM) ||
        ((mode.mode_params.flags & edid_16_9_3D_flag & STM_MODE_FLAGS_3D_FRAME_PACKED) == STM_MODE_FLAGS_3D_FRAME_PACKED))) ||
        (mode.mode_params.hdmi_vic_codes[STM_AR_INDEX_4_3] &&
        (((mode.mode_params.flags & edid_4_3_3D_flag & STM_MODE_FLAGS_3D_SBS_HALF) == STM_MODE_FLAGS_3D_SBS_HALF) ||
        ((mode.mode_params.flags & edid_4_3_3D_flag & STM_MODE_FLAGS_3D_TOP_BOTTOM) == STM_MODE_FLAGS_3D_TOP_BOTTOM) ||
        ((mode.mode_params.flags & edid_4_3_3D_flag & STM_MODE_FLAGS_3D_FRAME_PACKED) == STM_MODE_FLAGS_3D_FRAME_PACKED))))
        {
          return stmhdmi_output_start(hdmi, &mode);
        }
      }
      else
      {
        return stmhdmi_output_start(hdmi, &mode);
      }
    }
  }

  DPRINTK("Current video mode not reported as supported by display\n");

  /*
   * We are still trying to only set modes indicated as valid in the EDID. So
   * for DVI devices allow modes that are inside the timing limits.
   */
  if(hdmi->edid_info.display_type == STM_DISPLAY_DVI)
    return stmhdmi_enable_mode_by_timing_limits(hdmi, &mode);

  return -EINVAL;
}


/*******************************************************************************
 * Kernel management thread state machine helper functions.
 */

static void stmhdmi_start_display(struct stm_hdmi *hdmi)
{
  uint16_t status = 0;
  if(!hdmi->disable)
  {
    DPRINTK("Starting HDMI Output hdmi = %p\n",hdmi);
    if ((hdmi->hdcp_enable) && (!hdmi->current_hdcp_status))
    {
      stm_display_link_hdcp_get_sink_status_info( hdmi->link, &status, 0);
    }
    if(stmhdmi_enable_link(hdmi) != 0)
    {
      /*
       * If we had an error starting the output, do a stop just in case it was
       * already running. Be careful, HDCP has to be stopped first when
       * it was already enabled. This might happen for instance if we got a restart
       * due to a change in the EDID mode semantics and the current mode is
       * no longer allowed on the display.
       */
      if (stmhdmi_output_stop_hdcp(hdmi) != 0)
        printk(KERN_ERR "HDMI manager can not stop HDCP properly\n");

      stm_display_output_set_control(hdmi->hdmi_output, OUTPUT_CTRL_VIDEO_OUT_SELECT, STM_VIDEO_OUT_NONE);
      hdmi->stop_output = true;

      if (stm_display_link_set_ctrl(hdmi->link, STM_DISPLAY_LINK_CTRL_DISPLAY_MODE, STM_DISPLAY_INVALID)<0)
        printk(KERN_ERR "HDMI manager can not set Display mode properly\n");

      if(hdmi->deferred_disconnect > 0)
      {
        /*
         * We were waiting to see if we could ignore the HPD pulse,
         * but it turns out the new EDID is not compatible with the
         * existing display mode so we now disable the output and signal
         * the hotplug attribute so applications can check the EDID change.
         */
        DPRINTK("Doing deferred output disable due to incompatible EDID\n");
        hdmi->disable = true;
        hdmi_sysfs_notify(&(hdmi->class_device->kobj), NULL, "hotplug");
     }
     hdmi_sysfs_notify(&(hdmi->class_device->kobj), NULL, "tmds_status");
    }
    else
    {
      hdmi->stop_output = false;
      if ((status & HDCP_STATUS_HDMI_MODE) == HDCP_STATUS_HDMI_MODE)
      {
        if ((hdmi->hdcp_enable) && (!hdmi->current_hdcp_status))
        {
          if (stm_display_link_set_ctrl(hdmi->link, STM_DISPLAY_LINK_CTRL_HDCP_ENABLE, 2))
            printk(KERN_ERR "Can't start HDCP Authentication\n");
          hdmi->current_hdcp_status =true;
        }
      }
      else
      {
        if (stmhdmi_output_start_hdcp(hdmi) !=0)
         DPRINTK("HDMI manager can not start HDCP properly\n");
      }

    }
  }
  else
  {
    /*
     * We are not starting the display because we are disabled, so the sysfs
     * hotplug notification that is normally done when we enter the connected
     * state will not happen. Hence we signal it now so applications know
     * to check the new EDID and to possibly re-enable the HDMI output.
     */
    hdmi_sysfs_notify(&(hdmi->class_device->kobj), NULL, "hotplug");
    hdmi_sysfs_notify(&(hdmi->class_device->kobj), NULL, "tmds_status");
    DPRINTK("HDMI Output disabled\n");
    stmhdmi_output_stop_hdcp(hdmi);
    stm_display_output_stop(hdmi->hdmi_output);
    hdmi->stop_output = true;
    if (stm_display_link_set_ctrl(hdmi->link, STM_DISPLAY_LINK_CTRL_DISPLAY_MODE, STM_DISPLAY_INVALID)<0)
      printk(KERN_ERR "HDMI manager can not set Display mode properly\n");
  }
  if(hdmi->stop_output)
    hdmi_release_device(hdmi);

  hdmi->deferred_disconnect = 0;

}

static void stmhdmi_stop_display(struct stm_hdmi *hdmi)
{
  DPRINTK("Stoping HDMI Output hdmi = %p\n",hdmi);

  if (!hdmi->stop_output)
  {
    if (stmhdmi_output_stop_hdcp(hdmi) != 0)
      printk(KERN_ERR "HDMI manager can not stop HDCP properly\n");

    stm_display_output_stop(hdmi->hdmi_output);
    hdmi->stop_output = true;
    hdmi_release_device(hdmi);

    if (stm_display_link_set_ctrl(hdmi->link, STM_DISPLAY_LINK_CTRL_DISPLAY_MODE, STM_DISPLAY_INVALID)<0)
      printk(KERN_ERR "HDMI manager can not set Display mode properly\n");

    if(hdmi->deferred_disconnect > 0)
    {
      /*
       * We were waiting to see if we could ignore the HPD pulse,
       * but it turns out the new EDID is not compatible with the
       * existing display mode so we now disable the output and signal
       * the hotplug attribute so applications can check the EDID change.
       */
      DPRINTK("Doing deferred output disable due to incompatible EDID\n");
      hdmi->disable = true;
      hdmi_sysfs_notify(&(hdmi->class_device->kobj), NULL, "hotplug");
    }

    hdmi_sysfs_notify(&(hdmi->class_device->kobj), NULL, "tmds_status");

    hdmi->deferred_disconnect = 0;
  }

}


static void stmhdmi_disconnect_display(struct stm_hdmi *hdmi)
{
  stmhdmi_invalidate_edid_info(hdmi);
  if(hdmi->hotplug_mode == STMHDMI_HPD_STOP_IF_NECESSARY)
  {
    DPRINTK("Deferring disconnect hdmi = %p\n",hdmi);
    /*
     * Wait for 3x 1/2 second timeouts to see if HPD gets re-asserted again
     * before really stopping the HDMI output. This is based on the time
     * a Yamaha RX-V1800 keeps hotplug de-asserted when a single TV connected
     * to it asserts its hotplug (approx 1s) plus some margin for us to respond.
     *
     * It is possible that AV receivers acting as repeaters will keep HPD
     * de-asserted while they authenticate all downstream devices, which can
     * take up to 4.2seconds (see HDCP specification) for the maximum repeater
     * depth. Do we want to wait that long?
     *
     */
    hdmi->deferred_disconnect = 3;
  }
  else
  {
    if(hdmi->hotplug_mode == STMHDMI_HPD_DISABLE_OUTPUT)
    {
      DPRINTK("Disabling HDMI Output hdmi = %p\n",hdmi);
      hdmi->disable = true;
    }

    hdmi_sysfs_notify(&(hdmi->class_device->kobj), NULL, "hotplug");
  }
}


static void stmhdmi_connect_display(struct stm_hdmi *hdmi)
{
  DPRINTK("HDMI Output connected\n");
 /*
  * We always re-read the EDID, as we might have received a hotplug
  * event just because the user changed the TV setup and the EDID
  * information has now changed.
  */
  if (hdmi->edid_updated)
    return;

  if(stmhdmi_read_edid(hdmi) != 0)
  {
    DPRINTK("EDID Read Error, setup safe EDID\n");
    stmhdmi_safe_edid(hdmi);
  }

  stmhdmi_dump_edid(hdmi);
  /*
   * Schedule the sending of the product identifier HDMI info frame.
   */
  if(hdmi->spd_frame->data[HDMI_SPD_INFOFRAME_SPI_OFFSET] != HDMI_SPD_INFOFRAME_SPI_DISABLE)
    (void)stm_display_output_queue_metadata(hdmi->hdmi_output, hdmi->spd_metadata);
  /*
   * Signal the hotplug sysfs attribute now the output is alive.
   */
  hdmi_sysfs_notify(&(hdmi->class_device->kobj), NULL, "hotplug");

}


static int stmhdmi_determine_link_connectivity(struct stm_hdmi *hdmi)
{
  int retVal  = 0;

  if ( (retVal = stm_display_link_hpd_get_state(hdmi->link, &hdmi->hpd_state)) != 0 )
  {
    printk(KERN_ERR "Error can't get HPD status\n");
    return retVal;
  }

  if ( (retVal = stm_display_link_rxsense_get_state(hdmi->link, &hdmi->rxsense_state)) != 0 )
  {
    printk(KERN_ERR "Error can't get RxSense status\n");
    return retVal;
  }

  return ((hdmi->hpd_state == STM_DISPLAY_LINK_HPD_STATE_HIGH) && (!hdmi->link_capability.rxsense || (hdmi->rxsense_state == STM_DISPLAY_LINK_RXSENSE_STATE_ACTIVE)));
}

int stmhdmi_manager(void *data)
{
  stm_event_info_t                       display_link_event[DISPLAY_LINK_EVT_NUMBER];
  uint32_t                               event_count;
  stm_event_subscription_h               display_link_event_subscription;
  stm_event_subscription_entry_t         display_link_event_subscription_entry ;
  int                                    n, retVal = 0;
  unsigned long                          saveFlags;
  struct stm_hdmi                       *hdmi = (struct stm_hdmi *)data;
  bool                                   is_suspended = false;
  stm_display_output_connection_status_t hdmi_status;
  bool                                   connexion_status_changed=false;



  DPRINTK("Start HDMI Manager\n");

  /*
   * Check the initial states of HPD, HDCP and Rx sense from display link
   */
  if ( (retVal = stm_display_link_hpd_get_state(hdmi->link, &hdmi->hpd_state)) != 0 )
  {
    printk(KERN_ERR "Error can't get HPD status");
    return retVal;
  }
  if ( (retVal = stm_display_link_rxsense_get_state(hdmi->link, &hdmi->rxsense_state)) != 0 )
  {
    printk(KERN_ERR "Error can't get RxSense status\n");
    return retVal;
  }

  /*
   * Subscribe to HPD, Rx sense and HDCP events
   */
  display_link_event_subscription_entry.object = (stm_object_h) hdmi->link;
  display_link_event_subscription_entry.event_mask =
      STM_DISPLAY_LINK_HPD_STATE_CHANGE_EVT |
      STM_DISPLAY_LINK_RXSENSE_STATE_CHANGE_EVT |
      STM_DISPLAY_LINK_HDCP_STATUS_CHANGE_EVT;
  display_link_event_subscription_entry.cookie = NULL;

  if ((retVal = stm_event_subscription_create(&display_link_event_subscription_entry, 1,
      &display_link_event_subscription))!=0)
  {
    printk(KERN_ERR "Error can't create event subscription\n");
    return retVal;
  }

  if ((retVal = stm_event_set_wait_queue(display_link_event_subscription, &hdmi->status_wait_queue, true))!=0)
  {
    printk(KERN_ERR "Error can't set wait queue event\n");
    return retVal;
  }

  set_freezable();

  DPRINTK("Starting HDMI Thread for info = %p\n",hdmi);

  while(1)
  {
    wait_event_freezable(hdmi->status_wait_queue,
                                ((stm_event_wait(display_link_event_subscription, 0 , 0, NULL, NULL)!= 0) ||
                                 (hdmi->status_changed != 0) || kthread_should_stop()));

    mutex_lock(&(hdmi->lock));

    if(kthread_should_stop())
    {
      printk(KERN_ERR "HDMI Thread terminating for info = %p\n",hdmi);
      stmhdmi_output_stop_hdcp(hdmi);
      stm_display_output_stop(hdmi->hdmi_output);
      hdmi->stop_output = true;
      if (stm_display_link_set_ctrl(hdmi->link, STM_DISPLAY_LINK_CTRL_DISPLAY_MODE, STM_DISPLAY_INVALID)<0)
        printk(KERN_ERR "HDMI manager can not set Display mode properly\n");
      mutex_unlock(&(hdmi->lock));
      break;
    }

    if ((retVal = stm_event_wait(display_link_event_subscription, 0 , DISPLAY_LINK_EVT_NUMBER, &event_count, display_link_event))==0)
    {
      printk(KERN_INFO " An event is retrieved :HPD/HDCP/RXsense event\n");
    }

   /*
    * Handle a real HDMI, HPD and HDCP state change depending on the received event
    */
    for (n=0;n<event_count;n++)
    {
      switch(display_link_event[n].event.event_id)
      {
        case STM_DISPLAY_LINK_HPD_STATE_CHANGE_EVT:
        {
          if ( (retVal = stm_display_link_hpd_get_state(hdmi->link, &hdmi->hpd_state)) != 0 )
          {
            printk(KERN_ERR "Error can't get HPD status\n");
          }
          DPRINTK("An event is retrieved :HPD event =%d\n", hdmi->hpd_state);
          hdmi_sysfs_notify(&(hdmi->class_device->kobj), NULL, "hotplug");

          if ( hdmi->hpd_state == STM_DISPLAY_LINK_HPD_STATE_HIGH)
          {
            stmhdmi_connect_display(hdmi);
            if ((retVal = send_event(hdmi, HDMI_EVENT_DISPLAY_CONNECTED & (~HDMI_EVENT_DISPLAY_PRIVATE_START)))!=0)
            {
                printk(KERN_ERR "Error can't send event\n");
            }
            if ((!hdmi->link_capability.rxsense) ||( hdmi->rxsense_state == STM_DISPLAY_LINK_RXSENSE_STATE_ACTIVE))
              stmhdmi_start_display(hdmi);
           }
          else
          {
            stmhdmi_disconnect_display(hdmi);
            if ((retVal = send_event(hdmi, HDMI_EVENT_DISPLAY_DISCONNECTED & (~HDMI_EVENT_DISPLAY_PRIVATE_START)))!=0)
            {
                printk(KERN_ERR "Error can't send event\n");
            }
            stmhdmi_stop_display(hdmi);
          }
          connexion_status_changed=true;
          break;
        }
        case STM_DISPLAY_LINK_RXSENSE_STATE_CHANGE_EVT:
        {
          if ( (retVal = stm_display_link_rxsense_get_state(hdmi->link, &hdmi->rxsense_state)) != 0 )
          {
            printk(KERN_ERR "Error can't get RxSense status\n");
          }
          DPRINTK("An event is retrieved :Rx sense event =%d\n",hdmi->rxsense_state );
          hdmi_sysfs_notify(&(hdmi->class_device->kobj), NULL, "rxsense_status");

          if ((hdmi->rxsense_state == STM_DISPLAY_LINK_RXSENSE_STATE_ACTIVE) && (hdmi->hpd_state == STM_DISPLAY_LINK_HPD_STATE_HIGH))
          {
             stmhdmi_start_display(hdmi);
          }
          else
          {
            stmhdmi_stop_display(hdmi);
          }
          connexion_status_changed=true;
          break;
        }
        case STM_DISPLAY_LINK_HDCP_STATUS_CHANGE_EVT:
        {
          if ((retVal = stm_display_link_hdcp_get_status(hdmi->link, &hdmi->hdcp_status)) != 0)
          {
            printk(KERN_ERR "Can't get HDCP status\n");
          }
          DPRINTK(" An event is retrieved :HDCP event =%d\n",hdmi->hdcp_status );
          hdmi_sysfs_notify(&(hdmi->class_device->kobj), NULL, "hdcp_status");

         /*
          * HDCP daemon has triggered HDCP authentication by another client entity
          * HDMI module has to switch corresponding booleans
          */
          if (hdmi->hdcp_status != STM_DISPLAY_LINK_HDCP_STATUS_DISABLED)
          {
            hdmi->hdcp_enable = true;
            hdmi->current_hdcp_status = true;
            hdmi->hdcp_stopped = false;
          }
          else if (!connexion_status_changed && !hdmi->status_changed && !hdmi->stop_output && !hdmi->hdcp_stopped)
          {
            DPRINTK("HDCP event is received change the booleans\n");
            /*
             * The user stops HDCP, neither in the context of HPD/Rx sense
             * nor in HDMI configuration change
             */
             hdmi->hdcp_enable = false;
             hdmi->current_hdcp_status = false;
          }
          connexion_status_changed=false;
          break;
        }
        default:
          break;
      }
    }

    /* Get the hdmi to check if it is suspended or not */
    is_suspended = false;
    if(stm_display_output_get_connection_status(hdmi->hdmi_output, &hdmi_status) < 0)
    {
      is_suspended = true;
    }


    if (((event_count ==0)||(display_link_event[0].event.event_id==STM_DISPLAY_LINK_HDCP_STATUS_CHANGE_EVT)) && likely(hdmi->status_changed) && !is_suspended)
    {
      if (stmhdmi_determine_link_connectivity(hdmi))
      {
        stmhdmi_connect_display(hdmi);
        stmhdmi_start_display(hdmi);
      }
    }

    spin_lock_irqsave(&(hdmi->spinlock), saveFlags);
    hdmi->status_changed = 0;
    spin_unlock_irqrestore(&(hdmi->spinlock), saveFlags);

    mutex_unlock(&(hdmi->lock));
    continue;
  }

  if ((retVal = stm_event_subscription_delete(display_link_event_subscription))!=0)
  {
    printk(KERN_ERR "Error can't delete event subscription\n");
    return retVal;
  }

  stmhdmi_output_stop_hdcp(hdmi);
  stm_display_output_stop(hdmi->hdmi_output);
  hdmi->stop_output = true;
  if (stm_display_link_set_ctrl(hdmi->link, STM_DISPLAY_LINK_CTRL_DISPLAY_MODE, STM_DISPLAY_INVALID)<0)
    printk(KERN_ERR "HDMI manager can not set Display mode properly\n");
  stmhdmi_disconnect_display(hdmi);
  hdmi_release_device(hdmi);
  return 0;
}

