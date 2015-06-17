/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/pixelcapturetest/display.c
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/


#include <linux/kernel.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 2, 2)
#include <linux/export.h>
#else
#include <linux/module.h>
#endif

#include <linux/module.h> /* Needed by all modules */
#include <linux/kernel.h> /* Needed for KERN_INFO */
#include <linux/syscalls.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/fcntl.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/slab.h>   /* Needed for kmalloc and kfree */
#include <linux/wait.h>
#include <linux/spinlock_types.h>

#include <linux/bpa2.h>
#include <linux/fb.h>
#include <linux/io.h>

/*
 * This test builds against the version of stmfb.h in this source tree, rather
 * than the one that is shipped as part of the kernel headers package for
 * consistency. Normal user applications should use <linux/stmfb.h>
 */
#include <stm_display.h>
#include <stmfb.h>

#include "common.h"
#include "display.h"

/* align addr on a size boundary - adjust address up/down if needed */
#define _ALIGN_UP(addr,size)  (((addr)+((size)-1))&(~((size)-1)))
#define _ALIGN_DOWN(addr,size)  ((addr)&(~((size)-1)))

/* align addr on a size boundary - adjust address up if needed */
#define _ALIGN(addr,size)     _ALIGN_UP(addr,size)

/*
 * For the moment Display doesn't support automatic window mode for GFX
 * planes.
 *
 * Enable this flag when Display becomes supporting this mode.
 */
//#define CAPTURE_SET_WINDOWS_AUTOMATIC_MODE

/*************************************************************/
/**                                                         **/
/**  Set Plane Input and Output Windows for MANUAL_MODE     **/
/**                                                         **/
/**                                                         **/
/*************************************************************/
static int plane_set_io_windows (const stm_display_plane_h const plane,
                                 struct stm_display_io_windows  io_windows,
                                 int mode_auto)
{
  if(!mode_auto)
  {
    stm_rect_t    plane_window;

    /*
     * Set the Input and Output windows sizes and put them in "Manual" mode.
     * Do not enable plane scaling as this will be impacting the displayed
     * picture quality in case both Plane and Capture scaling are enabled then
     * you cannot decide whatever quality is good or not!
     */
    plane_window      = io_windows.output_window;

    if(stm_display_plane_set_compound_control(plane, PLANE_CTRL_INPUT_WINDOW_VALUE, &plane_window)<0)
    {
      printk("Unable to set the Input window rect\n");
      return signal_pending(current)?-ERESTARTSYS:-EINVAL;
    }

    if(stm_display_plane_set_compound_control(plane, PLANE_CTRL_OUTPUT_WINDOW_VALUE, &plane_window)<0)
    {
      printk("Unable to set the Output window rect\n");
      return signal_pending(current)?-ERESTARTSYS:-EINVAL;
    }

    if(stm_display_plane_set_control(plane, PLANE_CTRL_INPUT_WINDOW_MODE, MANUAL_MODE)<0)
    {
      printk("Unable to set the Input window mode\n");
      return signal_pending(current)?-ERESTARTSYS:-EINVAL;
    }

    if(stm_display_plane_set_control(plane, PLANE_CTRL_OUTPUT_WINDOW_MODE, MANUAL_MODE)<0)
    {
      printk("Unable to set the Output window mode\n");
      return signal_pending(current)?-ERESTARTSYS:-EINVAL;
    }
  }
  else
  {
    if(stm_display_plane_set_control(plane, PLANE_CTRL_INPUT_WINDOW_MODE, AUTO_MODE)<0)
    {
      printk("Unable to set the Input window mode\n");
      return signal_pending(current)?-ERESTARTSYS:-EINVAL;
    }

    if(stm_display_plane_set_control(plane, PLANE_CTRL_OUTPUT_WINDOW_MODE, AUTO_MODE)<0)
    {
      printk("Unable to set the Output window mode\n");
      return signal_pending(current)?-ERESTARTSYS:-EINVAL;
    }
  }
  return 0;
}


