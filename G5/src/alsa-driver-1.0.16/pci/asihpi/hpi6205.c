
/******************************************************************************

    AudioScience HPI driver
    Copyright (C) 1997-2003  AudioScience Inc. <support@audioscience.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of version 2 of the GNU General Public License as
    published by the Free Software Foundation;

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

 Hardware Programming Interface (HPI) for AudioScience ASI5000
 series adapters.
 These PCI bus adapters are based on a TI C6205 PCI bus mastering DSP

 Exported functions:
 void HPI_6205(struct hpi_message *phm, struct hpi_response *phr)

(C) Copyright AudioScience Inc. 1998-2003
*******************************************************************************/
#define SOURCEFILE_NAME "hpi6205.c"
#include "hpi.h"
#include "hpidebug.h"
#include "hpi6205.h"
#include "hpidspcd.h"
#include "hpicmn.h"

/*****************************************************************************/
/* HPI6205 specific error codes */
#define HPI6205_ERROR_BASE		    1000
#define HPI6205_ERROR_MEM_ALLOC		    1001
#define HPI6205_ERROR_6205_NO_IRQ	    1002
#define HPI6205_ERROR_6205_INIT_FAILED	    1003
/*#define HPI6205_ERROR_MISSING_DSPCODE 1004 */
#define HPI6205_ERROR_UNKNOWN_PCI_DEVICE   1005
#define HPI6205_ERROR_6205_REG				1006
#define HPI6205_ERROR_6205_DSPPAGE			1007
#define HPI6205_ERROR_BAD_DSPINDEX			1008
#define HPI6205_ERROR_C6713_HPIC			1009
#define HPI6205_ERROR_C6713_HPIA			1010
#define HPI6205_ERROR_C6713_PLL				1011
#define HPI6205_ERROR_DSP_INTMEM	    1012
#define HPI6205_ERROR_DSP_EXTMEM			1013
#define HPI6205_ERROR_DSP_PLD		    1014
#define HPI6205_ERROR_MSG_RESP_IDLE_TIMEOUT 1015
#define HPI6205_ERROR_MSG_RESP_TIMEOUT		1016
#define HPI6205_ERROR_6205_EEPROM			1017
#define HPI6205_ERROR_DSP_EMIF				1018

#define Hpi6205_Error(nDspIndex, err) (err)
/*****************************************************************************/
/* for C6205 PCI i/f */
/* Host Status Register (HSR) bitfields */
#define C6205_HSR_INTSRC	0x01
#define C6205_HSR_INTAVAL	0x02
#define C6205_HSR_INTAM		0x04
#define C6205_HSR_CFGERR	0x08
#define C6205_HSR_EEREAD	0x10
/* Host-to-DSP Control Register (HDCR) bitfields */
#define C6205_HDCR_WARMRESET	0x01
#define C6205_HDCR_DSPINT		0x02
#define C6205_HDCR_PCIBOOT		0x04
/* DSP Page Register (DSPP) bitfields, */
/* defines 4 Mbyte page that BAR0 points to */
#define C6205_DSPP_MAP1		0x400

/* BAR0 maps to prefetchable 4 Mbyte memory block set by DSPP.
 * BAR1 maps to non-prefetchable 8 Mbyte memory block
 * of DSP memory mapped registers (starting at 0x01800000).
 * 0x01800000 is hardcoded in the PCI i/f, so that only the offset from this
 * needs to be added to the BAR1 base address set in the PCI config reg
 */
#define C6205_BAR1_PCI_IO_OFFSET (0x027FFF0L)
#define C6205_BAR1_HSR (C6205_BAR1_PCI_IO_OFFSET)
#define C6205_BAR1_HDCR (C6205_BAR1_PCI_IO_OFFSET+4)
#define C6205_BAR1_DSPP (C6205_BAR1_PCI_IO_OFFSET+8)

/* used to control LED (revA) and reset C6713 (revB) */
#define C6205_BAR0_TIMER1_CTL (0x01980000L)

/* For first 6713 in CE1 space, using DA17,16,2 */
#define HPICL_ADDR		0x01400000L
#define HPICH_ADDR	0x01400004L
#define HPIAL_ADDR	0x01410000L
#define HPIAH_ADDR	0x01410004L
#define HPIDIL_ADDR	0x01420000L
#define HPIDIH_ADDR	0x01420004L
#define HPIDL_ADDR	0x01430000L
#define HPIDH_ADDR	0x01430004L

#define C6713_EMIF_GCTL			0x01800000
#define C6713_EMIF_CE1			0x01800004
#define C6713_EMIF_CE0		0x01800008
#define C6713_EMIF_CE2		0x01800010
#define C6713_EMIF_CE3		0x01800014
#define C6713_EMIF_SDRAMCTL	0x01800018
#define C6713_EMIF_SDRAMTIMING	0x0180001C
#define C6713_EMIF_SDRAMEXT	0x01800020

struct hpi_hw_obj {
	/* PCI registers */
	__iomem u32 *prHSR;
	__iomem u32 *prHDCR;
	__iomem u32 *prDSPP;

	u32 dwDspPage;

	struct consistent_dma_area hLockedMem;
	volatile struct bus_master_interface *pInterfaceBuffer;

	u16 flagOStreamJustReset[HPI_MAX_STREAMS];
	/* a non-NULL handle means there is an HPI allocated buffer */
	struct consistent_dma_area InStreamHostBuffers[HPI_MAX_STREAMS];
	struct consistent_dma_area OutStreamHostBuffers[HPI_MAX_STREAMS];
	/* non-zero size means a buffer exists, may be external */
	u32 InStreamHostBufferSize[HPI_MAX_STREAMS];
	u32 OutStreamHostBufferSize[HPI_MAX_STREAMS];

	struct consistent_dma_area hControlCache;
	struct consistent_dma_area hAsyncEventBuffer;
	struct hpi_control_cache_single *pControlCache;
	struct hpi_async_event *pAsyncEventBuffer;
};

/*****************************************************************************/
/* local prototypes */

#define CheckBeforeBBMCopy(status, pBBMData, lFirstWrite, lSecondWrite)

static u16 Hpi6205_AdapterBootLoadDsp(
	struct hpi_adapter_obj *pao,
	u32 *pdwOsErrorCode
);
static u16 Hpi6205_MessageResponseSequence(
	struct hpi_adapter_obj *pao,
	struct hpi_message *phm,
	struct hpi_response *phr
);
static void HW_Message(
	struct hpi_adapter_obj *pao,
	struct hpi_message *phm,
	struct hpi_response *phr
);

#define HPI6205_TIMEOUT 1000000

static void SubSysCreateAdapter(
	struct hpi_message *phm,
	struct hpi_response *phr
);
static void SubSysDeleteAdapter(
	struct hpi_message *phm,
	struct hpi_response *phr
);

static void AdapterGetAsserts(
	struct hpi_adapter_obj *pao,
	struct hpi_message *phm,
	struct hpi_response *phr
);

static u16 CreateAdapterObj(
	struct hpi_adapter_obj *pao,
	u32 *pdwOsErrorCode
);
static void DeleteAdapterObj(
	struct hpi_adapter_obj *pao
);

static void OutStreamHostBufferAllocate(
	struct hpi_adapter_obj *pao,
	struct hpi_message *phm,
	struct hpi_response *phr
);
static void OutStreamHostBufferFree(
	struct hpi_adapter_obj *pao,
	struct hpi_message *phm,
	struct hpi_response *phr
);
static void OutStreamWrite(
	struct hpi_adapter_obj *pao,
	struct hpi_message *phm,
	struct hpi_response *phr
);
static void OutStreamGetInfo(
	struct hpi_adapter_obj *pao,
	struct hpi_message *phm,
	struct hpi_response *phr
);
static void OutStreamStart(
	struct hpi_adapter_obj *pao,
	struct hpi_message *phm,
	struct hpi_response *phr
);
static void OutStreamOpen(
	struct hpi_adapter_obj *pao,
	struct hpi_message *phm,
	struct hpi_response *phr
);
static void OutStreamReset(
	struct hpi_adapter_obj *pao,
	struct hpi_message *phm,
	struct hpi_response *phr
);

static void InStreamHostBufferAllocate(
	struct hpi_adapter_obj *pao,
	struct hpi_message *phm,
	struct hpi_response *phr
);
static void InStreamHostBufferFree(
	struct hpi_adapter_obj *pao,
	struct hpi_message *phm,
	struct hpi_response *phr
);
static void InStreamRead(
	struct hpi_adapter_obj *pao,
	struct hpi_message *phm,
	struct hpi_response *phr
);
static void InStreamGetInfo(
	struct hpi_adapter_obj *pao,
	struct hpi_message *phm,
	struct hpi_response *phr
);
static void InStreamStart(
	struct hpi_adapter_obj *pao,
	struct hpi_message *phm,
	struct hpi_response *phr
);

static u32 BootLoader_ReadMem32(
	struct hpi_adapter_obj *pao,
	int nDSPIndex,
	u32 dwAddress
);
static u16 BootLoader_WriteMem32(
	struct hpi_adapter_obj *pao,
	int nDSPIndex,
	u32 dwAddress,
	u32 dwData
);
static u16 BootLoader_ConfigEMIF(
	struct hpi_adapter_obj *pao,
	int nDSPIndex
);
static u16 BootLoader_TestMemory(
	struct hpi_adapter_obj *pao,
	int nDSPIndex,
	u32 dwAddress,
	u32 dwLength
);
static u16 BootLoader_TestInternalMemory(
	struct hpi_adapter_obj *pao,
	int nDSPIndex
);
static u16 BootLoader_TestExternalMemory(
	struct hpi_adapter_obj *pao,
	int nDSPIndex
);
static u16 BootLoader_TestPld(
	struct hpi_adapter_obj *pao,
	int nDSPIndex
);

/*****************************************************************************/

static void SubSysMessage(
	struct hpi_message *phm,
	struct hpi_response *phr
)
{

	switch (phm->wFunction) {
	case HPI_SUBSYS_OPEN:
	case HPI_SUBSYS_CLOSE:
	case HPI_SUBSYS_GET_INFO:
	case HPI_SUBSYS_DRIVER_UNLOAD:
	case HPI_SUBSYS_DRIVER_LOAD:
	case HPI_SUBSYS_FIND_ADAPTERS:
		/* messages that should not get here */
		phr->wError = HPI_ERROR_UNIMPLEMENTED;
		break;
	case HPI_SUBSYS_CREATE_ADAPTER:
		SubSysCreateAdapter(phm, phr);
		break;
	case HPI_SUBSYS_DELETE_ADAPTER:
		SubSysDeleteAdapter(phm, phr);
		break;
	default:
		phr->wError = HPI_ERROR_INVALID_FUNC;
		break;
	}
}

static void ControlMessage(
	struct hpi_adapter_obj *pao,
	struct hpi_message *phm,
	struct hpi_response *phr
)
{

	struct hpi_hw_obj *pHw6205 = pao->priv;

	switch (phm->wFunction) {
	case HPI_CONTROL_GET_STATE:
		if (pao->wHasControlCache)
			if (HpiCheckControlCache
				(&pHw6205->pControlCache[phm->u.c.
						wControlIndex], phm, phr))
				break;
		HW_Message(pao, phm, phr);
		break;
	case HPI_CONTROL_GET_INFO:
		HW_Message(pao, phm, phr);
		break;
	case HPI_CONTROL_SET_STATE:
		HW_Message(pao, phm, phr);
		if (pao->wHasControlCache)
			HpiSyncControlCache(&pHw6205->
				pControlCache[phm->u.c.
					wControlIndex], phm, phr);
		break;
	default:
		phr->wError = HPI_ERROR_INVALID_FUNC;
		break;
	}
}

