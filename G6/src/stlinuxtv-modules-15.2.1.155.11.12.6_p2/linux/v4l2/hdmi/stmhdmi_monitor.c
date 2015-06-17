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
 *************************************************************************/
#include <linux/freezer.h>
#include <linux/gpio.h>

#include <media/v4l2-event.h>

#include <stm_common.h>
#include <stm_event.h>
#include <event_subscriber.h>

#include <stmdisplay.h>
#include <stm_display_link.h>
#include <linux/stm/stmcoredisplay.h>
#include <linux/stm/stmcorehdmi.h>

#include <stmhdmi_v4l2_ctrls.h>
#include "stmhdmi_device.h"

/*
 * We are going to handle only 2 display events; HPD and RxSense
 */
#define DISPLAY_LINK_EVENTS	2

static void stmhdmi_configure_hdmi_formatting(struct stm_hdmi *hdmi)
{
	uint32_t video_type;
	uint32_t colour_depth;
	uint32_t quantization = 0;
	uint32_t support_deepcolour;

	/*
	 * We are connected to a CEA conformant HDMI device. In this case the spec
	 * says we must do HDMI; if the device does not support YCrCb then force
	 * RGB output.
	 */
	if(hdmi->edid_info.cea_capabilities & (STM_CEA_CAPS_YUV|STM_CEA_CAPS_422))
	{
		video_type = hdmi->video_type & (STM_VIDEO_OUT_RGB |
				STM_VIDEO_OUT_YUV |
				STM_VIDEO_OUT_422 |
				STM_VIDEO_OUT_444);
	}
	else
	{
		video_type = STM_VIDEO_OUT_RGB;
	}

	DPRINTK("Setting HDMI output format %s\n",(video_type&STM_VIDEO_OUT_RGB)?"RGB":"YUV");

	support_deepcolour = (hdmi->edid_info.hdmi_vsdb_flags & STM_HDMI_VSDB_DC_MASK)?1:0;

	if(stm_display_output_set_control(hdmi->hdmi_output, OUTPUT_CTRL_SINK_SUPPORTS_DEEPCOLOR, support_deepcolour) < 0)
	{
		DPRINTK("Unable to set sink deepcolor state on HDMI output, HW probably doesn't support it\n");
		support_deepcolour = 0;
	}

	/*
	 * Filter the requested colour depth based on the EDID and colour format,
	 * falling back to standard colour if there is any mismatch.
	 */
	if(!support_deepcolour               ||
			(video_type & STM_VIDEO_OUT_422)  ||
			((video_type & STM_VIDEO_OUT_444) && !(hdmi->edid_info.hdmi_vsdb_flags & STM_HDMI_VSDB_DC_Y444)))
	{
		DPRINTK("Deepcolour not supported in requested colour format\n");
		colour_depth = STM_VIDEO_OUT_24BIT;
	}
	else
	{
		switch(hdmi->video_type & STM_VIDEO_OUT_DEPTH_MASK)
		{
			case STM_VIDEO_OUT_30BIT:
				if(hdmi->edid_info.hdmi_vsdb_flags & STM_HDMI_VSDB_DC_30BIT)
					colour_depth = STM_VIDEO_OUT_30BIT;
				else
					colour_depth = STM_VIDEO_OUT_24BIT;
				break;
			case STM_VIDEO_OUT_36BIT:
				if(hdmi->edid_info.hdmi_vsdb_flags & STM_HDMI_VSDB_DC_36BIT)
					colour_depth = STM_VIDEO_OUT_36BIT;
				else
					colour_depth = STM_VIDEO_OUT_24BIT;
				break;
			case STM_VIDEO_OUT_48BIT:
				if(hdmi->edid_info.hdmi_vsdb_flags & STM_HDMI_VSDB_DC_48BIT)
					colour_depth = STM_VIDEO_OUT_48BIT;
				else
					colour_depth = STM_VIDEO_OUT_24BIT;
				break;
			case STM_VIDEO_OUT_24BIT:
			default:
				colour_depth = STM_VIDEO_OUT_24BIT;
				break;
		}
	}

	switch(colour_depth)
	{
		case STM_VIDEO_OUT_24BIT:
			DPRINTK("Setting 24bit colour\n");
			break;
		case STM_VIDEO_OUT_30BIT:
			DPRINTK("Setting 30bit colour\n");
			break;
		case STM_VIDEO_OUT_36BIT:
			DPRINTK("Setting 36bit colour\n");
			break;
		case STM_VIDEO_OUT_48BIT:
			DPRINTK("Setting 48bit colour\n");
			break;
	}

	stm_display_output_set_control(hdmi->hdmi_output, OUTPUT_CTRL_VIDEO_OUT_SELECT, (STM_VIDEO_OUT_HDMI | video_type | colour_depth));

	if(hdmi->edid_info.cea_vcdb_flags & STM_CEA_VCDB_QS_RGB_QUANT)
		quantization |= (uint32_t)STM_VCDB_QUANTIZATION_RGB;

	if(hdmi->edid_info.cea_vcdb_flags & STM_CEA_VCDB_QY_YCC_QUANT)
		quantization |= (uint32_t)STM_VCDB_QUANTIZATION_YCC;

	stm_display_output_set_control(hdmi->hdmi_output, OUTPUT_CTRL_VCDB_QUANTIZATION_SUPPORT, quantization);

	if(hdmi->cea_mode_from_edid)
	{
		uint32_t tmp = (hdmi->edid_info.tv_aspect == STM_WSS_ASPECT_RATIO_4_3)?STM_AVI_VIC_4_3:STM_AVI_VIC_16_9;
		stm_display_output_set_control(hdmi->hdmi_output, OUTPUT_CTRL_AVI_VIC_SELECT, tmp);
	}

	DPRINTK("Finished\n");
}


