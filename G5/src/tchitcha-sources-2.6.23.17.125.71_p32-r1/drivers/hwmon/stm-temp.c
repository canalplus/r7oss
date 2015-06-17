/*
 * Copyright (C) 2009 STMicroelectronics Limited
 * Author: Pawel Moll <pawel.moll@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/sysfs.h>
#include <linux/err.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/soc.h>

struct stm_temp_sensor {
	struct class_device *class_dev;

	struct plat_stm_temp_data *plat_data;

	struct sysconf_field *pdn;
	struct sysconf_field *dcorrect;
	struct sysconf_field *overflow;
	struct sysconf_field *data;

	unsigned long (*custom_get_data)(void *priv);
};

static ssize_t stm_temp_show_temp(struct device *dev,
		struct device_attribute *devattr, char *buf)
{
	ssize_t result;
	struct stm_temp_sensor *sensor = dev_get_drvdata(dev);
	unsigned long overflow, data;


	overflow = sysconf_read(sensor->overflow);
	if (sensor->plat_data->custom_get_data)
		data = sensor->plat_data->custom_get_data(
				sensor->plat_data->custom_priv);
	else
		data = sysconf_read(sensor->data);
	overflow |= sysconf_read(sensor->overflow);

	if (!overflow)
		result = sprintf(buf, "%lu\n", (data + 20) * 1000);
	else
		result = sprintf(buf, "!\n");

	return result;
}

static ssize_t stm_temp_show_name(struct device *dev,
		struct device_attribute *devattr, char *buf)
{
	struct stm_temp_sensor *sensor = dev_get_drvdata(dev);

	return sprintf(buf, "%s\n", sensor->plat_data->name);
}

static DEVICE_ATTR(temp1_input, S_IRUGO, stm_temp_show_temp, NULL);
static DEVICE_ATTR(temp1_label, S_IRUGO, stm_temp_show_name, NULL);

static int __devinit stm_temp_probe(struct platform_device *pdev)
{
	struct stm_temp_sensor *sensor = platform_get_drvdata(pdev);
	struct plat_stm_temp_data *plat_data = pdev->dev.platform_data;
	int err;

	sensor = kzalloc(sizeof(*sensor), GFP_KERNEL);
	if (!sensor) {
		dev_err(&pdev->dev, "Out of memory!\n");
		err = -ENOMEM;
		goto error_kzalloc;
	}

	sensor->plat_data = plat_data;

	err = -EBUSY;

	sensor->pdn = sysconf_claim(plat_data->pdn.group,
			plat_data->pdn.num, plat_data->pdn.lsb,
			plat_data->pdn.msb, pdev->dev.bus_id);
	if (!sensor->pdn) {
		dev_err(&pdev->dev, "Can't claim PDN sysconf bit!\n");
		goto error_pdn;
	}

	if (!plat_data->custom_set_dcorrect) {
		sensor->dcorrect = sysconf_claim(plat_data->dcorrect.group,
				plat_data->dcorrect.num,
				plat_data->dcorrect.lsb,
				plat_data->dcorrect.msb, pdev->dev.bus_id);
		if (!sensor->dcorrect) {
			dev_err(&pdev->dev, "Can't claim DCORRECT sysconf "
					"bits!\n");
			goto error_dcorrect;
		}
	}

	sensor->overflow = sysconf_claim(plat_data->overflow.group,
			plat_data->overflow.num, plat_data->overflow.lsb,
			plat_data->overflow.msb, pdev->dev.bus_id);
	if (!sensor->overflow) {
		dev_err(&pdev->dev, "Can't claim OVERFLOW sysconf bit!\n");
		goto error_overflow;
	}

	if (!plat_data->custom_get_data) {
		sensor->data = sysconf_claim(plat_data->data.group,
				plat_data->data.num, plat_data->data.lsb,
				plat_data->data.msb, pdev->dev.bus_id);
		if (!sensor->data) {
			dev_err(&pdev->dev, "Can't claim DATA sysconf bits!\n");
			goto error_data;
		}
	}

	sensor->class_dev = hwmon_device_register(&pdev->dev);
	if (IS_ERR(sensor->class_dev)) {
		err = PTR_ERR(sensor->class_dev);
		dev_err(&pdev->dev, "Failed to register hwmon device!\n");
		goto error_class_dev;
	}

	if (device_create_file(&pdev->dev, &dev_attr_temp1_input) != 0) {
		dev_err(&pdev->dev, "Failed to create temp1_input file!\n");
		goto error_temp1_input;
	}
	if (plat_data->name)
		if (device_create_file(&pdev->dev, &dev_attr_temp1_label)
				!= 0) {
			dev_err(&pdev->dev, "Failed to create temp1_label "
					"file!\n");
			goto error_temp1_label;
		}

	if (plat_data->custom_set_dcorrect) {
		plat_data->custom_set_dcorrect(plat_data->custom_priv);
	} else {
		if (!plat_data->calibrated)
			plat_data->calibration_value = 16;

		sysconf_write(sensor->dcorrect, plat_data->calibration_value);
	}

	sysconf_write(sensor->pdn, 1);

	platform_set_drvdata(pdev, sensor);

	return 0;

error_temp1_label:
	device_remove_file(&pdev->dev, &dev_attr_temp1_input);
error_temp1_input:
	hwmon_device_unregister(sensor->class_dev);
error_class_dev:
	if (sensor->data)
		sysconf_release(sensor->data);
error_data:
	sysconf_release(sensor->overflow);
error_overflow:
	if (sensor->dcorrect)
		sysconf_release(sensor->dcorrect);
error_dcorrect:
	sysconf_release(sensor->pdn);
error_pdn:
	kfree(sensor);
error_kzalloc:
	return err;
}

static int __devexit stm_temp_remove(struct platform_device *pdev)
{
	struct stm_temp_sensor *sensor = platform_get_drvdata(pdev);

	hwmon_device_unregister(sensor->class_dev);

	sysconf_write(sensor->pdn, 0);

	sysconf_release(sensor->pdn);
	if (sensor->dcorrect)
		sysconf_release(sensor->dcorrect);
	sysconf_release(sensor->overflow);
	if (sensor->data)
		sysconf_release(sensor->data);

	kfree(sensor);

	return 0;
}

static struct platform_driver stm_temp_driver = {
	.driver.name	= "stm-temp",
	.probe		= stm_temp_probe,
	.remove		= stm_temp_remove,
};

static int __init stm_temp_init(void)
{
	return platform_driver_register(&stm_temp_driver);
}

static void __exit stm_temp_exit(void)
{
	platform_driver_unregister(&stm_temp_driver);
}

module_init(stm_temp_init);
module_exit(stm_temp_exit);

MODULE_AUTHOR("Pawel Moll <pawel.moll@st.com>");
MODULE_DESCRIPTION("STMicroelectronics SOC internal temperature sensor driver");
MODULE_LICENSE("GPL");
