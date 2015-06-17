/************************************************************************
Copyright (C) 2011 STMicroelectronics. All Rights Reserved.

This file is part of the Transport Engine Library.

The Transport Engine is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Transport Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Transport Engine; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
USA.

The Transport Engine Library may alternatively be licensed under a
proprietary license from ST.

************************************************************************/
/**
   @file       pti_filter.c
   @brief      Filter Object Initialisation, Termination and Manipulation Functions.

   This file implements the functions for creating, destroying and accessing PTI filters.  This
   includes SECTION filters, PES filters and a various Conditional Access Filters.

 */

#if 0
#define STPTI_PRINT
#endif

/* Includes ---------------------------------------------------------------- */

/* ANSI C includes */

/* STAPI includes */

/* Includes from API level */
#include "../pti_debug.h"
#include "../pti_hal_api.h"

/* Includes from the HAL / ObjMan level */
#include "pti_vdevice.h"
#include "pti_session.h"
#include "pti_slot.h"
#include "pti_filter.h"

/* MACROS ------------------------------------------------------------------ */
#define NUMBER_OF_FILTERFLUSH_STRUCTURES 8

/* Private Constants ------------------------------------------------------- */

/* Private Types ----------------------------------------------------------- */
typedef struct {
	U32 DiscardRegionSize;					/**< the number of bytes from the read pointer to discard over */
	U8 DiscardFiltersMask[SECTION_MATCHBYTES_LENGTH];	/**< the section filters to discard */
	BOOL DiscardSlotBasedFilter;				/**< if we should discard slot based filters (e.g. TINY) */
} stptiHAL_FiltersFlushStruct_t;

/* Private Variables ------------------------------------------------------- */

/* Private Function Prototypes --------------------------------------------- */
static BOOL stptiHAL_FilterIsSlotBased(stptiHAL_FilterType_t FilterType);

ST_ErrorCode_t stptiHAL_FilterAllocator(FullHandle_t FilterHandle, void *params_p);
ST_ErrorCode_t stptiHAL_FilterAssociator(FullHandle_t FilterHandle, FullHandle_t AssocObjectHandle);
ST_ErrorCode_t stptiHAL_FilterDisassociator(FullHandle_t FilterHandle, FullHandle_t AssocObjectHandle);
ST_ErrorCode_t stptiHAL_FilterDeallocator(FullHandle_t FilterHandle);
ST_ErrorCode_t stptiHAL_FilterSetDiscardOnCRCErr(FullHandle_t FilterHandle, BOOL Discard);
ST_ErrorCode_t stptiHAL_FilterEnable(FullHandle_t FilterHandle, BOOL Enable);
ST_ErrorCode_t stptiHAL_FilterUpdate(FullHandle_t FilterHandle, stptiHAL_FilterType_t FilterType, BOOL OneShotMode,
				     BOOL ForceCRCCheck, U8 *FilterData_p, U8 *FilterMask_p, U8 *FilterSense_p);
ST_ErrorCode_t stptiHAL_FilterUpdateProprietaryFilter(FullHandle_t FilterHandle, stptiHAL_FilterType_t FilterType,
						      void *FilterData_p, size_t SizeofFilterData);
void stptiHAL_FilterCAMSet(stptiHAL_vDevice_t *vDevice_p, int Filter, stptiHAL_FilterType_t FilterType,
			   U8 *FilterData_p, U8 *FilterMask_p, U8 *FilterSense_p, BOOL OneShotMode,
			   BOOL ForceCRCCheck);

/* Public Constants -------------------------------------------------------- */

/* Export the API */
const stptiHAL_FilterAPI_t stptiHAL_FilterAPI = {
	{
		/* Allocator              Associator                 Disassociator                 Deallocator */
		stptiHAL_FilterAllocator, stptiHAL_FilterAssociator, stptiHAL_FilterDisassociator, stptiHAL_FilterDeallocator,
		NULL, NULL
	},
	stptiHAL_FilterSetDiscardOnCRCErr,
	stptiHAL_FilterEnable,
	stptiHAL_FilterUpdate,
	stptiHAL_FilterUpdateProprietaryFilter,
};

/* Object Management Functions ------------------------------------------------------------------ */

/**
   @brief  Allocate a filter

   This function allocates the filter.

   @param  FilterHandle         The handle of the filter.
   @param  params_p             The parameters for this filter.

   @return                      A standard st error type...
                                - ST_NO_ERROR

 */
