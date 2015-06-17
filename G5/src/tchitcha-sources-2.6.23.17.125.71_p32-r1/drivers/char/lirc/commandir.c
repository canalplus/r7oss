
/*
 *
 *	Hardware Driver for COMMANDIR USB Transceiver
 *	2005-2007 InnovationOne - Matt Bodkin, Evelyn Yeung
 *
 *	Version 1.4.2
 *	For 2.4.* or 2.6.* kernel versions
 *	Based on the USB Skeleton driver, versions 0.7 and 2.0
 *
 */

#include <linux/version.h>
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2, 6, 18)
#include <linux/config.h>
#else
#include <linux/autoconf.h>
#endif

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/usb.h>
#include <linux/smp_lock.h>
#include "commandir.h"

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
#include <linux/sched.h>
#include <linux/signal.h>
#include <linux/poll.h>
#include <linux/fcntl.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/devfs_fs_kernel.h>
#else
#include <linux/kref.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 18)
#include <asm/uaccess.h>
#else
#include <linux/uaccess.h>
#endif
#endif

#define DRIVER_VERSION "v1.1.2"
#define DRIVER_AUTHOR "Evelyn Yeung, InnovationOne"
#define DRIVER_DESC "CommandIR USB Transceiver Driver"

#define USB_CMDIR_VENDOR_ID	0x10c4
#define USB_CMDIR_PRODUCT_ID	0x0003
#define USB_CMDIR_MINOR_BASE	192

/* table of devices that work with this driver */
static struct usb_device_id cmdir_table [] =
{
	{ USB_DEVICE(USB_CMDIR_VENDOR_ID, USB_CMDIR_PRODUCT_ID) },
	{ }			/* Terminating entry */
};
MODULE_DEVICE_TABLE(usb, cmdir_table);

static int cmdir_open(struct inode *inode, struct file *file);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
static void *cmdir_probe(struct usb_device *dev, unsigned int ifnum,
			  const struct usb_device_id *id);
static int cmdir_ioctl(struct inode *inode, struct file *file,
		       unsigned int cmd, unsigned long arg);
static void cmdir_disconnect(struct usb_device *dev, void *ptr);
#else
static int cmdir_probe(struct usb_interface *interface,
		       const struct usb_device_id *id);
static void cmdir_disconnect(struct usb_interface *interface);
#endif
static int cmdir_release(struct inode *inode, struct file *file);
static int cmdir_check(int device_num);
static void init_cmdir_var(int device_num);
static void reset_cmdir(int device_num);
static void update_cmdir_string(int device_num);
static void print_cmdir(int device_num);
static ssize_t cmdir_file_read(struct file *file, char *buffer,
			       size_t count, loff_t *ppos);
ssize_t cmdir_read(unsigned char *buffer, size_t count);
static ssize_t cmdir_file_write(struct file *file, const char *buffer,
				size_t count, loff_t *ppos);
int cmdir_write(unsigned char *buffer, int count, void *callback_fct, int u);
int write_core(unsigned char *buffer, int count,
	       void *callback_fct, int device_num);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
static void cmdir_write_bulk_callback(struct urb *urb);
#else
static void cmdir_write_bulk_callback(struct urb *urb, struct pt_regs *regs);
#endif
int set_tx_channels(unsigned int next_tx);

int add_cmdir_queue(unsigned char *buffer, int count,
		    void *callback_vct, int usecdelay);
int cmdir_write_queue(unsigned char *buffer, int count, void *callback_vct);
int send_queue(void);
int wait_to_tx(int usecs);

/* circular packet queue */
unsigned char ourbuffers[QUEUELENGTH][64];
int waitusecs[QUEUELENGTH];
int ourbufferlengths[QUEUELENGTH];
int nexttosend;
int nexttofill;
int send_status = SEND_IDLE;
int last_tx_sec;
int last_tx_usec;

static int curTXFill;
struct timeval tp;

int debug_commandir;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)

/* Structure to hold all of our device specific stuff */
struct usb_skel {
	struct usb_device *udev; /* save off the usb device pointer */
	struct usb_interface *interface; /* the interface for this device */
	devfs_handle_t devfs; /* devfs device node */
	unsigned char minor; /* the starting minor number */
	unsigned char num_ports; /* the number of ports this device has */
	char num_interrupt_in; /* number of interrupt in endpoints */
	char num_bulk_in; /* number of bulk in endpoints */
	char num_bulk_out; /* number of bulk out endpoints */

	unsigned char *bulk_in_buffer; /* the buffer to receive data */
	int bulk_in_size; /* the size of the receive buffer */
	__u8 bulk_in_endpointAddr; /* the address of the bulk in endpoint */

	unsigned char *bulk_out_buffer; /* the buffer to send data */
	int bulk_out_size; /* the size of the send buffer */
	struct urb *write_urb; /* the urb used to send data */
	__u8 bulk_out_endpointAddr; /* the address of the bulk out endpoint */

	struct tq_struct tqueue; /* task queue for line discipline waking up */
	int open_count; /* number of times this port has been opened */
	struct semaphore sem; /* locks this structure */
};

extern devfs_handle_t usb_devfs_handle; /* the global usb devfs handle */

/* array of pointers to our devices that are currently connected */
static struct usb_skel *minor_table[MAX_DEVICES];
/* lock to protect the minor_table structure */
static DECLARE_MUTEX(minor_table_mutex);

static struct file_operations cmdir_fops = {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 16)
	.owner		= THIS_MODULE,
#endif /* kernel < 2.6.16 */
#endif /* kernel >= 2.6.0 */
	.read		= cmdir_file_read,
	.write		= cmdir_file_write,
	.ioctl		= cmdir_ioctl,
	.open		= cmdir_open,
	.release	= cmdir_release,
};

