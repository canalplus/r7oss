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
 *	Module Name:	assoc.c
 *
 *	Abstract:
 *
 *	Revision History:
 *	Who		When		What
 *	--------	----------	-----------------------------
 *	Name		Date		Modification logs
 *	John		2004-9-3	porting from RT2500
 *
 ***************************************************************************/

#include "rt_config.h"
#include <net/iw_handler.h>



UCHAR	CipherWpaTemplate[] = {
		0xdd, 					// WPA IE
		0x16,					// Length
		0x00, 0x50, 0xf2, 0x01,	// oui
		0x01, 0x00,				// Version
		0x00, 0x50, 0xf2, 0x02,	// Multicast
		0x01, 0x00,				// Number of unicast
		0x00, 0x50, 0xf2, 0x02,	// unicast
		0x01, 0x00,				// number of authentication method
		0x00, 0x50, 0xf2, 0x01	// authentication
		};
UCHAR	CipherWpa2Template[] = {
		0x30,					// RSN IE
		0x14,					// Length
		0x01, 0x00,				// Version
		0x00, 0x0f, 0xac, 0x02,	// group cipher, TKIP
		0x01, 0x00,				// number of pairwise
		0x00, 0x0f, 0xac, 0x02,	// unicast
		0x01, 0x00,				// number of authentication method
		0x00, 0x0f, 0xac, 0x02,	// authentication
		0x00, 0x00,				// RSN capability
		};
UCHAR   CipherWpa2TemplateLen = (sizeof(CipherWpa2Template) / sizeof(UCHAR));

/*
    ==========================================================================
    Description:
        association state machine init, including state transition and timer init
		All functions run in process context

    Parameters:
        S - pointer to the association state machine
    Note:
        The state machine looks like the following

                               ASSOC_IDLE               ASSOC_WAIT_RSP             REASSOC_WAIT_RSP             DISASSOC_WAIT_RSP
    MT2_MLME_ASSOC_REQ       mlme_assoc_req_action    invalid_state_when_assoc   invalid_state_when_assoc       invalid_state_when_assoc
    MT2_MLME_REASSOC_REQ     mlme_reassoc_req_action  invalid_state_when_reassoc invalid_state_when_reassoc     invalid_state_when_reassoc
    MT2_MLME_DISASSOC_REQ    mlme_disassoc_req_action mlme_disassoc_req_action   mlme_disassoc_req_action       mlme_disassoc_req_action
    MT2_PEER_DISASSOC_REQ    peer_disassoc_action     peer_disassoc_action       peer_disassoc_action           peer_disassoc_action
    MT2_PEER_ASSOC_REQ       drop                     drop                       drop                           drop
    MT2_PEER_ASSOC_RSP       drop                     peer_assoc_rsp_action      drop                           drop
    MT2_PEER_REASSOC_REQ     drop                     drop                       drop                           drop
    MT2_PEER_REASSOC_RSP     drop                     drop                       peer_reassoc_rsp_action        drop
    MT2_CLS3ERR              cls3err_action           cls3err_action             cls3err_action                 cls3err_action
    MT2_ASSOC_TIMEOUT        timer_nop                assoc_timeout_action       timer_nop                      timer_nop
    MT2_REASSOC_TIMEOUT      timer_nop                timer_nop                  reassoc_timeout_action         timer_nop
    MT2_DISASSOC_TIMEOUT     timer_nop                timer_nop                  timer_nop                      disassoc_timeout_action
    ==========================================================================
 */
VOID AssocStateMachineInit(
    IN	PRTMP_ADAPTER	pAd,
    IN  STATE_MACHINE *S,
    OUT STATE_MACHINE_FUNC Trans[])
{
    StateMachineInit(S, (STATE_MACHINE_FUNC*)Trans, MAX_ASSOC_STATE, MAX_ASSOC_MSG, (STATE_MACHINE_FUNC)Drop, ASSOC_IDLE, ASSOC_MACHINE_BASE);

    // first column
    StateMachineSetAction(S, ASSOC_IDLE, MT2_MLME_ASSOC_REQ, (STATE_MACHINE_FUNC)MlmeAssocReqAction);
    StateMachineSetAction(S, ASSOC_IDLE, MT2_MLME_REASSOC_REQ, (STATE_MACHINE_FUNC)MlmeReassocReqAction);
    StateMachineSetAction(S, ASSOC_IDLE, MT2_MLME_DISASSOC_REQ, (STATE_MACHINE_FUNC)MlmeDisassocReqAction);
    StateMachineSetAction(S, ASSOC_IDLE, MT2_PEER_DISASSOC_REQ, (STATE_MACHINE_FUNC)PeerDisassocAction);

    // second column
    StateMachineSetAction(S, ASSOC_WAIT_RSP, MT2_MLME_ASSOC_REQ, (STATE_MACHINE_FUNC)InvalidStateWhenAssoc);
    StateMachineSetAction(S, ASSOC_WAIT_RSP, MT2_MLME_REASSOC_REQ, (STATE_MACHINE_FUNC)InvalidStateWhenReassoc);
    StateMachineSetAction(S, ASSOC_WAIT_RSP, MT2_MLME_DISASSOC_REQ, (STATE_MACHINE_FUNC)InvalidStateWhenDisassociate);
    StateMachineSetAction(S, ASSOC_WAIT_RSP, MT2_PEER_DISASSOC_REQ, (STATE_MACHINE_FUNC)PeerDisassocAction);
    StateMachineSetAction(S, ASSOC_WAIT_RSP, MT2_PEER_ASSOC_RSP, (STATE_MACHINE_FUNC)PeerAssocRspAction);
    StateMachineSetAction(S, ASSOC_WAIT_RSP, MT2_ASSOC_TIMEOUT, (STATE_MACHINE_FUNC)AssocTimeoutAction);

    // third column
    StateMachineSetAction(S, REASSOC_WAIT_RSP, MT2_MLME_ASSOC_REQ, (STATE_MACHINE_FUNC)InvalidStateWhenAssoc);
    StateMachineSetAction(S, REASSOC_WAIT_RSP, MT2_MLME_REASSOC_REQ, (STATE_MACHINE_FUNC)InvalidStateWhenReassoc);
    StateMachineSetAction(S, REASSOC_WAIT_RSP, MT2_MLME_DISASSOC_REQ, (STATE_MACHINE_FUNC)InvalidStateWhenDisassociate);
    StateMachineSetAction(S, REASSOC_WAIT_RSP, MT2_PEER_DISASSOC_REQ, (STATE_MACHINE_FUNC)PeerDisassocAction);
    StateMachineSetAction(S, REASSOC_WAIT_RSP, MT2_PEER_REASSOC_RSP, (STATE_MACHINE_FUNC)PeerReassocRspAction);
    StateMachineSetAction(S, REASSOC_WAIT_RSP, MT2_REASSOC_TIMEOUT, (STATE_MACHINE_FUNC)ReassocTimeoutAction);

    // fourth column
    StateMachineSetAction(S, DISASSOC_WAIT_RSP, MT2_MLME_ASSOC_REQ, (STATE_MACHINE_FUNC)InvalidStateWhenAssoc);
    StateMachineSetAction(S, DISASSOC_WAIT_RSP, MT2_MLME_REASSOC_REQ, (STATE_MACHINE_FUNC)InvalidStateWhenReassoc);
    StateMachineSetAction(S, DISASSOC_WAIT_RSP, MT2_MLME_DISASSOC_REQ, (STATE_MACHINE_FUNC)InvalidStateWhenDisassociate);
    StateMachineSetAction(S, DISASSOC_WAIT_RSP, MT2_PEER_DISASSOC_REQ, (STATE_MACHINE_FUNC)PeerDisassocAction);
    StateMachineSetAction(S, DISASSOC_WAIT_RSP, MT2_DISASSOC_TIMEOUT, (STATE_MACHINE_FUNC)DisassocTimeoutAction);

    // timer init
    RTMPInitTimer(pAd, &pAd->MlmeAux.AssocTimer, &AssocTimeout);
    RTMPInitTimer(pAd, &pAd->MlmeAux.ReassocTimer, &ReassocTimeout);

}

