#ifndef CONFIG_ARCH_STI
#include <linux/platform_device.h>
#include <linux/stm/platform.h>
#else
#include <linux/clkdev.h>
#endif
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/clk-private.h>

#ifdef CONFIG_SUPERH
#include <asm/irq-ilc.h>
#endif

#ifdef CONFIG_ARM
#include <asm/mach-types.h>
#endif


#include <linux/stm/bdisp2_shared.h>
#include "bdisp2/bdisp2_os.h"
#include "blit_debug.h"
#include "blitter_device.h"
#include "device/device_dt.h"

/*
 * Define the maximum number of clocks the blitter driver can be parsing from
 * the DT node.
 * Current BDispII IP version is using only 3 clocks ; two are shared (ic_clk
 * and cpu_clk) and one is owned by the bdisp IP (bdisp_clk).
 *
 * The driver is only taking care of IP and ICN clocks; CPU clock is not
 * managed in the blitter driver.
 */

extern struct of_device_id stm_blitter_match[];

#ifndef CONFIG_ARCH_STI

#define MAX_BDISP_CLK_NB	4

static struct clk_lookup *clocks[MAX_BDISP_CLK_NB];
static int clk_id = 0;

static struct clk_lookup * __init
clk_add_alias_mine(const char *alias, const char *alias_dev_name, char *id,
	struct device *dev)
{
	struct clk *r = clk_get(dev, id);
	struct clk_lookup *l;

	if (IS_ERR(r))
		return NULL;

	l = clkdev_alloc(r, alias, alias_dev_name);
	clk_put(r);
	if (!l)
		return NULL;
	clkdev_add(l);
	return l;
}
#endif

static void __init stm_blit_dt_dump(struct device_node        *np,
			            struct stm_plat_blit_data *data)
{
	const char *nameReq;
	const char *nameComp = NULL;
	int index = 0;

	/* Get the first requested compatible field */
	of_property_read_string_index(np, "compatible", 0, &nameReq);

	while (strlen(stm_blitter_match[index].compatible)) {
		if ((enum stm_blitter_device_type)stm_blitter_match[index].data
		      == data->device_type) {
			nameComp = stm_blitter_match[index].compatible;
			break;
		}
		index++;
	}

	/* Print-out used version of Blitter  */
	stm_blit_printd(0,
			"Required stm-blitter     : %s\n",
			nameReq);

	if (nameComp != NULL)
	{
		if (strcmp(nameComp, nameReq) != 0)
			stm_blit_printi("Device NOT FOUND using compatible: %s (%d)\n",
					nameComp, data->device_type);
		else
			stm_blit_printd(0,
					"Using stm-blitter device : %s (%d)\n",
					nameComp, data->device_type);
	}

	stm_blit_printd(0,
			"AQs (%d) CQs (%d)\n",
			data->nb_aq, data->nb_cq);
	stm_blit_printd(0,
			"Line delay (%d) MB delay (%d) Rotation delay (%d)\n",
			data->line_buffer_length, data->mb_buffer_length,
			data->rotate_buffer_length);
}

#ifndef CONFIG_ARCH_STI
int __init stm_blit_dt_get_compatible(struct device_node *np)
{
	int index;
	int index2;
	const char *name;

	/* Loop over all valid compatibility string */
	index = 0;
	while (of_property_read_string_index(np, "compatible", index, &name)
	       == 0) {
		/* Lock for compatibility in the compatible table */
		index2 = 0;
		while (strlen(stm_blitter_match[index2].compatible)) {
			if (strcmp(stm_blitter_match[index2].compatible, name)
			    == 0) {
				/* We get request compatibility, return */
				return (enum stm_blitter_device_type)
					stm_blitter_match[index2].data;
			}
			index2++;
		}
		index++;
	}

	/* This should never happen, there should
	   always found a compatible device node */
	BUG_ON(true);
	return -EINVAL;
}
#endif

