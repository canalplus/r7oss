/*
 * -------------------------------------------------------------------------
 * Copyright (C) 2014  STMicroelectronics
 * Author: Francesco M. Virlinzi  <francesco.virlinzi@st.com>
 *	   Sudeep Biswas	  <sudeep.biswas@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License V.2 ONLY.  See linux/COPYING for more information.
 *
 * ------------------------------------------------------------------------- */

#include <asm/cacheflush.h>
#include <asm-generic/sections.h>
#include <asm/smp_scu.h>
#include <asm/suspend.h>
#include <linux/io.h>
#include <linux/of_gpio.h>
#include <linux/clk.h>
#include <linux/power/st_lpm.h>
#include <linux/power/st_lpm_def.h>

#include "suspend.h"
#include "suspend_internal.h"
#include "poke_table.h"
#include "synopsys_dwc_ddr32.h"

#define STI_GPIO_REG_CLR_POUT	0x08

#define BAUDRATE_VAL_M1(bps, clk)  \
((((bps * (1 << 14)) + (1 << 13)) / (clk / (1 << 6))))
#define UART_BAUDRATE		115200
#define UART_CONFIG_REG_OFFSET	0x0c
#define UART_TIMEOUT_REG_OFFSET	0x1c
#define UART_INT_REG_OFFSET	0x10
#define SCU_CONFIG		0x04

#define DTU_ADDRESS_WIDTH_VAL  ((0<<9) | (3<<6) | (1<<3) | (3<<0))
#define DTU_CFG_VAL ((127<<16) | (1<<8) | (1<<7) | (63<<1) | (1<<0))

enum cps_tag_mark {UNMARK, MARK};

struct cps_private_data {
	void __iomem *scu_virtualbase;
	void __iomem *early_console_base;
	unsigned long early_console_rate;
};

struct cps_private_data cps_data;

/* Define CPS specific poke tables */
static struct poke_operation sti_cps_dtuc_1[] = {
	/* set the pctl dtu working address (for both write and read) */
	POKE32(DTUWACTL, 0),
	POKE32(DTURACTL, 0),

	/* set the data written by the dtu to 0 */
	POKE32(DTUWD0, 0),
	POKE32(DTUWD1, 0),
	POKE32(DTUWD2, 0),
	POKE32(DTUWD3, 0),

	/* configure the pctl dtu to access memory devices correctly */
	POKE32(DTUAWDT, DTU_ADDRESS_WIDTH_VAL),
	POKE32(DTUCFG, DTU_CFG_VAL),

	/* run a first time the pctl dtu , 64 bytes erased */
	POKE32(DTUECTL, 0x1),

	/* wait while transfers are complete */
	WHILE_NE32(DTUECTL, 0x1, 0x0),
	POKE32(DTUCFG, 0),
};

static struct poke_operation sti_cps_dtuc_2[] = {
	/*
	 * add 0x10 to the dtu address and configure the dtu
	 * again for the next 64 bytes to be erased
	 */
	POKE32(DTUWACTL, 0x10),
	POKE32(DTURACTL, 0x10),

	POKE32(DTUCFG, DTU_CFG_VAL),

	/* run again the dtu */
	POKE32(DTUECTL, 0x1),

	/* wait while transfers are complete */
	WHILE_NE32(DTUECTL, 0x1, 0x0),
	POKE32(DTUCFG, 0),
};

/* Below poke table is ddr low power operations before system enters CPS */
static struct poke_operation sti_cps_ddr_lp_enter[] = {
	UPDATE32(0, 0, 0),
	WHILE_NE32(0, 0, 0),
};

/* Below poke table is ddr controller operations before system enters CPS */
static struct poke_operation sti_cps_ddr_enter[] = {
	OR32(DDR_DTU_CFG, DDR_DTU_CFG_ENABLE),

	/* Synopsys DDR32: in Self-Refresh=> */
	POKE32(DDR_SCTL, DDR_SCTL_SLEEP),
	WHILE_NE32(DDR_STAT, DDR_STAT_MASK, DDR_STAT_LOW_POWER),

	/* PHY Low power state */
	/* CK output disable */
	UPDATE32(DDR_PHY_DSGCR, ~DDR_PHY_CKOE, 0),

