/************************************************************************
Copyright (C) 2011 STMicroelectronics. All Rights Reserved.

This file is part of the stm_fe Library.

stm_fe is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

stm_fe is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with stm_fe; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The stm_fe Library may alternatively be licensed under a proprietary
license from ST.

Source file name : stm_cec.h
Author :           bharatj

API dedinitions for demodulation device

Date        Modification                                    Name
----        ------------                                    --------
05-Mar-12   Created                                         bharatj

************************************************************************/


#ifndef _STM_CEC_H
#define _STM_CEC_H

#include "stm_common.h"


#define STM_CEC_MAX_MSG_LEN		16


typedef enum
{
	STM_CEC_CTRL_FLAG_UPDATE_LOGICAL_ADDR = 1
} stm_cec_ctrl_flag_t;

typedef enum
{
	STM_CEC_EVENT_MSG_RECEIVED = (1<<0),
	STM_CEC_EVENT_MSG_RECEIVED_ERROR = (1<<1)
} stm_cec_event_t;

typedef struct stm_cec_s 	*stm_cec_h;

typedef union stm_cec_ctrl_type_u
{
	struct logic_addr_param_s {
		uint8_t		logical_addr;
		uint8_t		enable;
	}logic_addr_param;
}stm_cec_ctrl_type_t;


typedef struct stm_cec_msg_s {
	uint8_t		msg[STM_CEC_MAX_MSG_LEN];
	uint8_t 	msg_len;
}stm_cec_msg_t;


int __must_check stm_cec_new(uint32_t device_id, stm_cec_h *device);
int __must_check stm_cec_delete(stm_cec_h device);
int __must_check stm_cec_send_msg(stm_cec_h  device, uint8_t retries, stm_cec_msg_t *cec_msg_p);
int __must_check stm_cec_receive_msg( stm_cec_h  device, stm_cec_msg_t *cec_msg_p);
int __must_check stm_cec_set_compound_control( stm_cec_h  device, stm_cec_ctrl_flag_t ctrl_flag, stm_cec_ctrl_type_t *value_p);
int __must_check stm_cec_get_compound_control(stm_cec_h device,  stm_cec_ctrl_flag_t lctrl_flag, stm_cec_ctrl_type_t *value_p);


#endif
