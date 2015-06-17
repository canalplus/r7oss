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

Extended Message Function With Response Cacheing

(C) Copyright AudioScience Inc. 2002
*****************************************************************************/
#define SOURCEFILE_NAME "hpimsgx.c"
#include "hpi.h"
#include "hpimsgx.h"
#include "hpidebug.h"

extern struct pci_device_id asihpi_pci_tbl[];	/* hpimod.c mod device tbl */

static struct hpios_spinlock msgxLock;

static HPI_HandlerFunc *hpi_entry_points[HPI_MAX_ADAPTERS];

static HPI_HandlerFunc *HPI_LookupEntryPointFunction(
	struct hpi_pci *PciInfo
)
{

	int i;

	for (i = 0; asihpi_pci_tbl[i].vendor != 0; i++) {
		if (asihpi_pci_tbl[i].vendor != PCI_ANY_ID
			&& asihpi_pci_tbl[i].vendor != PciInfo->wVendorId)
			continue;
		if (asihpi_pci_tbl[i].device != PCI_ANY_ID
			&& asihpi_pci_tbl[i].device != PciInfo->wDeviceId)
			continue;
		if (asihpi_pci_tbl[i].subvendor != PCI_ANY_ID
			&& asihpi_pci_tbl[i].subvendor !=
			PciInfo->wSubSysVendorId)
			continue;
		if (asihpi_pci_tbl[i].subdevice != PCI_ANY_ID
			&& asihpi_pci_tbl[i].subdevice !=
			PciInfo->wSubSysDeviceId)
			continue;

		HPI_DEBUG_LOG(DEBUG, " %x,%lu\n", i,
			asihpi_pci_tbl[i].driver_data);
		return (HPI_HandlerFunc *) asihpi_pci_tbl[i].driver_data;
	}

	return NULL;
}

static inline void HW_EntryPoint(
	struct hpi_message *phm,
	struct hpi_response *phr
)
{

	HPI_HandlerFunc *ep;

	if (phm->wAdapterIndex < HPI_MAX_ADAPTERS) {
		ep = (HPI_HandlerFunc *) hpi_entry_points[phm->wAdapterIndex];
		if (ep) {
			HPI_DEBUG_MESSAGE(phm);
			ep(phm, phr);
			HPI_DEBUG_RESPONSE(phr);
			return;
		}
	}
	HPI_InitResponse(phr, phm->wObject, phm->wFunction,
		HPI_ERROR_PROCESSING_MESSAGE);
}

static void AdapterOpen(
	struct hpi_message *phm,
	struct hpi_response *phr
);
static void AdapterClose(
	struct hpi_message *phm,
	struct hpi_response *phr
);

static void MixerOpen(
	struct hpi_message *phm,
	struct hpi_response *phr
);
static void MixerClose(
	struct hpi_message *phm,
	struct hpi_response *phr
);

static void OutStreamOpen(
	struct hpi_message *phm,
	struct hpi_response *phr,
	void *hOwner
);
static void OutStreamClose(
	struct hpi_message *phm,
	struct hpi_response *phr,
	void *hOwner
);
static void InStreamOpen(
	struct hpi_message *phm,
	struct hpi_response *phr,
	void *hOwner
);
static void InStreamClose(
	struct hpi_message *phm,
	struct hpi_response *phr,
	void *hOwner
);

static void HPIMSGX_Reset(
	u16 wAdapterIndex
);
static u16 HPIMSGX_Init(
	struct hpi_message *phm,
	struct hpi_response *phr
);
static void HPIMSGX_Cleanup(
	u16 wAdapterIndex,
	void *hOwner
);

#ifndef DISABLE_PRAGMA_PACK1
#pragma pack(push, 1)
#endif

struct hpi_subsys_response {
	struct hpi_response_header h;
	struct hpi_subsys_res s;
};

struct hpi_adapter_response {
	struct hpi_response_header h;
	struct hpi_adapter_res a;
};

struct hpi_mixer_response {
	struct hpi_response_header h;
	struct hpi_mixer_res m;
};

struct hpi_stream_response {
	struct hpi_response_header h;
	struct hpi_stream_res d;
};

struct adapter_info {
	u16 wType;
	u16 wNumInStreams;
	u16 wNumOutStreams;
};

struct asi_open_state {
	int nOpenFlag;
	void *hOwner;
	u16 wDspIndex;
};

#ifndef DISABLE_PRAGMA_PACK1
#pragma pack(pop)
#endif

/* Globals */
static struct hpi_adapter_response aRESP_HPI_ADAPTER_OPEN[HPI_MAX_ADAPTERS];

static struct hpi_stream_response
	aRESP_HPI_OSTREAM_OPEN[HPI_MAX_ADAPTERS][HPI_MAX_STREAMS];

