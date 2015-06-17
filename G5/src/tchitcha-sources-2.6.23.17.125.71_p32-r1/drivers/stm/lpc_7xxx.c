/* --------------------------------------------------------------------
 * <root>/drivers/stm/lpc_71xx.c
 * --------------------------------------------------------------------
 *  Copyright (C) 2009 : STMicroelectronics
 *  Author: Francesco M. Virlinzi <francesco.virlinzi@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License version 2.0 ONLY.  See linux/COPYING for more information.
 *
 */

#include <linux/stm/lpc.h>
#include <linux/stm/soc.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <asm/clock.h>


#ifdef CONFIG_STM_LPC_DEBUG
#define dgb_print(fmt, args...)	printk(KERN_INFO "%s: " fmt, __func__ , ## args)
#else
#define dgb_print(fmt, args...)
#endif

/*
 * LPC Workaround on 7111:
 * The LPC when enabled can not be stopped
 * To reset in any case the LPC when the system is resume
 * we route the irw_wake_irq and the eth_wake_irq to the
 * on the __'ilc3_wakeup_out' signal__ able to reset
 * the LPC.
 * In this manner any wakeup interrupt will reset the LPC
 * and will gave us the reprogramming capability.
 *
 * LPC Workaround on 7141:
 * On 7141 the LPC can work as watchdog.
 * On this platform to reset the LPC a WatchDog_reset
 * is requested able to perform a LPC reset but without
 * any real WDT Reset signal able to hangs the SOC.
 */


#define LPA_LS		0x410
#define LPA_MS		0x414
#define LPA_START	0x418
#define LPA_WDT		0x510	/* stx7141 */

#define DEFAULT_LPC_FREQ	46875	/* Hz */

struct lpc_device {
	unsigned long base;
	struct platform_device *pdev;
	unsigned long long timeout;
	unsigned long state;
	int irq;
};

static struct lpc_device lpc;
struct clk *lpc_clk;

#define lpc_store32(lpc, offset, value)	iowrite32(value, (lpc)->base + offset)
#define lpc_load32(lpc, offset)		ioread32((lpc)->base + offset)

#define lpc_set_enabled(l)	device_set_wakeup_enable(&(l)->pdev->dev, 1)
#define lpc_set_disabled(l)	device_set_wakeup_enable(&(l)->pdev->dev, 0)

#define lpc_set_timeout(l, t)		(l)->timeout = (t)
#define lpc_read_timeout(l)		((l)->timeout)
#define lpc_is_enabled(l)		((l)->state & LPC_STATE_ENABLED)

void stlpc_write(unsigned long long counter)
{
	dgb_print("\n");
	lpc_set_timeout(&lpc, counter);
}
EXPORT_SYMBOL(stlpc_write);

unsigned long long stlpc_read(void)
{
	return lpc_read_timeout(&lpc);
}
EXPORT_SYMBOL(stlpc_read);

unsigned long stlpc_read_sec(void)
{
	return lpc_read_timeout(&lpc) /
		(lpc_clk ? clk_get_rate(lpc_clk) : DEFAULT_LPC_FREQ);
}
EXPORT_SYMBOL(stlpc_read_sec);

int stlpc_set(int enable, unsigned long long tick)
{
	dgb_print("\n");
	if (enable) {
		lpc_set_timeout(&lpc, tick);
		lpc_set_enabled(&lpc);
	} else
		lpc_set_disabled(&lpc);
	return 0;
}
EXPORT_SYMBOL(stlpc_set);

static irqreturn_t stlpc_irq(int this_irq, void *data)
{
	dgb_print("Interrupt from LPC\n");
	lpc_store32(&lpc, LPA_START, 0);
	return IRQ_HANDLED;
}

static ssize_t stlpc_show_timeout(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int ret = 0;
	unsigned long long value;
	dgb_print("\n");
	value = lpc_read_timeout(&lpc);

	ret = sprintf(buf, "%llu", value);
	return ret;
}

static ssize_t stlpc_store_timeout(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	unsigned long long value = 0;
	dgb_print("\n");
	value =  simple_strtoul(buf, NULL, 10);
	dgb_print("value = %llu\n", value);
	value *= (lpc_clk ? clk_get_rate(lpc_clk) : DEFAULT_LPC_FREQ);
	dgb_print("tick = %llu\n", value);
	lpc_set_timeout(&lpc, value);
	return count;
}

static DEVICE_ATTR(timeout, S_IRUGO | S_IWUSR, stlpc_show_timeout,
			stlpc_store_timeout);

#ifdef CONFIG_STM_LPC_DEBUG
static ssize_t stlpc_store_enable(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	long flags;
	union {
		unsigned long long value;
		unsigned long parts[2];
	} tmp;
	tmp.value = lpc.timeout;
	dgb_print("\n");
	if (strcmp(buf, "on") == 0) {
		local_irq_save(flags);
		lpc_store32(&lpc, LPA_START, 1);
		lpc_store32(&lpc, LPA_LS, tmp.parts[0]);
		lpc_store32(&lpc, LPA_MS, tmp.parts[1]);
		local_irq_restore(flags);
	} else if (strcmp(buf, "off") == 0) {
		local_irq_save(flags);
		lpc_store32(&lpc, LPA_START, 0);
		local_irq_restore(flags);
	};
	return count;
}

