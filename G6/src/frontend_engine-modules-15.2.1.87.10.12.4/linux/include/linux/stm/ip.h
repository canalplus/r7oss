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

Source file name : ip.h
Author :           SD

Configuration data types for a demodulator device

Date        Modification                                    Name
----        ------------                                    --------
7-Jul-11   Created                                         SD

************************************************************************/
#ifndef _IP_H
#define _IP_H

#define STM_FE_IP_NAME "stm_fe_ip"
#define IP_NAME_MAX_LEN 32
#define IP_ADDR_LEN 42

struct ip_config_s {
	char ip_name[IP_NAME_MAX_LEN];
	char ip_addr[IP_ADDR_LEN];
	int ip_port;
	int protocol;
	char ethdev[IP_NAME_MAX_LEN];
};

#endif /* _IP_H */