static int stmhdmi_determine_pixel_repetition(struct stm_hdmi    *hdmi,
		stm_display_mode_t *mode)

{
	int repeat = hdmi->max_pixel_repeat;
	uint32_t saved_flags = mode->mode_params.flags;
	stm_display_mode_t m;

	DPRINTK("\n");

	/*
	 * Find the maximum pixel repeat we can use for SD/ED modes; this is so we
	 * always have the maximum audio bandwidth available.
	 */
	while(repeat>1)
	{
		DPRINTK("Trying pixel repeat = %d\n",repeat);
		if(!stm_display_output_find_display_mode(hdmi->main_output,
					mode->mode_params.active_area_width*repeat,
					mode->mode_params.active_area_height,
					mode->mode_timing.lines_per_frame,
					mode->mode_timing.pixels_per_line*repeat,
					mode->mode_timing.pixel_clock_freq*repeat,
					mode->mode_params.scan_type,
					&m))
		{
			int n;
			DPRINTK("Found SoC timing mode for pixel repeated mode %d\n",m.mode_id);
			for(n=0;n<hdmi->edid_info.num_modes;n++)
			{
				const stm_display_mode_t *tmp = &hdmi->edid_info.display_modes[n];

				if(tmp->mode_params.hdmi_vic_codes[STM_AR_INDEX_4_3] == m.mode_params.hdmi_vic_codes[STM_AR_INDEX_4_3] &&
						tmp->mode_params.hdmi_vic_codes[STM_AR_INDEX_16_9] == m.mode_params.hdmi_vic_codes[STM_AR_INDEX_16_9])
				{
					DPRINTK("Matched pixel repeated mode %d with display EDID supported modes\n",tmp->mode_id);
					*mode = m;
					break;
				}
			}

			if(n<hdmi->edid_info.num_modes)
				break;
		}

		repeat /= 2;
	}

	switch(repeat)
	{
		case 4:
			mode->mode_params.flags = (saved_flags | STM_MODE_FLAGS_HDMI_PIXELREP_4X);
			break;
		case 2:
			mode->mode_params.flags = (saved_flags | STM_MODE_FLAGS_HDMI_PIXELREP_2X);
			break;
		default:
			if(mode->mode_params.output_standards & STM_OUTPUT_STD_SD_MASK)
			{
				repeat = 2;
				DPRINTK("No supported pixel repeated mode found for SD interlaced timing\n");
				DPRINTK("Trying to find 2x pixel repeat for non-strict EDID semantics\n");
				if(!stm_display_output_find_display_mode(hdmi->main_output,
							mode->mode_params.active_area_width*repeat,
							mode->mode_params.active_area_height,
							mode->mode_timing.lines_per_frame,
							mode->mode_timing.pixels_per_line*repeat,
							mode->mode_timing.pixel_clock_freq*repeat,
							mode->mode_params.scan_type,
							&m))
				{
					*mode = m;
					mode->mode_params.flags = (saved_flags | STM_MODE_FLAGS_HDMI_PIXELREP_2X);
				}
				else
				{
					DPRINTK("No really no pixel repeated mode supported by the hardware, errr that's odd\n");
					return -EINVAL;
				}
			}
			else
			{
				DPRINTK("Using no pixel repetition for ED/HD mode\n");
			}
			break;
	}

	return 0;
}


