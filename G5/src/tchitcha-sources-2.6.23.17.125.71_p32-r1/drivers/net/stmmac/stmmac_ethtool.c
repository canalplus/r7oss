/*
 * drivers/net/stmmac/stmmac_ethtool.c
 *
 * STMMAC Ethernet Driver
 * Ethtool support for STMMAC Ethernet Driver
 *
 * Author: Giuseppe Cavallaro
 *
 * Copyright (c) 2006-2007 STMicroelectronics
 *
 */
#include <linux/kernel.h>
#include <linux/etherdevice.h>
#include <linux/mm.h>
#include <linux/ethtool.h>
#include <linux/mii.h>
#include <linux/phy.h>
#include <linux/io.h>

#include "stmmac.h"

#define REG_SPACE_SIZE	0x1054
#define MAC100_ETHTOOL_NAME	"st_mac100"
#define GMAC_ETHTOOL_NAME	"st_gmac"

void stmmac_ethtool_getdrvinfo(struct net_device *dev,
			       struct ethtool_drvinfo *info)
{
	struct stmmac_priv *priv = netdev_priv(dev);

	if (!priv->is_gmac)
		strcpy(info->driver, MAC100_ETHTOOL_NAME);
	else
		strcpy(info->driver, GMAC_ETHTOOL_NAME);

	strcpy(info->version, DRV_MODULE_VERSION);
	info->fw_version[0] = '\0';
	return;
}

int stmmac_ethtool_getsettings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	struct stmmac_priv *priv = netdev_priv(dev);
	struct phy_device *phy = priv->phydev;
	int rc;
	if (phy == NULL) {
		printk(KERN_ERR "%s: %s: PHY is not registered\n",
		       __FUNCTION__, dev->name);
		return -ENODEV;
	}

	if (!netif_running(dev)) {
		printk(KERN_ERR "%s: interface is disabled: we cannot track "
		       "link speed / duplex setting\n", dev->name);
		return -EBUSY;
	}

	cmd->transceiver = XCVR_INTERNAL;
	spin_lock_irq(&priv->lock);
	rc = phy_ethtool_gset(phy, cmd);
	spin_unlock_irq(&priv->lock);
	return rc;
}

int stmmac_ethtool_setsettings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	struct stmmac_priv *priv = netdev_priv(dev);
	struct phy_device *phy = priv->phydev;
	int rc;

	spin_lock(&priv->lock);
	rc = phy_ethtool_sset(phy, cmd);
	spin_unlock(&priv->lock);

	return rc;
}

u32 stmmac_ethtool_getmsglevel(struct net_device *dev)
{
	struct stmmac_priv *priv = netdev_priv(dev);
	return priv->msg_enable;
}

void stmmac_ethtool_setmsglevel(struct net_device *dev, u32 level)
{
	struct stmmac_priv *priv = netdev_priv(dev);
	priv->msg_enable = level;

}

int stmmac_check_if_running(struct net_device *dev)
{
	if (!netif_running(dev))
		return -EBUSY;
	return (0);
}

int stmmac_ethtool_get_regs_len(struct net_device *dev)
{
	return (REG_SPACE_SIZE);
}

void stmmac_ethtool_gregs(struct net_device *dev,
			  struct ethtool_regs *regs, void *space)
{
	int i;
	u32 *reg_space = (u32 *) space;

	struct stmmac_priv *priv = netdev_priv(dev);

	memset(reg_space, 0x0, REG_SPACE_SIZE);

	if (!priv->is_gmac) {
		/* MAC registers */
		for (i = 0; i < 12; i++)
			reg_space[i] = readl(dev->base_addr + (i * 4));
		/* DMA registers */
		for (i = 0; i < 9; i++)
			reg_space[i + 12] =
			    readl(dev->base_addr + (DMA_BUS_MODE + (i * 4)));
		reg_space[22] = readl(dev->base_addr + DMA_CUR_TX_BUF_ADDR);
		reg_space[23] = readl(dev->base_addr + DMA_CUR_RX_BUF_ADDR);
	} else {
		/* MAC registers */
		for (i = 0; i < 55; i++)
			reg_space[i] = readl(dev->base_addr + (i * 4));
		/* DMA registers */
		for (i = 0; i < 22; i++)
			reg_space[i + 55] =
			    readl(dev->base_addr + (DMA_BUS_MODE + (i * 4)));
	}

