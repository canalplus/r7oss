/*
 * Broadcom BCM7xxx internal transceivers support.
 *
 * Copyright (C) 2013, Broadcom Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/phy.h>
#include <linux/delay.h>
#include <linux/mdio.h>
#include <linux/brcmphy.h>
#include <linux/clk.h>

struct bcm7xxx_phy_priv {
	struct clk	*clk;
};

static int bcm7xxx_apd_enable(struct phy_device *phydev)
{
	int val;

	/* Enable powering down of the DLL during auto-power down */
	val = bcm54xx_shadow_read(phydev, BCM54XX_SHD_SCR3);
	if (val < 0)
		return val;

	val |= BCM54XX_SHD_SCR3_DLLAPD_DIS;
	bcm54xx_shadow_write(phydev, BCM54XX_SHD_SCR3, val);

	/* Enable auto-power down */
	val = bcm54xx_shadow_read(phydev, BCM54XX_SHD_APD);
	if (val < 0)
		return val;

	val |= BCM54XX_SHD_APD_EN;
	return bcm54xx_shadow_write(phydev, BCM54XX_SHD_APD, val);
}

static int bcm7xxx_eee_enable(struct phy_device *phydev)
{
	int val;

	val = phy_read_mmd_indirect(phydev->bus, CL45VEN_EEE_CONTROL,
				    MDIO_MMD_AN, phydev->addr);
	if (val < 0)
		return val;

	/* Enable general EEE feature at the PHY level */
	val |= LPI_FEATURE_EN | LPI_FEATURE_EN_DIG1000X;

	phy_write_mmd_indirect(phydev->bus, CL45VEN_EEE_CONTROL,
			       MDIO_MMD_AN, phydev->addr, val);

	/* Advertise supported modes */
	val = phy_read_mmd_indirect(phydev->bus, MDIO_AN_EEE_ADV,
				    MDIO_MMD_AN, phydev->addr);

	val |= (MDIO_AN_EEE_ADV_100TX | MDIO_AN_EEE_ADV_1000T);
	phy_write_mmd_indirect(phydev->bus, MDIO_AN_EEE_ADV,
			       MDIO_MMD_AN, phydev->addr, val);

	return 0;
}

static void phy_write_exp(struct phy_device *phydev,
					u16 reg, u16 value)
{
	phy_write(phydev, 0x17, 0xf00 | reg);
	phy_write(phydev, 0x15, value);
}

static void phy_write_misc(struct phy_device *phydev,
					u16 reg, u16 chl, u16 value)
{
	int tmp;

	phy_write(phydev, 0x18, 0x7);

	tmp = phy_read(phydev, 0x18);
	tmp |= 0x800;
	phy_write(phydev, 0x18, tmp);

	tmp = (chl * 0x2000) | reg;
	phy_write(phydev, 0x17, tmp);

	phy_write(phydev, 0x15, value);
}

static void r_rc_cal_reset(struct phy_device *phydev)
{
	/* Reset R_CAL/RC_CAL Engine */
	phy_write_exp(phydev, 0x00b0, 0x0010);

	/* Disable Reset R_AL/RC_CAL Engine */
	phy_write_exp(phydev, 0x00b0, 0x0000);
}

static int bcm7xxx_28nm_b0_afe_config_init(struct phy_device *phydev)
{
	/* Increase VCO range to prevent unlocking problem of PLL at low
	 * temp
	 */
	phy_write_misc(phydev, 0x0032, 0x0001, 0x0048);

	/* Change Ki to 011 */
	phy_write_misc(phydev, 0x0032, 0x0002, 0x021b);

	/* Disable loading of TVCO buffer to bandgap, set bandgap trim
	 * to 111
	 */
	phy_write_misc(phydev, 0x0033, 0x0000, 0x0e20);

	/* Adjust bias current trim by -3 */
	phy_write_misc(phydev, 0x000a, 0x0000, 0x690b);

	/* Switch to CORE_BASE1E */
	phy_write(phydev, 0x1e, 0xd);

	r_rc_cal_reset(phydev);

	/* write AFE_RXCONFIG_0 */
	phy_write_misc(phydev, 0x38, 0x0000, 0xeb19);

	/* write AFE_RXCONFIG_1 */
	phy_write_misc(phydev, 0x38, 0x0001, 0x9a3f);

	/* write AFE_RX_LP_COUNTER */
	phy_write_misc(phydev, 0x38, 0x0003, 0x7fc0);

	/* write AFE_HPF_TRIM_OTHERS */
	phy_write_misc(phydev, 0x3A, 0x0000, 0x000b);

	/* write AFTE_TX_CONFIG */
	phy_write_misc(phydev, 0x39, 0x0000, 0x0800);

	return 0;
}

