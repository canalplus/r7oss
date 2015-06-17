/************************************************************************
Copyright (C) 2011 STMicroelectronics. All Rights Reserved.

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

Source file name : dvb_diseqc.c

Diseqc attach / detach functions

Date        Modification                                    Name
----        ------------                                    --------

 ************************************************************************/

#include "dvb_diseqc.h"
#include "dvb_util.h"
#include <frontend/dvb_stm_fe.h>

void *diseqc_attach(struct stm_dvb_diseqc_s *dvb_diseqc)
{
	void *ret = NULL;
	char *name = dvb_diseqc->diseqc_config->diseqc_name;
	int config = 0;

	if (ATTACH_STM_FE & set_conf(name, NULL, &config)) {
		ret =
		    dvb_stm_fe_diseqc_attach(dvb_diseqc->dvb_demod->demod,
					     dvb_diseqc->diseqc_config);
		dvb_diseqc->ctrl_via_stm_fe = true;
		printk(KERN_INFO "Attached stm_fe driver for %s ...\n", name);
	} else
		printk(KERN_ERR "No diseqc device attached ...");

	return ret;
}

int diseqc_detach(struct stm_dvb_diseqc_s *dvb_diseqc)
{
	int ret = 0;
	char *name = NULL;

	if ((!dvb_diseqc->diseqc_config) || (!dvb_diseqc->dvb_demod)) {
		printk(KERN_ERR "%s: dvb_diseqc does not contain valid info\n",
								      __func__);
		return -EINVAL;
	}
	name = dvb_diseqc->diseqc_config->diseqc_name;

	if (dvb_diseqc->ctrl_via_stm_fe == true) {
		ret = dvb_stm_fe_diseqc_detach(dvb_diseqc->dvb_demod->demod);
		if (!ret)
			printk(KERN_INFO "%s: Detached stm_fe driver for %s\n",
								__func__, name);
	}

	return ret;
}