	/* ODT output disable */
	UPDATE32(DDR_PHY_DSGCR, ~DDR_PHY_ODTOE, 0),

	/* Disable AC ODTs, and drivers */
	UPDATE32(DDR_PHY_ACIOCR, ~DDR_PHY_ACIOCR_ACOE, 0),
	UPDATE32(DDR_PHY_ACIOCR, ~DDR_PHY_ACIOCR_ACODT, 0),
	OR32(DDR_PHY_ACIOCR, DDR_PHY_ACIOCR_ACPDD),
	OR32(DDR_PHY_ACIOCR, DDR_PHY_ACIOCR_CSPDD),

	/* Disable clock ODT, and driver */
	UPDATE32(DDR_PHY_ACIOCR, ~DDR_PHY_ACIOCR_CKODT, 0),
	OR32(DDR_PHY_ACIOCR, DDR_PHY_ACIOCR_CKPDD),

	/* Disable Data ODT, driver and receiver */
	OR32(DDR_PHY_DXCCR, DDR_PHY_DXCCR_DXPDD),
	OR32(DDR_PHY_DXCCR, DDR_PHY_DXCCR_DXPDR),
	UPDATE32(DDR_PHY_DXCCR, ~DDR_PHY_DXCCR_DXODT, 0),
};

static struct poke_operation __cps_lmi_retention[] = {
	/*
	 * Enable retention mode gpio
	 * Address and value set in sti_setup_lmi_retention_gpio.
	 */
	POKE32(0x0, 0x0),
};

static struct poke_operation __cps_enter_passive[] = {
	/*
	 * Send message 'ENTER_PASSIVE' (0x5)
	 */
	POKE32(0x0, LPM_MSG_ENTER_PASSIVE),
};
/* End defining poke tables */

static struct sti_suspend_table sti_cps_tables[MAX_SUSPEND_TABLE_SIZE];

/*
 * sti_cps_marker writes/unwrites a signature and then a CPS wakeup jump
 * address. This is written starting at CONFIG_CPS_TAG_PHYSICAL_ADDRESS.
 * After CPS wakeup PBL checks this signature and then jumps
 * both cpu0 and cpu1 to the CPS wakeup jump address.
 * In this case CPS wakeup jump address is the physical address of the function
 * sti_defrost_kernel. This can be in ddr as pbl already took
 * ddrs out of self refresh and lmi retention
 */
static void sti_cps_marker(enum cps_tag_mark enable)
{
	int cps_tag_offset;
	unsigned long jmp_addr = __pa(sti_defrost_kernel);

	const long linux_marker_on[] = {
			0x7a6f7266,	/* froz */
			0x6c5f6e65,	/* en_l */
			0x78756e69 };	/* inux */

	const long linux_marker_off[] = {-2, -2, -2};

	cps_tag_offset = st_lpm_get_dmem_offset(ST_SBC_DMEM_CPS_TAG);

	if (enable) {
		st_lpm_write_dmem((unsigned char *)linux_marker_on,
				ARRAY_SIZE(linux_marker_on) * sizeof(long),
				cps_tag_offset);
		/*
		 * After writing the CPS signature write the jump address
		 * in SBC DMEM.
		 */
		st_lpm_write_dmem((unsigned char *)&jmp_addr, sizeof(long),
				cps_tag_offset +
				ARRAY_SIZE(linux_marker_on) * sizeof(long));
	} else {
		st_lpm_write_dmem((unsigned char *)linux_marker_off,
				ARRAY_SIZE(linux_marker_off) * sizeof(long),
				cps_tag_offset);
	}
}

static void sti_cps_init_early_console(void __iomem *asc_base,
	unsigned long asc_clk)
{
	writel(0x1189 & ~0x80, asc_base + UART_CONFIG_REG_OFFSET);/* ctrl */
	writel(BAUDRATE_VAL_M1(UART_BAUDRATE, asc_clk), asc_base); /* baud */
	writel(20, asc_base + UART_TIMEOUT_REG_OFFSET);		/* timeout */
	writel(1, asc_base + UART_INT_REG_OFFSET);		/* int */
	writel(0x1189, asc_base + UART_CONFIG_REG_OFFSET);	/* ctrl */
	pr_info("\nsti pm cps: Early console ready\n");
}

