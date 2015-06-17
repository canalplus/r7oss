#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/pm_runtime.h>
#include <linux/io.h>

#include <linux/delay.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include "device/device_dt.h"
#endif

#include "bdisp2/bdisp2_os.h"
#include "blit_debug.h"

#include "bdisp2/bdispII_aq.h"
#include "bdisp2/bdispII_fops.h"
#include "blitter_device.h"
#include "class.h"


/* provide some compatibility for older kernels */
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 3, 0)
static void __iomem *devm_request_and_ioremap(struct device *dev,
			struct resource *res)
{
	resource_size_t size;
	const char *name;
	void __iomem *dest_ptr;

	BUG_ON(!dev);

	if (!res || resource_type(res) != IORESOURCE_MEM) {
		dev_err(dev, "invalid resource\n");
		return NULL;
	}

	size = resource_size(res);
	name = res->name ?: dev_name(dev);

	if (!devm_request_mem_region(dev, res->start, size, name)) {
		dev_err(dev, "can't request region for resource %pR\n", res);
		return NULL;
	}

	if (res->flags & IORESOURCE_CACHEABLE)
		dest_ptr = devm_ioremap(dev, res->start, size);
	else
		dest_ptr = devm_ioremap_nocache(dev, res->start, size);

	if (!dest_ptr) {
		dev_err(dev, "ioremap failed for resource %pR\n", res);
		devm_release_mem_region(dev, res->start, size);
	}

	return dest_ptr;
}

struct clk_devres {
	struct clk *clk;
};

static void devm_clk_release(struct device *dev, void *res)
{
	struct clk_devres *dr = res;

	clk_put(dr->clk);
}

static struct clk *devm_clk_get(struct device *dev, const char *id)
{
	struct clk_devres *dr;
	struct clk *clk;

	dr = devres_alloc(devm_clk_release, sizeof(struct clk_devres),
			  GFP_KERNEL);
	if (!dr)
		return ERR_PTR(-ENOMEM);

	clk = clk_get(dev, id);
	if (IS_ERR(clk)) {
		devres_free(dr);
		return clk;
	}

	dr->clk = clk;
	devres_add(dev, dr);

	return clk;
}
#endif

static int __init
stm_blit_memory_request(struct platform_device  *pdev,
			const char              *mem_name,
			struct resource        **mem_region,
			void __iomem           **io_base)
{
	struct resource *resource;

	resource = platform_get_resource_byname(pdev,
						IORESOURCE_MEM, mem_name);
	*io_base = devm_request_and_ioremap(&pdev->dev, resource);
	if (!*io_base)
		return -ENODEV;

	*mem_region = resource;

	dev_dbg(&pdev->dev, "Base address is 0x%p\n", *io_base);

	return 0;
}

static int __init
stm_bdisp2_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct stm_plat_blit_data *plat_data;
	struct stm_bdisp2_config *bc;
	int  i;
	int  res;

	stm_blit_printd(0, "%s (pdev %p, dev %p)\n", __func__, pdev, dev);

#ifdef CONFIG_OF
	if (pdev->dev.of_node)
		stm_blit_dt_get_pdata(pdev);
#endif

	plat_data = (struct stm_plat_blit_data *)pdev->dev.platform_data;

	if (!plat_data) {
		res = -ENOMEM;
		goto out;
	}

	bc = devm_kzalloc(dev, sizeof(*bc), GFP_KERNEL);
	if (!bc) {
		stm_blit_printe(
			"Can't allocate memory for device description\n");
		res = -ENOMEM;
		goto out;
	}
	spin_lock_init(&bc->register_lock);


	res = stm_blit_memory_request(pdev, "bdisp-io",
				      &bc->mem_region, &bc->io_base);
	if (res)
		goto out;

	/* clock stuff */
	bc->clk_ip = devm_clk_get(dev, "bdisp0");
	if (IS_ERR(bc->clk_ip))
		bc->clk_ip = NULL;

	bc->clk_ip_freq = plat_data->clk_ip_freq;
	if (bc->clk_ip_freq) {
		res = clk_set_rate(bc->clk_ip, bc->clk_ip_freq);
		if (res)
			goto out;
	}

