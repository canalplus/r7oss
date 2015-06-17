#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/init.h>    /* Initilisation support */
#include <linux/kernel.h>  /* Kernel support */

#include "infra_platform.h"

MODULE_AUTHOR("");
MODULE_DESCRIPTION("STMicroelectronics arch module");
MODULE_LICENSE("GPL");

void arch_modprobe_func(void)
{
	pr_info("This is INFRA's ARCH Module\n");
}
EXPORT_SYMBOL(arch_modprobe_func);

static int __init arch_init_module(void)
{
	pr_info("Load the ARCH Module %s\n", __FUNCTION__);
#ifndef CONFIG_OF
	lx_cpu_configure();
	cec_configure();
#endif
	return 0;
}
static void __exit arch_exit_module(void)
{
	pr_info("Unload the ARCH Module %s\n", __FUNCTION__);
#ifndef CONFIG_OF
	lx_cpu_unconfigure();
	cec_unconfigure();
#endif
}

module_init(arch_init_module);
module_exit(arch_exit_module);
