/************************************************************************
 * Copyright (C) 2013 STMicroelectronics. All Rights Reserved.
 *
 * This file is part of the STLinuxTV Library.
 *
 * STLinuxTV is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation.
 *
 * STLinuxTV is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with player2; see the file COPYING.  If not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * The STLinuxTV Library may alternatively be licensed under a proprietary
 * license from ST.
 * Implementation of HDMI controls using V4L2
 *************************************************************************/
#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/kthread.h>

#include <media/v4l2-dev.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-event.h>

#include <linux/gpio.h>
#include <linux/stm/pad.h>

#include <stmdisplay.h>
#include <linux/stm/stmcoredisplay.h>
#include <linux/stm/hdmiplatform.h>
#include <linux/stm/stmcorehdmi.h>

#include <linux/stmedia/stmedia.h>
#include <stmhdmi_v4l2_ctrls.h>
#include "stmhdmi_device.h"
#include "stmhdmi.h"
#include "stm_v4l2_common.h"

/*
 * struct stm_v4l2_hdmi_device
 * This is the HDMI-V4L2 structure
 */
struct stm_v4l2_hdmi_device {
	struct mutex mutex;
	struct video_device vdev;
	struct media_pad *pads;
	struct stm_hdmi *hdmi;
};

static struct stm_v4l2_hdmi_device stm_v4l2_hdmi_device;

static char *hdmi0;
module_param(hdmi0, charp, 0444);
MODULE_PARM_DESC(hdmi0, "[enable|disable]");

static int thread_stl_hdmid[2] = { SCHED_NORMAL, 0 };
module_param_array_named(thread_STL_Hdmid, thread_stl_hdmid, int, NULL, 0644);
MODULE_PARM_DESC(thread_stl_hdmid, "STL-Hdmid thread:s(Mode),p(Priority)");

/**
 * stm_v4l2_queue_event
 * HDMI manager thread is capable of processing HDMI events. This
 * function will be used by this thread to queue the events when they
 * are available. Only the subscribed events are queued.
 */
void stm_v4l2_event_queue(int evt_type, int evt_value)
{
	struct v4l2_event evt;

	evt.type = evt_type;
	evt.u.data[0] = evt_value;

	v4l2_event_queue(&stm_v4l2_hdmi_device.vdev, &evt);
}

/**
 * stm_hdmi_g_ext_ctrls
 * This function get the HDMI controls for the application.
 */
static long stm_hdmi_g_ext_ctrls(struct v4l2_ext_control *ctrl,
						struct stm_hdmi *hdmi)
{
	int ret = 0;

	/*
	 * Get the controls now
	 */
	switch(ctrl->id) {
	case V4L2_CID_DV_STM_SPD_VENDOR_DATA:
	{
		int size = ctrl->size;
	
		/*
		 * For getting the string data, application must
		 * provide valid buffer size for it.
		 */
		if (size < HDMI_SPD_INFOFRAME_VENDOR_LENGTH) {
			HDMI_ERROR("Small buffer for vendor name\n");
			ctrl->size = HDMI_SPD_INFOFRAME_VENDOR_LENGTH;
			ret = -ENOSPC;
			break;
		}

		strlcpy(ctrl->string, &hdmi->spd_frame->data
				[HDMI_SPD_INFOFRAME_VENDOR_START],
				HDMI_SPD_INFOFRAME_VENDOR_LENGTH);

		break;
	}

	case V4L2_CID_DV_STM_SPD_PRODUCT_DATA:
	{
		int size = ctrl->size;
	
		/*
		 * For getting the string data, application must
		 * provide valid buffer size for it.
		 */
		if (size < HDMI_SPD_INFOFRAME_PRODUCT_LENGTH) {
			HDMI_ERROR("Small buffer for product name\n");
			ctrl->size = HDMI_SPD_INFOFRAME_PRODUCT_LENGTH;
			ret = -ENOSPC;
			break;
		}

		strlcpy(ctrl->string, &hdmi->spd_frame->data
				[HDMI_SPD_INFOFRAME_PRODUCT_START],
				HDMI_SPD_INFOFRAME_PRODUCT_LENGTH);

		break;
	}

	case V4L2_CID_DV_STM_SPD_IDENTIFIER_DATA:
	{
		ctrl->value = hdmi->spd_frame->data
				[HDMI_SPD_INFOFRAME_SPI_OFFSET];

		break;
			
	}
	case V4L2_CID_DV_STM_TX_AVMUTE:
	{
		ret = stm_display_output_get_control(hdmi->hdmi_output,
				OUTPUT_CTRL_HDMI_AVMUTE, &ctrl->value);
		if (ret)
			HDMI_ERROR("Failed to the AV mute status\n");
	
		break;
	}

	case V4L2_CID_DV_STM_TX_AUDIO_SOURCE:
	{
		stm_display_output_audio_source_t aud_src;

		/*
		 * Get audio source
		 */
		ret = stm_display_output_get_control(hdmi->hdmi_output,
				OUTPUT_CTRL_AUDIO_SOURCE_SELECT, &aud_src);
		if (ret) {
			HDMI_ERROR("Unable to get audio source\n");
			break;
		}
			
		/*
		 * Convert ST control to V4L2 control
		 */
		switch (aud_src) {
		case STM_AUDIO_SOURCE_2CH_I2S:
			ctrl->value = V4L2_DV_STM_AUDIO_SOURCE_PCM;
			break;

		case STM_AUDIO_SOURCE_SPDIF:
			ctrl->value = V4L2_DV_STM_AUDIO_SOURCE_SPDIF;
			break;

		case STM_AUDIO_SOURCE_8CH_I2S:
			ctrl->value = V4L2_DV_STM_AUDIO_SOURCE_8CH_I2S;
			break;

		case STM_AUDIO_SOURCE_NONE:
			ctrl->value = V4L2_DV_STM_AUDIO_SOURCE_NONE;
			break;

		default:
			ret = -EINVAL;
			ctrl->value = ~0;
		}
		
		break;
	}

	case V4L2_DV_STM_TX_OVERSCAN_MODE:
	{
		/*
		 * Get the scan mode for the device
		 */
		ret = stm_display_output_get_control(hdmi->hdmi_output,
				OUTPUT_CTRL_AVI_SCAN_INFO, &ctrl->value);
		if (ret)
			HDMI_ERROR("Unable to get the scan mode\n");

		break;
	}

	case V4L2_CID_DV_STM_TX_CONTENT_TYPE:
	{
		ret = stm_display_output_get_control(hdmi->hdmi_output,
				OUTPUT_CTRL_AVI_CONTENT_TYPE, &ctrl->value);
		if (ret)
			HDMI_ERROR("Unable to get the content type\n");
	
		break;
	}

	case V4L2_CID_DV_TX_MODE:
	{
		int stm_tx_mode = hdmi->hdmi_safe_mode;
		
		/*
		 * There's one-to-one mapping with STHDMI internal
		 * controls, so, this switch case can be removed
		 */
		switch(stm_tx_mode) {
		case STMHDMIIO_SAFE_MODE_DVI:
			ctrl->value = V4L2_DV_TX_MODE_DVI_D;
			break;

		case STMHDMIIO_SAFE_MODE_HDMI:
			ctrl->value = V4L2_DV_TX_MODE_HDMI;
			break;

		default:
			ret = -EINVAL;
			ctrl->value = ~0;
			break;
		}

		break;
	}


	case V4L2_CID_DV_TX_RGB_RANGE:
	{
		enum stmhdmiio_quantization_mode quant_mode;
			
		ret = stm_display_output_get_control(hdmi->hdmi_output,
			OUTPUT_CTRL_AVI_QUANTIZATION_MODE, &quant_mode);

		switch(quant_mode) {
		case STMHDMIIO_QUANTIZATION_AUTO:
			ctrl->value = V4L2_DV_RGB_RANGE_AUTO;
			break;

		case  STMHDMIIO_QUANTIZATION_LIMITED:
			ctrl->value = V4L2_DV_RGB_RANGE_LIMITED;
			break;

		case STMHDMIIO_QUANTIZATION_FULL:
			ctrl->value = V4L2_DV_RGB_RANGE_FULL;
			break;

		default:
			HDMI_ERROR("Invalid Tx RGB range\n");
			ctrl->value = quant_mode;
			break;
		}
	
		break;
	}

	case V4L2_CID_DV_STM_TX_CONTROL:
	{
		ctrl->value = !hdmi->disable;
			
		break;
	}

	default:
		HDMI_ERROR("Invalid control specified, cannot get value\n");
		ret = -EINVAL;
	}

