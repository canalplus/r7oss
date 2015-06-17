#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/init.h>    /* Initilisation support */
#include <linux/kernel.h>  /* Kernel support */
#include <asm/irq-ilc.h>

#include <linux/stm/pad.h>
#include <linux/stm/platform.h>

#include "infra_platform.h"

void  stx7108_configure_cec(int dev_num) /* To be called from setup.c*/
{

}

void cec_configure (void)
{
	stx7108_configure_cec(0);
}


void cec_unconfigure(void)
{
	//platform_device_unregister(&stx7108_cec_devices[0]);
}

