#ifndef __RLK_INIC_DEF_H__
#define __RLK_INIC_DEF_H__

#include <linux/kernel.h>
#include <linux/compiler.h>
#include <linux/module.h>
//#include <linux/config.h>
#include <linux/autoconf.h>//peter : for FC6
#include <linux/version.h>
#include <linux/byteorder/generic.h>
#include <linux/unistd.h>
#include <linux/ctype.h>
#include <linux/wireless.h>
#include <linux/notifier.h>
#include <linux/list.h>
#include <linux/if_bridge.h>
#include <linux/netdevice.h>
#include <net/iw_handler.h>
#include "crc.h"

#define INIC_INF_TYPE_PCI 0
#define INIC_INF_TYPE_MII 1
#define INIC_INF_TYPE_USB 2

#ifdef IKANOS_VX_1X0
	typedef void (*IkanosWlanTxCbFuncP)(void *, void *);

	struct IKANOS_TX_INFO
	{
		struct net_device *netdev;
		IkanosWlanTxCbFuncP *fp;
	};
#endif // IKANOS_VX_1X0 //

#ifndef SET_MODULE_OWNER
#define SET_MODULE_OWNER(dev) do {} while(0)
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
typedef unsigned long uintptr_t;
#endif

#ifndef INT32
#define INT32        int
#endif
#ifndef UINT32
#define UINT32       unsigned int
#endif
#define INIC_INFNAME			"ra"

#define INIC_PFX_FILE_PATH					"/etc/Wireless/" CHIP_NAME INTERFACE "/"
#if (CONFIG_INF_TYPE == INIC_INF_TYPE_MII)
#ifdef PHASE_LOAD_CODE
#define INIC_PHASE_FIRMWARE_PATH			INIC_PFX_FILE_PATH "iNIC.bin"
#endif
#endif
#define INIC_AP_FIRMWARE_PATH				INIC_PFX_FILE_PATH "iNIC_ap.bin"
#define INIC_AP_PROFILE_PATH				INIC_PFX_FILE_PATH "iNIC_ap.dat"
#define INIC_AP_CONCURRENT_PROFILE_PATH 	INIC_PFX_FILE_PATH "iNIC_ap1.dat"

#define INIC_STA_FIRMWARE_PATH				INIC_PFX_FILE_PATH "iNIC_sta.bin"
#define INIC_STA_PROFILE_PATH				INIC_PFX_FILE_PATH "iNIC_sta.dat"
#define INIC_STA_CONCURRENT_PROFILE_PATH 	INIC_PFX_FILE_PATH "iNIC_sta1.dat"

#define fATE_LOAD_EEPROM					0x0C43
#define EEPROM_SIZE							0x200
#define EEPROM_BIN_FILE_PATH 				INIC_PFX_FILE_PATH "iNIC_e2p.bin"

#define CONCURRENT_EEPROM_BIN_FILE_PATH	INIC_PFX_FILE_PATH "iNIC_e2p1.bin"


#if defined(MULTIPLE_CARD_SUPPORT) || defined(CONFIG_CONCURRENT_INIC_SUPPORT)

#define INIC_STA_CARD_INFO_PATH				INIC_PFX_FILE_PATH "iNIC_sta_card.dat"
#define INIC_AP_CARD_INFO_PATH				INIC_PFX_FILE_PATH "iNIC_ap_card.dat"
// MC: Multple Cards
#define MAX_NUM_OF_MULTIPLE_CARD		32

// record whether the card in the card list is used in the card file
extern u8  MC_CardUsed[MAX_NUM_OF_MULTIPLE_CARD];
// record used card irq in the card list
extern s32 MC_CardIrq[MAX_NUM_OF_MULTIPLE_CARD];


#endif // MULTIPLE_CARD_SUPPORT //

