/***************************************************************************
 * RT2x00 SourceForge Project - http://rt2x00.serialmonkey.com             *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 *                                                                         *
 *   Licensed under the GNU GPL                                            *
 *   Original code supplied under license from RaLink Inc, 2004.           *
 ***************************************************************************/

/***************************************************************************
 *	Module Name:	rtusb_io.c
 *
 *	Abstract:
 *
 *	Revision History:
 *	Who		When		What
 *	--------	----------	-----------------------------
 *	Paul Lin	06-25-2004	created
 *
 ***************************************************************************/

#include	"rt_config.h"


/*
	========================================================================

	Routine Description: NIC initialization complete

	Arguments:

	Return Value:

	Note:

	========================================================================
*/
NTSTATUS	RTUSBFirmwareRun(
	IN	PRTMP_ADAPTER	pAd)
{
	NTSTATUS	Status;

	Status = RTUSB_VendorRequest(
		pAd,
		0,
		DEVICE_VENDOR_REQUEST_OUT,
		0x01,
		0x8,
		0,
		NULL,
		0);

	return Status;
}

/*
	========================================================================

	Routine Description: Read various length data from RT2573

	Arguments:

	Return Value:

	Note:

	========================================================================
*/
NTSTATUS	RTUSBMultiRead(
	IN	PRTMP_ADAPTER	pAd,
	IN	USHORT			Offset,
	OUT	PVOID			pData,
	IN	USHORT			length)
{
	NTSTATUS	Status;

	Status = RTUSB_VendorRequest(
		pAd,
		0,
		DEVICE_VENDOR_REQUEST_IN,
		0x7,
		0,
		Offset,
		pData,
		length);

	return Status;
}

/*
	========================================================================

	Routine Description: Write various length data to RT2573

	Arguments:

	Return Value:

	Note:

	========================================================================
*/
NTSTATUS	RTUSBMultiWrite(
	IN	PRTMP_ADAPTER	pAd,
	IN	USHORT			Offset,
	IN	PVOID			pData,
	IN	USHORT			length)
{
	NTSTATUS	Status;

	Status = RTUSB_VendorRequest(
		pAd,
		0,
		DEVICE_VENDOR_REQUEST_OUT,
		0x6,
		0,
		Offset,
		pData,
		length);

	return Status;
}

/*
	========================================================================

	Routine Description: Read 32-bit MAC register

	Arguments:

	Return Value:

	Note:

	========================================================================
*/
NTSTATUS	RTUSBReadMACRegister(
	IN	PRTMP_ADAPTER	pAd,
	IN	USHORT			Offset,
	OUT	PULONG			pValue)
{
	NTSTATUS	Status;
	Status = RTUSB_VendorRequest(
		pAd,
		0,
		DEVICE_VENDOR_REQUEST_IN,
		0x7,
		0,
		Offset,
		pValue,
		4);
	le32_to_cpus(pValue);

	return Status;
}

/*
	========================================================================

	Routine Description: Write 32-bit MAC register

	Arguments:

	Return Value:

	Note:

	========================================================================
*/
NTSTATUS	RTUSBWriteMACRegister(
	IN	PRTMP_ADAPTER	pAd,
	IN	USHORT			Offset,
	IN	ULONG			Value)
{
	NTSTATUS	Status;

	cpu_to_le32s(&Value);
	Status = RTUSB_VendorRequest(
		pAd,
		0,
		DEVICE_VENDOR_REQUEST_OUT,
		0x6,
		0x00,
		Offset,
		&Value,
		4);

	return Status;
}

NTSTATUS	RT73WriteTXRXCSR0(
	IN	PRTMP_ADAPTER pAd,
	IN	BOOLEAN 	disableRx,
	IN	BOOLEAN		dropControl)
{
    ULONG val = 0x0046b032;

    if (pAd->PortCfg.BssType != BSS_MONITOR)
    {
	val |= 0x00100000;		//Drop promiscuous frames if not in rfmon
	val |= 0x00200000;		//Drop to_ds (packets from station to access point/distribution system)
    }

    if (disableRx == TRUE)
    {
	val |= 0x00010000;
    }

    if (dropControl == TRUE)
    {
	val |= 0x00080000;
    }

    return RTUSBWriteMACRegister(pAd, TXRX_CSR0, val);
}

/*
	========================================================================

	Routine Description: Write 32-bit MAC register

	Arguments:

	Return Value:

	Note:

	========================================================================
*/
NTSTATUS    RTUSBSetLED(
	IN	PRTMP_ADAPTER		pAd,
	IN	MCU_LEDCS_STRUC		LedStatus,
	IN	USHORT				LedIndicatorStrength)
{
	NTSTATUS	Status;

	Status = RTUSB_VendorRequest(
		pAd,
		0,
		DEVICE_VENDOR_REQUEST_OUT,
		0x0a,
		LedStatus.word,
		LedIndicatorStrength,
		NULL,
		0);

	return Status;
}

