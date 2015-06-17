/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/hdmi/hdmisysfs.c
 * Copyright (c) 2008-2012 STMicroelectronics Limited.
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
#include <linux/slab.h>
#include <linux/fb.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/kthread.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/wait.h>

#include <asm/uaccess.h>
#include <asm/irq.h>
#include <linux/semaphore.h>

#include <stm_display.h>
#include <linux/stm/stmcoredisplay.h>
#include <linux/stm/stmcorehdmi.h>

#include "stmhdmi.h"

/******************************************************************************
 * HDMI Sysfs implementation.
 */

/*
 * enumerate_dvi_modes is called when no display modes can be retrieved
 * from the display device (probably because the device uses DVI rather than
 * true HDMI). Basically we examine every display mode supported by the driver
 * to determine if it meets the refresh rate/pixel clock contraints reported by
 * the display device.
 */
static int enumerate_dvi_modes(struct stm_hdmi *hdmi, stm_display_mode_t *display_modes)
{
  int i;
  int num_modes = 0;

  unsigned int clock, max_clock;
  unsigned int vfreq, min_vfreq, max_vfreq;
  unsigned int hfreq, min_hfreq, max_hfreq;

  stm_display_mode_t  mode;

  mutex_lock(&hdmi->lock);

  max_clock = hdmi->edid_info.max_clock * 1000000;
  min_vfreq = hdmi->edid_info.min_vfreq * 1000;
  max_vfreq = hdmi->edid_info.max_vfreq * 1000;
  min_hfreq = hdmi->edid_info.min_hfreq * 1000000;
  max_hfreq = hdmi->edid_info.max_hfreq * 1000000;

  for (i=0; i<STM_TIMING_MODE_COUNT; i++) {
    if (stm_display_output_get_display_mode(hdmi->main_output, i, &mode)<0)
      continue;

    /*
     * Ignore non CEA861 or VESA modes
     */
    if((mode.mode_params.output_standards & (STM_OUTPUT_STD_CEA861 | STM_OUTPUT_STD_VESA_MASK)) == 0)
      continue;

    clock = mode.mode_timing.pixel_clock_freq;
    vfreq = mode.mode_params.vertical_refresh_rate;
    hfreq = vfreq * mode.mode_timing.lines_per_frame;
    if (STM_INTERLACED_SCAN == mode.mode_params.scan_type)
      hfreq /= 2;

    if (clock <= max_clock && min_vfreq <= vfreq && vfreq <= max_vfreq &&
                              min_hfreq <= hfreq && hfreq <= max_hfreq)
      display_modes[num_modes++] = mode;
  }

  mutex_unlock(&hdmi->lock);

  return num_modes;
}


/* convert an stm_display_mode_t to ASCII, the ASCII format is defined in
 * fbsysfs.c:mode_string()
 */
static int mode_string(char *buf, unsigned int offset, const stm_display_mode_t *mode)
{
  char m = 'U';
  char v = 'p';

  if (mode->mode_params.hdmi_vic_codes[STM_AR_INDEX_4_3] ||
      mode->mode_params.hdmi_vic_codes[STM_AR_INDEX_16_9]||
      mode->mode_params.hdmi_vic_codes[STM_AR_INDEX_64_27]) {
    m = 'S'; // a standard (meaning, in our case, CEA-861) mode
  }

  if (STM_INTERLACED_SCAN == mode->mode_params.scan_type) {
    v = 'i';
  }

  return snprintf(&buf[offset], PAGE_SIZE - offset, "%c:%ux%u%c-%u\n", m,
                  mode->mode_params.active_area_width,
      mode->mode_params.active_area_height,
      v,
      mode->mode_params.vertical_refresh_rate / 1000);
}


static int stmhdmi_is_suspended(struct stm_hdmi *hdmi)
{
  int ret = 0;
  stm_display_output_connection_status_t  current_status;

  ret = stm_display_output_get_connection_status(hdmi->hdmi_output, &current_status);

  return ret;
}