/*
    ==========================================================================
    Description:
        Association timeout procedure. After association timeout, this function
        will be called and it will put a message into the MLME queue
    Parameters:
        Standard timer parameters
    ==========================================================================
 */
VOID AssocTimeout(
    IN	unsigned long data)
{
    RTMP_ADAPTER *pAd = (RTMP_ADAPTER *)data;
    DBGPRINT(RT_DEBUG_TRACE,"ASSOC - enqueue MT2_ASSOC_TIMEOUT \n");
    MlmeEnqueue(pAd, ASSOC_STATE_MACHINE, MT2_ASSOC_TIMEOUT, 0, NULL);
    RTUSBMlmeUp(pAd);
}

/*
    ==========================================================================
    Description:
        Reassociation timeout procedure. After reassociation timeout, this
        function will be called and put a message into the MLME queue
    Parameters:
        Standard timer parameters
    ==========================================================================
 */
VOID ReassocTimeout(
    IN	unsigned long data)
{
    RTMP_ADAPTER *pAd = (RTMP_ADAPTER *)data;
    DBGPRINT(RT_DEBUG_TRACE,"ASSOC - enqueue MT2_REASSOC_TIMEOUT \n");
    MlmeEnqueue(pAd, ASSOC_STATE_MACHINE, MT2_REASSOC_TIMEOUT, 0, NULL);
    RTUSBMlmeUp(pAd);
}

/*
	==========================================================================
	Description:
		mlme assoc req handling procedure
	Parameters:
		Adapter - Adapter pointer
		Elem - MLME Queue Element
	Pre:
		the station has been authenticated and the following information is stored in the config
			-# SSID
			-# supported rates and their length
			-# listen interval (Adapter->PortCfg.default_listen_count)
			-# Transmit power  (Adapter->PortCfg.tx_power)
	Post  :
		-# An association request frame is generated and sent to the air
		-# Association timer starts
		-# Association state -> ASSOC_WAIT_RSP

	==========================================================================
 */
