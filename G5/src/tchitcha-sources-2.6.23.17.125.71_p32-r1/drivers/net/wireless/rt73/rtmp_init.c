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
 *	Module Name:	rtmp_init.c
 *
 *	Abstract: Miniport generic portion header file
 *
 *	Revision History:
 *	Who		When		What
 *	--------	----------	-----------------------------
 *	Paul Lin	2002-08-01	created
 *	John Chang	2004-08-20	RT2561/2661 use scatter-gather scheme
 *	Olivier Cornu	2007-05-14	Remove .dat file code
 ***************************************************************************/

#include	"rt_config.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,5,0)
#define RT_USB_ALLOC_URB(iso)	usb_alloc_urb(iso, GFP_KERNEL);
#else
#define RT_USB_ALLOC_URB(iso)	usb_alloc_urb(iso);
#endif

UCHAR	 BIT8[] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80};
ULONG	 BIT32[] = {0x00000001, 0x00000002, 0x00000004, 0x00000008,
					0x00000010, 0x00000020, 0x00000040, 0x00000080,
					0x00000100, 0x00000200, 0x00000400, 0x00000800,
					0x00001000, 0x00002000, 0x00004000, 0x00008000,
					0x00010000, 0x00020000, 0x00040000, 0x00080000,
					0x00100000, 0x00200000, 0x00400000, 0x00800000,
					0x01000000, 0x02000000, 0x04000000, 0x08000000,
					0x10000000, 0x20000000, 0x40000000, 0x80000000};

char*	CipherName[] = {"none","wep64","wep128","TKIP","AES","CKIP64","CKIP128"};

const unsigned short ccitt_16Table[] = {
	0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
	0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
	0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
	0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
	0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485,
	0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
	0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
	0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
	0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
	0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
	0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12,
	0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
	0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41,
	0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
	0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
	0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
	0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F,
	0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
	0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E,
	0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
	0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
	0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
	0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C,
	0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
	0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB,
	0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
	0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
	0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
	0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9,
	0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
	0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8,
	0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0
};
#define ByteCRC16(v, crc) \
	(unsigned short)((crc << 8) ^  ccitt_16Table[((crc >> 8) ^ (v)) & 255])


//
// BBP register initialization set
//
BBP_REG_PAIR   BBPRegTable[] = {
	{3, 	0x80},
	{15,	0x30},
	{17,	0x20},
	{21,	0xc8},
	{22,	0x38},
	{23,	0x06},
	{24,	0xfe},
	{25,	0x0a},
	{26,	0x0d},
	{32,	0x0b},
	{34,	0x12},
	{37,	0x07},
	{39,	0xf8}, // 2005-09-02 by Gary, Atheros 11b issue
	{41,	0x60}, // 03-09 gary
	{53,	0x10}, // 03-09 gary
	{54,	0x18}, // 03-09 gary
	{60,	0x10},
	{61,	0x04},
	{62,	0x04},
	{75,	0xfe},
	{86,	0xfe},
	{88,	0xfe},
	{90,	0x0f},
	{99,	0x00},
	{102,	0x16},
	{107,	0x04},
};
#define	NUM_BBP_REG_PARMS	(sizeof(BBPRegTable) / sizeof(BBP_REG_PAIR))


//
// ASIC register initialization sets
//
RTMP_REG_PAIR	MACRegTable[] =	{
	{TXRX_CSR0, 	0x025fb032}, // 0x3040, RX control, default Disable RX
	{TXRX_CSR1, 	0x9eaa9eaf}, // 0x3044, BBP 30:Ant-A RSSI, R51:Ant-B RSSI, R42:OFDM rate, R47:CCK SIGNAL
	{TXRX_CSR2, 	0x8a8b8c8d}, // 0x3048, CCK TXD BBP registers
	{TXRX_CSR3, 	0x00858687}, // 0x304c, OFDM TXD BBP registers
	{TXRX_CSR7, 	0x2E31353B}, // 0x305c, ACK/CTS payload consume time for 18/12/9/6 mbps
	{TXRX_CSR8, 	0x2a2a2a2c}, // 0x3060, ACK/CTS payload consume time for 54/48/36/24 mbps
	{TXRX_CSR15,	0x0000000f}, // 0x307c, TKIP MIC priority byte "AND" mask
	{MAC_CSR6,		0x00000fff}, // 0x3018, MAX frame length
	{MAC_CSR8,		0x016c030a}, // 0x3020, SIFS/EIFS time, set SIFS delay time.
	{MAC_CSR10, 	0x00000718}, // 0x3028, ASIC PIN control in various power states
	{MAC_CSR12, 	0x00000004}, // 0x3030, power state control, set to AWAKE state
	{MAC_CSR13, 	0x00007f00}, // 0x3034, GPIO pin#7 as bHwRadio (input:0), otherwise (output:1)
	{SEC_CSR0,		0x00000000}, // 0x30a0, invalidate all shared key entries
	{SEC_CSR1,		0x00000000}, // 0x30a4, reset all shared key algorithm to "none"
	{SEC_CSR5,		0x00000000}, // 0x30b4, reset all shared key algorithm to "none"
	{PHY_CSR1,		0x000023b0}, // 0x3084, BBP Register R/W mode set to "Parallel mode"
	{PHY_CSR5,		0x00040a06}, //  0x060a100c
	{PHY_CSR6,		0x00080606},
	{PHY_CSR7,		0x00000408},
	{AIFSN_CSR, 	0x00002273},
	{CWMIN_CSR, 	0x00002344},
	{CWMAX_CSR, 	0x000034aa},
};
#define	NUM_MAC_REG_PARMS	(sizeof(MACRegTable) / sizeof(RTMP_REG_PAIR))

VOID CreateThreads(PRTMP_ADAPTER pAd)
{
	// Creat MLME Thread
	pAd->MLMEThr_pid = kernel_thread(MlmeThread, pAd, CLONE_VM);
	if (pAd->MLMEThr_pid < 0) {
		DBGPRINT(RT_DEBUG_ERROR, "%s: unable to start mlme thread for %s\n",
				__FUNCTION__, pAd->net_dev->name);
		KPRINT(KERN_WARNING, "%s: unable to start mlme thread\n",
				pAd->net_dev->name);
	}

	// Creat Command Thread
	pAd->RTUSBCmdThr_pid = kernel_thread(RTUSBCmdThread, pAd, CLONE_VM);
	if (pAd->RTUSBCmdThr_pid < 0) {
		DBGPRINT(RT_DEBUG_ERROR, "%s: unable to start Cmd thread for %s\n",
				__FUNCTION__, pAd->net_dev->name);
		KPRINT(KERN_WARNING, "%s: unable to start Cmd thread\n",
				pAd->net_dev->name);
	}
	DBGPRINT(RT_DEBUG_INFO, "-  (%s) Mlme pid=%d, Cmd pid=%d\n",
			__FUNCTION__, pAd->MLMEThr_pid, pAd->RTUSBCmdThr_pid);
}

void KillThreads(PRTMP_ADAPTER pAd)
{
	int             ret;

	if (pAd->MLMEThr_pid > 0)
	{
		ret = kill_proc (pAd->MLMEThr_pid, SIGTERM, 1);
		if (ret)
		{
			DBGPRINT(RT_DEBUG_ERROR, "%s(%s): unable to signal mlme thread"
					" (pid=%d, err=%d)\n",
					__FUNCTION__, pAd->net_dev->name, pAd->MLMEThr_pid, ret);
			KPRINT(KERN_ERR, "(%s) unable to signal mlme thread"
					" (pid=%d, err=%d)\n",
					pAd->net_dev->name, pAd->MLMEThr_pid, ret);
			//return ret;		Fix process killing
		}
		else wait_for_completion (&pAd->mlmenotify);
	}
	if (pAd->RTUSBCmdThr_pid> 0)
	{
		ret = kill_proc (pAd->RTUSBCmdThr_pid, SIGTERM, 1);
		if (ret)
		{
			DBGPRINT(RT_DEBUG_ERROR, "%s(%s): unable to signal cmd thread"
					" (pid=%d, err=%d)\n",
					__FUNCTION__, pAd->net_dev->name, pAd->MLMEThr_pid, ret);
			KPRINT(KERN_ERR, "(%s) unable to signal cmd thread"
					" (pid=%d, err=%d)\n",
					pAd->net_dev->name, pAd->RTUSBCmdThr_pid, ret);
			//return ret;		Fix process killing
		}
		else wait_for_completion (&pAd->cmdnotify);
	}
	// reset mlme & command thread
    pAd->MLMEThr_pid = -1;
	pAd->RTUSBCmdThr_pid = -1;

} /* End KillThreads () */