static ssize_t show_hdmi_aspect(struct device *dev, struct device_attribute *attr, char *buf)
{
  struct stm_hdmi *hdmi = dev_get_drvdata(dev);

  if(stmhdmi_is_suspended(hdmi) < 0)
    return 0;

#define L(x) case STM_WSS_ASPECT_RATIO_##x: return sprintf(buf, "%s\n", #x);
  switch(hdmi->edid_info.tv_aspect) {
  L(UNKNOWN)
  L(4_3)
  L(16_9)
  L(14_9)
  L(GT_16_9)
  }
#undef L

  return sprintf(buf, "UNKNOWN\n");
}


static ssize_t show_hdmi_cea861(struct device *dev, struct device_attribute *attr, char *buf)
{
  struct stm_hdmi *hdmi = dev_get_drvdata(dev);
  int i,sz;


  if(stmhdmi_is_suspended(hdmi) < 0)
    return 0;

  sz = 0;

  for(i=0;i<STM_MAX_CEA_MODES;i++)
  {
    switch(hdmi->edid_info.cea_codes[i].cea_code_descriptor)
    {
      case STM_CEA_VIDEO_CODE_UNSUPPORTED:
      default:
        break;
      case STM_CEA_VIDEO_CODE_NATIVE:
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "%d*\n", i);
        break;
      case STM_CEA_VIDEO_CODE_NON_NATIVE:
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "%d\n", i);
        break;
    }
  }

  return sz;
}


static ssize_t show_hdmi_3D_modes(struct device *dev, struct device_attribute *attr, char *buf)
{
  struct stm_hdmi *hdmi = dev_get_drvdata(dev);
  int i;
  int sz=0;

  if(stmhdmi_is_suspended(hdmi) < 0)
    return 0;

#define S(x) sz += snprintf(&buf[sz],PAGE_SIZE - sz, "%s", #x)

  for(i=0;i<STM_MAX_CEA_MODES;i++)
  {
    if(hdmi->edid_info.cea_codes[i].cea_code_descriptor == STM_CEA_VIDEO_CODE_UNSUPPORTED)
      continue;

    sz += snprintf(&buf[sz], PAGE_SIZE - sz, "%d", i);

    if ((hdmi->edid_info.cea_codes[i].supported_3d_flags & STM_MODE_FLAGS_3D_MASK) != 0)
    {
      if (hdmi->edid_info.cea_codes[i].supported_3d_flags & STM_MODE_FLAGS_3D_FRAME_PACKED)
        S(:3D_FRAME_PACKED);
      if (hdmi->edid_info.cea_codes[i].supported_3d_flags & STM_MODE_FLAGS_3D_FIELD_ALTERNATIVE)
        S(:3D_FIELD_ALTERNATIVE);
      if (hdmi->edid_info.cea_codes[i].supported_3d_flags & STM_MODE_FLAGS_3D_TOP_BOTTOM)
        S(:3D_TOP_BOTTOM);
      if (hdmi->edid_info.cea_codes[i].supported_3d_flags & STM_MODE_FLAGS_3D_SBS_HALF)
      {
        S(:3D_SBS_HALF);
        if (hdmi->edid_info.cea_codes[i].sbs_half_detail==0x0)
          S([ALL_SUB_SAMPLING]);
        if (hdmi->edid_info.cea_codes[i].sbs_half_detail==0x1)
          S([H_SUB_SAMPLING]);
        if (hdmi->edid_info.cea_codes[i].sbs_half_detail==0x6)
          S([ALL_QUINCUNX_MATRIX]);
        if (hdmi->edid_info.cea_codes[i].sbs_half_detail==0x7)
          S([QUINCUNX_MATRIX_OLP_ORP]);
        if (hdmi->edid_info.cea_codes[i].sbs_half_detail==0x8)
          S([QUINCUNX_MATRIX_OLP_ERP]);
        if (hdmi->edid_info.cea_codes[i].sbs_half_detail==0x9)
          S([QUINCUNX_MATRIX_ELP_ORP]);
        if (hdmi->edid_info.cea_codes[i].sbs_half_detail==0xA)
          S([QUINCUNX_MATRIX_ELP_ERP]);
      }
    }
    else
    {
      S(:2D);
    }

    S(\n);
  }
#undef S

  return sz;
}


static ssize_t show_hdmi_display_type(struct device *dev, struct device_attribute *attr, char *buf)
{
  struct stm_hdmi *hdmi = dev_get_drvdata(dev);

  if(stmhdmi_is_suspended(hdmi) < 0)
    return 0;

#define L(x) case STM_DISPLAY_##x: return sprintf(buf, "%s\n", #x);
  switch(hdmi->edid_info.display_type) {
  L(INVALID)
  L(DVI)
  L(HDMI)
  }
#undef L

  return sprintf(buf, "UNKNOWN\n");
}


