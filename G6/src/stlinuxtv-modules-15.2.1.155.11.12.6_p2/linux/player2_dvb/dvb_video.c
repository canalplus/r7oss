/************************************************************************
Copyright (C) 2005 STMicroelectronics. All Rights Reserved.

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

Source file name : dvb_video.c
Author :           Julian

Implementation of linux dvb video device

Date        Modification                                    Name
----        ------------                                    --------
24-Mar-05   Created                                         Julian

************************************************************************/

#include <linux/module.h>
#include <linux/ioport.h>
#include <linux/mm.h>
#include <linux/bpa2.h>
#include <linux/dvb/audio.h>
#include <linux/dvb/video.h>
#include <linux/version.h>
#include <asm/uaccess.h>

#include "stmedia.h"
#include "stmedia_export.h"

#include "dvb_module.h"
#include "dvb_video.h"
#include "backend.h"
#include "display.h"
#include "stmedia_export.h"
#include "dvb_v4l2_export.h"

#include "stm_v4l2_encode.h"

#include "demux_filter.h"

/*{{{  prototypes*/
static int VideoOpen(struct inode *Inode, struct file *File);
static int VideoRelease(struct inode *Inode, struct file *File);
static int VideoIoctl(
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 35))
			     struct inode *Inode,
#endif
			     struct file *File,
			     unsigned int IoctlCode, void *ParamAddress);
static ssize_t dvb_video_write(struct file *file, const char __user * buffer,
						  size_t count, loff_t * ppos);
static unsigned int VideoPoll(struct file *File, poll_table * Wait);

static int VideoIoctlSetDisplayFormat(struct VideoDeviceContext_s *Context,
				      unsigned int Format);
static int VideoIoctlSetFormat(struct VideoDeviceContext_s *Context,
			       unsigned int Format);
static int VideoIoctlCommand(struct VideoDeviceContext_s *Context,
			     struct video_command *VideoCommand);
int VideoIoctlSetPlayInterval(struct VideoDeviceContext_s *Context,
			      video_play_interval_t * PlayInterval);

void VideoSetEvent(unsigned int number_of_events,
		   stm_event_info_t * eventsInfo);

int VideoStreamEventSubscriptionCreate(struct VideoDeviceContext_s *Context);
int VideoStreamEventSubscriptionDelete(struct VideoDeviceContext_s *Context);
static int VideoStop(struct VideoDeviceContext_s *Context, unsigned int Mode,
						 unsigned int VideoTerminate);
/*}}}*/

static long DvbVideoUnlockedIoctl(struct file *file, unsigned int foo,
			     unsigned long bar)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 35))
	return dvb_generic_ioctl(NULL, file, foo, bar);
#else
	return dvb_generic_ioctl(file, foo, bar);
#endif
}

/*{{{  static data*/
static struct file_operations VideoFops = {
owner:	THIS_MODULE,
write:	dvb_video_write,
unlocked_ioctl:DvbVideoUnlockedIoctl,
open:	VideoOpen,
release:VideoRelease,
poll:	VideoPoll
};

static struct dvb_device VideoDevice = {
priv:	NULL,
users:	8,
readers:7,
writers:1,
fops:	&VideoFops,
kernel_ioctl:VideoIoctl,
};

/* Assign encodings to backend id's using the actual ID's rather on relying on the correct order */

static stm_se_stream_encoding_t VideoContent[] = {
	STM_SE_STREAM_ENCODING_VIDEO_AUTO,
	STM_SE_STREAM_ENCODING_VIDEO_MPEG1,
	STM_SE_STREAM_ENCODING_VIDEO_MPEG2,
	STM_SE_STREAM_ENCODING_VIDEO_MJPEG,
	STM_SE_STREAM_ENCODING_VIDEO_DIVX3,
	STM_SE_STREAM_ENCODING_VIDEO_DIVX4,
	STM_SE_STREAM_ENCODING_VIDEO_DIVX5,
	STM_SE_STREAM_ENCODING_VIDEO_MPEG4P2,
	STM_SE_STREAM_ENCODING_VIDEO_H264,
	STM_SE_STREAM_ENCODING_VIDEO_WMV,
	STM_SE_STREAM_ENCODING_VIDEO_VC1,
	STM_SE_STREAM_ENCODING_VIDEO_RAW,
	STM_SE_STREAM_ENCODING_VIDEO_H263,
	STM_SE_STREAM_ENCODING_VIDEO_FLV1,
	STM_SE_STREAM_ENCODING_VIDEO_VP6,
	STM_SE_STREAM_ENCODING_VIDEO_RMV,
	STM_SE_STREAM_ENCODING_VIDEO_DIVXHD,
	STM_SE_STREAM_ENCODING_VIDEO_AVS,
	STM_SE_STREAM_ENCODING_VIDEO_VP3,
	STM_SE_STREAM_ENCODING_VIDEO_THEORA,
	STM_SE_STREAM_ENCODING_VIDEO_CAP,
	STM_SE_STREAM_ENCODING_VIDEO_VP8,
	STM_SE_STREAM_ENCODING_VIDEO_MVC,
	STM_SE_STREAM_ENCODING_VIDEO_HEVC,
	STM_SE_STREAM_ENCODING_VIDEO_VC1_RP227SPMP,
	STM_SE_STREAM_ENCODING_VIDEO_NONE,
	STM_SE_STREAM_ENCODING_VIDEO_DVP
};

#define PES_VIDEO_START_CODE                            0xe0
#define MPEG2_SEQUENCE_END_CODE                         0xb7
#ifdef INJECT_WITH_PTS
static const unsigned char Mpeg2VideoPesHeader[] = {
	0x00, 0x00, 0x01, PES_VIDEO_START_CODE,	/* header start code */
	0x00, 0x00,		/* Length word */
	0x80,			/* Marker (10), Scrambling control, Priority, Alignment, Copyright, Original/Copy */
	0x80,			/* Pts present, ESCR, ES_rate, DSM_trick, Add_copy, CRC_flag, Extension */
	0x05,			/* Pes header data length (pts) */
	0x21,			/* Marker bit + Pts top 3 bits + marker bit */
	0x00, 0x01,		/* next 15 bits + marker bit */
	0x00, 0x01		/* bottom 15 bits + marker bit */
};
#else
static const unsigned char Mpeg2VideoPesHeader[] = {
	0x00, 0x00, 0x01, PES_VIDEO_START_CODE,	/* header start code */
	0x00, 0x00,		/* Length word */
	0x80,			/* Marker (10), Scrambling control, Priority, Alignment, Copyright, Original/Copy */
	0x00,			/* Pts present, ESCR, ES_rate, DSM_trick, Add_copy, CRC_flag, Extension */
	0x00,			/* Pes header data length (no pts) */
};
#endif
static const unsigned char Mpeg2SequenceEnd[] = {
	0x00, 0x00, 0x01, MPEG2_SEQUENCE_END_CODE,	/* sequence end code */
	0xff, 0xff, 0xff, 0xff,	/* padding */
	0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff
};

#if defined(SDK2_ENABLE_STLINUXTV_STATISTICS)
/*}}}*/
/*{{{  Sysfs attributes */
static ssize_t VideoSysfsShowPlayState(struct device *class_dev,
				       struct device_attribute *attr, char *buf)
{
	struct VideoDeviceContext_s *Context =
	    container_of(class_dev, struct VideoDeviceContext_s, VideoClassDevice);

	switch ((int)Context->VideoPlayState) {
#define C(x) case STM_DVB_VIDEO_ ## x: return sprintf(buf, "%s\n", #x);
		C(STOPPED)
		    C(PLAYING)
		    C(FREEZED)
		    C(INCOMPLETE)
#undef C
	}

	return sprintf(buf, "UNKNOWN\n");
}
static DEVICE_ATTR(play_state, S_IRUGO, VideoSysfsShowPlayState, NULL);

static ssize_t VideoSysfsShowGrabCount(struct device *class_dev,
				       struct device_attribute *attr, char *buf)
{
	struct VideoDeviceContext_s *Context =
	    container_of(class_dev, struct VideoDeviceContext_s, VideoClassDevice);
	return sprintf(buf, "%d\n", Context->stats.grab_buffer_count);
}
static DEVICE_ATTR(grab_buffer_count, S_IRUGO, VideoSysfsShowGrabCount, NULL);

/*}}}*/
/*{{{  VideoInitSysfsAttributes */
int VideoInitSysfsAttributes(struct VideoDeviceContext_s *Context)
{
	int Result;

	Result = device_create_file(&Context->VideoClassDevice,
				    &dev_attr_play_state);
	if (Result)
		goto failed_play_state;

	Result = device_create_file(&Context->VideoClassDevice,
				    &dev_attr_grab_buffer_count);
	if (Result)
		goto failed_grab_count;

	return 0;

failed_grab_count:
	device_remove_file(&Context->VideoClassDevice, &dev_attr_play_state);
failed_play_state:
	DVB_ERROR("device_create_file failed (%d)\n", Result);
	return -1;
}

/*}}}*/

/**
 * VideoRemoveSysfsAttributes
 * This removes the video sub-device entery from sysfs
 */
void VideoRemoveSysfsAttributes(struct VideoDeviceContext_s *Context)
{
	if (!Context)
		return;

	device_remove_file(&Context->VideoClassDevice, &dev_attr_play_state);
	device_remove_file(&Context->VideoClassDevice, &dev_attr_grab_buffer_count);
}
#endif

void VideoResetStats(struct VideoDeviceContext_s *Context)
{
	if (!Context)
		return;

	memset(&Context->stats, 0, sizeof(struct AVStats_s));
}

/**
 * video_decoder_link_setup()
 * Callback to setup links for video decoder entity with display source and
 * encoder.
 */
static int video_decoder_link_setup(struct media_entity *entity,
				    const struct media_pad *local,
				    const struct media_pad *remote, u32 flags)
{
	int ret = 0;
	struct VideoDeviceContext_s *vid_ctx;

	/*
	 * Video decoder can only be connected to display source and encoder.
	 */
	if (remote->entity->type != MEDIA_ENT_T_V4L2_SUBDEV_PLANE_VIDEO &&
		remote->entity->type != MEDIA_ENT_T_V4L2_SUBDEV_VIDEO_ENCODER) {
		DVB_ERROR("Invalid remote type for %s\n", entity->name);
		return -EINVAL;
	}

	vid_ctx = stm_media_entity_to_video_context(entity);

	/*
	 * Take ioctl mutex to prevent race between MC and Video Ioctls.
	 * Play/Stop affects the connection and disconnection
	 */
	mutex_lock(&vid_ctx->vidops_mutex);

	/*
	 * Check from the sink if it's ready for connection
	 */
	if (flags & MEDIA_LNK_FL_ENABLED) {

		ret = media_entity_call(remote->entity,
				link_setup, remote, local, flags);
		if (ret)
			goto video_link_setup_done;
	}

	/*
	 * If there's no video stream, then,
	 * connect   : is deferred
	 * disconnect: is already done, but remove this from Media
	 *             controller database now
	 */
	if (!vid_ctx->VideoStream)
		goto video_link_setup_done;

	/*
	 * Setup the connection between video decoder -> display source
	 */
	if (remote->entity->type == MEDIA_ENT_T_V4L2_SUBDEV_PLANE_VIDEO) {

		struct stmedia_v4l2_subdev *sd;

		sd = entity_to_stmedia_v4l2_subdev(remote->entity);

		if (flags & MEDIA_LNK_FL_ENABLED) {

			ret = stm_se_play_stream_attach
					(vid_ctx->VideoStream->Handle,
					sd->stm_obj,
					STM_SE_PLAY_STREAM_OUTPUT_PORT_DEFAULT);

			/*
			 * Reconnection request received, no problem
			 */
			if (ret == -EALREADY)
				ret = 0;
			if(ret) {
				DVB_ERROR("%s -> %s failed (ret: %d)\n",
				entity->name, remote->entity->name, ret);
				goto video_link_setup_done;
			}

		} else {

			ret = stm_se_play_stream_detach
				(vid_ctx->VideoStream->Handle, sd->stm_obj);

			if (ret == -ENOTCONN)
				ret = 0;
			if (ret) {
				DVB_ERROR("%s -XX-> %s failed (ret: %d)\n",
				entity->name, remote->entity->name, ret);
				goto video_link_setup_done;
			}
		}

	} else if (remote->entity->type ==
					MEDIA_ENT_T_V4L2_SUBDEV_VIDEO_ENCODER) {

		/*
		 * Video decoder to encoder connection management
		 */
		if (flags & MEDIA_LNK_FL_ENABLED)
			ret = stm_dvb_stream_connect_encoder
					(&vid_ctx->mc_video_pad,
					remote, remote->entity->type,
					vid_ctx->VideoStream->Handle);
		else
			ret = stm_dvb_stream_disconnect_encoder
					(&vid_ctx->mc_video_pad,
					remote, remote->entity->type,
					vid_ctx->VideoStream->Handle);
	}

video_link_setup_done:
	mutex_unlock(&vid_ctx->vidops_mutex);
	return ret;
}

