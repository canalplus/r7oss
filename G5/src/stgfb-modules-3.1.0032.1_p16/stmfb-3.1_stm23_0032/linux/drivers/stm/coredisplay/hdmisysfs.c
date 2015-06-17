/***********************************************************************
 *
 * File: linux/drivers/stm/coredisplay/hdmisysfs.c
 * Copyright (c) 2008 STMicroelectronics Limited.
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
#include <asm/semaphore.h>

#include <stmdisplay.h>
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
static int enumerate_dvi_modes(struct stm_hdmi *hdmi, const stm_mode_line_t **display_modes)
{
  int i;
  int num_modes = 0;

  unsigned int clock, max_clock;
  unsigned int vfreq, min_vfreq, max_vfreq;
  unsigned int hfreq, min_hfreq, max_hfreq;
  const stm_mode_line_t *mode;

  mutex_lock(&hdmi->lock);

  max_clock = hdmi->edid_info.max_clock * 1000000;
  min_vfreq = hdmi->edid_info.min_vfreq * 1000;
  max_vfreq = hdmi->edid_info.max_vfreq * 1000;
  min_hfreq = hdmi->edid_info.min_hfreq * 1000000;
  max_hfreq = hdmi->edid_info.max_hfreq * 1000000;

  for (i=0; i<STVTG_TIMING_MODE_COUNT; i++) {
    if (!(mode = stm_display_output_get_display_mode(hdmi->main_output, i)))
      continue;

    clock = mode->TimingParams.ulPixelClock;
    vfreq = mode->ModeParams.FrameRate;
    hfreq = vfreq * mode->TimingParams.LinesByFrame;
    if (SCAN_I == mode->ModeParams.ScanType)
      hfreq /= 2;

    if (clock <= max_clock && min_vfreq <= vfreq && vfreq <= max_vfreq &&
                              min_hfreq <= hfreq && hfreq <= max_hfreq)
      display_modes[num_modes++] = mode;
  }

  mutex_unlock(&hdmi->lock);

  return num_modes;
}


/* convert an stm_mode_line_t to ASCII, the ASCII format is defined in
 * fbsysfs.c:mode_string()
 */
static int mode_string(char *buf, unsigned int offset, const stm_mode_line_t *mode)
{
  char m = 'U';
  char v = 'p';

  if (mode->ModeParams.HDMIVideoCodes[AR_INDEX_4_3] ||
      mode->ModeParams.HDMIVideoCodes[AR_INDEX_16_9]) {
    m = 'S'; // a standard (meaning, in our case, CEA-861) mode
  }

  if (SCAN_I == mode->ModeParams.ScanType) {
    v = 'i';
  }

  return snprintf(&buf[offset], PAGE_SIZE - offset, "%c:%lux%lu%c-%lu\n", m,
                  mode->ModeParams.ActiveAreaWidth,
      mode->ModeParams.ActiveAreaHeight,
      v,
      mode->ModeParams.FrameRate / 1000);
}


static ssize_t show_hdmi_aspect(struct class_device *class_device, char *buf)
{
  struct stm_hdmi *hdmi = class_get_devdata(class_device);

#define L(x) case STM_WSS_##x: return sprintf(buf, "%s\n", #x);
  switch(hdmi->edid_info.tv_aspect) {
  L(OFF)
  L(4_3)
  L(16_9)
  L(14_9)
  L(GT_16_9)
  }
#undef L

  return sprintf(buf, "UNKNOWN\n");
}


