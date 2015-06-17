/************************************************************************
Copyright (C) 2012 STMicroelectronics. All Rights Reserved.

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

Source file name : ipfec.h
Author :           SD

Configuration data types for a ip fec device

Date        Modification                                    Name
----        ------------                                    --------
30-Jul-12   Created                                         SD

************************************************************************/
#ifndef _IPFEC_H
#define _IPFEC_H

#define STM_FE_IPFEC_NAME "stm_fe_ipfec"
#define IPFEC_NAME_MAX_LEN 32
#define IPFEC_ADDR_LEN 42

struct ipfec_config_s {
	char ipfec_name[IPFEC_NAME_MAX_LEN];
	char ipfec_addr[IPFEC_ADDR_LEN];
	int ipfec_port;
	int ipfec_scheme;
};

#endif /* _IPFEC_H */
