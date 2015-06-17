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
 *	Module Name:	mlme.h
 *
 *	Abstract:
 *
 *	Revision History:
 *	Who		When		What
 *	--------	----------	-----------------------------
 *	John Chang	2003-08-28	Created
 *	John Chang	2004-09-06	modified for RT2600
 *
 ***************************************************************************/

#ifndef __MLME_H__
#define __MLME_H__

#include "oid.h"

// maximum supported capability information -
// ESS, IBSS, Privacy, Short Preamble, Spectrum mgmt, Short Slot
#define SUPPORTED_CAPABILITY_INFO   0x0533

#define END_OF_ARGS                 -1
#define LFSR_MASK                   0x80000057
#define MLME_TASK_EXEC_INTV         1000         // 1 sec
//#define TBTT_PRELOAD_TIME         384          // usec. LomgPreamble + 24-byte at 1Mbps

#define BEACON_LOST_TIME            (4*HZ)      // 2048 msec = 2 sec

#define AUTH_TIMEOUT                300         // unit: msec
#define ASSOC_TIMEOUT               300         // unit: msec
#define JOIN_TIMEOUT                2000        // unit: msec
#define MIN_CHANNEL_TIME            110         // unit: msec, for dual band scan
#define MAX_CHANNEL_TIME            140         // unit: msec, for single band scan
//#define	ACTIVE_SCAN_TIME		    30			// Active scan waiting for probe response time
#define	FAST_ACTIVE_SCAN_TIME	    30 		    // Active scan waiting for probe response time
#define CW_MIN_IN_BITS              4         // actual CwMin = 2^CW_MIN_IN_BITS - 1
#define CW_MAX_IN_BITS              10        // actual CwMax = 2^CW_MAX_IN_BITS - 1

// Note: RSSI_TO_DBM_OFFSET has been changed to variable for new RF (2004-0720).
// SHould not refer to this constant anymore
#define RSSI_TO_DBM_OFFSET          120 // for RT2530 RSSI-115 = dBm
#define RSSI_FOR_MID_TX_POWER       -55  // -55 db is considered mid-distance
#define RSSI_FOR_LOW_TX_POWER       -45  // -45 db is considered very short distance and
                                        // eligible to use a lower TX power
#define RSSI_FOR_LOWEST_TX_POWER    -30
//#define MID_TX_POWER_DELTA          0   // 0 db from full TX power upon mid-distance to AP
#define LOW_TX_POWER_DELTA          6    // -3 db from full TX power upon very short distance. 1 grade is 0.5 db
#define LOWEST_TX_POWER_DELTA       16   // -8 db from full TX power upon shortest distance. 1 grade is 0.5 db

#define RSSI_TRIGGERED_UPON_BELOW_THRESHOLD     0
#define RSSI_TRIGGERED_UPON_EXCCEED_THRESHOLD   1
#define RSSI_THRESHOLD_FOR_ROAMING              25
#define RSSI_DELTA                              5

// Channel Quality Indication
#define CQI_IS_GOOD(cqi)            ((cqi) >= 50)
//#define CQI_IS_FAIR(cqi)          (((cqi) >= 20) && ((cqi) < 50))
#define CQI_IS_POOR(cqi)            (cqi < 50)  //(((cqi) >= 5) && ((cqi) < 20))
#define CQI_IS_BAD(cqi)             (cqi < 5)
#define CQI_IS_DEAD(cqi)            (cqi == 0)   //((cqi) < 5)

// weighting factor to calculate Channel quality, total should be 100%
#define RSSI_WEIGHTING                   50
#define TX_WEIGHTING                     30
#define RX_WEIGHTING                     20

#define MAX_LEN_OF_BSS_TABLE             64
#define BSS_NOT_FOUND                    0xFFFFFFFF

#define SCAN_PASSIVE                     18
#define SCAN_ACTIVE                      19
#define FAST_SCAN_ACTIVE                 24		// scan with probe request, and wait beacon and probe response
#define MAX_LEN_OF_MLME_QUEUE            20


