/*
 * -------------------------------------------------------------------------
 * Copyright (C) 2014  STMicroelectronics
 * Author:	Sudeep Biswas		<sudeep.biswas@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License V.2 ONLY.  See linux/COPYING for more information.
 * ------------------------------------------------------------------------- */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/of_address.h>
#include <linux/mm.h>
#include <linux/dma-contiguous.h>

#include <linux/power/st_lpm.h>

#define MAX_DDR_PCTL_NUMBER	2

#define PHYZQ0CR0		0x588

#define MIXER_BANK_BIT_REG	0x200
#define PHYS_ADD_SHIFT		22
#define PHYS_ADD_MASK		0x3F
#define BANK_BIT_POS_MASK	0x1F
#define BANK_MASK		0x1
#define	BUS_SIZE_REG		0x038
#define BUS_SIZE_SHIFT		2
#define BUS_SIZE_MASK		0x3
#define ROW_SHIFT		10
#define ROW_MASK		0xFFFF
#define COL_MASK		0x3E0
#define PCTL_ADD_ROW_SHIFT	13
#define PCTL_ADD_BANK_SHIFT	10
#define PCTL_ADD_COL_SHIFT	0
#define PHY_ADD_BANK_SHIFT	28
#define PHY_ADD_ROW_SHIFT	12
#define PHY_ADD_COL_SHIFT	0

struct ddr_cntrl_data {
	void * __iomem ddr_pctl_addresses[MAX_DDR_PCTL_NUMBER];
	struct page *allocated_page[MAX_DDR_PCTL_NUMBER];
	unsigned long pctl_address[MAX_DDR_PCTL_NUMBER];
	unsigned long phy_address[MAX_DDR_PCTL_NUMBER];
	unsigned long nr_ddr_pctl;
};

/* convert_address convert phyaddress to pctl_address & phy_address */
static void convert_address(unsigned long phyaddress,
			    void *mixer_base,
			    unsigned long *pctl_address,
			    unsigned long *phy_address)
{
	unsigned long mixerbankbitreg, bank = 0;
	unsigned long bankbitpos, ddrbussize, i, row, col;

	mixerbankbitreg = readl(mixer_base + MIXER_BANK_BIT_REG +
					(((phyaddress >> PHYS_ADD_SHIFT)
						& PHYS_ADD_MASK) * 4));

	for (i = 0; i < 3; i++) {
		bankbitpos = (mixerbankbitreg >> (i * 8)) & BANK_BIT_POS_MASK;
		bank = bank |
			((phyaddress >> (bankbitpos - i)) & BANK_MASK) << i;
		phyaddress = (phyaddress & ((1 << (bankbitpos - i)) - 1)) |
				((phyaddress >> 1) &
					(~((1 << (bankbitpos - i)) - 1)));
	}

	ddrbussize = (readl(mixer_base + BUS_SIZE_REG) >> BUS_SIZE_SHIFT)
			& BUS_SIZE_MASK;
	phyaddress = phyaddress >> ddrbussize;

	row = (phyaddress >> ROW_SHIFT) & ROW_MASK;
	col = phyaddress & COL_MASK;

	*pctl_address = (row << PCTL_ADD_ROW_SHIFT) |
				(bank << PCTL_ADD_BANK_SHIFT) |
					(col << PCTL_ADD_COL_SHIFT);

	*phy_address  = (bank << PHY_ADD_BANK_SHIFT) |
				(row << PHY_ADD_ROW_SHIFT) |
					(col << PHY_ADD_COL_SHIFT);
}

