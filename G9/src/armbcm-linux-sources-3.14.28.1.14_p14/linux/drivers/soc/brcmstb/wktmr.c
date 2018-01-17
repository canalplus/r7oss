/*
 * Copyright © 2014 Broadcom Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt)	KBUILD_MODNAME ": " fmt

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/stat.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/suspend.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/pm.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/irqreturn.h>
#include <linux/pm_wakeup.h>
#include <linux/reboot.h>
#include <linux/rtc.h>

#include <asm/mach/time.h>

#define DRV_NAME	"brcm-waketimer"

static struct brcmstb_waketmr {
	struct rtc_device *rtc;
	struct device *dev;
	void __iomem *base;
	unsigned int irq;
	int wake_timeout;
	struct notifier_block reboot_notifier;
	bool alarm_is_active;
} wktimer;

/* No timeout */
#define BRCMSTB_WKTMR_DEFAULT_TIMEOUT	(-1)

#define BRCMSTB_WKTMR_EVENT		0x00
#define BRCMSTB_WKTMR_COUNTER		0x04
#define BRCMSTB_WKTMR_ALARM		0x08
#define BRCMSTB_WKTMR_PRESCALER		0x0C
#define BRCMSTB_WKTMR_PRESCALER_VAL	0x10

static inline void brcmstb_waketmr_clear_alarm(struct brcmstb_waketmr *timer)
{
	writel_relaxed(1, timer->base + BRCMSTB_WKTMR_EVENT);
	(void)readl_relaxed(timer->base + BRCMSTB_WKTMR_EVENT);
	timer->alarm_is_active = false;
}

static void brcmstb_waketmr_set_alarm(struct brcmstb_waketmr *timer,
		unsigned int secs, bool sysfs)
{
	unsigned int t = 0;

	brcmstb_waketmr_clear_alarm(timer);

	if (sysfs)
		t = readl_relaxed(timer->base + BRCMSTB_WKTMR_COUNTER);
	writel_relaxed(t + secs + 1, timer->base + BRCMSTB_WKTMR_ALARM);
	timer->alarm_is_active = true;
}

static irqreturn_t brcmstb_waketmr_irq(int irq, void *data)
{
	struct brcmstb_waketmr *timer = data;
	pm_wakeup_event(timer->dev, 0);
	timer->alarm_is_active = false;
	return IRQ_HANDLED;
}

/* Fixed 27Mhz frequency since WKTMR is in the UPG clock domain. This
 * information should come from Device Tree eventually
 */
#define WKTMR_FREQ		27000000

struct wktmr_time {
	u32			sec;
	u32			pre;
};

static void wktmr_read(struct wktmr_time *t)
{
	u32 tmp;

	do {
		t->sec = readl_relaxed(wktimer.base + BRCMSTB_WKTMR_COUNTER);
		tmp = readl_relaxed(wktimer.base + BRCMSTB_WKTMR_PRESCALER_VAL);
	} while (tmp >= WKTMR_FREQ);

	t->pre = WKTMR_FREQ - tmp;
}

static void brcmstb_waketmr_read_persistent_clock(struct timespec *ts)
{
	struct wktmr_time now;

	wktmr_read(&now);

	ts->tv_sec = now.sec;
	ts->tv_nsec = now.pre * (1000000000 / WKTMR_FREQ);
}

static ssize_t brcmstb_waketmr_timeout_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct brcmstb_waketmr *timer = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", timer->wake_timeout);
}

static ssize_t brcmstb_waketmr_timeout_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct brcmstb_waketmr *timer = dev_get_drvdata(dev);
	int timeout;
	int ret;

#ifdef CONFIG_RTC_CLASS
	dev_warn(dev, "Using sysfs attributes, consider using 'rtcwake'\n");
#endif
	ret = kstrtoint(buf, 0, &timeout);
	if (ret < 0)
		return ret;

	/* Allow -1 as "no timeout" */
	if (timeout < -1)
		return -EINVAL;

	timer->wake_timeout = timeout;

	return count;
}

static const DEVICE_ATTR(timeout, S_IRUGO | S_IWUSR,
		brcmstb_waketmr_timeout_show,
		brcmstb_waketmr_timeout_store);

