/************************************************************************
Copyright (C) 2007, 2009, 2010, 2014 STMicroelectronics. All Rights Reserved.

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
 * Implementation of hdmirx v4l2 control device
************************************************************************/
#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/slab.h>
#include <media/v4l2-subdev.h>
#include <media/v4l2-event.h>
#include "stm_se.h"
#include "pseudo_mixer.h"

#include "stmedia.h"
#include "linux/stm/stmedia_export.h"
#include "linux/dvb/dvb_v4l2_export.h"
#include "stm_hdmirx.h"

#include "stm_v4l2_video_capture.h"
#include "stm_v4l2_audio_decoder.h"
#include <linux/stm_v4l2_hdmi_export.h>

#define MAX_NO_PORTS 4

/* The HDMIRX entity pad info */
#define HDMIRX_SNK_PAD 0
#define HDMIRX_VID_SRC_PAD 1
#define HDMIRX_AUD_SRC_PAD 2
#define HDMIRX_EVENT_QUEUE_DEPTH	1

/* the HDMIRx v4l driver status */
#define HDMIRX_PORT_CONNECTED	0x01
#define HDMIRX_PLANE_CONNECTED	0x02
#define HDMIRX_SIGNAL_PRESENT	0x04

/* this number is assigned corresponding to the PIX_HDMIR enum in VIBE*/
#define STM_SOURCE_TYPE_HDMI    0

#define _warn(a,b...)  printk(KERN_WARNING "Warning:<%s:%d>"  a, __FUNCTION__ ,__LINE__,##b)
#define _info(a,b...)  printk(KERN_INFO "Info:<%s:%d>"  a, __FUNCTION__ ,__LINE__,##b)
#define _err(a,b...)  printk(KERN_ERR "Err:<%s:%d>"  a, __FUNCTION__ ,__LINE__,##b)
#define _dbg(a,b...)  printk(KERN_DEBUG "Debug:<%s:%d>"  a, __FUNCTION__ ,__LINE__,##b)

/* Supported coding types by SPDIFIn codec in SE*/
#define supported_coding_types(coding_type) \
	(coding_type == STM_HDMIRX_AUDIO_CODING_TYPE_PCM) || \
	(coding_type == STM_HDMIRX_AUDIO_CODING_TYPE_AC3) || \
	(coding_type == STM_HDMIRX_AUDIO_CODING_TYPE_AAC) || \
	(coding_type == STM_HDMIRX_AUDIO_CODING_TYPE_DDPLUS) || \
	(coding_type == STM_HDMIRX_AUDIO_CODING_TYPE_DTS) || \
	(coding_type == STM_HDMIRX_AUDIO_CODING_TYPE_DTS_HD)

#define MUTEX_INTERRUPTIBLE(mutex)		\
	if (mutex_lock_interruptible(mutex))	\
		return -ERESTARTSYS;

#define ROUTE_EVT_HOTPLUG	0x1
#define ROUTE_EVT_VIDEO		0x2
#define ROUTE_EVT_AUDIO		0x3
#define ROUTE_EVT_SIG		0x4

/** struct hdmiport_s - HDMI Rx port structure.*/
struct hdmiport_s {
	/*! The port handle returned from the port_open call to the HDMI KPI driver. */
	stm_hdmirx_port_h port_hdl;
	/*! The port instance. This identifies the HDMI port number. */
	int port_id;
	/*! Reference to the vidextin_hdmirx_ctx_s structure. */
	struct vidextin_hdmirx_ctx_s *extctx;
};

/** HDMI Route structure in the HDMI receiver
 *  There will as many instances as the HDMI routes in the system. TODO extend for Audio
 */
struct hdmirx_ctx_s {
	/*!The shared hdmi device structure */
	struct hdminput_shared_s *hdmidev;
	struct video_device *subdev_viddev;
	/*!The v4l2 sub-device structure. This will be initialized
	   when  the v4l subdevice for HDMIRx route is created */
	struct v4l2_subdev hdmirx_sd;
	/*! Stores the pad info. For the hdmirx there will 1 sink
	   pad and 2 source pad (Video and Audio). */
	struct media_pad pads[3];
	/*!Stores the HDMIRx route handle when the Route Open call is made. */
	stm_hdmirx_route_h rxhdl;
	/*! the instance number of the Object. Each Instance
	   will represent a HDMI Route present on the platform. */
	int route_inst;
	/*! event subscription handle */
	stm_event_subscription_h rxevtsubs;
	/*! protect the vidsetup from concurrent access */
	struct mutex vidsetup_lock;
	/*! Indicates the state of hdmirx is in for video */
	unsigned char rx_video_state;
	bool hpd_state;
	bool signal_status;
	/*! number of planes connected */
	unsigned int nb_planes_connected;

	/*! Audio Playback Handle */
	stm_se_playback_h aud_playback;
	/*! Audio PlayStream Handle */
	stm_se_play_stream_h aud_playstream;
	/*! Audio sink */
	stm_object_h aud_sink;
	/*! hdmirx audio reader */
	stm_se_audio_reader_h aud_reader;
	/*! no of mixer connections to this HDMIRx route */
	unsigned int num_mixer_conn;
};

/** HDMI Rx ports context structure.*/
struct vidextin_hdmirx_ctx_s {
	/*! The shared hdmi device structure */
	struct hdminput_shared_s *hdmidev;
	/*! The v4l2 sub-device structure.This will be initialized
	   when the v4l subdev for VidExtin_Hdmirx route is created. */
	struct v4l2_subdev v4l2_vidext_sd;
	/*!Stores the pad info. This will created accoridng to the
	   number of PORTS on the platform. */
	struct media_pad *port_pad;
	/*! This will represent a port. This object will passed to
	   the HDMI Event handler and will used as the cookie. */
	struct hdmiport_s *port;
	/*! Number of HDMI ports availabe on the platform. */
	int nb_ports;
	/*!Event subscription handle */
	stm_event_subscription_h extevtsubs;
};

/** This is a singular instance object and it will be created during the driver init.*/
struct hdminput_shared_s {
	/*! Hdmi device Handle and will be a singular instance
	   in a system.This handle has to be allocated during the module init. */
	stm_hdmirx_device_h hdmirxdev_h;
	/*! This will be lock used for any HDMI route or Port operation */
	struct mutex hdmirx_lock;
	/*! This will point to the array of hdmirx_ctx_s pointers which will
	   be created according to the number of routes. */
	struct hdmirx_ctx_s *hdmirx;
	int nb_hdmirx;		/*!number of hdmirx_ctx_s instance created. */
	struct vidextin_hdmirx_ctx_s vidextin_hdmirx;	/*!vidextin_hdmirx_ctx_s  instance. */
};

struct hdminput_shared_s hdmiinput_shared;

/*
 * We need to configure DVP only when it is present. Audio is managed
 * in the case of DVP by virtue of a separate audio decoder subdev
 */
#ifdef CONFIG_STLINUXTV_DVP
static int hdmirx_route_configure_dvp(struct hdmirx_ctx_s *hdmirx_ctx,
					const struct media_pad *dvp_pad);
#endif
#ifdef CONFIG_STLINUXTV_I2S_CAPTURE
static int hdmirx_route_configure_audiodec(struct hdmirx_ctx_s *hdmirx_ctx,
					const struct media_pad *auddec_pad);
#endif

/**
 * route_process_event() - process route events
 */
static void route_process_event(__u32 type, __u32 val, struct v4l2_event *event)
{
	switch (type) {
	case ROUTE_EVT_HOTPLUG:
		event->type = V4L2_EVENT_STI_HOTPLUG;
		event->u.data[0] = val;
		break;

	case ROUTE_EVT_VIDEO:
		event->id = 1;
		event->type = V4L2_EVENT_SOURCE_CHANGE;
		event->u.src_change.changes = val;
		break;

	case ROUTE_EVT_AUDIO:
		event->id = 2;
		event->type = V4L2_EVENT_SOURCE_CHANGE;
		event->u.src_change.changes = val;
		break;

	case ROUTE_EVT_SIG:
		event->id = 0;
		event->type = V4L2_EVENT_STI_HDMI_SIG;
		event->u.data[0] = val;
		break;
	}
}

/**
 * route_event_queue_fh() - queue events per file handle
 * @fh  : valid fh pointer
 * @type: hpd, audio. video
 * @val : value to pass on
 * This queues the event to the particular file handle.
 * To be used only for initial feeback
 */
static void route_event_queue_fh(struct v4l2_fh *fh, __u32 type, __u32 val)
{
	struct v4l2_event event;
	memset(&event, 0, sizeof(event));
	route_process_event(type, val, &event);
	v4l2_event_queue_fh(fh, &event);
}

/**
 * route_event_queue() - queue events to all open handles
 * @sd  : route subdev pointer
 * @type: hpd, audio. video
 * @val : value to pass on
 * This queues the event to the device
 */
static void route_event_queue(struct v4l2_subdev *sd, __u32 type, __u32 val)
{
	struct v4l2_event event;
	memset(&event, 0, sizeof(event));
	route_process_event(type, val, &event);
	if (sd->devnode)
		v4l2_event_queue(sd->devnode, &event);
}

/**
 * map_stm_to_v4l2_codec() - map stm codec type to v4l2 codec type
 */
static __u32 map_stm_to_v4l2_codec(stm_hdmirx_audio_coding_type_t codec)
{
	__u32 v4l2_codec;

	switch (codec) {
	case STM_HDMIRX_AUDIO_CODING_TYPE_PCM:
		v4l2_codec = V4L2_MPEG_AUDIO_STM_ENCODING_PCM;
		break;

	case STM_HDMIRX_AUDIO_CODING_TYPE_MPEG1:
		v4l2_codec = V4L2_MPEG_AUDIO_STM_ENCODING_LAYER_1;
		break;

	case STM_HDMIRX_AUDIO_CODING_TYPE_MPEG2:
		v4l2_codec = V4L2_MPEG_AUDIO_STM_ENCODING_LAYER_2;
		break;

	case STM_HDMIRX_AUDIO_CODING_TYPE_MP3:
		v4l2_codec = V4L2_MPEG_AUDIO_STM_ENCODING_LAYER_3;
		break;

	case STM_HDMIRX_AUDIO_CODING_TYPE_AC3:
		v4l2_codec = V4L2_MPEG_AUDIO_STM_ENCODING_AC3;
		break;

	case STM_HDMIRX_AUDIO_CODING_TYPE_AAC:
		v4l2_codec = V4L2_MPEG_AUDIO_STM_ENCODING_AAC;
		break;

	case STM_HDMIRX_AUDIO_CODING_TYPE_NONE:
	default:
		v4l2_codec = ~V4L2_MPEG_AUDIO_STM_ENCODING_PCM;
	}

	return v4l2_codec;
}

/**
 * hdmirx_route_find_port() - finds the connected port
 */
static stm_hdmirx_port_h hdmirx_route_find_port(struct hdmirx_ctx_s *route_ctx)
{
	int id = 0;
	struct media_pad *port_pad;
	struct v4l2_subdev *port_sd;
	struct vidextin_hdmirx_ctx_s *port_ctx;
	stm_hdmirx_port_h port = NULL;

	/*
	 * Search for the port pad to which this route pad is connected
	 */
	port_pad = stm_media_find_remote_pad_with_type(&route_ctx->pads[HDMIRX_SNK_PAD],
					MEDIA_LNK_FL_ENABLED,
					MEDIA_ENT_T_V4L2_SUBDEV_VIDEXTIN_HDMIRX,
					&id);
	if (!port_pad) {
		pr_debug("%s(): no port connected to this route\n", __func__);
		goto port_found;
	}

	/*
	 * Get the port context and handle
	 */
	port_sd = media_entity_to_v4l2_subdev(port_pad->entity);
	port_ctx = v4l2_get_subdevdata(port_sd);
	port = port_ctx->port[port_pad->index].port_hdl;

port_found:
	return port;
}


/**
 * hdmirx_route_get_plug_status() - retrieve the plug status
 * Return Value:
 * @false: no signal present, @true : signal present
 */
static bool hdmirx_route_get_plug_status(struct hdmirx_ctx_s *route_ctx)
{
	int ret;
	bool hpd_status = false;
	stm_hdmirx_port_source_plug_status_t plug_status;
	stm_hdmirx_port_h port;

	/*
	 * Get the connected port
	 */
	port = hdmirx_route_find_port(route_ctx);
	if (!port) {
		pr_err("%s(): no port connected to this route\n", __func__);
		goto get_plug_status_done;
	}

	/*
	 * Get the signal status
	 */
	ret = stm_hdmirx_port_get_source_plug_status(port, &plug_status);
	if (ret) {
		pr_err("%s(): failed to get the plug status\n", __func__);
		goto get_plug_status_done;
	}

	if (plug_status == STM_HDMIRX_PORT_SOURCE_PLUG_STATUS_IN)
		hpd_status = true;

get_plug_status_done:
	return hpd_status;
}

/**
 * hdmirx_route_detect_signal() - detects if a stable hdmi signal is present
 */
