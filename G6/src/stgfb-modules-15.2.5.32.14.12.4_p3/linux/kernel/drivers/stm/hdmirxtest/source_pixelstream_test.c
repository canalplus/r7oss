/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/hdmirxtest/source_pixelstream_test.c
 * Copyright (c) 2005-2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <hdmirx_test.h>
#include <stm_display.h>


static stm_display_device_h hDisplay  = 0;   /* Display Device Handle       */
static stm_display_plane_h  hPlane    = 0;   /* Display Plane Handle        */
static stm_display_source_h hSource   = 0;   /* Display Source connected to */
static stm_display_output_h hOutput   = 0;   /* Display Output Handle       */
static stm_display_source_pixelstream_h pixelstream_interface = 0;
static stm_display_source_pixelstream_params_t pixelstream_params;

extern uint32_t Plane_ID;

uint32_t stmhdmirx_pixelstream_init(void)
{
  uint32_t err=0;
  uint32_t ids[7];
  uint32_t numids = 7;
  int i = 0;
  stm_display_source_interface_params_t iface_params;
  stm_display_pixel_stream_params_t pixel_stream_params;

  /* Set pixel stream input params */
  iface_params.interface_type = STM_SOURCE_PIXELSTREAM_IFACE;
  iface_params.interface_params.pixel_stream_params = &pixel_stream_params;
  pixel_stream_params.source_type = 0;
  pixel_stream_params.instance_number = 0;

  err = stm_display_open_device(0, &hDisplay);
  if(err!=0)
  {
    HDMI_TEST_PRINT ("ERROR : Failed to open display device\n");
    return err;
  }

  err = stm_display_device_open_output(hDisplay, 0, &hOutput);
  if(err!=0)
  {
    HDMI_TEST_PRINT ("ERROR : Failed to open output\n");
    return err;
  }

  err = stm_display_device_open_plane(hDisplay, Plane_ID, &hPlane);
  if(err!=0)
  {
    HDMI_TEST_PRINT ("ERROR : Failed to open plane\n");
    return err;
  }

  err = stm_display_plane_connect_to_output(hPlane, hOutput);
  if(err!=0)
  {
    HDMI_TEST_PRINT ("ERROR : Failed to  connect plane to output\n");
    return err;
  }

  stm_display_plane_get_available_source_id(hPlane, ids, numids);

  while ((i<numids)&&(pixelstream_interface == 0))
  {
    if(stm_display_device_open_source(hDisplay, ids[i], &hSource) < 0)
      HDMI_TEST_PRINT ("ERROR : Failed to open source %d \n", ids[i]);

    stm_display_source_get_interface(hSource, iface_params, (void **)&pixelstream_interface);

    if(pixelstream_interface == 0)
    {
      stm_display_source_close(hSource);
      hSource=0;
    }
    else
    {
      break;
    }
    i++;
  }

  if(pixelstream_interface == 0)
  {
    HDMI_TEST_PRINT ("ERROR : Failed to get pixel stream interface \n");
    return 1;
  }
  else
  {
    HDMI_TEST_PRINT ("Connect plane to source....\n");
    err = stm_display_plane_connect_to_source(hPlane, hSource);
    if(err!=0)
    {
      HDMI_TEST_PRINT ("ERROR : Failed to connect plane to source\n");
      return err;
    }
  }
  return err;
}

uint32_t stmhdmirx_pixelstream_set_signal_property(stm_hdmirx_signal_property_t signal_property)
{
  int res = 0;
  memset(&pixelstream_params,0,sizeof(stm_display_source_pixelstream_params_t));

  pixelstream_params.active_window.width = signal_property.timing.hactive;
  pixelstream_params.active_window.height = signal_property.timing.vActive;
  pixelstream_params.active_window.x = signal_property.timing.hactive_start;
  pixelstream_params.active_window.y = signal_property.timing.vactive_start;

  pixelstream_params.visible_area.width = signal_property.timing.hactive;
  pixelstream_params.visible_area.height = signal_property.timing.vActive;
  pixelstream_params.visible_area.x = 0;
  pixelstream_params.visible_area.y = 0;

  pixelstream_params.htotal = signal_property.timing.htotal;
  pixelstream_params.vtotal = signal_property.timing.vtotal;

  pixelstream_params.pixel_aspect_ratio.numerator = 1;
  pixelstream_params.pixel_aspect_ratio.denominator = 1;

  HDMI_TEST_PRINT("pixel_clock_hz=%d\n",signal_property.timing.pixel_clock_hz);
  HDMI_TEST_PRINT("htotal=%d\n",signal_property.timing.htotal);
  HDMI_TEST_PRINT("vtotal=%d\n",signal_property.timing.vtotal);
  HDMI_TEST_PRINT("src_frame_rate=%d\n", (uint32_t) signal_property.timing.pixel_clock_hz/(signal_property.timing.htotal*signal_property.timing.vtotal)*1000);

  pixelstream_params.src_frame_rate = (uint32_t) signal_property.timing.pixel_clock_hz/(signal_property.timing.htotal*signal_property.timing.vtotal)*1000;

  if (signal_property.timing.scan_type == STM_HDMIRX_SIGNAL_SCAN_TYPE_INTERLACED)
    pixelstream_params.flags = STM_PIXELSTREAM_SRC_INTERLACED;

  if (signal_property.signal_type == STM_HDMIRX_SIGNAL_TYPE_DVI)
  {
    pixelstream_params.colorType=STM_PIXELSTREAM_SRC_RGB;
    stm_display_source_pixelstream_set_input_params(pixelstream_interface, &pixelstream_params);
  }
  else
  {
    stmhdmirx_pixelstream_set_video_property(signal_property.video_property);
  }

  return res;
}

