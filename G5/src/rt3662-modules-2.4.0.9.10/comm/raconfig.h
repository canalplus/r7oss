#ifndef __RACONFIG_H__
#define __RACONFIG_H__

#include <linux/kernel.h>
#include <linux/compiler.h>
#include <linux/module.h>
//#include <linux/config.h>
#include <linux/autoconf.h>//peter : for FC6
#include <linux/version.h>
#include <linux/byteorder/generic.h>
#include <linux/unistd.h>
#include <linux/ctype.h>
#include <linux/cache.h>
#include <linux/skbuff.h>
#include <linux/interrupt.h>
#include <linux/notifier.h>
#include <net/iw_handler.h>

/*
 *	IEEE 802.3 Ethernet magic constants.  The frame sizes omit the preamble
 *	and FCS/CRC (frame check sequence). 
 */

#define ETH_ALEN	6		/* Octets in one ethernet addr	 */
#define ETH_HLEN	14		/* Total octets in header.	 */


/* 
 *	Ethernet Protocol ID used by RaCfg Protocol
 */

#define ETH_P_RACFG	0xFFFF

#define INBAND_HEADER_SIZE ETH_HLEN  // DA(6) + SA(6) + PROTO(2)
#define RACFG_HEADER_SIZE  12 // MAGIC(4) + CMD_TYPE(2) + CMD_ID(2) + CMD_LEN(2) + CMD_SEQ(2)

#define FLAG_BIG_ENDIAN (1 << 0)
/*
 * This is an RaCfg frame header 
 */

struct racfghdr // 32 bytes
{
	u32     magic_no;
	u16     command_type;
	u16     command_id;
	u16     length;
	u16     sequence; // packet sequence
	u16		dev_id;
	u16		dev_type;
	u16     command_seq;
	u16		flags;   // host->iNIC: ENDIAN, 
               // iNIC->host: flags (wireless_send_event) 
	s32     status;
	u32     reserved[2];
	u8      data[0];
}  __attribute__((packed));



struct reg_str
{
	u32     address;
	u32     value;
} __attribute__((packed));

typedef struct arg_box
{
    uintptr_t arg1;
    uintptr_t arg2;
} ArgBox;


/* 
 * RaCfg frame Comand Type
 */

/* use in bootstrapping state */

#define RACFG_CMD_TYPE_PASSIVE_MASK  0x7FFF
#define RACFG_CMD_TYPE_RSP_FLAG      0x8000

/* 
 * Bootstrapping command group 
 */

/* command type */
/* protocol = 2880 */
#define RACFG_CMD_TYPE_ATE          0x0005
/* protocol = ffff */
#define RACFG_CMD_TYPE_BOOTSTRAP	0x0001
#define RACFG_CMD_TYPE_ASYNC        0x0005
#define RACFG_CMD_TYPE_COPY_TO_USER 0x0006
#define RACFG_CMD_TYPE_IWREQ_STRUC  0x0007
#define RACFG_CMD_TYPE_IOCTL_STATUS 0x0008
#define RACFG_CMD_TYPE_SYNC         0x0009
//#define RACFG_CMD_TYPE_MBSS         0x0009
#define RACFG_CMD_TYPE_IW_HANDLER   0x000a
#define RACFG_CMD_TYPE_TUNNEL		0x000b 
#define RACFG_CMD_TYPE_IGMP_TUNNEL	0x000c 
#ifdef WIDI_SUPPORT
#define RACFG_CMD_TYPE_WIDI_NOTIFY 	0x000e // for WIDI notify Tunnel
#endif

/* ASYNC FEEDBACK Command ID */
#define RACFG_CMD_CONSOLE               0x0000
#define RACFG_CMD_WSC_UPDATE_CFG        0x0001
#define RACFG_CMD_WIRELESS_SEND_EVENT   0x0002
#define RACFG_CMD_SEND_DAEMON_SIGNAL    0x0003
#define RACFG_CMD_HEART_BEAT            0x0004
#define RACFG_CMD_WIRELESS_SEND_EVENT2  0x0005
#define RACFG_CMD_EXT_EEPROM_UPDATE     0x0006 

