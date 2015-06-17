/************************************************************************
Copyright (C) 2013 STMicroelectronics. All Rights Reserved.

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
 * Implementation of v4l2 VXI control device
 * First initial driver by Akram Ben Belgacem
************************************************************************/
#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/slab.h>
#include <media/v4l2-subdev.h>


#include "stmedia.h"
#include "linux/stm/stmedia_export.h"
#include "linux/dvb/dvb_v4l2_export.h"
#include "stm_vxi.h"

/* The VXI entity pad info */
#define VXI_SNK_PAD 0
#define VXI_SRC_PAD 1
#define VXI_PAD_NB  2

/* The VXI source type value */
#define STM_SOURCE_TYPE_VXI	2

/* *** HDMI/CVBS input - PAL  -  720 x  288 x 50I *** */
#define VXI_DV_TIMINGS_FROM_PAL_INPUT_TIMING       \
	( {                                                      \
		dv_timings->type = V4L2_DV_BT_656_1120;\
		dv_timings->bt.interlaced = V4L2_DV_INTERLACED;\
		dv_timings->bt.width = 720;\
		dv_timings->bt.height = 288;\
		dv_timings->bt.hfrontporch = 136;\
		dv_timings->bt.vfrontporch = 23;\
		dv_timings->bt.pixelclock = 864 * 312 * 50;\
		dv_timings->bt.hsync = 8;\
		dv_timings->bt.vsync = 1;\
	} )

#define VXI_MBUS_FMT_FROM_PAL       \
	( {                                                      \
		fmt->width = 720;\
		fmt->height = 288;\
		fmt->code = V4L2_MBUS_FMT_YUYV8_2X8;\
		fmt->field = V4L2_FIELD_INTERLACED;\
		fmt->colorspace = V4L2_COLORSPACE_SMPTE170M;\
	} )

#define _warn(a,b...)  printk(KERN_WARNING "Warning:<%s:%d>"  a, __FUNCTION__ ,__LINE__,##b)
#define _info(a,b...)  printk(KERN_INFO "Info:<%s:%d>"  a, __FUNCTION__ ,__LINE__,##b)
#define _err(a,b...)  printk(KERN_ERR "Err:<%s:%d>"  a, __FUNCTION__ ,__LINE__,##b)
#define _dbg(a,b...)  printk(KERN_DEBUG "Debug:<%s:%d>"  a, __FUNCTION__ ,__LINE__,##b)


/** VXI Context structure
 *  There will as many instances as the VXI inputs in the system.
 */
struct vxi_ctx_s {
	/*! VXI device Handle. This handle has to be allocated during the module init.*/
	stm_vxi_h vxidev_h;
	/*!The shared vxi device structure */
	struct vxiinput_shared_s *vxidev;
	/*!The v4l2 sub-device structure. This will be initialized
	when  the v4l subdevice for VXI input is created*/
	struct v4l2_subdev vxi_sd;
	/*! Stores the pad info. For the vxi there will 1 sink
	pad and 1 source pad (Only Video).*/
	struct media_pad pads[VXI_PAD_NB];
	/*! the instance number of the Object. Each Instance
	will represent a VXI hardware IP present on the platform.*/
	int vxi_inst;
	/* FIXME we dont really need to cache values here. Once stmvout implements the
	  "Get" versions of the ioctl, we can remove these variables */
	/*! store the timings information */
	struct v4l2_dv_timings dv_timings;
	/*! store the media bus format */
	struct v4l2_mbus_framefmt fmt;
};

/** VXI inputs context structure.*/
struct vidextin_vxi_ctx_s {
	/*! The shared vxi device structure */
	struct vxiinput_shared_s *vxidev;
	/*! The v4l2 sub-device structure.This will be initialized
	when the v4l subdev for VidExtin_Vxi route is created. */
	struct v4l2_subdev v4l2_vidext_sd;
	/*!Stores the pad info. This will created accoridng to the
	number of INPUTS on the platform.*/
	struct media_pad *input_pad;
	/*! the instance number of the Object. Each Instance
	will represent a VXI input present on the platform.*/
	int vidextin_inst;
};

