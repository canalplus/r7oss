/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/hdmirxtest/mode_detection_test.c
 * Copyright (c) 2005-2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <hdmirx_test.h>

static stm_hdmirx_test_t stm_hdmirx_test;
static stm_event_subscription_h hdmirx_subscription;

static void hdmirx_evt_handler(int number_of_events, stm_event_info_t * events)
{
	int32_t err, i, j, port_id;
	bool no_port_connected;
	stm_hdmirx_information_t info;
	stm_hdmirx_port_source_plug_status_t plugin_status;
	stm_hdmirx_route_signal_status_t signal_status;
	stm_hdmirx_signal_property_t signal_property;
	stm_hdmirx_video_property_t video_property;
	stm_hdmirx_audio_property_t audio_property;
	stm_hdmirx_route_hdcp_status_t hdcp_status;
	uint32_t route_id=stm_hdmirx_test.route_id;

	for (i = 0; i < number_of_events; ++i) {
		no_port_connected=true;
		for (port_id=0;port_id<stm_hdmirx_test.number_of_ports;port_id++)
		{
			if (events[i].event.object == stm_hdmirx_test.port_p[port_id])
			{
				switch (events[i].event.event_id) {
				case STM_HDMIRX_PORT_SOURCE_PLUG_EVT:
					{
						stm_hdmirx_port_get_source_plug_status(stm_hdmirx_test.port_p[port_id], &plugin_status);
						if (plugin_status == STM_HDMIRX_PORT_SOURCE_PLUG_STATUS_IN)
						{
							HDMI_TEST_PRINT("Cable Connected  PluginStatus:0x%x \n", plugin_status);
							if (stm_hdmirx_test.connection_state[route_id].is_connected)
							{
								err = stm_hdmirx_port_disconnect(stm_hdmirx_test.port_p[stm_hdmirx_test.connection_state[route_id].port_id]);
								if(err == 0)
									HDMI_TEST_PRINT("Port %d is successfully disConnect from route %d \n", stm_hdmirx_test.connection_state[route_id].port_id, route_id);
								else
									HDMI_TEST_PRINT("unable to DisConnect port %d from route %d \n", stm_hdmirx_test.connection_state[route_id].port_id, route_id);
							}
							err = stm_hdmirx_port_connect(stm_hdmirx_test.port_p[port_id], stm_hdmirx_test.route_p[route_id]);
							if(err == 0)
							{
								HDMI_TEST_PRINT("Port %d is successfully disConnect from route %d \n", port_id, route_id);
								stm_hdmirx_test.connection_state[route_id].is_connected=true;
								stm_hdmirx_test.connection_state[route_id].port_id=port_id;
							}
						}
						else
						{
							HDMI_TEST_PRINT("Cable DisConnected  PluginStatus:0x%x \n", plugin_status);
							#ifdef STHDMIRX_PIXSTREAM
							stmhdmirx_pixelstream_capturing(FALSE);
							#endif
							err = stm_hdmirx_port_disconnect(stm_hdmirx_test.port_p[port_id]);
							if(err == 0)
							{
								stm_hdmirx_test.connection_state[route_id].is_connected=false;
								HDMI_TEST_PRINT("Port %d is successfully disConnect from route %d \n", port_id, route_id);
							}
							else
							{
								HDMI_TEST_PRINT("unable to DisConnect port %d from route %d \n", port_id, route_id);
							}
						}
						break;
					}
				default:
					HDMI_TEST_PRINT("Other port event\n");
					break;
				}
			}
		}

		for (j=0;j<stm_hdmirx_test.number_of_routes;j++)
		{
			if(stm_hdmirx_test.connection_state[j].is_connected) no_port_connected=false;
		}

		if (!no_port_connected && events[i].event.object == stm_hdmirx_test.route_p[route_id]) {
			switch (events[i].event.event_id) {
			case STM_HDMIRX_ROUTE_SIGNAL_STATUS_CHANGE_EVT:
				{
					stm_hdmirx_route_get_signal_status(stm_hdmirx_test.route_p[route_id], &signal_status);
					if (signal_status == STM_HDMIRX_ROUTE_SIGNAL_STATUS_PRESENT)
					{
						HDMI_TEST_PRINT("signal status :STM_HDMIRX_ROUTE_SIGNAL_STATUS_PRESENT\n ");
						err = stm_hdmirx_route_get_signal_property(stm_hdmirx_test.route_p[route_id], &signal_property);
						if (err != 0) {
							HDMI_TEST_PRINT("ERROR :0x%x\n", err);
							break;
						}
						HDMI_TEST_PRINT("***********Signal Property************\n");
						HDMI_TEST_PRINT("Signal Type :%d\n", signal_property.signal_type);
						HDMI_TEST_PRINT("HACTIVE :%d\n", signal_property.timing.hactive);
						HDMI_TEST_PRINT("HTOTAL :%d\n",  signal_property.timing.htotal);
						HDMI_TEST_PRINT("VACTIVE :%d\n", signal_property.timing.vActive);
						HDMI_TEST_PRINT("VTOTAL :%d\n",  signal_property.timing.vtotal);
						HDMI_TEST_PRINT("ScanType :%d\n", signal_property.timing.scan_type);
						HDMI_TEST_PRINT("Pixel Clock :%d\n", signal_property.timing.pixel_clock_hz);
						HDMI_PRINT("Hsync polarity :%d\n", signal_property.timing.hsync_polarity);
						HDMI_PRINT("Vsync polarity :%d\n", signal_property.timing.vsync_polarity);
						HDMI_PRINT("H active start :%d\n", signal_property.timing.hactive_start);
						HDMI_PRINT("V active start :%d\n", signal_property.timing.vactive_start);
						HDMI_TEST_PRINT("*************************************\n");
						#ifdef STHDMIRX_PIXSTREAM
						stmhdmirx_pixelstream_set_signal_property(signal_property);
						stmhdmirx_pixelstream_capturing(TRUE);
						#endif
						err = stm_hdmirx_route_set_audio_out_enable(stm_hdmirx_test.route_p[route_id], TRUE);
						err |= stm_hdmirx_route_get_hdcp_status(stm_hdmirx_test.route_p[route_id], &hdcp_status);
						if (err != 0) {
							HDMI_TEST_PRINT("ERROR :0x%x\n", err);
							break;
						}
						HDMI_TEST_PRINT("HDCP_STATUS:0x%x\n",hdcp_status);
					}

					if (signal_status == STM_HDMIRX_ROUTE_SIGNAL_STATUS_NOT_PRESENT)
					{
						HDMI_TEST_PRINT("signal status :STM_HDMIRX_ROUTE_SIGNAL_STATUS_NOT_PRESENT\n ");
						#ifdef STHDMIRX_PIXSTREAM
						stmhdmirx_pixelstream_capturing(FALSE);
						#endif
						err = stm_hdmirx_route_set_audio_out_enable(stm_hdmirx_test.route_p[route_id], FALSE);
						if (err != 0) {
							HDMI_TEST_PRINT("ERROR :0x%x\n", err);
							break;
						}
					}
					break;
				}
			case STM_HDMIRX_ROUTE_VIDEO_PROPERTY_CHANGE_EVT:
				{
					HDMI_TEST_PRINT("signal status :STM_HDMIRX_ROUTE_VIDEO_PROPERTY_CHANGE_EVT\n ");
					err = stm_hdmirx_route_get_video_property(stm_hdmirx_test.route_p[route_id], &video_property);
					if (err != 0) {
						HDMI_TEST_PRINT("ERROR :0x%x\n", err);
						break;
					}
					HDMI_TEST_PRINT("************VIdeo Property**************\n");
					HDMI_TEST_PRINT("Video3d :%d\n",video_property.video_3d);
					HDMI_TEST_PRINT("color_format :%d\n", video_property.color_format);
					HDMI_TEST_PRINT("content_type :%d\n", video_property.content_type);
					HDMI_TEST_PRINT("color_depth :%d\n",video_property.color_depth);
					HDMI_TEST_PRINT("active_format_aspect :%d\n", video_property.active_format_aspect);
					HDMI_TEST_PRINT("************************************\n");
					#ifdef STHDMIRX_PIXSTREAM
					stmhdmirx_pixelstream_set_video_property(video_property);
					#endif
					break;
				}
			case STM_HDMIRX_ROUTE_AUDIO_PROPERTY_CHANGE_EVT:
				{
					HDMI_TEST_PRINT("signal status :STM_HDMIRX_ROUTE_AUDIO_PROPERTY_CHANGE_EVT\n ");
					err = stm_hdmirx_route_get_audio_property(stm_hdmirx_test.route_p[route_id], &audio_property);
					if (err != 0) {
						HDMI_TEST_PRINT("ERROR :0x%x\n", err);
						break;
					}
					HDMI_TEST_PRINT("**********Audio Property************\n");
					HDMI_TEST_PRINT("channel_allocation :%d\n", audio_property.channel_allocation);
					HDMI_TEST_PRINT("coding_type :%d\n", audio_property.coding_type);
					HDMI_TEST_PRINT("sampling_frequency :%d\n", audio_property.sampling_frequency);
					HDMI_TEST_PRINT("stream_type :%d\n", audio_property.stream_type);
					HDMI_TEST_PRINT("channel_count :%d\n", audio_property.channel_count);
					HDMI_TEST_PRINT("************************************\n");
					break;
				}
			case STM_HDMIRX_ROUTE_VS_INFO_UPDATE_EVT:
				{
					HDMI_TEST_PRINT("Received event STM_HDMIRX_ROUTE_VS_INFO_UPDATE_EVT\n");
					err = stm_hdmirx_route_get_info(stm_hdmirx_test.route_p[route_id], STM_HDMIRX_INFO_VSINFO,(void *)&info);
					if (err != 0) {
						HDMI_TEST_PRINT("ERROR :0x%x\n", err);
						break;
					}
					break;
				}
			 default:
				HDMI_TEST_PRINT("Other route event\n");
			}
		}
	}
}