/* SYNC FEEDBACK Command ID */
#define RACFG_CMD_GET_MAC             0x0000
#define RACFG_CMD_MBSS_OPEN           0x0001
#define RACFG_CMD_MBSS_CLOSE          0x0002
#define RACFG_CMD_WDS_OPEN            0x0003
#define RACFG_CMD_WDS_CLOSE           0x0004
#define RACFG_CMD_APCLI_OPEN          0x0005
#define RACFG_CMD_APCLI_CLOSE         0x0006
#ifdef MESH_SUPPORT
#define RACFG_CMD_MESH_OPEN           0x0007
#define RACFG_CMD_MESH_CLOSE          0x0008
#endif // MESH_SUPPORT //

/* IW HANDLER Command ID */
/* Must enumerate the Command ID from 0x0001 */
#define RACFG_CMD_GET_STATS           0x0001

/* command id */
#define RACFG_CMD_BOOT_NOTIFY		0x0001
#define RACFG_CMD_BOOT_INITCFG		0x0002
#define RACFG_CMD_BOOT_UPLOAD		0x0003
#define RACFG_CMD_BOOT_STARTUP		0x0004
#define RACFG_CMD_BOOT_REGWR		0x0005
#define RACFG_CMD_BOOT_REGRD		0x0006
#define RACFG_CMD_BOOT_RESET		0x0007

#define RACFG_CMD_ID_NONE			0x0000

#define RACFG_SIGUSR1 1
#define RACFG_SIGUSR2 2

#if WIRELESS_EXT <= 11
#ifndef SIOCDEVPRIVATE
#define SIOCDEVPRIVATE                              0x8BE0
#endif
#ifndef SIOCIWFIRSTPRIV
#define SIOCIWFIRSTPRIV								SIOCDEVPRIVATE
#endif
#endif
/* for AP */
#define RT_PRIV_IOCTL								(SIOCIWFIRSTPRIV + 0x01)
/* for STA */
#define RT_PRIV_STA_IOCTL							(SIOCIWFIRSTPRIV + 0x0E)


#define RTPRIV_IOCTL_SET							(SIOCIWFIRSTPRIV + 0x02)
#define RTPRIV_IOCTL_ATE							(SIOCIWFIRSTPRIV + 0x08)


#define RTPRIV_IOCTL_BBP                            (SIOCIWFIRSTPRIV + 0x03)
#define RTPRIV_IOCTL_MAC                            (SIOCIWFIRSTPRIV + 0x05)
#define RTPRIV_IOCTL_E2P                            (SIOCIWFIRSTPRIV + 0x07)

#define RTPRIV_IOCTL_STATISTICS                     (SIOCIWFIRSTPRIV + 0x09)
#define RTPRIV_IOCTL_ADD_PMKID_CACHE                (SIOCIWFIRSTPRIV + 0x0A)
#define RTPRIV_IOCTL_RADIUS_DATA                    (SIOCIWFIRSTPRIV + 0x0C)
#define RTPRIV_IOCTL_GSITESURVEY					(SIOCIWFIRSTPRIV + 0x0D)
/* for AP */
#define RTPRIV_IOCTL_ADD_WPA_KEY                    (SIOCIWFIRSTPRIV + 0x0E)
#define RTPRIV_IOCTL_GET_MAC_TABLE					(SIOCIWFIRSTPRIV + 0x0F)
#define RTPRIV_IOCTL_STATIC_WEP_COPY                (SIOCIWFIRSTPRIV + 0x10)

#define RTPRIV_IOCTL_SHOW							(SIOCIWFIRSTPRIV + 0x11)
#define RTPRIV_IOCTL_WSC_PROFILE                    (SIOCIWFIRSTPRIV + 0x12)
#define RTPRIV_IOCTL_QUERY_BATABLE                  (SIOCIWFIRSTPRIV + 0x16)

