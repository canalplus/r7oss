/*
 *  sata_brcmstb_phy.c - Broadcom SATA3 AHCI Controller PHY Driver
 *
 *  Copyright (C) 2009 - 2013 Broadcom Corporation
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#define pr_fmt(fmt) "brcm-sata3-phy: " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/blkdev.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/libata.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/ahci_platform.h>
#include <linux/compiler.h>
#include <linux/brcmstb/brcmstb.h>
#include <scsi/scsi_host.h>

#include "sata_brcmstb.h"
#include "ahci.h"

static void sata_mdio_wr_28nm(void __iomem *addr, u32 port, u32 bank, u32 ofs,
			      u32 msk, u32 value)
{
	u32 tmp;
	void __iomem *base = addr + (port * SATA_MDIO_REG_SPACE_SIZE);

	writel(bank, base + SATA_MDIO_BANK_OFFSET);
	tmp = readl(base + SATA_MDIO_REG_OFFSET(ofs));
	tmp = (tmp & msk) | value;
	writel(tmp, base + SATA_MDIO_REG_OFFSET(ofs));
}

static void sata_mdio_wr_legacy(void __iomem *addr, u32 port, u32 bank, u32 ofs,
				u32 msk, u32 value)
{
	u32 tmp;
	u32 bank_port = bank + (port * SATA_MDIO_REG_LEGACY_BANK_OFS);

	writel(bank_port, addr + SATA_MDIO_BANK_OFFSET);
	tmp = readl(addr + SATA_MDIO_REG_OFFSET(ofs));
	tmp = (tmp & msk) | value;
	writel(tmp, addr + SATA_MDIO_REG_OFFSET(ofs));
}

/* These defaults were characterized by H/W group */
#define FMIN_VAL_DEFAULT 0x3df
#define FMAX_VAL_DEFAULT 0x3df
#define FMAX_VAL_SSC 0x83

static void cfg_ssc_28nm(void __iomem *base, int port, int ssc_en)
{
	u32 tmp;

	/* override the TX spread spectrum setting */
	tmp = TXPMD_CONTROL1_TX_SSC_EN_FRC_VAL | TXPMD_CONTROL1_TX_SSC_EN_FRC;
	sata_mdio_wr_28nm(base, port, TXPMD_REG_BANK, TXPMD_CONTROL1, ~tmp,
		tmp);

	/* set fixed min freq */
	sata_mdio_wr_28nm(base, port, TXPMD_REG_BANK,
		TXPMD_TX_FREQ_CTRL_CONTROL2,
		~TXPMD_TX_FREQ_CTRL_CONTROL2_FMIN_MASK,
		FMIN_VAL_DEFAULT);

	/* set fixed max freq depending on SSC config */
	if (ssc_en) {
		pr_info("Enabling SSC on port %d\n", port);
		tmp = FMAX_VAL_SSC;
	} else
		tmp = FMAX_VAL_DEFAULT;

	sata_mdio_wr_28nm(base, port, TXPMD_REG_BANK,
		TXPMD_TX_FREQ_CTRL_CONTROL3,
		~TXPMD_TX_FREQ_CTRL_CONTROL3_FMAX_MASK, tmp);
	
	/*TCH G9*/
	sata_mdio_wr_28nm(base, port, 0x70, 0x81, 0xFFFF0020, 0xC20 );
	sata_mdio_wr_28nm(base, port, 0x70, 0x82, 0xFFFF0008, 0x21c8);
}

static void cfg_ssc_legacy(void __iomem *base, int port, int ssc_en)
{
	u32 tmp;

	/* override the TX spread spectrum setting */
	tmp = TXPMD_CONTROL1_TX_SSC_EN_FRC_VAL | TXPMD_CONTROL1_TX_SSC_EN_FRC;
	sata_mdio_wr_legacy(base, port, TXPMD_REG_BANK_LEGACY, TXPMD_CONTROL1,
		~tmp, tmp);

	/* set fixed min freq */
	sata_mdio_wr_legacy(base, port, TXPMD_REG_BANK_LEGACY,
		TXPMD_TX_FREQ_CTRL_CONTROL2,
		~TXPMD_TX_FREQ_CTRL_CONTROL2_FMIN_MASK,
		FMIN_VAL_DEFAULT);

	/* set fixed max freq depending on SSC config */
	if (ssc_en) {
		pr_info("Enabling SSC on port %d\n", port);
		tmp = FMAX_VAL_SSC;
	} else
		tmp = FMAX_VAL_DEFAULT;

	sata_mdio_wr_legacy(base, port, TXPMD_REG_BANK_LEGACY,
		TXPMD_TX_FREQ_CTRL_CONTROL3,
		~TXPMD_TX_FREQ_CTRL_CONTROL3_FMAX_MASK, tmp);
}