/** This is a singular instance object and it will be created during the driver init.*/
struct vxiinput_shared_s {
	/*! This will point to the array of vxi_ctx_s pointers which will
	be created according to the number of available instance of vxi hw. */
	struct vxi_ctx_s *vxi;
	int nb_vxi; /*!< number of vxi_ctx_s instance created.*/
	/*! This will point to the array of vidextin_vxi_ctx_s pointers which will
	be created according to the number of inputs. */
	struct vidextin_vxi_ctx_s *vidextin_vxi; /*!< vidextin_vxi_ctx_s  instance. */
	int nb_vidextin_vxi; /*!< number of vidextin_vxi_ctx_s instance created.*/
};

struct vxiinput_shared_s vxiinput_shared;

/** init_vxi_timing_mbus_fmt() - init timing info and mbus_fmt
 * @pre The system should be initialized.
 * @param[out] *dv_timings The dv timing structure.
 * @param[out] *fmt The mbus fmt structure.
 * @return 0 if mappping successful.
 *     -EINVAL if error.
 */
static int init_vxi_timing_mbus_fmt(struct v4l2_dv_timings *dv_timings,
				struct v4l2_mbus_framefmt *fmt)
{
	memset(dv_timings, 0, sizeof(*dv_timings));
	memset(fmt, 0, sizeof(*fmt));

	VXI_DV_TIMINGS_FROM_PAL_INPUT_TIMING;

	VXI_MBUS_FMT_FROM_PAL;

	return 0;
}

/** vxi_set_interface_params() -This fumction program interface parameters for PAL
 * standard.
 *
 * @pre The video plane should be connected to the video source
 * @param[in] *vxictx The pointer to the VXI route ctx
 * @param[in] *input_pad The input pad information.
 * @return TRUE if connection of route to input is validated.
 * FALSE if connection to input is not valid.
 */
static int vxi_set_interface_params(struct vxi_ctx_s *vxictx,
				struct v4l2_subdev *plane_sd)
{
	int ret = 0;
	struct v4l2_control ctrl;

	_dbg("***********Interface Params************/n");
	_dbg("HACTIVE :%d ", vxictx->dv_timings.bt.width);
	_dbg("VACTIVE :%d ", vxictx->dv_timings.bt.height);
	_dbg("*************************************");

	ret =
	    v4l2_subdev_call(plane_sd, video, s_dv_timings,
			     &vxictx->dv_timings);
	if (ret < 0)
		return ret;

	ret = v4l2_subdev_call(plane_sd, video, s_mbus_fmt, &vxictx->fmt);
	if (ret < 0)
		return ret;

	/*Assuming Timing is stable now */
	ctrl.id = V4L2_CID_DV_STM_RX_PIXEL_TIMING_STABILITY;
	ctrl.value = VCSPIXEL_TIMING_STABLE;
	ret = v4l2_subdev_call(plane_sd, core, s_ctrl, &ctrl);
	return ret;
}

/** vxi_video_s_dv_timings() -to set the dvtimings
 *
 * @param[in] *sd The pointer to the VXI route subdevice
 * @param[in] *timings The dv_timings structure
 */
static int vxi_video_s_dv_timings(struct v4l2_subdev *sd,
			struct v4l2_dv_timings *timings)
{
	struct vxi_ctx_s *vxictx = v4l2_get_subdevdata(sd);
	struct media_pad *plane_pad;
	int id = 0;

	/* Do we need to do a sanity test of the timings ? */
	memcpy(&vxictx->dv_timings, timings, sizeof(struct v4l2_dv_timings));


	/* We need to get the connected input to route */
	plane_pad = stm_media_find_remote_pad(&vxictx->pads[VXI_SRC_PAD],
					    MEDIA_LNK_FL_ENABLED, &id);
	if (!plane_pad) {
		_info(" VXI route not connected to plane yet ");
		return 0;
	}

	return v4l2_subdev_call(media_entity_to_v4l2_subdev(plane_pad->entity),
				video, s_dv_timings, &vxictx->dv_timings);
}

/** vxi_video_g_dv_timings() -to get the dvtimings
 *
 * @param[in] *sd The pointer to the VXI route subdevice
 * @param[out] *timings The dv_timings structure
 */
static int vxi_video_g_dv_timings(struct v4l2_subdev *sd,
			struct v4l2_dv_timings *timings)
{
	struct vxi_ctx_s *vxictx = v4l2_get_subdevdata(sd);

	memcpy(timings, &vxictx->dv_timings, sizeof(struct v4l2_dv_timings));
	return 0;
}

/** vxi_video_s_mbus_fmt() -to set the mbus_fmt
 *
 * @param[in] *sd The pointer to the VXI route subdevice
 * @param[in] *fmt The v4l2_mbus_framefmt structure
 */