/*
	========================================================================

	Routine Description: Read 8-bit BBP register

	Arguments:

	Return Value:

	Note:

	========================================================================
*/
NTSTATUS	RTUSBReadBBPRegister(
	IN	PRTMP_ADAPTER	pAd,
	IN	UCHAR			Id,
	IN	PUCHAR			pValue)
{
	PHY_CSR3_STRUC  *PhyCsr3 = kzalloc(sizeof(PHY_CSR3_STRUC), GFP_KERNEL);
	UINT			i = 0;

	if (!PhyCsr3) {
		DBGPRINT(RT_DEBUG_ERROR, "couldn't allocate memory\n");
		return -ENOMEM;
	}

	// Verify the busy condition
	do
	{
		RTUSBReadMACRegister(pAd, PHY_CSR3, &PhyCsr3->word);
		if (!(PhyCsr3->field.Busy == BUSY))
			break;
		i++;
	}
	while ((i < RETRY_LIMIT) && (!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST)));
	//DBGPRINT(RT_DEBUG_INFO, "- %s: Pre-busy PHY_CSR3=0x%08x\n",
			//__FUNCTION__, PhyCsr3.word);

	if ((i == RETRY_LIMIT) || (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST)))
	{
		//
		// Read failed then Return Default value.
		//
		*pValue = pAd->BbpWriteLatch[Id];

		KPRINT(KERN_NOTICE,
				"- BBP read: Pre-busy error or device removed!!!\n");
		DBGPRINT(RT_DEBUG_ERROR, "Retry count exhausted or device removed!!!\n");
		kfree(PhyCsr3);
		return STATUS_UNSUCCESSFUL;
	}

	// Prepare for write material
	PhyCsr3->word 				= 0;
	PhyCsr3->field.fRead			= 1;
	PhyCsr3->field.Busy			= 1;
	PhyCsr3->field.RegNum 		= Id;
	RTUSBWriteMACRegister(pAd, PHY_CSR3, PhyCsr3->word);

	i = 0;
	// Verify the busy condition
	do
	{
		RTUSBReadMACRegister(pAd, PHY_CSR3, &PhyCsr3->word);
		if (!(PhyCsr3->field.Busy == BUSY))
		{
			*pValue = (UCHAR)PhyCsr3->field.Value;
			break;
		}
		i++;
	}
	while ((i < RETRY_LIMIT) && (!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST)));
	//DBGPRINT(RT_DEBUG_INFO, "- %s: Post-busy PHY_CSR3=0x%08x\n",
			//__FUNCTION__, PhyCsr3.word);

	if ((i == RETRY_LIMIT) || (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST)))
	{
		//
		// Read failed then Return Default value.
		//
		*pValue = pAd->BbpWriteLatch[Id];

		KPRINT(KERN_NOTICE,
				"- BBP read: Post-busy error or device removed!!!\n");
		DBGPRINT(RT_DEBUG_ERROR, "Retry count exhausted or device removed!!!\n");
		kfree(PhyCsr3);
		return STATUS_UNSUCCESSFUL;
	}

	kfree(PhyCsr3);
	return STATUS_SUCCESS;
}

/*
	========================================================================

	Routine Description: Write 8-bit BBP register

	Arguments:

	Return Value:

	Note:

	========================================================================
*/
NTSTATUS	RTUSBWriteBBPRegister(
	IN	PRTMP_ADAPTER	pAd,
	IN	UCHAR			Id,
	IN	UCHAR			Value)
{
	PHY_CSR3_STRUC  *PhyCsr3 = kzalloc(sizeof(PHY_CSR3_STRUC), GFP_KERNEL);
	UINT			i = 0;

	if (!PhyCsr3) {
		DBGPRINT(RT_DEBUG_ERROR, "couldn't allocate memory\n");
		return -ENOMEM;
	}
	// Verify the busy condition
	do
	{
		RTUSBReadMACRegister(pAd, PHY_CSR3, &PhyCsr3->word);
		if (!(PhyCsr3->field.Busy == BUSY))
			break;
		i++;
	}
	while ((i < RETRY_LIMIT) && (!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST)));

	if ((i == RETRY_LIMIT) || (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST)))
	{
		DBGPRINT(RT_DEBUG_ERROR,
				"- %s: Busy error=0x%08x or device removed!!!\n",
				__FUNCTION__, PhyCsr3->word);
		KPRINT(KERN_NOTICE,
				"- BBP write: Retry count exhausted or device removed!!!\n");
		kfree(PhyCsr3);
		return STATUS_UNSUCCESSFUL;
	}

	// Prepare for write material
	PhyCsr3->word 				= 0;
	PhyCsr3->field.fRead			= 0;
	PhyCsr3->field.Value			= Value;
	PhyCsr3->field.Busy			= 1;
	PhyCsr3->field.RegNum 		= Id;
	RTUSBWriteMACRegister(pAd, PHY_CSR3, PhyCsr3->word);

	pAd->BbpWriteLatch[Id] = Value;

	kfree(PhyCsr3);
	return STATUS_SUCCESS;
}