ST_ErrorCode_t stptiHAL_FilterAllocator(FullHandle_t FilterHandle, void *params_p)
{
	/* Already write locked */
	stptiHAL_Filter_t *Filter_p = stptiHAL_GetObjectFilter_p(FilterHandle);
	stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(FilterHandle);
	stptiHAL_FilterConfigParams_t *Parameters_p = (stptiHAL_FilterConfigParams_t *) params_p;

	ST_ErrorCode_t Error = ST_NO_ERROR;

	Filter_p->FilterType = Parameters_p->FilterType;
	Filter_p->DiscardOnCRCError = vDevice_p->ForceDiscardSectionOnCRCError;
	Filter_p->OneShotMode = FALSE;
	Filter_p->ForceCRCCheck = FALSE;

	if (ST_NO_ERROR == Error) {
		Filter_p->EnableOnAssociation = FALSE;
		Filter_p->FilterEnabled = FALSE;

		switch (Filter_p->FilterType) {
		case stptiHAL_PES_STREAMID_FILTER:
			Filter_p->u.PESStreamIDMask = 0;
			if (ST_NO_ERROR ==
			    stptiOBJMAN_AddToList(&vDevice_p->PESStreamIDFilterHandles, Filter_p, FALSE,
						  &Filter_p->FilterIndex)) {
				if (Filter_p->FilterIndex >= (int)vDevice_p->TP_Interface.NumberOfSlots) {
					/* Too many slots */
					stptiOBJMAN_RemoveFromList(&vDevice_p->PESStreamIDFilterHandles,
								   Filter_p->FilterIndex);
					Error = ST_ERROR_NO_MEMORY;
				}
			}
			break;

		case stptiHAL_TINY_FILTER:
			Filter_p->u.TinyFilter.FilterData = 0xFFFF;
			Filter_p->u.TinyFilter.FilterMask = 0xFFFF;
			Filter_p->u.TinyFilter.FilterAnyMatchMask = 0;
			break;

		case stptiHAL_PROPRIETARY_FILTER:
			break;

		case stptiHAL_SHORT_FILTER:
		case stptiHAL_LONG_FILTER:
		case stptiHAL_PNMM_FILTER:
		case stptiHAL_SHORT_VNMM_FILTER:
		case stptiHAL_LONG_VNMM_FILTER:
		case stptiHAL_PNMM_VNMM_FILTER:
			/* Section filter */
			if (ST_NO_ERROR ==
			    stptiOBJMAN_AddToList(&vDevice_p->SectionFilterHandles, Filter_p, FALSE,
						  &Filter_p->FilterIndex)) {
				if (Filter_p->FilterIndex >= (int)vDevice_p->TP_Interface.NumberOfSectionFilters) {
					/* Too many slots */
					stptiOBJMAN_RemoveFromList(&vDevice_p->SectionFilterHandles,
								   Filter_p->FilterIndex);
					Error = ST_ERROR_NO_MEMORY;
				} else {
					stptiHAL_FilterCAMSet(vDevice_p, Filter_p->FilterIndex,
							      Filter_p->FilterType, NULL, NULL, NULL, FALSE, FALSE);
				}
			}
			break;

		case stptiHAL_NO_FILTER:
		default:
			Error = ST_ERROR_BAD_PARAMETER;
			break;
		}
	}

	return (Error);
}

/**
   @brief  Assocate this filter

   This function assocates a filter to a slot.

   @param  FilterHandle         The handle of the filter.
   @param  AssocObjectHandle    The handle of the object to be associated.

   @return                      A standard st error type...
                                - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_FilterAssociator(FullHandle_t FilterHandle, FullHandle_t AssocObjectHandle)
{
	/* Already write locked */
	ST_ErrorCode_t Error = ST_NO_ERROR;

	/* Avoid compiler warning */
	FilterHandle = FilterHandle;

	switch (stptiOBJMAN_GetObjectType(AssocObjectHandle)) {
	case OBJECT_TYPE_SLOT:
		/* Nothing to do (all done by slot associator) */
		break;

	default:
		STPTI_PRINTF_ERROR("Filter cannot associate to this kind of object %d",
				   stptiOBJMAN_GetObjectType(AssocObjectHandle));
		Error = ST_ERROR_BAD_PARAMETER;
		break;
	}

	return (Error);
}

/**
   @brief  Disassociate this filter from another object

   This function disassociates the filter from a slot.

   @param  FilterHandle         The handle of the filter.
   @param  AssocObjectHandle    The handle of the object to be disassociated.

   @return                      A standard st error type...
                                - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_FilterDisassociator(FullHandle_t FilterHandle, FullHandle_t AssocObjectHandle)
{
	/* Already write locked */
	ST_ErrorCode_t Error = ST_NO_ERROR;

	/* Avoid compiler warning */
	FilterHandle = FilterHandle;

	switch (stptiOBJMAN_GetObjectType(AssocObjectHandle)) {
	case OBJECT_TYPE_SLOT:
		/* Nothing to do.  Disabling of the filter is done by slot disassociator since the fields are in the slot. */
		break;

	default:
		/* Allow disassociation, even from invalid types, else we create a clean up problem */
		STPTI_PRINTF_ERROR("Filter disassociating from invalid type %d.",
				   stptiOBJMAN_GetObjectType(AssocObjectHandle));
		break;
	}

	return (Error);
}

/**
   @brief  Deallocate this filter

   This function deallocates the filter.

   @param  FilterHandle         The Handle of the filter.

   @return                      A standard st error type...
                                - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_FilterDeallocator(FullHandle_t FilterHandle)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;

	/* Already write locked */
	stptiHAL_Filter_t *Filter_p = stptiHAL_GetObjectFilter_p(FilterHandle);
	stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(FilterHandle);

	switch (Filter_p->FilterType) {
	case stptiHAL_PES_STREAMID_FILTER:
		stptiOBJMAN_RemoveFromList(&vDevice_p->PESStreamIDFilterHandles, Filter_p->FilterIndex);
		break;

	case stptiHAL_SHORT_FILTER:
	case stptiHAL_LONG_FILTER:
	case stptiHAL_PNMM_FILTER:
	case stptiHAL_SHORT_VNMM_FILTER:
	case stptiHAL_LONG_VNMM_FILTER:
	case stptiHAL_PNMM_VNMM_FILTER:
		/* For h/w filter we also clear the CAMs */
		stptiHAL_FilterCAMSet(vDevice_p, Filter_p->FilterIndex, stptiHAL_LONG_FILTER, NULL, NULL, NULL, FALSE,
				      FALSE);
		stptiOBJMAN_RemoveFromList(&vDevice_p->SectionFilterHandles, Filter_p->FilterIndex);
		break;

	default:
	case stptiHAL_TINY_FILTER:
		break;
	}

	return (Error);
}

/* Object HAL functions ------------------------------------------------------------------------- */

