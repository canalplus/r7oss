#include <linux/platform_device.h>
#include <linux/stm/platform.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/stm/fli7610.h>

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

static struct stm_plat_blit_data plat_blit_fli7610[] = {
	[0] = {
	.device_type = STM_BLITTER_VERSION_STiH415_FLI7610,
	.nb_aq = 1,
	.nb_cq = 1,
	.line_buffer_length = 720,
	.mb_buffer_length = 720,
	.rotate_buffer_length = 64,
	.device_name = "stm-bdispII.0",
	},

	[1] = {
	.device_type = STM_BLITTER_VERSION_STiH415_FLI7610,
	.nb_aq = 1,
	.nb_cq = 1,
	.line_buffer_length = 720,
	.mb_buffer_length = 720,
	.rotate_buffer_length = 64,
	.device_name = "stm-bdispII.1",
	},
};

static struct platform_device bdisp_fli7610[] = {
	[0] = {
	.name = "stm-bdispII",
	.id = 0,
	.num_resources = 3,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM_NAMED("bdisp-io", 0xfd341000, 0x1000),
#if defined(CONFIG_ARM)
		FLI7610_RESOURCE_IRQ_NAMED("bdisp-aq1", 74),
#elif defined(CONFIG_SUPERH)
		FLI7610_RESOURCE_IRQ_NAMED("bdisp-aq2", 74),
#endif
#if defined(CONFIG_ARM)
		FLI7610_RESOURCE_IRQ_NAMED("bdisp-cq1", 75),
#elif defined(CONFIG_SUPERH)
		FLI7610_RESOURCE_IRQ_NAMED("bdisp-cq2", 75),
#endif
	},

	.dev.platform_data = ((void *) (uintptr_t)
			      &plat_blit_fli7610[0]),
	.dev.release = release,
	},

	[1] = {
	.name = "stm-bdispII",
	.id = 1,
	.num_resources = 3,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM_NAMED("bdisp-io", 0xfd342000, 0x1000),
#if defined(CONFIG_ARM)
		FLI7610_RESOURCE_IRQ_NAMED("bdisp-aq1", 76),
#elif defined(CONFIG_SUPERH)
		FLI7610_RESOURCE_IRQ_NAMED("bdisp-aq2", 76),
#endif
#if defined(CONFIG_ARM)
		FLI7610_RESOURCE_IRQ_NAMED("bdisp-cq1", 77),
#elif defined(CONFIG_SUPERH)
		FLI7610_RESOURCE_IRQ_NAMED("bdisp-cq2", 77),
#endif
	},

	.dev.platform_data = ((void *) (uintptr_t)
			      &plat_blit_fli7610[1]),
	.dev.release = release,
	},
};

static struct platform_device *fli7610_devices[] __initdata = {
	&bdisp_fli7610[0],
	&bdisp_fli7610[1],
};

static int __init stm_blit_fli7610_init(void)
{
	stm_blit_printd(0, "%s\n", __func__);

	return platform_add_devices(fli7610_devices,
				    ARRAY_SIZE(fli7610_devices));
}

static void __exit stm_blit_fli7610_exit(void)
{
	int i;

	stm_blit_printd(0, "%s\n", __func__);

	for (i = 0; i < ARRAY_SIZE(bdisp_fli7610); ++i)
		platform_device_unregister(&bdisp_fli7610[i]);
}

#ifdef UNIFIED_DRIVER

int __init stm_blit_glue_init(void)
{
	return stm_blit_fli7610_init();
}

void __exit stm_blit_glue_exit(void)
{
	stm_blit_fli7610_exit();
}

#else /* UNIFIED_DRIVER */

MODULE_AUTHOR("André Draszik <andre.draszik@st.com");
MODULE_DESCRIPTION("STMicroelectronics FLI7610 BDispII driver glue");
MODULE_LICENSE("GPL");

module_init(stm_blit_fli7610_init);
module_exit(stm_blit_fli7610_exit);

#endif /* UNIFIED_DRIVER */
