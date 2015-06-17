#include "rlk_inic.h"

#if WIRELESS_EXT >= 12
static int GetStatsHandler(iNIC_PRIVATE *pAd, int cmd, int dev_id, char *p_iw_stats);


static int GetStatsHandler(
	iNIC_PRIVATE *pAd,
	int cmd,
	int dev_id,
	char *p_iw_stats)
{
    static unsigned short seq = 0;
	unsigned char buffer[100];/* could be smaller I guess */
	unsigned char *p = &buffer[0];
    unsigned char *payload;
	//IW_TYPE type = 0;/* dummy */

    p += sizeof(struct racfghdr);
    payload = p;

    SendRaCfgCommand(pAd, RACFG_CMD_TYPE_IW_HANDLER, cmd, p - payload, 0, seq, 0, dev_id, NULL);

    /*
	make_in_band_frame(buffer, RACFG_CMD_TYPE_IW_HANDLER, cmd, (unsigned short)type, dev_id, p - payload, seq);
    send_racfg_cmd(pAd, buffer, p - buffer);
    */

#if 0
	if (pAd->RaCfgObj.bDebugShow)
	{
		hex_dump("In-Band Msg", buffer, p-buffer);
	}
#endif
    return RaCfgWaitSyncRsp(pAd, cmd, seq++, NULL, p_iw_stats, 2);
}

// This function will be called when query /proc
struct iw_statistics *rlk_inic_get_wireless_stats(
    struct net_device *net_dev)
{
    int dev_id = 0, error = 1;	
	iNIC_PRIVATE *pAd = netdev_priv(net_dev);
	char iw_stats_buffer[100];
	char *p = iw_stats_buffer;
	

	/* sanity check */
	if (pAd == NULL)
	{
		return (struct iw_statistics *) NULL;
	}

    if (!is_fw_running(pAd))
        {
        printk("WARN: IOCTL get_wireless_stats ignored (interface not opened yet)\n");
        return (struct iw_statistics *) NULL;
    }
#ifdef NM_SUPPORT
	force_net_running(net_dev);
#endif
    error = GetStatsHandler(pAd, RACFG_CMD_GET_STATS, dev_id, iw_stats_buffer);


    if (!error)
	{
		memcpy(&(pAd->iw_stats.status), p, 2/* sizeof(pAd->iw_stats.status) */);
		p += 2/* sizeof(pAd->iw_stats.status) */;
		memcpy(&(pAd->iw_stats.qual), p, 4/* sizeof(pAd->iw_stats.qual) */);
		p += 4/* sizeof(pAd->iw_stats.qual) */;
		memcpy(&(pAd->iw_stats.discard), p, 20/* sizeof(pAd->iw_stats.discard) */);
		p += 20/* sizeof(pAd->iw_stats.discard) */;
		memcpy(&(pAd->iw_stats.miss), p, 4/* sizeof(pAd->iw_stats.miss) */);
		return &(pAd->iw_stats);
	}
	/* Not found */
	else
	{
		return (struct iw_statistics *) NULL;
	}
}
#endif /* WIRELESS_EXT >= 12 */

/*
This is required for LinEX2004/kernel2.6.7 to provide iwlist scanning function
*/
int
rt_ioctl_giwname_wrapper(struct net_device *dev,
						 struct iw_request_info *info,
						 char *name, char *extra)
{
	iNIC_PRIVATE *rt = netdev_priv(dev);
	struct iwreq wrq;
	int rc = 0, bss_index = 0, cmd = (int)(info->cmd);

	/* sanity check */
	if (rt == NULL)
	{
		return -ENETDOWN;
	}


	if (!is_fw_running(rt))
	{
	/**************************************
	* HAL (0.5.12) add issue SIOCGIWNAME 
	* to determine if wireless or ethernet, 
	* before iface is up!! 
	* HAL (0.5.8) only check sysfs has 
	* "wireless", ex:
	* /sys/class/net/ra0/wireless
	* which needs dev->wireless_handlers be
	* set correctly
	**************************************/
	if (cmd == SIOCGIWNAME)
	{
		return 0; // make HAL(0.5.12) happy
    }
        printk("WARN: IOCTL %04x ignored (interface not opened yet)\n", cmd);
        return -EINVAL;
    }

#ifdef NM_SUPPORT
	force_net_running(dev);
#endif
//	DBGPRINT("IOCTL::%s ==>\n", __FUNCTION__);

	ASSERT(extra == NULL);
	memset(&wrq, 0, sizeof(struct iwreq));

//	printk("IOCTL command: %04x\n", cmd);
	rc = RTMPIoctlHandler(rt, cmd, IW_NAME_TYPE, bss_index, &wrq, extra, TRUE);
	strncpy(name, wrq.u.name, IFNAMSIZ);    

	return rc;
}