/**
   @brief  Set the section discard mechanism for this filter

   This function overrides the default DiscardOnCRCError (to FALSE) from vdevice.

   NOTE:  If any filter on a slot has DiscardOnCRCError set to false then all filters on that slot
   are treated as having DiscardOnCRCError set to false (i.e. bad sections are not discarded by
   a slot if any filter on that slot wants to see them).

   @param  FilterHandle The Handle of the slot.
   @param  Discard      If FALSE receive sections when bad CRC. Use Force CRC for DVB TOT sections.

   @return                      A standard st error type...
                                - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_FilterSetDiscardOnCRCErr(FullHandle_t FilterHandle, BOOL Discard)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	stptiOBJMAN_WriteLock(FilterHandle, &LocalLockState);
	{
		stptiHAL_Filter_t *Filter_p = stptiHAL_GetObjectFilter_p(FilterHandle);
		Object_t *SlotAssoc_p;
		int index;

		Filter_p->DiscardOnCRCError = Discard;

		stptiOBJMAN_FirstInList(&Filter_p->ObjectHeader.AssociatedObjects, (void *)&SlotAssoc_p, &index);
		while (index >= 0) {
			FullHandle_t ObjectHandle = stptiOBJMAN_ObjectPointerToHandle(SlotAssoc_p);
			if (stptiOBJMAN_GetObjectType(ObjectHandle) == OBJECT_TYPE_SLOT) {
				Error = stptiHAL_SlotSyncDiscardOnCRCError(ObjectHandle);
				if (ST_NO_ERROR == Error) {
					break;
				}
			}
			stptiOBJMAN_NextInList(&Filter_p->ObjectHeader.AssociatedObjects, (void *)&SlotAssoc_p, &index);
		}
	}
	stptiOBJMAN_Unlock(FilterHandle, &LocalLockState);

	return (Error);
}

/**
   @brief  Set the filter Enable for this slot

   This function Enables or Disables this filter.  If it is not associated, it will be
   enabled/disabled when associated.

   @param  FilterHandle           The Handle of the slot.
   @param  Enable               TRUE if to be enabled.

   @return                      A standard st error type...
                                - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_FilterEnable(FullHandle_t FilterHandle, BOOL Enable)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	stptiOBJMAN_WriteLock(FilterHandle, &LocalLockState);
	{
		stptiHAL_Filter_t *Filter_p = stptiHAL_GetObjectFilter_p(FilterHandle);
		Object_t *SlotAssoc_p;
		int index;

		stptiOBJMAN_FirstInList(&Filter_p->ObjectHeader.AssociatedObjects, (void *)&SlotAssoc_p, &index);
		while (index >= 0) {
			FullHandle_t ObjectHandle = stptiOBJMAN_ObjectPointerToHandle(SlotAssoc_p);
			if (stptiOBJMAN_GetObjectType(ObjectHandle) == OBJECT_TYPE_SLOT) {
				Error = stptiHAL_SlotUpdateFilterState(ObjectHandle, FilterHandle, !Enable, Enable);
				if (ST_NO_ERROR == Error) {
					break;
				}
			}
			stptiOBJMAN_NextInList(&Filter_p->ObjectHeader.AssociatedObjects, (void *)&SlotAssoc_p, &index);
		}
		Filter_p->EnableOnAssociation = Enable;
	}
	stptiOBJMAN_Unlock(FilterHandle, &LocalLockState);

	return (Error);
}

/**
   @brief  Set the Filter

   This function set the filter data

   @param  SlotHandle           The Handle of the slot.
   @param  Pid                  The PID to be set (or 0xE000 if clearing it)

   @return                      A standard st error type...
                                - ST_NO_ERROR
 */
ST_ErrorCode_t stptiHAL_FilterUpdate(FullHandle_t FilterHandle, stptiHAL_FilterType_t FilterType, BOOL OneShotMode,
				     BOOL ForceCRCCheck, U8 *FilterData_p, U8 *FilterMask_p, U8 *FilterSense_p)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	stptiOBJMAN_WriteLock(FilterHandle, &LocalLockState);
	{
		stptiHAL_Filter_t *Filter_p = stptiHAL_GetObjectFilter_p(FilterHandle);
		stptiHAL_vDevice_t *vDevice_p = stptiHAL_GetObjectvDevice_p(FilterHandle);

		if (stptiHAL_FilterIsSlotBased(Filter_p->FilterType) && FilterType != Filter_p->FilterType) {
			/* Is is important not to change filter type once it is allocated.  We relax the
			   restriction for CAM Based filters, as some types are not known until FilterSet
			   (for example version not match mode). */
			STPTI_PRINTF_ERROR("You cannot change the filter type once allocated.");
			Error = STPTI_ERROR_INVALID_FILTER_TYPE;
		} else {
			switch (Filter_p->FilterType) {
			case stptiHAL_PES_STREAMID_FILTER:
				/* Set the streamID mask value and if associated the slot info in the TP */
				Filter_p->u.PESStreamIDMask = FilterData_p[0];
				break;

			case stptiHAL_TINY_FILTER:
				Filter_p->OneShotMode = OneShotMode;
				if (FilterData_p == NULL || FilterMask_p == NULL) {
					Filter_p->u.TinyFilter.FilterData = 0xFFFF;
					Filter_p->u.TinyFilter.FilterMask = 0xFFFF;
					Filter_p->u.TinyFilter.FilterAnyMatchMask = 0;
				} else {
					Filter_p->u.TinyFilter.FilterData =
					    (U16) FilterData_p[0] << 8 | (U16) FilterData_p[1];
					Filter_p->u.TinyFilter.FilterMask =
					    (U16) FilterMask_p[0] << 8 | (U16) FilterMask_p[1];
					if (FilterSense_p != NULL) {
						Filter_p->u.TinyFilter.FilterAnyMatchMask =
						    (U16) FilterSense_p[0] << 8 | (U16) FilterSense_p[1];
					} else {
						Filter_p->u.TinyFilter.FilterAnyMatchMask = 0;
					}
				}
				break;

			case stptiHAL_PROPRIETARY_FILTER:
				Error = STPTI_ERROR_INVALID_FILTER_TYPE;
				break;
				break;

			case stptiHAL_SHORT_FILTER:
			case stptiHAL_LONG_FILTER:
			case stptiHAL_PNMM_FILTER:
			case stptiHAL_SHORT_VNMM_FILTER:
			case stptiHAL_LONG_VNMM_FILTER:
			case stptiHAL_PNMM_VNMM_FILTER:
				Filter_p->FilterType = FilterType;	/* Allow changing of filter for these types (e.g. stptiHAL_SHORT_FILTER -> stptiHAL_SHORT_VNMM_FILTER) */
				Filter_p->OneShotMode = OneShotMode;
				Filter_p->ForceCRCCheck = ForceCRCCheck;
				/* Filter setup saved inside the CAM on the TP */
				stptiHAL_FilterCAMSet(vDevice_p, Filter_p->FilterIndex,
						      Filter_p->FilterType, FilterData_p, FilterMask_p, FilterSense_p,
						      OneShotMode, ForceCRCCheck);
				break;

			case stptiHAL_NO_FILTER:
			default:
				Error = ST_ERROR_BAD_PARAMETER;
				break;
			}

			/* Update the Filters for any associate slots */
			if (ST_NO_ERROR == Error) {
				Object_t *AssociatedObject;
				int index;

				stptiOBJMAN_FirstInList(&Filter_p->ObjectHeader.AssociatedObjects,
							(void *)&AssociatedObject, &index);
				while (index >= 0) {
					if (stptiOBJMAN_GetObjectType(AssociatedObject->Handle) == OBJECT_TYPE_SLOT) {
						/* update the filters but leave the enable / disable state as is */
						Error =
						    stptiHAL_SlotUpdateFilterState(AssociatedObject->Handle,
										   FilterHandle, FALSE, FALSE);
					}
					stptiOBJMAN_NextInList(&Filter_p->ObjectHeader.AssociatedObjects,
							       (void *)&AssociatedObject, &index);
				}

				/* Make sure the filter is enabled on filter on association */
				Filter_p->EnableOnAssociation = TRUE;
			}
		}
	}
	stptiOBJMAN_Unlock(FilterHandle, &LocalLockState);

	return (Error);
}

