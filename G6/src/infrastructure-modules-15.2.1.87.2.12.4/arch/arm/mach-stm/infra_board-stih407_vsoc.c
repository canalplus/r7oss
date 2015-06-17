#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/init.h>    /* Initilisation support */
#include <linux/kernel.h>  /* Kernel support */

#include <linux/stm/pad.h>
#include <linux/stm/platform.h>
#include <linux/stm/stih407.h>
#include <linux/stm/stih407-periphs.h>

#include "infra_platform.h"

static void cec_release(struct device *device);
static void stih407_unconfigure_cec(uint32_t dev_id);
static void stih407_configure_cec(uint32_t dev_id);

static struct stm_pad_config stih407_cec_pad_configs[] = {
/*CEC block in SBC */
	[0] = {
		/* CEC_HDMI PIO2[4]*/
		.gpios_num = 1,
		.gpios = (struct stm_pad_gpio []) {
		       /* CEC_HDMI*/
			STM_PAD_PIO_BIDIR(STIH407_GPIO(2), 4, 1),
		},
	}
};

static struct platform_device stih407_cec_devices[] = {
	[0] = {
		.name = "stm-cec",
		.id   = 0,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0x94A087C, 0x64),
			STIH407_RESOURCE_IRQ(140),
		},
		.dev.release = cec_release,
		.dev.platform_data = &(struct stm_cec_platform_data) {
			.pad_config = &stih407_cec_pad_configs[0],
			.num_config_data = 1,
			.config_data = (struct cec_config_data[]){
				{
					.address = 0x9600008,
					.mask = (1<<2),
				},
			},
		},
	},
};

static struct platform_device stm_lx_cpu_devices[] = {
	[0] = {
		/* This LX corresponds to DMU0 */
		.name = "stm-lx-cpu",
		.id = 0
	},
	[1] = {
		/* This LX corresponds to AUD0 */
		.name = "stm-lx-cpu",
		.id = 1
	},
	[2] = {
		/* This LX corresponds to GP0 */
		.name = "stm-lx-cpu",
		.id = 2
	},
	[3] = {
		/* This LX corresponds to GP1 */
		.name = "stm-lx-cpu",
		.id = 3
	}
};

static void cec_release(struct device *device)
{

/* TO BE DEFINED */
}

void  stih407_configure_cec(uint32_t dev_id)
{
	platform_device_register(&stih407_cec_devices[dev_id]);
}

void  stih407_unconfigure_cec(uint32_t dev_id)
{
	platform_device_unregister(&stih407_cec_devices[dev_id]);
}

void cec_configure(void)
{
	stih407_configure_cec(0);
}

void cec_unconfigure(void)
{
	stih407_unconfigure_cec(0);
}

void lx_cpu_configure(void)
{
	clk_add_alias("clk_lx_cpu",
			"stm-lx-cpu.0",
			"CLK_C0_ST231_DMU",
			NULL);
	clk_add_alias("clk_lx_cpu",
			"stm-lx-cpu.2",
			"CLK_C0_ST231_AUD_0_GP_0",
			NULL);
	clk_add_alias("clk_lx_cpu",
			"stm-lx-cpu.3",
			"CLK_C0_ST231_AUD_0_GP_0",
			NULL);
	clk_add_alias("clk_lx_cpu",
			"stm-lx-cpu.4",
			"CLK_C0_ST231_GP_1",
			NULL);

	platform_device_register(&stm_lx_cpu_devices[0]);
	platform_device_register(&stm_lx_cpu_devices[1]);
	platform_device_register(&stm_lx_cpu_devices[2]);
	platform_device_register(&stm_lx_cpu_devices[3]);

}

void lx_cpu_unconfigure(void)
{
	platform_device_unregister(&stm_lx_cpu_devices[0]);
	platform_device_unregister(&stm_lx_cpu_devices[1]);
	platform_device_unregister(&stm_lx_cpu_devices[2]);
	platform_device_unregister(&stm_lx_cpu_devices[3]);

	return;
}
