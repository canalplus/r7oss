/*
 * PCI-compatible layer for 2.2 kernels
 */

#ifdef CONFIG_PCI

static LIST_HEAD(pci_drivers);

struct pci_driver_mapping {
	struct pci_dev *dev;
	struct pci_driver *drv;
	unsigned long dma_mask;
	void *driver_data;
	u32 saved_config[16];
};

#define PCI_MAX_MAPPINGS 64
static struct pci_driver_mapping drvmap [PCI_MAX_MAPPINGS] = { { NULL, } , };


static struct pci_driver_mapping *get_pci_driver_mapping(struct pci_dev *dev)
{
	int i;
	
	for (i = 0; i < PCI_MAX_MAPPINGS; i++)
		if (drvmap[i].dev == dev)
			return &drvmap[i];
	return NULL;
}

struct pci_driver *snd_pci_compat_get_pci_driver(struct pci_dev *dev)
{
	struct pci_driver_mapping *map = get_pci_driver_mapping(dev);
	if (map)
		return map->drv;
	return NULL;
}

void * snd_pci_compat_get_driver_data (struct pci_dev *dev)
{
	struct pci_driver_mapping *map = get_pci_driver_mapping(dev);
	if (map)
		return map->driver_data;
	return NULL;
}


void snd_pci_compat_set_driver_data (struct pci_dev *dev, void *driver_data)
{
	struct pci_driver_mapping *map = get_pci_driver_mapping(dev);
	if (map)
		map->driver_data = driver_data;
}


unsigned long snd_pci_compat_get_dma_mask (struct pci_dev *dev)
{
	if (dev) {
		struct pci_driver_mapping *map = get_pci_driver_mapping(dev);
		if (map)
			return map->dma_mask;
		return 0;
	} else
		return 0xffffff; /* ISA - 16MB */
}


int snd_pci_compat_set_dma_mask (struct pci_dev *dev, unsigned long mask)
{
	if (dev) {
		struct pci_driver_mapping *map = get_pci_driver_mapping(dev);
		if (map)
			map->dma_mask = mask;
	}
	return 0;
}


const struct pci_device_id * snd_pci_compat_match_device(const struct pci_device_id *ids, struct pci_dev *dev)
{
	u16 subsystem_vendor, subsystem_device;

	pci_read_config_word(dev, PCI_SUBSYSTEM_VENDOR_ID, &subsystem_vendor);
	pci_read_config_word(dev, PCI_SUBSYSTEM_ID, &subsystem_device);

	while (ids->vendor || ids->subvendor || ids->class_mask) {
		if ((ids->vendor == PCI_ANY_ID || ids->vendor == dev->vendor) &&
		    (ids->device == PCI_ANY_ID || ids->device == dev->device) &&
		    (ids->subvendor == PCI_ANY_ID || ids->subvendor == subsystem_vendor) &&
		    (ids->subdevice == PCI_ANY_ID || ids->subdevice == subsystem_device) &&
		    !((ids->class ^ dev->class) & ids->class_mask))
			return ids;
		ids++;
	}
	return NULL;
}

static int snd_pci_announce_device(struct pci_driver *drv, struct pci_dev *dev)
{
        int i;
	const struct pci_device_id *id;

	if (drv->id_table) {
		id = snd_pci_compat_match_device(drv->id_table, dev);
		if (!id)
			return 0;
	} else {
		id = NULL;
	}
        for (i = 0; i < PCI_MAX_MAPPINGS; i++) {
	        if (drvmap[i].dev == NULL) {
	                drvmap[i].dev = dev;
	                drvmap[i].drv = drv;
			drvmap[i].dma_mask = ~0UL;
	                break;
	        }
        }
        if (i >= PCI_MAX_MAPPINGS)
		return 0;
	if (drv->probe(dev, id) < 0) {
                 drvmap[i].dev = NULL;
	         return 0;
	}
	return 1;
}

