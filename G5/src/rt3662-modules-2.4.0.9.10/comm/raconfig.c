#include "rlk_inic.h"
#include <linux/delay.h>

#ifdef WOWLAN_SUPPORT
#include <linux/suspend.h>
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#endif // WOWLAN_SUPPORT // 
   
#define MAX_EAPOL_SIZE		1522

//wpa_supplicant event flags
#define RT_ASSOC_EVENT_FLAG                         0x0101
#define RT_DISASSOC_EVENT_FLAG                      0x0102

#ifdef NM_SUPPORT
inline static void hardware_reset(iNIC_PRIVATE *pAd, int op)
{
#if (CONFIG_INF_TYPE==INIC_INF_TYPE_MII)
	/* poll down GPIO */
	//printk("Please issue GPIO to start iNIC\n");
	if (pAd->hardware_reset)
		pAd->hardware_reset(op);
#else
	if (op)
        rlk_inic_init_hw(pAd);
	else
	    rlk_inic_stop_hw(pAd);
#endif
}
#endif

/*
 * Local Functions
 */
//static int OpenRaCfgSocket(void);
//static void RaCfgMasterClose(int sig);
//static struct file *OpenRLK_INICFirmware(u8 *path);
//static struct file *OpenRLK_INICProfile(u8 *path);
//static void RcvRaCfgHandler(int sig);
//static void SendRaCfgCommand(iNIC_PRIVATE *pAd, u16 cmd_type, u16 cmd_id, u16 len, u16 seq, u8 *data);

#ifdef MULTIPLE_CARD_SUPPORT
// record whether the card in the card list is used in the card file
u8  MC_CardUsed[MAX_NUM_OF_MULTIPLE_CARD];
// record used card irq in the card list
s32 MC_CardIrq[MAX_NUM_OF_MULTIPLE_CARD];
#endif // MULTIPLE_CARD_SUPPORT //



#ifdef CONFIG_CONCURRENT_INIC_SUPPORT
CONCURRENT_OBJECT ConcurrentObj;
#endif // CONFIG_CONCURRENT_INIC_SUPPORT //


static void send_racfg_cmd(iNIC_PRIVATE *pAd, char *, int);

static int  RaCfgEnqueue(iNIC_PRIVATE *pAd, RaCfgQueue *, struct semaphore *, PRaCfgFunc func, uintptr_t arg);
static uintptr_t RaCfgDequeue(iNIC_PRIVATE *pAd, RaCfgQueue *, struct semaphore *);
static int  RaCfgQueueFull(RaCfgQueue  *queue);
static int  RaCfgQueueEmpty(RaCfgQueue *queue); 
static void RaCfgQueueInit(RaCfgQueue  *queue, char *name);
static void RaCfgQueueReset(RaCfgQueue *queue, struct semaphore *); 

static int  RaCfgTaskThread(void *arg);
static int  RaCfgBacklogThread(void *arg);

static ArgBox *GetArgBox(void);
static void FreeArgBox(ArgBox *box);
static void _upload_firmware(iNIC_PRIVATE *pAd);
static void _upload_profile(iNIC_PRIVATE *pAd);

#ifdef CONFIG_CONCURRENT_INIC_SUPPORT
static void RaCfgConcurrentOpenAction(uintptr_t);
#else // CONFIG_CONCURRENT_INIC_SUPPORT //
static void RaCfgOpenAction(uintptr_t);
#endif // CONFIG_CONCURRENT_INIC_SUPPORT //
static void RaCfgInitCfgAction(uintptr_t);
static void RaCfgUploadAction(uintptr_t);
static void RaCfgRestartiNIC(uintptr_t);
static void RaCfgEnqueueRestart(iNIC_PRIVATE *pAd);
static void RaCfgHeartBeatTimeOut(uintptr_t);
static void RaCfgInitThreads(iNIC_PRIVATE *pAd);
static void RaCfgKillThread(iNIC_PRIVATE *pAd);
void ReadCrcHeader(CRC_HEADER *hdr, unsigned char *buffer);

static void rlk_inic_set_mac(iNIC_PRIVATE *pAd, char *mac_addr);
static int get_mac_from_inic(iNIC_PRIVATE *pAd);
#ifndef NM_SUPPORT
static int netdev_event(struct notifier_block *this, unsigned long event, void *ptr);
void close_all_interfaces(struct net_device *root_dev);
#endif

#ifdef WOWLAN_SUPPORT 
static int pm_wow_event(struct notifier_block *this, unsigned long event, void *arg);
#endif // WOWLAN_SUPPORT //

extern const struct iw_handler_def rlk_inic_iw_handler_def;

#if (CONFIG_INF_TYPE==INIC_INF_TYPE_MII)
extern void racfg_inband_hook_init(iNIC_PRIVATE *pAd);
extern void racfg_inband_hook_cleanup(void);
#endif

static void RaCfgAddHeartBeatTimer(iNIC_PRIVATE *pAd);
static void RaCfgDelHeartBeatTimer(iNIC_PRIVATE *pAd);

#ifdef WOWLAN_SUPPORT
static void RaCfgAddWowInbandTimer(iNIC_PRIVATE *pAd);
static void RaCfgDelHeartWowInbandTimer(iNIC_PRIVATE *pAd);
#endif 

int init_flag = 0;

#ifdef NM_SUPPORT

#if WIRELESS_EXT > 14
static void inline we_send_private_event(iNIC_PRIVATE *pAd, const char* str)
{
	union iwreq_data wrqu;
	char *memptr;
	int n = strlen(str);

	BUG_ON(n > IW_CUSTOM_MAX);
	memptr = kmalloc(IW_CUSTOM_MAX, GFP_KERNEL);
	if (!memptr)
		return;
	wrqu.data.pointer = memptr;
	wrqu.data.length = n;
	strcpy(memptr, str);
	wireless_send_event(pAd->RaCfgObj.MainDev, IWEVCUSTOM, &wrqu, memptr);
	kfree(memptr);
}
static void inline we_shutdowned(iNIC_PRIVATE *pAd)
{
	we_send_private_event(pAd, "shutdowned");
}

static void inline we_fw_upload_failed(iNIC_PRIVATE *pAd)
{
	we_send_private_event(pAd, "fw upload failed");
}

static void inline we_fw_uploaded(iNIC_PRIVATE *pAd)
{
	we_send_private_event(pAd, "fw uploaded");
}
#else
static void inline we_shutdowned(iNIC_PRIVATE *pAd) {}
static void inline we_fw_upload_failed(iNIC_PRIVATE *pAd) {}
static void inline we_fw_uploaded(iNIC_PRIVATE *pAd) {}
#endif // WIRELESS_EXT > 14

void RaCfgShutdown(iNIC_PRIVATE *pAd)
{
	printk("=== shutdown ====\n");

	/* suspend the heartbeat timer */
	RaCfgDelHeartBeatTimer(pAd);

#ifdef WOWLAN_SUPPORT
	RaCfgDelHeartWowInbandTimer(pAd);
#endif // WOWLAN_SUPPORT //

	/* close interface ra0 */
	pAd->RaCfgObj.flg_is_open = 0;
	dev_change_flags(pAd->dev, pAd->dev->flags & ~IFF_UP);
	//pAd->dev->stop(pAd->dev);
	hardware_reset(pAd, 0);
	we_shutdowned(pAd);
}


void RaCfgStartup(iNIC_PRIVATE *pAd)
{
	printk("=== startup (no iface up) ====\n");

	/* startup interface ra0 */
	pAd->RaCfgObj.flg_is_open = 1;
	//dev_change_flags(pAd->dev, IFF_UP);
	hardware_reset(pAd, 1);
	/* resume the heartbeat timer */
	pAd->RaCfgObj.fw_upload_counter = 0;
	RaCfgAddHeartBeatTimer(pAd);

#ifdef WOWLAN_SUPPORT
	RaCfgAddWowInbandTimer(pAd);
#endif 	
	
	get_mac_from_inic(pAd);
}

void RaCfgRestart(iNIC_PRIVATE *pAd)
{
	if (pAd->RaCfgObj.flg_is_open && pAd->RaCfgObj.bGetMac)
		return;
	RaCfgShutdown(pAd);
	RaCfgStartup(pAd);
}
#endif

static void RaCfgAddHeartBeatTimer(iNIC_PRIVATE *pAd)
{

#ifdef CONFIG_CONCURRENT_INIC_SUPPORT
	/* Only main interface has heart beat timer for system. */
	if(ConcurrentObj.CardCount== 0)
		pAd = gAdapter[0];
	else
		return;
#endif // CONFIG_CONCURRENT_INIC_SUPPORT //

	RTMP_SEM_LOCK(&pAd->RaCfgObj.timerLock);

	if (pAd->RaCfgObj.threads_exit ||
		pAd->RaCfgObj.fw_upload_counter >= max_fw_upload)
	{
		RTMP_SEM_UNLOCK(&pAd->RaCfgObj.timerLock);
		return;
	}

	if (!pAd->RaCfgObj.flg_is_open)
	{
		RTMP_SEM_UNLOCK(&pAd->RaCfgObj.timerLock);
		return;
	}

	if (!pAd->RaCfgObj.heartBeat.function)
	{
		init_timer(&pAd->RaCfgObj.heartBeat);
		pAd->RaCfgObj.heartBeat.function = RaCfgHeartBeatTimeOut;
		pAd->RaCfgObj.heartBeat.data = (uintptr_t)pAd;
	}
	mod_timer(&pAd->RaCfgObj.heartBeat, jiffies + HEART_BEAT_TIMEOUT * HZ);
	RTMP_SEM_UNLOCK(&pAd->RaCfgObj.timerLock);
}

static void RaCfgDelHeartBeatTimer(iNIC_PRIVATE *pAd)
{

#ifdef CONFIG_CONCURRENT_INIC_SUPPORT
	/* Only main interface has heart beat timer for system. */
	if(ConcurrentObj.CardCount== 0)
		pAd = gAdapter[0];
	else
		return;
#endif // CONFIG_CONCURRENT_INIC_SUPPORT //

	RTMP_SEM_LOCK(&pAd->RaCfgObj.timerLock);
	if (pAd->RaCfgObj.heartBeat.function)
	{
		printk("Delete HeartBeatTimer ....\n");
		del_timer_sync(&pAd->RaCfgObj.heartBeat);
		pAd->RaCfgObj.heartBeat.function = NULL;
		pAd->RaCfgObj.heartBeat.data = 0;
	}
	RTMP_SEM_UNLOCK(&pAd->RaCfgObj.timerLock);
}

extern const struct iw_handler_def rlk_inic_iw_handler_def;
struct iw_handler_def* get_wireless_handler(void)
{
    return (struct iw_handler_def*)&rlk_inic_iw_handler_def;
}

void RaCfgInit(iNIC_PRIVATE *pAd, struct net_device *dev, char *conf_mac, char *conf_mode, int built_bridge, int chk_sum_off)
{

	dev->priv_flags = INT_MAIN;

	// initialized main bss
	pAd->RaCfgObj.MainDev = dev;
	pAd->RaCfgObj.MBSSID[0].MSSIDDev = dev;
	pAd->RaCfgObj.BssidNum = 1;
	pAd->RaCfgObj.bBridge = built_bridge;
	pAd->RaCfgObj.bChkSumOff= chk_sum_off;

	pAd->RaCfgObj.HeartBeatCount = 0;
	//pAd->RaCfgObj.RebootCount = 0;

#ifdef NM_SUPPORT
	pAd->RaCfgObj.bWpaSupplicantUp = TRUE;
#if (CONFIG_INF_TYPE==INIC_INF_TYPE_MII)
	// please assign your GPIO hardware reset handler here
	pAd->hardware_reset = 0;
#endif
	hardware_reset(pAd, 0);
	udelay(10000);
	hardware_reset(pAd, 1);
#else
	pAd->RaCfgObj.net_dev_notifier.notifier_call = netdev_event;
	pAd->RaCfgObj.net_dev_notifier.next = NULL;
	pAd->RaCfgObj.net_dev_notifier.priority = 0;
	register_netdevice_notifier(&pAd->RaCfgObj.net_dev_notifier);
#endif

#ifdef WOWLAN_SUPPORT 
	pAd->RaCfgObj.pm_wow_notifier.notifier_call = pm_wow_event;
	pAd->RaCfgObj.pm_wow_notifier.next 			= NULL;
	pAd->RaCfgObj.pm_wow_notifier.priority 		= 0;
	pAd->RaCfgObj.pm_wow_state 					= WOW_CPU_UP;   
	register_cpu_notifier(&pAd->RaCfgObj.pm_wow_notifier);
#endif 


	if (strcmp(conf_mode, "sta") == 0)
	{
		pAd->RaCfgObj.opmode = 0;
	}
	else
	{
		pAd->RaCfgObj.opmode = 1;
	}

	if (strlen(conf_mac))
	{
		unsigned char byte;

		AtoH(conf_mac, &byte, 1);

		if (byte & 0x01)
		{
			printk("WARNING! Multicast MAC addr given.\n");
		}
	}


	/* set MAC address */
	if (strlen(conf_mac) != 17 || 
		conf_mac[2] != ':'  || conf_mac[5] != ':'  || conf_mac[8] != ':' ||
		conf_mac[11] != ':' || conf_mac[14] != ':')
	{

#if (CONFIG_INF_TYPE==INIC_INF_TYPE_MII)
		pAd->RaCfgObj.eapol_skb = dev_alloc_skb(MAX_EAPOL_SIZE);
		if (pAd->RaCfgObj.eapol_skb)
		{
			skb_reserve(pAd->RaCfgObj.eapol_skb, 2);
		}
		pAd->RaCfgObj.eapol_command_seq = 0;
		pAd->RaCfgObj.eapol_seq = 0;

		if (pAd->master && syncmiimac)
		{
			printk("===> Sync Mac with MII master\n");
			dev->dev_addr[0] = pAd->master->dev_addr[0];
			dev->dev_addr[1] = pAd->master->dev_addr[1];
			dev->dev_addr[2] = pAd->master->dev_addr[2];
			dev->dev_addr[3] = pAd->master->dev_addr[3];
			dev->dev_addr[4] = pAd->master->dev_addr[4];
			dev->dev_addr[5] = pAd->master->dev_addr[5];
		}
		else
#endif
		{
			printk("===> Get MAC from iNIC\n");
			dev->dev_addr[0] = (u8)0x00;
			dev->dev_addr[1] = (u8)0x43;
			dev->dev_addr[2] = (u8)0x0C;
			dev->dev_addr[3] = (u8)0x00;
			dev->dev_addr[4] = (u8)0x00;
			dev->dev_addr[5] = (u8)0x00;
		}
	}
	else
	{
		rlk_inic_set_mac(pAd, conf_mac);
	}

	pAd->RaCfgObj.bRestartiNIC = FALSE;

	pAd->RaCfgObj.threads_exit = 0;
	pAd->RaCfgObj.wait_completed = 0;
	init_waitqueue_head(&pAd->RaCfgObj.waitQH);


	sema_init(&pAd->RaCfgObj.taskSem, 0);
  sema_init(&pAd->RaCfgObj.backlogSem, 0);

	RTMP_SEM_INIT(&pAd->RaCfgObj.waitLock);
	RTMP_SEM_INIT(&pAd->RaCfgObj.timerLock);

#ifdef WOWLAN_SUPPORT
	RTMP_SEM_INIT(&pAd->RaCfgObj.WowInbandSignalTimerLock);
#endif // #ifdef WOWLAN_SUPPORT // 

	RaCfgQueueInit(&pAd->RaCfgObj.taskQueue, "task");
	RaCfgQueueInit(&pAd->RaCfgObj.backlogQueue, "backlog");
	RaCfgQueueInit(&pAd->RaCfgObj.waitQueue, "wait");

#ifdef TEST_BOOT_RECOVER
	pAd->RaCfgObj.cfg_simulated = FALSE;
	pAd->RaCfgObj.firm_simulated = FALSE;
#endif

	RaCfgInitThreads(pAd);

	/* if operation mode is STA */
	if (pAd->RaCfgObj.opmode == 0)
	{
		printk("====> WIRELESS_EXT=%d, HANDLER_VERSION=%d\n", WIRELESS_EXT, IW_HANDLER_VERSION);
#if WIRELESS_EXT >= 12
#if IW_HANDLER_VERSION < 7
		//dev->get_wireless_stats = rlk_inic_get_wireless_stats;
#endif // IW_HANDLER_VERSION < 7 //
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,21) || defined(CONFIG_WIRELESS_EXT)
		//dev->wireless_handlers = (struct iw_handler_def *) &rlk_inic_iw_handler_def;
		dev->wireless_handlers = get_wireless_handler();
#endif
#endif
	}
#if (CONFIG_INF_TYPE==INIC_INF_TYPE_MII)

#ifdef CONFIG_CONCURRENT_INIC_SUPPORT
	/* The hook only need be changed one time. */
	if(pAd->RaCfgObj.InterfaceNumber == 0)
#endif // CONFIG_CONCURRENT_INIC_SUPPORT //		
	racfg_inband_hook_init(pAd);
#endif
}

void RaCfgExit(iNIC_PRIVATE *pAd)
{
	RaCfgDelHeartBeatTimer(pAd);

#ifdef WOWLAN_SUPPORT	
	RaCfgDelHeartWowInbandTimer(pAd);
#endif // WOWLAN_SUPPORT //	
	
	
#if (CONFIG_INF_TYPE==INIC_INF_TYPE_MII)
	racfg_inband_hook_cleanup();
	if (pAd->RaCfgObj.eapol_skb != NULL)
	{
		kfree_skb(pAd->RaCfgObj.eapol_skb);
	}
#endif

	if (pAd->RaCfgObj.opmode && pAd->RaCfgObj.BssidNum > 1)
	{
		rlk_inic_mbss_remove(pAd);
	}
	if (pAd->RaCfgObj.bWds)
	{
		rlk_inic_wds_remove(pAd);
	}
	if (pAd->RaCfgObj.bApcli)
	{
		rlk_inic_apcli_remove(pAd);
	}
#ifdef MESH_SUPPORT
	if (pAd->RaCfgObj.bMesh)
	{
		rlk_inic_mesh_remove(pAd);
	}
#endif // MESH_SUPPORT //
	RaCfgKillThread(pAd);

#ifndef NM_SUPPORT
	unregister_netdevice_notifier(&pAd->RaCfgObj.net_dev_notifier);
	printk("unregister_netdevice_notifier ok\n");
#endif

#ifdef WOWLAN_SUPPORT 
	unregister_cpu_notifier(&pAd->RaCfgObj.pm_wow_notifier);
	pAd->RaCfgObj.pm_wow_state 	= WOW_CPU_UP;  
#endif // WOWLAN_SUPPORT  //

}

static void RaCfgKillThread(iNIC_PRIVATE *pAd)
{
	int ret;

	pAd->RaCfgObj.threads_exit = 1;
	up(&pAd->RaCfgObj.taskSem);   
	up(&pAd->RaCfgObj.backlogSem);   

	if (pAd->RaCfgObj.task_thread_pid > 0)
	{
		ret = KILL_THREAD_PID (GET_PID(pAd->RaCfgObj.task_thread_pid), SIGTERM, 1);
		if (ret)
		{
			printk (KERN_WARNING "unable to kill RaCfg task thread, pid=%d, ret=%d!\n",
					pAd->RaCfgObj.task_thread_pid, ret);
		}
		wait_for_completion(&pAd->RaCfgObj.TaskThreadComplete);
		pAd->RaCfgObj.task_thread_pid = -1;
	}

	if (pAd->RaCfgObj.backlog_thread_pid > 0)
	{
		ret = KILL_THREAD_PID (GET_PID(pAd->RaCfgObj.backlog_thread_pid), SIGTERM, 1);
		if (ret)
		{
			printk (KERN_WARNING "unable to kill RaCfg backlog thread, pid=%d, ret=%d!\n",
					pAd->RaCfgObj.backlog_thread_pid, ret);
		}
		wait_for_completion (&pAd->RaCfgObj.BacklogThreadComplete);
		pAd->RaCfgObj.backlog_thread_pid = -1;
	}

	//wait_for_completion (&pAd->notify);

	printk("RaCfgTaskThread Complete and Exit\n");
	printk("RaCfgBacklogThread Complete and Exit\n");
}

