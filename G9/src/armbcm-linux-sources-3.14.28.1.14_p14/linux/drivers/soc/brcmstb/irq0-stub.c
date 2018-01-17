/*
 * Copyright (C) 2015 Broadcom Corporation
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

#include <linux/brcmstb/brcmstb.h>
#include <linux/syscore_ops.h>
#include <soc/brcmstb/common.h>

static void __brcmstb_irq0_init(void)
{
	BDEV_WR(BCHP_IRQ0_IRQEN, BCHP_IRQ0_IRQEN_uarta_irqen_MASK
		| BCHP_IRQ0_IRQEN_uartb_irqen_MASK
		| BCHP_IRQ0_IRQEN_uartc_irqen_MASK
	);
}

#ifdef CONFIG_PM_SLEEP
static struct syscore_ops brcmstb_irq0_syscore_ops = {
	.resume         = __brcmstb_irq0_init,
};
#endif

static int brcmstb_irq0_init(void)
{
	if (!soc_is_brcmstb())
		return 0;

	__brcmstb_irq0_init();
#ifdef CONFIG_PM_SLEEP
	register_syscore_ops(&brcmstb_irq0_syscore_ops);
#endif

	return 0;
}
early_initcall(brcmstb_irq0_init);
