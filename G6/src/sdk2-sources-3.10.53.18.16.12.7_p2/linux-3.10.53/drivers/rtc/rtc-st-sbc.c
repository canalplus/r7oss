/*
 * This driver implements a RTC for the Low Power Mode
 * in some STMicroelectronics devices.
 *
 * Copyright (C) 2014 STMicroelectronics Limited
 * Author: Satbir Singh <satbir.singh@st.com>
 * Author: David Paris <david.paris@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License version 2.  See linux/COPYING for more information.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/rtc.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/power/st_lpm.h>

struct st_sbc_rtc {
	struct rtc_device *rtc_dev;
	struct rtc_wkalrm alarm;
	struct rtc_time tm_cur;
};

static int st_sbc_rtc_alarm_callback(void *data)
{
	struct device *dev = (struct device *)data;
	struct st_sbc_rtc *rtc = dev_get_drvdata(dev);

	dev_dbg(dev, "%s - SBC Alarm rang\n", __func__);
	rtc_update_irq(rtc->rtc_dev, 1, RTC_AF);
	rtc->alarm.enabled = 0;
	return 0;
}

static int st_sbc_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	int ret = 0;
	struct st_sbc_rtc *rtc = dev_get_drvdata(dev);

	memcpy(&rtc->tm_cur, tm, sizeof(rtc->tm_cur));

	st_lpm_set_rtc(tm);

	return ret;
}

static int st_sbc_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	struct st_sbc_rtc *rtc = dev_get_drvdata(dev);

	st_lpm_get_rtc(tm);

	memcpy(&rtc->tm_cur, tm, sizeof(rtc->tm_cur));

	return 0;
}

static int st_sbc_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *wkalrm)
{
	struct st_sbc_rtc *rtc = dev_get_drvdata(dev);

	memcpy(wkalrm, &rtc->alarm, sizeof(struct rtc_wkalrm));

	return 0;
}

static int st_sbc_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *t)
{
	int ret = 0;
	unsigned long lpt_alarm, lpt_cur;
	struct st_sbc_rtc *rtc = dev_get_drvdata(dev);
	struct rtc_time tm;

	rtc_tm_to_time(&t->time, &lpt_alarm);
	st_sbc_rtc_read_time(dev, &tm);
	rtc_tm_to_time(&tm, &lpt_cur);
	lpt_alarm = lpt_alarm - lpt_cur;
	rtc_time_to_tm(lpt_alarm, &t->time);

	memcpy(&rtc->alarm, t, sizeof(struct rtc_wkalrm));

	ret = st_lpm_set_wakeup_time(lpt_alarm);
	if (ret < 0)
		return ret;

	device_set_wakeup_enable(dev, true);
	st_lpm_set_wakeup_device(ST_LPM_WAKEUP_RTC);

	return 0;
}

static struct rtc_class_ops st_sbc_rtc_ops = {
	.read_time = st_sbc_rtc_read_time,
	.set_time = st_sbc_rtc_set_time,
	.read_alarm = st_sbc_rtc_read_alarm,
	.set_alarm = st_sbc_rtc_set_alarm,
};

static int st_sbc_rtc_probe(struct platform_device *pdev)
{
	int ret = -ENODEV;
	struct st_sbc_rtc *rtc;
#ifdef CONFIG_RTC_HCTOSYS
	struct rtc_device *hctosys_rtc;
	struct rtc_time tm;
	struct timespec tv = {
		.tv_nsec = NSEC_PER_SEC >> 1,
	};
#endif

	rtc = devm_kzalloc(&pdev->dev,
		sizeof(struct st_sbc_rtc), GFP_KERNEL);
	if (unlikely(!rtc))
		return -ENOMEM;

	device_init_wakeup(&pdev->dev, true);

	platform_set_drvdata(pdev, rtc);

	rtc->rtc_dev = rtc_device_register("rtc-st-sbc", &pdev->dev,
		&st_sbc_rtc_ops, THIS_MODULE);
	if (IS_ERR(rtc->rtc_dev)) {
		device_init_wakeup(&pdev->dev, false);
		return PTR_ERR(rtc->rtc_dev);
	}

	ret = st_lpm_register_callback(ST_LPM_RTC, st_sbc_rtc_alarm_callback,
			(void *)&pdev->dev);

#ifdef CONFIG_RTC_HCTOSYS
	hctosys_rtc = rtc_class_open(CONFIG_RTC_HCTOSYS_DEVICE);

	if (hctosys_rtc == NULL) {
		pr_err("%s: unable to open rtc device (%s)\n",
				__FILE__, CONFIG_RTC_HCTOSYS_DEVICE);
		return -ENODEV;
	}

	if (!strcmp(hctosys_rtc->name, rtc->rtc_dev->name)) {
		ret = rtc_read_time(rtc->rtc_dev, &tm);
		if (ret) {
			dev_err(rtc->rtc_dev->dev.parent,
			"hctosys: unable to read the hardware clock\n");
			goto error;
		}

		ret = rtc_valid_tm(&tm);
		if (ret) {
			dev_err(rtc->rtc_dev->dev.parent,
					"hctosys: invalid date/time\n");
			goto error;
		}

		rtc_tm_to_time(&tm, &tv.tv_sec);

		ret = do_settimeofday(&tv);

		dev_info(rtc->rtc_dev->dev.parent,
				"Setting system clock to %d-%02d-%02d %02d:%02d:%02d UTC\n",
				tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
				tm.tm_hour, tm.tm_min, tm.tm_sec);
	}

error:
	rtc_class_close(hctosys_rtc);
#endif

	return ret;
}

static int st_sbc_rtc_remove(struct platform_device *pdev)
{
	struct st_sbc_rtc *rtc = platform_get_drvdata(pdev);

	rtc_device_unregister(rtc->rtc_dev);
	device_init_wakeup(&pdev->dev, false);

	return 0;
}

#ifdef CONFIG_PM
static int st_sbc_rtc_suspend(struct device *dev)
{
	return 0;
}
static int st_sbc_rtc_resume(struct device *dev)
{
	struct st_sbc_rtc *rtc = dev_get_drvdata(dev);

	device_set_wakeup_enable(dev, false);
	rtc_update_irq(rtc->rtc_dev, 1, RTC_AF);

	return 0;
}

static const struct dev_pm_ops st_sbc_rtc_pm_ops = {
	.suspend = st_sbc_rtc_suspend,
	.resume = st_sbc_rtc_resume,
};

#define ST_SBC_RTC_PM_OPS  (&st_sbc_rtc_pm_ops)
#else
#define ST_SBC_RTC_PM_OPS  NULL
#endif

static struct of_device_id st_sbc_rtc_match[] = {
	{
		.compatible = "st,rtc-sbc",
	},
	{},
};
MODULE_DEVICE_TABLE(of, st_sbc_rtc_match);

static struct platform_driver st_rtc_platform_driver = {
	.driver = {
		.name = "rtc-st-sbc",
		.owner = THIS_MODULE,
		.pm = ST_SBC_RTC_PM_OPS,
		.of_match_table = of_match_ptr(st_sbc_rtc_match),
	},
	.probe = st_sbc_rtc_probe,
	.remove = st_sbc_rtc_remove,
};

static int __init st_sbc_rtc_init(void)
{
	return platform_driver_register(&st_rtc_platform_driver);
}

void __exit st_sbc_rtc_exit(void)
{
	pr_info("ST RTC SBC driver removed\n");
	platform_driver_unregister(&st_rtc_platform_driver);
}

module_init(st_sbc_rtc_init);
module_exit(st_sbc_rtc_exit);

MODULE_DESCRIPTION("STMicroelectronics SBC RTC driver");
MODULE_AUTHOR("david.paris@st.com");
MODULE_LICENSE("GPLv2");
