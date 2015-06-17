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

 Hardware Programming Interface (HPI) for AudioScience ASI6200 series adapters.
 These PCI bus adapters are based on the TI C6711 DSP.

 Exported functions:
 void HPI_6000(struct hpi_message *phm, struct hpi_response *phr)

 #defines
 HIDE_PCI_ASSERTS to show the PCI asserts
 PROFILE_DSP2 get profile data from DSP2 if present (instead of DSP 1)

(C) Copyright AudioScience Inc. 1998-2003
*******************************************************************************/
#define SOURCEFILE_NAME "hpi6000.c"

#include "hpi.h"
#include "hpidebug.h"
#include "hpi6000.h"
#include "hpidspcd.h"
#include "hpicmn.h"

#ifndef offsetof
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif

#define HPI_HIF_BASE (0x00000200)	/* start of C67xx internal RAM */
#define HPI_HIF_ADDR(member) \
	(HPI_HIF_BASE + offsetof(struct hpi_hif_6000, member))
#define HPI_HIF_ERROR_MASK	0x4000

/* HPI6000 specific error codes */

#define HPI6000_ERROR_BASE				900
#define HPI6000_ERROR_MSG_RESP_IDLE_TIMEOUT		901
#define HPI6000_ERROR_MSG_RESP_SEND_MSG_ACK		902
#define HPI6000_ERROR_MSG_RESP_GET_RESP_ACK		903
#define HPI6000_ERROR_MSG_GET_ADR			904
#define HPI6000_ERROR_RESP_GET_ADR			905
#define HPI6000_ERROR_MSG_RESP_BLOCKWRITE32		906
#define HPI6000_ERROR_MSG_RESP_BLOCKREAD32		907
#define HPI6000_ERROR_MSG_INVALID_DSP_INDEX		908
#define HPI6000_ERROR_CONTROL_CACHE_PARAMS		909

#define HPI6000_ERROR_SEND_DATA_IDLE_TIMEOUT		911
#define HPI6000_ERROR_SEND_DATA_ACK			912
#define HPI6000_ERROR_SEND_DATA_ADR			913
#define HPI6000_ERROR_SEND_DATA_TIMEOUT			914
#define HPI6000_ERROR_SEND_DATA_CMD			915
#define HPI6000_ERROR_SEND_DATA_WRITE			916
#define HPI6000_ERROR_SEND_DATA_IDLECMD			917
#define HPI6000_ERROR_SEND_DATA_VERIFY			918

#define HPI6000_ERROR_GET_DATA_IDLE_TIMEOUT		921
#define HPI6000_ERROR_GET_DATA_ACK			922
#define HPI6000_ERROR_GET_DATA_CMD			923
#define HPI6000_ERROR_GET_DATA_READ			924
#define HPI6000_ERROR_GET_DATA_IDLECMD			925

#define HPI6000_ERROR_CONTROL_CACHE_ADDRLEN		951
#define HPI6000_ERROR_CONTROL_CACHE_READ		952
#define HPI6000_ERROR_CONTROL_CACHE_FLUSH		953

#define HPI6000_ERROR_MSG_RESP_GETRESPCMD		961
#define HPI6000_ERROR_MSG_RESP_IDLECMD			962
#define HPI6000_ERROR_MSG_RESP_BLOCKVERIFY32		963

/* adapter init errors */
#define HPI6000_ERROR_UNHANDLED_SUBSYS_ID		930

/* can't access PCI2040 */
#define HPI6000_ERROR_INIT_PCI2040			931
/* can't access DSP HPI i/f */
#define HPI6000_ERROR_INIT_DSPHPI			932
/* can't access internal DSP memory */
#define HPI6000_ERROR_INIT_DSPINTMEM			933
/* can't access SDRAM - test#1 */
#define HPI6000_ERROR_INIT_SDRAM1			934
/* can't access SDRAM - test#2 */
#define HPI6000_ERROR_INIT_SDRAM2			935

#define HPI6000_ERROR_INIT_VERIFY			938

#define HPI6000_ERROR_INIT_NOACK			939

#define HPI6000_ERROR_INIT_PLDTEST1			941
#define HPI6000_ERROR_INIT_PLDTEST2			942

/* local defines */

#define HIDE_PCI_ASSERTS
#define PROFILE_DSP2

/* for PCI2040 i/f chip */
/* HPI CSR registers */
/* word offsets from CSR base */
/* use when io addresses defined as u32 * */

#define INTERRUPT_EVENT_SET	0
#define INTERRUPT_EVENT_CLEAR	1
#define INTERRUPT_MASK_SET	2
#define INTERRUPT_MASK_CLEAR	3
#define HPI_ERROR_REPORT	4
#define HPI_RESET		5
#define HPI_DATA_WIDTH		6

#define MAX_DSPS 2
/* HPI registers, spaced 8K bytes = 2K words apart */
#define DSP_SPACING		0x800

#define CONTROL			0x0000
#define ADDRESS			0x0200
#define DATA_AUTOINC		0x0400
#define DATA			0x0600

#define TIMEOUT 500000

struct dsp_obj {
	__iomem u32 *prHPIControl;
	__iomem u32 *prHPIAddress;
	__iomem u32 *prHPIData;
	__iomem u32 *prHPIDataAutoInc;
	char cDspRev;		/*A, B */
	u32 dwControlCacheAddressOnDSP;
	u32 dwControlCacheLengthOnDSP;
	struct hpi_adapter_obj *paParentAdapter;
};

struct hpi_hw_obj {
	__iomem u32 *dw2040_HPICSR;
	__iomem u32 *dw2040_HPIDSP;

	u16 wNumDsp;
	struct dsp_obj ado[MAX_DSPS];

	u32 dwMessageBufferAddressOnDSP;
	u32 dwResponseBufferAddressOnDSP;
	u32 dwPCI2040HPIErrorCount;

	/* counts consecutive communications errors reported from DSP  */
	u16 wNumErrors;
	u16 wDspCrashed;	/* when '1' DSP has crashed/died/OTL */

	struct hpi_control_cache_single aControlCache[HPI_NMIXER_CONTROLS];
};

static u16 Hpi6000_DspBlockWrite32(
	struct hpi_adapter_obj *pao,
	u16 wDspIndex,
	u32 dwHpiAddress,
	u32 *dwSource,
	u32 dwCount
);
static u16 Hpi6000_DspBlockRead32(
	struct hpi_adapter_obj *pao,
	u16 wDspIndex,
	u32 dwHpiAddress,
	u32 *dwDest,
	u32 dwCount
);

static short Hpi6000_AdapterBootLoadDsp(
	struct hpi_adapter_obj *pao,
	u32 *pdwOsErrorCode
);
static short Hpi6000_Check_PCI2040_ErrorFlag(
	struct hpi_adapter_obj *pao,
	u16 nReadOrWrite
);
#define H6READ 1
#define H6WRITE 0

static short Hpi6000_UpdateControlCache(
	struct hpi_adapter_obj *pao,
	struct hpi_message *phm
);
static short Hpi6000_MessageResponseSequence(
	struct hpi_adapter_obj *pao,
	struct hpi_message *phm,
	struct hpi_response *phr
);

static void HW_Message(
	struct hpi_adapter_obj *pao,
	struct hpi_message *phm,
	struct hpi_response *phr
);
static short Hpi6000_WaitDspAck(
	struct hpi_adapter_obj *pao,
	u16 wDspIndex,
	u32 dwAckValue
);
static short Hpi6000_SendHostCommand(
	struct hpi_adapter_obj *pao,
	u16 wDspIndex,
	u32 dwHostCmd
);
static void Hpi6000_SendDspInterrupt(
	struct dsp_obj *pdo
);
static short Hpi6000_SendData(
	struct hpi_adapter_obj *pao,
	struct hpi_message *phm,
	struct hpi_response *phr
);
static short Hpi6000_GetData(
	struct hpi_adapter_obj *pao,
	struct hpi_message *phm,
	struct hpi_response *phr
);

static void HpiWriteWord(
	struct dsp_obj *pdo,
	u32 dwAddress,
	u32 dwData
);
static u32 HpiReadWord(
	struct dsp_obj *pdo,
	u32 dwAddress
);
static void HpiWriteBlock(
	struct dsp_obj *pdo,
	u32 dwAddress,
	u32 *pdwData,
	u32 dwLength
);
static void HpiReadBlock(
	struct dsp_obj *pdo,
	u32 dwAddress,
	u32 *pdwData,
	u32 dwLength
);

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

