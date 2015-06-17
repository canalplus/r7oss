/************************************************************************
Copyright (C) 2012 STMicroelectronics. All Rights Reserved.

This file is part of the STLinuxTV Library.

STLinuxTV is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

STLinuxTV is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with STLinuxTV; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The STLinuxTV Library may alternatively be licensed under a proprietary
license from ST.

Source file name : dvb_ipfec.c
Author :           Shantanu Dey

Specific fe ip fec api's attach functions

Date        Modification                                    Name
----        ------------                                    --------
30-Aug-12   Created                                         SD

 ************************************************************************/
#include <linux/platform_device.h>
#include <linux/stm/ip.h>
#include <frontend/dvb_ipfe.h>
#include <frontend/dvb_stm_fe.h>
#include "dvb_adaptation.h"

int ipfec_attach(struct stm_dvb_ipfec_s *dvb_ipfec)
{
	int ret = 0;
	ret = dvb_stm_fe_ipfec_attach(dvb_ipfec->dvb_ip->ip,
					     dvb_ipfec->ipfec_config);
	return ret;
}

EXPORT_SYMBOL(ipfec_attach);
