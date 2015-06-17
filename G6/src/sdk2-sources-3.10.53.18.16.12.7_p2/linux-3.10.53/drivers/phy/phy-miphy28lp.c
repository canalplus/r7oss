/*
 * Copyright (C) 2013 STMicroelectronics
 *
 * STMicroelectronics PHY driver miphy28lp.
 *
 * Author: Alexandre Torgue <alexandre.torgue@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2, as
 * published by the Free Software Foundation.
 *
 */

#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/clk.h>
#include <linux/phy/phy.h>
#include <linux/delay.h>
#include <linux/mfd/syscon.h>
#include <linux/regmap.h>
#include <linux/reset.h>
#include <dt-bindings/phy/phy-miphy28lp.h>

#define PHYS_NUM	3

/* MiPHY registers */
#define MIPHY_STATUS_1		0x02
#define MIPHY_PHY_RDY		0x01
#define MIPHY_PLL_HFC_RDY	0x06
#define MIPHY_COMP_FSM_6	0x3f
#define MIPHY_COMP_DONE		0x80

#define MIPHY_CTRL_REG		0x04
#define MIPHY_PX_RX_POL		BIT(5)

/*
 * On STiH407 the glue logic can be different among MiPHY devices; for example:
 * MiPHY0: OSC_FORCE_EXT means:
 *  0: 30MHz crystal clk - 1: 100MHz ext clk routed through MiPHY1
 * MiPHY1: OSC_FORCE_EXT means:
 *  1: 30MHz crystal clk - 0: 100MHz ext clk routed through MiPHY1
 * Some devices have not the possibility to check if the osc is ready.
 */
#define MIPHY_OSC_FORCE_EXT	BIT(3)
#define MIPHY_OSC_RDY		BIT(5)

#define MIPHY_CTRL_MASK		0xf
#define MIPHY_CTRL_DEFAULT	0
#define MIPHY_CTRL_SYNC_D_EN	BIT(2)

/* SATA / PCIe defines */
#define SATA_CTRL_MASK		0x7
#define PCIE_CTRL_MASK		0xff
#define SATA_CTRL_SELECT_SATA	1
#define SATA_CTRL_SELECT_PCIE	0
#define SYSCFG_PCIE_PCIE_VAL	0x80
#define SATA_SPDMODE		1

/* Flag for secure mode */
#define MIPHY28LP_SEC_REG	1

struct miphy28lp_phy {
	struct phy *phy;
	/* Helper pointer depending on bellow options */
	void __iomem *base;
	void __iomem *pipebase;
	/* Ressource options for pcie / sata / usb3 */
	void __iomem *sata_up_base;
	void __iomem *pcie_up_base;
	void __iomem *usb3_up_base;
};

struct miphy28lp_dev {
	struct device *dev;
	struct platform_device *pdev;
	struct mutex miphy_mutex;
	struct regmap *regmap;
	struct reset_control *miphy_rst;
	struct miphy28lp_phy miphy;
	u32 sata_gen;
	u32 data;
	int miphy_device_conf;	/* device actually configured */
	/* Specific MiPHY platform options */
	bool osc_force_ext;
	bool osc_rdy;
	bool px_rx_pol_inv;
	/* Sysconfig registers offsets needed to configure the device */
	u32 syscfg_miphy;
	u32 syscfg_miphy_status;
	u32 syscfg_pci;
	u32 syscfg_sata;
};

struct miphy_initval {
	u16 reg;
	u16 val;
};

enum miphy_sata_gen { SATA_GEN1, SATA_GEN2, SATA_GEN3 };

#define to_miphy28lp_dev(inst) \
	container_of((inst), struct miphy28lp_dev, miphy);