#ifndef CONFIG_ARCH_STI
	bc->clk_ic = devm_clk_get(dev, "bdisp0_icn");
	if (IS_ERR(bc->clk_ic))
		bc->clk_ic = NULL;
#endif

	for (i = 0; i < STM_BDISP2_MAX_AQs; ++i) {
		static const char * const irq_name[] = { "bdisp-aq1",
							 "bdisp-aq2",
							 "bdisp-aq3",
							 "bdisp-aq4" };
		struct resource * const resource =
			platform_get_resource_byname(pdev, IORESOURCE_IRQ,
						     irq_name[i]);
		if (!resource) {
			stm_blit_printi("AQ(%s) not found\n", irq_name[i]);
			continue;
		}

		stm_blit_printd(0, "AQ(%d) IRQ: %u\n", i, resource->start);

		res = stm_bdisp2_aq_init(&bc->aq_data[i], i,
					 resource->start,
					 plat_data,
					 bc);
		if (res < 0)
			goto out_interrupts;

#if defined(CONFIG_PM_RUNTIME)
		bc->aq_data[i].dev = &pdev->dev;
#endif

		stm_blitter_register_device(dev,
					    plat_data->device_name,
					    &bdisp2_aq_fops,
					    &bc->aq_data[i]);
	}

	platform_set_drvdata(pdev, bc);

	/* The device is active already (clocks are running, etc. - tell the PM
	   subsystem about this. */
	pm_runtime_set_active(&pdev->dev);
	/* allow the PM subsystem to call our callbacks */
	pm_runtime_enable(&pdev->dev);
	/* Now that we have probed - we can suspend until we have work */
	pm_runtime_suspend(&pdev->dev);
	/* Set the delay to suspend the system */
	pm_runtime_set_autosuspend_delay(&pdev->dev, 50);
	/* Activate the delay mechanism */
	pm_runtime_use_autosuspend(&pdev->dev);

	return 0;

out_interrupts:
	while (i--) {
		stm_blitter_unregister_device(&bc->aq_data[i]);
		stm_bdisp2_aq_release(&bc->aq_data[i]);
	}
out:
	return res;
}

static int __exit
stm_bdisp2_remove(struct platform_device *pdev)
{
	struct stm_bdisp2_config *bc;

	stm_blit_printd(0, "%s\n", __func__);

	/* this will cause us to be suspended */
	pm_runtime_put_sync(&pdev->dev);
	/* disallow PM callbacks */
	pm_runtime_disable(&pdev->dev);

	bc = platform_get_drvdata(pdev);
	if (bc != NULL) {
		int i;
		for (i = 0; i < STM_BDISP2_MAX_AQs; ++i) {
			struct stm_bdisp2_aq * const aq = &bc->aq_data[i];
			if (!aq->irq)
				continue;

			stm_blitter_unregister_device(aq);
			stm_bdisp2_aq_release(aq);
		}
	}

	platform_set_drvdata(pdev, NULL);

#ifdef CONFIG_OF
	if (pdev->dev.of_node)
		stm_blit_dt_put_pdata(pdev);
#endif

	return 0;
}

#if defined(CONFIG_PM)
static int stm_bdisp2_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct stm_bdisp2_config *bc;
	int retval = 0;

	stm_blit_printd(0, "%s\n", __func__);

#ifdef CONFIG_PM_RUNTIME
	if (pm_runtime_suspended(dev)) {
		stm_blit_printd(0, "%s - Already suspended by Runtime PM\n", __func__);
		return retval;
	}
#endif

	bc = platform_get_drvdata(pdev);
	if (bc != NULL) {
		int i;
		for (i = 0; i < STM_BDISP2_MAX_AQs; ++i) {
			struct stm_bdisp2_aq * const aq = &bc->aq_data[i];
			if (!aq->irq)
				continue;

			retval |= stm_bdisp2_aq_suspend(aq);
		}
	}

	return retval;
}

