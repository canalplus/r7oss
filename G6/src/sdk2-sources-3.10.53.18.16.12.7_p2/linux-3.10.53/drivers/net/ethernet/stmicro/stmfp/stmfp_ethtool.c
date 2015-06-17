/***************************************************************************
  FPIF Ethtool support

  Copyright (C) 2011-2014 STMicroelectronics Ltd

  This program is free software; you can redistribute it and/or modify it
  under the terms and conditions of the GNU General Public License,
  version 2, as published by the Free Software Foundation.

  This program is distributed in the hope it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along
  with this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.

  The full GNU General Public License is included in this distribution in
  the file called "COPYING".

  Author: manish rathi <manish.rathi@st.com>
***************************************************************************/

#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/interrupt.h>
#include <linux/mii.h>
#include <linux/phy.h>
#include <linux/io.h>

#include "stmfp_main.h"

#define GMAC_ETHTOOL_NAME	"fpif_gmac"
#define REG_SPACE_SIZE (0x8f8)

static void fpif_ethtool_getdrvinfo(struct net_device *dev,
				    struct ethtool_drvinfo *info)
{
	strncpy(info->driver, GMAC_ETHTOOL_NAME, sizeof(info->driver));
	strncpy(info->version, DRV_MODULE_VERSION, sizeof(info->version));
	info->fw_version[0] = '\0';
}

static int fpif_ethtool_getsettings(struct net_device *dev,
				    struct ethtool_cmd *cmd)
{
	struct fpif_priv *priv = netdev_priv(dev);
	struct phy_device *phy = priv->phydev;

	if (!phy) {
		netdev_err(dev, "PHY is not registered\n");
		return -ENODEV;
	}
	if (!netif_running(dev)) {
		netdev_err(dev, "Interface is disabled\n");
		return -EBUSY;
	}
	cmd->transceiver = XCVR_INTERNAL;

	return phy_ethtool_gset(phy, cmd);
}

static int fpif_ethtool_setsettings(struct net_device *dev,
				    struct ethtool_cmd *cmd)
{
	struct fpif_priv *priv = netdev_priv(dev);
	struct phy_device *phy = priv->phydev;

	return phy_ethtool_sset(phy, cmd);
}

static int fpif_ethtool_get_regs_len(struct net_device *dev)
{
	return REG_SPACE_SIZE;
}

static void fpif_ethtool_gregs(struct net_device *dev,
			       struct ethtool_regs *regs, void *space)
{
	int i;
	u32 *reg_space = space;
	struct fpif_priv *priv = netdev_priv(dev);

	memset(reg_space, 0x0, REG_SPACE_SIZE);
	/* MAC registers */
	for (i = 0; i < REG_SPACE_SIZE / sizeof(u32); i++)
		reg_space[i] = readl(priv->rgmii_base + i * sizeof(u32));
}

static u32 fpif_ethtool_getmsglevel(struct net_device *dev)
{
	struct fpif_priv *priv = netdev_priv(dev);

	return priv->msg_enable;
}

static void fpif_ethtool_setmsglevel(struct net_device *dev, u32 level)
{
	struct fpif_priv *priv = netdev_priv(dev);

	priv->msg_enable = level;
}

static int fpif_check_if_running(struct net_device *dev)
{
	if (!netif_running(dev))
		return -EBUSY;

	return 0;
}

static const struct ethtool_ops fpif_ethtool_ops = {
	.begin = fpif_check_if_running,
	.get_drvinfo = fpif_ethtool_getdrvinfo,
	.get_settings = fpif_ethtool_getsettings,
	.set_settings = fpif_ethtool_setsettings,
	.get_regs = fpif_ethtool_gregs,
	.get_regs_len = fpif_ethtool_get_regs_len,
	.get_msglevel = fpif_ethtool_getmsglevel,
	.set_msglevel = fpif_ethtool_setmsglevel,
	.get_link = ethtool_op_get_link,
};

void fpif_set_ethtool_ops(struct net_device *netdev)
{
	SET_ETHTOOL_OPS(netdev, &fpif_ethtool_ops);
}
