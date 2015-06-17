#include "rlk_inic.h"
#include "iwhandler.h"

int SendFragmentPackets(
							  iNIC_PRIVATE *pAd, 
							  unsigned short cmd_type,
							  unsigned short cmd_id,
							  unsigned short cmd_seq,
							  unsigned short dev_type,
							  unsigned short dev_id,
							  unsigned char *msg, 
							  int total)
{
	unsigned short seq = 0;
	int len = 0;
	do
	{
		len = total;

		if (len > MAX_FEEDBACK_LEN)
		{
			//printk("fragment packet(%d), total=%d bytes\n", seq, total);
			len = MAX_FEEDBACK_LEN;
		}
		else if (len < 0)
		{
			len = 0;
		}

		SendRaCfgCommand(pAd, cmd_type, cmd_id, len, seq, cmd_seq, dev_type, dev_id, msg);

		msg += len;
		seq++;
		total -= MAX_FEEDBACK_LEN;
	} while (total >= 0);
	return 0;
}

int RTMPIoctlHandler(
					iNIC_PRIVATE *pAd, 
					int cmd,
					IW_TYPE type,
					int dev_id,
					struct iwreq *wrq,
					char *kernel_data,
					boolean bIW_Handler)
{
	static unsigned short seq = 0;
	unsigned char *buffer;
	unsigned char *p;
	//unsigned char *payload;
	unsigned short cmd_type = 0;
	int total;

#ifdef NM_SUPPORT

        //if (!netif_running(pAd->RaCfgObj.MBSSID[0].MSSIDDev))
        if (!is_fw_running(pAd)) // fw not running
        {
            //printk("WARNING! IOCTL %04x ignored (interface not opened yet)\n", cmd);
            switch (cmd)
            {
                case SIOCGIWRANGE:
                    {
                        struct iw_range *range = (struct iw_range *)kernel_data;
                        range->we_version_compiled = 19;
                        range->we_version_source   = 19;

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,24)

                        range->scan_capa = IW_SCAN_CAPA_ESSID | IW_SCAN_CAPA_CHANNEL;

#endif // KERNEL_VERSION  //
                    }
                    break;
                case SIOCSIWSCAN:
                    break;
            }
            return 0;
        }
#endif

    if (!wrq->u.data.pointer)
    {
        //printk("WARN! NULL iwreq buffer pointer, set buffer size=0.\n");
        wrq->u.data.length = 0;
    }


	total = wrq->u.data.length + sizeof(struct iwreq);
	buffer = kmalloc(total, MEM_ALLOC_FLAG);
	if (!buffer)
	{
		printk("ERROR: Cannot alloc (%d bytes) in band command packet.\n", total);
		return -ENOMEM;
	}

#if 0
	if (type == IW_POINT_TYPE &&  wrq->u.data.length > total)
	{
		printk("WARNING: IOCTL buffer size specified (%d) over limit (%d), truncated.\n",
			   wrq->u.data.length, total);

		wrq->u.data.length = total;
	}
#endif

	ASSERT(wrq != NULL);
	if (cmd == SIOCETHTOOL)
	{
		kfree(buffer);
		return -EOPNOTSUPP;  
	}

	p = buffer;
	//p += sizeof(struct racfghdr);
	// payload = p;
	// data

	// type: device type may be marked as APCLI flag, clear it before encode
#ifdef MESH_SUPPORT
	p = IWREQencode(p, wrq, type & ~(DEV_TYPE_WDS_FLAG | DEV_TYPE_APCLI_FLAG | DEV_TYPE_MESH_FLAG), kernel_data);
#else	
	p = IWREQencode(p, wrq, type & ~(DEV_TYPE_WDS_FLAG | DEV_TYPE_APCLI_FLAG), kernel_data);
#endif // MESH_SUPPORT //
	if (p == NULL)
	{
		kfree(buffer);
		return -EFAULT;
	}

	if (bIW_Handler == FALSE)
	{
		cmd_type = RACFG_CMD_TYPE_IWREQ_STRUC;
	}
	else
	{
		cmd_type =  RACFG_CMD_TYPE_IW_HANDLER;
	}

	SendFragmentPackets(pAd, cmd_type, cmd, seq, type, dev_id, buffer, (p - buffer));

