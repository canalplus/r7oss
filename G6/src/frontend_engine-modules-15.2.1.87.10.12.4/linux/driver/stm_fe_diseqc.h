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

Source file name : stm_fe_diseqc.h
Author :           Rahul.V

Header for stm_fe_diseqc.c

Date        Modification                                    Name
----        ------------                                    --------
16-Jun-11   Created                                         Rahul.V

************************************************************************/
#ifndef _STM_FE_DISEQC_H
#define _STM_FE_DISEQC_H

struct stm_fe_diseqc_s;

struct diseqc_ops_s {
	int (*init)(struct stm_fe_diseqc_s *priv);
	int (*transfer)(struct stm_fe_diseqc_s *priv, stm_fe_diseqc_mode_t mode,
				stm_fe_diseqc_msg_t *msg, uint32_t num_msg);
	int (*term)(struct stm_fe_diseqc_s *priv);
	int (*detach)(struct stm_fe_diseqc_s *priv);
};

struct stm_fe_diseqc_s {
	struct list_head list;
	char diseqc_name[32];
	uint32_t diseqc_id;
	stm_fe_demod_h demod_h;
	struct diseqc_config_s *config;
	struct diseqc_ops_s *ops;
	bool bypass_control;
	bool rpm_suspended;
	struct platform_device *diseqc_data;
	bool dt_enable;
	stm_fe_diseqc_mode_t mode;
};
int stm_fe_diseqc_probe(struct platform_device *pdev);
int stm_fe_diseqc_remove(struct platform_device *pdev);

#ifdef CONFIG_PM
int32_t stm_fe_diseqc_resume(struct device *dev);
int32_t stm_fe_diseqc_suspend(struct device *dev);
int32_t stm_fe_diseqc_restore(struct device *dev);
int32_t stm_fe_diseqc_freeze(struct device *dev);
#endif

struct stm_fe_diseqc_s *diseqc_from_name(const char *name);
struct stm_fe_diseqc_s *diseqc_from_id(uint32_t id);

#endif /* _STM_FE_DISEQC_H */
