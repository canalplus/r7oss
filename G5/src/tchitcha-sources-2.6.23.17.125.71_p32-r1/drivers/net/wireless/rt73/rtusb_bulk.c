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
 *	Module Name:	rtusb_bulk.c
 *
 *	Abstract:
 *
 *	Revision History:
 *	Who		When		What
 *	--------	----------	-----------------------------
 *	idamlaj		14-10-2006	RFMONTx (based on MarkW's code)
 *
 ***************************************************************************/

#include	"rt_config.h"


void RTusb_fill_bulk_urb (struct urb *pUrb,
	struct usb_device *pUsb_Dev,
	unsigned int bulkpipe,
	void *pTransferBuf,
	int BufSize,
	usb_complete_t Complete,
	void *pContext)
{

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
	usb_fill_bulk_urb(pUrb, pUsb_Dev, bulkpipe, pTransferBuf, BufSize, Complete, pContext);
#else
	FILL_BULK_URB(pUrb, pUsb_Dev, bulkpipe, pTransferBuf, BufSize, Complete, pContext);
#endif

}

// ************************ Completion Func ************************ //
/*
	========================================================================

	Routine Description:
		This routine processes data and RTS/CTS frame completions.
		If the current frame has transmitted OK and there are more fragments,
		then schedule the next frame fragment.

		If there's been an error, empty any remaining fragments for that
		queue from the tx ring.

	Arguments:
		pUrb		Our URB
		pt_regs		Historical

	Return Value:
		void

	Note:
		ALL (i.e. we've done a submit_urb) in-flight URBS are posted complete.
	========================================================================
*/
VOID RTUSBBulkOutDataPacketComplete(purbb_t pUrb, struct pt_regs *pt_regs)
{
	PTX_CONTEXT 	pTxContext;
	PRTMP_ADAPTER	pAd;
	NTSTATUS		status;
	UCHAR			BulkOutPipeId;
	unsigned long			flags;

	pTxContext= (PTX_CONTEXT)pUrb->context;
	pAd = pTxContext->pAd;
	status = pUrb->status;
	atomic_dec(&pAd->PendingTx);

	DBGPRINT(RT_DEBUG_TRACE, "--->%s status=%d PendingTx=%d\n",
			__FUNCTION__, status, atomic_read(&pAd->PendingTx));

	// Store BulkOut PipeId
	BulkOutPipeId = pTxContext->BulkOutPipeId;
	pAd->BulkOutDataOneSecCount++;

	switch (status) {
		case 0:					// OK
			if (pTxContext->LastOne == TRUE)
			{
				pAd->Counters.GoodTransmits++;
				FREE_TX_RING(pAd, BulkOutPipeId, pTxContext);
				pAd->TxRingTotalNumber[BulkOutPipeId]--;// sync. to PendingTx
				pTxContext = nextTxContext(pAd, BulkOutPipeId);
				if (pTxContext->bWaitingBulkOut == TRUE) {
					RTUSB_SET_BULK_FLAG(pAd,
							(fRTUSB_BULK_OUT_DATA_NORMAL << BulkOutPipeId));
				}
			}
			else {
				if ((!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_RESET_IN_PROGRESS)) &&
					(!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_HALT_IN_PROGRESS)) &&
					(!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST)) &&
					(!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_BULKOUT_RESET)))
				{
					FREE_TX_RING(pAd, BulkOutPipeId, pTxContext);
					pAd->TxRingTotalNumber[BulkOutPipeId]--;// sync. to PendingTx

					// Indicate next one is frag data which has highest priority
					RTUSB_SET_BULK_FLAG(pAd,
							(fRTUSB_BULK_OUT_DATA_FRAG << BulkOutPipeId));
				}
				else {
					do {
						FREE_TX_RING(pAd, BulkOutPipeId, pTxContext);
						pAd->TxRingTotalNumber[BulkOutPipeId]--;// sync. to PendingTx
						pTxContext = nextTxContext(pAd, BulkOutPipeId);
					} while (pTxContext->InUse != FALSE);
				}
			}
			RTUSBMlmeUp(pAd);
			break;

		case -ECONNRESET:		// async unlink via call to usb_unlink_urb()
		case -ENOENT:			// stopped by call to usb_kill_urb
		case -ESHUTDOWN:		// hardware gone = -108
		case -EPROTO:			// unplugged = -71
			DBGPRINT(RT_DEBUG_ERROR,"=== %s: shutdown status=%d\n",
					__FUNCTION__, status);
			do {
				FREE_TX_RING(pAd, BulkOutPipeId, pTxContext);
				pAd->TxRingTotalNumber[BulkOutPipeId]--;// sync. to PendingTx
				pTxContext = nextTxContext(pAd, BulkOutPipeId);
			} while (pTxContext->InUse != FALSE);
			break;

		default:
#if 1	// TODO: Think about if we really want to do this reset - bb
			do {
				FREE_TX_RING(pAd, BulkOutPipeId, pTxContext);
				pAd->TxRingTotalNumber[BulkOutPipeId]--;// sync. to PendingTx
				pTxContext = nextTxContext(pAd, BulkOutPipeId);
			} while (pTxContext->InUse != FALSE);

			if ((!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_RESET_IN_PROGRESS)) &&
				(!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_HALT_IN_PROGRESS)) &&
				(!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST)) &&
				(!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_BULKOUT_RESET)))
			{
				DBGPRINT(RT_DEBUG_ERROR, "Bulk Out Data Packet Failed\n");
				RTMP_SET_FLAG(pAd, fRTMP_ADAPTER_BULKOUT_RESET);
				RTUSBEnqueueInternalCmd(pAd, RT_OID_USB_RESET_BULK_OUT);
			}
#endif
			break;
	}
	NdisAcquireSpinLock(&pAd->BulkOutLock[BulkOutPipeId]);
	pAd->BulkOutPending[BulkOutPipeId] = FALSE;
	NdisReleaseSpinLock(&pAd->BulkOutLock[BulkOutPipeId]);
}