// bit definition of the 2-byte pBEACON->Capability field
#define CAP_IS_ESS_ON(x)                 (((x) & 0x0001) != 0)
#define CAP_IS_IBSS_ON(x)                (((x) & 0x0002) != 0)
#define CAP_IS_CF_POLLABLE_ON(x)         (((x) & 0x0004) != 0)
#define CAP_IS_CF_POLL_REQ_ON(x)         (((x) & 0x0008) != 0)
#define CAP_IS_PRIVACY_ON(x)             (((x) & 0x0010) != 0)
#define CAP_IS_SHORT_PREAMBLE_ON(x)      (((x) & 0x0020) != 0)
#define CAP_IS_PBCC_ON(x)                (((x) & 0x0040) != 0)
#define CAP_IS_AGILITY_ON(x)             (((x) & 0x0080) != 0)
#define CAP_IS_SPECTRUM_MGMT(x)          (((x) & 0x0100) != 0)  // 802.11e d9
#define CAP_IS_QOS(x)                    (((x) & 0x0200) != 0)  // 802.11e d9
#define CAP_IS_SHORT_SLOT(x)             (((x) & 0x0400) != 0)
#define CAP_IS_APSD(x)                   (((x) & 0x0800) != 0)  // 802.11e d9
#define CAP_IS_IMMED_BA(x)               (((x) & 0x1000) != 0)  // 802.11e d9
#define CAP_IS_DSSS_OFDM(x)              (((x) & 0x2000) != 0)
#define CAP_IS_DELAY_BA(x)               (((x) & 0x4000) != 0)  // 802.11e d9

#define CAP_GENERATE(ess,ibss,priv,s_pre,s_slot)  (((ess) ? 0x0001 : 0x0000) | ((ibss) ? 0x0002 : 0x0000) | ((priv) ? 0x0010 : 0x0000) | ((s_pre) ? 0x0020 : 0x0000) | ((s_slot) ? 0x0400 : 0x0000))

#define STA_QOS_CAPABILITY               0 // 1-byte. see 802.11e d9.0 for bit definition

#define ERP_IS_NON_ERP_PRESENT(x)        (((x) & 0x01) != 0)    // 802.11g
#define ERP_IS_USE_PROTECTION(x)         (((x) & 0x02) != 0)    // 802.11g
#define ERP_IS_USE_BARKER_PREAMBLE(x)    (((x) & 0x04) != 0)    // 802.11g

#define DRS_TX_QUALITY_WORST_BOUND       3
#define DRS_PENALTY                      8


//
// 802.11 frame formats
//

// 2-byte QOS CONTROL field
typedef struct PACKED {
#ifdef BIG_ENDIAN
    USHORT      Txop_QueueSize:8;
    USHORT      Rsv:1;
    USHORT      AckPolicy:2;
    USHORT      EOSP:1;
    USHORT      TID:4;
#else
    USHORT      TID:4;
    USHORT      EOSP:1;
    USHORT      AckPolicy:2;
    USHORT      Rsv:1;
    USHORT      Txop_QueueSize:8;
#endif
} QOS_CONTROL, *PQOS_CONTROL;

// 2-byte Frame control field
typedef	struct	PACKED {
#ifdef BIG_ENDIAN
    USHORT		Order:1;
    USHORT		Wep:1;
    USHORT		MoreData:1;
    USHORT		PwrMgmt:1;
    USHORT		Retry:1;
    USHORT		MoreFrag:1;
    USHORT		FrDs:1;
    USHORT		ToDs:1;
    USHORT		SubType:4;
    USHORT		Type:2;
    USHORT		Ver:2;
#else
	USHORT		Ver:2;				// Protocol version
	USHORT		Type:2;				// MSDU type
	USHORT		SubType:4;			// MSDU subtype
	USHORT		ToDs:1;				// To DS indication
	USHORT		FrDs:1;				// From DS indication
	USHORT		MoreFrag:1;			// More fragment bit
	USHORT		Retry:1;			// Retry status bit
	USHORT		PwrMgmt:1;			// Power management bit
	USHORT		MoreData:1;			// More data bit
	USHORT		Wep:1;				// Wep data
	USHORT		Order:1;			// Strict order expected
#endif
}	FRAME_CONTROL, *PFRAME_CONTROL;