	if (ret) {
		if (signal_pending(current))
			ret = -ERESTARTSYS;
	}

	return ret;
}

/**
 * stm_hdmi_s_ext_ctrls
 * This function sets the HDMI control requested by the application
 */
static long stm_hdmi_s_ext_ctrls(struct v4l2_ext_control *ctrl,
						struct stm_hdmi *hdmi)
{
	int ret = 0;

	/*
	 * Set the controls now.
	 */
	switch (ctrl->id) {
	case V4L2_CID_DV_STM_SPD_VENDOR_DATA:
	{
		int size = ctrl->size;
		char *str = ctrl->string;
		/*
		 * This is a string data, so, there has to be
		 * a valid size set for this in the ctrl block
		 */
		if (!size || ((strlen(str) + 1) < size)) {
			HDMI_ERROR("Invalid size for vendor info\n");
			ret = -EINVAL;
			break;
		}

		/*
		 * TODO: Check the start offset
		 */
		strlcpy(&hdmi->spd_frame->data
				[HDMI_SPD_INFOFRAME_VENDOR_START], str,
				HDMI_SPD_INFOFRAME_VENDOR_LENGTH);
		break;
	}

	case V4L2_CID_DV_STM_SPD_PRODUCT_DATA:
	{	
		int size = ctrl->size;
		char *str = ctrl->string;

		/*
		 * This is a string data, so, there has to be
		 * a valid size set for this in the ctrl block
		 */
		if (!size || ((strlen(str) + 1) < size)) {
			HDMI_ERROR("Invalid size for product info\n");
			ret = -EINVAL;
			break;
		}

		strlcpy(&hdmi->spd_frame->data
			[HDMI_SPD_INFOFRAME_PRODUCT_START], str,
			 HDMI_SPD_INFOFRAME_PRODUCT_LENGTH);
		break;
	}

	case V4L2_CID_DV_STM_SPD_IDENTIFIER_DATA:
	{
		int size = ctrl->size;
		/*
		 * This is a numeric data, so, the size field has to
		 * be set as 0 in the ctrl block.
		 */
		if (size) {
			HDMI_ERROR("Invalid ctrl value sent\n");
			ret = -EINVAL;
			break;
		}

		hdmi->spd_frame->data[HDMI_SPD_INFOFRAME_SPI_OFFSET]
					= (ctrl->value & 0xFF);

		break;
			
	}

	case V4L2_CID_DV_STM_TX_AVMUTE:
	{
		int disable = (ctrl->value ? 1 : 0);
		
		ret = stm_display_output_set_control(hdmi->hdmi_output,
			OUTPUT_CTRL_HDMI_AVMUTE, disable);
		if (ret)
			HDMI_ERROR("Cannot change the AV mute control\n");
		break;
	}

	case V4L2_CID_DV_STM_TX_AUDIO_SOURCE:
	{
		stm_display_output_audio_source_t aud_src = ~0;

		/*
		 * Convert V4L2 control to ST control
		 */
		switch (ctrl->value) {
		case V4L2_DV_STM_AUDIO_SOURCE_2CH_I2S:
		case V4L2_DV_STM_AUDIO_SOURCE_PCM:
			aud_src = STM_AUDIO_SOURCE_2CH_I2S;
			break;

		case V4L2_DV_STM_AUDIO_SOURCE_SPDIF:
			aud_src = STM_AUDIO_SOURCE_SPDIF;
			break;

		case V4L2_DV_STM_AUDIO_SOURCE_8CH_I2S:
			aud_src = STM_AUDIO_SOURCE_8CH_I2S;
			break;

		case V4L2_DV_STM_AUDIO_SOURCE_NONE:
			aud_src = STM_AUDIO_SOURCE_NONE;
			break;

		default:
			HDMI_ERROR("Invalid audio source specified\n");
		}

		if (aud_src == ~0)
			break;
		
		/*
		 * Set audio source
		 */
		ret = stm_display_output_set_control(hdmi->hdmi_output,
			OUTPUT_CTRL_AUDIO_SOURCE_SELECT, aud_src);
		if (ret)
			HDMI_ERROR("Unable to set audio source\n");
		
		break;
	}

	case V4L2_DV_STM_TX_OVERSCAN_MODE:
	{
		if (ctrl->value > V4L2_DV_STM_SCAN_UNDERSCANNED) {
			HDMI_ERROR("Invalid SCAN mode for hdmi device\n");
			ret = -EINVAL;
			break;
		}
			
		/*
		 * Set the scan mode for the device
		 */
		ret = stm_display_output_set_control(hdmi->hdmi_output,
			OUTPUT_CTRL_AVI_SCAN_INFO, ctrl->value);
		if (ret)
			HDMI_ERROR("Unable to set the scan mode\n");

		break;
	}

	case V4L2_CID_DV_STM_TX_CONTENT_TYPE:
	{
		if (ctrl->value > V4L2_DV_STM_CE_CONTENT) {
			HDMI_ERROR("Invalid content type\n");
			ret = -EINVAL;
			break;
		}

		ret = stm_display_output_set_control(hdmi->hdmi_output,
			OUTPUT_CTRL_AVI_CONTENT_TYPE, ctrl->value);
		if (ret)
			HDMI_ERROR("Unable to set the Content type\n");

		break;
	}

	case V4L2_CID_DV_STM_FLUSH_DATA_PACKET_QUEUE:
	{
		if(ctrl->value & V4L2_DV_STM_FLUSH_ACP_QUEUE)
			ret = stm_display_output_flush_metadata(hdmi->
			hdmi_output, STM_METADATA_TYPE_ACP_DATA);

		if(ctrl->value & V4L2_DV_STM_FLUSH_ISRC_QUEUE)
			ret = stm_display_output_flush_metadata(hdmi->
			hdmi_output, STM_METADATA_TYPE_ISRC_DATA);

		if(ctrl->value & V4L2_DV_STM_FLUSH_VENDOR_QUEUE)
			ret = stm_display_output_flush_metadata(hdmi->
			hdmi_output, STM_METADATA_TYPE_VENDOR_IFRAME);

		if(ctrl->value & V4L2_DV_STM_FLUSH_GAMUT_QUEUE)
			ret = stm_display_output_flush_metadata(hdmi->
			hdmi_output, STM_METADATA_TYPE_COLOR_GAMUT_DATA);

		if(ctrl->value & V4L2_DV_STM_FLUSH_NTSC_QUEUE)
			ret = stm_display_output_flush_metadata(hdmi->
			hdmi_output, STM_METADATA_TYPE_NTSC_IFRAME);
		break;
	}

	case V4L2_CID_DV_TX_MODE:
	{
		int stm_tx_mode = ~0;
			
		switch(ctrl->value) {
		case V4L2_DV_TX_MODE_DVI_D:
			stm_tx_mode = STMHDMIIO_SAFE_MODE_DVI;
			break;

		case V4L2_DV_TX_MODE_HDMI:
			stm_tx_mode = STMHDMIIO_SAFE_MODE_HDMI;
			break;

		default:
			ret = -EINVAL;
		}

		if ((stm_tx_mode == hdmi->hdmi_safe_mode) ||
					 (stm_tx_mode == ~0))
			break;

		hdmi->hdmi_safe_mode = stm_tx_mode;

		if(hdmi->status != STM_DISPLAY_DISCONNECTED) {
			hdmi->status = STM_DISPLAY_NEEDS_RESTART;
			hdmi->status_changed = 1;
			wake_up(&hdmi->status_wait_queue);
		}
			
		break;
	}


	case V4L2_CID_DV_TX_RGB_RANGE:
	{
		enum stmhdmiio_quantization_mode quant_mode = ~0;
			
		switch(ctrl->value) {
		case V4L2_DV_RGB_RANGE_AUTO:
			quant_mode = STMHDMIIO_QUANTIZATION_AUTO;
			break;

		case  V4L2_DV_RGB_RANGE_LIMITED:
			quant_mode = STMHDMIIO_QUANTIZATION_LIMITED;
			break;

		case V4L2_DV_RGB_RANGE_FULL:
			quant_mode = STMHDMIIO_QUANTIZATION_FULL;
			break;

		default:
			HDMI_ERROR("Invalid Tx RGB range\n");
			break;
		}
		if (quant_mode == ~0)
			break;	
	
		ret = stm_display_output_set_control(hdmi->hdmi_output,
			OUTPUT_CTRL_AVI_QUANTIZATION_MODE, quant_mode);
		if (ret)
			HDMI_ERROR("Unable to set the specified the RGB range\n");

		break;
	}

	case V4L2_CID_DV_STM_TX_CONTROL:
	{
		int enable = (ctrl->value ? 1 : 0);
		
		/*
		 * If HDMI requested status and current status is same,
		 * then do nothing.
		 */
		if ((enable && !hdmi->disable) ||
				 (!enable && hdmi->disable))
			break;
		
		hdmi->status_changed = 1;
		hdmi->disable = (!enable);
		wake_up(&hdmi->status_wait_queue);
			
		break;
	}

	default:
		ret = -EINVAL;
	}

