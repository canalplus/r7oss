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
 *	Module Name:	sync.c
 *
 *	Abstract:
 *
 *	Revision History:
 *	Who		When		What
 *	--------	----------	-----------------------------
 *	John Chang	2004-09-01	modified for rt2561/2661
 *
 ***************************************************************************/

#include "rt_config.h"

// 2.4 Ghz channel plan index in the TxPower arrays.
#define	BG_BAND_REGION_0_START	0			// 1,2,3,4,5,6,7,8,9,10,11
#define	BG_BAND_REGION_0_SIZE	11
#define	BG_BAND_REGION_1_START	0			// 1,2,3,4,5,6,7,8,9,10,11,12,13
#define	BG_BAND_REGION_1_SIZE	13
#define	BG_BAND_REGION_2_START	9			// 10,11
#define	BG_BAND_REGION_2_SIZE	2
#define	BG_BAND_REGION_3_START	9			// 10,11,12,13
#define	BG_BAND_REGION_3_SIZE	4
#define	BG_BAND_REGION_4_START	13			// 14
#define	BG_BAND_REGION_4_SIZE	1
#define	BG_BAND_REGION_5_START	0			// 1,2,3,4,5,6,7,8,9,10,11,12,13,14
#define	BG_BAND_REGION_5_SIZE	14
#define	BG_BAND_REGION_6_START	2			// 3,4,5,6,7,8,9
#define	BG_BAND_REGION_6_SIZE	7
#define	BG_BAND_REGION_7_START	4			// 5,6,7,8,9,10,11,12,13
#define	BG_BAND_REGION_7_SIZE	9

// 5 Ghz channel plan index in the TxPower arrays.
UCHAR A_BAND_REGION_0_CHANNEL_LIST[]={36, 40, 44, 48, 52, 56, 60, 64, 149, 153, 157, 161, 165};
UCHAR A_BAND_REGION_1_CHANNEL_LIST[]={36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140};
UCHAR A_BAND_REGION_2_CHANNEL_LIST[]={36, 40, 44, 48, 52, 56, 60, 64};
UCHAR A_BAND_REGION_3_CHANNEL_LIST[]={52, 56, 60, 64, 149, 153, 157, 161};
UCHAR A_BAND_REGION_4_CHANNEL_LIST[]={149, 153, 157, 161, 165};
UCHAR A_BAND_REGION_5_CHANNEL_LIST[]={149, 153, 157, 161};
UCHAR A_BAND_REGION_6_CHANNEL_LIST[]={36, 40, 44, 48};
UCHAR A_BAND_REGION_7_CHANNEL_LIST[]={36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 149, 153, 157, 161, 165};
UCHAR A_BAND_REGION_8_CHANNEL_LIST[]={52, 56, 60, 64};
UCHAR A_BAND_REGION_9_CHANNEL_LIST[]={34, 38, 42, 46};
UCHAR A_BAND_REGION_10_CHANNEL_LIST[]={34, 36, 38, 40, 42, 44, 46, 48, 52, 56, 60, 64};

/*
    ==========================================================================
    Description:
        The sync state machine,
    Parameters:
        Sm - pointer to the state machine
    Note:
        the state machine looks like the following

    Column 1-2
                        SYNC_IDLE                       JOIN_WAIT_BEACON
    MT2_MLME_SCAN_REQ   mlme_scan_req_action            invalid_state_when_scan
    MT2_MLME_JOIN_REQ   mlme_join_req_action            invalid_state_when_join
    MT2_MLME_START_REQ  mlme_start_req_action           invalid_state_when_start
    MT2_PEER_BEACON     peer_beacon                     peer_beacon_at_join_wait_beacon_action
    MT2_PEER_PROBE_RSP  peer_beacon                     drop
    MT2_PEER_ATIM       drop                            drop
    MT2_SCAN_TIMEOUT    Drop                            Drop
    MT2_BEACON_TIMEOUT  Drop                            beacon_timeout_at_join_wait_beacon_action
    MT2_ATIM_TIMEOUT    Drop                            Drop
    MT2_PEER_PROBE_REQ  ????                            drop

    column 3
                         SCAN_LISTEN
    MT2_MLME_SCAN_REQ    invalid_state_when_scan
    MT2_MLME_JOIN_REQ    invalid_state_when_join
    MT2_MLME_START_REQ   invalid_state_when_start
    MT2_PEER_BEACON      peer_beacon_at_scan_action
    MT2_PEER_PROBE_RSP   peer_probe_rsp_at_scan_action
    MT2_PEER_ATIM        drop
    MT2_SCAN_TIMEOUT     scan_timeout_action
    MT2_BEACON_TIMEOUT   Drop
    MT2_ATIM_TIMEOUT     Drop
    MT2_PEER_PROBE_REQ   drop
    ==========================================================================
 */
VOID SyncStateMachineInit(
    IN PRTMP_ADAPTER pAd,
    IN STATE_MACHINE *Sm,
    OUT STATE_MACHINE_FUNC Trans[])
{
    StateMachineInit(Sm, (STATE_MACHINE_FUNC*)Trans, MAX_SYNC_STATE, MAX_SYNC_MSG, (STATE_MACHINE_FUNC)Drop, SYNC_IDLE, SYNC_MACHINE_BASE);

    // column 1
    StateMachineSetAction(Sm, SYNC_IDLE, MT2_MLME_SCAN_REQ, (STATE_MACHINE_FUNC)MlmeScanReqAction);
    StateMachineSetAction(Sm, SYNC_IDLE, MT2_MLME_JOIN_REQ, (STATE_MACHINE_FUNC)MlmeJoinReqAction);
    StateMachineSetAction(Sm, SYNC_IDLE, MT2_MLME_START_REQ, (STATE_MACHINE_FUNC)MlmeStartReqAction);
    StateMachineSetAction(Sm, SYNC_IDLE, MT2_PEER_BEACON, (STATE_MACHINE_FUNC)PeerBeacon);
    StateMachineSetAction(Sm, SYNC_IDLE, MT2_PEER_PROBE_REQ, (STATE_MACHINE_FUNC)PeerProbeReqAction);

    //column 2
    StateMachineSetAction(Sm, JOIN_WAIT_BEACON, MT2_MLME_SCAN_REQ, (STATE_MACHINE_FUNC)InvalidStateWhenScan);
    StateMachineSetAction(Sm, JOIN_WAIT_BEACON, MT2_MLME_JOIN_REQ, (STATE_MACHINE_FUNC)InvalidStateWhenJoin);
    StateMachineSetAction(Sm, JOIN_WAIT_BEACON, MT2_MLME_START_REQ, (STATE_MACHINE_FUNC)InvalidStateWhenStart);
    StateMachineSetAction(Sm, JOIN_WAIT_BEACON, MT2_PEER_BEACON, (STATE_MACHINE_FUNC)PeerBeaconAtJoinAction);
    StateMachineSetAction(Sm, JOIN_WAIT_BEACON, MT2_BEACON_TIMEOUT, (STATE_MACHINE_FUNC)BeaconTimeoutAtJoinAction);

    // column 3
    StateMachineSetAction(Sm, SCAN_LISTEN, MT2_MLME_SCAN_REQ, (STATE_MACHINE_FUNC)InvalidStateWhenScan);
    StateMachineSetAction(Sm, SCAN_LISTEN, MT2_MLME_JOIN_REQ, (STATE_MACHINE_FUNC)InvalidStateWhenJoin);
    StateMachineSetAction(Sm, SCAN_LISTEN, MT2_MLME_START_REQ, (STATE_MACHINE_FUNC)InvalidStateWhenStart);
    StateMachineSetAction(Sm, SCAN_LISTEN, MT2_PEER_BEACON, (STATE_MACHINE_FUNC)PeerBeaconAtScanAction);
    StateMachineSetAction(Sm, SCAN_LISTEN, MT2_PEER_PROBE_RSP, (STATE_MACHINE_FUNC)PeerBeaconAtScanAction);
    StateMachineSetAction(Sm, SCAN_LISTEN, MT2_SCAN_TIMEOUT, (STATE_MACHINE_FUNC)ScanTimeoutAction);

    // timer init
    RTMPInitTimer(pAd, &pAd->MlmeAux.BeaconTimer, &BeaconTimeout);
    RTMPInitTimer(pAd, &pAd->MlmeAux.ScanTimer,   &ScanTimeout);

}

/*
    ==========================================================================
    Description:
        Becaon timeout handler, executed in timer thread
    ==========================================================================
 */
VOID BeaconTimeout(
    IN  unsigned long data)
{
    RTMP_ADAPTER *pAd = (RTMP_ADAPTER *)data;

    DBGPRINT(RT_DEBUG_TRACE,"SYNC - BeaconTimeout\n");
    MlmeEnqueue(pAd, SYNC_STATE_MACHINE, MT2_BEACON_TIMEOUT, 0, NULL);
    RTUSBMlmeUp(pAd);
}

/*
    ==========================================================================
    Description:
        Scan timeout handler, executed in timer thread
    ==========================================================================
 */
VOID ScanTimeout(
    IN  unsigned long data)
{
    RTMP_ADAPTER *pAd = (RTMP_ADAPTER *)data;

    DBGPRINT(RT_DEBUG_INFO,"SYNC - Scan Timeout \n");
    MlmeEnqueue(pAd, SYNC_STATE_MACHINE, MT2_SCAN_TIMEOUT, 0, NULL);
    RTUSBMlmeUp(pAd);
}

/*
    ==========================================================================
    Description:
        MLME SCAN req state machine procedure
    ==========================================================================
 */
VOID MlmeScanReqAction(
	IN PRTMP_ADAPTER pAd,
	IN MLME_QUEUE_ELEM *Elem)
{
	UCHAR           Ssid[MAX_LEN_OF_SSID], SsidLen, ScanType, BssType;
	ULONG           Now;
    USHORT          Status;
    PHEADER_802_11  pHdr80211;
    PUCHAR          pOutBuffer = NULL;
    USHORT          NStatus;

    // Increase the scan retry counters.
	pAd->PortCfg.ScanCnt++;

	// Suspend MSDU transmission here
	RTUSBSuspendMsduTransmission(pAd);

	// first check the parameter sanity
	if (MlmeScanReqSanity(pAd,
                          Elem->Msg,
                          Elem->MsgLen,
                          &BssType,
                          Ssid,
                          &SsidLen,
                          &ScanType))
	{
		DBGPRINT(RT_DEBUG_TRACE, "SYNC - MlmeScanReqAction\n");

        //
		// To prevent data lost.
		// Send an NULL data with turned PSM bit on to current associated AP before SCAN progress.
		// And should send an NULL data with turned PSM bit off to AP, when scan progress done
		//
		if (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_MEDIA_STATE_CONNECTED) && (INFRA_ON(pAd)))
		{
            NStatus = MlmeAllocateMemory(pAd, (PVOID)&pOutBuffer);  //Get an unused nonpaged memory
            if (NStatus == NDIS_STATUS_SUCCESS)
            {
				pHdr80211 = (PHEADER_802_11) pOutBuffer;
				MgtMacHeaderInit(pAd, pHdr80211, SUBTYPE_NULL_FUNC, 1, pAd->PortCfg.Bssid, pAd->PortCfg.Bssid);
				pHdr80211->Duration = 0;
				pHdr80211->FC.Type = BTYPE_DATA;
				pHdr80211->FC.PwrMgmt = PWR_SAVE;

				// Send using priority queue
				MiniportMMRequest(pAd, pOutBuffer, sizeof(HEADER_802_11));
				DBGPRINT(RT_DEBUG_TRACE, "MlmeScanReqAction -- Send PSM Data frame for off channel RM\n");

				RTMPusecDelay(5000);
			}
		}

		Now = jiffies;
		pAd->PortCfg.LastScanTime = Now;

		RTMPCancelTimer(&pAd->MlmeAux.BeaconTimer);
        RTMPCancelTimer(&pAd->MlmeAux.ScanTimer);

		// record desired BSS parameters
		pAd->MlmeAux.BssType = BssType;
		pAd->MlmeAux.ScanType = ScanType;
		pAd->MlmeAux.SsidLen = SsidLen;
		memcpy(pAd->MlmeAux.Ssid, Ssid, SsidLen);

		// start from the first channel
		pAd->MlmeAux.Channel = FirstChannel(pAd);
		ScanNextChannel(pAd);
	}
	else
	{
		DBGPRINT(RT_DEBUG_ERROR, "SYNC - MlmeScanReqAction() sanity check fail. BUG!!!\n");
		pAd->Mlme.SyncMachine.CurrState = SYNC_IDLE;
		Status = MLME_INVALID_FORMAT;
        MlmeEnqueue(pAd, MLME_CNTL_STATE_MACHINE, MT2_SCAN_CONF, 2, &Status);
	}
}

