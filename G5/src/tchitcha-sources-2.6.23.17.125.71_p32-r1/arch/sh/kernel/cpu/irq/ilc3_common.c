/*
 * Copyright (C) 2007 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

#include <linux/kernel.h>
#include <linux/platform_device.h>

#include <asm/io.h>

#define DRIVER_NAME "ilc3"

void __iomem *ilc_base;

void __init ilc_early_init(struct platform_device* pdev)
{
	int size = pdev->resource[0].end - pdev->resource[0].start + 1;

	ilc_base = ioremap(pdev->resource[0].start, size);
}

static int __init ilc_probe(struct platform_device *pdev)
{
	int size = pdev->resource[0].end - pdev->resource[0].start + 1;

	if (!request_mem_region(pdev->resource[0].start, size, pdev->name))
		return -EBUSY;

	/* Have we already been set up through ilc_early_init? */
	if (ilc_base)
		return 0;

	ilc_early_init(pdev);

	return 0;
}

static struct platform_driver ilc_driver = {
	.probe		= ilc_probe,
	.driver	= {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
	},
};

static int __init ilc_init(void)
{
	return platform_driver_register(&ilc_driver);
}

arch_initcall(ilc_init);