#ifdef CONFIG_SMP
static void sti_cps_write_pen_release(int val)
{
	pen_release = val;
	smp_wmb(); /* pen_release should be written */
	__cpuc_flush_dcache_area((void *)&pen_release, sizeof(pen_release));
	outer_clean_range(__pa(&pen_release), __pa(&pen_release + 1));
}
#else
static inline void sti_cps_write_pen_release(int val) {};
#endif


int sti_cps_prepare(struct sti_hw_state_desc *state)
{
	memcpy(state->eram_state_code_base, sti_cps_on_eram,
	       sti_cps_on_eram_sz);

	return 0;
}

int sti_cps_begin(struct sti_hw_state_desc *state)
{
	struct sti_suspend_table *table;
	int ret = 0;
	int offset, j;
	unsigned long pctl_address[MAX_DDR_PCTL_NUMBER];
	int poke_entry_del;
	int count = 0;

	static int call_number;
	static struct list_head *deleted_array[] = {NULL, NULL};

	switch (call_number) {
	case 0:
		offset = st_lpm_get_dmem_offset(ST_SBC_DMEM_DTU);

		if (offset >= 0)
			/*
			 * Only one LMI is supported for the purpose
			 * of DTU buffers
			 */
			ret = st_lpm_read_dmem((unsigned char *)
					       &pctl_address[0],
					       sizeof(unsigned long),
					       offset);

		if (offset < 0 || ret || !pctl_address[0])
			poke_entry_del = 1;
		else
			poke_entry_del = 0;

		list_for_each_entry(table,
				    &state->state_tables,
				    node) {
			unsigned long  *tbl_ptr;

			if (count > 1)
				break;

			deleted_array[count] = &table->node;

			if (poke_entry_del) {
				count++;
				continue;
			}

			tbl_ptr = table->enter;

			tbl_ptr[2] += pctl_address[0];

			tbl_ptr += (sizeof(struct poke_operation))/
					sizeof(unsigned long);

			tbl_ptr[2] += pctl_address[0];

			count++;
		}

		if (poke_entry_del) {
			for (j = 0; j < 2; j++)
				if (deleted_array[j])
					list_del(deleted_array[j]);
			 state->begin = NULL;
		} else {
			call_number++;
		}
		break;
	case 1:
		for (j = 0; j < 2; j++)
			if (deleted_array[j])
				list_del(deleted_array[j]);
		state->begin = NULL;
		break;
	default:
		break;
	}

	return 0;
}

int sti_cps_enter(struct sti_hw_state_desc *state)
{
	unsigned long flag;

	struct cps_private_data *cpsdata = ((struct cps_private_data *)state->
						state_private_data);
	VA2PA = (unsigned long) _text -
				(unsigned long) __pa(_text);

	if (state == NULL)
		return -EINVAL;

	/*
	 * Initialize the pen to be checked on resume by core 1
	 */
	if (num_present_cpus() > 1)
		sti_cps_write_pen_release(-1);

	local_irq_save(flag);
	/*
	 * Write the Linux Frozen Marker in the Main Memory
	 */
	sti_cps_marker(MARK);

	/*
	 * Flush __all__ the caches to avoid some pending write operation
	 * on the memory is still in D-cache...
	 */
	flush_cache_all();

	/* Flush and disable the outer L2 cache */
	outer_disable();

	pr_info("sti pm cps: CPU Going to be Frozen\n");

	/*
	 * Call the kernel suspend framework API
	 * After the framework finishes, sti_cps_exec_on_eram
	 * is called which takes the system to CPS.
	 * Once a wakeup is received, system is restored and
	 * execution is started just after this call, as if
	 * nothing has happened
	 */
	cpu_suspend((long)__pa(&state->eram_data), sti_cps_exec);

	/* SYSTEM HAS WOKENUP FROM CPS */

	/* First check whether we are running on the correct cpu */
	if (smp_processor_id())
		pr_err("sti pm cps: Error: Running on the wrong CPU\n");

	/* Resume the outer cache */
	outer_resume();

	/*
	 * remove the marker in memory also if the bootloader already did that
	 */
	sti_cps_marker(UNMARK);

	/*
	 * Here an __early__ console initialization to avoid
	 * blocking prints.
	 * This is required if the kernel boots with 'no_console_suspend'
	 */
	if (cpsdata->early_console_rate &&
	    cpsdata->early_console_base)
		sti_cps_init_early_console(cpsdata->early_console_base,
					   cpsdata->early_console_rate);

	pr_info("sti pm cps: System woken up from CPS\n");

	/*
	 * the early_trap_init isn't required because the CPU is already
	 * running on the correct pgtable where the vectors_page is already
	 * mapped
	 */
	local_irq_restore(flag);

	/*
	 * Enable the SCU if required
	 */
	if (num_present_cpus() > 1)
		scu_enable(cpsdata->scu_virtualbase);

	return 0;
}