static void RaCfgInitThreads(iNIC_PRIVATE *pAd)
{
	printk("============= Init Thread ===================\n");
	pAd->RaCfgObj.threads_exit = 0;
	init_completion(&pAd->RaCfgObj.TaskThreadComplete);
	init_completion(&pAd->RaCfgObj.BacklogThreadComplete);
	pAd->RaCfgObj.task_thread_pid = kernel_thread(RaCfgTaskThread, pAd, CLONE_VM);
	pAd->RaCfgObj.backlog_thread_pid = kernel_thread(RaCfgBacklogThread, pAd, CLONE_VM);
	printk("RacfgTaskThread pid = %d\n", pAd->RaCfgObj.task_thread_pid);
	printk("RacfgBacklogThread pid = %d\n", pAd->RaCfgObj.backlog_thread_pid);

}


static void RaCfgHeartBeatTimeOut(uintptr_t arg)
{
	iNIC_PRIVATE *pAd = (iNIC_PRIVATE *)arg;

#ifndef NM_SUPPORT
	if (!NETIF_IS_UP(pAd->RaCfgObj.MainDev))
		return;
#endif

	if (pAd->RaCfgObj.threads_exit)
		return;

	if (pAd->RaCfgObj.fw_upload_counter >= max_fw_upload)
	{
#ifdef NM_SUPPORT
		hardware_reset(pAd, 0);
#endif
		return;
	}

#if (CONFIG_INF_TYPE==INIC_INF_TYPE_MII)
	if (!(pAd->master->flags & IFF_PROMISC))
	{
		pAd->master->flags |= IFF_PROMISC;
		printk("Skip time out, set master to PROMISC\n");
		return;
	}
#endif

#if 0
	// 1. Check Threads, be sure not executed after:
	//    timer deleted == RaCfgReset == ra0 close
	RTMP_SEM_LOCK(&pAd->RaCfgObj.timerLock);
	if (NETIF_IS_UP(pAd->RaCfgObj.MainDev) && pAd->RaCfgObj.threads_exit)
	{
		// Avoid crashing when rebooting under ra0 is up
		if (pAd->RaCfgObj.RebootCount++ < 1)
		{
			printk("skip: %d\n", pAd->RaCfgObj.RebootCount);
			goto skip;
		}
		pAd->RaCfgObj.RebootCount = 0;


		pAd->RaCfgObj.threads_exit = 0;
		RaCfgQueueReset(&pAd->RaCfgObj.taskQueue, &pAd->RaCfgObj.taskSem);
		RaCfgQueueReset(&pAd->RaCfgObj.backlogQueue, &pAd->RaCfgObj.backlogSem);

		init_completion(&pAd->RaCfgObj.TaskThreadComplete);
		init_completion(&pAd->RaCfgObj.BacklogThreadComplete);
		pAd->RaCfgObj.task_thread_pid = 
		kernel_thread(RaCfgTaskThread, pAd, CLONE_VM);
		pAd->RaCfgObj.backlog_thread_pid = 
		kernel_thread(RaCfgBacklogThread, pAd, CLONE_VM);

		printk("\n==================================\n");
		printk("RacfgTaskThread restart pid = %d\n", 
			   pAd->RaCfgObj.task_thread_pid);
		printk("RacfgBacklogThread restart pid = %d\n", 
			   pAd->RaCfgObj.backlog_thread_pid);
		printk("==================================\n\n");
	}
	skip:
	RTMP_SEM_UNLOCK(&pAd->RaCfgObj.timerLock);
#endif

	// 2. check iNIC HeartBeat
	if (pAd->RaCfgObj.HeartBeatCount == 0)
	{
		printk("==== RaCfgCheckHeartBeatTimeOut! ===\n");
#ifdef TEST_BOOT_RECOVER
		pAd->RaCfgObj.cfg_simulated = FALSE;
		pAd->RaCfgObj.firm_simulated = FALSE;
#endif

		RaCfgEnqueue(pAd, &pAd->RaCfgObj.backlogQueue, &pAd->RaCfgObj.backlogSem, RaCfgRestartiNIC, (uintptr_t)pAd);
	}

	pAd->RaCfgObj.HeartBeatCount = 0;   
	mod_timer(&pAd->RaCfgObj.heartBeat, jiffies + HEART_BEAT_TIMEOUT * HZ);
}

static void RaCfgEnqueueRestart(iNIC_PRIVATE *pAd)
{

	RaCfgEnqueue(pAd, &pAd->RaCfgObj.backlogQueue, &pAd->RaCfgObj.backlogSem, rlk_inic_mbss_restart, (uintptr_t)pAd);
	RaCfgEnqueue(pAd, &pAd->RaCfgObj.backlogQueue, &pAd->RaCfgObj.backlogSem, rlk_inic_wds_restart, (uintptr_t)pAd);
	RaCfgEnqueue(pAd, &pAd->RaCfgObj.backlogQueue, &pAd->RaCfgObj.backlogSem, rlk_inic_apcli_restart, (uintptr_t)pAd);
#ifdef MESH_SUPPORT
	RaCfgEnqueue(pAd, &pAd->RaCfgObj.backlogQueue, &pAd->RaCfgObj.backlogSem, rlk_inic_mesh_restart, (uintptr_t)pAd);
#endif // MESH_SUPPORT // 

}

static void RaCfgRestartiNIC(uintptr_t arg)
{
	iNIC_PRIVATE *pAd = (iNIC_PRIVATE *)arg;
#ifdef CONFIG_CONCURRENT_INIC_SUPPORT
	int i = 0;
#endif // CONFIG_CONCURRENT_INIC_SUPPORT //

	pAd->RaCfgObj.bRestartiNIC = TRUE;
	printk("=========== RaCfgRestartiNIC begin =========\n");
	RaCfgCloseFile(pAd, &pAd->RaCfgObj.firmware);
#ifdef CONFIG_CONCURRENT_INIC_SUPPORT
	for(i = 0; i < CONCURRENT_CARD_NUM; i++)
	{
		RaCfgCloseFile(pAd, &ConcurrentObj.Profile[i]);
		if (gAdapter[i]->RaCfgObj.bExtEEPROM)
		{
			RaCfgCloseFile(gAdapter[i], &ConcurrentObj.ExtEeprom[i]);
		}		
	}
#else
	RaCfgCloseFile(pAd, &pAd->RaCfgObj.profile);
	if (pAd->RaCfgObj.bExtEEPROM)
		RaCfgCloseFile(pAd, &pAd->RaCfgObj.ext_eeprom);
#endif // CONFIG_CONCURRENT_INIC_SUPPORT //

	
#if (CONFIG_INF_TYPE==INIC_INF_TYPE_PCI)
	rlk_inic_close(pAd->dev);
	rlk_inic_open(pAd->dev);  
#elif (CONFIG_INF_TYPE==INIC_INF_TYPE_USB)
	RaCfgStateReset(pAd);
	close_all_interfaces((struct net_device *)pAd->dev);
#else
	RaCfgStateReset(pAd);
	RaCfgSetUp(pAd, pAd->RaCfgObj.MBSSID[0].MSSIDDev);
	printk("Please issue GPIO to Reset iNIC\n");
#endif
	pAd->RaCfgObj.bRestartiNIC = FALSE;
	printk("=========== RaCfgRestartiNIC done!=========\n");
}


void RaCfgSetUp(iNIC_PRIVATE *pAd, struct net_device *dev)
{
	int i;

	printk("Op mode = %d\n", pAd->RaCfgObj.opmode);

#if 0 //def CONFIG_RT2880_INIC_MII
	dev_change_flags(pAd->master, pAd->master->flags | IFF_PROMISC);
#endif

	if (pAd->RaCfgObj.opmode)
	{
		for (i=0; i<MAX_MBSSID_NUM; i++)
		{
			pAd->RaCfgObj.MBSSID[i].vlan_id = 0;
		}
	}
	
#ifdef CONFIG_CONCURRENT_INIC_SUPPORT
	/* After the first time opening, read all profile to setting attribute*/
	if(ConcurrentObj.CardCount == 0)
	{
		for(i=0; i<CONCURRENT_CARD_NUM; i++)
		{
			printk("Read profile[%d]\n", i);
			rlk_inic_read_profile(gAdapter[i]);		
		}
	}
#else
	rlk_inic_read_profile(pAd);
#endif // CONFIG_CONCURRENT_INIC_SUPPORT //

	if (pAd->RaCfgObj.opmode)
	{
		if (pAd->RaCfgObj.bApcli && ((pAd->RaCfgObj.BssidNum+1) > MAX_MBSSID_NUM))
		{
			// reserve a mbss for apcli
			pAd->RaCfgObj.BssidNum = MAX_MBSSID_NUM - 1;
			printk("%d BSS, one ApClient\n", pAd->RaCfgObj.BssidNum);
		}

#ifdef MESH_SUPPORT
		if (pAd->RaCfgObj.bApcli && pAd->RaCfgObj.bMesh && ((pAd->RaCfgObj.BssidNum+2) > MAX_MBSSID_NUM))
		{
			// reserve a mbss for mesh
			pAd->RaCfgObj.BssidNum = MAX_MBSSID_NUM - 2;
			printk("%d BSS, one MESH , one ApClient\n", pAd->RaCfgObj.BssidNum);
		}
		else if (!pAd->RaCfgObj.bApcli && pAd->RaCfgObj.bMesh && ((pAd->RaCfgObj.BssidNum+1) > MAX_MBSSID_NUM))
		{
			// reserve a mbss for mesh
			pAd->RaCfgObj.BssidNum = MAX_MBSSID_NUM - 1;
			printk("%d BSS, one MESH\n", pAd->RaCfgObj.BssidNum);

		}
#endif // MESH_SUPPORT //

		if (pAd->RaCfgObj.BssidNum > 1)
		{
			rlk_inic_mbss_init(dev, pAd); 
		}
		if (pAd->RaCfgObj.bWds)
		{
			rlk_inic_wds_init(dev, pAd);
		}
		if (pAd->RaCfgObj.bApcli)
		{
			rlk_inic_apcli_init(dev, pAd);
		}

	}
	else
	{
	}

#ifdef MESH_SUPPORT
	if (pAd->RaCfgObj.bMesh)
	{
		rlk_inic_mesh_init(dev, pAd);
	}
#endif // MESH_SUPPORT //


#ifndef NM_SUPPORT	
	//
	// profile must be readed before enable flg_is_open
	// ex: ExtEEPROM=1
	//
	pAd->RaCfgObj.flg_is_open = 1;
#endif	

	// Executing in task thread implies => boot fail 
	// => don't wait for MAC (iNIC not booted yet, no MAC returned)
	// => at the same time, we avoid blocking task thread
	if (current->pid != pAd->RaCfgObj.task_thread_pid)
	{
		// system thread or backlog (heartbeat time out)
		get_mac_from_inic(pAd);
	}
	RaCfgAddHeartBeatTimer(pAd);

#ifdef WOWLAN_SUPPORT
	RaCfgAddWowInbandTimer(pAd);
#endif // WOWLAN_SUPPORT //

}

int RaCfgDropRemoteInBandFrame(struct sk_buff *skb)
{
	int Status = 0;

	if (*((u16 *)&skb->data[12]) == htons(0xFFFF))
	{
		struct racfghdr *hdr = (struct racfghdr*)(&skb->data[14]);

		if (hdr->magic_no == cpu_to_le32(MAGIC_NUMBER))
		{
			dev_kfree_skb(skb);
			Status = 1;  
		}
	}

	return Status;
}

static int get_mac_from_inic(iNIC_PRIVATE *pAd)
{
	int ret = 0;

	if (init_flag) {
		pAd->RaCfgObj.bGetMac = TRUE;
		pAd->RaCfgObj.fw_upload_counter = 0;
		return ret;
	}
#ifdef CONFIG_CONCURRENT_INIC_SUPPORT
	if(ConcurrentObj.CardCount== 0)
#endif // #ifdef CONFIG_CONCURRENT_INIC_SUPPORT //		
	printk("Wait for boot done...\n");
	pAd->RaCfgObj.bGetMac = FALSE;

#ifdef CONFIG_CONCURRENT_INIC_SUPPORT
	if(ConcurrentObj.CardCount== 0)
#endif // #ifdef CONFIG_CONCURRENT_INIC_SUPPORT //		
	ret = RaCfgWaitSyncRsp(pAd, RACFG_CMD_GET_MAC, 0, NULL, NULL, GET_MAC_TIMEOUT); // 1 hours = infinitely 
	if (ret)
	{
		printk("ERROR! can't get target's MAC address, boot fail.\n");
		pAd->RaCfgObj.fw_upload_counter++;
#ifdef NM_SUPPORT
		we_fw_upload_failed(pAd);
#endif
	}
	else
	{
		pAd->RaCfgObj.bGetMac = TRUE;
		pAd->RaCfgObj.fw_upload_counter = 0;
		
#ifdef CONFIG_CONCURRENT_INIC_SUPPORT
		/* All interface is radio off after iNIC boot. 
		 * Set radio on when getting MAC. */	
		SetRadioOn(pAd, 1); 
#endif // CONFIG_CONCURRENT_INIC_SUPPORT //

		
#ifdef NM_SUPPORT
		we_fw_uploaded(pAd);
#endif
		init_flag = 1;
	}
	return ret;
}


void RaCfgStateReset(iNIC_PRIVATE *pAd)
{

#ifndef NM_SUPPORT
	pAd->RaCfgObj.flg_is_open = 0;
	RaCfgDelHeartBeatTimer(pAd);

#ifdef WOWLAN_SUPPORT	
	RaCfgDelHeartWowInbandTimer(pAd);
#endif // WOWLAN_SUPPORT //		
	
#endif
	/* Be sure to reset bGetMac, otherwise under syncmiimac<>0 mode,
	   if down+if up will cause unicast uploading profile/firmware,
	   which won't be received by boot ROM */
	pAd->RaCfgObj.bGetMac = FALSE;
#if (CONFIG_INF_TYPE == INIC_INF_TYPE_MII)
#ifdef PHASE_LOAD_CODE
	pAd->RaCfgObj.bLoadPhase = FALSE;	
#endif
#endif
	pAd->RaCfgObj.wait_completed = 0;
	RaCfgQueueReset(&pAd->RaCfgObj.taskQueue, &pAd->RaCfgObj.taskSem);
	RaCfgQueueReset(&pAd->RaCfgObj.backlogQueue, &pAd->RaCfgObj.backlogSem);
	RaCfgQueueReset(&pAd->RaCfgObj.waitQueue, NULL);
}


static int RaCfgBacklogThread(void *arg)
{
	iNIC_PRIVATE *pAd = (iNIC_PRIVATE *)arg; 

	KERNEL_THREAD_BEGIN("RaCfg Backlog");

	while (!pAd->RaCfgObj.threads_exit)
	{
		RaCfgDequeue(pAd, &pAd->RaCfgObj.backlogQueue, &pAd->RaCfgObj.backlogSem); 
	}

	pAd->RaCfgObj.backlog_thread_pid = -1;  
	complete_and_exit(&pAd->RaCfgObj.BacklogThreadComplete, 0);
	printk("%s: RaCfgBacklogThread Exit Complete\n", pAd->RaCfgObj.MainDev->name);
}

static int RaCfgTaskThread(void *arg)
{
	struct sk_buff *skb;
	struct racfghdr *p_racfgh;
	char *payload;
	struct net_device *dev = NULL;
	u16  cmd, len, sequence, dev_id, dev_type;
	u32  event;
	iNIC_PRIVATE *pAd = (iNIC_PRIVATE *)arg; 
	u8  *buffer=NULL, *ptr;

	KERNEL_THREAD_BEGIN("RaCfg Task");

	while (!pAd->RaCfgObj.threads_exit)
	{
		skb = (struct sk_buff *)RaCfgDequeue(pAd, &pAd->RaCfgObj.taskQueue, &pAd->RaCfgObj.taskSem); 
		//printk("Dequeue Feedback resp...\n");
		if (!skb)
		{
			// Queue elem may be a task func, has been invoked in dequeue.
			continue;
		}
		// feedback command ...
		p_racfgh =  (struct racfghdr *)skb->data;
		cmd      = le16_to_cpu(p_racfgh->command_id);
		len      = le16_to_cpu(p_racfgh->length);
		sequence = le16_to_cpu(p_racfgh->sequence);
		dev_id   = le16_to_cpu(p_racfgh->dev_id);
		dev_type = le16_to_cpu(p_racfgh->dev_type);
		event    = le32_to_cpu(p_racfgh->status);
		payload  = (char *)p_racfgh->data;

		switch (cmd)
		{
		case RACFG_CMD_CONSOLE:
			if (payload[len-1] != 0x00)	// not a terminial of string
			{
				printk("Warning! not null-end string, len=%d:\n%s\n", len, payload);
				payload[len-1] = 0x00;
			}
			printk("%s", payload);
			break;
		case RACFG_CMD_WSC_UPDATE_CFG:
			if (event != 0)
			{
				printk("ERROR! WSC completed, but profile update fail!\n");
				break;
			}

#ifdef CONFIG_CONCURRENT_INIC_SUPPORT
			if((pAd->RaCfgObj.InterfaceNumber >= 0)&&(pAd->RaCfgObj.InterfaceNumber <= CONCURRENT_CARD_NUM))
			{
				int idx = pAd->RaCfgObj.InterfaceNumber;
				RLK_STRCPY(pAd->RaCfgObj.profile.name, ConcurrentObj.Profile[idx].read_name);
			}
#endif // CONFIG_CONCURRENT_INIC_SUPPORT //

#ifdef DBG
			{		
				char buf[MAX_FILE_NAME_SIZE]; 
				snprintf(buf, sizeof(buf), "%s%s", root, pAd->RaCfgObj.profile.name);
				RLK_STRCPY(pAd->RaCfgObj.profile.name, buf);
			}
#endif

			buffer = &pAd->RaCfgObj.wsc_updated_profile[0];
			ptr = buffer + sequence * MAX_FEEDBACK_LEN;

			if (len > 0)
			{
				if (sequence == 0)
					memset(buffer, 0, MAX_PROFILE_SIZE);

				pAd->RaCfgObj.profile.seq = sequence;
				memcpy(ptr, payload, len);
				printk("WSC_UPDATE_CFG, received %d bytes\n", len);

				SendRaCfgCommand(pAd, RACFG_CMD_TYPE_ASYNC & RACFG_CMD_TYPE_PASSIVE_MASK, RACFG_CMD_WSC_UPDATE_CFG, len, pAd->RaCfgObj.profile.seq, 0, 0, 0, ptr);
				ptr += len;
			}
			printk("%s", buffer);
			if (len < MAX_FEEDBACK_LEN)
			{
				DBGPRINT("WSC_UPDATE_CFG Done\n");      
				// write cfg file
				RaCfgOpenFile(pAd, &pAd->RaCfgObj.profile, O_RDWR);
				RaCfgWriteFile(pAd, &pAd->RaCfgObj.profile, buffer, (ptr-buffer));
				RaCfgCloseFile(pAd, &pAd->RaCfgObj.profile);
			}
			break;
		case RACFG_CMD_EXT_EEPROM_UPDATE: 
			if (event != 0)
			{
				printk("ERROR! EXT_EEPROM_UPDATE completed, but profile update fail!\n");
				break;
			}

#ifdef CONFIG_CONCURRENT_INIC_SUPPORT
			if((pAd->RaCfgObj.InterfaceNumber >= 0)&&(pAd->RaCfgObj.InterfaceNumber <= CONCURRENT_CARD_NUM))
			{
				int idx = pAd->RaCfgObj.InterfaceNumber;
				RLK_STRCPY(pAd->RaCfgObj.ext_eeprom.name, ConcurrentObj.ExtEeprom[idx].read_name);
			}

#endif // CONFIG_CONCURRENT_INIC_SUPPORT //


#ifdef DBG
			{		
				char buf[MAX_FILE_NAME_SIZE]; 
				snprintf(buf, sizeof(buf), "%s%s", root, pAd->RaCfgObj.ext_eeprom.name);
				RLK_STRCPY(pAd->RaCfgObj.ext_eeprom.name, buf);
			}
#endif

			/* share wsc buffer */
			buffer = &pAd->RaCfgObj.wsc_updated_profile[0];
			ptr = buffer + sequence * MAX_FEEDBACK_LEN;

			if (len > 0)
			{
				if (sequence == 0)
					memset(buffer, 0, sizeof(buffer));

				pAd->RaCfgObj.ext_eeprom.seq = sequence;
				memcpy(ptr, payload, len);
				printk("EXT_EEPROM_UPDATE, received %d bytes\n", len);
				SendRaCfgCommand(pAd, RACFG_CMD_TYPE_ASYNC & RACFG_CMD_TYPE_PASSIVE_MASK, RACFG_CMD_EXT_EEPROM_UPDATE, len, pAd->RaCfgObj.ext_eeprom.seq, 0, 0, 0, ptr);
				ptr += len;

			}
			if (len < MAX_FEEDBACK_LEN)
			{
				DBGPRINT("EXT_EEPROM_UPDATE Done\n");      
				// write Ext EEPROM file
				RaCfgOpenFile(pAd, &pAd->RaCfgObj.ext_eeprom, O_RDWR);
				RaCfgWriteFile(pAd, &pAd->RaCfgObj.ext_eeprom, buffer, (ptr - buffer));
				RaCfgCloseFile(pAd, &pAd->RaCfgObj.ext_eeprom);
			}
			break;

		case RACFG_CMD_WIRELESS_SEND_EVENT:

			if (dev_type & DEV_TYPE_APCLI_FLAG)
				dev = pAd->RaCfgObj.APCLI[dev_id].MSSIDDev;
			else if (dev_type & DEV_TYPE_WDS_FLAG)
				dev = pAd->RaCfgObj.WDS[dev_id].MSSIDDev;
#ifdef MESH_SUPPORT
			else if (dev_type & DEV_TYPE_MESH_FLAG)
				dev = pAd->RaCfgObj.MESH[dev_id].MSSIDDev;
#endif // MESH_SUPPORT //
			else
				dev	= pAd->RaCfgObj.MBSSID[dev_id].MSSIDDev;

			ASSERT(dev);
			{
				union iwreq_data wrqu;

				// iw_point type 
				memset(&wrqu, 0, sizeof(wrqu));
				wrqu.data.length = len;
				wrqu.data.flags  = le16_to_cpu(p_racfgh->flags);
				wireless_send_event(dev, event, &wrqu, payload);

				if (wrqu.data.flags == RT_ASSOC_EVENT_FLAG)
					netif_carrier_on(dev);
				else if (wrqu.data.flags == RT_DISASSOC_EVENT_FLAG)
					netif_carrier_off(dev);
			}

#ifdef INBAND_DEBUG
			hex_dump("Wireless Event recevied:", skb->data, skb->len);
#endif
			break;

		case RACFG_CMD_WIRELESS_SEND_EVENT2:

			if (dev_type & DEV_TYPE_APCLI_FLAG)
				dev = pAd->RaCfgObj.APCLI[dev_id].MSSIDDev;
			else if (dev_type & DEV_TYPE_WDS_FLAG)
				dev = pAd->RaCfgObj.WDS[dev_id].MSSIDDev;
#ifdef MESH_SUPPORT
			else if (dev_type & DEV_TYPE_MESH_FLAG)
				dev = pAd->RaCfgObj.MESH[dev_id].MSSIDDev;
#endif // MESH_SUPPORT //
			else
				dev	= pAd->RaCfgObj.MBSSID[dev_id].MSSIDDev;

			ASSERT(dev)
			{
				union iwreq_data wrqu;
				memcpy(&wrqu, payload, sizeof(wrqu));
				wireless_send_event(dev, event, &wrqu, NULL);

				if (wrqu.data.flags == RT_ASSOC_EVENT_FLAG)
					netif_carrier_on(dev);
				else if (wrqu.data.flags == RT_DISASSOC_EVENT_FLAG)
					netif_carrier_off(dev);
			}
#ifdef INBAND_DEBUG
			hex_dump("Wireless Event2 recevied:", skb->data, skb->len);
#endif
			break;
		case RACFG_CMD_SEND_DAEMON_SIGNAL:
			//printk("Receive IAPP Signal: pid = %d\n", event);
			if (event != 0)
			{
				switch (dev_id)
				{
				case RACFG_SIGUSR1:
					KILL_THREAD_PID(GET_PID(event), SIGUSR1, 0);
					//printk("Send USR1 to pid=%d\n", event);
					break;
				case RACFG_SIGUSR2:
					KILL_THREAD_PID(GET_PID(event), SIGUSR2, 0);
					break;
				default:
					printk("ERROR! invalid racfg signal id (%d)\n", dev_id);
					break;
				}
			}
			break;

		}
		kfree_skb(skb);
	}

	pAd->RaCfgObj.task_thread_pid = -1;  
	complete_and_exit(&pAd->RaCfgObj.TaskThreadComplete, 0);
	printk("%s: RaCfgTaskThread Exit Complete\n", pAd->RaCfgObj.MainDev->name);
	return 0;
}

