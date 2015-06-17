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
 *	Module Name:	rt73.h
 *
 *	Abstract: RT2573 ASIC related definition & structures
 *
 *	Revision History:
 *	Who		When		What
 *	--------	----------	-----------------------------
 *	Nemo Tang	02-20-2005	created
 *	idamlaj	05-10-2006	Import rfmon implementation
 *
 ***************************************************************************/

#ifndef	__RT73_H__
#define	__RT73_H__
////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////
// reg for RT73 ///////////
//////////////////////////

#ifdef CONFIG_RT73_WIRELESS_FRMW_BUILTIN
extern char __initrt73frmw_start[];
extern char __initrt73frmw_end[];
#define FIRMWAREIMAGE_LENGTH    (__initrt73frmw_end - __initrt73frmw_start)
#else
#define FIRMWAREIMAGE_LENGTH MAX_FIRMWARE_IMAGE_SIZE
#endif

// 8051 firmware image - base address = 0x4000
#define FIRMWARE_IMAGE_BASE     0x800
#define FIRMWARE_IMAGE_SIZE	2048	// 2k byte
#define MAX_FIRMWARE_IMAGE_SIZE        2048

//
// Security key table memory, base address = 0x1000
//
#define SHARED_KEY_TABLE_BASE       0x1000      // 32-byte * 16-entry = 512-byte
#define PAIRWISE_KEY_TABLE_BASE     0x1200      // 32-byte * 64-entry = 2048-byte
#define PAIRWISE_TA_TABLE_BASE      0x1a00      // 8-byte * 64-entry = 512-byte

// 32-byte per entry, total 16-entry for shared key table, 64-entry for pairwise key table
typedef struct _HW_KEY_ENTRY {          // 32-byte per entry
	UCHAR   Key[16];
	UCHAR   TxMic[8];
	UCHAR   RxMic[8];
} HW_KEY_ENTRY, *PHW_KEY_ENTRY;
#define HW_KEY_ENTRY_SIZE           sizeof(HW_KEY_ENTRY)

// 64-entry for pairwise key table
typedef struct _HW_PAIRWISE_TA_ENTRY {  // 8-byte per entry
	UCHAR   Address[6];
	UCHAR   Rsv[2];
} HW_PAIRWISE_TA_ENTRY, PHW_PAIRWISE_TA_ENTRY;
#define HW_PAIRWISE_TA_ENTRY_SIZE   sizeof(HW_PAIRWISE_TA_ENTRY)

#define HW_DEBUG_SETTING_BASE   0x2bf0  // 0x2bf0~0x2bff total 16 bytes
#define HW_DEBUG_SETTING_END	0x2bff

// on-chip BEACON frame space - base address = 0x2400
#define HW_BEACON_BASE0         0x2400
#define HW_BEACON_BASE1         0x2500
#define HW_BEACON_BASE2         0x2600
#define HW_BEACON_BASE3         0x2700

//
// MAC Control Registers - base address 0x3000
//
#define MAC_CSR0            0x3000
#define MAC_CSR1            0x3004
#define MAC_CSR2            0x3008
#define MAC_CSR3            0x300c
#define MAC_CSR4            0x3010
#define MAC_CSR5            0x3014
#define MAC_CSR6            0x3018
#define MAC_CSR7            0x301c
#define MAC_CSR8            0x3020  // SIFS/EIFS
#define MAC_CSR9            0x3024
#define MAC_CSR10           0x3028  // power state configuration
#define MAC_CSR11           0x302c  // Power state transition time
#define MAC_CSR12           0x3030  // power state
#define MAC_CSR13           0x3034  // GPIO
#define MAC_CSR14           0x3038  // LED control
#define MAC_CSR15           0x303c  // NAV control

//
// TXRX control registers - base address 0x3000
//
#define TXRX_CSR0           0x3040
#define TXRX_CSR1           0x3044
#define TXRX_CSR2           0x3048
#define TXRX_CSR3           0x304c
#define TXRX_CSR4           0x3050
#define TXRX_CSR5           0x3054
#define TXRX_CSR6           0x3058  // ACK/CTS payload consumed time
#define TXRX_CSR7           0x305c  // ACK/CTS payload consumed time
#define TXRX_CSR8           0x3060  // ACK/CTS payload consumed time
#define TXRX_CSR9           0x3064  // BEACON SYNC
#define TXRX_CSR10          0x3068  // BEACON alignment
#define TXRX_CSR11          0x306c  // AES mask
#define TXRX_CSR12          0x3070  // TSF low 32
#define TXRX_CSR13          0x3074  // TSF high 32
#define TXRX_CSR14          0x3078  // TBTT timer
#define TXRX_CSR15          0x307c  // TKIP MIC priority byte "AND" mask

//
// PHY control registers - base address 0x3000
//
#define PHY_CSR0            0x3080  // RF/PS control
#define PHY_CSR1            0x3084
#define PHY_CSR2            0x3088  // pre-TX BBP control
#define PHY_CSR3            0x308c  // BBP access
#define PHY_CSR4            0x3090  // RF serial control
#define PHY_CSR5            0x3094  // RX to TX signal switch timing control
#define PHY_CSR6            0x3098  // TX to RX signal timing control
#define PHY_CSR7            0x309c  // TX DAC switching timing control

//
// Security control register - base address 0x3000
//
#define SEC_CSR0            0x30a0  // shared key table control
#define SEC_CSR1            0x30a4  // shared key table security mode
#define SEC_CSR2            0x30a8  // pairwise key table valid bitmap 0
#define SEC_CSR3            0x30ac  // pairwise key table valid bitmap 1
#define SEC_CSR4            0x30b0  // pairwise key table lookup control
#define SEC_CSR5            0x30b4  // shared key table security mode

//
// STA control registers - base address 0x3000
//
#define STA_CSR0            0x30c0  // CRC/PLCP error counter
#define STA_CSR1            0x30c4  // Long/False-CCA error counter
#define STA_CSR2            0x30c8  // RX FIFO overflow counter
#define STA_CSR3            0x30cc  // TX Beacon counter
#define STA_CSR4            0x30d0  // TX Retry (1) Counters
#define STA_CSR5            0x30d4  // TX Retry (2) Counters

//
// QOS control registers - base address 0x3000
//
#define QOS_CSR0            0x30e0  // TXOP holder MAC address 0
#define QOS_CSR1            0x30e4  // TXOP holder MAC address 1
#define QOS_CSR2            0x30e8  // TXOP holder timeout register
#define QOS_CSR3            0x30ec  // RX QOS-CFPOLL MAC address 0
#define QOS_CSR4            0x30f0  // RX QOS-CFPOLL MAC address 1
#define QOS_CSR5            0x30f4  // "QosControl" field of the RX QOS-CFPOLL



////////WMM Scheduler Register////////////

#define AIFSN_CSR               0x0400
#define CWMIN_CSR           	0x0404
#define CWMAX_CSR           	0x0408
#define AC_TXOP_CSR0        	0x040c
#define AC_TXOP_CSR1        	0x0410
////////////////////////////////////////////////////////////////////////////////////////

// ================================================================
// Tx /	Rx / Mgmt ring descriptor definition
// ================================================================

// value domain of pTxD->Owner and pRxD->Owner
#define	DESC_OWN_HOST		    0
#define	DESC_OWN_NIC		    1

// the following PID values are used to mark outgoing frame type in TXD so that
// proper TX statistics can be collected based on these categories
#define PID_DATA_REQUIRE_ACK    0x00    // b0~6 = MAC table index when acking as AP
#define PID_DATA_WITHOUT_ACK    0x40    // b0~6 = MAC table index when acting as AP
#define PID_NULL_AT_HIGH_RATE   0x80
#define PID_RTS_FRAME           0x81
#define PID_MGMT_FRAME          0x82
#define PID_CNTL_FRAME          0x83    // other non-RTS Control frame
#define PID_MCU_INTERNAL        0xff    // frame generated internally by 8051

#if 0
// the following PID values are used to mark outgoing frame type in TXD->PID so that
// proper TX statistics can be collected based on these categories
// b7-6 of PID field -
#define PTYPE_DATA_REQUIRE_ACK  0x00 // b7-6:00, b5-0: 0~59 is MAC table index (AID?), 60~63 is WDS index
#define PTYPE_NULL_AT_HIGH_RATE 0x40 // b7-6:01, b5-0: 0~59 is MAC table index (AID?), 60~63 is WDS index
#define PTYPE_RESERVED          0x80 // b7-6:10
#define PTYPE_SPECIAL           0xc0 // b7-6:11

// when b7-6=11 (PTYPE_SPECIAL), b5-0 coube be ...
#define PSUBTYPE_DATA_NO_ACK    0x00
#define PSUBTYPE_MGMT           0x01
#define PSUBTYPE_OTHER_CNTL     0x02
#define PSUBTYPE_RTS            0x03
#define PSUBTYPE_MCU_INTERNAL   0x04
#endif

// value domain of pTxD->HostQId (4-bit: 0~15)
#define QID_AC_BK               1   // meet ACI definition in 802.11e
#define QID_AC_BE               0   // meet ACI definition in 802.11e
#define QID_AC_VI               2
#define QID_AC_VO               3
#define QID_HCCA                4
#define NUM_OF_TX_RING          5
#define QID_MGMT                13
#define QID_RX                  14
#define QID_OTHER               15

//-----------------------------------------------------
// BBP & RF	definition
//-----------------------------------------------------
#define	BUSY		1
#define	IDLE		0

#define	BBP_R0					    0  // version
#define	BBP_R1				        1  // TSSI
#define	BBP_R2          			2  // TX configure
#define BBP_R3                      3
#define BBP_R4                      4
#define BBP_R5                      5
#define BBP_R6                      6
#define	BBP_R14			            14 // RX configure
#define BBP_R16                     16
#define BBP_R17                     17 // RX sensibility
#define BBP_R18                     18
#define BBP_R21                     21
#define BBP_R22                     22
#define BBP_R32                     32
#define BBP_R62                     62 // Rx SQ0 Threshold HIGH
#define BBP_R64                     64
#define BBP_R66                     66
#define BBP_R70                     70 // Japan filter
#define BBP_R77                     77
#define BBP_R82                     82
#define BBP_R83                     83
#define BBP_R84                     84
#define BBP_R94                     94 // Tx Gain Control