static int st_ddr_pctl_probe(struct platform_device *pdev)
{
	struct device_node *child;
	struct resource res;
	struct device_node *np;
	struct ddr_cntrl_data *ddr_data;
	unsigned long dtu_address;
	void * __iomem ddr_mixer_address[MAX_DDR_PCTL_NUMBER];
	unsigned long nr_mixer = 0, pctl_address, phy_address;
	struct page *page_alloc = NULL;
	int ret = 0;

	/*
	 * Set the local ddr_mixer_address array to zero. In case
	 * ddr mixer DT entry of a bank is missing but corresponding pctl
	 * entry is present, then before calling convert_address()
	 * we will check ddr_mixer_address array entry. It will be
	 * null in this case and we will not call convert_address()
	 * but will return error.
	 */
	memset(ddr_mixer_address, MAX_DDR_PCTL_NUMBER * sizeof(void *), 0x0);

	np = of_find_node_by_name(NULL, "ddr-mixer");
	if (IS_ERR_OR_NULL(np))
		return -ENODEV;

	for_each_child_of_node(np, child) {
		if (of_address_to_resource(child, 0, &res)) {
			of_node_put(np);
			return -ENXIO;
		}

		ddr_mixer_address[nr_mixer] =
			devm_ioremap_nocache(&pdev->dev, res.start,
					     resource_size(&res));
		if (!ddr_mixer_address[nr_mixer]) {
			of_node_put(np);
			return -ENOMEM;
		}

		nr_mixer++;
	}

	of_node_put(np);

	if (!nr_mixer) {
		dev_err(&pdev->dev, "DDRdrv:no ddr mixer DT entry defined\n");
		return -ENODEV;
	}

	np = pdev->dev.of_node;
	if (IS_ERR_OR_NULL(np))
		return -ENODEV;

	ddr_data = devm_kzalloc(&pdev->dev, sizeof(struct ddr_cntrl_data),
				GFP_KERNEL);
	if (!ddr_data)
		return -ENOMEM;

	for_each_child_of_node(np, child) {
		if (of_address_to_resource(child, 0, &res))
			return -ENXIO;

		ddr_data->ddr_pctl_addresses[ddr_data->nr_ddr_pctl] =
			devm_ioremap_nocache(&pdev->dev, res.start,
					     resource_size(&res));
		if (!ddr_data->ddr_pctl_addresses[ddr_data->nr_ddr_pctl])
			return -ENOMEM;

		/*
		 * For the first PCTL (LMI0) we allocate dtu buffer
		 * and prepare it for dtu algorithm. Currently we
		 * don't really support more than one LMI.
		 */
		if (ddr_data->nr_ddr_pctl > 0) {
			ddr_data->nr_ddr_pctl++;
			continue;
		}

		page_alloc = dma_alloc_from_contiguous(&pdev->dev, 1, 0);
		if (!page_alloc) {
			dev_err(&pdev->dev, "DDRdrv:CONFIG_CMA not active ?\n");
			return -ENOMEM;
		}

		dtu_address  = page_to_phys(page_alloc);
		ddr_data->allocated_page[ddr_data->nr_ddr_pctl] = page_alloc;

		/*
		 * if ddr_mixer_address[ddr_data->nr_ddr_pctl] is null then
		 * we must return error, should not call convert_address
		 */
		if (!ddr_mixer_address[ddr_data->nr_ddr_pctl]) {
			ret = -ENXIO;
			goto error;
		}

		convert_address(dtu_address,
				ddr_mixer_address[ddr_data->nr_ddr_pctl],
				&pctl_address,
				&phy_address);

		ddr_data->pctl_address[ddr_data->nr_ddr_pctl] = pctl_address;
		ddr_data->phy_address[ddr_data->nr_ddr_pctl] = phy_address;

		dev_dbg(&pdev->dev, "DTUphysadd(LMI0) = 0x%lx\n", dtu_address);
		dev_dbg(&pdev->dev, "DTUpctladd(LMI0) = 0x%lx\n", pctl_address);
		dev_dbg(&pdev->dev, "DTUphyadd(LMI0) = 0x%lx\n", phy_address);

		ddr_data->nr_ddr_pctl++;
	}

	if (!ddr_data->nr_ddr_pctl) {
		dev_err(&pdev->dev, "DDRdrv:no ddr pctl DT entry defined\n");
		ret = -ENODEV;
		goto error;
	}

	platform_set_drvdata(pdev, ddr_data);

	return 0;

error:
	dma_release_from_contiguous(&pdev->dev,
				    page_alloc,
				    1);
	return ret;
}

static int st_ddr_pctl_suspend(struct device *dev)
{
	struct ddr_cntrl_data *ddr_data;
	unsigned long iocal[MAX_DDR_PCTL_NUMBER];
	int i;
	unsigned long lpm_dmem_data[3];
	int offset;

	ddr_data = (struct ddr_cntrl_data *)dev_get_drvdata(dev);

	for (i = 0; i < ddr_data->nr_ddr_pctl; i++) {
		iocal[i] = readl((ddr_data->ddr_pctl_addresses[i]) + PHYZQ0CR0);
		dev_dbg(dev, "IOCAL val @suspend LMI%d = 0x%lx\n", i, iocal[i]);
	}

	if (!ddr_data->nr_ddr_pctl)
		return 0;

	/* Currently only one LMI is supported */
	lpm_dmem_data[0] = ddr_data->pctl_address[0];
	lpm_dmem_data[1] = iocal[0];
	lpm_dmem_data[2] = ddr_data->phy_address[0];

	offset = st_lpm_get_dmem_offset(ST_SBC_DMEM_DTU);
	if (offset < 0)
		return offset;

	return st_lpm_write_dmem((unsigned char *)lpm_dmem_data,
				 sizeof(long) * 3,
				 offset);
}

static int st_ddr_pctl_remove(struct platform_device *pdev)
{
	struct ddr_cntrl_data *ddr_data;
	int i;

	ddr_data = (struct ddr_cntrl_data *)dev_get_drvdata(&pdev->dev);

	for (i = 0; i < ddr_data->nr_ddr_pctl; i++) {
		if (!ddr_data->allocated_page[i])
			continue;

		dma_release_from_contiguous(&pdev->dev,
					    ddr_data->allocated_page[i],
					    1);
	}

	return 0;
}

static const struct dev_pm_ops st_ddr_pctl_pm = {
	.suspend = st_ddr_pctl_suspend,
};

static struct of_device_id st_ddr_pctl_match[] = {
	{
		.compatible = "st,ddr-pctl",
	},
	{},
};

MODULE_DEVICE_TABLE(of, st_ddr_pctl_match);

static struct platform_driver st_ddr_pctl_driver = {
	.driver.name = "st-ddr-pctl",
	.driver.owner = THIS_MODULE,
	.driver.pm = &st_ddr_pctl_pm,
	.driver.of_match_table = of_match_ptr(st_ddr_pctl_match),
	.probe = st_ddr_pctl_probe,
	.remove = st_ddr_pctl_remove,
};

static int __init st_ddr_pctl_init(void)
{
	int err = 0;

	err = platform_driver_register(&st_ddr_pctl_driver);
	if (err)
		pr_err("ST_DDR CNTRL driver fails on register (%x)\n" , err);
	else
		pr_info("ST_DDR CNTRL driver registered\n");

	return err;
}

void __exit st_ddr_pctl_exit(void)
{
	platform_driver_unregister(&st_ddr_pctl_driver);
}

module_init(st_ddr_pctl_init);
module_exit(st_ddr_pctl_exit);

MODULE_AUTHOR("Sudeep Biswas <sudeep.biswas@st.com>");
MODULE_DESCRIPTION("DDR CNTRL device driver for STI devices");
MODULE_LICENSE("GPL");