/*
    ==========================================================================
    Description:
        MLME JOIN req state machine procedure
    ==========================================================================
 */
VOID MlmeJoinReqAction(
    IN PRTMP_ADAPTER pAd,
    IN MLME_QUEUE_ELEM *Elem)
{
    BSS_ENTRY    *pBss;
    MLME_JOIN_REQ_STRUCT *Info = (MLME_JOIN_REQ_STRUCT *)(Elem->Msg);

    DBGPRINT(RT_DEBUG_TRACE, "SYNC - MlmeJoinReqAction(BSS #%d)\n", Info->BssIdx);

    // reset all the timers
    RTMPCancelTimer(&pAd->MlmeAux.ScanTimer);
    RTMPCancelTimer(&pAd->MlmeAux.BeaconTimer);

    pBss = &pAd->MlmeAux.SsidBssTab.BssEntry[Info->BssIdx];

    // record the desired SSID & BSSID we're waiting for
    COPY_MAC_ADDR(pAd->MlmeAux.Bssid, pBss->Bssid);
    memcpy(pAd->MlmeAux.Ssid, pBss->Ssid, pBss->SsidLen);
    pAd->MlmeAux.SsidLen = pBss->SsidLen;
    pAd->MlmeAux.BssType = pBss->BssType;
    pAd->MlmeAux.Channel = pBss->Channel;

    // switch channel and waiting for beacon timer
    AsicSwitchChannel(pAd, pAd->MlmeAux.Channel);
    AsicLockChannel(pAd, pAd->MlmeAux.Channel);

	RTMPSetTimer(pAd, &pAd->MlmeAux.BeaconTimer, JOIN_TIMEOUT);

    DBGPRINT(RT_DEBUG_TRACE, "SYNC - Switch to channel %d, SSID %s \n", pAd->MlmeAux.Channel, pAd->MlmeAux.Ssid);
    DBGPRINT(RT_DEBUG_TRACE, "SYNC - Wait BEACON from %02x:%02x:%02x:%02x:%02x:%02x ...\n",
        pAd->MlmeAux.Bssid[0], pAd->MlmeAux.Bssid[1],
        pAd->MlmeAux.Bssid[2], pAd->MlmeAux.Bssid[3],
        pAd->MlmeAux.Bssid[4], pAd->MlmeAux.Bssid[5]);


    pAd->Mlme.SyncMachine.CurrState = JOIN_WAIT_BEACON;
}

/*
    ==========================================================================
    Description:
        MLME START Request state machine procedure, starting an IBSS
    ==========================================================================
 */
VOID MlmeStartReqAction(
    IN PRTMP_ADAPTER pAd,
    IN MLME_QUEUE_ELEM *Elem)
{
    UCHAR         Ssid[MAX_LEN_OF_SSID], SsidLen;
    USHORT        Status;

	// New for WPA security suites
	UCHAR						VarIE[MAX_VIE_LEN]; 	// Total VIE length = MAX_VIE_LEN - -5
	NDIS_802_11_VARIABLE_IEs	*pVIE = NULL;
	LARGE_INTEGER				TimeStamp;
    BOOLEAN Privacy;
#ifdef	SINGLE_ADHOC_LINKUP
	ULONG	Bssidx;
	CF_PARM CfParm;
    CfParm.bValid = FALSE;
#endif

	// Init Variable IE structure
	pVIE = (PNDIS_802_11_VARIABLE_IEs) VarIE;
	pVIE->Length = 0;
	TimeStamp.vv.LowPart  = 0;
	TimeStamp.vv.HighPart = 0;

    if (MlmeStartReqSanity(pAd, Elem->Msg, Elem->MsgLen, Ssid, &SsidLen))
    {
        // reset all the timers
        RTMPCancelTimer(&pAd->MlmeAux.ScanTimer);
	    RTMPCancelTimer(&pAd->MlmeAux.BeaconTimer);

        //
        // Start a new IBSS. All IBSS parameters are decided now....
        //
        pAd->MlmeAux.BssType           = BSS_ADHOC;
        memcpy(pAd->MlmeAux.Ssid, Ssid, SsidLen);
        pAd->MlmeAux.SsidLen           = SsidLen;

        // generate a random number as BSSID
		if (!OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_BSSID_SET)) {
		MacAddrRandomBssid(pAd, pAd->MlmeAux.Bssid);
		}
        Privacy = (pAd->PortCfg.WepStatus == Ndis802_11Encryption1Enabled) ||
                  (pAd->PortCfg.WepStatus == Ndis802_11Encryption2Enabled) ||
                  (pAd->PortCfg.WepStatus == Ndis802_11Encryption3Enabled);

        pAd->MlmeAux.CapabilityInfo    = CAP_GENERATE(0,1,Privacy, (pAd->PortCfg.TxPreamble == Rt802_11PreambleShort), 1);
        pAd->MlmeAux.BeaconPeriod      = pAd->PortCfg.BeaconPeriod;
        pAd->MlmeAux.AtimWin           = pAd->PortCfg.AtimWin;
        pAd->MlmeAux.Channel           = pAd->PortCfg.Channel;

        pAd->MlmeAux.SupRateLen= pAd->PortCfg.SupRateLen;
        memcpy(pAd->MlmeAux.SupRate, pAd->PortCfg.SupRate, MAX_LEN_OF_SUPPORTED_RATES);
        RTMPCheckRates(pAd, pAd->MlmeAux.SupRate, &pAd->MlmeAux.SupRateLen);
        pAd->MlmeAux.ExtRateLen = pAd->PortCfg.ExtRateLen;
        memcpy(pAd->MlmeAux.ExtRate, pAd->PortCfg.ExtRate, MAX_LEN_OF_SUPPORTED_RATES);
        RTMPCheckRates(pAd, pAd->MlmeAux.ExtRate, &pAd->MlmeAux.ExtRateLen);

        // temporarily not support QOS in IBSS
        memset(&pAd->MlmeAux.APEdcaParm, 0, sizeof(EDCA_PARM));
        memset(&pAd->MlmeAux.APQbssLoad, 0, sizeof(QBSS_LOAD_PARM));
        memset(&pAd->MlmeAux.APQosCapability, 0, sizeof(QOS_CAPABILITY_PARM));

        AsicSwitchChannel(pAd, pAd->MlmeAux.Channel);
        AsicLockChannel(pAd, pAd->MlmeAux.Channel);

        DBGPRINT(RT_DEBUG_TRACE, "SYNC - MlmeStartReqAction(ch= %d,sup rates= %d, ext rates=%d)\n",
            pAd->MlmeAux.Channel, pAd->MlmeAux.SupRateLen, pAd->MlmeAux.ExtRateLen);

#ifdef	SINGLE_ADHOC_LINKUP
		// Add itself as the entry within BSS table
		Bssidx = BssTableSearch(&pAd->ScanTab, pAd->MlmeAux.Bssid, pAd->MlmeAux.Channel);
		if (Bssidx == BSS_NOT_FOUND)
		{
			Bssidx = BssTableSetEntry(pAd, &pAd->ScanTab, pAd->MlmeAux.Bssid,
				Ssid, SsidLen, pAd->MlmeAux.BssType, pAd->MlmeAux.BeaconPeriod,
				&CfParm, pAd->MlmeAux.AtimWin, pAd->MlmeAux.CapabilityInfo,
				pAd->MlmeAux.SupRate, pAd->MlmeAux.SupRateLen, pAd->MlmeAux.ExtRate, pAd->MlmeAux.ExtRateLen,
				pAd->MlmeAux.Channel, pAd->BbpRssiToDbmDelta - 30, TimeStamp, 0, NULL, NULL, NULL, 0, pVIE);
		}
#endif

        pAd->Mlme.SyncMachine.CurrState = SYNC_IDLE;
        Status = MLME_SUCCESS;
        MlmeEnqueue(pAd, MLME_CNTL_STATE_MACHINE, MT2_START_CONF, 2, &Status);
    }
    else
    {
        DBGPRINT_ERR("SYNC - MlmeStartReqAction() sanity check fail.\n");
        pAd->Mlme.SyncMachine.CurrState = SYNC_IDLE;
        Status = MLME_INVALID_FORMAT;
        MlmeEnqueue(pAd, MLME_CNTL_STATE_MACHINE, MT2_START_CONF, 2, &Status);
    }
}

/*
    ==========================================================================
    Description:
        peer sends beacon back when scanning
    ==========================================================================
 */