static int vxi_video_s_mbus_fmt(struct v4l2_subdev *sd,
		  struct v4l2_mbus_framefmt *fmt)
{
	struct vxi_ctx_s *vxictx = v4l2_get_subdevdata(sd);
	struct media_pad *plane_pad;
	int id = 0;

	memcpy(&vxictx->fmt, fmt, sizeof(*fmt));

	/* We need to get the connected input to route */
	plane_pad = stm_media_find_remote_pad(&vxictx->pads[VXI_SRC_PAD],
					    MEDIA_LNK_FL_ENABLED, &id);
	if (!plane_pad) {
		_info(" VXI route not connected to plane yet ");
		return 0;
	}

	return v4l2_subdev_call(media_entity_to_v4l2_subdev(plane_pad->entity),
				video, s_mbus_fmt, &vxictx->fmt);
}

/** vxi_video_g_mbus_fmt() -to get the mbus_fmt
 *
 * @param[in] *sd The pointer to the VXI route subdevice
 * @param[out] *fmt The v4l2_mbus_framefmt structure
 */
static int vxi_video_g_mbus_fmt(struct v4l2_subdev *sd,
		  struct v4l2_mbus_framefmt *fmt)
{
	struct vxi_ctx_s *vxictx = v4l2_get_subdevdata(sd);

	memcpy(fmt, &vxictx->fmt, sizeof(*fmt));
	return 0;
}


/** vidextin_vxi_link_setup() - The link setup callback for the vidextin
 *   vxi entity. Currently this callback  does not do anything.
 * @pre The system should be initialized.
 * @param[in] *entity The pointer to the vidextin vxi entity
 * @param[in] *local The local pad for the link setup
 * @param[in] *remote The remote pad for the link setup
 * @param[in] flags The link flags
 * @return 0 if successful.
 *   -EINVAL on error.
 */
static int vidextin_vxi_link_setup(struct media_entity *entity,
				      const struct media_pad *local,
				      const struct media_pad *remote, u32 flags)
{
	_dbg(" link callback invoked %x %s", flags,
	     remote->entity->name);
	return 0;
}

/** con_vxiinput_route() -This function is used connect the input to the route. We
 * should allow only the vidextin subdev pads to get connected.
 * @pre The vxi route and vidextin vxi entities should be existing.
 * @param[in] *local The local pad information. In this case the vxi sink pad.
 * @param[in] *remote The remote pad information. In this case the input.
 * @return 0 if successful.
 *  -EINVAL on error.
 */
static int con_vxiinput_route(const struct media_pad *local,
			      const struct media_pad *remote)
{
	struct v4l2_subdev *sd;
	struct v4l2_subdev *dest_sd, *plane_sd;
	struct vxi_ctx_s *vxictx;
	struct vidextin_vxi_ctx_s *extinctx;
	int id = 0, ret;
	struct media_pad *temp_pad, *plane_pad;

	if (local ==  NULL || remote == NULL) {
		_err("Arguments local or remote are NULL ");
		return -EINVAL;
	}

	sd = media_entity_to_v4l2_subdev(local->entity);
	vxictx = v4l2_get_subdevdata(sd);

	if (WARN_ON(vxictx == NULL))
		return -EINVAL;

	dest_sd = media_entity_to_v4l2_subdev(remote->entity);
	extinctx = v4l2_get_subdevdata(dest_sd);

	if (WARN_ON(extinctx == NULL))
		return -EINVAL;

	/*Check if the route is connected to any other input */
	do {
		temp_pad = stm_media_find_remote_pad((struct media_pad *)local, MEDIA_LNK_FL_ENABLED, &id);
		if (temp_pad && temp_pad->entity->type == MEDIA_ENT_T_V4L2_SUBDEV_VIDEXTIN_VXI) {
			_err("  Please disable the existing input connection");
			return -EBUSY;
		}
	}while(temp_pad);

	/*Setup the VXI input */
	if (stm_vxi_set_ctrl(vxictx->vxidev_h,
				STM_VXI_CTRL_SELECT_INPUT,
				extinctx->vidextin_inst) != 0) {
		_err("error setting VXI input");
		return -EINVAL;
	}

	id = 0;
	/* We need to get the connected input to route */
	plane_pad = stm_media_find_remote_pad(&vxictx->pads[VXI_SRC_PAD],
					    MEDIA_LNK_FL_ENABLED, &id);

	if (!plane_pad) {
		_info(" Route not connected to Plane yet ");
		return 0;
	}

	plane_sd = media_entity_to_v4l2_subdev(plane_pad->entity);
	/*
	 * VXI doesn't support hotplug detection for the moment.
	 * Program PAL mode by default until supporting hotplug.
	 */
	ret = vxi_set_interface_params(vxictx, plane_sd);
	if (ret < 0) {
		_err ("Setting Pixel Stream Interface Parameters failed!");
		return ret;
	}

	return 0;
}

