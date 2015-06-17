/***********************************************************************
 *
 * File: linux/drivers/media/video/stmvout_driver.c
 * Copyright (c) 2000-2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 * V4L2 video output device driver for ST SoC display subsystems.
 *
\***********************************************************************/

#include <linux/version.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/errno.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/mm.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-common.h>

#include <asm/semaphore.h>
#include <asm/page.h>

#include <stmdisplay.h>
#include <linux/stm/stmcoredisplay.h>

#include "stmvout_driver.h"
#include "stmvout.h"
#include "stm_v4l2.h"

struct plane_identifiers {
  stm_plane_id_t     plane_id;
  const char        *plane_name;
};

static const struct plane_identifiers device_names[] = {
  { OUTPUT_GDP1,   "RGB0" },
  { OUTPUT_GDP2,   "RGB1" },
  { OUTPUT_GDP3,   "RGB2" },
  { OUTPUT_GDP4,   "RGB3" },
  { OUTPUT_VID1,   "YUV0" },
  { OUTPUT_VID2,   "YUV1" },
  { OUTPUT_NULL,   "NULL"  },
  { OUTPUT_CUR,    "CUR"  },

  { OUTPUT_VIRT1,   "RGB1.0"  },
  { OUTPUT_VIRT2,   "RGB1.1"  },
  { OUTPUT_VIRT3,   "RGB1.2"  },
  { OUTPUT_VIRT4,   "RGB1.3"  },
  { OUTPUT_VIRT5,   "RGB1.4"  },
  { OUTPUT_VIRT6,   "RGB1.5"  },
  { OUTPUT_VIRT7,   "RGB1.6"  },
  { OUTPUT_VIRT8,   "RGB1.7"  },
  { OUTPUT_VIRT9,   "RGB1.8"  },
  { OUTPUT_VIRT10,  "RGB1.9"  },
  { OUTPUT_VIRT11,  "RGB1.10" },
  { OUTPUT_VIRT12,  "RGB1.11" },
  { OUTPUT_VIRT13,  "RGB1.12" },
  { OUTPUT_VIRT14,  "RGB1.13" },
  { OUTPUT_VIRT15,  "RGB1.14" },
};

static int video_nr = -1;
module_param(video_nr, int, 0444);

static stvout_device_t g_voutData[MAX_PIPELINES][MAX_PLANES];


static int
init_device_locked (struct _stvout_device * const device)
{
  int ret;

  stmvout_init_buffer_queues (device);

  if((ret = stmvout_allocate_clut (device))<0)
    return ret;

  /* Try and grab the hardware plane for this device, this can fail if it is
     already in use by another part of the display subsystem. */
  if (!device->pPlane) {
    device->pPlane = stm_display_get_plane (device->displayPipe.device,
					    device->planeConfig->id);
    if (!device->pPlane)
      return signal_pending (current) ? -ERESTARTSYS : -EBUSY;
  }

  return 0;
}


static int
stmvout_close_locked (struct _open_data     *open_data,
		      struct _stvout_device *device)
{
  int n;
  int opencount;

  if (open_data->isFirstUser) {
    debug_msg ("%s: turning off streaming and releasing buffers\n",
               __FUNCTION__);

    /* turned off streaming in case it's on */
    if (stmvout_streamoff (device) < 0)
      return -ERESTARTSYS;

    if (!stmvout_deallocate_mmap_buffers (device)) {
      panic ("%s: buffers still mmapped!\n", __FUNCTION__);
    }

  }

  for (opencount = 0, n = 0; n < MAX_OPENS; ++n)
    if (device->openData[n].isOpen)
      ++opencount;

  if (opencount == 1) {
    debug_msg ("%s: last user closed device\n", __FUNCTION__);
    if (device->pPlane) {
      debug_msg ("%s: releasing hardware resources\n", __FUNCTION__);

      if (stm_display_plane_release (device->pPlane) < 0)
	return -ERESTARTSYS;

      device->pPlane = NULL;
    }

    stmvout_deallocate_clut (device);
  }

  open_data->isOpen      = 0;
  open_data->isFirstUser = 0;
  open_data->pDev        = NULL;

  return 0;
}

static int
stmvout_close (struct stm_v4l2_handles    *handle,
	       enum _stm_v4l2_driver_type  type,
	       struct file                *file)
{
  open_data_t           *open_data = handle->v4l2type[type].handle;
  struct _stvout_device *device;
  int                    ret;

  if (!open_data)
    return 0;

  debug_msg ("%s in.\n", __FUNCTION__);

  device = open_data->pDev;

  /* Note that this is structured to allow the call to be interrupted by a
     signal and then restarted without corrupting the internal state. */
  if (down_interruptible (&device->devLock))
    return -ERESTARTSYS;

  ret = stmvout_close_locked (open_data, device);

