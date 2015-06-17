/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/hdmirxtest/audio_enable_test.c
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
	int32_t err,i;
	stm_hdmirx_port_source_plug_status_t plugin_status;
	stm_hdmirx_route_signal_status_t signal_status;
	stm_hdmirx_signal_property_t signal_property;
	stm_hdmirx_route_hdcp_status_t hdcp_status;
	stm_hdmirx_audio_property_t audio_property;
	bool audio_status;
	uint32_t port_id=stm_hdmirx_test.port_id;
	uint32_t route_id=stm_hdmirx_test.route_id;

	for (i = 0; i < number_of_events; ++i) {
		if (events[i].event.object == stm_hdmirx_test.port_p[port_id])
		{
			switch (events[i].event.event_id)
			{
				case STM_HDMIRX_PORT_SOURCE_PLUG_EVT:
				{
					stm_hdmirx_port_get_source_plug_status(stm_hdmirx_test.port_p[port_id], &plugin_status);
					if (plugin_status ==STM_HDMIRX_PORT_SOURCE_PLUG_STATUS_IN)
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
				}
				break;
				default:
					HDMI_TEST_PRINT("Other port event\n");
					break;
			}
		}
		if (stm_hdmirx_test.connection_state[route_id].is_connected && events[i].event.object == stm_hdmirx_test.route_p[route_id])
		{
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
						HDMI_TEST_PRINT("Pixe Clock :%d\n", signal_property.timing.pixel_clock_hz);
						HDMI_TEST_PRINT("*************************************\n");
						err = stm_hdmirx_route_set_audio_out_enable(stm_hdmirx_test.route_p[route_id], TRUE);
						err = stm_hdmirx_route_get_hdcp_status(stm_hdmirx_test.route_p[route_id], &hdcp_status);
						if (err != 0) {
							HDMI_TEST_PRINT("ERROR :0x%x\n", err);
							break;
						}
						HDMI_TEST_PRINT("HDCP_STATUS:0x%x\n",hdcp_status);
					}
					if (signal_status == STM_HDMIRX_ROUTE_SIGNAL_STATUS_NOT_PRESENT)
					{
						HDMI_TEST_PRINT("signal status :STM_HDMIRX_ROUTE_SIGNAL_STATUS_NOT_PRESENT\n ");
						err = stm_hdmirx_route_set_audio_out_enable(stm_hdmirx_test.route_p[route_id], FALSE);
						if (err != 0) {
							HDMI_TEST_PRINT("ERROR :0x%x\n", err);
							break;
						}
					}
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
					err = stm_hdmirx_route_get_audio_out_enable(stm_hdmirx_test.route_p[route_id], &audio_status);
					if (err != 0) {
						HDMI_TEST_PRINT("ERROR :0x%x\n", err);
						break;
					}
					HDMI_TEST_PRINT("Audio Status:0x%x\n", audio_status);
					err = stm_hdmirx_route_set_audio_out_enable(stm_hdmirx_test.route_p[route_id], !audio_status);
					if (err != 0) {
						HDMI_TEST_PRINT("ERROR :0x%x\n", err);
						break;
					}
					mdelay(500);
					err = stm_hdmirx_route_get_audio_out_enable(stm_hdmirx_test.route_p[route_id], &audio_status);
					if (err != 0) {
						HDMI_TEST_PRINT("ERROR :0x%x\n", err);
						break;
					}
					HDMI_TEST_PRINT("Audio Status:0x%x\n", audio_status);
					err = stm_hdmirx_route_set_audio_out_enable(stm_hdmirx_test.route_p[route_id], TRUE);
					if (err != 0) {
						HDMI_TEST_PRINT("ERROR :0x%x\n", err);
						break;
					}
					mdelay(500);
					err = stm_hdmirx_route_get_audio_out_enable(stm_hdmirx_test.route_p[route_id], &audio_status);
					if (err != 0) {
						HDMI_TEST_PRINT("ERROR :0x%x\n", err);
						break;
					}
					HDMI_TEST_PRINT("Audio Status:0x%x\n", audio_status);
					break;
				}
				default:
					break;
			}
		}
	}
}
uint32_t hdmirx_audio_enable_test(int port_id)
{
	int err, evt, route_id=0;
	stm_event_subscription_entry_t events_entry[HDMIRX_EVENTS];
	stm_hdmirx_port_source_plug_status_t plugin_status;

	err = stm_hdmirx_device_open(&stm_hdmirx_test.dev, 0);
	if (err != 0) {
		HDMI_TEST_PRINT("HdmiRx Device Open failed \n");
		return 0;
	}
	err = stm_hdmirx_port_open(stm_hdmirx_test.dev, port_id, &stm_hdmirx_test.port_p[port_id]);
	if (err != 0) {
		HDMI_TEST_PRINT("HdmiRx Port Open failed \n");
		stm_hdmirx_device_close(stm_hdmirx_test.dev);
		return 0;
	}
	stm_hdmirx_test.port_id=port_id;
	stm_hdmirx_test.route_id=route_id;
	err = stm_hdmirx_route_open(stm_hdmirx_test.dev, route_id, &stm_hdmirx_test.route_p[route_id]);
	if (err != 0) {
		HDMI_TEST_PRINT("HdmiRx Route Open failed \n");
		stm_hdmirx_port_close(stm_hdmirx_test.port_p[port_id]);
		stm_hdmirx_device_close(stm_hdmirx_test.dev);
		return 0;
	}

	events_entry[0].object = stm_hdmirx_test.port_p[port_id];
	events_entry[0].event_mask = STM_HDMIRX_PORT_SOURCE_PLUG_EVT;
	events_entry[0].cookie = NULL;
	events_entry[1].object = stm_hdmirx_test.route_p[route_id];
	events_entry[1].event_mask =
	    STM_HDMIRX_ROUTE_SIGNAL_STATUS_CHANGE_EVT |
			STM_HDMIRX_ROUTE_VIDEO_PROPERTY_CHANGE_EVT |
	    STM_HDMIRX_ROUTE_AUDIO_PROPERTY_CHANGE_EVT;
	events_entry[1].cookie = NULL;
	evt = stm_event_subscription_create(events_entry, HDMIRX_EVENTS, &hdmirx_subscription);

	if (evt != 0) {
		HDMI_TEST_PRINT("Subscription of event failed \n");
		stm_hdmirx_port_close(stm_hdmirx_test.port_p[port_id]);
		stm_hdmirx_device_close(stm_hdmirx_test.dev);
		return 0;
	} else
		HDMI_TEST_PRINT("Subscription of Event Ok\n");

	err = stm_event_set_handler(hdmirx_subscription, (stm_event_handler_t)
				    hdmirx_evt_handler);
	if (err) {
		HDMI_TEST_PRINT("set handler hdmirx failed");
	}

	stm_hdmirx_port_get_source_plug_status(stm_hdmirx_test.port_p[port_id], &plugin_status);
	if (plugin_status == STM_HDMIRX_PORT_SOURCE_PLUG_STATUS_IN)
	{
		HDMI_TEST_PRINT("port %d : Cable Connected PluginStatus:0x%x \n", port_id, plugin_status);
		err = stm_hdmirx_port_connect(stm_hdmirx_test.port_p[port_id], stm_hdmirx_test.route_p[route_id]);
		stm_hdmirx_test.connection_state[route_id].is_connected=true;
	}

	return 0;
}


uint32_t hdmirx_audio_enable_test_remove(void)
{
	int32_t err=0;
	uint32_t port_id=stm_hdmirx_test.port_id;
	uint32_t route_id=stm_hdmirx_test.route_id;

	err=stm_event_subscription_delete(hdmirx_subscription);
	if(stm_hdmirx_test.connection_state[route_id].is_connected)
		err|=stm_hdmirx_port_disconnect(stm_hdmirx_test.port_p[port_id]);
	err|=stm_hdmirx_route_close(stm_hdmirx_test.route_p[route_id]);
	err|=stm_hdmirx_port_close(stm_hdmirx_test.port_p[port_id]);
	err|=stm_hdmirx_device_close(stm_hdmirx_test.dev);

	return err;
}
