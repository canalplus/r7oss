/***********************************************************************
 *
 * File: linux/kernel/drivers/video/stmfbops.c
 * Copyright (c) 2007-2010 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/fb.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>

#include <asm/uaccess.h>
#include <linux/semaphore.h>

#include <stm_display.h>
#include <linux/stm/stmcoredisplay.h>

#include "stmfb.h"
#include "stmfbinfo.h"
#include "linux/kernel/drivers/stm/hdmi/stmhdmi.h"

/* -------------------- port of Vibe on kernel 3.10 ------------------------ */
/*
 * The VM flag VM_RESERVED has been removed and VM_DONTDUMP should be
 * used instead. Unfortunately VM_DONTDUMP is not avilable in 3.4,
 * hence the code below :-(
 */
#include <linux/version.h>
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0))
#define   STMFB_VM_FLAGS (VM_IO | VM_DONTEXPAND | VM_RESERVED)
#else
#define   STMFB_VM_FLAGS (VM_IO | VM_DONTEXPAND | VM_DONTDUMP)
#endif

/* ------------------- chipset specific functions -------------------------- */
/*
 * stmfb_check_var
 * determine if the video mode is valid, possibly altering the
 * incoming var to the closest match.
 */
static int
stmfb_check_var (struct fb_var_screeninfo * const var,
                 struct fb_info           * const info)
{
  struct stmfb_info        * const i = container_of (info, struct stmfb_info,
                                                     info);
  stm_display_mode_t        vm;
  struct stmfbio_planeinfo  plane;
  int                       ret;
  __u32                     saved_activate, saved_virtual;

  DPRINTK("in current process/pid: %p/%d, %u(+%u)x%u@%u\n",
          current,current->pid,
          var->xres,var->right_margin,var->yres,var->pixclock);

  if(i->platformDevice && i->platformDevice->dev.power.power_state.event != PM_EVENT_ON)
  {
    DPRINTK("Checking display mode while in suspend state???\n");
    return -EBUSY;
  }

  if (down_interruptible (&i->framebufferLock))
    return -ERESTARTSYS;

  /* Get hardware description of videomode to set */
  ret = stmfb_decode_var (var, &vm, &plane, i);

  up (&i->framebufferLock);

  if (ret < 0)
  {
    DPRINTK("failed\n");
    return ret;
  }

  /*
   * Re-create the VAR from the exact hardware description, this gives a
   * completely clean var, which is why we have to save and restore the
   * activate flags. Without this we get spurious mode changes when they
   * should have been just tests.
   * Also, now the driver internally has no notion anymore of a virtual
   * resolution, which means we have to restore the field as well as otherwise
   * applications might be confused if they read back a different value than
   * they set. Just restoring the virtual is ok, because if it was invalid,
   * not, the stmfb_decode_var() above would have failed.
   */
  saved_activate = var->activate;
  saved_virtual = var->yres_virtual;
  stmfb_encode_var(var, &vm, &plane, i);
  var->activate = saved_activate;
  var->yres_virtual = saved_virtual;

  DPRINTK("finished OK\n");

  return 0;
}


/*
 * Callback to indicate when a new framebuffer has made it onto the display
 * Currently only used to wait for the display to "pan".
 */
static void
stmfb_frame_displayed (void   * const pData,
                       stm_time64_t  vsyncTime,
                       uint16_t output_change,
                       uint16_t nb_planes,
                       stm_display_latency_params_t *display_latency_params)
{
  struct stmfb_info * const i = (struct stmfb_info *) pData;

#if defined(DEBUG_UPDATES)
  DPRINTK("in i = %p outstanding updates = %d\n",i,i->num_outstanding_updates);
#endif

  i->num_outstanding_updates--;
  wake_up(&i->framebuffer_updated_wait_queue);
}


/*
 * Helper to pan the framebuffer, this is called with the driver lock
 * already taken from both the framebuffer pan display and set mode
 * entrypoints.
 */
static int
stmfb_do_pan_display (const struct stmfbio_plane_pan * const pan,
                      struct stmfb_info              * const i)
{
#if defined(DEBUG_UPDATES)
  DPRINTK ("setting layer %u's base address to %x\n",
           pan->layerid, pan->baseaddr);
#endif

  BUG_ON (i->current_planeconfig_valid != 1);
  BUG_ON (pan->layerid != 0);

  if (pan->baseaddr == i->current_planeconfig.baseaddr)
  {
    DPRINTK ("Already panned to required position (%lx), doing nothing\n",
             pan->baseaddr);
    return 0;
  }

  /*
   * Only actually do the pan if we are powered up, otherwise just return
   * success. When the device is resumed the correct pan will be set.
   */
  if (i->platformDevice && i->platformDevice->dev.power.power_state.event != PM_EVENT_ON)
  {
    DPRINTK ("Device suspended, doing nothing\n");
    return 0;
  }

  /*
   * NOTE: It is fun to think about the race conditions here against the
   * display vsync, the GDP hardware reading the node update from memory and
   * the vsync handler. In the current systems we think the worst that can
   * happen is that you might wait for a vsync that isn't strictly necessary.
   * However on an SMP system, where interrupts are being handled on a
   * different CPU, you could get a delayed vsync interrupt handler waking up
   * the wait queue for a vsync interrupt that happened before the change in
   * buffer address completed. The result of this could be a display tearing
   * for one frame as the application draws into a buffer it thinks is no longer
   * on the display but in fact still is. On the next update things would get
   * corrected assuming the race condition did not happen again. It isn't clear
   * how you could prevent this given the hardware's behaviour, without
   * reverting to deferring updates to the vsync handler again.
   */
  if(stm_display_plane_set_control(i->hFBPlane,PLANE_CTRL_BUFFER_ADDRESS,pan->baseaddr)<0)
  {
    DPRINTK("Unable to set the buffer address\n");
    return signal_pending(current)?-ERESTARTSYS:-EINVAL;
  }

