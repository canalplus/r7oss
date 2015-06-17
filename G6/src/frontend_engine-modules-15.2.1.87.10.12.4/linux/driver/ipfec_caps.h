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

Source file name : ipfec_caps.h
Author :           SD

Header for setting the capabilities of FE IPFEC devices

Date        Modification                                    Name
----        ------------                                    --------
07-Jul-12   Created                                         SD

************************************************************************/
#ifndef _IPFEC_CAPS_H
#define _IPFEC_CAPS_H

stm_fe_ip_fec_caps_t ipfec_caps = {

	.scheme = STM_FE_IP_FEC_SCHEME_XOR_COLUMN
};

#endif /* _IPFEC_CAPS_H */
