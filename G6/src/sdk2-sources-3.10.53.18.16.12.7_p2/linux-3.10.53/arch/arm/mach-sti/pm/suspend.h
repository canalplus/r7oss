/*
 * -------------------------------------------------------------------------
 * Copyright (C) 2014  STMicroelectronics
 * Author: Francesco M. Virlinzi  <francesco.virlinzi@st.com>
 *	   Sudeep Biswas	  <sudeep.biswas@st.com>
 *	   Laurent MEUNIER	  <laurent.meunier@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License V.2 ONLY.  See linux/COPYING for more information.
 * ------------------------------------------------------------------------- */

#ifndef __STI_SUSPEND_H__
#define __STI_SUSPEND_H__

#include <linux/suspend.h>
#include <linux/list.h>

#define STI_SUSPEND_DESC_LEN 32

#define MAX_SUSPEND_STATE 2

#define MAX_GIC_SPI_INT 255

#define MAX_DDR_PCTL_NUMBER 2

/*  detailed DDR (lmi) possible states */
enum ddr_low_power_state {
	DDR_ON,
	DDR_SR,
	DDR_OFF
};

enum clocks_state {
	CLOCKS_ON,
	CLOCK_REDUCED,
	CLOCKS_OFF /* clocks off also means power off */
};

struct sti_suspend_table {
	unsigned long base_address;
	long *enter;
	unsigned long enter_size;
	long *exit;
	unsigned long exit_size;
	struct list_head node;
};

struct sti_suspend_eram_data {
	void *pa_table_enter;
	void *pa_table_exit;
	void *pa_sti_eram_code;
	void *pa_pokeloop;
};

struct sti_platform_suspend {
	struct sti_hw_state_desc *hw_states;
	int hw_state_nr;
	int index;
	unsigned long eram_base;
	struct platform_suspend_ops const ops;
};
/**
 * struct sti_low_power_syscfg_info - low power entry fields for secure chip
 * @ddr3_cfg_offset:	DDR3_0 Config Register offset in SYSCFG_CORE
 * @ddr3_stat_offset:	DDRSS 0 Status Register offset in SYSCFG_CORE
 * @cfg_shift:		DDR3SS0_PCTL_C_SYSREQ bit shift in DDR3_0 Config Reg
 * @stat_shift:		DDR3SS0_PCTL_C_SYSACK bit shift in DDRSS 0 Status Reg
 */
struct sti_low_power_syscfg_info {
	int ddr3_cfg_offset;
	int ddr3_stat_offset;
	int cfg_shift;
	int stat_shift;
};

struct sti_ddr3_low_power_info {
	struct sti_low_power_syscfg_info sysconf_info;
	unsigned long sysconf_base;
};

struct sti_hw_state_desc {
	int target_state;
	char desc[STI_SUSPEND_DESC_LEN];
	enum ddr_low_power_state  ddr_state;
	enum clocks_state clock_state;
	struct list_head state_tables;
	void __iomem *eram_vbase;
	void __iomem *eram_state_code_base;
	struct sti_suspend_eram_data eram_data;
	int (*setup)(struct sti_hw_state_desc *,
		      unsigned long *,
		      unsigned int,
		      unsigned long,
		      struct sti_ddr3_low_power_info *);
	int (*enter)(struct sti_hw_state_desc *);
	int (*prepare)(struct sti_hw_state_desc *);
	int (*begin)(struct sti_hw_state_desc *);
	void *state_private_data;
};

/* declaration of the entry functions for HPS mode */
int sti_hps_setup(struct sti_hw_state_desc *, unsigned long *, unsigned int,
		  unsigned long, struct sti_ddr3_low_power_info *);

int sti_hps_enter(struct sti_hw_state_desc *);

int sti_hps_prepare(struct sti_hw_state_desc *);

/* declaration of the entry functions for CPS mode */
int sti_cps_setup(struct sti_hw_state_desc *, unsigned long *, unsigned int,
		  unsigned long, struct sti_ddr3_low_power_info *);
int sti_cps_enter(struct sti_hw_state_desc *);

int sti_cps_prepare(struct sti_hw_state_desc *);

int sti_cps_begin(struct sti_hw_state_desc *);

#endif

