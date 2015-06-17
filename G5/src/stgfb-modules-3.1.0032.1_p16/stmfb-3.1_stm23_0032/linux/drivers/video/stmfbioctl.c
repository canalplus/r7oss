/***********************************************************************
 *
 * File: linux/drivers/video/stmfbioctl.c
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
#include <asm/semaphore.h>

#include <stmdisplay.h>
#include <linux/stm/stmcoredisplay.h>

#include "stmfb.h"
#include "stmfbinfo.h"


static void setDrawingState(stm_blitter_operation_t *op, STMFBIO_BLT_DATA *pData)
{
  op->ulFlags = pData->ulFlags;
  DPRINTK("Flags = 0x%08lx\n", pData->ulFlags);

  op->ulColorKeyLow  = pData->colourKey;
  op->ulColorKeyHigh = pData->colourKey;
  DPRINTK("Colour key = 0x%08lx\n", pData->colourKey);

  op->ulGlobalAlpha = pData->globalAlpha;
  DPRINTK("Global alpha = 0x%08lx\n", pData->globalAlpha);

  op->ulPlanemask = pData->planeMask;
  DPRINTK("plane mask = 0x%08lx\n", pData->planeMask);

  op->ulColour     = pData->colour;
  op->colourFormat = pData->srcFormat;

  DPRINTK("colour = 0x%08lx\n", pData->colour);
}


static int stmfbio_draw_rectangle(stm_display_blitter_t *blitter,
                                  stm_blitter_surface_t *basesurface,
                                  STMFBIO_BLT_DATA      *pData)
{
  stm_blitter_operation_t op = {0};
  stm_rect_t dst;
  int ret;

  DPRINTK("dstOffset = %lu ulFlags = 0x%08lx\n",pData->dstOffset,pData->ulFlags);


  if(pData->dst_left < pData->dst_right)
  {
    dst.left   = pData->dst_left;
    dst.right  = pData->dst_right;
  }
  else
  {
    dst.left   = pData->dst_right;
    dst.right  = pData->dst_left;
  }

  if(pData->dst_top < pData->dst_bottom)
  {
    dst.top    = pData->dst_top;
    dst.bottom = pData->dst_bottom;
  }
  else
  {
    dst.top    = pData->dst_bottom;
    dst.bottom = pData->dst_top;
  }

  DPRINTK("dst(%lu,%lu,%lu,%lu)\n",dst.left, dst.top, dst.right, dst.bottom);

  op.dstSurface.ulOffset = pData->dstOffset;
  op.dstSurface.ulWidth  = (ULONG)dst.right;
  op.dstSurface.ulHeight = (ULONG)dst.bottom;
  op.dstSurface.ulStride = pData->dstPitch;
  op.dstSurface.format   = pData->dstFormat;

  if(stm_create_subsurface(basesurface, &op.dstSurface)<0)
  {
    DPRINTK("Drawing area not contained in base surface\n");
    return -1;
  }

  setDrawingState(&op, pData);

  if (pData->operation == BLT_OP_FILL)
    ret = stm_display_blitter_fill_rect(blitter, &op, &dst);
  else
    ret = stm_display_blitter_draw_rect(blitter, &op, &dst);

  return ret;
}


static int stmfbio_do_blit(stm_display_blitter_t *blitter,
                           stm_blitter_surface_t *basesrcsurf,
                           stm_blitter_surface_t *basedstsurf,
                           unsigned long          clutaddress,
                           STMFBIO_BLT_DATA      *pData)
{
  stm_blitter_operation_t op = {0};
  stm_rect_t srcrect;
  stm_rect_t dstrect;
  int ret = 0;

  DPRINTK("src_off = 0x%lx src_pitch = 0x%lx\n", pData->srcOffset, pData->srcPitch);
  DPRINTK("dst_off = 0x%lx dst_pitch = 0x%lx\n", pData->dstOffset, pData->dstPitch);

  if(pData->dst_left < pData->dst_right)
  {
    dstrect.left   = pData->dst_left;
    dstrect.right  = pData->dst_right;
  }
  else
  {
    dstrect.left   = pData->dst_right;
    dstrect.right  = pData->dst_left;
  }

  if(pData->dst_top < pData->dst_bottom)
  {
    dstrect.top    = pData->dst_top;
    dstrect.bottom = pData->dst_bottom;
  }
  else
  {
    dstrect.top    = pData->dst_bottom;
    dstrect.bottom = pData->dst_top;
  }

  if(pData->src_left < pData->src_right)
  {
    srcrect.left   = pData->src_left;
    srcrect.right  = pData->src_right;
  }
  else
  {
    srcrect.left   = pData->src_right;
    srcrect.right  = pData->src_left;
  }

  if(pData->src_top < pData->src_bottom)
  {
    srcrect.top    = pData->src_top;
    srcrect.bottom = pData->src_bottom;
  }
  else
  {
    srcrect.top    = pData->src_bottom;
    srcrect.bottom = pData->src_top;
  }

  DPRINTK("src(%lu,%lu,%lu,%lu) -> dst(%lu,%lu,%lu,%lu)\n",
          srcrect.left,srcrect.top,srcrect.right,srcrect.bottom,
          dstrect.left,dstrect.top,dstrect.right,dstrect.bottom);

  op.dstSurface.ulOffset = pData->dstOffset;
  op.dstSurface.ulWidth  = dstrect.right;
  op.dstSurface.ulHeight = dstrect.bottom;
  op.dstSurface.ulStride = pData->dstPitch;
  op.dstSurface.format   = pData->dstFormat;

  if(stm_create_subsurface(basedstsurf, &op.dstSurface)<0)
  {
    DPRINTK("Destination area is outside of dest surface offset = %ld height = %ld stride = %ld\n",op.dstSurface.ulOffset,op.dstSurface.ulHeight,op.dstSurface.ulStride);
    return -1;
  }

  op.srcSurface.ulOffset = pData->srcOffset;
  op.srcSurface.ulWidth  = srcrect.right;
  op.srcSurface.ulHeight = srcrect.bottom;
  op.srcSurface.ulStride = pData->srcPitch;
  op.srcSurface.format   = pData->srcFormat;

  if(stm_create_subsurface(basesrcsurf, &op.srcSurface)<0)
  {
    DPRINTK("Src area is outside of source surface\n");
    return -1;
  }

  setDrawingState(&op, pData);

  op.ulCLUT = clutaddress;

  ret = stm_display_blitter_blit(blitter, &op, &srcrect, &dstrect);

  return ret;
}


static int stmfbio_blt(struct stmfb_info* i, u_long arg)
{
  STMFBIO_BLT_DATA      bltData;
  stm_blitter_surface_t dst;

  if (!i->pBlitter || !i->ulPFBBase)
    return -ENODEV;

  if (!i->current_planeconfig_valid)
    return -ENODEV;

  if (!copy_from_user(&bltData,(void*)arg,sizeof(bltData)))
  {
    dst.ulMemory = i->ulPFBBase;
    dst.ulSize   = i->ulFBSize;

    if (bltData.dstMemBase >= STMFBGP_GFX_FIRST)
    {
      unsigned int idx = bltData.dstMemBase - STMFBGP_GFX_FIRST;

      if (idx >= ARRAY_SIZE (i->AuxPart)
          || !i->AuxPart[idx])
        return -EINVAL;

      dst.ulMemory = i->AuxBase[idx];
      dst.ulSize   = i->AuxSize[idx];
    }

    switch(bltData.operation)
    {
      case BLT_OP_DRAW_RECTANGLE:
      case BLT_OP_FILL:
      {
        if(stmfbio_draw_rectangle(i->pBlitter, &dst, &bltData) < 0)
          return -EINVAL;

        break;
      }
      case BLT_OP_COPY:
      {
        stm_blitter_surface_t src;
        src.ulMemory = i->ulPFBBase;
        src.ulSize   = i->ulFBSize;

        if (bltData.srcMemBase >= STMFBGP_GFX_FIRST)
        {
          unsigned int idx = bltData.srcMemBase - STMFBGP_GFX_FIRST;

          if (idx >= ARRAY_SIZE (i->AuxPart)
              || !i->AuxPart[idx])
            return -EINVAL;

          src.ulMemory = i->AuxBase[idx];
          src.ulSize   = i->AuxSize[idx];
        }

        if(stmfbio_do_blit(i->pBlitter, &src, &dst, i->dmaBlitterCLUT, &bltData) < 0)
          return -EINVAL;

        break;
      }
      default:
      {
        return -ENOIOCTLCMD;
      }
    }

  }
  else
  {
    DPRINTK("failed to copy from user data.\n");
    return -EFAULT;
  }

  return 0;
}


static int stmfbio_set_blitter_palette(struct stmfb_info* i, u_long arg)
{
  DPRINTK("palette: copying to i->pBlitterCLUT = %p.\n",i->pBlitterCLUT);

  if (!i->pBlitter || !i->pBlitterCLUT)
    return -ENODEV;

  if(down_interruptible(&i->framebufferLock))
    return -ERESTARTSYS;

  if(stm_display_blitter_sync(i->pBlitter)<0)
  {
    DPRINTK("palette: sync blitter failed\n");
    up(&i->framebufferLock);
    return -EINTR;
  }

  if(copy_from_user(i->pBlitterCLUT,(void*)(arg+offsetof(STMFBIO_PALETTE,entries)),sizeof(unsigned long)*256))
  {
    DPRINTK("failed to copy palette from user data.\n");
    up(&i->framebufferLock);
    return -EFAULT;
  }

  DPRINTK("palette: [ 0x%lx, 0x%lx, 0x%lx, 0x%lx ]\n",i->pBlitterCLUT[0],i->pBlitterCLUT[1],i->pBlitterCLUT[2],i->pBlitterCLUT[3]);

  up(&i->framebufferLock);
  return 0;
}


static int stmfbio_pre_auth(struct stmfb_info *i)
{
  int res = 0;
  struct stm_hdmi *hdmi = i->hdmi;
  ULONG ok;

  if(!hdmi)
    return -ENODEV;

  if(mutex_lock_interruptible(&hdmi->lock))
    return -ERESTARTSYS;

  hdmi->auth = 0;

  if (STM_DISPLAY_CONNECTED == hdmi->status) {
    stm_display_output_get_control(hdmi->hdmi_output, STM_CTRL_HDMI_PREAUTH, &ok);
    if (!ok) {
      res = -EAGAIN;
    }
  } else {
    res = -ENODEV;
  }

  mutex_unlock(&hdmi->lock);
  return res;
}

static int stmfbio_post_auth(struct stmfb_info *i, unsigned int auth)
{
  struct stm_hdmi *hdmi = i->hdmi;
  int res = 0;

  if(!hdmi)
    return -ENODEV;

  if(mutex_lock_interruptible(&hdmi->lock))
    return -ERESTARTSYS;

  if (STM_DISPLAY_CONNECTED == hdmi->status) {
    stm_display_output_set_control(hdmi->hdmi_output, STM_CTRL_HDMI_POSTAUTH, auth);
    hdmi->auth = auth;
  } else {
    hdmi->auth = 0;
    res = -ENODEV;
  }

  mutex_unlock(&hdmi->lock);
  return res;
}

static int stmfbio_wait_for_reauth(struct stmfb_info *i, unsigned int timeout_ms)
{
  struct stm_hdmi *hdmi = i->hdmi;
  int jiffies, res;

  if(!hdmi)
    return -ENODEV;

  jiffies = ((timeout_ms * HZ) / 1000) + 1;
  res = wait_event_interruptible_timeout(hdmi->auth_wait_queue, hdmi->auth == 0, jiffies);
  if (unlikely(res < 0))
    return -ERESTARTSYS;

  if (STM_DISPLAY_CONNECTED != hdmi->status)
    return -ENODEV;

  if (hdmi->auth != 0)
    return -ETIMEDOUT;

  return 0;
}


static inline int stmfb_convert_metadata_result_to_errno(stm_meta_data_result_t res)
{
  switch(res)
  {
    case STM_METADATA_RES_OK:
      break;
    case STM_METADATA_RES_UNSUPPORTED_TYPE:
      return -EOPNOTSUPP;
    case STM_METADATA_RES_TIMESTAMP_IN_PAST:
    case STM_METADATA_RES_INVALID_DATA:
      return -EINVAL;
    case STM_METADATA_RES_QUEUE_BUSY:
      return -EBUSY;
    case STM_METADATA_RES_QUEUE_UNAVAILABLE:
      return -EAGAIN;
  }

  return 0;
}


static int stmfb_set_picture_configuration(struct stmfbio_picture_configuration *cfg, struct stmfb_info *i)
{
  stm_meta_data_result_t res;
  stm_meta_data_t *metadata;
  stm_picture_format_info_t *pic_info;
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

  if((metadata = kzalloc(sizeof(stm_meta_data_t)+sizeof(stm_picture_format_info_t),GFP_KERNEL)) == 0)
    return -ENOMEM;

  metadata->size      = sizeof(stm_meta_data_t)+sizeof(stm_picture_format_info_t);
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
   */
  spin_lock_irqsave(&i->framebufferSpinLock,flags);
  stm_meta_data_addref(metadata);
  spin_unlock_irqrestore(&i->framebufferSpinLock,flags);

  metadata->release   = (void(*)(struct stm_meta_data_s*))kfree;
  metadata->presentationTime = ((TIME64)cfg->timestamp.tv_sec * USEC_PER_SEC) +
                                (TIME64)cfg->timestamp.tv_usec;

  pic_info = (stm_picture_format_info_t*)&metadata->data[0];
  pic_info->flags           = cfg->flags;
  pic_info->pictureAspect   = (stm_wss_t)cfg->picture_aspect;
  pic_info->videoAspect     = (stm_wss_t)cfg->video_aspect;
  pic_info->letterboxStyle  = (stm_letterbox_t)cfg->letterbox_style;
  pic_info->pictureRescale  = (stm_picture_rescale_t)cfg->picture_rescale;
  pic_info->barEnable       = cfg->bar_enable;
  pic_info->topEndLine      = cfg->top_bar_end;
  pic_info->bottomStartLine = cfg->bottom_bar_start;
  pic_info->leftEndPixel    = cfg->left_bar_end;
  pic_info->rightStartPixel = cfg->right_bar_start;

  if(stm_display_output_queue_metadata(i->pFBMainOutput, metadata, &res)<0)
  {
    ret = signal_pending(current)?-EINTR:-EIO;
    goto exit;
  }

  /*
   * Set first stab at the wanted return value, this is likely to be revised
   * below.
   */
  ret = stmfb_convert_metadata_result_to_errno(res);

  if(i->hdmi == NULL)
  {
    /*
     * Display pipeline has no HDMI transmitter so just return this result.
     */
    goto exit;
  }

  switch(res)
  {
    case STM_METADATA_RES_OK:
    case STM_METADATA_RES_QUEUE_UNAVAILABLE:
      /*
       * If the DENC isn't in use the queue will not be available,
       * but that is fine we can continue to HDMI.
       */
      break;
    default:
      goto exit;
  }

  if(mutex_lock_interruptible(&i->hdmi->lock))
  {
    ret = -EINTR;
    goto exit;
  }

  /*
   * DVI or undetermined sink does not have InfoFrames.
   */
  if(i->hdmi->edid_info.display_type != STM_DISPLAY_HDMI)
    goto mutex_exit;


  if((i->hdmi->status != STM_DISPLAY_CONNECTED) &&
     (metadata->presentationTime == 0LL))
  {
    /*
     * This is a slight of hand for the usual case that a change
     * is required immediately but the HDMI output is currently
     * stopped and will not process its metadata queues. Flush the queue
     * because we know this request will be the valid data as soon
     * as the HDMI output is started again.
     */
    stm_display_output_flush_metadata(i->hdmi->hdmi_output,STM_METADATA_TYPE_PICTURE_INFO);
  }


  if(stm_display_output_queue_metadata(i->hdmi->hdmi_output, metadata, &res)<0)
  {
    ret = signal_pending(current)?-EINTR:-EIO;
    goto mutex_exit;
  }

  ret = stmfb_convert_metadata_result_to_errno(res);

