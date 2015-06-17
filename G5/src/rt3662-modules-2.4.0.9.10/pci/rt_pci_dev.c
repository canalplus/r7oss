#include "rlk_inic.h"
#ifdef NM_SUPPORT
#include "net/sch_generic.h"
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,20)
inline void netif_rx_complete(struct net_device *dev)
{
	unsigned long flags;

	local_irq_save(flags);
	BUG_ON(!test_bit(__LINK_STATE_RX_SCHED, &dev->state));
	list_del(&dev->poll_list);
	smp_mb__before_clear_bit();
	clear_bit(__LINK_STATE_RX_SCHED, &dev->state);
	local_irq_restore(flags);
}
#endif

#ifndef HAVE_FREE_NETDEV
inline void free_netdev(struct net_device *dev)
{
	kfree(dev);
}
#endif

#ifndef HAVE_NETDEV_PRIV
inline void *netdev_priv(struct net_device *dev)
{
	return dev->priv;
}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,29)
#define NETIF_RX_COMPLETE(dev, napi) 	netif_rx_complete(dev, napi)
#define NETIF_RX_SCHEDULE_PREP(dev, napi) 	netif_rx_schedule_prep(dev, napi)
#define __NETIF_RX_SCHEDULE(dev, napi)		__netif_rx_schedule(dev, napi)
#elif LINUX_VERSION_CODE == KERNEL_VERSION(2,6,29)
#define NETIF_RX_COMPLETE(dev, napi)	netif_rx_complete(napi);
#define NETIF_RX_SCHEDULE_PREP(dev, napi)	netif_rx_schedule_prep(napi)
#define __NETIF_RX_SCHEDULE(dev, napi)		__netif_rx_schedule(napi)
#else
#define NETIF_RX_COMPLETE(dev, napi) 	napi_complete(napi)
#define NETIF_RX_SCHEDULE_PREP(dev, napi) 	napi_schedule_prep(napi)
#define __NETIF_RX_SCHEDULE(dev, napi)		__napi_schedule(napi)
#endif

/* These identify the driver base version and may not be removed. */
static char version[] =
KERN_INFO DRV_NAME ": 802.11n WLAN PCI driver v" DRV_VERSION " (" DRV_RELDATE ")\n";

MODULE_AUTHOR("XiKun Wu <xikun_wu@ralinktech.com.tw>");
MODULE_DESCRIPTION("Ralink iNIC 802.11n WLAN PCI driver");

static int debug = -1;
char *mode  = "ap";	   // ap mode
char *mac = "";		   // default 00:00:00:00:00:00
static int bridge = 1; // enable built-in bridge
static int csumoff = 0;	// Enable Checksum Offload over IPv4(TCP, UDP) on STA mode
						// It's not recommended on AP mode
int max_fw_upload = 5; /* Max Firmware upload attempt */

#ifdef DBG
char *root = "";
#endif

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,12)
#ifdef DBG
MODULE_PARM (root, "s");
#endif
MODULE_PARM (debug, "i");
MODULE_PARM (bridge, "i");
MODULE_PARM (csumoff, "i");
MODULE_PARM (max_fw_upload, "i");
MODULE_PARM (mode, "s");
MODULE_PARM (mac, "s");
#else
#ifdef DBG
module_param (root, charp, 0);
MODULE_PARM_DESC (root,  DRV_NAME ": firmware and profile path offset");
#endif
module_param (debug, int, 0);
module_param (bridge, int, 1);
module_param (csumoff, int, 0);
module_param (max_fw_upload, int, 5);
module_param (mode, charp, 0);
module_param (mac, charp, 0);
#endif
MODULE_PARM_DESC (debug, DRV_NAME ": bitmapped message enable number");
MODULE_PARM_DESC (mode, DRV_NAME ": iNIC operation mode: AP(default) or STA");
MODULE_PARM_DESC (mac, DRV_NAME ": iNIC mac addr");
MODULE_PARM_DESC (max_fw_upload, DRV_NAME ": Max Firmware upload attempt");
MODULE_PARM_DESC (bridge, DRV_NAME ": on/off iNIC built-in bridge engine");
MODULE_PARM_DESC (csumoff, DRV_NAME ": on/off checksum offloading over IPv4 <TCP, UDP>");

int rlk_inic_open (struct net_device *dev);
int rlk_inic_close (struct net_device *dev);
int  rlk_inic_start_xmit(struct sk_buff *skb, struct net_device *dev);

static void rlk_inic_set_rx_mode (struct net_device *dev);
static int rlk_inic_send_packet(struct sk_buff *skb, struct net_device *dev);
static struct net_device_stats *rlk_inic_get_stats(struct net_device *dev);

static void __rlk_inic_set_rx_mode (struct net_device *dev);
static void rlk_inic_tx (iNIC_PRIVATE *rt);
static void rlk_inic_clean_rings (iNIC_PRIVATE *rt);

static struct pci_device_id rlk_inic_pci_tbl[] = {
	{0x1814, 0x0801, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},    
	{0,}		/* terminate list */
};

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,28)
static const struct net_device_ops netdev_ops = {
	.ndo_open		= rlk_inic_open,
	.ndo_stop		= rlk_inic_close,
	.ndo_set_multicast_list = rlk_inic_set_rx_mode,
#ifdef IKANOS_VX_1X0
	.ndo_start_xmit   = IKANOS_DataFramesTx,
#else
	.ndo_start_xmit    = rlk_inic_send_packet,
#endif
	.ndo_get_stats  = rlk_inic_get_stats,
	.ndo_do_ioctl       = rlk_inic_ioctl,
};
#endif

MODULE_DEVICE_TABLE(pci, rlk_inic_pci_tbl);

#ifdef DEBUG
static void dump_tx_desc(struct rlk_inic_tx_desc *desc)
{
	printk("== TX Desc == \n");
	printk("Word 0 - %08X\n", *(unsigned int *)&desc->txd_info1);
	printk("Word 1 - %08X\n", *(unsigned int *)&desc->txd_info2);
	printk("Word 2 - %08X\n", *(unsigned int *)&desc->txd_info3);
	printk("Word 3 - %08X\n", *(unsigned int *)&desc->txd_info4);

}
#endif


static void rlk_inic_int_disable(iNIC_PRIVATE *rt, int mask)
{
	u32 regValue;

	rt->int_disable_mask |= mask;
	/* disable RX_DONE_INT0 */
	regValue =  rt->int_enable_reg & ~(rt->int_disable_mask);

	RT_REG_WRITE32(FE_INT_ENABLE, regValue);    
}

static void rlk_inic_int_enable(iNIC_PRIVATE *rt, int mask)
{
	u32 regValue;

	rt->int_disable_mask &= ~(mask);
	regValue = rt->int_enable_reg & ~(rt->int_disable_mask);        
	RT_REG_WRITE32(FE_INT_ENABLE, regValue);    
}