VOID PeerBeaconAtScanAction(
    IN PRTMP_ADAPTER pAd,
    IN MLME_QUEUE_ELEM *Elem)
{
    UCHAR           Bssid[MAC_ADDR_LEN], Addr2[MAC_ADDR_LEN];
    UCHAR           Ssid[MAX_LEN_OF_SSID], BssType, Channel, NewChannel,
                    SsidLen, DtimCount, DtimPeriod, BcastFlag, MessageToMe;
    CF_PARM         CfParm;
    USHORT          BeaconPeriod, AtimWin, CapabilityInfo;
    PFRAME_802_11   pFrame;
    LARGE_INTEGER   TimeStamp;
    UCHAR           Erp;
	UCHAR         	SupRate[MAX_LEN_OF_SUPPORTED_RATES], ExtRate[MAX_LEN_OF_SUPPORTED_RATES];
	UCHAR		  	SupRateLen, ExtRateLen;
	UCHAR			LenVIE;
	UCHAR			CkipFlag;
	UCHAR			AironetCellPowerLimit;
	EDCA_PARM       EdcaParm;
	QBSS_LOAD_PARM  QbssLoad;
	QOS_CAPABILITY_PARM QosCapability;
    ULONG           RalinkIe;
	UCHAR						VarIE[MAX_VIE_LEN];		// Total VIE length = MAX_VIE_LEN - -5
	NDIS_802_11_VARIABLE_IEs	*pVIE = NULL;

	DBGPRINT(RT_DEBUG_TRACE, "--> %s\n", __FUNCTION__);

    // NdisFillMemory(Ssid, MAX_LEN_OF_SSID, 0x00);
    pFrame = (PFRAME_802_11) Elem->Msg;
	// Init Variable IE structure
	pVIE = (PNDIS_802_11_VARIABLE_IEs) VarIE;
	pVIE->Length = 0;
    if (PeerBeaconAndProbeRspSanity(pAd,
                                Elem->Msg,
                                Elem->MsgLen,
                                Addr2,
                                Bssid,
                                Ssid,
                                &SsidLen,
                                &BssType,
                                &BeaconPeriod,
                                &Channel,
                                &NewChannel,
                                &TimeStamp,
                                &CfParm,
                                &AtimWin,
                                &CapabilityInfo,
                                &Erp,
                                &DtimCount,
                                &DtimPeriod,
                                &BcastFlag,
                                &MessageToMe,
                                SupRate,
                                &SupRateLen,
                                ExtRate,
                                &ExtRateLen,
                                &CkipFlag,
                                &AironetCellPowerLimit,
                                &EdcaParm,
                                &QbssLoad,
                                &QosCapability,
                                &RalinkIe,
                                &LenVIE,
                                pVIE))
    {
        ULONG Idx;
        UCHAR Rssi = 0;
        CHAR  RealRssi;

        // This correct im-proper RSSI indication during SITE SURVEY issue.
        // Always report bigger RSSI during SCANNING when receiving multiple BEACONs from the same AP.
        // This case happens because BEACONs come from adjacent channels, so RSSI become weaker as we
        // switch to more far away channels.
		if (Elem->Channel != Channel)
			return;

        Idx = BssTableSearch(&pAd->ScanTab, Bssid, Channel);
        if (Idx != BSS_NOT_FOUND)
            Rssi = pAd->ScanTab.BssEntry[Idx].Rssi;


	    RealRssi = ConvertToRssi(pAd, Elem->Rssi, RSSI_NO_1);
	    if ((RealRssi + pAd->BbpRssiToDbmDelta) > Rssi)
	        Rssi = RealRssi + pAd->BbpRssiToDbmDelta;

//        DBGPRINT(RT_DEBUG_TRACE, "SYNC - PeerBeaconAtScanAction (SubType=%d, SsidLen=%d, Ssid=%s)\n", pFrame->Hdr.FC.SubType, SsidLen,Ssid);

        // Back Door Mechanism: Get AP Cfg Data
        if (pAd->PortCfg.bGetAPConfig)
	{
		CHAR CfgData[MAX_CFG_BUFFER_LEN+1] = {0};
		if (BackDoorProbeRspSanity(pAd, Elem->Msg, Elem->MsgLen, CfgData))
		{
			DBGPRINT(RT_DEBUG_ERROR, "- %s: CfgData(len:%d): %s\n",
					__FUNCTION__, (int)strlen(CfgData), CfgData);
			pAd->PortCfg.bGetAPConfig = FALSE;
		}
	}
        Idx = BssTableSetEntry(pAd, &pAd->ScanTab, Bssid, Ssid, SsidLen, BssType,
                    BeaconPeriod, &CfParm, AtimWin, CapabilityInfo, SupRate,
                    SupRateLen, ExtRate, ExtRateLen, Channel, Rssi, TimeStamp, CkipFlag,
                    &EdcaParm, &QosCapability, &QbssLoad, LenVIE, pVIE);
    }
    // sanity check fail, ignored
}

/*
    ==========================================================================
    Description:
        When waiting joining the (I)BSS, beacon received from external
    ==========================================================================
 */
VOID PeerBeaconAtJoinAction(
    IN PRTMP_ADAPTER pAd,
    IN MLME_QUEUE_ELEM *Elem)
{
    UCHAR         Bssid[MAC_ADDR_LEN], Addr2[MAC_ADDR_LEN];
    UCHAR         Ssid[MAX_LEN_OF_SSID], SsidLen, BssType, Channel, MessageToMe,
                  DtimCount, DtimPeriod, BcastFlag, NewChannel;
    LARGE_INTEGER TimeStamp;
    USHORT        BeaconPeriod, AtimWin, CapabilityInfo;
//    UINT          FrameLen = 0;
    CF_PARM       Cf;
    UCHAR         Erp;
	UCHAR         SupRate[MAX_LEN_OF_SUPPORTED_RATES], ExtRate[MAX_LEN_OF_SUPPORTED_RATES];
	UCHAR		  SupRateLen, ExtRateLen;
	UCHAR         CkipFlag;
	UCHAR		  LenVIE;
	UCHAR		  AironetCellPowerLimit;
	EDCA_PARM       EdcaParm;
	QBSS_LOAD_PARM  QbssLoad;
	QOS_CAPABILITY_PARM QosCapability;
    USHORT        Status;
	CHAR	        RealRssi = -85; //assume -85 dB
	UCHAR						VarIE[MAX_VIE_LEN];		// Total VIE length = MAX_VIE_LEN - -5
	NDIS_802_11_VARIABLE_IEs	*pVIE = NULL;
    ULONG           RalinkIe;
    ULONG           Idx;
	UCHAR   		PeerTxType;

	DBGPRINT(RT_DEBUG_TRACE, "--> %s\n", __FUNCTION__);

	// Init Variable IE structure
	pVIE = (PNDIS_802_11_VARIABLE_IEs) VarIE;
	pVIE->Length = 0;
    if (PeerBeaconAndProbeRspSanity(pAd,
                                Elem->Msg,
                                Elem->MsgLen,
                                Addr2,
                                Bssid,
                                Ssid,
                                &SsidLen,
                                &BssType,
                                &BeaconPeriod,
                                &Channel,
                                &NewChannel,
                                &TimeStamp,
                                &Cf,
                                &AtimWin,
                                &CapabilityInfo,
                                &Erp,
                                &DtimCount,
                                &DtimPeriod,
                                &BcastFlag,
                                &MessageToMe,
                                SupRate,
                                &SupRateLen,
                                ExtRate,
                                &ExtRateLen,
                                &CkipFlag,
                                &AironetCellPowerLimit,
                                &EdcaParm,
                                &QbssLoad,
                                &QosCapability,
                                &RalinkIe,
                                &LenVIE,
                                pVIE))
    {
		// Disqualify 11b only adhoc when we are in 11g only adhoc mode
		if (BssType == BSS_ADHOC)
		{
			PeerTxType = PeerTxTypeInUseSanity(Channel, SupRate, SupRateLen, ExtRate, ExtRateLen);
			if ((pAd->PortCfg.AdhocMode == ADHOC_11G) && (PeerTxType == CCK_RATE))
			{
				return;
			}
			else if ((pAd->PortCfg.AdhocMode == ADHOC_11B) && (PeerTxType == OFDM_RATE))
			{
			    return;
			}
		}

		// BEACON from desired BSS/IBSS found. We should be able to decide most
		// BSS parameters here.
		// Q. But what happen if this JOIN doesn't conclude a successful ASSOCIATEION?
		//    Do we need to receover back all parameters belonging to previous BSS?
		// A. Should be not. There's no back-door recover to previous AP. It still need
		//    a new JOIN-AUTH-ASSOC sequence.
		if (MAC_ADDR_EQUAL(pAd->MlmeAux.Bssid, Bssid))
        {
            DBGPRINT(RT_DEBUG_TRACE, "SYNC (%s) - receive desired BEACON Chan=%d\n", __FUNCTION__, Channel);

		    RTMPCancelTimer(&pAd->MlmeAux.BeaconTimer);


	        RealRssi = ConvertToRssi(pAd, Elem->Rssi, RSSI_NO_1);
	        pAd->PortCfg.LastRssi = RealRssi + pAd->BbpRssiToDbmDelta;
	        pAd->PortCfg.AvgRssi  = pAd->PortCfg.LastRssi;
	        pAd->PortCfg.AvgRssiX8 = pAd->PortCfg.AvgRssi << 3;


			//
			// We need to check if SSID only set to any, then we can record the current SSID.
			// Otherwise will cause hidden SSID association failed.
			//
			if (pAd->MlmeAux.SsidLen == 0)
			{
	           	memcpy(pAd->MlmeAux.Ssid, Ssid, SsidLen);
	           	pAd->MlmeAux.SsidLen = SsidLen;
			}
			else
			{
				Idx = BssSsidTableSearch(&pAd->ScanTab, Bssid, pAd->MlmeAux.Ssid, pAd->MlmeAux.SsidLen, Channel);

				if (Idx != BSS_NOT_FOUND)
				{
					//
					// Multiple SSID case, used correct CapabilityInfo
					//
					CapabilityInfo = pAd->ScanTab.BssEntry[Idx].CapabilityInfo;
				}
			}

           	pAd->MlmeAux.CapabilityInfo = CapabilityInfo & SUPPORTED_CAPABILITY_INFO;
            pAd->MlmeAux.BssType = BssType;
            pAd->MlmeAux.BeaconPeriod = BeaconPeriod;
            pAd->MlmeAux.Channel = Channel;
            pAd->MlmeAux.AtimWin = AtimWin;
            pAd->MlmeAux.CfpPeriod = Cf.CfpPeriod;
            pAd->MlmeAux.CfpMaxDuration = Cf.CfpMaxDuration;
            pAd->MlmeAux.APRalinkIe = RalinkIe;

            // Copy AP's supported rate to MlmeAux for creating assoication request
            // Also filter out not supported rate
            pAd->MlmeAux.SupRateLen = SupRateLen;
            memcpy(pAd->MlmeAux.SupRate, SupRate, SupRateLen);
			RTMPCheckRates(pAd, pAd->MlmeAux.SupRate, &pAd->MlmeAux.SupRateLen);
            pAd->MlmeAux.ExtRateLen = ExtRateLen;
            memcpy(pAd->MlmeAux.ExtRate, ExtRate, ExtRateLen);
			RTMPCheckRates(pAd, pAd->MlmeAux.ExtRate, &pAd->MlmeAux.ExtRateLen);

			//
			// Update MlmeRate & RtsRate.
			// We need to update those rates, for example on Roaming A to B,
			// MlmeRate will be RATE_6(OFDM) on 11A, but when roam to B.
			// RATE_6 can't be recognized by 11B AP and vice versa.
			//
			PeerTxType = PeerTxTypeInUseSanity(Channel, SupRate, SupRateLen, ExtRate, ExtRateLen);
			switch (PeerTxType)
			{
				case CCK_RATE: //CCK
				case CCKOFDM_RATE: //CCK + OFDM
					pAd->PortCfg.MlmeRate = RATE_2;
					pAd->PortCfg.RtsRate = RATE_2;
					break;
				case OFDM_RATE: //OFDM
					pAd->PortCfg.MlmeRate = RATE_6;
					pAd->PortCfg.RtsRate = RATE_6;
					break;
				default:
					pAd->PortCfg.MlmeRate = RATE_2;
					pAd->PortCfg.RtsRate = RATE_2;
					break;
			}

            // copy QOS related information
            if (pAd->PortCfg.bWmmCapable)
            {
                memcpy(&pAd->MlmeAux.APEdcaParm, &EdcaParm, sizeof(EDCA_PARM));
                memcpy(&pAd->MlmeAux.APQbssLoad, &QbssLoad, sizeof(QBSS_LOAD_PARM));
                memcpy(&pAd->MlmeAux.APQosCapability, &QosCapability, sizeof(QOS_CAPABILITY_PARM));
            }
            else
            {
                memset(&pAd->MlmeAux.APEdcaParm, 0, sizeof(EDCA_PARM));
                memset(&pAd->MlmeAux.APQbssLoad, 0, sizeof(QBSS_LOAD_PARM));
                memset(&pAd->MlmeAux.APQosCapability, 0, sizeof(QOS_CAPABILITY_PARM));
            }

            DBGPRINT(RT_DEBUG_TRACE, "SYNC - after JOIN, SupRateLen=%d, ExtRateLen=%d\n",
                pAd->MlmeAux.SupRateLen, pAd->MlmeAux.ExtRateLen);

            //Used the default TX Power Percentage.
		    pAd->PortCfg.TxPowerPercentage = pAd->PortCfg.TxPowerDefault;

            pAd->Mlme.SyncMachine.CurrState = SYNC_IDLE;
            Status = MLME_SUCCESS;
            MlmeEnqueue(pAd, MLME_CNTL_STATE_MACHINE, MT2_JOIN_CONF, 2, &Status);
        }
        // not to me BEACON, ignored
    }
    // sanity check fail, ignore this frame
}

