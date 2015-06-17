#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/init.h> /* Initilisation support */
#include <linux/kernel.h>/* Kernel support */
#include <linux/stm/pad.h>
#include <linux/stm/platform.h>
#include <linux/stm/stih415.h>
#include <linux/stm/stih415-periphs.h>
#include "infra_platform.h"

#include "infra_os_wrapper.h"
static void cec_release(struct device *device);
static void stih415_unconfigure_cec(uint32_t dev_id);
static void stih415_configure_cec(uint32_t dev_id);

static struct stm_pad_config stih415_cec_pad_configs[] = {
	/*CEC block in SBC */
	[0] = {
		/* CEC_HDMI PIO2[4]*/
		.gpios_num = 1,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_BIDIR(2, 4, 1), /* CEC_HDMI*/
		},
	}
};

static struct platform_device stih415_cec_devices[] = {
	[0] = {
		.name = "stm-cec",
		.id = 0,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xFE4A087C, 0x64),
			STIH415_RESOURCE_IRQ(211),
		},
		.dev.release = cec_release,
		.dev.platform_data = &(struct stm_cec_platform_data) {
			.pad_config = &stih415_cec_pad_configs[0],
			.num_config_data = 1,
			.config_data = (struct cec_config_data[]){
				{
					.address = 0xFE600034,
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
		/* This LX corresponds to DMU1 */
		.name = "stm-lx-cpu",
		.id = 1
	},
	[2] = {
		/* This LX corresponds to AUD0 */
		.name = "stm-lx-cpu",
		.id = 2
	},
	[3] = {
		/* This LX corresponds to GP0 */
		.name = "stm-lx-cpu",
		.id = 3
	},
	[4] = {
		/* This LX corresponds to GP1 */
		.name = "stm-lx-cpu",
		.id = 4
	}

};

static void cec_release(struct device *device)
{
	/* Dummy function for releasing the platform device */
	/* Doing nothing but needed */
}
void stih415_configure_cec(uint32_t dev_id) /* To be called from setup.c*/
{
	platform_device_register(&stih415_cec_devices[dev_id]);
}

void stih415_unconfigure_cec(uint32_t dev_id) /* To be called from setup.c*/
{
	platform_device_unregister(&stih415_cec_devices[dev_id]);
}

void cec_configure (void)
{
	stih415_configure_cec(0);
}

void cec_unconfigure (void)
{
	stih415_unconfigure_cec(0);
}

void lx_cpu_configure(void)
{
	clk_add_alias("clk_lx_cpu", "stm-lx-cpu.0", "CLKM_ST231_DMU_0", NULL);
	clk_add_alias("clk_lx_cpu", "stm-lx-cpu.1", "CLKM_ST231_DMU_1", NULL);
	clk_add_alias("clk_lx_cpu", "stm-lx-cpu.2", "CLKM_ST231_AUD", NULL);
	clk_add_alias("clk_lx_cpu", "stm-lx-cpu.3", "CLKM_ST231_GP_0", NULL);
	clk_add_alias("clk_lx_cpu", "stm-lx-cpu.4", "CLKM_ST231_GP_1", NULL);

	platform_device_register(&stm_lx_cpu_devices[0]);
	platform_device_register(&stm_lx_cpu_devices[1]);
	platform_device_register(&stm_lx_cpu_devices[2]);
	platform_device_register(&stm_lx_cpu_devices[3]);
	platform_device_register(&stm_lx_cpu_devices[4]);
}

void lx_cpu_unconfigure(void)
{
	platform_device_unregister(&stm_lx_cpu_devices[0]);
	platform_device_unregister(&stm_lx_cpu_devices[1]);
	platform_device_unregister(&stm_lx_cpu_devices[2]);
	platform_device_unregister(&stm_lx_cpu_devices[3]);
	platform_device_unregister(&stm_lx_cpu_devices[4]);

	return;
}
