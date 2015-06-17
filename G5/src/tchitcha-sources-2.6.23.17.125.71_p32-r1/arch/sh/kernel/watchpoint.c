/*
 * arch/sh/kernel/watchpoint.c - hardware watchpoint support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Copyright (C) STMicroelectronics, 2009
 *
 */
#include <asm/watchpoint.h>
#include <asm/ubc.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/module.h>

#ifdef CONFIG_WATCHPOINTS

struct hw_watchpoint __current_hw_watch;

static unsigned short last_enabled_state;

/* Initalize a hardware watchpoint structure */
void hw_watch_init(struct hw_watchpoint *wp)
{
	memset(wp, 0, sizeof(struct hw_watchpoint));
}
EXPORT_SYMBOL(hw_watch_init);

/* Disable the current hardware watchpoint */
void disable_hw_watch(void)
{
	last_enabled_state = ctrl_inw(UBC_BBRB);
	ctrl_outw(0, UBC_BBRB);
	ctrl_inw(UBC_BBRB); /* Flush */
}
EXPORT_SYMBOL(disable_hw_watch);

/* Re-enable a disabled hardware watchpoint */
void reenable_hw_watch(void)
{
	ctrl_outw(last_enabled_state, UBC_BBRB);
	ctrl_inw(UBC_BBRB); /* Flush */
}
EXPORT_SYMBOL(reenable_hw_watch);

/* Add a hardware watchpoint - only one can be set at a time */
int add_hw_watch(struct hw_watchpoint *wp)
{
	u16 brcr = BRCR_DBEB;
	brcr &= ~(BRCR_SEQ | BRCR_UBDE);

	if (!wp) {
		printk(KERN_ERR "watchpoint.c: NULL watchpoint passed to "
						"add_hw_watch()\n");
		return 1;
	}

	if ((wp->addr_mask != 0) && (wp->addr_mask != 0xffffffff)) {
		printk(KERN_ERR "watchpoint.c: Watchpoints address mask must "
						"be 0 or 0xffffffff\n");
		return 1;
	}

	if ((!wp->addr) && (!wp->addr_mask)) {
		printk(KERN_ERR "watchpoint.c: Watchpoints must be set "
						"on a specific address\n");
		return 1;
	}

	if (!wp->rw) {
		printk(KERN_ERR "watchpoint.c: Cannot set watchpoint with no "
						"access type (e.g. WP_READ)\n");
		return 1;
	}

	disable_hw_watch();

	/* Address to match */
	ctrl_outl(wp->addr, UBC_BARB);

	/* Address mask: exact address match unless mask specified,
	 * and no ASID matching */
	ctrl_outb((wp->addr_mask ? BAMR_ALL : BAMR_NONE) | BAMR_ASID,
		  UBC_BAMRB);

	/* Enable data break, non-sequential, disable user break function */
	ctrl_outw(brcr, UBC_BRCR);

	if (wp->val) {
		/* Value to match */
		ctrl_outl(wp->val, UBC_BDRB);

		/* Data mask: exact data match */
		ctrl_outl(0, UBC_BDMRB);
	} else  {
		/* Data mask: no data match */
		ctrl_outl(~0, UBC_BDMRB);
	}

	__current_hw_watch = *wp;

	/* Set read/write/access breakpoint according to wp->rw */
	ctrl_outw(wp->rw | BBR_DATA | (wp->addr_mask ? BBR_LONG : 0), UBC_BBRB);

	return 0;
}
EXPORT_SYMBOL(add_hw_watch);

#endif
