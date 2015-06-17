/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/scaler/scaler_module.c
 * Copyright (c) 2000-2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/device.h>
#include <linux/stat.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/of.h>
#include <linux/sched.h>

#include <thread_vib_settings.h>
#include <stm_scaler.h>
#include <scaler_device_priv.h>

extern int  scaler_platform_device_register(void);
extern void scaler_platform_device_unregister(void);

int thread_vib_scaler[2]={ THREAD_VIB_SCALER_POLICY, THREAD_VIB_SCALER_PRIORITY };
module_param_array_named(thread_VIB_Scaler, thread_vib_scaler, int, NULL, 0644);
MODULE_PARM_DESC(thread_VIB_Scaler, "VIB-Scaler thread:s(Mode),p(Priority)");

#ifdef CONFIG_PM
/* With Power Management */
static int scaler_driver_suspend(struct device *dev)
{
    /* struct platform_device *pdev = to_platform_device(dev); */
    return 0;
}
static int scaler_driver_resume(struct device *dev)
{
    /* struct platform_device *pdev = to_platform_device(dev); */
    return 0;
}

const struct dev_pm_ops scaler_pm_ops = {
  .suspend         = scaler_driver_suspend,
  .resume          = scaler_driver_resume,
  .freeze          = scaler_driver_suspend,
  .thaw            = scaler_driver_resume,
  .poweroff        = scaler_driver_suspend,
  .restore         = scaler_driver_resume,
  .runtime_suspend = scaler_driver_suspend,
  .runtime_resume  = scaler_driver_resume,
  .runtime_idle    = NULL,
};

#define SCALER_PM_OPS   (&scaler_pm_ops)

#else

/* Without Power Management */
#define SCALER_PM_OPS   NULL

#endif

#ifndef CONFIG_ARCH_STI
int __init scaler_driver_probe(struct platform_device *pdev)
#else
static int scaler_driver_probe(struct platform_device *pdev)
#endif
{
    int            result;
    struct device *dev = &pdev->dev;

    printk(KERN_INFO ">scaler_driver_probe\n");

    /* Check device tree presence but no datas taken from it currently because */
    /* no hardware resources taken by scaler driver */
    if(pdev->dev.of_node)
    {
        printk(KERN_INFO "scaler_driver_probe device tree defined\n");
    }
    else
    {
        printk(KERN_INFO "scaler_driver_probe device tree not defined\n");
    }

    result = stm_scaler_device_open();
    if(result)
    {
      printk(KERN_INFO "scaler_module_init failed on stm_scaler_device_open()\n");
    }
    else
    {
        /* No platform device private data. */
        platform_set_drvdata(pdev, 0);

        pm_runtime_set_active(dev);
        pm_runtime_enable(dev);
        pm_runtime_get(dev);

        printk(KERN_INFO "<scaler_driver_probe\n");
    }

    return result;
}

int __exit scaler_driver_remove(struct platform_device *pdev)
{
    stm_scaler_device_close();

    /* No hardware resources to release by scaler driver */

    /*
     * Missing pm_runtime_disable call in driver remove path caused
     * an "Unbalanaced pm_runtime_enable" warning when driver is reloaded.
     */
    pm_runtime_disable(&pdev->dev);

    return 0;
}

static struct of_device_id stm_scaler_match[] = {
    {
        .compatible = "st,scaler",
    },
    {},
};

/* Commented for not loading it automatically at boot time */
/* MODULE_DEVICE_TABLE(of, stm_scaler_match); */

static struct platform_driver scaler_driver = {
        .driver = {
                .name           = "stm-scaler",
                .owner          = THIS_MODULE,
                .pm             = SCALER_PM_OPS,
                .of_match_table = of_match_ptr(stm_scaler_match),
        },
        .probe  = scaler_driver_probe,
        .remove = __exit_p (scaler_driver_remove),
};

/******************************************************************************
 *  Modularization
 */

static int __init scaler_module_init(void)
{
    printk(KERN_INFO "scaler_module_init\n");

    /* Get device number and device datas */
    if(scaler_platform_device_register())
        return -ENODEV;

    if(platform_driver_register(&scaler_driver))
    {
        printk(KERN_ERR "stm-scaler: Unable to register CoreDisplay platform driver");
        return -ENODEV;
    }

    return 0;
}

static void __exit scaler_module_exit(void)
{
    printk(KERN_INFO "scaler_module_exit\n");

    platform_driver_unregister(&scaler_driver);

    scaler_platform_device_unregister();
}

module_init(scaler_module_init);
module_exit(scaler_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("STMicroelectronics");
MODULE_DESCRIPTION("The scaler driver");
MODULE_VERSION(KBUILD_VERSION);

/******************************************************************************
 *  Exported symbols
 */

EXPORT_SYMBOL(stm_scaler_open);
EXPORT_SYMBOL(stm_scaler_close);
EXPORT_SYMBOL(stm_scaler_dispatch);
EXPORT_SYMBOL(stm_scaler_abort);
EXPORT_SYMBOL(stm_scaler_get_capabilities);



