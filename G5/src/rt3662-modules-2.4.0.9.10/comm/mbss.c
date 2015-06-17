#include "rlk_inic.h"

/*
static void make_mbss_frame(
						   unsigned char **p, 
						   unsigned short CommandType, 
						   unsigned short CommandID, 
						   unsigned short len, 
						   unsigned short CommandSeq,
						   unsigned short devID);
*/
static int MbssHandler(iNIC_PRIVATE *pAd, int cmd, int dev_id);
static int MBSS_VirtualIF_Close(struct net_device *dev_p);
static int MBSS_VirtualIF_Open(struct net_device *dev_p);
static int MBSS_VirtualIF_Ioctl(struct net_device *dev_p, struct ifreq *rq_p, int cmd);
static int MBSS_VirtualIF_PacketSend(struct sk_buff *skb_p, struct net_device *dev_p);

struct net_device_stats *VirtualIF_get_stats(struct net_device *dev);

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,28)
static struct net_device_ops Netdev_Ops[MAX_MBSSID_NUM];
#endif

void rlk_inic_mbss_init (
					  struct net_device *main_dev_p, 
					  iNIC_PRIVATE *ad_p)
{
	struct net_device *cur_dev_p;
	char   slot_name[IFNAMSIZ];
	struct net_device *new_dev_p;
	VIRTUAL_ADAPTER *mbss_ad_p;
	int bss_index, max_bss_num;
	int index = 0;

	if (ad_p->RaCfgObj.flg_mbss_init)
		return;

	ad_p->RaCfgObj.flg_mbss_init = 1;

	printk("%s --->\n", __FUNCTION__);

	/* init */
	max_bss_num = ad_p->RaCfgObj.BssidNum;
	if (max_bss_num > MAX_MBSSID_NUM)
		max_bss_num = MAX_MBSSID_NUM;
	/* End of if */

	/* first bss_index must not be 0 (BSS0), must be 1 (BSS1) */
	for (bss_index = FIRST_MBSSID; bss_index < MAX_MBSSID_NUM; bss_index++)
		ad_p->RaCfgObj.MBSSID[bss_index].MSSIDDev = NULL;
	/* End of for */


	/* create virtual network interface */
	for (bss_index=FIRST_MBSSID; bss_index<max_bss_num; bss_index++)
	{
		/* allocate a new network device */
#if LINUX_VERSION_CODE <= 0x20402 // Red Hat 7.1
		new_dev_p = alloc_netdev(sizeof(VIRTUAL_ADAPTER), "eth%d", ether_setup);
#else
		new_dev_p = alloc_etherdev(sizeof(VIRTUAL_ADAPTER));
#endif // LINUX_VERSION_CODE //
		if (new_dev_p == NULL)
		{
			/* allocation fail, exit */
			ad_p->RaCfgObj.BssidNum = bss_index; /* re-assign new MBSS number */
			printk("Allocate network device fail (MBSS)...\n");
			break;
		} /* End of if */

		/* find an available interface name, max 32 ra interfaces */
		for (index = 0; index < MBSS_MAX_DEV_NUM; index++)
		{

#if defined(MULTIPLE_CARD_SUPPORT) || defined(CONFIG_CONCURRENT_INIC_SUPPORT)
			if (ad_p->RaCfgObj.InterfaceNumber >= 0)
				snprintf(slot_name, sizeof(slot_name), "%s%02d_%d", INIC_INFNAME, ad_p->RaCfgObj.InterfaceNumber, index);
			else
#endif
				snprintf(slot_name, sizeof(slot_name),"%s%d", INIC_INFNAME, index);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
			cur_dev_p = DEV_GET_BY_NAME(slot_name);
#else
			for (cur_dev_p=dev_base; cur_dev_p!=NULL; cur_dev_p=cur_dev_p->next)
			{
				if (strncmp(cur_dev_p->name, slot_name, 6) == 0)
					break;
				/* End of if */
			} /* End of for */
#endif // LINUX_VERSION_CODE //

			if (cur_dev_p == NULL)
				break; /* fine, the RA name is not used */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
			else
			{
				/* every time dev_get_by_name is called, and it has returned a
				   valid struct net_device **, dev_put should be called
				   afterwards, because otherwise the machine hangs when the
				   device is unregistered (since dev->refcnt > 1). */
				dev_put(cur_dev_p);
			} /* End of if */
#endif // LINUX_VERSION_CODE //
		} /* End of for */

		/* assign interface name to the new network interface */
		if (index < MBSS_MAX_DEV_NUM)
		{

#if defined(MULTIPLE_CARD_SUPPORT) || defined(CONFIG_CONCURRENT_INIC_SUPPORT)
			if (ad_p->RaCfgObj.InterfaceNumber >= 0)
				snprintf(new_dev_p->name, sizeof(new_dev_p->name), "%s%02d_%d", INIC_INFNAME, ad_p->RaCfgObj.InterfaceNumber, index);
			else
#endif		
				snprintf(new_dev_p->name, sizeof(new_dev_p->name), "%s%d", INIC_INFNAME, index);
			printk("Register MBSSID IF (%s)\n", new_dev_p->name);
		}
		else
		{
			/* error! no any available ra name can be used! */
			ad_p->RaCfgObj.BssidNum = bss_index; /* re-assign new MBSS number */
			printk("Has %d RA interfaces (MBSS)...\n", MBSS_MAX_DEV_NUM);
			kfree(new_dev_p);
			break;
		} /* End of if */

		/* init the new network interface */
		//ether_setup(new_dev_p);

		new_dev_p->hard_header_len += 4;

		mbss_ad_p = netdev_priv(new_dev_p);
		mbss_ad_p->VirtualDev = new_dev_p;	/* 4B */
		mbss_ad_p->RtmpDev    = main_dev_p;	/* 4B */

		/* init MAC address of virtual network interface */

		memmove(ad_p->RaCfgObj.MBSSID[bss_index].Bssid, 
				main_dev_p->dev_addr, MAC_ADDR_LEN);

#ifdef NEW_MBSS_SUPPORT
#ifdef CONFIG_CONCURRENT_INIC_SUPPORT
		if(ad_p == gAdapter[0])
#else
		if(1)
#endif
		{
			if(bss_index > 0){
				ad_p->RaCfgObj.MBSSID[bss_index].Bssid[0] += 2;
				ad_p->RaCfgObj.MBSSID[bss_index].Bssid[0] += ((bss_index - 1) << 2);
			}
		}
		else
#endif
		ad_p->RaCfgObj.MBSSID[bss_index].Bssid[5] += bss_index;

		memmove(new_dev_p->dev_addr,
				ad_p->RaCfgObj.MBSSID[bss_index].Bssid, MAC_ADDR_LEN);

		/* init operation functions */
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,28)
		new_dev_p->open             = MBSS_VirtualIF_Open;
		new_dev_p->stop             = MBSS_VirtualIF_Close;
		new_dev_p->hard_start_xmit  = MBSS_VirtualIF_PacketSend;
		new_dev_p->do_ioctl         = MBSS_VirtualIF_Ioctl;
		new_dev_p->get_stats        = VirtualIF_get_stats;
		/* if you dont implement get_stats, dont assign your function with empty
		   body; or kernel will panic */
#else
		Netdev_Ops[bss_index].ndo_open		= MBSS_VirtualIF_Open;
		Netdev_Ops[bss_index].ndo_stop		= MBSS_VirtualIF_Close;
		Netdev_Ops[bss_index].ndo_start_xmit		= MBSS_VirtualIF_PacketSend;
		Netdev_Ops[bss_index].ndo_do_ioctl       = MBSS_VirtualIF_Ioctl;
		Netdev_Ops[bss_index].ndo_get_stats      = VirtualIF_get_stats;
		new_dev_p->netdev_ops = (const struct net_device_ops *)&Netdev_Ops[bss_index];	
#endif		
		new_dev_p->priv_flags       = INT_MBSSID; /* we are virtual interface */

		/* register this device to OS */
		register_netdevice(new_dev_p);

		/* backup our virtual network interface */
		ad_p->RaCfgObj.MBSSID[bss_index].MSSIDDev = new_dev_p;
	} /* End of for */

	printk("%s <---\n", __FUNCTION__);
} /* End of RLK_INIC_MBSS_Init */


