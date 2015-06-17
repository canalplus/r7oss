/*
 * c8jpg.c, a driver for the C8JPG1 JPEG decoder IP
 *
 * Copyright (C) 2012-2013, STMicroelectronics R&D
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 */

#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>
#include <linux/debugfs.h>

#include <media/videobuf2-core.h>
#include <media/videobuf2-bpa2-contig.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#endif

#include "c8jpg.h"
#include "c8jpg-drv.h"

static int stm_c8jpg_probe(struct platform_device *pdev)
{
	struct stm_c8jpg_device *dev;
	struct resource *irq_resource, *mem_resource;

	int ret = 0;

	dev = devm_kzalloc(&pdev->dev,
			   sizeof(struct stm_c8jpg_device), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	spin_lock_init(&dev->lock);
	mutex_init(&dev->mutex_lock);

	dev->dev = &pdev->dev;

	irq_resource =
		platform_get_resource_byname(pdev, IORESOURCE_IRQ, "c8jpg-int");
	if (!irq_resource) {
		ret = -ENODEV;
		goto fini_get_resource;
	}

	mem_resource =
		platform_get_resource_byname(pdev, IORESOURCE_MEM, "c8jpg-io");
	if (!mem_resource) {
		ret = -ENODEV;
		goto fini_get_resource;
	}

#ifndef CONFIG_MACH_STM_VIRTUAL_PLATFORM
	dev->clk_ip = devm_clk_get(&pdev->dev, "c8jpg_dec");

	if (IS_ERR(dev->clk_ip)) {
	    ret = -ENODEV;
	    goto fini_get_resource;
	}

	clk_prepare_enable(dev->clk_ip);
#ifndef CONFIG_ARCH_STI
	dev->clk_ic = devm_clk_get(&pdev->dev, "c8jpg_icn");

	if (IS_ERR(dev->clk_ic)) {
	    ret = -ENODEV;
	    goto fini_get_resource;
	}

	clk_prepare_enable(dev->clk_ic);
#endif
#endif
	if (!devm_request_mem_region(&pdev->dev, mem_resource->start,
				     0x1000, pdev->name)) {
		dev_err(&pdev->dev, "couldn't request io region\n");
		ret = -ENOMEM;
		goto fini_get_resource;
	}

	dev->iomem =
		devm_ioremap_nocache(&pdev->dev, mem_resource->start, 0x1000);
	if (!dev->iomem) {
		ret = -EAGAIN;
		goto fini_get_resource;
	}

	dev_info(&pdev->dev, "found c8jpg device at: 0x%08lx, irq: %d\n",
		 (unsigned long)mem_resource->start, (int)irq_resource->start);

	dev->m2mdev = v4l2_m2m_init(&c8jpg_v4l2_m2m_ops);
	if (!dev->m2mdev) {
		ret = -EAGAIN;
		goto fini_request_ioremap;
	}

	ret = v4l2_device_register(&pdev->dev, &dev->v4l2dev);
	if (ret)
		goto fini_v4l2_m2m_init;

	dev_dbg(&pdev->dev, "registered a new v4l2 device\n");

	strlcpy(dev->videodev.name, "jpeg0", sizeof(dev->videodev.name));
	dev->videodev.v4l2_dev = &dev->v4l2dev;
	dev->videodev.fops = &stm_c8jpg_file_ops;
	dev->videodev.ioctl_ops = &stm_c8jpg_vidioc_ioctls_ops;
	dev->videodev.release = video_device_release_empty;
	dev->videodev.lock = &dev->mutex_lock;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 7, 0))
        dev->videodev.vfl_dir = VFL_DIR_M2M;
#endif

	ret = video_register_device(&dev->videodev, VFL_TYPE_GRABBER, -1);
	if (ret < 0) {
		dev_dbg(&pdev->dev, "could not register the video device!\n");
		goto fini_v4l2_device_register;
	}

	video_set_drvdata(&dev->videodev, dev);
	platform_set_drvdata(pdev, dev);

	dev->irq = irq_resource->start;

	ret = devm_request_irq(&pdev->dev, dev->irq, c8jpg_irq_handler,
			       0, "stm-c8jpg", (void *)dev);
	if (ret)
		goto fini_video_register_device;

	/* vb2_bpa2_contig own allocation context */
	dev->vb2_alloc_ctx =
		vb2_bpa2_contig_init_ctx(&pdev->dev,
					 bpa2_find_part("v4l2-stmc8jpg"));
	if (IS_ERR(dev->vb2_alloc_ctx)) {
		ret = PTR_ERR(dev->vb2_alloc_ctx);
		goto fini_device_irq;
	}

#ifdef CONFIG_DEBUG_FS
	dev->debugfs = debugfs_create_dir("stm_c8jpg", NULL);
	if (IS_ERR(dev->debugfs))
		goto fini_device_irq;

	debugfs_create_u32("frames", S_IRUGO,
			   dev->debugfs, &dev->nframes);
