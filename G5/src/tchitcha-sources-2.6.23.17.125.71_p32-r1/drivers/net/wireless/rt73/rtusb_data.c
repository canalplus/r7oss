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
 *	Module Name:	rtusb_data.c
 *
 *	Abstract: Ralink USB driver Tx/Rx functions
 *
 *	Revision History:
 *	Who		When		What
 *	--------	----------	-----------------------------
 *	idamlaj		05-10-2006	Import rfmon implementation
 *	idamlaj		14-10-2006	RFMONTx (based on MarkW's code)
 *
 ***************************************************************************/

#include "rt_config.h"
#include <net/iw_handler.h>

extern	UCHAR Phy11BGNextRateUpward[]; // defined in mlme.c

UCHAR	SNAP_802_1H[] = {0xaa, 0xaa, 0x03, 0x00, 0x00, 0x00};
UCHAR	SNAP_BRIDGE_TUNNEL[] = {0xaa, 0xaa, 0x03, 0x00, 0x00, 0xf8};
UCHAR	EAPOL_LLC_SNAP[]= {0xaa, 0xaa, 0x03, 0x00, 0x00, 0x00, 0x88, 0x8e};
UCHAR	EAPOL[] = {0x88, 0x8e};

UCHAR	IPX[] = {0x81, 0x37};
UCHAR	APPLE_TALK[] = {0x80, 0xf3};

UINT	_11G_RATES[12] = { 0, 0, 0, 0, 6, 9, 12, 18, 24, 36, 48, 54 };

UCHAR	RateIdToPlcpSignal[12] = {
	 0, /* RATE_1 */	1, /* RATE_2 */ 	2, /* RATE_5_5 */	3, /* RATE_11 */	// see BBP spec
	11, /* RATE_6 */   15, /* RATE_9 */    10, /* RATE_12 */   14, /* RATE_18 */	// see IEEE802.11a-1999 p.14
	 9, /* RATE_24 */  13, /* RATE_36 */	8, /* RATE_48 */   12  /* RATE_54 */ }; // see IEEE802.11a-1999 p.14

UCHAR	 OfdmSignalToRateId[16] = {
	RATE_54,  RATE_54,	RATE_54,  RATE_54,	// OFDM PLCP Signal = 0,  1,  2,  3 respectively
	RATE_54,  RATE_54,	RATE_54,  RATE_54,	// OFDM PLCP Signal = 4,  5,  6,  7 respectively
	RATE_48,  RATE_24,	RATE_12,  RATE_6,	// OFDM PLCP Signal = 8,  9,  10, 11 respectively
	RATE_54,  RATE_36,	RATE_18,  RATE_9,	// OFDM PLCP Signal = 12, 13, 14, 15 respectively
};

UCHAR default_cwmin[]={CW_MIN_IN_BITS, CW_MIN_IN_BITS, CW_MIN_IN_BITS-1, CW_MIN_IN_BITS-2};
UCHAR default_cwmax[]={CW_MAX_IN_BITS, CW_MAX_IN_BITS, CW_MIN_IN_BITS, CW_MIN_IN_BITS-1};
UCHAR default_sta_aifsn[]={3,7,2,2};

UCHAR MapUserPriorityToAccessCategory[8] = {QID_AC_BE, QID_AC_BK, QID_AC_BK, QID_AC_BE, QID_AC_VI, QID_AC_VI, QID_AC_VO, QID_AC_VO};



// Macro for rx indication
VOID REPORT_ETHERNET_FRAME_TO_LLC(
	IN	PRTMP_ADAPTER	pAd,
	IN	PUCHAR			p8023hdr,
	IN	PUCHAR			pData,
	IN	ULONG			DataSize,
	IN	struct net_device	*net_dev)
{
	struct sk_buff	*pSkb;

	if ((pSkb = __dev_alloc_skb(DataSize + LENGTH_802_3 + 2, MEM_ALLOC_FLAG)) != NULL)
	{
		pSkb->dev = net_dev;
		skb_reserve(pSkb, 2);	// 16 byte align the IP header
		memcpy(skb_put(pSkb, LENGTH_802_3), p8023hdr, LENGTH_802_3);
		memcpy(skb_put(pSkb, DataSize), pData, DataSize);
		pSkb->protocol = eth_type_trans(pSkb, net_dev);

		netif_rx(pSkb);

		pAd->net_dev->last_rx = jiffies;
		pAd->stats.rx_packets++;

		pAd->Counters8023.GoodReceives++;
	}
	//DBGPRINT(RT_DEBUG_TRACE, "<-- %s: pSkb %s\n", __FUNCTION__,
			//pSkb? "found": "n/a");
}

// Enqueue this frame to MLME engine
// We need to enqueue the whole frame because MLME need to pass data type
// information from 802.11 header
#define REPORT_MGMT_FRAME_TO_MLME(_pAd, _pFrame, _FrameSize, _Rssi, _PlcpSignal)		\
{																						\
	MlmeEnqueueForRecv(_pAd, (UCHAR)_Rssi, _FrameSize, _pFrame, (UCHAR)_PlcpSignal);   \
}

// NOTE: we do have an assumption here, that Byte0 and Byte1 always reasid at the same
//		 scatter gather buffer
NDIS_STATUS Sniff2BytesFromNdisBuffer(
	IN	struct sk_buff	*pFirstSkb,
	IN	UCHAR			DesiredOffset,
	OUT PUCHAR			pByte0,
	OUT PUCHAR			pByte1)
{
	PUCHAR pBufferVA;
	ULONG  BufferLen, AccumulateBufferLen, BufferBeginOffset;

	pBufferVA = (PVOID)pFirstSkb->data;
	BufferLen = pFirstSkb->len;
	BufferBeginOffset	= 0;
	AccumulateBufferLen = BufferLen;

	*pByte0 = *(PUCHAR)(pBufferVA + DesiredOffset - BufferBeginOffset);
	*pByte1 = *(PUCHAR)(pBufferVA + DesiredOffset - BufferBeginOffset + 1);
	return NDIS_STATUS_SUCCESS;
}

/*
	========================================================================

	Routine	Description:
		This routine classifies outgoing frames into several AC (Access
		Category) and enqueue them into corresponding s/w waiting queues.

	Arguments:
		pAd	Pointer	to our adapter
		pPacket		Pointer to send packet

	Return Value:
		None

	Note:

	========================================================================
*/
NDIS_STATUS	RTMPSendPacket(
	IN	PRTMP_ADAPTER	pAd,
	IN	struct sk_buff	*pSkb)
{
	PUCHAR			pSrcBufVA;
	UINT			AllowFragSize;
	UCHAR			NumberOfFrag;
	UCHAR			RTSRequired;
	UCHAR			QueIdx, UserPriority;
	NDIS_STATUS 	Status = NDIS_STATUS_SUCCESS;
	struct sk_buff_head	*pTxQueue;
	UCHAR			PsMode;

	DBGPRINT(RT_DEBUG_INFO, "====> RTMPSendPacket\n");

	// Prepare packet information structure for buffer descriptor
	pSrcBufVA = (PVOID)pSkb->data;

	// STEP 1. Check for virtual address allocation, it might fail !!!
	if (pSrcBufVA == NULL)
	{
		// Resourece is low, system did not allocate virtual address
		// return NDIS_STATUS_FAILURE directly to upper layer
		return NDIS_STATUS_FAILURE;
	}

	if (pSkb && pAd->PortCfg.BssType == BSS_MONITOR &&
		   pAd->bAcceptRFMONTx == TRUE)
	{
		skb_queue_tail(&pAd->SendTxWaitQueue[QID_AC_BE], pSkb);
		return (NDIS_STATUS_SUCCESS);
	}

	//
	// Check for multicast or broadcast (First byte of DA)
	//
	if ((*((PUCHAR) pSrcBufVA) & 0x01) != 0)
	{
		// For multicast & broadcast, there is no fragment allowed
		NumberOfFrag = 1;
	}
#if 0 //AGGREGATION_SUPPORT
	else if (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_AGGREGATION_INUSED))
	{
		NumberOfFrag = 1;	// Aggregation overwhelms fragmentation
	}
#endif
	else
	{
		// Check for payload allowed for each fragment
		AllowFragSize = (pAd->PortCfg.FragmentThreshold) - LENGTH_802_11 - LENGTH_CRC;

		// Calculate fragments required
		NumberOfFrag = ((pSkb->len - LENGTH_802_3 + LENGTH_802_1_H) / AllowFragSize) + 1;
		// Minus 1 if the size just match to allowable fragment size
		if (((pSkb->len - LENGTH_802_3 + LENGTH_802_1_H) % AllowFragSize) == 0)
		{
			NumberOfFrag--;
		}
	}

	// Save fragment number to Ndis packet reserved field
	RTMP_SET_PACKET_FRAGMENTS(pSkb, NumberOfFrag);


	// STEP 2. Check the requirement of RTS:
	//	   If multiple fragment required, RTS is required only for the first fragment
	//	   if the fragment size large than RTS threshold

	if (NumberOfFrag > 1)
		RTSRequired = (pAd->PortCfg.FragmentThreshold > pAd->PortCfg.RtsThreshold) ? 1 : 0;
	else
		RTSRequired = (pSkb->len > pAd->PortCfg.RtsThreshold) ? 1 : 0;

    //
	// Remove the following lines to avoid confusion.
	// CTS requirement will not use Flag "RTSRequired", instead moveing the
	// following lines to RTUSBHardTransmit(..)
	//
	// RTS/CTS may also be required in order to protect OFDM frame
	//if ((pAd->PortCfg.TxRate >= RATE_FIRST_OFDM_RATE) &&
	//	OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_BG_PROTECTION_INUSED))
	//	RTSRequired = 1;

	// Save RTS requirement to Ndis packet reserved field
	RTMP_SET_PACKET_RTS(pSkb, RTSRequired);
	RTMP_SET_PACKET_TXRATE(pSkb, pAd->PortCfg.TxRate);


	//
	// STEP 3. Traffic classification. outcome = <UserPriority, QueIdx>
	//
	UserPriority = 0;
	QueIdx		 = QID_AC_BE;
	if (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_WMM_INUSED))
	{
		USHORT Protocol;
		UCHAR  LlcSnapLen = 0, Byte0, Byte1;
		do
		{
			// get Ethernet protocol field
			Protocol = (USHORT)((pSrcBufVA[12] << 8) + pSrcBufVA[13]);
			if (Protocol <= 1500)
			{
				// get Ethernet protocol field from LLC/SNAP
				if (Sniff2BytesFromNdisBuffer(pSkb, LENGTH_802_3 + 6, &Byte0, &Byte1) != NDIS_STATUS_SUCCESS)
					break;

				Protocol = (USHORT)((Byte0 << 8) + Byte1);
				LlcSnapLen = 8;
			}

			// always AC_BE for non-IP packet
			if (Protocol != 0x0800)
				break;

			// get IP header
			if (Sniff2BytesFromNdisBuffer(pSkb, LENGTH_802_3 + LlcSnapLen, &Byte0, &Byte1) != NDIS_STATUS_SUCCESS)
				break;

			// return AC_BE if packet is not IPv4
			if ((Byte0 & 0xf0) != 0x40)
				break;

			UserPriority = (Byte1 & 0xe0) >> 5;
			QueIdx = MapUserPriorityToAccessCategory[UserPriority];

			// TODO: have to check ACM bit. apply TSPEC if ACM is ON
			// TODO: downgrade UP & QueIdx before passing ACM
			if (pAd->PortCfg.APEdcaParm.bACM[QueIdx])
			{
				UserPriority = 0;
				QueIdx		 = QID_AC_BE;
			}
		} while (FALSE);
	}

	RTMP_SET_PACKET_UP(pSkb, UserPriority);

	pTxQueue = &pAd->SendTxWaitQueue[QueIdx];

	//
	// For infrastructure mode, enqueue this frame immediately to sendwaitqueue
	// For Ad-hoc mode, check the DA power state, then decide which queue to enqueue
	//
	if (INFRA_ON(pAd))
	{
		// In infrastructure mode, simply enqueue the packet into Tx waiting queue.
		DBGPRINT(RT_DEBUG_INFO, "Infrastructure -> Enqueue one frame\n");

		// Enqueue Ndis packet to end of Tx wait queue
		skb_queue_tail(pTxQueue, pSkb);
		Status = NDIS_STATUS_SUCCESS;
#ifdef DBG
        pAd->RalinkCounters.OneSecOsTxCount[QueIdx]++;  // TODO: for debug only. to be removed
#endif
	}
	else
	{
		// In IBSS mode, power state of destination should be considered.
		PsMode = PWR_ACTIVE;		// Faked
		if (PsMode == PWR_ACTIVE)
		{
			DBGPRINT(RT_DEBUG_INFO,"-  %s(Ad-Hoc) Enqueue one frame\n",
					__FUNCTION__);

			// Enqueue Ndis packet to end of Tx wait queue
			skb_queue_tail(pTxQueue, pSkb);
			Status = NDIS_STATUS_SUCCESS;
#ifdef DBG
            pAd->RalinkCounters.OneSecOsTxCount[QueIdx]++;   // TODO: for debug only. to be removed
#endif
		}
	}
	return (Status);
} /* End RTMPSendPacket () */

/*
	========================================================================

	Routine Description:
		SendPackets handler

	Arguments:
		skb 			point to sk_buf which upper layer transmit
		net_dev 		point to net_dev
	Return Value:
		None

	Note:

	========================================================================
*/
INT RTMPSendPackets(
	IN	struct sk_buff		*pSkb,
	IN	struct net_device	*net_dev)
{
	PRTMP_ADAPTER	pAd = net_dev->priv;
	NDIS_STATUS 	Status = NDIS_STATUS_SUCCESS;
	//INT 			Index;

	DBGPRINT(RT_DEBUG_INFO, "===> RTMPSendPackets\n");

	if (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_RESET_IN_PROGRESS) ||
		RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_HALT_IN_PROGRESS))
	{
		// Drop send request since hardware is in reset state
		RTUSBFreeSkbBuffer(pSkb);
		return 0;
	}
	// Drop packets if no associations
	else if (!INFRA_ON(pAd) && !ADHOC_ON(pAd) && !(pAd->PortCfg.BssType == BSS_MONITOR && pAd->bAcceptRFMONTx == TRUE))
	{
		RTUSBFreeSkbBuffer(pSkb);
		return 0;
	}
	else
	{
		// initial pSkb->data_len=0, we will use this variable to store data size when fragment(in TKIP)
		// and pSkb->len is actual data len
		pSkb->data_len = pSkb->len;

		// Record that orignal packet source is from protocol layer,so that
		// later on driver knows how to release this skb buffer
		RTMP_SET_PACKET_SOURCE(pSkb, PKTSRC_NDIS);
		pAd->RalinkCounters.PendingNdisPacketCount ++;

		Status = RTMPSendPacket(pAd, pSkb);
		if (Status != NDIS_STATUS_SUCCESS)
		{
			// Errors before enqueue stage
			RELEASE_NDIS_PACKET(pAd, pSkb);
			DBGPRINT(RT_DEBUG_TRACE,"<---RTUSBSendPackets not dequeue\n");
			return 0;
		}
	}
	RTUSBMlmeUp(pAd);

	return 0;
}

