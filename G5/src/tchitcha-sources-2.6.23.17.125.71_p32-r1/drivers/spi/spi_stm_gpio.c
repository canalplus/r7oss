/*
 *  -------------------------------------------------------------------------
 *  spi_stm_gpio.c SPI/GPIO driver for STMicroelectronics platforms
 *  -------------------------------------------------------------------------
 *
 *  Copyright (c) 2008 STMicroelectronics Limited
 *  Author: Francesco Virlinzi <francesco.virlinzi@st.com>
 *
 *  May be copied or modified under the terms of the GNU General Public
 *  License version 2.0 ONLY.  See linux/COPYING for more information.
 *
 *  -------------------------------------------------------------------------
 *  Changelog:
 *  2008-01-24 Angus Clark <angus.clark@st.com>
 *    - chip_select modified to ignore devices with no chip_select, and keep
 *      hold of PIO pin (freeing pin selects STPIO_IN (high-Z) mode).
 *    - added spi_stmpio_setup() and spi_stmpio_setup_transfer() to enfore
 *	SPI_STMPIO_MAX_SPEED_HZ
 *  2008-08-28 Angus Clark <angus.clark@st.com>
 *    - Updated to fit with changes to 'ssc_pio_t'
 *    - Support for user-defined chip_select, specified in board setup
 *
 *  -------------------------------------------------------------------------
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/spi/spi.h>
#include <linux/stm/pio.h>
#include <linux/stm/soc.h>
#include <linux/io.h>
#include <linux/param.h>

#ifdef CONFIG_SPI_DEBUG
#define dgb_print(fmt, args...)  printk(KERN_INFO "%s: " \
					fmt, __FUNCTION__ , ## args)
#else
#define dgb_print(fmt, args...)	do { } while (0)
#endif

#define NAME "spi_stm_pio"

/* Maybe this should be included in platform_data? */
#define SPI_STMPIO_MAX_SPEED_HZ		1000000

static inline void setsck(struct spi_device *dev, int on)
{
	struct platform_device *pdev =
		container_of(dev->dev.parent, struct platform_device, dev);
	struct ssc_pio_t *pio_info =
		(struct ssc_pio_t *)pdev->dev.platform_data;
	stpio_set_pin(pio_info->clk, on ? 1 : 0);
}

static inline void setmosi(struct spi_device *dev, int on)
{
	struct platform_device *pdev
		= container_of(dev->dev.parent, struct platform_device, dev);
	struct ssc_pio_t *pio_info =
		(struct ssc_pio_t *)pdev->dev.platform_data;
	stpio_set_pin(pio_info->sdout, on ? 1 : 0);
}

static inline u32 getmiso(struct spi_device *dev)
{
	struct platform_device *pdev
		= container_of(dev->dev.parent, struct platform_device, dev);
	struct ssc_pio_t *pio_info =
		(struct ssc_pio_t *)pdev->dev.platform_data;
	return stpio_get_pin(pio_info->sdin) ? 1 : 0;
}

#define EXPAND_BITBANG_TXRX
#define spidelay(x) ndelay(x)
#include <linux/spi/spi_bitbang.h>

static u32 spi_gpio_txrx_mode0(struct spi_device *spi,
				unsigned nsecs, u32 word, u8 bits)
{
	dgb_print("\n");
	return bitbang_txrx_be_cpha0(spi, nsecs, 0, word, bits);
}

static u32 spi_gpio_txrx_mode1(struct spi_device *spi,
				unsigned nsecs, u32 word, u8 bits)
{
	dgb_print("\n");
	return bitbang_txrx_be_cpha1(spi, nsecs, 0, word, bits);
}

static u32 spi_gpio_txrx_mode2(struct spi_device *spi,
				unsigned nsecs, u32 word, u8 bits)
{
	dgb_print("\n");
	return bitbang_txrx_be_cpha0(spi, nsecs, 1, word, bits);
}