#define BBPR94_DEFAULT              0x06 // Add 1 value will gain 1db

#define RSSI_FOR_VERY_LOW_SENSIBILITY -35
#define RSSI_FOR_LOW_SENSIBILITY    -58
#define RSSI_FOR_MID_LOW_SENSIBILITY  -66
#define RSSI_FOR_MID_SENSIBILITY    -74

//-------------------------------------------------------------------------
// EEPROM definition
//-------------------------------------------------------------------------
#define EEDO        0x10
#define EEDI        0x08
#define EECS        0x04
#define EESK        0x02
#define EERL        0x01

#define EEPROM_WRITE_OPCODE     0x05
#define EEPROM_READ_OPCODE      0x06
#define EEPROM_EWDS_OPCODE      0x10
#define EEPROM_EWEN_OPCODE      0x13

#define	NUM_EEPROM_BBP_PARMS		19
#define	NUM_EEPROM_TX_G_PARMS			7
#define	NUM_EEPROM_BBP_TUNING_PARMS	7
#define EEPROM_VERSION_OFFSET       0x2
#define	EEPROM_MAC_ADDRESS_BASE_OFFSET		0x4
#define	EEPROM_BBP_BASE_OFFSET		0x20
#define	EEPROM_G_TX_PWR_OFFSET		0x46
#define	EEPROM_FREQ_OFFSET			0x5e
#define EEPROM_LED_OFFSET			0x60
#define EEPROM_A_TX_PWR_OFFSET      0x62
#define EEPROM_J52_TX_PWR_OFFSET    0x7C  //Skip 0x7C, it start from 0x7D
#define	EEPROM_TSSI_REF_OFFSET		0x4A
#define	EEPROM_TSSI_DELTA_OFFSET	0x1A

#define	EEPROM_RSSI_BG_OFFSET			0x9a
#define	EEPROM_RSSI_A_OFFSET			0x9c
#define	EEPROM_BG_TSSI_CALIBRAION	0x54
#define	EEPROM_A_TSSI_CALIBRAION	0x90
#define EEPROM_TXPOWER_DELTA_OFFSET 0x9e

// =================================================================================
// TX / RX ring descriptor format
// =================================================================================

// the first 24-byte in TXD is called TXINFO and will be DMAed to MAC block through TXFIFO.
// MAC block use this TXINFO to control the transmission behavior of this frame.
#define TXINFO_SIZE                 24

//
// TX descriptor format, Tx	ring, Mgmt Ring
//
#ifdef BIG_ENDIAN
typedef	struct	_TXD_STRUC {
	// Word 0
	ULONG       CipherAlg:3;
	ULONG       Burst2:1;			// definition as same as "Burst", for backward compatible set this one to the same as "Burst" set.
	ULONG       DataByteCnt:12;
	ULONG       KeyIndex:6;         // Key index (0~31) to the pairwise KEY table; or
									// 0~3 to shared KEY table 0 (BSS0). STA always use BSS0
									// 4~7 to shared KEY table 1 (BSS1)
									// 8~11 to shared KEY table 2 (BSS2)
									// 12~15 to shared KEY table 3 (BSS3)

	ULONG       KeyTable:1;        	// 1: use per-client pairwise KEY table, 0: shared KEY table
	ULONG       TkipMic:1;         	// 1: ASIC is responsible for appending TKIP MIC if TKIP is inused
	ULONG		RetryMd:1;         	// 1: Long retry (4 times), 0: short retry (7 times)
	ULONG		IFS:1;             	// 1: require a BACKOFF before this frame, 0:SIFS before this frame
	ULONG      	Ofdm:1;				// 1: TX using OFDM rates
	ULONG		Timestamp:1;		// 1: MAC auto overwrite current TSF into frame body
	ULONG		ACK:1;             	// 1: ACK is required
	ULONG		MoreFrag:1;			// 1: More fragment following this frame
	ULONG		Drop:1;				// 0: skip this frame, 1:valid frame inside
	ULONG		Burst:1;			// 1: Contiguously used current End Ponit, eg, Fragment packet should turn on.
									//	Tell EDCA that the next frame belongs to the same "burst" even though TXOP=0

	// Word	1
	ULONG       BufCount:3;         // number of buffers in this TXD
	ULONG       HwSeq:1;            // MAC auto replace the 12-bit frame sequence #
	ULONG       :6;
	ULONG       IvOffset:6;
	ULONG       Cwmax:4;
	ULONG       Cwmin:4;
	ULONG       Aifsn:4;
	ULONG       HostQId:4;          // EDCA/HCCA queue ID

	// Word	2
	ULONG      	PlcpLengthHigh:8;
	ULONG      	PlcpLengthLow:8;
	ULONG      	PlcpService:8;
	ULONG      	PlcpSignal:8;

	// Word	3
	ULONG       Iv;

	// Word	4
	ULONG       Eiv;

	// Word 5
	ULONG       Reserved:7;
	ULONG       bWaitingDmaDoneInt:1; // pure s/w flag. 1:TXD been filled with data and waiting for TxDoneISR for housekeeping
	ULONG       BbpTxPower:8;
	ULONG       PktId:8;            // driver assigned packet ID to categorize TXResult in TxDoneInterrupt
	ULONG       FrameOffset:8;      // frame start offset inside ASIC TXFIFO (after TXINFO field)

	// the above 24-byte is called TXINFO and will be DMAed to MAC block through TXFIFO.
	// MAC block use this TXINFO to control the transmission behavior of this frame.

	// The following fields are not used by MAC block. They are used by DMA block and HOST
	// driver only. Once a frame has been DMA to ASIC, all the following fields are useless
	// to ASIC.
}	TXD_STRUC, *PTXD_STRUC;
#else
typedef	struct	_TXD_STRUC {
	// word 0
	ULONG		Burst:1;			// 1: Contiguously used current End Ponit, eg, Fragment packet should turn on.
									//    Tell EDCA that the next frame belongs to the same "burst" even though TXOP=0
	ULONG		Drop:1;			    // 0: skip this frame, 1:valid frame inside
	ULONG		MoreFrag:1;			// 1: More fragment following this frame
	ULONG		ACK:1;              // 1: ACK is required
	ULONG		Timestamp:1;        // 1: MAC auto overwrite current TSF into frame body
	ULONG       	Ofdm:1;             // 1: TX using OFDM rates
	ULONG		IFS:1;              // 1: require a BACKOFF before this frame, 0:SIFS before this frame
	ULONG		RetryMd:1;          // 1: Long retry (4 times), 0: short retry (7 times)

	ULONG       TkipMic:1;          // 1: ASIC is responsible for appending TKIP MIC if TKIP is inused
	ULONG       KeyTable:1;         // 1: use per-client pairwise KEY table, 0: shared KEY table
	ULONG       KeyIndex:6;         // Key index (0~31) to the pairwise KEY table; or
									// 0~3 to shared KEY table 0 (BSS0). STA always use BSS0
									// 4~7 to shared KEY table 1 (BSS1)
									// 8~11 to shared KEY table 2 (BSS2)
									// 12~15 to shared KEY table 3 (BSS3)

	ULONG       DataByteCnt:12;
	ULONG       Burst2:1;            // definition as same as "Burst", for backward compatible set this one to the same as "Burst" set.
	ULONG       CipherAlg:3;

	// Word	1
	ULONG       HostQId:4;          // EDCA/HCCA queue ID
	ULONG       Aifsn:4;
	ULONG       Cwmin:4;
	ULONG       Cwmax:4;
	ULONG       IvOffset:6;
	ULONG       :6;
	ULONG       HwSeq:1;            // MAC auto replace the 12-bit frame sequence #
	ULONG       BufCount:3;         // number of buffers in this TXD

	// Word	2
	ULONG      	PlcpSignal:8;
	ULONG      	PlcpService:8;
	ULONG      	PlcpLengthLow:8;
	ULONG      	PlcpLengthHigh:8;

	// Word	3
	ULONG       Iv;

	// Word	4
	ULONG       Eiv;

	// Word 5
	ULONG       FrameOffset:8;      // frame start offset inside ASIC TXFIFO (after TXINFO field)
	ULONG       PktId:8;            // driver assigned packet ID to categorize TXResult in TxDoneInterrupt
	ULONG       BbpTxPower:8;
	ULONG       bWaitingDmaDoneInt:1; // pure s/w flag. 1:TXD been filled with data and waiting for TxDoneISR for housekeeping
	ULONG       Reserved:7;

	// the above 24-byte is called TXINFO and will be DMAed to MAC block through TXFIFO.
	// MAC block use this TXINFO to control the transmission behavior of this frame.

	// The following fields are not used by MAC block. They are used by DMA block and HOST
	// driver only. Once a frame has been DMA to ASIC, all the following fields are useless
	// to ASIC.
}	TXD_STRUC, *PTXD_STRUC;
#endif