/*************************************************************/
/**                                                         **/
/**  Fill Display Buffer descriptor                         **/
/**                                                         **/
/**                                                         **/
/*************************************************************/
int display_fill_buffer_descriptor(stm_display_buffer_t * pDisplayBuffer,
                            stm_pixel_capture_buffer_descr_t CaptureBuffer,
                            stm_pixel_capture_input_params_t InputParams)
{
  stm_pixel_format_t      ColorFmt;
  uint32_t                PixelDepth;
  unsigned long           BufferAdd;
  unsigned long           ChromaOffset = CaptureBuffer.chroma_offset;
  bool                    show_bottom_field;

  memset(pDisplayBuffer, 0, sizeof(stm_display_buffer_t) );

  /* update to the new captured buffer */
  switch (CaptureBuffer.cap_format.format) {
    case STM_PIXEL_FORMAT_RGB565:
      ColorFmt      = SURF_RGB565;
      PixelDepth    = 16;
      BufferAdd     = CaptureBuffer.rgb_address;
      break;
    case STM_PIXEL_FORMAT_ARGB1555:
      ColorFmt      = SURF_ARGB1555;
      PixelDepth    = 16;
      BufferAdd     = CaptureBuffer.rgb_address;
      break;
    case STM_PIXEL_FORMAT_ARGB4444:
      ColorFmt      = SURF_ARGB4444;
      PixelDepth    = 16;
      BufferAdd     = CaptureBuffer.rgb_address;
      break;
    case STM_PIXEL_FORMAT_RGB888:
      ColorFmt      = SURF_RGB888;
      PixelDepth    = 24;
      BufferAdd     = CaptureBuffer.rgb_address;
      break;
    case STM_PIXEL_FORMAT_ARGB8565:
      ColorFmt      = SURF_ARGB8565;
      PixelDepth    = 24;
      BufferAdd     = CaptureBuffer.rgb_address;
      break;
    case STM_PIXEL_FORMAT_ARGB8888:
      ColorFmt      = SURF_ARGB8888;
      PixelDepth    = 32;
      BufferAdd     = CaptureBuffer.rgb_address;
      break;

    case STM_PIXEL_FORMAT_YUV_NV12:
      ColorFmt      = SURF_YCbCr420R2B;
      PixelDepth    = 8;
      BufferAdd     = CaptureBuffer.luma_address;
      /*
       * Hardware supporting these formats reads 32pixel chunks.
       */
      CaptureBuffer.cap_format.stride = (CaptureBuffer.cap_format.stride + 31) & ~31;
      break;
    case STM_PIXEL_FORMAT_YUV_NV16:
      ColorFmt      = SURF_YCbCr422R2B;
      PixelDepth    = 8;
      BufferAdd     = CaptureBuffer.luma_address;
      /*
       * Hardware supporting these formats reads 8pixel chunks.
       */
      CaptureBuffer.cap_format.stride = (CaptureBuffer.cap_format.stride + 7) & ~7;
      break;
    case STM_PIXEL_FORMAT_YUV:
      ColorFmt      = SURF_CRYCB888;
      PixelDepth    = 24;
      BufferAdd     = CaptureBuffer.luma_address;
      break;
    case STM_PIXEL_FORMAT_YCbCr422R:
      ColorFmt      = SURF_YCBCR422R;
      PixelDepth    = 16;
      BufferAdd     = CaptureBuffer.luma_address;
      break;

    default:
      ColorFmt      = SURF_ARGB8888;
      PixelDepth    = 32;
      BufferAdd     = CaptureBuffer.rgb_address;
      break;
  }

  pDisplayBuffer->src.primary_picture.video_buffer_addr    = (uint32_t)BufferAdd;
  pDisplayBuffer->src.primary_picture.video_buffer_size    = CaptureBuffer.length;
  pDisplayBuffer->src.primary_picture.chroma_buffer_offset = ChromaOffset;
  pDisplayBuffer->src.primary_picture.color_fmt            = ColorFmt;
  pDisplayBuffer->src.primary_picture.pixel_depth          = PixelDepth;
  pDisplayBuffer->src.primary_picture.pitch                = CaptureBuffer.cap_format.stride;
  pDisplayBuffer->src.primary_picture.width                = CaptureBuffer.cap_format.width;
  pDisplayBuffer->src.primary_picture.height               = CaptureBuffer.cap_format.height;

  pDisplayBuffer->src.visible_area.x       = 0;
  pDisplayBuffer->src.visible_area.y       = 0;
  pDisplayBuffer->src.visible_area.width   = CaptureBuffer.cap_format.width;
  pDisplayBuffer->src.visible_area.height  = CaptureBuffer.cap_format.height;

  pDisplayBuffer->src.pixel_aspect_ratio.numerator   = InputParams.pixel_aspect_ratio.numerator;
  pDisplayBuffer->src.pixel_aspect_ratio.denominator = InputParams.pixel_aspect_ratio.denominator;

  pDisplayBuffer->src.src_frame_rate.numerator   = 1;
  pDisplayBuffer->src.src_frame_rate.denominator = 1;

  pDisplayBuffer->src.linear_center_percentage = 100;

  pDisplayBuffer->src.ColorKey.flags = SCKCF_NONE;
  pDisplayBuffer->src.ColorKey.enable = '\0';
  pDisplayBuffer->src.ColorKey.format = SCKCVF_RGB;
  pDisplayBuffer->src.ColorKey.r_info = SCKCCM_DISABLED;
  pDisplayBuffer->src.ColorKey.g_info = SCKCCM_DISABLED;
  pDisplayBuffer->src.ColorKey.b_info = SCKCCM_DISABLED;
  pDisplayBuffer->src.ColorKey.minval = 0;
  pDisplayBuffer->src.ColorKey.maxval = 0;

  pDisplayBuffer->src.clut_bus_address = 0;
  pDisplayBuffer->src.post_process_luma_type = 0;
  pDisplayBuffer->src.post_process_chroma_type = 0;

  show_bottom_field = (CaptureBuffer.cap_format.flags == STM_PIXEL_CAPTURE_BUFFER_BOTTOM_ONLY) ? true:false;

  if (!(CaptureBuffer.cap_format.flags & STM_PIXEL_CAPTURE_BUFFER_INTERLACED))
  {
    pDisplayBuffer->src.flags = 0;
  }
  else
  {
    /*
     * As we are queuing a single field from a single buffer; this is
     * basically like a "pause" trick mode. We need to indicate that we
     * prefer to use an interpolated version of the "other" field for
     * an interlaced display rather than the "real data" for that field in
     * the buffer using STM_BUFFER_SRC_INTERPOLATE_FIELDS.
     *
     * When the video pipeline has a de-interlacer which is
     * active this doesn't actually make a difference, we always use the
     * de-interlaced frame to generate both output fields. But when the
     * de-interlacer is switched off or doesn't exist this determines what
     * buffer data we will really use.
     */
    pDisplayBuffer->src.flags = STM_BUFFER_SRC_INTERLACED /*| STM_BUFFER_SRC_INTERPOLATE_FIELDS*/;
    /*pDisplayBuffer->src.flags |= show_bottom_field?STM_BUFFER_SRC_BOTTOM_FIELD_ONLY:STM_BUFFER_SRC_TOP_FIELD_ONLY;*/

    /*
     * For interlaced buffers we need an even number of lines.
     * We will round up and try and provide more than the number of
     * lines requested, assuming this doesn't go over the maximum allowed.
     */
    /*pDisplayBuffer->src.primary_picture.height += pDisplayBuffer->src.primary_picture.height % 2;*/
  }

  pDisplayBuffer->info.ulFlags = STM_BUFFER_PRESENTATION_PERSISTENT;
  pDisplayBuffer->info.nfields = 1;

  /* Set buffer full opaque */
  pDisplayBuffer->src.ulConstAlpha = 255;
  pDisplayBuffer->src.flags |= STM_BUFFER_SRC_CONST_ALPHA;

  /* Set premultiplied alpha */
  pDisplayBuffer->src.flags |= STM_BUFFER_SRC_PREMULTIPLIED_ALPHA;

  /* Non decimated buffers */
  pDisplayBuffer->src.horizontal_decimation_factor = STM_NO_DECIMATION;
  pDisplayBuffer->src.vertical_decimation_factor   = STM_NO_DECIMATION;

/*
  printk ("input_params: %d-%d@%dx%d - flags = %u\n", InputParams.active_window.x, InputParams.active_window.y,
            InputParams.active_window.width, InputParams.active_window.height,
            pDisplayBuffer->src.flags);

  printk ("src visible_area: %d-%d@%dx%d - flags = %u\n", pDisplayBuffer->src.visible_area.x, pDisplayBuffer->src.visible_area.y,
            pDisplayBuffer->src.visible_area.width, pDisplayBuffer->src.visible_area.height,
            pDisplayBuffer->src.flags);
*/
  return 0;

}
EXPORT_SYMBOL(display_fill_buffer_descriptor);