int rt_ioctl_siwfreq_wrapper(struct net_device *dev,
							 struct iw_request_info *info,
							 struct iw_freq *freq, char *extra)
{
	iNIC_PRIVATE *rt = netdev_priv(dev);
	struct iwreq wrq;
	int rc = 0, bss_index = 0, cmd = (int)(info->cmd);

	/* sanity check */
	if (rt == NULL)
	{
		return -ENETDOWN;
	}        

    if (!is_fw_running(rt))
        {
        printk("WARN: IOCTL %04x ignored (interface not opened yet)\n", cmd);
            return -EINVAL;
        }
#ifdef NM_SUPPORT
	force_net_running(dev);
#endif
	//DBGPRINT("IOCTL::%s ==>\n", __FUNCTION__);

	ASSERT(extra == NULL);
	memset(&wrq, 0, sizeof(struct iwreq));
	memcpy(&(wrq.u.freq), freq, sizeof(struct iw_freq));

	//printk("IOCTL command: %04x\n", cmd);
	rc = RTMPIoctlHandler(rt, cmd, IW_FREQ_TYPE, bss_index, &wrq, extra, TRUE);

	return rc;
}

int rt_ioctl_giwfreq_wrapper(struct net_device *dev,
							 struct iw_request_info *info,
							 struct iw_freq *freq, char *extra)
{
	iNIC_PRIVATE *rt = netdev_priv(dev);
	struct iwreq wrq;
	int rc = 0, bss_index = 0, cmd = (int)(info->cmd);

	/* sanity check */
	if (rt == NULL)
	{
		return -ENETDOWN;
	}

    if (!is_fw_running(rt))
        {
        printk("WARN: IOCTL %04x ignored (interface not opened yet)\n", cmd);
            return -EINVAL;
        }
#ifdef NM_SUPPORT
	force_net_running(dev);
#endif
	//DBGPRINT("IOCTL::%s ==>\n", __FUNCTION__);

	ASSERT(extra == NULL);
	memset(&wrq, 0, sizeof(struct iwreq));

	//printk("IOCTL command: %04x\n", cmd);
	rc = RTMPIoctlHandler(rt, cmd, IW_FREQ_TYPE, bss_index, &wrq, extra, TRUE);
	memcpy(freq, &(wrq.u.freq), sizeof(struct iw_freq));    

	return rc;
}

int rt_ioctl_siwmode_wrapper(struct net_device *dev,
							 struct iw_request_info *info,
							 __u32 *mode, char *extra)
{
	iNIC_PRIVATE *rt = netdev_priv(dev);
	struct iwreq wrq;
	int rc = 0, bss_index = 0, cmd = (int)(info->cmd);

	/* sanity check */
	if (rt == NULL)
	{
		return -ENETDOWN;
	}


    if (!is_fw_running(rt))
    {
        printk("WARN: IOCTL %04x ignored (interface not opened yet)\n", cmd);
            return -EINVAL;
        }
#ifdef NM_SUPPORT
	force_net_running(dev);
#endif
	//DBGPRINT("IOCTL::%s ==>\n", __FUNCTION__);

	ASSERT(extra == NULL);
	memset(&wrq, 0, sizeof(struct iwreq));
	memcpy(&(wrq.u.mode), mode, 4);

	//printk("IOCTL command: %04x mode = %d\n", cmd, mode);
	rc = RTMPIoctlHandler(rt, cmd, IW_MODE_TYPE, bss_index, &wrq, extra, TRUE);

	return rc;
}