static struct hpi_stream_response
	aRESP_HPI_ISTREAM_OPEN[HPI_MAX_ADAPTERS][HPI_MAX_STREAMS];

static struct hpi_mixer_response aRESP_HPI_MIXER_OPEN[HPI_MAX_ADAPTERS];

static struct hpi_subsys_response gRESP_HPI_SUBSYS_FIND_ADAPTERS;

static struct adapter_info aADAPTER_INFO[HPI_MAX_ADAPTERS];

/* use these to keep track of opens from user mode apps/DLLs */
static struct asi_open_state
	aOStreamUserOpen[HPI_MAX_ADAPTERS][HPI_MAX_STREAMS];

static struct asi_open_state
	aIStreamUserOpen[HPI_MAX_ADAPTERS][HPI_MAX_STREAMS];

static void SubSysMessage(
	struct hpi_message *phm,
	struct hpi_response *phr,
	void *hOwner
)
{
	switch (phm->wFunction) {
	case HPI_SUBSYS_GET_VERSION:
		HPI_InitResponse(phr, HPI_OBJ_SUBSYSTEM,
			HPI_SUBSYS_GET_VERSION, 0);
		phr->u.s.dwVersion = HPI_VER >> 8;	/* return major.minor */
		phr->u.s.dwData = HPI_VER;	/* return major.minor.release */
		break;
	case HPI_SUBSYS_OPEN:
		/*do not propagate the message down the chain */
		HPI_InitResponse(phr, HPI_OBJ_SUBSYSTEM, HPI_SUBSYS_OPEN, 0);
		break;
	case HPI_SUBSYS_CLOSE:
		/*do not propagate the message down the chain */
		HPI_InitResponse(phr, HPI_OBJ_SUBSYSTEM, HPI_SUBSYS_CLOSE, 0);
		HPIMSGX_Cleanup(HPIMSGX_ALLADAPTERS, hOwner);
		break;
	case HPI_SUBSYS_DRIVER_LOAD:
		/* Initialize this module's internal state */
		HpiOs_Msgxlock_Init(&msgxLock);
		memset(&hpi_entry_points, 0, sizeof(hpi_entry_points));
		HpiOs_LockedMem_Init();
		/* Init subsys_findadapters response to no-adapters */
		HPIMSGX_Reset(HPIMSGX_ALLADAPTERS);
		HPI_InitResponse(phr, HPI_OBJ_SUBSYSTEM,
			HPI_SUBSYS_DRIVER_LOAD, 0);
		/* individual HPIs dont implement driver load */
		HPI_COMMON(phm, phr);
		break;
	case HPI_SUBSYS_DRIVER_UNLOAD:
		HPI_COMMON(phm, phr);
		HPIMSGX_Cleanup(HPIMSGX_ALLADAPTERS, hOwner);
		HpiOs_LockedMem_FreeAll();
		HPI_InitResponse(phr, HPI_OBJ_SUBSYSTEM,
			HPI_SUBSYS_DRIVER_UNLOAD, 0);
		return;

	case HPI_SUBSYS_GET_INFO:
		HPI_COMMON(phm, phr);
		break;

	case HPI_SUBSYS_FIND_ADAPTERS:
		memcpy(phr, &gRESP_HPI_SUBSYS_FIND_ADAPTERS,
			sizeof(gRESP_HPI_SUBSYS_FIND_ADAPTERS));
		break;
	case HPI_SUBSYS_GET_NUM_ADAPTERS:
		memcpy(phr, &gRESP_HPI_SUBSYS_FIND_ADAPTERS,
			sizeof(gRESP_HPI_SUBSYS_FIND_ADAPTERS));
		phr->wFunction = HPI_SUBSYS_GET_NUM_ADAPTERS;
		break;
	case HPI_SUBSYS_GET_ADAPTER:
		{
			int nCount = phm->wAdapterIndex;
			int nIndex = 0;
			HPI_InitResponse(phr, HPI_OBJ_SUBSYSTEM,
				HPI_SUBSYS_GET_ADAPTER, 0);

			/* This is complicated by the fact that we want to
			 * "skip" 0's in the adapter list.
			 * First, make sure we are pointing to a
			 * non-zero adapter type.
			 */
			while (gRESP_HPI_SUBSYS_FIND_ADAPTERS.s.
				awAdapterList[nIndex] == 0) {
				nIndex++;
				if (nIndex >= HPI_MAX_ADAPTERS)
					break;
			}
			while (nCount) {
				/* move on to the next adapter */
				nIndex++;
				if (nIndex >= HPI_MAX_ADAPTERS)
					break;
				while (gRESP_HPI_SUBSYS_FIND_ADAPTERS.s.
					awAdapterList[nIndex] == 0) {
					nIndex++;
					if (nIndex >= HPI_MAX_ADAPTERS)
						break;
				}
				nCount--;
			}

			if (nIndex < HPI_MAX_ADAPTERS) {
				phr->u.s.wAdapterIndex = (u16)nIndex;
				phr->u.s.awAdapterList[0] =
					gRESP_HPI_SUBSYS_FIND_ADAPTERS.s.
					awAdapterList[nIndex];
			} else {
				phr->u.s.wAdapterIndex = 0;
				phr->u.s.awAdapterList[0] = 0;
				phr->wError = HPI_ERROR_BAD_ADAPTER_NUMBER;
			}
			break;
		}
	case HPI_SUBSYS_CREATE_ADAPTER:
		HPIMSGX_Init(phm, phr);
		break;
	case HPI_SUBSYS_DELETE_ADAPTER:
		HPIMSGX_Cleanup(phm->wAdapterIndex, hOwner);
		{
			struct hpi_message hm;
			struct hpi_response hr;
			/* call to HPI_ADAPTER_CLOSE */
			HPI_InitMessage(&hm, HPI_OBJ_ADAPTER,
				HPI_ADAPTER_CLOSE);
			hm.wAdapterIndex = phm->wAdapterIndex;
			HW_EntryPoint(&hm, &hr);
		}
		HW_EntryPoint(phm, phr);
		gRESP_HPI_SUBSYS_FIND_ADAPTERS.s.awAdapterList[phm->
			wAdapterIndex]
			= 0;
		hpi_entry_points[phm->wAdapterIndex] = NULL;
		break;
	default:
		HW_EntryPoint(phm, phr);
		break;
	}
}

