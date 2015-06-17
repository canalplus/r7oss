/***************************************************************************
 *
 * Copyright (C) 2004-2005  SMSC
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 ***************************************************************************
 * File: st40.c
 *
 * 03/18/2005 Phong Le, rev 1
 * Make this driver to work with Lan911x driver Version 1.13
 *
 * 03/22/2005 Bryan Whitehead, rev 2
 * Added support for 16/32 bit autodetect
 *
 * 04/11/2005 Bryan Whitehead, rev 3
 *    updated platform code to support version 1.14 platform changes
 *
 * 05/02/2005 Phong Le, rev 4
 * Make this driver to work with Lan911x driver Version 1.15
 *
 ***************************************************************************
 * NOTE: When making changes to platform specific code please remember to
 *   update the revision number in the PLATFORM_NAME macro. This is a
 *   platform specific version number and independent of the
 *   common code version number. The PLATFORM_NAME macro should be found in
 *   your platforms ".h" file.
 */

#ifndef ST40_H
#define ST40_H


//for a description of these MACROs see readme.txt

/* Following informations are now passed as platform device resources
 * (see Smsc9118_probe() in smsc9118.c) */
#define PLATFORM_CSBASE -1
#define PLATFORM_IRQ -1
#define PLATFORM_IRQ_POL -1
#define PLATFORM_IRQ_TYPE -1

#define PLATFORM_CACHE_LINE_BYTES (32UL)
#ifndef CONFIG_SMSC911x_DMA_NONE
#define PLATFORM_RX_DMA	(TRANSFER_REQUEST_DMA)
#define PLATFORM_TX_DMA	(TRANSFER_REQUEST_DMA)
#else
#define PLATFORM_RX_DMA	(TRANSFER_PIO)
#define PLATFORM_TX_DMA	(TRANSFER_PIO)
#endif
#define PLATFORM_NAME		"ST40 STMICRO r3"

//the dma threshold has not been thoroughly tuned but it is
//  slightly better than using zero
#define PLATFORM_DMA_THRESHOLD (200)

typedef struct _PLATFORM_DATA {
	DWORD dwBitWidth;
	DWORD dwIdRev;
	DWORD dwIrq;
	void * dev_id;
} PLATFORM_DATA, *PPLATFORM_DATA;

inline void Platform_SetRegDW(
		DWORD dwLanBase,
		DWORD dwOffset,
		DWORD dwVal)
{
	(*(volatile DWORD *)(dwLanBase+dwOffset))=dwVal;
}

inline DWORD Platform_GetRegDW(
	DWORD dwLanBase,
	DWORD dwOffset)
{
	return (*(volatile DWORD *)(dwLanBase+dwOffset));
}

//See readme.txt for a description of how these
//functions must be implemented
DWORD Platform_Initialize(
	PPLATFORM_DATA platformData,
	DWORD dwLanBase,
	DWORD dwBusWidth);
void Platform_CleanUp(
	PPLATFORM_DATA platformData);
BOOLEAN Platform_Is16BitMode(
	PPLATFORM_DATA platformData);
BOOLEAN Platform_RequestIRQ(
	PPLATFORM_DATA platformData,
	DWORD dwIrq,
	irqreturn_t (*pIsr)(int irq,void *dev_id),
	void *dev_id);
DWORD Platform_CurrentIRQ(
	PPLATFORM_DATA platformData);
void Platform_FreeIRQ(
	PPLATFORM_DATA platformData);
BOOLEAN Platform_IsValidDmaChannel(DWORD dwDmaCh);
BOOLEAN Platform_DmaInitialize(
	PPLATFORM_DATA platformData,
	DWORD dwDmaCh);
BOOLEAN Platform_DmaDisable(
	PPLATFORM_DATA platformData,
	const DWORD dwDmaCh);
void Platform_CacheInvalidate(
	PPLATFORM_DATA platformData,
	const void * const pStartAddress,
	const DWORD dwLengthInBytes);
void Platform_CachePurge(
	PPLATFORM_DATA platformData,
	const void * const pStartAddress,
	const DWORD dwLengthInBytes);
DWORD Platform_RequestDmaChannel(
	PPLATFORM_DATA platformData);
void Platform_ReleaseDmaChannel(
	PPLATFORM_DATA platformData,
	DWORD dwDmaChannel);
BOOLEAN Platform_DmaStartXfer(
	PPLATFORM_DATA platformData,
	const DMA_XFER * const pDmaXfer,
	void (*pCallback)(void*), void* pCallbackData);
DWORD Platform_DmaGetDwCnt(
	PPLATFORM_DATA platformData,
	const DWORD dwDmaCh);
void Platform_DmaComplete(
	PPLATFORM_DATA platformData,
	const DWORD dwDmaCh);
void Platform_GetFlowControlParameters(
	PPLATFORM_DATA platformData,
	PFLOW_CONTROL_PARAMETERS flowControlParameters,
	BOOLEAN useDma);
void Platform_WriteFifo(
	DWORD dwLanBase,
	DWORD *pdwBuf,
	DWORD dwDwordCount);