int rt_ioctl_siwu32_wrapper(struct net_device *dev,
							 struct iw_request_info *info,
							 __u32 *uwrq, char *extra)
{
	iNIC_PRIVATE *rt = netdev_priv(dev);
	int rc = 0, bss_index = 0, cmd = (int)(info->cmd);

    /* sanity check */
	if (rt == NULL)
	{
		return -ENETDOWN;
	}

    if (!is_fw_running(rt))
        {
        printk("WARN: IOCTL %04x ignored (interface not opened yet)\n", cmd);
            return -EINVAL;
        }
#ifdef NM_SUPPORT
	force_net_running(dev);
#endif
	ASSERT(extra != NULL);

#ifdef NM_SUPPORT
     switch( uwrq[0] )
    {
        case RACFG_CONTROL_RESET:
            // reset MII hardware GPIO
            RaCfgShutdown(rt);
            RaCfgStartup(rt);
            return 0;
        case RACFG_CONTROL_SHUTDOWN:
            RaCfgShutdown(rt);
            return 0;
        case RACFG_CONTROL_RESTART:
            RaCfgRestart(rt);
            return 0;
    } 
#endif

	//printk("IOCTL command: %04x subcmd = %d, value=%d\n", cmd, uwrq[0], uwrq[1]);
	rc = RTMPIoctlHandler(rt, cmd, IW_U32_TYPE, bss_index, (struct iwreq *)uwrq, extra, TRUE);
	return rc;
}


int rt_ioctl_giwmode_wrapper(struct net_device *dev,
							 struct iw_request_info *info,
							 __u32 *mode, char *extra)
{
	iNIC_PRIVATE *rt = netdev_priv(dev);
	struct iwreq wrq;
	int rc = 0, bss_index = 0, cmd = (int)(info->cmd);

	/* sanity check */
	if (rt == NULL)
	{
		return -ENETDOWN;
	}

    if (!is_fw_running(rt))
        {
        printk("WARN: IOCTL %04x ignored (interface not opened yet)\n", cmd);
            return -EINVAL;
        }
#ifdef NM_SUPPORT
	force_net_running(dev);
#endif
	//DBGPRINT("IOCTL::%s ==>\n", __FUNCTION__);

	ASSERT(extra == NULL);
	memset(&wrq, 0, sizeof(struct iwreq));

	//printk("IOCTL command: %04x\n", cmd);
	rc = RTMPIoctlHandler(rt, cmd, IW_MODE_TYPE, bss_index, &wrq, extra, TRUE);
	memcpy(mode, &(wrq.u.mode), 4); 

	return rc;
}

int rt_ioctl_siwap_wrapper(struct net_device *dev,
						   struct iw_request_info *info,
						   struct sockaddr *ap_addr, char *extra)
{
	iNIC_PRIVATE *rt = netdev_priv(dev);
	struct iwreq wrq;
	int rc = 0, bss_index = 0, cmd = (int)(info->cmd);

	/* sanity check */
	if (rt == NULL)
	{
		return -ENETDOWN;
	}

    if (!is_fw_running(rt))
        {
        printk("WARN: IOCTL %04x ignored (interface not opened yet)\n", cmd);
            return -EINVAL;
        }
#ifdef NM_SUPPORT
	force_net_running(dev);
#endif
	//DBGPRINT("IOCTL::%s ==>\n", __FUNCTION__);

	ASSERT(extra == NULL);
	memset(&wrq, 0, sizeof(struct iwreq));
	memcpy(&(wrq.u.addr), ap_addr, sizeof(struct sockaddr));

	//printk("IOCTL command: %04x\n", cmd);
	rc = RTMPIoctlHandler(rt, cmd, IW_SOCKADDR_TYPE, bss_index, &wrq, extra, TRUE);

	return rc;
}

int rt_ioctl_giwap_wrapper(struct net_device *dev,
						   struct iw_request_info *info,
						   struct sockaddr *ap_addr, char *extra)
{
	iNIC_PRIVATE *rt = netdev_priv(dev);
	struct iwreq wrq;
	int rc = 0, bss_index = 0, cmd = (int)(info->cmd);

	/* sanity check */
	if (rt == NULL)
	{
		return -ENETDOWN;
	}

    if (!is_fw_running(rt))
        {
        printk("WARN: IOCTL %04x ignored (interface not opened yet)\n", cmd);
            return -EINVAL;
        }
#ifdef NM_SUPPORT
	force_net_running(dev);
#endif
	//DBGPRINT("IOCTL::%s ==>\n", __FUNCTION__);