/** discon_vxiinput_route() -function used to disconnect the input from the route.
 * @pre The vxi route and vidextin vxi entities should be existing.
 * @param[in] *local The local pad information. In this case the vxi sink pad.
 * @param[in] *remote The remote pad information. In this case the input.
 * @return 0 if successful.
 *   -EINVAL on error.
 */
static int discon_vxiinput_route(const struct media_pad *local,
				 const struct media_pad *remote)
{
	struct v4l2_subdev *sd;
	struct v4l2_subdev *dest_sd, *plane_sd;
	struct vxi_ctx_s *vxictx;
	struct vidextin_vxi_ctx_s *extinctx;
	struct media_pad *plane_pad;
	struct v4l2_control ctrl;
	int id = 0;

	sd = media_entity_to_v4l2_subdev(local->entity);
	vxictx = v4l2_get_subdevdata(sd);

	if (WARN_ON(vxictx == NULL))
		return -EINVAL;

	dest_sd = media_entity_to_v4l2_subdev(remote->entity);
	extinctx = v4l2_get_subdevdata(dest_sd);

	if (WARN_ON(extinctx == NULL))
		return -EINVAL;

	/* We need to get the connected input to route */
	plane_pad = stm_media_find_remote_pad(&vxictx->pads[VXI_SRC_PAD],
					    MEDIA_LNK_FL_ENABLED, &id);

	if (!plane_pad) {
		_info(" Route not connected to Plane yet ");
		return 0;
	}

	/*
	 * We should be stopping VXI driver and disconnecting its input however
	 * there is no available VXI control or STKPI to do that!
	 * We could be hiding already connected planes in that case.
	 */
	plane_sd = media_entity_to_v4l2_subdev(plane_pad->entity);

	/*Assuming Timing is unstable */
	ctrl.id = V4L2_CID_DV_STM_RX_PIXEL_TIMING_STABILITY;
	ctrl.value = VCSPIXEL_TIMING_UNSTABLE;
	v4l2_subdev_call(plane_sd, core, s_ctrl, &ctrl);

	return 0;
}

/** con_vxi_plane() - function used to connect the route to the plane.
 * @pre The vxi route and plane entities should be existing.
 * @param[in] *vxi_vid_pad vxi_vid_pad pad information. In this case vxi src video pad.
 * @param[in] *plane_pad plane_pad pad information. In this case video plane sink pad.
 * @return 0 if successful.
 *    -EINVAL on error.
 */
static int con_vxi_plane(const struct media_pad *vxi_vid_pad,
			    const struct media_pad *plane_pad, u32 flags)
{
	struct v4l2_subdev *sd;
	struct vxi_ctx_s *vxictx;
	int ret = 0, id = 0;
	struct media_pad *inputpad;
	struct v4l2_subdev *plane_sd;

	sd = media_entity_to_v4l2_subdev(vxi_vid_pad->entity);
	vxictx = v4l2_get_subdevdata(sd);

	/*we need to set up the sink first */
	ret = media_entity_call(plane_pad->entity, link_setup,
		plane_pad, vxi_vid_pad, flags);
	if (ret < 0) {
		return ret;
	}

	/* We need to get the connected input to route */
	while(1) {
		inputpad = stm_media_find_remote_pad(&vxictx->pads[VXI_SNK_PAD],
						    MEDIA_LNK_FL_ENABLED, &id);
		if (!inputpad) {
			_info(" Input not connected to Route yet ");
			return 0;
		}
		if (inputpad->entity->type == MEDIA_ENT_T_V4L2_SUBDEV_VIDEXTIN_VXI)
			break;
	}
	plane_sd = media_entity_to_v4l2_subdev(plane_pad->entity);
	/*
	 * VXI doesn't support hotplug detection for the moment.
	 * Program PAL mode by default until supporting hotplug.
	 */
	ret = vxi_set_interface_params(vxictx, plane_sd);
	if (ret < 0) {
		_err ("Setting Pixel Stream Interface Parameters failed!");
		return ret;
	}

	return 0;
}