// NULL frame use BulkOutPipeId = 0
VOID RTUSBBulkOutNullFrameComplete(purbb_t pUrb, struct pt_regs *pt_regs)
{
	PRTMP_ADAPTER	pAd;
	PTX_CONTEXT		pNullContext;
	NTSTATUS		status;
	unsigned long			flags;

	pNullContext= (PTX_CONTEXT)pUrb->context;
	pAd = pNullContext->pAd;

	// Reset Null frame context flags
	pNullContext->IRPPending = FALSE;
	pNullContext->InUse = FALSE;
	status = pUrb->status;
	atomic_dec(&pAd->PendingTx);

	DBGPRINT(RT_DEBUG_TRACE, "--->%s status=%d PendingTx=%d\n",
			__FUNCTION__, status, atomic_read(&pAd->PendingTx));

	switch (status) {
		case 0:					// OK
			RTUSBMlmeUp(pAd);
			break;

		case -ECONNRESET:		// async unlink via call to usb_unlink_urb()
		case -ENOENT:			// stopped by call to usb_kill_urb
		case -ESHUTDOWN:		// hardware gone = -108
		case -EPROTO:			// unplugged = -71
			DBGPRINT(RT_DEBUG_ERROR,"=== %s: shutdown status=%d\n",
					__FUNCTION__, status);
			break;

		default:
#if 1	// STATUS_OTHER
			if ((!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_RESET_IN_PROGRESS)) &&
				(!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_HALT_IN_PROGRESS)) &&
				(!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST)) &&
				(!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_BULKOUT_RESET)))
			{
				DBGPRINT(RT_DEBUG_ERROR, "Bulk Out Null Frame Failed\n");
				RTMP_SET_FLAG(pAd, fRTMP_ADAPTER_BULKOUT_RESET);
				RTUSBEnqueueInternalCmd(pAd, RT_OID_USB_RESET_BULK_OUT);
			}
			break;
#endif
	}
	NdisAcquireSpinLock(&pAd->BulkOutLock[0]);
	pAd->BulkOutPending[0] = FALSE;
	NdisReleaseSpinLock(&pAd->BulkOutLock[0]);

	DBGPRINT(RT_DEBUG_TRACE, "<---RTUSBBulkOutNullFrameComplete\n");
}

// RTS frame use BulkOutPipeId = PipeID
VOID RTUSBBulkOutRTSFrameComplete(purbb_t pUrb, struct pt_regs *pt_regs)
{
	PRTMP_ADAPTER	pAd;
	PTX_CONTEXT		pRTSContext;
	NTSTATUS		status;
	unsigned long			flags;

	pRTSContext= (PTX_CONTEXT)pUrb->context;
	pAd = pRTSContext->pAd;

	// Reset RTS frame context flags
	pRTSContext->IRPPending = FALSE;
	pRTSContext->InUse = FALSE;
	status = pUrb->status;
	atomic_dec(&pAd->PendingTx);

	DBGPRINT(RT_DEBUG_TRACE, "--->%s status=%d PendingTx=%d\n",
			__FUNCTION__, status, atomic_read(&pAd->PendingTx));

	switch (status) {
		case 0:					// OK
			RTUSBMlmeUp(pAd);
			break;

		case -ECONNRESET:		// async unlink via call to usb_unlink_urb()
		case -ENOENT:			// stopped by call to usb_kill_urb
		case -ESHUTDOWN:		// hardware gone = -108
		case -EPROTO:			// unplugged = -71
			DBGPRINT(RT_DEBUG_ERROR,"=== %s: shutdown status=%d\n",
					__FUNCTION__, status);
			break;

		default:
#if 1	// STATUS_OTHER
			if ((!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_RESET_IN_PROGRESS)) &&
				(!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_HALT_IN_PROGRESS)) &&
				(!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST)) &&
				(!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_BULKOUT_RESET)))
			{
				DBGPRINT(RT_DEBUG_ERROR, "Bulk Out RTS Frame Failed\n");
				RTMP_SET_FLAG(pAd, fRTMP_ADAPTER_BULKOUT_RESET);
				RTUSBEnqueueInternalCmd(pAd, RT_OID_USB_RESET_BULK_OUT);
			}
			break;
#endif
	}
	NdisAcquireSpinLock(&pAd->BulkOutLock[pRTSContext->BulkOutPipeId]);
	pAd->BulkOutPending[pRTSContext->BulkOutPipeId] = FALSE;
	NdisReleaseSpinLock(&pAd->BulkOutLock[pRTSContext->BulkOutPipeId]);

	DBGPRINT(RT_DEBUG_TRACE, "<---RTUSBBulkOutRTSFrameComplete\n");

}

// MLME use BulkOutPipeId = 0
VOID RTUSBBulkOutMLMEPacketComplete(purbb_t pUrb, struct pt_regs *pt_regs)
{
	PTX_CONTEXT			pMLMEContext;
	PRTMP_ADAPTER		pAd;
	NTSTATUS			status;
	unsigned long				flags;

	pMLMEContext= (PTX_CONTEXT)pUrb->context;
	pAd = pMLMEContext->pAd;
	status = pUrb->status;
	atomic_dec(&pAd->PendingTx);

	DBGPRINT(RT_DEBUG_TRACE, "--->%s status=%d PendingTx=%d\n",
			__FUNCTION__, status, atomic_read(&pAd->PendingTx));

	pAd->PrioRingTxCnt--;
	if (pAd->PrioRingTxCnt < 0)
		pAd->PrioRingTxCnt = 0;

	pAd->PrioRingFirstIndex++;
	if (pAd->PrioRingFirstIndex >= PRIO_RING_SIZE)
	{
		pAd->PrioRingFirstIndex = 0;
	}

#if 0
	DBGPRINT(RT_DEBUG_INFO, "RTUSBBulkOutMLMEPacketComplete::PrioRingFirstIndex = %d, PrioRingTxCnt = %d, PopMgmtIndex = %d, PushMgmtIndex = %d, NextMLMEIndex = %d\n",
			pAd->PrioRingFirstIndex,
			pAd->PrioRingTxCnt, pAd->PopMgmtIndex, pAd->PushMgmtIndex, pAd->NextMLMEIndex);

#endif

	// Reset MLME context flags
	pMLMEContext->IRPPending	= FALSE;
	pMLMEContext->InUse 		= FALSE;

	switch (status) {
		case 0:					// OK
			if (pAd->PrioRingTxCnt > 0) {
				RTUSB_SET_BULK_FLAG(pAd, fRTUSB_BULK_OUT_MLME);
			}
			RTUSBMlmeUp(pAd);
			break;

		case -ECONNRESET:		// async unlink via call to usb_unlink_urb()
		case -ENOENT:			// stopped by call to usb_kill_urb
		case -ESHUTDOWN:		// hardware gone = -108
		case -EPROTO:			// unplugged = -71
			DBGPRINT(RT_DEBUG_ERROR,"=== %s: shutdown status=%d\n",
					__FUNCTION__, status);
			break;

		default:
#if 1	// STATUS_OTHER
			if ((!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_RESET_IN_PROGRESS)) &&
				(!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_HALT_IN_PROGRESS)) &&
				(!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST)) &&
				(!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_BULKOUT_RESET)))
			{
				DBGPRINT(RT_DEBUG_ERROR, "Bulk Out MLME Failed\n");
				RTMP_SET_FLAG(pAd, fRTMP_ADAPTER_BULKOUT_RESET);
				RTUSBEnqueueInternalCmd(pAd, RT_OID_USB_RESET_BULK_OUT);
			}
			break;