static const struct media_entity_operations video_decoder_media_ops = {
	.link_setup = video_decoder_link_setup,
};

static const struct v4l2_subdev_core_ops video_core_ops = {
	.queryctrl = v4l2_subdev_queryctrl,
	.querymenu = v4l2_subdev_querymenu,
	.g_ctrl = v4l2_subdev_g_ctrl,
	.s_ctrl = v4l2_subdev_s_ctrl,
	.g_ext_ctrls = v4l2_subdev_g_ext_ctrls,
	.try_ext_ctrls = v4l2_subdev_try_ext_ctrls,
	.s_ext_ctrls = v4l2_subdev_s_ext_ctrls,
};


static const struct v4l2_subdev_ops video_decoder_ops = {
	.core = &video_core_ops,
};

/*{{{  Videoinit*/
struct dvb_device *VideoInit(struct VideoDeviceContext_s *Context)
{
	int i;

	Context->VideoState.video_blank = 0;
	Context->VideoPlayState = STM_DVB_VIDEO_STOPPED;
	Context->VideoState.stream_source = VIDEO_SOURCE_DEMUX;
	Context->VideoState.video_format = VIDEO_FORMAT_4_3;
	Context->VideoState.display_format = VIDEO_CENTER_CUT_OUT;

	Context->VideoSize.w = 0;
	Context->VideoSize.h = 0;
	Context->VideoSize.aspect_ratio = 0;

	Context->FrameRate = 0;

	Context->VideoId = INVALID_DEMUX_ID;	/* CodeToInteger('v','i','d','s'); */
	Context->VideoEncoding = VIDEO_ENCODING_AUTO;
	Context->VideoEncodingFlags = 0;
	Context->VideoStream = NULL;
	Context->VideoOpenWrite = 0;
	Context->PlaySpeed = DVB_SPEED_NORMAL_PLAY;
	Context->frozen_playspeed = 0;

	for (i = 0; i < DVB_OPTION_MAX; i++)
		Context->PlayOption[i] = DVB_OPTION_VALUE_INVALID;

	Context->VideoPlayInterval.start = DVB_TIME_NOT_BOUNDED;
	Context->VideoPlayInterval.end = DVB_TIME_NOT_BOUNDED;

	Context->PixelAspectRatio.Numerator = 1;
	Context->PixelAspectRatio.Denominator = 1;

	init_waitqueue_head(&Context->VideoEvents.WaitQueue);
	Context->VideoEvents.Write = 0;
	Context->VideoEvents.Read = 0;
	Context->VideoEvents.Overflow = 0;

	Context->playback_context = &Context->DvbContext->PlaybackDeviceContext[Context->DvbContext->video_dev_off + Context->Id];

	Context->video_source = NULL;

	Context->demux_filter = NULL;

	return &VideoDevice;
}

/*}}}*/

/*{{{  VideoinitSubdev */
int VideoInitSubdev(struct VideoDeviceContext_s *Context)
{
	struct v4l2_subdev *sd = &Context->v4l2_video_sd;
	struct media_pad *pad = &Context->mc_video_pad;
	struct media_entity *me = &sd->entity;
	int ret;

	ret = VideoInitCtrlHandler(Context);
	if (ret < 0) {
		printk(KERN_ERR "%s: video ctrl handler init failed (%d)\n",
		       __func__, ret);
		return ret;
	}

	/* Initialize the V4L2 subdev / MC entity */
	v4l2_subdev_init(sd, &video_decoder_ops);

	/* Entities are called in the LinuxDVB manner */
	snprintf(sd->name, sizeof(sd->name), "dvb%d.video%d",
		 Context->DvbContext->DvbAdapter->num,
		 Context->Id);

	v4l2_set_subdevdata(sd, Context);

	pad->flags = MEDIA_PAD_FL_SOURCE;

	ret = media_entity_init(me, 1, pad, 0);
	if (ret < 0) {
		printk(KERN_ERR "%s: entity init failed(%d)\n", __func__, ret);
		v4l2_ctrl_handler_free(&Context->ctrl_handler);
		return ret;
	}

	me->ops = &video_decoder_media_ops;
	me->type = MEDIA_ENT_T_DVB_SUBDEV_VIDEO_DECODER;

	/* copy the ctrl handler to sd*/
	sd->ctrl_handler = &Context->ctrl_handler;

	ret = stm_media_register_v4l2_subdev(sd);
	if (ret < 0) {
		printk(KERN_ERR "%s: stm_media register failed (%d)\n",
		       __func__, ret);
		v4l2_ctrl_handler_free(&Context->ctrl_handler);
		return ret;
	}

	return 0;
}

/*}}}*/

/**
 * VideoReleaseSubdev
 * This function removes the video sub dev and its registration
 * with entity driver.
 */
void VideoReleaseSubdev(struct VideoDeviceContext_s *DeviceContext)
{
	struct v4l2_subdev *sd = &DeviceContext->v4l2_video_sd;
	struct media_entity *me = &sd->entity;

	v4l2_ctrl_handler_free(&DeviceContext->ctrl_handler);

	stm_media_unregister_v4l2_subdev(sd);

	media_entity_cleanup(me);
}

/*{{{  VideoSetInputWindowMode*/
int VideoSetInputWindowMode(struct VideoDeviceContext_s *Context,
				unsigned int Mode)
{
	int Result;

	mutex_lock(&Context->vidops_mutex);

	/* Set plane input window mode */
	Result = dvb_stm_display_set_input_window_mode(Context, Mode);

	mutex_unlock(&Context->vidops_mutex);

	return Result;
}

/*}}}*/
/*{{{  VideoGetInputWindowMode*/
int VideoGetInputWindowMode(struct VideoDeviceContext_s *Context,
				unsigned int *Mode)
{
	int Result;

	mutex_lock(&Context->vidops_mutex);

	/* Get plane input window mode */
	Result = dvb_stm_display_get_input_window_mode(Context, Mode);

	mutex_unlock(&Context->vidops_mutex);

	return Result;
}

/*}}}*/
/*{{{  VideoSetOutputWindowMode*/
int VideoSetOutputWindowMode(struct VideoDeviceContext_s *Context,
				unsigned int Mode)
{
	int Result;

	mutex_lock(&Context->vidops_mutex);

	/* Set plane output window mode */
	Result = dvb_stm_display_set_output_window_mode(Context, Mode);

	mutex_unlock(&Context->vidops_mutex);

	return Result;
}

/*}}}*/
/*{{{  VideoGetOutputWindowMode*/
int VideoGetOutputWindowMode(struct VideoDeviceContext_s *Context,
				unsigned int *Mode)
{
	int Result;

	mutex_lock(&Context->vidops_mutex);

	/* Get plane input window mode */
	Result = dvb_stm_display_get_output_window_mode(Context, Mode);

	mutex_unlock(&Context->vidops_mutex);

	return Result;
}

/*}}}*/
/*{{{  VideoSetOutputWindowValue*/
int VideoSetOutputWindowValue(struct VideoDeviceContext_s *Context,
			 unsigned int Left,
			 unsigned int Top,
			 unsigned int Width, unsigned int Height)
{
	int Result;

	mutex_lock(&Context->vidops_mutex);

	Result = dvb_stm_display_set_output_window_value(Context, Left, Top, Width, Height);

	mutex_unlock(&Context->vidops_mutex);

	return Result;
}

/*}}}*/
/*{{{  VideoGetOutputWindowValue*/
int VideoGetOutputWindowValue(struct VideoDeviceContext_s *Context,
			 unsigned int *Left,
			 unsigned int *Top,
			 unsigned int *Width, unsigned int *Height)
{
	int Result;

	mutex_lock(&Context->vidops_mutex);

	/* Get output window size from CoreDisplay */
	Result = dvb_stm_display_get_output_window_value(Context, Left, Top,
						   Width, Height);

	mutex_unlock(&Context->vidops_mutex);

	return Result;
}

/*}}}*/
/*{{{  VideoSetInputWindowValue*/
int VideoSetInputWindowValue(struct VideoDeviceContext_s *Context,
			unsigned int Left,
			unsigned int Top,
			unsigned int Width, unsigned int Height)
{
	int Result;

	DVB_DEBUG("%dx%d at %d,%d\n", Width, Height, Left, Top);
	mutex_lock(&Context->vidops_mutex);

	Result = dvb_stm_display_set_input_window_value(Context, Left, Top, Width, Height);

	mutex_unlock(&Context->vidops_mutex);

	return Result;
}

/*}}}*/
/*{{{  VideoGetInputWindowValue*/
int VideoGetInputWindowValue(struct VideoDeviceContext_s *Context,
			unsigned int *Left,
			unsigned int *Top,
			unsigned int *Width, unsigned int *Height)
{
	int Result;

	mutex_lock(&Context->vidops_mutex);

	/* Get input window size from CoreDisplay */
	Result = dvb_stm_display_get_input_window_value(Context, Left, Top,
						  Width, Height);

	mutex_unlock(&Context->vidops_mutex);

	return Result;
}

/*}}}*/

/*}}}*/
/*{{{  VideoGetPixelAspectRatio*/
int VideoGetPixelAspectRatio(struct VideoDeviceContext_s *Context,
			     unsigned int *Numerator, unsigned int *Denominator)
{
	/*DVB_DEBUG("\n"); */

	*Numerator = Context->PixelAspectRatio.Numerator;
	*Denominator = Context->PixelAspectRatio.Denominator;

	return 0;
}

/*}}}*/

/*}}}*/
static int VideoAttachToSinks(struct VideoDeviceContext_s *Context)
{
	struct media_pad *pad = &Context->mc_video_pad;
	struct stmedia_v4l2_subdev *sd;
	struct media_pad *remote_pad;
	struct media_entity *entity;
	int id = 0;
	int ret = 0;

	/* We go through all the enabled link */
	while (1) {
		/* Get the remote entity */
		remote_pad = stm_media_find_remote_pad(pad, MEDIA_LNK_FL_ENABLED, &id);
		if (!remote_pad) {
			break;
		}
		entity = remote_pad->entity;

		if (entity->type != MEDIA_ENT_T_V4L2_SUBDEV_PLANE_VIDEO)
			continue;

		sd = entity_to_stmedia_v4l2_subdev(entity);
		ret = stm_se_play_stream_attach( Context->VideoStream->Handle,
						 sd->stm_obj,
						 STM_SE_PLAY_STREAM_OUTPUT_PORT_DEFAULT );

		if(ret == -EALREADY)
			ret = 0;/* we ignore error if already connected*/
		if (ret){
			printk(KERN_ERR "%s: failed to attach to sink (%d)\n",
				       __func__, ret);
			return ret;
		}
	}

	return 0;
}

static int VideoDetachFromSinks(struct VideoDeviceContext_s *Context)
{
	struct media_pad *pad = &Context->mc_video_pad;
	struct stmedia_v4l2_subdev *sd;
	struct media_pad *remote_pad;
	struct media_entity *entity;
	int id = 0;
	int ret = 0;

	/* We go through all the enabled link */
	while (1) {
		/* Get the remote entity */
		remote_pad = stm_media_find_remote_pad(pad, MEDIA_LNK_FL_ENABLED, &id);
		if (!remote_pad) {
			break;
		}
		entity = remote_pad->entity;

		if (entity->type != MEDIA_ENT_T_V4L2_SUBDEV_PLANE_VIDEO)
			continue;

		sd = entity_to_stmedia_v4l2_subdev(entity);
		ret = stm_se_play_stream_detach( Context->VideoStream->Handle,
						 sd->stm_obj );
		if(ret == -ENOTCONN)
			ret = 0;/* we ignore error if already disconnected*/
		if (ret){
			printk(KERN_ERR "%s: failed to detach from sink (%d)\n",
				       __func__, ret);
			return ret;
		}
	}

	return 0;
}

/**
 * VideoStop
 * This stops the video decode and detaches it from player2
 */