static int stmhdmi_output_start(struct stm_hdmi    *hdmi,
		stm_display_mode_t *mode)
{
	stm_display_mode_t current_hdmi_mode = { STM_TIMING_MODE_RESERVED };

	DPRINTK("\n");

	/*
	 * Find out if the HDMI output is still running, we may be handling a deferred
	 * disconnection hotplug event.
	 */
	if(stm_display_output_get_current_display_mode(hdmi->hdmi_output, &current_hdmi_mode)<0)
		return -EINVAL;

	if(hdmi->deferred_disconnect > 0)
	{
		/*
		 * You might think, hold on how can the mode have changed in this case,
		 * but if the EDID changed what pixel repeated modes are supported then
		 * that might happen. Therefore we return an error and the output will
		 * be disabled and the application notified that it needs to do something.
		 */
		if((current_hdmi_mode.mode_id == mode->mode_id) && (current_hdmi_mode.mode_params.output_standards == mode->mode_params.output_standards))
			return 0;
		else
			return -EINVAL;
	}

	/*
	 * If we got a request to restart the output, without a disconnect first,
	 * then really do a restart if the mode is different by stopping the
	 * output first. If the mode is exactly the same just call start which
	 * will reset the output's connection state to connected.
	 */
	if((current_hdmi_mode.mode_id != mode->mode_id) || (current_hdmi_mode.mode_params.output_standards != mode->mode_params.output_standards))
		stm_display_output_stop(hdmi->hdmi_output);

	DPRINTK("Starting Video Mode %ux%u%s\n",mode->mode_params.active_area_width,
			mode->mode_params.active_area_height,
			(mode->mode_params.scan_type == STM_INTERLACED_SCAN)?"i":"p");

	if(stm_display_output_start(hdmi->hdmi_output, mode)<0)
		return -EINVAL;

	return 0;
}


static int stmhdmi_enable_mode_by_timing_limits(struct stm_hdmi    *hdmi,
		stm_display_mode_t *mode)
{
	uint32_t hfreq,vfreq;

	DPRINTK("Falling back refresh range limits for DVI monitor\n");

	if(mode->mode_timing.pixel_clock_freq > (hdmi->edid_info.max_clock*1000000))
	{
		DPRINTK("Pixel clock (%uHz) out of range\n",mode->mode_timing.pixel_clock_freq);
		return -EINVAL;
	}

	/*
	 * Check H & V Refresh instead
	 */
	hfreq = mode->mode_timing.pixel_clock_freq / mode->mode_timing.pixels_per_line;
	vfreq = mode->mode_params.vertical_refresh_rate / 1000;

	if((vfreq < hdmi->edid_info.min_vfreq) || (vfreq > hdmi->edid_info.max_vfreq))
	{
		DPRINTK("Vertical refresh (%uHz) out of range\n",vfreq);
		return -EINVAL;
	}

	if((hfreq < (hdmi->edid_info.min_hfreq*1000)) || (hfreq > (hdmi->edid_info.max_hfreq*1000)))
	{
		DPRINTK("Horizontal refresh (%uKHz) out of range\n",hfreq/1000);
		return -EINVAL;
	}

	DPRINTK("Starting DVI Video Mode %ux%u hfreq = %uKHz vfreq = %uHz\n",mode->mode_params.active_area_width,
			mode->mode_params.active_area_height,
			hfreq/1000,
			vfreq);

	return stmhdmi_output_start(hdmi, mode);
}


