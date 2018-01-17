/*
 * Copyright (C) 2010 Broadcom Corporation
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef _USB_BRCM_H
#define _USB_BRCM_H

#include <linux/usb.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/usb/hcd.h>

/* force non-byteswapping reads/writes on LE and BE alike */
#define CONFIG_USB_EHCI_BIG_ENDIAN_MMIO		1
#define CONFIG_USB_OHCI_BIG_ENDIAN_MMIO		1
#undef readl_be
#undef writel_be
#define readl_be(x)				__raw_readl(x)
#define writel_be(x, y)				__raw_writel(x, y)

extern int brcm_usb_probe(struct platform_device *pdev,
			const struct hc_driver *hc_driver,
			struct usb_hcd **hcdptr,
			struct clk **hcd_clk_ptr);
extern int brcm_usb_remove(struct platform_device *pdev, struct clk *hcd_clk);

#endif /* _USB_BRCM_H */