static int VideoStop(struct VideoDeviceContext_s *vid_ctx,
				unsigned int mode, unsigned int terminate)
{
	struct DvbStreamContext_s *stream_ctx = vid_ctx->VideoStream;
	unsigned int blank = false;
	int ret = 0;

	if (!stream_ctx)
		goto video_stop_done;

	/*
	 * Mark the video as stopped, so, that any operations being done
	 * on other states be immediately returned.
	 */
	vid_ctx->VideoPlayState = STM_DVB_VIDEO_STOPPED;

	/*
	 * Same the current DVB_OPTION_VIDEO_BLANK policy value
	 */
	DvbStreamGetOption(stream_ctx, DVB_OPTION_VIDEO_BLANK, &blank);

	/*
	 * Based on mode we request to clear ALL or ALL but the last frame
	 * of the display
	 */
	DvbStreamSetOption(stream_ctx, DVB_OPTION_VIDEO_BLANK,
			mode?DVB_OPTION_VALUE_ENABLE:DVB_OPTION_VALUE_DISABLE);

	/*
	 * WA: It has been observed that in certain scenarios dvb_video_write
	 * has taken the lock for injecting data, but is unable to inject as
	 * there are no free buffers with SE. So, free the SE buffers,
	 * anticipating that we have made enough space for injection to
	 * complete, so, that the lock can be released.
	 */
	ret = stm_se_play_stream_drain(stream_ctx->Handle, true);
	if (ret)
		DVB_ERROR("Failed to drain audio stream\n");

	/*
	 * video-stop ioctl or ctrl-c (video-release) may stop the video.
	 * Make the video-stop ioctl return from here and let video-
	 * release complete the stuff.
	 */
	if (mutex_lock_interruptible(&vid_ctx->vidwrite_mutex)) {
		if (!terminate) {
			ret = -ERESTARTSYS;
			goto video_stop_done;
		} else {
			mutex_lock(&vid_ctx->vidwrite_mutex);
		}
	}

	/*
	 * WA: The first drain may be competing with write and will return
	 * after it has flushed what is written. The remaining injected
	 * data needs to be flushed and is done now.
	 */
	ret = stm_se_play_stream_drain(stream_ctx->Handle, true);
	if (ret)
		DVB_ERROR("Failed to drain the injected data\n");

	if (vid_ctx->VideoState.stream_source == VIDEO_SOURCE_DEMUX){
		ret = stm_dvb_detach_stream_from_demux(vid_ctx->playback_context->stm_demux,
							vid_ctx->demux_filter,
							stream_ctx->Handle);
		if (ret)
			printk(KERN_ERR "Failed to detach stream from demux\n");

	} else if (vid_ctx->VideoState.stream_source == VIDEO_SOURCE_MEMORY) {

		if (vid_ctx->VideoStream->memsrc) {
			dvb_playstream_remove_memsrc(vid_ctx->VideoStream->memsrc);
			vid_ctx->VideoStream->memsrc = NULL;
		}
	}

	/*
	 * Restore the policy
	 */
	DvbStreamSetOption(stream_ctx, DVB_OPTION_VIDEO_BLANK, blank);

	if (terminate) {
		/*
		 * Remove video event subscription
		 */
		VideoStreamEventSubscriptionDelete(vid_ctx);

		ret = stm_dvb_stream_disconnect_encoder
				(&vid_ctx->mc_video_pad, NULL,
				MEDIA_ENT_T_V4L2_SUBDEV_VIDEO_ENCODER,
				vid_ctx->VideoStream->Handle);
		if (ret)
			DVB_ERROR("Failed to disconnect stream from encoder\n");

		VideoDetachFromSinks(vid_ctx);

		dvb_playback_remove_stream(vid_ctx->playback_context->Playback,
								stream_ctx);
		vid_ctx->VideoStream = NULL;
	}

	mutex_unlock(&vid_ctx->vidwrite_mutex);

video_stop_done:
	return ret;
}

/*{{{  Ioctls*/
/*{{{  VideoIoctlStop*/
int VideoIoctlStop(struct VideoDeviceContext_s *Context, unsigned int Mode)
{
	int Result = 0;

	if (Context->VideoPlayState == STM_DVB_VIDEO_STOPPED)
		/* Nothing to do */
		return 0;

	DVB_DEBUG("(video%d)\n", Context->Id);

	Result = VideoStop(Context,Mode,false);
	if (Result)
		printk(KERN_ERR "Failed to stop the video\n");

	DVB_DEBUG("Play state = %d\n", Context->VideoPlayState);

	return Result;
}
/*}}}*/
/*{{{  VideoIoctlPlay*/
int VideoIoctlPlay(struct VideoDeviceContext_s *Context)
{
	int ret = 0;
	sigset_t newsigs, oldsigs;
	unsigned int i;
	struct video_command VideoCommand;

	DVB_DEBUG("(video%d)\n", Context->Id);

	switch(Context->VideoPlayState){
	case STM_DVB_VIDEO_PLAYING:
	case STM_DVB_VIDEO_FREEZED:
		/* Play is used implicitly to exit slow motion and fast forward states so
		   set speed to times 1 if video is playing or has been frozen */
		if (Context->frozen_playspeed)
			Context->frozen_playspeed = 0;
		ret = VideoIoctlSetSpeed(Context, DVB_SPEED_NORMAL_PLAY, 1);
		break;

	case STM_DVB_VIDEO_STOPPED:
	case STM_DVB_VIDEO_INCOMPLETE:
		mutex_lock(&Context->playback_context->Playback_alloc_mutex);
		if (Context->playback_context->Playback == NULL) {
			/*
			   Check to see if we are wired to a demux.  If so the demux should create the playback and we will get
			   another play call.  Just exit in this case.  If we are playing from memory we need to create a playback.
			 */
			if (Context->VideoState.stream_source ==
			    VIDEO_SOURCE_DEMUX){
				Context->VideoPlayState = STM_DVB_VIDEO_INCOMPLETE;
				mutex_unlock(&Context->playback_context->Playback_alloc_mutex);
				goto end;
			}
			ret =
			    DvbPlaybackCreate(Context->playback_context->Id,
					      &Context->playback_context->Playback);
			if (ret < 0){
				mutex_unlock(&Context->playback_context->Playback_alloc_mutex);
				goto end;
			}
		}
		mutex_unlock(&Context->playback_context->Playback_alloc_mutex);
		DvbPlaybackInitOption(Context->playback_context->Playback, Context->PlayOption, Context->PlayValue);

		if ((Context->VideoStream == NULL)
		    || (Context->VideoPlayState == STM_DVB_VIDEO_INCOMPLETE)) {
			unsigned int DemuxId =
			    (Context->VideoState.stream_source ==
			     VIDEO_SOURCE_DEMUX) ? Context->
			    playback_context->Id : INVALID_DEMUX_ID;

#ifdef DEFAULT_TO_MPEG2
			if (Context->VideoEncoding == VIDEO_ENCODING_AUTO)
				Context->VideoEncoding = VIDEO_ENCODING_MPEG2;	/* Video always defaults to MPEG2 if not set by user */
#endif

			/* a signal received in here can cause issues. Turn them off, just for this bit... */
			sigfillset(&newsigs);
			sigprocmask(SIG_BLOCK, &newsigs, &oldsigs);

			ret = dvb_playback_add_stream(Context->playback_context->Playback,
						      STM_SE_MEDIA_VIDEO,
						      VideoContent
						      [Context->VideoEncoding],
						      Context->
						      VideoEncodingFlags,
						      Context->
						      DvbContext->DvbAdapter->
						      num, DemuxId, Context->Id,
						      &Context->VideoStream);

			sigprocmask(SIG_SETMASK, &oldsigs, NULL);

			if (ret != 0) {
				DVB_ERROR("Failed to create stream context (%d)\n", ret);
				goto end;
			}

			ret = VideoAttachToSinks(Context);
			if (ret) {
				DVB_ERROR("Failed to attach to sinks\n");
				goto end;
			}

			ret = stm_dvb_stream_connect_encoder
					(&Context->mc_video_pad, NULL,
					MEDIA_ENT_T_V4L2_SUBDEV_VIDEO_ENCODER,
					Context->VideoStream->Handle);
			if(ret) {
				DVB_ERROR("Failed to attach the stream to encoder\n");
				goto end;
			}


		} else {
			if ((Context->VideoEncoding <= VIDEO_ENCODING_AUTO)
			    || (Context->VideoEncoding >= VIDEO_ENCODING_NONE)) {
				DVB_ERROR
				    ("Cannot switch to undefined encoding after play has started\n");
				ret = -EINVAL;
				goto end;
			}

			ret = DvbStreamSwitch(Context->VideoStream,
						 VideoContent
						 [Context->VideoEncoding]);
			if (ret) {
				DVB_ERROR("Failed to perform stream switch\n");
				goto end;
			}
		}

		/*
		 * Create a memsrc if the stream source is memory
		 */
		if (Context->VideoState.stream_source == VIDEO_SOURCE_MEMORY) {
			ret = dvb_playstream_add_memsrc(STM_SE_MEDIA_VIDEO,
					 Context->Id, Context->VideoStream->Handle,
					 &Context->VideoStream->memsrc);
			if (ret) {
				DVB_ERROR("Failed to create memsrc\n");
				goto end;
			}
		}

		/* Enable or disable the stream based on the video_blank field */
		ret = DvbStreamEnable(Context->VideoStream, !Context->VideoState.video_blank);
		if (ret) {
			DVB_ERROR("Failed to enable stream\n");
			goto end;
		}

		ret = VideoIoctlSetSpeed(Context, Context->PlaySpeed, 0);
		if (ret) {
			DVB_ERROR("Failed to set speed\n");
			goto end;
		}

		ret = VideoIoctlSetPlayInterval(Context, &Context->VideoPlayInterval);
		if (ret) {
			DVB_ERROR("Failed to set play interval\n");
			goto end;
		}

		ret = VideoIoctlSetId(Context, Context->VideoId);
		if (ret) {
			DVB_ERROR("Failed to set VideoId\n");
			goto end;
		}

		ret = VideoStreamEventSubscriptionCreate(Context);
		if (ret) {
			DVB_ERROR("Failed to set subscribe to events\n");
			goto end;
		}

		memset(&VideoCommand, 0, sizeof(struct video_command));
		VideoCommand.cmd = VIDEO_CMD_SET_OPTION;
		for (i = 0; i < DVB_OPTION_MAX; i++) {
			if (Context->PlayOption[i] != DVB_OPTION_VALUE_INVALID) {
				VideoCommand.option.option = i;
				VideoCommand.option.value = Context->PlayValue[i];
				Context->PlayOption[i] = DVB_OPTION_VALUE_INVALID;
				VideoIoctlCommand(Context, &VideoCommand);
			}
		}

		if (Context->VideoState.stream_source == VIDEO_SOURCE_DEMUX) {
			ret = stm_dvb_attach_stream_to_demux(Context->playback_context->stm_demux,
					Context->demux_filter, Context->VideoStream->Handle);
			if (ret) {
				DVB_ERROR("Failed to attach stream to demuxd\n");
				goto end;
			}
		}

		break;

	default:
		/* NOT REACHED */
		break;
	}

	Context->VideoPlayState = STM_DVB_VIDEO_PLAYING;

end:
	DVB_DEBUG("(video%d) State = %d\n", Context->Id,
		  Context->VideoPlayState);
	return ret;
}

/*}}}*/

/**
 * VideoIoctlFreeze() - freeze the video playback
 */
static int VideoIoctlFreeze(struct VideoDeviceContext_s *Context)
{
	int ret = 0;

	DVB_DEBUG("(video%d)\n", Context->Id);

	if (Context->VideoPlayState == STM_DVB_VIDEO_PLAYING) {

		Context->frozen_playspeed = Context->PlaySpeed;

		ret = VideoIoctlSetSpeed(Context, DVB_SPEED_STOPPED, 1);
		if (ret) {
			DVB_ERROR("Failed to freeze the video playback\n");
			Context->frozen_playspeed = 0;
		}

	}

	return ret;
}

/**
 * VideoIoctlContinue() - continue from the last playback speed
 */