static u32 spi_gpio_txrx_mode3(struct spi_device *spi,
				unsigned nsecs, u32 word, u8 bits)
{
	dgb_print("\n");
	return bitbang_txrx_be_cpha1(spi, nsecs, 1, word, bits);
}

static void spi_stpio_chipselect(struct spi_device *spi, int value)
{
	unsigned int out;

	dgb_print("\n");
	if (spi->chip_select == SPI_NO_CHIPSELECT)
		return;

	/* Request stpio_pin if not already done so */
	/*  (stored in spi_device->controller_data) */
	if (!spi->controller_data)
		spi->controller_data =
			stpio_request_pin(spi_get_bank(spi->chip_select),
					  spi_get_line(spi->chip_select),
					  "spi-cs", STPIO_OUT);

	if (!spi->controller_data) {
		printk(KERN_ERR NAME " Error spi-cs locked or not-exist\n");
		return;
	}

	if (value == BITBANG_CS_ACTIVE)
		out = spi->mode & SPI_CS_HIGH ? 1 : 0;
	else
		out = spi->mode & SPI_CS_HIGH ? 0 : 1;

	stpio_set_pin((struct stpio_pin *)spi->controller_data, out);

	dgb_print("%s PIO%d[%d] -> %d \n",
		  value == BITBANG_CS_ACTIVE ? "select" : "deselect",
		  spi_get_bank(spi->chip_select),
		  spi_get_line(spi->chip_select), out);

	return;
}

struct spi_stm_gpio {
	struct spi_bitbang	bitbang;
	struct platform_device	*pdev;

	/* Max speed supported by STPIO bit-banging SPI controller */
	int max_speed_hz;
};

static int spi_stmpio_setup(struct spi_device *spi)
{
	struct spi_stm_gpio *spi_st = spi_master_get_devdata(spi->master);

	dgb_print("\n");

	if (spi->max_speed_hz > spi_st->max_speed_hz) {
		printk(KERN_ERR NAME " requested baud rate (%dhz) exceeds "
		       "max (%dhz)\n",
		       spi->max_speed_hz, spi_st->max_speed_hz);
		return -EINVAL;
	}

	return spi_bitbang_setup(spi);
}

static int spi_stmpio_setup_transfer(struct spi_device *spi,
				     struct spi_transfer *t)
{
	dgb_print("\n");

	if (t)
		if (t->speed_hz > spi->max_speed_hz) {
			printk(KERN_ERR NAME " requested baud rate (%dhz) "
			       "exceeds max (%dhz)\n",
			       t->speed_hz, spi->max_speed_hz);
			return -EINVAL;
		}

	return spi_bitbang_setup_transfer(spi, t);
}