#ifdef CONFIG_VIDEO_STM_C8JPG_DEBUG
	debugfs_create_u32("luma_crc32c", S_IRUGO,
			   dev->debugfs, &dev->luma_crc32c);
	debugfs_create_u32("chroma_crc32c", S_IRUGO,
			   dev->debugfs, &dev->chroma_crc32c);
	debugfs_create_u32("decode_time", S_IRUGO,
			   dev->debugfs, (u32*)&dev->hwdecode_time.tv_usec);
#endif
#endif

	dev_info(&pdev->dev, "decoder successfully probed\n");

	pm_runtime_enable(&pdev->dev);
	pm_runtime_resume(&pdev->dev);

	/* clocks disable for now */
	c8jpg_hw_reset(dev);

	pm_runtime_suspend(&pdev->dev);

	return ret;

fini_device_irq:
	devm_free_irq(&pdev->dev, dev->irq, NULL);
fini_video_register_device:
	video_unregister_device(&dev->videodev);
fini_v4l2_device_register:
	v4l2_device_unregister(&dev->v4l2dev);
fini_v4l2_m2m_init:
	v4l2_m2m_release(dev->m2mdev);

	platform_set_drvdata(pdev, NULL);
fini_request_ioremap:
	devm_iounmap(&pdev->dev, dev->iomem);
fini_get_resource:
#ifndef CONFIG_MACH_STM_VIRTUAL_PLATFORM
	if (!IS_ERR(dev->clk_ip))
		clk_disable_unprepare(dev->clk_ip);
#ifndef CONFIG_ARCH_STI
	if (!IS_ERR(dev->clk_ic))
		clk_disable_unprepare(dev->clk_ic);
#endif
#endif
	devm_kfree(&pdev->dev, dev);

	return ret;
}

static int stm_c8jpg_remove(struct platform_device *pdev)
{
	struct stm_c8jpg_device *dev = NULL;

	dev = platform_get_drvdata(pdev);
	BUG_ON(!dev);

#ifdef CONFIG_DEBUG_FS
	debugfs_remove_recursive(dev->debugfs);
#endif
	pm_runtime_disable(&pdev->dev);

	video_unregister_device(&dev->videodev);

	v4l2_device_unregister(&dev->v4l2dev);

	v4l2_m2m_release(dev->m2mdev);
	vb2_bpa2_contig_cleanup_ctx(dev->vb2_alloc_ctx);

#ifndef CONFIG_MACH_STM_VIRTUAL_PLATFORM
	clk_disable_unprepare(dev->clk_ip);
#ifndef CONFIG_ARCH_STI
	clk_disable_unprepare(dev->clk_ic);
#endif
#endif

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static int stm_c8jpg_runtime_suspend(struct device *dev)
{
	struct stm_c8jpg_device *cdev = dev_get_drvdata(dev);

	clk_disable_unprepare(cdev->clk_ip);
#ifndef CONFIG_ARCH_STI
	clk_disable_unprepare(cdev->clk_ic);
#endif

	return 0;
}

static int stm_c8jpg_runtime_resume(struct device *dev)
{
	struct stm_c8jpg_device *cdev = dev_get_drvdata(dev);

	clk_prepare_enable(cdev->clk_ip);
#ifndef CONFIG_ARCH_STI
	clk_prepare_enable(cdev->clk_ic);
#endif
	return 0;
}

static const struct dev_pm_ops stm_c8jpg_pm_ops = {
	.prepare = stm_c8jpg_v4l2_pm_prepare,
	.runtime_suspend = stm_c8jpg_runtime_suspend,
	.runtime_resume = stm_c8jpg_runtime_resume
};

#ifdef CONFIG_OF
struct of_device_id stm_c8jpg_match[] = {
	{ .compatible = "st,c8jpg" },
	{}
};
#endif

struct platform_driver stm_c8jpg_driver = {
	.driver.name = "stm-c8jpg",
	.driver.owner = THIS_MODULE,
	.driver.pm = &stm_c8jpg_pm_ops,
#ifdef CONFIG_OF
	.driver.of_match_table = of_match_ptr(stm_c8jpg_match),
#endif
	.probe = stm_c8jpg_probe,
	.remove = __exit_p(stm_c8jpg_remove)
};

static int __init stm_c8jpg_init(void)
{
	int ret;

	ret = platform_driver_register(&stm_c8jpg_driver);
	if (ret)
		return ret;

#ifndef CONFIG_OF
	ret = stm_c8jpg_device_init();
	if (ret)
		platform_driver_unregister(&stm_c8jpg_driver);
#endif

	return ret;
}

static void __exit stm_c8jpg_exit(void)
{
#ifndef CONFIG_OF
	stm_c8jpg_device_exit();
#endif

	platform_driver_unregister(&stm_c8jpg_driver);
}

MODULE_AUTHOR("Ilyes Gouta <ilyes.gouta@st.com>");
MODULE_AUTHOR("Aymen Abderrahmen <aymen.abderrahmen@st.com>");
MODULE_DESCRIPTION("STMicroelectronics C8JPG v4l2 device driver");
MODULE_LICENSE("GPL");

module_init(stm_c8jpg_init);
module_exit(stm_c8jpg_exit);
