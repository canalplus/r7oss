/***********************************************************************
 *
 * File: linux/kernel/drivers/video/stmfbioctl.c
 * Copyright (c) 2000, 2004, 2005 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fb.h>
#include <linux/device.h>
#include <linux/platform_device.h>

#include <asm/uaccess.h>
#include <linux/semaphore.h>

#include <stm_display.h>
#include <linux/stm/stmcoredisplay.h>

#include "stmfb.h"
#include "stmfbinfo.h"


static int stmfb_set_picture_configuration(struct stmfbio_picture_configuration *cfg, struct stmfb_info *i)
{
  stm_display_metadata_t                    *metadata;
  stm_picture_format_info_t                 *pic_info;
  stm_display_output_connection_status_t     hdmistatus;

  unsigned long flags;
  int ret = 0;

  if((cfg->flags & STMFBIO_PICTURE_FLAGS_PICUTRE_ASPECT) &&
     (cfg->picture_aspect > STMFBIO_PIC_PICTURE_ASPECT_16_9))
    return -EINVAL;

  if((cfg->flags & STMFBIO_PICTURE_FLAGS_VIDEO_ASPECT) &&
     (cfg->video_aspect > STMFBIO_PIC_VIDEO_ASPECT_GT_16_9))
    return -EINVAL;

  if((cfg->flags & STMFBIO_PICTURE_FLAGS_LETTERBOX) &&
     (cfg->letterbox_style > STMFBIO_PIC_LETTERBOX_SAP_4_3))
    return -EINVAL;

  if((cfg->flags & STMFBIO_PICTURE_FLAGS_RESCALE_INFO) &&
     (cfg->picture_rescale > STMFBIO_PIC_RESCALE_BOTH))
    return -EINVAL;

  if((metadata = kzalloc(sizeof(stm_display_metadata_t)+sizeof(stm_picture_format_info_t),GFP_KERNEL)) == 0)
    return -ENOMEM;

  metadata->size      = sizeof(stm_display_metadata_t)+sizeof(stm_picture_format_info_t);
  metadata->type      = STM_METADATA_TYPE_PICTURE_INFO;
  /*
   * We need to hold a reference for the metadata as we need to queue
   * the same thing twice (to analogue and HDMI outputs) and need to ensure we
   * do not get any race conditions in the lifetime of the object. We use the
   * metadata addref/release internal helpers to manage this. We must use
   * the release under interrupt lock once a queue has been successful to
   * ensure it is atomic with the VTG interrupt processing, but we actually do
   * this all the time to make the point and stop the code from
   * breaking in future changes.
   *
   * NOTE: this is not SMP safe because we are only locking interrupts on
   *       the CPU we are running on and the VTG interrupt could be
   *       processed by another CPU. We need to sort this out.
   */
  spin_lock_irqsave(&i->framebufferSpinLock,flags);
  stm_meta_data_addref(metadata);
  spin_unlock_irqrestore(&i->framebufferSpinLock,flags);

  metadata->release   = (void(*)(struct stm_display_metadata_s*))kfree;
  metadata->presentation_time = ((stm_time64_t)cfg->timestamp.tv_sec * USEC_PER_SEC) +
                                (stm_time64_t)cfg->timestamp.tv_usec;

  pic_info = (stm_picture_format_info_t*)&metadata->data[0];
  pic_info->flags                  = cfg->flags;
  pic_info->picture_aspect_ratio   = (stm_wss_aspect_ratio_t)cfg->picture_aspect;
  pic_info->video_aspect_ratio     = (stm_wss_aspect_ratio_t)cfg->video_aspect;
  pic_info->letterbox_style        = (stm_letterbox_style_t)cfg->letterbox_style;
  pic_info->picture_rescale        = (stm_picture_rescale_t)cfg->picture_rescale;
  pic_info->bar_data_present       = cfg->bar_enable;
  pic_info->bar_top_end_line       = cfg->top_bar_end;
  pic_info->bar_bottom_start_line  = cfg->bottom_bar_start;
  pic_info->bar_left_end_pixel     = cfg->left_bar_end;
  pic_info->bar_right_start_pixel  = cfg->right_bar_start;

  if((ret = stm_display_output_queue_metadata(i->hFBMainOutput, metadata))<0)
  {
    /*
     * If the DENC doesn't exist or isn't in use the queue will not be
     * available on the main output but that is fine, we can continue to HDMI.
     */
    if((ret != -EAGAIN) && (ret != -EOPNOTSUPP))
      goto exit;
  }

  ret = 0;

  if(i->hFBHDMI == NULL)
    goto exit;

  stm_display_output_get_connection_status(i->hFBHDMI, &hdmistatus);

  if((hdmistatus != STM_DISPLAY_CONNECTED) &&
     (metadata->presentation_time == 0LL))
  {
    /*
     * This is a slight of hand for the usual case that a change
     * is required immediately but the HDMI output is currently
     * stopped and will not process its metadata queues. Flush the queue
     * because we know this request will be the valid data as soon
     * as the HDMI output is started again.
     */
    stm_display_output_flush_metadata(i->hFBHDMI, STM_METADATA_TYPE_PICTURE_INFO);
  }

  ret = stm_display_output_queue_metadata(i->hFBHDMI, metadata);

