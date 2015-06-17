#define DRV_MODULE_VERSION	"May_09"

#if defined(CONFIG_VLAN_8021Q) || defined(CONFIG_VLAN_8021Q_MODULE)
#define STMMAC_VLAN_TAG_USED
#include <linux/if_vlan.h>
#endif

#include "common.h"
#ifdef CONFIG_STMMAC_TIMER
#include "stmmac_timer.h"
#endif

struct stmmac_priv {
	struct net_device *dev;
	struct device *device;

	int pbl;
	int is_gmac;
	void (*fix_mac_speed) (void *priv, unsigned int speed);
	void *bsp_priv;

	int bus_id;
	int phy_addr;
	int smi_bus_enabled;

	/*
	 * Copied from the stmmacphy devices's platform_data.
	 * Only valid if smi_bus_enabled set.
	 */
	int phy_mask;
	phy_interface_t phy_interface;
	int (*phy_reset) (void *priv);
	int phy_irq;

	struct phy_device *phydev;
	int oldlink;
	int speed;
	int oldduplex;

	struct mii_bus *mii;

	spinlock_t lock; /* interface lock */
	spinlock_t tx_lock; /* tx lock */

	struct dma_desc *dma_tx	____cacheline_aligned;
	dma_addr_t dma_tx_phy;
	struct dma_desc *dma_rx	____cacheline_aligned;
	dma_addr_t dma_rx_phy;
	struct sk_buff **tx_skbuff;
	struct sk_buff **rx_skbuff;
	dma_addr_t *rx_skbuff_dma;

	unsigned int cur_rx, dirty_rx;
	unsigned int cur_tx, dirty_tx;
	unsigned int dma_tx_size;
	unsigned int dma_rx_size;
	unsigned int dma_buf_sz;
	unsigned int rx_buff;
	struct tasklet_struct tx_task;
	struct stmmac_extra_stats xstats;
	struct mac_device_info *mac_type;
	unsigned int flow_ctrl;
	unsigned int pause;
	u32 msg_enable;
	int rx_csum;
	int tx_coe;
	int wolopts;
	int wolenabled;
	int shutdown;
	int tx_coalesce;
#ifdef CONFIG_STMMAC_TIMER
	struct stmmac_timer *tm;
#endif
#ifdef STMMAC_VLAN_TAG_USED
	struct vlan_group *vlgrp;
#endif
	int vlan_rx_filter;
};

extern int stmmac_mdio_unregister(struct net_device *ndev);
extern int stmmac_mdio_register(struct net_device *ndev);
