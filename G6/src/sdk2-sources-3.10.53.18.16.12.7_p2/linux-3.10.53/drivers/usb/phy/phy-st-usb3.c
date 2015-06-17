/*
 * Copyright (C) 2013 STMicroelectronics
 *
 * STMicroelectronics PHY driver for MiPHY LP28 (MiPHY2 for USB3 device)
 *
 * Author: Giuseppe Cavallaro <peppe.cavallaro@st.com>
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
#include <linux/regmap.h>
#include <linux/reset.h>
#include <linux/delay.h>
#include <linux/mfd/syscon.h>
#include <linux/usb/phy.h>

#define phy_to_priv(x)	container_of((x), struct sti_usb3_miphy, phy)

#define SSC_ON	0x11
#define SSC_OFF	0x01

/* Set MIPHY timer to 2s */
#define MIPHY_DEFAULT_TIMER	2000
static int miphy_timer_msecs = MIPHY_DEFAULT_TIMER;
module_param(miphy_timer_msecs, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(miphy_timer_msecs, "miphy timer init in msecs");
#define MIPHY_TIMER(x)	(jiffies + msecs_to_jiffies(x))

/* MiPHY_2 RX status */
#define MIPHY2_RX_CAL_COMPLETED		BIT(0)
#define MIPHY2_RX_OFFSET		BIT(1)

#define MIPHY2_RX_CAL_STS		0xA0

/* MiPHY2 Status */
#define MiPHY2_STATUS_1			0x2
#define MIPHY2_PHY_READY		BIT(0)

/* MiPHY_2 Control */
#define	SYSCFG5071			0x11c
#define MIPHY2_PX_TX_POLARITY		BIT(0)
#define MIPHY2_PX_RX_POLARITY		BIT(1)
#define MIPHY2_PX_SYNC_DETECT_ENABLE	BIT(2)
#define MIPHY2_CTRL_MASK		0x7

/**
 * struct sti_usb3_cfg - SoC specific PHY register mapping
 * @syscfg: Offset in syscfg registers bank
 * @cfg_mask: Bits mask for PHY configuration
 * @cfg: Static configuration value for PHY
 */
struct sti_usb3_cfg {
	u32 syscfg;
	u32 cfg_mask;
	u32 cfg;
};

struct sti_usb3_miphy {
	struct usb_phy phy;
	struct device *dev;
	struct regmap *regmap;
	const struct sti_usb3_cfg *cfg;
	struct reset_control *rstc;
	void __iomem *usb3_base;
	void __iomem *pipe_base;
	struct timer_list miphy_timer;
	struct workqueue_struct *miphy_queue;
	struct work_struct miphy_work;
	spinlock_t lock;
};

static struct sti_usb3_cfg sti_usb3_miphy_cfg = {
	.syscfg = SYSCFG5071,
	.cfg_mask = MIPHY2_CTRL_MASK,
	.cfg = MIPHY2_PX_SYNC_DETECT_ENABLE,
};

struct miphy_initval {
	u16 reg;
	u16 val;
};

/* That is a magic sequence of register settings provided at
 * verification level to setup the MiPHY2 for USB3 DWC3 device on STiH407.
 */
static const struct miphy_initval initvals[] = {
	/* Putting Macro in reset */
	{0x00, 0x01}, {0x00, 0x03},
	/* Wait for a while */
	{0x00, 0x01}, {0x04, 0x1C},
	/* PLL calibration */
	{0xEB, 0x1D}, {0x0D, 0x1E}, {0x0F, 0x00}, {0xC4, 0x70},
	{0xC9, 0x22}, {0xCA, 0x22}, {0xCB, 0x22}, {0xCC, 0x2A},
	/* Writing The PLL Ratio */
	{0xD4, 0xA6}, {0xD5, 0xAA}, {0xD6, 0xAA}, {0xD7, 0x04},
	{0xD3, 0x00},
	/* Writing The Speed Rate */
	{0x0F, 0x00}, {0x0E, 0x0A},
	/* RX Channel compensation and calibration */
	{0xC2, 0x1C}, {0x97, 0x51}, {0x98, 0x70}, {0x99, 0x5F},
	{0x9A, 0x22}, {0x9F, 0x0E},

	{0x7A, 0x05}, {0x7B, 0x05}, {0x7F, 0x78}, {0x30, 0x1B},
	/* Enable GENSEL_SEL and SSC */
	/* TX_SEL=0 swing preemp forced by pipe registres */
	{0x0A, SSC_ON},
	/* MIPHY Bias boost */
	{0x63, 0x00}, {0x64, 0xA7},
	/* TX compensation offset to re-center TX impedance */
	{0x42, 0x02},
	/* SSC modulation */
	{0x0C, 0x04},
	/* Enable RX autocalibration */
	{0x2B, 0x01},
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

static int sti_usb3_miphy_autocalibration(struct usb_phy *phy_dev,
					enum usb_device_speed speed)
{
	struct sti_usb3_miphy *miphy = phy_to_priv(phy_dev);

	writeb_relaxed(0x40, miphy->usb3_base + 0x01);
	writeb_relaxed(0x00, miphy->usb3_base + 0x01);
	dev_dbg(miphy->dev, "miphy autocalibration done\n");

	/*
	 * Set a safe delay after calibration; we use the same range as in the
	 * phy-miphy28lp driver, where a delay is set after miphy reset.
	 * Here we are in the same case given that we are
	 * performing a reset to allow miphy to re-calibrate itself.
	 */
	usleep_range(10, 20);

	/* Check status */
	if ((readb_relaxed(miphy->usb3_base + 0x83)) ||
	    (readb_relaxed(miphy->usb3_base + 0x84)))
		dev_err(miphy->dev, "miphy autocalibration failed!\n");

	return 0;
}

static void sti_usb3_miphy_work(struct work_struct *work)
{
	struct sti_usb3_miphy *miphy =
	    container_of(work, struct sti_usb3_miphy, miphy_work);
	u8 status;
	u8 reg;

	spin_lock(&miphy->lock);

	status = readb_relaxed(miphy->usb3_base + MiPHY2_STATUS_1);
	if (status & MIPHY2_PHY_READY) {
		dev_dbg(miphy->dev, "MiPHY: phy is ready\n");
		/* 1ms delay required for RX calibration to complete */
		spin_unlock(&miphy->lock);
		mdelay(1);
		spin_lock(&miphy->lock);
		reg = readb_relaxed(miphy->usb3_base + MIPHY2_RX_CAL_STS);
		if (!(reg & MIPHY2_RX_CAL_COMPLETED)
		    && !(reg & MIPHY2_RX_OFFSET))
			dev_warn(miphy->dev,
				"fail RX calibration, unplug/plug the cable\n");
	}

	spin_unlock(&miphy->lock);
}

/*
 * TD2.3 embedded host training failure error message is a required
 * certification test for Ehost.
 * Create a timer that polls every 2 seconds MIPHY status, If miphy become ready
 * due to an RX detection then check MIPHY RX status and print error message in
 * case of RX calibration failure.
 */
static void sti_usb3_miphy_timer(unsigned long data)
{
	struct sti_usb3_miphy *miphy = (void *)data;

	schedule_work(&miphy->miphy_work);
	/* Just check for a valid and safe timer value */
	if (miphy_timer_msecs <= MIPHY_DEFAULT_TIMER)
		miphy_timer_msecs = MIPHY_DEFAULT_TIMER;
	miphy->miphy_timer.expires = MIPHY_TIMER(miphy_timer_msecs);
	mod_timer(&miphy->miphy_timer, miphy->miphy_timer.expires);
}

static void sti_usb3_miphy_timer_init(struct sti_usb3_miphy *phy_dev)
{
	init_timer(&phy_dev->miphy_timer);

	phy_dev->miphy_timer.data = (unsigned long) phy_dev;
	phy_dev->miphy_timer.function = sti_usb3_miphy_timer;
	phy_dev->miphy_timer.expires = MIPHY_TIMER(miphy_timer_msecs);

	add_timer(&phy_dev->miphy_timer);
	dev_info(phy_dev->dev, "USB3 MIPHY28LP timer initialized\n");

}

static void sti_usb3_miphy28lp(struct sti_usb3_miphy *phy_dev)
{
	int i;
	struct device_node *np = phy_dev->dev->of_node;

	dev_info(phy_dev->dev, "MiPHY28LP setup\n");

	for (i = 0; i < ARRAY_SIZE(initvals); i++) {
		dev_dbg(phy_dev->dev, "reg: 0x%x=0x%x\n", initvals[i].reg,
			initvals[i].val);
		writeb_relaxed(initvals[i].val,
			       phy_dev->usb3_base + initvals[i].reg);
	}

	/*
	 * There are spare hard disks that don't support Host SSC modulation
	 * so, although that must be ON for USB3 to avoid failures on Electrical
	 * tests and because USB3 could impact signal at 2.4GHZ (WIFI...), we
	 * let the user to choose to disable it via DT.
	 */
	if (of_property_read_bool(np, "st,no-ssc"))
		writeb_relaxed(SSC_OFF, phy_dev->usb3_base + 0x0A);

	/* PIPE Wrapper Configuration */
	writeb_relaxed(0X68, phy_dev->pipe_base + 0x23);
	writeb_relaxed(0X61, phy_dev->pipe_base + 0x24);
	writeb_relaxed(0X68, phy_dev->pipe_base + 0x26);
	writeb_relaxed(0X61, phy_dev->pipe_base + 0x27);
	writeb_relaxed(0X18, phy_dev->pipe_base + 0x29);
	writeb_relaxed(0X61, phy_dev->pipe_base + 0x2A);

	/*pipe Wrapper usb3 TX swing de-emph margin PREEMPH[7:4], SWING[3:0] */
	writeb_relaxed(0X67, phy_dev->pipe_base + 0x68);
	writeb_relaxed(0X0D, phy_dev->pipe_base + 0x69);
	writeb_relaxed(0X67, phy_dev->pipe_base + 0x6A);
	writeb_relaxed(0X0D, phy_dev->pipe_base + 0x6B);
	writeb_relaxed(0X67, phy_dev->pipe_base + 0x6C);
	writeb_relaxed(0X0D, phy_dev->pipe_base + 0x6D);
	writeb_relaxed(0X67, phy_dev->pipe_base + 0x6E);
	writeb_relaxed(0X0D, phy_dev->pipe_base + 0x6F);
}

static int sti_usb3_miphy_init(struct usb_phy *phy)
{
	struct sti_usb3_miphy *phy_dev = phy_to_priv(phy);
	int ret;

	ret = regmap_update_bits(phy_dev->regmap, phy_dev->cfg->syscfg,
				 phy_dev->cfg->cfg_mask, phy_dev->cfg->cfg);
	if (ret)
		return ret;

	reset_control_deassert(phy_dev->rstc);

	/* Program the MiPHY2 internal registers */
	sti_usb3_miphy28lp(phy_dev);
	/* Start polling timer to get MIPHY2 status*/
	sti_usb3_miphy_timer_init(phy_dev);

	return 0;
}

static void sti_usb3_miphy_shutdown(struct usb_phy *phy)
{
	struct sti_usb3_miphy *phy_dev = phy_to_priv(phy);

	reset_control_assert(phy_dev->rstc);
	del_timer(&phy_dev->miphy_timer);
}

static const struct of_device_id sti_usb3_miphy_of_match[];

static int sti_usb3_miphy_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	const struct of_device_id *match;
	struct sti_usb3_miphy *phy_dev;
	struct device *dev = &pdev->dev;
	struct usb_phy *phy;
	struct resource *res;

	phy_dev = devm_kzalloc(dev, sizeof(*phy_dev), GFP_KERNEL);
	if (!phy_dev)
		return -ENOMEM;

	match = of_match_device(sti_usb3_miphy_of_match, &pdev->dev);
	if (!match)
		return -ENODEV;

	phy_dev->cfg = match->data;
	phy_dev->dev = dev;

	phy_dev->rstc = devm_reset_control_get(dev, NULL);
	if (IS_ERR(phy_dev->rstc)) {
		dev_err(dev, "failed to ctrl MiPHY2 USB3 reset\n");
		return PTR_ERR(phy_dev->rstc);
	}

	dev_info(dev, "reset MiPHY\n");
	reset_control_deassert(phy_dev->rstc);

	phy_dev->regmap = syscon_regmap_lookup_by_phandle(np, "st,syscfg");
	if (IS_ERR(phy_dev->regmap)) {
		dev_err(dev, "No syscfg phandle specified\n");
		return PTR_ERR(phy_dev->regmap);
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "usb3-uport");
	if (res) {
		phy_dev->usb3_base = devm_request_and_ioremap(&pdev->dev, res);
		if (!phy_dev->usb3_base) {
			dev_err(&pdev->dev, "Unable to map base registers\n");
			return -ENOMEM;
		}
	}
	/* Check for PIPE registers */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "pipew");
	if (res) {
		phy_dev->pipe_base = devm_request_and_ioremap(&pdev->dev, res);
		if (!phy_dev->pipe_base) {
			dev_err(&pdev->dev, "Unable to map PIPE registers\n");
			return -ENOMEM;
		}
	}

	dev_info(dev, "usb3 ioaddr 0x%p, pipew ioaddr 0x%p\n",
		 phy_dev->usb3_base, phy_dev->pipe_base);

	spin_lock_init(&phy_dev->lock);

	phy_dev->miphy_queue =
	    create_singlethread_workqueue("usb3_miphy_queue");
	if (!phy_dev->miphy_queue) {
		dev_err(phy_dev->dev, "couldn't create workqueue\n");
		return -ENOMEM;
	}

	INIT_WORK(&phy_dev->miphy_work, sti_usb3_miphy_work);

	phy = &phy_dev->phy;
	phy->dev = dev;
	phy->label = "USB3 MiPHY2 (LP28)";
	phy->init = sti_usb3_miphy_init;
	phy->type = USB_PHY_TYPE_USB3;
	phy->shutdown = sti_usb3_miphy_shutdown;
	if (of_property_read_bool(np, "st,auto-calibration"))
		phy->notify_disconnect = sti_usb3_miphy_autocalibration;

	usb_add_phy_dev(phy);

	platform_set_drvdata(pdev, phy_dev);

	dev_info(dev, "USB3 MiPHY2 probed\n");

	return 0;
}