/*
	========================================================================

	Routine	Description:
		Copy frame from waiting queue into relative ring buffer and set
	appropriate ASIC register to kick hardware encryption before really
	sent out to air.

	Arguments:
		pAd				Pointer	to our adapter
		PNDIS_PACKET	Pointer to outgoing Ndis frame
		NumberOfFrag	Number of fragment required

	Return Value:
		None

	Note:

	========================================================================
*/
#ifdef BIG_ENDIAN
static inline
#endif
NDIS_STATUS RTUSBHardTransmit(
	IN	PRTMP_ADAPTER	pAd,
	IN	struct sk_buff	*pSkb,
	IN	UCHAR			NumberRequired,
	IN	UCHAR			QueIdx)
{
	UINT			LengthQosPAD =0;
	UINT			BytesCopied;
	UINT			TxSize;
	UINT			FreeMpduSize;
	UINT			SrcRemainingBytes;
	USHORT			Protocol;
	UCHAR			FrameGap;
	HEADER_802_11	Header_802_11;
	PHEADER_802_11	pHeader80211;
	PUCHAR			pDest;
//	PUCHAR			pSrc;
	PTX_CONTEXT		pTxContext;
	PTXD_STRUC		pTxD;
//	PURB			pUrb;
	BOOLEAN			StartOfFrame;
	BOOLEAN			bEAPOLFrame;
	ULONG			Iv16;
	ULONG			Iv32;
	BOOLEAN			MICFrag;
//	PCIPHER_KEY		pWpaKey = NULL;
	BOOLEAN			Cipher;
	ULONG			TransferBufferLength;
	USHORT			AckDuration = 0;
	USHORT			EncryptionOverhead = 0;
	UCHAR			CipherAlg;
	BOOLEAN			bAckRequired;
	UCHAR			RetryMode = SHORT_RETRY;
	UCHAR			UserPriority;
	UCHAR			MpduRequired, RtsRequired;
	UCHAR			TxRate;
	PCIPHER_KEY		pKey = NULL ;
	PUCHAR			pSrcBufVA = NULL;
	ULONG			SrcBufLen;
	PUCHAR			pExtraLlcSnapEncap = NULL; // NULL: no extra LLC/SNAP is required
	UCHAR			KeyIdx;
	PUCHAR			pWirelessPacket;
	ULONG			NextMpduSize;
	BOOLEAN			bRTS_CTSFrame = FALSE;
	unsigned long flags;	// For 'Ndis' spin lock

    if ((pAd->PortCfg.bIEEE80211H == 1) && (pAd->PortCfg.RadarDetect.RDMode != RD_NORMAL_MODE))
    {
        DBGPRINT(RT_DEBUG_INFO,"RTUSBHardTransmit --> radar detect not in normal mode !!!\n");
        return (NDIS_STATUS_FAILURE);
    }

	if (pAd->PortCfg.BssType == BSS_MONITOR && pAd->bAcceptRFMONTx == TRUE)
	{
		DBGPRINT(RT_DEBUG_INFO,"==>INJECT\n");
		pTxContext	= &pAd->TxContext[QueIdx][pAd->NextTxIndex[QueIdx]];

		if (pAd->TxRingTotalNumber[QueIdx] >= TX_RING_SIZE)
		{
			//Modified by Thomas
			DBGPRINT_ERR("RTUSBHardTransmit: TX RING full\n");
			//pAd->RalinkCounters.TxRingErrCount++;

			//return (NDIS_STATUS_RESOURCES);
			return (NDIS_STATUS_RINGFULL);
		}

		pTxContext->InUse	= TRUE;
		pTxContext->LastOne	= TRUE;

		// Increase & maintain Tx Ring Index
		pAd->NextTxIndex[QueIdx]++;
		if (pAd->NextTxIndex[QueIdx] >= TX_RING_SIZE)
		{
			pAd->NextTxIndex[QueIdx] = 0;
		}

		pTxD  = (PTXD_STRUC) &pTxContext->TransferBuffer->TxDesc;
		memset(pTxD, 0, sizeof(TXD_STRUC));
		pWirelessPacket = pTxContext->TransferBuffer->WirelessPacket;
		memcpy( pWirelessPacket, pSkb->data, pSkb->len );
		TxSize = pSkb->len;				//+FCS

		/*for (i=0; i<TxSize; i++)
			DBGPRINT(RT_DEBUG_INFO,"RTUSBHardTransmit --> byte %u is %x\n", i, pWirelessPacket[i]);*/

		RTUSBWriteTxDescriptor(pAd, pTxD, CIPHER_NONE /*CipherAlg*/, 0, 0xff /*KeyIdx*/,
					FALSE /*bAckRequired*/, FALSE, FALSE, 4 /*RetryMode*/,
					IFS_BACKOFF /*FrameGap*/, RTMP_GET_PACKET_TXRATE(pSkb) /*TxRate*/,
					TxSize, QueIdx, 0, FALSE /*bRTS_CTSFrame*/);

		TransferBufferLength = TxSize + sizeof(TXD_STRUC);

		if ((TransferBufferLength % 4) == 1)
			TransferBufferLength  += 3;
		else if ((TransferBufferLength % 4) == 2)
			TransferBufferLength  += 2;
		else if ((TransferBufferLength % 4) == 3)
			TransferBufferLength  += 1;

		if ((TransferBufferLength % pAd->BulkOutMaxPacketSize) == 0)
			TransferBufferLength += 4;

		pTxContext->BulkOutSize = TransferBufferLength;
		pTxContext->bWaitingBulkOut = TRUE;
		RTUSB_SET_BULK_FLAG(pAd, (fRTUSB_BULK_OUT_DATA_NORMAL << QueIdx));

		pAd->TxRingTotalNumber[QueIdx]++;	// sync. to PendingTx
		RELEASE_NDIS_PACKET(pAd, pSkb);

		return (NDIS_STATUS_SUCCESS);
	}

	TxRate		 = RTMP_GET_PACKET_TXRATE(pSkb);
	MpduRequired = RTMP_GET_PACKET_FRAGMENTS(pSkb);
	RtsRequired  = RTMP_GET_PACKET_RTS(pSkb);
	UserPriority = RTMP_GET_PACKET_UP(pSkb);

	//
	// Prepare packet information structure which will be query for buffer descriptor
	//
	pSrcBufVA = (PVOID)pSkb->data;
	SrcBufLen = pSkb->len;

	// Check for virtual address allocation, it might fail !!!
	if (pSrcBufVA == NULL)
	{
		DBGPRINT(RT_DEBUG_TRACE, "pSrcBufVA == NULL\n");
		return(NDIS_STATUS_RESOURCES);
	}
	if (SrcBufLen < 14)
	{
		DBGPRINT(RT_DEBUG_ERROR, "RTUSBHardTransmit --> Skb buffer error !!!\n");
		return (NDIS_STATUS_FAILURE);
	}

	//
	// If DHCP datagram or ARP datagram , we need to send it as Low rates.
	//
	if (pAd->PortCfg.Channel <= 14)
	{
		//
		// Case 802.11 b/g
		// basic channel means that we can use CCKM's low rate as RATE_1.
		//
		if ((TxRate != RATE_1) && RTMPCheckDHCPFrame(pAd, pSkb))
			TxRate = RATE_1;
	}
	else
	{
		//
		// Case 802.11a
		// We must used OFDM's low rate as RATE_6, note RATE_1 is not allow
		// Only OFDM support on Channel > 14
		//
		if ((TxRate != RATE_6) && RTMPCheckDHCPFrame(pAd, pSkb))
			TxRate = RATE_6;
	}

	// ------------------------------------------
	// STEP 0.1 Add 802.1x protocol check.
	// ------------------------------------------
	// For non-WPA network, 802.1x message should not encrypt even privacy is on.
	if (NdisEqualMemory(EAPOL, pSrcBufVA + 12, 2))
	{
		bEAPOLFrame = TRUE;
		if (pAd->PortCfg.MicErrCnt >= 2)
			pAd->PortCfg.MicErrCnt++;
	}
	else
		bEAPOLFrame = FALSE;

	//
	// WPA 802.1x secured port control - drop all non-802.1x frame before port secured
	//
	if (((pAd->PortCfg.AuthMode == Ndis802_11AuthModeWPA)	 ||
		 (pAd->PortCfg.AuthMode == Ndis802_11AuthModeWPAPSK) ||
		 (pAd->PortCfg.AuthMode == Ndis802_11AuthModeWPA2)	 ||
		 (pAd->PortCfg.AuthMode == Ndis802_11AuthModeWPA2PSK)
#if WPA_SUPPLICANT_SUPPORT
		  || (pAd->PortCfg.IEEE8021X == TRUE)
#endif
         ) &&
		((pAd->PortCfg.PortSecured == WPA_802_1X_PORT_NOT_SECURED) || (pAd->PortCfg.MicErrCnt >= 2)) &&
		(bEAPOLFrame == FALSE))
	{
		DBGPRINT(RT_DEBUG_INFO, "RTUSBHardTransmit --> Drop packet before port secured !!!\n");
		return (NDIS_STATUS_FAILURE);
	}

	if (*pSrcBufVA & 0x01) // Multicast or Broadcast
	{
		bAckRequired = FALSE;
		INC_COUNTER64(pAd->WlanCounters.MulticastTransmittedFrameCount);
		Cipher = pAd->PortCfg.GroupCipher; // Cipher for Multicast or Broadcast
	}
	else
	{
		bAckRequired = TRUE;
		Cipher = pAd->PortCfg.PairCipher; // Cipher for Unicast
	}

	// 1. traditional TX burst
	if (pAd->PortCfg.bEnableTxBurst && (pAd->Sequence & 0x7))
		FrameGap = IFS_SIFS;
	// 2. frame belonging to AC that has non-zero TXOP
	else if (pAd->PortCfg.APEdcaParm.bValid && pAd->PortCfg.APEdcaParm.Txop[QueIdx])
		FrameGap = IFS_SIFS;
	// 3. otherwise, always BACKOFF before transmission
	else
		FrameGap = IFS_BACKOFF;		// Default frame gap mode

	Protocol = *(pSrcBufVA + 12) * 256 + *(pSrcBufVA + 13);
	// if orginal Ethernet frame contains no LLC/SNAP, then an extra LLC/SNAP encap is required
	if (Protocol > 1500)
	{
		pExtraLlcSnapEncap = SNAP_802_1H;
		if (NdisEqualMemory(IPX, pSrcBufVA + 12, 2) ||
			NdisEqualMemory(APPLE_TALK, pSrcBufVA + 12, 2))
		{
			pExtraLlcSnapEncap = SNAP_BRIDGE_TUNNEL;
		}
	}
	else
		pExtraLlcSnapEncap = NULL;


    // Update software power save state
	OPSTATUS_CLEAR_FLAG(pAd, fOP_STATUS_DOZE);
	pAd->PortCfg.Psm = PWR_ACTIVE;

	// -----------------------------------------------------------------
	// STEP 2. MAKE A COMMON 802.11 HEADER SHARED BY ENTIRE FRAGMENT BURST.
	// -----------------------------------------------------------------

	pAd->Sequence = ((pAd->Sequence) + 1) & (MAX_SEQ_NUMBER);
	MAKE_802_11_HEADER(pAd, Header_802_11, pSrcBufVA, pAd->Sequence);
#if 0
	if (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_WMM_INUSED))
		Header_802_11.FC.SubType = SUBTYPE_QDATA;
#endif

	// --------------------------------------------------------
	// STEP 3. FIND ENCRYPT KEY AND DECIDE CIPHER ALGORITHM
	//		Find the WPA key, either Group or Pairwise Key
	//		LEAP + TKIP also use WPA key.
	// --------------------------------------------------------
	// Decide WEP bit and cipher suite to be used. Same cipher suite should be used for whole fragment burst
	// In Cisco CCX 2.0 Leap Authentication
	//		   WepStatus is Ndis802_11Encryption1Enabled but the key will use PairwiseKey
	//		   Instead of the SharedKey, SharedKey Length may be Zero.
	KeyIdx = 0xff;
	if (bEAPOLFrame)
	{
        ASSERT(pAd->SharedKey[0].CipherAlg <= CIPHER_CKIP128);
        if ((pAd->SharedKey[0].CipherAlg) &&
            (pAd->SharedKey[0].KeyLen) )
        {
            CipherAlg = pAd->SharedKey[0].CipherAlg;
            KeyIdx = 0;
        }
    }
	else if (Cipher == Ndis802_11Encryption1Enabled)
	{
			// standard WEP64 or WEP128
		KeyIdx = pAd->PortCfg.DefaultKeyId;
	}
	else if ((Cipher == Ndis802_11Encryption2Enabled) ||
			 (Cipher == Ndis802_11Encryption3Enabled))
	{
		if (Header_802_11.Addr1[0] & 0x01) // multicast
			KeyIdx = pAd->PortCfg.DefaultKeyId;
		else if (pAd->SharedKey[0].KeyLen)
			KeyIdx = 0;
		else
			KeyIdx = pAd->PortCfg.DefaultKeyId;
	}

	if (KeyIdx == 0xff)
		CipherAlg = CIPHER_NONE;
	else if (pAd->SharedKey[KeyIdx].KeyLen == 0)
		CipherAlg = CIPHER_NONE;
	else
	{
		Header_802_11.FC.Wep = 1;
		CipherAlg = pAd->SharedKey[KeyIdx].CipherAlg;
		pKey = &pAd->SharedKey[KeyIdx];
	}

	DBGPRINT(RT_DEBUG_INFO,"RTUSBHardTransmit(bEAP=%d) - %s key#%d, KeyLen=%d\n",
		bEAPOLFrame, CipherName[CipherAlg], KeyIdx, pAd->SharedKey[KeyIdx].KeyLen);


	// STEP 3.1 if TKIP is used and fragmentation is required. Driver has to
	//			append TKIP MIC at tail of the scatter buffer (This must be the
	//			ONLY scatter buffer in the skb buffer).
	//			MAC ASIC will only perform IV/EIV/ICV insertion but no TKIP MIC
	if ((MpduRequired > 1) && (CipherAlg == CIPHER_TKIP))
	{
		pSkb->len += 8;
		CipherAlg = CIPHER_TKIP_NO_MIC;
	}

	// ----------------------------------------------------------------
	// STEP 4. Make RTS frame or CTS-to-self frame if required
	// ----------------------------------------------------------------

	//
	// calcuate the overhead bytes that encryption algorithm may add. This
	// affects the calculate of "duration" field
	//
	if ((CipherAlg == CIPHER_WEP64) || (CipherAlg == CIPHER_WEP128))
		EncryptionOverhead = 8; //WEP: IV[4] + ICV[4];
	else if (CipherAlg == CIPHER_TKIP_NO_MIC)
		EncryptionOverhead = 12;//TKIP: IV[4] + EIV[4] + ICV[4], MIC will be added to TotalPacketLength
	else if (CipherAlg == CIPHER_TKIP)
		EncryptionOverhead = 20;//TKIP: IV[4] + EIV[4] + ICV[4] + MIC[8]
	else if (CipherAlg == CIPHER_AES)
		EncryptionOverhead = 16;	// AES: IV[4] + EIV[4] + MIC[8]
	else
		EncryptionOverhead = 0;

	// decide how much time an ACK/CTS frame will consume in the air
	AckDuration = RTMPCalcDuration(pAd, pAd->PortCfg.ExpectedACKRate[TxRate], 14);

	// If fragment required, MPDU size is maximum fragment size
	// Else, MPDU size should be frame with 802.11 header & CRC
	if (MpduRequired > 1)
		NextMpduSize = pAd->PortCfg.FragmentThreshold;
	else
	{
		NextMpduSize = pSkb->len + LENGTH_802_11 + LENGTH_CRC - LENGTH_802_3;
		if (pExtraLlcSnapEncap)
			NextMpduSize += LENGTH_802_1_H;
	}

	if (RtsRequired || OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_RTS_PROTECTION_ENABLE))
	{
		RTMPSendRTSCTSFrame(pAd,
							Header_802_11.Addr1,
							NextMpduSize + EncryptionOverhead,
							TxRate,
							pAd->PortCfg.RtsRate,
							AckDuration,
							QueIdx,
							FrameGap,
							SUBTYPE_RTS);

		// RTS/CTS-protected frame should use LONG_RETRY (=4) and SIFS
		RetryMode = LONG_RETRY;
		FrameGap = IFS_SIFS;
		bRTS_CTSFrame = TRUE;

		if (RtsRequired)
			NumberRequired--;
	}
	else if ((pAd->PortCfg.TxRate >= RATE_FIRST_OFDM_RATE) && OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_BG_PROTECTION_INUSED))
	{
		RTMPSendRTSCTSFrame(pAd,
							Header_802_11.Addr1,
							NextMpduSize + EncryptionOverhead,
							TxRate,
							pAd->PortCfg.RtsRate,
							AckDuration,
							QueIdx,
							FrameGap,
							SUBTYPE_CTS);

		// RTS/CTS-protected frame should use LONG_RETRY (=4) and SIFS
		RetryMode = LONG_RETRY;
		FrameGap = IFS_SIFS;
		bRTS_CTSFrame = TRUE;
	}

	// --------------------------------------------------------
	// STEP 5. START MAKING MPDU(s)
	//		Start Copy Ndis Packet into Ring buffer.
	//		For frame required more than one ring buffer (fragment), all ring buffers
	//		have to be filled before kicking start tx bit.
	//		Make sure TX ring resource won't be used by other threads
	// --------------------------------------------------------
	SrcRemainingBytes = pSkb->len - LENGTH_802_3;
	SrcBufLen		 -= LENGTH_802_3;  // skip 802.3 header


	StartOfFrame = TRUE;
	MICFrag = FALSE;	// Flag to indicate MIC shall spread into two MPDUs

	// Start Copy Ndis Packet into Ring buffer.
	// For frame required more than one ring buffer (fragment), all ring buffers
	// have to be filled before kicking start tx bit.

	do
	{
		//
		// STEP 5.1 Get the Tx Ring descriptor & Dma Buffer address
		//
		pTxContext	= &pAd->TxContext[QueIdx][pAd->NextTxIndex[QueIdx]];

		if ((pTxContext->bWaitingBulkOut == TRUE) || (pTxContext->InUse == TRUE) ||
			(pAd->TxRingTotalNumber[QueIdx] >= TX_RING_SIZE))
		{
			//Modified by Thomas
			DBGPRINT_ERR("RTUSBHardTransmit: TX RING full\n");
			//pAd->RalinkCounters.TxRingErrCount++;
			//return (NDIS_STATUS_RESOURCES);
			return (NDIS_STATUS_RINGFULL);
		}
		pTxContext->InUse	= TRUE;

		// Increase & maintain Tx Ring Index
		pAd->NextTxIndex[QueIdx]++;
		if (pAd->NextTxIndex[QueIdx] >= TX_RING_SIZE)
		{
			pAd->NextTxIndex[QueIdx] = 0;
		}

		pTxD  = (PTXD_STRUC) &pTxContext->TransferBuffer->TxDesc;
		memset(pTxD, 0, sizeof(TXD_STRUC));
		pWirelessPacket = pTxContext->TransferBuffer->WirelessPacket;

		//
		// STEP 5.2 PUT IVOFFSET, IV, EIV INTO TXD
		//
		pTxD->IvOffset	= LENGTH_802_11;

#if 0
		if (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_WMM_INUSED))
			pTxD->IvOffset += 2;  // add QOS CONTROL bytes