static void AdapterMessage(
	struct hpi_message *phm,
	struct hpi_response *phr,
	void *hOwner
)
{
	switch (phm->wFunction) {
	case HPI_ADAPTER_OPEN:
		AdapterOpen(phm, phr);
		break;
	case HPI_ADAPTER_CLOSE:
		AdapterClose(phm, phr);
		break;
	default:
		HW_EntryPoint(phm, phr);
		break;
	}
}

static void MixerMessage(
	struct hpi_message *phm,
	struct hpi_response *phr
)
{
	switch (phm->wFunction) {
	case HPI_MIXER_OPEN:
		MixerOpen(phm, phr);
		break;
	case HPI_MIXER_CLOSE:
		MixerClose(phm, phr);
		break;
	default:
		HW_EntryPoint(phm, phr);
		break;
	}
}

static void OStreamMessage(
	struct hpi_message *phm,
	struct hpi_response *phr,
	void *hOwner
)
{
	if (phm->u.d.wStreamIndex >=
		aADAPTER_INFO[phm->wAdapterIndex].wNumOutStreams) {
		HPI_InitResponse(phr, HPI_OBJ_OSTREAM, phm->wFunction,
			HPI_ERROR_INVALID_OBJ_INDEX);
		return;
	}

	switch (phm->wFunction) {
	case HPI_OSTREAM_OPEN:
		OutStreamOpen(phm, phr, hOwner);
		break;
	case HPI_OSTREAM_CLOSE:
		OutStreamClose(phm, phr, hOwner);
		break;
	default:
		HW_EntryPoint(phm, phr);
		break;
	}
}

static void IStreamMessage(
	struct hpi_message *phm,
	struct hpi_response *phr,
	void *hOwner
)
{
	if (phm->u.d.wStreamIndex >=
		aADAPTER_INFO[phm->wAdapterIndex].wNumInStreams) {
		HPI_InitResponse(phr, HPI_OBJ_ISTREAM, phm->wFunction,
			HPI_ERROR_INVALID_OBJ_INDEX);
		return;
	}

	switch (phm->wFunction) {
	case HPI_ISTREAM_OPEN:
		InStreamOpen(phm, phr, hOwner);
		break;
	case HPI_ISTREAM_CLOSE:
		InStreamClose(phm, phr, hOwner);
		break;
	default:
		HW_EntryPoint(phm, phr);
		break;
	}
}

/* NOTE: HPI_Message() must be defined in the driver as a wrapper for
 * HPI_MessageEx so that functions in hpifunc.c compile.
 */