  up (&device->devLock);

  debug_msg ("%s out.\n", __FUNCTION__);

  return ret;
}


static int
stmvout_ioctl (struct stm_v4l2_handles    *handle,
	       struct stm_v4l2_driver     *driver,
	       int                         minor,
	       enum _stm_v4l2_driver_type  type,
	       struct file                *file,
	       unsigned int                cmd,
	       void                       *arg)
{
  int ret;
  open_data_t      *pOpenData = handle ? handle->v4l2type[type].handle : NULL;
  stvout_device_t  *pDev = NULL;
  struct semaphore *pLock = NULL;

  if (pOpenData) {
    BUG_ON (pOpenData->pDev == NULL);

    pDev  = pOpenData->pDev;
    pLock = &pDev->devLock;

    if (down_interruptible (pLock))
      return -ERESTARTSYS;
  }

  if (!pDev)
    switch (cmd) {
    case VIDIOC_ENUMOUTPUT:
    case VIDIOC_S_OUTPUT:
    case VIDIOC_G_OUTPUT:
      break;

    default:
      return -EINVAL;
    };


  switch (cmd) {
  case VIDIOC_ENUM_FMT:
    {
      struct v4l2_fmtdesc * const f = arg;

      if (f->type != V4L2_BUF_TYPE_VIDEO_OUTPUT)
	goto err_inval;

      if ((ret = stmvout_enum_buffer_format (pDev, f)) < 0)
	goto err_ret;
    }
    break;

  case VIDIOC_G_FMT:
    {
      struct v4l2_format * const fmt = arg;

      switch (fmt->type) {
      case V4L2_BUF_TYPE_VIDEO_OUTPUT:
	fmt->fmt.pix = pDev->bufferFormat.fmt.pix;
	break;

      default:
	err_msg ("G_FMT invalid buffer type %d\n", fmt->type);
	goto err_inval;
      }
    }
    break;

  case VIDIOC_S_FMT:
    {
      struct v4l2_format * const fmt = arg;

      switch (fmt->type) {
      case V4L2_BUF_TYPE_VIDEO_OUTPUT:
	/* Check ioctl is legal */
	if (!pOpenData->isFirstUser) {
	  err_msg ("%s: VIDIOC_S_FMT: changing IO format not available on "
		   "this file descriptor\n", __FUNCTION__);
	  goto err_busy;
	}

	if (pDev->isStreaming) {
	  err_msg ("%s: VIDIOC_S_FMT: device is streaming data, cannot "
		   "change format\n", __FUNCTION__);
	  goto err_busy;
	}

	if ((ret = stmvout_set_buffer_format (pDev, fmt, 1)) < 0)
	  goto err_ret;
	break;

      default:
	err_msg ("%s: VIDIOC_S_FMT: invalid buffer type %d\n",
		 __FUNCTION__, fmt->type);
	goto err_inval;
      }
    }
    break;

  case VIDIOC_TRY_FMT:
    {
      struct v4l2_format * const fmt = arg;

      switch (fmt->type) {
      case V4L2_BUF_TYPE_VIDEO_OUTPUT:
	if ((ret = stmvout_set_buffer_format (pDev, fmt, 0)) < 0)
	  goto err_ret;

	break;

      default:
	err_msg ("%s: VIDIOC_TRY_FMT: invalid buffer type %d\n",
		 __FUNCTION__, fmt->type);
	goto err_inval;
      }
    }
    break;

  case VIDIOC_CROPCAP:
    {
      struct v4l2_cropcap * const cropcap = arg;

      if (cropcap->type != V4L2_BUF_TYPE_VIDEO_OUTPUT) {
	debug_msg ("%s: VIDIOC_CROPCAP: invalid crop type %d\n",
		   __FUNCTION__, cropcap->type);
	goto err_inval;
      }

      if ((ret = stmvout_get_display_size (pDev, cropcap)) < 0)
	goto err_ret;
    }
    break;

  case VIDIOC_G_CROP:
    {
      struct v4l2_crop * const crop = arg;

      switch (crop->type) {
      case V4L2_BUF_TYPE_VIDEO_OUTPUT:
	crop->c = pDev->outputCrop.c;
	break;

      case V4L2_BUF_TYPE_PRIVATE:
	crop->c = pDev->srcCrop.c;
	break;

      default:
	err_msg ("%s: VIDIOC_G_CROP: invalid crop type %d\n",
		 __FUNCTION__, crop->type);
	goto err_inval;
      }
    }
    break;

  case VIDIOC_S_CROP:
    {
      struct v4l2_crop * const crop = arg;

      switch (crop->type) {
      case V4L2_BUF_TYPE_VIDEO_OUTPUT:
	if ((ret = stmvout_set_output_crop (pDev, crop)) < 0)
	  goto err_ret;
	break;

      case V4L2_BUF_TYPE_PRIVATE:
	/*
	 * This is a private interface for specifying the crop of the
	 * source buffer to be displayed, as opposed to the crop on the
	 * screen. V4L2 has no way of doing this but we need to be able to
	 * to accomodate MPEG decode where:
	 *
	 * 1. the actual image size is only known once you start decoding
         *    (generally too late to set the buffer format and allocate
         *    display buffers)
	 * 2. the buffer dimensions may be larger than the image size due to
	 *    HW restrictions (e.g. Macroblock formats, 720 pixel wide video
         *    must be in a 736 pixel wide buffer to have an even number of
         *    macroblocks)
	 * 3. the image size can change dynamically in the stream.
	 * 4. we need to support pan/scan and center cutout with widescreen->
	 *    4:3 scaling
	 * 5. to support DVD zoom modes
	 *
         */
	if ((ret = stmvout_set_buffer_crop (pDev, crop)) < 0)
	  goto err_ret;
	break;

      default:
	err_msg ("%s: VIDIOC_S_CROP: invalid buffer type %d\n",
		 __FUNCTION__, crop->type);
	goto err_inval;
      }
    }
    break;

  case VIDIOC_G_PARM:
    {
      struct v4l2_streamparm * const sp = arg;

      if (sp->type != V4L2_BUF_TYPE_VIDEO_OUTPUT)
	goto err_inval;

      /*
       * The output streaming params are only relevant when using write(),
       * not when using real streaming buffers. As we do not support the
       * write() interface this is always zeroed.
       */
      memset (&sp->parm.output, 0, sizeof (struct v4l2_outputparm));
    }
    break;

  case VIDIOC_S_PARM:
    goto err_inval;

  case VIDIOC_REQBUFS:
    {
      struct v4l2_requestbuffers * const req = arg;

      if (!pOpenData->isFirstUser) {
	err_msg ("%s: VIDIOC_REQBUFS: illegal in non-io open\n", __FUNCTION__);
	goto err_busy;
      }

      if (req->type != V4L2_BUF_TYPE_VIDEO_OUTPUT
	  || (req->memory != V4L2_MEMORY_MMAP
	      && req->memory != V4L2_MEMORY_USERPTR)) {
	err_msg ("%s: VIDIOC_REQBUFS: unsupported type (%d) or memory "
		 "parameters (%d)\n", __FUNCTION__, req->type, req->memory);
	goto err_inval;
      }

      if (pDev->bufferFormat.type != V4L2_BUF_TYPE_VIDEO_OUTPUT) {
        err_msg ("%s: VIDIOC_REQBUFS: pixel format not set, cannot allocate "
		 "buffers\n", __FUNCTION__);
	goto err_inval;
      }

      if (!stmvout_deallocate_mmap_buffers (pDev)) {
        err_msg ("%s: VIDIOC_REQBUFS: video buffer(s) still mapped, cannot "
		 "change\n", __FUNCTION__);
	goto err_busy;
      }

      if (req->memory == V4L2_MEMORY_MMAP
          && !stmvout_allocate_mmap_buffers (pDev, req)) {
        err_msg ("%s: VIDIOC_REQBUFS: unable to allocate video buffers\n",
		 __FUNCTION__);
	ret = -ENOMEM;
	goto err_ret;
      }
    }
    break;

  case VIDIOC_QUERYBUF:
    {
      struct v4l2_buffer * const pBuf = arg;
      streaming_buffer_t * const pStrmBuf = &pDev->streamBuffers[pBuf->index];

      if (!pOpenData->isFirstUser) {
	err_msg ("%s: VIDIOC_QUERYBUF: IO not available on this file "
		 "descriptor\n", __FUNCTION__);
	goto err_busy;
      }

      if (pBuf->type != V4L2_BUF_TYPE_VIDEO_OUTPUT) {
        err_msg ("%s: VIDIOC_QUERYBUF: illegal stream type\n", __FUNCTION__);
	goto err_inval;
      }

      if (pBuf->index >= pDev->n_streamBuffers
          || !pStrmBuf->physicalAddr) {
        err_msg ("%s: VIDIOC_QUERYBUF: bad parameter v4l2_buffer.index: %u\n",
		 __FUNCTION__, pBuf->index);
	goto err_inval;
      }

      *pBuf = pStrmBuf->vidBuf;
    }
    break;

  case VIDIOC_QBUF:
    {
      struct v4l2_buffer * const pBuf = arg;

      if (!pOpenData->isFirstUser) {
	err_msg ("%s: VIDIOC_QBUF: IO not available on this file descriptor\n",
		 __FUNCTION__);
	goto err_busy;
      }

      if (pBuf->type != V4L2_BUF_TYPE_VIDEO_OUTPUT) {
	err_msg ("%s: VIDIOC_QBUF: illegal stream type\n", __FUNCTION__);
	goto err_inval;
      }

      if (pBuf->memory != V4L2_MEMORY_MMAP
	  && pBuf->memory != V4L2_MEMORY_USERPTR) {
        err_msg ("%s: VIDIOC_QBUF: illegal memory type %d\n", __FUNCTION__,
		 pBuf->memory);
	goto err_inval;
      }

      if (pDev->outputCrop.type != V4L2_BUF_TYPE_VIDEO_OUTPUT) {
        err_msg ("%s: VIDIOC_QBUF: no valid output crop set, cannot queue "
		 "buffer\n", __FUNCTION__);
	goto err_inval;
      }

      if ((ret = stmvout_queue_buffer (pDev, pBuf)) < 0)
	goto err_ret;
    }
    break;

  case VIDIOC_DQBUF:
    {
      struct v4l2_buffer * const pBuf = arg;

      if (!pOpenData->isFirstUser) {
	err_msg ("%s: VIDIOC_DQBUF: IO not available on this file descriptor\n",
		 __FUNCTION__);
	goto err_busy;
      }

      if (pBuf->type != V4L2_BUF_TYPE_VIDEO_OUTPUT) {
        err_msg ("%s: VIDIOC_DQBUF: illegal stream type\n", __FUNCTION__);
	goto err_inval;
      }

      /*
       * The API documentation states that the memory field must be set
       * correctly by the application for the call to succeed. It isn't clear
       * under what circumstances this would ever be needed and the requirement
       * seems a bit over zealous, but we honour it anyway. I bet users will
       * miss the small print and log lots of bugs that this call doesn't work.
       */
      if (pBuf->memory != V4L2_MEMORY_MMAP
	  && pBuf->memory != V4L2_MEMORY_USERPTR) {
        err_msg ("%s: VIDIOC_DQBUF: illegal memory type %d\n",
		 __FUNCTION__, pBuf->memory);
	goto err_inval;
      }

      /*
       * We can release the driver lock now as stmvout_dequeue_buffer is safe
       * (the buffer queues have their own access locks), this makes
       * the logic a bit simpler particularly in the blocking case.
       */
      up (pLock);

      if ((file->f_flags & O_NONBLOCK) == O_NONBLOCK) {
	debug_msg ("%s: VIDIOC_DQBUF: Non-Blocking dequeue\n", __FUNCTION__);
        if (!stmvout_dequeue_buffer (pDev, pBuf))
          return -EAGAIN;

        return 0;
      } else {
	debug_msg ("%s: VIDIOC_DQBUF: Blocking dequeue\n", __FUNCTION__);
        return wait_event_interruptible (pDev->wqBufDQueue,
					 stmvout_dequeue_buffer (pDev, pBuf));
      }
    }
    break;

  case VIDIOC_STREAMON:
    {
      const __u32 * const type = arg;

      if (*type != V4L2_BUF_TYPE_VIDEO_OUTPUT) {
        err_msg ("%s: VIDIOC_STREAMON: illegal stream type %d\n",
		 __FUNCTION__, *type);
	goto err_inval;
      }

      if (pDev->isStreaming) {
        err_msg ("%s: VIDIOC_STREAMON: device already streaming\n",
		 __FUNCTION__);
	goto err_busy;
      }

#if defined(_STRICT_V4L2_API)
      if (!stmvout_has_queued_buffers (pDev)) {
        /* The API states that at least one buffer must be queued before
           STREAMON succeeds. */
	err_msg ("%s: VIDIOC_STREAMON: no buffers queued on stream\n",
		 __FUNCTION__);
	goto err_inval;
      }
#endif

      debug_msg ("%s: VIDIOC_STREAMON: Attaching plane to output\n",
                 __FUNCTION__);
      if (stm_display_plane_lock (pDev->pPlane) < 0) {
	if (signal_pending (current))
      	  goto err_restartsys;
	goto err_busy;
      }

      if (stm_display_plane_connect_to_output (pDev->pPlane,
                                               pDev->pOutput) < 0) {
	stm_display_plane_unlock (pDev->pPlane);
      	if (signal_pending (current))
      	  goto err_restartsys;
	ret = -EIO;
	goto err_ret;
      }

      debug_msg ("%s: VIDIOC_STREAMON: Starting the stream\n", __FUNCTION__);

      pDev->isStreaming = 1;
      wake_up_interruptible (&(pDev->wqStreamingState));

      /* Make sure any frames already on the pending queue are written to the
         hardware */
      stmvout_send_next_buffer_to_display (pDev);
    }
    break;

  case VIDIOC_STREAMOFF:
    {
      const __u32 * const type = arg;

      if (*type != V4L2_BUF_TYPE_VIDEO_OUTPUT) {
	err_msg ("%s: VIDIOC_STREAMOFF: illegal stream type\n", __FUNCTION__);
	goto err_inval;
      }

      if ((ret = stmvout_streamoff (pDev)) < 0)
	goto err_ret;
    }
    break;

  case VIDIOC_G_OUTPUT:
    {
      int i;
      int * const output = (int *) arg;

      *output = -1;
      for (i = 0; i < ARRAY_SIZE (g_voutData[minor]); ++i)
	if (pDev == &g_voutData[minor][i]) {
	  *output = i + driver->index_offset[minor];
	  break;
	}
    }
    break;

  case VIDIOC_S_OUTPUT:
    {
      int output = (*(int *) arg) - driver->index_offset[minor];
      int n, is_first_user;
      stvout_device_t  * const new_dev = &g_voutData[minor][output];
      struct semaphore *new_lock;
      open_data_t      *new_opendata;

      if (output < 0 || output >= driver->n_indexes[minor]) {
	err_msg ("S_OUTPUT: Output number %d out of range %d...%d\n",
		 output, 0, driver->n_indexes[minor]);
	goto err_inval;
      }

      /* same output as is already set? */
      if (new_dev == pDev) {
	ret = 0;
	goto err_ret;
      }

      /* if current output is busy, we can't change it. */
      if (pDev) {
	/* it's busy if the first opener is streaming or has buffers queued.
	   Following openers may not start streaming/queueing buffers, so we
	   only need to do this check for the first user.
	   Following openers may change the output associated with their
           filehandle, though. */
	if (pOpenData->isFirstUser
	    && (pDev->isStreaming || stmvout_has_queued_buffers (pDev))) {
	  err_msg ("S_OUTPUT: Output %d is active, cannot disassociate\n",
		   output);
	  goto err_busy;
	}
      }

      /* we should first check if we will be able to support another open on
         the new device and allocate it for use by ourselves, so that we
         don't end up without any device in case this fails... */
      new_lock = &new_dev->devLock;
      if (down_interruptible (new_lock))
	goto err_restartsys;

      is_first_user = 1;
      for (new_opendata = NULL, n = 0; n < MAX_OPENS; ++n) {
	if (!new_dev->openData[n].isOpen) {
	  if (unlikely (!new_opendata))
	    new_opendata = &new_dev->openData[n];
	} else if (new_dev->openData[n].isFirstUser)
	  is_first_user = 0;
      }
      if (unlikely (!new_opendata)) {
	err_msg ("S_OUTPUT: No more opens on this device\n");
	up (new_lock);
	goto err_busy;
      }

      if ((ret = init_device_locked (new_dev)) != 0) {
	up (new_lock);
	goto err_ret;
      }

      /* we have the device now, release the old one */

      if (pDev) {
	if ((ret = stmvout_close_locked (pOpenData, pDev)) != 0) {
	  up (new_lock);
	  goto err_ret;
	}

	up (pLock);
      }

      /* remember state */
      pDev = new_dev;
      pLock = new_lock;
      pOpenData = new_opendata;

      pOpenData->isOpen      = 1;
      pOpenData->isFirstUser = is_first_user;
      pOpenData->pDev        = pDev;
      debug_msg ("v4l2_open: isFirstUser: %c\n",
		 pOpenData->isFirstUser ? 'y' : 'n');

      stmvout_get_controls_from_core (pDev);

      handle->v4l2type[type].handle = pOpenData;
    }
    break;

  case VIDIOC_ENUMOUTPUT:
    {
      struct v4l2_output * const output = arg;
      int                 index = (output->index
                                   - driver->index_offset[minor]);

      if (index < 0 || index >= driver->n_indexes[minor])
	goto err_inval;

      snprintf (output->name, sizeof (output->name),
		"%s", g_voutData[minor][index].name);
      output->type      = V4L2_OUTPUT_TYPE_ANALOG;
      output->audioset  = 0;
      output->modulator = 0;

      if ((ret = stmvout_get_supported_standards (&g_voutData[minor][index],
                                                  &output->std)) < 0)
        goto err_ret;
    }
    break;

  case VIDIOC_G_OUTPUT_STD:
    {
      v4l2_std_id * const std_id = arg;

      if ((ret = stmvout_get_current_standard (pDev, std_id)) < 0)
	goto err_ret;
    }
    break;

  case VIDIOC_S_OUTPUT_STD:
    {
      v4l2_std_id * const std_id = arg;

      if ((ret = stmvout_set_current_standard (pDev, *std_id)) < 0)
	goto err_ret;
    }
    break;

  case VIDIOC_ENUM_OUTPUT_STD:
    {
      struct v4l2_standard * const std = arg;

      if ((ret = stmvout_get_display_standard (pDev, std)) < 0)
	goto err_ret;
    }
    break;

  case VIDIOC_QUERYCTRL:
    ret = stmvout_queryctrl (pDev, g_voutData[minor],
                             (struct v4l2_queryctrl * ) arg);
    goto err_ret;
    break;

  case VIDIOC_QUERYMENU:
    ret = stmvout_querymenu (pDev, g_voutData[minor],
                             (struct v4l2_querymenu *) arg);
    goto err_ret;
    break;

  case VIDIOC_S_CTRL:
    ret = stmvout_s_ctrl (pDev, g_voutData[minor],
                          (struct v4l2_control *) arg);
    goto err_ret;
    break;

  case VIDIOC_G_CTRL:
    ret = stmvout_g_ctrl (pDev, g_voutData[minor],
                          (struct v4l2_control *) arg);
    goto err_ret;
    break;

  case VIDIOC_G_EXT_CTRLS:
    ret = stmvout_g_ext_ctrls (pDev, (struct v4l2_ext_controls *) arg);
    goto err_ret;
    break;

  case VIDIOC_S_EXT_CTRLS:
    ret = stmvout_s_ext_ctrls (pDev, (struct v4l2_ext_controls *) arg);
    goto err_ret;
    break;

  case VIDIOC_TRY_EXT_CTRLS:
    ret = stmvout_try_ext_ctrls (pDev, (struct v4l2_ext_controls *) arg);
    goto err_ret;
    break;

  case VIDIOC_S_OUTPUT_ALPHA:
    {
      unsigned int alpha = * (unsigned int *) arg;

      if (alpha > 255)
	goto err_inval;

      pDev->globalAlpha = alpha;
    }
    break;

  /* All of the following are not supported and there need to return -EINVAL
     as per the V4L2 API spec. */
  case VIDIOC_ENUMAUDIO:
  case VIDIOC_ENUMAUDOUT:
  case VIDIOC_ENUMINPUT:
  case VIDIOC_G_AUDIO:
  case VIDIOC_S_AUDIO:
  case VIDIOC_G_AUDOUT:
  case VIDIOC_S_AUDOUT:
#ifdef VIDIOC_G_COMP
  /* These are currently ifdef'd out in the kernel headers, being
     deprecated? */
  case VIDIOC_G_COMP:
  case VIDIOC_S_COMP:
#endif
  case VIDIOC_G_FBUF:
  case VIDIOC_S_FBUF:
  case VIDIOC_G_FREQUENCY:
  case VIDIOC_S_FREQUENCY:
  case VIDIOC_G_INPUT:
  case VIDIOC_S_INPUT:
  case VIDIOC_G_JPEGCOMP:
  case VIDIOC_S_JPEGCOMP:
  case VIDIOC_G_MODULATOR:
  case VIDIOC_S_MODULATOR:
  case VIDIOC_G_PRIORITY:
  case VIDIOC_S_PRIORITY:
  case VIDIOC_G_TUNER:
  case VIDIOC_S_TUNER:
  case VIDIOC_OVERLAY:
    goto err_inval;

  default:
    ret = -ENOTTY;
    goto err_ret;
  }

  if (pLock) up (pLock);
  return 0;

err_restartsys:
  if (pLock) up (pLock);
  return -ERESTARTSYS;

err_busy:
  if (pLock) up (pLock);
  return -EBUSY;

err_inval:
  if (pLock) up (pLock);
  return -EINVAL;

err_ret:
  if (pLock) up (pLock);
  return ret;
}