void rlk_inic_mbss_restart(uintptr_t arg)
{
	iNIC_PRIVATE *pAd = (iNIC_PRIVATE *)arg;
	int dev_id;
	int error;

	for (dev_id=FIRST_MBSSID; dev_id<pAd->RaCfgObj.BssidNum; dev_id++)
	{
		error = 1;

		if (pAd->RaCfgObj.MBSSID[dev_id].flags)
		{
			error = MbssHandler(pAd, RACFG_CMD_MBSS_OPEN, dev_id);
			if (!error)
			{
				printk("ReOpen MBSS %d\n", dev_id);
			}
		}

	}
}

int rlk_inic_mbss_close(iNIC_PRIVATE *ad_p)
{
	int bss_index, error = 1;

	printk("%s --->\n", __FUNCTION__);

	for (bss_index=FIRST_MBSSID; bss_index<MAX_MBSSID_NUM; bss_index++)
	{
		if (ad_p->RaCfgObj.MBSSID[bss_index].MSSIDDev)
		{
			ad_p->RaCfgObj.mbss_close_all = 1;
			dev_close(ad_p->RaCfgObj.MBSSID[bss_index].MSSIDDev);
			ad_p->RaCfgObj.mbss_close_all = 0;
		}
		/* End of if */
	} /* End of for */


	/* send target command to shut the main interface (0) down */
#ifdef CONFIG_CONCURRENT_INIC_SUPPORT
	/* Shut down the iNIC after all interface is down */
	SetRadioOn(ad_p, 0);
	mdelay(500);
	if(ConcurrentObj.CardCount== 0)
#endif // CONFIG_CONCURRENT_INIC_SUPPORT //	
	
	if (NETIF_IS_UP(ad_p->RaCfgObj.MBSSID[0].MSSIDDev))
	{
          printk("RACFG_CMD_MBSS_CLOSE!!!!\n");
//		error = MbssHandler(ad_p, RACFG_CMD_MBSS_CLOSE, 0);
	}
	if (!error)
		printk("all interfaces closed.\n");
	printk("%s <---\n", __FUNCTION__);
	return error;
} /* End of RLK_INIC_MBSS_Close */