static int VideoIoctlContinue(struct VideoDeviceContext_s *Context)
{
	int ret = -EINVAL, speed = DVB_SPEED_NORMAL_PLAY;

	DVB_DEBUG("(video%d)\n", Context->Id);

	if ((Context->VideoPlayState == STM_DVB_VIDEO_FREEZED) ||
		(Context->VideoPlayState == STM_DVB_VIDEO_PLAYING)) {

		/*
		 * Reuse the playspeed if continue is after a pause
		 */
		if (Context->frozen_playspeed) {
			speed = Context->frozen_playspeed;
			Context->frozen_playspeed = 0;
		}

		ret = VideoIoctlSetSpeed(Context, speed, 1);
	}

	return ret;
}

/*{{{  VideoIoctlSelectSource*/
static int VideoIoctlSelectSource(struct VideoDeviceContext_s *Context,
				  video_stream_source_t Source)
{
	DVB_DEBUG("(video%d)\n", Context->Id);
	Context->VideoState.stream_source = Source;
	if (Source == VIDEO_SOURCE_DEMUX)
		Context->StreamType = STREAM_TYPE_TRANSPORT;
	else
		Context->StreamType = STREAM_TYPE_PES;
	DVB_DEBUG("Source = %x\n", Context->VideoState.stream_source);

	return 0;
}

/*}}}*/
/*{{{  VideoIoctlSetBlank*/
static int VideoIoctlSetBlank(struct VideoDeviceContext_s *Context,
			      unsigned int Mode)
{
	int Result = 0;

	DVB_DEBUG("(video%d) Blank = %d\n", Context->Id,
		  Context->VideoState.video_blank);

	Context->VideoState.video_blank = (int)Mode;

	/* If a video stream already exists, blank apply the mode now */
	if (Context->VideoStream != NULL)
		Result = DvbStreamEnable(Context->VideoStream, !Mode);

	return 0;
}

/*}}}*/
/*{{{  VideoIoctlGetStatus*/
static int VideoIoctlGetStatus(struct VideoDeviceContext_s *Context,
			       struct video_status *Status)
{
	DVB_DEBUG("(video%d)\n", Context->Id);
	memcpy(Status, &Context->VideoState, sizeof(struct video_status));
	Status->play_state = Context->VideoPlayState;

	return 0;
}

/*}}}*/

/**
 * VideoIoctlGetEvent() - fills the user event info
 * @vid_ctx   : Video decoder context
 * @vid_evt   : Location to store event info
 * @f_flags   : file->f_flags
 * Fill in the event info from the last read index when this function was
 * called or from the read index which got pushed to another location if
 * there's overflow. This means, the event may not be the latest which has
 * occured. So, it is recommended that poll() be intiated immediately, in
 * fact, just after decoder open so, that the event is dequeued as soon as
 * it occurs.
 */
static int VideoIoctlGetEvent(struct VideoDeviceContext_s *vid_ctx,
			      struct video_event *vid_evt, int f_flags)
{
	int ret = 0;
	struct VideoEvent_s *evt_lst = &vid_ctx->VideoEvents;

	if ((f_flags & O_NONBLOCK) != O_NONBLOCK) {
		/*
		 * Relinquish the ioctl lock, so, that other ioctl operations
		 * can be done till we wait for events.
		 */
		mutex_unlock(&vid_ctx->vidops_mutex);

		/*
		 * There may be inconsitency in reading Read/Write values,
		 * but, there is no reason to worry as the producer (event
		 * handler) will always keep them unequal. So, it is important
		 * here to break this wait. We take the lock later, then
		 * the access is serialized guarrating the correct values.
		 */
		ret = wait_event_interruptible(evt_lst->WaitQueue,
					(evt_lst->Write != evt_lst->Read));
		/*
		 * Reacquire the lock
		 */
		mutex_lock(&vid_ctx->vidops_mutex);

		if (ret) {
			ret = -ERESTARTSYS;
			goto get_event_failed;
		}
	}

	mutex_lock(&evt_lst->Lock);

	/*
	 * The internal event list overflowed.
	 */
	if (evt_lst->Overflow) {
		evt_lst->Overflow = 0;
		ret = -EOVERFLOW;
		goto get_event_done;
	}

	/*
	 * This can only happen for non-blocking mode
	 */
	if (evt_lst->Write == evt_lst->Read) {
		ret = -EWOULDBLOCK;
		goto get_event_done;
	}

	/*
	 * Copy the event information and pass onto user
	 */
	memcpy(vid_evt, &evt_lst->Event[evt_lst->Read],
				       sizeof(struct video_event));

	evt_lst->Read = (evt_lst->Read + 1) % MAX_VIDEO_EVENT;

get_event_done:
	mutex_unlock(&evt_lst->Lock);
get_event_failed:
	return ret;
}

/*{{{  VideoIoctlGetSize*/
int VideoIoctlGetSize(struct VideoDeviceContext_s *Context, video_size_t * Size)
{
	DVB_DEBUG("(video%d)\n", Context->Id);
	memcpy(Size, &Context->VideoSize, sizeof(video_size_t));

	return 0;
}

/*}}}*/
/*{{{  VideoIoctlSetDisplayFormat*/
static int VideoIoctlSetDisplayFormat(struct VideoDeviceContext_s *Context,
				      unsigned int Format)
{
	int Result = 0;
	unsigned int conv_mode;

	DVB_DEBUG("(video%d) Display format = %d\n", Context->Id, Format);

	switch (Format) {
	case VIDEO_PAN_SCAN:
		conv_mode = VCSASPECT_RATIO_CONV_PAN_AND_SCAN;
		break;
	case VIDEO_LETTER_BOX:
		conv_mode = VCSASPECT_RATIO_CONV_LETTER_BOX;
		break;
	case VIDEO_CENTER_CUT_OUT:
		conv_mode = VCSASPECT_RATIO_CONV_COMBINED;
		break;
	default:
		return -EINVAL;
		break;
	}

	Result = dvb_stm_display_set_aspect_ratio_conversion_mode(Context,
							conv_mode);

	if (Result != 0)
		return Result;

	Context->VideoState.display_format = (video_displayformat_t) Format;

	return Result;
}
/*}}}*/

/*{{{  VideoIoctlStillPicture*/
static int VideoIoctlStillPicture(struct VideoDeviceContext_s *Context,
				  struct video_still_picture *Still)
{
	int Result = 0;
	dvb_discontinuity_t Discontinuity =
	    DVB_DISCONTINUITY_SKIP | DVB_DISCONTINUITY_SURPLUS_DATA;
	unsigned char *Buffer = NULL;
	unsigned int Sync = true;

	DVB_DEBUG("(video%d)\n", Context->Id);
#if 0
	/* still picture required for blueray which is ts so we relax this restriction */
	if (Context->VideoState.stream_source == VIDEO_SOURCE_DEMUX) {
		DVB_ERROR
		    ("Stream source not VIDEO_SOURCE_MEMORY - cannot write I-frame\n");
		return -EPERM;	/* Not allowed to write to device if connected to demux */
	}
#endif

	if (Context->VideoPlayState == STM_DVB_VIDEO_STOPPED) {
		DVB_ERROR("Cannot inject still picture before play called\n");
		return -EPERM;
	}

	if (Context->VideoStream == NULL) {
		DVB_ERROR
		    ("No video stream exists to be written to (previous VIDEO_PLAY failed?)\n");
		return -ENODEV;
	}
#if 0
	if (!access_ok(VERIFY_READ, Still->iFrame, Still->size))
#else
	if (Still->iFrame == NULL)
#endif
	{
		DVB_ERROR("Invalid still picture data = %p, size = %d.\n",
			  Still->iFrame, Still->size);
		return -EFAULT;
	}

	Buffer =
	    bigphysarea_alloc(sizeof(Mpeg2VideoPesHeader) + Still->size +
			      sizeof(Mpeg2SequenceEnd));
	if (Buffer == NULL) {
		DVB_ERROR
		    ("Unable to create still picture buffer - insufficient memory\n");
		return -ENOMEM;
	}

	if ((Context->PlaySpeed == 0)
	    || (Context->PlaySpeed == DVB_SPEED_REVERSE_STOPPED)) {
		Result = VideoIoctlSetSpeed(Context, DVB_SPEED_NORMAL_PLAY, 0);
		if (Result < 0) {
			bigphysarea_free_pages(Buffer);
			return Result;
		}
	}

	/* switch off sync */
	DvbStreamGetOption(Context->VideoStream, DVB_OPTION_AV_SYNC, &Sync);
	DvbStreamSetOption(Context->VideoStream, DVB_OPTION_AV_SYNC,
			   DVB_OPTION_VALUE_DISABLE);

	/*
	   Firstly we drain the stream to allow any pictures in the pipeline to be displayed.  This
	   is followed by a jump with discard to flush out any partial frames.
	 */
	if (mutex_lock_interruptible(&Context->vidwrite_mutex) != 0)
		return -ERESTARTSYS;

	DvbStreamDrain(Context->VideoStream, false);
	Discontinuity = DVB_DISCONTINUITY_SKIP | DVB_DISCONTINUITY_SURPLUS_DATA;
	DvbStreamDiscontinuity(Context->VideoStream, Discontinuity);

	DVB_DEBUG("Still picture at %p, size = %d.\n", Still->iFrame,
		  Still->size);

	memcpy(Buffer, Mpeg2VideoPesHeader, sizeof(Mpeg2VideoPesHeader));
	Result =
	    copy_from_user(Buffer + sizeof(Mpeg2VideoPesHeader), Still->iFrame,
			   Still->size);
	if (Result) {
		mutex_unlock(&Context->vidwrite_mutex);
		return -EFAULT;
	}
	memcpy(Buffer + sizeof(Mpeg2VideoPesHeader) + Still->size,
	       Mpeg2SequenceEnd, sizeof(Mpeg2SequenceEnd));

	dvb_stream_inject(Context->VideoStream, Buffer,
			sizeof(Mpeg2VideoPesHeader) + Still->size +
			sizeof(Mpeg2SequenceEnd));
	Discontinuity = DVB_DISCONTINUITY_SKIP;
	DvbStreamDiscontinuity(Context->VideoStream, Discontinuity);
	DvbStreamDrain(Context->VideoStream, false);

	DvbStreamSetOption(Context->VideoStream, DVB_OPTION_AV_SYNC, Sync);

	mutex_unlock(&Context->vidwrite_mutex);

	bigphysarea_free_pages(Buffer);

	return 0;
}

/*}}}*/
/*{{{  VideoIoctlSetSpeed*/
int VideoIoctlSetSpeed(struct VideoDeviceContext_s *Context, int Speed,
		       int PlaySpeedUpdate)
{
	int DirectionChange;
	int Result;

	DVB_DEBUG("(video%d)\n", Context->Id);
	if (Context->playback_context->Playback == NULL) {
		Context->PlaySpeed = Speed;
		return 0;
	}

	/* If changing direction we require a write lock */
	DirectionChange = ((Speed * Context->PlaySpeed) < 0);
	if (DirectionChange) {
		/* Discard previously injected data to free the lock. */
		DvbStreamDrain(Context->VideoStream, true);
		if (mutex_lock_interruptible(&Context->vidwrite_mutex) !=
		    0)
			return -ERESTARTSYS;	/* Give up for now. */
	}

	Result = DvbPlaybackSetSpeed(Context->playback_context->Playback, Speed);
	if (Result >= 0)
		Result =
		    DvbPlaybackGetSpeed(Context->playback_context->Playback, &Context->PlaySpeed);

	/* If changing direction release write lock */
	if (DirectionChange)
		mutex_unlock(&Context->vidwrite_mutex);

	/* Fill in the VideoPlayState variable */
	if (PlaySpeedUpdate
	    && ((Context->VideoPlayState == STM_DVB_VIDEO_FREEZED) |
		(Context->VideoPlayState == STM_DVB_VIDEO_PLAYING))) {
		if (Context->PlaySpeed == DVB_SPEED_STOPPED)
			Context->VideoPlayState = STM_DVB_VIDEO_FREEZED;
		else
			Context->VideoPlayState = STM_DVB_VIDEO_PLAYING;
	}

	DVB_DEBUG("Speed set to %d\n", Context->PlaySpeed);

	return Result;
}

/*}}}*/
/*{{{  VideoIoctlFastForward*/
static int VideoIoctlFastForward(struct VideoDeviceContext_s *Context, int Frames)
{
	DVB_DEBUG("(video%d)\n", Context->Id);
	return VideoIoctlSetSpeed(Context,
				  DVB_SPEED_NORMAL_PLAY * (Frames + 1), 1);
}

/*}}}*/
/*{{{  VideoIoctlSlowMotion*/
static int VideoIoctlSlowMotion(struct VideoDeviceContext_s *Context,
				unsigned int Times)
{
	DVB_DEBUG("(video%d)\n", Context->Id);
	return VideoIoctlSetSpeed(Context, DVB_SPEED_NORMAL_PLAY / (Times + 1),
				  1);
}