uint32_t stmhdmirx_pixelstream_set_video_property(stm_hdmirx_video_property_t video_property)
{
  int res = 0;
  stm_display_source_pixelstream_ColorType_t colorType;
  stm_display_3d_sbs_sampling_mode_t sbs_sampling_mode;

  switch (video_property.color_format)
  {
  case STM_HDMIRX_COLOR_FORMAT_RGB:
    colorType = STM_PIXELSTREAM_SRC_RGB;
    break;
  case STM_HDMIRX_COLOR_FORMAT_YUV_422:
    colorType = STM_PIXELSTREAM_SRC_YUV_422;
    break;
  case STM_HDMIRX_COLOR_FORMAT_YUV_444:
    colorType = STM_PIXELSTREAM_SRC_YUV_444;
    break;
  default:
    colorType = STM_PIXELSTREAM_SRC_RGB;
  }
  pixelstream_params.colorType = colorType;

  switch (video_property.colorimetry)
  {
  case STM_HDMIRX_COLORIMETRY_STD_BT_601:
  case STM_HDMIRX_COLORIMETRY_STD_XVYCC_601:
  case STM_HDMIRX_COLORIMETRY_STD_sYCC_601:
  case STM_HDMIRX_COLORIMETRY_STD_AdobeYCC_601:
    /* do nothing */
    break;
  case STM_HDMIRX_COLORIMETRY_STD_BT_709:
  case STM_HDMIRX_COLORIMETRY_STD_XVYCC_709:
  default:
    pixelstream_params.flags |= STM_PIXELSTREAM_SRC_COLORSPACE_709;
    break;
  }

  if(video_property.video_3d)
  {
    switch(video_property.video_property_3d.format)
    {
    case STM_HDMIRX_3D_FORMAT_UNKNOWN:
      HDMI_TEST_PRINT ("2D Format\n");
      pixelstream_params.config_3D.formats = FORMAT_3D_NONE;
      break;
    case STM_HDMIRX_3D_FORMAT_SBYS_HALF:
      HDMI_TEST_PRINT ("Side By Side 3D Format\n");
      pixelstream_params.config_3D.formats = FORMAT_3D_SBS_HALF;
      pixelstream_params.config_3D.parameters.sbs.is_left_right_format = true;
      switch (video_property.video_property_3d.extdata.sbs_half_ext_data.sampling_mode)
      {
      case STM_HDMIRX_3D_SBYS_HALF_SAMPLING_HORZ_OLOR:
        sbs_sampling_mode=FORMAT_3D_SBS_HALF_SAMPLING_HORZ_OLOR;
        break;
      case STM_HDMIRX_3D_SBYS_HALF_SAMPLING_HORZ_OLER:
        sbs_sampling_mode=FORMAT_3D_SBS_HALF_SAMPLING_HORZ_OLER;
        break;
      case STM_HDMIRX_3D_SBYS_HALF_SAMPLING_HORZ_ELOR:
        sbs_sampling_mode=FORMAT_3D_SBS_HALF_SAMPLING_HORZ_ELOR;
        break;
      case STM_HDMIRX_3D_SBYS_HALF_SAMPLING_HORZ_ELER:
        sbs_sampling_mode=FORMAT_3D_SBS_HALF_SAMPLING_HORZ_ELER;
        break;
      case STM_HDMIRX_3D_SBYS_HALF_SAMPLING_QUIN_OLOR:
        sbs_sampling_mode=FORMAT_3D_SBS_HALF_SAMPLING_QUIN_OLOR;
        break;
      case STM_HDMIRX_3D_SBYS_HALF_SAMPLING_QUIN_OLER:
        sbs_sampling_mode=FORMAT_3D_SBS_HALF_SAMPLING_QUIN_OLER;
        break;
      case STM_HDMIRX_3D_SBYS_HALF_SAMPLING_QUIN_ELOR:
        sbs_sampling_mode=FORMAT_3D_SBS_HALF_SAMPLING_QUIN_ELOR;
        break;
      case STM_HDMIRX_3D_SBYS_HALF_SAMPLING_QUIN_ELER:
        sbs_sampling_mode=FORMAT_3D_SBS_HALF_SAMPLING_QUIN_ELER;
        break;
      default:
        sbs_sampling_mode=FORMAT_3D_SBS_NO_SUB_SAMPLING;
      }
      pixelstream_params.config_3D.parameters.sbs.sbs_sampling_mode = sbs_sampling_mode;
      break;
    case STM_HDMIRX_3D_FORMAT_TOP_AND_BOTTOM:
      HDMI_TEST_PRINT ("Top And Bottom 3D Format\n");
      pixelstream_params.config_3D.formats = FORMAT_3D_STACKED_HALF;
      pixelstream_params.config_3D.parameters.frame_packed.is_left_right_format = true;
      break;
    case STM_HDMIRX_3D_FORMAT_L_D:
      HDMI_TEST_PRINT ("3D_FORMAT_L_D\n");
      pixelstream_params.config_3D.formats = FORMAT_3D_L_D;
      break;
    case STM_HDMIRX_3D_FORMAT_L_D_G_GMINUSD:
      HDMI_TEST_PRINT ("3D_FORMAT_L_D_G_GMINUSD\n");
      pixelstream_params.config_3D.formats = FORMAT_3D_L_D_G_GMINUSD;
      break;
    case STM_HDMIRX_3D_FORMAT_FRAME_PACK:
      HDMI_TEST_PRINT ("3D_FORMAT_FRAME_PACK\n");
      pixelstream_params.config_3D.formats = FORMAT_3D_FRAME_SEQ;
      break;
    case STM_HDMIRX_3D_FORMAT_FIELD_ALT:
      HDMI_TEST_PRINT ("3D_FORMAT_FIELD_ALT\n");
      pixelstream_params.config_3D.formats = FORMAT_3D_FIELD_ALTERNATE;
      break;
    case STM_HDMIRX_3D_FORMAT_LINE_ALT:
      HDMI_TEST_PRINT ("3D_FORMAT_LINE_ALT\n");
      pixelstream_params.config_3D.formats = FORMAT_3D_PICTURE_INTERLEAVE;
      break;
    case STM_HDMIRX_3D_FORMAT_SBYS_FULL:
      HDMI_TEST_PRINT ("3D_FORMAT_SBYS_FULL\n");
      pixelstream_params.config_3D.formats = FORMAT_3D_SBS_FULL;
      break;
    default:
      HDMI_TEST_PRINT ("Unsupported 3D_FORMAT\n");
      pixelstream_params.config_3D.formats = FORMAT_3D_NONE;
      break;
    }
  }

  switch (video_property.color_depth)
  {
  case STM_HDMIRX_COLOR_DEPTH_24BPP:
    HDMI_TEST_PRINT ("Signal color depth 24 bits\n");
    pixelstream_params.colordepth =	8;
    break;
  case STM_HDMIRX_COLOR_DEPTH_30BPP:
    HDMI_TEST_PRINT ("Signal color depth 30 bits\n");
    pixelstream_params.colordepth =	10;
    break;
  case STM_HDMIRX_COLOR_DEPTH_36BPP:
    HDMI_TEST_PRINT ("Signal color depth 36 bits\n");
    pixelstream_params.colordepth =	12;
    break;
  case STM_HDMIRX_COLOR_DEPTH_48BPP:
    HDMI_TEST_PRINT ("Signal color depth 48 bits\n");
    pixelstream_params.colordepth =	16;
    break;
  default:
    pixelstream_params.colordepth =	8;
  }

  stm_display_source_pixelstream_set_input_params(pixelstream_interface, &pixelstream_params);

  return res;
}