static const struct miphy_initval miphylp28_initvals_sata[] = {
	/* Putting Macro in reset */
	{0x00, 0x03}, {0x00, 0x01},
	/* Wait for a while */
	{0x04, 0x1c}, {0xeb, 0x1d}, {0x0d, 0x1e},
	/* PLL Ratio */
	{0xd4, 0xc8}, {0xd5, 0x00}, {0xd6, 0x00}, {0xd7, 0x00},
	/* Number of PLL Calibrations */
	{0xd3, 0x00},
	/* Unbanked Settings */
	{0x4e, 0xd1}, {0x99, 0x1f}, {0x0a, 0x40},
	/* Banked settings */
	/* Gen 1 */
	{0x0f, 0x00},
	{0x0e, 0x00}, {0x63, 0x00}, {0x64, 0xae},
	{0x4a, 0x53}, {0x4b, 0x00},
	{0x7a, 0x0d}, {0x7f, 0x7d}, {0x80, 0x56}, {0x81, 0x00}, {0x7b, 0x00},
	/* Gen 2 */
	{0x0f, 0x01},
	{0x0e, 0x05}, {0x63, 0x00}, {0x64, 0xae},
	{0x4a, 0x72}, {0x4b, 0x20},
	{0x7a, 0x0d}, {0x7f, 0x7d}, {0x80, 0x56}, {0x81, 0x00}, {0x7b, 0x00},
	/* Gen 3 */
	{0x0f, 0x02},
	{0x0e, 0x0a}, {0x63, 0x00}, {0x64, 0xae},
	{0x4a, 0xc0}, {0x4b, 0x20},
	{0x7a, 0x0d}, {0x7f, 0x7d}, {0x80, 0x56}, {0x81, 0x00}, {0x7b, 0x00},
	/* Power control */
	{0xcd, 0x21},
	/* Macro out of reset */
	{0x00, 0x00},
	/* Poll for HFC ready after reset release */
	/* Compensation measurement */
	{0x01, 0x05}, {0xe9, 0x00}, {0x3a, 0x40}, {0x01, 0x00}, {0xe9, 0x40},
};

static const struct miphy_initval miphylp28_initvals0_pcie[] = {
	/* Putting Macro in reset */
	{0x00, 0x01}, {0x00, 0x03},
	/* Wait for a while */
	{0x00, 0x01}, {0x04, 0x14}, {0xeb, 0x1d}, {0x0d, 0x1e},
	{0xd4, 0xa6}, {0xd5, 0xaa}, {0xd6, 0xaa}, {0xd7, 0x00},
	{0xd3, 0x00}, {0x0a, 0x40}, {0x4e, 0xd1}, {0x99, 0x5f},
	{0x0f, 0x00}, {0x0e, 0x05}, {0x63, 0x00}, {0x64, 0xa5},
	{0x49, 0x07}, {0x4a, 0x71}, {0x4b, 0x60}, {0x78, 0x98},
	{0x7a, 0x0d}, {0x7b, 0x00}, {0x7f, 0x79}, {0x80, 0x56},
	{0x0f, 0x01}, {0x0e, 0x0a}, {0x63, 0x00}, {0x64, 0xa5},
	{0x49, 0x07}, {0x4a, 0x70}, {0x4b, 0x60}, {0x78, 0xcc},
	{0x7a, 0x0d}, {0x7b, 0x00}, {0x7f, 0x79}, {0x80, 0x56},
	{0xcd, 0x21}, {0x00, 0x00}, {0x01, 0x05}, {0xe9, 0x00},
	{0x0d, 0x1e}, {0x3a, 0x40}, {0x62, 0x02}, {0x65, 0xC0},
};

static const struct miphy_initval miphylp28_initvals1_pcie[] = {
	{0x01, 0x01}, {0x01, 0x00},
	{0xe9, 0x40}, {0xe3, 0x02},
};