/*}}}*/
/*{{{  VideoIoctlGetCapabilities*/
static int VideoIoctlGetCapabilities(struct VideoDeviceContext_s *Context,
				     int *Capabilities)
{
	DVB_DEBUG("(video%d)\n", Context->Id);
	*Capabilities = VIDEO_CAP_MPEG1 | VIDEO_CAP_MPEG2;
	DVB_DEBUG("Capabilities returned = %x\n", *Capabilities);

	return 0;
}

/*}}}*/
/*{{{  VideoIoctlGetPts*/
static int VideoIoctlGetPts(struct VideoDeviceContext_s *Context,
			    unsigned long long int *Pts)
{
	int Result;
	stm_se_play_stream_info_t PlayInfo;

	Result = DvbStreamGetPlayInfo(Context->VideoStream, &PlayInfo);
	if (Result != 0)
		return Result;

	*Pts = PlayInfo.pts;

	DVB_DEBUG("(video%d) %llu\n", Context->Id, *Pts);
	return Result;
}

/*}}}*/
/*{{{  VideoIoctlGetFrameCount*/
static int VideoIoctlGetFrameCount(struct VideoDeviceContext_s *Context,
				   unsigned long long int *FrameCount)
{
	int Result;
	stm_se_play_stream_info_t PlayInfo;

	/*DVB_DEBUG ("(video%d)\n", Context->Id); */
	Result = DvbStreamGetPlayInfo(Context->VideoStream, &PlayInfo);
	if (Result != 0)
		return Result;

	*FrameCount = PlayInfo.frame_count;

	return Result;
}

/*}}}*/
/*{{{  VideoIoctlGetPlayTime*/
static int VideoIoctlGetPlayTime(struct VideoDeviceContext_s *Context,
				 video_play_time_t * VideoPlayTime)
{
	stm_se_play_stream_info_t PlayInfo;

	if (Context->VideoStream == NULL)
		return -EINVAL;

	DVB_DEBUG("(video%d)\n", Context->Id);
	DvbStreamGetPlayInfo(Context->VideoStream, &PlayInfo);

	VideoPlayTime->system_time = PlayInfo.system_time;
	VideoPlayTime->presentation_time = PlayInfo.presentation_time;
	VideoPlayTime->pts = PlayInfo.pts;

	return 0;
}

/*}}}*/
/*{{{  VideoIoctlGetPlayInfo*/
static int VideoIoctlGetPlayInfo(struct VideoDeviceContext_s *Context,
				 video_play_info_t * VideoPlayInfo)
{
	stm_se_play_stream_info_t PlayInfo;

	if (Context->VideoStream == NULL)
		return -EINVAL;

	DVB_DEBUG("(video%d)\n", Context->Id);
	DvbStreamGetPlayInfo(Context->VideoStream, &PlayInfo);

	VideoPlayInfo->system_time = PlayInfo.system_time;
	VideoPlayInfo->presentation_time = PlayInfo.presentation_time;
	VideoPlayInfo->pts = PlayInfo.pts;
	VideoPlayInfo->frame_count = PlayInfo.frame_count;

	return 0;
}

/*}}}*/
/*{{{  VideoIoctlSetId*/
int VideoIoctlSetId(struct VideoDeviceContext_s *Context, int Id)
{
	DVB_DEBUG("(video%d) Setting Video Id to 0x%04x\n", Context->Id, Id);

	Context->VideoId = Id;

	return 0;
}

/*}}}*/

/**
 * VideoIoctlClearBuffer() - Clear video buffers
 * @Context: Video decoder context
 * This call is in response to VIDEO_CLEAR_BUFFER ioctl. It responsibility is
 * to flush the streaming engine (SE) buffers. After this call is completed,
 * there's nothing left for SE to decode.
 *
 * Historically, this callback used to inject SURPLUS discontinuity to notify SE
 * that the current injection may be partial, so, discard data from Collator.
 * Now, SE is itself discarding data from collator when we drain() with discard.
 * This affects the injection into SE, so, vidwrite_mutex is taken. It becomes
 * the caller responsibility not to introduce race between injection and clear,
 * because, whosoever takes the mutex first will be completed first and it is
 * very obvious not to flush after new injection.
 */
int VideoIoctlClearBuffer(struct VideoDeviceContext_s *Context)
{
	int Result = 0;

	DVB_DEBUG("(video%d)\n", Context->Id);

	DvbStreamDrain(Context->VideoStream, true);

	if (mutex_lock_interruptible(&Context->vidwrite_mutex) != 0)
		return -ERESTARTSYS;

	Result = DvbStreamDrain(Context->VideoStream, true);

	mutex_unlock(&Context->vidwrite_mutex);

	return Result;
}

/*{{{  VideoIoctlSetStreamType*/
static int VideoIoctlSetStreamType(struct VideoDeviceContext_s *Context,
				   unsigned int Type)
{
	DVB_DEBUG("(video%d) Set type to %d\n", Context->Id, Type);

	if (Type > STREAM_TYPE_RAW)
		return -EINVAL;

	Context->StreamType = (stream_type_t) Type;

	return 0;
}

/*}}}*/
/*{{{  VideoIoctlSetFormat*/

static int VideoIoctlSetFormat(struct VideoDeviceContext_s *Context,
			       unsigned int Format)
{
	int Result = 0;
	unsigned int aspect_ratio;

	DVB_DEBUG("(video%d) Format = %x\n", Context->Id, Format);

	switch (Format) {
	case VIDEO_FORMAT_4_3:
		aspect_ratio = VCSOUTPUT_DISPLAY_ASPECT_RATIO_4_3;
		break;
	case VIDEO_FORMAT_16_9:
		aspect_ratio = VCSOUTPUT_DISPLAY_ASPECT_RATIO_16_9;
		break;
	default:
		return -EINVAL;
		break;
	}

	Result =
		dvb_stm_display_set_output_display_aspect_ratio(Context,
							aspect_ratio);

        if (Result != 0)
                return Result;

	Context->VideoState.video_format = (video_format_t) Format;

	return Result;
}




/*}}}*/
/*{{{  VideoIoctlSetSystem*/
static int VideoIoctlSetSystem(struct VideoDeviceContext_s *Context,
			       video_system_t System)
{
	DVB_ERROR("Not supported\n");
	return -EPERM;
}

/*}}}*/
/*{{{  VideoIoctlSetHighlight*/
static int VideoIoctlSetHighlight(struct VideoDeviceContext_s *Context,
				  video_highlight_t * Highlight)
{
	DVB_ERROR("Not supported\n");
	return -EPERM;
}

/*}}}*/
/*{{{  VideoIoctlSetSpu*/
static int VideoIoctlSetSpu(struct VideoDeviceContext_s *Context, video_spu_t * Spu)
{
	DVB_ERROR("Not supported\n");
	return -EPERM;
}

/*}}}*/
/*{{{  VideoIoctlSetSpuPalette*/
static int VideoIoctlSetSpuPalette(struct VideoDeviceContext_s *Context,
				   video_spu_palette_t * SpuPalette)
{
	DVB_ERROR("Not supported\n");
	return -EPERM;
}

/*}}}*/
/*{{{  VideoIoctlGetNavi*/
static int VideoIoctlGetNavi(struct VideoDeviceContext_s *Context,
			     video_navi_pack_t * NaviPacket)
{
	DVB_ERROR("Not supported\n");
	return -EPERM;
}

/*}}}*/
/*{{{  VideoIoctlSetAttributes*/
static int VideoIoctlSetAttributes(struct VideoDeviceContext_s *Context,
				   video_attributes_t * Attributes)
{
	DVB_ERROR("Not supported\n");
	return -EPERM;
}

/*}}}*/
/*{{{  VideoIoctlGetFrameRate*/
int VideoIoctlGetFrameRate(struct VideoDeviceContext_s *Context,
				  int *FrameRate)
{
	DVB_DEBUG("(video%d)\n", Context->Id);

	*FrameRate = Context->FrameRate;
	return 0;
}

/*}}}*/
/*{{{  VideoIoctlSetEncoding*/
static int VideoIoctlSetEncoding(struct VideoDeviceContext_s *Context,
				 unsigned int Encoding)
{
	DVB_DEBUG("(video%d) Set encoding to %d\n", Context->Id, Encoding);

#define ENCODING_MASK  0xFFFF0000
	/*
         * Split Encoding type and ehavioural flags!
         * Some bits of encoding are used to modify the play_stream
         * behaviour (like video frame allocated by user) during its
         * init stage.
         * Isn't a good practice, an alternative (more expensive) is
         * to add new controls at playback level (container of play
         * stream)
         * For time being survive with this trick, but extract extra
         * flags since the Encoding only is used in many places.
         */
	Context->VideoEncodingFlags = (Encoding & ENCODING_MASK);
	Encoding &= ~ENCODING_MASK;

	if (Encoding > VIDEO_ENCODING_PRIVATE)
		return -EINVAL;

	/* Allow the stream switch to go ahead
	   if (Context->VideoEncoding == (video_encoding_t)Encoding)
	   return 0;
	 */

	Context->VideoEncoding = (video_encoding_t) Encoding;

	switch (Context->VideoPlayState) {
	case STM_DVB_VIDEO_STOPPED:
		return 0;
	case STM_DVB_VIDEO_INCOMPLETE:
		/* At this point we have received the missing piece of information which will allow the
		 * stream to be fully populated so we can reissue the play. */
		return VideoIoctlPlay(Context);
	default:
		{
			int Result = 0;

			if (Encoding == VIDEO_ENCODING_AUTO) {
				DVB_ERROR
				    ("Cannot switch to undefined encoding after play has started\n");
				return -EINVAL;
			}

			Result = DvbStreamSwitch(Context->VideoStream,
						 VideoContent
						 [Context->VideoEncoding]);

			/*
			   if (Result == 0)
			   Result  = VideoIoctlSetId            (Context, Context->VideoId);
			 */
			return Result;
		}
	}
}

/*}}}*/
/*{{{  VideoIoctlFlush*/
static int VideoIoctlFlush(struct VideoDeviceContext_s *Context)
{
	int Result = 0;

	DVB_DEBUG("(video%d), %p\n", Context->Id, Context->VideoStream);

	/* If the stream is frozen it cannot be drained so an error is returned. */
	if ((Context->PlaySpeed == 0)
	    || (Context->PlaySpeed == DVB_SPEED_REVERSE_STOPPED))
		return -EPERM;

	if (mutex_lock_interruptible(&Context->vidwrite_mutex) != 0)
		return -ERESTARTSYS;
	mutex_unlock(&Context->vidops_mutex);	/* release lock so non-writing ioctls still work while draining */

	Result = DvbStreamDrain(Context->VideoStream, false);

	mutex_unlock(&Context->vidwrite_mutex);	/* release write lock so actions which have context lock can complete */
	mutex_lock(&Context->vidops_mutex);	/* reclaim lock so can be released by outer function */

	return Result;
}

/*}}}*/
/*{{{  VideoIoctlDiscontinuity*/
static int VideoIoctlDiscontinuity(struct VideoDeviceContext_s *Context,
				   video_discontinuity_t Discontinuity)
{
	int Result = 0;
	/*DVB_DEBUG("(video%d) %x\n", Context->Id, Discontinuity); */

	/* If the stream is frozen a discontinuity cannot be injected. */
	if (Context->VideoPlayState == STM_DVB_VIDEO_FREEZED)
		return -EFAULT;

	/* If speed is zero and the mutex is unavailable a discontinuity cannot be injected. */
	if ((mutex_is_locked(&Context->vidwrite_mutex) != 0) &&
	    ((Context->PlaySpeed == 0)
	     || (Context->PlaySpeed == DVB_SPEED_REVERSE_STOPPED)))
		return -EFAULT;

	if (mutex_lock_interruptible(&Context->vidwrite_mutex) != 0)
		return -ERESTARTSYS;
	mutex_unlock(&Context->vidops_mutex);	/* release lock so non-writing ioctls still work during discontinuity */

	Result =
	    DvbStreamDiscontinuity(Context->VideoStream,
				   (dvb_discontinuity_t) Discontinuity);

	mutex_unlock(&Context->vidwrite_mutex);
	mutex_lock(&Context->vidops_mutex);	/* reclaim lock so can be released by outer function */

	return Result;
}