uint32_t hdmirx_mode_detection_test(void)
{
	int32_t err, evt, port_id, route_id=0;

	stm_hdmirx_device_capability_t dev_capability;
	stm_hdmirx_port_capability_t port_capability;
	stm_hdmirx_route_capability_t route_capability;
	stm_hdmirx_port_source_plug_status_t plugin_status;

	stm_event_subscription_entry_t events_entry[HDMIRX_EVENTS];

	HDMI_TEST_PRINT("\n***********MODE DETECTION TEST************\n");
	err = stm_hdmirx_device_open(&stm_hdmirx_test.dev, 0);
	if (err != 0) {
		HDMI_TEST_PRINT("HdmiRx Device Open failed \n");
		return 0;
	}
	err = stm_hdmirx_device_get_capability(stm_hdmirx_test.dev, &dev_capability);
	if (err != 0)
	{
		HDMI_TEST_PRINT("Error in getting Device Capability\n");
		return 0;
	}
	stm_hdmirx_test.number_of_ports=dev_capability.number_of_ports;
	stm_hdmirx_test.number_of_routes=dev_capability.number_of_routes;
	HDMI_TEST_PRINT("***********Device Capability************\n");
	HDMI_TEST_PRINT("Number of Ports :%d \n", dev_capability.number_of_ports);
	HDMI_TEST_PRINT("Number of Routes :%d \n", dev_capability.number_of_routes);
	HDMI_TEST_PRINT("**********End of Device Capability*********\n");

	stm_hdmirx_test.route_id=route_id;
	err = stm_hdmirx_route_open(stm_hdmirx_test.dev, route_id, &stm_hdmirx_test.route_p[route_id]);
	if (err != 0) {
		HDMI_TEST_PRINT("HdmiRx Route Open failed \n");
		stm_hdmirx_device_close(stm_hdmirx_test.dev);
		return 0;
	}

	events_entry[0].object = stm_hdmirx_test.route_p[route_id];
	events_entry[0].event_mask =
	    STM_HDMIRX_ROUTE_SIGNAL_STATUS_CHANGE_EVT |
	    STM_HDMIRX_ROUTE_VIDEO_PROPERTY_CHANGE_EVT |
	    STM_HDMIRX_ROUTE_AUDIO_PROPERTY_CHANGE_EVT |
	    STM_HDMIRX_ROUTE_VS_INFO_UPDATE_EVT;
	events_entry[0].cookie = NULL;

	for (port_id=0;port_id<stm_hdmirx_test.number_of_ports;port_id++)
	{
		err = stm_hdmirx_port_open(stm_hdmirx_test.dev, port_id, &stm_hdmirx_test.port_p[port_id]);
		if (err != 0) {
			HDMI_TEST_PRINT("HdmiRx Port Open failed \n");
			stm_hdmirx_route_close(stm_hdmirx_test.route_p[route_id]);
			stm_hdmirx_device_close(stm_hdmirx_test.dev);
			return 0;
		}
		stm_hdmirx_port_get_source_plug_status(stm_hdmirx_test.port_p[port_id], &plugin_status);
		if (plugin_status == STM_HDMIRX_PORT_SOURCE_PLUG_STATUS_IN)
		{
			HDMI_TEST_PRINT("port %d : Cable Connected PluginStatus:0x%x \n", port_id, plugin_status);
			err = stm_hdmirx_port_connect(stm_hdmirx_test.port_p[port_id], stm_hdmirx_test.route_p[route_id]);
			stm_hdmirx_test.connection_state[route_id].is_connected=true;
		}

		err = stm_hdmirx_port_get_capability(stm_hdmirx_test.port_p[port_id], &port_capability);
		if (err != 0)
			HDMI_TEST_PRINT("Error in getting Port Capability\n");

		HDMI_TEST_PRINT("*********Port %d Capability*************\n",port_id);
		HDMI_TEST_PRINT(" hpd_support :%d \n", port_capability.hpd_support);
		HDMI_TEST_PRINT(" ddc_interface_support :%d \n", port_capability.ddc2bi);
		HDMI_TEST_PRINT("internal_edid :%d\n", port_capability.internal_edid);
		HDMI_TEST_PRINT("*********End of Port Capability***********\n");

		events_entry[port_id+1].object = stm_hdmirx_test.port_p[port_id];
		events_entry[port_id+1].event_mask = STM_HDMIRX_PORT_SOURCE_PLUG_EVT;
		events_entry[port_id+1].cookie = NULL;
	}
#ifdef STHDMIRX_PIXSTREAM
	err = stmhdmirx_pixelstream_init();
	if (err != 0)
	{
		HDMI_TEST_PRINT("Error in getting Port Capability\n");
		return err;
	}
#endif
	evt = stm_event_subscription_create(events_entry, HDMIRX_EVENTS, &hdmirx_subscription);
	if (evt != 0) {
		HDMI_TEST_PRINT("Subscription of event failed \n");
		hdmirx_mode_detection_test_remove();
		return 0;
	} else
		HDMI_TEST_PRINT("Subscription of Event Ok\n");

	/* set the call back function for subscribed events */
	err = stm_event_set_handler(hdmirx_subscription, (stm_event_handler_t)
				    hdmirx_evt_handler);
	if (err) {
		HDMI_TEST_PRINT("set handler hdmirx failed");
	}

	err = stm_hdmirx_route_get_capability(stm_hdmirx_test.route_p[route_id], &route_capability);
	HDMI_TEST_PRINT("*************RouteCapability***************\n");
	HDMI_PRINT("video_3d_support : %s\n",route_capability.video_3d_support?"Yes":"No");
	HDMI_PRINT("Repeater Support functionality : %s\n",route_capability.repeater_fn_support?"Yes":"No");
	HDMI_TEST_PRINT("************End of Route Capability*************\n");

	return 0;
}


uint32_t hdmirx_mode_detection_test_remove(void)
{
	int32_t err=0,port_id;
	uint32_t route_id=stm_hdmirx_test.route_id;

	err=stm_event_subscription_delete(hdmirx_subscription);
	for (port_id=0;port_id<stm_hdmirx_test.number_of_ports;port_id++)
	{
		if(stm_hdmirx_test.connection_state[route_id].is_connected)
			err|=stm_hdmirx_port_disconnect(stm_hdmirx_test.port_p[port_id]);

		err|=stm_hdmirx_port_close(stm_hdmirx_test.port_p[port_id]);
	}
	err|=stm_hdmirx_route_close(stm_hdmirx_test.route_p[route_id]);
	err|=stm_hdmirx_device_close(stm_hdmirx_test.dev);
	#ifdef STHDMIRX_PIXSTREAM
	err|=stmhdmirx_pixelstream_term();
	#endif
	return err;
}
