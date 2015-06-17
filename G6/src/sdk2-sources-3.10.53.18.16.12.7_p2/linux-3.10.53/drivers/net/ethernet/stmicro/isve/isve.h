/*******************************************************************************
  Copyright (C) 2012  STMicroelectronics Ltd

  This program is free software; you can redistribute it and/or modify it
  under the terms and conditions of the GNU General Public License,
  version 2, as published by the Free Software Foundation.

  This program is distributed in the hope it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.

  The full GNU General Public License is included in this distribution in
  the file called "COPYING".

  Author: Giuseppe Cavallaro <peppe.cavallaro@st.com>
*******************************************************************************/
#ifndef __STMICRO_ISVE_CONFIG_H
#define __STMICRO_ISVE_CONFIG_H

#include "isve_hw.h"

#define ISVE_RESOURCE_NAME	"isve"
#define DRV_MODULE_VERSION	"Dec_2013"
#define ISVE_BUF_LEN		2048

struct isve_desc {
	void *buffer;
	dma_addr_t buf_addr;
};

struct isve_priv {
	struct net_device *dev;
	struct device *device;
	struct isve_extra_stats xstats;
	struct napi_struct napi;
	spinlock_t tx_lock;
	spinlock_t rx_lock;
	u32 msg_enable;

	struct isve_hw_ops *dfwd;
	void __iomem *ioaddr_dfwd;
	unsigned int queue_number;
	int irq_ds;
	struct isve_desc *rx_desc;
	unsigned int cur_rx;
	unsigned int skip_hdr;
	unsigned int hw_rem_hdr;

	void __iomem *ioaddr_upiim;
	unsigned int upstream_queue_size;
	int irq_us;
	struct isve_hw_ops *upiim;
	struct isve_desc *tx_desc;
	unsigned int cur_tx;
	unsigned int dirty_tx;
	char *ifname;
};

extern void isve_set_ethtool_ops(struct net_device *netdev);
#endif