ST_ErrorCode_t stptiHAL_FilterUpdateProprietaryFilter(FullHandle_t FilterHandle, stptiHAL_FilterType_t FilterType,
						      void *FilterData_p, size_t SizeofFilterData)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;
	stptiSupport_AccessLockState_t LocalLockState = stptiSupport_UNLOCKED;

	stptiOBJMAN_WriteLock(FilterHandle, &LocalLockState);
	{
		stptiHAL_Filter_t *Filter_p = stptiHAL_GetObjectFilter_p(FilterHandle);

		if (FilterType != Filter_p->FilterType) {
			STPTI_PRINTF_ERROR("You cannot change the filter type once allocated.");
			Error = STPTI_ERROR_INVALID_FILTER_TYPE;
		} else if (SizeofFilterData != sizeof(Filter_p->u.ProprietaryFilter)) {
			STPTI_PRINTF_ERROR("Invalid proprietary filter size %d (should be %d bytes)", SizeofFilterData,
					   sizeof(Filter_p->u.ProprietaryFilter));
			Error = ST_ERROR_BAD_PARAMETER;
		} else {
			if (Filter_p->FilterType == stptiHAL_PROPRIETARY_FILTER) {
				Object_t *AssociatedObject;
				int index;

				/* Copy EMM filter data across */
				memcpy(&Filter_p->u.ProprietaryFilter, FilterData_p, SizeofFilterData);

				stptiOBJMAN_FirstInList(&Filter_p->ObjectHeader.AssociatedObjects,
							(void *)&AssociatedObject, &index);
				while (index >= 0) {
					if (stptiOBJMAN_GetObjectType(AssociatedObject->Handle) == OBJECT_TYPE_SLOT) {
						/* update the filters but leave the enable / disable state as is */
						Error =
						    stptiHAL_SlotUpdateFilterState(AssociatedObject->Handle,
										   FilterHandle, FALSE, FALSE);
					}
					stptiOBJMAN_NextInList(&Filter_p->ObjectHeader.AssociatedObjects,
							       (void *)&AssociatedObject, &index);
				}

				/* Make sure the filter is enabled on filter on association */
				Filter_p->EnableOnAssociation = TRUE;
			} else {
				Error = STPTI_ERROR_INVALID_FILTER_TYPE;
			}
		}
	}
	stptiOBJMAN_Unlock(FilterHandle, &LocalLockState);

	return (Error);
}

/* Supporting Functions ------------------------------------------------------------------------- */

static BOOL stptiHAL_FilterIsSlotBased(stptiHAL_FilterType_t FilterType)
{
	stptiTP_SectionParams_t SectionParams = stptiHAL_FilterConvertToTPType(FilterType);
	return (SectionParams != SF_MODE_LONG && SectionParams != SF_MODE_PNMM && SectionParams != SF_MODE_SHORT);
}

stptiTP_SectionParams_t stptiHAL_FilterConvertToTPType(stptiHAL_FilterType_t FilterType)
{
	stptiTP_SectionParams_t SectionParams;

	/* convert HAL filter type into TP filter type */
	switch (FilterType) {
	case stptiHAL_TINY_FILTER:
		SectionParams = SF_MODE_TINY;
		break;

	case stptiHAL_SHORT_FILTER:
	case stptiHAL_SHORT_VNMM_FILTER:
	case stptiHAL_LONG_FILTER:
	case stptiHAL_LONG_VNMM_FILTER:
		SectionParams = SF_MODE_LONG;
		break;

	case stptiHAL_PNMM_FILTER:
	case stptiHAL_PNMM_VNMM_FILTER:
		SectionParams = SF_MODE_PNMM;
		break;

	case stptiHAL_PROPRIETARY_FILTER:
		SectionParams = SF_MODE_PROPRIETARY;
		break;

	default:
		STPTI_PRINTF_ERROR("Unknown Filter Type %d", (unsigned int)FilterType);
		/* intentionally drop into case below */

	case stptiHAL_PES_STREAMID_FILTER:
		SectionParams = SF_MODE_NONE;
		break;
	}

	return (SectionParams);
}