/*************************************************************/
/**                                                         **/
/** Initialize Display Context (TODO: create update thread) **/
/**                                                         **/
/**                                                         **/
/*************************************************************/
static struct stm_capture_display_context *initialize_display_context(stm_display_device_h hDev, stm_display_output_h hOutput,
                                                      stm_display_plane_h hPlane, stm_display_source_h hSource,
                                                      stm_display_source_queue_h  hQueue,
                                                      struct stm_display_io_windows io_windows,
                                                      int tunneling)
{
  struct stm_capture_display_context *Context;
  uint32_t timingID;

  if(stm_display_output_get_timing_identifier(hOutput, &timingID)<0)
    return NULL;

  Context = (struct stm_capture_display_context *)kmalloc(sizeof(struct stm_capture_display_context),GFP_KERNEL);
  if(!Context)
    return NULL;

  init_waitqueue_head (&Context->frameupdate);

  Context->hDev       = hDev;
  Context->hOutput    = hOutput;
  Context->hPlane     = hPlane;
  Context->hSource    = hSource;
  Context->hQueue     = hQueue;
  Context->timingID   = timingID;
  Context->io_windows = io_windows;
  Context->tunneling  = tunneling;

  /* Create thread */
  /* Create display thread */

  return Context;
}


/*************************************************************/
/**                                                         **/
/** Terminate Display Context (TODO: delete update thread)  **/
/**                                                         **/
/**                                                         **/
/*************************************************************/
static void terminate_display_context(struct stm_capture_display_context *pDisplayContext)
{
  if(pDisplayContext)
  {
    if(!pDisplayContext->hPlane || !pDisplayContext->hOutput)
    {
      printk ("Invalide Plane (%p) or Output (%p) handles!\n", pDisplayContext->hPlane, pDisplayContext->hOutput);
      return;
    }

    if(pDisplayContext->hSource)
    {
      if(!pDisplayContext->tunneling)
      {
        // Unlock this queue for our exclusive usage
        stm_display_source_queue_unlock(pDisplayContext->hQueue);
        stm_display_source_queue_release(pDisplayContext->hQueue);
      }

      stm_display_plane_disconnect_from_source(pDisplayContext->hPlane, pDisplayContext->hSource);
      stm_display_source_close(pDisplayContext->hSource);
    }
    stm_display_plane_disconnect_from_output(pDisplayContext->hPlane, pDisplayContext->hOutput);

    stm_display_plane_close(pDisplayContext->hPlane);
    stm_display_output_close(pDisplayContext->hOutput);
    stm_display_device_close(pDisplayContext->hDev);

    kfree(pDisplayContext);
  }
}