void RaCfgOpenFile(iNIC_PRIVATE *pAd, FileHandle *fh, int flag)
{
	if (fh->fp)	return;

	printk("Open file: %s\n", fh->name);

	fh->seq  = 0;
	fh->fp = filp_open(fh->name, flag, 0);
	if (IS_ERR(fh->fp) || !fh->fp->f_op)
	{
		printk("RaCfgOpenFile--> Open file fail: %s\n", fh->name);
		fh->fp = NULL;
		return;
	}
	if ((flag == O_RDONLY || flag == O_RDWR) && !fh->fp->f_op->read)
	{
		printk("RaCfgOpenFile: No read method\n");
		return;
	}
	if ((flag == O_WRONLY || flag == O_RDWR) && !fh->fp->f_op->write)
	{
		printk("RaCfgOpenFile: No write method\n");
		return;
	}


	fh->seq = 0;
	pAd->RaCfgObj.orgfs = get_fs();
	set_fs(KERNEL_DS);
}

void RaCfgCloseFile(iNIC_PRIVATE *pAd, FileHandle *fh)
{

	if (!fh->fp)
		return;

	set_fs(pAd->RaCfgObj.orgfs);
	if (filp_close(fh->fp, NULL))
		printk("RaCfgLoadFile--> Close file error: %s\n", fh->name);
	fh->fp = NULL;
	fh->seq = 0;
	fh->crc = 0;
}

void RaCfgWriteFile(iNIC_PRIVATE *pAd, FileHandle *fh, char *buff, int len)
{
	if (!fh->fp)
		return;

	if (!fh->fp->f_op->write)
	{
		printk("File System hasn't write handler to write file %s\n", fh->name);
		return;
	}

	fh->fp->f_op->write(fh->fp, buff, len, &fh->fp->f_pos);
}

/*! \brief  Initialize The RaCfgQueue
 *  \param  *Queue     The RaCfg Queue
 *  \pre
 *  \post
 *  \note   Because this is done only once (at the init stage), no need to be locked
 */
static void RaCfgQueueInit(RaCfgQueue *queue, char *name) 
{
	int i;

	RTMP_SEM_INIT(&queue->lock);

	strncpy(queue->name, name, sizeof(queue->name)-1);
	queue->name[sizeof(queue->name)-1]='\0';
	queue->num  = 0;
	queue->head = 0;
	queue->tail = 0;

	for (i = 0; i < MAX_LEN_OF_RACFG_QUEUE; i++)
	{
		queue->entry[i].func = NULL;
		queue->entry[i].arg  = 0;
	}
}

/*! \brief   The destructor of RaCfg Queue
 *  \param 
 *  \return
 *  \pre
 *  \post
 *  \note   Clear RaCfg Queue, Set queue->num to Zero.
 */
static void RaCfgQueueReset(RaCfgQueue *queue, struct semaphore *sem) 
{
	int i;

	RTMP_SEM_LOCK(&(queue->lock));
	queue->num  = 0;
	queue->head = 0;
	queue->tail = 0;

	for (i = 0; i < MAX_LEN_OF_RACFG_QUEUE; i++)
	{
		queue->entry[i].func = NULL;
		queue->entry[i].arg  = 0;
	}

	RTMP_SEM_UNLOCK(&(queue->lock));

	if (sem)
	{
		// clear sem->count
		while (!down_trylock(sem));
	}

}

/*! \brief   Enqueue a task for other threads
 *  \param  *Func     The task pointer
 *  \param  arg       The task's arg
 *  \return  0 if enqueue is successful, 1 if the queue is full
 *  \pre
 *  \post
 *  \note    The arg has to be initialized
 */
static int RaCfgEnqueue(iNIC_PRIVATE *pAd, RaCfgQueue *queue, struct semaphore *sem, PRaCfgFunc func, uintptr_t arg)
{
	int tail;

	if (RaCfgQueueFull(queue))
	{
		printk("RaCfgEnqueue: full (%d tasks pending in %s), task dropped.\n", 
			   queue->num, queue->name);
		return 1;
	}

	RTMP_SEM_LOCK(&(queue->lock));
	tail = queue->tail;
	queue->tail++;
	queue->num++;
	if (queue->tail == MAX_LEN_OF_RACFG_QUEUE)
	{
		queue->tail = 0;
	}

	queue->entry[tail].func = func;
	queue->entry[tail].arg  = arg;

	RTMP_SEM_UNLOCK(&(queue->lock));

	if (sem) up(sem);
	return 0;
}

/*! \brief   Dequeue a task from the RaCfg Queue, and execute it
 *  \param  *Elem     The task dequeued from RaCfgQueue
 *  \return  pointer to the queued argument
 *  \pre
 *  \post

 IRQL = DISPATCH_LEVEL

 */
static uintptr_t RaCfgDequeue(iNIC_PRIVATE *pAd, RaCfgQueue *queue, struct semaphore *sem)
{
	PRaCfgFunc task;
	uintptr_t arg = 0;

	if (sem)
	{
		if (down_interruptible(sem) == -EINTR)
		{
			// Catch System Reset Signal
			printk("\n\nNote!! Signal caught, all threads broken...\n\n");
			pAd->RaCfgObj.threads_exit = 1;
			return arg;
		}
		else
		{
			if (pAd->RaCfgObj.threads_exit)
			{
				printk("ERROR! all threads has been exited!\n");
				return arg;
			}

		}
	}
	if (queue->num == 0)
	{
		printk("ERROR! RaCfgDequeue while queue num=%d\n", queue->num);
		return 0;
	}


	RTMP_SEM_LOCK(&(queue->lock));
	task = queue->entry[queue->head].func;
	arg  = queue->entry[queue->head].arg;
	queue->num--;
	queue->head++;
	if (queue->head == MAX_LEN_OF_RACFG_QUEUE)
		queue->head = 0;

	RTMP_SEM_UNLOCK(&(queue->lock));

	if (!task) return arg;

	task(arg);
	return 0;
}


/*! \brief   test   if the RaCfg Queue is full
 *  \param   *queue The RaCfg Queue
 *  \return  1      if the queue is full, 0 otherwise
 *  \pre
 *  \post
 */
static int RaCfgQueueFull(RaCfgQueue *queue) 
{
	int ans;
	RTMP_SEM_LOCK(&(queue->lock));
	ans = (queue->num == MAX_LEN_OF_RACFG_QUEUE ? 1 : 0);
	RTMP_SEM_UNLOCK(&(queue->lock));
	return ans;
}

/*! \brief  test if the RaCfgQueue is empty
 *  \param  *queue    The RaCfgQueue
 *  \return 1 if the queue is empty, 0 otherwise
 *  \pre
 *  \post
 */
static int RaCfgQueueEmpty(RaCfgQueue *queue) 
{
	int ans;

	RTMP_SEM_LOCK(&(queue->lock));
	ans = (queue->num == 0 ? 1 : 0);
	RTMP_SEM_UNLOCK(&(queue->lock));

	return ans;
}

static ArgBox *GetArgBox(void)
{
	return kmalloc(sizeof(ArgBox), MEM_ALLOC_FLAG);
}

static void FreeArgBox(ArgBox *box)
{
	kfree(box);
}

#ifdef RETRY_PKT_SEND
static void upload_timeout(uintptr_t arg)
{
	iNIC_PRIVATE *pAd = (iNIC_PRIVATE *)arg;
	
	if(pAd->RaCfgObj.RPKTInfo.retry > 0){
		SendRaCfgCommand(pAd, 
				 RACFG_CMD_TYPE_BOOTSTRAP & RACFG_CMD_TYPE_PASSIVE_MASK, 
				 pAd->RaCfgObj.RPKTInfo.BootType, pAd->RaCfgObj.RPKTInfo.length, pAd->RaCfgObj.RPKTInfo.Seq, 0, 0, 0, pAd->RaCfgObj.RPKTInfo.buffer);
		pAd->RaCfgObj.RPKTInfo.retry--;

		// turn-on timer
		mod_timer(&pAd->RaCfgObj.uploadTimer, jiffies + RetryTimeOut*HZ/1000);
	}else{
		// delete timer
		if (pAd->RaCfgObj.uploadTimer.function)
		{
			del_timer_sync(&pAd->RaCfgObj.uploadTimer);
			pAd->RaCfgObj.uploadTimer.function = NULL;
			pAd->RaCfgObj.uploadTimer.data = 0;
		}
	}
}
#endif

static void _upload_firmware(iNIC_PRIVATE *pAd)
{
	int len;
	FileHandle *fh  = &pAd->RaCfgObj.firmware;
	//struct file *fp = fh->fp;
	struct file *fp;
	static int total = 0;
	unsigned char *last_hdr = 0;
	unsigned char buff[CRC_HEADER_LEN];

	if (!fh)	return;

	fp = fh->fp;
	if (!fp)	return;

	//len = read(fw_fd, buffer, MAX_FEEDBACK_LEN);
	len = fp->f_op->read(fp, pAd->RaCfgObj.upload_buf, MAX_FEEDBACK_LEN, &fp->f_pos);

	if (len > 0)
	{
		//DBGPRINT("(%d), len = %d\n", RaCfgObj.firmware.seq, len);
		fh->crc = crc32(fh->crc, pAd->RaCfgObj.upload_buf, len);

		if (len >= CRC_HEADER_LEN)
		{
			last_hdr = pAd->RaCfgObj.upload_buf + len - CRC_HEADER_LEN;
			if (len < MAX_FEEDBACK_LEN)
			{
				unsigned char *pp = (char *)(pAd->RaCfgObj.upload_buf + len - CRC_HEADER_LEN);
				int i = 0;
				printk("\nCRC:");
				for (i = 0; i < CRC_HEADER_LEN; pp++, i++)
					printk("%02x ", *pp);
				printk("\n");
			}
		}
		else
		{
			int slack = CRC_HEADER_LEN - len;
			char *curr = buff;
			//memcpy(curr, pAd->RaCfgObj.cmp_buf + MAX_FEEDBACK_LEN - slack, slack);
			memcpy(curr, &pAd->RaCfgObj.cmp_buf[MAX_FEEDBACK_LEN - slack], slack);
			curr += slack;
			memcpy(curr, pAd->RaCfgObj.upload_buf, len);
			last_hdr = buff;
		}

		memcpy(fh->hdr_src, last_hdr, CRC_HEADER_LEN);
		ReadCrcHeader(&fh->hdr, last_hdr);

		memcpy(pAd->RaCfgObj.cmp_buf, pAd->RaCfgObj.upload_buf, len);

#ifdef RETRY_PKT_SEND
	memcpy(pAd->RaCfgObj.RPKTInfo.buffer, pAd->RaCfgObj.upload_buf, len);
	pAd->RaCfgObj.RPKTInfo.Seq = fh->seq;
	pAd->RaCfgObj.RPKTInfo.BootType = RACFG_CMD_BOOT_UPLOAD;
	pAd->RaCfgObj.RPKTInfo.length = len;
	pAd->RaCfgObj.RPKTInfo.retry = FwRetryCnt;
	//turn-on timer
	if (!pAd->RaCfgObj.uploadTimer.function)
	{
		init_timer(&pAd->RaCfgObj.uploadTimer);
		pAd->RaCfgObj.uploadTimer.function = upload_timeout;
		pAd->RaCfgObj.uploadTimer.data = (uintptr_t)pAd;
	}
	mod_timer(&pAd->RaCfgObj.uploadTimer, jiffies + RetryTimeOut*HZ/1000);
#endif
		SendRaCfgCommand(pAd, 
						 RACFG_CMD_TYPE_BOOTSTRAP & RACFG_CMD_TYPE_PASSIVE_MASK, 
						 RACFG_CMD_BOOT_UPLOAD, len, fh->seq, 0, 0, 0, pAd->RaCfgObj.upload_buf);
		//printk("Send %d packet (%d bytes) to iNIC\n", fh->seq, len);
		fh->seq++;
		total += len;
		msleep (30);
	}
	else
	{
		u32 crc = 0;

#ifdef RETRY_PKT_SEND
	// delete timer
	if (pAd->RaCfgObj.uploadTimer.function)
	{
		del_timer_sync(&pAd->RaCfgObj.uploadTimer);
		pAd->RaCfgObj.uploadTimer.function = NULL;
		pAd->RaCfgObj.uploadTimer.data = 0;
	}
#endif

#if (CONFIG_INF_TYPE == INIC_INF_TYPE_MII)
#ifdef PHASE_LOAD_CODE
		if (!pAd->RaCfgObj.bLoadPhase)
		{
			SendRaCfgCommand(pAd, 
						 RACFG_CMD_TYPE_BOOTSTRAP & RACFG_CMD_TYPE_PASSIVE_MASK, 
						 RACFG_CMD_BOOT_STARTUP, 0, 0, 0, 0, 0, NULL);
			RaCfgCloseFile(pAd, fh);
			return;
		}
#endif
#endif
		if (fh->hdr.magic_no != 0x18140801)
		{
			printk("WARNING! %s has no crc record (magic_no=%04x),"
				   "no firmware integrity is guaranteed...\n", 
				   fh->name, fh->hdr.magic_no);
			goto done;
		}

		// final CRC = payload CRC + header CRC
		crc = crc32(fh->hdr.crc, fh->hdr_src, CRC_HEADER_LEN);

		if (fh->crc != crc)
		{
			printk("ERROR! CRC error %04x <> [%04x,%04x], size:%d<->%d:\n"
				   "\n==> %s is corrupted!\n\n", 
				   fh->crc, fh->hdr.crc, crc, 
				   total, fh->hdr.size,
				   fh->name);
			goto close;
		}

		printk("Send %s Firmware Done\n", DRV_NAME);        
		printk("===================================\n");
		printk("version: %s\n", fh->hdr.version);
		printk("size:    %d bytes\n", fh->hdr.size);
		printk("date:    %s\n", fh->hdr.date);
		printk("===================================\n\n");

		done:
		//RaCfgObj.firmware.seq = 0;
		//fp->f_op->lseek(fp, 0, SEEK_SET);
		DBGPRINT("Send STARTUP to %s\n", DRV_NAME);
		SendRaCfgCommand(pAd, 
						 RACFG_CMD_TYPE_BOOTSTRAP & RACFG_CMD_TYPE_PASSIVE_MASK, 
						 RACFG_CMD_BOOT_STARTUP, 0, 0, 0, 0, 0, NULL);
		close:
		DBGPRINT("Close Firmware file\n");      
		RaCfgCloseFile(pAd, fh);

	} 
}

