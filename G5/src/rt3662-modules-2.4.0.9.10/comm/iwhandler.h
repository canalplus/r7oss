#ifndef __IW_HANDLER_H__
#define __IW_HANDLER_H__

#include "rlk_inic.h"

int rt_ioctl_giwname_wrapper(struct net_device *dev,
		   struct iw_request_info *info,
		   char *name, char *extra);
int rt_ioctl_siwfreq_wrapper(struct net_device *dev,
			struct iw_request_info *info,
			struct iw_freq *freq, char *extra);
int rt_ioctl_giwfreq_wrapper(struct net_device *dev,
		   struct iw_request_info *info,
		   struct iw_freq *freq, char *extra);
int rt_ioctl_siwmode_wrapper(struct net_device *dev,
		   struct iw_request_info *info,
		   __u32 *mode, char *extra);
int rt_ioctl_giwmode_wrapper(struct net_device *dev,
		   struct iw_request_info *info,
		   __u32 *mode, char *extra);
int rt_ioctl_giwaplist_wrapper(struct net_device *dev,
			struct iw_request_info *info,
			struct iw_point *data, char *extra);
int rt_ioctl_siwscan_wrapper(struct net_device *dev,
        	struct iw_request_info *info,
			struct iw_point *data, char *extra);
int rt_ioctl_gsiwpoint_wrapper(struct net_device *dev,
			 struct iw_request_info *info,
			 struct iw_point *data, char *extra);
int rt_ioctl_giwscan_wrapper(struct net_device *dev,
			struct iw_request_info *info,
			struct iw_point *data, char *extra);
int rt_ioctl_siwpoint_wrapper(struct net_device *dev,
			 struct iw_request_info *info,
			 struct iw_point *data, char *extra);
int rt_ioctl_giwpoint_wrapper(struct net_device *dev,
			 struct iw_request_info *info,
			 struct iw_point *data, char *extra);
int rt_ioctl_siwap_wrapper(struct net_device *dev,
		      struct iw_request_info *info,
		      struct sockaddr *ap_addr, char *extra);
int rt_ioctl_giwap_wrapper(struct net_device *dev,
		      struct iw_request_info *info,
		      struct sockaddr *ap_addr, char *extra);
int rt_ioctl_siwparam_wrapper(struct net_device *dev,
			struct iw_request_info *info,
			struct iw_param *param, char *extra);
int rt_ioctl_giwparam_wrapper(struct net_device *dev,
			struct iw_request_info *info,
			struct iw_param *param, char *extra);

int rt_ioctl_siwu32_wrapper(struct net_device *dev,
							 struct iw_request_info *info,
							 __u32 *uwrq, char *extra);

#if WIRELESS_EXT > 17
int rt_ioctl_siwauth_wrapper(struct net_device *dev,
			  struct iw_request_info *info,
			  union iwreq_data *wrqu, char *extra);
int rt_ioctl_giwauth_wrapper(struct net_device *dev,
			       struct iw_request_info *info,
			       union iwreq_data *wrqu, char *extra);
int rt_ioctl_siwencodeext_wrapper(struct net_device *dev,
			   struct iw_request_info *info,
			   union iwreq_data *wrqu,
			   char *extra);
int rt_ioctl_giwencodeext_wrapper(struct net_device *dev,
			   struct iw_request_info *info,
			   union iwreq_data *wrqu,
			   char *extra);
int rt_ioctl_siwpmksa_wrapper(struct net_device *dev,
			   struct iw_request_info *info,
			   union iwreq_data *wrqu,
			   char *extra);

#endif // #if WIRELESS_EXT > 17 //

int rt_private_ioctl_bbp_wrapper(struct net_device *dev,
			   struct iw_request_info *info,
		       struct iw_point *wrq, char *extra);
		       
struct iw_handler_def* get_wireless_handler(void);

