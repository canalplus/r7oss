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

static struct stm_plat_blit_data plat_blit_7108[] = {
	[0] = {
	.device_type = STM_BLITTER_VERSION_7106_7108,
	.nb_aq = 4,
	.nb_cq = 2,
	.line_buffer_length = 720,
	.mb_buffer_length = 720,
	.rotate_buffer_length = 64,
	.device_name = "stm-bdispII.0",
	},

	[1] = {
	.device_type = STM_BLITTER_VERSION_7106_7108,
	.nb_aq = 4,
	.nb_cq = 2,
	.line_buffer_length = 720,
	.mb_buffer_length = 720,
	.rotate_buffer_length = 64,
	.device_name = "stm-bdispII.1",
	},
};

static struct platform_device bdisp_7108[] = {
	[0] = {
	.name = "stm-bdispII",
	.id = 0,
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
			      &plat_blit_7108[0]),
	.dev.release = release,
	},

	[1] = {
	.name = "stm-bdispII",
	.id = 1,
	.num_resources = 7,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM_NAMED("bdisp-io", 0xfdabc000, 0x1000),
		STM_PLAT_RESOURCE_IRQ_NAMED("bdisp-aq1", ILC_IRQ(173), -1),
		STM_PLAT_RESOURCE_IRQ_NAMED("bdisp-aq2", ILC_IRQ(174), -1),
		STM_PLAT_RESOURCE_IRQ_NAMED("bdisp-aq3", ILC_IRQ(175), -1),
		STM_PLAT_RESOURCE_IRQ_NAMED("bdisp-aq4", ILC_IRQ(176), -1),
		STM_PLAT_RESOURCE_IRQ_NAMED("bdisp-cq1", ILC_IRQ(177), -1),
		STM_PLAT_RESOURCE_IRQ_NAMED("bdisp-cq2", ILC_IRQ(178), -1),
	},

	.dev.platform_data = ((void *) (uintptr_t)
			      &plat_blit_7108[1]),
	.dev.release = release,
	},
};

static struct platform_device *sti7108_devices[] __initdata = {
	&bdisp_7108[0],
	&bdisp_7108[1],
};


static int __init stm_blit_stx7108_init(void)
{
	int n_devices = ARRAY_SIZE(sti7108_devices);

	stm_blit_printd(0, "%s\n", __func__);

	if (cpu_data->cut_major < 2) {
		bdisp_7108[0].id = -1;
		n_devices = 1;
	}

	return platform_add_devices(sti7108_devices, n_devices);
}

static void __exit stm_blit_stx7108_exit(void)
{
	stm_blit_printd(0, "%s\n", __func__);

	if (cpu_data->cut_major > 1)
		platform_device_unregister(&bdisp_7108[1]);
	platform_device_unregister(&bdisp_7108[0]);
}

#ifdef UNIFIED_DRIVER

int __init stm_blit_glue_init(void)
{
	return stm_blit_stx7108_init();
}

void __exit stm_blit_glue_exit(void)
{
	stm_blit_stx7108_exit();
}

#else /* UNIFIED_DRIVER */

MODULE_AUTHOR("André Draszik <andre.draszik@st.com");
MODULE_DESCRIPTION("STMicroelectronics STx7108 BDispII driver glue");
MODULE_LICENSE("GPL");

module_init(stm_blit_stx7108_init);
module_exit(stm_blit_stx7108_exit);

#endif /* UNIFIED_DRIVER */