#ifndef wait_event_interruptible_timeout
#define __wait_event_interruptible_timeout(wq, condition, ret)		\
do {									\
	wait_queue_t __wait;						\
	init_waitqueue_entry(&__wait, current);				\
									\
	add_wait_queue(&wq, &__wait);					\
	for (;;) {							\
		set_current_state(TASK_INTERRUPTIBLE);			\
		if (condition)						\
			break;						\
		if (!signal_pending(current)) {				\
			ret = schedule_timeout(ret);			\
			if (!ret)					\
				break;					\
			continue;					\
		}							\
		ret = -ERESTARTSYS;					\
		break;							\
	}								\
	current->state = TASK_RUNNING;					\
	remove_wait_queue(&wq, &__wait);				\
} while (0)

#define wait_event_interruptible_timeout(wq, condition, timeout)	\
({									\
	long __ret = timeout;						\
	if (!(condition))						\
		__wait_event_interruptible_timeout(wq, condition, __ret); \
	__ret;								\
})
#endif


/* hardware minimum and maximum for a single frame's data payload */
#define RLK_INIC_MIN_MTU		60	/* TODO: allow lower, but pad */
#define RLK_INIC_MAX_MTU		1536

#define MAC_ADDR_LEN         6
#define MBSS_MAX_DEV_NUM    32  /* for OS possible ra%d name    */
#define WDS_MAX_DEV_NUM     32  /* for OS possible wds%d name   */
#define APCLI_MAX_DEV_NUM   32  /* for OS possible apcli%d name */
#ifdef MESH_SUPPORT
#define MESH_MAX_DEV_NUM    32  /* for OS possible mesh%d name */
#endif // MESH_SUPPORT //
#define MAX_MBSSID_NUM       8
#define MAX_WDS_NUM          4
#define MAX_APCLI_NUM        1
#ifdef MESH_SUPPORT
#define MAX_MESH_NUM         1
#endif // MESH_SUPPORT //
#define FIRST_MBSSID         1
#define FIRST_WDSID          0
#define FIRST_APCLIID        0
#ifdef MESH_SUPPORT
#define FIRST_MESHID         0
#endif // MESH_SUPPORT //
#define MIN_NET_DEVICE_FOR_MBSSID       0x00 /* defined in iNIC firmware */
#define MIN_NET_DEVICE_FOR_WDS          0x10 /* defined in iNIC firmware */
#define MIN_NET_DEVICE_FOR_APCLI        0x20 /* defined in iNIC firmware */
#ifdef MESH_SUPPORT
#define MIN_NET_DEVICE_FOR_MESH         0x30 /* defined in iNIC firmware */
#endif // MESH_SUPPORT //

#define IFNAMSIZ            16
#define INT_MAIN        0x0100
#define INT_MBSSID      0x0200
#define INT_WDS         0x0300
#define INT_APCLI       0x0400
#ifdef MESH_SUPPORT
#define INT_MESH        0x0500
#endif // MESH_SUPPORT //

#define MAX_INI_BUFFER_SIZE			4096
#define MAX_PARAM_BUFFER_SIZE		(2048) 
#define ETH_MAC_ADDR_STR_LEN 17  		   // in format of xx:xx:xx:xx:xx:xx



#define MAX_LEN_OF_RACFG_QUEUE 		32
#define MAX_FEEDBACK_LEN			1024
#define MAX_PROFILE_SIZE            (8*MAX_FEEDBACK_LEN)
#define MAX_FILE_NAME_SIZE			256
#define MAX_EXT_EEPROM_SIZE			1024

#define NETIF_IS_UP(x)              (x->flags & IFF_UP)

#if (CONFIG_INF_TYPE!=INIC_INF_TYPE_MII)
#define GET_PARENT(x)  (((VIRTUAL_ADAPTER *)netdev_priv(x))->RtmpDev)
#else
#define GET_PARENT(x)  (((iNIC_PRIVATE *)netdev_priv(((VIRTUAL_ADAPTER *)netdev_priv(x))->RtmpDev))->RaCfgObj.MBSSID[0].MSSIDDev)
#endif