/*
	========================================================================

	Routine Description: Write RF register through MAC

	Arguments:

	Return Value:

	Note:

	========================================================================
*/
NTSTATUS	RTUSBWriteRFRegister(
	IN	PRTMP_ADAPTER	pAd,
	IN	ULONG			Value)
{
	PHY_CSR4_STRUC  *PhyCsr4 = kzalloc(sizeof(PHY_CSR4_STRUC), GFP_KERNEL);
	UINT			i = 0;

	if (!PhyCsr4) {
		DBGPRINT(RT_DEBUG_ERROR, "couldn't allocate memory\n");
		return -ENOMEM;
	}
	do
	{
		RTUSBReadMACRegister(pAd, PHY_CSR4, &PhyCsr4->word);
		if (!(PhyCsr4->field.Busy))
			break;
		i++;
	}
	while ((i < RETRY_LIMIT) && (!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST)));

	if ((i == RETRY_LIMIT) || (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST)))
	{
		DBGPRINT(RT_DEBUG_ERROR,
				"- %s: Busy error=0x%08x or device removed!!!\n",
				__FUNCTION__, PhyCsr4->word);
		KPRINT(KERN_NOTICE,
				"- RF write: Retry count exhausted or device removed!!!\n");
		kfree(PhyCsr4);
		return STATUS_UNSUCCESSFUL;
	}

	RTUSBWriteMACRegister(pAd, PHY_CSR4, Value);

	kfree(PhyCsr4);
	return STATUS_SUCCESS;
}

/*
	========================================================================

	Routine Description:

	Arguments:

	Return Value:

	Note:

	========================================================================
*/
NTSTATUS	RTUSBReadEEPROM(
	IN	PRTMP_ADAPTER	pAd,
	IN	USHORT			Offset,
	OUT	PVOID			pData,
	IN	USHORT			length)
{
	NTSTATUS	Status;

	Status = RTUSB_VendorRequest(
		pAd,
		0,
		DEVICE_VENDOR_REQUEST_IN,
		0x9,
		0,
		Offset,
		pData,
		length);

	return Status;
}

/*
	========================================================================

	Routine Description:

	Arguments:

	Return Value:

	Note:

	========================================================================
*/
NTSTATUS	RTUSBWriteEEPROM(
	IN	PRTMP_ADAPTER	pAd,
	IN	USHORT			Offset,
	IN	PVOID			pData,
	IN	USHORT			length)
{
	NTSTATUS	Status;

	Status = RTUSB_VendorRequest(
		pAd,
		0,
		DEVICE_VENDOR_REQUEST_OUT,
		0x8,
		0,
		Offset,
		pData,
		length);

	return Status;
}

/*
	========================================================================

	Routine Description:

	Arguments:

	Return Value:

	Note:

	========================================================================
*/
NTSTATUS	RTUSBStopRx(
	IN	PRTMP_ADAPTER	pAd)
{
	NTSTATUS	Status;

	Status = RTUSB_VendorRequest(pAd,
             0,
             DEVICE_VENDOR_REQUEST_OUT,
             0x0C,
             0x0,
             0x0,
             NULL,
             0);

	return Status;
}

/*
	========================================================================

	Routine Description:

	Arguments:

	Return Value:

	Note:

	========================================================================
*/
NTSTATUS RTUSBPutToSleep(
	IN	PRTMP_ADAPTER	pAd)
{
	NTSTATUS	Status;

	Status = RTUSB_VendorRequest(
		pAd,
		0,
		DEVICE_VENDOR_REQUEST_OUT,
		0x01,
		0x07,
		0,
		NULL,
		0);

	return Status;
}

/*
	========================================================================

	Routine Description:

	Arguments:

	Return Value:

	Note:

	========================================================================
*/
NTSTATUS RTUSBWakeUp(
	IN	PRTMP_ADAPTER	pAd)
{
	NTSTATUS	Status;

	Status = RTUSB_VendorRequest(
		pAd,
		0,
		DEVICE_VENDOR_REQUEST_OUT,
		0x01,
		0x09,
		0,
		NULL,
		0);

	return Status;
}