/*************************************************************/
/**                                                         **/
/**  Get and Connect GFX plane to the specified Output      **/
/**                                                         **/
/**                                                         **/
/*************************************************************/
stm_display_plane_h get_and_connect_gfx_plane_to_output(stm_display_device_h dev, stm_display_output_h hOutput, const char *name)
{
  stm_display_plane_h       hPlane = 0;
  uint32_t                  planeID[10];
  int                       nb_plane_found=0;
  stm_plane_capabilities_t  planeCapsValue, planeCapsMask;
  uint32_t                  ctrlVal = 0;
  bool                      check_plane_name = false;
  int                       ret,i;

  /* By default we will try to get any available GFX plane */
  if(strcmp(name, "Any"))
  {
    check_plane_name = true;
  }

  /*
   * Capabilites value for plane cannot depends on output path.
   * Check on the main output controls.
   */
  if(stm_display_output_get_control(hOutput, OUTPUT_CTRL_VIDEO_SOURCE_SELECT, &ctrlVal) < 0)
  {
    printk( KERN_ERR "%s: failed to get output ctrl OUTPUT_CTRL_VIDEO_SOURCE_SELECT\n",__func__);
    return 0;
  }

  switch(ctrlVal){
    case STM_VIDEO_SOURCE_MAIN_COMPOSITOR:
    case STM_VIDEO_SOURCE_MAIN_COMPOSITOR_BYPASS:
      /* Main Output (YPbPb or/and HDMI or/and DVO) */
      planeCapsValue = (stm_plane_capabilities_t)(PLANE_CAPS_GRAPHICS|PLANE_CAPS_PRIMARY_OUTPUT);
      break;
    case STM_VIDEO_SOURCE_AUX_COMPOSITOR:
    case STM_VIDEO_SOURCE_AUX_COMPOSITOR_BYPASS:
      /* Aux Output */
      planeCapsValue = (stm_plane_capabilities_t)(PLANE_CAPS_GRAPHICS|PLANE_CAPS_SECONDARY_OUTPUT);
      break;
    default:
      printk( KERN_ERR "%s: failed to find an appropriate plane capabilities\n",__func__);
      return 0;
  }
  planeCapsMask = planeCapsValue;

  if((nb_plane_found = stm_display_device_find_planes_with_capabilities(dev, planeCapsValue, planeCapsMask, planeID, 10)) <= 0)
  {
    printk(KERN_ERR "Failed to find a suitable video plane!\n");
    return 0;
  }

  for(i=0;i<nb_plane_found;i++) // Iterate for matching planes
  {
    const char *outname;
    const char *planename;

    if( stm_display_device_open_plane(dev, planeID[i],&hPlane) != 0)
    {
      printk(KERN_ERR "Failed to find a graphics plane for the output \n");
      return 0;
    }

    // try to connect the plane on the prefered output
    ret = stm_display_plane_connect_to_output(hPlane, hOutput);
    if (ret == 0 || ret == -EALREADY)
    {
      stm_display_output_get_name(hOutput, &outname);
      stm_display_plane_get_name(hPlane, &planename);
      if(!check_plane_name || !strcmp(planename, name))
      {
        printk(KERN_INFO "Plane \"%s\" successfully connected to Output \"%s\"\n",planename, outname);
        return hPlane; // break as it is ok
      }
      /*
       * We have succeed the connection... We are not going to use this plane
       * as it doesn't match to the one we are fetching so disconnect it from
       * the output before continuing.
       */
      stm_display_plane_disconnect_from_output(hPlane, hOutput);
    }
    stm_display_plane_close(hPlane);
    hPlane = 0;
  }

  return 0;
}