#ifdef RLK_INIC_NDEBUG
#define ASSERT(expr)
#else
#define ASSERT(expr) \
        if(unlikely(!(expr))) {                                   \
        printk(KERN_ERR "Assertion failed! %s,%s,%s,line=%d\n", \
        #expr,__FILE__,__FUNCTION__,__LINE__);          \
        }
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,9)
#define NETDEV_TX_OK     0        /* driver took care of packet */
#define NETDEV_TX_BUSY   1
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
#define SK_RECEIVE_QUEUE receive_queue
#define PCI_SET_CONSISTENT_DMA_MASK pci_set_dma_mask
#define SK_SOCKET socket
#else
#define SK_RECEIVE_QUEUE sk_receive_queue
#define PCI_SET_CONSISTENT_DMA_MASK pci_set_consistent_dma_mask
#define SK_SOCKET sk_socket
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,30)
#ifndef IRQ_HANDLED
#define irqreturn_t void
#define IRQ_HANDLED
#define IRQ_NONE
#endif
#endif

#ifndef __iomem
#define __iomem
#endif


#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,21)
#define DEV_SHORTCUT(ndev)  ((ndev)->class_dev.dev)
#else
#define DEV_SHORTCUT(ndev)  ((ndev)->dev.parent)
#endif

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,23)
#define DEV_GET_BY_NAME(name) dev_get_by_name(name);
#else
#define DEV_GET_BY_NAME(name) dev_get_by_name(&init_net, name);
#endif

#ifndef SET_NETDEV_DEV
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
#define SET_NETDEV_DEV(ndev, pdev)  do { } while (0)
#else
#define SET_NETDEV_DEV(ndev, pdev)  (DEV_SHORTCUT(ndev) = (pdev)) //do { } while (0)
#endif
#endif

#ifndef SEEK_SET
#define SEEK_SET 0
#endif

#ifndef SEEK_END
#define SEEK_END 2
#endif

//#define DBG

#ifdef DBG
#define DBGPRINT(fmt, args...)      printk(fmt, ## args)
#else
#define DBGPRINT(fmt, args...)
#endif


#ifndef TRUE
#define TRUE              (1)
#define FALSE             (0)
#endif


#define MEM_ALLOC_FLAG      (GFP_ATOMIC) //(GFP_DMA | GFP_ATOMIC)

#define MAGIC_NUMBER			0x18142880

#ifndef TRUE
#define FALSE 0
#define TRUE (!FALSE)
#endif


typedef spinlock_t RTMP_SPIN_LOCK;

#define RTMP_SEM_LOCK(__lock)                   \
{                                               \
    spin_lock_bh((spinlock_t *)(__lock));       \
}

#define RTMP_SEM_UNLOCK(__lock)                 \
{                                               \
    spin_unlock_bh((spinlock_t *)(__lock));     \
}
#define RTMP_SEM_INIT(__lock)            \
{                                               \
    spin_lock_init((spinlock_t *)(__lock));     \
}

typedef void (*PRaCfgFunc)(uintptr_t);

typedef struct _racfg_task
{
	PRaCfgFunc func;
    uintptr_t arg;
} RaCfgTask;

typedef struct _racfg_queue
{
    char name[32];
	u32 num;
	u32 head;
	u32 tail;
	RTMP_SPIN_LOCK lock;
	RaCfgTask entry[MAX_LEN_OF_RACFG_QUEUE];
} RaCfgQueue;


typedef struct file_handle
{
	char name[MAX_FILE_NAME_SIZE];
	
#if defined(MULTIPLE_CARD_SUPPORT) || defined(CONFIG_CONCURRENT_INIC_SUPPORT)
	/* read name form card file */
	char read_name[MAX_FILE_NAME_SIZE]; 
#endif // MULTIPLE_CARD_SUPPORT || CONFIG_CONCURRENT_INIC_SUPPORT

	struct file *fp;
	int  seq;
    u32  crc;
    CRC_HEADER hdr;
    unsigned char hdr_src[CRC_HEADER_LEN];
} FileHandle;

typedef unsigned char   boolean;

