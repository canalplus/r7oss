/************************************************************************
#include "ce_dt.h"
Copyright (C) 2011,2012 STMicroelectronics. All Rights Reserved.

This file is part of the Crypto_engine Library.

Crypto_engine is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

Crypto_engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Crypto_engine; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The Crypto_engine Library may alternatively be licensed under a proprietary
license from ST.

Source file name : stm_ce_pm.c

Declares stm_ce module init and exit functions
************************************************************************/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/moduleparam.h>
#include <linux/of.h>
#include <linux/suspend.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>

#include "stm_ce.h"
#include "stm_ce_osal.h"
#include "stm_ce_version.h"
#include "stm_ce_objects.h"
#include "ce_path_mme.h"
#include "stm_ce_mme_share.h"
#include "ce_path.h"
#include "ce_dt.h"
#include "mme.h"



#include <linux/pm_runtime.h>

#ifndef CONFIG_ARCH_STI
#include <linux/stm/platform.h>
#include <linux/stm/clk.h>
#endif

MODULE_AUTHOR("STMicroelectronics");
MODULE_DESCRIPTION("STKPI Crypto Engine");
MODULE_SUPPORTED_DEVICE("");
MODULE_LICENSE("GPL");
MODULE_VERSION(STM_CE_VERSION);

#ifdef CONFIG_STM_CE_TRACE
/* Module parameters to enable/disable trace */
int ce_trace_api_entry;
int ce_trace_api_exit;
int ce_trace_int_entry;
int ce_trace_int_exit;
int ce_trace_info;
int ce_trace_mme;
int ce_trace_error;

module_param(ce_trace_api_entry, int, S_IRUGO | S_IWUSR);
module_param(ce_trace_api_exit, int, S_IRUGO | S_IWUSR);
module_param(ce_trace_int_entry, int, S_IRUGO | S_IWUSR);
module_param(ce_trace_int_exit, int, S_IRUGO | S_IWUSR);
module_param(ce_trace_info, int, S_IRUGO | S_IWUSR);
module_param(ce_trace_error, int, S_IRUGO | S_IWUSR);
module_param(ce_trace_mme, int, S_IRUGO | S_IWUSR);

MODULE_PARM_DESC(ce_trace_api_entry, "Enables stm_ce api entry trace");
MODULE_PARM_DESC(ce_trace_api_exit, "Enables stm_ce api exit trace");
MODULE_PARM_DESC(ce_trace_error, "Enables stm_ce error trace");
MODULE_PARM_DESC(ce_trace_info, "Enables stm_ce info trace");
MODULE_PARM_DESC(ce_trace_mme, "Enables stm_ce MME trace");
MODULE_PARM_DESC(ce_trace_int_entry, "Enables internal function entry trace");
MODULE_PARM_DESC(ce_trace_int_exit, "Enables internal function exit trace");
#endif

static int ce_pm_suspend(struct device *dev)
{
	CE_INFO("Start CE suspend\n");

	/* Terminate the stm_ce */
	stm_ce_term();

	CE_INFO("Done CE suspend\n");
	return 0;
}

static int ce_pm_resume(struct device *dev)
{
	int err = 0;

	CE_INFO("Start CE resume\n");
	/* Reinitialize the stm_ce */
	err = stm_ce_init();
	if (err)
		CE_ERROR("stm_ce_init error 0x%x\n", err);

	CE_INFO("Done CE resume\n");
	return err;
}

#ifdef CONFIG_PM_RUNTIME
static int ce_pm_runtime_suspend(struct device *dev)
{
	int err = 0;
	stm_ce_t *ce = dev_get_drvdata(dev);

	CE_INFO("Start runtime suspend\n");

	err = ce_hal_clk_disable(ce->pdev);
	if (err) {
		CE_ERROR("Failed to disable clocks (%d)\n", err);
		return err;
	}

	CE_INFO("CE in Suspend State\n");
	return err;
}

static int ce_pm_runtime_resume(struct device *dev)
{
	int err = 0;
	stm_ce_t *ce = dev_get_drvdata(dev);

	CE_INFO("Start resume\n");
	err = ce_hal_clk_enable(ce->pdev);
	if (err) {
		CE_ERROR("Failed to enable clocks (%d)\n", err);
		return err;
	}

	CE_INFO("CE in Resume state\n");
	return err;
}
#endif

