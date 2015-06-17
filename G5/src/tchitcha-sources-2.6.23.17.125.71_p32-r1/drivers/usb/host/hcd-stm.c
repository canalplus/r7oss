/*
 * HCD (Host Controller Driver) for USB.
 *
 * Copyright (c) 2009 STMicroelectronics Limited
 * Author: Francesco Virlinzi
 *
 * Bus Glue for STMicroelectronics STx710x devices.
 *
 * This file is licenced under the GPL.
 */

#include <linux/platform_device.h>
#include <linux/stm/soc.h>
#include <linux/stm/pm.h>
#include <linux/delay.h>
#include <linux/usb.h>
#include "../core/hcd.h"
#include "./hcd-stm.h"

#undef dgb_print

#ifdef CONFIG_USB_DEBUG
#define dgb_print(fmt, args...)			\
		printk(KERN_INFO "%s: " fmt, __FUNCTION__ , ## args)
#else
#define dgb_print(fmt, args...)
#endif

static int st_usb_boot(struct platform_device *pdev)
{
	struct plat_usb_data *pl_data = pdev->dev.platform_data;
	struct drv_usb_data *usb_data = pdev->dev.driver_data;
	void *wrapper_base = usb_data->ahb2stbus_wrapper_glue_base;
	void *protocol_base = usb_data->ahb2stbus_protocol_base;
	unsigned long reg, req_reg;
	unsigned long flags = pl_data->flags;

	if (flags & (USB_FLAGS_STRAP_8BIT | USB_FLAGS_STRAP_16BIT)) {
		/* Set strap mode */
		reg = readl(wrapper_base + AHB2STBUS_STRAP_OFFSET);
		if (flags & USB_FLAGS_STRAP_16BIT)
			reg |= AHB2STBUS_STRAP_16_BIT;
		else
			reg &= ~AHB2STBUS_STRAP_16_BIT;
		writel(reg, wrapper_base + AHB2STBUS_STRAP_OFFSET);
	}

	if (flags & USB_FLAGS_STRAP_PLL) {
		/* Start PLL */
		reg = readl(wrapper_base + AHB2STBUS_STRAP_OFFSET);
		writel(reg | AHB2STBUS_STRAP_PLL,
			wrapper_base + AHB2STBUS_STRAP_OFFSET);
		mdelay(30);
		writel(reg & (~AHB2STBUS_STRAP_PLL),
			wrapper_base + AHB2STBUS_STRAP_OFFSET);
		mdelay(30);
	}

	if (flags & USB_FLAGS_OPC_MSGSIZE_CHUNKSIZE) {
		/* Set the STBus Opcode Config for load/store 32 */
		writel(AHB2STBUS_STBUS_OPC_32BIT,
			protocol_base + AHB2STBUS_STBUS_OPC_OFFSET);

		/* Set the Message Size Config to n packets per message */
		writel(AHB2STBUS_MSGSIZE_4,
			protocol_base + AHB2STBUS_MSGSIZE_OFFSET);

		/* Set the chunksize to n packets */
		writel(AHB2STBUS_CHUNKSIZE_4,
			protocol_base + AHB2STBUS_CHUNKSIZE_OFFSET);
	}

	if (flags & USB_FLAGS_STBUS_CONFIG_THRESHOLD_MASK) {
		req_reg = (1 << 21) |  /* Turn on read-ahead */
			  (0 << 15) |  /* Turn off write posting */
			  (1 << 14) |  /* Enable threshold */
			  (0 << 4)  ;  /* No messages */

		switch (flags & USB_FLAGS_STBUS_CONFIG_THRESHOLD_MASK) {
		case USB_FLAGS_STBUS_CONFIG_THRESHOLD_64:
			req_reg |= 6 << 0; /* 2^6 = 64 */
			break;
		case USB_FLAGS_STBUS_CONFIG_THRESHOLD_128:
			req_reg |= 7 << 0; /* 2^7 = 128 */
			break;
		case USB_FLAGS_STBUS_CONFIG_THRESHOLD_256:
			req_reg |= 8 << 0; /* 2^8 = 256 */
			break;
		default:
			BUG();
			break;
		}

		switch (flags & USB_FLAGS_STBUS_CONFIG_PKTS_PER_CHUNK_MASK) {
		case USB_FLAGS_STBUS_CONFIG_PKTS_PER_CHUNK_1:
			req_reg |= 0 << 9; /* 2^0 = 1 packet in a chunk */
			break;
		case USB_FLAGS_STBUS_CONFIG_PKTS_PER_CHUNK_2:
			req_reg |= 1 << 9; /* 2^1 = 2 packets in a chunk */
			break;
		case USB_FLAGS_STBUS_CONFIG_PKTS_PER_CHUNK_8:
			req_reg |= 3 << 9; /* 2^3 = 8 packets in a chunk */
			break;
		default:
			BUG();
			break;
		}

		switch (flags & USB_FLAGS_STBUS_CONFIG_OPCODE_MASK) {
		case USB_FLAGS_STBUS_CONFIG_OPCODE_LD32_ST32:
			req_reg |= 5 << 16; /* Opcode is store/load 32 */
			break;
		case USB_FLAGS_STBUS_CONFIG_OPCODE_LD64_ST64:
			req_reg |= 6 << 16; /* Opcode is store/load 64 */
			break;
		default:
			BUG();
			break;
		}

		do {
			writel(req_reg, protocol_base +
				AHB2STBUS_MSGSIZE_OFFSET);
			reg = readl(protocol_base + AHB2STBUS_MSGSIZE_OFFSET);
		} while ((reg & 0x7FFFFFFF) != req_reg);
	}
	return 0;
}

static int st_usb_remove(struct platform_device *pdev)
{
	struct resource *res;
	struct device *dev = &pdev->dev;
	struct drv_usb_data *dr_data = platform_get_drvdata(pdev);

	platform_pm_pwdn_req(pdev, HOST_PM | PHY_PM, 0);
	platform_pm_pwdn_ack(pdev, HOST_PM | PHY_PM, 0);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "wrapper");
	BUG_ON(!res);
	devm_release_mem_region(res->start, res->end - res->start);
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "protocol");
	BUG_ON(!res);
	devm_release_mem_region(res->start, res->end - res->start);

	if (dr_data->ehci_device)
		platform_device_unregister(dr_data->ehci_device);
	if (dr_data->ohci_device)
		platform_device_unregister(dr_data->ohci_device);

	return 0;
}