static ssize_t show_hdmi_cea861(struct class_device *class_device, char *buf)
{
  struct stm_hdmi *hdmi = class_get_devdata(class_device);
  int i,sz;

  sz = 0;

  for(i=0;i<STM_MAX_CEA_MODES;i++)
  {
    switch(hdmi->edid_info.cea_codes[i])
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


static ssize_t show_hdmi_display_type(struct class_device *class_device, char *buf)
{
  struct stm_hdmi *hdmi = class_get_devdata(class_device);

#define L(x) case STM_DISPLAY_##x: return sprintf(buf, "%s\n", #x);
  switch(hdmi->edid_info.display_type) {
  L(INVALID)
  L(DVI)
  L(HDMI)
  }
#undef L

  return sprintf(buf, "UNKNOWN\n");
}

static ssize_t show_hdmi_manufacturer(struct class_device *class_device, char *buf)
{
  struct stm_hdmi *hdmi = class_get_devdata(class_device);
  unsigned int mid = hdmi->edid_info.manufacturer_id;
  char c1, c2, c3;

  mid = (mid & 0xff) << 8 | (mid & 0xff00) >> 8;
  c1 = 'A' - 1 + (unsigned)((mid >> 10) & 0x1f);
  c2 = 'A' - 1 + (unsigned)((mid >> 5) & 0x1f);
  c3 = 'A' - 1 + (unsigned)((mid & 0x1f));

  return sprintf(buf, "%c%c%c\n", c1, c2, c3);
}


static ssize_t show_hdmi_display_name(struct class_device *class_device, char *buf)
{
  struct stm_hdmi *hdmi = class_get_devdata(class_device);

  if(hdmi->edid_info.monitor_name[0] != 0)
    return sprintf(buf, "%s\n", hdmi->edid_info.monitor_name);

  return sprintf(buf, "UNKNOWN\n");
}


static ssize_t show_hdmi_hotplug(struct class_device *class_device, char *buf)
{
  struct stm_hdmi *hdmi = class_get_devdata(class_device);

  return sprintf(buf, "%c\n", hdmi->status ? 'y' : 'n');
}


static ssize_t __show_hdmi_modes(const stm_mode_line_t **display_modes, int num_modes, char *buf)
{
  unsigned int i, sz;

  for (i=0, sz=0; i<num_modes; i++) {
    sz += mode_string(buf, sz, display_modes[i]);
  }

  return sz;
}


static ssize_t show_hdmi_modes(struct class_device *class_device, char *buf)
{
  struct stm_hdmi *hdmi = class_get_devdata(class_device);

  /* if no modes were retrieved from the display device then we must
   * infer them from the display characteristics.
   */
  if (0 == hdmi->edid_info.num_modes) {
    const stm_mode_line_t *display_modes[STVTG_TIMING_MODE_COUNT];
    int num_modes;
    num_modes = enumerate_dvi_modes(hdmi, display_modes);
    return __show_hdmi_modes(display_modes, num_modes, buf);
  }

  return __show_hdmi_modes(hdmi->edid_info.display_modes,
         hdmi->edid_info.num_modes, buf);
}


static ssize_t show_hdmi_speaker_allocation(struct class_device *class_device, char *buf)
{
  struct stm_hdmi *hdmi = class_get_devdata(class_device);
  int sz = 0;

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


static ssize_t show_hdmi_audio_capabilities(struct class_device *class_device, char *buf)
{
  struct stm_hdmi *hdmi = class_get_devdata(class_device);
  int sz = 0;
  stm_cea_audio_formats_t fmt;

  mutex_lock(&hdmi->lock);

  for(fmt = STM_CEA_AUDIO_LPCM;fmt<STM_CEA_AUDIO_LAST+1;fmt++) {
    if(hdmi->edid_info.audio_modes[fmt].format != 0)
      sz += show_hdmi_audio_mode(&hdmi->edid_info.audio_modes[fmt], &buf[sz]);
  }

  mutex_unlock(&hdmi->lock);

  return sz;
}


static ssize_t show_cec_address(struct class_device *class_device, char *buf)
{
  struct stm_hdmi *hdmi = class_get_devdata(class_device);
  int sz = 0;

  mutex_lock(&hdmi->lock);

  sz += sprintf(buf,"%x.%x.%x.%x\n",hdmi->edid_info.cec_address[0],
                                    hdmi->edid_info.cec_address[1],
                                    hdmi->edid_info.cec_address[2],
                                    hdmi->edid_info.cec_address[3]);

  mutex_unlock(&hdmi->lock);

  return sz;
}


static ssize_t show_hdmi_supports_ai(struct class_device *class_device, char *buf)
{
  struct stm_hdmi *hdmi = class_get_devdata(class_device);
  int sz = 0;

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

static ssize_t show_hdmi_underscan(struct class_device *class_device, char *buf)
{
  struct stm_hdmi *hdmi = class_get_devdata(class_device);
  int sz = 0;

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

static ssize_t show_hdmi_quantization(struct class_device *class_device, char *buf)
{
  struct stm_hdmi *hdmi = class_get_devdata(class_device);
  int sz = 0;

  mutex_lock(&hdmi->lock);

  sz += sprintf(buf,"%s\n",quantization[((hdmi->edid_info.cea_vcdb_flags>>6)&0x3)]);

  mutex_unlock(&hdmi->lock);

  return sz;
}


static ssize_t show_hdmi_colorspace(struct class_device *class_device, char *buf)
{
  struct stm_hdmi *hdmi = class_get_devdata(class_device);
  int sz = 0;

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


static ssize_t show_hdmi_colorgamut_profiles(struct class_device *class_device, char *buf)
{
  struct stm_hdmi *hdmi = class_get_devdata(class_device);
  int sz = 0;

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


static ssize_t show_hdmi_colorformat(struct class_device *class_device, char *buf)
{
  struct stm_hdmi *hdmi = class_get_devdata(class_device);
  int sz = 0;

  mutex_lock(&hdmi->lock);

#define S(x) sz += sprintf(&buf[sz], "%s", #x)

  if(hdmi->edid_info.cea_capabilities&STM_CEA_CAPS_SRGB)
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


static ssize_t show_hdmi_avlatency(struct class_device *class_device, char *buf)
{
  struct stm_hdmi *hdmi = class_get_devdata(class_device);
  int sz = 0;

  mutex_lock(&hdmi->lock);

#define S(x) sz += sprintf(&buf[sz], "%s", #x)
#define X(x) {if((x)==0) sz += sprintf(&buf[sz], "Unknown\n"); \
  else if((x)==255) sz += sprintf(&buf[sz], "No output\n"); \
  else sz += sprintf(&buf[sz], "%d\n",((x)-1)*2);}

  S(V:);X(hdmi->edid_info.progressive_video_latency);
  S(A:);X(hdmi->edid_info.progressive_audio_latency);
  S(IV:);X(hdmi->edid_info.interlaced_video_latency);
  S(IA:);X(hdmi->edid_info.interlaced_audio_latency);

#undef X
#undef S

  mutex_unlock(&hdmi->lock);

  return sz;
}


static ssize_t show_tmds_status(struct class_device *class_device, char *buf)
{
  struct stm_hdmi *hdmi = class_get_devdata(class_device);
  int sz = 0;

  mutex_lock(&hdmi->lock);

#define S(x) sz += sprintf(&buf[sz], "%s", #x)
  if(stm_display_output_get_current_display_mode(hdmi->hdmi_output) != NULL)
    S(Running);
  else
    S(Stopped);

  S(\n);
#undef S

  mutex_unlock(&hdmi->lock);

  return sz;
}


static struct class_device_attribute stmhdmi_device_attrs[] = {
__ATTR(aspect, S_IRUGO, show_hdmi_aspect, NULL),
__ATTR(cea861_codes, S_IRUGO, show_hdmi_cea861, NULL),
__ATTR(type, S_IRUGO, show_hdmi_display_type, NULL),
__ATTR(manufacturer, S_IRUGO, show_hdmi_manufacturer, NULL),
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
__ATTR(tmds_status, S_IRUGO, show_tmds_status, NULL)
};

int __init stmhdmi_init_class_device(struct stm_hdmi *hdmi, struct stmcore_display_pipeline_data *platform_data)
{
  int i,ret;

  hdmi->class_device = class_device_create(platform_data->class_device->class,
                                           platform_data->class_device,
                                           hdmi->cdev.dev,
                                           NULL,kobject_name(&hdmi->cdev.kobj));

  if(IS_ERR(hdmi->class_device))
  {
    hdmi->class_device = NULL;
    return -ENODEV;
  }

  class_set_devdata(hdmi->class_device,hdmi);

  for (i = 0; i < ARRAY_SIZE(stmhdmi_device_attrs); i++) {
    ret = class_device_create_file(hdmi->class_device, &stmhdmi_device_attrs[i]);
    if(ret)
      break;
  }

  return ret;
}
