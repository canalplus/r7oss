#include "rlk_inic.h"

/*
static void make_apcli_frame(
							unsigned char **p, 
							unsigned short CommandType, 
							unsigned short CommandID, 
							unsigned short len, 
							unsigned short CommandSeq,
							unsigned short devID);
*/

static int ApcliHandler(iNIC_PRIVATE *pAd, int cmd, int dev_id);

static int APCLI_VirtualIF_Close(struct net_device *dev_p);
static int APCLI_VirtualIF_Open(struct net_device *dev_p);
static int APCLI_VirtualIF_Ioctl(struct net_device *dev_p, struct ifreq *rq_p, int cmd);
static int APCLI_VirtualIF_PacketSend(struct sk_buff    *skb_p, struct net_device *dev_p);

extern struct net_device_stats *VirtualIF_get_stats(struct net_device *dev);

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,28)
static const struct net_device_ops netdev_ops = {
	.ndo_open		= APCLI_VirtualIF_Open,
	.ndo_stop		= APCLI_VirtualIF_Close,
	.ndo_start_xmit		= APCLI_VirtualIF_PacketSend,
	.ndo_do_ioctl       = APCLI_VirtualIF_Ioctl,
	.ndo_get_stats      = VirtualIF_get_stats,
};
#endif

void rlk_inic_apcli_init (
					   struct net_device *main_dev_p, 
					   iNIC_PRIVATE *ad_p)
{
	struct net_device *cur_dev_p;
	char   slot_name[IFNAMSIZ];
	struct net_device *new_dev_p;
	VIRTUAL_ADAPTER *apcli_ad_p;
	int apcli_index;
	int index = 0;

	if (ad_p->RaCfgObj.flg_apcli_init)
		return;

	ad_p->RaCfgObj.flg_apcli_init = 1;

	printk("%s --->\n", __FUNCTION__);

	/* End of if */

	/* first apcli_index must not be 0 (BSS0), must be 1 (BSS1) */
	for (apcli_index = FIRST_APCLIID; apcli_index < MAX_APCLI_NUM; apcli_index++)
		ad_p->RaCfgObj.APCLI[apcli_index].MSSIDDev = NULL;
	/* End of for */


	/* create virtual network interface */
	for (apcli_index=FIRST_APCLIID; apcli_index < MAX_APCLI_NUM; apcli_index++)
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
			printk("Allocate network device fail (APCLI)...\n");
			break;
		} /* End of if */

		/* find an available interface name, max 32 ra interfaces */
		for (index = 0; index < APCLI_MAX_DEV_NUM; index++)
		{

#if defined(MULTIPLE_CARD_SUPPORT)|defined(CONFIG_CONCURRENT_INIC_SUPPORT)
			if (ad_p->RaCfgObj.InterfaceNumber >= 0)
				snprintf(slot_name, sizeof(slot_name), "apcli%02d_%d", ad_p->RaCfgObj.InterfaceNumber, index);
			else
#endif 
				snprintf(slot_name, sizeof(slot_name), "apcli%d", index);


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
		if (index < APCLI_MAX_DEV_NUM)
		{
#if defined(MULTIPLE_CARD_SUPPORT) || defined(CONFIG_CONCURRENT_INIC_SUPPORT)
			if (ad_p->RaCfgObj.InterfaceNumber >= 0)
				snprintf(new_dev_p->name, sizeof(new_dev_p->name),"apcli%02d_%d", ad_p->RaCfgObj.InterfaceNumber, index);
			else
#endif 
				snprintf(new_dev_p->name, sizeof(new_dev_p->name),"apcli%d", index);
			printk("Register APCLI IF (apcli%d)\n", index);
		}
		else
		{
			/* error! no any available ra name can be used! */
			printk("Has %d RA interfaces (APCLI)...\n", APCLI_MAX_DEV_NUM);
			kfree(new_dev_p);
			break;
		} /* End of if */

		/* init the new network interface */
		//ether_setup(new_dev_p);

		new_dev_p->hard_header_len += 4;

		apcli_ad_p = netdev_priv(new_dev_p);
		apcli_ad_p->VirtualDev = new_dev_p;	 /* 4B */
		apcli_ad_p->RtmpDev    = main_dev_p; /* 4B */

		/* init MAC address of virtual network interface */

		memmove(ad_p->RaCfgObj.APCLI[apcli_index].Bssid, 
				main_dev_p->dev_addr, MAC_ADDR_LEN);

#ifdef NEW_MBSS_SUPPORT
#ifdef CONFIG_CONCURRENT_INIC_SUPPORT
		if(ad_p == gAdapter[0])
#else
		if(1)
#endif
		{
			ad_p->RaCfgObj.APCLI[apcli_index].Bssid[0] += 2;
#ifdef MESH_SUPPORT
			ad_p->RaCfgObj.APCLI[apcli_index].Bssid[0] += ((ad_p->RaCfgObj.BssidNum + MAX_MESH_NUM + apcli_index - 1) << 2);
#else
			ad_p->RaCfgObj.APCLI[apcli_index].Bssid[0] += ((ad_p->RaCfgObj.BssidNum + apcli_index - 1) << 2);
#endif
		}
		else
#endif
		{
			//ad_p->RaCfgObj.APCLI[apcli_index].Bssid[5] += apcli_index;
			ad_p->RaCfgObj.APCLI[apcli_index].Bssid[5] += ad_p->RaCfgObj.BssidNum + apcli_index;
#ifdef MESH_SUPPORT
			if (ad_p->RaCfgObj.bMesh)
				ad_p->RaCfgObj.APCLI[apcli_index].Bssid[5] += MAX_MESH_NUM;
#endif // MESH_SUPPORT //
		}
		memmove(new_dev_p->dev_addr,
				ad_p->RaCfgObj.APCLI[apcli_index].Bssid, MAC_ADDR_LEN);

		/* init operation functions */
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,28)
		new_dev_p->open             = APCLI_VirtualIF_Open;
		new_dev_p->stop             = APCLI_VirtualIF_Close;
		new_dev_p->hard_start_xmit  = APCLI_VirtualIF_PacketSend;
		new_dev_p->do_ioctl         = APCLI_VirtualIF_Ioctl;
		new_dev_p->get_stats        = VirtualIF_get_stats;
		/* if you dont implement get_stats, dont assign your function with empty
		   body; or kernel will panic */
