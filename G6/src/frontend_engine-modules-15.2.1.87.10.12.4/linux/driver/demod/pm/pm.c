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

Source file name : pm.c
Author :           Ankur

power management of demod

Date        Modification                                    Name
----        ------------                                    --------
22-Mar-12   Created                                         Ankur
22-May-13   Added CPS support                               Ankur
************************************************************************/

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/stm/plat_dev.h>
#include <linux/stm/demod.h>
#include <stm_registry.h>
#include <stm_fe.h>
#include <stfe_utilities.h>
#include <stm_fe_os.h>
#include <i2c_wrapper.h>
#include <stm_fe_demod.h>
#include "pm.h"

/*
 * Name: demod_suspend()
 *
 * Description: configure the stm_fe device for standby mode.
 */
int32_t demod_suspend(struct stm_fe_demod_s *priv)
{
	int32_t ret = -1;

	pr_info("%s: STM_FE demod suspending in low power mode...\n", __func__);

	/*Abort demod and tuner operations to get the semaphore quickly*/
	if (priv->ops->abort) {
		ret = priv->ops->abort(priv, TRUE);
		if (ret) {
			pr_err("%s: demod_abort failed\n", __func__);
			goto demod_suspend_fail;
		}
	} else {
		pr_err("%s: FP priv->ops->abort is NULL\n", __func__);
		ret = -EFAULT;
		goto demod_suspend_fail;
	}
	/* Wait for guardSemaphore */
	ret = stm_fe_sema_wait(priv->fe_task.guard_sem);
	if (ret)
		pr_err("%s: stm_fe_sema_wait failed: %d\n", __func__, ret);
	/*Undo the Abort before proceeding to acquisition*/
	ret = priv->ops->abort(priv, FALSE);
	if (ret) {
		pr_err("%s: demod_abort failed\n", __func__);
		goto suspend_fail_rel_sem;
	}

	if (priv->ops->standby) {
		ret = priv->ops->standby(priv, true);
		if (ret) {
			pr_err("%s: demod_standby failed: %d\n", __func__, ret);
			goto suspend_fail_rel_sem;
		}
	} else {
		pr_err("%s: FP priv->ops->standby is NULL\n", __func__);
		ret = -EFAULT;
		goto suspend_fail_rel_sem;
	}
	/* suspend the demod state machine by not releasing guard semaphore*/

	return ret;
suspend_fail_rel_sem:
	stm_fe_sema_signal(priv->fe_task.guard_sem);
demod_suspend_fail:
	return ret;
}

/*
 * Name: demod_resume()
 *
 * Description: configure the stm_fe device for wake-up.
 */
int32_t demod_resume(struct stm_fe_demod_s *priv)
{
	int32_t ret = -1;

	pr_info("%s: STM_FE demod resuming in normal power mode..\n", __func__);

	if (priv->ops->standby) {
		ret = priv->ops->standby(priv, false);
		if (ret) {
			pr_err("%s: demod_standby failed: %d\n", __func__, ret);
			return ret;
		}
	} else {
		pr_err("%s: FP priv->ops->standby is NULL\n", __func__);
		return -EFAULT;
	}

	if (priv->fe_task.prevstate != FE_TASKSTATE_TRACKING)
		/*Bring task to previous state*/
		priv->fe_task.nextstate = priv->fe_task.prevstate;
	else
		/*Bring task to previous state*/
		priv->fe_task.nextstate = FE_TASKSTATE_TUNE;

	priv->fe_task.delay_in_ms = DEMOD_WAKE_UP;
	fe_wake_up_interruptible(&priv->fe_task.waitq);

	/* This is to avoid high CPU usage if we are waking in IDLE state */
	if (priv->fe_task.nextstate == FE_TASKSTATE_IDLE)
		priv->fe_task.delay_in_ms = DEMOD_IDLE_PERIOD_MS;

	/*Signal guard semaphore to continue demod state machine */
	stm_fe_sema_signal(priv->fe_task.guard_sem);

	return ret;
}

/*
 * Name: demod_restore()
 *
 * Description: configure the stm_fe device after wake-up in CPS.
 */
int32_t demod_restore(struct stm_fe_demod_s *priv)
{
	int32_t ret = -1;

	pr_info("%s: STM_FE demod resuming in normal power mode..\n", __func__);

	if (priv->ops->restore) {
		ret = priv->ops->restore(priv);
		if (ret) {
			pr_err("%s: demod_restore failed: %d\n", __func__, ret);
			return ret;
		}
	} else {
		pr_err("%s: FP priv->ops->restore is NULL\n", __func__);
		return -EFAULT;
	}

	if (priv->fe_task.prevstate != FE_TASKSTATE_TRACKING)
		/*Bring task to previous state*/
		priv->fe_task.nextstate = priv->fe_task.prevstate;
	else
		/*Bring task to previous state*/
		priv->fe_task.nextstate = FE_TASKSTATE_TUNE;

	priv->fe_task.delay_in_ms = DEMOD_WAKE_UP;
	fe_wake_up_interruptible(&priv->fe_task.waitq);

	/*Signal guard semaphore to continue demod state machine */
	stm_fe_sema_signal(priv->fe_task.guard_sem);

	return ret;
}