int snd_pci_compat_register_driver(struct pci_driver *drv)
{
	struct pci_dev *dev;
	int count = 0;

	list_add_tail(&drv->node, &pci_drivers);
	pci_for_each_dev(dev) {
		struct pci_driver_mapping *map = get_pci_driver_mapping(dev);
		if (! map)
			count += snd_pci_announce_device(drv, dev);
	}
	if (! count)
		return -ENODEV;
	return 0;
}

void snd_pci_compat_unregister_driver(struct pci_driver *drv)
{
	struct pci_dev *dev;

	list_del(&drv->node);
	pci_for_each_dev(dev) {
		struct pci_driver_mapping *map = get_pci_driver_mapping(dev);
		if (map && map->drv == drv) {
			if (drv->remove)
				drv->remove(dev);
			map->dev = NULL;
			map->drv = NULL;
	        }
	}
}

unsigned long snd_pci_compat_get_size (struct pci_dev *dev, int n_base)
{
	u32 l, sz;
	int reg = PCI_BASE_ADDRESS_0 + (n_base << 2);

	pci_read_config_dword (dev, reg, &l);
	if (l == 0xffffffff)
		return 0;

	pci_write_config_dword (dev, reg, ~0);
	pci_read_config_dword (dev, reg, &sz);
	pci_write_config_dword (dev, reg, l);

	if (!sz || sz == 0xffffffff)
		return 0;
	if ((l & PCI_BASE_ADDRESS_SPACE) == PCI_BASE_ADDRESS_SPACE_MEMORY) {
		sz = ~(sz & PCI_BASE_ADDRESS_MEM_MASK);
	} else {
		sz = ~(sz & PCI_BASE_ADDRESS_IO_MASK) & 0xffff;
	}
	
	return sz;
}

/* hack, hack, hack - why 2.2 kernels are so broken? */
static struct broken_addr {
	u16 vendor;
	u16 device;
	u32 flags;
} broken_addr[] = {
	{ 0x8086, 0x2415, IORESOURCE_IO },		/* 82801AA */
	{ 0x8086, 0x2425, IORESOURCE_IO },		/* 82901AB */
	{ 0x8086, 0x2445, IORESOURCE_IO },		/* 82801BA */
	{ 0x8086, 0x2485, IORESOURCE_IO },		/* ICH3 */
	{ 0x8086, 0x7195, IORESOURCE_IO },		/* 440MX */
	{ 0, 0, 0 }
};

int snd_pci_compat_get_flags (struct pci_dev *dev, int n_base)
{
	unsigned long foo;
	int flags = 0;

	for (foo = 0; broken_addr[foo].vendor; foo++)
		if (dev->vendor == broken_addr[foo].vendor &&
		    dev->device == broken_addr[foo].device)
		    	return broken_addr[foo].flags;
	
	foo = dev->base_address[n_base] & PCI_BASE_ADDRESS_SPACE;
	if (foo & 1)
		flags |= IORESOURCE_IO;
	else
		flags |= IORESOURCE_MEM;

	return flags;
}

/*
 *  Set power management state of a device.  For transitions from state D3
 *  it isn't as straightforward as one could assume since many devices forget
 *  their configuration space during wakeup.  Returns old power state.
 */