const iw_handler rt_handler[] =
{
	(iw_handler) NULL,			            		/* SIOCSIWCOMMIT */
	(iw_handler) rt_ioctl_giwname_wrapper,			/* SIOCGIWNAME   */
	(iw_handler) NULL,  		            		/* SIOCSIWNWID   */
	(iw_handler) NULL,			            		/* SIOCGIWNWID   */
	(iw_handler) rt_ioctl_siwfreq_wrapper,		    /* SIOCSIWFREQ   */
	(iw_handler) rt_ioctl_giwfreq_wrapper,		    /* SIOCGIWFREQ   */
	(iw_handler) rt_ioctl_siwmode_wrapper,		    /* SIOCSIWMODE   */
	(iw_handler) rt_ioctl_giwmode_wrapper,		    /* SIOCGIWMODE   */
	(iw_handler) NULL,		                		/* SIOCSIWSENS   */
	(iw_handler) NULL,		                		/* SIOCGIWSENS   */
	(iw_handler) NULL /* not used */,				/* SIOCSIWRANGE  */
	(iw_handler) rt_ioctl_giwpoint_wrapper,		    /* SIOCGIWRANGE  */
	(iw_handler) NULL /* not used */,				/* SIOCSIWPRIV   */
	(iw_handler) NULL /* kernel code */,    		/* SIOCGIWPRIV   */
	(iw_handler) NULL /* not used */,				/* SIOCSIWSTATS  */
	(iw_handler) NULL /* kernel code */,    		/* SIOCGIWSTATS  */
	(iw_handler) NULL,		                		/* SIOCSIWSPY    */
	(iw_handler) NULL,		                		/* SIOCGIWSPY    */
	(iw_handler) NULL,				        		/* SIOCSIWTHRSPY */
	(iw_handler) NULL,				        		/* SIOCGIWTHRSPY */
	(iw_handler) rt_ioctl_siwap_wrapper,            /* SIOCSIWAP     */
	(iw_handler) rt_ioctl_giwap_wrapper,		    /* SIOCGIWAP     */
	(iw_handler) NULL,				        		/* SIOCSIWMLME   */
	(iw_handler) rt_ioctl_giwaplist_wrapper,	    /* SIOCGIWAPLIST */
#ifdef SIOCGIWSCAN
	(iw_handler) rt_ioctl_siwscan_wrapper,		    /* SIOCSIWSCAN   */
	(iw_handler) rt_ioctl_giwscan_wrapper,		    /* SIOCGIWSCAN   */
#else
	(iw_handler) NULL,				        		/* SIOCSIWSCAN   */
	(iw_handler) NULL,				        		/* SIOCGIWSCAN   */
#endif /* SIOCGIWSCAN */
	(iw_handler) rt_ioctl_siwpoint_wrapper,		    /* SIOCSIWESSID  */
	(iw_handler) rt_ioctl_giwpoint_wrapper,		    /* SIOCGIWESSID  */
	(iw_handler) rt_ioctl_siwpoint_wrapper,		    /* SIOCSIWNICKN  */
	(iw_handler) rt_ioctl_giwpoint_wrapper,		    /* SIOCGIWNICKN  */
	(iw_handler) NULL,				        		/* -- hole --    */
	(iw_handler) NULL,				        		/* -- hole --    */
	(iw_handler) NULL,		                		/* SIOCSIWRATE   */
	(iw_handler) NULL,		                		/* SIOCGIWRATE   */
	(iw_handler) rt_ioctl_siwparam_wrapper,		    /* SIOCSIWRTS    */
	(iw_handler) rt_ioctl_giwparam_wrapper,		    /* SIOCGIWRTS    */
	(iw_handler) rt_ioctl_siwparam_wrapper,		    /* SIOCSIWFRAG   */
	(iw_handler) rt_ioctl_giwparam_wrapper,		    /* SIOCGIWFRAG   */
	(iw_handler) NULL,		                		/* SIOCSIWTXPOW  */
	(iw_handler) NULL,		                		/* SIOCGIWTXPOW  */
	(iw_handler) NULL,		                		/* SIOCSIWRETRY  */
	(iw_handler) NULL,		                		/* SIOCGIWRETRY  */
	(iw_handler) rt_ioctl_siwpoint_wrapper,	    	/* SIOCSIWENCODE */
	(iw_handler) rt_ioctl_giwpoint_wrapper,	    	/* SIOCGIWENCODE */
	(iw_handler) NULL,		                		/* SIOCSIWPOWER  */
	(iw_handler) NULL,		                		/* SIOCGIWPOWER  */
	(iw_handler) NULL,								/* -- hole -- */	
	(iw_handler) NULL,								/* -- hole -- */
#if WIRELESS_EXT > 17	
    (iw_handler) rt_ioctl_siwpoint_wrapper,			/* SIOCSIWGENIE  */
	(iw_handler) rt_ioctl_giwpoint_wrapper,			/* SIOCGIWGENIE  */
	(iw_handler) rt_ioctl_siwauth_wrapper,		    /* SIOCSIWAUTH   */
	(iw_handler) rt_ioctl_giwauth_wrapper,		    /* SIOCGIWAUTH   */
	(iw_handler) rt_ioctl_siwencodeext_wrapper,	    /* SIOCSIWENCODEEXT */
	(iw_handler) rt_ioctl_giwencodeext_wrapper,	    /* SIOCGIWENCODEEXT */
	(iw_handler) rt_ioctl_siwpmksa_wrapper,			/* SIOCSIWPMKSA  */
#endif // #if WIRELESS_EXT > 17 //
};