static void AdapterMessage(
	struct hpi_adapter_obj *pao,
	struct hpi_message *phm,
	struct hpi_response *phr
)
{

	switch (phm->wFunction) {
	case HPI_ADAPTER_GET_INFO:
		HW_Message(pao, phm, phr);
		break;
	case HPI_ADAPTER_GET_ASSERT:
		AdapterGetAsserts(pao, phm, phr);
		break;
	default:
		HW_Message(pao, phm, phr);
		break;
	}
}

static void OStreamMessage(
	struct hpi_adapter_obj *pao,
	struct hpi_message *phm,
	struct hpi_response *phr
)
{

	if (phm->u.d.wStreamIndex >= HPI_MAX_STREAMS) {
		phr->wError = HPI_ERROR_INVALID_STREAM;
		HPI_DEBUG_LOG(WARNING,
			"Message referencing invalid stream %d "
			"on adapter index %d\n",
			phm->u.d.wStreamIndex, phm->wAdapterIndex);
		return;
	}

	switch (phm->wFunction) {
	case HPI_OSTREAM_WRITE:
		OutStreamWrite(pao, phm, phr);
		break;
	case HPI_OSTREAM_GET_INFO:
		OutStreamGetInfo(pao, phm, phr);
		break;
	case HPI_OSTREAM_HOSTBUFFER_ALLOC:
		OutStreamHostBufferAllocate(pao, phm, phr);
		break;
	case HPI_OSTREAM_HOSTBUFFER_FREE:
		OutStreamHostBufferFree(pao, phm, phr);
		break;
	case HPI_OSTREAM_START:
		OutStreamStart(pao, phm, phr);
		break;
	case HPI_OSTREAM_OPEN:
		OutStreamOpen(pao, phm, phr);
		break;
	case HPI_OSTREAM_RESET:
		OutStreamReset(pao, phm, phr);
		break;
	default:
		HW_Message(pao, phm, phr);
		break;
	}
}

static void IStreamMessage(
	struct hpi_adapter_obj *pao,
	struct hpi_message *phm,
	struct hpi_response *phr
)
{

	if (phm->u.d.wStreamIndex >= HPI_MAX_STREAMS) {
		phr->wError = HPI_ERROR_INVALID_STREAM;
		HPI_DEBUG_LOG(WARNING,
			"Message referencing invalid stream %d "
			"on adapter index %d\n",
			phm->u.d.wStreamIndex, phm->wAdapterIndex);
		return;
	}

	switch (phm->wFunction) {
	case HPI_ISTREAM_READ:
		InStreamRead(pao, phm, phr);
		break;
	case HPI_ISTREAM_GET_INFO:
		InStreamGetInfo(pao, phm, phr);
		break;
	case HPI_ISTREAM_HOSTBUFFER_ALLOC:
		InStreamHostBufferAllocate(pao, phm, phr);
		break;
	case HPI_ISTREAM_HOSTBUFFER_FREE:
		InStreamHostBufferFree(pao, phm, phr);
		break;
	case HPI_ISTREAM_START:
		InStreamStart(pao, phm, phr);
		break;
	default:
		HW_Message(pao, phm, phr);
		break;
	}
}

/*****************************************************************************/
/** Entry point to this HPI backend
 * All calls to the HPI start here
 */
void HPI_6205(
	struct hpi_message *phm,
	struct hpi_response *phr
)
{
	struct hpi_adapter_obj *pao = NULL;

	/* subsytem messages are processed by every HPI.
	 * All other messages are ignored unless the adapter index matches
	 * an adapter in the HPI
	 */
	HPI_DEBUG_LOG(DEBUG, "HPI Obj=%d, Func=%d\n", phm->wObject,
		phm->wFunction);

	/* if Dsp has crashed then do not communicate with it any more */
	if (phm->wObject != HPI_OBJ_SUBSYSTEM) {
		pao = HpiFindAdapter(phm->wAdapterIndex);
		if (!pao) {
			HPI_DEBUG_LOG(DEBUG,
				" %d,%d refused, for another HPI?\n",
				phm->wObject, phm->wFunction);
			return;
		}

		if (pao->wDspCrashed) {
			HPI_InitResponse(phr, phm->wObject, phm->wFunction,
				HPI_ERROR_DSP_HARDWARE);
			HPI_DEBUG_LOG(WARNING, " %d,%d dsp crashed.\n",
				phm->wObject, phm->wFunction);
			return;
		}
	}

	/* Init default response  */
	if (phm->wFunction != HPI_SUBSYS_CREATE_ADAPTER)
		HPI_InitResponse(phr, phm->wObject, phm->wFunction,
			HPI_ERROR_PROCESSING_MESSAGE);

	HPI_DEBUG_LOG(VERBOSE, "start of switch\n");
	switch (phm->wType) {
	case HPI_TYPE_MESSAGE:
		switch (phm->wObject) {
		case HPI_OBJ_SUBSYSTEM:
			SubSysMessage(phm, phr);
			break;

		case HPI_OBJ_ADAPTER:
			phr->wSize = sizeof(struct hpi_response_header) +
				sizeof(struct hpi_adapter_res);
			AdapterMessage(pao, phm, phr);
			break;

		case HPI_OBJ_CONTROL:
			ControlMessage(pao, phm, phr);
			break;

		case HPI_OBJ_OSTREAM:
			OStreamMessage(pao, phm, phr);
			break;

		case HPI_OBJ_ISTREAM:
			IStreamMessage(pao, phm, phr);
			break;

		default:
			HW_Message(pao, phm, phr);
			break;
		}
		break;

	default:
		phr->wError = HPI_ERROR_INVALID_TYPE;
		break;
	}
}

/*****************************************************************************/
/* SUBSYSTEM */

/** Create an adapter object and initialise it based on resource information
 * passed in in the message
 * *** NOTE - you cannot use this function AND the FindAdapters function at the
 * same time, the application must use only one of them to get the adapters ***
 */
static void SubSysCreateAdapter(
	struct hpi_message *phm,
	struct hpi_response *phr
)
{
	/* create temp adapter obj, because we don't know what index yet */
	struct hpi_adapter_obj ao;
	u32 dwOsErrorCode;
	u16 err;

	HPI_DEBUG_LOG(DEBUG, " SubSysCreateAdapter\n");

	memset(&ao, 0, sizeof(ao));

	/* this HPI only creates adapters for TI/PCI devices */
	if (phm->u.s.Resource.wBusType != HPI_BUS_PCI)
		return;
	if (phm->u.s.Resource.r.Pci->wVendorId != HPI_PCI_VENDOR_ID_TI)
		return;
	if (phm->u.s.Resource.r.Pci->wDeviceId != HPI_ADAPTER_DSP6205)
		return;

	ao.priv = kmalloc(sizeof(struct hpi_hw_obj), GFP_KERNEL);
	if (!ao.priv) {
		HPI_DEBUG_LOG(ERROR, "cant get mem for adapter object\n");
		phr->wError = HPI_ERROR_MEMORY_ALLOC;
		return;
	}
	memset(ao.priv, 0, sizeof(struct hpi_hw_obj));

	ao.Pci = *phm->u.s.Resource.r.Pci;
	err = CreateAdapterObj(&ao, &dwOsErrorCode);
	if (!err)
		err = HpiAddAdapter(&ao);
	if (err) {
		phr->u.s.dwData = dwOsErrorCode;
		DeleteAdapterObj(&ao);
		phr->wError = err;
		return;
	}

	phr->u.s.awAdapterList[ao.wIndex] = ao.wAdapterType;
	phr->u.s.wAdapterIndex = ao.wIndex;
	phr->u.s.wNumAdapters++;
	phr->wError = 0;
}

/** delete an adapter - required by WDM driver */
static void SubSysDeleteAdapter(
	struct hpi_message *phm,
	struct hpi_response *phr
)
{
	struct hpi_adapter_obj *pao = NULL;

	pao = HpiFindAdapter(phm->wAdapterIndex);
	if (!pao)
		return;

	DeleteAdapterObj(pao);
	phr->wError = 0;
}

