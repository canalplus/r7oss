/*
 *  Multi-Target Trace solution
 *
 *  MTT - sample software.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Copyright (C) STMicroelectronics, 2011
 */

#define __NO_VERSION__

#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/kdev_t.h>
#include <linux/cdev.h>

#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/version.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/timer.h>
#include <linux/wait.h>

#include "mttsample.h"

#include <trace/kptrace.h>

static dev_t dev;
static struct cdev fake_cdev;
static int Major;
static int Device_Open;

/*to fake hardware event*/
struct timer_list fake_timer;
wait_queue_head_t wq;
wait_queue_t wait;
static int got_irq;
static int nb_irqs;		/*count number of simulated irqs */

static int mttsample_open(struct inode *, struct file *);
static int mttsample_release(struct inode *, struct file *);
static ssize_t mttsample_read(struct file *, char *, size_t, loff_t *);
static ssize_t mttsample_write(struct file *, const char *, size_t, loff_t *);
static long mttsample_ioctl(struct file *file, unsigned int cmd,
			    unsigned long arg);

static const struct file_operations fops = {
	.read = mttsample_read,
	.write = mttsample_write,
	.open = mttsample_open,
	.release = mttsample_release,
	.unlocked_ioctl = mttsample_ioctl
};

/* Timer callback that simulates a end-of-DMA routine, a bit like those
 * that call snd_period_elapsed in alsa.
 */
static void mttsample_capture_DMA_callback(unsigned long cpu);

int __init mttsample_init(void)
{
	int ret;

	ret = alloc_chrdev_region(&dev, THIS_MINOR, 1, DEVICE_NAME);
	if (ret < 0) {
		printk(KERN_ERR "Registering mttsample device failed\n");
		return ret;
	}

	Major = MAJOR(dev);
	printk(KERN_NOTICE "Registering mttsample device with %d\n", Major);

	cdev_init(&fake_cdev, &fops);
	fake_cdev.owner = THIS_MODULE;
	fake_cdev.ops = &fops;
	ret = cdev_add(&fake_cdev, (unsigned int)dev, 1);
	if (ret)
		printk(KERN_NOTICE "Error %d adding mttsample", ret);

	init_timer(&fake_timer);
	fake_timer.function = mttsample_capture_DMA_callback;
	fake_timer.expires = jiffies + DELAY;
	fake_timer.data = (unsigned long)raw_smp_processor_id();

	init_waitqueue_head(&wq);

	return 0;
}

void __exit mttsample_exit(void)
{
	/* ==MTT== mtt_close must be called to release any component
	 * allocated with mtt_open.
	 **/
	printk(KERN_NOTICE "Unregister mttsample device\n");

	cdev_del(&fake_cdev);
	unregister_chrdev_region(dev, 1);
}

static int mttsample_open(struct inode *inode, struct file *file)
{
	if (Device_Open)
		return -EBUSY;

	Device_Open++;

	printk(KERN_INFO "mttsample: We don't have to, but let's create" \
	       " several components to illustrace it can be done, and show" \
	       " how it feels in the tool.");

	got_irq = 0;
	nb_irqs = 0;
	init_waitqueue_entry(&wait, current);
	add_wait_queue(&wq, &wait);

	return SUCCESS;
}

static int mttsample_release(struct inode *inode, struct file *file)
{
	remove_wait_queue(&wq, &wait);
	Device_Open--;
	return 0;
}

static ssize_t mttsample_read(struct file *filp, char __user *buffer,
			      size_t length, loff_t *offset)
{
	int bytes_read = 0;
	unsigned int val = 0;
	int timeout = 1 * HZ;	/*1s */

	kpprintf("Received Read request... ");


	/* ==MTT== in this hypothetical use-case, we trace the value of an irq
	 * counter before blocking, and after blocking. This fakes tracing the
	 * blocking read of a sound driver for instance.
	 * The read is resumed upon completion of a DMA channel.
	 */
	val = nb_irqs & 0xFF;


	kptrace_write_record("wait_event");
	wait_event_interruptible_timeout(wq, got_irq, timeout);
	kptrace_write_record("resume");

	got_irq = 0;

	/* We just fake a buffer by filling the size provided
	 * with the buffer number.
	 * Round to 32bits boundary.*/
	length &= 0xfffffff0;

	while (bytes_read < length) {
		if (copy_to_user(buffer, &val, sizeof(val)))
			goto __failed;
		bytes_read += sizeof(val);
	}
	return bytes_read;

__failed:
	return -EFAULT;
}

static ssize_t mttsample_write(struct file *filp,
			       const char *buff, size_t len, loff_t *off)
{

	/* for demo purpose, let's output a printk and an mtt_print
	 * version of the info.
	 */
	printk(KERN_INFO "write done.\n");

	return len;
}

/*
 * The ioctl() implementation
 */
static long /*__unopt*/ mttsample_ioctl(struct file *file, unsigned int cmd,
					unsigned long arg)
{
	int err = 0, ret = 0;

	if (_IOC_TYPE(cmd) != DEMOAPP_IOC_MAGIC)
		return -ENOTTY;
	if (_IOC_NR(cmd) > DEMOAPP_IOC_MAXNR)
		return -ENOTTY;

	if (_IOC_DIR(cmd) & _IOC_READ)
		err =
		    !access_ok(VERIFY_WRITE, (void __user *)arg,
			       _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ, (void __user *)arg,
				 _IOC_SIZE(cmd));
	if (err)
		return -EFAULT;

	switch (cmd) {

	case DEMOAPP_IOCRESET:
		del_timer_sync(&fake_timer);
		got_irq = 0;
		nb_irqs = 0;
		break;

	case DEMOAPP_IOCSTART:
		add_timer(&fake_timer);
		break;

	case DEMOAPP_IOCSTOP:
		del_timer_sync(&fake_timer);
		break;

	default:
		return -ENOTTY;
	}

	return ret;
}

static void mttsample_capture_DMA_callback(unsigned long cpu)
{
	got_irq = 1;
	wake_up_interruptible(&wq);

	/* Fake a hardware contention leading to an IRQ delay */
	if (nb_irqs++ != 5)
		mod_timer(&fake_timer, jiffies + DELAY);
	else
		mod_timer(&fake_timer, jiffies + DELAY * 3);
}

module_init(mttsample_init);
module_exit(mttsample_exit);
MODULE_LICENSE("GPL");