//
// Rx descriptor format, Rx	Ring
//
#ifdef BIG_ENDIAN
typedef	struct	_RXD_STRUC	{
	// Word	0
	ULONG		CipherAlg:3;
	ULONG		Rsv:1;
	ULONG		DataByteCnt:12;
	ULONG		KeyIndex:6;         // decryption key actually used
	ULONG		CipherErr:2;        // 0: decryption okay, 1:ICV error, 2:MIC error, 3:KEY not valid
//	ULONG		PhyErr:1;           // 1: this frame is received with PHY error
	ULONG		Ofdm:1;             // 1: this frame is received in OFDM rate
	ULONG		Crc:1;              // 1: CRC error
	ULONG		MyBss:1;            // 1: this frame belongs to the same BSSID
	ULONG		Bcast:1;            // 1: this is a broadcast frame
	ULONG		Mcast:1;            // 1: this is a multicast frame
	ULONG		U2M:1;              // 1: this RX frame is unicast to me
	ULONG		Drop:1;             // 1: drop without receiving to HOST
	ULONG		Owner:1;            // 1: owned by ASIC, 0: owned by HOST driver

	// Word 1
	ULONG		Rsv1:1;
	ULONG		FrameOffset:7;
	ULONG		Rsv0:8;
	ULONG		PlcpRssi:8;         // RSSI reported by BBP
	ULONG		PlcpSignal:8;       // RX raw data rate reported by BBP

	// Word 2
	ULONG		Iv;                 // received IV if originally encrypted; for replay attack checking

	// Word 3
	ULONG       Eiv;                // received EIV if originally encrypted; for replay attack checking

	// Word 4
	ULONG		Rsv2;

	// The above 20-byte is called RXINFO and is prepared by MAC RX block and passed
	// the HOST driver.

	// The following fields are for DMA block and HOST usage only. Can't be touched
	// by ASIC MAC block.

	// Word	5
	ULONG		Rsv3;	// BufPhyAddr;
#if 0
	// Word	6~15
	ULONG       Rsv3;
	ULONG       Rsv4;
	ULONG       Rsv5;
	ULONG       Rsv6;
	ULONG       Rsv7;
	ULONG       Rsv8;
	ULONG       Rsv9;
	ULONG       Rsv10;
	ULONG       Rsv11;
	ULONG       Rsv12;
#endif
}	RXD_STRUC, *PRXD_STRUC;
#else
typedef	struct	_RXD_STRUC	{
	// Word	0
	ULONG		Owner:1;            // 1: owned by ASIC, 0: owned by HOST driver
	ULONG       Drop:1;             // 1: drop without receiving to HOST
	ULONG		U2M:1;              // 1: this RX frame is unicast to me
	ULONG		Mcast:1;            // 1: this is a multicast frame
	ULONG		Bcast:1;            // 1: this is a broadcast frame
	ULONG		MyBss:1;            // 1: this frame belongs to the same BSSID
	ULONG		Crc:1;              // 1: CRC error
	ULONG		Ofdm:1;             // 1: this frame is received in OFDM rate
//	ULONG		PhyErr:1;           // 1: this frame is received with PHY error
	ULONG		CipherErr:2;        // 0: decryption okay, 1:ICV error, 2:MIC error, 3:KEY not valid
	ULONG       KeyIndex:6;         // decryption key actually used
	ULONG		DataByteCnt:12;
	ULONG       Rsv:1;
	ULONG       CipherAlg:3;

	// word 1
	ULONG       PlcpSignal:8;       // RX raw data rate reported by BBP
	ULONG       PlcpRssi:8;         // RSSI reported by BBP
	ULONG       Rsv0:8;
	ULONG       FrameOffset:7;
	ULONG       Rsv1:1;

	// Word	2
	ULONG       Iv;                 // received IV if originally encrypted; for replay attack checking

	// Word 3
	ULONG       Eiv;                // received EIV if originally encrypted; for replay attack checking

	// word 4
	ULONG       Rsv2;

	// The above 20-byte is called RXINFO and is prepared by MAC RX block and passed
	// the HOST driver.

	// The following fields are for DMA block and HOST usage only. Can't be touched
	// by ASIC MAC block.

	// Word	5
	ULONG		Rsv3;	// BufPhyAddr;
#if 0
	// Word	6~15
	ULONG       Rsv3;
	ULONG       Rsv4;
	ULONG       Rsv5;
	ULONG       Rsv6;
	ULONG       Rsv7;
	ULONG       Rsv8;
	ULONG       Rsv9;
	ULONG       Rsv10;
	ULONG       Rsv11;
	ULONG       Rsv12;
#endif
}	RXD_STRUC, *PRXD_STRUC;
#endif

// =================================================================================
// HOST-MCU communication data structure
// =================================================================================

//
// H2M_MAILBOX_CSR: Host-to-MCU Mailbox
//
#ifdef BIG_ENDIAN
typedef union  _H2M_MAILBOX_STRUC {
	struct {
		ULONG       Owner:8;
		ULONG       CmdToken:8;    // 0xff tells MCU not to report CmdDoneInt after excuting the command
		ULONG       HighByte:8;
		ULONG       LowByte:8;
	}   field;
	ULONG           word;
} H2M_MAILBOX_STRUC, *PH2M_MAILBOX_STRUC;
#else
typedef union  _H2M_MAILBOX_STRUC {
	struct {
		ULONG       LowByte:8;
		ULONG       HighByte:8;
		ULONG       CmdToken:8;
		ULONG       Owner:8;
	}   field;
	ULONG           word;
} H2M_MAILBOX_STRUC, *PH2M_MAILBOX_STRUC;
#endif

//
// M2H_CMD_DONE_CSR: MCU-to-Host command complete indication
//
#ifdef BIG_ENDIAN
typedef union _M2H_CMD_DONE_STRUC {
	struct  {
		ULONG       CmdToken3;
		ULONG       CmdToken2;
		ULONG       CmdToken1;
		ULONG       CmdToken0;
	} field;
	ULONG           word;
} M2H_CMD_DONE_STRUC, *PM2H_CMD_DONE_STRUC;
#else
typedef union _M2H_CMD_DONE_STRUC {
	struct  {
		ULONG       CmdToken0;
		ULONG       CmdToken1;
		ULONG       CmdToken2;
		ULONG       CmdToken3;
	} field;
	ULONG           word;
} M2H_CMD_DONE_STRUC, *PM2H_CMD_DONE_STRUC;
#endif

//
// MCU_INT_SOURCE_CSR, MCU_INT_MASK_CSR: MCU interrupt source/mask register
//
#ifdef BIG_ENDIAN
typedef union _MCU_INT_SOURCE_STRUC {
	struct {
		ULONG       :22;
		ULONG       TBTTExpire:1;
		ULONG       Twakeup:1;
		ULONG       McuInt7:1;
		ULONG       McuInt6:1;
		ULONG       McuInt5:1;
		ULONG       McuInt4:1;
		ULONG       McuInt3:1;
		ULONG       McuInt2:1;
		ULONG       McuInt1:1;
		ULONG       McuInt0:1;
	} field;
	ULONG           word;
} MCU_INT_SOURCE_STRUC, *PMCU_INT_SOURCE_STRUC, MCU_INT_MASK_STRUC, *PMCU_INT_MASK_STRUC;
#else
typedef union _MCU_INT_SOURCE_STRUC {
	struct {
		ULONG       McuInt0:1;
		ULONG       McuInt1:1;
		ULONG       McuInt2:1;
		ULONG       McuInt3:1;
		ULONG       McuInt4:1;
		ULONG       McuInt5:1;
		ULONG       McuInt6:1;
		ULONG       McuInt7:1;
		ULONG       Twakeup:1;
		ULONG       TBTTExpire:1;
		ULONG       :22;
	} field;
	ULONG           word;
} MCU_INT_SOURCE_STRUC, *PMCU_INT_SOURCE_STRUC, MCU_INT_MASK_STRUC, *PMCU_INT_MASK_STRUC;
#endif


//
// MCU_LEDCS: MCU LED Control Setting.
//
#ifdef BIG_ENDIAN
typedef union  _MCU_LEDCS_STRUC {
	struct	{
		USHORT		PolarityRDY_A:1;
		USHORT		PolarityRDY_G:1;
		USHORT		PolarityACT:1;
		USHORT		PolarityGPIO_4:1;
		USHORT		PolarityGPIO_3:1;
		USHORT		PolarityGPIO_2:1;
		USHORT		PolarityGPIO_1:1;
		USHORT		PolarityGPIO_0:1;
		USHORT		LinkAStatus:1;
		USHORT		LinkGStatus:1;
		USHORT		RadioStatus:1;
		USHORT		LedMode:5;
	} field;
	USHORT			word;
} MCU_LEDCS_STRUC, *PMCU_LEDCS_STRUC;
#else
typedef union  _MCU_LEDCS_STRUC {
	struct	{
		USHORT		LedMode:5;
		USHORT		RadioStatus:1;
		USHORT		LinkGStatus:1;
		USHORT		LinkAStatus:1;
		USHORT		PolarityGPIO_0:1;
		USHORT		PolarityGPIO_1:1;
		USHORT		PolarityGPIO_2:1;
		USHORT		PolarityGPIO_3:1;
		USHORT		PolarityGPIO_4:1;
		USHORT		PolarityACT:1;
		USHORT		PolarityRDY_G:1;
		USHORT		PolarityRDY_A:1;
	} field;
	USHORT			word;
} MCU_LEDCS_STRUC, *PMCU_LEDCS_STRUC;
#endif

// =================================================================================
// Register format
// =================================================================================

//
// MAC_CSR1: System control register
//
#ifdef BIG_ENDIAN
typedef	union	_MAC_CSR1_STRUC	{
	struct	{
		ULONG		Rsvd1:29;
		ULONG		HostReady:1;		// Host is ready after initialization, 1: ready
		ULONG		BbpReset:1;			// Hardware reset BBP
		ULONG		SoftReset:1;		// Software reset bit, 1: reset, 0: normal
	}	field;
	ULONG			word;
}	MAC_CSR1_STRUC, *PMAC_CSR1_STRUC;
#else
typedef	union	_MAC_CSR1_STRUC	{
	struct	{
		ULONG		SoftReset:1;		// Software reset bit, 1: reset, 0: normal
		ULONG		BbpReset:1;			// Hardware reset BBP
		ULONG		HostReady:1;		// Host is ready after initialization, 1: ready
		ULONG		Rsvd1:29;
	}	field;
	ULONG			word;
}	MAC_CSR1_STRUC, *PMAC_CSR1_STRUC;
#endif