/*
	==========================================================================
	Description:
		receive BEACON from peer

	==========================================================================
 */
VOID PeerBeacon(
	IN PRTMP_ADAPTER pAd,
	IN MLME_QUEUE_ELEM *Elem)
{
	UCHAR		  Bssid[MAC_ADDR_LEN], Addr2[MAC_ADDR_LEN];
	CHAR		  Ssid[MAX_LEN_OF_SSID];
	CF_PARM 	  CfParm;
	UCHAR		  SsidLen, MessageToMe=0, BssType, Channel, NewChannel, index=0;
	UCHAR		  DtimCount=0, DtimPeriod=0, BcastFlag=0;
	USHORT		  CapabilityInfo, AtimWin, BeaconPeriod;
	LARGE_INTEGER TimeStamp;
	USHORT		  TbttNumToNextWakeUp;
	UCHAR		  Erp;
	UCHAR		  SupRate[MAX_LEN_OF_SUPPORTED_RATES], ExtRate[MAX_LEN_OF_SUPPORTED_RATES];
	UCHAR		  SupRateLen, ExtRateLen;
	UCHAR		  CkipFlag;
	UCHAR		  LenVIE;
	UCHAR		  AironetCellPowerLimit;
	EDCA_PARM		EdcaParm;
	QBSS_LOAD_PARM	QbssLoad;
	QOS_CAPABILITY_PARM QosCapability;
	ULONG			RalinkIe;
	// New for WPA security suites
	UCHAR						VarIE[MAX_VIE_LEN];		// Total VIE length = MAX_VIE_LEN - -5
	NDIS_802_11_VARIABLE_IEs	*pVIE = NULL;

	DBGPRINT(RT_DEBUG_TRACE, "--> %s\n", __FUNCTION__);

	if (!INFRA_ON(pAd) && !ADHOC_ON(pAd)) {
		DBGPRINT(RT_DEBUG_ERROR, "<-- %s: Mode not specified\n", __FUNCTION__);
		return;
	}
	// Init Variable IE structure
	pVIE = (PNDIS_802_11_VARIABLE_IEs) VarIE;
	pVIE->Length = 0;
    if (PeerBeaconAndProbeRspSanity(pAd,
                                Elem->Msg,
                                Elem->MsgLen,
                                Addr2,
                                Bssid,
                                Ssid,
                                &SsidLen,
                                &BssType,
                                &BeaconPeriod,
                                &Channel,
                                &NewChannel,
                                &TimeStamp,
                                &CfParm,
                                &AtimWin,
                                &CapabilityInfo,
                                &Erp,
                                &DtimCount,
                                &DtimPeriod,
                                &BcastFlag,
                                &MessageToMe,
                                SupRate,
                                &SupRateLen,
                                ExtRate,
                                &ExtRateLen,
                                &CkipFlag,
                                &AironetCellPowerLimit,
                                &EdcaParm,
                                &QbssLoad,
                                &QosCapability,
                                &RalinkIe,
                                &LenVIE,
                                pVIE))
	{
		BOOLEAN is_my_bssid, is_my_ssid;
		ULONG	Bssidx;
		unsigned long Now;
		BSS_ENTRY *pBss;
		CHAR	RealRssi  = -85; //assume -85 dB
		UCHAR	PeerTxType;

		// Disqualify 11b only adhoc when we are in 11g only adhoc mode
		if (BssType == BSS_ADHOC)
		{
			PeerTxType = PeerTxTypeInUseSanity(Channel, SupRate, SupRateLen, ExtRate, ExtRateLen);
			if ((pAd->PortCfg.AdhocMode == ADHOC_11G) && (PeerTxType == CCK_RATE))
			{
				return;
			}
			else if ((pAd->PortCfg.AdhocMode == ADHOC_11B) && (PeerTxType == OFDM_RATE))
			{
				return;
			}
		}

		RealRssi = ConvertToRssi(pAd, Elem->Rssi, RSSI_NO_1);

		is_my_bssid = MAC_ADDR_EQUAL(Bssid, pAd->PortCfg.Bssid)? TRUE : FALSE;
		is_my_ssid = SSID_EQUAL(Ssid, SsidLen, pAd->PortCfg.Ssid, pAd->PortCfg.SsidLen)? TRUE:FALSE;

		// ignore BEACON not for my SSID
		if ((! is_my_ssid) && (! is_my_bssid))
			return;

		//
		// Housekeeping "SsidBssTab" table for later-on ROAMing usage.
		//
		Bssidx = BssTableSearch(&pAd->MlmeAux.SsidBssTab, Bssid, Channel);
		if (Bssidx == BSS_NOT_FOUND)
		{
			// discover new AP of this network, create BSS entry
			Bssidx = BssTableSetEntry(pAd, &pAd->MlmeAux.SsidBssTab, Bssid, Ssid, SsidLen,
						BssType, BeaconPeriod, &CfParm, AtimWin, CapabilityInfo,
						SupRate, SupRateLen, ExtRate, ExtRateLen, Channel, RealRssi + pAd->BbpRssiToDbmDelta, TimeStamp, CkipFlag,
						&EdcaParm, &QosCapability, &QbssLoad, LenVIE, pVIE);

			if (Bssidx == BSS_NOT_FOUND) // return if BSS table full
				return;

			DBGPRINT(RT_DEBUG_INFO, "SYNC - New AP added to SsidBssTab[%d], RSSI=%d, MAC=%02x:%02x:%02x:%02x:%02x:%02x\n",
				Bssidx, RealRssi, Bssid[0], Bssid[1], Bssid[2], Bssid[3], Bssid[4], Bssid[5]);
		}

        if ((pAd->PortCfg.bIEEE80211H == 1) && (NewChannel != 0) && (Channel != NewChannel))
		{
			// channel sanity check
	        for (index = 0 ; index < pAd->ChannelListNum; index++)
            {
				if (pAd->ChannelList[index].Channel == NewChannel)
				{
					pAd->MlmeAux.SsidBssTab.BssEntry[Bssidx].Channel = NewChannel;
					pAd->PortCfg.Channel = NewChannel;
					AsicSwitchChannel(pAd, pAd->PortCfg.Channel);
					AsicLockChannel(pAd, pAd->PortCfg.Channel);
					LinkDown(pAd, FALSE);
					RTMPusecDelay(1000000);		// use delay to prevent STA do reassoc
					DBGPRINT(RT_DEBUG_TRACE, "PeerBeacon - STA receive channel switch announcement IE (New Channel =%d)\n", NewChannel);
					break;
				}
			}

			if (index >= pAd->ChannelListNum)
			{
				DBGPRINT_ERR("PeerBeacon(can not find New Channel=%d in ChannelList[%d]\n", pAd->PortCfg.Channel, pAd->ChannelListNum);
			}
        }

		// if the ssid matched & bssid unmatched, we should select the bssid with large value.
		// This might happened when two STA start at the same time
		if ((! is_my_bssid) && ADHOC_ON(pAd))
		{
			INT	i;

			// Add to safe guard adhoc wep status mismatch
			if (pAd->PortCfg.WepStatus != pAd->MlmeAux.SsidBssTab.BssEntry[Bssidx].WepStatus)
			{
				DBGPRINT(RT_DEBUG_TRACE, "SYNC - Not matched wep status %d %d\n", pAd->PortCfg.WepStatus, pAd->MlmeAux.SsidBssTab.BssEntry[Bssidx].WepStatus);
				return;
			}

			// collapse into the ADHOC network which has bigger BSSID value.
			if (!OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_BSSID_SET)) {
				for (i = 0; i < 6; i++)
				{
					if (Bssid[i] < pAd->PortCfg.Bssid[i])
						break;

					if (Bssid[i] > pAd->PortCfg.Bssid[i])
					{
						DBGPRINT(RT_DEBUG_TRACE,
								"SYNC - merge to the IBSS with bigger BSSID="
								"%02x:%02x:%02x:%02x:%02x:%02x\n",
								Bssid[0], Bssid[1], Bssid[2],
								Bssid[3], Bssid[4], Bssid[5]);
						AsicDisableSync(pAd);
						COPY_MAC_ADDR(pAd->PortCfg.Bssid, Bssid);
						AsicSetBssid(pAd, pAd->PortCfg.Bssid);
						MakeIbssBeacon(pAd);		// re-build BEACON frame
						// copy BEACON frame to on-chip memory
						AsicEnableIbssSync(pAd);
						break;
					}
				}
			} /* End if (addr set by ioctl) */
		}

		DBGPRINT(RT_DEBUG_INFO, "SYNC - PeerBeacon from %02x:%02x:%02x:%02x:%02x:%02x - Dtim=%d/%d, Rssi=%ddBm\n",
			Bssid[0], Bssid[1], Bssid[2], Bssid[3], Bssid[4], Bssid[5],
			DtimCount, DtimPeriod, RealRssi);

		Now = jiffies;
		pBss = &pAd->MlmeAux.SsidBssTab.BssEntry[Bssidx];
		pBss->Rssi = RealRssi + pAd->BbpRssiToDbmDelta; 	  // lastest RSSI
		pBss->LastBeaconRxTime = Now;	// last RX timestamp

		//
		// BEACON from my BSSID - either IBSS or INFRA network
		//
		if (is_my_bssid)
		{
			pAd->PortCfg.LastBeaconRxTime = Now;
			DBGPRINT(RT_DEBUG_INFO,"Rx My BEACON\n");

			// Used the default TX Power Percentage, that set from UI.
			pAd->PortCfg.TxPowerPercentage = pAd->PortCfg.TxPowerDefault;

			// at least one 11b peer joined. downgrade the MaxTxRate to 11Mbps
			// after last 11b peer left for several seconds, we'll auto switch back to 11G rate
			// in MlmePeriodicExec()
			if (ADHOC_ON(pAd) && (SupRateLen+ExtRateLen <= 4))
			{
				// this timestamp is for MlmePeriodicExec() to check if all 11B peers have left
				pAd->PortCfg.Last11bBeaconRxTime = Now;

				if (pAd->PortCfg.MaxTxRate > RATE_11)
				{
					DBGPRINT(RT_DEBUG_TRACE, "SYNC - 11b peer joined. down-grade to 11b TX rates \n");
					memcpy(pAd->ActiveCfg.SupRate, SupRate, MAX_LEN_OF_SUPPORTED_RATES);
					pAd->ActiveCfg.SupRateLen = SupRateLen;
					memcpy(pAd->ActiveCfg.ExtRate, ExtRate, MAX_LEN_OF_SUPPORTED_RATES);
					pAd->ActiveCfg.ExtRateLen = ExtRateLen;
					MlmeUpdateTxRates(pAd, FALSE);
					MakeIbssBeacon(pAd);		// re-build BEACON frame
					AsicEnableIbssSync(pAd);	// copy to on-chip memory
				}
			}

			// check if RSSI reaches threshold
			RealRssi = ConvertToRssi(pAd, Elem->Rssi, RSSI_NO_1);
			pAd->PortCfg.LastRssi = RealRssi + pAd->BbpRssiToDbmDelta;
			pAd->PortCfg.AvgRssiX8 = (pAd->PortCfg.AvgRssiX8 - pAd->PortCfg.AvgRssi) + pAd->PortCfg.LastRssi;
			pAd->PortCfg.AvgRssi  = pAd->PortCfg.AvgRssiX8 >> 3;


			if ((pAd->PortCfg.RssiTriggerMode == RSSI_TRIGGERED_UPON_BELOW_THRESHOLD) &&
				(pAd->PortCfg.LastRssi < pAd->PortCfg.RssiTrigger))
			{
				DBGPRINT(RT_DEBUG_TRACE, "SYNC - NdisMIndicateStatus *** RSSI %d dBm, less than threshold %d dBm\n",
					pAd->PortCfg.LastRssi - pAd->BbpRssiToDbmDelta,
					pAd->PortCfg.RssiTrigger - pAd->BbpRssiToDbmDelta);
			}
			else if ((pAd->PortCfg.RssiTriggerMode == RSSI_TRIGGERED_UPON_EXCCEED_THRESHOLD) &&
				(pAd->PortCfg.LastRssi > pAd->PortCfg.RssiTrigger))
			{
				DBGPRINT(RT_DEBUG_TRACE, "SYNC - NdisMIndicateStatus *** RSSI %d dBm, greater than threshold %d dBm\n",
					pAd->PortCfg.LastRssi - pAd->BbpRssiToDbmDelta,
					pAd->PortCfg.RssiTrigger - pAd->BbpRssiToDbmDelta);
			}

			if (INFRA_ON(pAd)) // && (pAd->PortCfg.PhyMode == PHY_11BG_MIXED))
			{
				BOOLEAN bUseShortSlot, bUseBGProtection;

				// decide to use/change to -
				//		1. long slot (20 us) or short slot (9 us) time
				//		2. turn on/off RTS/CTS and/or CTS-to-self protection
				//		3. short preamble
				bUseShortSlot = pAd->PortCfg.UseShortSlotTime && CAP_IS_SHORT_SLOT(CapabilityInfo);
				if (bUseShortSlot != OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_SHORT_SLOT_INUSED))
					AsicSetSlotTime(pAd, bUseShortSlot);

				bUseBGProtection = (pAd->PortCfg.UseBGProtection == 1) ||	 // always use
								   ((pAd->PortCfg.UseBGProtection == 0) && ERP_IS_USE_PROTECTION(Erp));

				if (pAd->PortCfg.Channel > 14) // always no BG protection in A-band. falsely happened when switching A/G band to a dual-band AP
					bUseBGProtection = FALSE;

				if (bUseBGProtection != OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_BG_PROTECTION_INUSED))
				{
					if (bUseBGProtection)
						OPSTATUS_SET_FLAG(pAd, fOP_STATUS_BG_PROTECTION_INUSED);
					else
						OPSTATUS_CLEAR_FLAG(pAd, fOP_STATUS_BG_PROTECTION_INUSED);
					DBGPRINT(RT_DEBUG_TRACE, "SYNC - AP changed B/G protection to %d\n", bUseBGProtection);
				}

				if (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_SHORT_PREAMBLE_INUSED) &&
					ERP_IS_USE_BARKER_PREAMBLE(Erp))
				{
					MlmeSetTxPreamble(pAd, Rt802_11PreambleLong);
					DBGPRINT(RT_DEBUG_TRACE, "SYNC - AP forced to use LONG preamble\n");
				}

				if (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_WMM_INUSED)	  &&
					(EdcaParm.bValid == TRUE)						   &&
					(EdcaParm.EdcaUpdateCount != pAd->PortCfg.APEdcaParm.EdcaUpdateCount))
				{
					DBGPRINT(RT_DEBUG_TRACE, "SYNC - AP change EDCA parameters(from %d to %d)\n",
						pAd->PortCfg.APEdcaParm.EdcaUpdateCount,
						EdcaParm.EdcaUpdateCount);
					AsicSetEdcaParm(pAd, &EdcaParm);
				}

				// copy QOS related information
				memcpy(&pAd->PortCfg.APQbssLoad, &QbssLoad, sizeof(QBSS_LOAD_PARM));
				memcpy(&pAd->PortCfg.APQosCapability, &QosCapability, sizeof(QOS_CAPABILITY_PARM));

			}

			// only INFRASTRUCTURE mode support power-saving feature
			if (INFRA_ON(pAd) && (pAd->PortCfg.Psm == PWR_SAVE))
			{
//				UCHAR FreeNumber;
				//	1. AP has backlogged unicast-to-me frame, stay AWAKE, send PSPOLL
				//	2. AP has backlogged broadcast/multicast frame and we want those frames, stay AWAKE
				//	3. we have outgoing frames in TxRing or MgmtRing, better stay AWAKE
				//	4. Psm change to PWR_SAVE, but AP not been informed yet, we better stay AWAKE
				//	5. otherwise, put PHY back to sleep to save battery.
				if (MessageToMe)
				{
					DBGPRINT(RT_DEBUG_TRACE, "SYNC - AP backlog unicast-to-me, stay AWAKE, send PSPOLL\n");
					EnqueuePsPoll(pAd);
				}
				else if (BcastFlag && (DtimCount == 0) && OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_RECEIVE_DTIM))
				{
					DBGPRINT(RT_DEBUG_TRACE, "SYNC - AP backlog broadcast/multicast, stay AWAKE\n");
				}
