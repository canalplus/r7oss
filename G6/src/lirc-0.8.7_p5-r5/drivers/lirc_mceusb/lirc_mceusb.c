/*
 * LIRC driver for Windows Media Center Edition USB Infrared Transceivers
 *
 * (C) by Martin A. Blatter <martin_a_blatter@yahoo.com>
 *
 * Transmitter support and reception code cleanup.
 * (C) by Daniel Melander <lirc@rajidae.se>
 *
 * Original lirc_mceusb driver for 1st-gen device:
 * Copyright (c) 2003-2004 Dan Conti <dconti@acm.wwu.edu>
 *
 * Original lirc_mceusb driver deprecated in favor of this driver, which
 * supports the 1st-gen device now too. Transmit and receive support for
 * the 1st-gen device added June-September 2009,
 * by Jarod Wilson <jarod@wilsonet.com> and Patrick Calhoun <phineas@ou.edu>
 *
 * Derived from ATI USB driver by Paul Miller and the original
 * MCE USB driver by Dan Conti ((and now including chunks of the latter
 * relevant to the 1st-gen device initialization)
 *
 * This driver will only work reliably with kernel version 2.6.10
 * or higher, probably because of differences in USB device enumeration
 * in the kernel code. Device initialization fails most of the time
 * with earlier kernel versions.
 *
 **********************************************************************
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 5)
#error "*******************************************************"
#error "Sorry, this driver needs kernel version 2.6.5 or higher"
#error "*******************************************************"
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 33)
#include <linux/autoconf.h>
#endif
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kmod.h>
#include <linux/smp_lock.h>
#include <linux/completion.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 18)
#include <asm/uaccess.h>
#else
#include <linux/uaccess.h>
#endif
#include <linux/usb.h>
#include <linux/wait.h>
#include <linux/time.h>

#include "drivers/lirc.h"
#include "drivers/kcompat.h"
#include "drivers/lirc_dev/lirc_dev.h"

#define DRIVER_VERSION	"1.90"
#define DRIVER_AUTHOR	"Daniel Melander <lirc@rajidae.se>, " \
			"Martin Blatter <martin_a_blatter@yahoo.com>, " \
			"Dan Conti <dconti@acm.wwu.edu>"
#define DRIVER_DESC	"Windows Media Center Edition USB IR Transceiver " \
			"driver for LIRC"
#define DRIVER_NAME	"lirc_mceusb"

#define USB_BUFLEN	32	/* USB reception buffer length */
#define USB_CTRL_MSG_SZ	2	/* Size of usb ctrl msg on gen1 hw */
#define LIRCBUF_SIZE	256	/* LIRC work buffer length */

/* MCE constants */
#define MCE_CMDBUF_SIZE	384 /* MCE Command buffer length */
#define MCE_TIME_UNIT	50 /* Approx 50us resolution */
#define MCE_CODE_LENGTH	5 /* Normal length of packet (with header) */
#define MCE_PACKET_SIZE	4 /* Normal length of packet (without header) */
#define MCE_PACKET_HEADER 0x84 /* Actual header format is 0x80 + num_bytes */
#define MCE_CONTROL_HEADER 0x9F /* MCE status header */
#define MCE_TX_HEADER_LENGTH 3 /* # of bytes in the initializing tx header */
#define MCE_MAX_CHANNELS 2 /* Two transmitters, hardware dependent? */
#define MCE_DEFAULT_TX_MASK 0x03 /* Val opts: TX1=0x01, TX2=0x02, ALL=0x03 */
#define MCE_PULSE_BIT	0x80 /* Pulse bit, MSB set == PULSE else SPACE */
#define MCE_PULSE_MASK	0x7F /* Pulse mask */
#define MCE_MAX_PULSE_LENGTH 0x7F /* Longest transmittable pulse symbol */
#define MCE_PACKET_LENGTH_MASK  0x7F /* Packet length mask */


