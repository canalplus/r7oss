/*
 * Support for Broadcom STB reg save for power management
 *
 * Copyright (C) 2014 Broadcom Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/syscore_ops.h>
#include <linux/io.h>
#include <linux/slab.h>


struct brcmstb_reg_group {
	void __iomem *regs;
	unsigned int count;
};
static struct brcmstb_reg_group *reg_groups;
static int num_reg_groups;
static u32 *reg_mem;

static int reg_save(void)
{
	int i;
	unsigned int j, total;

	if (0 == num_reg_groups)
		return 0;

	for (i = 0, total = 0; i < num_reg_groups; i++) {
		struct brcmstb_reg_group *p = &reg_groups[i];
		for (j = 0; j < p->count; j++)
			reg_mem[total++] = __raw_readl(p->regs + (j * 4));
	}
	return 0;
}


static void reg_restore(void)
{
	int i;
	unsigned int j, total;

	if (0 == num_reg_groups)
		return;

	for (i = 0, total = 0; i < num_reg_groups; i++) {
		struct brcmstb_reg_group *p = &reg_groups[i];
		for (j = 0; j < p->count; j++)
			__raw_writel(reg_mem[total++], p->regs + (j * 4));
	}
}


static struct syscore_ops regsave_pm_ops = {
	.suspend        = reg_save,
	.resume         = reg_restore,
};


int brcmstb_regsave_init(void)
{
	struct resource res;
	struct device_node *dn, *pp;
	int ret = 0, len, num_phandles = 0, i;
	unsigned int total;
	resource_size_t size;
	const char *name;


	dn = of_find_node_by_name(NULL, "s3");
	if (!dn)
		/* FIXME: return -EINVAL when all bolts have 's3' node */
		goto fail;

	if (of_get_property(dn, "syscon-refs", &len))
		name = "syscon-refs";
	else if (of_get_property(dn, "regsave-refs", &len))
		name = "regsave-refs";
	else
		/* FIXME: return -EINVAL when all bolts have 'syscon-refs' */
		goto fail;

	num_phandles = len / 4;
	reg_groups = kzalloc(num_phandles * sizeof(struct brcmstb_reg_group),
			    GFP_KERNEL);
	if (reg_groups == NULL) {
		ret = -ENOMEM;
		goto fail;
	}

	for (i = 0; i < num_phandles; i++) {
		void __iomem *regs;

		pp = of_parse_phandle(dn, name, i);
		if (!pp) {
			ret = -EIO;
			goto fail;
		}
		ret = of_address_to_resource(pp, 0, &res);
		if (ret)
			goto fail;
		size = resource_size(&res);
		regs = ioremap(res.start, size);
		if (!regs) {
			ret = -EIO;
			goto fail;
		}

		reg_groups[num_reg_groups].regs = regs;
		reg_groups[num_reg_groups].count = (unsigned) (size >> 2);
		num_reg_groups++;
	};

	for (i = 0, total = 0; i < num_reg_groups; i++)
		total += reg_groups[i].count;
	reg_mem = kmalloc(total * sizeof(u32), GFP_KERNEL);
	if (!reg_mem) {
		ret = -ENOMEM;
		goto fail;
	}
	of_node_put(dn);
	register_syscore_ops(&regsave_pm_ops);
	return 0;
fail:
	if (reg_groups) {
		for (i = 0; i < num_phandles; i++)
			if (reg_groups[i].regs)
				iounmap(reg_groups[i].regs);
			else
				break;
		kfree(reg_groups);
	}
	kfree(reg_mem);
	of_node_put(dn);
	return ret;
}
