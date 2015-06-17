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

static struct stm_plat_blit_data plat_blit_7200c1 = {
	.device_type = STM_BLITTER_VERSION_7200c1,
	.nb_aq = 1,
	.nb_cq = 2,
	.line_buffer_length = 128,
	.mb_buffer_length = 128,
	.device_name = "stm-bdispII",
};

static struct platform_device bdisp_7200c1 = {
	.name = "stm-bdispII",
	.id = -1,
	.num_resources = 4,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM_NAMED("bdisp-io", 0xfd940000, 0x1000),
		STM_PLAT_RESOURCE_IRQ_NAMED("bdisp-aq1", ILC_IRQ(50), -1),
		STM_PLAT_RESOURCE_IRQ_NAMED("bdisp-cq1", ILC_IRQ(51), -1),
		STM_PLAT_RESOURCE_IRQ_NAMED("bdisp-cq2", ILC_IRQ(52), -1),
	},

	.dev.platform_data = ((void *) (uintptr_t)
			      &plat_blit_7200c1),
	.dev.release = release,
};

static struct stm_plat_blit_data plat_blit_7200c2 = {
	.device_type = STM_BLITTER_VERSION_7200c2;
	.nb_aq = 4;
	.nb_cq = 2;
	.line_buffer_length = 128;
	.mb_buffer_length = 128;
	.rotate_buffer_length = 16;
	.device_name = "stm-bdispII";
};

static struct platform_device bdisp_7200c2 = {
	.name = "stm-bdispII",
	.id = -1,
	.num_resources = 7,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM_NAMED("bdisp-io", 0xfd940000, 0x1000),
		STM_PLAT_RESOURCE_IRQ_NAMED("bdisp-aq1", ILC_IRQ(50),  -1),
		STM_PLAT_RESOURCE_IRQ_NAMED("bdisp-aq2", ILC_IRQ(142), -1),
		STM_PLAT_RESOURCE_IRQ_NAMED("bdisp-aq3", ILC_IRQ(143), -1),
		STM_PLAT_RESOURCE_IRQ_NAMED("bdisp-aq4", ILC_IRQ(144), -1),
		STM_PLAT_RESOURCE_IRQ_NAMED("bdisp-cq1", ILC_IRQ(51),  -1),
		STM_PLAT_RESOURCE_IRQ_NAMED("bdisp-cq2", ILC_IRQ(52),  -1),
	},

	.dev.platform_data = ((void *) (uintptr_t)
			      &plat_blit_7200c2),
	.dev.release = release,
};


static int __init stm_blit_stx7200_init(void)
{
	stm_blit_printd(0, "%s\n", __func__);

	if (cpu_data->cut_major == 1)
		return platform_device_register(&bdisp_7200c1);

	return platform_device_register(&bdisp_7200c2);
}

static void __exit stm_blit_stx7200_exit(void)
{
	stm_blit_printd(0, "%s\n", __func__);

	if (cpu_data->cut_major == 1)
		platform_device_unregister(&bdisp_7200c1);
	else
		platform_device_unregister(&bdisp_7200c2);
}

#ifdef UNIFIED_DRIVER

int __init stm_blit_glue_init(void)
{
	return stm_blit_stx7200_init();
}

void __exit stm_blit_glue_exit(void)
{
	stm_blit_stx7200_exit();
}

#else /* UNIFIED_DRIVER */

MODULE_AUTHOR("André Draszik <andre.draszik@st.com");
MODULE_DESCRIPTION("STMicroelectronics STx7200 BDispII driver glue");
MODULE_LICENSE("GPL");

module_init(stm_blit_stx7200_init);
module_exit(stm_blit_stx7200_exit);

#endif /* UNIFIED_DRIVER */
