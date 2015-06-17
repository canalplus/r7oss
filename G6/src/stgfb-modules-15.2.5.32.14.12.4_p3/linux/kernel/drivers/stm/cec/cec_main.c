/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/cec/cec_main.c
 * Copyright (c) 2005-2014 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/init.h>/* Initilisation support */
#include <linux/device.h>
#include <linux/kernel.h>  /* Kernel support */
#include <linux/clk.h>
#include <linux/pm_runtime.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/of.h>
#ifdef CONFIG_ARCH_STI
#include <linux/reset.h>
#include <linux/platform_device.h>
#include <linux/pinctrl/consumer.h>
#else
#include <linux/stm/platform.h>
#include <linux/stm/pad.h>
#include <linux/stm/sysconf.h>
#endif

#include <linux/stm/cecplatform.h>
#include "stm_cec.h"
#include "cec_core.h"
#include "cec_debug.h"

MODULE_AUTHOR("BHARAT");
MODULE_DESCRIPTION("STMicroelectronics CEC driver");
MODULE_LICENSE("GPL");


static void *stm_cec_dt_get_pdata(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct stm_cec_platform_data *data;
	uint32_t of_data;

	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data) {
		printk(KERN_ALERT "%s : Unable to allocate platform data\n",
						__func__);
		return ERR_PTR(-ENOMEM);
	}

#ifdef CONFIG_ARCH_STI
	data->pdev = pdev;
	data->pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(data->pinctrl))
		return (void*)PTR_ERR(data->pinctrl);

	data->reset = devm_reset_control_get(&pdev->dev, NULL);
	if (IS_ERR(data->reset))
		return (void*)PTR_ERR(data->reset);
#else
	data->pad_config = stm_of_get_pad_config(&pdev->dev);
	data->sysconf = stm_of_sysconf_claim(np, "reset_ctrl");
#endif

	/*default value is 0 i.e. no correction needed on clock frequency
	 * for new SOCs*/
	if ((of_property_read_u32(np,
				"clk_err_correction",
				&data->clk_err_correction)))
		data->clk_err_correction = 0;

	cec_debug_trace(CEC_API,
			"CEC Clk correction:%d\n",
			data->clk_err_correction);

	/*default behaviour is TRUE i.e for new SOCs, ack logic for broadcast
	 * messages is inside the h/w */
	if (!(of_property_read_u32(np, "auto_ack_for_broadcast_tx", &of_data)))
		data->auto_ack_for_broadcast_tx = (uint8_t)of_data;
	else
		data->auto_ack_for_broadcast_tx = 1;

	cec_debug_trace(CEC_API,
			"CEC Auto ACK for broadcast messages:%d\n",
			data->auto_ack_for_broadcast_tx);

	return data;
}

static int __init cec_probe(struct platform_device * pdev)
{
	int	error = 0;
	struct cec_hw_data_s	*cec_hw_data_p ;
	struct stm_cec_platform_data *plat_data ;
	struct device *dev = &pdev->dev;
	struct resource		*r_irq;
	int id;

	if (pdev->dev.of_node) {
		plat_data = stm_cec_dt_get_pdata(pdev);
		id = of_alias_get_id(pdev->dev.of_node, "cec");
	} else{
		plat_data =
			(struct stm_cec_platform_data *)(dev->platform_data);
		id = pdev->id ;
	}

	if (!(plat_data) || IS_ERR(plat_data)) {
		printk(KERN_ALERT "No platform data found\n");
		return -ENODEV;
	}

	if (id < 0) {
		printk(KERN_ALERT "No ID specified or in DT alias\n");
		return -ENODEV;
	}

	error = cec_get_hw_container(id, &cec_hw_data_p);

	strlcpy(cec_hw_data_p->dev_name, pdev->name, CEC_DEV_NAME_SIZE);
	cec_hw_data_p->platform_data = plat_data;
	cec_hw_data_p->base_address = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	cec_hw_data_p->next_p = NULL;
#ifdef CONFIG_ARCH_STI
	cec_hw_data_p->clk =of_clk_get_by_name(pdev->dev.of_node, "cec_clk");
#else
	cec_hw_data_p->clk = clk_get(dev, "sbc_comms_clk");
#endif
	if (!cec_hw_data_p->clk) {
		printk(KERN_ALERT"%s: clk_get failed\n", __func__);
		return -1;
	}
	r_irq = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	cec_hw_data_p->r_irq = *r_irq;
	cec_hw_data_p->init = false;

	error = dev_set_drvdata(dev, cec_hw_data_p);
	if (error != 0) {
		printk(KERN_ALERT"Failed(%d) to set drv data for CEC\n", error);
		return error;
	}

	device_set_wakeup_capable(dev, true);

	pm_runtime_set_active(dev);
	pm_suspend_ignore_children(dev, 1);
	pm_runtime_enable(dev);



	cec_debug_trace(CEC_API, "base:%x Clk:%p Irq: %x\n",
			cec_hw_data_p->base_address->start,
			cec_hw_data_p->clk,
			cec_hw_data_p->r_irq.start);
	return 0;
}

