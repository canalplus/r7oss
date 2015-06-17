/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/hdmirxtest/read_edid_test.c
 * Copyright (c) 2005-2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

/* Local Includes -------------------------------------------------*/
#include <hdmirx_test.h>

uint32_t hdmirx_read_edid(int port_id)
{
	int32_t err, count;
	stm_hdmirx_test_t stm_hdmirx_test;
	stm_hdmirx_port_edid_block_t edid_block;

	HDMI_TEST_PRINT("***********RAED EDID TEST************\n");
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
	stm_hdmirx_port_read_edid_block(stm_hdmirx_test.port_p[port_id], 0, &edid_block);
	HDMI_TEST_PRINT("\nBlock0");
	for (count = 0; count < 128; count++) {
		if(count%16==0)
			printk("\n%x0|", (count/16));
		printk(" %02X", edid_block[count]);
	}
	printk("\n");
	stm_hdmirx_port_read_edid_block(stm_hdmirx_test.port_p[port_id], 1, &edid_block);
	HDMI_TEST_PRINT("\nBlock1");
	for (count = 0; count < 128; count++) {
		if(count%16==0)
			printk("\n%x0|", ((count+128)/16));
		printk(" %02X", edid_block[count]);
	}
	printk("\n");

	stm_hdmirx_port_close(stm_hdmirx_test.port_p[port_id]);
	stm_hdmirx_device_close(stm_hdmirx_test.dev);

	HDMI_TEST_PRINT("***********RAED EDID TEST DONE************\n");
	return 0;
}