	ASSERT(extra == NULL);
	memset(&wrq, 0, sizeof(struct iwreq));

	//printk("IOCTL command: %04x\n", cmd);
	rc = RTMPIoctlHandler(rt, cmd, IW_SOCKADDR_TYPE, bss_index, &wrq, extra, TRUE);
	memcpy(ap_addr, &(wrq.u.addr), sizeof(struct sockaddr));

	return rc;
}

/* .token_size = sizeof(struct sockaddr) + sizeof(struct iw_quality) */
int rt_ioctl_giwaplist_wrapper(struct net_device *dev,
							  struct iw_request_info *info,
							  struct iw_point *data, char *extra)
{
	iNIC_PRIVATE *rt = netdev_priv(dev);
	struct iwreq wrq;
	int rc = 0, bss_index = 0, cmd = (int)(info->cmd);
    unsigned short token_size = sizeof(struct sockaddr) + sizeof(struct iw_quality);

	/* sanity check */
	if (rt == NULL)
	{
		return -ENETDOWN;
	}

    if (!is_fw_running(rt))
        {
        printk("WARN: IOCTL %04x ignored (interface not opened yet)\n", cmd);
            return -EINVAL;
        }
#ifdef NM_SUPPORT
	force_net_running(dev);
#endif
	//DBGPRINT(" ===> %s : data->length = %d\n", __FUNCTION__, data->length);

	ASSERT(extra != NULL);
	memset(&wrq, 0, sizeof(struct iwreq));

	wrq.u.data.flags = data->flags;

	//printk("IOCTL command: 0x%08x\n", cmd);
	rc = RTMPIoctlHandler(rt, cmd, IW_POINT_TYPE, bss_index, &wrq, extra, TRUE);

	/* We can not overwrite the pointer of data,*/
	/* so we just copy the length field and the flags field. */
	//memcpy(data, &(wrq.u.data), sizeof(struct iw_point));
	data->length = wrq.u.data.length / token_size; 
	data->flags = wrq.u.data.flags;

	//DBGPRINT(" <=== %s : data->length = %d\n", __FUNCTION__, data->length);

	return rc;
}

/* .token_size	= 1 */
int rt_ioctl_siwscan_wrapper(struct net_device *dev,
							  struct iw_request_info *info,
							  struct iw_point *data, char *extra)
{
	iNIC_PRIVATE *rt = netdev_priv(dev);
	struct iwreq wrq;
	int rc = 0, bss_index = 0, cmd = (int)(info->cmd), user_length = 0;

	/* sanity check */
	if (rt == NULL)
	{
		return -ENETDOWN;
	}

    if (!is_fw_running(rt))
        {
        printk("WARN: IOCTL %04x ignored (interface not opened yet)\n", cmd);
            return -EINVAL;
        }
#ifdef NM_SUPPORT
	force_net_running(dev);
#endif
	//DBGPRINT(" ===> %s : data->length = %d\n", __FUNCTION__, data->length);

	//ASSERT(extra != NULL);
	memset(&wrq, 0, sizeof(struct iwreq));
	memcpy(&(wrq.u.data), data, sizeof(struct iw_point));
	user_length = wrq.u.data.length;
	wrq.u.data.length = 0;
	//wrq.u.data.flags = data->flags;

	//printk("IOCTL command: 0x%08x\n", cmd);
	rc = RTMPIoctlHandler(rt, cmd, IW_POINT_TYPE, bss_index, &wrq, extra, TRUE);

	return rc;
}

/* .token_size	= 1 */
int rt_ioctl_giwscan_wrapper(struct net_device *dev,
							  struct iw_request_info *info,
							  struct iw_point *data, char *extra)
{
	iNIC_PRIVATE *rt = netdev_priv(dev);
	struct iwreq wrq;
	int rc = 0, bss_index = 0, cmd = (int)(info->cmd);

	/* sanity check */
	if (rt == NULL)
	{
		return -ENETDOWN;
	}

    if (!is_fw_running(rt))
        {
        printk("WARN: IOCTL %04x ignored (interface not opened yet)\n", cmd);
            return -EINVAL;
        }
#ifdef NM_SUPPORT
	force_net_running(dev);
#endif
	//printk(" ===> %s : data->length = %d\n", __FUNCTION__, data->length);

