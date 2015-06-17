/**************************************************************************

  ST  Fastpath Interface driver
  Copyright(c) 2011 - 2014 ST Microelectronics Corporation.

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

  Contact Information:
  Manish Rathi <manish.rathi@st.com>

TBD
- Watchdog
- VLAN offload
- Flow control support
- ndo_poll_controller callback
- ioctl for startup queues
- Power Management
- Change MTU for shared channel
**************************************************************************/

#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/vmalloc.h>
#include <linux/pagemap.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/platform_device.h>
#include <linux/tcp.h>
#include <net/tcp.h>
#include <linux/ipv6.h>
#include <net/checksum.h>
#include <net/ip6_checksum.h>
#include <linux/mii.h>
#include <linux/ethtool.h>
#include <linux/if_vlan.h>
#include <linux/cpu.h>
#include <linux/smp.h>
#include <linux/skbuff.h>
#include <linux/phy.h>
#include <linux/netdevice.h>
#include <linux/stmfp.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_net.h>
#include <linux/reset.h>
#include <linux/clk.h>
#include <net/netevent.h>
#include <linux/inetdevice.h>
#include <../net/bridge/br_private.h>
#include "stmfp_main.h"

static const u32 default_msg_level = (NETIF_MSG_LINK |
				      NETIF_MSG_IFUP | NETIF_MSG_IFDOWN |
				      NETIF_MSG_TIMER);

static int debug = -1;

static struct fp_promisc_info fp_promisc[NUM_PHY_INTFS];

static struct fp_qos_queue fp_qos_queue_info[NUM_QOS_QUEUES] = {
	{256, 36, 255, 255, 36},/* DOCSIS QoS Queue 0 */
	{256, 30, 36, 255, 36},	/* DOCSIS QoS Queue 1 */
	{256, 30, 36, 255, 36},	/* DOCSIS QoS Queue2 */
	{256, 30, 36, 255, 36},	/* DOCSIS QoS Queue3 */
	{256, 36, 255, 255, 36},/* GIGE QoS Queue0 */
	{256, 30, 36, 255, 36},	/* GIGE QoS Queue1 */
	{256, 30, 36, 255, 36},	/* GIGE QoS Queue2 */
	{256, 30, 36, 255, 36},	/* GIGE QoS Queue3 */
	{32, 32, 32, 32, 6},	/* ISIS QoS Queue */
	{32, 32, 32, 32, 6},	/* AP QoS Queue */
	{32, 32, 32, 32, 6},	/* NP QoS Queue */
	{32, 32, 32, 32, 6},	/* WIFI QoS Queue */
	{32, 32, 32, 32, 6},	/* RECIRC QoS Queue */
};

static struct fp_qos_queue fpl_qos_queue_info[NUM_QOS_QUEUES] = {
	 {256, 36, 255, 255, 22},	/* DOCSIS QoS Queue 0 */
	 {256, 24, 36, 255, 22},		/* DOCSIS QoS Queue 1 */
	 {256, 24, 36, 255, 22},		/* DOCSIS QoS Queue2 */
	 {256, 24, 36, 255, 22},	/* DOCSIS QoS Queue3 */
	 {256, 36, 255, 255, 22},	/* GIGE0 QoS Queue0 */
	 {256, 24, 36, 255, 22},		/* GIGE0 QoS Queue1 */
	 {256, 24, 36, 255, 22},		/* GIGE0 QoS Queue2 */
	 {256, 24, 36, 255, 22},	/* GIGE0 QoS Queue3 */
	 {256, 36, 255, 255, 22},	/* GIGE1 QoS Queue0 */
	 {256, 24, 36, 255, 22},		/* GIGE1 QoS Queue1 */
	 {256, 24, 36, 255, 22},		/* GIGE1 QoS Queue2 */
	 {256, 24, 36, 255, 22},	/* GIGE1 QoS Queue3 */
	 {32, 31, 31, 31, 16},	/* DMA0 QoS Queue */
	 {32, 31, 31, 31, 16},	/* DMA1 QoS Queue */
	 {32, 31, 31, 31, 16},	/* RECIRC QoS Queue */
};

static int fpif_clean_tx_ring(struct fpif_priv *priv);

static struct notifier_block ovs_dp_device_notifier;

static int is_fpport(struct net_device *netdev)
{
	if ((!strcmp(netdev->name, "fpdocsis")) ||
	    (!strcmp(netdev->name, "fpgige0")) ||
	     (!strcmp(netdev->name, "fpgige1")))
		return 1;
	return 0;
}


static int stmfp_if_config_dt(struct platform_device *pdev,
				struct plat_fpif_data *plat,
				  struct device_node *node, int version)
{
	int ret = 0;

	if (of_get_property(node, "fixed-link", NULL)) {
		plat->phy_bus_name = devm_kzalloc(&pdev->dev,
						   8 * sizeof(char *),
						   GFP_KERNEL);
		strcpy(plat->phy_bus_name, "fixed");
	}

	of_property_read_u32(node, "st,phy-addr", &plat->phy_addr);
	of_property_read_u32(node, "st,phy-bus-id", &plat->bus_id);
	plat->interface = of_get_phy_mode(node);
	if (plat->phy_bus_name)
		plat->mdio_bus_data = NULL;
	else
		plat->mdio_bus_data = devm_kzalloc(&pdev->dev,
					sizeof(struct stmfp_mdio_bus_data),
					GFP_KERNEL);
	plat->tso_enabled = 1;
	plat->rx_dma_ch = 0;
	plat->tx_dma_ch = 0;
	strncpy(plat->ifname, node->name, sizeof(plat->ifname));
	if (!strcmp(node->name, "fpdocsis")) {
		plat->iftype = DEVID_DOCSIS;
		plat->q_idx = 3;
		plat->buf_thr = 35;
	} else if (!strcmp(node->name, "fpgige0")) {
		plat->iftype = DEVID_GIGE0;
		plat->q_idx = 7;
		plat->buf_thr = 35;
	} else if (!strcmp(node->name, "fpgige1")) {
		plat->iftype = DEVID_GIGE1;
		if (version == FP)
			plat->q_idx = 8;
		else
			plat->q_idx = 11;
		plat->buf_thr = 35;
	} else {
		dev_warn(&pdev->dev, "Incorrect interface names in DT\n");
		ret = -ENODEV;
	}

	return ret;
}


static u64 stmfp_dma_mask = DMA_BIT_MASK(32);
static int stmfp_probe_config_dt(struct platform_device *pdev,
				  struct plat_stmfp_data *plat)
{
	struct device_node *np = pdev->dev.of_node;
	struct device_node *node;
	struct plat_fpif_data *if_data;
	int ret;

	if (!np)
		return -ENODEV;

	of_property_read_u32(np, "st,fp_clk_rate", &plat->fp_clk_rate);

	if (of_device_is_compatible(np, "st,fplite")) {
		plat->version = FPLITE;
		plat->available_l2cam = 128;
		plat->l2cam_size = 128;
		plat->common_cnt = 56;
		plat->empty_cnt = 16;
	} else {
		plat->version = FP;
		plat->available_l2cam = 256;
		plat->l2cam_size = 256;
		plat->common_cnt = 60;
		plat->empty_cnt = 6;
	}

	if (!pdev->dev.dma_mask)
		pdev->dev.dma_mask = &stmfp_dma_mask;

	for_each_child_of_node(np, node) {
		if_data = devm_kzalloc(&pdev->dev,
				sizeof(struct plat_fpif_data), GFP_KERNEL);
		if (if_data == NULL)
			return -ENOMEM;

		ret = stmfp_if_config_dt(pdev, if_data, node, plat->version);
		if (ret)
			return ret;
		plat->if_data[if_data->iftype] = if_data;
	}

	return 0;
}


static inline void fpif_write_reg(void __iomem *fp_reg, u32 val)
{
	writel(val, fp_reg);
}


static inline struct sk_buff *fpif_poll_start_skb(struct fpif_priv *priv,
						  gfp_t mask)
{
	struct sk_buff *skb = NULL;
	struct net_device *netdev = priv->netdev;

	skb = __netdev_alloc_skb(netdev, priv->rx_buffer_size, mask);

	return skb;
}


static void fpif_rxb_release(struct fpif_priv *priv)
{
	unsigned int i;
	struct device *dev = priv->dev;
	struct fpif_rxdma *rxdma_ptr = priv->rxdma_ptr;

	if (netif_msg_ifdown(priv))
		dev_dbg(dev, "%s\n", __func__);
	for (i = 0; i < FPIF_RX_RING_SIZE; i++) {
		if (rxdma_ptr->fp_rx_skbuff[i].skb) {
			dma_unmap_single(dev,
					 rxdma_ptr->fp_rx_skbuff[i].dma_ptr,
					 priv->rx_buffer_size, DMA_FROM_DEVICE);
			dev_kfree_skb_any(rxdma_ptr->fp_rx_skbuff[i].skb);
			rxdma_ptr->fp_rx_skbuff[i].skb = NULL;
			rxdma_ptr->fp_rx_skbuff[i].dma_ptr = 0;
		}
	}

	return;
}


static void fpif_txr_release(struct fpif_priv *priv, int cpu)
{
	unsigned int i;
	struct device *dev = priv->dev;
	struct fpif_txdma *txdma_ptr = priv->txdma_ptr;
	struct fp_tx_ring *tx_ring;

	for (i = 0; i < FPIF_TX_RING_SIZE; i++) {
		tx_ring = &txdma_ptr->fp_tx_skbuff[cpu][i];
		if (tx_ring->skb) {
			dma_unmap_single(dev, tx_ring->dma_ptr,
					 tx_ring->skb->len, DMA_TO_DEVICE);
			dev_kfree_skb_any(tx_ring->skb);
			tx_ring->skb = NULL;
			tx_ring->dma_ptr = 0;
			tx_ring->skb_data = NULL;
			tx_ring->len_eop = 0;
			tx_ring->priv = NULL;
		}
	}
	txdma_ptr->head_tx_sw[cpu] = 0;
	txdma_ptr->last_tx_sw[cpu] = 0;
	txdma_ptr->done_tx_sw[cpu] = 0;
	txdma_ptr->start_tx_sw[cpu] = 0;

	return;
}


static void fpif_txb_release(struct fpif_priv *priv)
{
	unsigned int i;
	struct fpif_txdma *txdma_ptr = priv->txdma_ptr;

	for (i = 0; i < FPIF_TX_RING_SIZE; i++) {
		fpif_txr_release(priv, 0);
		fpif_txr_release(priv, 1);
		txdma_ptr->fp_txb[i].idx = IDX_INV;
	}
	txdma_ptr->avail[0] = FPIF_TX_RING_SIZE - 1;
	txdma_ptr->avail[1] = FPIF_TX_RING_SIZE - 1;
}


static inline void fpif_q_rx_buffer(struct fpif_rxdma *rxdma_ptr,
			struct sk_buff *skb, dma_addr_t buf_ptr)
{
	void __iomem *hw_buf_ptr;
	u32 head_rx = rxdma_ptr->head_rx;

	hw_buf_ptr = &rxdma_ptr->bufptr[head_rx];
	fpif_write_reg(hw_buf_ptr, buf_ptr);
	rxdma_ptr->fp_rx_skbuff[head_rx].skb = skb;
	rxdma_ptr->fp_rx_skbuff[head_rx].dma_ptr = buf_ptr;
	rxdma_ptr->head_rx = (head_rx + 1) & RX_RING_MOD_MASK;
	fpif_write_reg(&rxdma_ptr->rx_ch_reg->rx_cpu,
		       rxdma_ptr->head_rx);
}


static int fpif_rxb_setup(struct fpif_priv *priv)
{
	unsigned int i;
	int ret = 0;
	struct sk_buff *skb;
	dma_addr_t dma_addr;
	struct device *dev = priv->dev;

	/* Setup the skbuff rings */
	for (i = 0; i < FPIF_RX_BUFS - 1; i++) {
		skb = fpif_poll_start_skb(priv, GFP_KERNEL);
		if (NULL == skb) {
			netdev_err(priv->netdev, "Err:Allocating RX buf\n");
			if (i)
				break;
			fpif_rxb_release(priv);
			return -ENOMEM;
		} else {
			dma_addr = dma_map_single(dev, skb->data,
						  priv->rx_buffer_size,
						  DMA_FROM_DEVICE);
			if (dma_mapping_error(dev, dma_addr)) {
				dev_err(priv->dev, "Err dma map addr=%x\n",
					dma_addr);
				continue;
			}
			fpif_q_rx_buffer(priv->rxdma_ptr, skb, dma_addr);
		}
	}
	if (netif_msg_ifup(priv))
		dev_dbg(dev, "%s %d done\n", __func__, priv->id);

	return ret;
}


