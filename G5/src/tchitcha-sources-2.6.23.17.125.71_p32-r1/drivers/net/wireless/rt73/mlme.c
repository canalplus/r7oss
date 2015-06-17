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
 *	Module Name:	mlme.c
 *
 *	Abstract:
 *
 *	Revision History:
 *	Who		When		What
 *	--------	----------	-----------------------------
 *	John Chang	2004-08-25	Modify from RT2500 code base
 *	John Chang	2004-09-06	modified for RT2600
 *	idamlaj	05-10-2006	Import rfmon implementation
 *
 ***************************************************************************/

#include "rt_config.h"
#include <stdarg.h>
#include <net/iw_handler.h>


// since RT61 has better RX sensibility, we have to limit TX ACK rate not to exceed our normal data TX rate.
// otherwise the WLAN peer may not be able to receive the ACK thus downgrade its data TX rate
ULONG BasicRateMask[12] 		   = {0xfffff001 /* 1-Mbps */, 0xfffff003 /* 2 Mbps */, 0xfffff007 /* 5.5 */, 0xfffff00f /* 11 */,
									  0xfffff01f /* 6 */	 , 0xfffff03f /* 9 */	  , 0xfffff07f /* 12 */ , 0xfffff0ff /* 18 */,
									  0xfffff1ff /* 24 */	 , 0xfffff3ff /* 36 */	  , 0xfffff7ff /* 48 */ , 0xffffffff /* 54 */};

UCHAR BROADCAST_ADDR[MAC_ADDR_LEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
UCHAR ZERO_MAC_ADDR[MAC_ADDR_LEN]  = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

// e.g. RssiSafeLevelForTxRate[RATE_36]" means if the current RSSI is greater than
//		this value, then it's quaranteed capable of operating in 36 mbps TX rate in
//		clean environment.
//								  TxRate: 1   2   5.5	11	 6	  9    12	18	 24   36   48	54	 72  100
CHAR RssiSafeLevelForTxRate[] ={  -92, -91, -90, -87, -88, -86, -85, -83, -81, -78, -72, -71, -40, -40 };

								  //  1 	 2		 5.5	  11
UCHAR Phy11BNextRateDownward[] = {RATE_1, RATE_1,	RATE_2,  RATE_5_5};
UCHAR Phy11BNextRateUpward[]   = {RATE_2, RATE_5_5, RATE_11, RATE_11};

								  //  1 	 2		 5.5	  11		6		 9		  12	  18	   24		36		 48 	  54
UCHAR Phy11BGNextRateDownward[]= {RATE_1, RATE_1,	RATE_2,  RATE_5_5,RATE_11,	RATE_6,  RATE_11, RATE_12, RATE_18, RATE_24, RATE_36, RATE_48};
UCHAR Phy11BGNextRateUpward[]  = {RATE_2, RATE_5_5, RATE_11, RATE_12, RATE_9,	RATE_12, RATE_18, RATE_24, RATE_36, RATE_48, RATE_54, RATE_54};

								  //  1 	 2		 5.5	  11		6		 9		  12	  18	   24		36		 48 	  54
UCHAR Phy11ANextRateDownward[] = {RATE_6, RATE_6,	RATE_6,  RATE_6,  RATE_6,	RATE_6,  RATE_9,  RATE_12, RATE_18, RATE_24, RATE_36, RATE_48};
UCHAR Phy11ANextRateUpward[]   = {RATE_9, RATE_9,	RATE_9,  RATE_9,  RATE_9,	RATE_12, RATE_18, RATE_24, RATE_36, RATE_48, RATE_54, RATE_54};

//								RATE_1,  2, 5.5, 11,  6,  9, 12, 18, 24, 36, 48, 54
static USHORT RateUpPER[]	= {    40,	40,  35, 20, 20, 20, 20, 16, 10, 16, 10,  6 }; // in percentage
static USHORT RateDownPER[] = {    50,	50,  45, 45, 35, 35, 35, 35, 25, 25, 25, 13 }; // in percentage

UCHAR  RateIdToMbps[]	 = { 1, 2, 5, 11, 6, 9, 12, 18, 24, 36, 48, 54, 72, 100};
USHORT RateIdTo500Kbps[] = { 2, 4, 11, 22, 12, 18, 24, 36, 48, 72, 96, 108, 144, 200};

UCHAR	ZeroSsid[32] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};

UCHAR  SsidIe	 = IE_SSID;
UCHAR  SupRateIe = IE_SUPP_RATES;
UCHAR  ExtRateIe = IE_EXT_SUPP_RATES;
UCHAR  ErpIe	 = IE_ERP;
UCHAR  DsIe 	 = IE_DS_PARM;
UCHAR  TimIe	 = IE_TIM;
UCHAR  WpaIe	 = IE_WPA;
UCHAR  Wpa2Ie	 = IE_WPA2;
UCHAR  IbssIe	 = IE_IBSS_PARM;

extern UCHAR	WPA_OUI[];
extern UCHAR	RSN_OUI[];

RTMP_RF_REGS RF2528RegTable[] = {
//		ch	 R1 		 R2 		 R3(TX0~4=0) R4
		{1,  0x94002c0c, 0x94000786, 0x94068255, 0x940fea0b},
		{2,  0x94002c0c, 0x94000786, 0x94068255, 0x940fea1f},
		{3,  0x94002c0c, 0x9400078a, 0x94068255, 0x940fea0b},
		{4,  0x94002c0c, 0x9400078a, 0x94068255, 0x940fea1f},
		{5,  0x94002c0c, 0x9400078e, 0x94068255, 0x940fea0b},
		{6,  0x94002c0c, 0x9400078e, 0x94068255, 0x940fea1f},
		{7,  0x94002c0c, 0x94000792, 0x94068255, 0x940fea0b},
		{8,  0x94002c0c, 0x94000792, 0x94068255, 0x940fea1f},
		{9,  0x94002c0c, 0x94000796, 0x94068255, 0x940fea0b},
		{10, 0x94002c0c, 0x94000796, 0x94068255, 0x940fea1f},
		{11, 0x94002c0c, 0x9400079a, 0x94068255, 0x940fea0b},
		{12, 0x94002c0c, 0x9400079a, 0x94068255, 0x940fea1f},
		{13, 0x94002c0c, 0x9400079e, 0x94068255, 0x940fea0b},
		{14, 0x94002c0c, 0x940007a2, 0x94068255, 0x940fea13}
};
UCHAR	NUM_OF_2528_CHNL = (sizeof(RF2528RegTable) / sizeof(RTMP_RF_REGS));

RTMP_RF_REGS RF5226RegTable[] = {
//		ch	 R1 		 R2 		 R3(TX0~4=0) R4
		{1,  0x94002c0c, 0x94000786, 0x94068255, 0x940fea0b},
		{2,  0x94002c0c, 0x94000786, 0x94068255, 0x940fea1f},
		{3,  0x94002c0c, 0x9400078a, 0x94068255, 0x940fea0b},
		{4,  0x94002c0c, 0x9400078a, 0x94068255, 0x940fea1f},
		{5,  0x94002c0c, 0x9400078e, 0x94068255, 0x940fea0b},
		{6,  0x94002c0c, 0x9400078e, 0x94068255, 0x940fea1f},
		{7,  0x94002c0c, 0x94000792, 0x94068255, 0x940fea0b},
		{8,  0x94002c0c, 0x94000792, 0x94068255, 0x940fea1f},
		{9,  0x94002c0c, 0x94000796, 0x94068255, 0x940fea0b},
		{10, 0x94002c0c, 0x94000796, 0x94068255, 0x940fea1f},
		{11, 0x94002c0c, 0x9400079a, 0x94068255, 0x940fea0b},
		{12, 0x94002c0c, 0x9400079a, 0x94068255, 0x940fea1f},
		{13, 0x94002c0c, 0x9400079e, 0x94068255, 0x940fea0b},
		{14, 0x94002c0c, 0x940007a2, 0x94068255, 0x940fea13},

		{36, 0x94002c0c, 0x9400099a, 0x94098255, 0x940fea23},
		{40, 0x94002c0c, 0x940009a2, 0x94098255, 0x940fea03},
		{44, 0x94002c0c, 0x940009a6, 0x94098255, 0x940fea0b},
		{48, 0x94002c0c, 0x940009aa, 0x94098255, 0x940fea13},
		{52, 0x94002c0c, 0x940009ae, 0x94098255, 0x940fea1b},
		{56, 0x94002c0c, 0x940009b2, 0x94098255, 0x940fea23},
		{60, 0x94002c0c, 0x940009ba, 0x94098255, 0x940fea03},
		{64, 0x94002c0c, 0x940009be, 0x94098255, 0x940fea0b},

		{100, 0x94002c0c, 0x94000a2a, 0x940b8255, 0x940fea03},
		{104, 0x94002c0c, 0x94000a2e, 0x940b8255, 0x940fea0b},
		{108, 0x94002c0c, 0x94000a32, 0x940b8255, 0x940fea13},
		{112, 0x94002c0c, 0x94000a36, 0x940b8255, 0x940fea1b},
		{116, 0x94002c0c, 0x94000a3a, 0x940b8255, 0x940fea23},
		{120, 0x94002c0c, 0x94000a82, 0x940b8255, 0x940fea03},
		{124, 0x94002c0c, 0x94000a86, 0x940b8255, 0x940fea0b},
		{128, 0x94002c0c, 0x94000a8a, 0x940b8255, 0x940fea13},
		{132, 0x94002c0c, 0x94000a8e, 0x940b8255, 0x940fea1b},
		{136, 0x94002c0c, 0x94000a92, 0x940b8255, 0x940fea23},
		{140, 0x94002c0c, 0x94000a9a, 0x940b8255, 0x940fea03},

		{149, 0x94002c0c, 0x94000aa2, 0x940b8255, 0x940fea1f},
		{153, 0x94002c0c, 0x94000aa6, 0x940b8255, 0x940fea27},
		{157, 0x94002c0c, 0x94000aae, 0x940b8255, 0x940fea07},
		{161, 0x94002c0c, 0x94000ab2, 0x940b8255, 0x940fea0f},
		{165, 0x94002c0c, 0x94000ab6, 0x940b8255, 0x940fea17},

		//MMAC(Japan)J52 ch 34,38,42,46
		{34, 0x94002c0c, 0x9408099a, 0x940da255, 0x940d3a0b},
		{38, 0x94002c0c, 0x9408099e, 0x940da255, 0x940d3a13},
		{42, 0x94002c0c, 0x940809a2, 0x940da255, 0x940d3a1b},
		{46, 0x94002c0c, 0x940809a6, 0x940da255, 0x940d3a23},

};
UCHAR	NUM_OF_5226_CHNL = (sizeof(RF5226RegTable) / sizeof(RTMP_RF_REGS));

// Reset the RFIC setting to new series
static RTMP_RF_REGS RF5225RegTable[] = {
//		ch	 R1 		 R2 		 R3(TX0~4=0) R4
		{1,  0x95002ccc, 0x95004786, 0x95068455, 0x950ffa0b},
		{2,  0x95002ccc, 0x95004786, 0x95068455, 0x950ffa1f},
		{3,  0x95002ccc, 0x9500478a, 0x95068455, 0x950ffa0b},
		{4,  0x95002ccc, 0x9500478a, 0x95068455, 0x950ffa1f},
		{5,  0x95002ccc, 0x9500478e, 0x95068455, 0x950ffa0b},
		{6,  0x95002ccc, 0x9500478e, 0x95068455, 0x950ffa1f},
		{7,  0x95002ccc, 0x95004792, 0x95068455, 0x950ffa0b},
		{8,  0x95002ccc, 0x95004792, 0x95068455, 0x950ffa1f},
		{9,  0x95002ccc, 0x95004796, 0x95068455, 0x950ffa0b},
		{10, 0x95002ccc, 0x95004796, 0x95068455, 0x950ffa1f},
		{11, 0x95002ccc, 0x9500479a, 0x95068455, 0x950ffa0b},
		{12, 0x95002ccc, 0x9500479a, 0x95068455, 0x950ffa1f},
		{13, 0x95002ccc, 0x9500479e, 0x95068455, 0x950ffa0b},
		{14, 0x95002ccc, 0x950047a2, 0x95068455, 0x950ffa13},

		// 802.11 UNI / HyperLan 2
		{36, 0x95002ccc, 0x9500499a, 0x9509be55, 0x950ffa23},
		{40, 0x95002ccc, 0x950049a2, 0x9509be55, 0x950ffa03},
		{44, 0x95002ccc, 0x950049a6, 0x9509be55, 0x950ffa0b},
		{48, 0x95002ccc, 0x950049aa, 0x9509be55, 0x950ffa13},
		{52, 0x95002ccc, 0x950049ae, 0x9509ae55, 0x950ffa1b},
		{56, 0x95002ccc, 0x950049b2, 0x9509ae55, 0x950ffa23},
		{60, 0x95002ccc, 0x950049ba, 0x9509ae55, 0x950ffa03},
		{64, 0x95002ccc, 0x950049be, 0x9509ae55, 0x950ffa0b},

		// 802.11 HyperLan 2
		{100, 0x95002ccc, 0x95004a2a, 0x950bae55, 0x950ffa03},
		{104, 0x95002ccc, 0x95004a2e, 0x950bae55, 0x950ffa0b},
		{108, 0x95002ccc, 0x95004a32, 0x950bae55, 0x950ffa13},
		{112, 0x95002ccc, 0x95004a36, 0x950bae55, 0x950ffa1b},
		{116, 0x95002ccc, 0x95004a3a, 0x950bbe55, 0x950ffa23},
		{120, 0x95002ccc, 0x95004a82, 0x950bbe55, 0x950ffa03},
		{124, 0x95002ccc, 0x95004a86, 0x950bbe55, 0x950ffa0b},
		{128, 0x95002ccc, 0x95004a8a, 0x950bbe55, 0x950ffa13},
		{132, 0x95002ccc, 0x95004a8e, 0x950bbe55, 0x950ffa1b},
		{136, 0x95002ccc, 0x95004a92, 0x950bbe55, 0x950ffa23},

		// 802.11 UNII
		{140, 0x95002ccc, 0x95004a9a, 0x950bbe55, 0x950ffa03},
		{149, 0x95002ccc, 0x95004aa2, 0x950bbe55, 0x950ffa1f},
		{153, 0x95002ccc, 0x95004aa6, 0x950bbe55, 0x950ffa27},
		{157, 0x95002ccc, 0x95004aae, 0x950bbe55, 0x950ffa07},
		{161, 0x95002ccc, 0x95004ab2, 0x950bbe55, 0x950ffa0f},
		{165, 0x95002ccc, 0x95004ab6, 0x950bbe55, 0x950ffa17},

		//MMAC(Japan)J52 ch 34,38,42,46
		{34, 0x95002ccc, 0x9500499a, 0x9509be55, 0x950ffa0b},
		{38, 0x95002ccc, 0x9500499e, 0x9509be55, 0x950ffa13},
		{42, 0x95002ccc, 0x950049a2, 0x9509be55, 0x950ffa1b},
		{46, 0x95002ccc, 0x950049a6, 0x9509be55, 0x950ffa23},

};
UCHAR	NUM_OF_5225_CHNL = (sizeof(RF5225RegTable) / sizeof(RTMP_RF_REGS));


/*
	==========================================================================
	Description:
		initialize the MLME task and its data structure (queue, spinlock,
		timer, state machines).

	Return:
		always return NDIS_STATUS_SUCCESS

	==========================================================================
*/
NDIS_STATUS MlmeInit(
	IN PRTMP_ADAPTER pAd)
{
	NDIS_STATUS Status = NDIS_STATUS_SUCCESS;

	DBGPRINT(RT_DEBUG_TRACE, "--> MlmeInit\n");

	do
	{
		pAd->Mlme.Running = FALSE;

		NdisAllocateSpinLock(&pAd->Mlme.Queue.Lock);
		Status = MlmeQueueInit(&pAd->Mlme.Queue);
		if(Status != NDIS_STATUS_SUCCESS)
			break;

		// Initialize Mlme Memory Handler
		// Allocate 20 nonpaged memory pool which size are MAX_LEN_OF_MLME_BUFFER for use
		Status = MlmeInitMemoryHandler(pAd, 20, MAX_LEN_OF_MLME_BUFFER);
		if(Status != NDIS_STATUS_SUCCESS)
		{
			MlmeQueueDestroy(&pAd->Mlme.Queue);
			break;
		}

		// initialize table
		BssTableInit(&pAd->ScanTab);

		// init state machines
		ASSERT(ASSOC_FUNC_SIZE == MAX_ASSOC_MSG * MAX_ASSOC_STATE);
		AssocStateMachineInit(pAd, &pAd->Mlme.AssocMachine, pAd->Mlme.AssocFunc);

		ASSERT(AUTH_FUNC_SIZE == MAX_AUTH_MSG * MAX_AUTH_STATE);
		AuthStateMachineInit(pAd, &pAd->Mlme.AuthMachine, pAd->Mlme.AuthFunc);

		ASSERT(AUTH_RSP_FUNC_SIZE == MAX_AUTH_RSP_MSG * MAX_AUTH_RSP_STATE);
		AuthRspStateMachineInit(pAd, &pAd->Mlme.AuthRspMachine, pAd->Mlme.AuthRspFunc);

		ASSERT(SYNC_FUNC_SIZE == MAX_SYNC_MSG * MAX_SYNC_STATE);
		SyncStateMachineInit(pAd, &pAd->Mlme.SyncMachine, pAd->Mlme.SyncFunc);

		ASSERT(WPA_PSK_FUNC_SIZE == MAX_WPA_PSK_MSG * MAX_WPA_PSK_STATE);
		WpaPskStateMachineInit(pAd, &pAd->Mlme.WpaPskMachine, pAd->Mlme.WpaPskFunc);

		// Since we are using switch/case to implement it, the init is different from the above
		// state machine init
		MlmeCntlInit(pAd, &pAd->Mlme.CntlMachine, NULL);

		// Init mlme periodic timer
		RTMPInitTimer(pAd, &pAd->Mlme.PeriodicTimer, &MlmePeriodicExecTimeout);

		// software-based RX Antenna diversity
		RTMPInitTimer(pAd, &pAd->RxAnt.RxAntDiversityTimer, &AsicRxAntEvalTimeout);

		// Init timer to report link down event
		RTMPInitTimer(pAd, &pAd->Mlme.LinkDownTimer, &LinkDownExec);

	} while (FALSE);

	DBGPRINT(RT_DEBUG_TRACE, "<-- MlmeInit\n");

	return Status;
}

/*
	==========================================================================
	Description:
		main loop of the MLME
	Pre:
		Mlme has to be initialized, and there are something inside the queue
	Note:
		This function is invoked from MPSetInformation and MPReceive;
		This task guarantee only one MlmeHandler will run.
	==========================================================================
 */
VOID MlmeHandler(
	IN PRTMP_ADAPTER pAd)
{
	MLME_QUEUE_ELEM	*Elem = NULL;
	unsigned long			flags;

	// Only accept MLME and Frame from peer side, no other (control/data)
	// frame should get into this state machine

	// We fix the multiple context service drop problem identified by
	// Ben Hutchings in an SMP- safe way by combining TaskLock and Queue.Lock
	// per his suggestion.
	NdisAcquireSpinLock(&pAd->Mlme.Queue.Lock);
	if(pAd->Mlme.Running)
	{
		NdisReleaseSpinLock(&pAd->Mlme.Queue.Lock);
		return;
	}
	pAd->Mlme.Running = TRUE;

	// If there's a bubble, wait for it to collapse before proceeding.
    while (MlmeGetHead(&pAd->Mlme.Queue, &Elem)) {
		smp_read_barrier_depends();
		if (!Elem->Occupied) break;

		NdisReleaseSpinLock(&pAd->Mlme.Queue.Lock);

		if (Elem->MsgType == RT_CMD_RESET_MLME)
		{
			DBGPRINT(RT_DEBUG_TRACE, "-  %s: reset MLME state machine !!!\n",
					__FUNCTION__);
			MlmeRestartStateMachine(pAd);
			MlmePostRestartStateMachine(pAd);
		}
        //From message type, determine which state machine I should drive
		else if (pAd->PortCfg.BssType != BSS_MONITOR) switch (Elem->Machine)
		{
			case ASSOC_STATE_MACHINE:
				StateMachinePerformAction(pAd, &pAd->Mlme.AssocMachine, Elem);
				break;
			case AUTH_STATE_MACHINE:
				StateMachinePerformAction(pAd, &pAd->Mlme.AuthMachine, Elem);
				break;
			case AUTH_RSP_STATE_MACHINE:
				StateMachinePerformAction(pAd, &pAd->Mlme.AuthRspMachine, Elem);
				break;
			case SYNC_STATE_MACHINE:
				StateMachinePerformAction(pAd, &pAd->Mlme.SyncMachine, Elem);
				break;
			case MLME_CNTL_STATE_MACHINE:
				MlmeCntlMachinePerformAction(pAd, &pAd->Mlme.CntlMachine, Elem);
				break;
			case WPA_PSK_STATE_MACHINE:
				StateMachinePerformAction(pAd, &pAd->Mlme.WpaPskMachine, Elem);
				break;
			default:
				DBGPRINT(RT_DEBUG_ERROR, "ERROR: Illegal machine in MlmeHandler()\n");
				break;
		} // end of switch

		// free MLME element
        smp_mb();
        Elem->Occupied = FALSE;	// sic - bb
		NdisAcquireSpinLock(&pAd->Mlme.Queue.Lock);
		MlmeDequeue(&pAd->Mlme.Queue);
	}
	pAd->Mlme.Running = FALSE;
	NdisReleaseSpinLock(&pAd->Mlme.Queue.Lock);
}

static void cancelSMTimers(
	IN PRTMP_ADAPTER pAd)
{
	RTMPCancelTimer(&pAd->Mlme.PeriodicTimer);
	RTMPCancelTimer(&pAd->Mlme.LinkDownTimer);
	RTMPCancelTimer(&pAd->MlmeAux.AssocTimer);
	RTMPCancelTimer(&pAd->MlmeAux.ReassocTimer);
	RTMPCancelTimer(&pAd->MlmeAux.AuthTimer);
	RTMPCancelTimer(&pAd->MlmeAux.BeaconTimer);
	RTMPCancelTimer(&pAd->MlmeAux.ScanTimer);
    RTMPCancelTimer(&pAd->PortCfg.QuickResponeForRateUpTimer);
	RTMPCancelTimer(&pAd->RxAnt.RxAntDiversityTimer);

} /* End cancelSMTimers () */

VOID MlmeStart(
	IN PRTMP_ADAPTER pAd)
{
	// Set mlme periodic timer
	RTMPSetTimer(pAd, &pAd->Mlme.PeriodicTimer, MLME_TASK_EXEC_INTV);
}

/*
	==========================================================================
	Description:
		Destructor of MLME (Destroy queue, state machine, spin lock and timer)
	Parameters:
		Adapter - NIC Adapter pointer
	Post:
		The MLME task will no longer work properly

	==========================================================================
 */
VOID MlmeHalt(
	IN PRTMP_ADAPTER pAd)
{

	DBGPRINT(RT_DEBUG_TRACE, "==> MlmeHalt\n");

	MlmeRestartStateMachine(pAd);

	// Cancel pending timers
	cancelSMTimers(pAd);

	msleep(500); //RTMPusecDelay(500000); // 0.5 sec to guarantee timer canceled

	RTUSBwaitTxDone(pAd);

	// We want to wait until all pending receives and sends to the
	// device object. We cancel any
	// irps. Wait until sends and receives have stopped.
	//
	RTUSBCancelPendingIRPs(pAd);
	RTUSBCleanUpMLMEWaitQueue(pAd);
	RTUSBCleanUpMLMEBulkOutQueue(pAd);

	RTUSBfreeCmdQ(pAd, &pAd->CmdQ);

	MlmeQueueInit(&pAd->Mlme.Queue);
	BssTableInit(&pAd->ScanTab);	// no scan resuls when closed - bb

	DBGPRINT(RT_DEBUG_TRACE, "<== MlmeHalt\n");
}

VOID MlmeSuspend(
	IN PRTMP_ADAPTER pAd,
	IN BOOLEAN linkdown)
{
	MLME_QUEUE_ELEM		*Elem = NULL;
	unsigned long				flags;

	DBGPRINT(RT_DEBUG_TRACE, "==>MlmeSuspend\n");

	// Cancel pending timers
	cancelSMTimers(pAd);

	while (TRUE)
	{
		NdisAcquireSpinLock(&pAd->Mlme.Queue.Lock);
		if(pAd->Mlme.Running)
		{
			NdisReleaseSpinLock(&pAd->Mlme.Queue.Lock);
			if (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_HALT_IN_PROGRESS))
				return;

			RTMPusecDelay(100);
		}
		else
		{
			pAd->Mlme.Running = TRUE;
			NdisReleaseSpinLock(&pAd->Mlme.Queue.Lock);
			break;
		}
	}

	// Remove all Mlme queues elements
	NdisAcquireSpinLock(&pAd->Mlme.Queue.Lock);
	while (MlmeGetHead(&pAd->Mlme.Queue, &Elem)) {
		// free MLME element
		MlmeDequeue(&pAd->Mlme.Queue);
		Elem->Occupied = FALSE;
	}
	NdisReleaseSpinLock(&pAd->Mlme.Queue.Lock);
	RTUSBwaitTxDone(pAd);

	RTUSBCancelPendingIRPs(pAd);
	RTUSBCleanUpDataBulkInQueue(pAd);
	RTUSBCleanUpDataBulkOutQueue(pAd);
	RTUSBCleanUpMLMEBulkOutQueue(pAd);
	RTUSBCleanUpMLMEWaitQueue(pAd);
	RTUSBfreeCmdQ(pAd, &pAd->CmdQ);

	// Set all state machines back IDLE
	pAd->Mlme.CntlMachine.CurrState    = CNTL_IDLE;
	pAd->Mlme.AssocMachine.CurrState   = ASSOC_IDLE;
	pAd->Mlme.AuthMachine.CurrState    = AUTH_REQ_IDLE;
	pAd->Mlme.AuthRspMachine.CurrState = AUTH_RSP_IDLE;
	pAd->Mlme.SyncMachine.CurrState    = SYNC_IDLE;

	// Remove running state
	NdisAcquireSpinLock(&pAd->Mlme.Queue.Lock);
	pAd->Mlme.Running = FALSE;
	NdisReleaseSpinLock(&pAd->Mlme.Queue.Lock);

	DBGPRINT(RT_DEBUG_TRACE, "<==MlmeSuspend\n");
}

VOID MlmeResume(
	IN	PRTMP_ADAPTER	pAd)
{
	DBGPRINT(RT_DEBUG_TRACE, "==>MlmeResume\n");

	// Set all state machines back IDLE
	pAd->Mlme.CntlMachine.CurrState    = CNTL_IDLE;
	pAd->Mlme.AssocMachine.CurrState   = ASSOC_IDLE;
	pAd->Mlme.AuthMachine.CurrState    = AUTH_REQ_IDLE;
	pAd->Mlme.AuthRspMachine.CurrState = AUTH_RSP_IDLE;
	pAd->Mlme.SyncMachine.CurrState    = SYNC_IDLE;

	// If previously associated, reassociate on resume - bb.
	if (pAd->MlmeAux.SsidLen > 0) {
		memcpy(pAd->MlmeAux.AutoReconnectSsid, pAd->MlmeAux.Ssid,
				pAd->MlmeAux.SsidLen);
		pAd->MlmeAux.AutoReconnectSsidLen = pAd->MlmeAux.SsidLen;
	}
	MlmePeriodicExec(pAd);

	DBGPRINT(RT_DEBUG_TRACE, "<==MlmeResume\n");
}

/*
    ==========================================================================
    Description:
        This routine is executed periodically to -
        1. Decide if it's a right time to turn on PwrMgmt bit of all
           outgoiing frames
        2. Calculate ChannelQuality based on statistics of the last
           period, so that TX rate won't toggling very frequently between a
           successful TX and a failed TX.
        3. If the calculated ChannelQuality indicated current connection not
           healthy, then a ROAMing attempt is tried here.

    ==========================================================================
 */

#define ADHOC_BEACON_LOST_TIME		(10*HZ)  // 4 sec

// interrupt context (timer handler)
VOID MlmePeriodicExecTimeout(
	IN	unsigned long data)
{
	RTMP_ADAPTER	*pAd = (RTMP_ADAPTER *)data;
	RTUSBEnqueueInternalCmd(pAd, RT_OID_PERIODIC_EXECUT);
}

// process context
VOID MlmePeriodicExec(
	IN	PRTMP_ADAPTER pAd)
{
	// Timer need to reset every time, so using do-while loop
	do
	{
		if (pAd->PortCfg.BssType == BSS_MONITOR)
		{
			DBGPRINT(RT_DEBUG_TRACE, "==> MlmePeriodicExec Monitor Mode\n");
			break;
		}
		DBGPRINT(RT_DEBUG_TRACE, "==> MlmePeriodicExec\n");

		if (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_BSS_SCAN_IN_PROGRESS))
		{
			DBGPRINT(RT_DEBUG_INFO, "- MlmePeriodicExec scan active\n");
			break;
		}
		if ((pAd->PortCfg.bHardwareRadio == TRUE) &&
			(!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST)) &&
			(!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_HALT_IN_PROGRESS)))
		{
			RTUSBEnqueueInternalCmd(pAd, RT_OID_CHECK_GPIO);
		}

		// Do nothing if the driver is starting halt state.
		// This might happen when timer already been fired before cancel timer with mlmehalt
		if ((RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_HALT_IN_PROGRESS)) ||
			(RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_RADIO_OFF)) ||
			(RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_RADIO_MEASUREMENT)) ||
			(RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_RESET_IN_PROGRESS))) {
			DBGPRINT(RT_DEBUG_INFO, "- MlmePeriodicExec Hlt or Radio Off\n");
			break;
		}
		if (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_MEDIA_STATE_PENDING))
		{
			RTMP_CLEAR_FLAG(pAd, fRTMP_ADAPTER_MEDIA_STATE_PENDING);
			DBGPRINT(RT_DEBUG_INFO, "NDIS_STATUS_MEDIA_DISCONNECT Event B!\n");
		}

		//
		// hardware failure?
		//
		if (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST))
		{
			DBGPRINT(RT_DEBUG_INFO, "- MlmePeriodicExec NIC not exist\n");
			RTUSBRejectPendingPackets(pAd);
			break;
		}

		pAd->Mlme.Now = jiffies;

		// if MGMT RING is full more than twice within 1 second, we consider there's
		// a hardware problem stucking the TX path. In this case, try a hardware reset
		// to recover the system
		if (pAd->RalinkCounters.MgmtRingFullCount >= 2)
		{
			RTUSBfreeCmdQ(pAd, &pAd->CmdQ);
			RTUSBEnqueueInternalCmd(pAd, RT_OID_RESET_FROM_ERROR);

			DBGPRINT(RT_DEBUG_ERROR, "<---MlmePeriodicExec (Mgmt Ring Full)\n");
			break;
		}
		pAd->RalinkCounters.MgmtRingFullCount = 0;

		DBGPRINT(RT_DEBUG_INFO, "- MlmePeriodicExec call STAExec\n");
		STAMlmePeriodicExec(pAd);

	} while (0);

	if ((!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_RESET_IN_PROGRESS)) &&
		(!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_HALT_IN_PROGRESS)) &&
		(!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST)))
	{
		/* Only start the next period if we haven't been told to close while
		 * waiting for register I/O to complete. Otherwise, we can get into
		 * trouble when we (redundantly) start the timer on the next open.
		 * - bb */
		if (netif_running(pAd->net_dev)) {
			RTMPSetTimer(pAd, &pAd->Mlme.PeriodicTimer, MLME_TASK_EXEC_INTV);
		}
	}

	DBGPRINT(RT_DEBUG_TRACE, "<== MlmePeriodicExec\n");
}