/* module parameters */
#ifdef CONFIG_USB_DEBUG
static int debug = 1;
#else
static int debug;
#endif
#define dprintk(fmt, args...)					\
	do {							\
		if (debug)					\
			printk(KERN_DEBUG fmt, ## args);	\
	} while (0)

/* general constants */
#define SEND_FLAG_IN_PROGRESS	1
#define SEND_FLAG_COMPLETE	2
#define RECV_FLAG_IN_PROGRESS	3
#define RECV_FLAG_COMPLETE	4

#define MCEUSB_RX		1
#define MCEUSB_TX		2

#define VENDOR_PHILIPS		0x0471
#define VENDOR_SMK		0x0609
#define VENDOR_TATUNG		0x1460
#define VENDOR_GATEWAY		0x107b
#define VENDOR_SHUTTLE		0x1308
#define VENDOR_SHUTTLE2		0x051c
#define VENDOR_MITSUMI		0x03ee
#define VENDOR_TOPSEED		0x1784
#define VENDOR_RICAVISION	0x179d
#define VENDOR_ITRON		0x195d
#define VENDOR_FIC		0x1509
#define VENDOR_LG		0x043e
#define VENDOR_MICROSOFT	0x045e
#define VENDOR_FORMOSA		0x147a
#define VENDOR_FINTEK		0x1934
#define VENDOR_PINNACLE		0x2304
#define VENDOR_ECS		0x1019
#define VENDOR_WISTRON		0x0fb8
#define VENDOR_COMPRO		0x185b
#define VENDOR_NORTHSTAR	0x04eb
#define VENDOR_REALTEK		0x0bda
#define VENDOR_TIVO		0x105a

static struct usb_device_id mceusb_dev_table[] = {
	/* Original Microsoft MCE IR Transceiver (often HP-branded) */
	{ USB_DEVICE(VENDOR_MICROSOFT, 0x006d) },
	/* Philips Infrared Transceiver - Sahara branded */
	{ USB_DEVICE(VENDOR_PHILIPS, 0x0608) },
	/* Philips Infrared Transceiver - HP branded */
	{ USB_DEVICE(VENDOR_PHILIPS, 0x060c) },
	/* Philips SRM5100 */
	{ USB_DEVICE(VENDOR_PHILIPS, 0x060d) },
	/* Philips Infrared Transceiver - Omaura */
	{ USB_DEVICE(VENDOR_PHILIPS, 0x060f) },
	/* Philips Infrared Transceiver - Spinel plus */
	{ USB_DEVICE(VENDOR_PHILIPS, 0x0613) },
	/* Philips eHome Infrared Transceiver */
	{ USB_DEVICE(VENDOR_PHILIPS, 0x0815) },
	/* Philips/Spinel plus IR transceiver for ASUS */
	{ USB_DEVICE(VENDOR_PHILIPS, 0x206c) },
	/* Philips/Spinel plus IR transceiver for ASUS */
	{ USB_DEVICE(VENDOR_PHILIPS, 0x2088) },
	/* Realtek MCE IR Receiver */
	{ USB_DEVICE(VENDOR_REALTEK, 0x0161) },
	/* SMK/Toshiba G83C0004D410 */
	{ USB_DEVICE(VENDOR_SMK, 0x031d) },
	/* SMK eHome Infrared Transceiver (Sony VAIO) */
	{ USB_DEVICE(VENDOR_SMK, 0x0322) },
	/* bundled with Hauppauge PVR-150 */
	{ USB_DEVICE(VENDOR_SMK, 0x0334) },
	/* SMK eHome Infrared Transceiver */
	{ USB_DEVICE(VENDOR_SMK, 0x0338) },
	/* Tatung eHome Infrared Transceiver */
	{ USB_DEVICE(VENDOR_TATUNG, 0x9150) },
	/* Shuttle eHome Infrared Transceiver */
	{ USB_DEVICE(VENDOR_SHUTTLE, 0xc001) },
	/* Shuttle eHome Infrared Transceiver */
	{ USB_DEVICE(VENDOR_SHUTTLE2, 0xc001) },
	/* Gateway eHome Infrared Transceiver */
	{ USB_DEVICE(VENDOR_GATEWAY, 0x3009) },
	/* Mitsumi */
	{ USB_DEVICE(VENDOR_MITSUMI, 0x2501) },
	/* Topseed eHome Infrared Transceiver */
	{ USB_DEVICE(VENDOR_TOPSEED, 0x0001) },
	/* Topseed HP eHome Infrared Transceiver */
	{ USB_DEVICE(VENDOR_TOPSEED, 0x0006) },
	/* Topseed eHome Infrared Transceiver */
	{ USB_DEVICE(VENDOR_TOPSEED, 0x0007) },
	/* Topseed eHome Infrared Transceiver */
	{ USB_DEVICE(VENDOR_TOPSEED, 0x0008) },
	/* Topseed eHome Infrared Transceiver */
	{ USB_DEVICE(VENDOR_TOPSEED, 0x000a) },
	/* Topseed eHome Infrared Transceiver */
	{ USB_DEVICE(VENDOR_TOPSEED, 0x0011) },
	/* Ricavision internal Infrared Transceiver */
	{ USB_DEVICE(VENDOR_RICAVISION, 0x0010) },
	/* Itron ione Libra Q-11 */
	{ USB_DEVICE(VENDOR_ITRON, 0x7002) },
	/* FIC eHome Infrared Transceiver */
	{ USB_DEVICE(VENDOR_FIC, 0x9242) },
	/* LG eHome Infrared Transceiver */
	{ USB_DEVICE(VENDOR_LG, 0x9803) },
	/* Microsoft MCE Infrared Transceiver */
	{ USB_DEVICE(VENDOR_MICROSOFT, 0x00a0) },
	/* Formosa eHome Infrared Transceiver */
	{ USB_DEVICE(VENDOR_FORMOSA, 0xe015) },
	/* Formosa21 / eHome Infrared Receiver */
	{ USB_DEVICE(VENDOR_FORMOSA, 0xe016) },
	/* Formosa aim / Trust MCE Infrared Receiver */
	{ USB_DEVICE(VENDOR_FORMOSA, 0xe017) },
	/* Formosa Industrial Computing / Beanbag Emulation Device */
	{ USB_DEVICE(VENDOR_FORMOSA, 0xe018) },
	/* Formosa21 / eHome Infrared Receiver */
	{ USB_DEVICE(VENDOR_FORMOSA, 0xe03a) },
	/* Formosa Industrial Computing AIM IR605/A */
	{ USB_DEVICE(VENDOR_FORMOSA, 0xe03c) },
	/* Formosa Industrial Computing AIM IR605/A */
	{ USB_DEVICE(VENDOR_FORMOSA, 0xe03e) },
	/* Fintek eHome Infrared Transceiver */
	{ USB_DEVICE(VENDOR_FINTEK, 0x0602) },
	/* Fintek eHome Infrared Transceiver (in the AOpen MP45) */
	{ USB_DEVICE(VENDOR_FINTEK, 0x0702) },
	/* Pinnacle Remote Kit */
	{ USB_DEVICE(VENDOR_PINNACLE, 0x0225) },
	/* Elitegroup Computer Systems IR */
	{ USB_DEVICE(VENDOR_ECS, 0x0f38) },
	/* Wistron Corp. eHome Infrared Receiver */
	{ USB_DEVICE(VENDOR_WISTRON, 0x0002) },
	/* Compro K100 */
	{ USB_DEVICE(VENDOR_COMPRO, 0x3020) },
	/* Compro K100 v2 */
	{ USB_DEVICE(VENDOR_COMPRO, 0x3082) },
	/* Northstar Systems eHome Infrared Transceiver */
	{ USB_DEVICE(VENDOR_NORTHSTAR, 0xe004) },
	/* TiVo PC IR Receiver */
	{ USB_DEVICE(VENDOR_TIVO, 0x2000) },
	/* Terminating entry */
	{ }
};

static struct usb_device_id gen3_list[] = {
	{ USB_DEVICE(VENDOR_PINNACLE, 0x0225) },
	{ USB_DEVICE(VENDOR_TOPSEED, 0x0008) },
	{}
};

static struct usb_device_id microsoft_gen1_list[] = {
	{ USB_DEVICE(VENDOR_MICROSOFT, 0x006d) },
	{}
};

static struct usb_device_id transmitter_mask_list[] = {
	{ USB_DEVICE(VENDOR_MICROSOFT, 0x006d) },
	{ USB_DEVICE(VENDOR_PHILIPS, 0x060c) },
	{ USB_DEVICE(VENDOR_SMK, 0x031d) },
	{ USB_DEVICE(VENDOR_SMK, 0x0322) },
	{ USB_DEVICE(VENDOR_SMK, 0x0334) },
	{ USB_DEVICE(VENDOR_TOPSEED, 0x0001) },
	{ USB_DEVICE(VENDOR_TOPSEED, 0x0006) },
	{ USB_DEVICE(VENDOR_TOPSEED, 0x0007) },
	{ USB_DEVICE(VENDOR_TOPSEED, 0x0008) },
	{ USB_DEVICE(VENDOR_TOPSEED, 0x000a) },
	{ USB_DEVICE(VENDOR_TOPSEED, 0x0011) },
	{ USB_DEVICE(VENDOR_PINNACLE, 0x0225) },
	{}
};

/* data structure for each usb transceiver */
struct mceusb_dev {

	/* usb */
	struct usb_device *usbdev;
	struct urb *urb_in;
	int devnum;
	struct usb_endpoint_descriptor *usb_ep_in;
	struct usb_endpoint_descriptor *usb_ep_out;

	/* buffers and dma */
	unsigned char *buf_in;
	unsigned int len_in;
	dma_addr_t dma_in;
	dma_addr_t dma_out;
	unsigned int overflow_len;

	/* lirc */
	struct lirc_driver *d;
	lirc_t lircdata;
	unsigned char is_pulse;
	struct {
		u32 connected:1;
		u32 transmitter_mask_inverted:1;
		u32 microsoft_gen1:1;
		u32 reserved:29;
	} flags;

	unsigned char transmitter_mask;
	unsigned int carrier_freq;

	/* handle sending (init strings) */
	int send_flags;
	wait_queue_head_t wait_out;

	struct mutex dev_lock;
};

/*
 * MCE Device Command Strings
 * Device command responses vary from device to device...
 * - DEVICE_RESET resets the hardware to its default state
 * - GET_REVISION fetches the hardware/software revision, common
 *   replies are ff 0b 45 ff 1b 08 and ff 0b 50 ff 1b 42
 * - GET_CARRIER_FREQ gets the carrier mode and frequency of the
 *   device, with replies in the form of 9f 06 MM FF, where MM is 0-3,
 *   meaning clk of 10000000, 2500000, 625000 or 156250, and FF is
 *   ((clk / frequency) - 1)
 * - GET_RX_TIMEOUT fetches the receiver timeout in units of 50us,
 *   response in the form of 9f 0c msb lsb
 * - GET_TX_BITMASK fetches the transmitter bitmask, replies in
 *   the form of 9f 08 bm, where bm is the bitmask
 * - GET_RX_SENSOR fetches the RX sensor setting -- long-range
 *   general use one or short-range learning one, in the form of
 *   9f 14 ss, where ss is either 01 for long-range or 02 for short
 * - SET_CARRIER_FREQ sets a new carrier mode and frequency
 * - SET_TX_BITMASK sets the transmitter bitmask
 * - SET_RX_TIMEOUT sets the receiver timeout
 * - SET_RX_SENSOR sets which receiver sensor to use
 */
static char DEVICE_RESET[]	= {0x00, 0xff, 0xaa};
static char GET_REVISION[]	= {0xff, 0x0b};
static char GET_UNKNOWN[]	= {0xff, 0x18};
static char GET_UNKNOWN2[]	= {0x9f, 0x05};
static char GET_CARRIER_FREQ[]	= {0x9f, 0x07};
static char GET_RX_TIMEOUT[]	= {0x9f, 0x0d};
static char GET_TX_BITMASK[]	= {0x9f, 0x13};
static char GET_RX_SENSOR[]	= {0x9f, 0x15};
/* sub in desired values in lower byte or bytes for full command */
//static char SET_CARRIER_FREQ[]	= {0x9f, 0x06, 0x00, 0x00};
//static char SET_TX_BITMASK[]	= {0x9f, 0x08, 0x00};
//static char SET_RX_TIMEOUT[]	= {0x9f, 0x0c, 0x00, 0x00};
//static char SET_RX_SENSOR[]	= {0x9f, 0x14, 0x00};

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 11)
static unsigned long usecs_to_jiffies(const unsigned int u)
{
	if (u > jiffies_to_usecs(MAX_JIFFY_OFFSET))
		return MAX_JIFFY_OFFSET;
#if HZ <= USEC_PER_SEC && !(USEC_PER_SEC % HZ)
	return (u + (USEC_PER_SEC / HZ) - 1) / (USEC_PER_SEC / HZ);
#elif HZ > USEC_PER_SEC && !(HZ % USEC_PER_SEC)
	return u * (HZ / USEC_PER_SEC);
#else
	return (u * HZ + USEC_PER_SEC - 1) / USEC_PER_SEC;
#endif
}
#endif
static void mceusb_dev_printdata(struct mceusb_dev *ir, char *buf,
				 int len, bool out)
{
	char codes[USB_BUFLEN * 3 + 1];
	char inout[9];
	int i;
	u8 cmd, subcmd, data1, data2;
	int idx = 0;

	if (ir->flags.microsoft_gen1 && !out)
		idx = 2;

	if (len <= idx)
		return;

	for (i = 0; i < len && i < USB_BUFLEN; i++)
		snprintf(codes + i * 3, 4, "%02x ", buf[i] & 0xFF);

	printk(KERN_INFO "" DRIVER_NAME "[%d]: %sx data: %s (length=%d)\n",
		ir->devnum, (out ? "t" : "r"), codes, len);

	if (out)
		strcpy(inout, "Request\0");
	else
		strcpy(inout, "Got\0");

	cmd    = buf[idx] & 0xff;
	subcmd = buf[idx + 1] & 0xff;
	data1  = buf[idx + 2] & 0xff;
	data2  = buf[idx + 3] & 0xff;

	switch (cmd) {
	case 0x00:
		if (subcmd == 0xff && data1 == 0xaa)
			printk(KERN_INFO "Device reset requested\n");
		else
			printk(KERN_INFO "Unknown command 0x%02x 0x%02x\n",
				cmd, subcmd);
		break;
	case 0xff:
		switch (subcmd) {
		case 0x0b:
			if (len == 2)
				printk(KERN_INFO "Get hw/sw rev?\n");
			else
				printk(KERN_INFO "hw/sw rev 0x%02x 0x%02x "
					"0x%02x 0x%02x\n", data1, data2,
					buf[4], buf[5]);
			break;
		case 0xaa:
			printk(KERN_INFO "Device reset requested\n");
			break;
		case 0xfe:
			printk(KERN_INFO "Previous command not supported\n");
			break;
		case 0x18:
		case 0x1b:
		default:
			printk(KERN_INFO "Unknown command 0x%02x 0x%02x\n",
				cmd, subcmd);
			break;
		}
		break;
	case 0x9f:
		switch (subcmd) {
		case 0x03:
			printk(KERN_INFO "Ping\n");
			break;
		case 0x04:
			printk(KERN_INFO "Resp to 9f 05 of 0x%02x 0x%02x\n",
				data1, data2);
			break;
		case 0x06:
			printk(KERN_INFO "%s carrier mode and freq of 0x%02x 0x%02x\n",
				inout, data1, data2);
			break;
		case 0x07:
			printk(KERN_INFO "Get carrier mode and freq\n");
			break;
		case 0x08:
			printk(KERN_INFO "%s transmit blaster mask of 0x%02x\n",
				inout, data1);
			break;
		case 0x0c:
			/* value is in units of 50us, so x*50/100 or x/2 ms */
			printk(KERN_INFO "%s receive timeout of %d ms\n",
				inout, ((data1 << 8) | data2) / 2);
			break;
		case 0x0d:
			printk(KERN_INFO "Get receive timeout\n");
			break;
		case 0x13:
			printk(KERN_INFO "Get transmit blaster mask\n");
			break;
		case 0x14:
			printk(KERN_INFO "%s %s-range receive sensor in use\n",
				inout, data1 == 0x02 ? "short" : "long");
			break;
		case 0x15:
			if (len == 2)
				printk(KERN_INFO "Get receive sensor\n");
			else
				printk(KERN_INFO "Received pulse count is %d\n",
					((data1 << 8) | data2));
			break;
		case 0xfe:
			printk(KERN_INFO "Error! Hardware is likely wedged...\n");
			break;
		case 0x05:
		case 0x09:
		case 0x0f:
		default:
			printk(KERN_INFO "Unknown command 0x%02x 0x%02x\n",
				cmd, subcmd);
			break;
		}
		break;
	default:
		break;
	}
}