static bool hdmirx_route_detect_signal(struct hdmirx_ctx_s *hdmirxctx)
{
	bool status = false;
	int ret = 0;
	stm_hdmirx_route_signal_status_t signal_status;

	if (!hdmirxctx) {
		pr_debug("%s(): No hdmirx route context\n", __func__);
		goto detect_done;
	}

	/*
	 * Get the plug status of the connected hdmi port
	 */
	if (!hdmirx_route_get_plug_status(hdmirxctx)) {
		pr_err("%s(): HDMI cable not plugged in\n", __func__);
		goto detect_done;
	}

	/*
	 * Get the incoming signal status
	 */
	ret = stm_hdmirx_route_get_signal_status(hdmirxctx->rxhdl,
							&signal_status);
	if (ret) {
		pr_err("%s(): failed to get signal status\n", __func__);
		goto detect_done;
	}

	if (signal_status != STM_HDMIRX_ROUTE_SIGNAL_STATUS_PRESENT) {
		pr_err("%s(): No valid signal present\n", __func__);
		goto detect_done;
	}

	status = true;

detect_done:
	return status;
}

/**
 * hdmirx_route_convert_stm_timings() - convert stm specific timings to v4l2
 * @timing    : hdmirx signal timings
 * @dv_timings: dv_timings to fill in
 * Map the hdmirx signal property to v4l2_dv_timings. This helps in passing
 * the v4l2 type information between subdevs
 */
#if defined(CONFIG_STLINUXTV_DVP) || defined(CONFIG_STLINUXTV_HDMIRX_VIDEO_BYPASS)
static void
hdmirx_route_convert_stm_timings(stm_hdmirx_signal_timing_t *timing,
					struct v4l2_dv_timings *dv_timings)
{
	dv_timings->type = V4L2_DV_BT_656_1120;

	/*
	 * Interlaced or not?
	 * FIXME:Aren't we going to specific about interlaced type
	 */
	if (timing->scan_type == STM_HDMIRX_SIGNAL_SCAN_TYPE_INTERLACED)
		dv_timings->bt.interlaced = V4L2_FIELD_INTERLACED;
	else
		dv_timings->bt.interlaced = V4L2_FIELD_NONE;

	dv_timings->bt.width = timing->hactive;
	dv_timings->bt.height = timing->vActive;
	dv_timings->bt.hfrontporch = timing->hactive_start;
	dv_timings->bt.vfrontporch = timing->vactive_start;
	dv_timings->bt.pixelclock = timing->pixel_clock_hz;
	dv_timings->bt.hsync = (timing->htotal
				- timing->hactive
				- timing->hactive_start);
	dv_timings->bt.vsync = (timing->vtotal
				- timing->vActive
				- timing->vactive_start);
}
#endif

/**
 * hdmirx_route_convert_stm_format() - convert stm format to v4l2_mbus_framefmt.
 * @param[in] *signal_property The hdmi timing info structure
 * @param[out] *fmt struct v4l2_mbus_framefmt structure.
 * @return 0 if mappping successful.
 *     -EINVAL if error.
 */
#if defined(CONFIG_STLINUXTV_DVP) || defined(CONFIG_STLINUXTV_HDMIRX_VIDEO_BYPASS)
static int
hdmirx_route_convert_stm_format(stm_hdmirx_signal_property_t *property,
						struct v4l2_mbus_framefmt *fmt)
{
	int ret = 0;
	stm_hdmirx_video_property_t *vid_property = &property->video_property;

	/*
	 * Start off with frame height/width
	 */
	fmt->width = property->timing.hactive;
	fmt->height = property->timing.vActive;

	/*
	 * Force RGB color type for DVI mode
	 */
	if (property->signal_type == STM_HDMIRX_SIGNAL_TYPE_DVI) {
		fmt->code = V4L2_MBUS_FMT_STM_RGB8_3x8;
		fmt->colorspace = V4L2_COLORSPACE_SRGB;
		goto to_mbus_convert_done;
	}

	/*
	 * Find out the v4l2 color format
	 */
	switch (vid_property->color_format) {
	case STM_HDMIRX_COLOR_FORMAT_RGB:

		/*
		 * Determine the precise format based on color depth
		 */
		switch (vid_property->color_depth) {
		case STM_HDMIRX_COLOR_DEPTH_24BPP:
			fmt->code = V4L2_MBUS_FMT_RGB888_1X24;
			break;

		case STM_HDMIRX_COLOR_DEPTH_30BPP:
			fmt->code = V4L2_MBUS_FMT_RGB101010_1X30;
			break;

		default:
			ret = -EINVAL;
			goto bpp_invalid;
		}

		break;

	/*
	 * FIXME: Not yet defined in the spec
	 */
	case STM_HDMIRX_COLOR_FORMAT_YUV_422:
		/*
		 * That is 422 Raster Double/Single? Buffer format
		 */
		/* fmt->code = V4L2_MBUS_FMT_VYUY8_1X16; 	*/ /* YCbCr422RSB */
		fmt->code = V4L2_MBUS_FMT_YUYV8_2X8;			/* YCbCr422R2B */
		break;

	case STM_HDMIRX_COLOR_FORMAT_YUV_444:
		/*
		 * Determine the precise format based on color depth
		 */
		switch (vid_property->color_depth) {
		case STM_HDMIRX_COLOR_DEPTH_24BPP:
			fmt->code = V4L2_MBUS_FMT_YUV8_1X24;
			break;

		case STM_HDMIRX_COLOR_DEPTH_30BPP:
			fmt->code = V4L2_MBUS_FMT_YUV10_1X30;
			break;

		default:
			ret = -EINVAL;
			goto bpp_invalid;
		}

		break;

	}

	switch (vid_property->colorimetry) {
	case STM_HDMIRX_COLORIMETRY_STD_BT_601:
	case STM_HDMIRX_COLORIMETRY_STD_XVYCC_601:
	case STM_HDMIRX_COLORIMETRY_STD_sYCC_601:
	case STM_HDMIRX_COLORIMETRY_STD_AdobeYCC_601:
		fmt->colorspace = V4L2_COLORSPACE_SMPTE170M;
		break;

	case STM_HDMIRX_COLORIMETRY_STD_BT_709:
	case STM_HDMIRX_COLORIMETRY_STD_XVYCC_709:
		fmt->colorspace = V4L2_COLORSPACE_REC709;
		break;

	default:
		fmt->colorspace = V4L2_COLORSPACE_SRGB;
		break;
	}

to_mbus_convert_done:
	if (property->timing.scan_type ==
	    STM_HDMIRX_SIGNAL_SCAN_TYPE_INTERLACED) {
		fmt->field = V4L2_FIELD_INTERLACED;
	}else {
		fmt->field = V4L2_FIELD_NONE;
	}

bpp_invalid:
	return ret;
}
#endif

#ifdef CONFIG_STLINUXTV_HDMIRX_VIDEO_BYPASS
static int progtiming_to_planesubdev(struct hdmirx_ctx_s *hdmirxctx,
				     struct v4l2_subdev *sdev)
{
	int ret = 0;
	stm_hdmirx_signal_property_t signal_property;

	struct v4l2_dv_timings dv_timings;
	struct v4l2_mbus_framefmt fmt;

	memset(&dv_timings, 0, sizeof(dv_timings));
	memset(&fmt, 0, sizeof(fmt));
	memset(&signal_property, 0, sizeof(signal_property));

	ret = stm_hdmirx_route_get_signal_property(hdmirxctx->rxhdl,
						   &signal_property);
	if (ret < 0) {
		_err(" get signal property failed");
		return ret;
	}
	_info("***********Signal Property************");
	_info("Signal Type :%d ", signal_property.signal_type);
	_info("HACTIVE :%hd ", signal_property.timing.hactive);
	_info("HTOTAL :%hd ", signal_property.timing.htotal);
	_info("VACTIVE :%hd ", signal_property.timing.vActive);
	_info("VTOTAL :%hd ", signal_property.timing.vtotal);
	_info("ScanType :%d ", signal_property.timing.scan_type);
	_info("Pixel Clock :%d ", signal_property.timing.pixel_clock_hz);
	_info("**************************************");

	hdmirx_route_convert_stm_timings(&signal_property.timing, &dv_timings);

	ret =
	    v4l2_subdev_call(sdev, video, s_dv_timings,
			     &dv_timings);
	if (ret < 0)
		return ret;

	ret = hdmirx_route_convert_stm_format(&signal_property, &fmt);
	if (ret) {
		pr_err("%s(): Unsupported format found\n", __func__);
		goto prog_subdev_failed;
	}

	ret = v4l2_subdev_call(sdev, video, s_mbus_fmt, &fmt);

prog_subdev_failed:
	return ret;
}
#endif

/** hdmirx_video_setup() -
 * 1. Check if PORT_CONNECTED and PLANE_CONNECTED flag is set.
 * 2. Then check HDMIRX_SIGNAL_PRESENT flag is set or not
 * 2. If not set, then detect the signal
 * 3. If set, then enable display
 * 4. If any flag is unset, then disable the display
 *
 * @pre the hdmirx should be initialised
 * @param[in] *hdmirxctx The pointer to the HDMI route ctx
 * @return NONE
 */
#ifdef CONFIG_STLINUXTV_HDMIRX_VIDEO_BYPASS
void hdmirx_video_setup(struct hdmirx_ctx_s *hdmirxctx, struct v4l2_subdev *plane_sd)
{
	int ret = 0;
	unsigned char bitmask;
	struct v4l2_control ctrl;

	ctrl.id = V4L2_CID_DV_STM_RX_PIXEL_TIMING_STABILITY;
	ctrl.value = VCSPIXEL_TIMING_UNSTABLE;

	mutex_lock(&hdmirxctx->vidsetup_lock);

	_info("the current state is %x", hdmirxctx->rx_video_state);

	bitmask = HDMIRX_PLANE_CONNECTED|HDMIRX_PORT_CONNECTED;
	if (bitmask != (hdmirxctx->rx_video_state & bitmask)) {
		v4l2_subdev_call(plane_sd, core, s_ctrl, &ctrl);
		mutex_unlock(&hdmirxctx->vidsetup_lock);
		return;
	}

	bitmask = HDMIRX_SIGNAL_PRESENT;
	if (!(hdmirxctx->rx_video_state & bitmask)) {
		/* Try to detect the signal */
		if (!hdmirx_route_detect_signal(hdmirxctx)) {
			v4l2_subdev_call(plane_sd, core, s_ctrl, &ctrl);
			mutex_unlock(&hdmirxctx->vidsetup_lock);
			return;
		}
		hdmirxctx->rx_video_state |= HDMIRX_SIGNAL_PRESENT;
	}

	ret = progtiming_to_planesubdev(hdmirxctx, plane_sd);
	if(ret) {
		v4l2_subdev_call(plane_sd, core, s_ctrl, &ctrl);
		mutex_unlock(&hdmirxctx->vidsetup_lock);
		return;
	}

	/* Timing is stable now */
	ctrl.value = VCSPIXEL_TIMING_STABLE;
	v4l2_subdev_call(plane_sd, core, s_ctrl, &ctrl);
	mutex_unlock(&hdmirxctx->vidsetup_lock);
}
#endif

/** hdmiport_evt_handler() - The HDMI port event handler. This will be invoked
 * for any port related events.
 *
 * @pre The system should be initialized.
 * @param[in] number_of_events Number of events generated.
 * @param[in] *events The event info array.
 * @return NONE
 */
static void hdmiport_evt_handler(int number_of_events,
				 stm_event_info_t * events)
{
	int i;
	struct hdmiport_s *Ctx;
	/* can we assume we get only one event at a time */
	if (!(number_of_events > 0))
		return;

	/* retrieve device context from event subscription handler cookie */
	for (i = 0; i < number_of_events; ++i) {
		switch (events[i].event.event_id) {
		case STM_HDMIRX_PORT_SOURCE_PLUG_EVT:
			Ctx = events[i].cookie;
			_dbg(" port handler Source  Plug event trigerred : %d",
			     Ctx->port_id);
			break;
		}
	}
}

static void hdmirx_video_setup_find_plane(struct hdmirx_ctx_s *Ctx)
{
#ifdef CONFIG_STLINUXTV_HDMIRX_VIDEO_BYPASS
	struct media_pad *plane_pad;
	int id = 0;
	struct v4l2_subdev *plane_sd;

	/* try to get the connected plane subdev */
	do {
		plane_pad =
		    stm_media_find_remote_pad(&Ctx->pads[HDMIRX_VID_SRC_PAD],
					      MEDIA_LNK_FL_ENABLED, &id);
		if (plane_pad
		    && (plane_pad->entity->type ==
			MEDIA_ENT_T_V4L2_SUBDEV_PLANE_VIDEO)) {
			break;
	}
	} while (plane_pad);

	if(!plane_pad)/* no plane is yet connected*/
		return;

	plane_sd = media_entity_to_v4l2_subdev(plane_pad->entity);

	hdmirx_video_setup(Ctx, plane_sd);
#endif
}


/** hdmirx_audio_setup() -
 * @pre the hdmirx should be initialised
 * @param[in] *hdmirxctx The pointer to the HDMI route ctx
 * @return NONE
 */