typedef	struct	PACKED _HEADER_802_11	{
    FRAME_CONTROL   FC;
    USHORT          Duration;
    UCHAR           Addr1[MAC_ADDR_LEN];
    UCHAR           Addr2[MAC_ADDR_LEN];
	UCHAR			Addr3[MAC_ADDR_LEN];
#ifdef BIG_ENDIAN
    USHORT    		Sequence:12;
    USHORT    		Frag:4;
#else
	USHORT			Frag:4;
	USHORT			Sequence:12;
#endif
}	HEADER_802_11, *PHEADER_802_11;

typedef struct PACKED _FRAME_802_11 {
    HEADER_802_11   Hdr;
    UCHAR			Octet[0];
}   FRAME_802_11, *PFRAME_802_11;

typedef struct _PSPOLL_FRAME {
    FRAME_CONTROL   FC;
    USHORT          Aid;
    UCHAR           Bssid[MAC_ADDR_LEN];
    UCHAR           Ta[MAC_ADDR_LEN];
}   PSPOLL_FRAME, *PPSPOLL_FRAME;

typedef	struct	PACKED _RTS_FRAME	{
    FRAME_CONTROL   FC;
    USHORT          Duration;
    UCHAR           Addr1[MAC_ADDR_LEN];
    UCHAR           Addr2[MAC_ADDR_LEN];
}	RTS_FRAME, *PRTS_FRAME;

//
// Contention-free parameter (without ID and Length)
//
typedef struct PACKED {
    BOOLEAN     bValid;         // 1: variable contains valid value
    UCHAR       CfpCount;
    UCHAR       CfpPeriod;
    USHORT      CfpMaxDuration;
    USHORT      CfpDurRemaining;
} CF_PARM, *PCF_PARM;

typedef	struct	_CIPHER_SUITE	{
	NDIS_802_11_ENCRYPTION_STATUS	PairCipher;		// Unicast cipher 1, this one has more secured cipher suite
	NDIS_802_11_ENCRYPTION_STATUS	PairCipherAux;	// Unicast cipher 2 if AP announce two unicast cipher suite
	NDIS_802_11_ENCRYPTION_STATUS	GroupCipher;	// Group cipher
	USHORT							RsnCapability;	// RSN capability from beacon
	BOOLEAN							bMixMode;		// Indicate Pair & Group cipher might be different
}	CIPHER_SUITE, *PCIPHER_SUITE;

// EDCA configuration from AP's BEACON/ProbeRsp
typedef struct PACKED {
    BOOLEAN     bValid;         // 1: variable contains valid value
    BOOLEAN     bQAck;
    BOOLEAN     bQueueRequest;
    BOOLEAN     bTxopRequest;
//  BOOLEAN     bMoreDataAck;
    UCHAR       EdcaUpdateCount;
    UCHAR       Aifsn[4];       // 0:AC_BK, 1:AC_BE, 2:AC_VI, 3:AC_VO
    UCHAR       Cwmin[4];
    UCHAR       Cwmax[4];
    USHORT      Txop[4];      // in unit of 32-us
    BOOLEAN     bACM[4];      // 1: Admission Control of AC_BK is mandattory
} EDCA_PARM, *PEDCA_PARM;

// QBSS LOAD information from QAP's BEACON/ProbeRsp
typedef struct PACKED {
    BOOLEAN     bValid;                     // 1: variable contains valid value
    USHORT      StaNum;
    UCHAR       ChannelUtilization;
    USHORT      RemainingAdmissionControl;  // in unit of 32-us
} QBSS_LOAD_PARM, *PQBSS_LOAD_PARM;

// QOS Capability reported in QAP's BEACON/ProbeRsp
// QOS Capability sent out in QSTA's AssociateReq/ReAssociateReq
typedef struct PACKED {
    BOOLEAN     bValid;                     // 1: variable contains valid value
    BOOLEAN     bQAck;
    BOOLEAN     bQueueRequest;
    BOOLEAN     bTxopRequest;
//  BOOLEAN     bMoreDataAck;
    UCHAR       EdcaUpdateCount;
} QOS_CAPABILITY_PARM, *PQOS_CAPABILITY_PARM;

