/*
 * HS USB Host-mode HSET driver for Mentor USB Controller
 *
 * Copyright (C) 2005 Mentor Graphics Corporation
 * Copyright (C) 2007 Texas Instruments, Inc.
 *      Nishant Kamat <nskamat at ti.com>
 *      Vikram Pandita <vikram.pandita at ti.com>
 * Copyright (C) 2013 STMicroelectronics.
 *	Aymen Bouattay <aymen.bouattay at st.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2.
 *
 * This driver is based on the USB Skeleton driver-2.0
 * (drivers/usb/usb-skeleton.c) written by Greg Kroah-Hartman (greg at kroah.com)
 *
 * Notes:
 * There are 2 ways this driver can be used:
 *	1 - By attaching a HS OPT (OTG Protocol Tester) card.
 * 	    The OPT test application contains scripts to test each mode.
 *	    Each script attaches the OPT with a distinct VID/PID. Depending on
 *	    the VID/PID this driver go into a particular test mode.
 *	2 - Through /proc/drivers/musb_HSET interface.
 *	    This is a forceful method, and rather more useful.
 *	    Any USB device can be attached to the Host.
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kref.h>
#include <linux/usb.h>
#include <linux/timer.h>
#include <linux/seq_file.h>
#include <linux/workqueue.h>
#include <linux/usb/hcd.h>
#include <linux/usb/hset-usb.h>
#include <linux/usb/ch9.h> 
#include <linux/usbdevice_fs.h> 
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <asm-generic/uaccess.h>
#include "../drivers/usb/core/usb.h"
#include <../drivers/usb/host/ehci.h>

/*---------------------------------------------------------------------------*/
/* This is the list of VID/PID that the HS OPT card will use. */
static struct usb_device_id hset_table [] = {
	{ USB_DEVICE(0x1a0a, 0x0101) },	/* TEST_SE0_NAK */
	{ USB_DEVICE(0x1a0a, 0x0102) },	/* TEST_J */
	{ USB_DEVICE(0x1a0a, 0x0103) },	/* TEST_K */
	{ USB_DEVICE(0x1a0a, 0x0104) },	/* TEST_PACKET */
	{ USB_DEVICE(0x1a0a, 0x0105) },	/* TEST_FORCE_ENABLE */
	{ USB_DEVICE(0x1a0a, 0x0106) },	/* HS_HOST_PORT_SUSPEND_RESUME */
	{ USB_DEVICE(0x1a0a, 0x0107) },	/* SINGLE_STEP_GET_DEV_DESC */
	{ USB_DEVICE(0x1a0a, 0x0108) }, /* SINGLE_STEP_SET_FEATURE */
	{ } /* Terminating entry */
};
MODULE_DEVICE_TABLE (usb, hset_table);


#define to_hset_dev(d) container_of(d, struct usb_hset, kref)

static struct usb_hset *the_ehci_hset;
static struct usb_driver hset_driver;
static int ehci_hset_debugfs_create(const char *name, struct usb_hset *hset);
extern void xhci_set_test_mode(struct usb_hset *hset,int mode);
extern void xhci_hset_suspend(struct usb_hset *hset);
extern void xhci_hset_resume(struct usb_hset *hset);
extern void xhci_test_single_step_get_dev_desc(struct usb_hset *hset);
/*---------------------------------------------------------------------------*/
/* Test routines */

static inline void ehci_test_se0_nak(struct usb_hset *hset)
{
	unsigned int temp;

	  ehci_info (hset->ehci, "Testing se0 \n");
	  temp =readl (&hset->ehci->regs->port_status[0]) ;
	  writel (temp|PORT_TEST(3),&hset->ehci->regs->port_status[0]);
	  ehci_info (hset->ehci, "EHCI_Portsc %#x\n",hset->ehci->regs->port_status[0]);
}

static inline void ehci_test_j(struct usb_hset *hset)
{
	  struct usb_hcd *hcd = ehci_to_hcd(hset->ehci);
	  unsigned int temp;

	  ehci_info (hset->ehci, "Testing j State\n");
 
	  ehci_bus_suspend (hcd);

	  temp =readl (&hset->ehci->regs->port_status[0]) ;
	  writel (temp|PORT_TEST(1),&hset->ehci->regs->port_status[0]);
	  ehci_info (hset->ehci, "EHCI_Portsc %#x\n",hset->ehci->regs->port_status[0]);
}