static struct sata_phy_cfg_ops cfg_op_tbl[SATA_PHY_MDIO_END] = {
	[SATA_PHY_MDIO_LEGACY] = {
		.cfg_ssc = cfg_ssc_legacy,
	},
	[SATA_PHY_MDIO_28NM] = {
		.cfg_ssc = cfg_ssc_28nm,
	},
};

static struct sata_phy_cfg_ops *cfg_op;

static void _brcm_sata3_phy_cfg(const struct sata_brcm_pdata *pdata, int port,
			       int enable)
{
	/* yfzhang@broadcom.com has stated that the core will only have (2)
	 * ports. Further, the RDB currently lacks documentation for these
	 * registers. So just keep a map of which port corresponds to these
	 * magic registers.
	 */
	const u32 port_to_phy_ctrl_ofs[MAX_PHY_CTRL_PORTS] = {
		SATA_TOP_CTRL_PHY_CTRL_OFS + (0 * SATA_TOP_CTRL_PHY_CTRL_LEN),
		SATA_TOP_CTRL_PHY_CTRL_OFS + (1 * SATA_TOP_CTRL_PHY_CTRL_LEN),
	};
	void __iomem *top_ctrl;

	top_ctrl = ioremap(pdata->top_ctrl_base_addr, SATA_TOP_CTRL_REG_LENGTH);
	if (!top_ctrl) {
		pr_err("failed to ioremap SATA top ctrl regs\n");
		return;
	}

	if (port < MAX_PHY_CTRL_PORTS) {
		void __iomem *p;
		u32 reg;

		if (enable) {
			/* clear PHY_DEFAULT_POWER_STATE */
			p = top_ctrl + port_to_phy_ctrl_ofs[port] +
				SATA_TOP_CTRL_PHY_CTRL_1;
			reg = readl(p);
			reg &= ~SATA_TOP_CTRL_1_PHY_DEFAULT_POWER_STATE;
			writel(reg, p);

			/* reset the PHY digital logic */
			p = top_ctrl + port_to_phy_ctrl_ofs[port] +
				SATA_TOP_CTRL_PHY_CTRL_2;
			reg = readl(p);
			reg &= ~(SATA_TOP_CTRL_2_SW_RST_MDIOREG |
				 SATA_TOP_CTRL_2_SW_RST_OOB |
				 SATA_TOP_CTRL_2_SW_RST_RX);
			reg |= SATA_TOP_CTRL_2_SW_RST_TX;
			writel(reg, p);
			reg = readl(p);
			reg |= SATA_TOP_CTRL_2_PHY_GLOBAL_RESET;
			writel(reg, p);
			reg = readl(p);
			reg &= ~SATA_TOP_CTRL_2_PHY_GLOBAL_RESET;
			writel(reg, p);
			reg = readl(p);
		} else {
			/* power-off the PHY digital logic */
			p = top_ctrl + port_to_phy_ctrl_ofs[port] +
				SATA_TOP_CTRL_PHY_CTRL_2;
			reg = readl(p);
			reg |= (SATA_TOP_CTRL_2_SW_RST_MDIOREG |
				SATA_TOP_CTRL_2_SW_RST_OOB |
				SATA_TOP_CTRL_2_SW_RST_RX |
				SATA_TOP_CTRL_2_SW_RST_TX |
				SATA_TOP_CTRL_2_PHY_GLOBAL_RESET);
			writel(reg, p);

			/* set PHY_DEFAULT_POWER_STATE */
			p = top_ctrl + port_to_phy_ctrl_ofs[port] +
				SATA_TOP_CTRL_PHY_CTRL_1;
			reg = readl(p);
			reg |= SATA_TOP_CTRL_1_PHY_DEFAULT_POWER_STATE;
			writel(reg, p);
		}
	}

	iounmap(top_ctrl);
}

void brcm_sata3_phy_cfg(const struct sata_brcm_pdata *pdata, int port,
			int enable)
{
	const u32 phy_base = pdata->phy_base_addr;
	const int ssc_enable = pdata->phy_enable_ssc_mask & (1 << port);
	void __iomem *base;

	base = ioremap(phy_base, SATA_MDIO_REG_LENGTH);
	if (!base) {
		pr_err("%s: Failed to ioremap PHY registers!\n", __func__);
		goto err;
	}

	if (pdata->phy_generation == 0x2800)
		cfg_op = &cfg_op_tbl[SATA_PHY_MDIO_28NM];
	else
		cfg_op = &cfg_op_tbl[SATA_PHY_MDIO_LEGACY];

	if (enable) {
		_brcm_sata3_phy_cfg(pdata, port, 1);
		if (cfg_op->cfg_ssc)
			cfg_op->cfg_ssc(base, port, ssc_enable);
	} else
		_brcm_sata3_phy_cfg(pdata, port, 0);

	iounmap(base);

err:
	return;
}
EXPORT_SYMBOL(brcm_sata3_phy_cfg);

MODULE_LICENSE("GPL");