NDIS_STATUS NICInitTransmit(
	IN	PRTMP_ADAPTER	 pAd )
{
	UCHAR			i, acidx;
	NDIS_STATUS 	Status = NDIS_STATUS_SUCCESS;
	PTX_CONTEXT		pPsPollContext = &(pAd->PsPollContext);
	PTX_CONTEXT		pNullContext   = &(pAd->NullContext);
	PTX_CONTEXT		pRTSContext    = &(pAd->RTSContext);

	DBGPRINT(RT_DEBUG_TRACE,"--> NICInitTransmit\n");

	// Init 4 set of Tx parameters
	for (i = 0; i < 4; i++)
	{
		// Initialize all Transmit releated queues
		skb_queue_head_init(&pAd->SendTxWaitQueue[i]);

		pAd->NextTxIndex[i]			= 0;		// Next Free local Tx ring pointer
		pAd->TxRingTotalNumber[i]	= 0;
		pAd->NextBulkOutIndex[i]	= 0;		// Next Local tx ring pointer waiting for buck out
		pAd->BulkOutPending[i]		= FALSE;	// Buck Out control flag
	}

	pAd->PrivateInfo.TxRingFullCnt = 0;

	pAd->NextMLMEIndex		   = 0;
	pAd->PushMgmtIndex		   = 0;
	pAd->PopMgmtIndex		   = 0;
	atomic_set(&pAd->MgmtQueueSize, 0);


	pAd->PrioRingFirstIndex    = 0;
	pAd->PrioRingTxCnt		   = 0;

	do
	{
		//
		// TX_RING_SIZE
		//
		for (acidx = 0; acidx < 4; acidx++)
		{
			for ( i= 0; i < TX_RING_SIZE; i++ )
			{
				PTX_CONTEXT pTxContext = &(pAd->TxContext[acidx][i]);

				//Allocate URB
				pTxContext->pUrb = RT_USB_ALLOC_URB(0);
				if(pTxContext->pUrb == NULL){
					Status = NDIS_STATUS_RESOURCES;
					goto done;
				}
				pTxContext->TransferBuffer= (PTX_BUFFER) kmalloc(sizeof(TX_BUFFER), GFP_KERNEL);
				Status = NDIS_STATUS_SUCCESS;
				if(!pTxContext->TransferBuffer){
					DBGPRINT(RT_DEBUG_ERROR,"Not enough memory\n");
					Status = NDIS_STATUS_RESOURCES;
					goto out1;
				}

				memset(pTxContext->TransferBuffer, 0, sizeof(TX_BUFFER));

				pTxContext->pAd = pAd;
				pTxContext->InUse = FALSE;
				pTxContext->IRPPending = FALSE;
			}
		}


		//
		// PRIO_RING_SIZE
		//
		for ( i= 0; i < PRIO_RING_SIZE; i++ )
		{
			PTX_CONTEXT	pMLMEContext = &(pAd->MLMEContext[i]);

			pMLMEContext->pUrb = RT_USB_ALLOC_URB(0);
			if(pMLMEContext->pUrb == NULL){
				Status = NDIS_STATUS_RESOURCES;
				goto out1;
			}

			pMLMEContext->TransferBuffer= (PTX_BUFFER) kmalloc(sizeof(TX_BUFFER), GFP_KERNEL);
			if(!pMLMEContext->TransferBuffer){
				DBGPRINT(RT_DEBUG_ERROR,"Not enough memory\n");
				Status = NDIS_STATUS_RESOURCES;
				goto out2;
			}

			memset(pMLMEContext->TransferBuffer, 0, sizeof(TX_BUFFER));
			pMLMEContext->pAd = pAd ;
			pMLMEContext->InUse = FALSE;
			pMLMEContext->IRPPending = FALSE;
		}


		//
		// BEACON_RING_SIZE
		//
		for (i = 0; i < BEACON_RING_SIZE; i++)
		{
			PTX_CONTEXT	pBeaconContext = &(pAd->BeaconContext[i]);
			pBeaconContext->pUrb = RT_USB_ALLOC_URB(0);
			if(pBeaconContext->pUrb == NULL){
				Status = NDIS_STATUS_RESOURCES;
				goto out2;
			}

			pBeaconContext->TransferBuffer= (PTX_BUFFER) kmalloc(sizeof(TX_BUFFER), GFP_KERNEL);
			if(!pBeaconContext->TransferBuffer){
				DBGPRINT(RT_DEBUG_ERROR,"Not enough memory\n");
				Status = NDIS_STATUS_RESOURCES;
				goto out3;
			}
			memset(pBeaconContext->TransferBuffer, 0, sizeof(TX_BUFFER));

			pBeaconContext->pAd = pAd;
			pBeaconContext->InUse = FALSE;
			pBeaconContext->IRPPending = FALSE;
		}


		//
		// NullContext
		//
		pNullContext->pUrb = RT_USB_ALLOC_URB(0);
		if(pNullContext->pUrb == NULL){
			Status = NDIS_STATUS_RESOURCES;
			goto out3;
		}

		pNullContext->TransferBuffer= (PTX_BUFFER) kmalloc(sizeof(TX_BUFFER), GFP_KERNEL);
		if(!pNullContext->TransferBuffer){
			DBGPRINT(RT_DEBUG_ERROR,"Not enough memory\n");
			Status = NDIS_STATUS_RESOURCES;
			goto out4;
		}

		memset(pNullContext->TransferBuffer, 0, sizeof(TX_BUFFER));
		pNullContext->pAd = pAd;
		pNullContext->InUse = FALSE;
		pNullContext->IRPPending = FALSE;

		//
		// RTSContext
		//
		pRTSContext->pUrb = RT_USB_ALLOC_URB(0);
		if(pRTSContext->pUrb == NULL){
			Status = NDIS_STATUS_RESOURCES;
			goto out4;
		}

		pRTSContext->TransferBuffer= (PTX_BUFFER) kmalloc(sizeof(TX_BUFFER), GFP_KERNEL);
		if(!pRTSContext->TransferBuffer){
			DBGPRINT(RT_DEBUG_ERROR,"Not enough memory\n");
			Status = NDIS_STATUS_RESOURCES;
			goto out5;
		}

		memset(pRTSContext->TransferBuffer, 0, sizeof(TX_BUFFER));
		pRTSContext->pAd = pAd;
		pRTSContext->InUse = FALSE;
		pRTSContext->IRPPending = FALSE;


		//
		// PsPollContext
		//
		pPsPollContext->pUrb = RT_USB_ALLOC_URB(0);
		if(pPsPollContext->pUrb == NULL){
			Status = NDIS_STATUS_RESOURCES;
			goto out5;
		}

		pPsPollContext->TransferBuffer= (PTX_BUFFER) kmalloc(sizeof(TX_BUFFER), GFP_KERNEL);
		if(!pPsPollContext->TransferBuffer){
			DBGPRINT(RT_DEBUG_ERROR,"Not enough memory\n");
			Status = NDIS_STATUS_RESOURCES;
			goto out6;
		}

		memset(pPsPollContext->TransferBuffer, 0, sizeof(TX_BUFFER));
		pPsPollContext->pAd = pAd;
		pPsPollContext->InUse = FALSE;
		pPsPollContext->IRPPending = FALSE;

	}  while (FALSE);

	return Status;


out6:
	if (NULL != pPsPollContext->pUrb)
	{
		RTUSB_UNLINK_URB(pPsPollContext->pUrb);
		usb_free_urb(pPsPollContext->pUrb);
		pPsPollContext->pUrb = NULL;
	}
	if (NULL != pPsPollContext->TransferBuffer)
	{
		kfree(pPsPollContext->TransferBuffer);
		pPsPollContext->TransferBuffer = NULL;
	}
out5:
	if (NULL != pRTSContext->pUrb)
	{
		RTUSB_UNLINK_URB(pRTSContext->pUrb);
		usb_free_urb(pRTSContext->pUrb);
		pRTSContext->pUrb = NULL;
	}
	if (NULL != pRTSContext->TransferBuffer)
	{
		kfree(pRTSContext->TransferBuffer);
		pRTSContext->TransferBuffer = NULL;
	}
out4:
	if (NULL != pNullContext->pUrb)
	{
		RTUSB_UNLINK_URB(pNullContext->pUrb);
		usb_free_urb(pNullContext->pUrb);
		pNullContext->pUrb = NULL;
	}
	if (NULL != pNullContext->TransferBuffer)
	{
		kfree(pNullContext->TransferBuffer);
		pNullContext->TransferBuffer = NULL;
	}
out3:
	for (i = 0; i < BEACON_RING_SIZE; i++)
	{
		PTX_CONTEXT	pBeaconContext = &(pAd->BeaconContext[i]);
		if ( NULL != pBeaconContext->pUrb )
		{
			RTUSB_UNLINK_URB(pBeaconContext->pUrb);
			usb_free_urb(pBeaconContext->pUrb);
			pBeaconContext->pUrb = NULL;
		}

		if ( NULL != pBeaconContext->TransferBuffer )
		{
			kfree( pBeaconContext->TransferBuffer);
			pBeaconContext->TransferBuffer = NULL;
		}
	}
out2:
	for ( i= 0; i < PRIO_RING_SIZE; i++ )
	{
		PTX_CONTEXT pMLMEContext = &(pAd->MLMEContext[i]);

		if ( NULL != pMLMEContext->pUrb )
		{
			RTUSB_UNLINK_URB(pMLMEContext->pUrb);
			usb_free_urb(pMLMEContext->pUrb);
			pMLMEContext->pUrb = NULL;
		}

		if ( NULL != pMLMEContext->TransferBuffer )
		{
			kfree( pMLMEContext->TransferBuffer);
			pMLMEContext->TransferBuffer = NULL;
		}
	}
out1:
	for (acidx = 0; acidx < 4; acidx++)
	{
		for ( i= 0; i < TX_RING_SIZE; i++ )
		{
			PTX_CONTEXT pTxContext = &(pAd->TxContext[acidx][i]);

			if ( NULL != pTxContext->pUrb )
			{
				RTUSB_UNLINK_URB(pTxContext->pUrb);
				usb_free_urb(pTxContext->pUrb);
				pTxContext->pUrb = NULL;
			}
			if ( NULL != pTxContext->TransferBuffer )
			{
				kfree( pTxContext->TransferBuffer);
				pTxContext->TransferBuffer = NULL;
			}
		}
	}

done:
	return Status;
}

/*
	========================================================================

	Routine Description:
		Initialize receive data structures

	Arguments:
		Adapter 					Pointer to our adapter

	Return Value:
		NDIS_STATUS_SUCCESS
		NDIS_STATUS_RESOURCES

	Note:
		Initialize all receive releated private buffer, include those define
		in RTMP_ADAPTER structure and all private data structures. The mahor
		work is to allocate buffer for each packet and chain buffer to
		NDIS packet descriptor.

	========================================================================
*/
NDIS_STATUS NICInitRecv(
	IN	PRTMP_ADAPTER	pAd)
{
	UCHAR	i;
	NDIS_STATUS 	Status = NDIS_STATUS_SUCCESS;


	DBGPRINT(RT_DEBUG_TRACE,"--> NICInitRecv\n");
	pAd->NextRxBulkInIndex = 0;	// Rx Bulk pointer
	pAd->CurRxBulkInIndex = 0;
	atomic_set( &pAd->PendingRx, 0);
	for (i = 0; i < RX_RING_SIZE; i++)
	{
		PRX_CONTEXT  pRxContext = &(pAd->RxContext[i]);
		pRxContext->pUrb = RT_USB_ALLOC_URB(0);
		if(pRxContext->pUrb == NULL)
		{
			Status = NDIS_STATUS_RESOURCES;
			DBGPRINT(RT_DEBUG_TRACE,"--> pRxContext->pUrb == NULL\n");
			break;
		}

		pRxContext->TransferBuffer= (PUCHAR) kmalloc(BUFFER_SIZE, MEM_ALLOC_FLAG);
		if(!pRxContext->TransferBuffer)
		{
			DBGPRINT(RT_DEBUG_ERROR,"Not enough memory\n");
			Status = NDIS_STATUS_RESOURCES;
			break;
		}

		memset(pRxContext->TransferBuffer, 0, BUFFER_SIZE);

		pRxContext->pAd	= pAd;
		pRxContext->InUse = FALSE;
		atomic_set(&pRxContext->IrpLock, IRPLOCK_COMPLETED);
		pRxContext->IRPPending	= FALSE;
	}
	if (Status) ReleaseAdapter(pAd, TRUE, FALSE);

	DBGPRINT(RT_DEBUG_TRACE,"<-- NICInitRecv status=%d\n", Status);
	return Status;
}

////////////////////////////////////////////////////////////////////////////
//
//	FUNCTION
//		ReleaseAdapter
//
//	DESCRIPTION
//		Frees memory allocated for URBs and transfer buffers.
//
//	INPUT
//		Adapter 	Pointer to RTMP_ADAPTER structure
//
//	OUTPUT
//		-
//
////////////////////////////////////////////////////////////////////////////
VOID ReleaseAdapter(
	IN	PRTMP_ADAPTER pAd,
    IN  BOOLEAN         IsFree,
	IN  BOOLEAN         IsOnlyTx)
{
	UINT			i, acidx;
	PTX_CONTEXT		pNullContext   = &pAd->NullContext;
	PTX_CONTEXT		pPsPollContext = &pAd->PsPollContext;
	PTX_CONTEXT 	pRTSContext    = &pAd->RTSContext;

	DBGPRINT(RT_DEBUG_TRACE, "---> ReleaseAdapter\n");

    if (!IsOnlyTx)
    {
	    // Free all resources for the RECEIVE buffer queue.
	    for (i = 0; i < RX_RING_SIZE; i++)
	    {
		    PRX_CONTEXT  pRxContext = &(pAd->RxContext[i]);

		    if (pRxContext->pUrb != NULL)
		    {
			    RTUSB_UNLINK_URB(pRxContext->pUrb);
			    if (IsFree)
			    usb_free_urb(pRxContext->pUrb);
			    pRxContext->pUrb = NULL;
		    }
		    if (pRxContext->TransferBuffer != NULL)
		    {
			    kfree(pRxContext->TransferBuffer);
			    pRxContext->TransferBuffer = NULL;
		    }

	    }
    }

	// Free PsPoll frame resource
	if (NULL != pPsPollContext->pUrb)
	{
		RTUSB_UNLINK_URB(pPsPollContext->pUrb);
		if (IsFree)
		usb_free_urb(pPsPollContext->pUrb);
		pPsPollContext->pUrb = NULL;
	}
	if (NULL != pPsPollContext->TransferBuffer)
	{
		kfree(pPsPollContext->TransferBuffer);
		pPsPollContext->TransferBuffer = NULL;
	}

	// Free NULL frame resource
	if (NULL != pNullContext->pUrb)
	{
		RTUSB_UNLINK_URB(pNullContext->pUrb);
		if (IsFree)
		usb_free_urb(pNullContext->pUrb);
		pNullContext->pUrb = NULL;
	}
	if (NULL != pNullContext->TransferBuffer)
	{
		kfree(pNullContext->TransferBuffer);
		pNullContext->TransferBuffer = NULL;
	}

	// Free RTS frame resource
	if (NULL != pRTSContext->pUrb)
	{
		RTUSB_UNLINK_URB(pRTSContext->pUrb);
		if (IsFree)
		usb_free_urb(pRTSContext->pUrb);
		pRTSContext->pUrb = NULL;
	}
	if (NULL != pRTSContext->TransferBuffer)
	{
		kfree(pRTSContext->TransferBuffer);
		pRTSContext->TransferBuffer = NULL;
	}

	// Free beacon frame resource
	for (i = 0; i < BEACON_RING_SIZE; i++)
	{
		PTX_CONTEXT	pBeaconContext = &(pAd->BeaconContext[i]);
		if ( NULL != pBeaconContext->pUrb )
		{
			RTUSB_UNLINK_URB(pBeaconContext->pUrb);
			if (IsFree)
			usb_free_urb(pBeaconContext->pUrb);
			pBeaconContext->pUrb = NULL;
		}

		if ( NULL != pBeaconContext->TransferBuffer )
		{
			kfree( pBeaconContext->TransferBuffer);
			pBeaconContext->TransferBuffer = NULL;
		}
	}

	// Free Prio frame resource
	for ( i= 0; i < PRIO_RING_SIZE; i++ )
	{
		PTX_CONTEXT pMLMEContext = &(pAd->MLMEContext[i]);

		if ( NULL != pMLMEContext->pUrb )
		{
			RTUSB_UNLINK_URB(pMLMEContext->pUrb);
			if (IsFree)
			usb_free_urb(pMLMEContext->pUrb);
			pMLMEContext->pUrb = NULL;
		}

		if ( NULL != pMLMEContext->TransferBuffer )
		{
			kfree( pMLMEContext->TransferBuffer);
			pMLMEContext->TransferBuffer = NULL;
		}
	}

	// Free Tx frame resource
	for (acidx = 0; acidx < 4; acidx++)
	{
		for ( i= 0; i < TX_RING_SIZE; i++ )
		{
			PTX_CONTEXT pTxContext = &(pAd->TxContext[acidx][i]);

			if ( NULL != pTxContext->pUrb )
			{
				RTUSB_UNLINK_URB(pTxContext->pUrb);
				if (IsFree)
				usb_free_urb(pTxContext->pUrb);
				pTxContext->pUrb = NULL;
			}

			if ( NULL != pTxContext->TransferBuffer )
			{
				kfree( pTxContext->TransferBuffer);
				pTxContext->TransferBuffer = NULL;
			}
		}
	}

	DBGPRINT(RT_DEBUG_TRACE, "<--- ReleaseAdapter\n");

}

