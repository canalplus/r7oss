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

Source file name : stm_fe_install.h
Author :           Shobhit

Header for demod and tuner install functions

Date        Modification                                    Name
----        ------------                                    --------
16-Jun-11   Created                                         Shobhit

************************************************************************/
#ifndef _STM_FE_INSTALL_H
#define _STM_FE_INSTALL_H

#define stm_fe_external_attach(FUNCTION, ARGS...) ({ \
	void *__r = NULL; \
	typeof(&FUNCTION) __a = symbol_request(FUNCTION); \
	if (__a) { \
		__r = (void *) __a(ARGS); \
		if (!__r) { \
			symbol_put(FUNCTION); \
	} \
	} else { \
		__r = (void *)-ENODEV; \
	} \
	(int)__r; \
})
#define STM_FE			0
#define KERNEL_FE		1
int demod_install(struct stm_fe_demod_s *priv);
int tuner_install(struct stm_fe_demod_s *priv);
int lnb_install(struct stm_fe_lnb_s *lnb_priv);
int diseqc_install(struct stm_fe_diseqc_s *diseqc_priv);
int rf_matrix_install(struct stm_fe_rf_matrix_s *priv);
int demod_uninstall(struct stm_fe_demod_s *priv);
int tuner_uninstall(struct stm_fe_demod_s *priv);
int lnb_uninstall(struct stm_fe_lnb_s *lnb_priv);
int diseqc_uninstall(struct stm_fe_diseqc_s *diseqc_priv);
int rf_matrix_uninstall(struct stm_fe_rf_matrix_s *priv);
int demod_install_dt(struct stm_fe_demod_s *priv);
int tuner_install_dt(struct stm_fe_demod_s *priv);
int lnb_install_dt(struct stm_fe_lnb_s *lnb_priv);
int diseqc_install_dt(struct stm_fe_diseqc_s *diseqc_priv);
int rf_matrix_install_dt(struct stm_fe_rf_matrix_s *priv);
int demod_uninstall_dt(struct stm_fe_demod_s *priv);
int tuner_uninstall_dt(struct stm_fe_demod_s *priv);
int lnb_uninstall_dt(struct stm_fe_lnb_s *lnb_priv);
int diseqc_uninstall_dt(struct stm_fe_diseqc_s *diseqc_priv);
int rf_matrix_uninstall_dt(struct stm_fe_rf_matrix_s *priv);

#endif /* _STM_FE_INSTALL_H */
