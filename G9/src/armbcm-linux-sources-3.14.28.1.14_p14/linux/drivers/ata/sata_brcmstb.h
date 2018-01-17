/*
 *  sata_brcmstb.h - Broadcom SATA3 AHCI Controller Driver
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

#ifndef __SATA_BRCMSTB_H__
#define __SATA_BRCMSTB_H__

#ifdef __BIG_ENDIAN
#define DATA_ENDIAN			 2 /* AHCI->DDR inbound accesses */
#define MMIO_ENDIAN			 2 /* CPU->AHCI outbound accesses */
#else
#define DATA_ENDIAN			 0
#define MMIO_ENDIAN			 0
#endif

#define MAX_PORTS			32

#define SATA_MDIO_BANK_OFFSET		0x23c
#define SATA_MDIO_REG_OFFSET(ofs)	((ofs) * 4)
#define SATA_MDIO_REG_SPACE_SIZE	0x1000
#define SATA_MDIO_REG_LENGTH		0x1f00

/* The older SATA PHY registers duplicated per port registers within the map,
 * rather than having a separate map per port. */
#define SATA_MDIO_REG_LEGACY_BANK_OFS	0x10

#define MAX_PHY_CTRL_PORTS			2
#define SATA_TOP_CTRL_REG_LENGTH		0x24
#define SATA_TOP_CTRL_BUS_CTRL			0x4
#define SATA_TOP_CTRL_PHY_CTRL_OFS		0xc
#define SATA_TOP_CTRL_PHY_CTRL_LEN		0x8
#define SATA_TOP_CTRL_PHY_CTRL_1		0x0
#define SATA_TOP_CTRL_PHY_CTRL_2		0x4
#define SATA_TOP_CTRL_REGS_PER_PORT		2
#define SATA_TOP_CTRL_BUS_CTRL_OVERRIDE_HWINIT	BIT(16)
#define SATA_TOP_CTRL_2_SW_RST_MDIOREG		BIT(0)
#define SATA_TOP_CTRL_2_SW_RST_OOB		BIT(1)
#define SATA_TOP_CTRL_2_SW_RST_RX		BIT(2)
#define SATA_TOP_CTRL_2_SW_RST_TX		BIT(3)
#define SATA_TOP_CTRL_2_PHY_GLOBAL_RESET	BIT(14)
#define SATA_TOP_CTRL_1_PHY_DEFAULT_POWER_STATE	BIT(14)

#define SATA_FIRST_PORT_CTRL		0x700
#define SATA_NEXT_PORT_CTRL_OFFSET	0x80
#define SATA_PORT_PCTRL6(reg_base)	(reg_base + 0x18)

enum sata_mdio_phy_regs_28nm {
	PLL_REG_BANK_0 = 0x50,
	PLL_REG_BANK_0_PLLCONTROL_0 = 0x81,
	TXPMD_REG_BANK = 0x1a0,
	TXPMD_CONTROL1 = 0x81,
	TXPMD_CONTROL1_TX_SSC_EN_FRC = BIT(0),
	TXPMD_CONTROL1_TX_SSC_EN_FRC_VAL = BIT(1),
	TXPMD_TX_FREQ_CTRL_CONTROL1 = 0x82,
	TXPMD_TX_FREQ_CTRL_CONTROL2 = 0x83,
	TXPMD_TX_FREQ_CTRL_CONTROL2_FMIN_MASK = 0x3ff,
	TXPMD_TX_FREQ_CTRL_CONTROL3 = 0x84,
	TXPMD_TX_FREQ_CTRL_CONTROL3_FMAX_MASK = 0x3ff,
};

enum sata_mdio_phy_regs_legacy {
	TXPMD_REG_BANK_LEGACY = 0x1a0,
};

/**
 * struct pdev_map - Doubly-linked list used to associate a struct device
 *                     its associated platform devices.
 * @node: Forward/reverse links
 * @key: A pointer to a device struct
 * @brcm_pdev: The Broadcom SATA AHCI platform_device associated with the key
 * @ahci_pdev: The AHCI platform_device associated with the key
 */
struct pdev_map {
	struct list_head node;
	struct device *key;
	struct platform_device *brcm_pdev;
	struct platform_device *ahci_pdev;
};

/*
 * struct sata_brcm_pdata - Platform data for the Broadcom SATA AHCI driver
 *
 * These fields are defined in the driver's DT binding documentation.
 */
struct sata_brcm_pdata {
	struct platform_device *ahci_pdev;
	u32 phy_generation;
	u32 phy_base_addr;
	u32 phy_enable_ssc_mask;
	u32 top_ctrl_base_addr;
	u32 quirks;
	struct clk *sata_clk;
};

struct sata_phy_cfg_ops {
	void (*cfg_ssc)(void __iomem *base, int port, int ssc_en);
};

enum sata_phy_mdio_gen {
	SATA_PHY_MDIO_LEGACY = 0,
	SATA_PHY_MDIO_28NM,
	SATA_PHY_MDIO_END,
};

void brcm_sata3_phy_cfg(const struct sata_brcm_pdata *pdata, int port,
			int enable);

#endif /* __SATA_BRCMSTB_H__ */
