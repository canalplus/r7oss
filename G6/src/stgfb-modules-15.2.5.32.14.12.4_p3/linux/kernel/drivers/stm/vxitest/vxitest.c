/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/vxi/vxitest.c
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <linux/module.h>
#include <linux/slab.h>
#include <stm_vxi.h>
#include <stm_display.h>


static stm_vxi_h            hVxi      = 0;
static stm_display_device_h hDisplay  = 0;   /* Display Device Handle       */
static stm_display_plane_h  hPlane    = 0;   /* Display Plane Handle        */
static stm_display_source_h hSource   = 0;   /* Display Source connected to */
static stm_display_output_h hOutput   = 0;   /* Display Output Handle       */

static int __init stmvxitest_init(void)
{
  int res = 0;
  int i = 0;
  stm_vxi_capability_t capability;
  stm_display_source_interface_params_t iface_params;
  stm_display_source_pixelstream_h pixelstream_interface = 0;
  uint32_t ctrl = 0;
  uint32_t ids[7];
  uint32_t numids = 7;

  if(stm_display_open_device(0, &hDisplay)!=0)
    printk ("ERROR : Failed to open display device\n");

  if(stm_display_device_open_output(hDisplay, 0, &hOutput)!=0)
    printk ("ERROR : Failed to open output\n");


  /* Set pixel stream input params */
  iface_params.interface_type = STM_SOURCE_PIXELSTREAM_IFACE;
  iface_params.interface_params.pixel_stream_params = kzalloc(sizeof(stm_display_pixel_stream_params_t),GFP_KERNEL);
  iface_params.interface_params.pixel_stream_params->source_type = 2;
  iface_params.interface_params.pixel_stream_params->instance_number = 0;

  memset(&iface_params.interface_params.pixel_stream_params->worst_case_timing,0,sizeof(stm_display_source_pixelstream_params_t));

  /* *** HDMI 576I/625I  -  720 x  288 x 50I *** */
  iface_params.interface_params.pixel_stream_params->worst_case_timing.active_window.width = 720;
  iface_params.interface_params.pixel_stream_params->worst_case_timing.active_window.height = 288;
  iface_params.interface_params.pixel_stream_params->worst_case_timing.active_window.x = 264;
  iface_params.interface_params.pixel_stream_params->worst_case_timing.active_window.y = 23;

  iface_params.interface_params.pixel_stream_params->worst_case_timing.visible_area.width = 720;
  iface_params.interface_params.pixel_stream_params->worst_case_timing.visible_area.height = 576;
  iface_params.interface_params.pixel_stream_params->worst_case_timing.visible_area.x = 0;
  iface_params.interface_params.pixel_stream_params->worst_case_timing.visible_area.y = 0;

  iface_params.interface_params.pixel_stream_params->worst_case_timing.htotal = 864;
  iface_params.interface_params.pixel_stream_params->worst_case_timing.vtotal = 625;

  iface_params.interface_params.pixel_stream_params->worst_case_timing.pixel_aspect_ratio.numerator = 1;
  iface_params.interface_params.pixel_stream_params->worst_case_timing.pixel_aspect_ratio.denominator = 1;

  iface_params.interface_params.pixel_stream_params->worst_case_timing.src_frame_rate = 50000;

  iface_params.interface_params.pixel_stream_params->worst_case_timing.flags = STM_PIXELSTREAM_SRC_INTERLACED;  

  iface_params.interface_params.pixel_stream_params->worst_case_timing.colorType = STM_PIXELSTREAM_SRC_YUV_422;

  if(stm_display_device_open_plane(hDisplay, 6, &hPlane)!=0)
    printk ("ERROR : Failed to open plane\n");

  stm_display_plane_get_available_source_id(hPlane,ids, numids);

  while ((i<numids)&&(pixelstream_interface == 0))
  {
    if(stm_display_device_open_source(hDisplay, ids[i], &hSource) < 0)
      printk ("ERROR : Failed to open source %d \n", ids[i]);

    if(stm_display_source_get_interface(hSource, iface_params, (void **)&pixelstream_interface) != 0)
      printk ("No interface registred\n");

    if(pixelstream_interface != 0)
    {
      if(stm_display_plane_connect_to_source(hPlane, hSource)!=0)
        printk ("ERROR : Failed to connect plane to source\n");
      stm_display_source_pixelstream_set_input_params(pixelstream_interface, &iface_params.interface_params.pixel_stream_params->worst_case_timing);
    }
    else
    {
      stm_display_source_close(hSource);
      
      hSource=0;
    }
    i++;
  }
  if(pixelstream_interface == 0)
    printk ("ERROR : Failed to get pixel stream interface \n");
    
  printk ("Starting VXI STKPI tests...\n");

  res = stm_vxi_open(0, &hVxi);
  if(res < 0)
    printk ("ERROR : Failed to get a vxi handle with id = %d | err = %d\n",0, res);

  stm_vxi_get_capability(hVxi, &capability);
  printk ("get_capability :: number_of_inputs = %d \n", capability.number_of_inputs);

  res = stm_vxi_set_ctrl(hVxi, STM_VXI_CTRL_SELECT_INPUT, 0);
  res = stm_vxi_get_ctrl(hVxi, STM_VXI_CTRL_SELECT_INPUT, &ctrl);

  stm_display_source_pixelstream_set_signal_status (pixelstream_interface, PIXELSTREAM_SOURCE_STATUS_STABLE);
  return res;
}

static void __exit stmvxitest_exit(void)
{
  stm_display_plane_disconnect_from_source(hPlane, hSource);
  stm_display_plane_close(hPlane);
  stm_display_source_close(hSource);
  stm_display_device_close(hDisplay);

  stm_vxi_close(hVxi);
  printk ("End of  VXI STKPI tests...\n");
}

/******************************************************************************
 *  Modularization
 */
#ifdef MODULE

MODULE_AUTHOR ("STMicroelectronics");
MODULE_DESCRIPTION ("VXI driver");
MODULE_LICENSE ("GPL");

module_init (stmvxitest_init);
module_exit (stmvxitest_exit);

#endif /* MODULE */
