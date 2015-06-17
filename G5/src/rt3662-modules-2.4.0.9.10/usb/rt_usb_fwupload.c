#include "../comm/rlk_inic.h"

/*-------------------------------------------------------------------------*/
// USB Firmware Upgrade
//
/*-------------------------------------------------------------------------*/

extern int _append_extra_profile(iNIC_PRIVATE *pAd, int len);
extern void ReadCrcHeader(CRC_HEADER *hdr, unsigned char *buffer);

/* Firmware upgrade Functions */

#define RLK_INIC_MAX_EP1_BULK_OUT_SIZE 512

struct usb_device *rlk_inic_udev;
static struct semaphore *write_dat_file_semaphore;
static UINT32 BulkOutEpAddr,BulkOutMaxPacketSize;



void Start_fwupgrade_thread(iNIC_PRIVATE *pAd);

static void handle_crc_header(iNIC_PRIVATE *pAd, unsigned char *bulk_buf, int len, FileHandle *fh)
{
	unsigned char *last_hdr = 0;

	if (len >= CRC_HEADER_LEN)
	{
		last_hdr = bulk_buf + len - CRC_HEADER_LEN;
	}
	else
	{
		int slack = CRC_HEADER_LEN - len;
		unsigned char buff[CRC_HEADER_LEN];
		char *curr = buff;
		memcpy(curr, pAd->RaCfgObj.cmp_buf + RLK_INIC_MAX_EP1_BULK_OUT_SIZE - slack, slack);
		curr += slack;
		memcpy(curr, bulk_buf, len);
		last_hdr = buff;
	}

	if (len < RLK_INIC_MAX_EP1_BULK_OUT_SIZE)
	{
		memcpy(fh->hdr_src, last_hdr, CRC_HEADER_LEN);
		ReadCrcHeader(&fh->hdr, last_hdr);
		return;
	}

	memcpy(pAd->RaCfgObj.cmp_buf, bulk_buf, len);

}

static void rt_intr_complete (struct urb *urb)
{
	int		status = urb->status;

	if(status != 0)
		printk("callback fail=%d\n", status);
	else
	{
		up(write_dat_file_semaphore);
	}

}