#else
		new_dev_p->netdev_ops = &netdev_ops;
#endif
		new_dev_p->priv_flags       = INT_APCLI; /* we are virtual interface */

		/* register this device to OS */
		register_netdevice(new_dev_p);

		/* backup our virtual network interface */
		ad_p->RaCfgObj.APCLI[apcli_index].MSSIDDev = new_dev_p;
	} /* End of for */

	printk("%s <---\n", __FUNCTION__);
} /* End of RLK_INIC_APCLI_Init */


void rlk_inic_apcli_restart(uintptr_t arg)
{
	iNIC_PRIVATE *pAd = (iNIC_PRIVATE *)arg;
	int dev_id = 0;
	int error;

	if (pAd->RaCfgObj.APCLI[dev_id].flags)
	{
		error = 1;

		error = ApcliHandler(pAd, RACFG_CMD_APCLI_OPEN, dev_id);
		if (!error)
		{
			printk("ReOpen APCLI %d\n", dev_id);
		}
	}
}

int rlk_inic_apcli_close(iNIC_PRIVATE *ad_p)
{
	int apcli_index, error = 1;

	printk("%s --->\n", __FUNCTION__);

	for (apcli_index=FIRST_APCLIID; apcli_index<MAX_APCLI_NUM; apcli_index++)
	{
		if (ad_p->RaCfgObj.APCLI[apcli_index].MSSIDDev)
		{
			ad_p->RaCfgObj.apcli_close_all = 1;
			dev_close(ad_p->RaCfgObj.APCLI[apcli_index].MSSIDDev);
			ad_p->RaCfgObj.apcli_close_all = 0;
		}
		/* End of if */
	} /* End of for */

	printk("%s <---\n", __FUNCTION__);
	return error;
} /* End of RLK_INIC_APCLI_Close */