/*
	========================================================================

	Routine Description:

	Arguments:

	Return Value:

	Note:

	========================================================================
*/
VOID	RTUSBInitializeCmdQ(
	IN	PCmdQ	cmdq)
{
	cmdq->head = NULL;
	cmdq->tail = NULL;
	cmdq->size = 0;
}

/*
	========================================================================

	Routine Description:

	Arguments:

	Return Value:

	Note:

	========================================================================
*/
NDIS_STATUS	RTUSBEnqueueCmdFromNdis(
	IN	PRTMP_ADAPTER	pAd,
	IN	NDIS_OID		Oid,
	IN	BOOLEAN			SetInformation,
	IN	PVOID			pInformationBuffer,
	IN	ULONG			InformationBufferLength)
{
	PCmdQElmt	cmdqelmt = NULL;
    PCmdQElmt	Dcmdqelmt = NULL;
	unsigned long       flags;

	if (pAd->RTUSBCmdThr_pid < 0)
		return (NDIS_STATUS_RESOURCES);

    cmdqelmt = (PCmdQElmt) kmalloc(sizeof(CmdQElmt), MEM_ALLOC_FLAG);
	if (!cmdqelmt)
	{
		DBGPRINT(RT_DEBUG_ERROR,"Not enough memory\n");
		//kfree((PCmdQElmt)cmdqelmt);
		return NDIS_STATUS_RESOURCES;
	}

	if ((Oid == RT_OID_MULTI_READ_MAC) ||
		(Oid == RT_OID_VENDOR_READ_BBP) ||
#ifdef DBG
		(Oid == RT_OID_802_11_QUERY_HARDWARE_REGISTER) ||
#endif
		(Oid == RT_OID_USB_VENDOR_EEPROM_READ))
	{
		cmdqelmt->buffer = pInformationBuffer;
	}
	else
	{
		cmdqelmt->buffer = NULL;
		if (pInformationBuffer != NULL)
		{
			cmdqelmt->buffer =	kmalloc(InformationBufferLength, MEM_ALLOC_FLAG);
			if ((!cmdqelmt->buffer) )
			{
				//kfree((PVOID)cmdqelmt->buffer);
				if(cmdqelmt != NULL){
					kfree((PCmdQElmt)cmdqelmt);
				}
				return (NDIS_STATUS_RESOURCES);
			}
			else
			{
				memcpy(cmdqelmt->buffer, pInformationBuffer, InformationBufferLength);
				cmdqelmt->bufferlength = InformationBufferLength;
			}
		}
		else
			cmdqelmt->bufferlength = 0;
	}

	cmdqelmt->command = Oid;
	cmdqelmt->CmdFromNdis = TRUE;
	if (SetInformation == TRUE)
		cmdqelmt->SetOperation = TRUE;
	else
		cmdqelmt->SetOperation = FALSE;

	NdisAcquireSpinLock(&pAd->CmdQLock);
	EnqueueCmd((&pAd->CmdQ), cmdqelmt);
	NdisReleaseSpinLock(&pAd->CmdQLock);

#if 1
	NdisAcquireSpinLock(&pAd->CmdQLock);
	if( pAd->CmdQ.size > 2048 ){//Thomas add
		RTUSBDequeueCmd(&pAd->CmdQ, &Dcmdqelmt);
		if(Dcmdqelmt != NULL){
			if (Dcmdqelmt->buffer != NULL){
				kfree(Dcmdqelmt->buffer);
			}
			kfree((PCmdQElmt)Dcmdqelmt);
			Dcmdqelmt=NULL;
		}
	}
	NdisReleaseSpinLock(&pAd->CmdQLock);
#endif

	RTUSBCMDUp(pAd);

	if ((Oid == OID_802_11_BSSID_LIST_SCAN) ||
		(Oid == RT_OID_802_11_BSSID) ||
		(Oid == OID_802_11_SSID) ||
		(Oid == OID_802_11_DISASSOCIATE))
	{
		return(NDIS_STATUS_SUCCESS);
	}

    return(NDIS_STATUS_SUCCESS);
}