int sti_cps_get_eram_size(struct sti_hw_state_desc *state)
{
	struct sti_suspend_table *table;
	int eram_size = 0;

	eram_size = sti_cps_on_eram_sz + sti_pokeloop_sz;

	list_for_each_entry(table, &state->state_tables, node)
		eram_size += table->enter_size;

	eram_size += 2 * sizeof(struct poke_operation);

	return eram_size;
}

int sti_cps_setup(struct sti_hw_state_desc *state,
		  unsigned long *ddr_pctl_addr,
		  unsigned int no_pctl,
		  unsigned long eram_base,
		  struct sti_ddr3_low_power_info *ddr_sys_conf)

{
	struct device_node *np, *np_ref;
	const char *name;
	int gpio_pin, ret, reg, size_reg;
	unsigned int  gpioa, port;
	int index = 0, i;
	struct of_phandle_args args;
	struct clk *clk;

	/*
	 * Now patching the lmi retention data tbl and SBC mailbox cmd tbl.
	 * This cmd switches off the core chip, except the SBC itself
	 */
	np = of_find_compatible_node(NULL, NULL, "st,lpm");
	if (IS_ERR_OR_NULL(np))
		return -ENODEV;

	gpio_pin = of_get_named_gpio(np, "st,lmi-ret-gpio", 0);
	ret = gpio_request(gpio_pin, "lmi-retention");
	if (ret != 0) {
		of_node_put(np);
		return -ENODEV;
	}

	gpio_direction_output(gpio_pin, 1);
	ret = of_parse_phandle_with_args(np,
					 "st,lmi-ret-gpio",
					 "#gpio-cells",
					 0,
					 &args);
	if (ret != 0) {
		of_node_put(np);
		gpio_free(gpio_pin);
		return -ENODEV;
	}

	np_ref = args.np;
	name = of_get_property(np_ref,
			       "st,bank-name",
			       NULL);

	of_node_put(np_ref);

	if (!name) {
		of_node_put(np);
		gpio_free(gpio_pin);
		return -ENODEV;
	}

	ret = kstrtol(name + 3, 0, (long *)&port);

	if (ret != 0) {
		of_node_put(np);
		gpio_free(gpio_pin);
		return ret;
	}

	np_ref = of_find_node_by_name(NULL,
				      "pin-controller-sbc");

	if (IS_ERR_OR_NULL(np_ref) ||
	    of_property_read_u32_index(np_ref,
				       "ranges",
				       1,
				       &gpioa)) {
		of_node_put(np);
		gpio_free(gpio_pin);
		return -ENODEV;
	}

	of_node_put(np_ref);

	gpioa += port * 0x1000;

	*(((long *)__cps_lmi_retention) + 1) = STI_GPIO_REG_CLR_POUT + gpioa;

	*(((long *)__cps_lmi_retention) + 2) = 1 << args.args[0];

	/* Retrieve configuration register base address */
	if (of_property_read_u32_index(np, "reg", 2, &reg)) {
		of_node_put(np);
		gpio_free(gpio_pin);
		return -ENODEV;
	}

	*(((long *)__cps_enter_passive) + 1) = reg + 0x4;

	of_node_put(np);