VOID MlmeAssocReqAction(
	IN PRTMP_ADAPTER pAd,
	IN MLME_QUEUE_ELEM *Elem)
{
	UCHAR			 ApAddr[6];
	HEADER_802_11	 AssocHdr;
	UCHAR			 WmeIe[9] = {IE_VENDOR_SPECIFIC, 0x07, 0x00, 0x50, 0xf2, 0x02, 0x00, 0x01, STA_QOS_CAPABILITY};
	UCHAR			 CipherTmp[64];
	UCHAR			 CipherTmpLen;
	USHORT			 ListenIntv;
	ULONG			 Timeout;
	USHORT			 CapabilityInfo;
	PUCHAR			 pOutBuffer = NULL;
	ULONG			 FrameLen = 0;
	ULONG			 tmp;
	UCHAR			 VarIesOffset;
	USHORT			 Status;
	ULONG			 idx;
	BOOLEAN			 FoundPMK = FALSE;
	USHORT			 NStatus;

	// Block all authentication request durning WPA block period
	if (pAd->PortCfg.bBlockAssoc == TRUE)
	{
		DBGPRINT(RT_DEBUG_TRACE, "ASSOC - Block Assoc request durning WPA block period!\n");
		pAd->Mlme.AssocMachine.CurrState = ASSOC_IDLE;
		Status = MLME_STATE_MACHINE_REJECT;
		MlmeEnqueue(pAd, MLME_CNTL_STATE_MACHINE, MT2_ASSOC_CONF, 2, &Status);
	}
	// check sanity first
	else if (MlmeAssocReqSanity(pAd, Elem->Msg, Elem->MsgLen, ApAddr, &CapabilityInfo, &Timeout, &ListenIntv))
	{
		RTMPCancelTimer(&pAd->MlmeAux.AssocTimer);
		COPY_MAC_ADDR(pAd->MlmeAux.Bssid, ApAddr);

		// allocate and send out AssocRsp frame
		NStatus = MlmeAllocateMemory(pAd, (PVOID)&pOutBuffer);	//Get an unused nonpaged memory
		if (NStatus != NDIS_STATUS_SUCCESS)
		{
			DBGPRINT(RT_DEBUG_TRACE,"ASSOC - MlmeAssocReqAction() allocate memory failed \n");
			pAd->Mlme.AssocMachine.CurrState = ASSOC_IDLE;
			Status = MLME_FAIL_NO_RESOURCE;
			MlmeEnqueue(pAd, MLME_CNTL_STATE_MACHINE, MT2_ASSOC_CONF, 2, &Status);
			return;
		}

		// Add by James 03/06/27
		pAd->PortCfg.AssocInfo.Length = sizeof(NDIS_802_11_ASSOCIATION_INFORMATION);
		// Association don't need to report MAC address
		pAd->PortCfg.AssocInfo.AvailableRequestFixedIEs =
			NDIS_802_11_AI_REQFI_CAPABILITIES | NDIS_802_11_AI_REQFI_LISTENINTERVAL;
		pAd->PortCfg.AssocInfo.RequestFixedIEs.Capabilities = CapabilityInfo;
		pAd->PortCfg.AssocInfo.RequestFixedIEs.ListenInterval = ListenIntv;
		// Only reassociate need this
		//COPY_MAC_ADDR(pAd->PortCfg.AssocInfo.RequestFixedIEs.CurrentAPAddress, ApAddr);
		pAd->PortCfg.AssocInfo.OffsetRequestIEs = sizeof(NDIS_802_11_ASSOCIATION_INFORMATION);

		// First add SSID
		VarIesOffset = 0;
		memcpy(pAd->PortCfg.ReqVarIEs + VarIesOffset, &SsidIe, 1);
		VarIesOffset += 1;
		memcpy(pAd->PortCfg.ReqVarIEs + VarIesOffset, &pAd->MlmeAux.SsidLen, 1);
		VarIesOffset += 1;
		memcpy(pAd->PortCfg.ReqVarIEs + VarIesOffset, pAd->MlmeAux.Ssid, pAd->MlmeAux.SsidLen);
		VarIesOffset += pAd->MlmeAux.SsidLen;

		// Second add Supported rates
		memcpy(pAd->PortCfg.ReqVarIEs + VarIesOffset, &SupRateIe, 1);
		VarIesOffset += 1;
		memcpy(pAd->PortCfg.ReqVarIEs + VarIesOffset, &pAd->MlmeAux.SupRateLen, 1);
		VarIesOffset += 1;
		memcpy(pAd->PortCfg.ReqVarIEs + VarIesOffset, pAd->MlmeAux.SupRate, pAd->MlmeAux.SupRateLen);
		VarIesOffset += pAd->MlmeAux.SupRateLen;
		// End Add by James

		DBGPRINT(RT_DEBUG_TRACE, "ASSOC - Send ASSOC request...\n");
		MgtMacHeaderInit(pAd, &AssocHdr, SUBTYPE_ASSOC_REQ, 0, ApAddr, ApAddr);

		// Build basic frame first
		MakeOutgoingFrame(pOutBuffer,				&FrameLen,
						  sizeof(HEADER_802_11),	&AssocHdr,
						  2,						&CapabilityInfo,
						  2,						&ListenIntv,
						  1,						&SsidIe,
						  1,						&pAd->MlmeAux.SsidLen,
						  pAd->MlmeAux.SsidLen, 	pAd->MlmeAux.Ssid,
						  1,						&SupRateIe,
						  1,						&pAd->MlmeAux.SupRateLen,
						  pAd->MlmeAux.SupRateLen,	pAd->MlmeAux.SupRate,
						  END_OF_ARGS);

		if (pAd->MlmeAux.ExtRateLen != 0)
		{
			MakeOutgoingFrame(pOutBuffer + FrameLen,	&tmp,
							  1,						&ExtRateIe,
							  1,						&pAd->MlmeAux.ExtRateLen,
							  pAd->MlmeAux.ExtRateLen,	pAd->MlmeAux.ExtRate,
							  END_OF_ARGS);
			FrameLen += tmp;
		}

		if (pAd->MlmeAux.APEdcaParm.bValid)
		{
			WmeIe[8] |= (pAd->MlmeAux.APEdcaParm.EdcaUpdateCount & 0x0f);
			MakeOutgoingFrame(pOutBuffer + FrameLen,	&tmp,
							  9,						&WmeIe[0],
							  END_OF_ARGS);
			FrameLen += tmp;
		}


		// For WPA / WPA-PSK
		if ((pAd->PortCfg.AuthMode == Ndis802_11AuthModeWPA) ||
			(pAd->PortCfg.AuthMode == Ndis802_11AuthModeWPAPSK))
		{
			// Copy WPA template to buffer
			CipherTmpLen = sizeof(CipherWpaTemplate);
			memcpy(CipherTmp, CipherWpaTemplate, CipherTmpLen);
			// Modify Group cipher
			CipherTmp[11] = ((pAd->PortCfg.GroupCipher == Ndis802_11Encryption2Enabled) ? 0x2 : 0x4);
			// Modify Pairwise cipher
			CipherTmp[17] = ((pAd->PortCfg.PairCipher == Ndis802_11Encryption2Enabled) ? 0x2 : 0x4);
			// Modify AKM
			CipherTmp[23] = ((pAd->PortCfg.AuthMode == Ndis802_11AuthModeWPA) ? 0x1 : 0x2);
			// Make outgoing frame
			MakeOutgoingFrame(pOutBuffer + FrameLen,	&tmp,
							  CipherTmpLen,				&CipherTmp[0],
							  END_OF_ARGS);
			FrameLen += tmp;

			// Append Variable IE
			memcpy(pAd->PortCfg.ReqVarIEs + VarIesOffset, CipherTmp, CipherTmpLen);
			VarIesOffset += CipherTmpLen;

			// Set Variable IEs Length
			pAd->PortCfg.ReqVarIELen = VarIesOffset;
			pAd->PortCfg.AssocInfo.RequestIELength = VarIesOffset;
			// OffsetResponseIEs follow ReqVarIE
			pAd->PortCfg.AssocInfo.OffsetResponseIEs = sizeof(NDIS_802_11_ASSOCIATION_INFORMATION) + pAd->PortCfg.ReqVarIELen;
		}
		// For WPA2 / WPA2-PSK
		else if ((pAd->PortCfg.AuthMode == Ndis802_11AuthModeWPA2) ||
				 (pAd->PortCfg.AuthMode == Ndis802_11AuthModeWPA2PSK))
		{
			// Copy WPA2 template to buffer
			CipherTmpLen = sizeof(CipherWpa2Template);
			memcpy(CipherTmp, CipherWpa2Template, CipherTmpLen);
			// Modify Group cipher
			CipherTmp[7] = ((pAd->PortCfg.GroupCipher == Ndis802_11Encryption2Enabled) ? 0x2 : 0x4);
			// Modify Pairwise cipher
			CipherTmp[13] = ((pAd->PortCfg.PairCipher == Ndis802_11Encryption2Enabled) ? 0x2 : 0x4);
			// Modify AKM
			CipherTmp[19] = ((pAd->PortCfg.AuthMode == Ndis802_11AuthModeWPA2) ? 0x1 : 0x2);
			// Check for WPA PMK cache list
			if (pAd->PortCfg.AuthMode == Ndis802_11AuthModeWPA2)
			{
				// Search chched PMKID, append it if existed
				for (idx = 0; idx < PMKID_NO; idx++)
				{
					if (NdisEqualMemory(ApAddr, &pAd->PortCfg.SavedPMK[idx].BSSID, 6))
					{
						FoundPMK = TRUE;
						break;
					}
				}

				if (FoundPMK)
				{
					// Update length within RSN IE
					CipherTmp[1] += 18;
					// Set PMK number
					*(PUSHORT) &CipherTmp[CipherTmpLen] = 1;
					memcpy(&CipherTmp[CipherTmpLen + 2], &pAd->PortCfg.SavedPMK[idx].PMKID, 16);
					CipherTmpLen += 18;
				}
			}

			// Make outgoing frame
			MakeOutgoingFrame(pOutBuffer + FrameLen,	&tmp,
							  CipherTmpLen,				&CipherTmp[0],
							  END_OF_ARGS);
			FrameLen += tmp;

			// Append Variable IE
			memcpy(pAd->PortCfg.ReqVarIEs + VarIesOffset, CipherTmp, CipherTmpLen);
			VarIesOffset += CipherTmpLen;

			// Set Variable IEs Length
			pAd->PortCfg.ReqVarIELen = VarIesOffset;
			pAd->PortCfg.AssocInfo.RequestIELength = VarIesOffset;
			// OffsetResponseIEs follow ReqVarIE
			pAd->PortCfg.AssocInfo.OffsetResponseIEs = sizeof(NDIS_802_11_ASSOCIATION_INFORMATION) + pAd->PortCfg.ReqVarIELen;
		}
		else
		{
			// Do nothing
			;
		}
#if 0 //AGGREGATION_SUPPORT
		// add Ralink proprietary IE to inform AP this STA is going to use AGGREGATION, only when -
		// 1. user enable aggregation, AND
		// 2. AP annouces it's AGGREGATION-capable in BEACON
		if (pAd->PortCfg.bAggregationCapable && (pAd->MlmeAux.APRalinkIe & 0x00000001))
		{
			ULONG TmpLen;
			UCHAR RalinkIe[9] = {IE_VENDOR_SPECIFIC, 7, 0x00, 0x0c, 0x43, 0x01, 0x00, 0x00, 0x00};
			MakeOutgoingFrame(pOutBuffer+FrameLen,			 &TmpLen,
							  9,							 RalinkIe,
							  END_OF_ARGS);
			FrameLen += TmpLen;
		}
#endif

		MiniportMMRequest(pAd, pOutBuffer, FrameLen);

		RTMPSetTimer(pAd, &pAd->MlmeAux.AssocTimer, Timeout);
		pAd->Mlme.AssocMachine.CurrState = ASSOC_WAIT_RSP;
	}
	else
	{
		DBGPRINT(RT_DEBUG_TRACE,"ASSOC - MlmeAssocReqAction() sanity check failed. BUG!!!!!! \n");
		pAd->Mlme.AssocMachine.CurrState = ASSOC_IDLE;
		Status = MLME_INVALID_FORMAT;
		MlmeEnqueue(pAd, MLME_CNTL_STATE_MACHINE, MT2_ASSOC_CONF, 2, &Status);
	}

}