/*}}}*/
/*{{{  VideoIoctlStep*/
static int VideoIoctlStep(struct VideoDeviceContext_s *Context)
{
	int Result = 0;

	DVB_DEBUG("(video%d)\n", Context->Id);
	/* Step can only be used when the speed is zero */
	if ((Context->VideoStream != NULL) && ((Context->PlaySpeed == 0)
					       || (Context->PlaySpeed ==
						   DVB_SPEED_REVERSE_STOPPED)))
		Result = DvbStreamStep(Context->VideoStream);

	return Result;
}

/*}}}*/
/*{{{  VideoIoctlSetPlayInterval*/
int VideoIoctlSetPlayInterval(struct VideoDeviceContext_s *Context,
			      video_play_interval_t * PlayInterval)
{
	DVB_DEBUG("(video%d)\n", Context->Id);

	memcpy(&Context->VideoPlayInterval, PlayInterval,
	       sizeof(video_play_interval_t));

	if (Context->VideoStream == NULL)
		return 0;

	return DvbStreamSetPlayInterval(Context->VideoStream,
					(dvb_play_interval_t *) PlayInterval);
}

/*}}}*/
/*{{{  VideoIoctlSetSyncGroup*/
static int VideoIoctlSetSyncGroup(struct VideoDeviceContext_s *Context,
				  unsigned int Group)
{
	int Result = 0;
	unsigned int sync_id;
	struct PlaybackDeviceContext_s * new_context = NULL;

	DVB_DEBUG("(video%d) Group %d\n", Context->Id, Group);
	if (Context->VideoPlayState != STM_DVB_VIDEO_STOPPED){
		DVB_ERROR("Video device not stopped - cannot set\n");
		return -EPERM;
	}

	sync_id = Group & ~VIDEO_SYNC_GROUP_MASK;

	switch(Group & VIDEO_SYNC_GROUP_MASK){
	case VIDEO_SYNC_GROUP_DEMUX:
		if (sync_id > Context->DvbContext->demux_dev_nb){
			DVB_ERROR("Sync group value is out of range\n");
			return -EINVAL;
		}

		new_context = &Context->DvbContext->PlaybackDeviceContext[sync_id];
		break;

	case VIDEO_SYNC_GROUP_AUDIO:
		if (sync_id > Context->DvbContext->audio_dev_nb){
			DVB_ERROR("Sync group value is out of range\n");
			return -EINVAL;
		}

		new_context = Context->DvbContext->AudioDeviceContext[sync_id].playback_context;
		break;

	case VIDEO_SYNC_GROUP_VIDEO:
		if (sync_id > Context->DvbContext->video_dev_nb){
			DVB_ERROR("Sync group value is out of range\n");
			return -EINVAL;
		}

		new_context = Context->DvbContext->VideoDeviceContext[sync_id].playback_context;
		break;

	default:
		DVB_ERROR("Invalid sync group value\n");
		return -EINVAL;
	}

	if (Context->playback_context == new_context)
		return 0;

	mutex_lock(&Context->playback_context->Playback_alloc_mutex);
	if (Context->playback_context->Playback){
		if (DvbPlaybackDelete(Context->playback_context->Playback) == 0) {
			DVB_DEBUG("Playback deleted successfully\n");
			Context->playback_context->Playback = NULL;
		}
	}
	mutex_unlock(&Context->playback_context->Playback_alloc_mutex);

	Context->playback_context = new_context;

	return Result;
}
/*}}}*/

/*{{{  VideoIoctlCommand*/
static int VideoIoctlCommand(struct VideoDeviceContext_s *Context,
			     struct video_command *VideoCommand)
{
	int Result = 0;
	int ApplyLater = 0;

	DVB_DEBUG("(video%d)\n", Context->Id);
	if (VideoCommand->cmd == VIDEO_CMD_SET_OPTION) {
		if (VideoCommand->option.option >= DVB_OPTION_MAX) {
			DVB_ERROR("Option %d, out of range\n",
				  VideoCommand->option.option);
			return -EINVAL;
		}
		/*
		 *  Determine if the command should be applied to the playback or just the video stream.  Check if
		 *  the stream or playback is valid.  If so apply the option to the stream.  If not check to see if
		 *  there is a valid playback.  If so, apply the option to the playback if appropriate.  If not, save
		 *  command for later.
		 */

		DVB_DEBUG("Option %d, Value 0x%x\n",
			  VideoCommand->option.option,
			  VideoCommand->option.value);

		if ((VideoCommand->option.option ==
		     DVB_OPTION_DISCARD_LATE_FRAMES)
		    || (VideoCommand->option.option ==
			DVB_OPTION_VIDEO_START_IMMEDIATE)
		    || (VideoCommand->option.option ==
			DVB_OPTION_SYMMETRIC_JUMP_DETECTION)
		    || (VideoCommand->option.option ==
			DVB_OPTION_PTS_FORWARD_JUMP_DETECTION_THRESHOLD)
		    || (VideoCommand->option.option == DVB_OPTION_LIVE_PLAY)
		    || (VideoCommand->option.option == DVB_OPTION_PACKET_INJECTOR)
		    || (VideoCommand->option.option ==
			DVB_OPTION_CTRL_PLAYBACK_LATENCY)
		    || (VideoCommand->option.option ==
			DVB_OPTION_CTRL_VIDEO_DISCARD_FRAMES)
		    || (VideoCommand->option.option ==
			DVB_OPTION_MASTER_CLOCK)
		    || (VideoCommand->option.option ==
			DVB_OPTION_CTRL_VIDEO_MEMORY_PROFILE)
			||  (VideoCommand->option.option ==
			DVB_OPTION_CTRL_CONTAINER_FRAMERATE)) {
		    if (Context->playback_context->Playback != NULL)
			Result =
			    DvbPlaybackSetOption(Context->playback_context->Playback,
							 VideoCommand->
							 option.option,
							 (unsigned int)
							 VideoCommand->
							 option.value);
			else
				ApplyLater = 1;
		} else {
			if (Context->VideoStream != NULL)
				Result =
				    DvbStreamSetOption(Context->VideoStream,
						       VideoCommand->
						       option.option,
						       (unsigned int)
						       VideoCommand->
						       option.value);
			else
				ApplyLater = 1;
		}

		if (ApplyLater) {
			Context->PlayOption[VideoCommand->option.option] = VideoCommand->option.option;	/* save for later */
			Context->PlayValue[VideoCommand->option.option] =
			    VideoCommand->option.value;
			if (Context->playback_context->Playback != NULL)	/* apply to playback if appropriate */
				DvbPlaybackInitOption(Context->playback_context->Playback, Context->PlayOption, Context->PlayValue);
		}

	}
	if (Result != 0)
		return Result;

	return Result;
}

/*}}}*/
/*{{{  VideoIoctlSetClockDataPoint*/
int VideoIoctlSetClockDataPoint(struct VideoDeviceContext_s *Context,
				video_clock_data_point_t * ClockData)
{
	DVB_DEBUG("(video%d)\n", Context->Id);

	if (Context->playback_context->Playback == NULL)
		return -ENODEV;

	return DvbPlaybackSetClockDataPoint(Context->playback_context->Playback,
					    (dvb_clock_data_point_t *)
					    ClockData);
}

/*}}}*/
/*{{{  VideoIoctlSetTimeMapping*/
int VideoIoctlSetTimeMapping(struct VideoDeviceContext_s *Context,
			     video_time_mapping_t * TimeMapping)
{
	int Result;

	DVB_DEBUG("(video%d)\n", Context->Id);
	if ((Context->playback_context->Playback == NULL) || (Context->VideoStream == NULL))
		return -ENODEV;

	Result =
	    DvbStreamSetOption(Context->VideoStream,
			       DVB_OPTION_EXTERNAL_TIME_MAPPING,
			       DVB_OPTION_VALUE_ENABLE);
	if (Result) {
		printk(KERN_ERR "%s: failed to set stream option (%d)\n",
		       __func__, Result);
		return Result;
	}

	return DvbPlaybackSetNativePlaybackTime(Context->playback_context->Playback,
						TimeMapping->native_stream_time,
						TimeMapping->system_presentation_time);
}

/*}}}*/
/*{{{  VideoIoctlGetClockDataPoint*/
int VideoIoctlGetClockDataPoint(struct VideoDeviceContext_s *Context,
				video_clock_data_point_t * ClockData)
{
	DVB_DEBUG("(video%d)\n", Context->Id);

	if (Context->playback_context->Playback == NULL)
		return -ENODEV;

	return DvbPlaybackGetClockDataPoint(Context->playback_context->Playback,
					    (dvb_clock_data_point_t *)
					    ClockData);
}

/*}}}*/
/*}}}*/
/*{{{  VideoOpen*/
static int VideoOpen(struct inode *Inode, struct file *File)
{
	struct dvb_device *DvbDevice = (struct dvb_device *)File->private_data;
	struct VideoDeviceContext_s *Context =
	    (struct VideoDeviceContext_s *)DvbDevice->priv;
	unsigned int i;
	int Error;

	DVB_DEBUG("Id %d\n", Context->Id);

	mutex_lock(&Context->viddev_mutex);

	Error = dvb_generic_open(Inode, File);
	if (Error < 0) {
		mutex_unlock(&Context->viddev_mutex);
		return Error;
	}

	if ((File->f_flags & O_ACCMODE) != O_RDONLY) {
		Context->VideoPlayState = STM_DVB_VIDEO_STOPPED;
		Context->VideoEvents.Write = 0;
		Context->VideoEvents.Read = 0;
		Context->VideoEvents.Overflow = 0;
		for (i = 0; i < DVB_OPTION_MAX; i++)
			Context->PlayOption[i] = DVB_OPTION_VALUE_INVALID;
		Context->VideoOpenWrite = 1;
		VideoResetStats(Context);
	}

	mutex_unlock(&Context->viddev_mutex);

	return 0;
}

/*}}}*/
/*{{{  VideoRelease*/
static int VideoRelease(struct inode *Inode, struct file *File)
{
	struct dvb_device *DvbDevice = (struct dvb_device *)File->private_data;
	struct VideoDeviceContext_s *Context =
	    (struct VideoDeviceContext_s *)DvbDevice->priv;
	struct stm_dvb_demux_s *stm_demux;
	int Result;

	DVB_DEBUG("Id %d\n", Context->Id);

	/*
	 * In case of tunneled decode, we would like to modify the
	 * demuxer playback context, in case, we succeed in deleting
	 * it. This allows demux to avoid using a bad playback
	 * associated with the playback context. Since, the locking
	 * is always: demux, audio/video, so we maintain here as well
	 */
	stm_demux = (struct stm_dvb_demux_s *)Context->playback_context->stm_demux;
	if (stm_demux && (Context->VideoState.stream_source == VIDEO_SOURCE_DEMUX))
		down_write(&stm_demux->rw_sem);

	mutex_lock(&Context->viddev_mutex);

	if ((File->f_flags & O_ACCMODE) != O_RDONLY) {

		mutex_lock(&Context->vidops_mutex);

		Result = VideoStop(Context,DVB_OPTION_VALUE_ENABLE,true);

		mutex_unlock(&Context->vidops_mutex);

		if (Result)
			printk(KERN_ERR "Failed to stop the video\n");

		mutex_lock(&Context->playback_context->Playback_alloc_mutex);
		/* Try to delete the playback - it will fail if someone is still using it */
		if (Context->playback_context->Playback != NULL) {
			if (DvbPlaybackDelete(Context->playback_context->Playback) == 0) {
				DVB_DEBUG("Playback deleted successfully\n");
				Context->playback_context->Playback = NULL;
			}
		}
		mutex_unlock(&Context->playback_context->Playback_alloc_mutex);

		/*
		 * If we succeeded in deleting the playback, then, we reset the demux
		 * context too
		 */
		if (!Context->playback_context->Playback) {
			if (stm_demux && (Context->VideoState.stream_source == VIDEO_SOURCE_DEMUX))
				stm_demux->PlaybackContext = NULL;
		}

		Context->StreamType = STREAM_TYPE_TRANSPORT;
		Context->PlaySpeed = DVB_SPEED_NORMAL_PLAY;
		Context->VideoPlayInterval.start = DVB_TIME_NOT_BOUNDED;
		Context->VideoPlayInterval.end = DVB_TIME_NOT_BOUNDED;

		VideoInit(Context);
	}

	Result = dvb_generic_release(Inode, File);

	mutex_unlock(&Context->viddev_mutex);

	if (stm_demux && (Context->VideoState.stream_source == VIDEO_SOURCE_DEMUX))
		up_write(&stm_demux->rw_sem);

	return Result;
}