void HPI_MessageEx(
	struct hpi_message *phm,
	struct hpi_response *phr,
	void *hOwner
)
{
	HPI_DEBUG_MESSAGE(phm);

	if (phm->wType != HPI_TYPE_MESSAGE) {
		HPI_InitResponse(phr, phm->wObject, phm->wFunction,
			HPI_ERROR_INVALID_TYPE);
		return;
	}

	if (phm->wAdapterIndex >= HPI_MAX_ADAPTERS &&
		phm->wAdapterIndex != HPIMSGX_ALLADAPTERS) {
		HPI_InitResponse(phr, phm->wObject, phm->wFunction,
			HPI_ERROR_BAD_ADAPTER_NUMBER);
		return;
	}

	switch (phm->wObject) {
	case HPI_OBJ_SUBSYSTEM:
		SubSysMessage(phm, phr, hOwner);
		break;

	case HPI_OBJ_ADAPTER:
		AdapterMessage(phm, phr, hOwner);
		break;

	case HPI_OBJ_MIXER:
		MixerMessage(phm, phr);
		break;

	case HPI_OBJ_OSTREAM:
		OStreamMessage(phm, phr, hOwner);
		break;

	case HPI_OBJ_ISTREAM:
		IStreamMessage(phm, phr, hOwner);
		break;

	default:
		HW_EntryPoint(phm, phr);
		break;
	}
	HPI_DEBUG_RESPONSE(phr);
#if 1
	if (phr->wError >= HPI_ERROR_BACKEND_BASE) {
		void *ep = NULL;
		char *ep_name;

		hpi_debug_message(phm, HPI_DEBUG_FLAG_DEBUG
			FILE_LINE DBG_TEXT("DEBUG "));

		ep = hpi_entry_points[phm->wAdapterIndex];

		/* Don't need this? Have adapter index in debug info
		   Know at driver load time index->backend mapping */
		if (ep == HPI_6000)
			ep_name = "HPI_6000";
		else if (ep == HPI_6205)
			ep_name = "HPI_6205";
		else if (ep == HPI_4000)
			ep_name = "HPI_4000";
		else
			ep_name = "unknown";

		HPI_DEBUG_LOG(ERROR,
			DBG_TEXT("HPI %s Response - error# %d\n"),
			ep_name, phr->wError);

		if (hpiDebugLevel >= HPI_DEBUG_LEVEL_VERBOSE)
			hpi_debug_data((u16 *)phm,
				sizeof(*phm) / sizeof(u16));
	}
#endif
}

static void AdapterOpen(
	struct hpi_message *phm,
	struct hpi_response *phr
)
{
	HPI_DEBUG_LOG(VERBOSE, "AdapterOpen\n");
	memcpy(phr, &aRESP_HPI_ADAPTER_OPEN[phm->wAdapterIndex],
		sizeof(aRESP_HPI_ADAPTER_OPEN[0]));
}

static void AdapterClose(
	struct hpi_message *phm,
	struct hpi_response *phr
)
{
	HPI_DEBUG_LOG(VERBOSE, "AdapterClose\n");
	HPI_InitResponse(phr, HPI_OBJ_ADAPTER, HPI_ADAPTER_CLOSE, 0);
}

static void MixerOpen(
	struct hpi_message *phm,
	struct hpi_response *phr
)
{
	memcpy(phr, &aRESP_HPI_MIXER_OPEN[phm->wAdapterIndex],
		sizeof(aRESP_HPI_MIXER_OPEN[0]));
}

static void MixerClose(
	struct hpi_message *phm,
	struct hpi_response *phr
)
{
	HPI_InitResponse(phr, HPI_OBJ_MIXER, HPI_MIXER_CLOSE, 0);
}

static void InStreamOpen(
	struct hpi_message *phm,
	struct hpi_response *phr,
	void *hOwner
)
{

	struct hpi_message hm;
	struct hpi_response hr;

	HPI_InitResponse(phr, HPI_OBJ_ISTREAM, HPI_ISTREAM_OPEN, 0);

	HpiOs_Msgxlock_Lock(&msgxLock);

	if (aIStreamUserOpen[phm->wAdapterIndex][phm->u.d.wStreamIndex].
		nOpenFlag)
		phr->wError = HPI_ERROR_OBJ_ALREADY_OPEN;
	else if (aRESP_HPI_ISTREAM_OPEN[phm->wAdapterIndex]
		[phm->u.d.wStreamIndex].h.wError)
		memcpy(phr,
			&aRESP_HPI_ISTREAM_OPEN[phm->wAdapterIndex][phm->u.d.
				wStreamIndex],
			sizeof(aRESP_HPI_ISTREAM_OPEN[0][0]));
	else {
		aIStreamUserOpen[phm->wAdapterIndex][phm->u.d.wStreamIndex].
			nOpenFlag = 1;
		HpiOs_Msgxlock_UnLock(&msgxLock);

		/* issue a reset */
		memcpy(&hm, phm, sizeof(hm));
		hm.wFunction = HPI_ISTREAM_RESET;
		HW_EntryPoint(&hm, &hr);

		HpiOs_Msgxlock_Lock(&msgxLock);
		if (hr.wError) {
			aIStreamUserOpen[phm->wAdapterIndex][phm->u.d.
				wStreamIndex].nOpenFlag = 0;
			phr->wError = hr.wError;
		} else {
			aIStreamUserOpen[phm->wAdapterIndex][phm->u.d.
				wStreamIndex].nOpenFlag = 1;
			aIStreamUserOpen[phm->wAdapterIndex][phm->u.d.
				wStreamIndex].hOwner = hOwner;
			memcpy(phr,
				&aRESP_HPI_ISTREAM_OPEN[phm->
					wAdapterIndex][phm->u.
					d.
					wStreamIndex],
				sizeof(aRESP_HPI_ISTREAM_OPEN[0][0]));
		}
	}
	HpiOs_Msgxlock_UnLock(&msgxLock);
}