//
// MAC_CSR2: STA MAC register 0
//
#ifdef BIG_ENDIAN
typedef	union	_MAC_CSR2_STRUC	{
	struct	{
		UCHAR		Byte3;		// MAC address byte 3
		UCHAR		Byte2;		// MAC address byte 2
		UCHAR		Byte1;		// MAC address byte 1
		UCHAR		Byte0;		// MAC address byte 0
	}	field;
	ULONG			word;
}	MAC_CSR2_STRUC, *PMAC_CSR2_STRUC;
#else
typedef	union	_MAC_CSR2_STRUC	{
	struct	{
		UCHAR		Byte0;		// MAC address byte 0
		UCHAR		Byte1;		// MAC address byte 1
		UCHAR		Byte2;		// MAC address byte 2
		UCHAR		Byte3;		// MAC address byte 3
	}	field;
	ULONG			word;
}	MAC_CSR2_STRUC, *PMAC_CSR2_STRUC;
#endif

//
// MAC_CSR3: STA MAC register 1
//
#ifdef BIG_ENDIAN
typedef	union	_MAC_CSR3_STRUC	{
	struct	{
		UCHAR		Rsvd1;
		UCHAR		U2MeMask;
		UCHAR		Byte5;		// MAC address byte 5
		UCHAR		Byte4;		// MAC address byte 4
	}	field;
	ULONG			word;
}	MAC_CSR3_STRUC, *PMAC_CSR3_STRUC;
#else
typedef	union	_MAC_CSR3_STRUC	{
	struct	{
		UCHAR		Byte4;		// MAC address byte 4
		UCHAR		Byte5;		// MAC address byte 5
		UCHAR		U2MeMask;
		UCHAR		Rsvd1;
	}	field;
	ULONG			word;
}	MAC_CSR3_STRUC, *PMAC_CSR3_STRUC;
#endif

//
// MAC_CSR4: BSSID register 0
//
#ifdef BIG_ENDIAN
typedef	union	_MAC_CSR4_STRUC	{
	struct	{
		UCHAR		Byte3;		// BSSID byte 3
		UCHAR		Byte2;		// BSSID byte 2
		UCHAR		Byte1;		// BSSID byte 1
		UCHAR		Byte0;		// BSSID byte 0
	}	field;
	ULONG			word;
}	MAC_CSR4_STRUC, *PMAC_CSR4_STRUC;
#else
typedef	union	_MAC_CSR4_STRUC	{
	struct	{
		UCHAR		Byte0;		// BSSID byte 0
		UCHAR		Byte1;		// BSSID byte 1
		UCHAR		Byte2;		// BSSID byte 2
		UCHAR		Byte3;		// BSSID byte 3
	}	field;
	ULONG			word;
}	MAC_CSR4_STRUC, *PMAC_CSR4_STRUC;
#endif

//
// MAC_CSR5: BSSID register 1
//
#ifdef BIG_ENDIAN
typedef	union	_MAC_CSR5_STRUC	{
	struct	{
		USHORT		Rsvd:14;
		USHORT		BssIdMask:2; // 11: one BSSID, 00: 4 BSSID, 10 or 01: 2 BSSID
		UCHAR		Byte5;		 // BSSID byte 5
		UCHAR		Byte4;		 // BSSID byte 4
	}	field;
	ULONG			word;
}	MAC_CSR5_STRUC, *PMAC_CSR5_STRUC;
#else
typedef	union	_MAC_CSR5_STRUC	{
	struct	{
		UCHAR		Byte4;		 // BSSID byte 4
		UCHAR		Byte5;		 // BSSID byte 5
		USHORT      BssIdMask:2; // 11: one BSSID, 00: 4 BSSID, 10 or 01: 2 BSSID
		USHORT		Rsvd:14;
	}	field;
	ULONG			word;
}	MAC_CSR5_STRUC, *PMAC_CSR5_STRUC;
#endif

//
// MAC_CSR8: SIFS/EIFS register
#ifdef BIG_ENDIAN
typedef	union	_MAC_CSR8_STRUC	{
	struct	{
		USHORT		Eifs;               // in unit of 1-us
		UCHAR       SifsAfterRxOfdm;    // in unit of 1-us
		UCHAR		Sifs;               // in unit of 1-us
	}	field;
	ULONG			word;
}	MAC_CSR8_STRUC, *PMAC_CSR8_STRUC;
#else
typedef	union	_MAC_CSR8_STRUC	{
	struct	{
		UCHAR		Sifs;               // in unit of 1-us
		UCHAR       SifsAfterRxOfdm;    // in unit of 1-us
		USHORT		Eifs;               // in unit of 1-us
	}	field;
	ULONG			word;
}	MAC_CSR8_STRUC, *PMAC_CSR8_STRUC;
#endif

//
// MAC_CSR9: Back-Off control register
//
#ifdef BIG_ENDIAN
typedef	union	_MAC_CSR9_STRUC	{
	struct {
		ULONG		Rsvd:15;
		ULONG       CWSelect:1;     // 1: CWmin/Cwmax select from register, 0:select from TxD
		ULONG		CWMax:4;		// Bit for Cwmax, default Cwmax is 1023 (2^10 - 1).
		ULONG		CWMin:4;		// Bit for Cwmin. default Cwmin is 31 (2^5 - 1).
		ULONG		SlotTime:8;		// Slot time, default is 20us for 802.11B
	}	field;
	ULONG			word;
}	MAC_CSR9_STRUC, *PMAC_CSR9_STRUC;
#else
typedef	union	_MAC_CSR9_STRUC	{
	struct {
		ULONG		SlotTime:8;		// Slot time, default is 20us for 802.11B
		ULONG		CWMin:4;		// Bit for Cwmin. default Cwmin is 31 (2^5 - 1).
		ULONG		CWMax:4;		// Bit for Cwmax, default Cwmax is 1023 (2^10 - 1).
		ULONG       CWSelect:1;     // 1: CWmin/Cwmax select from register, 0:select from TxD
		ULONG		Rsvd:15;
	}	field;
	ULONG			word;
}	MAC_CSR9_STRUC, *PMAC_CSR9_STRUC;
#endif

//
// MAC_CSR11: Power saving transition time register
//
#ifdef BIG_ENDIAN
typedef union _MAC_CSR11_STRUC {
	struct {
		ULONG       :12;
		ULONG       Sleep2AwakeLatency:4;              // in unit of 1-TU
		ULONG       bAutoWakeupEnable:1;
		ULONG		NumOfTBTTBeforeWakeup:7;           // Number of beacon before wakeup
		ULONG		DelayAfterLastTBTTBeforeWakeup:8;  // Delay after Tbcn expired in units of 1-TU
	} field;
	ULONG   word;
} MAC_CSR11_STRUC, *PMAC_CSR11_STRUC;
#else
typedef union _MAC_CSR11_STRUC {
	struct {
		ULONG		DelayAfterLastTBTTBeforeWakeup:8;  // Delay after Tbcn expired in units of 1-TU
		ULONG		NumOfTBTTBeforeWakeup:7;           // Number of beacon before wakeup
		ULONG       bAutoWakeupEnable:1;
		ULONG       Sleep2AwakeLatency:4;              // in unit of 1-TU
		ULONG       :12;
	} field;
	ULONG   word;
} MAC_CSR11_STRUC, *PMAC_CSR11_STRUC;
#endif

//
// MAC_CSR12: Manual power control / status register (merge CSR20 & PWRCSR1)
//
#ifdef BIG_ENDIAN
typedef	union	_MAC_CSR12_STRUC	{
	struct	{
		ULONG		:28;
		ULONG		BbpRfStatus:1;			// 0: not ready, 1:ready
		ULONG       ForceWakeup:1;          // ForceWake has high privilege than PutToSleep when both set
		ULONG       PutToSleep:1;
		ULONG		CurrentPowerState:1;	// 0:sleep, 1:awake
	}	field;
	ULONG			word;
}	MAC_CSR12_STRUC, *PMAC_CSR12_STRUC;
#else
typedef	union	_MAC_CSR12_STRUC	{
	struct	{
		ULONG		CurrentPowerState:1;	// 0:sleep, 1:awake
		ULONG       PutToSleep:1;
		ULONG       ForceWakeup:1;          // ForceWake has high privilege than PutToSleep when both set
		ULONG		BbpRfStatus:1;			// 0: not ready, 1:ready
		ULONG		:28;
	}	field;
	ULONG			word;
}	MAC_CSR12_STRUC, *PMAC_CSR12_STRUC;
#endif

//
// MAC_CSR14: LED control register
//
#ifdef BIG_ENDIAN
typedef	union	_MAC_CSR14_STRUC	{
	struct	{
		ULONG		:12;
		ULONG       SwLed2:1;
		ULONG       HwLedPolarity:1;    // 0: active low, 1: active high
		ULONG		SwLed1:1;		    // s/w LED, 1: ON, 0: OFF
		ULONG		HwLed:1;			// h/w TX activity, 1: normal OFF, blinking upon TX, 0: normal ON, blinking upon TX
		ULONG		OffPeriod:8;		// Off period in unit of 1-ms, default 30ms
		ULONG		OnPeriod:8;			// On period in unit of 1-ms, default 70ms
	}	field;
	ULONG			word;
}	MAC_CSR14_STRUC, *PMAC_CSR14_STRUC;
#else
typedef	union	_MAC_CSR14_STRUC	{
	struct	{
		ULONG		OnPeriod:8;			// On period, default 70ms
		ULONG		OffPeriod:8;		// Off period, default 30ms
		ULONG		HwLed:1;			// h/w TX activity, 1: normal OFF, blinking upon TX, 0: normal ON, blinking upon TX
		ULONG		SwLed1:1;		    // s/w LED, 1: ON, 0: OFF
		ULONG       HwLedPolarity:1;    // 0: active low, 1: active high
		ULONG       SwLed2:1;
		ULONG		:12;
	}	field;
	ULONG			word;
}	MAC_CSR14_STRUC, *PMAC_CSR14_STRUC;
#endif