#endif
	}

	NdisAcquireSpinLock(&pAd->BulkOutLock[0]);
	pAd->BulkOutPending[0] = FALSE;
	NdisReleaseSpinLock(&pAd->BulkOutLock[0]);

	DBGPRINT(RT_DEBUG_TRACE, "<---RTUSBBulkOutMLMEPacketComplete\n");

}

// PS-Poll frame use BulkOutPipeId = 0
VOID RTUSBBulkOutPsPollComplete(purbb_t pUrb,struct pt_regs *pt_regs)
{
	PRTMP_ADAPTER	pAd;
	PTX_CONTEXT		pPsPollContext;
	NTSTATUS		status;
	unsigned long			flags;

	pPsPollContext= (PTX_CONTEXT)pUrb->context;
	pAd = pPsPollContext->pAd;

	// Reset PsPoll context flags
	pPsPollContext->IRPPending	= FALSE;
	pPsPollContext->InUse		= FALSE;
	status = pUrb->status;
	atomic_dec(&pAd->PendingTx);

	DBGPRINT(RT_DEBUG_TRACE, "--->%s status=%d PendingTx=%d\n",
			__FUNCTION__, status, atomic_read(&pAd->PendingTx));

	switch (status) {
		case 0:					// OK
			RTUSBMlmeUp(pAd);
			break;

		case -ECONNRESET:		// async unlink via call to usb_unlink_urb()
		case -ENOENT:			// stopped by call to usb_kill_urb
		case -ESHUTDOWN:		// hardware gone = -108
		case -EPROTO:			// unplugged = -71
			DBGPRINT(RT_DEBUG_ERROR,"=== %s: shutdown status=%d\n",
					__FUNCTION__, status);
			break;

		default:
#if 1	// STATUS_OTHER
			if ((!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_RESET_IN_PROGRESS)) &&
				(!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_HALT_IN_PROGRESS)) &&
				(!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST)) &&
				(!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_BULKOUT_RESET)))
			{
				DBGPRINT(RT_DEBUG_ERROR, "Bulk Out PSPoll Failed\n");
				RTMP_SET_FLAG(pAd, fRTMP_ADAPTER_BULKOUT_RESET);
				RTUSBEnqueueInternalCmd(pAd, RT_OID_USB_RESET_BULK_OUT);
			}
			break;
#endif
	}
	NdisAcquireSpinLock(&pAd->BulkOutLock[0]);
	pAd->BulkOutPending[0] = FALSE;
	NdisReleaseSpinLock(&pAd->BulkOutLock[0]);

	DBGPRINT(RT_DEBUG_TRACE, "<---RTUSBBulkOutPsPollComplete\n");

}

/*
	========================================================================

	Routine Description:
		This routine process Rx Irp and call rx complete function.

	Arguments:
		DeviceObject	Pointer to the device object for next lower
						device. DeviceObject passed in here belongs to
						the next lower driver in the stack because we
						were invoked via IoCallDriver in USB_RxPacket
						AND it is not OUR device object
	  Irp				Ptr to completed IRP
	  Context			Ptr to our Adapter object (context specified
						in IoSetCompletionRoutine

	Return Value:
		Always returns STATUS_MORE_PROCESSING_REQUIRED

	Note:
		Always returns STATUS_MORE_PROCESSING_REQUIRED
	========================================================================
*/
VOID RTUSBBulkRxComplete(purbb_t pUrb, struct pt_regs *pt_regs)
{
	PRX_CONTEXT 	pRxContext;
	PRTMP_ADAPTER	pAd;
	NTSTATUS		status;

	pRxContext= (PRX_CONTEXT)pUrb->context;
	pAd = pRxContext->pAd;

	//
	// We have a number of cases:
	//		1) The USB read timed out and we received no data.
	//		2) The USB read timed out and we received some data.
	//		3) The USB read was successful and fully filled our irp buffer.
	//		4) The irp was cancelled.
	//		5) Some other failure from the USB device object.
	//
	status = pUrb->status;
	atomic_set(&pRxContext->IrpLock, IRPLOCK_COMPLETED);
	atomic_dec(&pAd->PendingRx);

	DBGPRINT(RT_DEBUG_TRACE, "--->%s status=%d PendingRx=%d\n",
			__FUNCTION__, status, atomic_read(&pAd->PendingRx));

	switch (status)
	{
		case 0:
			RTUSBMlmeUp(pAd);
			break;

		case -ECONNRESET:		// async unlink via call to usb_unlink_urb()
		case -ENOENT:			// stopped by call to usb_kill_urb
		case -ESHUTDOWN:		// hardware gone = -108
		case -EPROTO:			// unplugged = -71
			DBGPRINT(RT_DEBUG_ERROR,"=== %s: shutdown status=%d\n",
					__FUNCTION__, status);
			pRxContext->InUse = FALSE;
		default:
			break;
	}
}

VOID	RTUSBInitTxDesc(
	IN	PRTMP_ADAPTER	pAd,
	IN	PTX_CONTEXT 	pTxContext,
	IN	UCHAR			BulkOutPipeId,
	IN	usb_complete_t	Func)
{
	PURB				pUrb;
	PUCHAR				pSrc = NULL;

	pUrb = pTxContext->pUrb;
	ASSERT(pUrb);

	// Store BulkOut PipeId
	pTxContext->BulkOutPipeId = BulkOutPipeId;

    pSrc = (PUCHAR) &pTxContext->TransferBuffer->TxDesc;


	//Initialize a tx bulk urb
	RTusb_fill_bulk_urb(pUrb,
						pAd->pUsb_Dev,
						usb_sndbulkpipe(pAd->pUsb_Dev, 1),
						pSrc,
						pTxContext->BulkOutSize,
						Func,
						pTxContext);

}