	return;
}

int stmmac_ethtool_set_tx_csum(struct net_device *netdev, u32 data)
{
	if (data) {
		netdev->features |= NETIF_F_HW_CSUM;
	} else {
		netdev->features &= ~NETIF_F_HW_CSUM;
	}

	return 0;
}

u32 stmmac_ethtool_get_rx_csum(struct net_device *dev)
{
	struct stmmac_priv *priv = netdev_priv(dev);

	return (priv->rx_csum);
}

static void
stmmac_get_pauseparam(struct net_device *netdev,
		      struct ethtool_pauseparam *pause)
{
	struct stmmac_priv *priv = netdev_priv(netdev);

	spin_lock(&priv->lock);

	pause->rx_pause = 0;
	pause->tx_pause = 0;
	pause->autoneg = priv->phydev->autoneg;

	if (priv->flow_ctrl & FLOW_RX)
		pause->rx_pause = 1;
	if (priv->flow_ctrl & FLOW_TX)
		pause->tx_pause = 1;

	spin_unlock(&priv->lock);
	return;
}

static int
stmmac_set_pauseparam(struct net_device *netdev,
		      struct ethtool_pauseparam *pause)
{
	struct stmmac_priv *priv = netdev_priv(netdev);
	struct phy_device *phy = priv->phydev;
	int new_pause = FLOW_OFF;
	int ret = 0;

	spin_lock(&priv->lock);

	if (pause->rx_pause)
		new_pause |= FLOW_RX;
	if (pause->tx_pause)
		new_pause |= FLOW_TX;

	priv->flow_ctrl = new_pause;

	if (phy->autoneg) {
		if (netif_running(netdev)) {
			struct ethtool_cmd cmd;
			/* auto-negotiation automatically restarted */
			cmd.cmd = ETHTOOL_NWAY_RST;
			cmd.supported = phy->supported;
			cmd.advertising = phy->advertising;
			cmd.autoneg = phy->autoneg;
			cmd.speed = phy->speed;
			cmd.duplex = phy->duplex;
			cmd.phy_address = phy->addr;
			ret = phy_ethtool_sset(phy, &cmd);
		}
	} else {
		unsigned long ioaddr = netdev->base_addr;
		priv->mac_type->ops->flow_ctrl(ioaddr, phy->duplex,
					       priv->flow_ctrl, priv->pause);
	}
	spin_unlock(&priv->lock);
	return ret;
}

static struct {
	const char str[ETH_GSTRING_LEN];
} ethtool_stats_keys[] = {
	{
	"tx_underflow"}, {
	"tx_carrier"}, {
	"tx_losscarrier"}, {
	"tx_heartbeat"}, {
	"tx_deferred"}, {
	"tx_vlan"}, {
	"rx_vlan"}, {
	"tx_jabber"}, {
	"tx_frame_flushed"}, {
	"tx_payload_error"}, {
	"tx_ip_header_error"}, {
	"rx_desc"}, {
	"rx_partial"}, {
	"rx_runt"}, {
	"rx_toolong"}, {
	"rx_collision"}, {
	"rx_crc"}, {
	"rx_lenght"}, {
	"rx_mii"}, {
	"rx_multicast"}, {
	"rx_gmac_overflow"}, {
	"rx_watchdog"}, {
	"da_rx_filter_fail"}, {
	"sa_rx_filter_fail"}, {
	"rx_missed_cntr"}, {
	"rx_overflow_cntr"}, {
	"tx_undeflow_irq"}, {
	"tx_process_stopped_irq"}, {
	"tx_jabber_irq"}, {
	"rx_overflow_irq"}, {
	"rx_buf_unav_irq"}, {
	"rx_process_stopped_irq"}, {
	"rx_watchdog_irq"}, {
	"tx_early_irq"}, {
	"fatal_bus_error_irq"}, {
	"threshold"}, {
	"tx_task_n"}, {
	"rx_poll_n"}, {
	"tx_pkt_n"}, {
	"rx_pkt_n"}, {
	"avg_tx_pkt_on_sched"}, {
	"avg_rx_pkt_on_sched"}, {
	"dma_tx_normal_irq"}, {
"dma_rx_normal_irq"},};