/**
   @brief  Parse the SectionFilterMetaData

   This function parses the metadata (match bytes and CRC metadata byte).

   @param  vDevice_p                a pointer to the the vDevice
   @param  LeadingMetadata          an array holding the slot index / match bytes metadata
   @param  CRC_MetaData             the crc valid byte (0 = CRC ok, else CRC bad)
   @param  SectionFilterMetaData_p  a pointer to the MetaData structure to return the results in
   @param  SectionPlusMetadataSize  Size of the section with Metadata
   @param  FlushFiltersArray_p      A pointer to the filter

   MetaData Format is currently...
     LeadingMetadata................... 1byte SLOT_INDEX + 8bytes MATCH_BYTES
     (section would be here)
     TrailingMetadata (CRC_MetaData)... 1byte CRC (0 is CRC ok, else bad)

 */
void stptiHAL_ParseMetaData(stptiHAL_vDevice_t *vDevice_p, U8 *LeadingMetadata, U8 CRC_MetaData,
			    stptiHAL_SectionFilterMetaData_t *SectionFilterMetaData_p, U32 SectionPlusMetadataSize,
			    void *FlushFiltersArray_p)
{
	/* We need to output the Matched filters and the CRC validity into the MetaData structure */
	int i, FilterIndex = 0;
	unsigned int FiltersMatched = 0;
	int SlotIndex = (int)(LeadingMetadata[0]);
	U8 *MatchBytes = &LeadingMetadata[SECTION_SLOT_INDEX_LENGTH];
	U8 MatchByteORed = 0;

	stptiHAL_FiltersFlushStruct_t *FilterFlushStructure_p = FlushFiltersArray_p;

	SectionFilterMetaData_p->CRC_OK = (CRC_MetaData == 0);

	/* Process the matched filters bytes (64bit little endian value each bit corresponding to the filter matched) */
	for (i = 0; i < SECTION_MATCHBYTES_LENGTH; i++) {
		U8 mask, MatchByte;
		MatchByte = MatchBytes[i];
		MatchByteORed |= MatchByte;
		if (FilterFlushStructure_p != NULL) {
			MatchByte &= ~FilterFlushStructure_p->DiscardFiltersMask[i]; /* will be zero if no discard necessary */
		}
		for (mask = 1; mask > 0; mask <<= 1) {
			if (MatchByte & mask) {
				Object_t *FilterObject_p;

				/* We code very defensively here, as it is possible filters get deallocated before this data is read */
				if (FilterIndex > stptiOBJMAN_GetListCapacity(&vDevice_p->SectionFilterHandles)) {
					SectionFilterMetaData_p->FilterHandles[FiltersMatched++] =
					    stptiOBJMAN_NullHandle;
				} else if (NULL ==
					   (FilterObject_p =
					    stptiOBJMAN_GetItemFromList(&vDevice_p->SectionFilterHandles,
									FilterIndex))) {
					SectionFilterMetaData_p->FilterHandles[FiltersMatched++] =
					    stptiOBJMAN_NullHandle;
				} else {
					SectionFilterMetaData_p->FilterHandles[FiltersMatched++] =
					    stptiOBJMAN_ObjectPointerToHandle(FilterObject_p);
				}
			}
			FilterIndex++;
		}
	}

	if (MatchByteORed == 0) {
		/* If all the MatchBytes are zero it means that we matched a slot based filter (such as a TINY filter) */

		Object_t *SlotObject_p;

		/* See if it has been "flushed" */
		if (!FilterFlushStructure_p->DiscardSlotBasedFilter) {
			/* We code very defensively here, as it is possible filters get deallocated before this data is read */
			if (SlotIndex > stptiOBJMAN_GetNumberOfItemsInList(vDevice_p->SlotHandles)) {
				SectionFilterMetaData_p->FilterHandles[FiltersMatched++] = stptiOBJMAN_NullHandle;
			} else if (NULL ==
				   (SlotObject_p = stptiOBJMAN_GetItemFromList(vDevice_p->SlotHandles, SlotIndex))) {
				SectionFilterMetaData_p->FilterHandles[FiltersMatched++] = stptiOBJMAN_NullHandle;
			} else {
				/* Now we have the Slot_p we lookup the Filter (there can only be 1 filter associated for this type of filter) */
				FullHandle_t FilterHandle;
				int count =
				    stptiOBJMAN_ReturnAssociatedObjects(stptiOBJMAN_ObjectPointerToHandle(SlotObject_p),
									&FilterHandle, 1,
									OBJECT_TYPE_FILTER);
				if (count == 1) {
					stptiHAL_Filter_t *Filter_p = stptiHAL_GetObjectFilter_p(FilterHandle);

					/* just check it is a Slot Based filter */
					if (stptiHAL_FilterIsSlotBased(Filter_p->FilterType)) {
						SectionFilterMetaData_p->FilterHandles[FiltersMatched++] = FilterHandle;
					}
				}
			}
		}
	}

	SectionFilterMetaData_p->FiltersMatched = FiltersMatched;

	/* If flush filter structure "active"? */
	if (FilterFlushStructure_p->DiscardRegionSize > 0) {
		/* If check to see if we are no outside the region for flushing */
		if (FilterFlushStructure_p->DiscardRegionSize <= SectionPlusMetadataSize) {
			/* Finished FlushFiltersStructure (move the structures down, and clear the last one) */
			memmove(FilterFlushStructure_p, FilterFlushStructure_p + 1,
				(NUMBER_OF_FILTERFLUSH_STRUCTURES - 1) * sizeof(stptiHAL_FiltersFlushStruct_t));
			memset(FilterFlushStructure_p + (NUMBER_OF_FILTERFLUSH_STRUCTURES - 1), 0,
			       sizeof(stptiHAL_FiltersFlushStruct_t));
		} else {
			FilterFlushStructure_p->DiscardRegionSize -= SectionPlusMetadataSize;
		}
	}
}