/* usb specific object needed to register this driver with the usb subsystem */
static struct usb_driver cmdir_driver = {
	.name		= "commandir",
	.probe		= cmdir_probe,
	.disconnect	= cmdir_disconnect,
	.fops		= &cmdir_fops,
	.minor		= USB_CMDIR_MINOR_BASE,
	.id_table	= cmdir_table,
};

#else /* kernel >= 2.6 */

/* Structure to hold all of our device specific stuff */
struct usb_skel {
	struct usb_device *udev; /* the usb device for this device */
	struct usb_interface *interface; /* the interface for this device */
	unsigned char *bulk_in_buffer; /* the buffer to receive data */
	size_t bulk_in_size; /* the size of the receive buffer */
	__u8 bulk_in_endpointAddr; /* the address of the bulk in endpoint */
	__u8 bulk_out_endpointAddr; /* the address of the bulk out endpoint */
	struct kref kref;
};
#define to_skel_dev(d) container_of(d, struct usb_skel, kref)

static struct file_operations cmdir_fops = {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 16)
	.owner =	THIS_MODULE,
#endif /* kernel < 2.6.16 */
#endif /* kernel >= 2.6.0 */
	.read =		cmdir_file_read,
	.write =	cmdir_file_write,
	.open =		cmdir_open,
	.release =	cmdir_release,
};

/* usb class driver info in order to get a minor number from the usb core,
 * and to have the device registered with devfs and the driver core */
static struct usb_class_driver cmdir_class = {
	.name =		"usb/commandir%d",
	.fops =		&cmdir_fops,
	/* .mode =	S_IFCHR | S_IRUSR | S_IWUSR |
	 *		S_IRGRP | S_IWGRP | S_IROTH, */
	.minor_base =	USB_CMDIR_MINOR_BASE,
};

static struct usb_driver cmdir_driver = {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 16)
	.owner =	THIS_MODULE,
#endif /* kernel < 2.6.16 */
#endif /* kernel >= 2.6.0 */
	.name =		"commandir",
	.probe =	cmdir_probe,
	.disconnect =	cmdir_disconnect,
	.id_table =	cmdir_table,
};

#endif /* kernel < 2.6.0 */

static int lcd_device;
static int rx_device;
static int def_device;

#define DEFAULT_TRANSMITTERS 0x0F
static unsigned int transmitters = DEFAULT_TRANSMITTERS;
static unsigned int next_transmitters = DEFAULT_TRANSMITTERS;

#define CMDIR_VAR_LEN 68
static char cmdir_var[] =
"COMMANDIRx:\n TX Enabled: 1, 2, 3, 4\n RX: commandirx\n LCD: commandirx";


#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
static inline void cmdir_delete(struct usb_skel *dev)
{
	minor_table[dev->minor] = NULL;
	if (dev->bulk_in_buffer != NULL)
		kfree(dev->bulk_in_buffer);
	if (dev->bulk_out_buffer != NULL)
		kfree(dev->bulk_out_buffer);
	if (dev->write_urb != NULL)
		usb_free_urb(dev->write_urb);
	kfree(dev);
}
#else
static void cmdir_delete(struct kref *kref)
{
	struct usb_skel *dev = to_skel_dev(kref);

	usb_put_dev(dev->udev);
	kfree(dev->bulk_in_buffer);
	kfree(dev);
}
#endif

static int __init usb_cmdir_init(void)
{
	int result;

	/* register this driver with the USB subsystem */
	result = usb_register(&cmdir_driver);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
	if (result < 0) {
		err("usb_register failed for the "__FILE__
		    " driver. Error number %d", result);
		return -1;
	}

	info(DRIVER_DESC " " DRIVER_VERSION);

	return 0;
#else
	if (result)
		err("usb_register failed. Error number %d", result);

	return result;
#endif
}

static int cmdir_open(struct inode *inode, struct file *file)
{
	struct usb_skel *dev;
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 0)
	struct usb_interface *interface;
#endif
	int subminor;
	int retval = 0;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
	subminor = MINOR(inode->i_rdev) - USB_CMDIR_MINOR_BASE;
	if ((subminor < 0) || (subminor >= MAX_DEVICES))
		return -ENODEV;
	MOD_INC_USE_COUNT;

	/* lock our minor table and get our local data for this minor */
	down(&minor_table_mutex);
	dev = minor_table[subminor];
	if (dev == NULL) {
		up(&minor_table_mutex);
		MOD_DEC_USE_COUNT;
		return -ENODEV;
	}

	down(&dev->sem);	/* lock this device */
	up(&minor_table_mutex);	/* unlock the minor table */
	++dev->open_count;	/* increment our usage count for the driver */
	file->private_data = dev; /* save object in file's private structure */
	up(&dev->sem);		/* unlock this device */

	return retval;
#else
	subminor = iminor(inode);
	interface = usb_find_interface(&cmdir_driver, subminor);
	if (!interface) {
		err("%s - error, can't find device for minor %d",
		     __FUNCTION__, subminor);
		retval = -ENODEV;
		goto exit;
	}

	dev = usb_get_intfdata(interface);
	if (!dev) {
		retval = -ENODEV;
		goto exit;
	}

	kref_get(&dev->kref);	/* increment our usage count for the device */
	file->private_data = dev; /* save object in file's private structure */

exit:
	return retval;
