/*
 * st-lpc-wdt.c
 *
 * Copyright (C) STMicroelectronics SA 2013
 * Author:  David Paris <david.paris@st.com> for STMicroelectronics.
 * License terms:  GNU General Public License (GPL), version 2
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/watchdog.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>

/* Low Power Alarm */
#define LPC_LPA_LSB_OFF		0x410
#define LPC_LPA_START_OFF	0x418
/* LPC as WDT */
#define LPC_WDT_OFF		0x510

#define WATCHDOG_MIN		1
#define WATCHDOG_MAX32		4294967 /* 32bit resolution */

#define WDT_ENABLE		1
#define WDT_DISABLE		0

#define STID127_SYSCFG_CPU(num)		((num - 700)*4)
#define STIH415_SYSCFG_SYSTEM(num)	((num - 600)*4)
#define STIH416_SYSCFG_CPU(num)		(2000 + ((num - 7500)*4))
#define STIH407_SYSCFG_CORE(num)	((num - 5000)*4)

struct cpurst_syscfg {
	struct regmap *syscfg;
	unsigned int type;
	unsigned int type_msk;
	unsigned int mask;
	unsigned int mask_msk;
};

struct st_lpc_wdt {
	void __iomem *ioaddr;
	struct cpurst_syscfg *cpurst;
	struct clk *clk;
	int rst_type;
} lpc_wdt;

static struct cpurst_syscfg stid127_cpurst = {
	.type = STID127_SYSCFG_CPU(701),
	.type_msk = BIT(2),
	.mask = STID127_SYSCFG_CPU(700),
	.mask_msk = BIT(2),
};

static struct cpurst_syscfg stih415_cpurst = {
	.type = STIH415_SYSCFG_SYSTEM(646),
	.type_msk = BIT(6),
	.mask = STIH415_SYSCFG_SYSTEM(645),
	.mask_msk = BIT(7),
};

static struct cpurst_syscfg stih416_cpurst = {
	.type = STIH416_SYSCFG_CPU(7547),
	.type_msk = BIT(6),
	.mask = STIH416_SYSCFG_CPU(7546),
	.mask_msk = BIT(7),
};

static struct cpurst_syscfg stih407_cpurst = {
	.mask = STIH407_SYSCFG_CORE(5129),
	.mask_msk = BIT(19),
};

static struct of_device_id st_lpc_wdt_match[] = {
	{
		.compatible = "st,stih407-lpc-wdt",
		.data = (void *)&stih407_cpurst,
	},
	{
		.compatible = "st,stih416-lpc-wdt",
		.data = (void *)&stih416_cpurst,
	},
	{
		.compatible = "st,stih415-lpc-wdt",
		.data = (void *)&stih415_cpurst,
	},
	{
		.compatible = "st,stid127-lpc-wdt",
		.data = (void *)&stid127_cpurst,
	},
	{},
};

MODULE_DEVICE_TABLE(of, st_lpc_wdt_match);

/*
 * st_lpc_wdt_setup:
 * Configure how LPC watchdog interrupt acts on platform
 *	enable: 0: No platform reboot when watchdog occurs
 *		1: Platform will reboot when watchdog occurs
 */
static void st_lpc_wdt_setup(bool enable)
{
	/*
	 * Here we choose type of watchdog reset
	 * 0: Cold 1: Warm
	 */
	if (lpc_wdt.cpurst->type) {
		regmap_update_bits(lpc_wdt.cpurst->syscfg,
				lpc_wdt.cpurst->type,
				lpc_wdt.cpurst->type_msk,
				lpc_wdt.rst_type);
	}

	/*
	 * Here we mask/unmask watchdog reset
	 */
	regmap_update_bits(lpc_wdt.cpurst->syscfg,
			lpc_wdt.cpurst->mask,
			lpc_wdt.cpurst->mask_msk,
			!enable);
}

/*
 * st_lpc_wdt_load_timer:
 * API to load the watchdog timeout value in the LPA down counter
 *	timeout: watchdog timeout in seconds
 */
static void st_lpc_wdt_load_timer(unsigned int timeout)
{
	timeout *= clk_get_rate(lpc_wdt.clk);

	/*
	 * Write only LSB as timeout in Watchdog
	 * framework is 32bits only
	 */
	writel_relaxed(timeout, lpc_wdt.ioaddr + LPC_LPA_LSB_OFF);
	writel_relaxed(1, lpc_wdt.ioaddr + LPC_LPA_START_OFF);
}

/*
 * st_lpc_wdt_start:
 * API to start to LPA downcounting
 *	wdd: the watchdog to start
 */
static int st_lpc_wdt_start(struct watchdog_device *wdd)
{
	writel_relaxed(1, lpc_wdt.ioaddr + LPC_WDT_OFF);

	return 0;
}

/*
 * st_lpc_wdt_stop:
 * API to stop to LPA downcounting
 *	wdd: the watchdog to stop
 */
static int st_lpc_wdt_stop(struct watchdog_device *wdd)
{
	writel_relaxed(0, lpc_wdt.ioaddr + LPC_WDT_OFF);

	return 0;
}

/*
 * st_lpc_wdt_set_timeout:
 * API calls by watchdog framework to load the watchdog
 * timeout value.
 *	wdd: the watchdog device to configure
 *	timeout: the watchdog timeout in seconds
 */