/*
	========================================================================

	Routine Description:

	Arguments:

	Return Value:

	Note:

	========================================================================
*/
VOID	RTUSBEnqueueInternalCmd(
	IN	PRTMP_ADAPTER	pAd,
	IN	NDIS_OID		Oid)
{
	PCmdQElmt	cmdqelmt = NULL;
	unsigned long       flags;

	if (pAd->RTUSBCmdThr_pid < 0) {
		DBGPRINT(RT_DEBUG_TRACE, "<-- %s: no CmdThr\n", __FUNCTION__);
		return;
	}
	switch (Oid)
	{
		case RT_OID_CHECK_GPIO:
			cmdqelmt = &(pAd->CmdQElements[CMD_CHECK_GPIO]);
			break;

		case RT_OID_PERIODIC_EXECUT:
			cmdqelmt = &(pAd->CmdQElements[CMD_PERIODIC_EXECUT]);
			break;

		//For Alpha only
		case RT_OID_ASICLED_EXECUT:
			cmdqelmt = &(pAd->CmdQElements[CMD_ASICLED_EXECUT]);
			break;

		case RT_OID_UPDATE_TX_RATE:
			cmdqelmt = &(pAd->CmdQElements[CMD_UPDATE_TX_RATE]);
			break;

		case RT_OID_SET_PSM_BIT_SAVE:
			cmdqelmt = &(pAd->CmdQElements[CMD_SET_PSM_SAVE]);
			break;

		case RT_OID_LINK_DOWN:
			cmdqelmt = &(pAd->CmdQElements[CMD_LINK_DOWN]);
			break;

		case RT_OID_USB_RESET_BULK_IN:
			cmdqelmt = &(pAd->CmdQElements[CMD_RESET_BULKIN]);
			break;

		case RT_OID_USB_RESET_BULK_OUT:
			cmdqelmt = &(pAd->CmdQElements[CMD_RESET_BULKOUT]);
			break;

		case RT_OID_RESET_FROM_ERROR:
			cmdqelmt = &(pAd->CmdQElements[CMD_RESET_FROM_ERROR]);
			break;

		case RT_OID_RESET_FROM_NDIS:
			cmdqelmt = &(pAd->CmdQElements[CMD_RESET_FROM_NDIS]);
			break;

		case RT_PERFORM_SOFT_DIVERSITY:
			cmdqelmt = &(pAd->CmdQElements[CMD_SOFT_DIVERSITY]);
			break;

        case RT_OID_FORCE_WAKE_UP:
            cmdqelmt = &(pAd->CmdQElements[CMD_FORCE_WAKEUP]);
            break;

        case RT_OID_SET_PSM_BIT_ACTIVE:
            cmdqelmt = &(pAd->CmdQElements[CMD_SET_PSM_ACTIVE]);
        break;

		default:
			break;
	}

	if ((cmdqelmt != NULL) && (cmdqelmt->InUse == FALSE) && (pAd->RTUSBCmdThr_pid > 0))
	{
		cmdqelmt->InUse = TRUE;
		cmdqelmt->command = Oid;

		NdisAcquireSpinLock(&pAd->CmdQLock);
		EnqueueCmd((&pAd->CmdQ), cmdqelmt);
		NdisReleaseSpinLock(&pAd->CmdQLock);

		RTUSBCMDUp(pAd);
		//DBGPRINT(RT_DEBUG_TRACE, "<-- %s: CmdThr up\n", __FUNCTION__);
	}
	else DBGPRINT(RT_DEBUG_TRACE, "<-- %s CMDThr for 0x%08x in use\n",
			__FUNCTION__, Oid);
}

/*
	========================================================================

	Routine Description:

	Arguments:

	Return Value:

	Note:

	========================================================================
*/
VOID	RTUSBDequeueCmd(
	IN	PCmdQ		cmdq,
	OUT	PCmdQElmt	*pcmdqelmt)
{
	*pcmdqelmt = cmdq->head;

	if (*pcmdqelmt != NULL)
	{
		cmdq->head = cmdq->head->next;
		cmdq->size--;
		if (cmdq->size == 0)
			cmdq->tail = NULL;
	}
}

VOID	RTUSBfreeCmdQElem(
	OUT	PCmdQElmt		cmdqelmt)
{

	if (cmdqelmt->CmdFromNdis == TRUE) {

		if ((cmdqelmt->command != OID_802_11_BSSID_LIST_SCAN) &&
			(cmdqelmt->command != RT_OID_802_11_BSSID) &&
			(cmdqelmt->command != OID_802_11_SSID) &&
			(cmdqelmt->command != OID_802_11_DISASSOCIATE))
		{
		}

		if ((cmdqelmt->command != RT_OID_MULTI_READ_MAC) &&
			(cmdqelmt->command != RT_OID_VENDOR_READ_BBP) &&
#ifdef DBG
			(cmdqelmt->command != RT_OID_802_11_QUERY_HARDWARE_REGISTER) &&
#endif
			(cmdqelmt->command != RT_OID_USB_VENDOR_EEPROM_READ))
		{
			if (cmdqelmt->buffer != NULL) {
				kfree(cmdqelmt->buffer);
			}
		}
		if(cmdqelmt != NULL) {
			kfree((PCmdQElmt)cmdqelmt);
		}
	}
	else {
		cmdqelmt->InUse = FALSE;
	}
} /* End RTUSBfreeCmdQElem () */