static int stm_bdisp2_rpm_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct stm_bdisp2_config *bc;
	int retval = 0;

	stm_blit_printd(0, "%s\n", __func__);

	if (pm_runtime_suspended(dev)) {
		stm_blit_printd(0, "%s - Already suspended by Runtime PM\n", __func__);
		return retval;
	}

	bc = platform_get_drvdata(pdev);
	if (bc != NULL) {
		int i;
		for (i = 0; i < STM_BDISP2_MAX_AQs; ++i) {
			struct stm_bdisp2_aq * const aq = &bc->aq_data[i];
			if (!aq->irq)
				continue;

			/* If blitter is still in use don't suspend, but update
			   last_busy so autosuspend is automatically rescheduled */
			if (bdisp2_is_idle(&aq->stdrv)) {
				retval |= stm_bdisp2_aq_suspend(aq);

				aq->stdrv.bdisp_shared->bdisp_suspended = 2;
				aq->stdrv.bdisp_shared->num_pending_requests--;
			} else {
				stm_blit_printd(0, "%s - Runtime PM suspend aborted, device still in use\n", __func__);

#if defined(CONFIG_PM_RUNTIME)
				/* Update to new last known busy time */
				pm_runtime_mark_last_busy(aq->dev);
#endif

				/* Return -EBUSY so that Runtime PM automatically reschedule a new suspend */
				retval |= -EBUSY;
			}
		}
	}

	return retval;
}

static int stm_bdisp2_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct stm_bdisp2_config *bc;
	int retval = 0;

	stm_blit_printd(0, "%s\n", __func__);

#ifdef CONFIG_PM_RUNTIME
	if (pm_runtime_suspended(dev)) {
		stm_blit_printd(0, "%s - Suspended by Runtime PM and should be waked by Runtime PM\n", __func__);
		return retval;
	}
#endif

	bc = platform_get_drvdata(pdev);
	if (bc != NULL) {
		int i;
		for (i = 0; i < STM_BDISP2_MAX_AQs; ++i) {
			struct stm_bdisp2_aq * const aq = &bc->aq_data[i];
			if (!aq->irq)
				continue;

			retval |= stm_bdisp2_aq_resume(aq);
		}
	}

	return retval;
}

static int stm_bdisp2_rpm_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct stm_bdisp2_config *bc;
	int retval = 0;

	stm_blit_printd(0, "%s\n", __func__);

	bc = platform_get_drvdata(pdev);
	if (bc != NULL) {
		int i;
		for (i = 0; i < STM_BDISP2_MAX_AQs; ++i) {
			struct stm_bdisp2_aq * const aq = &bc->aq_data[i];
			if (!aq->irq)
				continue;

			retval |= stm_bdisp2_aq_resume(aq);
		}
	}

	return retval;
}

#ifndef CONFIG_ARCH_STI
static int stm_bdisp2_freeze(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct stm_bdisp2_config *bc;
	int retval = 0;

	stm_blit_printd(0, "%s\n", __func__);

	bc = platform_get_drvdata(pdev);
	if (bc != NULL) {
		int i;
		for (i = 0; i < STM_BDISP2_MAX_AQs; ++i) {
			struct stm_bdisp2_aq * const aq = &bc->aq_data[i];
			if (!aq->irq)
				continue;

			retval |= stm_bdisp2_aq_freeze(aq);
		}
	}

	return retval;
}

static int stm_bdisp2_restore(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct stm_bdisp2_config *bc;
	int retval = 0;

	stm_blit_printd(0, "%s\n", __func__);

	bc = platform_get_drvdata(pdev);
	if (bc != NULL) {
		int i;
		for (i = 0; i < STM_BDISP2_MAX_AQs; ++i) {
			struct stm_bdisp2_aq * const aq = &bc->aq_data[i];
			if (!aq->irq)
				continue;

			retval |= stm_bdisp2_aq_restore(aq);
		}
	}

	return retval;
}
#endif /* CONFIG_ARCH_STI */

static const struct dev_pm_ops bdisp2_pm_ops = {
	.suspend         = stm_bdisp2_suspend,
	.resume          = stm_bdisp2_resume,
#ifndef CONFIG_ARCH_STI
	.freeze          = stm_bdisp2_freeze,
	.thaw            = stm_bdisp2_restore,
	.poweroff        = stm_bdisp2_freeze,
	.restore         = stm_bdisp2_restore,
#else
	.freeze          = stm_bdisp2_suspend,
	.thaw            = stm_bdisp2_resume,
	.poweroff        = stm_bdisp2_suspend,
	.restore         = stm_bdisp2_resume,
#endif
	.runtime_suspend = stm_bdisp2_rpm_suspend,
	.runtime_resume  = stm_bdisp2_rpm_resume,
	.runtime_idle    = NULL,
};