static int sti_usb3_miphy_remove(struct platform_device *pdev)
{
	struct sti_usb3_miphy *phy_dev = platform_get_drvdata(pdev);

	reset_control_assert(phy_dev->rstc);

	usb_remove_phy(&phy_dev->phy);
	del_timer(&phy_dev->miphy_timer);

	if (phy_dev->miphy_queue)
		destroy_workqueue(phy_dev->miphy_queue);

	return 0;
}

static const struct of_device_id sti_usb3_miphy_of_match[] = {
	{
	 .compatible = "st,sti-usb3phy",
	 .data = &sti_usb3_miphy_cfg},
	{},
};

MODULE_DEVICE_TABLE(of, sti_usb3_miphy_of_match);

static struct platform_driver sti_usb3_miphy_driver = {
	.probe = sti_usb3_miphy_probe,
	.remove = sti_usb3_miphy_remove,
	.driver = {
		   .name = "sti-usb3-phy",
		   .owner = THIS_MODULE,
		   .of_match_table = sti_usb3_miphy_of_match,
		   }
};

module_platform_driver(sti_usb3_miphy_driver);

#ifndef MODULE
static int __init miphy_cmdline_opt(char *str)
{
	char *opt;

	if (!str || !*str)
		return -EINVAL;
	while ((opt = strsep(&str, ",")) != NULL) {
		if (!strncmp(opt, "miphy_timer_msecs:", 18)) {
			if (kstrtoint(opt + 18, 0, &miphy_timer_msecs))
				goto err;
		}
	}
	return 0;

err:
	pr_err("%s: ERROR broken module parameter\n", __func__);
	return -EINVAL;
}

__setup("miphy_st=", miphy_cmdline_opt);
#endif /* MODULE */


MODULE_AUTHOR("Giuseppe Cavallaro <peppe.cavallaro@st.com>");
MODULE_DESCRIPTION("STMicroelectronics USB3 MiPHY for STiH407/STi8416 SoC");
MODULE_LICENSE("GPL v2");