/**********************************************************
 * mmap helper functions
 **********************************************************/
static void
stmvout_vm_open (struct vm_area_struct *vma)
{
  streaming_buffer_t *pBuf = vma->vm_private_data;

  debug_msg ("%s: %p [count=%d,vma=%08lx-%08lx]\n", __FUNCTION__, pBuf,
             pBuf->mapCount, vma->vm_start, vma->vm_end);

  pBuf->mapCount++;
}

static void
stmvout_vm_close (struct vm_area_struct *vma)
{
  streaming_buffer_t * const pBuf = vma->vm_private_data;

  debug_msg("%s: %p [count=%d,vma=%08lx-%08lx]\n", __FUNCTION__, pBuf,
             pBuf->mapCount, vma->vm_start, vma->vm_end);

  if (--pBuf->mapCount == 0) {
    debug_msg ("%s: %p clearing mapped flag\n", __FUNCTION__, pBuf);
    pBuf->vidBuf.flags &= ~V4L2_BUF_FLAG_MAPPED;
  }

  return;
}

static struct page*
stmvout_vm_nopage (struct vm_area_struct *vma,
		   unsigned long          vaddr,
		   int                   *type)
{
  struct page *page;
  void *page_addr;
  unsigned long page_frame;

  debug_msg ("%s: fault @ %08lx [vma %08lx-%08lx]\n", __FUNCTION__,
	     vaddr, vma->vm_start, vma->vm_end);

  if (vaddr > vma->vm_end)
    return NOPAGE_SIGBUS;

  /*
   * Note that this assumes an identity mapping between the page offset and
   * the pfn of the physical address to be mapped. This will get more complex
   * when the 32bit SH4 address space becomes available.
   */
  page_addr = (void*)((vaddr - vma->vm_start) + (vma->vm_pgoff << PAGE_SHIFT));

  page_frame = ((unsigned long) page_addr >> PAGE_SHIFT);

  if (!pfn_valid (page_frame))
    return NOPAGE_SIGBUS;

  page = virt_to_page (__va (page_addr));

  get_page (page);

  if (type)
    *type = VM_FAULT_MINOR;
  return page;
}