/*************************************************************/
/**                                                         **/
/**  Get and Connect Video plane to the specified Output    **/
/**                                                         **/
/**                                                         **/
/*************************************************************/
stm_display_plane_h get_and_connect_video_plane_to_output(stm_display_device_h dev, stm_display_output_h hOutput, const char *name)
{
  stm_display_plane_h       hPlane = 0;
  uint32_t                  planeID[10];
  int                       nb_plane_found=0;
  uint32_t                  ctrlVal = 0;
  int                       ret,i;
  stm_plane_capabilities_t  planeCapsValue, planeCapsMask;
  bool                      check_plane_name = false;

  /* By default we will try to get any available GFX plane */
  if(strcmp(name, "Any"))
  {
    check_plane_name = true;
  }

  /*
   * Capabilites value for plane cannot depends on output path.
   * Check on the main output controls.
   */
  if(stm_display_output_get_control(hOutput, OUTPUT_CTRL_VIDEO_SOURCE_SELECT, &ctrlVal) < 0)
  {
    printk( KERN_ERR "%s: failed to get output ctrl OUTPUT_CTRL_VIDEO_SOURCE_SELECT\n",__func__);
    return 0;
  }

  switch(ctrlVal){
    case STM_VIDEO_SOURCE_MAIN_COMPOSITOR:
    case STM_VIDEO_SOURCE_MAIN_COMPOSITOR_BYPASS:
      /* Select the Main video plane */
      planeCapsValue  = (stm_plane_capabilities_t) (PLANE_CAPS_VIDEO | PLANE_CAPS_PRIMARY_OUTPUT);
      break;
    case STM_VIDEO_SOURCE_AUX_COMPOSITOR:
    case STM_VIDEO_SOURCE_AUX_COMPOSITOR_BYPASS:
      /* Select the Aux video plane */
      planeCapsValue  = (stm_plane_capabilities_t) (PLANE_CAPS_VIDEO | PLANE_CAPS_SECONDARY_OUTPUT);
      break;
    default:
      printk( KERN_ERR "%s: failed to find an appropriate plane capabilities\n",__func__);
      return 0;
  }

  planeCapsMask = planeCapsValue;

  if((nb_plane_found = stm_display_device_find_planes_with_capabilities(dev, planeCapsValue, planeCapsMask, planeID, 10)) <= 0)
  {
    printk(KERN_ERR "Failed to find a suitable video plane!\n");
    return 0;
  }

  for(i=0;i<nb_plane_found;i++)
  {
    if( stm_display_device_open_plane(dev, planeID[i], &hPlane) != 0)
    {
      printk(KERN_ERR "Failed to find a video plane for the output \n");
      return 0;
    }

    // try to connect the plane on the output
    ret = stm_display_plane_connect_to_output(hPlane, hOutput);
    if (ret == 0 || ret == -EALREADY)
    {
      const char *outname;
      const char *planename;

      stm_display_output_get_name(hOutput,&outname);
      stm_display_plane_get_name(hPlane, &planename);
      if(!check_plane_name || !strcmp(planename, name))
      {
        printk(KERN_INFO "Plane \"%s\" successfully connected to Output \"%s\"\n",planename, outname);
        return hPlane; // break as it is ok
      }
      /*
       * We have succeed the connection... We are not going to use this plane
       * as it doesn't match to the one we are fetching so disconnect it from
       * the output before continuing.
       */
      stm_display_plane_disconnect_from_output(hPlane, hOutput);
    }
  }

  return hPlane;
}