#define RTPRIV_IOCTL_MEM                            (SIOCIWFIRSTPRIV + 0x17)
#define RTPRIV_IOCTL_E2P_READ_W						(SIOCIWFIRSTPRIV + 0x19)
#define RTPRIV_IOCTL_RF								(SIOCIWFIRSTPRIV + 0x1b)
#define RTPRIV_IOCTL_SWITCH							(SIOCIWFIRSTPRIV + 0x1d)


#ifdef NM_SUPPORT
#define RTPRIV_IOCTL_RESET						    (SIOCIWFIRSTPRIV + 0x20)
#define RTPRIV_IOCTL_SHUTDOWN						(SIOCIWFIRSTPRIV + 0x21)
#define RTPRIV_IOCTL_RESTART					    (SIOCIWFIRSTPRIV + 0x22)
#endif



enum {    
    SHOW_CONN_STATUS = 4,
    SHOW_DRVIER_VERION = 5,
	SHOW_BA_INFO = 6,
    SHOW_DESC_INFO = 7,
	RAIO_OFF = 10,
    RAIO_ON = 11,
#ifdef MESH_SUPPORT
	SHOW_MESH_INFO = 12,
	SHOW_NEIGHINFO_INFO = 13,
	SHOW_MESH_ROUTE_INFO = 14,
	SHOW_MESH_ENTRY_INFO = 15,
	SHOW_MULPATH_INFO = 16,
	SHOW_MCAST_AGEOUT_INFO = 17,
	SHOW_MESH_PKTSIG_INFO = 18,
#endif // MESH_SUPPORT //
    SHOW_DLS_ENTRY_INFO = 19,
	SHOW_CFG_VALUE = 20,
	SHOW_ADHOC_ENTRY_INFO = 21,
#ifdef MESH_SUPPORT
	SHOW_MESH_PROXY_INFO = 22,
#endif // MESH_SUPPORT //
	SHOW_ACM_BADNWIDTH = 23,
	SHOW_ACM_STREAM = 24,
	SHOW_TDLS_ENTRY_INFO = 25,
};


#define RTPRIV_IOCTL_SET_WSC_PROFILE_U32_ITEM       (SIOCIWFIRSTPRIV + 0x14)
#define RTPRIV_IOCTL_SET_WSC_PROFILE_STRING_ITEM    (SIOCIWFIRSTPRIV + 0x16)

enum {    
	WSC_CREDENTIAL_COUNT = 1,
	WSC_CREDENTIAL_SSID = 2,
	WSC_CREDENTIAL_AUTH_MODE = 3,
	WSC_CREDENTIAL_ENCR_TYPE = 4,
	WSC_CREDENTIAL_KEY_INDEX = 5,
	WSC_CREDENTIAL_KEY = 6,
	WSC_CREDENTIAL_MAC = 7,	
	WSC_SET_DRIVER_CONNECT_BY_CREDENTIAL_IDX = 8,
	WSC_SET_DRIVER_AUTO_CONNECT = 9,
	WSC_SET_CONF_MODE = 10, // Enrollee or Registrar
	WSC_SET_MODE = 11,      // PIN or PBC
	WSC_SET_PIN = 12,
	WSC_SET_SSID = 13,
	WSC_START = 14,
	WSC_STOP = 15,
	WSC_GEN_PIN_CODE = 16,
    RACFG_CONTROL_RESET = 17,
    RACFG_CONTROL_SHUTDOWN = 18,
    RACFG_CONTROL_RESTART = 19,
    WSC_AP_BAND = 20,
	WSC_SET_BSSID = 21,
#ifdef WIDI_SUPPORT
	WSC_GEN_PIN_CODE_WIDI = 22,
#endif
};

#if (CONFIG_INF_TYPE == INIC_INF_TYPE_MII)
#ifdef PHASE_LOAD_CODE
#define GET_MAC_TIMEOUT    30   // 30 secs
#else
#define GET_MAC_TIMEOUT    40   // 40 secs
#endif
#else
#define GET_MAC_TIMEOUT    20   // 20 secs
#endif

#ifdef CONFIG_CONCURRENT_INIC_SUPPORT
#undef GET_MAC_TIMEOUT
#define GET_MAC_TIMEOUT    40   // 50 secs
#endif


