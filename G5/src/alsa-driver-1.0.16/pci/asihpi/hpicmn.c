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

 Common functions used by hpixxxx.c modules

(C) Copyright AudioScience Inc. 1998-2003
*******************************************************************************/
#define SOURCEFILE_NAME "hpicmn.c"
#include "hpi.h"
#include "hpidebug.h"
#include "hpicmn.h"

struct hpi_adapters_list {
	struct hpios_spinlock aListLock;
	struct hpi_adapter_obj adapter[HPI_MAX_ADAPTERS];
	u16 gwNumAdapters;
};

static struct hpi_adapters_list adapters;

/**
* Given an HPI Message that was sent out and a response that was received,
* validate that the response has the correct fields filled in,
* i.e ObjectType, Function etc
**/
u16 HpiValidateResponse(
	struct hpi_message *phm,
	struct hpi_response *phr
)
{
	u16 wError = 0;

	if ((phr->wType != HPI_TYPE_RESPONSE)
		|| (phr->wObject != phm->wObject)
		|| (phr->wFunction != phm->wFunction))
		wError = HPI_ERROR_INVALID_RESPONSE;

	return wError;
}

u16 HpiAddAdapter(
	struct hpi_adapter_obj *pao
)
{
	u16 retval = 0;
	/*HPI_ASSERT(pao->wAdapterType); */

	HpiOs_Alistlock_Lock(&adapters);

	if (pao->wIndex >= HPI_MAX_ADAPTERS) {
		retval = HPI_ERROR_BAD_ADAPTER_NUMBER;
	} else if (adapters.adapter[pao->wIndex].wAdapterType != 0) {
		retval = HPI_DUPLICATE_ADAPTER_NUMBER;
	} else {
		adapters.adapter[pao->wIndex] = *pao;
		HpiOs_Dsplock_Init(&adapters.adapter[pao->wIndex]);
		adapters.gwNumAdapters++;
	}

	HpiOs_Alistlock_UnLock(&adapters);
	return retval;
}

void HpiDeleteAdapter(
	struct hpi_adapter_obj *pao
)
{
	memset(pao, 0, sizeof(struct hpi_adapter_obj));

	HpiOs_Alistlock_Lock(&adapters);
	adapters.gwNumAdapters--;	/* dec the number of adapters */
	HpiOs_Alistlock_UnLock(&adapters);
}

/**
* FindAdapter returns a pointer to the struct hpi_adapter_obj with
* index wAdapterIndex in an HPI_ADAPTERS_LIST structure.
*
*/
struct hpi_adapter_obj *HpiFindAdapter(
	u16 wAdapterIndex
)
{
	struct hpi_adapter_obj *pao = NULL;

	if (wAdapterIndex >= HPI_MAX_ADAPTERS) {
		HPI_DEBUG_LOG(VERBOSE,
			DBG_TEXT("FindAdapter invalid index %d "),
			wAdapterIndex);
		return NULL;
	}

	pao = &adapters.adapter[wAdapterIndex];
	if (pao->wAdapterType != 0) {
		HPI_DEBUG_LOG(VERBOSE, DBG_TEXT("Found adapter index %d\n"),
			wAdapterIndex);
		return (pao);
	} else {
		HPI_DEBUG_LOG(VERBOSE, DBG_TEXT("No adapter index %d\n"),
			wAdapterIndex);
		return (NULL);
	}
}

/**
*
* wipe an HPI_ADAPTERS_LIST structure.
*
**/
static void WipeAdapterList(
	void
)
{
	memset(&adapters, 0, sizeof(adapters));
}

