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
 *	Module Name:	rtmp_def.h
 *
 *	Abstract: Miniport related definition header
 *
 *	Revision History:
 *	Who		When		What
 *	--------	----------	-----------------------------
 *	Paul Lin	08-01-2002	created
 *	John Chang	08-05-2003	add definition for 11g & other drafts
 *	idamlaj	04-10-2006	Add extra devices
 *
 ***************************************************************************/

#ifndef __RTMP_DEF_H__
#define __RTMP_DEF_H__

//
//	Debug information verbosity: lower values indicate higher urgency
//
#define RT_DEBUG_OFF		0
#define RT_DEBUG_ERROR		1
#define RT_DEBUG_WARN		2
#define RT_DEBUG_TRACE		4
#define RT_DEBUG_INFO		8
#define RT_DEBUG_LOUD		16

#ifdef BIG_ENDIAN
#define DIR_READ					0
#define DIR_WRITE					1
#define TYPE_TXD					0
#define TYPE_RXD					1
#endif

//WEP
#define WEP_SMALL_KEY_LEN	(40/8)
#define WEP_LARGE_KEY_LEN	(104/8)

//
// Entry number for each DMA descriptor ring
//
#define TX_RING_SIZE			32
#define PRIO_RING_SIZE			16
#define RX_RING_SIZE			32
#define	BEACON_RING_SIZE		2
#define MGMT_RING_SIZE			PRIO_RING_SIZE
#define PRIO_BUFFER_SIZE		1024	// 2048
#define BUFFER_SIZE				2400	//2048
#define	MAX_FRAME_SIZE			2346	// Maximum 802.11 frame size
#define ALLOC_RX_PACKET_POOL	(RX_RING_SIZE)
#define ALLOC_RX_BUFFER_POOL	(ALLOC_RX_PACKET_POOL)
#define	TX_RING					0xa
#define	ATIM_RING				0xb
#define	PRIO_RING				0xc
#define	RX_RING					0xd
#define	BEACON_RING				0xe
#define	NULL_RING				0xf
#define	MAX_TX_PROCESS			2
#define	MAX_RX_PROCESS			4
#define	MAX_CLIENT				2

#define LOCAL_TXBUF_SIZE		2048
#define TXD_SIZE				24
#define RXD_SIZE				24
#define TX_DMA_1ST_BUFFER_SIZE	64	  // only the 1st physical buffer is pre-allocated
#define MGMT_DMA_BUFFER_SIZE	2048
#define RX_DMA_BUFFER_SIZE      4096
#define MAX_AGGREGATION_SIZE	4096
#define MAX_DMA_DONE_PROCESS	TX_RING_SIZE
#define MAX_TX_DONE_PROCESS 	8
#define MAX_NUM_OF_TUPLE_CACHE	2
#define MAX_MCAST_LIST_SIZE 	32


//
//	RTMP_ADAPTER flags
//
#define	fRTMP_ADAPTER_TEST_MODE				0x00000001
#define fRTMP_ADAPTER_MLME_RESET_IN_PROGRESS	 0x00000002
#define fRTMP_ADAPTER_HARDWARE_ERROR		0x00000004
#define fRTMP_ADAPTER_SEND_PACKET_ERROR 	0x00000010
#define fRTMP_ADAPTER_RECEIVE_PACKET_ERROR	0x00000020
#define fRTMP_ADAPTER_HALT_IN_PROGRESS		0x00000040
#define fRTMP_ADAPTER_RESET_IN_PROGRESS 	0x00000080
#define fRTMP_ADAPTER_NIC_NOT_EXIST 		0x00000100
#define fRTMP_ADAPTER_TX_RING_ALLOCATED 	0x00000200
#define fRTMP_ADAPTER_ATIM_RING_ALLOCATED	0x00000400
#define fRTMP_ADAPTER_PRIO_RING_ALLOCATED	0x00000800
#define fRTMP_ADAPTER_RX_RING_ALLOCATED 	0x00001000
#define fRTMP_ADAPTER_BSS_JOIN_IN_PROGRESS	0x00002000
#define fRTMP_ADAPTER_BSS_SCAN_IN_PROGRESS	0x00004000
#define	fRTMP_ADAPTER_REASSOC_IN_PROGRESS	0x00008000
#define	fRTMP_ADAPTER_MEDIA_STATE_PENDING	0x00010000
#define	fRTMP_ADAPTER_RADIO_OFF				0x00020000
#define	fRTMP_ADAPTER_BULKOUT_RESET			0x00040000
#define	fRTMP_ADAPTER_BULKIN_RESET			0x00080000
#define fRTMP_ADAPTER_RESET_DATA_SW_QUEUE	0x00100000
#define fRTMP_ADAPTER_RESET_PIPE_IN_PROGRESS 0x00200000
#define fRTMP_ADAPTER_SCAN_CHANNEL_IN_PROGRESS 0x04000000
#define	fRTMP_ADAPTER_RADIO_MEASUREMENT		0x08000000

#define fRTMP_ADAPTER_MEDIA_STATE_CHANGE	0x20000000