static int stmhdmi_enable_link(struct stm_hdmi *hdmi)
{
	int n;
	stm_display_mode_t mode;

	if(hdmi->edid_info.display_type == STM_DISPLAY_INVALID)
	{
		DPRINTK("Invalid EDID: Not enabling link\n");
		return -ENODEV;
	}

	/*
	 * Get the master output's mode, this is what we want to set
	 */
	if(stm_display_output_get_current_display_mode(hdmi->main_output, &mode)<0)
		return -EINVAL;

	BUG_ON(mode.mode_id == STM_TIMING_MODE_RESERVED);

	if(mode.mode_id < STM_TIMING_MODE_CUSTOM)
	{
		/*
		 * If the master is in a known standard mode, replace the output standard
		 * mask with the original template version so we can see if it is a CEA861
		 * mode or not.
		 */
		stm_display_mode_t tmp;
		if(stm_display_output_get_display_mode(hdmi->main_output, mode.mode_id, &tmp)<0)
			return -EINVAL;

		mode.mode_params.output_standards = tmp.mode_params.output_standards;
	}
	/*
	 * Make sure pixel repetition mode flags are cleared
	 */
	mode.mode_params.flags &= ~(STM_MODE_FLAGS_HDMI_PIXELREP_2X | STM_MODE_FLAGS_HDMI_PIXELREP_4X);

	if(hdmi->edid_info.display_type == STM_DISPLAY_DVI)
	{
		/*
		 * We believe we now have a computer monitor or a TV with a DVI input not
		 * a HDMI input. So set the output format to DVI and force RGB.
		 */
		DPRINTK("Setting DVI/RGB output\n");
		stm_display_output_set_control(hdmi->hdmi_output, OUTPUT_CTRL_VIDEO_OUT_SELECT, (STM_VIDEO_OUT_DVI | STM_VIDEO_OUT_RGB | STM_VIDEO_OUT_24BIT));
		/*
		 * DVI devices do not support deepcolor.
		 */
		stm_display_output_set_control(hdmi->hdmi_output, OUTPUT_CTRL_SINK_SUPPORTS_DEEPCOLOR, 0);
	}
	else
	{
		if(stmhdmi_determine_pixel_repetition(hdmi,&mode)!=0)
			return -EINVAL;

		stmhdmi_configure_hdmi_formatting(hdmi);
	}


	/*
	 * Allow pure VESA and NTG5 standards as well as CEA861, anything else is
	 * not supported. For modes like VGA, which are defined for both CEA and VESA
	 * then we prefer CEA so we get complete AVI infoframe behaviour.
	 *
	 * Note: do this _after_ looking for pixel repeated modes as that requires
	 * and may modify the tvStandard.
	 */
	if((mode.mode_params.output_standards & STM_OUTPUT_STD_CEA861) == 0)
	{
		DPRINTK("Non CEA861 mode\n");
		if(mode.mode_params.output_standards & STM_OUTPUT_STD_VESA_MASK)
			mode.mode_params.output_standards = (mode.mode_params.output_standards & STM_OUTPUT_STD_VESA_MASK);
		else if(mode.mode_params.output_standards & STM_OUTPUT_STD_NTG5)
			mode.mode_params.output_standards = STM_OUTPUT_STD_NTG5;
		else
			return -EINVAL;
	}
	else
	{
		/*
		 * Clear any standard flags other than for HDMI
		 */
		mode.mode_params.output_standards = STM_OUTPUT_STD_CEA861;
		DPRINTK("Configuring CEA861 mode\n");
	}

	/*
	 * If we have been asked not to check against the EDID, just start
	 * the mode.
	 */
	if(hdmi->non_strict_edid_semantics)
	{
		DPRINTK("Non strict EDID semantics selected, starting mode\n");
		return stmhdmi_output_start(hdmi, &mode);
	}

	for(n=0;n<hdmi->edid_info.num_modes;n++)
	{
		const stm_display_mode_t *tmp = &hdmi->edid_info.display_modes[n];

		/*
		 * We match up the analogue display mode with the EDID mode by using the
		 * HDMI video codes.
		 */
		if(tmp->mode_params.hdmi_vic_codes[STM_AR_INDEX_4_3] == mode.mode_params.hdmi_vic_codes[STM_AR_INDEX_4_3] &&
				tmp->mode_params.hdmi_vic_codes[STM_AR_INDEX_16_9] == mode.mode_params.hdmi_vic_codes[STM_AR_INDEX_16_9])
		{
			return stmhdmi_output_start(hdmi, &mode);
		}
	}

	DPRINTK("Current video mode not reported as supported by display\n");

	/*
	 * We are still trying to only set modes indicated as valid in the EDID. So
	 * for DVI devices allow modes that are inside the timing limits.
	 */
	if(hdmi->edid_info.display_type == STM_DISPLAY_DVI)
		return stmhdmi_enable_mode_by_timing_limits(hdmi, &mode);

	return -EINVAL;
}


/*******************************************************************************
 * Kernel management thread state machine helper functions.
 */

