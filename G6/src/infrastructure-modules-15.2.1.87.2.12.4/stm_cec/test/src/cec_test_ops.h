/************************************************************************
Copyright (C) 2011 STMicroelectronics. All Rights Reserved.

This file is part of the stm_fe Library.

stm_cec is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

stm_cec is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with stm_fe; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The stm_fe Library may alternatively be licensed under a proprietary
license from ST.

Source file name : cec_test_op.h
Author :           bharatj

API dedinitions for demodulation device

Date        Modification                                    Name
----        ------------                                    --------
05-Mar-12   Created                                         bharatj

************************************************************************/

#ifndef _CEC_TEST_OP_H_
#define _CEC_TEST_OP_H_


#define CEC_TEST_ADDR_INDEX		0
#define CEC_TEST_OPCODE_INDEX	1
#define CEC_TEST_OPERNAD_1_INDEX	2
#define CEC_TEST_OPERNAD_2_INDEX	3
#define CEC_TEST_OPERNAD_3_INDEX	4
#define CEC_TEST_OPERNAD_4_INDEX	5
#define CEC_TEST_OPERNAD_5_INDEX	6
#define CEC_TEST_OPERNAD_6_INDEX	7
#define CEC_TEST_OPERNAD_7_INDEX	8
#define CEC_TEST_OPERNAD_8_INDEX	9
#define CEC_TEST_OPERNAD_9_INDEX	10
#define CEC_TEST_OPERNAD_10_INDEX	11

enum cec_test_opcode_e
{
	CEC_TEST_OPCODE_FEATURE_ABORT                        = 0x00,
	CEC_TEST_OPCODE_ABORT_MESSAGE                        = 0xFF,
	CEC_TEST_OPCODE_GIVE_OSD_NAME                        = 0x46,
	CEC_TEST_OPCODE_SET_OSD_NAME                         = 0x47,
	CEC_TEST_OPCODE_STANDBY                              = 0x36,
	CEC_TEST_OPCODE_ACTIVE_SOURCE                        = 0x82,
	CEC_TEST_OPCODE_INACTIVE_SOURCE                      = 0x9D,
	CEC_TEST_OPCODE_REQUEST_ACTIVE_SOURCE                = 0x85,
	CEC_TEST_OPCODE_CEC_VERSION                          = 0x9E,
	CEC_TEST_OPCODE_GET_CEC_VERSION                      = 0x9F,
	CEC_TEST_OPCODE_GIVE_PHYSICAL_ADDRESS                = 0x83,
	CEC_TEST_OPCODE_REPORT_PHYSICAL_ADDRESS              = 0x84,
	CEC_TEST_OPCODE_DEVICE_VENDOR_ID                     = 0x87,
	CEC_TEST_OPCODE_GIVE_DEVICE_VENDOR_ID                = 0x8C,
	CEC_TEST_OPCODE_VENDOR_COMMAND                       = 0x89,
	CEC_TEST_OPCODE_VENDOR_COMMAND_WITH_ID               = 0xA0,
	CEC_TEST_OPCODE_ROUTING_CHANGE                       = 0x80,
	CEC_TEST_OPCODE_ROUTING_INFORMATION                  = 0x81,
	CEC_TEST_OPCODE_GIVE_DEVICE_POWER_STATUS             = 0x8F,
	CEC_TEST_OPCODE_REPORT_POWER_STATUS                  = 0x90,
	CEC_TEST_OPCODE_GET_MENU_LANGUAGE                    = 0x91,
	CEC_TEST_OPCODE_SET_MENU_LANGUAGE                    = 0x32,
	CEC_TEST_OPCODE_IMAGE_VIEW_ON                        = 0x04,
	CEC_TEST_OPCODE_SET_STREAM_PATH                      = 0x86,
	CEC_TEST_OPCODE_MENU_REQUEST                         = 0x8D,
	CEC_TEST_OPCODE_MENU_STATUS                          = 0x8E,
	CEC_TEST_OPCODE_POLLING_MESSAGE
};

enum cec_test_send_type_e{
	CEC_TEST_SEND_NOTHING=0xABCD,
	CEC_TEST_SEND_ABORT=0xFF,
	CEC_TEST_SEND_FEATURE_ABORT=0x00,
	CEC_TEST_GIVE_PHYSICAL_ADDRESS=0x83,
	CEC_TEST_SET_OSD_NAME=0x47
};

enum CEC_AbortReason_t
{
	CEC_ABORT_REASON_UNRECOGNIZED_OPCODE             = 0,
	CEC_ABORT_REASON_NOT_IN_CORRECT_MODE_TO_RESPOND  = 1,
	CEC_ABORT_REASON_CANNOT_PROVIDE_SOURCE           = 2,
	CEC_ABORT_REASON_INVALID_OPERAND                 = 3,
	CEC_ABORT_REASON_REFUSED                         = 4,
	CEC_ABORT_REASON_UNABLE_TO_DETERMINE             = 5
};

#endif