#endif
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
static void *cmdir_probe(struct usb_device *udev, unsigned int ifnum,
			 const struct usb_device_id *id)
{
	struct usb_skel *dev = NULL;
	struct usb_interface *interface;
	struct usb_interface_descriptor *iface_desc;
	struct usb_endpoint_descriptor *endpoint;
	int minor;
	int buffer_size;
	int i;
	char name[10];

	/* See if the device offered us matches what we can accept */
	if ((udev->descriptor.idVendor != USB_CMDIR_VENDOR_ID) ||
	    (udev->descriptor.idProduct != USB_CMDIR_PRODUCT_ID))
		return NULL;

	/* select a "subminor" number (part of a minor number) */
	down(&minor_table_mutex);
	for (minor = 0; minor < MAX_DEVICES; ++minor) {
		if (minor_table[minor] == NULL)
			break;
	}
	if (minor >= MAX_DEVICES) {
		info("Too many devices plugged in, cannot handle this device.");
		goto exit;
	}

	/* allocate memory for our device state and intialize it */
	dev = kmalloc(sizeof(struct usb_skel), GFP_KERNEL);
	if (dev == NULL) {
		err("Out of memory");
		goto exit;
	}
	memset(dev, 0x00, sizeof(*dev));
	minor_table[minor] = dev;

	interface = &udev->actconfig->interface[ifnum];

	init_MUTEX(&dev->sem);
	dev->udev = udev;
	dev->interface = interface;
	dev->minor = minor;

	/* set up and check the endpoint information */
	iface_desc = &interface->altsetting[0];
	for (i = 0; i < iface_desc->bNumEndpoints; ++i) {
		endpoint = &iface_desc->endpoint[i];

		if ((endpoint->bEndpointAddress & 0x80) &&
		    ((endpoint->bmAttributes & 3) == 0x02)) {
			/* we found a bulk in endpoint */
			buffer_size = endpoint->wMaxPacketSize;
			dev->bulk_in_size = buffer_size;
			dev->bulk_in_endpointAddr = endpoint->bEndpointAddress;
			dev->bulk_in_buffer = kmalloc(buffer_size, GFP_KERNEL);
			if (!dev->bulk_in_buffer) {
				err("Couldn't allocate bulk_in_buffer");
				goto error;
			}
		}

		if (((endpoint->bEndpointAddress & 0x80) == 0x00) &&
		    ((endpoint->bmAttributes & 3) == 0x02)) {
			/* we found a bulk out endpoint */
			dev->write_urb = usb_alloc_urb(0);
			if (!dev->write_urb) {
				err("No free urbs available");
				goto error;
			}
			buffer_size = endpoint->wMaxPacketSize;
			dev->bulk_out_size = buffer_size;
			dev->bulk_out_endpointAddr = endpoint->bEndpointAddress;
			dev->bulk_out_buffer = kmalloc(buffer_size, GFP_KERNEL);
			if (!dev->bulk_out_buffer) {
				err("Couldn't allocate bulk_out_buffer");
				goto error;
			}
			FILL_BULK_URB(dev->write_urb, udev,
				      usb_sndbulkpipe(udev,
					endpoint->bEndpointAddress),
				      dev->bulk_out_buffer, buffer_size,
				      cmdir_write_bulk_callback, dev);
		}
	}

	/* initialize the devfs node for this device and register it */
	sprintf(name, "commandir%d", dev->minor);

	dev->devfs = devfs_register(usb_devfs_handle, name,
				     DEVFS_FL_DEFAULT, USB_MAJOR,
				     USB_CMDIR_MINOR_BASE + dev->minor,
				     S_IFCHR | S_IRUSR | S_IWUSR |
				     S_IRGRP | S_IWGRP | S_IROTH,
				     &cmdir_fops, NULL);

	/* let the user know what node this device is now attached to */
	info("CommandIR USB device now attached to commandir%d", dev->minor);

	/* should reset just the one that was plugged in */
	reset_cmdir(minor);

	goto exit;

error:
	cmdir_delete(dev);
	dev = NULL;

exit:
	up(&minor_table_mutex);
	return dev;
}
#else
static int cmdir_probe(struct usb_interface *interface,
		       const struct usb_device_id *id)
{
	struct usb_skel *dev = NULL;
	struct usb_host_interface *iface_desc;
	struct usb_endpoint_descriptor *endpoint;
	size_t buffer_size;

	int i;
	int retval = -ENOMEM;
	int minor;

	/* allocate memory for our device state and initialize it */
	dev = kmalloc(sizeof(*dev), GFP_KERNEL);
	if (dev == NULL) {
		err("Out of memory");
		goto error;
	}
	memset(dev, 0x00, sizeof(*dev));
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 9)
	kref_init(&dev->kref, cmdir_delete);
#else
	kref_init(&dev->kref);
#endif
	dev->udev = usb_get_dev(interface_to_usbdev(interface));
	dev->interface = interface;

	/* set up the endpoint information */
	/* use only the first bulk-in and bulk-out endpoints */
	iface_desc = interface->cur_altsetting;
	for (i = 0; i < iface_desc->desc.bNumEndpoints; ++i) {
		endpoint = &iface_desc->endpoint[i].desc;

		if (!dev->bulk_in_endpointAddr &&
		    (endpoint->bEndpointAddress & USB_DIR_IN) &&
		    ((endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK)
					== USB_ENDPOINT_XFER_BULK)) {
			/* we found a bulk in endpoint */
			buffer_size = endpoint->wMaxPacketSize;
			dev->bulk_in_size = buffer_size;
			dev->bulk_in_endpointAddr = endpoint->bEndpointAddress;
			dev->bulk_in_buffer = kmalloc(buffer_size, GFP_KERNEL);
			if (!dev->bulk_in_buffer) {
				err("Could not allocate bulk_in_buffer");
				goto error;
			}
		}

		if (!dev->bulk_out_endpointAddr &&
		    !(endpoint->bEndpointAddress & USB_DIR_IN) &&
		    ((endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK)
					== USB_ENDPOINT_XFER_BULK)) {
			/* we found a bulk out endpoint */
			dev->bulk_out_endpointAddr = endpoint->bEndpointAddress;
		}
	}
	if (!(dev->bulk_in_endpointAddr && dev->bulk_out_endpointAddr)) {
		err("Could not find both bulk-in and bulk-out endpoints");
		goto error;
	}

	/* save our data pointer in this interface device */
	usb_set_intfdata(interface, dev);

	/* we can register the device now, as it is ready */
	retval = usb_register_dev(interface, &cmdir_class);
	if (retval) {
		/* something prevented us from registering this driver */
		err("Not able to get a minor for this device.");
		usb_set_intfdata(interface, NULL);
		goto error;
	}

	/* check whether minor already includes base */
	minor = interface->minor;
	if (minor >= USB_CMDIR_MINOR_BASE)
		minor = minor-USB_CMDIR_MINOR_BASE;

	/* let the user know what node this device is now attached to */
	info("CommandIR USB device now attached to commandir%d", minor);

	reset_cmdir(minor);

	return 0;

