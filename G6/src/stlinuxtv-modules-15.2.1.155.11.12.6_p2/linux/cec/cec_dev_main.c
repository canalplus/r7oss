/************************************************************************
 * Copyright (C) 2011 STMicroelectronics. All Rights Reserved.
 *
 * This file is part of the STLinuxTV Library.
 *
 * STLinuxTV is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation.
 *
 * STLinuxTV is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with STLinuxTV; see the file COPYING.  If not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * The STLinuxTV Library may alternatively be licensed under a proprietary
 * license from ST.
 *
 * Source file name : cec_dev_main.c
 * Author :           Nidhi Chadha
 *
 * Header for STLinuxTV package
 *
 * Date        Modification                                    Name
 * ----        ------------                                    --------
 * 24-10-2012   Created                                         nidhic
 *
 *   ************************************************************************/

#include <linux/compiler.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/poll.h>

#include <linux/uaccess.h>
#include <linux/semaphore.h>

#include "cec_internal.h"

#define CEC_DEVICE_NAME		"cec0"

static dev_t firstdevice;
struct stm_cec *cec;

static int stmcec_open(struct inode *inode, struct file *filp)
{

	int error;
	struct stm_cec *dev = container_of(inode->i_cdev, struct stm_cec, cdev);

	CEC_DEBUG_MSG("initialise cec object\n");

	mutex_lock(&dev->cecdev_mutex);

	error = stm_cec_new(0, &(dev->cec_device));
	if (error) {
		CEC_ERROR_MSG("Error in CEC init (%d)\n", error);
		goto cec_open_failed;
	}

	filp->private_data = dev;

cec_open_failed:
	mutex_unlock(&dev->cecdev_mutex);
	return error;
}

static ssize_t stmcec_read(struct file *filp, char __user *buf, size_t sz,
			   loff_t *off)
{
	struct stm_cec *dev = (struct stm_cec *)filp->private_data;
	ssize_t retval;
	struct stmcecio_msg *cecio_msg;
	stm_cec_msg_t cec_msg;
#if ENABLE_CEC_DEBUG
	int index;
#endif

	CEC_DEBUG_MSG("Receive/read cec message\n");
	mutex_lock(&dev->cecrw_mutex);

	retval = stm_cec_receive_msg(dev->cec_device, &(cec_msg));
	if (retval) {
		CEC_ERROR_MSG("Failed to receive cec msg\n");
		goto cec_read_failed;
	}

	cecio_msg = (struct stmcecio_msg *)&cec_msg;

	CEC_DEBUG_MSG("CEC msg(%d) :\n", cecio_msg->len);
#if ENABLE_CEC_DEBUG
	for (index = 0; index < cecio_msg->len; index++)
		CEC_DEBUG_MSG("0x%x ", cecio_msg->data[index]);
#endif

	retval = copy_to_user(buf, cecio_msg, sizeof(*cecio_msg));
	if (retval)
		CEC_ERROR_MSG("Failed to copy data to user (%d)\n", retval);

cec_read_failed:
	mutex_unlock(&dev->cecrw_mutex);
	return retval;
}

static ssize_t stmcec_write(struct file *filp, const char __user *buf,
			    size_t sz, loff_t *off)
{
	struct stm_cec *dev = (struct stm_cec *)filp->private_data;
	stm_cec_msg_t *cec_msg;
	struct stmcecio_msg cecio_msg;
	int retval;
	int retries = 1;
#if ENABLE_CEC_DEBUG
	int index;
#endif

	CEC_DEBUG_MSG("Send/write cec message\n");

	mutex_lock(&dev->cecrw_mutex);

	retval = copy_from_user(&cecio_msg, (void *)buf, sizeof(cecio_msg));
	if (retval) {
		CEC_ERROR_MSG("Failed to copy data from user (%d)\n", retval);
		goto cec_write_failed;
	}

	CEC_DEBUG_MSG("CEC msg to be sent(%d) :\n", cecio_msg.len);
#if ENABLE_CEC_DEBUG
	for (index = 0; index < cecio_msg.len; index++)
		CEC_DEBUG_MSG("0x%x ", cecio_msg.data[index]);
#endif
	CEC_DEBUG_MSG("Message to be sent to CEC driver :\n");
	cec_msg = (stm_cec_msg_t *) &cecio_msg;
	retval =
	    stm_cec_send_msg((stm_cec_h) dev->cec_device, retries,
			     (stm_cec_msg_t *) cec_msg);
	if (retval)
		CEC_ERROR_MSG("Failed to send cec msg (%d)\n", retval);

cec_write_failed:
	mutex_unlock(&dev->cecrw_mutex);
	return retval;
}

static unsigned int stmcec_poll(struct file *file,
					struct poll_table_struct *poll_table)
{
	int ret;
	struct stm_cec *cec = file->private_data;