typedef struct {
    UCHAR   Bssid[MAC_ADDR_LEN];
    UCHAR   Channel;
    UCHAR   BssType;
    USHORT  AtimWin;
    USHORT  BeaconPeriod;

    UCHAR   SupRate[MAX_LEN_OF_SUPPORTED_RATES];
    UCHAR   SupRateLen;
    UCHAR   ExtRate[MAX_LEN_OF_SUPPORTED_RATES];
    UCHAR   ExtRateLen;
    UCHAR   Rssi;
    UCHAR   Privacy;			// Indicate security function ON/OFF. Don't mess up with auth mode.
	UCHAR	Hidden;

    USHORT  DtimPeriod;
    USHORT  CapabilityInfo;

    USHORT  CfpCount;
    USHORT  CfpPeriod;
    USHORT  CfpMaxDuration;
    USHORT  CfpDurRemaining;
    UCHAR   SsidLen;
    CHAR    Ssid[MAX_LEN_OF_SSID];

    unsigned long   LastBeaconRxTime; // OS's timestamp

    // New for WPA2
	CIPHER_SUITE					WPA;			// AP announced WPA cipher suite
	CIPHER_SUITE					WPA2;			// AP announced WPA2 cipher suite

	// New for microsoft WPA support
	NDIS_802_11_FIXED_IEs	FixIEs;
	NDIS_802_11_AUTHENTICATION_MODE	AuthModeAux;	// Addition mode for WPA2 / WPA capable AP
	NDIS_802_11_AUTHENTICATION_MODE	AuthMode;
	UCHAR                           AuthBitMode;
	NDIS_802_11_WEP_STATUS	WepStatus;				// Unicast Encryption Algorithm extract from VAR_IE
	UCHAR					VarIELen;				// Length of next VIE include EID & Length
	UCHAR					VarIEs[MAX_VIE_LEN];

	// CCX Ckip information
    UCHAR   CkipFlag;
	// CCX 2 TSF
	//UCHAR	PTSF[4];		// Parent TSF
	//UCHAR	TTSF[8];		// Target TSF

    // 802.11e d9, and WMM
	EDCA_PARM           EdcaParm;
	QOS_CAPABILITY_PARM QosCapability;
	QBSS_LOAD_PARM      QbssLoad;
} BSS_ENTRY, *PBSS_ENTRY;

typedef struct {
    UCHAR           BssNr;
    UCHAR           BssOverlapNr;
    BSS_ENTRY       BssEntry[MAX_LEN_OF_BSS_TABLE];
} BSS_TABLE, *PBSS_TABLE;

typedef struct _MLME_QUEUE_ELEM {
    UCHAR             Msg[MAX_LEN_OF_MLME_BUFFER];
    ULONG             Machine;
    ULONG             MsgType;
    ULONG             MsgLen;
    LARGE_INTEGER     TimeStamp;
    UCHAR             Rssi;
    UCHAR             Signal;
    UCHAR             Channel;
    BOOLEAN           Occupied;
    BOOLEAN           bReqIsFromNdis;
} MLME_QUEUE_ELEM, *PMLME_QUEUE_ELEM;

typedef struct _MLME_QUEUE {
    ULONG             Num;
    ULONG             Head;
    ULONG             Tail;
    spinlock_t        Lock;
    MLME_QUEUE_ELEM   Entry[MAX_LEN_OF_MLME_QUEUE];
} MLME_QUEUE, *PMLME_QUEUE;

typedef VOID (*STATE_MACHINE_FUNC)(VOID *Adaptor, MLME_QUEUE_ELEM *Elem);

typedef struct _STATE_MACHINE {
    ULONG                           Base;
    ULONG                           NrState;
    ULONG                           NrMsg;
    ULONG                           CurrState;
    STATE_MACHINE_FUNC             *TransFunc;
} STATE_MACHINE, *PSTATE_MACHINE;