// process context
VOID STAMlmePeriodicExec(
	IN	PRTMP_ADAPTER pAd)
{
	TXRX_CSR4_STRUC *CurTxRxCsr4 = kzalloc(sizeof(CurTxRxCsr4), GFP_KERNEL);
	SHORT	dbm;
#if WPA_SUPPLICANT_SUPPORT
    union iwreq_data wrqu;
#endif

	if (!CurTxRxCsr4) {
		DBGPRINT(RT_DEBUG_ERROR, "couldn't allocate memory\n");
		return;
	}

	// WPA MIC error should block association attempt for 60 seconds
	if (pAd->PortCfg.bBlockAssoc &&
		(time_after(pAd->Mlme.Now, pAd->PortCfg.LastMicErrorTime + (60*HZ)))) {
		pAd->PortCfg.bBlockAssoc = FALSE;
	}
	DBGPRINT(RT_DEBUG_INFO, "MMCHK - PortCfg.Ssid=%s ... MlmeAux.Ssid=%s\n",
			pAd->PortCfg.Ssid, pAd->MlmeAux.Ssid);

	// add the most up-to-date h/w raw counters into software variable, so that
	// the dynamic tuning mechanism below are based on most up-to-date information
	NICUpdateRawCounters(pAd);

	// danamic tune BBP R17 to find a balance between sensibility and noise isolation
	AsicBbpTuning(pAd);

	if (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_MEDIA_STATE_CONNECTED))
	{
		// update channel quality for Roaming and UI LinkQuality display
		MlmeCalculateChannelQuality(pAd, pAd->Mlme.Now);

		// perform dynamic tx rate switching based on past TX history
		MlmeDynamicTxRateSwitching(pAd);
	}

	// must be AFTER MlmeDynamicTxRateSwitching() because it needs to know if
	// Radio is currently in noisy environment
	AsicAdjustTxPower(pAd);

	// if Rx Antenna is DIVERSITY ON, then perform Software-based diversity evaluation
	if (((pAd->Antenna.field.NumOfAntenna == 2) && (pAd->Antenna.field.TxDefaultAntenna == 0) && (pAd->Antenna.field.RxDefaultAntenna == 0))
		&& (pAd->Mlme.PeriodicRound % 2 == 1))
	{
		// check every 2 second. If rcv-beacon less than 5 in the past 2 second, then AvgRSSI is no longer a
		// valid indication of the distance between this AP and its clients.
		if (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_MEDIA_STATE_CONNECTED))
		{
			SHORT	realavgrssi1;
			BOOLEAN	evaluate = FALSE;

			if (pAd->PortCfg.NumOfAvgRssiSample < 5)
			{
				pAd->RxAnt.Pair1LastAvgRssi = (-115);
				pAd->RxAnt.Pair2LastAvgRssi = (-115);
				DBGPRINT(RT_DEBUG_TRACE, "MlmePeriodicExec: no traffic/beacon, reset RSSI\n");
			}
			else
				pAd->PortCfg.NumOfAvgRssiSample = 0;

			realavgrssi1 = (pAd->RxAnt.Pair1AvgRssi[pAd->RxAnt.Pair1PrimaryRxAnt] >> 3);

			DBGPRINT(RT_DEBUG_INFO,"Ant-realrssi0(%d),Lastrssi0(%d)\n",realavgrssi1, pAd->RxAnt.Pair1LastAvgRssi);

			if ((realavgrssi1 > (pAd->RxAnt.Pair1LastAvgRssi + 5)) || (realavgrssi1 < (pAd->RxAnt.Pair1LastAvgRssi - 5)))
			{
				evaluate = TRUE;
				pAd->RxAnt.Pair1LastAvgRssi = realavgrssi1;
			}

			if (evaluate == TRUE)
			{
				AsicEvaluateSecondaryRxAnt(pAd);
			}
		}
		else
		{
			UCHAR	temp;

			temp = pAd->RxAnt.Pair1PrimaryRxAnt;
			pAd->RxAnt.Pair1PrimaryRxAnt = pAd->RxAnt.Pair1SecondaryRxAnt;
			pAd->RxAnt.Pair1SecondaryRxAnt = temp;
			AsicSetRxAnt(pAd, pAd->RxAnt.Pair1PrimaryRxAnt, 0xFF);	//0xFF means not used.
		}
	}

	// G band - set BBP_R62 to 0x02 when site survey or rssi<-82
	// A band - always set BBP_R62 to 0x04
	if ((pAd->Mlme.SyncMachine.CurrState == SYNC_IDLE) && (pAd->PortCfg.Channel <= 14))
	{
		if (pAd->PortCfg.LastRssi >= (-82 + pAd->BbpRssiToDbmDelta))
		{
			RTUSBWriteBBPRegister(pAd, BBP_R62, 0x04);
		}
		else
		{
			RTUSBWriteBBPRegister(pAd, BBP_R62, 0x02);
		}
		DBGPRINT(RT_DEBUG_INFO, "STAMlmePeriodicExec - LastRssi=%d, BbpRssiToDbmDelta=%d\n", pAd->PortCfg.LastRssi, pAd->BbpRssiToDbmDelta);
	}

	if (INFRA_ON(pAd))
	{
		// Is PSM bit consistent with user power management policy?
		// This is the only place that will set PSM bit ON.
		MlmeCheckPsmChange(pAd, pAd->Mlme.Now);

		//
		// Lost Beacon for almost one sec && no data traffic then set R17 to lowbound.
		//
		if (INFRA_ON(pAd) &&
			time_after(pAd->Mlme.Now, pAd->PortCfg.LastBeaconRxTime + 1 * HZ) &&
			((pAd->BulkInDataOneSecCount + pAd->BulkOutDataOneSecCount) < 600))
		{
			if (pAd->PortCfg.Channel <= 14)
			{
				RTUSBWriteBBPRegister(pAd, BBP_R17, pAd->BbpTuning.R17LowerBoundG);
			}
			else
			{
				RTUSBWriteBBPRegister(pAd, BBP_R17, pAd->BbpTuning.R17LowerBoundA);
			}
		}

#if 0
		//Move the flowing code to RTUSBHardTransmit, after prepare EAPOL frame that idicated MIC error
		//To meet the WiFi Test Plan "The STAUT must deauthenticate itself from the AP".
		//Also need to bloc association frame.

		//The flowing code on here can't send disassociation frame to the AP, since AP may send deauthentication
		//	frame, on the meanwhile, station will call link down and then this code can't be performed.

		// Check for EAPOL frame sent after MIC countermeasures
		if (pAd->PortCfg.MicErrCnt >= 3)
		{

			MLME_DISASSOC_REQ_STRUCT	DisassocReq;

			// disassoc from current AP first
			DBGPRINT(RT_DEBUG_TRACE, ("MLME - disassociate with current AP after sending second continuous EAPOL frame\n"));
			DisassocParmFill(pAd, &DisassocReq, pAd->PortCfg.Bssid, REASON_MIC_FAILURE);
			MlmeEnqueue(pAd, ASSOC_STATE_MACHINE, MT2_MLME_DISASSOC_REQ,
						sizeof(MLME_DISASSOC_REQ_STRUCT), &DisassocReq);

			pAd->Mlme.CntlMachine.CurrState = CNTL_WAIT_DISASSOC;
			pAd->PortCfg.bBlockAssoc = TRUE;
		}
#endif
        // send out a NULL frame every 10 sec. for what??? inform "PwrMgmt" bit?
        if ((pAd->Mlme.PeriodicRound % 10) == 8)
            RTMPSendNullFrame(pAd, pAd->PortCfg.TxRate);

        if (CQI_IS_DEAD(pAd->Mlme.ChannelQuality))
        {
            DBGPRINT(RT_DEBUG_TRACE, "MMCHK - No BEACON. Dead CQI. Auto Recovery attempt #%d\n", pAd->RalinkCounters.BadCQIAutoRecoveryCount);

#if WPA_SUPPLICANT_SUPPORT
            if (pAd->PortCfg.WPA_Supplicant == TRUE) {
                // send disassoc event to wpa_supplicant
               memset(&wrqu, 0, sizeof(wrqu));
               wrqu.data.flags = RT_DISASSOC_EVENT_FLAG;
               wireless_send_event(pAd->net_dev, IWEVCUSTOM, &wrqu, NULL);
            }
#endif

            // Lost AP, send disconnect & link down event
            RTUSBEnqueueInternalCmd(pAd, RT_OID_LINK_DOWN);

            // RTMPPatchMacBbpBug(pAd);
            MlmeAutoReconnectLastSSID(pAd);
        }
        else if (CQI_IS_BAD(pAd->Mlme.ChannelQuality))
        {
            pAd->RalinkCounters.BadCQIAutoRecoveryCount ++;
            DBGPRINT(RT_DEBUG_TRACE, "MMCHK - Bad CQI. Auto Recovery attempt #%d\n", pAd->RalinkCounters.BadCQIAutoRecoveryCount);
            MlmeAutoReconnectLastSSID(pAd);
        }
// TODO: temp removed
#if 0
        else if (CQI_IS_POOR(pAd->Mlme.ChannelQuality))
        {
            // perform aggresive roaming only when SECURITY OFF or WEP64/128;
            // WPA and WPA-PSK has no aggresive roaming because re-negotiation
            // between 802.1x supplicant and authenticator/AAA server is required
            // but can't be guaranteed.
            if (pAd->PortCfg.AuthMode < Ndis802_11AuthModeWPA)
                MlmeCheckForRoaming(pAd, pAd->Mlme.Now);
        }
#endif
        // fast roaming
        if (pAd->PortCfg.bFastRoaming)
        {
            // Check the RSSI value, we should begin the roaming attempt
            DBGPRINT(RT_DEBUG_TRACE, "RxSignal %d\n", pAd->PortCfg.LastRssi - pAd->BbpRssiToDbmDelta);
            // Only perform action when signal is less than or equal to setting from the UI or registry
            if (pAd->PortCfg.LastRssi <= (pAd->BbpRssiToDbmDelta - pAd->PortCfg.dBmToRoam))
            {
                MlmeCheckForFastRoaming(pAd, pAd->Mlme.Now);
            }
        }
    }

	// !!! Regard the IBSS network as established one while both ADHOC_ON and fOP_STATUS_MEDIA_STATE_CONNECTED are TRUE !!!
#ifndef SINGLE_ADHOC_LINKUP
    else if (ADHOC_ON(pAd) && OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_MEDIA_STATE_CONNECTED))
    {
		// If all peers leave, and this STA becomes the last one in this IBSS, then change MediaState
		// to DISCONNECTED. But still holding this IBSS (i.e. sending BEACON) so that other STAs can
		// join later.
		if (time_after(pAd->Mlme.Now,pAd->PortCfg.LastBeaconRxTime + ADHOC_BEACON_LOST_TIME))
		{
            DBGPRINT(RT_DEBUG_TRACE, "MMCHK - excessive BEACON lost, last STA in this IBSS, MediaState=Disconnected\n");

			OPSTATUS_CLEAR_FLAG(pAd, fOP_STATUS_MEDIA_STATE_CONNECTED);
			// clean up previous SCAN result, add current BSS back to table if any
			BssTableDeleteEntry(&pAd->ScanTab, pAd->PortCfg.Bssid, pAd->PortCfg.Channel);

			pAd->PortCfg.LastScanTime = pAd->Mlme.Now;
        }
    }
#endif
	else // no INFRA nor ADHOC connection
	{
		if (!ADHOC_ON(pAd) && !INFRA_ON(pAd))
		{
			DBGPRINT(RT_DEBUG_INFO, "-  %s, no association so far\n",
					__FUNCTION__);
			if ((pAd->PortCfg.bAutoReconnect == TRUE) &&
				(MlmeValidateSSID(pAd->MlmeAux.AutoReconnectSsid, pAd->MlmeAux.AutoReconnectSsidLen) == TRUE))
			{
				if ((pAd->ScanTab.BssNr==0) && (pAd->Mlme.CntlMachine.CurrState == CNTL_IDLE))
				{
					MLME_SCAN_REQ_STRUCT	   ScanReq;

					if (time_after(pAd->Mlme.Now, pAd->PortCfg.LastScanTime + 10 * HZ))
					{
						DBGPRINT(RT_DEBUG_TRACE, "CNTL - No matching BSS, start a new ACTIVE scan SSID[%s]\n", pAd->MlmeAux.AutoReconnectSsid);
						ScanParmFill(pAd, &ScanReq, pAd->MlmeAux.AutoReconnectSsid, pAd->MlmeAux.AutoReconnectSsidLen, BSS_ANY, SCAN_ACTIVE);
						MlmeEnqueue(pAd, SYNC_STATE_MACHINE, MT2_MLME_SCAN_REQ,
								sizeof(MLME_SCAN_REQ_STRUCT), &ScanReq);
						pAd->Mlme.CntlMachine.CurrState = CNTL_WAIT_OID_LIST_SCAN;
						RTUSBMlmeUp(pAd);

						// Reset Missed scan number
						pAd->PortCfg.LastScanTime = pAd->Mlme.Now;
					}
					else if (pAd->PortCfg.BssType == BSS_ADHOC)  // Quit the forever scan when in a very clean room
					{
						MlmeAutoReconnectLastSSID(pAd);
					}
				}
				else if (pAd->Mlme.CntlMachine.CurrState == CNTL_IDLE)
				{
					if ((pAd->Mlme.PeriodicRound % 20) == 8)
					{
						MlmeAutoScan(pAd);
						pAd->PortCfg.LastScanTime = pAd->Mlme.Now;
					}
					else if ((pAd->Mlme.PeriodicRound % 2) == 0)
					{
						MlmeAutoReconnectLastSSID(pAd);
					}
					DBGPRINT(RT_DEBUG_INFO, "pAd->PortCfg.bAutoReconnect is TRUE\n");
				}

				// after once scanning, set wpanone-psk key in the case of using configuration file
				//if (pAd->Mlme.PeriodicRound >= 4)
				//{
					if ((pAd->PortCfg.BssType == BSS_ADHOC) &&
						(pAd->PortCfg.AuthMode == Ndis802_11AuthModeWPANone)&&
						((pAd->PortCfg.WepStatus == Ndis802_11Encryption2Enabled) ||
						(pAd->PortCfg.WepStatus == Ndis802_11Encryption3Enabled)) &&
						(pAd->PortCfg.WpaState == SS_START))
					{
						RTMPWPANoneAddKeyProc(pAd, pAd->PortCfg.PskKey.Key);
						// turn on the flag of PortCfg.WpaState as reading profile
						// and reset after adding key
						pAd->PortCfg.WpaState = SS_NOTUSE;
					}
				//}
			}
		}
	}

	// if all 11b peers leave this BSS more than 5 seconds, update Tx rate,
	// restore outgoing BEACON to support B/G-mixed mode
	if (ADHOC_ON(pAd))
	{
		// 1.	2003-04-17 john. this is a patch that driver forces a BEACON out if ASIC fails
		//		the "TX BEACON competition" for the entire past 1 sec.
		//		So that even when ASIC's BEACONgen engine been blocked
		//		by peer's BEACON due to slower system clock, this STA still can send out
		//		minimum BEACON to tell the peer I'm alive.
		//		drawback is that this BEACON won't be well aligned at TBTT boundary.
		// 2.	avoid mlme-queue full while doing radar detection
		if ((pAd->PortCfg.bIEEE80211H == 0) || (pAd->PortCfg.RadarDetect.RDMode == RD_NORMAL_MODE))
			EnqueueBeaconFrame(pAd);			  // software send BEACON

		if ((pAd->PortCfg.Channel <= 14)			 &&
			(pAd->PortCfg.MaxTxRate <= RATE_11) 	 &&
			(pAd->PortCfg.MaxDesiredRate > RATE_11)  &&
			(time_after(pAd->Mlme.Now, pAd->PortCfg.Last11bBeaconRxTime + 5 * HZ)))
		{
			DBGPRINT(RT_DEBUG_TRACE, "MMCHK - last 11B peer left, update Tx rates\n");

			memcpy(pAd->ActiveCfg.SupRate, pAd->PortCfg.SupRate, MAX_LEN_OF_SUPPORTED_RATES);
			pAd->ActiveCfg.SupRateLen = pAd->PortCfg.SupRateLen;

			RTUSBEnqueueInternalCmd(pAd, RT_OID_UPDATE_TX_RATE);	//MlmeUpdateTxRates(pAd, FALSE);
			AsicEnableIbssSync(pAd);	// copy to on-chip memory
		}


		//radar detect
		if (((pAd->PortCfg.PhyMode == PHY_11A) || (pAd->PortCfg.PhyMode == PHY_11ABG_MIXED)) && (pAd->PortCfg.bIEEE80211H == 1) && RadarChannelCheck(pAd, pAd->PortCfg.Channel))
		{
			// need to check channel availability, after switch channel
			if (pAd->PortCfg.RadarDetect.RDMode == RD_SILENCE_MODE)
			{
				pAd->PortCfg.RadarDetect.RDCount++;

				// channel availability check time is 60sec
				if (pAd->PortCfg.RadarDetect.RDCount > 65)
				{
					if (RadarDetectionStop(pAd))
					{
						pAd->ExtraInfo = DETECT_RADAR_SIGNAL;
						pAd->PortCfg.RadarDetect.RDCount = 0;		// stat at silence mode and detect radar signal
						DBGPRINT(RT_DEBUG_TRACE, "Found radar signal!!!\n\n");
					}
					else
					{
						DBGPRINT(RT_DEBUG_TRACE, "Not found radar signal, start send beacon\n");
						AsicEnableIbssSync(pAd);
						pAd->PortCfg.RadarDetect.RDMode = RD_NORMAL_MODE;
					}
				}
			}
		}

	}


	if (((pAd->Mlme.PeriodicRound % 2) == 0) &&
		(INFRA_ON(pAd) || ADHOC_ON(pAd)))
	{
		RTMPSetSignalLED(pAd, pAd->PortCfg.LastRssi - pAd->BbpRssiToDbmDelta);
	}

	if (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_MEDIA_STATE_CONNECTED))
	{
		//
		// Modify retry times (maximum 15) on low data traffic.
		// Should fix ping lost.
		//
		dbm = pAd->PortCfg.AvgRssi - pAd->BbpRssiToDbmDelta;

		//
		// Only on infrastructure mode will change the RetryLimit.
		//
		if (INFRA_ON(pAd))
		{
			if (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_MAX_RETRY_ENABLED))
			{
				if (pAd->RalinkCounters.OneSecTxNoRetryOkCount > 15)
				{
					RTUSBReadMACRegister(pAd,
						TXRX_CSR4,
						&CurTxRxCsr4->word);
					CurTxRxCsr4->field.ShortRetryLimit
						 = 0x07;
					CurTxRxCsr4->field.LongRetryLimit
						= 0x04;
					RTUSBWriteMACRegister(pAd,
							TXRX_CSR4,
							CurTxRxCsr4->word);
					OPSTATUS_CLEAR_FLAG(pAd, fOP_STATUS_MAX_RETRY_ENABLED);
				}
			}
			else
			{
				if (pAd->RalinkCounters.OneSecTxNoRetryOkCount <= 15)
				{
					RTUSBReadMACRegister(pAd,
						TXRX_CSR4,
						&CurTxRxCsr4->word);
					CurTxRxCsr4->field.ShortRetryLimit
						= 0x0f;
					CurTxRxCsr4->field.LongRetryLimit =
						 0x0f;
					RTUSBWriteMACRegister(pAd,
							TXRX_CSR4,
							CurTxRxCsr4->word);
					OPSTATUS_SET_FLAG(pAd, fOP_STATUS_MAX_RETRY_ENABLED);
				}
			}
		}

		if (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_RTS_PROTECTION_ENABLE))
		{
			if ((dbm > -60) || (pAd->RalinkCounters.OneSecTxNoRetryOkCount > 15))
				OPSTATUS_CLEAR_FLAG(pAd, fOP_STATUS_RTS_PROTECTION_ENABLE);
		}
		else
		{
			//
			// for long distance case, turn on RTS to protect data frame.
			//
			if ((dbm <= -60) && (pAd->RalinkCounters.OneSecTxNoRetryOkCount <= 15))
			{
				OPSTATUS_SET_FLAG(pAd, fOP_STATUS_RTS_PROTECTION_ENABLE);
			}
		}
	}

	//
	// Clear Tx/Rx Traffic one second count.
	//
	pAd->BulkLastOneSecCount = pAd->BulkOutDataOneSecCount + pAd->BulkInDataOneSecCount;
	pAd->BulkOutDataOneSecCount = 0;
	pAd->BulkInDataOneSecCount = 0;

	// clear all OneSecxxx counters.
	pAd->RalinkCounters.OneSecBeaconSentCnt = 0;
	pAd->RalinkCounters.OneSecFalseCCACnt = 0;
	pAd->RalinkCounters.OneSecRxFcsErrCnt = 0;
	pAd->RalinkCounters.OneSecRxOkCnt = 0;
	pAd->RalinkCounters.OneSecTxFailCount = 0;
	pAd->RalinkCounters.OneSecTxNoRetryOkCount = 0;
	pAd->RalinkCounters.OneSecTxRetryOkCount = 0;

	// TODO: for debug only. to be removed
	pAd->RalinkCounters.OneSecOsTxCount[QID_AC_BE] = 0;
	pAd->RalinkCounters.OneSecOsTxCount[QID_AC_BK] = 0;
	pAd->RalinkCounters.OneSecOsTxCount[QID_AC_VI] = 0;
	pAd->RalinkCounters.OneSecOsTxCount[QID_AC_VO] = 0;
	pAd->RalinkCounters.OneSecDmaDoneCount[QID_AC_BE] = 0;
	pAd->RalinkCounters.OneSecDmaDoneCount[QID_AC_BK] = 0;
	pAd->RalinkCounters.OneSecDmaDoneCount[QID_AC_VI] = 0;
	pAd->RalinkCounters.OneSecDmaDoneCount[QID_AC_VO] = 0;
	pAd->RalinkCounters.OneSecTxDoneCount = 0;
	pAd->RalinkCounters.OneSecTxAggregationCount = 0;
	pAd->RalinkCounters.OneSecRxAggregationCount = 0;

	pAd->Mlme.PeriodicRound ++;
	kfree(CurTxRxCsr4);
}

VOID LinkDownExec(
	IN	unsigned long data)
{
	//RTMP_ADAPTER *pAd = (RTMP_ADAPTER *)data;
}

VOID MlmeAutoScan(
	IN PRTMP_ADAPTER pAd)
{
	// check CntlMachine.CurrState to avoid collision with NDIS SetOID request

	// tell CNTL state machine NOT to call NdisMSetInformationComplete() after completing
	// this request, because this request is initiated by driver itself.
	pAd->MlmeAux.CurrReqIsFromNdis = FALSE;

	if (pAd->Mlme.CntlMachine.CurrState == CNTL_IDLE)
	{
		DBGPRINT(RT_DEBUG_TRACE, "MMCHK - Driver auto scan\n");
		MlmeEnqueue(pAd,
					MLME_CNTL_STATE_MACHINE,
					OID_802_11_BSSID_LIST_SCAN,
					0,
					NULL);
		RTUSBMlmeUp(pAd);
	}
}

VOID MlmeAutoRecoverNetwork(
	IN PRTMP_ADAPTER pAd)
{
	// check CntlMachine.CurrState to avoid collision with NDIS SetOID request
	if (pAd->Mlme.CntlMachine.CurrState == CNTL_IDLE)
	{
		NDIS_802_11_SSID OidSsid;
		OidSsid.SsidLength = pAd->PortCfg.SsidLen;
		memcpy(OidSsid.Ssid, pAd->PortCfg.Ssid, pAd->PortCfg.SsidLen);

		DBGPRINT(RT_DEBUG_TRACE, "MMCHK - Driver auto recovering network - %s\n", pAd->PortCfg.Ssid);

		// tell CNTL state machine NOT to call NdisMSetInformationComplete() after completing
		// this request, because this request is initiated by driver itself.
		pAd->MlmeAux.CurrReqIsFromNdis = FALSE;

		MlmeEnqueue(pAd,
					MLME_CNTL_STATE_MACHINE,
					OID_802_11_SSID,
					sizeof(NDIS_802_11_SSID),
					&OidSsid);
	   RTUSBMlmeUp(pAd);
	}

}

VOID MlmeAutoReconnectLastSSID(
	IN PRTMP_ADAPTER pAd)
{
	// check CntlMachine.CurrState to avoid collision with NDIS SetOID request
	if (pAd->Mlme.CntlMachine.CurrState == CNTL_IDLE &&
		(MlmeValidateSSID(pAd->MlmeAux.AutoReconnectSsid, pAd->MlmeAux.AutoReconnectSsidLen) == TRUE))
	{
		NDIS_802_11_SSID OidSsid;
		OidSsid.SsidLength = pAd->MlmeAux.AutoReconnectSsidLen;
		memcpy(OidSsid.Ssid, pAd->MlmeAux.AutoReconnectSsid, pAd->MlmeAux.AutoReconnectSsidLen);

		DBGPRINT(RT_DEBUG_TRACE, "Driver auto reconnect to last OID_802_11_SSID setting - %s\n", pAd->MlmeAux.AutoReconnectSsid);

		// We will only try this attemp once, therefore change the AutoReconnect flag afterwards.
		pAd->MlmeAux.CurrReqIsFromNdis = FALSE;

		MlmeEnqueue(pAd,
					MLME_CNTL_STATE_MACHINE,
					OID_802_11_SSID,
					sizeof(NDIS_802_11_SSID),
					&OidSsid);
		RTUSBMlmeUp(pAd);
	}
}

/*
	==========================================================================
	Validate SSID for connection try and rescan purpose
	Valid SSID will have visible chars only.
	The valid length is from 0 to 32.
	==========================================================================
 */
BOOLEAN	MlmeValidateSSID(
	IN PUCHAR	pSsid,
	IN UCHAR	SsidLen)
{
	int	index;

	if (SsidLen > MAX_LEN_OF_SSID)
		return (FALSE);

	// Check each character value
	for (index = 0; index < SsidLen; index++)
	{
		if (pSsid[index] < 0x20)
			return (FALSE);
	}

	// All checked
	return (TRUE);
}

/*
	==========================================================================
	Description:
		This routine checks if there're other APs out there capable for
		roaming. Caller should call this routine only when Link up in INFRA mode
		and channel quality is below CQI_GOOD_THRESHOLD.

	Output:
	==========================================================================
 */
VOID MlmeCheckForRoaming(
	IN PRTMP_ADAPTER pAd,
	IN unsigned long Now)
{
	USHORT	   i;
	BSS_TABLE  *pRoamTab = &pAd->MlmeAux.RoamTab;
	BSS_ENTRY  *pBss;

	DBGPRINT(RT_DEBUG_TRACE, "==> MlmeCheckForRoaming::pAd->ScanTab.BssNr = %d\n", pAd->ScanTab.BssNr);
	// put all roaming candidates into RoamTab, and sort in RSSI order
	BssTableInit(pRoamTab);
	for (i = 0; i < pAd->ScanTab.BssNr; i++)
	{
		pBss = &pAd->ScanTab.BssEntry[i];

		DBGPRINT(RT_DEBUG_TRACE, "MlmeCheckForRoaming::pBss->LastBeaconRxTime = %lu\n", pBss->LastBeaconRxTime);
		if (time_after(Now, pBss->LastBeaconRxTime + BEACON_LOST_TIME))
		{
			DBGPRINT(RT_DEBUG_TRACE, "1: AP disappear::Now = %lu\n", Now);
			continue;	 // AP disappear
		}
		if (pBss->Rssi <= RSSI_THRESHOLD_FOR_ROAMING)
		{
			DBGPRINT(RT_DEBUG_TRACE, "2: RSSI too weak::Rssi[%d] - RSSI_THRESHOLD_FOR_ROAMING[%d]\n", pBss->Rssi, RSSI_THRESHOLD_FOR_ROAMING);
			continue;	 // RSSI too weak. forget it.
		}
		if (MAC_ADDR_EQUAL(pBss->Bssid, pAd->PortCfg.Bssid))
		{
			DBGPRINT(RT_DEBUG_TRACE, "3: skip current AP\n");
			continue;	 // skip current AP
		}
		if (!SSID_EQUAL(pBss->Ssid, pBss->SsidLen, pAd->PortCfg.Ssid, pAd->PortCfg.SsidLen))
		{
			DBGPRINT(RT_DEBUG_TRACE, "4: skip different SSID\n");
			continue;	 // skip different SSID
		}
		if (pBss->Rssi < (pAd->PortCfg.LastRssi + RSSI_DELTA))
		{
			DBGPRINT(RT_DEBUG_TRACE, "5: only AP with stronger RSSI is eligible for roaming\n");
			continue;	 // only AP with stronger RSSI is eligible for roaming
		}
		// AP passing all above rules is put into roaming candidate table
		memcpy(&pRoamTab->BssEntry[pRoamTab->BssNr], pBss, sizeof(BSS_ENTRY));
		pRoamTab->BssNr += 1;
	}

	if (pRoamTab->BssNr > 0)
	{
		// check CntlMachine.CurrState to avoid collision with NDIS SetOID request
		if (pAd->Mlme.CntlMachine.CurrState == CNTL_IDLE)
		{
			pAd->RalinkCounters.PoorCQIRoamingCount ++;
			DBGPRINT(RT_DEBUG_TRACE, "MMCHK - Roaming attempt #%d\n", pAd->RalinkCounters.PoorCQIRoamingCount);
			MlmeEnqueue(pAd, MLME_CNTL_STATE_MACHINE, MT2_MLME_ROAMING_REQ, 0, NULL);
			RTUSBMlmeUp(pAd);
		}
	}
	DBGPRINT(RT_DEBUG_TRACE, "<== MlmeCheckForRoaming(# of candidate= %d)\n",pRoamTab->BssNr);
}

/*
	==========================================================================
	Description:
		This routine checks if there're other APs out there capable for
		roaming. Caller should call this routine only when link up in INFRA mode
		and channel quality is below CQI_GOOD_THRESHOLD.

	Output:
	==========================================================================
 */
VOID MlmeCheckForFastRoaming(
	IN	PRTMP_ADAPTER	pAd,
	IN	unsigned long	Now)
{
	USHORT     i;
	BSS_TABLE  *pRoamTab = &pAd->MlmeAux.RoamTab;
	BSS_ENTRY  *pBss;

	DBGPRINT(RT_DEBUG_TRACE, "==> MlmeCheckForFastRoaming\n");
	// put all roaming candidates into RoamTab, and sort in RSSI order
	BssTableInit(pRoamTab);
	for (i = 0; i < pAd->ScanTab.BssNr; i++)
	{
		pBss = &pAd->ScanTab.BssEntry[i];

		if ((pBss->Rssi <= 45) && (pBss->Channel == pAd->PortCfg.Channel))
			continue;    // RSSI too weak. forget it.
		if (MAC_ADDR_EQUAL(pBss->Bssid, pAd->PortCfg.Bssid))
			continue;    // skip current AP
		if (!SSID_EQUAL(pBss->Ssid, pBss->SsidLen, pAd->PortCfg.Ssid, pAd->PortCfg.SsidLen))
			continue;    // skip different SSID
		if (pBss->Rssi < (pAd->PortCfg.LastRssi + RSSI_DELTA))
			continue;    // skip AP without better RSSI

		DBGPRINT(RT_DEBUG_TRACE, "LastRssi = %d, pBss->Rssi = %d\n", pAd->PortCfg.LastRssi, pBss->Rssi);
		// AP passing all above rules is put into roaming candidate table
		memcpy(&pRoamTab->BssEntry[pRoamTab->BssNr], pBss, sizeof(BSS_ENTRY));
		pRoamTab->BssNr += 1;
	}

	if (pRoamTab->BssNr > 0)
	{
		// check CntlMachine.CurrState to avoid collision with NDIS SetOID request
		if (pAd->Mlme.CntlMachine.CurrState == CNTL_IDLE)
		{
			pAd->RalinkCounters.PoorCQIRoamingCount ++;
			DBGPRINT(RT_DEBUG_TRACE, "MMCHK - Roaming attempt #%d\n", pAd->RalinkCounters.PoorCQIRoamingCount);
			MlmeEnqueue(pAd, MLME_CNTL_STATE_MACHINE, MT2_MLME_ROAMING_REQ, 0, NULL);
			RTUSBMlmeUp(pAd);
		}
	}
	// Maybe site survey required
	else
	{
		if ((pAd->PortCfg.LastScanTime + 10 * 1000) < Now)
		{
		    MLME_SCAN_REQ_STRUCT	   ScanReq;

			// check CntlMachine.CurrState to avoid collision with NDIS SetOID request
			DBGPRINT(RT_DEBUG_TRACE, "MMCHK - Roaming, No eligable entry, try a new ACTIVE scan SSID[%s]\n", pAd->MlmeAux.AutoReconnectSsid);

            ScanParmFill(pAd, &ScanReq, pAd->MlmeAux.AutoReconnectSsid, pAd->MlmeAux.AutoReconnectSsidLen, BSS_ANY, SCAN_ACTIVE);
			MlmeEnqueue(pAd, SYNC_STATE_MACHINE, MT2_MLME_SCAN_REQ,
						sizeof(MLME_SCAN_REQ_STRUCT), &ScanReq);
			RTUSBMlmeUp(pAd);
			pAd->Mlme.CntlMachine.CurrState = CNTL_WAIT_OID_LIST_SCAN;
		}
	}

	DBGPRINT(RT_DEBUG_TRACE, "<== MlmeCheckForFastRoaming\n");
}

/*
	==========================================================================
	Description:
		This routine calculates TxPER, RxPER of the past N-sec period. And
		according to the calculation result, ChannelQuality is calculated here
		to decide if current AP is still doing the job.

		If ChannelQuality is not good, a ROAMing attempt may be tried later.
	Output:
		PortCfg.ChannelQuality - 0..100


	NOTE: This routine decide channle quality based on RX CRC error ratio.
		Caller should make sure a function call to NICUpdateRawCounters(pAd)
		is performed right before this routine, so that this routine can decide
		channel quality based on the most up-to-date information
	==========================================================================
 */