error:
	if (dev)
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 9)
		kref_put(&dev->kref);
#else
		kref_put(&dev->kref, cmdir_delete);
#endif
	return retval;
}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
static int cmdir_ioctl(struct inode *inode, struct file *file,
		       unsigned int cmd, unsigned long arg)
{
	struct usb_skel *dev;

	dev = (struct usb_skel *)file->private_data;

	/* lock this object */
	down(&dev->sem);

	/* verify that the device wasn't unplugged */
	if (dev->udev == NULL) {
		up(&dev->sem);
		return -ENODEV;
	}

	/* unlock the device */
	up(&dev->sem);

	/* return that we did not understand this ioctl call */
	return -ENOTTY;
}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
static void cmdir_disconnect(struct usb_device *udev, void *ptr)
{
	struct usb_skel *dev;
	int minor;

	dev = (struct usb_skel *)ptr;

	down(&minor_table_mutex);
	down(&dev->sem);

	minor = dev->minor;

	/* remove our devfs node */
	devfs_unregister(dev->devfs);

	/* if the device is not opened, then we clean up right now */
	if (!dev->open_count) {
		up(&dev->sem);
		cmdir_delete(dev);
	} else {
		dev->udev = NULL;
		up(&dev->sem);
	}

	info("CommandIR #%d now disconnected", minor);
	up(&minor_table_mutex);

	/* check if default RX device still exists */
	if (minor == rx_device) {
		/* decrement until find next valid device */
		while (rx_device > 0) {
			rx_device--;
			if (cmdir_check(rx_device) == 0) break;
		}
		if (minor > 0)
			info("Active Receiver is on CommandIR #%d", rx_device);
	}

}
#else
static void cmdir_disconnect(struct usb_interface *interface)
{
	struct usb_skel *dev;
	int minor = interface->minor;

	/* prevent cmdir_open() from racing cmdir_disconnect() */
	lock_kernel();

	dev = usb_get_intfdata(interface);
	usb_set_intfdata(interface, NULL);

	/* give back our minor */
	usb_deregister_dev(interface, &cmdir_class);

	unlock_kernel();

	/* decrement our usage count */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 9)
		kref_put(&dev->kref);
#else
		kref_put(&dev->kref, cmdir_delete);
#endif

	/* check whether minor already includes base */
	if (minor >= USB_CMDIR_MINOR_BASE)
		minor = minor-USB_CMDIR_MINOR_BASE;

	info("CommandIR #%d now disconnected", minor);

	/* check if default RX device still exists */
	if (minor == rx_device) {
		/* decrement until find next valid device */
		while (rx_device > 0) {
			rx_device--;
			if (cmdir_check(rx_device) == 0)
				break;
		}
		if (minor > 0)
			info("Active Receiver is on CommandIR #%d", rx_device);
	}
}
#endif

static int cmdir_release(struct inode *inode, struct file *file)
{
	struct usb_skel *dev;
	int retval = 0;

	dev = (struct usb_skel *)file->private_data;
	if (dev == NULL)
		/*dbg(" - object is NULL");*/
		return -ENODEV;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)

	/* lock our minor table */
	down(&minor_table_mutex);

	/* lock our device */
	down(&dev->sem);

	if (dev->open_count <= 0) {
		/*dbg(" - device not opened");*/
		retval = -ENODEV;
		goto exit_not_opened;
	}

	if (dev->udev == NULL) {
		/* the device was unplugged before the file was released */
		/*dbg(" - device unplugged before file released");*/
		up(&dev->sem);
		cmdir_delete(dev);
		up(&minor_table_mutex);
		MOD_DEC_USE_COUNT;
		return 0;
	}

	/* decrement our usage count for the device */
	--dev->open_count;
	if (dev->open_count <= 0) {
		/* shutdown any bulk writes that might be going on */
		usb_unlink_urb(dev->write_urb);
		dev->open_count = 0;
	}

	/* decrement our usage count for the module */
	MOD_DEC_USE_COUNT;

exit_not_opened:
	up(&dev->sem);
	up(&minor_table_mutex);

	return retval;

#else
	/* decrement the count on our device */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 9)
		kref_put(&dev->kref);
#else
		kref_put(&dev->kref, cmdir_delete);
#endif
	return retval;
#endif
}

static void __exit usb_cmdir_exit(void)
{
	/* deregister this driver with the USB subsystem */
	usb_deregister(&cmdir_driver);

}

static int cmdir_check(int device_num)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
	if (minor_table[device_num] == NULL)
		return -ENODEV;
	return 0;
#else
	struct usb_interface *interface;

	interface = usb_find_interface(&cmdir_driver,
				       USB_CMDIR_MINOR_BASE+device_num);
	if (!interface) {
		/* also check without adding base, for devfs */
		interface = usb_find_interface(&cmdir_driver, rx_device);
		if (!interface)
			return -ENODEV;
	}
	return 0;
#endif
}

