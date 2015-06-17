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

Source file name : dvb_ipfe.h
Author :           SD

Definition of ioctl Extensions to the LinuxDVB API for IP Frontend(v3) and other
supports:header file

Date        Modification                                    Name
----        ------------                                    --------
23-Mar-11   Created                                         SD

 ************************************************************************/

#ifndef _DVB_IPFE_H_
#define _DVB_IPFE_H_

#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/dvb/stm_frontend.h>
#include <linux/dvb/frontend.h>

#include "dvbdev.h"

struct dvb_ipfe;
struct dvb_ipfe_parameters;

struct dvb_ipfe_ops {
	struct dvb_frontend_info info;
	struct fe_ip_caps caps;
	int (*start) (struct dvb_ipfe * fe);
	int (*fec_start) (struct dvb_ipfe * fe);
	int (*stop) (struct dvb_ipfe * fe);
	int (*fec_stop) (struct dvb_ipfe * fe);
	int (*set_control) (struct dvb_ipfe * fe, fe_ip_control_t flag,
			    void *parg);
	int (*set_fec) (struct dvb_ipfe *fe, fe_ip_fec_control_t flag,
			      void *args);
	int (*set_rtcp) (struct dvb_ipfe * fe, void *parg);
	int (*get_capability) (struct dvb_ipfe * fe, void *parg);
};

struct dvb_ipfe_private {
	struct dvb_device *dvbdev;
	struct semaphore sem;
	char *buffer;

};

struct dvb_ipfe {
	struct dvb_ipfe_ops ops;
	struct dvb_adapter *dvb;
	void *ipfe_priv;
	void *ipfec_priv;
	struct dmx_demux *demux;
	u32 id;
};

int dvb_register_ipfe(struct dvb_adapter *dvb, struct dvb_ipfe *fe);
int dvb_unregister_ipfe(struct dvb_ipfe *fe);
int dvb_ipfe_init(struct dvb_adapter *, struct dvb_ipfe *, struct dmx_demux *);

#endif