void rlk_inic_mbss_remove(iNIC_PRIVATE *ad_p)
{
	int bss_index;


	printk("%s --->\n", __FUNCTION__);

	for (bss_index=FIRST_MBSSID; bss_index<MAX_MBSSID_NUM; bss_index++)
	{
		struct net_device *dev = ad_p->RaCfgObj.MBSSID[bss_index].MSSIDDev;
		if (dev)
		{
			printk("unregister %s\n", dev->name);
			// keep unregister_netdev(ra1) from calling dev_close(ra1), which
			// will be called in the following unregister_netdev(ra0)
			ad_p->RaCfgObj.mbss_close_all = 1;
			unregister_netdev(dev);
			ad_p->RaCfgObj.mbss_close_all = 0;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
			free_netdev(dev);
#else
			kfree(dev);
#endif // LINUX_VERSION_CODE //
			ad_p->RaCfgObj.MBSSID[bss_index].MSSIDDev = NULL;
		} /* End of if */
	} /* End of for */

	ad_p->RaCfgObj.flg_mbss_init = 0;

	printk("%s <---\n", __FUNCTION__);
} /* End of RLK_INIC_MBSS_Remove */

int MBSS_Find_DevID(
				   struct net_device *main_dev, 
				   struct net_device *mbss_dev)
{
	iNIC_PRIVATE *ad_p = NULL;
	int bss_index;

	ad_p = netdev_priv(main_dev);
	//netif_start_queue(virtual_ad_p->VirtualDev);