//
// TXRX_CSR0: TX/RX configuration register
//
#ifdef BIG_ENDIAN
typedef	union	TXRX_CSR0_STRUC	{
	struct	{
		ULONG		:5;
		ULONG       TxWithoutWaitingSBox:1;
		ULONG       DropAckCts:1;       // 1: drop received ACK and CTS
		ULONG		DropBcast:1;		// Drop broadcast frames
		ULONG		DropMcast:1;		// Drop multicast frames
		ULONG		DropVerErr:1;	    // Drop version error frame
		ULONG		DropToDs:1;			// Drop fram ToDs bit is true
		ULONG		DropNotToMe:1;		// Drop not to me unicast frame
		ULONG		DropControl:1;		// Drop control frame
		ULONG		DropPhyErr:1;		// Drop physical error
		ULONG		DropCRCErr:1;		// Drop CRC error
		ULONG		DisableRx:1;		// Disable Rx engine
		ULONG       AutoTxSeq:1;        // 1: ASIC auto replace sequence# in outgoing frame
		ULONG       TsfOffset:6;        // default is 24
		ULONG       RxAckTimeout:9;
	}	field;
	ULONG			word;
}	TXRX_CSR0_STRUC, *PTXRX_CSR0_STRUC;
#else
typedef	union	_TXRX_CSR0_STRUC	{
	struct	{
		ULONG       RxAckTimeout:9;
		ULONG       TsfOffset:6;        // default is 24
		ULONG       AutoTxSeq:1;        // 1: ASIC auto replace sequence# in outgoing frame
		ULONG		DisableRx:1;		// Disable Rx engine
		ULONG		DropCRCErr:1;		// Drop CRC error
		ULONG		DropPhyErr:1;		// Drop physical error
		ULONG		DropControl:1;		// Drop control frame
		ULONG		DropNotToMe:1;		// Drop not to me unicast frame
		ULONG		DropToDs:1;			// Drop fram ToDs bit is true
		ULONG		DropVerErr:1;	    // Drop version error frame
		ULONG		DropMcast:1;		// Drop multicast frames
		ULONG		DropBcast:1;		// Drop broadcast frames
		ULONG       DropAckCts:1;       // 1: drop received ACK and CTS
		ULONG       TxWithoutWaitingSBox:1;
		ULONG		:5;
	}	field;
	ULONG			word;
}	TXRX_CSR0_STRUC, *PTXRX_CSR0_STRUC;
#endif

//
// TXRX_CSR4: Auto-Responder/Tx-retry register
//
#ifdef BIG_ENDIAN
typedef	union	_TXRX_CSR4_STRUC	{
	struct	{
		ULONG       ShortRetryLimit:4;
		ULONG       LongRetryLimit:4;
		ULONG		Rsv1:1;
		ULONG		OfdmTxFallbacktoCCK:1;      // 0: Fallbackt o OFDM 6M oly, 1: Fallback to CCK 1M,2M
		ULONG       OfdmTxRateDownStep:2;       // 0:1-step, 1: 2-step, 2:3-step, 3:4-step
		ULONG       OfdmTxRateDownEnable:1;     // 1:enable
		ULONG       AutoResponderPreamble:1;    // 0:long, 1:short preamble
		ULONG       AutoResponderEnable:1;
		ULONG       AckCtsPsmBit:1;
		ULONG       Rsv2:5;
		ULONG       CntlFrameAckPolicy:3;
		ULONG       TxAckTimeout:8;
	}	field;
	ULONG			word;
}	TXRX_CSR4_STRUC, *PTXRX_CSR4_STRUC;
#else
typedef	union	_TXRX_CSR4_STRUC	{
	struct	{
		ULONG       TxAckTimeout:8;
		ULONG       CntlFrameAckPolicy:3;
		ULONG       Rsv2:5;
		ULONG       AckCtsPsmBit:1;
		ULONG       AutoResponderEnable:1;
		ULONG       AutoResponderPreamble:1;    // 0:long, 1:short preamble
		ULONG       OfdmTxRateDownEnable:1;     // 1:enable
		ULONG       OfdmTxRateDownStep:2;       // 0:1-step, 1: 2-step, 2:3-step, 3:4-step
		ULONG		OfdmTxFallbacktoCCK:1;      // 0: Fallbackt o OFDM 6M oly, 1: Fallback to CCK 1M,2M
		ULONG		Rsv1:1;
		ULONG       LongRetryLimit:4;
		ULONG       ShortRetryLimit:4;
	}	field;
	ULONG			word;
}	TXRX_CSR4_STRUC, *PTXRX_CSR4_STRUC;
#endif

//
// TXRX_CSR9: Synchronization control register
//
#ifdef BIG_ENDIAN
typedef	union	_TXRX_CSR9_STRUC	{
	struct	{
		ULONG		TxTimestampCompensate:8;
		ULONG       :3;
		ULONG		bBeaconGen:1;		// Enable beacon generator
		ULONG       bTBTTEnable:1;
		ULONG		TsfSyncMode:2;		// Enable TSF sync, 00: disable, 01: infra mode, 10: ad-hoc mode
		ULONG		bTsfTicking:1;		// Enable TSF auto counting
		ULONG       BeaconInterval:16;  // in unit of 1/16 TU
	}	field;
	ULONG			word;
}	TXRX_CSR9_STRUC, *PTXRX_CSR9_STRUC;
#else
typedef	union	_TXRX_CSR9_STRUC	{
	struct	{
		ULONG       BeaconInterval:16;  // in unit of 1/16 TU
		ULONG		bTsfTicking:1;		// Enable TSF auto counting
		ULONG		TsfSyncMode:2;		// Enable TSF sync, 00: disable, 01: infra mode, 10: ad-hoc mode
		ULONG       bTBTTEnable:1;
		ULONG		bBeaconGen:1;		// Enable beacon generator
		ULONG       :3;
		ULONG		TxTimestampCompensate:8;
	}	field;
	ULONG			word;
}	TXRX_CSR9_STRUC, *PTXRX_CSR9_STRUC;
#endif

//
// PHY_CSR3: BBP serial control register
//
#ifdef BIG_ENDIAN
typedef	union	_PHY_CSR3_STRUC	{
	struct	{
		ULONG		:15;
		ULONG		Busy:1;				// 1: ASIC is busy execute BBP programming.
		ULONG		fRead:1;		    // 0: Write	BBP, 1:	Read BBP
		ULONG		RegNum:7;			// Selected	BBP	register
		ULONG		Value:8;			// Register	value to program into BBP
	}	field;
	ULONG			word;
}	PHY_CSR3_STRUC, *PPHY_CSR3_STRUC;
#else
typedef	union	_PHY_CSR3_STRUC	{
	struct	{
		ULONG		Value:8;			// Register	value to program into BBP
		ULONG		RegNum:7;			// Selected	BBP	register
		ULONG		fRead:1;		    // 0: Write	BBP, 1:	Read BBP
		ULONG		Busy:1;				// 1: ASIC is busy execute BBP programming.
		ULONG		:15;
	}	field;
	ULONG			word;
}	PHY_CSR3_STRUC, *PPHY_CSR3_STRUC;
#endif

//
// PHY_CSR4: RF serial control register
//
#ifdef BIG_ENDIAN
typedef	union	_PHY_CSR4_STRUC	{
	struct	{
		ULONG		Busy:1;				// 1: ASIC is busy execute RF programming.
		ULONG		PLL_LD:1;			// RF PLL_LD status
		ULONG		IFSelect:1;			// 1: select IF	to program,	0: select RF to	program
		ULONG		NumberOfBits:5;		// Number of bits used in RFRegValue (I:20,	RFMD:22)
		ULONG		RFRegValue:24;		// Register	value (include register	id)	serial out to RF/IF	chip.
	}	field;
	ULONG			word;
}	PHY_CSR4_STRUC, *PPHY_CSR4_STRUC;
#else
typedef	union	_PHY_CSR4_STRUC	{
	struct	{
		ULONG		RFRegValue:24;		// Register	value (include register	id)	serial out to RF/IF	chip.
		ULONG		NumberOfBits:5;		// Number of bits used in RFRegValue (I:20,	RFMD:22)
		ULONG		IFSelect:1;			// 1: select IF	to program,	0: select RF to	program
		ULONG		PLL_LD:1;			// RF PLL_LD status
		ULONG		Busy:1;				// 1: ASIC is busy execute RF programming.
	}	field;
	ULONG			word;
}	PHY_CSR4_STRUC, *PPHY_CSR4_STRUC;
#endif

//
// SEC_CSR1: shared key table security mode register
//
#ifdef BIG_ENDIAN
typedef	union	_SEC_CSR1_STRUC	{
	struct	{
		ULONG       :1;
		ULONG       Bss1Key3CipherAlg:3;
		ULONG       :1;
		ULONG       Bss1Key2CipherAlg:3;
		ULONG       :1;
		ULONG       Bss1Key1CipherAlg:3;
		ULONG       :1;
		ULONG       Bss1Key0CipherAlg:3;
		ULONG       :1;
		ULONG       Bss0Key3CipherAlg:3;
		ULONG       :1;
		ULONG       Bss0Key2CipherAlg:3;
		ULONG       :1;
		ULONG       Bss0Key1CipherAlg:3;
		ULONG       :1;
		ULONG       Bss0Key0CipherAlg:3;
	}	field;
	ULONG			word;
}	SEC_CSR1_STRUC, *PSEC_CSR1_STRUC;
#else
typedef	union	_SEC_CSR1_STRUC	{
	struct	{
		ULONG       Bss0Key0CipherAlg:3;
		ULONG       :1;
		ULONG       Bss0Key1CipherAlg:3;
		ULONG       :1;
		ULONG       Bss0Key2CipherAlg:3;
		ULONG       :1;
		ULONG       Bss0Key3CipherAlg:3;
		ULONG       :1;
		ULONG       Bss1Key0CipherAlg:3;
		ULONG       :1;
		ULONG       Bss1Key1CipherAlg:3;
		ULONG       :1;
		ULONG       Bss1Key2CipherAlg:3;
		ULONG       :1;
		ULONG       Bss1Key3CipherAlg:3;
		ULONG       :1;
	}	field;
	ULONG			word;
}	SEC_CSR1_STRUC, *PSEC_CSR1_STRUC;
#endif

