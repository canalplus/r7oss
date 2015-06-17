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

Source file name : stm_frontend_lnb.h
Author :           Rahul.V

stm_fe component header

Date        Modification                                    Name
----        ------------                                    --------
16-Jun-11   Created                                         Rahul.V

************************************************************************/
#ifndef _STM_FRONTEND_LNB_H
#define _STM_FRONTEND_LNB_H

typedef enum stm_fe_lnb_tone_state_e {
	STM_FE_LNB_TONE_DEFAULT = 0,
	STM_FE_LNB_TONE_OFF = (1 << 0),
	STM_FE_LNB_TONE_22KHZ = (1 << 1)
} stm_fe_lnb_tone_state_t;

typedef enum stm_fe_lnb_polarization_e {
	STM_FE_LNB_PLR_DEFAULT = 0,
	STM_FE_LNB_PLR_OFF = (1 << 0),
	STM_FE_LNB_PLR_HORIZONTAL = (1 << 1),
	STM_FE_LNB_PLR_VERTICAL = (1 << 2)
} stm_fe_lnb_polarization_t;

typedef struct stm_fe_lnb_config_s {
	stm_fe_lnb_tone_state_t lnb_tone_state;
	stm_fe_lnb_polarization_t polarization;
} stm_fe_lnb_config_t;

typedef struct stm_fe_lnb_s *stm_fe_lnb_h;

int32_t __must_check stm_fe_lnb_new(const char *name, stm_fe_demod_h demod_obj,
							stm_fe_lnb_h *obj);
int32_t __must_check stm_fe_lnb_delete(stm_fe_lnb_h obj);
int32_t __must_check stm_fe_lnb_set_config(stm_fe_lnb_h obj,
					   stm_fe_lnb_config_t *lnbconfig);

#endif /* _STM_FRONTEND_LNB_H*/
