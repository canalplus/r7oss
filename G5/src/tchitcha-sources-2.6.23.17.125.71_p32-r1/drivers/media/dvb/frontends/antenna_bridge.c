#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>

#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>

extern int set_ant_bridge_mode(int activate_bridge);

struct ant_br_object
{
	struct kobject kobj;
	int bridge_status;
};

static void ant_br_release(struct kobject *k)
{
	struct ant_br_object *br_obj = container_of(k, struct ant_br_object, kobj);
	kfree(br_obj);
	return;
}

static ssize_t ant_br_show (struct kobject *k, struct attribute *attr, char *buf)
{
	struct ant_br_object *br_obj = container_of(k, struct ant_br_object, kobj);
	return sprintf(buf, "%d\n", br_obj->bridge_status);
}

static ssize_t ant_br_store (struct kobject *k, struct attribute *attr, const char *buf, size_t size)
{
	struct ant_br_object *br_obj = container_of(k, struct ant_br_object, kobj);
	int activation_req = -1, err = 0;

	/* 0 is considered as "deactivate" and 1 as "activate" */
	/* everything else is not considered as a valid input */
	sscanf(buf, "%d", &activation_req);

	if ((activation_req != 0 && activation_req != 1) || (activation_req == br_obj->bridge_status))
		return size;

	err = set_ant_bridge_mode(activation_req);

	if (!err)
		br_obj->bridge_status = activation_req;
	else
		printk(KERN_WARNING "Antenna bridge %sactivation failed\n", activation_req ? "" : "de");

	return size;
}

static struct sysfs_ops ant_br_ops =
{
	.show = ant_br_show,
	.store = ant_br_store,
};

static struct attribute ant_br_attr =
{
	.name = "bridge_status",
	.mode = 0600,
};

static struct attribute *ant_br_attrs [] = { &ant_br_attr, NULL };

static struct kobj_type ant_br_ktype =
{
	.release = ant_br_release,
	.sysfs_ops = &ant_br_ops,
	.default_attrs = ant_br_attrs,
};

static struct ant_br_object *ant_br_create(int value)
{
	struct ant_br_object *br_new_obj = kzalloc(sizeof(struct ant_br_object), GFP_KERNEL);

	if (!br_new_obj)
		return NULL;

	br_new_obj->bridge_status = value;

	br_new_obj->kobj.ktype = &ant_br_ktype;
	kobject_set_name(&br_new_obj->kobj, "bridge");
	kobject_init(&br_new_obj->kobj);
	kobject_add(&br_new_obj->kobj);

	return br_new_obj;
}

static struct ant_br_object *br_obj;

static int ant_br_driver_init(void)
{
	br_obj = ant_br_create(0);

	if (!br_obj)
		return -ENOMEM;
	else
		return 0;
}

static void ant_br_driver_exit(void)
{
	kobject_unregister(&br_obj->kobj);
}

module_init(ant_br_driver_init);
module_exit(ant_br_driver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Thomas Griozel <tgriozel@wyplay.com>");
MODULE_DESCRIPTION("Antenna bridge driver");
