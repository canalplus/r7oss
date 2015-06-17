/*
 * -------------------------------------------------------------------------
 * Copyright (C) 2014  STMicroelectronics
 * Author:	Sudeep Biswas		<sudeep.biswas@st.com>
 *		Francesco M. Virlinzi	<francesco.virlinzi@st.com>
 *		Laurent MEUNIER		<laurent.meunier@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License V.2 ONLY.  See linux/COPYING for more information.
 * ------------------------------------------------------------------------- */

#include <linux/irq.h>
#include <linux/irqchip/arm-gic.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>

#include "suspend.h"
#include "poke_table.h"

/* List of functions implementing the suspend_ops */
static int sti_suspend_valid(suspend_state_t state);
static int sti_suspend_begin(suspend_state_t state);
static int sti_suspend_prepare(void);
static int sti_suspend_enter(suspend_state_t state);

/* States definitions */
/*
 * Currently three low power suspend modes
 * are supported: freeze, standby and mem.
 * freeze support is in-build in kernel and no
 * support from platform power code is required.
 * standby and mem require platfrom code support.
 * They require explicit callback support.
 * It is to be noted that standby is mapped to HPS
 * and mem is mapped to CPS.
 */
struct sti_hw_state_desc sti_hw_pltf_states[MAX_SUSPEND_STATE],
	sti_hw_states[MAX_SUSPEND_STATE] = {
	{
		.target_state = PM_SUSPEND_STANDBY,
		.desc = "HOST PASSIVE STANDBY",
		.setup = sti_hps_setup,
		.enter = sti_hps_enter,
		.prepare = sti_hps_prepare,
		.clock_state = CLOCK_REDUCED,
		.ddr_state = DDR_SR,
	},
	{
		.target_state = PM_SUSPEND_MEM,
		.desc =	"CONTROLLER PASSIVE STANDBY",
		.setup = sti_cps_setup,
		.enter = sti_cps_enter,
		.begin = sti_cps_begin,
		.prepare = sti_cps_prepare,
		.clock_state = CLOCKS_OFF,
		.ddr_state = DDR_SR,
	},
};

struct sti_platform_suspend sti_suspend = {
	.hw_states = sti_hw_pltf_states,
	.hw_state_nr = 0,
	.index = -1,
	.ops.valid = sti_suspend_valid,
	.ops.begin = sti_suspend_begin,
	.ops.prepare_late = sti_suspend_prepare,
	.ops.enter = sti_suspend_enter,
};

static const unsigned long suspend_end_table[4] = { OP_END_POKES, 0, 0, 0 };

enum tbl_level {
	TBL_ENTER,
	TBL_EXIT
};

/*
 * The below object is defined for STiH407 family SoCs.
 * This will differ for other SoCs such as ORLY, etc.
 * Support added for only STiH407 family SoCs
 */
static const struct sti_low_power_syscfg_info ddr3_syscfg_info = {
	.ddr3_cfg_offset = 0x160,
	.ddr3_stat_offset = 0x920,
	.cfg_shift = 0,
	.stat_shift = 0,
};

static struct of_device_id suspend_of_match[] = {
	/* For STiH407 family SoCs */
	{
		.compatible = "st,stih407-ddr-controller",
		.data = &ddr3_syscfg_info,
	},
	/* Add compatible and data here and in the DT to support other SoCs */
	{}
};

static int sti_suspend_valid(suspend_state_t state)
{
	int i;

	for (i = 0; i < sti_suspend.hw_state_nr; i++) {
		/*  check if the right target state */
		if (state == sti_suspend.hw_states[i].target_state) {
			pr_info("sti pm: support mode: %s\n",
				sti_suspend.hw_states[i].desc);
			return true;
		}
	}

	/*  if not found, then not valid must return false */
	return false;
}

static int sti_suspend_begin(suspend_state_t state)
{
	int i;

	sti_suspend.index = -1;

	for (i = 0; i < sti_suspend.hw_state_nr; i++) {
		/*  check if the right target state */
		if (state == sti_suspend.hw_states[i].target_state) {
			sti_suspend.index = i;
			return 0;
		}
	}

	return -EINVAL;
}

static void sti_copy_suspend_table(struct sti_suspend_table *table,
				   void **__va_add,
				   void **__pa_add,
				   enum tbl_level level)
{
	unsigned long const *tbl_ptr;
	unsigned long tbl_len_byte;