static int bcm7xxx_28nm_d0_afe_config_init(struct phy_device *phydev)
{
	/* AFE_RXCONFIG_0 */
	phy_write_misc(phydev, 0x0038, 0x0000, 0xeb15);

	/* AFE_RXCONFIG_1 */
	phy_write_misc(phydev, 0x0038, 0x0001, 0x9b2f);

	/* AFE_RXCONFIG_2, set rCal offset for HT=0 code and LT=-2 code */
	phy_write_misc(phydev, 0x0038, 0x0002, 0x2003);

	/* AFE_RX_LP_COUNTER, set RX bandwidth to maximum */
	phy_write_misc(phydev, 0x0038, 0x0003, 0x7fc0);

	/* AFE_TX_CONFIG, set 100BT Cfeed=011 to improve rise/fall time */
	phy_write_misc(phydev, 0x0039, 0x0000, 0x0431);

	/* AFE_VDCA_ICTRL_0, set Iq=1101 instead of 0111 for AB symmetry */
	phy_write_misc(phydev, 0x0039, 0x0001, 0xa7da);

	/* AFE_VDAC_OTHERS_0, set 1000BT Cidac=010 for all ports */
	phy_write_misc(phydev, 0x0039, 0x0003, 0xa020);

	/* AFE_HPF_TRIM_OTHERS, set 100Tx/10BT to -4.5% swing and set rCal
	 * offset for HT=0 code
	 */
	phy_write_misc(phydev, 0x003a, 0x0000, 0x00e3);

	/* CORE_BASE1E, force trim to overwrite and set I_ext trim to 0000 */
	phy_write(phydev, 0x001e, 0x0010);

	/* DSP_TAP10, adjust bias current trim (+0% swing, +0 tick) */
	phy_write_misc(phydev, 0x000a, 0x0000, 0x011b);

	/* Reset R_CAL/RC_CAL engine */
	r_rc_cal_reset(phydev);

	return 0;
}

static int bcm7xxx_28nm_e0_plus_afe_config_init(struct phy_device *phydev)
{
	/* AFE_RXCONFIG_1, provide more margin for INL/DNL measurement on ATE */
	phy_write_misc(phydev, 0x0038, 0x0001, 0x9b2f);

	/* AFE_TX_CONFIG, set 100BT Cfeed=011 to improve rise/fall time */
	phy_write_misc(phydev, 0x0039, 0x0000, 0x0431);

	/* AFE_VDCA_ICTRL_0, set Iq=1101 instead of 0111 for AB symmetry */
	phy_write_misc(phydev, 0x0039, 0x0001, 0xa7da);

	/* AFE_HPF_TRIM_OTHERS, set 100Tx/10BT to -4.5% swing and set rCal
	 * offset for HT=0 code
	 */
	phy_write_misc(phydev, 0x003a, 0x0000, 0x00e3);

	/* CORE_BASE1E, force trim to overwrite and set I_ext trim to 0000 */
	phy_write(phydev, 0x001e, 0x0010);

	/* DSP_TAP10, adjust bias current trim (+0% swing, +0 tick) */
	phy_write_misc(phydev, 0x000a, 0x0000, 0x011b);

	/* Reset R_CAL/RC_CAL engine */
	r_rc_cal_reset(phydev);

	return 0;
}

static int bcm7xxx_28nm_config_init(struct phy_device *phydev)
{
	u8 rev = PHY_BRCM_7XXX_REV(phydev->dev_flags);
	u8 patch = PHY_BRCM_7XXX_PATCH(phydev->dev_flags);
	int ret = 0;

	pr_info_once("%s: %s PHY revision: 0x%02x, patch: %d\n",
		     dev_name(&phydev->dev), phydev->drv->name, rev, patch);

	/* Dummy read to a register to workaround an issue upon reset where the
	 * internal inverter may not allow the first MDIO transaction to pass
	 * the MDIO management controller and make us return 0xffff for such
	 * reads.
	 */
	phy_read(phydev, MII_BMSR);

	switch (rev) {
	case 0xb0:
		ret = bcm7xxx_28nm_b0_afe_config_init(phydev);
		break;
	case 0xd0:
		ret = bcm7xxx_28nm_d0_afe_config_init(phydev);
		break;
	case 0xe0:
	case 0xf0:
	/* Rev G0 introduces a roll over */
	case 0x10:
		ret = bcm7xxx_28nm_e0_plus_afe_config_init(phydev);
		break;
	default:
		break;
	}

	if (ret)
		return ret;

	ret = bcm7xxx_eee_enable(phydev);
	if (ret)
		return ret;

	return bcm7xxx_apd_enable(phydev);
}

