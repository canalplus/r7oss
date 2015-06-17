#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 5, 0) && (defined(CONFIG_PCMCIA) || defined(CONFIG_PCMCIA_MODULE))

#include <pcmcia/cs_types.h>
#include <pcmcia/cs.h>
#include <pcmcia/cistpl.h>
#include <pcmcia/ds.h>
#include <pcmcia/version.h>

struct cs_device_driver {
	const char *name;
};

struct pcmcia_driver {
	int                     use_count;
	dev_link_t              *(*attach)(void);
	void                    (*detach)(dev_link_t *);
	struct module           *owner;
	struct cs_device_driver	drv;
};

/* driver registration */
static inline int snd_compat_pcmcia_register_driver(struct pcmcia_driver *driver)
{
	servinfo_t serv;

	CardServices(GetCardServicesInfo, &serv);
	if (serv.Revision != CS_RELEASE_CODE) {
		printk(KERN_WARNING "%s: Card Services release does not match (%x != %x)!\n", driver->drv.name, serv.Revision, CS_RELEASE_CODE);
		return -EIO;
	}
	register_pccard_driver((dev_info_t *)driver->drv.name, driver->attach, driver->detach);
	return 0;
}

static inline void snd_compat_pcmcia_unregister_driver(struct pcmcia_driver *driver)
{
	unregister_pccard_driver((dev_info_t *)driver->drv.name);
}

#define pcmcia_register_driver(driver) snd_compat_pcmcia_register_driver(driver)
#define pcmcia_unregister_driver(driver) snd_compat_pcmcia_unregister_driver(driver)

#endif /* 2.5.0+ */