#define HEART_BEAT_TIMEOUT    10   // 10 secs

#ifdef WOWLAN_SUPPORT
#define WOW_INBAND_TIMEOUT    2   // 2 secs

#define WOW_CPU_UP			 0
#define WOW_CPU_DOWN		 1 
#endif // #ifdef WOWLAN_SUPPORT // 

/* IOCTL param type */


#define DEV_TYPE_APCLI_FLAG 0x8000  // (1 << 15)
#define DEV_TYPE_WDS_FLAG   0x4000  // (1 << 14)
#ifdef MESH_SUPPORT
#define DEV_TYPE_MESH_FLAG  0x2000  // (1 << 13)
#endif // MESH_SUPPORT //

#ifdef CONFIG_CONCURRENT_INIC_SUPPORT
#define DEV_TYPE_CONCURRENT_FLAG  0x0100  // (1 << 8)
#endif // CONFIG_CONCURRENT_INIC_SUPPORT //

typedef enum _IW_TYPE
{
	IW_NAME_TYPE=0,
	IW_POINT_TYPE,
	IW_PARAM_TYPE,
	IW_FREQ_TYPE,
	IW_MODE_TYPE,
	IW_QUALITY_TYPE,
	IW_SOCKADDR_TYPE,
    IW_U32_TYPE,
} IW_TYPE;

typedef enum _SOURCE_TYPE {
    SOURCE_MBSS=0,
    SOURCE_WDS,
    SOURCE_APCLI
#ifdef MESH_SUPPORT
    ,SOURCE_MESH
#endif // MESH_SUPPORT //
} SOURCE_TYPE;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,1)
#define KERNEL_THREAD_BEGIN(name)      \
{                                      \
    daemonize();                       \
    reparent_to_init();                \
}
#else
#define KERNEL_THREAD_BEGIN(name)      \
{                                      \
    daemonize(name);                   \
    allow_signal(SIGKILL);             \
}
#endif


// for STA Supplicant
#define RT_OID_WE_VERSION_COMPILED                  0x0622
#define RT_OID_WPA_SUPPLICANT_SUPPORT               0x0621
#define OID_GET_SET_TOGGLE                          0x8000

#define OID_802_11_BSSID_LIST						0x0609

#ifdef WOWLAN_SUPPORT
#define RT_OID_802_11_WAKEUP_SYNC  					0x0820
#endif // WOWLAN_SUPPORT //

#define ROUND_UP(__x, __y) \
        (((uintptr_t)((__x)+((__y)-1))) & ((uintptr_t)~((__y)-1)))

#define NDIS_STATUS_SUCCESS                     0x00
#define NDIS_STATUS_FAILURE                     0x01

#ifdef NM_SUPPORT
void RaCfgShutdown(iNIC_PRIVATE *pAd);
void RaCfgStartup(iNIC_PRIVATE *pAd);
void RaCfgRestart(iNIC_PRIVATE *pAd);
#endif

void RaCfgWriteFile(iNIC_PRIVATE *pAd, FileHandle *fh, char *buff, int len);
void RaCfgOpenFile(iNIC_PRIVATE *pAd, FileHandle *fh, int flag);
void RaCfgCloseFile(iNIC_PRIVATE *pAd, FileHandle *fh);
void RaCfgInit(iNIC_PRIVATE *pAd, struct net_device *dev, char *conf_mac, char *conf_mode, int built_bridge, int chk_sum_off);
void RaCfgExit(iNIC_PRIVATE *pAd);
void RaCfgSetUp(iNIC_PRIVATE *pAd, struct net_device *dev);
void RaCfgStateReset(iNIC_PRIVATE *pAd);
int  RaCfgWaitSyncRsp(iNIC_PRIVATE *pAd, u16 cmd, u16 cmd_seq, struct iwreq *wrq, char *kernel_data, int secs);
void IoctlRspHandler(iNIC_PRIVATE *pAd, struct sk_buff *skb);
void FeedbackRspHandler(iNIC_PRIVATE *pAd, struct sk_buff *skb);
int RaCfgDropRemoteInBandFrame(struct sk_buff *skb);
void RaCfgInterfaceOpen(iNIC_PRIVATE *pAd);
void RaCfgInterfaceClose(iNIC_PRIVATE *pAd);