// Lock bit for accessing different ring buffers
#define fRTMP_ADAPTER_TX_RING_BUSY			0x80000000
#define fRTMP_ADAPTER_PRIO_RING_BUSY		0x40000000
#define fRTMP_ADAPTER_ATIM_RING_BUSY		0x20000000
#define fRTMP_ADAPTER_RX_RING_BUSY			0x10000000

// Lock bit for accessing different queue
#define	fRTMP_ADAPTER_TX_QUEUE_BUSY 		0x08000000
#define	fRTMP_ADAPTER_PRIO_QUEUE_BUSY		0x04000000


//
//	STA operation status flags
//
#define fOP_STATUS_INFRA_ON 				0x00000001
#define fOP_STATUS_ADHOC_ON 				0x00000002
#define fOP_STATUS_BG_PROTECTION_INUSED 	0x00000004
#define fOP_STATUS_SHORT_SLOT_INUSED		0x00000008
#define fOP_STATUS_SHORT_PREAMBLE_INUSED	0x00000010
#define fOP_STATUS_RECEIVE_DTIM 			0x00000020
#define fOP_STATUS_TX_RATE_SWITCH_ENABLED	0x00000040
#define fOP_STATUS_MEDIA_STATE_CONNECTED	0x00000080
#define fOP_STATUS_WMM_INUSED				0x00000100
#define fOP_STATUS_AGGREGATION_INUSED		0x00000200
#define fOP_STATUS_DOZE 					0x00000400
#define fOP_STATUS_MAX_RETRY_ENABLED		0x00000800
#define fOP_STATUS_RTS_PROTECTION_ENABLE	0x00001000
#define fOP_STATUS_FIRMWARE_LOAD			0x00002000
#define fOP_STATUS_BSSID_SET				0x00004000

//
// Flags for Bulkflags control for bulk out data
//
#define	fRTUSB_BULK_OUT_DATA_NULL				0x00000001
#define fRTUSB_BULK_OUT_RTS						0x00000002
#define	fRTUSB_BULK_OUT_MLME					0x00000004
#define	fRTUSB_BULK_OUT_BEACON_1				0x00000008

#define	fRTUSB_BULK_OUT_DATA_NORMAL				0x00010000
#define	fRTUSB_BULK_OUT_DATA_NORMAL_2			0x00020000
#define	fRTUSB_BULK_OUT_DATA_NORMAL_3			0x00040000
#define	fRTUSB_BULK_OUT_DATA_NORMAL_4			0x00080000


#define	fRTUSB_BULK_OUT_BEACON_0				0x00000010
#define	fRTUSB_BULK_OUT_PSPOLL					0x00000020
#define	fRTUSB_BULK_OUT_DATA_FRAG				0x00000040
#define	fRTUSB_BULK_OUT_DATA_FRAG_2				0x00000080
#define	fRTUSB_BULK_OUT_DATA_FRAG_3				0x00000100
#define	fRTUSB_BULK_OUT_DATA_FRAG_4				0x00000200

//
//	AP's client table operation status flags
//
#define fCLIENT_STATUS_WMM_CAPABLE			0x00000001	// CLIENT can parse QOS DATA frame
#define fCLIENT_STATUS_AGGREGATION_CAPABLE	0x00000002	// CLIENT can receive Ralink's proprietary TX aggregation frame

//
//	STA configuration flags
//
//#define fSTA_CFG_ENABLE_TX_BURST			0x00000001


//
// Error code section
//
// NDIS_ERROR_CODE_ADAPTER_NOT_FOUND
#define ERRLOG_READ_PCI_SLOT_FAILED 	0x00000101L
#define ERRLOG_WRITE_PCI_SLOT_FAILED	0x00000102L
#define ERRLOG_VENDOR_DEVICE_NOMATCH	0x00000103L

// NDIS_ERROR_CODE_ADAPTER_DISABLED
#define ERRLOG_BUS_MASTER_DISABLED		0x00000201L

// NDIS_ERROR_CODE_UNSUPPORTED_CONFIGURATION
#define ERRLOG_INVALID_SPEED_DUPLEX 	0x00000301L
#define ERRLOG_SET_SECONDARY_FAILED 	0x00000302L

// NDIS_ERROR_CODE_OUT_OF_RESOURCES
#define ERRLOG_OUT_OF_MEMORY			0x00000401L
#define ERRLOG_OUT_OF_SHARED_MEMORY 	0x00000402L
#define ERRLOG_OUT_OF_MAP_REGISTERS 	0x00000403L
#define ERRLOG_OUT_OF_BUFFER_POOL		0x00000404L
#define ERRLOG_OUT_OF_NDIS_BUFFER		0x00000405L
#define ERRLOG_OUT_OF_PACKET_POOL		0x00000406L
#define ERRLOG_OUT_OF_NDIS_PACKET		0x00000407L
#define ERRLOG_OUT_OF_LOOKASIDE_MEMORY	0x00000408L

// NDIS_ERROR_CODE_HARDWARE_FAILURE
#define ERRLOG_SELFTEST_FAILED			0x00000501L
#define ERRLOG_INITIALIZE_ADAPTER		0x00000502L
#define ERRLOG_REMOVE_MINIPORT			0x00000503L