/**
   @brief  Allocates a filters flush structure array to suppress matched filter reporting.

   This function allocates memory for the FilterFlush mechanism.

   This is a requirement of FiltersFlush, a feature of the PTI where where we purge the existance
   of a filter from a collection of sections in a buffer.  Ideally this would be done by removing
   the sections from the buffer, but they could be interleaved with other "wanted" sections.
   Another problem is that caching issues prevent us from writing to the buffer at the same time as
   the h/w (firmware) writes to the buffer (due to cache line flushing creating a race condition).

   The easiest solution is to filter them out during the read stage (stptiHAL_ParseMetaData).  The
   structure is removed in stptiHAL_ParseMetaData when it is no longer need.

   @param  SessionHandle            A handle used to get the session (can also be an object handle)
   @param  FiltersFlushArray_pp     a pointer to the field in the buffer which is used to store the
                                    pointer for the flush filters array.  It is an array to be able
                                    to handle multiple successive flushes on the same buffer.

 */
ST_ErrorCode_t stptiHAL_FiltersFlushArrayAllocate(FullHandle_t SessionHandle, void **FiltersFlushArray_pp)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;

	void *p;
	p = kmalloc(NUMBER_OF_FILTERFLUSH_STRUCTURES * sizeof(stptiHAL_FiltersFlushStruct_t), GFP_KERNEL);

	if (p == NULL) {
		Error = ST_ERROR_NO_MEMORY;
	} else {
		stptiHAL_FiltersFlushArrayPurge(p);
		*FiltersFlushArray_pp = p;
	}

	return (Error);
}

/**
   @brief  Add a filters flush structure to suppress matched filter reporting.

   This function populates a structure (from a finite array of structures) for indicating which
   filters are to be ignored in the stptiHAL_ParseMetaData function above.

   @param  FiltersFlushArray_p      a pointer to the field in the buffer which is used to store the
                                    pointer for the flush filters array.  It is an array to be able
                                    to handle multiple successive flushes on the same buffer.
   @param  DiscardRegionSize        The total number of bytes of sections (including metadata)
                                    affected (usually the total amount of data in the buffer).
   @param  FilterHandles            An array of filter handles to "flush"
   @param  NumberOfHandles          Number of Handles in the FilterHandles array.

 */
ST_ErrorCode_t stptiHAL_FiltersFlushArrayAddStructure(void *FiltersFlushArray_p, U32 DiscardRegionSize,
						      FullHandle_t *FilterHandles, int NumberOfHandles)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;

	int i, NewStructureIndex = -1;
	U8 DiscardFiltersMask[SECTION_MATCHBYTES_LENGTH] = { 0 };
	BOOL DiscardingSlotBasedFilter = FALSE;

	stptiHAL_FiltersFlushStruct_t *FilterFlushArray = FiltersFlushArray_p;

	/* Create discard filter mask (filters to discard) */
	for (i = 0; i < NumberOfHandles; i++) {
		stptiHAL_Filter_t *Filter_p = stptiHAL_GetObjectFilter_p(FilterHandles[i]);
		if (Filter_p != NULL) {
			if (stptiHAL_FilterIsSlotBased(Filter_p->FilterType)) {
				/* Ideally we should check to see if the filter is or has been associated to the
				   buffer we are flushing.  Unfortunately this is not possible.  So we rely on
				   the user specifying filters that have or are associated (else they will flush
				   more sections than they expect). */
				DiscardingSlotBasedFilter = TRUE;
			} else {
				DiscardFiltersMask[Filter_p->FilterIndex / 8] |= 1 << (Filter_p->FilterIndex % 8);
			}
		}
	}

	/* Find a spare FiltersFlush Structure from the pool. */
	for (NewStructureIndex = 0; NewStructureIndex < NUMBER_OF_FILTERFLUSH_STRUCTURES; NewStructureIndex++) {
		if (FilterFlushArray[NewStructureIndex].DiscardRegionSize == 0) {
			break;
		} else {
			/* Whilst traversing the list, we make sure that the new filters specified are purged too
			   from any pending filter flushes */
			for (i = 0; i < SECTION_MATCHBYTES_LENGTH; i++) {
				FilterFlushArray[NewStructureIndex].DiscardFiltersMask[i] |= DiscardFiltersMask[i];
			}

			if (DiscardingSlotBasedFilter) {
				FilterFlushArray[NewStructureIndex].DiscardSlotBasedFilter = TRUE;
			}

			if (DiscardRegionSize >= FilterFlushArray[NewStructureIndex].DiscardRegionSize) {
				/* reduce the DiscardRegionSize by the structure already existing */
				DiscardRegionSize -= FilterFlushArray[NewStructureIndex].DiscardRegionSize;
			} else {
				/* This shouldn't happen and means data is being removed from the buffer without
				   using stptiHAL_ParseMetaData (BufferRead Section). */
				STPTI_PRINTF_ERROR("DiscardRegionSize is smaller than the sections to be flushed?");
				DiscardRegionSize = 0;
			}
		}
	}
	if (NewStructureIndex >= NUMBER_OF_FILTERFLUSH_STRUCTURES) {
		/* Not enough space in FilterFlushArray */
		STPTI_PRINTF_ERROR("Too many successive filter flushes without reading the sections.");
		Error = STPTI_ERROR_FLUSH_FILTERS_NOT_SUPPORTED;
	}

	/* Check that DiscardRegionSize hasn't been eroded away to 0.  There is no need to add a structure
	   if it is 0. */
	if (ST_NO_ERROR == Error && DiscardRegionSize > 0) {
		/* Populate new structure (and mark it as the tail) */
		for (i = 0; i < SECTION_MATCHBYTES_LENGTH; i++) {
			FilterFlushArray[NewStructureIndex].DiscardFiltersMask[i] |= DiscardFiltersMask[i];
		}

		if (DiscardingSlotBasedFilter) {
			FilterFlushArray[NewStructureIndex].DiscardSlotBasedFilter = TRUE;
		}

		FilterFlushArray[NewStructureIndex].DiscardRegionSize = DiscardRegionSize;
	}

	return (Error);
}