//
// SEC_CSR5: shared key table security mode register
//
#ifdef BIG_ENDIAN
typedef	union	_SEC_CSR5_STRUC	{
	struct	{
		ULONG       :1;
		ULONG       Bss3Key3CipherAlg:3;
		ULONG       :1;
		ULONG       Bss3Key2CipherAlg:3;
		ULONG       :1;
		ULONG       Bss3Key1CipherAlg:3;
		ULONG       :1;
		ULONG       Bss3Key0CipherAlg:3;
		ULONG       :1;
		ULONG       Bss2Key3CipherAlg:3;
		ULONG       :1;
		ULONG       Bss2Key2CipherAlg:3;
		ULONG       :1;
		ULONG       Bss2Key1CipherAlg:3;
		ULONG       :1;
		ULONG       Bss2Key0CipherAlg:3;
	}	field;
	ULONG			word;
}	SEC_CSR5_STRUC, *PSEC_CSR5_STRUC;
#else
typedef	union	_SEC_CSR5_STRUC	{
	struct	{
		ULONG       Bss2Key0CipherAlg:3;
		ULONG       :1;
		ULONG       Bss2Key1CipherAlg:3;
		ULONG       :1;
		ULONG       Bss2Key2CipherAlg:3;
		ULONG       :1;
		ULONG       Bss2Key3CipherAlg:3;
		ULONG       :1;
		ULONG       Bss3Key0CipherAlg:3;
		ULONG       :1;
		ULONG       Bss3Key1CipherAlg:3;
		ULONG       :1;
		ULONG       Bss3Key2CipherAlg:3;
		ULONG       :1;
		ULONG       Bss3Key3CipherAlg:3;
		ULONG       :1;
	}	field;
	ULONG			word;
}	SEC_CSR5_STRUC, *PSEC_CSR5_STRUC;
#endif

//
// STA_CSR0: RX PLCP error count & RX CRC error count
//
#ifdef BIG_ENDIAN
typedef	union	_STA_CSR0_STRUC	{
	struct	{
		USHORT  PlcpErr;
		USHORT  CrcErr;
	}	field;
	ULONG			word;
}	STA_CSR0_STRUC, *PSTA_CSR0_STRUC;
#else
typedef	union	_STA_CSR0_STRUC	{
	struct	{
		USHORT  CrcErr;
		USHORT  PlcpErr;
	}	field;
	ULONG			word;
}	STA_CSR0_STRUC, *PSTA_CSR0_STRUC;
#endif

//
// STA_CSR1: RX False CCA count & RX LONG frame count
//
#ifdef BIG_ENDIAN
typedef	union	_STA_CSR1_STRUC	{
	struct	{
		USHORT  FalseCca;
		USHORT  PhyErr;
	}	field;
	ULONG			word;
}	STA_CSR1_STRUC, *PSTA_CSR1_STRUC;
#else
typedef	union	_STA_CSR1_STRUC	{
	struct	{
		USHORT  PhyErr;
		USHORT  FalseCca;
	}	field;
	ULONG			word;
}	STA_CSR1_STRUC, *PSTA_CSR1_STRUC;
#endif

//
// STA_CSR2: TX Beacon count and RX FIFO overflow count
//
#ifdef BIG_ENDIAN
typedef	union	_STA_CSR2_STRUC	{
	struct	{
		USHORT  RxOverflowCount;
		USHORT  RxFifoOverflowCount;
	}	field;
	ULONG			word;
}	STA_CSR2_STRUC, *PSTA_CSR2_STRUC;
#else
typedef	union	_STA_CSR2_STRUC	{
	struct	{
		USHORT  RxFifoOverflowCount;
		USHORT  RxOverflowCount;
	}	field;
	ULONG			word;
}	STA_CSR2_STRUC, *PSTA_CSR2_STRUC;
#endif

//
// STA_CSR3: TX Beacon count
//
#ifdef BIG_ENDIAN
typedef	union	_STA_CSR3_STRUC	{
	struct	{
		USHORT  Rsv;
		USHORT  TxBeaconCount;
	}	field;
	ULONG			word;
}	STA_CSR3_STRUC, *PSTA_CSR3_STRUC;
#else
typedef	union	_STA_CSR3_STRUC	{
	struct	{
		USHORT  TxBeaconCount;
		USHORT  Rsv;
	}	field;
	ULONG			word;
}	STA_CSR3_STRUC, *PSTA_CSR3_STRUC;
#endif

//
// STA_CSR4: TX Retry count
//
#ifdef BIG_ENDIAN
typedef	union	_STA_CSR4_STRUC	{
	struct	{
		USHORT  TxOneRetryCount;
		USHORT  TxNoRetryCount;
	}	field;
	ULONG			word;
}	STA_CSR4_STRUC, *PSTA_CSR4_STRUC;
#else
typedef	union	_STA_CSR4_STRUC	{
	struct	{
		USHORT  TxNoRetryCount;
		USHORT  TxOneRetryCount;
	}	field;
	ULONG			word;
}	STA_CSR4_STRUC, *PSTA_CSR4_STRUC;
#endif


//
// STA_CSR5: TX Retry count
//
#ifdef BIG_ENDIAN
typedef	union	_STA_CSR5_STRUC	{
	struct	{
		USHORT  TxRetryFailCount;
		USHORT  TxMultiRetryCount;
	}	field;
	ULONG			word;
}	STA_CSR5_STRUC, *PSTA_CSR5_STRUC;
#else
typedef	union	_STA_CSR5_STRUC	{
	struct	{
		USHORT  TxMultiRetryCount;
		USHORT  TxRetryFailCount;
	}	field;
	ULONG			word;
}	STA_CSR5_STRUC, *PSTA_CSR5_STRUC;
#endif

//
// HOST_CMD_CSR: For HOST to interrupt embedded processor
//
#ifdef BIG_ENDIAN
typedef	union	_HOST_CMD_CSR_STRUC	{
	struct	{
		ULONG   Rsv:24;
		ULONG   InterruptMcu:1;
		ULONG   HostCommand:7;
	}	field;
	ULONG			word;
}	HOST_CMD_CSR_STRUC, *PHOST_CMD_CSR_STRUC;
#else
typedef	union	_HOST_CMD_CSR_STRUC	{
	struct	{
		ULONG   HostCommand:7;
		ULONG   InterruptMcu:1;
		ULONG   Rsv:24;
	}	field;
	ULONG			word;
}	HOST_CMD_CSR_STRUC, *PHOST_CMD_CSR_STRUC;
#endif

//
// TX_RING_CSR0: TX Ring size for AC_BK, AC_BE, AC_VI, AC_VO
//
#ifdef BIG_ENDIAN
typedef	union	_TX_RING_CSR0_STRUC	{
	struct	{
		UCHAR   Ac3Total;
		UCHAR   Ac2Total;
		UCHAR   Ac1Total;
		UCHAR   Ac0Total;
	}	field;
	ULONG			word;
}	TX_RING_CSR0_STRUC, *PTX_RING_CSR0_STRUC;
#else
typedef	union	_TX_RING_CSR0_STRUC	{
	struct	{
		UCHAR   Ac0Total;
		UCHAR   Ac1Total;
		UCHAR   Ac2Total;
		UCHAR   Ac3Total;
	}	field;
	ULONG			word;
}	TX_RING_CSR0_STRUC, *PTX_RING_CSR0_STRUC;
#endif

//
// TX_RING_CSR1: TX Ring size for MGMT Ring, HCCA Ring
//
#ifdef BIG_ENDIAN
typedef	union	_TX_RING_CSR1_STRUC	{
	struct	{
		ULONG   Rsv:10;
		ULONG   TxdSize:6;      // in unit of 32-bit
		ULONG   HccaTotal:8;
		ULONG   MgmtTotal:8;
	}	field;
	ULONG			word;
}	TX_RING_CSR1_STRUC, *PTX_RING_CSR1_STRUC;
#else
typedef	union	_TX_RING_CSR1_STRUC	{
	struct	{
		ULONG   MgmtTotal:8;
		ULONG   HccaTotal:8;
		ULONG   TxdSize:6;      // in unit of 32-bit
		ULONG   Rsv:10;
	}	field;
	ULONG			word;
}	TX_RING_CSR1_STRUC, *PTX_RING_CSR1_STRUC;
#endif

//
// AIFSN_CSR: AIFSN for each EDCA AC
//
#ifdef BIG_ENDIAN
typedef	union	_AIFSN_CSR_STRUC	{
	struct	{
		ULONG   Rsv:16;
		ULONG   Aifsn3:4;       // for AC_VO
		ULONG   Aifsn2:4;       // for AC_VI
		ULONG   Aifsn1:4;       // for AC_BK
		ULONG   Aifsn0:4;       // for AC_BE
	}	field;
	ULONG			word;
}	AIFSN_CSR_STRUC, *PAIFSN_CSR_STRUC;
#else
typedef	union	_AIFSN_CSR_STRUC	{
	struct	{
		ULONG   Aifsn0:4;       // for AC_BE
		ULONG   Aifsn1:4;       // for AC_BK
		ULONG   Aifsn2:4;       // for AC_VI
		ULONG   Aifsn3:4;       // for AC_VO
		ULONG   Rsv:16;
	}	field;
	ULONG			word;
}	AIFSN_CSR_STRUC, *PAIFSN_CSR_STRUC;
#endif

//
// CWMIN_CSR: CWmin for each EDCA AC
//
#ifdef BIG_ENDIAN
typedef	union	_CWMIN_CSR_STRUC	{
	struct	{
		ULONG   Rsv:16;
		ULONG   Cwmin3:4;       // for AC_VO
		ULONG   Cwmin2:4;       // for AC_VI
		ULONG   Cwmin1:4;       // for AC_BK
		ULONG   Cwmin0:4;       // for AC_BE
	}	field;
	ULONG			word;
}	CWMIN_CSR_STRUC, *PCWMIN_CSR_STRUC;
#else
typedef	union	_CWMIN_CSR_STRUC	{
	struct	{
		ULONG   Cwmin0:4;       // for AC_BE
		ULONG   Cwmin1:4;       // for AC_BK
		ULONG   Cwmin2:4;       // for AC_VI
		ULONG   Cwmin3:4;       // for AC_VO
		ULONG   Rsv:16;
	}	field;
	ULONG			word;
}	CWMIN_CSR_STRUC, *PCWMIN_CSR_STRUC;
#endif