  i->current_planeconfig.baseaddr = pan->baseaddr;

  return 0;
}


int
stmfb_set_plane_pan (const struct stmfbio_plane_pan * const pan,
                     struct stmfb_info              * const i)
{
  int ret;

  /* stmfb_do_pan_display() only works correctly, if the plane been queued
     previously. */
  ret = (i->current_planeconfig_valid == 1) ? 0 : -ESPIPE;

  if (!ret)
    ret = stmfb_verify_baseaddress (i, &i->current_planeconfig,
                                    (i->current_planeconfig.source.h
                                     * i->current_planeconfig.pitch),
                                    pan->baseaddr);
  if (!ret)
    ret = stmfb_do_pan_display (pan, i);
  return ret;
}

/*
 * stmfb_pan_display
 * Pan (or wrap, depending on the `vmode' field) the display using the
 * `xoffset' and `yoffset' fields of the `var' structure.
 * If the values don't fit, return -EINVAL.
 */
static int
stmfb_pan_display (struct fb_var_screeninfo * const var,
                   struct fb_info           * const info)
{
  struct stmfb_info        *i = container_of (info, struct stmfb_info, info);
  struct stmfbio_planeinfo  plane;
  struct stmfbio_plane_pan  pan;
  int                       ret;

  if(i->platformDevice && i->platformDevice->dev.power.power_state.event != PM_EVENT_ON)
  {
    DPRINTK("Pan display while in suspend state???\n");
    return -EBUSY;
  }

  if (down_interruptible (&i->framebufferLock))
    return -ERESTARTSYS;

  if (i->fbdev_api_suspended)
  {
    ret = -EBUSY;
    goto out;
  }

  ret = stmfb_decode_var (var, NULL, &plane, i);
  if (ret)
    goto out;

  if (i->current_planeconfig_valid != 1)
  {
    DPRINTK ("current plane config not valid\n");
    ret = -ENODEV;
    goto out;
  }

#if defined(DEBUG_UPDATES)
  DPRINTK ("panning layer 0 (i: %p) to xoffset: %u yoffset: %u (0x%08lx)\n",
           i, var->xoffset, var->yoffset, plane.config.baseaddr);
#endif

  pan.layerid = plane.layerid;
  pan.activate = plane.activate;
  pan.baseaddr = plane.config.baseaddr;
  ret = stmfb_do_pan_display (&pan, i);

out:
  up (&i->framebufferLock);

  return ret;
}


/*
 * stmfb_coldstart_display
 * Start the display running, where we know it is currently stopped.
 */
static int
stmfb_coldstart_display (struct stmfb_info            * const i,
                         const stm_display_mode_t     * const vm)
{
  int ret = 0;

  DPRINTK ("\n");

  if(i->main_config.activate == STMFBIO_ACTIVATE_ON_NEXT_CHANGE)
  {
    i->main_config.activate = STMFBIO_ACTIVATE_IMMEDIATE;
    ret = stmfb_set_output_configuration(&i->main_config, i);
    if(ret != 0)
    {
      DPRINTK("Setting main output configuration failed\n");
    }
  }

  ret += stm_display_output_start(i->hFBMainOutput, vm);

  if(ret == 0)
  {
    DPRINTK("Start Main video output successful\n");
  }

  if(i->hFBDVO && !(i->main_config.dvo_config & STMFBIO_OUTPUT_DVO_DISABLED))
  {
    ret += stm_display_output_start(i->hFBDVO, vm);

    if(ret == 0)
    {
      DPRINTK("Start DVO successful\n");
    }
  }

  if(ret < 0)
  {
    DPRINTK("Failed\n");
    if(signal_pending(current))
      return -ERESTARTSYS;
    else
      return -EIO;
  }

  DPRINTK("Initialised display output\n");
  return 0;
}


/*
 * stmfb_stop_display
 * Completely stop the display output
 */