/** discon_vxi_plane() - function used to disconnect the route to the plane.
 *  @pre The vxi route and plane entities should be existing.
 *  @param[in] *vxi_vid_pad The vxi_vid_pad pad information. In this case the vxi src video pad.
 *  @param[in] *plane_pad The plane_pad pad information. In this case the video plane sink pad.
 *  @return 0 if successful.
 *     -EINVAL on error.
 */
static int discon_vxi_plane(const struct media_pad *vxi_vid_pad,
			       const struct media_pad *plane_pad)
{
	struct v4l2_control ctrl;
	struct v4l2_subdev *plane_sd;

	plane_sd = media_entity_to_v4l2_subdev(plane_pad->entity);
	/*Assuming Timing is unstable */
	ctrl.id = V4L2_CID_DV_STM_RX_PIXEL_TIMING_STABILITY;
	ctrl.value = VCSPIXEL_TIMING_UNSTABLE;
	v4l2_subdev_call(plane_sd, core, s_ctrl, &ctrl);
	return 0;
}

/** vxi_link_setup() - The link setup callback for the vxi entity.
 * This is the place where the inputs are connected to the route and the routes
 *  are connected to the video planes.
 *
 * @note
 *  When any link between the Routes and inputs/planes are enabled, then

 *    1. The Routes need to be opened.
 *    2. If the target entity is vidextin_vxi then the  corresponding input should
 *         be opened and connected to the route.
 *    3. If the target is a plane, then the source to plane connection
 *         should be attempted.
 *    4. If all the above actions are successful then a Monitor thread corresponding
 *         to the Route should be spawned for the Monitoring of the Route events and
 *         taking appropriate actions.
 *    5. Also appropriate events must be subscribed and event handlers should be
 *         installed for the Route.
 *
 *    @pre The system should be initialized.
 *    @param[in] *entity The pointer to the vxi entity
 *    @param[in] *local The local pad for the link setup
 *    @param[in] *remote The remote pad for the link setup
 *    @param[in] flags The link flags
 *    @return 0 if successful.
 *      -EINVAL on error.
 */
static int vxi_link_setup(struct media_entity *entity,
			     const struct media_pad *local,
			     const struct media_pad *remote, u32 flags)
{
	int ret = 0;

	if (!(entity && local && remote))
		return -EINVAL;

	switch (remote->entity->type) {
	case MEDIA_ENT_T_V4L2_SUBDEV_VIDEXTIN_VXI:
		/* do the input to Route connection here */
		if ((flags & MEDIA_LNK_FL_ENABLED))
			ret = con_vxiinput_route(local, remote);
		else
			ret = discon_vxiinput_route(local, remote);

		break;

	case MEDIA_ENT_T_V4L2_SUBDEV_PLANE_VIDEO:
		/* Do the plane to route connection here */
		if ((flags & MEDIA_LNK_FL_ENABLED))
			ret = con_vxi_plane(local, remote, flags);
		else
			ret = discon_vxi_plane(local, remote);
		break;

	default:
		break;
	}
	return ret;
}

static const struct v4l2_subdev_video_ops vxi_video_ops = {
	.s_dv_timings = &vxi_video_s_dv_timings,
	.g_dv_timings = &vxi_video_g_dv_timings,
	.s_mbus_fmt = &vxi_video_s_mbus_fmt,
	.g_mbus_fmt = &vxi_video_g_mbus_fmt,
};

static const struct v4l2_subdev_ops vidextin_ops = {
	.core = NULL,
};

static const struct v4l2_subdev_ops vxi_ops = {
	.video = &vxi_video_ops,
};

static const struct media_entity_operations vidextin_media_ops = {
	.link_setup = vidextin_vxi_link_setup,
};

static const struct media_entity_operations vxi_media_ops = {
	.link_setup = vxi_link_setup,
};

/**vidextin_vxi_create_connection() - connect input,vxi entities.
 * @pre The vxi and vidextin  structure should be existing and initialized.
 * @param[in] *vxi The vxi context.
 * @param[in] *extinctx The vidextin_vxi_ctx_s context.
 * @param[in] no_of_inputs number of the vxi inputs.
 * @return 0 if successful.
 *    error value on error.
 */