#if 0
				else if ((RTUSBFreeTXDRequest(pAd, QID_AC_BE, TX_RING_SIZE, &FreeNumber) != NDIS_STATUS_SUCCESS)  ||
//						 (RTUSBFreeTXDRequest(pAd, QID_AC_BK, TX_RING_SIZE, &FreeNumber) != NDIS_STATUS_SUCCESS)  ||
//						 (RTUSBFreeTXDRequest(pAd, QID_AC_VI, TX_RING_SIZE, &FreeNumber) != NDIS_STATUS_SUCCESS)	   ||
//						 (RTUSBFreeTXDRequest(pAd, QID_AC_VO, TX_RING_SIZE, &FreeNumber) != NDIS_STATUS_SUCCESS)	   ||
						 (RTUSBFreeTXDRequest(pAd, QID_MGMT, MGMT_RING_SIZE, &FreeNumber) != NDIS_STATUS_SUCCESS))
				{
					// TODO: consider scheduled HCCA. might not be proper to use traditional DTIM-based power-saving scheme
					// can we cheat here (i.e. just check MGMT & AC_BE) for better performance?
					DBGPRINT(RT_DEBUG_TRACE, "SYNC - outgoing frame in TxRing/MgmtRing, stay AWAKE\n");
				}
#endif
				else
				{
					USHORT NextDtim = DtimCount;

					if (NextDtim == 0)
						NextDtim = DtimPeriod;

					TbttNumToNextWakeUp = pAd->PortCfg.DefaultListenCount;
					if (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_RECEIVE_DTIM) && (TbttNumToNextWakeUp > NextDtim))
						TbttNumToNextWakeUp = NextDtim;

					DBGPRINT(RT_DEBUG_INFO, "SYNC - PHY sleeps for %d TBTT, Dtim=%d/%d\n", TbttNumToNextWakeUp, DtimCount, DtimPeriod);
					AsicSleepThenAutoWakeup(pAd, TbttNumToNextWakeUp);
				}
			}

#ifndef	SINGLE_ADHOC_LINKUP
			// At least another peer in this IBSS, declare MediaState as CONNECTED
			if (ADHOC_ON(pAd) &&
				!OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_MEDIA_STATE_CONNECTED))
			{
				OPSTATUS_SET_FLAG(pAd, fOP_STATUS_MEDIA_STATE_CONNECTED);

				// 2003/03/12 - john
				// Make sure this entry in "ScanTab" table, thus complies to Microsoft's policy that
				// "site survey" result should always include the current connected network.
				//
				Bssidx = BssTableSearch(&pAd->ScanTab, Bssid, Channel);
				if (Bssidx == BSS_NOT_FOUND)
				{
					Bssidx = BssTableSetEntry(pAd, &pAd->ScanTab, Bssid, Ssid, SsidLen,
								BssType, BeaconPeriod, &CfParm, AtimWin, CapabilityInfo,
								SupRate, SupRateLen, ExtRate, ExtRateLen, Channel, RealRssi + pAd->BbpRssiToDbmDelta, TimeStamp, CkipFlag,
								&EdcaParm, &QosCapability, &QbssLoad, LenVIE, pVIE);
				}
			}
#endif
		}
		// not my BSSID, ignore it
	}
	// sanity check fail, ignore this frame
}

/*
    ==========================================================================
    Description:
        Receive PROBE REQ from remote peer when operating in IBSS mode
    ==========================================================================
 */