static void init_cmdir_var(int device_num)
{
	int i;
	unsigned int multiplier = 1;

	for (i = 0; i < device_num; i++)
		multiplier = multiplier*0x10;
	transmitters |= multiplier * 0x0F;
	next_transmitters = transmitters;
	info("commandir%d reset", device_num);
	return;
}

static void reset_cmdir(int device_num)
{
	unsigned char ctrl_buffer[MCU_CTRL_SIZE];
	int retval;
	int i;

	ctrl_buffer[0] = RESET_HEADER;
	for (i = 1; i < MCU_CTRL_SIZE; i++)
		ctrl_buffer[i] = 'j';
	retval = write_core(ctrl_buffer, MCU_CTRL_SIZE, NULL, device_num);

	init_cmdir_var(device_num);
	print_cmdir(device_num);

	return;
}

static void update_cmdir_string(int device_num)
{
	int next_comma = 0;
	int next_pos = 25;
	unsigned int multiplier;
	int i;

	/* cmdir_var[] = "COMMANDIRx:\n"
	 * 		 " TX Enabled: 1, 2, 3, 4\n"
	 * 		 " RX: commandirx\n"
	 * 		 " LCD: commandirx\n" */

	cmdir_var[9] = ASCII0+device_num;
	cmdir_var[50] = ASCII0+rx_device;
	cmdir_var[67] = ASCII0+lcd_device;

	for (i = 25; i < 35; i++)
		cmdir_var[i] = ' ';

	multiplier = 1;
	for (i = 0; i < device_num; i++)
		multiplier = multiplier*0x10;

	if (transmitters & (multiplier*0x01)) {
		cmdir_var[next_pos] = '1';
		next_pos += 3;
		next_comma++;
	}
	if (transmitters & (multiplier*0x02)) {
		cmdir_var[next_pos] = '2';
		if (next_comma > 0)
			cmdir_var[next_pos-2] = ',';
		next_pos += 3;
		next_comma++;
	}
	if (transmitters & (multiplier*0x04)) {
		cmdir_var[next_pos] = '3';
		if (next_comma > 0)
			cmdir_var[next_pos-2] = ',';
		next_pos += 3;
		next_comma++;
	}
	if (transmitters & (multiplier*0x08)) {
		cmdir_var[next_pos] = '4';
		if (next_comma > 0)
			cmdir_var[next_pos-2] = ',';
		next_pos += 3;
		next_comma++;
	}
	return;
}

static void print_cmdir(int device_num)
{
	update_cmdir_string(device_num);
	info("%s", cmdir_var);
	return;
}

static ssize_t cmdir_file_read(struct file *file, char *buffer,
			       size_t count, loff_t *ppos)
{
	int retval = 0;
	int minor = 0;
	struct usb_skel *dev;

	dev = (struct usb_skel *)file->private_data;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
	minor = dev->minor;
#else
	minor = dev->interface->minor;
	if (minor >= USB_CMDIR_MINOR_BASE)
		minor = minor - USB_CMDIR_MINOR_BASE;
#endif

	if (((int)*ppos) == 0) {
		update_cmdir_string(minor);
		if (copy_to_user(buffer, cmdir_var, CMDIR_VAR_LEN))
			retval = -EFAULT;
		else
			retval = CMDIR_VAR_LEN;
		return retval;
	} else
		return 0;
}

/*  Read data from CommandIR  */
ssize_t cmdir_read(unsigned char *buffer, size_t count)
{
	struct usb_skel *dev;
	int retval = 0;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
	if (minor_table[rx_device] == NULL)
		return -ENODEV;
	dev = minor_table[rx_device];

	/* lock this object */
	down(&dev->sem);
	retval = usb_bulk_msg(dev->udev, usb_rcvbulkpipe(dev->udev,
						dev->bulk_in_endpointAddr),
		 dev->bulk_in_buffer, dev->bulk_in_size, &count, HZ*100);
#else
	struct usb_interface *interface;
	interface = usb_find_interface(&cmdir_driver,
			USB_CMDIR_MINOR_BASE+rx_device);
	if (!interface) {
		/* also check without adding base, for devfs */
		interface = usb_find_interface(&cmdir_driver, rx_device);
		if (!interface)
			return -ENODEV;
	}
	dev = usb_get_intfdata(interface);
	if (!dev)
		return -ENODEV;
	retval = usb_bulk_msg(dev->udev, usb_rcvbulkpipe(dev->udev,
						dev->bulk_in_endpointAddr),
		 dev->bulk_in_buffer, min(dev->bulk_in_size, count),
		 &count, HZ*10);
#endif
	if (!retval) {
		if (!memcpy(buffer, dev->bulk_in_buffer, count))
			retval = -EFAULT;
		else {
			/* current status of the TX buffer */
			curTXFill = buffer[2];
			retval = count;
		}
	}
	/* suppress errors */
	/*
	else {
		err("Read from device failed, error %d",retval);
	}
	*/
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
	/* unlock the device */
	up(&dev->sem);
#endif
	/* printk(KERN_INFO "CommandIR Reporting TX buffer at %d bytes. \n",
	 * 	  curTXFill); */
	return retval;
}
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
EXPORT_SYMBOL_NOVERS(cmdir_read);
#else
EXPORT_SYMBOL(cmdir_read);
#endif

