/***********************************************************************
 *
 * File: linux/drivers/media/video/stm_v4l2.h
 * Copyright (c) 2007,2009 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 * V4L2 generic video driver
 *
\***********************************************************************/

#ifndef __STM_V4L2_H__
#define __STM_V4L2_H__

#include <linux/videodev.h>

// A single file handle can have more than one active type
// however audio and video can not be done together :-( limitation
// of API
enum _stm_v4l2_driver_type {
  STM_V4L2_VIDEO_OUTPUT,
  STM_V4L2_VIDEO_INPUT,
  STM_V4L2_AUDIO_OUTPUT,
  STM_V4L2_AUDIO_INPUT,
  STM_V4L2_SLICED_VBI_OUTPUT,

  STM_V4L2_MAX_TYPES
};

#define STM_V4L2_MAX_DEVICES   3

#define STM_V4L2_MAX_CONTROLS  255

struct stm_v4l2_driver;

struct stm_v4l2_handles {
  int used;
  int mask;

  // is this needed, anyway its to identify /dev/video[0|1|2]
  int device;

  struct {
    struct stm_v4l2_driver *driver;
    void                   *handle;
  } v4l2type[STM_V4L2_MAX_TYPES];
};

struct stm_v4l2_driver_control_range {
  unsigned int first;
  unsigned int last;
};

struct stm_v4l2_driver {
  // Driver should fill in these before registering the driver
  char               name[32];            //!< Name of the driver
  enum _stm_v4l2_driver_type type;        //!< Type of buffer, a driver can register as many as they like but only one type at a time.
  unsigned int       capabilities;        //!< Capabilities this provides (they get ored together to produce the final capailities
  struct stm_v4l2_driver_control_range control_range[STM_V4L2_MAX_DEVICES]; //!< range of controls this device supports
  int                n_indexes[STM_V4L2_MAX_DEVICES]; //!< number of devices
  void              *priv;                //!< Private Data for driver

  const u32 * const * control_classes;

  // Operations the driver should register with.
  // Only ioctl is mandatory, as the minimum you can implement
  // is a single control.

  // Once handle has been filled in via ioctl then the driver has claimed it for it's use.
  int          (*ioctl)       (struct stm_v4l2_handles    *handle,
			       struct stm_v4l2_driver     *driver,
			       int                         device,
			       enum _stm_v4l2_driver_type  type,
			       struct file                *file,
			       unsigned int                cmd,
			       void                       *arg);
  int          (*close)       (struct stm_v4l2_handles    *handle,
			       enum _stm_v4l2_driver_type  type,
			       struct file                *file);
  unsigned int (*poll)        (struct stm_v4l2_handles    *handle,
			       enum _stm_v4l2_driver_type  type,
			       struct file                *file,
			       poll_table                 *table);
  int          (*mmap)        (struct stm_v4l2_handles    *handle,
			       enum _stm_v4l2_driver_type  type,
			       struct file                *file,
			       struct vm_area_struct      *vma);
//  int          (*munmap)      (struct stm_v4l2_driver *driver,
//			       void                   *handle,
//			       void                   *start,
//			       size_t                  length);

  int index_offset[STM_V4L2_MAX_DEVICES]; //!< offset to subtract from device index passed in, drivers need to handle this!

  /* private for use by stm_v4l2.o */
  const void *inuse;
};


// Registration functions
extern int stm_v4l2_register_driver(const struct stm_v4l2_driver * const driver);
extern int stm_v4l2_unregister_driver(const struct stm_v4l2_driver * const driver);

extern void stm_v4l2_ctrl_query_fill (struct v4l2_queryctrl * const qctrl,
				      const char            * const name,
				      s32                    minimum,
				      s32                    maximum,
				      s32                    step,
				      s32                    default_value,
				      enum v4l2_ctrl_type    type,
				      u32                    flags);



#endif /* __STM_V4L2_H__ */