/** Create adapter object
  allocate buffers, bootload DSPs, initialise control cache
*/
static u16 CreateAdapterObj(
	struct hpi_adapter_obj *pao,
	u32 *pdwOsErrorCode
)
{
	struct hpi_hw_obj *pHw6205 = pao->priv;
	volatile struct bus_master_interface *interface;
	u32 dwPhysAddr;
	u32 dwTimeOut = HPI6205_TIMEOUT;
	u32 dwTemp1;
	int i;
	u16 err;

	/* init error reporting */
	pao->wDspCrashed = 0;

	for (i = 0; i < HPI_MAX_STREAMS; i++)
		pHw6205->flagOStreamJustReset[i] = 1;

	/* The C6205 memory area 1 is 8Mbyte window into DSP registers */
	pHw6205->prHSR = pao->Pci.apMemBase[1] +
		C6205_BAR1_HSR / sizeof(*pao->Pci.apMemBase[1]);
	pHw6205->prHDCR =
		pao->Pci.apMemBase[1] +
		C6205_BAR1_HDCR / sizeof(*pao->Pci.apMemBase[1]);
	pHw6205->prDSPP =
		pao->Pci.apMemBase[1] +
		C6205_BAR1_DSPP / sizeof(*pao->Pci.apMemBase[1]);
	pao->wHasControlCache = 0;

	if (HpiOs_LockedMem_Alloc(&pHw6205->hLockedMem,
			sizeof(struct bus_master_interface),
			pao->Pci.pOsData))
		pHw6205->pInterfaceBuffer = NULL;
	else if (HpiOs_LockedMem_GetVirtAddr
		(&pHw6205->hLockedMem, (void *)&pHw6205->pInterfaceBuffer))
		pHw6205->pInterfaceBuffer = NULL;

	HPI_DEBUG_LOG(DEBUG, "Interface buffer address %p\n",
		pHw6205->pInterfaceBuffer);
	if (pHw6205->pInterfaceBuffer) {
		memset((void *)pHw6205->pInterfaceBuffer, 0,
			sizeof(struct bus_master_interface));
		pHw6205->pInterfaceBuffer->dwDspAck = -1;
	}

	err = Hpi6205_AdapterBootLoadDsp(pao, pdwOsErrorCode);
	if (err)
		/* no need to clean up as SubSysCreateAdapter */
		/* calls DeleteAdapter on error. */
		return (err);

	HPI_DEBUG_LOG(INFO, "Load DSP code OK\n");

	/* allow boot load even if mem alloc wont work */
	if (!pHw6205->pInterfaceBuffer)
		return (Hpi6205_Error(0, HPI6205_ERROR_MEM_ALLOC));

	interface = pHw6205->pInterfaceBuffer;

	/* wait for first interrupt indicating the DSP init is done */
	dwTimeOut = HPI6205_TIMEOUT * 10;
	dwTemp1 = 0;
	while (((dwTemp1 & C6205_HSR_INTSRC) == 0) && --dwTimeOut)
		dwTemp1 = ioread32(pHw6205->prHSR);

	if (dwTemp1 & C6205_HSR_INTSRC)
		HPI_DEBUG_LOG(INFO,
			"Interrupt confirming DSP code running OK\n");
	else {
		HPI_DEBUG_LOG(ERROR,
			"Timed out waiting for interrupt "
			"confirming DSP code running\n");
		return (Hpi6205_Error(0, HPI6205_ERROR_6205_NO_IRQ));
	}

	/* reset the interrupt */
	iowrite32(C6205_HSR_INTSRC, pHw6205->prHSR);

	/* make sure the DSP has started ok */
	dwTimeOut = 100;
	while ((interface->dwDspAck != H620_HIF_RESET) && dwTimeOut) {
		dwTimeOut--;
		HpiOs_DelayMicroSeconds(10000);
	}

	if (dwTimeOut == 0) {
		HPI_DEBUG_LOG(ERROR, "Timed out waiting ack \n");
		return (Hpi6205_Error(0, HPI6205_ERROR_6205_INIT_FAILED));
	}
	/* Note that *pao, *pHw6205 are zeroed after allocation,
	 * so pointers and flags are NULL by default.
	 * Allocate bus mastering control cache buffer and tell the DSP about it
	 */
	if (interface->aControlCache.dwNumberOfControls) {
		err = HpiOs_LockedMem_Alloc(&pHw6205->hControlCache,
			interface->aControlCache.
			dwNumberOfControls *
			sizeof(struct hpi_control_cache_single),
			pao->Pci.pOsData);
		if (!err)
			err = HpiOs_LockedMem_GetVirtAddr(&pHw6205->
				hControlCache, (void *)
				&pHw6205->pControlCache);
		if (!err)
			memset((void *)pHw6205->pControlCache, 0,
				interface->aControlCache.
				dwNumberOfControls *
				sizeof(struct hpi_control_cache_single));
		if (!err) {
			err = HpiOs_LockedMem_GetPhysAddr(&pHw6205->
				hControlCache, &dwPhysAddr);
			interface->aControlCache.dwPhysicalPCI32address =
				dwPhysAddr;
		}

		if (!err)
			pao->wHasControlCache = 1;
		else {
			if (HpiOs_LockedMem_Valid(&pHw6205->hControlCache)) {
				HpiOs_LockedMem_Free(&pHw6205->hControlCache);
				pHw6205->pControlCache = NULL;
			}
			pao->wHasControlCache = 0;
		}
	}
	/* allocate bus mastering async buffer and tell the DSP about it */
	if (interface->aAsyncBuffer.b.dwSize) {
		err = HpiOs_LockedMem_Alloc(&pHw6205->hAsyncEventBuffer,
			interface->aAsyncBuffer.b.
			dwSize * sizeof(struct hpi_async_event),
			pao->Pci.pOsData);
		if (!err)
			err = HpiOs_LockedMem_GetVirtAddr(&pHw6205->
				hAsyncEventBuffer, (void *)
				&pHw6205->pAsyncEventBuffer);
		if (!err)
			memset((void *)pHw6205->pAsyncEventBuffer, 0,
				interface->aAsyncBuffer.b.dwSize *
				sizeof(struct hpi_async_event));
		if (!err) {
			err = HpiOs_LockedMem_GetPhysAddr(&pHw6205->
				hAsyncEventBuffer, &dwPhysAddr);
			interface->aAsyncBuffer.dwPhysicalPCI32address =
				dwPhysAddr;
		}
		if (err) {
			if (HpiOs_LockedMem_Valid(&pHw6205->
					hAsyncEventBuffer)) {
				HpiOs_LockedMem_Free(&pHw6205->
					hAsyncEventBuffer);
				pHw6205->pAsyncEventBuffer = NULL;
			}
		}
	}
	/* set interface to idle */
	interface->dwHostCmd = H620_HIF_IDLE;
	/* interrupt the DSP again */
	dwTemp1 = ioread32(pHw6205->prHDCR);
	dwTemp1 |= (u32)C6205_HDCR_DSPINT;
	iowrite32(dwTemp1, pHw6205->prHDCR);
	dwTemp1 &= ~(u32)C6205_HDCR_DSPINT;
	iowrite32(dwTemp1, pHw6205->prHDCR);

	{
		struct hpi_message hM;
		struct hpi_response hR;
		u32 nMaxStreams;

		HPI_DEBUG_LOG(VERBOSE, "HPI6205.C - send ADAPTER_GET_INFO\n");
		memset(&hM, 0, sizeof(hM));
		hM.wType = HPI_TYPE_MESSAGE;
		hM.wSize = sizeof(hM);
		hM.wObject = HPI_OBJ_ADAPTER;
		hM.wFunction = HPI_ADAPTER_GET_INFO;
		hM.wAdapterIndex = 0;
		memset(&hR, 0, sizeof(hR));
		hR.wSize = sizeof(hR);

		err = Hpi6205_MessageResponseSequence(pao, &hM, &hR);
		if (err) {
			HPI_DEBUG_LOG(ERROR, "message transport error %d\n",
				err);
			return (err);
		}
		if (hR.wError)
			return (hR.wError);

		pao->wAdapterType = hR.u.a.wAdapterType;
		pao->wIndex = hR.u.a.wAdapterIndex;

		nMaxStreams = hR.u.a.wNumOStreams + hR.u.a.wNumIStreams;

		HpiOs_LockedMem_Prepare((nMaxStreams * 6) / 10, nMaxStreams,
			65536, pao->Pci.pOsData);
	}

	HPI_DEBUG_LOG(VERBOSE, "Get adapter info OK\n");
	pao->wOpen = 0;		/* upon creation the adapter is closed */

	HPI_DEBUG_LOG(INFO, "Bootload DSP OK\n");
	return (0);
}

/** Free memory areas allocated by adapter
 * this routine is called from SubSysDeleteAdapter,
  * and SubSysCreateAdapter if duplicate index
*/
static void DeleteAdapterObj(
	struct hpi_adapter_obj *pao
)
{
	struct hpi_hw_obj *pHw6205;
	int i;

	pHw6205 = pao->priv;

	if (HpiOs_LockedMem_Valid(&pHw6205->hAsyncEventBuffer)) {
		HpiOs_LockedMem_Free(&pHw6205->hAsyncEventBuffer);
		pHw6205->pAsyncEventBuffer = NULL;
	}

	if (HpiOs_LockedMem_Valid(&pHw6205->hControlCache)) {
		HpiOs_LockedMem_Free(&pHw6205->hControlCache);
		pHw6205->pControlCache = NULL;
	}

	if (HpiOs_LockedMem_Valid(&pHw6205->hLockedMem)) {
		HpiOs_LockedMem_Free(&pHw6205->hLockedMem);
		pHw6205->pInterfaceBuffer = NULL;
	}

	for (i = 0; i < HPI_MAX_STREAMS; i++)
		if (HpiOs_LockedMem_Valid(&pHw6205->InStreamHostBuffers[i])) {
			HpiOs_LockedMem_Free(&pHw6205->
				InStreamHostBuffers[i]);
			/*?pHw6205->InStreamHostBuffers[i] = NULL; */
			pHw6205->InStreamHostBufferSize[i] = 0;
		}

	for (i = 0; i < HPI_MAX_STREAMS; i++)
		if (HpiOs_LockedMem_Valid(&pHw6205->OutStreamHostBuffers[i])) {
			HpiOs_LockedMem_Free(&pHw6205->
				OutStreamHostBuffers[i]);
			pHw6205->OutStreamHostBufferSize[i] = 0;
		}

	HpiOs_LockedMem_Unprepare(pao->Pci.pOsData);

	HpiDeleteAdapter(pao);
	kfree(pHw6205);
}

/*****************************************************************************/
/* ADAPTER */

static void AdapterGetAsserts(
	struct hpi_adapter_obj *pao,
	struct hpi_message *phm,
	struct hpi_response *phr
)
{
	HW_Message(pao, phm, phr);	/*get DSP asserts */
	return;
}

/*****************************************************************************/
/* OutStream Host buffer functions */

/** Allocate or attach buffer for busmastering
*/
static void OutStreamHostBufferAllocate(
	struct hpi_adapter_obj *pao,
	struct hpi_message *phm,
	struct hpi_response *phr
)
{
	u16 err = 0;
	u32 dwCommand = phm->u.d.u.Buffer.dwCommand;
	/*u16 wStreamIndex = phm->u.d.wStreamIndex; */
	struct hpi_hw_obj *pHw6205 = pao->priv;
	volatile struct bus_master_interface *interface =
		pHw6205->pInterfaceBuffer;

	HPI_InitResponse(phr, phm->wObject, phm->wFunction, 0);

	if (dwCommand == HPI_BUFFER_CMD_EXTERNAL
		|| dwCommand == HPI_BUFFER_CMD_INTERNAL_ALLOC) {
		/* ALLOC phase, allocate a buffer with power of 2 size,
		   get its bus address for PCI bus mastering
		 */
		phm->u.d.u.Buffer.dwBufferSize =
			roundup_pow_of_two(phm->u.d.u.Buffer.dwBufferSize);
		/* return old size and allocated size,
		   so caller can detect change */
		phr->u.d.u.stream_info.dwDataAvailable =
			pHw6205->OutStreamHostBufferSize[phm->u.d.
			wStreamIndex];
		phr->u.d.u.stream_info.dwBufferSize =
			phm->u.d.u.Buffer.dwBufferSize;

		if (pHw6205->OutStreamHostBufferSize[phm->u.d.wStreamIndex] ==
			phm->u.d.u.Buffer.dwBufferSize) {
			/* Same size, no action required */
			return;
		}

		if (HpiOs_LockedMem_Valid(&pHw6205->OutStreamHostBuffers[phm->
					u.d.wStreamIndex]))
			HpiOs_LockedMem_Free(&pHw6205->
				OutStreamHostBuffers[phm->u.d.wStreamIndex]);

		err = HpiOs_LockedMem_Alloc(&pHw6205->
			OutStreamHostBuffers[phm->u.d.
				wStreamIndex],
			phm->u.d.u.Buffer.dwBufferSize, pao->Pci.pOsData);

		if (err) {
			phr->wError = HPI_ERROR_INVALID_DATASIZE;
			pHw6205->OutStreamHostBufferSize[phm->u.d.
				wStreamIndex] = 0;
			return;
		}

		err = HpiOs_LockedMem_GetPhysAddr(&pHw6205->
			OutStreamHostBuffers
			[phm->u.d.wStreamIndex],
			&phm->u.d.u.Buffer.dwPciAddress);
		/* get the phys addr into msg for single call alloc caller
		 * needs to do this for split alloc (or use the same message)
		 * return the phy address for split alloc in the respose too
		 */
		phr->u.d.u.stream_info.dwAuxiliaryDataAvailable =
			phm->u.d.u.Buffer.dwPciAddress;

		if (err) {
			HpiOs_LockedMem_Free(&pHw6205->
				OutStreamHostBuffers[phm->u.d.wStreamIndex]);
			pHw6205->OutStreamHostBufferSize[phm->u.d.
				wStreamIndex] = 0;
			phr->wError = HPI_ERROR_MEMORY_ALLOC;
			return;
		}
	}

	if (dwCommand == HPI_BUFFER_CMD_EXTERNAL
		|| dwCommand == HPI_BUFFER_CMD_INTERNAL_GRANTADAPTER) {
		/* GRANT phase.  Set up the BBM status, tell the DSP about
		   the buffer so it can start using BBM.
		 */
		volatile struct hostbuffer_status_6205 *status;

		if (phm->u.d.u.Buffer.
			dwBufferSize & (phm->u.d.u.Buffer.dwBufferSize - 1)) {
			HPI_DEBUG_LOG(ERROR,
				"Buffer size must be 2^N not %d\n",
				phm->u.d.u.Buffer.dwBufferSize);
			phr->wError = HPI_ERROR_INVALID_DATASIZE;
			return;
		}
		pHw6205->OutStreamHostBufferSize[phm->u.d.wStreamIndex] =
			phm->u.d.u.Buffer.dwBufferSize;
		status = &interface->aOutStreamHostBufferStatus[phm->u.d.
			wStreamIndex];
		status->dwSamplesProcessed = 0;
		status->dwStreamState = HPI_STATE_STOPPED;
		status->dwDSPIndex = 0;
		status->dwHostIndex = status->dwDSPIndex;
		status->dwSizeInBytes = phm->u.d.u.Buffer.dwBufferSize;

		HW_Message(pao, phm, phr);

		if (phr->wError &&
			HpiOs_LockedMem_Valid(&pHw6205->
				OutStreamHostBuffers[phm->u.d.
					wStreamIndex])) {
			HpiOs_LockedMem_Free(&pHw6205->
				OutStreamHostBuffers[phm->u.d.wStreamIndex]);
			pHw6205->OutStreamHostBufferSize[phm->u.d.
				wStreamIndex] = 0;
		}
	}
}
static void OutStreamHostBufferFree(
	struct hpi_adapter_obj *pao,
	struct hpi_message *phm,
	struct hpi_response *phr
)
{
	struct hpi_hw_obj *pHw6205 = pao->priv;
	u32 dwCommand = phm->u.d.u.Buffer.dwCommand;

	if (pHw6205->OutStreamHostBufferSize[phm->u.d.wStreamIndex]) {
		if (dwCommand == HPI_BUFFER_CMD_EXTERNAL ||
			dwCommand == HPI_BUFFER_CMD_INTERNAL_REVOKEADAPTER) {
			pHw6205->OutStreamHostBufferSize[phm->u.d.
				wStreamIndex] = 0;
			HW_Message(pao, phm, phr);
			/* Tell adapter to stop using the host buffer. */
		}
		if (dwCommand == HPI_BUFFER_CMD_EXTERNAL ||
			dwCommand == HPI_BUFFER_CMD_INTERNAL_FREE)
			HpiOs_LockedMem_Free(&pHw6205->
				OutStreamHostBuffers[phm->u.d.wStreamIndex]);
	}
	/* Should HPI_ERROR_INVALID_OPERATION be returned
	   if no host buffer is allocated? */
	else
		HPI_InitResponse(phr, HPI_OBJ_OSTREAM,
			HPI_OSTREAM_HOSTBUFFER_FREE, 0);

}