static void __rlk_inic_set_rx_mode (struct net_device *dev)
{

}

static inline void rlk_inic_set_rxbufsize (iNIC_PRIVATE *rt)
{
	rt->rx_buf_sz = PKT_BUF_SZ;
}


/*
 *	Determine the packet's protocol ID. The rule here is that we 
 *	assume 802.3 if the type field is short enough to be a length.
 *	This is normal practice and works for any 'now in use' protocol.
 */



static inline void rlk_inic_rx_skb (iNIC_PRIVATE *rt, struct sk_buff *skb,
								  struct rlk_inic_rx_desc *desc)
{
	unsigned char dev_id; 
	
//	rt->net_stats.rx_packets++;
//	rt->net_stats.rx_bytes += skb->len;
	rt->dev->last_rx = jiffies;    

	dev_id = Remove_Vlan_Tag(rt, skb);

	skb->protocol = eth_type_trans (skb, skb->dev); 

	/* 
	 * if the packet is not racfg frame, 
	 * pass it into upper layer 
	 */
#ifndef SIMULATE_MII_MASTER_BY_PCI 
	if (racfg_frame_handle(rt, skb) == FALSE)
#endif
	{

/* Push up the protocol stack */
#ifdef IKANOS_VX_1X0
		skb->data -= 14;
		skb->len += 14;
		IKANOS_DataFrameRx(rt, dev_id, skb->dev, skb, skb->len);
#else
		//netif_rx(skb);
		netif_receive_skb(skb);
#endif
	}
}

