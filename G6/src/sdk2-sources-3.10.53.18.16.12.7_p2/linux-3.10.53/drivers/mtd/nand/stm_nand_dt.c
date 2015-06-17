#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/byteorder/generic.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/stm_nand.h>

/**
*	stm_of_get_partitions_node - get partitions node from stm-nand
*				type devices.
*	@dev		device pointer to use for devm allocations.
*	@np		device node of the driver.
*	@bank_nr	which bank number to use to get partitions.
*
*	Returns a node pointer if found, with refcount incremented, use
*	of_node_put() on it when done.
*
*/
struct device_node *stm_of_get_partitions_node(struct device_node *np,
		int bank_nr)
{
	char name[10];
	struct device_node *banks, *bank, *parts = NULL;

	banks = of_parse_phandle(np, "st,nand-banks", 0);
	if (banks) {
		sprintf(name, "bank%d", bank_nr);
		bank = of_get_child_by_name(banks, name);
		if (bank) {
			parts = of_get_child_by_name(bank, "partitions");
			of_node_put(bank);
		}
	}

	return parts;
}
EXPORT_SYMBOL(stm_of_get_partitions_node);

/**
 *	stm_of_get_nand_banks - Get nand banks info from a given device node.
 *
 *	@dev			device pointer to use for devm allocations.
 *	@np			device node of the driver.
 *	@banksp			double pointer to banks which is allocated
 *				and filled with bank data.
 *
 *	Returns a count of banks found in the given device node.
 *
 */
int stm_of_get_nand_banks(struct device *dev, struct device_node *np,
		struct stm_nand_bank_data **banksp)
{
	struct device_node *bank_np = NULL;
	struct stm_nand_bank_data *banks;
	struct device_node *banks_np;
	int nr_banks = 0;

	if (!np)
		return -ENODEV;

	banks_np = of_parse_phandle(np, "st,nand-banks", 0);
	if (!banks_np) {
		dev_warn(dev, "No NAND banks specified in DT: %s\n",
			 np->full_name);
		return -ENODEV;
	}

	for_each_child_of_node(banks_np, bank_np)
		nr_banks++;

	*banksp = devm_kzalloc(dev, sizeof(*banks) * nr_banks, GFP_KERNEL);
	banks = *banksp;
	bank_np = NULL;

	for_each_child_of_node(banks_np, bank_np) {
		struct device_node *timing;
		int bank = 0;

		of_property_read_u32(bank_np, "st,nand-csn", &banks[bank].csn);

		if (of_get_nand_bus_width(bank_np) == 16)
			banks[bank].options |= NAND_BUSWIDTH_16;
		if (of_get_nand_on_flash_bbt(bank_np))
			banks[bank].bbt_options |= NAND_BBT_USE_FLASH;
		if (of_property_read_bool(bank_np, "nand-no-autoincr"))
			banks[bank].options |= NAND_NO_AUTOINCR;

		banks[bank].nr_partitions = 0;
		banks[bank].partitions = NULL;

		timing = of_parse_phandle(bank_np, "st,nand-timing-data", 0);
		if (timing) {
			struct stm_nand_timing_data *td;

			td = devm_kzalloc(dev,
					  sizeof(struct stm_nand_timing_data),
					  GFP_KERNEL);

			of_property_read_u32(timing, "sig-setup",
					     &td->sig_setup);
			of_property_read_u32(timing, "sig-hold",
					     &td->sig_hold);
			of_property_read_u32(timing, "CE-deassert",
					     &td->CE_deassert);
			of_property_read_u32(timing, "WE-to-RBn",
					     &td->WE_to_RBn);
			of_property_read_u32(timing, "wr-on",
					     &td->wr_on);
			of_property_read_u32(timing, "wr-off",
					     &td->wr_off);
			of_property_read_u32(timing, "rd-on",
					     &td->rd_on);
			of_property_read_u32(timing, "rd-off",
					     &td->rd_off);
			of_property_read_u32(timing, "chip-delay",
					     &td->chip_delay);

			banks[bank].timing_data = td;
		}

		timing = of_parse_phandle(bank_np, "st,nand-timing-spec", 0);
		if (timing) {
			struct nand_timing_spec *ts;

			ts = devm_kzalloc(dev, sizeof(struct nand_timing_spec),
					  GFP_KERNEL);

			of_property_read_u32(timing, "tR", &ts->tR);
			of_property_read_u32(timing, "tCLS", &ts->tCLS);
			of_property_read_u32(timing, "tCS", &ts->tCS);
			of_property_read_u32(timing, "tALS", &ts->tALS);
			of_property_read_u32(timing, "tDS", &ts->tDS);
			of_property_read_u32(timing, "tWP", &ts->tWP);
			of_property_read_u32(timing, "tCLH", &ts->tCLH);
			of_property_read_u32(timing, "tCH", &ts->tCH);
			of_property_read_u32(timing, "tALH", &ts->tALH);
			of_property_read_u32(timing, "tDH", &ts->tDH);
			of_property_read_u32(timing, "tWB", &ts->tWB);
			of_property_read_u32(timing, "tWH", &ts->tWH);
			of_property_read_u32(timing, "tWC", &ts->tWC);
			of_property_read_u32(timing, "tRP", &ts->tRP);
			of_property_read_u32(timing, "tREH", &ts->tREH);
			of_property_read_u32(timing, "tRC", &ts->tRC);
			of_property_read_u32(timing, "tREA", &ts->tREA);
			of_property_read_u32(timing, "tRHOH", &ts->tRHOH);
			of_property_read_u32(timing, "tCEA", &ts->tCEA);
			of_property_read_u32(timing, "tCOH", &ts->tCOH);
			of_property_read_u32(timing, "tCHZ", &ts->tCHZ);
			of_property_read_u32(timing, "tCSD", &ts->tCSD);
			banks[bank].timing_spec = ts;
		}
		of_property_read_u32(bank_np, "st,nand-timing-relax",
				     &banks[bank].timing_relax);
		bank++;
	}

	return nr_banks;
}
EXPORT_SYMBOL(stm_of_get_nand_banks);