#endif

		if ((CipherAlg == CIPHER_WEP64) || (CipherAlg == CIPHER_WEP128))
		{
			PUCHAR pTmp;
			pTmp = (PUCHAR) &pTxD->Iv;
			*pTmp		= RandomByte(pAd);
			*(pTmp + 1) = RandomByte(pAd);
			*(pTmp + 2) = RandomByte(pAd);
			*(pTmp + 3) = (KeyIdx << 6);
		}
		else if ((CipherAlg == CIPHER_TKIP) || (CipherAlg == CIPHER_TKIP_NO_MIC))
		{
			RTMPInitTkipEngine(
				pAd,
				pKey->Key,
				KeyIdx,		// This might cause problem when using peer key
				Header_802_11.Addr2,
				pKey->TxMic,
				pKey->TxTsc,
				&Iv16,
				&Iv32);

			memcpy(&pTxD->Iv, &Iv16, 4);   // Copy IV
			memcpy(&pTxD->Eiv, &Iv32, 4);  // Copy EIV
			INC_TX_TSC(pKey->TxTsc);			   // Increase TxTsc for next transmission
		}
		else if (CipherAlg == CIPHER_AES)
		{
			PUCHAR	pTmp;
			pTmp = (PUCHAR) &Iv16;
			*pTmp		= pKey->TxTsc[0];
			*(pTmp + 1) = pKey->TxTsc[1];
			*(pTmp + 2) = 0;
			*(pTmp + 3) = (pAd->PortCfg.DefaultKeyId << 6) | 0x20;

			memcpy(&pTxD->Iv, &Iv16, 4);	// Copy IV
			// Copy EIV
			memcpy(&pTxD->Eiv,
			       (void *)(&pKey->TxTsc[2]), 4);
			INC_TX_TSC(pKey->TxTsc);				// Increase TxTsc for next transmission
		}


		//
		// STEP 5.3 COPY 802.11 HEADER INTO 1ST DMA BUFFER
		//
		pDest = pWirelessPacket;
		memcpy(pDest, &Header_802_11, sizeof(Header_802_11));
		pDest		+= sizeof(Header_802_11);

		//
		// Fragmentation is not allowed on multicast & broadcast
		// So, we need to used the MAX_FRAG_THRESHOLD instead of pAd->PortCfg.FragmentThreshold
		// otherwise if pSkb->len > pAd->PortCfg.FragmentThreshold then
		// packet will be fragment on multicast & broadcast.
		//
		// MpduRequired equals to 1 means this could be Aggretaion case.
		//
		if ((Header_802_11.Addr1[0] & 0x01) || MpduRequired == 1)
		{
			FreeMpduSize = MAX_FRAG_THRESHOLD - sizeof(Header_802_11) - LENGTH_CRC;
		}
		else
		{
			FreeMpduSize = pAd->PortCfg.FragmentThreshold - sizeof(Header_802_11) - LENGTH_CRC;
		}

#if 0
		if (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_WMM_INUSED))
		{
			// copy QOS CONTROL bytes
			*pDest		  =  (UserPriority & 0x0f) | pAd->PortCfg.AckPolicy[QueIdx];
			*(pDest+1)	  =  0;
			pDest		  += 2;
			FreeMpduSize  -= 2;
			if (pAd->PortCfg.AckPolicy[QueIdx] != NORMAL_ACK)
			{
				bAckRequired = FALSE;
			}
		}
#endif

		//
		// STEP 5.4 COPY LLC/SNAP, CKIP MIC INTO 1ST DMA BUFFER ONLY WHEN THIS
		//			MPDU IS THE 1ST OR ONLY FRAGMENT
		//
		if (Header_802_11.Frag == 0)
		{
			if (pExtraLlcSnapEncap)
			{
				if ((CipherAlg == CIPHER_TKIP_NO_MIC) && (pKey != NULL))
				{
					// Calculate MSDU MIC Value
					RTMPCalculateMICValue(pAd, pSkb, pExtraLlcSnapEncap, pKey);
				}

				// Insert LLC-SNAP encapsulation
				memcpy(pDest, pExtraLlcSnapEncap, 6);
				pDest += 6;
				memcpy(pDest, pSrcBufVA + 12, 2);
				pDest += 2;
				pSrcBufVA += LENGTH_802_3;
				FreeMpduSize -= LENGTH_802_1_H;

			}
			else
			{
				if ((CipherAlg == CIPHER_TKIP_NO_MIC) && (pKey != NULL))
				{
					// Calculate MSDU MIC Value
					RTMPCalculateMICValue(pAd, pSkb, pExtraLlcSnapEncap, pKey);
				}
				pSrcBufVA += LENGTH_802_3;
			}
		}

		// Start copying payload
		BytesCopied = 0;
		do
		{
			if (SrcBufLen >= FreeMpduSize)
			{
				// Copy only the free fragment size, and save the pointer
				// of current buffer descriptor for next fragment buffer.
				memcpy(pDest, pSrcBufVA, FreeMpduSize);
				BytesCopied += FreeMpduSize;
				pSrcBufVA	+= FreeMpduSize;
				pDest		+= FreeMpduSize;
				SrcBufLen	-= FreeMpduSize;
				break;
			}
			else
			{
				// Copy the rest of this buffer descriptor pointed data
				// into ring buffer.
				memcpy(pDest, pSrcBufVA, SrcBufLen);
				BytesCopied  += SrcBufLen;
				pDest		 += SrcBufLen;
				FreeMpduSize -= SrcBufLen;
			}

			// No more buffer descriptor
			// Add MIC value if needed

			//if ((pAd->PortCfg.WepStatus == Ndis802_11Encryption2Enabled) &&
			//	(MICFrag == FALSE) &&
			//	(pKey != NULL))

			if((CipherAlg == CIPHER_TKIP_NO_MIC) &&
			   (MICFrag == FALSE) &&
				(pKey != NULL))
			{
				// Fregment and TKIP//
				INT i;

				SrcBufLen = 8;		// Set length to MIC length
				DBGPRINT(RT_DEBUG_INFO, "Calculated TX MIC value =");
				for (i = 0; i < 8; i++)
				{
					DBGPRINT_RAW(RT_DEBUG_INFO, "%02x:", pAd->PrivateInfo.Tx.MIC[i]);
				}
				DBGPRINT_RAW(RT_DEBUG_INFO, "\n");

				if (FreeMpduSize >= SrcBufLen)
				{
					memcpy(pDest, pAd->PrivateInfo.Tx.MIC, SrcBufLen);
					BytesCopied  += SrcBufLen;
					pDest		 += SrcBufLen;
					FreeMpduSize -= SrcBufLen;
					SrcBufLen = 0;
				}
				else
				{
					memcpy(pDest, pAd->PrivateInfo.Tx.MIC, FreeMpduSize);
					BytesCopied  += FreeMpduSize;
					pSrcBufVA	  = pAd->PrivateInfo.Tx.MIC + FreeMpduSize;
					pDest		 += FreeMpduSize;
					SrcBufLen	 -= FreeMpduSize;
					MICFrag 	  = TRUE;
				}
			}
		}	while (FALSE); // End of copying payload

		// Real packet size, No 802.1H header for fragments except the first one.
		if ((StartOfFrame == TRUE) && (pExtraLlcSnapEncap != NULL))
		{
			TxSize = BytesCopied + LENGTH_802_11 + LENGTH_802_1_H + LengthQosPAD;
		}
		else
		{
			TxSize = BytesCopied + LENGTH_802_11 + LengthQosPAD;
		}

		SrcRemainingBytes -=  BytesCopied;

		//
		// STEP 5.6 MODIFY MORE_FRAGMENT BIT & DURATION FIELD. WRITE TXD
		//
		pHeader80211 = (PHEADER_802_11)pWirelessPacket;
		if (SrcRemainingBytes > 0) // more fragment is required
		{
			 ULONG NextMpduSize;

			 pHeader80211->FC.MoreFrag = 1;
			 NextMpduSize = min((ULONG)SrcRemainingBytes, (ULONG)pAd->PortCfg.FragmentThreshold);

			 if (NextMpduSize < pAd->PortCfg.FragmentThreshold)
			 {
				// In this case, we need to include LENGTH_802_11 and LENGTH_CRC for calculating Duration.
				pHeader80211->Duration = (3 * pAd->PortCfg.Dsifs) +
									(2 * AckDuration) +
									RTMPCalcDuration(pAd, TxRate, NextMpduSize + EncryptionOverhead + LENGTH_802_11 + LENGTH_CRC);
			 }
			 else
			 {
				pHeader80211->Duration = (3 * pAd->PortCfg.Dsifs) +
								(2 * AckDuration) +
								RTMPCalcDuration(pAd, TxRate, NextMpduSize + EncryptionOverhead);
			 }

#ifdef BIG_ENDIAN
			RTMPFrameEndianChange(pAd, (PUCHAR)pHeader80211, DIR_WRITE, FALSE);
#endif
			RTUSBWriteTxDescriptor(pAd, pTxD, CipherAlg, 0, KeyIdx, bAckRequired, TRUE, FALSE,
					RetryMode, FrameGap, TxRate, TxSize, QueIdx, 0, bRTS_CTSFrame);

			FrameGap = IFS_SIFS;	 // use SIFS for all subsequent fragments
			Header_802_11.Frag ++;	 // increase Frag #
		}
		else
		{
			pHeader80211->FC.MoreFrag = 0;
			if (pHeader80211->Addr1[0] & 0x01) // multicast/broadcast
				pHeader80211->Duration = 0;
			else
				pHeader80211->Duration = pAd->PortCfg.Dsifs + AckDuration;

			if ((bEAPOLFrame) && (TxRate > RATE_6))
				TxRate = RATE_6;

#ifdef BIG_ENDIAN
			RTMPFrameEndianChange(pAd, (PUCHAR)pHeader80211, DIR_WRITE, FALSE);
#endif
			RTUSBWriteTxDescriptor(pAd, pTxD, CipherAlg, 0, KeyIdx, bAckRequired, FALSE, FALSE,
					RetryMode, FrameGap, TxRate, TxSize, QueIdx, 0, bRTS_CTSFrame);

			if (skb_queue_len(&pAd->SendTxWaitQueue[QueIdx]) > 1)
				pTxD->Burst = 1;

		}

		TransferBufferLength = TxSize + sizeof(TXD_STRUC);

		if ((TransferBufferLength % 4) == 1)
			TransferBufferLength  += 3;
		else if ((TransferBufferLength % 4) == 2)
			TransferBufferLength  += 2;
		else if ((TransferBufferLength % 4) == 3)
			TransferBufferLength  += 1;


		if ((TransferBufferLength % pAd->BulkOutMaxPacketSize) == 0)
			TransferBufferLength += 4;

		pTxContext->BulkOutSize = TransferBufferLength;
		pTxContext->bWaitingBulkOut = TRUE;
		RTUSB_SET_BULK_FLAG(pAd, (fRTUSB_BULK_OUT_DATA_NORMAL << QueIdx));

		// Set frame gap for the rest of fragment burst.
		// It won't matter if there is only one fragment (single fragment frame).
		StartOfFrame = FALSE;
		NumberRequired--;
		if (NumberRequired == 0)
		{
			pTxContext->LastOne = TRUE;
		}
		else
		{
			pTxContext->LastOne = FALSE;
		}

		pAd->TxRingTotalNumber[QueIdx]++;	// sync. to PendingTx

	}	while (NumberRequired > 0);


	//
	// Check if MIC error twice within 60 seconds and send EAPOL MIC error to TX queue
	// then we enqueue a message for disasociating with the current AP
	//

	// Check for EAPOL frame sent after MIC countermeasures
	if (pAd->PortCfg.MicErrCnt >= 3)
	{
		MLME_DISASSOC_REQ_STRUCT	DisassocReq;

		// disassoc from current AP first
		DBGPRINT(RT_DEBUG_INFO, "- (%s) disassociate with current AP after"
				" sending second continuous EAPOL frame\n",
				__FUNCTION__);
		DisassocParmFill(pAd, &DisassocReq, pAd->PortCfg.Bssid, REASON_MIC_FAILURE);
		MlmeEnqueue(pAd, ASSOC_STATE_MACHINE, MT2_MLME_DISASSOC_REQ,
					sizeof(MLME_DISASSOC_REQ_STRUCT), &DisassocReq);

		pAd->Mlme.CntlMachine.CurrState = CNTL_WAIT_DISASSOC;
		pAd->PortCfg.bBlockAssoc = TRUE;
	}


	// release the skb buffer
	RELEASE_NDIS_PACKET(pAd, pSkb);

	return (NDIS_STATUS_SUCCESS);

} /* End RTUSBHardTransmit () */