void Platform_ReadFifo(
	DWORD dwLanBase,
	DWORD *pdwBuf,
	DWORD dwDwordCount);


#endif

static const char date_code[] = "072605";

/* SMSC LAN9118 Byte ordering test register offset */
#define BYTE_TEST_OFFSET	(0x64UL)
#define ID_REV_OFFSET		(0x50UL)

#define CpuToPhysicalAddr(cpuAddr) ((DWORD)cpuAddr)

DWORD Platform_Initialize(
	PPLATFORM_DATA platformData,
	DWORD dwLanBase, DWORD dwBusWidth)
{
	SMSC_ASSERT(platformData!=NULL);
	platformData->dwBitWidth=0;

	if(dwLanBase==0x0UL) {
		dwLanBase=PLATFORM_CSBASE;
	}

	platformData->dwBitWidth=16;

	return dwLanBase;
}

void Platform_CleanUp(
	PPLATFORM_DATA platformData)
{
}

BOOLEAN Platform_Is16BitMode(
	PPLATFORM_DATA platformData)
{
	SMSC_ASSERT(platformData != NULL);
	if (platformData->dwBitWidth == 16) {
		return TRUE;
	}
	return FALSE;
}

BOOLEAN Platform_RequestIRQ(
	PPLATFORM_DATA platformData,
	DWORD dwIrq,
	irqreturn_t (*pIsr)(int,void *),
	void * dev_id)
{
	SMSC_ASSERT(platformData != NULL);
	SMSC_ASSERT(platformData->dev_id == NULL);
	if (request_irq(
		dwIrq,
		pIsr,
		0,
		"SMSC_LAN9118_ISR",
		dev_id) != 0)
	{
		SMSC_WARNING("Unable to use IRQ = %ld", dwIrq);
		return FALSE;
	}
	platformData->dwIrq = dwIrq;
	platformData->dev_id = dev_id;
	return TRUE;
}

DWORD Platform_CurrentIRQ(
	PPLATFORM_DATA platformData)
{
	SMSC_ASSERT(platformData != NULL);
	return platformData->dwIrq;
}

void Platform_FreeIRQ(
	PPLATFORM_DATA platformData)
{
	SMSC_ASSERT(platformData != NULL);
	SMSC_ASSERT(platformData->dev_id != NULL);

	free_irq(platformData->dwIrq, platformData->dev_id);

	platformData->dwIrq = 0;
	platformData->dev_id = NULL;
}

void Platform_CacheInvalidate(
	PPLATFORM_DATA platformData,
	const void * const pStartAddress,
	const DWORD dwLengthInBytes)
{
	dma_cache_inv((void *)pStartAddress, (dwLengthInBytes));
}

void Platform_CachePurge(
	PPLATFORM_DATA platformData,
	const void * const pStartAddress,
	const DWORD dwLengthInBytes)
{
	dma_cache_wback_inv((void *)pStartAddress, (dwLengthInBytes));
}