void hdmirx_audio_setup(struct hdmirx_ctx_s *hdmirxctx)
{
#ifdef CONFIG_STLINUXTV_HDMIRX_AUDIO_BYPASS
	int ret;
	bool enable = false;
	unsigned char bitmask;
	stm_hdmirx_audio_property_t aud_prop;
	stm_hdmirx_signal_property_t sig_prop;
	stm_se_ctrl_audio_reader_source_info_t reader_source_info = {0};
	struct media_pad *pad;
	int id = 0;

	pad = stm_media_find_remote_pad_with_type
					(&hdmirxctx->pads[HDMIRX_AUD_SRC_PAD],
					MEDIA_LNK_FL_ENABLED,
					MEDIA_ENT_T_ALSA_SUBDEV_MIXER,
					&id);
	if (!pad) {
		pr_err("%s(): hdmirx route not connected to mixer\n", __func__);
		return;
	}

	bitmask = HDMIRX_PORT_CONNECTED;
	if (!(hdmirxctx->rx_video_state & bitmask)){
		return;
	}
	/* At this point we are sure route is initialised*/

	/* We reuse the vidsetup lock as there is not much lock contention*/
	mutex_lock(&hdmirxctx->vidsetup_lock);

	if (!hdmirx_route_detect_signal(hdmirxctx))
		goto audio_enable;

	/*Check if DVI */
	ret = stm_hdmirx_route_get_signal_property(hdmirxctx->rxhdl, &sig_prop);
	if (ret)
		goto audio_enable;

	if(sig_prop.signal_type != STM_HDMIRX_SIGNAL_TYPE_HDMI)
		goto audio_enable;

	ret = stm_hdmirx_route_get_audio_property(hdmirxctx->rxhdl,&aud_prop);
	if(ret)
		goto audio_enable;

	if(aud_prop.stream_type != STM_HDMIRX_AUDIO_STREAM_TYPE_IEC &&
		!(supported_coding_types(aud_prop.coding_type))){
		_err("This stream_type %d, coding_type %d is not supported",
				aud_prop.stream_type, aud_prop.coding_type);
		goto audio_enable;
	}

	if(!hdmirxctx->num_mixer_conn)
		goto audio_enable;

	/* At this point , we are sure that mixer is connected and reader is initialised*/
	reader_source_info.channel_count = aud_prop.channel_count;
	reader_source_info.sampling_frequency = aud_prop.sampling_frequency;
	ret = stm_se_audio_reader_set_compound_control(hdmirxctx->aud_reader,
		STM_SE_CTRL_AUDIO_READER_SOURCE_INFO, &reader_source_info);
	if(ret){
		_err("Audio reader source info set failed");
		goto audio_enable;
	}

	enable = true;

audio_enable:
	stm_hdmirx_route_set_audio_out_enable(hdmirxctx->rxhdl,enable);
	_info("audio enable: %d for hdmirx \n", enable);

	mutex_unlock(&hdmirxctx->vidsetup_lock);

	return;
#endif
}

/**
 * hdmirx_route_set_auddec_fmt_unstable() - tell the audio decoder, properties are unstable
 * @hdmirx_ctx: hdmirx route context
 * @auddec_pad: connected audio decoder pad
 * In repsonse to loss of hdmi signal, tell the audio decoder, that properties may
 * change next time on valid hdmi signal, so, get prepared for it
 */
#ifdef CONFIG_STLINUXTV_I2S_CAPTURE
static int hdmirx_route_set_auddec_fmt_unstable(struct hdmirx_ctx_s *hdmirx_ctx,
						const struct media_pad *auddec_pad)
{
	int id = 0, ret = 0;
	int is_stable = 0;
	struct v4l2_subdev *auddec_subdev;

	MUTEX_INTERRUPTIBLE(&hdmirx_ctx->vidsetup_lock);

	/*
	 * Check the status, and if unstable we process, else we go out
	 */
	if (hdmirx_route_detect_signal(hdmirx_ctx)) {
		pr_debug("%s(): stable hdmi signal, so, we do nothing\n", __func__);
		goto unstable_done;
	}

	/*
	 * Find the only connected decoder
	 */
	if (!auddec_pad) {
		auddec_pad = stm_media_find_remote_pad_with_type
			(&hdmirx_ctx->pads[HDMIRX_AUD_SRC_PAD],
			 MEDIA_LNK_FL_ENABLED,
			 MEDIA_ENT_T_V4L2_SUBDEV_AUDIO_DECODER,
			 &id);
		if (!auddec_pad) {
			pr_err("%s(): failed to find any enabled link\n", __func__);
			goto unstable_done;
		}
	}

	auddec_subdev = media_entity_to_v4l2_subdev(auddec_pad->entity);

	/*
	 * Inform the audio decoder about input change
	 */
	ret = v4l2_subdev_call(auddec_subdev, core, ioctl,
				VIDIOC_STI_S_AUDIO_INPUT_STABLE, &is_stable);
	if (ret)
		pr_err("%s(): failed to notify property stability\n", __func__);

	/*
	 * Disable the audio capture from hdmirx (either link is unstable or
	 * connection is snapped between route and decoder)
	 */
	ret = stm_hdmirx_route_set_audio_out_enable(hdmirx_ctx->rxhdl, false);
	if (ret)
		pr_err("%s(): failed to disable audio out from hdmirx route\n", __func__);

unstable_done:
	mutex_unlock(&hdmirx_ctx->vidsetup_lock);
	return ret;
}
#endif

/**
 * hdmirx_route_set_dvp_fmt_unstable() - tell the dvp that properties are unstable
 * @hdmirx_ctx: hdmirx route context
 * In repsonse to loss of hdmi signal, tell the DVP, that properties may
 * change next time on valid hdmi signal, so, get prepared for it
 */
#ifdef CONFIG_STLINUXTV_DVP
static int hdmirx_route_set_dvp_fmt_unstable(struct hdmirx_ctx_s *hdmirx_ctx,
						const struct media_pad *dvp_pad)
{
	int id = 0, ret = 0;
	struct v4l2_subdev *dvp_subdev;
	int is_stable = 0;

	MUTEX_INTERRUPTIBLE(&hdmirx_ctx->vidsetup_lock);

	/*
	 * Check the status, and if unstable we process, else we go out
	 */
	if (hdmirx_route_detect_signal(hdmirx_ctx)) {
		pr_debug("%s(): stable hdmi signal, so, we do nothing\n", __func__);
		goto unstable_done;
	}

	/*
	 * One DVP present, find that only one if connected
	 */
	if (!dvp_pad) {
		dvp_pad = stm_media_find_remote_pad_with_type
			(&hdmirx_ctx->pads[HDMIRX_VID_SRC_PAD],
			 MEDIA_LNK_FL_ENABLED,
			 MEDIA_ENT_T_V4L2_SUBDEV_DVP,
			 &id);
		if (!dvp_pad) {
			pr_err("%s(): failed to find any enabled link\n", __func__);
			goto unstable_done;
		}
	}

	dvp_subdev = media_entity_to_v4l2_subdev(dvp_pad->entity);

	ret = v4l2_subdev_call(dvp_subdev, core, ioctl,
				VIDIOC_STI_S_VIDEO_INPUT_STABLE, &is_stable);
	if (ret)
		pr_err("%s(): failed to notify property stability\n", __func__);

unstable_done:
	mutex_unlock(&hdmirx_ctx->vidsetup_lock);
	return ret;
}
#endif

/** hdmirx_evt_handler() - The HDMI route event handler.This wil be invoked for
 * any route related events.
 *
 * @pre The system should be initialized.
 * @param[in] number_of_events Number of events generated.
 * @param[in] *events The event info array.
 * @return NONE
 */
static void hdmirx_evt_handler(int number_of_events, stm_event_info_t * events)
{
	int i;
	bool plug_state, signal_status;
	struct hdmirx_ctx_s *Ctx;

	/* can we assume we get only one event at a time */
	if (!(number_of_events > 0))
		return;

	/* retrieve device context from event subscription handler cookie */
	for (i = 0; i < number_of_events; ++i) {
		Ctx = events[i].cookie;
		switch (events[i].event.event_id) {
		case STM_HDMIRX_ROUTE_SIGNAL_STATUS_CHANGE_EVT:

			pr_debug("%s(): Signal status changed\n", __func__);

			Ctx->rx_video_state &= (~HDMIRX_SIGNAL_PRESENT);

			/*
			 * Plane can be only be configured in ORLY as hdmirx port
			 * can be connected to plane. In Cannes, we configure DVP
			 */
			hdmirx_video_setup_find_plane(Ctx);
			hdmirx_audio_setup(Ctx);

			/*
			 * If the cable is plugged out or the signal is unstable,
			 * tell this to the connected subdev's
			 */
#ifdef CONFIG_STLINUXTV_DVP
			if (hdmirx_route_set_dvp_fmt_unstable(Ctx, NULL))
				pr_err("%s(): cannot set dvp fmt unstable\n", __func__);
#endif
#ifdef CONFIG_STLINUXTV_I2S_CAPTURE
			if (hdmirx_route_set_auddec_fmt_unstable(Ctx, NULL))
				pr_err("%s(): cannot set auddec fmt unstable\n", __func__);
#endif

			/*
			 * Queue the event to be pushed to the subscriber
			 */
			if (mutex_lock_interruptible(&Ctx->vidsetup_lock))
				return;

			/*
			 * Handling for HPD state. The idea is to propagate the events
			 * using hdmirx subdev only, so, the application receives the
			 * event in a serialized way. This HDMIRx event comes in 3 cases:
			 * a. Cable is plugged in
			 * b. Cable is plugged out
			 * c. Video timing changed
			 * We do not not want to signal HPD event for video timing
			 * change, but, only for actual HPD. For video timing change
			 * we are pushing a signal stability event
			 */
			plug_state = hdmirx_route_get_plug_status(Ctx);
			if (plug_state != Ctx->hpd_state) {

				pr_debug("%s(): hotplug event generated\n", __func__);

				Ctx->hpd_state = plug_state;
				route_event_queue(&Ctx->hdmirx_sd, ROUTE_EVT_HOTPLUG, Ctx->hpd_state);

				/*
				 * Clear the video/audio events data when HPD is low
				 */
				if (!Ctx->hpd_state) {
					route_event_queue(&Ctx->hdmirx_sd, ROUTE_EVT_VIDEO, 0);
					route_event_queue(&Ctx->hdmirx_sd, ROUTE_EVT_AUDIO, 0);
					route_event_queue(&Ctx->hdmirx_sd, ROUTE_EVT_SIG, 0);
					mutex_unlock(&Ctx->vidsetup_lock);
					break;
				}
			}

			/*
			 * Process signal stablity
			 */
			signal_status = hdmirx_route_detect_signal(Ctx);
			if (signal_status != Ctx->signal_status) {
				Ctx->signal_status = signal_status;
				route_event_queue(&Ctx->hdmirx_sd, ROUTE_EVT_SIG, signal_status);

				if (!signal_status) {
					route_event_queue(&Ctx->hdmirx_sd, ROUTE_EVT_VIDEO, 0);
					route_event_queue(&Ctx->hdmirx_sd, ROUTE_EVT_AUDIO, 0);
				}
			}

			mutex_unlock(&Ctx->vidsetup_lock);

			break;

		case STM_HDMIRX_ROUTE_VIDEO_PROPERTY_CHANGE_EVT:

			pr_debug("%s(): video property changed\n", __func__);

			Ctx->rx_video_state &= (~HDMIRX_SIGNAL_PRESENT);

			hdmirx_video_setup_find_plane(Ctx);

#ifdef CONFIG_STLINUXTV_DVP
			if (hdmirx_route_configure_dvp(Ctx, NULL))
				pr_err("%s(): failed to configure dvp subdev\n", __func__);
#endif
			/*
			 * Queue the event to be pushed to the subscriber
			 */
			if (mutex_lock_interruptible(&Ctx->vidsetup_lock))
				return;

			route_event_queue(&Ctx->hdmirx_sd, ROUTE_EVT_VIDEO,
							V4L2_EVENT_SRC_CH_RESOLUTION);

			mutex_unlock(&Ctx->vidsetup_lock);

			break;

		case STM_HDMIRX_ROUTE_AUDIO_PROPERTY_CHANGE_EVT:

			pr_debug("%s(): audio property changed\n", __func__);

			hdmirx_audio_setup(Ctx);

#ifdef CONFIG_STLINUXTV_I2S_CAPTURE
			if (hdmirx_route_configure_audiodec(Ctx, NULL))
				pr_err("%s(): failed to configure audio decoder subdev\n", __func__);
#endif

			/*
			 * Queue the event to be pushed to the subscriber
			 */
			if (mutex_lock_interruptible(&Ctx->vidsetup_lock))
				return;

			route_event_queue(&Ctx->hdmirx_sd, ROUTE_EVT_AUDIO,
							V4L2_EVENT_SRC_CH_RESOLUTION );

			mutex_unlock(&Ctx->vidsetup_lock);

			break;

		default:
			break;
		}
	}
}

/** vidextin_hdmirx_link_setup() - The link setup callback for the vidextin
 *   hdmi entity. Currently this callback  does not do anything.
 * @pre The system should be initialized.
 * @param[in] *entity The pointer to the vidextin hdmi entity
 * @param[in] *local The local pad for the link setup
 * @param[in] *remote The remote pad for the link setup
 * @param[in] flags The link flags
 * @return 0 if successful.
 *   -EINVAL on error.
 */