static inline void ehci_test_k(struct usb_hset *hset)
{
	struct usb_hcd *hcd = ehci_to_hcd(hset->ehci);
	unsigned int temp;

	ehci_info (hset->ehci, "Testing k State\n");
	ehci_bus_suspend (hcd);

	temp =readl (&hset->ehci->regs->port_status[0]) ;
	writel (temp|PORT_TEST(2),&hset->ehci->regs->port_status[0]);
	ehci_info (hset->ehci, "EHCI_Portsc %#x\n",hset->ehci->regs->port_status[0]);
}

static inline void ehci_test_packet(struct usb_hset *hset)
{
	
	struct usb_hcd *hcd = ehci_to_hcd(hset->ehci);
	unsigned int temp;

	ehci_info (hset->ehci, "Test packet\n");
	ehci_bus_suspend (hcd);
	temp =readl (&hset->ehci->regs->port_status[0]) ;
	writel (temp|PORT_TEST(4),&hset->ehci->regs->port_status[0]);
	ehci_info (hset->ehci, "EHCI_Portsc %#x\n",hset->ehci->regs->port_status[0]);
}

static inline void ehci_test_force_enable(struct usb_hset *hset)
{
	unsigned int temp;

	ehci_info (hset->ehci, "Testing force enable\n");
	temp =readl (&hset->ehci->regs->port_status[0]) ;
	writel (temp|PORT_TEST(5),&hset->ehci->regs->port_status[0]);
	ehci_info (hset->ehci, "EHCI_Portsc %#x\n",hset->ehci->regs->port_status[0]);
}

static void ehci_hset_suspend(struct usb_hset *hset)
{
	struct usb_hcd *hcd = ehci_to_hcd(hset->ehci);
	ehci_info (hset->ehci, "Testing device suspend\n");
	printk(KERN_INFO "device suspended");
	ehci_bus_suspend (hcd);
}

static void ehci_hset_resume(struct usb_hset *hset)
{
	struct usb_hcd *hcd = ehci_to_hcd(hset->ehci);
	ehci_info (hset->ehci, "Testing device resume\n");
	printk(KERN_INFO "device resuming");
	ehci_bus_resume (hcd);
}

static void ehci_test_suspend_resume(struct usb_hset *hset)
{
	struct usb_hcd *hcd = ehci_to_hcd(hset->ehci);
	
	printk(KERN_INFO "%s\n", __FUNCTION__);
	printk(KERN_INFO "suspending the device after 15 seconds");
	msleep(15000);
	ehci_bus_suspend(hcd);
	printk(KERN_INFO "device suspended, sleeping 15 seconds");
	msleep(15000);	/* Wait for 15 sec */
	ehci_bus_resume(hcd);
	printk (KERN_INFO "device resuming");
}

