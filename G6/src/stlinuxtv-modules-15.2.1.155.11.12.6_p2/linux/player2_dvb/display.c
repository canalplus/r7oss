/************************************************************************
Copyright (C) 2011 STMicroelectronics. All Rights Reserved.

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

Source file name : display.c
Author :           Julian

Access to all platform specific display information etc

Date        Modification                                    Name
----        ------------                                    --------
05-Apr-07   Created                                         Julian

************************************************************************/

#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/errno.h>
#include <include/stm_display.h>
#include <linux/stm/stmcoredisplay.h>
#include <linux/videodev2.h>
#include "dvb_v4l2_export.h"
#include "dvb_module.h"
#include "stmedia_export.h"
#include "stmvout_mc.h"
#include "stmvout_driver.h"
#include "display.h"

static struct v4l2_subdev * get_remote_plane_subdev(struct media_pad *pad)
{
	struct media_pad *remote_pad;
	struct media_entity *entity;
	int id = 0;

	/* Get the remote entity */
	while (1) {
		remote_pad =
		    stm_media_find_remote_pad(pad, MEDIA_LNK_FL_ENABLED, &id);
		if (!remote_pad) {
			printk(KERN_INFO
			       "%s: no plane connected at this decoder\n",
			       __func__);
			return NULL;
		}
		entity = remote_pad->entity;

		if (entity->type == MEDIA_ENT_T_V4L2_SUBDEV_PLANE_VIDEO)
			break;
	}

	return media_entity_to_v4l2_subdev(entity);
}

int dvb_stm_display_set_input_window_mode(struct VideoDeviceContext_s *Context,
						unsigned int Mode)
{
	struct media_pad *pad = &Context->mc_video_pad;
	struct v4l2_subdev *sd;
	struct v4l2_control ctrl;

	sd = get_remote_plane_subdev(pad);
	if (!sd)
		return 0;

	ctrl.id = V4L2_CID_STM_INPUT_WINDOW_MODE;
	ctrl.value = Mode;

	return v4l2_subdev_call(sd, core, s_ctrl, &ctrl);
}

int dvb_stm_display_get_input_window_mode(struct VideoDeviceContext_s *Context,
						unsigned int *Mode)
{
	struct media_pad *pad = &Context->mc_video_pad;
	struct v4l2_subdev *sd;
	struct v4l2_control ctrl;
	int ret;

	sd = get_remote_plane_subdev(pad);
	if (!sd)
		return 0;

	ctrl.id = V4L2_CID_STM_INPUT_WINDOW_MODE;

	ret = v4l2_subdev_call(sd, core, g_ctrl, &ctrl);
	if (ret)
		return ret;

	*Mode = ctrl.value;
	return 0;
}

int dvb_stm_display_set_output_window_mode(struct VideoDeviceContext_s *Context,
						unsigned int Mode)
{
	struct media_pad *pad = &Context->mc_video_pad;
	struct v4l2_subdev *sd;
	struct v4l2_control ctrl;

	sd = get_remote_plane_subdev(pad);
	if (!sd)
		return 0;

	ctrl.id = V4L2_CID_STM_OUTPUT_WINDOW_MODE;
	ctrl.value = Mode;

	return v4l2_subdev_call(sd, core, s_ctrl, &ctrl);
}

int dvb_stm_display_get_output_window_mode(struct VideoDeviceContext_s *Context,
						unsigned int *Mode)
{
	struct media_pad *pad = &Context->mc_video_pad;
	struct v4l2_subdev *sd;
	struct v4l2_control ctrl;
	int ret;

	sd = get_remote_plane_subdev(pad);
	if (!sd)
		return 0;

	ctrl.id = V4L2_CID_STM_OUTPUT_WINDOW_MODE;

	ret = v4l2_subdev_call(sd, core, g_ctrl, &ctrl);
	if (ret)
		return ret;

	*Mode = ctrl.value;
	return 0;
}

int dvb_stm_display_get_input_window_value(struct VideoDeviceContext_s *Context,
				     unsigned int *X, unsigned int *Y,
				     unsigned int *Width, unsigned int *Height)
{
	struct media_pad *pad = &Context->mc_video_pad;
	struct v4l2_subdev *sd;
	struct v4l2_subdev_crop crop;
	int ret;

	sd = get_remote_plane_subdev(pad);
	if (!sd)
		return 0;

	crop.pad = 0;

	ret = v4l2_subdev_call(sd, pad, get_crop, NULL, &crop);
	if (ret)
		return ret;

	*X = crop.rect.left;
	*Y = crop.rect.top;
	*Width = crop.rect.width;
	*Height = crop.rect.height;

	return 0;
}

