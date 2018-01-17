/*
 * sdhci-brcmstb.c Support for SDHCI on Broadcom SoC's
 *
 * Copyright (C) 2013 Broadcom Corporation
 *
 * Author: Al Cooper <acooper@broadcom.com>
 * Based on sdhci-dove.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/io.h>
#include <linux/mmc/host.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/brcmstb/brcmstb.h>

#include "sdhci-pltfm.h"

#define SDIO_CFG_REG(x, y)	(x + BCHP_SDIO_0_CFG_##y -	\
				BCHP_SDIO_0_CFG_REG_START)

struct sdhci_brcmstb_priv {
	void __iomem *cfg_regs;
	int host_driver_type;
	int host_hs_driver_type;
	int card_driver_type;
};

#define MASK_OFF_DRV (SDHCI_PRESET_SDCLK_FREQ_MASK |	\
			SDHCI_PRESET_CLKGEN_SEL_MASK)

static char *strength_type_to_string[] = {"B", "A", "C", "D"};

#if defined(CONFIG_BCM74371A0)
/*
 * HW7445-1183
 * Setting the RESET_ALL or RESET_DATA bits will hang the SDIO
 * core so don't allow these bits to be set. This workaround
 * allows the driver to be used for development and testing
 * but will prevent recovery from normally recoverable errors
 * and should NOT be used in production systems.
 */
static void sdhci_brcmstb_writeb(struct sdhci_host *host, u8 val, int reg)
{
	if (reg == SDHCI_SOFTWARE_RESET)
		val &= ~(SDHCI_RESET_ALL | SDHCI_RESET_DATA);
	writeb(val, host->ioaddr + reg);
}

/* We don't support drive strength override on chips that use the
 * old version of the SDIO core.
 */
static void set_host_driver_strength_overrides(
	struct sdhci_host *host,
	struct sdhci_brcmstb_priv *priv)
{
}

#else /* CONFIG_BCM74371A0 */

static void set_host_driver_strength_overrides(
	struct sdhci_host *host,
	struct sdhci_brcmstb_priv *priv)
{
	u16 strength;
	u16 sdr25;
	u16 sdr50;
	u16 ddr50;
	u16 sdr104;
	u32 val;
	u32 cfg_base = (u32)priv->cfg_regs;

	if (priv->host_driver_type) {
		dev_info(mmc_dev(host->mmc),
			"Changing UHS Host Driver TYPE Presets to TYPE %s\n",
			strength_type_to_string[priv->host_driver_type]);
		strength = (u16)priv->host_driver_type << 11;
		sdr25 = sdhci_readw(host,
				SDHCI_PRESET_FOR_SDR25) & MASK_OFF_DRV;
		sdr50 = sdhci_readw(host,
				SDHCI_PRESET_FOR_SDR50) & MASK_OFF_DRV;
		ddr50 = sdhci_readw(host,
				SDHCI_PRESET_FOR_DDR50) & MASK_OFF_DRV;
		sdr104 = sdhci_readw(host,
				SDHCI_PRESET_FOR_SDR104) & MASK_OFF_DRV;
		val = (sdr25 | strength);
		val |= ((u32)(sdr50 | strength)) << 16;
		val |= 0x80000000;
		DEV_WR(SDIO_CFG_REG(cfg_base, PRESET3), val);
		val = (sdr104 | strength);
		val |= ((u32)(ddr50 | strength)) << 16;
		val |= 0x80000000;
		DEV_WR(SDIO_CFG_REG(cfg_base, PRESET4), val);
	}

	/*
	 * The Host Controller Specification states that the driver
	 * strength setting is only valid for UHS modes, but our
	 * host controller allows this setting to be used for HS modes
	 * as well.
	 */
	if (priv->host_hs_driver_type) {
		u16 sdr12;
		u16 hs;

		dev_info(mmc_dev(host->mmc),
			"Changing HS Host Driver TYPE Presets to TYPE %s\n",
			strength_type_to_string[priv->host_hs_driver_type]);
		strength = (u16)priv->host_hs_driver_type << 11;
		sdr12 = sdhci_readw(host, SDHCI_PRESET_FOR_SDR12) &
			MASK_OFF_DRV;
		hs = sdhci_readw(host, SDHCI_PRESET_FOR_HS) & MASK_OFF_DRV;
		val = (hs | strength);
		val |= ((u32)(sdr12 | strength)) << 16;
		val |= 0x80000000;
		DEV_WR(SDIO_CFG_REG(cfg_base, PRESET2), val);
	}
}
#endif /* CONFIG_BCM74371A0 */