VOID PeerProbeReqAction(
    IN PRTMP_ADAPTER pAd,
    IN MLME_QUEUE_ELEM *Elem)
{
    UCHAR         Addr2[MAC_ADDR_LEN];
    CHAR          Ssid[MAX_LEN_OF_SSID];
    UCHAR         SsidLen;
    HEADER_802_11 ProbeRspHdr;
    PUCHAR        pOutBuffer = NULL;
    ULONG         FrameLen = 0;
    LARGE_INTEGER FakeTimestamp;
    UCHAR         DsLen = 1, IbssLen = 2;
    UCHAR         LocalErpIe[3] = {IE_ERP, 1, 0};
    USHORT        NStatus;
    BOOLEAN       Privacy;
    USHORT        BeaconPeriod, CapabilityInfo, AtimWin;

    if (! ADHOC_ON(pAd))
        return;

    if (PeerProbeReqSanity(pAd, Elem->Msg, Elem->MsgLen, Addr2, Ssid, &SsidLen))
    {
        if ((SsidLen == 0) || SSID_EQUAL(Ssid, SsidLen, pAd->PortCfg.Ssid, pAd->PortCfg.SsidLen))
        {
#if 0
            CSR15_STRUC Csr15;

            // we should respond a ProbeRsp only when we're the last BEACON transmitter
            // in this ADHOC network.
            RTMP_IO_READ32(pAd, CSR15, &Csr15.word);
            if (Csr15.field.BeaconSent == 0)
            {
                DBGPRINT(RT_DEBUG_INFO, "SYNC - NOT last BEACON sender, no PROBE_RSP to %02x:%02x:%02x:%02x:%02x:%02x...\n",
                    Addr2[0],Addr2[1],Addr2[2],Addr2[3],Addr2[4],Addr2[5] );
                return;
            }
#endif

            // allocate and send out ProbeReq frame
            NStatus = MlmeAllocateMemory(pAd, (PVOID)&pOutBuffer);  //Get an unused nonpaged memory
            if (NStatus != NDIS_STATUS_SUCCESS)
	            return;

            //pAd->PortCfg.AtimWin = 0;  // ??????
            DBGPRINT(RT_DEBUG_TRACE,
					"SYNC - Send PROBE_RSP to %02x:%02x:%02x:%02x:%02x:%02x\n",
			Addr2[0],Addr2[1],Addr2[2],Addr2[3],Addr2[4],Addr2[5] );
            MgtMacHeaderInit(pAd, &ProbeRspHdr,
							SUBTYPE_PROBE_RSP, 0, Addr2, pAd->PortCfg.Bssid);

			BeaconPeriod = cpu_to_le16(pAd->PortCfg.BeaconPeriod);
			Privacy = (pAd->PortCfg.WepStatus==Ndis802_11Encryption1Enabled) ||
					(pAd->PortCfg.WepStatus==Ndis802_11Encryption2Enabled) ||
					(pAd->PortCfg.WepStatus==Ndis802_11Encryption3Enabled);
			CapabilityInfo = cpu_to_le16(CAP_GENERATE(0, 1, Privacy,
					(pAd->PortCfg.TxPreamble == Rt802_11PreambleShort), 0));
			AtimWin = cpu_to_le16(pAd->ActiveCfg.AtimWin);

            MakeOutgoingFrame(pOutBuffer,                   &FrameLen,
                              sizeof(HEADER_802_11),        &ProbeRspHdr,
                              TIMESTAMP_LEN,                &FakeTimestamp,
                              2,                            &BeaconPeriod,
                              2,                            &CapabilityInfo,
                              1,                            &SsidIe,
                              1,                            &pAd->PortCfg.SsidLen,
                              pAd->PortCfg.SsidLen,         pAd->PortCfg.Ssid,
                              1,                            &SupRateIe,
                              1,                            &pAd->ActiveCfg.SupRateLen,
                              pAd->ActiveCfg.SupRateLen,    pAd->ActiveCfg.SupRate,
                              1,                            &DsIe,
                              1,                            &DsLen,
                              1,                            &pAd->PortCfg.Channel,
                              1,                            &IbssIe,
                              1,                            &IbssLen,
                              2,                            &AtimWin,
                              END_OF_ARGS);

            if (pAd->ActiveCfg.ExtRateLen)
            {
                ULONG tmp;
                MakeOutgoingFrame(pOutBuffer + FrameLen,        &tmp,
                                  3,                            LocalErpIe,
                                  1,                            &ExtRateIe,
                                  1,                            &pAd->ActiveCfg.ExtRateLen,
                                  pAd->ActiveCfg.ExtRateLen,    &pAd->ActiveCfg.ExtRate,
                                  END_OF_ARGS);
                FrameLen += tmp;
            }

			// If adhoc secruity is set for WPA-None, append the cipher suite IE
			if (pAd->PortCfg.AuthMode == Ndis802_11AuthModeWPANone)
			{
				ULONG	tmp;

				if (pAd->PortCfg.WepStatus == Ndis802_11Encryption2Enabled) 	// Tkip
				{
					MakeOutgoingFrame(pOutBuffer + FrameLen, 		&tmp,
							          1,						    &WpaIe,
							          1,							&CipherSuiteWpaNoneTkipLen,
							          CipherSuiteWpaNoneTkipLen,	&CipherSuiteWpaNoneTkip[0],
							          END_OF_ARGS);
					FrameLen += tmp;
				}
				else if (pAd->PortCfg.WepStatus == Ndis802_11Encryption3Enabled)	// Aes
				{
					MakeOutgoingFrame(pOutBuffer + FrameLen, 		&tmp,
							          1,						    &WpaIe,
							          1,							&CipherSuiteWpaNoneAesLen,
							          CipherSuiteWpaNoneAesLen,	    &CipherSuiteWpaNoneAes[0],
							          END_OF_ARGS);
					FrameLen += tmp;
				}
			}
            MiniportMMRequest(pAd, pOutBuffer, FrameLen);
        }
    }
}

VOID BeaconTimeoutAtJoinAction(
    IN PRTMP_ADAPTER pAd,
    IN MLME_QUEUE_ELEM *Elem)
{
    USHORT  Status;

    DBGPRINT(RT_DEBUG_TRACE, "SYNC - BeaconTimeoutAtJoinAction\n");
    pAd->Mlme.SyncMachine.CurrState = SYNC_IDLE;
    Status = MLME_REJ_TIMEOUT;
    MlmeEnqueue(pAd, MLME_CNTL_STATE_MACHINE, MT2_JOIN_CONF, 2, &Status);
}

/*
    ==========================================================================
    Description:
        Scan timeout procedure. basically add channel index by 1 and rescan
    ==========================================================================
 */
VOID ScanTimeoutAction(
    IN PRTMP_ADAPTER pAd,
    IN MLME_QUEUE_ELEM *Elem)
{
//  DBGPRINT(RT_DEBUG_TRACE,"SYNC - ScanTimeoutAction\n");
    pAd->MlmeAux.Channel = NextChannel(pAd, pAd->MlmeAux.Channel);
    ScanNextChannel(pAd); // this routine will stop if pAd->MlmeAux.Channel == 0
}

/*
    ==========================================================================
    Description:
        Scan next channel
    ==========================================================================
 */
VOID ScanNextChannel(
	IN PRTMP_ADAPTER pAd)
{
    HEADER_802_11   Hdr80211;
    PUCHAR          pOutBuffer = NULL;
    ULONG           FrameLen = 0;
    UCHAR           SsidLen = 0, ScanType = pAd->MlmeAux.ScanType;
    USHORT          Status;
    PHEADER_802_11  pHdr80211;
    USHORT          NStatus;
    PUCHAR          pSupRate = NULL;
    UCHAR           SupRateLen;
    PUCHAR          pExtRate = NULL;
    UCHAR           ExtRateLen;
//For A band
    UCHAR           ASupRate[] = {0x8C, 0x12, 0x98, 0x24, 0xb0, 0x48, 0x60, 0x6C};
    UCHAR           ASupRateLen = sizeof(ASupRate)/sizeof(UCHAR);

    DBGPRINT(RT_DEBUG_INFO, "ScanNextChannel(ch=%d)\n",pAd->MlmeAux.Channel);

    if (pAd->MlmeAux.Channel == 0)
    {
        DBGPRINT(RT_DEBUG_TRACE, "SYNC - End of SCAN, restore to channel %d\n",pAd->PortCfg.Channel);

        AsicSwitchChannel(pAd, pAd->PortCfg.Channel);
        AsicLockChannel(pAd, pAd->PortCfg.Channel);

        // G band - set BBP_R62 to 0x02 when site survey or rssi<-82
		// A band - always set BBP_R62 to 0x04
        if (pAd->PortCfg.Channel <= 14)
        {
        	RTUSBWriteBBPRegister(pAd, BBP_R62, 0x02);
        }
        else
        {
        	RTUSBWriteBBPRegister(pAd, BBP_R62, 0x04);
        }

		//
		// To prevent data lost.
		// Send an NULL data with turned PSM bit on to current associated AP before SCAN progress.
		// Now, we need to send an NULL data with turned PSM bit off to AP, when scan progress done
		//
		if (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_MEDIA_STATE_CONNECTED) && (INFRA_ON(pAd)))
		{
            NStatus = MlmeAllocateMemory(pAd, (PVOID)&pOutBuffer);  //Get an unused nonpaged memory
            if (NStatus == NDIS_STATUS_SUCCESS)
            {
				pHdr80211 = (PHEADER_802_11) pOutBuffer;
				MgtMacHeaderInit(pAd, pHdr80211, SUBTYPE_NULL_FUNC, 1, pAd->PortCfg.Bssid, pAd->PortCfg.Bssid);
				pHdr80211->Duration = 0;
				pHdr80211->FC.Type = BTYPE_DATA;
				pHdr80211->FC.PwrMgmt = PWR_ACTIVE;

				// Send using priority queue
				MiniportMMRequest(pAd, pOutBuffer, sizeof(HEADER_802_11));
				DBGPRINT(RT_DEBUG_TRACE, "MlmeScanReqAction -- Send PSM Data frame\n");

				RTMPusecDelay(5000);
			}
		}

        pAd->Mlme.SyncMachine.CurrState = SYNC_IDLE;
        Status = MLME_SUCCESS;
        MlmeEnqueue(pAd, MLME_CNTL_STATE_MACHINE, MT2_SCAN_CONF, 2, &Status);

    }
    else if (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST))
	{
		pAd->Mlme.SyncMachine.CurrState = SYNC_IDLE;
		Status = MLME_FAIL_NO_RESOURCE;
		MlmeEnqueue(pAd, SYNC_STATE_MACHINE, MT2_SCAN_CONF, 2, &Status);
	}
    else
    {
        // BBP and RF are not accessible in PS mode, we has to wake them up first
        if (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_DOZE))
            AsicForceWakeup(pAd);

        // leave PSM during scanning. otherwise we may lost ProbeRsp & BEACON
        if (pAd->PortCfg.Psm == PWR_SAVE)
            MlmeSetPsmBit(pAd, PWR_ACTIVE);

        AsicSwitchChannel(pAd, pAd->MlmeAux.Channel);
        AsicLockChannel(pAd, pAd->MlmeAux.Channel);

        // G band - set BBP_R62 to 0x02 when site survey or rssi<-82
		// A band - always set BBP_R62 to 0x04
        if (pAd->MlmeAux.Channel <= 14)
        {
		    //
			// For the high power and False CCA issue.(Gary)
			//
			RTUSBWriteBBPRegister(pAd, BBP_R62, 0x04);
			RTUSBWriteBBPRegister(pAd, 17, pAd->BbpTuning.R17LowerBoundG);        }
        else
        {
            if ((pAd->PortCfg.bIEEE80211H == 1) && RadarChannelCheck(pAd, pAd->MlmeAux.Channel))
			    ScanType = SCAN_PASSIVE;

        	RTUSBWriteBBPRegister(pAd, 17, pAd->BbpTuning.R17LowerBoundA);
        	RTUSBWriteBBPRegister(pAd, BBP_R62, 0x04);
        }


		// We need to shorten active scan time in order for WZC connect issue
		// Chnage the channel scan time for CISCO stuff based on its IAPP announcement
        if (ScanType == FAST_SCAN_ACTIVE)
        {
			RTMPSetTimer(pAd, &pAd->MlmeAux.ScanTimer, FAST_ACTIVE_SCAN_TIME);
        }
        else // must be SCAN_PASSIVE or SCAN_ACTIVE
        {
        	if (pAd->PortCfg.PhyMode == PHY_11ABG_MIXED)
				RTMPSetTimer(pAd, &pAd->MlmeAux.ScanTimer, MIN_CHANNEL_TIME);
			else
			    RTMPSetTimer(pAd, &pAd->MlmeAux.ScanTimer, MAX_CHANNEL_TIME);
        }

	if ((pAd->MlmeAux.Channel == 34) || (pAd->MlmeAux.Channel == 38) ||
	    (pAd->MlmeAux.Channel == 42) || (pAd->MlmeAux.Channel == 46))
	{
			ScanType = SCAN_PASSIVE;
	}

        if ((ScanType == SCAN_ACTIVE) || (ScanType == FAST_SCAN_ACTIVE))
        {
            NStatus = MlmeAllocateMemory(pAd, (PVOID)&pOutBuffer);  //Get an unused nonpaged memory
            if (NStatus != NDIS_STATUS_SUCCESS)
            {
    	        DBGPRINT(RT_DEBUG_TRACE, "SYNC - ScanNextChannel() allocate memory fail\n");
        	    pAd->Mlme.SyncMachine.CurrState = SYNC_IDLE;
        	    Status = MLME_FAIL_NO_RESOURCE;
                MlmeEnqueue(pAd, MLME_CNTL_STATE_MACHINE, MT2_SCAN_CONF, 2, &Status);
	            return;
    	    }

    		// There is no need to send broadcast probe request if active scan is in effect.
            SsidLen = pAd->MlmeAux.SsidLen;

   	if (pAd->MlmeAux.Channel <= 14)
	{
		// B&G band use 1,2,5.5,11
		pSupRate = pAd->PortCfg.SupRate;
		SupRateLen = pAd->PortCfg.SupRateLen;
		pExtRate = pAd->PortCfg.ExtRate;
		ExtRateLen = pAd->PortCfg.ExtRateLen;
	}
	else    // A band use OFDM rate
	{
		//
		// Overwrite Support Rate, CCK rate are not allowed
		//
		pSupRate = ASupRate;
		SupRateLen = ASupRateLen;
		ExtRateLen = 0;
	}


    	    MgtMacHeaderInit(pAd, &Hdr80211, SUBTYPE_PROBE_REQ, 0, BROADCAST_ADDR, BROADCAST_ADDR);
            MakeOutgoingFrame(pOutBuffer,               &FrameLen,
                              sizeof(HEADER_802_11),    &Hdr80211,
                              1,                        &SsidIe,
                              1,                        &SsidLen,
                              SsidLen,			pAd->MlmeAux.Ssid,
                              1,                        &SupRateIe,
                              1,                        &SupRateLen,
	                      SupRateLen,  		pSupRate,
                              END_OF_ARGS);

            if (ExtRateLen)
            {
                ULONG Tmp;
            	MakeOutgoingFrame(pOutBuffer + FrameLen,        &Tmp,
                                  1,                            &ExtRateIe,
                                  1,                            &ExtRateLen,
                                  ExtRateLen,      		pExtRate,
                                  END_OF_ARGS);
            	FrameLen += Tmp;
            }

            // add Simple Config Information Element
            if (pAd->PortCfg.bWscCapable && pAd->PortCfg.WscIEProbeReq.ValueLen)
            {
                ULONG WscTmpLen = 0;

                MakeOutgoingFrame(pOutBuffer+FrameLen,                      &WscTmpLen,
                                  pAd->PortCfg.WscIEProbeReq.ValueLen,      pAd->PortCfg.WscIEProbeReq.Value,
                                  END_OF_ARGS);
                FrameLen += WscTmpLen;
            }


            if (pAd->PortCfg.bGetAPConfig)
            {
                UCHAR RalinkSpecificIEForGetCfg[6] = {IE_VENDOR_SPECIFIC, 4, 0x00, 0x0c, 0x43, 0x80};
                ULONG Tmp = 0;
                MakeOutgoingFrame(pOutBuffer + FrameLen,        &Tmp,
                                  6,                            RalinkSpecificIEForGetCfg,
                                  END_OF_ARGS);
                FrameLen += Tmp;
            }

            MiniportMMRequest(pAd, pOutBuffer, FrameLen);

            DBGPRINT(RT_DEBUG_INFO, "SYNC - send ProbeReq @ channel=%d, Len=%d\n", pAd->MlmeAux.Channel, FrameLen);
        }

		// For SCAN_CISCO_PASSIVE, do nothing and silently wait for beacon or other probe reponse

        pAd->Mlme.SyncMachine.CurrState = SCAN_LISTEN;
    }
}