static void InStreamClose(
	struct hpi_message *phm,
	struct hpi_response *phr,
	void *hOwner
)
{

	struct hpi_message hm;
	struct hpi_response hr;

	HPI_InitResponse(phr, HPI_OBJ_ISTREAM, HPI_ISTREAM_CLOSE, 0);

	HpiOs_Msgxlock_Lock(&msgxLock);
	if (hOwner ==
		aIStreamUserOpen[phm->wAdapterIndex][phm->u.d.wStreamIndex].
		hOwner) {
		/* HPI_DEBUG_LOG(INFO,"closing adapter %d instream %d owned by %p\n",
		   phm->wAdapterIndex, phm->u.d.wStreamIndex, hOwner); */
		aIStreamUserOpen[phm->wAdapterIndex][phm->u.d.wStreamIndex].
			hOwner = NULL;
		HpiOs_Msgxlock_UnLock(&msgxLock);
		/* issue a reset */
		memcpy(&hm, phm, sizeof(hm));
		hm.wFunction = HPI_ISTREAM_RESET;
		HW_EntryPoint(&hm, &hr);
		HpiOs_Msgxlock_Lock(&msgxLock);
		if (hr.wError) {
			aIStreamUserOpen[phm->wAdapterIndex][phm->u.d.
				wStreamIndex].hOwner = hOwner;
			phr->wError = hr.wError;
		} else {
			aIStreamUserOpen[phm->wAdapterIndex][phm->u.d.
				wStreamIndex].nOpenFlag = 0;
			aIStreamUserOpen[phm->wAdapterIndex][phm->u.d.
				wStreamIndex].hOwner = NULL;
		}
	} else {
		HPI_DEBUG_LOG(WARNING,
			"%p trying to close %d instream %d owned by %p\n",
			hOwner, phm->wAdapterIndex,
			phm->u.d.wStreamIndex,
			aIStreamUserOpen[phm->wAdapterIndex][phm->u.d.
				wStreamIndex].hOwner);
		phr->wError = HPI_ERROR_OBJ_NOT_OPEN;
	}
	HpiOs_Msgxlock_UnLock(&msgxLock);
}

static void OutStreamOpen(
	struct hpi_message *phm,
	struct hpi_response *phr,
	void *hOwner
)
{

	struct hpi_message hm;
	struct hpi_response hr;

	HPI_InitResponse(phr, HPI_OBJ_OSTREAM, HPI_OSTREAM_OPEN, 0);

	HpiOs_Msgxlock_Lock(&msgxLock);

	if (aOStreamUserOpen[phm->wAdapterIndex][phm->u.d.wStreamIndex].
		nOpenFlag)
		phr->wError = HPI_ERROR_OBJ_ALREADY_OPEN;
	else if (aRESP_HPI_OSTREAM_OPEN[phm->wAdapterIndex]
		[phm->u.d.wStreamIndex].h.wError)
		memcpy(phr,
			&aRESP_HPI_OSTREAM_OPEN[phm->wAdapterIndex][phm->u.d.
				wStreamIndex],
			sizeof(aRESP_HPI_OSTREAM_OPEN[0][0]));
	else {
		aOStreamUserOpen[phm->wAdapterIndex][phm->u.d.wStreamIndex].
			nOpenFlag = 1;
		HpiOs_Msgxlock_UnLock(&msgxLock);

		/* issue a reset */
		memcpy(&hm, phm, sizeof(hm));
		hm.wFunction = HPI_OSTREAM_RESET;
		HW_EntryPoint(&hm, &hr);

		HpiOs_Msgxlock_Lock(&msgxLock);
		if (hr.wError) {
			aOStreamUserOpen[phm->wAdapterIndex][phm->u.d.
				wStreamIndex].nOpenFlag = 0;
			phr->wError = hr.wError;
		} else {
			aOStreamUserOpen[phm->wAdapterIndex][phm->u.d.
				wStreamIndex].nOpenFlag = 1;
			aOStreamUserOpen[phm->wAdapterIndex][phm->u.d.
				wStreamIndex].hOwner = hOwner;
			memcpy(phr,
				&aRESP_HPI_OSTREAM_OPEN[phm->
					wAdapterIndex][phm->u.
					d.
					wStreamIndex],
				sizeof(aRESP_HPI_OSTREAM_OPEN[0][0]));
		}
	}
	HpiOs_Msgxlock_UnLock(&msgxLock);
}