/**
   @brief  Clears filters flush structure array.

   This function sets to zero the entire filters flush array.  It is important this array is
   properly initialised as data is moved up when each structure is finished.

   @param  FiltersFlushArray_p     The pointer for the flush filters array.

 */
ST_ErrorCode_t stptiHAL_FiltersFlushArrayPurge(void *FiltersFlushArray_p)
{
	memset(FiltersFlushArray_p, 0, NUMBER_OF_FILTERFLUSH_STRUCTURES * sizeof(stptiHAL_FiltersFlushStruct_t));
	return (ST_NO_ERROR);
}

/**
   @brief  Deallocates a filters flush structure array.

   This function deallocates memory for the FilterFlush mechanism (called when deallocating a
   buffer).

   @param  SessionHandle            A handle used to get the session (can also be an object handle)
   @param  FiltersFlushArray_p      The pointer for the flush filters array.

 */
ST_ErrorCode_t stptiHAL_FiltersFlushArrayDeallocate(FullHandle_t SessionHandle, void *FiltersFlushArray_p)
{
	kfree(FiltersFlushArray_p);
	return (ST_NO_ERROR);
}

/**
   @brief  Filters flush structure array size.

   This function returns the number of bytes used for the filter flush structures.

   @param  FiltersFlushArray_p      The pointer for the flush filters array.

   @return                          The size (in terms of bytes)
 */
int stptiHAL_FiltersFlushArraySize(void)
{
	return (NUMBER_OF_FILTERFLUSH_STRUCTURES * sizeof(stptiHAL_FiltersFlushStruct_t));
}

/**
   @brief  Set a CAM entry in the Transport Processor

   This function sets a specified section filter CAM entry for a specified filter

   @param  CAMRegion_p        Pointer to the CAM base address.
   @param  FilterCAMTables_p  Pointer to the CAM table address.
   @param  Filter             The filter to be set
   @param  FilterType         The filter type (can only be set once, thereafter must be same type)
   @param  FilterData_p       The filter value bytes
   @param  FilterMask_p       The filter mask
   @param  FilterSense_p      The filter sense (Must Match Bit=0, Must Not Match Bit=1)

   @return                    A standard st error type...
                              - ST_NO_ERROR
 */