VOID	RTUSBInitRxDesc(
	IN	PRTMP_ADAPTER	pAd,
	IN	PRX_CONTEXT		pRxContext)
{
	PURB				pUrb;

	pUrb = pRxContext->pUrb;
	ASSERT(pUrb);

	//Initialize a rx bulk urb
	RTusb_fill_bulk_urb(pUrb,
						pAd->pUsb_Dev,
						usb_rcvbulkpipe(pAd->pUsb_Dev, 1),
						pRxContext->TransferBuffer,
						BUFFER_SIZE,
						RTUSBBulkRxComplete,
						pRxContext);
}

/*
	========================================================================

	Routine Description:
		Admit one URB for transmit. This routine submits one URB at a time,
		even though there may be multiple entries in the Tx ring.

	Arguments:

	Return Value:

	Note:
		TODO: Make sure Ralink's controller doesn't blow up before we try
		to change this to enqueue multiple URBs - bb.

	========================================================================
*/
VOID	RTUSBBulkOutDataPacket(
	IN	PRTMP_ADAPTER	pAd,
	IN	UCHAR			BulkOutPipeId,
	IN	UCHAR			Index)
{
	PTX_CONTEXT	pTxContext;
	PURB		pUrb;
	int 		ret = 0;
	unsigned long		flags;

	NdisAcquireSpinLock(&pAd->BulkOutLock[BulkOutPipeId]);
	if (pAd->BulkOutPending[BulkOutPipeId] == TRUE)
	{
		NdisReleaseSpinLock(&pAd->BulkOutLock[BulkOutPipeId]);
		return;
	}
	pAd->BulkOutPending[BulkOutPipeId] = TRUE;
	NdisReleaseSpinLock(&pAd->BulkOutLock[BulkOutPipeId]);

	pTxContext = &(pAd->TxContext[BulkOutPipeId][Index]);

	// Increase Total transmit byte counter
	pAd->RalinkCounters.TransmittedByteCount +=  pTxContext->BulkOutSize;


	// Clear Data flag
	RTUSB_CLEAR_BULK_FLAG(pAd, (fRTUSB_BULK_OUT_DATA_FRAG << BulkOutPipeId));
	RTUSB_CLEAR_BULK_FLAG(pAd, (fRTUSB_BULK_OUT_DATA_NORMAL << BulkOutPipeId));

	if (pTxContext->bWaitingBulkOut	!= TRUE)
	{
		DBGPRINT(RT_DEBUG_ERROR, "RTUSBBulkOutDataPacket failed, "
				"bWaitingBulkOut != TRUE, Index %d, NextBulkOutIndex %d\n",
				Index, pAd->NextBulkOutIndex[BulkOutPipeId]);
		NdisAcquireSpinLock(&pAd->BulkOutLock[BulkOutPipeId]);
		pAd->BulkOutPending[BulkOutPipeId] = FALSE;
		NdisReleaseSpinLock(&pAd->BulkOutLock[BulkOutPipeId]);
		return;
	}
	else if (pTxContext->BulkOutSize == 0)
	{
		//
		// This may happen on CCX Leap Ckip or Cmic
		// When the Key was been set not on time.
		// We will break it when the Key was Zero on RTUSBHardTransmit
		// And this will cause deadlock that the TxContext always InUse.
		//
		DBGPRINT(RT_DEBUG_ERROR, "RTUSBBulkOutDataPacket failed, "
				"BulkOutSize==0\n");
		NdisAcquireSpinLock(&pAd->BulkOutLock[BulkOutPipeId]);

		FREE_TX_RING(pAd, BulkOutPipeId, pTxContext);
		pAd->TxRingTotalNumber[BulkOutPipeId]--;    // sync. to PendingTx
		pAd->BulkOutPending[BulkOutPipeId] = FALSE;
		NdisReleaseSpinLock(&pAd->BulkOutLock[BulkOutPipeId]);

		return;
	}
	else if (!OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_MEDIA_STATE_CONNECTED) &&
			!(pAd->PortCfg.BssType == BSS_MONITOR && pAd->bAcceptRFMONTx==TRUE))
	{
		//
		// There is no connection, so we need to empty the Tx Bulk out Ring.
		//
		DBGPRINT(RT_DEBUG_ERROR, "RTUSBBulkOutDataPacket failed, "
				"Media Disconnected NextBulkOutIndex %d, NextIndex=%d\n",
				pAd->NextBulkOutIndex[BulkOutPipeId],
				pAd->NextTxIndex[BulkOutPipeId]);

		while (pTxContext->InUse != FALSE)
		{
			FREE_TX_RING(pAd, BulkOutPipeId, pTxContext);
			pAd->TxRingTotalNumber[BulkOutPipeId]--;    // sync. to PendingTx
			pTxContext = nextTxContext(pAd, BulkOutPipeId);
		}

		NdisAcquireSpinLock(&pAd->BulkOutLock[BulkOutPipeId]);
		pAd->BulkOutPending[BulkOutPipeId] = FALSE;
		NdisReleaseSpinLock(&pAd->BulkOutLock[BulkOutPipeId]);

		return;
	}

	// Init Tx context descriptor
	RTUSBInitTxDesc(pAd, pTxContext, BulkOutPipeId, RTUSBBulkOutDataPacketComplete);


	pTxContext->IRPPending = TRUE;

	pUrb = pTxContext->pUrb;
	if((ret = rtusb_submit_urb(pUrb))!=0)
	{
		DBGPRINT(RT_DEBUG_ERROR, "-  %s: Submit Tx URB failed %d\n",
				__FUNCTION__, ret);
		return;
	}
	else {
		atomic_inc(&pAd->PendingTx);
	}

	DBGPRINT(RT_DEBUG_TRACE, "<-- RTUSBBulkOutDataPacket \n");
	return;
}

