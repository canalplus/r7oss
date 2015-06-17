/*
 * This driver implements a RTC using the Low Power Timer in
 * the Low Power Controller (LPC) in some STMicroelectronics devices.
 *
 * See ADCS 2001950 for more details on the hardware.
 *
 * Copyright (C) 2009 STMicroelectronics Limited
 * Copyright (C) 2010 STMicroelectronics Limited
 * Copyright (C) 2013 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@@st.com>
 * Author: Francesco Virlinzi <francesco.virlinzi@st.com>
 * Author: David Paris <david.paris@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License GPLv2.  See linux/COPYING for more information.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/rtc.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/of_irq.h>

/* Low Power Timer */
#define LPC_LPT_LSB_OFF		0x400
#define LPC_LPT_MSB_OFF		0x404
#define LPC_LPT_START_OFF	0x408
/* Low Power Alarm */
#define LPC_LPA_LSB_OFF		0x410
#define LPC_LPA_MSB_OFF		0x414
#define LPC_LPA_START_OFF	0x418
/* LPC as WDT */
#define LPC_WDT_OFF		0x510
#define LPC_WDT_FLAG_OFF	0x514

struct st_rtc {
	struct rtc_device *rtc_dev;
	void __iomem *ioaddr;
	struct resource *res;
	struct clk *clk;
	short irq;
	bool irq_enabled:1;
	spinlock_t lock;
	struct rtc_wkalrm alarm;
};

static void st_rtc_set_hw_alarm(struct st_rtc *rtc,
		unsigned long msb, unsigned long  lsb)
{
	unsigned long flags;

	spin_lock_irqsave(&rtc->lock, flags);

	writel(1, rtc->ioaddr + LPC_WDT_OFF);

	writel(msb, rtc->ioaddr + LPC_LPA_MSB_OFF);
	writel(lsb, rtc->ioaddr + LPC_LPA_LSB_OFF);
	writel(1, rtc->ioaddr + LPC_LPA_START_OFF);

	writel(0, rtc->ioaddr + LPC_WDT_OFF);

	spin_unlock_irqrestore(&rtc->lock, flags);
}

static irqreturn_t st_rtc_isr(int this_irq, void *data)
{
	struct st_rtc *rtc = (struct st_rtc *)data;

	rtc_update_irq(rtc->rtc_dev, 1, RTC_AF);

	return IRQ_HANDLED;
}

static int st_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	struct st_rtc *rtc = dev_get_drvdata(dev);
	unsigned long long lpt;
	unsigned long lpt_lsb, lpt_msb;
	unsigned long flags;

	spin_lock_irqsave(&rtc->lock, flags);

	do {
		lpt_msb = readl(rtc->ioaddr + LPC_LPT_MSB_OFF);
		lpt_lsb = readl(rtc->ioaddr + LPC_LPT_LSB_OFF);
	} while (readl(rtc->ioaddr + LPC_LPT_MSB_OFF) != lpt_msb);

	spin_unlock_irqrestore(&rtc->lock, flags);

	lpt = ((unsigned long long)lpt_msb << 32) | lpt_lsb;
	do_div(lpt, clk_get_rate(rtc->clk));
	rtc_time_to_tm(lpt, tm);

	return 0;
}

static int st_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	struct st_rtc *rtc = dev_get_drvdata(dev);
	unsigned long secs, flags;
	int ret;
	unsigned long long lpt;

	ret = rtc_tm_to_time(tm, &secs);
	if (ret != 0)
		return ret;

	lpt = (unsigned long long)secs * clk_get_rate(rtc->clk);

	spin_lock_irqsave(&rtc->lock, flags);

	writel(lpt >> 32, rtc->ioaddr + LPC_LPT_MSB_OFF);
	writel(lpt, rtc->ioaddr + LPC_LPT_LSB_OFF);
	writel(1, rtc->ioaddr + LPC_LPT_START_OFF);

	spin_unlock_irqrestore(&rtc->lock, flags);

	return 0;
}

static int st_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *wkalrm)
{
	struct st_rtc *rtc = dev_get_drvdata(dev);
	unsigned long flags;

	spin_lock_irqsave(&rtc->lock, flags);

	memcpy(wkalrm, &rtc->alarm, sizeof(struct rtc_wkalrm));

	spin_unlock_irqrestore(&rtc->lock, flags);

	return 0;
}

static int st_rtc_alarm_irq_enable(struct device *dev, unsigned int enabled)
{
	struct st_rtc *rtc = dev_get_drvdata(dev);

	if (enabled && !rtc->irq_enabled) {
		enable_irq(rtc->irq);
		rtc->irq_enabled = 1;
	} else if (!enabled && rtc->irq_enabled) {
		disable_irq(rtc->irq);
		rtc->irq_enabled = 0;
	}

	return 0;
}

static int st_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *t)
{
	struct st_rtc *rtc = dev_get_drvdata(dev);
	struct rtc_time now;
	unsigned long now_secs;
	unsigned long alarm_secs;
	unsigned long long lpa;

	st_rtc_read_time(dev, &now);
	rtc_tm_to_time(&now, &now_secs);
	rtc_tm_to_time(&t->time, &alarm_secs);

	if (now_secs > alarm_secs)
		return -EINVAL; /* invalid alarm time */

	memcpy(&rtc->alarm, t, sizeof(struct rtc_wkalrm));

	alarm_secs -= now_secs; /* now many secs to fire */
	lpa = (unsigned long long)alarm_secs * clk_get_rate(rtc->clk);

	st_rtc_set_hw_alarm(rtc, lpa >> 32, lpa);

	st_rtc_alarm_irq_enable(dev, t->enabled);

	return 0;
}