	ASSERT(extra != NULL);
	memset((char *)&wrq, 0x00, sizeof(struct iwreq));

	wrq.u.data.flags = data->flags;
	/* The length of scan size is placed in wrq.u.data.pointer. */	
	wrq.u.data.length = sizeof(u32);
	wrq.u.data.pointer = (u32*)&data->length;
	memcpy(extra, &data->length, sizeof(u32));		

	rc = RTMPIoctlHandler(rt, cmd, IW_POINT_TYPE, bss_index, &wrq, extra, TRUE);

	/* We can not overwrite the pointer of data,*/
	/* so we just copy the length field and the flags field. */

	data->length = wrq.u.data.length; 
	data->flags = wrq.u.data.flags;

	//printk(" <=== %s : data->length = %d\n", __FUNCTION__, data->length);

	return rc;
}

int rt_ioctl_siwpoint_wrapper(struct net_device *dev,
							  struct iw_request_info *info,
							  struct iw_point *data, char *extra)
{
	iNIC_PRIVATE *rt = netdev_priv(dev);
	struct iwreq wrq;
	int rc = 0, bss_index = 0, cmd = (int)(info->cmd);
	int dev_type_flag = 0;
	
#ifdef MESH_SUPPORT
	VIRTUAL_ADAPTER *pVirtualAd;
    
	if (dev->priv_flags == INT_MESH)
    {
		pVirtualAd = netdev_priv(dev);
		rt = netdev_priv(pVirtualAd->RtmpDev);
		
        dev_type_flag = DEV_TYPE_MESH_FLAG;
    }		
#endif // MESH_SUPPORT //

	/* sanity check */
	if (rt == NULL)
	{
		return -ENETDOWN;
	}

    if (!is_fw_running(rt))
        {
        printk("WARN: IOCTL %04x ignored (interface not opened yet)\n", cmd);
            return -EINVAL;
        }
#ifdef NM_SUPPORT
	force_net_running(dev);
#endif
	//DBGPRINT(" ===> %s : data->length = %d\n", __FUNCTION__, data->length);

	//ASSERT(extra != NULL);
	memset(&wrq, 0, sizeof(struct iwreq));
	memcpy(&(wrq.u.data), data, sizeof(struct iw_point));
	//wrq.u.data.flags = data->flags;

	//printk("IOCTL command: 0x%08x\n", cmd);
	if (!ToLoadE2PBinFile(rt, &wrq))
	{
		rc = RTMPIoctlHandler(rt, cmd, IW_POINT_TYPE | dev_type_flag, bss_index, &wrq, extra, TRUE);
	}
	/* Send new EEPROM content for iNIC to overwrite the existing one. */
	else
	{
		rc = RTMPIoctlHandler(rt, cmd, IW_POINT_TYPE | dev_type_flag, bss_index, &wrq, NULL, FALSE);
	}
	return rc;
}

int rt_ioctl_gsiwpoint_wrapper(struct net_device *dev,
							  struct iw_request_info *info,
							  struct iw_point *data, char *extra)
{
	iNIC_PRIVATE *rt = netdev_priv(dev);
	struct iwreq wrq;
	int rc = 0, bss_index = 0, cmd = (int)(info->cmd);

	/* sanity check */
	if (rt == NULL)
	{
		return -ENETDOWN;
	}

    if (!is_fw_running(rt))
        {
        printk("WARN: IOCTL %04x ignored (interface not opened yet)\n", cmd);
            return -EINVAL;
        }
#ifdef NM_SUPPORT
	force_net_running(dev);
#endif
//	DBGPRINT(" ===> %s : data->length = %d\n", __FUNCTION__, data->length);

	ASSERT(extra != NULL);
	memset(&wrq, 0, sizeof(struct iwreq));

	wrq.u.data.flags = data->flags;
	wrq.u.data.length = data->length;
	wrq.u.data.pointer = data->pointer;

	if((extra != NULL)&&(data->pointer != NULL))
	{
		memcpy(extra, data->pointer, data->length);

	}
	