void ReadCrcHeader(CRC_HEADER *hdr, unsigned char *buffer)
{
	hdr->magic_no = le32_to_cpu(*(u32 *)buffer);
	buffer += sizeof(u32);
	hdr->crc = le32_to_cpu(*(u32 *)buffer);
	buffer += sizeof(u32);
	strncpy(hdr->version, buffer, CRC_VERSION_SIZE - 1);
	hdr->version[CRC_VERSION_SIZE - 1] = '0';
	buffer += CRC_VERSION_SIZE;
	strncpy(hdr->date, buffer, CRC_DATE_SIZE - 1);
	hdr->date[CRC_DATE_SIZE - 1] = '0'; 
	buffer += CRC_DATE_SIZE;
	hdr->size = le32_to_cpu(*(u32 *)buffer);
}
// IN len: bytes already read from profile in this packet
// return: bytes appended in this packet
int _append_extra_profile(iNIC_PRIVATE *pAd, int len)
{
    char tmpstr[256], *pos = tmpstr;
	int  addlen = 0;
	int  ret    = 0;
	int  space  = MAX_FEEDBACK_LEN - len;
	int fpE2pStart, fpE2pEnd;
	struct file *fpE2p = NULL;

	if (space <= 0)
		return 0;

	snprintf(pos, sizeof("\n"), "\n");
	addlen += strlen(pos);
	pos    += strlen(pos);
#ifdef MULTIPLE_CARD_SUPPORT
	if (strlen(pAd->RaCfgObj.MC_MAC)&&(pAd->RaCfgObj.InterfaceNumber >= 0))
	{
		snprintf(pos, sizeof(tmpstr) - addlen, "__SkipIfNotINIC__MAC=%s\n", pAd->RaCfgObj.MC_MAC);
		addlen += strlen(pos);
		pos    += strlen(pos);

	}
	else
#endif // MULTIPLE_CARD_SUPPORT //
#ifdef CONFIG_CONCURRENT_INIC_SUPPORT
	if (strlen(ConcurrentObj.Mac[0]))
	{
		snprintf(pos, sizeof(tmpstr) - addlen, "__SkipIfNotINIC__MAC=%s\n", ConcurrentObj.Mac[0]);
		addlen += strlen(pos);
		pos    += strlen(pos);

	}
	else
#endif // CONFIG_CONCURRENT_INIC_SUPPORT //
		if (strlen(mac))
	{
		snprintf(pos, sizeof(tmpstr) - addlen, "__SkipIfNotINIC__MAC=%s\n", mac);
		addlen += strlen(pos);
		pos    += strlen(pos);

	}
#if (CONFIG_INF_TYPE==INIC_INF_TYPE_MII)
	else if (pAd->master && syncmiimac)
	{

		snprintf(pos, sizeof(tmpstr) - addlen, "__SkipIfNotINIC__MAC=%02x:%02x:%02x:%02x:%02x:%02x\n", 
				pAd->master->dev_addr[0],
				pAd->master->dev_addr[1],
				pAd->master->dev_addr[2],
				pAd->master->dev_addr[3],
				pAd->master->dev_addr[4],
				pAd->master->dev_addr[5]);
		addlen += strlen(pos);
		pos    += strlen(pos);
	}

	if (pAd->master && syncmiimac==0)
	{
		/* Note: MiiMasterMac not a hidden attribute */

		snprintf(pos, sizeof(tmpstr) - addlen, "MiiMasterMac=%02x:%02x:%02x:%02x:%02x:%02x\n", 
				pAd->master->dev_addr[0],
				pAd->master->dev_addr[1],
				pAd->master->dev_addr[2],
				pAd->master->dev_addr[3],
				pAd->master->dev_addr[4],
				pAd->master->dev_addr[5]);
		addlen += strlen(pos);
		pos    += strlen(pos);
	}

#endif



	// BUILT_IN_BRIDGE

	snprintf(pos, sizeof(tmpstr) - addlen, "__SkipIfNotINIC__BUILT_IN_BRIDGE=%d\n", pAd->RaCfgObj.bBridge);
	addlen += strlen(pos);
	pos    += strlen(pos);

	// check sum offload
	if (pAd->RaCfgObj.bChkSumOff)
	{

		snprintf(pos, sizeof(tmpstr) - addlen, "__SkipIfNotINIC__CSUM_OFFLOAD=1\n");
		addlen += strlen(pos);
		pos    += strlen(pos);
	}

#if  ((CONFIG_CHIP_NAME==3883) || (CONFIG_CHIP_NAME==3662))
	{

#ifdef CONFIG_CONCURRENT_INIC_SUPPORT
		snprintf(pos, sizeof(tmpstr) - addlen, "__SkipIfNotINIC__CONCURRENT_ENABLE=1\n");
#else
		snprintf(pos, sizeof(tmpstr) - addlen, "__SkipIfNotINIC__CONCURRENT_ENABLE=0\n");
#endif // CONFIG_CONCURRENT_INIC_SUPPORT //
		addlen += strlen(pos);
		pos    += strlen(pos);
	}
#endif 

#ifdef NEW_MBSS_SUPPORT	
	{
		
		snprintf(pos, sizeof(tmpstr) - addlen, "__SkipIfNotINIC__NEWMBSS=1\n");
		addlen += strlen(pos);
		pos    += strlen(pos);
	}
#endif 


#ifdef __BIG_ENDIAN

	snprintf(pos, sizeof(tmpstr) - addlen, "__SkipIfNotINIC__BIG_ENDIAN=1\n");
	addlen += strlen(pos);
	pos    += strlen(pos);
#endif

#if defined(RLK_INIC_SOFTWARE_AGGREATION) || defined(RLK_INIC_TX_AGGREATION_ONLY)

    snprintf(pos, sizeof(tmpstr) - addlen, "__SkipIfNotINIC__SOFTWARE_AGGR=1\n");
    addlen += strlen(pos);
    pos    += strlen(pos);
#endif

	if (pAd->RaCfgObj.bExtEEPROM)
	{
		if (!pAd->RaCfgObj.bGetExtEEPROMSize)
		{
			pAd->RaCfgObj.bGetExtEEPROMSize = 1;    

#ifdef CONFIG_CONCURRENT_INIC_SUPPORT			
			pAd->RaCfgObj.ext_eeprom.fp = ConcurrentObj.ExtEeprom[0].fp;
#endif // CONFIG_CONCURRENT_INIC_SUPPORT //

			fpE2p = pAd->RaCfgObj.ext_eeprom.fp;
			if (!fpE2p)
			{
				pAd->RaCfgObj.ExtEEPROMSize = 0;
				printk("upload_profile Error : Can't read External EEPROM File\n");
			}
			else
			{
				fpE2pStart = fpE2p->f_op->llseek(fpE2p, 0, SEEK_SET);
				fpE2pEnd = fpE2p->f_op->llseek(fpE2p, 0, SEEK_END);         
				if (fpE2pEnd - fpE2pStart > MAX_EXT_EEPROM_SIZE)
				{
					pAd->RaCfgObj.ExtEEPROMSize = 0;
					printk("upload_profile Error : External EEPROM File size(%d) > MAX_EXT_EEPROM_SIZE(%d)\n", (fpE2pEnd - fpE2pStart), MAX_EXT_EEPROM_SIZE);
				}
				else
				{
					pAd->RaCfgObj.ExtEEPROMSize = fpE2pEnd - fpE2pStart;
					fpE2pStart = fpE2p->f_op->llseek(fpE2p, 0, SEEK_SET);
				}
			}
		}

		snprintf(pos, sizeof(tmpstr) - addlen, "__SkipIfNotINIC__EXT_EEPROM_SIZE=%d\n", pAd->RaCfgObj.ExtEEPROMSize);
		addlen += strlen(pos);
		pos    += strlen(pos);
	}

#ifndef CONFIG_CONCURRENT_INIC_SUPPORT
	// Add NULL string terminator
	*pos++ = 0x00;
	addlen++;
#endif // CONFIG_CONCURRENT_INIC_SUPPORT //

	if (pAd->RaCfgObj.extraProfileOffset < addlen)
	{
		pos = &tmpstr[pAd->RaCfgObj.extraProfileOffset];
		addlen -= pAd->RaCfgObj.extraProfileOffset;

		if (space > addlen)
		{
			memcpy((u8 *)(pAd->RaCfgObj.test_pool+len), pos, addlen);
			pAd->RaCfgObj.extraProfileOffset += addlen;
			ret = addlen;
		}
		else
		{
			memcpy((u8 *)(pAd->RaCfgObj.test_pool+len), pos, space);
			pAd->RaCfgObj.extraProfileOffset += space;
			ret = space;
		}
	}
	return ret;
}


#ifdef CONFIG_CONCURRENT_INIC_SUPPORT

/* Append NULL string terminator */
int _append_extra_profile2(iNIC_PRIVATE *pAd, int len)
{
    char tmpstr[256], *pos = tmpstr;
	int  addlen = 0;
	int  ret    = 0;
	int  space  = MAX_FEEDBACK_LEN - len;
	int fpE2pStart, fpE2pEnd;
	struct file *fpE2p = NULL;

	if (space <= 0)
		return 0;

	snprintf(pos, sizeof(tmpstr) - addlen, "\n");
	addlen += strlen(pos);
	pos    += strlen(pos);

	if (strlen(ConcurrentObj.Mac[1]))
	{

		snprintf(pos, sizeof(tmpstr) - addlen, "__SkipIfNotINIC__MAC=%s\n", ConcurrentObj.Mac[1]);
		addlen += strlen(pos);
		pos    += strlen(pos);

	}
	else
	if (strlen(mac2))
	{

		snprintf(pos, sizeof(tmpstr) - addlen, "__SkipIfNotINIC__MAC=%s\n", mac2);
		addlen += strlen(pos);
		pos    += strlen(pos);

	}
#if (CONFIG_INF_TYPE==INIC_INF_TYPE_MII)
	else if (pAd->master && syncmiimac)
	{
		snprintf(pos, sizeof(tmpstr) - addlen, "__SkipIfNotINIC__MAC=%02x:%02x:%02x:%02x:%02x:%02x\n", 
				pAd->master->dev_addr[0],
				pAd->master->dev_addr[1],
				pAd->master->dev_addr[2],
				pAd->master->dev_addr[3],
				pAd->master->dev_addr[4],
				pAd->master->dev_addr[5]);
		addlen += strlen(pos);
		pos    += strlen(pos);
	}	
#endif


	pAd = gAdapter[1];
	fpE2p = ConcurrentObj.ExtEeprom[1].fp;

	if (pAd->RaCfgObj.bExtEEPROM)
	{
		if (!pAd->RaCfgObj.bGetExtEEPROMSize)
		{
			pAd->RaCfgObj.bGetExtEEPROMSize = 1;   

			if (!fpE2p)
			{
				pAd->RaCfgObj.ExtEEPROMSize = 0;
				printk("upload_profile Error : Can't read External EEPROM File\n");
			}
			else
			{
				fpE2pStart = fpE2p->f_op->llseek(fpE2p, 0, SEEK_SET);
				fpE2pEnd = fpE2p->f_op->llseek(fpE2p, 0, SEEK_END);         
				if (fpE2pEnd - fpE2pStart > MAX_EXT_EEPROM_SIZE)
				{
					pAd->RaCfgObj.ExtEEPROMSize = 0;
					printk("upload_profile Error : External EEPROM File size(%d) > MAX_EXT_EEPROM_SIZE(%d)\n", (fpE2pEnd - fpE2pStart), MAX_EXT_EEPROM_SIZE);
				}
				else
				{
					pAd->RaCfgObj.ExtEEPROMSize = fpE2pEnd - fpE2pStart;
					fpE2pStart = fpE2p->f_op->llseek(fpE2p, 0, SEEK_SET);
				}
			}
		}

		snprintf(pos, sizeof(tmpstr) - addlen, "__SkipIfNotINIC__EXT_EEPROM_SIZE=%d\n", pAd->RaCfgObj.ExtEEPROMSize);
		addlen += strlen(pos);
		pos    += strlen(pos);
	}

	// Add NULL string terminator
	*pos++ = 0x00;
	addlen++;

	if (ConcurrentObj.extraProfileOffset2 < addlen)
	{
		pos = &tmpstr[ConcurrentObj.extraProfileOffset2];
		addlen -= ConcurrentObj.extraProfileOffset2;

		if (space > addlen)
		{
			memcpy((u8 *)(gAdapter[0]->RaCfgObj.test_pool+len), pos, addlen);
			ConcurrentObj.extraProfileOffset2 += addlen;
			ret = addlen;
		}
		else
		{
			memcpy((u8 *)(gAdapter[0]->RaCfgObj.test_pool+len), pos, space);
			ConcurrentObj.extraProfileOffset2 += space;
			ret = space;
		}
	}
	
	return ret;
}
#endif // CONFIG_CONCURRENT_INIC_SUPPORT //

static void _upload_profile(iNIC_PRIVATE *pAd)
{
	int len = 0,extra = 0, e2pLen = 0;
	struct file *fp = pAd->RaCfgObj.profile.fp;
	struct file *fpE2p = pAd->RaCfgObj.ext_eeprom.fp;
#ifdef CONFIG_CONCURRENT_INIC_SUPPORT
	int len2 = 0;	
	struct file *fp2 = NULL;
	static unsigned char bStartSecondProfile = FALSE;
	int i;

	pAd = gAdapter[0];
	fp = ConcurrentObj.Profile[0].fp;
	fp2 = ConcurrentObj.Profile[1].fp;	
#endif // CONFIG_CONCURRENT_INIC_SUPPORT //
	

	if (!fp) return;

	memset(pAd->RaCfgObj.test_pool, 0, MAX_FEEDBACK_LEN);
	//len = read(cfg_fd, RaCfgObj.test_pool, MAX_FEEDBACK_LEN);
	len = fp->f_op->read(fp, pAd->RaCfgObj.test_pool, MAX_FEEDBACK_LEN, &fp->f_pos);
	extra = _append_extra_profile(pAd, len);
	len += extra;


#ifndef CONFIG_CONCURRENT_INIC_SUPPORT
	if (pAd->RaCfgObj.bExtEEPROM && len < MAX_FEEDBACK_LEN)
	{
		if (!fpE2p)
		{
			printk("upload_profile Error : Can't read External EEPROM File\n");
			return;
		}
		e2pLen = fpE2p->f_op->read(fpE2p, pAd->RaCfgObj.test_pool + len, MAX_FEEDBACK_LEN - len, &fpE2p->f_pos);        
		len += e2pLen;  
	}
#endif // CONFIG_CONCURRENT_INIC_SUPPORT // 	

#ifdef CONFIG_CONCURRENT_INIC_SUPPORT

	/* upload the profile of second concurrent card */
	if(len < MAX_FEEDBACK_LEN)
	{
		if(bStartSecondProfile == FALSE)
		{
			bStartSecondProfile = TRUE;
			pAd->RaCfgObj.test_pool[len] ='\n';
			len++;
		}				
		len2 = fp2->f_op->read(fp2, pAd->RaCfgObj.test_pool + len, MAX_FEEDBACK_LEN - len, &fp2->f_pos);
	}
	len += len2;

	/* upload attributes of the 2nd card and null string terminator */
	extra = _append_extra_profile2(gAdapter[1], len);
	len += extra;

	/* upload both external EEPROM data */
	for(i = 0; i < CONCURRENT_CARD_NUM; i++)
	{
		fpE2p = ConcurrentObj.ExtEeprom[i].fp;
		
		if (gAdapter[i]->RaCfgObj.bExtEEPROM && len < MAX_FEEDBACK_LEN)
	{
		if (!fpE2p)
		{
			printk("upload_profile Error : Can't read External EEPROM File\n");
			return;
		}
		e2pLen = fpE2p->f_op->read(fpE2p, pAd->RaCfgObj.test_pool + len, MAX_FEEDBACK_LEN - len, &fpE2p->f_pos);        
		len += e2pLen;  
	}
	}	
#endif // CONFIG_CONCURRENT_INIC_SUPPORT //

//	printk("Upload Profile %d bytes from file, %d from extra...\n", len, extra);

	if (len > 0)
	{
#ifdef RETRY_PKT_SEND
		memcpy(pAd->RaCfgObj.RPKTInfo.buffer, pAd->RaCfgObj.test_pool, len);

#ifdef CONFIG_CONCURRENT_INIC_SUPPORT
		pAd->RaCfgObj.RPKTInfo.Seq = ConcurrentObj.Profile[0].seq;
#else
		pAd->RaCfgObj.RPKTInfo.Seq = pAd->RaCfgObj.profile.seq;
#endif // CONFIG_CONCURRENT_INIC_SUPPORT //

		pAd->RaCfgObj.RPKTInfo.BootType = RACFG_CMD_BOOT_INITCFG;
		pAd->RaCfgObj.RPKTInfo.length = len;
		pAd->RaCfgObj.RPKTInfo.retry = FwRetryCnt;
		// turn-on timer
		if (!pAd->RaCfgObj.uploadTimer.function)
		{
			init_timer(&pAd->RaCfgObj.uploadTimer);
			pAd->RaCfgObj.uploadTimer.function = upload_timeout;
			pAd->RaCfgObj.uploadTimer.data = (uintptr_t)pAd;
		}
		if(pAd->RaCfgObj.RPKTInfo.Seq == 0)
			mod_timer(&pAd->RaCfgObj.uploadTimer, jiffies + HZ);
		else
			mod_timer(&pAd->RaCfgObj.uploadTimer, jiffies + RetryTimeOut*HZ/1000);
#endif

#ifdef CONFIG_CONCURRENT_INIC_SUPPORT
		SendRaCfgCommand(pAd, 
						 RACFG_CMD_TYPE_BOOTSTRAP & RACFG_CMD_TYPE_PASSIVE_MASK,
						 RACFG_CMD_BOOT_INITCFG, len, ConcurrentObj.Profile[0].seq, 
						 0, 0, 0, pAd->RaCfgObj.test_pool);
		ConcurrentObj.Profile[0].seq++;  
#else 
		SendRaCfgCommand(pAd, 
						 RACFG_CMD_TYPE_BOOTSTRAP & RACFG_CMD_TYPE_PASSIVE_MASK,
						 RACFG_CMD_BOOT_INITCFG, len, pAd->RaCfgObj.profile.seq, 
						 0, 0, 0, pAd->RaCfgObj.test_pool);
		pAd->RaCfgObj.profile.seq++;  
#endif // CONFIG_CONCURRENT_INIC_SUPPORT
	}
	else
	{
#ifdef CONFIG_CONCURRENT_INIC_SUPPORT
		DBGPRINT("Send Init Cfg Data Done(%d packets)\n", ConcurrentObj.Profile[0].seq);
		for(i = 0; i < CONCURRENT_CARD_NUM; i++)
		{
			RaCfgCloseFile(pAd, &ConcurrentObj.Profile[i]);

			if (gAdapter[i]->RaCfgObj.bExtEEPROM)
			{
				RaCfgCloseFile(gAdapter[i], &ConcurrentObj.ExtEeprom[i]);
			}		
		}
#else
		DBGPRINT("Send Init Cfg Data Done(%d packets)\n", pAd->RaCfgObj.profile.seq);
		RaCfgCloseFile(pAd, &pAd->RaCfgObj.profile);
		if (pAd->RaCfgObj.bExtEEPROM)
			RaCfgCloseFile(pAd, &pAd->RaCfgObj.ext_eeprom);
#endif // CONFIG_CONCURRENT_INIC_SUPPORT //

		_upload_firmware(pAd);
	} 

}

#ifdef CONFIG_CONCURRENT_INIC_SUPPORT

static void RaCfgConcurrentOpenAction(uintptr_t arg)
{

	iNIC_PRIVATE *pAd = (iNIC_PRIVATE *)arg;
	int i;

	pAd = gAdapter[0];

	if (!pAd->RaCfgObj.opmode == 0)
	{
		RLK_STRCPY(pAd->RaCfgObj.firmware.name, INIC_AP_FIRMWARE_PATH);
	}
	else
	{
		RLK_STRCPY(pAd->RaCfgObj.firmware.name, INIC_STA_FIRMWARE_PATH);
	}

	for(i = 0; i < CONCURRENT_CARD_NUM; i++)
	{
		RLK_STRCPY(ConcurrentObj.Profile[i].name, ConcurrentObj.Profile[i].read_name);	
		RLK_STRCPY(ConcurrentObj.ExtEeprom[i].name, ConcurrentObj.ExtEeprom[i].read_name);		
	}

#ifdef DBG
	{		
		char buf[MAX_FILE_NAME_SIZE]; 
		
		sprintf(buf, "%s%s", root, pAd->RaCfgObj.firmware.name);
		RLK_STRCPY(pAd->RaCfgObj.firmware.name, buf);

		for(i = 0; i < CONCURRENT_CARD_NUM; i++)
		{
			snprintf(buf, sizeof(buf), "%s%s", root, ConcurrentObj.Profile[i].name);
			RLK_STRCPY(ConcurrentObj.Profile[i].name, buf);

			if (gAdapter[i]->RaCfgObj.bExtEEPROM)
			{
				snprintf(buf, sizeof(buf), "%s%s", root, ConcurrentObj.ExtEeprom[i].name);
				RLK_STRCPY(ConcurrentObj.ExtEeprom[i].name, buf);
			}
		}
	}

#endif

	// Reset files to avoid duplicated profile upload 
	// when multiple BOOT_NOTIFY coming too fast.
	RaCfgCloseFile(pAd, &pAd->RaCfgObj.firmware);
	RaCfgOpenFile(pAd, &pAd->RaCfgObj.firmware, O_RDONLY);
	pAd->RaCfgObj.extraProfileOffset = 0;

	if (IS_ERR(pAd->RaCfgObj.firmware.fp))
	{
		DBGPRINT("--> Error %ld opening %s\n", -PTR_ERR(pAd->RaCfgObj.firmware.fp),pAd->RaCfgObj.firmware.name);
		return;
	}  

	for(i = 0; i < CONCURRENT_CARD_NUM; i++)
	{
		RaCfgCloseFile(pAd, &ConcurrentObj.Profile[i]);
		RaCfgOpenFile(pAd, &ConcurrentObj.Profile[i],  O_RDONLY);

		if (IS_ERR(ConcurrentObj.Profile[i].fp))
		{
			DBGPRINT("--> Error %ld opening %s\n", -PTR_ERR(ConcurrentObj.Profile[i].fp),ConcurrentObj.Profile[i].name);
			return;
		}

		if (gAdapter[i]->RaCfgObj.bExtEEPROM)
		{
			RaCfgCloseFile(gAdapter[i], &ConcurrentObj.ExtEeprom[i]);
			RaCfgOpenFile(gAdapter[i], &ConcurrentObj.ExtEeprom[i],  O_RDONLY);
			if (IS_ERR(ConcurrentObj.ExtEeprom[i].fp))
			{
				DBGPRINT("--> Error %ld opening %s\n", -PTR_ERR(ConcurrentObj.ExtEeprom[i].fp),ConcurrentObj.ExtEeprom[i].name);
				return;
			}
			gAdapter[i]->RaCfgObj.bGetExtEEPROMSize = 0;        
		}		
	}
	ConcurrentObj.extraProfileOffset2 = 0;
	
	_upload_profile(pAd);
}