static int st_lpc_wdt_set_timeout(struct watchdog_device *wdd,
				 unsigned int timeout)
{
	wdd->timeout = timeout;
	st_lpc_wdt_load_timer(timeout);

	return 0;
}

/*
 * st_lpc_wdt_keep_alive:
 * API to kick the watchdog
 *	wdd: the watchdog device to kick
 */
static int st_lpc_wdt_keepalive(struct watchdog_device *wdd)
{
	st_lpc_wdt_load_timer(wdd->timeout);

	return 0;
}

static const struct watchdog_info lpc_wdt_info = {
	.options = WDIOF_SETTIMEOUT | WDIOF_KEEPALIVEPING | WDIOF_MAGICCLOSE,
	.identity = "ST LPC WDT",
};

static const struct watchdog_ops lpc_wdt_ops = {
	.owner = THIS_MODULE,
	.start = st_lpc_wdt_start,
	.stop  = st_lpc_wdt_stop,
	.ping  = st_lpc_wdt_keepalive,
	.set_timeout = st_lpc_wdt_set_timeout,
};

static struct watchdog_device lpc_wdt_dev = {
	.info = &lpc_wdt_info,
	.ops = &lpc_wdt_ops,
	.min_timeout = WATCHDOG_MIN,
	.max_timeout = WATCHDOG_MAX32,
};

#ifdef CONFIG_PM
static int st_lpc_wdt_suspend(struct device *dev)
{
	/*
	 * Stop watchdog during suspend
	 */
	if (watchdog_active(&lpc_wdt_dev))
		st_lpc_wdt_stop(&lpc_wdt_dev);

	return 0;
}

static int st_lpc_wdt_resume(struct device *dev)
{
	/*
	 * Re-start watchdog if it was active before suspend
	 */
	if (watchdog_active(&lpc_wdt_dev)) {
		st_lpc_wdt_load_timer(lpc_wdt_dev.timeout);
		st_lpc_wdt_start(&lpc_wdt_dev);
	}

	return 0;
}

SIMPLE_DEV_PM_OPS(st_lpc_wdt_pm_ops, st_lpc_wdt_suspend, st_lpc_wdt_resume);
#define ST_LPC_WDT_PM	(&st_lpc_wdt_pm_ops)
#else
#define ST_LPC_WDT_PM	NULL
#endif

static int st_lpc_wdt_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct device *dev = &pdev->dev;
	const struct of_device_id *match;
	struct resource *res;
	int ret = 0;
	const char *rst_type;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	lpc_wdt.ioaddr = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(lpc_wdt.ioaddr)) {
		pr_err("%s Ioremap resource not done\n", __func__);
		return PTR_ERR(lpc_wdt.ioaddr);
	}

	match = of_match_device(st_lpc_wdt_match, dev);

	lpc_wdt.cpurst = (struct cpurst_syscfg *)match->data;
	lpc_wdt.cpurst->syscfg =
		syscon_regmap_lookup_by_phandle(np, "st,syscfg");
	if (IS_ERR(lpc_wdt.cpurst->syscfg)) {
		dev_err(dev, "No syscfg phandle specified\n");
		return PTR_ERR(lpc_wdt.cpurst->syscfg);
	}

	lpc_wdt.clk = of_clk_get_by_name(np, "lpc_wdt");
	if (IS_ERR(lpc_wdt.clk)) {
		dev_err(dev, "Unable to request clock\n");
		return PTR_ERR(lpc_wdt.clk);
	}

	clk_prepare_enable(lpc_wdt.clk);

	ret = of_property_read_string(dev->of_node, "st,rst_type", &rst_type);
	if (ret >= 0) {
		if (!strcmp(rst_type, "warm"))
			lpc_wdt.rst_type = 1;
		else
			lpc_wdt.rst_type = 0;
	}

	platform_set_drvdata(pdev, &lpc_wdt);

	watchdog_set_nowayout(&lpc_wdt_dev, WATCHDOG_NOWAYOUT);

	ret = watchdog_register_device(&lpc_wdt_dev);
	if (ret) {
		dev_err(dev, "Unable to register device\n");
		clk_disable_unprepare(lpc_wdt.clk);
		return ret;
	}

	/* Init Watchdog timeout with value in DT */
	watchdog_init_timeout(&lpc_wdt_dev, 0, dev);

	st_lpc_wdt_setup(WDT_ENABLE);

	dev_info(dev, "LPC Watchdog driver registered, reset type is %s",
			lpc_wdt.rst_type ? "warm" : "cold");

	return ret;
}

static int st_lpc_wdt_remove(struct platform_device *pdev)
{
	if (watchdog_active(&lpc_wdt_dev))
		st_lpc_wdt_stop(&lpc_wdt_dev);

	st_lpc_wdt_setup(WDT_DISABLE);

	watchdog_unregister_device(&lpc_wdt_dev);

	return 0;
}

static struct platform_driver st_lpc_wdt_driver = {
	.probe		= st_lpc_wdt_probe,
	.remove		= st_lpc_wdt_remove,
	.driver		= {
		.name	= "st-lpc-wdt",
		.owner	= THIS_MODULE,
		.pm	= ST_LPC_WDT_PM,
		.of_match_table = of_match_ptr(st_lpc_wdt_match),
	},
};

module_platform_driver(st_lpc_wdt_driver);

MODULE_AUTHOR("David PARIS <david.paris@st.com>");
MODULE_DESCRIPTION("ST LPC Watchdog Driver");
MODULE_LICENSE("GPLv2");

