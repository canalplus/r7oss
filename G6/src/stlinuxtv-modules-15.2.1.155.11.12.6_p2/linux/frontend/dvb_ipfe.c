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

Source file name : dvb_ipfe.c
Author :           SD

Definition of ioctl Extensions to the LinuxDVB API for IP Frontend(v3) and other
supports

Date        Modification                                    Name
----        ------------                                    --------
23-Mar-11   Created                                         SD

 ************************************************************************/

#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/semaphore.h>
#include <linux/module.h>
#include <linux/list.h>
#include <linux/kthread.h>
#include <linux/version.h>
#include <asm/processor.h>

#include "dvb_ipfe.h"
#include "dvbdev.h"
#include "dvb_frontend.h"
#include <linux/dvb/version.h>

static int dvb_ipfe_debug;
module_param(dvb_ipfe_debug, int, 0664);
MODULE_PARM_DESC(dvb_ipfe_debug, "enable ipfe debug messages");

#define dprintk(x...) do { if (dvb_ipfe_debug) printk(x); } while (0)

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 35))
static int dvb_ipfe_do_ioctl(struct inode *inode, struct file *file,
			     unsigned int cmd, void *parg)
#else
static int dvb_ipfe_do_ioctl(struct file *file, unsigned int cmd, void *parg)
#endif
{
	struct dvb_device *dvbdev = file->private_data;
	struct dvb_ipfe *fe = (struct dvb_ipfe *)dvbdev->priv;
	int err = -EOPNOTSUPP;

	if ((file->f_flags & O_ACCMODE) == O_RDONLY &&
            _IOC_DIR(cmd) != _IOC_READ )
		return -EPERM;

	fe->id = dvbdev->id;

	switch (cmd) {

	case FE_GET_INFO:
		{
			struct dvb_frontend_info *info = parg;
			memcpy(info, &fe->ops.info,
			       sizeof(struct dvb_frontend_info));
			err = 0;
			break;
		}
	case FE_IP_START:
		{
			if (fe->ops.start)
				err = fe->ops.start(fe);
			break;
		}

	case FE_IP_FEC_START:
		{
			if (fe->ops.fec_start)
				err = fe->ops.fec_start(fe);
			break;
		}


	case FE_IP_STOP:
		{
			if (fe->ops.stop)
				err = fe->ops.stop(fe);
			break;
		}

	case FE_IP_FEC_STOP:
		{
			if (fe->ops.fec_stop)
				err = fe->ops.fec_stop(fe);
			break;
		}


	case FE_IP_SET_FILTER:
		{
			struct fe_ip_parameters *tvps = NULL;
			tvps = (struct fe_ip_parameters *)parg;
			if (fe->ops.set_control)
				err = fe->ops.set_control(fe,
							FE_IP_CTRL_SET_CONFIG,
							(void *)tvps);
			if (err) {
				printk(KERN_ERR "%s: failed", __func__);
				break;
			}
			if (fe->ops.start
			    && (tvps->flags == FE_IP_IMMEDIATE_START))
				err = fe->ops.start(fe);
			break;
		}

	case FE_IP_SET_FILTER_FEC:
		{
			struct fe_ip_fec_config_s *tvps = NULL;
			tvps = (struct fe_ip_fec_config_s *)parg;
			if (fe->ops.set_fec)
				err = fe->ops.set_fec(fe, FE_IP_FEC_RESET_STATS,
							(void *)tvps);
			if (err)
				printk(KERN_ERR "%s: failed", __func__);
			break;
		}

	case FE_IP_SET_FILTER_RTCP:
		{
			struct fe_ip_rtcp_config_s *tvps = NULL;
			tvps = (struct fe_ip_rtcp_config_s *)parg;
			if (fe->ops.set_rtcp)
				err = fe->ops.set_rtcp(fe,
							(void *)tvps);
			if (err)
				printk(KERN_ERR "%s: failed", __func__);
			break;
		}

	case FE_IP_GET_CAPS:
		{
			if (fe->ops.get_capability)
				err = fe->ops.get_capability(fe, (void *)parg);
			if (err)
				printk(KERN_ERR "%s: failed\n", __func__);
			break;
		}

	default:
		return -ENOTTY;
	}

	return err;
}

static long generic_unlocked_ioctl(struct file *file, unsigned int foo,
				   unsigned long bar)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 35))
	return dvb_generic_ioctl(NULL, file, foo, bar);
#else
	return dvb_generic_ioctl(file, foo, bar);
#endif
}

static int ipfe_release(struct inode *node, struct file *file)
{
	struct dvb_device *dvbdev = file->private_data;
	struct dvb_ipfe *fe = (struct dvb_ipfe *)dvbdev->priv;
	int ret = 0;

	dvb_generic_release(node, file);

	/*
	 * We have initialized max users to ~0 for this device. So, when
	 * the user count reaches the max, it means, this was the last
	 * user of this dvb device. So, we close the ipfe
	 */
	if (dvbdev->users == ~0) {
		if (fe->ops.stop){
			ret = fe->ops.stop(fe);
			if (ret)
				printk(KERN_INFO "%s: failed to stop the IPFE (%d)\n",
					__func__, ret);
		}
	}

	return ret;
}

static const struct file_operations dvb_ipfe_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = generic_unlocked_ioctl,
	.open = dvb_generic_open,
	.release = ipfe_release
};

int dvb_register_ipfe(struct dvb_adapter *dvb, struct dvb_ipfe *fe)
{

	int err = 0;

	struct dvb_ipfe_private *fepriv = NULL;
	static const struct dvb_device dvbdev_template = {
		.users = ~0,
		.writers = 1,
		.readers = (~0) - 1,
		.fops = &dvb_ipfe_fops,
		.kernel_ioctl = dvb_ipfe_do_ioctl
	};

	fe->ipfe_priv = kzalloc(sizeof(struct dvb_ipfe_private), GFP_KERNEL);

	if (fe->ipfe_priv == NULL) {
		err = -ENOMEM;
		return err;
	}
	fepriv = fe->ipfe_priv;
	fe->dvb = dvb;

	sema_init(&fepriv->sem, 1);
	dprintk("DVB: registering adapter for ipfrontend ...\n");

	err = dvb_register_device(fe->dvb, &fepriv->dvbdev, &dvbdev_template,
				  fe, DVB_DEVICE_FRONTEND);
	return err;
}

EXPORT_SYMBOL(dvb_register_ipfe);

int dvb_unregister_ipfe(struct dvb_ipfe *fe)
{
	struct dvb_ipfe_private *fepriv = fe->ipfe_priv;
	if (fepriv->dvbdev) {
		dvb_unregister_device(fepriv->dvbdev);
		/* fe is invalid now */
		kfree(fe->ipfe_priv);
	}
	return 0;
}

EXPORT_SYMBOL(dvb_unregister_ipfe);