static int vidextin_vxi_create_connection(struct vxi_ctx_s *vxi,
				   struct vidextin_vxi_ctx_s *extinctx,
				   int no_of_inputs)
{
	struct media_entity *src, *sink;
	int i, ret = 0;

	/* Create disabled links between vidextIn and vxi */
	sink = &(vxi->vxi_sd.entity);
	for (i = 0; i < no_of_inputs; ++i) {
		src = &(extinctx[i].v4l2_vidext_sd.entity);
		ret = media_entity_create_link(src, 0, sink, VXI_SNK_PAD, 0);
		if (ret < 0) {
			_err("%s: failed to create link (%d) ", __func__, ret);
			return ret;
		}
	}

	return 0;
}

/** vxi_plane_create_connection() - connect vxi, plane entities.
 * @pre The VXI Driver should be existing and initialized.
 * @param[in] vxi The vxi context.
 * @param[in] no_of_devices number of the vxi device instances.
 * @return 0 if successful.
 *   error value on error.
 */
static int vxi_plane_create_connection(struct vxi_ctx_s *vxi,
				    int no_of_devices)
{
	struct media_entity *src, *sink;
	int i, ret = 0;

	/* Create disabled links between vxi devices and video planes */
	for (i = 0; i < no_of_devices; i++) {
		src = &(vxi[i].vxi_sd.entity);
		sink =
		    stm_media_find_entity_with_type_first
		    (MEDIA_ENT_T_V4L2_SUBDEV_PLANE_VIDEO);
		while (sink) {
			ret =
			    media_entity_create_link(src, VXI_SRC_PAD,
						     sink, 0, 0);
			if (ret < 0) {
				_err("%s: failed to create link (%d) ",
				     __func__, ret);
				return ret;
			}
			_dbg(" create link input-plane2 ");

			sink =
			    stm_media_find_entity_with_type_next(sink,
								 MEDIA_ENT_T_V4L2_SUBDEV_PLANE_VIDEO);
		}
	}

	return 0;
}

/** vidextin_release() -release the vidextin entity.
 * @pre The VXI Driver should be existing and initialized.
 * @param[in] nb_inputs The number of vxi inputs available.
 */
static void vidextin_release(int nb_inputs)
{
	int i;

	for (i = 0; i < nb_inputs; i++) {
		struct vidextin_vxi_ctx_s *ctx = &vxiinput_shared.vidextin_vxi[i];
		if (ctx) {
			struct v4l2_subdev *sd = &ctx->v4l2_vidext_sd;
			struct media_pad *pad = ctx->input_pad;

			if (pad)
				kfree(pad);
			if (sd) {
				struct media_entity *me = &sd->entity;
				stm_media_unregister_v4l2_subdev(sd);
				if (me)
					media_entity_cleanup(me);
			}
		}
	}
}

/** vidextin_init() - Initialise the vidextin entity.
 * @pre The VXI Driver should be existing and initialized.
 * @param[in] *extinctx The VidExtin vxi context.
 * @param[in] nb_inputs The number of vxi inputs available.
 * @param[in] vxi_inst The vxi object instance number.
 * @return 0 if successful.
 *   -EINVAL on error.
 */
static int vidextin_init(struct vidextin_vxi_ctx_s *extinctx, int nb_inputs, int vxi_inst)
{
	int ret = 0;
	int i = 0;

	struct v4l2_subdev *sd = NULL;
	struct media_pad *pad = NULL;
	struct media_entity *me = NULL;

	for (i = 0; i < nb_inputs; ++i) {
		/* vairable initialization */
		sd = &extinctx[i].v4l2_vidext_sd;
		me = &sd->entity;
		extinctx[i].vxidev = &vxiinput_shared;
		extinctx[i].vidextin_inst = i;

		pad = kzalloc(sizeof(*(pad)), GFP_KERNEL);
		if (!pad) {
			_err("error allocating mem for media pads");
			return -ENOMEM;
		}
		extinctx[i].input_pad = pad;
		/* subdev init for vidextIn */
		v4l2_subdev_init(sd, &vidextin_ops);

		snprintf(sd->name, sizeof(sd->name), "vidextin-vxi-0%d", i);
		v4l2_set_subdevdata(sd, &extinctx[i]);

		pad->flags = MEDIA_PAD_FL_SOURCE;

		me->ops = &vidextin_media_ops;
		me->type = MEDIA_ENT_T_V4L2_SUBDEV_VIDEXTIN_VXI;

		ret = media_entity_init(me, 1, pad, 0);
		if (ret < 0) {
			_err("entity init failed(%d) ", ret);
			goto error;
		}

		ret = stm_media_register_v4l2_subdev(sd);
		if (ret < 0) {
			_err("error registering output Plug sub-device");
			goto error;
		}
	}

	/* Create disabled links between vidextIn and vxi */
	ret = vidextin_vxi_create_connection(&(vxiinput_shared.vxi[vxi_inst]),
							vxiinput_shared.vidextin_vxi,
							nb_inputs);
	if (ret) {
		_err("failed to create link (%x) ", ret);
		goto error;
	}

	return 0;

error:
	/*
	 * We don't clean anything here as this is will be done by
	 * vidextin_release()function.
	 */
	return ret;
}