static int __init spi_probe(struct platform_device *pdev)
{
	struct ssc_pio_t *pio_info =
			(struct ssc_pio_t *)pdev->dev.platform_data;
	struct spi_master *master;
	struct spi_stm_gpio *st_bitbang;

	dgb_print("\n");
	master = spi_alloc_master(&pdev->dev, sizeof(struct spi_stm_gpio));
	if (!master)
		return -1;

	st_bitbang = spi_master_get_devdata(master);
	if (!st_bitbang)
		return -1;

	platform_set_drvdata(pdev, st_bitbang);
	st_bitbang->bitbang.master = master;
	st_bitbang->bitbang.master->setup = spi_stmpio_setup;
	st_bitbang->bitbang.setup_transfer = spi_stmpio_setup_transfer;
	st_bitbang->bitbang.chipselect = spi_stpio_chipselect;
	st_bitbang->bitbang.txrx_word[SPI_MODE_0] = spi_gpio_txrx_mode0;
	st_bitbang->bitbang.txrx_word[SPI_MODE_1] = spi_gpio_txrx_mode1;
	st_bitbang->bitbang.txrx_word[SPI_MODE_2] = spi_gpio_txrx_mode2;
	st_bitbang->bitbang.txrx_word[SPI_MODE_3] = spi_gpio_txrx_mode3;

	if (pio_info->chipselect)
		st_bitbang->bitbang.chipselect = (void (*)
						  (struct spi_device *, int))
			(pio_info->chipselect);
	else
		st_bitbang->bitbang.chipselect = spi_stpio_chipselect;

	master->num_chipselect = SPI_NO_CHIPSELECT + 1;
	master->bus_num = pdev->id;
	st_bitbang->max_speed_hz = SPI_STMPIO_MAX_SPEED_HZ;

	pio_info->clk = stpio_request_pin(pio_info->pio[0].pio_port,
					  pio_info->pio[0].pio_pin,
					  "SPI Clock", STPIO_OUT);
	if (!pio_info->clk) {
		printk(KERN_ERR NAME " Faild to clk pin allocation PIO%d[%d]\n",
		       pio_info->pio[0].pio_port, pio_info->pio[0].pio_pin);
		return -1;
	}
	pio_info->sdout = stpio_request_pin(pio_info->pio[1].pio_port,
					    pio_info->pio[1].pio_pin,
					    "SPI Data Out", STPIO_OUT);
	if (!pio_info->sdout) {
		printk(KERN_ERR NAME " Faild to sda pin allocation PIO%d[%d]\n",
		       pio_info->pio[1].pio_port, pio_info->pio[1].pio_pin);
		return -1;
	}
	pio_info->sdin = stpio_request_pin(pio_info->pio[2].pio_port,
					   pio_info->pio[2].pio_pin,
					   "SPI Data In", STPIO_IN);
	if (!pio_info->sdin) {
		printk(KERN_ERR NAME " Faild to sdo pin allocation PIO%d[%d]\n",
		       pio_info->pio[1].pio_port, pio_info->pio[1].pio_pin);
		return -1;
	}

	stpio_set_pin(pio_info->clk, 0);
	stpio_set_pin(pio_info->sdout, 0);
	stpio_set_pin(pio_info->sdin, 0);

	if (spi_bitbang_start(&st_bitbang->bitbang)) {
		printk(KERN_ERR NAME
		       "The SPI Core refuses the spi_stm_gpio adapter\n");
		return -1;
	}

	printk(KERN_INFO NAME ": Registered SPI Bus %d: "
	       "SCL [%d,%d], SDO [%d,%d], SDI [%d, %d]\n",
	       master->bus_num,
	       pio_info->pio[0].pio_port, pio_info->pio[0].pio_pin,
	       pio_info->pio[1].pio_port, pio_info->pio[1].pio_pin,
	       pio_info->pio[2].pio_port, pio_info->pio[2].pio_pin);

	return 0;
}

static int spi_remove(struct platform_device *pdev)
{
	struct ssc_pio_t *pio_info =
			(struct ssc_pio_t *)pdev->dev.platform_data;
	struct spi_stm_gpio *sp = platform_get_drvdata(pdev);

	dgb_print("\n");
	spi_bitbang_stop(&sp->bitbang);
	spi_master_put(sp->bitbang.master);
	stpio_free_pin(pio_info->clk);
	stpio_free_pin(pio_info->sdout);
	stpio_free_pin(pio_info->sdin);
	return 0;
}

static struct platform_driver spi_sw_driver = {
	.driver.name = "spi_st_pio",
	.driver.owner = THIS_MODULE,
	.probe = spi_probe,
	.remove = spi_remove,
};

static int __init spi_gpio_init(void)
{
	printk(KERN_INFO NAME ": PIO based SPI Driver\n");
	return platform_driver_register(&spi_sw_driver);
}

static void __exit spi_gpio_exit(void)
{
	dgb_print("\n");
	platform_driver_unregister(&spi_sw_driver);
}

MODULE_AUTHOR("Francesco Virlinzi <francesco.virlinzi@st.com>");
MODULE_DESCRIPTION("GPIO based SPI Driver");
MODULE_LICENSE("GPL");

module_init(spi_gpio_init);
module_exit(spi_gpio_exit);