#define BDISP2_PM_OPS   (&bdisp2_pm_ops)
#else /* CONFIG_PM */
#define BDISP2_PM_OPS   NULL
#endif /* CONFIG_PM */

#ifdef CONFIG_OF
struct of_device_id stm_blitter_match[] = {
	{
	  .compatible = "st,bdispII-7109c3",
	  .data = (void *)STM_BLITTER_VERSION_7109c3
	},
	{
	  .compatible = "st,bdispII-7200c1",
	  .data = (void *)STM_BLITTER_VERSION_7200c1
	},
	{
	  .compatible = "st,bdispII-7200c2",
	  .data = (void *)STM_BLITTER_VERSION_7200c2_7111_7141_7105
	},
	{
	  .compatible = "st,bdispII-7106",
	  .data = (void *)STM_BLITTER_VERSION_7106_7108
	},
	{
	  .compatible = "st,bdispII-5206",
	  .data = (void *)STM_BLITTER_VERSION_5206
	},
	{
	  .compatible = "st,bdispII-fli75x0",
	  .data = (void *)STM_BLITTER_VERSION_FLI75x0
	},
	{
	  .compatible = "st,bdispII-h415",
	  .data = (void *)STM_BLITTER_VERSION_STiH415_FLI7610
	},
	{
	  .compatible = "st,bdispII-h416",
	  .data = (void *)STM_BLITTER_VERSION_STiH416
	},
	{
	  .compatible = "st,bdispII-h407",
	  .data = (void *)STM_BLITTER_VERSION_STiH407
	},
	{
	  .compatible = "st,bdispII-h205",
	  .data = (void *)STM_BLITTER_VERSION_STiH205
	},
	/* Default compatible */
	{
	  .compatible = "st,bdispII",
	  .data = (void *)STM_BLITTER_VERSION_7109c3
	},
	{},
};
#endif

static struct platform_driver __refdata stm_bdisp2_driver = {
	.driver.name = "stm-bdispII",
	.driver.owner = THIS_MODULE,
	.driver.pm = BDISP2_PM_OPS,
#ifdef CONFIG_OF
	.driver.of_match_table = of_match_ptr(stm_blitter_match),
#endif
	.probe = stm_bdisp2_probe,
	.remove = __exit_p(stm_bdisp2_remove),
};


#if defined(UNIFIED_DRIVER) && !defined(CONFIG_OF)
extern int __init stm_blit_glue_init(void);
extern void __exit stm_blit_glue_exit(void);
#endif

static int __init
stm_bdisp2_init(void)
{
	int res;

	stm_blit_printd(0, "%s\n", __func__);

	stm_blitter_class_init(STM_BLITTER_MAX_DEVICES);

#if defined(CONFIG_OF)
	/* This call is added here to allow setup of required
	   blitter counter for non unified loading */
	res = stm_blit_dt_init();
	if (res)
		return res;
#elif defined(UNIFIED_DRIVER)
	res = stm_blit_glue_init();
	if (res)
		return res;
#endif

	res = platform_driver_register(&stm_bdisp2_driver);

#if defined(CONFIG_OF)
	if (res)
		stm_blit_dt_exit();
#elif defined(UNIFIED_DRIVER)
	if (res)
		stm_blit_glue_exit();
#endif

	return res;
}

static void __exit
stm_bdisp2_exit(void)
{
	stm_blit_printd(0, "%s\n", __func__);

	platform_driver_unregister(&stm_bdisp2_driver);

#if defined(CONFIG_OF)
	stm_blit_dt_exit();
#elif defined(UNIFIED_DRIVER)
	stm_blit_glue_exit();
#endif

	stm_blitter_class_cleanup(STM_BLITTER_MAX_DEVICES);
}

MODULE_AUTHOR("André Draszik <andre.draszik@st.com");
MODULE_DESCRIPTION("STMicroelectronics BDispII driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(KBUILD_VERSION);

module_init(stm_bdisp2_init);
module_exit(stm_bdisp2_exit);