/*
	==========================================================================
	Description:
		mlme reassoc req handling procedure
	Parameters:
		Elem -
	Pre:
		-# SSID  (Adapter->PortCfg.ssid[])
		-# BSSID (AP address, Adapter->PortCfg.bssid)
		-# Supported rates (Adapter->PortCfg.supported_rates[])
		-# Supported rates length (Adapter->PortCfg.supported_rates_len)
		-# Tx power (Adapter->PortCfg.tx_power)

	==========================================================================
 */
VOID MlmeReassocReqAction(
	IN PRTMP_ADAPTER pAd,
	IN MLME_QUEUE_ELEM *Elem)
{
	UCHAR			ApAddr[6];
	HEADER_802_11	ReassocHdr;
	UCHAR			WmeIe[9] = {IE_VENDOR_SPECIFIC, 0x07, 0x00, 0x50, 0xf2, 0x02, 0x00, 0x01, STA_QOS_CAPABILITY};
	USHORT			CapabilityInfo, ListenIntv;
	ULONG			Timeout;
	ULONG			FrameLen = 0;
	ULONG			tmp;
	PUCHAR			pOutBuffer = NULL;
	USHORT			Status;
	USHORT			NStatus;

	// Block all authentication request durning WPA block period
	if (pAd->PortCfg.bBlockAssoc == TRUE)
	{
		DBGPRINT(RT_DEBUG_TRACE, "ASSOC - Block ReAssoc request durning WPA block period!\n");
		pAd->Mlme.AssocMachine.CurrState = ASSOC_IDLE;
		Status = MLME_STATE_MACHINE_REJECT;
		MlmeEnqueue(pAd, MLME_CNTL_STATE_MACHINE, MT2_REASSOC_CONF, 2, &Status);
	}
	// the parameters are the same as the association
	else if(MlmeAssocReqSanity(pAd, Elem->Msg, Elem->MsgLen, ApAddr, &CapabilityInfo, &Timeout, &ListenIntv))
	{
		RTMPCancelTimer(&pAd->MlmeAux.ReassocTimer);

		// allocate and send out ReassocReq frame
		NStatus = MlmeAllocateMemory(pAd, (PVOID)&pOutBuffer);	//Get an unused nonpaged memory
		if (NStatus != NDIS_STATUS_SUCCESS)
		{
			DBGPRINT(RT_DEBUG_TRACE,"ASSOC - MlmeReassocReqAction() allocate memory failed \n");
			pAd->Mlme.AssocMachine.CurrState = ASSOC_IDLE;
			Status = MLME_FAIL_NO_RESOURCE;
			MlmeEnqueue(pAd, MLME_CNTL_STATE_MACHINE, MT2_REASSOC_CONF, 2, &Status);
			return;
		}

		COPY_MAC_ADDR(pAd->MlmeAux.Bssid, ApAddr);

		// make frame, use bssid as the AP address??
		DBGPRINT(RT_DEBUG_TRACE, "ASSOC - Send RE-ASSOC request...\n");
		MgtMacHeaderInit(pAd, &ReassocHdr, SUBTYPE_REASSOC_REQ, 0, ApAddr, ApAddr);
		MakeOutgoingFrame(pOutBuffer,				&FrameLen,
						  sizeof(HEADER_802_11),	&ReassocHdr,
						  2,						&CapabilityInfo,
						  2,						&ListenIntv,
						  MAC_ADDR_LEN, 			ApAddr,
						  1,						&SsidIe,
						  1,						&pAd->MlmeAux.SsidLen,
						  pAd->MlmeAux.SsidLen, 	pAd->MlmeAux.Ssid,
						  1,						&SupRateIe,
						  1,						&pAd->MlmeAux.SupRateLen,
						  pAd->MlmeAux.SupRateLen,	pAd->MlmeAux.SupRate,
						  END_OF_ARGS);

		if (pAd->MlmeAux.ExtRateLen != 0)
		{
			MakeOutgoingFrame(pOutBuffer + FrameLen,		&tmp,
							  1,							&ExtRateIe,
							  1,							&pAd->MlmeAux.ExtRateLen,
							  pAd->MlmeAux.ExtRateLen,		pAd->MlmeAux.ExtRate,
							  END_OF_ARGS);
			FrameLen += tmp;
		}

		if (pAd->MlmeAux.APEdcaParm.bValid)
		{
			WmeIe[8] |= (pAd->MlmeAux.APEdcaParm.EdcaUpdateCount & 0x0f);
			MakeOutgoingFrame(pOutBuffer + FrameLen,	&tmp,
							  9,						&WmeIe[0],
							  END_OF_ARGS);
			FrameLen += tmp;
		}
#if 0 //AGGREGATION_SUPPORT
		// add Ralink proprietary IE to inform AP this STA is going to use AGGREGATION, only when -
		// 1. user enable aggregation, AND
		// 2. AP annouces it's AGGREGATION-capable in BEACON
		if (pAd->PortCfg.bAggregationCapable && (pAd->MlmeAux.APRalinkIe & 0x00000001))
		{
			ULONG TmpLen;
			UCHAR RalinkIe[9] = {IE_VENDOR_SPECIFIC, 7, 0x00, 0x0c, 0x43, 0x01, 0x00, 0x00, 0x00};
			MakeOutgoingFrame(pOutBuffer+FrameLen,			 &TmpLen,
							  9,							 RalinkIe,
							  END_OF_ARGS);
			FrameLen += TmpLen;
		}
#endif
		MiniportMMRequest(pAd, pOutBuffer, FrameLen);

		RTMPSetTimer(pAd, &pAd->MlmeAux.ReassocTimer, Timeout);
		pAd->Mlme.AssocMachine.CurrState = REASSOC_WAIT_RSP;
	}
	else
	{
		DBGPRINT(RT_DEBUG_TRACE,"ASSOC - MlmeReassocReqAction() sanity check failed. BUG!!!! \n");
		pAd->Mlme.AssocMachine.CurrState = ASSOC_IDLE;
		Status = MLME_INVALID_FORMAT;
		MlmeEnqueue(pAd, MLME_CNTL_STATE_MACHINE, MT2_REASSOC_CONF, 2, &Status);
	}
}

/*
    ==========================================================================
    Description:
        Upper layer issues disassoc request
    Parameters:
        Elem -
    ==========================================================================
 */
