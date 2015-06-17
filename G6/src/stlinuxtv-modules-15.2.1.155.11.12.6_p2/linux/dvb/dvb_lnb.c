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

Source file name : dvb_lnb.c
Author :           Rahul.V

Specific LNB device attach functions

Date        Modification                                    Name
----        ------------                                    --------
16-Jun-11   Created                                         Rahul.V

************************************************************************/
#include "dvb_lnb.h"
#include <lnbh24.h>

#include "dvb_util.h"
#include <frontend/dvb_stm_fe.h>

static void *dvb_lnbh24_attach(struct stm_dvb_lnb_s *dvb_lnb)
{
	struct dvb_frontend *fe = NULL;
	fe = dvb_attach(lnbh24_attach,
			dvb_lnb->dvb_demod->demod, dvb_lnb->i2c,
			0, 0, dvb_lnb->lnb_config->lnb_io.address);
	return fe;
}

void *lnb_attach(struct stm_dvb_lnb_s *dvb_lnb)
{
	void *ret = NULL;
	char *name;
	int config = 0;

	if (!dvb_lnb) {
		printk(KERN_ERR "%s(): Invalid lnb context\n", __func__);
		goto lnb_attach_failed;
	}

	name = dvb_lnb->lnb_config->lnb_name;
	if (ATTACH_STM_FE & set_conf(name, NULL, &config)) {
		ret =
		    dvb_stm_fe_lnb_attach(dvb_lnb->dvb_demod->demod,
					  dvb_lnb->lnb_config);
		dvb_lnb->ctrl_via_stm_fe = true;
		printk(KERN_INFO "%s(): Attached stm_fe driver for %s ...\n",
		       __func__, name);
	} else if (DEVICE_MATCH & set_conf(name, "LNBH24", &config))
		ret = dvb_lnbh24_attach(dvb_lnb);
	else
		printk(KERN_ERR " %s(): No lnb device attached ...", __func__);

lnb_attach_failed:
	return ret;
}

int lnb_detach(struct stm_dvb_lnb_s *dvb_lnb)
{
	int ret = 0;
	char *name = NULL;

	if ((!dvb_lnb->lnb_config) || (!dvb_lnb->dvb_demod)) {
		printk(KERN_ERR "%s: dvb_lnb does not contain valid info\n",
								      __func__);
		return -EINVAL;
	}
	name = dvb_lnb->lnb_config->lnb_name;

	if (dvb_lnb->ctrl_via_stm_fe == true) {
		ret = dvb_stm_fe_lnb_detach(dvb_lnb->dvb_demod->demod);
		if (!ret)
			printk(KERN_INFO "%s: Detached stm_fe driver for %s\n",
								__func__, name);
	}

	return ret;
}