static struct vm_operations_struct stmvout_vm_ops_memory =
{
  .open   = stmvout_vm_open,
  .close  = stmvout_vm_close,
  .nopage = stmvout_vm_nopage,
};

static struct vm_operations_struct stmvout_vm_ops_ioaddr =
{
  .open   = stmvout_vm_open,
  .close  = stmvout_vm_close,
  .nopage = NULL,
};

static int
stmvout_mmap (struct stm_v4l2_handles    *handle,
	      enum _stm_v4l2_driver_type  type,
	      struct file                *file,
	      struct vm_area_struct      *vma)
{
  open_data_t        * const pOpenData = handle->v4l2type[type].handle;
  stvout_device_t    *pDev;
  streaming_buffer_t *pBuf;

  if (!(vma->vm_flags & VM_WRITE)) {
    debug_msg ("mmap app bug: PROT_WRITE please\n");
    return -EINVAL;
  }

  if (!(vma->vm_flags & VM_SHARED)) {
    debug_msg ("mmap app bug: MAP_SHARED please\n");
    return -EINVAL;
  }

  BUG_ON (pOpenData == NULL);

  pDev = pOpenData->pDev;
  if (down_interruptible (&pDev->devLock))
    return -ERESTARTSYS;

  if (!pOpenData->isFirstUser) {
    err_msg ("mmap() another open file descriptor exists and is doing IO\n");
    goto err_busy;
  }