static int cec_remove(struct platform_device *pdev)
{
	struct cec_hw_data_s    *cec_hw_data_p ;
	struct stm_cec_platform_data *data;
	int id;

	id = of_alias_get_id(pdev->dev.of_node, "cec");
	cec_get_hw_container(id, &cec_hw_data_p);
	data = cec_hw_data_p->platform_data;
#ifndef CONFIG_ARCH_STI
	sysconf_release(data->sysconf);
#endif
	pm_runtime_disable(&pdev->dev);

	return 0;
}

#ifdef CONFIG_PM

static int cec_stm_suspend(struct device *dev)
{
	struct cec_hw_data_s	*cec_hw_data_p;
	bool may_wakeup = false;

	cec_hw_data_p = dev_get_drvdata(dev);
	if (!cec_hw_data_p) {
		cec_error_trace(CEC_API,
			"Failed to get dev info\n");
		return -1;
	}
	cec_debug_trace(CEC_API, "CEC going into suspend mode\n");
	may_wakeup = device_may_wakeup(dev);
	cec_core_suspend(cec_hw_data_p, may_wakeup);
	return 0;
}

static int cec_stm_resume(struct device *dev)
{
	struct cec_hw_data_s	*cec_hw_data_p;

	cec_hw_data_p = dev_get_drvdata(dev);
	if (!cec_hw_data_p) {
		cec_error_trace(CEC_API,
			"Failed to get dev info\n");
		return -1;
	}
	cec_debug_trace(CEC_API, "Resuming CEC\n");

	cec_core_resume(cec_hw_data_p);
	return 0;
}

static const struct dev_pm_ops stm_cec_pm_ops = {
	.suspend = cec_stm_suspend,      /* on standby/memstandby */
	.resume =  cec_stm_resume,        /* resume from standby/memstandby */
	.freeze = cec_stm_suspend,        /* hibernation */
	.restore = cec_stm_resume,      /* resume from hibernation */
	.runtime_suspend = cec_stm_suspend,
	.runtime_resume = cec_stm_resume,
};

#endif

static struct of_device_id stm_cec_match[] = {
	{
		.compatible = "st,cec",
	},
	{},
};

static struct platform_driver __refdata stm_cec_driver = {
	.driver = {
		.name = "stm-cec",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(stm_cec_match),
#ifdef CONFIG_PM
		/*For power management*/
		.pm = &stm_cec_pm_ops,
#endif
	},
	.probe = cec_probe,
	.remove = cec_remove,
};

static int __init cec_module_init(void)
{
	printk(KERN_ALERT "Load module stm_cec by %s (pid %i)\n",
		current->comm,
		current->pid);
	cec_alloc_global_param();
	platform_driver_register(&stm_cec_driver);

	return 0;
}

static void __exit cec_module_exit(void)
{
	platform_driver_unregister(&stm_cec_driver);
	cec_dealloc_global_param();
	printk(KERN_ALERT "Unload module stm_cec by %s (pid %i)\n",
		current->comm,
		current->pid);
}

module_init(cec_module_init);
module_exit(cec_module_exit);
