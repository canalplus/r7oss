/**
 * HS USB Host-mode HSET driver for DWC3 USB Controller
 *
 * Copyright (C) 2013 STMicroelectronics.
	Aymen Bouattay <aymen.bouattay at st.com>
	Abdelhamid Zouid <abdelhamid.zouid at st.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2.
 *
 * This driver is based on the USB Skeleton driver-2.0
 * (drivers/usb/usb-skeleton.c) written by Greg Kroah-Hartman (greg at kroah.com)
 *
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kref.h>
#include <linux/usb.h>
#include <../core/hub.h>
#include <linux/timer.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <asm/uaccess.h>
#include <linux/workqueue.h>
#include <linux/usb/hcd.h>
#include <linux/fs.h>
#include <linux/usb/hset-usb.h>
#include <../host/xhci.h>
#include <linux/usb/ch9.h>
#include <linux/usbdevice_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <core.h>


#define to_hset_dev(d) container_of(d, struct usb_hset, kref)
#define XHCI_USB2_TEST_MODE_MASK (0xf << 28)
#define XHCI_USB2_DEVICE_SLOT_DISABLE_MASK (0xff)
static struct usb_hset *the_xhci_hset;

/* hset handshake function copied from xhci.c*/
static int hset_handshake(struct xhci_hcd *xhci, void __iomem *ptr,
		      u32 mask, u32 done, int usec)
{
	u32	result;

	do {
		result = xhci_readl(xhci, ptr);
		if (result == ~(u32)0)		/* card removed */
			return -ENODEV;
		result &= mask;
		if (result == done)
			return 0;
		udelay(1);
		usec--;
	} while (usec > 0);
	return -ETIMEDOUT;
}

/*---------------------------------------------------------------------------*/
/* Test routines */

void xhci_set_test_mode(struct usb_hset *hset,int mode)
{
	u32			device_slot_reg;
	u32			port_status_base_reg;
	u32			command;
	u32			port_power_base_reg;
	u32			test_reg;

	/** step 1:Disable device slot */
	spin_lock_irq(&hset->xhci->lock);
	/*read the CONFIG register*/
	device_slot_reg = xhci_readl(hset->xhci, &hset->xhci->op_regs->config_reg);
	/*set max device slot number to 0*/
	device_slot_reg &= ~XHCI_USB2_DEVICE_SLOT_DISABLE_MASK;
	xhci_writel(hset->xhci, device_slot_reg, &hset->xhci->op_regs->config_reg);

	/** step 3: Set the Run/Stop (R/S) bit in the USBCMD register to a 0 and wait for the HCHalted (HCH) bit in
	the USBSTS register, to transition to a 1. Note that an xHC implementation shall not allow port
	testing with the R/S bit set to a 1 */
	command = xhci_readl(hset->xhci, &hset->xhci->op_regs->command);
	command &= ~CMD_RUN;
	xhci_writel(hset->xhci, command, &hset->xhci->op_regs->command);
	if (hset_handshake(hset->xhci, &hset->xhci->op_regs->status,
		      STS_HALT, STS_HALT, 100*100)) {
		xhci_warn(hset->xhci, "WARN: xHC CMD_RUN timeout\n");
		spin_unlock_irq(&hset->xhci->lock);
	}
	
	/* read the port status and control PORTSC */
	port_status_base_reg = xhci_readl(hset->xhci, &hset->xhci->op_regs->port_status_base);
	/* set PP flag to 0 */ 
	port_status_base_reg &= ~PORT_POWER; 
	xhci_writel(hset->xhci, port_status_base_reg, &hset->xhci->op_regs->port_status_base);
		
	/** step 4: Set the Port Test Control field in the port under test PORTPMSC register to the value
	corresponding to the desired test mode */
	/* read the port test mode value */
	port_power_base_reg = xhci_readl(hset->xhci, &hset->xhci->op_regs->port_power_base);
	/* clear test mode reserved bits 28-31 */
	port_power_base_reg &= ~XHCI_USB2_TEST_MODE_MASK; 
	switch (mode) {
	  case TEST_J:
	  case TEST_K:
	  case TEST_SE0_NAK:
	  case TEST_PACKET:
	    port_power_base_reg |= mode << 28;
	    xhci_writel(hset->xhci, port_power_base_reg, &hset->xhci->op_regs->port_power_base);
	    port_power_base_reg = xhci_readl(hset->xhci, &hset->xhci->op_regs->port_power_base);
	    port_power_base_reg &= ~XHCI_USB2_TEST_MODE_MASK; 	
	    port_power_base_reg |= mode << 28;
	    xhci_writel(hset->xhci, port_power_base_reg, &hset->xhci->op_regs->port_power_base);
	  break;
	  case TEST_FORCE_EN:
	    port_power_base_reg |= mode << 28;
	    xhci_writel(hset->xhci, port_power_base_reg, &hset->xhci->op_regs->port_power_base);
	    /* for force enable test restore R/S bit to 1 */
	    command = xhci_readl(hset->xhci, &hset->xhci->op_regs->command);
	    command |= CMD_RUN;
	    xhci_writel(hset->xhci, command, &hset->xhci->op_regs->command);
	    hset_handshake(hset->xhci, &hset->xhci->op_regs->status, STS_HALT,
		0, 250 * 1000);
	    printk ("xhci USBCMD R/S REG value %#x\n",command);
	  break;
	}

	test_reg = xhci_readl(hset->xhci, &hset->xhci->op_regs->port_power_base); /* just for debugging */
	printk ("xhci PRTPMSC1 REG value %#x\n",test_reg);
	spin_unlock_irq(&hset->xhci->lock);

/** TODO step 5: When the test is complete, if the xHC is running system software shall clear the R/S bit and ensure
the host controller is halted (HCHalted (HCH) bit is a 1) */

/** TODO step 6:Terminate and exit test mode by setting HCRST to a 1 */

}
EXPORT_SYMBOL_GPL(xhci_set_test_mode);