static long OutStreamGetSpaceAvailable(
	volatile struct hostbuffer_status_6205 *status
)
{
	return status->dwSizeInBytes - ((long)(status->dwHostIndex) -
		(long)(status->dwDSPIndex));
}

static void OutStreamWrite(
	struct hpi_adapter_obj *pao,
	struct hpi_message *phm,
	struct hpi_response *phr
)
{
	struct hpi_hw_obj *pHw6205 = pao->priv;
	volatile struct bus_master_interface *interface =
		pHw6205->pInterfaceBuffer;
	volatile struct hostbuffer_status_6205 *status;
	long dwSpaceAvailable;
	if (!pHw6205->OutStreamHostBufferSize[phm->u.d.wStreamIndex]) {
		/* there  is no BBM buffer */
		HW_Message(pao, phm, phr);
		return;
	}
	HPI_InitResponse(phr, phm->wObject, phm->wFunction, 0);

	status = &interface->aOutStreamHostBufferStatus[phm->u.d.
		wStreamIndex];

/* Set this to 1 to force this OutStremWrite() call to write data to the */
/* adapter's buffers for the first write following stream reset. */
#define OLD_STYLE_PREWRITE (1)

	/* check whether we need to send the format to the DSP */
	if (pHw6205->flagOStreamJustReset[phm->u.d.wStreamIndex]) {
#if OLD_STYLE_PREWRITE
		int nPartialWrite = 0;
		unsigned int nOriginalSize = 0;
#endif

		u16 wFunction = phm->wFunction;
		pHw6205->flagOStreamJustReset[phm->u.d.wStreamIndex] = 0;
		phm->wFunction = HPI_OSTREAM_SET_FORMAT;
		HW_Message(pao, phm, phr);	/* send the format to the DSP */
		phm->wFunction = wFunction;
		if (phr->wError)
			return;

#if OLD_STYLE_PREWRITE
		/* Send the first buffer to the DSP the old way. */
		/* Limit size of first transfer - */
		/* hopefully this will not not be triggered. */
		if (phm->u.d.u.Data.dwDataSize > HPI6205_SIZEOF_DATA) {
			nPartialWrite = 1;
			nOriginalSize = phm->u.d.u.Data.dwDataSize;
			phm->u.d.u.Data.dwDataSize = HPI6205_SIZEOF_DATA;
		}
		/* write it */
		phm->wFunction = HPI_OSTREAM_WRITE;
		HW_Message(pao, phm, phr);
		/* update status information that the DSP would typically
		 * update (and will update next time the DSP
		 * buffer update task reads data from the host BBM buffer)
		 */
		status->dwAuxiliaryDataAvailable = phm->u.d.u.Data.dwDataSize;

		/* if we did a full write, we can return from here. */
		if (!nPartialWrite)
			return;

		/* tweak buffer parameters and let the rest of the */
		/* buffer land in internal BBM buffer */
		phm->u.d.u.Data.dwDataSize =
			nOriginalSize - HPI6205_SIZEOF_DATA;
		phm->u.d.u.Data.pbData += HPI6205_SIZEOF_DATA;
#endif
	}

	dwSpaceAvailable = OutStreamGetSpaceAvailable(status);
	if (dwSpaceAvailable < (long)phm->u.d.u.Data.dwDataSize) {
		phr->wError = HPI_ERROR_INVALID_DATASIZE;
		return;
	}

	/* HostBuffers is used to indicate host buffer is internally allocated.
	   otherwise, assumed external, data written externally */
	if (HpiOs_LockedMem_Valid(&pHw6205->OutStreamHostBuffers[phm->u.d.
				wStreamIndex])) {
		u8 *pBBMData;
		long lFirstWrite;
		u8 *pAppData = (u8 *)phm->u.d.u.Data.pbData;

		if (HpiOs_LockedMem_GetVirtAddr
			(&pHw6205->OutStreamHostBuffers[phm->u.d.
					wStreamIndex], (void *)&pBBMData)) {
			phr->wError = HPI_ERROR_INVALID_OPERATION;
			return;
		}

		/* either all data,
		   or enough to fit from current to end of BBM buffer */
		lFirstWrite = min(phm->u.d.u.Data.dwDataSize,
			status->dwSizeInBytes -
			(status->dwHostIndex & (status->dwSizeInBytes - 1)));

		memcpy(pBBMData +
			(status->dwHostIndex & (status->dwSizeInBytes
					- 1)), pAppData, lFirstWrite);
		/* remaining data if any */
		memcpy(pBBMData, pAppData + lFirstWrite,
			phm->u.d.u.Data.dwDataSize - lFirstWrite);
	}
	status->dwHostIndex += phm->u.d.u.Data.dwDataSize;
}

static void OutStreamGetInfo(
	struct hpi_adapter_obj *pao,
	struct hpi_message *phm,
	struct hpi_response *phr
)
{
	struct hpi_hw_obj *pHw6205 = pao->priv;
	volatile struct bus_master_interface *interface =
		pHw6205->pInterfaceBuffer;
	volatile struct hostbuffer_status_6205 *status;

	if (!pHw6205->OutStreamHostBufferSize[phm->u.d.wStreamIndex]) {
		HW_Message(pao, phm, phr);
		return;
	}

	HPI_InitResponse(phr, phm->wObject, phm->wFunction, 0);

	status = &interface->aOutStreamHostBufferStatus[phm->u.d.
		wStreamIndex];

	phr->u.d.u.stream_info.wState = (u16)status->dwStreamState;
	phr->u.d.u.stream_info.dwSamplesTransferred =
		status->dwSamplesProcessed;
	phr->u.d.u.stream_info.dwBufferSize = status->dwSizeInBytes;
	phr->u.d.u.stream_info.dwDataAvailable =
		status->dwSizeInBytes - OutStreamGetSpaceAvailable(status);
	phr->u.d.u.stream_info.dwAuxiliaryDataAvailable =
		status->dwAuxiliaryDataAvailable;
}
static void OutStreamStart(
	struct hpi_adapter_obj *pao,
	struct hpi_message *phm,
	struct hpi_response *phr
)
{
	HW_Message(pao, phm, phr);
}
static void OutStreamReset(
	struct hpi_adapter_obj *pao,
	struct hpi_message *phm,
	struct hpi_response *phr
)
{
	struct hpi_hw_obj *pHw6205 = pao->priv;
	pHw6205->flagOStreamJustReset[phm->u.d.wStreamIndex] = 1;
	HW_Message(pao, phm, phr);
}
static void OutStreamOpen(
	struct hpi_adapter_obj *pao,
	struct hpi_message *phm,
	struct hpi_response *phr
)
{
	OutStreamReset(pao, phm, phr);
}

/*****************************************************************************/
/* InStream Host buffer functions */

static void InStreamHostBufferAllocate(
	struct hpi_adapter_obj *pao,
	struct hpi_message *phm,
	struct hpi_response *phr
)
{
	u16 err = 0;
	u32 dwCommand = phm->u.d.u.Buffer.dwCommand;
	struct hpi_hw_obj *pHw6205 = pao->priv;
	volatile struct bus_master_interface *interface =
		pHw6205->pInterfaceBuffer;

	HPI_InitResponse(phr, phm->wObject, phm->wFunction, 0);

	if (dwCommand == HPI_BUFFER_CMD_EXTERNAL ||
		dwCommand == HPI_BUFFER_CMD_INTERNAL_ALLOC) {
		phm->u.d.u.Buffer.dwBufferSize =
			roundup_pow_of_two(phm->u.d.u.Buffer.dwBufferSize);
		phr->u.d.u.stream_info.dwDataAvailable =
			pHw6205->InStreamHostBufferSize[phm->u.d.
			wStreamIndex];
		phr->u.d.u.stream_info.dwBufferSize =
			phm->u.d.u.Buffer.dwBufferSize;

		if (pHw6205->InStreamHostBufferSize[phm->u.d.wStreamIndex] ==
			phm->u.d.u.Buffer.dwBufferSize) {
			/* Same size, no action required */
			return;
		}

		if (HpiOs_LockedMem_Valid(&pHw6205->InStreamHostBuffers[phm->
					u.d.wStreamIndex]))
			HpiOs_LockedMem_Free(&pHw6205->
				InStreamHostBuffers[phm->u.d.wStreamIndex]);

		err = HpiOs_LockedMem_Alloc(&pHw6205->
			InStreamHostBuffers[phm->u.d.
				wStreamIndex],
			phm->u.d.u.Buffer.dwBufferSize, pao->Pci.pOsData);

		if (err) {
			phr->wError = HPI_ERROR_INVALID_DATASIZE;
			pHw6205->InStreamHostBufferSize[phm->u.d.
				wStreamIndex] = 0;
			return;
		}

		err = HpiOs_LockedMem_GetPhysAddr(&pHw6205->
			InStreamHostBuffers[phm->
				u.d.
				wStreamIndex],
			&phm->u.d.u.Buffer.dwPciAddress);
		/* get the phys addr into msg for single call alloc. Caller
		   needs to do this for split alloc so return the phy address */
		phr->u.d.u.stream_info.dwAuxiliaryDataAvailable =
			phm->u.d.u.Buffer.dwPciAddress;
		if (err) {
			HpiOs_LockedMem_Free(&pHw6205->
				InStreamHostBuffers[phm->u.d.wStreamIndex]);
			pHw6205->InStreamHostBufferSize[phm->u.d.
				wStreamIndex] = 0;
			phr->wError = HPI_ERROR_MEMORY_ALLOC;
			return;
		}
	}

	if (dwCommand == HPI_BUFFER_CMD_EXTERNAL ||
		dwCommand == HPI_BUFFER_CMD_INTERNAL_GRANTADAPTER) {
		volatile struct hostbuffer_status_6205 *status;

		if (phm->u.d.u.Buffer.
			dwBufferSize & (phm->u.d.u.Buffer.dwBufferSize - 1)) {
			HPI_DEBUG_LOG(ERROR,
				"Buffer size must be 2^N not %d\n",
				phm->u.d.u.Buffer.dwBufferSize);
			phr->wError = HPI_ERROR_INVALID_DATASIZE;
			return;
		}

		pHw6205->InStreamHostBufferSize[phm->u.d.wStreamIndex] =
			phm->u.d.u.Buffer.dwBufferSize;
		status = &interface->aInStreamHostBufferStatus[phm->u.d.
			wStreamIndex];
		status->dwSamplesProcessed = 0;
		status->dwStreamState = HPI_STATE_STOPPED;
		status->dwDSPIndex = 0;
		status->dwHostIndex = status->dwDSPIndex;
		status->dwSizeInBytes = phm->u.d.u.Buffer.dwBufferSize;
		HW_Message(pao, phm, phr);
		if (phr->wError
			&& HpiOs_LockedMem_Valid(&pHw6205->
				InStreamHostBuffers[phm->u.d.wStreamIndex])) {
			HpiOs_LockedMem_Free(&pHw6205->
				InStreamHostBuffers[phm->u.d.wStreamIndex]);
			pHw6205->InStreamHostBufferSize[phm->u.d.
				wStreamIndex] = 0;
		}
	}
}

