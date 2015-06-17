/************************************************************************
Copyright (C) 2013 STMicroelectronics. All Rights Reserved.

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

Source file name : stm_fe_rf_matrix.h
Author :           Ashish Gandhi

Header for stm_fe_rf_matrix.c

Date        Modification                                    Name
----        ------------                                    --------
24-Jul-13   Created                                         Ashish Gandhi
************************************************************************/
#ifndef _STM_FE_RF_MATRIX_H
#define _STM_FE_RF_MATRIX_H

struct stm_fe_rf_matrix_s;

struct rf_matrix_ops_s {
	int (*init)(struct stm_fe_rf_matrix_s *priv);
	int (*term)(struct stm_fe_rf_matrix_s *priv);
	int (*select_source)(struct stm_fe_rf_matrix_s *priv, uint32_t src_num);
	int (*detach)(struct stm_fe_rf_matrix_s *priv);
};

struct stm_fe_rf_matrix_s {
	struct list_head list;
	char name[32];
	uint32_t id;
	uint32_t grp_list_id;
	stm_fe_demod_h demod_h;
	stm_fe_lnb_h lnb_h;
	STCHIP_Handle_t rf_matrix_h;
	struct rf_matrix_config_s *config;
	fe_i2c_adapter i2c;
	struct rf_matrix_ops_s *ops;
	bool bypass_control;
	bool rpm_suspended;
	struct platform_device *data;
	bool dt_enable;
};
int stm_fe_rf_matrix_probe(struct platform_device *pdev);
int stm_fe_rf_matrix_remove(struct platform_device *pdev);

#ifdef CONFIG_PM
int32_t stm_fe_rf_matrix_suspend(struct device *dev);
int32_t stm_fe_rf_matrix_resume(struct device *dev);
int32_t stm_fe_rf_matrix_restore(struct device *dev);
int32_t stm_fe_rf_matrix_freeze(struct device *dev);
#endif

struct stm_fe_rf_matrix_s *rf_matrix_from_name(const char *name);
struct stm_fe_rf_matrix_s *rf_matrix_from_id(uint32_t id);
struct stm_fe_rf_matrix_s *rf_matrix_from_id_grp(uint32_t id, uint32_t grp_id);

#endif /* _STM_FE_RF_MATRIX_H */