// NDIS_ERROR_CODE_RESOURCE_CONFLICT
#define ERRLOG_MAP_IO_SPACE 			0x00000601L
#define ERRLOG_QUERY_ADAPTER_RESOURCES	0x00000602L
#define ERRLOG_NO_IO_RESOURCE			0x00000603L
#define ERRLOG_NO_INTERRUPT_RESOURCE	0x00000604L
#define ERRLOG_NO_MEMORY_RESOURCE		0x00000605L

//============================================================
// Length definitions
#define PEER_KEY_NO                       2
#define MAC_ADDR_LEN                      6
#define TIMESTAMP_LEN                     8
#define MAX_LEN_OF_SUPPORTED_RATES        12    // 1, 2, 5.5, 11, 6, 9, 12, 18, 24, 36, 48, 54
#define MAX_LEN_OF_KEY                    32      // 32 octets == 256 bits, Redefine for WPA

#define MAX_NUM_OF_CHANNELS               43    //1-14, 36/40/44/48/52/56/60/64/100/104/108/112/116/120/124/
                                                //128/132/136/140/149/153/157/161/165/34/38/42/46 + 1 as NULL termination
#define MAX_NUM_OF_A_CHANNELS             24    //36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 149, 153, 157, 161, 165
#define J52_CHANNEL_START_OFFSET          38    //1-14, 36/40/44/48/52/56/60/64/100/104/108/112/116/120/124/
                                                //128/132/136/140/149/153/157/161/165/
#define MAX_LEN_OF_SSID                   32
#define CIPHER_TEXT_LEN                   128
#define MAX_LEN_OF_MLME_BUFFER            2048
#define MAX_MLME_HANDLER_MEMORY           20    //each them cantains  MAX_LEN_OF_MLME_BUFFER size
#define MAX_FRAME_LEN                     2338
#define	MAX_VIE_LEN                       160	// New for WPA cipher suite variable IE sizes.

#define MAX_TX_POWER_LEVEL                100   /* mW */
#define MAX_RSSI_TRIGGER                 -10    /* dBm */
#define MIN_RSSI_TRIGGER                 -200   /* dBm */
#define MAX_FRAG_THRESHOLD                2346  /* byte count */
#define MIN_FRAG_THRESHOLD                256   /* byte count */
#define MAX_RTS_THRESHOLD                 2347  /* byte count */
#define MIN_RTS_THRESHOLD                 0

// key related definitions
#define SHARE_KEY_NUM                     4
#define MAX_LEN_OF_SHARE_KEY              16    // byte count
#define MAX_LEN_OF_PEER_KEY               16    // byte count
#define PAIRWISE_KEY_NO                   64    // in MAC ASIC pairwise key table
#define GROUP_KEY_NO                      4
#define	PMKID_NO                          4     // Number of PMKID saved supported


// power status related definitions
#define PWR_ACTIVE                        0
#define PWR_SAVE                          1
#define PWR_UNKNOWN                       2

// BSS Type definitions
#define BSS_ADHOC                         0  // = Ndis802_11IBSS
#define BSS_INFRA                         1  // = Ndis802_11Infrastructure
#define BSS_ANY                           2  // = Ndis802_11AutoUnknown
#define BSS_MONITOR			  3  // = Ndis802_11Monitor

// Reason code definitions
#define REASON_RESERVED                   0
#define REASON_UNSPECIFY                  1
#define REASON_NO_LONGER_VALID            2
#define REASON_DEAUTH_STA_LEAVING         3
#define REASON_DISASSOC_INACTIVE          4
#define REASON_DISASSPC_AP_UNABLE         5
#define REASON_CLS2ERR                    6
#define REASON_CLS3ERR                    7
#define REASON_DISASSOC_STA_LEAVING       8
#define REASON_STA_REQ_ASSOC_NOT_AUTH     9
#define	REASON_INVALID_IE                 13
#define	REASON_MIC_FAILURE                14
#define REASON_4_WAY_TIMEOUT              15
#define REASON_GROUP_KEY_HS_TIMEOUT       16
#define REASON_IE_DIFFERENT               17
#define REASON_MCIPHER_NOT_VALID          18
#define REASON_UCIPHER_NOT_VALID          19
#define REASON_AKMP_NOT_VALID             20
#define REASON_UNSUPPORT_RSNE_VER         21
#define REASON_INVALID_RSNE_CAP           22
#define REASON_8021X_AUTH_FAIL            23
#define REASON_CIPHER_SUITE_REJECTED      24

// Status code definitions
#define MLME_SUCCESS                      0
#define MLME_UNSPECIFY_FAIL               1
#define MLME_CANNOT_SUPPORT_CAP           10
#define MLME_REASSOC_DENY_ASSOC_EXIST     11
#define MLME_ASSOC_DENY_OUT_SCOPE         12
#define MLME_ALG_NOT_SUPPORT              13
#define MLME_SEQ_NR_OUT_OF_SEQUENCE       14
#define MLME_REJ_CHALLENGE_FAILURE        15
#define MLME_REJ_TIMEOUT                  16
#define MLME_ASSOC_REJ_UNABLE_HANDLE_STA  17
#define MLME_ASSOC_REJ_DATA_RATE          18