static void OutStreamClose(
	struct hpi_message *phm,
	struct hpi_response *phr,
	void *hOwner
)
{

	struct hpi_message hm;
	struct hpi_response hr;

	HPI_InitResponse(phr, HPI_OBJ_OSTREAM, HPI_OSTREAM_CLOSE, 0);

	HpiOs_Msgxlock_Lock(&msgxLock);

	if (hOwner ==
		aOStreamUserOpen[phm->wAdapterIndex][phm->u.d.wStreamIndex].
		hOwner) {
		/* HPI_DEBUG_LOG(INFO,"closing adapter %d outstream %d owned by %p\n",
		   phm->wAdapterIndex, phm->u.d.wStreamIndex, hOwner); */
		aOStreamUserOpen[phm->wAdapterIndex][phm->u.d.wStreamIndex].
			hOwner = NULL;
		HpiOs_Msgxlock_UnLock(&msgxLock);
		/* issue a reset */
		memcpy(&hm, phm, sizeof(hm));
		hm.wFunction = HPI_OSTREAM_RESET;
		HW_EntryPoint(&hm, &hr);
		HpiOs_Msgxlock_Lock(&msgxLock);
		if (hr.wError) {
			aOStreamUserOpen[phm->wAdapterIndex][phm->u.d.
				wStreamIndex].hOwner = hOwner;
			phr->wError = hr.wError;
		} else {
			aOStreamUserOpen[phm->wAdapterIndex][phm->u.d.
				wStreamIndex].nOpenFlag = 0;
			aOStreamUserOpen[phm->wAdapterIndex][phm->u.d.
				wStreamIndex].hOwner = NULL;
		}
	} else {
		HPI_DEBUG_LOG(WARNING,
			"%p trying to close %d outstream %d owned by %p\n",
			hOwner, phm->wAdapterIndex,
			phm->u.d.wStreamIndex,
			aOStreamUserOpen[phm->wAdapterIndex][phm->u.d.
				wStreamIndex].hOwner);
		phr->wError = HPI_ERROR_OBJ_NOT_OPEN;
	}
	HpiOs_Msgxlock_UnLock(&msgxLock);
}

static u16 AdapterPrepare(
	u16 wAdapter
)
{
	struct hpi_message hm;
	struct hpi_response hr;

	/* Open the adapter and streams */
	u16 i;

	/* call to HPI_ADAPTER_OPEN */
	HPI_InitMessage(&hm, HPI_OBJ_ADAPTER, HPI_ADAPTER_OPEN);
	hm.wAdapterIndex = wAdapter;
	HW_EntryPoint(&hm, &hr);
	memcpy(&aRESP_HPI_ADAPTER_OPEN[wAdapter], &hr,
		sizeof(aRESP_HPI_ADAPTER_OPEN[0]));
	if (hr.wError)
		return hr.wError;

	/* call to HPI_ADAPTER_GET_INFO */
	HPI_InitMessage(&hm, HPI_OBJ_ADAPTER, HPI_ADAPTER_GET_INFO);
	hm.wAdapterIndex = wAdapter;
	HW_EntryPoint(&hm, &hr);
	if (hr.wError)
		return hr.wError;

	aADAPTER_INFO[wAdapter].wNumOutStreams = hr.u.a.wNumOStreams;
	aADAPTER_INFO[wAdapter].wNumInStreams = hr.u.a.wNumIStreams;
	aADAPTER_INFO[wAdapter].wType = hr.u.a.wAdapterType;

	gRESP_HPI_SUBSYS_FIND_ADAPTERS.s.awAdapterList[wAdapter] =
		hr.u.a.wAdapterType;
	gRESP_HPI_SUBSYS_FIND_ADAPTERS.s.wNumAdapters++;
	if (gRESP_HPI_SUBSYS_FIND_ADAPTERS.s.wNumAdapters > HPI_MAX_ADAPTERS)
		gRESP_HPI_SUBSYS_FIND_ADAPTERS.s.wNumAdapters =
			HPI_MAX_ADAPTERS;

	/* call to HPI_OSTREAM_OPEN */
	for (i = 0; i < aADAPTER_INFO[wAdapter].wNumOutStreams; i++) {
		HPI_InitMessage(&hm, HPI_OBJ_OSTREAM, HPI_OSTREAM_OPEN);
		hm.wAdapterIndex = wAdapter;
		hm.u.d.wStreamIndex = i;
		HW_EntryPoint(&hm, &hr);
		memcpy(&aRESP_HPI_OSTREAM_OPEN[wAdapter][i], &hr,
			sizeof(aRESP_HPI_OSTREAM_OPEN[0][0]));
		aOStreamUserOpen[wAdapter][i].nOpenFlag = 0;
		aOStreamUserOpen[wAdapter][i].hOwner = NULL;
	}

	/* call to HPI_ISTREAM_OPEN */
	for (i = 0; i < aADAPTER_INFO[wAdapter].wNumInStreams; i++) {
		HPI_AdapterFindObject(NULL, wAdapter, HPI_OBJ_ISTREAM, i,
			&aIStreamUserOpen[wAdapter][i].wDspIndex);
		HPI_InitMessage(&hm, HPI_OBJ_ISTREAM, HPI_ISTREAM_OPEN);
		hm.wAdapterIndex = wAdapter;
		hm.u.d.wStreamIndex = i;
		hm.wDspIndex = aIStreamUserOpen[wAdapter][i].wDspIndex;
		HW_EntryPoint(&hm, &hr);
		memcpy(&aRESP_HPI_ISTREAM_OPEN[wAdapter][i], &hr,
			sizeof(aRESP_HPI_ISTREAM_OPEN[0][0]));
		aIStreamUserOpen[wAdapter][i].nOpenFlag = 0;
		aIStreamUserOpen[wAdapter][i].hOwner = NULL;
	}

	/* call to HPI_MIXER_OPEN */
	HPI_InitMessage(&hm, HPI_OBJ_MIXER, HPI_MIXER_OPEN);
	hm.wAdapterIndex = wAdapter;
	HW_EntryPoint(&hm, &hr);
	memcpy(&aRESP_HPI_MIXER_OPEN[wAdapter], &hr,
		sizeof(aRESP_HPI_MIXER_OPEN[0]));

	return gRESP_HPI_SUBSYS_FIND_ADAPTERS.h.wError;
}