static void stmhdmi_restart_display(struct stm_hdmi *hdmi)
{
	DPRINTK("Starting HDMI Output hdmi = %p\n",hdmi);

	/*
	 * We always re-read the EDID, as we might have received a hotplug
	 * event just because the user changed the TV setup and the EDID
	 * information has now changed.
	 */
	if(stmhdmi_read_edid(hdmi) != 0)
	{
		DPRINTK("EDID Read Error, setup safe EDID\n");
		stmhdmi_safe_edid(hdmi);
	}

	stmhdmi_dump_edid(hdmi);

	if(!hdmi->disable)
	{
		if(stmhdmi_enable_link(hdmi) != 0)
		{
			/*
			 * If we had an error starting the output, do a stop just in case it was
			 * already running. This might happen for instance if we got a restart
			 * due to a change in the EDID mode semantics and the current mode is
			 * no longer allowed on the display.
			 */
			stm_display_output_stop(hdmi->hdmi_output);

			hdmi->stop_output = true;

			if(hdmi->deferred_disconnect > 0)
			{
				/*
				 * We were waiting to see if we could ignore the HPD pulse,
				 * but it turns out the new EDID is not compatible with the
				 * existing display mode so we now disable the output and signal
				 * the hotplug attribute so applications can check the EDID change.
				 */
				DPRINTK("Doing deferred output disable due to incompatible EDID\n");
				hdmi->disable = true;
				sysfs_notify(&(hdmi->class_device->kobj), NULL, "hdmi_hotplug");
			}
		}
		else
		{
			hdmi->stop_output = false;
		}
	}
	else
	{
		/*
		 * We are not starting the display because we are disabled, so the sysfs
		 * hotplug notification that is normally done when we enter the connected
		 * state will not happen. Hence we signal it now so applications know
		 * to check the new EDID and to possibly re-enable the HDMI output.
		 */
		hdmi->stop_output = true;
		sysfs_notify(&(hdmi->class_device->kobj), NULL, "hdmi_hotplug");
	}

	hdmi->deferred_disconnect = 0;

}


static void stmhdmi_disconnect_display(struct stm_hdmi *hdmi)
{
	stmhdmi_invalidate_edid_info(hdmi);
	if(hdmi->hotplug_mode == STMHDMI_HPD_STOP_IF_NECESSARY)
	{
		DPRINTK("Deferring disconnect hdmi = %p\n",hdmi);
		/*
		 * Wait for 3x 1/2 second timeouts to see if HPD gets re-asserted again
		 * before really stopping the HDMI output. This is based on the time
		 * a Yamaha RX-V1800 keeps hotplug de-asserted when a single TV connected
		 * to it asserts its hotplug (approx 1s) plus some margin for us to respond.
		 *
		 * It is possible that AV receivers acting as repeaters will keep HPD
		 * de-asserted while they authenticate all downstream devices, which can
		 * take up to 4.2seconds (see HDCP specification) for the maximum repeater
		 * depth. Do we want to wait that long?
		 *
		 */
		hdmi->deferred_disconnect = 3;
	}
	else
	{
		DPRINTK("Stopping HDMI Output hdmi = %p\n",hdmi);
		stm_display_output_stop(hdmi->hdmi_output);
		if(hdmi->hotplug_mode == STMHDMI_HPD_DISABLE_OUTPUT)
		{
			DPRINTK("Disabling HDMI Output hdmi = %p\n",hdmi);
			hdmi->disable = true;
		}

		sysfs_notify(&(hdmi->class_device->kobj), NULL, "hdmi_hotplug");
	}
}


static void stmhdmi_connect_display(struct stm_hdmi *hdmi)
{
	if(!hdmi->disable)
	{
		DPRINTK("HDMI Output connected\n");
		/*
		 * Schedule the sending of the product identifier HDMI info frame.
		 */
		if(hdmi->spd_frame->data[HDMI_SPD_INFOFRAME_SPI_OFFSET] != HDMI_SPD_INFOFRAME_SPI_DISABLE)
			(void)stm_display_output_queue_metadata(hdmi->hdmi_output, hdmi->spd_metadata);
		/*
		 * Signal the hotplug sysfs attribute now the output is alive.
		 */
		sysfs_notify(&(hdmi->class_device->kobj), NULL, "hdmi_hotplug");
	}
	else
	{
		DPRINTK("HDMI Output disabled\n");
		stm_display_output_stop(hdmi->hdmi_output);
		hdmi->stop_output = true;
	}
}