static int stmmac_stats_count(struct net_device *dev)
{
	return EXTRA_STATS;
}

static void stmmac_ethtool_stats(struct net_device *dev,
				 struct ethtool_stats *dummy, u64 *buf)
{
	struct stmmac_priv *priv = netdev_priv(dev);
	unsigned long ioaddr = dev->base_addr;
	u32 *extra;
	int i;

	priv->mac_type->ops->dma_diagnostic_fr(&dev->stats, &priv->xstats,
					       ioaddr);
	if (priv->xstats.tx_task_n)
		priv->xstats.avg_tx_pkt_on_sched =
			(priv->xstats.tx_pkt_n / priv->xstats.tx_task_n);
	if (priv->xstats.rx_poll_n)
		priv->xstats.avg_rx_pkt_on_sched =
			(priv->xstats.rx_pkt_n / priv->xstats.rx_poll_n);

	extra = (u32 *) & priv->xstats;

	for (i = 0; i < EXTRA_STATS; i++)
		buf[i] = extra[i];
	return;
}

static void stmmac_get_strings(struct net_device *dev, u32 stringset, u8 *buf)
{
	switch (stringset) {
	case ETH_SS_STATS:
		memcpy(buf, &ethtool_stats_keys, sizeof(ethtool_stats_keys));
		break;
	default:
		WARN_ON(1);
		break;
	}
	return;
}

static void stmmac_get_wol(struct net_device *dev, struct ethtool_wolinfo *wol)
{
	struct stmmac_priv *priv = netdev_priv(dev);

	spin_lock_irq(&priv->lock);
	if (priv->wolenabled == PMT_SUPPORTED) {
		wol->supported = WAKE_MAGIC /*| WAKE_UCAST */ ;
		wol->wolopts = priv->wolopts;
	}
	spin_unlock_irq(&priv->lock);
}

static int stmmac_set_wol(struct net_device *dev, struct ethtool_wolinfo *wol)
{
	struct stmmac_priv *priv = netdev_priv(dev);
	u32 support = WAKE_MAGIC;

	if (priv->wolenabled == PMT_NOT_SUPPORTED)
		return -EINVAL;

	if (wol->wolopts & ~support)
		return -EINVAL;

	if (wol->wolopts == 0)
		device_set_wakeup_enable(priv->device, 0);
	else
		device_set_wakeup_enable(priv->device, 1);

	spin_lock_irq(&priv->lock);
	priv->wolopts = wol->wolopts;
	spin_unlock_irq(&priv->lock);

	return 0;
}

struct ethtool_ops stmmac_ethtool_ops = {
	.begin = stmmac_check_if_running,
	.get_drvinfo = stmmac_ethtool_getdrvinfo,
	.get_settings = stmmac_ethtool_getsettings,
	.set_settings = stmmac_ethtool_setsettings,
	.get_msglevel = stmmac_ethtool_getmsglevel,
	.set_msglevel = stmmac_ethtool_setmsglevel,
	.get_regs = stmmac_ethtool_gregs,
	.get_regs_len = stmmac_ethtool_get_regs_len,
	.get_link = ethtool_op_get_link,
	.get_rx_csum = stmmac_ethtool_get_rx_csum,
	.get_tx_csum = ethtool_op_get_tx_csum,
	.set_tx_csum = stmmac_ethtool_set_tx_csum,
	.get_sg = ethtool_op_get_sg,
	.set_sg = ethtool_op_set_sg,
#ifdef NETIF_F_TSO
	.get_tso = ethtool_op_get_tso,
	.set_tso = ethtool_op_set_tso,
#endif
	.get_pauseparam = stmmac_get_pauseparam,
	.set_pauseparam = stmmac_set_pauseparam,
	.get_ethtool_stats = stmmac_ethtool_stats,
	.get_stats_count = stmmac_stats_count,
	.get_strings = stmmac_get_strings,
	.get_wol = stmmac_get_wol,
	.set_wol = stmmac_set_wol,
};