VOID MlmeCalculateChannelQuality(
	IN PRTMP_ADAPTER pAd,
	IN unsigned long Now)
{
	ULONG TxOkCnt, TxCnt, TxPER, TxPRR;
	ULONG RxCnt, RxPER;
	UCHAR NorRssi;

	//
	// calculate TX packet error ratio and TX retry ratio - if too few TX samples, skip TX related statistics
	//
	TxOkCnt = pAd->RalinkCounters.OneSecTxNoRetryOkCount + pAd->RalinkCounters.OneSecTxRetryOkCount;
	TxCnt = TxOkCnt + pAd->RalinkCounters.OneSecTxFailCount;
	if (TxCnt < 5)
	{
		TxPER = 0;
		TxPRR = 0;
	}
	else
	{
		TxPER = (pAd->RalinkCounters.OneSecTxFailCount * 100) / TxCnt;
		TxPRR = ((TxCnt - pAd->RalinkCounters.OneSecTxNoRetryOkCount) * 100) / TxCnt;
	}

	//
	// calculate RX PER - don't take RxPER into consideration if too few sample
	//
	RxCnt = pAd->RalinkCounters.OneSecRxOkCnt + pAd->RalinkCounters.OneSecRxFcsErrCnt;
	if (RxCnt < 5)
		RxPER = 0;
	else
		RxPER = (pAd->RalinkCounters.OneSecRxFcsErrCnt * 100) / RxCnt;

	//
	// decide ChannelQuality based on: 1)last BEACON received time, 2)last RSSI, 3)TxPER, and 4)RxPER
	//
	if (INFRA_ON(pAd) &&
		(TxOkCnt < 2) && // no heavy traffic
		(time_after(Now, pAd->PortCfg.LastBeaconRxTime + BEACON_LOST_TIME)))
	{
		DBGPRINT(RT_DEBUG_TRACE, "BEACON lost > %d msec with TxOkCnt=%d -> CQI=0\n", BEACON_LOST_TIME, TxOkCnt);
		pAd->Mlme.ChannelQuality = 0;
	}
	else
	{
		// Normalize Rssi
		if (pAd->PortCfg.LastRssi > 0x50)
			NorRssi = 100;
		else if (pAd->PortCfg.LastRssi < 0x20)
			NorRssi = 0;
		else
			NorRssi = (pAd->PortCfg.LastRssi - 0x20) * 2;

		// ChannelQuality = W1*RSSI + W2*TxPRR + W3*RxPER	 (RSSI 0..100), (TxPER 100..0), (RxPER 100..0)
		pAd->Mlme.ChannelQuality = (RSSI_WEIGHTING * NorRssi +
									TX_WEIGHTING * (100 - TxPRR) +
									RX_WEIGHTING* (100 - RxPER)) / 100;
		if (pAd->Mlme.ChannelQuality >= 100)
			pAd->Mlme.ChannelQuality = 100;
	}

	DBGPRINT(RT_DEBUG_INFO, "MMCHK - CQI= %d (Tx Fail=%d/Retry=%d/Total=%d, Rx Fail=%d/Total=%d, RSSI=%d dbm)\n",
		pAd->Mlme.ChannelQuality,
		pAd->RalinkCounters.OneSecTxFailCount,
		pAd->RalinkCounters.OneSecTxRetryOkCount,
		TxCnt,
		pAd->RalinkCounters.OneSecRxFcsErrCnt,
		RxCnt, pAd->PortCfg.LastRssi - pAd->BbpRssiToDbmDelta);

}

/*
	==========================================================================
	Description:
		This routine calculates the acumulated TxPER of eaxh TxRate. And
		according to the calculation result, change PortCfg.TxRate which
		is the stable TX Rate we expect the Radio situation could sustained.

		PortCfg.TxRate will change dynamically within {RATE_1/RATE_6, MaxTxRate}
	Output:
		PortCfg.TxRate -


	NOTE:
		call this routine every second
	==========================================================================
 */
VOID MlmeDynamicTxRateSwitching(
	IN PRTMP_ADAPTER pAd)
{
	UCHAR	UpRate, DownRate, CurrRate;
	ULONG	TxTotalCnt, NewBasicRateBitmap;
	ULONG	TxErrorRatio = 0;
	BOOLEAN fUpgradeQuality = FALSE;
	SHORT	dbm = pAd->PortCfg.AvgRssi - pAd->BbpRssiToDbmDelta;

	CurrRate = pAd->PortCfg.TxRate;

	// do not reply ACK using TX rate higher than normal DATA TX rate
	NewBasicRateBitmap = pAd->PortCfg.BasicRateBitmap & BasicRateMask[CurrRate];
	RTUSBWriteMACRegister(pAd, TXRX_CSR5, NewBasicRateBitmap);

	// if no traffic in the past 1-sec period, don't change TX rate,
	// but clear all bad history. because the bad history may affect the next
	// Chariot throughput test
	TxTotalCnt = pAd->RalinkCounters.OneSecTxNoRetryOkCount +
				 pAd->RalinkCounters.OneSecTxRetryOkCount +
				 pAd->RalinkCounters.OneSecTxFailCount;

	if (TxTotalCnt)
		TxErrorRatio = ((pAd->RalinkCounters.OneSecTxRetryOkCount + pAd->RalinkCounters.OneSecTxFailCount) *100) / TxTotalCnt;

	DBGPRINT(RT_DEBUG_TRACE,"%d: NDIS push BE=%d, BK=%d, VI=%d, VO=%d, TX/RX AGGR=<%d,%d>, p-NDIS=%d, RSSI=%d, ACKbmap=%03x, PER=%d%%\n",
		RateIdToMbps[CurrRate],
		pAd->RalinkCounters.OneSecOsTxCount[QID_AC_BE],
		pAd->RalinkCounters.OneSecOsTxCount[QID_AC_BK],
		pAd->RalinkCounters.OneSecOsTxCount[QID_AC_VI],
		pAd->RalinkCounters.OneSecOsTxCount[QID_AC_VO],
		pAd->RalinkCounters.OneSecTxAggregationCount,
		pAd->RalinkCounters.OneSecRxAggregationCount,
		pAd->RalinkCounters.PendingNdisPacketCount,
		dbm,
		NewBasicRateBitmap & 0xfff,
		TxErrorRatio);

	if (! OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_TX_RATE_SWITCH_ENABLED))
		return;

	//
	// CASE 1. when TX samples are fewer than 15, then decide TX rate solely on RSSI
	//		   (criteria copied from RT2500 for Netopia case)
	//
	if (TxTotalCnt <= 15)
	{
		TxErrorRatio = 0;
		pAd->DrsCounters.TxRateUpPenalty = 0;
		memset(pAd->DrsCounters.TxQuality, 0, MAX_LEN_OF_SUPPORTED_RATES);
		memset(pAd->DrsCounters.PER, 0, MAX_LEN_OF_SUPPORTED_RATES);

		if (dbm >= -65)
			pAd->PortCfg.TxRate = RATE_54;
		else if (dbm >= -66)
			pAd->PortCfg.TxRate = RATE_48;
		else if (dbm >= -70)
			pAd->PortCfg.TxRate = RATE_36;
		else if (dbm >= -74)
			pAd->PortCfg.TxRate = RATE_24;
		else if (dbm >= -77)
			pAd->PortCfg.TxRate = RATE_18;
		else if (dbm >= -79)
			pAd->PortCfg.TxRate = RATE_12;
		else if (dbm >= -81)
		{
			// in 11A or 11G-only mode, no CCK rates available
			if ((pAd->PortCfg.Channel > 14) || (pAd->PortCfg.PhyMode == PHY_11G))
				pAd->PortCfg.TxRate = RATE_9;
			else
				pAd->PortCfg.TxRate = RATE_11;
		}
		else
		{
			// in 11A or 11G-only mode, no CCK rates available
			if ((pAd->PortCfg.Channel > 14) || (pAd->PortCfg.PhyMode == PHY_11G))
				pAd->PortCfg.TxRate = RATE_6;
			else
			{
				if (dbm >= -82)
					pAd->PortCfg.TxRate = RATE_11;
				else if (dbm >= -84)
					pAd->PortCfg.TxRate = RATE_5_5;
				else if (dbm >= -85)
					pAd->PortCfg.TxRate = RATE_2;
				else
					pAd->PortCfg.TxRate = RATE_1;
			}
		}

		if (pAd->PortCfg.TxRate > pAd->PortCfg.MaxTxRate)
			pAd->PortCfg.TxRate = pAd->PortCfg.MaxTxRate;

		return;
	}

	//
	// CASE2. enough TX samples, tune TX rate based on TxPER
	//
	do
	{
		pAd->DrsCounters.CurrTxRateStableTime ++;

		// decide the next upgrade rate and downgrade rate, if any
		if ((pAd->PortCfg.Channel > 14) ||		// must be in 802.11A band
			(pAd->PortCfg.PhyMode == PHY_11G))	// G-only mode, no CCK rates available
		{
			UpRate = Phy11ANextRateUpward[CurrRate];
			DownRate = Phy11ANextRateDownward[CurrRate];
		}
		else
		{
			UpRate = Phy11BGNextRateUpward[CurrRate];
			DownRate = Phy11BGNextRateDownward[CurrRate];
		}

		if (UpRate > pAd->PortCfg.MaxTxRate)
			UpRate = pAd->PortCfg.MaxTxRate;

		TxErrorRatio = ((pAd->RalinkCounters.OneSecTxRetryOkCount + pAd->RalinkCounters.OneSecTxFailCount) *100) / TxTotalCnt;

		// downgrade TX quality if PER >= Rate-Down threshold
		if (TxErrorRatio >= RateDownPER[CurrRate])
		{
			pAd->DrsCounters.TxQuality[CurrRate] = DRS_TX_QUALITY_WORST_BOUND;
		}
		// upgrade TX quality if PER <= Rate-Up threshold
		else if (TxErrorRatio <= RateUpPER[CurrRate])
		{
			fUpgradeQuality = TRUE;
			if (pAd->DrsCounters.TxQuality[CurrRate])
				pAd->DrsCounters.TxQuality[CurrRate] --;  // quality very good in CurrRate

			if (pAd->DrsCounters.TxRateUpPenalty)
				pAd->DrsCounters.TxRateUpPenalty --;
			else if (pAd->DrsCounters.TxQuality[UpRate])
				pAd->DrsCounters.TxQuality[UpRate] --;	  // may improve next UP rate's quality
		}

		pAd->DrsCounters.PER[CurrRate] = (UCHAR)TxErrorRatio;

#if 1
		// 2004-3-13 special case: Claim noisy environment
		//	 decide if there was a false "rate down" in the past 2 sec due to noisy
		//	 environment. if so, we would rather switch back to the higher TX rate.
		//	 criteria -
		//	   1. there's a higher rate available, AND
		//	   2. there was a rate-down happened, AND
		//	   3. current rate has 75% > PER > 20%, AND
		//	   4. comparing to UpRate, current rate didn't improve PER more than 5 %
		if ((UpRate != CurrRate)							  &&
			(pAd->DrsCounters.LastSecTxRateChangeAction == 2) &&
			(pAd->DrsCounters.PER[CurrRate] < 75) &&
			((pAd->DrsCounters.PER[CurrRate] > 20) || (pAd->DrsCounters.fNoisyEnvironment)) &&
			((pAd->DrsCounters.PER[CurrRate]+5) > pAd->DrsCounters.PER[UpRate]))
		{
			// we believe this is a noisy environment. better stay at UpRate
			DBGPRINT(RT_DEBUG_TRACE,"DRS: #### enter Noisy environment ####\n");
			pAd->DrsCounters.fNoisyEnvironment = TRUE;

			// 2004-3-14 when claiming noisy environment, we're not only switch back
			//	 to UpRate, but can be more aggressive to use one more rate up
			UpRate++;
			if ((UpRate==RATE_6) || (UpRate==RATE_9)) UpRate=RATE_12;
			if (UpRate > pAd->PortCfg.MaxTxRate)
				UpRate = pAd->PortCfg.MaxTxRate;
			pAd->PortCfg.TxRate = UpRate;
			break;
		}

		// 2004-3-12 special case: Leave noisy environment
		//	 The interference has gone suddenly. reset TX rate to
		//	 the theoritical value according to RSSI. Criteria -
		//	   1. it's currently in noisy environment
		//	   2. PER drops to be below 12%
		if ((pAd->DrsCounters.fNoisyEnvironment == TRUE) &&
			(pAd->DrsCounters.PER[CurrRate] <= 12))
		{
			UCHAR JumpUpRate;

			pAd->DrsCounters.fNoisyEnvironment = FALSE;
			for (JumpUpRate = RATE_54; JumpUpRate > RATE_1; JumpUpRate--)
			{
				if (dbm > RssiSafeLevelForTxRate[JumpUpRate])
					break;
			}

			if (JumpUpRate > pAd->PortCfg.MaxTxRate)
				JumpUpRate = pAd->PortCfg.MaxTxRate;

			DBGPRINT(RT_DEBUG_TRACE,"DRS: #### leave Noisy environment ####, RSSI=%d, JumpUpRate=%d\n",
				dbm, RateIdToMbps[JumpUpRate]);

			if (JumpUpRate > CurrRate)
			{
				pAd->PortCfg.TxRate = JumpUpRate;
				break;
			}
		}
#endif
		// we're going to upgrade CurrRate to UpRate at next few seconds,
		// but before that, we'd better try a NULL frame @ UpRate and
		// see if UpRate is stable or not. If this NULL frame fails, it will
		// downgrade TxQuality[CurrRate], so that STA won't switch to
		// to UpRate in the next second
		// 2004-04-07 requested by David Tung - sent test frames only in OFDM rates
		if (fUpgradeQuality 	 &&
			INFRA_ON(pAd)		 &&
			(UpRate != CurrRate) &&
			(UpRate > RATE_11)	 &&
			(pAd->DrsCounters.TxQuality[CurrRate] <= 1) &&
			(pAd->DrsCounters.TxQuality[UpRate] <= 1))
		{
			DBGPRINT(RT_DEBUG_TRACE,"DRS: 2 NULL frames at UpRate = %d Mbps\n",RateIdToMbps[UpRate]);
			RTMPSendNullFrame(pAd, UpRate);
		}

		// perform DRS - consider TxRate Down first, then rate up.
		//	   1. rate down, if current TX rate's quality is not good
		//	   2. rate up, if UPRate's quality is very good
		if ((pAd->DrsCounters.TxQuality[CurrRate] >= DRS_TX_QUALITY_WORST_BOUND) &&
			(CurrRate != DownRate))
		{
#if 1
			// guarantee a minimum TX rate for each RSSI segments
			if ((dbm >= -45) && (DownRate < RATE_48))
				pAd->PortCfg.TxRate = RATE_48;
			else if ((dbm >= -50) && (DownRate < RATE_36))
				pAd->PortCfg.TxRate = RATE_36;
			else if ((dbm >= -55) && (DownRate < RATE_24))
				pAd->PortCfg.TxRate = RATE_24;
			else if ((dbm >= -60) && (DownRate < RATE_18))
				pAd->PortCfg.TxRate = RATE_18;
			else if ((dbm >= -65) && (DownRate < RATE_12))
				pAd->PortCfg.TxRate = RATE_12;
			else if ((dbm >= -70) && (DownRate < RATE_9))
			{
				// in 11A or 11G-only mode, no CCK rates available
				if ((pAd->PortCfg.Channel > 14) || (pAd->PortCfg.PhyMode == PHY_11G))
					pAd->PortCfg.TxRate = RATE_9;
				else
					pAd->PortCfg.TxRate = RATE_11;
			}
			else
#endif
			{
				if ((dbm >= -75) && (DownRate < RATE_11))
					pAd->PortCfg.TxRate = RATE_11;
				else
				{
#ifdef WIFI_TEST
					if (DownRate <= RATE_2) break; // never goes lower than 5.5 Mbps TX rate
#endif
					// otherwise, if DownRate still better than the low bound that current RSSI can support,
					// go straight to DownRate
					pAd->PortCfg.TxRate = DownRate;
				}
			}
		}
		else if ((pAd->DrsCounters.TxQuality[CurrRate] <= 0) &&
			(pAd->DrsCounters.TxQuality[UpRate] <=0)		 &&
			(CurrRate != UpRate))
		{
			pAd->PortCfg.TxRate = UpRate;
		}

		//
		// To make sure TxRate didn't over MaxTxRate
		//
		if (pAd->PortCfg.TxRate > pAd->PortCfg.MaxTxRate)
			pAd->PortCfg.TxRate = pAd->PortCfg.MaxTxRate;

	}while (FALSE);


	// if rate-up happen, clear all bad history of all TX rates
	if (pAd->PortCfg.TxRate > CurrRate)
	{
		DBGPRINT(RT_DEBUG_TRACE,"DRS: ++TX rate from %d to %d Mbps\n", RateIdToMbps[CurrRate],RateIdToMbps[pAd->PortCfg.TxRate]);
		pAd->DrsCounters.CurrTxRateStableTime = 0;
		pAd->DrsCounters.TxRateUpPenalty = 0;
		pAd->DrsCounters.LastSecTxRateChangeAction = 1; // rate UP
		memset(pAd->DrsCounters.TxQuality, 0, MAX_LEN_OF_SUPPORTED_RATES);
		memset(pAd->DrsCounters.PER, 0, MAX_LEN_OF_SUPPORTED_RATES);
#if 0
//This mechanism should be changed on USB device.
		//
		// For TxRate fast train up, issued by David 2005/05/12
		//
		if (!pAd->PortCfg.QuickResponeForRateUpTimerRunning)
		{
			if (pAd->PortCfg.TxRate <= RATE_12)
				RTMPSetTimer(pAd, &pAd->PortCfg.QuickResponeForRateUpTimer, 200);
			else
			  RTMPSetTimer(pAd, &pAd->PortCfg.QuickResponeForRateUpTimer, 100);

			pAd->PortCfg.QuickResponeForRateUpTimerRunning = TRUE;
		}
#endif
	}
	// if rate-down happen, only clear DownRate's bad history
	else if (pAd->PortCfg.TxRate < CurrRate)
	{
		DBGPRINT(RT_DEBUG_TRACE,"DRS: --TX rate from %d to %d Mbps\n", RateIdToMbps[CurrRate],RateIdToMbps[pAd->PortCfg.TxRate]);
#if 0
//Remove this code for TxRate fast train up. issued by David 2005/05/12
		// shorter stable time require more penalty in next rate UP criteria
		//if (pAd->DrsCounters.CurrTxRateStableTime < 4)	  // less then 4 sec
		//	  pAd->DrsCounters.TxRateUpPenalty = DRS_PENALTY; // add 8 sec penalty
		//else if (pAd->DrsCounters.CurrTxRateStableTime < 8) // less then 8 sec
		//	  pAd->DrsCounters.TxRateUpPenalty = 2; 		  // add 2 sec penalty
		//else												  // >= 8 sec
#endif
		pAd->DrsCounters.TxRateUpPenalty = 0;			// no penalty

		pAd->DrsCounters.CurrTxRateStableTime = 0;
		pAd->DrsCounters.LastSecTxRateChangeAction = 2; // rate DOWN
		pAd->DrsCounters.TxQuality[pAd->PortCfg.TxRate] = 0;
		pAd->DrsCounters.PER[pAd->PortCfg.TxRate] = 0;
	}
	else
		pAd->DrsCounters.LastSecTxRateChangeAction = 0; // rate no change

}

/*
	==========================================================================
	Description:
		This routine is executed periodically inside MlmePeriodicExec() after
		association with an AP.
		It checks if PortCfg.Psm is consistent with user policy (recorded in
		PortCfg.WindowsPowerMode). If not, enforce user policy. However,
		there're some conditions to consider:
		1. we don't support power-saving in ADHOC mode, so Psm=PWR_ACTIVE all
		   the time when Mibss==TRUE
		2. When link up in INFRA mode, Psm should not be switch to PWR_SAVE
		   if outgoing traffic available in TxRing or MgmtRing.
	Output:
		1. change pAd->PortCfg.Psm to PWR_SAVE or leave it untouched


	==========================================================================
 */
VOID MlmeCheckPsmChange(
	IN PRTMP_ADAPTER pAd,
	IN unsigned long Now)
{
	ULONG	PowerMode;
	// condition -
	// 1. Psm maybe ON only happen in INFRASTRUCTURE mode
	// 2. user wants either MAX_PSP or FAST_PSP
	// 3. but current psm is not in PWR_SAVE
	// 4. CNTL state machine is not doing SCANning
	// 5. no TX SUCCESS event for the past 1-sec period
		PowerMode = pAd->PortCfg.WindowsPowerMode;

	if (INFRA_ON(pAd) &&
		(PowerMode != Ndis802_11PowerModeCAM) &&
		(pAd->PortCfg.Psm == PWR_ACTIVE) &&
//		(! RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_BSS_SCAN_IN_PROGRESS))
		(pAd->Mlme.CntlMachine.CurrState == CNTL_IDLE) &&
		(pAd->RalinkCounters.OneSecTxNoRetryOkCount == 0) &&
		(pAd->RalinkCounters.OneSecTxRetryOkCount == 0))
	{
		RTUSBEnqueueInternalCmd(pAd, RT_OID_SET_PSM_BIT_SAVE);
	}

}

VOID MlmeSetPsmBit(
	IN PRTMP_ADAPTER pAd,
	IN USHORT psm)
{
	TXRX_CSR4_STRUC *csr4 = kzalloc(sizeof(TXRX_CSR4_STRUC), GFP_KERNEL);

	if (!csr4) {
		DBGPRINT(RT_DEBUG_ERROR, "couldn't allocate memory\n");
		return;
	}

	pAd->PortCfg.Psm = psm;
	RTUSBReadMACRegister(pAd, TXRX_CSR4, &csr4->word);
	csr4->field.AckCtsPsmBit = (psm == PWR_SAVE)? 1:0;
	RTUSBWriteMACRegister(pAd, TXRX_CSR4, csr4->word);
	DBGPRINT(RT_DEBUG_TRACE, "MlmeSetPsmBit = %d\n", psm);
	kfree(csr4);
}

VOID MlmeSetTxPreamble(
	IN PRTMP_ADAPTER pAd,
	IN USHORT TxPreamble)
{
	TXRX_CSR4_STRUC *csr4 = kzalloc(sizeof(TXRX_CSR4_STRUC), GFP_KERNEL);

	if (!csr4) {
		DBGPRINT(RT_DEBUG_ERROR, "couldn't allocate memory\n");
		return;
	}

	RTUSBReadMACRegister(pAd, TXRX_CSR4, &csr4->word);
	if (TxPreamble == Rt802_11PreambleShort)
	{
		// NOTE: 1Mbps should always use long preamble
		DBGPRINT(RT_DEBUG_TRACE, "MlmeSetTxPreamble (= SHORT PREAMBLE)\n");
		OPSTATUS_SET_FLAG(pAd, fOP_STATUS_SHORT_PREAMBLE_INUSED);
		csr4->field.AutoResponderPreamble = 0;
	}
	else
	{
		DBGPRINT(RT_DEBUG_TRACE, "MlmeSetTxPreamble (= LONG PREAMBLE)\n");
		OPSTATUS_CLEAR_FLAG(pAd, fOP_STATUS_SHORT_PREAMBLE_INUSED);
		csr4->field.AutoResponderPreamble = 1;
	}
	RTUSBWriteMACRegister(pAd, TXRX_CSR4, csr4->word);
	kfree(csr4);
}

// bLinkUp is to identify the inital link speed.
// TRUE indicates the rate update at linkup, we should not try to set the rate at 54Mbps.
VOID MlmeUpdateTxRates(
	IN PRTMP_ADAPTER pAd,
	IN BOOLEAN		 bLinkUp)
{
	int i, num;
	UCHAR Rate, MaxDesire = RATE_1, MaxSupport = RATE_1;
	ULONG BasicRateBitmap = 0;
	UCHAR CurrBasicRate = RATE_1;
	UCHAR *pSupRate, *pExtRate, SupRateLen, ExtRateLen;

	// find max desired rate
	num = 0;
	for (i=0; i<MAX_LEN_OF_SUPPORTED_RATES; i++)
	{
		switch (pAd->PortCfg.DesireRate[i] & 0x7f)
		{
			case 2:  Rate = RATE_1;   num++;   break;
			case 4:  Rate = RATE_2;   num++;   break;
			case 11: Rate = RATE_5_5; num++;   break;
			case 22: Rate = RATE_11;  num++;   break;
			case 12: Rate = RATE_6;   num++;   break;
			case 18: Rate = RATE_9;   num++;   break;
			case 24: Rate = RATE_12;  num++;   break;
			case 36: Rate = RATE_18;  num++;   break;
			case 48: Rate = RATE_24;  num++;   break;
			case 72: Rate = RATE_36;  num++;   break;
			case 96: Rate = RATE_48;  num++;   break;
			case 108: Rate = RATE_54; num++;   break;
			default: Rate = RATE_1;   break;
		}
		if (MaxDesire < Rate)  MaxDesire = Rate;
	}

	// 2003-12-10 802.11g WIFI spec disallow OFDM rates in 802.11g ADHOC mode
	if ((pAd->PortCfg.BssType == BSS_ADHOC) 	   &&
		(pAd->PortCfg.PhyMode == PHY_11BG_MIXED)   &&
		(pAd->PortCfg.AdhocMode == 0) &&
		(MaxDesire > RATE_11))
		MaxDesire = RATE_11;

	pAd->PortCfg.MaxDesiredRate = MaxDesire;

	// Auto rate switching is enabled only if more than one DESIRED RATES are
	// specified; otherwise disabled
	if (num <= 1)
		OPSTATUS_CLEAR_FLAG(pAd, fOP_STATUS_TX_RATE_SWITCH_ENABLED);
	else
		OPSTATUS_SET_FLAG(pAd, fOP_STATUS_TX_RATE_SWITCH_ENABLED);


	if (ADHOC_ON(pAd) || INFRA_ON(pAd))
	{
		pSupRate = &pAd->ActiveCfg.SupRate[0];
		pExtRate = &pAd->ActiveCfg.ExtRate[0];
		SupRateLen = pAd->ActiveCfg.SupRateLen;
		ExtRateLen = pAd->ActiveCfg.ExtRateLen;
	}
	else
	{
		pSupRate = &pAd->PortCfg.SupRate[0];
		pExtRate = &pAd->PortCfg.ExtRate[0];
		SupRateLen = pAd->PortCfg.SupRateLen;
		ExtRateLen = pAd->PortCfg.ExtRateLen;
	}

	// find max supported rate
	for (i=0; i<SupRateLen; i++)
	{
		switch (pSupRate[i] & 0x7f)
		{
			case 2:   Rate = RATE_1;	if (pSupRate[i] & 0x80) BasicRateBitmap |= 0x0001;	 break;
			case 4:   Rate = RATE_2;	if (pSupRate[i] & 0x80) BasicRateBitmap |= 0x0002;	 break;
			case 11:  Rate = RATE_5_5;	if (pSupRate[i] & 0x80) BasicRateBitmap |= 0x0004;	 break;
			case 22:  Rate = RATE_11;	if (pSupRate[i] & 0x80) BasicRateBitmap |= 0x0008;	 break;
			case 12:  Rate = RATE_6;	/*if (pSupRate[i] & 0x80)*/  BasicRateBitmap |= 0x0010;  break;
			case 18:  Rate = RATE_9;	if (pSupRate[i] & 0x80) BasicRateBitmap |= 0x0020;	 break;
			case 24:  Rate = RATE_12;	/*if (pSupRate[i] & 0x80)*/  BasicRateBitmap |= 0x0040;  break;
			case 36:  Rate = RATE_18;	if (pSupRate[i] & 0x80) BasicRateBitmap |= 0x0080;	 break;
			case 48:  Rate = RATE_24;	/*if (pSupRate[i] & 0x80)*/  BasicRateBitmap |= 0x0100;  break;
			case 72:  Rate = RATE_36;	if (pSupRate[i] & 0x80) BasicRateBitmap |= 0x0200;	 break;
			case 96:  Rate = RATE_48;	if (pSupRate[i] & 0x80) BasicRateBitmap |= 0x0400;	 break;
			case 108: Rate = RATE_54;	if (pSupRate[i] & 0x80) BasicRateBitmap |= 0x0800;	 break;
			default:  Rate = RATE_1;	break;
		}
		if (MaxSupport < Rate)	MaxSupport = Rate;
	}
	for (i=0; i<ExtRateLen; i++)
	{
		switch (pExtRate[i] & 0x7f)
		{
			case 2:   Rate = RATE_1;	if (pExtRate[i] & 0x80) BasicRateBitmap |= 0x0001;	 break;
			case 4:   Rate = RATE_2;	if (pExtRate[i] & 0x80) BasicRateBitmap |= 0x0002;	 break;
			case 11:  Rate = RATE_5_5;	if (pExtRate[i] & 0x80) BasicRateBitmap |= 0x0004;	 break;
			case 22:  Rate = RATE_11;	if (pExtRate[i] & 0x80) BasicRateBitmap |= 0x0008;	 break;
			case 12:  Rate = RATE_6;	/*if (pExtRate[i] & 0x80)*/  BasicRateBitmap |= 0x0010;  break;
			case 18:  Rate = RATE_9;	if (pExtRate[i] & 0x80) BasicRateBitmap |= 0x0020;	 break;
			case 24:  Rate = RATE_12;	/*if (pExtRate[i] & 0x80)*/  BasicRateBitmap |= 0x0040;  break;
			case 36:  Rate = RATE_18;	if (pExtRate[i] & 0x80) BasicRateBitmap |= 0x0080;	 break;
			case 48:  Rate = RATE_24;	/*if (pExtRate[i] & 0x80)*/  BasicRateBitmap |= 0x0100;  break;
			case 72:  Rate = RATE_36;	if (pExtRate[i] & 0x80) BasicRateBitmap |= 0x0200;	 break;
			case 96:  Rate = RATE_48;	if (pExtRate[i] & 0x80) BasicRateBitmap |= 0x0400;	 break;
			case 108: Rate = RATE_54;	if (pExtRate[i] & 0x80) BasicRateBitmap |= 0x0800;	 break;
			default:  Rate = RATE_1;	break;
		}
		if (MaxSupport < Rate)	MaxSupport = Rate;
	}
	RTUSBWriteMACRegister(pAd, TXRX_CSR5, BasicRateBitmap);
	pAd->PortCfg.BasicRateBitmap = BasicRateBitmap;

	// calculate the exptected ACK rate for each TX rate. This info is used to caculate
	// the DURATION field of outgoing uniicast DATA/MGMT frame
	for (i=0; i<MAX_LEN_OF_SUPPORTED_RATES; i++)
	{
		if (BasicRateBitmap & (0x01 << i))
			CurrBasicRate = (UCHAR)i;
		pAd->PortCfg.ExpectedACKRate[i] = CurrBasicRate;
		DBGPRINT(RT_DEBUG_INFO, "Exptected ACK rate[%d] = %d Mbps\n", RateIdToMbps[i], RateIdToMbps[CurrBasicRate]);
	}

	// max tx rate = min {max desire rate, max supported rate}
	if (MaxSupport < MaxDesire)
		pAd->PortCfg.MaxTxRate = MaxSupport;
	else
		pAd->PortCfg.MaxTxRate = MaxDesire;

	// 2003-07-31 john - 2500 doesn't have good sensitivity at high OFDM rates. to increase the success
	// ratio of initial DHCP packet exchange, TX rate starts from a lower rate depending
	// on average RSSI
	//	 1. RSSI >= -70db, start at 54 Mbps (short distance)
	//	 2. -70 > RSSI >= -75, start at 24 Mbps (mid distance)
	//	 3. -75 > RSSI, start at 11 Mbps (long distance)
	if (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_TX_RATE_SWITCH_ENABLED) &&
		OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_MEDIA_STATE_CONNECTED))
	{
		short dbm = pAd->PortCfg.AvgRssi - pAd->BbpRssiToDbmDelta;
		if (bLinkUp == TRUE)
			pAd->PortCfg.TxRate = RATE_24;
		else
			pAd->PortCfg.TxRate = pAd->PortCfg.MaxTxRate;

		if (dbm < -75)
			pAd->PortCfg.TxRate = RATE_11;
		else if (dbm < -70)
			pAd->PortCfg.TxRate = RATE_24;

		// should never exceed MaxTxRate (consider 11B-only mode)
		if (pAd->PortCfg.TxRate > pAd->PortCfg.MaxTxRate)
			pAd->PortCfg.TxRate = pAd->PortCfg.MaxTxRate;
	   DBGPRINT(RT_DEBUG_TRACE, " MlmeUpdateTxRates (Rssi=%d, init TX rate = %d Mbps)\n", dbm, RateIdToMbps[pAd->PortCfg.TxRate]);
	}
	else
		pAd->PortCfg.TxRate = pAd->PortCfg.MaxTxRate;


	if (bLinkUp)
	{
		;;//Do nothing
	}
	else
	{
	    switch (pAd->PortCfg.PhyMode)
	    {
		    case PHY_11BG_MIXED:
		    case PHY_11B:
			    pAd->PortCfg.MlmeRate = RATE_2;
#ifdef	WIFI_TEST
			    pAd->PortCfg.RtsRate = RATE_11;
#else
			    pAd->PortCfg.RtsRate = RATE_2;
#endif
			    break;
		    case PHY_11A:
			    pAd->PortCfg.MlmeRate = RATE_6;
			    pAd->PortCfg.RtsRate = RATE_6;
			    break;
		    case PHY_11ABG_MIXED:
			    if (pAd->PortCfg.Channel <= 14)
			    {
				    pAd->PortCfg.MlmeRate = RATE_2;
				    pAd->PortCfg.RtsRate = RATE_2;
			    }
			    else
			    {
				    pAd->PortCfg.MlmeRate = RATE_6;
				    pAd->PortCfg.RtsRate = RATE_6;
			    }
			    break;
		    default: // error
			    pAd->PortCfg.MlmeRate = RATE_2;
			    pAd->PortCfg.RtsRate = RATE_2;
			    break;
        }

        //
        // Keep Basic Mlme Rate.
        //
        pAd->PortCfg.BasicMlmeRate = pAd->PortCfg.MlmeRate;
	}


	//
	//	Update MlmeRate & RtsRate for G only & A only on Adhoc mode.
	//
	if (ADHOC_ON(pAd))
	{
		if ((pAd->PortCfg.AdhocMode == ADHOC_11G) || (pAd->PortCfg.AdhocMode == ADHOC_11A))
		{
			pAd->PortCfg.MlmeRate = RATE_6;
			pAd->PortCfg.RtsRate = RATE_6;
		}
	}

	DBGPRINT(RT_DEBUG_TRACE, " MlmeUpdateTxRates (MaxDesire=%d, MaxSupport=%d, MaxTxRate=%d, Rate Switching =%d)\n",
			 RateIdToMbps[MaxDesire], RateIdToMbps[MaxSupport], RateIdToMbps[pAd->PortCfg.MaxTxRate], OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_TX_RATE_SWITCH_ENABLED));
	DBGPRINT(RT_DEBUG_TRACE, " MlmeUpdateTxRates (TxRate=%d, RtsRate=%d, BasicRateBitmap=0x%04x)\n",
			 RateIdToMbps[pAd->PortCfg.TxRate], RateIdToMbps[pAd->PortCfg.RtsRate], BasicRateBitmap);
}