static void RaCfgUpload(iNIC_PRIVATE *pAd, FileHandle *fh, int isconfig)
{

	int len=0, count = 0, extra = 0;
	struct urb		*pUrb = NULL;
	unsigned char *bulk_buf;
	dma_addr_t data_dma;
	struct file *fp = fh->fp;
	int Status = 0, fragment = 0;

	//printk("\nFunction RaCfgUpload =>get in \n");

	if (!fp) return;

	pUrb = RTUSB_ALLOC_URB(0);
	if (pUrb == NULL){
		printk("\n Allocate URB fail \n");
		return;
	}

	bulk_buf = RTUSB_URB_ALLOC_BUFFER(rlk_inic_udev, RLK_INIC_MAX_EP1_BULK_OUT_SIZE, &data_dma);
	if (bulk_buf == NULL){
		printk("\n Allocate bulk buffer fail \n");
		RTUSB_FREE_URB(pUrb);
		return;
	}


	if(isconfig){
		pAd->RaCfgObj.extraProfileOffset = 0;
		memset(pAd->RaCfgObj.test_pool, 0, MAX_FEEDBACK_LEN);

		extra = _append_extra_profile(pAd, 0);

		if(pAd->RaCfgObj.bExtEEPROM)
		{
			struct file *fpE2p = pAd->RaCfgObj.ext_eeprom.fp;
			int e2pLen;

			if (!fpE2p){
				printk("upload_profile Error : Can't read External EEPROM File\n");
				return;
			}
			e2pLen = fpE2p->f_op->read(fpE2p, pAd->RaCfgObj.test_pool + extra, MAX_FEEDBACK_LEN - extra, &fpE2p->f_pos);
			extra += e2pLen;
		}
	}

	while(1)
	{
		count++;
		if(count > 4000)
		{
			printk("break caused by file size > 2M\n");
			break;
		}

		memset(bulk_buf, 0x00, RLK_INIC_MAX_EP1_BULK_OUT_SIZE);
		len = fp->f_op->read(fp, bulk_buf, RLK_INIC_MAX_EP1_BULK_OUT_SIZE, &fp->f_pos);

		/* Config File Append */
		if(isconfig && (len < RLK_INIC_MAX_EP1_BULK_OUT_SIZE))
		{
			if(len || fragment)
			{
				if(fragment)
				{
					fragment = 0;
					len = extra-pAd->RaCfgObj.extraProfileOffset;
					// under verification
					if(len > RLK_INIC_MAX_EP1_BULK_OUT_SIZE)
					{
						fragment = 1;
						memcpy(bulk_buf, &pAd->RaCfgObj.test_pool[pAd->RaCfgObj.extraProfileOffset], RLK_INIC_MAX_EP1_BULK_OUT_SIZE);
						len = RLK_INIC_MAX_EP1_BULK_OUT_SIZE;
						pAd->RaCfgObj.extraProfileOffset += RLK_INIC_MAX_EP1_BULK_OUT_SIZE;
					}
					else
						memcpy(bulk_buf, &pAd->RaCfgObj.test_pool[pAd->RaCfgObj.extraProfileOffset], extra-pAd->RaCfgObj.extraProfileOffset);

				}
				else if((len + extra) <= RLK_INIC_MAX_EP1_BULK_OUT_SIZE)
				{
					memcpy((bulk_buf+len), pAd->RaCfgObj.test_pool, extra);
					len += extra;
				}
				else
				{
					fragment = 1;
					pAd->RaCfgObj.extraProfileOffset = (RLK_INIC_MAX_EP1_BULK_OUT_SIZE - len);
					memcpy((bulk_buf+len), pAd->RaCfgObj.test_pool, pAd->RaCfgObj.extraProfileOffset);
					len = RLK_INIC_MAX_EP1_BULK_OUT_SIZE;
				}
			}
		}


		if (len > 0)
		{

			//Initialize a tx bulk urb
			usb_fill_bulk_urb(pUrb,rlk_inic_udev,
					usb_sndbulkpipe(rlk_inic_udev, BulkOutEpAddr),
					bulk_buf,
					len,
					rt_intr_complete,
					NULL);


#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
			pUrb->transfer_dma = data_dma;
			pUrb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
#endif
			if(RTUSB_SUBMIT_URB(pUrb) != 0)
			{
				break;
			}


			Status = down_interruptible(write_dat_file_semaphore);
			if(Status != 0)
			{
				printk("down interrupt error\n");
				break;
			}

			if(!isconfig)
				handle_crc_header(pAd, bulk_buf, len, fh);

		}
		else
		{
			//printk("\nFunction RaCfgUpload => len is <0");
			break;
		}

#ifdef RLK_INIC_USBDEV_GEN2
		if(len == RLK_INIC_MAX_EP1_BULK_OUT_SIZE){
			memset(bulk_buf, 0x00, 512);
			len = 1;
			//Initialize a tx bulk urb
			usb_fill_bulk_urb(pUrb,
					rlk_inic_udev,
					usb_sndbulkpipe(rlk_inic_udev, BulkOutEpAddr),
					bulk_buf,
					len,
					rt_intr_complete,
					NULL);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
			pUrb->transfer_dma = data_dma;
			pUrb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
#endif

			if(RTUSB_SUBMIT_URB(pUrb) != 0)
			{
				printk("\n Submit Tx URB failed\n");
				break;
			}


			Status = down_interruptible(write_dat_file_semaphore);
			if(Status != 0){
				printk("down interrupt error\n");
				break;
			}

		}

#endif // RLK_INIC_USBDEV_GEN2

	}

	if(!isconfig){
		printk("\nSend %s Firmware Done\n", DRV_NAME);
		printk("===================================\n");
		printk("version: %s\n", fh->hdr.version);
		printk("size:    %d bytes\n", fh->hdr.size);
		printk("date:    %s\n", fh->hdr.date);
		printk("===================================\n\n");
	}

	RaCfgCloseFile(pAd, fh);
	if (pAd->RaCfgObj.bExtEEPROM)
		RaCfgCloseFile(pAd, &pAd->RaCfgObj.ext_eeprom);

	RTUSB_URB_FREE_BUFFER(rlk_inic_udev, RLK_INIC_MAX_EP1_BULK_OUT_SIZE, bulk_buf, data_dma);
	RTUSB_FREE_URB(pUrb);


}


VOID RTMPusecDelay(
		IN	ULONG	usec)
{
	ULONG	i;

	for (i = 0; i < (usec / 50); i++)
		udelay(50);

	if (usec % 50)
		udelay(usec % 50);
}