	if (level == TBL_ENTER) {
		tbl_ptr = table->enter;
		tbl_len_byte = table->enter_size;
	} else {
		tbl_ptr = table->exit;
		tbl_len_byte = table->exit_size;
	}

	/*
	 * if table->base_address is zero, this means no patching
	 * of poke table required while copying to eram. Poke table
	 * is already set.
	 */
	if (!table->base_address)
		memcpy(*__va_add, tbl_ptr, tbl_len_byte);
	else
		patch_poke_table_copy(*__va_add, tbl_ptr,
				      table->base_address,
				      tbl_len_byte/sizeof(long));

	*__va_add += tbl_len_byte;
	*__pa_add += tbl_len_byte;
}

static void sti_insert_poke_table_end(void **__va_add, void **__pa_add)
{
	memcpy(*__va_add, suspend_end_table,
	       ARRAY_SIZE(suspend_end_table) * sizeof(long));

	*__va_add += ARRAY_SIZE(suspend_end_table) * sizeof(long);
	*__pa_add += ARRAY_SIZE(suspend_end_table) * sizeof(long);
}

static int sti_suspend_prepare()
{
	int index;
	int ret;

	struct sti_suspend_table *table;
	void *__pa_eram;
	void *__va_eram;

	if (sti_suspend.index == -1)
		return -EINVAL;

	index = sti_suspend.index;

	pr_info("sti pm: Copying poke table data and poke loop to eram\n");

	if (sti_suspend.hw_states[index].begin) {
		ret = sti_suspend.hw_states[index].
			begin(&sti_suspend.hw_states[index]);
		if (ret)
			return ret;
	}

	__pa_eram = (void *)sti_suspend.eram_base;
	__va_eram = sti_suspend.hw_states[index].eram_vbase;

	/* copy the __pokeloop code in eram */
	sti_suspend.hw_states[index].eram_data.pa_pokeloop = __pa_eram;
	memcpy(__va_eram, sti_pokeloop, sti_pokeloop_sz);
	__va_eram += sti_pokeloop_sz;
	__pa_eram += sti_pokeloop_sz;

	/* copy the entry_tables in eram */
	sti_suspend.hw_states[index].eram_data.pa_table_enter = __pa_eram;

	list_for_each_entry(table,
			    &sti_suspend.hw_states[index].state_tables,
			    node) {
		if (!table->enter)
			continue;

		sti_copy_suspend_table(table, &__va_eram, &__pa_eram,
				       TBL_ENTER);
	}

	sti_insert_poke_table_end(&__va_eram, &__pa_eram);

	/* copy the exit_data_table in eram */
	sti_suspend.hw_states[index].eram_data.pa_table_exit = __pa_eram;

	list_for_each_entry_reverse(table,
				    &sti_suspend.hw_states[index].
				    state_tables,
				    node) {
		if (!table->exit)
			continue;

		sti_copy_suspend_table(table, &__va_eram, &__pa_eram,
				       TBL_EXIT);
	}

	sti_insert_poke_table_end(&__va_eram, &__pa_eram);

	sti_suspend.hw_states[index].eram_data.pa_sti_eram_code = __pa_eram;

	/*
	 * call prepare() for target state specific code to be
	 * copied to eram
	 */
	sti_suspend.hw_states[index].eram_state_code_base = __va_eram;

	pr_info("sti pm: entering prepare:%s\n",
		sti_suspend.hw_states[index].desc);

	if (sti_suspend.hw_states[index].prepare)
		return sti_suspend.hw_states[index].
			prepare(&sti_suspend.hw_states[index]);

	return 0;
}

static int sti_suspend_enter(suspend_state_t state)
{
	if (sti_suspend.index == -1 ||
	    state != sti_suspend.hw_states[sti_suspend.index].target_state)
			return -EINVAL;

	pr_info("sti pm: entering suspend:%s\n",
		sti_suspend.hw_states[sti_suspend.index].desc);

	if (!sti_suspend.hw_states[sti_suspend.index].enter) {
		pr_err("sti pm: state enter() is NULL\n");
		return -EFAULT;
	}

	return sti_suspend.hw_states[sti_suspend.index].
			enter(&sti_suspend.hw_states[sti_suspend.index]);
}