/*
	========================================================================

	Routine Description:
		Allocate DMA memory blocks for send, receive

	Arguments:
		Adapter		Pointer to our adapter

	Return Value:
		None.

	Note:

	========================================================================
*/
void RTMPInitAdapterBlock(
	IN	PRTMP_ADAPTER	pAd)
{
	UINT			i;
	PCmdQElmt		cmdqelmt;

	DBGPRINT(RT_DEBUG_TRACE, "--> RTMPInitAdapterBlock\n");

	// init counter
	pAd->WlanCounters.TransmittedFragmentCount.vv.LowPart =  0;
	pAd->WlanCounters.MulticastTransmittedFrameCount.vv.LowPart =0;
	pAd->WlanCounters.FailedCount.vv.LowPart =0;
	pAd->WlanCounters.NoRetryCount.vv.LowPart =0;
	pAd->WlanCounters.RetryCount.vv.LowPart =0;
	pAd->WlanCounters.MultipleRetryCount.vv.LowPart =0;
	pAd->WlanCounters.RTSSuccessCount.vv.LowPart =0;
	pAd->WlanCounters.RTSFailureCount.vv.LowPart =0;
	pAd->WlanCounters.ACKFailureCount.vv.LowPart =0;
	pAd->WlanCounters.FrameDuplicateCount.vv.LowPart =0;
	pAd->WlanCounters.ReceivedFragmentCount.vv.LowPart =0;
	pAd->WlanCounters.MulticastReceivedFrameCount.vv.LowPart =0;
	pAd->WlanCounters.FCSErrorCount.vv.LowPart =0;

	pAd->WlanCounters.TransmittedFragmentCount.vv.HighPart =  0;
	pAd->WlanCounters.MulticastTransmittedFrameCount.vv.HighPart =0;
	pAd->WlanCounters.FailedCount.vv.HighPart =0;
	pAd->WlanCounters.NoRetryCount.vv.HighPart =0;
	pAd->WlanCounters.RetryCount.vv.HighPart =0;
	pAd->WlanCounters.MultipleRetryCount.vv.HighPart =0;
	pAd->WlanCounters.RTSSuccessCount.vv.HighPart =0;
	pAd->WlanCounters.RTSFailureCount.vv.HighPart =0;
	pAd->WlanCounters.ACKFailureCount.vv.HighPart =0;
	pAd->WlanCounters.FrameDuplicateCount.vv.HighPart =0;
	pAd->WlanCounters.ReceivedFragmentCount.vv.HighPart =0;
	pAd->WlanCounters.MulticastReceivedFrameCount.vv.HighPart =0;
	pAd->WlanCounters.FCSErrorCount.vv.HighPart =0;

	do
	{
		for (i = 0; i < COMMAND_QUEUE_SIZE; i++)
		{
			cmdqelmt = &(pAd->CmdQElements[i]);
			memset(cmdqelmt, 0, sizeof(CmdQElmt));
			cmdqelmt->buffer = NULL;
			cmdqelmt->CmdFromNdis = FALSE;
			cmdqelmt->InUse = FALSE;
		}
		RTUSBInitializeCmdQ(&pAd->CmdQ);

		pAd->MLMEThr_pid= -1;
		pAd->RTUSBCmdThr_pid= -1;

		init_MUTEX_LOCKED(&(pAd->usbdev_semaphore));
		init_MUTEX_LOCKED(&(pAd->mlme_semaphore));
		init_MUTEX_LOCKED(&(pAd->RTUSBCmd_semaphore));

		init_completion (&pAd->mlmenotify);	// event initially non-signalled
		init_completion (&pAd->cmdnotify); 	// event initially non-signalled

		////////////////////////
		// Spinlock
		NdisAllocateSpinLock(&pAd->BulkOutLock[0]);
		NdisAllocateSpinLock(&pAd->BulkOutLock[1]);
		NdisAllocateSpinLock(&pAd->BulkOutLock[2]);
		NdisAllocateSpinLock(&pAd->BulkOutLock[3]);
		NdisAllocateSpinLock(&pAd->CmdQLock);

		NdisAllocateSpinLock(&pAd->MLMEWaitQueueLock);
		NdisAllocateSpinLock(&pAd->MLMEQLock);
		NdisAllocateSpinLock(&pAd->BulkFlagsLock);

	}	while (FALSE);

	DBGPRINT(RT_DEBUG_TRACE, "<-- RTMPInitAdapterBlock\n");
}

NDIS_STATUS	RTUSBWriteHWMACAddress(
	IN	PRTMP_ADAPTER		pAd)
{
	MAC_CSR2_STRUC		StaMacReg0;
	MAC_CSR3_STRUC		StaMacReg1;
	NDIS_STATUS			Status = NDIS_STATUS_SUCCESS;
	PUCHAR			curMAC;
	int			t;

	if (pAd->bLocalAdminMAC != TRUE)
	{
		if (!memcmp(pAd->net_dev->dev_addr, "\x00\x00\x00\x00\x00\x00", 6)) {
			KPRINT(KERN_INFO, "using permanent MAC addr\n");
			DBGPRINT(RT_DEBUG_INFO, "-   using permanent MAC addr\n");
			curMAC = pAd->PermanentAddress;
			// Also meets 2.6.24 pre-up requirements - bb
			memcpy(pAd->net_dev->dev_addr, curMAC, pAd->net_dev->addr_len);
		} else {
			KPRINT(KERN_INFO, "using net dev supplied MAC addr\n");
			DBGPRINT(RT_DEBUG_INFO, "-   using net dev supplied MAC addr\n");
			curMAC = pAd->net_dev->dev_addr;
		}

		KPRINT(KERN_INFO, "Active MAC addr: %02x:%02x:%02x:%02x:%02x:%02x\n",
			curMAC[0], curMAC[1], curMAC[2], curMAC[3], curMAC[4], curMAC[5]);
		for (t=0; t<6; t++) pAd->CurrentAddress[t] = curMAC[t];
	}

	// Write New MAC address to MAC_CSR2 & MAC_CSR3 & let ASIC know our new MAC
	StaMacReg0.field.Byte0 = pAd->CurrentAddress[0];
	StaMacReg0.field.Byte1 = pAd->CurrentAddress[1];
	StaMacReg0.field.Byte2 = pAd->CurrentAddress[2];
	StaMacReg0.field.Byte3 = pAd->CurrentAddress[3];
	StaMacReg1.field.Byte4 = pAd->CurrentAddress[4];
	StaMacReg1.field.Byte5 = pAd->CurrentAddress[5];
	StaMacReg1.field.U2MeMask = 0xff;

	KPRINT(KERN_INFO, "Local MAC = %02x:%02x:%02x:%02x:%02x:%02x\n",
		pAd->CurrentAddress[0], pAd->CurrentAddress[1], pAd->CurrentAddress[2],
		pAd->CurrentAddress[3], pAd->CurrentAddress[4], pAd->CurrentAddress[5]);
	DBGPRINT(RT_DEBUG_INFO, "- %s: Local MAC = %02x:%02x:%02x:%02x:%02x:%02x\n",
		__FUNCTION__,
		pAd->CurrentAddress[0], pAd->CurrentAddress[1], pAd->CurrentAddress[2],
		pAd->CurrentAddress[3], pAd->CurrentAddress[4], pAd->CurrentAddress[5]);

	RTUSBWriteMACRegister(pAd, MAC_CSR2, StaMacReg0.word);
	RTUSBWriteMACRegister(pAd, MAC_CSR3, StaMacReg1.word);

	return Status;
}

