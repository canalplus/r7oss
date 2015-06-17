/***********************************************************************\
 *
 * File: linux/drivers/media/video/stm_v4l2.c
 * Copyright (c) 2007-2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 * V4L2 generic video driver
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
#include <linux/interrupt.h>

#include <asm/semaphore.h>
#include <asm/page.h>
#include <asm/io.h>
#include <asm/page.h>
#include <asm/uaccess.h>
#include <asm/irq.h>

#include <media/v4l2-dev.h>
#include <media/v4l2-common.h>

#include "stm_v4l2.h"
#include "stmvout.h"

#if defined DEBUG
#  define PKMOD "stm_v4l2: "
#  define debug_msg(fmt,arg...) printk(PKMOD fmt,##arg)
#  define err_msg(fmt,arg...) printk(PKMOD fmt,##arg)
#  define info_msg(fmt,arg...) printk(PKMOD fmt,##arg)
#else
#  define debug_msg(fmt,arg...)
#  define err_msg(fmt,arg...)
#  define info_msg(fmt,arg...)
#endif



#define MAX_DRIVERS  8 /* The maximum number of drivers that can be
                        * registered */
#define MAX_DEVICES  3 /* The maximum number of devices (for us that's
                        * independent displays) */
#define MAX_HANDLES 32 /* Maximum number of allowed file handles */


static struct video_device     devices[STM_V4L2_MAX_DEVICES];

static struct stm_v4l2_driver  drivers[MAX_DRIVERS];
static struct stm_v4l2_handles handles[MAX_HANDLES];

static int start_indexes[STM_V4L2_MAX_TYPES][STM_V4L2_MAX_DEVICES];



int
stm_v4l2_register_driver (const struct stm_v4l2_driver * const driver)
{
  int i;
  int occupy = -1;

  BUG_ON (driver == NULL);

  /* some sanity checking first */
  for (i = 0; i < STM_V4L2_MAX_DEVICES; ++i) {
    const struct stm_v4l2_driver_control_range * const thiz
      = &driver->control_range[i];
    int j;

    /* last has to be > than first */
    if (unlikely (thiz->first > thiz->last)) {
      printk ("%s: invalid range: %d -> %d\n", __FUNCTION__,
	      driver->control_range[i].first, driver->control_range[i].last);

      return -EINVAL;
    }

    for (j = 0; j < MAX_DRIVERS; ++j) {
      int k;

      /* found a nice place where we will put this driver, if tests are
         sucessful */
      if (!drivers[j].inuse) {
	if (occupy == -1)
	  occupy = j;
	continue;
      }

      /* shouldn't register twice! */
      if (unlikely (drivers[j].inuse == driver)) {
	printk ("%s: driver %p already registered\n", __FUNCTION__, driver);

	return -EINVAL;
      }

      if(thiz->first != 0 && thiz->last != 0) {
        /* range can not be within the range of a different driver */
        for (k = 0; k < STM_V4L2_MAX_DEVICES; ++k) {
          const struct stm_v4l2_driver_control_range * const other =
            &drivers[j].control_range[k];

          if (unlikely ((thiz->first >= other->first
                         && thiz->first <= other->last)
                        || (thiz->last >= other->first
                            && thiz->last <= other->last))) {
            printk ("%s: trying to register a region already occupied: "
                    "this/other: %.8x->%.8x vs. %.8x->%.8x\n", __FUNCTION__,
                    thiz->first, thiz->last,
                    other->first, other->last);

            return -EINVAL;
          }
        }
      }
    }
  }

  if (occupy == -1)
    return -EMFILE;

  drivers[occupy] = *driver;
  drivers[occupy].inuse = driver;
  for (i = 0; i < STM_V4L2_MAX_DEVICES; ++i) {
    drivers[occupy].index_offset[i] = start_indexes[driver->type][i];
    start_indexes[driver->type][i] += driver->n_indexes[i];
  }

  printk ("%s: driver '%s' registered\n", __FUNCTION__, drivers[occupy].name);

  return 0;
}

int
stm_v4l2_unregister_driver (const struct stm_v4l2_driver * const driver)
{
  int i;

  for (i = 0; i < STM_V4L2_MAX_DEVICES; ++i) {
    if (drivers[i].inuse != driver)
      continue;

    drivers[i].inuse = NULL;
    printk ("%s: driver '%s' unregistered\n", __FUNCTION__, drivers[i].name);
    /* FIXME: should do sth about start_indexes[] as well! */
  }

  return 0;
}

