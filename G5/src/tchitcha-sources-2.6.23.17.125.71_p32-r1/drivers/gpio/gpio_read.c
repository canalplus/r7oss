#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>

struct gpio_read_object
{
	struct kobject kobj;
	int current_gpio;
};

static void gpio_read_release(struct kobject *k)
{
	struct gpio_read_object *obj = container_of(k, struct gpio_read_object, kobj);
	kfree(obj);
	return;
}

static ssize_t gpio_read_show (struct kobject *k, struct attribute *attr, char *buf)
{
	struct gpio_read_object *obj;

	if (!strcmp(attr->name, "help")) {
		return sprintf(buf, "Write a GPIO number in \"gpio_read\" and get its value from it\n");
	}

	obj = container_of(k, struct gpio_read_object, kobj);
	return sprintf(buf, "%d\n", gpio_get_value(obj->current_gpio));
}

static ssize_t gpio_read_store (struct kobject *k, struct attribute *attr, const char *buf, size_t size)
{
	struct gpio_read_object *obj;

	if (!strcmp(attr->name, "help")) {
		return -1;
	}

	obj = container_of(k, struct gpio_read_object, kobj);
	sscanf(buf, "%d", &obj->current_gpio);
	return sizeof(obj->current_gpio);
}

static struct sysfs_ops gpio_read_ops =
{
	.show = gpio_read_show,
	.store = gpio_read_store,
};

static struct attribute gpio_read_attr =
{
	.name = "gpio_read",
	.mode = 0600,
};

static struct attribute help_attr =
{
	.name = "help",
	.mode = 0400,
};

static struct attribute *gpio_read_attrs [] = { &gpio_read_attr, &help_attr, NULL };

static struct kobj_type gpio_read_ktype =
{
	.release = gpio_read_release,
	.sysfs_ops = &gpio_read_ops,
	.default_attrs = gpio_read_attrs,
};

static struct gpio_read_object *gpio_read_create(int value)
{
	struct gpio_read_object *new_obj = kzalloc(sizeof(struct gpio_read_object), GFP_KERNEL);

	if (!new_obj)
		return NULL;

	new_obj->current_gpio = value;

	new_obj->kobj.ktype = &gpio_read_ktype;
	kobject_set_name(&new_obj->kobj, "gpio");
	kobject_init(&new_obj->kobj);
	kobject_add(&new_obj->kobj);

	return new_obj;
}

static struct gpio_read_object *obj;

static int gpio_read_driver_init(void)
{
	obj = gpio_read_create(0);

	if (!obj)
		return -ENOMEM;
	else
		return 0;
}

static void gpio_read_driver_exit(void)
{
	kobject_unregister(&obj->kobj);
}

module_init(gpio_read_driver_init);
module_exit(gpio_read_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Thomas Griozel <tgriozel@wyplay.com>");
MODULE_DESCRIPTION("GPIO value read driver");