static int vidextin_hdmirx_link_setup(struct media_entity *entity,
				      const struct media_pad *local,
				      const struct media_pad *remote, u32 flags)
{
	_dbg(" link callback invoked %x %s", flags, remote->entity->name);
	return 0;
}

/** set_port_rx_connect() -This function is used connect the port to the route.
 * The functionality of the routine is :
 * 1. Open the hdmirx route
 * 2. subscribe the events
 * 3. Connect the port to the HDMIRx route
 * 4. Call the HDMIRx_Video_Setup
 * @pre The hdmi route and vidextin hdmi entities should be existing.
 * @param[in] *local The local pad information. In this case the hdmirx sink pad.
 * @param[in] *remote The remote pad information. In this case the port.
 * @return 0 if successful.
 *  -EINVAL on error.
 */
static int set_port_rx_connect(const struct media_pad *local,
			       const struct media_pad *remote)
{
	struct v4l2_subdev *sd;
	struct hdmirx_ctx_s *hdmirxctx;
	struct vidextin_hdmirx_ctx_s *extinctx;
	int id = 0, ret = 0;
	struct media_pad *temp_pad;
	stm_event_subscription_entry_t evtentry;

	sd = media_entity_to_v4l2_subdev(local->entity);
	hdmirxctx = v4l2_get_subdevdata(sd);

	if (WARN_ON(hdmirxctx == NULL))
		return -EINVAL;

	sd = media_entity_to_v4l2_subdev(remote->entity);
	extinctx = v4l2_get_subdevdata(sd);

	if (WARN_ON(extinctx == NULL))
		return -EINVAL;

	/*Check if the route is connected to any other port */
	do {
		temp_pad =
		    stm_media_find_remote_pad((struct media_pad *)local,
					      MEDIA_LNK_FL_ENABLED, &id);
		if (temp_pad
		    && (temp_pad->entity->type ==
			MEDIA_ENT_T_V4L2_SUBDEV_VIDEXTIN_HDMIRX)) {
			_err("  Please disable the existing port connection");
			return -EINVAL;
		}
	} while (temp_pad);

	ret = stm_hdmirx_route_open(hdmirxctx->hdmidev->hdmirxdev_h,
				    hdmirxctx->route_inst, &hdmirxctx->rxhdl);
	if (ret != 0) {
		_err("HdmiRx Device Open failed  ");
		return ret;
	}

	/*subscribe to events */
	evtentry.object = hdmirxctx->rxhdl;
	evtentry.event_mask = STM_HDMIRX_ROUTE_SIGNAL_STATUS_CHANGE_EVT
	    | STM_HDMIRX_ROUTE_VIDEO_PROPERTY_CHANGE_EVT |
	    STM_HDMIRX_ROUTE_AUDIO_PROPERTY_CHANGE_EVT;

	evtentry.cookie = hdmirxctx;
	ret =
	    stm_event_subscription_create(&evtentry, 1, &hdmirxctx->rxevtsubs);
	if (ret) {
		_err("subs create failed for hdmirx %d", ret);
		goto err1;
	}

	/* set the call back function for subscribed events */
	ret = stm_event_set_handler(hdmirxctx->rxevtsubs,
				    (stm_event_handler_t) hdmirx_evt_handler);
	if (ret) {
		_err("set handler hdmirx failed");
		goto err2;
	}

	/* remote->index is the port ID */
	ret = stm_hdmirx_port_connect(extinctx->port[remote->index].port_hdl,
				      hdmirxctx->rxhdl);
	if (ret != 0) {
		_err("HdmiRx Port Route Connect failed  ");
		goto err2;
	}
	/* the port connect setup is done */
	hdmirxctx->rx_video_state |= HDMIRX_PORT_CONNECTED;

	/* this can be made asynchrounous also */
	hdmirx_video_setup_find_plane(hdmirxctx);
	hdmirx_audio_setup(hdmirxctx);

	return 0;

err2:
	ret = stm_event_subscription_delete(hdmirxctx->rxevtsubs);
	hdmirxctx->rxevtsubs = NULL;
err1:
	stm_hdmirx_route_close(hdmirxctx->rxhdl);
	hdmirxctx->rxhdl = NULL;
	return ret;

}

/** discon_hdmiport_route() -function used to disconnect the port to the route.
 * @pre The hdmi route and vidextin hdmi entities should be existing.
 * @param[in] *local The local pad information. In this case the hdmirx sink pad.
 * @param[in] *remote The remote pad information. In this case the port.
 * @return 0 if successful.
 *   -EINVAL on error.
 */
static int unset_port_rx_connect(const struct media_pad *local,
				 const struct media_pad *remote)
{

	struct v4l2_subdev *sd;
	struct hdmirx_ctx_s *hdmirxctx;
	struct vidextin_hdmirx_ctx_s *extinctx;
	int ret = 0;

	sd = media_entity_to_v4l2_subdev(local->entity);
	hdmirxctx = v4l2_get_subdevdata(sd);

	if (WARN_ON(hdmirxctx == NULL))
		return -EINVAL;

	sd = media_entity_to_v4l2_subdev(remote->entity);
	extinctx = v4l2_get_subdevdata(sd);

	if (WARN_ON(extinctx == NULL))
		return -EINVAL;

	/* the port connect setup is undone */
	hdmirxctx->rx_video_state &=
	    ~(HDMIRX_PORT_CONNECTED | HDMIRX_SIGNAL_PRESENT);

	/* set the call back function for subscribed events */
	ret = stm_event_set_handler(hdmirxctx->rxevtsubs,
				    (stm_event_handler_t) NULL);

	/* Now disconnect and close route */
	ret =
	    stm_hdmirx_port_disconnect(extinctx->port[remote->index].port_hdl);
	if (ret) {
		_err("HdmiRx Port Disconnect failed  ");
		return ret;
	}

	/* this can be made asynchrounous also */
	hdmirx_video_setup_find_plane(hdmirxctx);
	hdmirx_audio_setup(hdmirxctx);

	/* Delete the subscritption first */
	ret = stm_event_subscription_delete(hdmirxctx->rxevtsubs);
	hdmirxctx->rxevtsubs = NULL;

	ret |= stm_hdmirx_route_close(hdmirxctx->rxhdl);
	if (ret) {
		_err("HdmiRx Route close failed  ");
	}

	hdmirxctx->rxhdl = NULL;
	return 0;
}

/** con_hdmirx_plane() - function used to connect the route to the plane.
 * 1. Maintain refcount and setup source , pixel stream  & Interface handle
 * 2. Allocate Mode ID
 * 3. Call the hdmirx_video_setup
 * @pre The hdmi route and plane entities should be existing.
 * @param[in] *hdmirx_vid_pad hdmirx_vid_pad pad information. In this case hdmirx src video pad.
 * @param[in] *plane_pad plane_pad pad information. In this case video plane sink pad.
 * @return 0 if successful.
 *    -EINVAL on error.
 */
static int con_hdmirx_plane(const struct media_pad *hdmirx_vid_pad,
			    const struct media_pad *plane_pad, u32 flags)
{
	int ret = -EINVAL;
#ifdef CONFIG_STLINUXTV_HDMIRX_VIDEO_BYPASS
	struct v4l2_subdev *sd;
	struct hdmirx_ctx_s *hdmirxctx;
	struct v4l2_subdev *plane_sd;

	sd = media_entity_to_v4l2_subdev(hdmirx_vid_pad->entity);
	hdmirxctx = v4l2_get_subdevdata(sd);

	/*we need to set up the sink first */
	ret = media_entity_call(plane_pad->entity, link_setup,
		plane_pad, hdmirx_vid_pad, flags);
	if (ret < 0) {
		return ret;
	}

	/* the plane connect setup is done */
	hdmirxctx->rx_video_state |= HDMIRX_PLANE_CONNECTED;
	hdmirxctx->nb_planes_connected++;

	plane_sd = media_entity_to_v4l2_subdev(plane_pad->entity);
	/* this can be made asynchrounous also */
	hdmirx_video_setup(hdmirxctx, plane_sd);

#endif
	return ret;
}

/** discon_hdmirx_plane() - function used to disconnect the route to the plane.
 *  @pre The hdmi route and plane entities should be existing.
 *  @param[in] *hdmirx_vid_pad The hdmirx_vid_pad pad information. In this case the hdmirx src video pad.
 *  @param[in] *plane_pad The plane_pad pad information. In this case the video plane sink pad.
 *  @return 0 if successful.
 *     -EINVAL on error.
 */
static int discon_hdmirx_plane(const struct media_pad *hdmirx_vid_pad,
			       const struct media_pad *plane_pad)
{
	int ret = -EINVAL;
#ifdef CONFIG_STLINUXTV_HDMIRX_VIDEO_BYPASS
	struct v4l2_subdev *sd;
	struct hdmirx_ctx_s *hdmirxctx;
	struct v4l2_subdev *plane_sd;

	sd = media_entity_to_v4l2_subdev(hdmirx_vid_pad->entity);
	hdmirxctx = v4l2_get_subdevdata(sd);

	hdmirxctx->nb_planes_connected--;
	if (hdmirxctx->nb_planes_connected) {
		ret = 0;
		goto discon_done;
	}

	hdmirxctx->rx_video_state &=
	    ~(HDMIRX_PLANE_CONNECTED | HDMIRX_SIGNAL_PRESENT);

	plane_sd = media_entity_to_v4l2_subdev(plane_pad->entity);
	/* this can be made asynchrounous also */
	hdmirx_video_setup(hdmirxctx, plane_sd);

discon_done:
#endif
	return ret;
}

#ifdef CONFIG_STLINUXTV_HDMIRX_AUDIO_BYPASS
static int deallocate_playback_stream_reader(struct hdmirx_ctx_s *ctx)
{

	_info("deallocate_playback_stream_reader");

	/* We need to deallocate only for the first mixer connection */
	if(ctx->num_mixer_conn > 0)
		return 0;

	if(ctx->aud_reader){
		stm_se_audio_reader_delete(ctx->aud_reader);
		ctx->aud_reader = NULL;
	}
	if(ctx->aud_playstream){
		stm_se_play_stream_delete(ctx->aud_playstream);
		ctx->aud_playstream = NULL;
	}

	if(ctx->aud_playback){
		stm_se_playback_delete(ctx->aud_playback);
		ctx->aud_playback = NULL;
	}

	return 0;
}
#endif

#ifdef CONFIG_STLINUXTV_HDMIRX_AUDIO_BYPASS
static int allocate_playback_stream_reader(struct hdmirx_ctx_s *ctx)
{
	int ret = 0;

	_info("allocate_playback_stream_reader\n");

	/* We need to allocate only for the first mixer connection */
	if(ctx->num_mixer_conn > 0)
		return 0;

	if(ctx->aud_reader || ctx->aud_playback || ctx->aud_playback)
		_err("The playback handles need to be NULL");

	/* FIXME , need to have reader name from pseudo_mixer.c and remove hardcoding*/
	/* In case we start to support multiple readers, then we need to have a mapping
	here */
	ret = stm_se_audio_reader_new("hdmirx_in_0", "hw:0,5",
						&(ctx->aud_reader));
	if(ret)
		return -EINVAL;
	ret = stm_se_playback_new("HDMIRx_Aud_Playback_0", &(ctx->aud_playback));
	if(ret)
		goto free_reader;


	/* Set the Limit (1024 PPM)for clock rates aujustment for the renderer.
       integer 'value' sets the limit to 2^value parts per million. */

	ret = stm_se_playback_set_control(ctx->aud_playback,STM_SE_CTRL_CLOCK_RATE_ADJUSTMENT_LIMIT, 10);
	if(ret)
		_err("Could not set Clock rate adjustment limit to (1 << 10) ");

	ret = stm_se_play_stream_new("HDMIRx_Aud_stream_0", ctx->aud_playback,
				STM_SE_STREAM_ENCODING_AUDIO_SPDIFIN,
				&(ctx->aud_playstream));
	if(ret)
		goto free_playback;

	/* Disable AV Sync - temporary till SE supports it*/
	ret = stm_se_play_stream_set_control(ctx->aud_playstream,STM_SE_CTRL_ENABLE_SYNC,0);
	if(ret)
		_err("AVSync Disable failed ");

	return 0;

free_playback:
	stm_se_playback_delete(ctx->aud_playback);
	ctx->aud_playback = NULL;

free_reader:
	stm_se_audio_reader_delete(ctx->aud_reader);
	ctx->aud_reader = NULL;

	return ret;
}
#endif


/** con_hdmirx_mixer() - function used to connect the route to the mixer.
 * 1. Connect the playstream to mixer
 * @pre The hdmi route and plane entities should be existing.
 * @param[in] *hdmirx_aud_pad hdmirx_aud_pad pad information. In this case hdmirx src audio pad.
 * @param[in] *mixer_pad mixer_pad pad information.
 * @return 0 if successful.
 *    -EINVAL on error.
 */