static void usb_async_callback(struct urb *urb, struct pt_regs *regs)
{
	struct mceusb_dev *ir;
	int len;

	if (!urb)
		return;

	ir = urb->context;
	if (ir) {
		len = urb->actual_length;

		dprintk(DRIVER_NAME
			"[%d]: callback called (status=%d len=%d)\n",
			ir->devnum, urb->status, len);

		if (debug)
			mceusb_dev_printdata(ir, urb->transfer_buffer, len, true);
	}

}

/* request incoming or send outgoing usb packet - used to initialize remote */
static void mce_request_packet(struct mceusb_dev *ir,
			       struct usb_endpoint_descriptor *ep,
			       unsigned char *data, int size, int urb_type)
{
	int res;
	struct urb *async_urb;
	unsigned char *async_buf;

	if (urb_type == MCEUSB_TX) {
		async_urb = usb_alloc_urb(0, GFP_KERNEL);
		if (unlikely(!async_urb)) {
			printk(KERN_ERR "Error, couldn't allocate urb!\n");
			return;
		}

		async_buf = kzalloc(size, GFP_KERNEL);
		if (!async_buf) {
			printk(KERN_ERR "Error, couldn't allocate buf!\n");
			usb_free_urb(async_urb);
			return;
		}

		/* outbound data */
		usb_fill_int_urb(async_urb, ir->usbdev,
			usb_sndintpipe(ir->usbdev, ep->bEndpointAddress),
			async_buf, size, (usb_complete_t) usb_async_callback,
			ir, ep->bInterval);
		memcpy(async_buf, data, size);

	} else if (urb_type == MCEUSB_RX) {
		/* standard request */
		async_urb = ir->urb_in;
		ir->send_flags = RECV_FLAG_IN_PROGRESS;
	} else {
		printk(KERN_ERR "Error! Unknown urb type %d\n", urb_type);
		return;
	}

	dprintk(DRIVER_NAME "[%d]: receive request called (size=%#x)\n",
		ir->devnum, size);

	async_urb->transfer_buffer_length = size;
	async_urb->dev = ir->usbdev;

	res = usb_submit_urb(async_urb, GFP_ATOMIC);
	if (res) {
		dprintk(DRIVER_NAME "[%d]: receive request FAILED! (res=%d)\n",
			ir->devnum, res);
		return;
	}
	dprintk(DRIVER_NAME "[%d]: receive request complete (res=%d)\n",
		ir->devnum, res);
}