#else // CONFIG_CONCURRENT_INIC_SUPPORT //

static void RaCfgOpenAction(uintptr_t arg)
{

	iNIC_PRIVATE *pAd = (iNIC_PRIVATE *)arg;

#if (CONFIG_INF_TYPE == INIC_INF_TYPE_MII)
#ifdef PHASE_LOAD_CODE
	if (!pAd->RaCfgObj.bLoadPhase)
	{
#ifdef RETRY_PKT_SEND
		pAd->RaCfgObj.RPKTInfo.Seq = pAd->RaCfgObj.profile.seq;
		pAd->RaCfgObj.RPKTInfo.BootType = RACFG_CMD_BOOT_INITCFG;
		pAd->RaCfgObj.RPKTInfo.length = 0;
		pAd->RaCfgObj.RPKTInfo.retry = FwRetryCnt;
		// turn-on timer
		if (!pAd->RaCfgObj.uploadTimer.function)
		{
			init_timer(&pAd->RaCfgObj.uploadTimer);
			pAd->RaCfgObj.uploadTimer.function = upload_timeout;
			pAd->RaCfgObj.uploadTimer.data = (uintptr_t)pAd;
		}
		if(pAd->RaCfgObj.RPKTInfo.Seq == 0)
			mod_timer(&pAd->RaCfgObj.uploadTimer, jiffies + HZ);
		else
			mod_timer(&pAd->RaCfgObj.uploadTimer, jiffies + RetryTimeOut*HZ/1000);
#endif
		RLK_STRCPY(pAd->RaCfgObj.firmware.name, INIC_PHASE_FIRMWARE_PATH); 
		RaCfgCloseFile(pAd, &pAd->RaCfgObj.firmware);
		RaCfgOpenFile(pAd, &pAd->RaCfgObj.firmware, O_RDONLY);
		SendRaCfgCommand(pAd, 
						 RACFG_CMD_TYPE_BOOTSTRAP & RACFG_CMD_TYPE_PASSIVE_MASK,
						 RACFG_CMD_BOOT_INITCFG, 0, pAd->RaCfgObj.profile.seq, 
						 0, 0, 0, pAd->RaCfgObj.test_pool);
		pAd->RaCfgObj.profile.seq++;		
		return;
	}
#endif
#endif
	if (!pAd->RaCfgObj.opmode == 0)
	{
		RLK_STRCPY(pAd->RaCfgObj.firmware.name, INIC_AP_FIRMWARE_PATH);
			RLK_STRCPY(pAd->RaCfgObj.profile.name,  INIC_AP_PROFILE_PATH);

	}
	else
	{
		RLK_STRCPY(pAd->RaCfgObj.firmware.name, INIC_STA_FIRMWARE_PATH);
			RLK_STRCPY(pAd->RaCfgObj.profile.name,  INIC_STA_PROFILE_PATH);
	}

	if (pAd->RaCfgObj.bExtEEPROM)
		RLK_STRCPY(pAd->RaCfgObj.ext_eeprom.name,  EEPROM_BIN_FILE_PATH);


#ifdef MULTIPLE_CARD_SUPPORT
	if (pAd->RaCfgObj.InterfaceNumber >= 0)
		RLK_STRCPY(pAd->RaCfgObj.profile.name,  pAd->RaCfgObj.profile.read_name);			
#endif // MULTIPLE_CARD_SUPPORT //		


#ifdef DBG
	{
		char buf[MAX_FILE_NAME_SIZE]; 

		snprintf(buf, MAX_FILE_NAME_SIZE, "%s%s", root, pAd->RaCfgObj.firmware.name);

		strncpy(pAd->RaCfgObj.firmware.name, buf, MAX_FILE_NAME_SIZE - 1);
		pAd->RaCfgObj.firmware.name[MAX_FILE_NAME_SIZE -1] = '0';

		snprintf(buf, MAX_FILE_NAME_SIZE, "%s%s", root, pAd->RaCfgObj.profile.name);
		strncpy(pAd->RaCfgObj.profile.name, buf, MAX_FILE_NAME_SIZE - 1);
		pAd->RaCfgObj.profile.name[MAX_FILE_NAME_SIZE - 1] = '0';
			
		if (pAd->RaCfgObj.bExtEEPROM)
		{
			snprintf(buf, MAX_FILE_NAME_SIZE, "%s%s", root, pAd->RaCfgObj.ext_eeprom.name);
			strncpy(pAd->RaCfgObj.ext_eeprom.name, buf, MAX_FILE_NAME_SIZE - 1);
			pAd->RaCfgObj.ext_eeprom.name[MAX_FILE_NAME_SIZE -1] = '0';
		}
	}

#endif
	// Reset files to avoid duplicated profile upload 
	// when multiple BOOT_NOTIFY coming too fast.
	RaCfgCloseFile(pAd, &pAd->RaCfgObj.firmware);
	RaCfgCloseFile(pAd, &pAd->RaCfgObj.profile);
	if (pAd->RaCfgObj.bExtEEPROM)
		RaCfgCloseFile(pAd, &pAd->RaCfgObj.ext_eeprom);

	RaCfgOpenFile(pAd, &pAd->RaCfgObj.firmware, O_RDONLY);
	RaCfgOpenFile(pAd, &pAd->RaCfgObj.profile,  O_RDONLY);
	if (pAd->RaCfgObj.bExtEEPROM)
	{
		RaCfgOpenFile(pAd, &pAd->RaCfgObj.ext_eeprom,  O_RDONLY);
		pAd->RaCfgObj.bGetExtEEPROMSize = 0;        
	}

	pAd->RaCfgObj.extraProfileOffset = 0;
	_upload_profile(pAd);
}

#endif // CONFIG_CONCURRENT_INIC_SUPPORT //

static void RaCfgUploadAction(uintptr_t arg) 
{
	ArgBox *box         = (ArgBox *)arg;
	iNIC_PRIVATE *pAd   = (iNIC_PRIVATE *)box->arg1;
	struct sk_buff *skb = (struct sk_buff *)box->arg2;
	struct racfghdr *p_racfgh =  (struct racfghdr *) skb->data;
	u16 length          = le16_to_cpu(p_racfgh->length);
	s16 sequence        = (s16)le16_to_cpu(p_racfgh->sequence);

	FreeArgBox(box);

	if (sequence != pAd->RaCfgObj.firmware.seq-1)
	{
#if 0
		printk("WARNING! iNIC firmware ack sequence %d<->%d mismatched," 
			   "duplicated packet? ignore...\n", 
			   sequence, pAd->RaCfgObj.firmware.seq-1);
#endif
		return;
	}

	if (memcmp(pAd->RaCfgObj.cmp_buf, p_racfgh->data, length) != 0)
	{
		DBGPRINT("UPLOAD(%d) ERROR! data check fail\n", sequence);
		//hex_dump("local:", pAd->RaCfgObj.cmp_buf, len);
		//hex_dump("remote:", data, len);
		kfree_skb(skb);
		RaCfgRestartiNIC((uintptr_t) pAd);
		return;
	}
	kfree_skb(skb);

#ifdef TEST_BOOT_RECOVER
	{
		if (!pAd->RaCfgObj.firm_simulated && sequence == 0)
		{
			DBGPRINT("Simulate UPLOAD  Error...\n");
			RaCfgRestartiNIC((uintptr_t) pAd);
			pAd->RaCfgObj.firm_simulated = TRUE;
			return;
		}
	}
#endif
	_upload_firmware(pAd);

}
static void RaCfgInitCfgAction(uintptr_t arg) 
{
	ArgBox *box         = (ArgBox *)arg;
	iNIC_PRIVATE *pAd   = (iNIC_PRIVATE *)box->arg1;
	struct sk_buff *skb = (struct sk_buff *)box->arg2;
	struct racfghdr *p_racfgh =  (struct racfghdr *) skb->data;
	u16 length          = le16_to_cpu(p_racfgh->length);
	s16 sequence        = (s16) le16_to_cpu(p_racfgh->sequence);

	FreeArgBox(box);

#ifdef CONFIG_CONCURRENT_INIC_SUPPORT
	if (sequence != ConcurrentObj.Profile[0].seq-1)
#else 
	if (sequence != pAd->RaCfgObj.profile.seq-1)
#endif // CONFIG_CONCURRENT_INIC_SUPPORT
	{
#if 0
		printk("WARNING! iNIC profile ack sequence %d<->%d mismatched," 
			   "duplicated packet? ignore...\n", 
			   sequence, pAd->RaCfgObj.profile.seq-1);
#endif
		return;
	}

	if (memcmp(pAd->RaCfgObj.test_pool, p_racfgh->data, length) != 0)
	{
		DBGPRINT("INITCFG(%d) ERROR! data check fail\n", sequence);
		//hex_dump("local:", pAd->RaCfgObj.test_pool, len);
		//hex_dump("remote:", data, len);
		kfree_skb(skb);
		RaCfgRestartiNIC((uintptr_t) pAd);
		return;
	}

	kfree_skb(skb);
#ifdef TEST_BOOT_RECOVER
	{
		if (!pAd->RaCfgObj.cfg_simulated && sequence == 0)
		{
			DBGPRINT("Simulate INITCFG Error...\n");
			RaCfgRestartiNIC((uintptr_t) pAd);
			pAd->RaCfgObj.cfg_simulated = TRUE;
			return;
		}
	}
#endif
#if (CONFIG_INF_TYPE == INIC_INF_TYPE_MII)
#ifdef PHASE_LOAD_CODE
		if (!pAd->RaCfgObj.bLoadPhase)
		{
#ifdef RETRY_PKT_SEND
			// delete timer
			if (pAd->RaCfgObj.uploadTimer.function)
			{
				del_timer_sync(&pAd->RaCfgObj.uploadTimer);
				pAd->RaCfgObj.uploadTimer.function = NULL;
				pAd->RaCfgObj.uploadTimer.data = 0;
			}
#endif
			_upload_firmware(pAd);
		}
		else
#endif
#endif
	_upload_profile(pAd);
}
#if (CONFIG_INF_TYPE==INIC_INF_TYPE_MII)
static void rlk_inic_check_mac(iNIC_PRIVATE *pAd)
{
	struct net_device *dev = pAd->RaCfgObj.MainDev;
	if (!pAd->master)
		return;

	if (!memcmp(dev->dev_addr, pAd->master->dev_addr, 6))
	{
		printk("Sync Mac with MII master done\n");
		return;
	}

	// not equal, warning....
	printk("Warning! MAC addr not equal to MII master => ");
	printk("set MiiMasterMac=%02x:%02x:%02x:%02x:%02x:%02x\n",
		   pAd->master->dev_addr[0],
		   pAd->master->dev_addr[1],
		   pAd->master->dev_addr[2],
		   pAd->master->dev_addr[3],
		   pAd->master->dev_addr[4],
		   pAd->master->dev_addr[5]);

	if (!strlen(mac))
	{
		printk("[syncmiimac] disabled?\n");
	}
	else
	{
		printk("You've assigned a different MAC.\n");
	}
}
#endif
int RaCfgWaitSyncRsp(iNIC_PRIVATE *pAd, u16 cmd, u16 cmd_seq, struct iwreq *wrq, char *kernel_data, int secs)
{
	u32 timeout =  secs * HZ; // 2 or 3 secs
	struct racfghdr *p_racfgh;
	u8 *payload, *user=NULL;
	int status = NDIS_STATUS_SUCCESS;
	unsigned short accumulate = 0;
#ifdef FIX_POTENTIAL_BUG
	char *accumulate_buffer = NULL;
#endif

	if (wrq)
	{
		user = (char *)wrq->u.data.pointer;
		wrq->u.data.length = 0;
	}

	while (1)
	{
		u16 command_type, command_id, command_seq, len, dev_type, seq;
		struct sk_buff *skb;

		if (!pAd->RaCfgObj.wait_completed)
		{
#ifdef CONFIG_PREEMPT
			int atomic_wait =0;
			DEFINE_WAIT(__wait);
			while (in_atomic())
			{
				atomic_wait++;
				prepare_to_wait(&pAd->RaCfgObj.waitQH, &__wait, TASK_INTERRUPTIBLE); 
				if (pAd->RaCfgObj.wait_completed)
				{
					printk("wait atomic, count=%d\n", atomic_wait);
					break;
				}

				//if (signal_pending(current))
				//    break;

			}
			finish_wait(&pAd->RaCfgObj.waitQH, &__wait);

			if (!atomic_wait)
			{
				wait_event_interruptible_timeout(pAd->RaCfgObj.waitQH, pAd->RaCfgObj.wait_completed, timeout);
			}
#else
			wait_event_interruptible_timeout(pAd->RaCfgObj.waitQH, pAd->RaCfgObj.wait_completed, timeout);
#endif
		}
		RTMP_SEM_LOCK(&pAd->RaCfgObj.waitLock);
		if (RaCfgQueueEmpty(&pAd->RaCfgObj.waitQueue))
		{
			pAd->RaCfgObj.wait_completed = 0;
			RTMP_SEM_UNLOCK(&pAd->RaCfgObj.waitLock);
			printk("timeout(%d secs): cmd=%04x no response\n", secs, cmd);
			status = -EFAULT;
			return status;
		}
		skb = (struct sk_buff *)RaCfgDequeue(pAd, &pAd->RaCfgObj.waitQueue, NULL); 
		pAd->RaCfgObj.wait_completed--;
		RTMP_SEM_UNLOCK(&pAd->RaCfgObj.waitLock);

		if (!skb)
		{
			printk("Error, no skb!\n");
			continue;
		}
		p_racfgh =  (struct racfghdr *)skb->data;
		command_type = le16_to_cpu(p_racfgh->command_type);
		command_id   = le16_to_cpu(p_racfgh->command_id);
		command_seq  = le16_to_cpu(p_racfgh->command_seq);
		len          = le16_to_cpu(p_racfgh->length);
		dev_type     = le16_to_cpu(p_racfgh->dev_type);

		seq          = le16_to_cpu(p_racfgh->sequence);

#ifdef CONFIG_CONCURRENT_INIC_SUPPORT
		dev_type &= ~DEV_TYPE_CONCURRENT_FLAG;
#endif // CONFIG_CONCURRENT_INIC_SUPPORT //


		payload = (char *)p_racfgh->data;

		if (command_id != cmd)
		{
			printk("Warning: ignore response(%d bytes), mismatch command"
				   "(%x<->%x)\n", len, command_id, cmd);
			kfree_skb(skb);
			continue;
		}
		if (command_seq != cmd_seq)
		{
			printk("Warning: ignore response(%d bytes), mismatch sequence"
				   "(%x<->%x)\n", len, command_seq, cmd_seq);
			kfree_skb(skb);
			continue;
		}

		switch (command_type)
		{
		case RACFG_CMD_TYPE_IOCTL_STATUS | RACFG_CMD_TYPE_RSP_FLAG:
		case RACFG_CMD_TYPE_SYNC         | RACFG_CMD_TYPE_RSP_FLAG:
			switch (command_id)
			{
			case RACFG_CMD_GET_MAC:
				if (len == 18)
				{
							rlk_inic_set_mac(pAd, payload);
#if (CONFIG_INF_TYPE==INIC_INF_TYPE_MII)
					rlk_inic_check_mac(pAd);
#endif
				}
				else if (len == 35)
				{
					/* 
					 * The following is an example of the format of concurrent card mac
					 * MAC(card 0):00:11:22:33:44:55
					 * MAC(card 1):00:11:22:33:44:66
					 * The return MAC string of iNIC is MAC(card0)MAC(card1)
					 * -->00:11:22:33:44:5500:11:22:33:44:66
					 */
					u8 mac1_tmp[32], mac2_tmp[32];	
					printk("Get concurrent card MAC address:%s\n", payload);
					strncpy(mac1_tmp, payload, 17);
					mac1_tmp[17] = '\0';
					strncpy(mac2_tmp, payload + 17, 17);
					mac2_tmp[17] = '\0';					
#ifdef CONFIG_CONCURRENT_INIC_SUPPORT 			
					rlk_inic_set_mac(gAdapter[0], mac1_tmp); 
					// Set the concurrent card mac address
					if(gAdapter[1])
						rlk_inic_set_mac(gAdapter[1], mac2_tmp);
					else
					{
						printk("Warning: gAdapter[1] is NULL. Can't sync the concurrent card mac address \n");
					}
#else // CONFIG_CONCURRENT_INIC_SUPPORT //
					rlk_inic_set_mac(pAd, mac1_tmp); 					
#endif // CONFIG_CONCURRENT_INIC_SUPPORT //			

#if (CONFIG_INF_TYPE==INIC_INF_TYPE_MII)
					rlk_inic_check_mac(pAd);
#endif
				}
				else if (len != 0)
				{
					printk("Warning: Skip mac addr with wrong size: %d\n", len);
				}
				break;
			}
			status = le32_to_cpu(p_racfgh->status);
			switch (status)
			{
			case -1:
				status = -EFAULT;
				break;
			case -2:
				status = -EOPNOTSUPP;
				break;
			case -3:
				status = -EINVAL;
				break;
			case -4:
				status = -ENETDOWN;
				break;
			case -5:
				status = -EAGAIN;
				break;                  
			}
#ifdef FIX_POTENTIAL_BUG
			if (accumulate_buffer)
			{
				kfree(accumulate_buffer);
				accumulate_buffer = NULL;
			}
#endif
			kfree_skb(skb);
			return status;
		case RACFG_CMD_TYPE_COPY_TO_USER | RACFG_CMD_TYPE_RSP_FLAG:
			ASSERT(wrq);

			if(!wrq)
			{
				status = -EINVAL;
				break;
			}			
			if (len > 0)
			{
				ASSERT(user);
				//printk("Received copy_to_user %d bytes\n", len);

#ifdef INBAND_DEBUG
				hex_dump("COPY_TO_USER recevied:", skb->data, skb->len);
#endif
				if (command_id == SIOCGIWRANGE)
				{
					//Only 1 packet is returned (1024 is enough)

					struct iw_range *range = (struct iw_range *)payload;
					
					range->we_version_compiled = 19;
					range->we_version_source   = 19;
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,24)
					range->scan_capa = IW_SCAN_CAPA_ESSID | IW_SCAN_CAPA_CHANNEL;
#endif
				}

				if (copy_to_user(user, payload, len))
				{
					status = -EFAULT;
					break;
				}
				else
				{
					user += len; 
					wrq->u.data.length += len;
				}
			}
			break;
		case RACFG_CMD_TYPE_IWREQ_STRUC | RACFG_CMD_TYPE_RSP_FLAG:
			ASSERT(wrq);
			if(!wrq)
			{
				status = -EINVAL;
				break;
			}
			
			if (len > MAX_FEEDBACK_LEN)
			{
				printk("ERROR! invalid iwreq len received:%d\n", len);
			}
			else
			{
#ifdef INBAND_DEBUG
				hex_dump("IWREQ recevied:", skb->data, skb->len);
#endif
				IWREQdecode(wrq, payload, dev_type, NULL);
			}
			break;
		case RACFG_CMD_TYPE_IW_HANDLER | RACFG_CMD_TYPE_RSP_FLAG:
#ifdef INBAND_DEBUG
			hex_dump("IW_HANDLER recevied:", skb->data, skb->len);
#endif
			if (command_id == RACFG_CMD_GET_STATS)
			{
				ASSERT(kernel_data);
				/* Note : wrq == NULL in this case */
				memmove(kernel_data, payload, len);
			}
			else
			{
				if (!kernel_data)
				{
					ASSERT(wrq);
					if(!wrq)
					{
						status = -EINVAL;
						break;
					}					
					ASSERT(len < MAX_FEEDBACK_LEN);
					IWREQdecode(wrq, payload, dev_type, NULL);
				}
				else
				{
#ifdef FIX_POTENTIAL_BUG
					/* payload= wrq + extra1 + extra2 + .... */
					if (!accumulate_buffer)
					{

						int size = *(u16 *)payload+1+sizeof(struct iwreq);
						accumulate_buffer = kmalloc(size, MEM_ALLOC_FLAG);
						if (!accumulate_buffer)
						{
							printk("ERROR! alloc %d bytes fail.\n", size);
							break;
						}

					}
					memmove(accumulate_buffer+accumulate, payload, len);

#else

					memmove(kernel_data + accumulate, payload, len);
#endif

					if (len >= MAX_FEEDBACK_LEN)
					{
						accumulate += len;
					}
					else
					{
						ASSERT(wrq);
						if(!wrq)
						{
							status = -EINVAL;
							break;
						}

#ifdef FIX_POTENTIAL_BUG
						IWREQdecode(wrq, accumulate_buffer, dev_type, kernel_data);
						// the last packet => free accumulate_buffer
						kfree(accumulate_buffer);
						accumulate_buffer = NULL;
#else
						IWREQdecode(wrq, kernel_data, dev_type, kernel_data);
#endif
						//hex_dump("decode afer", kernel_data, 32);
					}
				}
			}
			break;
		}
		kfree_skb(skb);

#if 0 /* for debug only */		
		printk("Receive data , len = %d:\n", len);
		{
			int i;
			for (i = 0; i < len; i++)
				printk("%02x\n", payload[i]);
		}
#endif		   

	}
	return status;
}


