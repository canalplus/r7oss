/*******************************************************************************
  This is the driver for the SoC Virtual Ethernet

	Copyright(C) 2012 STMicroelectronics Ltd

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

#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/skbuff.h>
#include <linux/ethtool.h>
#include <linux/if_ether.h>
#include <linux/crc32.h>
#include <linux/mii.h>
#include <linux/if.h>
#include <linux/if_vlan.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/prefetch.h>
#include <linux/etherdevice.h>
#include <linux/netdevice.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include "isve.h"
#include "docsis_glue.h"

#define ISVE_DEFAULT_QUEUE_SIZE	32
#define ISVE_XMIT_THRESH	8
#define	ISVE_IFNAME	"if"

static int debug = -1;		/* -1: default, 0: no output, 16:  all */
module_param(debug, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(debug, "Message Level (0: no output, 16: all)");

#define TX_TIMEO 10000
static int watchdog = TX_TIMEO;
module_param(watchdog, int, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(watchdog, "Transmit timeout in milliseconds");

static const u32 default_msg_level = (NETIF_MSG_DRV | NETIF_MSG_PROBE |
				      NETIF_MSG_LINK | NETIF_MSG_IFUP |
				      NETIF_MSG_IFDOWN | NETIF_MSG_TIMER);

static inline unsigned int isve_tx_avail(unsigned int dirty_tx,
				unsigned int cur_tx,
				unsigned int tx_size)
{
	return dirty_tx + tx_size - cur_tx - 1;
}

static void print_pkt(unsigned char *buf, int len)
{
	int j;
	pr_info("len = %d byte, buf addr: 0x%p\n", len, buf);
	for (j = 0; j < len; j++) {
		if ((j % 16) == 0)
			pr_info("\n%03x:", j);
		pr_info(" %02x", buf[j]);
	}
	pr_info("\n");
}

static void isve_tx_fault(struct net_device *dev);

/**
 * isve_release_rings: release the rx/tx buffers
 * @priv: private structure
 * Description: it de-allocates the TX / RX buffers and un-maps the
 * DMA addresses.
 */
static void isve_release_rings(struct isve_priv *priv)
{
	int i;
	struct device *dev = priv->device;

	dev_dbg(priv->device, "free transmit resources\n");
	for (i = 0; i < ISVE_DEFAULT_QUEUE_SIZE; i++) {
		dma_addr_t d;

		d = priv->tx_desc[i].buf_addr;
		if (d)
			dma_unmap_single(dev, d, ISVE_BUF_LEN, DMA_TO_DEVICE);

		if (priv->tx_desc[i].buffer != NULL)
			kfree(priv->tx_desc[i].buffer);
	}
	dev_dbg(priv->device, "free receive resources\n");
	for (i = 0; i < ISVE_DEFAULT_QUEUE_SIZE; i++) {
		dma_addr_t d;

		d = priv->rx_desc[i].buf_addr;
		if (d)
			dma_unmap_single(dev, d, ISVE_BUF_LEN, DMA_FROM_DEVICE);

		if (priv->rx_desc[i].buffer != NULL)
			kfree(priv->rx_desc[i].buffer);
	}
}

/**
 * isve_alloc_rings: allocate the rx/tx buffers
 * @priv: private structure
 * Description: it allocates the TX / RX resources (buffers etc).
 * Driver maintains two lists of buffers for managing the trasmission
 * and the reception processes.
 * Streaming DMA mappings are used to DMA transfer,
 */
static int isve_alloc_rings(struct isve_priv *priv)
{
	struct device *device = priv->device;
	int i, ret = 0;

	priv->rx_desc = kcalloc(ISVE_DEFAULT_QUEUE_SIZE,
				sizeof(struct isve_desc), GFP_KERNEL);
	if (priv->rx_desc == NULL)
		return -ENOMEM;

	for (i = 0; i < ISVE_DEFAULT_QUEUE_SIZE; i++) {
		void *buf;
		dma_addr_t d;

		buf = kzalloc(ISVE_BUF_LEN, GFP_KERNEL);
		if (buf == NULL) {
			ret = -ENOMEM;
			goto err;
		}

		d = dma_map_single(priv->device, buf, ISVE_BUF_LEN,
				   DMA_FROM_DEVICE);
		if (dma_mapping_error(priv->device, d)) {
			kfree(buf);
			ret = -ENOMEM;
			goto err;
		}

		priv->dfwd->init_rx_fifo(priv->ioaddr_dfwd, d);

		priv->rx_desc[i].buffer = buf;
		priv->rx_desc[i].buf_addr = d;

		if (netif_msg_hw(priv))
			dev_info(device, "dwfd - %d: buffer 0x%p addr 0x%x\n",
				 i, priv->rx_desc[i].buffer,
				 priv->rx_desc[i].buf_addr);
	}

	priv->cur_rx = 0;

	/* Allocate the transmit resources */
	priv->tx_desc = kcalloc(ISVE_DEFAULT_QUEUE_SIZE,
				sizeof(struct isve_desc), GFP_KERNEL);
	if (priv->tx_desc == NULL) {
		ret = -ENOMEM;
		goto err;
	}

	for (i = 0; i < ISVE_DEFAULT_QUEUE_SIZE; i++) {
		void *buf;
		dma_addr_t d;

		buf = kzalloc(ISVE_BUF_LEN, GFP_KERNEL);
		if (buf == NULL) {
			ret = -ENOMEM;
			goto err;
		}

		d = dma_map_single(priv->device, buf, ISVE_BUF_LEN,
				   DMA_TO_DEVICE);
		if (dma_mapping_error(priv->device, d)) {
			kfree(buf);
			ret = -ENOMEM;
			goto err;
		}

		priv->tx_desc[i].buffer = buf;
		priv->tx_desc[i].buf_addr = d;

		if (netif_msg_hw(priv))
			dev_info(device, "upiim - %d: buffer 0x%p, addr 0x%x\n",
				 i, priv->tx_desc[i].buffer,
				 priv->tx_desc[i].buf_addr);
	}
	priv->cur_tx = 0;
	priv->dirty_tx = 0;

	return ret;
err:
	isve_release_rings(priv);
	return ret;
}

/**
 * isve_tx: claim and freee the tx resources
 * @priv: private driver structure
 * Description: this actually is the UPIIM interrupt handler to read the free
 * queue address and move the dirty SW entry used for managing the process.
 * It also restarts the tx queue in case of it was stopped and according
 * to the threshold value.
 */
static void isve_tx(struct isve_priv *priv)
{
	unsigned int freed_addr;
	struct device *device = priv->device;

	spin_lock(&priv->tx_lock);

	do {
		freed_addr = priv->upiim->freed_tx_add(priv->ioaddr_upiim);
		if (freed_addr) {
			unsigned int entry = priv->dirty_tx %
				ISVE_DEFAULT_QUEUE_SIZE;

			if (netif_msg_hw(priv))
				dev_dbg(device, "entry %d: freed addr 0x%x\n",
					entry, freed_addr);
			priv->dirty_tx++;
		}
	} while (freed_addr > 0);

	if (netif_queue_stopped(priv->dev) &&
	    (isve_tx_avail(priv->dirty_tx, priv->cur_tx,
			   ISVE_DEFAULT_QUEUE_SIZE) > ISVE_XMIT_THRESH)) {
		if (netif_msg_hw(priv))
			dev_dbg(device, "restart tx\n");
		netif_wake_queue(priv->dev);
	}

	spin_unlock(&priv->tx_lock);
}

/**
 * isve_dwfd_isr: isve interrupt handler for the DWFD queue
 * @irq: interrupt number
 * @dev_id: device id
 * Description: this is the interrupt handler. It schedules NAPI poll method
 * to manage the reception (downstream) of the incoming frames.
 */
static irqreturn_t isve_dwfd_isr(int irq, void *dev_id)
{
	int ret;
	struct net_device *dev = (struct net_device *)dev_id;
	struct isve_priv *priv = netdev_priv(dev);

	if (unlikely(!dev))
		return IRQ_NONE;

	ret = priv->dfwd->isr(priv->ioaddr_dfwd, &priv->xstats);
	if (likely(ret == out_packet)) {
		priv->dfwd->enable_irq(priv->ioaddr_dfwd, 0);
		napi_schedule(&priv->napi);
	} else if (unlikely(ret == out_packet_dropped)) {
		dev->stats.rx_errors++;
		priv->dev->stats.rx_dropped++;
	}

	return IRQ_HANDLED;
}

/**
 * isve_upiim_isr: isve interrupt handler for the upstream queue
 * @irq: interrupt number
 * @dev_id: device id
 * Description: this is the interrupt handler. It calls the
 * UPIIM specific handler.
 */
static irqreturn_t isve_upiim_isr(int irq, void *dev_id)
{
	int ret;
	struct net_device *dev = (struct net_device *)dev_id;
	struct isve_priv *priv = netdev_priv(dev);

	if (unlikely(!dev))
		return IRQ_NONE;

	/* Clean tx resources */
	ret = priv->upiim->isr(priv->ioaddr_upiim, &priv->xstats);
	if (likely(ret == in_packet))
		isve_tx(priv);
	else if (unlikely(ret == in_packet_dropped))
		isve_tx_fault(dev);

	return IRQ_HANDLED;
}

/**
 * isve_provide_mac_addr: to provide the MAC address to the virtual iface
 * @priv: private structure
 * Description: verify if the MAC address is valid (that could be passed
 * by using other supports, e.g. nwhwconfig).
 * In case of no MAC address is passed it generates a random one
 */
static void isve_provide_mac_addr(struct isve_priv *priv)
{
	if (!is_valid_ether_addr(priv->dev->dev_addr))
		random_ether_addr(priv->dev->dev_addr);

	dev_info(priv->device, "\t%s: MAC address %pM\n", priv->dev->name,
		 priv->dev->dev_addr);
}

static void isve_check_dfwd_fifo(struct isve_priv *priv)
{
	int i;
	u32 status;

	/* dfwd fifo pointer needs to be reprogrammed as free-queue per queue
	 * no will be empty on eCM-reset condition.
	 * To detect the eCM-reset condition. we need to check if free-fifo and
	 * used-fifos are empty.
	 */
	status = priv->dfwd->dfwd_fifo_empty(priv->ioaddr_dfwd);
	if (status) {
		for (i = 0; i < ISVE_DEFAULT_QUEUE_SIZE; i++) {
			priv->dfwd->init_rx_fifo(priv->ioaddr_dfwd,
					priv->rx_desc[i].buf_addr);
		}
		priv->cur_rx = 0;
	}
}

/**
 *  isve_open: open entry point of the driver
 *  @dev : pointer to the device structure.
 *  Description:
 *  This function is the open entry point of the driver.
 *  It sets the MAC address, inits the Downstream and Upstream Cores,
 *  enables NAPI and starts the processes.
 *  Return value:
 *  0 on success and an appropriate (-)ve integer as defined in errno.h
 *  file on failure.
 */
static int isve_open(struct net_device *dev)
{
	struct isve_priv *priv = netdev_priv(dev);
	int ret = 0;

	isve_provide_mac_addr(priv);

	isve_check_dfwd_fifo(priv);

	ret = request_irq(priv->irq_ds, isve_dwfd_isr, IRQF_SHARED, dev->name,
			  dev);
	if (unlikely(ret < 0)) {
		dev_err(priv->device, "fail allocating DWFD IRQ %d (err %d)\n",
			priv->irq_ds, ret);
		return ret;
	}
	ret = request_irq(priv->irq_us, isve_upiim_isr, IRQF_SHARED, dev->name,
			  dev);
	if (unlikely(ret < 0)) {
		dev_err(priv->device, "fail allocating UPIIM IRQ %d (err %d)\n",
			priv->irq_ds, ret);
		free_irq(priv->irq_ds, dev);
		return ret;
	}

	netif_carrier_on(dev);

	priv->dfwd->dfwd_init(priv->ioaddr_dfwd, priv->hw_rem_hdr);
	priv->upiim->upiim_init(priv->ioaddr_upiim);

	/* Extra statistics */
	memset(&priv->xstats, 0, sizeof(struct isve_extra_stats));

	if (netif_msg_hw(priv)) {
		priv->dfwd->dump_regs(priv->ioaddr_dfwd);
		priv->upiim->dump_regs(priv->ioaddr_upiim);
	}

	napi_enable(&priv->napi);
	netif_start_queue(dev);

	/* Enable own queue in the docsis glue  */
	/*docsis_en_queue(DOCSIS_USIIM, true, priv->queue_number);*/
	docsis_en_queue(DOCSIS_DSFWD, true, priv->queue_number);

	dev_dbg(priv->device, "open exits fine...\n");
	return ret;
}

/**
 *  isve_release - close entry point of the driver
 *  @dev : device pointer.
 *  Description:
 *  This is the stop entry point of the driver.
 */
static int isve_release(struct net_device *dev)
{
	struct isve_priv *priv = netdev_priv(dev);

	/*docsis_en_queue(DOCSIS_USIIM, false, priv->queue_number);*/
	docsis_en_queue(DOCSIS_DSFWD, false, priv->queue_number);

	priv->dfwd->enable_irq(priv->ioaddr_dfwd, 0);
	priv->upiim->enable_irq(priv->ioaddr_dfwd, 0);

	netif_carrier_off(dev);
	netif_stop_queue(dev);
	napi_disable(&priv->napi);

	/* Free the IRQ lines */
	free_irq(priv->irq_us, dev);
	free_irq(priv->irq_ds, dev);

	return 0;
}

/**
 * isve_xmit: transmit function
 * @skb : the socket buffer
 * @dev : device pointer
 * Description : Tx entry point of the driver. Mainly it gets the skb and
 * fills the buffer to be passed to the DMA queue. Each pointer has to be
 * aligned to a 32-byte boundary so the skb->data is copied and linearized by
 * skb_copy_and_csum_dev callback.
 */
static netdev_tx_t isve_xmit(struct sk_buff *skb, struct net_device *dev)
{
	unsigned int len;
	struct isve_priv *priv = netdev_priv(dev);
	int entry = priv->cur_tx % ISVE_DEFAULT_QUEUE_SIZE;
	struct isve_desc *p = priv->tx_desc + entry;
	unsigned long flags;
	int pending;

	if (unlikely(skb_padto(skb, ETH_ZLEN)))
		return NETDEV_TX_OK;

	spin_lock_irqsave(&priv->tx_lock, flags);
	len = skb->len;

	if (netif_msg_hw(priv))
		dev_info(priv->device, "\nxmit: skb %p skb->data %p len %d\n",
			 skb, skb->data, len);

	dma_sync_single_for_cpu(priv->device, p->buf_addr, skb->len,
				DMA_TO_DEVICE);
	skb_copy_and_csum_dev(skb, p->buffer);
	dma_sync_single_for_device(priv->device, p->buf_addr, skb->len,
				   DMA_TO_DEVICE);

	priv->cur_tx++;
	dev->stats.tx_packets++;
	dev->stats.tx_bytes += len;

	if (netif_msg_pktdata(priv)) {
		dev_info(priv->device, "entry %d, dirty %d, cur_tx %d\n", entry,
			 priv->dirty_tx,
			 priv->cur_tx % ISVE_DEFAULT_QUEUE_SIZE);

		dev_info(priv->device, "dma 0x%x, dma data buf %p, len %d\n",
			 p->buf_addr, p->buffer, len);
		print_pkt(p->buffer, len);
	}


	dev_kfree_skb(skb);

	pending = isve_tx_avail(priv->dirty_tx, priv->cur_tx,
				ISVE_DEFAULT_QUEUE_SIZE);
	if (pending < ISVE_XMIT_THRESH) {
		dev_info(priv->device, "xmit: stop transmitted packets %d\n",
			 pending);
		netif_stop_queue(dev);
	}

	/* Fill HW pointers and len in the right order and trasmit ! */
	priv->upiim->fill_tx_add(priv->ioaddr_upiim, p->buf_addr, len);

	spin_unlock_irqrestore(&priv->tx_lock, flags);

	return NETDEV_TX_OK;
}

/**
 * isve_rx: receive function.
 * @priv : private structure
 * @limit : budget used for mitigation
 * Description: it handles the receive process and gets the incoming frames
 * from the downstream queue.
 */
static int isve_rx(struct isve_priv *priv, int limit)
{
	unsigned int count = 0;
	unsigned int len;
	void __iomem *ioaddr = priv->ioaddr_dfwd;
	struct sk_buff *skb;
	struct net_device *ndev = priv->dev;
	struct device *dev = priv->device;

	while (count < limit) {
		struct dfwd_fifo fifo;
		unsigned int entry = priv->cur_rx % ISVE_DEFAULT_QUEUE_SIZE;
		struct isve_desc *p = priv->rx_desc + entry;
		u32 addr;

		if (netif_msg_pktdata(priv))
			dev_info(dev, "Entry %d curr %d buf 0x%p, addr 0x%x\n",
				 entry, priv->cur_rx, p->buffer, p->buf_addr);

		fifo = priv->dfwd->get_rx_fifo_status(ioaddr);
		if (!fifo.used)
			break;

		/* Get the len before */
		len = priv->dfwd->get_rx_len(ioaddr);
		/* Now get the address, the entry is removed from the FIFO */
		addr = priv->dfwd->get_rx_used_add(ioaddr);
		if (netif_msg_hw(priv)) {
			dev_info(dev, "DFWD: used %d free %d\n", fifo.used,
				 fifo.free);
			dev_info(dev, "Regs: addr 0x%x, len  %d\n", addr, len);
		}

		/* The address should be the head of the software FIFO queue
		 * if HW is using the FIFO in the order SW has set it up.
		 */
		if (unlikely(addr != p->buf_addr))
			dev_err(dev, "rx used fifo not aligned with SW fifo\n");

		if (unlikely(len <= priv->skip_hdr))
			dev_err(dev, "len %d less than skip header bytes=%d\n",
				len, priv->skip_hdr);

		skb = netdev_alloc_skb_ip_align(priv->dev, len);
		if (unlikely(skb == NULL)) {
			dev_warn(dev, "low memory, packet dropped.\n");
			ndev->stats.rx_dropped++;
			break;
		}

		dma_sync_single_for_cpu(priv->device, p->buf_addr, len,
					DMA_FROM_DEVICE);
		/* For some downstream queues it could be needed to skip some
		 * bytes (DOCSIS Header) from the incoming frames.
		 * CRC is removed and the len is properly reported by the HW.
		 * We only need to remove the extra bytes added to fix the
		 * alignment (for example).
		 */
		skb_copy_to_linear_data(skb, p->buffer + priv->skip_hdr,
					len - priv->skip_hdr);
		dma_sync_single_for_device(priv->device, p->buf_addr, len,
					   DMA_FROM_DEVICE);
		skb_put(skb, len);

		if (netif_msg_pktdata(priv)) {
			dev_info(dev, "Frame received (%dbytes)\n",
			skb->len);
			print_pkt(skb->data, skb->len);
		}

		skb->protocol = eth_type_trans(skb, ndev);
		skb_checksum_none_assert(skb);
		netif_receive_skb(skb);

		priv->dfwd->init_rx_fifo(priv->ioaddr_dfwd, p->buf_addr);

		ndev->stats.rx_packets++;
		ndev->stats.rx_bytes += len;

		count++;
		priv->cur_rx++;
	}
	return count;
}

/**
 *  isve_poll - isve poll method (NAPI)
 *  @napi : pointer to the napi structure.
 *  @budget :napi maximum number of packets that can be received.
 *  Description : this function implements the the reception process.
 */
static int isve_poll(struct napi_struct *napi, int budget)
{
	struct isve_priv *priv = container_of(napi, struct isve_priv, napi);
	int work_done = 0;

	work_done = isve_rx(priv, budget);

	if (work_done < budget) {
		unsigned long flags;
		spin_lock_irqsave(&priv->rx_lock, flags);
		napi_complete(napi);
		priv->dfwd->enable_irq(priv->ioaddr_dfwd, 1);
		spin_unlock_irqrestore(&priv->rx_lock, flags);
	}
	return work_done;
}

/**
 *  isve_tx_fault: manage tx fault (e.g. watchdog timer)
 *  @dev : Pointer to net device structure
 *  Description: this function is called when a packet transmission fails to
 *  complete within a reasonable tmrate. The driver will mark the error in the
 *  netdev structure and arrange for the device to be reset to a sane state
 *  in order to transmit a new packet.
 */
static void isve_tx_fault(struct net_device *dev)
{
	struct isve_priv *priv = netdev_priv(dev);
	unsigned long flags;
	int i;

	dev_dbg(priv->device, "transmit fault\n");

	spin_lock_irqsave(&priv->tx_lock, flags);

	netif_stop_queue(priv->dev);

	priv->dev->stats.tx_errors++;
	priv->upiim->enable_irq(priv->ioaddr_upiim, 0);

	for (i = 0; i < ISVE_DEFAULT_QUEUE_SIZE; i++) {
		dma_addr_t d;

		d = priv->tx_desc[i].buf_addr;
		if (d)
			dma_unmap_single(priv->device, d, ISVE_BUF_LEN,
					 DMA_TO_DEVICE);
	}

	priv->upiim->upiim_init(priv->ioaddr_upiim);

	netif_start_queue(priv->dev);

	spin_unlock_irqrestore(&priv->tx_lock, flags);
}

/**
 *  isve_change_mtu: to change the MTU
 *  @ndev : Pointer to net device structure
 *  @new_mtu: new MTU
 *  Description: this is to change the MTU that cannot be bigger than the
 *  maximum buffer size (2048bytes).
 */
static int isve_change_mtu(struct net_device *ndev, int new_mtu)
{

	if (netif_running(ndev))
		return -EBUSY;

	if (new_mtu > ISVE_BUF_LEN)
		return -EINVAL;

	ndev->mtu = new_mtu;
	netdev_update_features(ndev);

	return 0;
}

static const struct net_device_ops isve_netdev_ops = {
	.ndo_open = isve_open,
	.ndo_start_xmit = isve_xmit,
	.ndo_stop = isve_release,
	.ndo_change_mtu = isve_change_mtu,
	.ndo_tx_timeout = isve_tx_fault,
	.ndo_set_mac_address = eth_mac_addr,
};

/**
 * isve_pltfr_probe: probe function.
 * @pdev: platform device pointer
 * Description: platform_device probe function. It is to allocate the necessary
 * resources and setup the net device.
 */
static int isve_pltfr_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct net_device *ndev;
	struct resource *res;
	struct isve_priv *priv = NULL;
	void __iomem *addr_dfwd, *addr_upiim;
	struct device *dev = &pdev->dev;
	struct device_node *np = pdev->dev.of_node;

	/* Get Downstream resources  */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -ENODEV;

	dev_dbg(dev, "get dfwd resource: res->start 0x%x\n", res->start);
	addr_dfwd = devm_ioremap_resource(dev, res);
	if (IS_ERR(addr_dfwd))
		return PTR_ERR(addr_dfwd);

	/* Get Upstream resources  */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res)
		return -ENODEV;

	dev_dbg(dev, "get upiim resource: res->start 0x%x\n", res->start);
	addr_upiim = devm_ioremap_resource(dev, res);
	if (IS_ERR(addr_upiim))
		return PTR_ERR(addr_upiim);

	/* Allocate the network device */
	ndev = alloc_etherdev(sizeof(struct isve_priv));
	if (!ndev) {
		dev_err(dev, "ERROR: allocating the device\n");
		return -ENOMEM;
	}

	SET_NETDEV_DEV(ndev, dev);
	ether_setup(ndev);
	ndev->netdev_ops = &isve_netdev_ops;

	ndev->watchdog_timeo = msecs_to_jiffies(watchdog);
	isve_set_ethtool_ops(ndev);

	/* Setup private structure */
	priv = netdev_priv(ndev);
	priv->device = dev;
	priv->dev = ndev;
	priv->msg_enable = netif_msg_init(debug, default_msg_level);

	spin_lock_init(&priv->tx_lock);
	spin_lock_init(&priv->rx_lock);

	priv->ioaddr_dfwd = addr_dfwd;
	priv->ioaddr_upiim = addr_upiim;

	platform_set_drvdata(pdev, priv->dev);

	/* Get platform data */
	if (np) {
		int id;

		of_property_read_u32(np, "isve,queue_number",
				     &priv->queue_number);
		of_property_read_u32(np, "isve,skip_hdr", &priv->skip_hdr);
		of_property_read_u32(np, "isve,hw_rem_hdr", &priv->hw_rem_hdr);

		id = of_alias_get_id(np, ISVE_IFNAME);
		if (id >= 0) {
			int size = sizeof(ISVE_IFNAME) + 2;
			priv->ifname = kmalloc(size, GFP_KERNEL);
			snprintf(priv->ifname, size, "%s%d", ISVE_IFNAME, id);
		}

		priv->irq_ds = irq_of_parse_and_map(np, 0);
		if (priv->irq_ds == -ENXIO) {
			dev_err(dev, "ERROR: DFWD IRQ config not found\n");
			ret = -ENXIO;
			goto err_free_netdev;
		}

		priv->irq_us = irq_of_parse_and_map(np, 1);
		if (priv->irq_us == -ENXIO) {
			dev_err(dev, "ERROR: UPIIM IRQ config not found\n");
			ret = -ENXIO;
			goto err_free_netdev;
		}

	}

	/* Override the ethX with the name passed from the platform */
	if (priv->ifname)
		strcpy(priv->dev->name, priv->ifname);

	/* Init Upstream/Downstream modules */
	priv->dfwd = isve_dfwd_core(priv->ioaddr_dfwd);
	priv->upiim = isve_upiim_core(priv->ioaddr_upiim);

	if ((!priv->dfwd) || (!priv->upiim)) {
		dev_err(dev, "hard error allocating HW ops\n");
		goto err_free_netdev;
	}

	/* Register the iface and exit */
	ret = register_netdev(ndev);
	if (ret)
		goto err_free_netdev;

	ret = isve_alloc_rings(priv);
	if (ret)
		goto err_unreg_net;

	netif_napi_add(ndev, &priv->napi, isve_poll, 16);

	dev_info(dev, "%s: dfwd 0x%p, upiim 0x%p (irq_ds %d, irq_us %d)\n",
		 priv->dev->name, priv->ioaddr_dfwd, priv->ioaddr_upiim,
		 priv->irq_ds, priv->irq_us);

	return 0;

err_unreg_net:
	unregister_netdev(ndev);

err_free_netdev:
	free_netdev(ndev);

	return ret;
}