static int con_hdmirx_mixer(const struct media_pad *hdmirx_aud_pad,
			    const struct media_pad *mixer_pad)
{
	int ret = -EINVAL;
#ifdef CONFIG_STLINUXTV_HDMIRX_AUDIO_BYPASS
	struct v4l2_subdev *sd;
	struct hdmirx_ctx_s *hdmirxctx;

	_info("connecting the mixer to the route");

	sd = media_entity_to_v4l2_subdev(hdmirx_aud_pad->entity);
	hdmirxctx = v4l2_get_subdevdata(sd);


	hdmirxctx->aud_sink = (stm_object_h)snd_pseudo_mixer_get_from_entity(mixer_pad->entity);

	if(hdmirxctx->num_mixer_conn == 0){/* this needs to be done only for first conn*/
		ret = allocate_playback_stream_reader(hdmirxctx);
		if(ret)
			return -EINVAL;

		ret = stm_se_audio_reader_attach(hdmirxctx->aud_reader, hdmirxctx->aud_playstream);
		if(ret)
			goto deallocate;

		ret = stm_se_play_stream_set_enable(hdmirxctx->aud_playstream, 1);
		if(ret)
			goto reader_detach;
	}

	ret = stm_se_play_stream_attach(hdmirxctx->aud_playstream,
					hdmirxctx->aud_sink,
					STM_SE_PLAY_STREAM_OUTPUT_PORT_DEFAULT);
	if(ret)
		goto reader_detach;

	hdmirxctx->num_mixer_conn++;

	hdmirx_audio_setup(hdmirxctx);
	return ret;

reader_detach:
	if(!hdmirxctx->num_mixer_conn)
		stm_se_audio_reader_detach(hdmirxctx->aud_reader, hdmirxctx->aud_playstream);
deallocate:
	if(!hdmirxctx->num_mixer_conn)
		deallocate_playback_stream_reader(hdmirxctx);

	ret = -EINVAL;
#endif
	return ret;
}

/** discon_hdmirx_mixer() - function used to disconnect the route from the mixer.
 * @pre The hdmi route and plane entities should be existing.
 * @param[in] *hdmirx_aud_pad hdmirx_aud_pad pad information. In this case hdmirx src audio pad.
 * @param[in] *mixer_pad mixer_pad pad information.
 * @return 0 if successful.
 *    -EINVAL on error.
 */
static int discon_hdmirx_mixer(const struct media_pad *hdmirx_aud_pad,
			    const struct media_pad *mixer_pad)
{
#ifdef CONFIG_STLINUXTV_HDMIRX_AUDIO_BYPASS
	int ret;
	struct v4l2_subdev *sd;
	struct hdmirx_ctx_s *hdmirxctx;


	_info("disconnecting the mixer to the route");

	sd = media_entity_to_v4l2_subdev(hdmirx_aud_pad->entity);
	hdmirxctx = v4l2_get_subdevdata(sd);

	hdmirxctx->aud_sink = (stm_object_h)snd_pseudo_mixer_get_from_entity(mixer_pad->entity);

	/* we intentionally do not return error if any of the below fails. We want to make sure
	we are good to do a reconnection */

	ret = stm_se_play_stream_detach(hdmirxctx->aud_playstream,
						hdmirxctx->aud_sink);
	if (ret)
		_err("stm_se_play_stream_detach failed");

	hdmirxctx->num_mixer_conn--;
	if(hdmirxctx->num_mixer_conn)
		return 0;

	ret = stm_se_play_stream_set_enable(hdmirxctx->aud_playstream, 0);
	if (ret)
		_err("stm_se_play_stream_set_enable failed");

	ret = stm_se_audio_reader_detach(hdmirxctx->aud_reader, hdmirxctx->aud_playstream);
	if (ret)
		_err("stm_se_audio_reader_detach failed");

	ret = deallocate_playback_stream_reader(hdmirxctx);
	if (ret)
		_err("deallocate_playback_stream_reader failed");

#endif
	return 0;
}

/**
 * stm_media_pad_to_hdmirx_route() - get the hdmirx route context from pad
 */
static inline struct hdmirx_ctx_s *
	stm_media_pad_to_hdmirx_route(const struct media_pad *hdmirx_route_pad)
{
	struct v4l2_subdev *route_subdev;
	struct hdmirx_ctx_s *hdmirx_ctx;

	route_subdev = media_entity_to_v4l2_subdev(hdmirx_route_pad->entity);
	hdmirx_ctx = v4l2_get_subdevdata(route_subdev);

	return hdmirx_ctx;
}

/**
 * hdmirx_route_configure_dvp() - configure dvp with input parameters
 * @hdmirx_ctx: hdmirx route context
 * @dvp_pad   : DVP media pad or NULL
 * Configure the DVP with input parameters. This is called from
 * link_setup as well as from events coming from hdmirx route.
 */
#ifdef CONFIG_STLINUXTV_DVP
static int hdmirx_route_configure_dvp(struct hdmirx_ctx_s *hdmirx_ctx,
					const struct media_pad *dvp_pad)
{
	int ret = 0;
	struct v4l2_dv_timings timings;
	struct v4l2_mbus_framefmt mbus_fmt;
	struct v4l2_subdev *dvp_subdev;
	stm_hdmirx_signal_property_t property;

	MUTEX_INTERRUPTIBLE(&hdmirx_ctx->vidsetup_lock);

	/*
	 * If there's no route opened till moment, we can't do anything
	 */
	if (!hdmirx_ctx->rxhdl) {
		pr_err("%s(): no route opened till now\n", __func__);
		goto configure_dvp_failed;
	}

	/*
	 * If dvp_pad is NULL, then this is called from video property change
	 * event, so, propgate it to connected pad, else don't bother
	 */
	if (!dvp_pad) {
		int id = 0;

		/*
		 * One DVP present, find that only one if connected
		 * FIXME: See if the SUBDEV_DVP type checking is okay
		 */
		dvp_pad = stm_media_find_remote_pad_with_type
						(&hdmirx_ctx->pads[HDMIRX_VID_SRC_PAD],
						MEDIA_LNK_FL_ENABLED,
						MEDIA_ENT_T_V4L2_SUBDEV_DVP,
						&id);
		if (!dvp_pad) {
			pr_err("%s(): failed to find any enabled link\n", __func__);
			ret = -EINVAL;
			goto configure_dvp_failed;
		}
	}

	pr_debug("%s(): configuring dvp subdev for input parameters\n", __func__);

	dvp_subdev = media_entity_to_v4l2_subdev(dvp_pad->entity);

	memset(&property, 0, sizeof(property));
	memset(&mbus_fmt, 0, sizeof(mbus_fmt));
	memset(&timings, 0, sizeof(timings));

	/*
	 * Get the signal property and map to dv_timings and v4l2_mbus_framefmt
	 */
	ret = stm_hdmirx_route_get_signal_property(hdmirx_ctx->rxhdl, &property);
	if (ret) {
		pr_err("%s(): failed to get signal property\n", __func__);
		goto configure_dvp_failed;
	}

	/*
	 * Convert signal properties to dv_timings and v4l2_mbus_framefmt
	 */
	hdmirx_route_convert_stm_timings(&property.timing, &timings);
	ret = hdmirx_route_convert_stm_format(&property, &mbus_fmt);
	if (ret) {
		pr_err("%s(): unsupported format found\n", __func__);
		goto configure_dvp_failed;
	}

	/*
	 * Configure dvp for input: dv_timings and format
	 */
	ret = v4l2_subdev_call(dvp_subdev, video, s_dv_timings, &timings);
	if (ret) {
		pr_err("%s(): failed to set dvp with input timings\n", __func__);
		goto configure_dvp_failed;
	}

	ret = v4l2_subdev_call(dvp_subdev, video, s_mbus_fmt, &mbus_fmt);
	if (ret)
		pr_err("%s(): failed to set format on DVP\n", __func__);

configure_dvp_failed:
	mutex_unlock(&hdmirx_ctx->vidsetup_lock);
	return ret;
}
#endif

/**
 * hdmirx_route_connect_dvp() - connect hdmirx route to dvp capture
 */
static int hdmirx_route_connect_dvp(const struct media_pad *local,
					const struct media_pad *remote)
{
	int ret = -EINVAL;

#ifdef CONFIG_STLINUXTV_DVP
	struct hdmirx_ctx_s *hdmirx_ctx;

	pr_debug("%s(): connecting hdmrx route to dvp\n", __func__);

	hdmirx_ctx = stm_media_pad_to_hdmirx_route(local);

	/*
	 * Before connecting, we can check if the DVP is already connected
	 * to some other route or not, but, this is not the case, as there's
	 * only a single route and it seems overkill at the moment to cater.
	 */

	/*
	 * Configure the dvp capture input params
	 */
	ret = hdmirx_route_configure_dvp(hdmirx_ctx, remote);
	if (ret) {
		pr_debug("%s(): failed to configure dvp for capture\n", __func__);
		ret = 0;
	}
#endif

	return ret;
}

/**
 * hdmirx_route_disconnect_dvp() - disconnect hdmirx route from dvp capture
 */
static int hdmirx_route_disconnect_dvp(const struct media_pad *local,
					const struct media_pad *remote)
{
	int ret = -EINVAL;

#ifdef CONFIG_STLINUXTV_DVP
	struct hdmirx_ctx_s *hdmirx_ctx;

	pr_debug("%s(): connecting hdmrx route to dvp\n", __func__);

	hdmirx_ctx = stm_media_pad_to_hdmirx_route(local);

	/*
	 * Tell the dvp, that the input format is unstable now,
	 * as, hdmirx-route is going to be disconnected.
	 */
	ret = hdmirx_route_set_dvp_fmt_unstable(hdmirx_ctx, remote);
	if (ret) {
		pr_debug("%s(): failed to mark dvp input unstable\n", __func__);
		ret = 0;
	}
#endif

	return ret;
}

/**
 * hdmirx_route_configure_audiodec() - configure audio decoder for capture
 * @hdmirx_ctx: hdmirx route context
 * @dvp_pad   : audio decoder media pad or NULL
 * Configure audio with input parameters. This is called from link_setup as
 * well as from events coming from hdmirx route.
 */
#ifdef CONFIG_STLINUXTV_I2S_CAPTURE
static int hdmirx_route_configure_audiodec(struct hdmirx_ctx_s *hdmirx_ctx,
						const struct media_pad *auddec_pad)
{
	int ret = 0;
	struct v4l2_subdev *auddec_subdev;
	stm_hdmirx_audio_property_t audio_property;
	stm_hdmirx_signal_property_t property;

	MUTEX_INTERRUPTIBLE(&hdmirx_ctx->vidsetup_lock);

	if (!auddec_pad) {
		/*
		 * If auddec_pad is NULL, then try finding out the remote pad
		 * connection if it at all exists
		 */
		int id = 0;

		auddec_pad = stm_media_find_remote_pad_with_type
						(&hdmirx_ctx->pads[HDMIRX_AUD_SRC_PAD],
						MEDIA_LNK_FL_ENABLED,
						MEDIA_ENT_T_V4L2_SUBDEV_AUDIO_DECODER,
						&id);
		if (!auddec_pad) {
			pr_err("%s(): failed to find any enabled link\n", __func__);
			ret = -EINVAL;
			goto configure_auddec_failed;
		}
	}

	pr_debug("%s(): configuring auddec subbdev for input parameters\n", __func__);

	auddec_subdev = media_entity_to_v4l2_subdev(auddec_pad->entity);

	memset(&property, 0, sizeof(property));
	memset(&audio_property, 0, sizeof(audio_property));

	/*
	 * Get the signal property and check for HDMI signal
	 */
	ret = stm_hdmirx_route_get_signal_property(hdmirx_ctx->rxhdl, &property);
	if (ret) {
		pr_err("%s(): failed to get signal property\n", __func__);
		goto configure_auddec_failed;
	}
	if (property.signal_type != STM_HDMIRX_SIGNAL_TYPE_HDMI) {
		pr_err("%s(): No audio signal, so, no need to configure audio capture\n", __func__);
		ret = -EINVAL;
		goto configure_auddec_failed;
	}

	/*
	 * Disable the capture now, since audio property changed.
	 */
	ret = stm_hdmirx_route_set_audio_out_enable(hdmirx_ctx->rxhdl, false);
	if (ret) {
		pr_err("%s(): failed to disable audio capture from hdmirx route\n", __func__);
		goto configure_auddec_failed;
	}

	/*
	 * Get the signal audio properties
	 */
	ret = stm_hdmirx_route_get_audio_property(hdmirx_ctx->rxhdl, &audio_property);
	if (ret) {
		pr_err("%s(): failed to get audio properties\n", __func__);
		goto configure_auddec_failed;
	}

	/*
	 * Configure audio decoder with audio properties
	 */
	ret = v4l2_subdev_call(auddec_subdev, core, ioctl,
				VIDIOC_STI_S_AUDIO_FMT, &audio_property);
	if (ret) {
		pr_err("%s(): failed to set the audio properties\n", __func__);
		goto configure_auddec_failed;
	}

	/*
	 * Enable the audio out from hdmirx, so, that it is available before
	 * SE reader starts delivering data to play stream
	 */
	ret = stm_hdmirx_route_set_audio_out_enable(hdmirx_ctx->rxhdl, true);
	if (ret)
		pr_err("%s(): failed to set audio enable from hdmirx route\n", __func__);

configure_auddec_failed:
	mutex_unlock(&hdmirx_ctx->vidsetup_lock);
	return ret;
}
#endif