/*
	========================================================================

	Routine Description:

	Arguments:

	Return Value:

	Note: NULL frame use BulkOutPipeId = 0

	========================================================================
*/
VOID	RTUSBBulkOutNullFrame(
	IN	PRTMP_ADAPTER	pAd)
{
	PTX_CONTEXT	pNullContext = &(pAd->NullContext);
	PURB		pUrb;
	int 		ret = 0;
	unsigned long		flags;

	NdisAcquireSpinLock(&pAd->BulkOutLock[0]);
	if (pAd->BulkOutPending[0] == TRUE)
	{
		NdisReleaseSpinLock(&pAd->BulkOutLock[0]);
		return;
	}
	pAd->BulkOutPending[0] = TRUE;
	NdisReleaseSpinLock(&pAd->BulkOutLock[0]);

	// Increase Total transmit byte counter
	pAd->RalinkCounters.TransmittedByteCount +=  pNullContext->BulkOutSize;

	DBGPRINT(RT_DEBUG_TRACE, "--->RTUSBBulkOutNullFrame \n");

	// Clear Null frame bulk flag
	RTUSB_CLEAR_BULK_FLAG(pAd, fRTUSB_BULK_OUT_DATA_NULL);


	// Init Tx context descriptor
	RTUSBInitTxDesc(pAd, pNullContext, 0, RTUSBBulkOutNullFrameComplete);
	pNullContext->IRPPending = TRUE;

	pUrb = pNullContext->pUrb;
	if((ret = rtusb_submit_urb(pUrb))!=0)
	{
		DBGPRINT(RT_DEBUG_ERROR,"Submit Tx URB failed %d\n", ret);
		return;
	}
	else {
		atomic_inc(&pAd->PendingTx);
	}

	DBGPRINT(RT_DEBUG_TRACE, "<---RTUSBBulkOutNullFrame PendingTx=%d\n",
			atomic_read(&pAd->PendingTx));
	return;
}

/*
	========================================================================

	Routine Description:

	Arguments:

	Return Value:

	Note:
		Apparently not called - bb.

	========================================================================
*/
VOID	RTUSBBulkOutRTSFrame(
	IN	PRTMP_ADAPTER	pAd)
{
	PTX_CONTEXT	pRTSContext = &(pAd->RTSContext);
	PURB		pUrb;
	int 		ret = 0;
	unsigned long		flags;
	UCHAR		PipeID=0;

	if (RTUSB_TEST_BULK_FLAG(pAd, fRTUSB_BULK_OUT_DATA_NORMAL_4))
		PipeID= 3;
	else if (RTUSB_TEST_BULK_FLAG(pAd, fRTUSB_BULK_OUT_DATA_NORMAL_3))
		PipeID= 2;
	else if (RTUSB_TEST_BULK_FLAG(pAd, fRTUSB_BULK_OUT_DATA_NORMAL_2))
		PipeID= 1;
	else if (RTUSB_TEST_BULK_FLAG(pAd, fRTUSB_BULK_OUT_DATA_NORMAL))
		PipeID= 0;


	NdisAcquireSpinLock(&pAd->BulkOutLock[PipeID]);
	if (pAd->BulkOutPending[PipeID] == TRUE)
	{
		NdisReleaseSpinLock(&pAd->BulkOutLock[PipeID]);
		return;
	}
	pAd->BulkOutPending[PipeID] = TRUE;
	NdisReleaseSpinLock(&pAd->BulkOutLock[PipeID]);

	// Increase Total transmit byte counter
	pAd->RalinkCounters.TransmittedByteCount +=  pRTSContext->BulkOutSize;

	DBGPRINT(RT_DEBUG_TRACE, "--->RTUSBBulkOutRTSFrame \n");

	// Clear RTS frame bulk flag
	RTUSB_CLEAR_BULK_FLAG(pAd, fRTUSB_BULK_OUT_RTS);

	// Init Tx context descriptor
	RTUSBInitTxDesc(pAd, pRTSContext, PipeID, RTUSBBulkOutRTSFrameComplete);
	pRTSContext->IRPPending = TRUE;

	pUrb = pRTSContext->pUrb;
	if((ret = rtusb_submit_urb(pUrb))!=0)
	{
		DBGPRINT(RT_DEBUG_ERROR,"Submit Tx URB failed %d\n", ret);
		return;
	}
	else {
		atomic_inc(&pAd->PendingTx);
	}

	DBGPRINT(RT_DEBUG_TRACE, "<---RTUSBBulkOutRTSFrame \n");
	return;
}

/*
	========================================================================

	Routine Description:

	Arguments:

	Return Value:

	Note: MLME use BulkOutPipeId = 0

	========================================================================
*/
VOID	RTUSBBulkOutMLMEPacket(
	IN	PRTMP_ADAPTER	pAd,
	IN	UCHAR			Index)
{
	PTX_CONTEXT		pMLMEContext;
	PURB			pUrb;
	int 			ret = 0;
	unsigned long			flags;

	pMLMEContext = &pAd->MLMEContext[Index];

	NdisAcquireSpinLock(&pAd->BulkOutLock[0]);
	if (pAd->BulkOutPending[0] == TRUE)
	{
		NdisReleaseSpinLock(&pAd->BulkOutLock[0]);
		return;
	}
	pAd->BulkOutPending[0] = TRUE;
	NdisReleaseSpinLock(&pAd->BulkOutLock[0]);

	DBGPRINT(RT_DEBUG_TRACE, "--->RTUSBBulkOutMLMEPacket\n");

	// Increase Total transmit byte counter
	pAd->RalinkCounters.TransmittedByteCount +=  pMLMEContext->BulkOutSize;

	// Clear MLME bulk flag
	RTUSB_CLEAR_BULK_FLAG(pAd, fRTUSB_BULK_OUT_MLME);

#if 0
	DBGPRINT(RT_DEBUG_INFO, "RTUSBBulkOutMLMEPacket::PrioRingFirstIndex = %d, PrioRingTxCnt = %d, PopMgmtIndex = %d, PushMgmtIndex = %d, NextMLMEIndex = %d\n",
			pAd->PrioRingFirstIndex,
			pAd->PrioRingTxCnt, pAd->PopMgmtIndex, pAd->PushMgmtIndex, pAd->NextMLMEIndex);
#endif

	// Init Tx context descriptor
	RTUSBInitTxDesc(pAd, pMLMEContext, 0, RTUSBBulkOutMLMEPacketComplete);
	pMLMEContext->IRPPending = TRUE;


	pUrb = pMLMEContext->pUrb;
	if((ret = rtusb_submit_urb(pUrb))!=0)
	{
		DBGPRINT(RT_DEBUG_ERROR,"Submit MLME URB failed %d\n", ret);
		return;
	}
	else {
		atomic_inc(&pAd->PendingTx);
	}

	DBGPRINT(RT_DEBUG_TRACE, "<---RTUSBBulkOutMLMEPacket \n");
	return;
}

