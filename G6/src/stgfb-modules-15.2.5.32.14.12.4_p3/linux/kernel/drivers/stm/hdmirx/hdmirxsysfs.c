/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/hdmirx/hdmirxsysfs.c
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
#include <linux/slab.h>
#include <linux/fb.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/kthread.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/wait.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <linux/semaphore.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/device.h>

#include "hdmirxplatform.h"
#include <stddefs.h>
#include <stm_hdmirx.h>
#include <hdmirx_drv.h>
#include <vibe_os.h>

static ssize_t stm_hdmirx_show_signal_info(struct device *dev,
                                       struct device_attribute *attr, char *buf);
static ssize_t stm_hdmirx_show_timing_info(struct device *dev,
                                       struct device_attribute *attr, char *buf);
static ssize_t stm_hdmirx_show_video_info(struct device *dev,
                                       struct device_attribute *attr, char *buf);
static ssize_t stm_hdmirx_show_audio_info(struct device *dev,
                                       struct device_attribute *attr, char *buf);
static ssize_t stm_hdmirx_change_port_hpd_signal(struct device *dev,
                                       struct device_attribute *attr, const char *buf, size_t sz);
#ifndef CONFIG_ARCH_STI
int __init stmhdmirx_class_device_register(struct class *stminput_class, struct platform_device *pdev);
#else
int stmhdmirx_class_device_register(struct class *stminput_class, struct platform_device *pdev);
#endif
void __exit stmhdmirx_class_device_unregister(struct platform_device *pdev);

struct stmhdmirx_device_attribute{
        struct device_attribute dev_attr;
        U32             id;
};

#define to_stmhdmirx_dev_attr(_dev_attr)                               \
        container_of(_dev_attr, struct stmhdmirx_device_attribute, dev_attr)

#define HDMIRX_DEVICE_ATTR(_name, _id, _mode, _show, _store){          \
         .dev_attr = __ATTR(_name, _mode, _show, _store),              \
         .id  = _id }

static struct device *port_class_device[4], *route_class_device[2], *hdmirx_class_device;

static ssize_t stm_hdmirx_show_signal_info(struct device *dev,
                                       struct device_attribute *attr, char *buf)
{
  ssize_t sz = 0;
  stm_hdmirx_signal_property_t signal_property;
  stm_hdmirx_route_signal_status_t signal_status;
  struct platform_device *pdev = dev_get_drvdata(dev);
  struct stmhdmirx_device_attribute *route_attr = to_stmhdmirx_dev_attr(attr);
  hdmirx_dev_handle_t *dHandle_p = &dev_handle[pdev->id];

  if(dHandle_p->Handle==HDMIRX_INVALID_DEVICE_HANDLE)
    return -EINVAL;

  if(dHandle_p->RouteHandle[route_attr->id].Handle != HDMIRX_INVALID_ROUTE_HANDLE)
  {
      if(stm_hdmirx_route_get_signal_property(dHandle_p->RouteHandle[route_attr->id].Handle, &signal_property)<0)
        return sz;
      sz += snprintf(&buf[sz], PAGE_SIZE - sz, "type %s\n", (signal_property.signal_type==STM_HDMIRX_SIGNAL_TYPE_HDMI)?"HDMI":"DVI");
      if(stm_hdmirx_route_get_signal_status(dHandle_p->RouteHandle[route_attr->id].Handle, &signal_status)<0)
        return sz;
      sz += snprintf(&buf[sz], PAGE_SIZE - sz, "state %s\n", (signal_status==STM_HDMIRX_ROUTE_SIGNAL_STATUS_PRESENT)?"Stable":"Unstable");
  }

  return sz;
}

static ssize_t stm_hdmirx_show_timing_info(struct device *dev,
                                       struct device_attribute *attr, char *buf)
{
  ssize_t sz = 0;
  stm_hdmirx_signal_property_t signal_property;
  stm_hdmirx_route_signal_status_t signal_status;
  struct platform_device *pdev = dev_get_drvdata(dev);
  struct stmhdmirx_device_attribute *route_attr = to_stmhdmirx_dev_attr(attr);
  hdmirx_dev_handle_t *dHandle_p = &dev_handle[pdev->id];
  uint64_t pix_clock_hz;

  if(dHandle_p->Handle==HDMIRX_INVALID_DEVICE_HANDLE)
    return -EINVAL;

  if(dHandle_p->RouteHandle[route_attr->id].Handle != HDMIRX_INVALID_ROUTE_HANDLE)
  {
      if(stm_hdmirx_route_get_signal_status(dHandle_p->RouteHandle[route_attr->id].Handle, &signal_status)<0)
        return sz;
      if(signal_status==STM_HDMIRX_ROUTE_SIGNAL_STATUS_PRESENT)
      {
        if(stm_hdmirx_route_get_signal_property(dHandle_p->RouteHandle[route_attr->id].Handle, &signal_property)<0)
          return sz;
        pix_clock_hz=(uint64_t)signal_property.timing.pixel_clock_hz;
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Pixel-clock %d\n", signal_property.timing.pixel_clock_hz);
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "H-freq %d\n", (signal_property.timing.pixel_clock_hz/signal_property.timing.htotal));
        if (signal_property.timing.scan_type==STM_HDMIRX_SIGNAL_SCAN_TYPE_PROGRESSIVE)
          sz += snprintf(&buf[sz], PAGE_SIZE - sz, "V-freq %d\n", (uint32_t)vibe_os_div64((pix_clock_hz*1000), ((uint32_t)signal_property.timing.htotal*signal_property.timing.vtotal)));
        else
          sz += snprintf(&buf[sz], PAGE_SIZE - sz, "V-freq %d\n", (uint32_t)vibe_os_div64((pix_clock_hz*1000), ((uint32_t)signal_property.timing.htotal*signal_property.timing.vtotal/2)));
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "H-total %d\n", signal_property.timing.htotal);
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "H-active %d\n", signal_property.timing.hactive);
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "H-start %d\n", signal_property.timing.hactive_start);
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "H-pol %s\n", (signal_property.timing.hsync_polarity==STM_HDMIRX_SIGNAL_POLARITY_POSITIVE)?"positive":"negative" );
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "V-total %d\n", signal_property.timing.vtotal);
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "V-active %d\n", signal_property.timing.vActive);
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "V-start %d\n", signal_property.timing.vactive_start);
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "V-pol %s\n", (signal_property.timing.vsync_polarity==STM_HDMIRX_SIGNAL_POLARITY_POSITIVE)?"positive":"negative");
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Scan-type %s\n", (signal_property.timing.scan_type==STM_HDMIRX_SIGNAL_SCAN_TYPE_PROGRESSIVE)?"PROGRESSIVE":"INTERLACED");
      }
  }

  return sz;
}

static ssize_t stm_hdmirx_show_video_info(struct device *dev,
                                       struct device_attribute *attr, char *buf)
{
  ssize_t sz = 0;
  int i;
  stm_hdmirx_video_property_t video_property;
  stm_hdmirx_route_signal_status_t signal_status;
  struct platform_device *pdev = dev_get_drvdata(dev);
  struct stmhdmirx_device_attribute *route_attr = to_stmhdmirx_dev_attr(attr);
  hdmirx_dev_handle_t *dHandle_p = &dev_handle[pdev->id];

  if(dHandle_p->Handle==HDMIRX_INVALID_DEVICE_HANDLE)
    return -EINVAL;