void
stm_v4l2_ctrl_query_fill (struct v4l2_queryctrl * const qctrl,
			  const char            * const name,
			  s32                    minimum,
			  s32                    maximum,
			  s32                    step,
			  s32                    default_value,
			  enum v4l2_ctrl_type    type,
			  u32                    flags)
{
  snprintf (qctrl->name, sizeof (qctrl->name), name);
  qctrl->minimum = minimum;
  qctrl->maximum = maximum;
  qctrl->step    = step;
  qctrl->default_value = default_value;
  qctrl->type    = type;
  qctrl->flags   = flags;

  qctrl->reserved[0] = qctrl->reserved[1] = 0;
}

EXPORT_SYMBOL (stm_v4l2_register_driver);
EXPORT_SYMBOL (stm_v4l2_unregister_driver);
EXPORT_SYMBOL (stm_v4l2_ctrl_query_fill);



/* Not used yet...., we certainly need to take the device look for allocating
   a handle, but that might be it */
//static struct semaphore devLock;



static int
stm_v4l2_open (struct inode *inode,
	       struct file  *file)
{
  unsigned int minor = iminor (inode);

  debug_msg ("%s in. minor = %d flags = 0x%08X\n", __FUNCTION__,
	     minor, file->f_flags);

  if (minor < 0 || minor >= MAX_DEVICES)
    return -ENODEV;

  file->private_data = NULL;

  debug_msg ("%s: out.\n", __FUNCTION__);

  return 0;
}


static int
stm_v4l2_close (struct inode *inode,
		struct file  *file)
{
  struct stm_v4l2_handles    * const handle = file->private_data;
  enum _stm_v4l2_driver_type  type;

  debug_msg ("%s in. minor = %d handle = %p\n", __FUNCTION__,
             iminor(inode), handle);

  if (handle) {
    for (type = 0; type < STM_V4L2_MAX_TYPES; ++type)
      if (handle->v4l2type[type].driver
	  && handle->v4l2type[type].driver->close) {
	int ret;
	ret = handle->v4l2type[type].driver->close (handle, type, file);
	if (ret == -ERESTARTSYS)
	  return -ERESTARTSYS;

	if (ret != 0)
	  printk ("%s: don't know how to handle result %d by type %d!\n",
		  __FUNCTION__, ret, type);
      }

      file->private_data = NULL;
      memset (handle, 0, sizeof (struct stm_v4l2_handles));
  }

  return 0;
}


static struct stm_v4l2_handles *
alloc_handle (void)
{
  int m;

  debug_msg ("%s: in.\n", __FUNCTION__);

  /* mutex handle lock */

  for (m = 0; m < MAX_HANDLES; ++m)
    if (!handles[m].used) {
      memset (&handles[m], 0, sizeof (struct stm_v4l2_handles));
      handles[m].used = 1;
      return &handles[m];
    }

  printk ("%s: Probably going to crash - fix me...\n", __FUNCTION__);

  return NULL;
}