/*
	========================================================================

	Routine Description:

	Arguments:

	Return Value:

	Note: PsPoll use BulkOutPipeId = 0

	========================================================================
*/
VOID	RTUSBBulkOutPsPoll(
	IN	PRTMP_ADAPTER	pAd)
{
	PTX_CONTEXT		pPsPollContext = &(pAd->PsPollContext);
	PURB			pUrb;
	int 			ret = 0;
	unsigned long			flags;

	NdisAcquireSpinLock(&pAd->BulkOutLock[0]);
	if (pAd->BulkOutPending[0] == TRUE)
	{
		NdisReleaseSpinLock(&pAd->BulkOutLock[0]);
		return;
	}
	pAd->BulkOutPending[0] = TRUE;
	NdisReleaseSpinLock(&pAd->BulkOutLock[0]);

	DBGPRINT(RT_DEBUG_TRACE, "--->RTUSBBulkOutPsPoll \n");

	// Clear PS-Poll bulk flag
	RTUSB_CLEAR_BULK_FLAG(pAd, fRTUSB_BULK_OUT_PSPOLL);


	// Init Tx context descriptor
	RTUSBInitTxDesc(pAd, pPsPollContext, 0, RTUSBBulkOutPsPollComplete);
	pPsPollContext->IRPPending = TRUE;

	pUrb = pPsPollContext->pUrb;
	if((ret = rtusb_submit_urb(pUrb))!=0)
	{
		DBGPRINT(RT_DEBUG_ERROR,"Submit Tx URB failed %d\n", ret);
		return;
	}
	else {
		atomic_inc(&pAd->PendingTx);
	}

	DBGPRINT(RT_DEBUG_TRACE, "<---RTUSBBulkOutPsPoll \n");
	return;
}

/*
	========================================================================

	Routine Description:
	USB_RxPacket initializes a URB and uses the Rx IRP to submit it
	to USB. It checks if an Rx Descriptor is available and passes the
	the coresponding buffer to be filled. If no descriptor is available
	fails the request. When setting the completion routine we pass our
	Adapter Object as Context.

	Arguments:

	Return Value:
		TRUE			found matched tuple cache
		FALSE			no matched found

	Note:

	========================================================================
*/
VOID	RTUSBBulkReceive(
	IN	PRTMP_ADAPTER	pAd)
{
	PRX_CONTEXT pRxContext;
	PURB		pUrb;
	int 		ret = 0;

	DBGPRINT(RT_DEBUG_TRACE,"RTUSBBulkReceive:: pAd->NextRxBulkInIndex = %d\n",
			pAd->NextRxBulkInIndex);

	if ((RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST))||
		(RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_RADIO_OFF))||
		(RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_RESET_PIPE_IN_PROGRESS)))
	{
		DBGPRINT(RT_DEBUG_INFO,"RTUSBBulkReceive:: can't start\n");
		return;
	}

	pRxContext = &(pAd->RxContext[pAd->NextRxBulkInIndex]);
	if (pRxContext->InUse == FALSE) {

		// Init Rx context descriptor
		memset(pRxContext->TransferBuffer, 0, BUFFER_SIZE);
		RTUSBInitRxDesc(pAd, pRxContext);

		pUrb = pRxContext->pUrb;
		atomic_set(&pRxContext->IrpLock, IRPLOCK_CANCELABLE);	// s/w timing
		if((ret = rtusb_submit_urb(pUrb)) == 0) {
			pRxContext->InUse = TRUE;
			atomic_inc(&pAd->PendingRx);
			pAd->NextRxBulkInIndex = (pAd->NextRxBulkInIndex+1) % RX_RING_SIZE;
		}
		else {	// -EPIPE -> disconnected
			atomic_set(&pRxContext->IrpLock, IRPLOCK_COMPLETED);
		}
		DBGPRINT(RT_DEBUG_TRACE,"<-- %s: Submit Rx URB ret=%d\n",
				__FUNCTION__, ret);
	}
	else {
		DBGPRINT(RT_DEBUG_TRACE,"<-- %s (Rx Ring full)\n", __FUNCTION__);
	}

	return;
}

/*
	========================================================================

	Routine Description:
	RTUSBBulkReceive cannot be called when in an interrupt as it raises a
	bug on ARM, so we schedule this function so that it calls
	RTUSBBulkReceive outside of interrupt context.

	Arguments: ulong data, memory address to pAd structure.

	Return Value:

	Note:

	========================================================================
*/

VOID	rtusb_bulkrx(
	IN	unsigned long	data)
{
	PRTMP_ADAPTER	pAd = (PRTMP_ADAPTER)data;

	RTUSBBulkReceive(pAd);
}

