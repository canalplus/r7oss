/*
 * Broadcom STB AVS TMON thermal sensor driver
 *
 * Copyright (C) 2015 Broadcom Corporation
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */

#define DRV_NAME	"brcmstb_thermal"

#define pr_fmt(fmt)	DRV_NAME ": " fmt

#include <linux/bitops.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/irqreturn.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/thermal.h>

#define AVS_TMON_STATUS			0x00
 #define AVS_TMON_STATUS_valid_msk	BIT(11)
 #define AVS_TMON_STATUS_data_msk	GENMASK(10, 1)
 #define AVS_TMON_STATUS_data_shift	1

#define AVS_TMON_EN_OVERTEMP_RESET	0x04
 #define AVS_TMON_EN_OVERTEMP_RESET_msk	BIT(0)

#define AVS_TMON_RESET_THRESH		0x08
 #define AVS_TMON_RESET_THRESH_msk	GENMASK(10, 1)
 #define AVS_TMON_RESET_THRESH_shift	1

#define AVS_TMON_INT_IDLE_TIME		0x10

#define AVS_TMON_EN_TEMP_INT_SRCS	0x14
 #define AVS_TMON_EN_TEMP_INT_SRCS_high	BIT(1)
 #define AVS_TMON_EN_TEMP_INT_SRCS_low	BIT(0)

#define AVS_TMON_INT_THRESH		0x18
 #define AVS_TMON_INT_THRESH_high_msk	GENMASK(26, 17)
 #define AVS_TMON_INT_THRESH_high_shift	17
 #define AVS_TMON_INT_THRESH_low_msk	GENMASK(10, 1)
 #define AVS_TMON_INT_THRESH_low_shift	1

#define AVS_TMON_TEMP_INT_CODE		0x1c
#define AVS_TMON_TP_TEST_ENABLE		0x20

enum avs_tmon_trip_type {
	TMON_TRIP_TYPE_LOW = 0,
	TMON_TRIP_TYPE_HIGH,
	TMON_TRIP_TYPE_RESET,
	TMON_TRIP_TYPE_MAX,
};

struct avs_tmon_trip {
	/* HW bit to enable the trip */
	u32 enable_offs;
	u32 enable_mask;

	/* HW field to read the trip temperature */
	u32 reg_offs;
	u32 reg_msk;
	int reg_shift;
};

static struct avs_tmon_trip avs_tmon_trips[] = {
	/* Trips when temperature is below threshold */
	[TMON_TRIP_TYPE_LOW] = {
		.enable_offs	= AVS_TMON_EN_TEMP_INT_SRCS,
		.enable_mask	= AVS_TMON_EN_TEMP_INT_SRCS_low,
		.reg_offs	= AVS_TMON_INT_THRESH,
		.reg_msk	= AVS_TMON_INT_THRESH_low_msk,
		.reg_shift	= AVS_TMON_INT_THRESH_low_shift,
	},
	/* Trips when temperature is above threshold */
	[TMON_TRIP_TYPE_HIGH] = {
		.enable_offs	= AVS_TMON_EN_TEMP_INT_SRCS,
		.enable_mask	= AVS_TMON_EN_TEMP_INT_SRCS_high,
		.reg_offs	= AVS_TMON_INT_THRESH,
		.reg_msk	= AVS_TMON_INT_THRESH_high_msk,
		.reg_shift	= AVS_TMON_INT_THRESH_high_shift,
	},
	/* Automatically resets chip when above threshold */
	[TMON_TRIP_TYPE_RESET] = {
		.enable_offs	= AVS_TMON_EN_OVERTEMP_RESET,
		.enable_mask	= AVS_TMON_EN_OVERTEMP_RESET_msk,
		.reg_offs	= AVS_TMON_RESET_THRESH,
		.reg_msk	= AVS_TMON_RESET_THRESH_msk,
		.reg_shift	= AVS_TMON_RESET_THRESH_shift,
	},
};

enum {
	/*
	 * Using the of-thermal framework
	 */
	AVS_TMON_OF_SENSOR_DRIVER,

	/*
	 * Old DT bindings didn't include the of-sensor framework, so support a
	 * full zone implementation temporarily
	 */
	AVS_TMON_ZONE_DRIVER,
};

struct brcmstb_thermal_priv {
	void __iomem *tmon_base;
	struct device *dev;
	struct thermal_zone_device *thermal;

	/*
	 * When .thermal is non-zero this member indicates whether
	 * this driver registered as a sensor (AVS_TMON_OF_SENSOR_DRIVER)
	 * or as a zone (AVS_TMON_ZONE_DRIVER)
	 */
	int regtype;
};