mutex_exit:
  mutex_unlock(&i->hdmi->lock);
exit:
  spin_lock_irqsave(&i->framebufferSpinLock,flags);
  stm_meta_data_release(metadata);
  spin_unlock_irqrestore(&i->framebufferSpinLock,flags);

  return ret;
}


int stmfb_ioctl(struct fb_info* fb, u_int cmd, u_long arg)
{
  struct stmfb_info* i = (struct stmfb_info* )fb;

  switch (cmd)
  {
    case STMFBIO_SET_PLANES:
      {
        stm_display_output_set_control(i->pFBMainOutput, STM_CTRL_SET_MIXER_PLANES, arg);
        break;
      }
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
      struct stmfb_videomode    vm;
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
          ret = stmfb_set_videomode (info.outputid, &vm, i);

        if (!ret && copy_to_user ((void *) arg, &info, sizeof (info)))
          return -EFAULT;
      }

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

    case STMFBIO_SET_VAR_SCREENINFO_EX:
    {
      struct stmfbio_var_screeninfo_ex tmp;
      int ret;

      DPRINTK("STMFBIO_SET_VAR_SCREENINFO_EX\n");

      if(copy_from_user(&tmp, (void*)arg, sizeof(tmp)))
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
        copy_to_user((void*)arg,&tmp, sizeof(tmp));
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

    case STMFBIO_GET_SHARED_AREA:
    {
      struct stmfbio_shared shared;

      shared.physical = i->ulPSharedAreaBase;
      shared.size     = i->ulSharedAreaSize;

      if (copy_to_user ((void *) arg, &shared, sizeof (shared)))
          return -EFAULT;

      break;
    }

    case STMFBIO_BLT:
    {
      int ret;

      if(down_interruptible(&i->framebufferLock))
        return -ERESTARTSYS;

      ret = stmfbio_blt(i, arg);

      up(&i->framebufferLock);

      if(ret<0)
      {
        if(signal_pending(current))
          return -ERESTARTSYS;
        else
          return ret;
      }

      break;
    }

    case STMFBIO_SET_BLITTER_PALETTE:
      return stmfbio_set_blitter_palette(i, arg);

    case STMFBIO_SYNC_BLITTER:
    {
      int err;

      if (!i->pBlitter)
        return -ENODEV;

      if(down_interruptible(&i->framebufferLock))
      {
        DPRINTK(" taking framebuffer lock interrupted\n");
        return -ERESTARTSYS;
      }

      if((err = stm_display_blitter_sync(i->pBlitter)) != 0)
      {
        up(&i->framebufferLock);
        if(signal_pending(current))
        {
          DPRINTK(" sync interrupted\n");
          return -ERESTARTSYS;
        }
        else
        {
          DPRINTK(" sync error! code = %d\n",err);
          return -EIO;
        }
      }

      up(&i->framebufferLock);

      break;
    }

    case STMFBIO_WAIT_NEXT:
    {
      int err;

      if (!i->pBlitter)
        return -ENODEV;

      if(down_interruptible(&i->framebufferLock))
      {
        DPRINTK(" taking framebuffer lock interrupted\n");
        return -ERESTARTSYS;
      }

      if((err = stm_display_blitter_waitnext(i->pBlitter)) != 0)
      {
        up(&i->framebufferLock);
        if(signal_pending(current))
        {
          DPRINTK(" wait_next interrupted\n");
          return -ERESTARTSYS;
        }
        else
        {
          DPRINTK(" wait_next error! code = %d\n",err);
          return -EIO;
        }
      }

      up(&i->framebufferLock);

      break;
    }

    case STMFBIO_GET_BLTLOAD:
      {
      if (!i->pBlitter)
        return -ENODEV;

      return put_user (stm_display_blitter_get_bltload (i->pBlitter),
                       (unsigned long *) arg);
      }

    case STMFBIO_PRE_AUTH:
      return stmfbio_pre_auth(i);

    case STMFBIO_POST_AUTH:
      return stmfbio_post_auth(i, arg);

    case STMFBIO_WAIT_FOR_REAUTH:
      return stmfbio_wait_for_reauth(i, arg);


    case STMFBIO_SET_DAC_HD_POWER:
    {
      if(down_interruptible(&i->framebufferLock))
        return -ERESTARTSYS;
      stm_display_output_set_control(i->pFBMainOutput, STM_CTRL_DAC_HD_POWER, arg);
      up(&i->framebufferLock);
      return 0;
    }

    case STMFBIO_SET_DAC_HD_FILTER:
    {
      if(down_interruptible(&i->framebufferLock))
        return -ERESTARTSYS;
      stm_display_output_set_control(i->pFBMainOutput, STM_CTRL_DAC_HD_FILTER, arg);
      up(&i->framebufferLock);
      return 0;
    }

    case STMFBIO_SET_CGMS_ANALOG:
    {
      if(down_interruptible(&i->framebufferLock))
        return -ERESTARTSYS;
      stm_display_output_set_control(i->pFBMainOutput, STM_CTRL_CGMS_ANALOG, arg);
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

    default:
    {
      DPRINTK(" Invalid IOCTL code 0x%08x\n",cmd);
      return -ENOTTY;
    }
  }

  return 0;
}


EXPORT_SYMBOL(stmfbio_draw_rectangle);
EXPORT_SYMBOL(stmfbio_do_blit);
