/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/hdmirxtest/hdmirx_test.c
 * Copyright (c) 2005-2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <hdmirx_test.h>

/******On fly parameters definition********************************************/
static bool hot_plug_test = 0 ;
module_param( hot_plug_test, bool, 0644);
MODULE_PARM_DESC(hot_plug_test, " Default=0 so option is not launched ");

static bool audio_enable_test = 0 ;
module_param( audio_enable_test, bool, 0644);
MODULE_PARM_DESC(audio_enable_test, " Default=0 so option is not launched ");

static bool mode_detection_test = 0 ;
module_param( mode_detection_test, bool, 0644);
MODULE_PARM_DESC(mode_detection_test, " Default=0 so option is not launched ");

static bool get_property_test = 0 ;
module_param( get_property_test, bool, 0644);
MODULE_PARM_DESC(get_property_test, " Default=0 so option is not launched ");

static bool open_close_test = 0 ;
module_param( open_close_test, bool, 0644);
MODULE_PARM_DESC(open_close_test, " Default=0 so option is not launched ");

static bool read_edid_test = 0 ;
module_param( read_edid_test, bool, 0644);
MODULE_PARM_DESC(read_edid_test, " Default=0 so option is not launched ");

static bool update_edid_test = 0 ;
module_param( update_edid_test, bool, 0644);
MODULE_PARM_DESC(update_edid_test, " Default=0 so option is not launched ");

static int hdmirx_port_num = HDMIRX_INPUT_PORT_1 ; //Port 0
module_param( hdmirx_port_num, int, 0644);
MODULE_PARM_DESC(hdmirx_port_num, " Default=0 so Port 0 selected ");

static int plane_id = 4 ; //MAIN_VID
module_param( plane_id, int, 0644);
MODULE_PARM_DESC(plane_id, " Default=4 for Main_VID plane ");

int Plane_ID;
/**************************C O D E*********************************************/
static int32_t __init stm_hdmirx_test_init(void)
{
  uint32_t error = 1;

  Plane_ID=plane_id;
  if (hot_plug_test) {
    HDMI_TEST_PRINT("\n*********** Loading HdmiRx Hot Plug test Module for port num: %d ************\n",hdmirx_port_num);
    error = hdmirx_hot_plug_test(hdmirx_port_num);
  } else if (audio_enable_test) {
    HDMI_TEST_PRINT("\n*********** Loading HdmiRx Audio Enable test Module for port num: %d ************\n",hdmirx_port_num);
    error = hdmirx_audio_enable_test(hdmirx_port_num);
  } else if (mode_detection_test) {
    HDMI_TEST_PRINT("\n*********** Loading HdmiRx Auto Mode Detection test Module ************\n");
    error = hdmirx_mode_detection_test();
  } else if (get_property_test) {
    HDMI_TEST_PRINT("\n*********** Loading HdmiRx Get Property test Module for port num: %d ************\n",hdmirx_port_num);
    error = hdmirx_get_property_test(hdmirx_port_num);
  } else if (open_close_test) {
    HDMI_TEST_PRINT("\n*********** Loading HdmiRx Open Close test Module for port num: %d ************\n",hdmirx_port_num);
    error = hdmirx_open_close_test();
  } else if (read_edid_test) {
    HDMI_TEST_PRINT("\n*********** Loading HdmiRx Read Edid test Module for port num: %d ************\n",hdmirx_port_num);
    error = hdmirx_read_edid(hdmirx_port_num);
  } else if (update_edid_test) {
    HDMI_TEST_PRINT("\n*********** Loading HdmiRx Update Edid test Module for port num: %d ************\n",hdmirx_port_num);
    error = hdmirx_update_edid(hdmirx_port_num);
  } else {
      HDMI_TEST_PRINT("No test is specified: Please define only one of the tests below as parameter for the test module \n");
      HDMI_TEST_PRINT("hot_plug_test \n");
      HDMI_TEST_PRINT("audio_enable_test \n");
      HDMI_TEST_PRINT("get_property_test \n");
      HDMI_TEST_PRINT("open_close_test \n");
      HDMI_TEST_PRINT("read_edid_test \n");
      HDMI_TEST_PRINT("update_edid_test \n");
      HDMI_TEST_PRINT("mode_detection_test \n");
      HDMI_TEST_PRINT("\nUse the extra parameter [hdmirx_port_num=#] to select the hdmi-rx port\n");
  }

  if (error)
    HDMI_TEST_PRINT("ERROR:0x%x\n",error);
  return -error;
}

static void __exit stm_hdmirx_test_term(void)
{
  int32_t err=0;
  if(mode_detection_test)
    err=hdmirx_mode_detection_test_remove();
  else if(audio_enable_test)
    err=hdmirx_audio_enable_test_remove();
  else if(get_property_test)
    err=hdmirx_get_property_test_remove();

  if(err)
    HDMI_TEST_PRINT(KERN_INFO "\nError 0x%x when Removing stmhdmirx test module\n", err);
  else
    HDMI_TEST_PRINT(KERN_INFO "\nstmhdmirx test module removed successfully\n");
}

module_init(stm_hdmirx_test_init);
module_exit(stm_hdmirx_test_term);

MODULE_DESCRIPTION("stm_hdmirx test file");
MODULE_AUTHOR("STMicroelectronics Limited");
MODULE_VERSION("0.1");
MODULE_LICENSE("GPL");