static inline unsigned int rlk_inic_rx_csum_ok (u32 status)
{
	if (csumoff)
		return 1;
	else
		return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
static int rlk_inic_rx_poll (struct net_device *dev, int *budget)
{
	iNIC_PRIVATE *rt = netdev_priv(dev);
	unsigned rx_work = min(dev->quota, *budget);
#else
static int rlk_inic_rx_poll (struct napi_struct *napi, int budget)
{
	unsigned rx_work = budget;

	//struct net_device *dev = napi->dev;
	//iNIC_PRIVATE *rt = netdev_priv(dev);

	iNIC_PRIVATE *rt = container_of(napi, iNIC_PRIVATE, napi);
	struct net_device *dev = rt->dev;

#endif

	unsigned rx;
	int entry;


#ifdef __BIG_ENDIAN
	PDMA_RXD_INFO2_T rxd_info2;  
#endif

	rx_status_loop:
	rx = 0; 
	RT_REG_WRITE32(FE_INT_STATUS, rlk_inic_rx_intr_mask);

	while (1)
	{
		u32 status, len;
		dma_addr_t mapping;
		struct sk_buff *skb, *new_skb;
		struct rlk_inic_rx_desc *desc;
		unsigned buflen;

		entry = rt->rx_dma_owner_idx0;
		skb = rt->rx_skb[entry].skb;
		if (!skb)
			BUG();

		desc = &rt->rx_ring[entry];
		mapping = rt->rx_skb[entry].mapping;

#ifdef __BIG_ENDIAN
		*(int *)&rxd_info2 = cpu_to_le32(*(int *)&desc->rxd_info2);
		if (rxd_info2.DDONE_bit == 0)
#else
		if (desc->rxd_info2.DDONE_bit == 0)
#endif
		{
			break;
		}

#ifdef __BIG_ENDIAN
		len = rxd_info2.PLEN0;
#else
		len = desc->rxd_info2.PLEN0;
#endif

		if (len > 1536)
		{

			rt->net_stats.rx_dropped++;
			printk("ERROR! Packet size(%d) overflow!\n", len);
			goto rx_next;
		}
		ASSERT(len<=1536);
		status = 0;

		if (netif_msg_rx_status(rt))
			printk(KERN_DEBUG "%s: rx slot %d status 0x%x len %d\n",
				   rt->dev->name, entry, status, len);

		buflen = rt->rx_buf_sz + RX_OFFSET;
#ifdef PCI_FORCE_DMA
		new_skb = __dev_alloc_skb (buflen, GFP_ATOMIC|GFP_DMA);
#else
		new_skb = dev_alloc_skb (buflen);
#endif
		if (!new_skb)
		{
			rt->net_stats.rx_dropped++;
			printk("!new_skb\n");
			goto rx_next;
		}

		//memset(new_skb->data, 0, rt->rx_buf_sz);

		skb_reserve(new_skb, RX_OFFSET);
		new_skb->dev = rt->dev;

		pci_unmap_single(rt->pdev, mapping,
						 buflen, PCI_DMA_FROMDEVICE);

		/* Handle checksum offloading for incoming packets. */
		if (rlk_inic_rx_csum_ok(status))
			skb->ip_summed = CHECKSUM_UNNECESSARY;
		else
			skb->ip_summed = CHECKSUM_NONE;

		skb_put(skb, len);

		mapping = rt->rx_skb[entry].mapping =
				  pci_map_single(rt->pdev, new_skb->tail,
								 buflen, PCI_DMA_FROMDEVICE);
		rt->rx_skb[entry].skb = new_skb;

		//hex_dump("RX ", skb->data, skb->len);
		rlk_inic_rx_skb(rt, skb, desc);

		rx++;


rx_next:
	
		rt->rx_ring[entry].rxd_info1.PDP0 = cpu_to_le32(mapping);
		memset(&rt->rx_ring[entry].rxd_info2, 0, sizeof(PDMA_RXD_INFO2_T));
		memset(&rt->rx_ring[entry].rxd_info4, 0, sizeof(PDMA_RXD_INFO4_T));
		rt->rx_ring[entry].rxd_info2.LS0 = 1;
		wmb();

		/* move pointer to next RXD which wants to alloc */
		RT_REG_WRITE32_FLUSH(RX_CALC_IDX0, entry);

		RLK_INIC_RX_IDX0_INC(rt->rx_dma_owner_idx0);

		if (!rx_work--)
			break;
	}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
	dev->quota -= rx;
	*budget -= rx;
#endif

	/* 
	 * if we did not reach work limit, then we're done with
	 * this round of polling
	 */

	if (rx_work)
	{
		unsigned long flags;

		if (RT_REG_READ32(   FE_INT_STATUS)    &    rlk_inic_rx_intr_mask)
			goto rx_status_loop;

		//local_irq_disable();
		local_irq_save(flags);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
		netif_rx_complete(dev);
#else
		NETIF_RX_COMPLETE(dev, napi);
#endif
		rlk_inic_int_enable(rt, FE_RX_DONE_INT0);             
		local_irq_restore(flags);

		//local_irq_enable();

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
		return 0;	/* done */
	}

	return 1;		/* not done */
#else
	}
	return rx;
#endif

}

static irqreturn_t
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
rlk_inic_interrupt (int irq, void *dev_instance, struct pt_regs *regs)
#else
rlk_inic_interrupt (int irq, void *dev_instance)
#endif
{
	struct net_device *dev = dev_instance;
	iNIC_PRIVATE *rt;
	//u32 flags;
	u32 status;

	if (unlikely(dev == NULL))
	{
		return IRQ_NONE;
	}

	rt = netdev_priv(dev);

	status = RT_REG_READ32(FE_INT_STATUS);

	if (!status || status == 0xFFFFFFFF)
		return IRQ_NONE;

#if 0
	RT_REG_WRITE32_FLUSH(FE_INT_STATUS, status); //status & ~rlk_inic_rx_intr_mask);
#else
	RT_REG_WRITE32_FLUSH(FE_INT_STATUS, status & ~rlk_inic_rx_intr_mask);
#endif

	spin_lock(&rt->lock);
	//local_irq_save(flags);

	/* close possible race's with dev_close */
	if (unlikely(!netif_running(dev)))
	{
		rlk_inic_int_disable(rt, INT_ENABLE_ALL);
		spin_unlock(&rt->lock);
		//local_irq_restore(flags);
		return IRQ_HANDLED;
	}

	if (status & FE_RX_DONE_INT0)
	{

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
		if (netif_rx_schedule_prep(dev))
		{
			rlk_inic_int_disable(rt, FE_RX_DONE_INT0);
			__netif_rx_schedule(dev);
		}
#else
		if (NETIF_RX_SCHEDULE_PREP(dev,  &rt->napi))
		{
			rlk_inic_int_disable(rt, FE_RX_DONE_INT0);
			__NETIF_RX_SCHEDULE(dev,  &rt->napi);
		}
		/*
		else
		{
			rlk_inic_int_enable(rt, rt->int_enable_reg);
		}
		*/
#endif
	}

	if (status & FE_TX_DONE_INT0)
	{
		rlk_inic_tx(rt);
	}


	spin_unlock(&rt->lock);
	//local_irq_restore(flags);

	return IRQ_HANDLED;
}

static void rlk_inic_tx (iNIC_PRIVATE *rt)
{
	int entry;
	struct rlk_inic_tx_desc *tx_desc;
	struct sk_buff *skb;

#ifdef __BIG_ENDIAN
	PDMA_TXD_INFO2_T txd_info2;
	PDMA_TXD_INFO4_T txd_info4;
#endif

	while ((signed int)(rt->tx0_send_count - rt->tx0_done_count) > 0)
	{
		entry = rt->tx0_done_count % RLK_INIC_TX_RING_SIZE;
		tx_desc = (struct rlk_inic_tx_desc *) &rt->tx_ring0[entry];

#ifdef __BIG_ENDIAN
		*(int *)&txd_info2=cpu_to_le32(*(int *)&tx_desc->txd_info2);
		*(int *)&txd_info4=cpu_to_le32(*(int *)&tx_desc->txd_info4);
#endif
		rmb();
#ifdef __BIG_ENDIAN
		if (txd_info2.DDONE_bit == 0)
#else
		if (tx_desc->txd_info2.DDONE_bit == 0)
#endif
		{
			break;
		}

		skb = rt->tx_skb[entry].skb;
		if (!skb)
		{
			printk("rlk_inic_tx => ERROR !! send count=%d, done_count=%d, entry=%d\n",
				   rt->tx0_send_count, rt->tx0_done_count, entry);
			BUG();
		}

		pci_unmap_single(rt->pdev, rt->tx_skb[entry].mapping,
						 skb->len, PCI_DMA_TODEVICE);

		/* update tx stats */
		//rt->net_stats.tx_packets++;
		//rt->net_stats.tx_bytes += skb->len;

		dev_kfree_skb_irq(skb);

		rt->tx_skb[entry].skb = NULL;

		/* reset tx desc */
#ifdef __BIG_ENDIAN
		txd_info2.LS0_bit = 1;
		txd_info2.DDONE_bit = 1;
		txd_info4.PN = 1;
		txd_info4.QN = 0;
		*(int *)&tx_desc->txd_info2=cpu_to_le32(*(int *)&txd_info2);
		*(int *)&tx_desc->txd_info4=cpu_to_le32(*(int *)&txd_info4);
#else
		tx_desc->txd_info2.LS0_bit = 1;
		tx_desc->txd_info2.DDONE_bit = 1;
		tx_desc->txd_info4.PN = 1;
		tx_desc->txd_info4.QN = 0;
#endif
		rt->tx0_done_count++;
	}

	if (TX_BUFFS_AVAIL(rt) > 1)
		netif_wake_queue(rt->dev);
}


static void rlk_inic_show_debug(struct net_device *dev)
{
	iNIC_PRIVATE *rt = netdev_priv(dev);
	int x;

	printk("FE_INT_STATUS = %08x\n", RT_REG_READ32(FE_INT_STATUS));
	printk("FE_INT_ENABLE = %08x\n", RT_REG_READ32(FE_INT_ENABLE));

	for (x=0; x<0x40; x+=4)
	{
		printk("PDMA %02x -- 0x%08x\n", x, RT_REG_READ32(0x100+x));
	}


#if 0
	for (entry = 0; entry < RLK_INIC_TX_RING_SIZE; entry++)
	{
		tx_desc = (struct rlk_inic_tx_desc *) &rt->tx_ring0[entry];       
		rmb();
		printk("(%d) DONE_bit = %d\n", entry, tx_desc->txd_info2.DDONE_bit);
	}
#endif

	printk("tx0_send_count = %u, tx0_done_count = %u\n", rt->tx0_send_count, rt->tx0_done_count);
	//printk("rx_netlink = %d\n", rx_netlink);
}

#if 0
static void show_tx_desc(struct rlk_inic_tx_desc *desc)
{
	printk("== TX Desc == \n");
	printk("Word 0 - %08x\n", (u32)desc->txd_info1.SDP0);
	printk("Word 1 - %08x\n", *((u32 *)&(desc->txd_info2)));
}
#endif

static int txful;


static int rlk_inic_send_packet(struct sk_buff *skb, struct net_device *dev)
{
	int dev_id = 0;
	iNIC_PRIVATE *rt = netdev_priv(dev);

	ASSERT(skb);

	if (RaCfgDropRemoteInBandFrame(skb))
	{
		rt->net_stats.tx_dropped++;
		return NETDEV_TX_OK;
	}

	if (rt->RaCfgObj.bBridge == 1)
	{
		rt->net_stats.tx_packets++;
		rt->net_stats.tx_bytes += skb->len;

		return rlk_inic_start_xmit(skb, dev);
	}
	else
	{
		skb = Insert_Vlan_Tag(rt, skb, dev_id, SOURCE_MBSS);
		if (skb)
		{
			rt->net_stats.tx_packets++;
			rt->net_stats.tx_bytes += skb->len;

			return rlk_inic_start_xmit(skb, dev); 
		}
		else
		{
			rt->net_stats.tx_dropped++;
		}
	}
	return NETDEV_TX_OK;
} /* End of rlk_inic_send_packet */


int rlk_inic_start_xmit (struct sk_buff *skb, struct net_device *dev)
{
	iNIC_PRIVATE *rt = netdev_priv(dev);
	unsigned long flags;
	unsigned entry;
#ifdef __BIG_ENDIAN
	PDMA_TXD_INFO2_T txd_info2;
#endif

	if (skb->data[12] == 0xff && skb->data[13] == 0xff)
	{
		skb->data[6] |= 0x01;    
#ifdef INBAND_DEBUG
		if (rt->RaCfgObj.bGetMac)
		{
			struct racfghdr *p_racfgh =  (struct racfghdr *) &skb->data[14];
			if (le16_to_cpu(p_racfgh->dev_type) == IW_POINT_TYPE)
			{
				u16 total = le16_to_cpu(p_racfgh->length);
				u16 len   = *(u16 *)p_racfgh->data;
				if (len % 2) len++;	// pading to even number

				printk("iwreq iw_point type: len=%d, "
					   "flags=0x%04x, tail=0x%04x\n", len, 
					   *(u16 *)&(p_racfgh->data[4+len]), 
					   *(u16 *)&(p_racfgh->data[total-2]));
			}
			hex_dump("send", skb->data, 64);
		}
#endif
	}


	spin_lock_irqsave(&rt->lock, flags);

	rlk_inic_tx(rt);

//	rt->net_stats.tx_packets++;
//	rt->net_stats.tx_bytes += skb->len;

	/* This is a hard error, log it. */
	if (TX_BUFFS_AVAIL(rt) <=  1)
	{
		netif_stop_queue(dev);
		if ((++txful%5000) == 0)
		{
			printk(KERN_ERR PFX "%s: BUG! Tx Ring full when queue awake!\n",
				   dev->name);
			printk("txful = %d\n", txful);
			rlk_inic_show_debug(dev);
		}
		spin_unlock_irqrestore(&rt->lock, flags);

		printk(KERN_ERR PFX "%s: BUG! Tx Ring full?\n",dev->name);
#if 1
		/*
		 * caller take responsiblilty to handle this sk_buffer
		 */
		return NETDEV_TX_BUSY;
#else
		/* 
		 * driver handle this sk_buffer
		 */
		dev_kfree_skb(skb);
		return NETDEV_TX_OK; 
#endif
	}

	entry = rt->tx_cpu_owner_idx0;

#ifdef __BIG_ENDIAN
	*(int *)&txd_info2 = cpu_to_le32(*(int *)&rt->tx_ring0[entry].txd_info2);
	ASSERT(txd_info2.DDONE_bit != 0);
#else
	ASSERT(rt->tx_ring0[entry].txd_info2.DDONE_bit != 0);
#endif

	if (rt->tx_skb[entry].skb)
	{
		printk("ERROR(rlk_inic_start_xmit) !! send count=%d, done_count=%d, entry=%d, sk->len=%d\n",
			   rt->tx0_send_count, rt->tx0_done_count, entry, rt->tx_skb[entry].skb->len);
		ASSERT(rt->tx_skb[entry].skb == NULL);
	}


	if (skb_shinfo(skb)->nr_frags == 0)
	{
		struct rlk_inic_tx_desc *txd = &rt->tx_ring0[entry];
		u32 len;
		dma_addr_t mapping;

		len = skb->len;
		mapping = pci_map_single(rt->pdev, skb->data, len, PCI_DMA_TODEVICE);

		txd->txd_info1.SDP0 = cpu_to_le32(mapping);

#ifdef __BIG_ENDIAN
		txd_info2.SDL0=skb->len;
		*(int *)&txd->txd_info2=cpu_to_le32(*(int *)&txd_info2);
#else
		txd->txd_info2.SDL0 = skb->len;
#endif

		wmb();
#ifdef __BIG_ENDIAN
		txd_info2.DDONE_bit=0;
		*(int *)&txd->txd_info2=cpu_to_le32(*(int *)&txd_info2);
#else
		txd->txd_info2.DDONE_bit = 0;
#endif

		rt->tx_skb[entry].skb = skb;
		rt->tx_skb[entry].mapping = mapping;
		rt->tx_skb[entry].frag = 0;
		wmb();

		rt->tx0_send_count++;
		RLK_INIC_TX_IDX0_INC(rt->tx_cpu_owner_idx0);

		/* update TX_CTX_IDX0 */
		RT_REG_WRITE32_FLUSH(TX_CTX_IDX0, rt->tx_cpu_owner_idx0);
	}
	else
	{
		printk("skb_shinfo(0x%08lx)-nr_frags = %d\n", (uintptr_t)skb, skb_shinfo(skb)->nr_frags);
		rt->net_stats.rx_dropped++;
		dev_kfree_skb(skb); 
	}

	if (netif_msg_tx_queued(rt))
		printk(KERN_DEBUG "%s: tx queued, slot %d, skblen %d\n",
			   dev->name, entry, skb->len);


	if (TX_BUFFS_AVAIL(rt) <= 1)
		netif_stop_queue(dev);

	spin_unlock_irqrestore(&rt->lock, flags);

	dev->trans_start = jiffies;

	return NETDEV_TX_OK;
}

/* 
 * Set or clear the multicast filter for this adaptor.
 * This routine is not state sensitive and need not be SMP locked. 
 */

static void rlk_inic_set_rx_mode (struct net_device *dev)
{
	unsigned long flags;
	iNIC_PRIVATE *rt = netdev_priv(dev);

	spin_lock_irqsave (&rt->lock, flags);
	__rlk_inic_set_rx_mode(dev);
	spin_unlock_irqrestore (&rt->lock, flags);
}

static void __rlk_inic_get_stats(iNIC_PRIVATE *rt)
{
}

static struct net_device_stats *rlk_inic_get_stats(struct net_device *dev)
{
	iNIC_PRIVATE *rt = netdev_priv(dev);
	unsigned long flags;

	/* The chip only need report frame silently dropped. */
	spin_lock_irqsave(&rt->lock, flags);
	if (netif_running(dev) && netif_device_present(dev))
		__rlk_inic_get_stats(rt);
	spin_unlock_irqrestore(&rt->lock, flags);

	return &rt->net_stats;
}

void rlk_inic_stop_hw (iNIC_PRIVATE *rt)
{
	unsigned int    reg;

	reg = RT_REG_READ32(PDMA_GLO_CFG);
	udelay(100);
	reg &= ~(TX_WB_DDONE | RX_DMA_EN | TX_DMA_EN);
	RT_REG_WRITE32(PDMA_GLO_CFG, reg);
	udelay(500);

	rlk_inic_int_disable(rt, 0);

	/* discard all frames */
//	rlk_inic_dma_rx_mode(GDMA_MODE_DISCARD_ALL);
}

static void rlk_inic_reset_hw (iNIC_PRIVATE *rt)
{
	unsigned int reg;
	unsigned int addr0_reg, addr1_reg;
	unsigned char int_reg, latency, cache_line_size;
	unsigned short cmd_reg, status_reg;
#ifdef PCIE_RESET
	unsigned short reset_flr;
#endif
	dma_addr_t ring_dma = rt->ring_dma;

	pci_read_config_word(rt->pdev, PCI_COMMAND, &cmd_reg);
	pci_read_config_word(rt->pdev, PCI_STATUS, &status_reg);
	pci_read_config_dword(rt->pdev, PCI_BASE_ADDRESS_0, &addr0_reg);
	pci_read_config_dword(rt->pdev, PCI_BASE_ADDRESS_1, &addr1_reg);
	pci_read_config_byte(rt->pdev, PCI_INTERRUPT_LINE, &int_reg);
	pci_read_config_byte(rt->pdev, PCI_LATENCY_TIMER, &latency);
	pci_read_config_byte(rt->pdev, PCI_CACHE_LINE_SIZE, &cache_line_size);
	printk("ADDR0 = %08x, ADDR1 = %08x\n", addr0_reg, addr1_reg);
	printk("CMD = %04x, STATUS = %04x\n", cmd_reg, status_reg);
	printk("LATENCY = %02x, CACHE LINE SIZE = %02x\n", latency, cache_line_size);
	printk("INT LINE = %d\n", int_reg);

#ifndef PCI_NONE_RESET
#ifdef PCIE_RESET
	pci_read_config_word(rt->pdev, RESET_FLR, &reset_flr);
	printk("Reset PCIe Card\n");
	reset_flr |= (1<<15);
	pci_write_config_word(rt->pdev, RESET_FLR, reset_flr);
#else
	printk("Reset PCI Card\n");
	RT_REG_WRITE32(RESET_CARD, 0x1);
#endif
#endif

/* 200ms would BUS error, if in RestartiNIC() */
	mdelay(100); //unit: minisec
	pci_write_config_word(rt->pdev, PCI_COMMAND, cmd_reg);
	pci_write_config_word(rt->pdev, PCI_STATUS, status_reg);
	pci_write_config_dword(rt->pdev, PCI_BASE_ADDRESS_0, addr0_reg);
	pci_write_config_dword(rt->pdev, PCI_BASE_ADDRESS_1, addr1_reg);

	pci_write_config_byte(rt->pdev, PCI_INTERRUPT_LINE, int_reg);
	pci_write_config_byte(rt->pdev, PCI_LATENCY_TIMER, 0x20);
	pci_write_config_byte(rt->pdev, PCI_CACHE_LINE_SIZE, cache_line_size);

	pci_read_config_dword(rt->pdev, PCI_BASE_ADDRESS_0, &addr0_reg);
	pci_read_config_dword(rt->pdev, PCI_BASE_ADDRESS_1, &addr1_reg);
	printk("ADDR0 = %08x, ADDR1 = %08x\n", addr0_reg, addr1_reg);
	printk("CMD = %04x, STATUS = %04x\n", cmd_reg, status_reg);
	printk("INT LINE = %d\n", int_reg);

	pci_read_config_byte(rt->pdev, PCI_LATENCY_TIMER, &latency);
	pci_read_config_byte(rt->pdev, PCI_CACHE_LINE_SIZE, &cache_line_size);
	printk("### LATENCY = %02x, CACHE LINE SIZE = %02x\n", latency, cache_line_size);

	reg = RT_REG_READ32(PDMA_RST_IDX);
	printk(" PDMA_RST_IDX = %x\n", reg); 
	RT_REG_WRITE32(PDMA_RST_IDX, reg);
	udelay(100);

	reg = RT_REG_READ32(PDMA_GLO_CFG);
	reg &= ~(TX_WB_DDONE | RX_DMA_EN  | TX_DMA_EN);
	RT_REG_WRITE32(PDMA_GLO_CFG, reg);
	printk("PDMA_GLO_CFG = %08x\n", RT_REG_READ32(PDMA_GLO_CFG));
	udelay(100);


	/* tell the adapter where the TX/RX rings are located */
	RT_REG_WRITE32(RX_BASE_PTR0, (ring_dma & 0xffffffff));
	ring_dma += sizeof(struct rlk_inic_rx_desc) * RLK_INIC_RX_RING_SIZE;
	RT_REG_WRITE32(TX_BASE_PTR0, (ring_dma & 0xffffffff));

	RT_REG_WRITE32(TX_CTX_IDX0, 0);
	RT_REG_WRITE32(RX_CALC_IDX0, (RLK_INIC_RX_RING_SIZE-1));

	/* set the number of tx and rx descriptors to hardware */
	RT_REG_WRITE32(RX_MAX_CNT0, RLK_INIC_RX_RING_SIZE);
	RT_REG_WRITE32(TX_MAX_CNT0, RLK_INIC_TX_RING_SIZE);

	/* enable delay tx : (RLK_INIC_TX_RING_SIZE/2) pended int or 0xFF x 20us */
	RT_REG_WRITE32(DELAY_INT_CFG, (1<<31 | (RLK_INIC_TX_RING_SIZE/2) << 24 | 0xFF<<16));

	printk("TX_MAX_CNT0 = %08x\n", RT_REG_READ32(TX_MAX_CNT0));


	rt->rx_dma_owner_idx0 = 0;
	rt->rx_wants_alloc_idx0 = (RLK_INIC_RX_RING_SIZE-1);

	rt->tx_cpu_owner_idx0 = 0;
	rt->tx_cpu_owner_idx1 = 0;

	rt->tx0_done_count = 0;
	rt->tx0_send_count = 0;
	rt->tx_cpu_owner_idx0 = 0;
}

static inline void rlk_inic_start_hw (iNIC_PRIVATE *rt)
{
	unsigned int reg;

	reg = RT_REG_READ32(PDMA_GLO_CFG);
	printk("2. reg = %08x\n", reg);
	reg |= TX_WB_DDONE | RX_DMA_EN | TX_DMA_EN;
	RT_REG_WRITE32(PDMA_GLO_CFG, reg);
	printk("2. PDMA_GLO_CFG = %08x, reg = %08x\n", RT_REG_READ32(PDMA_GLO_CFG), reg);
	udelay(100);
}

void rlk_inic_init_hw (iNIC_PRIVATE *rt)
{
	struct net_device *dev = rt->dev;

	pci_write_config_word(rt->pdev, PCI_LATENCY_TIMER, 0x80);


	rlk_inic_reset_hw(rt);

	__rlk_inic_set_rx_mode(dev);

	rlk_inic_start_hw(rt);
}

static int rlk_inic_refill_rx (iNIC_PRIVATE *rt)
{
	unsigned i;
#ifdef __BIG_ENDIAN
	PDMA_RXD_INFO2_T rxd_info2;
#endif

	for (i = 0; i < RLK_INIC_RX_RING_SIZE; i++)
	{
		struct sk_buff *skb;

#ifdef PCI_FORCE_DMA
		skb = __dev_alloc_skb(rt->rx_buf_sz + RX_OFFSET, GFP_ATOMIC|GFP_DMA);
#else
		skb = dev_alloc_skb(rt->rx_buf_sz + RX_OFFSET);
#endif
		if (!skb)
		{
			printk("rlk_inic_refill_rx error: can't alloc skb(%d bytes)!\n", rt->rx_buf_sz + RX_OFFSET);
			goto err_out;
		}

		skb->dev = rt->dev;
		skb_reserve(skb, RX_OFFSET);

		rt->rx_skb[i].mapping = pci_map_single(rt->pdev,
											   skb->tail, rt->rx_buf_sz, PCI_DMA_FROMDEVICE);
		rt->rx_skb[i].skb = skb;
		rt->rx_skb[i].frag = 0;

		/* clear rx_ring */
		memset((void *)&rt->rx_ring[i], 0, sizeof(struct rlk_inic_rx_desc));

#ifdef __BIG_ENDIAN
		*(int *)&rxd_info2=cpu_to_le32(*(int *)&rt->rx_ring[i].rxd_info2);
		rxd_info2.DDONE_bit = 0;
		rxd_info2.LS0 = 1;
		*(int *)&rt->rx_ring[i].rxd_info2=cpu_to_le32(*(int *)&rxd_info2);
#else
		rt->rx_ring[i].rxd_info2.DDONE_bit = 0;
		rt->rx_ring[i].rxd_info2.LS0 = 1;
#endif
		rt->rx_ring[i].rxd_info1.PDP0 = cpu_to_le32(rt->rx_skb[i].mapping);
	}

	return 0;

	err_out:
	rlk_inic_clean_rings(rt);
	return -ENOMEM;
}

static int rlk_inic_init_rings (iNIC_PRIVATE *rt)
{
	int i;

#ifdef __BIG_ENDIAN
	PDMA_TXD_INFO2_T txd_info2;
	PDMA_TXD_INFO4_T txd_info4;

	memset(&txd_info2, 0, sizeof(txd_info2));
	memset(&txd_info4, 0, sizeof(txd_info4));

	txd_info2.LS0_bit = 1;
	txd_info2.DDONE_bit = 1;
	txd_info4.PN = 1;
	txd_info4.QN = 0;
#endif

	memset(rt->tx_ring0, 0, sizeof(struct rlk_inic_tx_desc) * RLK_INIC_TX_RING_SIZE);
	memset(&rt->rx_skb, 0, sizeof(struct ring_info) * RLK_INIC_RX_RING_SIZE);
	memset(&rt->tx_skb, 0, sizeof(struct ring_info) * RLK_INIC_TX_RING_SIZE);


	for (i = 0; i < RLK_INIC_TX_RING_SIZE; i++)
	{
#ifdef __BIG_ENDIAN
		*(int *)&rt->tx_ring0[i].txd_info2=cpu_to_le32(*(int *)&txd_info2);
		*(int *)&rt->tx_ring0[i].txd_info4=cpu_to_le32(*(int *)&txd_info4);
#else
		rt->tx_ring0[i].txd_info2.LS0_bit = 1;
		rt->tx_ring0[i].txd_info2.DDONE_bit = 1;
		rt->tx_ring0[i].txd_info4.PN = 1;
		rt->tx_ring0[i].txd_info4.QN = 0;
#endif

	}

	rt->tx0_done_count = 0;
	rt->tx0_send_count = 0;
	rt->tx_cpu_owner_idx0 = 0;

	return rlk_inic_refill_rx (rt);
}

static int rlk_inic_alloc_rings (iNIC_PRIVATE *rt)
{
	void *mem;

	mem = pci_alloc_consistent(rt->pdev, RLK_INIC_RING_BYTES, &rt->ring_dma);
	if (!mem)
		return -ENOMEM;

	rt->rx_ring = mem;

	rt->tx_ring0 = (struct rlk_inic_tx_desc *) &rt->rx_ring[RLK_INIC_RX_RING_SIZE];

	mem += (RLK_INIC_RING_BYTES - RLK_INIC_STATS_SIZE);
	rt->nic_stats = mem;
	rt->nic_stats_dma = rt->ring_dma + (RLK_INIC_RING_BYTES - RLK_INIC_STATS_SIZE);

	return rlk_inic_init_rings(rt);
}

static void rlk_inic_clean_rings (iNIC_PRIVATE *rt)
{
	unsigned i;

	memset(rt->rx_ring, 0, sizeof(struct rlk_inic_rx_desc) * RLK_INIC_RX_RING_SIZE);
	memset(rt->tx_ring0, 0, sizeof(struct rlk_inic_tx_desc) * RLK_INIC_TX_RING_SIZE);

	for (i = 0; i < RLK_INIC_RX_RING_SIZE; i++)
	{
		if (rt->rx_skb[i].skb)
		{
			pci_unmap_single(rt->pdev, rt->rx_skb[i].mapping,
							 rt->rx_buf_sz, PCI_DMA_FROMDEVICE);
			dev_kfree_skb(rt->rx_skb[i].skb);
		}
	}

	for (i = 0; i < RLK_INIC_TX_RING_SIZE; i++)
	{
		if (rt->tx_skb[i].skb)
		{
			struct sk_buff *skb = rt->tx_skb[i].skb;
			pci_unmap_single(rt->pdev, rt->tx_skb[i].mapping,
							 skb->len, PCI_DMA_TODEVICE);
			dev_kfree_skb(skb);
			rt->net_stats.tx_dropped++;
		}
	}

	memset(&rt->rx_skb, 0, sizeof(struct ring_info) * RLK_INIC_RX_RING_SIZE);
	memset(&rt->tx_skb, 0, sizeof(struct ring_info) * RLK_INIC_TX_RING_SIZE);

	rt->tx0_done_count = 0;
	rt->tx0_send_count = 0;
	rt->tx_cpu_owner_idx0 = 0;
}

static void rlk_inic_free_rings (iNIC_PRIVATE *rt)
{
	if (!rt->rx_ring || !rt->tx_ring0 || !rt->nic_stats)
		return;

	rlk_inic_clean_rings(rt);
	pci_free_consistent(rt->pdev, RLK_INIC_RING_BYTES, rt->rx_ring, rt->ring_dma);
	rt->rx_ring = NULL;
	rt->tx_ring0 = NULL;
	rt->nic_stats = NULL;
}

int rlk_inic_open (struct net_device *dev)
{
	iNIC_PRIVATE *rt = netdev_priv(dev);
	int rc;

	printk("iNIC Open %s\n", dev->name);

	// to start racfg_frame_handle()
	RaCfgInterfaceOpen(rt);

	if (netif_msg_ifup(rt))
		printk(KERN_DEBUG "%s: enabling interface\n", dev->name);

#ifndef NM_SUPPORT
	rc = rlk_inic_alloc_rings(rt);
	if (rc)
		return rc;

	rlk_inic_init_hw(rt);
	rt->int_enable_reg = INT_ENABLE_ALL;
	rt->int_disable_mask = 0;
	rc = request_irq(dev->irq, rlk_inic_interrupt, SA_SHIRQ, dev->name, dev);
	if (rc)
		goto err_out_hw;
	netif_carrier_on(dev);
	netif_start_queue(dev);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)
	napi_enable(&rt->napi);
#endif
	rlk_inic_int_enable(rt, rt->int_enable_reg);

	RaCfgSetUp(rt, dev);
#endif


#ifdef IKANOS_VX_1X0
	VR_IKANOS_FP_Init(rt, rlk_inic_send_packet); 
#endif

	return 0;
#ifndef NM_SUPPORT
	err_out_hw:
	rlk_inic_stop_hw(rt);
	rlk_inic_free_rings(rt);
#endif
	return rc;
}