static int select_one_drive_strength(struct sdhci_host *host, int supported,
				int requested, char *type)
{
	char strength_ok_msg[] = "Changing %s Driver to TYPE %s\n";
	char strength_err_msg[] =
		"Request to change %s Driver to TYPE %s not supported by %s\n";
	if (supported & (1 << requested)) {
		if (requested)
			dev_info(mmc_dev(host->mmc), strength_ok_msg, type,
				strength_type_to_string[requested], type);
		return requested;
	} else {
		dev_warn(mmc_dev(host->mmc), strength_err_msg, type,
			strength_type_to_string[requested], type);
		return 0;
	}
}

static int sdhci_brcmstb_select_drive_strength(struct sdhci_host *host,
					struct mmc_card *card,
					unsigned int max_dtr, int host_drv,
					int card_drv, int *drv_type)
{
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_brcmstb_priv *priv = sdhci_pltfm_priv(pltfm_host);

	*drv_type = select_one_drive_strength(host, host_drv,
					priv->host_driver_type,	"Host");
	return select_one_drive_strength(host, card_drv,
					priv->card_driver_type,	"Card");
}

static struct sdhci_ops sdhci_brcmstb_ops = {
	.select_drive_strength	= sdhci_brcmstb_select_drive_strength,
};

static struct sdhci_pltfm_data sdhci_brcmstb_pdata = {
};

#if defined(CONFIG_BCM3390A0) || defined(CONFIG_BCM7250B0) ||	\
	defined(CONFIG_BCM7364A0) || defined(CONFIG_BCM7445D0)
static void sdhci_override_caps(struct sdhci_host *host,
				struct sdhci_brcmstb_priv *priv,
				uint32_t cap0_setbits,
				uint32_t cap0_clearbits,
				uint32_t cap1_setbits,
				uint32_t cap1_clearbits)
{
	uint32_t val;
	void *cfg_base = priv->cfg_regs;

	/*
	 * The CAP's override bits in the CFG registers default to all
	 * zeros so start by getting the correct settings from the HOST
	 * CAPS registers and then modify the requested bits and write
	 * them to the override CFG registers.
	 */
	val = sdhci_readl(host, SDHCI_CAPABILITIES);
	val &= ~cap0_clearbits;
	val |= cap0_setbits;
	DEV_WR(SDIO_CFG_REG(cfg_base, CAP_REG0), val);
	val = sdhci_readl(host, SDHCI_CAPABILITIES_1);
	val &= ~cap1_clearbits;
	val |= cap1_setbits;
	DEV_WR(SDIO_CFG_REG(cfg_base, CAP_REG1), val);
	DEV_WR(SDIO_CFG_REG(cfg_base, CAP_REG_OVERRIDE),
		BCHP_SDIO_0_CFG_CAP_REG_OVERRIDE_CAP_REG_OVERRIDE_MASK);
}

static void sdhci_fix_caps(struct sdhci_host *host,
			struct sdhci_brcmstb_priv *priv)
{
#if defined(CONFIG_BCM7445D0)
	/* Fixed for E0 and above */
	if (BRCM_CHIP_REV() >= 0x40)
		return;
#endif
	/* Disable SDR50 support because tuning is broken. */
	sdhci_override_caps(host, priv, 0, 0, 0, SDHCI_SUPPORT_SDR50);
}
#else
static void sdhci_fix_caps(struct sdhci_host *host,
			struct sdhci_brcmstb_priv *priv)
{
}
#endif

#ifdef CONFIG_PM_SLEEP

static int sdhci_brcmstb_suspend(struct device *dev)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	int res;

	res = sdhci_suspend_host(host);
	if (res)
		return res;
	clk_disable(pltfm_host->clk);
	return res;
}