/**
 * hdmirx_route_connect_auddec() - connect hdmirx route to audio decoder
 */
static int hdmirx_route_connect_auddec(const struct media_pad *local,
						const struct media_pad *remote)
{
	int ret = -EINVAL;

#ifdef CONFIG_STLINUXTV_I2S_CAPTURE
	struct hdmirx_ctx_s *hdmirx_ctx;

	pr_debug("%s(): connecting hdmrx route to audio decoder\n", __func__);

	hdmirx_ctx = stm_media_pad_to_hdmirx_route(local);

	/*
	 *  Configure audio decoder for capture
	 */
	ret = hdmirx_route_configure_audiodec(hdmirx_ctx, remote);
	if (ret) {
		pr_debug("%s(): failed to configure audio decoder for capture\n", __func__);
		ret = 0;
	}

#endif
	return ret;
}

/**
 * hdmirx_route_disconnect_auddec() - disconnect hdmirx route from audio decoder
 */
static int hdmirx_route_disconnect_auddec(const struct media_pad *local,
						const struct media_pad *remote)
{
	int ret = -EINVAL;

#ifdef CONFIG_STLINUXTV_I2S_CAPTURE
	struct hdmirx_ctx_s *hdmirx_ctx;

	pr_debug("%s(): connecting hdmrx route to audio decoder\n", __func__);

	hdmirx_ctx = stm_media_pad_to_hdmirx_route(local);

	/*
	 *  Configure audio decoder for capture
	 */
	ret = hdmirx_route_set_auddec_fmt_unstable(hdmirx_ctx, NULL);
	if (ret) {
		pr_debug("%s(): failed to set input of audio decoder unstable\n", __func__);
		ret = 0;
	}

#endif
	return ret;
}

/** hdmirx_link_setup() - The link setup callback for the hdmirx entity.
 * This is the place where the ports are connected to the route and the routes
 *  are connected to the video planes.
 *
 * @note
 *  When any link between the Routes and ports/planes are enabled, then

 *    1. The Routes need to be opened.
 *    2. If the target entity is vidextin_hdmirx then the  corresponding port should
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
 *    @param[in] *entity The pointer to the hdmirx entity
 *    @param[in] *local The local pad for the link setup
 *    @param[in] *remote The remote pad for the link setup
 *    @param[in] flags The link flags
 *    @return 0 if successful.
 *      -EINVAL on error.
 */
static int hdmirx_link_setup(struct media_entity *entity,
			     const struct media_pad *local,
			     const struct media_pad *remote, u32 flags)
{
	int ret = 0;

	if (!(entity && local && remote))
		return -EINVAL;

	switch (remote->entity->type) {
	case MEDIA_ENT_T_V4L2_SUBDEV_VIDEXTIN_HDMIRX:
		/* do the port to Route connection here */
		if ((flags & MEDIA_LNK_FL_ENABLED)) {
			_dbg("  Connecting the port to the Route  ");
			ret = set_port_rx_connect(local, remote);
		} else {
			ret = unset_port_rx_connect(local, remote);
		}

		break;

	case MEDIA_ENT_T_V4L2_SUBDEV_PLANE_VIDEO:
		/* Do the plane to route connection here */
		if ((flags & MEDIA_LNK_FL_ENABLED)) {
			ret = con_hdmirx_plane(local, remote, flags);
		} else {
			ret = discon_hdmirx_plane(local, remote);
		}

		break;
	case MEDIA_ENT_T_ALSA_SUBDEV_MIXER:
		/* Do the mixer connections here */
		if ((flags & MEDIA_LNK_FL_ENABLED)) {
			ret = con_hdmirx_mixer(local, remote);
			_dbg("Connected to mixer %d" , ret);
		} else {
			ret = discon_hdmirx_mixer(local, remote);
		}

		break;
	case MEDIA_ENT_T_DEVNODE_V4L:
		/* Handle the device node to Route connection here */
		break;

	/*
	 * This connection is only valid if the SoC is containing a
	 * DVP hardware. This does not map to anyother hw IP
	 */
	case MEDIA_ENT_T_V4L2_SUBDEV_DVP:
		if (flags & MEDIA_LNK_FL_ENABLED)
			ret = hdmirx_route_connect_dvp(local, remote);
		else
			ret = hdmirx_route_disconnect_dvp(local, remote);

		break;

	/*
	 * This is specific to audio capture from I2S bus and then
	 * passing the information to v4l2 audio decoder subdev.
	 */
	case MEDIA_ENT_T_V4L2_SUBDEV_AUDIO_DECODER:
		if (flags & MEDIA_LNK_FL_ENABLED)
			ret = hdmirx_route_connect_auddec(local, remote);
		else
			ret = hdmirx_route_disconnect_auddec(local, remote);

		break;

	default:
		break;
	}
	return ret;
}

static const struct v4l2_subdev_ops vidextin_ops = {
};

static const struct media_entity_operations vidextin_media_ops = {
	.link_setup = vidextin_hdmirx_link_setup,
};

/**
 * hdmirx_route_core_ioctl() - core ioctl for hdmirx subdev (route)
 */
static long hdmirx_route_core_ioctl(struct v4l2_subdev *sd,
					unsigned int cmd, void *arg)
{
	long ret = 0;
	struct hdmirx_ctx_s *route_ctx = v4l2_get_subdevdata(sd);

	switch(cmd) {
	case VIDIOC_SUBDEV_STI_G_AUDIO_FMT:
	{
		stm_hdmirx_signal_property_t property;
		stm_hdmirx_audio_property_t audio_property;
		struct stm_v4l2_subdev_audio_fmt *fmt = arg;
		struct stm_v4l2_audio_fmt audio_fmt;

		if (fmt->pad != 2) {
			pr_err("%s(): pad = 2 valid for audio\n", __func__);
			break;
		}

		/*
		 * If no audio signal, no audio properties
		 */
		memset(&property, 0, sizeof(property));
		ret = stm_hdmirx_route_get_signal_property(route_ctx->rxhdl, &property);
		if (ret) {
			pr_err("%s(): failed to get signal property\n", __func__);
			break;
		}
		if (property.signal_type != STM_HDMIRX_SIGNAL_TYPE_HDMI) {
			pr_err("%s(): It's DVI signal, so, cannot get audio properties\n", __func__);
			break;
		}

		/*
		 * Get the signal audio properties
		 */
		memset(&audio_property, 0, sizeof(audio_property));
		ret = stm_hdmirx_route_get_audio_property(route_ctx->rxhdl, &audio_property);
		if (ret) {
			pr_err("%s(): failed to get audio properties\n", __func__);
			break;
		}

		/*
		 * Fill in the values
		 */
		memset(&audio_fmt, 0, sizeof(audio_fmt));
		audio_fmt.codec = map_stm_to_v4l2_codec(audio_property.coding_type);
		audio_fmt.channel_count = audio_property.channel_count;
		audio_fmt.sampling_frequency = audio_property.sampling_frequency;
		ret = copy_to_user((void *)&fmt->fmt, (void *)&audio_fmt, sizeof(audio_fmt));
		if (ret)
			pr_err("%s(): failed to copy data to user\n", __func__);

		break;
	}

	default:
		ret = -EINVAL;

	}

	return ret;
}

/**
 * hdmirx_queue_initial_event() - queue the intial status as event
 */
static void hdmirx_queue_initial_event(struct hdmirx_ctx_s *route_ctx,
				struct v4l2_subscribed_event *sev, unsigned elems)
{
	bool status;
	/*
	 * id is directly related to what event we propagate.
	 * 0 is for HPD
	 * 1 is for video
	 * 2 is for audio
	 * so, sev->type can be skipped
	 */
	sev->elems = elems;

	switch (sev->id) {
	case HDMIRX_SNK_PAD:

		status = hdmirx_route_get_plug_status(route_ctx);
		route_ctx->hpd_state = status;
		route_event_queue_fh(sev->fh, ROUTE_EVT_HOTPLUG, status);

		if (!status) {
			route_event_queue_fh(sev->fh, ROUTE_EVT_SIG, 0);
			break;
		}

		status = hdmirx_route_detect_signal(route_ctx);
		route_event_queue_fh(sev->fh, ROUTE_EVT_SIG, status);

		break;

	case HDMIRX_VID_SRC_PAD:
	case HDMIRX_AUD_SRC_PAD:
	{
		int ret;
		__u32 status;
		stm_hdmirx_signal_property_t property;

		status = hdmirx_route_detect_signal(route_ctx);
		if (!status) {
			route_event_queue_fh(sev->fh, ROUTE_EVT_VIDEO, 0);
			route_event_queue_fh(sev->fh, ROUTE_EVT_AUDIO, 0);
			break;
		}

		ret = stm_hdmirx_route_get_signal_property(route_ctx->rxhdl, &property);
		if (ret) {
			pr_err("%s(): failed to get signal property\n", __func__);
			break;
		}

		if (!property.timing.hactive && !property.timing.vActive)
			status = 0;
		else
			status = V4L2_EVENT_SRC_CH_RESOLUTION;

		route_event_queue_fh(sev->fh, ROUTE_EVT_VIDEO, status);

		if (property.signal_type != STM_HDMIRX_SIGNAL_TYPE_HDMI)
			status = 0;
		else
			status = V4L2_EVENT_SRC_CH_RESOLUTION;

		route_event_queue_fh(sev->fh, ROUTE_EVT_AUDIO, status);

		break;
	}

	}
}

/**
 * hdmirx_event_add() - add into internal link list the subscriber
 */
static int hdmirx_event_add(struct v4l2_subscribed_event *sev, unsigned elems)
{
	struct v4l2_fh *fh = sev->fh;
	struct v4l2_subdev *sd = vdev_to_v4l2_subdev(fh->vdev);
	struct hdmirx_ctx_s *route_ctx = v4l2_get_subdevdata(sd);

	/*
	 * If initial status is requested, we push it to the event queue
	 */
	if (sev->flags & V4L2_EVENT_SUB_FL_SEND_INITIAL)
		hdmirx_queue_initial_event(route_ctx, sev, elems);

	return 0;
}

/**
 * hdmirx_event_replace() - replace the event in the list
 */
static void hdmirx_event_replace(struct v4l2_event *old,
					const struct v4l2_event *new)
{
	/*
	 * We toggle the state of events
	 */
	switch (new->id) {
	case HDMIRX_SNK_PAD:
		old->u.data[0] = new->u.data[0];
		break;

	case HDMIRX_VID_SRC_PAD:
	case HDMIRX_AUD_SRC_PAD:
		old->u.src_change.changes = new->u.src_change.changes;
		break;
	}
}

/*
 * hdmirx event subscription handling ops
 * Implement merge, if queue depth > 1
 */
struct v4l2_subscribed_event_ops hdmirx_route_event_ops = {
	.add = hdmirx_event_add,
	.replace = hdmirx_event_replace,
};

/**
 * hdmirx_route_core_subscribe_event() - subscribe to hdmirx events
 */
static int hdmirx_route_core_subscribe_event(struct v4l2_subdev *sd,
					struct v4l2_fh *fh,
					struct v4l2_event_subscription *sub)
{
	int ret = 0;
	struct hdmirx_ctx_s *route_ctx = v4l2_get_subdevdata(sd);

	/*
	 * Sanity test before we subscribe for any events
	 */
	if (sub->id > 2) {
		pr_err("%s(): Invalid pad for any event\n", __func__);
		ret = -EINVAL;
		goto subscribe_failed;
	}

	if (((sub->type == V4L2_EVENT_STI_HOTPLUG) ||
			(sub->type == V4L2_EVENT_STI_HDMI_SIG)) && sub->id) {
		pr_err("%s(): Hotplug/signal status event is from pad 0 only\n", __func__);
		ret = -EINVAL;
		goto subscribe_failed;
	}

	if ((sub->type == V4L2_EVENT_SOURCE_CHANGE) && sub->id <= 0) {
		pr_err("%s(): pad=1 for video and pad = 2 for audio events\n", __func__);
		ret = -EINVAL;
		goto subscribe_failed;
	}

	MUTEX_INTERRUPTIBLE(&route_ctx->vidsetup_lock);

	/*
	 * Subscribe to events
	 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 5, 0))
	ret = v4l2_event_subscribe(fh, sub, HDMIRX_EVENT_QUEUE_DEPTH);
#else
	ret = v4l2_event_subscribe(fh, sub,
			HDMIRX_EVENT_QUEUE_DEPTH, &hdmirx_route_event_ops);
#endif
	mutex_unlock(&route_ctx->vidsetup_lock);

	if (ret)
		pr_err("%s(): failed to return error\n", __func__);

subscribe_failed:
	return ret;
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 9, 0))
static  int hdmirx_route_event_unsubscribe(struct v4l2_subdev *sd,
		struct v4l2_fh *fh, struct v4l2_event_subscription *sub)
{
	return v4l2_event_unsubscribe(fh, sub);
}
#endif

/**
 * hdmirx_route_get_fmt() - get the audio/video pad fmt
 */
static int hdmirx_route_get_fmt(struct v4l2_subdev *sd,
			struct v4l2_subdev_fh *fh, struct v4l2_subdev_format *format)
{
	int ret = 0;
	struct hdmirx_ctx_s *route_ctx;
	stm_hdmirx_signal_property_t property;