VOID MlmeDisassocReqAction(
    IN PRTMP_ADAPTER pAd,
    IN MLME_QUEUE_ELEM *Elem)
{
    PMLME_DISASSOC_REQ_STRUCT pDisassocReq;
    HEADER_802_11         DisassocHdr;
    PCHAR                 pOutBuffer = NULL;
    ULONG                 FrameLen = 0;
    USHORT                Status;
    USHORT                NStatus;
#if WPA_SUPPLICANT_SUPPORT
    union iwreq_data      wrqu;
#endif

    // skip sanity check
    pDisassocReq = (PMLME_DISASSOC_REQ_STRUCT)(Elem->Msg);

    // allocate and send out DeassocReq frame
    NStatus = MlmeAllocateMemory(pAd, (PVOID)&pOutBuffer);  //Get an unused nonpaged memory
    if (NStatus != NDIS_STATUS_SUCCESS)
    {
        DBGPRINT(RT_DEBUG_TRACE, "ASSOC - MlmeDisassocReqAction() allocate memory failed\n");
        Status = MLME_FAIL_NO_RESOURCE;
        MlmeEnqueue(pAd, MLME_CNTL_STATE_MACHINE, MT2_DISASSOC_CONF, 2, &Status);
        return;
    }

    DBGPRINT(RT_DEBUG_TRACE, "ASSOC - Send DISASSOC request\n");
    MgtMacHeaderInit(pAd, &DisassocHdr, SUBTYPE_DISASSOC, 0, pDisassocReq->Addr, pDisassocReq->Addr);
    MakeOutgoingFrame(pOutBuffer,           &FrameLen,
                      sizeof(HEADER_802_11),&DisassocHdr,
                      2,                    &pDisassocReq->Reason,
                      END_OF_ARGS);
    MiniportMMRequest(pAd, pOutBuffer, FrameLen);


	// Set the control aux SSID to prevent it reconnect to old SSID
	// Since calling this indicate user don't want to connect to that SSID anymore.
	// 2004-11-10 can't reset this info, cause it may be the new SSID that user requests for
	// pAd->MlmeAux.SsidLen = MAX_LEN_OF_SSID;
	// NdisZeroMemory(pAd->MlmeAux.Ssid, MAX_LEN_OF_SSID);
    // NdisZeroMemory(pAd->MlmeAux.Bssid, MAC_ADDR_LEN);

#if WPA_SUPPLICANT_SUPPORT
    if (pAd->PortCfg.WPA_Supplicant == TRUE) {
        //send disassociate event to wpa_supplicant
        memset(&wrqu, 0, sizeof(wrqu));
        wrqu.data.flags = RT_DISASSOC_EVENT_FLAG;
        wireless_send_event(pAd->net_dev, IWEVCUSTOM, &wrqu, NULL);
    }
#endif

    pAd->PortCfg.DisassocReason = pDisassocReq->Reason;
    COPY_MAC_ADDR(pAd->PortCfg.DisassocSta, pDisassocReq->Addr);

	Status = MLME_SUCCESS;
	MlmeEnqueue(pAd, MLME_CNTL_STATE_MACHINE, MT2_DISASSOC_CONF, 2, &Status);
}

#if WPA_SUPPLICANT_SUPPORT
#ifdef DBG
static void _rtmp_hexdump(int level, const char *title, const u8 *buf,
			 size_t len, int show)
{
	size_t i;

	DBGPRINT(level, "%s - hexdump(len=%lu):", title, (unsigned long)len);
	if (show) {
		for (i = 0; i < len; i++)
			DBGPRINT_RAW(level, " %02x", buf[i]);
	} else {
		DBGPRINT_RAW(level, " [REMOVED]");
	}
	DBGPRINT_RAW(level, "\n");
}

void rtmp_hexdump(int level, const char *title, const u8 *buf, size_t len)
{
	_rtmp_hexdump(level, title, buf, len, 1);
}
#endif

NDIS_STATUS miniport_query_info(
     IN PRTMP_ADAPTER pAd,
     IN USHORT oid,
     OUT void *buf,
     OUT ULONG bufsize)
{
    // First add AssocInfo
	memcpy(buf, &pAd->PortCfg.AssocInfo, sizeof(NDIS_802_11_ASSOCIATION_INFORMATION));
	// Second add ReqVarIEs
	memcpy(buf + sizeof(NDIS_802_11_ASSOCIATION_INFORMATION), pAd->PortCfg.ReqVarIEs, pAd->PortCfg.ReqVarIELen);
	// Third add ResVarIEs
	memcpy(buf + sizeof(NDIS_802_11_ASSOCIATION_INFORMATION) + pAd->PortCfg.ReqVarIELen, pAd->PortCfg.ResVarIEs, pAd->PortCfg.ResVarIELen);

	return 0;
}