/**
* SubSysGetAdapters fills awAdapterList in an struct hpi_response structure
* with all adapters in the given HPI_ADAPTERS_LIST.
*
*/
static void SubSysGetAdapters(
	struct hpi_response *phr
)
{
	/* fill in the response adapter array with the position */
	/* identified by the adapter number/index of the adapters in */
	/* this HPI */
	/* i.e. if we have an A120 with it's jumper set to */
	/* Adapter Number 2 then put an Adapter type A120 in the */
	/* array in position 1 */
	/* NOTE: AdapterNumber is 1..N, Index is 0..N-1 */

	/* input:  NONE */
	/* output: wNumAdapters */
	/*		   awAdapter[] */
	/* */

	short i;
	struct hpi_adapter_obj *pao = NULL;

	HPI_DEBUG_LOG(VERBOSE, DBG_TEXT("SubSysGetAdapters\n"));

	/* for each adapter, place it's type in the position of the array */
	/* corresponding to it's adapter number */
	for (i = 0; i < adapters.gwNumAdapters; i++) {
		pao = &adapters.adapter[i];
		if (phr->u.s.awAdapterList[pao->wIndex] != 0) {
			phr->wError = HPI_DUPLICATE_ADAPTER_NUMBER;
			phr->wSpecificError = pao->wIndex;
			return;
		}
		phr->u.s.awAdapterList[pao->wIndex] = pao->wAdapterType;
	}

	phr->u.s.wNumAdapters = adapters.gwNumAdapters;
	phr->wError = 0;	/* the function completed OK; */
}