static void mce_async_out(struct mceusb_dev *ir, unsigned char *data, int size)
{
	mce_request_packet(ir, ir->usb_ep_out, data, size, MCEUSB_TX);
}

static void mce_sync_in(struct mceusb_dev *ir, unsigned char *data, int size)
{
	mce_request_packet(ir, ir->usb_ep_in, data, size, MCEUSB_RX);
}

static int unregister_from_lirc(struct mceusb_dev *ir)
{
	struct lirc_driver *d = ir->d;
	int devnum;
	int rtn;

	devnum = ir->devnum;
	dprintk(DRIVER_NAME "[%d]: unregister from lirc called\n", devnum);

	rtn = lirc_unregister_driver(d->minor);
	if (rtn > 0) {
		printk(DRIVER_NAME "[%d]: error in lirc_unregister minor: %d\n"
			"Trying again...\n", devnum, d->minor);
		if (rtn == -EBUSY) {
			printk(DRIVER_NAME
				"[%d]: device is opened, will unregister"
				" on close\n", devnum);
			return -EAGAIN;
		}
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(HZ);

		rtn = lirc_unregister_driver(d->minor);
		if (rtn > 0)
			printk(DRIVER_NAME "[%d]: lirc_unregister failed\n",
			devnum);
	}

	if (rtn) {
		printk(DRIVER_NAME "[%d]: didn't free resources\n", devnum);
		return -EAGAIN;
	}

	printk(DRIVER_NAME "[%d]: usb remote disconnected\n", devnum);

	lirc_buffer_free(d->rbuf);
	kfree(d->rbuf);
	kfree(d);
	kfree(ir);
	return 0;
}

static int mceusb_ir_open(void *data)
{
	struct mceusb_dev *ir = data;

	if (!ir) {
		printk(DRIVER_NAME "[?]: %s called with no context\n",
		       __func__);
		return -EIO;
	}
	dprintk(DRIVER_NAME "[%d]: mceusb IR device opened\n", ir->devnum);

	MOD_INC_USE_COUNT;
	if (!ir->flags.connected) {
		if (!ir->usbdev)
			return -ENOENT;
		ir->flags.connected = 1;
	}

	return 0;
}

static void mceusb_ir_close(void *data)
{
	struct mceusb_dev *ir = data;

	if (!ir) {
		printk(DRIVER_NAME "[?]: %s called with no context\n",
		       __func__);
		return;
	}
	dprintk(DRIVER_NAME "[%d]: mceusb IR device closed\n", ir->devnum);

	if (ir->flags.connected) {
		mutex_lock(&ir->dev_lock);
		ir->flags.connected = 0;
		mutex_unlock(&ir->dev_lock);
	}
	MOD_DEC_USE_COUNT;
}