	for (bss_index=FIRST_MBSSID; bss_index<MAX_MBSSID_NUM; bss_index++)
	{
		if (ad_p->RaCfgObj.MBSSID[bss_index].MSSIDDev == mbss_dev)
			return bss_index;
		/* End of if */
	} /* End of for */
	printk("ERROR! Can't find MBSS dev id: %s\n", mbss_dev->name);
	return 0;

}
static int MBSS_VirtualIF_Open(struct net_device *dev_p)
{
	int dev_id = 0, error = 1;
	VIRTUAL_ADAPTER *virtual_ad_p = netdev_priv(dev_p);
	iNIC_PRIVATE    *pAd;

	ASSERT(virtual_ad_p->VirtualDev);
	ASSERT(virtual_ad_p->RtmpDev);


	//if (!NETIF_IS_UP(virtual_ad_p->RtmpDev))
	if (!NETIF_IS_UP(GET_PARENT(dev_p)))
	{
		printk("%s should be open at first\n", GET_PARENT(dev_p)->name);
		return -EFAULT;
	}


	netif_carrier_on(dev_p);
	netif_start_queue(dev_p);
	//netif_start_queue(virtual_ad_p->VirtualDev);

	dev_id = MBSS_Find_DevID(virtual_ad_p->RtmpDev, dev_p);
	ASSERT(dev_id > 0);
	ASSERT(netdev_priv(virtual_ad_p->RtmpDev));
	pAd = (iNIC_PRIVATE *) netdev_priv(virtual_ad_p->RtmpDev);
	error = MbssHandler(netdev_priv(virtual_ad_p->RtmpDev), RACFG_CMD_MBSS_OPEN, dev_id);
	if (!error)
	{
		pAd->RaCfgObj.MBSSID[dev_id].flags = 1;
		printk("%s: ==>>> MBSS_VirtualIF_Open\n", dev_p->name);
	}


	return error;
} /* End of MBSS_VirtualIF_Open */

static int MBSS_VirtualIF_Close(struct net_device *dev_p)
{
	int dev_id = 0, error = 1;
	VIRTUAL_ADAPTER *virtual_ad_p = netdev_priv(dev_p);
	iNIC_PRIVATE    *pAd;

	ASSERT(virtual_ad_p->VirtualDev);
	ASSERT(virtual_ad_p->RtmpDev);

	//if (!NETIF_IS_UP(virtual_ad_p->RtmpDev))
	if (!NETIF_IS_UP(GET_PARENT(dev_p)))
	{
		printk("%s should be open at first\n", GET_PARENT(dev_p)->name);
		return -EFAULT;
	}

	if (((iNIC_PRIVATE *)netdev_priv(virtual_ad_p->RtmpDev))->RaCfgObj.mbss_close_all)
		return 0;


	netif_stop_queue(dev_p);
	netif_carrier_off(dev_p);

	dev_id = MBSS_Find_DevID(virtual_ad_p->RtmpDev, dev_p);
	ASSERT(dev_id > 0);
	ASSERT(netdev_priv(virtual_ad_p->RtmpDev));
	pAd = (iNIC_PRIVATE *) netdev_priv(virtual_ad_p->RtmpDev);
	error = MbssHandler(netdev_priv(virtual_ad_p->RtmpDev), RACFG_CMD_MBSS_CLOSE, dev_id);
	if (!error)
	{
		printk("%s: ==>>> MBSS_VirtualIF_Close\n", dev_p->name);
		pAd->RaCfgObj.MBSSID[dev_id].flags = 0;
	}

	return error;
} /* End of MBSS_VirtualIF_Close */