VOID	RTUSBfreeCmdQ(
	IN	PRTMP_ADAPTER	pAd,
	IN	PCmdQ			cmdq)
{
	CmdQElmt		*cmdqelmt;
	unsigned long	flags;	// For "Ndis" spin lock

	NdisAcquireSpinLock(&pAd->CmdQLock);
	while (cmdq->size > 0)
	{
		RTUSBDequeueCmd(cmdq, &cmdqelmt);
		RTUSBfreeCmdQElem(cmdqelmt);
	}
	NdisReleaseSpinLock(&pAd->CmdQLock);

} /* End RTUSBfreeCmdQ () */

/*
    ========================================================================
	  usb_control_msg - Builds a control urb, sends it off and waits for completion
	  @dev: pointer to the usb device to send the message to
	  @pipe: endpoint "pipe" to send the message to
	  @request: USB message request value
	  @requesttype: USB message request type value
	  @value: USB message value
	  @index: USB message index value
	  @data: pointer to the data to send
	  @size: length in bytes of the data to send
	  @timeout: time in jiffies to wait for the message to complete before
			  timing out (if 0 the wait is forever)
	  Context: !in_interrupt ()

	  This function sends a simple control message to a specified endpoint
	  and waits for the message to complete, or timeout.
	  If successful, it returns the number of bytes transferred, otherwise a negative error number.

	 Don't use this function from within an interrupt context, like a
	  bottom half handler.	If you need an asynchronous message, or need to send
	  a message from within interrupt context, use usb_submit_urb()
	  If a thread in your driver uses this call, make sure your disconnect()
	  method can wait for it to complete.  Since you don't have a handle on
	  the URB used, you can't cancel the request.


	Routine Description:

	Arguments:

	Return Value:

	Note:

	========================================================================
*/
INT	    RTUSB_VendorRequest(
	IN	PRTMP_ADAPTER	pAd,
	IN	ULONG			TransferFlags,
	IN	UCHAR			RequestType,
	IN	UCHAR			Request,
	IN	USHORT			Value,
	IN	USHORT			Index,
	IN	PVOID			TransferBuffer,
	IN	ULONG			TransferBufferLength)
{
	int ret;

	if (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST))
	{
		DBGPRINT(RT_DEBUG_ERROR,"device disconnected\n");
		return -1;
	}
	else if (in_interrupt())
	{
		DBGPRINT(RT_DEBUG_ERROR,"in_interrupt, return RTUSB_VendorRequest\n");

		return -1;
	}
	else
	{

		if( RequestType == DEVICE_VENDOR_REQUEST_OUT)
			ret=usb_control_msg(pAd->pUsb_Dev, usb_sndctrlpipe( pAd->pUsb_Dev, 0 ), Request, RequestType, Value,Index, TransferBuffer, TransferBufferLength, CONTROL_TIMEOUT_JIFFIES);
		else if(RequestType == DEVICE_VENDOR_REQUEST_IN)
			ret=usb_control_msg(pAd->pUsb_Dev, usb_rcvctrlpipe( pAd->pUsb_Dev, 0 ), Request, RequestType, Value,Index, TransferBuffer, TransferBufferLength, CONTROL_TIMEOUT_JIFFIES);
		else
		{
			DBGPRINT(RT_DEBUG_ERROR,"vendor request direction is failed\n");
			ret = -1;
		}
        if (ret < 0) {
			switch (ret) {
			case -ECONNRESET:		// async unlink via call to usb_unlink_urb()
			case -ENOENT:			// stopped by call to usb_kill_urb
			case -ESHUTDOWN:		// hardware gone = -108
			case -EPROTO:			// unplugged = -71
				RTMP_SET_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST);
				DBGPRINT(RT_DEBUG_ERROR,"=== %s: Non-recoverable err = %d\n",
					__FUNCTION__, ret);

				break;

			default:
				DBGPRINT(RT_DEBUG_ERROR,"USBVendorRequest failed ret=%d \n",ret);
				break;
			}
		}
	}
	return ret;
}

/*
	========================================================================

	Routine Description:
	  Creates an IRP to submite an IOCTL_INTERNAL_USB_RESET_PORT
	  synchronously. Callers of this function must be running at
	  PASSIVE LEVEL.

	Arguments:

	Return Value:

	Note:

	========================================================================
*/
NTSTATUS	RTUSB_ResetDevice(
	IN	PRTMP_ADAPTER	pAd)
{
	NTSTATUS		Status = TRUE;

	DBGPRINT(RT_DEBUG_TRACE, "--->USB_ResetDevice\n");
	//RTMP_SET_FLAG(pAd, fRTMP_ADAPTER_RESET_IN_PROGRESS);
	return Status;
}