static void InStreamHostBufferFree(
	struct hpi_adapter_obj *pao,
	struct hpi_message *phm,
	struct hpi_response *phr
)
{
	struct hpi_hw_obj *pHw6205 = pao->priv;
	u32 dwCommand = phm->u.d.u.Buffer.dwCommand;

	if (pHw6205->InStreamHostBufferSize[phm->u.d.wStreamIndex]) {
		if (dwCommand == HPI_BUFFER_CMD_EXTERNAL ||
			dwCommand == HPI_BUFFER_CMD_INTERNAL_REVOKEADAPTER) {
			pHw6205->InStreamHostBufferSize[phm->u.d.
				wStreamIndex] = 0;
			HW_Message(pao, phm, phr);
		}

		if (dwCommand == HPI_BUFFER_CMD_EXTERNAL ||
			dwCommand == HPI_BUFFER_CMD_INTERNAL_FREE) {
			HpiOs_LockedMem_Free(&pHw6205->
				InStreamHostBuffers[phm->u.d.wStreamIndex]);
		}
	} else {
		/* Should HPI_ERROR_INVALID_OPERATION be returned
		   if no host buffer is allocated? */
		HPI_InitResponse(phr, HPI_OBJ_ISTREAM,
			HPI_ISTREAM_HOSTBUFFER_FREE, 0);

	}

}

static void InStreamStart(
	struct hpi_adapter_obj *pao,
	struct hpi_message *phm,
	struct hpi_response *phr
)
{
	HW_Message(pao, phm, phr);
}

static long InStreamGetBytesAvailable(
	volatile struct hostbuffer_status_6205 *status
)
{
	return (long)(status->dwDSPIndex) - (long)(status->dwHostIndex);
}

static void InStreamRead(
	struct hpi_adapter_obj *pao,
	struct hpi_message *phm,
	struct hpi_response *phr
)
{
	struct hpi_hw_obj *pHw6205 = pao->priv;
	volatile struct bus_master_interface *interface =
		pHw6205->pInterfaceBuffer;
	volatile struct hostbuffer_status_6205 *status;
	long dwDataAvailable;
	u8 *pBBMData;
	long lFirstRead;
	u8 *pAppData = (u8 *)phm->u.d.u.Data.pbData;

	if (!pHw6205->InStreamHostBufferSize[phm->u.d.wStreamIndex]) {
		HW_Message(pao, phm, phr);
		return;
	}
	HPI_InitResponse(phr, phm->wObject, phm->wFunction, 0);

	status = &interface->aInStreamHostBufferStatus[phm->u.d.wStreamIndex];
	dwDataAvailable = InStreamGetBytesAvailable(status);
	if (dwDataAvailable < (long)phm->u.d.u.Data.dwDataSize) {
		phr->wError = HPI_ERROR_INVALID_DATASIZE;
		return;
	}

	if (HpiOs_LockedMem_Valid(&pHw6205->InStreamHostBuffers[phm->u.d.
				wStreamIndex])) {
		if (HpiOs_LockedMem_GetVirtAddr(&pHw6205->
				InStreamHostBuffers[phm->u.d.wStreamIndex],
				(void *)&pBBMData)) {
			phr->wError = HPI_ERROR_INVALID_OPERATION;
			return;
		}

		/* either all data,
		   or enough to fit from current to end of BBM buffer */
		lFirstRead = min(phm->u.d.u.Data.dwDataSize,
			status->dwSizeInBytes -
			(status->dwHostIndex & (status->dwSizeInBytes - 1)));

		memcpy(pAppData, pBBMData +
			(status->dwHostIndex & (status->dwSizeInBytes
					- 1)), lFirstRead);
		/* remaining data if any */
		memcpy(pAppData + lFirstRead,
			pBBMData, phm->u.d.u.Data.dwDataSize - lFirstRead);
	}
	status->dwHostIndex += phm->u.d.u.Data.dwDataSize;
}

static void InStreamGetInfo(
	struct hpi_adapter_obj *pao,
	struct hpi_message *phm,
	struct hpi_response *phr
)
{
	struct hpi_hw_obj *pHw6205 = pao->priv;
	volatile struct bus_master_interface *interface =
		pHw6205->pInterfaceBuffer;
	volatile struct hostbuffer_status_6205 *status;
	if (!pHw6205->InStreamHostBufferSize[phm->u.d.wStreamIndex]) {
		HW_Message(pao, phm, phr);
		return;
	}

	status = &interface->aInStreamHostBufferStatus[phm->u.d.wStreamIndex];

	HPI_InitResponse(phr, phm->wObject, phm->wFunction, 0);

	phr->u.d.u.stream_info.wState = (u16)status->dwStreamState;
	phr->u.d.u.stream_info.dwSamplesTransferred =
		status->dwSamplesProcessed;
	phr->u.d.u.stream_info.dwBufferSize = status->dwSizeInBytes;
	phr->u.d.u.stream_info.dwDataAvailable =
		InStreamGetBytesAvailable(status);
	phr->u.d.u.stream_info.dwAuxiliaryDataAvailable =
		status->dwAuxiliaryDataAvailable;
}

/*****************************************************************************/
/* LOW-LEVEL */

#define HPI6205_MAX_FILES_TO_LOAD 2

static u16 Hpi6205_AdapterBootLoadDsp(
	struct hpi_adapter_obj *pao,
	u32 *pdwOsErrorCode
)
{
	struct hpi_hw_obj *pHw6205 = pao->priv;
	struct dsp_code DspCode;
	u16 anBootLoadFamily[HPI6205_MAX_FILES_TO_LOAD];
	u32 dwTemp;
	int nDsp = 0, i = 0;
	u16 err = 0;

	switch (pao->Pci.wSubSysDeviceId) {
	case HPI_ADAPTER_FAMILY_ASI5000:
		anBootLoadFamily[0] = pao->Pci.wSubSysDeviceId;
		anBootLoadFamily[1] = 0;
		break;
	case HPI_ADAPTER_FAMILY_ASI6500:
		/* 6500 uses 6600 code */
		anBootLoadFamily[0] = HPI_ADAPTER_FAMILY_ASI6205;
		anBootLoadFamily[1] = HPI_ADAPTER_FAMILY_ASI6600;
		break;
	case HPI_ADAPTER_FAMILY_ASI6400:
	case HPI_ADAPTER_FAMILY_ASI6600:
	case HPI_ADAPTER_FAMILY_ASI8700:
	case HPI_ADAPTER_FAMILY_ASI8900:
		anBootLoadFamily[0] = HPI_ADAPTER_FAMILY_ASI6205;
		anBootLoadFamily[1] = pao->Pci.wSubSysDeviceId;
		break;
	default:
		return (Hpi6205_Error(0, HPI6205_ERROR_UNKNOWN_PCI_DEVICE));
	}

	/* reset DSP by writing a 1 to the WARMRESET bit */
	dwTemp = C6205_HDCR_WARMRESET;
	iowrite32(dwTemp, pHw6205->prHDCR);
	HpiOs_DelayMicroSeconds(1000);
/*	for(i=0;i<1000; i++) dwTemp = ioread32(pao,pHw6205->prHDCR); */

	/* check that PCI i/f was configured by EEPROM */
	dwTemp = ioread32(pHw6205->prHSR);
	if ((dwTemp & (C6205_HSR_CFGERR | C6205_HSR_EEREAD)) !=
		C6205_HSR_EEREAD)
		return Hpi6205_Error(0, HPI6205_ERROR_6205_EEPROM);
	dwTemp |= 0x04;
	/* disable PINTA interrupt */
	iowrite32(dwTemp, pHw6205->prHSR);

	/* check control register reports PCI boot mode */
	dwTemp = ioread32(pHw6205->prHDCR);
	if (!(dwTemp & C6205_HDCR_PCIBOOT))
		return Hpi6205_Error(0, HPI6205_ERROR_6205_REG);

	/* try writing a couple of numbers to the DSP page register */
	/* and reading them back. */
	dwTemp = 1;
	iowrite32(dwTemp, pHw6205->prDSPP);
	if ((dwTemp | C6205_DSPP_MAP1) != ioread32(pHw6205->prDSPP))
		return Hpi6205_Error(0, HPI6205_ERROR_6205_DSPPAGE);
	dwTemp = 2;
	iowrite32(dwTemp, pHw6205->prDSPP);
	if ((dwTemp | C6205_DSPP_MAP1) != ioread32(pHw6205->prDSPP))
		return Hpi6205_Error(0, HPI6205_ERROR_6205_DSPPAGE);
	dwTemp = 3;
	iowrite32(dwTemp, pHw6205->prDSPP);
	if ((dwTemp | C6205_DSPP_MAP1) != ioread32(pHw6205->prDSPP))
		return Hpi6205_Error(0, HPI6205_ERROR_6205_DSPPAGE);
	/* reset DSP page to the correct number */
	dwTemp = 0;
	iowrite32(dwTemp, pHw6205->prDSPP);
	if ((dwTemp | C6205_DSPP_MAP1) != ioread32(pHw6205->prDSPP))
		return Hpi6205_Error(0, HPI6205_ERROR_6205_DSPPAGE);
	pHw6205->dwDspPage = 0;

	/* release 6713 from reset before 6205 is bootloaded.
	   This ensures that the EMIF is inactive,
	   and the 6713 HPI gets the correct bootmode etc
	 */
	if (anBootLoadFamily[1] != 0) {
		/* DSP 1 is a C6713 */
		/* CLKX0 <- '1' release the C6205 bootmode pulldowns */
		BootLoader_WriteMem32(pao, 0, (0x018C0024L), 0x00002202);
		HpiOs_DelayMicroSeconds(100);
		/* Reset the 6713 #1 - revB */
		BootLoader_WriteMem32(pao, 0, C6205_BAR0_TIMER1_CTL, 0);

		/* dummy read every 4 words for 6205 advisory 1.4.4 */
		BootLoader_ReadMem32(pao, 0, 0);

		HpiOs_DelayMicroSeconds(100);
		/* Release C6713 from reset - revB */
		BootLoader_WriteMem32(pao, 0, C6205_BAR0_TIMER1_CTL, 4);
		HpiOs_DelayMicroSeconds(100);
	}

	for (nDsp = 0; nDsp < HPI6205_MAX_FILES_TO_LOAD; nDsp++) {
		/* is there a DSP to load? */
		if (anBootLoadFamily[nDsp] == 0)
			continue;

		err = BootLoader_ConfigEMIF(pao, nDsp);
		if (err)
			return (err);

		err = BootLoader_TestInternalMemory(pao, nDsp);
		if (err)
			return (err);

		err = BootLoader_TestExternalMemory(pao, nDsp);
		if (err)
			return (err);

		err = BootLoader_TestPld(pao, nDsp);
		if (err)
			return (err);

		/* write the DSP code down into the DSPs memory */
		DspCode.psDev = pao->Pci.pOsData;
		err = HpiDspCode_Open(anBootLoadFamily[nDsp],
			&DspCode, pdwOsErrorCode);
		if (err)
			return (err);

		while (1) {
			u32 dwLength;
			u32 dwAddress;
			u32 dwType;
			u32 *pdwCode;

			err = HpiDspCode_ReadWord(&DspCode, &dwLength);
			if (err)
				break;
			if (dwLength == 0xFFFFFFFF)
				break;	/* end of code */

			err = HpiDspCode_ReadWord(&DspCode, &dwAddress);
			if (err)
				break;
			err = HpiDspCode_ReadWord(&DspCode, &dwType);
			if (err)
				break;
			err = HpiDspCode_ReadBlock(dwLength, &DspCode,
				&pdwCode);
			if (err)
				break;
			for (i = 0; i < (int)dwLength; i++) {
				err = BootLoader_WriteMem32(pao, nDsp,
					dwAddress, *pdwCode);
				if (err)
					break;
				/* dummy read every 4 words */
				/* for 6205 advisory 1.4.4 */
				if (i % 4 == 0)
					BootLoader_ReadMem32(pao, nDsp,
						dwAddress);
				pdwCode++;
				dwAddress += 4;
			}

		}
		if (err) {
			HpiDspCode_Close(&DspCode);
			return (err);
		}

		/* verify code */
		HpiDspCode_Rewind(&DspCode);
		while (1) {
			u32 dwLength = 0;
			u32 dwAddress = 0;
			u32 dwType = 0;
			u32 *pdwCode = NULL;
			u32 dwData = 0;

			HpiDspCode_ReadWord(&DspCode, &dwLength);
			if (dwLength == 0xFFFFFFFF)
				break;	/* end of code */

			HpiDspCode_ReadWord(&DspCode, &dwAddress);
			HpiDspCode_ReadWord(&DspCode, &dwType);
			HpiDspCode_ReadBlock(dwLength, &DspCode, &pdwCode);

			for (i = 0; i < (int)dwLength; i++) {
				dwData = BootLoader_ReadMem32(pao, nDsp,
					dwAddress);
				if (dwData != *pdwCode) {
					err = 0;
					break;
				}
				pdwCode++;
				dwAddress += 4;
			}
			if (err)
				break;
		}
		HpiDspCode_Close(&DspCode);
		if (err)
			return (err);
	}

	/* After bootloading all DSPs, start DSP0 running
	 * The DSP0 code will handle starting and synchronizing with its slaves
	 */
	if (pHw6205->pInterfaceBuffer) {
		/* we need to tell the card the physical PCI address */
		u32 dwPhysicalPCIaddress;
		volatile struct bus_master_interface *interface =
			pHw6205->pInterfaceBuffer;
		u32 dwHostMailboxAddressOnDsp;
		u32 dwPhysicalPCIaddressVerify = 0;
		int nTimeOut = 10;
		/* set ack so we know when DSP is ready to go */
		/* (dwDspAck will be changed to HIF_RESET) */
		interface->dwDspAck = H620_HIF_UNKNOWN;

		err = HpiOs_LockedMem_GetPhysAddr(&pHw6205->hLockedMem,
			&dwPhysicalPCIaddress);

		/* locate the host mailbox on the DSP. */
		dwHostMailboxAddressOnDsp = 0x80000000;
		while ((dwPhysicalPCIaddress != dwPhysicalPCIaddressVerify)
			&& nTimeOut--) {
			err = BootLoader_WriteMem32(pao, 0,
				dwHostMailboxAddressOnDsp,
				dwPhysicalPCIaddress);
			dwPhysicalPCIaddressVerify =
				BootLoader_ReadMem32(pao, 0,
				dwHostMailboxAddressOnDsp);
		}
	}
	HPI_DEBUG_LOG(DEBUG, "Starting DSPs running\n");
	/* enable interrupts */
	dwTemp = ioread32(pHw6205->prHSR);
	dwTemp &= ~(u32)C6205_HSR_INTAM;
	iowrite32(dwTemp, pHw6205->prHSR);

	/* start code running... */
	dwTemp = ioread32(pHw6205->prHDCR);
	dwTemp |= (u32)C6205_HDCR_DSPINT;
	iowrite32(dwTemp, pHw6205->prHDCR);

	/* give the DSP 10ms to start up */
	HpiOs_DelayMicroSeconds(10000);
	return err;

}