int dvb_stm_display_get_output_window_value(struct VideoDeviceContext_s *Context,
				      unsigned int *X, unsigned int *Y,
				      unsigned int *Width, unsigned int *Height)
{
	struct media_pad *pad = &Context->mc_video_pad;
	struct v4l2_subdev *sd;
	struct v4l2_subdev_crop crop;
	int ret;

	sd = get_remote_plane_subdev(pad);
	if (!sd)
		return 0;

	crop.pad = 1;

	ret = v4l2_subdev_call(sd, pad, get_crop, NULL, &crop);
	if (ret)
		return ret;

	*X = crop.rect.left;
	*Y = crop.rect.top;
	*Width = crop.rect.width;
	*Height = crop.rect.height;

	return 0;
}

int dvb_stm_display_set_input_window_value(struct VideoDeviceContext_s *Context,
				     unsigned int X, unsigned int Y,
				     unsigned int Width, unsigned int Height)
{
	struct media_pad *pad = &Context->mc_video_pad;
	struct v4l2_subdev *sd;
	struct v4l2_subdev_crop crop;
	int ret;

	sd = get_remote_plane_subdev(pad);
	if (!sd)
		return 0;

	crop.pad = 0;
	crop.rect.left = X;
	crop.rect.top = Y;
	crop.rect.width = Width;
	crop.rect.height = Height;

	ret = v4l2_subdev_call(sd, pad, set_crop, NULL, &crop);
	if (ret)
		return ret;

	return 0;
}

int dvb_stm_display_set_output_window_value(struct VideoDeviceContext_s *Context,
				      unsigned int X, unsigned int Y,
				      unsigned int Width, unsigned int Height)
{
	struct media_pad *pad = &Context->mc_video_pad;
	struct v4l2_subdev *sd;
	struct v4l2_subdev_crop crop;
	int ret;

	sd = get_remote_plane_subdev(pad);
	if (!sd)
		return 0;

	crop.which = V4L2_SUBDEV_FORMAT_ACTIVE;
	crop.pad = 1;
	crop.rect.left = X;
	crop.rect.top = Y;
	crop.rect.width = Width;
	crop.rect.height = Height;

	ret = v4l2_subdev_call(sd, pad, set_crop, NULL, &crop);
	if (ret)
		return ret;

	return 0;
}

int dvb_stm_display_set_aspect_ratio_conversion_mode(struct VideoDeviceContext_s
						     *Context,
						     unsigned int
						     AspectRatioConversionMode)
{
	struct media_pad *pad = &Context->mc_video_pad;
	struct v4l2_subdev *sd;
	struct v4l2_control ctrl;

	sd = get_remote_plane_subdev(pad);
	if (!sd)
		return 0;

	ctrl.id = V4L2_CID_STM_ASPECT_RATIO_CONV_MODE;
	ctrl.value = AspectRatioConversionMode;

	return v4l2_subdev_call(sd, core, s_ctrl, &ctrl);
}

int dvb_stm_display_get_aspect_ratio_conversion_mode(struct VideoDeviceContext_s
						     *Context,
						     unsigned int
						     *AspectRatioConversionMode)
{
	struct media_pad *pad = &Context->mc_video_pad;
	struct v4l2_subdev *sd;
	struct v4l2_control ctrl;
	int ret;

	sd = get_remote_plane_subdev(pad);
	if (!sd)
		return 0;

	ctrl.id = V4L2_CID_STM_ASPECT_RATIO_CONV_MODE;

	ret = v4l2_subdev_call(sd, core, g_ctrl, &ctrl);
	if (ret)
		return ret;

	*AspectRatioConversionMode = ctrl.value;

	return 0;
}

int dvb_stm_display_set_output_display_aspect_ratio(struct VideoDeviceContext_s
						    *Context,
						    unsigned int
						    OutputDisplayAspectRatio)
{
	struct media_pad *pad = &Context->mc_video_pad;
	struct v4l2_subdev *sd;
	struct v4l2_control ctrl;

	sd = get_remote_plane_subdev(pad);
	if (!sd)
		return 0;

	ctrl.id = V4L2_CID_DV_STM_TX_ASPECT_RATIO;
	ctrl.value = OutputDisplayAspectRatio;

	return v4l2_subdev_call(sd, core, s_ctrl, &ctrl);
}

int dvb_stm_display_get_output_display_aspect_ratio(struct VideoDeviceContext_s
						    *Context,
						    unsigned int
						    *OutputDisplayAspectRatio)
{
	struct media_pad *pad = &Context->mc_video_pad;
	struct v4l2_subdev *sd;
	struct v4l2_control ctrl;
	int ret;

	sd = get_remote_plane_subdev(pad);
	if (!sd)
		return 0;

	ctrl.id = V4L2_CID_DV_STM_TX_ASPECT_RATIO;

	ret = v4l2_subdev_call(sd, core, g_ctrl, &ctrl);
	if (ret)
		return ret;

	*OutputDisplayAspectRatio = ctrl.value;
	return 0;
}

/*}}}*/