static ssize_t cmdir_file_write(struct file *file, const char *buffer,
				size_t count, loff_t *ppos)
{
	int retval;
	int i;
	int equalsign = 0;
	int changeType = 0;
	unsigned char ctrl_buffer[MCU_CTRL_SIZE];
	char *local_buffer;
	int minor;

	/* set as default - if non-specific error,
	 * won't keep calling this function */
	retval = count;
	local_buffer = kmalloc(count, GFP_KERNEL);

	/* verify that we actually have some data to write */
	if (count == 0) {
		err("Write request of 0 bytes");
		goto exit;
	}
	if (count > 64) {
		err("Input too long");
		goto exit;
	}

	/* copy the data from userspace into our local buffer */
	if (copy_from_user(local_buffer, buffer, count)) {
		retval = -EFAULT;
		goto exit;
	}

	/* parse code */
	changeType = cNothing;
	equalsign = 0;
	for (i = 0; i < MCU_CTRL_SIZE; i++)
		ctrl_buffer[i] = 'j';

	for (i = 0; i < count; i++) {
		switch (local_buffer[i]) {
		case 'X':
		case 'x':
			if ((i > 0) && ((local_buffer[i - 1] == 'R')
			    || (local_buffer[i - 1] == 'r')))
				changeType = cRX;
			break;
		case 'S':
		case 's':
			if ((i > 1) && ((local_buffer[i - 1] == 'E')
			    || (local_buffer[i - 1] == 'e'))) {
				if ((local_buffer[i-2] == 'R')
				    || (local_buffer[i-2] == 'r'))
					changeType = cRESET;
			}
			break;
		case 'L':
		case 'l':
			if ((i > 0) && ((local_buffer[i - 1] == 'F')
			    || (local_buffer[i - 1] == 'f')))
				changeType = cFLASH;
			break;
		case 'C':
		case 'c':
			if ((i > 0) && ((local_buffer[i - 1] == 'L')
			    || (local_buffer[i - 1] == 'l')))
				changeType = cLCD;
			break;
		case '=':
			if (changeType != cNothing)
				equalsign = i;
			break;
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
			if (equalsign > 0) {
				minor = local_buffer[i] - ASCII0;
				switch (changeType) {
				case cRESET:
					ctrl_buffer[0] = RESET_HEADER;
					retval = write_core(ctrl_buffer,
						MCU_CTRL_SIZE,
						cmdir_write_bulk_callback,
						minor);
					if (retval != MCU_CTRL_SIZE) {
						if (retval == -ENODEV)
							err("Device %d "
							    "unplugged", minor);
						else
							err("Error on write to "
							    "%d", minor);
						goto exit;
					} else
						retval = count;
					init_cmdir_var(minor);
					break;
				case cFLASH:
					ctrl_buffer[0] = FLASH_HEADER;
					info("Flashing indicators on device %d",
					     minor);
					retval = write_core(ctrl_buffer,
						MCU_CTRL_SIZE,
						cmdir_write_bulk_callback,
						minor);
					if (retval != MCU_CTRL_SIZE) {
						if (retval == -ENODEV)
							err("Device %d "
							    "unplugged", minor);
						else
							err("Error on write to "
							    "%d", minor);
						goto exit;
					} else
						retval = count;
					break;
				case cRX:
					rx_device = minor;
					info("Default receiver set to %d",
					     minor);
					break;
				case cLCD:
					lcd_device = minor;
					info("commandir: Default LCD set to %d",
					     minor);
					break;
				default:
					break;
				}
			}
			break;
		case ',':
			equalsign = 0;
			changeType = cNothing;
			break;
		default:
			if ((equalsign > 0) && (local_buffer[i] > 32)) {
				err("Non-numerical argument");
				goto exit;
			}
			break;
		}
	}

	if ((changeType != cNothing) && (equalsign == 0))
		err("No device specified");
	if (changeType == cNothing)
		err("Unknown command");

exit:
	kfree(local_buffer);
	return retval;
}

int cmdir_write(unsigned char *buffer, int count,
		void *callback_fct, int usecdelay)
{
	/* Always add to queue, then send queue number
	 * no locks
	 * mbodkin, Sept 8, 2005 */
	int ret = 0;
	if (debug_commandir == 1) {
		do_gettimeofday(&tp);
		printk(KERN_INFO "cmdir_write at %d\n", (int)tp.tv_usec);
	}
	ret = add_cmdir_queue(buffer, count, callback_fct, usecdelay);

	if (ret == -1)  {
		printk(KERN_INFO "cmdir_write returning 0\n");
		return 0;
	}
	return count;

}
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
EXPORT_SYMBOL_NOVERS(cmdir_write);
#else
EXPORT_SYMBOL(cmdir_write);
#endif

int add_cmdir_queue(unsigned char *buffer, int count,
		    void *callback_vct, int usecdelay)
{
	int ret = 0;
	if ((nexttofill + 1) % (QUEUELENGTH - 1) == nexttosend) {

		/* our buffer is full */
		printk(KERN_INFO "Too many packets backlogged "
		       "in CommandIR Queue.\n");
		return -1;
	}
	/* go ahead and use this one: */
	memcpy(ourbuffers[nexttofill], buffer, count);
	ourbufferlengths[nexttofill] = count;
	waitusecs[nexttofill] = (usecdelay == 0) ? 10000 : usecdelay;
	/* printk(KERN_INFO "Adding %d to queue at position %d.\n",
	 *        count, nexttofill); */
	nexttofill = (nexttofill + 1) % (QUEUELENGTH - 1);
	ret = nexttofill;
	/* if (timer_running == 0) */
	send_queue(); /* fake it if the timer's not running */
	return ret;	/* we accepted the full packet */

}

