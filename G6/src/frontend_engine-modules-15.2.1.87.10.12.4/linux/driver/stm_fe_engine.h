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

Source file name : stm_fe_engine.h
Author :           Rahul.V

Header for stm_fe_engine.c

Date        Modification                                    Name
----        ------------                                    --------
18-Jul-11   Created                                         Rahul.V

************************************************************************/
#ifndef _STM_FE_ENGINE_H
#define _STM_FE_ENGINE_H

struct stm_fe_engine_s {
	struct list_head demod_list;
	struct list_head lnb_list;
	struct list_head diseqc_list;
	struct list_head rf_matrix_list;
	struct list_head ip_list;
	struct list_head ipfec_list;
	struct stm_fe_demod_obj_s demod_obj_type;
	struct stm_fe_ip_obj_s ip_obj_type;
	uint32_t obj_cnt;
	stm_fe_object_info_t *fe;
};

struct list_head *get_demod_list(void);
struct list_head *get_diseqc_list(void);
struct list_head *get_rf_matrix_list(void);
struct list_head *get_lnb_list(void);
void incr_obj_cnt(void);
void decr_obj_cnt(void);
stm_object_h *get_type_for_registry(void);

#endif /* _STM_FE_ENGINE_H */