	if (state->ddr_state == DDR_SR) {
		for (i = 0; i < no_pctl; i++) {
			if (i == 0) {
				populate_suspend_table_entry(&sti_cps_tables
							     [index++],
							     (long *)
							     sti_cps_dtuc_1,
							     NULL,
							     sizeof
							     (sti_cps_dtuc_1),
							     0,
							     ddr_pctl_addr[i]);

				populate_suspend_table_entry(&sti_cps_tables
							     [index++],
							     (long *)
							     sti_cps_dtuc_2,
							     NULL,
							     sizeof
							     (sti_cps_dtuc_2),
							     0,
							     ddr_pctl_addr[i]);
			}

			*(((long *)sti_cps_ddr_lp_enter) + 1) = ddr_sys_conf[i].
								sysconf_info.
								ddr3_cfg_offset;

			*(((long *)sti_cps_ddr_lp_enter) + 2) = ~(0x1 <<
								ddr_sys_conf[i].
								sysconf_info.
								cfg_shift);

			*(((long *)sti_cps_ddr_lp_enter) + 5) = ddr_sys_conf[i].
								sysconf_info.
								ddr3_stat_offset
								;

			*(((long *)sti_cps_ddr_lp_enter) + 6) = (0x1 <<
								ddr_sys_conf[i].
								sysconf_info.
								stat_shift);

			populate_suspend_table_entry(&sti_cps_tables[index++],
						     (long *)
						     sti_cps_ddr_lp_enter,
						     NULL,
						     sizeof(
						     sti_cps_ddr_lp_enter),
						     0,
						     ddr_sys_conf[i].
						     sysconf_base);

			populate_suspend_table_entry(&sti_cps_tables[index++],
						     (long *)sti_cps_ddr_enter,
						     NULL,
						     sizeof(sti_cps_ddr_enter),
						     0,
						     ddr_pctl_addr[i]);
		}

		populate_suspend_table_entry(&sti_cps_tables[index++],
					     (long *)__cps_lmi_retention,
					     NULL,
					     sizeof(__cps_lmi_retention),
					     0,
					     0);
	} else if (state->ddr_state == DDR_ON) {
		pr_info("sti pm cps: DDR_ON not supported in CPS\n");
		gpio_free(gpio_pin);
		return -EINVAL;
	}

	populate_suspend_table_entry(&sti_cps_tables[index++],
				     (long *)__cps_enter_passive,
				     NULL,
				     sizeof(__cps_enter_passive),
				     0,
				     0);

	INIT_LIST_HEAD(&state->state_tables);
	for (i = 0; i < index; ++i)
		list_add_tail(&sti_cps_tables[i].node,
			      &state->state_tables);

	state->eram_vbase = ioremap(eram_base, sti_cps_get_eram_size(state));
	if (!state->eram_vbase) {
		pr_err("sti pm cps: eram base remap failed in CPS setup\n");
		return -EINVAL;
	}

	/* Read the SCU base and map */
	np = of_find_compatible_node(NULL, NULL, "arm,cortex-a9-scu");

	if (IS_ERR_OR_NULL(np)) {
		pr_err("sti pm cps: SCU entry not found in DT\n");
		iounmap(state->eram_vbase);
		return -ENODEV;
	}

	if (of_property_read_u32_index(np, "reg", 0, &reg)) {
		pr_err("sti pm cps: SCU base address not found\n");
		iounmap(state->eram_vbase);
		of_node_put(np);
		return -ENODEV;
	}

	if (of_property_read_u32_index(np, "reg", 1, &size_reg)) {
		pr_err("sti pm cps: SCU base size not found\n");
		iounmap(state->eram_vbase);
		of_node_put(np);
		return -ENODEV;
	}

	cps_data.scu_virtualbase = ioremap_nocache(reg, size_reg);
	if (!cps_data.scu_virtualbase) {
		pr_err("sti pm cps: SCU base remap failed\n");
		iounmap(state->eram_vbase);
		of_node_put(np);
		return -ENODEV;
	}
	of_node_put(np);

	/*
	 * Read the console UART physical base address
	 * and the rate of the clock
	 */
	if (of_chosen) {
		if (of_get_property(of_chosen, "linux,stdout-path", NULL))
			np = of_find_node_by_path(of_get_property(of_chosen,
						  "linux,stdout-path", NULL));

		if  (!IS_ERR(np)) {
			if (!of_property_read_u32_index(np, "reg", 0, &reg))
				if (!of_property_read_u32_index(np, "reg",
								1,
								&size_reg))
					cps_data.early_console_base =
						ioremap_nocache(reg, size_reg);

			clk = of_clk_get(np, 0);
			if (clk)
				cps_data.early_console_rate =
					clk_get_rate(clk);

			of_node_put(np);
		}
	}

	state->state_private_data = &cps_data;

	return 0;
}