static void fp_txdma_setup(struct fpif_priv *priv)
{
	u32 current_tx;
	struct fpif_txdma *txdma_ptr = priv->txdma_ptr;
	struct fpif_grp *fpgrp = priv->fpgrp;
	int tx_ch = priv->tx_dma_ch, i;

	if (txdma_ptr->users) {
		set_bit(priv->id, &txdma_ptr->users);
		return;
	}
	set_bit(priv->id, &txdma_ptr->users);

	/* TX DMA Init */
	txdma_ptr->tx_ch_reg = &fpgrp->txbase->per_ch[tx_ch];
	txdma_ptr->bufptr = fpgrp->txbase->buf[tx_ch];
	txdma_ptr->head_tx = 0;
	txdma_ptr->last_tx = 0;

	spin_lock_init(&txdma_ptr->fpif_txlock);
	spin_lock_init(&txdma_ptr->txlock[0]);
	spin_lock_init(&txdma_ptr->txlock[1]);

	fpif_write_reg(&txdma_ptr->txbase->tx_bpai_clear, BIT(tx_ch));

	current_tx = readl(&txdma_ptr->txbase->tx_irq_enables[0]);
	current_tx |= BIT(tx_ch);
	fpif_write_reg(&txdma_ptr->txbase->tx_irq_enables[0], current_tx);

	fpif_write_reg(&txdma_ptr->tx_ch_reg->tx_delay, DELAY_TX_INTRH);

	txdma_ptr->avail[0] = FPIF_TX_RING_SIZE - 1;
	txdma_ptr->avail[1] = FPIF_TX_RING_SIZE - 1;

	for (i = 0; i < FPIF_TX_RING_SIZE; i++)
		txdma_ptr->fp_txb[i].idx = IDX_INV;

	if (netif_msg_ifup(priv))
		dev_dbg(priv->dev, "%s %x done\n", __func__, priv->id);
}


static void fp_txdma_release(struct fpif_priv *priv)
{
	u32 current_tx;
	struct fpif_txdma *txdma_ptr = priv->txdma_ptr;
	int tx_ch = priv->tx_dma_ch;

	clear_bit(priv->id, &txdma_ptr->users);
	if (txdma_ptr->users)
		return;

	current_tx = readl(&txdma_ptr->txbase->tx_irq_enables[0]);
	current_tx &= ~(BIT(tx_ch));
	fpif_write_reg(&txdma_ptr->txbase->tx_irq_enables[0], current_tx);
	fpif_write_reg(&txdma_ptr->txbase->tx_bpai_clear, BIT(tx_ch));

	txdma_ptr->head_tx = 0;
	txdma_ptr->last_tx = 0;

	fpif_txb_release(priv);

	if (netif_msg_ifdown(priv))
		dev_dbg(priv->dev, "%s %x done\n", __func__, priv->id);
}

static int fp_rxdma_setup(struct fpif_priv *priv)
{
	u32 current_rx;
	struct fpif_rxdma *rxdma_ptr = priv->rxdma_ptr;
	struct fpif_grp *fpgrp = priv->fpgrp;
	int err, rx_ch = priv->rx_dma_ch;

	if (rxdma_ptr->users) {
		set_bit(priv->id, &rxdma_ptr->users);
		return 0;
	}

	set_bit(priv->id, &rxdma_ptr->users);

	/* RX DMA Init */
	rxdma_ptr->rx_ch_reg = &fpgrp->rxbase->per_ch[rx_ch];
	rxdma_ptr->bufptr = fpgrp->rxbase->buf[rx_ch];
	rxdma_ptr->head_rx = 0;
	rxdma_ptr->last_rx = 0;

	fpif_write_reg(&rxdma_ptr->rxbase->rx_bpai_clear, BIT(rx_ch));
	current_rx = readl(&rxdma_ptr->rxbase->rx_irq_enables[rx_ch]);
	current_rx |= BIT(rx_ch);
	fpif_write_reg(&rxdma_ptr->rxbase->rx_irq_enables[rx_ch], current_rx);

	err = fpif_rxb_setup(priv);
	if (netif_msg_ifup(priv))
		dev_dbg(priv->dev, "%s %x done\n", __func__, priv->id);

	return err;
}

static void fp_rxdma_release(struct fpif_priv *priv)
{
	u32 current_rx;
	struct fpif_rxdma *rxdma_ptr = priv->rxdma_ptr;
	int rx_ch = priv->rx_dma_ch;

	clear_bit(priv->id, &rxdma_ptr->users);
	if (rxdma_ptr->users)
		return;

	fpif_write_reg(&rxdma_ptr->rxbase->rx_bpai_clear, BIT(rx_ch));
	current_rx = readl(&rxdma_ptr->rxbase->rx_irq_enables[rx_ch]);
	current_rx &= ~(BIT(rx_ch));
	fpif_write_reg(&rxdma_ptr->rxbase->rx_irq_enables[rx_ch], current_rx);
	fpif_write_reg(&rxdma_ptr->rx_ch_reg->rx_delay, DELAY_RX_INTR);
	fpif_write_reg(&rxdma_ptr->rx_ch_reg->rx_thresh, RXDMA_THRESH);

	rxdma_ptr->head_rx = 0;
	rxdma_ptr->last_rx = 0;

	fpif_rxb_release(priv);
	if (netif_msg_ifup(priv))
		dev_dbg(priv->dev, "%s %x done\n", __func__, priv->id);
}


void add_tcam_docsis(void *base)
{
	struct fp_tcam_info tcam_info;
	int idx;

	memset(&tcam_info, 0, sizeof(tcam_info));
	tcam_info.sp = DEVID_DOCSIS;
	tcam_info.drop = 1;
	idx = TCAM_CM_SRCDOCSIS_DROP;
	add_tcam(base, &tcam_info, idx);
}

static void fp_qos_setup(struct fpif_grp *fpgrp, int num_q,
			struct fp_qos_queue *fp_qos)
{
	int idx;
	int start_q, size_q;

	start_q = 0;
	for (idx = 0; idx < num_q; idx++) {
		size_q = fp_qos[idx].q_size;
		fpif_write_reg(fpgrp->base + QOS_Q_START_PTR +
			       idx * QOS_Q_RPT_OFFSET, start_q);
		fpif_write_reg(fpgrp->base + QOS_Q_END_PTR +
			idx * QOS_Q_RPT_OFFSET, start_q + size_q - 1);
		fpif_write_reg(fpgrp->base + QOS_Q_CONTROL +
			       idx * QOS_Q_RPT_OFFSET, 0x00000003);
		fpif_write_reg(fpgrp->base + QOS_Q_THRES_0 +
			       idx * QOS_Q_RPT_OFFSET, fp_qos[idx].threshold_0);
		fpif_write_reg(fpgrp->base + QOS_Q_THRES_1 +
			       idx * QOS_Q_RPT_OFFSET, fp_qos[idx].threshold_1);
		fpif_write_reg(fpgrp->base + QOS_Q_THRES_2 +
			       idx * QOS_Q_RPT_OFFSET, fp_qos[idx].threshold_2);
		fpif_write_reg(fpgrp->base + QOS_Q_DROP_ENTRY_LIMIT +
			       idx * QOS_Q_RPT_OFFSET, size_q - 1);
		fpif_write_reg(fpgrp->base + QOS_Q_BUFF_RSRV +
			       idx * QOS_Q_RPT_OFFSET, fp_qos[idx].buf_rsvd);

		fpif_write_reg(fpgrp->base + QOS_Q_CLEAR_STATS +
			       idx * QOS_Q_RPT_OFFSET, 0);
		start_q = start_q + size_q;
	}

	/* Queue Manager common count setup */
	fpif_write_reg(fpgrp->base +
		       QOS_Q_COMMON_CNT_THRESH, fpgrp->plat->common_cnt);
	fpif_write_reg(fpgrp->base +
		       QOS_Q_COMMON_CNT_EMPTY_COUNT, fpgrp->plat->empty_cnt);
}


void stmfp_hwinit_badf(struct fpif_grp *fpgrp)
{
	fpif_write_reg(fpgrp->base + FILT_BADF, PKTLEN_ERR | MALFORM_PKT |
		       EARLY_EOF | L4_CSUM_ERR | IPV4_L3_CSUM_ERR |
		       SAMEIP_SRC_DEST | IPSRC_LOOP | TTL0_ERR | IPV4_BAD_HLEN);
	fpif_write_reg(fpgrp->base + FILT_BADF_DROP, PKTLEN_ERR |
		       MALFORM_PKT | EARLY_EOF | L4_CSUM_ERR |
		       IPV4_L3_CSUM_ERR | SAMEIP_SRC_DEST |
		       IPSRC_LOOP | IPV4_BAD_HLEN);

	fpif_write_reg(fpgrp->base + FP_MISC, MISC_DEFRAG_EN | MISC_PASS_BAD);
	fpif_write_reg(fpgrp->base + FP_DEFRAG_CNTRL, DEFRAG_REPLACE |
		       DEFRAG_PAD_REMOVAL);
}


