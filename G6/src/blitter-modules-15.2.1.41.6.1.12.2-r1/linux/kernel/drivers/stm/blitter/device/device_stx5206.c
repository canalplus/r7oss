#include <linux/platform_device.h>
#include <linux/stm/platform.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <linux/stm/bdisp2_shared.h>
#include "bdisp2/bdisp2_os.h"
#include "blit_debug.h"

#ifdef UNIFIED_DRIVER
#undef __exit
#define __exit
#endif


static void release(struct device *dev) { }

static struct stm_plat_blit_data plat_blit_5206 = {
	.device_type = STM_BLITTER_VERSION_5206,
	.nb_aq = 4,
	.nb_cq = 2,
	.line_buffer_length = 720,
	.mb_buffer_length = 720,
	.rotate_buffer_length = 64,
	.device_name = "stm-bdispII",
};

static struct platform_device bdisp_5206 = {
	.name = "stm-bdispII",
	.id = -1,
	.num_resources = 7,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM_NAMED("bdisp-io", 0xfe20b000, 0x1000),
		STM_PLAT_RESOURCE_IRQ_NAMED("bdisp-aq1", evt2irq(0x15e0), -1),
		STM_PLAT_RESOURCE_IRQ_NAMED("bdisp-aq2", evt2irq(0x15c0), -1),
		STM_PLAT_RESOURCE_IRQ_NAMED("bdisp-aq3", evt2irq(0x15a0), -1),
		STM_PLAT_RESOURCE_IRQ_NAMED("bdisp-aq4", evt2irq(0x1580), -1),
		STM_PLAT_RESOURCE_IRQ_NAMED("bdisp-cq1", evt2irq(0x13c0), -1),
		STM_PLAT_RESOURCE_IRQ_NAMED("bdisp-cq2", evt2irq(0x13a0), -1),
	},

	.dev.platform_data = ((void *) (uintptr_t)
			      &plat_blit_5206),
	.dev.release = release,
};


static int __init stm_blit_stx5206_init(void)
{
	stm_blit_printd(0, "%s\n", __func__);

	return platform_device_register(&bdisp_5206);
}

static void __exit stm_blit_stx5206_exit(void)
{
	stm_blit_printd(0, "%s\n", __func__);

	platform_device_unregister(&bdisp_5206);
}

#ifdef UNIFIED_DRIVER

int __init stm_blit_glue_init(void)
{
	return stm_blit_stx5206_init();
}

void __exit stm_blit_glue_exit(void)
{
	stm_blit_stx5206_exit();
}

#else /* UNIFIED_DRIVER */

MODULE_AUTHOR("André Draszik <andre.draszik@st.com");
MODULE_DESCRIPTION("STMicroelectronics STx5206 BDispII driver glue");
MODULE_LICENSE("GPL");

module_init(stm_blit_stx5206_init);
module_exit(stm_blit_stx5206_exit);

#endif /* UNIFIED_DRIVER */
