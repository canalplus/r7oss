/************************************************************************
Copyright (C) 2012 STMicroelectronics. All Rights Reserved.

This file is part of the STLinuxTV Library.

STLinuxTV is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

STLinuxTV is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with STLinuxTV; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The STLinuxTV Library may alternatively be licensed under a proprietary
license from ST.

Source file name : dvb_ca.c

Implements LinuxDVB CA device for STLinuxTV
************************************************************************/
#include <dvb/dvb_adaptation.h>
#include <dvb/dvb_adapt_ca.h>
#include <linux/version.h>

static int stm_dvb_ca_ioctl(
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 35))
				   struct inode *inode,
#endif
				   struct file *file,
				   unsigned int cmd, void *arg)
{
	struct dvb_device *dvbdev = file->private_data;
	struct stm_dvb_ca_s *stm_ca = dvbdev->priv;
	int err;

	switch (cmd) {
	case CA_RESET:
		err = stm_dvb_ca_reset(stm_ca);
		break;
	case CA_GET_CAP:
		err = stm_dvb_ca_get_cap(stm_ca, (ca_caps_t *)arg);
		break;
	case CA_GET_SLOT_INFO:
		err = stm_dvb_ca_get_slot_info(stm_ca, (ca_slot_info_t *)arg);
		break;
	case CA_GET_DESCR_INFO:
		err = stm_dvb_ca_get_descr_info(stm_ca, (ca_descr_info_t *)arg);
		break;
	case CA_SET_PID:
		err = stm_dvb_ca_set_pid(stm_ca, (ca_pid_t *) arg);
		break;
	case CA_SET_DESCR:
		err = stm_dvb_ca_set_descr(stm_ca, (ca_descr_t *) arg);
		break;
	case CA_SEND_MSG:
		err = stm_dvb_ca_send_msg(stm_ca, (dvb_ca_msg_t *) arg);
		break;
	case CA_GET_MSG:
		err = stm_dvb_ca_get_msg(stm_ca, (dvb_ca_msg_t *) arg);
		break;
	default:
		printk(KERN_ERR "ca%d: Unhandled CA ioctl 0x%x\n",
		       stm_ca->ca_id, cmd);
		err = -EOPNOTSUPP;
	}
	return err;
}

static int ca_count = 0;

long stm_dvb_ca_generic_ioctl(struct file *file, unsigned int foo,
			      unsigned long bar)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 35))
	return dvb_generic_ioctl(NULL, file, foo, bar);
#else
	return dvb_generic_ioctl(file, foo, bar);
#endif
}

static const struct file_operations ca_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = stm_dvb_ca_generic_ioctl,
	.open = dvb_generic_open,
	.release = dvb_generic_release,
};

static struct dvb_device stm_dvb_ca = {
	.users = 1,
	.readers = 1,
	.writers = 1,
	.fops = &ca_fops,
	.kernel_ioctl = stm_dvb_ca_ioctl,
};

int stm_dvb_ca_attach(struct dvb_adapter *adapter,
		      struct stm_dvb_demux_s *demux,
		      struct stm_dvb_ca_s **allocated_ca)
{
	int err = 0;
	struct stm_dvb_ca_s *stm_ca = NULL;

	stm_ca = kzalloc(sizeof(struct stm_dvb_ca_s), GFP_KERNEL);
	if (!stm_ca) {
		err = -ENOMEM;
		goto error;
	}

	/* Initialize ca object */
	stm_ca->ca_id = ca_count++;
	INIT_LIST_HEAD(&stm_ca->channel_list);

	/* Link demux and ca devices */
	demux->ca = stm_ca;
	stm_ca->demux = demux;

	err = dvb_register_device(adapter, &stm_ca->cadev, &stm_dvb_ca,
				  stm_ca, DVB_DEVICE_CA);
	if (err < 0)
		goto error;

	*allocated_ca = stm_ca;
	return 0;
error:
	allocated_ca = NULL;
	demux->ca = NULL;
	if (stm_ca)
		kfree(stm_ca);
	return err;
}

int stm_dvb_ca_delete(struct stm_dvb_ca_s *stm_ca)
{
	int err = 0;

	if (stm_ca->cadev)
		dvb_unregister_device(stm_ca->cadev);

	if (stm_ca->demux)
		stm_ca->demux->ca = NULL;

	/* Delete the session object - detaches/deletes any child transforms */
	if (stm_ca->session) {
		err = stm_ce_session_delete((stm_ce_handle_t) stm_ca->session);
		if (err)
			printk(KERN_ERR "Unable to delete stm_ce session "
			       "(%d)\n", err);
	}

	/* Free-up memory allocated for ca object */
	if (stm_ca->profile)
		kfree(stm_ca->profile);
	kfree(stm_ca);

	return err;
}