/**
* CheckControlCache checks if a given struct hpi_control_cache_single control
* value is in the cache and fills the struct hpi_response accordingly.
* It returns nonzero if a cache hit occurred, zero otherwise.
*/
short HpiCheckControlCache(
	volatile struct hpi_control_cache_single *pC,
	struct hpi_message *phm,
	struct hpi_response *phr
)
{
	short found = 1;
	/* if the control type in the cache is non-zero then */
	/* we have cached control information to process */
	phr->wSize =
		sizeof(struct hpi_response_header) +
		sizeof(struct hpi_control_res);
	phr->wError = 0;
	switch (pC->ControlType) {
	case HPI_CONTROL_METER:
		if (phm->u.c.wAttribute == HPI_METER_PEAK) {
			phr->u.c.anLogValue[0] = pC->u.p.anLogPeak[0];
			phr->u.c.anLogValue[1] = pC->u.p.anLogPeak[1];
		} else if (phm->u.c.wAttribute == HPI_METER_RMS) {
			phr->u.c.anLogValue[0] = pC->u.p.anLogRMS[0];
			phr->u.c.anLogValue[1] = pC->u.p.anLogRMS[1];
		} else
			found = 0;	/* signal that message was not cached */
		break;
	case HPI_CONTROL_VOLUME:
		if (phm->u.c.wAttribute == HPI_VOLUME_GAIN) {
			phr->u.c.anLogValue[0] = pC->u.v.anLog[0];
			phr->u.c.anLogValue[1] = pC->u.v.anLog[1];
		} else
			found = 0;	/* signal that message was not cached */
		break;
	case HPI_CONTROL_MULTIPLEXER:
		if (phm->u.c.wAttribute == HPI_MULTIPLEXER_SOURCE) {
			phr->u.c.dwParam1 = pC->u.x.wSourceNodeType;
			phr->u.c.dwParam2 = pC->u.x.wSourceNodeIndex;
		} else
			found = 0;	/* signal that message was not cached */
		break;
	case HPI_CONTROL_CHANNEL_MODE:
		if (phm->u.c.wAttribute == HPI_CHANNEL_MODE_MODE)
			phr->u.c.dwParam1 = pC->u.m.wMode;
		else
			found = 0;	/* signal that message was not cached */

	case HPI_CONTROL_LEVEL:
		if (phm->u.c.wAttribute == HPI_LEVEL_GAIN) {
			phr->u.c.anLogValue[0] = pC->u.l.anLog[0];
			phr->u.c.anLogValue[1] = pC->u.l.anLog[1];
		} else
			found = 0;	/* signal that message was not cached */
		break;
	case HPI_CONTROL_TUNER:
		if (phm->u.c.wAttribute == HPI_TUNER_FREQ)
			phr->u.c.dwParam1 = pC->u.t.dwFreqInkHz;
		else if (phm->u.c.wAttribute == HPI_TUNER_BAND)
			phr->u.c.dwParam1 = pC->u.t.wBand;
		else if ((phm->u.c.wAttribute == HPI_TUNER_LEVEL) &&
			(phm->u.c.dwParam1 == HPI_TUNER_LEVEL_AVERAGE))
			phr->u.c.dwParam1 = pC->u.t.wLevel;
		else
			found = 0;	/* signal that message was not cached */
		break;
	case HPI_CONTROL_AESEBU_RECEIVER:
		if (phm->u.c.wAttribute == HPI_AESEBURX_ERRORSTATUS)
			phr->u.c.dwParam1 = pC->u.aes3rx.dwErrorStatus;
		else if (phm->u.c.wAttribute == HPI_AESEBURX_FORMAT)
			phr->u.c.dwParam1 = pC->u.aes3rx.dwSource;
		else
			found = 0;	/* signal that message was not cached */
		break;
	case HPI_CONTROL_AESEBU_TRANSMITTER:
		if (phm->u.c.wAttribute == HPI_AESEBUTX_FORMAT)
			phr->u.c.dwParam1 = pC->u.aes3tx.dwFormat;
		else
			found = 0;	/* signal that message was not cached */
		break;
	case HPI_CONTROL_TONEDETECTOR:
		if (phm->u.c.wAttribute == HPI_TONEDETECTOR_STATE)
			phr->u.c.dwParam1 = pC->u.tone.wState;
		else
			found = 0;	/* signal that message was not cached */
		break;
	case HPI_CONTROL_SILENCEDETECTOR:
		if (phm->u.c.wAttribute == HPI_SILENCEDETECTOR_STATE) {
			phr->u.c.dwParam1 = pC->u.silence.dwState;
			phr->u.c.dwParam2 = pC->u.silence.dwCount;
		} else
			found = 0;	/* signal that message was not cached */
		break;
	case HPI_CONTROL_SAMPLECLOCK:
		if (phm->u.c.wAttribute == HPI_SAMPLECLOCK_SOURCE)
			phr->u.c.dwParam1 = pC->u.clk.wSource;
		else if (phm->u.c.wAttribute == HPI_SAMPLECLOCK_SOURCE_INDEX) {
			if (pC->u.clk.wSourceIndex ==
				HPI_ERROR_ILLEGAL_CACHE_VALUE) {
				phr->u.c.dwParam1 = 0;
				phr->wError = HPI_ERROR_INVALID_OPERATION;
			} else
				phr->u.c.dwParam1 = pC->u.clk.wSourceIndex;
		} else if (phm->u.c.wAttribute == HPI_SAMPLECLOCK_SAMPLERATE)
			phr->u.c.dwParam1 = pC->u.clk.dwSampleRate;
		else
			found = 0;	/* signal that message was not cached */
		break;
	default:
		found = 0;	/* signal that message was not cached */
		break;
	}
	if (found == 0)
		HPI_DEBUG_LOG(VERBOSE, DBG_TEXT("Adap %d, Control %d, ")
			DBG_TEXT("Control type %d, Cached %d\n"),
			phm->wAdapterIndex, pC->ControlIndex,
			pC->ControlType, found);

	return found;
}