int send_queue()
{
	int last_sent = 0;
	int ret = 0;
	if (debug_commandir == 1) {
		do_gettimeofday(&tp);
		printk(KERN_INFO "Send_queue() at %d\n", (int)tp.tv_usec);
	}
	/* initiate the send/callback routine if not already running. */
	if (send_status == SEND_IDLE) {
		if (!(nexttofill == nexttosend)) {
			/* start it up: */

			last_sent = nexttosend - 1;
			if (last_sent < 0)
				last_sent = QUEUELENGTH - 1;
			/* Final check - is it TIME to send this packet yet? */
			/* if (wait_to_tx(waitusecs[last_sent]) == 0) { */
			/* always send if there's room,
			 * otherwise wait until room */
			if (curTXFill < 190) {
				if (debug_commandir == 1) {
					do_gettimeofday(&tp);
					printk(KERN_INFO "Sending packet data "
					       "at %d\n", (int)tp.tv_usec);
				}
				ret = cmdir_write_queue(ourbuffers[nexttosend],
				      ourbufferlengths[nexttosend], NULL);
				if (ret <= 0) {
					/* send failed - the device is either
					 * unplugged or full
					 * nexttosend =
					 * 	(nexttosend + 1)
					 * 	% (QUEUELENGTH - 1); */
					send_status = SEND_IDLE;
					return 0; /*send_queue(); */
				} else
					nexttosend = (nexttosend + 1)
						     % (QUEUELENGTH - 1);
				return 1;
			} else {
				if (debug_commandir == 1) {
					do_gettimeofday(&tp);
					printk(KERN_INFO "Not time to send yet "
					       "- starting timer at %d.\n",
					       (int)tp.tv_usec);
					printk(KERN_INFO "Enabling timer.\n");
				}
				return 0; /* doesn't matter anymore */
			}
		} else {
			if (debug_commandir == 1) {
				do_gettimeofday(&tp);
				printk(KERN_INFO "No more data to send %d!\n",
				       (int)tp.tv_usec);
			}
			last_tx_sec = 0; /* reset our TX counters */
			last_tx_usec = 0;
			return 1; /* nothing more to send! */
		}
	} else {
		if (debug_commandir == 1)
			/* will try again on the callback */
			printk(KERN_INFO "Already sending\n");
		return 1;  /* then the timer shouldn't be running... */
	}
	return 0; /* should never get here... */
}


int wait_to_tx(int usecs)
{
	/* don't return until last_time + usecs has been reached
	 * for non-zero last_tx's. */
	int wait_until_sec = 0, wait_until_usec = 0;
	int now_sec = 0, now_usec = 0;
	if (debug_commandir == 1)
		printk(KERN_INFO "waittotx(%d)\n", usecs);
	if (usecs == 0)
		return 0;

	if (!(last_tx_sec == 0 && last_tx_usec == 0)) {
		/* calculate wait time: */
		wait_until_sec = last_tx_sec + (usecs / 1000000);
		wait_until_usec = last_tx_usec + usecs;

		do_gettimeofday(&tp);
		now_sec = tp.tv_sec;
		now_usec = tp.tv_usec;

		if (wait_until_usec > 1000000) {
			/* we've spilled over to the next second. */
			wait_until_sec++;
			wait_until_usec -= 1000000;
			/* printk(KERN_INFO "usec rollover\n"); */
		}
		if (debug_commandir == 1)
			printk(KERN_INFO "Testing for the right second, now = "
			       "%d %d, wait = %d %d\n",
			       now_sec, now_usec,
			       wait_until_sec, wait_until_usec);
		/* now we are always on the same second. */
		if (now_sec > wait_until_sec) {
			if (debug_commandir == 1)
				printk(KERN_INFO "Setting last_tx_sec to %d.\n",
				       wait_until_sec);
			last_tx_sec = wait_until_sec;
			last_tx_usec = wait_until_usec;
			return 0;
		}

		if ((now_sec == wait_until_sec)
		    && (now_usec > wait_until_usec)) {
			if (debug_commandir == 1)
				printk(KERN_INFO "Setting last_tx_sec to %d.\n",
				       wait_until_sec);
			last_tx_sec = wait_until_sec;
			last_tx_usec = wait_until_usec;
			return 0;
		}
		return -1; /* didn't send */
	}

	do_gettimeofday(&tp);
	last_tx_usec = tp.tv_usec;
	last_tx_sec = tp.tv_sec;
	return 0; /* if there's no last even, go ahead and send */
}


int cmdir_write_queue(unsigned char *buffer, int count, void *callback_fct)
{
	int retval = count;
	static char prev_signal_num;
	unsigned char next_mask;
	unsigned int multiplier;
	int i;

	send_status = SEND_ACTIVE;

	if (count < 2) {
		err("Not enough bytes (write request of %d bytes)", count);
		return count;
	}

	/* check data; decide which device to send to */
	switch (buffer[0]) {
	case TX_HEADER:
	case TX_HEADER_NEW:
		/* this is LIRC transmit data */
		if (curTXFill >= 190) {
			printk(KERN_INFO
			       "TX buffer too full to send more TX data\n");
			return 0;
		}
		if (next_transmitters != transmitters) {
			if (buffer[1] != prev_signal_num)
				/* this is new signal; change transmitter mask*/
				transmitters = next_transmitters;
		}
		prev_signal_num = buffer[1];

		multiplier = 1;
		for (i = 0; i < MAX_DEVICES; i++) {
			next_mask = 0;
			if (transmitters & (0x01*multiplier))
				next_mask |= TX1_ENABLE;
			if (transmitters & (0x02*multiplier))
				next_mask |= TX2_ENABLE;
			if (transmitters & (0x04*multiplier))
				next_mask |= TX3_ENABLE;
			if (transmitters & (0x08*multiplier))
				next_mask |= TX4_ENABLE;

			if (next_mask > 0) {
				buffer[1] = next_mask;
				retval = write_core(buffer, count,
					 callback_fct, i);
				if (retval != count) {
					if (retval == -ENODEV)
						err("Device %d not plugged in",
						    i);
					else
						err("Write error to device %d",
						    i);
					return retval;
				}
			}
			multiplier = multiplier*0x10;
		}
		return retval;
		break;
	case LCD_HEADER:
		return write_core(buffer, count, callback_fct, lcd_device);
		break;
	default:
		return write_core(buffer, count, callback_fct, def_device);
		break;
	}
	/* should never get here */
	return retval;

}

