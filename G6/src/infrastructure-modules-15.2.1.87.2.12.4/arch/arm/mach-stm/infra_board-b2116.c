#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/init.h>    /* Initilisation support */
#include <linux/kernel.h>  /* Kernel support */

#include <linux/stm/pad.h>
#include <linux/stm/platform.h>
#include <linux/stm/stih416.h>
#include <linux/stm/mpe42-periphs.h>
#include <linux/stm/sasg2-periphs.h>

#include "infra_platform.h"

/*H416 is used as base for H315*/
static void cec_release(struct device *device);
static void  stih315_unconfigure_cec(uint32_t dev_id);
static void  stih315_configure_cec(uint32_t dev_id);

static struct stm_pad_config stih315_cec_pad_configs[] = {
/*CEC block in SBC */
	[0] = {
		/* CEC_HDMI PIO2[4]*/
		.gpios_num = 1,
		.gpios = (struct stm_pad_gpio []) {
			STIH416_PAD_PIO_BIDIR(2, 4, 1),       /* CEC_HDMI*/
		},
	}
};

static struct platform_device stih315_cec_devices[] = {
[0] = {
		.name = "stm-cec",
		.id   = 0,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xFE4A087C, 0x64),
			STIH416_RESOURCE_IRQ(211),
		},
		.dev.release = cec_release,
		.dev.platform_data = &(struct stm_cec_platform_data) {
			.pad_config = &stih315_cec_pad_configs[0],
			.num_config_data = 1,
			.config_data = (struct cec_config_data[]){
				{
					.address = 0xFE6007D8,
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
		.num_resources = 1,
		.id = 0,
		.resource = (struct resource[]) {
                        STM_PLAT_RESOURCE_MEM(0xFD800000, 0x1000),
                },

	},
	[1] = {
		/* This LX corresponds to AUD0 */
		.name = "stm-lx-cpu",
		.num_resources = 1,
		.id = 1,
		.resource = (struct resource[]) {
                        STM_PLAT_RESOURCE_MEM(0xFDA00000, 0x1000),
                },

	},
	[2] = {
		/* This LX corresponds to GP0 */
		.name = "stm-lx-cpu",
		.num_resources = 1,
		.id = 2,
		.resource = (struct resource[]) {
                        STM_PLAT_RESOURCE_MEM(0xFDB00000, 0x1000),
                },

	},
	[3] = {
		/* This LX corresponds to GP1 */
		.name = "stm-lx-cpu",
		.num_resources = 1,
		.id = 3,
		.resource = (struct resource[]) {
                        STM_PLAT_RESOURCE_MEM(0xFDC00000, 0x1000),
                },

	}
};
static void cec_release(struct device *device)
{

/* TO BE DEFINED */

}

void  stih315_configure_cec(uint32_t dev_id)
{
       platform_device_register(&stih315_cec_devices[dev_id]);
}

void  stih315_unconfigure_cec(uint32_t dev_id)
{
	platform_device_unregister(&stih315_cec_devices[dev_id]);
}

void cec_configure (void)
{
	stih315_configure_cec(0);
}

void cec_unconfigure (void)
{
	stih315_unconfigure_cec(0);
}

void lx_cpu_configure(void)
{
	clk_add_alias("lx_cpu_clk_0", "stm-lx-cpu.0", "CLK_M_ST231_DMU_0",  NULL);
        clk_add_alias("lx_cpu_clk_1", "stm-lx-cpu.0", "CLK_M_VID_DMU_0",    NULL);
        clk_add_alias("lx_cpu_clk_2", "stm-lx-cpu.0", "CLK_M_RX_ICN_DMU_0", NULL);
        clk_add_alias("lx_cpu_clk_3", "stm-lx-cpu.0", "CLK_M_ICN_ST231",    NULL);

        clk_add_alias("lx_cpu_clk_0", "stm-lx-cpu.1", "CLK_M_ST231_AUD", NULL);
        clk_add_alias("lx_cpu_clk_1", "stm-lx-cpu.1", "CLK_M_ICN_ST231", NULL);

        clk_add_alias("lx_cpu_clk_0", "stm-lx-cpu.2", "CLK_M_ST231_GP_0", NULL);
        clk_add_alias("lx_cpu_clk_1", "stm-lx-cpu.2", "CLK_M_ICN_ST231",  NULL);

        clk_add_alias("lx_cpu_clk_0", "stm-lx-cpu.3", "CLK_M_ST231_GP_1", NULL);
        clk_add_alias("lx_cpu_clk_1", "stm-lx-cpu.3", "CLK_M_ICN_ST231",  NULL);

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