void stptiHAL_FilterCAMSet(stptiHAL_vDevice_t *vDevice_p, int Filter, stptiHAL_FilterType_t FilterType,
			   U8 *FilterData_p, U8 *FilterMask_p, U8 *FilterSense_p, BOOL OneShotMode,
			   BOOL ForceCRCCheck)
{
	ST_ErrorCode_t Error = ST_NO_ERROR;

	U64 FilterDataA = 0, FilterDataB = 0, FilterMaskA = 0, FilterMaskB = 0, NotMatchSensitiveFilterBits = 0;
	U64 nmc = 3;		/* invalid mode */
	U8 nmv = 0xFF;		/* invalid value */

	int i;

	static U8 DummyArray[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };	/* 16x 0xFF */

	/* Allow for NULL parameters (to be used for SF initialisation?) */
	if (FilterData_p == NULL) {
		FilterData_p = DummyArray;
	}

	if (FilterMask_p == NULL) {
		FilterMask_p = DummyArray;
	}

	if (FilterSense_p == NULL) {
		FilterSense_p = DummyArray;
	}

	/* At the moment the FilterFeature is used to only control NotMatch Mode */
	switch (FilterType) {
	case stptiHAL_SHORT_VNMM_FILTER:
	case stptiHAL_LONG_VNMM_FILTER:
	case stptiHAL_PNMM_VNMM_FILTER:
		if (((FilterMask_p[3] >> 1) & 0x1F) != 0) {
			nmc = 1;
			nmv = (FilterData_p[3] >> 1) & 0x1F;
			/* Need to mask off those bits so that they are sensitive to the not_match logic rather than the standard filter logic. */
			NotMatchSensitiveFilterBits = 0x1F << (3 * 8 + 1);
		} else {
			/* version bits in the section are masked out by the filter mask so disable version not match mode */
			nmc = 0;
			nmv = 0;
		}
		break;

	default:
		nmc = 0;
		nmv = 0;
		break;
	}

	switch (FilterType) {
	case stptiHAL_SHORT_VNMM_FILTER:
	case stptiHAL_SHORT_FILTER:
		for (i = 7; i >= 0; i--) {
			FilterDataA <<= 8;
			FilterDataA |= FilterData_p[i];
			FilterMaskA <<= 8;
			FilterMaskA |= FilterMask_p[i];
		}
		FilterDataB = 0x0000000000000000LL;
		FilterMaskB = 0x0000000000000000LL;
		FilterMaskA &= ~NotMatchSensitiveFilterBits;	/* Mask off any bits to be handled by NotMatch logic */
		break;

	case stptiHAL_LONG_VNMM_FILTER:
	case stptiHAL_LONG_FILTER:
		for (i = 7; i >= 0; i--) {
			FilterDataA <<= 8;
			FilterDataA |= FilterData_p[i];
			FilterDataB <<= 8;
			FilterDataB |= FilterData_p[i + 8];
			FilterMaskA <<= 8;
			FilterMaskA |= FilterMask_p[i];
			FilterMaskB <<= 8;
			FilterMaskB |= FilterMask_p[i + 8];
		}
		FilterMaskA &= ~NotMatchSensitiveFilterBits;	/* Mask off any bits to be handled by NotMatch logic */
		break;

	case stptiHAL_PNMM_FILTER:
	case stptiHAL_PNMM_VNMM_FILTER:
		for (i = 7; i >= 0; i--) {
			FilterDataA <<= 8;
			FilterDataA |= FilterData_p[i];
			FilterMaskA <<= 8;
			FilterMaskA |= FilterMask_p[i] & FilterSense_p[i];
			FilterMaskB <<= 8;
			FilterMaskB |= FilterMask_p[i] & ~FilterSense_p[i];
		}
		FilterMaskA &= ~NotMatchSensitiveFilterBits;	/* Mask off any bits to be handled by NotMatch logic */
		FilterMaskB &= ~NotMatchSensitiveFilterBits;	/* Mask off any bits to be handled by NotMatch logic */
		FilterDataB = FilterDataA;

		/* If CAM B (the negative match filter) ends up with a mask of zero it will always match.
		   As it works in a negative sense (the section is only passed if it does NOT match) it
		   makes the filter useless.  So in this case we must make sure the filter never matches
		   - i.e. disable it so that the filter behaves like a traditional positive match filter */
		if (FilterMaskB == 0) {
			/* User expects that a masked off negative match filter will be disabled (so we make sure it always "not matches") */
			FilterDataB = 0xffffffffffffffffLL;
			FilterMaskB = 0xffffffffffffffffLL;
		}
		break;

	default:
		STPTI_PRINTF_ERROR("Unknown Filter Type %d", (unsigned int)FilterType);
		Error = ST_ERROR_BAD_PARAMETER;
		break;
	}

	/* Okay now we just need to fill CAMs with our prepared values */
	if (ST_NO_ERROR == Error) {
		volatile U64 *CAMRegion_p = vDevice_p->TP_Interface.FilterCAMRegion_p;
		volatile stptiTP_CamTable_t *FilterCAMTable_p = vDevice_p->TP_Interface.FilterCAMTables_p;
		int FilterCamSize = (int)vDevice_p->TP_Interface.SizeOfCAM;
		int FilterCAMOffset = Filter;	/* Offset into the CAM for the first filter has already been applied to CAMRegion_p */

		U64 FilterBitMask = 1LL << Filter, OneShotMask = 0, ForceCRCCheckMask = 0, PnmmNotLmmMask = 0;
		int NMC_BitOffset, NMC_Index;

		NMC_BitOffset = Filter * 2;	/* Two NMC bits per Filter */
		NMC_Index = NMC_BitOffset / 64;	/* FilterCAMTable_p->NotMatchControlA is a U64 */
		NMC_BitOffset = NMC_BitOffset % 64;

		stptiHAL_pDeviceXP70ReadModifyWrite(&(FilterCAMTable_p->NotMatchControlA[NMC_Index]),
						    3LL << NMC_BitOffset, nmc << NMC_BitOffset);
		stptiHAL_pDeviceXP70Write(&FilterCAMTable_p->NotMatchValueA[Filter], nmv);

		stptiHAL_pDeviceXP70Write(&(CAMRegion_p[FilterCAMOffset]), FilterDataA);
		FilterCAMOffset += FilterCamSize;
		stptiHAL_pDeviceXP70Write(&(CAMRegion_p[FilterCAMOffset]), FilterMaskA);
		FilterCAMOffset += FilterCamSize;
		stptiHAL_pDeviceXP70Write(&(CAMRegion_p[FilterCAMOffset]), FilterDataB);
		FilterCAMOffset += FilterCamSize;
		stptiHAL_pDeviceXP70Write(&(CAMRegion_p[FilterCAMOffset]), FilterMaskB);

		/* We need to update the filter type depending on whether its a LONG or PNMM CAM operation */
		if (SF_MODE_PNMM == stptiHAL_FilterConvertToTPType(FilterType)) {
			PnmmNotLmmMask = FilterBitMask;
		}
		stptiHAL_pDeviceXP70ReadModifyWrite(&FilterCAMTable_p->PnmmNotLmmMask, FilterBitMask, PnmmNotLmmMask);

		if (OneShotMode) {
			OneShotMask = FilterBitMask;
		}
		stptiHAL_pDeviceXP70ReadModifyWrite(&FilterCAMTable_p->OneShotFilterMask, FilterBitMask, OneShotMask);

		if (ForceCRCCheck) {
			ForceCRCCheckMask = FilterBitMask;
		}
		stptiHAL_pDeviceXP70ReadModifyWrite(&FilterCAMTable_p->ForceCRCFilterMask, FilterBitMask,
						    ForceCRCCheckMask);
	}
}

/**
   @brief  Initialise a CAM entry in the Transport Processor

   This function initialises the filter CAM table, and the CAMs themselves.

   @param  vDevice_p          Pointer to the vDevice.
   @param  FilterCAMOffset    Offset in the CAM of the first filter
   @param  NumberOfFilters    The number of filters to be initialised (0 if none).

   @return                    A standard st error type...
                              - ST_NO_ERROR
 */
void stptiHAL_FilterTPEntryInitialise(stptiHAL_vDevice_t *vDevice_p, U32 FilterCAMOffset, int NumberOfFilters)
{
	volatile stptiTP_CamTable_t *FilterCAMTable_p = vDevice_p->TP_Interface.FilterCAMTables_p;

	stptiHAL_pDeviceXP70Memset((void *)FilterCAMTable_p, 0, sizeof(stptiTP_CamTable_t));
	FilterCAMTable_p->FilterCAMOffset = FilterCAMOffset;

	if (NumberOfFilters > 0 && FilterCAMOffset < 0xffffffff) {
		volatile U64 *p = vDevice_p->TP_Interface.FilterCAMRegion_p;
		stptiHAL_pDeviceXP70Memset((void *)p, 0xff, NumberOfFilters * sizeof(U64));	/* CAMA data */
		p += vDevice_p->TP_Interface.SizeOfCAM;
		stptiHAL_pDeviceXP70Memset((void *)p, 0xff, NumberOfFilters * sizeof(U64));	/* CAMA mask */
		p += vDevice_p->TP_Interface.SizeOfCAM;
		stptiHAL_pDeviceXP70Memset((void *)p, 0xff, NumberOfFilters * sizeof(U64));	/* CAMB data */
		p += vDevice_p->TP_Interface.SizeOfCAM;
		stptiHAL_pDeviceXP70Memset((void *)p, 0xff, NumberOfFilters * sizeof(U64));	/* CAMB mask */
	}
}