static ssize_t show_hdmi_display_name(struct device *dev, struct device_attribute *attr, char *buf)
{
  struct stm_hdmi *hdmi = dev_get_drvdata(dev);

  if(stmhdmi_is_suspended(hdmi) < 0)
    return 0;

  if(hdmi->edid_info.monitor_name[0] != 0)
    return sprintf(buf, "%s\n", hdmi->edid_info.monitor_name);

  return sprintf(buf, "UNKNOWN\n");
}


static ssize_t show_rxsense_status(struct device *dev, struct device_attribute *attr, char *buf)
{
  stm_display_link_rxsense_state_t state;
  struct stm_hdmi *hdmi = dev_get_drvdata(dev);
  unsigned int sz = 0;
  int ret;

  if ((ret = stm_display_link_rxsense_get_state(hdmi->link, &state)) < 0)
    return ret;

#define S(x) sz += sprintf(&buf[sz], "%s", #x)
  if(state == STM_DISPLAY_LINK_RXSENSE_STATE_INACTIVE)
    S(Inactive);
  else if(state == STM_DISPLAY_LINK_RXSENSE_STATE_ACTIVE)
    S(Active);
  else
    S(Unknown);

  S(\n);
#undef S

  return sz;
}


static ssize_t show_hdmi_hotplug(struct device *dev, struct device_attribute *attr, char *buf)
{
  stm_display_link_hpd_state_t state;
  struct stm_hdmi *hdmi = dev_get_drvdata(dev);
  int ret;

  if ((ret = stm_display_link_hpd_get_state(hdmi->link, &state)) < 0)
    return ret;

  return sprintf(buf, "%c\n", (state==STM_DISPLAY_LINK_HPD_STATE_HIGH) ? 'y' : 'n');
}


static ssize_t __show_hdmi_modes(const stm_display_mode_t *display_modes, int num_modes, char *buf)
{
  unsigned int i, sz;

  for (i=0, sz=0; i<num_modes; i++) {
    sz += mode_string(buf, sz, &display_modes[i]);
  }

  return sz;
}


static ssize_t show_hdmi_modes(struct device *dev, struct device_attribute *attr, char *buf)
{
  struct stm_hdmi *hdmi = dev_get_drvdata(dev);
  ssize_t sz;

  if(stmhdmi_is_suspended(hdmi) < 0)
    return 0;

  /* if no modes were retrieved from the display device then we must
   * infer them from the display characteristics.
   */
  if ((hdmi->edid_info.display_type == STM_DISPLAY_DVI) && (hdmi->edid_info.num_modes <= 1))
  {
    stm_display_mode_t *display_modes;
    int num_modes;

    display_modes = kmalloc(sizeof(stm_display_mode_t)*STM_TIMING_MODE_COUNT, GFP_KERNEL);
    if(!display_modes)
      return 0;

    num_modes = enumerate_dvi_modes(hdmi, display_modes);
    sz = __show_hdmi_modes(display_modes, num_modes, buf);
    kfree(display_modes);
    return sz;
  }

  return __show_hdmi_modes(hdmi->edid_info.display_modes,
         hdmi->edid_info.num_modes, buf);
}