#define MLME_ASSOC_REJ_NO_EXT_RATE        22
#define MLME_ASSOC_REJ_NO_EXT_RATE_PBCC   23
#define MLME_ASSOC_REJ_NO_CCK_OFDM        24

#define MLME_INVALID_FORMAT               0x51
#define MLME_FAIL_NO_RESOURCE             0x52
#define MLME_STATE_MACHINE_REJECT         0x53
#define MLME_MAC_TABLE_FAIL               0x54

//IE code
#define IE_SSID                           0
#define IE_SUPP_RATES                     1
#define IE_FH_PARM                        2
#define IE_DS_PARM                        3
#define IE_CF_PARM                        4
#define IE_TIM                            5
#define IE_IBSS_PARM                      6
#define IE_COUNTRY                        7     // 802.11d
#define IE_802_11D_REQUEST                10    // 802.11d
#define IE_QBSS_LOAD                      11    // 802.11e d9
#define IE_EDCA_PARAMETER                 12    // 802.11e d9
#define IE_TSPEC                          13    // 802.11e d9
#define IE_TCLAS                          14    // 802.11e d9
#define IE_SCHEDULE                       15    // 802.11e d9
#define IE_CHALLENGE_TEXT                 16
#define IE_POWER_CONSTRAINT               32    // 802.11h d3.3
#define IE_POWER_CAPABILITY               33    // 802.11h d3.3
#define IE_TPC_REQUEST                    34    // 802.11h d3.3
#define IE_TPC_REPORT                     35    // 802.11h d3.3
#define IE_SUPP_CHANNELS                  36    // 802.11h d3.3
#define IE_CHANNEL_SWITCH_ANNOUNCEMENT    37    // 802.11h d3.3
#define IE_MEASUREMENT_REQUEST            38    // 802.11h d3.3
#define IE_MEASUREMENT_REPORT             39    // 802.11h d3.3
#define IE_QUIET                          40    // 802.11h d3.3
#define IE_IBSS_DFS                       41    // 802.11h d3.3
#define IE_ERP                            42    // 802.11g
#define IE_TS_DELAY                       43    // 802.11e d9
#define IE_TCLAS_PROCESSING               44    // 802.11e d9
#define IE_QOS_CAPABILITY                 45    // 802.11e d6
#define IE_EXT_SUPP_RATES                 50    // 802.11g
#define IE_WPA                            221   // WPA
#define IE_VENDOR_SPECIFIC                221   // Wifi,WMM (WME),EOU
#define IE_RSN                            48    // 802.11i d3.0
#define IE_WPA2                           48    // WPA2



// ========================================================
// MLME state machine definition
// ========================================================

// STA MLME state mahcines
#define ASSOC_STATE_MACHINE             1
#define AUTH_STATE_MACHINE              2
#define AUTH_RSP_STATE_MACHINE          3
#define SYNC_STATE_MACHINE              4
#define MLME_CNTL_STATE_MACHINE         5
#define WPA_PSK_STATE_MACHINE           6

//
// STA's CONTROL/CONNECT state machine: states, events, total function #
//
#define CNTL_IDLE                       0
#define CNTL_WAIT_DISASSOC              1
#define CNTL_WAIT_JOIN                  2
#define CNTL_WAIT_REASSOC               3
#define CNTL_WAIT_START                 4
#define CNTL_WAIT_AUTH                  5
#define CNTL_WAIT_ASSOC                 6
#define CNTL_WAIT_AUTH2                 7
#define CNTL_WAIT_OID_LIST_SCAN         8
#define CNTL_WAIT_OID_DISASSOC          9

#define MT2_ASSOC_CONF                  34
#define MT2_AUTH_CONF                   35
#define MT2_DEAUTH_CONF                 36
#define MT2_DISASSOC_CONF               37
#define MT2_REASSOC_CONF                38
#define MT2_PWR_MGMT_CONF               39
#define MT2_JOIN_CONF                   40
#define MT2_SCAN_CONF                   41
#define MT2_START_CONF                  42
#define MT2_GET_CONF                    43
#define MT2_SET_CONF                    44
#define MT2_RESET_CONF                  45

#define MT2_DEAUTH_IND              	46
#define MT2_ASSOC_IND               	47
#define MT2_DISASSOC_IND            	48
#define MT2_REASSOC_IND             	49
#define MT2_AUTH_IND                	50

#define MT2_SCAN_END_CONF               51  // For scan end
#define MT2_MLME_ROAMING_REQ            52

#define CNTL_FUNC_SIZE                  1

//
// STA's ASSOC state machine: states, events, total function #
//
#define ASSOC_IDLE                      0
#define ASSOC_WAIT_RSP                  1
#define REASSOC_WAIT_RSP                2
#define DISASSOC_WAIT_RSP               3
#define MAX_ASSOC_STATE                 4