/*
	========================================================================

	Routine	Description:
		Copy frame from waiting queue into relative ring buffer and set
	appropriate ASIC register to kick hardware transmit function

	Arguments:
		pAd			Pointer	to our adapter
		pBuffer		Pointer to	memory of outgoing frame
		Length		Size of outgoing management frame

	Return Value:
		NDIS_STATUS_FAILURE
		NDIS_STATUS_PENDING
		NDIS_STATUS_SUCCESS

	Note:

	========================================================================
*/
VOID	RTUSBMlmeHardTransmit(
	IN	PRTMP_ADAPTER	pAd,
	IN	PMGMT_STRUC		pMgmt)
{
	PTX_CONTEXT		pMLMEContext;
	PTXD_STRUC		pTxD;
	PUCHAR			pDest;
	PHEADER_802_11	pHeader_802_11;
	BOOLEAN 		AckRequired, InsertTimestamp;
	ULONG			TransferBufferLength;
	PVOID			pBuffer = pMgmt->pBuffer;
	ULONG			Length = pMgmt->Length;
	UCHAR			QueIdx;
	UCHAR			MlmeRate;
	unsigned long flags;	// For 'Ndis' spin lock

	DBGPRINT(RT_DEBUG_INFO, "--->RTUSBMlmeHardTransmit\n");

	pAd->PrioRingTxCnt++;

	pMLMEContext = &pAd->MLMEContext[pAd->NextMLMEIndex];
	pMLMEContext->InUse = TRUE;

	// Increase & maintain Tx Ring Index
	pAd->NextMLMEIndex++;
	if (pAd->NextMLMEIndex >= PRIO_RING_SIZE)
	{
		pAd->NextMLMEIndex = 0;
	}

	pDest = pMLMEContext->TransferBuffer->WirelessPacket;

	pTxD = (PTXD_STRUC)(pMLMEContext->TransferBuffer);
	memset(pTxD, 0, sizeof(TXD_STRUC));

	pHeader_802_11 = (PHEADER_802_11) pBuffer;

	// Verify Mlme rate for a / g bands.
    if (pHeader_802_11->Addr1[0] & 0x01)
	{
		MlmeRate = pAd->PortCfg.BasicMlmeRate;
	}
	else
	{
		MlmeRate = pAd->PortCfg.MlmeRate;
	}

	if ((pAd->LatchRfRegs.Channel > 14) && (MlmeRate < RATE_6)) // 11A band
		MlmeRate = RATE_6;

	DBGPRINT(RT_DEBUG_INFO, "- %s: Rate %d Channel %d\n",
			__FUNCTION__, MlmeRate, pAd->LatchRfRegs.Channel );

    // Before radar detection done, mgmt frame can not be sent but probe req
	// Because we need to use probe req to trigger driver to send probe req in passive scan
	if ((pHeader_802_11->FC.SubType != SUBTYPE_PROBE_REQ) && (pAd->PortCfg.bIEEE80211H == 1) && (pAd->PortCfg.RadarDetect.RDMode != RD_NORMAL_MODE))
	{
		DBGPRINT(RT_DEBUG_ERROR, "RTUSBMlmeHardTransmit --> radar detect not in normal mode !!!\n");
		return;
	}


	if (pHeader_802_11->FC.PwrMgmt != PWR_SAVE)
	{
		pHeader_802_11->FC.PwrMgmt = (pAd->PortCfg.Psm == PWR_SAVE);
	}

	InsertTimestamp = FALSE;
	if (pHeader_802_11->FC.Type == BTYPE_CNTL) // must be PS-POLL
	{
		AckRequired = FALSE;
	}
	else // BTYPE_MGMT or BMGMT_DATA(must be NULL frame)
	{
		pAd->Sequence		= ((pAd->Sequence) + 1) & (MAX_SEQ_NUMBER);
		pHeader_802_11->Sequence = pAd->Sequence;

		if (pHeader_802_11->Addr1[0] & 0x01) // MULTICAST, BROADCAST
		{
			INC_COUNTER64(pAd->WlanCounters.MulticastTransmittedFrameCount);
			AckRequired = FALSE;
			pHeader_802_11->Duration = 0;
		}
		else
		{
			AckRequired = TRUE;
			pHeader_802_11->Duration = RTMPCalcDuration(pAd, MlmeRate, 14);
			if (pHeader_802_11->FC.SubType == SUBTYPE_PROBE_RSP)
			{
				InsertTimestamp = TRUE;
			}
		}
	}

	memcpy(pDest, pBuffer, Length);
	pHeader_802_11 = (PHEADER_802_11) pDest;

	// Initialize Priority Descriptor
	// For inter-frame gap, the number is for this frame and next frame
	// For MLME rate, we will fix as 2Mb to match other vendor's implement

	QueIdx = QID_AC_BE;

	// MakeIbssBeacon has made a canned and flipped TxD - bb
	if (pHeader_802_11->FC.SubType != SUBTYPE_BEACON) {
		RTUSBWriteTxDescriptor(pAd, pTxD, CIPHER_NONE, 0, 0,
				AckRequired, FALSE, FALSE, SHORT_RETRY, IFS_BACKOFF,
				MlmeRate, /*Length+4*/ Length, QueIdx, PID_MGMT_FRAME, FALSE);
	}
#ifdef BIG_ENDIAN
	RTMPFrameEndianChange(pAd, (PUCHAR)pHeader_802_11, DIR_WRITE, FALSE);
#endif

	// Build our URB for USBD
	TransferBufferLength = sizeof(TXD_STRUC) + Length;
	if ((TransferBufferLength % 2) == 1)
		TransferBufferLength++;


	if ((TransferBufferLength % pAd->BulkOutMaxPacketSize) == 0)
		TransferBufferLength += 2;

	pMLMEContext->BulkOutSize = TransferBufferLength;
	RTUSB_SET_BULK_FLAG(pAd, fRTUSB_BULK_OUT_MLME);

	DBGPRINT(RT_DEBUG_INFO, "<---RTUSBMlmeHardTransmit\n");

} /* End RTUSBMlmeHardTransmit () */

/*
	========================================================================

	Routine	Description:
		This subroutine will scan through releative ring descriptor to find
		out avaliable free ring descriptor and compare with request size.

	Arguments:
		pAd			Pointer	to our adapter
		RingType	Selected Ring

	Return Value:
		NDIS_STATUS_FAILURE		Not enough free descriptor
		NDIS_STATUS_SUCCESS		Enough free descriptor

	Note:

	========================================================================
*/
NDIS_STATUS	RTUSBFreeDescriptorRequest(
	IN	PRTMP_ADAPTER	pAd,
	IN	UCHAR			RingType,
	IN	UCHAR			BulkOutPipeId,
	IN	UCHAR			NumberRequired)
{
	UCHAR			FreeNumber = 0;
	UINT			Index;
	NDIS_STATUS		Status = NDIS_STATUS_FAILURE;

	switch (RingType)
	{
		case TX_RING:
			Index = (pAd->NextTxIndex[BulkOutPipeId] + 1) % TX_RING_SIZE;
			do
			{
				PTX_CONTEXT	pTxD  = &pAd->TxContext[BulkOutPipeId][Index];

				// While Owner bit is NIC, obviously ASIC still need it.
				// If valid bit is TRUE, indicate that TxDone has not process yet
				// We should not use it until TxDone finish cleanup job
				if (pTxD->InUse == FALSE)
				{
					// This one is free
					FreeNumber++;
				}
				else
				{
					break;
				}
				Index = (Index + 1) % TX_RING_SIZE;
			}	while (FreeNumber < NumberRequired);	// Quit here ! Free number is enough !

			if (FreeNumber >= NumberRequired)
			{
				Status = NDIS_STATUS_SUCCESS;
			}

			break;

		case PRIO_RING:
			Index = pAd->NextMLMEIndex;
			do
			{
				PTX_CONTEXT	pTxD  = &pAd->MLMEContext[Index];

				// While Owner bit is NIC, obviously ASIC still need it.
				// If valid bit is TRUE, indicate that TxDone has not process yet
				// We should not use it until TxDone finish cleanup job
				if (pTxD->InUse == FALSE)
				{
					// This one is free
					FreeNumber++;
				}
				else
				{
					break;
				}

				Index = (Index + 1) % PRIO_RING_SIZE;
			}	while (FreeNumber < NumberRequired);	// Quit here ! Free number is enough !

			if (FreeNumber >= NumberRequired)
			{
				Status = NDIS_STATUS_SUCCESS;
			}
			break;

		default:
			DBGPRINT(RT_DEBUG_ERROR, "--->RTUSBFreeDescriptorRequest() -----!! \n");

			break;
	}

	return (Status);
}

/*
	========================================================================

	Routine Description:

	Arguments:

	Return Value:

	Note:

	========================================================================
*/
VOID	RTUSBRejectPendingPackets(
	IN	PRTMP_ADAPTER	pAd)
{
	UCHAR			Index;

	DBGPRINT(RT_DEBUG_TRACE, "--->RejectPendingPackets\n");

	for (Index = 0; Index < 4; Index++)
	{
		skb_queue_purge(&pAd->SendTxWaitQueue[Index]);
	}

	DBGPRINT(RT_DEBUG_TRACE, "<---RejectPendingPackets\n");
}

/*
	========================================================================

	Routine	Description:
		Calculates the duration which is required to transmit out frames
	with given size and specified rate.

	Arguments:
		pTxD		Pointer to transmit descriptor
		Ack			Setting for Ack requirement bit
		Fragment	Setting for Fragment bit
		RetryMode	Setting for retry mode
		Ifs			Setting for IFS gap
		Rate		Setting for transmit rate
		Service		Setting for service
		Length		Frame length
		TxPreamble	Short or Long preamble when using CCK rates
		QueIdx - 0-3, according to 802.11e/d4.4 June/2003

	Return Value:
		None

	========================================================================
*/
VOID	RTUSBWriteTxDescriptor(
	IN	PRTMP_ADAPTER pAd,
	IN	PTXD_STRUC	pTxD,
	IN	UCHAR		CipherAlg,
	IN	UCHAR		KeyTable,
	IN	UCHAR		KeyIdx,
	IN	BOOLEAN		Ack,
	IN	BOOLEAN		Fragment,
	IN	BOOLEAN 	InsTimestamp,
	IN	UCHAR		RetryMode,
	IN	UCHAR		Ifs,
	IN	UINT		Rate,
	IN	ULONG		Length,
	IN	UCHAR		QueIdx,
	IN	UCHAR		PID,
	IN	BOOLEAN		bAfterRTSCTS)
{
	UINT	Residual;

	pTxD->HostQId	  = QueIdx;
	pTxD->MoreFrag	  = Fragment;
	pTxD->ACK		  = Ack;
	pTxD->Timestamp   = InsTimestamp;
	pTxD->RetryMd	  = RetryMode;
	pTxD->Ofdm		  = (Rate < RATE_FIRST_OFDM_RATE)? 0:1;
	pTxD->IFS		  = Ifs;
	pTxD->PktId 	  = PID;
	pTxD->Drop		  = 1;	 // 1:valid, 0:drop
	pTxD->HwSeq 	  = 1;	  // (QueIdx == QID_MGMT)? 1:0;
	pTxD->BbpTxPower  = DEFAULT_BBP_TX_POWER; // TODO: to be modified
	pTxD->DataByteCnt = Length;

	RTMPCckBbpTuning(pAd, Rate);

	// fill encryption related information, if required
	pTxD->CipherAlg   = CipherAlg;
	if (CipherAlg != CIPHER_NONE)
	{
		pTxD->KeyTable	  = KeyTable;
		pTxD->KeyIndex	  = KeyIdx;
		pTxD->TkipMic	  = 1;
	}

	// In TKIP+fragmentation. TKIP MIC is already appended by driver. MAC needs not generate MIC
	if (CipherAlg == CIPHER_TKIP_NO_MIC)
	{
		pTxD->CipherAlg   = CIPHER_TKIP;
		pTxD->TkipMic	  = 0;	 // tell MAC need not insert TKIP MIC
	}


	if (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_WMM_INUSED))
	{
		if ((pAd->PortCfg.APEdcaParm.bValid) && (QueIdx <= QID_AC_VO))
		{
			pTxD->Cwmin = pAd->PortCfg.APEdcaParm.Cwmin[QueIdx];
			pTxD->Cwmax = pAd->PortCfg.APEdcaParm.Cwmax[QueIdx];
			pTxD->Aifsn = pAd->PortCfg.APEdcaParm.Aifsn[QueIdx];
		}
		else
		{
			DBGPRINT(RT_DEBUG_ERROR," WMM in used but EDCA not valid ERROR !!\n)");
		}
	}
	else
	{
        if (bAfterRTSCTS)
        {
            // After RTS/CTS frame, data frame should use SIFS time.
            // To patch this code, add the following code.
            // Recommended by Jerry 2005/07/25 for WiFi testing with Proxim AP
            pTxD->Cwmin = 0;
            pTxD->Cwmax = 0;
            pTxD->Aifsn = 1;
            pTxD->IFS = IFS_BACKOFF;
        }
        else
        {
            pTxD->Cwmin = CW_MIN_IN_BITS;
            pTxD->Cwmax = CW_MAX_IN_BITS;
            pTxD->Aifsn = 2;
	    }
	}

	// fill up PLCP SIGNAL field
	pTxD->PlcpSignal = RateIdToPlcpSignal[Rate];
	if (((Rate == RATE_2) || (Rate == RATE_5_5) || (Rate == RATE_11)) &&
		(OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_SHORT_PREAMBLE_INUSED)))
	{
		pTxD->PlcpSignal |= 0x0008;
	}

	// fill up PLCP SERVICE field, not used for OFDM rates
	pTxD->PlcpService = 4; // Service;

	// file up PLCP LENGTH_LOW and LENGTH_HIGH fields
	Length += LENGTH_CRC;	// CRC length
	switch (CipherAlg)
	{
		case CIPHER_WEP64:		 Length += 8;	 break;  // IV + ICV
		case CIPHER_WEP128: 	 Length += 8;	 break;  // IV + ICV
		case CIPHER_TKIP:		 Length += 20;	 break;  // IV + EIV + MIC + ICV
		case CIPHER_AES:		 Length += 16;	 break;  // IV + EIV + MIC
		case CIPHER_CKIP64: 	 Length += 8;	 break;  // IV + CMIC + ICV, but CMIC already inserted by driver
		case CIPHER_CKIP128:	 Length += 8;	 break;  // IV + CMIC + ICV, but CMIC already inserted by driver
		case CIPHER_TKIP_NO_MIC: Length += 12;	 break;  // IV + EIV + ICV
		default:								 break;
	}

	if (Rate < RATE_FIRST_OFDM_RATE)	// 11b - RATE_1, RATE_2, RATE_5_5, RATE_11
	{
		if ((Rate == RATE_1) || ( Rate == RATE_2))
		{
			Length = Length * 8 / (Rate + 1);
		}
		else
		{
			Residual = ((Length * 16) % (11 * (1 + Rate - RATE_5_5)));
			Length = Length * 16 / (11 * (1 + Rate - RATE_5_5));
			if (Residual != 0)
			{
				Length++;
			}
			if ((Residual <= (3 * (1 + Rate - RATE_5_5))) && (Residual != 0))
			{
				if (Rate == RATE_11)			// Only 11Mbps require length extension bit
					pTxD->PlcpService |= 0x80; // 11b's PLCP Length extension bit
			}
		}

		pTxD->PlcpLengthHigh = Length >> 8; // 256;
		pTxD->PlcpLengthLow = Length % 256;
	}
	else	// OFDM - RATE_6, RATE_9, RATE_12, RATE_18, RATE_24, RATE_36, RATE_48, RATE_54
	{
		pTxD->PlcpLengthHigh = Length >> 6; // 64;	// high 6-bit of total byte count
		pTxD->PlcpLengthLow = Length % 64;	 // low 6-bit of total byte count
	}

	pTxD->Burst  = Fragment;
	pTxD->Burst2 = pTxD->Burst;

#ifdef BIG_ENDIAN
	RTMPDescriptorEndianChange((PUCHAR)pTxD, TYPE_TXD);
#endif
}

/*
	========================================================================

	Routine	Description:
		To do the enqueue operation and extract the first item of waiting
		list. If a number of available shared memory segments could meet
		the request of extracted item, the extracted item will be fragmented
		into shared memory segments.

	Arguments:
		pAd			Pointer	to our adapter
		pQueue		Pointer to Waiting Queue

	Return Value:
		None

	Note:
		Called only from process context, protected by the usb semaphore.

	========================================================================
*/
VOID	RTMPDeQueuePacket(
	IN	PRTMP_ADAPTER	pAd,
	IN	UCHAR			BulkOutPipeId)
{
	struct sk_buff	*pSkb;
	UCHAR			FragmentRequired;
	NDIS_STATUS		Status;
	UCHAR			Count = 0;
	struct sk_buff_head	*pQueue;
	UCHAR			QueIdx;

	QueIdx = BulkOutPipeId;

	if (pAd->TxRingTotalNumber[BulkOutPipeId])
		DBGPRINT(RT_DEBUG_INFO,"--RTMPDeQueuePacket %d TxRingTotalNumber= %d !!--\n", BulkOutPipeId, (INT)pAd->TxRingTotalNumber[BulkOutPipeId]);

	// Select Queue
	pQueue = &pAd->SendTxWaitQueue[BulkOutPipeId];

	// Check queue before dequeue
	while (!skb_queue_empty(pQueue) && (Count < MAX_TX_PROCESS))
	{
		// Reset is in progress, stop immediately
		if ( RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_RESET_IN_PROGRESS) ||
			 RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_HALT_IN_PROGRESS))
		{
			DBGPRINT(RT_DEBUG_ERROR,"--RTMPDeQueuePacket %d reset-in-progress !!--\n", BulkOutPipeId);
			RTUSBFreeSkbBuffer(skb_dequeue(pQueue));
			continue;
		}
		if (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_BSS_SCAN_IN_PROGRESS)) {
			DBGPRINT(RT_DEBUG_TRACE,
				 "RTUSBDeQueuePacket scanning. Flags = 0x%x\n",
				 pAd->Flags);
			break;
		}

		// Dequeue the first entry from head of queue list
		pSkb = skb_dequeue(pQueue);

		// RTS or CTS-to-self for B/G protection mode has been set already.
		// There is no need to re-do it here.
		// Total fragment required = number of fragment + RST if required
		FragmentRequired = RTMP_GET_PACKET_FRAGMENTS(pSkb) + RTMP_GET_PACKET_RTS(pSkb);

		if ((RTUSBFreeDescriptorRequest(pAd, TX_RING, BulkOutPipeId, FragmentRequired) == NDIS_STATUS_SUCCESS))
		{
			// Avaliable ring descriptors are enough for this frame
			// Call hard transmit
			// Nitro mode / Normal mode selection

			Status = RTUSBHardTransmit(pAd, pSkb, FragmentRequired, QueIdx);


			if (Status == NDIS_STATUS_FAILURE)
			{
				// Packet failed due to various Ndis Packet error
				RTUSBFreeSkbBuffer(pSkb);
				break;
			}
			else if (Status == NDIS_STATUS_RESOURCES)
			{
				// Not enough free tx ring, it might happen due to free descriptor inquery might be not correct
				// It also might change to NDIS_STATUS_FAILURE to simply drop the frame
				// Put the frame back into head of queue
				skb_queue_head(pQueue, pSkb);
				break;
			}else if(Status == NDIS_STATUS_RINGFULL){//Thomas add
				pAd->TxRingTotalNumber[QueIdx]= 0;
				RTUSBFreeSkbBuffer(pSkb);
				break;
			}

			Count++;
		}
		else
		{
			DBGPRINT(RT_DEBUG_INFO,"--RTMPDeQueuePacket %d queue full !! TxRingTotalNumber= %d !! FragmentRequired=%d !!\n", BulkOutPipeId, (INT)pAd->TxRingTotalNumber[BulkOutPipeId], FragmentRequired);
			skb_queue_head(pQueue, pSkb);
		    pAd->PrivateInfo.TxRingFullCnt++;

			break;
		}
	}
}