VOID MlmeRadioOff(
	IN PRTMP_ADAPTER pAd)
{
	ULONG						i = 0;

	DBGPRINT(RT_DEBUG_TRACE, "===>MlmeRadioOff()\n");

	// Set LED Status first.
	RTMPSetLED(pAd, LED_RADIO_OFF);

	//
	// Since set flag fRTMP_ADAPTER_RADIO_OFF will diable RTUSBKickBulkOut function.
	// So before set flag fRTMP_ADAPTER_RADIO_OFF,
	// we should send a disassoc frame to our AP if neend.
	//
	if (!RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST))
	{
		// all calls now preceeded with disassoc request - for now - bb
		// Set Radio off flag will turn off RTUSBKickBulkOut function
		RTMP_SET_FLAG(pAd, fRTMP_ADAPTER_RADIO_OFF);
	}
	else
	{
		// Set Radio off flag will turn off RTUSBKickBulkOut function
		RTMP_SET_FLAG(pAd, fRTMP_ADAPTER_RADIO_OFF);
	}

	RTUSBRejectPendingPackets(pAd);//reject all NDIS packets waiting in TX queue
	MlmeSuspend(pAd, TRUE);

	// Disable Rx
	RT73WriteTXRXCSR0(pAd, TRUE, FALSE);
	RTUSBWriteMACRegister(pAd, MAC_CSR10, 0x00001818); // Turn off radio


	//Ask our device to complete any pending bulk in IRP.
	while ((atomic_read(&pAd->PendingRx ) > 0) ||
			(pAd->BulkOutPending[0] == TRUE) ||
			(pAd->BulkOutPending[1] == TRUE) ||
			(pAd->BulkOutPending[2] == TRUE) ||
			(pAd->BulkOutPending[3] == TRUE))
	{
		if (atomic_read(&pAd->PendingRx) > 0)
		{
			DBGPRINT(RT_DEBUG_TRACE,
					"- (%s) BulkIn IRP Pending!!!\n", __FUNCTION__);
			RTUSBStopRx(pAd);
		}

		if ((pAd->BulkOutPending[0] == TRUE) ||
			(pAd->BulkOutPending[1] == TRUE) ||
			(pAd->BulkOutPending[2] == TRUE) ||
			(pAd->BulkOutPending[3] == TRUE))
		{
			DBGPRINT(RT_DEBUG_TRACE,
					"- (%s) BulkOut IRP Pending!!!\n", __FUNCTION__);
			if (i == 0)
			{
				RTUSBCancelPendingBulkOutIRP(pAd);
				i++;
			}
		}

		msleep(500);	//RTMPusecDelay(500000);
	}

	// Clean up old bss table
	BssTableInit(&pAd->ScanTab);

	DBGPRINT(RT_DEBUG_TRACE, "<== MlmeRadioOff\n");
}

VOID MlmeRadioOn(
	IN PRTMP_ADAPTER pAd)
{
	DBGPRINT(RT_DEBUG_TRACE,"MlmeRadioOn()\n");

	// Turn on radio, Abort TX, Disable RX
	RTUSBWriteMACRegister(pAd, MAC_CSR10, 0x00000718);
	RT73WriteTXRXCSR0(pAd, TRUE, FALSE);

	// Clear Radio off flag
	RTMP_CLEAR_FLAG(pAd, fRTMP_ADAPTER_RADIO_OFF);

	RTUSBBulkReceive(pAd);

	// enable RX of MAC block
	RT73WriteTXRXCSR0(pAd, FALSE, TRUE);	// Staion not drop control frame will fail WiFi Certification.

	// Set LED
	RTMPSetLED(pAd, LED_RADIO_ON);

	// Switch to current channel, since during reset process, the connection should remains on.
	AsicSwitchChannel(pAd, pAd->PortCfg.Channel);
	AsicLockChannel(pAd, pAd->PortCfg.Channel);

	//
	// After Radio On, we should enqueue Reset MLME,
	// To make sure during Radio Off any MLME message enqueued will be reset.
	// After MLME Reset the routine RTUSBResumeMsduTransmission will also be call
	// To
	//
	MlmeResume(pAd);

} /* End MlmeRadioOn () */

// ===========================================================================================
// bss_table.c
// ===========================================================================================


/*! \brief initialize BSS table
 *	\param p_tab pointer to the table
 *	\return none
 *	\pre
 *	\post
 */
VOID BssTableInit(
	IN BSS_TABLE *Tab)
{
	int i;

	Tab->BssNr = 0;
	Tab->BssOverlapNr = 0;
	for (i = 0; i < MAX_LEN_OF_BSS_TABLE; i++)
	{
		memset(&Tab->BssEntry[i], 0, sizeof(BSS_ENTRY));
	}
}

/*! \brief search the BSS table by SSID
 *	\param p_tab pointer to the bss table
 *	\param ssid SSID string
 *	\return index of the table, BSS_NOT_FOUND if not in the table
 *	\pre
 *	\post
 *	\note search by sequential search
 */
ULONG BssTableSearch(
	IN BSS_TABLE *Tab,
	IN PUCHAR	 pBssid,
	IN UCHAR	 Channel)
{
	UCHAR i;

	for (i = 0; i < Tab->BssNr; i++)
	{
		//
		// Some AP that support A/B/G mode that may used the same BSSID on 11A and 11B/G.
		// We should distinguish this case.
		//
		if ((((Tab->BssEntry[i].Channel <= 14) && (Channel <= 14)) ||
			 ((Tab->BssEntry[i].Channel > 14) && (Channel > 14))) &&
			MAC_ADDR_EQUAL(Tab->BssEntry[i].Bssid, pBssid))
		{
			return i;
		}
	}
	return (ULONG)BSS_NOT_FOUND;
}

ULONG BssSsidTableSearch(
	IN BSS_TABLE *Tab,
	IN PUCHAR	 pBssid,
	IN PUCHAR	 pSsid,
	IN UCHAR	 SsidLen,
	IN UCHAR	 Channel)
{
	UCHAR i;

	for (i = 0; i < Tab->BssNr; i++)
	{
		//
		// Some AP that support A/B/G mode that may used the same BSSID on 11A and 11B/G.
		// We should distinguish this case.
		//
		if ((((Tab->BssEntry[i].Channel <= 14) && (Channel <= 14)) ||
			 ((Tab->BssEntry[i].Channel > 14) && (Channel > 14))) &&
			MAC_ADDR_EQUAL(Tab->BssEntry[i].Bssid, pBssid) &&
			SSID_EQUAL(pSsid, SsidLen, Tab->BssEntry[i].Ssid, Tab->BssEntry[i].SsidLen))
		{
			return i;
		}
	}
	return (ULONG)BSS_NOT_FOUND;
}

ULONG BssTableSearchWithSSID(
	IN BSS_TABLE *Tab,
	IN PUCHAR	 Bssid,
	IN PUCHAR	 pSsid,
	IN UCHAR	 SsidLen,
	IN UCHAR	 Channel)
{
	UCHAR i;

	for (i = 0; i < Tab->BssNr; i++)
	{
		if ((((Tab->BssEntry[i].Channel <= 14) && (Channel <= 14)) ||
			((Tab->BssEntry[i].Channel > 14) && (Channel > 14))) &&
			MAC_ADDR_EQUAL(&(Tab->BssEntry[i].Bssid), Bssid) &&
			(SSID_EQUAL(pSsid, SsidLen, Tab->BssEntry[i].Ssid, Tab->BssEntry[i].SsidLen) ||
			(NdisEqualMemory(pSsid, ZeroSsid, SsidLen)) ||
			(NdisEqualMemory(Tab->BssEntry[i].Ssid, ZeroSsid, Tab->BssEntry[i].SsidLen))))
		{
			return i;
		}
	}
	return (ULONG)BSS_NOT_FOUND;
}

VOID BssTableDeleteEntry(
	IN OUT	BSS_TABLE *Tab,
	IN		PUCHAR	  pBssid,
	IN		UCHAR	  Channel)
{
	UCHAR i, j;

	for (i = 0; i < Tab->BssNr; i++)
	{
		//printf("comparing %s and %s\n", p_tab->bss[i].ssid, ssid);
		if ((Tab->BssEntry[i].Channel == Channel) &&
			(MAC_ADDR_EQUAL(Tab->BssEntry[i].Bssid, pBssid)))
		{
			for (j = i; j < Tab->BssNr - 1; j++)
			{
				memcpy(&(Tab->BssEntry[j]), &(Tab->BssEntry[j + 1]), sizeof(BSS_ENTRY));
			}
			Tab->BssNr -= 1;
			return;
		}
	}
}

/*! \brief
 *	\param
 *	\return
 *	\pre
 *	\post
 */
VOID BssEntrySet(
	IN	PRTMP_ADAPTER	pAd,
	OUT BSS_ENTRY *pBss,
	IN PUCHAR pBssid,
	IN CHAR Ssid[],
	IN UCHAR SsidLen,
	IN UCHAR BssType,
	IN USHORT BeaconPeriod,
	IN PCF_PARM pCfParm,
	IN USHORT AtimWin,
	IN USHORT CapabilityInfo,
	IN UCHAR SupRate[],
	IN UCHAR SupRateLen,
	IN UCHAR ExtRate[],
	IN UCHAR ExtRateLen,
	IN UCHAR Channel,
	IN UCHAR Rssi,
	IN LARGE_INTEGER TimeStamp,
	IN UCHAR CkipFlag,
	IN PEDCA_PARM pEdcaParm,
	IN PQOS_CAPABILITY_PARM pQosCapability,
	IN PQBSS_LOAD_PARM pQbssLoad,
	IN UCHAR LengthVIE,
	IN PNDIS_802_11_VARIABLE_IEs pVIE)
{
	//pBss->LastBeaconRxTime = jiffies;     // MlmeCheckForRoaming related

	COPY_MAC_ADDR(pBss->Bssid, pBssid);
	// Default Hidden SSID to be TRUE, it will be turned to FALSE after coping SSID
	pBss->Hidden = 1;
	if (SsidLen > 0)
	{
		// For hidden SSID AP, it might send beacon with SSID len equal to 0
		// Or send beacon /probe response with SSID len matching real SSID length,
		// but SSID is all zero. such as "00-00-00-00" with length 4.
		// We have to prevent this case overwrite correct table
		if (NdisEqualMemory(Ssid, ZeroSsid, SsidLen) == 0)
		{
			memcpy(pBss->Ssid, Ssid, SsidLen);
			pBss->SsidLen = SsidLen;
			pBss->Hidden = 0;
		}
	}
	pBss->BssType = BssType;
	pBss->BeaconPeriod = BeaconPeriod;
	if (BssType == BSS_INFRA)
	{
		if (pCfParm->bValid)
		{
			pBss->CfpCount = pCfParm->CfpCount;
			pBss->CfpPeriod = pCfParm->CfpPeriod;
			pBss->CfpMaxDuration = pCfParm->CfpMaxDuration;
			pBss->CfpDurRemaining = pCfParm->CfpDurRemaining;
		}
	}
	else
	{
		pBss->AtimWin = AtimWin;
	}

	pBss->CapabilityInfo = CapabilityInfo;
	// The privacy bit indicate security is ON, it maight be WEP, TKIP or AES
	// Combine with AuthMode, they will decide the connection methods.
	pBss->Privacy = CAP_IS_PRIVACY_ON(pBss->CapabilityInfo);
	memcpy(pBss->SupRate, SupRate, SupRateLen);
	pBss->SupRateLen = SupRateLen;
	memcpy(pBss->ExtRate, ExtRate, ExtRateLen);
	pBss->ExtRateLen = ExtRateLen;
	pBss->Channel = Channel;
	pBss->Rssi = Rssi;
	// Update CkipFlag. if not exists, the value is 0x0
	pBss->CkipFlag = CkipFlag;

	// New for microsoft Fixed IEs
	memcpy(pBss->FixIEs.Timestamp, &TimeStamp, 8);
	pBss->FixIEs.BeaconInterval = BeaconPeriod;
	pBss->FixIEs.Capabilities = CapabilityInfo;

	if (LengthVIE != 0)
	{
		pBss->VarIELen = LengthVIE;
		memcpy(pBss->VarIEs, pVIE, pBss->VarIELen);
	}
	else
	{
		pBss->VarIELen = 0;
	}

	BssCipherParse(pBss);

	// new for QOS
	if (pEdcaParm)
		memcpy(&pBss->EdcaParm, pEdcaParm, sizeof(EDCA_PARM));
	else
		pBss->EdcaParm.bValid = FALSE;
	if (pQosCapability)
		memcpy(&pBss->QosCapability, pQosCapability, sizeof(QOS_CAPABILITY_PARM));
	else
		pBss->QosCapability.bValid = FALSE;
	if (pQbssLoad)
		memcpy(&pBss->QbssLoad, pQbssLoad, sizeof(QBSS_LOAD_PARM));
	else
		pBss->QbssLoad.bValid = FALSE;
}

/*!
 *	\brief insert an entry into the bss table
 *	\param p_tab The BSS table
 *	\param Bssid BSSID
 *	\param ssid SSID
 *	\param ssid_len Length of SSID
 *	\param bss_type
 *	\param beacon_period
 *	\param timestamp
 *	\param p_cf
 *	\param atim_win
 *	\param cap
 *	\param rates
 *	\param rates_len
 *	\param channel_idx
 *	\return none
 *	\pre
 *	\post
 *	\note If SSID is identical, the old entry will be replaced by the new one
 */
ULONG BssTableSetEntry(
	IN	PRTMP_ADAPTER	pAd,
	OUT BSS_TABLE *Tab,
	IN PUCHAR pBssid,
	IN CHAR Ssid[],
	IN UCHAR SsidLen,
	IN UCHAR BssType,
	IN USHORT BeaconPeriod,
	IN CF_PARM *CfParm,
	IN USHORT AtimWin,
	IN USHORT CapabilityInfo,
	IN UCHAR SupRate[],
	IN UCHAR SupRateLen,
	IN UCHAR ExtRate[],
	IN UCHAR ExtRateLen,
	IN UCHAR ChannelNo,
	IN UCHAR Rssi,
	IN LARGE_INTEGER TimeStamp,
	IN UCHAR CkipFlag,
	IN PEDCA_PARM pEdcaParm,
	IN PQOS_CAPABILITY_PARM pQosCapability,
	IN PQBSS_LOAD_PARM pQbssLoad,
	IN UCHAR LengthVIE,
	IN PNDIS_802_11_VARIABLE_IEs pVIE)
{
	ULONG	Idx;

	Idx = BssTableSearchWithSSID(Tab, pBssid,  Ssid, SsidLen, ChannelNo);
	if (Idx == BSS_NOT_FOUND)
	{
		if (Tab->BssNr >= MAX_LEN_OF_BSS_TABLE)
	    {
			//
			// It may happen when BSS Table was full.
			// The desired AP will not be added into BSS Table
			// In this case, if we found the desired AP then overwrite BSS Table.
			//
            if(!OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_MEDIA_STATE_CONNECTED))
			{
				if (MAC_ADDR_EQUAL(pAd->MlmeAux.Bssid, pBssid) ||
					SSID_EQUAL(pAd->MlmeAux.Ssid, pAd->MlmeAux.SsidLen, Ssid, SsidLen))
				{
					Idx = Tab->BssOverlapNr;
					BssEntrySet(pAd, &Tab->BssEntry[Idx], pBssid, Ssid, SsidLen, BssType, BeaconPeriod,
								CfParm, AtimWin, CapabilityInfo, SupRate, SupRateLen, ExtRate, ExtRateLen,
								ChannelNo, Rssi, TimeStamp, CkipFlag, pEdcaParm, pQosCapability, pQbssLoad, LengthVIE, pVIE);
					Tab->BssOverlapNr = (Tab->BssOverlapNr++) % MAX_LEN_OF_BSS_TABLE;
				}
				return Idx;
			}
			else
				return BSS_NOT_FOUND;
		}

		Idx = Tab->BssNr;
		BssEntrySet(pAd, &Tab->BssEntry[Idx], pBssid, Ssid, SsidLen, BssType, BeaconPeriod,
					CfParm, AtimWin, CapabilityInfo, SupRate, SupRateLen, ExtRate, ExtRateLen,
					ChannelNo, Rssi, TimeStamp, CkipFlag, pEdcaParm, pQosCapability, pQbssLoad, LengthVIE, pVIE);
		Tab->BssNr++;
	}
	else
	{
		BssEntrySet(pAd, &Tab->BssEntry[Idx], pBssid, Ssid, SsidLen, BssType, BeaconPeriod,
					CfParm, AtimWin, CapabilityInfo, SupRate, SupRateLen, ExtRate, ExtRateLen,
					ChannelNo, Rssi, TimeStamp, CkipFlag, pEdcaParm, pQosCapability, pQbssLoad, LengthVIE, pVIE);
	}

	return Idx;
}

BOOLEAN RTMPCheckAKM(
    IN NDIS_802_11_AUTHENTICATION_MODE auth,
    IN BSS_ENTRY *pBss)
{
    switch (auth)
    {
        case Ndis802_11AuthModeWPA:
    	    if(pBss->AuthBitMode & WPA1AKMBIT)
	            return TRUE;
	        else
	            return FALSE;
        case Ndis802_11AuthModeWPAPSK:
    	    if(pBss->AuthBitMode & WPA1PSKAKMBIT)
	            return TRUE;
	        else
	            return FALSE;
        case Ndis802_11AuthModeWPA2:
       	    if(pBss->AuthBitMode & WPA2AKMBIT)
	            return TRUE;
	        else
	            return FALSE;
        case Ndis802_11AuthModeWPA2PSK:
            if(pBss->AuthBitMode & WPA2PSKAKMBIT)
	            return TRUE;
	        else
	            return FALSE;
        default:
            return FALSE;
    }
}

VOID BssTableSsidSort(
	IN	PRTMP_ADAPTER	pAd,
	OUT BSS_TABLE *OutTab,
	IN	CHAR Ssid[],
	IN	UCHAR SsidLen)
{
	INT i;
	BssTableInit(OutTab);

	for (i = 0; i < pAd->ScanTab.BssNr; i++)
	{
		BSS_ENTRY *pInBss = &pAd->ScanTab.BssEntry[i];

		if ((pInBss->BssType == pAd->PortCfg.BssType) &&
			SSID_EQUAL(Ssid, SsidLen, pInBss->Ssid, pInBss->SsidLen))
		{
			BSS_ENTRY *pOutBss = &OutTab->BssEntry[OutTab->BssNr];

			// New for WPA2
			// Check the Authmode first
			if (pAd->PortCfg.AuthMode >= Ndis802_11AuthModeWPA)
			{
				if (pAd->PortCfg.AuthMode == Ndis802_11AuthModeWPANone)
				{
					if (pAd->PortCfg.AuthMode != pInBss->AuthMode)
						continue;// None matched
				}
				else
				{
					// Check AuthMode and AuthBitMode for matching, in case AP support dual-mode
					if(!RTMPCheckAKM(pAd->PortCfg.AuthMode,pInBss))
						continue;// None matched
				}

				// Check cipher suite, AP must have more secured cipher than station setting
				if ((pAd->PortCfg.AuthMode == Ndis802_11AuthModeWPA) || (pAd->PortCfg.AuthMode == Ndis802_11AuthModeWPAPSK))
				{
					// If it's not mixed mode, we should only let BSS pass with the same encryption
					if (pInBss->WPA.bMixMode == FALSE)
						if (pAd->PortCfg.WepStatus != pInBss->WPA.GroupCipher)
							continue;

					// check group cipher
					if (pAd->PortCfg.WepStatus < pInBss->WPA.GroupCipher)
						continue;

					// check pairwise cipher, skip if none matched
					// If profile set to AES, let it pass without question.
					// If profile set to TKIP, we must find one mateched
					if ((pAd->PortCfg.WepStatus == Ndis802_11Encryption2Enabled) &&
						(pAd->PortCfg.WepStatus != pInBss->WPA.PairCipher) &&
						(pAd->PortCfg.WepStatus != pInBss->WPA.PairCipherAux))
						continue;
				}
				else if ((pAd->PortCfg.AuthMode == Ndis802_11AuthModeWPA2) || (pAd->PortCfg.AuthMode == Ndis802_11AuthModeWPA2PSK))
				{
					// If it's not mixed mode, we should only let BSS pass with the same encryption
					if (pInBss->WPA2.bMixMode == FALSE)
						if (pAd->PortCfg.WepStatus != pInBss->WPA2.GroupCipher)
							continue;

					// check group cipher
					if (pAd->PortCfg.WepStatus < pInBss->WPA2.GroupCipher)
						continue;

					// check pairwise cipher, skip if none matched
					// If profile set to AES, let it pass without question.
					// If profile set to TKIP, we must find one mateched
					if ((pAd->PortCfg.WepStatus == Ndis802_11Encryption2Enabled) &&
						(pAd->PortCfg.WepStatus != pInBss->WPA2.PairCipher) &&
						(pAd->PortCfg.WepStatus != pInBss->WPA2.PairCipherAux))
						continue;
				}
			}
			// Bss Type matched, SSID matched.
			// We will check wepstatus for qualification Bss
			else if (pAd->PortCfg.WepStatus != pInBss->WepStatus)
			{
				DBGPRINT(RT_DEBUG_TRACE,"PortCfg.WepStatus=%d, while pInBss->WepStatus=%d\n", pAd->PortCfg.WepStatus, pInBss->WepStatus);
				continue;
			}

			// Since the AP is using hidden SSID, and we are trying to connect to ANY
			// It definitely will fail. So, skip it.
			// CCX also require not even try to connect it!!
			if (SsidLen == 0)
				continue;

			// copy matching BSS from InTab to OutTab
			memcpy(pOutBss, pInBss, sizeof(BSS_ENTRY));

			OutTab->BssNr++;
		}
		else if ((pInBss->BssType == pAd->PortCfg.BssType) && (SsidLen == 0))
		{
			BSS_ENTRY *pOutBss = &OutTab->BssEntry[OutTab->BssNr];

			// New for WPA2
			// Check the Authmode first
			if (pAd->PortCfg.AuthMode >= Ndis802_11AuthModeWPA)
			{
				// Check AuthMode and AuthBitMode for matching, in case AP support dual-mode
				if(!RTMPCheckAKM(pAd->PortCfg.AuthMode,pInBss))
					continue;   // None matched

				// Check cipher suite, AP must have more secured cipher than station setting
				if ((pAd->PortCfg.AuthMode == Ndis802_11AuthModeWPA) || (pAd->PortCfg.AuthMode == Ndis802_11AuthModeWPAPSK))
				{
					// If it's not mixed mode, we should only let BSS pass with the same encryption
					if (pInBss->WPA.bMixMode == FALSE)
						if (pAd->PortCfg.WepStatus != pInBss->WPA.GroupCipher)
							continue;

					// check group cipher
					if (pAd->PortCfg.WepStatus < pInBss->WPA.GroupCipher)
						continue;

					// check pairwise cipher, skip if none matched
					// If profile set to AES, let it pass without question.
					// If profile set to TKIP, we must find one mateched
					if ((pAd->PortCfg.WepStatus == Ndis802_11Encryption2Enabled) &&
						(pAd->PortCfg.WepStatus != pInBss->WPA.PairCipher) &&
						(pAd->PortCfg.WepStatus != pInBss->WPA.PairCipherAux))
						continue;
				}
				else if ((pAd->PortCfg.AuthMode == Ndis802_11AuthModeWPA2) || (pAd->PortCfg.AuthMode == Ndis802_11AuthModeWPA2PSK))
				{
					// If it's not mixed mode, we should only let BSS pass with the same encryption
					if (pInBss->WPA2.bMixMode == FALSE)
						if (pAd->PortCfg.WepStatus != pInBss->WPA2.GroupCipher)
							continue;

					// check group cipher
					if (pAd->PortCfg.WepStatus < pInBss->WPA2.GroupCipher)
						continue;

					// check pairwise cipher, skip if none matched
					// If profile set to AES, let it pass without question.
					// If profile set to TKIP, we must find one mateched
					if ((pAd->PortCfg.WepStatus == Ndis802_11Encryption2Enabled) &&
						(pAd->PortCfg.WepStatus != pInBss->WPA2.PairCipher) &&
						(pAd->PortCfg.WepStatus != pInBss->WPA2.PairCipherAux))
						continue;
				}
			}
			// Bss Type matched, SSID matched.
			// We will check wepstatus for qualification Bss
			else if (pAd->PortCfg.WepStatus != pInBss->WepStatus)
					continue;

			// copy matching BSS from InTab to OutTab
			memcpy(pOutBss, pInBss, sizeof(BSS_ENTRY));

			OutTab->BssNr++;
		}

		if (OutTab->BssNr >= MAX_LEN_OF_BSS_TABLE)
			break;

	}

	BssTableSortByRssi(OutTab);

	if (OutTab->BssNr > 0)
	{
		if (pAd->PortCfg.AuthMode == Ndis802_11AuthModeWPA2PSK)
			RTMPMakeRSNIE(pAd, OutTab->BssEntry[0].WPA2.GroupCipher);
		else if (pAd->PortCfg.AuthMode == Ndis802_11AuthModeWPAPSK)
			RTMPMakeRSNIE(pAd, OutTab->BssEntry[0].WPA.GroupCipher);
	}

}

VOID BssTableSortByRssi(
	IN OUT BSS_TABLE *OutTab)
{
	INT 	  i, j;
	BSS_ENTRY TmpBss;

	for (i = 0; i < OutTab->BssNr - 1; i++)
	{
		for (j = i+1; j < OutTab->BssNr; j++)
		{
			if (OutTab->BssEntry[j].Rssi > OutTab->BssEntry[i].Rssi)
			{
				memcpy(&TmpBss, &OutTab->BssEntry[j], sizeof(BSS_ENTRY));
				memcpy(&OutTab->BssEntry[j], &OutTab->BssEntry[i], sizeof(BSS_ENTRY));
				memcpy(&OutTab->BssEntry[i], &TmpBss, sizeof(BSS_ENTRY));
			}
		}
	}
}