static int
stmfb_stop_display (struct stmfb_info * const i)
{
  unsigned long saveFlags;
  int           res = 0;

  DPRINTK("Flushing FB plane\n");

  wait_event(i->framebuffer_updated_wait_queue, (i->num_outstanding_updates == 0));

  BUG_ON((i->hQueueInterface == NULL));

  res = stm_display_source_queue_flush(i->hQueueInterface,1);
  /*
   * Note: even if the flush call failed because a signal is pending, we still
   *       have to be able to release the queue handle.
   */
  if(res<0)
  {
    if((res == -EINTR) && signal_pending(current))
      return -ERESTARTSYS;

    return res;
  }

  if (i->current_planeconfig_valid)
    /* the config is valid, but will need to be requeued */
    i->current_planeconfig_valid = 2;

  if(!signal_pending(current) && i->hdmi_fops && i->hdmi_fops->unlocked_ioctl)
  {
    mm_segment_t oldfs = get_fs();

    /*
     * It is important to ignore errors other than when a signal was delivered,
     * as if the HDMI device isn't in a state to restart we do not actually
     * care in this code. We need to ensure that HDCP is disabled via this control
     * before stopping output. Otherwise this call will be failing.
     */
    set_fs(KERNEL_DS);
    if(i->hdmi_fops->unlocked_ioctl(&i->hdmi_dev, STMHDMIIO_SET_DISABLED, 1) == -ERESTARTSYS)
      res = -ERESTARTSYS;
    set_fs(oldfs);

    /*
     * We can't stop the HDMI output as soons as HDCP is not yet disabled however
     * it is important to put the HDMI device in a state to restart so it can be
     * restarted later.
     */
    if((res == 0) && (i->hFBHDMI != NULL))
      stm_display_output_set_connection_status(i->hFBHDMI, STM_DISPLAY_NEEDS_RESTART);
  }

  if(stm_display_output_stop(i->hFBMainOutput)<0)
  {
    if(!signal_pending(current) && i->current_planeconfig_valid)
    {
      /*
       * We couldn't stop the main output because some other plane was
       * active on the display (e.g. an open, streaming, V4L2 output).
       * However we flushed the framebuffer plane in preparation to stop
       * the output. In order that the system isn't left in an inconsistent
       * state, reinstate the previous framebuffer onto the display.
       */
      stmfb_queuebuffer(i);
      i->current_planeconfig_valid = 1;
    }

    DPRINTK("Failed, could not stop output\n");
    /*
     * Test signal pending state again, in case a signal came in and
     * disturbed the queue buffer call. Remember this is still an
     * error path which needs to return -EBUSY unless a signal is pending.
     */
    if(signal_pending(current))
      return -ERESTARTSYS;
    else
      return -EBUSY;
  }

  /*
   * Invalidate the current video mode and reset the HDMI hotplug
   * status (note this is done under interrupt lock, although there
   * shouldn't be any VTG interrupts at this point because we have stopped
   * the outputs). This forces HDMI to re-start on a mode change as the
   * Vsync handling will think there has been a new hotplug event if the
   * TV is actually connected.
   */
  spin_lock_irqsave(&(i->framebufferSpinLock), saveFlags);

  i->current_videomode_valid = 0;
  i->info.mode = NULL;

  spin_unlock_irqrestore(&(i->framebufferSpinLock), saveFlags);

  DPRINTK("Finish OK\n");

  return 0;
}

/*
 * stmfb_restart_display
 * helper for stmfb_set_par
 */
static int
stmfb_restart_display (struct stmfb_info            * const i,
                       const stm_display_mode_t     * const vm)
{
  int ret;
  DPRINTK("Doing display restart\n");

  ret = stmfb_stop_display(i);
  if(ret<0)
    return ret;

  return stmfb_coldstart_display(i,vm);
}


static int stmfb_set_io_windows (struct stmfb_info                 * const i,
                                 const struct stmfbio_plane_config * const c)
{
  stm_rect_t    input_window, output_window;

  /*
   * Set the Input and Output windows sizes and put them in "Manual" mode
   */

  input_window.x      = 0;
  input_window.y      = 0;
  input_window.width  = c->source.w;
  input_window.height = c->source.h;

  if(stm_display_plane_set_compound_control(i->hFBPlane, PLANE_CTRL_INPUT_WINDOW_VALUE, &input_window)<0)
  {
    DPRINTK("Unable to set the Input window rect\n");
    return signal_pending(current)?-ERESTARTSYS:-EINVAL;
  }

  output_window.x      = c->dest.x;
  output_window.y      = c->dest.y;
  output_window.width  = c->dest.dim.w;
  output_window.height = c->dest.dim.h;

  if(stm_display_plane_set_compound_control(i->hFBPlane, PLANE_CTRL_OUTPUT_WINDOW_VALUE, &output_window)<0)
  {
    DPRINTK("Unable to set the Output window rect\n");
    return signal_pending(current)?-ERESTARTSYS:-EINVAL;
  }

  if(stm_display_plane_set_control(i->hFBPlane, PLANE_CTRL_INPUT_WINDOW_MODE, MANUAL_MODE)<0)
  {
    DPRINTK("Unable to set the Input window mode\n");
    return signal_pending(current)?-ERESTARTSYS:-EINVAL;
  }

  if(stm_display_plane_set_control(i->hFBPlane, PLANE_CTRL_OUTPUT_WINDOW_MODE, MANUAL_MODE)<0)
  {
    DPRINTK("Unable to set the Output window mode\n");
    return signal_pending(current)?-ERESTARTSYS:-EINVAL;
  }

  return 0;
}