static const struct miphy_initval miphylp28_initvals_usb3[] = {
	/* Putting Macro in reset */
	{0x00, 0x01}, {0x00, 0x03},
	/* Wait for a while */
	{0x00, 0x01}, {0x04, 0x1C},
	/* PLL calibration */
	{0xEB, 0x1D}, {0x0D, 0x1E}, {0x0F, 0x00}, {0xC4, 0x70},
	{0xC9, 0x02}, {0xCA, 0x02}, {0xCB, 0x02}, {0xCC, 0x0A},
	/* Writing The PLL Ratio */
	{0xD4, 0xA6}, {0xD5, 0xAA}, {0xD6, 0xAA}, {0xD7, 0x04},
	{0xD3, 0x00},
	/* Writing The Speed Rate */
	{0x0F, 0x00}, {0x0E, 0x0A},
	/* RX Channel compensation and calibration */
	{0xC2, 0x1C}, {0x97, 0x51}, {0x98, 0x70}, {0x99, 0x5F},
	{0x9A, 0x22}, {0x9F, 0x0E},

	{0x7A, 0x05}, {0x7F, 0x78}, {0x30, 0x1B},
	/* Enable GENSEL_SEL and SSC */
	/* TX_SEL=0 swing preemp forced by pipe registres */
	{0x0A, 0x11},
	/* MIPHY Bias boost */
	{0x63, 0x00}, {0x64, 0xA7},
	/* TX compensation offset to re-center TX impedance */
	{0x42, 0x02},
	/* SSC modulation */
	{0x0C, 0x04},
	/* MIPHY TX control */
	{0x0F, 0x00}, {0xE5, 0x5A}, {0xE6, 0xA0}, {0xE4, 0x3C},
	{0xE6, 0xA1}, {0xE3, 0x00}, {0xE3, 0x02}, {0xE3, 0x00},
	/* Rx PI controller settings */
	{0x78, 0xCA},
	/* MIPHY RX input bridge control */
	/* INPUT_BRIDGE_EN_SW=1, manual input bridge control[0]=1 */
	{0xCD, 0x21}, {0xCD, 0x29}, {0xCE, 0x1A},
	/* MIPHY Reset */
	{0x00, 0x01}, {0x00, 0x00}, {0x01, 0x04}, {0x01, 0x05},
	{0xE9, 0x00}, {0x0D, 0x1E}, {0x3A, 0x40}, {0x01, 0x01},
	{0x01, 0x00}, {0xE9, 0x40}, {0x0F, 0x00}, {0x0B, 0x00},
	{0x62, 0x00}, {0x0F, 0x00}, {0xE3, 0x02}, {0x26, 0xA5},
	{0x0F, 0x00},
};

static void miphy_write_initvals(struct miphy28lp_phy *phy_miphy,
				 const struct miphy_initval *initvals,
				 int count)
{
	int i;
	struct miphy28lp_dev *miphy_dev = to_miphy28lp_dev(phy_miphy);

	for (i = 0; i < count; i++) {
		dev_dbg(miphy_dev->dev, "MiPHY28LP reg: 0x%x=0x%x\n",
			initvals[i].reg, initvals[i].val);
		if (i == 2)
			usleep_range(10, 20); /* extra delay after resetting */
		writeb_relaxed(initvals[i].val,
			       phy_miphy->base + initvals[i].reg);
	}
}

static inline int miphy_is_ready(struct miphy28lp_phy *phy_miphy)
{
	unsigned long finish = jiffies + 5 * HZ;
	u8 mask = MIPHY_PLL_HFC_RDY;
	u8 val;
	struct miphy28lp_dev *miphy_dev = to_miphy28lp_dev(phy_miphy);

	/*
	 * For PCIe and USB3 check only that PLL and HFC are ready
	 * For SATA check also that phy is ready!
	 */
	if (miphy_dev->miphy_device_conf == MIPHY28LP_SATA)
		mask |= MIPHY_PHY_RDY;

	do {
		val = readb_relaxed(phy_miphy->base + MIPHY_STATUS_1);
		if ((val & mask) != mask)
			cpu_relax();
		else
			return 0;
	} while (!time_after_eq(jiffies, finish));

	return -EBUSY;
}