	if (ret) {
		if (signal_pending(current))
			ret = -ERESTARTSYS;
	}

	return ret;
}

/**
 * stm_v4l2_hdmi_querycap
 * VIDIOC_QUERCAP callback. This function returns the capability of this driver
 */
static int stm_v4l2_hdmi_querycap(struct file *file, void *fh,
					struct v4l2_capability *caps)
{
	strlcpy(caps->driver, "Output Control", sizeof(caps->driver));
	strlcpy(caps->card, "STMicroelectronics", sizeof(caps->card));
	caps->version = LINUX_VERSION_CODE;
	caps->capabilities = V4L2_CAP_VIDEO_OUTPUT;

	return 0;
}

/**
 * stm_v4l2_hdmi_g_ext_ctrls
 * VIDIOC_G_EXT_CTRLS callback. This will process the get controls
 * functionality provided by the driver
 */
static int stm_v4l2_hdmi_g_ext_ctrls(struct file *file, void *fh,
					struct v4l2_ext_controls *ext_ctrls)
{
	struct v4l2_ext_control *ext_ctrl = ext_ctrls->controls;
	struct stm_hdmi *hdmi = video_drvdata(file);
	int index, ret = 0;

	if (ext_ctrls->ctrl_class != V4L2_CTRL_CLASS_DV) {
		HDMI_ERROR("Invalid Control class for HDMI\n");
		ret = -EINVAL;
		goto hdmi_g_ext_ctrls_invalid_error;
	}