/*****************************************************************************/
/* Bootloader utility functions */

static u32 BootLoader_ReadMem32(
	struct hpi_adapter_obj *pao,
	int nDSPIndex,
	u32 dwAddress
)
{
	struct hpi_hw_obj *pHw6205 = pao->priv;
	u32 dwData = 0;
	__iomem u32 *pData;

	if (nDSPIndex == 0) {	/* DSP 0 is always C6205 */
		if ((dwAddress >= 0x01800000) & (dwAddress < 0x02000000)) {
			/* BAR1 register access */
			pData = pao->Pci.apMemBase[1] +
				(dwAddress & 0x007fffff) /
				sizeof(*pao->Pci.apMemBase[1]);
		} else {
			u32 dw4MPage = dwAddress >> 22L;
			if (dw4MPage != pHw6205->dwDspPage) {
				pHw6205->dwDspPage = dw4MPage;
				/* *INDENT OFF* */
				iowrite32(pHw6205->dwDspPage,
					pHw6205->prDSPP);
				/* *INDENT-ON* */
			}
			dwAddress &= 0x3fffff;	/* address within 4M page */
			/* BAR0 memory access */
			pData = pao->Pci.apMemBase[0] +
				dwAddress / sizeof(u32);
		}
		dwData = ioread32(pData);
	} else if (nDSPIndex == 1) {	/* DSP 1 is a C6713 */
		u32 dwLsb;
		BootLoader_WriteMem32(pao, 0, HPIAL_ADDR, dwAddress);
		BootLoader_WriteMem32(pao, 0, HPIAH_ADDR, dwAddress >> 16);
		dwLsb = BootLoader_ReadMem32(pao, 0, HPIDL_ADDR);
		dwData = BootLoader_ReadMem32(pao, 0, HPIDH_ADDR);
		dwData = (dwData << 16) | (dwLsb & 0xFFFF);
	}
	return dwData;
}
static u16 BootLoader_WriteMem32(
	struct hpi_adapter_obj *pao,
	int nDSPIndex,
	u32 dwAddress,
	u32 dwData
)
{
	struct hpi_hw_obj *pHw6205 = pao->priv;
	u16 err = 0;
	__iomem u32 *pData;
	/*	u32 dwVerifyData=0; */

	if (nDSPIndex == 0) {
		/* DSP 0 is always C6205 */
		if ((dwAddress >= 0x01800000) & (dwAddress < 0x02000000)) {
			/* BAR1 - DSP  register access using */
			/* Non-prefetchable PCI access */
			pData = pao->Pci.apMemBase[1] +
				(dwAddress & 0x007fffff) /
				sizeof(*pao->Pci.apMemBase[1]);
		} else {
			/* BAR0 access - all of DSP memory using */
			/* pre-fetchable PCI access */
			u32 dw4MPage = dwAddress >> 22L;
			if (dw4MPage != pHw6205->dwDspPage) {
				pHw6205->dwDspPage = dw4MPage;
				/* *INDENT-OFF* */
				iowrite32(pHw6205->dwDspPage, pHw6205->prDSPP);
				/* *INDENT-ON* */
			}
			dwAddress &= 0x3fffff;	/* address within 4M page */
			pData = pao->Pci.apMemBase[0] +
				dwAddress / sizeof(u32);
		}
		iowrite32(dwData, pData);
	} else if (nDSPIndex == 1) {	/* DSP 1 is a C6713 */
		BootLoader_WriteMem32(pao, 0, HPIAL_ADDR, dwAddress);
		BootLoader_WriteMem32(pao, 0, HPIAH_ADDR, dwAddress >> 16);

		/* dummy read every 4 words for 6205 advisory 1.4.4 */
		BootLoader_ReadMem32(pao, 0, 0);

		BootLoader_WriteMem32(pao, 0, HPIDL_ADDR, dwData);
		BootLoader_WriteMem32(pao, 0, HPIDH_ADDR, dwData >> 16);

		/* dummy read every 4 words for 6205 advisory 1.4.4 */
		BootLoader_ReadMem32(pao, 0, 0);
	} else
		err = Hpi6205_Error(nDSPIndex, HPI6205_ERROR_BAD_DSPINDEX);
	return err;
}

