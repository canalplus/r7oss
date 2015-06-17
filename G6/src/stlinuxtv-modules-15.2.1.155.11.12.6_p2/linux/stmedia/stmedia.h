/************************************************************************
Copyright (C) 2012 STMicroelectronics. All Rights Reserved.

This file is part of the STLinuxTV Library.

STLinuxTV is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

STLinuxTV is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with player2; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The STLinuxTV Library may alternatively be licensed under a proprietary
license from ST.
 * Implementation of common V4L2-MediaController registration functions
************************************************************************/

#ifndef __STMEDIA_H__
#define __STMEDIA_H__

#ifndef ENABLE_ZERO_COPY_TRACE
#define ENABLE_ZERO_COPY_TRACE                0
#endif

#define _STRACE(fmt, args...)   \
		((void)(ENABLE_ZERO_COPY_TRACE && \
			(printk(KERN_INFO "%s: " fmt, __func__, ##args), 0)))

#include <media/v4l2-dev.h>	/* Should be removed */
#include <media/v4l2-common.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ctrls.h>
#include <stm_common.h>

/*
 * Structure used to be able to "share" STKPI object between our V4L2 entities
 */
struct stmedia_v4l2_subdev {
	struct v4l2_subdev sdev;	/* v4l2-sub-device */
	stm_object_h stm_obj;
};

#define entity_to_stmedia_v4l2_subdev(_e) \
	container_of(media_entity_to_v4l2_subdev(_e), \
			struct stmedia_v4l2_subdev, sdev)

int stm_media_register_v4l2_subdev(struct v4l2_subdev *sd);
int stm_media_register_v4l2_subdev_nodes(void);
void stm_media_unregister_v4l2_subdev(struct v4l2_subdev *sd);
int stm_media_register_v4l2_video_device(struct video_device *viddev, int type,
					 int nr);
void stm_media_unregister_v4l2_video_device(struct video_device *viddev);
int stm_media_register_entity(struct media_entity *entity);
void stm_media_unregister_entity(struct media_entity *entity);
struct media_entity *stm_media_find_entity_with_type_first(unsigned int type);
struct media_entity *stm_media_find_entity_with_type_next(struct media_entity
							  *cur,
							  unsigned int type);
struct media_pad *stm_media_find_remote_pad_with_type
				(const struct media_pad *pad,
				unsigned int state,
				unsigned long entity_type,
				unsigned int *id);
struct media_pad *stm_media_find_remote_pad(const struct media_pad *pad,
					    unsigned int state,
					    unsigned int *id);
struct v4l2_subdev *stm_media_remote_v4l2_subdev(const struct media_pad *local);

#endif