static int MBSS_VirtualIF_Ioctl(
							   struct net_device *dev_p, 
							   struct ifreq      *rq_p, 
							   int                cmd)
{
	VIRTUAL_ADAPTER *virtual_ad_p;
	struct net_device *main_dev_p; 
	iNIC_PRIVATE *ad_p;

	//if (!NETIF_IS_UP(dev_p))
	if (!NETIF_IS_UP(GET_PARENT(dev_p)))
		return -ENETDOWN;
	/* sanity check */
	virtual_ad_p = netdev_priv(dev_p);
	ASSERT(virtual_ad_p);
	main_dev_p = virtual_ad_p->RtmpDev;
	ASSERT(main_dev_p);
	ad_p = netdev_priv(virtual_ad_p->RtmpDev);
	ASSERT(ad_p);

#if 0
	#define fRTMP_ADAPTER_INTERRUPT_IN_USE      0x00000002
	#define RTMP_TEST_FLAG(_M, _F)      (((_M)->Flags & (_F)) != 0)
	if (!RTMP_TEST_FLAG(ad_p, fRTMP_ADAPTER_INTERRUPT_IN_USE))
		return -ENETDOWN;
#endif

	/* do real IOCTL */
	return rlk_inic_ioctl(dev_p, rq_p, cmd);
} /* End of MBSS_VirtualIF_Ioctl */


void insert_vlan_id(
				   struct sk_buff  *skb,
				   int             vlan_id)
{
	unsigned char   TPID[2] = {0x81, 0x00};
	struct vlan_tic TCI;
	u16     *pTCI = (u16 *) &TCI;


	TCI.vid = vlan_id;
	TCI.priority = 0;
	TCI.cfi = 0;

	/* copy VLAN tag (4B) */
	/* do NOT use memcpy to speed up */
	*((u16 *)(skb->data+12)) = *(u16 *) TPID;
	*((u16 *)(skb->data+12+2)) = htons(*pTCI);
} 


struct net_device_stats *VirtualIF_get_stats(struct net_device *dev)
{
	VIRTUAL_ADAPTER *virtual_ad_p = netdev_priv(dev);

	return &virtual_ad_p->net_stats;
}


/*
   ========================================================================
   Routine Description:
	   Dummy routine to avoid kernel crash
	========================================================================
*/
static int MBSS_VirtualIF_PacketSend(
									struct sk_buff  *skb_p,
									struct net_device *dev_p)
{
	int dev_id = 0;
	VIRTUAL_ADAPTER *virtual_ad_p = netdev_priv(dev_p);
	iNIC_PRIVATE *rt;


	ASSERT(skb_p);
	ASSERT(virtual_ad_p->VirtualDev);
	ASSERT(virtual_ad_p->RtmpDev);
	if (RaCfgDropRemoteInBandFrame(skb_p))
	{
		return NETDEV_TX_OK;
	}

	dev_id = MBSS_Find_DevID(virtual_ad_p->RtmpDev, dev_p);
	ASSERT(dev_id > 0);
	rt = (iNIC_PRIVATE *) netdev_priv(virtual_ad_p->RtmpDev);

	if (rt->RaCfgObj.bBridge == 1)
	{
		// built in bridge enabled,all packets sent by main device,
		// virtual device should drop all the packets(may flooded by bridge) 
		// ==> (X) rlk_inic_start_xmit(skb, dev);
		virtual_ad_p->net_stats.tx_dropped++;
		dev_kfree_skb(skb_p);
		//hex_dump("======> Drop MBSS send:", skb_p->data, skb_p->len);
		return NETDEV_TX_OK;

	}
	else
	{
		
#ifdef IKANOS_VX_1X0		
		IkanosWlanTxCbFuncP *fp = &IKANOS_WlanDataFramesTx;

		skb_p->apFlowData.txDev = dev_p;
		skb_p->apFlowData.txApId = IKANOS_PERAP_ID;
		rt->RaCfgObj.IkanosTxInfo.netdev = dev_p;
		rt->RaCfgObj.IkanosTxInfo.fp = fp;
		skb_p->apFlowData.txHandle = &(rt->RaCfgObj.IkanosTxInfo);
		ap2apFlowProcess(skb_p, dev_p);
#endif		
		
		skb_p = Insert_Vlan_Tag(rt, skb_p, dev_id, SOURCE_MBSS);
		
		
		if (skb_p)
		{
			virtual_ad_p->net_stats.tx_packets++;
			virtual_ad_p->net_stats.tx_bytes += skb_p->len;

			return rlk_inic_start_xmit(skb_p, virtual_ad_p->RtmpDev); 
		}
		else
		{
			virtual_ad_p->net_stats.tx_dropped++;
		}
		//printk("send packet on %s\n", dev_p->name);
	}
	return NETDEV_TX_OK;
} /* End of MBSS_VirtualIF_PacketSend */


