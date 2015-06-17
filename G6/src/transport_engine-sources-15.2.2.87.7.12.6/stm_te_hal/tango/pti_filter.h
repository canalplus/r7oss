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
   @file   pti_filter.h
   @brief  Filter Object Initialisation, Termination and Manipulation Functions.

   This file implements the functions for creating, destroying and accessing PTI filters.  This
   includes SECTION filters, PES filters and a various Conditional Access Filters.

   This file should ONLY contain function prototypes and constants that are
   INTERNAL to the stptiHAL.  i.e. with the exception of the HAL object
   registration function in pti_driver.c, NOTHING outside of the tango
   directory should be referring to this file.

   Defines and enums relevent to the API of the HAL must be put into
   pti_hal_api.h and will be shared across all HALs.


 */

#ifndef _PTI_FILTER_H_
#define _PTI_FILTER_H_

/* Includes ---------------------------------------------------------------- */

/* ANSI C includes */

/* STAPI includes */

/* Includes from API level */
#include "../pti_hal_api.h"

/* Includes from the HAL / ObjMan level */
#include "../objman/pti_object.h"
#include "firmware/pti_tp_api.h"

/* Exported Types ---------------------------------------------------------- */

typedef struct {
	Object_t ObjectHeader;			/* Must be first in structure */
	stptiHAL_FilterType_t FilterType;	/**< Filter mode (LONG, PNMM) */
	int FilterIndex;			/**< The index of this filter in the CAMs or just memory array for PES streamID */
	BOOL EnableOnAssociation;
	BOOL FilterEnabled;
	BOOL DiscardOnCRCError;			/**< Discard sections if there is a CRC error */
	BOOL OneShotMode;			/**< If TRUE the filter will only match once */
	BOOL ForceCRCCheck;			/**< If TRUE the CRC will be performed when SSI==0 */
	union {
		U8 PESStreamIDMask;		/**< PES StreamID Filter Mask */
		struct {
			U16 FilterData;
			U16 FilterMask;
			U16 FilterAnyMatchMask;
		} TinyFilter;
		stptiTP_ProprietaryFilter_t ProprietaryFilter;
	} u;
} stptiHAL_Filter_t;

/* Exported Function Prototypes -------------------------------------------- */
extern const stptiHAL_FilterAPI_t stptiHAL_FilterAPI;

void stptiHAL_ParseMetaData(stptiHAL_vDevice_t *vDevice_p, U8 *MatchBytes, U8 CRC_MetaData,
			    stptiHAL_SectionFilterMetaData_t *SectionFilterMetaData_p, U32 SectionPlusMetadataSize,
			    void *FlushFiltersArray_p);

stptiTP_SectionParams_t stptiHAL_FilterConvertToTPType(stptiHAL_FilterType_t FilterType);

ST_ErrorCode_t stptiHAL_FiltersFlushArrayAllocate(FullHandle_t SessionHandle, void **FiltersFlushArray_pp);
ST_ErrorCode_t stptiHAL_FiltersFlushArrayAddStructure(void *FiltersFlushArray_p, U32 DiscardRegionSize,
						      FullHandle_t *FilterHandles, int NumberOfHandles);
ST_ErrorCode_t stptiHAL_FiltersFlushArrayPurge(void *FiltersFlushArray_p);
int stptiHAL_FiltersFlushArraySize(void);
ST_ErrorCode_t stptiHAL_FiltersFlushArrayDeallocate(FullHandle_t SessionHandle, void *FiltersFlushArray_p);

void stptiHAL_FilterTPEntryInitialise(stptiHAL_vDevice_t *vDevice_p, U32 FilterCAMOffset, int NumberOfFilters);

#define stptiHAL_GetObjectFilter_p(ObjectHandle)   ((stptiHAL_Filter_t *)stptiOBJMAN_HandleToObjectPointer(ObjectHandle,OBJECT_TYPE_FILTER))

#endif /* _PTI_FILTER_H_ */