static int miphy_osc_is_ready(struct miphy28lp_dev *miphy_dev)
{
	u32 val;
	unsigned long finish = jiffies + 5 * HZ;

	if (!miphy_dev->osc_rdy)
		return 0;

	if (!miphy_dev->syscfg_miphy_status)
		return -EINVAL;

	do {
		regmap_read(miphy_dev->regmap, miphy_dev->syscfg_miphy_status,
			    &val);
		if ((val & MIPHY_OSC_RDY) != MIPHY_OSC_RDY)
			cpu_relax();
		else
			return 0;
	} while (!time_after_eq(jiffies, finish));

	return -EBUSY;
}

static void miphy28lp_phy_get_glue(struct platform_device *pdev,
				   struct miphy28lp_dev *miphy_dev)
{
	struct resource *res;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
					   "miphy-ctrl-glue");
	if (!res)
		dev_warn(miphy_dev->dev, "No res for sysconf MiPHY CTRL\n");
	else
		miphy_dev->syscfg_miphy = res->start;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
					   "miphy-status-glue");
	if (!res)
		dev_warn(miphy_dev->dev, "No res for sysconf MiPHY status\n");
	else
		miphy_dev->syscfg_miphy_status = res->start;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "pcie-glue");
	if (!res)
		dev_warn(miphy_dev->dev, "No res for sysconf PCI glue\n");
	else
		miphy_dev->syscfg_pci = res->start;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "sata-glue");
	if (!res)
		dev_warn(miphy_dev->dev, "No res for sysconf SATA glue\n");
	else
		miphy_dev->syscfg_sata = res->start;
}

/* MiPHY reset and sysconf setup */
static int miphy28lp_setup(struct miphy28lp_dev *miphy_dev, u32 miphy_val)
{
	int err;

	if (!miphy_dev->syscfg_miphy)
		return -EINVAL;

	err = reset_control_assert(miphy_dev->miphy_rst);
	if (err) {
		dev_err(miphy_dev->dev, "unable to bring out of miphy reset\n");
		return err;
	}

	if (miphy_dev->osc_force_ext)
		miphy_val |= MIPHY_OSC_FORCE_EXT;

	regmap_update_bits(miphy_dev->regmap, miphy_dev->syscfg_miphy,
			   MIPHY_CTRL_MASK, miphy_val);

	err = reset_control_deassert(miphy_dev->miphy_rst);
	if (err) {
		dev_err(miphy_dev->dev, "unable to bring out of miphy reset\n");
		return err;
	}

	return miphy_osc_is_ready(miphy_dev);
}

static void miphy_sata_tune_ssc(struct miphy28lp_dev *miphy_dev,
				struct miphy28lp_phy *phy_miphy)
{
	u8 val;
	struct device_node *np = miphy_dev->dev->of_node;

	/* Compensate Tx impedance to avoid out of range values */
	if (of_property_read_bool(np, "st,ssc-on")) {
		/*
		 * Enable the SSC on PLL for all banks
		 * SSC Modulation @ 31 KHz and 4000 ppm modulation amp
		 */

		val = readb_relaxed(phy_miphy->base + 0x0C);
		val |= 0x04;
		writeb_relaxed(val, phy_miphy->base + 0x0C);
		val = readb_relaxed(phy_miphy->base + 0x0A);
		val |= 0x10;
		writeb_relaxed(val, phy_miphy->base + 0x0A);

		for (val = 0; val < 3; val++) {
			writeb_relaxed(val, phy_miphy->base + 0x0F);
			writeb_relaxed(0x3C, phy_miphy->base + 0xE4);
			writeb_relaxed(0x6C, phy_miphy->base + 0xE5);
			writeb_relaxed(0x81, phy_miphy->base + 0xE6);
			writeb_relaxed(0x00, phy_miphy->base + 0xE3);
			writeb_relaxed(0x02, phy_miphy->base + 0xE3);
			writeb_relaxed(0x00, phy_miphy->base + 0xE3);
		}

	}

	return;
}

