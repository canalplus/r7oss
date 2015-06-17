#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/init.h> /* Initilisation support */
#include <linux/kernel.h> /* Kernel support */
#include <linux/clk.h>

#ifndef CONFIG_ARCH_STI
#include <linux/stm/pad.h>
#include <linux/stm/platform.h>
#endif

#include "stm_scr.h"
#include "scr_data_parsing.h"
#include "scr_internal.h"
#include "scr_ca.h"
MODULE_AUTHOR("");
MODULE_DESCRIPTION("STMicroelectronics SCR driver");
MODULE_LICENSE("GPL");

#define SCR_CA_DEVICE_NAME "stm_scrca"

static int __init scr_ca_init_module(void)
{
	int err = 0, i = 0;
	uint32_t minor;
	struct scr_platform_data_s *scr_data = get_platform_data();

	for (i = 0 ; i < STM_SCR_MAX; i++) {
		if (!scr_data[i].base_address)
			continue;

		minor = scr_data[i].id + STM_SCR_MAX;
		cdev_init(&scr_data[i].ca_device_data.cdev, scr_data[i].fops);
		scr_data[i].ca_device_data.cdev.owner = THIS_MODULE;
		scr_data[i].ca_device_data.devt = MKDEV(scr_data[i].scr_major,
						minor);
		kobject_set_name(&scr_data[i].ca_device_data.cdev.kobj,
				"scr_ca%d", scr_data->id);
		err = cdev_add(&scr_data[i].ca_device_data.cdev,
				scr_data[i].ca_device_data.devt, 1);
		if (err) {
			ca_error_print(
			"character device add failed");
			goto add_fail;
		}
		scr_data[i].ca_device_data.cdev_class_dev =
			device_create(scr_data[i].scr_class, NULL,
					scr_data[i].ca_device_data.devt,
					&scr_data[i],
					"%s%d", SCR_CA_DEVICE_NAME, i);

		if (IS_ERR(scr_data[i].ca_device_data.cdev_class_dev)) {
			err = PTR_ERR(
				scr_data[i].ca_device_data.cdev_class_dev);
			ca_error_print("device create add ca failed %d", i);
			cdev_del(&scr_data[i].ca_device_data.cdev);
			return err;
		}
	}
	scr_ca_init();
	printk(KERN_ALERT "Load module scr_ca by %s (pid %i)\n",
		current->comm, current->pid);
	return 0;

add_fail:
	return err;
}
static void __exit scr_ca_exit_module(void)
{
	int i = 0;
	struct cdev *cdev_p = NULL;
	struct scr_platform_data_s *scr_data = get_platform_data();

	scr_ca_term();

	for (i = 0 ; i < STM_SCR_MAX; i++) {
		if ((scr_data[i].ca_device_data.cdev_class_dev) &&\
			(scr_data[i].scr_class)) {
			device_destroy(scr_data[i].scr_class,
					scr_data[i].ca_device_data.devt);
		cdev_p = &scr_data[i].ca_device_data.cdev;

		if (cdev_p != NULL)
			cdev_del(cdev_p);
		cdev_p = NULL;
		}
	}
	printk(KERN_ALERT "Unload module scr_ca by %s (pid %i)\n",
		current->comm, current->pid);
}

module_init(scr_ca_init_module);
module_exit(scr_ca_exit_module);
