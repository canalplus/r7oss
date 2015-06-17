/************************************************************************
 * Copyright (C) 2011 STMicroelectronics. All Rights Reserved.
 *
 * This file is part of the stm_infra Library.
 *
 * stm_fe is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation.
 *
 * stm_fe is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with stm_infra; see the file COPYING.  If not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * The stm_infra Library may alternatively be licensed under a proprietary
 * license from ST.
 *
 * Source file name : stm_power.c
 * Author : Payal Singla
 *
 * API dedinitions for demodulation device
 *
 * Date        Modification                                    Name
 * ----        ------------                                    --------
 * 17 Sep 2012	generic code modifications			bharatj
 *
 *  ************************************************************************/

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/init.h>    /* Initilisation support */
#include <linux/kernel.h>  /* Kernel support */
#include <linux/clk.h>
#ifndef CONFIG_ARCH_STI
#include <linux/stm/platform.h>
#endif
#include <linux/pm_runtime.h>
#include <linux/firmware.h>
#include <linux/libelf.h>
#include <linux/of.h>

#include "infra_os_wrapper.h"
#include "infra_platform.h"

MODULE_AUTHOR("");
MODULE_DESCRIPTION("STMicroelectronics LXCPU driver");
MODULE_LICENSE("GPL");

#define POWER_DEBUG  0
#define POWER_ERROR  1

#if POWER_ERROR
#define power_error_print(fmt, ...)         pr_err(fmt,  ##__VA_ARGS__)
#else
#define power_error_print(fmt, ...)         while(0)
#endif

#if POWER_DEBUG
#define power_debug_print(fmt, ...)         pr_info(fmt,  ##__VA_ARGS__)
#else
#define power_debug_print(fmt, ...)         while(0)
#endif

#define stm_lx_get_clock_id(id, buf) \
				{ \
				  sprintf(buf, "lx_cpu_clk_%d", id); \
				}

static int stm_lx_cpu_suspend(struct device *dev);
static int stm_lx_cpu_resume(struct device *dev);
static int stm_lx_cpu_probe(struct platform_device *stm_lx_cpu_data);

static struct dev_pm_ops stm_lx_cpu_pm_ops = {
        .suspend = stm_lx_cpu_suspend,
        .resume = stm_lx_cpu_resume,
        .runtime_suspend = stm_lx_cpu_suspend,
        .runtime_resume = stm_lx_cpu_resume,
};

static int stm_lx_cpu_remove(struct platform_device *pdev)
{
	pm_runtime_disable(&pdev->dev);
	pm_runtime_put(&pdev->dev);

	/* suspend device before removing it
	 * So that clock user can be set to zero
	 * when this module is not present
	 */
	stm_lx_cpu_suspend(&pdev->dev);
	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id stm_lx_match[] = {
	{
		.compatible = "st,stm-lx-cpu",
	},
	{},
};
#endif

static struct platform_driver stm_lx_cpu_driver = {
        .driver ={
                .name = "stm-lx-cpu",
                .owner = THIS_MODULE,
                .pm = &stm_lx_cpu_pm_ops,
		.of_match_table = of_match_ptr(stm_lx_match),
        },
	.probe = stm_lx_cpu_probe,
	.remove = stm_lx_cpu_remove,
};

static int stm_lx_cpu_suspend(struct device *dev)
{
	struct clk *clk_p;
	int count;

	struct platform_device * stm_lx_cpu_data = dev->platform_data;
	power_debug_print(" <%s>:: <%d> Suspending LX CPU %s\n",
		__FUNCTION__,__LINE__,dev->kobj.name);

	for(count = 0 ; count < stm_lx_cpu_data->num_resources; count++ )
	{
		char buf[15];
		stm_lx_get_clock_id(count,buf);
		clk_p = clk_get(dev, buf);
		if (IS_ERR(clk_p)) {
			power_error_print("<%s>:: <%d> LX %s could not be suspended,\
				Clock %s not found\n",
				__FUNCTION__,__LINE__,dev->kobj.name,
				buf);
				return -ENXIO;
		}
		clk_disable(clk_p);
	}

	return 0;
}

static int stm_lx_cpu_resume(struct device *dev)
{
	struct clk *clk_p;
	int count;
	struct platform_device * stm_lx_cpu_data = dev->platform_data;
	power_debug_print(" <%s>:: <%d> Resuming LX CPU %s\n",
		__FUNCTION__,__LINE__,dev->kobj.name);

	for(count = 0 ; count < stm_lx_cpu_data->num_resources; count++ )
	{
		char buf[15];
		stm_lx_get_clock_id(count, buf);
		clk_p = clk_get(dev, buf);
		if (IS_ERR(clk_p)) {
			power_error_print("<%s>:: <%d> LX %s could not be resumed,\
				Clock %s not found\n",
				__FUNCTION__,__LINE__,dev->kobj.name,
				 buf);
				 return -ENXIO;
		}
		clk_enable(clk_p);
	}

	return 0;
}
#ifdef CONFIG_OF
static struct platform_device *
stm_lx_dt_get_pdata(struct platform_device *pdev)
{
	int count = 0;

	pdev->num_resources = of_property_count_strings(pdev->dev.of_node,
					"clock-names");
	for(count = 0; count < pdev->num_resources; count ++)
	{
		const char *clock_name;
		char buf[15];
		of_property_read_string_index(pdev->dev.of_node,
					"clock-names", count,
					&clock_name);
		power_debug_print(" <%s> :: %d clock name %s \n",
				 __FUNCTION__,__LINE__,clock_name);
		stm_lx_get_clock_id(count, buf);
		clk_add_alias(buf,pdev->dev.kobj.name,(char*)clock_name,NULL);
	}
	return pdev;
}
#else
static struct platform_device *
stm_lx_dt_get_pdata(struct platform_device *pdev)
{
        return NULL;
}
#endif

static int stm_lx_cpu_probe(struct platform_device *stm_lx_cpu_data)
{
	if (stm_lx_cpu_data->dev.of_node)
		stm_lx_cpu_data->dev.platform_data  = stm_lx_dt_get_pdata(stm_lx_cpu_data);
	else
		stm_lx_cpu_data->dev.platform_data = stm_lx_cpu_data;

	pm_runtime_set_active(&stm_lx_cpu_data->dev);
	pm_suspend_ignore_children(&stm_lx_cpu_data->dev, 1);
	pm_runtime_enable(&stm_lx_cpu_data->dev);
	pm_runtime_get(&stm_lx_cpu_data->dev);
	/*
	 * Since device is set to active state by calling pm_runtime_get().
	 * So we need to enable the device used by this device i.e. the clocks.
	 * Since the LX clocks are just enabled by the target pack ,
	 * so the LX clock user count is zero.
	 * Hence we need to increment it to one by enabling the clocks
	 * else suspend/resume functions will never be called.
	 */
	stm_lx_cpu_resume(&stm_lx_cpu_data->dev);
        return 0;
}


static int __init lx_cpu_init_module(void)
{

	power_debug_print("Load module stm_lx_cpu by %s (pid %i)\n",
			current->comm, current->pid);
#ifndef CONFIG_OF
	arch_modprobe_func();
#endif
	platform_driver_register(&stm_lx_cpu_driver);

	return 0;
}

static void __exit lx_cpu_exit_module(void)
{
	platform_driver_unregister(&stm_lx_cpu_driver);

	power_debug_print("Unload module stm_lx_cpu by %s (pid %i)\n",
				current->comm, current->pid);
}

module_init(lx_cpu_init_module);
module_exit(lx_cpu_exit_module);