/*
	========================================================================

	Routine Description:
		Read initial parameters from EEPROM

	Arguments:
		Adapter						Pointer to our adapter

	Return Value:
		None

	Note:

	========================================================================
*/
VOID NICReadEEPROMParameters(
	IN	PRTMP_ADAPTER	pAd)
{
	USHORT  i, value2;
	USHORT  *value = kzalloc(sizeof(USHORT), GFP_KERNEL);
	EEPROM_ANTENNA_STRUC	Antenna;
	EEPROM_VERSION_STRUC  *Version = kzalloc(
				sizeof(EEPROM_VERSION_STRUC), GFP_KERNEL);
	CHAR *ChannelTxPower = kzalloc(sizeof(CHAR)*MAX_NUM_OF_CHANNELS,
					GFP_KERNEL);
	EEPROM_LED_STRUC *LedSetting = kzalloc(sizeof(EEPROM_LED_STRUC),
						GFP_KERNEL);

	DBGPRINT(RT_DEBUG_TRACE, "--> NICReadEEPROMParameters\n");

	if (!value || !Version || !ChannelTxPower || !LedSetting) {
		DBGPRINT(RT_DEBUG_ERROR, "couldn't allocate memory\n");
		return;
	}
	//Read MAC address.
	RTUSBReadEEPROM(pAd, EEPROM_MAC_ADDRESS_BASE_OFFSET,
					pAd->PermanentAddress, MAC_ADDR_LEN);
	DBGPRINT(RT_DEBUG_INFO, "- Local MAC = %02x:%02x:%02x:%02x:%02x:%02x\n",
			pAd->PermanentAddress[0], pAd->PermanentAddress[1],
			pAd->PermanentAddress[2], pAd->PermanentAddress[3],
			pAd->PermanentAddress[4], pAd->PermanentAddress[5]);

	// Init the channel number for TX channel power
	// 0. 11b/g
	for (i = 0; i < 14; i++)
		pAd->TxPower[i].Channel = i + 1;
	// 1. UNI 36 - 64
	for (i = 0; i < 8; i++)
		pAd->TxPower[i + 14].Channel = 36 + i * 4;
	// 2. HipperLAN 2 100 - 140
	for (i = 0; i < 11; i++)
		pAd->TxPower[i + 22].Channel = 100 + i * 4;
	// 3. UNI 140 - 165
	for (i = 0; i < 5; i++)
		pAd->TxPower[i + 33].Channel = 149 + i * 4;

	// 34/38/42/46
	for (i = 0; i < 4; i++)
		pAd->TxPower[i + 38].Channel = 34 + i * 4;

	// if E2PROM version mismatch with driver's expectation, then skip
	// all subsequent E2RPOM retieval and set a system error bit to notify GUI
	RTUSBReadEEPROM(pAd, EEPROM_VERSION_OFFSET, (PUCHAR)&Version->word, 2);
	Version->word = le16_to_cpu(Version->word);
	pAd->EepromVersion = Version->field.Version +
			     Version->field.FaeReleaseNumber * 256;
	DBGPRINT(RT_DEBUG_TRACE, "E2PROM: Version = %d, FAE release #%d\n",
		 Version->field.Version, Version->field.FaeReleaseNumber);

	// Read BBP default from EEPROM, store to array(EEPROMDefaultValue) in pAd
	RTUSBReadEEPROM(pAd, EEPROM_BBP_BASE_OFFSET,
				(PUCHAR)(pAd->EEPROMDefaultValue), 2 * NUM_EEPROM_BBP_PARMS);

	// Bit of a swag, here - bb
	for (i = 0; i < NUM_EEPROM_BBP_PARMS; i++) {
		pAd->EEPROMDefaultValue[i] = le16_to_cpu(pAd->EEPROMDefaultValue[i]);
	}
	// We have to parse NIC configuration 0 at here.
	// If TSSI did not have preloaded value, it should reset TxAutoAgc to false
	// Therefore, we have to read TxAutoAgc control beforehand.
	// Read Tx AGC control bit
	Antenna.word = pAd->EEPROMDefaultValue[0];
	if (Antenna.field.DynamicTxAgcControl == 1)
		pAd->bAutoTxAgcA = pAd->bAutoTxAgcG = TRUE;
	else
		pAd->bAutoTxAgcA = pAd->bAutoTxAgcG = FALSE;

	//
	// Reset PhyMode if we don't support 802.11a
	//
	if ((pAd->PortCfg.PhyMode == PHY_11ABG_MIXED) || (pAd->PortCfg.PhyMode == PHY_11A))
	{
		//
		// Only RFIC_5226, RFIC_5225 suport 11a
		//
		if ((Antenna.field.RfIcType == RFIC_2528) || (Antenna.field.RfIcType == RFIC_2527))
			pAd->PortCfg.PhyMode = PHY_11BG_MIXED;

		//
		// Reset Adhoc Mode if we don't support 802.11a
		//
		if ((pAd->PortCfg.AdhocMode == ADHOC_11A) || (pAd->PortCfg.AdhocMode == ADHOC_11ABG_MIXED))
		{
			//
			// Only RFIC_5226, RFIC_5225 suport 11a
			//
			if ((Antenna.field.RfIcType == RFIC_2528) || (Antenna.field.RfIcType == RFIC_2527))
				pAd->PortCfg.AdhocMode = ADHOC_11BG_MIXED;
		}

    }


	// Read Tx power value for all 14 channels
	// Value from 1 - 0x7f. Default value is 24.
	// 0. 11b/g
	// Power value 0xFA (-6) ~ 0x24 (36)
	RTUSBReadEEPROM(pAd, EEPROM_G_TX_PWR_OFFSET,
					ChannelTxPower, 2 * NUM_EEPROM_TX_G_PARMS);
	for (i = 0; i < 2 * NUM_EEPROM_TX_G_PARMS; i++)
	{
		if ((ChannelTxPower[i] > 36) || (ChannelTxPower[i] < -6))
			pAd->TxPower[i].Power = 24;
		else
			pAd->TxPower[i].Power = ChannelTxPower[i];

		DBGPRINT(RT_DEBUG_INFO, "Tx power for channel %d : 0x%02x\n",
				pAd->TxPower[i].Channel, (UCHAR)(pAd->TxPower[i].Power));
	}

	// 1. UNI 36 - 64, HipperLAN 2 100 - 140, UNI 140 - 165
	// Power value 0xFA (-6) ~ 0x24 (36)
	RTUSBReadEEPROM(pAd, EEPROM_A_TX_PWR_OFFSET,
					ChannelTxPower, MAX_NUM_OF_A_CHANNELS);
	for (i = 0; i < MAX_NUM_OF_A_CHANNELS; i++)
	{
		if ((ChannelTxPower[i] > 36) || (ChannelTxPower[i] < -6))
			pAd->TxPower[i + 14].Power = 24;
		else
			pAd->TxPower[i + 14].Power = ChannelTxPower[i];
		DBGPRINT(RT_DEBUG_INFO, "Tx power for channel %d : 0x%02x\n",
				pAd->TxPower[i + 14].Channel,
				(UCHAR)(pAd->TxPower[i + 14].Power));
	}

	//
	// we must skip frist value, so we get TxPower as ChannelTxPower[i + 1];
	// because the TxPower was stored from 0x7D, but we need to read EEPROM
	// from 0x7C. (Word alignment)
	//
	// for J52, 34/38/42/46
	RTUSBReadEEPROM(pAd, EEPROM_J52_TX_PWR_OFFSET,
					ChannelTxPower, 6); //must Read even valuse

	for (i = 0; i < 4; i++)
	{
		ASSERT(pAd->TxPower[J52_CHANNEL_START_OFFSET + i].Channel == 34 + i * 4);
		if ((ChannelTxPower[i] > 36) || (ChannelTxPower[i] < -6))
			pAd->TxPower[J52_CHANNEL_START_OFFSET + i].Power = 24;
		else
			pAd->TxPower[J52_CHANNEL_START_OFFSET + i].Power = ChannelTxPower[i + 1];

		DBGPRINT(RT_DEBUG_INFO, "Tx power for channel %d : 0x%02x\n",
				pAd->TxPower[J52_CHANNEL_START_OFFSET + i].Channel,
				(UCHAR)pAd->TxPower[J52_CHANNEL_START_OFFSET + i].Power);
	}

	// Read TSSI reference and TSSI boundary for temperature compensation.
	// 0. 11b/g
	{
		RTUSBReadEEPROM(pAd, EEPROM_BG_TSSI_CALIBRAION, ChannelTxPower, 10);
		pAd->TssiMinusBoundaryG[4] = ChannelTxPower[0];
		pAd->TssiMinusBoundaryG[3] = ChannelTxPower[1];
		pAd->TssiMinusBoundaryG[2] = ChannelTxPower[2];
		pAd->TssiMinusBoundaryG[1] = ChannelTxPower[3];
		pAd->TssiPlusBoundaryG[1] = ChannelTxPower[4];
		pAd->TssiPlusBoundaryG[2] = ChannelTxPower[5];
		pAd->TssiPlusBoundaryG[3] = ChannelTxPower[6];
		pAd->TssiPlusBoundaryG[4] = ChannelTxPower[7];
		pAd->TssiRefG	= ChannelTxPower[8];
		pAd->TxAgcStepG = ChannelTxPower[9];
		pAd->TxAgcCompensateG = 0;
		pAd->TssiMinusBoundaryG[0] = pAd->TssiRefG;
		pAd->TssiPlusBoundaryG[0]  = pAd->TssiRefG;

		// Disable TxAgc if the based value is not right
		if (pAd->TssiRefG == 0xff)
			pAd->bAutoTxAgcG = FALSE;

		DBGPRINT(RT_DEBUG_INFO,"E2PROM: G Tssi[-4 .. +4] = %d %d %d %d - %d -%d %d %d %d, step=%d, tuning=%d\n",
			pAd->TssiMinusBoundaryG[4], pAd->TssiMinusBoundaryG[3],
			pAd->TssiMinusBoundaryG[2], pAd->TssiMinusBoundaryG[1],
			pAd->TssiRefG,
			pAd->TssiPlusBoundaryG[1], pAd->TssiPlusBoundaryG[2],
			pAd->TssiPlusBoundaryG[3], pAd->TssiPlusBoundaryG[4],
			pAd->TxAgcStepG, pAd->bAutoTxAgcG);
	}
	// 1. 11a
	{
		RTUSBReadEEPROM(pAd, EEPROM_A_TSSI_CALIBRAION, ChannelTxPower, 10);
		pAd->TssiMinusBoundaryA[4] = ChannelTxPower[0];
		pAd->TssiMinusBoundaryA[3] = ChannelTxPower[1];
		pAd->TssiMinusBoundaryA[2] = ChannelTxPower[2];
		pAd->TssiMinusBoundaryA[1] = ChannelTxPower[3];
		pAd->TssiPlusBoundaryA[1] = ChannelTxPower[4];
		pAd->TssiPlusBoundaryA[2] = ChannelTxPower[5];
		pAd->TssiPlusBoundaryA[3] = ChannelTxPower[6];
		pAd->TssiPlusBoundaryA[4] = ChannelTxPower[7];
		pAd->TssiRefA	= ChannelTxPower[8];
		pAd->TxAgcStepA = ChannelTxPower[9];
		pAd->TxAgcCompensateA = 0;
		pAd->TssiMinusBoundaryA[0] = pAd->TssiRefA;
		pAd->TssiPlusBoundaryA[0]  = pAd->TssiRefA;

		// Disable TxAgc if the based value is not right
		if (pAd->TssiRefA == 0xff)
			pAd->bAutoTxAgcA = FALSE;

		DBGPRINT(RT_DEBUG_INFO,"E2PROM: A Tssi[-4 .. +4] = %d %d %d %d - %d -%d %d %d %d, step=%d, tuning=%d\n",
			pAd->TssiMinusBoundaryA[4], pAd->TssiMinusBoundaryA[3],
			pAd->TssiMinusBoundaryA[2], pAd->TssiMinusBoundaryA[1],
			pAd->TssiRefA,
			pAd->TssiPlusBoundaryA[1], pAd->TssiPlusBoundaryA[2],
			pAd->TssiPlusBoundaryA[3], pAd->TssiPlusBoundaryA[4],
			pAd->TxAgcStepA, pAd->bAutoTxAgcA);
	}
	pAd->BbpRssiToDbmDelta = 0x79;

	RTUSBReadEEPROM(pAd, EEPROM_FREQ_OFFSET, (PUCHAR)value, 2);
	*value = le16_to_cpu(*value);

	DBGPRINT(RT_DEBUG_INFO, "E2PROM[EEPROM_FREQ_OFFSET]=0x%04x\n", *value);
	if ((*value & 0xFF00) == 0xFF00)
	{
		pAd->RFProgSeq = 0;
	}
	else
	{
		pAd->RFProgSeq = (*value & 0x0300) >> 8;  /* bit 8,9 */
	}

	*value &= 0x00FF;
	if (*value != 0x00FF)
		pAd->RfFreqOffset = (ULONG) *value;
	else
		pAd->RfFreqOffset = 0;
	DBGPRINT(RT_DEBUG_TRACE, "E2PROM: RF freq offset=0x%x\n", pAd->RfFreqOffset);

	//CountryRegion byte offset = 0x25
	*value = pAd->EEPROMDefaultValue[2] >> 8; /* n.b. already flipped
						    - bb */
	value2 = pAd->EEPROMDefaultValue[2] & 0x00FF;
    if ((*value <= REGION_MAXIMUM_BG_BAND) && (value2 <= REGION_MAXIMUM_A_BAND))
	{
		pAd->PortCfg.CountryRegion = ((UCHAR) *value) | 0x80;
		pAd->PortCfg.CountryRegionForABand = ((UCHAR) value2) | 0x80;
	}

	//
	// Get RSSI Offset on EEPROM 0x9Ah & 0x9Ch.
	// The valid value are (-10 ~ 10)
	//
	RTUSBReadEEPROM(pAd, EEPROM_RSSI_BG_OFFSET, (PUCHAR)value, 2);
	*value = le16_to_cpu(*value);
	pAd->BGRssiOffset1 = *value & 0x00ff;
	pAd->BGRssiOffset2 = (*value >> 8);
	DBGPRINT(RT_DEBUG_INFO, "E2PROM[EEPROM_RSSI_BG_OFFSET]=0x%04x\n",
				*value);

	// Validate 11b/g RSSI_1 offset.
	if ((pAd->BGRssiOffset1 < -10) || (pAd->BGRssiOffset1 > 10))
		pAd->BGRssiOffset1 = 0;

	// Validate 11b/g RSSI_2 offset.
	if ((pAd->BGRssiOffset2 < -10) || (pAd->BGRssiOffset2 > 10))
		pAd->BGRssiOffset2 = 0;

	RTUSBReadEEPROM(pAd, EEPROM_RSSI_A_OFFSET, (PUCHAR) value, 2);
	*value = le16_to_cpu(*value);
	DBGPRINT(RT_DEBUG_INFO, "E2PROM[EEPROM_RSSI_A_OFFSET]=0x%04x\n",
				*value);
	pAd->ARssiOffset1 = *value & 0x00ff;
	pAd->ARssiOffset2 = (*value >> 8);

	// Validate 11a RSSI_1 offset.
	if ((pAd->ARssiOffset1 < -10) || (pAd->ARssiOffset1 > 10))
		pAd->ARssiOffset1 = 0;

	//Validate 11a RSSI_2 offset.
	if ((pAd->ARssiOffset2 < -10) || (pAd->ARssiOffset2 > 10))
		pAd->ARssiOffset2 = 0;

	//
	// Get LED Setting.
	//
	RTUSBReadEEPROM(pAd, EEPROM_LED_OFFSET, (PUCHAR)&LedSetting->word, 2);

	DBGPRINT(RT_DEBUG_INFO, "E2PROM[EEPROM_LED_OFFSET]=0x%04x\n",
			LedSetting->word);
	LedSetting->word = le16_to_cpu(LedSetting->word);
	if (LedSetting->word == 0xFFFF)
	{
		//
		// Set it to Default.
		//
		LedSetting->field.PolarityRDY_G = 0;   /* Active High */
		LedSetting->field.PolarityRDY_A = 0;   /* Active High */
		LedSetting->field.PolarityACT = 0;	 /* Active High */
		LedSetting->field.PolarityGPIO_0 = 0; /* Active High */
		LedSetting->field.PolarityGPIO_1 = 0; /* Active High */
		LedSetting->field.PolarityGPIO_2 = 0; /* Active High */
		LedSetting->field.PolarityGPIO_3 = 0; /* Active High */
		LedSetting->field.PolarityGPIO_4 = 0; /* Active High */
		LedSetting->field.LedMode = LED_MODE_DEFAULT;
	}
	pAd->LedCntl.word = 0;
	pAd->LedCntl.field.LedMode = LedSetting->field.LedMode;
	pAd->LedCntl.field.PolarityRDY_G = LedSetting->field.PolarityRDY_G;
	pAd->LedCntl.field.PolarityRDY_A = LedSetting->field.PolarityRDY_A;
	pAd->LedCntl.field.PolarityACT = LedSetting->field.PolarityACT;
	pAd->LedCntl.field.PolarityGPIO_0 = LedSetting->field.PolarityGPIO_0;
	pAd->LedCntl.field.PolarityGPIO_1 = LedSetting->field.PolarityGPIO_1;
	pAd->LedCntl.field.PolarityGPIO_2 = LedSetting->field.PolarityGPIO_2;
	pAd->LedCntl.field.PolarityGPIO_3 = LedSetting->field.PolarityGPIO_3;
	pAd->LedCntl.field.PolarityGPIO_4 = LedSetting->field.PolarityGPIO_4;

	RTUSBReadEEPROM(pAd, EEPROM_TXPOWER_DELTA_OFFSET, (PUCHAR)value, 2);
	*value = le16_to_cpu(*value);
	DBGPRINT(RT_DEBUG_INFO,
		"E2PROM[EEPROM_TXPOWER_DELTA_OFFSET]=0x%04x\n", *value);
	*value = *value & 0x00ff;
	if (*value != 0xff) {
		pAd->TxPowerDeltaConfig.value = (UCHAR) *value;
		if (pAd->TxPowerDeltaConfig.field.DeltaValue > 0x04)
			pAd->TxPowerDeltaConfig.field.DeltaValue = 0x04;
	}
	else
		pAd->TxPowerDeltaConfig.field.TxPowerEnable = FALSE;
	kfree(Version);
	kfree(ChannelTxPower);
	kfree(value);
	kfree(LedSetting);
	DBGPRINT(RT_DEBUG_TRACE, "<-- NICReadEEPROMParameters\n");
}