void IoctlRspHandler(iNIC_PRIVATE *pAd, struct sk_buff *skb)
{
	int error;
	//printk("Enqueu IOCTL resp, queue size=%d\n", pAd->RaCfgObj.waitQueue.num);

	error = RaCfgEnqueue(pAd, &pAd->RaCfgObj.waitQueue, NULL, NULL, (uintptr_t) skb);

	RTMP_SEM_LOCK(&pAd->RaCfgObj.waitLock);
	if (error)
		kfree_skb(skb);
	else
		pAd->RaCfgObj.wait_completed++;
	RTMP_SEM_UNLOCK(&pAd->RaCfgObj.waitLock);

	wake_up_interruptible(&pAd->RaCfgObj.waitQH);
}

void FeedbackRspHandler(iNIC_PRIVATE *pAd, struct sk_buff *skb)
{
	// Will be dequeued by original IOCTL thread, not RaCfgTaskThread
	//printk("Enqueue Feedback resp, queue size=%d..\n", pAd->RaCfgObj.taskQueue.num);
	int error = RaCfgEnqueue(pAd, &pAd->RaCfgObj.taskQueue, &pAd->RaCfgObj.taskSem, NULL, (uintptr_t) skb);
	if (error)
		kfree_skb(skb);
}

#ifdef PCI_NONE_RESET
static int bDropMultiBootNotify = 0;
#endif

/*
* For code size, on-chip BootCode Don't check any error
* RaCfgAgent just process and ack the command from RaCfgMaster
* In host, RaCfgMaster MUST be responsible for error handle
*/
//static void RaCfgCommandHandler(iNIC_PRIVATE *pAd, u16 command_id, char *data, u16 len, u16 seq)
static void RaCfgCommandHandler(iNIC_PRIVATE *pAd, struct sk_buff *skb)
{
	struct racfghdr *p_racfgh =  (struct racfghdr *) skb->data;
	u16 command_id   = le16_to_cpu(p_racfgh->command_id);
	u16 seq          = le16_to_cpu(p_racfgh->sequence);
	ArgBox *box = NULL;
	int error = 0;

	switch (command_id)
	{
	case RACFG_CMD_BOOT_NOTIFY:
#ifdef PCI_NONE_RESET
		if (!bDropMultiBootNotify)
		{
			DBGPRINT("RACFG_CMD_BOOT_NOTIFY\n");
#ifdef CONFIG_CONCURRENT_INIC_SUPPORT 		
		RaCfgEnqueue(pAd, &pAd->RaCfgObj.taskQueue, &pAd->RaCfgObj.taskSem, RaCfgConcurrentOpenAction, (uintptr_t)pAd);
#else
			RaCfgEnqueue(pAd, &pAd->RaCfgObj.taskQueue, &pAd->RaCfgObj.taskSem, RaCfgOpenAction, (uintptr_t)pAd);
#endif // CONFIG_CONCURRENT_INIC_SUPPORT //

			bDropMultiBootNotify = 1;
		}
#else
#if (CONFIG_INF_TYPE == INIC_INF_TYPE_MII)
#ifdef PHASE_LOAD_CODE
		    if (pAd->RaCfgObj.bLoadPhase)
		    {
			    pAd->RaCfgObj.dropNotifyCount++;
			    if (pAd->RaCfgObj.dropNotifyCount <= MAX_NOTIFY_DROP_NUMBER)
			    {
				    kfree_skb(skb);
				    break;			
			    }	
		    }
            pAd->RaCfgObj.dropNotifyCount = 0;
#endif
#endif
		DBGPRINT("RACFG_CMD_BOOT_NOTIFY\n");
#ifdef CONFIG_CONCURRENT_INIC_SUPPORT 		
		RaCfgEnqueue(pAd, &pAd->RaCfgObj.taskQueue, &pAd->RaCfgObj.taskSem, RaCfgConcurrentOpenAction, (uintptr_t)pAd);
#else
		RaCfgEnqueue(pAd, &pAd->RaCfgObj.taskQueue, &pAd->RaCfgObj.taskSem, RaCfgOpenAction, (uintptr_t)pAd);
#endif // CONFIG_CONCURRENT_INIC_SUPPORT //

#endif
		kfree_skb(skb);
		break;
	case RACFG_CMD_BOOT_INITCFG:
		DBGPRINT("RACFG_CMD_BOOT_INITCFG(%d)\n", seq);
		if (!(box = GetArgBox()))
		{
			printk("ERROR, can't alloc ArgBox\n");
			return;
		}
		box->arg1 = (uintptr_t)pAd;
		box->arg2 = (uintptr_t)skb;
		error = RaCfgEnqueue(pAd, &pAd->RaCfgObj.taskQueue, &pAd->RaCfgObj.taskSem, RaCfgInitCfgAction, (uintptr_t)box);
		if (error)
		{
			FreeArgBox(box);
			kfree_skb(skb);
		}
		break;
	case RACFG_CMD_BOOT_UPLOAD:
		if (seq < 3)
		{
			DBGPRINT("RACFG_CMD_BOOT_UPLOAD(%d)\n", seq);
		}
		if (!(box = GetArgBox()))
		{
			printk("ERROR, can't alloc ArgBox\n");
			return;
		}
		box->arg1 = (uintptr_t)pAd;
		box->arg2 = (uintptr_t)skb;
		error = RaCfgEnqueue(pAd, &pAd->RaCfgObj.taskQueue, &pAd->RaCfgObj.taskSem, RaCfgUploadAction, (uintptr_t)box);
		if (error)
		{
			FreeArgBox(box);
			kfree_skb(skb);
		}

		break;
	case RACFG_CMD_BOOT_STARTUP:
		DBGPRINT("RACFG_CMD_BOOT_STARTUP\n");
#if (CONFIG_INF_TYPE == INIC_INF_TYPE_MII)
#ifdef PHASE_LOAD_CODE
			if (pAd->RaCfgObj.bLoadPhase)
				pAd->RaCfgObj.bLoadPhase = FALSE;
			else
				pAd->RaCfgObj.bLoadPhase = TRUE;
#endif
#endif
		kfree_skb(skb);
		break;
	case RACFG_CMD_BOOT_REGRD:
		kfree_skb(skb);
		break;
	case RACFG_CMD_BOOT_REGWR:
		kfree_skb(skb);
		break;
	}


}


void hex_dump(char *str, unsigned char *pSrcBufVA, unsigned int SrcBufLen)
{
	unsigned char *pt;
	int x;

	pt = pSrcBufVA;
	DBGPRINT("%s: %p, len = %d\n", str, pSrcBufVA, SrcBufLen);
	for (x=0; x<SrcBufLen; x++)
	{
		if (x % 16 == 0)
			DBGPRINT("0x%04x : ", x);
		DBGPRINT("%02x ", ((unsigned char)pt[x]));
		if (x%16 == 15)	DBGPRINT("\n");
	}
	DBGPRINT("\n");
}

/*
void make_in_band_frame(
					   unsigned char *p, 
					   unsigned short CommandType, 
					   unsigned short CommandID, 
					   unsigned short DevType, 
					   unsigned short DevID,
					   unsigned short len, 
					   unsigned short CommandSeq)
{
	struct racfghdr *hdr = (struct racfghdr*)p;

	hdr->magic_no = cpu_to_le32(MAGIC_NUMBER);
	hdr->command_type = cpu_to_le16(CommandType);
	hdr->command_id = cpu_to_le16(CommandID);
	hdr->command_seq  = cpu_to_le16(CommandSeq);
	hdr->length = cpu_to_le16(len);
	hdr->dev_type = cpu_to_le16(DevType); // as iwreq type
	hdr->dev_id = cpu_to_le16(DevID);

	hdr->flags = 0;
#ifdef __BIG_ENDIAN
	hdr->flags |= FLAG_BIG_ENDIAN;
#endif
}
*/


void SendRaCfgCommand(iNIC_PRIVATE *pAd, u16 cmd_type, u16 cmd_id, u16 len, u16 seq, u16 cmd_seq, u16 dev_type, u16 dev_id, u8 *data)
{
	struct racfghdr *p_racfgh;
	p_racfgh = (struct racfghdr *)&pAd->RaCfgObj.packet[ETH_HLEN];

	p_racfgh->magic_no = cpu_to_le32(MAGIC_NUMBER);
	p_racfgh->command_type = cpu_to_le16(cmd_type);
	p_racfgh->command_id = cpu_to_le16(cmd_id);
	p_racfgh->length = cpu_to_le16(len);
	p_racfgh->sequence = cpu_to_le16(seq);

	p_racfgh->command_seq  = cpu_to_le16(cmd_seq);
	p_racfgh->dev_type = cpu_to_le16(dev_type);	// as iwreq type
	p_racfgh->dev_id = cpu_to_le16(dev_id);
	p_racfgh->flags = 0;
#ifdef __BIG_ENDIAN
	p_racfgh->flags |= FLAG_BIG_ENDIAN;
#endif


#ifdef CONFIG_CONCURRENT_INIC_SUPPORT
	if(pAd->RaCfgObj.InterfaceNumber == 1)
		p_racfgh->dev_type = cpu_to_le16(dev_type | DEV_TYPE_CONCURRENT_FLAG);
#endif // CONFIG_CONCURRENT_INIC_SUPPORT //

	if (len)
		memcpy(&p_racfgh->data[0], data, len);

	/* 
	 * don't care device id, device type, and reserved field
	 */
	send_racfg_cmd(pAd, (char *)p_racfgh, sizeof(struct racfghdr) + len);    
}


boolean racfg_frame_handle(iNIC_PRIVATE *pAd, struct sk_buff *skb)
{
	boolean bRacfg = FALSE;
	struct racfghdr *p_racfgh =  (struct racfghdr *) skb->data;
	// 
	// Sanity Check for RaCfg Header
	// 

	if (skb->protocol == ETH_P_RACFG && 
		le32_to_cpu(p_racfgh->magic_no) == MAGIC_NUMBER)
	{
		u16 command_type = le16_to_cpu(p_racfgh->command_type);
		u16 command_id   = le16_to_cpu(p_racfgh->command_id);
#if (CONFIG_INF_TYPE==INIC_INF_TYPE_MII)
		u16 len          = le16_to_cpu(p_racfgh->length);
		u8 *payload      = (char *)p_racfgh->data;
		u16 command_seq  = le16_to_cpu(p_racfgh->command_seq);
		u16 seq          = le16_to_cpu(p_racfgh->sequence);
#endif



#ifdef 	CONFIG_CONCURRENT_INIC_SUPPORT	
		if(((command_type&0x7FFF) == RACFG_CMD_TYPE_BOOTSTRAP)||
			(((command_type&0x7FFF) == RACFG_CMD_TYPE_SYNC)&&(command_id == RACFG_CMD_GET_MAC)))
		{
			if((!gAdapter[0]->RaCfgObj.flg_is_open)&&!(gAdapter[1]->RaCfgObj.flg_is_open))
			{
				kfree_skb(skb);
				return TRUE;
			}				
		}
		else
#endif // CONFIG_CONCURRENT_INIC_SUPPORT //			

		// use flg_is_open instead of NETIF_IS_UP to judge if ra0 is up
		// because NETIF_IS_UP won't true until host get MAC from iNIC
#ifdef NM_SUPPORT
		if (!pAd->RaCfgObj.MBSSID[0].MSSIDDev)
#else
		if (!pAd->RaCfgObj.MBSSID[0].MSSIDDev || !pAd->RaCfgObj.flg_is_open)
#endif
		{
#ifdef 	CONFIG_CONCURRENT_INIC_SUPPORT	
			/* 
			 * The system heart beat timer is on the main interface.
			 * Even the main interface is not opened, the heart beat can be received. 
			 */
			if((command_type == RACFG_CMD_TYPE_ASYNC)&& (ConcurrentObj.CardCount > 0))
			{
				switch(command_id)
				{
					case RACFG_CMD_HEART_BEAT:
					case RACFG_CMD_CONSOLE:
					{
			kfree_skb(skb);
			return TRUE;
		}
						break;
					default:
						break;
				}
			}else			
#endif // CONFIG_CONCURRENT_INIC_SUPPORT //				
			{
				kfree_skb(skb);
				return TRUE;
			}
		}

#if 1

#if (CONFIG_INF_TYPE==INIC_INF_TYPE_MII)
		if ((syncmiimac ==0)&&(pAd->RaCfgObj.bGetMac && !pAd->RaCfgObj.bRestartiNIC))
		{
			u8  *pSrcBufVA;
			pSrcBufVA = skb->data-14;
			if (memcmp(pSrcBufVA, pAd->master->dev_addr, 6) ||
				memcmp(pSrcBufVA+6, pAd->RaCfgObj.MainDev->dev_addr, 6))
			{
				kfree_skb(skb);
				return TRUE;
			}
		}
		else
#endif
			if (pAd->RaCfgObj.bGetMac && !pAd->RaCfgObj.bRestartiNIC)
		{
			u8  *pSrcBufVA;

			pSrcBufVA = skb->data-14;
#if (CONFIG_INF_TYPE==INIC_INF_TYPE_MII)
			if (syncmiimac && pAd->RaCfgObj.bLocalAdminAddr)
				pSrcBufVA[6] &= 0x7D;
			else
#endif
			pSrcBufVA[6] &= 0xFE;
			if (memcmp(pSrcBufVA, pAd->RaCfgObj.MainDev->dev_addr, 6) ||
				memcmp(pSrcBufVA+6, pAd->RaCfgObj.MainDev->dev_addr, 6))
			{
				kfree_skb(skb);
				return TRUE;
			}
		}
#endif

		switch (command_type)
		{
		case RACFG_CMD_TYPE_ASYNC | RACFG_CMD_TYPE_RSP_FLAG:
			//printk("Receive feedback: command type=%04x\n", command_type);
			if (command_id == RACFG_CMD_HEART_BEAT)
			{
				pAd->RaCfgObj.HeartBeatCount++;
				if (pAd->RaCfgObj.bRestartiNIC)
				{
					// make sure iNIC is ready for receiving commands
					pAd->RaCfgObj.bRestartiNIC = FALSE;
					RaCfgEnqueueRestart(pAd);
				}
				kfree_skb(skb);
			}
			else
			{
				FeedbackRspHandler(pAd, skb);
			}
			break;
		case RACFG_CMD_TYPE_SYNC         | RACFG_CMD_TYPE_RSP_FLAG:
			if (pAd->RaCfgObj.bGetMac && (command_id == RACFG_CMD_GET_MAC))
			{
				kfree_skb(skb);
				break;
			}
		case RACFG_CMD_TYPE_COPY_TO_USER | RACFG_CMD_TYPE_RSP_FLAG:
		case RACFG_CMD_TYPE_IWREQ_STRUC  | RACFG_CMD_TYPE_RSP_FLAG:
		case RACFG_CMD_TYPE_IW_HANDLER   | RACFG_CMD_TYPE_RSP_FLAG:
		case RACFG_CMD_TYPE_IOCTL_STATUS | RACFG_CMD_TYPE_RSP_FLAG:
			IoctlRspHandler(pAd, skb);
			break;
		case RACFG_CMD_TYPE_BOOTSTRAP | RACFG_CMD_TYPE_RSP_FLAG:
		case RACFG_CMD_TYPE_BOOTSTRAP:
			RaCfgCommandHandler(pAd, skb);
			break;

#if (CONFIG_INF_TYPE==INIC_INF_TYPE_MII)				
				case RACFG_CMD_TYPE_IGMP_TUNNEL | RACFG_CMD_TYPE_RSP_FLAG:
				{
					u16 src_port = le16_to_cpu(p_racfgh->dev_id);
					u16 payload_len = le16_to_cpu(p_racfgh->length);					
					IgmpTunnelRcvPkt(p_racfgh->data, payload_len,src_port);		
					kfree_skb(skb);
					break;
				}
		case RACFG_CMD_TYPE_TUNNEL| RACFG_CMD_TYPE_RSP_FLAG:
			{
				int bss_index = 0;
				u8 *pSrcBufVA = NULL;

				DBGPRINT("command_seq = %d, seq = %d \n", command_seq, seq);
				DBGPRINT("eapol_command_seq = %d, eapol_seq = %d \n", 
					pAd->RaCfgObj.eapol_command_seq, pAd->RaCfgObj.eapol_seq);

				// Check packet sequence
				if (seq == 0)
				{
					// Set a new packet sequence record
					pAd->RaCfgObj.eapol_command_seq = command_seq;
					pAd->RaCfgObj.eapol_seq = 0;

					// Flush old eapol_skb
					if (pAd->RaCfgObj.eapol_skb != NULL)
					{
						if (pAd->RaCfgObj.eapol_skb->len > 0)
						{
							printk("Warning: set a new packet sequence record, old eapol_skb len = %d\n", pAd->RaCfgObj.eapol_skb->len);                            
							printk("Warning: ignore mismatch eapol packet\n");
							pAd->RaCfgObj.eapol_skb->len = 0;
							pAd->RaCfgObj.eapol_skb->tail = pAd->RaCfgObj.eapol_skb->data;
						}
					}
				}
				else
				{
					// Ingore mismatch packet sequence, and flush the eapol_skb
					if ((pAd->RaCfgObj.eapol_command_seq != command_seq)||(pAd->RaCfgObj.eapol_seq != seq))
					{
						printk("Warning: ignore mismatch eapol packet\n");
						pAd->RaCfgObj.eapol_seq = 0;
						pAd->RaCfgObj.eapol_command_seq = 0;
						pAd->RaCfgObj.eapol_skb->len = 0;
						pAd->RaCfgObj.eapol_skb->tail = pAd->RaCfgObj.eapol_skb->data;
						break;
					}
				}
				pAd->RaCfgObj.eapol_seq ++;

				// Assemble EAPOL packet data				
				if (pAd->RaCfgObj.eapol_skb == NULL)
				{
					pAd->RaCfgObj.eapol_skb = dev_alloc_skb(MAX_EAPOL_SIZE);
					if (pAd->RaCfgObj.eapol_skb == NULL)
					{
						kfree_skb(skb);
						pAd->RaCfgObj.eapol_seq = 0;
						pAd->RaCfgObj.eapol_command_seq = 0;
						break;
					}
					skb_reserve(pAd->RaCfgObj.eapol_skb, 2);

				}

				if (pAd->RaCfgObj.eapol_skb->len + len <= (MAX_EAPOL_SIZE-2))
				{
					memcpy(skb_put(pAd->RaCfgObj.eapol_skb, len), 
						   payload, len);

					// free wrapped eapol frame
					kfree_skb(skb);
				}
				else
				{
					// free wrapped eapol frame
					kfree_skb(skb);
					pAd->RaCfgObj.eapol_seq = 0;
					pAd->RaCfgObj.eapol_command_seq = 0;
					pAd->RaCfgObj.eapol_skb->len = 0;
					pAd->RaCfgObj.eapol_skb->tail = pAd->RaCfgObj.eapol_skb->data;
					break;
				}

				if (len < MAX_FEEDBACK_LEN)
				{
					pSrcBufVA = pAd->RaCfgObj.eapol_skb->data;
					// Map interface index by source MAC address
					if (memcmp(pSrcBufVA, pAd->dev->dev_addr, 5) == 0)
						bss_index = pSrcBufVA[5] - pAd->dev->dev_addr[5];

						if((bss_index < 0)||(bss_index >= MAX_MBSSID_NUM))
					{
							// Reset Assembled eapol frame. 
						pAd->RaCfgObj.eapol_seq = 0;
						pAd->RaCfgObj.eapol_command_seq = 0;
						pAd->RaCfgObj.eapol_skb->len = 0;
						pAd->RaCfgObj.eapol_skb->tail = pAd->RaCfgObj.eapol_skb->data;
							break;
						}

						if(!NETIF_IS_UP(pAd->RaCfgObj.MBSSID[bss_index].MSSIDDev))
					{
							// Reset Assembled eapol frame. 
						pAd->RaCfgObj.eapol_seq = 0;
						pAd->RaCfgObj.eapol_command_seq = 0;
						pAd->RaCfgObj.eapol_skb->len = 0;
						pAd->RaCfgObj.eapol_skb->tail = pAd->RaCfgObj.eapol_skb->data;
						break;
					}

					// Fill sk_buff with information, and pass skb to network layer
					pAd->RaCfgObj.eapol_skb->dev = pAd->RaCfgObj.MBSSID[bss_index].MSSIDDev;
					pAd->RaCfgObj.eapol_skb->protocol = 
					eth_type_trans(pAd->RaCfgObj.eapol_skb, pAd->RaCfgObj.MBSSID[bss_index].MSSIDDev);
					pAd->RaCfgObj.eapol_skb->ip_summed = CHECKSUM_UNNECESSARY;
					netif_receive_skb(pAd->RaCfgObj.eapol_skb);  

					// Re-allocate a sk_buff
					pAd->RaCfgObj.eapol_skb = dev_alloc_skb(MAX_EAPOL_SIZE);

					if (pAd->RaCfgObj.eapol_skb)
					{
						skb_reserve(pAd->RaCfgObj.eapol_skb, 2);
					}
				}
				break;              
			}
#endif				
#ifdef WIDI_SUPPORT
		case RACFG_CMD_TYPE_WIDI_NOTIFY | RACFG_CMD_TYPE_RSP_FLAG:
			{
				u16 msg_type = le16_to_cpu(p_racfgh->dev_type);
				u16 payload_len = le16_to_cpu(p_racfgh->length);					
				WIDINotify(pAd->RaCfgObj.MainDev, p_racfgh->data, payload_len, msg_type);		
				kfree_skb(skb);
				break;
			}
#endif
		}

		bRacfg = TRUE;
	}

	return(bRacfg);
}

