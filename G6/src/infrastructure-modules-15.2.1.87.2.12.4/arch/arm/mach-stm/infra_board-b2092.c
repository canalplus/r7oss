#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/init.h> /* Initilisation support */
#include <linux/kernel.h> /* Kernel support */

#include <linux/stm/pad.h>
#include <linux/stm/platform.h>
#include <linux/stm/stih416.h>
#include <linux/stm/stih415-periphs.h>

#include "infra_platform.h"

static void cec_release(struct device *device);
static void stih416_unconfigure_cec(uint32_t dev_id);
static void stih416_configure_cec(uint32_t dev_id);

static struct stm_pad_config stih416_cec_pad_configs[] = {
	/*CEC block in SBC */
	[0] = {
		/* CEC_HDMI PIO2[4]*/
		.gpios_num = 1,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_BIDIR(2, 4, 1),       /* CEC_HDMI*/
		},
	}
};

static struct platform_device stih416_cec_devices[] = {
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
			.pad_config = &stih416_cec_pad_configs[0],
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
static void cec_release(struct device *device)
{
	/* Dummy function for releasing the  platform device */
	/* Doing nothing but needed */
}

void  stih416_configure_cec(uint32_t dev_id) /* To be called from setup.c*/
{
       platform_device_register(&stih416_cec_devices[dev_id]);
}

void  stih416_unconfigure_cec(uint32_t dev_id) /* To be called from setup.c*/
{
	platform_device_unregister(&stih416_cec_devices[dev_id]);
}


void cec_configure (void)
{
	stih416_configure_cec(0);
}

void cec_unconfigure (void)
{
	stih416_unconfigure_cec(0);
}