VOID	RTMPDeQueuePackets(
IN	PRTMP_ADAPTER	pAd)
{
	int	Index;

	for (Index = 0; Index < 4; Index++)
	{
		if(!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_BSS_SCAN_IN_PROGRESS) &&
			!skb_queue_empty(&pAd->SendTxWaitQueue[Index]))
		{
			RTMPDeQueuePacket(pAd, Index);
		}
	}
}

/*
	========================================================================
	 Description:
		This is the completion routine for the USB_RxPacket which submits
		a URB to USBD for a transmission.
	Note:
		Called in process context.
	========================================================================
*/
VOID	RTUSBRxPacket(
	IN	 unsigned long data)
{
	purbb_t 			pUrb = (purbb_t)data;
	PRTMP_ADAPTER		pAd;
	PRX_CONTEXT 		pRxContext;
	PRXD_STRUC			pRxD;
#ifdef BIG_ENDIAN
	PRXD_STRUC			pDestRxD;
	RXD_STRUC			RxD;
#endif
	PHEADER_802_11		pHeader;
	PUCHAR				pData;
	PUCHAR				pDA, pSA;
	NDIS_STATUS			Status;
	USHORT				DataSize, Msdu2Size;
	UCHAR				Header802_3[14];
	PCIPHER_KEY 		pWpaKey;
//	  struct sk_buff	  *pSkb;
	BOOLEAN				EAPOLFrame;
	struct net_device			*net_dev;
	wlan_ng_prism2_header	*ph;
	int				i;

	pRxContext = (PRX_CONTEXT)pUrb->context;
	pAd = pRxContext->pAd;
	net_dev = pAd->net_dev;
	Status = pUrb->status;

	DBGPRINT(RT_DEBUG_TRACE, "--> RTUSBRxPacket len=%d, status=%d\n",
			pRxContext->pUrb->actual_length, Status);

	if(Status || RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST) ) {
		DBGPRINT(RT_DEBUG_TRACE, "<-- RTUSBRxPacket disconnected\n");
		return;
	}
	do
	{
		if (pRxContext->pUrb->actual_length >= sizeof(RXD_STRUC)+ LENGTH_802_11)
		{
		pData = pRxContext->TransferBuffer;
#ifndef BIG_ENDIAN
		pRxD = (PRXD_STRUC) pData;
#else
		pDestRxD = (PRXD_STRUC) pData;
		RxD = *pDestRxD;
		pRxD = &RxD;
		RTMPDescriptorEndianChange((PUCHAR)pRxD, TYPE_RXD);
#endif

		// Cast to 802.11 header for flags checking
		pHeader	= (PHEADER_802_11) (pData + sizeof(RXD_STRUC) );

#ifdef BIG_ENDIAN
		RTMPFrameEndianChange(pAd, (PUCHAR)pHeader, DIR_READ, FALSE);
#endif
		DBGPRINT(RT_DEBUG_INFO, "-   %s: Frame type %d subtype %d\n",
				__FUNCTION__, pHeader->FC.Type, pHeader->FC.SubType);
		if (pRxD->DataByteCnt < 4)
			Status = NDIS_STATUS_FAILURE;
		else
		{
			// Increase Total receive byte counter after real data received no mater any error or not
			pAd->RalinkCounters.ReceivedByteCount += (pRxD->DataByteCnt - 4);
			pAd->RalinkCounters.RxCount ++;

			// Check for all RxD errors
			Status = RTMPCheckRxDescriptor(pAd, pHeader, pRxD);

		}

		/* Only recieve valid packets in to monitor mode */
		if (pAd->PortCfg.BssType == BSS_MONITOR && Status == NDIS_STATUS_SUCCESS)
         	{
 	        	struct sk_buff  *skb;
 	       		if ((skb = __dev_alloc_skb(2048, GFP_DMA|GFP_KERNEL)) != NULL)
 	        	{
				if (pAd->bAcceptRFMONTx == TRUE) {
					if (pAd->ForcePrismHeader != 1)
						goto rfmontx_80211_receive;
				} else {
					if (pAd->ForcePrismHeader == 2)
						goto rfmontx_80211_receive;
				}
					// setup the wlan-ng prismheader

				if (skb_headroom(skb) < sizeof(wlan_ng_prism2_header))
					pskb_expand_head(skb, sizeof(wlan_ng_prism2_header), 0, GFP_KERNEL);

				ph = (wlan_ng_prism2_header *)
					skb_push(skb, sizeof(wlan_ng_prism2_header));
				memset(ph, 0, sizeof(wlan_ng_prism2_header));

				ph->msgcode	= DIDmsg_lnxind_wlansniffrm;
				ph->msglen	= sizeof(wlan_ng_prism2_header);
				strcpy(ph->devname, pAd->net_dev->name);

				ph->hosttime.did	= DIDmsg_lnxind_wlansniffrm_hosttime;
				ph->mactime.did		= DIDmsg_lnxind_wlansniffrm_mactime;
				ph->channel.did		= DIDmsg_lnxind_wlansniffrm_channel;
				ph->rssi.did		= DIDmsg_lnxind_wlansniffrm_rssi;
				ph->signal.did		= DIDmsg_lnxind_wlansniffrm_signal;
				ph->noise.did		= DIDmsg_lnxind_wlansniffrm_noise;
				ph->rate.did		= DIDmsg_lnxind_wlansniffrm_rate;
				ph->istx.did		= DIDmsg_lnxind_wlansniffrm_istx;
				ph->frmlen.did		= DIDmsg_lnxind_wlansniffrm_frmlen;

				ph->hosttime.len	= 4;
				ph->mactime.len		= 4;
				ph->channel.len		= 4;
				ph->rssi.len		= 4;
				ph->signal.len		= 4;
				ph->noise.len		= 4;
				ph->rate.len		= 4;
				ph->istx.len		= 4;
				ph->frmlen.len		= 4;

				ph->hosttime.data	= jiffies;
				ph->channel.data	= pAd->PortCfg.Channel;
				ph->signal.data		= pRxD->PlcpRssi;
				ph->noise.data		= (pAd->BbpWriteLatch[17] > pAd->BbpTuning.R17UpperBoundG) ?
									pAd->BbpTuning.R17UpperBoundG : ((ULONG) pAd->BbpWriteLatch[17]);
				ph->rssi.data		= ph->signal.data - ph->noise.data;
				ph->frmlen.data		= pRxD->DataByteCnt;

				if (pRxD->Ofdm == 1)
				{
					for (i = 4; i < 12; i++)
						if (pRxD->PlcpSignal == RateIdToPlcpSignal[i])
							ph->rate.data = _11G_RATES[i] * 2;
				}
				else
					ph->rate.data = pRxD->PlcpSignal / 5;

					// end prismheader setup

			rfmontx_80211_receive:
      				skb->dev = pAd->net_dev;
      				memcpy(skb_put(skb, pRxD->DataByteCnt), pHeader, pRxD->DataByteCnt);
					skb_reset_mac_header(skb);
      				skb->pkt_type = PACKET_OTHERHOST;
     				skb->protocol = htons(ETH_P_802_2);
        			skb->ip_summed = CHECKSUM_NONE;
	               		netif_rx(skb);
       			}
         	continue;
		}

		if (Status == NDIS_STATUS_SUCCESS)
		{
			// Apply packet filtering rule based on microsoft requirements.
			Status = RTMPApplyPacketFilter(pAd, pRxD, pHeader);
		}

		// Add receive counters
		if (Status == NDIS_STATUS_SUCCESS)
		{
			// Increase 802.11 counters & general receive counters
			INC_COUNTER64(pAd->WlanCounters.ReceivedFragmentCount);
		}
		else
		{
			// Increase general counters
			pAd->Counters.RxErrors++;
		}

		// Check for retry bit, if this bit is on, search the cache with SA & sequence
		// as index, if matched, discard this frame, otherwise, update cache
		// This check only apply to unicast data & management frames
		if ((pRxD->U2M) && (Status == NDIS_STATUS_SUCCESS) && (pHeader->FC.Type != BTYPE_CNTL))
		{
			if (pHeader->FC.Retry)
			{
				if (RTMPSearchTupleCache(pAd, pHeader) == TRUE)
				{
					// Found retry frame in tuple cache, Discard this frame / fragment
					// Increase 802.11 counters
					INC_COUNTER64(pAd->WlanCounters.FrameDuplicateCount);
					DBGPRINT(RT_DEBUG_INFO, "duplicate frame %d\n", pHeader->Sequence);
					Status = NDIS_STATUS_FAILURE;
				}
				else
				{
					RTMPUpdateTupleCache(pAd, pHeader);
				}
			}
			else	// Update Tuple Cache
			{
				RTMPUpdateTupleCache(pAd, pHeader);
			}
		}

		if ((pRxD->U2M)	|| ((pHeader->FC.SubType == SUBTYPE_BEACON) && (MAC_ADDR_EQUAL(&pAd->PortCfg.Bssid, &pHeader->Addr2))))
		{
			if ((pAd->Antenna.field.NumOfAntenna == 2) && (pAd->Antenna.field.TxDefaultAntenna == 0) && (pAd->Antenna.field.RxDefaultAntenna == 0))
			{
				COLLECT_RX_ANTENNA_AVERAGE_RSSI(pAd, ConvertToRssi(pAd, (UCHAR)pRxD->PlcpRssi, RSSI_NO_1), 0); //Note: RSSI2 not used on RT73
				pAd->PortCfg.NumOfAvgRssiSample ++;
			}
		}

		//
		// Do RxD release operation	for	all	failure	frames
		//
		if (Status == NDIS_STATUS_SUCCESS)
		{
			do
			{
				// pData : Pointer skip	the RxD Descriptior and the first 24 bytes,	802.11 HEADER
				pData += LENGTH_802_11 + sizeof(RXD_STRUC);
				DataSize = (USHORT) pRxD->DataByteCnt - LENGTH_802_11;

				//
				// CASE I. receive a DATA frame
				//
				if (pHeader->FC.Type == BTYPE_DATA)
				{
					DBGPRINT(RT_DEBUG_INFO, "-  %s: data frame\n", __FUNCTION__);
					// before LINK UP, all DATA frames are rejected
					if (!OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_MEDIA_STATE_CONNECTED))
					{
						DBGPRINT(RT_DEBUG_TRACE,"RxDone- drop DATA frame before LINK UP(len=%d)\n",pRxD->DataByteCnt);
						break;
					}
                    pAd->BulkInDataOneSecCount++;


					// remove the 2 extra QOS CNTL bytes
					if (pHeader->FC.SubType & 0x08)
					{
						pData += 2;
						DataSize -= 2;
					}

					// remove the 2 extra AGGREGATION bytes
					Msdu2Size = 0;
					if (pHeader->FC.Order)
					{
						Msdu2Size = *pData + (*(pData+1) << 8);
						if ((Msdu2Size <= 1536) && (Msdu2Size < DataSize))
						{
							pData += 2;
							DataSize -= 2;
						}
						else
							Msdu2Size = 0;
					}

					// Drop not my BSS frame
					//
					// Not drop EAPOL frame, since this have happen on the first time that we link up
					// And need some more time to set BSSID to asic
					// So pRxD->MyBss may be 0
					//
			        if (RTMPEqualMemory(EAPOL, pData + 6, 2))
						EAPOLFrame = TRUE;
					else
						EAPOLFrame = FALSE;

					if ((pRxD->MyBss == 0) && (EAPOLFrame != TRUE)) {
						DBGPRINT(RT_DEBUG_INFO, "-  %s: !MyBss || !EAPOL\n",
								__FUNCTION__);
						break; // give up this frame
					}
					// Drop NULL (+CF-POLL) (+CF-ACK) data frame
					if ((pHeader->FC.SubType & 0x04) == 0x04)
					{
						DBGPRINT(RT_DEBUG_TRACE,"RxDone- drop NULL frame(subtype=%d)\n",pHeader->FC.SubType);
						break;
					}


					// prepare 802.3 header: DA=addr1; SA=addr3 in INFRA mode, DA=addr2 in ADHOC mode
					pDA = pHeader->Addr1;
					if (INFRA_ON(pAd))
						pSA	= pHeader->Addr3;
					else
						pSA	= pHeader->Addr2;

					if (pHeader->FC.Wep) // frame received in encrypted format
					{
						if (pRxD->CipherAlg == CIPHER_NONE) // unsupported cipher suite
						{
							DBGPRINT(RT_DEBUG_INFO, "-  %s: nonsup cipher\n", __FUNCTION__);
							break; // give up this frame
						}
						else if (pAd->SharedKey[pRxD->KeyIndex].KeyLen == 0)
						{
							DBGPRINT(RT_DEBUG_INFO, "-  %s: keylen=0\n", __FUNCTION__);
							break; // give up this frame since the keylen is invalid.
						}
					}
					else
					{	// frame received in clear text
						// encryption in-use but receive a non-EAPOL clear text frame, drop it
						if (((pAd->PortCfg.WepStatus == Ndis802_11Encryption1Enabled) ||
							(pAd->PortCfg.WepStatus == Ndis802_11Encryption2Enabled) ||
							(pAd->PortCfg.WepStatus == Ndis802_11Encryption3Enabled)) &&
							(pAd->PortCfg.PrivacyFilter == Ndis802_11PrivFilter8021xWEP) &&
							(!NdisEqualMemory(EAPOL_LLC_SNAP, pData, LENGTH_802_1_H)))
						{
							DBGPRINT(RT_DEBUG_INFO, "-  %s: clear text\n", __FUNCTION__);
							break; // give up this frame
						}
					}

					//
					// Case I.1  Process Broadcast & Multicast data frame
					//
					if (pRxD->Bcast || pRxD->Mcast)
					{
						PUCHAR pRemovedLLCSNAP;

						INC_COUNTER64(pAd->WlanCounters.MulticastReceivedFrameCount);

						// Drop Mcast/Bcast frame with fragment bit on
						if (pHeader->FC.MoreFrag)
						{
							DBGPRINT(RT_DEBUG_INFO, "-  %s: mcast w/more\n", __FUNCTION__);
							break; // give up this frame
						}

						// Filter out Bcast frame which AP relayed for us
						if (pHeader->FC.FrDs && MAC_ADDR_EQUAL(pHeader->Addr3, pAd->CurrentAddress))
						{
							DBGPRINT(RT_DEBUG_INFO, "-  %s: relay mcast\n", __FUNCTION__);
							break; // give up this frame
						}

						// build 802.3 header and decide if remove the 8-byte LLC/SNAP encapsulation
						CONVERT_TO_802_3(Header802_3, pDA, pSA, pData, DataSize, pRemovedLLCSNAP);
						REPORT_ETHERNET_FRAME_TO_LLC(pAd,Header802_3, pData, DataSize, pAd->net_dev);
						DBGPRINT(RT_DEBUG_TRACE, "!!! report BCAST DATA to LLC (len=%d) !!!\n", DataSize);
					}
					//
					// Case I.2  Process unicast-to-me DATA frame
					//
					else if	(pRxD->U2M)
					{
						RECORD_LATEST_RX_DATA_RATE(pAd, pRxD);

//#if WPA_SUPPLICANT_SUPPORT
                    if (pAd->PortCfg.WPA_Supplicant == TRUE)
			{
					// All EAPoL frames have to pass to upper layer (ex. WPA_SUPPLICANT daemon)
					// TBD : process fragmented EAPol frames
					if(NdisEqualMemory(EAPOL_LLC_SNAP, pData, LENGTH_802_1_H))
					{
						PUCHAR pRemovedLLCSNAP;
						int		success = 0;

						// In 802.1x mode, if the received frame is EAP-SUCCESS packet, turn on the PortSecured variable
						if ( pAd->PortCfg.IEEE8021X == TRUE
						    && (EAP_CODE_SUCCESS == RTMPCheckWPAframeForEapCode(pAd, pData, DataSize, LENGTH_802_1_H)))
						{
								DBGPRINT(RT_DEBUG_TRACE, "Receive EAP-SUCCESS Packet\n");
								pAd->PortCfg.PortSecured = WPA_802_1X_PORT_SECURED;

								success = 1;
						}

						// build 802.3 header and decide if remove the 8-byte LLC/SNAP encapsulation
						CONVERT_TO_802_3(Header802_3, pDA, pSA, pData, DataSize, pRemovedLLCSNAP);
                    				REPORT_ETHERNET_FRAME_TO_LLC(pAd, Header802_3, pData, DataSize, net_dev);
						DBGPRINT(RT_DEBUG_TRACE, "!!! report EAPoL DATA to LLC (len=%d) !!!\n", DataSize);

						if(success)
						{
							// For static wep mode, need to set wep key to Asic again
							if(pAd->PortCfg.IEEE8021x_required_keys == 0)
							{
							 	int idx;

								idx = pAd->PortCfg.DefaultKeyId;
								for (idx=0; idx < 4; idx++)
								{
									DBGPRINT(RT_DEBUG_TRACE, "Set WEP key to Asic again =>\n");

									if(pAd->PortCfg.DesireSharedKey[idx].KeyLen != 0)
									{
						                union
						                {
		                                    char buf[sizeof(NDIS_802_11_WEP)+MAX_LEN_OF_KEY- 1];
		                                    NDIS_802_11_WEP keyinfo;
                                        }   WepKey;
                                        int len;


	                                    memset(&WepKey, 0, sizeof(WepKey));
                                        len =pAd->PortCfg.DesireSharedKey[idx].KeyLen;

			              			    memcpy(WepKey.keyinfo.KeyMaterial,
			              			                    pAd->PortCfg.DesireSharedKey[idx].Key,
			              			                    pAd->PortCfg.DesireSharedKey[idx].KeyLen);

                                        WepKey.keyinfo.KeyIndex = 0x80000000 + idx;
			                            WepKey.keyinfo.KeyLength = len;
			                            pAd->SharedKey[idx].KeyLen =(UCHAR) (len <= WEP_SMALL_KEY_LEN ? WEP_SMALL_KEY_LEN : WEP_LARGE_KEY_LEN);

		                                // need to enqueue cmd to thread
			                            RTUSBEnqueueCmdFromNdis(pAd, OID_802_11_ADD_WEP, TRUE, &WepKey, sizeof(WepKey.keyinfo) + len - 1);
									}
								}
							}
						}

						break;
					}
				    } /* End (pAd->PortCfg.WPA_Supplicant == TRUE) */
			else
			{
//#else
						// Special DATA frame that has to pass to MLME
						//	 1. EAPOL handshaking frames when driver supplicant enabled, pass to MLME for special process
						if (NdisEqualMemory(EAPOL_LLC_SNAP, pData, LENGTH_802_1_H) && (pAd->PortCfg.WpaState != SS_NOTUSE))
						{
							DataSize += LENGTH_802_11;
							REPORT_MGMT_FRAME_TO_MLME(pAd, pHeader, DataSize, pRxD->PlcpRssi, pRxD->PlcpSignal);
							DBGPRINT(RT_DEBUG_TRACE, "!!! report EAPOL/AIRONET DATA to MLME (len=%d) !!!\n", DataSize);
							break;	// end of processing this frame
						}
//#endif
                    }
						if (pHeader->Frag == 0) 	// First or Only fragment
						{
							PUCHAR pRemovedLLCSNAP;
							DBGPRINT(RT_DEBUG_INFO, "-  %s: 1st/only frag\n", __FUNCTION__);

							CONVERT_TO_802_3(Header802_3, pDA, pSA, pData, DataSize, pRemovedLLCSNAP);
							pAd->FragFrame.Flags &= 0xFFFFFFFE;

							// Firt Fragment & LLC/SNAP been removed. Keep the removed LLC/SNAP for later on
							// TKIP MIC verification.
							if (pHeader->FC.MoreFrag && pRemovedLLCSNAP)
							{
								memcpy(pAd->FragFrame.Header_LLC, pRemovedLLCSNAP, LENGTH_802_1_H);
								pAd->FragFrame.Flags |= 0x01;
							}

							// One & The only fragment
							if (pHeader->FC.MoreFrag == FALSE)
							{
								if ((pHeader->FC.Order == 1)  && (Msdu2Size > 0)) // this is an aggregation
								{
									USHORT Payload1Size, Payload2Size;
									PUCHAR pData2;

									pAd->RalinkCounters.OneSecRxAggregationCount ++;
									Payload1Size = DataSize - Msdu2Size;
									Payload2Size = Msdu2Size - LENGTH_802_3;

									REPORT_ETHERNET_FRAME_TO_LLC(pAd, Header802_3, pData, Payload1Size, pAd->net_dev);
									DBGPRINT(RT_DEBUG_TRACE, "!!! report segregated MSDU1 to LLC (len=%d, proto=%02x:%02x) %02x:%02x:%02x:%02x-%02x:%02x:%02x:%02x\n",
															LENGTH_802_3+Payload1Size, Header802_3[12], Header802_3[13],
															*pData, *(pData+1),*(pData+2),*(pData+3),*(pData+4),*(pData+5),*(pData+6),*(pData+7));

									pData2 = pData + Payload1Size + LENGTH_802_3;
									REPORT_ETHERNET_FRAME_TO_LLC(pAd, pData + Payload1Size, pData2, Payload2Size, pAd->net_dev);
									DBGPRINT(RT_DEBUG_INFO, "!!! report segregated MSDU2 to LLC (len=%d, proto=%02x:%02x) %02x:%02x:%02x:%02x-%02x:%02x:%02x:%02x\n",
															LENGTH_802_3+Payload2Size, *(pData2 -2), *(pData2 - 1),
															*pData2, *(pData2+1),*(pData2+2),*(pData2+3),*(pData2+4),*(pData2+5),*(pData2+6),*(pData2+7));
								}
								else
								{
									REPORT_ETHERNET_FRAME_TO_LLC(pAd, Header802_3, pData, DataSize, pAd->net_dev);
									DBGPRINT(RT_DEBUG_INFO, "!!! report DATA (no frag) to LLC (len=%d, proto=%02x:%02x) %02x:%02x:%02x:%02x-%02x:%02x:%02x:%02x\n",
															DataSize, Header802_3[12], Header802_3[13],
															*pData, *(pData+1),*(pData+2),*(pData+3),*(pData+4),*(pData+5),*(pData+6),*(pData+7));
								}
							}
							// First fragment - record the 802.3 header and frame body
							else
							{
								memcpy(&pAd->FragFrame.Buffer[LENGTH_802_3], pData, DataSize);
								memcpy(pAd->FragFrame.Header802_3, Header802_3, LENGTH_802_3);
								pAd->FragFrame.RxSize	 = DataSize;
								pAd->FragFrame.Sequence = pHeader->Sequence;
								pAd->FragFrame.LastFrag = pHeader->Frag;		// Should be 0
							}
						} //First or Only fragment
						// Middle & End of fragment burst fragments
						else
						{
							DBGPRINT(RT_DEBUG_INFO, "-  %s: mid/end frag\n", __FUNCTION__);
							// No LLC-SNAP header in except the first fragment frame
							if ((pHeader->Sequence != pAd->FragFrame.Sequence) ||
								(pHeader->Frag != (pAd->FragFrame.LastFrag + 1)))
							{
								// Fragment is not the same sequence or out of fragment number order
								// Clear Fragment frame contents
								memset(&pAd->FragFrame, 0, sizeof(FRAGMENT_FRAME));
								break;
							}
							else if ((pAd->FragFrame.RxSize + DataSize) > MAX_FRAME_SIZE)
							{
								// Fragment frame is too large, it exeeds the maximum frame size.
								// Clear Fragment frame contents
								memset(&pAd->FragFrame, 0, sizeof(FRAGMENT_FRAME));
								break; // give up this frame
							}

							// concatenate this fragment into the re-assembly buffer
							memcpy(&pAd->FragFrame.Buffer[LENGTH_802_3 + pAd->FragFrame.RxSize], pData, DataSize);
							pAd->FragFrame.RxSize	+= DataSize;
							pAd->FragFrame.LastFrag = pHeader->Frag;		// Update fragment number

							// Last fragment
							if (pHeader->FC.MoreFrag == FALSE)
							{
								DBGPRINT(RT_DEBUG_INFO, "-  %s: end frag\n", __FUNCTION__);
								// For TKIP frame, calculate the MIC value
								if (pRxD->CipherAlg == CIPHER_TKIP)
								{
									pWpaKey = &pAd->SharedKey[pRxD->KeyIndex];

									// Minus MIC length
									pAd->FragFrame.RxSize -= 8;

									if (pAd->FragFrame.Flags & 0x00000001)
									{
										// originally there's an LLC/SNAP field in the first fragment
										// but been removed in re-assembly buffer. here we have to include
										// this LLC/SNAP field upon calculating TKIP MIC
										// pData = pAd->FragFrame.Header_LLC;
										// Copy LLC data to the position in front of real data for MIC calculation
										memcpy(&pAd->FragFrame.Buffer[LENGTH_802_3 - LENGTH_802_1_H],
														pAd->FragFrame.Header_LLC,
														LENGTH_802_1_H);
										pData = (PUCHAR) &pAd->FragFrame.Buffer[LENGTH_802_3 - LENGTH_802_1_H];
										DataSize = (USHORT)pAd->FragFrame.RxSize + LENGTH_802_1_H;
									}
									else
									{
										pData = (PUCHAR) &pAd->FragFrame.Buffer[LENGTH_802_3];
										DataSize = (USHORT)pAd->FragFrame.RxSize;
									}

									if (RTMPTkipCompareMICValue(
											pAd,
											pData,
											pDA,
											pSA,
											pWpaKey->RxMic,
											DataSize) == FALSE)
									{
										DBGPRINT(RT_DEBUG_ERROR,"Rx MIC Value error 2\n");
										RTMPReportMicError(pAd, pWpaKey);
										break;	// give up this frame
									}
								}

								pData = &pAd->FragFrame.Buffer[LENGTH_802_3];
								REPORT_ETHERNET_FRAME_TO_LLC(pAd, pAd->FragFrame.Header802_3, pData, pAd->FragFrame.RxSize, pAd->net_dev);
								DBGPRINT(RT_DEBUG_TRACE, "!!! report DATA (fragmented) to LLC (len=%d) !!!\n", pAd->FragFrame.RxSize);
							}
						}
					}
				} // FC.Type == BTYPE_DATA
				//
				// CASE II. receive a MGMT frame
				//
				else if (pHeader->FC.Type == BTYPE_MGMT)
				{
					REPORT_MGMT_FRAME_TO_MLME(pAd, pHeader, pRxD->DataByteCnt, pRxD->PlcpRssi, pRxD->PlcpSignal);
					break;	// end of processing this frame
				}
				//
				// CASE III. receive a CNTL frame
				//
				else if (pHeader->FC.Type == BTYPE_CNTL)
					break; // give up this frame
				//
				// CASE IV. receive a frame of invalid type
				//
				else {
					DBGPRINT(RT_DEBUG_INFO, "-  %s: unkn frame\n", __FUNCTION__);
					break; // give up this frame
				}
			} while (FALSE); // ************* exit point *********

		}//if (Status == NDIS_STATUS_SUCCESS)

		else if (Status == NDIS_STATUS_RESET)
		{
			RTUSBEnqueueInternalCmd(pAd, RT_OID_USB_RESET_BULK_IN);
			DBGPRINT(RT_DEBUG_TRACE, "<---RTUSBRxPacket RESET_BULK\n");
			return;
		}
	  }//if (pRxContext->pUrb->actual_length >= sizeof(RXD_STRUC)+ LENGTH_802_11)
	} while (0);

	DBGPRINT(RT_DEBUG_TRACE, "<---RTUSBRxPacket\n");
}