//
// CWMAX_CSR: CWmin for each EDCA AC
//
#ifdef BIG_ENDIAN
typedef	union	_CWMAX_CSR_STRUC	{
	struct	{
		ULONG   Rsv:16;
		ULONG   Cwmax3:4;       // for AC_VO
		ULONG   Cwmax2:4;       // for AC_VI
		ULONG   Cwmax1:4;       // for AC_BK
		ULONG   Cwmax0:4;       // for AC_BE
	}	field;
	ULONG			word;
}	CWMAX_CSR_STRUC, *PCWMAX_CSR_STRUC;
#else
typedef	union	_CWMAX_CSR_STRUC	{
	struct	{
		ULONG   Cwmax0:4;       // for AC_BE
		ULONG   Cwmax1:4;       // for AC_BK
		ULONG   Cwmax2:4;       // for AC_VI
		ULONG   Cwmax3:4;       // for AC_VO
		ULONG   Rsv:16;
	}	field;
	ULONG			word;
}	CWMAX_CSR_STRUC, *PCWMAX_CSR_STRUC;
#endif

//
// TX_CNTL_CSR: KICK/Abort TX
//
#ifdef BIG_ENDIAN
typedef	union	_TX_CNTL_CSR_STRUC	{
	struct	{
		ULONG   Rsv1:11;
		ULONG   AbortTxMgmt:1;
		ULONG   AbortTxAc3:1;       // for AC_VO
		ULONG   AbortTxAc2:1;       // for AC_VI
		ULONG   AbortTxAc1:1;       // for AC_BE
		ULONG   AbortTxAc0:1;       // for AC_BK
		ULONG   Rsv2:11;
		ULONG   KickTxMgmt:1;
		ULONG   KickTxAc3:1;       // for AC_VO
		ULONG   KickTxAc2:1;       // for AC_VI
		ULONG   KickTxAc1:1;       // for AC_BE
		ULONG   KickTxAc0:1;       // for AC_BK
	}	field;
	ULONG			word;
}	TX_CNTL_CSR_STRUC, *PTX_CNTL_CSR_STRUC;
#else
typedef	union	_TX_CNTL_CSR_STRUC	{
	struct	{
		ULONG   KickTxAc0:1;       // for AC_BK
		ULONG   KickTxAc1:1;       // for AC_BE
		ULONG   KickTxAc2:1;       // for AC_VI
		ULONG   KickTxAc3:1;       // for AC_VO
		ULONG   KickTxMgmt:1;
		ULONG   Rsv2:11;
		ULONG   AbortTxAc0:1;       // for AC_BK
		ULONG   AbortTxAc1:1;       // for AC_BE
		ULONG   AbortTxAc2:1;       // for AC_VI
		ULONG   AbortTxAc3:1;       // for AC_VO
		ULONG   AbortTxMgmt:1;
		ULONG   Rsv1:11;
	}	field;
	ULONG			word;
}	TX_CNTL_CSR_STRUC, *PTX_CNTL_CSR_STRUC;
#endif

//
// CWMAX_CSR: CWmin for each EDCA AC
//
#ifdef BIG_ENDIAN
typedef	union	_RX_RING_CSR_STRUC	{
	struct	{
		ULONG   Rsv:13;
		ULONG   RxdWritebackSize:3;
		ULONG   :2;
		ULONG   RxdSize:6;      // in unit of 32-bit
		ULONG   RxRingTotal:8;
	}	field;
	ULONG			word;
}	RX_RING_CSR_STRUC, *PRX_RING_CSR_STRUC;
#else
typedef	union	_RX_RING_CSR_STRUC	{
	struct	{
		ULONG   RxRingTotal:8;
		ULONG   RxdSize:6;      // in unit of 32-bit
		ULONG   :2;
		ULONG   RxdWritebackSize:3;
		ULONG   Rsv:13;
	}	field;
	ULONG			word;
}	RX_RING_CSR_STRUC, *PRX_RING_CSR_STRUC;
#endif

//
// INT_SOURCE_CSR: Interrupt source register. Write one to clear corresponding bit
//
#ifdef BIG_ENDIAN
typedef	union	_INT_SOURCE_CSR_STRUC	{
	struct	{
		ULONG       :10;
		ULONG       HccaDmaDone:1;
		ULONG       MgmtDmaDone:1;
		ULONG       Ac3DmaDone:1;
		ULONG       Ac2DmaDone:1;
		ULONG       Ac1DmaDone:1;
		ULONG       Ac0DmaDone:1;
		ULONG		:11;
		ULONG       TxAbortDone:1;
		ULONG       :1;
		ULONG       BeaconTxDone:1;
		ULONG		RxDone:1;
		ULONG		TxDone:1;
	}	field;
	ULONG			word;
}	INT_SOURCE_CSR_STRUC, *PINT_SOURCE_CSR_STRUC;
#else
typedef	union	_INT_SOURCE_CSR_STRUC	{
	struct	{
		ULONG		TxDone:1;
		ULONG		RxDone:1;
		ULONG       BeaconTxDone:1;
		ULONG       :1;
		ULONG       TxAbortDone:1;
		ULONG		:11;
		ULONG       Ac0DmaDone:1;
		ULONG       Ac1DmaDone:1;
		ULONG       Ac2DmaDone:1;
		ULONG       Ac3DmaDone:1;
		ULONG       MgmtDmaDone:1;
		ULONG       HccaDmaDone:1;
		ULONG       :10;
	}	field;
	ULONG			word;
} INT_SOURCE_CSR_STRUC, *PINT_SOURCE_CSR_STRUC;
#endif

//
// INT_MASK_CSR:   Interrupt MASK register.   1: the interrupt is mask OFF
//
#ifdef BIG_ENDIAN
typedef	union	_INT_MASK_CSR_STRUC	{
	struct	{
		ULONG       :10;
		ULONG       HccaDmaDone:1;
		ULONG       MgmtDmaDone:1;
		ULONG       Ac3DmaDone:1;
		ULONG       Ac2DmaDone:1;
		ULONG       Ac1DmaDone:1;
		ULONG       Ac0DmaDone:1;
		ULONG       MitigationPeriod:8; // interrupt mitigation in unit of 32 PCI clock
		ULONG       bEnableMitigationPeriod:1;
		ULONG		:2;
		ULONG       TxAbortDone:1;
		ULONG       :1;
		ULONG       BeaconTxDone:1;
		ULONG		RxDone:1;
		ULONG		TxDone:1;
	}	field;
	ULONG			word;
}INT_MASK_CSR_STRUC, *PINT_MASK_CSR_STRUC;
#else
typedef	union	_INT_MASK_CSR_STRUC	{
	struct	{
		ULONG		TxDone:1;
		ULONG		RxDone:1;
		ULONG       BeaconTxDone:1;
		ULONG       :1;
		ULONG       TxAbortDone:1;
		ULONG		:2;
		ULONG       bEnableMitigationPeriod:1;
		ULONG       MitigationPeriod:8; // interrupt mitigation in unit of 32 PCI clock
		ULONG       Ac0DmaDone:1;
		ULONG       Ac1DmaDone:1;
		ULONG       Ac2DmaDone:1;
		ULONG       Ac3DmaDone:1;
		ULONG       MgmtDmaDone:1;
		ULONG       HccaDmaDone:1;
		ULONG       :10;
	}	field;
	ULONG			word;
} INT_MASK_CSR_STRUC, *PINT_MASK_CSR_STRUC;
#endif
//
// E2PROM_CSR: EEPROM control register
//
#ifdef BIG_ENDIAN
typedef	union	_E2PROM_CSR_STRUC	{
	struct	{
		ULONG		Rsvd:25;
		ULONG       LoadStatus:1;   // 1:loading, 0:done
		ULONG		Type:1;			// 1: 93C46, 0:93C66
		ULONG		EepromDO:1;
		ULONG		EepromDI:1;
		ULONG		EepromCS:1;
		ULONG		EepromSK:1;
		ULONG		Reload:1;		// Reload EEPROM content, write one to reload, self-cleared.
	}	field;
	ULONG			word;
}	E2PROM_CSR_STRUC, *PE2PROM_CSR_STRUC;
#else
typedef	union	_E2PROM_CSR_STRUC	{
	struct	{
		ULONG		Reload:1;		// Reload EEPROM content, write one to reload, self-cleared.
		ULONG		EepromSK:1;
		ULONG		EepromCS:1;
		ULONG		EepromDI:1;
		ULONG		EepromDO:1;
		ULONG		Type:1;			// 1: 93C46, 0:93C66
		ULONG       LoadStatus:1;   // 1:loading, 0:done
		ULONG		Rsvd:25;
	}	field;
	ULONG			word;
}	E2PROM_CSR_STRUC, *PE2PROM_CSR_STRUC;
#endif

//
// AC_TXOP_CSR0: AC_BK/AC_BE TXOP register
//
#ifdef BIG_ENDIAN
typedef	union	_AC_TXOP_CSR0_STRUC	{
	struct	{
		USHORT  Ac1Txop;        // for AC_BE, in unit of 32us
		USHORT  Ac0Txop;        // for AC_BK, in unit of 32us
	}	field;
	ULONG			word;
}	AC_TXOP_CSR0_STRUC, *PAC_TXOP_CSR0_STRUC;
#else
typedef	union	_AC_TXOP_CSR0_STRUC	{
	struct	{
		USHORT  Ac0Txop;        // for AC_BK, in unit of 32us
		USHORT  Ac1Txop;        // for AC_BE, in unit of 32us
	}	field;
	ULONG			word;
}	AC_TXOP_CSR0_STRUC, *PAC_TXOP_CSR0_STRUC;
#endif