  if(dHandle_p->RouteHandle[route_attr->id].Handle != HDMIRX_INVALID_ROUTE_HANDLE)
  {
    if(stm_hdmirx_route_get_signal_status(dHandle_p->RouteHandle[route_attr->id].Handle, &signal_status)<0)
      return sz;
    if(signal_status==STM_HDMIRX_ROUTE_SIGNAL_STATUS_PRESENT)
    {
      if(stm_hdmirx_route_get_video_property(dHandle_p->RouteHandle[route_attr->id].Handle, &video_property)<0)
        return sz;
      if(video_property.hdmi_video_format)
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Ext-Video-code %d\n", video_property.video_timing_code);
      else
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Video-code %d\n", video_property.video_timing_code);
      sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Pixel-repeat %d\n", video_property.pixel_repeat_factor);
      sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Picture-aspect %s\n", (video_property.picture_aspect==STM_HDMIRX_ASPECT_RATIO_4_3)?"4:3":"16:9");
      switch(video_property.active_format_aspect)
      {
      case STM_HDMIRX_ASPECT_RATIO_4_3:
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Active-aspect 4_3\n"); break;
      case STM_HDMIRX_ASPECT_RATIO_16_9:
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Active-aspect 16_9\n"); break;
      case STM_HDMIRX_ASPECT_RATIO_16_9_TOP:
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Active-aspect 16_9_TOP\n"); break;
      case STM_HDMIRX_ASPECT_RATIO_14_9_TOP:
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Active-aspect 14_9_TOP\n"); break;
      case STM_HDMIRX_ASPECT_RATIO_4_3_CENTER:
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Active-aspect 4_3_CENTER\n"); break;
      case STM_HDMIRX_ASPECT_RATIO_16_9_CENTER:
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Active-aspect 16_9_CENTER\n"); break;
      case STM_HDMIRX_ASPECT_RATIO_14_9_CENTER:
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Active-aspect 14_9_CENTER\n"); break;
      case STM_HDMIRX_ASPECT_RATIO_4_3_CENTER_14_9_SP:
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Active-aspect 4_3_CENTER_14_9_SP\n"); break;
      case STM_HDMIRX_ASPECT_RATIO_16_9_CENTER_14_9_SP:
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Active-aspect 16_9_CENTER_14_9_SP\n"); break;
      case STM_HDMIRX_ASPECT_RATIO_16_9_CENTER__4_3_SP:
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Active-aspect 16_9_CENTER__4_3_SP\n"); break;
      default: /* STM_HDMIRX_ASPECT_RATIO_NONE */
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Active-aspect NONE\n"); break;
      }
      switch(video_property.color_depth)
      {
      case STM_HDMIRX_COLOR_DEPTH_24BPP: /* 24 bits */
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Color-depth 24\n"); break;
      case STM_HDMIRX_COLOR_DEPTH_30BPP: /* 30 bits */
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Color-depth 30\n"); break;
      case STM_HDMIRX_COLOR_DEPTH_36BPP: /* 36 bits */
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Color-depth 36\n"); break;
      case STM_HDMIRX_COLOR_DEPTH_48BPP: /* 48 bits */
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Color-depth 48\n"); break;
      }
      switch(video_property.color_format)
      {
      case STM_HDMIRX_COLOR_FORMAT_RGB: /* rgb */
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Color-format RGB\n"); break;
      case STM_HDMIRX_COLOR_FORMAT_YUV_422: /* yuv 422 */
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Color-format YUV_422\n"); break;
      case STM_HDMIRX_COLOR_FORMAT_YUV_444: /* yuv 444 */
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Color-format YUV_444\n"); break;
      }
      switch(video_property.colorimetry)
      {
      case STM_HDMIRX_COLORIMETRY_STD_BT_601:
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Colorimetry BT_601\n"); break;
      case STM_HDMIRX_COLORIMETRY_STD_BT_709:
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Colorimetry BT_709\n"); break;
      case STM_HDMIRX_COLORIMETRY_STD_XVYCC_601:
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Colorimetry XVYCC_601\n"); break;
      case STM_HDMIRX_COLORIMETRY_STD_XVYCC_709:
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Colorimetry XVYCC_709\n"); break;
      case STM_HDMIRX_COLORIMETRY_STD_sYCC_601:
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Colorimetry sYCC_601\n"); break;
      case STM_HDMIRX_COLORIMETRY_STD_AdobeYCC_601:
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Colorimetry AdobeYCC_601\n"); break;
      case STM_HDMIRX_COLORIMETRY_STD_AdobeRGB:
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Colorimetry AdobeRGB\n"); break;
      default: /* STM_HDMIRX_COLORIMETRY_STD_DEFAULT */
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Colorimetry DEFAULT\n"); break;
      }
      switch(video_property.scan_info)
      {
      case STM_HDMIRX_SCAN_INFO_NO_DATA:
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Scan-info NO_DATA\n"); break;
      case STM_HDMIRX_SCAN_INFO_FOR_OVERSCANNED_DISPLAY:
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Scan-info OVERSCANNED\n"); break;
      case STM_HDMIRX_SCAN_INFO_FOR_UNDER_SCANNED_DISPLAY:
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Scan-info UNDER_SCANNED\n"); break;
      default: /* STM_HDMIRX_SCAN_INFO_RESERVED */
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Scan-info RESERVED\n"); break;
      }
      switch(video_property.scaling_info)
      {
      case STM_HDMIRX_SCALING_INFO_H_SCALED:
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Scaling-info H_SCALED\n"); break;
      case STM_HDMIRX_SCALING_INFO_V_SCALED:
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Scaling-info V_SCALED\n"); break;
      case STM_HDMIRX_SCALING_INFO_HV_SCALED:
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Scaling-info HV_SCALED\n"); break;
      default: /* STM_HDMIRX_SCALING_INFO_NO_SCALING */
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Scaling-info NO_SCALING\n"); break;
      }
      switch(video_property.rgb_quant_range)
      {
      case STM_HDMIRX_RGB_QUANT_RANGE_DEFAULT:
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "RGB-quant-range DEFAULT\n"); break;
      case STM_HDMIRX_RGB_QUANT_RANGE_LIMITED:
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "RGB-quant-range LIMITED\n"); break;
      case STM_HDMIRX_RGB_QUANT_RANGE_FULL:
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "RGB-quant-range FULL\n"); break;
      default: /* STM_HDMIRX_RGB_QUANT_RANGE_RESERVED */
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "RGB-quant-range RESERVED\n"); break;
      }
      switch(video_property.yc_quant_range)
      {
      case STM_HDMIRX_YC_QUANT_RANGE_LIMITED:
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "YUV-quant-range LIMITED\n"); break;
      case STM_HDMIRX_YC_QUANT_RANGE_FULL:
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "YUV-quant-range FULL\n"); break;
      default: /* STM_HDMIRX_YC_QUANT_RANGE_RESERVED */
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "YUV-quant-range RESERVED\n"); break;
      }
      if (video_property.it_content)
      {
        switch(video_property.content_type)
        {
        case STM_HDMIRX_CONTENT_TYPE_GRAPHICS:
          sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Content-type GRAPHICS\n"); break;
        case STM_HDMIRX_CONTENT_TYPE_PHOTO:
          sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Content-type PHOTO\n"); break;
        case STM_HDMIRX_CONTENT_TYPE_CINEMA:
          sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Content-type CINEMA\n"); break;
        case STM_HDMIRX_CONTENT_TYPE_GAME:
          sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Content-type GAME\n"); break;
        default: /* STM_HDMIRX_CONTENT_TYPE_UNKNOWN */
          sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Content-type UNKNOWN\n"); break;
        }
      }
      if(video_property.video_3d)
      {
        switch(video_property.video_property_3d.format)
        {
        case STM_HDMIRX_3D_FORMAT_FRAME_PACK:
          sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Video-3D-format FRAME_PACK\n"); break;
        case STM_HDMIRX_3D_FORMAT_FIELD_ALT:
          sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Video-3D-format FIELD_ALT\n"); break;
        case STM_HDMIRX_3D_FORMAT_LINE_ALT:
          sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Video-3D-format LINE_ALT\n"); break;
        case STM_HDMIRX_3D_FORMAT_SBYS_FULL:
          sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Video-3D-format SBYS_FULL\n"); break;
        case STM_HDMIRX_3D_FORMAT_SBYS_HALF:
          sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Video-3D-format SBYS_HALF\n");
          switch(video_property.video_property_3d.extdata.sbs_half_ext_data.sampling_mode)
          {
          case STM_HDMIRX_3D_SBYS_HALF_SAMPLING_HORZ_OLOR:
            sz += snprintf(&buf[sz], PAGE_SIZE - sz, "3D-ext_data HORZ_OLOR\n"); break;
          case STM_HDMIRX_3D_SBYS_HALF_SAMPLING_HORZ_OLER:
            sz += snprintf(&buf[sz], PAGE_SIZE - sz, "3D-ext_data HORZ_OLER\n"); break;
          case STM_HDMIRX_3D_SBYS_HALF_SAMPLING_HORZ_ELOR:
            sz += snprintf(&buf[sz], PAGE_SIZE - sz, "3D-ext_data HORZ_ELOR\n"); break;
          case STM_HDMIRX_3D_SBYS_HALF_SAMPLING_HORZ_ELER:
            sz += snprintf(&buf[sz], PAGE_SIZE - sz, "3D-ext_data HORZ_ELER\n"); break;
          case STM_HDMIRX_3D_SBYS_HALF_SAMPLING_QUIN_OLOR:
            sz += snprintf(&buf[sz], PAGE_SIZE - sz, "3D-ext_data QUIN_OLOR\n"); break;
          case STM_HDMIRX_3D_SBYS_HALF_SAMPLING_QUIN_OLER:
            sz += snprintf(&buf[sz], PAGE_SIZE - sz, "3D-ext_data QUIN_OLER\n"); break;
          case STM_HDMIRX_3D_SBYS_HALF_SAMPLING_QUIN_ELOR:
            sz += snprintf(&buf[sz], PAGE_SIZE - sz, "3D-ext_data QUIN_ELOR\n"); break;
          case STM_HDMIRX_3D_SBYS_HALF_SAMPLING_QUIN_ELER:
            sz += snprintf(&buf[sz], PAGE_SIZE - sz, "3D-ext_data QUIN_ELER\n"); break;
          }
          break;
        case STM_HDMIRX_3D_FORMAT_L_D:
          sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Video-3D-type L_D\n"); break;
        case STM_HDMIRX_3D_FORMAT_L_D_G_GMINUSD:
          sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Video-3D-type L_D_G_GMINUSD\n"); break;
        case STM_HDMIRX_3D_FORMAT_TOP_AND_BOTTOM:
          sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Video-3D-type TOP_AND_BOTTOM\n");
          sz += snprintf(&buf[sz], PAGE_SIZE - sz, "3D-metatdata-type %s\n", (video_property.video_property_3d.metatdata.type==STM_HDMIRX_3D_METADATA_ISO23002_3)?"ISO23002_3":"NONE");
          sz += snprintf(&buf[sz], PAGE_SIZE - sz, "3D-metatdata-length %d\n", video_property.video_property_3d.metatdata.length);
          for(i=0; i<video_property.video_property_3d.metatdata.length; i++)
            sz += snprintf(&buf[sz], PAGE_SIZE - sz, "3D-metatdata[%d] %d\n", i, video_property.video_property_3d.metatdata.metadata[i]);
          break;
        default: /* STM_HDMIRX_3D_FORMAT_UNKNOWN */
          sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Video-3D-type UNKNOWN\n"); break;
        }
      }
    }
  }

  return sz;
}
static ssize_t stm_hdmirx_show_audio_info(struct device *dev,
                                       struct device_attribute *attr, char *buf)
{
  ssize_t sz = 0;
  stm_hdmirx_audio_property_t audio_property;
  stm_hdmirx_route_signal_status_t signal_status;
  bool audio_enabled;
  struct platform_device *pdev = dev_get_drvdata(dev);
  struct stmhdmirx_device_attribute *route_attr = to_stmhdmirx_dev_attr(attr);
  hdmirx_dev_handle_t *dHandle_p = &dev_handle[pdev->id];