static void fp_hwinit(struct fpif_grp *fpgrp)
{
	int idx;

	if (fpgrp->plat->platinit)
		fpgrp->plat->platinit(fpgrp);

	/* FP Hardware Init */
	fpif_write_reg(fpgrp->base + FP_SOFT_RST, 1);
	fpif_write_reg(fpgrp->base + FP_SOFT_RST, 0);

	stmfp_hwinit_badf(fpgrp);
	fpif_write_reg(fpgrp->base + RGMII0_OFFSET + RGMII_MACINFO0,
		       MACINFO_FULL_DUPLEX | MACINFO_SPEED_1000 |
		       MACINFO_RGMII_MODE | MACINFO_DONTDECAPIQ |
		       MACINFO_MTU1 | MACINFO_FLOWCTRL_REACTION_EN);
	fpif_write_reg(fpgrp->base + RGMII0_OFFSET + RGMII_RX_STAT_RESET, 0);
	fpif_write_reg(fpgrp->base + RGMII0_OFFSET + RGMII_TX_STAT_RESET, 0);
	fpif_write_reg(fpgrp->base + RGMII0_OFFSET + RGMII_GLOBAL_MACINFO3,
		       ETH_DATA_LEN + ETH_HLEN + VLAN_HLEN * 2 + ETH_FCS_LEN);

	if (fpgrp->plat->version != FP) {
		fpif_write_reg(fpgrp->base + RGMII1_OFFSET +
			       RGMII_MACINFO0, MACINFO_FULL_DUPLEX
			       | MACINFO_SPEED_1000 | MACINFO_RGMII_MODE
			       | MACINFO_DONTDECAPIQ | MACINFO_MTU1
			       | MACINFO_FLOWCTRL_REACTION_EN);
		fpif_write_reg(fpgrp->base + RGMII1_OFFSET +
			       RGMII_RX_STAT_RESET, 0);
		fpif_write_reg(fpgrp->base + RGMII1_OFFSET +
			       RGMII_TX_STAT_RESET, 0);
		fpif_write_reg(fpgrp->base + RGMII1_OFFSET +
			       RGMII_GLOBAL_MACINFO3, ETH_DATA_LEN +
			       ETH_HLEN + VLAN_HLEN * 2 + ETH_FCS_LEN);
	}

	if (fpgrp->plat->version != FPLITE)
		fpif_write_reg(fpgrp->base + FP_IMUX_TXDMA_RATE_CONTROL,
			       IMUX_TXDMA_RATE);
	fpif_write_reg(fpgrp->base + FP_IMUX_TXDMA_TOE_RATE_CONTROL,
		       IMUX_TXDMA_RATE);

	/* QManager Qos Queue Setup */
	if (fpgrp->plat->version == FPLITE)
		fp_qos_setup(fpgrp, 15, fpl_qos_queue_info);
	else
		fp_qos_setup(fpgrp, 13, fp_qos_queue_info);

	/* Session Startup Queues */
	for (idx = 0; idx < NUM_STARTUP_QUEUES; idx++) {
		fpif_write_reg(fpgrp->base + SU_Q_BUSY +
			       idx * STARTUP_Q_RPT_OFF, 0);
	}

	/* Session Startup Queue Control */
	fpif_write_reg(fpgrp->base + SU_Q_GLOBAL_PACKET_RESERVE,
		       SU_Q_MAX_PKT_G);
	fpif_write_reg(fpgrp->base + SU_Q_GLOBAL_BUFFER_RESERVE,
		       SU_Q_MAX_BUF_G);
	fpif_write_reg(fpgrp->base + SU_Q_PACKET_RESERVE, SU_Q_MAX_PKT);
	fpif_write_reg(fpgrp->base + SU_Q_BUFFER_RESERVE, SU_Q_MAX_BUF);

	/* Interface Settings */
	for (idx = 0; idx < NUM_PORTS; idx++) {
		fpif_write_reg(fpgrp->base + FP_PORTSETTINGS_LO + idx *
			       PORT_SETTINGS_RPT_OFF, DEF_QOSNONIP |
			       DEF_QOSIP | DEF_QOS_LBL | DEF_VLAN_ID |
			       NOVLAN_HW);
		fpif_write_reg(fpgrp->base + FP_PORTSETTINGS_HI +
			       idx * PORT_SETTINGS_RPT_OFF,
			       ETH_DATA_LEN + ETH_HLEN + VLAN_HLEN * 2);
	}

	/* QoS label level settings */
	fpif_write_reg(fpgrp->base + QOS_TRANSMIT_DESCRIPTOR +
		       0 * QOS_DESCRIPTOR_RPT_OFF, 0 << 3 | 3 << 1 | 1 << 0);
	fpif_write_reg(fpgrp->base + QOS_TRANSMIT_DESCRIPTOR +
		       1 * QOS_DESCRIPTOR_RPT_OFF, 1 << 3 | 3 << 1 | 1 << 0);
	fpif_write_reg(fpgrp->base + QOS_TRANSMIT_DESCRIPTOR +
		       2 * QOS_DESCRIPTOR_RPT_OFF, 0 << 3 | 2 << 1 | 1 << 0);
	fpif_write_reg(fpgrp->base + QOS_TRANSMIT_DESCRIPTOR +
		       3 * QOS_DESCRIPTOR_RPT_OFF, 1 << 3 | 2 << 1 | 1 << 0);
	fpif_write_reg(fpgrp->base + QOS_TRANSMIT_DESCRIPTOR +
		       4 * QOS_DESCRIPTOR_RPT_OFF, 0 << 3 | 1 << 1 | 0 << 0);
	fpif_write_reg(fpgrp->base + QOS_TRANSMIT_DESCRIPTOR +
		       5 * QOS_DESCRIPTOR_RPT_OFF, 1 << 3 | 1 << 1 | 0 << 0);
	fpif_write_reg(fpgrp->base + QOS_TRANSMIT_DESCRIPTOR +
		       6 * QOS_DESCRIPTOR_RPT_OFF, 0 << 3 | 0 << 1 | 0 << 0);
	fpif_write_reg(fpgrp->base + QOS_TRANSMIT_DESCRIPTOR +
		       7 * QOS_DESCRIPTOR_RPT_OFF, 1 << 3 | 0 << 1 | 0 << 0);

	/* DOCSIS SRR bit rate control */
	fpif_write_reg(fpgrp->base + QOS_Q_SRR_BIT_RATE_CTRL +
		       DEVID_DOCSIS * QOS_Q_SRR_BIT_RATE_CTRL_OFF,
		       BW_SHAPING | MAX_MBPS | fpgrp->plat->fp_clk_rate);

	/* GIGE0 SRR bit rate control */
	fpif_write_reg(fpgrp->base + QOS_Q_SRR_BIT_RATE_CTRL +
		       DEVID_GIGE0 * QOS_Q_SRR_BIT_RATE_CTRL_OFF,
		       BW_SHAPING | MAX_MBPS | fpgrp->plat->fp_clk_rate);

	/* GIGE1 SRR bit rate control */
	fpif_write_reg(fpgrp->base + QOS_Q_SRR_BIT_RATE_CTRL +
		       DEVID_GIGE1 * QOS_Q_SRR_BIT_RATE_CTRL_OFF,
		       BW_SHAPING | MAX_MBPS | fpgrp->plat->fp_clk_rate);

	/* EMUX thresholds */
	fpif_write_reg(fpgrp->base + FP_EMUX_THRESHOLD +
		       DEVID_DOCSIS * EMUX_THRESHOLD_RPT_OFF, EMUX_THR);
	fpif_write_reg(fpgrp->base + FP_EMUX_THRESHOLD +
		       DEVID_GIGE0 * EMUX_THRESHOLD_RPT_OFF, EMUX_THR);
	fpif_write_reg(fpgrp->base + FP_EMUX_THRESHOLD +
		       DEVID_GIGE1 * EMUX_THRESHOLD_RPT_OFF, EMUX_THR);
	if (fpgrp->plat->version != FP) {
		fpif_write_reg(fpgrp->base + FP_EMUX_THRESHOLD +
			       DEVID_RXDMA * EMUX_THRESHOLD_RPT_OFF, EMUX_THR);
		fpif_write_reg(fpgrp->base + FP_EMUX_THRESHOLD +
			       DEVID_RECIRC * EMUX_THRESHOLD_RPT_OFF, EMUX_THR);
	}
	fpif_write_reg(fpgrp->base + L2_CAM_CFG_COMMAND, L2CAM_CLEAR);

	if (fpgrp->plat->version != FPLITE) {
		fpif_write_reg(fpgrp->base + FPTXDMA_ENDIANNESS,
			       DMA_REV_ENDIAN);
		fpif_write_reg(fpgrp->base + FPTXDMA_T3W_CONFIG,
			       CONFIG_OP16 | CONFIG_OP32 | CONFIG_MOPEN);
		fpif_write_reg(fpgrp->base + FPTXDMA_BPAI_PRIORITY, BPAI_PRIO);
	} else {
		fpif_write_reg(fpgrp->base + FPTOE_ENDIANNESS, DMA_REV_ENDIAN);
		fpif_write_reg(fpgrp->base + FPTOE_T3W_CONFIG,
			       CONFIG_OP16 | CONFIG_OP32 | CONFIG_MOPEN);
		fpif_write_reg(fpgrp->base + FPTOE_MAX_NONSEG,
			       DMA_MAX_NONSEG_SIZE);
	}

	fpif_write_reg(fpgrp->base + FPRXDMA_T3R_CONFIG,
		       CONFIG_OP16 | CONFIG_OP32 | CONFIG_MOPEN);
	fpif_write_reg(fpgrp->base + FPRXDMA_ENDIANNESS, DMA_REV_ENDIAN);
	init_tcam(fpgrp->base);
	add_tcam_docsis(fpgrp->base);

	return;
}


static int fpif_deinit(struct fpif_grp *fpgrp)
{
	int j;
	struct fpif_priv *priv;

	unregister_netdevice_notifier(&ovs_dp_device_notifier);
	for (j = 0; j < NUM_INTFS; j++) {
		priv = netdev_priv(fpgrp->netdev[j]);
		if (NULL == priv)
			continue;
		if (priv->plat) {
			if (priv->plat->mdio_bus_data)
				fpif_mdio_unregister(priv->netdev);
		}
		if (priv->netdev->reg_state == NETREG_REGISTERED)
			unregister_netdev(priv->netdev);
		free_netdev(priv->netdev);
	}

	for (j = 0; j < fpgrp->l2cam_size; j++)
		fpgrp->l2_idx[j] = NUM_INTFS;

	reset_control_assert(fpgrp->rstc);

	clk_disable_unprepare(fpgrp->clk_fp);
	clk_put(fpgrp->clk_fp);
	clk_disable_unprepare(fpgrp->clk_ife);
	clk_put(fpgrp->clk_ife);

	return 0;
}


static int fpif_clean_tx_ring_hw(struct fpif_priv *priv)
{
	u32 tail_ptr, last_tx, done_tx_sw, idx;
	struct fpif_txdma *txdma_ptr = priv->txdma_ptr;
	int cpu;
	struct device *dev = priv->dev;

	/* We update  last_tx & done_tx_sw[cpu] */
	tail_ptr = readl(&txdma_ptr->tx_ch_reg->tx_done);
	last_tx = txdma_ptr->last_tx;
	while (last_tx != tail_ptr) {
		cpu = txdma_ptr->fp_txb[last_tx].cpu;
		idx = txdma_ptr->fp_txb[last_tx].idx;
		done_tx_sw = (idx + 1) & TX_RING_MOD_MASK;
		txdma_ptr->done_tx_sw[cpu] = done_tx_sw;
		last_tx = (last_tx + 1) & TX_RING_MOD_MASK;
		/* clear the tx interrupt */
		if (last_tx == tail_ptr)
			fpif_write_reg(&txdma_ptr->txbase->tx_irq_flags, 1);
		tail_ptr = readl(&txdma_ptr->tx_ch_reg->tx_done);
	}
	txdma_ptr->last_tx = last_tx;
	if (netif_msg_tx_done(priv))
		dev_dbg(dev, "last_tx=%d tail=%d\n", last_tx, tail_ptr);
	fpif_write_reg(&txdma_ptr->tx_ch_reg->tx_delay, DELAY_TX_INTRH);

	return 0;
}


static void wake_all_fpq(struct fpif_grp *fpgrp)
{
	int i;
	struct net_device *netdev;

	while (fpgrp->stop_q) {
		i = find_first_bit(&fpgrp->stop_q, NUM_INTFS);
		clear_bit(i, &fpgrp->stop_q);
		netdev = fp_promisc[i].netdev;
		/* Do I need to check interface up/down */
		if ((netdev->flags & IFF_UP) && (netif_queue_stopped(netdev)))
			netif_wake_queue(netdev);
	}
}


static int fpif_clean_tx_ring_sw(struct fpif_priv *priv, int cpu)
{
	int  last_tx_sw, done_tx_sw;
	struct net_device *netdev;
	struct fp_data *buf_ptr;
	int len, eop;
	dma_addr_t dma_addr;
	struct sk_buff *skb;
	struct fp_tx_ring *tx_ring_ptr;
	struct fpif_txdma *txdma_ptr = priv->txdma_ptr;
	int avail, cntr = 0, ret = 1;

	last_tx_sw = txdma_ptr->last_tx_sw[cpu];
	done_tx_sw = txdma_ptr->done_tx_sw[cpu];

	while (last_tx_sw != done_tx_sw) {
		tx_ring_ptr = &(txdma_ptr->fp_tx_skbuff[cpu][last_tx_sw]);
		skb = tx_ring_ptr->skb;
		dma_addr = tx_ring_ptr->dma_ptr;
		buf_ptr = tx_ring_ptr->skb_data;
		len = tx_ring_ptr->len_eop & 0xffff;
		eop = tx_ring_ptr->len_eop >> 16;
		priv = tx_ring_ptr->priv;
		netdev = priv->netdev;
		if (skb == NULL)
			dma_free_coherent(priv->dev, FP_HDR_SIZE,
					  buf_ptr, dma_addr);
		else
			dma_unmap_single(priv->dev, (dma_addr_t) dma_addr,
					 len, DMA_TO_DEVICE);

		if (eop)
			dev_kfree_skb_any(skb);

		tx_ring_ptr->skb = 0;
		tx_ring_ptr->skb_data = 0;
		last_tx_sw = (last_tx_sw + 1) & TX_RING_MOD_MASK;
		cntr++;
	}
	txdma_ptr->last_tx_sw[cpu] = last_tx_sw;
	txdma_ptr->avail[cpu] += cntr;
	avail = txdma_ptr->avail[cpu];

	if (avail > MAX_SKB_FRAGS)
		wake_all_fpq(priv->fpgrp);

	if (avail == FPIF_TX_RING_SIZE  - 1)
		ret = 0;

	return ret;
}



static int check_tx_busy(struct fpif_priv *priv, int nr_pkt)
{
	int remain;
	struct fpif_txdma *txdma_ptr = priv->txdma_ptr;
	u32 head_tx = txdma_ptr->head_tx;
	u32 last_tx = txdma_ptr->last_tx;

	if (head_tx >= last_tx)
		remain = FPIF_TX_RING_SIZE - (head_tx - last_tx) - 1;
	else
		remain = last_tx - head_tx - 1;
	if (nr_pkt > remain) {
		dev_info(priv->dev, "fastpath:busy remain=%d\n", remain);
		return NETDEV_TX_BUSY;
	}

	if (remain < DELAY_TX_THR)
		fpif_write_reg(&txdma_ptr->tx_ch_reg->tx_delay, DELAY_TX_INTRL);

	return NETDEV_TX_OK;
}


static inline int fpif_q_tx_buffer_hw(struct fpif_priv *priv)
{
	void __iomem *loptr, *hiptr;
	struct fpif_txdma *txdma_ptr = priv->txdma_ptr;
	struct tx_buf *bufptr = txdma_ptr->bufptr;
	dma_addr_t dma_ptr;
	int len_eop;
	int cpu = smp_processor_id();
	u32 head_sw;
	u32 start_sw;
	u32 head_tx;

	head_tx = txdma_ptr->head_tx;
	head_sw = txdma_ptr->head_tx_sw[cpu];
	start_sw = txdma_ptr->start_tx_sw[cpu];
	/* In this, head_tx and head_tx_sw[cpu] can be changed */
	while (start_sw != head_sw) {
		/* In this loop, start_sw and head_tx can be changed */
		if (check_tx_busy(priv, 1) == NETDEV_TX_BUSY)
			break;

		txdma_ptr->fp_txb[head_tx].cpu = cpu;
		txdma_ptr->fp_txb[head_tx].idx = start_sw;

		dma_ptr = txdma_ptr->fp_tx_skbuff[cpu][start_sw].dma_ptr;
		len_eop = txdma_ptr->fp_tx_skbuff[cpu][start_sw].len_eop;
		loptr = (&bufptr[head_tx].lo);
		hiptr = (&bufptr[head_tx].hi);
		fpif_write_reg(loptr, dma_ptr);
		fpif_write_reg(hiptr, len_eop);

		head_tx = (head_tx + 1) & TX_RING_MOD_MASK;
		fpif_write_reg(&txdma_ptr->tx_ch_reg->tx_cpu, head_tx);
		txdma_ptr->head_tx = head_tx;

		start_sw = (start_sw + 1) & TX_RING_MOD_MASK;
		txdma_ptr->start_tx_sw[cpu] = start_sw;
	}

	return NETDEV_TX_OK;
}