static void send_packet_to_lirc(struct mceusb_dev *ir)
{
	if (ir->lircdata) {
		lirc_buffer_write(ir->d->rbuf,
				  (unsigned char *) &ir->lircdata);
		wake_up(&ir->d->rbuf->wait_poll);
		ir->lircdata = 0;
	}
}

static void mceusb_process_ir_data(struct mceusb_dev *ir, int buf_len)
{
	int i, j;
	int packet_len = 0;
	int start_index = 0;

	/* skip meaningless 0xb1 0x60 header bytes on orig receiver */
	if (ir->flags.microsoft_gen1)
		start_index = 2;

	/* this should only trigger w/the 1st-gen mce receiver */
	for (i = start_index; i < (start_index + ir->overflow_len) &&
	     i < buf_len; i++) {
		/* rising/falling flank */
		if (ir->is_pulse != (ir->buf_in[i] & MCE_PULSE_BIT)) {
			send_packet_to_lirc(ir);
			ir->is_pulse = ir->buf_in[i] & MCE_PULSE_BIT;
		}

		/* accumulate mce pulse/space values */
		ir->lircdata += (ir->buf_in[i] & MCE_PULSE_MASK) *
				MCE_TIME_UNIT;
		ir->lircdata |= (ir->is_pulse ? PULSE_BIT : 0);
	}
	start_index += ir->overflow_len;
	ir->overflow_len = 0;

	for (i = start_index; i < buf_len; i++) {
		/* decode mce packets of the form (84),AA,BB,CC,DD */
		if (ir->buf_in[i] >= 0x80 && ir->buf_in[i] <= 0x9e) {
			/* data headers */
			/* decode packet data */
			packet_len = ir->buf_in[i] & MCE_PACKET_LENGTH_MASK;
			ir->overflow_len = i + 1 + packet_len - buf_len;
			for (j = 1; j <= packet_len && (i + j < buf_len); j++) {
				/* rising/falling flank */
				if (ir->is_pulse !=
				    (ir->buf_in[i + j] & MCE_PULSE_BIT)) {
					send_packet_to_lirc(ir);
					ir->is_pulse =
						ir->buf_in[i + j] &
							MCE_PULSE_BIT;
				}

				/* accumulate mce pulse/space values */
				ir->lircdata +=
					(ir->buf_in[i + j] & MCE_PULSE_MASK) *
						MCE_TIME_UNIT;
				ir->lircdata |= (ir->is_pulse ? PULSE_BIT : 0);
			}

			i += packet_len;
		} else if (ir->buf_in[i] == MCE_CONTROL_HEADER) {
			/* status header (0x9F) */
			/*
			 * A transmission containing one or more consecutive ir
			 * commands always ends with a GAP of 100ms followed by
			 * the sequence 0x9F 0x01 0x01 0x9F 0x15 0x00 0x00 0x80
			 */

#if 0
	Uncomment this if the last 100ms "infinity"-space should be transmitted
	to lirc directly instead of at the beginning of the next transmission.
	Changes pulse/space order.

			if (++i < buf_len && ir->buf_in[i]==0x01)
				send_packet_to_lirc(ir);

#endif

			/* end decode loop */
			dprintk(DRIVER_NAME "[%d] %s: found control header\n",
				ir->devnum, __func__);
			ir->overflow_len = 0;
			break;
		} else {
			dprintk(DRIVER_NAME "[%d] %s: stray packet?\n",
				ir->devnum, __func__);
			ir->overflow_len = 0;
		}
	}

	return;
}

static void mceusb_dev_recv(struct urb *urb, struct pt_regs *regs)
{
	struct mceusb_dev *ir;
	int buf_len;

	if (!urb)
		return;

	ir = urb->context;
	if (!ir) {
		urb->transfer_flags |= URB_ASYNC_UNLINK;
		usb_unlink_urb(urb);
		return;
	}

	buf_len = urb->actual_length;

	if (debug)
		mceusb_dev_printdata(ir, urb->transfer_buffer, buf_len, false);

	if (ir->send_flags == RECV_FLAG_IN_PROGRESS) {
		ir->send_flags = SEND_FLAG_COMPLETE;
		dprintk(DRIVER_NAME "[%d]: setup answer received %d bytes\n",
			ir->devnum, buf_len);
	}

	switch (urb->status) {
	/* success */
	case 0:
		mceusb_process_ir_data(ir, buf_len);
		break;

	case -ECONNRESET:
	case -ENOENT:
	case -ESHUTDOWN:
		urb->transfer_flags |= URB_ASYNC_UNLINK;
		usb_unlink_urb(urb);
		return;

	case -EPIPE:
	default:
		break;
	}

	usb_submit_urb(urb, GFP_ATOMIC);
}


static ssize_t mceusb_transmit_ir(struct file *file, const char *buf,
				  size_t n, loff_t *ppos)
{
	int i, count = 0, cmdcount = 0;
	struct mceusb_dev *ir = NULL;
	lirc_t wbuf[LIRCBUF_SIZE]; /* Workbuffer with values from lirc */
	unsigned char cmdbuf[MCE_CMDBUF_SIZE]; /* MCE command buffer */
	unsigned long signal_duration = 0; /* Singnal length in us */
	struct timeval start_time, end_time;

	do_gettimeofday(&start_time);

	/* Retrieve lirc_driver data for the device */
	ir = lirc_get_pdata(file);
	if (!ir || !ir->usb_ep_out)
		return -EFAULT;

	if (n % sizeof(lirc_t))
		return -EINVAL;
	count = n / sizeof(lirc_t);

	/* Check if command is within limits */
	if (count > LIRCBUF_SIZE || count%2 == 0)
		return -EINVAL;
	if (copy_from_user(wbuf, buf, n))
		return -EFAULT;

	/* MCE tx init header */
	cmdbuf[cmdcount++] = MCE_CONTROL_HEADER;
	cmdbuf[cmdcount++] = 0x08;
	cmdbuf[cmdcount++] = ir->transmitter_mask;

	/* Generate mce packet data */
	for (i = 0; (i < count) && (cmdcount < MCE_CMDBUF_SIZE); i++) {
		signal_duration += wbuf[i];
		wbuf[i] = wbuf[i] / MCE_TIME_UNIT;

		do { /* loop to support long pulses/spaces > 127*50us=6.35ms */

			/* Insert mce packet header every 4th entry */
			if ((cmdcount < MCE_CMDBUF_SIZE) &&
			    (cmdcount - MCE_TX_HEADER_LENGTH) %
			     MCE_CODE_LENGTH == 0)
				cmdbuf[cmdcount++] = MCE_PACKET_HEADER;

			/* Insert mce packet data */
			if (cmdcount < MCE_CMDBUF_SIZE)
				cmdbuf[cmdcount++] =
					(wbuf[i] < MCE_PULSE_BIT ?
					 wbuf[i] : MCE_MAX_PULSE_LENGTH) |
					 (i & 1 ? 0x00 : MCE_PULSE_BIT);
			else
				return -EINVAL;
		} while ((wbuf[i] > MCE_MAX_PULSE_LENGTH) &&
			 (wbuf[i] -= MCE_MAX_PULSE_LENGTH));
	}

	/* Fix packet length in last header */
	cmdbuf[cmdcount - (cmdcount - MCE_TX_HEADER_LENGTH) % MCE_CODE_LENGTH] =
		0x80 + (cmdcount - MCE_TX_HEADER_LENGTH) % MCE_CODE_LENGTH - 1;

	/* Check if we have room for the empty packet at the end */
	if (cmdcount >= MCE_CMDBUF_SIZE)
		return -EINVAL;

	/* All mce commands end with an empty packet (0x80) */
	cmdbuf[cmdcount++] = 0x80;

	/* Transmit the command to the mce device */
	mce_async_out(ir, cmdbuf, cmdcount);

	/*
	 * The lircd gap calculation expects the write function to
	 * wait the time it takes for the ircommand to be sent before
	 * it returns.
	 */
	do_gettimeofday(&end_time);
	signal_duration -= (end_time.tv_usec - start_time.tv_usec) +
			   (end_time.tv_sec - start_time.tv_sec) * 1000000;

	/* delay with the closest number of ticks */
	set_current_state(TASK_INTERRUPTIBLE);
	schedule_timeout(usecs_to_jiffies(signal_duration));

	return n;
}