static int ehci_test_single_step_get_dev_desc(struct usb_hset *hset)
{ 
	struct ehci_hcd *ehci = hset->ehci;
	struct usb_hcd *hcd = ehci_to_hcd(hset->ehci);
	struct usb_bus *bus = hcd_to_bus(hcd);
	struct list_head  qtd_list;
	struct list_head  test_list;
	struct usb_device          *dev;
	struct ehci_qtd            *qtd;
	struct urb                 *HSET_urb;
	struct usb_ctrlrequest     setup_packet;
	char       data_buffer[USB_DT_DEVICE_SIZE];

	ehci_info(ehci, "Testing SINGLE_STEP_GET_DEV_DESC\n");

	if (bus == NULL) {
		ehci_err(ehci, "EHSET: usb_bus pointer is NULL\n");
			return -EPIPE;
	}

	dev = bus->root_hub;
	if (dev == NULL) {
		ehci_err(ehci, "EHSET: root_hub pointer is NULL\n");
			return -EPIPE;
	}

	ehci_info(ehci, "Wait for 15s\n");
	set_current_state(TASK_UNINTERRUPTIBLE);
	schedule_timeout(msecs_to_jiffies(15000));

	setup_packet.bRequestType = USB_DIR_IN;
	setup_packet.bRequest = USB_REQ_GET_DESCRIPTOR;
	setup_packet.wValue = (USB_DT_DEVICE << 8);
	setup_packet.wIndex = 0;
	setup_packet.wLength = USB_DT_DEVICE_SIZE;

	INIT_LIST_HEAD(&qtd_list);
	INIT_LIST_HEAD(&test_list);
	HSET_urb = usb_alloc_urb(0, GFP_KERNEL);
	HSET_urb->transfer_buffer_length = USB_DT_DEVICE_SIZE;
	HSET_urb->dev = dev;
	HSET_urb->pipe = usb_rcvctrlpipe(dev, 0);
	HSET_urb->ep = &dev->ep0;
	HSET_urb->hcpriv = dev->ep0.hcpriv;
	HSET_urb->setup_packet = (char *)&setup_packet;
	HSET_urb->transfer_buffer = data_buffer;
	HSET_urb->transfer_flags = URB_HCD_DRIVER_TEST;
	HSET_urb->setup_dma = dma_map_single(
	hcd->self.controller,
	HSET_urb->setup_packet,
	sizeof(struct usb_ctrlrequest),
	DMA_TO_DEVICE);
	HSET_urb->transfer_dma = dma_map_single(
	hcd->self.controller,
	HSET_urb->transfer_buffer,
	sizeof(struct usb_ctrlrequest),
	DMA_TO_DEVICE);
	if (!HSET_urb->setup_dma || !HSET_urb->transfer_dma) {
	      ehci_err(ehci, "dma_map_single Failed\n");
	      return -EPIPE;
	}

	if (!qh_urb_transaction(ehci, HSET_urb, &qtd_list, GFP_ATOMIC)) {
	      ehci_err(ehci, "qh_urb_transaction Failed\n");
	      return -EPIPE;
	}

	qtd =  container_of(qtd_list.next, struct ehci_qtd, qtd_list);
	list_del_init(&qtd->qtd_list);
	list_add(&qtd->qtd_list, &test_list);
	qtd = container_of(qtd_list.next, struct ehci_qtd, qtd_list);
	list_del_init(&qtd->qtd_list);
	list_add_tail(&qtd->qtd_list, &test_list);
	qtd = container_of(qtd_list.next, struct ehci_qtd, qtd_list);
	list_del_init(&qtd->qtd_list);
	ehci_qtd_free(ehci, qtd);

	ehci_info(ehci, "Sending SETUP&DATA PHASE\n");
	if (submit_async(ehci, HSET_urb, &test_list, GFP_ATOMIC)) {
	      ehci_err(ehci, "Failed to queue up qtds\n");
	      return -EPIPE;
	}
	return 0;
}