NTSTATUS    RLK_INICUSB_ControlPipeRequest(
		IN	UCHAR			RequestType,
		IN	UCHAR			Request,
		IN	USHORT			Value,
		IN	USHORT			Index,
		IN	PVOID			TransferBuffer,
		IN	UINT32			TransferBufferLength)
{
	int				ret;

	if (in_interrupt())
	{
		DBGPRINT("in_interrupt, RTUSB_VendorRequest Request%02x Value%04x Offset%04x\n",Request,Value,Index);
		return -1;
	}
	else
	{
#define MAX_RETRY_COUNT  10

		int retryCount = 0;

		// Acquire Control token
		do {
			if( RequestType == DEVICE_VENDOR_REQUEST_OUT)
			{
				ret=usb_control_msg(rlk_inic_udev, usb_sndctrlpipe(rlk_inic_udev,0 ), Request, RequestType, Value,Index, TransferBuffer, TransferBufferLength, CONTROL_TIMEOUT_JIFFIES);
			}
			else if(RequestType == DEVICE_VENDOR_REQUEST_IN)
			{
				ret=usb_control_msg(rlk_inic_udev, usb_rcvctrlpipe( rlk_inic_udev, 0 ), Request, RequestType, Value,Index, TransferBuffer, TransferBufferLength, CONTROL_TIMEOUT_JIFFIES);
			}
			else
			{
				DBGPRINT("vendor request direction is failed\n");
				ret = -1;
			}

			retryCount++;
			if (ret < 0)
				RTMPusecDelay(5000);

		} while((ret < 0) &&  (retryCount < MAX_RETRY_COUNT));

		if (ret < 0)
		{
			DBGPRINT("RTUSB_VendorRequest failed(%d), ReqType=%s, Req=0x%x, Index=0x%x\n",
					ret, (RequestType == DEVICE_VENDOR_REQUEST_OUT ? "OUT" : "IN"), Request, Index);
			if (Request == 0x2)
				DBGPRINT("\tRequest Value=0x%04x!\n", Value);

			//				if ((TransferBuffer!= NULL) && (TransferBufferLength > 0))
			//					hex_dump("Failed TransferBuffer value", TransferBuffer, TransferBufferLength);
		}
		//else
			//printk("\n ret>=0 => controlpipe ok\n");

	}

	return ret;
}


NTSTATUS RLK_INIC_USB_iNIC_CmdIN(UINT8 *pData,int request)
{
	memset(pData,0,64);
	flush_cache_all();
#ifdef RLK_INIC_USBDEV_GEN2
	if(request == RACFG_CMD_BOOT_STARTUP ){
		/* Turn ON watchdog, and START UP */
		return(RLK_INICUSB_ControlPipeRequest(DEVICE_VENDOR_REQUEST_IN,
				request,
				60,
				0,
				pData,
				64));

	}
#endif // RLK_INIC_USBDEV_GEN2

	/* START UP without watchdog */
	return(RLK_INICUSB_ControlPipeRequest(DEVICE_VENDOR_REQUEST_IN,
			request,
			0,
			0,
			pData,
			64));

}

int rlk_inic_racfg(UINT *pData,UINT32 request)
{
	int i=0,ret = 0;

	while(1)
	{
		ret = RLK_INIC_USB_iNIC_CmdIN((UINT8 *)pData,request);
		if(ret < 0){
			return ret;
		}
		else	{
			pData[0] = le32_to_cpu(pData[0]);
			if( pData[0] == (UINT8)request)
				break;
		}

		i++;
		if( i > 9){
			return -1;
		}
	}

	return ret;
}


