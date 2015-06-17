/* linux/drivers/i2c/busses/i2c-stm-pio.c

   Copyright (c) 2004 STMicroelectronics Limited
   Author: Stuart Menefy <stuart.menefy@st.com>

   STM I2C bus driver using PIO pins

   Derived from i2c-velleman.c which was:
   Copyright (C) 1995-96, 2000 Simon G. Vogl

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>
#include <linux/i2c-id.h>
#include <linux/stm/pio.h>
#include <linux/stm/soc.h>
#include <asm/io.h>
#include <asm/param.h> /* for HZ */

#define NAME "i2c_stm_pio"

static void bit_stm_pio_setscl(void *data, int state)
{
	struct platform_device *pdev = (struct platform_device *)data;
	struct ssc_pio_t *pio_info =
		(struct ssc_pio_t *)pdev->dev.platform_data;
	stpio_set_pin(pio_info->clk, state);
	stpio_get_pin(pio_info->clk);
}

static void bit_stm_pio_setsda(void *data, int state)
{
	struct platform_device *pdev = (struct platform_device *)data;
	struct ssc_pio_t *pio_info =
		(struct ssc_pio_t *)pdev->dev.platform_data;
	stpio_set_pin(pio_info->sdout, state);
	stpio_get_pin(pio_info->sdout);
}

static int bit_stm_pio_getscl(void *data)
{
	struct platform_device *pdev = (struct platform_device *)data;
	struct ssc_pio_t *pio_info =
		(struct ssc_pio_t *)pdev->dev.platform_data;
	return stpio_get_pin(pio_info->clk);
}

static int bit_stm_pio_getsda(void *data)
{
	struct platform_device *pdev = (struct platform_device *)data;
	struct ssc_pio_t *pio_info = (struct ssc_pio_t *)pdev->dev.platform_data;
	return stpio_get_pin(pio_info->sdout);
}

static int __init i2c_stm_probe(struct platform_device *pdev)
{
	struct ssc_pio_t *pio_info =
		(struct ssc_pio_t *)pdev->dev.platform_data;
	struct i2c_adapter *i2c_bus;
	struct i2c_algo_bit_data *algo;

	i2c_bus = devm_kzalloc(&pdev->dev, sizeof(struct i2c_adapter),
			GFP_KERNEL);
	if (!i2c_bus)
		return -1;

	algo = devm_kzalloc(&pdev->dev, sizeof(struct i2c_algo_bit_data),
			GFP_KERNEL);
	if (!algo)
		return -1;

	pio_info->clk = stpio_request_set_pin(pio_info->pio[0].pio_port,
			pio_info->pio[0].pio_pin,
			"I2C Clock", STPIO_BIDIR, 1);

	if (!pio_info->clk) {
		printk(KERN_ERR NAME"Failed to clk pin allocation\n");
		return -1;
	}
	pio_info->sdout = stpio_request_set_pin(pio_info->pio[1].pio_port,
			pio_info->pio[1].pio_pin,
			"I2C Data", STPIO_BIDIR, 1);
	if (!pio_info->sdout){
		printk(KERN_ERR NAME"Failed to sda pin allocation\n");
		return -1;
	}

	printk(KERN_INFO NAME": allocated pin (%d,%d) for scl (0x%p)\n",
			pio_info->pio[0].pio_port,
			pio_info->pio[0].pio_pin,
			pio_info->clk);
	printk(KERN_INFO NAME": allocated pin (%d,%d) for sda (0x%p)\n",
			pio_info->pio[1].pio_port,
			pio_info->pio[1].pio_pin,
			pio_info->sdout);

	sprintf(i2c_bus->name, "i2c_pio_%d", pdev->id);
	i2c_bus->nr = pdev->id;
	i2c_bus->id = I2C_HW_B_STM_PIO;
	i2c_bus->algo_data = algo;
	i2c_bus->dev.parent = &pdev->dev;

	algo->data = pdev;
	algo->setsda = bit_stm_pio_setsda;
	algo->setscl = bit_stm_pio_setscl;
	algo->getsda = bit_stm_pio_getsda;
	algo->getscl = bit_stm_pio_getscl;
	algo->udelay = 5;
	algo->timeout = HZ;

	pdev->dev.driver_data = (void *)i2c_bus;
	if (i2c_bit_add_numbered_bus(i2c_bus) < 0) {
		printk(KERN_ERR NAME "The I2C Core refuses the i2c-pio "
				"adapter\n");
		return -1;
	}

	return 0;
}

static int i2c_stm_remove(struct platform_device *pdev)
{
	struct ssc_pio_t *pio_info =
		(struct ssc_pio_t *)pdev->dev.platform_data;
	struct i2c_adapter *i2c_bus =
		(struct i2c_adapter *)pdev->dev.driver_data;
	struct i2c_algo_bit_data *algo = i2c_bus->algo_data;

	i2c_del_adapter(i2c_bus);

	stpio_free_pin(pio_info->clk);
	stpio_free_pin(pio_info->sdout);
	devm_kfree(&pdev->dev,algo);
	devm_kfree(&pdev->dev,i2c_bus);
	return 0;
}

#ifdef CONFIG_PM
static int i2c_stm_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct ssc_pio_t *pio_info =
		(struct ssc_pio_t *)pdev->dev.platform_data;
	stpio_configure_pin(pio_info->clk, STPIO_IN);
	stpio_configure_pin(pio_info->sdout, STPIO_IN);
	return 0;
}
static int i2c_stm_resume(struct platform_device *pdev)
{
	struct ssc_pio_t *pio_info =
		(struct ssc_pio_t *)pdev->dev.platform_data;
	stpio_configure_pin(pio_info->clk, STPIO_BIDIR);
	stpio_configure_pin(pio_info->sdout, STPIO_BIDIR);

	stpio_set_pin(pio_info->clk, 1);
	stpio_set_pin(pio_info->sdout, 1);
	return 0;
}
#else
  #define i2c_stm_suspend	NULL
  #define i2c_stm_resume	NULL
#endif

static struct platform_driver i2c_sw_driver = {
	.driver.name = "i2c_stm_pio",
	.driver.owner = THIS_MODULE,
	.probe = i2c_stm_probe,
	.remove= i2c_stm_remove,
	.suspend = i2c_stm_suspend,
	.resume = i2c_stm_resume,
};

static int __init i2c_stm_pio_init(void)
{
	printk(KERN_INFO "i2c: STM PIO based I2C Driver\n");

	platform_driver_register(&i2c_sw_driver);

	return 0;
}

static void __exit i2c_stm_pio_exit(void)
{
	platform_driver_unregister(&i2c_sw_driver);
}

MODULE_AUTHOR("Stuart Menefy <stuart.menefy@st.com>");
MODULE_DESCRIPTION("STM PIO based I2C Driver");
MODULE_LICENSE("GPL");

module_init(i2c_stm_pio_init);
module_exit(i2c_stm_pio_exit);