/** vxi_release() - release the vxi entity.
 * @pre The VXI Driver should be existing and initialized.
 * @param[in] nvxi The number of vxi route available.
 */
static void vxi_release(int nvxi)
{
	int i;

	for (i = 0; i < nvxi; i++) {
		struct vxi_ctx_s *vxi_context = &(vxiinput_shared.vxi[i]);
		if (vxi_context) {
			struct v4l2_subdev *sdvxi = &vxi_context->vxi_sd;

			if (sdvxi) {
				struct media_entity *mevxi = &sdvxi->entity;
				stm_media_unregister_v4l2_subdev(sdvxi);
				if (mevxi)
					media_entity_cleanup(mevxi);
			}
			if (vxi_context->vxidev_h)
				stm_vxi_close(vxi_context->vxidev_h);
		}
	}
}

/** vxi_init() - Initialise the vxi entity.
 * @pre The VXI Driver should be existing and initialized.
 * @param[in] *vxi_context The vxi context.
 * @param[in] vxi_inst The instance number of the context.
 * @return 0 if successful.
 *   -EINVAL on error.
 */
static int vxi_init(struct vxi_ctx_s *vxi_context, int vxi_inst)
{
	struct v4l2_subdev *sdvxi = NULL;
	struct media_pad *pad = NULL;
	struct media_entity *mevxi = NULL;
	int ret = 0;

	/* vairable initalization */
	sdvxi = &vxi_context->vxi_sd;
	pad = vxi_context->pads;
	mevxi = &sdvxi->entity;

	/* STKPI VXI initializatio, */
	ret = stm_vxi_open(vxi_inst, &(vxi_context->vxidev_h));
	if (ret == -ENODEV) {
		_err("VXI Ip not present  ");
		return ret;
	}
	if (ret != 0) {
		_err("VXI Open failed  ");
		return ret;
	}

	vxi_context->vxidev = &vxiinput_shared;
	/* subdev init for vxi */
	v4l2_subdev_init(sdvxi, &vxi_ops);
	snprintf(sdvxi->name, sizeof(sdvxi->name), "%s%d", STM_VXI_SUBDEV,
		 vxi_inst);
	vxi_context->vxi_inst = vxi_inst;
	v4l2_set_subdevdata(sdvxi, vxi_context);

	pad[VXI_SNK_PAD].flags = MEDIA_PAD_FL_SINK;
	pad[VXI_SRC_PAD].flags = MEDIA_PAD_FL_SOURCE;

	mevxi->ops = &vxi_media_ops;
	mevxi->type = MEDIA_ENT_T_V4L2_SUBDEV_VXI;

	ret =
	    media_entity_init(mevxi, ARRAY_SIZE(vxi_context->pads), pad,
			      0);
	if (ret < 0) {
		_err("%s: entity init failed(%d) ", __func__, ret);
		return ret;
	}

	ret = stm_media_register_v4l2_subdev(sdvxi);
	if (ret < 0) {
		_err("register subdev failed(%d) ", ret);
		goto err1;
	}

	init_vxi_timing_mbus_fmt(&vxi_context->dv_timings, &vxi_context->fmt);

	return 0;

err1:
	media_entity_cleanup(mevxi);
	return ret;

}