  if(dHandle_p->Handle==HDMIRX_INVALID_DEVICE_HANDLE)
    return -EINVAL;

  if(dHandle_p->RouteHandle[route_attr->id].Handle != HDMIRX_INVALID_ROUTE_HANDLE)
  {
    if(stm_hdmirx_route_get_signal_status(dHandle_p->RouteHandle[route_attr->id].Handle, &signal_status)<0)
      return sz;
    if(signal_status==STM_HDMIRX_ROUTE_SIGNAL_STATUS_PRESENT)
    {
      if(stm_hdmirx_route_get_audio_out_enable(dHandle_p->RouteHandle[route_attr->id].Handle, &audio_enabled)<0)
        return sz;
      sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Audio-muted %s\n", (audio_enabled)?"NO":"YES");

      if(stm_hdmirx_route_get_audio_property(dHandle_p->RouteHandle[route_attr->id].Handle, &audio_property)<0)
        return sz;
      switch(audio_property.stream_type)
      {
      case STM_HDMIRX_AUDIO_STREAM_TYPE_IEC:
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Stream-type IEC\n"); break;
      case STM_HDMIRX_AUDIO_STREAM_TYPE_DSD:
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Stream-type DSD\n"); break;
      case STM_HDMIRX_AUDIO_STREAM_TYPE_DST:
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Stream-type DST\n"); break;
      case STM_HDMIRX_AUDIO_STREAM_TYPE_HBR:
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Stream-type HBR\n"); break;
      default: /* STM_HDMIRX_AUDIO_STREAM_TYPE_UNKNOWN */
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Stream-type UNKNOWN\n"); break;
      }
      switch(audio_property.coding_type)
      {
      case STM_HDMIRX_AUDIO_CODING_TYPE_PCM:
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Coding-type PCM\n"); break;
      case STM_HDMIRX_AUDIO_CODING_TYPE_AC3:
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Coding-type AC3\n"); break;
      case STM_HDMIRX_AUDIO_CODING_TYPE_MPEG1:
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Coding-type MPEG1\n"); break;
      case STM_HDMIRX_AUDIO_CODING_TYPE_MP3:
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Coding-type MP3\n"); break;
      case STM_HDMIRX_AUDIO_CODING_TYPE_MPEG2:
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Coding-type MPEG2\n"); break;
      case STM_HDMIRX_AUDIO_CODING_TYPE_AAC:
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Coding-type AAC\n"); break;
      case STM_HDMIRX_AUDIO_CODING_TYPE_DTS:
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Coding-type DTS\n"); break;
      case STM_HDMIRX_AUDIO_CODING_TYPE_ATRAC:
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Coding-type ATRAC\n"); break;
      case STM_HDMIRX_AUDIO_CODING_TYPE_DSD:
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Coding-type DSD\n"); break;
      case STM_HDMIRX_AUDIO_CODING_TYPE_DDPLUS:
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Coding-type DDPLUS\n"); break;
      case STM_HDMIRX_AUDIO_CODING_TYPE_DTS_HD:
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Coding-type DTS_HD\n"); break;
      case STM_HDMIRX_AUDIO_CODING_TYPE_MAT:
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Coding-type MAT\n"); break;
      case STM_HDMIRX_AUDIO_CODING_TYPE_DST:
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Coding-type DST\n"); break;
      case STM_HDMIRX_AUDIO_CODING_TYPE_WMA_PRO:
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Coding-type WMA_PRO\n"); break;
      default: /* STM_HDMIRX_AUDIO_CODING_TYPE_NONE */
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Coding-type NONE\n"); break;
      }
      sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Sampling-frequency %d\n", audio_property.sampling_frequency);
      sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Channel-count %d\n", audio_property.channel_count);
      sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Channel-allocation %d\n", audio_property.channel_allocation);
      if(audio_property.sample_size!=STM_HDMIRX_AUDIO_SAMPLE_SIZE_NONE)
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Sample-size %d\n", (audio_property.sample_size+15));
      else
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Sample-size NONE\n");
      sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Level-shift %d\n", audio_property.level_shift);
      switch(audio_property.lfe_pb_level)
      {
      case STM_HDMIRX_AUDIO_LFE_PLAYBACK_LEVEL_0Db:
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Lfe-level 0Db\n"); break;
      case STM_HDMIRX_AUDIO_LFE_PLAYBACK_LEVEL_10Db:
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Lfe-level 10Db\n"); break;
      default: /* STM_HDMIRX_AUDIO_LFE_PLAYBACK_LEVEL_UNKNOWN */
        sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Lfe-level UNKNOWN\n"); break;
      }
      sz += snprintf(&buf[sz], PAGE_SIZE - sz, "DownMix-in-HiBit %s\n", (audio_property.down_mix_inhibit)?"TRUE":"FALSE");
    }
  }

