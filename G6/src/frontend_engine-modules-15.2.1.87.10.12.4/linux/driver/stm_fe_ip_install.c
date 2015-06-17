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

Source file name : stm_fe_ip_install.c
Author :          SD

attach and detach the FE IP low level function

Date        Modification                                    Name
----        ------------                                    --------
20-Jan-12   Created                                         SD
************************************************************************/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/platform_device.h>
#include <linux/stm/plat_dev.h>
#include <linux/stm/ip.h>
#include <stm_registry.h>
#include <stm_fe.h>
#include <stm_fe_os.h>
#include "stm_fe_ip.h"
#include "stm_fe_ipfec.h"
#include <fe_ip.h>
#include "stm_fe_ip_install.h"

/*
 * Name: ip_install()
 *
 * Description: low level function pointer installation according to FE IP type
 *
 */
int ip_install(struct stm_fe_ip_s *priv)
{
	int ret = 0;

	ret = stm_fe_ip_external_attach(fe_ip_attach, priv);
	if (ret) {
		pr_err("%s: unable to Install FE IP Functions\n", __func__);
		return -EFAULT;
	}

	return ret;
}

/*
 * Name: ip_uninstall()
 *
 * Description: uninstall the wrapper function pointer for FE IP
 *
 */
int ip_uninstall(struct stm_fe_ip_s *priv)
{
	int ret = 0;

	if (priv->ops->fe_ip_detach) {
		priv->ops->fe_ip_detach(priv);
	} else {
		pr_err("%s: FP detach is NULL\n", __func__);
		return -EFAULT;
	}

	return ret;
}
/*
 * Name: ipfec_install()
 *
 * Description: low level function pointer installation according to IP FECtype
 *
 */
int ipfec_install(struct stm_fe_ipfec_s *priv)
{
	int ret = 0;

	ret = stm_fe_ip_external_attach(fe_ipfec_attach, priv);
	if (ret) {
		pr_err("%s: unable to Install FE IP Functions\n", __func__);
		return -EFAULT;
	}

	return ret;
}

/*
 * Name: ipfec_uninstall()
 *
 * Description: uninstall the wrapper function pointer for IP FEC
 *
 */
int ipfec_uninstall(struct stm_fe_ipfec_s *priv)
{
	int ret = 0;

	if (priv->ops->fe_ipfec_detach)
		priv->ops->fe_ipfec_detach(priv);
	else {
		pr_err("%s: FP detach is NULL\n", __func__);
		return -EFAULT;
	}

	return ret;
}