static void set_transmitter_mask(struct mceusb_dev *ir, unsigned int mask)
{
	if (ir->flags.transmitter_mask_inverted)
		/*
		 * The mask begins at 0x02 and has an inverted
		 * numbering scheme
		 */
		ir->transmitter_mask =
			(mask != 0x03 ? mask ^ 0x03 : mask) << 1;
	else
		ir->transmitter_mask = mask;
}


/* Sets the send carrier frequency */
static int set_send_carrier(struct mceusb_dev *ir, int carrier)
{
	int clk = 10000000;
	int prescaler = 0, divisor = 0;
	unsigned char cmdbuf[] = { 0x9F, 0x06, 0x01, 0x80 };

	/* Carrier is changed */
	if (ir->carrier_freq != carrier) {

		if (carrier <= 0) {
			ir->carrier_freq = carrier;
			dprintk(DRIVER_NAME "[%d]: SET_CARRIER disabling "
				"carrier modulation\n", ir->devnum);
			mce_async_out(ir, cmdbuf, sizeof(cmdbuf));
			return carrier;
		}

		for (prescaler = 0; prescaler < 4; ++prescaler) {
			divisor = (clk >> (2 * prescaler)) / carrier;
			if (divisor <= 0xFF) {
				ir->carrier_freq = carrier;
				cmdbuf[2] = prescaler;
				cmdbuf[3] = divisor;
				dprintk(DRIVER_NAME "[%d]: SET_CARRIER "
					"requesting %d Hz\n",
					ir->devnum, carrier);

				/* Transmit new carrier to mce device */
				mce_async_out(ir, cmdbuf, sizeof(cmdbuf));
				return carrier;
			}
		}

		return -EINVAL;

	}

	return carrier;
}


#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 35)
static int mceusb_lirc_ioctl(struct inode *node, struct file *filep,
			     unsigned int cmd, unsigned long arg)
#else
static long mceusb_lirc_ioctl(struct file *filep, unsigned int cmd,
			      unsigned long arg)
#endif
{
	int result;
	unsigned int ivalue;
	struct mceusb_dev *ir = NULL;

	/* Retrieve lirc_driver data for the device */
	ir = lirc_get_pdata(filep);
	if (!ir || !ir->usb_ep_out)
		return -EFAULT;


	switch (cmd) {
	case LIRC_SET_TRANSMITTER_MASK:

		result = get_user(ivalue, (unsigned int *) arg);
		if (result)
			return result;
		switch (ivalue) {
		case 0x01: /* Transmitter 1     => 0x04 */
		case 0x02: /* Transmitter 2     => 0x02 */
		case 0x03: /* Transmitter 1 & 2 => 0x06 */
			set_transmitter_mask(ir, ivalue);
			break;

		default: /* Unsupported transmitter mask */
			return MCE_MAX_CHANNELS;
		}

		dprintk(DRIVER_NAME ": SET_TRANSMITTERS mask=%d\n", ivalue);
		break;

	case LIRC_SET_SEND_CARRIER:

		result = get_user(ivalue, (unsigned int *) arg);
		if (result)
			return result;

		set_send_carrier(ir, ivalue);
		break;

	default:
		return -ENOIOCTLCMD;
	}

	return 0;
}

static struct file_operations lirc_fops = {
	.owner	= THIS_MODULE,
	.write	= mceusb_transmit_ir,
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 35)
	.ioctl	= mceusb_lirc_ioctl,
#else
	.unlocked_ioctl	= mceusb_lirc_ioctl,
#endif
};

static void mceusb_gen1_init(struct mceusb_dev *ir)
{
	int ret;
	int maxp = ir->len_in;
	char *data;

	data = kzalloc(USB_CTRL_MSG_SZ, GFP_KERNEL);
	if (!data) {
		printk(KERN_ERR "%s: memory allocation failed!\n", __func__);
		return;
	}

	/*
	 * This is a strange one. They issue a set address to the device
	 * on the receive control pipe and expect a certain value pair back
	 */
	ret = usb_control_msg(ir->usbdev, usb_rcvctrlpipe(ir->usbdev, 0),
			      USB_REQ_SET_ADDRESS, USB_TYPE_VENDOR, 0, 0,
			      data, USB_CTRL_MSG_SZ, HZ * 3);
	dprintk("%s - ret = %d, devnum = %d\n",
		__func__, ret, ir->usbdev->devnum);
	dprintk("%s - data[0] = %d, data[1] = %d\n",
		__func__, data[0], data[1]);

	/* set feature: bit rate 38400 bps */
	ret = usb_control_msg(ir->usbdev, usb_sndctrlpipe(ir->usbdev, 0),
			      USB_REQ_SET_FEATURE, USB_TYPE_VENDOR,
			      0xc04e, 0x0000, NULL, 0, HZ * 3);

	dprintk("%s - ret = %d\n", __func__, ret);

	/* bRequest 4: set char length to 8 bits */
	ret = usb_control_msg(ir->usbdev, usb_sndctrlpipe(ir->usbdev, 0),
			      4, USB_TYPE_VENDOR,
			      0x0808, 0x0000, NULL, 0, HZ * 3);
	dprintk("%s - retB = %d\n", __func__, ret);

	/* bRequest 2: set handshaking to use DTR/DSR */
	ret = usb_control_msg(ir->usbdev, usb_sndctrlpipe(ir->usbdev, 0),
			      2, USB_TYPE_VENDOR,
			      0x0000, 0x0100, NULL, 0, HZ * 3);
	dprintk("%s - retC = %d\n", __func__, ret);

	/* device reset */
	mce_async_out(ir, DEVICE_RESET, sizeof(DEVICE_RESET));
	mce_sync_in(ir, NULL, maxp);

	/* get hw/sw revision? */
	mce_async_out(ir, GET_REVISION, sizeof(GET_REVISION));
	mce_sync_in(ir, NULL, maxp);

	kfree(data);

	return;

};