/*
 * Slightly modified version of platform_device_register_simple()
 * which assigns parent and has no resources.
 */
static struct platform_device
*stm_usb_device_create(const char *name, int id, struct platform_device *parent)
{
	struct platform_device *pdev;
	int retval;

	pdev = platform_device_alloc(name, id);
	if (!pdev) {
		retval = -ENOMEM;
		goto error;
	}

	pdev->dev.parent = &parent->dev;
	pdev->dev.dma_mask = parent->dev.dma_mask;

	retval = platform_device_add(pdev);
	if (retval)
		goto error;

	return pdev;

error:
	platform_device_put(pdev);
	return ERR_PTR(retval);
}

static int st_usb_probe(struct platform_device *pdev)
{
	struct drv_usb_data *dr_data;
	struct device *dev = &pdev->dev;
	struct resource *res;
	int ret = 0;

	dgb_print("\n");
	/* Power on */
	platform_pm_pwdn_req(pdev, HOST_PM | PHY_PM, 0);
	/* Wait the ack */
	platform_pm_pwdn_ack(pdev, HOST_PM | PHY_PM, 0);

	dr_data = kzalloc(sizeof(struct drv_usb_data), GFP_KERNEL);
	if (!dr_data)
		return -ENOMEM;

	platform_set_drvdata(pdev, dr_data);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "wrapper");
	if (!res) {
		ret = -ENXIO;
		goto err_0;
	}
	if (devm_request_mem_region(dev, res->start,
		res->end - res->start, pdev->name) < 0) {
		ret = -EBUSY;
		goto err_0;
	}
	dr_data->ahb2stbus_wrapper_glue_base =
		devm_ioremap_nocache(dev, res->start, res->end - res->start);

	if (!dr_data->ahb2stbus_wrapper_glue_base) {
		ret = -EFAULT;
		goto err_1;
	}
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "protocol");
	if (!res) {
		ret = -ENXIO;
		goto err_2;
	}
	if (devm_request_mem_region(dev, res->start, res->end - res->start,
		pdev->name) < 0) {
		ret = -EBUSY;
		goto err_2;
	}
	dr_data->ahb2stbus_protocol_base =
		devm_ioremap_nocache(dev, res->start, res->end - res->start);

	if (!dr_data->ahb2stbus_protocol_base) {
		ret = -EFAULT;
		goto err_3;
	}
	st_usb_boot(pdev);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ehci");
	if (res) {
		dr_data->ehci_device = stm_usb_device_create("stm-ehci",
			pdev->id, pdev);
		if (IS_ERR(dr_data->ehci_device)) {
			ret = (int)dr_data->ehci_device;
			goto err_4;
		}
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "ohci");
	if (res) {
		dr_data->ohci_device =
			stm_usb_device_create("stm-ohci", pdev->id, pdev);
		if (IS_ERR(dr_data->ohci_device)) {
			if (dr_data->ehci_device)
				platform_device_del(dr_data->ehci_device);
			ret = (int)dr_data->ohci_device;
			goto err_4;
		}
	}
	return ret;

