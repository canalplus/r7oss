#ifndef __LINUX_HSET_USB_H
#define __LINUX_HSET_USB_H

#ifdef __KERNEL__

/* Structure to hold all of our controller specific stuff */
struct usb_hset {
	struct usb_device	*udev;	/* the usb device for this device */
	struct usb_interface	*interface; /* the interface for this device */
	struct kref	kref;
	struct xhci_hcd	*xhci; /* the controller */
	struct ehci_hcd *ehci; /* the controller */
};

static int xhci_hset_debugfs_create(const char *name, struct usb_hset *hset);
static int xhci_hset_show(struct seq_file *s, void *unused);
static ssize_t xhci_hset_write(struct file *file, const char __user *ubuf, size_t count, loff_t *ppos);
void xhci_set_test_mode(struct usb_hset *hset,int mode);
void stm_init_xhci_hset(const char *name, struct usb_hcd *hcd);
void stm_exit_xhci_hset(const char *name, struct usb_hcd *hcd);
void stm_init_ehci_hset(const char *name, struct usb_hcd *hcd);
void stm_exit_ehci_hset(const char *name, struct usb_hcd *hcd);
extern struct usb_hset* xhci_init_hset_dev(struct usb_hcd *controller);
extern struct usb_hub *usb_hub_to_struct_hub(struct usb_device *hdev);

#endif  /* __KERNEL__ */

#endif