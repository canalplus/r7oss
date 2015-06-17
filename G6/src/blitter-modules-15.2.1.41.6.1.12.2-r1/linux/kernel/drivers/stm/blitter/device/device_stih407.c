#include <linux/platform_device.h>
#include <linux/stm/platform.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/stm/stih407.h>

#ifdef CONFIG_SUPERH
#include <asm/irq-ilc.h>
#endif

#ifdef CONFIG_ARM
#include <asm/mach-types.h>
#endif


#include <linux/stm/bdisp2_shared.h>
#include "bdisp2/bdisp2_os.h"
#include "blit_debug.h"

#ifdef UNIFIED_DRIVER
#undef __exit
#define __exit
#endif


static void release(struct device *dev) { }

static struct stm_plat_blit_data plat_blit_h407[] = {
	[0] = {
	.device_type = STM_BLITTER_VERSION_STiH407,
	.nb_aq = 1,
	.nb_cq = 0,
	.line_buffer_length = 2048,
	.mb_buffer_length = 2048,
	.rotate_buffer_length = 256,
	.device_name = "stm-bdispII.0",
	},

	[1] = {
	.device_type = STM_BLITTER_VERSION_STiH407,
	.nb_aq = 1,
	.nb_cq = 0,
	.line_buffer_length = 2048,
	.mb_buffer_length = 2048,
	.rotate_buffer_length = 256,
	.device_name = "stm-bdispII.1",
	},
};

static struct platform_device bdisp_h407[] = {
	[0] = {
	.name = "stm-bdispII",
	.id = 0,
	.num_resources = 2,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM_NAMED("bdisp-io", 0x9f10000, 0x1000),
#if defined(CONFIG_ARM)
		STIH407_RESOURCE_IRQ_NAMED("bdisp-aq1", 38),
#elif defined(CONFIG_SUPERH)
		STIH407_RESOURCE_IRQ_NAMED("bdisp-aq2", 38),
#endif
	},

	.dev.platform_data = ((void *) (uintptr_t)
			      &plat_blit_h407[0]),
	.dev.release = release,
	},

	[1] = {
	.name = "stm-bdispII",
	.id = 1,
	.num_resources = 2,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM_NAMED("bdisp-io", 0x9f11000, 0x1000),
#if defined(CONFIG_ARM)
		STIH407_RESOURCE_IRQ_NAMED("bdisp-aq1", 39),
#elif defined(CONFIG_SUPERH)
		STIH407_RESOURCE_IRQ_NAMED("bdisp-aq2", 39),
#endif
	},

	.dev.platform_data = ((void *) (uintptr_t)
			      &plat_blit_h407[1]),
	.dev.release = release,
	},
};

static struct platform_device *h407_devices[] __initdata = {
	&bdisp_h407[0],
	&bdisp_h407[1],
};


/* we have to implement our own version of clk_add_alias(), as we want to be
   able to also remove the alias on module unload! */
static struct clk_lookup *clocks[4];
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

static int __init stm_blit_stih407_init(void)
{
	int ret;
	unsigned int i;

	stm_blit_printd(0, "%s\n", __func__);

	ret = platform_add_devices(h407_devices,
				   ARRAY_SIZE(h407_devices));
	if (ret)
		return ret;

	for (i = 0; i < ARRAY_SIZE(h407_devices); ++i) {
		char clk_name[17];

		snprintf(clk_name, sizeof(clk_name), "CLK_M_BDISP_%01u", i);
		clocks[i*2 + 0] = clk_add_alias_mine("bdisp0",
					dev_name(&h407_devices[i]->dev),
					clk_name, NULL);

		snprintf(clk_name, sizeof(clk_name), "CLK_M_ICN_BDISP");
		clocks[i*2 + 1] = clk_add_alias_mine("bdisp0_icn",
					dev_name(&h407_devices[i]->dev),
					clk_name, NULL);
	}

	return 0;
}

static void __exit stm_blit_stih407_exit(void)
{
	unsigned int i;

	stm_blit_printd(0, "%s\n", __func__);

	for (i = 0; i < ARRAY_SIZE(clocks); ++i) {
		if (!clocks[i])
			continue;

		clkdev_drop(clocks[i]);
	}

	for (i = 0; i < ARRAY_SIZE(bdisp_h407); ++i)
		platform_device_unregister(&bdisp_h407[i]);
}

#ifdef UNIFIED_DRIVER

int __init stm_blit_glue_init(void)
{
	return stm_blit_stih407_init();
}

void __exit stm_blit_glue_exit(void)
{
	stm_blit_stih407_exit();
}

#else /* UNIFIED_DRIVER */

MODULE_DESCRIPTION("STMicroelectronics STiH407 BDispII driver glue");
MODULE_LICENSE("GPL");

module_init(stm_blit_stih407_init);
module_exit(stm_blit_stih407_exit);

#endif /* UNIFIED_DRIVER */