/**
 * isve_pltfr_remove: remove driver function
 * @pdev: platform device pointer
 * Description: this function resets the TX/RX processes and release the
 * resources used.
 */
static int isve_pltfr_remove(struct platform_device *pdev)
{
	struct net_device *ndev = platform_get_drvdata(pdev);
	struct isve_priv *priv = netdev_priv(ndev);

	isve_release_rings(priv);

	unregister_netdev(ndev);
	free_netdev(ndev);

	platform_set_drvdata(pdev, NULL);

	return 0;
}

static struct of_device_id stm_isve_match[] = {
	{
		.compatible = "st,isve",
	},
	{},
};

MODULE_DEVICE_TABLE(of, stm_isve_match);

static struct platform_driver isve_driver = {
	.probe = isve_pltfr_probe,
	.remove = isve_pltfr_remove,
	.driver = {
		   .name = ISVE_RESOURCE_NAME,
		   .owner = THIS_MODULE,
		   .of_match_table = of_match_ptr(stm_isve_match),
	},
};

module_platform_driver(isve_driver);

#ifndef MODULE
static int __init isve_cmdline_opt(char *str)
{
	char *opt;

	if (!str || !*str)
		return -EINVAL;

	while ((opt = strsep(&str, ",")) != NULL) {
		if (!strncmp(opt, "debug:", 6)) {
			if (kstrtoul(opt + 6, 0, (unsigned long *)&debug))
				goto err;
		} else if (!strncmp(opt, "watchdog:", 9)) {
			if (kstrtoul(opt + 9, 0, (unsigned long *)&watchdog))
				goto err;

		}
	}
	return 0;

err:
	return -EINVAL;
}

__setup("isve=", isve_cmdline_opt);
#endif /* MODULE */

MODULE_DESCRIPTION("Integrated SoC Virtual Ethernet driver");
MODULE_AUTHOR("Giuseppe Cavallaro <peppe.cavallaro@st.com>");
MODULE_LICENSE("GPL");