/** Updates the cache with Set values.

Only update if no error.
Volume and Level return the limited values in the response, so use these
Multiplexer does so use sent values
*/
void HpiSyncControlCache(
	volatile struct hpi_control_cache_single *pC,
	struct hpi_message *phm,
	struct hpi_response *phr
)
{
	if (phr->wError)
		return;

	switch (pC->ControlType) {
	case HPI_CONTROL_VOLUME:
		if (phm->u.c.wAttribute == HPI_VOLUME_GAIN) {
			pC->u.v.anLog[0] = phr->u.c.anLogValue[0];
			pC->u.v.anLog[1] = phr->u.c.anLogValue[1];
		}
		break;
	case HPI_CONTROL_MULTIPLEXER:
		/* mux does not return its setting on Set command. */
		if (phm->u.c.wAttribute == HPI_MULTIPLEXER_SOURCE) {
			pC->u.x.wSourceNodeType = (u16)phm->u.c.dwParam1;
			pC->u.x.wSourceNodeIndex = (u16)phm->u.c.dwParam2;
		}
		break;
	case HPI_CONTROL_CHANNEL_MODE:
		/* mux does not return its setting on Set command. */
		if (phm->u.c.wAttribute == HPI_MULTIPLEXER_SOURCE)
			pC->u.m.wMode = (u16)phm->u.c.dwParam1;
		break;
	case HPI_CONTROL_LEVEL:
		if (phm->u.c.wAttribute == HPI_LEVEL_GAIN) {
			pC->u.v.anLog[0] = phr->u.c.anLogValue[0];
			pC->u.v.anLog[1] = phr->u.c.anLogValue[1];
		}
		break;
	case HPI_CONTROL_AESEBU_TRANSMITTER:
		if (phm->u.c.wAttribute == HPI_AESEBUTX_FORMAT)
			pC->u.aes3tx.dwFormat = phm->u.c.dwParam1;
		break;
	case HPI_CONTROL_AESEBU_RECEIVER:
		if (phm->u.c.wAttribute == HPI_AESEBURX_FORMAT)
			pC->u.aes3rx.dwSource = phm->u.c.dwParam1;
	case HPI_CONTROL_SAMPLECLOCK:
		if (phm->u.c.wAttribute == HPI_SAMPLECLOCK_SOURCE)
			pC->u.clk.wSource = (u16)phm->u.c.dwParam1;
		else if (phm->u.c.wAttribute == HPI_SAMPLECLOCK_SOURCE_INDEX) {
			if (pC->u.clk.wSourceIndex ==
				HPI_ERROR_ILLEGAL_CACHE_VALUE) {
				phr->u.c.dwParam1 = 0;
				phr->wError = HPI_ERROR_INVALID_OPERATION;
			} else
				pC->u.clk.wSourceIndex =
					(u16)phm->u.c.dwParam1;
		} else if (phm->u.c.wAttribute == HPI_SAMPLECLOCK_SAMPLERATE)
			pC->u.clk.dwSampleRate = phm->u.c.dwParam1;
		break;
	default:
		break;
	}
}

static void SubSysMessage(
	struct hpi_message *phm,
	struct hpi_response *phr
)
{

	switch (phm->wFunction) {
	case HPI_SUBSYS_OPEN:
	case HPI_SUBSYS_CLOSE:
	case HPI_SUBSYS_DRIVER_UNLOAD:
		phr->wError = 0;
		break;
	case HPI_SUBSYS_DRIVER_LOAD:
		WipeAdapterList();
		HpiOs_Alistlock_Init(&adapters);
		phr->wError = 0;
		break;
	case HPI_SUBSYS_GET_INFO:
		SubSysGetAdapters(phr);
		break;
	case HPI_SUBSYS_CREATE_ADAPTER:
	case HPI_SUBSYS_DELETE_ADAPTER:
		phr->wError = 0;
		break;
	default:
		phr->wError = HPI_ERROR_INVALID_FUNC;
		break;
	}
}

void HPI_COMMON(
	struct hpi_message *phm,
	struct hpi_response *phr
)
{
	switch (phm->wType) {
	case HPI_TYPE_MESSAGE:
		switch (phm->wObject) {
		case HPI_OBJ_SUBSYSTEM:
			SubSysMessage(phm, phr);
			break;
		}
		break;

	default:
		phr->wError = HPI_ERROR_INVALID_TYPE;
		break;
	}
}