	for (index = 0; index < ext_ctrls->count; index++) {
		ret = stm_hdmi_g_ext_ctrls(&ext_ctrl[index], hdmi);
		if (ret) {
			HDMI_ERROR("Failed to get the ext ctrl: %d\n", index);
			ext_ctrls->error_idx = index;
			break;
		}
	}

hdmi_g_ext_ctrls_invalid_error:
	return ret;
}

/**
 * stm_v4l2_hdmi_s_ext_ctrls
 * VIDIOC_S_EXT_CTRLS callback. This will process the set controls
 * functionality provided by the driver
 */
static int stm_v4l2_hdmi_s_ext_ctrls(struct file *file, void *fh,
					struct v4l2_ext_controls *ext_ctrls)
{
	struct v4l2_ext_control *ext_ctrl = ext_ctrls->controls;
	struct stm_hdmi *hdmi = video_drvdata(file);
	int index, ret = 0;

	if (ext_ctrls->ctrl_class != V4L2_CTRL_CLASS_DV) {
		HDMI_ERROR("Invalid Control class for HDMI\n");
		ret = -EINVAL;
		goto hdmi_s_ext_ctrl_invalid_error;
	}

	for (index = 0; index < ext_ctrls->count; index++) {
		ret = stm_hdmi_s_ext_ctrls(&ext_ctrl[index], hdmi);
		if (ret) {
			HDMI_ERROR("Failed to set the ext ctrl: %d\n", index);
			ext_ctrls->error_idx = index;
			break;
		}
	}

hdmi_s_ext_ctrl_invalid_error:
	return ret;
}

/**
 * stm_v4l2_hdmi_subscribe_event
 * VIDIOC_SUBSCRIBE_EVENT callback. Only HPD events will be handled by the
 * driver and so, only it's subscription is allowed to happen
 */
static int STM_V4L2_FUNC1(stm_v4l2_hdmi_subscribe_event,
		 struct v4l2_fh *fh, struct v4l2_event_subscription *evt_s)
{
		int ret = 0;

		if (!(evt_s->type & V4L2_EVENT_STM_TX_HOTPLUG)) {
			HDMI_ERROR("Invalid event subscription type\n");
			ret = -EINVAL;
			goto hdmi_subscribe_event_invalid_error;
		}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0))
		evt_s->flags = V4L2_EVENT_SUB_FL_ALLOW_FEEDBACK;
		ret = v4l2_event_subscribe(fh, evt_s, 1);
#else
		/*
		 * V4L2_EVENT_SUB_FL_ALLOW_FEEDBACK flag should be driven by user
		 */
		ret = v4l2_event_subscribe(fh, evt_s, 1, NULL);
#endif
		if (ret)
			HDMI_ERROR("Unable to subscribe the event\n");

hdmi_subscribe_event_invalid_error:
	return ret;
}