/*************************************************************/
/**                                                         **/
/**  Get a plane with the reqired name and and connect it   **/
/**  to the specified Output                                **/
/**                                                         **/
/*************************************************************/
stm_display_plane_h get_and_connect_my_plane_to_output(stm_display_device_h dev, stm_display_output_h hOutput, const char *name)
{
  stm_display_plane_h       hPlane = 0;

  printk(KERN_INFO "%s : try to find %s plane\n", __func__, name);

  /* try to get a GFX plane which is available for us */
  hPlane = get_and_connect_gfx_plane_to_output(dev, hOutput, name);
  if(!hPlane)
  {
    /* try to find a valid video plane */
    hPlane = get_and_connect_video_plane_to_output(dev, hOutput, name);
  }

  return hPlane;
}


/*************************************************************/
/**                                                         **/
/**  Get and Connect a Source to the specified plane        **/
/**  This gets and locks (try) the source queue buffer      **/
/**  interface                                              **/
/**                                                         **/
/*************************************************************/
stm_display_source_h get_and_connect_source_to_plane(stm_display_device_h dev,
                                                     stm_display_plane_h pPlane,
                                                     stm_display_source_queue_h *pQueue,
                                                     int tunneling)
{
  stm_display_source_h  Source = 0;
  uint32_t              sourceID;
  stm_display_source_interface_params_t iface_params;
  int                   ret;

  for(sourceID=0 ; ; sourceID++)  // Iterate available sources
  {
    if(stm_display_device_open_source(dev, sourceID,&Source) != 0)
    {
      printk(KERN_ERR "Failed to get a source for plane \n");
      return 0;
    }

    if(!tunneling)
    {
      /*
       * Lock the QueueBuffer Interface for our exclusive queue_buffer usage
       */
      iface_params.interface_type = STM_SOURCE_QUEUE_IFACE;
      if(stm_display_source_get_interface(Source, iface_params, (void*)pQueue) != 0)
      {
        printk(KERN_ERR "No interface registred\n");
        stm_display_source_close(Source);
        Source = 0;
        continue;
      }

      // try to lock the source interface for our exclusif usage
      if(stm_display_source_queue_lock(*pQueue, false)<0)
      {
        printk(KERN_ERR "Cannot lock queue\n");
        /* This source is already used to feed another plane */
        stm_display_source_close(Source);
        Source = 0;
        continue;
      }
    }

    // Now we can connect the source on the prefered plane
    ret = stm_display_plane_connect_to_source(pPlane, Source);
    if (ret == 0 || ret == -EALREADY)
    {
      printk(KERN_ERR "Plane %p successfully connected to Source %p\n",pPlane, Source);
      return Source; // break as it is ok
    }

    stm_display_source_queue_unlock(*pQueue);
    stm_display_source_close(Source);
    *pQueue = 0;
    Source = 0;
  }
  printk(KERN_ERR "Failed to get a source for plane %p\n",pPlane);
  return 0;
}