VOID BssCipherParse(
	IN OUT	PBSS_ENTRY	pBss)
{
	PEID_STRUCT 		 pEid;
	PUCHAR				pTmp;
	RSN_IE_HEADER_STRUCT			RsnHeader;
	PCIPHER_SUITE_STRUCT			pCipher;
	PAKM_SUITE_STRUCT				pAKM;
	USHORT							Count;
	INT								Length;
	NDIS_802_11_ENCRYPTION_STATUS	TmpCipher;

	//
	// WepStatus will be reset later, if AP announce TKIP or AES on the beacon frame.
	//
	if (pBss->Privacy)
	{
		pBss->WepStatus 	= Ndis802_11WEPEnabled;
	}
	else
	{
		pBss->WepStatus 	= Ndis802_11WEPDisabled;
	}
	// Set default to disable & open authentication before parsing variable IE
	pBss->AuthMode		= Ndis802_11AuthModeOpen;
//	pBss->AuthModeAux	= Ndis802_11AuthModeOpen;
	pBss->AuthBitMode   = 0;

	// Init WPA setting
	pBss->WPA.PairCipher	= Ndis802_11WEPDisabled;
	pBss->WPA.PairCipherAux = Ndis802_11WEPDisabled;
	pBss->WPA.GroupCipher	= Ndis802_11WEPDisabled;
	pBss->WPA.RsnCapability = 0;
	pBss->WPA.bMixMode		= FALSE;

	// Init WPA2 setting
	pBss->WPA2.PairCipher	 = Ndis802_11WEPDisabled;
	pBss->WPA2.PairCipherAux = Ndis802_11WEPDisabled;
	pBss->WPA2.GroupCipher	 = Ndis802_11WEPDisabled;
	pBss->WPA2.RsnCapability = 0;
	pBss->WPA2.bMixMode 	 = FALSE;
	Length = (INT) pBss->VarIELen;

	while (Length > 0)
	{
		// Parse cipher suite base on WPA1 & WPA2, they should be parsed differently
		pTmp = ((PUCHAR) pBss->VarIEs) + pBss->VarIELen - Length;
		pEid = (PEID_STRUCT) pTmp;
		switch (pEid->Eid)
		{
			case IE_WPA:
				if (NdisEqualMemory(pEid->Octet, WPA_OUI, 4) != 1)
				{
					// if unsupported vendor specific IE
					break;
				}
				// Skip OUI, version, and multicast suite
				// This part should be improved in the future when AP supported multiple cipher suite.
				// For now, it's OK since almost all APs have fixed cipher suite supported.
				// pTmp = (PUCHAR) pEid->Octet;
				pTmp   += 11;

				// Cipher Suite Selectors from Spec P802.11i/D3.2 P26.
				//	Value	   Meaning
				//	0			None
				//	1			WEP-40
				//	2			Tkip
				//	3			WRAP
				//	4			AES
				//	5			WEP-104
				// Parse group cipher
				switch (*pTmp)
				{
					case 1:
					case 5:	// Although WEP is not allowed in WPA related auth mode, we parse it anyway
						pBss->WPA.GroupCipher = Ndis802_11Encryption1Enabled;
						break;
					case 2:
						pBss->WPA.GroupCipher = Ndis802_11Encryption2Enabled;
						break;
					case 4:
						pBss->WPA.GroupCipher = Ndis802_11Encryption3Enabled;
						break;
					default:
						break;
				}
				// number of unicast suite
				pTmp   += 1;

				// skip all unicast cipher suites
				//Count = *(PUSHORT) pTmp;
				memcpy(&Count, pTmp, sizeof(USHORT));
#ifdef BIG_ENDIAN
				Count = SWAP16(Count);
#endif
				pTmp   += sizeof(USHORT);

				// Parsing all unicast cipher suite
				while (Count > 0)
				{
					// Skip OUI
					pTmp += 3;
					TmpCipher = Ndis802_11WEPDisabled;
					switch (*pTmp)
					{
						case 1:
						case 5: // Although WEP is not allowed in WPA related auth mode, we parse it anyway
							TmpCipher = Ndis802_11Encryption1Enabled;
							break;
						case 2:
							TmpCipher = Ndis802_11Encryption2Enabled;
							break;
						case 4:
							TmpCipher = Ndis802_11Encryption3Enabled;
							break;
						default:
							break;
					}
					if (TmpCipher > pBss->WPA.PairCipher)
					{
						// Move the lower cipher suite to PairCipherAux
						pBss->WPA.PairCipherAux = pBss->WPA.PairCipher;
						pBss->WPA.PairCipher	= TmpCipher;
					}
					else
					{
						pBss->WPA.PairCipherAux = TmpCipher;
					}
					pTmp++;
					Count--;
				}

				// 4. get AKM suite counts
				//Count   = *(PUSHORT) pTmp;
				memcpy(&Count, pTmp, sizeof(USHORT));
#ifdef BIG_ENDIAN
				Count = SWAP16(Count);
#endif
				pTmp   += sizeof(USHORT);

				while(Count >0)
				{
					pTmp   += 3;

					switch (*pTmp)
					{
						case 1:
							// Set AP support WPA1 mode
							pBss->AuthBitMode|=WPA1AKMBIT;
							pBss->AuthMode	  = Ndis802_11AuthModeWPA;
							break;
						case 2:
							// Set AP support WPA1PSK
							pBss->AuthBitMode|=WPA1PSKAKMBIT;
							pBss->AuthMode	  = Ndis802_11AuthModeWPAPSK;
							break;
						default:
							break;
					}
					pTmp++;
					Count--;
				}


				// Fixed for WPA-None
				if (pBss->BssType == BSS_ADHOC)
				{
					pBss->AuthMode	  = Ndis802_11AuthModeWPANone;
//					pBss->AuthModeAux = Ndis802_11AuthModeWPANone;
					pBss->WepStatus   = pBss->WPA.GroupCipher;
					// Patched bugs for old driver
					if (pBss->WPA.PairCipherAux == Ndis802_11WEPDisabled)
						pBss->WPA.PairCipherAux = pBss->WPA.GroupCipher;
				}
				else
					pBss->WepStatus   = pBss->WPA.PairCipher;

				// Check the Pair & Group, if different, turn on mixed mode flag
				if (pBss->WPA.GroupCipher != pBss->WPA.PairCipher)
					pBss->WPA.bMixMode = TRUE;

				break;

			case IE_RSN:
				memcpy(&RsnHeader, pTmp, sizeof(PRSN_IE_HEADER_STRUCT));

				// 0. Version must be 1
#ifdef BIG_ENDIAN
				RsnHeader.Version = SWAP16(RsnHeader.Version);
#endif
				if (RsnHeader.Version != 1)
					break;
				pTmp   += sizeof(RSN_IE_HEADER_STRUCT);

				// 1. Check group cipher
				pCipher = (PCIPHER_SUITE_STRUCT) pTmp;
				if (!RTMPEqualMemory(pTmp, RSN_OUI, 3))
					break;

				// Parse group cipher
				switch (pCipher->Type)
				{
					case 1:
					case 5:	// Although WEP is not allowed in WPA related auth mode, we parse it anyway
						pBss->WPA2.GroupCipher = Ndis802_11Encryption1Enabled;
						break;
					case 2:
						pBss->WPA2.GroupCipher = Ndis802_11Encryption2Enabled;
						break;
					case 4:
						pBss->WPA2.GroupCipher = Ndis802_11Encryption3Enabled;
						break;
					default:
						break;
				}
				// set to correct offset for next parsing
				pTmp   += sizeof(CIPHER_SUITE_STRUCT);

				// 2. Get pairwise cipher counts
				//Count = *(PUSHORT) pTmp;
				memcpy(&Count, pTmp, sizeof(USHORT));
#ifdef BIG_ENDIAN
				Count = SWAP16(Count);
#endif
				pTmp   += sizeof(USHORT);

				// 3. Get pairwise cipher
				// Parsing all unicast cipher suite
				while (Count > 0)
				{
					// Skip OUI
					pCipher = (PCIPHER_SUITE_STRUCT) pTmp;
					TmpCipher = Ndis802_11WEPDisabled;
					switch (pCipher->Type)
					{
						case 1:
						case 5: // Although WEP is not allowed in WPA related auth mode, we parse it anyway
							TmpCipher = Ndis802_11Encryption1Enabled;
							break;
						case 2:
							TmpCipher = Ndis802_11Encryption2Enabled;
							break;
						case 4:
							TmpCipher = Ndis802_11Encryption3Enabled;
							break;
						default:
							break;
					}
					if (TmpCipher > pBss->WPA2.PairCipher)
					{
						// Move the lower cipher suite to PairCipherAux
						pBss->WPA2.PairCipherAux = pBss->WPA2.PairCipher;
						pBss->WPA2.PairCipher	 = TmpCipher;
					}
					else
					{
						pBss->WPA2.PairCipherAux = TmpCipher;
					}
					pTmp += sizeof(CIPHER_SUITE_STRUCT);
					Count--;
				}

				// 4. get AKM suite counts
				//Count   = *(PUSHORT) pTmp;
				memcpy(&Count, pTmp, sizeof(USHORT));
#ifdef BIG_ENDIAN
				Count = SWAP16(Count);
#endif
				pTmp   += sizeof(USHORT);

				// 5. Get AKM ciphers
				pAKM = (PAKM_SUITE_STRUCT) pTmp;
				if (!RTMPEqualMemory(pTmp, RSN_OUI, 3))
					break;

				while(Count >0)
				{
					pTmp   += 3;

					switch (*pTmp)
					{
						case 1:
							// Set AP support WPA2 mode
							pBss->AuthBitMode|=WPA2AKMBIT;
							pBss->AuthMode	  = Ndis802_11AuthModeWPA2;
							break;
						case 2:
							// Set AP support WPA2PSK
							pBss->AuthBitMode|=WPA2PSKAKMBIT;
							pBss->AuthMode	  = Ndis802_11AuthModeWPA2PSK;
							break;
						default:
							break;
					}
					pTmp++;
					Count--;
				}


				// Fixed for WPA-None
				if (pBss->BssType == BSS_ADHOC)
				{
					pBss->AuthMode = Ndis802_11AuthModeWPANone;
//					pBss->AuthModeAux = Ndis802_11AuthModeWPANone;
					pBss->WPA.PairCipherAux = pBss->WPA2.PairCipherAux;
					pBss->WPA.GroupCipher	= pBss->WPA2.GroupCipher;
					pBss->WepStatus 		= pBss->WPA.GroupCipher;
					// Patched bugs for old driver
					if (pBss->WPA.PairCipherAux == Ndis802_11WEPDisabled)
						pBss->WPA.PairCipherAux = pBss->WPA.GroupCipher;
				}
				pBss->WepStatus   = pBss->WPA2.PairCipher;

				// 6. Get RSN capability
				//pBss->WPA2.RsnCapability = *(PUSHORT) pTmp;
				memcpy(&pBss->WPA2.RsnCapability, pTmp, sizeof(USHORT));
#ifdef BIG_ENDIAN
				pBss->WPA2.RsnCapability = SWAP16(pBss->WPA2.RsnCapability);
#endif
				pTmp += sizeof(USHORT);

				// Check the Pair & Group, if different, turn on mixed mode flag
				if (pBss->WPA2.GroupCipher != pBss->WPA2.PairCipher)
					pBss->WPA2.bMixMode = TRUE;

				break;

			default:
				break;
		}
		Length -= (pEid->Len + 2);
	}
}


// ===========================================================================================
// mac_table.c
// ===========================================================================================

/*! \brief generates a random mac address value for IBSS BSSID
 *	\param Addr the bssid location
 *	\return none
 *	\pre
 *	\post
 */
VOID MacAddrRandomBssid(
	IN PRTMP_ADAPTER pAd,
	OUT PUCHAR pAddr)
{
	INT i;

	for (i = 0; i < MAC_ADDR_LEN; i++)
	{
		pAddr[i] = RandomByte(pAd);
	}

	pAddr[0] = (pAddr[0] & 0xfe) | 0x02;  // the first 2 bits must be 01xxxxxxxx
}

/*! \brief init the management mac frame header
 *	\param p_hdr mac header
 *	\param subtype subtype of the frame
 *	\param p_ds destination address, don't care if it is a broadcast address
 *	\return none
 *	\pre the station has the following information in the pAd->UserCfg
 *	 - bssid
 *	 - station address
 *	\post
 *	\note this function initializes the following field
 */
VOID MgtMacHeaderInit(
	IN	PRTMP_ADAPTER	pAd,
	IN OUT PHEADER_802_11 pHdr80211,
	IN UCHAR SubType,
	IN UCHAR ToDs,
	IN PUCHAR pDA,
	IN PUCHAR pBssid)
{
	memset(pHdr80211, 0, sizeof(HEADER_802_11));
	pHdr80211->FC.Type = BTYPE_MGMT;
	pHdr80211->FC.SubType = SubType;
	pHdr80211->FC.ToDs = ToDs;
	COPY_MAC_ADDR(pHdr80211->Addr1, pDA);
	COPY_MAC_ADDR(pHdr80211->Addr2, pAd->CurrentAddress);
	COPY_MAC_ADDR(pHdr80211->Addr3, pBssid);
}

// ===========================================================================================
// mem_mgmt.c
// ===========================================================================================

/*!***************************************************************************
 * This routine build an outgoing frame, and fill all information specified
 * in argument list to the frame body. The actual frame size is the summation
 * of all arguments.
 * input params:
 *		Buffer - pointer to a pre-allocated memory segment
 *		args - a list of <int arg_size, arg> pairs.
 *		NOTE NOTE NOTE!!!! the last argument must be NULL, otherwise this
 *						   function will FAIL!!!
 * return:
 *		Size of the buffer
 * usage:
 *		MakeOutgoingFrame(Buffer, output_length, 2, &fc, 2, &dur, 6, p_addr1, 6,p_addr2, END_OF_ARGS);
 ****************************************************************************/
ULONG MakeOutgoingFrame(
	OUT CHAR *Buffer,
	OUT ULONG *FrameLen, ...)
{
	CHAR   *p;
	int 	leng;
	ULONG	TotLeng;
	va_list Args;

	// calculates the total length
	TotLeng = 0;
	va_start(Args, FrameLen);
	do
	{
		leng = va_arg(Args, int);
		if (leng == END_OF_ARGS)
		{
			break;
		}
		p = va_arg(Args, PVOID);
		memcpy(&Buffer[TotLeng], p, leng);
		TotLeng = TotLeng + leng;
	} while(TRUE);

	va_end(Args); /* clean up */
	*FrameLen = TotLeng;
	return TotLeng;
}

// ===========================================================================================
// mlme_queue.c
// ===========================================================================================

/*! \brief	Initialize The MLME Queue, used by MLME Functions
 *	\param	*Queue	   The MLME Queue
 *	\return Always	   Return NDIS_STATE_SUCCESS in this implementation
 *	\pre
 *	\post
 *	\note	Because this is done only once (at the init stage), no need to be locked
 */
NDIS_STATUS MlmeQueueInit(
	IN MLME_QUEUE *Queue)
{
	INT i;
	unsigned long			flags;

	NdisAcquireSpinLock(&(Queue->Lock));

	Queue->Num	= 0;
	Queue->Head = 0;
	Queue->Tail = 0;

	for (i = 0; i < MAX_LEN_OF_MLME_QUEUE; i++)
	{
		Queue->Entry[i].Occupied = FALSE;
		Queue->Entry[i].MsgLen = 0;
		memset(Queue->Entry[i].Msg, 0, MAX_LEN_OF_MLME_BUFFER);
	}
	NdisReleaseSpinLock(&(Queue->Lock));

	return NDIS_STATUS_SUCCESS;
}

/*! \brief	 Enqueue a message for other threads, if they want to send messages to MLME thread
 *	\param	*Queue	  The MLME Queue
 *	\param	 Machine  The State Machine Id
 *	\param	 MsgType  The Message Type
 *	\param	 MsgLen   The Message length
 *	\param	*Msg	  The message pointer
 *	\return  TRUE if enqueue is successful, FALSE if the queue is full
 *	\pre
 *	\post
 *	\note	 The message has to be initialized
 */
BOOLEAN MlmeEnqueue(
	IN	PRTMP_ADAPTER	pAd,
	IN ULONG Machine,
	IN ULONG MsgType,
	IN ULONG MsgLen,
	IN VOID *Msg)
{
	INT Tail;
	MLME_QUEUE	*Queue = (MLME_QUEUE *)&pAd->Mlme.Queue;
	unsigned long		flags;

	// Do nothing if the driver is starting halt state.
	// This might happen when timer already been fired before cancel timer with mlmehalt
	if (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_HALT_IN_PROGRESS))
		return FALSE;

	// First check the size, it MUST not exceed the mlme queue size
	if (MsgLen > MAX_LEN_OF_MLME_BUFFER)
	{
		DBGPRINT_ERR("MlmeEnqueue: msg too large, size = %d \n", MsgLen);
		return FALSE;
	}

	if (pAd->MLMEThr_pid > 0)
	{
		NdisAcquireSpinLock(&Queue->Lock);
		if (Queue->Num == MAX_LEN_OF_MLME_QUEUE) {
			NdisReleaseSpinLock(&Queue->Lock);
			DBGPRINT_ERR("MlmeEnqueue: full, msg dropped and may corrupt MLME\n");
			return FALSE;
		}
		// If another context preempts us, it uses the next element - sic. bb
		Tail = Queue->Tail++;
		Queue->Tail %= MAX_LEN_OF_MLME_QUEUE;
		Queue->Num++;

	// We guard against Ben Hutchings' incomplete queue element problem by not
	// setting the Occupied flag until the memcpy is done. The ocurrence of a
	// refresh cycle during a copy can stretch the time by up to 100 usec
	// (well, quite a few usec, anyway); not good when interrupts are disabled.
	// Note that this can leave a bubble in the queue, but it will have
	// disappeared by the time this thread gets around to calling MlmeHandler.
	// All items will be handled in their proper order, but possibly not in the
	// context in which they were added. - bb
		NdisReleaseSpinLock(&Queue->Lock);
		DBGPRINT(RT_DEBUG_INFO, "MlmeEnqueue, num=%d\n",Queue->Num);

		Queue->Entry[Tail].Machine = Machine;
		Queue->Entry[Tail].MsgType = MsgType;
		Queue->Entry[Tail].MsgLen  = MsgLen;
		memcpy(Queue->Entry[Tail].Msg, Msg, MsgLen);

		//MlmeHandler will stop when it finds this false.
    	smp_wmb();
    	Queue->Entry[Tail].Occupied = TRUE;

	}

	return TRUE;
}

/*! \brief	 This function is used when Recv gets a MLME message
 *	\param	*Queue			 The MLME Queue
 *	\param	 Rssi			 The receiving RSSI strength
 *	\param	 MsgLen 		 The length of the message
 *	\param	*Msg			 The message pointer
 *	\return  TRUE if everything ok, FALSE otherwise (like Queue Full)
 *	\pre
 *	\post
 */
BOOLEAN MlmeEnqueueForRecv(
	IN	PRTMP_ADAPTER	pAd,
	IN UCHAR Rssi,
	IN ULONG MsgLen,
	IN VOID *Msg,
	IN UCHAR Signal)
{
	INT 			Tail, Machine;
	PFRAME_802_11	pFrame = (PFRAME_802_11)Msg;
	ULONG			MsgType;
	MLME_QUEUE		*Queue = (MLME_QUEUE *)&pAd->Mlme.Queue;
	unsigned long	flags;

	if((pAd->MLMEThr_pid <= 0)) {
		DBGPRINT_ERR("MlmeEnqueueForRecv: MLME Thread inactive\n");
		return FALSE;
	}
	// Do nothing if the driver is starting halt state.
	// This might happen when timer already been fired before cancel timer with mlmehalt
	if (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_HALT_IN_PROGRESS)) {
		DBGPRINT(RT_DEBUG_INFO, "MlmeEnqueueForRecv: Halt in progress\n");
		return FALSE;
	}
	// First check the size, it MUST not exceed the mlme queue size
	if (MsgLen > MAX_LEN_OF_MLME_BUFFER)
	{
		DBGPRINT_ERR("MlmeEnqueueForRecv: frame too large, size = %d \n", MsgLen);
		return FALSE;
	}

	if (!MsgTypeSubst(pAd, pFrame, &Machine, &MsgType))
	{
		DBGPRINT_ERR("MlmeEnqueueForRecv: un-recongnized mgmt->subtype=%d\n",pFrame->Hdr.FC.SubType);
		return FALSE;
	}

	if ((Machine == SYNC_STATE_MACHINE)&&(MsgType == MT2_PEER_PROBE_RSP)&&(pAd->PortCfg.bGetAPConfig))
	{
		if (pAd->Mlme.SyncMachine.CurrState == SYNC_IDLE)
		{
			CHAR CfgData[MAX_CFG_BUFFER_LEN+1] = {0};
			if (BackDoorProbeRspSanity(pAd, Msg, MsgLen, CfgData))
			{
				DBGPRINT(RT_DEBUG_INFO,
						"MlmeEnqueueForRecv: CfgData(len:%d):\n%s\n",
						(int)strlen(CfgData), CfgData);
				pAd->PortCfg.bGetAPConfig = FALSE;
			}
		}
	}

	NdisAcquireSpinLock(&Queue->Lock);
	if (Queue->Num == MAX_LEN_OF_MLME_QUEUE) {
		NdisReleaseSpinLock(&Queue->Lock);
		DBGPRINT_ERR("MlmeEnqueueForRecv: full and dropped\n");
		return FALSE;
	}
	Tail = Queue->Tail++;
	Queue->Tail %= MAX_LEN_OF_MLME_QUEUE;
	Queue->Num++;
	NdisReleaseSpinLock(&Queue->Lock);
	DBGPRINT(RT_DEBUG_INFO, "MlmeEnqueueForRecv, num=%d\n",Queue->Num);

	// OK, we got all the informations, it is time to put things into queue
	// See MlmeEnqueue note for use of Occupied flag.
	Queue->Entry[Tail].Machine = Machine;
	Queue->Entry[Tail].MsgType = MsgType;
	Queue->Entry[Tail].MsgLen  = MsgLen;
	Queue->Entry[Tail].Rssi = Rssi;
	Queue->Entry[Tail].Signal = Signal;
	Queue->Entry[Tail].bReqIsFromNdis = FALSE;
	Queue->Entry[Tail].Channel = pAd->LatchRfRegs.Channel;
	memcpy(Queue->Entry[Tail].Msg, Msg, MsgLen);
    smp_wmb();
    Queue->Entry[Tail].Occupied = TRUE;

	return TRUE;
}

/*! \brief   Get the first message from the MLME Queue
 * 			WARNING: Must be call with Mlme.Queue.Lock held
 *  \param  *Queue    The MLME Queue
 *  \param  *Elem     The message dequeued from MLME Queue
 *  \return  TRUE if the Elem contains something, FALSE otherwise
 *  \pre
 *  \post
 */
BOOLEAN MlmeGetHead(
    IN MLME_QUEUE *Queue,
    OUT MLME_QUEUE_ELEM **Elem)
{
    if (Queue->Num == 0)
	    return FALSE;
    *Elem = &Queue->Entry[Queue->Head];
    return TRUE;
}

/*! \brief   Remove the first message from the MLME Queue
 * 			WARNING: Must be call with Mlme.Queue.Lock held
 *  \param  *Queue    The MLME Queue
 *  \return  TRUE if a message was removed, FALSE if the queue was empty
 *  \pre
 *  \post
 */
BOOLEAN MlmeDequeue(
    IN MLME_QUEUE *Queue)
{
    if (Queue->Num == 0)
	    return FALSE;
    Queue->Head = (Queue->Head + 1) % MAX_LEN_OF_MLME_QUEUE;
    Queue->Num--;
    DBGPRINT(RT_DEBUG_INFO, "MlmeDequeue, num=%d\n",Queue->Num);

    return TRUE;
}

VOID MlmeRestartStateMachine(
	IN	PRTMP_ADAPTER	pAd)
{
	DBGPRINT(RT_DEBUG_TRACE, "!!! MlmeRestartStateMachine !!!\n");

	RTMP_SET_FLAG(pAd, fRTMP_ADAPTER_MLME_RESET_IN_PROGRESS);

	// Cancel all timer events
	// Be careful to cancel new added timer
	cancelSMTimers(pAd);

	// Set all state machines back IDLE
	pAd->Mlme.CntlMachine.CurrState    = CNTL_IDLE;
	pAd->Mlme.AssocMachine.CurrState   = ASSOC_IDLE;
	pAd->Mlme.AuthMachine.CurrState    = AUTH_REQ_IDLE;
	pAd->Mlme.AuthRspMachine.CurrState = AUTH_RSP_IDLE;
	pAd->Mlme.SyncMachine.CurrState    = SYNC_IDLE;

	RTMP_CLEAR_FLAG(pAd, fRTMP_ADAPTER_MLME_RESET_IN_PROGRESS);

}

VOID MlmePostRestartStateMachine(
	IN	PRTMP_ADAPTER	pAd)
{

	//
	// Flag fRTMP_ADAPTER_MLME_RESET_IN_PROGRESS will be clear at MlmeHandler
	// when required to do MLME Reset!
	// Since MlmeRestartStateMachine will do nothing when Mlme is running.
	//
	while (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_MLME_RESET_IN_PROGRESS))
		msleep(100);	//RTMPusecDelay(100000);

	DBGPRINT(RT_DEBUG_TRACE, "!!! MlmePostRestartStateMachine !!!\n");

	// Change back to original channel in case of doing scan
	AsicSwitchChannel(pAd, pAd->PortCfg.Channel);
	AsicLockChannel(pAd, pAd->PortCfg.Channel);

	// Resume MSDU which is turned off durning scan
	RTUSBResumeMsduTransmission(pAd);
}

/*! \brief	 The destructor of MLME Queue
 *	\param
 *	\return
 *	\pre
 *	\post
 *	\note	Clear Mlme Queue, Set Queue->Num to Zero.
 */
VOID MlmeQueueDestroy(
	IN MLME_QUEUE *pQueue)
{
	unsigned long			flags;

	NdisAcquireSpinLock(&(pQueue->Lock));

	pQueue->Num  = 0;
	pQueue->Head = 0;
	pQueue->Tail = 0;

	NdisReleaseSpinLock(&(pQueue->Lock));
	NdisFreeSpinLock(&(pQueue->Lock));
}

/*! \brief	 To substitute the message type if the message is coming from external
 *	\param	pFrame		   The frame received
 *	\param	*Machine	   The state machine
 *	\param	*MsgType	   the message type for the state machine
 *	\return TRUE if the substitution is successful, FALSE otherwise
 *	\pre
 *	\post
 */
BOOLEAN MsgTypeSubst(
	IN PRTMP_ADAPTER  pAd,
	IN PFRAME_802_11 pFrame,
	OUT INT *Machine,
	OUT INT *MsgType)
{
	USHORT Seq;
	UCHAR	EAPType;
	BOOLEAN 	Return;

// only PROBE_REQ can be broadcast, all others must be unicast-to-me && is_mybssid; otherwise,
// ignore this frame

	// wpa EAPOL PACKET
	if (pFrame->Hdr.FC.Type == BTYPE_DATA)
	{
		*Machine = WPA_PSK_STATE_MACHINE;
		EAPType = *((UCHAR*)pFrame+LENGTH_802_11+LENGTH_802_1_H+1);
		Return = WpaMsgTypeSubst(EAPType, MsgType);
		return Return;

	}
	else if (pFrame->Hdr.FC.Type == BTYPE_MGMT)
	{
		switch (pFrame->Hdr.FC.SubType)
		{
			case SUBTYPE_ASSOC_REQ:
				*Machine = ASSOC_STATE_MACHINE;
				*MsgType = MT2_PEER_ASSOC_REQ;
				break;
			case SUBTYPE_ASSOC_RSP:
				*Machine = ASSOC_STATE_MACHINE;
				*MsgType = MT2_PEER_ASSOC_RSP;
			break;
			case SUBTYPE_REASSOC_REQ:
				*Machine = ASSOC_STATE_MACHINE;
				*MsgType = MT2_PEER_REASSOC_REQ;
				break;
			case SUBTYPE_REASSOC_RSP:
				*Machine = ASSOC_STATE_MACHINE;
				*MsgType = MT2_PEER_REASSOC_RSP;
			break;
			case SUBTYPE_PROBE_REQ:
				*Machine = SYNC_STATE_MACHINE;
				*MsgType = MT2_PEER_PROBE_REQ;
			break;
			case SUBTYPE_PROBE_RSP:
				*Machine = SYNC_STATE_MACHINE;
				*MsgType = MT2_PEER_PROBE_RSP;
			break;
			case SUBTYPE_BEACON:
				*Machine = SYNC_STATE_MACHINE;
				*MsgType = MT2_PEER_BEACON;
				break;
			case SUBTYPE_ATIM:
				*Machine = SYNC_STATE_MACHINE;
				*MsgType = MT2_PEER_ATIM;
			break;
			case SUBTYPE_DISASSOC:
				*Machine = ASSOC_STATE_MACHINE;
				*MsgType = MT2_PEER_DISASSOC_REQ;
				break;
			case SUBTYPE_AUTH:
				// get the sequence number from payload 24 Mac Header + 2 bytes algorithm
				memcpy(&Seq, &pFrame->Octet[2], sizeof(USHORT));
				if (Seq == 1 || Seq == 3)
				{
					*Machine = AUTH_RSP_STATE_MACHINE;
					*MsgType = MT2_PEER_AUTH_ODD;
				}
				else if (Seq == 2 || Seq == 4)
				{
					*Machine = AUTH_STATE_MACHINE;
					*MsgType = MT2_PEER_AUTH_EVEN;
				}
				else
				{
					return FALSE;
				}
				break;
			case SUBTYPE_DEAUTH:
				*Machine = AUTH_RSP_STATE_MACHINE;
				*MsgType = MT2_PEER_DEAUTH;
				break;
			default:
				return FALSE;
		}

		return TRUE;
	}

	return FALSE;
}

// ===========================================================================================
// state_machine.c
// ===========================================================================================

/*! \brief Initialize the state machine.
 *	\param *S			pointer to the state machine
 *	\param	Trans		State machine transition function
 *	\param	StNr		number of states
 *	\param	MsgNr		number of messages
 *	\param	DefFunc 	default function, when there is invalid state/message combination
 *	\param	InitState	initial state of the state machine
 *	\param	Base		StateMachine base, internal use only
 *	\pre p_sm should be a legal pointer
 *	\post
 */
VOID StateMachineInit(
	IN STATE_MACHINE *S,
	IN STATE_MACHINE_FUNC Trans[],
	IN ULONG StNr,
	IN ULONG MsgNr,
	IN STATE_MACHINE_FUNC DefFunc,
	IN ULONG InitState,
	IN ULONG Base)
{
	ULONG i, j;

	// set number of states and messages
	S->NrState = StNr;
	S->NrMsg   = MsgNr;
	S->Base    = Base;

	S->TransFunc  = Trans;

	// init all state transition to default function
	for (i = 0; i < StNr; i++)
	{
		for (j = 0; j < MsgNr; j++)
		{
			S->TransFunc[i * MsgNr + j] = DefFunc;
		}
	}

	// set the starting state
	S->CurrState = InitState;

}

/*! \brief This function fills in the function pointer into the cell in the state machine
 *	\param *S	pointer to the state machine
 *	\param St	state
 *	\param Msg	incoming message
 *	\param f	the function to be executed when (state, message) combination occurs at the state machine
 *	\pre *S should be a legal pointer to the state machine, st, msg, should be all within the range, Base should be set in the initial state
 *	\post
 */
VOID StateMachineSetAction(
	IN STATE_MACHINE *S,
	IN ULONG St,
	IN ULONG Msg,
	IN STATE_MACHINE_FUNC Func)
{
	ULONG MsgIdx;

	MsgIdx = Msg - S->Base;

	if (St < S->NrState && MsgIdx < S->NrMsg)
	{
		// boundary checking before setting the action
		S->TransFunc[St * S->NrMsg + MsgIdx] = Func;
	}
}

/*! \brief	 This function does the state transition
 *	\param	 *Adapter the NIC adapter pointer
 *	\param	 *S 	  the state machine
 *	\param	 *Elem	  the message to be executed
 *	\return   None
 */
VOID StateMachinePerformAction(
	IN	PRTMP_ADAPTER	pAd,
	IN STATE_MACHINE *S,
	IN MLME_QUEUE_ELEM *Elem)
{
	(*(S->TransFunc[S->CurrState * S->NrMsg + Elem->MsgType - S->Base]))(pAd, Elem);
}

/*
	==========================================================================
	Description:
		The drop function, when machine executes this, the message is simply
		ignored. This function does nothing, the message is freed in
		StateMachinePerformAction()
	==========================================================================
 */
VOID Drop(
	IN PRTMP_ADAPTER pAd,
	IN MLME_QUEUE_ELEM *Elem)
{
}

// ===========================================================================================
// lfsr.c
// ===========================================================================================

/*
	==========================================================================
	Description:
	==========================================================================
 */
VOID LfsrInit(
	IN PRTMP_ADAPTER pAd,
	IN ULONG Seed)
{
	if (Seed == 0)
		pAd->Mlme.ShiftReg = 1;
	else
		pAd->Mlme.ShiftReg = Seed;
}

/*
	==========================================================================
	Description:
	==========================================================================
 */
UCHAR RandomByte(
	IN PRTMP_ADAPTER pAd)
{
	ULONG i;
	UCHAR R, Result;

	R = 0;

	for (i = 0; i < 8; i++)
	{
		if (pAd->Mlme.ShiftReg & 0x00000001)
		{
			pAd->Mlme.ShiftReg = ((pAd->Mlme.ShiftReg ^ LFSR_MASK) >> 1) | 0x80000000;
			Result = 1;
		}
		else
		{
			pAd->Mlme.ShiftReg = pAd->Mlme.ShiftReg >> 1;
			Result = 0;
		}
		R = (R << 1) | Result;
	}

	return R;
}

/*
	==========================================================================
	Description:
	==========================================================================
 */
VOID AsicSwitchChannel(
	IN PRTMP_ADAPTER pAd,
	IN UCHAR Channel)
{
	ULONG	R3 = DEFAULT_RF_TX_POWER, R4;
	CHAR	TxPwer = 0, Bbp94 = BBPR94_DEFAULT;
	UCHAR	index, BbpReg;

	// Select antenna
	AsicAntennaSelect(pAd, Channel);

	// Search Tx power value
	for (index = 0; index < pAd->ChannelListNum; index++)
	{
		if (Channel == pAd->ChannelList[index].Channel)
		{
			TxPwer = pAd->ChannelList[index].Power;
			break;
		}
	}

	if (TxPwer > 31)
	{
		//
		// R3 can't large than 36 (0x24), 31 ~ 36 used by BBP 94
		//
		R3 = 31;
		if (TxPwer <= 36)
			Bbp94 = BBPR94_DEFAULT + (UCHAR) (TxPwer - 31);
	}
	else if (TxPwer < 0)
	{
		//
		// R3 can't less than 0, -1 ~ -6 used by BBP 94
		//
		R3 = 0;
		if (TxPwer >= -6)
			Bbp94 = BBPR94_DEFAULT + TxPwer;
	}
	else
	{
		// 0 ~ 31
		R3 = (ULONG) TxPwer;
	}


	// E2PROM setting is calibrated for maximum TX power (i.e. 100%)
	// We lower TX power here according to the percentage specified from UI
	if (pAd->PortCfg.TxPowerPercentage > 90)	   // 91 ~ 100%, treat as 100% in terms of mW
		;
	else if (pAd->PortCfg.TxPowerPercentage > 60)  // 61 ~ 90%, treat as 75% in terms of mW
	{
		if (R3 > 2)
			R3 -= 2;
		else
			R3 = 0;
	}
	else if (pAd->PortCfg.TxPowerPercentage > 30)  // 31 ~ 60%, treat as 50% in terms of mW
	{
		if (R3 > 6)
			R3 -= 6;
		else
			R3 = 0;
	}
	else if (pAd->PortCfg.TxPowerPercentage > 15)  // 16 ~ 30%, treat as 25% in terms of mW
	{
		if (R3 > 12)
			R3 -= 12;
		else
			R3 = 0;
	}
	else if (pAd->PortCfg.TxPowerPercentage > 9)   // 10 ~ 15%, treat as 12.5% in terms of mW
	{
		if (R3 > 18)
			R3 -= 18;
		else
			R3 = 0;
	}
	else											 // 0 ~ 9 %, treat as 6.25% in terms of mW
	{
		if (R3 > 24)
			R3 -= 24;
		else
			R3 = 0;
	}

	if (R3 > 31)  R3 = 31;	// Maximum value 31

	if (Bbp94 < 0) Bbp94 = 0;

	R3 = R3 << 9; // shift TX power control to correct RF R3 bit position

	switch (pAd->RfIcType)
	{
		case RFIC_2528:

			for (index = 0; index < NUM_OF_2528_CHNL; index++)
			{
				if (Channel == RF2528RegTable[index].Channel)
				{
					R3 = R3 | (RF2528RegTable[index].R3 & 0xffffc1ff); // set TX power
					R4 = (RF2528RegTable[index].R4 & (~0x0003f000)) | (pAd->RfFreqOffset << 12);

					// Update variables
					pAd->LatchRfRegs.Channel = Channel;
					pAd->LatchRfRegs.R1 = RF2528RegTable[index].R1;
					pAd->LatchRfRegs.R2 = RF2528RegTable[index].R2;
					pAd->LatchRfRegs.R3 = R3;
					pAd->LatchRfRegs.R4 = R4;

					break;
				}
			}
			break;

		case RFIC_5226:
			for (index = 0; index < NUM_OF_5226_CHNL; index++)
			{
				if (Channel == RF5226RegTable[index].Channel)
				{
					R3 = R3 | (RF5226RegTable[index].R3 & 0xffffc1ff); // set TX power
					R4 = (RF5226RegTable[index].R4 & (~0x0003f000)) | (pAd->RfFreqOffset << 12);

					// Update variables
					pAd->LatchRfRegs.Channel = Channel;
					pAd->LatchRfRegs.R1 = RF5226RegTable[index].R1;
					pAd->LatchRfRegs.R2 = RF5226RegTable[index].R2;
					pAd->LatchRfRegs.R3 = R3;
					pAd->LatchRfRegs.R4 = R4;

					break;
				}
			}
			break;

		case RFIC_5225:
		case RFIC_2527:
			for (index = 0; index < NUM_OF_5225_CHNL; index++)
			{
				if (Channel == RF5225RegTable[index].Channel)
				{
					R3 = R3 | (RF5225RegTable[index].R3 & 0xffffc1ff); // set TX power
					R4 = (RF5225RegTable[index].R4 & (~0x0003f000)) | (pAd->RfFreqOffset << 12);

					// Update variables
					pAd->LatchRfRegs.Channel = Channel;
					pAd->LatchRfRegs.R1 = RF5225RegTable[index].R1;
					pAd->LatchRfRegs.R2 = RF5225RegTable[index].R2;
					pAd->LatchRfRegs.R3 = R3;
					pAd->LatchRfRegs.R4 = R4;

					break;
				}
			}

			RTUSBReadBBPRegister(pAd, BBP_R3, &BbpReg);
			if ((pAd->RfIcType == RFIC_5225) || (pAd->RfIcType == RFIC_2527))
				BbpReg &= 0xFE;    // b0=0 for none Smart mode
			else
				BbpReg |= 0x01;    // b0=1 for Smart mode
			RTUSBWriteBBPRegister(pAd, BBP_R3, BbpReg);
			break;

		default:
			break;
	}

	if (Bbp94 != BBPR94_DEFAULT)
	{
		RTUSBWriteBBPRegister(pAd, BBP_R94, Bbp94);
		pAd->Bbp94 = Bbp94;
	}

	if (!OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_MEDIA_STATE_CONNECTED))
	{
		if (Channel <= 14)
		{
			if (pAd->BbpTuning.R17LowerUpperSelect == 0)
				RTUSBWriteBBPRegister(pAd, BBP_R17, pAd->BbpTuning.R17LowerBoundG);
			else
				RTUSBWriteBBPRegister(pAd, BBP_R17, pAd->BbpTuning.R17UpperBoundG);
		}
		else
		{
			if (pAd->BbpTuning.R17LowerUpperSelect == 0)
				RTUSBWriteBBPRegister(pAd, BBP_R17, pAd->BbpTuning.R17LowerBoundA);
			else
				RTUSBWriteBBPRegister(pAd, BBP_R17, pAd->BbpTuning.R17UpperBoundA);
		}
	}

	// Set RF value 1's set R3[bit2] = [0]
	RTUSBWriteRFRegister(pAd, pAd->LatchRfRegs.R1);
	RTUSBWriteRFRegister(pAd, pAd->LatchRfRegs.R2);
	RTUSBWriteRFRegister(pAd, (pAd->LatchRfRegs.R3 & (~0x04)));
	RTUSBWriteRFRegister(pAd, pAd->LatchRfRegs.R4);

	// Set RF value 2's set R3[bit2] = [1]
	RTUSBWriteRFRegister(pAd, pAd->LatchRfRegs.R1);
	RTUSBWriteRFRegister(pAd, pAd->LatchRfRegs.R2);
	RTUSBWriteRFRegister(pAd, (pAd->LatchRfRegs.R3 | 0x04));
	RTUSBWriteRFRegister(pAd, pAd->LatchRfRegs.R4);

	// Set RF value 3's set R3[bit2] = [0]
	RTUSBWriteRFRegister(pAd, pAd->LatchRfRegs.R1);
	RTUSBWriteRFRegister(pAd, pAd->LatchRfRegs.R2);
	RTUSBWriteRFRegister(pAd, (pAd->LatchRfRegs.R3 & (~0x04)));
	RTUSBWriteRFRegister(pAd, pAd->LatchRfRegs.R4);

	//
	// On 11A/11G, We should delay and wait RF/BBP to be stable
	// and the appropriate time should be 10 micro seconds
	// It's not recommend to use NdisStallExecution on PASSIVE_LEVEL
	// use NdisMSleep to dealy 10 microsecond instead.
	//
	RTMPusecDelay(10);

	DBGPRINT(RT_DEBUG_TRACE, "AsicSwitchChannel(RF=%d) to #%d, TXPwr=%d%%, R1=0x%08x, R2=0x%08x, R3=0x%08x, R4=0x%08x\n",
		pAd->RfIcType,
		pAd->LatchRfRegs.Channel,
		(R3 & 0x00003e00) >> 9,
		pAd->LatchRfRegs.R1,
		pAd->LatchRfRegs.R2,
		pAd->LatchRfRegs.R3,
		pAd->LatchRfRegs.R4);
}