exit:
  spin_lock_irqsave(&i->framebufferSpinLock,flags);
  stm_meta_data_release(metadata);
  spin_unlock_irqrestore(&i->framebufferSpinLock,flags);

  return ret;
}


int stmfb_ioctl(struct fb_info* fb, u_int cmd, u_long arg)
{
  struct stmfb_info* i = (struct stmfb_info* )fb;

  if(!i->platformDevice)
  {
    DPRINTK("STMFBIO invalid framebuffer device!\n");
    return -EFAULT;
  }
  else if(i->platformDevice->dev.power.power_state.event != PM_EVENT_ON)
  {
    DPRINTK("STMFBIO while in suspend state???\n");
    return -EBUSY;
  }

  switch (cmd)
  {
    case STMFBIO_GET_OUTPUTSTANDARDS:
      {
      struct stmfbio_outputstandards standards, *user = (void *) arg;

      DPRINTK("STMFBIO_GET_OUTPUTSTANDARDS\n");

      if (get_user(standards.outputid, &user->outputid))
        return -EFAULT;

      if (standards.outputid != STMFBIO_OUTPUTID_MAIN)
        return -EINVAL;

      if (down_interruptible(&i->framebufferLock))
        return -ERESTARTSYS;

      stmfb_get_outputstandards(i, &standards);

      up(&i->framebufferLock);

      if (put_user (standards.all_standards, &user->all_standards))
        return -EFAULT;

      return 0;
      }
      break;

    case STMFBIO_GET_OUTPUTINFO:
      {
      struct stmfbio_outputinfo info, *user = (void *) arg;
      int                       ret;

      DPRINTK("STMFBIO_GET_OUTPUTINFO\n");

      if (get_user(info.outputid, &user->outputid))
        return -EFAULT;

      if (info.outputid != STMFBIO_OUTPUTID_MAIN)
        return -EINVAL;

      if(down_interruptible(&i->framebufferLock))
        return -ERESTARTSYS;

      if (!i->current_videomode_valid) {
        up(&i->framebufferLock);
        return -EAGAIN;
      }
      ret = stmfb_videomode_to_outputinfo(&i->current_videomode, &info);

      up(&i->framebufferLock);

      if (!ret && copy_to_user ((void *) arg, &info, sizeof (info)))
        return -EFAULT;

      return ret;
      }
      break;
    case STMFBIO_SET_OUTPUTINFO:
      {
      struct stmfbio_outputinfo info;
      stm_display_mode_t        vm;
      int                       ret;

      DPRINTK("STMFBIO_SET_OUTPUTINFO\n");

      if (copy_from_user (&info, (void *) arg, sizeof (info)))
        return -EFAULT;

      if (info.outputid != STMFBIO_OUTPUTID_MAIN)
        return -EINVAL;

      if (down_interruptible (&i->framebufferLock))
        return -ERESTARTSYS;

      /* prevent the framebuffer kernel API from messing around w/ the
         config in the future, until all apps have exited */
      i->fbdev_api_suspended = 1;

      ret = stmfb_outputinfo_to_videomode (i, &info, &vm);
      up (&i->framebufferLock);

      if (!ret) {
        /*
         * Re-create the VAR from the exact hardware description, this gives a
         * completely clean var, which is why we have to save and restore the
         * activate flags. Without this we get spurious mode changes when they
         * should have been just tests.
         */
        enum _stmfbio_activate activate = info.activate;
        ret = stmfb_videomode_to_outputinfo (&vm, &info);
        info.activate = activate;

        if (!ret && ((activate & STMFBIO_ACTIVATE_MASK) == STMFBIO_ACTIVATE_IMMEDIATE))
        {
          if((i->current_3d_config.activate & STMFBIO_ACTIVATE_MASK) == STMFBIO_ACTIVATE_ON_NEXT_CHANGE)
            i->current_3d_config.activate = STMFBIO_ACTIVATE_IMMEDIATE;

          if((i->current_3d_config.activate & STMFBIO_ACTIVATE_MASK) == STMFBIO_ACTIVATE_IMMEDIATE)
            stmfb_set_videomode_3d_flags(i,&vm);

          ret = stmfb_set_videomode (info.outputid, &vm, i);
        }

        if (!ret && copy_to_user ((void *) arg, &info, sizeof (info)))
          return -EFAULT;
      }

      return ret;
      }
      break;

    case STMFBIO_GET_3D_CONFIG:
      {
      struct stmfbio_3d_configuration info, *user = (void *) arg;

      DPRINTK("STMFBIO_GET_3D_CONFIG\n");

      if (get_user(info.outputid, &user->outputid))
        return -EFAULT;

      if (info.outputid != STMFBIO_OUTPUTID_MAIN)
        return -EINVAL;

      if(down_interruptible(&i->framebufferLock))
        return -ERESTARTSYS;

      info = i->current_3d_config;

      up(&i->framebufferLock);

      if (copy_to_user ((void *) arg, &info, sizeof (info)))
        return -EFAULT;

      return 0;
      }
      break;
    case STMFBIO_SET_3D_CONFIG:
      {
      struct stmfbio_3d_configuration info;
      int ret;

      DPRINTK("STMFBIO_SET_3D_CONFIG\n");

      if (copy_from_user ((void *)&info, (void *) arg, sizeof (info)))
        return -EFAULT;

      if (info.outputid != STMFBIO_OUTPUTID_MAIN)
        return -EINVAL;

      if (down_interruptible (&i->framebufferLock))
        return -ERESTARTSYS;

      /*
       * If we don't have a valid videomode, change an immediate activation to
       * a deferred one; the settings will take effect when the next valid video
       * mode is set.
       */
      if(!i->current_videomode_valid && ((info.activate  & STMFBIO_ACTIVATE_MASK) == STMFBIO_ACTIVATE_IMMEDIATE))
        info.activate = STMFBIO_ACTIVATE_ON_NEXT_CHANGE;

      up (&i->framebufferLock);

      ret = stmfb_set_3d_configuration(i, &info);


      return ret;
      }
      break;

    case STMFBIO_GET_PLANEMODE:
      {
      struct stmfbio_planeinfo plane, *user = (void *) arg;

      DPRINTK("STMFBIO_GET_PLANEMODE\n");

      if (get_user (plane.layerid, &user->layerid))
        return -EFAULT;

      if (plane.layerid != 0)
        return -EINVAL;

      if (down_interruptible (&i->framebufferLock))
        return -ERESTARTSYS;

      if (!i->current_planeconfig_valid) {
        up (&i->framebufferLock);
        return -EAGAIN;
      }

      plane.config = i->current_planeconfig;
      plane.activate = STMFBIO_ACTIVATE_IMMEDIATE;

      up (&i->framebufferLock);

      if (copy_to_user ((void *) arg, &plane, sizeof (plane)))
        return -EFAULT;

      return 0;
      }
      break;
    case STMFBIO_SET_PLANEMODE:
      {
      struct stmfbio_planeinfo plane;
      int                      ret;

      DPRINTK("STMFBIO_SET_PLANEMODE\n");

      if (copy_from_user (&plane, (void *) arg, sizeof (plane)))
        return -EFAULT;

      if (plane.layerid != 0)
        return -EINVAL;

      plane.config.bitdepth = stmfb_bitdepth_for_pixelformat (plane.config.format);

      if(down_interruptible(&i->framebufferLock))
        return -ERESTARTSYS;

      ret = stmfb_verify_planeinfo (i, &plane);

      up(&i->framebufferLock);

      if (!ret && ((plane.activate & STMFBIO_ACTIVATE_MASK) == STMFBIO_ACTIVATE_IMMEDIATE))
        ret = stmfb_set_planemode (&plane, i);

      return ret;
      }
      break;
    case STMFBIO_PAN_PLANE:
      {
      struct stmfbio_plane_pan pan;
      int                      ret;

      DPRINTK("STMFBIO_PAN_PLANE\n");

      if (copy_from_user (&pan, (void *) arg, sizeof (pan)))
        return -EFAULT;

      if (pan.layerid != 0)
        return -EINVAL;

      if (down_interruptible (&i->framebufferLock))
        return -ERESTARTSYS;

      /* prevent the framebuffer kernel API from messing around w/ the
         config in the future, until all apps have exited */
      i->fbdev_api_suspended = 1;

      ret = stmfb_set_plane_pan (&pan, i);

      up (&i->framebufferLock);

      return ret;
      }
      break;

    case STMFBIO_GET_VAR_SCREENINFO_EX:
    {
      DPRINTK("STMFBIO_GET_VAR_SCREENINFO_EX\n");

      if(down_interruptible(&i->framebufferLock))
        return -ERESTARTSYS;

      if(copy_to_user((void*)arg,&i->current_var_ex,sizeof(struct stmfbio_var_screeninfo_ex)))
      {
        up(&i->framebufferLock);
        return -EFAULT;
      }

      up(&i->framebufferLock);

      break;
    }

    case STMFBIO_GET_VAR_SCREENINFO_EX2:
    {
      struct stmfbio_var_screeninfo_ex2 tmp = {0};

      DPRINTK("STMFBIO_GET_VAR_SCREENINFO_EX\n");

      if(down_interruptible(&i->framebufferLock))
        return -ERESTARTSYS;

      /*
       * If there are pending updates for the next change then try to retreive
       * actual hardware proprieties like opacity, color keying and flicker
       * filter values.
       */
      tmp = i->current_var_ex;

      if(i->current_planeconfig_valid
         && i->current_var_ex.activate == STMFBIO_ACTIVATE_ON_NEXT_CHANGE)
      {
        /* retreive opacity value */
        __u32 alpha = (__u32)tmp.opacity;

        if(stm_display_plane_get_control(i->hFBPlane, PLANE_CTRL_TRANSPARENCY_VALUE, &alpha)<0)
        {
          up(&i->framebufferLock);
          return -EFAULT;
        }
        tmp.opacity = (__u8)alpha;
      }

      /* copy back current framebuffer state to the user */
      if(copy_to_user((void*)arg,&tmp,sizeof(struct stmfbio_var_screeninfo_ex2)))
      {
        up(&i->framebufferLock);
        return -EFAULT;
      }

      up(&i->framebufferLock);

      break;
    }

    case STMFBIO_SET_VAR_SCREENINFO_EX:
    {
      struct stmfbio_var_screeninfo_ex2 tmp;
      int ret;

      DPRINTK("STMFBIO_SET_VAR_SCREENINFO_EX\n");

      if(copy_from_user(&tmp, (void*)arg, sizeof(struct stmfbio_var_screeninfo_ex)))
        return -EFAULT;

      if(down_interruptible(&i->framebufferLock))
        return -ERESTARTSYS;

      /*
       * If we don't have a valid videomode, change an immediate activation to
       * a deferred one; the settings will take effect when the next valid video
       * mode is set.
       */
      if(!i->current_planeconfig_valid
         && tmp.activate == STMFBIO_ACTIVATE_IMMEDIATE)
        tmp.activate = STMFBIO_ACTIVATE_ON_NEXT_CHANGE;

      tmp.caps &= ~STMFBIO_VAR_CAPS_FULLSCREEN;

      ret = stmfb_set_var_ex(&tmp, i);

      up(&i->framebufferLock);

      if(ret != 0)
      {
        /*
         * Copy back the var to set the failed entry
         */
        if(copy_to_user((void*)arg,&tmp, sizeof(tmp)))
          return -EFAULT;

        return ret;
      }

      break;
    }

    case STMFBIO_SET_VAR_SCREENINFO_EX2:
    {
      struct stmfbio_var_screeninfo_ex2 tmp;
      int ret;

      DPRINTK("STMFBIO_SET_VAR_SCREENINFO_EX\n");

      if(copy_from_user(&tmp, (void*)arg, sizeof(struct stmfbio_var_screeninfo_ex2)))
        return -EFAULT;

      if(down_interruptible(&i->framebufferLock))
        return -ERESTARTSYS;

      /*
       * If we don't have a valid videomode, change an immediate activation to
       * a deferred one; the settings will take effect when the next valid video
       * mode is set.
       */
      if(!i->current_planeconfig_valid
         && tmp.activate == STMFBIO_ACTIVATE_IMMEDIATE)
        tmp.activate = STMFBIO_ACTIVATE_ON_NEXT_CHANGE;

      ret = stmfb_set_var_ex(&tmp, i);

      up(&i->framebufferLock);

      if(ret != 0)
      {
        /*
         * Copy back the var to set the failed entry
         */
        if(copy_to_user((void*)arg,&tmp, sizeof(tmp)))
          return -EFAULT;

        return ret;
      }

      break;
    }

    case STMFBIO_GET_OUTPUT_CONFIG:
    {
      struct stmfbio_output_configuration tmp;
      int ret;

      DPRINTK("STMFBIO_GET_OUTPUT_CONFIG\n");

      if(copy_from_user(&tmp, (void*)arg, sizeof(tmp)))
        return -EFAULT;

      if(down_interruptible(&i->framebufferLock))
        return -ERESTARTSYS;

      ret = stmfb_get_output_configuration(&tmp,i);

      up(&i->framebufferLock);

      if(ret == 0)
      {
        if(copy_to_user((void*)arg,&tmp,sizeof(tmp)))
          ret = -EFAULT;
      }

      return ret;
    }

    case STMFBIO_SET_OUTPUT_CONFIG:
    {
      struct stmfbio_output_configuration tmp;
      int ret;

      DPRINTK("STMFBIO_SET_OUTPUT_CONFIG\n");

      if(copy_from_user(&tmp, (void*)arg, sizeof(tmp)))
        return -EFAULT;

      if(down_interruptible(&i->framebufferLock))
        return -ERESTARTSYS;

      ret = stmfb_set_output_configuration(&tmp, i);

      up(&i->framebufferLock);

      if(ret != 0)
      {
        /*
         * Copy back the var to set the failed entry
         */
        if( copy_to_user((void*)arg,&tmp, sizeof(tmp)) ) {}; //empty if condition added to remove warning
        return ret;
      }

      break;
    }


    case STMFBIO_SET_PICTURE_CONFIG:
    {
      struct stmfbio_picture_configuration tmp;
      int ret;

      DPRINTK("STMFBIO_SET_PICTURE_CONFIG\n");

      if(copy_from_user(&tmp, (void*)arg, sizeof(tmp)))
        return -EFAULT;

      if(down_interruptible(&i->framebufferLock))
        return -ERESTARTSYS;

      ret = stmfb_set_picture_configuration(&tmp, i);

      up(&i->framebufferLock);

      return ret;
    }


    case STMFBIO_GET_AUXMEMORY2:
    {
      struct stmfbio_auxmem2 auxmem;

      if (copy_from_user (&auxmem, (void *) arg, sizeof (auxmem)))
        return -EFAULT;

      if (auxmem.index >= ARRAY_SIZE (i->AuxBase))
        return -EINVAL;

      auxmem.physical = i->AuxBase[auxmem.index];
      auxmem.size     = i->AuxSize[auxmem.index];

      if (copy_to_user ((void *) arg, &auxmem, sizeof (auxmem)))
        return -EFAULT;

      break;
    }

    case STMFBIO_SET_DAC_HD_POWER:
    {
      if(down_interruptible(&i->framebufferLock))
        return -ERESTARTSYS;
      stm_display_output_set_control(i->hFBMainOutput, OUTPUT_CTRL_DAC_HD_POWER_DOWN, arg);
      up(&i->framebufferLock);
      return 0;
    }

    case STMFBIO_SET_DAC_HD_FILTER:
    {
      if(down_interruptible(&i->framebufferLock))
        return -ERESTARTSYS;
      stm_display_output_set_control(i->hFBMainOutput, OUTPUT_CTRL_DAC_HD_ALT_FILTER, arg);
      up(&i->framebufferLock);
      return 0;
    }

    case STMFBIO_SET_CGMS_ANALOG:
    {
      if(down_interruptible(&i->framebufferLock))
        return -ERESTARTSYS;
      stm_display_output_set_control(i->hFBMainOutput, OUTPUT_CTRL_SD_CGMS, arg);
      up(&i->framebufferLock);
      return 0;
    }
    case STMFBIO_SET_WSS_ANALOG:
    {
      if(down_interruptible(&i->framebufferLock))
        return -ERESTARTSYS;
      stm_display_output_set_control(i->hFBMainOutput, OUTPUT_CTRL_WSS_INSERTION, arg);
      up(&i->framebufferLock);
      return 0;
    }
    case STMFBIO_SET_CLOSED_CAPTION:
    {
      if(down_interruptible(&i->framebufferLock))
        return -ERESTARTSYS;
      stm_display_output_set_control(i->hFBMainOutput, OUTPUT_CTRL_CC_INSERTION_ENABLE, arg);
      up(&i->framebufferLock);
      return 0;
    }
    case STMFBIO_SET_VPS_ANALOG:
    {
      struct stmfbio_vps vpsconfig;
      stm_display_vps_t tmp;
      DPRINTK("STMFBIO_SET_VPS_ANALOG\n");
      if(copy_from_user(&vpsconfig, (void*)arg, sizeof(vpsconfig)))
        return -EFAULT;

      tmp.vps_enable = (vpsconfig.vps_enable != 0);
      tmp.vps_data[0] = vpsconfig.vps_data[0];
      tmp.vps_data[1] = vpsconfig.vps_data[1];
      tmp.vps_data[2] = vpsconfig.vps_data[2];
      tmp.vps_data[3] = vpsconfig.vps_data[3];
      tmp.vps_data[4] = vpsconfig.vps_data[4];
      tmp.vps_data[5] = vpsconfig.vps_data[5];

      if(down_interruptible(&i->framebufferLock))
        return -ERESTARTSYS;
      stm_display_output_set_compound_control(i->hFBMainOutput, OUTPUT_CTRL_VPS, (void*)&tmp);
      up(&i->framebufferLock);
      return 0;
    }

    case FBIO_WAITFORVSYNC:
    {
      struct stmcore_display_pipeline_data *pd = *((struct stmcore_display_pipeline_data **)i->platformDevice->dev.platform_data);

      DPRINTK("FBIO_WAITFORVSYNC\n");

      if(down_interruptible(&i->framebufferLock))
        return -ERESTARTSYS;

      if(!i->current_videomode_valid)
      {
        up(&i->framebufferLock);
        return -ENODEV;
      }

      up(&i->framebufferLock);

      interruptible_sleep_on(&pd->display_runtime->vsync_wait_queue);
      if(signal_pending(current))
        return -ERESTARTSYS;

      break;
    }

    case FBIOGET_VBLANK:
    {
      struct stmcore_display_pipeline_data *pd = *((struct stmcore_display_pipeline_data **)i->platformDevice->dev.platform_data);
      struct fb_vblank vblank;
      vblank.flags = FB_VBLANK_HAVE_COUNT;
      vblank.count = atomic_read (&pd->display_runtime->vsync_count);

      if (copy_to_user ((void *) arg, &vblank, sizeof (vblank)))
          return -EFAULT;

      break;
    }

    case STMFBIO_SET_PANEL_CONFIG:
    {
      struct stmfbio_panel_config user_panel_config;
      stm_display_panel_config_t kernel_panel_config;
      stm_display_mode_t kernel_disp_mode = {
         STM_TIMING_MODE_CUSTOM,
         { 60000, STM_PROGRESSIVE_SCAN, 1920, 1080, 24, 41,
         STM_OUTPUT_STD_UNDEFINED, {{0,0},{0,0}}, {0,0}, STM_MODE_FLAGS_NONE },
         { 2200, 1125, 148500000, STM_SYNC_NEGATIVE, 2, STM_SYNC_NEGATIVE, 6}};
      int ret;

      DPRINTK("STMFBIO_SET_PANEL_CONFIG\n");

      if (copy_from_user (&user_panel_config, (void *) arg, sizeof (user_panel_config)))
        return -EFAULT;

      if (down_interruptible (&i->framebufferLock))
        return -ERESTARTSYS;

      /* prevent the framebuffer kernel API from messing around w/ the
         config in the future, until all apps have exited */
      i->fbdev_api_suspended = 1;

      up (&i->framebufferLock);

      memset (&kernel_panel_config, 0, sizeof (kernel_panel_config));

      /* panel config parameters for compound control */
      kernel_panel_config.color_config.enable_lut1 = (bool)user_panel_config.enable_lut1;
      kernel_panel_config.color_config.enable_lut2 = (bool)user_panel_config.enable_lut2;
      kernel_panel_config.color_config.linear_color_remap_table_p = \
      (uint8_t *)user_panel_config.linear_color_remap_table_p;
      kernel_panel_config.color_config.lookup_table1_p = \
      (uint8_t *)user_panel_config.lookup_table1_p;
      kernel_panel_config.color_config.lookup_table2_p = \
      (uint8_t *)user_panel_config.lookup_table2_p;
      kernel_panel_config.dither_type = (stm_display_panel_dither_mode_t)user_panel_config.dither;
      kernel_panel_config.misc_timing_config.afr_enable = (bool)user_panel_config.afr_enable;
      kernel_panel_config.misc_timing_config.lock_method = \
      (stm_display_panel_lock_method_t)user_panel_config.lock_method;
      kernel_panel_config.misc_timing_config.is_half_display_clock = \
      (bool)user_panel_config.is_half_display_clock;
      kernel_panel_config.panel_power_up_timing.pwr_to_de_delay_during_power_on \
      = user_panel_config.pwr_to_de_delay_during_power_on;
      kernel_panel_config.panel_power_up_timing.de_to_bklt_on_delay_during_power_on \
      = user_panel_config.de_to_bklt_on_delay_during_power_on;
      kernel_panel_config.panel_power_up_timing.bklt_to_de_off_delay_during_power_off \
      = user_panel_config.bklt_to_de_off_delay_during_power_off;
      kernel_panel_config.panel_power_up_timing.de_to_pwr_delay_during_power_off \
      = user_panel_config.de_to_pwr_delay_during_power_off;

      /* panel display timing generator */
      kernel_disp_mode.mode_id = STM_TIMING_MODE_CUSTOM;
      kernel_disp_mode.mode_params.output_standards = STM_OUTPUT_STD_UNDEFINED;
      kernel_disp_mode.mode_params.scan_type = STM_PROGRESSIVE_SCAN;
      kernel_disp_mode.mode_params.active_area_start_pixel = user_panel_config.active_area_start_pixel;
      kernel_disp_mode.mode_params.active_area_width = user_panel_config.active_area_width;
      kernel_disp_mode.mode_params.active_area_start_line = user_panel_config.active_area_start_line;
      kernel_disp_mode.mode_params.active_area_height = user_panel_config.active_area_height;
      kernel_disp_mode.mode_params.vertical_refresh_rate = user_panel_config.vertical_refresh_rate;
      kernel_disp_mode.mode_timing.hsync_width = user_panel_config.hsync_width;
      kernel_disp_mode.mode_timing.vsync_width= user_panel_config.vsync_width;
      kernel_disp_mode.mode_timing.hsync_polarity = (stm_sync_polarity_t)user_panel_config.hsync_polarity;
      kernel_disp_mode.mode_timing.vsync_polarity= (stm_sync_polarity_t)user_panel_config.vsync_polarity;
      kernel_disp_mode.mode_timing.pixel_clock_freq = user_panel_config.pixel_clock_freq;
      kernel_disp_mode.mode_timing.pixels_per_line = user_panel_config.pixels_per_line;
      kernel_disp_mode.mode_timing.lines_per_frame= user_panel_config.lines_per_frame;

      /* panel config */
      ret = stm_display_output_set_compound_control(i->hFBMainOutput, \
      OUTPUT_CTRL_PANEL_CONFIGURE, &kernel_panel_config);
      if(ret)
        return ret;

      /* panel mode set */
      ret = stmfb_set_panelmode(STMFBIO_OUTPUTID_MAIN, &kernel_disp_mode, i);
      return ret;
    }
      break;

    default:
    {
      DPRINTK(" Invalid IOCTL code 0x%08x\n",cmd);
      return -ENOTTY;
    }
  }

  return 0;
}