#if (CONFIG_INF_TYPE==INIC_INF_TYPE_MII)				
void IgmpTunnelRcvPkt(char *buff, int len, int src_port)
{
	printk("IgmpTunnelRcvPkt : src_port=%d\n", src_port);
	hex_dump("Rcv Igmp Pkt", buff, len);	
}

int IgmpTunnelSendPkt(struct net_device *net_dev, char *buff, int len)
{
	iNIC_PRIVATE    *pAd = (iNIC_PRIVATE *) netdev_priv(net_dev);
	struct sk_buff  *skb = NULL;
	struct racfghdr *p_racfgh = NULL;
	int 	status;
		
	if (len <= 0) 
		return FALSE;
	
	skb = dev_alloc_skb(len + sizeof(struct racfghdr) + ETH_HLEN + 2);
	if (skb == NULL)
	{
		printk("IgmpTunnelSendPkt Can not allocate skb buffer\n");
		return FALSE;
	}
	skb_reserve(skb, 2);
	skb_put(skb, len + sizeof(struct racfghdr) + ETH_HLEN);

	// build DA SA Type
	if(syncmiimac == 0)
	{
		memcpy(skb->data, net_dev->dev_addr, ETH_ALEN);
	    memcpy(skb->data + ETH_ALEN, pAd->master->dev_addr , ETH_ALEN);
	}
	else 
	{
		memcpy(skb->data, net_dev->dev_addr, ETH_ALEN);
		skb->data[0] |= 0x01;
		memcpy(skb->data + ETH_ALEN, net_dev->dev_addr, ETH_ALEN);
	}
	memset(skb->data + 2*ETH_ALEN, 0xff, 2);

    // build In Band Frame    
	p_racfgh = (struct racfghdr *) (skb->data + ETH_HLEN);
	memset(p_racfgh, 0x0, sizeof(struct racfghdr));
	p_racfgh->magic_no = cpu_to_le32(MAGIC_NUMBER);
	p_racfgh->command_type = cpu_to_le16(RACFG_CMD_TYPE_IGMP_TUNNEL);
	p_racfgh->length = cpu_to_le16(len);
#ifdef __BIG_ENDIAN
	p_racfgh->flags |= FLAG_BIG_ENDIAN;
#endif

    // copy buffer to In Band payload
	memcpy(&p_racfgh->data[0], buff, len);
	
	status = rlk_inic_start_xmit(skb, net_dev);
	
	if (status == NETDEV_TX_BUSY)
	{
		dev_kfree_skb(skb);
		printk("%s: in-band tx busy - free skb\n", __FUNCTION__);
	}
	return TRUE;
}
#endif

// mark SA MacAddr as multicast, to avoid leaking out of the bridge
unsigned char BootCmdHdr[]  = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01, 0x0c, 0x43, 0x00, 0x00, 0x00, 0xff, 0xff};

void send_racfg_cmd(iNIC_PRIVATE *pAd, char *buff, int len)
{
	struct net_device *dev = pAd->RaCfgObj.MainDev;
	unsigned char   p8023hdr[14];
	struct sk_buff  *skb;
	int				status;
	
	//iNIC_PRIVATE    *pAd = (iNIC_PRIVATE *) netdev_priv(dev);

	//crash on linux_2_6: skb = __dev_alloc_skb(256, GFP_DMA | GFP_ATOMIC);
	//skb = dev_alloc_skb(256);
	skb = dev_alloc_skb(len + 16);
	if (skb == NULL)
	{
		printk("Can not allocate skb buffer\n");
		return;
	}

#if (CONFIG_INF_TYPE==INIC_INF_TYPE_MII)
	if ((syncmiimac ==0)&&(pAd->RaCfgObj.bGetMac && !pAd->RaCfgObj.bRestartiNIC))
	{
		memcpy(p8023hdr, dev->dev_addr, 6);
		memcpy(p8023hdr+6, pAd->master->dev_addr , 6);
		memset(p8023hdr+12, 0xff, 2);
		/* Note: Legacy usage of STA MiiMasterMac has been changed 
		   if (!pAd->RaCfgObj.opmode) memcpy(p8023hdr+6, dev->dev_addr, 6);
		 */
	}
	else
#endif
		if (pAd->RaCfgObj.bGetMac && !pAd->RaCfgObj.bRestartiNIC)
	{
		memcpy(p8023hdr, dev->dev_addr, 6);
		p8023hdr[0] |= 0x01;
		memcpy(p8023hdr+6, dev->dev_addr, 6);
		memset(p8023hdr+12, 0xff, 2);
	}
	else
	{
		memcpy(p8023hdr, BootCmdHdr, 14);
	}

	skb_reserve(skb, 2);
	memcpy(skb_put(skb, 14), p8023hdr, 14);
	memcpy(skb_put(skb, len), buff, len);

	/*
	if (pAd->RaCfgObj.bGetMac)
	{
		hex_dump("SendInBandCmd:", buff, 48);
	}
	*/

	status = rlk_inic_start_xmit(skb, dev);
	
	// free skb buffer
	if (status == NETDEV_TX_BUSY)
	{		
		dev_kfree_skb(skb);
		printk("%s: in-band tx busy - free skb\n", __FUNCTION__);
	}
}


static void rlk_inic_set_mac(iNIC_PRIVATE *pAd, char *mac_addr)
{
	int j;
	char *p = mac_addr;
	struct net_device *dev = pAd->RaCfgObj.MainDev;

	for (j = 0; j < MAC_ADDR_LEN; j++)
	{
		AtoH(p, &dev->dev_addr[j], 1);
		p += 3;
	}

	for (j = 0; j < pAd->RaCfgObj.BssidNum; j++)
	{
		struct net_device *mbss_dev = pAd->RaCfgObj.MBSSID[j].MSSIDDev;
		if (!mbss_dev) continue;

		memmove(pAd->RaCfgObj.MBSSID[j].Bssid, 
				dev->dev_addr, MAC_ADDR_LEN);
#ifdef NEW_MBSS_SUPPORT
#ifdef CONFIG_CONCURRENT_INIC_SUPPORT
		if(pAd == gAdapter[0])
#else
		if(1)
#endif
		{
			if(j > 0){
				pAd->RaCfgObj.MBSSID[j].Bssid[0] += 2;
				pAd->RaCfgObj.MBSSID[j].Bssid[0] += ((j - 1) << 2);
			}
		}
		else
#endif
		pAd->RaCfgObj.MBSSID[j].Bssid[5] += j;
		memmove(mbss_dev->dev_addr,
				pAd->RaCfgObj.MBSSID[j].Bssid, MAC_ADDR_LEN);
		printk("Update MAC(%d)=%02x:%02x:%02x:%02x:%02x:%02x\n",
			   j, 
			   mbss_dev->dev_addr[0],
			   mbss_dev->dev_addr[1],
			   mbss_dev->dev_addr[2],
			   mbss_dev->dev_addr[3],
			   mbss_dev->dev_addr[4],
			   mbss_dev->dev_addr[5]);
	}

	// WDS
	for (j = 0; j < MAX_WDS_NUM; j++)
	{
		struct net_device *wds_dev = pAd->RaCfgObj.WDS[j].MSSIDDev;
		if (!wds_dev) continue;

		memmove(pAd->RaCfgObj.WDS[j].Bssid, 
				dev->dev_addr, MAC_ADDR_LEN);
		memmove(wds_dev->dev_addr,
				pAd->RaCfgObj.WDS[j].Bssid, MAC_ADDR_LEN);
		printk("Update WDS(%d) MAC=%02x:%02x:%02x:%02x:%02x:%02x\n",
			   j, 
			   wds_dev->dev_addr[0],
			   wds_dev->dev_addr[1],
			   wds_dev->dev_addr[2],
			   wds_dev->dev_addr[3],
			   wds_dev->dev_addr[4],
			   wds_dev->dev_addr[5]);
	}

#ifdef MESH_SUPPORT
	// MESH
	for (j = 0; j < MAX_MESH_NUM; j++)
	{
		struct net_device *mesh_dev = pAd->RaCfgObj.MESH[j].MSSIDDev;
		if (!mesh_dev)	continue;

		memmove(pAd->RaCfgObj.MESH[j].Bssid, 
				dev->dev_addr, MAC_ADDR_LEN);

		// if AP has own Mac, STA Mac same as Main Device
		if (pAd->RaCfgObj.opmode == 1)
		{
#ifdef NEW_MBSS_SUPPORT
#ifdef CONFIG_CONCURRENT_INIC_SUPPORT
			if(pAd == gAdapter[0])
#else
			if(1)
#endif
			{
				pAd->RaCfgObj.MESH[j].Bssid[0] += 2;
				pAd->RaCfgObj.MESH[j].Bssid[0] += ((pAd->RaCfgObj.BssidNum + j - 1) << 2);
			}
			else
#endif
			pAd->RaCfgObj.MESH[j].Bssid[5] += pAd->RaCfgObj.BssidNum + j;
		}
		memmove(mesh_dev->dev_addr,
				pAd->RaCfgObj.MESH[j].Bssid, MAC_ADDR_LEN);
		printk("Update MESH(%d) MAC=%02x:%02x:%02x:%02x:%02x:%02x\n",
			   j, 
			   mesh_dev->dev_addr[0],
			   mesh_dev->dev_addr[1],
			   mesh_dev->dev_addr[2],
			   mesh_dev->dev_addr[3],
			   mesh_dev->dev_addr[4],
			   mesh_dev->dev_addr[5]);
	}
#endif // MESH_SUPPORT //

	// APCLI
	for (j = 0; j < MAX_APCLI_NUM; j++)
	{
		struct net_device *apcli_dev = pAd->RaCfgObj.APCLI[j].MSSIDDev;
		if (!apcli_dev)	continue;

		memmove(pAd->RaCfgObj.APCLI[j].Bssid, 
				dev->dev_addr, MAC_ADDR_LEN);
#ifdef NEW_MBSS_SUPPORT
#ifdef CONFIG_CONCURRENT_INIC_SUPPORT
		if(pAd == gAdapter[0])
#else
		if(1)
#endif
		{
			pAd->RaCfgObj.APCLI[j].Bssid[0] += 2;
#ifdef MESH_SUPPORT
			pAd->RaCfgObj.APCLI[j].Bssid[0] += ((pAd->RaCfgObj.BssidNum + MAX_MESH_NUM + j - 1) << 2);
#else
			pAd->RaCfgObj.APCLI[j].Bssid[0] += ((pAd->RaCfgObj.BssidNum + j - 1) << 2);
#endif
		}
		else
#endif
		{
		pAd->RaCfgObj.APCLI[j].Bssid[5] += pAd->RaCfgObj.BssidNum + j;
#ifdef MESH_SUPPORT
		if (pAd->RaCfgObj.bMesh)
			pAd->RaCfgObj.APCLI[j].Bssid[5] += MAX_MESH_NUM;
#endif // MESH_SUPPORT //
		}
		memmove(apcli_dev->dev_addr,
				pAd->RaCfgObj.APCLI[j].Bssid, MAC_ADDR_LEN);
		printk("Update APCLI(%d) MAC=%02x:%02x:%02x:%02x:%02x:%02x\n",
			   j, 
			   apcli_dev->dev_addr[0],
			   apcli_dev->dev_addr[1],
			   apcli_dev->dev_addr[2],
			   apcli_dev->dev_addr[3],
			   apcli_dev->dev_addr[4],
			   apcli_dev->dev_addr[5]);
	}
}


#ifndef NM_SUPPORT

#ifndef container_of
#define container_of    list_entry
#endif
#define TO_RAOBJ(obj)    container_of(obj, RACFG_OBJECT, net_dev_notifier)
/*
 * Closing MBSS at NETDEV_GOING_DOWN, instead of rlk_inic_close() ,
 * thus we can  send remote TARGET the CLOSE command in time.
 */
 
static int netdev_event(
					   struct notifier_block *this,
					   unsigned long event, void *arg)
{
        printk("EVENT 0x%lu\n",event);
        if (event == NETDEV_UP)
        {
          RACFG_OBJECT  *pRaObj = (RACFG_OBJECT *)TO_RAOBJ(this);
          struct net_device *dev = (struct net_device *)pRaObj->MainDev;
          iNIC_PRIVATE *rt = netdev_priv(dev);

            printk("NETDEV UP\n");
            RaCfgEnqueueRestart(rt);
        }

        else
	if (event == NETDEV_GOING_DOWN)
	{
		if (arg)
		{
			RACFG_OBJECT  *pRaObj = (RACFG_OBJECT *)TO_RAOBJ(this);

#ifdef WOWLAN_SUPPORT 	
			if(pRaObj->bWoWlanUp == TRUE && pRaObj->pm_wow_state == WOW_CPU_DOWN )
			{
				DBGPRINT("CPU is in Sleep mode....\n");
				return NOTIFY_OK;
			}
			else
			{
				DBGPRINT("pRaObj->pm_wow_state = %ld \n", pRaObj->pm_wow_state);
			}	
#endif // WOWLAN_SUPPORT // 			
			
			if (arg == pRaObj->MainDev)
			{
				DBGPRINT("Net device %s going down....\n", ((struct net_device *)arg)->name);
				close_all_interfaces((struct net_device *)arg);
			}
#if (CONFIG_INF_TYPE==INIC_INF_TYPE_MII)
#ifndef MII_SLAVE_STANDALONE
			else
			{
				struct net_device *dev = (struct net_device *)pRaObj->MainDev;
				iNIC_PRIVATE *pAd = (iNIC_PRIVATE *) netdev_priv(dev);
				if (arg == pAd->master && NETIF_IS_UP(dev))
				{
					DBGPRINT("WARNING: wireless slave (%s) for "
							 "MII (%s) also going down....\n", 
							 pRaObj->MainDev->name, 
							 ((struct net_device *)arg)->name);
					dev_close(dev);
				}
			}
#endif
#endif
		}
		return NOTIFY_OK;
	}
#if (CONFIG_INF_TYPE==INIC_INF_TYPE_MII)

	else if (event == NETDEV_REGISTER)
	{
		RACFG_OBJECT  *pRaObj = (RACFG_OBJECT *)TO_RAOBJ(this);
		struct net_device *dev = (struct net_device *)pRaObj->MainDev;
		iNIC_PRIVATE *pAd = (iNIC_PRIVATE *) netdev_priv(dev);
		if (!pAd->master) // master unregistered, get it back
		{
			struct net_device *master = (struct net_device *)arg;

			if (!strcmp(master->name, miimaster))
			{
				printk("MII master (%s) registration detected.\n", 
					   master->name);
				pAd->master = master;
			}
		}
	}
	else if (event == NETDEV_UNREGISTER)
	{
		if (arg)
		{
			RACFG_OBJECT  *pRaObj = (RACFG_OBJECT *)TO_RAOBJ(this);
			struct net_device *dev = (struct net_device *)pRaObj->MainDev;
			iNIC_PRIVATE *pAd = (iNIC_PRIVATE *) netdev_priv(dev);
			if (arg == pAd->master)
			{
				printk("MII master (%s) unregistration detected.\n", 
					   pAd->master->name);
				pAd->master = NULL;
			}
		}
	}
#endif
	return NOTIFY_DONE;
}

void close_all_interfaces(struct net_device *root_dev)
{
	iNIC_PRIVATE *rt = (iNIC_PRIVATE *) netdev_priv(root_dev);

#ifdef CONFIG_CONCURRENT_INIC_SUPPORT
	ConcurrentObj.CardCount--;
#endif // CONFIG_CONCURRENT_INIC_SUPPORT //

	if (rt->RaCfgObj.bWds)
        rlk_inic_wds_close(rt);
	if (rt->RaCfgObj.bApcli)
        rlk_inic_apcli_close(rt);
#ifdef MESH_SUPPORT
	if (rt->RaCfgObj.bMesh)
		rlk_inic_mesh_close(rt);
#endif // MESH_SUPPORT //

    /* we have to call rlk_inic_mbss_close to send "close ra0" *
	 * command to iNIC, no matter mbss is enabled or not     */
    rlk_inic_mbss_close(rt);
}

#endif // of not define NM_SUPPORT


#ifdef WOWLAN_SUPPORT 
 
#define TO_RAOBJ_PM(obj)    container_of(obj, RACFG_OBJECT, pm_wow_notifier)

static int pm_wow_event(struct notifier_block *this, unsigned long event, void *arg)
{
	RACFG_OBJECT  *pRaObj = (RACFG_OBJECT *)TO_RAOBJ_PM(this);

	printk("INIC host driver receiving cpu notifier \n"); 

	if(pRaObj->bWoWlanUp != TRUE )
		return NOTIFY_DONE;

    switch (event) 
	{        
		case PM_HIBERNATION_PREPARE:        
		case PM_SUSPEND_PREPARE:   
		case CPU_DOWN_PREPARE_FROZEN: 	             
			//cpu goes to sleep 
			if (pRaObj->pm_wow_state != WOW_CPU_DOWN)
			{	      
				pRaObj->pm_wow_state = WOW_CPU_DOWN;   
				printk("ralink host drivr detect CPU sleep \n"); 
			}
			break;              
		case PM_POST_HIBERNATION:        
		case PM_POST_SUSPEND:      
		case CPU_ONLINE_FROZEN:            
			//cpu goes to wake up  
			if (pRaObj->pm_wow_state != WOW_CPU_UP)      
			{
				pRaObj->pm_wow_state = WOW_CPU_UP;              
				printk("ralink host drivr detect CPU wakeup \n"); 
			}
			break;        
		default:                
				break;
    }
    			
	return NOTIFY_DONE;	
}					   
#endif // WOWLAN_SUPPORT  //



