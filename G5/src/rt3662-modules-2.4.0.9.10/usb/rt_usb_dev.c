#include "../comm/rlk_inic.h"
#include <linux/workqueue.h>
/*
 * This is a "USB networking" framework that works with ralink rlk_inic.
 * We call this "Host Driver", and will create a netdevice to control networking
 * behavior. Besides, we also upload "Firmware" to rlk_inic device, which control
 * wireless and usb interface communication in rlk_inic device.
 */


#define	EXTRA_PAD					36	// 31 byte Max assignment + 4 byte header&count + 1 byte padding.

/* global iNIC private data */
static iNIC_PRIVATE *gAdapter = NULL;
extern struct usb_device *rlk_inic_udev;

#ifdef DBG
char *root = "";
#endif
static int debug = -1;
char *mode  = "sta";	    // ap mode
char *mac = "";		    // default 00:00:00:00:00:00
static int bridge = 1;  // enable built-in bridge
static int csumoff = 0; // Enable Checksum Offload over IPv4(TCP, UDP)
int max_fw_upload = 5;  // Max Firmware upload attempt

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,12)
#ifdef DBG
MODULE_PARM (root, "s");
#endif
MODULE_PARM (debug, "i");
MODULE_PARM (bridge, "i");
MODULE_PARM (csumoff, "i");
MODULE_PARM (mode, "s");
MODULE_PARM (mac, "s");
#else
#ifdef DBG
module_param (root, charp, 0);
MODULE_PARM_DESC (root, DRV_NAME ": firmware and profile path offset");
#endif
module_param (debug, int, 0);
module_param (bridge, int, 1);
module_param (csumoff, int, 0);
module_param (mode, charp, 0);
module_param (mac, charp, 0);
#endif
MODULE_PARM_DESC (debug, DRV_NAME ": bitmapped message enable number");
MODULE_PARM_DESC (mode, DRV_NAME ": iNIC operation mode: AP(default) or STA");
MODULE_PARM_DESC (mac, DRV_NAME ": iNIC mac addr");
MODULE_PARM_DESC (bridge, DRV_NAME ": on/off iNIC built-in bridge engine");
MODULE_PARM_DESC (csumoff, DRV_NAME ": on/off checksum offloading over IPv4 <TCP, UDP>");


extern int	NICLoadFirmware(iNIC_PRIVATE *pAd);
extern int rlk_inic_racfg(UINT *pData,UINT32 request);
extern BOOLEAN RLK_INICProbePostConfig(void *, INT32);

/*   PACE_PATCH  */
static struct semaphore wifi_reset_chip_sem;
/*   END PACE_PATCH   */

static int usbnet_open (struct net_device *net);
static int usbnet_stop (struct net_device *net);
static int usbnet_start_xmit(struct sk_buff *skb, struct net_device *net);

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,28)
static const struct net_device_ops netdev_ops = {
	.ndo_open		= usbnet_open,
	.ndo_stop		= usbnet_stop,
	.ndo_start_xmit    = usbnet_start_xmit,
	.ndo_do_ioctl       = rlk_inic_ioctl,
};
#endif