  debug_msg ("v4l2_mmap offset = 0x%08X\n", (int) vma->vm_pgoff);
  pBuf = stmvout_get_buffer_from_mmap_offset (pDev, vma->vm_pgoff*PAGE_SIZE);
  if (!pBuf) {
    /* no such buffer */
    err_msg ("%s: Invalid offset parameter\n", __FUNCTION__);
    goto err_inval;
  }

  debug_msg ("v4l2_mmap pBuf = 0x%08lx\n", (unsigned long) pBuf);

  if (pBuf->vidBuf.length != (vma->vm_end - vma->vm_start)) {
    /* wrong length */
    err_msg ("%s: Wrong length parameter\n", __FUNCTION__);
    goto err_inval;
  }

  if (!pBuf->cpuAddr) {
    /* not requested */
    err_msg ("%s: Buffer is not available for mapping\n", __FUNCTION__);
    goto err_inval;
  }

  if (pBuf->mapCount > 0) {
    err_msg ("%s: Buffer is already mapped\n", __FUNCTION__);
    goto err_busy;
  }

  vma->vm_flags       |= VM_RESERVED | VM_IO | VM_DONTEXPAND | VM_LOCKED;
  vma->vm_page_prot    = pgprot_noncached (vma->vm_page_prot);
  vma->vm_private_data = pBuf;

