/************************************************************************
Copyright (C) 2011 STMicroelectronics. All Rights Reserved.

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

Source file name : stv0367qam_drv.h
Author :           LLA

stv0367 lla header file

Date        Modification                                    Name
----        ------------                                    --------
30-Jul-11   Created                                         Ankur
04-Jul-12   Updated to v1.9
************************************************************************/
#ifndef _STV0367QAM_DRV_H
#define _STV0367QAM_DRV_H

#include "stv0367qam_init.h"
#include "stv0367qam_util.h"
#include <fe_tc_tuner.h>
#include <fecab_commlla_str.h>

/****************************************************************
	367QAM Specific typedefs
****************************************************************/
typedef InternalParamsPtr FE_367qam_Handle_t;/*Handle to the FE */

/* Signal type enum */
typedef enum {
	FE_367qam_NOTUNER,
	FE_367qam_NOAGC,
	FE_367qam_NOSIGNAL,
	FE_367qam_NOTIMING,
	FE_367qam_TIMINGOK,
	FE_367qam_NOCARRIER,
	FE_367qam_CARRIEROK,
	FE_367qam_NOBLIND,
	FE_367qam_BLINDOK,
	FE_367qam_NODEMOD,
	FE_367qam_DEMODOK,
	FE_367qam_DATAOK
} FE_367qam_SIGNALTYPE_t;

/****************************************************************
		INTERNAL STRUCTURES
****************************************************************/
typedef struct {
	STCHIP_Handle_t hDemod;/* Handle to the demod */
	STCHIP_Handle_t hTuner;/* Handle to the tuner */
	STCHIP_Handle_t hTuner2;/* Handle to the tuner */

	FE_367qam_SIGNALTYPE_t State;

	S32 Crystal_Hz,/* Crystal frequency (Hz) */
	 IF_Freq_kHz, Frequency_kHz,/* Current tuner frequency (KHz) */
	 SymbolRate_Bds,/* Symbol rate (Bds) */
	 MasterClock_Hz,/* Master clock frequency (Hz) */
	 AdcClock_Hz,/* ADC clock frequency (Hz) */
	 SearchRange_Hz,/* Search range (Hz) */
	 DerotOffset_Hz;/* Derotator offset during software zigzag (Hz) */
	U32 FirstTimeBER;
	FE_CAB_Modulation_t Modulation;/* QAM Size */
	FE_367qam_Monitor Monitor_results;/* Monitorting counters */
	FE_CAB_SearchResult_t DemodResult;/* Search results */
} FE_367qam_InternalParams_t;

/****************************************************************
		API FUNCTIONS
****************************************************************/
ST_Revision_t FE_367qam_GetLLARevision(void);
#ifdef CHIP_STAPI
#ifndef STFRONTEND_FORCE_STI2C_DEPRECATED
STCHIP_Error_t FE_367qam_Repeater(STCHIP_Handle_t hChip, BOOL State,
				  unsigned char *Buffer);
#else
STCHIP_Error_t FE_367qam_Repeater(STCHIP_Handle_t hChip, BOOL State);
#endif
#else
STCHIP_Error_t FE_367qam_Repeater(STCHIP_Handle_t hChip, BOOL State);
#endif
FE_LLA_Error_t FE_367qam_Init(FE_CAB_InitParams_t *pInit,
			      FE_367qam_Handle_t *Handle);

FE_LLA_Error_t FE_367qam_Search(FE_367qam_Handle_t Handle,
				FE_CAB_SearchParams_t *pParams,
				FE_CAB_SearchResult_t *pResult);

FE_LLA_Error_t FE_367qam_SuccessRate(FE_367qam_Handle_t Handle,
				     FE_CAB_SearchParams_t *pSearch,
				     S32 RFOffset_kHz);
BOOL FE_367qam_Status(FE_367qam_Handle_t Handle);

FE_LLA_Error_t FE_367qam_GetSignalInfo(FE_367qam_Handle_t Handle,
				       FE_CAB_SignalInfo_t *pInfo);

FE_LLA_Error_t FE_367qam_Term(FE_367qam_Handle_t Handle);
FE_LLA_Error_t FE_367qam_SetStandby(FE_367qam_Handle_t Handle, U8 StandbyOn);
void FE_367qam_SetAllPasscoefficient(STCHIP_Handle_t hChip, U32 MasterClk_Hz,
				     U32 SymbolRate);
FE_LLA_Error_t FE_367qam_SetAbortFlag(FE_367qam_Handle_t Handle, BOOL Abort);

#endif/* _STV0367QAM_DRV_H */