/* Notifier callback
 * Called by the PM framework prior / after entering / leaving a state
 */
static int ce_pm_notifier_call(struct notifier_block *this,
				unsigned long event, void *ptr)
{
	switch (event) {
	case PM_SUSPEND_PREPARE:
		CE_INFO("CE notifier: PM_SUSPEND_PREPARE\n");
		ce_pm_suspend(&stm_ce_global->pdev->dev);
		CE_INFO("CE notifier: PM_SUSPEND_PREPARE done\n");
		break;
	case PM_POST_SUSPEND:
		CE_INFO("CE notifier: PM_POST_SUSPEND\n");
		ce_pm_resume(&stm_ce_global->pdev->dev);
		CE_INFO("CE notifier: PM_POST_SUSPEND done\n");
		break;

	default:
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block ce_pm_notifier = {
	.notifier_call = ce_pm_notifier_call,
};

static int __init pm_ce_probe(struct platform_device *pdev)
{
	int ret = -EINVAL;
	stm_ce_t *ce = stm_ce_global;

	ret = ce_hal_dt_check();
	if (ret) {
		CE_ERROR("Failed to find CE DT data (%d)", ret);
		return -EINVAL;
	}

	ret = ce_hal_dt_get_pdata(pdev);
	if (ret) {
		CE_ERROR("Failed to parse CE DT data (%d)", ret);
		return ret;
	}

	ret = ce_hal_clk_enable(pdev);
	if (ret) {
		CE_ERROR("Failed to enable clocks (%d)", ret);
		goto cleanup_pdata;
	}

	ce->pdev = pdev;
	platform_set_drvdata(pdev, ce);

	ret = stm_ce_init();
	if (ret) {
		CE_ERROR("stm_ce_init error 0x%x\n", ret);
		goto cleanup_pdata;
	}

	register_pm_notifier(&ce_pm_notifier);

#ifdef CONFIG_PM_RUNTIME
	/* Enable runtime power management */
	ret = pm_runtime_set_active(&pdev->dev);
	if (ret) {
		CE_ERROR("pm_runtime_set_active error 0x%x\n", ret);
		goto term_ce;
	}
#endif
	pm_suspend_ignore_children(&pdev->dev, 1);
#ifdef CONFIG_PM_RUNTIME
	pm_runtime_enable(&pdev->dev);
#endif

	return 0;

#ifdef CONFIG_PM_RUNTIME
term_ce:
	stm_ce_term();
#endif
cleanup_pdata:
	ce_hal_dt_put_pdata(pdev);

	return ret;
}

static int pm_ce_remove(struct platform_device *pdev)
{
	int ret;

	stm_ce_term();
#ifdef CONFIG_PM_RUNTIME
	pm_runtime_disable(&pdev->dev);
#endif

	unregister_pm_notifier(&ce_pm_notifier);

	ret = ce_hal_dt_put_pdata(pdev);
	if (ret) {
		CE_ERROR("Failed to release CE DT data (%d)", ret);
		return ret;
	}

	CE_INFO("TE-PM: pm_ce_remove\n");
	return 0;
}

void pm_ce_dev_release(struct device *dev)
{
}

static const struct dev_pm_ops ce_pm_ops = {
#ifdef CONFIG_PM_RUNTIME
	.runtime_suspend = ce_pm_runtime_suspend,
	.runtime_resume = ce_pm_runtime_resume,
#endif
};

static struct platform_driver __refdata pm_ce_driver = {
	.driver = {
		.name = "stm-ce",
		.owner = THIS_MODULE,
		.pm = &ce_pm_ops,
		.of_match_table = of_match_ptr(ce_dt_sc_match),
		},
	.probe = pm_ce_probe,
	.remove = pm_ce_remove,
};

static int __init pm_ce_init(void)
{
	int err;

	stm_ce_global = kzalloc(sizeof(stm_ce_t), GFP_KERNEL);
	if (stm_ce_global == NULL)
		return -ENOMEM;

	err = platform_driver_register(&pm_ce_driver);
	CE_INFO("platform_driver_register done: %d\n", err);

	return err;
}
module_init(pm_ce_init);

static void __exit pm_ce_release(void)
{
	platform_driver_unregister(&pm_ce_driver);
	CE_INFO("platform_driver_unregister done\n");
	kfree(stm_ce_global);
	stm_ce_global = NULL;
}
module_exit(pm_ce_release);