#ifdef CONFIG_PM
void xhci_hset_suspend(struct usb_hset *hset)
{
	struct usb_hcd *hcd;
	hcd=xhci_to_hcd(hset->xhci);
	printk(KERN_INFO "device suspended");
	xhci_bus_suspend (hcd);
}

void xhci_hset_resume(struct usb_hset *hset)
{
	struct usb_hcd *hcd;
	hcd=xhci_to_hcd(hset->xhci);
	printk(KERN_INFO "device resumed");
	xhci_bus_resume (hcd);
}
#else
#define hset_resume NULL
#define hset_suspend NULL
#endif	/* CONFIG_PM */

void xhci_test_single_step_get_dev_desc(struct usb_hset *hset)
{ 
	struct xhci_hcd *xhci = hset->xhci;
	struct usb_device *udev;	//= hset->udev;
	struct usb_bus *bus = hcd_to_bus(xhci_to_hcd(xhci));
	struct usb_device *root_hub;
	struct usb_hub *hub;
	
	printk("%s\n", __func__);
	root_hub = bus->root_hub;
	hub = usb_hub_to_struct_hub(root_hub);
	
	if (!xhci || !bus || !bus->root_hub) {
		printk(KERN_ERR "Host controller not ready!\n");
		return;
	}
	udev = hub->ports[0]->child;
	if (!udev) {
			printk(KERN_ERR "No device connected.\n");
			return;
	}
	usb_get_device_descriptor(udev, sizeof(struct usb_device_descriptor));
}