BOOLEAN RLK_INICProbePostConfig(
		IN void 				*_dev_p,
		IN INT32				interface)
{
	iNIC_PRIVATE *pAd = (iNIC_PRIVATE *)_dev_p;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,5,0)
	struct usb_device *xdev = pAd->sbdev.udev;
	int conf_num;

	BulkOutEpAddr = 0;

	/* There should be only one endpoint (BULK_OUT) in FW upgrade stage (RLK_INIC)*/
	for ( conf_num = 0; conf_num < xdev->descriptor.bNumConfigurations; conf_num++ ) {
		struct usb_config_descriptor *conf = NULL;
		int intf_num;

		conf = &(xdev->config[conf_num]);
		for ( intf_num = 0; intf_num < conf->bNumInterfaces; intf_num++ ) {
			struct usb_interface *comm_intf_group = NULL;
			int altset_num;

			comm_intf_group = &( conf->interface[intf_num] );
			for ( altset_num = 0; altset_num < comm_intf_group->num_altsetting; altset_num++ ) {
				struct usb_interface_descriptor *comm_intf = NULL;
				int ep_num;

				comm_intf = &( comm_intf_group->altsetting[altset_num] );
				for ( ep_num = 0; ep_num < comm_intf->bNumEndpoints; ep_num++ ) {
					if(((comm_intf->endpoint[ep_num].bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_OUT) && (comm_intf->endpoint[ep_num].bmAttributes == USB_ENDPOINT_XFER_BULK)){
						BulkOutEpAddr = comm_intf->endpoint[ep_num].bEndpointAddress;
						BulkOutMaxPacketSize =comm_intf->endpoint[ep_num].wMaxPacketSize;
						goto found;
					}
				}
			}
		}
	}

	found:

#else
	struct usb_host_interface *iface_desc;
	struct usb_interface *intf = pAd->sbdev.intf;
	UINT32 i,NumberOfPipes;

	iface_desc = intf->cur_altsetting;
	/* get # of enpoints  */
	NumberOfPipes = iface_desc->desc.bNumEndpoints;
	BulkOutEpAddr = 0;

	for(i=0; i < NumberOfPipes; i++)	{
		if ((iface_desc->endpoint[i].desc.bmAttributes == USB_ENDPOINT_XFER_BULK) && ((iface_desc->endpoint[i].desc.bEndpointAddress & USB_ENDPOINT_DIR_MASK) == USB_DIR_OUT))
		{
			BulkOutEpAddr = iface_desc->endpoint[i].desc.bEndpointAddress;
			BulkOutMaxPacketSize = iface_desc->endpoint[i].desc.wMaxPacketSize;
			break;
		}
	}
#endif

	if (BulkOutEpAddr)
	{
		BulkOutMaxPacketSize = le16_to_cpu(BulkOutMaxPacketSize);
		//printk("BULK OUT MaximumPacketSize = 0x%04x\n", BulkOutMaxPacketSize);
		//printk("EP address = 0x%02x  \n", BulkOutEpAddr);

	}else{
		printk("%s: Could not find bulk-out endpoints\n", __FUNCTION__);
		return FALSE;
	}

	return TRUE;
}


int	NICLoadFirmware(iNIC_PRIVATE *pAd)
{
	unsigned char usb_ep0_pData[64]={0};
	int ret;


	//hex_dump("start of NICLoadFirmware", usb_ep0_pData, 64);

	if(pAd == NULL)
		return -1;

	ret = rlk_inic_racfg((UINT *)usb_ep0_pData,RACFG_CMD_BOOT_NOTIFY);
	//hex_dump("NICLoadFirmware after rlk_inic_racfg ", usb_ep0_pData, 64);

	if(ret <0)
	{
		printk("\n!! The %s not respond !!  --> RACFG_CMD_BOOT_NOTIFY\n", DRV_NAME);
		goto err_out;
	}
	else
	{
		printk("\n The RLK iNIC card is live \n");
	}

	/* Create kernel thread to upgrade f/w to device */
	Start_fwupgrade_thread(pAd);

#if 0 /* BOOT_STARTUP when interface open */
	ret = rlk_inic_racfg((UINT *)usb_ep0_pData,RACFG_CMD_BOOT_STARTUP);

	if(ret <0)
	{
		printk("\n The %s not respond !! --> RACFG_CMD_BOOT_STARTUP\n", DRV_NAME);
		goto err_out;
	}
#endif

	return 0;

	err_out:

	return -1;

}

/***************************************************/
//
//	 f/w upgrade Thread
//
/**************************************************/


void fw_thread_init(PUCHAR pThreadName, PVOID pNotify)
{

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
	daemonize(pThreadName /*"%s",pAd->net_dev->name*/);

	allow_signal(SIGTERM);
	allow_signal(SIGKILL);
	current->flags |= PF_NOFREEZE;
#else
	unsigned long flags;

	daemonize();
	reparent_to_init();
	strcpy(current->comm, pThreadName);

	siginitsetinv(&current->blocked, sigmask(SIGTERM) | sigmask(SIGKILL));

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,4,22)
	/* Allow interception of SIGKILL only
	 * Don't allow other signals to interrupt the transmission */
	spin_lock_irqsave(&current->sigmask_lock, flags);
	flush_signals(current);
	recalc_sigpending(current);
	spin_unlock_irqrestore(&current->sigmask_lock, flags);
#endif
#endif


#if 0 /* Continue main program when thread complete (f/w upgrade complete) */
	/* signal that we've started the thread */
	complete(pNotify);
