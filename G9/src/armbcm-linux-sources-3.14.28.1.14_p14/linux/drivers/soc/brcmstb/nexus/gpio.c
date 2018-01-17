/*
 * Nexus GPIO(s) resolution API
 *
 * Copyright (C) 2015-2016, Broadcom Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * A copy of the GPL is available at
 * http://www.broadcom.com/licenses/GPLv2.php or from the Free Software
 * Foundation at https://www.gnu.org/licenses/ .
 */

#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/gpio.h>
#include <linux/bitmap.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/basic_mmio_gpio.h>

#include <linux/brcmstb/brcmstb.h>
#include <linux/brcmstb/gpio_api.h>

#include "gpio.h"
#define GIO_DATA_OFFSET 4
#define GIO_DIR_OFFSET	8

static const char *brcmstb_gpio_compat = GPIO_DT_COMPAT;

static int brcmstb_gpio_chip_find(struct gpio_chip *chip, void *data)
{
	struct device_node *dn = data;

	if (chip->of_node == dn)
		return 1;

	return 0;
}

/* Keep an additional bitmap of which GPIOs are requested by Nexus to
 * maintain Linux/Nexus GPIO exclusive ownership managed via gpio_request().
 * We do want multiple consecutive calls to brcmstb_gpio_irq() not to fail
 * because the first one claimed ownernship already.
 */
static DECLARE_BITMAP(brcmstb_gpio_requested, CONFIG_ARCH_NR_GPIO);

static int brcmstb_gpio_request(unsigned int gpio)
{
	int ret = 0;

	/* Request the GPIO to flag that Nexus now owns it, but
	 * also keep it requested in our local bitmap to avoid
	 * subsequent gpio_request() calls to the same GPIO
	 */
	if (!test_bit(gpio, brcmstb_gpio_requested)) {
		ret = gpio_request(gpio, "nexus");
		if (ret) {
			pr_err("%s: GPIO request failed\n", __func__);
			return ret;
		}

		/* We could get ownership, so flag it now */
		set_bit(gpio, brcmstb_gpio_requested);
	}

	return ret;
}

static int brcmstb_gpio_find_base_by_addr(uint32_t addr, phys_addr_t *start)
{
	struct device_node *dn;
	struct gpio_chip *gc;
	struct resource res;
	int ret;

	for_each_compatible_node(dn, NULL, brcmstb_gpio_compat) {
		ret = of_address_to_resource(dn, 0, &res);
		if (ret) {
			pr_err("%s: unable to translate resource\n", __func__);
			continue;
		}

		if (res.flags != IORESOURCE_MEM) {
			pr_err("%s: invalid resource type\n", __func__);
			continue;
		}

		/* Mask address with the physical offset, since the address has
		 * already been translated based on the busing hierarchy
		 */
		res.start &= ~BCHP_PHYSICAL_OFFSET;
		res.end &= ~BCHP_PHYSICAL_OFFSET;

		/* Verify address in the resource range, if not, go to the
		 * other GPIO controllers
		 */
		if (addr < res.start || addr >= res.end)
			continue;

		gc = gpiochip_find(dn, brcmstb_gpio_chip_find);
		if (!gc) {
			pr_err("%s: unable to find gpio chip\n", __func__);
			continue;
		}

		if (start)
			*start = (phys_addr_t)res.start;

		return gc->base;
	}

	return -ENODEV;

}

static int brcmstb_gpio_find_by_addr(uint32_t addr, unsigned int shift)
{
	int gpio, gpio_base;
	phys_addr_t start, bank_offset;

	gpio_base = brcmstb_gpio_find_base_by_addr(addr, &start);
	if (gpio_base >= 0) {
		/* Now find out what GPIO bank this pin belongs to */
		bank_offset = ((phys_addr_t)addr - start) / GIO_BANK_SIZE;

		gpio = gpio_base + shift + bank_offset * GPIO_PER_BANK;

		pr_debug("%s: xlate base=%d, offset=%d, shift=%d, gpio=%d\n",
			__func__, (int)gpio_base, (int)bank_offset, shift,
			(int)(gpio - gpio_base));

		return gpio;
	}

	return gpio_base;
}