#if 1
const iw_handler rt_priv_handlers[] = {
	(iw_handler) NULL, /* + 0x00 */
	(iw_handler) NULL, /* + 0x01 */
	(iw_handler) rt_ioctl_siwpoint_wrapper, /* + 0x02 */
//#ifdef DBG	
	(iw_handler) rt_private_ioctl_bbp_wrapper, /* + 0x03 */	
//#else
//	(iw_handler) NULL, /* + 0x03 */
//#endif
	(iw_handler) NULL, /* + 0x04 */
	(iw_handler) NULL, /* + 0x05 */
	(iw_handler) NULL, /* + 0x06 */
	(iw_handler) NULL, /* + 0x07 */
	(iw_handler) NULL, /* + 0x08 */
	(iw_handler) rt_ioctl_giwpoint_wrapper, /* + 0x09 */
	(iw_handler) NULL, /* + 0x0A */
	(iw_handler) NULL, /* + 0x0B */
	(iw_handler) NULL, /* + 0x0C */
	(iw_handler) NULL, /* + 0x0D */
	(iw_handler) NULL, /* + 0x0E */
	(iw_handler) NULL, /* + 0x0F */
	(iw_handler) NULL, /* + 0x10 */
	(iw_handler) rt_ioctl_gsiwpoint_wrapper, /* + 0x11 */
    (iw_handler) NULL, /* + 0x12 */
	(iw_handler) NULL, /* + 0x13 */
	(iw_handler) rt_ioctl_siwu32_wrapper, /* + 0x14 */
	(iw_handler) NULL, /* + 0x15 */
	(iw_handler) rt_ioctl_siwpoint_wrapper, /* + 0x16 */
	(iw_handler) NULL, /* + 0x17 */
	(iw_handler) rt_private_ioctl_bbp_wrapper, /* + 0x18 */
};
#else
const iw_handler rt_priv_handlers[] = {
	(iw_handler) NULL, /* + 0x00 */
	(iw_handler) NULL, /* + 0x01 */
	(iw_handler) rt_ioctl_setparam, /* + 0x02 */
//#ifdef DBG	
	(iw_handler) rt_private_ioctl_bbp, /* + 0x03 */	
//#else
//	(iw_handler) NULL, /* + 0x03 */
//#endif
	(iw_handler) NULL, /* + 0x04 */
	(iw_handler) NULL, /* + 0x05 */
	(iw_handler) NULL, /* + 0x06 */
	(iw_handler) NULL, /* + 0x07 */
	(iw_handler) NULL, /* + 0x08 */
	(iw_handler) rt_private_get_statistics, /* + 0x09 */
	(iw_handler) NULL, /* + 0x0A */
	(iw_handler) NULL, /* + 0x0B */
	(iw_handler) NULL, /* + 0x0C */
	(iw_handler) NULL, /* + 0x0D */
	(iw_handler) NULL, /* + 0x0E */
	(iw_handler) NULL, /* + 0x0F */
	(iw_handler) NULL, /* + 0x10 */
	(iw_handler) rt_private_show, /* + 0x11 */
    (iw_handler) NULL, /* + 0x12 */
	(iw_handler) NULL, /* + 0x13 */
#ifdef WSC_STA_SUPPORT	
	(iw_handler) rt_private_set_wsc_u32_item, /* + 0x14 */
#else
    (iw_handler) NULL, /* + 0x14 */
#endif // WSC_STA_SUPPORT //
	(iw_handler) NULL, /* + 0x15 */
#ifdef WSC_STA_SUPPORT	
	(iw_handler) rt_private_set_wsc_string_item, /* + 0x16 */
#else
    (iw_handler) NULL, /* + 0x16 */
#endif // WSC_STA_SUPPORT //
	(iw_handler) NULL, /* + 0x17 */
	(iw_handler) NULL, /* + 0x18 */
};
#endif // 1 //