#ifdef RETRY_PKT_SEND
	if (wrq->u.data.flags != fATE_LOAD_EEPROM)
	{
		int rspValue = 0;
		pAd->RaCfgObj.RPKTInfo.retry = InbandRetryCnt;

		do{
			rspValue = RaCfgWaitSyncRsp(pAd, cmd, seq++, wrq, kernel_data, 2);
			if(rspValue < 0){
				SendFragmentPackets(pAd, cmd_type, cmd, seq, type, dev_id, buffer, (p - buffer));
				pAd->RaCfgObj.RPKTInfo.retry--;
			}else{
				kfree(buffer);
				return rspValue;
			}
		}while(pAd->RaCfgObj.RPKTInfo.retry > 0);
	}
#endif

	kfree(buffer);

	if (wrq->u.data.flags != fATE_LOAD_EEPROM)
	{
		return RaCfgWaitSyncRsp(pAd, cmd, seq++, wrq, kernel_data, 2);
	}
	/* Wait longer for e2p loading to complete. */
	else
	{
		return RaCfgWaitSyncRsp(pAd, cmd, seq++, wrq, kernel_data, 3);
	}
}

/* 
	==========================================================================
	Description:
		Load and Write EEPROM from a binary file prepared in advance.
		
		Return:
			TRUE if all parameters are OK, FALSE otherwise
	==========================================================================
*/
int Set_ATE_Load_E2P_Wrapper(
							iNIC_PRIVATE *rt, 
							struct iwreq *wrq)
{
	boolean         ret = FALSE;
	unsigned char   *src;
	struct file     *srcf;
	s32             retval, orgfsuid, orgfsgid;
	mm_segment_t    orgfs;
	u8              WriteEEPROM[EEPROM_SIZE];
	UINT32          FileLength = 0;

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,28)
	struct cred *override_cred, *old_cred;
#endif

	DBGPRINT("===> %s\n", __FUNCTION__);

	src = (unsigned char *) EEPROM_BIN_FILE_PATH;

	/* zero the e2p buffer */
	memset(WriteEEPROM, 0x00, EEPROM_SIZE);

	/* save uid and gid used for filesystem access.
	** set user and group to 0 (root)
	*/
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,28)
	orgfsuid = current->fsuid;
	orgfsgid = current->fsgid;
	/* as root */
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

	do
	{
		/* open the bin file */
		srcf = filp_open(src, O_RDONLY, 0);

		if (IS_ERR(srcf))
		{
			printk("%s - Error %ld opening %s\n", __FUNCTION__, -PTR_ERR(srcf), src);
			break;
		}

		/* the object must have a read method */
		if ((srcf->f_op == NULL) || (srcf->f_op->read == NULL))
		{
			printk("%s - %s does not have a read method\n", __FUNCTION__, src);
			break;
		}

		/* read the firmware from the file *.bin */
		FileLength = srcf->f_op->read(srcf,
									  (u8 *)WriteEEPROM,
									  EEPROM_SIZE,
									  &srcf->f_pos);

		if (FileLength != EEPROM_SIZE)
		{
			printk("%s: error file length (=%d) in e2p.bin\n",
				   __FUNCTION__, FileLength);
			break;
		}
		else
		{
			/* replace the data in struct iwreq to the content of .bin file  */
			memcpy(wrq->u.data.pointer, WriteEEPROM, FileLength);
			wrq->u.data.length = FileLength;

			/* give it a special flags for iNIC to identify */
			wrq->u.data.flags = fATE_LOAD_EEPROM;
			ret = TRUE;
		}
		break;
	} while (FALSE);

	/* close firmware file */
	if (IS_ERR(srcf))
	{
		;
	}
	else
	{
		retval = filp_close(srcf, NULL);            
		if (retval)
		{
			DBGPRINT("--> Error %d closing %s\n", -retval, src);
		}
	}

	/* restore */
	set_fs(orgfs);
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,28)
	current->fsuid = orgfsuid;
	current->fsgid = orgfsgid;      
#else
	revert_creds(old_cred);
	put_cred(override_cred);
#endif
	DBGPRINT("<=== %s (ret=%d)\n", __FUNCTION__, ret);

	return ret;
}

char * rtstrchr(const char * s, int c)
{
	for (; *s != (char) c; ++s)
		if (*s == '\0')
			return NULL;
	return(char *) s;
}