static int ehci_test_single_step_set_feature(struct usb_hset *hset)
{
	struct ehci_hcd *ehci = hset->ehci;
	struct usb_hcd *hcd = ehci_to_hcd(hset->ehci);
	struct usb_bus *bus = hcd_to_bus(hcd); 
	struct list_head  qtd_list;
	struct list_head  setup_list;
	struct list_head  data_list;
	struct usb_device *dev;
	struct ehci_qtd *qtd;
	struct urb *HSET_urb;
	struct usb_ctrlrequest setup_packet;
	char data_buffer[USB_DT_DEVICE_SIZE];

	ehci_info(ehci, "Testing SINGLE_STEP_SET_FEATURE\n");
	if (bus == NULL) {
		ehci_err (ehci, "EHSET: usb_bus pointer is NULL\n");
			return -EPIPE;
	}

	dev = bus->root_hub;
	if (dev == NULL) {
		ehci_err(ehci, "EHSET: root_hub pointer is NULL\n");
			return -EPIPE;
	}

	setup_packet.bRequestType = USB_DIR_IN;
	setup_packet.bRequest = USB_REQ_GET_DESCRIPTOR;
	setup_packet.wValue = (USB_DT_DEVICE << 8);
	setup_packet.wIndex = 0;
	setup_packet.wLength = USB_DT_DEVICE_SIZE;

	INIT_LIST_HEAD(&qtd_list);
	INIT_LIST_HEAD(&setup_list);
	INIT_LIST_HEAD(&data_list);
	HSET_urb = usb_alloc_urb(0, GFP_KERNEL);
	HSET_urb->transfer_buffer_length =
	USB_DT_DEVICE_SIZE;

	HSET_urb->dev = dev;
	HSET_urb->pipe = usb_rcvctrlpipe(dev, 0);
	HSET_urb->ep = &dev->ep0;
	HSET_urb->hcpriv = dev->ep0.hcpriv;
	HSET_urb->setup_packet = (char *)&setup_packet;
	HSET_urb->transfer_buffer = data_buffer;
	HSET_urb->transfer_flags = URB_HCD_DRIVER_TEST;
	HSET_urb->setup_dma = dma_map_single(
				      hcd->self.controller,
				      HSET_urb->setup_packet,
				      sizeof(struct usb_ctrlrequest),
				      DMA_TO_DEVICE);
	HSET_urb->transfer_dma = dma_map_single(
				      hcd->self.controller,
				      HSET_urb->transfer_buffer,
				      sizeof (struct usb_ctrlrequest),
				      DMA_TO_DEVICE);
	if (!HSET_urb->setup_dma || !HSET_urb->transfer_dma) {
				      ehci_err (ehci, "dma_map_single Failed\n");
				      return -EPIPE;
	}

	if (!qh_urb_transaction(ehci, HSET_urb, &qtd_list,GFP_ATOMIC)) {
		ehci_err(ehci, "qh_urb_transaction Failed\n");
		return -EPIPE;
	}

	qtd = container_of(qtd_list.next,struct ehci_qtd, qtd_list);
	list_del_init (&qtd->qtd_list);
	list_add (&qtd->qtd_list, &setup_list);
	qtd = container_of (qtd_list.next,struct ehci_qtd, qtd_list);
	list_del_init(&qtd->qtd_list);
	list_add(&qtd->qtd_list, &data_list);
	qtd = container_of (qtd_list.next,struct ehci_qtd, qtd_list);
	list_del_init(&qtd->qtd_list);
	ehci_qtd_free(ehci, qtd);

	ehci_info(ehci, "Sending SETUP PHASE\n");
	if (submit_async(ehci, HSET_urb,&setup_list, GFP_ATOMIC)) {
		ehci_err(ehci, "Failed to queue up qtds\n");
		return -EPIPE;
	}
	ehci_info(ehci, "Wait for 15s\n");
	set_current_state(TASK_UNINTERRUPTIBLE);
	schedule_timeout(msecs_to_jiffies(15000));
	HSET_urb->status = 0;
	HSET_urb->actual_length = 0;

	ehci_info(ehci, "Sending DATA PHASE\n");
	if (submit_async(ehci, HSET_urb,&data_list, GFP_ATOMIC)) {
		ehci_err(ehci, "Failed to queue up qtds\n");
		return -EPIPE;
	}
	return 0;
}

static void enumerate_bus(struct work_struct *ignored)
{
	struct usb_hset *hset = the_ehci_hset;
	struct ehci_hcd *ehci = hset->ehci;
	struct usb_hcd *hcd = ehci_to_hcd(ehci);
	struct usb_device *udev;
	struct usb_bus *bus = hcd_to_bus(hcd);

	printk(KERN_INFO "%s\n", __FUNCTION__);
	if (!ehci || !bus || !bus->root_hub) {
		printk(KERN_ERR "Host controller not ready!\n");
		return;
	}
	if (udev)
		usb_reset_device(udev);
}

DECLARE_WORK(enumerate, enumerate_bus);

/*---------------------------------------------------------------------------*/

/* This function can be called either by musb_init_hset() or usb_hset_probe().
 * musb_init_hset() is called by the controller driver during its init(),
 * while usb_hset_probe() is called when an OPT is attached. We take care not
 * to allocate the usb_hset structure twice.
 */
static struct usb_hset* ehci_init_hset_dev(struct usb_hcd *controller)
{
	