static void mceusb_gen2_init(struct mceusb_dev *ir)
{
	int maxp = ir->len_in;

	/* device reset */
	mce_async_out(ir, DEVICE_RESET, sizeof(DEVICE_RESET));
	mce_sync_in(ir, NULL, maxp);

	/* get hw/sw revision? */
	mce_async_out(ir, GET_REVISION, sizeof(GET_REVISION));
	mce_sync_in(ir, NULL, maxp);

	/* unknown what the next two actually return... */
	mce_async_out(ir, GET_UNKNOWN, sizeof(GET_UNKNOWN));
	mce_sync_in(ir, NULL, maxp);
	mce_async_out(ir, GET_UNKNOWN2, sizeof(GET_UNKNOWN2));
	mce_sync_in(ir, NULL, maxp);
}

static void mceusb_get_parameters(struct mceusb_dev *ir)
{
	int maxp = ir->len_in;

	/* get the carrier and frequency */
	mce_async_out(ir, GET_CARRIER_FREQ, sizeof(GET_CARRIER_FREQ));
	mce_sync_in(ir, NULL, maxp);

	/* get the transmitter bitmask */
	mce_async_out(ir, GET_TX_BITMASK, sizeof(GET_TX_BITMASK));
	mce_sync_in(ir, NULL, maxp);

	/* get receiver timeout value */
	mce_async_out(ir, GET_RX_TIMEOUT, sizeof(GET_RX_TIMEOUT));
	mce_sync_in(ir, NULL, maxp);

	/* get receiver sensor setting */
	mce_async_out(ir, GET_RX_SENSOR, sizeof(GET_RX_SENSOR));
	mce_sync_in(ir, NULL, maxp);
}

static int __devinit mceusb_dev_probe(struct usb_interface *intf,
				      const struct usb_device_id *id)
{
	struct usb_device *dev = interface_to_usbdev(intf);
	struct usb_host_interface *idesc;
	struct usb_endpoint_descriptor *ep = NULL;
	struct usb_endpoint_descriptor *ep_in = NULL;
	struct usb_endpoint_descriptor *ep_out = NULL;
	struct usb_host_config *config;
	struct mceusb_dev *ir = NULL;
	struct lirc_driver *driver = NULL;
	struct lirc_buffer *rbuf = NULL;
	int devnum, pipe, maxp;
	int minor = 0;
	int i;
	char buf[63], name[128] = "";
	int mem_failure = 0;
	bool is_gen3;
	bool is_microsoft_gen1;

	dprintk(DRIVER_NAME ": %s called\n", __func__);

	config = dev->actconfig;
	idesc = intf->cur_altsetting;

	is_gen3 = usb_match_id(intf, gen3_list) ? 1 : 0;
	is_microsoft_gen1 = usb_match_id(intf, microsoft_gen1_list) ? 1 : 0;

	/* step through the endpoints to find first bulk in and out endpoint */
	for (i = 0; i < idesc->desc.bNumEndpoints; ++i) {
		ep = &idesc->endpoint[i].desc;

		if ((ep_in == NULL)
			&& ((ep->bEndpointAddress & USB_ENDPOINT_DIR_MASK)
			    == USB_DIR_IN)
			&& (((ep->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK)
			    == USB_ENDPOINT_XFER_BULK)
			|| ((ep->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK)
			    == USB_ENDPOINT_XFER_INT))) {

			dprintk(DRIVER_NAME ": acceptable inbound endpoint "
				"found\n");
			ep_in = ep;
			ep_in->bmAttributes = USB_ENDPOINT_XFER_INT;
			ep_in->bInterval = 1;
		}

		if ((ep_out == NULL)
			&& ((ep->bEndpointAddress & USB_ENDPOINT_DIR_MASK)
			    == USB_DIR_OUT)
			&& (((ep->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK)
			    == USB_ENDPOINT_XFER_BULK)
			|| ((ep->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK)
			    == USB_ENDPOINT_XFER_INT))) {

			dprintk(DRIVER_NAME ": acceptable outbound endpoint "
				"found\n");
			ep_out = ep;
			ep_out->bmAttributes = USB_ENDPOINT_XFER_INT;
			ep_out->bInterval = 1;
		}
	}
	if (ep_in == NULL || ep_out == NULL) {
		dprintk(DRIVER_NAME ": inbound and/or "
			"outbound endpoint not found\n");
		return -ENODEV;
	}

	devnum = dev->devnum;
	pipe = usb_rcvintpipe(dev, ep_in->bEndpointAddress);
	maxp = usb_maxpacket(dev, pipe, usb_pipeout(pipe));

	mem_failure = 0;
	ir = kzalloc(sizeof(struct mceusb_dev), GFP_KERNEL);
	if (!ir)
		goto mem_alloc_fail;

	driver = kzalloc(sizeof(struct lirc_driver), GFP_KERNEL);
	if (!driver)
		goto mem_alloc_fail;

	rbuf = kzalloc(sizeof(struct lirc_buffer), GFP_KERNEL);
	if (!rbuf)
		goto mem_alloc_fail;

	if (lirc_buffer_init(rbuf, sizeof(lirc_t), LIRCBUF_SIZE))
		goto mem_alloc_fail;

	ir->buf_in = usb_alloc_coherent(dev, maxp, GFP_ATOMIC, &ir->dma_in);
	if (!ir->buf_in)
		goto buf_in_alloc_fail;

	ir->urb_in = usb_alloc_urb(0, GFP_KERNEL);
	if (!ir->urb_in)
		goto urb_in_alloc_fail;

	strcpy(driver->name, DRIVER_NAME " ");
	driver->minor = -1;
	driver->features = LIRC_CAN_SEND_PULSE |
		LIRC_CAN_SET_TRANSMITTER_MASK |
		LIRC_CAN_REC_MODE2 |
		LIRC_CAN_SET_SEND_CARRIER;
	driver->data = ir;
	driver->rbuf = rbuf;
	driver->set_use_inc = &mceusb_ir_open;
	driver->set_use_dec = &mceusb_ir_close;
	driver->code_length = sizeof(lirc_t) * 8;
	driver->fops  = &lirc_fops;
	driver->dev   = &intf->dev;
	driver->owner = THIS_MODULE;

	mutex_init(&ir->dev_lock);
	init_waitqueue_head(&ir->wait_out);

	minor = lirc_register_driver(driver);
	if (minor < 0)
		goto lirc_register_fail;

	driver->minor = minor;
	ir->d = driver;
	ir->devnum = devnum;
	ir->usbdev = dev;
	ir->len_in = maxp;
	ir->overflow_len = 0;
	ir->flags.connected = 0;
	ir->flags.microsoft_gen1 = is_microsoft_gen1;
	ir->flags.transmitter_mask_inverted =
		usb_match_id(intf, transmitter_mask_list) ? 0 : 1;

	ir->lircdata = PULSE_MASK;
	ir->is_pulse = 0;

	/* Saving usb interface data for use by the transmitter routine */
	ir->usb_ep_in = ep_in;
	ir->usb_ep_out = ep_out;

	if (dev->descriptor.iManufacturer
	    && usb_string(dev, dev->descriptor.iManufacturer,
			  buf, sizeof(buf)) > 0)
		strlcpy(name, buf, sizeof(name));
	if (dev->descriptor.iProduct
	    && usb_string(dev, dev->descriptor.iProduct,
			  buf, sizeof(buf)) > 0)
		snprintf(name + strlen(name), sizeof(name) - strlen(name),
			 " %s", buf);
	printk(DRIVER_NAME "[%d]: %s on usb%d:%d\n", devnum, name,
	       dev->bus->busnum, devnum);

	/* flush buffers on the device */
	mce_sync_in(ir, NULL, maxp);
	mce_sync_in(ir, NULL, maxp);

	/* wire up inbound data handler */
	usb_fill_int_urb(ir->urb_in, dev, pipe, ir->buf_in,
		maxp, (usb_complete_t) mceusb_dev_recv, ir, ep_in->bInterval);
	ir->urb_in->transfer_dma = ir->dma_in;
	ir->urb_in->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

	/* initialize device */
	if (ir->flags.microsoft_gen1)
		mceusb_gen1_init(ir);
	else if (!is_gen3)
		mceusb_gen2_init(ir);

	mceusb_get_parameters(ir);

	/* ir->flags.transmitter_mask_inverted must be set */
	set_transmitter_mask(ir, MCE_DEFAULT_TX_MASK);

	usb_set_intfdata(intf, ir);

	return 0;

	/* Error-handling path */
lirc_register_fail:
	usb_free_urb(ir->urb_in);
urb_in_alloc_fail:
	usb_free_coherent(dev, maxp, ir->buf_in, ir->dma_in);
buf_in_alloc_fail:
	lirc_buffer_free(rbuf);
mem_alloc_fail:
	kfree(rbuf);
	kfree(driver);
	kfree(ir);
	printk(KERN_ERR "out of memory (code=%d)\n", mem_failure);

	return -ENOMEM;
}