static int miphy28lp_phy_init_sata(struct miphy28lp_phy *phy_miphy)
{
	u8 val;
	int count, err, sata_conf = SATA_CTRL_SELECT_SATA;
	struct miphy28lp_dev *miphy_dev = to_miphy28lp_dev(phy_miphy);

	dev_info(miphy_dev->dev, "%s\n", __func__);

	if ((!miphy_dev->syscfg_sata) || (!miphy_dev->syscfg_pci)
		|| (!phy_miphy->sata_up_base))
		return -EINVAL;

	phy_miphy->base = phy_miphy->sata_up_base;

	dev_info(miphy_dev->dev, "sata-up mode, addr 0x%p\n", phy_miphy->base);

	/* Configure the glue-logic */
	sata_conf |= (miphy_dev->sata_gen << SATA_SPDMODE);
	regmap_update_bits(miphy_dev->regmap, miphy_dev->syscfg_sata,
			   SATA_CTRL_MASK, sata_conf);
	regmap_update_bits(miphy_dev->regmap, miphy_dev->syscfg_pci,
			   PCIE_CTRL_MASK, SATA_CTRL_SELECT_PCIE);

	/* MiPHY path and clocking init */
	err = miphy28lp_setup(miphy_dev, MIPHY_CTRL_DEFAULT);
	if (err) {
		dev_err(miphy_dev->dev, "SATA phy setup failed\n");
		return err;
	}

	/* initialize miphy */
	count = ARRAY_SIZE(miphylp28_initvals_sata);
	miphy_write_initvals(phy_miphy, miphylp28_initvals_sata, count);

	if (miphy_dev->px_rx_pol_inv) {
		/* Invert Rx polarity */
		val = readb_relaxed(phy_miphy->base + MIPHY_CTRL_REG);
		val |= MIPHY_PX_RX_POL;
		writeb_relaxed(val, phy_miphy->base + MIPHY_CTRL_REG);
	}

	miphy_sata_tune_ssc(miphy_dev, phy_miphy);

	return miphy_is_ready(phy_miphy);
}

static void miphy_pcie_tune_ssc(struct miphy28lp_dev *miphy_dev,
				struct miphy28lp_phy *phy_miphy)
{
	u8 val;
	struct device_node *np = miphy_dev->dev->of_node;

	/* Compensate Tx impedance to avoid out of range values */
	if (of_property_read_bool(np, "st,ssc-on")) {
		/*
		 * Enable the SSC on PLL for all banks
		 * SSC Modulation @ 31 KHz and 4000 ppm modulation amp
		 */
		val = readb_relaxed(phy_miphy->base + 0x0C);
		val |= 0x04;
		writeb_relaxed(val, phy_miphy->base + 0x0C);
		val = readb_relaxed(phy_miphy->base + 0x0A);
		val |= 0x10;
		writeb_relaxed(val, phy_miphy->base + 0x0A);

		for (val = 0; val < 2; val++) {
			writeb_relaxed(val, phy_miphy->base + 0x0F);
			writeb_relaxed(0x69, phy_miphy->base + 0xE5);
			writeb_relaxed(0x21, phy_miphy->base + 0xE6);
			writeb_relaxed(0x3c, phy_miphy->base + 0xE4);
			writeb_relaxed(0x21, phy_miphy->base + 0xE6);
			writeb_relaxed(0x00, phy_miphy->base + 0xE3);
			writeb_relaxed(0x02, phy_miphy->base + 0xE3);
			writeb_relaxed(0x00, phy_miphy->base + 0xE3);
		}

	}
}