	struct usb_hset *hset = NULL;
	hset = kmalloc(sizeof(*hset), GFP_KERNEL);
	if (hset == NULL) {
		printk("Out of memory\n");
		return NULL;
	}
	memset(hset, 0x00, sizeof(*hset));
	hset->ehci = (struct ehci_hcd *) (controller->hcd_priv);
	kref_init(&hset->kref);
	the_ehci_hset = hset;
	return hset;
}

static void hset_delete(struct kref *kref)
{
	struct usb_hset *dev = to_hset_dev(kref);

	kfree (dev);
}
/*---------------------------------------------------------------------------*/
/* Usage of HS OPT */

static inline struct ehci_hcd *dev_to_musb(struct device *dev)
{
#ifdef CONFIG_USB_HSET
        /* usbcore insists dev->driver_data is a "struct hcd *" */
        return (struct ehci_hcd *) (dev_get_drvdata(dev));
#else
        return dev_get_drvdata(dev);
#endif
}

#ifdef	CONFIG_PM
/* Called when the HS OPT is attached as a device */
static int hset_probe(struct usb_interface *interface,
		      const struct usb_device_id *id)
{
	struct usb_hset *hset;
	struct usb_device *udev;
	struct usb_hcd *hcd;
	int retval = -ENOMEM;
	
	udev = usb_get_dev(interface_to_usbdev(interface));
	hcd = bus_to_hcd(udev->bus);
	
	if (!hcd)
		return retval;
		
	
	
	if (strncmp(hcd->self.bus_name, "xhci", 4)==0) {
	  
	  hset = xhci_init_hset_dev(hcd);

	  if (!hset)
		return retval;

	  hset->udev = udev;
	  hset->interface = interface;
	  usb_set_intfdata(interface, hset);
	  
	  switch(id->idProduct) {
	    case 0x0101:
		xhci_set_test_mode(hset,TEST_SE0_NAK);
	    break;
	    case 0x0102:
		xhci_set_test_mode(hset,TEST_J);
	    break;
	    case 0x0103:
		xhci_set_test_mode(hset,TEST_K);
	    break;
	    case 0x0104:
		xhci_set_test_mode(hset,TEST_PACKET);
	    break;
	    case 0x0105:
		xhci_set_test_mode(hset,TEST_FORCE_EN);
	    break;
	    case 0x0106:
		xhci_hset_suspend(hset);
		msleep(15000);
		xhci_hset_resume(hset);
	    break;
	    case 0x0107:
		xhci_test_single_step_get_dev_desc(hset);
	    break;
	    case 0x0108:
		xhci_test_single_step_get_dev_desc(hset);
		msleep(15000);	/* SOFs for 15 sec */
		xhci_test_single_step_get_dev_desc(hset);
	    break;
	  };
	}
	
	if (strncmp(hcd->self.bus_name, "ehci", 4)==0) {
	  
	  hset = ehci_init_hset_dev(hcd);

	  if (!hset)
		return retval;

	  hset->udev = udev;
	  hset->interface = interface;
	  usb_set_intfdata(interface, hset);
	  
	  switch(id->idProduct) {
	    case 0x0101:
			ehci_test_se0_nak(hset);
	    break;
	    case 0x0102:
			ehci_test_j (hset);
	    break;
		  case 0x0103:
			ehci_test_k (hset);
		  break;
		  case 0x0104:
			ehci_test_packet (hset);
		  break;
		  case 0x0105:
			ehci_test_force_enable(hset);
		  break;
		  case 0x0106:
			ehci_test_suspend_resume (hset);
		  break;
		  case 0x0107:
			ehci_test_single_step_set_feature (hset);
		  break;
		  case 0x0108:
			ehci_test_single_step_get_dev_desc(hset);
			msleep(15000);	/* SOFs for 15 sec */
			ehci_test_single_step_set_feature(hset);
		  break;
		};

	}
	
	return 0;
}
#endif

static void hset_disconnect(struct usb_interface *interface)
{
	struct usb_hset *hset;

	/* prevent hset_open() from racing hset_disconnect() */
	hset = usb_get_intfdata(interface);
	usb_set_intfdata(interface, NULL);

	usb_put_dev(hset->udev);
	kref_put(&hset->kref, hset_delete);
}