#define ASSOC_MACHINE_BASE              0
#define MT2_MLME_ASSOC_REQ              0
#define MT2_MLME_REASSOC_REQ            1
#define MT2_MLME_DISASSOC_REQ           2
#define MT2_PEER_DISASSOC_REQ           3
#define MT2_PEER_ASSOC_REQ              4
#define MT2_PEER_ASSOC_RSP              5
#define MT2_PEER_REASSOC_REQ            6
#define MT2_PEER_REASSOC_RSP            7
#define MT2_DISASSOC_TIMEOUT            8
#define MT2_ASSOC_TIMEOUT               9
#define MT2_REASSOC_TIMEOUT             10
#define MAX_ASSOC_MSG                   11

#define ASSOC_FUNC_SIZE                 (MAX_ASSOC_STATE * MAX_ASSOC_MSG)

//
// STA's AUTHENTICATION state machine: states, evvents, total function #
//
#define AUTH_REQ_IDLE					0
#define AUTH_WAIT_SEQ2					1
#define AUTH_WAIT_SEQ4					2
#define MAX_AUTH_STATE					3

#define AUTH_MACHINE_BASE				0
#define MT2_MLME_AUTH_REQ				0
#define MT2_PEER_AUTH_EVEN				1
#define MT2_AUTH_TIMEOUT				2
#define MAX_AUTH_MSG					3

#define AUTH_FUNC_SIZE					(MAX_AUTH_STATE * MAX_AUTH_MSG)

//
// STA's AUTH_RSP state machine: states, events, total function #
//
#define AUTH_RSP_IDLE                   0
#define AUTH_RSP_WAIT_CHAL              1
#define MAX_AUTH_RSP_STATE              2

#define AUTH_RSP_MACHINE_BASE           0
#define MT2_AUTH_CHALLENGE_TIMEOUT      0
#define MT2_PEER_AUTH_ODD               1
#define MT2_PEER_DEAUTH                 2
#define MAX_AUTH_RSP_MSG                3

#define AUTH_RSP_FUNC_SIZE              (MAX_AUTH_RSP_STATE * MAX_AUTH_RSP_MSG)

//
// STA's SYNC state machine: states, events, total function #
//
#define SYNC_IDLE                       0  // merge NO_BSS,IBSS_IDLE,IBSS_ACTIVE and BSS in to 1 state
#define JOIN_WAIT_BEACON                1
#define SCAN_LISTEN                     2
#define MAX_SYNC_STATE                  3

#define SYNC_MACHINE_BASE               0
#define MT2_MLME_SCAN_REQ               0
#define MT2_MLME_JOIN_REQ               1
#define MT2_MLME_START_REQ              2
#define MT2_PEER_BEACON                 3
#define MT2_PEER_PROBE_RSP              4
#define MT2_PEER_ATIM                   5
#define MT2_SCAN_TIMEOUT                6
#define MT2_BEACON_TIMEOUT              7
#define MT2_ATIM_TIMEOUT                8
#define MT2_PEER_PROBE_REQ              9
#define MAX_SYNC_MSG                    10

#define SYNC_FUNC_SIZE                  (MAX_SYNC_STATE * MAX_SYNC_MSG)

//
// STA's WPA-PSK State machine: states, events, total function #
//
#define WPA_PSK_IDLE					0
#define MAX_WPA_PSK_STATE				1

#define WPA_MACHINE_BASE				0
#define MT2_EAPPacket					0
#define MT2_EAPOLStart					1
#define MT2_EAPOLLogoff 				2
#define MT2_EAPOLKey					3
#define MT2_EAPOLASFAlert				4
#define MAX_WPA_PSK_MSG 				5

#define	WPA_PSK_FUNC_SIZE				(MAX_WPA_PSK_STATE * MAX_WPA_PSK_MSG)

//
// MIB access: messages #
//
#define MT2_GET_REQ 					31
#define MT2_SET_REQ 					32
#define MT2_RESET_REQ					33

// =============================================================================

// value domain of MacHdr.tyte, which is b3..b2 of the 1st-byte of MAC header
#define BTYPE_MGMT					0
#define BTYPE_CNTL					1
#define BTYPE_DATA					2

// value domain of MacHdr.subtype, which is b7..4 of the 1st-byte of MAC header
// Management frame
#define SUBTYPE_ASSOC_REQ           0
#define SUBTYPE_ASSOC_RSP           1
#define SUBTYPE_REASSOC_REQ         2
#define SUBTYPE_REASSOC_RSP         3
#define SUBTYPE_PROBE_REQ           4
#define SUBTYPE_PROBE_RSP           5
#define SUBTYPE_BEACON              8
#define SUBTYPE_ATIM                9
#define SUBTYPE_DISASSOC            10
#define SUBTYPE_AUTH                11
#define SUBTYPE_DEAUTH              12
#define SUBTYPE_ACTION              13

// Control Frame
#define SUBTYPE_BLOCK_ACK_REQ       8
#define SUBTYPE_BLOCK_ACK           9
#define SUBTYPE_PS_POLL             10
#define SUBTYPE_RTS                 11  // 1011
#define SUBTYPE_CTS                 12  // 1100
#define SUBTYPE_ACK                 13  // 1101
#define SUBTYPE_CFEND               14
#define SUBTYPE_CFEND_CFACK         15