int rlk_inic_close (struct net_device *dev)
{
	iNIC_PRIVATE *rt = netdev_priv(dev);
#ifndef NM_SUPPORT
	unsigned long flags;
#endif
	DBGPRINT("iNIC Closing\n");

	rt->RaCfgObj.bExtEEPROM = FALSE;

#ifndef NM_SUPPORT
	spin_lock_irqsave(&rt->lock, flags);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)
	napi_disable(&rt->napi);
#endif

	if (netif_msg_ifdown(rt))
		DBGPRINT("%s: disabling interface\n", dev->name);
	netif_stop_queue(dev);
	netif_carrier_off(dev);
	rlk_inic_stop_hw(rt);
	spin_unlock_irqrestore(&rt->lock, flags);
	//synchronize_irq(dev->irq);
	free_irq(dev->irq, dev);
	rlk_inic_free_rings(rt);

	RaCfgStateReset(rt);
#endif
	DBGPRINT("iNIC Closed\n");

	RaCfgInterfaceClose(rt);

	return 0;
}

static int rlk_inic_init_one (struct pci_dev *pdev, const struct pci_device_id *ent)
{
	int i;
	char name[8];
	struct net_device *device;
	struct net_device *dev;
	iNIC_PRIVATE *rt;
	int rc;
	void __iomem *regs;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
	long pciaddr;
#endif
	unsigned int pci_using_dac;
	u8 pci_rev;
	char *print_name;

#ifndef MODULE
	static int version_printed;
	if (version_printed++ == 0)
		printk("%s", version);
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
	print_name = pdev ? (char *)pci_name(pdev) : DRV_NAME;
#else
	print_name = pdev ? (char *)pdev->slot_name : DRV_NAME;
#endif

	pci_read_config_byte(pdev, PCI_REVISION_ID, &pci_rev);

	printk(KERN_INFO PFX "pci dev %s (id %04x:%04x rev %02x)\n",
		   print_name, pdev->vendor, pdev->device, pci_rev);

	dev = alloc_etherdev(sizeof(iNIC_PRIVATE));

	// reserve 4 octets for vlan tag
	//dev->hard_header_len += 4;

	if (!dev)
	{
		printk("Can't alloc net device!\n");
		return -ENOMEM;
	}
	SET_MODULE_OWNER(dev);
	SET_NETDEV_DEV(dev, &pdev->dev);

	rt = netdev_priv(dev);

	rt->pdev = pdev;
	rt->dev = dev;
	rt->msg_enable = (debug < 0 ? RLK_INIC_DEF_MSG_ENABLE : debug);

	// reset all structure of RaCfgObj 
	memset(&rt->RaCfgObj, 0, sizeof(RACFG_OBJECT));


	spin_lock_init (&rt->lock);
	rlk_inic_set_rxbufsize(rt);

	rc = pci_enable_device(pdev);
	if (rc)
		goto err_out_free;

	rc = pci_set_mwi(pdev);
	if (rc)
		goto err_out_disable;

	rc = pci_request_regions(pdev, DRV_NAME);
	if (rc)
		goto err_out_mwi;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
	pciaddr = pci_resource_start(pdev, 1);
	if (!pciaddr)
	{
		rc = -EIO;
		printk(KERN_ERR PFX "no MMIO resource for pci dev %s\n",
			   print_name);
		goto err_out_res;
	}
	if (pci_resource_len(pdev, 1) < RLK_INIC_REGS_SIZE)
	{
		rc = -EIO;
		printk(KERN_ERR PFX "MMIO resource (%lx) too small on pci dev %s\n",
			   (unsigned long) pci_resource_len(pdev, 1), print_name);
		goto err_out_res;
	}
#endif
	/* Configure DMA attributes. */
	if ((sizeof(dma_addr_t) > 4) &&
		!PCI_SET_CONSISTENT_DMA_MASK(pdev, 0xffffffffffffffffULL) &&
		!pci_set_dma_mask(pdev, 0xffffffffffffffffULL))
	{
		pci_using_dac = 1;
	}
	else
	{
		pci_using_dac = 0;

		rc = pci_set_dma_mask(pdev, 0xffffffffULL);
		if (rc)
		{
			printk(KERN_ERR PFX "No usable DMA configuration, "
				   "aborting.\n");
			goto err_out_res;
		}
		rc = PCI_SET_CONSISTENT_DMA_MASK(pdev, 0xffffffffULL);
		if (rc)
		{
			printk(KERN_ERR PFX "No usable consistent DMA configuration, "
				   "aborting.\n");
			goto err_out_res;
		}
	}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
	regs = pci_iomap(pdev, 1, RLK_INIC_REGS_SIZE);
	if (!regs)
	{
		rc = -EIO;
		printk(KERN_ERR PFX "Cannot map PCI MMIO (%lx) on pci dev %s\n",
			   (unsigned long)pci_resource_len(pdev, 1), print_name);
		goto err_out_res;
	}

#else
	regs = ioremap(pciaddr, RLK_INIC_REGS_SIZE);
	if (!regs)
	{
		rc = -EIO;
		printk(KERN_ERR PFX "Cannot map PCI MMIO (%lx@%lx) on pci dev %s\n",
			   (unsigned long)pci_resource_len(pdev, 1), pciaddr, print_name);
		goto err_out_res;
	}
#endif
	dev->base_addr = (unsigned long) regs;
	rt->regs = regs;
	printk("rt->regs = %p\n", regs);

	rlk_inic_stop_hw(rt);

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,28)
	dev->open               = rlk_inic_open;
	dev->stop               = rlk_inic_close;
	dev->set_multicast_list = rlk_inic_set_rx_mode;