/*}}}*/
/*{{{  VideoIoctl*/
static int VideoIoctl(
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 35))
			     struct inode *Inode,
#endif
			     struct file *File,
			     unsigned int IoctlCode, void *Parameter)
{
	struct dvb_device *DvbDevice = (struct dvb_device *)File->private_data;
	struct VideoDeviceContext_s *Context =
	    (struct VideoDeviceContext_s *)DvbDevice->priv;
	int Result = 0;

	/*DVB_DEBUG("VideoIoctl : Ioctl %08x\n", IoctlCode); */

	/*
	   if (((File->f_flags & O_ACCMODE) == O_RDONLY) &&
	   (IoctlCode != VIDEO_GET_STATUS) && (IoctlCode != VIDEO_GET_EVENT) && (IoctlCode != VIDEO_GET_SIZE))
	   return -EPERM;
	 */
	if ((File->f_flags & O_ACCMODE) == O_RDONLY) {
		switch (IoctlCode) {
		case VIDEO_GET_STATUS:
		case VIDEO_GET_EVENT:
		case VIDEO_GET_SIZE:
		case VIDEO_GET_FRAME_RATE:
			/*  Not allowed as they require an active player
			   case    VIDEO_GET_PTS:
			   case    VIDEO_GET_PLAY_TIME:
			   case    VIDEO_GET_FRAME_COUNT:
			 */
			break;
		default:
			return -EPERM;
		}
	}

	if (!Context->VideoOpenWrite)	/* Check to see that somebody has the device open for write */
		return -EBADF;

	mutex_lock(&Context->vidops_mutex);

	switch (IoctlCode) {
	case VIDEO_STOP:
		Result = VideoIoctlStop(Context, (unsigned int)Parameter);
		break;
	case VIDEO_PLAY:
		Result = VideoIoctlPlay(Context);
		break;
	case VIDEO_FREEZE:
		Result = VideoIoctlFreeze(Context);
		break;
	case VIDEO_CONTINUE:
		Result = VideoIoctlContinue(Context);
		break;
	case VIDEO_SELECT_SOURCE:
		Result =
		    VideoIoctlSelectSource(Context,
					   (video_stream_source_t) Parameter);
		break;
	case VIDEO_SET_BLANK:
		Result = VideoIoctlSetBlank(Context, (unsigned int)Parameter);
		break;
	case VIDEO_GET_STATUS:
		Result =
		    VideoIoctlGetStatus(Context,
					(struct video_status *)Parameter);
		break;
	case VIDEO_GET_EVENT:
		Result =
		    VideoIoctlGetEvent(Context, (struct video_event *)Parameter,
				       File->f_flags);
		break;
	case VIDEO_GET_SIZE:
		Result = VideoIoctlGetSize(Context, (video_size_t *) Parameter);
		break;
	case VIDEO_SET_DISPLAY_FORMAT:
		Result =
		    VideoIoctlSetDisplayFormat(Context, (video_displayformat_t)
					       Parameter);
		break;
	case VIDEO_STILLPICTURE:
		Result =
		    VideoIoctlStillPicture(Context,
					   (struct video_still_picture *)
					   Parameter);
		break;
	case VIDEO_FAST_FORWARD:
		Result = VideoIoctlFastForward(Context, (int)Parameter);
		break;
	case VIDEO_SLOWMOTION:
		Result = VideoIoctlSlowMotion(Context, (int)Parameter);
		break;
	case VIDEO_GET_CAPABILITIES:
		Result = VideoIoctlGetCapabilities(Context, (int *)Parameter);
		break;
	case VIDEO_GET_PTS:
		Result =
		    VideoIoctlGetPts(Context,
				     (unsigned long long int *)Parameter);
		break;
	case VIDEO_GET_FRAME_COUNT:
		Result =
		    VideoIoctlGetFrameCount(Context, (unsigned long long int *)
					    Parameter);
		break;
	case VIDEO_SET_ID:
		Result = VideoIoctlSetId(Context, (int)Parameter);
		break;
	case VIDEO_CLEAR_BUFFER:
		Result = VideoIoctlClearBuffer(Context);
		break;
	case VIDEO_SET_STREAMTYPE:
		Result =
		    VideoIoctlSetStreamType(Context, (unsigned int)Parameter);
		break;
	case VIDEO_SET_FORMAT:
		Result = VideoIoctlSetFormat(Context, (unsigned int)Parameter);
		break;
	case VIDEO_SET_SYSTEM:
		Result =
		    VideoIoctlSetSystem(Context, (video_system_t) Parameter);
		break;
	case VIDEO_SET_HIGHLIGHT:
		Result =
		    VideoIoctlSetHighlight(Context,
					   (video_highlight_t *) Parameter);
		break;
	case VIDEO_SET_SPU:
		Result = VideoIoctlSetSpu(Context, (video_spu_t *) Parameter);
		break;
	case VIDEO_SET_SPU_PALETTE:
		Result =
		    VideoIoctlSetSpuPalette(Context,
					    (video_spu_palette_t *) Parameter);
		break;
	case VIDEO_GET_NAVI:
		Result =
		    VideoIoctlGetNavi(Context, (video_navi_pack_t *) Parameter);
		break;
	case VIDEO_SET_ATTRIBUTES:
		Result =
		    VideoIoctlSetAttributes(Context,
					    (video_attributes_t *) Parameter);
		break;
	case VIDEO_GET_FRAME_RATE:
		Result = VideoIoctlGetFrameRate(Context, (int *)Parameter);
		break;
	case VIDEO_SET_ENCODING:
		Result =
		    VideoIoctlSetEncoding(Context, (unsigned int)Parameter);
		break;
	case VIDEO_FLUSH:
		Result = VideoIoctlFlush(Context);
		break;
	case VIDEO_SET_SPEED:
		Result = VideoIoctlSetSpeed(Context, (int)Parameter, 1);
		break;
	case VIDEO_DISCONTINUITY:
		Result =
		    VideoIoctlDiscontinuity(Context,
					    (video_discontinuity_t) Parameter);
		break;
	case VIDEO_STEP:
		Result = VideoIoctlStep(Context);
		break;
	case VIDEO_SET_PLAY_INTERVAL:
		Result =
		    VideoIoctlSetPlayInterval(Context, (video_play_interval_t *)
					      Parameter);
		break;
	case VIDEO_SET_SYNC_GROUP:
		Result =
		    VideoIoctlSetSyncGroup(Context, (unsigned int)Parameter);
		break;
	case VIDEO_COMMAND:
		Result =
		    VideoIoctlCommand(Context,
				      (struct video_command *)Parameter);
		break;
	case VIDEO_GET_PLAY_TIME:
		Result =
		    VideoIoctlGetPlayTime(Context,
					  (video_play_time_t *) Parameter);
		break;
	case VIDEO_GET_PLAY_INFO:
		Result =
		    VideoIoctlGetPlayInfo(Context,
					  (video_play_info_t *) Parameter);
		break;
	case VIDEO_SET_CLOCK_DATA_POINT:
		Result =
		    VideoIoctlSetClockDataPoint(Context,
						(video_clock_data_point_t *)
						Parameter);
		break;
	case VIDEO_SET_TIME_MAPPING:
		Result =
		    VideoIoctlSetTimeMapping(Context, (video_time_mapping_t *)
					     Parameter);
		break;
	case VIDEO_GET_CLOCK_DATA_POINT:
		Result =
		    VideoIoctlGetClockDataPoint(Context,
						(video_clock_data_point_t *)
						Parameter);
		break;

	default:
		DVB_ERROR("Error - invalid ioctl %08x\n", IoctlCode);
		Result = -ENOIOCTLCMD;
	}

	mutex_unlock(&Context->vidops_mutex);

	return Result;
}

/*}}}*/

/**
 * dvb_video_write
 * This function is the /dev/videoX write call. This is used to push
 * data to player2.
 */
static ssize_t dvb_video_write(struct file *file, const char __user * buffer,
						  size_t count, loff_t * ppos)
{
	struct dvb_device *dvbdev = (struct dvb_device *)file->private_data;
	struct VideoDeviceContext_s *vid_ctx = dvbdev->priv;
	struct DvbStreamContext_s *stream_ctx = vid_ctx->VideoStream;
	char *usr_buf = (char *)buffer;
	int ret = 0;

	if (!count)
		goto write_invalid_state;

	mutex_lock(&vid_ctx->vidwrite_mutex);

	/*
	 * Injecting data to streaming engine is not allowed if the device
	 * is connected to demux.
	 */
	if (vid_ctx->VideoState.stream_source == VIDEO_SOURCE_DEMUX) {
		DVB_ERROR("Video stream source must be VIDEO_SOURCE_MEMORY\n");
		ret = -EPERM;
		goto write_invalid_state;
	}


	if (!vid_ctx->VideoStream) {
		DVB_ERROR("No video stream exists, issue VIDEO_PLAY first\n");
		ret = -EINVAL;
		goto write_invalid_state;
	}

	if (vid_ctx->VideoPlayState == STM_DVB_VIDEO_STOPPED) {
		DVB_ERROR("Video not started, start the video first\n");
		ret = -EINVAL;
		goto write_invalid_state;
	}


#ifdef CONFIG_HCE
/*
 * VSOC - DvbStreamGetFirstBuffer & DvbStreamInject use physical address
 */
	usr_buf = bigphysarea_alloc(count);
	if (!usr_buf) {
		DVB_ERROR("Out of memory for HCE video write\n");
		ret = -ENOMEM;
		goto write_invalid_state;
	}

	ret = copy_from_user(usr_buf, buffer, count);
	if (ret) {
		DVB_ERROR("Failed to copy user buffer\n");
		bigphysarea_free_pages(usr_buf);
		goto write_invalid_state;
	}
#endif

	if (vid_ctx->VideoPlayState == STM_DVB_VIDEO_INCOMPLETE) {
		DvbStreamGetFirstBuffer(vid_ctx->VideoStream, usr_buf, count);
		DvbStreamIdentifyVideo(stream_ctx, &(vid_ctx->VideoEncoding));
		VideoIoctlPlay(vid_ctx);

		if (vid_ctx->VideoPlayState == STM_DVB_VIDEO_INCOMPLETE) {
			vid_ctx->VideoPlayState = STM_DVB_VIDEO_STOPPED;
			goto write_invalid_state;
		}
	}

	ret = dvb_stream_inject(stream_ctx, usr_buf, count);

#ifdef CONFIG_HCE
/*
 * VSOC - DvbStreamGetFirstBuffer & DvbStreamInject use physical address
 */
	bigphysarea_free_pages(usr_buf);
#endif

	mutex_unlock(&vid_ctx->vidwrite_mutex);

	return ret;

write_invalid_state:
	mutex_unlock(&vid_ctx->vidwrite_mutex);
	return ret;
}

/*}}}*/
/*{{{  VideoPoll*/
static unsigned int VideoPoll(struct file *File, poll_table * Wait)
{
	struct dvb_device *DvbDevice = (struct dvb_device *)File->private_data;
	struct VideoDeviceContext_s *Context =
	    (struct VideoDeviceContext_s *)DvbDevice->priv;
	unsigned int Mask = 0;

	/*DVB_DEBUG ("(video%d)\n", Context->Id); */

	poll_wait(File, &Context->VideoEvents.WaitQueue, Wait);

	if (Context->VideoEvents.Write != Context->VideoEvents.Read)
		Mask = POLLPRI;

	if (((File->f_flags & O_ACCMODE) != O_RDONLY)
	    && (Context->VideoStream != NULL)) {
		if ((Context->VideoPlayState == STM_DVB_VIDEO_PLAYING)
		    || (Context->VideoPlayState == STM_DVB_VIDEO_INCOMPLETE)) {
			Mask |= (POLLOUT | POLLWRNORM);
		}
	}
	return Mask;
}

