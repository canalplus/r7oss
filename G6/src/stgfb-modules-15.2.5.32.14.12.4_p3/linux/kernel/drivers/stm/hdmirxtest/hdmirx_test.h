/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/hdmirxtest/hdmirx_test.h
 * Copyright (c) 2005-2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef  HDMIRX_TEST_H
#define HDMIRX_TEST_H
/* Standard Includes ----------------------------------------------*/
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>

/* Local Includes -------------------------------------------------*/
#include <stm_hdmirx.h>
#include <hdmirx_drv.h>

#define HDMIRX_EVENTS 2
#define HDMI_TEST_PRINT(a,b...) printk("hdmirx_test: %s: "a, __FUNCTION__ ,##b)

typedef struct stm_hdmirx_connection_s
{
  bool is_connected;
  uint32_t port_id;
}stm_hdmirx_connection_t;

typedef struct stm_hdmirx_test_s
{
  stm_hdmirx_device_h dev;
  stm_hdmirx_route_h route_p[STHDMIRX_MAX_ROUTE];
  stm_hdmirx_port_h port_p[STHDMIRX_MAX_PORT];
  stm_hdmirx_connection_t connection_state[STHDMIRX_MAX_ROUTE];
  uint32_t port_id;
  uint32_t route_id;
  uint32_t number_of_ports;
  uint32_t number_of_routes;
} stm_hdmirx_test_t;

uint32_t hdmirx_hot_plug_test(int port_id);
uint32_t hdmirx_audio_enable_test(int port_id);
uint32_t hdmirx_get_property_test(int port_id);
uint32_t hdmirx_read_edid(int port_id);
uint32_t hdmirx_update_edid(int port_id);
uint32_t hdmirx_open_close_test(void);
uint32_t hdmirx_mode_detection_test(void);

uint32_t stmhdmirx_pixelstream_init(void);
uint32_t stmhdmirx_pixelstream_term(void);
uint32_t stmhdmirx_pixelstream_set_signal_property(stm_hdmirx_signal_property_t signal_property);
uint32_t stmhdmirx_pixelstream_set_video_property(stm_hdmirx_video_property_t video_property);
void stmhdmirx_pixelstream_capturing(bool signal_stable);


uint32_t hdmirx_mode_detection_test_remove(void);
uint32_t hdmirx_audio_enable_test_remove(void);
uint32_t hdmirx_get_property_test_remove(void);
#endif
