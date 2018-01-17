/*
 * Nexus interrupt(s) resolution API
 *
 * Copyright (C) 2015-2016, Broadcom
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * A copy of the GPL is available at
 * http://www.broadcom.com/licenses/GPLv2.php or from the Free Software
 * Foundation at https://www.gnu.org/licenses/ .
 */

#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/of.h>
#include <linux/of_irq.h>

#include <linux/brcmstb/irq_api.h>

#define BUILD_IRQ_NAME(_name)			\
	case brcmstb_l2_irq_##_name:		\
		return __stringify(_name);	\

static const char *brcmstb_l2_irq_to_name(brcmstb_l2_irq irq)
{
	switch (irq) {
	BUILD_IRQ_NAME(gio);
	BUILD_IRQ_NAME(gio_aon);
	BUILD_IRQ_NAME(iica);
	BUILD_IRQ_NAME(iicb);
	BUILD_IRQ_NAME(iicc);
	BUILD_IRQ_NAME(iicd);
	BUILD_IRQ_NAME(iice);
	BUILD_IRQ_NAME(iicf);
	BUILD_IRQ_NAME(iicg);
	BUILD_IRQ_NAME(irb);
	BUILD_IRQ_NAME(icap);
	BUILD_IRQ_NAME(kbd1);
	BUILD_IRQ_NAME(kbd2);
	BUILD_IRQ_NAME(kbd3);
	BUILD_IRQ_NAME(ldk);
	BUILD_IRQ_NAME(spi);
	BUILD_IRQ_NAME(ua);
	BUILD_IRQ_NAME(ub);
	BUILD_IRQ_NAME(uc);
	default:
		return NULL;
	}

	return NULL;
}

static int brcmstb_resolve_l2_irq(struct device_node *np, brcmstb_l2_irq irq)
{
	int i, num_elems, ret;
	const char *int_name, *irq_name;
	u32 hwirq;

	irq_name = brcmstb_l2_irq_to_name(irq);
	if (!irq_name)
		return -EINVAL;

	/* Special case for AON interrupt names, search the other node for the
	 * same name
	 */
	if (strstr(irq_name, "aon") && !strstr(np->name, "aon"))
		return -EAGAIN;

	pr_debug("%s: resolving on node %s\n", __func__, np->full_name);

	num_elems = of_property_count_strings(np, "interrupt-names");
	if (num_elems <= 0) {
		pr_err("Unable to find an interrupt-names property, check DT\n");
		return -EINVAL;
	}

	for (i = 0; i < num_elems; i++) {
		ret = of_property_read_u32_index(np, "interrupts", i,
						 &hwirq);
		if (ret < 0)
			return ret;

		ret = of_property_read_string_index(np,
						    "interrupt-names",
						    i, &int_name);
		if (ret < 0)
			return ret;

		/* We may be requesting to match, eg: "gio" with "gio_aon" */
		if (!strncasecmp(int_name, irq_name, strlen(int_name)))
			break;
	}

	if (i == num_elems) {
		pr_debug("%s: exceeded search for %s\n", __func__, irq_name);
		return -ENOENT;
	}

	pr_debug("%s IRQ name: %s Node: %s @%d mapped to: %d\n", __func__,
		 irq_name, np->full_name, i, hwirq);

	ret = of_irq_get(np, i);
	if (ret < 0) {
#ifndef CONFIG_BCM7120_L2_IRQ
		if (ret == -EPROBE_DEFER)
			pr_warn("Could't get IRQ. Enable CONFIG_BCM7120_L2_IRQ"
				" if you want \"%s\" interrupt support\n",
				irq_name);
#endif
	}

	return ret;
}

static const char *nexus_irq0_node_names[] = { "nexus-irq0", "nexus-irq0_aon" };

int brcmstb_get_l2_irq_id(brcmstb_l2_irq irq)
{
	struct device_node *np;
	int ret = -ENOENT;
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(nexus_irq0_node_names); i++) {
		np = of_find_node_by_name(NULL, nexus_irq0_node_names[i]);
		if (!np)
			continue;

		ret = brcmstb_resolve_l2_irq(np, irq);
		if (ret < 0)
			continue;

		if (ret == 0)
			return -EBUSY;

		if (ret > 0)
			break;
	}

	return ret;
}
EXPORT_SYMBOL(brcmstb_get_l2_irq_id);
