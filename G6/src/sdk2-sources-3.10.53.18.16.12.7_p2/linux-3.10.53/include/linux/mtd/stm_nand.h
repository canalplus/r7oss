/*
 *  Copyright (C) 2013 STMicroelectronics Limited
 *
 *  May be copied or modified under the terms of the GNU General Public
 *  License.  See linux/COPYING for more information.
 */

#ifndef __LINUX_NAND_H
#define __LINUX_NAND_H

#include <linux/io.h>

/*** NAND flash timing and bank data ***/

/*
 * Legacy specification for NAND timing parameters.  Deprecated in favour of
 * include/mtd/nand.h:nand_timing_spec.
 */
struct stm_nand_timing_data {
	/* Times specified in ns.  (Will be rounded up to nearest multiple of
	   EMI clock period.) */
	int sig_setup;
	int sig_hold;
	int CE_deassert;
	int WE_to_RBn;

	int wr_on;
	int wr_off;

	int rd_on;
	int rd_off;

	int chip_delay;		/* delay in us */
};

/*
 * Board-level specification relating to a 'bank' of NAND Flash
 */
struct stm_nand_bank_data {
	int			csn;
	int			nr_partitions;
	struct mtd_partition	*partitions;
	unsigned int		options;
	unsigned int		bbt_options;
	unsigned int emi_withinbankoffset;

	/*
	 * The AC specification of the NAND device can be used to optimise how
	 * the STM NAND drivers interact with the NAND device.  During
	 * initialisation, NAND accesses are configured according to one of the
	 * following methods, in order of precedence:
	 *
	 *   1. Using the data in 'struct nand_timing_spec', if supplied.
	 *
	 *   2. Using the data in 'struct stm_nand_timing_data', if supplied.
	 *      Not supported by the stm-nand-bch driver, and deprecated in
	 *      favour of method 1.
	 *
	 *   3. Using the ONFI timing mode, as advertised by the device during
	 *      ONFI-probing (ONFI-compliant NAND only).
	 *
	 */
	struct stm_nand_timing_data *timing_data; /* [DEPRECATED] */

	struct nand_timing_spec *timing_spec;

	/*
	 * No. of IP clk cycles by which to 'relax' the timing configuration.
	 * Required on some boards to to accommodate board-level limitations.
	 * Used in conjunction with 'nand_timing_spec' and ONFI configuration.
	 */
	int			timing_relax;
};

/*** NAND flash platform data ***/

struct stm_plat_nand_flex_data {
	int nr_banks;
	struct stm_nand_bank_data *banks;
	unsigned int flex_rbn_connected:1;
	uint32_t flex_select_reg;
	uint32_t flex_select_msk;
};

enum stm_nand_bch_ecc_config {
	BCH_ECC_CFG_AUTO = 0,
	BCH_ECC_CFG_NOECC,
	BCH_ECC_CFG_18BIT,
	BCH_ECC_CFG_30BIT
};

struct stm_plat_nand_bch_data {
	struct stm_nand_bank_data *bank;
	enum stm_nand_bch_ecc_config bch_ecc_cfg;

	/* The threshold at which the number of corrected bit-flips per sector
	 * is deemed to have reached an excessive level (triggers '-EUCLEAN' to
	 * be returned to the caller).  The value should be in the range 1 to
	 * <ecc-strength> where <ecc-strength> is 18 or 30, depending on the BCH
	 * ECC mode in operation.  A value of 0 is interpreted by the driver as
	 * <ecc-strength>.
	 */
	unsigned int bch_bitflip_threshold;
	uint32_t bch_select_reg;
	uint32_t bch_select_msk;
};

struct stm_plat_nand_emi_data {
	unsigned int nr_banks;
	struct stm_nand_bank_data *banks;
	int emi_rbn_gpio;
};

struct stm_nand_config {
	enum {
		stm_nand_emi,
		stm_nand_flex,
		stm_nand_afm,
		stm_nand_bch,
	} driver;
	int nr_banks;
	struct stm_nand_bank_data *banks;
	union {
		int emi_gpio;
		int flex_connected;
	} rbn;
	enum stm_nand_bch_ecc_config bch_ecc_cfg;
	unsigned int bch_bitflip_threshold; /* See description in
					     * 'stm_plat_nand_bch_data'.
					     */
};

/* ************************************************************************** */

enum nandi_controllers {
	STM_NANDI_UNCONFIGURED,
	STM_NANDI_HAMMING,
	STM_NANDI_BCH
};

static inline void stm_nandi_select(enum nandi_controllers controller,
			uint32_t nandi_select_reg,
			uint32_t nandi_select_msk)
{
	unsigned v;
	static void __iomem *nandi_config_reg;

	if (!nandi_config_reg) {
		nandi_config_reg = ioremap(nandi_select_reg, 4);
		if (!nandi_config_reg) {
			pr_err("%s: failed to ioremap nandi select register\n",
				__func__);
			return;
		}
	}

	v = readl(nandi_config_reg);

	if (controller == STM_NANDI_HAMMING) {
		if (v & nandi_select_msk)
			return;
		v |= nandi_select_msk;
	} else {
		if (!(v & nandi_select_msk))
			return;
		v &= ~nandi_select_msk;
	}

	writel(v, nandi_config_reg);
	readl(nandi_config_reg);
}

#endif /* __LINUX_NAND_H */