/*
	========================================================================

	Routine Description:
		Called only from MLMEThread.
		Service each packet that is done and not yet serviced.

	Arguments:

	Return Value:

	Note:
		Called in process context.

	========================================================================
*/
VOID	RTUSBDequeueRxPackets(
	IN	PRTMP_ADAPTER	pAd)
{
	int			i = pAd->CurRxBulkInIndex;

	do {
		PRX_CONTEXT pRxContext = &pAd->RxContext[i];

		if (atomic_read(&pRxContext->IrpLock) != IRPLOCK_COMPLETED ||
			pRxContext->InUse == FALSE) {
			break;
		}
		RTUSBRxPacket((unsigned long)pRxContext->pUrb);
		pRxContext->InUse = FALSE;

		if ((!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_RESET_IN_PROGRESS)) &&
			(!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_BULKIN_RESET)) &&
			(!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_RADIO_OFF)) &&
			(!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_HALT_IN_PROGRESS)) &&
			(!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST))) {
				RTUSBBulkReceive(pAd);
		}
		if (++i >= RX_RING_SIZE) i = 0;
	} while (1);

	pAd->CurRxBulkInIndex = i;

} /* End RTUSBDequeueRxPackets () */

/*
	========================================================================

	Routine Description:

	Arguments:

	Return Value:

	Note:

	========================================================================
*/
VOID	RTUSBDequeueMLMEPacket(
	IN	PRTMP_ADAPTER	pAd)
{
	PMGMT_STRUC		pMgmt;
	unsigned long flags;

	DBGPRINT(RT_DEBUG_INFO, "RTUSBDequeueMLMEPacket\n");
	NdisAcquireSpinLock(&pAd->MLMEWaitQueueLock);
	while ((pAd->PopMgmtIndex != pAd->PushMgmtIndex) || (atomic_read(&pAd->MgmtQueueSize) > 0))
	{
		pMgmt = &pAd->MgmtRing[pAd->PopMgmtIndex];

		if (RTUSBFreeDescriptorRequest(pAd, PRIO_RING, 0, 1) == NDIS_STATUS_SUCCESS)
		{
			atomic_dec(&pAd->MgmtQueueSize);
			pAd->PopMgmtIndex = (pAd->PopMgmtIndex + 1) % MGMT_RING_SIZE;
			NdisReleaseSpinLock(&pAd->MLMEWaitQueueLock);

			RTUSBMlmeHardTransmit(pAd, pMgmt);

			MlmeFreeMemory(pAd, pMgmt->pBuffer);
			pMgmt->pBuffer = NULL;
			pMgmt->Valid = FALSE;

			NdisAcquireSpinLock(&pAd->MLMEWaitQueueLock);
		}
		else
		{
			DBGPRINT(RT_DEBUG_TRACE, "not enough space in PrioRing[pAdapter->MgmtQueueSize=%d]\n", atomic_read(&pAd->MgmtQueueSize));
			DBGPRINT(RT_DEBUG_TRACE, "RTUSBDequeueMLMEPacket::PrioRingFirstIndex = %d, PrioRingTxCnt = %d, PopMgmtIndex = %d, PushMgmtIndex = %d, NextMLMEIndex = %d\n",
			pAd->PrioRingFirstIndex, pAd->PrioRingTxCnt,
			pAd->PopMgmtIndex, pAd->PushMgmtIndex, pAd->NextMLMEIndex);
			break;
		}
	}
	NdisReleaseSpinLock(&pAd->MLMEWaitQueueLock);
}

/*
	========================================================================

	Routine Description:

	Arguments:

	Return Value:

	Note:

	========================================================================
*/
VOID	RTUSBCleanUpMLMEWaitQueue(
	IN	PRTMP_ADAPTER	pAd)
{
	PMGMT_STRUC		pMgmt;
	unsigned long flags;

	DBGPRINT(RT_DEBUG_TRACE, "--->CleanUpMLMEWaitQueue\n");

	NdisAcquireSpinLock(&pAd->MLMEWaitQueueLock);
	while (pAd->PopMgmtIndex != pAd->PushMgmtIndex)
	{
		pMgmt = (PMGMT_STRUC)&pAd->MgmtRing[pAd->PopMgmtIndex];
		MlmeFreeMemory(pAd, pMgmt->pBuffer);
		pMgmt->pBuffer = NULL;
		pMgmt->Valid = FALSE;
		atomic_dec(&pAd->MgmtQueueSize);

		pAd->PopMgmtIndex++;
		if (pAd->PopMgmtIndex >= MGMT_RING_SIZE)
		{
			pAd->PopMgmtIndex = 0;
		}
	}
	NdisReleaseSpinLock(&pAd->MLMEWaitQueueLock);

	DBGPRINT(RT_DEBUG_TRACE, "<---CleanUpMLMEWaitQueue\n");
}