/*************************************************************/
/**                                                         **/
/**  Get STKPI handle of the Analog Main Output             **/
/**                                                         **/
/**                                                         **/
/*************************************************************/
static stm_display_output_h get_analog_output(stm_display_device_h dev, bool findmain)
{
  stm_display_output_h       hOutput = 0;
  uint32_t                   outputID[10];
  int i, num_ids;

  stm_display_output_capabilities_t  outputCapsValue, outputCapsMask;

  /*
   * OutputCapsValue should be initialed to OUTPUT_CAPS_DISPLAY_TIMING_MASTER.
   * No need for other caps as all master outputs now have same
   */
  outputCapsValue = OUTPUT_CAPS_DISPLAY_TIMING_MASTER;

  outputCapsMask = outputCapsValue;

  if((num_ids = stm_display_device_find_outputs_with_capabilities(dev, outputCapsValue, outputCapsMask, outputID, 10)) <= 0)
  {
    printk(KERN_ERR "Failed to find a suitable output!\n");
    return 0;
  }

  for(i=0; i < num_ids; i++)
  {
    uint32_t ctrlVal = 0;
    if( stm_display_device_open_output(dev, outputID[i], &hOutput) != 0)
    {
      printk(KERN_ERR "Failed to get an output handle!\n");
      return 0;
    }

    /*
     * Capabilites value for analog outputs are same now.
     * Check on the output controls.
     */
    if(stm_display_output_get_control(hOutput, OUTPUT_CTRL_VIDEO_SOURCE_SELECT, &ctrlVal) < 0)
    {
      printk( KERN_ERR "%s: failed to get output ctrl OUTPUT_CTRL_VIDEO_SOURCE_SELECT\n",__func__);
      continue;
    }

    if(findmain)
    {
      if(ctrlVal == STM_VIDEO_SOURCE_MAIN_COMPOSITOR)
        break;
    }
    else
    {
      if(ctrlVal == STM_VIDEO_SOURCE_AUX_COMPOSITOR)
        break;
    }

    stm_display_output_close(hOutput);
    hOutput = NULL;
  }

  if(hOutput)
  {
    const char *outname;
    stm_display_output_get_name(hOutput,&outname);
    printk(KERN_ERR "Successfully got a suitable Output \"%s\"\n", outname);
  }

  return hOutput;
}