	if (!cec->subscription)
		return POLLERR;

	poll_wait(file, &cec->wait_queue, poll_table);

	ret = stm_event_wait(cec->subscription, 0, 0, NULL, NULL);
	if (ret != -ENOSPC)
		return 0;

	return (POLLIN | POLLPRI | POLLRDNORM);
}

static int stmcec_release(struct inode *inode, struct file *filp)
{
	int retval;
	struct stm_cec *dev = filp->private_data;

	mutex_lock(&dev->cecdev_mutex);
	mutex_lock(&dev->cecops_mutex);
	mutex_lock(&dev->cecrw_mutex);

	if (dev->subscription) {
		if (stm_event_subscription_delete(dev->subscription))
			CEC_DEBUG_MSG("Failed to delete event subscription\n");
		dev->subscription = NULL;
	}

	CEC_DEBUG_MSG("Release cec dev\n");

	retval = stm_cec_delete(dev->cec_device);
	if (retval)
		CEC_ERROR_MSG("Failure in stm_cec_delete with error %d\n",
			      retval);

	dev->cec_device = NULL;

	mutex_unlock(&dev->cecrw_mutex);
	mutex_unlock(&dev->cecops_mutex);
	mutex_unlock(&dev->cecdev_mutex);

	return retval;
}

const struct file_operations cec_fops = {
	.owner = THIS_MODULE,
	.open = stmcec_open,
	.read = stmcec_read,
	.write = stmcec_write,
	.poll = stmcec_poll,
	.unlocked_ioctl = stmcec_ioctl,
	.release = stmcec_release
};

static struct stm_cec *__init stmcec_create_dev_struct(void)
{
	struct stm_cec *cec;
	CEC_DEBUG_MSG("Create cec dev struct\n");
	cec = kzalloc(sizeof(struct stm_cec), GFP_KERNEL);
	if (!cec) {
		CEC_ERROR_MSG("Error in allocation of dev struct\n");
		return NULL;
	}

	mutex_init(&cec->cecdev_mutex);
	mutex_init(&cec->cecops_mutex);
	mutex_init(&cec->cecrw_mutex);
	init_waitqueue_head(&cec->wait_queue);

	return cec;
}

static int __init stmcec_register_device(struct stm_cec *cec, int id)
{
	CEC_DEBUG_MSG("Register as dev file\n");
	cdev_init(&(cec->cdev), &cec_fops);
	cec->cdev.owner = THIS_MODULE;
	kobject_set_name(&(cec->cdev.kobj), "cec%d", id);

	if (cdev_add(&(cec->cdev), MKDEV(MAJOR(firstdevice), id), 1)) {
		CEC_ERROR_MSG("cdev_add failure\n");
		return -ENODEV;
	}
	/* create class in order to make dev */
	cec->cdev_class = class_create(THIS_MODULE, CEC_DEVICE_NAME);

	if (cec->cdev_class == NULL) {
		CEC_ERROR_MSG("failure in class_create\n");
		cdev_del(&cec->cdev);
		unregister_chrdev_region(firstdevice, 1);
	}

	cec->cdev_class_dev =
	    device_create(cec->cdev_class, NULL, firstdevice, NULL,
			  CEC_DEVICE_NAME);
	if (cec->cdev_class_dev == NULL) {
		CEC_ERROR_MSG("failure in device_create\n");
		class_destroy(cec->cdev_class);
		unregister_chrdev_region(firstdevice, 1);
		return -1;
	}

	return 0;
}

static void __exit stmcec_exit(void)
{
	CEC_DEBUG_MSG("exit func of stmcec module\n");
	/* TODO : Error check */
	device_unregister(cec->cdev_class_dev);
	class_destroy(cec->cdev_class);
	cdev_del(&cec->cdev);
	unregister_chrdev_region(firstdevice, 1);
}

static int __init stmcec_init(void)
{
	CEC_DEBUG_MSG("Init func of stmcec module\n");
	if (alloc_chrdev_region(&firstdevice, 0, 1, "cec") < 0) {
		CEC_ERROR_MSG(KERN_ERR
			      "%s: unable to allocate device numbers\n",
			      __func__);
		return -ENODEV;
	}

	cec = stmcec_create_dev_struct();
	if (cec == NULL) {
		CEC_ERROR_MSG("failure in creating the dev struct\n");
		return -ENOMEM;
	}

	if (stmcec_register_device(cec, 0)) {
		CEC_ERROR_MSG("failure in registering cec device t\n");
		goto exit_error;
	}

	return 0;

exit_error:
	return -ENODEV;
}

module_init(stmcec_init);
module_exit(stmcec_exit);

MODULE_LICENSE("GPL");