void rlk_inic_apcli_remove(iNIC_PRIVATE *ad_p)
{
	int apcli_index;


	printk("%s --->\n", __FUNCTION__);

	for (apcli_index=FIRST_APCLIID; apcli_index<MAX_APCLI_NUM; apcli_index++)
	{
		struct net_device *dev = ad_p->RaCfgObj.APCLI[apcli_index].MSSIDDev;
		if (dev)
		{
			printk("unregister %s\n", dev->name);
			// keep unregister_netdev(ra1) from calling dev_close(ra1), which
			// will be called in the following unregister_netdev(ra0)
			ad_p->RaCfgObj.apcli_close_all = 1;
			unregister_netdev(dev);
			ad_p->RaCfgObj.apcli_close_all = 0;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
			free_netdev(dev);
#else
			kfree(dev);
#endif // LINUX_VERSION_CODE //
			ad_p->RaCfgObj.APCLI[apcli_index].MSSIDDev = NULL;
		} /* End of if */
	} /* End of for */

	ad_p->RaCfgObj.flg_apcli_init = 0;

	printk("%s <---\n", __FUNCTION__);
} /* End of RLK_INIC_APCLI_Remove */

int APCLI_Find_DevID(
					struct net_device *main_dev, 
					struct net_device *apcli_dev)
{
	iNIC_PRIVATE *ad_p = NULL;
	int apcli_index;

	ad_p = netdev_priv(main_dev);
	//netif_start_queue(virtual_ad_p->VirtualDev);

	for (apcli_index=FIRST_APCLIID; apcli_index<MAX_APCLI_NUM; apcli_index++)
	{
		if (ad_p->RaCfgObj.APCLI[apcli_index].MSSIDDev == apcli_dev)
			return apcli_index;
		/* End of if */
	} /* End of for */
	return 0;

}
static int APCLI_VirtualIF_Open(struct net_device *dev_p)
{
	int dev_id = 0, error = 1;
	VIRTUAL_ADAPTER *virtual_ad_p = netdev_priv(dev_p);
	iNIC_PRIVATE    *pAd;

	ASSERT(virtual_ad_p->VirtualDev);
	ASSERT(virtual_ad_p->RtmpDev);

	//if (!NETIF_IS_UP(virtual_ad_p->RtmpDev))
	if (!NETIF_IS_UP(GET_PARENT(dev_p)))
	{
		printk("%s should be open at first\n", virtual_ad_p->RtmpDev->name);
		return -EFAULT;
	}


	netif_carrier_on(dev_p);
	netif_start_queue(dev_p);
	//netif_start_queue(virtual_ad_p->VirtualDev);

	dev_id = APCLI_Find_DevID(virtual_ad_p->RtmpDev, dev_p);
	ASSERT(dev_id >= 0);
	ASSERT(netdev_priv(virtual_ad_p->RtmpDev));
	pAd = (iNIC_PRIVATE *) netdev_priv(virtual_ad_p->RtmpDev);
	error = ApcliHandler(netdev_priv(virtual_ad_p->RtmpDev), RACFG_CMD_APCLI_OPEN, dev_id);
	if (!error)
	{
		pAd->RaCfgObj.APCLI[dev_id].flags = 1;
		printk("%s: ==>>> APCLI_VirtualIF_Open\n", dev_p->name);
	}
	return error;
} /* End of APCLI_VirtualIF_Open */