static ssize_t show_hdmi_speaker_allocation(struct device *dev, struct device_attribute *attr, char *buf)
{
  struct stm_hdmi *hdmi = dev_get_drvdata(dev);
  int sz = 0;

  if(stmhdmi_is_suspended(hdmi) < 0)
    return 0;

  mutex_lock(&hdmi->lock);

#define S(x) sz += sprintf(&buf[sz], "%s", #x)

  if(hdmi->edid_info.speaker_allocation & STM_CEA_SPEAKER_FLFR)
    S(FL/FR);

  if(hdmi->edid_info.speaker_allocation & STM_CEA_SPEAKER_LFE)
    S(:LFE);

  if(hdmi->edid_info.speaker_allocation & STM_CEA_SPEAKER_FC)
    S(:FC);

  if(hdmi->edid_info.speaker_allocation & STM_CEA_SPEAKER_RLRR)
    S(:RL/RR);

  if(hdmi->edid_info.speaker_allocation & STM_CEA_SPEAKER_RC)
    S(:RC);

  if(hdmi->edid_info.speaker_allocation & STM_CEA_SPEAKER_FLCFRC)
    S(:FLC/FRC);

  if(hdmi->edid_info.speaker_allocation & STM_CEA_SPEAKER_RLCRRC)
    S(:RLC/RRC);

  if(hdmi->edid_info.speaker_allocation & STM_CEA_SPEAKER_FLWFRW)
    S(:FLW/FRW);

  if(hdmi->edid_info.speaker_allocation & STM_CEA_SPEAKER_FLHFRH)
    S(:FLH/FRH);

  if(hdmi->edid_info.speaker_allocation & STM_CEA_SPEAKER_TC)
    S(:TC);

  if(hdmi->edid_info.speaker_allocation & STM_CEA_SPEAKER_FCH)
    S(:FCH);

  S(\n);
#undef S

  mutex_unlock(&hdmi->lock);

  return sz;
}


static char *audio_formats[STM_CEA_AUDIO_LAST+1] = {
  "Reserved",
  "LPCM",
  "AC3",
  "MPEG1(L1/2)",
  "MP3",
  "MPEG2",
  "AAC",
  "DTS",
  "ATRAC",
  "One Bit Audio",
  "Dolby Digital+",
  "DTS-HD",
  "MLP",
  "DST",
  "WMA Pro",
  "Reserved"
};

static char *ex_audio_formats[STM_CEA_AUDIO_EX_LAST+1] = {
  "Reserved",
  "HE-ACC",
  "HE-ACCv2",
  "MPEG Surround",
};

static ssize_t show_hdmi_audio_mode(struct stm_cea_audio_descriptor *mode, char *buf)
{
  int sz = 0;

  if(mode->format < STM_CEA_AUDIO_LAST)
    sz += sprintf(buf,"%u:%s",mode->format, audio_formats[mode->format]);
  else
    sz += sprintf(buf,"%u.%u:%s",mode->format, mode->ex_format, ex_audio_formats[mode->ex_format]);

  sz += sprintf(&buf[sz],":%uch",mode->max_channels);

#define S(x) sz += sprintf(&buf[sz], "%s", #x)

  if(mode->sample_rates & STM_CEA_AUDIO_RATE_32_KHZ)
    S(:32KHz);

  if(mode->sample_rates & STM_CEA_AUDIO_RATE_44_1_KHZ)
    S(:44.1KHz);

  if(mode->sample_rates & STM_CEA_AUDIO_RATE_48_KHZ)
    S(:48KHz);

  if(mode->sample_rates & STM_CEA_AUDIO_RATE_88_2_KHZ)
    S(:88.2KHz);

  if(mode->sample_rates & STM_CEA_AUDIO_RATE_96_KHZ)
    S(:96KHz);

  if(mode->sample_rates & STM_CEA_AUDIO_RATE_176_4_KHZ)
    S(:176.4KHz);

  if(mode->sample_rates & STM_CEA_AUDIO_RATE_192_KHZ)
    S(:192KHz);

  if(mode->format == STM_CEA_AUDIO_LPCM)
  {
    if(mode->lpcm_bit_depths & 0x1)
      S(:16bit);
    if(mode->lpcm_bit_depths & 0x2)
      S(:20bit);
    if(mode->lpcm_bit_depths & 0x4)
      S(:24bit);
  }
#undef S

  if(mode->format >= STM_CEA_AUDIO_AC3 && mode->format <= STM_CEA_AUDIO_ATRAC)
    sz += sprintf(&buf[sz], ":%ukbps", mode->max_bit_rate);

  if(mode->format == STM_CEA_AUDIO_WMA_PRO)
    sz += sprintf(&buf[sz], ":profile-%u", mode->profile);

  sz += sprintf(&buf[sz],"\n");

  return sz;
}