	/*
	 * Get format support only for audio/video pad
	 */
	if (format->pad != HDMIRX_VID_SRC_PAD) {
		pr_err("%s(): pad = 1/2 is valid for getting fmt\n", __func__);
		ret = -EINVAL;
		goto get_fmt_done;
	}

	route_ctx = v4l2_get_subdevdata(sd);

	MUTEX_INTERRUPTIBLE(&route_ctx->vidsetup_lock);

	/*
	 * Get the signal property and map to v4l2_mbus_framefmt
	 */
	ret = stm_hdmirx_route_get_signal_property(route_ctx->rxhdl, &property);
	if (ret) {
		pr_err("%s(): failed to get signal property\n", __func__);
		goto get_fmt_unlock_done;
	}

	/*
	 * Map to mbus fmt
	 */
	ret = hdmirx_route_convert_stm_format(&property, &format->format);
	if (ret)
		pr_err("%s(): invalid property received\n", __func__);

get_fmt_unlock_done:
	mutex_unlock(&route_ctx->vidsetup_lock);
get_fmt_done:
	return ret;
}

/**
 * hdmirx_route_pad_get_edid() - get edid from hdmirx route
 */
static int hdmirx_route_pad_get_edid(struct v4l2_subdev *sd,
					struct v4l2_subdev_edid *edid)
{
	int ret = 0;
	uint8_t *buf = edid->edid;
	stm_hdmirx_port_h port;
	struct hdmirx_ctx_s *route_ctx;
	short unsigned int max_blocks, start_block = edid->start_block;

	/*
	 * Only sink pad is connected to port, rest pads are invalid
	 */
	if (edid->pad != HDMIRX_SNK_PAD) {
		pr_err("%s(): pad = 0 is valid for getting edid\n", __func__);
		ret = -EINVAL;
		goto get_edid_done;
	}

	route_ctx = v4l2_get_subdevdata(sd);

	MUTEX_INTERRUPTIBLE(&route_ctx->vidsetup_lock);

	/*
	 * Get the port connected to route
	 */
	port = hdmirx_route_find_port(route_ctx);
	if (!port) {
		pr_err("%s(): No port connected to this route\n", __func__);
		ret = -ENOTCONN;
		goto unlock_get_edid_done;
	}

	/*
	 * Read edid from the first block to find out how many extended
	 * blocks information we have. If the read is requested from
	 * block0, we use the copied data, else discard and store the
	 * requested data.
	 */
	ret = stm_hdmirx_port_read_edid_block(port, 0, (uint8_t(*)[128])buf);
	if (ret) {
		pr_err("%s(): failed to read of block 0\n", __func__);
		ret = -ENODATA;
		goto unlock_get_edid_done;
	}

	/*
	 * Update about the total edid blocks available to the application
	 */
	max_blocks = 1 + buf[0x7E];
	if (max_blocks < edid->start_block) {
		pr_err("%s(): Requested start block doesn't exist\n", __func__);
		memset(buf, 0, 128);
		ret = -ENODATA;
		goto unlock_get_edid_done;
	}

	edid->blocks = min_t(int, max_blocks, edid->blocks);
	if ((edid->start_block + edid->blocks) > max_blocks) {
		pr_err("%s(): lesser number of blocks are available\n", __func__);
		edid->blocks -= edid->start_block;
	}

	pr_debug("%s(): EDID max blocks available: %d\n", __func__, max_blocks);

	/*
	 * If read is requested from block0, then we use this one, else discard
	 */
	if (!edid->start_block) {
		buf += 128;
		start_block = 1;
	} else {
		memset(buf, 0, 128);
	}

	/*
	 * Fetch the edid starting from the requested block
	 */
	for (; start_block < (edid->start_block + edid->blocks); start_block++) {
		ret = stm_hdmirx_port_read_edid_block(port,
					start_block, (uint8_t(*)[128])buf);
		if (ret) {
			pr_err("%s(): failed to read block %d\n", __func__, start_block);
			break;
		}
		buf += 128;
	}

unlock_get_edid_done:
	mutex_unlock(&route_ctx->vidsetup_lock);
get_edid_done:
	return ret;
}

/**
 * hdmirx_route_pad_set_edid() - set edid on hdmirx route
 */
static int hdmirx_route_pad_set_edid(struct v4l2_subdev *sd,
					struct v4l2_subdev_edid *edid)
{
	stm_hdmirx_port_h port;
	struct hdmirx_ctx_s *route_ctx;
	uint8_t *buf = edid->edid;
	int ret = 0, i;

	/*
	 * Only sink pad is connected to port, rest pads are invalid
	 */
	if (edid->pad != HDMIRX_SNK_PAD) {
		pr_err("%s(): pad = 0 is the valid for setting edid\n", __func__);
		ret = -EINVAL;
		goto set_edid_done;
	}

	/*
	 * The start block is always 0
	 */
	if (edid->start_block != 0) {
		pr_err("%s(): set start_block = 0 for setting edid\n", __func__);
		ret = -EINVAL;
		goto set_edid_done;
	}

	route_ctx = v4l2_get_subdevdata(sd);

	MUTEX_INTERRUPTIBLE(&route_ctx->vidsetup_lock);

	/*
	 * Get the port connected to route
	 */
	port = hdmirx_route_find_port(route_ctx);
	if (!port) {
		pr_err("%s(): No port connected to this route\n", __func__);
		ret = -ENOTCONN;
		goto unlock_set_edid_done;
	}

	/*
	 * Erase EDID if edid->blocks = 0
	 */
	if (!edid->blocks) {
		uint8_t zero_edid[128];
		memset(zero_edid, 0, sizeof(zero_edid));

		ret = stm_hdmirx_port_update_edid_block(port,
					0, (uint8_t (*)[128])zero_edid);
		if (ret)
			pr_err("%s(): failed to erase EDID\n", __func__);

		/*
		 * Prevent source to read EDID
		 */
		ret = stm_hdmirx_port_set_hpd_signal(port, STM_HDMIRX_PORT_HPD_STATE_LOW);
		if (ret)
			pr_err("%s(): failed to assert HPD low\n", __func__);

		goto unlock_set_edid_done;
	}

	/*
	 * Force in a hotplug, so, that the source read edid again
	 */
	ret = stm_hdmirx_port_set_hpd_signal(port, STM_HDMIRX_PORT_HPD_STATE_LOW);
	if (ret) {
		pr_err("%s(): failed to assert HPD low\n", __func__);
		goto unlock_set_edid_done;
	}

	/*
	 * Set the EDID information
	 */
	for (i = 0; i < edid->blocks; i++) {
		ret = stm_hdmirx_port_update_edid_block(port,
						i, (uint8_t(*)[128])buf);
		if (ret) {
			pr_err("%s(): failed to set EDID for block: %d\n", __func__, i);
			edid->blocks = i;
			ret = -E2BIG;
			goto unlock_set_edid_done;
		}
		buf += 128;
	}

	ret = stm_hdmirx_port_set_hpd_signal(port, STM_HDMIRX_PORT_HPD_STATE_HIGH);
	if (ret)
		pr_err("%s(): failed to assert HPD high\n", __func__);

unlock_set_edid_done:
	mutex_unlock(&route_ctx->vidsetup_lock);
set_edid_done:
	return ret;
}

/*
 * hdmirx-route subdev pad ops
 */
static struct v4l2_subdev_pad_ops hdmirx_route_subdev_pad_ops = {
	.get_fmt = hdmirx_route_get_fmt,
	.get_edid = hdmirx_route_pad_get_edid,
	.set_edid = hdmirx_route_pad_set_edid,
};

/*
 * hdmirx-route subdev core ops
 */
static const struct v4l2_subdev_core_ops hdmirx_route_subdev_core_ops = {
	.ioctl =  hdmirx_route_core_ioctl,
	.subscribe_event = hdmirx_route_core_subscribe_event,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 9, 0))
	.unsubscribe_event = hdmirx_route_event_unsubscribe,
#else
	.unsubscribe_event = v4l2_event_subdev_unsubscribe,
#endif
};

/*
 * hdmirx_route subdev ops
 */
static const struct v4l2_subdev_ops hdmirx_route_subdev_ops = {
	.core = &hdmirx_route_subdev_core_ops,
	.pad = &hdmirx_route_subdev_pad_ops,
};

static const struct media_entity_operations hdmirx_media_ops = {
	.link_setup = hdmirx_link_setup,
};

/** vidextin_release() -release the vidextin entity.
 * @pre The HDMI Driver should be existing and initialized.
 * @param[in] nb_ports The nukber of hdmi ports available.
 */
static void vidextin_release(int nb_ports)
{
	int i, ret;
	struct vidextin_hdmirx_ctx_s *ctx = &hdmiinput_shared.vidextin_hdmirx;
	struct v4l2_subdev *sd = &ctx->v4l2_vidext_sd;
	struct media_pad *pad = ctx->port_pad;
	struct media_entity *me = &sd->entity;

	kfree(pad);
	stm_media_unregister_v4l2_subdev(sd);
	media_entity_cleanup(me);
	for (i = 0; i < nb_ports; i++)
		stm_hdmirx_port_close(ctx->port[i].port_hdl);

	kfree(ctx->port);
	ret = stm_event_subscription_delete(ctx->extevtsubs);
}

/** vidextin_init() - Initialise the vidextin entity.
 * @pre The HDMI Driver should be existing and initialized.
 * @param[in] *extinctx The VidExtin hdmirx context.
 * @param[in] nb_ports The nukber of hdmi ports available.
 * @return 0 if successful.
 *   -EINVAL on error.
 */
static int vidextin_init(struct vidextin_hdmirx_ctx_s *extinctx, int nb_ports)
{
	int ret = 0;
	int i = 0, j = 0;

	struct v4l2_subdev *sd = NULL;
	struct media_pad *pad = NULL;
	struct media_entity *me = NULL;
	stm_event_subscription_entry_t *evtentry;

	/* Sanity check of the parameters */
	if (!extinctx || (nb_ports > MAX_NO_PORTS)) {
		return -EINVAL;
	}

	/* vairable initialization */
	sd = &extinctx->v4l2_vidext_sd;
	pad = extinctx->port_pad;
	me = &sd->entity;
	extinctx->hdmidev = &hdmiinput_shared;
	extinctx->nb_ports = nb_ports;

	pad = kzalloc(sizeof(*(pad)) * nb_ports, GFP_KERNEL);
	if (!pad) {
		_err("error allocating mem for media pads");
		return -ENOMEM;
	}
	extinctx->port_pad = pad;
	/* subdev init for vidextIn */
	v4l2_subdev_init(sd, &vidextin_ops);

	snprintf(sd->name, sizeof(sd->name), "vidextin-hdmirx");
	v4l2_set_subdevdata(sd, extinctx);

	/*Find the number of HDMI ports by probing the STKPI HDMI driver here */
	for (i = 0; i < nb_ports; ++i) {
		pad[i].flags = MEDIA_PAD_FL_SOURCE;
	}

	me->ops = &vidextin_media_ops;
	me->type = MEDIA_ENT_T_V4L2_SUBDEV_VIDEXTIN_HDMIRX;

	ret = media_entity_init(me, nb_ports, pad, 0);
	if (ret < 0) {
		_err("entity init failed(%d) ", ret);
		goto err1;
	}

	ret = stm_media_register_v4l2_subdev(sd);
	if (ret < 0) {
		_err("error registering output Plug sub-device");
		goto err2;
	}

	extinctx->port =
	    kzalloc(sizeof(*(extinctx->port)) * nb_ports, GFP_KERNEL);
	if (!extinctx->port) {
		_err("error allocating mem for ports");
		goto err3;
	}

	evtentry = kzalloc(sizeof(*(evtentry)) * nb_ports, GFP_KERNEL);
	if (!evtentry) {
		_err("error allocating mem for ports");
		ret = -ENOMEM;
		goto err4;
	}

	/* We have an assymetry here . Probably due to the ARCH of vidextin
	 *  where all the ports are 1 media entity */
	for (i = 0; i < nb_ports; ++i) {
		ret =
		    stm_hdmirx_port_open(extinctx->hdmidev->hdmirxdev_h, i,
					 &extinctx->port[i].port_hdl);
		if (ret != 0) {
			_err("HdmiRx Port Open failed  ");
			goto err5;
		}

		extinctx->port[i].port_id = i;
		extinctx->port[i].extctx = extinctx;

		/* Initialize the evt entry struct */
		evtentry[i].object = extinctx->port[i].port_hdl;
		evtentry[i].event_mask = STM_HDMIRX_PORT_SOURCE_PLUG_EVT;
		evtentry[i].cookie = &(extinctx->port[i]);
	}
	/* Subscribe to events for the ports */
	ret =
	    stm_event_subscription_create(evtentry, nb_ports,
					  &extinctx->extevtsubs);
	if (ret) {
		_err("subs create failed for vidext %d", ret);
		goto err5;
	}

	/* set the call back function for subscribed events */
	ret = stm_event_set_handler(extinctx->extevtsubs, (stm_event_handler_t)
				    hdmiport_evt_handler);
	if (ret) {
		_err("set handler vidext failed");
		goto err6;
	}

	/* Looks like this memory can be freed */
	kfree(evtentry);

	return 0;

err6:
	ret = stm_event_subscription_delete(extinctx->extevtsubs);
	extinctx->extevtsubs = NULL;
err5:
	for (j = 0; j < i; j++)
		stm_hdmirx_port_close(extinctx->port[j].port_hdl);
	kfree(evtentry);
err4:
	kfree(extinctx->port);
err3:
	stm_media_unregister_v4l2_subdev(sd);
err2:
	media_entity_cleanup(me);
err1:
	kfree(pad);
	return ret;

}