#ifdef IKANOS_VX_1X0
	dev->hard_start_xmit    = IKANOS_DataFramesTx;
#else
	dev->hard_start_xmit    = rlk_inic_send_packet;
#endif

	dev->get_stats          = rlk_inic_get_stats;
	dev->do_ioctl           = rlk_inic_ioctl;
#else
	dev->netdev_ops = &netdev_ops;
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
	dev->poll               = rlk_inic_rx_poll;
	dev->weight             = 64;	/* arbitrary? from NAPI_HOWTO.txt. */
#else
	memset(&rt->napi, 0, sizeof(struct napi_struct));
	netif_napi_add(dev, &rt->napi, rlk_inic_rx_poll, 64);
#endif

#if 0
	dev->tx_timeout = rlk_inic_tx_timeout;
	dev->watchdog_timeo = TX_TIMEOUT;
#endif

	if (pci_using_dac)
		dev->features |= NETIF_F_HIGHDMA;

	if (csumoff)
	{
		dev->features |= NETIF_F_IP_CSUM;
	}

	dev->irq = pdev->irq;


#ifdef MULTIPLE_CARD_SUPPORT
	// find its profile path
	CardInfoRead(rt);   
#endif // MULTIPLE_CARD_SUPPORT //


	for (i = 0; i <32; i++)
	{
#ifdef MULTIPLE_CARD_SUPPORT
		if (rt->RaCfgObj.InterfaceNumber >= 0)
			snprintf(name, sizeof(name), "ra%02d_%d", rt->RaCfgObj.InterfaceNumber, i);
		else
#endif // MULTIPLE_CARD_SUPPORT //
			snprintf(name, sizeof(name), "%s%d", INIC_INFNAME, i);

		device = DEV_GET_BY_NAME(name);        
		if (device == NULL)	 break;
		dev_put(device);
	}
	if (i == 32)
	{
		printk(KERN_ERR "No available dev name\n");
		goto err_out_iomap;
	}