/*
	========================================================================

	Routine Description:
		Set default value from EEPROM

	Arguments:
		Adapter						Pointer to our adapter

	Return Value:
		None

	Note:

	========================================================================
*/
VOID NICInitAsicFromEEPROM(
	IN	PRTMP_ADAPTER	pAd)
{
	ULONG					data;
	USHORT					i;
	ULONG					MiscMode;
	EEPROM_ANTENNA_STRUC	*Antenna = kzalloc(sizeof(ULONG), GFP_KERNEL);
	EEPROM_NIC_CONFIG2_STRUC	NicConfig2;

	DBGPRINT(RT_DEBUG_TRACE, "--> NICInitAsicFromEEPROM\n");

	for(i = 3; i < NUM_EEPROM_BBP_PARMS; i++)
	{
		UCHAR BbpRegIdx, BbpValue;

		if ((pAd->EEPROMDefaultValue[i] != 0xFFFF) && (pAd->EEPROMDefaultValue[i] != 0))
		{
			BbpRegIdx = (UCHAR)(pAd->EEPROMDefaultValue[i] >> 8);
			BbpValue  = (UCHAR)(pAd->EEPROMDefaultValue[i] & 0xff);
			RTUSBWriteBBPRegister(pAd, BbpRegIdx, BbpValue);
		}
	}

	Antenna->word = pAd->EEPROMDefaultValue[0];

	if (Antenna->word == 0xFFFF)
	{
		Antenna->word = 0;
		Antenna->field.RfIcType = RFIC_5226;
		Antenna->field.HardwareRadioControl = 0; /* no hardware
							    control */
		Antenna->field.DynamicTxAgcControl = 0;
		Antenna->field.FrameType = 0;
		Antenna->field.RxDefaultAntenna = 2; 	/* Ant-B */
		Antenna->field.TxDefaultAntenna = 2; 	/* Ant-B */
		Antenna->field.NumOfAntenna = 2;
		DBGPRINT(RT_DEBUG_WARN,
			"E2PROM error, hard code as 0x%04x\n",
			Antenna->word);
	}

	pAd->RfIcType = (UCHAR) Antenna->field.RfIcType;
	DBGPRINT(RT_DEBUG_WARN, "pAd->RfIcType = %d\n", pAd->RfIcType);

	//
	// For RFIC RFIC_5225 & RFIC_2527
	// Must enable RF RPI mode on PHY_CSR1 bit 16.
	//
	if ((pAd->RfIcType == RFIC_5225) || (pAd->RfIcType == RFIC_2527))
	{
		RTUSBReadMACRegister(pAd, PHY_CSR1, &MiscMode);
		MiscMode |= 0x10000;
		RTUSBWriteMACRegister(pAd, PHY_CSR1, MiscMode);
	}

	// Save the antenna for future use
	pAd->Antenna.word = Antenna->word;

	// Read Hardware controlled Radio state enable bit
	if (Antenna->field.HardwareRadioControl == 1)
	{
		pAd->PortCfg.bHardwareRadio = TRUE;

		// Read GPIO pin7 as Hardware controlled radio state
		RTUSBReadMACRegister(pAd, MAC_CSR13, &data);

		//
		// The GPIO pin7 default is 1:Pull-High, means HW Radio Enable.
		// When the value is 0, means HW Radio disable.
		//
		if ((data & 0x80) == 0)
		{
			pAd->PortCfg.bHwRadio = FALSE;
			// Update extra information to link is up
			pAd->ExtraInfo = HW_RADIO_OFF;
		}
	}
	else
		pAd->PortCfg.bHardwareRadio = FALSE;

	pAd->PortCfg.bRadio = pAd->PortCfg.bSwRadio && pAd->PortCfg.bHwRadio;

	if (pAd->PortCfg.bRadio == FALSE)
	{
		RTUSBWriteMACRegister(pAd, MAC_CSR10, 0x00001818);
		RTMP_SET_FLAG(pAd, fRTMP_ADAPTER_RADIO_OFF);

		RTMPSetLED(pAd, LED_RADIO_OFF);
	}
	else
	{
		RTMPSetLED(pAd, LED_RADIO_ON);
	}

	NicConfig2.word = pAd->EEPROMDefaultValue[1];
	if (NicConfig2.word == 0xffff)
	{
		NicConfig2.word = 0;
	}
	// Save the antenna for future use
	pAd->NicConfig2.word = NicConfig2.word;

	DBGPRINT(RT_DEBUG_TRACE, "Use Hw Radio Control Pin=%d; if used Pin=%d;\n",
		pAd->PortCfg.bHardwareRadio, pAd->PortCfg.bHardwareRadio);

	DBGPRINT(RT_DEBUG_TRACE, "RFIC=%d, LED mode=%d\n", pAd->RfIcType, pAd->LedCntl.field.LedMode);

	pAd->PortCfg.BandState = UNKNOWN_BAND;

	DBGPRINT(RT_DEBUG_TRACE, "<-- NICInitAsicFromEEPROM\n");
}

/*
	========================================================================

	Routine Description:
		Initialize NIC hardware

	Arguments:
		Adapter						Pointer to our adapter

	Return Value:
		None

	Note:

	========================================================================
*/
NDIS_STATUS	NICInitializeAsic(
	IN	PRTMP_ADAPTER	pAd)
{
	ULONG     Index;
	ULONG *Counter = kzalloc(sizeof(ULONG), GFP_KERNEL);
	UCHAR *Value = kzalloc(sizeof(UCHAR), GFP_KERNEL);
	ULONG *Version = kzalloc(sizeof(ULONG), GFP_KERNEL);
	MAC_CSR12_STRUC *MacCsr12 = kzalloc(sizeof(MAC_CSR12_STRUC),
					    GFP_KERNEL);

	if (!Counter || !Value || !Version || !MacCsr12) {
		DBGPRINT(RT_DEBUG_ERROR, "couldn't allocate memory\n");
		return -ENOMEM;
	}

	DBGPRINT(RT_DEBUG_TRACE, "--> NICInitializeAsic ASIC Ver 0x%x\n",
				 *Version);
	*Value = 0xff;
	RTUSBReadMACRegister(pAd, MAC_CSR0, Version);

	// Initialize MAC register to default value
	for (Index = 0; Index < NUM_MAC_REG_PARMS; Index++)
	{
		RTUSBWriteMACRegister(pAd, (USHORT)MACRegTable[Index].Register, MACRegTable[Index].Value);
	}

	// Set Host ready before kicking Rx
	RTUSBWriteMACRegister(pAd, MAC_CSR1, 0x3);
	RTUSBWriteMACRegister(pAd, MAC_CSR1, 0x0);

	//
	// Before program BBP, we need to wait BBP/RF get wake up.
	//
	Index = 0;
	do
	{
		RTUSBReadMACRegister(pAd, MAC_CSR12, &MacCsr12->word);

		if (MacCsr12->field.BbpRfStatus == 1)
			break;

		RTUSBWriteMACRegister(pAd, MAC_CSR12, 0x4); //Force wake up.
		RTMPusecDelay(1000);
	} while (Index++ < 1000);

	// Read BBP register, make sure BBP is up and running before write new data
	Index = 0;
	do
	{
		RTUSBReadBBPRegister(pAd, BBP_R0, Value);
		DBGPRINT(RT_DEBUG_TRACE, "BBP version = %d\n", *Value);
	} while ((++Index < 100) && ((*Value == 0xff) || (*Value == 0x00)));
	// Initialize BBP register to default value
	for (Index = 0; Index < NUM_BBP_REG_PARMS; Index++)
	{
		RTUSBWriteBBPRegister(pAd, BBPRegTable[Index].Register, BBPRegTable[Index].Value);
	}

	// Clear raw counters
	RTUSBReadMACRegister(pAd, STA_CSR0, Counter);
	RTUSBReadMACRegister(pAd, STA_CSR1, Counter);
	RTUSBReadMACRegister(pAd, STA_CSR2, Counter);
	// assert HOST ready bit
	RTUSBWriteMACRegister(pAd, MAC_CSR1, 0x4);

	DBGPRINT(RT_DEBUG_TRACE, "<-- NICInitializeAsic\n");

	kfree(Version);
	kfree(MacCsr12);
	kfree(Counter);
	kfree(Value);
	return NDIS_STATUS_SUCCESS;
}

/*
	========================================================================

	Routine Description:
		Reset NIC Asics

	Arguments:
		Adapter						Pointer to our adapter

	Return Value:
		None

	Note:
		Reset NIC to initial state AS IS system boot up time.

	========================================================================
*/
VOID NICIssueReset(
	IN	PRTMP_ADAPTER	pAd)
{

}

