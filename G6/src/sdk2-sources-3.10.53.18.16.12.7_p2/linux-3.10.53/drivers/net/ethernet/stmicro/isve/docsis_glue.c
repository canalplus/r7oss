/*
 * Copyright (C) 2013 STMicroelectronics Limited
 * Author: Giuseppe Cavallaro <peppe.cavallaro@st.com>
 *
 * This is a driver used for configuring on some ST platforms the Upstream
 * and Downstream queues before opening the Virtual Interface with ISVE
 * driver.
 * Currently it only enables the Downstream queue but it can be extended
 * to support further settings.
 * It is a platform driver and needs to have some platform fields: it also
 * supports DT model.
 * This driver can be used to expose the queue registers via debugFS support.
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/of.h>
#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>
#endif
#include "docsis_glue.h"

struct stm_docsis_priv {
	void __iomem *dfwd_bioaddr;	/* base addr for Forward block */
	void __iomem *upiim_bioaddr;	/* base addr for Upstream block */
	struct device *dev;
};
struct stm_docsis_priv *priv;

#define DOWNSTREAM_REG_N	10
#define UPSTREAM_REG_N		12

#define DSFWD_ROUTE_MASK		0x18
#define USIIM_ARBITRATE_OFFSET		0x2c
#define ENABLE_QUEUE(q)	((1 << q) & 0x0000FFFF)
#define DISABLE_QUEUE(q)  ~(1 << q)

#ifdef CONFIG_DEBUG_FS
static struct dentry *docsis_queue_status;

static int docsis_queue_sysfs_read(struct seq_file *seq, void *v)
{
	int i;

	seq_puts(seq, "DFWD: forwarding block registers:\n");
	for (i = 0; i < DOWNSTREAM_REG_N; i++)
		seq_printf(seq, "0x%x = 0x%08X\n", i * 0x4,
			   readl_relaxed(priv->dfwd_bioaddr + i * 0x4));

	seq_puts(seq, "\nUPIIM: upstream block registers:\n");
	for (i = 0; i < UPSTREAM_REG_N; i++)
		seq_printf(seq, "0x%x = 0x%08X\n", i * 0x4,
			   readl_relaxed(priv->upiim_bioaddr + i * 0x4));

	return 0;
}

static int docsis_queue_sysfs_ring_open(struct inode *inode, struct file *file)
{
	return single_open(file, docsis_queue_sysfs_read, inode->i_private);
}

static const struct file_operations docsis_queue_status_fops = {
	.owner = THIS_MODULE,
	.open = docsis_queue_sysfs_ring_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static void docsis_queue_init_debug_fs(void)
{
	docsis_queue_status =
	    debugfs_create_file("docsis-glue", S_IRUGO, NULL, NULL,
				&docsis_queue_status_fops);
	if (!docsis_queue_status || IS_ERR(docsis_queue_status))
		dev_err(priv->dev, "cannot create debugFS entry\n");
}

static void docsis_queue_exit_debug_fs(void)
{
	debugfs_remove(docsis_queue_status);
}
#else
#define docsis_queue_init_debug_fs()	do {} while (0)
#define docsis_queue_exit_debug_fs()	do {} while (0)
#endif

/**
 * docsis_en_queue - enable/disable docsis queues
 * @type: downstream or upstream docsis queue type
 * @en: enable or disable the queue
 * @q: number of the queue to enable or disable
 * Description: this is a function that can be called to enable or disable
 * a downstream or upstream queue.
 */
void docsis_en_queue(enum docsis_queue_en type, bool en, int q)
{
	u32 value;
	void __iomem *ioaddr;

	if (type == DOCSIS_USIIM)
		ioaddr = priv->upiim_bioaddr + USIIM_ARBITRATE_OFFSET;
	else if (type == DOCSIS_DSFWD)
		ioaddr = priv->dfwd_bioaddr + DSFWD_ROUTE_MASK;
	else {
		dev_err(priv->dev, "invalid queue parameter\n");
		return;
	}

	/* This address can also be written by another CPU. Since both CPUs
	 * only enable bits we can loop until our changes are accepted.
	 */
	do {
		value = readl_relaxed(ioaddr);

		if (en)
			value |= ENABLE_QUEUE(q);
		else
			value &= DISABLE_QUEUE(q);

		writel_relaxed(value, ioaddr);

	} while (value != readl_relaxed(ioaddr));

	dev_info(priv->dev, "%s %s queue %d\n", (type == DOCSIS_USIIM) ?
		 "USIIM" : "DSFWD", en ? "enable" : "disable", q);
}
EXPORT_SYMBOL(docsis_en_queue);

static int docsis_queue_driver_probe(struct platform_device *pdev)
{
	struct resource *res, *res1;
	struct device *dev = &pdev->dev;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		dev_err(dev, "Unable to allocate platform data\n");
		return -ENOMEM;
	}
	priv->dev = dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -ENODEV;

	dev_info(dev, "get dfwd resource: res->start 0x%x\n", res->start);
	priv->dfwd_bioaddr = devm_request_and_ioremap(dev, res);
	if (!priv->dfwd_bioaddr) {
		dev_err(dev, ": ERROR: dfwd memory mapping failed");
		release_mem_region(res->start, resource_size(res));
		return -ENOMEM;
	}

	res1 = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res1)
		return -ENODEV;

	dev_info(dev, "get upiim resource: res->start 0x%x\n", res1->start);
	priv->upiim_bioaddr = devm_request_and_ioremap(dev, res1);
	if (!priv->upiim_bioaddr) {
		dev_err(dev, "ERROR: upiim mem mapping failed\n");
		release_mem_region(res->start, resource_size(res));
		release_mem_region(res1->start, resource_size(res1));
		return -ENOMEM;
	}

	docsis_queue_init_debug_fs();

	return 0;
}

static int docsis_queue_driver_remove(struct platform_device *pdev)
{
	docsis_queue_exit_debug_fs();

	return 0;
}

static struct of_device_id stm_docsis_queue_match[] = {
	{
	 .compatible = "st,docsis",
	 },
	{},
};

MODULE_DEVICE_TABLE(of, stm_docsis_queue_match);

static struct platform_driver docsis_queue_driver = {
	.driver = {
		   .name = "docsis_glue",
		   .owner = THIS_MODULE,
		   .of_match_table = of_match_ptr(stm_docsis_queue_match),
		   },
	.probe = docsis_queue_driver_probe,
	.remove = docsis_queue_driver_remove,
};

module_platform_driver(docsis_queue_driver);
MODULE_AUTHOR("Giuseppe Cavallaro <peppe.cavallaro@st.com>");
MODULE_DESCRIPTION("ST Docsis Glue logic driver to manage input/output queue");
MODULE_LICENSE("GPL");
