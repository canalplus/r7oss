#ifndef	__RT_USB_DEV_H
#define	__RT_USB_DEV_H

/******************************************************************************************/
/* USB Product related define 																   */
#if (CONFIG_CHIP_NAME==3052)
#define USB_PID 0x3052
#endif

#if (CONFIG_CHIP_NAME==3883)
#define USB_PID 0x3883
#endif

#if (CONFIG_CHIP_NAME==3352)
#define USB_PID 0x3352
#endif

#if (CONFIG_CHIP_NAME==5350)
#define USB_PID 0x5350
#endif

#if (CONFIG_CHIP_NAME==3662)
#define USB_PID 0x3662
#endif

/******************************************************************************************/
/* network related define 																	   */

#if defined(RLK_INIC_SOFTWARE_AGGREATION) || defined(RLK_INIC_TX_AGGREATION_ONLY)
#define	AGGR_QUEUE_SIZE			(15 * 1516)
#define	MAX_PKT_OF_AGGR_QUEUE	1024
#endif


/* USB Networking Link Interface */
#if  LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
#define 	RX_QLEN(dev)		1
#define 	TX_QLEN(dev)		1
#else

#if defined(RLK_INIC_SOFTWARE_AGGREATION) || defined(RLK_INIC_TX_AGGREATION_ONLY)
#define 	RX_QLEN(dev)		1
#define 	TX_QLEN(dev)		1
#else
#define 	RX_MAX_QUEUE_MEMORY (60 * 1518)
#define	RX_QLEN(dev) (((dev)->udev->speed == USB_SPEED_HIGH) ? \
			(RX_MAX_QUEUE_MEMORY/(dev)->rx_urb_size) : 4)
#define	TX_QLEN(dev) (((dev)->udev->speed == USB_SPEED_HIGH) ? \
			(RX_MAX_QUEUE_MEMORY/(dev)->hard_mtu) : 4)
#endif

#endif

/* throttle rx/tx briefly after some faults, so khubd might disconnect() us (it polls at HZ/4 usually) before we report too many false errors.*/
#define THROTTLE_JIFFIES	(HZ/8)

/* we record the state for each of our queued skbs */
enum skb_state {
	illegal = 0,
	tx_start, tx_done,
	rx_start, rx_done, rx_cleanup
};

struct skb_data {	/* skb->cb is one of these */
	struct urb		*urb;
	struct usbnet		*dev;
	enum skb_state		state;
	size_t			length;
};


/******************************************************************************************/
/*** USB related define 																	    */

typedef int				NTSTATUS;
typedef void				VOID;

typedef unsigned char		UINT8;
typedef signed int			INT;

typedef unsigned char		UCHAR;
typedef unsigned short		USHORT;
typedef unsigned int		UINT;
typedef unsigned long		ULONG;

typedef VOID *				PVOID;
typedef UCHAR * 			PUCHAR;

typedef unsigned char		BOOLEAN;

/* macro */
#define IN
#define OUT

/* vendor specific sub-class and protocol */
#define USB_SUBCLASS_VENDOR	0xFF
#define USB_PROTO_VENDOR		0xFF

/* USB Firmware related */
#define DEVICE_VENDOR_REQUEST_OUT       0x40
#define DEVICE_VENDOR_REQUEST_IN        0xc0
#define INTERFACE_VENDOR_REQUEST_OUT    0x41
#define INTERFACE_VENDOR_REQUEST_IN     0xc1

// vendor-specific control operations
#define CONTROL_TIMEOUT_JIFFIES ( (300 * HZ) / 100)
#define UNLINK_TIMEOUT_MS		3

#define RTUSB_FREE_URB(pUrb)	usb_free_urb(pUrb)

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
#define RTUSB_ALLOC_URB(iso)                                               usb_alloc_urb(iso, GFP_ATOMIC)
#define RTUSB_SUBMIT_URB(pUrb)                                             usb_submit_urb(pUrb, GFP_ATOMIC)
#define	RTUSB_URB_ALLOC_BUFFER(pUsb_Dev, BufSize, pDma_addr)               usb_buffer_alloc(pUsb_Dev, BufSize, GFP_ATOMIC, pDma_addr)
#define	RTUSB_URB_FREE_BUFFER(pUsb_Dev, BufSize, pTransferBuf, Dma_addr)   usb_buffer_free(pUsb_Dev, BufSize, pTransferBuf, Dma_addr)