// Data Frame
#define SUBTYPE_DATA                0
#define SUBTYPE_DATA_CFACK          1
#define SUBTYPE_DATA_CFPOLL         2
#define SUBTYPE_DATA_CFACK_CFPOLL   3
#define SUBTYPE_NULL_FUNC           4
#define SUBTYPE_CFACK               5  // 0101
#define SUBTYPE_CFPOLL              6
#define SUBTYPE_CFACK_CFPOLL        7
#define SUBTYPE_QDATA               8
#define SUBTYPE_QDATA_CFACK         9
#define SUBTYPE_QDATA_CFPOLL        10
#define SUBTYPE_QDATA_CFACK_CFPOLL  11
#define SUBTYPE_QOS_NULL            12
#define SUBTYPE_QOS_CFACK           13
#define SUBTYPE_QOS_CFPOLL          14
#define SUBTYPE_QOS_CFACK_CFPOLL    15

// ACK policy of QOS Control field bit 6:5
#define NORMAL_ACK                  0x00  // b6:5 = 00
#define NO_ACK                      0x20  // b6:5 = 01
#define NO_EXPLICIT_ACK             0x40  // b6:5 = 10
#define BLOCK_ACK                   0x60  // b6:5 = 11

//
// rtmp_data.c use these definition
//
#define	LENGTH_802_11				24
#define LENGTH_802_11_WITH_ADDR4    30
#define	LENGTH_802_11_AND_H			30
#define	LENGTH_802_11_CRC_H			34
#define	LENGTH_802_11_CRC			28
#define	LENGTH_802_3				14
#define LENGTH_802_3_TYPE			2
#define LENGTH_802_1_H				8
#define LENGTH_EAPOL_H				4
#define	LENGTH_CRC                  4
#define	MAX_SEQ_NUMBER				0x0fff

#define	TX_RESULT_SUCCESS       0
#define TX_RESULT_ZERO_LENGTH   1
#define TX_RESULT_UNDER_RUN     2
#define TX_RESULT_PHY_ERROR     4
#define	TX_RESULT_RETRY_FAIL	6

#define	RATE_1					0
#define	RATE_2					1
#define	RATE_5_5				2
#define	RATE_11					3
#define RATE_6					4	// OFDM
#define RATE_9					5	// OFDM
#define RATE_12 				6	// OFDM
#define RATE_18 				7	// OFDM
#define RATE_24 				8	// OFDM
#define RATE_36 				9	// OFDM
#define RATE_48 				10	// OFDM
#define RATE_54 				11	// OFDM
#define RATE_FIRST_OFDM_RATE	RATE_6
#define RATE_AUTO_SWITCH		255 // for UserCfg.FixedTxRate only


#define CCK_RATE                    1
#define OFDM_RATE                   2
#define CCKOFDM_RATE                3

// pTxD->Ifs
#define	IFS_BACKOFF				0
#define	IFS_SIFS				1

// pTxD->RetryMode
#define	LONG_RETRY				1
#define	SHORT_RETRY				0

// Country Region definition
#define REGION_0_BG_BAND                  0 	  // 1-11
#define REGION_1_BG_BAND                  1 	  // 1-13
#define REGION_2_BG_BAND                  2 	  // 10-11
#define REGION_3_BG_BAND                  3 	  // 10-13
#define REGION_4_BG_BAND                  4 	  // 14
#define REGION_5_BG_BAND                  5 	  // 1-14
#define REGION_6_BG_BAND                  6 	  // 3-9
#define REGION_7_BG_BAND                  7 	  // 5-13
#define REGION_MAXIMUM_BG_BAND            REGION_7_BG_BAND

#define REGION_0_A_BAND                   0 	  // 36, 40, 44, 48, 52, 56, 60, 64, 149, 153, 157, 161, 165
#define REGION_1_A_BAND                   1 	  // 36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140
#define REGION_2_A_BAND                   2 	  // 36, 40, 44, 48, 52, 56, 60, 64
#define REGION_3_A_BAND                   3 	  // 52, 56, 60, 64, 149, 153, 157, 161
#define REGION_4_A_BAND                   4 	  // 149, 153, 157, 161, 165
#define REGION_5_A_BAND                   5 	  // 149, 153, 157, 161
#define REGION_6_A_BAND                   6 	  // 36, 40, 44, 48
#define REGION_7_A_BAND                   7 	  // 36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 149, 153, 157, 161, 165
#define REGION_8_A_BAND                   8       // 52, 56, 60, 64
#define REGION_9_A_BAND                   9       // 34, 38, 42, 46
#define REGION_10_A_BAND                  10      // 34, 36, 38, 40, 42, 44, 46, 48, 52, 56, 60, 64
#define REGION_MAXIMUM_A_BAND             REGION_10_A_BAND


// pTxD->CipherAlg
#define	CIPHER_NONE				0
#define	CIPHER_WEP64			1
#define	CIPHER_WEP128			2
#define	CIPHER_TKIP				3
#define	CIPHER_AES				4
#define CIPHER_CKIP64			5
#define CIPHER_CKIP128			6
#define CIPHER_TKIP_NO_MIC		7	 // MIC has been appended by driver, not a valid value in hardware key table