  return sz;
}

static ssize_t stm_hdmirx_show_hdcp_status(struct device *dev,
                                       struct device_attribute *attr, char *buf)
{
  ssize_t sz = 0;
  stm_hdmirx_route_hdcp_status_t hdcp_status;
  struct platform_device *pdev = dev_get_drvdata(dev);
  struct stmhdmirx_device_attribute *route_attr = to_stmhdmirx_dev_attr(attr);
  hdmirx_dev_handle_t *dHandle_p = &dev_handle[pdev->id];

  if(dHandle_p->Handle==HDMIRX_INVALID_DEVICE_HANDLE)
    return -EINVAL;

  if(dHandle_p->RouteHandle[route_attr->id].Handle != HDMIRX_INVALID_ROUTE_HANDLE)
  {
    if(stm_hdmirx_route_get_hdcp_status(dHandle_p->RouteHandle[route_attr->id].Handle, &hdcp_status)<0)
      return sz;
    switch(hdcp_status)
    {
    case STM_HDMIRX_ROUTE_HDCP_STATUS_NOT_AUTHENTICATED:
      sz += snprintf(&buf[sz], PAGE_SIZE - sz, "HDCP NOT_AUTHENTICATED\n"); break;
    case STM_HDMIRX_ROUTE_HDCP_STATUS_AUTHENTICATION_DETECTED:
      sz += snprintf(&buf[sz], PAGE_SIZE - sz, "HDCP AUTHENTICATED\n"); break;
    case STM_HDMIRX_ROUTE_HDCP_STATUS_NOISE_DETECTED:
      sz += snprintf(&buf[sz], PAGE_SIZE - sz, "HDCP NOISE_DETECTED\n"); break;
    }
  }

  return sz;
}

static ssize_t stm_hdmirx_show_vs_info_frame(struct device *dev,
                                       struct device_attribute *attr, char *buf)
{
  ssize_t sz = 0;
  stm_hdmirx_vs_info_t vs_info;
  struct platform_device *pdev = dev_get_drvdata(dev);
  struct stmhdmirx_device_attribute *route_attr = to_stmhdmirx_dev_attr(attr);
  hdmirx_dev_handle_t *dHandle_p = &dev_handle[pdev->id];

  if(dHandle_p->Handle==HDMIRX_INVALID_DEVICE_HANDLE)
    return -EINVAL;

  if(dHandle_p->RouteHandle[route_attr->id].Handle != HDMIRX_INVALID_ROUTE_HANDLE)
  {
    if(stm_hdmirx_route_get_info(dHandle_p->RouteHandle[route_attr->id].Handle, STM_HDMIRX_INFO_VSINFO, &vs_info)<0)
      return sz;
  }
  return sz;
}

static ssize_t stm_hdmirx_show_spd_info_frame(struct device *dev,
                                       struct device_attribute *attr, char *buf)
{
  ssize_t sz = 0;
  stm_hdmirx_spd_info_t spd_info;
  struct platform_device *pdev = dev_get_drvdata(dev);
  struct stmhdmirx_device_attribute *route_attr = to_stmhdmirx_dev_attr(attr);
  hdmirx_dev_handle_t *dHandle_p = &dev_handle[pdev->id];

  if(dHandle_p->Handle==HDMIRX_INVALID_DEVICE_HANDLE)
    return -EINVAL;

  if(dHandle_p->RouteHandle[route_attr->id].Handle != HDMIRX_INVALID_ROUTE_HANDLE)
  {
    if(stm_hdmirx_route_get_info(dHandle_p->RouteHandle[route_attr->id].Handle, STM_HDMIRX_INFO_VSINFO, &spd_info)<0)
      return sz;
  }

  return sz;
}

static ssize_t stm_hdmirx_show_ms_info_frame(struct device *dev,
                                       struct device_attribute *attr, char *buf)
{
  ssize_t sz = 0;
  stm_hdmirx_mpeg_source_info_t ms_info;
  struct platform_device *pdev = dev_get_drvdata(dev);
  struct stmhdmirx_device_attribute *route_attr = to_stmhdmirx_dev_attr(attr);
  hdmirx_dev_handle_t *dHandle_p = &dev_handle[pdev->id];

  if(dHandle_p->Handle==HDMIRX_INVALID_DEVICE_HANDLE)
    return -EINVAL;

  if(dHandle_p->RouteHandle[route_attr->id].Handle != HDMIRX_INVALID_ROUTE_HANDLE)
  {
    if(stm_hdmirx_route_get_info(dHandle_p->RouteHandle[route_attr->id].Handle, STM_HDMIRX_INFO_VSINFO, &ms_info)<0)
      return sz;
  }
  return sz;
}

static ssize_t stm_hdmirx_show_isrc_info_frame(struct device *dev,
                                       struct device_attribute *attr, char *buf)
{
  ssize_t sz = 0;
  stm_hdmirx_isrc_info_t isrc_info;
  struct platform_device *pdev = dev_get_drvdata(dev);
  struct stmhdmirx_device_attribute *route_attr = to_stmhdmirx_dev_attr(attr);
  hdmirx_dev_handle_t *dHandle_p = &dev_handle[pdev->id];

  if(dHandle_p->Handle==HDMIRX_INVALID_DEVICE_HANDLE)
    return -EINVAL;

  if(dHandle_p->RouteHandle[route_attr->id].Handle != HDMIRX_INVALID_ROUTE_HANDLE)
  {
    if(stm_hdmirx_route_get_info(dHandle_p->RouteHandle[route_attr->id].Handle, STM_HDMIRX_INFO_VSINFO, &isrc_info)<0)
      return sz;
  }
  return sz;
}

static ssize_t stm_hdmirx_show_acp_info_frame(struct device *dev,
                                       struct device_attribute *attr, char *buf)
{
  ssize_t sz = 0;
  stm_hdmirx_acp_info_t acp_info;
  struct platform_device *pdev = dev_get_drvdata(dev);
  struct stmhdmirx_device_attribute *route_attr = to_stmhdmirx_dev_attr(attr);
  hdmirx_dev_handle_t *dHandle_p = &dev_handle[pdev->id];

  if(dHandle_p->Handle==HDMIRX_INVALID_DEVICE_HANDLE)
    return sz;
  if(dHandle_p->RouteHandle[route_attr->id].Handle != HDMIRX_INVALID_ROUTE_HANDLE)
  {
    if(stm_hdmirx_route_get_info(dHandle_p->RouteHandle[route_attr->id].Handle, STM_HDMIRX_INFO_VSINFO, &acp_info)<0)
      return sz;
  }