void stmhdmirx_pixelstream_capturing(bool signal_stable)
{
  if(pixelstream_interface)
  {
    if(signal_stable)
    {
      HDMI_TEST_PRINT ("Starting HDMIRX capture ...\n");
      stm_display_source_pixelstream_set_signal_status (pixelstream_interface, PIXELSTREAM_SOURCE_STATUS_STABLE);
    }
    else
    {
      HDMI_TEST_PRINT ("Stopping HDMIRX capture ...\n");
      stm_display_source_pixelstream_set_signal_status (pixelstream_interface, PIXELSTREAM_SOURCE_STATUS_UNSTABLE);
    }
  }
}

uint32_t stmhdmirx_pixelstream_term(void)
{
  uint32_t err=0;
  if(pixelstream_interface)
  {
    err |= stm_display_source_pixelstream_set_signal_status (pixelstream_interface, PIXELSTREAM_SOURCE_STATUS_UNSTABLE);
    err |= stm_display_plane_disconnect_from_source(hPlane, hSource);
  }
  err |= stm_display_plane_disconnect_from_output(hPlane, hOutput);
  stm_display_plane_close(hPlane);
  stm_display_output_close(hOutput);
  stm_display_source_close(hSource);
  stm_display_device_close(hDisplay);
  return err;
}