const struct iw_priv_args ap_privtab_i[] =
{
	{
		.cmd = RTPRIV_IOCTL_SET,
		.set_args = IW_PRIV_TYPE_CHAR | 1024,
		.name = "set",
	},
	{
		.cmd = RTPRIV_IOCTL_SHOW,
		.set_args = IW_PRIV_TYPE_CHAR | 1024,
		.name = "show",
	},
	{
		.cmd = RTPRIV_IOCTL_GSITESURVEY,
		.set_args = IW_PRIV_TYPE_CHAR | 1024,
		.get_args = IW_PRIV_TYPE_CHAR | 1024 ,
		.name = "get_site_survey",
	},
	{
		.cmd = RTPRIV_IOCTL_GET_MAC_TABLE,
		.set_args = IW_PRIV_TYPE_CHAR | 1024,
		.get_args = IW_PRIV_TYPE_CHAR | 1024 ,
		.name = "get_mac_table",
	},
	{
		.cmd = RTPRIV_IOCTL_BBP,
		.set_args = IW_PRIV_TYPE_CHAR | 1024,
		.get_args = IW_PRIV_TYPE_CHAR | 1024,
		.name = "bbp",
	},
	{
		.cmd = RTPRIV_IOCTL_MAC,
		.set_args = IW_PRIV_TYPE_CHAR | 1024,
		.get_args = IW_PRIV_TYPE_CHAR | 1024,
		.name = "mac",
	},
	{
		.cmd = RTPRIV_IOCTL_MEM,
		.set_args = IW_PRIV_TYPE_CHAR | 1024,
		.get_args = IW_PRIV_TYPE_CHAR | 1024,
		.name = "mem",
	},
#if (CONFIG_INF_TYPE==INIC_INF_TYPE_MII)
	{
		.cmd = RTPRIV_IOCTL_SWITCH,
		.set_args = IW_PRIV_TYPE_CHAR | 1024,
		.get_args = IW_PRIV_TYPE_CHAR | 1024,
		.name = "switch",
	},
#endif 
	{
		.cmd = RTPRIV_IOCTL_RF,
		.set_args = IW_PRIV_TYPE_CHAR | 1024,
		.get_args = IW_PRIV_TYPE_CHAR | 1024,
		.name = "rf",
	},
	{
		.cmd = RTPRIV_IOCTL_E2P,
		.set_args = IW_PRIV_TYPE_CHAR | 1024,
		.get_args = IW_PRIV_TYPE_CHAR | 1024,
		.name = "e2p",
	},
	{
		.cmd = RTPRIV_IOCTL_E2P_READ_W,
		.set_args = IW_PRIV_TYPE_CHAR | 1024,
		.get_args = IW_PRIV_TYPE_CHAR | 1024,
		.name = "eeread_w",
	},
	{
		.cmd = RTPRIV_IOCTL_WSC_PROFILE,
		.set_args = IW_PRIV_TYPE_CHAR | 1024,
		.get_args = IW_PRIV_TYPE_CHAR | 1024 ,
		.name = "get_wsc_profile",
	},
	{
		.cmd = RTPRIV_IOCTL_QUERY_BATABLE,
		.set_args = IW_PRIV_TYPE_CHAR | 1024,
		.get_args = IW_PRIV_TYPE_CHAR | 1024 ,
		.name = "get_ba_table",
	},
	{
		.cmd = RTPRIV_IOCTL_STATISTICS,
		.set_args = IW_PRIV_TYPE_CHAR | 1024,
		.get_args = IW_PRIV_TYPE_CHAR | 1024,
		.name = "stat",
	},

};

struct iw_priv_args sta_privtab_i[] = {