/*
	==========================================================================
	Description:
		This function is required for 2421 only, and should not be used during
		site survey. It's only required after NIC decided to stay at a channel
		for a longer period.
		When this function is called, it's always after AsicSwitchChannel().
	==========================================================================
 */
VOID AsicLockChannel(
	IN PRTMP_ADAPTER pAd,
	IN UCHAR Channel)
{
	// Not used.
}


char *AntStr[3] = {"DIVERSITY","Ant-A","Ant-B"};

#define SOFTWARE_DIVERSITY	0
#define ANTENNA_A			1
#define ANTENNA_B			2
#define HARDWARE_DIVERSITY	3

/*
	==========================================================================
	Description:
	==========================================================================
 */
VOID AsicAntennaSelect(
	IN	PRTMP_ADAPTER	pAd,
	IN	UCHAR			Channel)
{
	ULONG	*value = kzalloc(sizeof(ULONG), GFP_KERNEL);
	ABGBAND_STATE	BandState;


	DBGPRINT(RT_DEBUG_INFO,"AsicAntennaSelect(ch=%d) - Tx=%s, Rx=%s\n",
		Channel, AntStr[pAd->Antenna.field.TxDefaultAntenna], AntStr[pAd->Antenna.field.RxDefaultAntenna]);

	if (!value) {
		DBGPRINT(RT_DEBUG_ERROR, "couldn't allocate memory\n");
		return;
	}
	if (Channel <= 14)
		BandState = BG_BAND;
	else
		BandState = A_BAND;

	//
	//	Only the first time switch from g to a or a to g
	//	and then will be reset the BBP, otherwise do nothing.
	//
	if (BandState == pAd->PortCfg.BandState)
		return;

	// Change BBP setting during siwtch from a->g, g->a
	if (Channel <= 14)
	{
		if (pAd->NicConfig2.field.ExternalLNA)
		{
			// TRUE
			RTUSBWriteBBPRegister(pAd, 17, 0x30); // if external LNA enable, this value need to be offset 0x10
			RTUSBWriteBBPRegister(pAd, 96, 0x68); // if external LNA enable, R96 need to shit 0x20 on B/G mode, Request by David 2005/05/12
			RTUSBWriteBBPRegister(pAd, 104, 0x3C);// if external LNA enable, R104 need to shit 0x10 on B/G mode, Request by David 2005/05/12
			RTUSBWriteBBPRegister(pAd, 75, 0x80);// if external LNA enable, set this to 0x80 on B/G mode, Request by David 2005/05/12
			RTUSBWriteBBPRegister(pAd, 86, 0x80);// if external LNA enable, set this to 0x80 on B/G mode, Request by David 2005/05/12
			RTUSBWriteBBPRegister(pAd, 88, 0x80);// if external LNA enable, set this to 0x80 on B/G mode, Request by David 2005/05/12
		}
		else
		{
			RTUSBWriteBBPRegister(pAd, 17, 0x20);
			RTUSBWriteBBPRegister(pAd, 96, 0x48);
			RTUSBWriteBBPRegister(pAd, 104, 0x2C);
			RTUSBWriteBBPRegister(pAd, 75, 0xFE);// if external LNA enable, set this to 0x80 on B/G mode, Request by David 2005/05/12
			RTUSBWriteBBPRegister(pAd, 86, 0xFE);// if external LNA enable, set this to 0x80 on B/G mode, Request by David 2005/05/12
			RTUSBWriteBBPRegister(pAd, 88, 0xFE);// if external LNA enable, set this to 0x80 on B/G mode, Request by David 2005/05/12
		}
		RTUSBWriteBBPRegister(pAd, 35, 0x50);
		RTUSBWriteBBPRegister(pAd, 97, 0x48);
		RTUSBWriteBBPRegister(pAd, 98, 0x48);
	}
	else
	{
		if (pAd->NicConfig2.field.ExternalLNA)
		{
			RTUSBWriteBBPRegister(pAd, 17, 0x38); // if external LNA enable, this value need to be offset 0x10
			RTUSBWriteBBPRegister(pAd, 96, 0x78); // if external LNA enable, R96 need to shit 0x20 on B/G mode, Request by David 2005/05/12
			RTUSBWriteBBPRegister(pAd, 104, 0x48);// if external LNA enable, R104 need to shit 0x10 on B/G mode, Request by David 2005/05/12
			RTUSBWriteBBPRegister(pAd, 75, 0x80);// if external LNA enable, set this to 0x80 on B/G mode, Request by David 2005/05/12
			RTUSBWriteBBPRegister(pAd, 86, 0x80);// if external LNA enable, set this to 0x80 on B/G mode, Request by David 2005/05/12
			RTUSBWriteBBPRegister(pAd, 88, 0x80);// if external LNA enable, set this to 0x80 on B/G mode, Request by David 2005/05/12
		}
		else
		{
			RTUSBWriteBBPRegister(pAd, 17, 0x28);
			RTUSBWriteBBPRegister(pAd, 96, 0x58);
			RTUSBWriteBBPRegister(pAd, 104, 0x38);
		}
		RTUSBWriteBBPRegister(pAd, 35, 0x60);
		RTUSBWriteBBPRegister(pAd, 97, 0x58);
		RTUSBWriteBBPRegister(pAd, 98, 0x58);
	}

	RTUSBReadMACRegister(pAd, PHY_CSR0, value);
	*value = *value & 0xfffcffff;   /* Mask off bit 16, bit 17 */
	if (Channel <= 14)
		/* b16 to enable G band PA_PE*/
		*value = *value | BIT32[16];
	else
		/* b17 to enable A band PA_PE */
		*value = *value | BIT32[17];
	RTUSBWriteMACRegister(pAd, PHY_CSR0, *value);

	pAd->PortCfg.BandState = BandState;

	AsicAntennaSetting(pAd, BandState);
	kfree(value);
}

VOID AsicAntennaSetting(
	IN	PRTMP_ADAPTER	pAd,
	IN	ABGBAND_STATE	BandState)
{
	UCHAR		R3 = 0, R4 = 0, R77 = 0;
	UCHAR		FrameTypeMaskBit5 = 0;

	// driver must disable Rx when switching antenna, otherwise ASIC will keep default state
	// after switching, driver needs to re-enable Rx later
	RT73WriteTXRXCSR0(pAd, TRUE,FALSE);
	// Update antenna registers
	RTUSBReadBBPRegister(pAd, BBP_R3, &R3);
	RTUSBReadBBPRegister(pAd, BBP_R4, &R4);
	RTUSBReadBBPRegister(pAd, BBP_R77, &R77);

	R3	&= 0xfe;		// clear Bit 0
	R4	&= ~0x23;		// clear Bit 0,1,5

	FrameTypeMaskBit5 = ~(pAd->Antenna.field.FrameType << 5);

	//
	// For Smart a/b/g need to set R3 bit[0] to 1
	//
	// R3 |= 0x01;	//RFIC_5325, RFIC_2529, <Bit0> = <1>
	//

	//Select RF_Type
	switch (pAd->Antenna.field.RfIcType)
	{
		case RFIC_5226:
			//Support 11B/G/A
			if (BandState == BG_BAND)
			{
				//Check Rx Anttena
				if (pAd->Antenna.field.RxDefaultAntenna == ANTENNA_A)
				{
					R4	= R4 | 0x01;		// <Bit5:Bit1:Bit0> = <0:0:1>
					R4	= R4 & FrameTypeMaskBit5;

					R77 = R77 | 0x03;	// <Bit1:Bit0> = <1:1>

					RTUSBWriteBBPRegister(pAd, BBP_R77, R77);
				}
				else if (pAd->Antenna.field.RxDefaultAntenna == ANTENNA_B)
				{
					R4	= R4 | 0x01;		// <Bit5:Bit1:Bit0> = <0:0:1>
					R4	= R4 & FrameTypeMaskBit5;

					R77 = R77 & 0xfc;		// <Bit1:Bit0> = <0:0>

					RTUSBWriteBBPRegister(pAd, BBP_R77, R77);
				}
				else if (pAd->Antenna.field.RxDefaultAntenna == HARDWARE_DIVERSITY)
				{
					R4	= R4 | 0x22;		// <Bit5:Bit1:Bit0> = <1:1:0>
					R4	= R4 & FrameTypeMaskBit5;
				}
				else
				{
					; //SOFTWARE_DIVERSITY
					R4	= R4 | 0x01;		// <Bit5:Bit1:Bit0> = <0:0:1>
					R4	= R4 & FrameTypeMaskBit5;

					pAd->RxAnt.Pair1PrimaryRxAnt   = 1;  // assume ant-B
					pAd->RxAnt.Pair1SecondaryRxAnt = 0;  // assume ant-A

					AsicSetRxAnt(pAd, pAd->RxAnt.Pair1PrimaryRxAnt, pAd->RxAnt.Pair2PrimaryRxAnt);
				}
			}
			else //A_BAND
			{
				//Check Rx Anttena
				if (pAd->Antenna.field.RxDefaultAntenna == ANTENNA_A)
				{
					R4	= R4 | 0x01;		// <Bit5:Bit1:Bit0> = <0:0:1>
					R4	= R4 & FrameTypeMaskBit5;

					R77 = R77 & 0xfc;		// <Bit1:Bit0> = <0:0>

					RTUSBWriteBBPRegister(pAd, BBP_R77, R77);
				}
				else if (pAd->Antenna.field.RxDefaultAntenna == ANTENNA_B)
				{
					R4	= R4 | 0x01;		// <Bit5:Bit1:Bit0> = <0:0:1>
					R4	= R4 & FrameTypeMaskBit5;

					R77	= R77 | 0x03;	// <Bit1:Bit0> = <1:1>

					RTUSBWriteBBPRegister(pAd, BBP_R77, R77);
				}
				else if (pAd->Antenna.field.RxDefaultAntenna == HARDWARE_DIVERSITY)
				{
					R4	= R4 | 0x02;		// <Bit5:Bit1:Bit0> = <0:1:0>
					R4	= R4 & FrameTypeMaskBit5;
				}
				else
				{
					; //SOFTWARE_DIVERSITY
					R4	= R4 | 0x01;		// <Bit5:Bit1:Bit0> = <0:0:1>
					R4	= R4 & FrameTypeMaskBit5;

					pAd->RxAnt.Pair1PrimaryRxAnt   = 1;  // assume ant-B
					pAd->RxAnt.Pair1SecondaryRxAnt = 0;  // assume ant-A
					AsicSetRxAnt(pAd, pAd->RxAnt.Pair1PrimaryRxAnt, pAd->RxAnt.Pair2PrimaryRxAnt);
				}
			}
			break;

		case RFIC_2528:
			//Support 11B/G
			//Check Rx Anttena
			//Check Rx Anttena
			if (pAd->Antenna.field.RxDefaultAntenna == ANTENNA_A)
			{
				R4	= R4 | 0x21;		// <Bit5:Bit1:Bit0> = <1:0:1>
				R4	= R4 & FrameTypeMaskBit5;

				R77	= R77 | 0x03;	// <Bit1:Bit0> = <1:1>

				RTUSBWriteBBPRegister(pAd, BBP_R77, R77);
			}
			else if (pAd->Antenna.field.RxDefaultAntenna == ANTENNA_B)
			{
				R4	= R4 | 0x21;		// <Bit5:Bit1:Bit0> = <1:0:1>
				R4	= R4 & FrameTypeMaskBit5;

				R77 = R77 & 0xfc;		// <Bit1:Bit0> = <0:0>

				RTUSBWriteBBPRegister(pAd, BBP_R77, R77);
			}
			else if (pAd->Antenna.field.RxDefaultAntenna == HARDWARE_DIVERSITY)
			{
				R4	= R4 | 0x22;		// <Bit5:Bit1:Bit0> = <1:1:0>
				R4	= R4 & FrameTypeMaskBit5;
			}
			else
			{
				; //SOFTWARE_DIVERSITY
				R4	= R4 | 0x21;		// <Bit5:Bit1:Bit0> = <1:0:1>
				R4	= R4 & FrameTypeMaskBit5;

				pAd->RxAnt.Pair1PrimaryRxAnt   = 1;  // assume ant-B
				pAd->RxAnt.Pair1SecondaryRxAnt = 0;  // assume ant-A
				AsicSetRxAnt(pAd, pAd->RxAnt.Pair1PrimaryRxAnt, pAd->RxAnt.Pair2PrimaryRxAnt);
			}
			break;

		case RFIC_5225:
			//Support 11B/G/A
			if (BandState == BG_BAND)
			{
				//Check Rx Anttena
				if (pAd->Antenna.field.RxDefaultAntenna == ANTENNA_A)
				{
					R4	= R4 | 0x01;		// <Bit5:Bit1:Bit0> = <0:0:1>
					R77 = R77 | 0x03;	// <Bit1:Bit0> = <1:1>

					RTUSBWriteBBPRegister(pAd, BBP_R77, R77);
				}
				else if (pAd->Antenna.field.RxDefaultAntenna == ANTENNA_B)
				{
					R4	= R4 | 0x01;		// <Bit5:Bit1:Bit0> = <0:0:1>
					R77 = R77 & 0xfc;		// <Bit1:Bit0> = <0:0>

					RTUSBWriteBBPRegister(pAd, BBP_R77, R77);
				}
				else if (pAd->Antenna.field.RxDefaultAntenna == HARDWARE_DIVERSITY)
				{
					R4	= R4 | 0x22;		// <Bit5:Bit1:Bit0> = <1:1:0>
				}
				else
				{
					; //SOFTWARE_DIVERSITY
					R4	= R4 | 0x01;		// <Bit5:Bit1:Bit0> = <0:0:1>
					pAd->RxAnt.Pair1PrimaryRxAnt   = 1;  // assume ant-B
					pAd->RxAnt.Pair1SecondaryRxAnt = 0;  // assume ant-A

					AsicSetRxAnt(pAd, pAd->RxAnt.Pair1PrimaryRxAnt, pAd->RxAnt.Pair2PrimaryRxAnt);
				}
			}
			else //A_BAND
			{
				//Check Rx Anttena
				if (pAd->Antenna.field.RxDefaultAntenna == ANTENNA_A)
				{
					R4	= R4 | 0x01;		// <Bit5:Bit1:Bit0> = <0:0:1>
					R77 = R77 & 0xfc;		// <Bit1:Bit0> = <0:0>

					RTUSBWriteBBPRegister(pAd, BBP_R77, R77);
				}
				else if (pAd->Antenna.field.RxDefaultAntenna == ANTENNA_B)
				{
					R4	= R4 | 0x01;		// <Bit5:Bit1:Bit0> = <0:0:1>
					R77	= R77 | 0x03;	// <Bit1:Bit0> = <1:1>

					RTUSBWriteBBPRegister(pAd, BBP_R77, R77);
				}
				else if (pAd->Antenna.field.RxDefaultAntenna == HARDWARE_DIVERSITY)
				{
					R4	= R4 | 0x02;		// <Bit5:Bit1:Bit0> = <0:1:0>
				}
				else
				{
					; //SOFTWARE_DIVERSITY
					R4	= R4 | 0x01;		// <Bit5:Bit1:Bit0> = <0:0:1>
					pAd->RxAnt.Pair1PrimaryRxAnt   = 1;  // assume ant-B
					pAd->RxAnt.Pair1SecondaryRxAnt = 0;  // assume ant-A
					AsicSetRxAnt(pAd, pAd->RxAnt.Pair1PrimaryRxAnt, pAd->RxAnt.Pair2PrimaryRxAnt);
				}
			}
			break;

		case RFIC_2527:
			//Support 11B/G
			//Check Rx Anttena
			//Check Rx Anttena
			if (pAd->Antenna.field.RxDefaultAntenna == ANTENNA_A)
			{
				R4	= R4 | 0x21;		// <Bit5:Bit1:Bit0> = <1:0:1>
				R4	= R4 & FrameTypeMaskBit5;

				R77	= R77 | 0x03;	// <Bit1:Bit0> = <1:1>

				RTUSBWriteBBPRegister(pAd, BBP_R77, R77);
			}
			else if (pAd->Antenna.field.RxDefaultAntenna == ANTENNA_B)
			{
				R4	= R4 | 0x21;		// <Bit5:Bit1:Bit0> = <1:0:1>
				R4	= R4 & FrameTypeMaskBit5;

				R77 = R77 & 0xfc;		// <Bit1:Bit0> = <0:0>

				RTUSBWriteBBPRegister(pAd, BBP_R77, R77);
			}
			else if (pAd->Antenna.field.RxDefaultAntenna == HARDWARE_DIVERSITY)
			{
				R4	= R4 | 0x22;		// <Bit5:Bit1:Bit0> = <1:1:0>
				R4	= R4 & FrameTypeMaskBit5;
			}
			else
			{
				; //SOFTWARE_DIVERSITY
				R4	= R4 | 0x21;		// <Bit5:Bit1:Bit0> = <1:0:1>
				R4	= R4 & FrameTypeMaskBit5;

				pAd->RxAnt.Pair1PrimaryRxAnt   = 1;  // assume ant-B
				pAd->RxAnt.Pair1SecondaryRxAnt = 0;  // assume ant-A
				AsicSetRxAnt(pAd, pAd->RxAnt.Pair1PrimaryRxAnt, pAd->RxAnt.Pair2PrimaryRxAnt);
			}
			break;

		default:
			DBGPRINT(RT_DEBUG_TRACE, "Unkown RFIC Type\n");
			break;
	}

	RTUSBWriteBBPRegister(pAd, BBP_R3, R3);
	RTUSBWriteBBPRegister(pAd, BBP_R4, R4);

	RT73WriteTXRXCSR0(pAd, FALSE, TRUE);		 // Staion not drop control frame will fail WiFi Certification.
}

/*
	==========================================================================
	Description:
		Gives CCK TX rate 2 more dB TX power.
		This routine works only in LINK UP in INFRASTRUCTURE mode.

		calculate desired Tx power in RF R3.Tx0~5,	should consider -
		0. if current radio is a noisy environment (pAd->DrsCounters.fNoisyEnvironment)
		1. TxPowerPercentage
		2. auto calibration based on TSSI feedback
		3. extra 2 db for CCK
		4. -10 db upon very-short distance (AvgRSSI >= -40db) to AP

	NOTE: Since this routine requires the value of (pAd->DrsCounters.fNoisyEnvironment),
		it should be called AFTER MlmeDynamicTxRatSwitching()
	==========================================================================
 */
VOID AsicAdjustTxPower(
	IN PRTMP_ADAPTER pAd)
{
	ULONG	R3, CurrTxPwr;
	SHORT	dbm;
	UCHAR	TxRate, Channel, index;
	BOOLEAN bAutoTxAgc = FALSE;
	UCHAR	TssiRef, *pTssiMinusBoundary, *pTssiPlusBoundary, TxAgcStep;
	UCHAR	BbpR1, idx;
	PCHAR	pTxAgcCompensate;
	CHAR	TxPwer=0;

	TxRate	= pAd->PortCfg.TxRate;
	Channel = pAd->PortCfg.Channel;

	dbm = pAd->PortCfg.AvgRssi - pAd->BbpRssiToDbmDelta;

	// get TX Power base from E2PROM
	R3 = DEFAULT_RF_TX_POWER;
	for (index= 0 ; index < pAd->ChannelListNum; index++)
	{
		if (pAd->ChannelList[index].Channel == pAd->PortCfg.Channel)
		{
			TxPwer = pAd->ChannelList[index].Power;
			break;
		}
	}

	if ((TxPwer > 31) || (TxPwer < 0))
		R3 = 0;
	else
		R3 = (ULONG) TxPwer;

	if (R3 > 31) R3 = 31;

	// error handling just in case
	if (index >= pAd->ChannelListNum)
	{
		DBGPRINT_ERR("AsicAdjustTxPower(can find pAd->PortCfg.Channel=%d in ChannelList[%d]\n", pAd->PortCfg.Channel, pAd->ChannelListNum);
		return;
	}

	// TX power compensation for temperature variation based on TSSI. try every 4 second
	if (pAd->Mlme.PeriodicRound % 4 == 0)
	{
		if (pAd->PortCfg.Channel <= 14)
		{
			bAutoTxAgc		   = pAd->bAutoTxAgcG;
			TssiRef 		   = pAd->TssiRefG;
			pTssiMinusBoundary = &pAd->TssiMinusBoundaryG[0];
			pTssiPlusBoundary  = &pAd->TssiPlusBoundaryG[0];
			TxAgcStep		   = pAd->TxAgcStepG;
			pTxAgcCompensate   = &pAd->TxAgcCompensateG;
		}
		else
		{
			bAutoTxAgc		   = pAd->bAutoTxAgcA;
			TssiRef 		   = pAd->TssiRefA;
			pTssiMinusBoundary = &pAd->TssiMinusBoundaryA[0];
			pTssiPlusBoundary  = &pAd->TssiPlusBoundaryA[0];
			TxAgcStep		   = pAd->TxAgcStepA;
			pTxAgcCompensate   = &pAd->TxAgcCompensateA;
		}

		if (bAutoTxAgc)
		{
			RTUSBReadBBPRegister(pAd, BBP_R1,  &BbpR1);
			if (BbpR1 > pTssiMinusBoundary[1])
			{
				// Reading is larger than the reference value
				// check for how large we need to decrease the Tx power
				for (idx = 1; idx < 5; idx++)
				{
					if (BbpR1 <= pAd->TssiMinusBoundaryG[idx])	// Found the range
						break;
				}
				// The index is the step we should decrease, idx = 0 means there is nothing to compensate
				if (R3 > (ULONG) (TxAgcStep * (idx-1)))
					*pTxAgcCompensate = -(TxAgcStep * (idx-1));
				else
					*pTxAgcCompensate = -((UCHAR)R3);

				R3 += (*pTxAgcCompensate);
				DBGPRINT(RT_DEBUG_TRACE, "-- Tx Power, BBP R1=%x, TssiRef=%x, TxAgcStep=%x, step = -%d\n",
					BbpR1, TssiRef, TxAgcStep, idx-1);
			}
			else if (BbpR1 < pTssiPlusBoundary[1])
			{
				// Reading is smaller than the reference value
				// check for how large we need to increase the Tx power
				for (idx = 1; idx < 5; idx++)
				{
					if (BbpR1 >= pTssiPlusBoundary[idx])   // Found the range
						break;
				}
				// The index is the step we should increase, idx = 0 means there is nothing to compensate
				*pTxAgcCompensate = TxAgcStep * (idx-1);
				R3 += (*pTxAgcCompensate);
				DBGPRINT(RT_DEBUG_TRACE, "++ Tx Power, BBP R1=%x, Tssi0=%x, TxAgcStep=%x, step = +%d\n",
					BbpR1, TssiRef, TxAgcStep, idx-1);
			}
		}
	}
	else
	{
		if (pAd->PortCfg.Channel <= 14)
		{
			bAutoTxAgc		   = pAd->bAutoTxAgcG;
			pTxAgcCompensate   = &pAd->TxAgcCompensateG;
		}
		else
		{
			bAutoTxAgc		   = pAd->bAutoTxAgcA;
			pTxAgcCompensate   = &pAd->TxAgcCompensateA;
		}

		if (bAutoTxAgc)
			R3 += (*pTxAgcCompensate);
	}


	// E2PROM setting is calibrated for maximum TX power (i.e. 100%)
	// We lower TX power here according to the percentage specified from UI
	if (pAd->PortCfg.TxPowerPercentage == 0xffffffff)		// AUTO TX POWER control
	{
#if 1
		// only INFRASTRUCTURE mode and AUTO-TX-power need furthur calibration
		if (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_MEDIA_STATE_CONNECTED))
		{
			// low TX power upon very-short distance to AP to solve some vendor's AP RX problem
			// in this case, no TSSI compensation is required.
			if (dbm > RSSI_FOR_LOWEST_TX_POWER)
			{
				if (R3 > LOWEST_TX_POWER_DELTA)
					R3 -= LOWEST_TX_POWER_DELTA;
				else
					R3 = 0;
			}
			else if (dbm > RSSI_FOR_LOW_TX_POWER)
			{
				if (R3 > LOW_TX_POWER_DELTA)
					R3 -= LOW_TX_POWER_DELTA;
				else
					R3 = 0;
			}
		}
#endif
	}
	else if (pAd->PortCfg.TxPowerPercentage > 90)  // 91 ~ 100% & AUTO, treat as 100% in terms of mW
		;
	else if (pAd->PortCfg.TxPowerPercentage > 60)  // 61 ~ 90%, treat as 75% in terms of mW
	{
		if (R3 > 2)
			R3 -= 2;
		else
			R3 = 0;
	}
	else if (pAd->PortCfg.TxPowerPercentage > 30)  // 31 ~ 60%, treat as 50% in terms of mW
	{
		if (R3 > 6)
			R3 -= 6;
		else
			R3 = 0;
	}
	else if (pAd->PortCfg.TxPowerPercentage > 15)  // 16 ~ 30%, treat as 25% in terms of mW
	{
		if (R3 > 12)
			R3 -= 12;
		else
			R3 = 0;
	}
	else if (pAd->PortCfg.TxPowerPercentage > 9)   // 10 ~ 15%, treat as 12.5% in terms of mW
	{
		if (R3 > 18)
			R3 -= 18;
		else
			R3 = 0;
	}
	else										   // 0 ~ 9 %, treat as MIN(~3%) in terms of mW
	{
		if (R3 > 24)
			R3 -= 24;
		else
			R3 = 0;
	}

	if (R3 > 31)  R3 = 31;	 //Maximum value 31

	// compare the desired R3.TxPwr value with current R3, if not equal
	// set new R3.TxPwr
	CurrTxPwr = (pAd->LatchRfRegs.R3 >> 9) & 0x0000001f;
	if (CurrTxPwr != R3)
	{
		CurrTxPwr = R3;
		R3 = (pAd->LatchRfRegs.R3 & 0xffffc1ff) | (R3 << 9);
		pAd->LatchRfRegs.R3 = R3;

		// Set RF value 1's set R3[bit2] = [0]
		RTUSBWriteRFRegister(pAd, pAd->LatchRfRegs.R1);
		RTUSBWriteRFRegister(pAd, pAd->LatchRfRegs.R2);
		RTUSBWriteRFRegister(pAd, (pAd->LatchRfRegs.R3 & (~0x04)));
		RTUSBWriteRFRegister(pAd, pAd->LatchRfRegs.R4);

		// Set RF value 2's set R3[bit2] = [1]
		RTUSBWriteRFRegister(pAd, pAd->LatchRfRegs.R1);
		RTUSBWriteRFRegister(pAd, pAd->LatchRfRegs.R2);
		RTUSBWriteRFRegister(pAd, (pAd->LatchRfRegs.R3 | 0x04));
		RTUSBWriteRFRegister(pAd, pAd->LatchRfRegs.R4);

		// Set RF value 3's set R3[bit2] = [0]
		RTUSBWriteRFRegister(pAd, pAd->LatchRfRegs.R1);
		RTUSBWriteRFRegister(pAd, pAd->LatchRfRegs.R2);
		RTUSBWriteRFRegister(pAd, (pAd->LatchRfRegs.R3 & (~0x04)));
		RTUSBWriteRFRegister(pAd, pAd->LatchRfRegs.R4);

//		DBGPRINT(RT_DEBUG_TRACE, "AsicAdjustTxPower = %d, AvgRssi = %d\n", CurrTxPwr, dbm);
	}
		DBGPRINT(RT_DEBUG_TRACE, "AsicAdjustTxPower = %d, AvgRssi = %d\n", CurrTxPwr, dbm);

}

/*
	==========================================================================
	Description:
		put PHY to sleep here, and set next wakeup timer. PHY doesn't not wakeup
		automatically. Instead, MCU will issue a TwakeUpInterrupt to host after
		the wakeup timer timeout. Driver has to issue a separate command to wake
		PHY up.

	==========================================================================
 */
VOID AsicSleepThenAutoWakeup(
	IN PRTMP_ADAPTER pAd,
	IN USHORT TbttNumToNextWakeUp)
{
	MAC_CSR11_STRUC csr11;

	// we have decided to SLEEP, so at least do it for a BEACON period.
	if (TbttNumToNextWakeUp == 0)
		TbttNumToNextWakeUp = 1;

	// set MAC_CSR11 for next wakeup
	csr11.word = 0;
	csr11.field.Sleep2AwakeLatency = 5;
	csr11.field.NumOfTBTTBeforeWakeup = TbttNumToNextWakeUp - 1;
	csr11.field.DelayAfterLastTBTTBeforeWakeup = pAd->PortCfg.BeaconPeriod - 10; // 5 TU ahead of desired TBTT

	// To make sure ASIC Auto-Wakeup functionality works properly
	// We must disable it first to let ASIC recounting the Auto-Wakeup period.
	// After that enable it.
	//
	csr11.field.bAutoWakeupEnable = 0;	//Disable
	RTUSBWriteMACRegister(pAd, MAC_CSR11, csr11.word);

	csr11.field.bAutoWakeupEnable = 1;	//Enable
	RTUSBWriteMACRegister(pAd, MAC_CSR11, csr11.word);

	DBGPRINT(RT_DEBUG_TRACE, ">>>AsicSleepThenAutoWakeup(sleep %d TU)<<<\n",
			(csr11.field.NumOfTBTTBeforeWakeup * pAd->PortCfg.BeaconPeriod) + csr11.field.DelayAfterLastTBTTBeforeWakeup);

	RTUSBPutToSleep(pAd);

	OPSTATUS_SET_FLAG(pAd, fOP_STATUS_DOZE);
}

