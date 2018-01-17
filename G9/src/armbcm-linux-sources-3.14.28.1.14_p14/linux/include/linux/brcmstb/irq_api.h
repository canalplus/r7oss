/*
 * Copyright © 2015-2016 Broadcom
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2, as
 * published by the Free Software Foundation (the "GPL").
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * A copy of the GPL is available at
 * http://www.broadcom.com/licenses/GPLv2.php or from the Free Software
 * Foundation at https://www.gnu.org/licenses/ .
 */

#ifndef _BRCMSTB_IRQ_API_H
#define _BRCMSTB_IRQ_API_H

/* List of L2 IRQ's handled by the Linux */
typedef enum brcmstb_l2_irq {
	brcmstb_l2_irq_gio,
	brcmstb_l2_irq_gio_aon,
	brcmstb_l2_irq_iica,
	brcmstb_l2_irq_iicb,
	brcmstb_l2_irq_iicc,
	brcmstb_l2_irq_iicd,
	brcmstb_l2_irq_iice,
	brcmstb_l2_irq_iicf,
	brcmstb_l2_irq_iicg,
	brcmstb_l2_irq_irb,
	brcmstb_l2_irq_icap,
	brcmstb_l2_irq_kbd1,
	brcmstb_l2_irq_kbd2,
	brcmstb_l2_irq_kbd3,
	brcmstb_l2_irq_ldk,
	brcmstb_l2_irq_spi,
	brcmstb_l2_irq_ua,
	brcmstb_l2_irq_ub,
	brcmstb_l2_irq_uc,
	brcmstb_l2_irq_max
} brcmstb_l2_irq;

int brcmstb_get_l2_irq_id(brcmstb_l2_irq irq);

#endif /* _BRCMSTB_IRQ_API_H */