static int miphy28lp_phy_init_pcie(struct miphy28lp_phy *phy_miphy)
{
	int count, err;
	u8 val;
	unsigned long finish = jiffies + 5 * HZ;
	struct miphy28lp_dev *miphy_dev = to_miphy28lp_dev(phy_miphy);

	dev_info(miphy_dev->dev, "%s\n", __func__);

	if ((!miphy_dev->syscfg_sata) || (!miphy_dev->syscfg_pci)
		|| (!phy_miphy->pcie_up_base) || (!phy_miphy->pipebase))
		return -EINVAL;

	phy_miphy->base = phy_miphy->pcie_up_base;

	dev_info(miphy_dev->dev, "pcie-up mode, addr 0x%p\n", phy_miphy->base);

	/* Configure the glue-logic */
	regmap_update_bits(miphy_dev->regmap, miphy_dev->syscfg_sata,
			   SATA_CTRL_MASK, SATA_CTRL_SELECT_PCIE);
	regmap_update_bits(miphy_dev->regmap, miphy_dev->syscfg_pci,
			   PCIE_CTRL_MASK, SYSCFG_PCIE_PCIE_VAL);

	if (miphy_dev->data != MIPHY28LP_SEC_REG) {
		/* MiPHY path and clocking init */
		err = miphy28lp_setup(miphy_dev, MIPHY_CTRL_DEFAULT);
		if (err) {
			dev_err(miphy_dev->dev, "PCIe phy setup failed\n");
			return err;
		}
	}

	count = ARRAY_SIZE(miphylp28_initvals0_pcie);
	miphy_write_initvals(phy_miphy, miphylp28_initvals0_pcie, count);

	/* extra delay to wait pll lock */
	usleep_range(100, 120);

	count = ARRAY_SIZE(miphylp28_initvals1_pcie);
	miphy_write_initvals(phy_miphy, miphylp28_initvals1_pcie, count);

	miphy_pcie_tune_ssc(miphy_dev, phy_miphy);
	/* Waiting for Compensation to complete */
	do {
		val = readb_relaxed(phy_miphy->base + MIPHY_COMP_FSM_6);
		if (time_after_eq(jiffies, finish))
			return -EBUSY;
		cpu_relax();
	} while (!(val & MIPHY_COMP_DONE));

	/* PIPE Wrapper Configuration */
	writeb_relaxed(0x68, phy_miphy->pipebase + 0x104); /* Rise_0 */
	writeb_relaxed(0x61, phy_miphy->pipebase + 0x105); /* Rise_1 */
	writeb_relaxed(0x68, phy_miphy->pipebase + 0x108); /* Fall_0 */
	writeb_relaxed(0x61, phy_miphy->pipebase + 0x109); /* Fall-1 */
	writeb_relaxed(0x68, phy_miphy->pipebase + 0x10c); /* Threshhold_0 */
	writeb_relaxed(0x60, phy_miphy->pipebase + 0x10d); /* Threshold_1 */

	/* Wait for phy_ready */
	return miphy_is_ready(phy_miphy);
}

static int miphy28lp_phy_init_usb3(struct miphy28lp_phy *phy_miphy)
{
	int count, err;
	struct miphy28lp_dev *miphy_dev = to_miphy28lp_dev(phy_miphy);

	dev_info(miphy_dev->dev, "%s\n", __func__);

	if ((!phy_miphy->usb3_up_base) || (!phy_miphy->pipebase))
		return -EINVAL;

	phy_miphy->base = phy_miphy->usb3_up_base;

	dev_info(miphy_dev->dev, "usb3-up mode, addr 0x%p\n", phy_miphy->base);

	/* MiPHY path and clocking init */
	err = miphy28lp_setup(miphy_dev, MIPHY_CTRL_SYNC_D_EN);
	if (err) {
		dev_err(miphy_dev->dev, "USB3 phy setup failed\n");
		return err;
	}

	count = ARRAY_SIZE(miphylp28_initvals_usb3);
	miphy_write_initvals(phy_miphy, miphylp28_initvals_usb3, count);

	/* PIPE Wrapper Configuration */
	writeb_relaxed(0x68, phy_miphy->pipebase + 0x23);
	writeb_relaxed(0x61, phy_miphy->pipebase + 0x24);
	writeb_relaxed(0x68, phy_miphy->pipebase + 0x26);
	writeb_relaxed(0x61, phy_miphy->pipebase + 0x27);
	writeb_relaxed(0x18, phy_miphy->pipebase + 0x29);
	writeb_relaxed(0x60, phy_miphy->pipebase + 0x2a);

	/* pipe Wrapper usb3 TX swing de-emph margin PREEMPH[7:4], SWING[3:0] */
	writeb_relaxed(0X67, phy_miphy->pipebase + 0x68);
	writeb_relaxed(0X0D, phy_miphy->pipebase + 0x69);
	writeb_relaxed(0X67, phy_miphy->pipebase + 0x6A);
	writeb_relaxed(0X0D, phy_miphy->pipebase + 0x6B);
	writeb_relaxed(0X67, phy_miphy->pipebase + 0x6C);
	writeb_relaxed(0X0D, phy_miphy->pipebase + 0x6D);
	writeb_relaxed(0X67, phy_miphy->pipebase + 0x6E);
	writeb_relaxed(0X0D, phy_miphy->pipebase + 0x6F);

	return miphy_is_ready(phy_miphy);
}

