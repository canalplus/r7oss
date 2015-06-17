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

static struct stm_plat_blit_data plat_blit_h205[] = {
	.device_type = STM_BLITTER_VERSION_STiH205,
	.nb_aq = 4,
	.nb_cq = 2,
	.line_buffer_length = 720,
	.mb_buffer_length = 720,
	.rotate_buffer_length = 64,
	.device_name = "stm-bdispII",
};

static struct platform_device bdisp_h205 = {
	.name = "stm-bdispII",
	.id = -1,
	.num_resources = 7,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM_NAMED("bdisp-io", 0xfdabe000, 0x1000),
		STM_PLAT_RESOURCE_IRQ_NAMED("bdisp-aq1", ILC_IRQ(81), -1),
		STM_PLAT_RESOURCE_IRQ_NAMED("bdisp-aq2", ILC_IRQ(82), -1),
		STM_PLAT_RESOURCE_IRQ_NAMED("bdisp-aq3", ILC_IRQ(83), -1),
		STM_PLAT_RESOURCE_IRQ_NAMED("bdisp-aq4", ILC_IRQ(84), -1),
		STM_PLAT_RESOURCE_IRQ_NAMED("bdisp-cq1", ILC_IRQ(85), -1),
		STM_PLAT_RESOURCE_IRQ_NAMED("bdisp-cq2", ILC_IRQ(86), -1),
	},

	.dev.platform_data = ((void *) (uintptr_t)
			      &plat_blit_h205),
	.dev.release = release,
};


static int __init stm_blit_stih205_init(void)
{
	stm_blit_printd(0, "%s\n", __func__);

	return platform_device_register(&bdisp_h205);
}

static void __exit stm_blit_stih205_exit(void)
{
	stm_blit_printd(0, "%s\n", __func__);

	platform_device_unregister(&bdisp_h205);
}

#ifdef UNIFIED_DRIVER

int __init stm_blit_glue_init(void)
{
	return stm_blit_stih205_init();
}

void __exit stm_blit_glue_exit(void)
{
	stm_blit_stih205_exit();
}

#else /* UNIFIED_DRIVER */

MODULE_DESCRIPTION("STMicroelectronics STiH205 BDispII driver glue");
MODULE_LICENSE("GPL");

module_init(stm_blit_stih205_init);
module_exit(stm_blit_stih205_exit);

#endif /* UNIFIED_DRIVER */