static int
stm_v4l2_do_ioctl (struct inode *inode,
		   struct file  *file,
		   unsigned int  cmd,
		   void         *arg)
{
  struct stm_v4l2_handles *handle = file->private_data;
  unsigned int             minor  = iminor (inode);
  int ret = 0;
  int type = -1, index = -1;
  int n;
  int is_enum = false;

  switch (cmd) {
  case VIDIOC_RESERVED:         //_IO   ('V',  1)
  case VIDIOC_G_MPEGCOMP:       //_IOR  ('V',  6, struct v4l2_mpeg_compression)
  case VIDIOC_S_MPEGCOMP:       //_IOW  ('V',  7, struct v4l2_mpeg_compression)
  case VIDIOC_G_PRIORITY:       //_IOR  ('V', 67, enum v4l2_priority)
  case VIDIOC_S_PRIORITY:       //_IOW  ('V', 68, enum v4l2_priority)
  case VIDIOC_G_MODULATOR:      //_IOWR ('V', 54, struct v4l2_modulator)
  case VIDIOC_S_MODULATOR:      //_IOW  ('V', 55, struct v4l2_modulator)
  case VIDIOC_G_FREQUENCY:      //_IOWR ('V', 56, struct v4l2_frequency)
  case VIDIOC_S_FREQUENCY:      //_IOW  ('V', 57, struct v4l2_frequency)
  case VIDIOC_G_TUNER:          //_IOWR ('V', 29, struct v4l2_tuner)
  case VIDIOC_S_TUNER:          //_IOW  ('V', 30, struct v4l2_tuner)
  case VIDIOC_G_JPEGCOMP:       //_IOR  ('V', 61, struct v4l2_jpegcompression)
  case VIDIOC_S_JPEGCOMP:       //_IOW  ('V', 62, struct v4l2_jpegcompression)
  case VIDIOC_ENUM_FRAMESIZES:  //_IOWR ('V', 74, struct v4l2_frmsizeenum)
  case VIDIOC_ENUM_FRAMEINTERVALS: //_IOWR ('V', 75, struct v4l2_frmivalenum)
  case VIDIOC_G_ENC_INDEX:         //_IOR  ('V', 76, struct v4l2_enc_idx)
  case VIDIOC_ENCODER_CMD:         //_IOWR ('V', 77, struct v4l2_encoder_cmd)
  case VIDIOC_TRY_ENCODER_CMD:     //_IOWR ('V', 78, struct v4l2_encoder_cmd)
    debug_msg ("IOCTL %.8x is not implemented\n", cmd);
    ret = -EINVAL;
    break;

  default:
    debug_msg ("IOCTL %.8x is unknown\n", cmd);
    ret = -ENOIOCTLCMD;
    break;

  /* video ioctls */
  case VIDIOC_QUERYCAP:         //_IOR  ('V',  0, struct v4l2_capability)
    {
      struct v4l2_capability *b = arg;

      //debug_msg("v4l2_ioctl - VIDIOC_QUERYCAP\n");

      strcpy (b->driver, "stm_v4l2");
      strcpy (b->card, "STMicroelectronics CE Device");
      strcpy (b->bus_info,"");

      b->version      = LINUX_VERSION_CODE;
      b->capabilities = (0
			 | V4L2_CAP_VIDEO_OVERLAY
			 | V4L2_CAP_VIDEO_OUTPUT
			 | V4L2_CAP_STREAMING
			 | V4L2_CAP_SLICED_VBI_OUTPUT
			);
    }
    return 0;

  /* audio input ioctls */
  case VIDIOC_ENUMAUDIO:        //_IOWR ('V', 65, struct v4l2_audio)
    is_enum = true;
    /* fall through */
  case VIDIOC_G_AUDIO:          //_IOR  ('V', 33, struct v4l2_audio)
  case VIDIOC_S_AUDIO:          //_IOW  ('V', 34, struct v4l2_audio)
    if (arg) {
      const struct v4l2_audio * const audio = arg;
      type = STM_V4L2_AUDIO_INPUT;
      index = audio->index;
    } else ret = -EINVAL;
    break;

  /* audio output ioctls */
  case VIDIOC_ENUMAUDOUT:       //_IOWR ('V', 66, struct v4l2_audioout)
    is_enum = true;
    /* fall through */
  case VIDIOC_G_AUDOUT:         //_IOR  ('V', 49, struct v4l2_audioout)
  case VIDIOC_S_AUDOUT:         //_IOW  ('V', 50, struct v4l2_audioout)
    if (arg) {
      const struct v4l2_audio * const audio = arg;
      type = STM_V4L2_AUDIO_OUTPUT;
      index = audio->index;
    } else ret = -EINVAL;
    break;

  /* video input ioctls */
  case VIDIOC_ENUMINPUT:        //_IOWR ('V', 26, struct v4l2_input)
    if (arg) {
      const struct v4l2_input * const input = arg;
      type = STM_V4L2_VIDEO_INPUT;
      index = input->index;
      is_enum = true;
    } else ret = -EINVAL;
    break;

  case VIDIOC_G_INPUT:          //_IOR  ('V', 38, int)
  case VIDIOC_S_INPUT:          //_IOWR ('V', 39, int)
    if (arg) {
      const int * const input = arg;
      type = STM_V4L2_VIDEO_INPUT;
      index = *input;
    } else ret = -EINVAL;
    break;

  case VIDIOC_G_FBUF:           //_IOR  ('V', 10, struct v4l2_framebuffer)
  case VIDIOC_S_FBUF:           //_IOW  ('V', 11, struct v4l2_framebuffer)
  case VIDIOC_OVERLAY:          //_IOW  ('V', 14, int)
    type = STM_V4L2_VIDEO_INPUT;
    break;

  /* these are for inputs only, read the spec... */
  case VIDIOC_G_STD:            //_IOR  ('V', 23, v4l2_std_id)
  case VIDIOC_S_STD:            //_IOW  ('V', 24, v4l2_std_id)
  case VIDIOC_QUERYSTD:         //_IOR  ('V', 63, v4l2_std_id)
    type = STM_V4L2_VIDEO_INPUT;
    break;
  case VIDIOC_ENUMSTD:          //_IOWR ('V', 25, struct v4l2_standard)
    if (arg) {
      type = STM_V4L2_VIDEO_INPUT;
      is_enum = true;
    } else ret = -EINVAL;
    break;

  /* the input standard IOCTLs cloned for output */
  case VIDIOC_G_OUTPUT_STD:     //_IOR  ('V', BASE_VIDIOC_PRIVATE, v4l2_std_id)
  case VIDIOC_S_OUTPUT_STD:     //_IOW  ('V', BASE_VIDIOC_PRIVATE+1, v4l2_std_id)
    type = STM_V4L2_VIDEO_OUTPUT;
    break;

  case VIDIOC_ENUM_OUTPUT_STD:  //_IOWR ('V', BASE_VIDIOC_PRIVATE+2, struct v4l2_standard)
    if (arg) {
      type = STM_V4L2_VIDEO_OUTPUT;
      is_enum = true;
    } else ret = -EINVAL;
    break;

  /* video output ioctls */
  case VIDIOC_G_OUTPUT:         //_IOR  ('V', 46, int)
  case VIDIOC_S_OUTPUT:         //_IOWR ('V', 47, int)
    if (arg) {
      const int * const input = arg;
      type = STM_V4L2_VIDEO_OUTPUT;
      index = *input;
    } else ret = -EINVAL;
    break;

  case VIDIOC_ENUMOUTPUT:       //_IOWR ('V', 48, struct v4l2_output)
    if (arg) {
      const struct v4l2_output * const output = arg;
      type = STM_V4L2_VIDEO_OUTPUT;
      index = output->index;
      is_enum = true;
    } else ret = -EINVAL;
    break;

  case VIDIOC_G_FMT:            //_IOWR ('V',  4, struct v4l2_format)
  case VIDIOC_S_FMT:            //_IOWR ('V',  5, struct v4l2_format)
  case VIDIOC_TRY_FMT:          //_IOWR ('V', 64, struct v4l2_format)
    if (arg) {
      const struct v4l2_format * const format = arg;
      switch (format->type)
      {
        case V4L2_BUF_TYPE_VIDEO_OUTPUT:
          type =  STM_V4L2_VIDEO_OUTPUT;
          break;
        case V4L2_BUF_TYPE_VIDEO_CAPTURE:
        case V4L2_BUF_TYPE_VIDEO_OVERLAY:
          type = STM_V4L2_VIDEO_INPUT;
          break;
        case V4L2_BUF_TYPE_SLICED_VBI_OUTPUT:
          type = STM_V4L2_SLICED_VBI_OUTPUT;
          break;
        default:
          ret = -EINVAL;
      }
    } else ret = -EINVAL;
    break;

  /* this needs to be generic, and probably a way to allocate from video or
     system memory remember buffers can be allocated for input or output,
     for the moment we will pass to the driver */
  case VIDIOC_REQBUFS:          //_IOWR ('V',  8, struct v4l2_requestbuffers)
    if (arg) {
      const struct v4l2_requestbuffers * const rb = arg;
      switch (rb->type)
      {
        case V4L2_BUF_TYPE_VIDEO_OUTPUT:
          type =  STM_V4L2_VIDEO_OUTPUT;
          break;
        case V4L2_BUF_TYPE_VIDEO_CAPTURE:
          type = STM_V4L2_VIDEO_INPUT;
          break;
        case V4L2_BUF_TYPE_SLICED_VBI_OUTPUT:
          type = STM_V4L2_SLICED_VBI_OUTPUT;
          break;
        default:
          ret = -EINVAL;
      }
    } else ret = -EINVAL;
    break;

  case VIDIOC_QUERYBUF:         //_IOWR ('V',  9, struct v4l2_buffer)
  case VIDIOC_QBUF:             //_IOWR ('V', 15, struct v4l2_buffer)
  case VIDIOC_DQBUF:            //_IOWR ('V', 17, struct v4l2_buffer)
    if (arg) {
      const struct v4l2_buffer * const buffer = arg;
      switch (buffer->type)
      {
        case V4L2_BUF_TYPE_VIDEO_OUTPUT:
          type =  STM_V4L2_VIDEO_OUTPUT;
          break;
        case V4L2_BUF_TYPE_VIDEO_CAPTURE:
          type = STM_V4L2_VIDEO_INPUT;
          break;
        case V4L2_BUF_TYPE_SLICED_VBI_OUTPUT:
          type = STM_V4L2_SLICED_VBI_OUTPUT;
          break;
        default:
          ret = -EINVAL;
      }
    } else ret = -EINVAL;
    break;

  case VIDIOC_G_PARM:           //_IOWR ('V', 21, struct v4l2_streamparm)
  case VIDIOC_S_PARM:           //_IOWR ('V', 22, struct v4l2_streamparm)
    if (arg) {
      const struct v4l2_streamparm * const param = arg;
      switch (param->type)
      {
        case V4L2_BUF_TYPE_VIDEO_OUTPUT:
          type =  STM_V4L2_VIDEO_OUTPUT;
          break;
        case V4L2_BUF_TYPE_VIDEO_CAPTURE:
          type = STM_V4L2_VIDEO_INPUT;
          break;
        default:
          ret = -EINVAL;
      }
    } else ret = -EINVAL;
    break;

  case VIDIOC_CROPCAP:          //_IOWR ('V', 58, struct v4l2_cropcap)
    if (arg) {
      const struct v4l2_cropcap * const cropcap = arg;
      switch (cropcap->type)
      {
        case V4L2_BUF_TYPE_VIDEO_OUTPUT:
          type =  STM_V4L2_VIDEO_OUTPUT;
          break;
        case V4L2_BUF_TYPE_VIDEO_CAPTURE:
        case V4L2_BUF_TYPE_VIDEO_OVERLAY:
          type = STM_V4L2_VIDEO_INPUT;
          break;
        default:
          ret = -EINVAL;
      }
    } else ret = -EINVAL;
    break;

  case VIDIOC_G_CROP:           //_IOWR ('V', 59, struct v4l2_crop)
  case VIDIOC_S_CROP:           //_IOW  ('V', 60, struct v4l2_crop)
    if (arg) {
      const struct v4l2_crop * const crop = arg;
      switch (crop->type)
      {
        /* what should we do about BUF_TYPE_PRIVATE */
        case V4L2_BUF_TYPE_VIDEO_OUTPUT:
        case V4L2_BUF_TYPE_PRIVATE:
          type =  STM_V4L2_VIDEO_OUTPUT;
          break;
        case V4L2_BUF_TYPE_VIDEO_CAPTURE:
        case V4L2_BUF_TYPE_VIDEO_OVERLAY:
        case V4L2_BUF_TYPE_PRIVATE + 1:
          type = STM_V4L2_VIDEO_INPUT;
          break;
        default:
          ret = -EINVAL;
      }
    } else ret = -EINVAL;
    break;

  /* can be input or output but why not audio????? */
  case VIDIOC_STREAMON:         //_IOW  ('V', 18, int)
  case VIDIOC_STREAMOFF:        //_IOW  ('V', 19, int)
    if (arg) {
      const int * const v4ltype = arg;
      switch (*v4ltype)
      {
        case V4L2_BUF_TYPE_VIDEO_OUTPUT:
          type =  STM_V4L2_VIDEO_OUTPUT;
          break;
        case V4L2_BUF_TYPE_VIDEO_CAPTURE:
          type = STM_V4L2_VIDEO_INPUT;
          break;
        case V4L2_BUF_TYPE_SLICED_VBI_OUTPUT:
          type = STM_V4L2_SLICED_VBI_OUTPUT;
          break;
        default:
          ret = -EINVAL;
      }
    } else ret = -EINVAL;
    break;

  /* Driver independent
     however we need to get the correct ones */
  case VIDIOC_ENUM_FMT:         //_IOWR ('V',  2, struct v4l2_fmtdesc)
    if (arg) {
      const struct v4l2_fmtdesc * const fmt = arg;
      switch (fmt->type)
      {
        case V4L2_BUF_TYPE_VIDEO_OUTPUT:
          type =  STM_V4L2_VIDEO_OUTPUT;
          break;
        case V4L2_BUF_TYPE_VIDEO_CAPTURE:
          type = STM_V4L2_VIDEO_INPUT;
          break;
        default:
          ret = -EINVAL;
      }
    } else ret = -EINVAL;
    break;

  /* control ioctls */
  case VIDIOC_G_CTRL:           //_IOWR ('V', 27, struct v4l2_control)
  case VIDIOC_S_CTRL:           //_IOWR ('V', 28, struct v4l2_control)
  case VIDIOC_QUERYCTRL:        //_IOWR ('V', 36, struct v4l2_queryctrl)
  case VIDIOC_QUERYMENU:        //_IOWR ('V', 37, struct v4l2_querymenu)
    if (arg) {
      /* cheat a little we know id is always first one */
      const struct v4l2_control * const control = arg;
      index = control->id;
      type = STM_V4L2_MAX_TYPES;
    } else ret = -EINVAL;
    break;

  case VIDIOC_G_EXT_CTRLS:
  case VIDIOC_S_EXT_CTRLS:
  case VIDIOC_TRY_EXT_CTRLS:
    if (arg) {
      struct v4l2_ext_controls * const ctrls = arg;
      index = V4L2_CTRL_ID2CLASS (ctrls->ctrl_class) | V4L2_CTRL_FLAG_NEXT_CTRL;
      type = STM_V4L2_MAX_TYPES;
    } else ret = -EINVAL;
    break;

  case VIDIOC_G_SLICED_VBI_CAP: //_IOR  ('V', 69, struct v4l2_sliced_vbi_cap)
    if (arg) {
      struct v4l2_sliced_vbi_cap * const cap = arg;
      if(cap->type == V4L2_BUF_TYPE_SLICED_VBI_OUTPUT)
        type = STM_V4L2_SLICED_VBI_OUTPUT;
      else
        ret = -EINVAL;
    } else ret = -EINVAL;
    break;

  case VIDIOC_S_OUTPUT_ALPHA: //_IOW  ('V', BASE_VIDIOC_PRIVATE+3, unsigned int)
    type = STM_V4L2_VIDEO_OUTPUT;
    break;
  };

  /* Cool now we have all the information we need. */

  if (ret)
    return ret;

  /* If it is a control let's deal with that */
  if (type == STM_V4L2_MAX_TYPES) {
    for (n = 0; n < MAX_DRIVERS; ++n) {
      if (!drivers[n].inuse)
	continue;

      if ((drivers[n].control_classes
	   && v4l2_ctrl_next (drivers[n].control_classes, index))
	  || (index >= drivers[n].control_range[minor].first
	      && index <= (drivers[n].control_range[minor].last))) {
	ret = drivers[n].ioctl (handle, &drivers[n], minor,
				drivers[n].type, file, cmd, arg);

	return ret;
      }
    }
    return -EINVAL;
  }


  /* if no handle, then we need to get one or there would be no handle for a
     specified type */
  if (!handle
      || (type != -1 && !handle->v4l2type[type].driver)
      || is_enum) {
    if (!handle)
      handle = alloc_handle ();

    ret = -EINVAL;
    for (n = 0; n < MAX_DRIVERS; ++n) {
      if (!drivers[n].inuse)
	continue;

      if (type == -1 || type == drivers[n].type) {
	ret = drivers[n].ioctl (handle,
				&drivers[n], minor, drivers[n].type,
				file, cmd, arg);
	if (handle->v4l2type[drivers[n].type].handle && !is_enum) {
	  if (ret) {
	    printk ("%s: BUG ON... can't set a handle and return an error\n",
		    __FUNCTION__);
	    BUG ();
	  }
	  handle->v4l2type[drivers[n].type].driver = &drivers[n];
	  handle->device = minor;
	  printk ("%s: Assigning handle %p to device %d file %p\n",
		  __FUNCTION__, handle, minor, file);
	  file->private_data = handle;
	  return ret;
	}

        /* Not sure if this is safe but if the driver didn't return a
           problem then assume it was an enum or something that didn't need
           to be associated (allocated is too strong a word) */
	if (!ret) {
	  if (!file->private_data)
	    memset (handle, 0, sizeof (struct stm_v4l2_handles));
	  return ret;
	}
      }
    }

    /* Nothing worked so clear the handle and go again */
    if (!file->private_data)
      memset (handle, 0, sizeof (struct stm_v4l2_handles));
    return ret;
  }

  /* Ok if type != -1 and handle[type].driver->ioctl and handle[type].handle,
     just call it */
  if (type != -1) {
    if (handle->v4l2type[type].driver)
      return handle->v4l2type[type].driver->ioctl (handle,
						   handle->v4l2type[type].driver,
						   minor, type, file, cmd, arg);
    else
      return -EINVAL;
  }

  for (n = 0; n < STM_V4L2_MAX_TYPES; ++n)
    if (handle->v4l2type[n].driver)
      if (!handle->v4l2type[n].driver->ioctl (handle,
					      handle->v4l2type[n].driver,
					      minor, n, file, cmd, arg))
	return 0;

  return -EINVAL;
}