	//printk("IOCTL command: 0x%08x\n", cmd);
	rc = RTMPIoctlHandler(rt, cmd, IW_POINT_TYPE, bss_index, &wrq, extra, TRUE);

	/* We can not overwrite the pointer of data,*/
	/* so we just copy the length field and the flags field. */
	//memcpy(data, &(wrq.u.data), sizeof(struct iw_point));
	data->length = wrq.u.data.length; 
	data->flags = wrq.u.data.flags;

	//DBGPRINT(" <=== %s : data->length = %d\n", __FUNCTION__, data->length);

	return rc;
}

int rt_ioctl_giwpoint_wrapper(struct net_device *dev,
							  struct iw_request_info *info,
							  struct iw_point *data, char *extra)
{
	iNIC_PRIVATE *rt = netdev_priv(dev);
	struct iwreq wrq;
	int rc = 0, bss_index = 0, cmd = (int)(info->cmd);
	int dev_type_flag = 0;

#ifdef MESH_SUPPORT
	VIRTUAL_ADAPTER *pVirtualAd;
    
	if (dev->priv_flags == INT_MESH)
    {
		pVirtualAd = netdev_priv(dev);
		rt = netdev_priv(pVirtualAd->RtmpDev);
		
        dev_type_flag = DEV_TYPE_MESH_FLAG;
    }		
#endif // MESH_SUPPORT //

	/* sanity check */
	if (rt == NULL)
	{
		return -ENETDOWN;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)
	if (cmd == SIOCGIWRANGE)
	{
        // struct iw_range has been re-defined, fields shifted.
	    struct iw_range *range = (struct iw_range *)extra;
	    range->we_version_compiled = 19;
	    range->we_version_source   = 19;
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,24)	    
	    range->scan_capa = IW_SCAN_CAPA_ESSID | IW_SCAN_CAPA_CHANNEL;
#endif	    
	    range->enc_capa = IW_ENC_CAPA_CIPHER_TKIP | IW_ENC_CAPA_CIPHER_CCMP | IW_ENC_CAPA_WPA | IW_ENC_CAPA_WPA2;
	    return 0;
	}
#endif // KERNEL_VERSION //

    if (!is_fw_running(rt))
        {
        printk("WARN: IOCTL %04x ignored (interface not opened yet)\n", cmd);
            return -EINVAL;
        }
#ifdef NM_SUPPORT
	force_net_running(dev);
#endif
	//DBGPRINT(" ===> %s : data->length = %d\n", __FUNCTION__, data->length);

	ASSERT(extra != NULL);
	memset(&wrq, 0, sizeof(struct iwreq));

	wrq.u.data.flags = data->flags;
	wrq.u.data.length = data->length;

	//printk("IOCTL command: 0x%08x, len=%d", cmd, data->length);
	rc = RTMPIoctlHandler(rt, cmd, IW_POINT_TYPE | dev_type_flag, bss_index, &wrq, extra, TRUE);

	/* We can not overwrite the pointer of data,*/
	/* so we just copy the length field and the flags field. */
	//memcpy(data, &(wrq.u.data), sizeof(struct iw_point));
	data->length = wrq.u.data.length; 
	data->flags = wrq.u.data.flags;

	//DBGPRINT(" <=== %s : data->length = %d\n", __FUNCTION__, data->length);

	return rc;
}

int rt_ioctl_siwparam_wrapper(struct net_device *dev,
							  struct iw_request_info *info,
							  struct iw_param *param, char *extra)
{
	iNIC_PRIVATE *rt = netdev_priv(dev);
	struct iwreq wrq;
	int rc = 0, bss_index = 0, cmd = (int)(info->cmd);

	/* sanity check */
	if (rt == NULL)
	{
		return -ENETDOWN;
	}
    if (!is_fw_running(rt))
        {
        printk("WARN: IOCTL %04x ignored (interface not opened yet)\n", cmd);
            return -EINVAL;
        }
#ifdef NM_SUPPORT
	force_net_running(dev);
#endif
	//DBGPRINT("IOCTL::%s ==>\n", __FUNCTION__);

	ASSERT(extra == NULL);
	memset(&wrq, 0, sizeof(struct iwreq));
	memcpy(&(wrq.u.param), param, sizeof(struct iw_param));

