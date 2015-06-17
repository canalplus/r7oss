/************************************************************************
Copyright (C) 2014 STMicroelectronics. All Rights Reserved.

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
 * stm subdev open/close helper functions
************************************************************************/
#ifndef __STM_V4L2_HELPER_H__
#define __STM_V4L2_HELPER_H__

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <mediactl/mediactl.h>
#include <mediactl/v4l2subdev.h>

/**
 * stm_v4l2_media_open() - opens the media controller device
 */
static int __attribute__((unused))
stm_v4l2_media_open(int id, struct media_device **media)
{
	int ret = 0;

	/*
	 * Open the media device
	 */
	*media = media_open("/dev/media0");
	if (!*media) {
		printf("%s(): No media device to open\n", __func__);
		ret = -ENODEV;
	}

	return ret;
}

static void  __attribute__((unused))
stm_v4l2_media_close(struct media_device *media)
{
	media_close(media);
}

/**
 * stm_v4l2_subdev_open() - finds entity and open the corresponding subdev
 */
static int stm_v4l2_subdev_open(struct media_device *media,
			char *entity_name, struct media_entity **entity)
{
	int ret = 0;

	/*
	 * Get the requested entity
	 */
	if (!*entity) {
		*entity = media_get_entity_by_name(media,
					entity_name, strlen(entity_name));
		if (!*entity) {
			printf("%s(): No %s entity found\n", __func__, entity_name);
			ret = -ENODEV;
			goto entity_search_failed;
		}
	}

	/*
	 * Open the corresponding subdev
	 */
	if (!(*entity)->fd || ((*entity)->fd < 0)) {

		ret = v4l2_subdev_open(*entity);
		if (ret) {
			printf("%s(): failed to open %s subdevice\n", __func__, entity_name);
			goto subdev_open_failed;
		}
	}

	return 0;

subdev_open_failed:
	*entity = NULL;
entity_search_failed:
	return ret;
}

/**
 * stm_v4l2_subdev_close() - close the subdev and reset the entity
 */
static void stm_v4l2_subdev_close(struct media_entity *entity)
{
	if (!entity)
		goto close_done;

	v4l2_subdev_close(entity);

close_done:
	return;
}
#endif