static void HPIMSGX_Reset(
	u16 wAdapterIndex
)
{
	int i;
	u16 wAdapter;
	struct hpi_response hr;

	if (wAdapterIndex == HPIMSGX_ALLADAPTERS) {
		/* reset all responses to contain errors */
		HPI_InitResponse(&hr, HPI_OBJ_SUBSYSTEM,
			HPI_SUBSYS_FIND_ADAPTERS, 0);
		memcpy(&gRESP_HPI_SUBSYS_FIND_ADAPTERS, &hr,
			sizeof(&gRESP_HPI_SUBSYS_FIND_ADAPTERS));

		for (wAdapter = 0; wAdapter < HPI_MAX_ADAPTERS; wAdapter++) {

			HPI_InitResponse(&hr, HPI_OBJ_ADAPTER,
				HPI_ADAPTER_OPEN, HPI_ERROR_BAD_ADAPTER);
			memcpy(&aRESP_HPI_ADAPTER_OPEN[wAdapter], &hr,
				sizeof(aRESP_HPI_ADAPTER_OPEN[wAdapter]));

			HPI_InitResponse(&hr, HPI_OBJ_MIXER, HPI_MIXER_OPEN,
				HPI_ERROR_INVALID_OBJ);
			memcpy(&aRESP_HPI_MIXER_OPEN[wAdapter], &hr,
				sizeof(aRESP_HPI_MIXER_OPEN[wAdapter]));

			for (i = 0; i < HPI_MAX_STREAMS; i++) {
				HPI_InitResponse(&hr, HPI_OBJ_OSTREAM,
					HPI_OSTREAM_OPEN,
					HPI_ERROR_INVALID_OBJ);
				memcpy(&aRESP_HPI_OSTREAM_OPEN[wAdapter][i],
					&hr,
					sizeof(aRESP_HPI_OSTREAM_OPEN
						[wAdapter]
						[i]));
				HPI_InitResponse(&hr, HPI_OBJ_ISTREAM,
					HPI_ISTREAM_OPEN,
					HPI_ERROR_INVALID_OBJ);
				memcpy(&aRESP_HPI_ISTREAM_OPEN[wAdapter][i],
					&hr,
					sizeof(aRESP_HPI_ISTREAM_OPEN
						[wAdapter]
						[i]));
			}
		}
	} else if (wAdapterIndex < HPI_MAX_ADAPTERS) {
		aRESP_HPI_ADAPTER_OPEN[wAdapterIndex].h.wError =
			HPI_ERROR_BAD_ADAPTER;
		aRESP_HPI_MIXER_OPEN[wAdapterIndex].h.wError =
			HPI_ERROR_INVALID_OBJ;
		for (i = 0; i < HPI_MAX_STREAMS; i++) {
			aRESP_HPI_OSTREAM_OPEN[wAdapterIndex][i].h.wError =
				HPI_ERROR_INVALID_OBJ;
			aRESP_HPI_ISTREAM_OPEN[wAdapterIndex][i].h.wError =
				HPI_ERROR_INVALID_OBJ;
		}
		if (gRESP_HPI_SUBSYS_FIND_ADAPTERS.s.
			awAdapterList[wAdapterIndex]) {
			gRESP_HPI_SUBSYS_FIND_ADAPTERS.s.
				awAdapterList[wAdapterIndex] = 0;
			gRESP_HPI_SUBSYS_FIND_ADAPTERS.s.wNumAdapters--;
		}
	}
}