/*
	==========================================================================
	Description:
		AsicForceWakeup() is used whenever manual wakeup is required
		AsicForceSleep() should only be used when not in INFRA BSS. When
		in INFRA BSS, we should use AsicSleepThenAutoWakeup() instead.
	==========================================================================
 */
VOID AsicForceSleep(
	IN PRTMP_ADAPTER pAd)
{
	MAC_CSR11_STRUC csr11;

	DBGPRINT(RT_DEBUG_TRACE, ">>>AsicForceSleep<<<\n");

	// no auto wakeup
	csr11.word = 0;
	csr11.field.Sleep2AwakeLatency = 5;
	csr11.field.bAutoWakeupEnable = 0;
	RTUSBWriteMACRegister(pAd, MAC_CSR11, csr11.word);

	RTUSBPutToSleep(pAd);

	OPSTATUS_SET_FLAG(pAd, fOP_STATUS_DOZE);
}

/*
	==========================================================================
	Description:
		AsicForceWakeup() is used whenever Twakeup timer (set via AsicSleepThenAutoWakeup)
		expired.

	==========================================================================
 */
VOID AsicForceWakeup(
	IN PRTMP_ADAPTER pAd)
{
	MAC_CSR11_STRUC csr11;

	DBGPRINT(RT_DEBUG_TRACE, ">>>AsicForceWakeup<<<\n");

	RTUSBWakeUp(pAd);

	// cancel auto wakeup timer
	csr11.word = 0;
	csr11.field.Sleep2AwakeLatency = 5;
	csr11.field.bAutoWakeupEnable = 0;
	RTUSBWriteMACRegister(pAd, MAC_CSR11, csr11.word);

	OPSTATUS_CLEAR_FLAG(pAd, fOP_STATUS_DOZE);
}

/*
	==========================================================================
	Description:

	==========================================================================
 */
VOID AsicSetBssid(
	IN PRTMP_ADAPTER pAd,
	IN PUCHAR		 pBssid)
{
	ULONG		  Addr4;

	DBGPRINT(RT_DEBUG_TRACE, ">>>AsicSetBssid<<<\n");

	Addr4 = (ULONG)(pBssid[0])		 |
			(ULONG)(pBssid[1] << 8)  |
			(ULONG)(pBssid[2] << 16) |
			(ULONG)(pBssid[3] << 24);
	RTUSBWriteMACRegister(pAd, MAC_CSR4, Addr4);

	// always one BSSID in STA mode
	Addr4 = (ULONG)(pBssid[4]) | (ULONG)(pBssid[5] << 8) | 0x00030000;
	RTUSBWriteMACRegister(pAd, MAC_CSR5, Addr4);
}

/*
	==========================================================================
	Description:

	==========================================================================
 */
VOID AsicDisableSync(
	IN PRTMP_ADAPTER pAd)
{
	TXRX_CSR9_STRUC *csr = kzalloc(sizeof(TXRX_CSR9_STRUC), GFP_KERNEL);
	DBGPRINT(RT_DEBUG_TRACE, "--->Disable TSF synchronization\n");

	if (!csr) {
		DBGPRINT(RT_DEBUG_ERROR, "couldn't allocate memory\n");
		return;
	}

	// 2003-12-20 disable TSF and TBTT while NIC in power-saving have side effect
	//			  that NIC will never wakes up because TSF stops and no more
	//			  TBTT interrupts
	RTUSBReadMACRegister(pAd, TXRX_CSR9, &csr->word);
	csr->field.bBeaconGen = 0;
	csr->field.TsfSyncMode = 0;
	csr->field.bTBTTEnable = 0;
	csr->field.bTsfTicking = 0;
	RTUSBWriteMACRegister(pAd, TXRX_CSR9, csr->word);
	kfree(csr);
}

/*
	==========================================================================
	Description:
	==========================================================================
 */
VOID AsicEnableBssSync(
	IN PRTMP_ADAPTER pAd)
{
	TXRX_CSR9_STRUC *csr = kzalloc(sizeof(TXRX_CSR9_STRUC), GFP_KERNEL);

	DBGPRINT(RT_DEBUG_TRACE, "--->AsicEnableBssSync(INFRA mode)\n");

	if (!csr) {
		DBGPRINT(RT_DEBUG_ERROR, "couldn't allocate memory\n");
		return;
	}

	RTUSBReadMACRegister(pAd, TXRX_CSR9, &csr->word);

	/* ASIC register in units of 1/16 TU */
	csr->field.BeaconInterval = pAd->PortCfg.BeaconPeriod << 4;
	csr->field.bTsfTicking = 1;
	csr->field.TsfSyncMode = 1; /* sync TSF in INFRASTRUCTURE mode */
	csr->field.bBeaconGen  = 0; /* do NOT generate BEACON */
	csr->field.bTBTTEnable = 1;

	RTUSBWriteMACRegister(pAd, TXRX_CSR9, csr->word);
	kfree(csr);
}

/*
	==========================================================================
	Description:
	Note:
		BEACON frame in shared memory should be built ok before this routine
		can be called. Otherwise, a garbage frame maybe transmitted out every
		Beacon period.

	==========================================================================
 */
VOID AsicEnableIbssSync(
	IN PRTMP_ADAPTER pAd)
{
	TXRX_CSR9_STRUC *csr9 = kmalloc(sizeof(TXRX_CSR9_STRUC), GFP_KERNEL);
	PUCHAR			ptr;
	UINT i;
	DBGPRINT(RT_DEBUG_ERROR, "--->AsicEnableIbssSync(ADHOC mode)\n");

	RTUSBReadMACRegister(pAd, TXRX_CSR9, &csr9->word);
	csr9->field.bBeaconGen = 0;
	csr9->field.bTBTTEnable = 0;
	csr9->field.bTsfTicking = 0;
	RTUSBWriteMACRegister(pAd, TXRX_CSR9, csr9->word);

	RTUSBWriteMACRegister(pAd, HW_BEACON_BASE0, 0); // invalidate BEACON0 owner/valid bit to prevent garbage
	RTUSBWriteMACRegister(pAd, HW_BEACON_BASE1, 0); // invalidate BEACON1 owner/valid bit to prevent garbage
	RTUSBWriteMACRegister(pAd, HW_BEACON_BASE2, 0); // invalidate BEACON2 owner/valid bit to prevent garbage
	RTUSBWriteMACRegister(pAd, HW_BEACON_BASE3, 0); // invalidate BEACON3 owner/valid bit to prevent garbage

	// move BEACON TXD and frame content to on-chip memory
	ptr = (PUCHAR)&pAd->BeaconTxD;
	for (i = 0; i < TXINFO_SIZE; i += 4)  // 24-byte TXINFO field
	{
		RTUSBMultiWrite(pAd, HW_BEACON_BASE0 + i, ptr, 4);
		ptr += 4;
	}

	// start right after the 24-byte TXINFO field
	ptr = pAd->BeaconBuf;
	for (i = 0; i < pAd->BeaconTxD.DataByteCnt; i += 4)
	{
		RTUSBMultiWrite(pAd, HW_BEACON_BASE0 + TXINFO_SIZE + i, ptr, 4);
		ptr += 4;
	}

	//
	// For Wi-Fi faily generated beacons between participating stations.
	// Set TBTT phase adaptive adjustment step to 8us (default 16us)
	//
	RTUSBWriteMACRegister(pAd, TXRX_CSR10, 0x00001008);

	// start sending BEACON
	/* ASIC register in units of 1/16 TU */
	csr9->field.BeaconInterval = pAd->PortCfg.BeaconPeriod << 4;
	csr9->field.bTsfTicking = 1;
	csr9->field.TsfSyncMode = 2; /* sync TSF in IBSS mode */
	csr9->field.bTBTTEnable = 1;
	csr9->field.bBeaconGen = 1;
	RTUSBWriteMACRegister(pAd, TXRX_CSR9, csr9->word);
	kfree(csr9);

}

/*
	==========================================================================
	Description:
	==========================================================================
 */
VOID AsicSetEdcaParm(
	IN PRTMP_ADAPTER pAd,
	IN PEDCA_PARM	 pEdcaParm)
{
	AC_TXOP_CSR0_STRUC csr0;
	AC_TXOP_CSR1_STRUC csr1;
	AIFSN_CSR_STRUC    AifsnCsr;
	CWMIN_CSR_STRUC    CwminCsr;
	CWMAX_CSR_STRUC    CwmaxCsr;

	DBGPRINT(RT_DEBUG_TRACE,"AsicSetEdcaParm\n");
	if ((pEdcaParm == NULL) || (pEdcaParm->bValid == FALSE) || (pAd->PortCfg.bWmmCapable == FALSE))
	{
		OPSTATUS_CLEAR_FLAG(pAd, fOP_STATUS_WMM_INUSED);

		csr0.field.Ac0Txop = 0; 	// QID_AC_BE
		csr0.field.Ac1Txop = 0; 	// QID_AC_BK
		RTUSBWriteMACRegister(pAd, AC_TXOP_CSR0, csr0.word);
		if (pAd->PortCfg.PhyMode == PHY_11B)
		{
			csr1.field.Ac2Txop = cpu2le16(192); 	// AC_VI: 192*32us ~= 6ms
			csr1.field.Ac3Txop = cpu2le16(96);		// AC_VO: 96*32us  ~= 3ms
		}
		else
		{
			csr1.field.Ac2Txop = cpu2le16(96);		// AC_VI: 96*32us ~= 3ms
			csr1.field.Ac3Txop = cpu2le16(48);		// AC_VO: 48*32us ~= 1.5ms
		}
		RTUSBWriteMACRegister(pAd, AC_TXOP_CSR1, csr1.word);

		CwminCsr.word = 0;
		CwminCsr.field.Cwmin0 = CW_MIN_IN_BITS;
		CwminCsr.field.Cwmin1 = CW_MIN_IN_BITS;
		CwminCsr.field.Cwmin2 = CW_MIN_IN_BITS;
		CwminCsr.field.Cwmin3 = CW_MIN_IN_BITS;
		RTUSBWriteMACRegister(pAd, CWMIN_CSR, CwminCsr.word);

		CwmaxCsr.word = 0;
		CwmaxCsr.field.Cwmax0 = CW_MAX_IN_BITS;
		CwmaxCsr.field.Cwmax1 = CW_MAX_IN_BITS;
		CwmaxCsr.field.Cwmax2 = CW_MAX_IN_BITS;
		CwmaxCsr.field.Cwmax3 = CW_MAX_IN_BITS;
		RTUSBWriteMACRegister(pAd, CWMAX_CSR, CwmaxCsr.word);

		RTUSBWriteMACRegister(pAd, AIFSN_CSR, 0x00002222);

		memset(&pAd->PortCfg.APEdcaParm, 0, sizeof(EDCA_PARM));
	}
	else
	{
		OPSTATUS_SET_FLAG(pAd, fOP_STATUS_WMM_INUSED);

		//
		// Modify Cwmin/Cwmax/Txop on queue[QID_AC_VI], Recommend by Jerry 2005/07/27
		// To degrade our VIDO Queue's throughput for WiFi WMM S3T07 Issue.
		//
		pEdcaParm->Cwmin[QID_AC_VI] += 1;
		pEdcaParm->Cwmax[QID_AC_VI] += 1;
		pEdcaParm->Txop[QID_AC_VI] = pEdcaParm->Txop[QID_AC_VI] * 7 / 10;

		csr0.field.Ac0Txop = cpu2le16(pEdcaParm->Txop[QID_AC_BE]);
		csr0.field.Ac1Txop = cpu2le16(pEdcaParm->Txop[QID_AC_BK]);
		RTUSBWriteMACRegister(pAd, AC_TXOP_CSR0, csr0.word);

		csr1.field.Ac2Txop = cpu2le16(pEdcaParm->Txop[QID_AC_VI]);
		csr1.field.Ac3Txop = cpu2le16(pEdcaParm->Txop[QID_AC_VO]);
		RTUSBWriteMACRegister(pAd, AC_TXOP_CSR1, csr1.word);

		CwminCsr.word = 0;
		CwminCsr.field.Cwmin0 = pEdcaParm->Cwmin[QID_AC_BE];
		CwminCsr.field.Cwmin1 = pEdcaParm->Cwmin[QID_AC_BK];
		CwminCsr.field.Cwmin2 = pEdcaParm->Cwmin[QID_AC_VI];
		CwminCsr.field.Cwmin3 = pEdcaParm->Cwmin[QID_AC_VO];
		RTUSBWriteMACRegister(pAd, CWMIN_CSR, CwminCsr.word);

		CwmaxCsr.word = 0;
		CwmaxCsr.field.Cwmax0 = pEdcaParm->Cwmax[QID_AC_BE];
		CwmaxCsr.field.Cwmax1 = pEdcaParm->Cwmax[QID_AC_BK];
		CwmaxCsr.field.Cwmax2 = pEdcaParm->Cwmax[QID_AC_VI];
		CwmaxCsr.field.Cwmax3 = pEdcaParm->Cwmax[QID_AC_VO];
		RTUSBWriteMACRegister(pAd, CWMAX_CSR, CwmaxCsr.word);

		AifsnCsr.word = 0;
		AifsnCsr.field.Aifsn0 = pEdcaParm->Aifsn[QID_AC_BE];
		AifsnCsr.field.Aifsn1 = pEdcaParm->Aifsn[QID_AC_BK];
		AifsnCsr.field.Aifsn2 = pEdcaParm->Aifsn[QID_AC_VI];
		AifsnCsr.field.Aifsn3 = pEdcaParm->Aifsn[QID_AC_VO];
		RTUSBWriteMACRegister(pAd, AIFSN_CSR, AifsnCsr.word);

		memcpy(&pAd->PortCfg.APEdcaParm, pEdcaParm, sizeof(EDCA_PARM));

		DBGPRINT(RT_DEBUG_TRACE,"EDCA [#%d]: AIFSN CWmin CWmax	TXOP(us)  ACM\n", pEdcaParm->EdcaUpdateCount);
		DBGPRINT(RT_DEBUG_TRACE,"	 AC_BE	   %d	  %d	 %d 	%4d 	%d\n",
			pEdcaParm->Aifsn[0],
			pEdcaParm->Cwmin[0],
			pEdcaParm->Cwmax[0],
			pEdcaParm->Txop[0]<<5,
			pEdcaParm->bACM[0]);
		DBGPRINT(RT_DEBUG_TRACE,"	 AC_BK	   %d	  %d	 %d 	%4d 	%d\n",
			pEdcaParm->Aifsn[1],
			pEdcaParm->Cwmin[1],
			pEdcaParm->Cwmax[1],
			pEdcaParm->Txop[1]<<5,
			pEdcaParm->bACM[1]);
		DBGPRINT(RT_DEBUG_TRACE,"	 AC_VI	   %d	  %d	 %d 	%4d 	%d\n",
			pEdcaParm->Aifsn[2],
			pEdcaParm->Cwmin[2],
			pEdcaParm->Cwmax[2],
			pEdcaParm->Txop[2]<<5,
			pEdcaParm->bACM[2]);
		DBGPRINT(RT_DEBUG_TRACE,"	 AC_VO	   %d	  %d	 %d 	%4d 	%d\n",
			pEdcaParm->Aifsn[3],
			pEdcaParm->Cwmin[3],
			pEdcaParm->Cwmax[3],
			pEdcaParm->Txop[3]<<5,
			pEdcaParm->bACM[3]);
	}
}

/*
	==========================================================================
	Description:
	==========================================================================
 */
VOID AsicSetSlotTime(
	IN PRTMP_ADAPTER pAd,
	IN BOOLEAN		 bUseShortSlotTime)
{
	MAC_CSR9_STRUC *Csr9 = kzalloc(sizeof(MAC_CSR9_STRUC), GFP_KERNEL);

	if (!Csr9) {
		DBGPRINT(RT_DEBUG_ERROR, "couldn't allocate memory\n");
		return;
	}
	if ( pAd->PortCfg.Channel > 14)  //check if in the A band
		bUseShortSlotTime = TRUE;

	if (bUseShortSlotTime &&
		OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_SHORT_SLOT_INUSED)) {
		kfree(Csr9);
		return;
	} else if ((!bUseShortSlotTime) &&
		(!OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_SHORT_SLOT_INUSED))) {
		kfree(Csr9);
		return;
	}
	if (bUseShortSlotTime)
		OPSTATUS_SET_FLAG(pAd, fOP_STATUS_SHORT_SLOT_INUSED);
	else
		OPSTATUS_CLEAR_FLAG(pAd, fOP_STATUS_SHORT_SLOT_INUSED);

	RTUSBReadMACRegister(pAd, MAC_CSR9, &Csr9->word);
	Csr9->field.SlotTime = (bUseShortSlotTime)? 9 : 20;

	// force using short SLOT time for FAE to demo performance when TxBurst is ON
	if (pAd->PortCfg.bEnableTxBurst)
		Csr9->field.SlotTime = 9;

	if (pAd->PortCfg.BssType == BSS_ADHOC)
		Csr9->field.SlotTime = 20;

	RTUSBWriteMACRegister(pAd, MAC_CSR9, Csr9->word);


	DBGPRINT(RT_DEBUG_TRACE, "AsicSetSlotTime(=%d us)\n",
				 Csr9->field.SlotTime);
	kfree(Csr9);
}

/*
	==========================================================================
	Description:
		danamic tune BBP R17 to find a balance between sensibility and
		noise isolation

	==========================================================================
 */
VOID AsicBbpTuning(
	IN PRTMP_ADAPTER pAd)
{
	UCHAR R17, R17UpperBound, R17LowerBound;
	int dbm;

    if ((pAd->BbpTuning.bEnable == FALSE) || (pAd->PortCfg.RadarDetect.RDMode != RD_NORMAL_MODE))
        return;

	if (pAd->BbpTuning.bEnable == FALSE)
		return;

	R17 = pAd->BbpWriteLatch[17];

	//
	// I. Work as a STA
	//
	if (pAd->Mlme.CntlMachine.CurrState != CNTL_IDLE)  // no R17 tuning when SCANNING
		return;

	dbm = pAd->PortCfg.AvgRssi - pAd->BbpRssiToDbmDelta;

	//
	// Decide R17 Range.
	//
	if (dbm > -82)
	{
		pAd->BbpTuning.R17LowerBoundG         = 0x1C; // for best RX sensibility
		pAd->BbpTuning.R17UpperBoundG		  = 0x40; // for best RX noise isolation to prevent false CCA
	}
	else if (dbm > -84)
	{
		pAd->BbpTuning.R17LowerBoundG         = 0x1C; // for best RX sensibility
		pAd->BbpTuning.R17UpperBoundG		  = 0x20; // for best RX noise isolation to prevent false CCA
	}
	else
	{
		pAd->BbpTuning.R17LowerBoundG         = 0x1C; // for best RX sensibility
		pAd->BbpTuning.R17UpperBoundG         = 0x1C; // for best RX noise isolation to prevent false CCA
	}

	// external LNA has different R17 base
	if (pAd->NicConfig2.field.ExternalLNA)
	{
		pAd->BbpTuning.R17LowerBoundG += 0x14;
		pAd->BbpTuning.R17UpperBoundG += 0x10;
	}

	if (pAd->LatchRfRegs.Channel <= 14)
	{
		R17UpperBound = pAd->BbpTuning.R17UpperBoundG;
		R17LowerBound = pAd->BbpTuning.R17LowerBoundG;
	}
	else
	{
		R17UpperBound = pAd->BbpTuning.R17UpperBoundA;
		R17LowerBound = pAd->BbpTuning.R17LowerBoundA;
	}

	if (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_MEDIA_STATE_CONNECTED))
	{
#if 0
		// Rule 0. "long distance case"
		//	 when RSSI is too weak, many signals will become false CCA thus affect R17 tuning.
		//	 so in this case, just stop R17 tuning
		if ((dbm < -80) && (pAd->Mlme.PeriodicRound > 20))
		{
			DBGPRINT(RT_DEBUG_TRACE, "weak RSSI=%d, CCA=%d, stop tuning, R17 = 0x%x\n",
				dbm, pAd->RalinkCounters.OneSecFalseCCACnt, R17);
			return;
		}

		// Rule 1. "special big-R17 for short-distance" when not SCANNING
		else
#endif
		if (dbm >= RSSI_FOR_VERY_LOW_SENSIBILITY)
		{
			if (R17 != 0x60) // R17UpperBound)
			{
				R17 = 0x60; // R17UpperBound;
				RTUSBWriteBBPRegister(pAd, 17, R17);
			}
			DBGPRINT(RT_DEBUG_TRACE, "Avg RSSI=%d, BbpRssiToDbmDelta =%d\n", pAd->PortCfg.AvgRssi, pAd->BbpRssiToDbmDelta);
			DBGPRINT(RT_DEBUG_TRACE, " 2 strong RSSI=%d, CCA=%d, fixed R17 at 0x%x\n",
				dbm, pAd->RalinkCounters.OneSecFalseCCACnt, R17);
			return;
		}

		else if (dbm >= RSSI_FOR_LOW_SENSIBILITY)
		{
			if (R17 != R17UpperBound)
			{
				R17 = R17UpperBound;
				RTUSBWriteBBPRegister(pAd, 17, R17);
			}
			DBGPRINT(RT_DEBUG_TRACE, "3 strong RSSI=%d, CCA=%d, fixed R17 at 0x%x\n",
				dbm, pAd->RalinkCounters.OneSecFalseCCACnt, R17);
			return;
		}

		else if (dbm >= RSSI_FOR_MID_LOW_SENSIBILITY)
		{
			if (R17 != (R17LowerBound + 0x10))
			{
				R17 = R17LowerBound + 0x10;
				RTUSBWriteBBPRegister(pAd, 17, R17);
			}
			DBGPRINT(RT_DEBUG_TRACE, "mid RSSI=%d, CCA=%d, fixed R17 at 0x%x\n",
				dbm, pAd->RalinkCounters.OneSecFalseCCACnt, R17);
			return;
		}

		// Rule 2. "middle-R17 for mid-distance" when not SCANNING
		else if (dbm >= RSSI_FOR_MID_SENSIBILITY)
		{
			if (R17 != (R17LowerBound + 0x10))
			{
				R17 = R17LowerBound + 0x08;
				RTUSBWriteBBPRegister(pAd, 17, R17);
			}
			DBGPRINT(RT_DEBUG_TRACE, "mid RSSI=%d, CCA=%d, fixed R17 at 0x%x\n",
				dbm, pAd->RalinkCounters.OneSecFalseCCACnt, R17);
			return;
		}

		// Rule 2.1 originated from RT2500 Netopia case which changes R17UpperBound according
		//			to RSSI.
		else
		{
			// lower R17UpperBound when RSSI weaker than -70 dbm
			R17UpperBound -= 2*(RSSI_FOR_MID_SENSIBILITY - dbm);
			if (R17UpperBound < R17LowerBound)
				R17UpperBound = R17LowerBound;

			if (R17 > R17UpperBound)
			{
				R17 = R17UpperBound;
				RTUSBWriteBBPRegister(pAd, 17, R17);
				DBGPRINT(RT_DEBUG_TRACE, "RSSI=%d, CCA=%d, R17=R17UpperBound=0x%x\n", dbm, pAd->RalinkCounters.OneSecFalseCCACnt, R17);
				return;
			}

			// if R17 not exceeds R17UpperBound yet, then goes down to Rule 3 "R17 tuning based on False CCA"
		}

	}

	// Rule 3. otherwise, R17 is currenly in dynamic tuning range
	//	  Keep dynamic tuning based on False CCA counter

	if ((pAd->RalinkCounters.OneSecFalseCCACnt > pAd->BbpTuning.FalseCcaUpperThreshold) &&
		(R17 < R17UpperBound))
	{
		R17 += pAd->BbpTuning.R17Delta;

		if (R17 >= R17UpperBound)
			R17 = R17UpperBound;

		RTUSBWriteBBPRegister(pAd, 17, R17);
		DBGPRINT(RT_DEBUG_TRACE, "RSSI=%d, CCA=%d, ++R17= 0x%x\n", dbm, pAd->RalinkCounters.OneSecFalseCCACnt, R17);
	}
	else if ((pAd->RalinkCounters.OneSecFalseCCACnt < pAd->BbpTuning.FalseCcaLowerThreshold) &&
		(R17 > R17LowerBound))
	{
		R17 -= pAd->BbpTuning.R17Delta;

		if (R17 <= R17LowerBound)
			R17 = R17LowerBound;

		RTUSBWriteBBPRegister(pAd, 17, R17);
		DBGPRINT(RT_DEBUG_TRACE, "RSSI=%d, CCA=%d, --R17= 0x%x\n", dbm, pAd->RalinkCounters.OneSecFalseCCACnt, R17);
	}
	else
	{
		DBGPRINT(RT_DEBUG_TRACE, "RSSI=%d, CCA=%d, keep R17 at 0x%x\n", dbm, pAd->RalinkCounters.OneSecFalseCCACnt, R17);
	}

}

VOID AsicAddSharedKeyEntry(
	IN PRTMP_ADAPTER pAd,
	IN UCHAR		 BssIndex,
	IN UCHAR		 KeyIdx,
	IN UCHAR		 CipherAlg,
	IN PUCHAR		 pKey,
	IN PUCHAR		 pTxMic,
	IN PUCHAR		 pRxMic)
{
	INT   i;
	ULONG offset;
	ULONG *csr0 = kzalloc(sizeof(ULONG), GFP_KERNEL);
	SEC_CSR1_STRUC *csr1 = kzalloc(sizeof(SEC_CSR1_STRUC), GFP_KERNEL);



		union aaa {
		ULONG	temp_ul;
		struct bbb{
#ifdef BIG_ENDIAN
			UCHAR ch4;
			UCHAR ch3;
			UCHAR ch2;
			UCHAR ch1;
#else
			UCHAR ch1;
			UCHAR ch2;
			UCHAR ch3;
			UCHAR ch4;
#endif
			} temp_uc;
	} ddd;

	DBGPRINT(RT_DEBUG_TRACE, "AsicAddSharedKeyEntry: %s key #%d\n", CipherName[CipherAlg], BssIndex*4 + KeyIdx);

	if (!csr0 || !csr1) {
		DBGPRINT(RT_DEBUG_ERROR, "couldn't allocate memory\n");
		return;
	}

	DBGPRINT(RT_DEBUG_TRACE, "   Key =");
	for (i = 0; i < 16; i++)
	{
		DBGPRINT_RAW(RT_DEBUG_TRACE, "%02x:", pKey[i]);
	}
	DBGPRINT_RAW(RT_DEBUG_TRACE, "\n");
	if (pRxMic)
	{
		DBGPRINT(RT_DEBUG_TRACE, "   Rx MIC Key = ");
		for (i = 0; i < 8; i++)
		{
			DBGPRINT_RAW(RT_DEBUG_TRACE, "%02x:", pRxMic[i]);
		}
		DBGPRINT_RAW(RT_DEBUG_TRACE, "\n");
	}
	if (pTxMic)
	{
		DBGPRINT(RT_DEBUG_TRACE, "   Tx MIC Key = ");
		for (i = 0; i < 8; i++)
		{
			DBGPRINT_RAW(RT_DEBUG_TRACE, "%02x:", pTxMic[i]);
		}
		DBGPRINT_RAW(RT_DEBUG_TRACE, "\n");
	}

	//
	// enable this key entry
	//
	RTUSBReadMACRegister(pAd, SEC_CSR0, csr0);
	*csr0 = *csr0 & ~BIT32[BssIndex*4 + KeyIdx]; /* turn off the valid bit*/
	RTUSBWriteMACRegister(pAd, SEC_CSR0, *csr0);


	//
	// fill key material - key + TX MIC + RX MIC
	//
	offset = SHARED_KEY_TABLE_BASE + (4*BssIndex + KeyIdx)*HW_KEY_ENTRY_SIZE;

	/* For big endian machines, we need to pre- swap the key data so that
	 * the correct byte order is restored after endianization. */
	for (i=0; i<MAX_LEN_OF_SHARE_KEY; i = i+4)
	{
		ddd.temp_uc.ch1  = pKey[i];
		ddd.temp_uc.ch2 =  pKey[i+1];
		ddd.temp_uc.ch3 =  pKey[i+2];
		ddd.temp_uc.ch4 =  pKey[i+3];

		RTUSBWriteMACRegister(pAd, (USHORT) (offset + i), ddd.temp_ul);
	}

	offset += MAX_LEN_OF_SHARE_KEY;
	if (pTxMic)
	{
		for (i=0; i<8; i=i+4)
		{
			ddd.temp_uc.ch1  = pTxMic[i];
			ddd.temp_uc.ch2 =  pTxMic[i+1];
			ddd.temp_uc.ch3 =  pTxMic[i+2];
			ddd.temp_uc.ch4 =  pTxMic[i+3];
			RTUSBWriteMACRegister(pAd, (USHORT) (offset + i), ddd.temp_ul);
		}
	}

	offset += 8;
	if (pRxMic)
	{
		for (i=0; i<8; i = i+4)
		{
			ddd.temp_uc.ch1  = pRxMic[i];
			ddd.temp_uc.ch2 =  pRxMic[i+1];
			ddd.temp_uc.ch3 =  pRxMic[i+2];
			ddd.temp_uc.ch4 =  pRxMic[i+3];

			RTUSBWriteMACRegister(pAd, (USHORT) (offset + i), ddd.temp_ul);
		}
	}

	//
	// Update cipher algorithm. STA always use BSS0
	//
	RTUSBReadMACRegister(pAd, SEC_CSR1, &csr1->word);
	if (KeyIdx == 0)
		csr1->field.Bss0Key0CipherAlg = CipherAlg;
	else if (KeyIdx == 1)
		csr1->field.Bss0Key1CipherAlg = CipherAlg;
	else if (KeyIdx == 2)
		csr1->field.Bss0Key2CipherAlg = CipherAlg;
	else
		csr1->field.Bss0Key3CipherAlg = CipherAlg;
	RTUSBWriteMACRegister(pAd, SEC_CSR1, csr1->word);

	//
	// enable this key entry
	//
	RTUSBReadMACRegister(pAd, SEC_CSR0, csr0);
	*csr0 |= BIT32[BssIndex*4 + KeyIdx]; 	/* turn on the valid bit */
	RTUSBWriteMACRegister(pAd, SEC_CSR0, *csr0);

	kfree(csr0);
	kfree(csr1);
}

VOID AsicRemoveSharedKeyEntry(
	IN PRTMP_ADAPTER pAd,
	IN UCHAR		 BssIndex,
	IN UCHAR		 KeyIdx)
{
	ULONG *SecCsr0 = kzalloc(sizeof(ULONG), GFP_KERNEL);

	if (!SecCsr0) {
		printk("Unable to allocate memory for wireless\n");
		return;
	}
	DBGPRINT(RT_DEBUG_TRACE,"AsicRemoveSharedKeyEntry: #%d \n", BssIndex*4 + KeyIdx);

	ASSERT(BssIndex < 4);
	ASSERT(KeyIdx < SHARE_KEY_NUM);

	RTUSBReadMACRegister(pAd, SEC_CSR0, SecCsr0);
	*SecCsr0 &= (~BIT32[BssIndex*4 + KeyIdx]); // clear the valid bit
	RTUSBWriteMACRegister(pAd, SEC_CSR0, *SecCsr0);
	kfree(SecCsr0);
}