VOID link_status_handler(
    IN PRTMP_ADAPTER pAd)
{
	NDIS_802_11_ASSOCIATION_INFORMATION *ndis_assoc_info;

	unsigned char *wpa_assoc_info_req, *wpa_assoc_info_resp, *ies;
	unsigned char *p;
	int i;
	unsigned char *assoc_info;
	union iwreq_data wrqu;
	NDIS_STATUS res;

	const int assoc_size = sizeof(*ndis_assoc_info) + IW_CUSTOM_MAX;
	assoc_info = kmalloc(assoc_size, MEM_ALLOC_FLAG);
	if (!assoc_info) {
              DBGPRINT(RT_DEBUG_TRACE, "couldn't allocate memory\n");
		return;
	}
	memset(assoc_info, 0, assoc_size);
	ndis_assoc_info = (NDIS_802_11_ASSOCIATION_INFORMATION *)assoc_info;

	res = miniport_query_info(pAd, OID_802_11_ASSOCIATION_INFORMATION,
				  assoc_info, assoc_size);
	if (res) {
		DBGPRINT(RT_DEBUG_TRACE, "query assoc_info failed\n");
		if(assoc_info != NULL){
			kfree(assoc_info);
		}
		return;
	}

	/*
	 * TODO: backwards compatibility would require that IWEVCUSTOM
	 * is sent even if WIRELESS_EXT > 17. This version does not do
	 * this in order to allow wpa_supplicant to be tested with
	 * WE-18.
	 */

#ifdef DBG
	rtmp_hexdump(RT_DEBUG_TRACE, "ASSOCINFO", (const u8 *) ndis_assoc_info,
		    sizeof(NDIS_802_11_ASSOCIATION_INFORMATION));
#endif

    wpa_assoc_info_req = kmalloc(IW_CUSTOM_MAX, MEM_ALLOC_FLAG);
	if (!wpa_assoc_info_req) {
		DBGPRINT(RT_DEBUG_TRACE, "couldn't allocate memory\n");
		//kfree(wpa_assoc_info_req);//Thomas
		return;
	}

    //send ReqIEs
    memset(wpa_assoc_info_req, 0, IW_CUSTOM_MAX);
	p = wpa_assoc_info_req;
	p += sprintf(p, "ASSOCINFO(ReqIEs=");
	ies = ((char *)ndis_assoc_info) +
		ndis_assoc_info->OffsetRequestIEs;
	for (i = 0; i < ndis_assoc_info->RequestIELength; i++)
		p += sprintf(p, "%02x", ies[i]);

	memset(&wrqu, 0, sizeof(wrqu));
    wrqu.data.length = p - wpa_assoc_info_req;
	wrqu.data.flags = RT_REQIE_EVENT_FLAG;
	DBGPRINT(RT_DEBUG_TRACE, "adding %d bytes\n", wrqu.data.length);
	wireless_send_event(pAd->net_dev, IWEVCUSTOM, &wrqu, wpa_assoc_info_req);

    wpa_assoc_info_resp = kmalloc(IW_CUSTOM_MAX, MEM_ALLOC_FLAG);
	if (!wpa_assoc_info_resp) {
		DBGPRINT(RT_DEBUG_TRACE, "couldn't allocate memory\n");
		//kfree(wpa_assoc_info_resp); //Thomas
		return;
	}

    //send RespIEs
	memset(wpa_assoc_info_resp, 0, IW_CUSTOM_MAX);
	p = wpa_assoc_info_resp;
	p += sprintf(p, " RespIEs=");
	ies = ((char *)ndis_assoc_info) +
		ndis_assoc_info->OffsetResponseIEs;
	for (i = 0; i < ndis_assoc_info->ResponseIELength; i++)
		p += sprintf(p, "%02x", ies[i]);
	p += sprintf(p, ")");

    memset(&wrqu, 0, sizeof(wrqu));
	wrqu.data.length = p - wpa_assoc_info_resp;
	wrqu.data.flags = RT_RESPIE_EVENT_FLAG;
	wireless_send_event(pAd->net_dev, IWEVCUSTOM, &wrqu, wpa_assoc_info_resp);

        memset(&wrqu, 0, sizeof(wrqu));
        wrqu.data.flags = RT_ASSOCINFO_EVENT_FLAG;
        wireless_send_event(pAd->net_dev, IWEVCUSTOM, &wrqu, NULL);

#if 0
	/* we need 28 extra bytes for the format strings */
	if ((ndis_assoc_info->RequestIELength +
	     ndis_assoc_info->ResponseIELength + 28) > IW_CUSTOM_MAX) {
		//WARNING("information element is too long! (%u,%u),"
		//	"association information dropped",
		//	ndis_assoc_info->RequestIELength,
		//	ndis_assoc_info->ResponseIELength);
		DBGPRINT(RT_DEBUG_TRACE, "information element is too long! "
			"association information dropped\n");
		kfree(assoc_info);
		return;
	}

	wpa_assoc_info = kmalloc(IW_CUSTOM_MAX, MEM_ALLOC_FLAG);
	if (!wpa_assoc_info) {
		DBGPRINT(RT_DEBUG_TRACE, "couldn't allocate memory\n");
		kfree(assoc_info);
		return;
	}
	p = wpa_assoc_info;
	p += sprintf(p, "ASSOCINFO(ReqIEs=");
	ies = ((char *)ndis_assoc_info) +
		ndis_assoc_info->OffsetRequestIEs;
	for (i = 0; i < ndis_assoc_info->RequestIELength; i++)
		p += sprintf(p, "%02x", ies[i]);


	p += sprintf(p, " RespIEs=");
	ies = ((char *)ndis_assoc_info) +
		ndis_assoc_info->OffsetResponseIEs;
	for (i = 0; i < ndis_assoc_info->ResponseIELength; i++)
		p += sprintf(p, "%02x", ies[i]);

	p += sprintf(p, ")");

	memset(&wrqu, 0, sizeof(wrqu));
	wrqu.data.length = p - wpa_assoc_info;
	//DBGPRINT(RT_DEBUG_TRACE, "adding %d bytes\n", wrqu.data.length);
	wireless_send_event(pAd->net_dev, IWEVCUSTOM, &wrqu, wpa_assoc_info);

	kfree(wpa_assoc_info);
#endif
	if(wpa_assoc_info_req != NULL){
		kfree(wpa_assoc_info_req);
	}
	if(wpa_assoc_info_resp != NULL){
		kfree(wpa_assoc_info_resp);
	}
	if(assoc_info != NULL){
		kfree(assoc_info);
	}


	return;
}
#endif

/*
    ==========================================================================
    Description:
        peer sends assoc rsp back
    Parameters:
        Elme - MLME message containing the received frame
    ==========================================================================
 */
VOID PeerAssocRspAction(
    IN PRTMP_ADAPTER pAd,
    IN MLME_QUEUE_ELEM *Elem)
{
    USHORT        CapabilityInfo, Status, Aid;
    UCHAR         SupRate[MAX_LEN_OF_SUPPORTED_RATES], SupRateLen;
    UCHAR         ExtRate[MAX_LEN_OF_SUPPORTED_RATES], ExtRateLen;
    UCHAR         Addr2[MAC_ADDR_LEN];
	EDCA_PARM     EdcaParm;

#if WPA_SUPPLICANT_SUPPORT
    union iwreq_data wrqu;
#endif

    if (PeerAssocRspSanity(pAd, Elem->Msg, Elem->MsgLen, Addr2, &CapabilityInfo, &Status, &Aid, SupRate, &SupRateLen, ExtRate, &ExtRateLen, &EdcaParm))
    {
        // The frame is for me ?
        if(MAC_ADDR_EQUAL(Addr2, pAd->MlmeAux.Bssid))
        {
            DBGPRINT(RT_DEBUG_TRACE, "ASSOC - receive ASSOC_RSP to me (status=%d)\n", Status);
            RTMPCancelTimer(&pAd->MlmeAux.AssocTimer);
            if(Status == MLME_SUCCESS)
            {
                //
				// There may some packets will be drop, if we haven't set the BSS type!
				// For example: EAPOL packet and the case of WHQL lost Packets.
				// Since this may some delays to set those variables at LinkUp(..) on this (Mlme) thread.
				//
				// This is a trick, set the BSS type here.
				//
				if (pAd->PortCfg.BssType == BSS_INFRA)
				{
					OPSTATUS_SET_FLAG(pAd, fOP_STATUS_MEDIA_STATE_CONNECTED);
				}


                // go to procedure listed on page 376
                AssocPostProc(pAd, Addr2, CapabilityInfo, Aid, SupRate, SupRateLen, ExtRate, ExtRateLen, &EdcaParm);

#if WPA_SUPPLICANT_SUPPORT
                if (pAd->PortCfg.WPA_Supplicant == TRUE) {
                    // collect associate info
                    link_status_handler(pAd);
                    //send associnfo event to wpa_supplicant
                    memset(&wrqu, 0, sizeof(wrqu));
                    wrqu.data.flags = RT_ASSOC_EVENT_FLAG;
                    wireless_send_event(pAd->net_dev, IWEVCUSTOM, &wrqu, NULL);
                }
#endif

            }

            pAd->Mlme.AssocMachine.CurrState = ASSOC_IDLE;
            MlmeEnqueue(pAd, MLME_CNTL_STATE_MACHINE, MT2_ASSOC_CONF, 2, &Status);
        }
    }
    else
    {
        DBGPRINT(RT_DEBUG_TRACE, "ASSOC - PeerAssocRspAction() sanity check fail\n");
    }
}

/*
    ==========================================================================
    Description:
        peer sends reassoc rsp
    Parametrs:
        Elem - MLME message cntaining the received frame
    ==========================================================================
 */