static int APCLI_VirtualIF_Close(struct net_device *dev_p)
{
	int dev_id = 0, error = 1;
	VIRTUAL_ADAPTER *virtual_ad_p = netdev_priv(dev_p);
	iNIC_PRIVATE    *pAd;

	ASSERT(virtual_ad_p->VirtualDev);
	ASSERT(virtual_ad_p->RtmpDev);

	//if (!NETIF_IS_UP(virtual_ad_p->RtmpDev))
	if (!NETIF_IS_UP(GET_PARENT(dev_p)))
	{
		printk("%s should be open at first\n", virtual_ad_p->RtmpDev->name);
		return -EFAULT;
	}

	if (((iNIC_PRIVATE *)netdev_priv(virtual_ad_p->RtmpDev))->RaCfgObj.apcli_close_all)
		return 0;


	netif_stop_queue(dev_p);
	netif_carrier_off(dev_p);

	dev_id = APCLI_Find_DevID(virtual_ad_p->RtmpDev, dev_p);
	ASSERT(dev_id >= 0);
	ASSERT(netdev_priv(virtual_ad_p->RtmpDev));
	pAd = (iNIC_PRIVATE *) netdev_priv(virtual_ad_p->RtmpDev);
	error = ApcliHandler(netdev_priv(virtual_ad_p->RtmpDev), RACFG_CMD_APCLI_CLOSE, dev_id);
	if (!error)
	{
		pAd->RaCfgObj.APCLI[dev_id].flags = 0;
		printk("%s: ==>>> APCLI_VirtualIF_Close\n", dev_p->name);
	}

	return error;
} /* End of APCLI_VirtualIF_Close */

static int APCLI_VirtualIF_Ioctl(
								struct net_device *dev_p, 
								struct ifreq      *rq_p, 
								int                cmd)
{
	VIRTUAL_ADAPTER *virtual_ad_p;
	iNIC_PRIVATE *ad_p;

	//if (!NETIF_IS_UP(dev_p))
	if (!NETIF_IS_UP(GET_PARENT(dev_p)))
		return -ENETDOWN;
	/* sanity check */
	virtual_ad_p = netdev_priv(dev_p);
	ASSERT(virtual_ad_p);
	ASSERT(virtual_ad_p->RtmpDev);
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
} /* End of APCLI_VirtualIF_Ioctl */


/*
   ========================================================================
   Routine Description:
	   Dummy routine to avoid kernel crash
	========================================================================
*/
static int APCLI_VirtualIF_PacketSend(
									 struct sk_buff  *skb_p,
									 struct net_device *dev_p)
{
	int dev_id = 0;
	VIRTUAL_ADAPTER *virtual_ad_p = netdev_priv(dev_p);
	iNIC_PRIVATE *rt;


	ASSERT(skb_p);
	ASSERT(virtual_ad_p->VirtualDev);
	ASSERT(virtual_ad_p->RtmpDev);

	dev_id = APCLI_Find_DevID(virtual_ad_p->RtmpDev, dev_p);
	ASSERT(dev_id >= 0);
	rt = netdev_priv(virtual_ad_p->RtmpDev);

	if (rt->RaCfgObj.bBridge == 1 || !rt->RaCfgObj.bApcli)
	{
		// built in bridge enabled,all packets sent by main device,
		// virtual device should drop all the packets(may flooded by bridge) 
		// ==> (X) rlk_inic_start_xmit(skb, dev);
		virtual_ad_p->net_stats.tx_dropped++;
		dev_kfree_skb(skb_p);
	}
	else
	{
		skb_p = Insert_Vlan_Tag(rt, skb_p, dev_id, SOURCE_APCLI);
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
} /* End of APCLI_VirtualIF_PacketSend */

/*
static void make_apcli_frame(
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


static int ApcliHandler(iNIC_PRIVATE *pAd, int cmd, int dev_id)
{
	static unsigned short cmd_seq = 0;

	//unsigned char buffer[sizeof(struct racfghdr)];
	//unsigned char *p = &buffer[0];
	struct iwreq wrq; // dummy
	int error = 0;

	SendRaCfgCommand(pAd, RACFG_CMD_TYPE_SYNC, cmd, 0, 0, cmd_seq, 0, dev_id, NULL);

	/*
	make_apcli_frame(&p, RACFG_CMD_TYPE_SYNC, cmd,  0, cmd_seq, (unsigned short)dev_id);
	send_racfg_cmd(pAd, buffer, (p - buffer));
	*/

	// close all interfaces , iNIC shutdown, no feedback
	if (cmd == RACFG_CMD_APCLI_CLOSE && dev_id == 0)
		return error;

	error = RaCfgWaitSyncRsp(pAd, cmd, cmd_seq++, &wrq, NULL, 2);
	if (!error)
	{
		printk("setup APCLI[%d] success\n", dev_id);
	}
	else
	{
		printk("setup APCLI[%d] fail\n", dev_id);
	}
	return error; 
}