static DEVICE_ATTR(enable, S_IWUSR, NULL, stlpc_store_enable);
#endif

static int __init stlpc_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct plat_lpc_data *data = (struct plat_lpc_data *)
		pdev->dev.platform_data;

	dgb_print("\n");
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -ENODEV;
	if (data->no_hw_req)
		goto no_hw_request;
	if (!devm_request_mem_region(&pdev->dev, res->start,
		res->end - res->start, "stlpc")){
		printk(KERN_ERR "%s: Request mem 0x%x region not done\n",
			__func__, res->start);
		return -ENOMEM;
	}

	if (!(lpc.base = (unsigned long)devm_ioremap_nocache
		         (&pdev->dev, res->start, res->end - res->start))) {
		printk(KERN_ERR "%s: Request iomem 0x%x region not done\n",
			__func__, (unsigned int)res->start);
		return -ENOMEM;
	}

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		printk(KERN_ERR "%s Request irq %d not done\n",
			__func__, res->start);
		return -ENODEV;
	}

	if (res->start == -1)
		goto no_irq;

	set_irq_type(res->start, data->irq_edge_level);
	if (devm_request_irq(&pdev->dev, res->start, stlpc_irq,
		IRQF_DISABLED, "stlpc", &lpc) < 0){
		printk(KERN_ERR "%s: Request irq not done\n", __func__);
		return -ENODEV;
	}

no_hw_request:
	if (data->clk_id) {
		dgb_print("Looking for clk: %s\n", data->clk_id);
		lpc_clk = clk_get(NULL, data->clk_id);
		if (lpc_clk)
			dgb_print("Using clock %s @ %u Hz\n",
				lpc_clk->name, clk_get_rate(lpc_clk));
	}

no_irq:
	lpc.irq = res->start;
	lpc.pdev = pdev;
	if (device_create_file(&(pdev->dev), &dev_attr_timeout));
#ifdef CONFIG_STM_LPC_DEBUG
	if (device_create_file(&(pdev->dev), &dev_attr_enable));
#endif
	return 0;
}

static int stlpc_remove(struct platform_device *pdev)
{
	struct resource *res;
	dgb_print("\n");
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	devm_free_irq(&pdev->dev, res->start, NULL);
	devm_iounmap(&pdev->dev, (void *)lpc.base);
	return 0;
}

#ifdef CONFIG_PM
static int stlpc_suspend(struct platform_device *pdev, pm_message_t state)
{
	union {
		unsigned long long value;
		unsigned long parts[2];
	} tmp;
	dgb_print("\n");
	if (state.event != PM_EVENT_SUSPEND  ||
	    !device_may_wakeup(&(pdev->dev)) ||
	    !lpc_read_timeout(&lpc))
		return  0;
	tmp.value = lpc.timeout;
	dgb_print("LPC able to wakeup\n");
lpc_retry:
	lpc_store32(&lpc, LPA_LS, tmp.parts[0]);
	lpc_store32(&lpc, LPA_MS, tmp.parts[1]);
	lpc_store32(&lpc, LPA_START, 1);

	if (!lpc_load32(&lpc, LPA_LS) &&
	    !lpc_load32(&lpc, LPA_MS))
		goto lpc_retry;
	dgb_print("LPC wakeup On\n");
	return 0;
}

static int stlpc_resume(struct platform_device *pdev)
{
	long flags;
	struct plat_lpc_data *pdata = (struct plat_lpc_data *)
		pdev->dev.platform_data;
/*
 * Reset the 'enable' and the 'timeout' to be
 * compliant with the hardware reset
 */
	dgb_print("\n");
	if (device_may_wakeup(&(pdev->dev))) {
		local_irq_save(flags);
		if (pdata->need_wdt_reset) {
			lpc_store32(&lpc, LPA_MS, 0);
			lpc_store32(&lpc, LPA_LS, 0);
			lpc_store32(&lpc, LPA_WDT, 1);
			lpc_store32(&lpc, LPA_START, 1);
			lpc_store32(&lpc, LPA_WDT, 0);
		} else {
			lpc_store32(&lpc, LPA_START, 0);
			lpc_store32(&lpc, LPA_MS, 1);
			lpc_store32(&lpc, LPA_LS, 1);
		}
		local_irq_restore(flags);
	}

	lpc_set_disabled(&lpc);
	lpc_set_timeout(&lpc, 0);
	return 0;
}
#else
#define stlpc_suspend	NULL
#define stlpc_resume	NULL
#endif

static struct platform_driver stlpc_driver = {
	.driver.name = "stlpc",
	.driver.owner = THIS_MODULE,
	.probe = stlpc_probe,
	.remove = stlpc_remove,
	.suspend = stlpc_suspend,
	.resume = stlpc_resume,
};

static int __init stlpc_init(void)
{
	dgb_print("\n");
	platform_driver_register(&stlpc_driver);
	printk(KERN_INFO "stlpc device driver registered\n");
	return 0;
}

static void __exit stlpc_exit(void)
{
	dgb_print("\n");
	platform_driver_unregister(&stlpc_driver);
}

module_init(stlpc_init);
module_exit(stlpc_exit);

MODULE_AUTHOR("STMicroelectronics  <www.st.com>");
MODULE_DESCRIPTION("LPC device driver for STMicroelectronics devices");
MODULE_LICENSE("GPL");
