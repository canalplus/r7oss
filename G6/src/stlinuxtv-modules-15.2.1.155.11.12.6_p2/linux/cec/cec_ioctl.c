/***********************************************************************
 *
 * File: APPS_STLinuxTV/linux/cec/cec_ioctl.c
 * Copyright (c) 2005-2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#include <linux/version.h>
#include <linux/compiler.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/wait.h>

#include <linux/uaccess.h>
#include <linux/semaphore.h>

#include "stmcec.h"
#include "stm_event.h"
#include "cec_internal.h"

static int stmcec_set_logical_address(struct stm_cec *dev, unsigned long arg)
{
	struct stmcecio_logical_addr logical_addr;
	stm_cec_ctrl_type_t cec_ctrl;
	int retval;

	CEC_DEBUG_MSG("setting logical address\n");

	mutex_lock(&dev->cecops_mutex);
	mutex_lock(&dev->cecrw_mutex);

	retval =
	    copy_from_user(&logical_addr, (void *)arg, sizeof(logical_addr));
	if (retval != 0) {
		CEC_ERROR_MSG("failure in copying from user\n");
		goto set_logical_addr_failed;
	}

	cec_ctrl.logic_addr_param.logical_addr = logical_addr.addr;
	cec_ctrl.logic_addr_param.enable = 1;

	CEC_DEBUG_MSG
	    ("logical address to set in cec driver : %d ( enabled : %d)\n",
	     cec_ctrl.logic_addr_param.logical_addr,
	     cec_ctrl.logic_addr_param.enable);

	retval = stm_cec_set_compound_control(dev->cec_device,
					STM_CEC_CTRL_FLAG_UPDATE_LOGICAL_ADDR,
					&cec_ctrl);
	if (retval != 0)
		CEC_ERROR_MSG("error in stm_cec_compound_control\n");

set_logical_addr_failed:
	mutex_unlock(&dev->cecrw_mutex);
	mutex_unlock(&dev->cecops_mutex);
	return retval;
}

static int stmcec_get_logical_address(struct stm_cec *dev, unsigned long arg)
{
	struct stmcecio_logical_addr logical_addr;
	stm_cec_ctrl_type_t cec_ctrl;
	int retval;

	mutex_lock(&dev->cecops_mutex);

	/* initializing cec ctrl */
	cec_ctrl.logic_addr_param.logical_addr = 0;
	cec_ctrl.logic_addr_param.enable = 0;

	CEC_DEBUG_MSG("logical address  : %d ( enabled : %d)\n",
		      cec_ctrl.logic_addr_param.logical_addr,
		      cec_ctrl.logic_addr_param.enable);

	CEC_DEBUG_MSG("get logical address from cec driver\n");

	retval = stm_cec_get_compound_control(dev->cec_device,
					STM_CEC_CTRL_FLAG_UPDATE_LOGICAL_ADDR,
					&cec_ctrl);
	if (retval) {
		CEC_ERROR_MSG("error in stm_cec_compound_control\n");
		goto get_cec_addr_failed;
	}
	CEC_DEBUG_MSG
	    ("logical address received from cec driver : %d ( enabled : %d)\n",
	     cec_ctrl.logic_addr_param.logical_addr,
	     cec_ctrl.logic_addr_param.enable);

	logical_addr.addr = cec_ctrl.logic_addr_param.logical_addr;
	logical_addr.enable = cec_ctrl.logic_addr_param.enable;

	retval = copy_to_user((void *)arg, &logical_addr, sizeof(logical_addr));
	if (retval != 0) {
		CEC_ERROR_MSG("error in copy_to_user\n");
		retval = -EFAULT;
	}

get_cec_addr_failed:
	mutex_unlock(&dev->cecops_mutex);
	return retval;
}

static int stmcec_subscribe_event(struct stm_cec *dev, unsigned long arg)
{
	int retval;
	struct stmcec_event_subscription evt_subscription;
	stm_event_subscription_entry_t subscription_entry;

	CEC_DEBUG_MSG("Create subscription : cec_object : %p\n",
		      dev->cec_device);

	mutex_lock(&dev->cecops_mutex);

	if (copy_from_user
	    (&evt_subscription, (void *)arg, sizeof(evt_subscription))) {
		CEC_ERROR_MSG("error in copy_from_user\n");
		retval = -EFAULT;
		goto evt_subs_failed;
	}

	/* dev->subscription_id = (void*)evt_subscription.id ; */
	subscription_entry.cookie = (void *)evt_subscription.id; /* TBC */
	subscription_entry.object = dev->cec_device;
	subscription_entry.event_mask = evt_subscription.type;
	retval = stm_event_subscription_create(&subscription_entry, 1,
						    &dev->subscription);
	if (retval) {
		CEC_ERROR_MSG("Error cant create event subscription\n");
		goto evt_subs_failed;
	}

	retval = stm_event_set_wait_queue(dev->subscription,
						&dev->wait_queue, true);
	if (retval) {
		CEC_ERROR_MSG("Unable to set wait queue for CEC events\n");
		goto evt_set_queue_failed;
	}

	mutex_unlock(&dev->cecops_mutex);
	return 0;

evt_set_queue_failed:
	if(stm_event_subscription_delete(dev->subscription))
		CEC_DEBUG_MSG("Failed to delete event subscription\n");
evt_subs_failed:
	mutex_unlock(&dev->cecops_mutex);
	return retval;
}

static int stmcec_unsubscribe_event(struct stm_cec *dev, unsigned long arg)
{
	int retval;

	mutex_lock(&dev->cecops_mutex);

	CEC_DEBUG_MSG("Delete subscription cec_object : %p\n",
		      dev->cec_device);

	retval = stm_event_subscription_delete(dev->subscription);
	if (retval)
		CEC_ERROR_MSG("Error: cant delete event subscription\n");

	mutex_unlock(&dev->cecops_mutex);

	return retval;
}

static int stmcec_dqevent(struct stm_cec *dev,
				unsigned long arg, int nonblocking)
{
	int retval = 0, timeout;
	__u32 number_of_events;
	stm_event_info_t evt_info;
	struct stmcec_event read_event;

	CEC_DEBUG_MSG("dequeue event cec_object : %p\n", dev->cec_device);

	if (nonblocking)
		timeout = 0;
	else
		timeout = -1;

	retval = stm_event_wait(dev->subscription, timeout, 1,
				&number_of_events, &evt_info);
	if (retval != 0)
		return retval;

	read_event.id = (__u32) evt_info.cookie;
	read_event.type = evt_info.event.event_id;

	retval = copy_to_user((void *)arg, &read_event, sizeof(read_event));
	if (retval != 0) {
		CEC_ERROR_MSG("error in copy_to_user\n");
		retval = -EFAULT;
	}
	return retval;
}

long stmcec_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct stm_cec *dev = (struct stm_cec *)filp->private_data;
	long retval = 0;

	switch (cmd) {
	case STMCECIO_SET_LOGICAL_ADDRESS:
		retval = stmcec_set_logical_address(dev, arg);
		break;

	case STMCECIO_GET_LOGICAL_ADDRESS:
		retval = stmcec_get_logical_address(dev, arg);
		break;

	case STMCECIO_DQEVENT:
		retval = stmcec_dqevent(dev, arg, (filp->f_flags & O_NONBLOCK));
		break;

	case STMCECIO_SUBSCRIBE_EVENT:
		retval = stmcec_subscribe_event(dev, arg);
		break;

	case STMCECIO_UNSUBSCRIBE_EVENT:
		retval = stmcec_unsubscribe_event(dev, arg);
		break;
	}

	return retval;
}