// MLME AUX data structure that hold temporarliy settings during a connection attempt.
// Once this attemp succeeds, all settings will be copy to pAd->ActiveCfg.
// A connection attempt (user set OID, roaming, CCX fast roaming,..) consists of
// several steps (JOIN, AUTH, ASSOC or REASSOC) and may fail at any step. We purposely
// separate this under-trial settings away from pAd->ActiveCfg so that once
// this new attempt failed, driver can auto-recover back to the active settings.
typedef struct _MLME_AUX {
    UCHAR               BssType;
    UCHAR               Ssid[MAX_LEN_OF_SSID];
    UCHAR               SsidLen;
    UCHAR               Bssid[MAC_ADDR_LEN];
	UCHAR				AutoReconnectSsid[MAX_LEN_OF_SSID];
	UCHAR				AutoReconnectSsidLen;
    USHORT              Alg;
    UCHAR               ScanType;
    UCHAR               Channel;
    USHORT              Aid;
    USHORT              CapabilityInfo;
    USHORT              BeaconPeriod;
    USHORT              CfpMaxDuration;
    USHORT              CfpPeriod;
    USHORT              AtimWin;

	// Copy supported rate from desired AP's beacon. We are trying to match
	// AP's supported and extended rate settings.
	UCHAR		        SupRate[MAX_LEN_OF_SUPPORTED_RATES];
	UCHAR		        ExtRate[MAX_LEN_OF_SUPPORTED_RATES];
	UCHAR		        SupRateLen;
	UCHAR		        ExtRateLen;

    // new for QOS
    QOS_CAPABILITY_PARM APQosCapability;    // QOS capability of the current associated AP
    EDCA_PARM           APEdcaParm;         // EDCA parameters of the current associated AP
    QBSS_LOAD_PARM      APQbssLoad;         // QBSS load of the current associated AP

    // new to keep Ralink specific feature
    ULONG               APRalinkIe;

    BSS_TABLE           SsidBssTab;     // AP list for the same SSID
    BSS_TABLE           RoamTab;        // AP list eligible for roaming
    ULONG               BssIdx;
    ULONG               RoamIdx;

	BOOLEAN				CurrReqIsFromNdis;  // TRUE - then we should call NdisMSetInformationComplete()
                                            // FALSE - req is from driver itself.
                                            // no NdisMSetInformationComplete() is required

	RALINK_TIMER_STRUCT BeaconTimer, ScanTimer;
	RALINK_TIMER_STRUCT AuthTimer;
	RALINK_TIMER_STRUCT AssocTimer, ReassocTimer;
} MLME_AUX, *PMLME_AUX;

// assoc struct is equal to reassoc
typedef struct _MLME_ASSOC_REQ_STRUCT{
    UCHAR     Addr[MAC_ADDR_LEN];
    USHORT    CapabilityInfo;
    USHORT    ListenIntv;
    ULONG     Timeout;
} MLME_ASSOC_REQ_STRUCT, *PMLME_ASSOC_REQ_STRUCT, MLME_REASSOC_REQ_STRUCT, *PMLME_REASSOC_REQ_STRUCT;

typedef struct _MLME_DISASSOC_REQ_STRUCT{
    UCHAR     Addr[MAC_ADDR_LEN];
    USHORT    Reason;
} MLME_DISASSOC_REQ_STRUCT, *PMLME_DISASSOC_REQ_STRUCT;

typedef struct _MLME_AUTH_REQ_STRUCT {
    UCHAR        Addr[MAC_ADDR_LEN];
    USHORT       Alg;
    ULONG        Timeout;
} MLME_AUTH_REQ_STRUCT, *PMLME_AUTH_REQ_STRUCT;

typedef struct _MLME_DEAUTH_REQ_STRUCT {
    UCHAR        Addr[MAC_ADDR_LEN];
    USHORT       Reason;
} MLME_DEAUTH_REQ_STRUCT, *PMLME_DEAUTH_REQ_STRUCT;

typedef struct {
    ULONG      BssIdx;
} MLME_JOIN_REQ_STRUCT;

typedef struct _MLME_SCAN_REQ_STRUCT {
    UCHAR      Bssid[MAC_ADDR_LEN];
    UCHAR      BssType;
    UCHAR      ScanType;
    UCHAR      SsidLen;
    CHAR       Ssid[MAX_LEN_OF_SSID];
} MLME_SCAN_REQ_STRUCT, *PMLME_SCAN_REQ_STRUCT;

typedef struct _MLME_START_REQ_STRUCT {
    CHAR        Ssid[MAX_LEN_OF_SSID];
    UCHAR       SsidLen;
} MLME_START_REQ_STRUCT, *PMLME_START_REQ_STRUCT;

typedef struct {
    UCHAR   Eid;
    UCHAR   Len;
    CHAR   Octet[1];
} EID_STRUCT,*PEID_STRUCT;

#endif  // __MLME_H__

