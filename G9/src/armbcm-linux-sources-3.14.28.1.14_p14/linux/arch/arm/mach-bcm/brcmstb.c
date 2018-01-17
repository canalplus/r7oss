/*
 * Copyright (C) 2013 Broadcom Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/clk-provider.h>
#include <linux/console.h>
#include <linux/clocksource.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dma-contiguous.h>
#include <linux/errno.h>
#include <linux/export.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/jiffies.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/printk.h>
#if defined(CONFIG_BRCMSTB)
#include <linux/brcmstb/bmem.h>
#include <linux/brcmstb/brcmstb.h>
#include <linux/brcmstb/cma_driver.h>
#endif
#include <linux/clk/clk-brcmstb.h>
#include <linux/phy.h>
#include <linux/phy_fixed.h>
#include <linux/irqchip.h>
#include <linux/irqchip/arm-gic.h>
#include <linux/soc/brcmstb/brcmstb.h>
#include <asm/cacheflush.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/map.h>
#include <asm/mach/time.h>
#include <asm/setup.h>

/***********************************************************************
 * STB CPU (main application processor)
 ***********************************************************************/

static const char *brcmstb_match[] __initconst = {
	"brcm,bcm7445",
	"brcm,brcmstb",
	NULL
};

#if defined(CONFIG_BRCMSTB)
/*
 * HACK: The following drivers are still using BDEV macros:
 * - XPT DMA
 * - SPI
 * - NAND
 * - SDHCI
 * - MoCA
 *
 * Once these drivers have migrated over to using 'of_iomap()' and standard
 * register accessors, we can eliminate this static mapping.
 */
static struct map_desc brcmstb_io_map[] __initdata = {
	{
	.virtual = (unsigned long)BRCMSTB_PERIPH_VIRT,
	.pfn     = __phys_to_pfn(BRCMSTB_PERIPH_PHYS),
	.length  = BRCMSTB_PERIPH_LENGTH,
	.type    = MT_DEVICE,
	},
};

static void __init brcmstb_map_io(void)
{
	iotable_init(brcmstb_io_map, ARRAY_SIZE(brcmstb_io_map));
}

static void __init brcmstb_reserve(void)
{
	brcmstb_memory_reserve();
	cma_reserve();
	bmem_reserve();
}

static void __init brcmstb_init_irq(void)
{
	/* Force lazily-disabled IRQs to be masked before suspend */
	gic_arch_extn.flags |= IRQCHIP_MASK_ON_SUSPEND;

	irqchip_init();
}

static void __init brcmstb_init_machine(void)
{
	struct platform_device_info devinfo = { .name = "cpufreq-cpu0", };

	of_platform_populate(NULL, of_default_bus_match_table, NULL, NULL);
	cma_register();
	platform_device_register_full(&devinfo);
}

static void __init brcmstb_init_early(void)
{
	brcmstb_biuctrl_init();
#ifdef CONFIG_SERIAL_CORE_CONSOLE
	add_preferred_console("ttyS", 0, "115200");
#endif
}

static void __init brcmstb_init_time(void)
{
	brcmstb_clocks_init();
	clocksource_of_init();
}
#endif /* CONFIG_BRCMSTB */

DT_MACHINE_START(BRCMSTB, "Broadcom STB (Flattened Device Tree)")
	.dt_compat	= brcmstb_match,
#if defined(CONFIG_BRCMSTB)
	.map_io		= brcmstb_map_io,
	.reserve	= brcmstb_reserve,
	.init_machine	= brcmstb_init_machine,
	.init_early	= brcmstb_init_early,
	.init_irq	= brcmstb_init_irq,
	.init_time	= brcmstb_init_time,
#endif
MACHINE_END
