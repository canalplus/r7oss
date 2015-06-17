/************************************************************************
Copyright (C) 2013 STMicroelectronics. All Rights Reserved.

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

Source file name : dvb_rf_matrix.c

Diseqc attach / detach functions

Date        Modification                                    Name
----        ------------                                    --------
23-Aug-13   Created					    Ashish Gandhi

 ************************************************************************/

#include "dvb_rf_matrix.h"
#include "dvb_util.h"
#include <frontend/dvb_stm_fe.h>

void *rf_matrix_attach(struct stm_dvb_rf_matrix_s *dvb_rf_matrix)
{
	void *ret = NULL;
	char *name = dvb_rf_matrix->config->name;
	int config = 0;

	if (ATTACH_STM_FE & set_conf(name, NULL, &config)) {
		ret =
		    dvb_stm_fe_rf_matrix_attach(dvb_rf_matrix->dvb_demod->demod,
					     dvb_rf_matrix->config);
		dvb_rf_matrix->ctrl_via_stm_fe = true;
		printk(KERN_INFO "Attached stm_fe driver for %s ...\n", name);
	} else
		printk(KERN_ERR "No rf_matrix device attached ...");

	return ret;
}

int rf_matrix_detach(struct stm_dvb_rf_matrix_s *dvb_rf_matrix)
{
	int ret = 0;
	char *name = NULL;

	if ((!dvb_rf_matrix->config) || (!dvb_rf_matrix->dvb_demod)) {
		printk(KERN_ERR "%s: dvb_rf_matrix does not contain valid info\n",
								      __func__);
		return -EINVAL;
	}
	name = dvb_rf_matrix->config->name;

	if (dvb_rf_matrix->ctrl_via_stm_fe == true) {
		ret =
		   dvb_stm_fe_rf_matrix_detach(dvb_rf_matrix->dvb_demod->demod);
		if (!ret)
			printk(KERN_INFO "%s: Detached stm_fe driver for %s\n",
								__func__, name);
	}

	return ret;
}