static u16 HPIMSGX_Init(
	struct hpi_message *phm,
	/* HPI_SUBSYS_CREATE_ADAPTER structure with */
	/* resource list or NULL=find all */
	struct hpi_response *phr
	/* response from HPI_ADAPTER_GET_INFO */
)
{
	HPI_HandlerFunc *entry_point_func;
	struct hpi_response hr;

	if (gRESP_HPI_SUBSYS_FIND_ADAPTERS.s.wNumAdapters >= HPI_MAX_ADAPTERS)
		return HPI_ERROR_BAD_ADAPTER_NUMBER;

	/* Init response here so we can pass in previous adapter list */
	HPI_InitResponse(&hr, phm->wObject, phm->wFunction,
		HPI_ERROR_INVALID_OBJ);
	memcpy(hr.u.s.awAdapterList,
		gRESP_HPI_SUBSYS_FIND_ADAPTERS.s.awAdapterList,
		sizeof(gRESP_HPI_SUBSYS_FIND_ADAPTERS.s.awAdapterList));

	entry_point_func =
		HPI_LookupEntryPointFunction(phm->u.s.Resource.r.Pci);

	if (entry_point_func) {
		HPI_DEBUG_MESSAGE(phm);
		entry_point_func(phm, &hr);
	} else {
		phr->wError = HPI_ERROR_PROCESSING_MESSAGE;
		return phr->wError;
	}
	/* if the adapter was created succesfully save the mapping for future use */
	if (hr.wError == 0) {
		hpi_entry_points[hr.u.s.wAdapterIndex] = entry_point_func;
		/* prepare adapter (pre-open streams etc.) */
		HPI_DEBUG_LOG(DEBUG,
			"HPI_SUBSYS_CREATE_ADAPTER successful,"
			" preparing adapter\n");
		AdapterPrepare(hr.u.s.wAdapterIndex);
	}
	memcpy(phr, &hr, hr.wSize);
	return phr->wError;
}

static void HPIMSGX_Cleanup(
	u16 wAdapterIndex,
	void *hOwner
)
{
	int i, wAdapter, wAdapterLimit;

	if (!hOwner)
		return;

	if (wAdapterIndex == HPIMSGX_ALLADAPTERS) {
		wAdapter = 0;
		wAdapterLimit = HPI_MAX_ADAPTERS;
	} else {
		wAdapter = wAdapterIndex;
		wAdapterLimit = wAdapter + 1;
	}

	for (; wAdapter < wAdapterLimit; wAdapter++) {
		/*	printk(KERN_INFO "Cleanup adapter #%d\n",wAdapter); */
		for (i = 0; i < HPI_MAX_STREAMS; i++) {
			if (hOwner == aOStreamUserOpen[wAdapter][i].hOwner) {
				struct hpi_message hm;
				struct hpi_response hr;

				HPI_DEBUG_LOG(DEBUG,
					"Close adapter %d ostream %d\n",
					wAdapter, i);

				HPI_InitMessage(&hm, HPI_OBJ_OSTREAM,
					HPI_OSTREAM_RESET);
				hm.wAdapterIndex = (u16)wAdapter;
				hm.u.d.wStreamIndex = (u16)i;
				/* hm.wDspIndex = Always 0 for Ostream */
				HW_EntryPoint(&hm, &hr);

				hm.wFunction = HPI_OSTREAM_HOSTBUFFER_FREE;
				HW_EntryPoint(&hm, &hr);

				hm.wFunction = HPI_OSTREAM_GROUP_RESET;
				HW_EntryPoint(&hm, &hr);

				aOStreamUserOpen[wAdapter][i].nOpenFlag = 0;
				aOStreamUserOpen[wAdapter][i].hOwner = NULL;
			}
			if (hOwner == aIStreamUserOpen[wAdapter][i].hOwner) {
				struct hpi_message hm;
				struct hpi_response hr;

				HPI_DEBUG_LOG(DEBUG,
					"Close adapter %d istream %d\n",
					wAdapter, i);

				HPI_InitMessage(&hm, HPI_OBJ_ISTREAM,
					HPI_ISTREAM_RESET);
				hm.wAdapterIndex = (u16)wAdapter;
				hm.u.d.wStreamIndex = (u16)i;
				hm.wDspIndex =
					aIStreamUserOpen[wAdapter][i].
					wDspIndex;
				HW_EntryPoint(&hm, &hr);

				hm.wFunction = HPI_ISTREAM_HOSTBUFFER_FREE;
				HW_EntryPoint(&hm, &hr);

				hm.wFunction = HPI_ISTREAM_GROUP_RESET;
				HW_EntryPoint(&hm, &hr);

				aIStreamUserOpen[wAdapter][i].nOpenFlag = 0;
				aIStreamUserOpen[wAdapter][i].hOwner = NULL;
			}
		}
	}
}