int __init stm_blit_dt_get_pdata(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	const struct of_device_id *device_id;
	struct stm_plat_blit_data *data;
	int ret;

	stm_blit_printd(0, "%s\n", __func__);

	pdev->dev.platform_data = NULL;

	/* Allocate memory space for platform data */
	data = devm_kzalloc(&pdev->dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	/* Get device type that has probed this */
	device_id = of_match_device(stm_blitter_match, &pdev->dev);
	data->device_type = (enum stm_blitter_device_type)device_id->data;

	/* Read node properties */
	ret = of_property_read_u32(np, "aq", &data->nb_aq);
	if (ret != 0)
		return -EINVAL;

	ret = of_property_read_u32(np, "cq", &data->nb_cq);
	if (ret != 0)
		return -EINVAL;

	ret = of_property_read_u32(np, "line_buffer_length",
				   &data->line_buffer_length);
	if (ret != 0)
		return -EINVAL;

	ret = of_property_read_u32(np, "rotate_buffer_length",
				   &data->rotate_buffer_length);
	if (ret != 0)
		return -EINVAL;

	ret = of_property_read_u32(np, "mb_buffer_length",
				   &data->mb_buffer_length);
	if (ret != 0)
		return -EINVAL;

	ret = of_property_read_u32(np, "clock-frequency",
				   &data->clk_ip_freq);
	if (ret != 0)
		/* Optional, otherwise will use default value */
		data->clk_ip_freq = 0;

	snprintf(data->device_name, sizeof(data->device_name),
			 "stm-bdispII.%01d", of_alias_get_id(np, "blitter"));

	pdev->dev.platform_data = (void *)(uintptr_t)data;

	stm_blit_dt_dump(np, data);

	return 0;
}

void __exit stm_blit_dt_put_pdata(struct platform_device *pdev)
{
	struct stm_plat_blit_data *data
	   = (struct stm_plat_blit_data *) pdev->dev.platform_data;

	stm_blit_printd(0, "%s\n", __func__);

	devm_kfree(&pdev->dev, data);

	pdev->dev.platform_data = NULL;
}

#ifndef CONFIG_ARCH_STI
static int __init
stm_blit_dt_setup_clocks(struct device_node *np)
{
	struct platform_device *pdev    = NULL;
	struct device *dev              = NULL;
	struct device_node *clocks_node = NULL;
	struct device_node *clk_node    = NULL;
	int ret;

	stm_blit_printd(0, "%s\n", __func__);

	if (np == NULL)
		return -EINVAL;

	pdev = of_find_device_by_node(np);
	if (pdev == NULL)
		return -ENODEV;

	dev = &pdev->dev;
	if (dev == NULL)
		return -ENODEV;

	/* setup clocks */
	clocks_node = of_get_child_by_name(np, "clocks");
	clk_node = NULL;
	if (!clocks_node) {
		stm_blit_printd(0, "%s : no bdisp2 clocks found\n", __func__);
	} else {
		while((clk_node = of_get_next_child(clocks_node, clk_node))
			  && (clk_id<MAX_BDISP_CLK_NB)) {
			const char *s = NULL;

			/* retreive the clock name i.e "CLK_IC_BDISP_0". */
			of_property_read_string(clk_node, "clk-name", &s );
			if(s) {
				struct clk *clk = NULL;
				unsigned int clk_rate = 0;
				char clk_name[32];

				/* add our alias */
				strlcpy(clk_name, s, sizeof(clk_name));
				clocks[clk_id] = clk_add_alias_mine(clk_node->name, dev_name(dev), clk_name, NULL);

				if(clocks[clk_id])
					stm_blit_printd(0, "add clock alias %s (clk_name = %s)\n",
							clk_node->name, clk_name);

				clk = clk_get(dev, clk_name);
				if(IS_ERR(clk))
					continue;

				ret = of_property_read_u32(clk_node, "clk-rate", &clk_rate);
				if (ret != 0)
					return -EINVAL;
				/* setup default clk_rate */
				clk_set_rate(clk, (unsigned long)clk_rate);
				clk_put(clk);
				clk_id++;
			}

			of_node_put(clk_node); /* free the node */
		}
		of_node_put(clocks_node);
	}

	return 0;
}
#endif

int __init stm_blit_dt_init(void)
{
#ifndef CONFIG_ARCH_STI
	struct device_node *np;

	stm_blit_printd(0, "%s\n", __func__);

	for_each_matching_node(np, stm_blitter_match)
	{
		stm_blit_dt_setup_clocks(np);
	}
#endif

	return 0;
}

int stm_blit_dt_exit(void)
{
	struct device_node *np;
#ifndef CONFIG_ARCH_STI
	int i;
#endif

	stm_blit_printd(0, "%s\n", __func__);

	np = of_find_matching_node(NULL, stm_blitter_match);
	if (!np)
		return -ENODEV;

#ifndef CONFIG_ARCH_STI
	for (i = 0; i < ARRAY_SIZE(clocks); ++i) {
		if (!clocks[i])
			continue;

		clkdev_drop(clocks[i]);
	}
#endif

	return 0;
}