static ssize_t show_hdmi_audio_capabilities(struct device *dev, struct device_attribute *attr, char *buf)
{
  struct stm_hdmi *hdmi = dev_get_drvdata(dev);
  int sz = 0;
  stm_cea_audio_formats_t fmt;

  if(stmhdmi_is_suspended(hdmi) < 0)
    return 0;

  mutex_lock(&hdmi->lock);

  for(fmt = STM_CEA_AUDIO_LPCM;fmt<STM_CEA_AUDIO_LAST+1;fmt++) {
    if(hdmi->edid_info.audio_modes[fmt].format != 0)
      sz += show_hdmi_audio_mode(&hdmi->edid_info.audio_modes[fmt], &buf[sz]);
  }

  mutex_unlock(&hdmi->lock);

  return sz;
}


static ssize_t show_cec_address(struct device *dev, struct device_attribute *attr, char *buf)
{
  struct stm_hdmi *hdmi = dev_get_drvdata(dev);
  int sz = 0;

  if(stmhdmi_is_suspended(hdmi) < 0)
    return 0;

  mutex_lock(&hdmi->lock);

  sz += sprintf(buf,"%x.%x.%x.%x\n",hdmi->edid_info.cec_address[0],
                                    hdmi->edid_info.cec_address[1],
                                    hdmi->edid_info.cec_address[2],
                                    hdmi->edid_info.cec_address[3]);

  mutex_unlock(&hdmi->lock);

  return sz;
}


static ssize_t show_hdmi_supports_ai(struct device *dev, struct device_attribute *attr, char *buf)
{
  struct stm_hdmi *hdmi = dev_get_drvdata(dev);
  int sz = 0;

  if(stmhdmi_is_suspended(hdmi) < 0)
    return 0;

  mutex_lock(&hdmi->lock);

  sz += sprintf(buf,"%s\n",(hdmi->edid_info.hdmi_vsdb_flags&STM_HDMI_VSDB_SUPPORTS_AI)?"y":"n");

  mutex_unlock(&hdmi->lock);

  return sz;
}


static char *scan_support[STM_CEA_VCDB_CE_SCAN_BOTH+1] = {
  "Unsupported",
  "Overscanned",
  "Underscanned",
  "Selectable",
};

static char *ptscan_support[STM_CEA_VCDB_CE_SCAN_BOTH+1] = {
  "Undefined",
  "Overscanned",
  "Underscanned",
  "Selectable",
};

static ssize_t show_hdmi_underscan(struct device *dev, struct device_attribute *attr, char *buf)
{
  struct stm_hdmi *hdmi = dev_get_drvdata(dev);
  int sz = 0;

  if(stmhdmi_is_suspended(hdmi) < 0)
    return 0;

  mutex_lock(&hdmi->lock);

  sz += sprintf(buf,"CEA:%s\n",(hdmi->edid_info.cea_capabilities&STM_CEA_CAPS_UNDERSCAN)?"Underscanned":"Overscanned");
  sz += sprintf(&buf[sz],"VCDB:%s",scan_support[hdmi->edid_info.cea_vcdb_flags & 0x3]);
  sz += sprintf(&buf[sz],":%s",scan_support[(hdmi->edid_info.cea_vcdb_flags >> 2) & 0x3]);
  sz += sprintf(&buf[sz],":%s\n",ptscan_support[(hdmi->edid_info.cea_vcdb_flags >> 4) & 0x3]);

  mutex_unlock(&hdmi->lock);

  return sz;
}


static char *quantization[4] = {
  "Unsupported",
  "RGB",
  "YCC",
  "RGB:YCC",
};

static ssize_t show_hdmi_quantization(struct device *dev, struct device_attribute *attr, char *buf)
{
  struct stm_hdmi *hdmi = dev_get_drvdata(dev);
  int sz = 0;

  if(stmhdmi_is_suspended(hdmi) < 0)
    return 0;

  mutex_lock(&hdmi->lock);

  sz += sprintf(buf,"%s\n",quantization[((hdmi->edid_info.cea_vcdb_flags>>6)&0x3)]);

  mutex_unlock(&hdmi->lock);

  return sz;
}