/*
	========================================================================

	Routine	Description:
		Suspend MSDU transmission

	Arguments:
		pAd		Pointer	to our adapter

	Return Value:
		None

	Note:

	========================================================================
*/
VOID	RTUSBSuspendMsduTransmission(
	IN	PRTMP_ADAPTER	pAd)
{
	DBGPRINT(RT_DEBUG_TRACE,"SCANNING, suspend MSDU transmission ...\n");

	//
	// Before BSS_SCAN_IN_PROGRESS, we need to keep Current R17 value and
	// use Lowbound as R17 value on ScanNextChannel(...)
	//
	if (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_MEDIA_STATE_CONNECTED))
		RTUSBReadBBPRegister(pAd, 17, &pAd->BbpTuning.R17CurrentValue);

	RTMP_SET_FLAG(pAd, fRTMP_ADAPTER_BSS_SCAN_IN_PROGRESS);
}

/*
	========================================================================

	Routine	Description:
		Resume MSDU transmission

	Arguments:
		pAd		Pointer	to our adapter

	Return Value:
		None

	Note:

	========================================================================
*/
VOID	RTUSBResumeMsduTransmission(
	IN	PRTMP_ADAPTER	pAd)
{
	DBGPRINT(RT_DEBUG_ERROR,"SCAN done, resume MSDU transmission ...\n");
	RTMP_CLEAR_FLAG(pAd, fRTMP_ADAPTER_BSS_SCAN_IN_PROGRESS);

	//
	// After finish BSS_SCAN_IN_PROGRESS, we need to restore Current R17 value
	//
	if (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_MEDIA_STATE_CONNECTED))
		RTUSBWriteBBPRegister(pAd, 17, pAd->BbpTuning.R17CurrentValue);

	if ((!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_RESET_IN_PROGRESS)) &&
		(!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_HALT_IN_PROGRESS)) &&
		(!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_RADIO_OFF)))
	{
		RTMPDeQueuePackets(pAd);
	}

	// Kick bulk out
	RTUSBKickBulkOut(pAd);
}

/*
	========================================================================

	Routine	Description:
		API for MLME to transmit management frame to AP (BSS Mode)
	or station (IBSS Mode)

	Arguments:
		pAd			Pointer	to our adapter
		Buffer		Pointer to	memory of outgoing frame
		Length		Size of outgoing management frame

	Return Value:
		NDIS_STATUS_FAILURE
		NDIS_STATUS_PENDING
		NDIS_STATUS_SUCCESS

	Note:

	========================================================================
*/
VOID	MiniportMMRequest(
	IN	PRTMP_ADAPTER	pAd,
	IN	PVOID			pBuffer,
	IN	ULONG			Length)
{
	unsigned long flags;

	DBGPRINT(RT_DEBUG_INFO, "---> MiniportMMRequest\n");

	if (pBuffer)
	{
		PMGMT_STRUC	pMgmt;

		// Check management ring free avaliability
		NdisAcquireSpinLock(&pAd->MLMEWaitQueueLock);
		pMgmt = (PMGMT_STRUC)&pAd->MgmtRing[pAd->PushMgmtIndex];
		// This management cell has been occupied
		if (pMgmt->Valid == TRUE)
		{
			NdisReleaseSpinLock(&pAd->MLMEWaitQueueLock);
			MlmeFreeMemory(pAd, pBuffer);
			pAd->RalinkCounters.MgmtRingFullCount++;
			DBGPRINT(RT_DEBUG_WARN, "MiniportMMRequest (error:: MgmtRing full)\n");
		}
		// Insert this request into software managemnet ring
		else
		{
			pMgmt->pBuffer = pBuffer;
			pMgmt->Length  = Length;
			pMgmt->Valid   = TRUE;
			pAd->PushMgmtIndex++;
			atomic_inc(&pAd->MgmtQueueSize);
			if (pAd->PushMgmtIndex >= MGMT_RING_SIZE)
			{
				pAd->PushMgmtIndex = 0;
			}
			NdisReleaseSpinLock(&pAd->MLMEWaitQueueLock);
		}
	}
	else
		DBGPRINT(RT_DEBUG_WARN, "MiniportMMRequest (error:: NULL msg)\n");

	RTUSBDequeueMLMEPacket(pAd);

	// If pAd->PrioRingTxCnt is larger than 0, this means that prio_ring have something to transmit.
	// Then call KickBulkOut to transmit it
	if (pAd->PrioRingTxCnt > 0)
	{
		if (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_DOZE))
			AsicForceWakeup(pAd);
		RTUSBKickBulkOut(pAd);
	}

	DBGPRINT(RT_DEBUG_INFO, "<--- MiniportMMRequest\n");
}

/*
	========================================================================

	Routine	Description:
		Search tuple cache for receive duplicate frame from unicast frames.

	Arguments:
		pAd				Pointer	to our adapter
		pHeader			802.11 header of receiving frame

	Return Value:
		TRUE			found matched tuple cache
		FALSE			no matched found

	Note:

	========================================================================
*/
BOOLEAN	RTMPSearchTupleCache(
	IN	PRTMP_ADAPTER	pAd,
	IN	PHEADER_802_11	pHeader)
{
	INT	Index;

	for (Index = 0; Index < MAX_CLIENT; Index++)
	{
		if (pAd->TupleCache[Index].Valid == FALSE)
			continue;

		if (MAC_ADDR_EQUAL(pAd->TupleCache[Index].MacAddress, pHeader->Addr2) &&
			(pAd->TupleCache[Index].Sequence == pHeader->Sequence) &&
			(pAd->TupleCache[Index].Frag == pHeader->Frag))
		{
//			DBGPRINT(RT_DEBUG_TRACE,"DUPCHECK - duplicate frame hit entry %d\n", Index);
			return (TRUE);
		}
	}
	return (FALSE);
}

/*
	========================================================================

	Routine	Description:
		Update tuple cache for new received unicast frames.

	Arguments:
		pAd				Pointer	to our adapter
		pHeader			802.11 header of receiving frame

	Return Value:
		None

	Note:

	========================================================================
*/
VOID	RTMPUpdateTupleCache(
	IN	PRTMP_ADAPTER	pAd,
	IN	PHEADER_802_11	pHeader)
{
	UCHAR	Index;

	for (Index = 0; Index < MAX_CLIENT; Index++)
	{
		if (pAd->TupleCache[Index].Valid == FALSE)
		{
			// Add new entry
			COPY_MAC_ADDR(pAd->TupleCache[Index].MacAddress, pHeader->Addr2);
			pAd->TupleCache[Index].Sequence = pHeader->Sequence;
			pAd->TupleCache[Index].Frag 	= pHeader->Frag;
			pAd->TupleCache[Index].Valid	= TRUE;
			pAd->TupleCacheLastUpdateIndex	= Index;
			DBGPRINT(RT_DEBUG_INFO,"DUPCHECK - Add Entry %d, MAC=%02x:%02x:%02x:%02x:%02x:%02x\n", Index,
				pAd->TupleCache[Index].MacAddress[0], pAd->TupleCache[Index].MacAddress[1],
				pAd->TupleCache[Index].MacAddress[2], pAd->TupleCache[Index].MacAddress[3],
				pAd->TupleCache[Index].MacAddress[4], pAd->TupleCache[Index].MacAddress[5]);
			return;
		}
		else if (MAC_ADDR_EQUAL(pAd->TupleCache[Index].MacAddress, pHeader->Addr2))
		{
			// Update old entry
			pAd->TupleCache[Index].Sequence = pHeader->Sequence;
			pAd->TupleCache[Index].Frag 	= pHeader->Frag;
			return;
		}
	}

	// tuple cache full, replace the first inserted one (even though it may not be
	// least referenced one)
	if (Index == MAX_CLIENT)
	{
		pAd->TupleCacheLastUpdateIndex ++;
		if (pAd->TupleCacheLastUpdateIndex >= MAX_CLIENT)
			pAd->TupleCacheLastUpdateIndex = 0;
		Index = pAd->TupleCacheLastUpdateIndex;

		// replace with new entry
		COPY_MAC_ADDR(pAd->TupleCache[Index].MacAddress, pHeader->Addr2);
		pAd->TupleCache[Index].Sequence = pHeader->Sequence;
		pAd->TupleCache[Index].Frag 	= pHeader->Frag;
		pAd->TupleCache[Index].Valid	= TRUE;
		DBGPRINT(RT_DEBUG_INFO,"DUPCHECK - replace Entry %d, MAC=%02x:%02x:%02x:%02x:%02x:%02x\n", Index,
			pAd->TupleCache[Index].MacAddress[0], pAd->TupleCache[Index].MacAddress[1],
			pAd->TupleCache[Index].MacAddress[2], pAd->TupleCache[Index].MacAddress[3],
			pAd->TupleCache[Index].MacAddress[4], pAd->TupleCache[Index].MacAddress[5]);
	}
}

/*
	========================================================================

	Routine	Description:
		Apply packet filter policy, return NDIS_STATUS_FAILURE if this frame
		should be dropped.

	Arguments:
		pAd		Pointer	to our adapter
		pRxD			Pointer	to the Rx descriptor
		pHeader			Pointer to the 802.11 frame header

	Return Value:
		NDIS_STATUS_SUCCESS		Accept frame
		NDIS_STATUS_FAILURE		Drop Frame

	Note:
		Maganement frame should bypass this filtering rule.

	========================================================================
*/
NDIS_STATUS	RTMPApplyPacketFilter(
	IN	PRTMP_ADAPTER	pAd,
	IN	PRXD_STRUC		pRxD,
	IN	PHEADER_802_11	pHeader)
{
	UCHAR	i;

	// 0. Management frame should bypass all these filtering rules.
	if (pHeader->FC.Type == BTYPE_MGMT)
		return(NDIS_STATUS_SUCCESS);

	// 0.1	Drop all Rx frames if MIC countermeasures kicks in
	if (pAd->PortCfg.MicErrCnt >= 2)
	{
		DBGPRINT(RT_DEBUG_TRACE,"Rx dropped by MIC countermeasure\n");
		return(NDIS_STATUS_FAILURE);
	}

	// 1. Drop unicast to me packet if NDIS_PACKET_TYPE_DIRECTED is FALSE
	if (pRxD->U2M)
	{
		if (pAd->bAcceptDirect == FALSE)
		{
			DBGPRINT(RT_DEBUG_TRACE,"Rx U2M dropped by RX_FILTER\n");
			return(NDIS_STATUS_FAILURE);
		}
	}

	// 2. Drop broadcast packet if NDIS_PACKET_TYPE_BROADCAST is FALSE
	else if (pRxD->Bcast)
	{
		if (pAd->bAcceptBroadcast == FALSE)
		{
			DBGPRINT(RT_DEBUG_TRACE,"Rx BCAST dropped by RX_FILTER\n");
			return(NDIS_STATUS_FAILURE);
		}
	}

	// 3. Drop (non-Broadcast) multicast packet if NDIS_PACKET_TYPE_ALL_MULTICAST is false
	//	  and NDIS_PACKET_TYPE_MULTICAST is false.
	//	  If NDIS_PACKET_TYPE_MULTICAST is true, but NDIS_PACKET_TYPE_ALL_MULTICAST is false.
	//	  We have to deal with multicast table lookup & drop not matched packets.
	else if (pRxD->Mcast)
	{
		if (pAd->bAcceptAllMulticast == FALSE)
		{
			if (pAd->bAcceptMulticast == FALSE)
			{
				DBGPRINT(RT_DEBUG_INFO,"Rx MCAST dropped by RX_FILTER\n");
				return(NDIS_STATUS_FAILURE);
			}
			else
			{
				// Selected accept multicast packet based on multicast table
				for (i = 0; i < pAd->NumberOfMcastAddresses; i++)
				{
					if (MAC_ADDR_EQUAL(pHeader->Addr1, pAd->McastTable[i]))
						break;		// Matched
				}

				// Not matched
				if (i == pAd->NumberOfMcastAddresses)
				{
					DBGPRINT(RT_DEBUG_INFO,"Rx MCAST %02x:%02x:%02x:%02x:%02x:%02x dropped by RX_FILTER\n",
						pHeader->Addr1[0], pHeader->Addr1[1], pHeader->Addr1[2],
						pHeader->Addr1[3], pHeader->Addr1[4], pHeader->Addr1[5]);
					return(NDIS_STATUS_FAILURE);
				}
				else
				{
					DBGPRINT(RT_DEBUG_INFO,"Accept multicast %02x:%02x:%02x:%02x:%02x:%02x\n",
						pHeader->Addr1[0], pHeader->Addr1[1], pHeader->Addr1[2],
						pHeader->Addr1[3], pHeader->Addr1[4], pHeader->Addr1[5]);
				}
			}
		}
	}

	// 4. Not U2M, not Mcast, not Bcast, must be unicast to other DA.
	//	  Since we did not implement promiscuous mode, just drop this kind of packet for now.
	else
		return(NDIS_STATUS_FAILURE);

	return(NDIS_STATUS_SUCCESS);
}

/*
	========================================================================

	Routine	Description:
		Check Rx descriptor, return NDIS_STATUS_FAILURE if any error dound

	Arguments:
		pRxD		Pointer	to the Rx descriptor

	Return Value:
		NDIS_STATUS_SUCCESS		No err
		NDIS_STATUS_FAILURE		Error

	Note:

	========================================================================
*/
NDIS_STATUS	RTMPCheckRxDescriptor(
	IN	PRTMP_ADAPTER	pAd,
	IN	PHEADER_802_11	pHeader,
	IN	PRXD_STRUC	pRxD)
{
	PCIPHER_KEY 	pWpaKey;

	// Phy errors & CRC errors
	if (/*(pRxD->PhyErr) ||*/ (pRxD->Crc))
	{
		DBGPRINT(RT_DEBUG_ERROR, "pRxD->Crc error\n")
		return (NDIS_STATUS_FAILURE);
	}

	// Drop ToDs promiscous frame, it is opened due to CCX 2 channel load statistics
	if (pHeader->FC.ToDs && pAd->PortCfg.BssType != BSS_MONITOR)		//Don't drop to_ds frame if in monitor mode
		return(NDIS_STATUS_FAILURE);

	// Paul 04-03 for OFDM Rx length issue
	if (pRxD->DataByteCnt > MAX_AGGREGATION_SIZE)
	{
		DBGPRINT(RT_DEBUG_ERROR, "received packet too long\n");
		return (NDIS_STATUS_FAILURE);
	}

	// Drop not U2M frames, cant's drop here because we will drop beacon in this case
	// I am kind of doubting the U2M bit operation
	// if (pRxD->U2M == 0)
	//	return(NDIS_STATUS_FAILURE);

	// drop decyption fail frame
	if (pRxD->CipherErr)
	{
		UINT i;
		PUCHAR ptr = (PUCHAR)pHeader;
		DBGPRINT(RT_DEBUG_ERROR,"ERROR: CRC ok but CipherErr %d (len = %d, Mcast=%d, Cipher=%s, KeyId=%d)\n",
			pRxD->CipherErr,
			pRxD->DataByteCnt,
			pRxD->Mcast | pRxD->Bcast,
			CipherName[pRxD->CipherAlg],
			pRxD->KeyIndex);
#if 1
		for (i=0;i<64; i+=16)
		{
			DBGPRINT(RT_DEBUG_ERROR,"%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x - %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n",
				*ptr,*(ptr+1),*(ptr+2),*(ptr+3),*(ptr+4),*(ptr+5),*(ptr+6),*(ptr+7),
				*(ptr+8),*(ptr+9),*(ptr+10),*(ptr+11),*(ptr+12),*(ptr+13),*(ptr+14),*(ptr+15));
			ptr += 16;
		}
#endif


		//
		// MIC Error
		//
		if (pRxD->CipherErr == 2)
		{
			pWpaKey = &pAd->SharedKey[pRxD->KeyIndex];
			RTMPReportMicError(pAd, pWpaKey);
			DBGPRINT(RT_DEBUG_ERROR,"Rx MIC Value error\n");
		}

		if ((pRxD->CipherAlg == CIPHER_AES) &&
			(pHeader->Sequence == pAd->FragFrame.Sequence))
		{
			//
			// Acceptable since the First FragFrame no CipherErr problem.
			//
			return (NDIS_STATUS_FAILURE);
		}

		return (NDIS_STATUS_FAILURE);
	}

	return (NDIS_STATUS_SUCCESS);
}

