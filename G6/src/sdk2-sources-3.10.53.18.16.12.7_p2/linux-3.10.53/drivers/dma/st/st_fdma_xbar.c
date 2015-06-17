/*
 * STMicroelectronics FDMA cross bar driver
 *
 * Copyright (c) 2012 STMicroelectronics Limited
 *
 * Author: John Boddie <john.boddie@st.com>
 *
 * This code is basically a copy of drivers/stm/fdma-xbar.c!
 *
 * There has got to be a better way of doing this...
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>

#include <linux/platform_data/dma-st-fdma.h>
#include "st_fdma.h"


/*
 * Device type
 */

struct st_fdma_xbar {
	void __iomem *io_base;

	struct st_fdma_dreq_router router;

	int first_fdma_id;
	int last_fdma_id;
};


/*
 * Routing functions
 */

static int st_fdma_xbar_route(struct st_fdma_dreq_router *router,
		int input_req_line, int fdma, int fdma_req_line)
{
	struct st_fdma_xbar *xbar = container_of(router, struct st_fdma_xbar,
						router);
	int output_line;

	/* Calculate the output line for the FDMA */
	fdma = fdma - xbar->first_fdma_id;
	output_line = (fdma * ST_FDMA_NUM_DREQS) + fdma_req_line;

	/* Write to the xbar */
	writel(input_req_line, xbar->io_base + (output_line * 4));

	return 0;
}


/*
 * Platform driver initialise
 */

static int st_fdma_xbar_probe(struct platform_device *pdev)
{
	struct st_fdma_xbar *xbar;
	struct resource *iores;
	int result;

	/* Allocate xbar device structure */
	xbar = devm_kzalloc(&pdev->dev, sizeof(*xbar), GFP_KERNEL);
	if (!xbar) {
		dev_err(&pdev->dev, "Failed to allocate device structure\n");
		return -ENOMEM;
	}

	/* Request the xbar memory region */
	iores = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!iores) {
		dev_err(&pdev->dev, "Failed to get memory resource\n");
		return -ENXIO;
	}

	/* Request the xbar memory region and remap into uncached memory */
	xbar->io_base = devm_request_and_ioremap(&pdev->dev, iores);
	if (!xbar->io_base) {
		dev_err(&pdev->dev, "Failed to ioremap memory region\n");
		return -EBUSY;
	}

	xbar->router.route = st_fdma_xbar_route;
	xbar->router.xbar_id = pdev->id;
	if (pdev->dev.of_node)
		xbar->router.xbar_id = of_alias_get_id(pdev->dev.of_node,
								"fdmaxbar");

	/* Does config specify first and last fdma id that uses this xbar? */
	if (pdev->dev.platform_data) {
		struct st_plat_fdma_xbar_data *plat_data;

		plat_data = pdev->dev.platform_data;

		xbar->first_fdma_id = plat_data->first_fdma_id;
		xbar->last_fdma_id = plat_data->last_fdma_id;
	} else if (pdev->dev.of_node) {
		of_property_read_u32(pdev->dev.of_node, "first-fdma",
					 &xbar->first_fdma_id);
		of_property_read_u32(pdev->dev.of_node, "last-fdma",
					 &xbar->last_fdma_id);
	}

	/* An ID of -1 means there is only one xbar, designate it as 0 */
	if (xbar->router.xbar_id == (u8)-1)
		xbar->router.xbar_id = 0;

	/* Register the dreq router with the FDMA */
	result = st_fdma_register_dreq_router(&xbar->router);
	if (result) {
		dev_err(&pdev->dev, "Failed to register dreq router\n");
		return result;
	}

	/* Associate this xbar with platform device */
	platform_set_drvdata(pdev, xbar);

	return 0;
}

static int st_fdma_xbar_remove(struct platform_device *pdev)
{
	struct st_fdma_xbar *xbar = platform_get_drvdata(pdev);

	/* Clear the platform driver data */
	platform_set_drvdata(pdev, NULL);

	/* Unregister the dreq router from the FDMA */
	st_fdma_unregister_dreq_router(&xbar->router);

	return 0;
}


#ifdef CONFIG_OF
static struct of_device_id st_fdma_xbar_match[] = {
	{
		.compatible = "st,fdma-xbar",
	},
	{},
};

MODULE_DEVICE_TABLE(of, st_fdma_xbar_match);
#endif
/*
 * Module initialisation
 */

static struct platform_driver st_fdma_xbar_platform_driver = {
	.driver.name	= "stm-fdma-xbar",
	.driver.of_match_table = of_match_ptr(st_fdma_xbar_match),
	.probe		= st_fdma_xbar_probe,
	.remove		= st_fdma_xbar_remove,
};

static int st_fdma_xbar_init(void)
{
	return platform_driver_register(&st_fdma_xbar_platform_driver);
}

static void st_fdma_xbar_exit(void)
{
	platform_driver_unregister(&st_fdma_xbar_platform_driver);
}

MODULE_AUTHOR("John Boddie <john.boddie@st.com>");
MODULE_DESCRIPTION("STMicroelectronics FDMA cross bar driver");
MODULE_LICENSE("GPL");

module_init(st_fdma_xbar_init);
module_exit(st_fdma_xbar_exit);