static inline void fpif_q_tx_buffer(struct fpif_priv *priv,
				   struct fp_tx_ring *tx_ring_ptr, int nr_frags)
{
	struct fpif_txdma *txdma_ptr = priv->txdma_ptr;
	int cpu = smp_processor_id();
	u32 head_sw;
	int avail;

	head_sw = txdma_ptr->head_tx_sw[cpu];
	txdma_ptr->fp_tx_skbuff[cpu][head_sw] = *tx_ring_ptr;
	head_sw = (head_sw + 1) & TX_RING_MOD_MASK;
	txdma_ptr->head_tx_sw[cpu] = head_sw;
	txdma_ptr->avail[cpu]--;
	avail = txdma_ptr->avail[cpu];
	if (avail <= nr_frags + 1) {
		set_bit(priv->id, &priv->fpgrp->stop_q);
		netif_stop_queue(priv->netdev);
	}
}

static inline void fpif_fill_fphdr(struct fp_hdr *fphdr, struct sk_buff *skb,
			int skblen, struct fpif_priv *priv, int tso)
{
	u16 temp;
	u16 len = skblen;
	u32 dmap = priv->dmap;
	u32 len_mask = 0;
	u32 ifidx = priv->ifidx;
	int ip_len = ip_hdrlen(skb);
	int l3offset = 0, l4offset = 0, proto = 0, mss = 0;

	if (tso) {
		skb->transport_header = skb->network_header + ip_len;
		l3offset = (skb->network_header - skb->data - FP_HDR_SIZE + 2)
				>> 2;
		l4offset = (skb->transport_header - skb->data - FP_HDR_SIZE +
				2) >> 2;
		proto = FPHDR_PROT_TCP; /* TCP */
		mss = FPHDR_TCP_MSS_WORD; /* TCP mss in word size multiple */
	}

	fphdr->word0 = ntohl((ifidx << FPHDR_IFIDX_SHIFT) |
			((SP_SWDEF << FPHDR_SP_SHIFT) & FPHDR_SP_MASK) |
			FPHDR_CSUM_MASK | FPHDR_BRIDGE_MASK |
			(dmap & FPHDR_DEST_MASK));
	fphdr->word1 = ntohl((mss << FPHDR_MSS_SHIFT) |
			FPHDR_MANGLELIST_MASK | FPHDR_DONE_MASK);
	fphdr->word2 = ntohl((proto << FPHDR_PROTO_SHIFT) | (l3offset <<
			FPHDR_L3_SHIFT) | (l4offset << FPHDR_L4_SHIFT) |
			FPHDR_NEXTHOP_IDX_MASK | FPHDR_SMAC_IDX_MASK);

	/**
	 * Here we want to keep 2 bytes after fastpath header intact so
	 * we store this in temp variable and write in word3 with len
	 */
	temp = *(u16 *)((u8 *)fphdr + FP_HDR_SIZE);

	/* This is required for hw tso workaround */
	if (tso)
		len_mask = FPHDR_TSO_LEN_MASK;

	fphdr->word3 = ntohl(len_mask | (len << FPHDR_LEN_SHIFT) |
			htons(temp));
}


static int put_l2cam(struct fpif_priv *priv, u8 dev_addr[], int *idx)
{
	u32 val;
	u32 dp = priv->dma_port;
	u32 sp = priv->sp;
	int cam_sts;
	struct fpif_grp *fpgrp = priv->fpgrp;
	struct device *dev = priv->dev;

	if (!fpgrp->available_l2cam) {
		dev_err(dev, "ERROR : No available L2CAM entries\n");
		return -EIO;
	}

	if (netif_msg_ifup(priv))
		dev_dbg(dev, "put_l2cam(%d) %x-%x-%x-%x-%x-%x\n", priv->id,
			dev_addr[0], dev_addr[1], dev_addr[2], dev_addr[3],
			dev_addr[4], dev_addr[5]);

	fpif_write_reg(fpgrp->base + L2_CAM_CFG_MODE, HW_MANAGED);

	val = (dev_addr[2] << 24) | (dev_addr[3] << 16) |
		       (dev_addr[4] << 8) | dev_addr[5];
	fpif_write_reg(priv->fpgrp->base + L2_CAM_MAC_DA_LOW, val);

	val = (BIT(L2CAM_BRIDGE_SHIFT)) | (dp << L2CAM_DP_SHIFT) |
		(sp << L2CAM_SP_SHIFT) | (dev_addr[0] << 8) | dev_addr[1];
	fpif_write_reg(priv->fpgrp->base + L2_CAM_MAC_DA_HIGH, val);

	fpif_write_reg(priv->fpgrp->base + L2_CAM_CFG_COMMAND, L2CAM_ADD);

	cam_sts = readl(priv->fpgrp->base + L2_CAM_CFG_STATUS);
	*idx = (cam_sts >> L2CAM_IDX_SHIFT) & L2CAM_IDX_MASK;
	if (netif_msg_ifup(priv))
		dev_dbg(dev, "idx=%d cam_sts=%x\n", *idx, cam_sts);
	cam_sts = (cam_sts >> L2CAM_STS_SHIFT) & L2CAM_STS_MASK;
	if (cam_sts) {
		if (cam_sts & ~L2CAM_COLL_MASK) {
			dev_err(dev, "ERR:add entry L2CAM 0x%x\n", cam_sts);
			return -EIO;
		} else {
		/* Return in case of Duplicate Entry */
			return 0;
		}
	}

	fpgrp->l2_idx[*idx] = priv->id;
	fpgrp->available_l2cam--;

	return 0;
}


static void remove_l2cam(struct fpif_priv *priv, int idx)
{
	u32 val;
	u32 status;
	struct fpif_grp *fpgrp = priv->fpgrp;
	struct device *dev = priv->dev;

	if ((idx < 0) || (idx >= fpgrp->l2cam_size))
		dev_err(dev, "fp:ERR:Inv idx %d pass\n", idx);

	fpif_write_reg(fpgrp->base + L2_CAM_CFG_MODE, SW_MANAGED);
	val = (idx << 8) | L2CAM_READ;
	fpif_write_reg(fpgrp->base + L2_CAM_CFG_COMMAND, val);
	val = (idx << 8) | L2CAM_DEL;
	fpif_write_reg(fpgrp->base + L2_CAM_CFG_COMMAND, val);
	status = readl(fpgrp->base + L2_CAM_CFG_STATUS);
	fpgrp->available_l2cam++;
	if (netif_msg_ifup(priv))
		dev_dbg(dev, "(%d) %d sts=%x\n", priv->id, idx, status);
}


static int remove_l2cam_if(struct fpif_priv *priv)
{
	struct fpif_grp *fpgrp = priv->fpgrp;
	int i, cnt = fpgrp->l2cam_size;

	for (i = 0; i < cnt; i++) {
		if (fpgrp->l2_idx[i] != priv->id)
			continue;
		remove_l2cam(priv, i);
		fpgrp->l2_idx[i] = NUM_INTFS;
	}
	priv->ifaddr_idx = IDX_INV;
	priv->br_l2cam_idx = IDX_INV;

	return 0;
}


static int fpif_change_mtu(struct net_device *dev, int new_mtu)
{
	struct fpif_priv *priv = netdev_priv(dev);

	if (!priv->rgmii_base)
		return -EPERM;

	if (netif_running(dev)) {
		netdev_err(dev, "must be stopped for MTU change\n");
		return -EBUSY;
	}

	if ((new_mtu < MIN_ETH_FRAME_SIZE) || (new_mtu > JUMBO_FRAME_SIZE)) {
		netdev_err(dev, "Invalid MTU setting\n");
		return -EINVAL;
	}

	/**
	 * Only stop and start the controller if it isn't already
	 * stopped, and we changed something
	 */
	priv->rx_buffer_size = new_mtu + dev->hard_header_len +
						VLAN_HLEN * 2;
	dev->mtu = new_mtu;

	fpif_write_reg(priv->rgmii_base + RGMII_GLOBAL_MACINFO3, new_mtu +
			ETH_HLEN + VLAN_HLEN * 2 + ETH_FCS_LEN);

	fpif_write_reg(priv->fpgrp->base + FP_PORTSETTINGS_HI +
		       priv->sp * PORT_SETTINGS_RPT_OFF,
		       new_mtu + ETH_HLEN + VLAN_HLEN * 2);

	return 0;
}


static void remove_tcam_promisc_fp(struct fpif_priv *priv)
{
	int idx;

	idx = TCAM_PROMS_FP_IDX + priv->id;
	remove_tcam(priv->fpgrp->base, idx);

	return;
}

static void add_tcam_promisc_fp(struct fpif_priv *priv)
{
	struct net_device *netdev = priv->netdev;
	int idx;
	struct fp_tcam_info tcam_info;

	memset(&tcam_info, 0, sizeof(tcam_info));
	tcam_info.sp = priv->sp;
	tcam_info.dev_addr_d = netdev->dev_addr;
	idx = TCAM_PROMS_FP_IDX + priv->id;
	mod_tcam(priv->fpgrp->base, &tcam_info, idx);

	return;
}


static void add_tcam_allmulti(struct fpif_priv *priv)
{
	struct fp_tcam_info tcam_info;
	int idx;

	if (priv->logical_if)
		return;

	memset(&tcam_info, 0, sizeof(tcam_info));
	tcam_info.sp = priv->sp;
	tcam_info.bridge = 1;
	tcam_info.cont = 1;
	tcam_info.redir = 1;
	tcam_info.dest = priv->dma_port;
	tcam_info.all_multi = 1;
	idx = TCAM_ALLMULTI_IDX + priv->id;
	add_tcam(priv->fpgrp->base, &tcam_info, idx);
	priv->allmulti_idx = idx;

	return;
}


static void remove_tcam_br(struct fpif_priv *priv)
{
	int idx;
	struct fpif_grp *fpgrp = priv->fpgrp;

	if (priv->logical_if)
		return;

	if (priv->br_tcam_idx == IDX_INV)
		return;

	idx = TCAM_PROMS_FP_IDX + priv->id;
	remove_tcam(fpgrp->base, idx);
	priv->br_tcam_idx = IDX_INV;

	if (priv->promisc_idx != TCAM_IDX_INV) {
		add_tcam_promisc_fp(priv);
		return;
	}

	if (priv->allmulti_idx != TCAM_IDX_INV)
		add_tcam_allmulti(priv);

	return;
}


static void add_tcam_br(struct fpif_priv *priv, struct net_device *netdev)
{
	struct fp_tcam_info tcam_info;
	struct fpif_grp *fpgrp = priv->fpgrp;
	int idx;

	if (priv->logical_if)
		return;

	remove_tcam_promisc_fp(priv);

	memset(&tcam_info, 0, sizeof(tcam_info));
	tcam_info.dev_addr_d = netdev->dev_addr;
	tcam_info.sp = priv->sp;
	idx = TCAM_PROMS_FP_IDX + priv->id;
	mod_tcam(fpgrp->base, &tcam_info, idx);
	priv->br_tcam_idx = idx;

	return;
}


static void remove_tcam_promisc(struct fpif_priv *priv)
{
	struct fpif_grp *fpgrp = priv->fpgrp;

	if (priv->logical_if)
		return;

	if (priv->promisc_idx != TCAM_IDX_INV) {
		remove_tcam(fpgrp->base, TCAM_PROMS_SP_IDX + priv->id);
		remove_tcam(fpgrp->base, TCAM_PROMS_FP_IDX + priv->id);
		priv->promisc_idx = TCAM_IDX_INV;
	}
}


static void remove_tcam_allmulti(struct fpif_priv *priv)
{
	struct fpif_grp *fpgrp = priv->fpgrp;

	if (priv->logical_if)
		return;

	if (priv->allmulti_idx != TCAM_IDX_INV) {
		remove_tcam(fpgrp->base, TCAM_ALLMULTI_IDX + priv->id);
		priv->allmulti_idx = TCAM_IDX_INV;
	}
}


static void add_tcam_promisc(struct fpif_priv *priv)
{
	int idx;
	struct fp_tcam_info tcam_info;

	if (priv->logical_if)
		return;

	if (priv->promisc_idx == TCAM_IDX_INV) {
		add_tcam_promisc_fp(priv);

		memset(&tcam_info, 0, sizeof(tcam_info));
		tcam_info.sp = priv->sp;
		tcam_info.bridge = 1;
		tcam_info.cont = 1;
		tcam_info.redir = 1;
		tcam_info.dest = priv->dma_port;
		idx = TCAM_PROMS_SP_IDX + priv->id;
		add_tcam(priv->fpgrp->base, &tcam_info, idx);
		priv->promisc_idx = idx;
	}

	return;
}

static void add_in_cams(struct  net_device *br, struct net_device *port)
{
	struct fpif_priv *priv = netdev_priv(port);
	int err, idx;
	struct device *dev = priv->dev;

	if (priv->br_l2cam_idx != IDX_INV)
		return;

	if (!(br->flags & IFF_UP))
		return;

	fp_promisc[priv->id].ifidx_log = br->ifindex;
	err = put_l2cam(priv, br->dev_addr, &idx);
	if (err) {
		dev_err(dev, "fp:ERROR in putting br mac in l2cam\n");
		priv->br_l2cam_idx = IDX_INV;
	} else {
		priv->br_l2cam_idx = idx;
	}
	add_tcam_br(priv, br);

	return;
}