static int
stmfb_queue_new_fb_configuration (struct stmfb_info                 * const i,
                                  const struct stmfbio_plane_config * const c)
{
  int ret;
  DPRINTK("Setting new framebuffer configuration\n");

  /*
   * Queue a new buffer configuration on the framebuffer display plane.
   */
  memset(&i->current_buffer_setup, 0, sizeof(stm_display_buffer_t));

  /*
   * Try changing the plane input and output windows first, if it fails then
   * we are left with a nice blank configuration.
   */
  ret = stmfb_set_io_windows(i, c);
  if(ret<0)
    return ret;


  if(stm_display_plane_set_control(i->hFBPlane,PLANE_CTRL_BUFFER_ADDRESS,c->baseaddr)<0)
  {
    DPRINTK("Unable to set the buffer address\n");
    return signal_pending(current)?-ERESTARTSYS:-EINVAL;
  }

  i->current_buffer_setup.src.primary_picture.video_buffer_addr = 0;
  i->current_buffer_setup.src.primary_picture.video_buffer_size = c->pitch * c->source.h;
  i->current_buffer_setup.src.clut_bus_address  = i->dmaFBCLUT;

  i->current_buffer_setup.src.primary_picture.width     = c->source.w;
  i->current_buffer_setup.src.primary_picture.height    = c->source.h;
  i->current_buffer_setup.src.primary_picture.pitch     = c->pitch;
  i->current_buffer_setup.src.primary_picture.pixel_depth = c->bitdepth;
  i->current_buffer_setup.src.primary_picture.color_fmt   = c->format;

  i->current_buffer_setup.src.visible_area.x = 0;
  i->current_buffer_setup.src.visible_area.y = 0;
  i->current_buffer_setup.src.visible_area.width = c->source.w;
  i->current_buffer_setup.src.visible_area.height = c->source.h;

  /*
   * The persistent flag keeps this buffer on the display until
   * another buffer is queued or the plane is released.
   *
   * The presentation as graphics changes the behaviour when the display is
   * interlaced; it will only update the display on top fields and therefore
   * will be visible on a matched top/bottom field pair.
   */
  i->current_buffer_setup.info.ulFlags          = (STM_BUFFER_PRESENTATION_PERSISTENT |
                                                   STM_BUFFER_PRESENTATION_GRAPHICS |
                                                   STM_BUFFER_PRESENTATION_DIRECT_BUFFER_ADDR);
  i->current_buffer_setup.info.display_callback = stmfb_frame_displayed;
  i->current_buffer_setup.info.puser_data        = i;

  i->current_planeconfig = *c;
  i->current_planeconfig_valid = 1;

  i->current_var_ex.activate = STMFBIO_ACTIVATE_IMMEDIATE;
  /*
   * Override the flicker filter state for 8bit modes, we need to force the
   * filter off.
   */
  if(i->current_buffer_setup.src.primary_picture.pixel_depth == 8
     || c->format == SURF_ACLUT88)
    i->current_var_ex.ff_state = STMFBIO_FF_OFF;

  if((ret = stmfb_set_var_ex(&i->current_var_ex, i))<0)
    return ret;

  /* Wait for the flip to happen */
//  wait_event(i->framebuffer_updated_wait_queue, (i->num_outstanding_updates == 0));

  i->info.fix.line_length = c->pitch;
  i->info.fix.visual      = (((c->bitdepth == 8)
                              || c->format == SURF_ACLUT88
                             ) ? FB_VISUAL_PSEUDOCOLOR : FB_VISUAL_TRUECOLOR);

  return 0;
}


static int
stmfb_set_videomode_locked (u32                           output,
                            const stm_display_mode_t     * const vm,
                            struct stmfb_info            * const i)
{
  int ret = 0;

  if(output != STMFBIO_OUTPUTID_MAIN)
    return -EINVAL;

  if(vm->mode_params.output_standards & STM_OUTPUT_STD_SD_MASK)
    i->main_config.sdtv_encoding = vm->mode_params.output_standards & STM_OUTPUT_STD_SD_MASK;

  if(!i->current_videomode_valid)
  {
    ret = stmfb_coldstart_display(i,vm);
    if(ret<0)
      return ret;
    if (i->current_planeconfig_valid)
      i->current_planeconfig_valid = 2;
  }
  else if((i->current_videomode.mode_id != vm->mode_id) ||
          (i->current_videomode.mode_params.flags != vm->mode_params.flags))
  {
    /*
     * Try and change the display mode on the fly, to allow us to do a low
     * impact change between 60/59.94Hz variants of HD modes.
     */
    ret = stm_display_output_start (i->hFBMainOutput, vm);
    if (ret == 0)
    {
      if(i->main_config.activate == STMFBIO_ACTIVATE_ON_NEXT_CHANGE)
      {
        i->main_config.activate = STMFBIO_ACTIVATE_IMMEDIATE;
        ret = stmfb_set_output_configuration(&i->main_config, i);
        if(ret < 0)
        {
          /*
           * If a signal happened then reset the activate flag so it
           * will happen next time. But other than that keep going.
           */
          if((ret = -EINTR) && signal_pending(current))
          {
            ret = -ERESTARTSYS;
            i->main_config.activate = STMFBIO_ACTIVATE_ON_NEXT_CHANGE;
          }
          else
          {
            DPRINTK("On the fly display change failed due to invalid pending configuration changes\n");
            stmfb_stop_display(i);
            return ret;
          }
        }
      }

      DPRINTK("On the fly display change successful\n");
    }
    else
    {
      if(signal_pending(current))
        return -ERESTARTSYS;

      if((ret = stmfb_restart_display(i,vm))<0)
        return ret;
    }

  }

  i->current_videomode = *vm;
  i->current_videomode_valid = 1;

  i->info.mode = NULL;

  return ret;
}


int
stmfb_set_videomode (enum stmfbio_output_id        output,
                     const stm_display_mode_t     * const vm,
                     struct stmfb_info            * const i)
{
  int ret = 0;

  DPRINTK("in current process = %p pid = %d i = %p\n",current,(current!=NULL)?current->pid:0,i);

  if(output != STMFBIO_OUTPUTID_MAIN)
    return -EINVAL;

  if(!i->platformDevice)
  {
    DPRINTK("Missing platform device pointer???\n");
    return -ENODEV;
  }

  if(i->platformDevice && i->platformDevice->dev.power.power_state.event != PM_EVENT_ON)
  {
    DPRINTK("Changing display mode while in suspend state???\n");
    return -EBUSY;
  }

  if(down_interruptible(&i->framebufferLock))
    return -ERESTARTSYS;

  ret = stmfb_set_videomode_locked(output, vm, i);
  if (!ret && i->current_planeconfig_valid == 2)
    /* we might have to requeue the plane configuration -> if the resolution
       changed */
    ret = stmfb_queue_new_fb_configuration (i, &i->current_planeconfig);

  up(&i->framebufferLock);

  DPRINTK("out\n");
  return ret;
}

int
stmfb_set_panelmode (enum stmfbio_output_id        output,
                     const stm_display_mode_t     * const vm,
                     struct stmfb_info            * const i)
{
  int ret = 0;

  DPRINTK("in current process = %p pid = %d i = %p\n",current,(current!=NULL)?current->pid:0,i);

