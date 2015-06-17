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

Source file name : stm_frontend_rf_matrix.h
Author :           Rahul.V

stm_fe component header

Date        Modification                                    Name
----        ------------                                    --------
16-Jun-11   Created                                         Rahul.V

************************************************************************/
#ifndef _STM_FRONTEND_RF_MATRIX_H
#define _STM_FRONTEND_RF_MATRIX_H

typedef struct stm_fe_rf_matrix_caps_s {
	uint32_t max_input;
} stm_fe_rf_matrix_caps_t;

typedef struct stm_fe_rf_matrix_s *stm_fe_rf_matrix_h;

int32_t __must_check stm_fe_rf_matrix_new(const char *name,
			stm_fe_demod_h demod_obj, stm_fe_rf_matrix_h *obj);
int32_t __must_check stm_fe_rf_matrix_delete(stm_fe_rf_matrix_h obj);
int32_t __must_check stm_fe_rf_matrix_select_source(stm_fe_rf_matrix_h obj,
							uint32_t src_num);

#endif /* _STM_FRONTEND_RF_MATRIX_H */
