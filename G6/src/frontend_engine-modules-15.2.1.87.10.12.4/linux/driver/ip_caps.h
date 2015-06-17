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

Source file name : ip_caps.h
Author :           SD

Header for setting the capabilities of FE IP devices

Date        Modification                                    Name
----        ------------                                    --------
07-Jul-11   Created                                         SD

************************************************************************/
#ifndef _IP_CAPS_H
#define _IP_CAPS_H

stm_fe_ip_caps_t ipfe_caps = {

	.ec_types = STM_FE_IP_ERROR_CONTROL_NONE,
	.protocol = STM_FE_IP_PROTOCOL_RTP_TS | STM_FE_IP_PROTOCOL_UDP_TS,

};


#endif /* _IP_CAPS_H */