static void del_from_cams(struct  net_device *br, struct net_device *port)
{
	struct fpif_priv *priv = netdev_priv(port);

	if ((priv->br_l2cam_idx != priv->ifaddr_idx) &&
	    (priv->br_l2cam_idx != IDX_INV) &&
	    (port->flags & IFF_UP))
		remove_l2cam(priv, priv->br_l2cam_idx);

	priv->br_l2cam_idx = IDX_INV;
	remove_tcam_br(priv);
	fp_promisc[priv->id].ifidx_log = IDX_INV;

	return;
}


static void add_fpbr(struct net_device *brdev, struct net_device *port_netdev)
{
	int i;
	u8 *mac = brdev->dev_addr;

	for (i = 0; i < NUM_PHY_INTFS; i++) {
		if (fp_promisc[i].netdev == port_netdev) {
			memcpy(fp_promisc[i].mac, mac, ETH_ALEN);
			add_in_cams(brdev, port_netdev);
		}
	}

	return;
}

static void upd_fpbr_table(struct net_device *brdev)
{
	int i;
	struct net_device *port_netdev;
	u8 *mac = brdev->dev_addr;

	for (i = 0; i < NUM_PHY_INTFS; i++) {
		if (fp_promisc[i].ifidx_log != brdev->ifindex)
			continue;
		memcpy(fp_promisc[i].mac, mac, ETH_ALEN);
		port_netdev = fp_promisc[i].netdev;
		del_from_cams(brdev, port_netdev);
		add_in_cams(brdev, port_netdev);
	}

	return;
}

static void del_fpbr_table(struct net_device *brdev)
{
	int i;
	struct net_device *port_netdev;

	for (i = 0; i < NUM_PHY_INTFS; i++) {
		if (fp_promisc[i].ifidx_log == brdev->ifindex) {
			port_netdev = fp_promisc[i].netdev;
			del_from_cams(brdev, port_netdev);
		}
	}

	return;
}


void fp_add_tcam_promisc(const void *netdev_log, const void *netdev_phy,
		int fp_idx)
{
	struct net_device *port_netdev = (struct net_device *)netdev_phy;
	struct net_device *netdev = (struct net_device *)netdev_log;
	u8 *mac = netdev->dev_addr;
	int ifidx_log = netdev->ifindex;

	if (fp_promisc[fp_idx].ifidx_log == ifidx_log) {
		if (!compare_ether_addr(mac, fp_promisc[fp_idx].mac))
			return;
		pr_err("mrt:mac addr passed by FP API not same\n");
	}

	if (fp_promisc[fp_idx].netdev == port_netdev)
		add_fpbr(netdev, port_netdev);
	else
		pr_err("Invalid fpidx %d passed\n", fp_idx);

	return;
}
EXPORT_SYMBOL(fp_add_tcam_promisc);