/**
 * stm_v4l2_hdmi_default
 * Any other IOCTL code not supported via video_ioctl2 will be processed here.
 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0))
static long stm_v4l2_hdmi_default(struct file *file, void *fh,
				bool valid_prio, int cmd, void *arg)
#else
static long stm_v4l2_hdmi_default(struct file *file, void *fh,
				bool valid_prio, unsigned int cmd, void *arg)
#endif
{
	struct stm_hdmi *hdmi = video_drvdata(file);
	int ret = 0;

	switch(cmd) {
	case VIDIOC_SUBDEV_G_EDID:
	{
		/*
		 * This is a standard interface but not yet part of kernel
		 * release we are working on.
		 */
		struct v4l2_subdev_edid *edid_sd = arg;
		unsigned char *edid_src;
		unsigned int size, block = 1;

		/*
		 * We have one EDID block info, so, return an error if the
		 * start block is other than 0
		 */
		if ((edid_sd->start_block != 0) || (edid_sd->blocks > 256)) {
			ret = -EINVAL;
			break;
		}
		
		size = EDID_MAX_SIZE * block;
		edid_src = &hdmi->edid_info.base_raw[0] +
				 (edid_sd->start_block * EDID_MAX_SIZE);

		if(hdmi->edid_info.display_type != STM_DISPLAY_INVALID) {
			edid_sd->blocks = block;
			ret = copy_to_user(edid_sd->edid, edid_src, size);
		} else {
			ret = -ENODATA;
		}

		break;
	}

	default:
		HDMI_ERROR("Invalid IO control requested\n");
		ret = -EINVAL;
	}

	return ret;
}

struct v4l2_ioctl_ops stm_v4l2_hdmi_ioctl_ops = {
	.vidioc_querycap = stm_v4l2_hdmi_querycap,
	.vidioc_g_ext_ctrls = stm_v4l2_hdmi_g_ext_ctrls,
	.vidioc_s_ext_ctrls = stm_v4l2_hdmi_s_ext_ctrls,
	.vidioc_subscribe_event = stm_v4l2_hdmi_subscribe_event,
	.vidioc_unsubscribe_event = v4l2_event_unsubscribe,
	.vidioc_default = stm_v4l2_hdmi_default,
	
};

/**
 * stm_v4l2_hdmi_poll
 * Wait for the events and notify the user space
 */
static unsigned int stm_v4l2_hdmi_poll(struct file *file, 
					struct poll_table_struct *poll_table)
{
	struct v4l2_fh *fh = file->private_data;

	poll_wait(file, &fh->wait, poll_table);
	if (v4l2_event_pending(fh))
		return POLLIN;
	else
		return 0;
}

/**
 * stmhdmi_vsync_cb
 * This is the VSYNC callback
 */
static void stmhdmi_vsync_cb(stm_vsync_context_handle_t context,
						uint32_t timingevent)
{
	stm_display_output_connection_status_t status;
	struct stm_hdmi *hdmi = (struct stm_hdmi *)context;

	stm_display_output_get_connection_status(hdmi->hdmi_output, &status);

	if(hdmi->hotplug_gpio != STM_GPIO_INVALID) {
		unsigned hotplugstate = gpio_get_value(hdmi->hotplug_gpio);
		if(status == STM_DISPLAY_DISCONNECTED) {
			/*
			 * If the device has just been plugged in,
			 * flag that we now need to start the output.
			 */
			if(hotplugstate != 0) {
				status = STM_DISPLAY_NEEDS_RESTART;
				stm_display_output_set_connection_status(hdmi->
							hdmi_output, status);
			}
		} else {
			/*
			 * We may either be waiting for the output to be
			 * started, or already started, so only change the
			 * state if the device has now been disconnected.
			 */
			if(hotplugstate == 0) {
				status = STM_DISPLAY_DISCONNECTED;
				stm_display_output_set_connection_status(hdmi->
							hdmi_output, status);
			}
		}
	}

	if(status != hdmi->status) {
		hdmi->status = status;
		hdmi->status_changed = 1;
		wake_up(&hdmi->status_wait_queue);
	}
}

/**
 * stmhdmi_create_spd_metadata
 * Allocate memory for SPD metadata and infoframe.
 * Fill in the SPD info which can later be transmitted to sink
 */
static int __init stmhdmi_create_spd_metadata(struct stm_hdmi *hdmi)
{
	hdmi->spd_metadata = kzalloc(sizeof(stm_display_metadata_t) + 
				sizeof(stm_hdmi_info_frame_t), GFP_KERNEL);
	if (!hdmi->spd_metadata) {
		HDMI_ERROR("Unable to allocate memory for SPD metadata\n");
		return -ENOMEM;
	}

	hdmi->spd_metadata->size = sizeof(stm_display_metadata_t) + 
					sizeof(stm_hdmi_info_frame_t);
	hdmi->spd_metadata->type = STM_METADATA_TYPE_SPD_IFRAME;

	hdmi->spd_frame = (stm_hdmi_info_frame_t*)&hdmi->spd_metadata->data[0];
	hdmi->spd_frame->type = HDMI_SPD_INFOFRAME_TYPE;
	hdmi->spd_frame->version = HDMI_SPD_INFOFRAME_VERSION;
	hdmi->spd_frame->length = HDMI_SPD_INFOFRAME_LENGTH;
	strncpy(&hdmi->spd_frame->data[HDMI_SPD_INFOFRAME_VENDOR_START],
				"STM", HDMI_SPD_INFOFRAME_VENDOR_LENGTH);
	strncpy(&hdmi->spd_frame->data[HDMI_SPD_INFOFRAME_PRODUCT_START],
				"STLinux", HDMI_SPD_INFOFRAME_PRODUCT_LENGTH);
	hdmi->spd_frame->data[HDMI_SPD_INFOFRAME_SPI_OFFSET] =
						HDMI_SPD_INFOFRAME_SPI_PC;

	return 0;
}