/*
	========================================================================

	Routine Description:
		Check ASIC registers and find any reason the system might hang

	Arguments:
		Adapter						Pointer to our adapter

	Return Value:
		None

	========================================================================
*/
BOOLEAN	NICCheckForHang(
	IN	PRTMP_ADAPTER	pAd)
{
	return (FALSE);
}

/*
	========================================================================

	Routine Description:
		Read statistical counters from hardware registers and record them
		in software variables for later on query

	Arguments:
		pAd					Pointer to our adapter

	Return Value:
		None


	========================================================================
*/
VOID NICUpdateRawCounters(
	IN PRTMP_ADAPTER pAd)
{
	ULONG	OldValue;
	STA_CSR0_STRUC *StaCsr0 = kzalloc(sizeof(STA_CSR0_STRUC), GFP_KERNEL);
	STA_CSR1_STRUC *StaCsr1 = kzalloc(sizeof(STA_CSR1_STRUC), GFP_KERNEL);
	STA_CSR2_STRUC *StaCsr2 = kzalloc(sizeof(STA_CSR2_STRUC), GFP_KERNEL);
	STA_CSR3_STRUC *StaCsr3 = kzalloc(sizeof(STA_CSR3_STRUC), GFP_KERNEL);
	STA_CSR4_STRUC *StaCsr4 = kzalloc(sizeof(STA_CSR4_STRUC), GFP_KERNEL);
	STA_CSR5_STRUC *StaCsr5 = kzalloc(sizeof(STA_CSR5_STRUC), GFP_KERNEL);

	if (!StaCsr0 || !StaCsr1 || !StaCsr2 || !StaCsr3 || !StaCsr4 ||
	    !StaCsr5) {
		DBGPRINT(RT_DEBUG_ERROR, "couldn't allocate memory\n");
		return;
	}
	RTUSBReadMACRegister(pAd, STA_CSR0, &StaCsr0->word);

	// Update RX PLCP error counter
	pAd->PrivateInfo.PhyRxErrCnt += StaCsr0->field.PlcpErr;

	// Update FCS counters
	OldValue= pAd->WlanCounters.FCSErrorCount.vv.LowPart;
	pAd->WlanCounters.FCSErrorCount.vv.LowPart +=
			(StaCsr0->field.CrcErr); //*>> 7); */
	if (pAd->WlanCounters.FCSErrorCount.vv.LowPart < OldValue)
		pAd->WlanCounters.FCSErrorCount.vv.HighPart++;

	// Add FCS error count to private counters
	OldValue = pAd->RalinkCounters.RealFcsErrCount.vv.LowPart;
	pAd->RalinkCounters.RealFcsErrCount.vv.LowPart += StaCsr0->field.CrcErr;
	if (pAd->RalinkCounters.RealFcsErrCount.vv.LowPart < OldValue)
		pAd->RalinkCounters.RealFcsErrCount.vv.HighPart++;


	// Update False CCA counter
	RTUSBReadMACRegister(pAd, STA_CSR1, &StaCsr1->word);
	pAd->RalinkCounters.OneSecFalseCCACnt += StaCsr1->field.FalseCca;

	// Update RX Overflow counter
	RTUSBReadMACRegister(pAd, STA_CSR2, &StaCsr2->word);
	pAd->Counters8023.RxNoBuffer +=
		(StaCsr2->field.RxOverflowCount +
		 StaCsr2->field.RxFifoOverflowCount);

	// Update BEACON sent count
	RTUSBReadMACRegister(pAd, STA_CSR3, &StaCsr3->word);
	pAd->RalinkCounters.OneSecBeaconSentCnt += StaCsr3->field.TxBeaconCount;

	RTUSBReadMACRegister(pAd, STA_CSR4, &StaCsr4->word);
	RTUSBReadMACRegister(pAd, STA_CSR5, &StaCsr5->word);

	// 1st - Transmit Success
	OldValue = pAd->WlanCounters.TransmittedFragmentCount.vv.LowPart;
	pAd->WlanCounters.TransmittedFragmentCount.vv.LowPart +=
		(StaCsr4->field.TxOneRetryCount +
		 StaCsr4->field.TxNoRetryCount +
		 StaCsr5->field.TxMultiRetryCount);
	if (pAd->WlanCounters.TransmittedFragmentCount.vv.LowPart < OldValue)
	{
		pAd->WlanCounters.TransmittedFragmentCount.vv.HighPart++;
	}

	// 2rd	-success and no retry
	OldValue = pAd->WlanCounters.RetryCount.vv.LowPart;
	pAd->WlanCounters.NoRetryCount.vv.LowPart +=
		StaCsr4->field.TxNoRetryCount;
	if (pAd->WlanCounters.NoRetryCount.vv.LowPart < OldValue)
	{
		pAd->WlanCounters.NoRetryCount.vv.HighPart++;
	}

	// 3rd	-success and retry
	OldValue = pAd->WlanCounters.RetryCount.vv.LowPart;
	pAd->WlanCounters.RetryCount.vv.LowPart +=
		(StaCsr4->field.TxOneRetryCount  +
		 StaCsr5->field.TxMultiRetryCount);
	if (pAd->WlanCounters.RetryCount.vv.LowPart < OldValue)
	{
		pAd->WlanCounters.RetryCount.vv.HighPart++;
	}
	// 4th - fail
	OldValue = pAd->WlanCounters.FailedCount.vv.LowPart;
	pAd->WlanCounters.FailedCount.vv.LowPart +=
			StaCsr5->field.TxRetryFailCount;
	if (pAd->WlanCounters.FailedCount.vv.LowPart < OldValue)
	{
		pAd->WlanCounters.FailedCount.vv.HighPart++;
	}


	pAd->RalinkCounters.OneSecTxNoRetryOkCount =
			StaCsr4->field.TxNoRetryCount;
	pAd->RalinkCounters.OneSecTxRetryOkCount =
			StaCsr4->field.TxOneRetryCount +
			StaCsr5->field.TxMultiRetryCount;
	pAd->RalinkCounters.OneSecTxFailCount =
			StaCsr5->field.TxRetryFailCount;
	pAd->RalinkCounters.OneSecFalseCCACnt = StaCsr1->field.FalseCca;
	pAd->RalinkCounters.OneSecRxOkCnt = pAd->RalinkCounters.RxCount;
	pAd->RalinkCounters.RxCount = 0; /* Reset RxCount */
	pAd->RalinkCounters.OneSecRxFcsErrCnt = StaCsr0->field.CrcErr;
	pAd->RalinkCounters.OneSecBeaconSentCnt = StaCsr3->field.TxBeaconCount;

	kfree(StaCsr0);
	kfree(StaCsr1);
	kfree(StaCsr2);
	kfree(StaCsr3);
	kfree(StaCsr4);
	kfree(StaCsr5);
}

/*
	========================================================================

	Routine Description:
		Reset NIC from error

	Arguments:
		Adapter						Pointer to our adapter

	Return Value:
		None

	Note:
		Reset NIC from error state

	========================================================================
*/
VOID NICResetFromError(
	IN	PRTMP_ADAPTER	pAd)
{
	NICInitializeAsic(pAd);
#ifdef	INIT_FROM_EEPROM
	NICInitAsicFromEEPROM(pAd);
#endif
	RTUSBWriteHWMACAddress(pAd);

	// Switch to current channel, since during reset process, the connection
	// should remain on.
	AsicSwitchChannel(pAd, pAd->PortCfg.Channel);
	AsicLockChannel(pAd, pAd->PortCfg.Channel);
}

INT LoadFirmware (PRTMP_ADAPTER pAd, char *firmName)
{
	const struct firmware *fw_entry = NULL;
#ifndef CONFIG_RT73_WIRELESS_FRMW_BUILTIN
	struct usb_device *dev = pAd->pUsb_Dev;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
	char udevice[16];
	snprintf(udevice, sizeof(udevice), "rt73%3.3d%3.3d", dev->bus->busnum, dev->devnum);
#else
	struct device *udevice = &dev->dev;
#endif
#endif

	size_t size;
	u8 *data;
	USHORT i, loaded = 0;
	ULONG *reg = kzalloc(sizeof(ULONG), GFP_KERNEL);
	u16 crc = 0;
	INT status;
#ifdef CONFIG_RT73_WIRELESS_FRMW_BUILTIN
	unsigned char *pFirmwareImage;
	int booted = 0;
	INT ret;
#endif
#define BUFFERED_COPY
#ifdef BUFFERED_COPY
	u8 buf[64];
#else
	u32 buf;
#endif
	DBGPRINT(RT_DEBUG_TRACE, "--> LoadFirmware \n");

	if (!reg) {
		DBGPRINT(RT_DEBUG_ERROR, "couldn't allocate memory\n");
		return -ENOMEM;
	}

#ifdef CONFIG_RT73_WIRELESS_FRMW_BUILTIN
	// Access firmware file
	if ((__initrt73frmw_start != NULL) && (!booted)) {
		pFirmwareImage = kmalloc(MAX_FIRMWARE_IMAGE_SIZE,
				MEM_ALLOC_FLAG);
		if (pFirmwareImage == NULL) {
			DBGPRINT(RT_DEBUG_ERROR, "couldn't allocate memory\n");
			return -ENOMEM;
		}
		memset(pFirmwareImage, 0x00, MAX_FIRMWARE_IMAGE_SIZE);

		memcpy(pFirmwareImage, __initrt73frmw_start,
			FIRMWAREIMAGE_LENGTH);
		for (i = 0; i < FIRMWAREIMAGE_LENGTH; i = i + 4) {
			ret = RTUSBMultiWrite(pAd, FIRMWARE_IMAGE_BASE + i,
						pFirmwareImage + i, 4);
			if (ret < 0) {
				status = NDIS_STATUS_FAILURE;
				break;
			}
		}
		if (pFirmwareImage != NULL)
			kfree(pFirmwareImage);
		/* Send 'run firmware' request to device */
		status = RTUSBFirmwareRun(pAd);
		if (status < 0) {
			KPRINT(KERN_ERR, "Device refuses to run firmware\n");
			return status;
		}
		/* Reset LED */
		RTMPSetLED(pAd, LED_LINK_DOWN);
		/* Firmware loaded ok */
		OPSTATUS_SET_FLAG(pAd, fOP_STATUS_FIRMWARE_LOAD);
		status = NDIS_STATUS_SUCCESS;
		booted = 1;
		DBGPRINT(RT_DEBUG_TRACE, "<-- LoadFirmware (status: %d, loaded:"
				 "%d)\n", status, loaded);
		return status;
	}
#else
	status = request_firmware(&fw_entry, firmName, udevice);
	if (status) {
		KPRINT(KERN_ERR, "Failed to request_firmware. "
		"Check your firmware file location\n");
		kfree(reg);
		return status;
	}
#endif

	if (fw_entry->size != FIRMWARE_IMAGE_SIZE) {
		DBGPRINT(RT_DEBUG_ERROR, "rt73: Firmware file size error "
			"(%d instead of %d)\n",
			(int)fw_entry->size, FIRMWARE_IMAGE_SIZE);
		KPRINT(KERN_ERR, "Firmware file size error "
			"(%d instead of %d)\n",
			(int)fw_entry->size, FIRMWARE_IMAGE_SIZE);
		status = -EBADF;
		goto error;
	}

	// Firmware CRC check
	size = fw_entry->size - 2;
	data = (u8 *)fw_entry->data;

	for (i=0; i < size; i++)
		crc = ByteCRC16(*data++, crc);
	crc = ByteCRC16(0x00, crc);
	crc = ByteCRC16(0x00, crc);

	if (crc != ((fw_entry->data[size] << 8) | fw_entry->data[size + 1])) {
		DBGPRINT(RT_DEBUG_ERROR, "rt73: Firmware CRC error "
				"Check your firmware file integrity\n");
		KPRINT(KERN_ERR, "Firmware CRC error "
				"Check your firmware file integrity\n");
		status = -EBADF;
		goto error;
	}

	// Wait for stable hardware
	for (i = 0; i < 100; i++) {
		RTUSBReadMACRegister(pAd, MAC_CSR0, reg);
		if (reg)
			break;
		msleep(1);
	}

	if (!reg) {
		DBGPRINT(RT_DEBUG_ERROR, "rt73: Unstable hardware\n");
		KPRINT(KERN_ERR, "Unstable hardware\n");
		status = -EBUSY;
		goto error;
	}

	// Write firmware to device
	for (i = 0; i < FIRMWARE_IMAGE_SIZE; i += sizeof(buf)) {
#ifdef BUFFERED_COPY
		memcpy(&buf, &fw_entry->data[i], sizeof(buf));
#else
		buf = *(u32 *) &fw_entry->data[i];
#endif
		if ((status = RTUSBMultiWrite(pAd, FIRMWARE_IMAGE_BASE + i,
						&buf, sizeof(buf))) < 0) {
			DBGPRINT(RT_DEBUG_ERROR, "rt73: Firmware loading error\n");
			KPRINT(KERN_ERR, "Firmware loading error\n");
			goto error;
		}
		loaded += status;
	}
	DBGPRINT(RT_DEBUG_TRACE, "%d bytes written to device.\n", loaded);

	if (loaded < FIRMWARE_IMAGE_SIZE) {
		// Should never happen
		DBGPRINT(RT_DEBUG_ERROR, "rt73: Firmware loading incomplete\n");
		KPRINT(KERN_ERR, "Firmware loading incomplete\n");
		status = -EIO;
		goto error;
	}


	// Send 'run firmware' request to device
	if ((status = RTUSBFirmwareRun(pAd)) < 0) {
		DBGPRINT(RT_DEBUG_ERROR, "rt73: Device refuses to run firmware\n");
		KPRINT(KERN_ERR, "Device refuses to run firmware\n");
		goto error;
	}

	// Reset LED
	RTMPSetLED(pAd, LED_LINK_DOWN);

	// Firmware loaded ok
	OPSTATUS_SET_FLAG (pAd, fOP_STATUS_FIRMWARE_LOAD );
	status = 0;

error:
	release_firmware(fw_entry);

	DBGPRINT(RT_DEBUG_TRACE, "<-- LoadFirmware (status: %d, loaded: %d)\n",
							status, loaded);
	kfree(reg);
	return status;
}

 /**
  * strstr - Find the first substring in a %NUL terminated string
  * @s1: The string to be searched
  * @s2: The string to search for
  */