int brcmstb_gpio_update32(uint32_t addr, uint32_t mask, uint32_t value)
{
	struct bgpio_chip *bgc;
	struct gpio_chip *gc;
	int ret, bit, gpio_base;
	phys_addr_t start, offset, bank_offset;
	unsigned long flags;
	uint32_t ivalue;
	void __iomem *reg;

	gpio_base = brcmstb_gpio_find_base_by_addr(addr, &start);
	if (gpio_base < 0) {
		pr_err("%s: addr is not in GPIO range\n", __func__);
		return -EPERM;
	}

	/* Now find out what GPIO bank this pin belongs to */
	offset = (phys_addr_t)addr - start;
	bank_offset = offset / GIO_BANK_SIZE;
	offset -= bank_offset * GIO_BANK_SIZE;

	pr_debug("%s: xlate base=%d, offset=%d, gpio=%d\n",
		__func__, (int)gpio_base, (int)bank_offset,
		(int)(bank_offset * GPIO_PER_BANK));

	gpio_base += bank_offset * GPIO_PER_BANK;

	for (bit = 0; bit < GPIO_PER_BANK; bit++) {
		/* Ignore bits which are not in mask */
		if (!((1 << bit) & mask))
			continue;

		ret = brcmstb_gpio_request(gpio_base + bit);
		if (ret < 0) {
			pr_err("%s: unable to request gpio %d\n",
				__func__, gpio_base + bit);
			return ret;
		}
	}

	/* We got full access to the entire mask, do the write */

	pr_info("%s: offset=0x%08x mask=0x%08x, value=0x%08x\n",
		__func__, addr, mask, value);

	gc = gpiod_to_chip(gpio_to_desc(gpio_base));
	if (gc == NULL) {
		pr_err("%s: unable to resolve gpio chip\n", __func__);
		return -EPERM;
	}
	bgc = to_bgpio_chip(gc);
	reg = bgc->reg_dat;
	if (reg == 0) {
		pr_err("%s: unable to resolve GIO mapped address\n", __func__);
		return -EPERM;
	}

	spin_lock_irqsave(&bgc->lock, flags);
	ivalue = bgc->read_reg(reg + offset - GIO_DATA_OFFSET);
	ivalue &= ~(mask);
	ivalue |= (value & mask);

	/* update shadows */
	switch (offset) {
	case GIO_DATA_OFFSET:
		bgc->data = ivalue;
		break;
	case GIO_DIR_OFFSET:
		bgc->dir = ivalue;
		break;
	default:
		break;
	}

	bgc->write_reg(reg + offset - GIO_DATA_OFFSET, ivalue);
	spin_unlock_irqrestore(&bgc->lock, flags);

	return 0;
}

int brcmstb_gpio_irq(uint32_t addr, unsigned int shift)
{
	int gpio, ret;

	gpio = brcmstb_gpio_find_by_addr(addr, shift);
	if (gpio < 0)
		return gpio;

	ret = brcmstb_gpio_request(gpio);
	if (ret < 0)
		return ret;

	ret = gpio_to_irq(gpio);
	if (ret < 0) {
		gpio_free(gpio);
		pr_err("%s: unable to map GPIO%d to irq, ret=%d\n",
		       __func__, gpio, ret);
	}

	return ret;
}
EXPORT_SYMBOL(brcmstb_gpio_irq);

static void brcmstb_gpio_free(unsigned int gpio)
{
    if (test_bit(gpio, brcmstb_gpio_requested)) {
        gpio_free(gpio);
        clear_bit(gpio, brcmstb_gpio_requested);
    }
}

void brcmstb_gpio_remove(void)
{
    unsigned i;

    for (i = 0; i < CONFIG_ARCH_NR_GPIO; i++)
    {
        brcmstb_gpio_free(i);
    }
}
EXPORT_SYMBOL(brcmstb_gpio_remove);
