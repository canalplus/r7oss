#ifndef _DEVICE_COMPAT_H
#define _DEVICE_COMPAT_H

struct device {
	void *private_data;
	struct device_driver *driver;
	struct pm_dev *pm_dev;
	char	bus_id[20];
};

struct bus_type {
};

struct device_driver {
	const char *name;
	struct bus_type *bus;
	struct module *owner;
	int (*probe)(struct device *dev);
	int (*remove)(struct device *dev);
	void (*shutdown)(struct device *dev);
	int (*suspend)(struct device *dev, pm_message_t state);
	int (*resume)(struct device *dev);
	struct list_head list;
	struct list_head device_list;
};

struct device_attribute {
};

int snd_compat_driver_register(struct device_driver *driver);
void snd_compat_driver_unregister(struct device_driver *driver);

#define driver_register		snd_compat_driver_register
#define driver_unregister	snd_compat_driver_unregister
#define dev_set_drvdata(dev,ptr)	((dev)->private_data = (ptr))
#define dev_get_drvdata(dev)	(dev)->private_data

#endif