static int
stm_v4l2_ioctl (struct inode  *inode,
		struct file   *file,
		unsigned int   cmd,
		unsigned long  arg)
{
  return video_usercopy (inode, file, cmd, arg, stm_v4l2_do_ioctl);
}


static unsigned int
stm_v4l2_poll (struct file *file,
	       poll_table  *table)
{
  struct stm_v4l2_handles *handle = file->private_data;
  int ret = 0;
  int n;

  if (handle)
    for (n = 0; n < STM_V4L2_MAX_TYPES; ++n)
      if (handle->v4l2type[n].driver
	  && handle->v4l2type[n].driver->poll)
        /* FIXME: this overwrites the return value */
	ret = handle->v4l2type[n].driver->poll (handle, n, file, table);

  return ret;
}


static int
stm_v4l2_mmap (struct file           *file,
	       struct vm_area_struct *vma)
{
  struct stm_v4l2_handles *handle = file->private_data;
  int ret = -ENODEV;
  int n;

  if (handle)
    for (n = 0; n < STM_V4L2_MAX_TYPES; ++n)
      if (handle->v4l2type[n].driver
	  && handle->v4l2type[n].handle
	  && handle->v4l2type[n].driver->mmap)
	if (!(ret = handle->v4l2type[n].driver->mmap (handle, n, file, vma)))
	  return 0;