/*
	========================================================================

	Routine Description:

	Arguments:

	Return Value:

	Note:

	========================================================================
*/
VOID	RTUSBKickBulkOut(
	IN	PRTMP_ADAPTER pAd)
{

	DBGPRINT(RT_DEBUG_TRACE, "--->RTUSBKickBulkOut\n");


	if (!(RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST)) &&
		!(RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_HALT_IN_PROGRESS)) &&
		!(RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_RESET_IN_PROGRESS)) &&
		!(RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_RESET_PIPE_IN_PROGRESS)) &&
		!(RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_RADIO_OFF)))
	{

		// 1. Data Fragment has highest priority
		if (RTUSB_TEST_BULK_FLAG(pAd, fRTUSB_BULK_OUT_DATA_FRAG))
		{
			if ((!LOCAL_TX_RING_EMPTY(pAd, 0)) &&
				(!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_BSS_SCAN_IN_PROGRESS)))
			{
				RTUSBBulkOutDataPacket(pAd, 0, pAd->NextBulkOutIndex[0]);
			}
		}

		if (RTUSB_TEST_BULK_FLAG(pAd, fRTUSB_BULK_OUT_DATA_FRAG_2))
		{
			if ((!LOCAL_TX_RING_EMPTY(pAd, 1)) &&
				(!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_BSS_SCAN_IN_PROGRESS)))
			{
				RTUSBBulkOutDataPacket(pAd, 1, pAd->NextBulkOutIndex[1]);
			}
		}

		if (RTUSB_TEST_BULK_FLAG(pAd, fRTUSB_BULK_OUT_DATA_FRAG_3))
		{
			if ((!LOCAL_TX_RING_EMPTY(pAd, 2)) &&
				(!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_BSS_SCAN_IN_PROGRESS)))
			{
				RTUSBBulkOutDataPacket(pAd, 2, pAd->NextBulkOutIndex[2]);
			}
		}

		if (RTUSB_TEST_BULK_FLAG(pAd, fRTUSB_BULK_OUT_DATA_FRAG_4))
		{
			if ((!LOCAL_TX_RING_EMPTY(pAd, 3)) &&
				(!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_BSS_SCAN_IN_PROGRESS)))
			{
				RTUSBBulkOutDataPacket(pAd, 3, pAd->NextBulkOutIndex[3]);
			}
		}

		// 2. PS-Poll frame is next
		if (RTUSB_TEST_BULK_FLAG(pAd, fRTUSB_BULK_OUT_PSPOLL))
		{
			RTUSBBulkOutPsPoll(pAd);
		}
		// 5. Mlme frame is next
		else if (RTUSB_TEST_BULK_FLAG(pAd, fRTUSB_BULK_OUT_MLME))
		{
			RTUSBBulkOutMLMEPacket(pAd, pAd->PrioRingFirstIndex);
		}

		// 6. Data frame normal is next
		if (RTUSB_TEST_BULK_FLAG(pAd, fRTUSB_BULK_OUT_DATA_NORMAL))
		{
			if ((!LOCAL_TX_RING_EMPTY(pAd, 0)) &&
				(!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_BSS_SCAN_IN_PROGRESS)))
			{
				RTUSBBulkOutDataPacket(pAd, 0, pAd->NextBulkOutIndex[0]);
			}
		}

		if (RTUSB_TEST_BULK_FLAG(pAd, fRTUSB_BULK_OUT_DATA_NORMAL_2))
		{
			if ((!LOCAL_TX_RING_EMPTY(pAd, 1)) &&
				(!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_BSS_SCAN_IN_PROGRESS)))
			{
				RTUSBBulkOutDataPacket(pAd, 1, pAd->NextBulkOutIndex[1]);
			}
		}

		if (RTUSB_TEST_BULK_FLAG(pAd, fRTUSB_BULK_OUT_DATA_NORMAL_3))
		{
			if ((!LOCAL_TX_RING_EMPTY(pAd, 2)) &&
				(!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_BSS_SCAN_IN_PROGRESS)))
			{
				RTUSBBulkOutDataPacket(pAd, 2, pAd->NextBulkOutIndex[2]);
			}
		}

		if (RTUSB_TEST_BULK_FLAG(pAd, fRTUSB_BULK_OUT_DATA_NORMAL_4))
		{
			if ((!LOCAL_TX_RING_EMPTY(pAd, 3)) &&
				(!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_BSS_SCAN_IN_PROGRESS)))
			{
				RTUSBBulkOutDataPacket(pAd, 3, pAd->NextBulkOutIndex[3]);
			}
		}

		// 7. Null frame is the last
		else if (RTUSB_TEST_BULK_FLAG(pAd, fRTUSB_BULK_OUT_DATA_NULL))
		{
			if (!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_BSS_SCAN_IN_PROGRESS))
			{
				RTUSBBulkOutNullFrame(pAd);
			}
		}

		// 8. No data avaliable
		else
		{

		}
	}

	DBGPRINT(RT_DEBUG_TRACE, "<---RTUSBKickBulkOut\n");
}

/*
	========================================================================

	Routine Description:

	Arguments:

	Return Value:

	Note:

	========================================================================
*/
VOID	RTUSBCleanUpDataBulkInQueue(
	IN	PRTMP_ADAPTER	pAd)
{
	int			i = 0;

	DBGPRINT(RT_DEBUG_TRACE, "--> %s: %d PendingRx left\n",
			__FUNCTION__, atomic_read(&pAd->PendingRx));

	do {
		PRX_CONTEXT pRxContext = &pAd->RxContext[i];

		pRxContext->InUse = FALSE;
		atomic_set(&pRxContext->IrpLock, IRPLOCK_COMPLETED);
		pRxContext->IRPPending	= FALSE;
	} while (++i < RX_RING_SIZE);

	DBGPRINT(RT_DEBUG_TRACE, "<-- RTUSBCleanUpDataBulkInQueue\n");

} /* End RTUSBCleanUpDataBulkInQueue () */

/*
	========================================================================

	Routine Description:

	Arguments:

	Return Value:

	Note:

	========================================================================
*/
VOID	RTUSBCleanUpDataBulkOutQueue(
	IN	PRTMP_ADAPTER	pAd)
{
	UCHAR			Idx;
	PTX_CONTEXT 	pTxContext;
	unsigned long			flags;

	DBGPRINT(RT_DEBUG_TRACE, "--->RTUSBCleanUpDataBulkOutQueue\n");

	for (Idx = 0; Idx < 4; Idx++)
	{
		while (!LOCAL_TX_RING_EMPTY(pAd, Idx))
		{
			pTxContext						= &(pAd->TxContext[Idx][pAd->NextBulkOutIndex[Idx]]);
			pTxContext->LastOne 			= FALSE;
			pTxContext->InUse				= FALSE;
			pTxContext->bWaitingBulkOut		= FALSE;
			pAd->NextBulkOutIndex[Idx] = (pAd->NextBulkOutIndex[Idx] + 1) % TX_RING_SIZE;
		}
		NdisAcquireSpinLock(&pAd->BulkOutLock[Idx]);
		pAd->BulkOutPending[Idx] = FALSE;
		NdisReleaseSpinLock(&pAd->BulkOutLock[Idx]);
	}

	DBGPRINT(RT_DEBUG_TRACE, "<---RTUSBCleanUpDataBulkOutQueue\n");
}

/*
	========================================================================

	Routine Description:

	Arguments:

	Return Value:

	Note:

	========================================================================
*/
VOID	RTUSBCleanUpMLMEBulkOutQueue(
	IN	PRTMP_ADAPTER	pAd)
{
	unsigned long	flags;	// For "Ndis" spin lock

	DBGPRINT(RT_DEBUG_TRACE, "--->%s\n", __FUNCTION__);

	NdisAcquireSpinLock(&pAd->MLMEQLock);
	while (pAd->PrioRingTxCnt > 0)
	{
		pAd->MLMEContext[pAd->PrioRingFirstIndex].InUse = FALSE;

		pAd->PrioRingFirstIndex++;
		if (pAd->PrioRingFirstIndex >= PRIO_RING_SIZE)
		{
			pAd->PrioRingFirstIndex = 0;
		}

		pAd->PrioRingTxCnt--;
	}
	NdisReleaseSpinLock(&pAd->MLMEQLock);

	DBGPRINT(RT_DEBUG_TRACE, "<---%s\n", __FUNCTION__);
}

VOID	RTUSBwaitRxDone(
	IN	PRTMP_ADAPTER	pAd)
{
	int i;

	for (i = 0; atomic_read(&pAd->PendingRx) > 0 && i < 25; i++) {
		msleep(UNLINK_TIMEOUT_MS);
	}

} /* End RTUSBwaitRxDone () */

