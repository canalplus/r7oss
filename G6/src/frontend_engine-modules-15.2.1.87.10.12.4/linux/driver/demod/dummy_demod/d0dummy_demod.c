/************************************************************************
Copyright (C) 2011 STMicroelectronics. All Rights Reserved.

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

Source file name : d0dummy_demod.c
Author :           Rahul

wrapper for dummy_demod driver

Date        Modification                                    Name
----        ------------                                    --------
20-Jun-11   Created                                         Shobhit

************************************************************************/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/platform_device.h>
#include <linux/stm/plat_dev.h>
#include <linux/stm/demod.h>
#include <linux/stm/lnb.h>
#include <linux/stm/diseqc.h>
#include <stm_registry.h>
#include <stm_fe.h>
#include <stfe_utilities.h>
#include <stm_fe_os.h>
#include <i2c_wrapper.h>
#include <stm_fe_demod.h>
#include <stm_fe_diseqc.h>
#include "d0dummy_demod.h"

#define DUMMY_DEMOD_CURRENT_DRV_REVISION "DUMMY_DEMOD-DRV_REL_1.0"

static int dummy_demod_init(struct stm_fe_demod_s *priv);
static int dummy_demod_tune(struct stm_fe_demod_s *priv, bool *lock);
static int dummy_demod_scan(struct stm_fe_demod_s *priv, bool *lock);
static int dummy_demod_status(struct stm_fe_demod_s *priv, bool *locked);
static int dummy_demod_tracking(struct stm_fe_demod_s *priv);
static int dummy_demod_unlock(struct stm_fe_demod_s *priv);
static int dummy_demod_term(struct stm_fe_demod_s *priv);
static int dummy_demod_abort(struct stm_fe_demod_s *priv, bool abort);
static int dummy_demod_standby(struct stm_fe_demod_s *priv, bool standby);
static int dummy_demod_restore(struct stm_fe_demod_s *priv);

/*
 * Name: stm_fe_dummy_demod_attach()
 *
 * Description: Installed the wrapper function pointers for driver
 *
 */
int stm_fe_dummy_demod_attach(struct stm_fe_demod_s *priv)
{
	if (!priv->ops)
		return -EFAULT;

	pr_info("%s\n", __func__);

	priv->ops->init = dummy_demod_init;
	priv->ops->tune = dummy_demod_tune;
	priv->ops->scan = dummy_demod_scan;
	priv->ops->tracking = dummy_demod_tracking;
	priv->ops->status = dummy_demod_status;
	priv->ops->unlock = dummy_demod_unlock;
	priv->ops->term = dummy_demod_term;
	priv->ops->abort = dummy_demod_abort;
	priv->ops->standby = dummy_demod_standby;
	priv->ops->restore = dummy_demod_restore;
	priv->ops->detach = stm_fe_dummy_demod_detach;

	return 0;
}
EXPORT_SYMBOL(stm_fe_dummy_demod_attach);

/*
 * Name: stm_fe_dummy_demod_detach()
 *
 * Description: Uninstalled the wrapper function pointers for driver
 *
 */
int stm_fe_dummy_demod_detach(struct stm_fe_demod_s *priv)
{
	if (!priv->ops)
		return -EFAULT;

	priv->ops->detach = NULL;
	priv->ops->standby = NULL;
	priv->ops->restore = NULL;
	priv->ops->abort = NULL;
	priv->ops->term = NULL;
	priv->ops->unlock = NULL;
	priv->ops->status = NULL;
	priv->ops->tracking = NULL;
	priv->ops->scan = NULL;
	priv->ops->tune = NULL;
	priv->ops->init = NULL;

	return 0;
}
EXPORT_SYMBOL(stm_fe_dummy_demod_detach);



/*
 * Name: dummy_demod_init()
 *
 * Description: Providing Initiliazing parameters to driver
 *
 */
static int dummy_demod_init(struct stm_fe_demod_s *priv)
{

	pr_info("%s\n", __func__);

	/* Implement device initialisation here */
	/* Configuration information is in priv->config */

	return 0;
}

/*
 * Name: dummy_demod_term()
 *
 * Description: terminating the dummy_demod driver
 *
 */
static int dummy_demod_term(struct stm_fe_demod_s *priv)
{

	pr_info("%s\n", __func__);

	/* Implement device termination here */

	return 0;
}

/*
 * Name: dummy_demod_tune()
 *
 * Description: Provided tuning parameters to driver and perfomed tuning
 *
 */
static int dummy_demod_tune(struct stm_fe_demod_s *priv, bool *lock)
{

	pr_info("%s\n", __func__);
	*lock = true;
	/* Implement tune algorithm here */
	return 0;
}

/*
 * Name: dummy_demod_scan()
 *
 * Description: Provided scanning parameters to driver and perfomed tuning
 *
 */
static int dummy_demod_scan(struct stm_fe_demod_s *priv, bool *lock)
{

	pr_info("%s\n", __func__);

	/* Implement scan algorithm here */
	return 0;
}



/*
 * Name: dummy_demod_tracking()
 *
 * Description: Checking the lock status of demodulator
 *
 */
static int dummy_demod_tracking(struct stm_fe_demod_s *priv)
{

	/*pr_info("%s\n", __func__);*/

	/* Implement signal statistics tracking here */
	return 0;

}

static int dummy_demod_status(struct stm_fe_demod_s *priv, bool *locked)
{

	/* pr_info("%s\n", __func__);*/
	*locked = true;
	/* Implement signal status tracking here */

	return 0;
}


static int dummy_demod_unlock(struct stm_fe_demod_s *priv)
{

	pr_info("%s\n", __func__);

	/* Implement unlock here */
	return 0;
}

static int dummy_demod_abort(struct stm_fe_demod_s *priv, bool abort)
{

	pr_info("%s\n", __func__);

	/* Implement abort here */
	return 0;
}

static int dummy_demod_standby(struct stm_fe_demod_s *priv, bool standby)
{

	pr_info("%s\n", __func__);

	/* Implement standby here */
	return 0;
}

static int dummy_demod_restore(struct stm_fe_demod_s *priv)
{

	pr_info("%s\n", __func__);

	/* Implement restore here */
	return 0;
}

static int32_t __init dummy_demod_drv_init(void)
{
	pr_info("Loading dummy_demod driver module...\n");

	return 0;
}

module_init(dummy_demod_drv_init);

static void __exit dummy_demod_drv_term(void)
{
	pr_info("Removing dummy_demod driver module ...\n");

}

module_exit(dummy_demod_drv_term);

MODULE_DESCRIPTION("Low level driver for DUMMY_DEMOD satellite demodulator");
MODULE_AUTHOR("STMicroelectronics Limited");
MODULE_VERSION(DUMMY_DEMOD_CURRENT_DRV_REVISION);
MODULE_LICENSE("GPL");