VOID PeerReassocRspAction(
    IN PRTMP_ADAPTER pAd,
    IN MLME_QUEUE_ELEM *Elem)
{
    USHORT      CapabilityInfo;
    USHORT      Status;
    USHORT      Aid;
    UCHAR       SupRate[MAX_LEN_OF_SUPPORTED_RATES], SupRateLen;
    UCHAR       ExtRate[MAX_LEN_OF_SUPPORTED_RATES], ExtRateLen;
    UCHAR       Addr2[MAC_ADDR_LEN];
    EDCA_PARM   EdcaParm;

#if WPA_SUPPLICANT_SUPPORT
    union iwreq_data wrqu;
#endif

    if(PeerAssocRspSanity(pAd, Elem->Msg, Elem->MsgLen, Addr2, &CapabilityInfo, &Status, &Aid, SupRate, &SupRateLen, ExtRate, &ExtRateLen, &EdcaParm))
    {
        if(MAC_ADDR_EQUAL(Addr2, pAd->MlmeAux.Bssid)) // The frame is for me ?
        {
            DBGPRINT(RT_DEBUG_TRACE, "ASSOC - receive REASSOC_RSP to me (status=%d)\n", Status);
            RTMPCancelTimer(&pAd->MlmeAux.ReassocTimer);

            if(Status == MLME_SUCCESS)
            {
                // go to procedure listed on page 376
                AssocPostProc(pAd, Addr2, CapabilityInfo, Aid, SupRate, SupRateLen, ExtRate, ExtRateLen, &EdcaParm);

#if WPA_SUPPLICANT_SUPPORT
                if (pAd->PortCfg.WPA_Supplicant == TRUE) {
                    //collect associate info
                    link_status_handler(pAd);
                    //send associnfo event to wpa_supplicant
                    memset(&wrqu, 0, sizeof(wrqu));
                    wrqu.data.flags = RT_ASSOC_EVENT_FLAG;
                    wireless_send_event(pAd->net_dev, IWEVCUSTOM, &wrqu, NULL);
                }
		        DBGPRINT(RT_DEBUG_OFF, "ASSOC - receive REASSOC_RSP to me (status=%d)\n", Status);
#endif

            }

            pAd->Mlme.AssocMachine.CurrState = ASSOC_IDLE;
            MlmeEnqueue(pAd, MLME_CNTL_STATE_MACHINE, MT2_REASSOC_CONF, 2, &Status);
        }
    }
    else
    {
        DBGPRINT(RT_DEBUG_TRACE, "ASSOC - PeerReassocRspAction() sanity check fail\n");
    }

}

/*
    ==========================================================================
    Description:
        procedures on IEEE 802.11/1999 p.376
    Parametrs:
    ==========================================================================
 */
VOID AssocPostProc(
    IN PRTMP_ADAPTER pAd,
    IN PUCHAR pAddr2,
    IN USHORT CapabilityInfo,
    IN USHORT Aid,
    IN UCHAR SupRate[],
    IN UCHAR SupRateLen,
    IN UCHAR ExtRate[],
    IN UCHAR ExtRateLen,
    IN PEDCA_PARM pEdcaParm)
{
	ULONG Idx;
	UCHAR VarIesOffset;

    pAd->MlmeAux.BssType = BSS_INFRA;
    COPY_MAC_ADDR(pAd->MlmeAux.Bssid, pAddr2);
    pAd->MlmeAux.Aid = Aid;
    pAd->MlmeAux.CapabilityInfo = CapabilityInfo & SUPPORTED_CAPABILITY_INFO;
    memcpy(&pAd->MlmeAux.APEdcaParm, pEdcaParm, sizeof(EDCA_PARM));

    // filter out un-supported rates
    pAd->MlmeAux.SupRateLen = SupRateLen;
    memcpy(pAd->MlmeAux.SupRate, SupRate, SupRateLen);
    RTMPCheckRates(pAd, pAd->MlmeAux.SupRate, &pAd->MlmeAux.SupRateLen);

    // filter out un-supported rates
    pAd->MlmeAux.ExtRateLen = ExtRateLen;
    memcpy(pAd->MlmeAux.ExtRate, ExtRate, ExtRateLen);
    RTMPCheckRates(pAd, pAd->MlmeAux.ExtRate, &pAd->MlmeAux.ExtRateLen);

	// Set New WPA information
	Idx = BssTableSearch(&pAd->ScanTab, pAddr2, pAd->MlmeAux.Channel);
	if (Idx == BSS_NOT_FOUND)
	{
		DBGPRINT_ERR("ASSOC - Can't find BSS after receiving Assoc response\n");
	}
	else
	{
		// Mod by James to fix OID_802_11_ASSOCIATION_INFORMATION
		pAd->PortCfg.AssocInfo.Length = sizeof(NDIS_802_11_ASSOCIATION_INFORMATION); //+ sizeof(NDIS_802_11_FIXED_IEs); 	// Filled in assoc request
		pAd->PortCfg.AssocInfo.AvailableResponseFixedIEs =
			NDIS_802_11_AI_RESFI_CAPABILITIES | NDIS_802_11_AI_RESFI_STATUSCODE | NDIS_802_11_AI_RESFI_ASSOCIATIONID;
		pAd->PortCfg.AssocInfo.ResponseFixedIEs.Capabilities  = CapabilityInfo;
		pAd->PortCfg.AssocInfo.ResponseFixedIEs.StatusCode    = MLME_SUCCESS;		// Should be success, add failed later
		pAd->PortCfg.AssocInfo.ResponseFixedIEs.AssociationId = Aid;

		// Copy BSS VarIEs to PortCfg associnfo structure.
		// First add Supported rates
		VarIesOffset = 0;
		memcpy(pAd->PortCfg.ResVarIEs + VarIesOffset, &SupRateIe, 1);
		VarIesOffset += 1;
		memcpy(pAd->PortCfg.ResVarIEs + VarIesOffset, &SupRateLen, 1);
		VarIesOffset += 1;
		memcpy(pAd->PortCfg.ResVarIEs + VarIesOffset, SupRate, SupRateLen);
		VarIesOffset += SupRateLen;
//		memcpy(pAd->PortCfg.ResVarIEs + VarIesOffset, &ExtRateIe, 1);
//		VarIesOffset += 1;
//		memcpy(pAd->PortCfg.ResVarIEs + VarIesOffset, &ExtRateLen, 1);
//		VarIesOffset += 1;
//		memcpy(pAd->PortCfg.ResVarIEs + VarIesOffset, ExtRate, ExtRateLen);
//		VarIesOffset += ExtRateLen;

		// Second add RSN
		memcpy(pAd->PortCfg.ResVarIEs + VarIesOffset, pAd->ScanTab.BssEntry[Idx].VarIEs, pAd->ScanTab.BssEntry[Idx].VarIELen);
		VarIesOffset += pAd->ScanTab.BssEntry[Idx].VarIELen;

		// Set Variable IEs Length
		pAd->PortCfg.ResVarIELen = VarIesOffset;
		pAd->PortCfg.AssocInfo.ResponseIELength = VarIesOffset;
	}
}

/*
    ==========================================================================
    Description:
        left part of IEEE 802.11/1999 p.374
    Parameters:
        Elem - MLME message containing the received frame
    ==========================================================================
 */