static short CreateAdapterObj(
	struct hpi_adapter_obj *pao,
	u32 *pdwOsErrorCode
);

/* local globals */

static u16 gwPciReadAsserts;	/* used to count PCI2040 errors */
static u16 gwPciWriteAsserts;	/* used to count PCI2040 errors */

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

	switch (phm->wFunction) {
	case HPI_CONTROL_GET_STATE:
		if (pao->wHasControlCache) {
			u16 err;
			err = Hpi6000_UpdateControlCache(pao, phm);

			if (err) {
				phr->wError = err;
				break;
			}

			if (HpiCheckControlCache
				(&((struct hpi_hw_obj *)pao->priv)->
					aControlCache[phm->u.c.wControlIndex],
					phm, phr))
				break;
		}
		HW_Message(pao, phm, phr);
		break;
	case HPI_CONTROL_GET_INFO:
		HW_Message(pao, phm, phr);
		break;
	case HPI_CONTROL_SET_STATE:
		HW_Message(pao, phm, phr);
		HpiSyncControlCache(&((struct hpi_hw_obj *)pao->priv)->
			aControlCache[phm->u.c.wControlIndex], phm, phr);
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
	case HPI_ADAPTER_OPEN:
	case HPI_ADAPTER_CLOSE:
	case HPI_ADAPTER_TEST_ASSERT:
	case HPI_ADAPTER_SELFTEST:
	case HPI_ADAPTER_GET_MODE:
	case HPI_ADAPTER_SET_MODE:
	case HPI_ADAPTER_FIND_OBJECT:
	case HPI_ADAPTER_GET_PROPERTY:
	case HPI_ADAPTER_SET_PROPERTY:
	case HPI_ADAPTER_ENUM_PROPERTY:
		HW_Message(pao, phm, phr);
		break;
	default:
		phr->wError = HPI_ERROR_INVALID_FUNC;
		break;
	}
}

static void OStreamMessage(
	struct hpi_adapter_obj *pao,
	struct hpi_message *phm,
	struct hpi_response *phr
)
{
	switch (phm->wFunction) {
	case HPI_OSTREAM_HOSTBUFFER_ALLOC:
	case HPI_OSTREAM_HOSTBUFFER_FREE:
		/* Don't let these messages go to the HW function because
		 * they're called without allocating the spinlock.
		 * For the HPI6000 adapters the HW would return
		 * HPI_ERROR_INVALID_FUNC anyway.
		 */
		phr->wError = HPI_ERROR_INVALID_FUNC;
		break;
	default:
		HW_Message(pao, phm, phr);
		return;
	}
}

static void IStreamMessage(
	struct hpi_adapter_obj *pao,
	struct hpi_message *phm,
	struct hpi_response *phr
)
{

	switch (phm->wFunction) {
	case HPI_ISTREAM_HOSTBUFFER_ALLOC:
	case HPI_ISTREAM_HOSTBUFFER_FREE:
		/* Don't let these messages go to the HW function because
		 * they're called without allocating the spinlock.
		 * For the HPI6000 adapters the HW would return
		 * HPI_ERROR_INVALID_FUNC anyway.
		 */
		phr->wError = HPI_ERROR_INVALID_FUNC;
		break;
	default:
		HW_Message(pao, phm, phr);
		return;
	}
}

/************************************************************************/
/** HPI_6000()
 * Entry point from HPIMAN
 * All calls to the HPI start here
 */