/* Convert a HW code to a temperature reading (millidegree celsius) */
static inline int avs_tmon_code_to_temp(u32 code)
{
	return (410040 - (int)((code & 0x3FF) * 487));
}

/*
 * Convert a temperature value (millidegree celsius) to a HW code
 *
 * @temp: temperature to convert
 * @low: if true, round toward the low side
 */
static inline u32 avs_tmon_temp_to_code(int temp, bool low)
{
	if (temp < -88161)
		return 0x3FF;	/* Maximum code value */

	if (temp >= 410040)
		return 0;	/* Minimum code value */

	if (low)
		return (u32)(DIV_ROUND_UP(410040 - temp, 487));
	else
		return (u32)((410040 - temp) / 487);
}

static int brcmstb_get_temp(void *data, long *temp)
{
	struct brcmstb_thermal_priv *priv = data;
	u32 val;
	long t;

	val = __raw_readl(priv->tmon_base + AVS_TMON_STATUS);

	if (!(val & AVS_TMON_STATUS_valid_msk)) {
		dev_err(priv->dev, "reading not valid\n");

		return -EIO;
	}

	val = (val & AVS_TMON_STATUS_data_msk) >> AVS_TMON_STATUS_data_shift;

	t = avs_tmon_code_to_temp(val);
	if (t < 0)
		*temp = 0;
	else
		*temp = t;

	return 0;
}

/*
 * Active when the of-thermal framework is not used
 */
static int brcmstb_get_zone_temp(struct thermal_zone_device *thermal,
				 unsigned long *temp)
{
	return brcmstb_get_temp(thermal->devdata, temp);
}

static void avs_tmon_trip_enable(struct brcmstb_thermal_priv *priv,
				 enum avs_tmon_trip_type type, int en)
{
	struct avs_tmon_trip *trip = &avs_tmon_trips[type];
	u32 val = __raw_readl(priv->tmon_base + trip->enable_offs);

	pr_debug("%s trip, type %d\n", en ? "enable" : "disable", type);

	if (en)
		val |= trip->enable_mask;
	else
		val &= ~trip->enable_mask;

	__raw_writel(val, priv->tmon_base + trip->enable_offs);
}

static int avs_tmon_get_trip_temp(struct brcmstb_thermal_priv *priv,
				  enum avs_tmon_trip_type type)
{
	struct avs_tmon_trip *trip = &avs_tmon_trips[type];
	u32 val = __raw_readl(priv->tmon_base + trip->reg_offs);

	val &= trip->reg_msk;
	val >>= trip->reg_shift;

	return avs_tmon_code_to_temp(val);
}

static void avs_tmon_set_trip_temp(struct brcmstb_thermal_priv *priv,
				   enum avs_tmon_trip_type type,
				   int temp)
{
	struct avs_tmon_trip *trip = &avs_tmon_trips[type];
	u32 val, orig;

	pr_debug("set temp %d to %d\n", type, temp);

	/* round toward low temp for the low interrupt */
	val = avs_tmon_temp_to_code(temp, type == TMON_TRIP_TYPE_LOW);

	/* TODO: Check for overflow? */
	val <<= trip->reg_shift;
	val &= trip->reg_msk;

	orig = __raw_readl(priv->tmon_base + trip->reg_offs);
	orig &= ~trip->reg_msk;
	orig |= val;
	__raw_writel(orig, priv->tmon_base + trip->reg_offs);
}

static int avs_tmon_get_intr_temp(struct brcmstb_thermal_priv *priv)
{
	u32 val;

	val = __raw_readl(priv->tmon_base + AVS_TMON_TEMP_INT_CODE);
	return avs_tmon_code_to_temp(val);
}

static irqreturn_t brcmstb_tmon_irq_thread(int irq, void *data)
{
	struct brcmstb_thermal_priv *priv = data;
	int low, high, intr;

	low = avs_tmon_get_trip_temp(priv, TMON_TRIP_TYPE_LOW);
	high = avs_tmon_get_trip_temp(priv, TMON_TRIP_TYPE_HIGH);
	intr = avs_tmon_get_intr_temp(priv);

	dev_dbg(priv->dev, "low/intr/high: %d/%d/%d\n",
			low, intr, high);

	/* Disable high-temp until next threshold shift */
	if (intr >= high)
		avs_tmon_trip_enable(priv, TMON_TRIP_TYPE_HIGH, 0);
	/* Disable low-temp until next threshold shift */
	if (intr <= low)
		avs_tmon_trip_enable(priv, TMON_TRIP_TYPE_LOW, 0);

	/*
	 * Notify using the interrupt temperature, in case the temperature
	 * changes before it can next be read out
	 */
	thermal_zone_device_update_temp(priv->thermal, intr);

	return IRQ_HANDLED;
}