static struct usb_driver hset_driver = {
	.name =		"hset",
	.probe =	hset_probe,
	.disconnect =	hset_disconnect,
	.id_table =	hset_table,
};

static int __init usb_hset_init(void)
{
	int result;
	/* register this driver with the USB subsystem */
	result = usb_register(&hset_driver);
	if (result)
		printk("usb_register failed. Error number %d", result);

	return result;
}

static void __exit usb_hset_exit(void)
{
	/* deregister this driver with the USB subsystem */
	usb_deregister(&hset_driver);
}

module_init (usb_hset_init);
module_exit (usb_hset_exit);

MODULE_LICENSE("GPL");

/*---------------------------------------------------------------------------*/

void stm_init_ehci_hset(const char *name, struct usb_hcd *hcd)
{
	struct usb_hset *hset;
	hset = ehci_init_hset_dev(hcd);
	ehci_hset_debugfs_create(name, hset);
}

void stm_exit_ehci_hset(const char *name, struct usb_hcd *hcd)
{
	struct usb_hset *hset = the_ehci_hset;

	if (!hset) {
		printk(KERN_DEBUG "No hset device!\n");
		return;
	}
	kref_put(&hset->kref, hset_delete);
}

static int dump_menu(char *buffer)
{
	int count = 0;

	*buffer = 0;
	count = sprintf(buffer, "HOST-side high-speed electrical test modes:\n"
				"J: Test_J\n"
				"K: Test_K\n"
				"S: Test_SE0_NAK\n"
				"P: Test_PACKET\n"
				"H: Test_FORCE_ENABLE\n"
				"U: Test_SUSPEND_RESUME\n"
				"G: Test_SINGLE_STEP_GET_DESC\n"
				"F: Test_SINGLE_STEP_SET_FEATURE\n"
				"E: Enumerate bus\n"
				"D: Suspend bus\n"
				"R: Resume bus\n"
				"?: help menu\n");
	if (count < 0)
		return count;
	buffer += count;
	return count;
}

static int proc_ehci_hset_show(struct seq_file *s, void *unused)
{
	return 0;
}
static ssize_t proc_ehci_hset_write(struct file *file,
		const char __user *ubuf, size_t count, loff_t *ppos)
{
	struct seq_file	*s = file->private_data;
	char cmd;
	struct usb_hset *hset = s->private;
	char help[500];
	
	if(copy_from_user(&cmd,ubuf, 1))
		return -EFAULT;
switch (cmd) {
	case 'S':
		ehci_test_se0_nak(hset);
		break;
	case 'J':
		ehci_test_j(hset);
		break;
	case 'K':
		ehci_test_k(hset);
		break;
	case 'P':
		ehci_test_packet(hset);
		break;
	case 'H':
		ehci_test_force_enable(hset);
		break;
	case 'U':
		ehci_test_suspend_resume(hset);
		break;
	case 'G':
		ehci_test_single_step_get_dev_desc(hset);
		break;
	case 'F':
		ehci_test_single_step_set_feature(hset);
		break;
	case 'E':
		schedule_work(&enumerate);
		break;
	case 'D':
		ehci_hset_suspend(hset);
		break;
	case 'R':
		ehci_hset_resume(hset);
		break;
	case '?':
		dump_menu(help);
		printk(KERN_INFO "%s", help);
		break;
	default:
		printk(KERN_ERR "Command %c not implemented\n", cmd);
		break;}
	return count;
}
static int proc_ehci_hset_open(struct inode *inode, struct file *file)
{
		return single_open(file, proc_ehci_hset_show, inode->i_private);  
}
static const struct file_operations hset_fops = {
	.open = proc_ehci_hset_open,
	.write = proc_ehci_hset_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int ehci_hset_debugfs_create(const char *name, struct usb_hset *hset)
{
	struct dentry	*file;
	int	ret = 0;

	if (!name || !hset)
		return NULL;

	file = debugfs_create_file(name, S_IRUGO | S_IWUSR, NULL,
				hset, &hset_fops);
	if (!file) {
		ret = -ENOMEM;
	}
	return ret;
}
