/*
 * drivers/stm/pio10.c
 *
 * (c) 2009 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * Support for the "Standalone 10 banks PIO" block. See ADCS 8073738
 * for details.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/stm/pio.h>
#include <linux/stm/soc.h>
#include "pio_i.h"

#define STPIO10_STATUS_BANK	0xf000
#define STPIO10_STATUS_OFFSET	0x80

struct stpio10_port {
	void __iomem *base;
	struct stpio_port *ports[0];
};

static void stpio10_irq_chip_handler(unsigned int irq, struct irq_desc *desc)
{
	const struct stpio10_port *port10 = get_irq_data(irq);
	unsigned long status;
	int portno;

	status = readl(port10->base + STPIO10_STATUS_OFFSET);
	while ((portno = ffs(status)) != 0) {
		portno--;
		stpio_irq_handler(port10->ports[portno]);
		status &= ~(1<<portno);
	}
}

void __init stpio10_early_init(struct platform_device *pdev,
			       unsigned long start)
{
	int j;
	struct stpio10_data *data = pdev->dev.platform_data;

	for (j = 0; j < data->num_pio; j++)
		stpio_init_port(data->start_pio+j,
				start + (j*0x1000), pdev->dev.bus_id);
}

static int __devinit stpio10_probe(struct platform_device *pdev)
{
	struct stpio10_data *data = pdev->dev.platform_data;
	int port10_size;
	struct stpio10_port *port10;
	struct device *dev = &pdev->dev;
	unsigned long start, size;
	int irq;
	int j;

	port10_size = sizeof(*port10);
	port10_size += data->num_pio*sizeof(struct stpio_port *);
	port10 = devm_kzalloc(dev, port10_size, GFP_KERNEL);
	if (!port10)
		return -ENOMEM;

	if (stpio_get_resources(pdev, &start, &size, &irq))
		return -EIO;

	if (!devm_request_mem_region(dev, start, size, pdev->name))
		return -EBUSY;

	port10->base = devm_ioremap_nocache(dev, start + STPIO10_STATUS_BANK,
					    0x100);
	if (!port10->base)
		return -ENOMEM;

	for (j = 0; j < data->num_pio; j++) {
		struct stpio_port *port;
		port = stpio_init_port(data->start_pio+j,
				       start + (j*0x1000), pdev->dev.bus_id);
		if (IS_ERR(port))
		    return PTR_ERR(port);
		port10->ports[j] = port;
	}

	set_irq_chained_handler(irq, stpio10_irq_chip_handler);
	set_irq_data(irq, port10);
	for (j = 0; j < data->num_pio; j++)
		stpio_init_irq(data->start_pio+j);

	platform_set_drvdata(pdev, port10);

	return 0;
}

static int __devexit stpio10_remove(struct platform_device *pdev)
{
	struct stpio10_data *data = pdev->dev.platform_data;
	struct stpio10_port *port10 = platform_get_drvdata(pdev);
	int j;
	struct resource *resource;

	for (j = 0; j < data->num_pio; j++)
		stpio_remove_port(port10->ports[j]);

	resource = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	BUG_ON(!resource);
	free_irq(resource->start, port10);

	return 0;
}

static struct platform_driver stpio10_driver = {
	.driver	= {
		.name	= "stpio10",
		.owner	= THIS_MODULE,
	},
	.probe		= stpio10_probe,
	.remove		= __devexit_p(stpio10_remove),
};

static int __init stpio10_init(void)
{
	return platform_driver_register(&stpio10_driver);
}
postcore_initcall(stpio10_init);

MODULE_AUTHOR("Stuart Menefy <stuart.menefy@st.com>");
MODULE_DESCRIPTION("STMicroelectronics PIO driver");
MODULE_LICENSE("GPL");