static int bcm7xxx_28nm_resume(struct phy_device *phydev)
{
	int ret;

	ret = bcm7xxx_28nm_config_init(phydev);
	if (ret)
		return ret;

	return genphy_config_aneg(phydev);
}

static int phy_set_clr_bits(struct phy_device *dev, int location,
					int set_mask, int clr_mask)
{
	int v, ret;

	v = phy_read(dev, location);
	if (v < 0)
		return v;

	v &= ~clr_mask;
	v |= set_mask;

	ret = phy_write(dev, location, v);
	if (ret < 0)
		return ret;

	return v;
}

static int bcm7xxx_config_init(struct phy_device *phydev)
{
	/* Enable 64 clock MDIO */
	phy_write(phydev, 0x1d, 0x1000);
	phy_read(phydev, 0x1d);

	/* Workaround only required for 100Mbits/sec capable PHYs */
	if (phydev->supported & PHY_GBIT_FEATURES)
		return 0;

	/* set shadow mode 2 */
	phy_set_clr_bits(phydev, 0x1f, 0x0004, 0x0004);

	/* Workaround for SWLINUX-2281: explicitly reset IDDQ_CLKBIAS
	 * in the Shadow 2 regset, due to power sequencing issues.
	 */
	/* set iddq_clkbias */
	phy_write(phydev, 0x14, 0x0F00);
	udelay(10);
	/* reset iddq_clkbias */
	phy_write(phydev, 0x14, 0x0C00);

	/* Workaround for SWLINUX-2056: fix timing issue between the ephy
	 * digital and the ephy analog blocks.  This clock inversion will
	 * inherently fix any setup and hold issue.
	 */
	phy_write(phydev, 0x13, 0x7555);

	/* reset shadow mode 2 */
	phy_set_clr_bits(phydev, 0x1f, 0x0004, 0);

	return 0;
}

/* Workaround for putting the PHY in IDDQ mode, required
 * for all BCM7XXX 40nm and 65nm PHYs
 */
static int bcm7xxx_suspend(struct phy_device *phydev)
{
	int ret;
	const struct bcm7xxx_regs {
		int reg;
		u16 value;
	} bcm7xxx_suspend_cfg[] = {
		{ 0x1f, 0x008b },
		{ 0x10, 0x01c0 },
		{ 0x14, 0x7000 },
		{ 0x1f, 0x000f },
		{ 0x10, 0x20d0 },
		{ 0x1f, 0x000b },
	};
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(bcm7xxx_suspend_cfg); i++) {
		ret = phy_write(phydev,
				bcm7xxx_suspend_cfg[i].reg,
				bcm7xxx_suspend_cfg[i].value);
		if (ret)
			return ret;
	}

	return 0;
}

static int bcm7xxx_dummy_config_init(struct phy_device *phydev)
{
	return 0;
}

static int bcm7xxx_28nm_probe(struct phy_device *phydev)
{
	struct bcm7xxx_phy_priv *priv;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	phydev->priv = priv;

	priv->clk = clk_get(&phydev->dev, "sw_gphy");
	if (IS_ERR(priv->clk)) {
		dev_err(&phydev->dev, "failed to request GPHY clock\n");
		priv->clk = NULL;
	}

	clk_prepare_enable(priv->clk);

	return 0;
}

static void bcm7xxx_28nm_remove(struct phy_device *phydev)
{
	struct bcm7xxx_phy_priv *priv = phydev->priv;

	clk_disable_unprepare(priv->clk);
	clk_put(priv->clk);
	kfree(priv);
	phydev->priv = NULL;
}