static struct rtc_class_ops st_rtc_ops = {
	.read_time = st_rtc_read_time,
	.set_time = st_rtc_set_time,
	.read_alarm = st_rtc_read_alarm,
	.set_alarm = st_rtc_set_alarm,
	.alarm_irq_enable = st_rtc_alarm_irq_enable,
};

#ifdef CONFIG_PM
static int st_rtc_suspend(struct device *dev)
{
	struct st_rtc *rtc = dev_get_drvdata(dev);

	if (device_may_wakeup(dev))
		return 0;

	writel(1, rtc->ioaddr + LPC_WDT_OFF);
	writel(0, rtc->ioaddr + LPC_LPA_START_OFF);
	writel(0, rtc->ioaddr + LPC_WDT_OFF);

	return 0;
}

static int st_rtc_resume(struct device *dev)
{
	struct st_rtc *rtc = dev_get_drvdata(dev);

	rtc_alarm_irq_enable(rtc->rtc_dev, 0);

	/*
	 * clean 'rtc->alarm' to allow a new
	 * a new .set_alarm to the upper RTC layer
	 */
	memset(&rtc->alarm, 0, sizeof(struct rtc_wkalrm));

	writel(0, rtc->ioaddr + LPC_LPA_MSB_OFF);
	writel(0, rtc->ioaddr + LPC_LPA_LSB_OFF);
	writel(1, rtc->ioaddr + LPC_WDT_OFF);
	writel(1, rtc->ioaddr + LPC_LPA_START_OFF);
	writel(0, rtc->ioaddr + LPC_WDT_OFF);

	return 0;
}

SIMPLE_DEV_PM_OPS(st_rtc_pm_ops, st_rtc_suspend, st_rtc_resume);
#define ST_LPC_RTC_PM	(&st_rtc_pm_ops)
#else
#define ST_LPC_RTC_PM	NULL
#endif

static int st_rtc_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct st_rtc *rtc;
	struct resource *res;
	int ret = 0;
	struct rtc_time tm_check;

	rtc = devm_kzalloc(&pdev->dev, sizeof(struct st_rtc), GFP_KERNEL);
	if (unlikely(!rtc))
		return -ENOMEM;

	spin_lock_init(&rtc->lock);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	rtc->ioaddr = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(rtc->ioaddr)) {
		pr_err("%s Ioremap resource not done\n", __func__);
		return PTR_ERR(rtc->ioaddr);
	}

	rtc->irq = irq_of_parse_and_map(np, 0);
	if (!rtc->irq) {
		dev_err(&pdev->dev, "IRQ missing or invalid\n");
		return -EINVAL;
	}

	ret = devm_request_irq(&pdev->dev, rtc->irq, st_rtc_isr, 0,
			pdev->name, rtc);
	if (ret) {
		dev_err(&pdev->dev, "Failed to request irq %i\n", rtc->irq);
		return ret;
	}

	enable_irq_wake(rtc->irq);
	disable_irq(rtc->irq);

	rtc->clk = of_clk_get_by_name(np, "lpc_rtc");
	if (IS_ERR(rtc->clk)) {
		dev_err(&pdev->dev, "Unable to request clock\n");
		return PTR_ERR(rtc->clk);
	}

	clk_prepare_enable(rtc->clk);

	device_set_wakeup_capable(&pdev->dev, 1);

	platform_set_drvdata(pdev, rtc);

	/*
	 * The RTC-LPC is able to manage date.year > 2038
	 * but currently the kernel can not manage this date!
	 * If the RTC-LPC has a date.year > 2038 then
	 * it's set to the epoch "Jan 1st 2000"
	 */
	st_rtc_read_time(&pdev->dev, &tm_check);

	if (tm_check.tm_year >=  (2038 - 1900)) {
		memset(&tm_check, 0, sizeof(tm_check));
		tm_check.tm_year = 100;
		tm_check.tm_mday = 1;
		st_rtc_set_time(&pdev->dev, &tm_check);
	}

	rtc->rtc_dev = rtc_device_register("st-lpc-rtc", &pdev->dev,
					   &st_rtc_ops, THIS_MODULE);
	if (IS_ERR(rtc->rtc_dev)) {
		clk_disable_unprepare(rtc->clk);
		ret = PTR_ERR(rtc->rtc_dev);
	}

	return ret;
}

static int st_rtc_remove(struct platform_device *pdev)
{
	struct st_rtc *rtc = platform_get_drvdata(pdev);

	if (likely(rtc->rtc_dev))
		rtc_device_unregister(rtc->rtc_dev);

	return 0;
}

static struct of_device_id st_rtc_match[] = {
	{
		.compatible = "st,lpc-rtc",
	},
	{},
};

MODULE_DEVICE_TABLE(of, st_rtc_match);

static struct platform_driver st_rtc_platform_driver = {
	.driver = {
		.name = "st-lpc-rtc",
		.owner = THIS_MODULE,
		.pm = ST_LPC_RTC_PM,
		.of_match_table = of_match_ptr(st_rtc_match),
	},
	.probe = st_rtc_probe,
	.remove = st_rtc_remove,
};

module_platform_driver(st_rtc_platform_driver);

MODULE_DESCRIPTION("STMicroelectronics LPC RTC driver");
MODULE_AUTHOR("Stuart Menefy <stuart.menefy@st.com>");
MODULE_LICENSE("GPLv2");
