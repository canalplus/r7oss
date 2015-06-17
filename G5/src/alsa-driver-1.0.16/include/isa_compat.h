/*
 * ISA bus.
 */

#ifndef __LINUX_ISA_H
#define __LINUX_ISA_H

#include <linux/device.h>

struct isa_driver {
	int (*match)(struct device *, unsigned int);
	int (*probe)(struct device *, unsigned int);
	int (*remove)(struct device *, unsigned int);
	void (*shutdown)(struct device *, unsigned int);
#ifdef CONFIG_PM
	int (*suspend)(struct device *, unsigned int, pm_message_t);
	int (*resume)(struct device *, unsigned int);
#endif
	struct device_driver driver;
	struct device *devices;
};

#define to_isa_driver(x) container_of((x), struct isa_driver, driver)

int snd_isa_register_driver(struct isa_driver *, unsigned int);
void snd_isa_unregister_driver(struct isa_driver *);

#define isa_register_driver	snd_isa_register_driver
#define isa_unregister_driver	snd_isa_unregister_driver

#define HAVE_DUMMY_SND_ISA_WRAPPER	1

#endif /* __LINUX_ISA_H */