int snd_pci_compat_set_power_state(struct pci_dev *dev, int new_state)
{
	u32 base[5], romaddr;
	u16 pci_command, pwr_command;
	u8  pci_latency, pci_cacheline;
	int i, old_state;
	int pm = snd_pci_compat_find_capability(dev, PCI_CAP_ID_PM);

	if (!pm)
		return 0;
	pci_read_config_word(dev, pm + PCI_PM_CTRL, &pwr_command);
	old_state = pwr_command & PCI_PM_CTRL_STATE_MASK;
	if (old_state == new_state)
		return old_state;
	if (old_state == 3) {
		pci_read_config_word(dev, PCI_COMMAND, &pci_command);
		pci_write_config_word(dev, PCI_COMMAND, pci_command & ~(PCI_COMMAND_IO | PCI_COMMAND_MEMORY));
		for (i = 0; i < 5; i++)
			pci_read_config_dword(dev, PCI_BASE_ADDRESS_0 + i*4, &base[i]);
		pci_read_config_dword(dev, PCI_ROM_ADDRESS, &romaddr);
		pci_read_config_byte(dev, PCI_LATENCY_TIMER, &pci_latency);
		pci_read_config_byte(dev, PCI_CACHE_LINE_SIZE, &pci_cacheline);
		pci_write_config_word(dev, pm + PCI_PM_CTRL, new_state);
		for (i = 0; i < 5; i++)
			pci_write_config_dword(dev, PCI_BASE_ADDRESS_0 + i*4, base[i]);
		pci_write_config_dword(dev, PCI_ROM_ADDRESS, romaddr);
		pci_write_config_byte(dev, PCI_INTERRUPT_LINE, dev->irq);
		pci_write_config_byte(dev, PCI_CACHE_LINE_SIZE, pci_cacheline);
		pci_write_config_byte(dev, PCI_LATENCY_TIMER, pci_latency);
		pci_write_config_word(dev, PCI_COMMAND, pci_command);
	} else
		pci_write_config_word(dev, pm + PCI_PM_CTRL, (pwr_command & ~PCI_PM_CTRL_STATE_MASK) | new_state);
	return old_state;
}

/*
 *  Initialize device before it's used by a driver. Ask low-level code
 *  to enable I/O and memory. Wake up the device if it was suspended.
 *  Beware, this function can fail.
 */
int snd_pci_compat_enable_device(struct pci_dev *dev)
{
	u16 pci_command;

	pci_read_config_word(dev, PCI_COMMAND, &pci_command);
	pci_write_config_word(dev, PCI_COMMAND, pci_command | (PCI_COMMAND_IO | PCI_COMMAND_MEMORY));
	snd_pci_compat_set_power_state(dev, 0);
	return 0;
}

void snd_pci_compat_disable_device(struct pci_dev *dev)
{
	u16 pci_command;

	pci_read_config_word(dev, PCI_COMMAND, &pci_command);
	if (pci_command & PCI_COMMAND_MASTER) {
		pci_command &= ~PCI_COMMAND_MASTER;
		pci_write_config_word(dev, PCI_COMMAND, pci_command);
	}
}

int snd_pci_compat_find_capability(struct pci_dev *dev, int cap)
{
	u16 status;
	u8 pos, id;
	int ttl = 48;

	pci_read_config_word(dev, PCI_STATUS, &status);
	if (!(status & PCI_STATUS_CAP_LIST))
		return 0;
	pci_read_config_byte(dev, PCI_CAPABILITY_LIST, &pos);
	while (ttl-- && pos >= 0x40) {
		pos &= ~3;
		pci_read_config_byte(dev, pos + PCI_CAP_LIST_ID, &id);
		if (id == 0xff)
			break;
		if (id == cap)
			return pos;
		pci_read_config_byte(dev, pos + PCI_CAP_LIST_NEXT, &pos);
	}
	return 0;
}

int snd_pci_compat_dma_supported(struct pci_dev *dev, dma_addr_t mask)
{
	return 1;
}

/*
 */
int snd_pci_compat_request_region(struct pci_dev *pdev, int bar, char *res_name)
{
	int flags;

	if (pci_resource_len(pdev, bar) == 0)
		return 0;
	flags = snd_pci_compat_get_flags(pdev, bar);
	if (flags & IORESOURCE_IO) {
		if (check_region(pci_resource_start(pdev, bar), pci_resource_len(pdev, bar)))
			goto err_out;
		snd_memory_wrapper_request_region(pci_resource_start(pdev, bar),
						  pci_resource_len(pdev, bar), res_name);
	}
	return 0;

err_out:
	printk(KERN_WARNING "PCI: Unable to reserve %s region #%d:%lx@%lx for device %s\n",
		flags & IORESOURCE_IO ? "I/O" : "mem",
		bar + 1, /* PCI BAR # */
		pci_resource_len(pdev, bar), pci_resource_start(pdev, bar),
		res_name);
	return -EBUSY;
}