  return ret;
}


static void
stm_v4l2_vdev_release (struct video_device *vdev)
{
  /*
   * Nothing to do as the video_device is not dynamically allocated,
   * but the V4L framework complains if this method is not present.
   */
}


static struct file_operations stm_v4l2_fops = {
  .owner   = THIS_MODULE,
  .open    = stm_v4l2_open,
  .release = stm_v4l2_close,
  .ioctl   = stm_v4l2_ioctl,
  .llseek  = no_llseek,
  .read    = NULL,
  .write   = NULL,
  .mmap    = stm_v4l2_mmap,
  .poll    = stm_v4l2_poll,
};

struct video_device v4l2_template __devinitdata = {
  .type     = (VID_TYPE_OVERLAY   | VID_TYPE_SCALES),
  .type2    = (V4L2_CAP_STREAMING | V4L2_CAP_VIDEO_OUTPUT),
  .hardware = VID_HARDWARE_STMVOUT,
  .fops     = &stm_v4l2_fops,
  .release  = stm_v4l2_vdev_release,
  .minor    = -1
};

static int video_nr = -1;
module_param(video_nr, int, 0444);


static void
stm_v4l2_exit (void)
{
  int n;

  for (n = 0; n < MAX_DEVICES; ++n)
    video_unregister_device (&devices[n]);
}


static int __init
stm_v4l2_init (void)
{
  int n;
  int ret;

  for (n = 0; n < MAX_DEVICES; ++n) {
    memcpy (&devices[n], &v4l2_template, sizeof (struct video_device));
    snprintf (devices[n].name, sizeof (devices[n].name),
	      "pipeline-%d", n);
    ret = video_register_device (&devices[n], VFL_TYPE_GRABBER, n);
    if (unlikely (ret < 0))
      goto err;
  }

  return 0;

err:
  while (--n > 0)
    video_unregister_device (&devices[n]);

  return ret;
}

module_init (stm_v4l2_init);
module_exit (stm_v4l2_exit);

MODULE_LICENSE ("GPL");
