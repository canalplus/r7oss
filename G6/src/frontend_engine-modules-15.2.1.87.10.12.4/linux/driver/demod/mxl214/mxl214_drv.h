/************************************************************************
Copyright (C) 2014 STMicroelectronics. All Rights Reserved.

This file is part of the stm_fe Library.

stm_fe is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

stm_fe is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with stm_fe; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The stm_fe Library may alternatively be licensed under a proprietary
license from ST.

Source file name :mxl214_drv.h
Author :

Header for low level driver of mxl214_drv

Date        Modification                                    Name
----        ------------                                    --------
30-June-14   Created						MS

************************************************************************/
#ifndef _MXL214_DRV_H
#define _MXL214_DRV_H

#include <mxl214_init.h>
#include <stfe_utilities.h>
#include "fecab_commlla_str.h"
#include "maxlineardatatypes.h"
#include "mxl_hrcls_demodapi.h"

#define MXL214_HRCLS_NBREGS 2500
#define MXL214_HRCLS_NBFIELDS 7800

typedef InternalParamsPtr FE_MXL214_Handle_t; /*Handle to the FE */
typedef FE_LLA_Error_t FE_MXL214_Error_t;
typedef U8	FE_MXL214_DEMOD_t; /*Current Demod*/

typedef FE_TS_Config_t FE_MXL214_TSConfig_t;
/****************************************************************
	SEARCH STRUCTURES
****************************************************************/
typedef FE_CAB_SearchParams_t FE_MXL214_SearchParams_t;
typedef FE_CAB_SearchResult_t FE_MXL214_SearchResult_t;
/************************
	INFO STRUCTURE
************************/
typedef FE_CAB_SignalInfo_t FE_MXL214_SignalInfo_t;

/****************************************************************
					INTERNAL PARAMS STRUCTURES
 ****************************************************************/
typedef struct {
	STCHIP_Handle_t hDemod;	/*Handle to the chip*/
	MXL_STATUS_E status;
	S32 Crystal_Hz,/* Crystal frequency (Hz) */
	 IF_Freq_kHz, /*IF in Khz*/
	 Frequency_kHz,/* Current tuner frequency (KHz) */
	 SymbolRate_Bds,/* Symbol rate (Bds) */
	 MasterClock_Hz,/* Master clock frequency (Hz) */
	 AdcClock_Hz,/* ADC clock frequency (Hz) */
	 SearchRange_Hz;/* Search range (Hz) */
	MXL_HRCLS_QAM_TYPE_E Modulation;/* QAM Size */
	MXL_HRCLS_ANNEX_TYPE_E Annex;
	FE_CAB_SearchResult_t DemodResult;/* Search results */

} FE_MXL214_InternalParams_t;

FE_MXL214_Error_t FE_MXL214_Init(FE_CAB_InitParams_t	*pInit,
				 FE_MXL214_Handle_t *Handle);

FE_MXL214_Error_t FE_MXL214_SetStandby(FE_MXL214_Handle_t Handle,
					U8 StandbyOn, FE_MXL214_DEMOD_t Demod);
FE_MXL214_Error_t FE_MXL214_SetAbortFlag(FE_MXL214_Handle_t Handle,
					BOOL Abort, FE_MXL214_DEMOD_t Demod);
FE_MXL214_Error_t FE_MXL214_Unlock(FE_MXL214_Handle_t Handle,
					FE_MXL214_DEMOD_t Demod);
BOOL FE_MXL214_Status(FE_MXL214_Handle_t Handle, FE_MXL214_DEMOD_t Demod);
FE_MXL214_Error_t FE_MXL214_Term(FE_MXL214_Handle_t Handle);
FE_MXL214_Error_t FE_MXL214_GetSignalInfo(FE_MXL214_Handle_t Handle,
					  FE_MXL214_DEMOD_t Demod,
					  FE_CAB_SignalInfo_t *pInfo);
FE_MXL214_Error_t	FE_MXL214_Search(FE_MXL214_Handle_t Handle,
					FE_MXL214_DEMOD_t Demod,
					FE_MXL214_SearchParams_t *sp,
					FE_MXL214_SearchResult_t *rp);





#endif /* _MXL214_DRV_H */