typedef struct _MULTISSID_STRUCT
{
	unsigned char       Bssid[MAC_ADDR_LEN];
	struct net_device   *MSSIDDev;	
	u16                 vlan_id;  
	u16                 flags;
	u8					bVLAN_tag;
} MULTISSID_STRUCT, *PMULTISSID_STRUCT;

typedef struct _VIRTUAL_ADAPTER
{
	struct net_device       *RtmpDev;
	struct net_device       *VirtualDev;
	struct net_device_stats     net_stats;
} VIRTUAL_ADAPTER, PVIRTUAL_ADAPTER;


#define MAX_NOTIFY_DROP_NUMBER	5

//#define TEST_BOOT_RECOVER 1

#ifdef RETRY_PKT_SEND
#define RetryTimeOut 1000    // default 50ms, better set timeout >= 50ms
#define FwRetryCnt	10		 // retry counts for fw upload
#define InbandRetryCnt	3	// retry counts for inband

typedef struct _RETRY_PKT_INFO
{
	int Seq;
	int BootType;
	int length;
	int retry;
	u8 buffer[MAX_FEEDBACK_LEN];
} RETRY_PKT_INFO;

#endif

typedef struct __RACFG_OBJECT
{
	struct net_device           *MainDev;
	struct completion           TaskThreadComplete;
	struct completion           BacklogThreadComplete;
	pid_t                       task_thread_pid;
	pid_t                       backlog_thread_pid;
	unsigned char               opmode;						// 0: STA, 1: AP
	boolean                     bBridge;					// use built-in bridge or not ?
    boolean                     bChkSumOff;
	unsigned char               BssidNum;
	unsigned char               bWds;						// enable wds interface not
	unsigned char               bApcli;						// enable apclient interface
#ifdef MESH_SUPPORT
	unsigned char               bMesh;						// enable mesh interface
#endif // MESH_SUPPORT //
	MULTISSID_STRUCT            MBSSID[MAX_MBSSID_NUM];
	MULTISSID_STRUCT            WDS[MAX_WDS_NUM];
	MULTISSID_STRUCT            APCLI[MAX_APCLI_NUM];
#ifdef MESH_SUPPORT
	MULTISSID_STRUCT            MESH[MAX_MESH_NUM];
#endif // MESH_SUPPORT //
	boolean                     threads_exit;
	boolean                     flg_is_open;
	boolean                     flg_mbss_init;
	boolean                     flg_wds_init;
	boolean                     flg_apcli_init;
#ifdef MESH_SUPPORT
	boolean                     flg_mesh_init;
#endif // MESH_SUPPORT //
	boolean                     wds_close_all;
	boolean                     mbss_close_all;
	boolean                     apcli_close_all;
#ifdef MESH_SUPPORT
	boolean                     mesh_close_all;
#endif // MESH_SUPPORT //
	boolean                     bRestartiNIC;
	boolean						bGetMac;
	struct timer_list           heartBeat;					// timer to check the iNIC heart beat
	RTMP_SPIN_LOCK              timerLock;					// for heartBeat timer

#ifdef RETRY_PKT_SEND
	struct timer_list           uploadTimer;					// timer to check the iNIC upload process
	RETRY_PKT_INFO 		RPKTInfo;
#endif

	wait_queue_head_t           waitQH;
	int                         wait_completed;
	RTMP_SPIN_LOCK              waitLock;
	struct semaphore			taskSem; 
	struct semaphore			backlogSem; 
	RaCfgQueue					taskQueue;
	RaCfgQueue					backlogQueue;
	RaCfgQueue                  waitQueue; 

	u8                          packet[1536];
	u8                          wsc_updated_profile[MAX_PROFILE_SIZE];
	FileHandle                  firmware, profile, ext_eeprom;
	boolean               		bGetExtEEPROMSize;			 // for upload profile
	boolean               		bExtEEPROM;					 // enable external eeprom not
	unsigned int 				ExtEEPROMSize;				 // for external eeprom
	char                        test_pool[MAX_FEEDBACK_LEN];
	char                        cmp_buf[MAX_FEEDBACK_LEN];
	char                        upload_buf[MAX_FEEDBACK_LEN];
	mm_segment_t                orgfs;
	boolean                     bDebugShow;
	unsigned char               HeartBeatCount;
	//unsigned char               RebootCount;
	struct notifier_block       net_dev_notifier;
#ifdef TEST_BOOT_RECOVER
    boolean                     cfg_simulated;
    boolean                     firm_simulated;
#endif
#ifdef IKANOS_VX_1X0
	struct IKANOS_TX_INFO		IkanosTxInfo;
	struct IKANOS_TX_INFO		IkanosRxInfo[MAX_MBSSID_NUM];
#endif // IKANOS_VX_1X0 //
    int                         extraProfileOffset;
#if defined(MULTIPLE_CARD_SUPPORT) || defined(CONFIG_CONCURRENT_INIC_SUPPORT)
	s8					InterfaceNumber;
	u8					bRadioOff;
#endif 

#ifdef MULTIPLE_CARD_SUPPORT
	s8					MC_RowID;
	u8					MC_MAC[20];
#endif // MULTIPLE_CARD_SUPPORT //

#if (CONFIG_INF_TYPE==INIC_INF_TYPE_MII)
	struct sk_buff *eapol_skb;	
	u16 eapol_command_seq;
	u16 eapol_seq;
	boolean               		bLocalAdminAddr;			// for locally administered address
#ifdef PHASE_LOAD_CODE
	boolean						bLoadPhase;					//0:initail; 1:load 
	u8							dropNotifyCount;					//drop notify
#endif
#endif
    boolean                     bWpaSupplicantUp;
    int         fw_upload_counter;
    
#ifdef WOWLAN_SUPPORT
	struct notifier_block		pm_wow_notifier;
	unsigned long				pm_wow_state;
	boolean                     bWoWlanUp;
	struct timer_list           WowInbandSignalTimer;					// timer to check the WOWLAN heart beat
	RTMP_SPIN_LOCK              WowInbandSignalTimerLock;				// for WowInbandSignalTimer timer
	u8							WowInbandInterval;						// WOW inband timer interval					
#endif // WOWLAN_SUPPORT //     
    
} RACFG_OBJECT, PRACFG_OBJECT;