/*
 * sti_gic_set_wake is called whenever any driver
 * that is capable of wakeup calls enable_irq_wakeup().
 * This simply checks if valid IRQ number is given and
 * does not require to do anything else. If irq number
 * is invalid then error is returned.
 */
static int sti_gic_set_wake(struct irq_data *d, unsigned int on)
{
	if (d->irq <= MAX_GIC_SPI_INT)
		return 0;

	return -ENXIO;
}

static int __init sti_suspend_setup(void)
{
	int i;
	int ret = 0;
	struct device_node *np, *child, *np_ref;
	int reg, index;
	unsigned long ddr_pctl_addresses[MAX_DDR_PCTL_NUMBER];
	unsigned int nr_ddr_pctl = 0;
	struct resource res;
	const struct of_device_id *match = NULL;
	struct sti_ddr3_low_power_info lp_info[MAX_DDR_PCTL_NUMBER];
	__be32 addr;

	np = of_find_node_by_name(NULL, "ddr-pctl-controller");
	if (IS_ERR_OR_NULL(np))
		return -ENODEV;

	for_each_child_of_node(np, child) {
		if (!of_address_to_resource(child, 0, &res)) {
			ddr_pctl_addresses[nr_ddr_pctl] =
				(unsigned long)res.start;
		}

		np_ref = of_parse_phandle(child, "st,syscfg", 0);
		if (IS_ERR_OR_NULL(np_ref)) {
			of_node_put(np);
			return -ENODEV;
		}

		addr = *of_get_address(np_ref, 0, NULL, NULL);
		if (!addr) {
			of_node_put(np);
			return -ENODEV;
		}

		lp_info[nr_ddr_pctl].sysconf_base = be32_to_cpu(addr);

		of_node_put(np_ref);

		match = of_match_node(suspend_of_match, child);
		if (!match) {
			of_node_put(np);
			return -ENODEV;
		}

		lp_info[nr_ddr_pctl].sysconf_info =
				*((struct sti_low_power_syscfg_info *)
				match->data);

		nr_ddr_pctl++;
	}

	of_node_put(np);

	/*
	 * Read the eram 1 base address where the low power entry/exit
	 * code will be loaded
	 */
	np = of_find_node_by_type(NULL, "eram");
	if (IS_ERR_OR_NULL(np))
		return -ENODEV;

	index = of_property_match_string(np, "reg-names", "eram_1");
	if (index < 0) {
		of_node_put(np);
		return -ENODEV;
	}

	index = index * (of_n_addr_cells(np) + of_n_size_cells(np));
	if (of_property_read_u32_index(np, "reg", index, &reg)) {
		pr_err("sti pm: eram_1 base address not found\n");
		of_node_put(np);
		return -ENODEV;
	}

	sti_suspend.eram_base = reg;
	of_node_put(np);

	for (i = 0; i < MAX_SUSPEND_STATE; i++) {
		sti_suspend.hw_states[sti_suspend.hw_state_nr] =
			sti_hw_states[i];
		ret = sti_suspend.hw_states[sti_suspend.hw_state_nr]
			.setup(&sti_suspend.hw_states[sti_suspend.hw_state_nr],
			       ddr_pctl_addresses,
			       nr_ddr_pctl,
			       sti_suspend.eram_base,
			       lp_info);
		if (!ret)
			sti_suspend.hw_state_nr++;
	}

	if (!sti_suspend.hw_state_nr)
		return -ENODEV;

	suspend_set_ops(&sti_suspend.ops);

	 /*
	  * irqchip's irq_set_wake() is called whenever any driver
	  * that is capable of wakeup calls enable_irq_wakeup()
	  * If bsp code set this, its fine, otherwise it wont
	  * work. So for safety we set this from here too. It does
	  * nothing apart from checking the validity of the irq
	  * number that can possibly give a wakeup. This
	  * assignment and the corresponding function can be
	  * removed if you are sure that somebody set this before
	  * with a properly written function. Currently in 3.10 kernel
	  * nobody is setting this. Hence this is done by the power
	  * code.
	  */
	if (!gic_arch_extn.irq_set_wake)
		gic_arch_extn.irq_set_wake = sti_gic_set_wake;

	pr_info("sti pm: Suspend support registered\n");

	return 0;
}

module_init(sti_suspend_setup);

