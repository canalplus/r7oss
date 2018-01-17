/*
 *  Bluetooth GPIO Wake Up driver
 *
 *  Copyright (c) 2016 Technicolor Inc.
 *  Cyril Tostivint <cyril.tostivint@technicolor.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  Example of device-tree node:
 *  	btwake {
 *  		gpios = <0xa9 0xa 0x1>;
 *  		compatible = "btwakeup";
 *  	};
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>


struct btwakeup_priv {
	unsigned int irq;
};


static int btwakeup_suspend(struct device *dev)
{
	struct btwakeup_priv *priv = dev_get_drvdata(dev);

	if (device_may_wakeup(dev))
		enable_irq_wake(priv->irq);

	return 0;
}

static int btwakeup_resume(struct device *dev)
{
	struct btwakeup_priv *priv = dev_get_drvdata(dev);

	if (device_may_wakeup(dev))
		disable_irq_wake(priv->irq);

	return 0;
}

/**
 * When going in deep standby the suspend fcuntion isn't called
 * so we use the shutdown one. More over the enable_irq_wake is
 * useless as we are no going in sleep but kernel is shuting down
 */
static void btwakeup_shutdown(struct platform_device *pdev)
{
	struct btwakeup_priv *priv = dev_get_drvdata(&pdev->dev);

	if (device_may_wakeup(&pdev->dev))
		enable_irq(priv->irq);
}

static irqreturn_t btwakeup_irq_handler(int irq, void *data)
{
	return IRQ_HANDLED;
}

static int btwakeup_probe(struct platform_device *pdev)
{
	struct btwakeup_priv *priv;
	int gpio, err;

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	device_set_wakeup_capable(&pdev->dev, 1);

	gpio = of_get_gpio(pdev->dev.of_node, 0);
	if (gpio < 0) {
		dev_err(&pdev->dev, "Missing gpio property in device-tree: %d", gpio);
		return -EINVAL;
	}

	priv->irq = gpio_to_irq(gpio);

	err = devm_request_irq(&pdev->dev, priv->irq,
			btwakeup_irq_handler, IRQF_TRIGGER_RISING,
			dev_name(&pdev->dev), priv);
	if (err){
		dev_err(&pdev->dev, "Failed to request interrupt\n");
		return err;
	}

	disable_irq(priv->irq);

	platform_set_drvdata(pdev, priv);
	return 0;
}


static const struct dev_pm_ops btwakeup_pm_ops = {
	.suspend= btwakeup_suspend,
	.resume = btwakeup_resume,
};

static struct of_device_id btwakeup_of_match[] = {
	{ .compatible = "btwakeup" },
	{},
};
MODULE_DEVICE_TABLE(of, btwakeup_of_match);

static struct platform_driver btwakeup_driver = {
	.driver = {
		.name = "btwakeup",
		.owner	= THIS_MODULE,
		.of_match_table = btwakeup_of_match,
		.pm = &btwakeup_pm_ops,
	},
	.probe = btwakeup_probe,
	.shutdown = btwakeup_shutdown,
};

module_platform_driver(btwakeup_driver);

MODULE_AUTHOR("Technicolor");
MODULE_DESCRIPTION("Bluetooth Wake Up driver");
MODULE_VERSION("2");
MODULE_LICENSE("GPL");