void HPI_6000(
	struct hpi_message *phm,
	struct hpi_response *phr
)
{
	struct hpi_adapter_obj *pao = NULL;

	/* subsytem messages get executed by every HPI. */
	/* All other messages are ignored unless the adapter index matches */
	/* an adapter in the HPI */
	HPI_DEBUG_LOG(DEBUG, "O %d,F %d\n", phm->wObject, phm->wFunction);

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
			HPI_DEBUG_LOG(DEBUG, " %d,%d dsp crashed.\n",
				phm->wObject, phm->wFunction);
			return;
		}
	}
	/* Init default response including the size field */
	if (phm->wFunction != HPI_SUBSYS_CREATE_ADAPTER)
		HPI_InitResponse(phr, phm->wObject, phm->wFunction,
			HPI_ERROR_PROCESSING_MESSAGE);

	switch (phm->wType) {
	case HPI_TYPE_MESSAGE:
		switch (phm->wObject) {
		case HPI_OBJ_SUBSYSTEM:
			SubSysMessage(phm, phr);
			break;

		case HPI_OBJ_ADAPTER:
			phr->wSize =
				sizeof(struct hpi_response_header) +
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

/************************************************************************/
/* SUBSYSTEM */

/* create an adapter object and initialise it based on resource information
 * passed in in the message
 * NOTE - you cannot use this function AND the FindAdapters function at the
 * same time, the application must use only one of them to get the adapters
 */
static void SubSysCreateAdapter(
	struct hpi_message *phm,
	struct hpi_response *phr
)
{
	/* create temp adapter obj, because we don't know what index yet */
	struct hpi_adapter_obj ao;
	struct hpi_adapter_obj *pao;
	u32 dwOsErrorCode;
	short nError = 0;
	u32 dwDspIndex = 0;

	HPI_DEBUG_LOG(VERBOSE, "SubSysCreateAdapter\n");

	memset(&ao, 0, sizeof(ao));

	/* this HPI only creates adapters for TI/PCI2040 based devices */
	if (phm->u.s.Resource.wBusType != HPI_BUS_PCI)
		return;
	if (phm->u.s.Resource.r.Pci->wVendorId != HPI_PCI_VENDOR_ID_TI)
		return;
	if (phm->u.s.Resource.r.Pci->wDeviceId != HPI_ADAPTER_PCI2040)
		return;

	ao.priv = kmalloc(sizeof(struct hpi_hw_obj), GFP_KERNEL);
	if (!ao.priv) {
		HPI_DEBUG_LOG(ERROR, "cant get mem for adapter object\n");
		phr->wError = HPI_ERROR_MEMORY_ALLOC;
		return;
	}

	memset(ao.priv, 0, sizeof(struct hpi_hw_obj));
	/* create the adapter object based on the resource information */
	/*? memcpy(&ao.Pci,&phm->u.s.Resource.r.Pci,sizeof(ao.Pci)); */
	ao.Pci = *phm->u.s.Resource.r.Pci;

	nError = CreateAdapterObj(&ao, &dwOsErrorCode);
	if (!nError)
		nError = HpiAddAdapter(&ao);
	if (nError) {
		phr->u.s.dwData = dwOsErrorCode;
		kfree(ao.priv);
		phr->wError = nError;
		return;
	}
	/* need to update paParentAdapter */
	pao = HpiFindAdapter(ao.wIndex);
	if (!pao) {
		/* We just added this adapter, why can't we find it!? */
		HPI_DEBUG_LOG(ERROR, "lost adapter after boot\n");
		phr->wError = 950;
		return;
	}

	for (dwDspIndex = 0; dwDspIndex < MAX_DSPS; dwDspIndex++) {
		struct hpi_hw_obj *phw = (struct hpi_hw_obj *)pao->priv;
		phw->ado[dwDspIndex].paParentAdapter = pao;
	}

	phr->u.s.awAdapterList[ao.wIndex] = ao.wAdapterType;
	phr->u.s.wAdapterIndex = ao.wIndex;
	phr->u.s.wNumAdapters++;
	phr->wError = 0;
}

static void SubSysDeleteAdapter(
	struct hpi_message *phm,
	struct hpi_response *phr
)
{
	struct hpi_adapter_obj *pao = NULL;
	void *priv;

	pao = HpiFindAdapter(phm->wAdapterIndex);
	if (!pao)
		return;

	priv = pao->priv;
	HpiDeleteAdapter(pao);
	kfree(priv);

	phr->wError = 0;
}

/* this routine is called from SubSysFindAdapter and SubSysCreateAdapter */
static short CreateAdapterObj(
	struct hpi_adapter_obj *pao,
	u32 *pdwOsErrorCode
)
{
	short nBootError = 0;
	u32 dwDspIndex = 0;
	struct hpi_hw_obj *phw = (struct hpi_hw_obj *)pao->priv;

	/* init error reporting */
	phw->wNumErrors = 0;
	pao->wDspCrashed = 0;

	/* The PCI2040 has the following address map */
	/* BAR0 - 4K = HPI control and status registers on PCI2040 (HPI CSR) */
	/* BAR1 - 32K = HPI registers on DSP */
	phw->dw2040_HPICSR = pao->Pci.apMemBase[0];
	phw->dw2040_HPIDSP = pao->Pci.apMemBase[1];
	HPI_DEBUG_LOG(VERBOSE, "csr %p, dsp %p\n",
		phw->dw2040_HPICSR, phw->dw2040_HPIDSP);

	/* set addresses for the possible DSP HPI interfaces */
	for (dwDspIndex = 0; dwDspIndex < MAX_DSPS; dwDspIndex++) {
		phw->ado[dwDspIndex].prHPIControl =
			phw->dw2040_HPIDSP + (CONTROL +
			DSP_SPACING * dwDspIndex);

		phw->ado[dwDspIndex].prHPIAddress =
			phw->dw2040_HPIDSP + (ADDRESS +
			DSP_SPACING * dwDspIndex);
		phw->ado[dwDspIndex].prHPIData =
			phw->dw2040_HPIDSP + (DATA +
			DSP_SPACING * dwDspIndex);

		phw->ado[dwDspIndex].prHPIDataAutoInc =
			phw->dw2040_HPIDSP +
			(DATA_AUTOINC + DSP_SPACING * dwDspIndex);

		HPI_DEBUG_LOG(VERBOSE, "ctl %p, adr %p, dat %p, dat++ %p\n",
			phw->ado[dwDspIndex].prHPIControl,
			phw->ado[dwDspIndex].prHPIAddress,
			phw->ado[dwDspIndex].prHPIData,
			phw->ado[dwDspIndex].prHPIDataAutoInc);

		phw->ado[dwDspIndex].paParentAdapter = pao;
	}

	phw->dwPCI2040HPIErrorCount = 0;
	pao->wHasControlCache = 0;

	/* Set the default number of DSPs on this card */
	/* This is (conditionally) adjusted after bootloading */
	/* of the first DSP in the bootload section. */
	phw->wNumDsp = 1;

	nBootError = Hpi6000_AdapterBootLoadDsp(pao, pdwOsErrorCode);
	if (nBootError)
		return (nBootError);

	HPI_DEBUG_LOG(INFO, "Bootload DSP OK\n");

	phw->dwMessageBufferAddressOnDSP = 0L;
	phw->dwResponseBufferAddressOnDSP = 0L;

	/* get info about the adapter by asking the adapter */
	/* send a HPI_ADAPTER_GET_INFO message */
	{
		struct hpi_message hM;
		struct hpi_response hR0;	/* response from DSP 0 */
		struct hpi_response hR1;	/* response from DSP 1 */
		u16 wError = 0;

		HPI_DEBUG_LOG(VERBOSE, "send ADAPTER_GET_INFO\n");
		memset(&hM, 0, sizeof(hM));
		hM.wType = HPI_TYPE_MESSAGE;
		hM.wSize = sizeof(struct hpi_message);
		hM.wObject = HPI_OBJ_ADAPTER;
		hM.wFunction = HPI_ADAPTER_GET_INFO;
		hM.wAdapterIndex = 0;
		hM.wDspIndex = 0;
		memset(&hR0, 0, sizeof(hR0));
		memset(&hR1, 0, sizeof(hR1));
		hR0.wSize = sizeof(hR0);
		hR1.wSize = sizeof(hR1);

		wError = Hpi6000_MessageResponseSequence(pao, &hM, &hR0);
		if (hR0.wError) {
			HPI_DEBUG_LOG(DEBUG, "message error %d\n",
				hR0.wError);
			return (hR0.wError);	/*error */
		}
		if (phw->wNumDsp == 2) {
			hM.wDspIndex = 1;
			wError = Hpi6000_MessageResponseSequence(pao, &hM,
				&hR1);
			if (wError)
				return wError;
		}
		pao->wAdapterType = hR0.u.a.wAdapterType;
		pao->wIndex = hR0.u.a.wAdapterIndex;
	}

	memset(&phw->aControlCache[0], 0,
		sizeof(struct hpi_control_cache_single) *
		HPI_NMIXER_CONTROLS);
	/* Read the control cache length to figure out if it is turned on */
	if (HpiReadWord
		(&phw->ado[0], HPI_HIF_ADDR(dwControlCacheSizeInBytes)))
		pao->wHasControlCache = 1;
	else
		pao->wHasControlCache = 0;

	HPI_DEBUG_LOG(DEBUG, "Get adapter info ASI%04X index %d\n",
		pao->wAdapterType, pao->wIndex);
	pao->wOpen = 0;		/* upon creation the adapter is closed */
	return 0;
}

/************************************************************************/
/* ADAPTER */

static void AdapterGetAsserts(
	struct hpi_adapter_obj *pao,
	struct hpi_message *phm,
	struct hpi_response *phr
)
{
#ifndef HIDE_PCI_ASSERTS
	/* if we have PCI2040 asserts then collect them */
	if ((gwPciReadAsserts > 0) || (gwPciWriteAsserts > 0)) {
		phr->u.a.dwSerialNumber =
			gwPciReadAsserts * 100 + gwPciWriteAsserts;
		phr->u.a.wAdapterIndex = 1;	/* assert count */
		phr->u.a.wAdapterType = -1;	/* "dsp index" */
		strcpy(phr->u.a.szAdapterAssert, "PCI2040 error");
		gwPciReadAsserts = 0;
		gwPciWriteAsserts = 0;
		phr->wError = 0;
	} else
#endif
		HW_Message(pao, phm, phr);	/*get DSP asserts */

	return;
}

/************************************************************************/
/* LOW-LEVEL */

static short Hpi6000_AdapterBootLoadDsp(
	struct hpi_adapter_obj *pao,
	u32 *pdwOsErrorCode
)
{
	struct hpi_hw_obj *phw = (struct hpi_hw_obj *)pao->priv;
	short nError;
	u32 dwTimeout;
	u32 dwRead = 0;
	u32 i = 0;
	u32 dwData = 0;
	u32 j = 0;
	u32 dwTestAddr = 0x80000000;
	u32 dwTestData = 0x00000001;
	u32 dw2040Reset = 0;
	u32 dwDspIndex = 0;
	u32 dwEndian = 0;
	u32 dwAdapterInfo = 0;
	u32 dwDelay = 0;

	struct dsp_code DspCode;
	u16 nBootLoadFamily = 0;

	/* NOTE don't use wAdapterType in this routine. It is not setup yet */

	switch (pao->Pci.wSubSysDeviceId) {
	case 0x5100:
	case 0x5110:		/* ASI5100 revB or higher with C6711D */
	case 0x6100:
	case 0x6200:
		nBootLoadFamily = HPI_ADAPTER_FAMILY_ASI6200;
		break;
	case 0x8800:
		nBootLoadFamily = HPI_ADAPTER_FAMILY_ASI8800;
		break;
	default:
		return (HPI6000_ERROR_UNHANDLED_SUBSYS_ID);
	}

	/* reset all DSPs, indicate two DSPs are present
	 * set RST3-=1 to disconnect HAD8 to set DSP in little endian mode
	 */
	dwEndian = 0;
	dw2040Reset = 0x0003000F;
	iowrite32(dw2040Reset, phw->dw2040_HPICSR + HPI_RESET);

	/* read back register to make sure PCI2040 chip is functioning
	 * note that bits 4..15 are read-only and so should always return zero,
	 * even though we wrote 1 to them
	 */
	for (i = 0; i < 1000; i++)
		dwDelay = ioread32(phw->dw2040_HPICSR + HPI_RESET);
	if (dwDelay != dw2040Reset) {
		HPI_DEBUG_LOG(ERROR, "INIT_PCI2040 %x %x\n",
			dw2040Reset, dwDelay);
		return (HPI6000_ERROR_INIT_PCI2040);
	}

	/* Indicate that DSP#0,1 is a C6X */
	iowrite32(0x00000003, phw->dw2040_HPICSR + HPI_DATA_WIDTH);
	/* set Bit30 and 29 - which will prevent Target aborts from being
	 * issued upon HPI or GP error
	 */
	iowrite32(0x60000000, phw->dw2040_HPICSR + INTERRUPT_MASK_SET);

	/* isolate DSP HAD8 line from PCI2040 so that
	 * Little endian can be set by pullup
	 */
	dw2040Reset = dw2040Reset & (~(dwEndian << 3));
	iowrite32(dw2040Reset, phw->dw2040_HPICSR + HPI_RESET);

	phw->ado[0].cDspRev = 'B';	/* revB */
	phw->ado[1].cDspRev = 'B';	/* revB */

	/*Take both DSPs out of reset, setting HAD8 to the correct Endian */
	dw2040Reset = dw2040Reset & (~0x00000001);	/* start DSP 0 */
	iowrite32(dw2040Reset, phw->dw2040_HPICSR + HPI_RESET);
	dw2040Reset = dw2040Reset & (~0x00000002);	/* start DSP 1 */
	iowrite32(dw2040Reset, phw->dw2040_HPICSR + HPI_RESET);

	/* set HAD8 back to PCI2040, now that DSP set to little endian mode */
	dw2040Reset = dw2040Reset & (~0x00000008);
	iowrite32(dw2040Reset, phw->dw2040_HPICSR + HPI_RESET);
	/*delay to allow DSP to get going */
	for (i = 0; i < 100; i++)
		dwDelay = ioread32(phw->dw2040_HPICSR + HPI_RESET);

	/* loop through all DSPs, downloading DSP code */
	for (dwDspIndex = 0; dwDspIndex < phw->wNumDsp; dwDspIndex++) {
		struct dsp_obj *pdo = &phw->ado[dwDspIndex];

		/* configure DSP so that we download code into the SRAM */
		/* set control reg for little endian, HWOB=1 */
		iowrite32(0x00010001, pdo->prHPIControl);

		/* test access to the HPI address register (HPIA) */
		dwTestData = 0x00000001;
		for (j = 0; j < 32; j++) {
			iowrite32(dwTestData, pdo->prHPIAddress);
			dwData = ioread32(pdo->prHPIAddress);
			if (dwData != dwTestData) {
				HPI_DEBUG_LOG(ERROR,
					"INIT_DSPHPI %x %x %x\n",
					dwTestData, dwData, dwDspIndex);
				return (HPI6000_ERROR_INIT_DSPHPI);
			}
			dwTestData = dwTestData << 1;
		}

/* if C6713 the setup PLL to generate 225MHz from 25MHz.
* Since the PLLDIV1 read is sometimes wrong, even on a C6713,
* we're going to do this unconditionally
*/
/* PLLDIV1 should have a value of 8000 after reset */
/*
	if (HpiReadWord(pdo,0x01B7C118) == 0x8000)
*/
		{
			/* C6713 datasheet says we cannot program PLL from HPI,
			 * and indeed if we try to set the PLL multiply from the
			 * HPI, the PLL does not seem to lock,
			 * so we enable the PLL and use the default of x 7
			 */
			HpiWriteWord(pdo, 0x01B7C100, 0x0000);	/* bypass PLL */
			for (i = 0; i < 100; i++)
				dwDelay =
					ioread32(phw->dw2040_HPICSR +
					HPI_RESET);

			/*  ** use default of PLL  x7 ** */
			/* EMIF = 225/3=75MHz */
			HpiWriteWord(pdo, 0x01B7C120, 0x8002);
			/* peri = 225/2 */
			HpiWriteWord(pdo, 0x01B7C11C, 0x8001);
			/* cpu	= 225/1 */
			HpiWriteWord(pdo, 0x01B7C118, 0x8000);
			/* ~200us delay */
			for (i = 0; i < 2000; i++)
				dwDelay =
					ioread32(phw->dw2040_HPICSR +
					HPI_RESET);
			/* PLL not bypassed */
			HpiWriteWord(pdo, 0x01B7C100, 0x0001);
			/* ~200us delay */
			for (i = 0; i < 2000; i++)
				dwDelay =
					ioread32(phw->dw2040_HPICSR +
					HPI_RESET);
		}

		/* test r/w to internal DSP memory
		 * C6711 has L2 cache mapped to 0x0 when reset
		 *
		 *  revB - because of bug 3.0.1 last HPI read
		 * (before HPI address issued) must be non-autoinc
		 */
		/* test each bit in the 32bit word */
		for (i = 0; i < 100; i++) {
			dwTestAddr = 0x00000000;
			dwTestData = 0x00000001;
			for (j = 0; j < 32; j++) {
				HpiWriteWord(pdo, dwTestAddr + i, dwTestData);
				dwData = HpiReadWord(pdo, dwTestAddr + i);
				if (dwData != dwTestData) {
					HPI_DEBUG_LOG(ERROR,
						"DSP mem %x %x %x %x\n",
						dwTestAddr + i,
						dwTestData, dwData,
						dwDspIndex);

					return (HPI6000_ERROR_INIT_DSPINTMEM);
				}
				dwTestData = dwTestData << 1;
			}
		}

		/* memory map of ASI6200
		   00000000-0000FFFF	16Kx32 internal program
		   01800000-019FFFFF	Internal peripheral
		   80000000-807FFFFF	CE0 2Mx32 SDRAM running @ 100MHz
		   90000000-9000FFFF	CE1 Async peripherals:

		   EMIF config
		   ------------
		   Global EMIF control
		   0 -
		   1 -
		   2 -
		   3 CLK2EN=1	CLKOUT2 enabled
		   4 CLK1EN=0	CLKOUT1 disabled
		   5 EKEN=1 <----------------!! C6713 specific, enables ECLKOUT
		   6 -
		   7 NOHOLD=1	external HOLD disabled
		   8 HOLDA=0	HOLDA output is low
		   9 HOLD=0		HOLD input is low
		   10 ARDY=1	ARDY input is high
		   11 BUSREQ=0	 BUSREQ output is low
		   12,13 Reserved = 1
		 */
		HpiWriteWord(pdo, 0x01800000, 0x34A8);

		/* EMIF CE0 setup - 2Mx32 Sync DRAM
		   31..28	Wr setup
		   27..22	Wr strobe
		   21..20	Wr hold
		   19..16	Rd setup
		   15..14	-
		   13..8	Rd strobe
		   7..4		MTYPE	0011		Sync DRAM 32bits
		   3		Wr hold MSB
		   2..0		Rd hold
		 */
		HpiWriteWord(pdo, 0x01800008, 0x00000030);

		/* EMIF SDRAM Extension
		   31-21	0
		   20		WR2RD = 0
		   19-18	WR2DEAC=1
		   17		WR2WR=0
		   16-15	R2WDQM=2
		   14-12	RD2WR=4
		   11-10	RD2DEAC=1
		   9		RD2RD= 1
		   8-7		THZP = 10b
		   6-5		TWR  = 2-1 = 01b (tWR = 10ns)
		   4		TRRD = 0b = 2 ECLK (tRRD = 14ns)
		   3-1		TRAS = 5-1 = 100b (Tras=42ns = 5 ECLK)
		   1		CAS latency = 3 ECLK
		   (for Micron 2M32-7 operating at 100Mhz)
		 */

		/* need to use this else DSP code crashes */
		HpiWriteWord(pdo, 0x01800020, 0x001BDF29);

		/* EMIF SDRAM control - set up for a 2Mx32 SDRAM (512x32x4 bank)
		   31		-		-
		   30		SDBSZ	1		4 bank
		   29..28	SDRSZ	00		11 row address pins
		   27..26	SDCSZ	01		8 column address pins
		   25		RFEN	1		refersh enabled
		   24		INIT	1		init SDRAM
		   23..20	TRCD	0001
		   19..16	TRP		0001
		   15..12	TRC		0110
		   11..0	-		-
		 */
		/*	need to use this else DSP code crashes */
		HpiWriteWord(pdo, 0x01800018, 0x47117000);

		/* EMIF SDRAM Refresh Timing */
		HpiWriteWord(pdo, 0x0180001C, 0x00000410);

		/*MIF CE1 setup - Async peripherals
		   @100MHz bus speed, each cycle is 10ns,
		   31..28	Wr setup  = 1
		   27..22	Wr strobe = 3			30ns
		   21..20	Wr hold = 1
		   19..16	Rd setup =1
		   15..14	Ta = 2
		   13..8	Rd strobe = 3			30ns
		   7..4		MTYPE	0010		Async 32bits
		   3		Wr hold MSB =0
		   2..0		Rd hold = 1
		 */
		{
			u32 dwCE1 = (1L << 28) | (3L << 22) | (1L << 20) |
				(1L << 16) | (2L << 14) | (3L << 8) |
				(2L << 4) | 1L;
			HpiWriteWord(pdo, 0x01800004, dwCE1);
		}

		/* delay a little to allow SDRAM and DSP to "get going" */

		for (i = 0; i < 1000; i++)
			dwDelay = ioread32(phw->dw2040_HPICSR + HPI_RESET);

		/* test access to SDRAM */
		{
			dwTestAddr = 0x80000000;
			dwTestData = 0x00000001;
			/* test each bit in the 32bit word */
			for (j = 0; j < 32; j++) {
				HpiWriteWord(pdo, dwTestAddr, dwTestData);
				dwData = HpiReadWord(pdo, dwTestAddr);
				if (dwData != dwTestData) {
					HPI_DEBUG_LOG(ERROR,
						"DSP dram %x %x %x %x\n",
						dwTestAddr,
						dwTestData, dwData,
						dwDspIndex);

					return (HPI6000_ERROR_INIT_SDRAM1);
				}
				dwTestData = dwTestData << 1;
			}
			/* test every Nth address in the DRAM */
#define DRAM_SIZE_WORDS 0x200000	/*2Mx32 */
#define DRAM_INC 1024
			dwTestAddr = 0x80000000;
			dwTestData = 0x0;
			for (i = 0; i < DRAM_SIZE_WORDS; i = i + DRAM_INC) {
				HpiWriteWord(pdo, dwTestAddr + i, dwTestData);
				dwTestData++;
			}
			dwTestAddr = 0x80000000;
			dwTestData = 0x0;
			for (i = 0; i < DRAM_SIZE_WORDS; i = i + DRAM_INC) {
				dwData = HpiReadWord(pdo, dwTestAddr + i);
				if (dwData != dwTestData) {
					HPI_DEBUG_LOG(ERROR,
						"DSP dram %x %x %x %x\n",
						dwTestAddr + i,
						dwTestData, dwData,
						dwDspIndex);
					return (HPI6000_ERROR_INIT_SDRAM2);
				}
				dwTestData++;
			}

		}

		/* write the DSP code down into the DSPs memory */
		/*HpiDspCode_Open(nBootLoadFamily,&DspCode,pdwOsErrorCode); */
		DspCode.psDev = pao->Pci.pOsData;

		nError = HpiDspCode_Open(nBootLoadFamily, &DspCode,
			pdwOsErrorCode);

		if (nError)
			return (nError);

		while (1) {
			u32 dwLength;
			u32 dwAddress;
			u32 dwType;
			u32 *pdwCode;

			nError = HpiDspCode_ReadWord(&DspCode, &dwLength);
			if (nError)
				break;
			if (dwLength == 0xFFFFFFFF)
				break;	/* end of code */

			nError = HpiDspCode_ReadWord(&DspCode, &dwAddress);
			if (nError)
				break;
			nError = HpiDspCode_ReadWord(&DspCode, &dwType);
			if (nError)
				break;
			nError = HpiDspCode_ReadBlock(dwLength, &DspCode,
				&pdwCode);
			if (nError)
				break;
			nError = Hpi6000_DspBlockWrite32(pao,
				(u16)dwDspIndex, dwAddress, pdwCode,
				dwLength);
			if (nError)
				break;
		}

		if (nError) {
			HpiDspCode_Close(&DspCode);
			return (nError);
		}
		/* verify that code was written correctly */
		/* this time through, assume no errors in DSP code file/array */
		HpiDspCode_Rewind(&DspCode);
		while (1) {
			u32 dwLength;
			u32 dwAddress;
			u32 dwType;
			u32 *pdwCode;

			HpiDspCode_ReadWord(&DspCode, &dwLength);
			if (dwLength == 0xFFFFFFFF)
				break;	/* end of code */

			HpiDspCode_ReadWord(&DspCode, &dwAddress);
			HpiDspCode_ReadWord(&DspCode, &dwType);
			HpiDspCode_ReadBlock(dwLength, &DspCode, &pdwCode);

			for (i = 0; i < dwLength; i++) {
				dwData = HpiReadWord(pdo, dwAddress);
				if (dwData != *pdwCode) {
					nError = HPI6000_ERROR_INIT_VERIFY;
					HPI_DEBUG_LOG(ERROR,
						"DSP verify %x %x %x %x\n",
						dwAddress, *pdwCode,
						dwData, dwDspIndex);
					break;
				}
				pdwCode++;
				dwAddress += 4;
			}
			if (nError)
				break;
		}
		HpiDspCode_Close(&DspCode);
		if (nError)
			return (nError);

		/* zero out the hostmailbox */
		{
			u32 dwAddress = HPI_HIF_ADDR(dwHostCmd);
			for (i = 0; i < 4; i++) {
				HpiWriteWord(pdo, dwAddress, 0);
				dwAddress += 4;
			}
		}
		/* write the DSP number into the hostmailbox */
		/* structure before starting the DSP */
		HpiWriteWord(pdo, HPI_HIF_ADDR(dwDspNumber), dwDspIndex);

		/* write the DSP adapter Info into the */
		/* hostmailbox before starting the DSP */
		if (dwDspIndex > 0)
			HpiWriteWord(pdo, HPI_HIF_ADDR(dwAdapterInfo),
				dwAdapterInfo);

		/* step 3. Start code by sending interrupt */
		iowrite32(0x00030003, pdo->prHPIControl);
		for (i = 0; i < 10000; i++)
			dwDelay = ioread32(phw->dw2040_HPICSR + HPI_RESET);

		/* wait for a non-zero value in hostcmd -
		 * indicating initialization is complete
		 *
		 * Init could take a while if DSP checks SDRAM memory
		 * Was 200000. Increased to 2000000 for ASI8801 so we
		 * don't get 938 errors.
		 */
		dwTimeout = 2000000;
		while (dwTimeout) {
			do {
				dwRead = HpiReadWord(pdo,
					HPI_HIF_ADDR(dwHostCmd));
			} while (--dwTimeout
				&& Hpi6000_Check_PCI2040_ErrorFlag(pao,
					H6READ));

			if (dwRead)
				break;
			/* The following is a workaround for bug #94:
			 * Bluescreen on install and subsequent boots on a
			 * DELL PowerEdge 600SC PC with 1.8GHz P4 and
			 * ServerWorks chipset. Without this delay the system
			 * locks up with a bluescreen (NOT GPF or pagefault).
			 */
			else
				HpiOs_DelayMicroSeconds(1000);
		}
		if (dwTimeout == 0)
			return (HPI6000_ERROR_INIT_NOACK);

		/* read the DSP adapter Info from the */
		/* hostmailbox structure after starting the DSP */
		if (dwDspIndex == 0) {
			/*u32 dwTestData=0; */
			u32 dwMask = 0;

			dwAdapterInfo =
				HpiReadWord(pdo, HPI_HIF_ADDR(dwAdapterInfo));
			if ((HPI_HIF_ADAPTER_INFO_EXTRACT_ADAPTER
					(dwAdapterInfo)
					& HPI_ADAPTER_FAMILY_MASK) ==
				HPI_ADAPTER_ASI6200)
				/* all 6200 cards have this many DSPs */
				phw->wNumDsp = 2;

			/* test that the PLD is programmed */
			/* and we can read/write 24bits */
#define PLD_BASE_ADDRESS 0x90000000L	/*for ASI6100/6200/8800 */

			switch (nBootLoadFamily) {
			case HPI_ADAPTER_FAMILY_ASI6200:
				/* ASI6100/6200 has 24bit path to FPGA */
				dwMask = 0xFFFFFF00L;
				/* ASI5100 uses AX6 code, */
				/* but has no PLD r/w register to test */
				if ((pao->Pci.wSubSysDeviceId & 0xFF00) ==
					0x5100)
					dwMask = 0x00000000L;
				break;
			case HPI_ADAPTER_FAMILY_ASI8800:
				/* ASI8800 has 16bit path to FPGA */
				dwMask = 0xFFFF0000L;
				break;
			}
			dwTestData = 0xAAAAAA00L & dwMask;
			/* write to 24 bit Debug register (D31-D8) */
			HpiWriteWord(pdo, PLD_BASE_ADDRESS + 4L, dwTestData);
			dwRead = HpiReadWord(pdo,
				PLD_BASE_ADDRESS + 4L) & dwMask;
			if (dwRead != dwTestData) {
				HPI_DEBUG_LOG(ERROR,
					"PLD %x %x\n", dwTestData, dwRead);
				return (HPI6000_ERROR_INIT_PLDTEST1);
			}
			dwTestData = 0x55555500L & dwMask;
			HpiWriteWord(pdo, PLD_BASE_ADDRESS + 4L, dwTestData);
			dwRead = HpiReadWord(pdo,
				PLD_BASE_ADDRESS + 4L) & dwMask;
			if (dwRead != dwTestData) {
				HPI_DEBUG_LOG(ERROR,
					"PLD %x %x\n", dwTestData, dwRead);
				return (HPI6000_ERROR_INIT_PLDTEST2);
			}
		}
	}			/* for wNumDSP */
	return 0;
}

#define PCI_TIMEOUT 100

static int HpiSetAddress(
	struct dsp_obj *pdo,
	u32 dwAddress
)
{
	u32 dwTimeout = PCI_TIMEOUT;

	do {
		iowrite32(dwAddress, pdo->prHPIAddress);
	}
	while (Hpi6000_Check_PCI2040_ErrorFlag(pdo->paParentAdapter, H6WRITE)
		&& --dwTimeout);

	if (dwTimeout)
		return 0;

	return 1;
}

/* write one word to the HPI port */
static void HpiWriteWord(
	struct dsp_obj *pdo,
	u32 dwAddress,
	u32 dwData
)
{
	if (HpiSetAddress(pdo, dwAddress))
		return;
	iowrite32(dwData, pdo->prHPIData);
}

/* read one word from the HPI port */
static u32 HpiReadWord(
	struct dsp_obj *pdo,
	u32 dwAddress
)
{
	u32 dwData = 0;

	if (HpiSetAddress(pdo, dwAddress))
		return 0;	/*? No way to return error */

	/* take care of errata in revB DSP (2.0.1) */
	dwData = ioread32(pdo->prHPIData);
	return (dwData);
}

/* write a block of 32bit words to the DSP HPI port using auto-inc mode */
static void HpiWriteBlock(
	struct dsp_obj *pdo,
	u32 dwAddress,
	u32 *pdwData,
	u32 dwLength
)
{
	if (dwLength == 0)
		return;

	if (HpiSetAddress(pdo, dwAddress))
		return;

	{
		u16 wLength = dwLength - 1;
		iowrite32_rep(pdo->prHPIDataAutoInc, pdwData, wLength);

		/* take care of errata in revB DSP (2.0.1) */
		/* must end with non auto-inc */
		iowrite32(*(pdwData + dwLength - 1), pdo->prHPIData);
	}
}

/** read a block of 32bit words from the DSP HPI port using auto-inc mode
 */
static void HpiReadBlock(
	struct dsp_obj *pdo,
	u32 dwAddress,
	u32 *pdwData,
	u32 dwLength
)
{
	if (dwLength == 0)
		return;

	if (HpiSetAddress(pdo, dwAddress))
		return;

	{
		u16 wLength = dwLength - 1;
		ioread32_rep(pdo->prHPIDataAutoInc, pdwData, wLength);

		/* take care of errata in revB DSP (2.0.1) */
		/* must end with non auto-inc */
		*(pdwData + dwLength - 1) = ioread32(pdo->prHPIData);
	}
}

static u16 Hpi6000_DspBlockWrite32(
	struct hpi_adapter_obj *pao,
	u16 wDspIndex,
	u32 dwHpiAddress,
	u32 *dwSource,
	u32 dwCount
)
{
	struct dsp_obj *pdo =
		&(*(struct hpi_hw_obj *)pao->priv).ado[wDspIndex];
	u32 dwTimeOut = PCI_TIMEOUT;
	int nC6711BurstSize = 128;
	u32 dwLocalHpiAddress = dwHpiAddress;
	int wLocalCount = dwCount;
	int wXferSize;
	u32 *pdwData = dwSource;

	while (wLocalCount) {
		if (wLocalCount > nC6711BurstSize)
			wXferSize = nC6711BurstSize;
		else
			wXferSize = wLocalCount;

		dwTimeOut = PCI_TIMEOUT;
		do {
			HpiWriteBlock(pdo, dwLocalHpiAddress, pdwData,
				wXferSize);
		} while (Hpi6000_Check_PCI2040_ErrorFlag(pao, H6WRITE)
			&& --dwTimeOut);

		if (!dwTimeOut)
			break;
		pdwData += wXferSize;
		dwLocalHpiAddress += sizeof(u32) * wXferSize;
		wLocalCount -= wXferSize;
	}

	if (dwTimeOut)
		return 0;
	else
		return 1;
}

static u16 Hpi6000_DspBlockRead32(
	struct hpi_adapter_obj *pao,
	u16 wDspIndex,
	u32 dwHpiAddress,
	u32 *dwDest,
	u32 dwCount
)
{
	struct dsp_obj *pdo =
		&(*(struct hpi_hw_obj *)pao->priv).ado[wDspIndex];
	u32 dwTimeOut = PCI_TIMEOUT;
	int nC6711BurstSize = 16;
	u32 dwLocalHpiAddress = dwHpiAddress;
	int wLocalCount = dwCount;
	int wXferSize;
	u32 *pdwData = dwDest;
	u32 dwLoopCount = 0;

	while (wLocalCount) {
		if (wLocalCount > nC6711BurstSize)
			wXferSize = nC6711BurstSize;
		else
			wXferSize = wLocalCount;

		dwTimeOut = PCI_TIMEOUT;
		do {
			HpiReadBlock(pdo, dwLocalHpiAddress, pdwData,
				wXferSize);
		}
		while (Hpi6000_Check_PCI2040_ErrorFlag(pao, H6READ)
			&& --dwTimeOut);
		if (!dwTimeOut)
			break;

		pdwData += wXferSize;
		dwLocalHpiAddress += sizeof(u32) * wXferSize;
		wLocalCount -= wXferSize;
		dwLoopCount++;
	}

	if (dwTimeOut)
		return 0;
	else
		return 1;
}

static short Hpi6000_MessageResponseSequence(
	struct hpi_adapter_obj *pao,
	struct hpi_message *phm,
	struct hpi_response *phr
)
{
	u16 wDspIndex = phm->wDspIndex;
	struct hpi_hw_obj *phw = (struct hpi_hw_obj *)pao->priv;
	struct dsp_obj *pdo = &phw->ado[wDspIndex];
	u32 dwTimeout;
	u16 wAck;
	u32 dwAddress;
	u32 dwLength;
	u32 *pData;
	u16 wError = 0;

	/* does the DSP we are referencing exist? */
	if (wDspIndex >= phw->wNumDsp)
		return HPI6000_ERROR_MSG_INVALID_DSP_INDEX;

	wAck = Hpi6000_WaitDspAck(pao, wDspIndex, HPI_HIF_IDLE);
	if (wAck & HPI_HIF_ERROR_MASK) {
		phw->wNumErrors++;
		if (phw->wNumErrors == 10)
			pao->wDspCrashed = 1;
		return HPI6000_ERROR_MSG_RESP_IDLE_TIMEOUT;
	}
	phw->wNumErrors = 0;

	/* send the message */

	/* get the address and size */
	if (phw->dwMessageBufferAddressOnDSP == 0) {
		dwTimeout = TIMEOUT;
		do {
			dwAddress = HpiReadWord(pdo,
				HPI_HIF_ADDR(dwMessageBufferAddress));
			phw->dwMessageBufferAddressOnDSP = dwAddress;
		}
		while (Hpi6000_Check_PCI2040_ErrorFlag(pao, H6READ)
			&& --dwTimeout);
		if (!dwTimeout)
			return HPI6000_ERROR_MSG_GET_ADR;
	} else
		dwAddress = phw->dwMessageBufferAddressOnDSP;

	/*	  dwLength = sizeof(struct hpi_message); */
	dwLength = phm->wSize;

	/* send it */
	pData = (u32 *)phm;
	if (Hpi6000_DspBlockWrite32(pao, wDspIndex, dwAddress,
			pData, (u16)dwLength / 4))
		return HPI6000_ERROR_MSG_RESP_BLOCKWRITE32;

	if (Hpi6000_SendHostCommand(pao, wDspIndex, HPI_HIF_GET_RESP))
		return HPI6000_ERROR_MSG_RESP_GETRESPCMD;
	Hpi6000_SendDspInterrupt(pdo);

	wAck = Hpi6000_WaitDspAck(pao, wDspIndex, HPI_HIF_GET_RESP);
	if (wAck & HPI_HIF_ERROR_MASK)
		return HPI6000_ERROR_MSG_RESP_GET_RESP_ACK;

	/* get the address and size */
	if (phw->dwResponseBufferAddressOnDSP == 0) {
		dwTimeout = TIMEOUT;
		do {
			dwAddress = HpiReadWord(pdo,
				HPI_HIF_ADDR(dwResponseBufferAddress));
		} while (Hpi6000_Check_PCI2040_ErrorFlag(pao, H6READ)
			&& --dwTimeout);
		phw->dwResponseBufferAddressOnDSP = dwAddress;

		if (!dwTimeout)
			return HPI6000_ERROR_RESP_GET_ADR;
	} else
		dwAddress = phw->dwResponseBufferAddressOnDSP;

	/* read the length of the response back from the DSP */
	dwTimeout = TIMEOUT;
	do {
		dwLength = HpiReadWord(pdo, HPI_HIF_ADDR(dwLength));
	}
	while (Hpi6000_Check_PCI2040_ErrorFlag(pao, H6READ) && --dwTimeout);
	if (!dwTimeout)
		dwLength = sizeof(struct hpi_response);

	/* get it */
	pData = (u32 *)phr;
	if (Hpi6000_DspBlockRead32
		(pao, wDspIndex, dwAddress, pData, (u16)dwLength / 4))
		return HPI6000_ERROR_MSG_RESP_BLOCKREAD32;

	/* set i/f back to idle */
	if (Hpi6000_SendHostCommand(pao, wDspIndex, HPI_HIF_IDLE))
		return HPI6000_ERROR_MSG_RESP_IDLECMD;
	Hpi6000_SendDspInterrupt(pdo);

	wError = HpiValidateResponse(phm, phr);
	return wError;
}

/* have to set up the below defines to match stuff in the MAP file */

#define MSG_ADDRESS (HPI_HIF_BASE+0x18)
#define MSG_LENGTH 11
#define RESP_ADDRESS (HPI_HIF_BASE+0x44)
#define RESP_LENGTH 16
#define QUEUE_START  (HPI_HIF_BASE+0x88)
#define QUEUE_SIZE 0x8000

static short Hpi6000_SendData_CheckAdr(
	u32 dwAddress,
	u32 dwLengthInDwords
)
{
/*#define CHECKING	 // comment this line in to enable checking */
#ifdef CHECKING
	if (dwAddress < (u32)MSG_ADDRESS)
		return 0;
	if (dwAddress > (u32)(QUEUE_START + QUEUE_SIZE))
		return 0;
	if ((dwAddress + (dwLengthInDwords << 2)) >
		(u32)(QUEUE_START + QUEUE_SIZE))
		return 0;
#else
	(void)dwAddress;
	(void)dwLengthInDwords;
	return 1;
#endif
}

static short Hpi6000_SendData(
	struct hpi_adapter_obj *pao,
	struct hpi_message *phm,
	struct hpi_response *phr
)
{
	u16 wDspIndex = phm->wDspIndex;
	struct dsp_obj *pdo =
		&(*(struct hpi_hw_obj *)pao->priv).ado[wDspIndex];
	u32 dwDataSent = 0;
	u16 wAck;
	u32 dwLength, dwAddress;
	u32 *pData = (u32 *)phm->u.d.u.Data.pbData;
	u16 wTimeOut = 8;

	(void)phr;

	/* round dwDataSize down to nearest 4 bytes */
	while ((dwDataSent < (phm->u.d.u.Data.dwDataSize & ~3L))
		&& --wTimeOut) {
		wAck = Hpi6000_WaitDspAck(pao, wDspIndex, HPI_HIF_IDLE);
		if (wAck & HPI_HIF_ERROR_MASK)
			return HPI6000_ERROR_SEND_DATA_IDLE_TIMEOUT;

		if (Hpi6000_SendHostCommand
			(pao, wDspIndex, HPI_HIF_SEND_DATA))
			return HPI6000_ERROR_SEND_DATA_CMD;

		Hpi6000_SendDspInterrupt(pdo);

		wAck = Hpi6000_WaitDspAck(pao, wDspIndex, HPI_HIF_SEND_DATA);

		if (wAck & HPI_HIF_ERROR_MASK)
			return HPI6000_ERROR_SEND_DATA_ACK;

		do {
			/* get the address and size */
			dwAddress = HpiReadWord(pdo, HPI_HIF_ADDR(dwAddress));
			/* DSP returns number of DWORDS */
			dwLength = HpiReadWord(pdo, HPI_HIF_ADDR(dwLength));
		}
		while (Hpi6000_Check_PCI2040_ErrorFlag(pao, H6READ));

		if (!Hpi6000_SendData_CheckAdr(dwAddress, dwLength))
			return HPI6000_ERROR_SEND_DATA_ADR;

		/* send the data. break data into 512 DWORD blocks (2K bytes)
		 * and send using block write. 2Kbytes is the max as this is the
		 * memory window given to the HPI data register by the PCI2040
		 */

		{
			u32 dwLen = dwLength;
			u32 dwBlkLen = 512;
			while (dwLen) {
				if (dwLen < dwBlkLen)
					dwBlkLen = dwLen;
				if (Hpi6000_DspBlockWrite32(pao, wDspIndex,
						dwAddress, pData, dwBlkLen))
					return HPI6000_ERROR_SEND_DATA_WRITE;
				dwAddress += dwBlkLen * 4;
				pData += dwBlkLen;
				dwLen -= dwBlkLen;
			}
		}

		if (Hpi6000_SendHostCommand(pao, wDspIndex, HPI_HIF_IDLE))
			return HPI6000_ERROR_SEND_DATA_IDLECMD;

		Hpi6000_SendDspInterrupt(pdo);

		dwDataSent += dwLength * 4;
	}
	if (!wTimeOut)
		return HPI6000_ERROR_SEND_DATA_TIMEOUT;
	return 0;
}

static short Hpi6000_GetData(
	struct hpi_adapter_obj *pao,
	struct hpi_message *phm,
	struct hpi_response *phr
)
{
	u16 wDspIndex = phm->wDspIndex;
	struct dsp_obj *pdo =
		&(*(struct hpi_hw_obj *)pao->priv).ado[wDspIndex];
	u32 dwDataGot = 0;
	u16 wAck;
	u32 dwLength, dwAddress;
	u32 *pData = (u32 *)phm->u.d.u.Data.pbData;

	(void)phr;		/* this parameter not used! */

	/* round dwDataSize down to nearest 4 bytes */
	while (dwDataGot < (phm->u.d.u.Data.dwDataSize & ~3L)) {
		wAck = Hpi6000_WaitDspAck(pao, wDspIndex, HPI_HIF_IDLE);
		if (wAck & HPI_HIF_ERROR_MASK)
			return HPI6000_ERROR_GET_DATA_IDLE_TIMEOUT;

		if (Hpi6000_SendHostCommand(pao, wDspIndex, HPI_HIF_GET_DATA))
			return HPI6000_ERROR_GET_DATA_CMD;
		Hpi6000_SendDspInterrupt(pdo);

		wAck = Hpi6000_WaitDspAck(pao, wDspIndex, HPI_HIF_GET_DATA);

		if (wAck & HPI_HIF_ERROR_MASK)
			return HPI6000_ERROR_GET_DATA_ACK;

		/* get the address and size */
		do {
			dwAddress = HpiReadWord(pdo, HPI_HIF_ADDR(dwAddress));
			dwLength = HpiReadWord(pdo, HPI_HIF_ADDR(dwLength));
		}
		while (Hpi6000_Check_PCI2040_ErrorFlag(pao, H6READ));

		/* read the data */
		{
			u32 dwLen = dwLength;
			u32 dwBlkLen = 512;
			while (dwLen) {
				if (dwLen < dwBlkLen)
					dwBlkLen = dwLen;
				if (Hpi6000_DspBlockRead32(pao, wDspIndex,
						dwAddress, pData, dwBlkLen))
					return HPI6000_ERROR_GET_DATA_READ;
				dwAddress += dwBlkLen * 4;
				pData += dwBlkLen;
				dwLen -= dwBlkLen;
			}
		}

		if (Hpi6000_SendHostCommand(pao, wDspIndex, HPI_HIF_IDLE))
			return HPI6000_ERROR_GET_DATA_IDLECMD;
		Hpi6000_SendDspInterrupt(pdo);

		dwDataGot += dwLength * 4;
	}
	return 0;
}

static void Hpi6000_SendDspInterrupt(
	struct dsp_obj *pdo
)
{
	iowrite32(0x00030003, pdo->prHPIControl);	/* DSPINT */
}

static short Hpi6000_SendHostCommand(
	struct hpi_adapter_obj *pao,
	u16 wDspIndex,
	u32 dwHostCmd
)
{
	struct dsp_obj *pdo =
		&(*(struct hpi_hw_obj *)pao->priv).ado[wDspIndex];
	u32 dwTimeout = TIMEOUT;

	/* set command */
	do {
		HpiWriteWord(pdo, HPI_HIF_ADDR(dwHostCmd), dwHostCmd);
		/* flush the FIFO */
		HpiSetAddress(pdo, HPI_HIF_ADDR(dwHostCmd));
	}
	while (Hpi6000_Check_PCI2040_ErrorFlag(pao, H6WRITE) && --dwTimeout);

	/* reset the interrupt bit */
	iowrite32(0x00040004, pdo->prHPIControl);

	if (dwTimeout)
		return 0;
	else
		return 1;
}

/* if the PCI2040 has recorded an HPI timeout, reset the error and return 1 */
static short Hpi6000_Check_PCI2040_ErrorFlag(
	struct hpi_adapter_obj *pao,
	u16 nReadOrWrite
)
{
	u32 dwHPIError;

	struct hpi_hw_obj *phw = (struct hpi_hw_obj *)pao->priv;

	/* read the error bits from the PCI2040 */
	dwHPIError = ioread32(phw->dw2040_HPICSR + HPI_ERROR_REPORT);
	if (dwHPIError) {
		/* reset the error flag */
		iowrite32(0L, phw->dw2040_HPICSR + HPI_ERROR_REPORT);
		phw->dwPCI2040HPIErrorCount++;
		if (nReadOrWrite == 1)
			gwPciReadAsserts++;	/************* inc global */
		else
			gwPciWriteAsserts++;
		return 1;
	} else
		return 0;
}

static short Hpi6000_WaitDspAck(
	struct hpi_adapter_obj *pao,
	u16 wDspIndex,
	u32 dwAckValue
)
{
	struct dsp_obj *pdo =
		&(*(struct hpi_hw_obj *)pao->priv).ado[wDspIndex];
	u32 dwAck = 0L;
	u32 dwTimeout;
	u32 dwHPIC = 0L;

	/* wait for host interrupt to signal ack is ready */
	dwTimeout = TIMEOUT;
	while (--dwTimeout) {
		dwHPIC = ioread32(pdo->prHPIControl);
		if (dwHPIC & 0x04)	/* 0x04 = HINT from DSP */
			break;
	}
	if (dwTimeout == 0)
		return HPI_HIF_ERROR_MASK;

	/* wait for dwAckValue */
	dwTimeout = TIMEOUT;
	while (--dwTimeout) {
		/* read the ack mailbox */
		dwAck = HpiReadWord(pdo, HPI_HIF_ADDR(dwDspAck));
		if (dwAck == dwAckValue)
			break;
		if ((dwAck & HPI_HIF_ERROR_MASK) &
			!Hpi6000_Check_PCI2040_ErrorFlag(pao, H6READ))
			break;
		/*for (i=0;i<1000;i++) */
		/*	dwPause=i+1; */
	}
	if (dwAck & HPI_HIF_ERROR_MASK)
		/* indicates bad read from DSP -
		   typically 0xffffff is read for some reason */
		dwAck = HPI_HIF_ERROR_MASK;

	if (dwTimeout == 0)
		dwAck = HPI_HIF_ERROR_MASK;
	return (short)dwAck;
}

static short Hpi6000_UpdateControlCache(
	struct hpi_adapter_obj *pao,
	struct hpi_message *phm
)
{
	const u16 wDspIndex = phm->wDspIndex;
	struct hpi_hw_obj *phw = (struct hpi_hw_obj *)pao->priv;
	struct dsp_obj *pdo = &phw->ado[wDspIndex];
	u32 dwTimeout;
	u32 dwCacheDirtyFlag;
	u16 err;

	HpiOs_Dsplock_Lock(pao);

	dwTimeout = TIMEOUT;
	do {
		dwCacheDirtyFlag = HpiReadWord((struct dsp_obj *)pdo,
			HPI_HIF_ADDR(dwControlCacheIsDirty));
	}
	while (Hpi6000_Check_PCI2040_ErrorFlag(pao, H6READ) && --dwTimeout);
	if (!dwTimeout) {
		err = HPI6000_ERROR_CONTROL_CACHE_PARAMS;
		goto unlock;
	}

	if (dwCacheDirtyFlag) {
		/* read the cached controls */
		u32 dwAddress;
		u32 dwLength;

		dwTimeout = TIMEOUT;
		if (pdo->dwControlCacheAddressOnDSP == 0) {
			do {
				dwAddress =
					HpiReadWord((struct dsp_obj *)pdo,
					HPI_HIF_ADDR(dwControlCacheAddress));

				dwLength =
					HpiReadWord((struct dsp_obj *)pdo,
					HPI_HIF_ADDR
					(dwControlCacheSizeInBytes));
			}
			while (Hpi6000_Check_PCI2040_ErrorFlag(pao, H6READ)
				&& --dwTimeout);
			if (!dwTimeout) {
				err = HPI6000_ERROR_CONTROL_CACHE_ADDRLEN;
				goto unlock;
			}
			pdo->dwControlCacheAddressOnDSP = dwAddress;
			pdo->dwControlCacheLengthOnDSP = dwLength;
		} else {
			dwAddress = pdo->dwControlCacheAddressOnDSP;
			dwLength = pdo->dwControlCacheLengthOnDSP;
		}

		if (Hpi6000_DspBlockRead32(pao, wDspIndex, dwAddress,
				(u32 *)&phw->aControlCache[0],
				dwLength / sizeof(u32))) {
			err = HPI6000_ERROR_CONTROL_CACHE_READ;
			goto unlock;
		}
		do {
			HpiWriteWord((struct dsp_obj *)pdo,
				HPI_HIF_ADDR(dwControlCacheIsDirty), 0);
			/* flush the FIFO */
			HpiSetAddress(pdo, HPI_HIF_ADDR(dwHostCmd));
		}
		while (Hpi6000_Check_PCI2040_ErrorFlag(pao, H6WRITE)
			&& --dwTimeout);
		if (!dwTimeout) {
			err = HPI6000_ERROR_CONTROL_CACHE_FLUSH;
			goto unlock;
		}

	}
	err = 0;

unlock:
	HpiOs_Dsplock_UnLock(pao);
	return err;
}

static void HW_Message(
	struct hpi_adapter_obj *pao,
	struct hpi_message *phm,
	struct hpi_response *phr
)
{
	u16 nError = 0;

	HpiOs_Dsplock_Lock(pao);

	nError = Hpi6000_MessageResponseSequence(pao, phm, phr);

	/* maybe an error response */
	if (nError) {
		/* something failed in the HPI/DSP interface */
		phr->wError = nError;
		/* just the header of the response is valid */
		phr->wSize = sizeof(struct hpi_response_header);
		goto err;
	}

	if (phr->wError != 0)	/* something failed in the DSP */
		goto err;

	switch (phm->wFunction) {
	case HPI_OSTREAM_WRITE:
	case HPI_ISTREAM_ANC_WRITE:
		nError = Hpi6000_SendData(pao, phm, phr);
		break;
	case HPI_ISTREAM_READ:
	case HPI_OSTREAM_ANC_READ:
		nError = Hpi6000_GetData(pao, phm, phr);
		break;
	case HPI_ADAPTER_GET_ASSERT:
		phr->u.a.wAdapterIndex = 0;	/* dsp 0 default */
		if (((struct hpi_hw_obj *)pao->priv)->wNumDsp == 2) {
			if (!phr->u.a.wAdapterType) {
				/* no assert from dsp 0, check dsp 1 */
				phm->wDspIndex = 1;
				nError = Hpi6000_MessageResponseSequence(pao,
					phm, phr);
				phr->u.a.wAdapterIndex = 1;
			}
		}
	}

	if (nError)
		phr->wError = nError;

err:
	HpiOs_Dsplock_UnLock(pao);
	return;
}