struct net_device *current_device(void);
int RTMPIoctlHandler(iNIC_PRIVATE *pAd, int cmd, IW_TYPE type, int dev_id, struct iwreq *wrq, char *kernel_data, boolean bIW_Handler);
int Set_ATE_Load_E2P_Wrapper(iNIC_PRIVATE *pAd, struct iwreq *wrq);
char * rtstrchr(const char * s, int c);
boolean ToLoadE2PBinFile(iNIC_PRIVATE *pAd, struct iwreq *wrq);
int  rlk_inic_ioctl(struct net_device *dev, struct ifreq *rq, int cmd);

boolean racfg_frame_handle(iNIC_PRIVATE *pAd, struct sk_buff *skb);
void hex_dump(char *str, unsigned char *pSrcBufVA, unsigned int SrcBufLen);
void insert_vlan_id(struct sk_buff  *skb, int vlan_id); 

unsigned char *IWREQencode(unsigned char *buf, struct iwreq *wrq, IW_TYPE type, char *kernel_data);
unsigned char *IWREQdecode(struct iwreq *wrq, unsigned char *buf, IW_TYPE type, char *kernel_data);
//unsigned char *IWHandlerencode(unsigned char *buf, struct iwreq *wrq, IW_TYPE type, char *extra);
//unsigned char *IWHandlerdecode(struct iwreq *wrq, unsigned char *buf, IW_TYPE type, char *extra, int command);

void SendRaCfgCommand(iNIC_PRIVATE *pAd, u16 cmd_type, u16 cmd_id, u16 len, u16 seq, u16 cmd_seq, u16 dev_type, u16 dev_id, u8 *data);
//void make_in_band_frame(unsigned char *p, unsigned short CommandType, unsigned short CommandID, unsigned short DevType, unsigned short DevID, unsigned short len, unsigned short CommandSeq);


void AtoH(char * src, unsigned char * dest, int destlen);
unsigned char BtoH(char ch);

int  rlk_inic_start_xmit(struct sk_buff *skb, struct net_device *dev);

boolean rlk_inic_read_profile(iNIC_PRIVATE *ad_p);

int  rlk_inic_mbss_close(iNIC_PRIVATE *ad_p);
void rlk_inic_mbss_remove(iNIC_PRIVATE *ad_p);
void rlk_inic_mbss_init (struct net_device *main_dev_p, iNIC_PRIVATE *ad_p); 
int MBSS_Find_DevID(struct net_device *main_dev, struct net_device *mbss_dev);
int APCLI_Find_DevID(struct net_device *main_dev, struct net_device *apcli_dev);
int WDS_Find_DevID(struct net_device *main_dev, struct net_device *wds_dev);
#ifdef MESH_SUPPORT
int MESH_Find_DevID(struct net_device *main_dev, struct net_device *mesh_dev);
#endif // MESH_SUPPORT //

struct sk_buff*  Insert_Vlan_Tag(iNIC_PRIVATE *rt, struct sk_buff *skb_p, int dev_id, SOURCE_TYPE dev_source);
unsigned char Remove_Vlan_Tag(iNIC_PRIVATE *rt, struct sk_buff *skb);


void rlk_inic_wds_init (struct net_device *main_dev_p, iNIC_PRIVATE *ad_p);
void rlk_inic_wds_remove(iNIC_PRIVATE *ad_p);
int rlk_inic_wds_close(iNIC_PRIVATE *ad_p);

void rlk_inic_apcli_init (struct net_device *main_dev_p, iNIC_PRIVATE *ad_p);
void rlk_inic_apcli_remove(iNIC_PRIVATE *ad_p);
int rlk_inic_apcli_close(iNIC_PRIVATE *ad_p);