struct sk_buff*  Insert_Vlan_Tag(
								iNIC_PRIVATE *rt,
								struct sk_buff  *skb_p,
								int dev_id,
								SOURCE_TYPE dev_source)
{
	int source_id = 1;
	char *ifname = ""; // for debug

	if (rt->RaCfgObj.opmode == 0)
	{
		// STA needn't insert vlan tag, no matter 
		// that bridge is disabled or not.
		return skb_p;
	}

	switch (dev_source)
	{
	case SOURCE_MBSS:
		source_id = dev_id + MIN_NET_DEVICE_FOR_MBSSID + 1;
		ifname="ra";
		break;
	case SOURCE_WDS:
		source_id = dev_id + MIN_NET_DEVICE_FOR_WDS + 1;
		ifname="wds";
		break;
	case SOURCE_APCLI:
		source_id = dev_id + MIN_NET_DEVICE_FOR_APCLI + 1;
		ifname="apcli";
		break;
#ifdef MESH_SUPPORT
	case SOURCE_MESH:
		source_id = dev_id + MIN_NET_DEVICE_FOR_MESH + 1;
		ifname="mesh";
		break;                  
#endif // MESH_SUPPORT //
	default:
		printk("ERROR! Invalid device source id=%d\n", dev_source);
		break;
	}

	if (skb_cloned(skb_p) || skb_shared(skb_p))
	{
		struct sk_buff *nskb;

		nskb = pskb_copy(skb_p, GFP_ATOMIC);
		dev_kfree_skb(skb_p);
		if (!nskb)
			return NULL;
		skb_p = nskb;
	}


	// disable built-in bridge engine
	// use vlan id to distinguish multiple bssid 
	if (dev_source == SOURCE_MBSS && 
		(*((u16 *)(&skb_p->data[12])) == htons(0x8100)) && 
		(rt->RaCfgObj.MBSSID[dev_id].bVLAN_tag == 0))
	{
		struct vlan_tic *pTIC, TCI;
		u16 *p, value;

		value = ntohs(*((u16 *)(&skb_p->data[14])));
		pTIC = (struct vlan_tic *) &value;
		TCI.priority = pTIC->priority;
		TCI.cfi = pTIC->cfi;
		TCI.vid = source_id; 
		p = (u16 *) &TCI;
		*(u16 *)(skb_p->data+14) = htons(*p);
	}
	else
	{
		//
		// insert vlan tag into packet
		// need 4 octets extra-room in packet buffer
		// 
		if (skb_headroom(skb_p) >= 4)
		{
			//printk("have enough head room\n");
			memmove(skb_p->data-4, skb_p->data, 12);
			skb_p->data -= 4;
			skb_p->len += 4;
			insert_vlan_id(skb_p, source_id);
		}
		else
		{
			struct sk_buff *skb2;

			if (net_ratelimit())
			{
				printk("!!! please double NET_SKB_PAD in skbuff.h to expand head room and rebuild kernel !!!\n");
			}
			skb2 = skb_realloc_headroom(skb_p, 4);

			if (skb2)
			{
				dev_kfree_skb(skb_p);

				skb_p = skb2;
				memmove(skb_p->data-4, skb_p->data, 12);
				skb_p->data -= 4;
				skb_p->len += 4;
				insert_vlan_id(skb_p, source_id);
			}
			else
			{
				dev_kfree_skb(skb_p);
				if (net_ratelimit()) printk("%s%d: skb_realloc_headroom failed\n", ifname, dev_id);
				skb_p = NULL;
			}
		}
	}
	return skb_p;
}