VOID PeerDisassocAction(
    IN PRTMP_ADAPTER pAd,
    IN MLME_QUEUE_ELEM *Elem)
{
    UCHAR         Addr2[MAC_ADDR_LEN];
    USHORT        Reason;

#if WPA_SUPPLICANT_SUPPORT
    union iwreq_data wrqu;
#endif

    if(PeerDisassocSanity(pAd, Elem->Msg, Elem->MsgLen, Addr2, &Reason))
    {
        if (INFRA_ON(pAd) && MAC_ADDR_EQUAL(pAd->PortCfg.Bssid, Addr2))
        {
            LinkDown(pAd, TRUE);
            pAd->Mlme.AssocMachine.CurrState = ASSOC_IDLE;

#if WPA_SUPPLICANT_SUPPORT
            if (pAd->PortCfg.WPA_Supplicant == TRUE) {
                // send disassoc event to wpa_supplicant
                memset(&wrqu, 0, sizeof(wrqu));
                wrqu.data.flags = RT_DISASSOC_EVENT_FLAG;
                wireless_send_event(pAd->net_dev, IWEVCUSTOM, &wrqu, NULL);
            }
#endif

#if 0
            // 2004-09-11 john: can't remember why AP will DISASSOCIATE us.
            //   But since it says for 2430 only, we temporaily remove the patch.
            // 2002/11/21 -
            //   patch RT2430/RT2420 hangup issue. We suspect this AP DIS-ASSOCIATE frame
            //   is caused by PHY hangup, so we reset PHY, then auto recover the connection.
            //   if this attempt fails, then remains in LinkDown and leaves the problem
            //   to MlmePeriodicExec()
            // NICPatchRT2430Bug(pAd);
            pAd->RalinkCounters.BeenDisassociatedCount ++;
			// Remove auto recover effort when disassociate by AP, re-enable for patch 2430 only
            DBGPRINT(RT_DEBUG_TRACE, "ASSOC - Disassociated by AP, Auto Recovery attempt #%d\n", pAd->RalinkCounters.BeenDisassociatedCount);
            MlmeAutoReconnectLastSSID(pAd);
#endif
        }
    }
    else
    {
        DBGPRINT(RT_DEBUG_TRACE, "ASSOC - PeerDisassocAction() sanity check fail\n");
    }

}

/*
    ==========================================================================
    Description:
        what the state machine will do after assoc timeout
    ==========================================================================
 */
VOID AssocTimeoutAction(
    IN PRTMP_ADAPTER pAd,
    IN MLME_QUEUE_ELEM *Elem)
{
    USHORT Status;
    DBGPRINT(RT_DEBUG_TRACE, "ASSOC - AssocTimeoutAction\n");
    pAd->Mlme.AssocMachine.CurrState = ASSOC_IDLE;
    Status = MLME_REJ_TIMEOUT;
    MlmeEnqueue(pAd, MLME_CNTL_STATE_MACHINE, MT2_ASSOC_CONF, 2, &Status);

}

/*
    ==========================================================================
    Description:
        what the state machine will do after reassoc timeout
    ==========================================================================
 */
VOID ReassocTimeoutAction(
    IN PRTMP_ADAPTER pAd,
    IN MLME_QUEUE_ELEM *Elem)
{
    USHORT Status;
    DBGPRINT(RT_DEBUG_TRACE, "ASSOC - ReassocTimeoutAction\n");
    pAd->Mlme.AssocMachine.CurrState = ASSOC_IDLE;
    Status = MLME_REJ_TIMEOUT;
    MlmeEnqueue(pAd, MLME_CNTL_STATE_MACHINE, MT2_REASSOC_CONF, 2, &Status);
}

/*
    ==========================================================================
    Description:
        what the state machine will do after disassoc timeout
    ==========================================================================
 */
VOID DisassocTimeoutAction(
    IN PRTMP_ADAPTER pAd,
    IN MLME_QUEUE_ELEM *Elem)
{
    USHORT Status;
    DBGPRINT(RT_DEBUG_TRACE, "ASSOC - DisassocTimeoutAction\n");
    pAd->Mlme.AssocMachine.CurrState = ASSOC_IDLE;
    Status = MLME_SUCCESS;
    MlmeEnqueue(pAd, MLME_CNTL_STATE_MACHINE, MT2_DISASSOC_CONF, 2, &Status);
}

VOID InvalidStateWhenAssoc(
    IN PRTMP_ADAPTER pAd,
    IN MLME_QUEUE_ELEM *Elem)
{
    USHORT Status;
    DBGPRINT(RT_DEBUG_TRACE, "ASSOC - InvalidStateWhenAssoc(state=%d), reset ASSOC state machine\n",
        pAd->Mlme.AssocMachine.CurrState);
    pAd->Mlme.AssocMachine.CurrState = ASSOC_IDLE;
    Status = MLME_STATE_MACHINE_REJECT;
    MlmeEnqueue(pAd, MLME_CNTL_STATE_MACHINE, MT2_ASSOC_CONF, 2, &Status);
}

VOID InvalidStateWhenReassoc(
    IN PRTMP_ADAPTER pAd,
    IN MLME_QUEUE_ELEM *Elem)
{
    USHORT Status;
    DBGPRINT(RT_DEBUG_TRACE, "ASSOC - InvalidStateWhenReassoc(state=%d), reset ASSOC state machine\n",
        pAd->Mlme.AssocMachine.CurrState);
    pAd->Mlme.AssocMachine.CurrState = ASSOC_IDLE;
    Status = MLME_STATE_MACHINE_REJECT;
    MlmeEnqueue(pAd, MLME_CNTL_STATE_MACHINE, MT2_REASSOC_CONF, 2, &Status);
}

VOID InvalidStateWhenDisassociate(
    IN PRTMP_ADAPTER pAd,
    IN MLME_QUEUE_ELEM *Elem)
{
    USHORT Status;
    DBGPRINT(RT_DEBUG_TRACE, "ASSOC - InvalidStateWhenDisassoc(state=%d), reset ASSOC state machine\n",
        pAd->Mlme.AssocMachine.CurrState);
    pAd->Mlme.AssocMachine.CurrState = ASSOC_IDLE;
    Status = MLME_STATE_MACHINE_REJECT;
    MlmeEnqueue(pAd, MLME_CNTL_STATE_MACHINE, MT2_DISASSOC_CONF, 2, &Status);
}

/*
    ==========================================================================
    Description:
        right part of IEEE 802.11/1999 page 374
    Note:
        This event should never cause ASSOC state machine perform state
        transition, and has no relationship with CNTL machine. So we separate
        this routine as a service outside of ASSOC state transition table.
    ==========================================================================
 */
VOID Cls3errAction(
    IN PRTMP_ADAPTER pAd,
    IN PUCHAR      pAddr)
{
    HEADER_802_11       DisassocHdr;
    PCHAR               pOutBuffer = NULL;
    ULONG               FrameLen = 0;
    USHORT              Reason = REASON_CLS3ERR;
    USHORT              NStatus;

    // allocate memory
    NStatus = MlmeAllocateMemory(pAd, (PVOID)&pOutBuffer);  //Get an unused nonpaged memory
    if (NStatus != NDIS_STATUS_SUCCESS)
        return;

    DBGPRINT(RT_DEBUG_TRACE, "ASSOC - Class 3 Error, Send DISASSOC frame\n");

    MgtMacHeaderInit(pAd, &DisassocHdr, SUBTYPE_DISASSOC, 0, pAddr, pAd->PortCfg.Bssid);
    MakeOutgoingFrame(pOutBuffer,               &FrameLen,
                      sizeof(HEADER_802_11),    &DisassocHdr,
                      2,                        &Reason,
                      END_OF_ARGS);

    MiniportMMRequest(pAd, pOutBuffer, FrameLen);


    pAd->PortCfg.DisassocReason = REASON_CLS3ERR;
    COPY_MAC_ADDR(pAd->PortCfg.DisassocSta, pAddr);
}