char * rtstrstr(const char * s1,const char * s2)
{
	INT l1, l2;

	l2 = strlen(s2);
	if (!l2)
		return (char *) s1;

	l1 = strlen(s1);

	while (l1 >= l2)
	{
		l1--;
		if (!memcmp(s1,s2,l2))
			return (char *) s1;
		s1++;
	}

	return NULL;
}

/**
 * rstrtok - Split a string into tokens
 * @s: The string to be searched
 * @ct: The characters to search for
 * * WARNING: strtok is deprecated, use strsep instead. However strsep is not compatible with old architecture.
 */
char * __rstrtok;
char * rstrtok(char * s,const char * ct)
{
	char *sbegin, *send;

	sbegin  = s ? s : __rstrtok;
	if (!sbegin)
	{
		return NULL;
	}

	sbegin += strspn(sbegin,ct);
	if (*sbegin == '\0')
	{
		__rstrtok = NULL;
		return( NULL );
	}

	send = strpbrk( sbegin, ct);
	if (send && *send != '\0')
		*send++ = '\0';

	__rstrtok = send;

	return (sbegin);
}

#ifndef BIG_ENDIAN
/*
	========================================================================

	Routine Description:
		Compare two memory block

	Arguments:
		Adapter 					Pointer to our adapter

	Return Value:
		1:			memory are equal
		0:			memory are not equal

	Note:

	========================================================================
*/
ULONG	RTMPEqualMemory(
	IN	PVOID	pSrc1,
	IN	PVOID	pSrc2,
	IN	ULONG	Length)
{
	PUCHAR	pMem1;
	PUCHAR	pMem2;
	ULONG	Index = 0;

	pMem1 = (PUCHAR) pSrc1;
	pMem2 = (PUCHAR) pSrc2;

	for (Index = 0; Index < Length; Index++)
	{
		if (pMem1[Index] != pMem2[Index])
		{
			break;
		}
	}

	if (Index == Length)
	{
		return (1);
	}
	else
	{
		return (0);
	}
}
#endif
/*
	========================================================================

	Routine Description:
		Compare two memory block

	Arguments:
		pSrc1		Pointer to first memory address
		pSrc2		Pointer to second memory addres

	Return Value:
		0:			memory is equal
		1:			pSrc1 memory is larger
		2:			pSrc2 memory is larger

	Note:

	========================================================================
*/
ULONG	RTMPCompareMemory(
	IN	PVOID	pSrc1,
	IN	PVOID	pSrc2,
	IN	ULONG	Length)
{
	PUCHAR	pMem1;
	PUCHAR	pMem2;
	ULONG	Index = 0;

	pMem1 = (PUCHAR) pSrc1;
	pMem2 = (PUCHAR) pSrc2;

	for (Index = 0; Index < Length; Index++)
	{
		if (pMem1[Index] > pMem2[Index])
			return (1);
		else if (pMem1[Index] < pMem2[Index])
			return (2);
	}

	// Equal
	return (0);
}

/*
	========================================================================

	Routine Description:
		Zero out memory block

	Arguments:
		pSrc1		Pointer to memory address
		Length		Size

	Return Value:
		None

	Note:

	========================================================================
*/
VOID	RTMPZeroMemory(
	IN	PVOID	pSrc,
	IN	ULONG	Length)
{
	memset(pSrc, 0, Length);
}

VOID	RTMPFillMemory(
	IN	PVOID	pSrc,
	IN	ULONG	Length,
	IN	UCHAR	Fill)
{
	memset(pSrc, Fill, Length);
}

/*
	========================================================================

	Routine Description:
		Copy data from memory block 1 to memory block 2

	Arguments:
		pDest		Pointer to destination memory address
		pSrc		Pointer to source memory address
		Length		Copy size

	Return Value:
		None

	Note:

	========================================================================
*/
VOID	RTMPMoveMemory(
	OUT PVOID	pDest,
	IN	PVOID	pSrc,
	IN	ULONG	Length)
{
#ifdef RTMP_EMBEDDED
	if(Length <= 8)
	{
		*(PUCHAR)pDest++ = *(PUCHAR)pSrc++;
		if(--Length == 0)	return;
		*(PUCHAR)pDest++ = *(PUCHAR)pSrc++;
		if(--Length == 0)	return;
		*(PUCHAR)pDest++ = *(PUCHAR)pSrc++;
		if(--Length == 0)	return;
		*(PUCHAR)pDest++ = *(PUCHAR)pSrc++;
		if(--Length == 0)	return;
		*(PUCHAR)pDest++ = *(PUCHAR)pSrc++;
		if(--Length == 0)	return;
		*(PUCHAR)pDest++ = *(PUCHAR)pSrc++;
		if(--Length == 0)	return;
		*(PUCHAR)pDest++ = *(PUCHAR)pSrc++;
		if(--Length == 0)	return;
		*(PUCHAR)pDest++ = *(PUCHAR)pSrc++;
		if(--Length == 0)	return;
	}
	else
		memcpy(pDest, pSrc, Length);
#else
	memcpy(pDest, pSrc, Length);
#endif
}

/*
	========================================================================

	Routine Description:
		Initialize port configuration structure

	Arguments:
		Adapter			Pointer to our adapter

	Return Value:
		None

	Note:

	========================================================================
*/
VOID	PortCfgInit(
	IN	PRTMP_ADAPTER pAd)
{
	UINT i;

	DBGPRINT(RT_DEBUG_TRACE, "--> PortCfgInit\n");

	//
	//	part I. intialize common configuration
	//
	for(i = 0; i < SHARE_KEY_NUM; i++)
	{
		pAd->SharedKey[i].KeyLen = 0;
		pAd->SharedKey[i].CipherAlg = CIPHER_NONE;
	}

	pAd->Antenna.field.TxDefaultAntenna = 2;	// Ant-B
	pAd->Antenna.field.RxDefaultAntenna = 2;	// Ant-B
	pAd->Antenna.field.NumOfAntenna = 2;

	pAd->LedCntl.field.LedMode = LED_MODE_DEFAULT;
	pAd->LedIndicatorStrength = 0;
	pAd->bAutoTxAgcA = FALSE;			// Default is OFF
	pAd->bAutoTxAgcG = FALSE;			// Default is OFF
	pAd->RfIcType = RFIC_5226;

	pAd->PortCfg.Dsifs = 10;	  // in units of usec
	pAd->PortCfg.PrivacyFilter = Ndis802_11PrivFilterAcceptAll;
	pAd->PortCfg.TxPower = 100; //mW
	pAd->PortCfg.TxPowerPercentage = 0xffffffff; // AUTO
	pAd->PortCfg.TxPowerDefault = 0xffffffff; // AUTO
	pAd->PortCfg.TxPreamble = Rt802_11PreambleAuto; // use Long preamble on TX by defaut
	pAd->PortCfg.bUseZeroToDisableFragment = FALSE;
	pAd->PortCfg.RtsThreshold = 2347;
	pAd->PortCfg.FragmentThreshold = 2346;
    pAd->PortCfg.dBmToRoam = 70;    // default threshold used
	pAd->PortCfg.UseBGProtection = 0;	 // 0: AUTO
	pAd->PortCfg.bEnableTxBurst = 0;
	pAd->PortCfg.PhyMode = 0xff;	 // unknown
	pAd->PortCfg.BandState = UNKNOWN_BAND;
	pAd->PortCfg.UseShortSlotTime = TRUE;   // default short slot used, it depends on AP's capability

	pAd->bAcceptDirect = TRUE;
	pAd->bAcceptMulticast = FALSE;
	pAd->bAcceptBroadcast = TRUE;
	pAd->bAcceptAllMulticast = TRUE;
	pAd->bAcceptRFMONTx	= FALSE;

	pAd->bLocalAdminMAC = FALSE; //TRUE;


    pAd->PortCfg.RadarDetect.CSPeriod = 10;
	pAd->PortCfg.RadarDetect.CSCount = 0;
	pAd->PortCfg.RadarDetect.RDMode = RD_NORMAL_MODE;


	//
	// part II. intialize STA specific configuration
	//
	pAd->PortCfg.Psm = PWR_ACTIVE;
	pAd->PortCfg.BeaconPeriod = 100;	 // in mSec

	pAd->PortCfg.ScanCnt = 0;

	pAd->PortCfg.AuthMode = Ndis802_11AuthModeOpen;
	pAd->PortCfg.WepStatus = Ndis802_11EncryptionDisabled;
	pAd->PortCfg.PairCipher = Ndis802_11EncryptionDisabled;
	pAd->PortCfg.GroupCipher = Ndis802_11EncryptionDisabled;
	pAd->PortCfg.bMixCipher = FALSE;
	pAd->PortCfg.DefaultKeyId = 0;

	// 802.1x port control
	pAd->PortCfg.PortSecured = WPA_802_1X_PORT_NOT_SECURED;
	pAd->PortCfg.LastMicErrorTime = 0;
	pAd->PortCfg.MicErrCnt		  = 0;
	pAd->PortCfg.bBlockAssoc	  = FALSE;
	pAd->PortCfg.WpaState		  = SS_NOTUSE;		// Handle by microsoft unless RaConfig changed it.

	pAd->PortCfg.RssiTrigger = 0;
	pAd->PortCfg.LastRssi = 0;
	pAd->PortCfg.LastRssi2 = 0;
	pAd->PortCfg.AvgRssi  = 0;
	pAd->PortCfg.AvgRssiX8 = 0;
	pAd->PortCfg.RssiTriggerMode = RSSI_TRIGGERED_UPON_BELOW_THRESHOLD;
	pAd->PortCfg.AtimWin = 0;
	pAd->PortCfg.DefaultListenCount = 3;//default listen count;
	pAd->PortCfg.BssType = BSS_INFRA;  // BSS_INFRA or BSS_ADHOC
	pAd->PortCfg.AdhocMode = 0;

	// global variables mXXXX used in MAC protocol state machines
	OPSTATUS_SET_FLAG(pAd, fOP_STATUS_RECEIVE_DTIM);
	OPSTATUS_CLEAR_FLAG(pAd, fOP_STATUS_ADHOC_ON);
	OPSTATUS_CLEAR_FLAG(pAd, fOP_STATUS_INFRA_ON);

	// PHY specification
	pAd->PortCfg.PhyMode = PHY_11ABG_MIXED; 	// default PHY mode
	OPSTATUS_CLEAR_FLAG(pAd, fOP_STATUS_SHORT_PREAMBLE_INUSED);  // CCK use LONG preamble

	// user desired power mode
	pAd->PortCfg.WindowsPowerMode = Ndis802_11PowerModeCAM;
	pAd->PortCfg.WindowsBatteryPowerMode = Ndis802_11PowerModeCAM;
	pAd->PortCfg.bWindowsACCAMEnable = FALSE;


	RTMPInitTimer(pAd, &pAd->PortCfg.QuickResponeForRateUpTimer, &StaQuickResponeForRateUpExec);
	pAd->PortCfg.QuickResponeForRateUpTimerRunning = FALSE;


	pAd->PortCfg.bHwRadio  = TRUE; // Default Hardware Radio status is On
	pAd->PortCfg.bSwRadio  = TRUE; // Default Software Radio status is On
	pAd->PortCfg.bRadio    = TRUE; // bHwRadio && bSwRadio
	pAd->PortCfg.bHardwareRadio = FALSE;		// Default is OFF
	pAd->PortCfg.bShowHiddenSSID = FALSE;		// Default no show
	pAd->PortCfg.AdhocMode = 0; // b/g in adhoc

	// Nitro mode control
	pAd->PortCfg.bAutoReconnect = TRUE;

	// Save the init time as last scan time, the system should do scan after 2 seconds.
	// This patch is for driver wake up from standby mode, system will do scan right away.
	pAd->PortCfg.LastScanTime = 0;

	// Default for extra information is not valid
	pAd->ExtraInfo = EXTRA_INFO_CLEAR;

	// Default Config change flag
	pAd->bConfigChanged = FALSE;


	//
	// part III. others
	//
	// dynamic BBP R17:sensibity tuning to overcome background noise
	pAd->BbpTuning.bEnable				  = TRUE;
	pAd->BbpTuning.R17LowerBoundG		  = 0x20; // for best RX sensibility
	pAd->BbpTuning.R17UpperBoundG		  = 0x40; // for best RX noise isolation to prevent false CCA
	pAd->BbpTuning.R17LowerBoundA		  = 0x28; // for best RX sensibility
	pAd->BbpTuning.R17UpperBoundA		  = 0x48; // for best RX noise isolation to prevent false CCA
	pAd->BbpTuning.R17LowerUpperSelect	  = 0;	  // Default used LowerBound.
	pAd->BbpTuning.FalseCcaLowerThreshold = 100;
	pAd->BbpTuning.FalseCcaUpperThreshold = 512;
	pAd->BbpTuning.R17Delta 			  = 4;

    pAd->Bbp94 = BBPR94_DEFAULT;
	pAd->BbpForCCK = FALSE;

//#if WPA_SUPPLICANT_SUPPORT
	pAd->PortCfg.IEEE8021X = 0;
	pAd->PortCfg.IEEE8021x_required_keys = 0;
	pAd->PortCfg.WPA_Supplicant = FALSE;
	pAd->PortCfg.bWscCapable = TRUE;
	pAd->PortCfg.WscIEProbeReq.ValueLen = 0;
	pAd->PortCfg.Send_Beacon = FALSE;
//#endif

	DBGPRINT(RT_DEBUG_TRACE, "<-- PortCfgInit\n");

}