static int dp_device_event(struct notifier_block *unused, unsigned long event,
			   void *ptr)
{
	struct net_device *netdev = ptr;
	struct net_bridge *br = netdev_priv(netdev);
	struct net_bridge_port *p;
	int linux_bridge;

	linux_bridge = netdev->priv_flags & IFF_EBRIDGE;
	switch (event) {
	case NETDEV_CHANGE:
		del_fpbr_table(netdev);
		if ((netdev->flags & IFF_UP) && (linux_bridge)) {
			list_for_each_entry(p, &br->port_list, list) {
				if (!is_fpport(p->dev))
					continue;
				if (p->dev->flags & IFF_UP)
					add_fpbr(netdev, p->dev);
			}
		}
		break;

	case NETDEV_DOWN:
		del_fpbr_table(netdev);
		break;

	case NETDEV_CHANGEADDR:
		upd_fpbr_table(netdev);
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block ovs_dp_device_notifier = {
	.notifier_call = dp_device_event
};


static void fpif_set_multi(struct net_device *dev)
{
	struct netdev_hw_addr *ha;
	struct fpif_priv *priv = netdev_priv(dev);
	struct fpif_grp *fpgrp = priv->fpgrp;
	int idx;

	if (priv->logical_if)
		return;

	/* promisc and allmulti idx share the same location in TCAM */
	if (dev->flags & IFF_PROMISC) {
		add_tcam_promisc(priv);
		priv->allmulti_idx = TCAM_IDX_INV;
		return;
	}
	remove_tcam_promisc(priv);
	if (dev->flags & IFF_ALLMULTI) {
		if (priv->allmulti_idx == TCAM_IDX_INV)
			add_tcam_allmulti(priv);
		return;
	}
	remove_tcam_allmulti(priv);
	if (netdev_mc_empty(dev))
		return;
	if (netdev_mc_count(dev) > fpgrp->available_l2cam) {
		netdev_err(dev, "netdev_mc_count (%d) > l2cam size\n",
			   netdev_mc_count(dev));
		if (priv->allmulti_idx == TCAM_IDX_INV)
			add_tcam_allmulti(priv);
		return;
	}
	netdev_for_each_mc_addr(ha, dev)
		put_l2cam(priv, ha->addr, &idx);
}

/**
 * fpif_process_frame() -- handle one incoming packet
 * @priv: private structure pointer
 * @skb: socket buffer pointer
 * Description: this is the main function to process the incoming frames
 */
static inline int fpif_process_frame(struct fpif_priv *priv,
				     struct sk_buff *skb)
{
	unsigned int pkt_len, src_if, dmap, dev_id;
	struct net_device *netdev;
	u32 badf_flag;
	struct fp_data *buf_ptr;
	struct fpif_grp *fpgrp = priv->fpgrp;
	struct net_device *ndev_cpu;
	struct net_device *ndev = priv->netdev;
	int ret;
	int smac_idx, wlan_dest = 0;
	struct fpif_priv *xpriv;
	struct device *dev = priv->dev;

	buf_ptr = (void *)skb->data;
	pkt_len = ntohl((buf_ptr->hdr.word3));
	pkt_len = pkt_len >> FPHDR_LEN_SHIFT;
	badf_flag = (buf_ptr->hdr.word2) & 0x4000;

	smac_idx = ntohl((buf_ptr->hdr.word2));
	smac_idx = (smac_idx & FPHDR_SMAC_IDX_MASK) >>
			FPHDR_SMAC_IDX_SHIFT;
	src_if = ntohl((buf_ptr->hdr.word0));
	dmap = (src_if & FPHDR_DEST_MASK) >> FPHDR_DEST_SHIFT;
	src_if = (src_if & FPHDR_SP_MASK) >> FPHDR_SP_SHIFT;
	dev_id = src_if;
	netdev = fpgrp->netdev[dev_id];
	xpriv = netdev_priv(netdev);

	if (pkt_len > priv->rx_buffer_size) {
		dev_err(dev, "invalid pkt_len %d %p\n", pkt_len, buf_ptr);
		dev_err(dev, "FPhdr=%x %x %x %x\n", buf_ptr->hdr.word0,
			buf_ptr->hdr.word1, buf_ptr->hdr.word2,
			buf_ptr->hdr.word3);
		return 1;
	}

	skb->dev = netdev;
	skb_reserve(skb, FP_HDR_SIZE);
	skb_put(skb, pkt_len);
	netdev->last_rx = jiffies;

	ndev->stats.rx_packets++;
	ndev->stats.rx_bytes += skb->len;
	if (unlikely(badf_flag))
		dev_info(dev, "fastpath:Bad frame received\n");
	else
		skb->ip_summed = CHECKSUM_UNNECESSARY;

	if (smac_idx == MNGL_IDX_WLAN0) {
		wlan_dest = 1;
		if (!fpgrp->ndev_cpu[0])
			fpgrp->ndev_cpu[0] =
				dev_get_by_name(dev_net(ndev),
						fpgrp->wlan0);
		fpgrp->wlan0_cnt++;
		ndev_cpu = fpgrp->ndev_cpu[0];
	}

	if (smac_idx == MNGL_IDX_WLAN1) {
		wlan_dest = 1;
		if (!fpgrp->ndev_cpu[1])
			fpgrp->ndev_cpu[1] =
				dev_get_by_name(dev_net(ndev),
						fpgrp->wlan1);
		ndev_cpu = fpgrp->ndev_cpu[1];
		fpgrp->wlan1_cnt++;
	}

	if (!wlan_dest) {
		/* Tell the skb what kind of packet this is */
		skb->protocol = eth_type_trans(skb, netdev);
		/* Send the packet up the stack */
		if (netif_msg_pktdata(priv))
			dev_dbg(dev, "prot=0x%x len=%d dtlen=%d dma=%d\n",
				htons(skb->protocol), skb->len,
				skb->data_len, xpriv->rx_dma_ch);
		netif_receive_skb(skb);
	} else {
		if (netif_msg_pktdata(priv))
			dev_dbg(dev, "dmap=%d smac=%d cpu=%p\n", dmap,
				smac_idx, ndev_cpu);
		if (!ndev_cpu) {
			dev_err(dev, "No ref of wlan device\n");
			return -ENODEV;
		}
		if (!ndev_cpu->netdev_ops->ndo_start_xmit) {
			dev_err(dev, "No ref of xmit func of wlan dev\n");
			return -ENXIO;
		}
		skb->dev = ndev_cpu;
		ret = ndev_cpu->netdev_ops->ndo_start_xmit(skb, ndev_cpu);
		/* TBD: Need to handle this in a better way by keeping buffers*/
		if (ret != NETDEV_TX_OK)
			dev_err(dev, "TXQ of wifi is full\n");
	}
	return 0;
}


/**
 * fpif_clean_rx_ring() -- Processes each frame in the rx ring
 * until the budget/quota has been reached. Returns the number
 * of frames handled
 * @priv: private structure pointer
 * @limit: budget limit from poll method
 * Description: directly invoked by NAPI method it is to collect the frames
 * and clean the ring.
 */
static inline int fpif_clean_rx_ring(struct fpif_priv *priv, int limit)
{
	struct sk_buff *skb;
	u32 tail_ptr, last_rx;
	int cntr = 0;
	dma_addr_t dma_addr;
	struct fpif_rxdma *rxdma_ptr = priv->rxdma_ptr;
	struct device *dev = priv->dev;
	int dma_ch = priv->rx_dma_ch;
	int ret;

	tail_ptr = readl(&rxdma_ptr->rx_ch_reg->rx_done);
	last_rx = rxdma_ptr->last_rx;
	fpif_write_reg(&rxdma_ptr->rxbase->rx_irq_flags, BIT(dma_ch));

	while (last_rx != tail_ptr && limit--) {
		dma_addr = rxdma_ptr->fp_rx_skbuff[last_rx].dma_ptr;
		skb = rxdma_ptr->fp_rx_skbuff[last_rx].skb;
		rxdma_ptr->fp_rx_skbuff[last_rx].skb = 0;
		rxdma_ptr->fp_rx_skbuff[last_rx].dma_ptr = 0;
		cntr++;
		dma_unmap_single(priv->dev, (dma_addr_t)dma_addr,
				 priv->rx_buffer_size, DMA_FROM_DEVICE);
		ret = fpif_process_frame(priv, skb);
		if (ret)
			dev_kfree_skb(skb);

		skb = fpif_poll_start_skb(priv, GFP_ATOMIC);
		if (NULL == skb) {
			netdev_err(priv->netdev, "skb is not free\n");
			break;
		}

		dma_addr = dma_map_single(priv->dev, skb->data,
				   priv->rx_buffer_size, DMA_FROM_DEVICE);
		if (dma_mapping_error(priv->dev, dma_addr))
			dev_err(dev, "Err in dma map addr=%x\n", dma_addr);
		else
			fpif_q_rx_buffer(rxdma_ptr, skb, dma_addr);

		last_rx = (last_rx + 1) & RX_RING_MOD_MASK;
		/* clear the rx interrupt */
		if (last_rx == tail_ptr)
			fpif_write_reg(&rxdma_ptr->rxbase->rx_irq_flags,
				       BIT(dma_ch));
		tail_ptr = readl(&rxdma_ptr->rx_ch_reg->rx_done);
	}

	rxdma_ptr->last_rx = last_rx;

	return cntr;
}


/**
 * fpif_clean_tx_ring() -- Processes each frame in the tx ring
 *   until the work limit has been reached. Returns the number
 *   of frames handled
 * @priv: private structure pointer
 * Description: directly invoked by NAPI method it is to clean the TX ring
 */
static int fpif_clean_tx_ring(struct fpif_priv *priv)
{
	struct fpif_txdma *txdma_ptr = priv->txdma_ptr;
	int pending;
	int cpu = smp_processor_id();
	int other_cpu = cpu ^ 1;

	spin_lock(&txdma_ptr->fpif_txlock);
	fpif_clean_tx_ring_hw(priv);
	spin_unlock(&txdma_ptr->fpif_txlock);

	spin_lock(&txdma_ptr->txlock[cpu]);
	pending = fpif_clean_tx_ring_sw(priv, cpu);
	spin_unlock(&txdma_ptr->txlock[cpu]);

	if (txdma_ptr->avail[other_cpu] < FPIF_TX_RING_SIZE - 1) {
		if (spin_trylock(&txdma_ptr->txlock[other_cpu])) {
			fpif_clean_tx_ring_sw(priv, other_cpu);
			spin_unlock(&txdma_ptr->txlock[other_cpu]);
		}
	}

	return pending;
}


static int check_napi_sched(struct fpif_grp *fpgrp)
{
	int cpu = smp_processor_id();
	struct fpif_priv *priv;

	fpif_write_reg(&fpgrp->txbase->tx_irq_enables[0], 0);
	fpif_write_reg(&fpgrp->rxbase->rx_irq_enables[cpu], 0);
	fpif_write_reg(&fpgrp->rxbase->rx_irq_flags, BIT(cpu));
	fpif_write_reg(&fpgrp->txbase->tx_irq_flags, 1);
	priv = netdev_priv(fpgrp->netdev[DEVID_FP0 + cpu]);

	if (napi_schedule_prep(&priv->napi))
		__napi_schedule(&priv->napi);

	return 0;
}


static int fpif_poll(struct napi_struct *napi, int budget)
{
	struct fpif_priv *priv = container_of(napi,
			struct fpif_priv, napi);
	struct fp_rxdma_regs *rxbase = priv->rxdma_ptr->rxbase;
	struct fp_txdma_regs *txbase = priv->txdma_ptr->txbase;
	int pending, idx, howmany_rx;

	howmany_rx = fpif_clean_rx_ring(priv, budget);
	pending = fpif_clean_tx_ring(priv);

	if (howmany_rx < budget) {
		napi_complete(&priv->napi);
		idx = priv->id - DEVID_FP0;
		fpif_write_reg(&rxbase->rx_irq_enables[idx], BIT(idx));
		fpif_write_reg(&txbase->tx_irq_enables[0], 1);
	}

	return howmany_rx;
}



/**
 * fpif_intr - Interrupt Handler
 * @irq: interrupt number
 * @data: pointer to a network platform interface device structure
 **/
static irqreturn_t fpif_intr(int irq, void *data)
{
	struct fpif_grp *fpgrp = data;

	if (fpgrp->plat->preirq)
		fpgrp->plat->preirq(fpgrp);

	check_napi_sched(fpgrp);

	if (fpgrp->plat->postirq)
		fpgrp->plat->postirq(fpgrp);

	return IRQ_HANDLED;
}


static int fpif_xmit_frame_sg(struct fpif_priv *priv, struct sk_buff *skb)
{
	struct device *dev = priv->dev;
	u32 nr_frags = skb_shinfo(skb)->nr_frags;
	struct skb_frag_struct *frag;
	struct fp_tx_ring tx_ring;
	u32 len;
	void *offset;
	dma_addr_t dma_addr;
	int f, eop;

	for (f = 0; f < nr_frags; f++) {
		if (f == (nr_frags - 1))
			eop = 1;
		else
			eop = 0;

		frag = &skb_shinfo(skb)->frags[f];
		if (!frag) {
			dev_kfree_skb(skb);
			return -EINVAL;
		}
		len = frag->size;
		offset = &frag;

		dma_addr = skb_frag_dma_map(dev, frag, 0, len,
					    DMA_TO_DEVICE);
		if (dma_mapping_error(dev, dma_addr)) {
			dev_err(dev, "Err dma frag addr=%x\n", dma_addr);
			return -EINVAL;
		}

		tx_ring.skb = skb;
		tx_ring.skb_data = offset;
		tx_ring.dma_ptr = dma_addr;
		tx_ring.len_eop = eop << 16 | len;
		tx_ring.priv = priv;
		fpif_q_tx_buffer(priv, &tx_ring, nr_frags);
	}

	return 0;
}


static int tx_remain(struct fpif_priv *priv)
{
	int remain;
	struct fpif_txdma *txdma_ptr = priv->txdma_ptr;
	int cpu = smp_processor_id();

	remain = txdma_ptr->avail[cpu];

	return remain;
}


/**
 * fpif_xmit_frame: transmit function
 * @skb: socket buffer pointer
 * @netdev: net_device pointer
 * Description: this is called by the kernel when a frame is ready for xfer.
 * It is pointed to by the dev->hard_start_xmit function pointer
 * the frames.
 */
static int fpif_xmit_frame(struct sk_buff *skb, struct net_device *netdev)
{
	struct fpif_priv *priv = netdev_priv(netdev);
	struct fp_tx_ring tx_ring;
	struct fp_hdr *fphdr;
	struct device *dev = priv->dev;
	int remain, eop, ret = NETDEV_TX_OK;
	dma_addr_t dma_addr, dma_fphdr;
	u32 data_len, nr_frags;
	struct fpif_txdma *txdma_ptr = priv->txdma_ptr;
	int cpu = smp_processor_id();
	int tso = 0;
	int l3_prot;

	if (netif_msg_pktdata(priv))
		dev_dbg(dev, "TX:len=%d data_len=%d prot=%x id=%d ch=%d\n",
			skb->len, skb->data_len, htons(skb->protocol),
			priv->id, priv->tx_dma_ch);

	if (priv->logical_if) {
		dev_warn(dev, "wlan upstream flow hit, type=%d len=%d\n",
			 skb->pkt_type, skb->len);
		netdev->stats.tx_packets++;
		netdev->stats.tx_bytes += skb->len;
		dev_kfree_skb_any(skb);
		return ret;
	}

	if ((skb->skb_iif == 0) && (skb->network_header)) {
		l3_prot = ((struct iphdr *)(skb->network_header))->protocol;
		if ((l3_prot == IPPROTO_TCP) && (priv->plat->tso_enabled))
			tso = 1;
	}

	spin_lock(&txdma_ptr->txlock[cpu]);
	nr_frags = skb_shinfo(skb)->nr_frags;

	remain = tx_remain(priv);
	if (remain < (nr_frags + 1)) {
		spin_unlock(&txdma_ptr->txlock[cpu]);
		dev_info(dev, "fastpath:stop Q\n");
		netif_stop_queue(netdev);
		return NETDEV_TX_BUSY;
	}

	if (unlikely((skb->data - FP_HDR_SIZE) < skb->head)) {
		fphdr = dma_alloc_coherent(dev, FP_HDR_SIZE,
				&dma_fphdr, GFP_ATOMIC);
		if (fphdr == NULL) {
			spin_unlock(&txdma_ptr->txlock[cpu]);
			netdev_err(netdev, "dma_alloc_coherent failed for fphdr\n");
			return NETDEV_TX_BUSY;
		}
		fpif_fill_fphdr(fphdr, skb, skb->len, priv, 0);
		tx_ring.skb = NULL;
		tx_ring.skb_data = fphdr;
		tx_ring.dma_ptr = dma_fphdr;
		tx_ring.len_eop = FP_HDR_SIZE;
		tx_ring.priv = priv;
		netdev->trans_start = jiffies;
		fpif_q_tx_buffer(priv, &tx_ring, nr_frags);
	} else {
		fphdr = (struct fp_hdr *)skb_push(skb, FP_HDR_SIZE);
		fpif_fill_fphdr(fphdr, skb, skb->len - FP_HDR_SIZE, priv, tso);
	}

	if (nr_frags == 0) {
		data_len = skb->len;
		eop = 1;
	} else {
		data_len = skb->len - skb->data_len;
		eop = 0;
	}

	dma_addr = dma_map_single(dev, skb->data, data_len, DMA_TO_DEVICE);

	if (dma_mapping_error(dev, dma_addr)) {
		spin_unlock(&txdma_ptr->txlock[cpu]);
		dev_err(dev, "Err dma mapping addr=%x\n", dma_addr);
		return NETDEV_TX_BUSY;
	}

	tx_ring.skb = skb;
	tx_ring.skb_data = skb->data;
	tx_ring.dma_ptr = dma_addr;
	tx_ring.len_eop = eop << 16 | data_len;
	tx_ring.priv = priv;
	netdev->trans_start = jiffies;
	fpif_q_tx_buffer(priv, &tx_ring, 0);

	if (nr_frags)
		ret = fpif_xmit_frame_sg(priv, skb);
	if (spin_trylock(&txdma_ptr->fpif_txlock)) {
		fpif_q_tx_buffer_hw(priv);
		if (remain < FP_TX_FREE_BUDGET) {
			fpif_clean_tx_ring_hw(priv);
			fpif_clean_tx_ring_sw(priv, cpu);
		}
		spin_unlock(&txdma_ptr->fpif_txlock);
	}
	spin_unlock(&txdma_ptr->txlock[cpu]);

	return ret;
}



/**
 * fpif_adjust_link
 * @dev: net device structure
 * Description: it adjusts the link parameters.
 */
static void fpif_adjust_link(struct net_device *dev)
{
	struct fpif_priv *priv = netdev_priv(dev);
	struct phy_device *phydev = priv->phydev;
	unsigned long flags;
	int new_state = 0;
	u32 mac_info;
	void *base = priv->rgmii_base;

	if (unlikely(phydev == NULL))
		return;

	if (unlikely(base == NULL))
		return;

	phydev->irq = PHY_IGNORE_INTERRUPT;

	if ((phydev->speed != 10) && (phydev->speed != 100) &&
	    (phydev->speed != 1000)) {
		dev_warn(priv->dev, "%s:Speed(%d) is not 10/100/1000",
			 dev->name, phydev->speed);
		return;
	}

	spin_lock_irqsave(&priv->fpif_lock, flags);
	if (phydev->link) {
		mac_info = readl(base + RGMII_MACINFO0);

		if (phydev->duplex != priv->oldduplex) {
			new_state = 1;
			mac_info &= ~(MACINFO_DUPLEX);
			if (!(phydev->duplex))
				mac_info |= (MACINFO_HALF_DUPLEX);
			else
				mac_info |= (MACINFO_FULL_DUPLEX);

			priv->oldduplex = phydev->duplex;
		}

		if (phydev->speed != priv->speed) {
			new_state = 1;
			mac_info &= ~(MACINFO_SPEED);
			switch (phydev->speed) {
			case 1000:
				mac_info |= (MACINFO_SPEED_1000);
				break;
			case 100:
				mac_info |= (MACINFO_SPEED_100);
				break;
			case 10:
				mac_info |= (MACINFO_SPEED_10);
				break;
			default:
				dev_warn(priv->dev,
					 "%s:Speed(%d)not 10/100/1000",
					 dev->name, phydev->speed);
				break;
			}

			priv->speed = phydev->speed;
		}

		if (new_state) {
			if (netif_msg_link(priv))
				dev_dbg(priv->dev,
					"Writing %x for speed %d duplex %d\n",
					 mac_info, phydev->speed,
					 phydev->duplex);
			fpif_write_reg(base + RGMII_MACINFO0, mac_info);
		}

		if (!priv->oldlink) {
			new_state = 1;
			priv->oldlink = 1;
		}
	} else if (priv->oldlink) {
		new_state = 1;
		priv->oldlink = 0;
		priv->speed = 0;
		priv->oldduplex = -1;
	}

	if (new_state && netif_msg_link(priv))
		phy_print_status(phydev);

	spin_unlock_irqrestore(&priv->fpif_lock, flags);
}

/**
 * fpif_init_phy - PHY initialization
 * @dev: net device structure
 * Description: it initializes the driver's PHY state, and attaches the PHY
 * to the mac driver.
 *  Return value:
 *  0 on success
 */
static int fpif_init_phy(struct net_device *dev)
{
	struct fpif_priv *priv = netdev_priv(dev);
	struct phy_device *phydev;
	char phy_id[MII_BUS_ID_SIZE + 3];
	char bus_id[MII_BUS_ID_SIZE];
	int interface = priv->plat->interface;

	if (priv->plat->phy_bus_name)
		snprintf(bus_id, MII_BUS_ID_SIZE, "%s-%x",
			 priv->plat->phy_bus_name, priv->plat->bus_id);
	else
		snprintf(bus_id, MII_BUS_ID_SIZE, "stmfp-%x",
			 priv->plat->bus_id);

	snprintf(phy_id, MII_BUS_ID_SIZE + 3, PHY_ID_FMT, bus_id,
		 priv->plat->phy_addr);
	priv->oldlink = 0;
	priv->speed = 0;
	priv->oldduplex = -1;
	if (netif_msg_link(priv))
		dev_dbg(priv->dev, "Trying to attach to %s\n", phy_id);
	phydev = phy_connect(dev, phy_id, &fpif_adjust_link, interface);
	if (IS_ERR(phydev)) {
		netdev_err(dev, "Could not attach to PHY\n");
		return PTR_ERR(phydev);
	}
	dev_info(priv->dev, "attached to PHY (UID 0x%x) Link = %d\n",
		 phydev->phy_id, phydev->link);
	priv->phydev = phydev;

	return 0;
}


static void fp_vif_close(struct net_device *netdev)
{
	struct fpif_priv *priv = netdev_priv(netdev);

	priv->users--;
	if (priv->users)
		return;

	netif_stop_queue(netdev);

	napi_disable(&priv->napi);
	fp_txdma_release(priv);
	fp_rxdma_release(priv);

	return;
}


static int fp_vif_open(struct net_device *netdev)
{
	struct fpif_priv *priv = netdev_priv(netdev);
	int err;

	if (priv->users) {
		priv->users++;
		return 0;
	}


	err = fp_rxdma_setup(priv);
	if (err) {
		dev_err(priv->dev, "Error in fp_rxdma_setup\n");
		return err;
	}
	fp_txdma_setup(priv);
	napi_enable(&priv->napi);
	netif_start_queue(netdev);
	priv->users++;
	return 0;
}


/**
 * fpif_open - Called when a network interface is made active
 * @netdev: network interface device structure
 *
 * Returns 0 on success, negative value on failure
 *
 * The open entry point is called when a network interface is made
 * active by the system (IFF_UP).  At this point all resources needed
 * for transmit and receive operations are allocated, the interrupt
 * handler is registered with the OS, the watchdog timer is started,
 * and the stack is notified that the interface is ready.
 */
static int fpif_open(struct net_device *netdev)
{
	struct fpif_priv *priv = netdev_priv(netdev);
	struct fpif_grp *fpgrp = priv->fpgrp;
	int err, idx;
	u8 bcast_macaddr[] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
	u32 macinfo;
	struct net_device *upper_dev;

	if (priv->logical_if) {
		fpgrp->ndev_cpu[0] = dev_get_by_name(dev_net(netdev),
							fpgrp->wlan0);
		fpgrp->ndev_cpu[1] = dev_get_by_name(dev_net(netdev),
							fpgrp->wlan1);
		return 0;
	}

	if (!is_valid_ether_addr(netdev->dev_addr)) {
		random_ether_addr(netdev->dev_addr);
		dev_warn(priv->dev, "generated random MAC address %pM\n",
			 netdev->dev_addr);
	}

	err = fpif_init_phy(netdev);
	if (unlikely(err)) {
		netdev_err(netdev, "Can't attach to PHY\n");
		return err;
	}
	if (priv->phydev)
		phy_start(priv->phydev);

	dev_dbg(priv->dev, "device MAC address :%p\n", netdev->dev_addr);
	napi_enable(&priv->napi);
	netif_start_queue(netdev);
	err = put_l2cam(priv, bcast_macaddr, &idx);
	if (err) {
		netdev_err(netdev, "Unable to put in l2cam for bcast\n");
		goto open_err4;
	}
	if (netif_msg_ifup(priv))
		dev_dbg(priv->dev, "l2_bcast_idx=%d\n", idx);
	err = put_l2cam(priv, netdev->dev_addr, &idx);
	if (err) {
		netdev_err(netdev, "Unable to put in l2cam\n");
		goto open_err3;
	}
	priv->ifaddr_idx = idx;
	if (netif_msg_ifup(priv))
		dev_dbg(priv->dev, "l2_idx=%d\n", idx);

	upper_dev = netdev_master_upper_dev_get_rcu(netdev);
	if (upper_dev)
		add_in_cams(upper_dev, netdev);

	if (priv->rgmii_base) {
		macinfo = readl(priv->rgmii_base + RGMII_MACINFO0);
		macinfo |= MACINFO_TXEN | MACINFO_RXEN;
		fpif_write_reg(priv->rgmii_base + RGMII_MACINFO0, macinfo);
	}

	err = fp_vif_open(fpgrp->netdev[DEVID_FP0]);
	if (err) {
		netdev_err(netdev, "Unable to setup buffers for dma0\n");
		goto open_err2;
	}

	err = fp_vif_open(fpgrp->netdev[DEVID_FP1]);
	if (err) {
		netdev_err(netdev, "Unable to setup buffers for dma1\n");
		goto open_err1;
	}

	if (priv->id == DEVID_DOCSIS)
		remove_tcam(fpgrp->base, TCAM_CM_SRCDOCSIS_DROP);

	return 0;

open_err1:
	fp_vif_close(fpgrp->netdev[DEVID_FP1]);

open_err2:
	fp_vif_close(fpgrp->netdev[DEVID_FP0]);
	if (priv->rgmii_base) {
		macinfo = readl(priv->rgmii_base + RGMII_MACINFO0);
		macinfo &= ~(MACINFO_TXEN | MACINFO_RXEN);
		fpif_write_reg(priv->rgmii_base + RGMII_MACINFO0, macinfo);
	}
	if (upper_dev)
		del_from_cams(upper_dev, netdev);

open_err3:
	remove_l2cam_if(priv);

open_err4:
	netif_stop_queue(netdev);
	napi_disable(&priv->napi);
	if (priv->phydev) {
		phy_stop(priv->phydev);
		phy_disconnect(priv->phydev);
		priv->phydev = NULL;
	}

	return err;
}


/**
 * fpif_close - Disables a network interface
 * @netdev: network interface device structure
 * Returns 0, this is not allowed to fail
 * The close entry point is called when an interface is de-activated
 * by the OS.
 **/
static int fpif_close(struct net_device *netdev)
{
	struct fpif_priv *priv = netdev_priv(netdev);
	struct fpif_grp *fpgrp = priv->fpgrp;
	u32 macinfo;
	struct net_device *upper_dev;

	if (priv->logical_if) {
		fpgrp->ndev_cpu[0] = NULL;
		fpgrp->ndev_cpu[1] = NULL;
		return 0;
	}

	if (priv->rgmii_base) {
		macinfo = readl(priv->rgmii_base + RGMII_MACINFO0);
		macinfo &= ~(MACINFO_TXEN | MACINFO_RXEN);
		fpif_write_reg(priv->rgmii_base + RGMII_MACINFO0, macinfo);
	}

	fp_vif_close(fpgrp->netdev[DEVID_FP0]);
	fp_vif_close(fpgrp->netdev[DEVID_FP1]);

	netif_stop_queue(netdev);
	napi_disable(&priv->napi);
	if (priv->phydev) {
		phy_stop(priv->phydev);
		phy_disconnect(priv->phydev);
		priv->phydev = NULL;
	}

	remove_tcam_promisc(priv);
	remove_tcam_allmulti(priv);

	upper_dev = netdev_master_upper_dev_get_rcu(netdev);
	if (upper_dev)
		del_from_cams(upper_dev, netdev);

	remove_l2cam_if(priv);

	if (priv->id == DEVID_DOCSIS)
		add_tcam_docsis(priv->fpgrp->base);
	return 0;
}


static void fpif_tx_timeout(struct net_device *netdev)
{
	pr_info("%s\n", __func__);

	return;
}


/**
 * fpif_get_stats - Get System Network Statistics
 * @dev: network interface device structure
 * @net_stats: rtnl_link stats
 * Description: returns the address of the device statistics structure.
 **/
static struct rtnl_link_stats64 *fpif_get_stats64(struct net_device *dev,
		struct rtnl_link_stats64 *net_stats)
{
	struct fpif_priv *priv = netdev_priv(dev);
	int sum = 0, sum_tx_errors = 0;
	u64 lo, hi;

	net_stats->rx_dropped = 0;
	net_stats->tx_dropped = 0;

	if (priv->logical_if) {
		net_stats->rx_packets = dev->stats.rx_packets;
		net_stats->rx_bytes = dev->stats.rx_bytes;
		net_stats->tx_packets = dev->stats.tx_packets;
		net_stats->tx_bytes = dev->stats.tx_bytes;
		return net_stats;
	}

	if (priv->rgmii_base) {
		lo = readl(priv->rgmii_base + RGMII_TX_CMPL_COUNT_LO);
		hi = readl(priv->rgmii_base + RGMII_TX_CMPL_COUNT_HI);
		net_stats->tx_packets = lo | (hi << 32);
		lo = readl(priv->rgmii_base + RGMII_TX_BYTE_COUNT_LO);
		hi = readl(priv->rgmii_base + RGMII_TX_BYTE_COUNT_HI);
		net_stats->tx_bytes = lo | (hi << 32);

		net_stats->rx_errors =
		    readl(priv->rgmii_base + RGMII_RX_ERROR_COUNT);
		net_stats->rx_crc_errors =
		    readl(priv->rgmii_base + RGMII_RX_FCS_ERR_CNT);

		lo = readl(priv->rgmii_base + RGMII_RX_BCAST_COUNT_LO) +
		    readl(priv->rgmii_base + RGMII_RX_MCAST_COUNT_LO) +
		    readl(priv->rgmii_base + RGMII_RX_UNICAST_COUNT_LO);
		hi = readl(priv->rgmii_base + RGMII_RX_BCAST_COUNT_HI) +
		    readl(priv->rgmii_base + RGMII_RX_MCAST_COUNT_HI) +
		    readl(priv->rgmii_base + RGMII_RX_UNICAST_COUNT_HI);

		net_stats->rx_packets = lo | (hi << 32);

		lo = readl(priv->rgmii_base + RGMII_RX_BYTE_COUNT_LO);
		hi = readl(priv->rgmii_base + RGMII_RX_BYTE_COUNT_HI);
		net_stats->rx_bytes = lo | (hi << 32);
		/* total number of multicast packets received */
		lo = readl(priv->rgmii_base + RGMII_RX_MCAST_COUNT_LO);
		hi = readl(priv->rgmii_base + RGMII_RX_MCAST_COUNT_HI);
		net_stats->multicast = lo | (hi << 32);

		/* Received length is unexpected */
		/* TBC:What if receive less than minimum ethernet frame */
		net_stats->rx_length_errors =
		    readl(priv->rgmii_base + RGMII_RX_OVERSIZED_ERR_CNT);

		net_stats->rx_frame_errors =
		    readl(priv->rgmii_base + RGMII_RX_ALIGN_ERR_CNT) +
		    readl(priv->rgmii_base + RGMII_RX_SYMBOL_ERR_CNT);

		net_stats->rx_missed_errors =
		    net_stats->rx_errors - (net_stats->rx_frame_errors +
					    net_stats->rx_length_errors +
					    net_stats->rx_over_errors);

		/* TBC : How to get rx collisions */
		net_stats->collisions =
		    readl(priv->rgmii_base + RGMII_TX_1COLL_COUNT) +
		    readl(priv->rgmii_base + RGMII_TX_MULT_COLL_COUNT) +
		    readl(priv->rgmii_base + RGMII_TX_LATE_COLL) +
		    readl(priv->rgmii_base + RGMII_TX_EXCESS_COLL) +
		    readl(priv->rgmii_base + RGMII_TX_ABORT_INTERR_COLL);
		sum_tx_errors = net_stats->collisions;
		net_stats->tx_aborted_errors =
		    readl(priv->rgmii_base + RGMII_TX_ABORT_COUNT);
		sum_tx_errors += net_stats->tx_aborted_errors;
		sum_tx_errors +=
		    sum + readl(priv->rgmii_base + RGMII_TX_DEFER_COUNT);
	} else {
		net_stats->rx_packets = readl(priv->fpgrp->base + FP_IMUX_PKTC +
				IMUX_RPT_OFF * priv->sp);
		net_stats->rx_bytes = readl(priv->fpgrp->base + FP_IMUX_BYTEC +
				IMUX_RPT_OFF * priv->sp);
		net_stats->tx_packets = readl(priv->fpgrp->base +
					FP_EMUX_PACKET_COUNT +
					EMUX_THRESHOLD_RPT_OFF * priv->sp);
		net_stats->tx_bytes = readl(priv->fpgrp->base +
					FP_EMUX_BYTE_COUNT +
					EMUX_THRESHOLD_RPT_OFF * priv->sp);
	}

	sum += readl(priv->fpgrp->base + FP_EMUX_DROP_PACKET_COUNT +
		     EMUX_THRESHOLD_RPT_OFF * priv->sp);
	net_stats->tx_fifo_errors = sum;
	sum_tx_errors += sum;
	net_stats->tx_errors = sum_tx_errors;

	return net_stats;
}

static const struct net_device_ops fpif_netdev_ops = {
	.ndo_open = fpif_open,
	.ndo_start_xmit = fpif_xmit_frame,
	.ndo_stop = fpif_close,
	.ndo_change_mtu = fpif_change_mtu,
	.ndo_set_rx_mode = fpif_set_multi,
	.ndo_get_stats64 = fpif_get_stats64,
	.ndo_set_mac_address = eth_mac_addr,
	.ndo_tx_timeout = fpif_tx_timeout,
};

static struct plat_fpif_data fp_vif_data[NUM_VIRT_INTFS] = {
	{
	.iftype = DEVID_FP0,
	.ifname = "fpvif0",
	.tx_dma_ch = 0,
	.rx_dma_ch = 0,
	.q_idx = 3,
	.buf_thr = 35,
	},
	{
	.iftype = DEVID_FP1,
	.ifname = "fpvif1",
	.tx_dma_ch = 0,
	.rx_dma_ch = 1,
	.q_idx = 3,
	.buf_thr = 35,
	},
};

static int fpif_init(struct fpif_grp *fpgrp)
{
	int err, j, rx_dma_ch, tx_dma_ch;
	struct fpif_priv *priv;
	struct net_device *netdev;
	struct plat_fpif_data *fpif_data;

	fpgrp->available_l2cam = fpgrp->plat->available_l2cam;
	fpgrp->l2cam_size = fpgrp->plat->l2cam_size;
	if (fpgrp->plat->version == FP)
		fpgrp->txbase = fpgrp->base + FASTPATH_TXDMA_BASE;
	else
		fpgrp->txbase = fpgrp->base + FASTPATH_TOE_BASE;
	fpgrp->rxbase = fpgrp->base + FASTPATH_RXDMA_BASE;
	for (j = 0; j < NUM_INTFS; j++) {
		if (j < NUM_PHY_INTFS)
			fpif_data = fpgrp->plat->if_data[j];
		else
			fpif_data = &(fp_vif_data[j - NUM_PHY_INTFS]);
		if (!fpif_data)
			continue;
		netdev = alloc_etherdev(sizeof(struct fpif_priv));
		if (!netdev) {
			err = -ENOMEM;
			pr_err("Error in alloc_etherdev for j=%d\n", j);
			goto err_init;
		}
		fpgrp->netdev[j] = netdev;
		priv = netdev_priv(netdev);
		priv->plat = fpif_data;
		priv->fpgrp = fpgrp;
		priv->netdev = netdev;
		SET_NETDEV_DEV(netdev, fpgrp->dev);
		priv->netdev->base_addr = (u32) fpgrp->base;
		priv->dev = fpgrp->dev;
		priv->msg_enable = netif_msg_init(debug, default_msg_level);
		tx_dma_ch = priv->plat->tx_dma_ch;
		rx_dma_ch = priv->plat->rx_dma_ch;
		priv->tx_dma_ch = tx_dma_ch;
		priv->rx_dma_ch = rx_dma_ch;
		if (rx_dma_ch >= MAX_RXDMA) {
			netdev_err(priv->netdev, "Wrong rxch(%d) passed\n",
				   rx_dma_ch);
			err = -EINVAL;
			goto err_init;
		}
		if (tx_dma_ch >= MAX_TXDMA) {
			netdev_err(priv->netdev, "Wrong txch(%d) passed\n",
				   tx_dma_ch);
			err = -EINVAL;
			goto err_init;
		}
		priv->txdma_ptr = &(fpgrp->txdma_info[tx_dma_ch]);
		priv->txdma_ptr->txbase = fpgrp->txbase;
		priv->rxdma_ptr = &(fpgrp->rxdma_info[rx_dma_ch]);
		priv->rxdma_ptr->rxbase = fpgrp->rxbase;
		spin_lock_init(&priv->fpif_lock);
		/* initialize a napi context */
		netif_napi_add(netdev, &priv->napi, fpif_poll, FP_NAPI_BUDGET);
		netdev->netdev_ops = &fpif_netdev_ops;
		fpif_set_ethtool_ops(netdev);
		netdev->hw_features |= NETIF_F_HW_CSUM | NETIF_F_RXCSUM;
		netdev->hw_features |= NETIF_F_SG | NETIF_F_TSO;
		netdev->features |= netdev->hw_features;
		netdev->hard_header_len += FP_HDR_SIZE;
		strncpy(netdev->name, priv->plat->ifname,
			sizeof(priv->plat->ifname));
		netdev->watchdog_timeo = 5 * HZ,

		priv->promisc_idx = TCAM_IDX_INV;
		priv->allmulti_idx = TCAM_IDX_INV;
		priv->br_l2cam_idx = IDX_INV;
		priv->br_tcam_idx = IDX_INV;
		priv->ifaddr_idx = IDX_INV;
		priv->id = priv->plat->iftype;
		switch (priv->id) {
		case DEVID_GIGE0:
			priv->sp = SP_GIGE;
			priv->dmap = DEST_GIGE;
			priv->rgmii_base = fpgrp->base + RGMII0_OFFSET;
			fp_promisc[priv->id].netdev = netdev;
			fp_promisc[priv->id].ifidx_log = IDX_INV;
			break;

		case DEVID_DOCSIS:
			priv->sp = SP_DOCSIS;
			priv->dmap = DEST_DOCSIS;
			priv->ifidx = 0xf;
			fp_promisc[priv->id].netdev = netdev;
			fp_promisc[priv->id].ifidx_log = IDX_INV;
			break;

		case DEVID_GIGE1:
			priv->sp = SP_ISIS;
			priv->dmap = DEST_ISIS;
			fp_promisc[priv->id].netdev = netdev;
			fp_promisc[priv->id].ifidx_log = IDX_INV;
			if (fpgrp->plat->version != FP)
				priv->rgmii_base = fpgrp->base + RGMII1_OFFSET;
			break;

		case DEVID_FP0:
			priv->sp = SP_SWDEF;
			priv->dmap = 0;
			priv->logical_if = 1;
			break;

		case DEVID_FP1:
			priv->sp = SP_SWDEF;
			priv->dmap = 0;
			priv->logical_if = 1;
			break;

		default:
			netdev_err(netdev, "port type not correct\n");
			err = -EINVAL;
			goto err_init;
		}

		if (rx_dma_ch == 0)
			priv->dma_port = DEST_NPDMA;
		else if (rx_dma_ch == 1)
			priv->dma_port = DEST_APDMA;
		else
			priv->dma_port = DEST_WDMA;

		priv->rx_buffer_size = netdev->mtu + netdev->hard_header_len +
					VLAN_HLEN * 2;
		priv->txdma_ptr->users = 0;
		priv->rxdma_ptr->users = 0;

		err = register_netdev(netdev);
		if (err) {
			netdev_err(netdev, "register_netdev failed %d\n", err);
			goto err_init;
		}

		if (priv->id == DEVID_DOCSIS)
			stmfp_init_sysfs(netdev);

		if (priv->plat->mdio_bus_data) {
			err = fpif_mdio_register(netdev);
			if (err < 0) {
				netdev_err(netdev, "fpif_mdio_register err=%d\n",
					   err);
				goto err_init;
			}
		}
	}

	for (j = 0; j < fpgrp->l2cam_size; j++)
		fpgrp->l2_idx[j] = NUM_INTFS;

	err = register_netdevice_notifier(&ovs_dp_device_notifier);
	if (err)
		pr_err("fp:ERROR in reg notifier for netdev\n");
	return 0;
 err_init:
	fpif_deinit(fpgrp);
	return err;
}

static int fpif_probe(struct platform_device *pdev)
{
	int err;
	void __iomem *base;
	struct resource *res = NULL;
	int tx_irq;
	int rx_irq0, rx_irq1;
	struct device *dev = &pdev->dev;
	struct fpif_grp *fpgrp;
	struct plat_stmfp_data *plat_dat;

	pr_debug("%s\n", __func__);

	/* Map FastPath register memory */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		pr_err("ERROR :%s: platform_get_resource ", __func__);
		return -ENODEV;
	}

	base = devm_request_and_ioremap(&pdev->dev, res);
	if (!base) {
		pr_err("ERROR :%s: Couldn't ioremap regs\n", __func__);
		return -ENODEV;
	}
	pr_info("fastpath base=%p\n", base);

	if (pdev->dev.of_node) {
		plat_dat =
		    devm_kzalloc(&pdev->dev, sizeof(struct plat_stmfp_data),
				 GFP_KERNEL);
		if (!plat_dat)
			return -ENOMEM;

		if (pdev->dev.platform_data)
			memcpy(plat_dat, pdev->dev.platform_data,
			       sizeof(struct plat_stmfp_data));

		err = stmfp_probe_config_dt(pdev, plat_dat);
		if (err) {
			pr_err("%s: FP config of DT failed", __func__);
			return err;
		}
		if (pdev->dev.platform_data)
			pdev->dev.platform_data = plat_dat;
	} else {
		plat_dat = pdev->dev.platform_data;
	}

	pdev->dev.platform_data = plat_dat;

	/* Get Rx interrupt number */
	rx_irq0 = platform_get_irq_byname(pdev, "fprxdma0irq");
	if (rx_irq0 == -ENXIO) {
		pr_err("%s: ERROR: Rx IRQ0 config info not found\n", __func__);
		return -ENXIO;
	}

	rx_irq1 = platform_get_irq_byname(pdev, "fprxdma1irq");
	if (rx_irq1 == -ENXIO) {
		pr_err("%s: ERROR: Rx IRQ1 config info not found\n", __func__);
		return -ENXIO;
	}

	/* Get Tx interrupt number */
	tx_irq = platform_get_irq_byname(pdev, "fptxdmairq");
	if (tx_irq == -ENXIO) {
		pr_err("%s: ERROR: Tx IRQ config info not found\n", __func__);
		return -ENXIO;
	}

	fpgrp = devm_kzalloc(&pdev->dev, sizeof(struct fpif_grp),
						GFP_KERNEL);
	if (fpgrp == NULL) {
		pr_err("ERROR : Error in allocation of memory\n");
		return -ENOMEM;
	}

	err = devm_request_irq(&pdev->dev, rx_irq0, fpif_intr, 0,
					"fastpath_rx0", fpgrp);
	if (err) {
		pr_err("ERROR : Unable to allocate rx0 intr err=%d\n", err);
		return -ENXIO;
	}
	irq_set_affinity_hint(rx_irq0, cpumask_of(0));

	err = devm_request_irq(&pdev->dev, rx_irq1, fpif_intr, 0,
					"fastpath_rx1", fpgrp);
	if (err) {
		pr_err("ERROR : Unable to allocate rx1 intr err=%d\n", err);
		return -ENXIO;
	}
	irq_set_affinity_hint(rx_irq1, cpumask_of(1));

	err = devm_request_irq(&pdev->dev, tx_irq, fpif_intr, 0,
				"fastpath_tx", (void *)fpgrp);
	if (err) {
		pr_err("ERROR : Unable to allocate tx intr err=%d\n", err);
		return -ENXIO;
	}

	pr_info("irqs= %d %d %d\n", rx_irq0, rx_irq1, tx_irq);
	fpgrp->base = base;
	fpgrp->dev = dev;
	fpgrp->tx_irq = tx_irq;
	fpgrp->rx_irq0 = rx_irq0;
	fpgrp->rx_irq1 = rx_irq1;
	fpgrp->plat = dev->platform_data;

	err = fpif_init(fpgrp);
	if (err < 0) {
		pr_err("ERROR: %s: err=%d\n", __func__, err);
		return err;
	}
	platform_set_drvdata(pdev, fpgrp);

	fpgrp->clk_fp = devm_clk_get(&pdev->dev, "CLK_FP");
	if (IS_ERR(fpgrp->clk_fp)) {
		dev_err(&pdev->dev, "CLK_FP not found\n");
		return PTR_ERR(fpgrp->clk_fp);
	}
	clk_prepare_enable(fpgrp->clk_fp);

	fpgrp->clk_ife = devm_clk_get(&pdev->dev, "CLK_IFE_216_FP");
	if (IS_ERR(fpgrp->clk_ife)) {
		dev_err(&pdev->dev, "CLK_IFE_216_FP not found\n");
		clk_put(fpgrp->clk_fp);
		return PTR_ERR(fpgrp->clk_ife);
	}
	clk_prepare_enable(fpgrp->clk_ife);

	/* Get reset */
	fpgrp->rstc = devm_reset_control_get(&pdev->dev, NULL);
	if (IS_ERR(fpgrp->rstc)) {
		err = -EINVAL;
		goto exit_not_reset;
	}
	/* Perform full reset */
	reset_control_assert(fpgrp->rstc);
	reset_control_deassert(fpgrp->rstc);

	fp_hwinit(fpgrp);
	pr_info("%s ends\n", __func__);

	return 0;

exit_not_reset:
	clk_disable_unprepare(fpgrp->clk_fp);
	clk_put(fpgrp->clk_fp);
	clk_disable_unprepare(fpgrp->clk_ife);
	clk_put(fpgrp->clk_ife);

	return err;

}

static int fpif_remove(struct platform_device *pdev)
{
	struct fpif_grp *fpgrp = platform_get_drvdata(pdev);

	fpif_deinit(fpgrp);
	return 0;
}

static const struct of_device_id stmfp_dt_ids[] = {
	{.compatible = "st,fp"},
	{.compatible = "st,fplite"},
	{ }
};

MODULE_DEVICE_TABLE(of, stmfp_dt_ids);

static struct platform_driver fpif_driver = {
	.probe = fpif_probe,
	.remove = fpif_remove,
	.driver = {
		   .name = DRV_NAME,
		   .owner = THIS_MODULE,
		   .of_match_table = of_match_ptr(stmfp_dt_ids),
		   },
};

static int __init fpif_init_module(void)
{
	return platform_driver_register(&fpif_driver);
}

/**
 * fpif_exit_module - Device exit Routine
 * fpif_exit_module is to alert the driver
 * that it should release a device because
 * the driver is going to be removed from
 * memory.
 **/
static void __exit fpif_exit_module(void)
{
	platform_driver_unregister(&fpif_driver);
}

module_init(fpif_init_module);
module_exit(fpif_exit_module);

MODULE_DESCRIPTION("STMFP 10/100/1000 Ethernet driver");
MODULE_AUTHOR("Manish Rathi <manish.rathi@st.com>");
MODULE_LICENSE("GPL");