/*************************************************************/
/**                                                         **/
/**  Setup the Main display through VIBE KPIs               **/
/**                                                         **/
/**                                                         **/
/*************************************************************/
int setup_main_display(uint32_t timingID, stm_pixel_capture_input_params_t *InputParams,
                                  struct stm_capture_display_context       **pDisplayContext,
                                  struct stm_display_io_windows            io_windows,
                                  char *plane_name,
                                  int main_to_aux,
                                  int stream_inj_ena,
                                  int tunneling)
{
  stm_display_device_h        hDev      = 0;
  stm_display_output_h        hOutput   = 0;
  stm_display_plane_h         hPlane    = 0;
  stm_display_source_h        hSource   = 0;
  stm_display_source_queue_h  hQueue    = 0;
  int                         device    = 0;
  stm_display_mode_t          ModeLine  = { STM_TIMING_MODE_RESERVED };
  stm_ycbcr_colorspace_t      colorspace;
  stm_rational_t              displayAspectRatio;

  if(stm_display_open_device(device,&hDev)!=0)
  {
    printk(KERN_ERR "Display device not found\n");
    return -1;
  }

  /* Setup Capture input params */

  if(stream_inj_ena)
  {
    /* Fixed scenario for 800x300 input params */
    InputParams->active_window.x                = 16;
    InputParams->active_window.y                = 30;
    InputParams->active_window.width            = 720;
    InputParams->active_window.height           = 240;
    InputParams->pixel_aspect_ratio.numerator   = 16;
    InputParams->pixel_aspect_ratio.denominator = 9;
    InputParams->src_frame_rate                 = 50000;
    InputParams->color_space                    = STM_PIXEL_CAPTURE_RGB/*STM_PIXEL_CAPTURE_BT709*/;
    InputParams->htotal                         = 800; /* now begin used by dvp driver */
    InputParams->vtotal                         = 300; /* now begin used by dvp driver */
    InputParams->flags                          = 0;
  }
  else
  {
    hOutput = get_analog_output(hDev, (main_to_aux & 0x1));
    if(!hOutput)
    {
      printk(KERN_ERR "Unable to get output\n");
      goto out_error;
    }

    if(stm_display_output_get_current_display_mode(hOutput, &ModeLine)<0)
    {
      printk(KERN_ERR "Unable to use requested display mode\n");
      goto out_error;
    }

    /* Setup capture input parameters according to current input mode */
    InputParams->active_window.x                = ModeLine.mode_params.active_area_start_pixel;
    InputParams->active_window.y                = ModeLine.mode_params.active_area_start_line;
    InputParams->active_window.width            = ModeLine.mode_params.active_area_width;
    InputParams->active_window.height           = ModeLine.mode_params.active_area_height;
    InputParams->src_frame_rate                 = ModeLine.mode_params.vertical_refresh_rate;
    InputParams->vtotal                         = ModeLine.mode_timing.vsync_width;
    InputParams->htotal                         = ModeLine.mode_timing.hsync_width;
    if(ModeLine.mode_params.scan_type == STM_PROGRESSIVE_SCAN)
      InputParams->flags                          = 0;
    else
      InputParams->flags                          = STM_PIXEL_CAPTURE_BUFFER_INTERLACED;

    if(stm_display_output_get_control(hOutput, OUTPUT_CTRL_YCBCR_COLORSPACE, &colorspace)<0)
    {
      printk(KERN_ERR "Unable to get output color space info\n");
      goto out_error;
    }

    /* Default to YUV444 input format */
    InputParams->pixel_format = STM_PIXEL_FORMAT_YUV;

    switch(colorspace)
    {
      case STM_YCBCR_COLORSPACE_AUTO_SELECT:
        InputParams->color_space                    =  (ModeLine.mode_params.output_standards & STM_OUTPUT_STD_HD_MASK)?STM_PIXEL_CAPTURE_BT709:STM_PIXEL_CAPTURE_RGB;
        break;
      case STM_YCBCR_COLORSPACE_709:
        InputParams->color_space                    = STM_PIXEL_CAPTURE_BT709;
        break;
      case STM_YCBCR_COLORSPACE_601:
        InputParams->color_space                    = STM_PIXEL_CAPTURE_BT601;
        break;
      default:
        InputParams->color_space                    = STM_PIXEL_CAPTURE_RGB;
        InputParams->pixel_format                   = STM_PIXEL_FORMAT_RGB888;
        break;
    }

    if(stm_display_output_get_compound_control(hOutput, OUTPUT_CTRL_DISPLAY_ASPECT_RATIO, &displayAspectRatio)<0)
    {
      printk(KERN_ERR "Unable to get output color space info\n");
      goto out_error;
    }

    InputParams->pixel_aspect_ratio.numerator   = displayAspectRatio.numerator;
    InputParams->pixel_aspect_ratio.denominator = displayAspectRatio.denominator;

    stm_display_output_close(hOutput);
  }

  /* Get the display output */
  hOutput = get_analog_output(hDev, (main_to_aux & 0x2));
  if(!hOutput)
  {
    printk(KERN_ERR "Unable to get output\n");
    goto out_error;
  }

  hPlane = get_and_connect_my_plane_to_output(hDev, hOutput, plane_name);
  if(!hPlane)
  {
    printk(KERN_ERR "Unable to get graphics plane\n");
    goto out_error;
  }

  if(plane_set_io_windows(hPlane, io_windows, 0)<0)
  {
    printk(KERN_ERR "Cannot configure the plane\n");
    goto out_error;
  }

  /*
   * Get a Source and try to connect it on created plane
   */
  hSource = get_and_connect_source_to_plane(hDev ,hPlane, &hQueue, tunneling);
  if (!hSource)
  {
    printk(KERN_ERR "Unable to get a source for plane %p\n", hPlane);
    goto out_error;
  }

  if((*pDisplayContext = initialize_display_context(hDev, hOutput, hPlane, hSource, hQueue, io_windows, tunneling)) == 0)
  {
    printk(KERN_ERR "Unable to start display update thread\n");
    goto out_error;
  }

  return 0;

out_error:
  if(!tunneling)
  {
    if(hQueue)
    {
      stm_display_source_queue_unlock(hQueue);
      stm_display_source_queue_release(hQueue);
    }
  }
  if(hSource)
    stm_display_source_close(hSource);
  if(hPlane)
    stm_display_plane_close(hPlane);
  if(hOutput)
    stm_display_output_close(hOutput);
  if(hDev)
    stm_display_device_close(hDev);

  return -1;
}

/*************************************************************/
/**                                                         **/
/**  Free the Main display handles                          **/
/**                                                         **/
/**                                                         **/
/*************************************************************/
void free_main_display_ressources(struct stm_capture_display_context *pDisplayContext)
{
  terminate_display_context(pDisplayContext);
}