/*-------------------------------------------------------------------------*/
//	rt_get_endpoints:  		get IN and OUT Endpoint Address
/*-------------------------------------------------------------------------*/
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
int rt_get_endpoints(struct usbnet *dev, struct usb_interface *intf)
{
	struct usb_interface_descriptor *alt;
	struct usb_endpoint_descriptor	*in=NULL, *out=NULL;
	int				tmp;

	for (tmp = 0; tmp < intf->max_altsetting; tmp++) {
		unsigned	ep;
		alt = intf->altsetting + tmp;
		for (ep = 0; ep < alt->bNumEndpoints; ep++) {
			struct usb_endpoint_descriptor	*e;
			e = alt->endpoint + ep;
			if (e->bmAttributes == USB_ENDPOINT_XFER_BULK){
				if (((e->bEndpointAddress & USB_ENDPOINT_DIR_MASK)==USB_DIR_IN) && !in){
						in = e;
				} else if (((e->bEndpointAddress & USB_ENDPOINT_DIR_MASK)==USB_DIR_OUT) && !out){
						out = e;
				}
			}
			if (in && out)
				goto ep_found;
		}
	}

ep_found:

	if(!in || !out)
		return -EINVAL;

	tmp = usb_set_interface (dev->udev, alt->bInterfaceNumber, alt->bAlternateSetting);
	if (tmp < 0)
		return tmp;

	dev->in = usb_rcvbulkpipe (dev->udev,
			in->bEndpointAddress & USB_ENDPOINT_NUMBER_MASK);
	dev->out = usb_sndbulkpipe (dev->udev,
			out->bEndpointAddress & USB_ENDPOINT_NUMBER_MASK);
	return 0;

}
#else
int rt_get_endpoints(struct usbnet *dev, struct usb_interface *intf)
{
	struct usb_host_interface	*alt = NULL;
	struct usb_host_endpoint	*in = NULL, *out = NULL;
	int				tmp;

	for (tmp = 0; tmp < intf->num_altsetting; tmp++) {
		unsigned	ep;
		alt = intf->altsetting + tmp;
		for (ep = 0; ep < alt->desc.bNumEndpoints; ep++) {
			struct usb_host_endpoint	*e;
			e = alt->endpoint + ep;
			if(e->desc.bmAttributes == USB_ENDPOINT_XFER_BULK){
				if(((e->desc.bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_IN) && !in)
					in = e;
				else if(((e->desc.bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_OUT) && !out)
					out = e;
			}
		}
		if (in && out)
			break;
	}

	if(!in || !out)
		return -EINVAL;

	tmp = usb_set_interface (dev->udev, alt->desc.bInterfaceNumber,  alt->desc.bAlternateSetting);
	if (tmp < 0)
		return tmp;

	dev->in = usb_rcvbulkpipe (dev->udev,
			(in->desc.bEndpointAddress & USB_ENDPOINT_NUMBER_MASK));
	dev->out = usb_sndbulkpipe (dev->udev,
			(out->desc.bEndpointAddress & USB_ENDPOINT_NUMBER_MASK));

	return 0;
}
#endif

/*-------------------------------------------------------------------------*/
//	rt_skb_return:  (1) handle in-band command	(2) bypass packets to interface layer
/*-------------------------------------------------------------------------*/
void rt_skb_return (struct usbnet *dev, struct sk_buff *skb)
{
	iNIC_PRIVATE *rt = netdev_priv(dev->net);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
	skb->dev = dev->net;
#endif

	skb->protocol = eth_type_trans (skb, dev->net);

	if(racfg_frame_handle(rt, skb)== TRUE)
		return;

	dev->stats.rx_packets++;
	dev->stats.rx_bytes += skb->len;

	if(csumoff)
		skb->ip_summed = CHECKSUM_UNNECESSARY;

	memset (skb->cb, 0, sizeof (struct skb_data));
	netif_rx (skb);

}

/*-------------------------------------------------------------------------*/
//	defer_bh: 	defer packets, put skb to done queue
/*-------------------------------------------------------------------------*/
static void defer_bh(struct usbnet *dev, struct sk_buff *skb, struct sk_buff_head *list)
{
	unsigned long		flags;

	spin_lock_irqsave(&list->lock, flags);
	__skb_unlink(skb, list);
	spin_unlock(&list->lock);
	spin_lock(&dev->done.lock);
	__skb_queue_tail(&dev->done, skb);
	if (dev->done.qlen == 1)
		tasklet_schedule(&dev->bh);
	spin_unlock_irqrestore(&dev->done.lock, flags);
}

/*-------------------------------------------------------------------------*/
static void rx_complete (struct urb *urb);

/*-------------------------------------------------------------------------*/
//	rx_submit: 	defer packets, put skb to done queue
/*-------------------------------------------------------------------------*/
static void rx_submit (struct usbnet *dev, struct urb *urb, unsigned flags)
{
	struct sk_buff		*skb;
	struct skb_data		*entry;
	int			retval = 0;
	unsigned long		lockflags;
	size_t			size = dev->rx_urb_size;

#ifdef RLK_INIC_USBDEV_GEN2
	struct net_device	*net = dev->net;
	iNIC_PRIVATE *pAd = netdev_priv(net);
#endif

#ifdef RLK_INIC_SOFTWARE_AGGREATION
	size += EXTRA_PAD;
#else
	size += 5;
#endif

	if ((skb = alloc_skb (size, flags)) == NULL) {
		devdbg(dev, "no rx skb");
		usb_free_urb (urb);
		return;
	}

#ifdef RLK_INIC_SOFTWARE_AGGREATION
	if(((unsigned int)skb->data % 32) != 0)
		skb_reserve (skb, 32 - ((unsigned int)skb->data % 32));
#endif

	entry = (struct skb_data *) skb->cb;
	entry->urb = urb;
	entry->dev = dev;
	entry->length = 0;

	usb_fill_bulk_urb (urb, dev->udev, dev->in,
		skb->data, size, rx_complete, skb);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
	urb->transfer_flags |= USB_ASYNC_UNLINK;
#endif
	spin_lock_irqsave (&dev->rxq.lock, lockflags);

#ifdef RLK_INIC_USBDEV_GEN2
	if (pAd->flag >= FLAG_READY_MODE) {
#else
	if (netif_running (dev->net) && netif_device_present (dev->net)) {
#endif
		switch (retval = RTUSB_SUBMIT_URB (urb)) {
		case -EPIPE:
		case -ENOMEM:
			break;
		case -ENODEV:
			if (netif_msg_rx_err (dev))
				devdbg(dev, "device gone");
			netif_device_detach (dev->net);
			break;
		default:
			if (netif_msg_rx_err (dev))
				devdbg(dev, "rx submit, %d", retval);
			tasklet_schedule (&dev->bh);
			break;
		case 0:
			__skb_queue_tail (&dev->rxq, skb);
		}
	} else {
		devdbg (dev, "rx: stopped");
		retval = -ENOLINK;
	}
	spin_unlock_irqrestore (&dev->rxq.lock, lockflags);

	if (retval) {
		dev_kfree_skb_any (skb);
		usb_free_urb (urb);
	}
}

/*-------------------------------------------------------------------------*/
//	rx_process: 	process rx packets, if len=0, error packets, put to done queue(rx_cleanup)
/*-------------------------------------------------------------------------*/
static inline void rx_process (struct usbnet *dev, struct sk_buff *skb)
{
#ifdef RLK_INIC_SOFTWARE_AGGREATION
	struct sk_buff *each_skb;
	int count, i;
	unsigned int length=0;
#endif

	if (skb->len){
#ifdef RLK_INIC_SOFTWARE_AGGREATION
		count = skb->data[0] + (skb->data[1] << 8);
		skb->data += 2;
		for(i=0; i<count; i++){
			length = skb->data[0] + (skb->data[1] << 8);
			if(length > dev->net->mtu + dev->net->hard_header_len){
				printk("Error Aggregate Pkt size %d\n", length);
				break;
			}
			skb->data +=2;
			each_skb = alloc_skb(length + 2, GFP_ATOMIC);
			skb_reserve(each_skb, 2);
			memcpy(each_skb->data, skb->data, length);
			skb_put(each_skb, length);
			each_skb->dev = dev->net;
			rt_skb_return(dev, each_skb);
			skb->data += length;
		}
		dev_kfree_skb_any (skb);
#else
		rt_skb_return (dev, skb);
#endif
	}else {
		devdbg(dev, "rx_process skb->len=0, drop!\n");
		dev->stats.rx_errors++;
		skb_queue_tail (&dev->done, skb);
	}
}

/*-------------------------------------------------------------------------*/
//	rx_complete: 	called when urb rx complete (call back function)
/*-------------------------------------------------------------------------*/
static void rx_complete (struct urb *urb)
{
	struct sk_buff		*skb = (struct sk_buff *) urb->context;
	struct skb_data		*entry = (struct skb_data *) skb->cb;
	struct usbnet		*dev = entry->dev;
	int			urb_status = urb->status;

#ifdef RLK_INIC_USBDEV_GEN2
	struct net_device	*net = dev->net;
	iNIC_PRIVATE *pAd = netdev_priv(net);
#endif

	skb_put (skb, urb->actual_length);
	entry->state = rx_done;
	entry->urb = NULL;

	switch (urb_status) {
	case 0: 		/* success */
		if (skb->len < (dev->net->hard_header_len+2)) {
			entry->state = rx_cleanup;
			dev->stats.rx_errors++;
			dev->stats.rx_length_errors++;
			devdbg(dev, "rx length %d, hard_head %d", skb->len, dev->net->hard_header_len);
		}else if(skb->data[0]==0x30 && skb->data[1]==0x52){
			/* remove header : rlk_inic specific */
			skb->len -= 2;
			skb->data += 2;
		}else{
			entry->state = rx_cleanup;
			dev->stats.rx_errors++;
			dev->stats.rx_length_errors++;
			devdbg(dev, "error packet header received! 0x%02x 0x%02x", skb->data[0], skb->data[1]);
		}
 		break;
	case -EPIPE:
		dev->stats.rx_errors++;
	case -ECONNRESET:		/* async unlink */
	case -ESHUTDOWN:		/* hardware gone */
		if (netif_msg_ifdown (dev))
			devdbg (dev, "rx shutdown, code %d", urb_status);
		goto block;
	case -EPROTO:
	case -ETIME:
	case -EILSEQ:
		dev->stats.rx_errors++;
		if (!timer_pending (&dev->delay)) {
			mod_timer (&dev->delay, jiffies + THROTTLE_JIFFIES);
			if (netif_msg_link (dev))
				devdbg (dev, "rx throttle %d", urb_status);
		}
block:
		entry->state = rx_cleanup;
		entry->urb = urb;
		urb = NULL;
		break;
	case -EOVERFLOW:
		dev->stats.rx_over_errors++;
	default:
		entry->state = rx_cleanup;
		dev->stats.rx_errors++;
		if (netif_msg_rx_err (dev))
			devdbg (dev, "rx status %d", urb_status);
		break;
	}

	/* move packets from rx queue to done queue */
	defer_bh(dev, skb, &dev->rxq);

	if (urb) {
#ifdef RLK_INIC_USBDEV_GEN2
		if (pAd->flag >= FLAG_READY_MODE){
#else
		if (netif_running (dev->net)){
#endif
			rx_submit (dev, urb, GFP_ATOMIC | GFP_DMA);
			return;
		}
		usb_free_urb (urb);
	}
	if (netif_msg_rx_err (dev))
		devdbg (dev, "no read resubmitted");
}

/*-------------------------------------------------------------------------*/
//	usbnet_stop: 	called when interface down
/*-------------------------------------------------------------------------*/
static int usbnet_stop (struct net_device *net)
{
	struct usbnet		*dev = netdev_priv(net);
	iNIC_PRIVATE *pAd = netdev_priv(net);

	devdbg (dev, "Function call %s mode=%d\n", __FUNCTION__, pAd->flag);
#if 0  // Do nothing ... never stop
	netif_stop_queue (net);

	/* deferred work (task, timer, softirq) must also stop.  can't flush_scheduled_work()
	 *  until we drop rtnl (later), else workers could deadlock */
	del_timer_sync (&dev->delay);
	tasklet_kill (&dev->bh);

	pAd->flag = FLAG_INIT_MODE;
	RaCfgStateReset(pAd);
	RaCfgInterfaceClose(pAd);
#endif
	netif_carrier_off(net);
	return 0;
}

/*-------------------------------------------------------------------------*/
//	usbnet_open: 	called when interface up
/*-------------------------------------------------------------------------*/
static int usbnet_open (struct net_device *net)
{
	struct usbnet		*dev = netdev_priv(net);
	int			retval = 0;
	iNIC_PRIVATE *pAd = netdev_priv(net);
	unsigned char usb_ep0_pData[64];
	int ret = -1;

	devdbg (dev, "1.Function call %s mode=%d\n", __FUNCTION__, pAd->flag);

	if(pAd->flag < FLAG_READY_MODE)	/* interface up only allowed when module has inserted */
		return 0;

	if(pAd->flag == FLAG_READY_MODE){ 	/* notify rlk_inic start to upload firmware */
		devdbg (dev, "2.Function call %s mode=%d\n", __FUNCTION__, pAd->flag);

		rlk_inic_read_profile(pAd); /* for Ext E2P status check */
		devdbg (dev, "3.Function call %s mode=%d\n", __FUNCTION__, pAd->flag);

		ret = NICLoadFirmware(pAd);
		devdbg (dev, "4.Function call %s mode=%d\n", __FUNCTION__, pAd->flag);

		if(ret){
			pAd->flag = FLAG_FWFAIL_MODE;
			rlk_inic_racfg((UINT *)usb_ep0_pData,RACFG_CMD_BOOT_RESET);
			return 0;
		}

		pAd->flag = FLAG_OPEN_MODE;
		retval = rlk_inic_racfg((UINT *)usb_ep0_pData,RACFG_CMD_BOOT_STARTUP);
		if(retval <0){
			devdbg (dev, "\n!! The %s not respond !! --> RACFG_CMD_BOOT_STARTUP\n", DRV_NAME);
			pAd->flag = FLAG_FWFAIL_MODE;
			rlk_inic_racfg((UINT *)usb_ep0_pData,RACFG_CMD_BOOT_RESET);
			return 0;
		}

		RaCfgInterfaceOpen(pAd);

		RaCfgSetUp(pAd, net);
		netif_carrier_off(net);
		if(pAd->RaCfgObj.bGetMac == TRUE){
			pAd->flag = FLAG_FWDONE_MODE;
		}else{
			devdbg (dev, "\n!! %s not respond ,reset iNIC....\n", DRV_NAME);
			pAd->flag = FLAG_FWFAIL_MODE;
			rlk_inic_racfg((UINT *)usb_ep0_pData,RACFG_CMD_BOOT_RESET);
			return 0;
		}

	}else{	/* firmware is running */

		netif_start_queue (net);
		netif_carrier_off(net);
		tasklet_schedule (&dev->bh);
	}

	return 0;

}

/*-------------------------------------------------------------------------*/
//	tx_complete: 	send urb complete (call back function)
/*-------------------------------------------------------------------------*/
int rlk_inic_usb_xmit (struct sk_buff *skb, struct net_device *net);
static void tx_complete (struct urb *urb)
{
	struct sk_buff		*skb = (struct sk_buff *) urb->context;
	struct skb_data		*entry = (struct skb_data *) skb->cb;
	struct usbnet		*dev = entry->dev;

	if (urb->status == 0) {
		dev->stats.tx_packets++;
		dev->stats.tx_bytes += entry->length;
	} else {
		dev->stats.tx_errors++;
	}

	urb->dev = NULL;
	entry->state = tx_done;
	defer_bh(dev, skb, &dev->txq);
}

#if defined(RLK_INIC_SOFTWARE_AGGREATION) || defined(RLK_INIC_TX_AGGREATION_ONLY)
/*-------------------------------------------------------------------------*/
//	software aggregate: under testing
/*-------------------------------------------------------------------------*/
void skb_aggregate_queue(struct sk_buff *skb, struct net_device *net, int intail)
{
	struct usbnet		*dev = netdev_priv(net);
	unsigned long		flags;

	spin_lock_irqsave (&dev->aggr_q.lock, flags);
	if(intail)
		__skb_queue_tail (&dev->aggr_q, skb);
	else
		__skb_queue_head(&dev->aggr_q, skb);
	spin_unlock_irqrestore (&dev->aggr_q.lock, flags);

	if(dev->aggr_q.qlen >= MAX_PKT_OF_AGGR_QUEUE){
		//printk("netif_stop_queue\n");
		netif_stop_queue (net);
	}
}

struct sk_buff *skb_aggregate(struct net_device *net)
{
	struct usbnet		*dev = netdev_priv(net);
	struct sk_buff 	*skb=NULL, *aggr_skb=NULL;
	unsigned long		flags;
	unsigned short	len=0, count=0;
	int				original_qlen=dev->aggr_q.qlen;
	char				*ptr;

	do{
		spin_lock_irqsave (&dev->aggr_q.lock, flags);
		skb = __skb_dequeue(&dev->aggr_q);
		spin_unlock_irqrestore (&dev->aggr_q.lock, flags);

		if(skb == NULL)
			break;

		if(count==0){
			if ((aggr_skb = alloc_skb(AGGR_QUEUE_SIZE + EXTRA_PAD, GFP_ATOMIC | GFP_DMA)) == NULL) {
				printk("no tx aggr skb");
				return NULL;
			}
			if(((unsigned int)aggr_skb->data % 32) != 0)
				skb_reserve(aggr_skb, 32 - ((unsigned int)skb->data % 32));
#ifndef RLK_INIC_TX_AGGREATION_ONLY
			skb_reserve(aggr_skb, 4);
#endif
		}

		len = (unsigned short) skb->len;
#ifdef RLK_INIC_TX_AGGREATION_ONLY
		if((aggr_skb->len + 6 + len) > AGGR_QUEUE_SIZE)
#else
		if((aggr_skb->len + 2 + len) > AGGR_QUEUE_SIZE)
#endif
		{
			skb_aggregate_queue(skb, net, 0);
			break;
		}
#ifdef RLK_INIC_TX_AGGREATION_ONLY
		len += 2; // for 0x30 0x52
		ptr = aggr_skb->tail;
		*ptr = (unsigned char) (len & 0x00FF);
		ptr++;
		*ptr = (unsigned char) ((len >> 8) & 0x00FF);
		ptr++;
		*ptr = 0x00;
		ptr++;
		*ptr = 0x00;
		ptr++;
		*ptr = 0x30;
		ptr++;
		*ptr = 0x52;

		aggr_skb->tail += 6;
		aggr_skb->len += 6;
		memcpy(aggr_skb->tail, skb->data, skb->len);
		skb_put(aggr_skb, skb->len);
		dev_kfree_skb_any(skb);

		while(((unsigned long)aggr_skb->tail % 4) != 0){
			ptr = aggr_skb->tail;
			*ptr = 0x00;
			aggr_skb->tail++;
			aggr_skb->len++;
		}
		count++;
#else
		ptr = aggr_skb->tail;
		*ptr = (unsigned char) (len & 0x00FF);
		ptr++;
		*ptr = (unsigned char) ((len >> 8) & 0x00FF);
		aggr_skb->tail += 2;
		aggr_skb->len += 2;
		memcpy(aggr_skb->tail, skb->data, skb->len);
		skb_put(aggr_skb, skb->len);
		dev_kfree_skb_any(skb);
		count++;
#endif
	}while((aggr_skb->len + 2) < AGGR_QUEUE_SIZE);

	if(count == 0)
		return NULL;

	if(original_qlen >= MAX_PKT_OF_AGGR_QUEUE && dev->aggr_q.qlen < MAX_PKT_OF_AGGR_QUEUE){
		//printk("netif_wake_queue\n");
		netif_wake_queue(net);
	}

#ifdef RLK_INIC_TX_AGGREATION_ONLY
	aggr_skb->len += 4;
	ptr = aggr_skb->tail;
	memset(ptr, 0x00, 4);
	aggr_skb->tail+=4;
#else
	aggr_skb->len += 2;
	aggr_skb->data -= 2;
	aggr_skb->data[0] = (unsigned char) (count & 0x00FF);
	aggr_skb->data[1] = (unsigned char) ((count >> 8) & 0x00FF);
#endif
	return aggr_skb;

}

/*-------------------------------------------------------------------------*/
//	rlk_inic_usb_xmit: 	send out packets with header added.
/*-------------------------------------------------------------------------*/
int rlk_inic_usb_xmit (struct sk_buff *skb, struct net_device *net)
{
	struct usbnet		*dev = netdev_priv(net);
	int			length=0;
	int			retval = NET_XMIT_SUCCESS;
	struct urb		*urb = NULL;
	struct skb_data		*entry;
	unsigned long		flags;
	struct sk_buff *pkt;

	/* drop packet when f/w not ready */
	if(gAdapter && gAdapter->flag <= FLAG_FWFAIL_MODE){
		if (skb)
			dev_kfree_skb_any (skb);
		return NETDEV_TX_OK;
	}

	if(skb != NULL){
		skb_aggregate_queue(skb, net, 1);
	}
	spin_lock_irqsave (&dev->txq.lock, flags);
	if (dev->txq.qlen >= TX_QLEN (dev)){
		spin_unlock_irqrestore (&dev->txq.lock, flags);
		return NETDEV_TX_OK;
	}
	dev->txq.qlen++;
	spin_unlock_irqrestore (&dev->txq.lock, flags);

	pkt = skb_aggregate(net);
	if(pkt == NULL){
		spin_lock_irqsave (&dev->txq.lock, flags);
		dev->txq.qlen--;
		spin_unlock_irqrestore (&dev->txq.lock, flags);
		return NETDEV_TX_OK;
	}

#ifdef RLK_INIC_TX_AGGREATION_ONLY
	length = pkt->len;
#else
	pkt->data-=2;
	pkt->len+=2;
	pkt->data[0]=0x30;
	pkt->data[1]=0x52;

	length = pkt->len;

	if((length % dev->maxpacket) == 0){
		pkt->data[pkt->len] = 0;
		__skb_put(pkt, 1);
	}
#endif


	if (!(urb = RTUSB_ALLOC_URB(0))) {
		if (netif_msg_tx_err (dev))
			devdbg (dev, "no urb");
		goto drop;
	}

	entry = (struct skb_data *) pkt->cb;
	entry->urb = urb;
	entry->dev = dev;
	entry->state = tx_start;
	entry->length = length;

	usb_fill_bulk_urb (urb, dev->udev, dev->out,
			pkt->data, pkt->len, tx_complete, pkt);

	spin_lock_irqsave (&dev->txq.lock, flags);
	switch ((retval = RTUSB_SUBMIT_URB (urb))) {
	case -EPIPE:
		netif_stop_queue (net);
		break;
	default:
		if (netif_msg_tx_err (dev))
			devdbg (dev, "tx: submit urb err %d", retval);
		break;
	case 0:
		net->trans_start = jiffies;
		dev->txq.qlen--;
		__skb_queue_tail (&dev->txq, pkt);
	}
	spin_unlock_irqrestore (&dev->txq.lock, flags);

	if (retval) {
		if (netif_msg_tx_err (dev))
			devdbg (dev, "drop, code %d", retval);
drop:
		retval = NET_XMIT_SUCCESS;
		dev->stats.tx_dropped++;
		if (pkt)
			dev_kfree_skb_any (pkt);

		spin_lock_irqsave (&dev->txq.lock, flags);
		dev->txq.qlen--;
		spin_unlock_irqrestore (&dev->txq.lock, flags);

		usb_free_urb (urb);
	} else if (netif_msg_tx_queued (dev)) {
		devdbg (dev, "> tx, len %d, type 0x%x",
			length, pkt->protocol);
	}
	return retval;
}

#else
/*-------------------------------------------------------------------------*/
//	rlk_inic_usb_xmit: 	send out packets with header added.
/*-------------------------------------------------------------------------*/
int rlk_inic_usb_xmit (struct sk_buff *skb, struct net_device *net)
{
	struct usbnet		*dev = netdev_priv(net);
	int			length;
	int			retval = NET_XMIT_SUCCESS;
	struct urb		*urb = NULL;
	struct skb_data		*entry;
	unsigned long		flags;

	/* drop packet when f/w not ready */
	if(gAdapter && gAdapter->flag <= FLAG_FWFAIL_MODE){
		if (skb)
			dev_kfree_skb_any (skb);
		return NETDEV_TX_OK;
	}

	if (skb->data[12] == 0xff && skb->data[13] == 0xff)
	{
		skb->data[6] |= 0x01;
	}

	/* add header for rlk_inic device */
	skb->data-=2;
	skb->len+=2;
	skb->data[0]=0x30;
	skb->data[1]=0x52;

	length = skb->len;

	/* don't assume the hardware handles USB_ZERO_PACKET
	 *  we set  header == 0x3053 here to indicate this is a padding 0 packet
	 *  with packet size = max ep size + 1
	 */
	if ((length % dev->maxpacket) == 0) {
		if (skb_tailroom(skb)) {
			skb->data[1] = 0x53;
			skb->data[skb->len] = 0;
			__skb_put(skb, 1);
		}
	}

	if (!(urb = RTUSB_ALLOC_URB(0))) {
		if (netif_msg_tx_err (dev))
			devdbg (dev, "no urb");
		goto drop;
	}


	entry = (struct skb_data *) skb->cb;
	entry->urb = urb;
	entry->dev = dev;
	entry->state = tx_start;
	entry->length = length;

	usb_fill_bulk_urb (urb, dev->udev, dev->out,
			skb->data, skb->len, tx_complete, skb);

	spin_lock_irqsave (&dev->txq.lock, flags);
	switch ((retval = RTUSB_SUBMIT_URB (urb))) {
	case -EPIPE:
		netif_stop_queue (net);
		break;
	default:
		if (netif_msg_tx_err (dev))
			devdbg (dev, "tx: submit urb err %d", retval);
		break;
	case 0:
		net->trans_start = jiffies;
		__skb_queue_tail (&dev->txq, skb);
		if (dev->txq.qlen >= TX_QLEN (dev))
			netif_stop_queue (net);
	}
	spin_unlock_irqrestore (&dev->txq.lock, flags);

	if (retval) {
		if (netif_msg_tx_err (dev))
			devdbg (dev, "drop, code %d", retval);
drop:
		retval = NET_XMIT_SUCCESS;
		dev->stats.tx_dropped++;
		if (skb)
			dev_kfree_skb_any (skb);
		usb_free_urb (urb);
	} else if (netif_msg_tx_queued (dev)) {
		devdbg (dev, "> tx, len %d, type 0x%x",
			length, skb->protocol);
	}
	return retval;
}
#endif

/*-------------------------------------------------------------------------*/
//	usbnet_start_xmit: 	send out packets or add vlan tag.
/*-------------------------------------------------------------------------*/
static int usbnet_start_xmit(struct sk_buff *skb, struct net_device *net)
{
	int dev_id = 0;
	iNIC_PRIVATE *rt = netdev_priv(net);

	ASSERT(skb);

	if (RaCfgDropRemoteInBandFrame(skb))
		return 0;

	if (rt->RaCfgObj.bBridge == 1)
		rlk_inic_usb_xmit(skb, net);
	else	{
		skb = Insert_Vlan_Tag(rt, skb, dev_id, SOURCE_MBSS);
		if (skb){
			rlk_inic_usb_xmit(skb, net);
		}
	}
	return NETDEV_TX_OK;
}

/*-------------------------------------------------------------------------*/
//	usbnet_bh: 	tasklet (1) dispatch packets (2) allocate urb for tx queue and rx queue
/*-------------------------------------------------------------------------*/

static void usbnet_bh (unsigned long param)
{
	struct usbnet		*dev = (struct usbnet *) param;
	struct sk_buff		*skb;
	struct skb_data		*entry;

#ifdef RLK_INIC_USBDEV_GEN2
	struct net_device	*net = dev->net;
	iNIC_PRIVATE *pAd = netdev_priv(net);
#endif

	/* process done queue */
	while ((skb = skb_dequeue (&dev->done))) {
		entry = (struct skb_data *) skb->cb;
		switch (entry->state) {
		case rx_done:
			entry->state = rx_cleanup;
			rx_process (dev, skb);
			continue;
		case tx_done:
		case rx_cleanup:
			usb_free_urb (entry->urb);
			dev_kfree_skb (skb);
			continue;
		default:
			devdbg (dev,"bogus skb state %d", entry->state);
		}
	}

	/* prepare urb for rxq and txq */
#ifdef RLK_INIC_USBDEV_GEN2
	if(pAd->flag >= FLAG_READY_MODE){
#else
	if (netif_running (dev->net) 	&& netif_device_present (dev->net) && !timer_pending (&dev->delay)) {
#endif
		int rxqlen, temp, txqlen;

		temp = dev->rxq.qlen;

		rxqlen = RX_QLEN (dev);
		txqlen = TX_QLEN (dev);

		if (temp < rxqlen) {
			struct urb	*urb;
			int		i;

			// don't refill the queue all at once
			for (i = 0; i < 10 && dev->rxq.qlen < rxqlen; i++) {
				urb = RTUSB_ALLOC_URB (0);
				if (urb != NULL){
					rx_submit (dev, urb, GFP_ATOMIC | GFP_DMA);
				}
			}
			if (temp != dev->rxq.qlen && netif_msg_link (dev))
				devdbg (dev, "rxqlen %d --> %d", temp, dev->rxq.qlen);
			if (dev->rxq.qlen < rxqlen)
				tasklet_schedule (&dev->bh);
		}
		if (dev->txq.qlen < txqlen){
#if defined(RLK_INIC_SOFTWARE_AGGREATION) || defined(RLK_INIC_TX_AGGREATION_ONLY)
			if(dev->aggr_q.qlen !=0)
				rlk_inic_usb_xmit(NULL, dev->net);
#else
			netif_wake_queue (dev->net);
#endif
		}
	}

}

/*-----------------------------------------------------------------------------*/
//	rt_disconnect: 	called when usb disconnect
//		(1) if called by "ifconfig ra# up", put original usb_device
//		(2) if called after firmware running, unregister netdev, free iNIC private, put usb_device
/*-----------------------------------------------------------------------------*/


/* PACE_PATCH */
static struct workqueue_struct *my_wq;
static struct work_struct work;
static void  reset_wifi( struct work_struct *work)
{
	printk("About to reset\n");
	msleep(2000);
       *(volatile unsigned int*)(0xFE010000+8)=0x2;
        msleep(200);
       *(volatile unsigned int*)(0xFE010000+4)=0x2;
	printk("Resetted\n");
	up(&wifi_reset_chip_sem);

}
/* END PACE_PATCH */
void rt_disconnect (struct usb_device *xdev, struct usb_interface *intf, void *ptr)
{
	struct usbnet		*dev;
	iNIC_PRIVATE *pAd = gAdapter;
	struct usb_driver		*driver;
/*   PACE_PATCH  */   
	 down(&wifi_reset_chip_sem);
/*   END PACE_PATCH   */
	if(pAd == NULL){
		printk("Error! pAd == NULL\n");
		/*   PACE_PATCH  */   
		up(&wifi_reset_chip_sem);
		/*   END PACE_PATCH   */
		return;
	}

	if(pAd->flag == FLAG_OPEN_MODE){ /* call by ifconfig ra# up */
		if(xdev)
			usb_put_dev (xdev);
		/*   PACE_PATCH  */   
		up(&wifi_reset_chip_sem);
		/*   END PACE_PATCH   */
		return;
	}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
	if(pAd->sbdev.udev != xdev){
		//printk("Error! usb_device not match\n");
		/*   PACE_PATCH  */   
		up(&wifi_reset_chip_sem);
		/*   END PACE_PATCH   */
		return;
	}
#else
	if(pAd->sbdev.intf != intf){
		//printk("Error! interface not match\n");
		/*   PACE_PATCH  */   
		up(&wifi_reset_chip_sem);
		/*   END PACE_PATCH   */
		return;
	}
#endif

	if (pAd->RaCfgObj.opmode == 0)
	{
#if IW_HANDLER_VERSION < 7
		pAd->dev->get_wireless_stats = NULL;
#endif
		pAd->dev->wireless_handlers = NULL;
	}

	pAd->flag = FLAG_INIT_MODE;

	unregister_netdev(pAd->dev);

	/* we don't hold rtnl here ... */
	flush_scheduled_work ();

	dev = &(pAd->sbdev);

	// devdbg(dev, "txq len=%d, rxq len=%d\n", dev->txq.qlen, dev->rxq.qlen);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
	if(dev->data && dev->control){
		driver = driver_of(intf);
		usb_driver_release_interface(driver, dev->data);
		usb_driver_release_interface(driver, dev->control);
	}
#endif

	RaCfgExit(pAd);
	gAdapter = NULL;

	free_netdev(pAd->dev);

	if(xdev)
		usb_put_dev (xdev);
    my_wq = create_workqueue("my_queue");
	{
/*   PACE_PATCH  */   
	
      INIT_WORK( &work, reset_wifi );
	  queue_work( my_wq, &work);
	}
/*   END PACE_PATCH   */
	return;

}

static struct net_device_stats *rt_net_get_stats(struct net_device *netdev)
{
	struct usbnet	*dev;

	dev = netdev_priv(netdev);

	return &dev->stats;
}


/*---------------------------------------------------------------------------*/
//	phase1_probe: 	called when usb device probe (not upload firmware yet)
//		(1) create net_dev
//		(2) create rlk_inic private data
//		(3) upload rlk_inic firmware
/*---------------------------------------------------------------------------*/
static int phase1_probe (struct usb_device *xdev, struct usb_interface *udev, const struct usb_device_id *prod)
{
	struct net_device		*net;
	struct usbnet			*dev;
	iNIC_PRIVATE       *pAd = (iNIC_PRIVATE *) NULL;
	u8 node_id[6] = {0x00, 0x43, 0x0C, 0x01, 0x23, 0x45}; /* Default mac */

	/* (1) create net_dev */
	net = alloc_etherdev(sizeof(iNIC_PRIVATE));

 	SET_MODULE_OWNER(net);

	dev = netdev_priv(net);
	skb_queue_head_init (&dev->rxq);
	skb_queue_head_init (&dev->txq);
	skb_queue_head_init (&dev->done);
#ifdef RLK_INIC_SOFTWARE_AGGREATION
	skb_queue_head_init (&dev->aggr_q);
#endif
	dev->bh.func = usbnet_bh;
	dev->bh.data = (unsigned long) dev;
	dev->delay.function = usbnet_bh;
	dev->delay.data = (unsigned long) dev;
	init_timer (&dev->delay);

	dev->udev = xdev;
	dev->intf = udev;
	dev->net = net;
	dev->hard_mtu = net->mtu + net->hard_header_len;

	if (!dev->rx_urb_size)
		dev->rx_urb_size = dev->hard_mtu;

	RLK_STRCPY (net->name, "ra%d");
	memcpy (net->dev_addr, node_id, sizeof node_id);
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,28)
	net->hard_start_xmit = usbnet_start_xmit;
	net->open = usbnet_open;
	net->stop = usbnet_stop;
	net->do_ioctl = rlk_inic_ioctl;
#else
	net->netdev_ops = &netdev_ops;
#endif
	net->get_stats = rt_net_get_stats;
	if(csumoff == 1)
		net->features |= NETIF_F_IP_CSUM;

	/* (2) create RLK_INIC private data structure */
	pAd = netdev_priv(net);
	gAdapter = pAd;
	pAd->dev = net;
	pAd->flag = FLAG_INIT_MODE;
	spin_lock_init (&pAd->lock);
	RaCfgInit(pAd, net, mac, mode, bridge, csumoff);
	SET_NETDEV_DEV(net, xdev->dev.parent);
	register_netdev(pAd->dev);

	/* (3) probe iNIC bootcode info */
	rlk_inic_udev = pAd->sbdev.udev;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
	usb_get_dev(rlk_inic_udev);
#else
	rlk_inic_udev = usb_get_dev(rlk_inic_udev);
#endif
	RLK_INICProbePostConfig(pAd,0);

	pAd->flag = FLAG_READY_MODE;

#ifdef DBG
//	dev->msg_enable = 0xFFFF;
#endif

	return 0;

}

/*-------------------------------------------------------------------------*/
static int rt_generic_bind(struct usbnet *dev, struct usb_interface *intf);

/*---------------------------------------------------------------------------*/
//	phase2_probe: 	called when usb device probe (firmware is running)
//		(1) rt_bind to bind CDC ether protocol
//		(2) start  tasklet to handle packets
/*---------------------------------------------------------------------------*/
static int phase2_probe (struct usb_device *xdev, struct usb_interface *udev, const struct usb_device_id *prod)
{
	struct usbnet			*dev;
	struct net_device		*net;
	int				status;

	net = gAdapter->dev;
	dev = netdev_priv(net);

	usb_get_dev (xdev);
	status = -ENOMEM;

	dev->udev = xdev;
	dev->intf = udev;
	rlk_inic_udev = xdev;

	status = rt_generic_bind (dev, udev);
	if (status < 0)
		goto out;

	dev->maxpacket = usb_maxpacket (dev->udev, dev->out, 1);
	/* maybe the remote can't receive an Ethernet MTU */
	if (net->mtu > (dev->hard_mtu - net->hard_header_len))
		net->mtu = dev->hard_mtu - net->hard_header_len;
	dev->rx_urb_size = dev->hard_mtu;

	// start as if the link is up
	netif_device_attach (net);

	// delay posting reads until we're fully open
	tasklet_schedule (&dev->bh);

	return 0;

out:
	usb_put_dev(xdev);
	return status;
}

/*---------------------------------------------------------------------------*/
//	flush_Adapter_netdev: 	remove Adapter and Netdevice if upload code fail.
/*---------------------------------------------------------------------------*/
void flush_Adapter_netdev(void)
{

	if(gAdapter==NULL)
		return;
	RaCfgStateReset(gAdapter);
	unregister_netdev(gAdapter->dev);
	RaCfgExit(gAdapter);
	free_netdev(gAdapter->dev);
	gAdapter = NULL;
}
/*---------------------------------------------------------------------------*/
//	rt_probe: 	usb probe entry point
/*---------------------------------------------------------------------------*/
int rt_probe (struct usb_device *xdev, struct usb_interface *udev, const struct usb_device_id *prod)
{

/*   PACE_PATCH     Wait for reset completed  */
	down(&wifi_reset_chip_sem);
	up(&wifi_reset_chip_sem);
/*   END PACE_PATCH   */
#ifdef RLK_INIC_USBDEV_GEN2

 	if((prod->idProduct == USB_PID) && (prod->idVendor==0x148F)){
		if(gAdapter != NULL){
			if( gAdapter->flag == FLAG_FWFAIL_MODE){
				flush_Adapter_netdev();
			}else{
				return -1;
			}
		}
		phase1_probe(xdev, udev, prod);
		return phase2_probe(xdev, udev, prod);
 	}

	return -1;

#else

	if((prod->idProduct == 0xFFFF) && (prod->idVendor==0x148f)){
		if(gAdapter != NULL){
			if( gAdapter->flag == FLAG_FWFAIL_MODE){
				flush_Adapter_netdev();
			}else{
				return -1;
			}
		}
		return phase1_probe(xdev, udev, prod);
	}else{
		if(gAdapter == NULL){
			printk("Error: Boot from Firmware idProduct=%x idVendor=%x\n", prod->idProduct, prod->idVendor);
			return -1;
		}
		return phase2_probe(xdev, udev, prod);
	}
#endif //RLK_INIC_USBDEV_GEN2


}

/*---------------------------------------------------------------------------*/
// rt_generic_bind:
// 	probes control interface, claims data interface, collects the bulk
// 	endpoints, activates data interface, sets MTU.
/*---------------------------------------------------------------------------*/
int rt_generic_bind(struct usbnet *dev, struct usb_interface *intf)
{
#ifdef RLK_INIC_USBDEV_GEN2
	int				status=0;
	struct usb_driver		*driver;

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
	dev->control = intf;
	driver = driver_of(dev->control);
	dev->data = intf;
	dev->hard_mtu = 1518;
#else
	dev->control = dev->udev->config->interface;
	dev->data = dev->udev->config->interface;
	dev->hard_mtu = 1518;
#endif

#else
	u8				*buf;
	int				status=0, len=0;
	struct usb_driver		*driver;

/* there should be 2 usb_interface (control, data) */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
	buf  = dev->udev->config->interface->altsetting->extra;
	len  = dev->udev->config->interface->altsetting->extralen;
	dev->control = dev->udev->config->interface;
#else
	buf  = intf->cur_altsetting->extra;
	len  = intf->cur_altsetting->extralen;
	dev->control = intf;
	driver = driver_of(dev->control);
#endif

	/* (1) find out control and data interface */
	len = len - buf[0];
	buf = buf + buf[0];

	if(len < 5)
		goto bad_desc;
	else{
		dev->control = usb_ifnum_to_if(dev->udev, buf[3]);
		dev->data = usb_ifnum_to_if(dev->udev, buf[4]);
		//devdbg(dev,"control=%x, data=%x\n", dev->control, dev->data);
	}

	/* (2) find out wMaxSegmentSize */
	len = len - buf[0];
	buf = buf + buf[0];

	if(len < 10)
		goto bad_desc;
	else{
#ifdef __BIG_ENDIAN
		u16	tmp = (buf[8]<<8) + buf[9];
#else
		u16	tmp = (buf[9]<<8) + buf[8];
#endif
		dev->hard_mtu = le16_to_cpu(tmp);
	}

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
	status = usb_driver_claim_interface(driver, dev->data, dev);

	if (status < 0)
		return status;
#endif

#endif // RLK_INIC_USBDEV_GEN2

	status = rt_get_endpoints(dev, dev->data);

	if (status < 0) {
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,5,0)
		/* ensure immediate exit from usbnet_disconnect */
		usb_driver_release_interface(driver, dev->data);
#endif
		return status;
	}

	return 0;

bad_desc:
	devdbg(dev,"bad %s descriptors\n", DRV_NAME);
	return -ENODEV;
}

/*-------------------------------------------------------------------------*/

static const struct usb_device_id	products [] = {
#ifdef RLK_INIC_USBDEV_GEN2
	{USB_DEVICE(0x148F,USB_PID)},
#else
	/* 	RLK_INIC BOOTCODE  */
	{USB_DEVICE(0x148F,0xFFFF)},
	/* 	RLK_INIC FIRMWARE*/
	{USB_DEVICE_AND_INT_INFO(0x148F, USB_PID, USB_CLASS_VENDOR_SPEC, USB_SUBCLASS_VENDOR, USB_PROTO_VENDOR)},
	/* 	END	 */
#endif
	{ },
};

MODULE_DEVICE_TABLE(usb, products);


#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
/**************************************************************************/
//tested for kernel 2.4 series
/**************************************************************************/
static void *rtusb_probe(struct usb_device *dev, UINT interface, const struct usb_device_id *id)
{
	if(rt_probe(dev, NULL, id)<0)
		return 0;
	else
		return 1;
}

//Disconnect function is called within exit routine
static void rtusb_disconnect(struct usb_device *dev, void *ptr)
{
	rt_disconnect(dev, NULL, ptr);
}

static struct usb_driver rlk_inic_drv = {
	name:"rlk_inic_drv",
	probe:rtusb_probe,
	id_table:products,
	disconnect:rtusb_disconnect,
};

#else
/**************************************************************************/
//tested for kernel 2.6series
/**************************************************************************/
int rtusb_probe (struct usb_interface *udev, const struct usb_device_id *prod)
{
	struct usb_device *xdev = interface_to_usbdev (udev);
	return rt_probe(xdev, udev, prod);
}

void rtusb_disconnect (struct usb_interface *intf)
{
	struct usb_device *xdev = interface_to_usbdev (intf);
	rt_disconnect(xdev, intf, NULL);
}

extern void close_all_interfaces(struct net_device *);

static int rtusb_suspend(struct usb_interface *intf, pm_message_t state)
{
/*   PACE_PATCH     Wait for reset completed  */
	
	down(&wifi_reset_chip_sem);
	up(&wifi_reset_chip_sem);
/*   END PACE_PATCH   */
#ifdef RLK_INIC_USBDEV_GEN2

	printk("rt_usb_suspend\n");

#else
	struct usbnet	*dev;
	iNIC_PRIVATE *pAd = gAdapter;

	dev = &(pAd->sbdev);

	if(intf != dev->control)
		return 0;

	close_all_interfaces(dev->net);
	netif_device_detach (dev->net);
#endif
	return 0;
}
static int rtusb_resume(struct usb_interface *intf)
{
	return 0;
}

static struct usb_driver rlk_inic_drv = {
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,15)
	.owner = THIS_MODULE,
#endif
	.name =		"rlk_inic_drv",
	.id_table =	products,
	.probe =	rtusb_probe,
	.disconnect =	rtusb_disconnect,
	.suspend =	rtusb_suspend,
	.resume =	rtusb_resume,
};

#endif // LINUX_VERSION_CODE //


static int __init cdc_init(void)
{
	/*   PACE_PATCH  */
	sema_init(&wifi_reset_chip_sem, 1);
	/*   END PACE_PATCH   */
	return usb_register(&rlk_inic_drv);
}

module_init(cdc_init);

static void __exit cdc_exit(void)
{
	unsigned char usb_ep0_pData[64];

	if(gAdapter != NULL)
		rlk_inic_racfg((UINT *)usb_ep0_pData,RACFG_CMD_BOOT_RESET);

	if(gAdapter != NULL && gAdapter->flag == FLAG_FWFAIL_MODE)
		flush_Adapter_netdev();

	usb_deregister(&rlk_inic_drv);
}

module_exit(cdc_exit);

MODULE_AUTHOR("iNIC@Ralink");
MODULE_DESCRIPTION(CHIP_NAME " " INTERFACE " Ethernet devices");

/*NS pace: added GPL licence*/
MODULE_LICENSE("GPL");