/**
 * stmhdmi_destroy_spd_metadata
 * This function resets the SPD metadata and de-allocates
 * memory allocated during the SPD generation
 */
static void __exit stmhdmi_destroy_spd_metadata(struct stm_hdmi *hdmi)
{
	hdmi->spd_frame = NULL;
	kfree(hdmi->spd_metadata);
	hdmi->spd_metadata = NULL;
}

/**
 * stmhdmi_probe
 * This is the HDMI driver probe function which looks
 * for the HDMI platform device
 */
static int stmhdmi_probe(struct platform_device *pdev)
{
	struct stm_hdmi_platform_data *hdmi_pdev;
	struct stmcore_display_pipeline_data display_pipeline;
	stm_display_link_type_t link_type;
	stm_display_link_capability_t link_caps;
	struct stm_pad_state *hpd_pad = NULL;
	unsigned long hpd_gpio = STM_GPIO_INVALID;
	struct stm_hdmi *hdmi;
	char *paramstring = hdmi0;
	int ret = 0;
	struct sched_param param;

	if (pdev->id != 0) {
		HDMI_ERROR("Only one HDMI-Tx output device supported\n");
		ret = -EINVAL;
		goto hdmi_probe_invalid_error;
	}

	/*
	 * Get the HDMI device platform data
	 */
	hdmi_pdev = (struct stm_hdmi_platform_data *)pdev->dev.platform_data;
	if (!hdmi_pdev) {
		HDMI_ERROR("No HDMI platform device present\n");
		ret = -EINVAL;
		goto hdmi_probe_invalid_error;
	}

	/*
	 * Get the HDMI display pipeline
	 */
	ret = stmcore_get_display_pipeline(hdmi_pdev->pipeline_id,
							&display_pipeline);
	if (ret) {
		HDMI_ERROR("HDMI display pipeline (%u) not available\n", 
						hdmi_pdev->pipeline_id);
		goto hdmi_probe_invalid_error;
	}

	/*
	 * Is HDMI GPIO based?
	 */
	if (hdmi_pdev->hpd_pad_config.gpios_num > 0) {
		hpd_pad = stm_pad_claim(&hdmi_pdev->hpd_pad_config,
							"HDMI Hotplug");
		if (!hpd_pad) {
			HDMI_ERROR("HDMI device cannot claim any HPD pads,"
						" hotplug may not work\n");
		} else if (hdmi_pdev->hpd_poll_pio) {
			hpd_gpio = stm_pad_gpio_request_input(hpd_pad, "HPD");
		}
	}

	/*
	 * Allocate the memory for hdmi context and initialize it.
	 */
	hdmi = kzalloc(sizeof(struct stm_hdmi), GFP_KERNEL);
	if (!hdmi) {
		HDMI_ERROR("Unable to allocate memory for HDMI state struct\n");
                goto hdmi_probe_alloc_failed;
	}
	mutex_init(&hdmi->lock);
	spin_lock_init(&hdmi->spinlock);
	init_waitqueue_head(&hdmi->status_wait_queue);
	hdmi->pipeline_id = hdmi_pdev->pipeline_id;
	hdmi->hotplug_pad = hpd_pad;
	hdmi->hotplug_gpio = hpd_gpio;
	hdmi->video_type = (STM_VIDEO_OUT_RGB | STM_VIDEO_OUT_24BIT);
	hdmi->stop_output = 0;
	hdmi->max_pixel_repeat = 1;
	/*
	 * Set the default CEA selection behaviour to use
	 * the aspect ratio in the EDID.
	 */
	hdmi->cea_mode_from_edid = 1;

	/*
	 * Open the output device with the corresponding device ID.
	 */
	ret = stm_display_open_device(hdmi_pdev->device_id, &(hdmi->device));
	if (ret) {
		HDMI_ERROR("Unable to open display device %d\n",
							hdmi_pdev->device_id);
		goto hdmi_probe_hdmi_device_open_failed;
	}

	/*
	 * Check if the device is disabled
	 */
	if (paramstring) {
		if ((paramstring[0] == 'd') || (paramstring[0] == 'D')) {
			HDMI_ERROR("hdmi0 is initially disabled, "
				"use 'stfbset -e hdmi' to enable it\n");
			hdmi->disable = 1;
		}
	}

	/*
	 * Note that we are trusting the output identifiers are valid
	 * and pointing to correct output types.
	 */
	/*
	 * Open the MAIN output display device
	 */
	ret = stm_display_device_open_output(hdmi->device,
			display_pipeline.main_output_id, &(hdmi->main_output));
	if (ret) {
		HDMI_ERROR("Can't open main output device(ret :%d)\n", ret);
		goto hdmi_probe_main_output_open_failed;
	}
	/*
	 * Open the HDMI output display device
	 */
	ret = stm_display_device_open_output(hdmi->device,
			display_pipeline.hdmi_output_id, &(hdmi->hdmi_output));
	if (ret) {
		HDMI_ERROR("Can't open hdmi output device(ret: %d)\n", ret);
		goto hdmi_probe_hdmi_output_open_failed;
	}
	hdmi->hdmi_output_id = display_pipeline.hdmi_output_id;

	/*
	 * Get the output caps of HDMI output device
	 */
	ret = stm_display_output_get_capabilities(hdmi->hdmi_output,
						&hdmi->capabilities);
	if (ret) {
		HDMI_ERROR("Can't fetch HDMI caps(ret: %d)\n", ret);
		goto hdmi_probe_get_caps_failed;
	}
	if (!(hdmi->capabilities & OUTPUT_CAPS_HDMI))
	{
		HDMI_ERROR("HDMI output device doesn't support HDMI\n");
		goto hdmi_probe_get_caps_failed;
	}

	/*
	 * Allocate memory for SPD metadata and fill in the required info
	 */
	ret = stmhdmi_create_spd_metadata(hdmi);
	if (ret) {
		HDMI_ERROR("Unable to generate SPD metadata\n");
		goto hdmi_probe_get_caps_failed;
	}

	/*
	 * If we split the HDMI management into another module then we should change
	 * the owner field in the callback info to THIS_MODULE. However this is
	 * linked into the coredisplay module at the moment we do not want to have
	 * another reference to ourselves.
	 */
	INIT_LIST_HEAD(&(hdmi->vsync_cb_info.node));
	hdmi->vsync_cb_info.owner = NULL;
	hdmi->vsync_cb_info.context = hdmi;
	hdmi->vsync_cb_info.cb = stmhdmi_vsync_cb;
	ret = stmcore_register_vsync_callback(display_pipeline.display_runtime,
							&hdmi->vsync_cb_info);
	if (ret) { 
		HDMI_ERROR("Can't register hdmi-vsync callback(ret: %d)\n",
									 ret);
		goto hdmi_probe_vsync_cb_register_failed;
	}

	/*
	 * Open display link connection
	 */
	ret = stm_display_link_open (hdmi->pipeline_id, &hdmi->link);
	if (ret) {
		HDMI_ERROR("Can't Open display link with id(%d) (ret: %d)\n",
							hdmi->pipeline_id, ret);
		goto hdmi_probe_display_link_open_failed;
	}

	/*
	 * Get display link type
	 */
	ret = stm_display_link_get_type(hdmi->link, &link_type);
	if (ret) {
		HDMI_ERROR("Can't get the link type (ret: %d)\n", ret);
		goto hdmi_probe_get_link_type_failed;
	}
	if (link_type != STM_DISPLAY_LINK_TYPE_HDMI) {
		HDMI_ERROR("Link type is not HDMI\n");
		goto hdmi_probe_get_link_type_failed;
	}

	/*
	 * Check display link capability
	 */
	ret = stm_display_link_get_capability (hdmi->link, &link_caps);
	if (ret) {
		HDMI_ERROR("Can't get the link caps(ret: %d)\n", ret);
		goto hdmi_probe_get_link_type_failed;
	}

	/*
	 * If display link supports RxSense, then enable it
	 */
	if (link_caps.rxsense) {
		ret = stm_display_link_set_ctrl(hdmi->link,
				 STM_DISPLAY_LINK_CTRL_RXSENSE_ENABLE, 1);
		if (ret) {
			HDMI_ERROR("Failed to enable RxSense(ret: %d)\n", ret);
			goto hdmi_probe_get_link_type_failed;
		}
	}
	hdmi->link_capability = link_caps;

	/*
	 * Start HDMI manager
	 */
	hdmi->thread = kthread_run(stmhdmi_manager, hdmi, "STL-Hdmid/%d", pdev->id);
	if (!hdmi->thread) {
		HDMI_ERROR("Failed to start HDMI manager\n");
		goto hdmi_probe_start_hdmi_manager_failed;
	}

	if (thread_stl_hdmid[1]) {
		param.sched_priority = thread_stl_hdmid[1];
		if (sched_setscheduler(hdmi->thread, thread_stl_hdmid[0], &param)) {
			printk(KERN_ERR "FAILED to set scheduling parameters to \
			priority %d Mode :(%d)\n", \
			thread_stl_hdmid[1], thread_stl_hdmid[0]);
		}
	}

	/*
	 * Register hdmi sysfs attributes
	 */
	hdmi->class_device = &stm_v4l2_hdmi_device.vdev.dev;
	ret = stmhdmi_register_sysfs_attributes(hdmi->class_device);
	if (ret) {
		HDMI_ERROR("Cannot create a HDMI sysfs attributes\n");
		goto hdmi_probe_sysfs_attrs_failed;
	}

	/*
	 * The event may or or may not be generated which is required
	 * to restart the HDMI display. Indicating from here
	 * that the status has changed.
	 */
	hdmi->status_changed = 1;

	platform_set_drvdata(pdev, hdmi);
	stm_v4l2_hdmi_device.hdmi = hdmi;

	return 0;

hdmi_probe_sysfs_attrs_failed:
	kthread_stop(hdmi->thread);
hdmi_probe_start_hdmi_manager_failed:
	if (link_caps.rxsense)
		stm_display_link_set_ctrl(hdmi->link,
				STM_DISPLAY_LINK_CTRL_RXSENSE_ENABLE, 0);
hdmi_probe_get_link_type_failed:
	stm_display_link_close(hdmi->link);	
hdmi_probe_display_link_open_failed:
	stmcore_unregister_vsync_callback(display_pipeline.display_runtime,
                                                        &hdmi->vsync_cb_info);
hdmi_probe_vsync_cb_register_failed:
	stmhdmi_destroy_spd_metadata(hdmi);
hdmi_probe_get_caps_failed:
	stm_display_output_close(hdmi->hdmi_output);
hdmi_probe_hdmi_output_open_failed:
	stm_display_output_close(hdmi->main_output);
hdmi_probe_main_output_open_failed:
	stm_display_device_close(hdmi->device);
hdmi_probe_hdmi_device_open_failed:
	kfree(hdmi);
hdmi_probe_alloc_failed:
	if (hpd_pad)
		stm_pad_release(hpd_pad);
hdmi_probe_invalid_error:
	return ret;
}

