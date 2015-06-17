/*******************************************************************************
  ISVE Ethtool support

  Copyright (C) 2012 STMicroelectronics Ltd

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

#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/interrupt.h>
#include <linux/mii.h>
#include <linux/phy.h>
#include <linux/io.h>

#include "isve.h"

struct isve_stats {
	char stat_string[ETH_GSTRING_LEN];
	int sizeof_stat;
	int stat_offset;
};

#define ISVE_STAT(m)	\
	{ #m, FIELD_SIZEOF(struct isve_extra_stats, m),	\
	offsetof(struct isve_priv, xstats.m)}

static const struct isve_stats isve_gstrings_stats[] = {
	ISVE_STAT(tx_pkt_n),
	ISVE_STAT(rx_pkt_n),
	ISVE_STAT(dfwd_ovf_drop_cnt),
	ISVE_STAT(dfwd_panic_drop_cnt),
	ISVE_STAT(dfwd_int_drop_pkt_int),
	ISVE_STAT(dfwd_int_free_fifo),
	ISVE_STAT(dfwd_int_free_fifo_full),
	ISVE_STAT(dfwd_int_free_fifo_empty),
	ISVE_STAT(dfwd_int_fifo),
	ISVE_STAT(dfwd_int_fifo_full),
	ISVE_STAT(dfwd_int_fifo_not_empty),
	ISVE_STAT(upiim_inq_int_fill_fifo),
	ISVE_STAT(upiim_inq_int_fill_full_fifo),
	ISVE_STAT(upiim_inq_int_fill_empty_fifo),
	ISVE_STAT(upiim_inq_int_free_fifo),
	ISVE_STAT(upiim_inq_int_free_full_fifo),
	ISVE_STAT(upiim_inq_int_free_empty_fifo),
};

#define ISVE_STATS_LEN ARRAY_SIZE(isve_gstrings_stats)

static void isve_ethtool_getdrvinfo(struct net_device *dev,
				    struct ethtool_drvinfo *info)
{
	strlcpy(info->driver, ISVE_RESOURCE_NAME, sizeof(info->driver));

	strcpy(info->version, DRV_MODULE_VERSION);
	info->fw_version[0] = '\0';
}

static u32 isve_ethtool_getmsglevel(struct net_device *dev)
{
	struct isve_priv *priv = netdev_priv(dev);
	return priv->msg_enable;
}

static void isve_ethtool_setmsglevel(struct net_device *dev, u32 level)
{
	struct isve_priv *priv = netdev_priv(dev);
	priv->msg_enable = level;

}

static int isve_check_if_running(struct net_device *dev)
{
	if (!netif_running(dev))
		return -EBUSY;
	return 0;
}

static void isve_get_ethtool_stats(struct net_device *dev,
				   struct ethtool_stats *dummy, u64 *data)
{
	struct isve_priv *priv = netdev_priv(dev);
	int i, j = 0;

	priv->dfwd->get_stats(priv->ioaddr_dfwd, &priv->xstats);

	for (i = 0; i < ISVE_STATS_LEN; i++) {
		char *p = (char *)priv + isve_gstrings_stats[i].stat_offset;
		data[j++] = (isve_gstrings_stats[i].sizeof_stat ==
			     sizeof(u64)) ? (*(u64 *) p) : (*(u32 *) p);
	}
}

static int isve_get_sset_count(struct net_device *netdev, int sset)
{
	switch (sset) {
	case ETH_SS_STATS:
		return ISVE_STATS_LEN;
	default:
		return -EOPNOTSUPP;
	}
}

static void isve_get_strings(struct net_device *dev, u32 stringset, u8 *data)
{
	int i;
	u8 *p = data;

	switch (stringset) {
	case ETH_SS_STATS:
		for (i = 0; i < ISVE_STATS_LEN; i++) {
			memcpy(p, isve_gstrings_stats[i].stat_string,
			       ETH_GSTRING_LEN);
			p += ETH_GSTRING_LEN;
		}
		break;
	default:
		WARN_ON(1);
		break;
	}
}

static const struct ethtool_ops isve_ethtool_ops = {
	.begin = isve_check_if_running,
	.get_drvinfo = isve_ethtool_getdrvinfo,
	.get_msglevel = isve_ethtool_getmsglevel,
	.set_msglevel = isve_ethtool_setmsglevel,
	.get_ethtool_stats = isve_get_ethtool_stats,
	.get_strings = isve_get_strings,
	.get_sset_count = isve_get_sset_count,
};

void isve_set_ethtool_ops(struct net_device *netdev)
{
	SET_ETHTOOL_OPS(netdev, &isve_ethtool_ops);
}