/*
    ==========================================================================
    Description:
    ==========================================================================
 */
VOID InvalidStateWhenScan(
    IN PRTMP_ADAPTER pAd,
    IN MLME_QUEUE_ELEM *Elem)
{
    USHORT  Status;
    DBGPRINT(RT_DEBUG_TRACE, "AYNC - InvalidStateWhenScan(state=%d). Reset SYNC machine\n", pAd->Mlme.SyncMachine.CurrState);
    pAd->Mlme.SyncMachine.CurrState = SYNC_IDLE;
    Status = MLME_STATE_MACHINE_REJECT;
    MlmeEnqueue(pAd, MLME_CNTL_STATE_MACHINE, MT2_SCAN_CONF, 2, &Status);
}

/*
    ==========================================================================
    Description:
    ==========================================================================
 */
VOID InvalidStateWhenJoin(
    IN PRTMP_ADAPTER pAd,
    IN MLME_QUEUE_ELEM *Elem)
{
    USHORT  Status;

    DBGPRINT(RT_DEBUG_TRACE, "AYNC - InvalidStateWhenJoin(state=%d). Reset SYNC machine\n", pAd->Mlme.SyncMachine.CurrState);
    pAd->Mlme.SyncMachine.CurrState = SYNC_IDLE;
    Status = MLME_STATE_MACHINE_REJECT;
    MlmeEnqueue(pAd, MLME_CNTL_STATE_MACHINE, MT2_JOIN_CONF, 2, &Status);
}

/*
    ==========================================================================
    Description:
    ==========================================================================
 */
VOID InvalidStateWhenStart(
    IN PRTMP_ADAPTER pAd,
    IN MLME_QUEUE_ELEM *Elem)
{
    USHORT  Status;

    DBGPRINT(RT_DEBUG_TRACE, "AYNC - InvalidStateWhenStart(state=%d). Reset SYNC machine\n", pAd->Mlme.SyncMachine.CurrState);
    pAd->Mlme.SyncMachine.CurrState = SYNC_IDLE;
    Status = MLME_STATE_MACHINE_REJECT;
    MlmeEnqueue(pAd, MLME_CNTL_STATE_MACHINE, MT2_START_CONF, 2, &Status);
}

/*
    ==========================================================================
    Description:

    ==========================================================================
 */
VOID EnqueuePsPoll(
    IN PRTMP_ADAPTER pAd)
{
    DBGPRINT(RT_DEBUG_TRACE, "SYNC - send PsPoll ...\n");
    MiniportMMRequest(pAd, (PUCHAR)&pAd->PsPollFrame, sizeof(PSPOLL_FRAME));
}

// 2003-04-17 john
// driver force send out a BEACON frame to cover ADHOC mode BEACON starving issue
// that is, in ADHOC mode, driver guarantee itself can send out at least a BEACON
// per a specified duration, even the peer's clock is faster than us and win all the
// hardware-based BEACON TX oppertunity.
// we may remove this software feature once 2560 IC fix this problem in ASIC.

// Beacon has been built in MakeIbssBeacon () - bb
VOID EnqueueBeaconFrame(
    IN PRTMP_ADAPTER pAd)
{

    PTXD_STRUC		pTxD;
    PCHAR           pBeaconFrame = pAd->BeaconBuf;
    PUCHAR			pOutBuffer = NULL;
    NDIS_STATUS		NStatus;

	pTxD = &pAd->BeaconTxD;

#ifdef BIG_ENDIAN
    RTMPDescriptorEndianChange((PUCHAR)pTxD, TYPE_TXD);
#endif
    DBGPRINT(RT_DEBUG_TRACE, "SYNC (%s) - driver sent BEACON (len=%d)\n",
			__FUNCTION__, pTxD->DataByteCnt);
#ifdef BIG_ENDIAN
    RTMPDescriptorEndianChange((PUCHAR)pTxD, TYPE_TXD);
#endif

    NStatus = MlmeAllocateMemory(pAd, (PVOID)&pOutBuffer);  //Get an unused nonpaged memory
	if (NStatus != NDIS_STATUS_SUCCESS)
	{
		DBGPRINT(RT_DEBUG_TRACE, "EnqueueBeaconFrame allocate memory fail\n");
		return;
	}

	// Preserve eight byte TSF little endian byte order - bb
    RTUSBMultiRead(pAd, TXRX_CSR12,
				pBeaconFrame + sizeof(HEADER_802_11), TIMESTAMP_LEN);

	memcpy(pOutBuffer, pBeaconFrame, 256);
	MiniportMMRequest(pAd, pOutBuffer, pTxD->DataByteCnt);

}

/*
    ==========================================================================
    Description:
    ==========================================================================
 */
VOID EnqueueProbeRequest(
    IN PRTMP_ADAPTER pAd)
{
    PUCHAR          pOutBuffer;
    ULONG           FrameLen = 0;
    HEADER_802_11   Hdr80211;
    USHORT          NStatus;

    DBGPRINT(RT_DEBUG_TRACE, "force out a ProbeRequest ...\n");


    NStatus = MlmeAllocateMemory(pAd, (PVOID)&pOutBuffer);  //Get an unused nonpaged memory
    if (NStatus != NDIS_STATUS_SUCCESS)
    {
        DBGPRINT(RT_DEBUG_TRACE, "EnqueueProbeRequest() allocate memory fail\n");
        return;
    }
    else
    {
        MgtMacHeaderInit(pAd, &Hdr80211, SUBTYPE_PROBE_REQ, 0, BROADCAST_ADDR, BROADCAST_ADDR);

		// this ProbeRequest explicitly specify SSID to reduce unwanted ProbeResponse
        MakeOutgoingFrame(pOutBuffer,                     &FrameLen,
                          sizeof(HEADER_802_11),          &Hdr80211,
                          1,                              &SsidIe,
                          1,                              &pAd->PortCfg.SsidLen,
                          pAd->PortCfg.SsidLen,		      pAd->PortCfg.Ssid,
                          1,                              &SupRateIe,
                          1,                              &pAd->ActiveCfg.SupRateLen,
                          pAd->ActiveCfg.SupRateLen,      pAd->ActiveCfg.SupRate,
                          END_OF_ARGS);

        // add Simple Config Information Element
        if (pAd->PortCfg.bWscCapable && pAd->PortCfg.WscIEProbeReq.ValueLen)
        {
            ULONG WscTmpLen = 0;

            MakeOutgoingFrame(pOutBuffer+FrameLen,                   &WscTmpLen,
        		              pAd->PortCfg.WscIEProbeReq.ValueLen,   pAd->PortCfg.WscIEProbeReq.Value,
                              END_OF_ARGS);
            FrameLen += WscTmpLen;
        }

	    if (pAd->PortCfg.bGetAPConfig)
	    {
	 	    UCHAR RalinkSpecificIEForGetCfg[6] = {IE_VENDOR_SPECIFIC, 4, 0x00, 0x0c, 0x43, 0x80};
		    ULONG Tmp = 0;
		    MakeOutgoingFrame(pOutBuffer + FrameLen,        &Tmp,
			                  6,                            RalinkSpecificIEForGetCfg,
			                  END_OF_ARGS);
		FrameLen += Tmp;
	    }

        MiniportMMRequest(pAd, pOutBuffer, FrameLen);
    }

}

/*
    ==========================================================================
    Description:
        Update PortCfg->ChannelList[] according to 1) Country Region 2) RF IC type,
        and 3) PHY-mode user selected.
        The outcome is used by driver when doing site survey.
    ==========================================================================
 */