  if (virt_addr_valid (pBuf->cpuAddr)) {
    debug_msg ("%s: remapping memory.\n", __FUNCTION__);
    vma->vm_ops = &stmvout_vm_ops_memory;
  } else {
    debug_msg ("%s: remapping IO space.\n", __FUNCTION__);
    /* Note that this assumes an identity mapping between the page offset and
       the pfn of the physical address to be mapped. This will get more
       complex when the 32bit SH4 address space becomes available. */
    if (remap_pfn_range (vma, vma->vm_start, vma->vm_pgoff,
			 (vma->vm_end - vma->vm_start), vma->vm_page_prot)) {
      up (&pDev->devLock);
      return -EAGAIN;
    }

    vma->vm_ops = &stmvout_vm_ops_ioaddr;
  }

  pBuf->mapCount = 1;
  pBuf->vidBuf.flags |= V4L2_BUF_FLAG_MAPPED;

  up (&pDev->devLock);

  return 0;

err_busy:
  up (&pDev->devLock);
  return -EBUSY;

err_inval:
  up (&pDev->devLock);
  return -EINVAL;
}


/************************************************
 * v4l2_poll
 ************************************************/
static unsigned int
stmvout_poll (struct stm_v4l2_handles    *handle,
	      enum _stm_v4l2_driver_type  type,
	      struct file                *file,
	      poll_table                 *table)
{
  open_data_t     * const pOpenData = handle->v4l2type[type].handle;
  stvout_device_t *pDev;
  unsigned int     mask;

  BUG_ON (pOpenData == NULL);
  pDev = pOpenData->pDev;

  if (down_interruptible (&pDev->devLock))
    return -ERESTARTSYS;

  /* Add DQueue wait queue to the poll wait state */
  poll_wait (file, &pDev->wqBufDQueue,      table);
  poll_wait (file, &pDev->wqStreamingState, table);

  if (!pOpenData->isFirstUser)
    mask = POLLERR;
  else if (!pDev->isStreaming)
    mask = POLLOUT; /* Write is available when not streaming */
  else if (stmvout_has_completed_buffers (pDev))
    mask = POLLIN; /* Indicate we can do a DQUEUE ioctl */
  else
    mask = 0;

  up (&pDev->devLock);

  return mask;
}