static int sdhci_brcmstb_resume(struct device *dev)
{
	struct sdhci_host *host = dev_get_drvdata(dev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	struct sdhci_brcmstb_priv *priv = sdhci_pltfm_priv(pltfm_host);
	int err;

	err = clk_enable(pltfm_host->clk);
	if (err)
		return err;
	sdhci_fix_caps(host, priv);
	set_host_driver_strength_overrides(host, priv);
	return sdhci_resume_host(host);
}

#endif /* CONFIG_PM_SLEEP */

static SIMPLE_DEV_PM_OPS(sdhci_brcmstb_pmops, sdhci_brcmstb_suspend,
			sdhci_brcmstb_resume);

static void sdhci_brcmstb_of_get_driver_type(struct device_node *dn,
					char *name, int *dtype)
{
	const char *driver_type;
	int res;

	res = of_property_read_string(dn, name, &driver_type);
	if (res == 0) {
		if (strcmp(driver_type, "A") == 0)
			*dtype = MMC_SET_DRIVER_TYPE_A;
		else if (strcmp(driver_type, "B") == 0)
			*dtype = MMC_SET_DRIVER_TYPE_B;
		else if (strcmp(driver_type, "C") == 0)
			*dtype = MMC_SET_DRIVER_TYPE_C;
		else if (strcmp(driver_type, "D") == 0)
			*dtype = MMC_SET_DRIVER_TYPE_D;
	}
}


static int sdhci_brcmstb_probe(struct platform_device *pdev)
{
	struct device_node *dn = pdev->dev.of_node;
	struct sdhci_host *host;
	struct sdhci_pltfm_host *pltfm_host;
	struct sdhci_brcmstb_priv *priv;
	struct clk *clk;
	struct resource *resource;
	int res;

	clk = of_clk_get_by_name(dn, "sw_sdio");
	if (IS_ERR(clk)) {
		dev_err(&pdev->dev, "Clock not found in Device Tree\n");
		clk = NULL;
	}
	res = clk_prepare_enable(clk);
	if (res)
		goto undo_clk_get;

#if defined(CONFIG_BCM74371A0)
	/* Only enable reset workaround for 74371a0 senior */
	if (BRCM_CHIP_ID() == 0x7439)
		sdhci_brcmstb_ops.write_b = sdhci_brcmstb_writeb;
#endif /* CONFIG_BCM74371A0 */
	sdhci_brcmstb_pdata.ops = &sdhci_brcmstb_ops;
	host = sdhci_pltfm_init(pdev, &sdhci_brcmstb_pdata,
				sizeof(struct sdhci_brcmstb_priv));
	if (IS_ERR(host)) {
		res = PTR_ERR(host);
		goto undo_clk_prep;
	}
	sdhci_get_of_property(pdev);
	mmc_of_parse(host->mmc);
	pltfm_host = sdhci_priv(host);
	priv = sdhci_pltfm_priv(pltfm_host);
	resource = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (resource == NULL) {
		dev_err(&pdev->dev, "can't get SDHCI CFG base address\n");
		return -EINVAL;
	}
	priv->cfg_regs = devm_request_and_ioremap(&pdev->dev, resource);
	if (!priv->cfg_regs) {
		dev_err(&pdev->dev, "can't map register space\n");
		return -EINVAL;
	}
	sdhci_fix_caps(host, priv);

	sdhci_brcmstb_of_get_driver_type(dn, "host-driver-strength",
					&priv->host_driver_type);
	sdhci_brcmstb_of_get_driver_type(dn, "host-hs-driver-strength",
					&priv->host_hs_driver_type);
	sdhci_brcmstb_of_get_driver_type(dn, "card-driver-strength",
					&priv->card_driver_type);
	set_host_driver_strength_overrides(host, priv);

	res = sdhci_add_host(host);
	if (res)
		goto undo_pltfm_init;

	pltfm_host->clk = clk;
	return res;

undo_pltfm_init:
	sdhci_pltfm_free(pdev);
undo_clk_prep:
	clk_disable_unprepare(clk);
undo_clk_get:
	clk_put(clk);
	return res;
}

static int sdhci_brcmstb_remove(struct platform_device *pdev)
{
	struct sdhci_host *host = platform_get_drvdata(pdev);
	struct sdhci_pltfm_host *pltfm_host = sdhci_priv(host);
	int res;
	res = sdhci_pltfm_unregister(pdev);
	clk_disable_unprepare(pltfm_host->clk);
	clk_put(pltfm_host->clk);
	return res;
}


static const struct of_device_id sdhci_brcm_of_match[] = {
	{ .compatible = "brcm,sdhci-brcmstb" },
	{},
};

static struct platform_driver sdhci_brcmstb_driver = {
	.driver		= {
		.name	= "sdhci-brcmstb",
		.owner	= THIS_MODULE,
		.pm	= &sdhci_brcmstb_pmops,
		.of_match_table = of_match_ptr(sdhci_brcm_of_match),
	},
	.probe		= sdhci_brcmstb_probe,
	.remove		= sdhci_brcmstb_remove,
};

module_platform_driver(sdhci_brcmstb_driver);

MODULE_DESCRIPTION("SDHCI driver for Broadcom");
MODULE_AUTHOR("Al Cooper <acooper@broadcom.com>");
MODULE_LICENSE("GPL v2");