UCHAR BtoH(
	IN CHAR		ch)
{
	if (ch >= '0' && ch <= '9') return (ch - '0');		  // Handle numerals
	if (ch >= 'A' && ch <= 'F') return (ch - 'A' + 0xA);  // Handle capitol hex digits
	if (ch >= 'a' && ch <= 'f') return (ch - 'a' + 0xA);  // Handle small hex digits
	return(255);
}

//
//	PURPOSE:  Converts ascii string to network order hex
//
//	PARAMETERS:
//	  src	 - pointer to input ascii string
//	  dest	 - pointer to output hex
//	  destlen - size of dest
//
//	COMMENTS:
//
//	  2 ascii bytes make a hex byte so must put 1st ascii byte of pair
//	  into upper nibble and 2nd ascii byte of pair into lower nibble.
//
VOID AtoH(
	IN CHAR		*src,
	OUT UCHAR	*dest,
	IN INT		destlen)
{
	CHAR *srcptr;
	PUCHAR destTemp;

	srcptr = src;
	destTemp = (PUCHAR) dest;

	while(destlen--)
	{
		*destTemp = BtoH(*srcptr++) << 4;	 // Put 1st ascii byte in upper nibble.
		*destTemp += BtoH(*srcptr++);	   // Add 2nd ascii byte to above.
		destTemp++;
	}
}

VOID	RTMPPatchMacBbpBug(
	IN	PRTMP_ADAPTER	pAd)
{
#if 0
	ULONG	Index;

	// Initialize BBP register to default value
	for (Index = 0; Index < NUM_BBP_REG_PARMS; Index++)
	{
		RTUSBWriteBBPRegister(pAd, BBPRegTable[Index].Register, (UCHAR)BBPRegTable[Index].Value);
	}

	// Initialize RF register to default value
	AsicSwitchChannel(pAd, pAd->PortCfg.Channel);
	AsicLockChannel(pAd, pAd->PortCfg.Channel);

	// Re-init BBP register from EEPROM value
	NICInitAsicFromEEPROM(pAd);
#endif
}

// Unify all delay routine by using udelay
VOID	RTMPusecDelay(
	IN	ULONG	usec)
{
	ULONG	i;

	for (i = 0; i < (usec / 50); i++)
		udelay(50);

	if (usec % 50)
		udelay(usec % 50);
}

/*
	========================================================================

	Routine Description:
		Set LED Status

	Arguments:
		pAd						Pointer to our adapter
		Status					LED Status

	Return Value:
		None

	Note:

	========================================================================
*/
VOID	RTMPSetLED(
	IN PRTMP_ADAPTER	pAd,
	IN UCHAR			Status)
{
	switch (Status)
	{
		case LED_LINK_DOWN:
			pAd->LedCntl.field.LinkGStatus = 0;
			pAd->LedCntl.field.LinkAStatus = 0;
			pAd->LedIndicatorStrength = 0;
			RTUSBSetLED(pAd, pAd->LedCntl, pAd->LedIndicatorStrength);
			break;
		case LED_LINK_UP:
			if (pAd->PortCfg.Channel <= 14)
			{
				// 11 G mode
				pAd->LedCntl.field.LinkGStatus = 1;
				pAd->LedCntl.field.LinkAStatus = 0;
			}
			else
			{
				//11 A mode
				pAd->LedCntl.field.LinkGStatus = 0;
				pAd->LedCntl.field.LinkAStatus = 1;
			}

			RTUSBSetLED(pAd, pAd->LedCntl, pAd->LedIndicatorStrength);
			break;
		case LED_RADIO_ON:
			pAd->LedCntl.field.RadioStatus = 1;
			RTUSBSetLED(pAd, pAd->LedCntl, pAd->LedIndicatorStrength);
			break;
		case LED_HALT:
			//Same as Radio Off.
		case LED_RADIO_OFF:
			pAd->LedCntl.field.RadioStatus = 0;
			pAd->LedCntl.field.LinkGStatus = 0;
			pAd->LedCntl.field.LinkAStatus = 0;
			pAd->LedIndicatorStrength = 0;
			RTUSBSetLED(pAd, pAd->LedCntl, pAd->LedIndicatorStrength);
			break;
		default:
			DBGPRINT(RT_DEBUG_WARN, "RTMPSetLED::Unknown Status %d\n", Status);
			break;
	}
}

/*
	========================================================================

	Routine Description:
		Set LED Signal Stregth

	Arguments:
		pAd						Pointer to our adapter
		Dbm						Signal Stregth

	Return Value:
		None

	Note:
		Can be run on any IRQL level.

		According to Microsoft Zero Config Wireless Signal Stregth definition as belows.
		<= -90	No Signal
		<= -81	Very Low
		<= -71	Low
		<= -67	Good
		<= -57	Very Good
		 > -57	Excellent
	========================================================================
*/
VOID RTMPSetSignalLED(
	IN PRTMP_ADAPTER	pAd,
	IN NDIS_802_11_RSSI Dbm)
{
	USHORT		nLed = 0;

	if (Dbm <= -90)
		nLed = 0;
	else if (Dbm <= -81)
		nLed = 1;
	else if (Dbm <= -71)
		nLed = 2;
	else if (Dbm <= -67)
		nLed = 3;
	else if (Dbm <= -57)
		nLed = 4;
	else
		nLed = 5;

	//
	// Update Signal Stregth to if changed.
	//
	if ((pAd->LedIndicatorStrength != nLed) &&
		(pAd->LedCntl.field.LedMode == LED_MODE_SIGNAL_STREGTH))
	{
		pAd->LedIndicatorStrength = nLed;
		RTUSBSetLED(pAd, pAd->LedCntl, pAd->LedIndicatorStrength);
	}
}

VOID RTMPCckBbpTuning(
	IN	PRTMP_ADAPTER	pAd,
	IN	UINT			TxRate)
{
	CHAR		Bbp94 = 0xFF;

	//
	// Do nothing if TxPowerEnable == FALSE
	//
	if (pAd->TxPowerDeltaConfig.field.TxPowerEnable == FALSE)
		return;

	if ((TxRate < RATE_FIRST_OFDM_RATE) &&
		(pAd->BbpForCCK == FALSE))
	{
		Bbp94 = pAd->Bbp94;

		if (pAd->TxPowerDeltaConfig.field.Type == 1)
		{
			Bbp94 += pAd->TxPowerDeltaConfig.field.DeltaValue;
		}
		else
		{
			Bbp94 -= pAd->TxPowerDeltaConfig.field.DeltaValue;
		}
		pAd->BbpForCCK = TRUE;
	}
	else if ((TxRate >= RATE_FIRST_OFDM_RATE) &&
		(pAd->BbpForCCK == TRUE))
	{
		Bbp94 = pAd->Bbp94;
		pAd->BbpForCCK = FALSE;
	}

	if ((Bbp94 >= 0) && (Bbp94 <= 0x0C))
	{
		// sb safe, because we're now in process context.
		RTUSBWriteBBPRegister(pAd, BBP_R94, Bbp94);
	}
}

/*
	========================================================================

	Routine Description:
		Init timer objects

	Arguments:
		pAd			Pointer to our adapter
		pTimer				Timer structure
		pTimerFunc			Function to execute when timer expired

	Return Value:
		None

	Note:

	========================================================================
*/
VOID	RTMPInitTimer(
	IN	PRTMP_ADAPTER			pAd,
	IN	PRALINK_TIMER_STRUCT	pTimer,
	IN	PVOID					pTimerFunc)
{
	init_timer(&pTimer->Timer);
	pTimer->Timer.data = (unsigned long)pAd;
	pTimer->Timer.function = pTimerFunc;

}

/*
	========================================================================

	Routine Description:
		Init timer objects

	Arguments:
		pAd			Pointer to our adapter
		pTimer				Timer structure
		Value				Timer value in milliseconds

	Return Value:
		None

	Note:

	========================================================================
*/
VOID	RTMPSetTimer(
	IN	PRTMP_ADAPTER			pAd,
	IN	PRALINK_TIMER_STRUCT	pTimer,
	IN	ULONG					Value)
{
	//
	// We should not set a timer when driver is on Halt state.
	//
	if (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_HALT_IN_PROGRESS))
		return;
	pTimer->Timer.expires = jiffies + (Value * HZ)/1000;
	add_timer(&pTimer->Timer);
}

/*
	========================================================================

	Routine Description:
		Cancel timer objects

	Arguments:
		pTimer				Timer structure

	Return Value:
		None

	Note:
		Reset NIC to initial state AS IS system boot up time.

	========================================================================
*/
INT	RTMPCancelTimer(
	IN	PRALINK_TIMER_STRUCT	pTimer)
{
	// reset timer if caller isn't the timer function itself
	if (timer_pending(&pTimer->Timer))
		return del_timer_sync(&pTimer->Timer);
	return 0;
}


