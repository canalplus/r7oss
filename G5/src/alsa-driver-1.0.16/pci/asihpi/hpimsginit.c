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

 Hardware Programming Interface (HPI) Utility functions.

 (C) Copyright AudioScience Inc. 2007
*******************************************************************************/

#include "hpi.h"
#include "hpimsginit.h"

/* The actual message size for each object type */
static u16 aMsgSize[HPI_OBJ_MAXINDEX + 1] = HPI_MESSAGE_SIZE_BY_OBJECT;
/* The actual response size for each object type */
static u16 aResSize[HPI_OBJ_MAXINDEX + 1] = HPI_RESPONSE_SIZE_BY_OBJECT;
/* Flag to enable alternate message type for SSX2 bypass. */
static u16 gwSSX2Bypass = 0;

/** \internal
  * Used by ASIO driver to disable SSX2 for a single process
  * \param phSubSys Pointer to HPI subsystem handle.
  * \param wBypass New bypass setting 0 = off, nonzero = on
  * \return Previous bypass setting.
  */
u16 HPI_SubSysSsx2Bypass(
	struct hpi_hsubsys *phSubSys,
	u16 wBypass
)
{
	u16 oldValue = gwSSX2Bypass;

	gwSSX2Bypass = wBypass;

	return oldValue;
}

/** \internal
  * initialize the HPI message structure
  */
void HPI_InitMessage(
	struct hpi_message *phm,
	u16 wObject,
	u16 wFunction
)
{
	memset(phm, 0, sizeof(*phm));
	if ((wObject > 0) && (wObject <= HPI_OBJ_MAXINDEX)) {
		if (gwSSX2Bypass)
			phm->wType = HPI_TYPE_SSX2BYPASS_MESSAGE;
		else
			phm->wType = HPI_TYPE_MESSAGE;
		phm->wSize = aMsgSize[wObject];
		phm->wObject = wObject;
		phm->wFunction = wFunction;
		phm->wDspIndex = 0;
		/* Expect adapter index to be set by caller */
	} else {
		phm->wType = 0;
		phm->wSize = 0;
	}
}

/** \internal
  * initialize the HPI response structure
  */
void HPI_InitResponse(
	struct hpi_response *phr,
	u16 wObject,
	u16 wFunction,
	u16 wError
)
{
	memset(phr, 0, sizeof(*phr));
	phr->wType = HPI_TYPE_RESPONSE;
	phr->wSize = aResSize[wObject];
	phr->wObject = wObject;
	phr->wFunction = wFunction;
	phr->wError = wError;
	phr->wSpecificError = 0;
}