#ifdef MESH_SUPPORT
void rlk_inic_mesh_init (struct net_device *main_dev_p, iNIC_PRIVATE *ad_p);
void rlk_inic_mesh_remove(iNIC_PRIVATE *ad_p);
int rlk_inic_mesh_close(iNIC_PRIVATE *ad_p);
#endif // MESH_SUPPORT //

int rlk_inic_open(struct net_device *dev);
int rlk_inic_close (struct net_device *dev);

void rlk_inic_mbss_restart(uintptr_t arg);
void rlk_inic_wds_restart(uintptr_t arg);
void rlk_inic_apcli_restart(uintptr_t arg);
#ifdef MESH_SUPPORT
void rlk_inic_mesh_restart(uintptr_t arg);
#endif // MESH_SUPPORT //

#ifdef MULTIPLE_CARD_SUPPORT
boolean CardInfoRead(iNIC_PRIVATE *pAd);
#endif

#ifdef CONFIG_CONCURRENT_INIC_SUPPORT
boolean ConcurrentCardInfoRead(void);
void DispatchAdapter(iNIC_PRIVATE **ppAd, struct sk_buff *skb);
void SetRadioOn(iNIC_PRIVATE *pAd, u8 Operation);
#endif // CONFIG_CONCURRENT_INIC_SUPPORT //

extern struct net_device *S_device;
extern unsigned char RaCfgCmdHdr[];
extern unsigned int debug_level, temp_debug_level;
extern char *mode, *mac;

#ifdef CONFIG_CONCURRENT_INIC_SUPPORT
extern char *mac2;
#endif // CONFIG_CONCURRENT_INIC_SUPPORT //

#ifdef DBG
extern char *root;
#endif
#if (CONFIG_INF_TYPE==INIC_INF_TYPE_MII)
extern char *miimaster;
extern int syncmiimac;
void IgmpTunnelRcvPkt(char *buff, int len, int src_port);
int IgmpTunnelSendPkt(struct net_device *net_dev, char *buff, int len);
#endif
#ifdef WIDI_SUPPORT
int WIDINotify(struct net_device *dev, char *buff, int len, unsigned short type);
#endif
extern int max_fw_upload;


#if WIRELESS_EXT >= 12
// This function will be called when query /proc
struct iw_statistics *rlk_inic_get_wireless_stats(struct net_device *net_dev);
#endif // WIRELESS_EXT //

#if (CONFIG_INF_TYPE==INIC_INF_TYPE_MII)
extern void rlk_inic_mii_init ( struct net_device *main_dev_p, iNIC_PRIVATE *ad_p);
extern void rlk_inic_mii_remove(iNIC_PRIVATE *ad_p);
extern void rlk_inic_init_hw (iNIC_PRIVATE *rt);
extern int rlk_inic_mii_xmit (struct sk_buff *skb, struct net_device *net);
#define rlk_inic_start_xmit rlk_inic_mii_xmit
#endif


#if (CONFIG_INF_TYPE==INIC_INF_TYPE_USB)
extern int rlk_inic_usb_xmit (struct sk_buff *skb, struct net_device *net);
#define rlk_inic_start_xmit rlk_inic_usb_xmit
#endif

static int inline is_fw_running(iNIC_PRIVATE *pAd)
{
//#ifdef NM_SUPPORT
   return pAd->RaCfgObj.bGetMac;
//#else
//   return netif_running(pAd->RaCfgObj.MBSSID[0].MSSIDDev);
//#endif
}

struct iw_handler_def* get_wireless_handler(void);

#ifdef NM_SUPPORT
static void inline force_net_running(struct net_device *netdev)
{
    // Network manager may issue this ioctl
    // to judge if wireless or not, even not ifup
#if (CONFIG_INF_TYPE==INIC_INF_TYPE_MII)
	iNIC_PRIVATE *pAd = netdev_priv(netdev);

	if (!netif_running(pAd->master))
		dev_change_flags(pAd->master, pAd->master->flags|IFF_UP|IFF_PROMISC);
#else
	if (!netif_running(netdev))
		dev_change_flags(netdev, netdev->flags|IFF_UP);
#endif // !INIC_INF_TYPE_MII
}
#endif
#endif /* __RACONFIG_H__*/