// value domain for pAd->RfIcType
#define RFIC_5226				1  //A/B/G
#define RFIC_2528				2  //B/G
#define RFIC_5225				3  //A/B/G
#define RFIC_2527				4  //B/G

// LED Status.
#define LED_LINK_DOWN               0
#define LED_LINK_UP                 1
#define LED_RADIO_OFF               2
#define LED_RADIO_ON                3
#define LED_HALT                    4

// value domain of pAdapter->LedCntl.LedMode and E2PROM
#define LED_MODE_DEFAULT			0
#define LED_MODE_TWO_LED			1
#define LED_MODE_SIGNAL_STREGTH		2


// RC4 init value, used fro WEP & TKIP
#define PPPINITFCS32			0xffffffff	 /* Initial FCS value */

#define PAIRWISE_KEY_TABLE			1
#define SHARED_KEY_TABLE			0

#define DEFAULT_BBP_TX_POWER		0
#define DEFAULT_RF_TX_POWER 		5

#define MAX_INI_BUFFER_SIZE 		(4*1024)	// 4K bytes

#define MAX_LEN_OF_MAC_TABLE        64
#define HASH_TABLE_SIZE             256

// Event definition
#define MAX_NUM_OF_EVENT            10  // entry # in EVENT table
#define EVENT_MAX_EVENT_TYPE        6


// 802.1X controlled port definition
#define	WPA_802_1X_PORT_SECURED			1
#define	WPA_802_1X_PORT_NOT_SECURED		2

#define	PAIRWISE_KEY			1
#define	GROUP_KEY				2

#define AUTH_MODE_OPEN					  0x00
#define AUTH_MODE_KEY					  0x01
#define AUTH_MODE_AUTO_SWITCH			  0x03

// wpapsk EAPOL Key descripter frame format related length
#define LEN_KEY_DESC_NONCE          32
#define LEN_KEY_DESC_IV             16
#define LEN_KEY_DESC_RSC            8
#define LEN_KEY_DESC_ID             8
#define LEN_KEY_DESC_REPLAY         8
#define LEN_KEY_DESC_MIC            16

#define LEN_MASTER_KEY              32

// EAPOL EK, MK
#define LEN_EAP_EK                  16
#define LEN_EAP_MICK                16
#define LEN_EAP_KEY                 ((LEN_EAP_EK)+(LEN_EAP_MICK))
#define PMK_LEN                     32

// TKIP key related
#define LEN_PMKID                   16
#define LEN_TKIP_EK                 16
#define LEN_TKIP_RXMICK             8
#define LEN_TKIP_TXMICK             8
#define LEN_AES_EK                  16
#define LEN_AES_KEY                 LEN_AES_EK
#define LEN_TKIP_KEY                ((LEN_TKIP_EK)+(LEN_TKIP_RXMICK)+(LEN_TKIP_TXMICK))
#define TKIP_AP_TXMICK_OFFSET       ((LEN_EAP_KEY)+(LEN_TKIP_EK))
#define TKIP_AP_RXMICK_OFFSET       (TKIP_AP_TXMICK_OFFSET+LEN_TKIP_TXMICK)
#define	TKIP_GTK_LENGTH             ((LEN_TKIP_EK)+(LEN_TKIP_RXMICK)+(LEN_TKIP_TXMICK))

#define LEN_PTK                     ((LEN_EAP_KEY)+(LEN_TKIP_KEY))
#define MAX_LEN_OF_RSNIE            80
#define MIN_LEN_OF_RSNIE            8


// definition RSSI Number
#define RSSI_NO_1					1
#define RSSI_NO_2					2


// definition of radar detection
#define RD_NORMAL_MODE              0	// Not found radar signal
#define RD_SWITCHING_MODE           1	// Found radar signal, and doing channel switch
#define RD_SILENCE_MODE             2	// After channel switch, need to be silence a while to ensure radar not found

#define MAX_CFG_BUFFER_LEN          1056
////////////////////////////////////////////////////////////////////////////
// RT73 internal usage
////////////////////////////////////////////////////////////////////////////
#define	CMD_RESET_BULKOUT		0
#define	CMD_RESET_BULKIN		1
#define	CMD_RESET_FROM_ERROR		2
#define	CMD_LINK_DOWN			3
#define	CMD_UPDATE_TX_RATE		4
#define	CMD_SET_PSM_SAVE		5
#define	CMD_RESET_FROM_NDIS		6
#define	CMD_PERIODIC_EXECUT		7
#define CMD_ASICLED_EXECUT		8
#define CMD_CHECK_GPIO			9
#define CMD_Remove_AllKeys		10
#define CMD_SOFT_DIVERSITY		11
#define CMD_FORCE_WAKEUP		12
#define CMD_SET_PSM_ACTIVE		13

#define COMMAND_QUEUE_SIZE		14


#define	USB_DEVICE_MAX_CONFIG_DESCRIPTOR_SIZE	1024

#define UNLINK_TIMEOUT_MS		3

//-------------------
// Frame Sizes
//-------------------

