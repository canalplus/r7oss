/************************************************************************
Copyright (C) 2011 STMicroelectronics. All Rights Reserved.

This file is part of the Transport Engine Library.

The Transport Engine is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Transport Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Transport Engine; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
USA.

The Transport Engine Library may alternatively be licensed under a
proprietary license from ST.

 ************************************************************************/
/**
 * @file pti_platform.h
 * @brief Defines structures for supplying TP and STFE hardware configuration
 * parameters to platform devices
 */

#ifndef _PTI_PLATFORM_H
#define _PTI_PLATFORM_H

#include <linux/err.h>
#include <linux/types.h>
#include <linux/clk.h>
#include "pti_dt.h"
#include "pti_tshal_api.h"

/* Describes a clock used by STPTI device */
struct stpti_clk {
	struct clk *clk;
	const char *name;
	/* Set to a non-zero if we need to force the clock frequency.
	 * If zero we leave the clock frequency as configured by the system
	 */
	uint32_t freq;

	struct clk *parent_clk;
	const char *parent_name;
	int enable_count;
};

/* Describes a Tango TP device */
struct stpti_tp_config {
	/* TP memory map */
	uint32_t ddem_offset;
	uint32_t idem_offset;
	uint32_t st_bux_plug_write_offset;
	uint32_t st_bux_plug_read_offset;
	uint32_t core_ctrl_offset;
	uint32_t mailbox0_to_xp70_offset;
	uint32_t mailbox0_to_host_offset;
	uint32_t writelock_error_offset;
	uint32_t t3_addr_filter_offset;
	uint32_t timer_counter_offset;
	uint32_t power_down;
	const char *firmware;

	/* Firmware configuration (specific to a TP) */
	int nb_vdevice;
	int nb_slot;
	int nb_section_filter;
	int nb_dma_structure;
	int nb_indexer;
	int nb_status_blk;

	/* Enable secure coprocessor bypass */
	bool sc_bypass;

	/* Timer counter for timestamps */
	uint32_t timer_counter_divider;

	/* general driver features affected by platform */
	bool permit_powerdown;

	/* Use software leaky pid */
	bool software_leaky_pid;
	uint32_t software_leaky_pid_timeout;

	/* Clocks used by this TP device */
	struct stpti_clk *clk;
	uint32_t nb_clk;

	/* HAL pdevice ID */
	uint32_t id;
};

/* Struct for describing an STFE device */
#define STFE_V1 (0)
#define STFE_V2 (1)
struct stpti_stfe_config {
	uint32_t stfe_version;
	uint32_t nb_ib;
	uint32_t nb_mib;
	uint32_t nb_swts;
	uint32_t nb_tsdma;
	uint32_t nb_ccsc;
	uint32_t nb_tag;

	uint32_t ib_offset;
	uint32_t tag_offset;
	uint32_t pid_filter_offset;
	uint32_t system_regs_offset;
	uint32_t memdma_offset;
	uint32_t tsdma_offset;
	uint32_t ccsc_offset;
	uint32_t power_down;
	stptiTSHAL_TSInputDestination_t default_dest;

	const char *firmware;

	/* Clocks used by this STFE device */
	struct stpti_clk *clk;
	uint32_t nb_clk;
	bool stfe_ccsc_clk_enabled;
};

/* Helper function to initialise clk pointers in stpti_clk array */
static inline void get_clks(const char *devname, struct stpti_clk *clks,
		uint32_t nb_clk)
{
	int i;

	for (i = 0; i < nb_clk; i++) {
#ifndef CONFIG_ARCH_STI
		clks[i].clk = clk_get_sys(devname, clks[i].name);
#endif
		if (IS_ERR(clks[i].clk)) {
			pr_warning("%s: Clock %s not found", devname,
					clks[i].name);
			clks[i].clk = NULL;
		}
		if (clks[i].parent_name) {
			clks[i].parent_clk = clk_get_sys(devname,
					clks[i].parent_name);
			if (IS_ERR(clks[i].parent_clk)) {
				pr_warning("%s :parent %s not found\n",
				devname, clks[i].parent_name);
				clks[i].parent_clk = NULL;
			}
		}

	}
};

static inline void put_clks(struct stpti_clk *clks, uint32_t nb_clk)
{
	int i;
	for (i = 0; i < nb_clk; i++) {
		if (clks[i].clk) {
			clk_put(clks[i].clk);
			clks[i].clk = NULL;
			if (clks[i].parent_clk) {
				clk_put(clks[i].parent_clk);
				clks[i].parent_clk = NULL;
			}
		}
	}
}

static inline void dummy_release(struct device *dev)
{
}

extern int stptiAPI_register_devices(void);
extern void stptiAPI_unregister_devices(void);

#endif
