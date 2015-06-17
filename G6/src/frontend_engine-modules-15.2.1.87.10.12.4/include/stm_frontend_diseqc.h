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

Source file name : stm_frontend_diseqc.h
Author :           Rahul.V

stm_fe component header

Date        Modification                                    Name
----        ------------                                    --------
16-Jun-11   Created                                         Rahul.V

************************************************************************/

#ifndef _STM_FRONTEND_DISEQC_H
#define _STM_FRONTEND_DISEQC_H

typedef enum stm_fe_diseqc_mode_e {
	STM_FE_DISEQC_TONE_BURST_OFF = 0,
	STM_FE_DISEQC_TONE_BURST_CONTINUOUS = (1 << 0),
	STM_FE_DISEQC_TONE_BURST_SEND_0_UNMODULATED = (1 << 1),
	STM_FE_DISEQC_TONE_BURST_SEND_0_MODULATED = (1 << 2),
	STM_FE_DISEQC_TONE_BURST_SEND_1_MODULATED = (1 << 3),
	STM_FE_DISEQC_COMMAND = (1 << 31)
} stm_fe_diseqc_mode_t;

typedef enum stm_fe_diseqc_tranfer_op_e {
	STM_FE_DISEQC_TRANSFER_SEND = (1 << 0),
	STM_FE_DISEQC_TRANSFER_RECEIVE = (1 << 1)
} stm_fe_diseqc_tranfer_op_t;

typedef enum stm_fe_diseqc_version_e {
	STM_FE_DISEQC_VER_1_2 = (1 << 0),
	STM_FE_DISEQC_VER_2_0 = (1 << 1)
} stm_fe_diseqc_version_t;

#define STM_FE_MAX_DISEQC_PACKET_SIZE 42
typedef struct stm_fe_diseqc_msg_s {
	uint8_t msg[STM_FE_MAX_DISEQC_PACKET_SIZE];
	uint32_t msg_len;
	stm_fe_diseqc_tranfer_op_t op;
	uint32_t timeout;
	uint32_t timegap;
} stm_fe_diseqc_msg_t;

typedef struct stm_fe_diseqc_caps_s {
	stm_fe_diseqc_version_t version;
} stm_fe_diseqc_caps_t;

typedef struct stm_fe_diseqc_s *stm_fe_diseqc_h;

int32_t __must_check stm_fe_diseqc_new(const char *name,
				stm_fe_demod_h demod_obj, stm_fe_diseqc_h *obj);
int32_t __must_check stm_fe_diseqc_delete(stm_fe_diseqc_h obj);
int32_t __must_check stm_fe_diseqc_transfer(stm_fe_diseqc_h obj,
	stm_fe_diseqc_mode_t mode, stm_fe_diseqc_msg_t *msg, uint32_t num);

#endif /*_STM_FRONTEND_DISEQC_H*/