static inline int is_superspeed(struct usb_device *hdev)
{
	return (hdev->descriptor.bDeviceProtocol == USB_HUB_PR_SS);
}
static struct usb_hub *usb_hub;
void test_hub_port_connect_change(struct usb_hub *hub)
{
	usb_hub = hub;
	return;
}
static void xhci_test_single_step_set_feature(struct usb_hset *hset)
{
#if 0
	struct xhci_hcd *xhci = hset->xhci;
	struct usb_hcd *hcd = xhci->shared_hcd;
	struct usb_bus *bus = hcd_to_bus(hcd);
	struct usb_device *udev;
	struct usb_device *hdev;
	struct usb_device_descriptor *buf;
	int retval;
	int r = 0;

	printk(KERN_ERR "%s\n", __func__);
	if (!xhci || !hcd ) {
		printk(KERN_ERR "Host controller not ready!\n");
		return;
	}
	hdev = usb_hub->hdev;
        udev = usb_alloc_dev(hdev, hdev->bus, 1);
        if (!udev) {
                       printk(KERN_ERR "couldn't allocate port usb_device\n");
        }

               usb_set_device_state(udev, USB_STATE_POWERED);
               udev->bus_mA = usb_hub->mA_per_port;
               udev->level = hdev->level + 1;

               /* Only USB 3.0 devices are connected to SuperSpeed hubs. */
               if (is_superspeed(usb_hub->hdev))
                       udev->speed = USB_SPEED_SUPER;
               else
                       udev->speed = USB_SPEED_UNKNOWN;

    // hdev->children[0] = udev;
	hdev->hub->ports[0]->child = udev;
		udev->ep0.desc.wMaxPacketSize = cpu_to_le16(512);
	/* Set up TT records, if needed  */
	if (hdev->tt) {
		udev->tt = hdev->tt;
		udev->ttport = hdev->ttport;
	} else if (udev->speed != USB_SPEED_HIGH
			&& hdev->speed == USB_SPEED_HIGH) {
		if (!usb_hub->tt.hub) {
			printk(KERN_ERR "parent hub has no TT\n");
			retval = -EINVAL;
		}
		udev->tt = &usb_hub->tt;
		udev->ttport = 1;
	}
	usb_set_device_state(udev, USB_STATE_DEFAULT);
	buf = kmalloc(64, GFP_NOIO);
	if (!buf) {
		return;
	}
	if (udev->state != USB_STATE_DEFAULT)
		return;
	retval = hcd->driver->enable_device(hcd, udev);
	if (retval < 0)
		printk(KERN_ERR "hub device enable fails\n");

  /* Retry on all errors; some devices are flakey.
	 * 255 is for WUSB devices, we actually need to use
	 * 512 (WUSB1.0[4.8.1]).*/


		buf->bMaxPacketSize0 = 0;
		r = usb_control_msg(udev, usb_rcvaddr0pipe(),
			USB_REQ_GET_DESCRIPTOR, USB_DIR_IN,
			USB_DT_DEVICE << 8, 0,
			buf, 64,
			USB_CTRL_GET_TIMEOUT);
			switch (buf->bMaxPacketSize0) {
			case 8: case 16: case 32: case 64: case 255:
				if (buf->bDescriptorType ==
					USB_DT_DEVICE) {
				r = 0;
				break;
			}
			/* FALL THROUGH */
			default:
				if (r == 0)
					r = -EPROTO;
			break;
			}
		udev->descriptor.bMaxPacketSize0 = buf->bMaxPacketSize0;
		kfree(buf);
#endif
       //hub_port_init(usb_hub_test, udev, 1, 0);

	/*udev = bus->root_hub->children[0];
	if (!udev) {
			printk(KERN_ERR "No device connected.\n");
			return;
	}
	usb_get_device_descriptor(udev, sizeof(struct usb_device_descriptor));*/

}
static void test_hot_reset(struct usb_hset *hset)
{
	struct xhci_hcd *xhci = hset->xhci;
	struct usb_hcd *hcd = xhci->shared_hcd;
	unsigned int		reg;
	__le32 __iomem *addr ;

	printk(KERN_ERR "%s\n", __func__);
	if (!xhci || !hcd ) {
		printk(KERN_ERR "Host controller not ready!\n");
		return;
	}
	
	spin_lock_irq(&xhci->lock);
	addr = &xhci->op_regs->port_status_base;
	addr += 4;
	
	printk("%p reg = 0x%x\n",addr,(unsigned int) xhci_readl(xhci, addr));
	
	reg = (unsigned int) xhci_readl(xhci, addr);
	reg |= (0x1 << 4);
	reg |= (0x1 << 9);
	reg &= ~(0x1 << 1);
	xhci_writel(xhci,reg , addr);
 	reg = (unsigned int) xhci_readl(xhci, addr);
 	reg |= (0x1 << 21);
 	xhci_writel(xhci,reg , addr);
	spin_unlock_irq(&xhci->lock);
	return;
	
}

static void test_U1_test(struct usb_hset *hset)
{
	struct xhci_hcd *xhci = hset->xhci;
	struct usb_hcd *hcd = xhci->shared_hcd;
	unsigned int		reg;
	__le32 __iomem *addr ;
	
	printk(KERN_ERR "%s\n", __func__);
	if (!xhci || !hcd ) {
		printk(KERN_ERR "Host controller not ready!\n");
		return;
	}
	spin_lock_irq(&xhci->lock);
	addr = &xhci->op_regs->port_power_base;
	addr += 4;
	
	printk("%p reg = 0x%x\n",addr,(unsigned int) xhci_readl(xhci, addr));
	
	reg = (unsigned int) xhci_readl(xhci, addr);
	reg &= 0xFFFF0000;
	reg |=0x7F;
	xhci_writel(xhci,reg , addr);
	spin_unlock_irq(&xhci->lock);
	return;
}

static void test_U2_test(struct usb_hset *hset)
{
	struct xhci_hcd *xhci = hset->xhci;
	struct usb_hcd *hcd = xhci->shared_hcd;
	unsigned int		reg;
	__le32 __iomem *addr ;
	
	printk(KERN_ERR "%s\n", __func__);
	if (!xhci || !hcd ) {
		printk(KERN_ERR "Host controller not ready!\n");
		return;
	}
	spin_lock_irq(&xhci->lock);
	addr = &xhci->op_regs->port_power_base;
	addr += 4;
	
	printk("%p reg = 0x%x\n",addr,(unsigned int) xhci_readl(xhci, addr));
	
	reg = (unsigned int) xhci_readl(xhci, addr);
	reg &= 0xFFFF0000;
	reg |=(0x7F << 8);
	xhci_writel(xhci,reg , addr);
	spin_unlock_irq(&xhci->lock);
	return;
}