VOID BuildChannelList(
	IN PRTMP_ADAPTER pAd)
{
    UCHAR i, j, index=0, num=0;
    PUCHAR	pChannelList=NULL;

    memset(pAd->ChannelList, 0, MAX_NUM_OF_CHANNELS * sizeof(CHANNEL_TX_POWER));

    // if not 11a-only mode, channel list starts from 2.4Ghz band
    if (pAd->PortCfg.PhyMode != PHY_11A)
    {
        switch (pAd->PortCfg.CountryRegion & 0x7f)
        {
            case REGION_0_BG_BAND:	// 1 -11
                memcpy(&pAd->ChannelList[index], &pAd->TxPower[BG_BAND_REGION_0_START], sizeof(CHANNEL_TX_POWER) * BG_BAND_REGION_0_SIZE);
                index += BG_BAND_REGION_0_SIZE;
                break;
            case REGION_1_BG_BAND:	// 1 - 13
                memcpy(&pAd->ChannelList[index], &pAd->TxPower[BG_BAND_REGION_1_START], sizeof(CHANNEL_TX_POWER) * BG_BAND_REGION_1_SIZE);
                index += BG_BAND_REGION_1_SIZE;
                break;
            case REGION_2_BG_BAND:	// 10 - 11
                memcpy(&pAd->ChannelList[index], &pAd->TxPower[BG_BAND_REGION_2_START], sizeof(CHANNEL_TX_POWER) * BG_BAND_REGION_2_SIZE);
                index += BG_BAND_REGION_2_SIZE;
                break;
            case REGION_3_BG_BAND:	// 10 - 13
                memcpy(&pAd->ChannelList[index], &pAd->TxPower[BG_BAND_REGION_3_START], sizeof(CHANNEL_TX_POWER) * BG_BAND_REGION_3_SIZE);
                index += BG_BAND_REGION_3_SIZE;
                break;
            case REGION_4_BG_BAND:	// 14
                memcpy(&pAd->ChannelList[index], &pAd->TxPower[BG_BAND_REGION_4_START], sizeof(CHANNEL_TX_POWER) * BG_BAND_REGION_4_SIZE);
                index += BG_BAND_REGION_4_SIZE;
                break;
            case REGION_5_BG_BAND:	// 1 - 14
                memcpy(&pAd->ChannelList[index], &pAd->TxPower[BG_BAND_REGION_5_START], sizeof(CHANNEL_TX_POWER) * BG_BAND_REGION_5_SIZE);
                index += BG_BAND_REGION_5_SIZE;
                break;
            case REGION_6_BG_BAND:	// 3 - 9
                memcpy(&pAd->ChannelList[index], &pAd->TxPower[BG_BAND_REGION_6_START], sizeof(CHANNEL_TX_POWER) * BG_BAND_REGION_6_SIZE);
                index += BG_BAND_REGION_6_SIZE;
                break;
			case REGION_7_BG_BAND:  // 5 - 13
                memcpy(&pAd->ChannelList[index], &pAd->TxPower[BG_BAND_REGION_7_START], sizeof(CHANNEL_TX_POWER) * BG_BAND_REGION_7_SIZE);
                index += BG_BAND_REGION_7_SIZE;
                break;
            default:                // Error. should never happen
                break;
        }
    }

    if (pAd->PortCfg.PhyMode == PHY_11A || (pAd->PortCfg.PhyMode == PHY_11ABG_MIXED))
    {
    	switch (pAd->PortCfg.CountryRegionForABand & 0x7f)
        {
            case REGION_0_A_BAND:
            	num = sizeof(A_BAND_REGION_0_CHANNEL_LIST)/sizeof(UCHAR);
	            pChannelList = A_BAND_REGION_0_CHANNEL_LIST;
                break;
            case REGION_1_A_BAND:
            	num = sizeof(A_BAND_REGION_1_CHANNEL_LIST)/sizeof(UCHAR);
            	pChannelList = A_BAND_REGION_1_CHANNEL_LIST;
                break;
            case REGION_2_A_BAND:
            	num = sizeof(A_BAND_REGION_2_CHANNEL_LIST)/sizeof(UCHAR);
            	pChannelList = A_BAND_REGION_2_CHANNEL_LIST;
                break;
            case REGION_3_A_BAND:
            	num = sizeof(A_BAND_REGION_3_CHANNEL_LIST)/sizeof(UCHAR);
            	pChannelList = A_BAND_REGION_3_CHANNEL_LIST;
                break;
            case REGION_4_A_BAND:
            	num = sizeof(A_BAND_REGION_4_CHANNEL_LIST)/sizeof(UCHAR);
            	pChannelList = A_BAND_REGION_4_CHANNEL_LIST;
                break;
            case REGION_5_A_BAND:
            	num = sizeof(A_BAND_REGION_5_CHANNEL_LIST)/sizeof(UCHAR);
            	pChannelList = A_BAND_REGION_5_CHANNEL_LIST;
                break;
            case REGION_6_A_BAND:
            	num = sizeof(A_BAND_REGION_6_CHANNEL_LIST)/sizeof(UCHAR);
            	pChannelList = A_BAND_REGION_6_CHANNEL_LIST;
                break;
            case REGION_7_A_BAND:
            	num = sizeof(A_BAND_REGION_7_CHANNEL_LIST)/sizeof(UCHAR);
            	pChannelList = A_BAND_REGION_7_CHANNEL_LIST;
                break;
            case REGION_8_A_BAND:
				num = sizeof(A_BAND_REGION_8_CHANNEL_LIST)/sizeof(UCHAR);
				pChannelList = A_BAND_REGION_8_CHANNEL_LIST;
				break;
			case REGION_9_A_BAND:
				num = sizeof(A_BAND_REGION_9_CHANNEL_LIST)/sizeof(UCHAR);
				pChannelList = A_BAND_REGION_9_CHANNEL_LIST;
				break;
			case REGION_10_A_BAND:
				num = sizeof(A_BAND_REGION_10_CHANNEL_LIST)/sizeof(UCHAR);
				pChannelList = A_BAND_REGION_10_CHANNEL_LIST;
				break;
            default:            // Error. should never happen
            	DBGPRINT(RT_DEBUG_WARN,"countryregion=%d not support", pAd->PortCfg.CountryRegionForABand);
                break;
        }

		if (num != 0)
		{
	        for (i=0; i<num; i++)
	        {
	        	for (j=0; j<MAX_NUM_OF_CHANNELS; j++)
	        	{
	        		if (pChannelList[i] == pAd->TxPower[j].Channel)
	        		{
			        	memcpy(&pAd->ChannelList[index+i], &pAd->TxPower[j], sizeof(CHANNEL_TX_POWER));
	        		}
	        	}
	        }
	        index += num;
		}
    }

    pAd->ChannelListNum = index;


    if (pAd->PortCfg.BssType == BSS_ADHOC)
    {
        if (ChannelSanity(pAd, pAd->PortCfg.AdhocChannel) == TRUE)
			pAd->PortCfg.Channel = pAd->PortCfg.AdhocChannel;   // sync. to the value of PortCfg.AdhocChannel
	}


    DBGPRINT(RT_DEBUG_TRACE,"country code=%d/%d, RFIC=%d, PHY mode=%d, support %d channels\n",
        pAd->PortCfg.CountryRegion, pAd->PortCfg.CountryRegionForABand, pAd->RfIcType, pAd->PortCfg.PhyMode, pAd->ChannelListNum);
	DBGPRINT(RT_DEBUG_TRACE, "channel #");
    for (i=0;i<index;i++)
    {
        DBGPRINT_RAW(RT_DEBUG_TRACE," %d", pAd->ChannelList[i].Channel);
    }
	DBGPRINT_RAW(RT_DEBUG_TRACE, "\n");
}

/*
    ==========================================================================
    Description:
        This routine returns the next channel number. This routine is called
        during driver need to start a site survey of all supported channels.
    Return:
        next_channel - the next channel number valid in current country code setting.
    Note:
        return 0 if no more next channel
    ==========================================================================
 */
UCHAR NextChannel(
    IN PRTMP_ADAPTER pAd,
    IN UCHAR channel)
{
    int i;
    UCHAR next_channel = 0;

    for (i = 0; i < (pAd->ChannelListNum - 1); i++)
        if (channel == pAd->ChannelList[i].Channel)
        {
            next_channel = pAd->ChannelList[i+1].Channel;
			break;
	}
    return next_channel;
}

/*
    ==========================================================================
    Description:
        This routine return the first channel number according to the country
        code selection and RF IC selection (signal band or dual band). It is called
        whenever driver need to start a site survey of all supported channels.
    Return:
        ch - the first channel number of current country code setting
    ==========================================================================
 */
UCHAR FirstChannel(
    IN PRTMP_ADAPTER pAd)
{
	return pAd->ChannelList[0].Channel;
}

CHAR	ConvertToRssi(
    IN PRTMP_ADAPTER pAd,
	IN	UCHAR	Rssi,
	IN  UCHAR   RssiNumber)
{
#ifdef BIG_ENDIAN
	typedef	union	_LNA_AGC
	{
		struct
		{
            UCHAR	Rsvd:1;
            UCHAR	Lna:2;
			UCHAR	Agc:5;
		}	field;
		UCHAR		Byte;
	}	LNA_AGC;
#else
	typedef	union	_LNA_AGC
	{
		struct
		{
			UCHAR	Agc:5;
			UCHAR	Lna:2;
			UCHAR	Rsvd:1;
		}	field;
		UCHAR		Byte;
	}	LNA_AGC;
#endif

	LNA_AGC	LnaAgc;
	CHAR	RssiOffset = 0;

	//
	// Get RSSI Offset.
	//
	if (pAd->PortCfg.Channel <= 14)
	{
		if (RssiNumber == RSSI_NO_1)
			RssiOffset = pAd->BGRssiOffset1;
		else if (RssiNumber == RSSI_NO_2)
			RssiOffset = pAd->BGRssiOffset2;
	}
	else
	{
		if (RssiNumber == RSSI_NO_1)
			RssiOffset = pAd->ARssiOffset1;
		else if (RssiNumber == RSSI_NO_2)
			RssiOffset = pAd->ARssiOffset2;
	}

	LnaAgc.Byte = Rssi;
	if (pAd->NicConfig2.field.ExternalLNA == 0)
	{
		if (LnaAgc.field.Lna == 0x03)
		{
			if (pAd->PortCfg.Channel <= 14)
				return (LnaAgc.field.Agc * 2 - 90 + RssiOffset); //for B/G mode
			else
				return (LnaAgc.field.Agc * 2 - 96 + RssiOffset); //for A mode
		}
		else if (LnaAgc.field.Lna == 0x02)
		{
			if (pAd->PortCfg.Channel <= 14)
				return (LnaAgc.field.Agc * 2 - 74 + RssiOffset); //for B/G mode
			else
				return (LnaAgc.field.Agc * 2 - 82 + RssiOffset); //for A mode
		}
		else if (LnaAgc.field.Lna == 0x01)
		{
			return (LnaAgc.field.Agc * 2 - 64 + RssiOffset);
		}
		else
		{
			return -1;
		}
	}
	else
	{
		// RSSI needs to be offset when external LNA enable
		if (LnaAgc.field.Lna == 0x03)
		{
			if (pAd->PortCfg.Channel <= 14)
				return (LnaAgc.field.Agc * 2 - (90 + 14) + RssiOffset); //for B/G mode
			else
				return (LnaAgc.field.Agc * 2 - (100 + 14) + RssiOffset); //for A mode
		}
		else if (LnaAgc.field.Lna == 0x02)
		{
			if (pAd->PortCfg.Channel <= 14)
				return (LnaAgc.field.Agc * 2 - (74 + 14) + RssiOffset); //for B/G mode
			else
				return (LnaAgc.field.Agc * 2 - (86 + 14) + RssiOffset); //for A mode
		}
		else if (LnaAgc.field.Lna == 0x01)
		{
			return (LnaAgc.field.Agc * 2 - (64 + 14) + RssiOffset);
		}
		else
		{
			return -1;
		}
	}
}

