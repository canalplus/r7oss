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

Source file name : stm_frontend_satcr.h
Author :           Rahul.V

stm_fe component header

Date        Modification                                    Name
----        ------------                                    --------
16-Jun-11   Created                                         Rahul.V

************************************************************************/

#ifndef _STM_FRONTEND_SATCR_H
#define _STM_FRONTEND_SATCR_H

typedef enum stm_fe_satcr_command_type_e {
	STM_FE_SATCR_ODU_CHANNEL_CHANGE = (1 << 0),
	STM_FE_SATCR_ODU_POWER_OFF = (1 << 1)
} stm_fe_satcr_command_type_t;

typedef enum stm_fe_satcr_lnb_index_e {
	STM_FE_SATCR_LNB0 = (1 << 0),
	STM_FE_SATCR_LNB1 = (1 << 1),
	STM_FE_SATCR_LNB2 = (1 << 2),
	STM_FE_SATCR_LNB3 = (1 << 3)
} stm_fe_satcr_lnb_index_t;

typedef enum stm_fe_satcr_bpf_index_e {
	STM_FE_SATCR_UB_0 = (1<<0),
	STM_FE_SATCR_UB_1 = (1<<1),
	STM_FE_SATCR_UB_2 = (1<<2),
	STM_FE_SATCR_UB_3 = (1<<3),
	STM_FE_SATCR_UB_4 = (1<<4),
	STM_FE_SATCR_UB_5 = (1<<5),
	STM_FE_SATCR_UB_6 = (1<<6),
	STM_FE_SATCR_UB_7 = (1<<7),
	STM_FE_SATCR_UB_8 = (1<<8),
	STM_FE_SATCR_UB_9 = (1<<9),
	STM_FE_SATCR_UB_10 = (1<<10),
	STM_FE_SATCR_UB_11 = (1<<11)
} stm_fe_satcr_bpf_index_t;

typedef struct stm_fe_satcr_command_s {
	uint32_t freq;
	stm_fe_satcr_command_type_t type;
	stm_fe_satcr_lnb_index_t lnb_index;
	stm_fe_satcr_bpf_index_t user_band;
} stm_fe_satcr_command_t;

#endif /*_STM_FRONTEND_SATCR_H*/
