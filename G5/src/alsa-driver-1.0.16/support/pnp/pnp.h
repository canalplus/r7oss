/*
 * Linux Plug and Play Support
 * Copyright by Adam Belay <ambx1@neo.rr.com>
 *
 * Reduced and backported for compatibility reasons for 2.2 and
 * 2.4 kernels by Jaroslav Kysela <perex@perex.cz>
 *
 */

#ifndef _LINUX_PNP_H
#define _LINUX_PNP_H

#ifdef __KERNEL__

#include <linux/isapnp.h>
#include <linux/list.h>

#define PNP_MAX_PORT		8
#define PNP_MAX_MEM		4
#define PNP_MAX_IRQ		2
#define PNP_MAX_DMA		2
#define PNP_MAX_DEVICES		8
#define PNP_ID_LEN		8

#ifndef ISAPNP_ALSA_LOCAL
#define isapnp_dev pci_dev
#define isapnp_card pci_bus
#endif

struct pnp_card {
	struct isapnp_card p;
};

struct pnp_dev {
	struct isapnp_dev p;
	void * driver_data;
	struct pm_dev *pm;
};

static inline void *pnp_get_drvdata (struct pnp_dev *dev)
{
	return dev->driver_data;
}

static inline void pnp_set_drvdata (struct pnp_dev *dev, void *data)
{
	dev->driver_data = data;
}

/*
 * Resource Management
 */

/* Use these instead of directly reading pnp_dev to get resource information */
#define pnp_port_start(dev,bar)   ((dev)->p.resource[(bar)].start)
#define pnp_port_end(dev,bar)     ((dev)->p.resource[(bar)].end)
#define pnp_port_flags(dev,bar)   ((dev)->p.resource[(bar)].flags)
#define pnp_port_valid(dev,bar)   (pnp_port_flags((dev),(bar)) & IORESOURCE_IO)
#define pnp_port_len(dev,bar) \
	((pnp_port_start((dev),(bar)) == 0 &&	\
	  pnp_port_end((dev),(bar)) ==		\
	  pnp_port_start((dev),(bar))) ? 0 :	\
	  					\
	 (pnp_port_end((dev),(bar)) -		\
	  pnp_port_start((dev),(bar)) + 1))

#define pnp_mem_start(dev,bar)   ((dev)->p.resource[(bar)+8].start)
#define pnp_mem_end(dev,bar)     ((dev)->p.resource[(bar)+8].end)
#define pnp_mem_flags(dev,bar)   ((dev)->p.resource[(bar)+8].flags)
#define pnp_mem_valid(dev,bar)   (pnp_mem_flags((dev),(bar)) & IORESOURCE_MEM)
#define pnp_mem_len(dev,bar) \
	((pnp_mem_start((dev),(bar)) == 0 &&	\
	  pnp_mem_end((dev),(bar)) ==		\
	  pnp_mem_start((dev),(bar))) ? 0 :	\
	  					\
	 (pnp_mem_end((dev),(bar)) -		\
	  pnp_mem_start((dev),(bar)) + 1))

#define pnp_irq(dev,bar)	 ((dev)->p.irq_resource[(bar)].start)
#define pnp_irq_flags(dev,bar)	 ((dev)->p.irq_resource[(bar)].flags)
#define pnp_irq_valid(dev,bar)   (pnp_irq_flags((dev),(bar)) & IORESOURCE_IRQ)

#define pnp_dma(dev,bar)	 ((dev)->p.dma_resource[(bar)].start)
#define pnp_dma_flags(dev,bar)	 ((dev)->p.dma_resource[(bar)].flags)
#define pnp_dma_valid(dev,bar)   (pnp_dma_flags((dev),(bar)) & IORESOURCE_DMA)

#define PNP_PORT_FLAG_16BITADDR	ISAPNP_PORT_FLAG_16BITADDR
#define PNP_PORT_FLAG_FIXED	ISAPNP_PORT_FLAG_FIXED

#define PNP_RES_PRIORITY_PREFERRED	ISAPNP_RES_PRIORITY_PREFERRED
#define PNP_RES_PRIORITY_ACCEPTABLE	ISAPNP_RES_PRIORITY_ACCEPTABLE
#define PNP_RES_PRIORITY_FUNCTIONAL	ISAPNP_RES_PRIORITY_FUNCTIONAL
#define PNP_RES_PRIORITY_INVALID	ISAPNP_RES_PRIORITY_INVALID

struct pnp_resource_table {
	struct resource port_resource[PNP_MAX_PORT];
	struct resource mem_resource[PNP_MAX_MEM];
	struct resource dma_resource[PNP_MAX_DMA];
	struct resource irq_resource[PNP_MAX_IRQ];
};


/*
 * Device Managemnt
 */

struct pnp_card_link {
	struct pnp_card * card;
	struct pnp_card_driver * driver;
	void * driver_data;
	struct pm_dev *pm;
};

static inline void *pnp_get_card_drvdata (struct pnp_card_link *pcard)
{
	return pcard->driver_data;
}

static inline void pnp_set_card_drvdata (struct pnp_card_link *pcard, void *data)
{
	pcard->driver_data = data;
}