	{
		.cmd = RTPRIV_IOCTL_SET,
		.set_args = IW_PRIV_TYPE_CHAR | 1024,
		.name = "set",
	},
	{
		.cmd = RTPRIV_IOCTL_SHOW,
		.get_args = IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK,
		.name = "",
	},
	{
		.cmd = RTPRIV_IOCTL_SHOW,
		.set_args = IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK,
		.get_args = IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK,
		.name = "",
	},
	{
		.cmd = SHOW_CONN_STATUS,
		.get_args = IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK,
		.name = "connStatus",
	},
	{
		.cmd = SHOW_DRVIER_VERION,
		.get_args = IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK,
		.name = "driverVer",
	},
	{
		.cmd = SHOW_BA_INFO,
		.get_args = IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK,
		.name = "bainfo",
	},
	{
		.cmd = SHOW_DESC_INFO,
		.get_args = IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK,
		.name = "descinfo",
	},
	{
		.cmd = RAIO_OFF,
		.get_args = IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK,
		.name = "radio_off",
	},
	{
		.cmd = RAIO_ON,
		.get_args = IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK,
		.name = "radio_on",
	},
	{
		.cmd = SHOW_DLS_ENTRY_INFO,
		.get_args = IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK,
		.name = "dlsentryinfo",
	},
	{
		.cmd = SHOW_ADHOC_ENTRY_INFO,
		.get_args = IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK,
		.name = "adhocEntry",
	},
	{
		.cmd = SHOW_CFG_VALUE,
		.set_args = IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK,
		.get_args = IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK,
		.name = "show",
	},
	{
		.cmd = SHOW_ACM_BADNWIDTH,
		.get_args = IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK,
		.name = "acmDisplay",
	},
#if 0
	{
		.cmd = SHOW_ACM_STREAM,
		.get_args = IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK,
		.name = "acmDisplayStream",
	},
#endif
	{
		.cmd = SHOW_TDLS_ENTRY_INFO,
		.get_args = IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK,
		.name = "tdlsentryinfo",
	},
	
/* --- sub-ioctls relations --- */
	{
		.cmd = RTPRIV_IOCTL_BBP,
		.set_args = IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK,
		.get_args = IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK,
		.name = "bbp",
	},
	{
		.cmd = RTPRIV_IOCTL_MAC,
		.set_args = IW_PRIV_TYPE_CHAR | 1024,
		.get_args = IW_PRIV_TYPE_CHAR | 1024,
		.name = "mac",
	},
	{
		.cmd = RTPRIV_IOCTL_E2P,
		.set_args = IW_PRIV_TYPE_CHAR | 1024,
		.get_args = IW_PRIV_TYPE_CHAR | 1024,
		.name = "e2p",
	},
	{
		.cmd = RTPRIV_IOCTL_MEM,
		.set_args = IW_PRIV_TYPE_CHAR | 1024,
		.get_args = IW_PRIV_TYPE_CHAR | 1024,
		.name = "mem",
	},
#if (CONFIG_INF_TYPE==INIC_INF_TYPE_MII)
	{
		.cmd = RTPRIV_IOCTL_SWITCH,
		.set_args = IW_PRIV_TYPE_CHAR | 1024,
		.get_args = IW_PRIV_TYPE_CHAR | 1024,
		.name = "switch",
	},
#endif
	{
		.cmd = RTPRIV_IOCTL_RF,
		.set_args = IW_PRIV_TYPE_CHAR | 1024,
		.get_args = IW_PRIV_TYPE_CHAR | 1024,
		.name = "rf",
	},
	{
		.cmd = RTPRIV_IOCTL_E2P_READ_W,
		.set_args = IW_PRIV_TYPE_CHAR | 1024,
		.get_args = IW_PRIV_TYPE_CHAR | 1024,
		.name = "eeread_w",
	},
	{
		.cmd = RTPRIV_IOCTL_STATISTICS,
		.get_args = IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK,
		.name = "stat",
	},
	{
		.cmd = RTPRIV_IOCTL_GSITESURVEY,
		.get_args = IW_PRIV_TYPE_CHAR | 1024,
		.name = "get_site_survey",
	},
	{
		.cmd = RTPRIV_IOCTL_SET_WSC_PROFILE_U32_ITEM,
		.set_args = IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		.name = "",
	},
	{
		.cmd = RTPRIV_IOCTL_SET_WSC_PROFILE_U32_ITEM,
		.set_args = IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 0,
		.name = "",
	},
	{
		.cmd = RTPRIV_IOCTL_SET_WSC_PROFILE_STRING_ITEM,
		.set_args = IW_PRIV_TYPE_CHAR | 128,
		.name = "",
	},
	{
		.cmd = WSC_CREDENTIAL_COUNT,
		.set_args = IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		.name = "wsc_cred_count",
	},
	{
		.cmd = WSC_CREDENTIAL_SSID,
		.set_args = IW_PRIV_TYPE_CHAR | 128,
		.name = "wsc_cred_ssid",
	},
	{
		.cmd = WSC_CREDENTIAL_AUTH_MODE,
		.set_args = IW_PRIV_TYPE_CHAR | 128,
		.name = "wsc_cred_auth",
	},
	{
		.cmd = WSC_CREDENTIAL_ENCR_TYPE,
		.set_args = IW_PRIV_TYPE_CHAR | 128,
		.name = "wsc_cred_encr",
	},
	{
		.cmd = WSC_CREDENTIAL_KEY_INDEX,
		.set_args = IW_PRIV_TYPE_CHAR | 128,
		.name = "wsc_cred_keyIdx",
	},
	{
		.cmd = WSC_CREDENTIAL_KEY,
		.set_args = IW_PRIV_TYPE_CHAR | 128,
		.name = "wsc_cred_key",
	},
	{
		.cmd = WSC_CREDENTIAL_MAC,
		.set_args = IW_PRIV_TYPE_CHAR | 128,
		.name = "wsc_cred_mac",
	},
	{
		.cmd = WSC_SET_DRIVER_CONNECT_BY_CREDENTIAL_IDX,
		.set_args = IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		.name = "wsc_conn_by_idx",
	},
	{
		.cmd = WSC_SET_DRIVER_AUTO_CONNECT,
		.set_args = IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		.name = "wsc_auto_conn",
	},
	{
		.cmd = WSC_SET_CONF_MODE,
		.set_args = IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		.name = "wsc_conf_mode",
	},
	{
		.cmd = WSC_SET_MODE,
		.set_args = IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
		.name = "wsc_mode",
	},
	{
		.cmd = WSC_SET_PIN,
		.set_args = IW_PRIV_TYPE_CHAR | 128,
		.name = "wsc_pin",
	},
	{
		.cmd = WSC_SET_SSID,
		.set_args = IW_PRIV_TYPE_CHAR | 128,
		.name = "wsc_ssid",
	},
	{
		.cmd = WSC_START,
		.set_args = IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 0,
		.name = "wsc_start",
	},
	{
		.cmd = WSC_STOP,
		.set_args = IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 0,
		.name = "wsc_stop",
	},
	{
		.cmd = WSC_GEN_PIN_CODE,
		.set_args = IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 0,
		.name = "wsc_gen_pincode",
	},
#ifdef NM_SUPPORT
	{
		.cmd = RACFG_CONTROL_RESET,
		.set_args = IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 0,
		.name = "reset",
	},
	{
		.cmd = RACFG_CONTROL_SHUTDOWN,
		.set_args = IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 0,
		.name = "shutdown",
	},
	{
		.cmd = RACFG_CONTROL_RESTART,
		.set_args = IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 0,
		.name = "restart",
	},
#endif
	{
		.cmd = WSC_AP_BAND,
		.set_args = IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 0,
		.name = "wsc_ap_band",
	},
	{
		.cmd = WSC_SET_BSSID,
		.set_args = IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 0,
		.name = "wsc_bssid",
	},
#ifdef WIDI_SUPPORT
	{
		.cmd = WSC_GEN_PIN_CODE_WIDI,
		.set_args = IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 0,
		.name = "wsc_widi_pincode",
	},
#endif
};

