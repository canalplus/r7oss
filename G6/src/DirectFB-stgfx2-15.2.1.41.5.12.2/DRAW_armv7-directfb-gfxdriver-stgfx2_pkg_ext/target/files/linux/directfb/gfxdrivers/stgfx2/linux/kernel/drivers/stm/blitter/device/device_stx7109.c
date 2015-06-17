#include <linux/platform_device.h>
#include <linux/stm/platform.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <asm/irq-ilc.h>

#include <linux/stm/bdisp2_shared.h>
#include "bdisp2/bdisp2_os.h"
#include "blit_debug.h"

#ifdef UNIFIED_DRIVER
#undef __exit
#define __exit
#endif


static void release(struct device *dev) { }

static struct stm_plat_blit_data plat_blit_7109c3 = {
	.device_type = STM_BLITTER_VERSION_7109c3,
	.nb_aq = 1,
	.nb_cq = 1,
	.line_buffer_length = 128,
	.mb_buffer_length = 128,
	.device_name = "stm-bdispII",
};

static struct platform_device bdisp_7109c3 = {
	.name = "stm-bdispII",
	.id = -1,
	.num_resources = 3,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM_NAMED("bdisp-io", 0x1920b000, 0x1000),
		STM_PLAT_RESOURCE_IRQ_NAMED("bdisp-aq1", ILC_IRQ(156), -1),
		STM_PLAT_RESOURCE_IRQ_NAMED("bdisp-cq1", evt2irq(0x1760), -1),
	},

	.dev.platform_data = ((void *) (uintptr_t)
			      &plat_blit_7109c3),
	.dev.release = release,
};


static int __init stm_blit_stx7109_init(void)
{
	stm_blit_printd(0, "%s\n", __func__);

	if (cpu_data->type == CPU_STX7109)
		if (cpu_data->cut_major >= 3)
			return platform_device_register(&bdisp_7109c3);

	printk(KERN_INFO "not supported on this SoC and/or cut\n");

	return -ENODEV;
}

static void __exit stm_blit_stx7109_exit(void)
{
	stm_blit_printd(0, "%s\n", __func__);

	if (cpu_data->type == CPU_STX7109)
		if (cpu_data->cut_major >= 3)
			platform_device_unregister(&bdisp_7109c3);
}

#ifdef UNIFIED_DRIVER

int __init stm_blit_glue_init(void)
{
	return stm_blit_stx7109_init();
}

void __exit stm_blit_glue_exit(void)
{
	stm_blit_stx7109_exit();
}

#else /* UNIFIED_DRIVER */

MODULE_AUTHOR("André Draszik <andre.draszik@st.com");
MODULE_DESCRIPTION("STMicroelectronics STx7109 cut3 BDispII driver glue");
MODULE_LICENSE("GPL");

module_init(stm_blit_stx7109_init);
module_exit(stm_blit_stx7109_exit);

#endif /* UNIFIED_DRIVER */