static u16 BootLoader_ConfigEMIF(
	struct hpi_adapter_obj *pao,
	int nDSPIndex
)
{
	u16 err = 0;

	if (nDSPIndex == 0) {
		u32 dwSetting;

		/* DSP 0 is always C6205 */

		/* Set the EMIF */
		/* memory map of C6205 */
		/* 00000000-0000FFFF	16Kx32 internal program */
		/* 00400000-00BFFFFF	CE0	2Mx32 SDRAM running @ 100MHz */

		/* EMIF config */
		/*------------ */
		/* Global EMIF control */
		BootLoader_WriteMem32(pao, nDSPIndex, 0x01800000, 0x3779);
#define WS_OFS 28
#define WST_OFS 22
#define WH_OFS 20
#define RS_OFS 16
#define RST_OFS 8
#define MTYPE_OFS 4
#define RH_OFS 0

		/* EMIF CE0 setup - 2Mx32 Sync DRAM on ASI5000 cards only */
		dwSetting = 0x00000030;
		BootLoader_WriteMem32(pao, nDSPIndex, 0x01800008, dwSetting);
		if (dwSetting != BootLoader_ReadMem32(pao, nDSPIndex,
				0x01800008))
			return Hpi6205_Error(nDSPIndex,
				HPI6205_ERROR_DSP_EMIF);

		/* EMIF CE1 setup - 32 bit async. This is 6713 #1 HPI, */
		/* which occupies D15..0. 6713 starts at 27MHz, so need */
		/* plenty of wait states. See dsn8701.rtf, and 6713 errata. */
		/* WST should be 71, but 63  is max possible */
		dwSetting = (1L << WS_OFS) | (63L << WST_OFS) |
			(1L << WH_OFS) | (1L << RS_OFS) |
			(63L << RST_OFS) | (1L << RH_OFS) | (2L << MTYPE_OFS);
		BootLoader_WriteMem32(pao, nDSPIndex, 0x01800004, dwSetting);
		if (dwSetting != BootLoader_ReadMem32(pao, nDSPIndex,
				0x01800004))
			return Hpi6205_Error(nDSPIndex,
				HPI6205_ERROR_DSP_EMIF);

		/* EMIF CE2 setup - 32 bit async. This is 6713 #2 HPI, */
		/* which occupies D15..0. 6713 starts at 27MHz, so need */
		/* plenty of wait states */
		dwSetting = (1L << WS_OFS) |
			(28L << WST_OFS) |
			(1L << WH_OFS) |
			(1L << RS_OFS) |
			(63L << RST_OFS) | (1L << RH_OFS) | (2L << MTYPE_OFS);
		BootLoader_WriteMem32(pao, nDSPIndex, 0x01800010, dwSetting);
		if (dwSetting != BootLoader_ReadMem32(pao, nDSPIndex,
				0x01800010))
			return Hpi6205_Error(nDSPIndex,
				HPI6205_ERROR_DSP_EMIF);

		/* EMIF CE3 setup - 32 bit async. */
		/* This is the PLD on the ASI5000 cards only */
		dwSetting = (1L << WS_OFS) |
			(10L << WST_OFS) |
			(1L << WH_OFS) |
			(1L << RS_OFS) |
			(10L << RST_OFS) | (1L << RH_OFS) | (2L << MTYPE_OFS);
		BootLoader_WriteMem32(pao, nDSPIndex, 0x01800014, dwSetting);
		if (dwSetting != BootLoader_ReadMem32(pao, nDSPIndex,
				0x01800014))
			return Hpi6205_Error(nDSPIndex,
				HPI6205_ERROR_DSP_EMIF);

		/* set EMIF SDRAM control for 2Mx32 SDRAM (512x32x4 bank) */
		/*  need to use this else DSP code crashes? */
		BootLoader_WriteMem32(pao, nDSPIndex, 0x01800018, 0x07117000);

		/* EMIF SDRAM Refresh Timing */
		/* EMIF SDRAM timing  (orig = 0x410, emulator = 0x61a) */
		BootLoader_WriteMem32(pao, nDSPIndex, 0x0180001C, 0x00000410);

	} else if (nDSPIndex == 1) {
		/* test access to the C6713s HPI registers */
		u32 dwWriteData = 0, dwReadData = 0, i = 0;

		/* Set up HPIC for little endian, by setiing HPIC:HWOB=1 */
		dwWriteData = 1;
		BootLoader_WriteMem32(pao, 0, HPICL_ADDR, dwWriteData);
		BootLoader_WriteMem32(pao, 0, HPICH_ADDR, dwWriteData);
		/* C67 HPI is on lower 16bits of 32bit EMIF */
		dwReadData = 0xFFF7 & BootLoader_ReadMem32(pao, 0,
			HPICL_ADDR);
		if (dwWriteData != dwReadData) {
			err = Hpi6205_Error(nDSPIndex,
				HPI6205_ERROR_C6713_HPIC);
			HPI_DEBUG_LOG(ERROR,
				"HPICL %x %x\n", dwWriteData, dwReadData);

			return err;
		}
		/* HPIA - walking ones test */
		dwWriteData = 1;
		for (i = 0; i < 32; i++) {
			BootLoader_WriteMem32(pao, 0, HPIAL_ADDR,
				dwWriteData);
			BootLoader_WriteMem32(pao, 0, HPIAH_ADDR,
				(dwWriteData >> 16));
			dwReadData = 0xFFFF & BootLoader_ReadMem32(pao, 0,
				HPIAL_ADDR);
			dwReadData = dwReadData |
				((0xFFFF & BootLoader_ReadMem32(pao,
						0, HPIAH_ADDR))
				<< 16);
			if (dwReadData != dwWriteData) {
				err = Hpi6205_Error(nDSPIndex,
					HPI6205_ERROR_C6713_HPIA);
				HPI_DEBUG_LOG(ERROR,
					"HPIA %x %x\n",
					dwWriteData, dwReadData);
				return err;
			}
			dwWriteData = dwWriteData << 1;
		}

		/* setup C67x PLL
		 *  ** C6713 datasheet says we cannot program PLL from HPI,
		 * and indeed if we try to set the PLL multiply from the HPI,
		 * the PLL does not seem to lock, so we enable the PLL and
		 * use the default multiply of x 7, which for a 27MHz clock
		 * gives a DSP speed of 189MHz
		 */
		/* bypass PLL */
		BootLoader_WriteMem32(pao, nDSPIndex, 0x01B7C100, 0x0000);
		HpiOs_DelayMicroSeconds(1000);
		/* EMIF = 189/3=63MHz */
		BootLoader_WriteMem32(pao, nDSPIndex, 0x01B7C120, 0x8002);
		/* peri = 189/2 */
		BootLoader_WriteMem32(pao, nDSPIndex, 0x01B7C11C, 0x8001);
		/* cpu	= 189/1 */
		BootLoader_WriteMem32(pao, nDSPIndex, 0x01B7C118, 0x8000);
		HpiOs_DelayMicroSeconds(1000);
		/* ** SGT test to take GPO3 high when we start the PLL */
		/* and low when the delay is completed */
		/* FSX0 <- '1' (GPO3) */
		BootLoader_WriteMem32(pao, 0, (0x018C0024L), 0x00002A0A);
		/* PLL not bypassed */
		BootLoader_WriteMem32(pao, nDSPIndex, 0x01B7C100, 0x0001);
		HpiOs_DelayMicroSeconds(1000);
		/* FSX0 <- '0' (GPO3) */
		BootLoader_WriteMem32(pao, 0, (0x018C0024L), 0x00002A02);

		/* 6205 EMIF CE1 resetup - 32 bit async. */
		/* Now 6713 #1 is running at 189MHz can reduce waitstates */
		BootLoader_WriteMem32(pao, 0, 0x01800004,	/* CE1 */
			(1L << WS_OFS) |
			(8L << WST_OFS) |
			(1L << WH_OFS) |
			(1L << RS_OFS) |
			(12L << RST_OFS) |
			(1L << RH_OFS) | (2L << MTYPE_OFS));

		HpiOs_DelayMicroSeconds(1000);

		/* check that we can read one of the PLL registers */
		/* PLL should not be bypassed! */
		if ((BootLoader_ReadMem32(pao, nDSPIndex, 0x01B7C100) & 0xF)
			!= 0x0001) {
			err = Hpi6205_Error(nDSPIndex,
				HPI6205_ERROR_C6713_PLL);
			return err;
		}
		/* setup C67x EMIF */
		BootLoader_WriteMem32(pao, nDSPIndex, C6713_EMIF_GCTL,
			0x000034A8);
		BootLoader_WriteMem32(pao, nDSPIndex, C6713_EMIF_CE0,
			0x00000030);
		BootLoader_WriteMem32(pao, nDSPIndex, C6713_EMIF_SDRAMEXT,
			0x001BDF29);
		BootLoader_WriteMem32(pao, nDSPIndex, C6713_EMIF_SDRAMCTL,
			0x47117000);
		BootLoader_WriteMem32(pao, nDSPIndex, C6713_EMIF_SDRAMTIMING,
			0x00000410);

		HpiOs_DelayMicroSeconds(1000);
	} else if (nDSPIndex == 2) {
		/* DSP 2 is a C6713 */

	} else
		err = Hpi6205_Error(nDSPIndex, HPI6205_ERROR_BAD_DSPINDEX);
	return err;
}

static u16 BootLoader_TestMemory(
	struct hpi_adapter_obj *pao,
	int nDSPIndex,
	u32 dwStartAddress,
	u32 dwLength
)
{
	u32 i = 0, j = 0;
	u32 dwTestAddr = 0;
	u32 dwTestData = 0, dwData = 0;

	dwLength = 1000;

	/* for 1st word, test each bit in the 32bit word, */
	/* dwLength specifies number of 32bit words to test */
	/*for(i=0; i<dwLength; i++) */
	i = 0;
	{
		dwTestAddr = dwStartAddress + (u32)i *4;
		dwTestData = 0x00000001;
		for (j = 0; j < 32; j++) {
			BootLoader_WriteMem32(pao, nDSPIndex, dwTestAddr,
				dwTestData);
			dwData = BootLoader_ReadMem32(pao, nDSPIndex,
				dwTestAddr);
			if (dwData != dwTestData) {
				HPI_DEBUG_LOG(VERBOSE,
					"Memtest error details	"
					"%08x %08x %08x %i\n",
					dwTestAddr, dwTestData,
					dwData, nDSPIndex);
				return (1);	/* error */
			}
			dwTestData = dwTestData << 1;
		}		/* for(j) */
	}			/* for(i) */

	/* for the next 100 locations test each location, leaving it as zero */
	/* write a zero to the next word in memory before we read */
	/* the previous write to make sure every memory location is unique */
	for (i = 0; i < 100; i++) {
		dwTestAddr = dwStartAddress + (u32)i *4;
		dwTestData = 0xA5A55A5A;
		BootLoader_WriteMem32(pao, nDSPIndex, dwTestAddr, dwTestData);
		BootLoader_WriteMem32(pao, nDSPIndex, dwTestAddr + 4, 0);
		dwData = BootLoader_ReadMem32(pao, nDSPIndex, dwTestAddr);
		if (dwData != dwTestData) {
			HPI_DEBUG_LOG(VERBOSE,
				"Memtest error details	"
				"%08x %08x %08x %i\n",
				dwTestAddr, dwTestData, dwData, nDSPIndex);
			return (1);	/* error */
		}
		/* leave location as zero */
		BootLoader_WriteMem32(pao, nDSPIndex, dwTestAddr, 0x0);
	}

	/* zero out entire memory block */
	for (i = 0; i < dwLength; i++) {
		dwTestAddr = dwStartAddress + (u32)i *4;
		BootLoader_WriteMem32(pao, nDSPIndex, dwTestAddr, 0x0);
	}
	return (0);		/*success! */
}

static u16 BootLoader_TestInternalMemory(
	struct hpi_adapter_obj *pao,
	int nDSPIndex
)
{
	int err = 0;
	if (nDSPIndex == 0) {
		/* DSP 0 is a C6205 */
		/* 64K prog mem */
		err = BootLoader_TestMemory(pao, nDSPIndex, 0x00000000,
			0x10000);
		if (!err)
			/* 64K data mem */
			err = BootLoader_TestMemory(pao, nDSPIndex,
				0x80000000, 0x10000);
	} else if ((nDSPIndex == 1) || (nDSPIndex == 2)) {
		/* DSP 1&2 are a C6713 */
		/* 192K internal mem */
		err = BootLoader_TestMemory(pao, nDSPIndex, 0x00000000,
			0x30000);
		if (!err)
			/* 64K internal mem / L2 cache */
			err = BootLoader_TestMemory(pao, nDSPIndex,
				0x00030000, 0x10000);
	} else
		return Hpi6205_Error(nDSPIndex, HPI6205_ERROR_BAD_DSPINDEX);

	if (err)
		return (Hpi6205_Error(nDSPIndex, HPI6205_ERROR_DSP_INTMEM));
	else
		return 0;
}
static u16 BootLoader_TestExternalMemory(
	struct hpi_adapter_obj *pao,
	int nDSPIndex
)
{
	u32 dwDRAMStartAddress = 0;
	u32 dwDRAMSize = 0;

	if (nDSPIndex == 0) {
		/* only test for SDRAM if an ASI5000 card */
		if (pao->Pci.wSubSysDeviceId == 0x5000) {
			/* DSP 0 is always C6205 */
			dwDRAMStartAddress = 0x00400000;
			dwDRAMSize = 0x200000;
			/*dwDRAMinc=1024; */
		} else
			return (0);
	} else if ((nDSPIndex == 1) || (nDSPIndex == 2)) {
		/* DSP 1 is a C6713 */
		dwDRAMStartAddress = 0x80000000;
		dwDRAMSize = 0x200000;
		/*dwDRAMinc=1024; */
	} else
		return Hpi6205_Error(nDSPIndex, HPI6205_ERROR_BAD_DSPINDEX);

	if (BootLoader_TestMemory
		(pao, nDSPIndex, dwDRAMStartAddress, dwDRAMSize))
		return (Hpi6205_Error(nDSPIndex, HPI6205_ERROR_DSP_EXTMEM));
	return 0;
}

static u16 BootLoader_TestPld(
	struct hpi_adapter_obj *pao,
	int nDSPIndex
)
{
	u32 dwData = 0;
	if (nDSPIndex == 0) {
		/* only test for DSP0 PLD on ASI5000 card */
		if (pao->Pci.wSubSysDeviceId == 0x5000) {
			/* PLD is located at CE3=0x03000000 */
			dwData = BootLoader_ReadMem32(pao, nDSPIndex,
				0x03000008);
			if ((dwData & 0xF) != 0x5)
				return (Hpi6205_Error
					(nDSPIndex, HPI6205_ERROR_DSP_PLD));
			dwData = BootLoader_ReadMem32(pao, nDSPIndex,
				0x0300000C);
			if ((dwData & 0xF) != 0xA)
				return (Hpi6205_Error
					(nDSPIndex, HPI6205_ERROR_DSP_PLD));
		}
	} else if (nDSPIndex == 1) {
		/* DSP 1 is a C6713 */
		if (pao->Pci.wSubSysDeviceId == 0x8700) {
			/* PLD is located at CE1=0x90000000 */
			dwData = BootLoader_ReadMem32(pao, nDSPIndex,
				0x90000010);
			if ((dwData & 0xFF) != 0xAA)
				return (Hpi6205_Error
					(nDSPIndex, HPI6205_ERROR_DSP_PLD));
			/* 8713 - LED on */
			BootLoader_WriteMem32(pao, nDSPIndex, 0x90000000,
				0x02);
		}
	}
	return (0);
}