  return sz;
}

static ssize_t stm_hdmirx_show_gbd_info_frame(struct device *dev,
                                       struct device_attribute *attr, char *buf)
{
  ssize_t sz = 0;
  stm_hdmirx_gbd_info_t gbd_info;
  struct platform_device *pdev = dev_get_drvdata(dev);
  struct stmhdmirx_device_attribute *route_attr = to_stmhdmirx_dev_attr(attr);
  hdmirx_dev_handle_t *dHandle_p = &dev_handle[pdev->id];

  if(dHandle_p->Handle==HDMIRX_INVALID_DEVICE_HANDLE)
    return -EINVAL;

  if(dHandle_p->RouteHandle[route_attr->id].Handle != HDMIRX_INVALID_ROUTE_HANDLE)
  {
    if(stm_hdmirx_route_get_info(dHandle_p->RouteHandle[route_attr->id].Handle, STM_HDMIRX_INFO_VSINFO, &gbd_info)<0)
      return sz;
  }
  return sz;
}

static ssize_t stm_hdmirx_show_route_capability(struct device *dev,
                                       struct device_attribute *attr, char *buf)
{
  ssize_t sz = 0;
  stm_hdmirx_route_capability_t route_capability;
  struct platform_device *pdev = dev_get_drvdata(dev);
  struct stmhdmirx_device_attribute *route_attr = to_stmhdmirx_dev_attr(attr);
  hdmirx_dev_handle_t *dHandle_p = &dev_handle[pdev->id];
  stm_hdmirx_device_h devHandle=NULL;
  stm_hdmirx_route_h routeHandle=NULL;

  if(dHandle_p->Handle==HDMIRX_INVALID_DEVICE_HANDLE)
  {
    if (stm_hdmirx_device_open(&devHandle, pdev->id) < 0)
    {
      HDMI_PRINT(KERN_ERR "HdmiRx%d Device Open failed \n",pdev->id);
      return -EINVAL;
    }
    if (devHandle!=dHandle_p->Handle)
    {
      HDMI_PRINT(KERN_ERR "Unknown error occurred\n");
      stm_hdmirx_device_close(devHandle);
      devHandle=NULL;
      return -EINVAL;
    }
    if (stm_hdmirx_route_open(devHandle, route_attr->id, &routeHandle) < 0) {
      HDMI_PRINT(KERN_ERR "HdmiRx Route%d Open failed \n", route_attr->id);
      stm_hdmirx_device_close(devHandle);
      return -EINVAL;
    }
    if (routeHandle!=dHandle_p->RouteHandle[route_attr->id].Handle)
    {
      HDMI_PRINT(KERN_ERR "Unknown error occurred\n");
      stm_hdmirx_route_close(routeHandle);
      stm_hdmirx_device_close(devHandle);
      routeHandle=NULL;
      devHandle=NULL;
      return -EINVAL;
    }
  }

  if(dHandle_p->RouteHandle[route_attr->id].Handle != HDMIRX_INVALID_ROUTE_HANDLE)
  {
    if(stm_hdmirx_route_get_capability(dHandle_p->RouteHandle[route_attr->id].Handle, &route_capability)<0)
      goto close;
    sz += snprintf(&buf[sz], PAGE_SIZE - sz, "3D-support %s\n", (route_capability.video_3d_support)?"YES":"NO");
  }

close:
  if(routeHandle)
    stm_hdmirx_route_close(routeHandle);
  if(devHandle)
    stm_hdmirx_device_close(devHandle);
  return sz;
}


static ssize_t stm_hdmirx_show_port_capability(struct device *dev,
                                       struct device_attribute *attr, char *buf)
{
  ssize_t sz = 0;
  stm_hdmirx_port_capability_t port_capability;
  struct platform_device *pdev = dev_get_drvdata(dev);
  struct stmhdmirx_device_attribute *port_attr = to_stmhdmirx_dev_attr(attr);
  hdmirx_dev_handle_t *dHandle_p = &dev_handle[pdev->id];
  stm_hdmirx_device_h devHandle=NULL;
  stm_hdmirx_port_h portHandle=NULL;

  if(dHandle_p->Handle==HDMIRX_INVALID_DEVICE_HANDLE)
  {
    if (stm_hdmirx_device_open(&devHandle, pdev->id) < 0)
    {
      HDMI_PRINT(KERN_ERR "HdmiRx%d Device Open failed \n",pdev->id);
      return -EINVAL;
    }
    if (devHandle!=dHandle_p->Handle)
    {
      HDMI_PRINT(KERN_ERR "Unknown error occurred\n");
      stm_hdmirx_device_close(devHandle);
      devHandle=NULL;
      return -EINVAL;
    }
    if (stm_hdmirx_port_open(devHandle, port_attr->id, &portHandle) < 0) {
      HDMI_PRINT(KERN_ERR "HdmiRx Port%d Open failed \n", port_attr->id);
      stm_hdmirx_device_close(devHandle);
      return -EINVAL;
    }
    if (portHandle!=dHandle_p->PortHandle[port_attr->id].Handle)
    {
      HDMI_PRINT(KERN_ERR "Unknown error occurred\n");
      stm_hdmirx_port_close(portHandle);
      stm_hdmirx_device_close(devHandle);
      portHandle=NULL;
      devHandle=NULL;
      return -EINVAL;
    }
  }

  if(dHandle_p->PortHandle[port_attr->id].Handle != HDMIRX_INVALID_PORT_HANDLE)
  {
    if(stm_hdmirx_port_get_capability(dHandle_p->PortHandle[port_attr->id].Handle, &port_capability)<0)
      goto close;
    sz += snprintf(&buf[sz], PAGE_SIZE - sz, "HPD-support %s\n", (port_capability.hpd_support)?"YES":"NO");
    sz += snprintf(&buf[sz], PAGE_SIZE - sz, "EDID %s\n", (port_capability.internal_edid)?"Internal":"External");
    sz += snprintf(&buf[sz], PAGE_SIZE - sz, "DDC2BI-support %s\n", (port_capability.hpd_support)?"YES":"NO");
  }

close:
  if(portHandle)
    stm_hdmirx_port_close(portHandle);
  if(devHandle)
    stm_hdmirx_device_close(devHandle);
  return sz;
}

static ssize_t stm_hdmirx_show_port_source_status(struct device *dev,
                                       struct device_attribute *attr, char *buf)
{
  ssize_t sz = 0;
  stm_hdmirx_port_source_plug_status_t source_plug_status;
  struct platform_device *pdev = dev_get_drvdata(dev);
  struct stmhdmirx_device_attribute *port_attr = to_stmhdmirx_dev_attr(attr);
  hdmirx_dev_handle_t *dHandle_p = &dev_handle[pdev->id];
  stm_hdmirx_device_h devHandle=NULL;
  stm_hdmirx_port_h portHandle=NULL;

  if(dHandle_p->Handle==HDMIRX_INVALID_DEVICE_HANDLE)
  {
    if (stm_hdmirx_device_open(&devHandle, pdev->id) < 0)
    {
      HDMI_PRINT(KERN_ERR "HdmiRx%d Device Open failed \n",pdev->id);
      return -EINVAL;
    }
    if (devHandle!=dHandle_p->Handle)
    {
      HDMI_PRINT(KERN_ERR "Unknown error occurred\n");
      stm_hdmirx_device_close(devHandle);
      devHandle=NULL;
      return -EINVAL;
    }
    if (stm_hdmirx_port_open(devHandle, port_attr->id, &portHandle) < 0) {
      HDMI_PRINT(KERN_ERR "HdmiRx Port%d Open failed \n", port_attr->id);
      stm_hdmirx_device_close(devHandle);
      return -EINVAL;
    }
    if (portHandle!=dHandle_p->PortHandle[port_attr->id].Handle)
    {
      HDMI_PRINT(KERN_ERR "Unknown error occurred\n");
      stm_hdmirx_port_close(portHandle);
      stm_hdmirx_device_close(devHandle);
      portHandle=NULL;
      devHandle=NULL;
      return -EINVAL;
    }
  }