#ifdef MULTIPLE_CARD_SUPPORT
	if (rt->RaCfgObj.InterfaceNumber >= 0)
		snprintf(dev->name, sizeof(dev->name), "ra%02d_%d", rt->RaCfgObj.InterfaceNumber, i);
	else
#endif // MULTIPLE_CARD_SUPPORT //
		snprintf(dev->name, sizeof(dev->name), "%s%d", INIC_INFNAME, i);

	// Be sure to init racfg before register_netdev,Otherwise 
	// network manager cannot identify ra0 as wireless device


#ifdef MULTIPLE_CARD_SUPPORT
	if (rt->RaCfgObj.InterfaceNumber >= 0)
		RaCfgInit(rt, dev, rt->RaCfgObj.MC_MAC , mode, bridge, csumoff);
	else
#endif // MULTIPLE_CARD_SUPPORT //
		RaCfgInit(rt, dev, mac, mode, bridge, csumoff);

	rc = register_netdev(dev);
	if (rc)
		goto err_out_iomap;

	DBGPRINT("%s: Ralink iNIC at 0x%lx, "
			 "%02x:%02x:%02x:%02x:%02x:%02x, "
			 "IRQ %d\n",
			 dev->name,
			 dev->base_addr,
			 dev->dev_addr[0], dev->dev_addr[1],
			 dev->dev_addr[2], dev->dev_addr[3],
			 dev->dev_addr[4], dev->dev_addr[5],
			 dev->irq);

	pci_set_drvdata(pdev, dev);
	/* enable busmastering and memory-write-invalidate */
	pci_set_master(pdev);
	dev->priv_flags         = INT_MAIN;