VOID	RTUSBwaitTxDone(
	IN	PRTMP_ADAPTER	pAd)
{
	int i;

	for (i = 0; atomic_read(&pAd->PendingTx) > 0 && i < 25; i++) {
		msleep(UNLINK_TIMEOUT_MS);
	}

} /* End RTUSBwaitTxDone () */

/*
	========================================================================

	Routine Description:

	Arguments:

	Return Value:

	Note:
		Must be called in process context.

	========================================================================
*/
VOID	RTUSBCancelPendingBulkInIRP(
	IN	PRTMP_ADAPTER	pAd)
{
	PRX_CONTEXT	pRxContext;
	//UINT		i;
	int			i = pAd->CurRxBulkInIndex;

	DBGPRINT(RT_DEBUG_TRACE, "--> %s: %d PendingRx left\n",
			__FUNCTION__, atomic_read(&pAd->PendingRx));
#if 0
	for ( i = 0; i < RX_RING_SIZE; i++)
	{
		pRxContext = &(pAd->RxContext[i]);
		if(atomic_read(&pRxContext->IrpLock) == IRPLOCK_CANCELABLE)
		{
			RTUSB_UNLINK_URB(pRxContext->pUrb);
		}
		pRxContext->InUse = FALSE;
		atomic_set(&pRxContext->IrpLock, IRPLOCK_COMPLETED);
	}
#else
	// Cancel till we've caught up with newly issued recieves - bb
	do {
		pRxContext = &pAd->RxContext[i];
		if(atomic_read(&pRxContext->IrpLock) == IRPLOCK_CANCELABLE)
		{
			RTUSB_UNLINK_URB(pRxContext->pUrb);
		}
		if (++i >= RX_RING_SIZE) i = 0;
	} while (i != pAd->NextRxBulkInIndex);
#endif

	// maybe wait for cancellations to finish.
	RTUSBwaitRxDone(pAd);
	pAd->CurRxBulkInIndex = pAd->NextRxBulkInIndex = 0;
	DBGPRINT(RT_DEBUG_TRACE, "<-- %s: %d PendingRx left\n",
			__FUNCTION__, atomic_read(&pAd->PendingRx));
}

/*
	========================================================================

	Routine Description:

	Arguments:

	Return Value:

	Note:
		Must be called in process context.

	========================================================================
*/
VOID	RTUSBCancelPendingBulkOutIRP(
	IN	PRTMP_ADAPTER	pAd)
{
	PTX_CONTEXT		pTxContext;
	PTX_CONTEXT		pMLMEContext;
	PTX_CONTEXT		pBeaconContext;
	PTX_CONTEXT		pNullContext;
	PTX_CONTEXT		pPsPollContext;
	PTX_CONTEXT		pRTSContext;
	UINT			i, Idx;
	unsigned long			flags;

	DBGPRINT(RT_DEBUG_TRACE, "--> %s: %d PendingTx left\n",
			__FUNCTION__, atomic_read(&pAd->PendingTx));
	for (Idx = 0; Idx < 4; Idx++)
	{
		for (i = 0; i < TX_RING_SIZE; i++)
		{
			pTxContext = &(pAd->TxContext[Idx][i]);

			if (pTxContext->IRPPending == TRUE)
			{

				// Get the USB_CONTEXT and cancel it's IRP; the completion routine will itself
				// remove it from the HeadPendingSendList and NULL out HeadPendingSendList
				//	when the last IRP on the list has been	cancelled; that's how we exit this loop
				//

				RTUSB_UNLINK_URB(pTxContext->pUrb);

				// Sleep 200 microseconds to give cancellation time to work
				RTMPusecDelay(200);
			}
		}
	}

	for (i = 0; i < PRIO_RING_SIZE; i++)
	{
		pMLMEContext = &(pAd->MLMEContext[i]);

		if(pMLMEContext->IRPPending == TRUE)
		{

			// Get the USB_CONTEXT and cancel it's IRP; the completion routine will itself
			// remove it from the HeadPendingSendList and NULL out HeadPendingSendList
			//	when the last IRP on the list has been	cancelled; that's how we exit this loop
			//

			RTUSB_UNLINK_URB(pMLMEContext->pUrb);

			// Sleep 200 microsecs to give cancellation time to work
			RTMPusecDelay(200);
		}
	}

	for (i = 0; i < BEACON_RING_SIZE; i++)
	{
		pBeaconContext = &(pAd->BeaconContext[i]);

		if(pBeaconContext->IRPPending == TRUE)
		{

			// Get the USB_CONTEXT and cancel it's IRP; the completion routine will itself
			// remove it from the HeadPendingSendList and NULL out HeadPendingSendList
			//	when the last IRP on the list has been	cancelled; that's how we exit this loop
			//

			RTUSB_UNLINK_URB(pBeaconContext->pUrb);

			// Sleep 200 microsecs to give cancellation time to work
			RTMPusecDelay(200);
		}
	}

	pNullContext = &(pAd->NullContext);
	if (pNullContext->IRPPending == TRUE)
		RTUSB_UNLINK_URB(pNullContext->pUrb);

	pRTSContext = &(pAd->RTSContext);
	if (pRTSContext->IRPPending == TRUE)
		RTUSB_UNLINK_URB(pRTSContext->pUrb);

	pPsPollContext = &(pAd->PsPollContext);
	if (pPsPollContext->IRPPending == TRUE)
		RTUSB_UNLINK_URB(pPsPollContext->pUrb);

	for (Idx = 0; Idx < 4; Idx++)
	{
		NdisAcquireSpinLock(&pAd->BulkOutLock[Idx]);
		pAd->BulkOutPending[Idx] = FALSE;
		NdisReleaseSpinLock(&pAd->BulkOutLock[Idx]);
	}

	// maybe wait for cancellations to finish.
	RTUSBwaitTxDone(pAd);
	DBGPRINT(RT_DEBUG_TRACE, "<-- %s: %d PendingTx left\n",
			__FUNCTION__, atomic_read(&pAd->PendingTx));
}

/*
	========================================================================

	Routine Description:

	Arguments:

	Return Value:

	Note:

	========================================================================
*/
VOID	RTUSBCancelPendingIRPs(
	IN	PRTMP_ADAPTER	pAd)
{
	RTUSBCancelPendingBulkInIRP(pAd);
	RTUSBCancelPendingBulkOutIRP(pAd);
}
