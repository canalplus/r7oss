/* --------------------------------------------------------------------
 * <root>/drivers/stm/lpc_5xxx.c
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
#include <linux/io.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/errno.h>


#ifdef CONFIG_STM_LPC_DEBUG
#define dgb_print(fmt, args...)	printk(KERN_INFO "%s: " fmt, __func__, ## args)
#else
#define dgb_print(fmt, args...)
#endif

#define SS_LOCK_CFG		0x300

#define SS_LPC_CFG0		0x120
#define SS_LPC_CFG1		0x124
#define SS_LPC_CFG1_ENABLE	(1<<4)
#define SS_LPC_MASK		0xfffff
#define SS_LPC_MASK_LOW		0xffff
#define SS_LPC_MASK_HI		0xf
#define SS_LPC_CLOCK		1	/* Hz */

struct lpc_device {
	unsigned long base; /* base address for system services */
	struct platform_device *pdev;
	unsigned long timeout;
	unsigned long state;
};

static struct lpc_device lpc;

#define lpc_store32(lpc, offset, value)	iowrite32(value, (lpc)->base + offset)
#define lpc_load32(lpc, offset)		ioread32((lpc)->base + offset)

#define lpc_set_enabled(l)	device_set_wakeup_enable(&(l)->pdev->dev, 1)
#define lpc_set_disabled(l)	device_set_wakeup_enable(&(l)->pdev->dev, 0)

#define lpc_set_timeout(l, t)		(l)->timeout = (t)
#define lpc_read_timeout(l)		((l)->timeout)
#define lpc_is_enabled(l)		((l)->state & LPC_STATE_ENABLED)




static irqreturn_t stlpc_irq(int this_irq, void *data)
{
	dgb_print("Received interrupt from LPC\n");
	return IRQ_HANDLED;
}

/*
 * Currently the LPC_UNLOCK/LPC_LOCK are required
 * only on the stx5197 platform
 */
static inline void LPC_UNLOCK(void)
{
	writel(0xf0, lpc.base + SS_LOCK_CFG);
	writel(0x0f, lpc.base + SS_LOCK_CFG);
}

static inline void LPC_LOCK(void)
{
	writel(0x100, lpc.base + SS_LOCK_CFG);
}


void stlpc_write(unsigned long long counter)
{
	union {
		unsigned long parts[2];
		unsigned long long value;
	} tmp;
	tmp.value = counter;
	dgb_print("\n");
	lpc_set_timeout(&lpc, tmp.parts[0]);
}
EXPORT_SYMBOL(stlpc_write);

unsigned long long stlpc_read(void)
{
	union {
		unsigned long parts[2];
		unsigned long long value;
	} tmp;
	tmp.value = 0;
	tmp.parts[0] = lpc_read_timeout(&lpc);
	return tmp.value;
}
EXPORT_SYMBOL(stlpc_read);

int stlpc_set(int enable, unsigned long long tick)
{
	union {
		unsigned long parts[2];
		unsigned long long value;
	} tmp;
	tmp.value = tick;
	dgb_print("\n");
	if (enable) {
		lpc_set_timeout(&lpc, tmp.parts[0]);
		lpc_set_enabled(&lpc);
	} else
		lpc_set_disabled(&lpc);
	return 0;
}
EXPORT_SYMBOL(stlpc_set);

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
	value *= SS_LPC_CLOCK;
	value &= SS_LPC_MASK;
	dgb_print("tick = %llu\n", value);
	lpc_set_timeout(&lpc, value);
	return count;
}

static DEVICE_ATTR(timeout, S_IRUGO | S_IWUSR, stlpc_show_timeout,
			stlpc_store_timeout);

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

no_hw_request:
	if (!(lpc.base =
		devm_ioremap_nocache(&pdev->dev, res->start,
				(int)(res->end - res->start)))) {
		printk(KERN_ERR "%s: Request iomem 0x%x region not done\n",
			__func__, (unsigned int)res->start);
		return -ENOMEM;
	}

/*
 *  The LPC on 5197 doesn't generate interrupt.
 *  It generates the 'wakeup event signal' connected to the
 *  System Service'
 *  On the 'wakeup event signal' the Systme Service exits from
 *  Standby and goes directly in X1 state
 */
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res)
		return -ENODEV;

	if (res->start == -1)
		goto no_irq;
	if (devm_request_irq(&pdev->dev, res->start, stlpc_irq,
		IRQF_DISABLED, "stlpc", &lpc) < 0) {
		printk(KERN_ERR "%s: Request irq not done\n", __func__);
		return -ENODEV;
	}
	set_irq_type(res->start, data->irq_edge_level);
	enable_irq_wake(res->start);

no_irq:
	lpc.pdev = pdev;
	if (device_create_file(&(pdev->dev), &dev_attr_timeout));
	return 0;
}

static int stlpc_remove(struct platform_device *pdev)
{
	dgb_print("\n");
	devm_iounmap(&pdev->dev, lpc.base);
	return 0;
}

#ifdef CONFIG_PM
static int stlpc_suspend(struct platform_device *pdev, pm_message_t state)
{
	unsigned long timeout = lpc.timeout & SS_LPC_MASK;

	dgb_print("\n");
	if (state.event != PM_EVENT_SUSPEND  ||
	    !device_may_wakeup(&(pdev->dev)) ||
	    !lpc_read_timeout(&lpc))
		return  0;
	dgb_print("LPC able to wakeup\n");
	LPC_UNLOCK();
	lpc_store32(&lpc, SS_LPC_CFG0, timeout & SS_LPC_MASK_LOW);
	timeout >>= 16;
	timeout |= SS_LPC_CFG1_ENABLE;
	lpc_store32(&lpc, SS_LPC_CFG1, timeout);
	LPC_LOCK();
	dgb_print("LPC wakeup On\n");
	return 0;
}

static int stlpc_resume(struct platform_device *pdev)
{
/*
 * Reset the 'enable' and the 'timeout' to be
 * compliant with the hardware reset
 */
	dgb_print("\n");
	lpc_set_disabled(&lpc);
	lpc_set_timeout(&lpc, 0);
	LPC_UNLOCK();
	lpc_store32(&lpc, SS_LPC_CFG0, 1);
	lpc_store32(&lpc, SS_LPC_CFG1, 0);
	LPC_LOCK();
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