struct pnp_fixup {
	char id[7];
	void (*quirk_function)(struct pnp_dev *dev);	/* fixup function */
};

/* config modes */
#define PNP_CONFIG_AUTO		0x0001	/* Use the Resource Configuration Engine to determine resource settings */
#define PNP_CONFIG_MANUAL	0x0002	/* the config has been manually specified */
#define PNP_CONFIG_FORCE	0x0004	/* disables validity checking */
#define PNP_CONFIG_INVALID	0x0008	/* If this flag is set, the pnp layer will refuse to activate the device */

/* capabilities */
#define PNP_READ		0x0001
#define PNP_WRITE		0x0002
#define PNP_DISABLE		0x0004
#define PNP_CONFIGURABLE	0x0008
#define PNP_REMOVABLE		0x0010

#define pnp_can_read(dev)	1
#define pnp_can_write(dev)	1
#define pnp_can_disable(dev)	0
#define pnp_can_configure(dev)	1

#define pnp_device_is_isapnp(dev) 1

#define isapnp_card_number(dev) (dev->p.bus->number)
#define isapnp_csn_number(dev)  (dev->p.devfn)

/*
 * Driver Management
 */

struct pnp_id {
	char id[PNP_ID_LEN];
	struct pnp_id * next;
};

struct pnp_device_id {
	char id[PNP_ID_LEN];
	unsigned long driver_data;	/* data private to the driver */
};

struct pnp_card_device_id {
	char id[PNP_ID_LEN];
	unsigned long driver_data;	/* data private to the driver */
	struct {
		char id[PNP_ID_LEN];
	} devs[PNP_MAX_DEVICES];	/* logical devices */
};

struct pnp_driver {
	char * name;
	const struct pnp_device_id *id_table;
	unsigned int flags;
	int  (*probe)  (struct pnp_dev *dev, const struct pnp_device_id *dev_id);
	void (*remove) (struct pnp_dev *dev);
	int (*suspend)(struct pnp_dev *dev, pm_message_t state);
	int (*resume)(struct pnp_dev *dev);
};

#define	to_pnp_driver(drv) container_of(drv, struct pnp_driver, driver)

struct pnp_card_driver {
	struct list_head global_list;
	char * name;
	const struct pnp_card_device_id *id_table;
	unsigned int flags;
	int  (*probe)  (struct pnp_card_link *card, const struct pnp_card_device_id *card_id);
	void (*remove) (struct pnp_card_link *card);
	int (*suspend)(struct pnp_card_link *card, pm_message_t state);
	int (*resume)(struct pnp_card_link *card);
	struct pnp_driver link;
};

#define	to_pnp_card_driver(drv) container_of(drv, struct pnp_card_driver, link)

/* pnp driver flags */
#define PNP_DRIVER_RES_DO_NOT_CHANGE	0x0001	/* do not change the state of the device */
#define PNP_DRIVER_RES_DISABLE		0x0003	/* ensure the device is disabled */

#define pnp_resource_change(resource, start, size) isapnp_resource_change(resource, start, size)

#if defined(CONFIG_PNP)

struct pnp_dev * pnp_request_card_device(struct pnp_card_link *clink, const char * id, struct pnp_dev * from);
void pnp_release_card_device(struct pnp_dev * dev);
int pnp_register_card_driver(struct pnp_card_driver * drv);
void pnp_unregister_card_driver(struct pnp_card_driver * drv);
int pnp_register_driver(struct pnp_driver *drv);
void pnp_unregister_driver(struct pnp_driver *drv);
void pnp_init_resource_table(struct pnp_resource_table *table);
int pnp_manual_config_dev(struct pnp_dev *dev, struct pnp_resource_table *res, int mode);
int pnp_activate_dev(struct pnp_dev *dev);
static inline int pnp_is_active(struct pnp_dev *dev) { return dev->p.active; }
int pnp_disable_dev(struct pnp_dev *dev);

#else

static inline struct pnp_dev * pnp_request_card_device(struct pnp_card_link *clink, const char * id, struct pnp_dev * from) { return NULL; }
static inline void pnp_release_card_device(struct pnp_dev * dev) { ; }
static inline int pnp_register_card_driver(struct pnp_card_driver * drv) { return 0; }
static inline void pnp_unregister_card_driver(struct pnp_card_driver * drv) { ; }
static inline int pnp_register_driver(struct pnp_driver *drv) { return 0; }
static inline void pnp_unregister_driver(struct pnp_driver *drv) { ; }
static inline void pnp_init_resource_table(struct pnp_resource_table *table) { ; }
static inline int pnp_manual_config_dev(struct pnp_dev *dev, struct pnp_resource_table *res, int mode) { return -ENODEV; }
static inline int pnp_activate_dev(struct pnp_dev *dev) { return -ENODEV; }
static inline int pnp_is_active(struct pnp_dev * dev) { return -ENODEV; }
static inline int pnp_disable_dev(struct pnp_dev *dev) { return -ENODEV; }

#endif


#endif /* __KERNEL__ */

#endif /* _LINUX_PNP_H */
