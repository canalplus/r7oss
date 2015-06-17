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
#include "device/device_stx7106.h"

static void release(struct device *dev) { }

static struct stm_plat_blit_data plat_blit_7106[] = {
	.device_type = STM_BLITTER_VERSION_7106_7108,
	.nb_aq = 4,
	.nb_cq = 2,
	.line_buffer_length = 720,
	.mb_buffer_length = 720,
	.rotate_buffer_length = 64,
	.device_name = "stm-bdispII",
};

static struct platform_device bdisp_7106 = {
	.name = "stm-bdispII",
	.id = -1,
	.num_resources = 7,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM_NAMED("bdisp-io", 0xfe20b000, 0x1000),
		STM_PLAT_RESOURCE_IRQ_NAMED("bdisp-aq1", evt2irq(0x1220), -1),
		STM_PLAT_RESOURCE_IRQ_NAMED("bdisp-aq2", evt2irq(0x1220), -1),
		STM_PLAT_RESOURCE_IRQ_NAMED("bdisp-aq3", evt2irq(0x1220), -1),
		STM_PLAT_RESOURCE_IRQ_NAMED("bdisp-aq4", evt2irq(0x1220), -1),
		STM_PLAT_RESOURCE_IRQ_NAMED("bdisp-cq1", evt2irq(0x1760), -1),
		STM_PLAT_RESOURCE_IRQ_NAMED("bdisp-cq2", evt2irq(0x1760), -1),
	},

	.dev.platform_data = ((void *) (uintptr_t)
			      &plat_blit_7106),
	.dev.release = release,
};


#ifndef UNIFIED_DRIVER
static
#endif
int __init stm_blit_stx7106_init(void)
{
	stm_blit_printd(0, "%s\n", __func__);

	if (cpu_data->type == CPU_STX7106)
		return platform_device_register(&bdisp_7106);

	return -ENODEV;
}

#ifndef UNIFIED_DRIVER
static
#endif
void __exit stm_blit_stx7106_exit(void)
{
	stm_blit_printd(0, "%s\n", __func__);

	if (cpu_data->type == CPU_STX7106)
		platform_device_unregister(&bdisp_7106);
}

#ifndef UNIFIED_DRIVER

MODULE_AUTHOR("André Draszik <andre.draszik@st.com");
MODULE_DESCRIPTION("STMicroelectronics STx7106 BDispII driver glue");
MODULE_LICENSE("GPL");

module_init(stm_blit_stx7106_init);
module_exit(stm_blit_stx7106_exit);

#endif /* UNIFIED_DRIVER */