unsigned char Remove_Vlan_Tag(
					iNIC_PRIVATE *rt,
					struct sk_buff  *skb)
{
	int dev_id = 0;
	
	// default hard header len 
	SET_SKB_HARD_HEADER_LEN(skb, ETH_HLEN);

	if (*((u16 *)(&skb->data[12])) != htons(0xFFFF))
	{
		// not use built-in bridge engine
		if (rt->RaCfgObj.bBridge == 0 && rt->RaCfgObj.opmode == 1)
		{
			if (*((u16 *)(&skb->data[12])) == htons(0x8100))
			{
				struct vlan_tic *pTIC, TCI;
				u16 *p, value;
								
				int bMBSS = 0;


				value = ntohs(*((u16 *)(skb->data+14)));
				pTIC = (struct vlan_tic *) &value;

				// get interface index from vlan_id
				dev_id = pTIC->vid-1;

				//printk("Remove_Vlan_Tag(Rx): dev_id = %d\n", dev_id);

				if (dev_id >= MIN_NET_DEVICE_FOR_MBSSID && 
					dev_id <  MIN_NET_DEVICE_FOR_MBSSID + MAX_MBSSID_NUM)
				{
					dev_id -= MIN_NET_DEVICE_FOR_MBSSID;
					// identify packet which bssid it come from
					skb->dev = rt->RaCfgObj.MBSSID[dev_id].MSSIDDev;
					if (dev_id == 0)
					{
						rt->net_stats.rx_packets++;
						rt->net_stats.rx_bytes += skb->len;
					}
					else
					{
						VIRTUAL_ADAPTER *virtual_ad_p = netdev_priv(skb->dev);

						virtual_ad_p->net_stats.rx_packets++;
						virtual_ad_p->net_stats.rx_bytes += skb->len;
					}
					ASSERT(skb->dev);
					bMBSS = 1;
				}
				else if (dev_id >= MIN_NET_DEVICE_FOR_WDS && 
						 dev_id <  MIN_NET_DEVICE_FOR_WDS + MAX_WDS_NUM)
				{
					dev_id -= MIN_NET_DEVICE_FOR_WDS;
					// identify packet which bssid it come from
					skb->dev = rt->RaCfgObj.WDS[dev_id].MSSIDDev;
					{
						VIRTUAL_ADAPTER *virtual_ad_p = netdev_priv(skb->dev);

						virtual_ad_p->net_stats.rx_packets++;
						virtual_ad_p->net_stats.rx_bytes += skb->len;
					}
					ASSERT(skb->dev);
				}
				else if (dev_id >= MIN_NET_DEVICE_FOR_APCLI && 
						 dev_id <  MIN_NET_DEVICE_FOR_APCLI + MAX_APCLI_NUM)
				{
					dev_id -= MIN_NET_DEVICE_FOR_APCLI;
					// identify packet which bssid it come from
					skb->dev = rt->RaCfgObj.APCLI[dev_id].MSSIDDev;
					{
						VIRTUAL_ADAPTER *virtual_ad_p = netdev_priv(skb->dev);

						virtual_ad_p->net_stats.rx_packets++;
						virtual_ad_p->net_stats.rx_bytes += skb->len;
					}
					ASSERT(skb->dev);
				}
#ifdef MESH_SUPPORT
				else if (dev_id >= MIN_NET_DEVICE_FOR_MESH && 
						 dev_id <  MIN_NET_DEVICE_FOR_MESH + MAX_MESH_NUM)
				{
					dev_id -= MIN_NET_DEVICE_FOR_MESH;
					// identify packet which bssid it come from
					skb->dev = rt->RaCfgObj.MESH[dev_id].MSSIDDev;
					{
						VIRTUAL_ADAPTER *virtual_ad_p = netdev_priv(skb->dev);

						virtual_ad_p->net_stats.rx_packets++;
						virtual_ad_p->net_stats.rx_bytes += skb->len;
					}
					ASSERT(skb->dev);
				}
#endif // MESH_SUPPORT //

				else
				{
					rt->net_stats.rx_dropped++;

					printk("ERROR! Invalid dev_id=%d\n", dev_id);
					return 0;
				}

				/*
				ASSERT(dev_id >= 0 && dev_id < rt->RaCfgObj.BssidNum);
				if (dev_id < 0 || dev_id >= rt->RaCfgObj.BssidNum)
				{
					//printk("dev_id = %d\n", dev_id);
					dev_id = 1;
				}
				*/

				if (bMBSS && rt->RaCfgObj.MBSSID[dev_id].vlan_id)
				{
					TCI.priority = pTIC->priority;
					TCI.cfi = pTIC->cfi;
					// restore vlan_id
					TCI.vid = rt->RaCfgObj.MBSSID[dev_id].vlan_id;

					p = (u16 *) &TCI;
					*(u16 *)(skb->data+14) = htons(*p);
					//hex_dump("Vlan", skb->data, skb->len);
					SET_SKB_HARD_HEADER_LEN(skb, ETH_HLEN+4);
				}
				else
				{
					// Remove internal VLAN Tag
					//hex_dump("Vlan", skb->data, skb->len);					
					memmove(&skb->data[4], &skb->data[0], 12);
					skb->data += 4;
					skb->len -=4;
					SET_SKB_HARD_HEADER_LEN(skb, ETH_HLEN);
					//hex_dump("No Vlan", skb->data, skb->len);
				}

			} /* if 8100 */
		} /* if bridge==0 */
		else
		{
			rt->net_stats.rx_packets++;
			rt->net_stats.rx_bytes += skb->len;
		}
	} /* if FFFF */
	