#define MAC_ADDRESS_LENGTH              6

#define HEADER_SIZE                     14
#define MAXIMUM_PACKET_SIZE             1500

#define	USB_DEVICE_MAX_CONFIG_DESCRIPTOR_SIZE	1024

#define MAX_QUEUE_SIZE                  100

#define USB_TX_HEADER_SIZE              8//WLength+TxRate+PaddingBytes+Reserved
#define USB_RX_HEADER_SIZE              12

#define WIRELESS_HEADER_OVERHEAD        18
#define MAX_TX_PADDING_BYTES            50
#define MAX_RX_PADDING_BYTES            66
#define CRC32_BYTES                     4

#define MAX_WIRELESS_SIZE               WIRELESS_HEADER_OVERHEAD + HEADER_SIZE + MAXIMUM_PACKET_SIZE + CRC32_BYTES


//--------------------
// REQUEST SUPPORT
//--------------------
#define DEVICE_VENDOR_REQUEST_OUT       (USB_DIR_OUT | USB_TYPE_VENDOR | USB_RECIP_DEVICE)  // 0x40
#define DEVICE_VENDOR_REQUEST_IN        (USB_DIR_IN | USB_TYPE_VENDOR | USB_RECIP_DEVICE )  // 0xc0
#define INTERFACE_VENDOR_REQUEST_OUT    0x41
#define INTERFACE_VENDOR_REQUEST_IN     0xc1


#define	RETRY_LIMIT			3

typedef UCHAR ADDRESS[MAC_ADDRESS_LENGTH];


//-------------------
// VID/PID
//-------------------

#define RT73_USB_DEVICES { \
 /* AboCom */\
 {USB_DEVICE(0x07b8,0xb21d)},\
 /* Askey */\
 {USB_DEVICE(0x1690,0x0722)},\
 /* ASUS */\
 {USB_DEVICE(0x0b05,0x1723)},\
 {USB_DEVICE(0x0b05,0x1724)},\
 /* Buffalo */\
 {USB_DEVICE(0x0411,0x00d8)},	/* WLI-US-SG54HP */\
 {USB_DEVICE(0x0411,0x00f4)},\
 /* Belkin */\
 {USB_DEVICE(0x050d,0x7050)},\
 {USB_DEVICE(0x050d,0x705a)},\
 {USB_DEVICE(0x050d,0x905b)},\
 {USB_DEVICE(0x050d,0x905c)},\
 /* Billionton */\
 {USB_DEVICE(0x1631,0xc019)},\
 /* CNet */\
 {USB_DEVICE(0x1371,0x9022)},\
 {USB_DEVICE(0x1371,0x9032)},\
 /* Conceptronic */\
 {USB_DEVICE(0x14b2,0x3c22)},\
 /* Corega */\
 {USB_DEVICE(0x07aa, 0x002e)}, /* CG-WLUSB2GPX*/\
 /* D-Link */\
 {USB_DEVICE(0x07d1,0x3c03)},\
 {USB_DEVICE(0x07d1,0x3c04)},\
 {USB_DEVICE(0x07d1,0x3c06)}, /* D-Link DWA111 - thanks to zenon_666 */ \
 {USB_DEVICE(0x07d1,0x3c07)},\
 /* Gemtek*/\
 {USB_DEVICE(0x15a9,0x0004)},\
 /* Gigabyte */\
 {USB_DEVICE(0x1044,0x8008)},\
 {USB_DEVICE(0x1044,0x800a)},\
 /* Huawei-3Com */\
 {USB_DEVICE(0x1472,0x0009)},\
 /* Hercules */\
 {USB_DEVICE(0x06f8,0xe010)},\
 {USB_DEVICE(0x06f8,0xe020)},\
 /* LinkSys */\
 {USB_DEVICE(0x13b1,0x0020)},\
 {USB_DEVICE(0x13b1,0x0023)},\
 /* MSI */\
 {USB_DEVICE(0x0db0,0x6877)},\
 {USB_DEVICE(0x0db0,0x6874)},\
 {USB_DEVICE(0x0db0,0xa861)},\
 {USB_DEVICE(0x0db0,0xa874)},\
 /* Ralink */\
 {USB_DEVICE(0x04bb,0x093d)},/* From ver 1.1.0.2 of their driver */\
 {USB_DEVICE(0x148f,0x2573)},\
 {USB_DEVICE(0x148f,0x2671)},\
 /* Qcom */\
 {USB_DEVICE(0x18e8,0x6196)},\
 {USB_DEVICE(0x18e8,0x6229)},\
 {USB_DEVICE(0x18e8,0x6238)},\
 /* Sitecom */\
 {USB_DEVICE(0x0df6,0x9712)},\
 {USB_DEVICE(0x0df6,0x90ac)},\
 /* Surecom */\
 {USB_DEVICE(0x0769,0x31f3)},\
 /* Planex */\
 {USB_DEVICE(0x2019,0xab01)},\
 {USB_DEVICE(0x2019,0xab50)},\
 /* Senao */\
 {USB_DEVICE(0x1740,0x7100)},\
 {0}} /* end marker */


#endif	// __RTMP_DEF_H__