void RaCfgInterfaceOpen(iNIC_PRIVATE *pAd)
{
#ifdef NM_SUPPORT	
	pAd->RaCfgObj.flg_is_open = 1;
#endif	
}

void RaCfgInterfaceClose(iNIC_PRIVATE *pAd)
{
#ifdef NM_SUPPORT	
	pAd->RaCfgObj.flg_is_open = 0;
#endif	
}

#ifdef MULTIPLE_CARD_SUPPORT

extern int profile_get_keyparameter(
								   char *   key,
								   char *   dest,   
								   int     destsize,
								   char *   buffer);


boolean CardInfoRead(
					iNIC_PRIVATE *pAd)
{

	struct file *srcf;
	s32 retval, orgfsuid, orgfsgid;
	mm_segment_t orgfs;
	s8 *buffer, *tmpbuf, search_buf[30];
	boolean flg_match_ok = FALSE;
	s32 card_free_id, card_nouse_id, card_same_mac_id, card_match_id;
	u32 card_index;
	s8 card_info_path[30];

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,28)
	struct cred *override_cred, *old_cred;
#endif

	// init
	buffer = kmalloc(MAX_INI_BUFFER_SIZE, MEM_ALLOC_FLAG);
	if (buffer == NULL)
		return FALSE;

	tmpbuf = kmalloc(MAX_PARAM_BUFFER_SIZE, MEM_ALLOC_FLAG);
	if (tmpbuf == NULL)
	{
		kfree(buffer);
		return NDIS_STATUS_FAILURE;
	}

#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,28)
	orgfsuid = current->fsuid;
	orgfsgid = current->fsgid;
	current->fsuid = current->fsgid = 0;
#else
	orgfsuid = current_fsuid();
	orgfsgid = current_fsgid();
	override_cred = prepare_creds();
	if (!override_cred)
		return -ENOMEM;
	override_cred->fsuid = 0;
	override_cred->fsgid = 0;
	old_cred = override_creds(override_cred);
#endif

	orgfs = get_fs();
	set_fs(KERNEL_DS);


	// open card information file
	if (strcmp(mode, "sta") == 0)
	{
		RLK_STRCPY(card_info_path, INIC_STA_CARD_INFO_PATH);
	}
	else
	{
		RLK_STRCPY(card_info_path, INIC_AP_CARD_INFO_PATH);
	}   
	srcf = filp_open(card_info_path, O_RDONLY, 0);
	if (IS_ERR(srcf))
	{
		/* card information file does not exist */
		DBGPRINT("--> Error %ld opening %s\n", -PTR_ERR(srcf), card_info_path);
		return FALSE;
	}

	pAd->RaCfgObj.InterfaceNumber = -1; // use default profile path

	if (srcf->f_op && srcf->f_op->read)
	{
		/* card information file exists so reading the card information */
		memset(buffer, 0x00, MAX_INI_BUFFER_SIZE);
		retval = srcf->f_op->read(srcf, buffer, MAX_INI_BUFFER_SIZE, &srcf->f_pos);
		if (retval < 0)
		{
			/* read fail */
			DBGPRINT("--> Read %s error %d\n", card_info_path, -retval);
		}
		else
		{
			/* get card selection method */
			memset(tmpbuf, 0x00, MAX_PARAM_BUFFER_SIZE);


			// init
			card_free_id = -1;
			card_nouse_id = -1;
			card_same_mac_id = -1;
			card_match_id = -1;

			// search current card information records
			for (card_index=0;
				card_index<MAX_NUM_OF_MULTIPLE_CARD;
				card_index++)
			{
				if (MC_CardIrq[card_index] == 0)
				{
					// Irq is 0 so the entry is available
					MC_CardUsed[card_index] = 0;

					if (card_free_id < 0)
						card_free_id = card_index; // 1st free entry
				}
				else
				{
					if (MC_CardIrq[card_index] == pAd->dev->irq)
					{
						// We find the entry with same irq
						if (card_same_mac_id < 0)
							card_same_mac_id = card_index; // 1st same entry
					}
					else
					{
						// Irq is not 0 but used flag == 0
						if ((MC_CardUsed[card_index] == 0) &&
							(card_nouse_id < 0))
						{
							card_nouse_id = card_index;	// 1st available entry
						}
					}
				}
			}

			DBGPRINT("MC> Free = %d, Same = %d, NOUSE = %d\n",
					 card_free_id, card_same_mac_id, card_nouse_id);

			if (card_same_mac_id >= 0)
			{
				// same irq entry is found
				card_match_id = card_same_mac_id;
			}
			else
			{
				// the card is 1st plug-in, try to find the match card profile
				if (card_free_id >= 0)
					card_match_id = card_free_id;
				else
					card_match_id = card_nouse_id;


				// if not find a free one, use the available one
				if (card_match_id < 0)
					card_match_id = card_nouse_id;
			}

			if (card_match_id >= 0)
			{


				// read card file path
				snprintf(search_buf, sizeof(search_buf), "%02dProfilePath", card_match_id);
				if (profile_get_keyparameter(search_buf, tmpbuf, 256, buffer))
				{
					if (strlen(tmpbuf) < sizeof(pAd->RaCfgObj.profile.read_name))
					{

						pAd->RaCfgObj.InterfaceNumber = card_match_id;	/* base 0 */

						// backup card information
						MC_CardUsed[card_match_id] = 1;
						MC_CardIrq[card_match_id] = pAd->dev->irq;

						// backup card file path
						memmove(pAd->RaCfgObj.profile.read_name, tmpbuf , strlen(tmpbuf));
						pAd->RaCfgObj.profile.read_name[strlen(tmpbuf)] = '\0';
						flg_match_ok = TRUE;

						DBGPRINT("Card Profile Name = %s\n", pAd->RaCfgObj.profile.read_name);
					}
					else
					{
						DBGPRINT("Card Profile Name length too large!\n");
					}
				}
				else
				{
					DBGPRINT("Can not find search key word in card.dat!\n");
				}

				// read MAC address
				snprintf(search_buf, sizeof(search_buf), "%02dMAC", card_match_id);
				if (profile_get_keyparameter(search_buf, tmpbuf, 256, buffer))
				{
					RLK_STRCPY(pAd->RaCfgObj.MC_MAC, tmpbuf);
					DBGPRINT("%02dCard MAC address = %s\n", card_match_id, pAd->RaCfgObj.MC_MAC);
				}


				if ((flg_match_ok != TRUE) &&
					(card_match_id < MAX_NUM_OF_MULTIPLE_CARD))
				{
					MC_CardUsed[card_match_id] = 0;
					MC_CardIrq[card_match_id] = 0;
				}
			} // if (card_match_id >= 0)
		}
	}

	if (pAd->RaCfgObj.InterfaceNumber <0)
	{

		if (strcmp(mode, "sta") == 0)
		{
			RLK_STRCPY(pAd->RaCfgObj.profile.read_name, INIC_STA_PROFILE_PATH);
		}
		else
		{
			RLK_STRCPY(pAd->RaCfgObj.profile.read_name, INIC_AP_PROFILE_PATH);
		}       
	}
	DBGPRINT("MC> ROW = %d, PATH = %s\n", pAd->RaCfgObj.InterfaceNumber, pAd->RaCfgObj.profile.name);


	// close file
	retval = filp_close(srcf, NULL);
	set_fs(orgfs);
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,28)
	current->fsuid = orgfsuid;
	current->fsgid = orgfsgid;
#else
	revert_creds(old_cred);
	put_cred(override_cred);
#endif
	kfree(buffer);
	kfree(tmpbuf);
	return flg_match_ok;
}
#endif // MULTIPLE_CARD_SUPPORT //

#ifdef CONFIG_CONCURRENT_INIC_SUPPORT

extern int profile_get_keyparameter(
								   char *   key,
								   char *   dest,   
								   int     destsize,
								   char *   buffer);

boolean ConcurrentCardInfoRead(void)
{

	struct file *srcf;
	s32 retval, orgfsuid, orgfsgid;
	mm_segment_t orgfs;
	s8 *buffer, *tmpbuf, search_buf[30];
	boolean flg_match_ok = FALSE;
	s8 card_info_path[256];
	int card_idx;
	int i;
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,28)
	struct cred *override_cred, *old_cred;
#endif	
	// init 

	for(i = 0;i < CONCURRENT_CARD_NUM; i++)
		RLK_STRCPY(ConcurrentObj.Mac[i],"");

	buffer = kmalloc(MAX_INI_BUFFER_SIZE, MEM_ALLOC_FLAG);
	if (buffer == NULL)
		return FALSE;

	tmpbuf = kmalloc(MAX_PARAM_BUFFER_SIZE, MEM_ALLOC_FLAG);
	if (tmpbuf == NULL)
	{
		kfree(buffer);
		return NDIS_STATUS_FAILURE;
	}
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,28)
	orgfsuid = current->fsuid;
	orgfsgid = current->fsgid;
	current->fsuid = current->fsgid = 0;
#else
	orgfsuid = current_fsuid();
	orgfsgid = current_fsgid();
	override_cred = prepare_creds();
	if (!override_cred)
		return -ENOMEM;
	override_cred->fsuid = 0;
	override_cred->fsgid = 0;
	old_cred = (struct cred *)override_creds(override_cred);
#endif
	orgfs = get_fs();
	set_fs(KERNEL_DS);


	// Open card information file and Set card default dat file path 
	if (strcmp(mode, "sta") == 0)
	{
		RLK_STRCPY(card_info_path, INIC_STA_CARD_INFO_PATH);
		RLK_STRCPY(ConcurrentObj.Profile[0].read_name, INIC_STA_PROFILE_PATH);
		RLK_STRCPY(ConcurrentObj.Profile[1].read_name, INIC_STA_CONCURRENT_PROFILE_PATH);		
	}
	else
	{
		RLK_STRCPY(card_info_path, INIC_AP_CARD_INFO_PATH);	
		RLK_STRCPY(ConcurrentObj.Profile[0].read_name, INIC_AP_PROFILE_PATH);
		RLK_STRCPY(ConcurrentObj.Profile[1].read_name, INIC_AP_CONCURRENT_PROFILE_PATH);
	}

	RLK_STRCPY(ConcurrentObj.ExtEeprom[0].read_name, EEPROM_BIN_FILE_PATH);		
	RLK_STRCPY(ConcurrentObj.ExtEeprom[1].read_name, CONCURRENT_EEPROM_BIN_FILE_PATH);		


#ifdef DBG
	snprintf(buffer, sizeof(buffer), "%s%s", root, card_info_path);
	RLK_STRCPY(card_info_path, buffer);
#endif
	
	srcf = filp_open(card_info_path, O_RDONLY, 0);
	if (IS_ERR(srcf))
	{
		/* card information file does not exist */
		DBGPRINT("No card information file.\nUse default profile path.\n");
		return FALSE;
	}
	else
	{
		DBGPRINT("Open %s to set profile path\n", card_info_path);		
	}


	if (srcf->f_op && srcf->f_op->read)
	{
		/* card information file exists so reading the card information */
		memset(buffer, 0x00, MAX_INI_BUFFER_SIZE);
		retval = srcf->f_op->read(srcf, buffer, MAX_INI_BUFFER_SIZE, &srcf->f_pos);
		if (retval < 0)
		{
			/* read fail */
			DBGPRINT("--> Read %s error %d.\n Use default profile path.\n", card_info_path, -retval);
		}
		else
		{
			/* get card selection method */
			memset(tmpbuf, 0x00, MAX_PARAM_BUFFER_SIZE);
			

			for(card_idx = 0; card_idx < CONCURRENT_CARD_NUM; card_idx++)
			{
				// read card file path
				snprintf(search_buf, sizeof(search_buf), "%02dProfilePath", card_idx);
				if (profile_get_keyparameter(search_buf, tmpbuf, 256, buffer))
				{
					if (strlen(tmpbuf) < sizeof(ConcurrentObj.Profile[card_idx].read_name))
					{
						// backup card file path
						memmove(ConcurrentObj.Profile[card_idx].read_name, tmpbuf , strlen(tmpbuf));
						ConcurrentObj.Profile[card_idx].read_name[strlen(tmpbuf)] = '\0';

							
						DBGPRINT("Card Profile Name[%d] = %s\n", card_idx, ConcurrentObj.Profile[card_idx].read_name);
					}
					else
					{
						DBGPRINT("Card Profile Name[%d] length too large!\n", card_idx);
					}					
				}
				else
				{
					DBGPRINT("Can not find search key word in card.dat!\n");
				}
				// read MAC address
				snprintf(search_buf, sizeof(search_buf), "%02dMAC", card_idx);
				if (profile_get_keyparameter(search_buf, tmpbuf, 256, buffer))
				{
					RLK_STRCPY(ConcurrentObj.Mac[card_idx], tmpbuf);
					DBGPRINT("%02dCard MAC address = %s\n", card_idx, tmpbuf);
				}				
			}
		}
	}
	
	// close file
	retval = filp_close(srcf, NULL);
	set_fs(orgfs);
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,28)
	current->fsuid = orgfsuid;
	current->fsgid = orgfsgid;
#else
	revert_creds(old_cred);
	put_cred(override_cred);
#endif
	kfree(buffer);
	kfree(tmpbuf);
	return flg_match_ok;
}



void DispatchAdapter(iNIC_PRIVATE **ppAd, struct sk_buff *skb)
{

	/* Dispatch in-band command to the relative adapter */
	if(skb->protocol == 0xFFFF)
	{
		struct racfghdr *p_racfgh =  (struct racfghdr *) skb->data;	
		u16 command_type	= le16_to_cpu(p_racfgh->command_type);
		u16 command_id  	= le16_to_cpu(p_racfgh->command_id);
		u16 dev_type		= le16_to_cpu(p_racfgh->dev_type);
		u8 interface_number = 0;

		/*
		 * The iNIC bootloader only send boot notiy to the main interface.
		 * The second interface maybe be opended in advance.
		 * Dispatch the boot notify to the interface has been opened.
		 */
		
		if(((command_type&0x7FFF) == RACFG_CMD_TYPE_BOOTSTRAP))
		{
			*ppAd = gAdapter[0];
		}
		else if((((command_type&0x7FFF) == RACFG_CMD_TYPE_SYNC)&&(command_id == RACFG_CMD_GET_MAC)))
		{
			int i;
			for(i=0;i<2;i++)
			{
				if(gAdapter[i])
				{
					if(gAdapter[i]->RaCfgObj.flg_is_open)
					{
						*ppAd = gAdapter[i];
						break;
					}
				}
			}
		}
		else 
		{
			interface_number = ((dev_type & DEV_TYPE_CONCURRENT_FLAG)?1:0);

			*ppAd = gAdapter[interface_number];
		}
	}
}

void SetRadioOn(iNIC_PRIVATE *pAd, u8 Operation)
{
	struct net_device	*dev = pAd->dev;
	struct iwreq wrq;
	char tmp[64];

	/* If radio is set off in profile, radio is not on when interface is up. */
	if ((pAd->RaCfgObj.bRadioOff == TRUE)&&(Operation == 1))
	{
		return;
	}
	
	if(pAd->RaCfgObj.opmode)
	{
		/* AP mode */
	snprintf(tmp, sizeof(tmp), "RadioOn=%d", Operation);

	memset(&wrq, 0, sizeof(wrq));

	RLK_STRCPY(wrq.ifr_ifrn.ifrn_name, dev->name);
	wrq.u.data.length = strlen(tmp);
	wrq.u.data.pointer = tmp;

	/* wrq is allocate in kernel space.  Set wrq.u.data.pointer to kernel_data*/
	RTMPIoctlHandler(pAd, RTPRIV_IOCTL_SET, IW_POINT_TYPE, 0, &wrq, tmp, FALSE);	
	}
	else
	{
		/* STA mode */

		memset(&wrq, 0, sizeof(wrq));		
		RLK_STRCPY(wrq.ifr_ifrn.ifrn_name, dev->name);

		if(Operation)
			wrq.u.data.flags = RAIO_ON;
		else
			wrq.u.data.flags = RAIO_OFF;
		
		RTMPIoctlHandler(pAd, RTPRIV_IOCTL_SHOW, IW_POINT_TYPE, 0, &wrq, NULL, TRUE);	
	}

	
	return;
}

#endif // CONFIG_CONCURRENT_INIC_SUPPORT //

#ifdef WOWLAN_SUPPORT

static void RaCfgWowInbandSend(uintptr_t arg)
{
	iNIC_PRIVATE *pAd = (iNIC_PRIVATE *)arg;
	struct net_device	*dev = pAd->dev;
	
	u32 sync_info;  
	struct iwreq wrq;
	sync_info = 1;
	memset(&wrq, 0, sizeof(wrq));

	RLK_STRCPY(wrq.ifr_ifrn.ifrn_name, dev->name);
	wrq.u.data.length = sizeof(u32); 
	wrq.u.data.pointer = (char *) &sync_info;
	wrq.u.data.flags = (OID_GET_SET_TOGGLE | RT_OID_802_11_WAKEUP_SYNC);

	/* wrq is allocate in kernel space.  Set wrq.u.data.pointer to kernel_data*/
	RTMPIoctlHandler(pAd, RT_PRIV_IOCTL, IW_POINT_TYPE, 0, &wrq, (char *) &sync_info, FALSE);	 		
	//DBGPRINT("==== RaCfgWowInbandSend! ===\n");
	
}

static void RaCfgWowInbandTimeout(uintptr_t arg)
{
	iNIC_PRIVATE *pAd = (iNIC_PRIVATE *)arg;
	
#ifndef NM_SUPPORT
	if (!NETIF_IS_UP(pAd->RaCfgObj.MainDev))
		return;
#endif

	if (!pAd->RaCfgObj.bWoWlanUp)
		return;
	
	//wow heart beat is sent, only when CPU is up.
	if (pAd->RaCfgObj.pm_wow_state == WOW_CPU_UP )
	RaCfgEnqueue(pAd, &pAd->RaCfgObj.backlogQueue, &pAd->RaCfgObj.backlogSem, RaCfgWowInbandSend, (uintptr_t)pAd);

	mod_timer(&pAd->RaCfgObj.WowInbandSignalTimer, jiffies + WOW_INBAND_TIMEOUT * HZ);
}

static void RaCfgAddWowInbandTimer(iNIC_PRIVATE *pAd)
{
	RTMP_SEM_LOCK(&pAd->RaCfgObj.WowInbandSignalTimerLock);

	if (!pAd->RaCfgObj.bWoWlanUp)
	{
		RTMP_SEM_UNLOCK(&pAd->RaCfgObj.WowInbandSignalTimerLock);
		return;
	}	

	if (!pAd->RaCfgObj.WowInbandSignalTimer.function)
	{
		init_timer(&pAd->RaCfgObj.WowInbandSignalTimer);
		pAd->RaCfgObj.WowInbandSignalTimer.function = RaCfgWowInbandTimeout;
		pAd->RaCfgObj.WowInbandSignalTimer.data = (uintptr_t)pAd;
	}
	mod_timer(&pAd->RaCfgObj.WowInbandSignalTimer, jiffies + WOW_INBAND_TIMEOUT * HZ);
	RTMP_SEM_UNLOCK(&pAd->RaCfgObj.WowInbandSignalTimerLock);
}	

static void RaCfgDelHeartWowInbandTimer(iNIC_PRIVATE *pAd)
{
	if (!pAd->RaCfgObj.bWoWlanUp)
		return;	
	
	RTMP_SEM_LOCK(&pAd->RaCfgObj.WowInbandSignalTimerLock);
	if (pAd->RaCfgObj.WowInbandSignalTimer.function)
	{
		del_timer_sync(&pAd->RaCfgObj.WowInbandSignalTimer);
		pAd->RaCfgObj.WowInbandSignalTimer.function = NULL;
		pAd->RaCfgObj.WowInbandSignalTimer.data = 0;
	}
	RTMP_SEM_UNLOCK(&pAd->RaCfgObj.WowInbandSignalTimerLock);
}

#endif // WOWLAN_SUPPORT //