static int
ConfigureVOUTDevice (stvout_device_t *pDev,
		     int              pipelinenr,
		     int              planenr)
{
  int ret;
  int i;
  debug_msg("ConfigureVOUTDevice in. pDev = 0x%08X\n", (unsigned int)pDev);

  init_waitqueue_head(&(pDev->wqBufDQueue));
  init_waitqueue_head(&(pDev->wqStreamingState));
  sema_init(&(pDev->devLock),1);

  stmcore_get_display_pipeline(pipelinenr,&pDev->displayPipe);

  pDev->planeConfig = &pDev->displayPipe.planes[planenr];

  i = 0;
  while((i < ARRAY_SIZE(device_names))
        && (device_names[i].plane_id != pDev->planeConfig->id))
    i++;
  BUG_ON (i == ARRAY_SIZE(device_names));

  sprintf (pDev->name, "%s", device_names[i].plane_name);

  debug_msg ("ConfigureVOUTDevice name = %s\n", pDev->name);

  pDev->globalAlpha  = 255;  /* Initially set to opqaue */

  pDev->currentOutputNum = -1;

  if((ret = stmvout_set_video_output(pDev, SVOI_MAIN)) < 0)
    return ret;

  debug_msg("ConfigureVOUTDevice out.\n");

  return 0;
}