const struct iw_handler_def rlk_inic_iw_handler_def =
{
#define	N(a)	(sizeof (a) / sizeof (a[0]))
	.standard	= (iw_handler *) rt_handler,
	.num_standard	= sizeof(rt_handler) / sizeof(iw_handler),
	.private	= (iw_handler *) rt_priv_handlers,
	.num_private		= N(rt_priv_handlers),
	.private_args	= (struct iw_priv_args *) sta_privtab_i,
	.num_private_args	= N(sta_privtab_i),
#if IW_HANDLER_VERSION >= 7
    .get_wireless_stats = rlk_inic_get_wireless_stats,
#endif
#if WIRELESS_EXT > 15
//	.spy_offset	= offsetof(struct hostap_interface, spy_data),
#endif /* WIRELESS_EXT > 15 */
};

#ifdef MESH_SUPPORT
const iw_handler rt_mesh_priv_handlers[] = {
	(iw_handler) NULL, /* + 0x00 */
	(iw_handler) NULL, /* + 0x01 */
	(iw_handler) rt_ioctl_siwpoint_wrapper, /* + 0x02 */	
	(iw_handler) NULL, /* + 0x03 */	
	(iw_handler) NULL, /* + 0x04 */
	(iw_handler) NULL, /* + 0x05 */
	(iw_handler) NULL, /* + 0x06 */
	(iw_handler) NULL, /* + 0x07 */
	(iw_handler) NULL, /* + 0x08 */
	(iw_handler) NULL, /* + 0x09 */
	(iw_handler) NULL, /* + 0x0A */
	(iw_handler) NULL, /* + 0x0B */
	(iw_handler) NULL, /* + 0x0C */
	(iw_handler) NULL, /* + 0x0D */
	(iw_handler) NULL, /* + 0x0E */
	(iw_handler) NULL, /* + 0x0F */
	(iw_handler) NULL, /* + 0x10 */
	(iw_handler) rt_ioctl_giwpoint_wrapper, /* + 0x11 */
    (iw_handler) NULL, /* + 0x12 */
	(iw_handler) NULL, /* + 0x13 */
	(iw_handler) NULL, /* + 0x14 */
	(iw_handler) NULL, /* + 0x15 */
	(iw_handler) NULL, /* + 0x16 */
	(iw_handler) NULL, /* + 0x17 */
	(iw_handler) NULL, /* + 0x18 */
};