  if(dHandle_p->PortHandle[port_attr->id].Handle != HDMIRX_INVALID_PORT_HANDLE)
  {
    if(stm_hdmirx_port_get_source_plug_status(dHandle_p->PortHandle[port_attr->id].Handle, &source_plug_status)<0)
      goto close;
    sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Source %s\n", (source_plug_status==STM_HDMIRX_PORT_SOURCE_PLUG_STATUS_OUT)?"pluged OUT":"pluged IN");
  }

close:
  if(portHandle)
    stm_hdmirx_port_close(portHandle);
  if(devHandle)
    stm_hdmirx_device_close(devHandle);
  return sz;
}

static ssize_t stm_hdmirx_show_port_hpd_signal(struct device *dev,
                                       struct device_attribute *attr, char *buf)
{
  ssize_t sz = 0;
  stm_hdmirx_port_hpd_state_t hpd_state;
  struct platform_device *pdev = dev_get_drvdata(dev);
  struct stmhdmirx_device_attribute *port_attr = to_stmhdmirx_dev_attr(attr);
  hdmirx_dev_handle_t *dHandle_p = &dev_handle[pdev->id];

  if(dHandle_p->Handle==HDMIRX_INVALID_DEVICE_HANDLE)
    return -EINVAL;

  if(dHandle_p->PortHandle[port_attr->id].Handle != HDMIRX_INVALID_PORT_HANDLE)
  {
    if(stm_hdmirx_port_get_hpd_signal(dHandle_p->PortHandle[port_attr->id].Handle, &hpd_state)<0)
      return sz;
    sz += snprintf(&buf[sz], PAGE_SIZE - sz, "HPD %s\n", (hpd_state==STM_HDMIRX_PORT_HPD_STATE_HIGH)?"HIGH":"LOW");
  }
  return sz;
}

static ssize_t stm_hdmirx_change_port_hpd_signal(struct device *dev,
                                       struct device_attribute *attr, const char *buf, size_t count)
{
  struct platform_device *pdev = dev_get_drvdata(dev);
  struct stmhdmirx_device_attribute *port_attr = to_stmhdmirx_dev_attr(attr);
  hdmirx_dev_handle_t *dHandle_p = &dev_handle[pdev->id];
  int value;

  if(dHandle_p->Handle==HDMIRX_INVALID_DEVICE_HANDLE)
    return -EINVAL;

  if(! sscanf(buf,"%u\n",&value))
    return -EINVAL;

  if(dHandle_p->PortHandle[port_attr->id].Handle != HDMIRX_INVALID_PORT_HANDLE)
  {
    if(stm_hdmirx_port_set_hpd_signal(dHandle_p->PortHandle[port_attr->id].Handle, (stm_hdmirx_port_hpd_state_t)value )<0)
    {
      HDMI_PRINT(KERN_ERR "Error: setting hpd level failed\n");
      return count;
    }
  }

  return count;
}

static ssize_t stm_hdmirx_show_port_edid_data(struct device *dev,
                                       struct device_attribute *attr, char *buf)
{
  ssize_t sz = 0;
  uint8_t count,block,checksum=0,ext_blocks=0;
  stm_hdmirx_port_edid_block_t edid_block;
  struct platform_device *pdev = dev_get_drvdata(dev);
  struct stmhdmirx_device_attribute *port_attr = to_stmhdmirx_dev_attr(attr);
  hdmirx_dev_handle_t *dHandle_p = &dev_handle[pdev->id];
  stm_hdmirx_device_h devHandle=NULL;
  stm_hdmirx_port_h portHandle=NULL;

  if(dHandle_p->Handle==HDMIRX_INVALID_DEVICE_HANDLE)
  {
    if (stm_hdmirx_device_open(&devHandle, pdev->id) < 0)
    {
      HDMI_PRINT(KERN_ERR "HdmiRx%d Device Open failed \n",pdev->id);
      return -EINVAL;
    }
    if (devHandle!=dHandle_p->Handle)
    {
      HDMI_PRINT(KERN_ERR "Unknown error occurred\n");
      stm_hdmirx_device_close(devHandle);
      devHandle=NULL;
      return -EINVAL;
    }
    if (stm_hdmirx_port_open(devHandle, port_attr->id, &portHandle) < 0) {
      HDMI_PRINT(KERN_ERR "HdmiRx Port%d Open failed \n", port_attr->id);
      stm_hdmirx_device_close(devHandle);
      return -EINVAL;
    }
    if (portHandle!=dHandle_p->PortHandle[port_attr->id].Handle)
    {
      HDMI_PRINT(KERN_ERR "Unknown error occurred\n");
      stm_hdmirx_port_close(portHandle);
      stm_hdmirx_device_close(devHandle);
      portHandle=NULL;
      devHandle=NULL;
      return -EINVAL;
    }
  }

  if(dHandle_p->PortHandle[port_attr->id].Handle != HDMIRX_INVALID_PORT_HANDLE)
  {
    if(stm_hdmirx_port_read_edid_block(dHandle_p->PortHandle[port_attr->id].Handle, 0, &edid_block)<0)
      goto close;
    for (count = 0; count < 128; count++)
    {
      if(count%16==0)
        sz += snprintf(&buf[sz], PAGE_SIZE - sz,"\n%x0|", (count/16));
      sz += snprintf(&buf[sz], PAGE_SIZE - sz," %02X", edid_block[count]);
      checksum+=edid_block[count];
    }
    if (checksum!=0)
      HDMI_PRINT(KERN_WARNING "Edid block0 data integrity error (wrong checksum !!)\n");
    else
      (edid_block[126]<4)?(ext_blocks=edid_block[126]):(ext_blocks=1);

    sz += snprintf(&buf[sz], PAGE_SIZE - sz,"\n");
    for (block=1; block<(ext_blocks+1); block++)
    {
      checksum=0;
      if(stm_hdmirx_port_read_edid_block(dHandle_p->PortHandle[port_attr->id].Handle, block, &edid_block)<0)
        goto close;
      for (count = 0; count < 128; count++)
      {
        if(count%16==0)
          sz += snprintf(&buf[sz], PAGE_SIZE - sz,"\n%x0|", ((count+block*128)/16));
        sz += snprintf(&buf[sz], PAGE_SIZE - sz," %02X", edid_block[count]);
        checksum+=edid_block[count];
      }
      sz += snprintf(&buf[sz], PAGE_SIZE - sz,"\n");
      if (checksum!=0)
        HDMI_PRINT(KERN_WARNING "Edid block%d data integrity error (wrong checksum !!)\n",block);
    }
  }

close:
  if(portHandle)
    stm_hdmirx_port_close(portHandle);
  if(devHandle)
    stm_hdmirx_device_close(devHandle);
  return sz;
}

static ssize_t stm_hdmirx_change_port_edid_data(struct device *dev,
                                       struct device_attribute *attr, const char *buf, size_t count)
{
  uint16_t sz=0, hcount=0;
  uint8_t checksum=0, ext_blocks=0;
  unsigned int i,block, hex_count, value;
  stm_hdmirx_port_edid_block_t edid_block[4];
  struct platform_device *pdev = dev_get_drvdata(dev);
  struct stmhdmirx_device_attribute *port_attr = to_stmhdmirx_dev_attr(attr);
  hdmirx_dev_handle_t *dHandle_p = &dev_handle[pdev->id];
  stm_hdmirx_device_h devHandle=NULL;
  stm_hdmirx_port_h portHandle=NULL;