#ifdef DBG
#define HARDWARE_MAC	0
#define HARDWARE_BBP	1
#define HARDWARE_RF		2
NDIS_STATUS     RTUSBQueryHardWareRegister(
	IN	PRTMP_ADAPTER	pAd,
	IN	PVOID			pBuf)
{
	PRT_802_11_HARDWARE_REGISTER	pHardwareRegister;
	ULONG							Value;
	USHORT							Offset;
	UCHAR							bbpValue;
	UCHAR							bbpID;
	NDIS_STATUS						Status = NDIS_STATUS_SUCCESS;

	pHardwareRegister = (PRT_802_11_HARDWARE_REGISTER) pBuf;

	if (pHardwareRegister->HardwareType == HARDWARE_MAC)
	{
		//Check Offset is valid?
		if (pHardwareRegister->Offset > 0xF4)
			Status = NDIS_STATUS_FAILURE;

		Offset = (USHORT) pHardwareRegister->Offset;
		RTUSBReadMACRegister(pAd, Offset, &Value);
		pHardwareRegister->Data = Value;
		DBGPRINT(RT_DEBUG_TRACE, "MAC:Offset[0x%04x]=[0x%04x]\n", Offset, Value);
	}
	else if (pHardwareRegister->HardwareType == HARDWARE_BBP)
	{
		bbpID = (UCHAR) pHardwareRegister->Offset;

		RTUSBReadBBPRegister(pAd, bbpID, &bbpValue);
		pHardwareRegister->Data = bbpValue;
		DBGPRINT(RT_DEBUG_TRACE, "BBP:ID[0x%02x]=[0x%02x]\n", bbpID, bbpValue);
	}
	else
		Status = NDIS_STATUS_FAILURE;

	return Status;
}