/*}}}*/
/*{{{  VideoStreamEventSubscriptionCreate */
int VideoStreamEventSubscriptionCreate(struct VideoDeviceContext_s *Context)
{
	uint32_t message_mask;
	uint32_t event_mask;
	int Result = 0;

	/* mask of expected video stream event */
	event_mask = STM_SE_PLAY_STREAM_EVENT_FIRST_FRAME_ON_DISPLAY
	    | STM_SE_PLAY_STREAM_EVENT_FRAME_DECODED_LATE
	    | STM_SE_PLAY_STREAM_EVENT_DATA_DELIVERED_LATE
	    | STM_SE_PLAY_STREAM_EVENT_FATAL_ERROR
	    | STM_SE_PLAY_STREAM_EVENT_FATAL_HARDWARE_FAILURE
	    | STM_SE_PLAY_STREAM_EVENT_MESSAGE_READY
	    | STM_SE_PLAY_STREAM_EVENT_FRAME_DECODED
	    | STM_SE_PLAY_STREAM_EVENT_FRAME_RENDERED
	    | STM_SE_PLAY_STREAM_EVENT_STREAM_IN_SYNC
	    | STM_SE_PLAY_STREAM_EVENT_END_OF_STREAM
	    | STM_SE_PLAY_STREAM_EVENT_FRAME_STARVATION
	    | STM_SE_PLAY_STREAM_EVENT_FRAME_SUPPLIED;


	if ((Context == NULL) || (Context->VideoStream == NULL)
	    || (Context->VideoStream->Handle == NULL))
		return -EINVAL;

	/* If events are already subscribed, don't do anything */
	if (Context->VideoMsgSubscription && Context->VideoEventSubscription)
		return 0;

	/* create video stream event subscription and handler
	   (only one event handle at a time)    */
	Result =
	    DvbEventSubscribe(Context->VideoStream, Context, event_mask,
			      VideoSetEvent, &Context->VideoEventSubscription);
	if (Result < 0) {
		Context->VideoEventSubscription = NULL;
		BACKEND_ERROR
		    ("Failed to set Video stream Event handler: Context %x \n",
		     (uint32_t) Context);
		return -EINVAL;
	}

	/* mask of expected video stream message */
	message_mask = STM_SE_PLAY_STREAM_MSG_MESSAGE_LOST
	    | STM_SE_PLAY_STREAM_MSG_VIDEO_PARAMETERS_CHANGED
	    | STM_SE_PLAY_STREAM_MSG_FRAME_RATE_CHANGED
	    | STM_SE_PLAY_STREAM_MSG_STREAM_UNPLAYABLE
	    | STM_SE_PLAY_STREAM_MSG_TRICK_MODE_CHANGE
	    | STM_SE_PLAY_STREAM_MSG_VSYNC_OFFSET_MEASURED;

	/* create video stream message subscription */
	Result =
	    DvbStreamMessageSubscribe(Context->VideoStream, message_mask, 10,
				      &Context->VideoMsgSubscription);
	if (Result < 0) {
		Context->VideoMsgSubscription = NULL;
		BACKEND_ERROR
		    ("Failed to create video stream message subscription: Context %x \n",
		     (uint32_t) Context);
		return -EINVAL;
	}

	return Result;
}

/*}}}*/
/*{{{  VideoStreamEventSubscriptionDelete */
int VideoStreamEventSubscriptionDelete(struct VideoDeviceContext_s *Context)
{
	int Result = 0;

	if ((Context == NULL) || (Context->VideoStream == NULL)
	    || (Context->VideoStream->Handle == NULL))
		return -EINVAL;

	/* delete video stream event subscription */
	if (Context->VideoEventSubscription) {
		Result =
			DvbEventUnsubscribe(Context->VideoStream,
					Context->VideoEventSubscription);
		Context->VideoEventSubscription = NULL;
		if (Result < 0) {
			BACKEND_ERROR
				("Failed to reset Video stream Event handler: Context %x \n",
				 (uint32_t) Context);
			return -EINVAL;
		}
	}

	/* delete video stream message subscription */
	if (Context->VideoMsgSubscription) {
		Result =
			DvbStreamMessageUnsubscribe(Context->VideoStream,
					Context->VideoMsgSubscription);
		Context->VideoMsgSubscription = NULL;
		if (Result < 0) {
			BACKEND_ERROR
				("Failed to create video stream message subscription: Context %x \n",
				 (uint32_t) Context);
			return -EINVAL;
		}
	}

	return Result;
}

/**
 * stm_video_enqueue_event() - enqueues the video event
 * @ev_list : event information from the decoder context
 * @event   : event to enqueue
 * Enqueues the video events for retrieval by application.
 */
static void stm_video_enqueue_event(struct VideoEvent_s *ev_list, struct video_event *event)
{
	unsigned int next;

	mutex_lock(&ev_list->Lock);

	next = (ev_list->Write + 1) % MAX_VIDEO_EVENT;

	if (next == ev_list->Read) {
		ev_list->Overflow = true;
		ev_list->Read = (ev_list->Read + 1) % MAX_VIDEO_EVENT;
	}

	memcpy(&(ev_list->Event[ev_list->Write]), event, sizeof(*event));

	ev_list->Write = next;

	mutex_unlock(&ev_list->Lock);

	wake_up_interruptible(&ev_list->WaitQueue);
}

/*{{{  VideoSetMessageEvent */
void VideoSetMessageEvent( struct VideoDeviceContext_s *Context,
			   time_t timestamp )
{
	struct VideoEvent_s *ev_list;
	struct video_event event;
	video_size_t *size;
	bool event_to_notify = true;
	struct stm_se_play_stream_msg_s StreamMsg;
	stm_se_play_stream_video_parameters_t *param;
	int ret = 0;

	ev_list = &Context->VideoEvents;

	/* we can receive more than one message,
	 * so need to loop to purge message queue*/
	while (1) {
		event_to_notify = true;
		memset(&event, 0, sizeof(struct video_event));

		ret = DvbStreamGetMessage(Context->VideoStream,
					  Context->VideoMsgSubscription,
					  &StreamMsg);
		if (ret) {
			if (ret != -EAGAIN)
				DVB_ERROR("Stream get message failed (%d)\n",
					  ret);
			break;
		}

		event.timestamp = timestamp;

		switch(StreamMsg.msg_id) {
		case STM_SE_PLAY_STREAM_MSG_VIDEO_PARAMETERS_CHANGED:
		{
			param = &StreamMsg.u.video_parameters;
			size = &Context->VideoSize;

			/*
			 * With this single stream message, SE is pushing
			 * video parameters (height, width, framerate) etc.
			 * Since, LDVB has a union of framerate with other
			 * video parameters, so, we need to push 2 events
			 * to the upper layer, by enqueing 2 events here.
			 */
			if (size->w != param->width ||
				size->h != param->height ||
				size->aspect_ratio != (video_format_t)param->aspect_ratio) {

				/*
				 * NOTE: For the moment, the LDVB enum for
				 * aspect ratio and SE are equivalent, so,
				 * direct comparison yield correct results
				 */

				/*
				 * Setup the event data
				 */
				event.type = VIDEO_EVENT_SIZE_CHANGED;
				event.u.size.w = param->width;
				event.u.size.h = param->height;
				event.u.size.aspect_ratio = param->aspect_ratio;

				/*
				 * Setup the video context data
				 */
				size->w = param->width;
				size->h = param->height;
				size->aspect_ratio = param->aspect_ratio;

				Context->VideoState.video_format = param->aspect_ratio;
				Context->PixelAspectRatio.Numerator = param->pixel_aspect_ratio_numerator;
				Context->PixelAspectRatio.Denominator = param->pixel_aspect_ratio_denominator;

				/*
				 * Enqueue the event information in the event queue
				 */
				stm_video_enqueue_event(ev_list, &event);
			}


			/*
			 * Process for the framerate event
			 */
			if (Context->FrameRate != param->frame_rate) {

				event.type = VIDEO_EVENT_FRAME_RATE_CHANGED;
				event.u.frame_rate = param->frame_rate;

				Context->FrameRate = param->frame_rate;

				/*
				 * Enqueue the event information in the event queue
				 */
				stm_video_enqueue_event(ev_list, &event);
			}

			/*
			 * Since the event is already notified, so, no need to enqueue
			 * again. This is placed here, as it is very likely, that, there
			 * is no spurious event from SE, and in case it is, no need to notify
			 */
			event_to_notify = false;

			break;
		}

		/* The code below uses the frame_rate to store the unplayable
		 * reason as this event is not defined in the standard
		 * structure */
		case STM_SE_PLAY_STREAM_MSG_STREAM_UNPLAYABLE:
			event.type = VIDEO_EVENT_STREAM_UNPLAYABLE;
			event.u.frame_rate = (unsigned int)(StreamMsg.u.reason);
			break;

		/* The code below uses the frame_rate to store the unplayable
		 * trick_mode_domain as this event is not defined in the
		 * standard structure */
		case STM_SE_PLAY_STREAM_MSG_TRICK_MODE_CHANGE:
			event.type = VIDEO_EVENT_TRICK_MODE_CHANGE;
			event.u.frame_rate = StreamMsg.u.trick_mode_domain;
			break;

		/* The code below uses the frame_rate to store the lost_count
		 * as this event is not defined in the standard structure */
		case STM_SE_PLAY_STREAM_MSG_MESSAGE_LOST:
			event.type = VIDEO_EVENT_LOST;
			event.u.frame_rate = StreamMsg.u.lost_count;
			break;

		default:
			event_to_notify = false;
			DVB_DEBUG("Unknown SE message received (%d)\n",
				  StreamMsg.msg_id);
			break;
		}

		if (!event_to_notify)
			continue;

		stm_video_enqueue_event(ev_list, &event);
	}
}
/*}}}*/

/*{{{  VideoSetEvent*/
void VideoSetEvent(unsigned int number_of_events, stm_event_info_t * eventsInfo)
{
	struct VideoEvent_s *ev_list;
	struct video_event event;
	bool event_to_notify;
	struct VideoDeviceContext_s *Context;

	/* retrieve device context from event subscription handler cookie */
	Context = eventsInfo->cookie;

	ev_list = &Context->VideoEvents;

	while(number_of_events){
		event_to_notify = true;

		memset(&event, 0, sizeof(struct video_event));
		event.timestamp = (time_t) eventsInfo->timestamp;

		switch(eventsInfo->event.event_id) {
		case STM_SE_PLAY_STREAM_EVENT_MESSAGE_READY:
			VideoSetMessageEvent(Context,
					     (time_t) eventsInfo->timestamp);
			event_to_notify = false;
			/* Nothing to notify here since already done within
			 * VideoSetMessageEvent function */
			break;

		case STM_SE_PLAY_STREAM_EVENT_FRAME_DECODED:
			event.type = VIDEO_EVENT_FRAME_DECODED;
			break;

		case STM_SE_PLAY_STREAM_EVENT_FRAME_RENDERED:
			event.type = VIDEO_EVENT_FRAME_RENDERED;
			break;

		case STM_SE_PLAY_STREAM_EVENT_FIRST_FRAME_ON_DISPLAY:
			event.type = VIDEO_EVENT_FIRST_FRAME_ON_DISPLAY;
			break;

		case STM_SE_PLAY_STREAM_EVENT_FRAME_DECODED_LATE:
			event.type = VIDEO_EVENT_FRAME_DECODED_LATE;
			break;

		case STM_SE_PLAY_STREAM_EVENT_DATA_DELIVERED_LATE:
			event.type = VIDEO_EVENT_DATA_DELIVERED_LATE;
			break;

		case STM_SE_PLAY_STREAM_EVENT_FATAL_ERROR:
			event.type = VIDEO_EVENT_FATAL_ERROR;
			break;

		case STM_SE_PLAY_STREAM_EVENT_FATAL_HARDWARE_FAILURE:
			event.type = VIDEO_EVENT_FATAL_HARDWARE_FAILURE;
			break;

		case STM_SE_PLAY_STREAM_EVENT_STREAM_IN_SYNC:
			event.type = VIDEO_EVENT_STREAM_IN_SYNC;
			break;

                case STM_SE_PLAY_STREAM_EVENT_END_OF_STREAM:
                        event.type = VIDEO_EVENT_END_OF_STREAM;
                        break;

                case STM_SE_PLAY_STREAM_EVENT_FRAME_STARVATION:
                        event.type      = VIDEO_EVENT_FRAME_STARVATION;
                        break;

                case STM_SE_PLAY_STREAM_EVENT_FRAME_SUPPLIED:
                        event.type      = VIDEO_EVENT_FRAME_SUPPLIED;
                        break;

		default:
			DVB_DEBUG("Unknown SE event received (%d)\n",
				  eventsInfo->event.event_id);
			event_to_notify = false;
		}

		if (!event_to_notify){
			number_of_events--;
			eventsInfo++;
			continue;
		}

		stm_video_enqueue_event(ev_list, &event);

		number_of_events--;
		eventsInfo++;
	}
}

/*}}}*/