static int brcmstb_set_trips(void *data, unsigned long low, unsigned long high)
{
	struct brcmstb_thermal_priv *priv = data;

	pr_debug("set trips %lu <--> %lu\n", low, high);

	if (low) {
		if (low > INT_MAX) {
			low = INT_MAX;
		}
		avs_tmon_set_trip_temp(priv, TMON_TRIP_TYPE_LOW, (int)low);
		avs_tmon_trip_enable(priv, TMON_TRIP_TYPE_LOW, 1);
	} else {
		avs_tmon_trip_enable(priv, TMON_TRIP_TYPE_LOW, 0);
	}

	if (high < ULONG_MAX) {
		if (high > INT_MAX) {
			high = INT_MAX;
		}
		avs_tmon_set_trip_temp(priv, TMON_TRIP_TYPE_HIGH, (int)high);
		avs_tmon_trip_enable(priv, TMON_TRIP_TYPE_HIGH, 1);
	} else {
		avs_tmon_trip_enable(priv, TMON_TRIP_TYPE_HIGH, 0);
	}

	return 0;
}

static struct thermal_zone_device_ops zone_ops = {
	.get_temp	= brcmstb_get_zone_temp,
};

static struct thermal_zone_of_device_ops of_ops = {
	.get_temp	= brcmstb_get_temp,
	.set_trips	= brcmstb_set_trips,
};

static const struct of_device_id brcmstb_thermal_id_table[] = {
	{ .compatible = "brcm,avs-tmon" },
	{},
};
MODULE_DEVICE_TABLE(of, brcmstb_thermal_id_table);

/*
 * Deprecated: we should be using the of-thermal framework, rather than
 * registering our own thermal zone. This is provided as a temporary help, in
 * case of an old device tree which does not include the thermal-zones node and
 * sensor phandles.
 */
static int brcmstb_thermal_zone_probe(struct platform_device *pdev,
				      struct brcmstb_thermal_priv *priv)
{
	struct thermal_zone_device *thermal;

	thermal = thermal_zone_device_register("avs_tmon", 0, 0, priv,
					       &zone_ops, NULL, 0, 0);
	if (IS_ERR(thermal)) {
		dev_err(&pdev->dev, "failed to register zone device\n");
		return PTR_ERR(thermal);
	}

	priv->regtype = AVS_TMON_ZONE_DRIVER;
	priv->thermal = thermal;
	dev_info(&pdev->dev, "registered AVS TMON zone driver\n");

	return 0;
}

static int brcmstb_thermal_probe(struct platform_device *pdev)
{
	struct thermal_zone_device *thermal;
	struct brcmstb_thermal_priv *priv;
	struct resource *res;
	int irq, ret;

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	priv->tmon_base = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(priv->tmon_base))
		return PTR_ERR(priv->tmon_base);

	priv->dev = &pdev->dev;
	platform_set_drvdata(pdev, priv);

	thermal = thermal_zone_of_sensor_register(&pdev->dev, 0, priv, &of_ops);
	if (IS_ERR(thermal))
		/*
		 * Fall back to registering our own zone, for legacy purposes
		 */
		return brcmstb_thermal_zone_probe(pdev, priv);

	priv->regtype = AVS_TMON_OF_SENSOR_DRIVER;
	priv->thermal = thermal;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "could not get IRQ\n");
		ret = irq;
		goto err;
	}
	ret = devm_request_threaded_irq(&pdev->dev, irq, NULL,
					brcmstb_tmon_irq_thread, IRQF_ONESHOT,
					DRV_NAME, priv);
	if (ret < 0) {
		dev_err(&pdev->dev, "could not request IRQ: %d\n", ret);
		goto err;
	}

	dev_info(&pdev->dev, "registered AVS TMON of-sensor driver\n");

	return 0;

err:
	thermal_zone_of_sensor_unregister(&pdev->dev, priv->thermal);
	return ret;
}

static int brcmstb_thermal_exit(struct platform_device *pdev)
{
	struct brcmstb_thermal_priv *priv = platform_get_drvdata(pdev);
	struct thermal_zone_device *thermal = priv->thermal;

	if (thermal) {
		if (priv->regtype == AVS_TMON_ZONE_DRIVER)
			thermal_zone_device_unregister(thermal);
		else
			thermal_zone_of_sensor_unregister(&pdev->dev,
							  priv->thermal);
	}

	return 0;
}

static struct platform_driver brcmstb_thermal_driver = {
	.probe = brcmstb_thermal_probe,
	.remove = brcmstb_thermal_exit,
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = brcmstb_thermal_id_table,
	},
};
module_platform_driver(brcmstb_thermal_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Brian Norris");
MODULE_DESCRIPTION("Broadcom STB AVS TMON thermal driver");