/** stm_v4l2_vxi_init() -function used to disconnect the route to the plane.
 * @pre The vxi driver should be initialized.
 * @note
 * 1. Find the number of valid VXI devices and initialize the corresponding
 * number of vxi_ctx_s structures.
 * 2. Open the stkpi vxi device and store the device handle in vxi_ctx_s.
 * 3. Initialize the vidextin_vxi_ctx_s object.
 * 4. Find the number of valid Inputs and initialize the corresponding number of
 *     vidextin_vxi_ctx_s structures.
 * 5. Create the subdevices and media Entities for vxi and vidextin_vxi.
 *     Also make sure to register the subdev_ops structure for the subdevices.
 * 6. The links between the vidextin_vxi and vxi entities should be created
 *     (but not enabled).
 *
 *     @return 0 if successful.
 *     -ENOMEM on memory allocation error.
 *     -EINVAL on other error.
 */
static int __init stm_v4l2_vxi_init(void)
{
	int ret = 0, i, no_of_vxi_init = 0;
	stm_vxi_capability_t capability;

	memset(&vxiinput_shared, 0, sizeof(vxiinput_shared));

	/*For the moment there is only one VXI instance */
	while(!ret) {
		stm_vxi_h vxidev_h=NULL;
		ret = stm_vxi_open(no_of_vxi_init, &(vxidev_h));
		if (ret == 0) {
			stm_vxi_close(vxidev_h);
			no_of_vxi_init++;
		}
	}

	if(no_of_vxi_init == 0) {
		_err("VXI Ip not present  ");
		return ret;
	}

	/* for the moment there is only one VXI hardware instance */
	vxiinput_shared.nb_vxi = no_of_vxi_init;

	vxiinput_shared.vxi =
	    kzalloc(sizeof(*(vxiinput_shared.vxi)) *
		    vxiinput_shared.nb_vxi, GFP_KERNEL);

	if (!vxiinput_shared.vxi) {
		_err("error allocating mem for vxi devices");
		ret = -ENOMEM;
		goto err1;
	}

	for (i = 0; i < vxiinput_shared.nb_vxi; ++i) {
		ret = vxi_init(&(vxiinput_shared.vxi[i]), i);
		if (ret < 0) {
			_err("v4l2_vxi init failed");
			goto err2;
		}
		vxiinput_shared.vxi[i].vxidev = &vxiinput_shared;

		ret =
				stm_vxi_get_capability(vxiinput_shared.vxi[i].vxidev_h,
								 &capability);
		if (ret != 0) {
			_err("VXI get_capability failed  ");
			goto err2;
		}
		vxiinput_shared.nb_vidextin_vxi = capability.number_of_inputs;

		vxiinput_shared.vidextin_vxi =
				kzalloc(sizeof(*(vxiinput_shared.vidextin_vxi)) *
					vxiinput_shared.nb_vidextin_vxi, GFP_KERNEL);

		if (!vxiinput_shared.vidextin_vxi) {
			_err("error allocating mem for inputs");
			ret = -ENOMEM;
			goto err3;
		}

		ret =
				vidextin_init(vxiinput_shared.vidextin_vxi,
					capability.number_of_inputs,
					i);
		if (ret != 0) {
			_err("vidextin_init failed  ");
			goto err3;
		}
	}

	/* Create disabled links between vxi and plane */
	ret = vxi_plane_create_connection(vxiinput_shared.vxi,
							 vxiinput_shared.nb_vxi);
	if (ret) {
		_err("failed to create link (%d) ", ret);
		goto err3;
	}

	return 0;

err3:
	vidextin_release(vxiinput_shared.nb_vidextin_vxi);
	if (vxiinput_shared.vidextin_vxi)
		kfree(vxiinput_shared.vidextin_vxi);

err2:
	vxi_release(vxiinput_shared.nb_vxi);
	if (vxiinput_shared.vxi)
		kfree(vxiinput_shared.vxi);

err1:
	memset(&vxiinput_shared, 0, sizeof(vxiinput_shared));
	return ret;
}

module_init(stm_v4l2_vxi_init);

/** stm_v4l2_vxi_exit() - fucntion called when exiting the vxi drivers
 *  @return 0 if successful.
 */
static void __exit stm_v4l2_vxi_exit(void)
{
	vxi_release(vxiinput_shared.nb_vxi);
	kfree(vxiinput_shared.vxi);
	vidextin_release(vxiinput_shared.nb_vidextin_vxi);
	kfree(vxiinput_shared.vidextin_vxi);
	memset(&vxiinput_shared, 0, sizeof(vxiinput_shared));
	return;
}

module_exit(stm_v4l2_vxi_exit);

MODULE_DESCRIPTION("STLinuxTV VXI driver");
MODULE_AUTHOR("STMicroelectronics Limited");
MODULE_LICENSE("GPL");