static void stmhdmi_get_hdmi_status(struct stm_hdmi *hdmi)
{
	stm_display_output_connection_status_t hdmi_status;
	stm_display_output_get_connection_status(hdmi->hdmi_output, &hdmi_status);

	if(hdmi->hotplug_gpio != STM_GPIO_INVALID)
	{
		unsigned hotplugstate = gpio_get_value(hdmi->hotplug_gpio);
		if(hdmi_status == STM_DISPLAY_DISCONNECTED)
		{
			/*
			 * If the device has just been plugged in, flag that we now need to
			 * start the output.
			 */
			if(hotplugstate != 0)
			{
				hdmi_status = STM_DISPLAY_NEEDS_RESTART;
				stm_display_output_set_connection_status(hdmi->hdmi_output, hdmi_status);
			}
		}
		else
		{
			/*
			 * We may either be waiting for the output to be started, or already
			 * started, so only change the state if the device has now been
			 * disconnected.
			 */
			if(hotplugstate == 0)
			{
				hdmi_status = STM_DISPLAY_DISCONNECTED;
				stm_display_output_set_connection_status(hdmi->hdmi_output, hdmi_status);
			}
		}
	}

	/*
	 * If there's no change in HDMI status, do nothing,
	 */
	if(hdmi_status == hdmi->status)
		return;

	/*
	 * Status changed, update it and post the event
	 */
	hdmi->status = hdmi_status;
	if (hdmi->status)
		stm_v4l2_event_queue(V4L2_EVENT_STM_TX_HOTPLUG,
					V4L2_DV_STM_HPD_STATE_HIGH);
	else 
		stm_v4l2_event_queue(V4L2_EVENT_STM_TX_HOTPLUG,
					V4L2_DV_STM_HPD_STATE_LOW);
}

/**
 * stmhdmi_manager
 * This is responsible for managing the HDMI state machine. Hotplug and
 * RxSense events are handled by this thread.
 */