static void test_U3_test(struct usb_hset *hset)
{
	struct xhci_hcd *xhci_hcd = hset->xhci;
	struct usb_hcd *hcd = xhci_hcd->shared_hcd;
	struct xhci_hcd	*xhci = hcd_to_xhci(hcd);
	
	printk(KERN_ERR "%s\n", __func__);
	if (!xhci || !hcd ) {
		printk(KERN_ERR "Host controller not ready!\n");
		return;
	}
	
	printk(KERN_INFO "device suspended");
	xhci_bus_suspend (hcd);

	return;
}

/*---------------------------------------------------------------------------*/

/* This function is called by stm_init_xhci_hset ().
 * stm_init_xhci_hset() is called by the controller driver during its init(),
 */
struct usb_hset* xhci_init_hset_dev(struct usb_hcd *controller)
{
	struct usb_hset *hset = NULL;
	hset = kmalloc(sizeof(*hset), GFP_KERNEL);
	if (hset == NULL) {
		return NULL;
	}
	memset(hset, 0x00, sizeof(*hset));
	hset->xhci = hcd_to_xhci (controller);
	kref_init(&hset->kref);
	the_xhci_hset = hset;
	return hset;
}

static void hset_delete(struct kref *kref)
{
	struct usb_hset *dev = to_hset_dev(kref);

	kfree (dev);
}

/*---------------------------------------------------------------------------*/

void stm_init_xhci_hset(const char *name, struct usb_hcd *hcd)
{
	struct usb_hset *hset;
	hset = xhci_init_hset_dev(hcd);
	xhci_hset_debugfs_create(name, hset);
}

void stm_exit_xhci_hset(const char *name, struct usb_hcd *hcd)
{
	struct usb_hset *hset = the_xhci_hset;

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
				"N: Test_SE0_NAK\n"
				"P: Test_PACKET\n"
				"F: Test_FORCE_ENABLE\n"
				"S: Test_SUSPEND_RESUME\n"
				"G: Test_SINGLE_STEP_GET_DESC\n"
				"I: Test_SINGLE_STEP_SET_FEATURE\n"
				"E: Enumerate bus\n"
				"D: Suspend bus\n"
				"R: Resume bus\n"
				"H: Hot Reset\n"
				"1: U1 power test\n"
				"2: U2 power test\n"
				"3: U3 power test\n"
				"?: help menu\n");
	if (count < 0)
		return count;
	buffer += count;
	return count;
}

static int xhci_hset_show(struct seq_file *s, void *unused)
{
	return 0;
}

static ssize_t xhci_hset_write(struct file *file,
		const char __user *ubuf, size_t count, loff_t *ppos)
{
	struct seq_file		*s = file->private_data;
	char cmd;
	struct usb_hset *hset = s->private;
	char help[500];
	
	if(copy_from_user(&cmd,ubuf, 1))
		return -EFAULT;
		
		switch (cmd) {
	case 'J':
		xhci_set_test_mode(hset,TEST_J);
		break;
	case 'K':
		xhci_set_test_mode(hset,TEST_K);
		break;
	case 'N':
		xhci_set_test_mode(hset,TEST_SE0_NAK);
		break;
	case 'P':
		xhci_set_test_mode(hset,TEST_PACKET);
		break;
	case 'F':
		xhci_set_test_mode(hset,TEST_FORCE_EN);
		break;
 	case 'S':
 		xhci_hset_suspend(hset);
		msleep(15000);	/* wait 15 sec */
		xhci_hset_resume(hset);
 		break;
	case 'G':
		xhci_test_single_step_get_dev_desc(hset);
		break;
	case 'I':
		xhci_test_single_step_set_feature(hset);
		break;
// 	case 'E':
// 		schedule_work(&enumerate);
// 		break;
#ifdef CONFIG_PM
	case 'D':
		xhci_hset_suspend(hset);
		break;
	case 'R':
		xhci_hset_resume(hset);
		break;
#endif		
	case 'H':
		test_hot_reset(hset);
		break;
	case '1':
		test_U1_test(hset);
		break;
	case '2':
		test_U2_test(hset);
		break;
	case '3':
		test_U3_test(hset);
		break;
	case '?':
		dump_menu(help);
		printk(KERN_INFO "%s", help);
		break;
	default:
		printk(KERN_ERR "Command %c not implemented\n", cmd);
		break;
	}
	return count;
}

static int xhci_hset_open(struct inode *inode, struct file *file)
{
		return single_open(file, xhci_hset_show, inode->i_private);  
}
static const struct file_operations hset_fops = {
	.open = xhci_hset_open,
	.write = xhci_hset_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int xhci_hset_debugfs_create(const char *name, struct usb_hset *hset)
{
	int	ret = 0;
	struct dentry	*file;
	
	if (!name || !hset)
		return -ENOMEM;

	file = debugfs_create_file(name, S_IRUGO | S_IWUSR, NULL,
				hset, &hset_fops);
	if (!file) {
		ret = -ENOMEM;
	}
	return ret;
}