  if(count<256)
    return count;

  if(dHandle_p->Handle==HDMIRX_INVALID_DEVICE_HANDLE)
  {
    if (stm_hdmirx_device_open(&devHandle, pdev->id) < 0)
    {
      HDMI_PRINT(KERN_ERR "HdmiRx%d Device Open failed \n",pdev->id);
      return -EINVAL;
    }
    if (devHandle!=dHandle_p->Handle)
    {
      HDMI_PRINT(KERN_ERR "Unknown error occurred\n");
      stm_hdmirx_device_close(devHandle);
      devHandle=NULL;
      return -EINVAL;
    }
    if (stm_hdmirx_port_open(devHandle, port_attr->id, &portHandle) < 0) {
      HDMI_PRINT(KERN_ERR "HdmiRx Port%d Open failed \n", port_attr->id);
      stm_hdmirx_device_close(devHandle);
      return -EINVAL;
    }
    if (portHandle!=dHandle_p->PortHandle[port_attr->id].Handle)
    {
      HDMI_PRINT(KERN_ERR "Unknown error occurred\n");
      stm_hdmirx_port_close(portHandle);
      stm_hdmirx_device_close(devHandle);
      portHandle=NULL;
      devHandle=NULL;
      return -EINVAL;
    }
  }

  for (i=0; i<128; i++)
  {
    if(i%16==0)
    {
      sz += sscanf(&buf[sz],"\n%x|", &hex_count)*4;
      if (hcount!=hex_count)
      {
        HDMI_PRINT(KERN_WARNING "Bud file format\n");
        goto close;
      }
      hcount+=0x10;
    }
    if (sz>count) goto close;
    sz += sscanf(&buf[sz]," %02x",&value)*3;
    if (sz>count) goto close;
    edid_block[0][i]=value;
    /* calculate checksum */
    checksum+=value;
  }

  if (checksum!=0)
    HDMI_PRINT(KERN_WARNING "Please verify edid block0 data integrity (wrong checksum !!)\n");

  /* check block0 HEAD 00FFFFFFFFFFFF00 */
  if (edid_block[0][0]!=0x0 || edid_block[0][7]!=0x0)
  {
    HDMI_PRINT(KERN_WARNING "Please verify is Block0 HEAD is 00FFFFFFFFFFFF00\n");
  }
  else
  {
    for (i=1; i<7; i++)
    {
      if (edid_block[0][i] != 0xff)
      {
        HDMI_PRINT(KERN_WARNING "Please verify is Block0 HEAD is 00FFFFFFFFFFFF00\n");
        break;
      }
    }
  }

  if(edid_block[0][126]==0 || (sz+256<count))
  {
    goto update;
  }

  (edid_block[0][126]<4)?(ext_blocks=edid_block[0][126]):(ext_blocks=1);

  for (block=1; block<(ext_blocks+1); block++)
  {
    checksum=0;
    for (i=0; i<128; i++)
    {
      if(i%16==0)
      {
        sz += sscanf(&buf[sz],"\n%x|", &hex_count)*4;
        if (hcount!=hex_count)
        {
          HDMI_PRINT(KERN_WARNING "Bud file format\n");
          goto close;
        }
        hcount+=0x10;
      }
      if (sz>count) goto close;
      sz += sscanf(&buf[sz]," %02x",&value)*3;
      if (sz>count) goto close;
      edid_block[block][i]=value;
      checksum+=value;
    }
    if (checksum!=0)
      HDMI_PRINT(KERN_WARNING "Please verify edid block%d data integrity (wrong checksum !!)\n",block);
  }

update:
  if(dHandle_p->PortHandle[port_attr->id].Handle != HDMIRX_INVALID_PORT_HANDLE)
  {
    for (block=0; block<(ext_blocks+1); block++)
    {
      if(stm_hdmirx_port_update_edid_block(dHandle_p->PortHandle[port_attr->id].Handle, block, &edid_block[block])<0)
        goto close;
    }
  }

close:
  if(portHandle)
    stm_hdmirx_port_close(portHandle);
  if(devHandle)
    stm_hdmirx_device_close(devHandle);
  return count;
}

static ssize_t stm_hdmirx_show_device_capability(struct device *dev,
                                       struct device_attribute *attr, char *buf)
{
  ssize_t sz = 0;
  stm_hdmirx_device_capability_t device_capability;
  struct platform_device *pdev = dev_get_drvdata(dev);
  hdmirx_dev_handle_t *dHandle_p = &dev_handle[pdev->id];
  stm_hdmirx_device_h devHandle=NULL;

  if(dHandle_p->Handle==HDMIRX_INVALID_DEVICE_HANDLE)
  {
    if (stm_hdmirx_device_open(&devHandle, pdev->id) < 0)
    {
      HDMI_PRINT(KERN_ERR "HdmiRx%d Device Open failed \n",pdev->id);
      return -EINVAL;
    }
    if (devHandle!=dHandle_p->Handle)
    {
      HDMI_PRINT(KERN_ERR "Unknown error occurred\n");
      stm_hdmirx_device_close(devHandle);
      devHandle=NULL;
      return -EINVAL;
    }
  }

  if(stm_hdmirx_device_get_capability(dHandle_p->Handle, &device_capability)<0)
    goto close;
  sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Number-of-routes %d\n", device_capability.number_of_routes);
  sz += snprintf(&buf[sz], PAGE_SIZE - sz, "Number-of-ports %d\n", device_capability.number_of_ports);

close:
  if(devHandle)
    stm_hdmirx_device_close(devHandle);
  return sz;
}

static struct device_attribute stm_hdmirx_dev_attrs[] =
{
  __ATTR(capability, S_IRUGO, stm_hdmirx_show_device_capability, NULL)
};

static struct stmhdmirx_device_attribute stm_hdmirx_route_attrs[2][12]=
{
  {
    HDMIRX_DEVICE_ATTR(capability, 0, S_IRUGO, stm_hdmirx_show_route_capability, NULL),
    HDMIRX_DEVICE_ATTR(signal_info, 0, S_IRUGO, stm_hdmirx_show_signal_info, NULL),
    HDMIRX_DEVICE_ATTR(timing_info, 0, S_IRUGO, stm_hdmirx_show_timing_info, NULL),
    HDMIRX_DEVICE_ATTR(video_info, 0, S_IRUGO, stm_hdmirx_show_video_info, NULL),
    HDMIRX_DEVICE_ATTR(audio_info, 0, S_IRUGO, stm_hdmirx_show_audio_info, NULL),
    HDMIRX_DEVICE_ATTR(hdcp_status, 0, S_IRUGO, stm_hdmirx_show_hdcp_status, NULL),
    HDMIRX_DEVICE_ATTR(vs_infoframe, 0, S_IRUGO, stm_hdmirx_show_vs_info_frame, NULL),
    HDMIRX_DEVICE_ATTR(spd_infoframe, 0, S_IRUGO, stm_hdmirx_show_spd_info_frame, NULL),
    HDMIRX_DEVICE_ATTR(ms_infoframe, 0, S_IRUGO, stm_hdmirx_show_ms_info_frame, NULL),
    HDMIRX_DEVICE_ATTR(isrc_infoframe, 0, S_IRUGO, stm_hdmirx_show_isrc_info_frame, NULL),
    HDMIRX_DEVICE_ATTR(acp_infoframe, 0, S_IRUGO, stm_hdmirx_show_acp_info_frame, NULL),
    HDMIRX_DEVICE_ATTR(gbd_infoframe, 0, S_IRUGO, stm_hdmirx_show_gbd_info_frame, NULL)},
  {
    HDMIRX_DEVICE_ATTR(capability, 1, S_IRUGO, stm_hdmirx_show_route_capability, NULL),
    HDMIRX_DEVICE_ATTR(signal_info, 1, S_IRUGO, stm_hdmirx_show_signal_info, NULL),
    HDMIRX_DEVICE_ATTR(timing_info, 1, S_IRUGO, stm_hdmirx_show_timing_info, NULL),
    HDMIRX_DEVICE_ATTR(video_info, 1, S_IRUGO, stm_hdmirx_show_video_info, NULL),
    HDMIRX_DEVICE_ATTR(audio_info, 1, S_IRUGO, stm_hdmirx_show_audio_info, NULL),
    HDMIRX_DEVICE_ATTR(hdcp_status, 0, S_IRUGO, stm_hdmirx_show_hdcp_status, NULL),
    HDMIRX_DEVICE_ATTR(vs_infoframe, 0, S_IRUGO, stm_hdmirx_show_vs_info_frame, NULL),
    HDMIRX_DEVICE_ATTR(spd_infoframe, 0, S_IRUGO, stm_hdmirx_show_spd_info_frame, NULL),
    HDMIRX_DEVICE_ATTR(ms_infoframe, 0, S_IRUGO, stm_hdmirx_show_ms_info_frame, NULL),
    HDMIRX_DEVICE_ATTR(isrc_infoframe, 0, S_IRUGO, stm_hdmirx_show_isrc_info_frame, NULL),
    HDMIRX_DEVICE_ATTR(acp_infoframe, 0, S_IRUGO, stm_hdmirx_show_acp_info_frame, NULL),
    HDMIRX_DEVICE_ATTR(gbd_infoframe, 0, S_IRUGO, stm_hdmirx_show_gbd_info_frame, NULL)},
};