void snd_pci_compat_release_region(struct pci_dev *pdev, int bar)
{
	int flags;

	if (pci_resource_len(pdev, bar) == 0)
		return;
	flags = snd_pci_compat_get_flags(pdev, bar);
	if (flags & IORESOURCE_IO) {
		release_region(pci_resource_start(pdev, bar),
			       pci_resource_len(pdev, bar));
	}
}

int snd_pci_compat_request_regions(struct pci_dev *pdev, char *res_name)
{
	int i;
	
	for (i = 0; i < 6; i++)
		if (pci_request_region(pdev, i, res_name))
			goto err;
	return 0;
 err:
	while (--i >= 0)
		snd_pci_compat_release_region(pdev, i);
	return -EBUSY;
}

void snd_pci_compat_release_regions(struct pci_dev *pdev)
{
	int i;
	for (i = 0; i < 6; i++)
		snd_pci_compat_release_region(pdev, i);
}

int snd_pci_compat_save_state(struct pci_dev *pdev)
{
	struct pci_driver_mapping *map = get_pci_driver_mapping(pdev);

	if (map) {
		int i;
		u32 *buffer = map->saved_config;
		for (i = 0; i < 16; i++, buffer++)
			pci_read_config_dword(pdev, i * 4, buffer);
	}
	return 0;
}

int snd_pci_compat_restore_state(struct pci_dev *pdev)
{
	struct pci_driver_mapping *map = get_pci_driver_mapping(pdev);

	if (map) {
		int i;
		u32 *buffer = map->saved_config;
		for (i = 0; i < 16; i++, buffer++)
			pci_write_config_dword(pdev, i * 4, *buffer);
	}
	return 0;
}

#endif /* CONFIG_PCI */

/* these functions are outside of CONFIG_PCI */

static void *snd_pci_compat_alloc_consistent1(unsigned long dma_mask,
					      unsigned long size,
					      int hop)
{
	void *res;

	if (++hop > 10)
		return NULL;
	res = snd_malloc_pages(size, GFP_KERNEL | (dma_mask <= 0x00ffffff ? GFP_DMA : 0));
	if (res == NULL)
		return NULL;
	if ((virt_to_bus(res) & ~dma_mask) ||
	    ((virt_to_bus(res) + size - 1) & ~dma_mask)) {
		void *res1 = snd_pci_compat_alloc_consistent1(dma_mask, size, hop);
		snd_free_pages(res, size);
		return res1;
	}
	return res;
}

void *snd_pci_compat_alloc_consistent(struct pci_dev *dev,
				      long size,
				      dma_addr_t *dmaaddr)
{
	unsigned long dma_mask;
	void *res;

#ifdef CONFIG_PCI
	dma_mask = snd_pci_compat_get_dma_mask(dev);
#else
	dma_mask = 0xffffff; /* ISA - 16MB */
#endif
	res = snd_pci_compat_alloc_consistent1(dma_mask, size, 0);
	if (res != NULL)
		*dmaaddr = (dma_addr_t)virt_to_bus(res);
	return res;
}

void snd_pci_compat_free_consistent(struct pci_dev *dev, long size, void *ptr, dma_addr_t dmaaddr)
{
	if (bus_to_virt(dmaaddr) != ptr) {
		printk(KERN_ERR "invalid address given %p != %lx to snd_pci_compat_free_consistent\n", ptr, (unsigned long)dmaaddr);
		return;
	}
	snd_free_pages(ptr, size);
}