static ssize_t show_hdmi_colorspace(struct device *dev, struct device_attribute *attr, char *buf)
{
  struct stm_hdmi *hdmi = dev_get_drvdata(dev);
  int sz = 0;

  if(stmhdmi_is_suspended(hdmi) < 0)
    return 0;

  mutex_lock(&hdmi->lock);

#define S(x) sz += sprintf(&buf[sz], "%s", #x)
#define C(x) if(sz != 0) sz += sprintf(&buf[sz], "%s", #x)

  if(hdmi->edid_info.cea_capabilities&STM_CEA_CAPS_SRGB)
    S(sRGB);
  if(hdmi->edid_info.cea_capabilities&(STM_CEA_CAPS_YUV|STM_CEA_CAPS_422))
  {
    C(:);
    S(Ycc601:Ycc709);
  }
  if(hdmi->edid_info.cea_coldb_flags&STM_CEA_COLDB_XVYCC601)
  {
    C(:);
    S(xvYcc601);
  }
  if(hdmi->edid_info.cea_coldb_flags&STM_CEA_COLDB_XVYCC709)
  {
    C(:);
    S(xvYcc709);
  }
  if(hdmi->edid_info.cea_coldb_flags&STM_CEA_COLDB_SYCC601)
  {
    C(:);
    S(sYcc601);
  }
  if(hdmi->edid_info.cea_coldb_flags&STM_CEA_COLDB_ADOBEYCC601)
  {
    C(:);
    S(AdobeYcc601);
  }
  if(hdmi->edid_info.cea_coldb_flags&STM_CEA_COLDB_ADOBERGB)
  {
    C(:);
    S(AdobeRGB);
  }

  S(\n);
#undef S
#undef C

  mutex_unlock(&hdmi->lock);

  return sz;
}


static ssize_t show_hdmi_colorgamut_profiles(struct device *dev, struct device_attribute *attr, char *buf)
{
  struct stm_hdmi *hdmi = dev_get_drvdata(dev);
  int sz = 0;

  if(stmhdmi_is_suspended(hdmi) < 0)
    return 0;

  mutex_lock(&hdmi->lock);

#define S(x) sz += sprintf(&buf[sz], "%s", #x)
#define C(x) if(sz != 0) sz += sprintf(&buf[sz], "%s", #x)

  if(hdmi->edid_info.cea_coldb_flags&STM_CEA_COLDB_MD_PROFILE0)
    S(0);
  if(hdmi->edid_info.cea_coldb_flags&STM_CEA_COLDB_MD_PROFILE1)
  {
    C(:);
    S(1);
  }
  if(hdmi->edid_info.cea_coldb_flags&STM_CEA_COLDB_MD_PROFILE2)
  {
    C(:);
    S(2);
  }
  if(hdmi->edid_info.cea_coldb_flags&STM_CEA_COLDB_MD_PROFILE3)
  {
    C(:);
    S(3);
  }

  if(sz==0)
    S(none);

  S(\n);
#undef S
#undef C

  mutex_unlock(&hdmi->lock);

  return sz;
}


static ssize_t show_hdmi_colorformat(struct device *dev, struct device_attribute *attr, char *buf)
{
  struct stm_hdmi *hdmi = dev_get_drvdata(dev);
  int sz = 0;

  if(stmhdmi_is_suspended(hdmi) < 0)
    return 0;

  mutex_lock(&hdmi->lock);

#define S(x) sz += sprintf(&buf[sz], "%s", #x)

  if(hdmi->edid_info.cea_capabilities&STM_CEA_CAPS_RGB)
    S(RGB:24);

  if(hdmi->edid_info.hdmi_vsdb_flags&STM_HDMI_VSDB_DC_30BIT)
    S(:30);

  if(hdmi->edid_info.hdmi_vsdb_flags&STM_HDMI_VSDB_DC_36BIT)
    S(:36);

  if(hdmi->edid_info.hdmi_vsdb_flags&STM_HDMI_VSDB_DC_48BIT)
    S(:48);

  S(\n);

  if(hdmi->edid_info.cea_capabilities&STM_CEA_CAPS_422)
    S(YUV422:24\n);

  if(hdmi->edid_info.cea_capabilities&STM_CEA_CAPS_422)
    S(YUV444:24);

  if(hdmi->edid_info.hdmi_vsdb_flags&STM_HDMI_VSDB_DC_Y444)
  {
    if(hdmi->edid_info.hdmi_vsdb_flags&STM_HDMI_VSDB_DC_30BIT)
      S(:30);

    if(hdmi->edid_info.hdmi_vsdb_flags&STM_HDMI_VSDB_DC_36BIT)
      S(:36);

    if(hdmi->edid_info.hdmi_vsdb_flags&STM_HDMI_VSDB_DC_48BIT)
      S(:48);
  }

  S(\n);

#undef S

  mutex_unlock(&hdmi->lock);

  return sz;
}