  if(output != STMFBIO_OUTPUTID_MAIN)
    return -EINVAL;

  if(!i->platformDevice)
  {
    DPRINTK("Missing platform device pointer???\n");
    return -ENODEV;
  }

  if(i->platformDevice && i->platformDevice->dev.power.power_state.event != PM_EVENT_ON)
  {
    DPRINTK("Changing display mode while in suspend state???\n");
    return -EBUSY;
  }

  if(down_interruptible(&i->framebufferLock))
    return -ERESTARTSYS;

  if((ret = stmfb_restart_display(i,vm))<0)
    return ret;

    i->current_videomode = *vm;
    i->current_videomode_valid = 1;
    i->info.mode = NULL;

  up(&i->framebufferLock);

  DPRINTK("out\n");
  return ret;
}


int
stmfb_set_3d_configuration(struct stmfb_info                     * const i,
                           const struct stmfbio_3d_configuration * const c)
{
  /*
   * This function is in the "ops" file because it needs to be able to call
   * stmfb_set_videomode_locked() when doing an immediate activation of the
   * 3D flags.
   */
  int ret = 0;

  if(c->mode > STMFBIO_LINE_ALTERNATIVE)
    return -EINVAL;

  if(c->framebuffer_type > STMFBIO_3D_FB_STEREO)
    return -EINVAL;

  if((c->activate & STMFBIO_ACTIVATE_MASK) > STMFBIO_ACTIVATE_TEST)
    return -EINVAL;

  switch(c->mode)
  {
    case STMFBIO_3D_NONE:
    case STMFBIO_3D_SBS_HALF:
    case STMFBIO_3D_TOP_BOTTOM:
      if(c->framebuffer_depth != 0)
      {
        DPRINTK("Cannot change framebuffer 3D depth in 2D/packed 3D modes\n");
        return -EINVAL;
      }
      if(c->framebuffer_type != STMFBIO_3D_FB_MONO)
      {
        DPRINTK("Cannot use a stereo framebuffer in 2D/packed 3D modes\n");
        return -EINVAL;
      }
      /*
       * We can always support these configurations
       */
      break;

    case STMFBIO_3D_FRAME_PACKED:
    case STMFBIO_3D_FIELD_ALTERNATIVE:
      /*
       * TODO: determine if the system supports HDMI1.4 double clocked 3D modes
       */
      DPRINTK("Framepacked/field alternative modes not yet implemented\n");
      return -EINVAL;

    case STMFBIO_3D_FRAME_SEQUENTIAL:
    case STMFBIO_3D_LL_RR:
    case STMFBIO_LINE_ALTERNATIVE:
      /*
       * TODO: determine if the main output supports TV panel 3D modes
       */
      DPRINTK("TV panel 3D modes not yet implemented\n");
      return -EINVAL;
  }

  if((c->activate & STMFBIO_ACTIVATE_MASK) == STMFBIO_ACTIVATE_TEST)
    return 0; /* Nothing more to do */

  if (down_interruptible (&i->framebufferLock))
    return -ERESTARTSYS;

  if((c->activate & STMFBIO_ACTIVATE_MASK) != STMFBIO_ACTIVATE_TEST)
    i->current_3d_config = *c;

  if((c->activate & STMFBIO_ACTIVATE_MASK) == STMFBIO_ACTIVATE_IMMEDIATE)
  {
    stm_display_mode_t vm;

    BUG_ON(!i->current_videomode_valid);
    vm = i->current_videomode;

    stmfb_set_videomode_3d_flags(i, &vm);

    ret = stmfb_set_videomode_locked(c->outputid, &vm, i);
    if(ret<0)
    {
      DPRINTK("Unable to change 3D configuration for current output mode\n");
    }
  }

  up (&i->framebufferLock);

  return ret;
}


static int
stmfb_set_planeinfo_locked (const struct stmfbio_planeinfo * const plane,
                            struct stmfb_info              * const i)
{
  const struct stmfbio_plane_config *c = &plane->config;
  int                                ret;

  if (plane->layerid != 0)
    return -EINVAL;

  if (i->current_planeconfig_valid == 1
      && i->current_planeconfig.format == c->format
      && i->current_planeconfig.pitch == c->pitch
      && i->current_planeconfig.source.w == c->source.w
      && i->current_planeconfig.source.h == c->source.h
      && !memcmp (&i->current_planeconfig.dest, &c->dest, sizeof (c->dest)))
  {
    struct stmfbio_plane_pan pan;

    BUG_ON (i->current_planeconfig.bitdepth != c->bitdepth);

    /*
     * All we have been asked to change in effect is the panning, this often
     * happens with VT switches, so just do a pan. This might make switching
     * between DirectFB applications very marginally faster.
     */
    DPRINTK("Display mode already set, just panning display to %lx\n",
            c->baseaddr);

    pan.layerid = plane->layerid;
    pan.activate = plane->activate;
    pan.baseaddr = plane->config.baseaddr;
    ret = stmfb_do_pan_display (&pan, i);

    return ret;
  }

  ret = stmfb_queue_new_fb_configuration (i,c);

  i->info.mode = NULL;

  return ret;
}

int
stmfb_set_planemode (const struct stmfbio_planeinfo * const plane,
                     struct stmfb_info              * const i)
{
  int ret;

  DPRINTK("in current process = %p pid = %d i = %p\n",current,(current!=NULL)?current->pid:0,i);

  if(!i)
    return -ENODEV;

  if(!plane || plane->layerid != 0)
    return -EINVAL;

  if(!i->platformDevice)
  {
    DPRINTK("Missing platform device pointer???\n");
    return -ENODEV;
  }