/** Transfer data to or from DSP
 nOperation = H620_H620_HIF_SEND_DATA or H620_HIF_GET_DATA
*/
static short Hpi6205_TransferData(
	struct hpi_adapter_obj *pao,
	u8 *pData,
	u32 dwDataSize,
	int nOperation
)
{
	struct hpi_hw_obj *pHw6205 = pao->priv;
	u32 dwDataTransferred = 0;
	/*u8  *pData =(u8  *)phm->u.d.u.Data.dwpbData; */
	/*u16 wTimeOut=8; */
	u16 err = 0;
	u32 dwTimeOut, dwTemp1, dwTemp2;
	volatile struct bus_master_interface *interface =
		pHw6205->pInterfaceBuffer;

	dwDataSize &= ~3L;	/* round dwDataSize down to nearest 4 bytes */

	/* make sure state is IDLE */
	dwTimeOut = HPI6205_TIMEOUT;
	dwTemp2 = 0;
	while ((interface->dwDspAck != H620_HIF_IDLE) && dwTimeOut--)
		HpiOs_DelayMicroSeconds(1);

	if (interface->dwDspAck != H620_HIF_IDLE)
		return HPI_ERROR_DSP_HARDWARE;

	interface->dwHostCmd = nOperation;

	while (dwDataTransferred < dwDataSize) {
		u32 nThisCopy = dwDataSize - dwDataTransferred;

		if (nThisCopy > HPI6205_SIZEOF_DATA)
			nThisCopy = HPI6205_SIZEOF_DATA;

		if (nOperation == H620_HIF_SEND_DATA)
			memcpy((void *)&interface->u.bData[0],
				&pData[dwDataTransferred], nThisCopy);

		interface->dwTransferSizeInBytes = nThisCopy;

		/* interrupt the DSP */
		dwTemp1 = ioread32(pHw6205->prHDCR);
		dwTemp1 |= (u32)C6205_HDCR_DSPINT;
		iowrite32(dwTemp1, pHw6205->prHDCR);
		dwTemp1 &= ~(u32)C6205_HDCR_DSPINT;
		iowrite32(dwTemp1, pHw6205->prHDCR);

		/* spin waiting on the result */
		dwTimeOut = HPI6205_TIMEOUT;
		dwTemp2 = 0;
		while ((dwTemp2 == 0) && dwTimeOut--) {
			/* give 16k bus mastering transfer time to happen */
			/*(16k / 132Mbytes/s = 122usec) */
			HpiOs_DelayMicroSeconds(20);
			dwTemp2 = ioread32(pHw6205->prHSR);
			dwTemp2 &= C6205_HSR_INTSRC;
		}
		HPI_DEBUG_LOG(DEBUG, "Spun %d times for data xfer of %d\n",
			HPI6205_TIMEOUT - dwTimeOut, nThisCopy);
		if (dwTemp2 == C6205_HSR_INTSRC) {
			HPI_DEBUG_LOG(VERBOSE,
				"Interrupt from HIF <data> OK\n");
			/*
			   if(interface->dwDspAck != nOperation) {
			   HPI_DEBUG_LOG(DEBUG("interface->dwDspAck=%d,
			   expected %d \n",
			   interface->dwDspAck,nOperation);
			   }
			 */
		}
/* need to handle this differently... */
		else {
			HPI_DEBUG_LOG(ERROR,
				"Interrupt from HIF <data> BAD\n");
			err = HPI_ERROR_DSP_HARDWARE;
		}

		/* reset the interrupt from the DSP */
		iowrite32(C6205_HSR_INTSRC, pHw6205->prHSR);

		if (nOperation == H620_HIF_GET_DATA)
			memcpy(&pData[dwDataTransferred],
				(void *)&interface->u.bData[0], nThisCopy);

		dwDataTransferred += nThisCopy;
	}
	if (interface->dwDspAck != nOperation)
		HPI_DEBUG_LOG(DEBUG, "interface->dwDspAck=%d, expected %d\n",
			interface->dwDspAck, nOperation);
	/*			err=HPI_ERROR_DSP_HARDWARE; */

	/* set interface back to idle */
	interface->dwHostCmd = H620_HIF_IDLE;
	/* interrupt the DSP again */
	dwTemp1 = ioread32(pHw6205->prHDCR);
	dwTemp1 |= (u32)C6205_HDCR_DSPINT;
	iowrite32(dwTemp1, pHw6205->prHDCR);
	dwTemp1 &= ~(u32)C6205_HDCR_DSPINT;
	iowrite32(dwTemp1, pHw6205->prHDCR);

	return err;
}

static unsigned int messageCount;

static u16 Hpi6205_MessageResponseSequence(
	struct hpi_adapter_obj *pao,
	struct hpi_message *phm,
	struct hpi_response *phr
)
{
	u32 dwTemp1, dwTemp2, dwTimeOut, dwTimeOut2;
	struct hpi_hw_obj *pHw6205 = pao->priv;
	volatile struct bus_master_interface *interface =
		pHw6205->pInterfaceBuffer;
	u16 err = 0;

	messageCount++;
	/* Assume buffer of type struct bus_master_interface
	   is allocated "noncacheable" */

	/* make sure state is IDLE */
	dwTimeOut = HPI6205_TIMEOUT;
	dwTemp2 = 0;
	while ((interface->dwDspAck != H620_HIF_IDLE) && --dwTimeOut)
		HpiOs_DelayMicroSeconds(1);

	if (dwTimeOut == 0) {
		HPI_DEBUG_LOG(DEBUG, "Timeout waiting for idle\n");
		return (Hpi6205_Error
			(0, HPI6205_ERROR_MSG_RESP_IDLE_TIMEOUT));
	}
	interface->u.MessageBuffer = *phm;
	/* signal we want a response */
	interface->dwHostCmd = H620_HIF_GET_RESP;

	/* interrupt the DSP */
	dwTemp1 = ioread32(pHw6205->prHDCR);
	dwTemp1 |= (u32)C6205_HDCR_DSPINT;
	HpiOs_DelayMicroSeconds(1);
	iowrite32(dwTemp1, pHw6205->prHDCR);
	dwTemp1 &= ~(u32)C6205_HDCR_DSPINT;
	iowrite32(dwTemp1, pHw6205->prHDCR);

	/* spin waiting on state change (start of msg process) */
	dwTimeOut2 = HPI6205_TIMEOUT;
	dwTemp2 = 0;
	while ((interface->dwDspAck != H620_HIF_GET_RESP) && --dwTimeOut2) {
		dwTemp2 = ioread32(pHw6205->prHSR);
		dwTemp2 &= C6205_HSR_INTSRC;
	}
	if (dwTimeOut2 == 0) {
		HPI_DEBUG_LOG(DEBUG,
			"(%u)Timed out waiting for "
			"GET_RESP state [%x]\n",
			messageCount, interface->dwDspAck);
	} else {
		HPI_DEBUG_LOG(VERBOSE,
			"(%u)Transition to GET_RESP after %u\n",
			messageCount, HPI6205_TIMEOUT - dwTimeOut2);
	}

	/* spin waiting on HIF interrupt flag (end of msg process) */
	dwTimeOut = HPI6205_TIMEOUT;
	dwTemp2 = 0;
	while ((dwTemp2 == 0) && --dwTimeOut) {
		dwTemp2 = ioread32(pHw6205->prHSR);
		dwTemp2 &= C6205_HSR_INTSRC;
		/* HpiOs_DelayMicroSeconds(5); */
	}
	if (dwTemp2 == C6205_HSR_INTSRC) {
		if ((interface->dwDspAck != H620_HIF_GET_RESP)) {
			HPI_DEBUG_LOG(DEBUG,
				"(%u)interface->dwDspAck(0x%x) != "
				"H620_HIF_GET_RESP, t=%u\n",
				messageCount, interface->dwDspAck,
				HPI6205_TIMEOUT - dwTimeOut);
		} else {
			HPI_DEBUG_LOG(VERBOSE,
				"(%u)Int with GET_RESP after %u\n",
				messageCount, HPI6205_TIMEOUT - dwTimeOut);
		}

	}
/* need to handle this differently... */
	else
		HPI_DEBUG_LOG(ERROR,
			"Interrupt from HIF module BAD (wFunction %x)\n",
			phm->wFunction);

	/* reset the interrupt from the DSP */
	iowrite32(C6205_HSR_INTSRC, pHw6205->prHSR);

	/* read the result */
	if (dwTimeOut != 0)
		*phr = interface->u.ResponseBuffer;

	/* set interface back to idle */
	interface->dwHostCmd = H620_HIF_IDLE;
	/* interrupt the DSP again */
	dwTemp1 = ioread32(pHw6205->prHDCR);
	dwTemp1 |= (u32)C6205_HDCR_DSPINT;
	iowrite32(dwTemp1, pHw6205->prHDCR);
	dwTemp1 &= ~(u32)C6205_HDCR_DSPINT;
	iowrite32(dwTemp1, pHw6205->prHDCR);

/* EWB move timeoutcheck to after IDLE command, maybe recover? */
	if ((dwTimeOut == 0) || (dwTimeOut2 == 0)) {
		HPI_DEBUG_LOG(DEBUG, "Something timed out!\n");
		return Hpi6205_Error(0, HPI6205_ERROR_MSG_RESP_TIMEOUT);
	}
	/* special case for adapter close - */
	/* wait for the DSP to indicate it is idle */
	if (phm->wFunction == HPI_ADAPTER_CLOSE) {
		/* make sure state is IDLE */
		dwTimeOut = HPI6205_TIMEOUT;
		dwTemp2 = 0;
		while ((interface->dwDspAck != H620_HIF_IDLE) && --dwTimeOut)
			HpiOs_DelayMicroSeconds(1);

		if (dwTimeOut == 0) {
			HPI_DEBUG_LOG(DEBUG,
				"Timeout waiting for idle "
				"(on AdapterClose)\n");
			return (Hpi6205_Error
				(0, HPI6205_ERROR_MSG_RESP_IDLE_TIMEOUT));
		}
	}
	err = HpiValidateResponse(phm, phr);
	return err;
}

static void HW_Message(
	struct hpi_adapter_obj *pao,
	struct hpi_message *phm,
	struct hpi_response *phr
)
{

	u16 err = 0;

	HpiOs_Dsplock_Lock(pao);

	err = Hpi6205_MessageResponseSequence(pao, phm, phr);

	/* maybe an error response */
	if (err) {
		/* something failed in the HPI/DSP interface */
		phr->wError = err;
		/* just the header of the response is valid */
		phr->wSize = sizeof(struct hpi_response_header);
		goto err;
	}
	if (phr->wError != 0)	/* something failed in the DSP */
		goto err;

	switch (phm->wFunction) {
	case HPI_OSTREAM_WRITE:
	case HPI_ISTREAM_ANC_WRITE:
		err = Hpi6205_TransferData(pao,
			phm->u.d.u.Data.pbData,
			phm->u.d.u.Data.dwDataSize, H620_HIF_SEND_DATA);
		break;

	case HPI_ISTREAM_READ:
	case HPI_OSTREAM_ANC_READ:
		err = Hpi6205_TransferData(pao,
			phm->u.d.u.Data.pbData,
			phm->u.d.u.Data.dwDataSize, H620_HIF_GET_DATA);
		break;

	case HPI_CONTROL_SET_STATE:
		if (phm->wObject == HPI_OBJ_CONTROLEX
			&& phm->u.cx.wAttribute == HPI_COBRANET_SET_DATA)
			err = Hpi6205_TransferData(pao,
				phm->u.cx.u.cobranet_bigdata.pbData,
				phm->u.cx.u.cobranet_bigdata.
				dwByteCount, H620_HIF_SEND_DATA);
		break;

	case HPI_CONTROL_GET_STATE:
		if (phm->wObject == HPI_OBJ_CONTROLEX
			&& phm->u.cx.wAttribute == HPI_COBRANET_GET_DATA)
			err = Hpi6205_TransferData(pao,
				phm->u.cx.u.cobranet_bigdata.pbData,
				phr->u.cx.u.cobranet_data.dwByteCount,
				H620_HIF_GET_DATA);
		break;
	}
	phr->wError = err;

err:
	HpiOs_Dsplock_UnLock(pao);

	return;
}