int stmhdmi_manager(void *data)
{
	stm_event_info_t evt_q[DISPLAY_LINK_EVENTS];
	stm_event_subscription_h evt_subs;
	stm_event_subscription_entry_t evt_subs_entry;
	unsigned int event_count;
	bool link_state = false;
	int i, ret = 0;
	unsigned long flags;
	struct stm_hdmi *hdmi = (struct stm_hdmi *)data;

	HDMI_INFO("Start HDMI Manager\n");
	HDMI_DEBUG("Starting HDMI Thread %p\n", hdmi);

	/*
	 * Check the initial states of HPD from DisplayLink
	 */
	ret = stm_display_link_hpd_get_state(hdmi->link, &hdmi->hpd_state);
	if (ret) {
		HDMI_ERROR("Cannot get HPD state\n");
		goto hdmi_get_hpd_state_failed;
	}

	/*
	 * Determine HDMI status once HPD status is retrieved
	 */
	stmhdmi_get_hdmi_status(hdmi);

	/*
	 * Check the initial state of RxSense from DisplayLink
	 */
	ret = stm_display_link_rxsense_get_state(hdmi->link,
						&hdmi->rxsense_state);
	if (ret) {
		HDMI_ERROR("Cannot get RxSense state\n");
		goto hdmi_get_hpd_state_failed;
	}

	/*
	 * Subscribe to HPD, RxSense events
	 */
	evt_subs_entry.object = (stm_object_h)hdmi->link;
	evt_subs_entry.event_mask = STM_DISPLAY_LINK_HPD_STATE_CHANGE_EVT |
				STM_DISPLAY_LINK_RXSENSE_STATE_CHANGE_EVT;
	evt_subs_entry.cookie = NULL;
	ret = stm_event_subscription_create(&evt_subs_entry, 1, &evt_subs);
	if (ret) {
		HDMI_ERROR("Cannot create event subscription\n");
		goto hdmi_get_hpd_state_failed;
	}

	/*
	 * Set the wait queue for event subscription
	 */
	ret = stm_event_set_wait_queue(evt_subs,
				 &hdmi->status_wait_queue, true);
	if (ret) {
		HDMI_ERROR("Error can't set wait queue event\n");
		goto hdmi_set_wait_queue_failed;
	}

	/*
	 * Mark the task as freezable
	 */
	set_freezable();

	/*
	 * Start HDMI state machine and process HDMI events
	 */
	while(1) {
		wait_event_freezable_timeout(hdmi->status_wait_queue,
			((stm_event_wait(evt_subs, 0 , 0, NULL, NULL)!= 0) ||
			(hdmi->status_changed != 0) || kthread_should_stop()),
			 HZ/2);

		mutex_lock(&(hdmi->lock));

		/*
		 * Module is going to unload, stop the display
		 */
		if(kthread_should_stop())
		{
			HDMI_INFO("HDMI Manager Exiting\n");
			HDMI_DEBUG("HDMI Thread terminating %p\n", hdmi);
			mutex_unlock(&(hdmi->lock));
			break;
		}

		/*
		 * Check for any available events
		 */
		ret = stm_event_wait(evt_subs, 0 , DISPLAY_LINK_EVENTS,
							&event_count, evt_q);
		if (ret == 0) {
			HDMI_DEBUG("HDMI/RxSense event received\n");
		}

		/*
		 * Handle a real HPD, RxSense event
		 */
		for (i=0; i<event_count; i++) {
			switch(evt_q[i].event.event_id) {
			case STM_DISPLAY_LINK_HPD_STATE_CHANGE_EVT:
			{
				ret = stm_display_link_hpd_get_state(hdmi->link,
							 &hdmi->hpd_state);
				if (ret) {
					HDMI_ERROR("Can't get HPD status\n");
					break;
				}

				stmhdmi_get_hdmi_status(hdmi);

				break;
			}

			case STM_DISPLAY_LINK_RXSENSE_STATE_CHANGE_EVT:
			{
				ret = stm_display_link_rxsense_get_state(hdmi->
						link, &hdmi->rxsense_state);
				if (ret)
					HDMI_ERROR("Can't get RxSense"
								" status\n");

				break;
			}

			default:
				HDMI_ERROR("Invalid HDMI state\n");
			}
		}

		spin_lock_irqsave(&(hdmi->spinlock), flags);
		/*
		 * Handle the 1/2 second timeout to re-send the SPD info frame
		 * and handle deferred disconnection after a HPD de-assert.
		 */
		if((likely(hdmi->status_changed == 0) && (event_count==0))) {
			spin_unlock_irqrestore(&(hdmi->spinlock), flags);
			if((hdmi->spd_frame->data[HDMI_SPD_INFOFRAME_SPI_OFFSET]
					!= HDMI_SPD_INFOFRAME_SPI_DISABLE) &&
					link_state && (!hdmi->stop_output)) {

				ret = stm_display_output_queue_metadata(hdmi->
					hdmi_output, hdmi->spd_metadata);
				if (ret)
					HDMI_ERROR("Unable to queue SPD\n");

			}
			mutex_unlock(&(hdmi->lock));
			continue;
		}
		hdmi->status_changed = 0;
		spin_unlock_irqrestore(&(hdmi->spinlock), flags);

		link_state = (hdmi->hpd_state == 
				STM_DISPLAY_LINK_HPD_STATE_HIGH) &&
				(!hdmi->link_capability.rxsense ||
				(hdmi->rxsense_state == 
					STM_DISPLAY_LINK_RXSENSE_STATE_ACTIVE));
		if (!link_state) {

			HDMI_DEBUG("Link Down\n");
			stm_display_output_stop(hdmi->hdmi_output);
			hdmi->stop_output = true;
			stmhdmi_disconnect_display(hdmi);

		} else {
			HDMI_DEBUG("Link Up\n");
			switch(hdmi->status) {
			case STM_DISPLAY_NEEDS_RESTART:
			{
				stmhdmi_restart_display(hdmi);
				break;
			}

			case STM_DISPLAY_CONNECTED:
			{
				stmhdmi_connect_display(hdmi);
				break;
			}

			default:
				HDMI_ERROR("Invalid hdmi state\n");
			}
		}

		mutex_unlock(&(hdmi->lock));
	}

	/*
	 * Stop the display when the thread is going to exit
	 */ 
	stm_display_output_stop(hdmi->hdmi_output);

	/*
	 * Error handling for failure cases and needed while exiting
	 * this thread during module unload
	 */
hdmi_set_wait_queue_failed:
	ret = stm_event_subscription_delete(evt_subs);
	if (ret)
		HDMI_ERROR("Failed to delete event subscription\n");
hdmi_get_hpd_state_failed:
	return ret;
}