  if(i->platformDevice && i->platformDevice->dev.power.power_state.event != PM_EVENT_ON)
  {
    DPRINTK("Changing display mode while in suspend state???\n");
    return -EBUSY;
  }

  if(down_interruptible(&i->framebufferLock))
    return -ERESTARTSYS;

  /* prevent the framebuffer kernel API from messing around w/ the
     config in the future, until all apps have exited */
  i->fbdev_api_suspended = 1;

  ret = stmfb_set_planeinfo_locked(plane, i);

  up(&i->framebufferLock);

  DPRINTK("out\n");
  return ret;
}

/*
 * stmfb_set_par
 * Set the hardware to display the videomode the mode passed in
 */
static int
stmfb_set_par (struct fb_info * const info)
{
  struct stmfb_info         * const i = container_of (info, struct stmfb_info,
                                                      info);
  stm_display_mode_t         vm;
  struct stmfbio_planeinfo   plane;
  int                        ret;

  DPRINTK("in current process = %p pid = %d i = %p\n",current,(current!=NULL)?current->pid:0,i);

  if(!i->platformDevice)
  {
    DPRINTK("Missing platform device pointer???\n");
    return -ENODEV;
  }

  if(i->platformDevice && i->platformDevice->dev.power.power_state.event != PM_EVENT_ON)
  {
    DPRINTK("Setting hardware while in suspend state???\n");
    return -EBUSY;
  }

  if(down_interruptible(&i->framebufferLock))
    return -ERESTARTSYS;

  if (i->fbdev_api_suspended)
  {
    up(&i->framebufferLock);
    return -EBUSY;
  }

  /* Get hardware description of videomode and planeconfig to set */
  if(stmfb_decode_var(&i->info.var, &vm, &plane, i) < 0)
  {
    up(&i->framebufferLock);
    printk(KERN_ERR "%s: unable to decode videomode, corrupted state?\n", __FUNCTION__);
    return -EIO;
  }

  if((i->current_3d_config.activate & STMFBIO_ACTIVATE_MASK) == STMFBIO_ACTIVATE_ON_NEXT_CHANGE)
    i->current_3d_config.activate = STMFBIO_ACTIVATE_IMMEDIATE;

  if((i->current_3d_config.activate & STMFBIO_ACTIVATE_MASK) == STMFBIO_ACTIVATE_IMMEDIATE)
    stmfb_set_videomode_3d_flags(i,&vm);

  ret = stmfb_set_videomode_locked (STMFBIO_OUTPUTID_MAIN, &vm, i);
  if(!ret)
    ret = stmfb_set_planeinfo_locked (&plane, i);
  if(!ret)
    i->info.mode = (struct fb_videomode *) fb_match_mode (&i->info.var,
                                                          &i->info.modelist);

  up(&i->framebufferLock);

  DPRINTK("out\n");
  return ret;
}


static void
stmfb_set_pseudo_palette (u_int           regno,
                          u_int           red,
                          u_int           green,
                          u_int           blue,
                          u_int           transp,
                          struct fb_info * const info)
{
  struct stmfb_info * const i = container_of (info, struct stmfb_info, info);
  stm_pixel_format_t  format = SURF_NULL_PAD;

  if (i->current_planeconfig_valid)
    format = i->current_planeconfig.format;

  if (unlikely (format == SURF_NULL_PAD))
  {
    struct stmfbio_planeinfo plane;
    /*
     * We have to decode the var to get the full display format, as we may be
     * called before any videomode has been set.
     */
    /* Get hardware description of videomode and planeconfig to set */
    if (down_interruptible (&i->framebufferLock))
      return;

    if (stmfb_decode_var (&i->info.var, NULL, &plane, i) < 0)
    {
      up (&i->framebufferLock);

      printk(KERN_ERR "%s: unable to decode videomode, corrupted state?\n",__FUNCTION__);
      return;
    }

    up (&i->framebufferLock);

    format = plane.config.format;
  }

  /*
   * We need to maintain the pseudo palette as this is what is used
   * by the cfb routines to draw on the framebuffer for the VT code.
   */
  switch (format)
  {
    case SURF_RGB565:
    {
      i->pseudo_palette[regno] = (red   & 0xf800       ) |
                                ((green & 0xfc00) >> 5 ) |
                                ((blue  & 0xf800) >> 11);
      break;
    }
    case SURF_ARGB1555:
    {
      int alpha = (transp>0)?1:0;
      i->pseudo_palette[regno] = ((red   & 0xf800) >> 1 ) |
                                 ((green & 0xf800) >> 6 ) |
                                 ((blue  & 0xf800) >> 11) |
                                  (alpha           << 15);
      break;
    }
    case SURF_ARGB4444:
    {
      i->pseudo_palette[regno] = ((red    & 0xe000) >> 4 ) |
                                 ((green  & 0xe000) >> 8 ) |
                                 ((blue   & 0xe000) >> 12) |
                                 ((transp & 0xe000)      );
      break;
    }
    case SURF_RGB888:
    {
      i->pseudo_palette[regno] = ((red    & 0xff00) << 8) |
                                 ((green  & 0xff00)     ) |
                                 ((blue   & 0xff00) >> 8);
      break;
    }
    case SURF_ARGB8565:
    {
      i->pseudo_palette[regno] = (red   & 0xf800       ) |
                                ((green & 0xfc00) >> 5 ) |
                                ((blue  & 0xf800) >> 11) |
                                ((transp& 0xff00) << 16);
      break;
    }
    case SURF_ARGB8888:
    {
      i->pseudo_palette[regno] = ((red    & 0xff00) << 8 ) |
                                 ((green  & 0xff00)      ) |
                                 ((blue   & 0xff00) >> 8 ) |
                                 ((transp & 0xff00) << 16);
      break;
    }
    case SURF_BGRA8888:
    {
      i->pseudo_palette[regno] = ((red    & 0xff00)      ) |
                                 ((green  & 0xff00) <<  8) |
                                 ((blue   & 0xff00) << 16) |
                                 ((transp & 0xff00) >>  8);
      break;
    }
    default:
      break;
  }
}