/**
 * stmhdmi_remove
 * Remove the stmhdmi driver from the system. This is responsible for
 * stopping HDMI manager (kernel thread) and initiate device unregsistration
 * and its attributes removal
 */
static int __exit stmhdmi_remove(struct platform_device *pdev)
{
	struct stm_hdmi *hdmi = (struct stm_hdmi *)platform_get_drvdata(pdev);
	struct stmcore_display_pipeline_data display_pipe;

	HDMI_DEBUG("Exiting HDMI module\n");

	if(!hdmi) {
		HDMI_DEBUG("No device context present, nothing to do\n");
		return 0;
	}

	if (hdmi->thread)
		kthread_stop(hdmi->thread);
		
	stmhdmi_unregister_sysfs_attributes(hdmi->class_device);

	(void)stmcore_get_display_pipeline(hdmi->pipeline_id, &display_pipe);

	if (hdmi->link_capability.rxsense)
		stm_display_link_set_ctrl(hdmi->link,
				STM_DISPLAY_LINK_CTRL_RXSENSE_ENABLE, 0);


	stmcore_unregister_vsync_callback(display_pipe.display_runtime,
			&hdmi->vsync_cb_info);

	stmhdmi_destroy_spd_metadata(hdmi);

	stm_display_output_close(hdmi->hdmi_output);

	stm_display_output_close(hdmi->main_output);

	stm_display_link_close(hdmi->link);

	stm_display_device_close(hdmi->device);

	if (hdmi->hotplug_pad) {
		if (hdmi->hotplug_gpio != STM_GPIO_INVALID)
			stm_pad_gpio_free(hdmi->hotplug_pad, hdmi->hotplug_gpio);
		stm_pad_release(hdmi->hotplug_pad);
	}

	kfree(hdmi);
	hdmi = NULL;

	platform_set_drvdata(pdev,NULL);

	return 0;
}