	return dev_id;
}

/*
static void make_mbss_frame(
						   unsigned char **p, 
						   unsigned short CommandType, 
						   unsigned short CommandID, 
						   unsigned short len, 
						   unsigned short CommandSeq,
						   unsigned short devID)
{
	struct racfghdr *hdr = (struct racfghdr*)(*p);

	hdr->magic_no     = cpu_to_le32(MAGIC_NUMBER);
	hdr->command_type = cpu_to_le16(CommandType);
	hdr->command_id   = cpu_to_le16(CommandID);
	hdr->command_seq  = cpu_to_le16(CommandSeq);
	hdr->length       = cpu_to_le16(len);
	hdr->dev_id       = cpu_to_le16(devID);
	*p = hdr->data;

	hdr->flags = 0;
#ifdef __BIG_ENDIAN
	hdr->flags |= FLAG_BIG_ENDIAN;
#endif
}
*/


static int MbssHandler(iNIC_PRIVATE *pAd, int cmd, int dev_id)
{
	static unsigned short cmd_seq = 0;

	//unsigned char buffer[sizeof(struct racfghdr)];
	//unsigned char *p = &buffer[0];
	struct iwreq wrq; // dummy
	int error = 0;

	SendRaCfgCommand(pAd, RACFG_CMD_TYPE_SYNC, cmd, 0, 0, cmd_seq, 0, dev_id, NULL);

	/*
	make_mbss_frame(&p, RACFG_CMD_TYPE_SYNC, cmd,  0, cmd_seq, (unsigned short)dev_id);
	send_racfg_cmd(pAd, buffer, (p - buffer));
	*/

	/* close all interfaces , iNIC shutdown, no feedback
	if (cmd == RACFG_CMD_MBSS_CLOSE && dev_id == 0)
		return error;
		*/

	error = RaCfgWaitSyncRsp(pAd, cmd, cmd_seq++, &wrq, NULL, 2);

#if (CONFIG_INF_TYPE==INIC_INF_TYPE_USB)
	// USB only
	if (error){
		printk("resend mbss close\n");
		error = RaCfgWaitSyncRsp(pAd, cmd, cmd_seq++, &wrq, NULL, 2);
	}
#endif

	if (!error)
	{
		printk("setup MBSS[%d] success\n", dev_id);
	}
	else
	{
		printk("setup MBSS[%d] fail\n", dev_id);
	}
	return error; 
}