static int miphy28lp_phy_init(struct phy *phy)
{
	int ret;
	struct miphy28lp_phy *phy_miphy = phy_get_drvdata(phy);
	struct miphy28lp_dev *miphy_dev = to_miphy28lp_dev(phy_miphy);

	mutex_lock(&miphy_dev->miphy_mutex);

	switch (miphy_dev->miphy_device_conf) {
	case MIPHY28LP_SATA:
		ret = miphy28lp_phy_init_sata(phy_miphy);
		break;
	case MIPHY28LP_PCIE:
		ret = miphy28lp_phy_init_pcie(phy_miphy);
		break;
	case MIPHY28LP_USB3:
		ret = miphy28lp_phy_init_usb3(phy_miphy);
		break;
	default:
		return -EINVAL;
	}

	mutex_unlock(&miphy_dev->miphy_mutex);

	return ret;
}

static struct phy *miphy28lp_phy_xlate(struct device *dev,
				       struct of_phandle_args *args)
{
	struct miphy28lp_dev *state = dev_get_drvdata(dev);

	state->miphy_device_conf = -EINVAL;

	if (WARN_ON(args->args[0] > PHYS_NUM))
		return ERR_PTR(-ENODEV);

	state->miphy_device_conf = args->args[0];

	return state->miphy.phy;
}

static int miphy28lp_phy_power_off(struct phy *x)
{
	return 0;
}

static int miphy28lp_phy_power_on(struct phy *x)
{
	return 0;
}

static struct phy_ops miphy28lp_phy_ops = {
	.init = miphy28lp_phy_init,
	.owner = THIS_MODULE,
	.power_on	= miphy28lp_phy_power_on,
	.power_off	= miphy28lp_phy_power_off,
};

static const struct of_device_id miphy28lp_phy_of_match[] = {
	{.compatible = "st,miphy28lp-phy",},
	{.compatible = "st,miphy28lp-phy-sec", .data = (void *)MIPHY28LP_SEC_REG},
	{},
};

static int miphy28lp_phy_get_of(struct device_node *np,
				struct miphy28lp_dev *miphy_dev)
{
	const char *sata_gen;
	struct resource *res;
	struct platform_device *pdev = miphy_dev->pdev;

	miphy_dev->regmap = syscon_regmap_lookup_by_phandle(np, "st,syscfg");

	if (IS_ERR(miphy_dev->regmap)) {
		dev_err(miphy_dev->dev, "No syscfg phandle specified\n");
		return PTR_ERR(miphy_dev->regmap);
	}

	miphy_dev->osc_force_ext = of_property_read_bool(np,
							 "st,osc-force-ext");
	miphy_dev->osc_rdy = of_property_read_bool(np, "st,osc-rdy");

	miphy_dev->px_rx_pol_inv = of_property_read_bool(np,
							"st,px_rx_pol_inv");