err_4:
	devm_iounmap(dev, dr_data->ahb2stbus_protocol_base);
err_3:
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "protocol");
	BUG_ON(!res);
	devm_release_mem_region(res->start, res->end - res->start);
err_2:
	devm_iounmap(dev, dr_data->ahb2stbus_wrapper_glue_base);
err_1:
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "wrapper");
	BUG_ON(!res);
	devm_release_mem_region(res->start, res->end - res->start);
err_0:
	kfree(dr_data);
	return ret;
}

static void st_usb_shutdown(struct platform_device *pdev)
{
	dgb_print("\n");
	platform_pm_pwdn_req(pdev, HOST_PM | PHY_PM, 1);
}

#ifdef CONFIG_PM
static int st_usb_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct drv_usb_data *dr_data = platform_get_drvdata(pdev);
	struct plat_usb_data *pl_data = pdev->dev.platform_data;
	void *wrapper_base = dr_data->ahb2stbus_wrapper_glue_base;
	void *protocol_base = dr_data->ahb2stbus_protocol_base;
	long reg;
	dgb_print("\n");

	if (pl_data->flags & USB_FLAGS_STRAP_PLL) {
		/* PLL turned off */
		reg = readl(wrapper_base + AHB2STBUS_STRAP_OFFSET);
		writel(reg | AHB2STBUS_STRAP_PLL,
			wrapper_base + AHB2STBUS_STRAP_OFFSET);
	}

	writel(0, wrapper_base + AHB2STBUS_STRAP_OFFSET);
	writel(0, protocol_base + AHB2STBUS_STBUS_OPC_OFFSET);
	writel(0, protocol_base + AHB2STBUS_MSGSIZE_OFFSET);
	writel(0, protocol_base + AHB2STBUS_CHUNKSIZE_OFFSET);
	writel(0, protocol_base + AHB2STBUS_MSGSIZE_OFFSET);

	writel(1, protocol_base + AHB2STBUS_SW_RESET);
	mdelay(10);
	writel(0, protocol_base + AHB2STBUS_SW_RESET);

	platform_pm_pwdn_req(pdev, HOST_PM | PHY_PM, 1);
	platform_pm_pwdn_ack(pdev, HOST_PM | PHY_PM, 1);
	return 0;
}
static int st_usb_resume(struct platform_device *pdev)
{
	dgb_print("\n");
	platform_pm_pwdn_req(pdev, HOST_PM | PHY_PM, 0);
	platform_pm_pwdn_ack(pdev, HOST_PM | PHY_PM, 0);
	st_usb_boot(pdev);
	return 0;
}
#else
#define st_usb_suspend	NULL
#define st_usb_resume	NULL
#endif

static struct platform_driver st_usb_driver = {
	.driver.name = "st-usb",
	.driver.owner = THIS_MODULE,
	.probe = st_usb_probe,
	.shutdown = st_usb_shutdown,
	.suspend = st_usb_suspend,
	.resume = st_usb_resume,
	.remove = st_usb_remove,
};

static int __init st_usb_init(void)
{
	return platform_driver_register(&st_usb_driver);
}

static void __exit st_usb_exit(void)
{
	platform_driver_unregister(&st_usb_driver);
}

MODULE_LICENSE("GPL");

module_init(st_usb_init);
module_exit(st_usb_exit);