void Platform_GetFlowControlParameters(
	PPLATFORM_DATA platformData,
	PFLOW_CONTROL_PARAMETERS flowControlParameters,
	BOOLEAN useDma)
{
	memset(flowControlParameters,0,sizeof(FLOW_CONTROL_PARAMETERS));
	flowControlParameters->BurstPeriod=100;
	flowControlParameters->IntDeas=0;
	if(useDma) {
		if(Platform_Is16BitMode(platformData)) {
			switch(platformData->dwIdRev&0xFFFF0000) {
			case 0x01180000UL:
			case 0x01170000UL:
			case 0x01120000UL:
				//117/118,16 bit,DMA
				flowControlParameters->MaxThroughput=(0xEAF0CUL);
				flowControlParameters->MaxPacketCount=(0x282UL);
				flowControlParameters->PacketCost=(0xC2UL);
				flowControlParameters->BurstPeriod=(0x66UL);
				flowControlParameters->IntDeas=(40UL);
				break;
			case 0x01160000UL:
			case 0x01150000UL:
				//115/116,16 bit,DMA
				flowControlParameters->MaxThroughput=0xB3A3CUL;
				flowControlParameters->MaxPacketCount=0x1E6UL;
				flowControlParameters->PacketCost=0xF4UL;
				flowControlParameters->BurstPeriod=0x26UL;
				flowControlParameters->IntDeas=40UL;
				break;
			default:break;//make lint happy
			}
		} else {
			/* st40 DMA now only support 16-bit, not 32-bit */
			switch(platformData->dwIdRev&0xFFFF0000) {
			case 0x01180000UL:
			case 0x01170000UL:
			case 0x01120000UL:
				//117/118,32 bit,DMA
				flowControlParameters->MaxThroughput=(0xC7F82UL);
				flowControlParameters->MaxPacketCount=(0x21DUL);
				flowControlParameters->PacketCost=(0x17UL);
				flowControlParameters->BurstPeriod=(0x1EUL);
				flowControlParameters->IntDeas=(0x17UL);
				break;
			case 0x01160000UL:
			case 0x01150000UL:
				//115/116,32 bit,DMA
				flowControlParameters->MaxThroughput=0xABE0AUL;
				flowControlParameters->MaxPacketCount=0x1D1UL;
				flowControlParameters->PacketCost=0x00UL;
				flowControlParameters->BurstPeriod=0x30UL;
				flowControlParameters->IntDeas=0x0A;
				break;
			default:break;//make lint happy
			}
		}
	} else {
		if(Platform_Is16BitMode(platformData)) {
			switch(platformData->dwIdRev&0xFFFF0000) {
			case 0x01180000UL:
			case 0x01170000UL:
			case 0x01120000UL:
				//117/118,16 bit,PIO
				flowControlParameters->MaxThroughput=(0xA0C9EUL);
				flowControlParameters->MaxPacketCount=(0x1B3UL);
				flowControlParameters->PacketCost=(0x1C4UL);
				flowControlParameters->BurstPeriod=(0x5CUL);
				flowControlParameters->IntDeas=(60UL);
				break;
			case 0x01160000UL:
			case 0x01150000UL:
				//115/116,16 bit,PIO
				flowControlParameters->MaxThroughput=(0x76A6AUL);
				flowControlParameters->MaxPacketCount=(0x141UL);
				flowControlParameters->PacketCost=(0x11AUL);
				flowControlParameters->BurstPeriod=(0x77UL);
				flowControlParameters->IntDeas=(70UL);
				break;
			default:break;//make lint happy
			}
		} else {
			/* st40 PIO now only support 16-bit, not 32-bit */
			switch(platformData->dwIdRev&0xFFFF0000) {
			case 0x01180000UL:
			case 0x01170000UL:
			case 0x01120000UL:
				//117/118,32 bit,PIO
				flowControlParameters->MaxThroughput=(0xAE5C8UL);
				flowControlParameters->MaxPacketCount=(0x1D8UL);
				flowControlParameters->PacketCost=(0UL);
				flowControlParameters->BurstPeriod=(0x57UL);
				flowControlParameters->IntDeas=(0x14UL);
				break;
			case 0x01160000UL:
			case 0x01150000UL:
				//115/116,32 bit,PIO
				flowControlParameters->MaxThroughput=(0x9E338UL);
				flowControlParameters->MaxPacketCount=(0x1ACUL);
				flowControlParameters->PacketCost=(0xD2UL);
				flowControlParameters->BurstPeriod=(0x60UL);
				flowControlParameters->IntDeas=(0x0EUL);
				break;
			default:break;//make lint happy
			}
		}
	}
}

#if 0
void Platform_WriteFifo(
	DWORD dwLanBase,
	DWORD *pdwBuf,
	DWORD dwDwordCount)
{
	volatile DWORD *pdwReg;
	pdwReg = (volatile DWORD *)(dwLanBase+TX_DATA_FIFO);
	while(dwDwordCount)
	{
		*pdwReg = *pdwBuf++;
		dwDwordCount--;
	}
}
void Platform_ReadFifo(
	DWORD dwLanBase,
	DWORD *pdwBuf,
	DWORD dwDwordCount)
{
	const volatile DWORD * const pdwReg =
		(const volatile DWORD * const)(dwLanBase+RX_DATA_FIFO);

	while (dwDwordCount)
	{
		*pdwBuf++ = *pdwReg;
		dwDwordCount--;
	}
}
#else
#include <asm/io.h>
void Platform_WriteFifo(
	DWORD dwLanBase,
	DWORD *pdwBuf,
	DWORD dwDwordCount)
{
	writesl((void *)(dwLanBase + TX_DATA_FIFO), pdwBuf, dwDwordCount);
}
void Platform_ReadFifo(
	DWORD dwLanBase,
	DWORD *pdwBuf,
	DWORD dwDwordCount)
{
	readsl((void *)(dwLanBase + RX_DATA_FIFO), pdwBuf, dwDwordCount);
}
#endif

#ifdef CONFIG_SMSC911x_DMA_NONE
DWORD Platform_RequestDmaChannel(PPLATFORM_DATA platformData)
{ return TRANSFER_REQUEST_DMA; }

DWORD Platform_RequestDmaChannelSg(PPLATFORM_DATA platformData)
{ return TRANSFER_REQUEST_DMA; }

void Platform_ReleaseDmaChannel(PPLATFORM_DATA platformData, DWORD dwDmaChannel)
{ }

BOOLEAN Platform_IsValidDmaChannel(DWORD dwDmaCh)
{ return FALSE; }

BOOLEAN Platform_DmaInitialize(
	PPLATFORM_DATA platformData,
	DWORD dwDmaCh)
{ return FALSE; }

BOOLEAN Platform_DmaStartXfer(
	PPLATFORM_DATA platformData,
	const DMA_XFER * const pDmaXfer,
	void (*pCallback)(void*),
	void* pCallbackData)
{ return FALSE; }

BOOLEAN Platform_DmaStartSgXfer(
	PPLATFORM_DATA platformData,
	const DMA_XFER * const pDmaXfer,
	void (*pCallback)(void*),
	void* pCallbackData)
{ return FALSE; }
#else
#ifdef CONFIG_STM_DMA
#include "st40-shdma.c"
#else
#include "st40-gpdma.c"
#endif
#endif