static ssize_t show_hdmi_avlatency(struct device *dev, struct device_attribute *attr, char *buf)
{
  struct stm_hdmi *hdmi = dev_get_drvdata(dev);
  int sz = 0;

  if(stmhdmi_is_suspended(hdmi) < 0)
    return 0;

  mutex_lock(&hdmi->lock);

#define S(x) sz += sprintf(&buf[sz], "%s", #x)


#define X(x) {if((x)==255) sz += sprintf(&buf[sz], "No output\n"); \
  else if (((x)>0) && ((x)<=251)) sz += sprintf(&buf[sz], "%d\n",((x)-1)*2); \
  else sz += sprintf(&buf[sz], "%d\n",0);}

  S(V:);X(hdmi->edid_info.progressive_video_latency);
  S(A:);X(hdmi->edid_info.progressive_audio_latency);
  S(IV:);X(hdmi->edid_info.interlaced_video_latency);
  S(IA:);X(hdmi->edid_info.interlaced_audio_latency);

#undef X
#undef S

  mutex_unlock(&hdmi->lock);

  return sz;
}


static ssize_t show_tmds_status(struct device *dev, struct device_attribute *attr, char *buf)
{
  struct stm_hdmi *hdmi = dev_get_drvdata(dev);
  stm_display_mode_t current_hdmi_mode = { STM_TIMING_MODE_RESERVED };
  int sz = 0;

  mutex_lock(&hdmi->lock);


#define S(x) sz += sprintf(&buf[sz], "%s", #x)
  if(stmhdmi_is_suspended(hdmi) < 0)
    S(Suspended);
  else if(stm_display_output_get_current_display_mode(hdmi->hdmi_output,&current_hdmi_mode) == 0)
    S(Running);
  else
    S(Stopped);

  S(\n);
#undef S

  mutex_unlock(&hdmi->lock);

  return sz;
}

static struct device_attribute stmhdmi_device_attrs[] = {
__ATTR(aspect, S_IRUGO, show_hdmi_aspect, NULL),
__ATTR(cea861_codes, S_IRUGO, show_hdmi_cea861, NULL),
__ATTR(type, S_IRUGO, show_hdmi_display_type, NULL),
__ATTR(name, S_IRUGO, show_hdmi_display_name, NULL),
__ATTR(hotplug, S_IRUGO, show_hdmi_hotplug, NULL),
__ATTR(modes, S_IRUGO, show_hdmi_modes, NULL),
__ATTR(speaker_allocation, S_IRUGO, show_hdmi_speaker_allocation, NULL),
__ATTR(audio_capabilities, S_IRUGO, show_hdmi_audio_capabilities, NULL),
__ATTR(cec_address, S_IRUGO, show_cec_address, NULL),
__ATTR(supports_ai, S_IRUGO, show_hdmi_supports_ai, NULL),
__ATTR(underscan, S_IRUGO, show_hdmi_underscan, NULL),
__ATTR(quantization_control, S_IRUGO, show_hdmi_quantization, NULL),
__ATTR(colorspaces, S_IRUGO, show_hdmi_colorspace, NULL),
__ATTR(color_format, S_IRUGO, show_hdmi_colorformat, NULL),
__ATTR(colorgamut_profiles, S_IRUGO, show_hdmi_colorgamut_profiles, NULL),
__ATTR(av_latency, S_IRUGO, show_hdmi_avlatency, NULL),
__ATTR(tmds_status, S_IRUGO, show_tmds_status, NULL),
__ATTR(modes_3d, S_IRUGO, show_hdmi_3D_modes, NULL),
__ATTR(rxsense_status, S_IRUGO, show_rxsense_status, NULL)
};

int stmhdmi_create_class_device_files(struct stm_hdmi *hdmi, struct device *display_class_device)
{
  int i,ret;

  for (i = 0; i < ARRAY_SIZE(stmhdmi_device_attrs); i++) {
    ret = device_create_file(hdmi->class_device, &stmhdmi_device_attrs[i]);
    if(ret)
      break;
  }

  return ret;
}