/** hdmirx_release() - release the hdmirx entity.
 * @pre The HDMI Driver should be existing and initialized.
 * @param[in] nhdmirx The number of hdmi route available.
 */
static void hdmirx_release(int nhdmirx)
{
	int i;
	struct media_entity *mehdmirx;
	struct hdmirx_ctx_s *hdmirx_context;
	struct v4l2_subdev *sdhdmirx;

	for (i = 0; i < nhdmirx; i++) {
		hdmirx_context = &(hdmiinput_shared.hdmirx[i]);
		sdhdmirx = &hdmirx_context->hdmirx_sd;
		mehdmirx = &sdhdmirx->entity;
		stm_media_unregister_v4l2_subdev(sdhdmirx);
		media_entity_cleanup(mehdmirx);
	}
}

/** hdmirx_init() - Initialise the hdmirx entity.
 * @pre The HDMI Driver should be existing and initialized.
 * @param[in] *hdmirx_context The hdmirx context.
 * @param[in] hdmirx_inst The instance number of the context.
 * @return 0 if successful.
 *   -EINVAL on error.
 */
static int hdmirx_init(struct hdmirx_ctx_s *hdmirx_context, int hdmirx_inst)
{
	struct v4l2_subdev *sdhdmirx = NULL;
	struct media_pad *pad = NULL;
	struct media_entity *mehdmirx = NULL;
	int ret = 0;

	if (!hdmirx_context) {
		return -EINVAL;
	}

	/*
	 * Set up the hdmirx route subdev
	 */
	sdhdmirx = &hdmirx_context->hdmirx_sd;
	pad = hdmirx_context->pads;
	mehdmirx = &sdhdmirx->entity;

	hdmirx_context->hdmidev = &hdmiinput_shared;
	/* subdev init for hdmirx */
	v4l2_subdev_init(sdhdmirx, &hdmirx_route_subdev_ops);
	sdhdmirx->flags = (V4L2_SUBDEV_FL_HAS_EVENTS |
				V4L2_SUBDEV_FL_HAS_DEVNODE);
	snprintf(sdhdmirx->name, sizeof(sdhdmirx->name), "%s%d",
		 STM_HDMIRX_SUBDEV, hdmirx_inst);
	hdmirx_context->route_inst = hdmirx_inst;
	v4l2_set_subdevdata(sdhdmirx, hdmirx_context);

	pad[HDMIRX_SNK_PAD].flags = MEDIA_PAD_FL_SINK;
	pad[HDMIRX_VID_SRC_PAD].flags = MEDIA_PAD_FL_SOURCE;
	pad[HDMIRX_AUD_SRC_PAD].flags = MEDIA_PAD_FL_SOURCE;

	mehdmirx->ops = &hdmirx_media_ops;
	mehdmirx->type = MEDIA_ENT_T_V4L2_SUBDEV_HDMIRX;

	ret =
	    media_entity_init(mehdmirx, ARRAY_SIZE(hdmirx_context->pads), pad,
			      0);
	if (ret < 0) {
		_err("%s: entity init failed(%d) ", __func__, ret);
		return ret;
	}

	ret = stm_media_register_v4l2_subdev(sdhdmirx);
	if (ret < 0) {
		_err("register subdev failed(%d) ", ret);
		goto err1;
	}

	mutex_init(&hdmirx_context->vidsetup_lock);

	return 0;

err1:
	media_entity_cleanup(mehdmirx);
	return ret;

}

/**
 * hdmirx_port_connect_route() - connect hdmirx port to route
 * @port_pad : valid port pad
 * @route_pad: valid route pad
 * Since, we have only 1 route and 1 port to connect to, so,
 * immmutable, enabled connections are made. If ever, we do
 * a multi hdmirx-in SoC, this needs to be changed: Label: MULTI
 */
static int hdmirx_port_connect_route(struct media_pad *port_pad,
					struct media_pad *route_pad)
{
	int ret = 0;

	/*
	 * Connect port with route
	 */
	ret = set_port_rx_connect(route_pad, port_pad);
	if (ret) {
		pr_err("%s(): failed to connect port to route\n", __func__);
		goto connect_done;
	}

	/*
	 * Setup the connection
	 */
	ret = media_entity_create_link(port_pad->entity, 0,
				route_pad->entity, HDMIRX_SNK_PAD,
				(MEDIA_LNK_FL_ENABLED | MEDIA_LNK_FL_IMMUTABLE));
	if (ret) {
		pr_err("%s(): failed to setup link port->route\n", __func__);
		goto connect_failed;
	}

	return 0;

connect_failed:
	unset_port_rx_connect(port_pad, route_pad);
connect_done:
	return ret;
}

/** con_route_plane_entities() - connect route, plane entities.
 * @pre The HDMI Driver should be existing and initialized.
 * @param[in] hdmirx The hdmirx context.
 * @param[in] no_of_route number of the hdmi routes.
 * @return 0 if successful.
 *   error value on error.
 */
static int con_route_plane_entities(struct hdmirx_ctx_s *hdmirx,
				    int no_of_route)
{
#ifdef CONFIG_STLINUXTV_HDMIRX_VIDEO_BYPASS
	struct media_entity *src, *sink;
	int i, ret = 0;

	/* Create disabled links between hdmirx and video planes */
	for (i = 0; i < no_of_route; i++) {
		src = &(hdmirx[i].hdmirx_sd.entity);
		sink =
		    stm_media_find_entity_with_type_first
		    (MEDIA_ENT_T_V4L2_SUBDEV_PLANE_VIDEO);
		while (sink) {
			ret =
			    media_entity_create_link(src, HDMIRX_VID_SRC_PAD,
						     sink, 0, 0);
			if (ret < 0) {
				;
				_err("%s: failed to create link (%d) ",
				     __func__, ret);
				return ret;
			}
			_dbg(" create link route-plane2 ");

			sink =
			    stm_media_find_entity_with_type_next(sink,
								 MEDIA_ENT_T_V4L2_SUBDEV_PLANE_VIDEO);
		}
	}

#endif
	return 0;
}


/** con_route_mixer_entities() - connect route, mixer entities.
 * @pre The HDMI Driver should be existing and initialized.
 * @param[in] hdmirx The hdmirx context.
 * @param[in] no_of_route number of the hdmi routes.
 * @return 0 if successful.
 *   error value on error.
 */
static int con_route_mixer_entities(struct hdmirx_ctx_s *hdmirx,
				    int no_of_route)
{
#ifdef CONFIG_STLINUXTV_HDMIRX_AUDIO_BYPASS
	struct media_entity *src, *sink;
	int i, ret = 0;

	/* Create disabled links between hdmirx and video planes */
	for (i = 0; i < no_of_route; i++) {
		src = &(hdmirx[i].hdmirx_sd.entity);
		sink =
		    stm_media_find_entity_with_type_first
		    (MEDIA_ENT_T_ALSA_SUBDEV_MIXER);
		while (sink) {
			ret =
			    media_entity_create_link(src, HDMIRX_AUD_SRC_PAD,
						     sink, 0, 0);
			if (ret < 0) {
				;
				_err("%s: failed to create link (%d) ",
				     __func__, ret);
				return ret;
			}
			_dbg(" create link route-mixer ");

			sink =
			    stm_media_find_entity_with_type_next(sink,
								 MEDIA_ENT_T_ALSA_SUBDEV_MIXER);
		}
	}

#endif
	return 0;
}


/** stm_v4l2_hdmirx_init() -function used to disconnect the route to the plane.
 * @pre The hdmirx driver should be initialized.
 * @note
 * 1. Create the subdevices and media Entities for hdmirx and vidextin_hdmirx.
 *     Also make sure to register the subdev_ops structure for the subdevices.
 * 2. The links between the vidextin_hdmirx and hdmirx entities should be created
 *     (but not enabled).
 * 3. Similarly link the vidextin_input entity , hdmirx entity to the Capture
 *     device.
 * 4. Open the stkpi hdmi device and store the device handle in HdmiInputShared_t.
 * 5. Initialize the VidExtin_Hdmirx_Context_t object.
 * 6. Find the number of valid Routes and initialize the corresponding number of
 *     HDMIRx_Context_t structures.
 *
 *     @return 0 if successful.
 *     -EINVAL on error.
 */
static int __init stm_v4l2_hdmirx_init(void)
{
	int ret = 0, i, no_of_hdmirx_init = 0;
	stm_hdmirx_device_capability_t capability;

	memset(&hdmiinput_shared, 0, sizeof(hdmiinput_shared));
	ret = stm_hdmirx_device_open(&(hdmiinput_shared.hdmirxdev_h), 0);
	/* we need to handle the case of HDMIRX not present seperatelt */
	if (ret == -ENODEV) {
		_err("HDMIRx Ip not present  ");
		hdmiinput_shared.hdmirxdev_h = NULL;
		return 0;
	}
	if (ret != 0) {
		_err("HDMIRx Open failed  ");
		return ret;
	}

	ret =
	    stm_hdmirx_device_get_capability(hdmiinput_shared.hdmirxdev_h,
					     &capability);
	if (ret != 0) {
		_err("HdmiRx get_capability failed  ");
		goto err1;
	}

	ret =
	    vidextin_init(&hdmiinput_shared.vidextin_hdmirx,
			  capability.number_of_ports);
	if (ret != 0) {
		_err("vidextin_init failed  ");
		goto err1;
	}

	hdmiinput_shared.nb_hdmirx = capability.number_of_routes;
	no_of_hdmirx_init = hdmiinput_shared.nb_hdmirx;
	hdmiinput_shared.hdmirx =
	    kzalloc(sizeof(*(hdmiinput_shared.hdmirx)) *
		    hdmiinput_shared.nb_hdmirx, GFP_KERNEL);

	if (!hdmiinput_shared.hdmirx) {
		_err("error allocating mem for ports");
		ret = -ENOMEM;
		goto err2;
	}

	for (i = 0; i < hdmiinput_shared.nb_hdmirx; ++i) {
		ret = hdmirx_init(&(hdmiinput_shared.hdmirx[i]), i);
		if (ret < 0) {
			_err("v4l2_hdmirx init failed");
			no_of_hdmirx_init = i - 1;
			goto err3;
		}
		hdmiinput_shared.hdmirx[i].hdmidev = &hdmiinput_shared;
	}

	/* Create disabled links between vidextIn and hdmixrx */
	ret = hdmirx_port_connect_route(hdmiinput_shared.vidextin_hdmirx.port_pad,
						      hdmiinput_shared.hdmirx->pads);
	if (ret) {
		_err("failed to create link (%x) ", ret);
		goto err3;
	}

	/* Create disabled links between hdmixrx and plane */
	ret = con_route_plane_entities(hdmiinput_shared.hdmirx,
				       hdmiinput_shared.nb_hdmirx);
	if (ret) {
		_err("failed to create link (%d) ", ret);
		goto err3;
	}

	/* Create disabled links between hdmixrx and mixer */
	ret = con_route_mixer_entities(hdmiinput_shared.hdmirx,
				       hdmiinput_shared.nb_hdmirx);
	if (ret) {
		_err("failed to create link (%d) ", ret);
		goto err3;
	}

	return 0;

err3:
	hdmirx_release(no_of_hdmirx_init);
	kfree(hdmiinput_shared.hdmirx);

err2:
	vidextin_release(capability.number_of_ports);

err1:
	stm_hdmirx_device_close(hdmiinput_shared.hdmirxdev_h);
	memset(&hdmiinput_shared, 0, sizeof(hdmiinput_shared));
	return ret;

}

module_init(stm_v4l2_hdmirx_init);

/** stm_v4l2_hdmirx_exit() - fucntion called when exiting the hdmirx drivers
 *  @return 0 if successful.
 */
static void __exit stm_v4l2_hdmirx_exit(void)
{
	unset_port_rx_connect(hdmiinput_shared.hdmirx->pads,
				hdmiinput_shared.vidextin_hdmirx.port_pad);
	hdmirx_release(hdmiinput_shared.nb_hdmirx);
	kfree(hdmiinput_shared.hdmirx);
	vidextin_release(hdmiinput_shared.vidextin_hdmirx.nb_ports);
	stm_hdmirx_device_close(hdmiinput_shared.hdmirxdev_h);
	memset(&hdmiinput_shared, 0, sizeof(hdmiinput_shared));
	return;
}

module_exit(stm_v4l2_hdmirx_exit);

MODULE_DESCRIPTION("STLinuxTV HDMIRx driver");
MODULE_AUTHOR("STMicroelectronics Limited");
MODULE_LICENSE("GPL");
