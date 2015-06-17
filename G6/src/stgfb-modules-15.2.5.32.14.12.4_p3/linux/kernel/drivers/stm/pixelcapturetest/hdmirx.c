/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/pixelcapturetest/hdmirx.c
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 * Date        Modification                                    Name
 * ----        ------------                                    --------
 * 25-Jun-12   Created                                         Sonia Varada
 * 25-Oct-13   Modified                                        Akram BB
 *
\***********************************************************************/


/******************************************************************************
  I N C L U D E    F I L E S
******************************************************************************/

/* Local Includes -------------------------------------------------*/
#include <stm_pixel_capture.h>
#include "hdmirx_test.h"

/****************L O C A L    D E F I N I T I O N S**************************************/
#define HDMIRX_EVENTS                     2
#define HDMIRX_EVENTS_TIMEOUT             2000
#define HDMIRX_EVENTS_DETECTION_MAX_COUNT 100

/******************************************************************************
  G L O B A L   V A R I A B L E S
******************************************************************************/
static stm_hdmirx_device_h dev;
static stm_hdmirx_port_h port1;
static stm_hdmirx_route_h route;
static   stm_event_subscription_h hdmirx_subscription;

uint32_t hdmirx_mode_detection_test_start(stm_pixel_capture_input_params_t  *InputParams_p)
{
  int32_t err, actual_count, evt;
  hdmirx_port_handle_t *port_handle;
  stm_hdmirx_port_s *hport;
  uint32_t event_count=0;
  uint64_t total_lines;

  stm_hdmirx_device_capability_t dev_capability;
  stm_hdmirx_port_capability_t port_capability;
  stm_hdmirx_port_source_plug_status_t plugin_status;
  stm_hdmirx_route_capability_t route_capability;
  stm_hdmirx_route_signal_status_t signal_status;
  stm_hdmirx_signal_property_t signal_property;
  stm_hdmirx_video_property_t video_property;
  stm_hdmirx_audio_property_t audio_property;
  stm_hdmirx_route_hdcp_status_t hdcp_status;

  stm_event_subscription_entry_t events_entry[HDMIRX_EVENTS];
  stm_event_info_t hdmirx_events;
  stm_hdmirx_information_t info;
  stm_hdmirx_spd_info_t spd_info;

  HDMI_TEST_PRINT("\n***********MODE DETECTION TEST************\n");
  err = stm_hdmirx_device_open(&dev, 0);
  if (err != 0) {
    HDMI_TEST_PRINT("HdmiRx Device Open failed \n");
    return 0;
  }
  err = stm_hdmirx_device_get_capability(dev, &dev_capability);
  if (err != 0)
    HDMI_TEST_PRINT("Error in getting Device Capability\n");
  HDMI_TEST_PRINT("***********Device Capability************\n");
  HDMI_TEST_PRINT("Number of Ports :%d \n", dev_capability.number_of_ports);
  HDMI_TEST_PRINT("Number of Routes :%d \n", dev_capability.number_of_routes);
  HDMI_TEST_PRINT("**********End of Device Capability*********\n");
  err = stm_hdmirx_port_open(dev, HDMIRX_INPUT_PORT_1, &port1);
  if (err != 0) {
    HDMI_TEST_PRINT("HdmiRx Port Open failed \n");
    return 0;
  }

  err = stm_hdmirx_route_open(dev, 0, &route);
  if (err != 0) {
    HDMI_TEST_PRINT("HdmiRx Route Open failed \n");
    return 0;
  }
  events_entry[0].object = port1;
  events_entry[0].event_mask = STM_HDMIRX_PORT_SOURCE_PLUG_EVT;
  events_entry[0].cookie = NULL;
  events_entry[1].object = route;
  events_entry[1].event_mask =
      STM_HDMIRX_ROUTE_SIGNAL_STATUS_CHANGE_EVT |
      STM_HDMIRX_ROUTE_VIDEO_PROPERTY_CHANGE_EVT |
      STM_HDMIRX_ROUTE_AUDIO_PROPERTY_CHANGE_EVT |
      STM_HDMIRX_ROUTE_ACP_INFO_UPDATE_EVT |
      STM_HDMIRX_ROUTE_VS_INFO_UPDATE_EVT |
      STM_HDMIRX_ROUTE_SPD_INFO_UPDATE_EVT |
      STM_HDMIRX_ROUTE_MPEGSOURCE_INFO_UPDATE_EVT |
      STM_HDMIRX_ROUTE_ISRC_INFO_UPDATE_EVT |
      STM_HDMIRX_ROUTE_GBD_INFO_UPDATE_EVT;
  events_entry[1].cookie = NULL;

  evt = stm_event_subscription_create(events_entry, HDMIRX_EVENTS,
            &hdmirx_subscription);
  if (evt != 0) {
    HDMI_TEST_PRINT("Subscription of event failed \n");
    err = -ENODEV;
    goto exit_error;
  } else
    HDMI_TEST_PRINT("Subscription of Event Ok\n");

  err = stm_hdmirx_port_get_capability(port1, &port_capability);
  if (err != 0)
  {
    HDMI_TEST_PRINT("Error in getting Port Capability\n");
    goto exit_error;
  }

  HDMI_TEST_PRINT("*********%d Port Capability*************\n",
       HDMIRX_INPUT_PORT_1);
  HDMI_TEST_PRINT(" hpd_support :%d \n", port_capability.hpd_support);
  HDMI_TEST_PRINT(" ddc_interface_support :%d \n", port_capability.ddc2bi);
  HDMI_TEST_PRINT("internal_edid :%d\n", port_capability.internal_edid);
  HDMI_TEST_PRINT("*********End of Port Capability***********\n");

  err = stm_hdmirx_route_get_capability(route, &route_capability);
  HDMI_TEST_PRINT("*************RouteCapability***************\n");
  HDMI_TEST_PRINT("video_3d_support :0x%x\n",
       route_capability.video_3d_support);
  HDMI_TEST_PRINT("************End of Route Capability*************\n");

  hport = (stm_hdmirx_port_s *) port1;
  port_handle = (hdmirx_port_handle_t *) hport->handle;

  while (event_count < HDMIRX_EVENTS_DETECTION_MAX_COUNT) {
    evt = stm_event_wait(hdmirx_subscription, HDMIRX_EVENTS_TIMEOUT, 1, &actual_count,
           &hdmirx_events);
    if (evt < 0) {
      /* timeout */
      HDMI_TEST_PRINT("************ TIMEOUT (%d)*************\n", event_count);
    }
    else {
      if (hdmirx_events.event.object == port1) {
        if (hdmirx_events.event.event_id == STM_HDMIRX_PORT_SOURCE_PLUG_EVT) {
          stm_hdmirx_port_get_source_plug_status(port1,
                      &plugin_status);

          if (plugin_status ==
              STM_HDMIRX_PORT_SOURCE_PLUG_STATUS_IN)
          {
            HDMI_TEST_PRINT("Cable Connected  PluginStatus:0x%x \n",
                 plugin_status);
            err = stm_hdmirx_port_connect(port1, route);
          }
          if (plugin_status == STM_HDMIRX_PORT_SOURCE_PLUG_STATUS_OUT) {
            HDMI_TEST_PRINT("Cable DisConnected  PluginStatus:0x%x \n",
                 plugin_status);
            err = stm_hdmirx_port_disconnect(port1);
          }
        }
      }
      if (hdmirx_events.event.object == route) {
        if (hdmirx_events.event.event_id == STM_HDMIRX_ROUTE_SIGNAL_STATUS_CHANGE_EVT) {
          stm_hdmirx_route_get_signal_status(route, &signal_status);
          if (signal_status == STM_HDMIRX_ROUTE_SIGNAL_STATUS_PRESENT)
          {
            HDMI_TEST_PRINT
                ("signal status :STM_HDMIRX_ROUTE_SIGNAL_STATUS_PRESENT\n ");
            err = stm_hdmirx_route_get_signal_property(route, &signal_property);
            if (err != 0)
            {
              HDMI_TEST_PRINT("error %d in stm_hdmirx_route_get_signal_property()\n", err);
              break;
            }
            HDMI_TEST_PRINT("***********Signal Property************\n");
            HDMI_TEST_PRINT("Signal Type :%d\n",
                 signal_property.signal_type);
            HDMI_TEST_PRINT("HACTIVE :%d\n",
                 signal_property.timing.hactive);
            HDMI_TEST_PRINT("HTOTAL :%d\n",
                 signal_property.timing.htotal);
            HDMI_TEST_PRINT("VACTIVE :%d\n",
                 signal_property.timing.vActive);
            HDMI_TEST_PRINT("VTOTAL :%d\n",
                 signal_property.timing.vtotal);
            HDMI_TEST_PRINT("ScanType :%d\n",
                 signal_property.
                 timing.scan_type);
            HDMI_TEST_PRINT("Pixe Clock :%d\n",
                 signal_property.timing.pixel_clock_hz);
            HDMI_TEST_PRINT("*************************************\n");
            stm_hdmirx_route_set_audio_out_enable(route, TRUE);
            stm_hdmirx_route_get_hdcp_status(route, &hdcp_status);
            HDMI_TEST_PRINT("HDCP_STATUS:0x%x\n",hdcp_status);

            /* break immediatly */
            err = 0;
            break;
          }

          if (signal_status == STM_HDMIRX_ROUTE_SIGNAL_STATUS_NOT_PRESENT)
          {
            HDMI_TEST_PRINT
                ("signal status :STM_HDMIRX_ROUTE_SIGNAL_STATUS_NOT_PRESENT\n ");
            stm_hdmirx_route_set_audio_out_enable(route, FALSE);
          }
        }

        if (hdmirx_events.event.event_id == STM_HDMIRX_ROUTE_VIDEO_PROPERTY_CHANGE_EVT) {
          HDMI_TEST_PRINT
              ("signal status :STM_HDMIRX_ROUTE_VIDEO_PROPERTY_CHANGE_EVT\n ");
          err = stm_hdmirx_route_get_video_property(route,
                             &video_property);
          HDMI_TEST_PRINT("************VIdeo Property**************/n");
          HDMI_TEST_PRINT("Video3d :%d\n",video_property.video_3d);
          HDMI_TEST_PRINT("color_format :%d\n",
               video_property.color_format);
          HDMI_TEST_PRINT("content_type :%d\n",
               video_property.content_type);
          HDMI_TEST_PRINT("color_depth :%d\n",video_property.color_depth);
          HDMI_TEST_PRINT("active_format_aspect :%d\n",
               video_property.active_format_aspect);
          HDMI_TEST_PRINT("************************************\n");

        }
        if (hdmirx_events.event.event_id == STM_HDMIRX_ROUTE_AUDIO_PROPERTY_CHANGE_EVT) {
          HDMI_TEST_PRINT
              ("signal status :STM_HDMIRX_ROUTE_AUDIO_PROPERTY_CHANGE_EVT\n ");
          err = stm_hdmirx_route_get_audio_property
              (route, &audio_property);
          HDMI_TEST_PRINT("**********Audio Property************/n");
          HDMI_TEST_PRINT("channel_allocation :%d\n",
               audio_property.
               channel_allocation);
          HDMI_TEST_PRINT("coding_type :%d\n",
               audio_property.coding_type);
          HDMI_TEST_PRINT("sampling_frequency :%d\n",
               audio_property.
               sampling_frequency);
          HDMI_TEST_PRINT("stream_type :%d\n",
               audio_property.stream_type);
          HDMI_TEST_PRINT("channel_count :%d\n",
               audio_property.channel_count);
          HDMI_TEST_PRINT("************************************\n");
        }
        if (hdmirx_events.event.event_id == STM_HDMIRX_ROUTE_ACP_INFO_UPDATE_EVT) {
          HDMI_TEST_PRINT
              ("Received event STM_HDMIRX_ROUTE_ACP_INFO_UPDATE_EVT\n");
          err = stm_hdmirx_route_get_info(route,
                      STM_HDMIRX_INFO_ACPINFO,(void *)&info);
        }
        if (hdmirx_events.event.event_id == STM_HDMIRX_ROUTE_MPEGSOURCE_INFO_UPDATE_EVT) {
          HDMI_TEST_PRINT
              ("Received event STM_HDMIRX_ROUTE_MPEGSOURCE_INFO_UPDATE_EVT\n");
          err = stm_hdmirx_route_get_info(route,
                STM_HDMIRX_INFO_MPEGSOURCEINFO,(void *)&info);

        }
        if (hdmirx_events.event.event_id == STM_HDMIRX_ROUTE_SPD_INFO_UPDATE_EVT) {
          HDMI_TEST_PRINT
              ("Received event STM_HDMIRX_ROUTE_SPD_INFO_UPDATE_EVT\n");
          err = stm_hdmirx_route_get_info(route,
                  STM_HDMIRX_INFO_SPDINFO,(void *)&spd_info);

        }
        if (hdmirx_events.event.event_id == STM_HDMIRX_ROUTE_VS_INFO_UPDATE_EVT) {
          HDMI_TEST_PRINT
              ("Received event STM_HDMIRX_ROUTE_VS_INFO_UPDATE_EVT\n");
          err = stm_hdmirx_route_get_info(route,
                      STM_HDMIRX_INFO_VSINFO,(void *)&info);
        }
      }
    }
    event_count++;
    err = -EINVAL;
  }

exit_error:
  if (err == 0) {
    InputParams_p->active_window.x                = signal_property.timing.hactive_start;
    InputParams_p->active_window.y                = signal_property.timing.vactive_start;
    InputParams_p->active_window.width            = signal_property.timing.hactive;
    InputParams_p->active_window.height           = signal_property.timing.vActive;
    InputParams_p->src_frame_rate                 = 50000; /* default to 50 Hz */
    InputParams_p->htotal                         = signal_property.timing.htotal;
    InputParams_p->vtotal                         = signal_property.timing.vtotal;
    InputParams_p->flags                          = signal_property.timing.scan_type ? 0:STM_PIXEL_CAPTURE_BUFFER_INTERLACED;

    /*
     * Calculate input frame rate
     */
    total_lines = (uint64_t)(signal_property.timing.htotal * signal_property.timing.vtotal);
    if (total_lines) {
      uint64_t pixelclock = signal_property.timing.pixel_clock_hz * 1000;
      do_div(pixelclock, total_lines);
      InputParams_p->src_frame_rate = pixelclock;
    }

    switch(signal_property.video_property.active_format_aspect)
    {
      case STM_HDMIRX_ASPECT_RATIO_4_3:
      case STM_HDMIRX_ASPECT_RATIO_4_3_CENTER:
      case STM_HDMIRX_ASPECT_RATIO_4_3_CENTER_14_9_SP:
        InputParams_p->pixel_aspect_ratio.numerator   = 4;
        InputParams_p->pixel_aspect_ratio.denominator = 3;
      break;
      case STM_HDMIRX_ASPECT_RATIO_14_9_TOP:
      case STM_HDMIRX_ASPECT_RATIO_14_9_CENTER:
        InputParams_p->pixel_aspect_ratio.numerator   = 14;
        InputParams_p->pixel_aspect_ratio.denominator = 9;
      break;
      case STM_HDMIRX_ASPECT_RATIO_16_9:
      case STM_HDMIRX_ASPECT_RATIO_16_9_TOP:
      case STM_HDMIRX_ASPECT_RATIO_16_9_CENTER:
      case STM_HDMIRX_ASPECT_RATIO_16_9_CENTER_14_9_SP:
      case STM_HDMIRX_ASPECT_RATIO_16_9_CENTER__4_3_SP:
      default:
        InputParams_p->pixel_aspect_ratio.numerator   = 16;
        InputParams_p->pixel_aspect_ratio.denominator = 9;
      break;
    }

    /* Default to YUV444 input format */
    InputParams_p->pixel_format                   = STM_PIXEL_FORMAT_YUV;

    if(signal_property.video_property.color_format == STM_HDMIRX_COLOR_FORMAT_RGB)
    {
      InputParams_p->color_space                        = STM_PIXEL_CAPTURE_RGB;
      InputParams_p->pixel_format                       = STM_PIXEL_FORMAT_RGB888;
    }
    else
    {
      switch(signal_property.video_property.colorimetry)
      {
        case STM_HDMIRX_COLORIMETRY_STD_BT_601:
        case STM_HDMIRX_COLORIMETRY_STD_XVYCC_601:
        case STM_HDMIRX_COLORIMETRY_STD_sYCC_601:
        case STM_HDMIRX_COLORIMETRY_STD_AdobeYCC_601:
          InputParams_p->color_space                    = STM_PIXEL_CAPTURE_BT601;
        break;
        case STM_HDMIRX_COLORIMETRY_STD_BT_709:
        case STM_HDMIRX_COLORIMETRY_STD_XVYCC_709:
          InputParams_p->color_space                    = STM_PIXEL_CAPTURE_BT709;
        break;
        case STM_HDMIRX_COLORIMETRY_STD_DEFAULT:
        case STM_HDMIRX_COLORIMETRY_STD_AdobeRGB:
        default:
          InputParams_p->color_space                    = STM_PIXEL_CAPTURE_RGB;
          InputParams_p->pixel_format                   = STM_PIXEL_FORMAT_RGB888;
        break;
      }

      if(signal_property.video_property.color_format == STM_HDMIRX_COLOR_FORMAT_YUV_422)
      {
        /* Input is YUV422 */
        InputParams_p->pixel_format                   = STM_PIXEL_FORMAT_YCbCr422R;
      }
    }

    /* Vertical Signal Polarity */
    if(signal_property.timing.vsync_polarity == STM_HDMIRX_SIGNAL_POLARITY_NEGATIVE)
      InputParams_p->vsync_polarity = STM_PIXEL_CAPTURE_FIELD_POLARITY_LOW;

    /* Horizontal Signal Polarity */
    if(signal_property.timing.hsync_polarity == STM_HDMIRX_SIGNAL_POLARITY_NEGATIVE)
      InputParams_p->hsync_polarity = STM_PIXEL_CAPTURE_FIELD_POLARITY_LOW;
  }
  else {
    HDMI_TEST_PRINT("ERROR :0x%x\n", err);
    hdmirx_mode_detection_test_stop();
  }

  return err;
}

void hdmirx_mode_detection_test_stop(void)
{
  int err=0;

  if(hdmirx_subscription)
    err=stm_event_subscription_delete(hdmirx_subscription);
  if(err < 0)
    HDMI_TEST_PRINT("ERROR : failed to unregister event (err=0x%x)\n", err);

  if(port1)
  {
    stm_hdmirx_port_disconnect(port1);
    stm_hdmirx_port_close(port1);
  }

  if(route)
  {
    stm_hdmirx_route_close(route);
  }

  if(dev)
    stm_hdmirx_device_close(dev);
}