static inline struct usb_driver *driver_of(struct usb_interface *intf)
{
	return to_usb_driver(intf->dev.driver);
}
#else
#define usb_get_dev		usb_inc_dev_use
#define usb_put_dev		usb_dec_dev_use
#define	flush_scheduled_work	flush_scheduled_tasks
#define RTUSB_ALLOC_URB(iso)                                               usb_alloc_urb(iso)
#define RTUSB_SUBMIT_URB(pUrb)                                             usb_submit_urb(pUrb)
#define RTUSB_URB_ALLOC_BUFFER(pUsb_Dev, BufSize, pDma_addr)               kmalloc(BufSize, GFP_ATOMIC)
#define	RTUSB_URB_FREE_BUFFER(pUsb_Dev, BufSize, pTransferBuf, Dma_addr)   kfree(pTransferBuf)
#endif

#define USB_DEVICE_AND_INT_INFO(vend,prod,cl,sc,pr) \
	match_flags: USB_DEVICE_ID_MATCH_INT_INFO|USB_DEVICE_ID_MATCH_DEVICE, idVendor: (vend), idProduct: (prod), bInterfaceClass: (cl), bInterfaceSubClass: (sc), bInterfaceProtocol: (pr)


/*** File handler ***/
#ifndef PF_NOFREEZE
#define PF_NOFREEZE  0
#endif

typedef struct rt_thread_info
{
	pid_t fwupgrade_pid;
	struct completion fwupgrad_notify;
	struct inic_private *pAd;
} rt_threadInfo;


/* interface from usbnet core to each USB networking link we handle */
struct usbnet {
	/* housekeeping */
	struct usb_device	*udev;
	struct usb_interface	*intf;
	struct driver_info	*driver_info;
	const char		*driver_name;
	void			*driver_priv;

	/* i/o info: pipes etc */
	unsigned		in, out;
	unsigned		maxpacket;
	struct timer_list	delay;
	int			msg_enable;

	/* protocol/interface state */
	struct net_device	*net;
	struct net_device_stats	stats;
	struct usb_interface		*control;
	struct usb_interface		*data;
	u32			hard_mtu;	/* count any extra framing */
	size_t			rx_urb_size;	/* size for rx urbs */

	/* various kinds of pending driver work */
#if defined(RLK_INIC_SOFTWARE_AGGREATION) || defined(RLK_INIC_TX_AGGREATION_ONLY)
	struct sk_buff_head	aggr_q;
#endif
	struct sk_buff_head	rxq;
	struct sk_buff_head	txq;
	struct sk_buff_head	done;
	struct tasklet_struct	bh;

};

#if (CONFIG_INF_TYPE==INIC_INF_TYPE_USB)
typedef struct inic_private
{
	struct usbnet sbdev;
	struct net_device   *dev;
	spinlock_t          lock;
	u32                 msg_enable;
    struct net_device_stats     net_stats;
	RACFG_OBJECT				RaCfgObj;
	struct iw_statistics iw_stats;
	u8			flag;
#define	FLAG_INIT_MODE	0x01	/* initial mode, insmod ready */
#define	FLAG_FWFAIL_MODE	0x02	/* firmware upgrade fail */
#define	FLAG_READY_MODE	0x03	/* ready mode, fw upload ready */
#define	FLAG_OPEN_MODE	0x04	/* open mode, wait fw run */
#define	FLAG_FWDONE_MODE	0x05	/* firmware done mode, fw is running */

} iNIC_PRIVATE;
#endif

/* messaging support includes the interface name, so it must not be
 * used before it has one ... notably, in minidriver bind() calls.
 */
#ifdef DBG
#define devdbg(usbnet, fmt, arg...) \
	printk(KERN_DEBUG "%s: " fmt "\n" , (usbnet)->net->name , ## arg)
#else
#define devdbg(usbnet, fmt, arg...) do {} while(0)
#endif

#endif	/* __RT_USB_DEV_H */