VOID AsicAddPairwiseKeyEntry(
	IN PRTMP_ADAPTER pAd,
	IN PUCHAR		 pAddr,
	IN UCHAR		 KeyIdx,
	IN UCHAR		 CipherAlg,
	IN PUCHAR		 pKey,
	IN PUCHAR		 pTxMic,
	IN PUCHAR		 pRxMic)
{
	ULONG offset;
	ULONG *csr2 = kmalloc(sizeof(ULONG), GFP_KERNEL);
	ULONG *csr3 = kmalloc(sizeof(ULONG), GFP_KERNEL);

	if (!csr2 || !csr3) {
		DBGPRINT(RT_DEBUG_ERROR, "couldn't allocate memory\n");
		return;
	}


	DBGPRINT(RT_DEBUG_TRACE,"AsicAddPairwiseKeyEntry: Entry#%d Alg=%s mac=%02x:%02x:%02x:%02x:%02x:%02x\n",
		KeyIdx, CipherName[CipherAlg], pAddr[0], pAddr[1], pAddr[2], pAddr[3], pAddr[4], pAddr[5]);
	DBGPRINT(RT_DEBUG_TRACE,"  Key	  = %02x:%02x:%02x:%02x:...\n", pKey[0],pKey[1],pKey[2],pKey[3]);
	DBGPRINT(RT_DEBUG_TRACE,"  TxMIC  = %02x:%02x:%02x:%02x:...\n", pTxMic[0],pTxMic[1],pTxMic[2],pTxMic[3]);
	DBGPRINT(RT_DEBUG_TRACE,"  pRxMic = %02x:%02x:%02x:%02x:...\n", pRxMic[0],pRxMic[1],pRxMic[2],pRxMic[3]);

	offset = PAIRWISE_KEY_TABLE_BASE + (KeyIdx * HW_KEY_ENTRY_SIZE);

	RTUSBMultiWrite(pAd, (USHORT)(offset), pKey, HW_KEY_ENTRY_SIZE);

	offset = PAIRWISE_TA_TABLE_BASE + (KeyIdx * HW_PAIRWISE_TA_ENTRY_SIZE);
	RTUSBMultiWrite(pAd, (USHORT)(offset), pAddr, MAC_ADDR_LEN);

	RTUSBMultiWrite(pAd, (USHORT)(offset+MAC_ADDR_LEN), &CipherAlg, 1);

	// enable this entry
	if (KeyIdx < 32)
	{
		RTUSBReadMACRegister(pAd, SEC_CSR2, csr2);
		*csr2 |= BIT32[KeyIdx];
		RTUSBWriteMACRegister(pAd, SEC_CSR2, *csr2);
	}
	else
	{
		RTUSBReadMACRegister(pAd, SEC_CSR3, csr3);
		*csr3 |= BIT32[KeyIdx-32];
		RTUSBWriteMACRegister(pAd, SEC_CSR3, *csr3);
	}
	kfree(csr2);
	kfree(csr3);
}

VOID AsicRemovePairwiseKeyEntry(
	IN PRTMP_ADAPTER pAd,
	IN UCHAR		 KeyIdx)
{
	ULONG csr2, csr3;

	DBGPRINT(RT_DEBUG_INFO,"AsicRemovePairwiseKeyEntry: #%d \n", KeyIdx);

	// invalidate this entry
	if (KeyIdx < 32)
	{
		RTUSBReadMACRegister(pAd, SEC_CSR2, &csr2);
		csr2 &= (~BIT32[KeyIdx]);
		RTUSBWriteMACRegister(pAd, SEC_CSR2, csr2);
	}
	else
	{
		RTUSBReadMACRegister(pAd, SEC_CSR3, &csr3);
		csr3 &= (~BIT32[KeyIdx-32]);
		RTUSBWriteMACRegister(pAd, SEC_CSR3, csr3);
	}
}

/*
	========================================================================

	Routine Description:
		Verify the support rate for different PHY type

	Arguments:
		pAd 				Pointer to our adapter

	Return Value:
		None

	========================================================================
*/
VOID	RTMPCheckRates(
	IN		PRTMP_ADAPTER	pAd,
	IN OUT	UCHAR			SupRate[],
	IN OUT	UCHAR			*SupRateLen)
{
	UCHAR	RateIdx, i, j;
	UCHAR	NewRate[12], NewRateLen;

	NewRateLen = 0;

	if (pAd->PortCfg.PhyMode == PHY_11B)
		RateIdx = 4;
//	else if ((pAd->PortCfg.PhyMode == PHY_11BG_MIXED) &&
//		(pAd->PortCfg.BssType == BSS_ADHOC) 		  &&
//		(pAd->PortCfg.AdhocMode == 0))
//		RateIdx = 4;
	else
		RateIdx = 12;

	// Check for support rates exclude basic rate bit
	for (i = 0; i < *SupRateLen; i++)
		for (j = 0; j < RateIdx; j++)
			if ((SupRate[i] & 0x7f) == RateIdTo500Kbps[j])
				NewRate[NewRateLen++] = SupRate[i];

	*SupRateLen = NewRateLen;
	memcpy(SupRate, NewRate, NewRateLen);
}

/*
	========================================================================

	Routine Description:
		Set Rx antenna for software diversity

	Arguments:
		pAd 				Pointer to our adapter
		Pair1				0: for E1;	1:for E2
		Pair2				0: for E4;	1:for E3

	Return Value:
		None

	========================================================================
*/
VOID AsicSetRxAnt(
	IN PRTMP_ADAPTER	pAd,
	IN UCHAR			Pair1,
	IN UCHAR			Pair2)
{
	DBGPRINT(RT_DEBUG_INFO, "AsicSetRxAnt, pair1=%d, pair2=%d\n", Pair1, Pair2);

	if (pAd->RfIcType == RFIC_2528)
	{
		UCHAR		R77;

		// Update antenna registers
		RTUSBReadBBPRegister(pAd, BBP_R77, &R77);

		R77	&= ~0x03;		// clear Bit 0,1
		if (Pair1 == 0)
		{
			R77	= R77 | 0x03;	// <Bit1:Bit0> = <1:1>
		}
		else
		{
			//R77;				// <Bit1:Bit0> = <0:0>
		}

		// Disable Rx
		RT73WriteTXRXCSR0(pAd, TRUE, FALSE);
		RTUSBWriteBBPRegister(pAd, BBP_R77, R77);

		RT73WriteTXRXCSR0(pAd, FALSE, TRUE); // Staion not drop control frame will fail WiFi Certification.
	}
	else if (pAd->RfIcType == RFIC_5226)
	{
		UCHAR		R77;

		// Update antenna registers
		RTUSBReadBBPRegister(pAd, BBP_R77, &R77);

		R77	&= ~0x03;		// clear Bit 0,1

		//Support 11B/G/A
		if (pAd->PortCfg.BandState == BG_BAND)
		{
			//Check Rx Anttena
			if (Pair1 == 0)
			{
				R77 = R77 | 0x03;	// <Bit1:Bit0> = <1:1>
			}
			else
			{
				//R77;				// <Bit1:Bit0> = <0:0>
			}
		}
		else //A_BAND
		{
			//Check Rx Anttena
			if (Pair1 == 0)
			{
				//R77;				// <Bit1:Bit0> = <0:0>
			}
			else
			{
				R77	= R77 | 0x03;	// <Bit1:Bit0> = <1:1>
			}
		}

		// Disable Rx
		RT73WriteTXRXCSR0(pAd, TRUE, FALSE);
		RTUSBWriteBBPRegister(pAd, BBP_R77, R77);

		// enable RX of MAC block
		RT73WriteTXRXCSR0(pAd, FALSE, TRUE);	// Staion not drop control frame will fail WiFi Certification.
	}
	else if (pAd->RfIcType == RFIC_5225)
	{
		UCHAR		R77;

		// Update antenna registers
		RTUSBReadBBPRegister(pAd, BBP_R77, &R77);

		R77	&= ~0x03;		// clear Bit 0,1

		//Support 11B/G/A
		if (pAd->PortCfg.BandState == BG_BAND)
		{
			//Check Rx Anttena
			if (Pair1 == 0)
			{
				R77 = R77 | 0x03;	// <Bit1:Bit0> = <1:1>
			}
			else
			{
				//R77;				// <Bit1:Bit0> = <0:0>
			}
		}
		else //A_BAND
		{
			//Check Rx Anttena
			if (Pair1 == 0)
			{
				//R77;				// <Bit1:Bit0> = <0:0>
			}
			else
			{
				R77	= R77 | 0x03;	// <Bit1:Bit0> = <1:1>
			}
		}

		// Disable Rx
		RT73WriteTXRXCSR0(pAd, TRUE, FALSE);
		RTUSBWriteBBPRegister(pAd, BBP_R77, R77);

		// enable RX of MAC block
		RT73WriteTXRXCSR0(pAd, FALSE, TRUE);	// Staion not drop control frame will fail WiFi Certification.

	}
	else if (pAd->RfIcType == RFIC_2527)
	{
		UCHAR		R77;

		// Update antenna registers
		RTUSBReadBBPRegister(pAd, BBP_R77, &R77);

		R77	&= ~0x03;		// clear Bit 0,1
		if (Pair1 == 0)
		{
			R77	= R77 | 0x03;	// <Bit1:Bit0> = <1:1>
		}
		else
		{
			//R77;				// <Bit1:Bit0> = <0:0>
		}

		// Disable Rx
		RT73WriteTXRXCSR0(pAd, TRUE, FALSE);
		RTUSBWriteBBPRegister(pAd, BBP_R77, R77);

		// enable RX of MAC block
		RT73WriteTXRXCSR0(pAd, FALSE, TRUE); // Staion not drop control frame will fail WiFi Certification.

	}
}

// switch to secondary RxAnt pair for a while to collect it's average RSSI
// also set a timeout routine to do the actual evaluation. If evaluation
// result shows a much better RSSI using secondary RxAnt, then a official
// RX antenna switch is performed.
VOID AsicEvaluateSecondaryRxAnt(
	IN PRTMP_ADAPTER pAd)
{
	DBGPRINT(RT_DEBUG_TRACE,"AntDiv - before evaluate Pair1-Ant (%d,%d), Pair2-Ant (%d,%d)\n",
		pAd->RxAnt.Pair1PrimaryRxAnt, pAd->RxAnt.Pair1SecondaryRxAnt,pAd->RxAnt.Pair2PrimaryRxAnt, pAd->RxAnt.Pair2SecondaryRxAnt);

	AsicSetRxAnt(pAd, pAd->RxAnt.Pair1SecondaryRxAnt, 0xFF);

	pAd->RxAnt.EvaluatePeriod = 1; //1:Means switch to SecondaryRxAnt, 0:Means switch to Pair1PrimaryRxAnt
	pAd->RxAnt.FirstPktArrivedWhenEvaluate = FALSE;
	pAd->RxAnt.RcvPktNumWhenEvaluate = 0;

	// a one-shot timer to end the evalution
	if (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_MEDIA_STATE_CONNECTED))
		RTMPSetTimer(pAd, &pAd->RxAnt.RxAntDiversityTimer, 100);
	else
		RTMPSetTimer(pAd, &pAd->RxAnt.RxAntDiversityTimer, 300);

}

// this timeout routine collect AvgRssi[SecondaryRxAnt] and decide the best Ant
VOID AsicRxAntEvalTimeout(
	IN	unsigned long data)

{
	RTMP_ADAPTER	*pAd = (RTMP_ADAPTER *)data;

	RTUSBEnqueueInternalCmd(pAd, RT_PERFORM_SOFT_DIVERSITY);
}

VOID AsicRxAntEvalAction(
	IN PRTMP_ADAPTER pAd)
{
	UCHAR			temp;

	DBGPRINT(RT_DEBUG_TRACE,"After Eval,RSSI[0,1,2,3]=<%d,%d,%d,%d>,RcvPktNumWhenEvaluate=%d\n",
		(pAd->RxAnt.Pair1AvgRssi[0] >> 3), (pAd->RxAnt.Pair1AvgRssi[1] >> 3),
		(pAd->RxAnt.Pair2AvgRssi[0] >> 3), (pAd->RxAnt.Pair2AvgRssi[1] >> 3), pAd->RxAnt.RcvPktNumWhenEvaluate);

	// Do nothing if the driver is starting halt state.
	// This might happen when timer already been fired before cancel timer with mlmehalt
	if (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_HALT_IN_PROGRESS))
		return;

	if ((pAd->RxAnt.RcvPktNumWhenEvaluate != 0) && (pAd->RxAnt.Pair1AvgRssi[pAd->RxAnt.Pair1SecondaryRxAnt] >= pAd->RxAnt.Pair1AvgRssi[pAd->RxAnt.Pair1PrimaryRxAnt]))
	{
		//
		// select PrimaryRxAntPair
		//	  Role change, Used Pair1SecondaryRxAnt as PrimaryRxAntPair.
		//	  Since Pair1SecondaryRxAnt Quality good than Pair1PrimaryRxAnt
		//
		temp = pAd->RxAnt.Pair1PrimaryRxAnt;
		pAd->RxAnt.Pair1PrimaryRxAnt = pAd->RxAnt.Pair1SecondaryRxAnt;
		pAd->RxAnt.Pair1SecondaryRxAnt = temp;

		pAd->RxAnt.Pair1LastAvgRssi = (pAd->RxAnt.Pair1AvgRssi[pAd->RxAnt.Pair1SecondaryRxAnt] >> 3);
	}
	else
	{
		AsicSetRxAnt(pAd, pAd->RxAnt.Pair1PrimaryRxAnt, 0xFF);
	}

	pAd->RxAnt.EvaluatePeriod = 0; //1:Means switch to SecondaryRxAnt, 0:Means switch to Pair1PrimaryRxAnt

	DBGPRINT(RT_DEBUG_TRACE,"After Eval-Pair1 #%d,Pair2 #%d\n",pAd->RxAnt.Pair1PrimaryRxAnt,pAd->RxAnt.Pair2PrimaryRxAnt);
}

/*
	========================================================================
	Routine Description:
		Station side, Auto TxRate faster train up timer call back function.
	Return Value:
		None
	========================================================================
*/
VOID StaQuickResponeForRateUpExec(
	IN	unsigned long data)
{
	PRTMP_ADAPTER	pAd = (PRTMP_ADAPTER)data;
	UCHAR	UpRate, DownRate, CurrRate;
	ULONG	TxTotalCnt, TxErrorRatio = 0;

	do
	{
		//
		// Only link up will to do the TxRate faster trains up.
		//
		if (!OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_MEDIA_STATE_CONNECTED))
			break;

		TxTotalCnt = pAd->RalinkCounters.OneSecTxNoRetryOkCount +
					 pAd->RalinkCounters.OneSecTxRetryOkCount +
					 pAd->RalinkCounters.OneSecTxFailCount;

		// skip this time that has no traffic in the past period
		if (TxTotalCnt == 0)
		{
			break;
		}

		// decide the next upgrade rate and downgrade rate, if any
		CurrRate = pAd->PortCfg.TxRate;
		if ((pAd->PortCfg.Channel > 14) ||		// must be in 802.11A band
			(pAd->PortCfg.PhyMode == PHY_11G))	// G-only mode, no CCK rates available
		{
			if (Phy11ANextRateUpward[CurrRate] <= pAd->PortCfg.MaxTxRate)
				UpRate = Phy11ANextRateUpward[CurrRate];
			else
				UpRate = CurrRate;
			DownRate = Phy11ANextRateDownward[CurrRate];
		}
		else
		{
			if (pAd->PortCfg.MaxTxRate < RATE_FIRST_OFDM_RATE)
			{
				if (Phy11BNextRateUpward[CurrRate] <= pAd->PortCfg.MaxTxRate)
					UpRate = Phy11BNextRateUpward[CurrRate];
				else
					UpRate = CurrRate;
				DownRate = Phy11BNextRateDownward[CurrRate];
			}
			else
			{
				if (Phy11BGNextRateUpward[CurrRate] <= pAd->PortCfg.MaxTxRate)
					UpRate = Phy11BGNextRateUpward[CurrRate];
				else
					UpRate = CurrRate;
				DownRate = Phy11BGNextRateDownward[CurrRate];
			}
		}

		//
		// PART 1. Decide TX Quality
		//	 decide TX quality based on Tx PER when enough samples are available
		//
		if (TxTotalCnt > 15)
		{
			TxErrorRatio = ((pAd->RalinkCounters.OneSecTxRetryOkCount + pAd->RalinkCounters.OneSecTxFailCount) * 100) / TxTotalCnt;
			// downgrade TX quality if PER >= Rate-Down threshold
			if (TxErrorRatio >= RateDownPER[CurrRate])
			{
				pAd->DrsCounters.TxQuality[CurrRate] = DRS_TX_QUALITY_WORST_BOUND;
			}
		}

		pAd->DrsCounters.PER[CurrRate] = (UCHAR)TxErrorRatio;

		//
		// PART 2. Perform TX rate switching
		//	 perform rate switching
		//
		if ((pAd->DrsCounters.TxQuality[CurrRate] >= DRS_TX_QUALITY_WORST_BOUND) && (CurrRate != DownRate))
		{
			pAd->PortCfg.TxRate = DownRate;
		}
		// PART 3. Post-processing if TX rate switching did happen
		//	   if rate-up happen, clear all bad history of all TX rates
		//	   if rate-down happen, only clear DownRate's bad history

		if (pAd->PortCfg.TxRate < CurrRate)
		{
			DBGPRINT(RT_DEBUG_TRACE,"StaQuickResponeForRateUpExec: Before TX rate = %d Mbps, Now Tx rate = %d Mbps\n", RateIdToMbps[CurrRate], RateIdToMbps[pAd->PortCfg.TxRate]);

			// shorter stable time require more penalty in next rate UP criteria
			//if (pAd->DrsCounters.CurrTxRateStableTime < 4)	  // less then 4 sec
			//	  pAd->DrsCounters.TxRateUpPenalty = DRS_PENALTY; // add 8 sec penalty
			//else if (pAd->DrsCounters.CurrTxRateStableTime < 8) // less then 8 sec
			//	  pAd->DrsCounters.TxRateUpPenalty = 2; 		  // add 2 sec penalty
			//else
				pAd->DrsCounters.TxRateUpPenalty = 0;			// no penalty

			pAd->DrsCounters.CurrTxRateStableTime = 0;
			pAd->DrsCounters.LastSecTxRateChangeAction = 2; // rate down
			pAd->DrsCounters.TxQuality[pAd->PortCfg.TxRate] = 0;
			pAd->DrsCounters.PER[pAd->PortCfg.TxRate] = 0;
		}
		else
			pAd->DrsCounters.LastSecTxRateChangeAction = 0; // rate no change

		// reset all OneSecxxx counters
		pAd->RalinkCounters.OneSecTxFailCount = 0;
		pAd->RalinkCounters.OneSecTxNoRetryOkCount = 0;
		pAd->RalinkCounters.OneSecTxRetryOkCount = 0;
	} while (FALSE);
	pAd->PortCfg.QuickResponeForRateUpTimerRunning = FALSE;
}

/*
	========================================================================

	Routine Description:
		Get an unused nonpaged system-space memory for use

	Arguments:
		pAd 				Pointer to our adapter
		AllocVa 			Pointer to the base virtual address for later use

	Return Value:
		NDIS_STATUS_SUCCESS
		NDIS_STATUS_FAILURE
		NDIS_STATUS_RESOURCES

	Note:

	========================================================================
*/

NDIS_STATUS MlmeAllocateMemory(
	IN PRTMP_ADAPTER pAd,
	OUT PVOID		 *AllocVa)
{
	PMLME_MEMORY_STRUCT 			pMlmeMemoryStruct = NULL;

	DBGPRINT(RT_DEBUG_INFO,"==> MlmeAllocateMemory\n");


	if (pAd->Mlme.MemHandler.pUnUseHead == NULL)
	{ //There are no available memory for caller use
		DBGPRINT(RT_DEBUG_ERROR,"MlmeAllocateMemory, failed!! (There are no available memory in list)\n");
		DBGPRINT(RT_DEBUG_ERROR,"<== MlmeAllocateMemory\n");
		return (NDIS_STATUS_RESOURCES);
	}
	pMlmeMemoryStruct = pAd->Mlme.MemHandler.pUnUseHead;
	//Unused memory point to next available
	pAd->Mlme.MemHandler.pUnUseHead = pAd->Mlme.MemHandler.pUnUseHead->Next;
	if (pAd->Mlme.MemHandler.pUnUseHead == NULL)
		pAd->Mlme.MemHandler.pUnUseTail = NULL;
	pAd->Mlme.MemHandler.UnUseCount--;

	*AllocVa = pMlmeMemoryStruct->AllocVa;			//Saved porint to Pointer the base virtual address of the nonpaged memory for caller use.

	pMlmeMemoryStruct->Next = NULL;
	//Append the unused memory link list to the in-used link list tail
	if (pAd->Mlme.MemHandler.pInUseHead == NULL)
	{//no head, so current Item assign to In-use Head.
		pAd->Mlme.MemHandler.pInUseHead = pMlmeMemoryStruct;
		pAd->Mlme.MemHandler.pInUseTail = pMlmeMemoryStruct;
	}
	else
	{
		pAd->Mlme.MemHandler.pInUseTail->Next = pMlmeMemoryStruct;
		pAd->Mlme.MemHandler.pInUseTail = pAd->Mlme.MemHandler.pInUseTail->Next;
	}
	pAd->Mlme.MemHandler.InUseCount++;



	DBGPRINT(RT_DEBUG_INFO, "MlmeAllocateMemory [pMlmeMemoryStruct=%p][VA=%p]\n", pMlmeMemoryStruct, pMlmeMemoryStruct->AllocVa);
	DBGPRINT(RT_DEBUG_INFO, "<== MlmeAllocateMemory[IN:%d][UN:%d]\n",
				pAd->Mlme.MemHandler.InUseCount, pAd->Mlme.MemHandler.UnUseCount);
	return (NDIS_STATUS_SUCCESS);
}

/*
	========================================================================

	Routine Description:
		Mlme free the in-used nonpaged memory,
		move it to the unused memory link list

	Arguments:
		pAd 				Pointer to our adapter
		AllocVa 			Pointer to the base virtual address for free

	Return Value:
		None

	Note:

	========================================================================
*/
VOID	MlmeFreeMemory(
	IN PRTMP_ADAPTER pAd,
	IN PVOID		 AllocVa)
{
	PMLME_MEMORY_STRUCT 			pPrevious;
	PMLME_MEMORY_STRUCT 			pMlmeMemoryStruct;
	BOOLEAN 						bIsFound;

	if (AllocVa == NULL)
		return;

	bIsFound = FALSE;
	pPrevious = NULL;
	pMlmeMemoryStruct = pAd->Mlme.MemHandler.pInUseHead;
	while (pMlmeMemoryStruct)
	{
//		DBGPRINT(RT_DEBUG_TRACE,"pMlmeMemoryStruct->AllocVa=%x\n",(int)pMlmeMemoryStruct->AllocVa);
		if (pMlmeMemoryStruct->AllocVa == AllocVa)
		{
			//Found virtual address in the in-used link list
			//Remove it from the memory in-used link list, and move it to the unused link list
			if (pPrevious == NULL)
				pAd->Mlme.MemHandler.pInUseHead = pMlmeMemoryStruct->Next;
			else
				pPrevious->Next = pMlmeMemoryStruct->Next;
			pAd->Mlme.MemHandler.InUseCount--;

			//append it to the tail of unused list
			pMlmeMemoryStruct->Next = NULL;
			if ((pAd->Mlme.MemHandler.pUnUseHead == NULL))
			{ //No head, add it as head
				pAd->Mlme.MemHandler.pUnUseHead = pMlmeMemoryStruct;
				pAd->Mlme.MemHandler.pUnUseTail = pMlmeMemoryStruct;
			}
			else
			{
				//Append it to the tail in pAd->Mlme.MemHandler.pUnUseTail
				pAd->Mlme.MemHandler.pUnUseTail->Next = pMlmeMemoryStruct;
				pAd->Mlme.MemHandler.pUnUseTail = pAd->Mlme.MemHandler.pUnUseTail->Next;
			}
			pAd->Mlme.MemHandler.UnUseCount++;

			bIsFound = TRUE;
			break;
		}
		else
		{
			pPrevious = pMlmeMemoryStruct;
			pMlmeMemoryStruct = pMlmeMemoryStruct->Next;
		}
	}
 }

/*
	========================================================================

	Routine Description:
		Allocates resident (nonpaged) system-space memory for MLME send frames

	Arguments:
		pAd 				Pointer to our adapter
		Number				Total nonpaged memory for use
		Size				Each nonpaged memory size

	Return Value:
		NDIS_STATUS_SUCCESS
		NDIS_STATUS_RESOURCES

	Note:

	========================================================================
*/
NDIS_STATUS MlmeInitMemoryHandler(
	IN PRTMP_ADAPTER pAd,
	IN UINT  Number,
	IN UINT  Size)
{
	PMLME_MEMORY_STRUCT 		Current = NULL;
	NDIS_STATUS 				Status = NDIS_STATUS_SUCCESS;
	UINT						i;

	DBGPRINT(RT_DEBUG_TRACE,"==> MlmeInitMemoryHandler\n");
	pAd->Mlme.MemHandler.MemoryCount = 0;
	pAd->Mlme.MemHandler.pInUseHead = NULL;
	pAd->Mlme.MemHandler.pInUseTail = NULL;
	pAd->Mlme.MemHandler.pUnUseHead = NULL;
	pAd->Mlme.MemHandler.pUnUseTail = NULL;
	pAd->Mlme.MemHandler.MemRunning = FALSE;

	//initial the memory free-pending array all to NULL;
	for (i = 0; i < MAX_MLME_HANDLER_MEMORY; i++)
		pAd->Mlme.MemHandler.MemFreePending[i] = NULL;

	//
	// Available nonpaged memory counts MAX_MLME_HANDLER_MEMORY
	//
	if (Number > MAX_MLME_HANDLER_MEMORY)
		Number = MAX_MLME_HANDLER_MEMORY;

	for (i = 0; i < Number; i++)
	{
		//Allocate a nonpaged memory for link list use.
		Current= (PMLME_MEMORY_STRUCT) kmalloc(sizeof(MLME_MEMORY_STRUCT), MEM_ALLOC_FLAG);
		if (!Current)
		{
			DBGPRINT(RT_DEBUG_ERROR, "Not enough memory\n");
			Status = NDIS_STATUS_RESOURCES;
			break;
		}

		Current->AllocVa= (VOID *) kmalloc(Size, MEM_ALLOC_FLAG);
		if (!Current->AllocVa)
		{
			DBGPRINT(RT_DEBUG_ERROR, "Not enough memory\n");
			if( Current != NULL){
				kfree((VOID *)Current);
			}
			Status = NDIS_STATUS_RESOURCES;
			break;
		}

		memset(Current->AllocVa, 0, Size);

		pAd->Mlme.MemHandler.MemoryCount++;

		//build up the link list
		if (pAd->Mlme.MemHandler.pUnUseHead != NULL)
		{
			Current->Next = pAd->Mlme.MemHandler.pUnUseHead;
			pAd->Mlme.MemHandler.pUnUseHead = Current;
		}
		else
		{
			Current->Next = NULL;
			pAd->Mlme.MemHandler.pUnUseHead = Current;
		}

		if (pAd->Mlme.MemHandler.pUnUseTail == NULL)
			pAd->Mlme.MemHandler.pUnUseTail = Current;

	}
	if (pAd->Mlme.MemHandler.MemoryCount < Number)
	{
		Status = NDIS_STATUS_RESOURCES;
		DBGPRINT(RT_DEBUG_TRACE,
				"MlmeInitMemoryHandler failed [Require=%d, available=%d]\n",
				Number, pAd->Mlme.MemHandler.MemoryCount);
		MlmeFreeMemoryHandler(pAd);
	}
	else {
		pAd->Mlme.MemHandler.InUseCount = 0;
		pAd->Mlme.MemHandler.UnUseCount = Number;
		pAd->Mlme.MemHandler.PendingCount = 0;
	}
	DBGPRINT(RT_DEBUG_TRACE, "<== MlmeInitMemoryHandler Status=%d\n",Status);

	return (Status);
}

/*
	========================================================================

	Routine Description:
		Free Mlme memory handler (link list, nonpaged memory, spin lock)

	Arguments:
		pAd 				Pointer to our adapter

	Return Value:
		None
	========================================================================
*/
VOID MlmeFreeMemoryHandler(
	IN PRTMP_ADAPTER pAd)
{
	PMLME_MEMORY_STRUCT 	 pMlmeMemoryStruct = NULL;

	DBGPRINT(RT_DEBUG_TRACE, "--->MlmeFreeMemoryHandler\n");

	//Free nonpaged memory, free it in the *In-used* link list.
	while (pAd->Mlme.MemHandler.pInUseHead != NULL)
	{
		pMlmeMemoryStruct = pAd->Mlme.MemHandler.pInUseHead;
		pAd->Mlme.MemHandler.pInUseHead = pAd->Mlme.MemHandler.pInUseHead->Next;
		//Free the virtual address in AllocVa which size is MAX_LEN_OF_MLME_BUFFER
		if(pMlmeMemoryStruct->AllocVa != NULL){
			kfree(pMlmeMemoryStruct->AllocVa);
		}
		//Free the link list item self
		if(pMlmeMemoryStruct != NULL){
			kfree(pMlmeMemoryStruct);
		}
	}

	//Free nonpaged memory, free it in the *Unused* link list.
	while (pAd->Mlme.MemHandler.pUnUseHead != NULL)
	{
		pMlmeMemoryStruct = pAd->Mlme.MemHandler.pUnUseHead;
		pAd->Mlme.MemHandler.pUnUseHead = pAd->Mlme.MemHandler.pUnUseHead->Next;
		//Free the virtual address in AllocVa which size is MAX_LEN_OF_MLME_BUFFER
		if(pMlmeMemoryStruct->AllocVa != NULL){
			kfree(pMlmeMemoryStruct->AllocVa);
		}

		//Free the link list item self
		if(pMlmeMemoryStruct != NULL){
			kfree(pMlmeMemoryStruct);
		}
	}

	DBGPRINT(RT_DEBUG_TRACE, "<---MlmeFreeMemoryHandler\n");
}

/*
	========================================================================

	Routine Description:
		Radar detection routine

	Arguments:
		pAd     Pointer to our adapter

	Return Value:

	========================================================================
*/
VOID RadarDetectionStart(
	IN PRTMP_ADAPTER	pAd)
{
	DBGPRINT(RT_DEBUG_TRACE, "RadarDetectionStart--->\n");

	RT73WriteTXRXCSR0(pAd, TRUE, FALSE);        // Disable Rx

	// Set all relative BBP register to enter into radar detection mode
	RTUSBWriteBBPRegister(pAd, BBP_R82, 0x20);
	RTUSBWriteBBPRegister(pAd, BBP_R83, 0);
	RTUSBWriteBBPRegister(pAd, BBP_R84, 0x40);

	RTUSBReadBBPRegister(pAd, BBP_R18, &pAd->PortCfg.RadarDetect.BBPR18);
	RTUSBWriteBBPRegister(pAd, BBP_R18, 0xFF);

	RTUSBReadBBPRegister(pAd, BBP_R21, &pAd->PortCfg.RadarDetect.BBPR21);
	RTUSBWriteBBPRegister(pAd, BBP_R21, 0x3F);
	RTUSBReadBBPRegister(pAd, BBP_R22, &pAd->PortCfg.RadarDetect.BBPR22);
	RTUSBWriteBBPRegister(pAd, BBP_R22, 0x3F);

	RTUSBReadBBPRegister(pAd, BBP_R16, &pAd->PortCfg.RadarDetect.BBPR16);
	RTUSBWriteBBPRegister(pAd, BBP_R16, 0xBD);
	RTUSBReadBBPRegister(pAd, BBP_R17, &pAd->PortCfg.RadarDetect.BBPR17);
	if (pAd->NicConfig2.field.ExternalLNA)
	{
		RTUSBWriteBBPRegister(pAd, 17, 0x44); // if external LNA enable, this value need to be offset 0x10
	}
	else
	{
		RTUSBWriteBBPRegister(pAd, 17, 0x34);
	}

	RTUSBReadBBPRegister(pAd, BBP_R64, &pAd->PortCfg.RadarDetect.BBPR64);
	RTUSBWriteBBPRegister(pAd, BBP_R64, 0x21);

	RT73WriteTXRXCSR0(pAd, FALSE, FALSE);       // enable RX of MAC block
}

/*
	========================================================================

	Routine Description:
		Radar detection routine

	Arguments:
		pAd     Pointer to our adapter

	Return Value:
		TRUE    Found radar signal
		FALSE   Not found radar signal

	========================================================================
*/
BOOLEAN RadarDetectionStop(
	IN PRTMP_ADAPTER	pAd)
{
	UCHAR	R66;

	// Need to read the result of detection first
	// If restore all registers first, then the result will be reset.
	RTUSBReadBBPRegister(pAd, BBP_R66, &R66);

	// Restore all relative BBP register to exit radar detection mode
	RTUSBWriteBBPRegister(pAd, BBP_R16, pAd->PortCfg.RadarDetect.BBPR16);
	RTUSBWriteBBPRegister(pAd, BBP_R17, pAd->PortCfg.RadarDetect.BBPR17);
	RTUSBWriteBBPRegister(pAd, BBP_R18, pAd->PortCfg.RadarDetect.BBPR18);
	RTUSBWriteBBPRegister(pAd, BBP_R21, pAd->PortCfg.RadarDetect.BBPR21);
	RTUSBWriteBBPRegister(pAd, BBP_R22, pAd->PortCfg.RadarDetect.BBPR22);
	RTUSBWriteBBPRegister(pAd, BBP_R64, pAd->PortCfg.RadarDetect.BBPR64);

	if (R66 == 1)
		return TRUE;
	else
		return FALSE;
}

/*
	========================================================================

	Routine Description:
		Radar channel check routine

	Arguments:
		pAd     Pointer to our adapter

	Return Value:
		TRUE    need to do radar detect
		FALSE   need not to do radar detect

	========================================================================
*/
BOOLEAN RadarChannelCheck(
	IN PRTMP_ADAPTER	pAd,
	IN UCHAR			Ch)
{
	INT		i;
	UCHAR	Channel[15]={52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140};

	for (i=0; i<15; i++)
	{
		if (Ch == Channel[i])
		{
			break;
		}
	}

	if (i != 15)
		return TRUE;
	else
		return FALSE;
}