static struct stmhdmirx_device_attribute stm_hdmirx_port_attrs[4][4]=
{
  {
    HDMIRX_DEVICE_ATTR(capability, 0, S_IRUGO, stm_hdmirx_show_port_capability, NULL),
    HDMIRX_DEVICE_ATTR(source_status, 0, S_IRUGO, stm_hdmirx_show_port_source_status, NULL),
    HDMIRX_DEVICE_ATTR(hpd_signal, 0, S_IRUGO|S_IWUSR, stm_hdmirx_show_port_hpd_signal, stm_hdmirx_change_port_hpd_signal),
    HDMIRX_DEVICE_ATTR(edid_data, 0, S_IRUGO|S_IWUSR, stm_hdmirx_show_port_edid_data, stm_hdmirx_change_port_edid_data) },
  {
    HDMIRX_DEVICE_ATTR(capability, 1, S_IRUGO, stm_hdmirx_show_port_capability, NULL),
    HDMIRX_DEVICE_ATTR(source_status, 1, S_IRUGO, stm_hdmirx_show_port_source_status, NULL),
    HDMIRX_DEVICE_ATTR(hpd_signal, 1, S_IRUGO|S_IWUSR, stm_hdmirx_show_port_hpd_signal, stm_hdmirx_change_port_hpd_signal),
    HDMIRX_DEVICE_ATTR(edid_data, 1, S_IRUGO|S_IWUSR, stm_hdmirx_show_port_edid_data, stm_hdmirx_change_port_edid_data) },
  {
    HDMIRX_DEVICE_ATTR(capability, 2, S_IRUGO, stm_hdmirx_show_port_capability, NULL),
    HDMIRX_DEVICE_ATTR(source_status, 2, S_IRUGO, stm_hdmirx_show_port_source_status, NULL),
    HDMIRX_DEVICE_ATTR(hpd_signal, 2, S_IRUGO|S_IWUSR, stm_hdmirx_show_port_hpd_signal, stm_hdmirx_change_port_hpd_signal),
    HDMIRX_DEVICE_ATTR(edid_data, 2, S_IRUGO|S_IWUSR, stm_hdmirx_show_port_edid_data, stm_hdmirx_change_port_edid_data) },
  {
    HDMIRX_DEVICE_ATTR(capability, 3, S_IRUGO, stm_hdmirx_show_port_capability, NULL),
    HDMIRX_DEVICE_ATTR(source_status, 3, S_IRUGO, stm_hdmirx_show_port_source_status, NULL),
    HDMIRX_DEVICE_ATTR(hpd_signal, 3, S_IRUGO|S_IWUSR, stm_hdmirx_show_port_hpd_signal, stm_hdmirx_change_port_hpd_signal),
    HDMIRX_DEVICE_ATTR(edid_data, 3, S_IRUGO|S_IWUSR, stm_hdmirx_show_port_edid_data, stm_hdmirx_change_port_edid_data) }
};

#ifndef CONFIG_ARCH_STI
int __init stmhdmirx_class_device_register(struct class *stminput_class, struct platform_device *pdev)
#else
int stmhdmirx_class_device_register(struct class *stminput_class, struct platform_device *pdev)
#endif
{
  int ret=0, i,id;
  stm_hdmirx_platform_data_t *platform_data=pdev->dev.platform_data;

  hdmirx_class_device = device_create(stminput_class, NULL, MKDEV(0, 0), pdev,"hdmirx%d", pdev->id);

  if (IS_ERR(hdmirx_class_device))
    {
      HDMI_PRINT(KERN_ERR "Failed to create hdmirx%d class device \n", pdev->id);
      hdmirx_class_device = NULL;
      return -ENODEV;
    }

  for (i = 0; i < ARRAY_SIZE(stm_hdmirx_dev_attrs); i++)
    {
      ret = device_create_file(
              hdmirx_class_device, &stm_hdmirx_dev_attrs[i]);
      if (ret)
        break;
    }

  for (id = 0; id < platform_data->soc->num_routes; id++)
  {
    route_class_device[id] = device_create(stminput_class, hdmirx_class_device, MKDEV(0, 0), pdev,"route%d", id);
    if (IS_ERR(route_class_device[id]))
    {
      HDMI_PRINT(KERN_ERR "Failed to create route%d class device \n", id);
      route_class_device[id] = NULL;
      return -ENODEV;
    }
    for (i = 0; i < ARRAY_SIZE(stm_hdmirx_route_attrs[id]); i++)
      {
        ret = device_create_file(
                route_class_device[id], &stm_hdmirx_route_attrs[id][i].dev_attr);
        if (ret)
          break;
      }
  }
  for (id = 0; id < platform_data->board->num_ports; id++)
  {
    port_class_device[id] = device_create(stminput_class, hdmirx_class_device, MKDEV(0, 0), pdev,"port%d", id);
    if (IS_ERR(port_class_device[id]))
    {
      HDMI_PRINT(KERN_ERR "Failed to create port%d class device\n", id);
      port_class_device[id] = NULL;
      return -ENODEV;
    }
    for (i = 0; i < ARRAY_SIZE(stm_hdmirx_port_attrs[id]); i++)
      {
        ret = device_create_file(
                port_class_device[id], &stm_hdmirx_port_attrs[id][i].dev_attr);
        if (ret)
          break;
      }
  }

  return ret;
}

void __exit stmhdmirx_class_device_unregister(struct platform_device *pdev)
{
  stm_hdmirx_platform_data_t *platform_data=pdev->dev.platform_data;
  int id;

  for (id = 0; id < platform_data->soc->num_routes; id++)
  {
    if (route_class_device[id])
    {
      device_unregister(route_class_device[id]);
      route_class_device[id] = NULL;
    }
  }
  for (id = 0; id < platform_data->board->num_ports; id++)
  {
    if (port_class_device[id])
    {
      device_unregister(port_class_device[id]);
      port_class_device[id] = NULL;
    }
  }
  if (hdmirx_class_device)
  {
    device_unregister(hdmirx_class_device);
    hdmirx_class_device = NULL;
  }
}