	//printk("IOCTL command: %04x\n", cmd);
	rc = RTMPIoctlHandler(rt, cmd, IW_PARAM_TYPE, bss_index, &wrq, extra, TRUE);

	return rc;
}

int rt_ioctl_giwparam_wrapper(struct net_device *dev,
							  struct iw_request_info *info,
							  struct iw_param *param, char *extra)
{
	iNIC_PRIVATE *rt = netdev_priv(dev);
	struct iwreq wrq;
	int rc = 0, bss_index = 0, cmd = (int)(info->cmd);

	/* sanity check */
	if (rt == NULL)
	{
		return -ENETDOWN;
	}
    if (!is_fw_running(rt))
        {
        printk("WARN: IOCTL %04x ignored (interface not opened yet)\n", cmd);
            return -EINVAL;
        }
#ifdef NM_SUPPORT
	force_net_running(dev);
#endif
	//DBGPRINT("IOCTL::%s ==>\n", __FUNCTION__);

	ASSERT(extra == NULL);
	memset(&wrq, 0, sizeof(struct iwreq));

	//printk("IOCTL command: %04x\n", cmd);
	rc = RTMPIoctlHandler(rt, cmd, IW_PARAM_TYPE, bss_index, &wrq, extra, TRUE);
	memcpy(param, &(wrq.u.param),  sizeof(struct iw_param));

	return rc;
}

#if WIRELESS_EXT > 17
int rt_ioctl_siwauth_wrapper(struct net_device *dev,
							 struct iw_request_info *info,
							 union iwreq_data *wrqu, char *extra)
{
	int rc = 0;
	struct iw_param *param = &wrqu->param;

	rc = rt_ioctl_siwparam_wrapper(dev, info, param, extra);

	return rc;
}
int rt_ioctl_giwauth_wrapper(struct net_device *dev,
							 struct iw_request_info *info,
							 union iwreq_data *wrqu, char *extra)
{
	int rc = 0;
	struct iw_param *param = &wrqu->param;

	rc = rt_ioctl_giwparam_wrapper(dev, info, param, extra);

	return rc;
}
/* peter 0919 : need to be test */
int rt_ioctl_siwencodeext_wrapper(struct net_device *dev,
								  struct iw_request_info *info,
								  union iwreq_data *wrqu,
								  char *extra)
{
	iNIC_PRIVATE *rt = netdev_priv(dev);
	struct iwreq wrq;
	int rc = 0, bss_index = 0, cmd = (int)(info->cmd);

	/* sanity check */
	if (rt == NULL)
	{
		return -ENETDOWN;
	}
    if (!is_fw_running(rt))
        {
        printk("WARN: IOCTL %04x ignored (interface not opened yet)\n", cmd);
            return -EINVAL;
        }
#ifdef NM_SUPPORT
	force_net_running(dev);
#endif
	//hex_dump("wrqu->encoding.pointer", wrqu->encoding.pointer, wrqu->encoding.length);// peter 0919
	//hex_dump("extra", extra, wrqu->encoding.length);// peter 0919

	//DBGPRINT(" ===> %s : wrqu->data.length = %d\n", __FUNCTION__, wrqu->data.length);

	ASSERT(extra != NULL);
	memset(&wrq, 0, sizeof(struct iwreq));
	memcpy(&(wrq.u.data), &(wrqu->data), sizeof(struct iw_point));

	//printk("IOCTL command: 0x%08x\n", cmd);
	rc = RTMPIoctlHandler(rt, cmd, IW_POINT_TYPE, bss_index, &wrq, extra, TRUE);

	return rc;
}

