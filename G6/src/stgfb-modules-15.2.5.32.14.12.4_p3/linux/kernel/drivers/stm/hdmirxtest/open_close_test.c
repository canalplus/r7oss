/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/hdmirxtest/open_close_test.c
 * Copyright (c) 2005-2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <hdmirx_test.h>

static stm_hdmirx_test_t stm_hdmirx_test;

uint32_t hdmirx_open_close_test(void)
{
	int32_t err=0;
	stm_hdmirx_device_capability_t dev_capability;
	stm_hdmirx_port_capability_t port_capability;
	stm_hdmirx_route_capability_t route_capability;
	stm_hdmirx_port_source_plug_status_t plugin_status;
	uint32_t route_id,port_id;

	HDMI_TEST_PRINT("\n***********MODE DETECTION TEST************\n");
	err = stm_hdmirx_device_open(&stm_hdmirx_test.dev, 0);
	if (err != 0) {
		HDMI_TEST_PRINT("HdmiRx Device Open failed \n");
		return 0;
	}
	err = stm_hdmirx_device_get_capability(stm_hdmirx_test.dev, &dev_capability);
	if (err != 0)
		HDMI_TEST_PRINT("Error in getting Device Capability\n");
	stm_hdmirx_test.number_of_ports=dev_capability.number_of_ports;
	stm_hdmirx_test.number_of_routes=dev_capability.number_of_routes;
	HDMI_TEST_PRINT("***********Device Capability************\n");
	HDMI_TEST_PRINT("Number of Ports :%d \n", dev_capability.number_of_ports);
	HDMI_TEST_PRINT("Number of Routes :%d \n", dev_capability.number_of_routes);
	HDMI_TEST_PRINT("**********End of Device Capability*********\n");

	for (route_id=0;route_id<stm_hdmirx_test.number_of_routes;route_id++)
	{
		err = stm_hdmirx_route_open(stm_hdmirx_test.dev, route_id, &stm_hdmirx_test.route_p[route_id]);
		if (err != 0) {
			HDMI_TEST_PRINT("HdmiRx Route Open failed \n");
			return 0;
		}
		err = stm_hdmirx_route_get_capability(stm_hdmirx_test.route_p[route_id], &route_capability);
		HDMI_TEST_PRINT("*************Route %d Capability***************\n", route_id);
		HDMI_TEST_PRINT("video_3d_support :0x%x\n",
				 route_capability.video_3d_support);
		HDMI_TEST_PRINT("************End of Route Capability*************\n");
	}

	for (port_id=0;port_id<stm_hdmirx_test.number_of_ports;port_id++)
	{
		err = stm_hdmirx_port_open(stm_hdmirx_test.dev, port_id, &stm_hdmirx_test.port_p[port_id]);
		if (err != 0) {
			HDMI_TEST_PRINT("HdmiRx Port Open failed \n");
			return 0;
		}
		err = stm_hdmirx_port_get_capability(stm_hdmirx_test.port_p[port_id], &port_capability);
		if (err != 0)
			HDMI_TEST_PRINT("Error in getting Port Capability\n");

		HDMI_TEST_PRINT("*********Port %d Capability*************\n",port_id);
		HDMI_TEST_PRINT(" hpd_support :%d \n", port_capability.hpd_support);
		HDMI_TEST_PRINT(" ddc_interface_support :%d \n", port_capability.ddc2bi);
		HDMI_TEST_PRINT("internal_edid :%d\n", port_capability.internal_edid);
		HDMI_TEST_PRINT("*********End of Port Capability***********\n");
		stm_hdmirx_port_get_source_plug_status(stm_hdmirx_test.port_p[port_id], &plugin_status);
		if (plugin_status == STM_HDMIRX_PORT_SOURCE_PLUG_STATUS_IN)
		{
			HDMI_TEST_PRINT("port %d : Cable Connected PluginStatus:0x%x \n", port_id, plugin_status);
		}
		else
		{
			HDMI_TEST_PRINT("port %d : Cable DisConnected  PluginStatus:0x%x \n",port_id, plugin_status);
		}
	}


	mdelay(1000);
	for (route_id=0;route_id<stm_hdmirx_test.number_of_routes;route_id++)
	{
		stm_hdmirx_route_close(stm_hdmirx_test.route_p[route_id]);
	}
	for (port_id=0;port_id<stm_hdmirx_test.number_of_ports;port_id++)
	{
		stm_hdmirx_port_close(stm_hdmirx_test.port_p[port_id]);
	}
	stm_hdmirx_device_close(stm_hdmirx_test.dev);
	return 0;
}