#ifdef CONFIG_CONCURRENT_INIC_SUPPORT

#define CONCURRENT_CARD_NUM 2

typedef struct __CONCURRENT_OBJECT
{
u8 			CardCount;
char 		Mac[CONCURRENT_CARD_NUM][64];
FileHandle	Profile[CONCURRENT_CARD_NUM];
FileHandle	ExtEeprom[CONCURRENT_CARD_NUM];
int			extraProfileOffset2;
}CONCURRENT_OBJECT;

#endif // CONFIG_CONCURRENT_INIC_SUPPORT //

#if (CONFIG_INF_TYPE==INIC_INF_TYPE_MII)
typedef struct inic_private
{
	struct net_device   *dev;
	struct net_device   *master;
	spinlock_t          lock;
	u32                 msg_enable;
	struct net_device_stats     net_stats;

	RACFG_OBJECT	    RaCfgObj;
	struct iw_statistics iw_stats;
#ifdef NM_SUPPORT
    int                 (*hardware_reset)(int op);
#endif

} iNIC_PRIVATE;
#endif


struct vlan_tic
{
#ifdef __BIG_ENDIAN
	u16     priority : 3;
	u16     cfi      : 1;
	u16     vid      : 12;
#else
	u16     vid      : 12;
	u16     cfi      : 1;
	u16     priority : 3;
#endif
} __attribute__((packed));



#define GET_SKB_HARD_HEADER_LEN(_x)  (_x->cb[0])
#define SET_SKB_HARD_HEADER_LEN(_x, _y)  (_x->cb[0] = _y)

//#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,4,31)
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,9)
#define BR_HOOK_NOT_HANDLED 1
#define BR_HOOK_HANDLED     0
#define DECLARE_BR_HANDLE_FRAME(func, p, skb, pskb) \
    static int (*func)(struct sk_buff *skb)