static void __devexit mceusb_dev_disconnect(struct usb_interface *intf)
{
	struct usb_device *dev = interface_to_usbdev(intf);
	struct mceusb_dev *ir = usb_get_intfdata(intf);

	usb_set_intfdata(intf, NULL);

	if (!ir || !ir->d)
		return;

	ir->usbdev = NULL;
	wake_up_all(&ir->wait_out);

	mutex_lock(&ir->dev_lock);
	usb_kill_urb(ir->urb_in);
	usb_free_urb(ir->urb_in);
	usb_free_coherent(dev, ir->len_in, ir->buf_in, ir->dma_in);
	mutex_unlock(&ir->dev_lock);

	unregister_from_lirc(ir);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
static int mceusb_dev_suspend(struct usb_interface *intf, pm_message_t message)
{
	struct mceusb_dev *ir = usb_get_intfdata(intf);
	printk(DRIVER_NAME "[%d]: suspend\n", ir->devnum);
	usb_kill_urb(ir->urb_in);
	return 0;
}

static int mceusb_dev_resume(struct usb_interface *intf)
{
	struct mceusb_dev *ir = usb_get_intfdata(intf);
	printk(DRIVER_NAME "[%d]: resume\n", ir->devnum);
	if (usb_submit_urb(ir->urb_in, GFP_ATOMIC))
		return -EIO;
	return 0;
}
#endif

static struct usb_driver mceusb_dev_driver = {
	LIRC_THIS_MODULE(.owner = THIS_MODULE)
	.name =		DRIVER_NAME,
	.probe =	mceusb_dev_probe,
	.disconnect =	mceusb_dev_disconnect,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
	.suspend =	mceusb_dev_suspend,
	.resume =	mceusb_dev_resume,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 23)
	.reset_resume =	mceusb_dev_resume,
#endif
#endif
	.id_table =	mceusb_dev_table
};

static int __init mceusb_dev_init(void)
{
	int i;

	printk(KERN_INFO DRIVER_NAME ": " DRIVER_DESC " " DRIVER_VERSION "\n");
	printk(KERN_INFO DRIVER_NAME ": " DRIVER_AUTHOR "\n");
	dprintk(DRIVER_NAME ": debug mode enabled\n");

	i = usb_register(&mceusb_dev_driver);
	if (i < 0) {
		printk(DRIVER_NAME ": usb register failed, result = %d\n", i);
		return -ENODEV;
	}

	return 0;
}

static void __exit mceusb_dev_exit(void)
{
	usb_deregister(&mceusb_dev_driver);
}

module_init(mceusb_dev_init);
module_exit(mceusb_dev_exit);

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_LICENSE("GPL");
MODULE_DEVICE_TABLE(usb, mceusb_dev_table);
/* this was originally lirc_mceusb2, lirc_mceusb and lirc_mceusb2 merged now */
MODULE_ALIAS("lirc_mceusb2");

module_param(debug, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(debug, "Debug enabled or not");

EXPORT_NO_SYMBOLS;