static struct stm_v4l2_driver stmvout_stm_v4l2_driver = {
  .name  = "stmvout",
  .type  = STM_V4L2_VIDEO_OUTPUT,

  .ioctl = stmvout_ioctl,
  .close = stmvout_close,
  .poll  = stmvout_poll,
  .mmap  = stmvout_mmap,

  .control_classes = stmvout_ctrl_classes,
};

static void __exit
stmvout_exit (void)
{
  int i,j;

  stm_v4l2_unregister_driver (&stmvout_stm_v4l2_driver);

  for(i=0;i<MAX_PIPELINES;i++)
  {
    for(j=0;j<MAX_PLANES;j++)
    {
      if(g_voutData[i][j].pOutput)
        stm_display_output_release(g_voutData[i][j].pOutput);
    }
  }
}


static int __init
stmvout_init (void)
{
  struct stmcore_display_pipeline_data tmpdisplaypipe;
  int                                  pipeline;
  int                                  plane;
  int                                  registered_plane;

  pipeline = 0;
  while (stmcore_get_display_pipeline (pipeline, &tmpdisplaypipe) == 0) {
    /* complain when we need to increase STM_V4L2_MAX_DEVICES */
    if (pipeline >= STM_V4L2_MAX_DEVICES) {
      WARN_ON (pipeline >= STM_V4L2_MAX_DEVICES);
      break;
    }

    plane = registered_plane = 0;
    while (tmpdisplaypipe.planes[plane].id != 0) {
      /* complain when we need to increase MAX_PLANES */
      if (registered_plane >= MAX_PLANES) {
	WARN_ON (registered_plane >= MAX_PLANES);
	break;
      }

      if (ConfigureVOUTDevice (&g_voutData[pipeline][registered_plane],
			       pipeline, plane) == 0)
	++registered_plane;
      else
	printk ("%s: Failed to configure a device we know exists: %d:%d:%d\n",
		__FUNCTION__,
		pipeline, plane, tmpdisplaypipe.planes[plane].id);

      ++plane;
    }

    stmvout_stm_v4l2_driver.control_range[pipeline].first = 0;
    stmvout_stm_v4l2_driver.control_range[pipeline].last = 0;
    stmvout_stm_v4l2_driver.n_indexes[pipeline] = plane;

    pipeline++;
  }

  return stm_v4l2_register_driver (&stmvout_stm_v4l2_driver);
}

module_init (stmvout_init);
module_exit (stmvout_exit);

MODULE_LICENSE ("GPL");