static int brcmstb_waketmr_prepare_suspend(struct brcmstb_waketmr *timer)
{
	struct device *dev = timer->dev;
	int ret;

	if (device_may_wakeup(dev) && timer->wake_timeout >= 0) {
		dev_dbg(dev, "enable wake IRQ\n");
		ret = enable_irq_wake(timer->irq);
		if (ret) {
			dev_err(dev, "failed to enable wake-up interrupt\n");
			return ret;
		}

		/*
		 * Using the ioctl() interface, the alarm timer will have
		 * already been enabled in brcmstb_waketmr_setalarm().
		 * However, using the sysfs interface, we have to enable it
		 * here, because brcmstb_waketmr_setalarm() isn't called.
		 */
		if (!timer->alarm_is_active)
			brcmstb_waketmr_set_alarm(timer, timer->wake_timeout,
						  true);
	} else {
		dev_dbg(dev, "nothing to do: wake_timeout: %d\n",
				timer->wake_timeout);
	}
	return 0;
}

/* If enabled as a wakeup-source, arm the timer when powering off */
static int brcmstb_waketmr_reboot(struct notifier_block *nb,
		unsigned long action, void *data)
{
	struct brcmstb_waketmr *timer;
	timer = container_of(nb, struct brcmstb_waketmr, reboot_notifier);

	/* Set timer for cold boot */
	if (action == SYS_POWER_OFF)
		brcmstb_waketmr_prepare_suspend(timer);

	return NOTIFY_DONE;
}

#ifdef CONFIG_RTC_CLASS
static int brcmstb_waketmr_gettime(struct device *dev,
				   struct rtc_time *tm)
{
	struct wktmr_time now;

	wktmr_read(&now);

	rtc_time_to_tm(now.sec, tm);

	return 0;
}

static int brcmstb_waketmr_settime(struct device *dev,
				   struct rtc_time *tm)
{
	struct brcmstb_waketmr *timer = dev_get_drvdata(dev);
	unsigned long sec;
	int ret;

	ret = rtc_valid_tm(tm);
	if (ret)
		return ret;

	rtc_tm_to_time(tm, &sec);

	dev_dbg(dev, "%s: sec=%ld\n", __FUNCTION__, sec);
	writel_relaxed(sec, timer->base + BRCMSTB_WKTMR_COUNTER);

	return 0;
}

static int brcmstb_waketmr_getalarm(struct device *dev,
				    struct rtc_wkalrm *alarm)
{
	struct brcmstb_waketmr *timer = dev_get_drvdata(dev);
	unsigned long sec;
	u32 reg;

	sec = readl_relaxed(timer->base + BRCMSTB_WKTMR_ALARM);
	if (sec == 0) {
		/* Alarm is disabled */
		alarm->enabled = 0;
		alarm->time.tm_mon = -1;
		alarm->time.tm_mday = -1;
		alarm->time.tm_year = -1;
		alarm->time.tm_hour = -1;
		alarm->time.tm_min = -1;
		alarm->time.tm_sec = -1;
		dev_dbg(dev, "%s: alarm is disabled\n", __FUNCTION__);
	} else {
		/* Alarm is enabled */
		alarm->enabled = 1;
		rtc_time_to_tm(sec, &alarm->time);
		dev_dbg(dev, "%s: alarm is enabled\n", __FUNCTION__);
	}

	reg = readl_relaxed(timer->base + BRCMSTB_WKTMR_EVENT);
	alarm->pending = !!(reg & 1);
	dev_dbg(dev, "%s: alarm pending=%d\n", __FUNCTION__, alarm->pending);

	return 0;
}

static int brcmstb_waketmr_setalarm(struct device *dev,
				     struct rtc_wkalrm *alarm)
{
	struct brcmstb_waketmr *timer = dev_get_drvdata(dev);
	unsigned long sec;

	if (alarm->enabled)
		rtc_tm_to_time(&alarm->time, &sec);
	else
		sec = 0;

	timer->wake_timeout = sec;
	dev_dbg(dev, "%s: timeout=%ld\n", __FUNCTION__, sec);
	brcmstb_waketmr_set_alarm(timer, sec, false);

	return 0;
}

/*
 * Does not do much but keep the RTC class happy. We always support
 * alarms.
 */
static int brcmstb_waketmr_alarm_enable(struct device *dev,
					unsigned int enabled)
{
	dev_dbg(dev, "%s: enabled=%d\n", __FUNCTION__, enabled);
	return 0;
}