struct iw_priv_args sta_mesh_privtab_i[] = {
	{
		.cmd = RTPRIV_IOCTL_SET,
		.set_args = IW_PRIV_TYPE_CHAR | 1024,
		.name = "set",
	},
#if 0
	{
		.cmd = RTPRIV_IOCTL_SHOW,
		.get_args = IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK,
		.name = "",
	},
#endif
	{
		.cmd = RTPRIV_IOCTL_SHOW,
		.get_args = IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK,
		.name = "",
	},
	{
		.cmd = SHOW_MESH_INFO,
		.get_args = IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK,
		.name = "meshinfo",
	},
	{
		.cmd = SHOW_NEIGHINFO_INFO,
		.get_args = IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK,
		.name = "neighinfo",
	},
	{
		.cmd = SHOW_MESH_ROUTE_INFO,
		.get_args = IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK,
		.name = "meshrouteinfo",
	},
	{
		.cmd = SHOW_MESH_ENTRY_INFO,
		.get_args = IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK,
		.name = "meshentryinfo",
	},
	{
		.cmd = SHOW_MULPATH_INFO,
		.get_args = IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK,
		.name = "multipathinfo",
	},
	{
		.cmd = SHOW_MCAST_AGEOUT_INFO,
		.get_args = IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK,
		.name = "mcastageoutinfo",
	},
	{
		.cmd = SHOW_MESH_PKTSIG_INFO,
		.get_args = IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK,
		.name = "pktsiginfo",
	},
	{
		.cmd = SHOW_MESH_PROXY_INFO,
		.get_args = IW_PRIV_TYPE_CHAR | IW_PRIV_SIZE_MASK,
		.name = "meshproxyinfo",
	},
};

/* This handler only for Station Mesh */
const struct iw_handler_def mesh_iw_handler_def =
{
#define	N(a)	(sizeof (a) / sizeof (a[0]))
	.standard	= (iw_handler *) NULL,
	.num_standard	= 0,
	.private	= (iw_handler *) rt_mesh_priv_handlers,
	.num_private		= N(rt_mesh_priv_handlers),
	.private_args	= (struct iw_priv_args *) sta_mesh_privtab_i,
	.num_private_args	= N(sta_mesh_privtab_i),
#if IW_HANDLER_VERSION >= 7
    .get_wireless_stats = NULL,
#endif
};
#endif // MESH_SUPPORT //
#endif /* __IW_HANDLER_H__*/