static void stmhdmi_shutdown(struct platform_device *pdev)
{
	return;
}

/**
 * stm_v4l2_hdmi_vdev_release
 * This is the video device release function which is called when the
 * video device is destroyed. This is needed by the V4L2 on registration
 * At present, there's nothing to do and is here for successful init.
 */
static void stm_v4l2_hdmi_vdev_release(struct video_device *vdev)
{
	return;
}

/*
 * The V4L2 operations for the ST HDMI video device.
 */
static struct v4l2_file_operations stm_v4l2_hdmi_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = video_ioctl2,
	.open = v4l2_fh_open,
	.release = v4l2_fh_release,
	.poll = stm_v4l2_hdmi_poll,
};

/*
 * Definition of the platform driver struct for ST HDMI device
 */
static struct platform_driver stm_hdmi_driver = {
	.probe    = stmhdmi_probe,
	.shutdown = stmhdmi_shutdown,
	.remove   = __exit_p(stmhdmi_remove),
	.driver   = {
		.name     = "hdmi",
		.owner    = THIS_MODULE
	}
};

/**
 * stmhdmi_init
 * The module initialization function. This registers the ST HDMI
 * platform driver for HDMI platform device and exposes its inteface
 * using V4L2 video device.
 */
static int __init stm_hdmi_init(void)
{
	struct video_device *vdev = &stm_v4l2_hdmi_device.vdev;
	struct media_pad *pads;
	int ret = 0;

	/*
	 * Initialize the media_entity
	 */
	pads = (struct media_pad *)kzalloc(sizeof(*pads), GFP_KERNEL);
	if (!pads) {
		printk(KERN_ERR "Out of memory for hdmi pads\n");
		ret = -ENOMEM;
		goto pad_alloc_failed;
	}
	stm_v4l2_hdmi_device.pads = pads;

	ret = media_entity_init(&vdev->entity, 1, pads, 0);
	if (ret) {
		printk(KERN_ERR "hdmi entity init failed\n");
		goto entity_init_failed;
	}

	/*
	 * Register as a video device (/dev/videoX)
	 */
	mutex_init(&stm_v4l2_hdmi_device.mutex);
	vdev->fops = &stm_v4l2_hdmi_fops;
	vdev->ioctl_ops = &stm_v4l2_hdmi_ioctl_ops;
	strlcpy(vdev->name, "HDMI Tx Device", sizeof(vdev->name));
	vdev->minor = -1;
	vdev->release = stm_v4l2_hdmi_vdev_release;
	vdev->lock = &stm_v4l2_hdmi_device.mutex;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 7, 0))
        vdev->vfl_dir = VFL_DIR_TX;
#endif
	ret = stm_media_register_v4l2_video_device(vdev, VFL_TYPE_GRABBER, -1);
	if (ret) {
		HDMI_ERROR("Unable to register video device\n");
		goto hdmi_init_failed;
	}

	platform_driver_register(&stm_hdmi_driver);

	video_set_drvdata(vdev, stm_v4l2_hdmi_device.hdmi);

	return 0;

hdmi_init_failed:
	media_entity_cleanup(&vdev->entity);
entity_init_failed:
	kfree(pads);
pad_alloc_failed:
	return ret;
}

/**
 * stmhdmi_exit
 * The module exit function. This unregisters the ST HDMI
 * video device and the platform driver for the same.
 */
static void __exit stm_hdmi_exit(void)
{
	platform_driver_unregister(&stm_hdmi_driver);

	stm_media_unregister_v4l2_video_device(&stm_v4l2_hdmi_device.vdev);

	media_entity_cleanup(&stm_v4l2_hdmi_device.vdev.entity);

	kfree(stm_v4l2_hdmi_device.pads);
}

module_init(stm_hdmi_init);
module_exit(stm_hdmi_exit);
MODULE_LICENSE("GPL");
