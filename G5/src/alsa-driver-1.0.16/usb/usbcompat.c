#define __NO_VERSION__
#include <linux/config.h>
#include <linux/version.h>

#include <linux/module.h>
#include <linux/usb.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,3,0)

struct snd_compat_usb_device_id {
	__u16		match_flags;
	__u16		idVendor;
	__u16		idProduct;
	__u16		bcdDevice_lo, bcdDevice_hi;
	__u8		bDeviceClass;
	__u8		bDeviceSubClass;
	__u8		bDeviceProtocol;
	__u8		bInterfaceClass;
	__u8		bInterfaceSubClass;
	__u8		bInterfaceProtocol;
	unsigned long	driver_info;
};

struct usb_device;
struct usb_interface;

struct snd_compat_usb_driver {
	const char *name;
	void *(*probe)(struct usb_device *dev, unsigned intf, const struct snd_compat_usb_device_id *id);
	void (*disconnect)(struct usb_device *, void *);
	struct list_head driver_list;
	const struct snd_compat_usb_device_id *id_table;
};

int snd_compat_usb_register(struct snd_compat_usb_driver *);
void snd_compat_usb_deregister(struct snd_compat_usb_driver *);
void snd_compat_usb_driver_claim_interface(struct snd_compat_usb_driver *, struct usb_interface *iface, void *ptr);

#define USB_DEVICE_ID_MATCH_VENDOR		0x0001
#define USB_DEVICE_ID_MATCH_PRODUCT		0x0002
#define USB_DEVICE_ID_MATCH_DEV_LO		0x0004
#define USB_DEVICE_ID_MATCH_DEV_HI		0x0008
#define USB_DEVICE_ID_MATCH_DEV_CLASS		0x0010
#define USB_DEVICE_ID_MATCH_DEV_SUBCLASS	0x0020
#define USB_DEVICE_ID_MATCH_DEV_PROTOCOL	0x0040
#define USB_DEVICE_ID_MATCH_INT_CLASS		0x0080
#define USB_DEVICE_ID_MATCH_INT_SUBCLASS	0x0100
#define USB_DEVICE_ID_MATCH_INT_PROTOCOL	0x0200

#define MAX_USB_DRIVERS	5
struct snd_usb_reg_table {
	struct usb_driver driver;
	struct snd_compat_usb_driver *orig;
};

static struct snd_usb_reg_table my_usb_drivers[MAX_USB_DRIVERS];

static void *snd_usb_compat_probe(struct usb_device *dev, unsigned int ifnum)
{
	struct snd_compat_usb_driver *p;
	struct usb_interface_descriptor *alts = usb_ifnum_to_if(dev, ifnum)->altsetting;
	const struct snd_compat_usb_device_id *tbl;
	struct snd_compat_usb_device_id id;
	int i;

	for (i = 0; i < MAX_USB_DRIVERS; i++) {
		if (! (p = my_usb_drivers[i].orig))
			continue;
		for (tbl = p->id_table; tbl->match_flags; tbl++) {
			/* we are too lazy to check all entries... */
			if ((tbl->match_flags & USB_DEVICE_ID_MATCH_VENDOR) &&
			    tbl->idVendor != dev->descriptor.idVendor)
				continue;
			if ((tbl->match_flags & USB_DEVICE_ID_MATCH_PRODUCT) &&
			    tbl->idProduct != dev->descriptor.idProduct)
				continue;
			if ((tbl->match_flags & USB_DEVICE_ID_MATCH_INT_CLASS) &&
			    tbl->bInterfaceClass != alts->bInterfaceClass)
				continue;
			if ((tbl->match_flags & USB_DEVICE_ID_MATCH_INT_SUBCLASS) &&
			    tbl->bInterfaceSubClass != alts->bInterfaceSubClass)
				continue;
			id = *tbl;
			id.idVendor = dev->descriptor.idVendor;
			id.idProduct = dev->descriptor.idProduct;
			id.bInterfaceClass = alts->bInterfaceClass;
			id.bInterfaceSubClass = alts->bInterfaceSubClass;
			return p->probe(dev, ifnum, &id);
		}
	}
	return NULL;
}

int snd_compat_usb_register(struct snd_compat_usb_driver *driver)
{
	int i;
	struct usb_driver *drv;

	for (i = 0; i < MAX_USB_DRIVERS; i++) {
		if (! my_usb_drivers[i].orig)
			break;
	}
	if (i >= MAX_USB_DRIVERS)
		return -ENOMEM;
	my_usb_drivers[i].orig = driver;
	drv = &my_usb_drivers[i].driver;
	drv->name = driver->name;
	drv->probe = snd_usb_compat_probe;
	drv->disconnect = driver->disconnect;
	INIT_LIST_HEAD(&drv->driver_list);
	usb_register(drv);
	return 0;
}

static struct snd_usb_reg_table *find_matching_usb_driver(struct snd_compat_usb_driver *driver)
{
	int i;
	for (i = 0; i < MAX_USB_DRIVERS; i++) {
		if (my_usb_drivers[i].orig == driver)
			return &my_usb_drivers[i];
	}
	return NULL;
}

void snd_compat_usb_deregister(struct snd_compat_usb_driver *driver)
{
	struct snd_usb_reg_table *tbl;	
	if ((tbl = find_matching_usb_driver(driver)) != NULL) {
		usb_deregister(&tbl->driver);
		tbl->orig = NULL;
	}
}

void snd_compat_usb_driver_claim_interface(struct snd_compat_usb_driver *driver, struct usb_interface *iface, void *ptr)
{
	struct snd_usb_reg_table *tbl;
	if ((tbl = find_matching_usb_driver(driver)) != NULL)
		usb_driver_claim_interface(&tbl->driver, iface, ptr);
}

EXPORT_SYMBOL(snd_compat_usb_register);
EXPORT_SYMBOL(snd_compat_usb_deregister);
EXPORT_SYMBOL(snd_compat_usb_driver_claim_interface);

#endif /* LINUX_VERSION < 2.3.0 */