#if WPA_SUPPLICANT_SUPPORT
static void ralink_michael_mic_failure(struct net_device *dev, PCIPHER_KEY pWpaKey)
{
	union iwreq_data wrqu;
	char buf[128];

	/* TODO: needed parameters: count, keyid, key type, TSC */

	//Check for Group or Pairwise MIC error
	if (pWpaKey->Type == PAIRWISE_KEY)
		sprintf(buf, "MLME-MICHAELMICFAILURE.indication unicast");
	else
		sprintf(buf, "MLME-MICHAELMICFAILURE.indication broadcast");
	memset(&wrqu, 0, sizeof(wrqu));
	wrqu.data.length = strlen(buf);
	//send mic error event to wpa_supplicant
	wireless_send_event(dev, IWEVCUSTOM, &wrqu, buf);
}
#endif

/*
	========================================================================

	Routine	Description:
		Process MIC error indication and record MIC error timer.

	Arguments:
		pAd		Pointer	to our adapter
		pWpaKey			Pointer	to the WPA key structure

	Return Value:
		None

	Note:

	========================================================================
*/
VOID	RTMPReportMicError(
	IN	PRTMP_ADAPTER	pAd,
	IN	PCIPHER_KEY		pWpaKey)
{
	unsigned long	Now;
	struct
	{
		NDIS_802_11_STATUS_INDICATION		Status;
		NDIS_802_11_AUTHENTICATION_REQUEST	Request;
	}	Report;

#if WPA_SUPPLICANT_SUPPORT
    if (pAd->PortCfg.WPA_Supplicant == TRUE) {
        //report mic error to wpa_supplicant
        ralink_michael_mic_failure(pAd->net_dev, pWpaKey);
    }
#endif

	// 0. Set Status to indicate auth error
	Report.Status.StatusType = Ndis802_11StatusType_Authentication;

	// 1. Check for Group or Pairwise MIC error
	if (pWpaKey->Type == PAIRWISE_KEY)
		Report.Request.Flags = NDIS_802_11_AUTH_REQUEST_PAIRWISE_ERROR;
	else
		Report.Request.Flags = NDIS_802_11_AUTH_REQUEST_GROUP_ERROR;

	// 2. Copy AP MAC address
	COPY_MAC_ADDR(Report.Request.Bssid, pWpaKey->BssId);

	// 3. Calculate length
	Report.Request.Length = sizeof(NDIS_802_11_AUTHENTICATION_REQUEST);

	// 4. Record Last MIC error time and count
	Now = jiffies;
	if (pAd->PortCfg.MicErrCnt == 0)
	{
		pAd->PortCfg.MicErrCnt++;
		pAd->PortCfg.LastMicErrorTime = Now;
	}
	else if (pAd->PortCfg.MicErrCnt == 1)
	{
		if (time_after(Now, pAd->PortCfg.LastMicErrorTime + 60 * 1000))
		{
			// Update Last MIC error time, this did not violate two MIC errors within 60 seconds
			pAd->PortCfg.LastMicErrorTime = Now;
		}
		else
		{
			pAd->PortCfg.LastMicErrorTime = Now;
			// Violate MIC error counts, MIC countermeasures kicks in
			pAd->PortCfg.MicErrCnt++;
			// We shall block all reception
			// We shall clean all Tx ring and disassoicate from AP after next EAPOL frame
			RTUSBRejectPendingPackets(pAd);
			RTUSBCleanUpDataBulkOutQueue(pAd);
		}
	}
	else
	{
		// MIC error count >= 2
		// This should not happen
		;
	}
}

/*
	==========================================================================
	Description:
		Send out a NULL frame to AP. The prpose is to inform AP this client
		current PSM bit.
	NOTE:
		This routine should only be used in infrastructure mode.
	==========================================================================
 */
VOID	RTMPSendNullFrame(
	IN	PRTMP_ADAPTER	pAd,
	IN	UCHAR			TxRate)
{
	PTX_CONTEXT		pNullContext;
	PTXD_STRUC		pTxD;
	UCHAR			QueIdx =QID_AC_VO;
	PHEADER_802_11	pHdr80211;
	ULONG			TransferBufferLength;
	unsigned long flags;	// For 'Ndis' spin lock

	if(OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_WMM_INUSED))
	{
		QueIdx =QID_AC_VO;
	}
	else
	{
		QueIdx =QID_AC_BE;
	}

	if ((RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_RESET_IN_PROGRESS)) ||
		(RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_BULKOUT_RESET)) ||
		(RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_HALT_IN_PROGRESS)) ||
		(RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST)))
	{
		return;
	}

	// WPA 802.1x secured port control
	if (((pAd->PortCfg.AuthMode == Ndis802_11AuthModeWPA) ||
		(pAd->PortCfg.AuthMode == Ndis802_11AuthModeWPAPSK)
#if WPA_SUPPLICANT_SUPPORT
	    || (pAd->PortCfg.IEEE8021X == TRUE)
#endif
        ) &&
		(pAd->PortCfg.PortSecured == WPA_802_1X_PORT_NOT_SECURED))
	{
		return;
	}

	pNullContext = &(pAd->NullContext);
	if (pNullContext->InUse == FALSE)
	{
		// Set the in use bit
		pNullContext->InUse = TRUE;

		// Fill Null frame body and TxD
		pTxD  = (PTXD_STRUC) &pNullContext->TransferBuffer->TxDesc;
		memset(pTxD, 0, sizeof(TXD_STRUC));

		pHdr80211 = (PHEADER_802_11) &pAd->NullContext.TransferBuffer->NullFrame;
		MgtMacHeaderInit(pAd, pHdr80211, SUBTYPE_NULL_FUNC, 1, pAd->PortCfg.Bssid, pAd->PortCfg.Bssid);
		pHdr80211->Duration = RTMPCalcDuration(pAd, TxRate, 14);
		pHdr80211->FC.Type = BTYPE_DATA;
		pHdr80211->FC.PwrMgmt = (pAd->PortCfg.Psm == PWR_SAVE);


#ifdef BIG_ENDIAN
		RTMPFrameEndianChange(pAd, (PUCHAR)pHdr80211, DIR_WRITE, FALSE);
#endif
		RTUSBWriteTxDescriptor(pAd, pTxD, CIPHER_NONE, 0,0, FALSE, FALSE, FALSE, SHORT_RETRY,
			IFS_BACKOFF, TxRate, sizeof(HEADER_802_11), QueIdx, PID_MGMT_FRAME, FALSE);

		DBGPRINT(RT_DEBUG_INFO, "- (%s) send NULL Frame @%d Mbps...\n",
				__FUNCTION__, RateIdToMbps[TxRate]);
	}

	// Build our URB for USBD
	TransferBufferLength = sizeof(TXD_STRUC) + sizeof(HEADER_802_11);
	if ((TransferBufferLength % 2) == 1)
		TransferBufferLength++;
	if ((TransferBufferLength % pAd->BulkOutMaxPacketSize) == 0)
		TransferBufferLength += 2;

	// Fill out frame length information for global Bulk out arbitor
	pNullContext->BulkOutSize = TransferBufferLength;
	RTUSB_SET_BULK_FLAG(pAd, fRTUSB_BULK_OUT_DATA_NULL);

	// Kick bulk out
	RTUSBKickBulkOut(pAd);
}

VOID	RTMPSendRTSCTSFrame(
	IN	PRTMP_ADAPTER	pAd,
	IN	PUCHAR			pDA,
	IN	ULONG			NextMpduSize,
	IN	UCHAR			TxRate,
	IN	UCHAR			RTSRate,
	IN	USHORT			AckDuration,
	IN	UCHAR			QueIdx,
	IN	UCHAR			FrameGap,
	IN	UCHAR			Type)
{
	PTX_CONTEXT 		pTxContext;
	PTXD_STRUC			pTxD;
	PRTS_FRAME			pRtsFrame;
	PUCHAR				pBuf;
	ULONG				Length = 0;
	ULONG				TransferBufferLength = 0;
	unsigned long flags;	// For 'Ndis' spin lock

	if ((Type != SUBTYPE_RTS) && ( Type != SUBTYPE_CTS))
	{
		DBGPRINT(RT_DEBUG_WARN, "Making RTS/CTS Frame failed, type not matched!\n");
		return;
	}
	else if ((Type == SUBTYPE_RTS) && ((*pDA) & 0x01))
	{
		if ((*pDA) & 0x01)
		{
			// should not use RTS/CTS to protect MCAST frame since no one will reply CTS
			DBGPRINT(RT_DEBUG_INFO,"Not use RTS Frame to proect MCAST frame\n");
			return;
		}
	}

	pTxContext	= &pAd->TxContext[QueIdx][pAd->NextTxIndex[QueIdx]];
	if (pTxContext->InUse == FALSE)
	{
		pTxContext->InUse	= TRUE;
		pTxContext->LastOne = FALSE;
		pAd->NextTxIndex[QueIdx]++;
		if (pAd->NextTxIndex[QueIdx] >= TX_RING_SIZE)
	    {
            pAd->NextTxIndex[QueIdx] = 0;
        }

		pTxD = (PTXD_STRUC) &pTxContext->TransferBuffer->TxDesc;

		pRtsFrame = (PRTS_FRAME) &pTxContext->TransferBuffer->RTSFrame;
		pBuf = (PUCHAR) pRtsFrame;

		memset(pRtsFrame, 0, sizeof(RTS_FRAME));
		pRtsFrame->FC.Type	  = BTYPE_CNTL;
		// CTS-to-self's duration = SIFS + MPDU
		pRtsFrame->Duration = (2 * pAd->PortCfg.Dsifs) + RTMPCalcDuration(pAd, TxRate, NextMpduSize) + AckDuration;// SIFS + Data + SIFS + ACK

		// Write Tx descriptor
		// Don't kick tx start until all frames are prepared
		// RTS has to set more fragment bit for fragment burst
		// RTS did not encrypt
		if (Type == SUBTYPE_RTS)
		{
			DBGPRINT(RT_DEBUG_INFO,"Making RTS Frame\n");

			pRtsFrame->FC.SubType = SUBTYPE_RTS;
			COPY_MAC_ADDR(pRtsFrame->Addr1, pDA);
			COPY_MAC_ADDR(pRtsFrame->Addr2, pAd->CurrentAddress);

			// RTS's duration need to include and extra (SIFS + CTS) time
			pRtsFrame->Duration += (pAd->PortCfg.Dsifs + RTMPCalcDuration(pAd, RTSRate, 14)); // SIFS + CTS-Duration

			Length = sizeof(RTS_FRAME);

#ifdef BIG_ENDIAN
			RTMPFrameEndianChange(pAd, (PUCHAR)pRtsFrame, DIR_WRITE, FALSE);
#endif
			RTUSBWriteTxDescriptor(pAd, pTxD, CIPHER_NONE, 0,0, TRUE, TRUE, FALSE, SHORT_RETRY,
				FrameGap, RTSRate, Length, QueIdx, 0, FALSE);
		}
		else if (Type == SUBTYPE_CTS)
		{
			DBGPRINT(RT_DEBUG_INFO,"Making CTS-to-self Frame\n");
			pRtsFrame->FC.SubType = SUBTYPE_CTS;
			COPY_MAC_ADDR(pRtsFrame->Addr1, pAd->CurrentAddress);

			Length = 10;  //CTS frame length.

#ifdef BIG_ENDIAN
			RTMPFrameEndianChange(pAd, (PUCHAR)pRtsFrame, DIR_WRITE, FALSE);
#endif
			RTUSBWriteTxDescriptor(pAd, pTxD, CIPHER_NONE, 0,0, FALSE, TRUE, FALSE, SHORT_RETRY,
				FrameGap, RTSRate, Length, QueIdx, 0, FALSE);
		}


		// Build our URB for USBD
		TransferBufferLength = sizeof(TXD_STRUC) + Length;
		if ((TransferBufferLength % 2) == 1)
			TransferBufferLength++;
		if ((TransferBufferLength % pAd->BulkOutMaxPacketSize) == 0)
			TransferBufferLength += 2;

		// Fill out frame length information for global Bulk out arbitor
		pTxContext->BulkOutSize = TransferBufferLength;
		pTxContext->bWaitingBulkOut = TRUE;


        pAd->TxRingTotalNumber[QueIdx]++;  // sync. to PendingTx
		RTUSB_SET_BULK_FLAG(pAd, fRTUSB_BULK_OUT_DATA_NORMAL << QueIdx);

	}
}

/*
	========================================================================

	Routine	Description:
		Calculates the duration which is required to transmit out frames
	with given size and specified rate.

	Arguments:
		pAd				Pointer	to our adapter
		Rate			Transmit rate
		Size			Frame size in units of byte

	Return Value:
		Duration number in units of usec

	Note:

	========================================================================
*/
USHORT	RTMPCalcDuration(
	IN	PRTMP_ADAPTER	pAd,
	IN	UCHAR			Rate,
	IN	ULONG			Size)
{
	ULONG	Duration = 0;

	if (Rate < RATE_FIRST_OFDM_RATE) // CCK
	{
		if ((Rate > RATE_1) && (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_SHORT_PREAMBLE_INUSED)))
			Duration = 96;	// 72+24 preamble+plcp
		else
			Duration = 192; // 144+48 preamble+plcp

		Duration += (USHORT)((Size << 4) / RateIdTo500Kbps[Rate]);
		if ((Size << 4) % RateIdTo500Kbps[Rate])
			Duration ++;
	}
	else // OFDM rates
	{
		Duration = 20 + 6;		// 16+4 preamble+plcp + Signal Extension
		Duration += 4 * (USHORT)((11 + Size * 4) / RateIdTo500Kbps[Rate]);
		if ((11 + Size * 4) % RateIdTo500Kbps[Rate])
			Duration += 4;
	}

	return (USHORT)Duration;

}

/*
	========================================================================

	Routine	Description:
		Check the out going frame, if this is an DHCP or ARP datagram
	will be duplicate another frame at low data rate transmit.

	Arguments:
		pAd			Pointer	to our adapter
		pSkb		Pointer to outgoing skb buffer

	Return Value:
		TRUE		To be transmitted at Low data rate transmit. (1Mbps/6Mbps)
		FALSE		Do nothing.

	Note:

		MAC header + IP Header + UDP Header
		  14 Bytes	  20 Bytes

		UDP Header
		00|01|02|03|04|05|06|07|08|09|10|11|12|13|14|15|
						Source Port
		16|17|18|19|20|21|22|23|24|25|26|27|28|29|30|31|
					Destination Port

		port 0x43 means Bootstrap Protocol, server.
		Port 0x44 means Bootstrap Protocol, client.

	========================================================================
*/
BOOLEAN 	RTMPCheckDHCPFrame(
	IN	PRTMP_ADAPTER	pAd,
	IN	struct sk_buff	*pSkb)
{
	PUCHAR			pSrc;
	ULONG			SrcLen = 0;

	pSrc = (PVOID)pSkb->data;
	SrcLen = pSkb->len;

	// Check ARP packet
	if (SrcLen >= 13)
	{
		if ((pSrc[12] == 0x08) && (pSrc[13] == 0x06))
		{
			DBGPRINT(RT_DEBUG_INFO,"RTMPCheckDHCPFrame - ARP packet\n");
			return TRUE;
		}
	}

	// Check foe DHCP & BOOTP protocol
	if (SrcLen >= 37)
	{
		if ((pSrc[12] == 0x08) && (pSrc[13] == 0x00) && // It's an IP packet
		    ((pSrc[14] & 0xf0) == 0x40) && // It's IPv4
		    (pSrc[23] == 17) && // It's UDP
		    ((pSrc[36] == 0x00) && ((pSrc[37] == 0x43) || (pSrc[37] == 0x44))) ) // dest port is a DHCP port
		{
			DBGPRINT(RT_DEBUG_INFO,"RTMPCheckDHCPFrame - DHCP packet\n");
			return TRUE;
		}
		else
		{
			#ifdef DBG
			int is_ip = ((pSrc[12] == 0x08) && (pSrc[13] == 0x00));
			int is_ipv4 = ((pSrc[14] & 0xf0) == 0x40);
			int is_udp = (pSrc[23] == 17);
			int dest_port = (pSrc[36] << 8) | pSrc[37];
			#endif

			DBGPRINT(RT_DEBUG_INFO,"RTMPCheckDHCPFrame - not DHCP, ip %d ipv4 %d udp %d destport %d\n",
				 is_ip, is_ipv4, is_udp, dest_port );
		}

	}

	return FALSE;
}
