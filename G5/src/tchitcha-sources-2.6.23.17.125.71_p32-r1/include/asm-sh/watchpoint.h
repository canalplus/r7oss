#ifndef ASM_SH_WATCHPOINT_H
#define ASM_SH_WATCHPOINT_H

/*
 * include/asm-sh/watchpoint.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
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

#include <linux/ptrace.h>
#include <linux/types.h>
#include <asm/ubc.h>

#ifdef CONFIG_WATCHPOINTS

/* Watchpoint access types */
#define WP_READ 		BBR_READ
#define WP_WRITE 		BBR_WRITE
#define WP_ACCESS 		BBR_READ | BBR_WRITE

struct hw_watchpoint
{
	u32 addr;			/* Address to match */
	u32 addr_mask;			/* Address mask */
	u32 val;			/* Value to match (optional) */
	u32 rw;				/* An access type, e.g. WP_WRITE */
	u32 oneshot;		/* If set to 1, the watchpoint will be disabled
						   after it is first hit */

	/* User handler to be called when the watchpoint is hit.
	 * Optional; by default, the stack and regs will be dumped
	 */
	void (*handler) (unsigned long r4, unsigned long r5,
			 unsigned long r6, unsigned long r7,
			 struct pt_regs *regs);
};

/* Initialize a hardware watchpoint structure before use */
void hw_watch_init(struct hw_watchpoint *wp);

/* Add a hardware watchpoint - only one can be set at a time*/
int add_hw_watch(struct hw_watchpoint *wp);

/* Disable the current hardware watchpoint */
void disable_hw_watch(void);

/* Re-enable a disabled hardware watchpoint */
void reenable_hw_watch(void);

extern struct hw_watchpoint __current_hw_watch;

#endif

#endif