NDIS_STATUS     RTUSBSetHardWareRegister(
	IN	PRTMP_ADAPTER	pAd,
	IN	PVOID			pBuf)
{
	PRT_802_11_HARDWARE_REGISTER	pHardwareRegister;
	ULONG							Value;
	USHORT							Offset;
	UCHAR							bbpValue;
	UCHAR							bbpID;
	NDIS_STATUS						Status = NDIS_STATUS_SUCCESS;

	pHardwareRegister = (PRT_802_11_HARDWARE_REGISTER) pBuf;

	if (pHardwareRegister->HardwareType == HARDWARE_MAC)
	{
		//Check Offset is valid?
		if (pHardwareRegister->Offset > 0xF4)
			Status = NDIS_STATUS_FAILURE;

		Offset = (USHORT) pHardwareRegister->Offset;
		Value = (ULONG) pHardwareRegister->Data;
		RTUSBWriteMACRegister(pAd, Offset, Value);
		DBGPRINT(RT_DEBUG_TRACE, "RT_OID_802_11_SET_HARDWARE_REGISTER (MAC offset=0x%08x, data=0x%08x)\n", pHardwareRegister->Offset, pHardwareRegister->Data);

		// 2004-11-08 a special 16-byte on-chip memory is used for RaConfig to pass debugging parameters to driver
		// for debug-tuning only
		if ((pHardwareRegister->Offset >= HW_DEBUG_SETTING_BASE) &&
			(pHardwareRegister->Offset <= HW_DEBUG_SETTING_END))
		{
			// 0x2bf0: test power-saving feature
			if (pHardwareRegister->Offset == HW_DEBUG_SETTING_BASE)
			{
#if 0
				ULONG isr, imr, gimr;
				USHORT tbtt = 3;

				RTMP_IO_READ32(pAd, MCU_INT_SOURCE_CSR, &isr);
				RTMP_IO_READ32(pAd, MCU_INT_MASK_CSR, &imr);
				RTMP_IO_READ32(pAd, INT_MASK_CSR, &gimr);
				DBGPRINT(RT_DEBUG_TRACE, "Sleep %d TBTT, 8051 IMR=%08x, ISR=%08x, MAC IMR=%08x\n", tbtt, imr, isr, gimr);
				AsicSleepThenAutoWakeup(pAd, tbtt);
#endif
			}
			// 0x2bf4: test H2M_MAILBOX. byte3: Host command, byte2: token, byte1-0: arguments
			else if (pHardwareRegister->Offset == (HW_DEBUG_SETTING_BASE + 4))
			{
				// 0x2bf4: byte0 non-zero: enable R17 tuning, 0: disable R17 tuning
				if (pHardwareRegister->Data & 0x000000ff)
				{
					pAd->BbpTuning.bEnable = TRUE;
					DBGPRINT(RT_DEBUG_TRACE,"turn on R17 tuning\n");
				}
				else
				{
					UCHAR R17;

					pAd->BbpTuning.bEnable = FALSE;
					if (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_MEDIA_STATE_CONNECTED))
					{
						if (pAd->PortCfg.Channel > 14)
							R17 = pAd->BbpTuning.R17LowerBoundA;
						else
							R17 = pAd->BbpTuning.R17LowerBoundG;
						RTUSBWriteBBPRegister(pAd, 17, R17);
						DBGPRINT(RT_DEBUG_TRACE,"turn off R17 tuning, restore to 0x%02x\n", R17);
					}
				}
			}
			// 0x2bf8: test ACK policy and QOS format in ADHOC mode
			else if (pHardwareRegister->Offset == (HW_DEBUG_SETTING_BASE + 8))
			{
				PUCHAR pAckStr[4] = {"NORMAL", "NO-ACK", "NO-EXPLICIT-ACK", "BLOCK-ACK"};
				EDCA_PARM DefaultEdcaParm;

				// byte0 b1-0 means ACK POLICY - 0: normal ACK, 1: no ACK, 2:no explicit ACK, 3:BA
				pAd->PortCfg.AckPolicy[0] = ((UCHAR)pHardwareRegister->Data & 0x02) << 5;
				pAd->PortCfg.AckPolicy[1] = ((UCHAR)pHardwareRegister->Data & 0x02) << 5;
				pAd->PortCfg.AckPolicy[2] = ((UCHAR)pHardwareRegister->Data & 0x02) << 5;
				pAd->PortCfg.AckPolicy[3] = ((UCHAR)pHardwareRegister->Data & 0x02) << 5;
				DBGPRINT(RT_DEBUG_TRACE, "ACK policy = %s\n", pAckStr[(UCHAR)pHardwareRegister->Data & 0x02]);

				// any non-ZERO value in byte1 turn on EDCA & QOS format
				if (pHardwareRegister->Data & 0x0000ff00)
				{
					memset(&DefaultEdcaParm, 0, sizeof(EDCA_PARM));
					DefaultEdcaParm.bValid = TRUE;
					DefaultEdcaParm.Aifsn[0] = 3;
					DefaultEdcaParm.Aifsn[1] = 7;
					DefaultEdcaParm.Aifsn[2] = 2;
					DefaultEdcaParm.Aifsn[3] = 2;

					DefaultEdcaParm.Cwmin[0] = 4;
					DefaultEdcaParm.Cwmin[1] = 4;
					DefaultEdcaParm.Cwmin[2] = 3;
					DefaultEdcaParm.Cwmin[3] = 2;

					DefaultEdcaParm.Cwmax[0] = 10;
					DefaultEdcaParm.Cwmax[1] = 10;
					DefaultEdcaParm.Cwmax[2] = 4;
					DefaultEdcaParm.Cwmax[3] = 3;

					DefaultEdcaParm.Txop[0]  = 0;
					DefaultEdcaParm.Txop[1]  = 0;
					DefaultEdcaParm.Txop[2]  = 96;
					DefaultEdcaParm.Txop[3]  = 48;
					AsicSetEdcaParm(pAd, &DefaultEdcaParm);
				}
				else
					AsicSetEdcaParm(pAd, NULL);
			}
			// 0x2bfc: turn ON/OFF TX aggregation
			else if (pHardwareRegister->Offset == (HW_DEBUG_SETTING_BASE + 12))
			{
				if (pHardwareRegister->Data)
					OPSTATUS_SET_FLAG(pAd, fOP_STATUS_AGGREGATION_INUSED);
				else
					OPSTATUS_CLEAR_FLAG(pAd, fOP_STATUS_AGGREGATION_INUSED);
				DBGPRINT(RT_DEBUG_TRACE, "AGGREGATION = %d\n", OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_AGGREGATION_INUSED));
			}
			else
				Status = NDIS_STATUS_FAILURE;
		}
	}
	else if (pHardwareRegister->HardwareType == HARDWARE_BBP)
	{
		bbpID = (UCHAR) pHardwareRegister->Offset;
		bbpValue = (UCHAR) pHardwareRegister->Data;
		RTUSBWriteBBPRegister(pAd, bbpID, bbpValue);
		DBGPRINT(RT_DEBUG_TRACE, "BBP:ID[0x%02x]=[0x%02x]\n", bbpID, bbpValue);
	}

	return Status;
}
#endif