/*
 * stmfb_setcolreg
 * Set a single color register. The values supplied have a 16 bit magnitude
 * Return != 0 for invalid regno.
 */
static int
stmfb_setcolreg (u_int           regno,
                 u_int           red,
                 u_int           green,
                 u_int           blue,
                 u_int           transp,
                 struct fb_info * const info)
{
  struct stmfb_info * const i = container_of (info, struct stmfb_info, info);
  unsigned long      alpha;

  if(i->platformDevice && i->platformDevice && i->platformDevice->dev.power.power_state.event != PM_EVENT_ON)
  {
    DPRINTK("Setting Color Regs while in suspend state???\n");
    return -EBUSY;
  }

  if (regno > 255)
    return -EINVAL;

  /*
   * First set the console palette (first 16 entries only) for RGB framebuffers
   */
  if (regno < 16)
    stmfb_set_pseudo_palette(regno, red, green, blue, transp, info);

  /*
   * Now set the real CLUT for 8bit surfaces.
   *
   * Note that transparency is converted to a range of 0-128 for the hardware
   * CLUT. This is different from the pseudo palette where alpha image formats
   * use the full 0-255 8bit range where available.
   */
  alpha = ((transp>>8)+1)/2;
  i->pFBCLUT[regno] = ( alpha     <<24) |
                      ((red>>8)   <<16) |
                      ((green>>8) <<8 ) |
                       (blue>>8);

#if DEBUG_CLUT
  DPRINTK("pFCLUT[%u] = 0x%08lx\n",regno,i->pFBCLUT[regno]);
#endif

  return 0;
}

void
stmfb_sync_fb_with_output(struct stmfb_info * const i)
{
  stm_display_mode_t current_mode;

  if (stm_display_output_get_current_display_mode(i->hFBMainOutput,
              &current_mode)<0)
  {
    DPRINTK("Unable to get current display mode\n");
    return;
  }
  if(i->current_videomode.mode_id != current_mode.mode_id)
  {
    DPRINTK("mode Changed FB:%d HW:%d\n",
            i->current_videomode.mode_id, current_mode.mode_id);

    stmfb_encode_mode(&current_mode, i);

    /* we have to requeue the plane configuration as resolution changed */
    stmfb_queue_new_fb_configuration (i, &i->current_planeconfig);
  }
}

static int
stmfb_iomm_addressable (const struct stmfb_info * const i,
                        unsigned long            physaddr)
{
  int n;
  const struct stmcore_display_pipeline_data *pd;

  pd = *((struct stmcore_display_pipeline_data **)i->platformDevice->dev.platform_data);

  for (n=0; n<pd->whitelist_size; n++)
    if (physaddr == pd->whitelist[n])
      return 1;

  return 0;
}

static int
stmfb_iomm_vma_fault (struct vm_area_struct *vma, struct vm_fault *vmf)
{
  /* we want to provoke a bus error rather than give the client the zero page */
  return VM_FAULT_SIGBUS;
}

static struct vm_operations_struct stmfb_iomm_nopage_ops = {
  .fault  = stmfb_iomm_vma_fault,
};

static int
stmfb_iomm_mmap (struct stmfb_info     * const i,
                 struct vm_area_struct * const vma)
{
  unsigned long rawaddr, physaddr, vsize, off;
  const struct stmcore_display_pipeline_data * const pd =
    *((struct stmcore_display_pipeline_data **)i->platformDevice->dev.platform_data);

  vma->vm_flags |= STMFB_VM_FLAGS; /* changes for diff. kernel version */
  vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

  rawaddr  = (vma->vm_pgoff << PAGE_SHIFT);
  physaddr = rawaddr + pd->io_offset;
  vsize = vma->vm_end - vma->vm_start;

  for (off=0; off<vsize; off+=PAGE_SIZE)
  {
    if (stmfb_iomm_addressable(i, rawaddr+off))
      io_remap_pfn_range(vma, vma->vm_start+off, (physaddr+off) >> PAGE_SHIFT, PAGE_SIZE, vma->vm_page_prot);
  }

  // ensure we get bus errors when we access illegal memory address
  vma->vm_ops = &stmfb_iomm_nopage_ops;

  return 0;
}


/* This is more or less a verbatim copy of fb_mmap().
 * Sadly we have to copy it on order to hook any attempt
 * to mmap() the registers. We also want to allow to mmap the
 * auxmem, which the linux fb subsystem knows nothing about.
 * In the process we are also forced to hook attempts to
 * mmap() the framebuffer itself.
 *
 * Sigh.
 */
#include <asm/fb.h>
static int
stmfb_mmap (struct fb_info        * const info,
            struct vm_area_struct * const vma)
{
  struct stmfb_info * const i = container_of (info, struct stmfb_info, info);
  unsigned long      off;
  unsigned long      start;
  u32                len;
  unsigned int       len_requested = vma->vm_end - vma->vm_start;

  if(i->platformDevice && i->platformDevice->dev.power.power_state.event != PM_EVENT_ON)
  {
    DPRINTK("MMap while in suspend state???\n");
    return -EBUSY;
  }

  off = vma->vm_pgoff << PAGE_SHIFT;

  /* frame buffer memory */
  start = info->fix.smem_start;
  len = PAGE_ALIGN((start & ~PAGE_MASK) + info->fix.smem_len);