#endif

}


void fw_upgrade_to_dev(iNIC_PRIVATE *pAd)
{
	unsigned char usb_ep0_pData[64]={0};
	int ret;


	//	set_current_state(TASK_UNINTERRUPTIBLE);
	//hex_dump("start fw_upgrade_to_dev", usb_ep0_pData, 64);
	if (!pAd->RaCfgObj.opmode == 0)
	{
		RLK_STRCPY(pAd->RaCfgObj.firmware.name, INIC_AP_FIRMWARE_PATH);
		RLK_STRCPY(pAd->RaCfgObj.profile.name,  INIC_AP_PROFILE_PATH);
		RLK_STRCPY(pAd->RaCfgObj.ext_eeprom.name,  EEPROM_BIN_FILE_PATH);
	}
	else
	{
		RLK_STRCPY(pAd->RaCfgObj.firmware.name, INIC_STA_FIRMWARE_PATH);
		RLK_STRCPY(pAd->RaCfgObj.profile.name,  INIC_STA_PROFILE_PATH);
		RLK_STRCPY(pAd->RaCfgObj.ext_eeprom.name,  EEPROM_BIN_FILE_PATH);
	}



	/* upgrade config file ************************************************/
	ret = rlk_inic_racfg((UINT *)usb_ep0_pData,RACFG_CMD_BOOT_INITCFG);

	if(ret <0)	{
		printk("\n The %s not respond !! --> RACFG_CMD_BOOT_INITCFG\n", DRV_NAME);
		return;
	}
	//hex_dump("fw_upgrade_to_dev after rlk_inic_racfg for .dat ", usb_ep0_pData, 64);
	printk("\nReady load firmware = %s\n", pAd->RaCfgObj.profile.name);

	RaCfgOpenFile(pAd, &pAd->RaCfgObj.profile,O_RDONLY);


	if (pAd->RaCfgObj.bExtEEPROM)
	{
		RaCfgOpenFile(pAd, &pAd->RaCfgObj.ext_eeprom, O_RDONLY);
		pAd->RaCfgObj.bGetExtEEPROMSize = 0;
	}
	RaCfgUpload(pAd, &pAd->RaCfgObj.profile, 1);

	/* upgrade firmware ************************************************/
	ret = rlk_inic_racfg((UINT *)usb_ep0_pData,RACFG_CMD_BOOT_UPLOAD);

	if(ret <0)	{
		printk("\n The %s not respond !! --> RACFG_CMD_BOOT_UPLOAD\n", DRV_NAME);
		return;
	}
	//hex_dump("fw_upgrade_to_dev after rlk_inic_racfg for .bin ", usb_ep0_pData, 64);
	printk("\nReady load firmware = %s\n", pAd->RaCfgObj.firmware.name);

	RaCfgOpenFile(pAd, &pAd->RaCfgObj.firmware,O_RDONLY);
	RaCfgUpload(pAd, &pAd->RaCfgObj.firmware, 0);

}

INT write_firmware_thread (
		IN void *Context)
{

	rt_threadInfo *fw_info = (rt_threadInfo *)Context;
	fw_thread_init("RT_FWUpgrade", (PVOID)&(fw_info->fwupgrad_notify));
	fw_upgrade_to_dev(fw_info->pAd);
	complete_and_exit (&fw_info->fwupgrad_notify, 0);

	return 0;
}

void Start_fwupgrade_thread(iNIC_PRIVATE *pAd)
{
	rt_threadInfo *rt_fw_info = kmalloc(sizeof(rt_threadInfo), GFP_ATOMIC);
	write_dat_file_semaphore = kmalloc(sizeof(struct semaphore), GFP_ATOMIC);

	sema_init(write_dat_file_semaphore, 0);

	init_completion(&rt_fw_info->fwupgrad_notify);

	rt_fw_info->pAd = pAd;
	rt_fw_info->fwupgrade_pid = kernel_thread(write_firmware_thread, rt_fw_info, CLONE_VM);

	if (rt_fw_info->fwupgrade_pid < 0) {
		printk("unable to start kernel firmware upgrade thread\n");
	}
	else
	{
		printk("f/w upgrade pid=%d, %x\n", rt_fw_info->fwupgrade_pid, rt_fw_info->fwupgrade_pid);
	}

	wait_for_completion(&rt_fw_info->fwupgrad_notify);
	kfree(write_dat_file_semaphore);
	kfree(rt_fw_info);

}