#ifdef NM_SUPPORT
	rc = rlk_inic_alloc_rings(rt);
	if (rc)
		return rc;

	rlk_inic_init_hw(rt);
	rt->int_enable_reg = INT_ENABLE_ALL;
	rt->int_disable_mask = 0;
	rc = request_irq(dev->irq, rlk_inic_interrupt, SA_SHIRQ, dev->name, dev);
	if (rc)
		goto err_out_hw;
	netif_carrier_on(dev);
	netif_start_queue(dev);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)
	napi_enable(&rt->napi);
#endif
	rlk_inic_int_enable(rt, rt->int_enable_reg);
	rtnl_unlock();
	dev_change_flags(dev, dev->flags | IFF_UP);
	rtnl_lock();

	RaCfgSetUp(rt, dev);
#endif

	return 0;

	err_out_iomap:
	iounmap(regs);
	err_out_res:
	pci_release_regions(pdev);
	err_out_mwi:
	pci_clear_mwi(pdev);
	err_out_disable:
	pci_disable_device(pdev);
	err_out_free:
	free_netdev(dev);
	return rc;

#ifdef NM_SUPPORT
	err_out_hw:
	rlk_inic_stop_hw(rt);
	rlk_inic_free_rings(rt);
	return rc;
#endif

}

static void rlk_inic_remove_one (struct pci_dev *pdev)
{
	struct net_device *dev = pci_get_drvdata(pdev);
	iNIC_PRIVATE *rt = netdev_priv(dev);

#ifdef MULTIPLE_CARD_SUPPORT
	if ((rt->RaCfgObj.InterfaceNumber >= 0) && (rt->RaCfgObj.InterfaceNumber <= MAX_NUM_OF_MULTIPLE_CARD))
		MC_CardUsed[rt->RaCfgObj.InterfaceNumber] = 0;	// not clear irq
#endif // MULTIPLE_CARD_SUPPORT //	

#ifdef NM_SUPPORT
	if (netif_msg_ifdown(rt))
		DBGPRINT("%s: disabling interface\n", dev->name);
	netif_stop_queue(dev);
	netif_carrier_off(dev);
	rlk_inic_stop_hw(rt);
	//synchronize_irq(dev->irq);
	free_irq(dev->irq, dev);
	rlk_inic_free_rings(rt);

	RaCfgStateReset(rt);
#endif
	//sock_release(nl_sk->SK_SOCKET);
	RaCfgExit(rt);

	if (!dev)
		BUG();

	printk("unregister netdev %s\n", dev->name);

	if (rt->RaCfgObj.opmode == 0)
	{
#if IW_HANDLER_VERSION < 7
		dev->get_wireless_stats = NULL;
#endif
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,21) || defined(CONFIG_WIRELESS_EXT)
		dev->wireless_handlers = NULL;
#endif
	}

	unregister_netdev(dev);
	iounmap(rt->regs);
	pci_release_regions(pdev);
	pci_disable_device(pdev);
	pci_set_drvdata(pdev, NULL);

	free_netdev(dev);
}


static struct pci_driver rlk_inic_driver = {
	.name         = DRV_NAME,
	.id_table     = rlk_inic_pci_tbl,
	.probe        = rlk_inic_init_one,
	.remove       = rlk_inic_remove_one,
};

static int __init rlk_inic_init (void)
{
#ifdef MODULE
	printk("%s", version);
#endif

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,21)
	return pci_module_init (&rlk_inic_driver);
#else
	return pci_register_driver(&rlk_inic_driver);
#endif
}

static void __exit rlk_inic_exit (void)
{
	pci_unregister_driver (&rlk_inic_driver);
}

module_init(rlk_inic_init);
module_exit(rlk_inic_exit);