#define DEFINE_BR_HANDLE_FRAME(func, p, skb, pskb) \
    static int func(struct sk_buff *skb)
#define DECLARE_BR_HANDLE_FRAME_SKB(skb, pskb)
#define BR_HANDLE_FRAME(func, p, skb, pskb) \
    func(skb)
#define DEFINE_PACKET_TYPE_FUNC(func, skb, dev, pt, orig_dev) \
    static int func(struct sk_buff *skb, struct net_device *dev, \
                    struct packet_type *pt)

#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,9) && LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,21)

#define BR_HOOK_NOT_HANDLED 0
#define BR_HOOK_HANDLED     1
#define DECLARE_BR_HANDLE_FRAME(func, p, skb, pskb) \
    static int (*func)(struct net_bridge_port *p, struct sk_buff **pskb)
#define DEFINE_BR_HANDLE_FRAME(func, p, skb, pskb) \
    static int func(struct net_bridge_port *p, struct sk_buff **pskb)
#define DECLARE_BR_HANDLE_FRAME_SKB(skb, pskb) \
    struct sk_buff *skb = *pskb
#define BR_HANDLE_FRAME(func, p, skb, pskb) \
    func(p, pskb)
#define DEFINE_PACKET_TYPE_FUNC(func, skb, dev, pt, orig_dev) \
    static int func(struct sk_buff *skb, struct net_device *dev, \
                    struct packet_type *pt, struct net_device *orig_dev)
#else

#define BR_HOOK_NOT_HANDLED skb
#define BR_HOOK_HANDLED     NULL
#define DECLARE_BR_HANDLE_FRAME(func, p, skb, pskb) \
    static struct sk_buff *(*func)(struct net_bridge_port *p, struct sk_buff *skb)
#define DEFINE_BR_HANDLE_FRAME(func, p, skb, pskb) \
    static struct sk_buff *func(struct net_bridge_port *p, struct sk_buff *skb)
#define DECLARE_BR_HANDLE_FRAME_SKB(skb, pskb)
#define BR_HANDLE_FRAME(func, p, skb, pskb) \
    func(p, skb)
#define DEFINE_PACKET_TYPE_FUNC(func, skb, dev, pt, orig_dev) \
    static int func(struct sk_buff *skb, struct net_device *dev, \
                    struct packet_type *pt, struct net_device *orig_dev)
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
typedef struct pid * THREAD_PID;
#define THREAD_PID_INIT_VALUE NULL
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,30)
#define	GET_PID(_v)	find_get_pid(_v)
#else
#define GET_PID(_v) task_pid(find_task_by_vpid((_v)))
#endif
#define GET_PID_NUMBER(_v) pid_nr((_v))
#define CHECK_PID_LEGALITY(_pid) if (pid_nr((_pid)) >= 0)
#define KILL_THREAD_PID(_A, _B, _C) kill_pid((_A), (_B), (_C))
#else
typedef pid_t THREAD_PID;
#define THREAD_PID_INIT_VALUE -1
#define GET_PID(_v) (_v)
#define GET_PID_NUMBER(_v) (_v)
#define CHECK_PID_LEGALITY(_pid) if ((_pid) >= 0)
#define KILL_THREAD_PID(_A, _B, _C) kill_proc((_A), (_B), (_C))
#endif

#ifdef CONFIG_CONCURRENT_INIC_SUPPORT
extern CONCURRENT_OBJECT ConcurrentObj;
#endif // CONFIG_CONCURRENT_INIC_SUPPORT //

#if (CONFIG_INF_TYPE == INIC_INF_TYPE_MII)
extern iNIC_PRIVATE *gAdapter[2];
#endif


#define RLK_STRLEN(target_len, source_len) (target_len)
#define RLK_STRCPY(target_src, source_src) strncpy(target_src, source_src, RLK_STRLEN(sizeof(target_src), sizeof(source_src)))


#endif /* __RLK_INIC_DEF_H__ */

