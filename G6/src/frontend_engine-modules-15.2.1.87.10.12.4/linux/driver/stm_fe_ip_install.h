/************************************************************************
Copyright (C) 2011, 2012 STMicroelectronics. All Rights Reserved.

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

Source file name : stm_fe_ip_install.h
Author :           SD

Header for FE IP install functions

Date        Modification                                    Name
----        ------------                                    --------
20-Jan-12   Created                                         SD

************************************************************************/
#ifndef _STM_FE_IP_INSTALL_H
#define _STM_FE_IP_INSTALL_H

#define stm_fe_ip_external_attach(FUNCTION, ARGS...) ({ \
	void *__r = NULL; \
	typeof(&FUNCTION) __a = symbol_request(FUNCTION); \
	if (__a) { \
		__r = (void *) __a(ARGS); \
		if (!__r) { \
			symbol_put(FUNCTION); \
		} \
	} else { \
	    pr_err("STM_FE: Unable to find symbol "#FUNCTION"()\n"); \
	} \
	(int)__r; \
})

int ip_install(struct stm_fe_ip_s *priv);
int ipfec_install(struct stm_fe_ipfec_s *priv);
int ip_uninstall(struct stm_fe_ip_s *priv);
int ipfec_uninstall(struct stm_fe_ipfec_s *priv);

#endif /* _STM_FE_IP_INSTALL_H */