  if (off >= len)
  {
    unsigned int idx;
    int is_auxmem = 0;

    /* check if mmap of auxmem was requested */
    for (idx = 0; !is_auxmem && idx < ARRAY_SIZE (i->AuxPart); ++idx)
    {
      if (i->AuxPart[idx]
          && off >= i->AuxBase[idx]
          && (off + len_requested) >= i->AuxBase[idx] /* fight hackers 'n crackers */
          && (off + len_requested) <= (i->AuxBase[idx] + i->AuxSize[idx]))
      {
        /* yes! */
        start = i->AuxBase[idx];
        off -= (start & PAGE_MASK);
        len = PAGE_ALIGN((start & ~PAGE_MASK) + i->AuxSize[idx]);
        is_auxmem = 1;
      }
    }

    if (!is_auxmem)
      /* memory mapped io */
      return stmfb_iomm_mmap(i, vma);
  }

  start &= PAGE_MASK;
  if ((len_requested + off) > len)
    return -EINVAL;
  off += start;
  vma->vm_pgoff = off >> PAGE_SHIFT;
  /* This is an IO map - tell maydump to skip this VMA */
  vma->vm_flags |= STMFB_VM_FLAGS; /* changes for diff. kernel version */
  fb_pgprotect(NULL, vma, off); /* using NULL here breaks powerpc! but we
                                   don't have file :( Since we're not powerpc,
                                   we don't care :) */

  DPRINTK("%s: remap vaddr %lx paddr %lx size %x prot %x\n", __FUNCTION__,
          vma->vm_start, off, len_requested, (unsigned int)&vma->vm_page_prot);

  if (io_remap_pfn_range(vma, vma->vm_start, off >> PAGE_SHIFT,
                         len_requested, vma->vm_page_prot))
    return -EAGAIN;

  return 0;
}


static int
stmfb_open (struct fb_info *info,
            int             user)
{
  int ret = 0;

  /* fbdev application */
  struct stmfb_info * const i = container_of(info, struct stmfb_info, info);

  DPRINTK ("%s from current process/pid: %p/%d\n",
           __FUNCTION__, current, current->pid);

#ifdef CONFIG_PM_RUNTIME
  if (user <= 1)
  {
    if(i->platformDevice)
      pm_runtime_get_sync(&i->platformDevice->dev);
  }
#endif

  if (user == 1)
  {
    if(i->platformDevice && i->platformDevice->dev.power.power_state.event != PM_EVENT_ON)
    {
      DPRINTK("Changing display mode while in suspend state???\n");
      ret = -EBUSY;
      goto exit_open;
    }

    if (down_interruptible (&i->framebufferLock))
    {
      ret = -ERESTARTSYS;
      goto exit_open;
    }

    /*
     * synchronize with output when we are the first instance, particularly
     * after a dynamic switch mode via v4l2
     */
    if (i->opens == 0)
    {
      stmfb_sync_fb_with_output(i);
    }

    ++i->opens;
    up (&i->framebufferLock);
  }
  else if (user != 0)
  {
    /* == 0 is fb console, others are not recognized... */
    ret = -EPERM;
  }

exit_open:
#ifdef CONFIG_PM_RUNTIME
  if(i->platformDevice && (user <= 1) && ret)
    pm_runtime_put_sync(&i->platformDevice->dev);
#endif
  return ret;
}

static int
stmfb_release (struct fb_info *info,
               int             user)
{
  int ret = 0;

  /* fbdev application */
  struct stmfb_info * const i = container_of(info, struct stmfb_info, info);

  DPRINTK ("%s from current process/pid: %p/%d\n",
           __FUNCTION__, current, current->pid);

  if (user == 1)
  {
    /* restore config when last app exited that used our own extended API */

    if(i->platformDevice && i->platformDevice->dev.power.power_state.event != PM_EVENT_ON)
    {
      DPRINTK("Changing display mode while in suspend state???\n");
      ret = -EBUSY;
      goto exit_close;
    }

    if (down_interruptible (&i->framebufferLock))
    {
      ret = -ERESTARTSYS;
      goto exit_close;
    }

    if (--i->opens == 0) {
      int was_suspended = i->fbdev_api_suspended;

      i->fbdev_api_suspended = 0;
      up (&i->framebufferLock);

      if (was_suspended)
        /* we don't care about success - basically somebody could have opened
           the device again and used the extended API before stmfb_set_par()
           could do its job. That's fine... */
        stmfb_set_par (info);
    }
    else
      up (&i->framebufferLock);
  }
  else if (user != 0)
  {
    /* == 0 is fb console, others are not recognized... */
    ret = -EPERM;
  }

exit_close:
#ifdef CONFIG_PM_RUNTIME
    if(i->platformDevice && ret == 0)
      pm_runtime_put_sync(&i->platformDevice->dev);
#endif
  return ret;
}

/*
 * Framebuffer device structure.
 */
struct fb_ops stmfb_ops = {
  .owner = THIS_MODULE,

  .fb_open    = stmfb_open,
  .fb_release = stmfb_release,

  .fb_read  = NULL,
  .fb_write = NULL,

  .fb_check_var = stmfb_check_var,
  .fb_set_par   = stmfb_set_par,

  .fb_setcolreg = stmfb_setcolreg,

  .fb_blank = NULL,

  .fb_pan_display = stmfb_pan_display,

  .fb_fillrect  = cfb_fillrect,
  .fb_copyarea  = cfb_copyarea,
  .fb_imageblit = cfb_imageblit,

  .fb_rotate = NULL,

  .fb_ioctl = stmfb_ioctl,

  .fb_mmap = stmfb_mmap,
};