//
// AC_TXOP_CSR1: AC_VO/AC_VI TXOP register
//
#ifdef BIG_ENDIAN
typedef	union	_AC_TXOP_CSR1_STRUC	{
	struct	{
		USHORT  Ac3Txop;        // for AC_VO, in unit of 32us
		USHORT  Ac2Txop;        // for AC_VI, in unit of 32us
	}	field;
	ULONG			word;
}	AC_TXOP_CSR1_STRUC, *PAC_TXOP_CSR1_STRUC;
#else
typedef	union	_AC_TXOP_CSR1_STRUC	{
	struct	{
		USHORT  Ac2Txop;        // for AC_VI, in unit of 32us
		USHORT  Ac3Txop;        // for AC_VO, in unit of 32us
	}	field;
	ULONG			word;
}	AC_TXOP_CSR1_STRUC, *PAC_TXOP_CSR1_STRUC;
#endif


// -------------------------------------------------------------------
//  E2PROM data layout
// -------------------------------------------------------------------

//
// EEPROM antenna select format
//
#ifdef BIG_ENDIAN
typedef	union	_EEPROM_ANTENNA_STRUC	{
	struct	{
		USHORT      RfIcType:5;             // see E2PROM document
		USHORT		HardwareRadioControl:1;	// 1: Hardware controlled radio enabled, Read GPIO0 required.
		USHORT      DynamicTxAgcControl:1;
		USHORT		Rsv:2;
		USHORT		FrameType:1;			// 0: DPDT , 1: SPDT , noted this bit is valid for g only.
		USHORT		RxDefaultAntenna:2;		// default of antenna, 0: diversity, 1:antenna-A, 2:antenna-B reserved (default = 0)
		USHORT		TxDefaultAntenna:2;		// default of antenna, 0: diversity, 1:antenna-A, 2:antenna-B reserved (default = 0)
		USHORT		NumOfAntenna:2;			// Number of antenna
	}	field;
	USHORT			word;
}	EEPROM_ANTENNA_STRUC, *PEEPROM_ANTENNA_STRUC;
#else
typedef	union	_EEPROM_ANTENNA_STRUC	{
	struct	{
		USHORT		NumOfAntenna:2;			// Number of antenna
		USHORT		TxDefaultAntenna:2;		// default of antenna, 0: diversity, 1:antenna-A, 2:antenna-B reserved (default = 0)
		USHORT		RxDefaultAntenna:2;		// default of antenna, 0: diversity, 1:antenna-A, 2:antenna-B reserved (default = 0)
		USHORT		FrameType:1;			// 0: DPDT , 1: SPDT , noted this bit is valid for g only.
		USHORT		Rsv:2;
		USHORT      DynamicTxAgcControl:1;
		USHORT		HardwareRadioControl:1;	// 1: Hardware controlled radio enabled, Read GPIO0 required.
		USHORT      RfIcType:5;             // see E2PROM document
	}	field;
	USHORT			word;
}	EEPROM_ANTENNA_STRUC, *PEEPROM_ANTENNA_STRUC;
#endif

#ifdef BIG_ENDIAN
typedef	union	_EEPROM_NIC_CINFIG2_STRUC	{
	struct	{
		USHORT      Rsv2:11;					// must be 0
		USHORT		ExternalLNA:1;			// external LNA enable
		USHORT		Rsv1:4;
	}	field;
	USHORT			word;
}	EEPROM_NIC_CONFIG2_STRUC, *PEEPROM_NIC_CONFIG2_STRUC;
#else
typedef	union	_EEPROM_NIC_CINFIG2_STRUC	{
	struct	{
		USHORT		Rsv1:4;
		USHORT		ExternalLNA:1;			// external LNA enable
		USHORT      Rsv2:11;                 // must be 0
	}	field;
	USHORT			word;
}	EEPROM_NIC_CONFIG2_STRUC, *PEEPROM_NIC_CONFIG2_STRUC;
#endif

#ifdef BIG_ENDIAN
typedef	union	_EEPROM_TX_PWR_STRUC	{
	struct	{
		UCHAR	Byte1;				// High Byte
		UCHAR	Byte0;				// Low Byte
	}	field;
	USHORT	word;
}	EEPROM_TX_PWR_STRUC, *PEEPROM_TX_PWR_STRUC;
#else
typedef	union	_EEPROM_TX_PWR_STRUC	{
	struct	{
		UCHAR	Byte0;				// Low Byte
		UCHAR	Byte1;				// High Byte
	}	field;
	USHORT	word;
}	EEPROM_TX_PWR_STRUC, *PEEPROM_TX_PWR_STRUC;
#endif

#ifdef BIG_ENDIAN
typedef	union	_EEPROM_VERSION_STRUC	{
	struct	{
		UCHAR	Version;			// High Byte
		UCHAR	FaeReleaseNumber;	// Low Byte
	}	field;
	USHORT	word;
}	EEPROM_VERSION_STRUC, *PEEPROM_VERSION_STRUC;
#else
typedef	union	_EEPROM_VERSION_STRUC	{
	struct	{
		UCHAR	FaeReleaseNumber;	// Low Byte
		UCHAR	Version;			// High Byte
	}	field;
	USHORT	word;
}	EEPROM_VERSION_STRUC, *PEEPROM_VERSION_STRUC;
#endif

#ifdef BIG_ENDIAN
typedef	union	_EEPROM_LED_STRUC	{
	struct	{
		USHORT	Rsvd:3;				// Reserved
		USHORT	LedMode:5;			// Led mode.
		USHORT	PolarityGPIO_4:1;	// Polarity GPIO#4 setting.
		USHORT	PolarityGPIO_3:1;	// Polarity GPIO#3 setting.
		USHORT	PolarityGPIO_2:1;	// Polarity GPIO#2 setting.
		USHORT	PolarityGPIO_1:1;	// Polarity GPIO#1 setting.
		USHORT	PolarityGPIO_0:1;	// Polarity GPIO#0 setting.
		USHORT	PolarityACT:1;		// Polarity ACT setting.
		USHORT	PolarityRDY_A:1;		// Polarity RDY_A setting.
		USHORT	PolarityRDY_G:1;		// Polarity RDY_G setting.
	}	field;
	USHORT	word;
}	EEPROM_LED_STRUC, *PEEPROM_LED_STRUC;
#else
typedef	union	_EEPROM_LED_STRUC	{
	struct	{
		USHORT	PolarityRDY_G:1;		// Polarity RDY_G setting.
		USHORT	PolarityRDY_A:1;		// Polarity RDY_A setting.
		USHORT	PolarityACT:1;		// Polarity ACT setting.
		USHORT	PolarityGPIO_0:1;	// Polarity GPIO#0 setting.
		USHORT	PolarityGPIO_1:1;	// Polarity GPIO#1 setting.
		USHORT	PolarityGPIO_2:1;	// Polarity GPIO#2 setting.
		USHORT	PolarityGPIO_3:1;	// Polarity GPIO#3 setting.
		USHORT	PolarityGPIO_4:1;	// Polarity GPIO#4 setting.
		USHORT	LedMode:5;			// Led mode.
		USHORT	Rsvd:3;				// Reserved
	}	field;
	USHORT	word;
}	EEPROM_LED_STRUC, *PEEPROM_LED_STRUC;
#endif

#ifdef BIG_ENDIAN
typedef	union	_EEPROM_TXPOWER_DELTA_STRUC	{
	struct	{
		UCHAR	TxPowerEnable:1;// Enable
		UCHAR	Type:1;			// 1: plus the delta value, 0: minus the delta value
		UCHAR	DeltaValue:6;	// Tx Power dalta value (MAX=4)
	}	field;
	UCHAR	value;
}	EEPROM_TXPOWER_DELTA_STRUC, *PEEPROM_TXPOWER_DELTA_STRUC;
#else
typedef	union	_EEPROM_TXPOWER_DELTA_STRUC	{
	struct	{
		UCHAR	DeltaValue:6;	// Tx Power dalta value (MAX=4)
		UCHAR	Type:1;			// 1: plus the delta value, 0: minus the delta value
		UCHAR	TxPowerEnable:1;// Enable
	}	field;
	UCHAR	value;
}	EEPROM_TXPOWER_DELTA_STRUC, *PEEPROM_TXPOWER_DELTA_STRUC;
#endif

#ifndef _PRISMHEADER
#define _PRISMHEADER

enum {
	DIDmsg_lnxind_wlansniffrm		= 0x00000044,
	DIDmsg_lnxind_wlansniffrm_hosttime	= 0x00010044,
	DIDmsg_lnxind_wlansniffrm_mactime	= 0x00020044,
	DIDmsg_lnxind_wlansniffrm_channel	= 0x00030044,
	DIDmsg_lnxind_wlansniffrm_rssi		= 0x00040044,
	DIDmsg_lnxind_wlansniffrm_sq		= 0x00050044,
	DIDmsg_lnxind_wlansniffrm_signal	= 0x00060044,
	DIDmsg_lnxind_wlansniffrm_noise		= 0x00070044,
	DIDmsg_lnxind_wlansniffrm_rate		= 0x00080044,
	DIDmsg_lnxind_wlansniffrm_istx		= 0x00090044,
	DIDmsg_lnxind_wlansniffrm_frmlen	= 0x000A0044
};
enum {
	P80211ENUM_msgitem_status_no_value	= 0x00
};
enum {
	P80211ENUM_truth_false			= 0x00,
	P80211ENUM_truth_true			= 0x01
};

typedef struct {
	u_int32_t did;
	u_int16_t status;
	u_int16_t len;
	u_int32_t data;
} p80211item_uint32_t;

typedef struct {
	u_int32_t msgcode;
	u_int32_t msglen;
#define WLAN_DEVNAMELEN_MAX 16
	u_int8_t devname[WLAN_DEVNAMELEN_MAX];
	p80211item_uint32_t hosttime;
	p80211item_uint32_t mactime;
	p80211item_uint32_t channel;
	p80211item_uint32_t rssi;
	p80211item_uint32_t sq;
	p80211item_uint32_t signal;
	p80211item_uint32_t noise;
	p80211item_uint32_t rate;
	p80211item_uint32_t istx;
	p80211item_uint32_t frmlen;
} wlan_ng_prism2_header;

#endif

#endif	// __RT73_H__