#define BCM7XXX_28NM_GPHY(_oui, _name)					\
{									\
	.phy_id		= (_oui),					\
	.phy_id_mask	= 0xfffffff0,					\
	.name		= _name,					\
	.features	= PHY_GBIT_FEATURES |				\
			  SUPPORTED_Pause | SUPPORTED_Asym_Pause,	\
	.flags		= PHY_IS_INTERNAL,				\
	.probe		= bcm7xxx_28nm_probe,				\
	.remove		= bcm7xxx_28nm_remove,				\
	.config_init	= bcm7xxx_28nm_config_init,			\
	.config_aneg	= genphy_config_aneg,				\
	.read_status	= genphy_read_status,				\
	.resume		= bcm7xxx_28nm_resume,				\
	.driver		= { .owner = THIS_MODULE },			\
}

static struct phy_driver bcm7xxx_driver[] = {
	BCM7XXX_28NM_GPHY(PHY_ID_BCM7250, "Broadcom BCM7250"),
	BCM7XXX_28NM_GPHY(PHY_ID_BCM7364, "Broadcom BCM7364"),
	BCM7XXX_28NM_GPHY(PHY_ID_BCM7366, "Broadcom BCM7366"),
	BCM7XXX_28NM_GPHY(PHY_ID_BCM74371, "Broadcom BCM74371"),
	BCM7XXX_28NM_GPHY(PHY_ID_BCM7439, "Broadcom BCM7439"),
	BCM7XXX_28NM_GPHY(PHY_ID_BCM7439_2, "Broadcom BCM7439 (2)"),
	BCM7XXX_28NM_GPHY(PHY_ID_BCM7445, "Broadcom BCM7445"),
	BCM7XXX_28NM_GPHY(PHY_ID_BCM7455, "Broadcom BCM7455"),
{
	.phy_id		= PHY_BCM_OUI_4,
	.phy_id_mask	= 0xffff0000,
	.name		= "Broadcom BCM7XXX 40nm",
	.features	= PHY_GBIT_FEATURES |
			  SUPPORTED_Pause | SUPPORTED_Asym_Pause,
	.flags		= PHY_IS_INTERNAL,
	.probe		= bcm7xxx_28nm_probe,
	.remove		= bcm7xxx_28nm_remove,
	.config_init	= bcm7xxx_config_init,
	.config_aneg	= genphy_config_aneg,
	.read_status	= genphy_read_status,
	.suspend	= bcm7xxx_suspend,
	.resume		= bcm7xxx_config_init,
	.driver		= { .owner = THIS_MODULE },
}, {
	.phy_id		= PHY_BCM_OUI_5,
	.phy_id_mask	= 0xffffff00,
	.name		= "Broadcom BCM7XXX 65nm",
	.features	= PHY_BASIC_FEATURES |
			  SUPPORTED_Pause | SUPPORTED_Asym_Pause,
	.flags		= PHY_IS_INTERNAL,
	.probe		= bcm7xxx_28nm_probe,
	.remove		= bcm7xxx_28nm_remove,
	.config_init	= bcm7xxx_dummy_config_init,
	.config_aneg	= genphy_config_aneg,
	.read_status	= genphy_read_status,
	.suspend	= bcm7xxx_suspend,
	.resume		= bcm7xxx_config_init,
	.driver		= { .owner = THIS_MODULE },
} };

static struct mdio_device_id __maybe_unused bcm7xxx_tbl[] = {
	{ PHY_ID_BCM7250, 0xfffffff0, },
	{ PHY_ID_BCM7364, 0xfffffff0, },
	{ PHY_ID_BCM7366, 0xfffffff0, },
	{ PHY_ID_BCM74371, 0xfffffff0, },
	{ PHY_ID_BCM7439, 0xfffffff0, },
	{ PHY_ID_BCM7445, 0xfffffff0, },
	{ PHY_ID_BCM7455, 0xfffffff0, },
	{ PHY_BCM_OUI_4, 0xffff0000 },
	{ PHY_BCM_OUI_5, 0xffffff00 },
	{ }
};

static int __init bcm7xxx_phy_init(void)
{
	return phy_drivers_register(bcm7xxx_driver,
			ARRAY_SIZE(bcm7xxx_driver));
}

static void __exit bcm7xxx_phy_exit(void)
{
	phy_drivers_unregister(bcm7xxx_driver,
			ARRAY_SIZE(bcm7xxx_driver));
}

module_init(bcm7xxx_phy_init);
module_exit(bcm7xxx_phy_exit);

MODULE_DEVICE_TABLE(mdio, bcm7xxx_tbl);

MODULE_DESCRIPTION("Broadcom BCM7xxx internal PHY driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Broadcom Corporation");