int write_core(unsigned char *buffer, int count,
	       void *callback_fct, int device_num)
{
	struct usb_skel *dev;
	int retval = count;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
	ssize_t bytes_written = 0;
	if (minor_table[device_num] == NULL)
		/* device is unplugged */
		return -ENODEV;
	dev = minor_table[device_num];

	/* lock this object */
	down(&dev->sem);

	/* see if we are already in the middle of a write */
	if (dev->write_urb->status == -EINPROGRESS) {
		/*suppress errors - should just try sending again*/
		/*dbg(" - already writing");*/
		retval = -EINPROGRESS;
		goto exit;
	}

	/* we can only write as much as 1 urb will hold */
	bytes_written = (count > dev->bulk_out_size) ?
				dev->bulk_out_size : count;

	/* copy the data into our urb */
	if (!(memcpy(dev->write_urb->transfer_buffer, buffer, bytes_written))) {
		retval = -EFAULT;
		goto exit;
	}

	/* set up our urb */
	if (callback_fct == NULL) {
		/*FILL_BULK_URB(dev->write_urb, dev->udev,
			usb_sndbulkpipe(dev->udev, dev->bulk_out_endpointAddr),
			dev->write_urb->transfer_buffer, bytes_written,
			cmdir_write_bulk_callback, dev); */
		usb_fill_bulk_urb(dev->write_urb, dev->udev,
			usb_sndbulkpipe(dev->udev, dev->bulk_out_endpointAddr),
			dev->write_urb->transfer_buffer, bytes_written,
			cmdir_write_bulk_callback, dev);
	} else {
		FILL_BULK_URB(dev->write_urb, dev->udev,
			usb_sndbulkpipe(dev->udev, dev->bulk_out_endpointAddr),
			dev->write_urb->transfer_buffer, bytes_written,
			callback_fct, dev);
	}

	/* send the data out the bulk port */
	retval = usb_submit_urb(dev->write_urb);
	if (!(retval))
		retval = bytes_written;
	/* suppress errors */
	/* else {
		err("Failed submitting write urb, error %d", retval);
	} */
exit:
	/* unlock the device */
	up(&dev->sem);
	return retval;  /* this should be return error I think */

#else
	struct usb_interface *interface;
	struct urb *urb = NULL;
	char *buf = NULL;
	interface = usb_find_interface(&cmdir_driver,
			USB_CMDIR_MINOR_BASE + device_num);
	if (!interface) {
		/* also check without adding base, for devfs */
		interface = usb_find_interface(&cmdir_driver, device_num);
		if (!interface)
			return -ENODEV;
	}
	dev = usb_get_intfdata(interface);
	if (!dev)
		return -ENODEV;
	/* create a urb, and a buffer for it, and copy the data to the urb */
	urb = usb_alloc_urb(0, GFP_ATOMIC);	/* Now -=Atomic=- */
	if (!urb) {
		retval = -ENOMEM;
		goto error;
	}
	buf = usb_buffer_alloc(dev->udev, count,
		GFP_KERNEL, &urb->transfer_dma);
	if (!buf) {
		retval = -ENOMEM;
		goto error;
	}
	if (!memcpy(buf, buffer, count)) {
		retval = -EFAULT;
		goto error;
	}
	/* initialize the urb properly */
	if (callback_fct == NULL) {
		usb_fill_bulk_urb(urb, dev->udev,
			  usb_sndbulkpipe(dev->udev,
				dev->bulk_out_endpointAddr),
			  buf, count, (void *) cmdir_write_bulk_callback, dev);
	} else {
		usb_fill_bulk_urb(urb, dev->udev,
			  usb_sndbulkpipe(dev->udev,
				dev->bulk_out_endpointAddr),
			  buf, count, callback_fct, dev);
	}
	urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;  /* double check this */

	/* send the data out the bulk port */
	retval = usb_submit_urb(urb, GFP_KERNEL);
	if (retval) {
		err("%s - failed submitting write urb, error %d",
		    __FUNCTION__, retval);
		goto error;
	}

	/* release our reference to this urb, the USB
	 * core will eventually free it entirely */
	usb_free_urb(urb);
	return count;

error:
	usb_buffer_free(dev->udev, count, buf, urb->transfer_dma);
	usb_free_urb(urb);
	return retval;
#endif
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
static void cmdir_write_bulk_callback(struct urb *urb)
{
	if (debug_commandir == 1) {
		do_gettimeofday(&tp);
		printk(KERN_INFO "cmdir_write_bulk_callback at %d\n",
		       (int)tp.tv_usec);
	}
	/*if ((urb->status != -ENOENT) && (urb->status != -ECONNRESET))
		return;
	else {
		dbg(" - urb status: %d", urb->status);
		return;
	}*/

	send_status = SEND_IDLE;
	/* printk(KERN_INFO "cmdir_write_bulk_callback - set idle\n"); */
	send_queue(); /* send the next packet */
	return;
}
#else
static void cmdir_write_bulk_callback(struct urb *urb, struct pt_regs *regs)
{
	struct usb_skel *dev;
	dev = (struct usb_skel *)urb->context;
	send_status = SEND_IDLE;
	if (debug_commandir == 1)
		printk(KERN_INFO "callback()\n");
	/* free up our allocated buffer */

	usb_buffer_free(urb->dev, urb->transfer_buffer_length,
			urb->transfer_buffer, urb->transfer_dma);
	send_queue(); /* send the next packet */

}
#endif

int set_tx_channels(unsigned int next_tx)
{
	next_transmitters = next_tx;
	return 0;
}
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
EXPORT_SYMBOL_NOVERS(set_tx_channels);
#else
EXPORT_SYMBOL(set_tx_channels);
#endif

module_init(usb_cmdir_init);
module_exit(usb_cmdir_exit);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