boolean ToLoadE2PBinFile(
						iNIC_PRIVATE *rt, 
						struct iwreq *wrq)
{
	signed char *this_char = NULL;
	signed char *value;
	boolean ret = FALSE;

	if (wrq->u.data.length > 0)
	{
		this_char = kmalloc(wrq->u.data.length, MEM_ALLOC_FLAG);
	}

	/* series of sanity checking */
	if (this_char == NULL)
	{
		return FALSE;
	}

	if (!(copy_from_user(this_char, wrq->u.data.pointer, wrq->u.data.length)))
	{
		if (!*this_char)
		{
			goto fail;
		}
		if ((value = rtstrchr(this_char, '=')) != NULL)
		{
			*value++ = 0;
		}
		if (!value)
		{
			goto fail;
		}
		if (strcmp(this_char, "ATELDE2P") != 0)
		{
			goto fail;
		}
		if ((*value))
		{
			ret = Set_ATE_Load_E2P_Wrapper(rt, wrq);
		}
	}
		kfree(this_char);
	return ret;

fail:
    kfree(this_char);
	return FALSE;
}

int rlk_inic_ioctl (struct net_device *dev, struct ifreq *rq, int cmd)
{
	VIRTUAL_ADAPTER *pVirtualAd;
	iNIC_PRIVATE *rt;
	struct iwreq *wrq = (struct iwreq *) rq;
	int rc = 0, if_index = 0;
	int dev_type_flag = 0;

	ASSERT(rq != NULL);
	if (dev->priv_flags == INT_MAIN)
	{
		rt = netdev_priv(dev);
#if (CONFIG_INF_TYPE==INIC_INF_TYPE_MII)
        if (!is_fw_running(rt))
        {
            printk("WARN! IOCTL %04x ignored (interface not opened yet)\n", cmd);
            return 0;
        }
#endif
	}
	else
	{
		pVirtualAd = netdev_priv(dev);
		rt = netdev_priv(pVirtualAd->RtmpDev);
        if (dev->priv_flags == INT_MBSSID)
        {
            if_index = MBSS_Find_DevID(GET_PARENT(dev), dev);
        } else if (dev->priv_flags == INT_APCLI)
        {
            if_index = APCLI_Find_DevID(GET_PARENT(dev), dev);
            dev_type_flag = DEV_TYPE_APCLI_FLAG;
        } else if (dev->priv_flags == INT_WDS)
        {
            if_index = WDS_Find_DevID(GET_PARENT(dev), dev);
            dev_type_flag = DEV_TYPE_WDS_FLAG;
        }
#ifdef MESH_SUPPORT
		else if (dev->priv_flags == INT_MESH)
        {
            if_index = MESH_Find_DevID(GET_PARENT(dev), dev);
            dev_type_flag = DEV_TYPE_MESH_FLAG;
        }			
#endif // MESH_SUPPORT //
    } 

	if (rt == NULL)
	{
		/* if 1st open fail, rt will be free;
		   So the dev->priv will be NULL in 2rd open */
		return -ENETDOWN;
	}

	if (!is_fw_running(rt))
		return -EINVAL;

	if (access_ok(VERIFY_READ, wrq->u.data.pointer, wrq->u.data.length) != TRUE)
		return rc;


	switch (cmd)
	{
		/*
		case SIOCGIFHWADDR:
			memcpy(wrq->u.name, dev->dev_addr, 6);
			printk("%02x:%02x:%02x\n", dev->dev_addr[3], dev->dev_addr[4], dev->dev_addr[5]);
			break;
		*/
		
		case SIOCETHTOOL:
		case SIOCSIFFLAGS:
			rc = -EACCES;
			break;

		case SIOCSIWRATE:		//set default bit rate (bps)
			rc = RTMPIoctlHandler(rt, cmd, IW_PARAM_TYPE | dev_type_flag, if_index, wrq, NULL, FALSE);
			break;
		case SIOCGIWRTS:		//get RTS/CTS threshold (bytes)
		case SIOCSIWRTS:		//set RTS/CTS threshold (bytes)
		case SIOCGIWFRAG:		//get fragmentation thr (bytes)
		case SIOCSIWFRAG:		//set fragmentation thr (bytes)
		case SIOCGIWENCODE:		//get encoding token & mode
		case SIOCSIWENCODE:	 	//set encoding token & mode
		case SIOCGIWNWID: 		// get network id
		case SIOCSIWNWID: 		// set network id (the cell)
		//case SIOCSIWESSID:		//Set ESSID
		case SIOCSIWAP:	 		//set access point MAC addresses
		case SIOCSIWMODE:  		//set operation mode
		case SIOCGIWSENS:		//get sensitivity (dBm)
		case SIOCSIWSENS:		//set sensitivity (dBm)
		case SIOCGIWPOWER:		//get Power Management settings
		case SIOCSIWPOWER:		//set Power Management settings
		case SIOCGIWTXPOW:		//get transmit power (dBm)
		case SIOCSIWTXPOW:		//set transmit power (dBm)
		//case SIOCGIWRANGE:    //Get range of parameters
		case SIOCGIWRETRY:		//get retry limits and lifetime
		case SIOCSIWRETRY:		//set retry limits and lifetime
		case SIOCSIWPMKSA:      // set PMKSA
			rc = -EOPNOTSUPP;
			break;
		case SIOCGIFHWADDR:
			rc = RTMPIoctlHandler(rt, cmd, IW_NAME_TYPE | dev_type_flag, if_index, wrq, NULL, FALSE);
			break;
		case SIOCGIWNAME:
			rc = RTMPIoctlHandler(rt, cmd, IW_NAME_TYPE | dev_type_flag, if_index, wrq, NULL, FALSE);
			break;
		case SIOCSIWESSID:		//Set ESSID
		case SIOCGIWESSID:	//Get ESSID
			rc = RTMPIoctlHandler(rt, cmd, IW_POINT_TYPE | dev_type_flag, if_index, wrq, NULL, FALSE);
			break;
		case SIOCGIWFREQ: // get channel/frequency (Hz)
			rc = RTMPIoctlHandler(rt, cmd, IW_FREQ_TYPE | dev_type_flag, if_index, wrq, NULL, FALSE);
			break;
		case SIOCSIWFREQ: //set channel/frequency (Hz)
		case SIOCGIWNICKN:
		case SIOCSIWNICKN: //set node name/nickname
		case SIOCGIWRATE:  //get default bit rate (bps)
			rc = RTMPIoctlHandler(rt, cmd, IW_PARAM_TYPE | dev_type_flag, if_index, wrq, NULL, FALSE);
			break;
		case SIOCGIWAP:
			rc = RTMPIoctlHandler(rt, cmd, IW_SOCKADDR_TYPE | dev_type_flag, if_index, wrq, NULL, FALSE);
			break;
		case SIOCGIWMODE:  //get operation mode
			rc = RTMPIoctlHandler(rt, cmd, IW_MODE_TYPE | dev_type_flag, if_index, wrq, NULL, FALSE);
			break;
		case SIOCGIWRANGE:	//Get range of parameters
			rc = RTMPIoctlHandler(rt, cmd, IW_POINT_TYPE | dev_type_flag, if_index, wrq, NULL, FALSE);

				break;
		case RT_PRIV_IOCTL:
			{
				if (wrq->u.data.flags == OID_802_11_BSSID_LIST)/* == OID_802_11_BSSID_LIST */
				{ 
					u32 length;
					u32 *pdata;
					
					length = wrq->u.data.length;
					wrq->u.data.length = sizeof(u32);
					pdata = (u32 *) wrq->u.data.pointer;
					*pdata = length;
				}
				
				rc = RTMPIoctlHandler(rt, cmd, IW_POINT_TYPE | dev_type_flag, if_index, wrq, NULL, FALSE);
			}
			break;
		case SIOCGIWPRIV:
			if (wrq->u.data.pointer)
			{
				int table_size, entry_size;
				struct iw_priv_args *privtable_i;

				privtable_i = (struct iw_priv_args  *) ((rt->RaCfgObj.opmode) ? ap_privtab_i : sta_privtab_i);
				table_size = (rt->RaCfgObj.opmode) ? sizeof(ap_privtab_i) : sizeof(sta_privtab_i);
				entry_size = (rt->RaCfgObj.opmode) ? sizeof(ap_privtab_i[0]) : sizeof(sta_privtab_i[0]);

				if (access_ok(VERIFY_WRITE, wrq->u.data.pointer, table_size) != TRUE)
					break;
				wrq->u.data.length = table_size / entry_size;
				if (copy_to_user(wrq->u.data.pointer, privtable_i, table_size))
					rc = -EFAULT;
			}
			break;

			/* Let RT_PRIV_STA_IOCTL and RTPRIV_IOCTL_ADD_WPA_KEY go to iNIC by this way. */
			/* due to the two OIDs are numbered differently in AP and STA */
		default:
			/* Need to be confirmed that NULL case will never happened. */
			if ((rt->RaCfgObj.opmode != 1) && (cmd == RT_PRIV_STA_IOCTL))
			{

				if (wrq->u.data.flags == RT_OID_WE_VERSION_COMPILED)
				{
					u32 we_version_compiled;
					wrq->u.data.length = sizeof(u32);
					we_version_compiled = WIRELESS_EXT;
					printk("Wireless Ext Version = %d\n", we_version_compiled);
					return copy_to_user(wrq->u.data.pointer, &we_version_compiled, wrq->u.data.length);                                     
				}
                if (wrq->u.data.flags == (OID_GET_SET_TOGGLE|RT_OID_WPA_SUPPLICANT_SUPPORT))
                {
                    unsigned char wpa_supplicant_enable = 0;
                    int status;
                    if (wrq->u.data.length != sizeof(char))
                    {
                        printk("ERROR! RT_OID_WPA_SUPPLICANT_SUPPORT size invalid: %d\n", wrq->u.data.length);
                    } 

                    wrq->u.data.length = sizeof(char);
                    status = copy_from_user((char *)&wpa_supplicant_enable, wrq->u.data.pointer, wrq->u.data.length);   
                    printk("WpaSupplicantUp = %d\n", wpa_supplicant_enable);
                    rt->RaCfgObj.bWpaSupplicantUp = wpa_supplicant_enable;
                }
			}

			/* RT_OID_802_11_RESET_COUNTERS from boa will bring a NULL wrq->u.data.pointer */
			/* 0x8513 == RT_OID_802_11_RESET_COUNTERS | OID_GET_SET_TOGGLE */
			//if ((wrq->u.data.pointer) || (wrq->u.data.flags == 0x8513)) 
			{
				struct iw_priv_args *privtable_i;
				u32 length;
				u32 *pdata;

				privtable_i = (struct iw_priv_args  *) ((rt->RaCfgObj.opmode) ? ap_privtab_i : sta_privtab_i);

				if (access_ok(VERIFY_WRITE, wrq->u.data.pointer, sizeof(privtable_i)) != TRUE)
					break;

				/* for wpa_supplicant */
			
				/* if the buffer for fw to allocate is bigger than the user-space allocated from host driver,
             	* it may cause the specific program panic, so doing below sanity check.
             	* however, for backford compatibility, we need to double check the wrq->u.data.length.
             	* because, in old host driver, "wrq->u.data.length" was assigned to 0 intentionally
             	* to prevent tranxmit 8k more bytes from host drive, we put the actual length at the address of pointer only
             	*/  
				if (wrq->u.data.flags == OID_802_11_BSSID_LIST)/* == OID_802_11_BSSID_LIST */
				{
					length = wrq->u.data.length;
					wrq->u.data.length = sizeof(u32);
					pdata = (u32 *) wrq->u.data.pointer;
					*pdata = length;
				}
			 

				if (!ToLoadE2PBinFile(rt, wrq))
				{
					//rt->RaCfgObj.bDebugShow = 1;
					rc = RTMPIoctlHandler(rt, cmd, IW_POINT_TYPE | dev_type_flag, if_index, wrq, NULL, FALSE);
					//rt->RaCfgObj.bDebugShow = 0;
				}
				else
				{	/* The struct iw_req is given a special wrq->u.data.flags == fATE_LOAD_EEPROM(0x0C43)  
					 * for iNIC to identify it as a request to load e2p from binary file.
					 */
					rc = RTMPIoctlHandler(rt, cmd, IW_POINT_TYPE | dev_type_flag, 0/* == if_index */, wrq, NULL, FALSE);
				}
				break;

			}
			break;
	}

	return rc;
}

