/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/hdmirxtest/hot_plug_test.c
 * Copyright (c) 2005-2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <hdmirx_test.h>

uint32_t hdmirx_hot_plug_test(int port_id)
{
	int32_t err, evt;
	stm_hdmirx_test_t stm_hdmirx_test;
	stm_event_subscription_h hdmirx_subscription;
	stm_event_subscription_entry_t events_entry[HDMIRX_EVENTS];
	stm_event_info_t hdmirx_events;
	uint32_t actual_count;
	stm_hdmirx_port_source_plug_status_t plugin_status;
	stm_hdmirx_port_hpd_state_t hpd_status;

	HDMI_TEST_PRINT("\n*******HOT PLUG TEST************\n");
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

	events_entry[0].object = stm_hdmirx_test.port_p[port_id];
	events_entry[0].event_mask = STM_HDMIRX_PORT_SOURCE_PLUG_EVT;
	events_entry[0].cookie = NULL;

	evt = stm_event_subscription_create(events_entry, HDMIRX_EVENTS, &hdmirx_subscription);
	if (evt != 0) {
		HDMI_TEST_PRINT("Subscription of event failed \n");
		stm_hdmirx_port_close(stm_hdmirx_test.port_p[port_id]);
		stm_hdmirx_device_close(stm_hdmirx_test.dev);
		return 0;
	} else
		HDMI_TEST_PRINT("Subscription of Event Ok\n");

	while (1) {
		evt = stm_event_wait(hdmirx_subscription, -1, 1, &actual_count, &hdmirx_events);
		if (evt == 0) {
			if (hdmirx_events.event.object == stm_hdmirx_test.port_p[port_id]) {
				if (hdmirx_events.event.event_id == STM_HDMIRX_PORT_SOURCE_PLUG_EVT) {
					stm_hdmirx_port_get_source_plug_status(stm_hdmirx_test.port_p[port_id], &plugin_status);
					if (plugin_status == STM_HDMIRX_PORT_SOURCE_PLUG_STATUS_IN)
					{
						HDMI_TEST_PRINT("Cable Connected  PluginStatus:0x%x \n", plugin_status);
						break;
					}
					if (plugin_status == STM_HDMIRX_PORT_SOURCE_PLUG_STATUS_OUT)
						HDMI_TEST_PRINT("Cable DisConnected  PluginStatus:0x%x \n", plugin_status);
				}
			}
		}
	}

	stm_hdmirx_port_get_hpd_signal(stm_hdmirx_test.port_p[port_id], &hpd_status);
	HDMI_TEST_PRINT("HPD STATUS:0x%x\n", hpd_status);
	stm_hdmirx_port_set_hpd_signal(stm_hdmirx_test.port_p[port_id], STM_HDMIRX_PORT_HPD_STATE_LOW);
	mdelay(5000);
	stm_hdmirx_port_get_hpd_signal(stm_hdmirx_test.port_p[port_id], &hpd_status);
	HDMI_TEST_PRINT("HPD STATUS :0x%x\n", hpd_status);
	stm_hdmirx_port_set_hpd_signal(stm_hdmirx_test.port_p[port_id], STM_HDMIRX_PORT_HPD_STATE_HIGH);
	mdelay(5000);
	stm_hdmirx_port_get_hpd_signal(stm_hdmirx_test.port_p[port_id], &hpd_status);
	HDMI_TEST_PRINT("HPD STATUS :0x%x\n", hpd_status);

	err=stm_event_subscription_delete(hdmirx_subscription);
  stm_hdmirx_port_close(stm_hdmirx_test.port_p[port_id]);
  stm_hdmirx_device_close(stm_hdmirx_test.dev);

	return 0;
}