int rt_ioctl_giwencodeext_wrapper(struct net_device *dev,
								  struct iw_request_info *info,
								  union iwreq_data *wrqu,
								  char *extra)
{
	iNIC_PRIVATE *rt = netdev_priv(dev);
	struct iwreq wrq;
	int rc = 0, bss_index = 0, cmd = (int)(info->cmd);

	/* sanity check */
	if (rt == NULL)
	{
		return -ENETDOWN;
	}
    if (!is_fw_running(rt))
        {
        printk("WARN: IOCTL %04x ignored (interface not opened yet)\n", cmd);
            return -EINVAL;
        }
#ifdef NM_SUPPORT
	force_net_running(dev);
#endif
	//hex_dump("wrqu->encoding.pointer", wrqu->encoding.pointer, wrqu->encoding.length);// peter 0919
	//hex_dump("extra", extra, wrqu->encoding.length);// peter 0919

	//printk(" ===> %s : wrqu->data.length = %d\n", __FUNCTION__, wrqu->data.length);

	ASSERT(extra != NULL);
	memset(&wrq, 0, sizeof(struct iwreq));
	memcpy(&(wrq.u.data), &(wrqu->data), sizeof(struct iw_point));

	//printk("IOCTL command: 0x%08x\n", cmd);
	rc = RTMPIoctlHandler(rt, cmd, IW_POINT_TYPE, bss_index, &wrq, extra, TRUE);
    //printk(" <=== %s\n", __FUNCTION__);
	return rc;
}
int rt_ioctl_siwpmksa_wrapper(struct net_device *dev,
			   struct iw_request_info *info,
			   union iwreq_data *wrqu,
			   char *extra)
{
    iNIC_PRIVATE *rt = netdev_priv(dev);
	struct iwreq wrq;
	int rc = 0, bss_index = 0, cmd = (int)(info->cmd);
	/* sanity check */
	if (rt == NULL)
	{
		return -ENETDOWN;
	}
    if (!is_fw_running(rt))
        {
        printk("WARN: IOCTL %04x ignored (interface not opened yet)\n", cmd);
            return -EINVAL;
        }
#ifdef NM_SUPPORT
	force_net_running(dev);
#endif
	//hex_dump("wrqu->encoding.pointer", wrqu->encoding.pointer, wrqu->encoding.length);// peter 0919
	//hex_dump("extra", extra, wrqu->encoding.length);// peter 0919

	//printk(" ===> %s : wrqu->data.length = %d\n", __FUNCTION__, wrqu->data.length);

	ASSERT(extra != NULL);
	memset(&wrq, 0, sizeof(struct iwreq));
	memcpy(&(wrq.u.data), &(wrqu->data), sizeof(struct iw_point));

	//printk("IOCTL command: 0x%08x\n", cmd);
	rc = RTMPIoctlHandler(rt, cmd, IW_POINT_TYPE, bss_index, &wrq, extra, TRUE);
	//printk(" <=== %s \n", __FUNCTION__);

	return rc;

}
#endif // #if WIRELESS_EXT > 17 //
//#ifdef DBG
/* Both setting and getting information will be the case. */
/* However, the OS will view it as a GET cmd because it is an odd-numberd OID */
int rt_private_ioctl_bbp_wrapper(struct net_device *dev,
								 struct iw_request_info *info,
								 struct iw_point *data, char *extra)
{
	iNIC_PRIVATE *rt = netdev_priv(dev);
	struct iwreq wrq;
	int rc = 0, bss_index = 0, cmd = (int)(info->cmd);

	/* sanity check */
	if (rt == NULL)
	{
		return -ENETDOWN;
	}
    if (!is_fw_running(rt))
        {
        printk("WARN: IOCTL %04x ignored (interface not opened yet)\n", cmd);
            return -EINVAL;
        }
#ifdef NM_SUPPORT
	force_net_running(dev);
#endif
	ASSERT(extra != NULL);
	memset(&wrq, 0, sizeof(struct iwreq));
	/* write bbp */
	wrq.u.data.pointer = kmalloc(MAX_INI_BUFFER_SIZE, MEM_ALLOC_FLAG);

	if (wrq.u.data.pointer == NULL)
    {
        printk("ERROR: Cannot alloc iwreq data buffer.\n");
        return -ENOMEM;
    }

	memcpy(wrq.u.data.pointer, data->pointer, data->length);
	wrq.u.data.flags = data->flags;
	wrq.u.data.length = data->length;
	
	memcpy(extra, wrq.u.data.pointer, wrq.u.data.length);

	rc = RTMPIoctlHandler(rt, cmd, IW_POINT_TYPE, bss_index, &wrq, extra, TRUE);
	/* read bbp */
	data->length = wrq.u.data.length; 
	ASSERT((data->length < MAX_INI_BUFFER_SIZE) && (wrq.u.data.pointer != NULL));
    kfree((char *)wrq.u.data.pointer);
	return rc;
}

//#endif // DBG //