static const struct rtc_class_ops brcmstb_waketmr_ops = {
	.read_time	= brcmstb_waketmr_gettime,
	.set_time	= brcmstb_waketmr_settime,
	.read_alarm	= brcmstb_waketmr_getalarm,
	.set_alarm	= brcmstb_waketmr_setalarm,
	.alarm_irq_enable = brcmstb_waketmr_alarm_enable,
};
#endif /* CONFIG_RTC_CLASS */

static int __init brcmstb_waketmr_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct brcmstb_waketmr *timer = &wktimer;
	struct resource *res;
	int ret;

	platform_set_drvdata(pdev, timer);

	if (timer->dev)
		return -EBUSY;

	timer->dev = dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	timer->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(timer->base))
		return PTR_ERR(timer->base);

	/*
	 * Set wakeup capability before requesting wakeup interrupt, so we can
	 * process boot-time "wakeups" (e.g., from S5 soft-off)
	 */
	device_set_wakeup_capable(dev, true);
	device_wakeup_enable(dev);

	timer->irq = platform_get_irq(pdev, 0);
	if ((int)timer->irq < 0)
		return -ENODEV;

	ret = devm_request_irq(dev, timer->irq, brcmstb_waketmr_irq, 0,
			                DRV_NAME, timer);
	if (ret < 0)
		return ret;

	timer->reboot_notifier.notifier_call = brcmstb_waketmr_reboot;
	register_reboot_notifier(&timer->reboot_notifier);

	timer->wake_timeout = BRCMSTB_WKTMR_DEFAULT_TIMEOUT;
#ifdef CONFIG_RTC_CLASS
	timer->rtc = rtc_device_register("brcmstb-waketmr", dev,
					 &brcmstb_waketmr_ops, THIS_MODULE);
	if (IS_ERR(timer->rtc)) {
		dev_err(dev, "unable to register device\n");
		return PTR_ERR(timer->rtc);
	}
#endif

	ret = device_create_file(dev, &dev_attr_timeout);
	if (ret) {
		unregister_reboot_notifier(&timer->reboot_notifier);
		return ret;
	}

	register_persistent_clock(NULL, brcmstb_waketmr_read_persistent_clock);

	dev_info(dev, "registered, with irq %d\n", timer->irq);
	return ret;
}

static int brcmstb_waketmr_remove(struct platform_device *pdev)
{
	struct brcmstb_waketmr *timer = dev_get_drvdata(&pdev->dev);

	device_remove_file(&pdev->dev, &dev_attr_timeout);
	unregister_reboot_notifier(&timer->reboot_notifier);
#ifdef CONFIG_RTC_CLASS
	rtc_device_unregister(timer->rtc);
#endif

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int brcmstb_waketmr_suspend(struct device *dev)
{
	struct brcmstb_waketmr *timer = dev_get_drvdata(dev);

	return brcmstb_waketmr_prepare_suspend(timer);
}

static int brcmstb_waketmr_resume(struct device *dev)
{
	struct brcmstb_waketmr *timer = dev_get_drvdata(dev);
	int ret;

	if (!device_may_wakeup(dev) || timer->wake_timeout < 0)
		return 0;

	ret = disable_irq_wake(timer->irq);

	brcmstb_waketmr_clear_alarm(timer);

	return ret;
}
#endif /* CONFIG_PM_SLEEP */

static SIMPLE_DEV_PM_OPS(brcmstb_waketmr_pm_ops, brcmstb_waketmr_suspend,
		brcmstb_waketmr_resume);

static const struct of_device_id brcmstb_waketmr_of_match[] = {
	{ .compatible = "brcm,brcmstb-waketimer" },
	{},
};

static struct platform_driver brcmstb_waketmr_driver = {
	.remove			= brcmstb_waketmr_remove,
	.driver = {
		.name		= DRV_NAME,
		.owner		= THIS_MODULE,
		.pm		= &brcmstb_waketmr_pm_ops,
		.of_match_table	= of_match_ptr(brcmstb_waketmr_of_match),
	}
};

static int __init brcmstb_waketmr_init(void)
{
	return platform_driver_probe(&brcmstb_waketmr_driver,
				     brcmstb_waketmr_probe);
}

static void __exit brcmstb_waketmr_exit(void)
{
	platform_driver_unregister(&brcmstb_waketmr_driver);
}

module_init(brcmstb_waketmr_init);
module_exit(brcmstb_waketmr_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Broadcom Corporation");
MODULE_DESCRIPTION("Wake-up timer driver for STB chips");