	if (of_get_property(np, "st,sata_gen", NULL)) {
		of_property_read_string(np, "st,sata_gen", &sata_gen);
		if (!strcmp(sata_gen, "gen3"))
			miphy_dev->sata_gen = SATA_GEN3;
		else if (!strcmp(sata_gen, "gen2"))
			miphy_dev->sata_gen = SATA_GEN2;
		else
			miphy_dev->sata_gen = SATA_GEN1;
	} else
		miphy_dev->sata_gen = SATA_GEN1;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "sata-up");
	if (res) {
		miphy_dev->miphy.sata_up_base = devm_ioremap(&pdev->dev,
				res->start, resource_size(res));
		if (!miphy_dev->miphy.sata_up_base)
			return -ENOMEM;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "pcie-up");
	if (res) {
		miphy_dev->miphy.pcie_up_base = devm_ioremap(&pdev->dev,
				res->start, resource_size(res));
		if (!miphy_dev->miphy.pcie_up_base)
			return -ENOMEM;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "usb3-up");
	if (res) {
		miphy_dev->miphy.usb3_up_base = devm_ioremap(&pdev->dev,
				res->start, resource_size(res));
		if (!miphy_dev->miphy.usb3_up_base)
			return -ENOMEM;
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "pipew");
	if (res) {
		miphy_dev->miphy.pipebase = devm_ioremap(&pdev->dev,
				res->start, resource_size(res));
		if (!miphy_dev->miphy.pipebase)
			return -ENOMEM;
	}

	miphy28lp_phy_get_glue(pdev, miphy_dev);

	miphy_dev->data = (u32)of_match_node(miphy28lp_phy_of_match, np)->data;
	if (!miphy_dev->data)
		miphy_dev->data = 0;

	return 0;
}

static int miphy28lp_probe_resets(struct platform_device *pdev,
				  struct miphy28lp_dev *miphy_dev)
{
	int err;

	miphy_dev->miphy_rst = devm_reset_control_get(&pdev->dev,
						      "miphy-sw-rst");
	if (IS_ERR(miphy_dev->miphy_rst)) {
		dev_warn(&pdev->dev, "miphy soft reset control not defined\n");
		return 0;
	}

	err = reset_control_deassert(miphy_dev->miphy_rst);
	if (err) {
		dev_err(&pdev->dev, "unable to bring out of miphy reset\n");
		return err;
	}

	return 0;
}

static int miphy28lp_phy_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct miphy28lp_dev *miphy_dev;
	struct device *dev = &pdev->dev;
	struct phy_provider *phy_provider;
	struct phy *phy;
	int ret;

	miphy_dev = devm_kzalloc(dev, sizeof(*miphy_dev), GFP_KERNEL);
	if (!miphy_dev)
		return -ENOMEM;

	mutex_init(&miphy_dev->miphy_mutex);

	miphy_dev->dev = dev;
	miphy_dev->pdev = pdev;

	dev_set_drvdata(dev, miphy_dev);

	ret = miphy28lp_phy_get_of(np, miphy_dev);
	if (ret)
		return ret;

	ret = miphy28lp_probe_resets(pdev, miphy_dev);
	if (ret)
		return ret;

	phy_provider = devm_of_phy_provider_register(dev, miphy28lp_phy_xlate);
	if (IS_ERR(phy_provider))
		return PTR_ERR(phy_provider);

	phy = devm_phy_create(dev, &miphy28lp_phy_ops, NULL);
	if (IS_ERR(phy)) {
		dev_err(dev, "failed to create Display Port PHY\n");
		return PTR_ERR(phy);
	}

	miphy_dev->miphy.phy = phy;

	phy_set_drvdata(phy, &miphy_dev->miphy);

	return 0;
}

MODULE_DEVICE_TABLE(of, miphy28lp_phy_of_match);

static struct platform_driver miphy28lp_phy_driver = {
	.probe = miphy28lp_phy_probe,
	.driver = {
		   .name = "miphy28lp-phy",
		   .owner = THIS_MODULE,
		   .of_match_table = miphy28lp_phy_of_match,
		   }
};

module_platform_driver(miphy28lp_phy_driver);

MODULE_AUTHOR("Alexandre Torgue <alexandre.torgue@st.com>");
MODULE_DESCRIPTION("STMicroelectronics miphy28lp driver");
MODULE_LICENSE("GPL v2");
