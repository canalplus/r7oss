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

Source file name : stv0903_drv.h
Author :           LLA

stv0903 lla header file

Date        Modification                                    Name
----        ------------                                    --------
20-Jun-11   Created

************************************************************************/
#ifndef _STV0903_DRV_H
#define _STV0903_DRV_H

/*#include "fesat_commlla_str.h"*/
/*#include "stv0903_init.h"*/
/*#include <fe_sat_tuner.h>*/
/*#include <stfe_utilities.h>*/

/****************************************************************
		COMMON STRUCTURES AND TYPEDEF
****************************************************************/

typedef InternalParamsPtr FE_STV0903_Handle_t;	/*Handle to the FE */

typedef FE_LLA_Error_t FE_STV0903_Error_t;	/*Error Type */

typedef FE_LLA_LOOKPOINT_t FE_STV0903_LOOKPOINT_t;

typedef FE_LLA_LOOKUP_t FE_STV0903_LOOKUP_t;

/*Current demod search state */
typedef FE_Sat_SEARCHSTATE_t FE_STV0903_SEARCHSTATE_t;

typedef FE_Sat_SIGNALTYPE_t FE_STV0903_SIGNALTYPE_t;

typedef FE_Sat_DemodPath_t FE_STV0903_DEMOD_t;	/*Current Demod */

typedef FE_Sat_Tuner_t FE_STV0903_Tuner_t;

typedef FE_Sat_TrackingStandard_t FE_STV0903_TrackingStandard_t;

typedef FE_Sat_SearchStandard_t FE_STV0903_SearchStandard_t;

typedef FE_Sat_SearchAlgo_t FE_STV0903_SearchAlgo_t;

typedef FE_Sat_Modulation_t FE_STV0903_Modulation_t;

typedef FE_Sat_ModCod_t FE_STV0903_ModCod_t;	/*ModCod */

typedef FE_Sat_FRAME_t FE_STV0903_FRAME_t;	/*Frame Type */

typedef FE_Sat_PILOTS_t FE_STV0903_PILOTS_t;

typedef FE_Sat_Rate_t FE_STV0903_Rate_t;

typedef FE_Sat_RollOff_t FE_STV0903_RollOff_t;

typedef FE_Sat_Search_IQ_Inv_t FE_STV0903_Search_IQ_Inv_t;

typedef FE_Sat_IQ_Inversion_t FE_STV0903_IQ_Inversion_t;

typedef FE_TS_OutputMode_t FE_STV0903_Clock_t;

typedef FE_TS_Config_t FE_STV0903_TSConfig_t;

typedef FE_Sat_DiseqC_TxMode FE_STV0903_DiseqC_TxMode;

/****************************************************************
			INIT STRUCTURES
structure passed to the FE_STV0900_Init() function
****************************************************************/

typedef FE_Sat_InitParams_t FE_STV0903_InitParams_t;

/****************************************************************
			SEARCH STRUCTURES
****************************************************************/

typedef FE_Sat_SearchParams_t FE_STV0903_SearchParams_t;

typedef FE_Sat_SearchResult_t FE_STV0903_SearchResult_t;

/************************
	INFO STRUCTURE
************************/
typedef FE_Sat_SignalInfo_t FE_STV0903_SignalInfo_t;

/****************************************************************
		INTERNAL PARAMS STRUCTURES
****************************************************************/
typedef struct {

	/*      Handle to the chip              */
	 /*HAL*/ STCHIP_Handle_t hDemod;

	TUNER_Handle_t hTuner;	/*      Handle to the tuner 1   */

/*Clocks and quartz General Params */
	S32 Quartz,		/*      Demod Reference frequency (Hz) */
	 MasterClock;		/*      Master clock frequency (Hz) */

	/*      manual RollOff for DVBS1/DSS only */
	FE_STV0903_RollOff_t RollOff;

/*Demod 1 */
	/* Tuner type STB6000,STB6100, STV6110 or SW_TUNER (soft control)  */
	FE_STV0903_Tuner_t TunerType;

	/* Global I,Q inversion I,Q connection from tuner to demod */
	FE_STV0903_IQ_Inversion_t TunerGlobal_IQ_Inv;

	S32 TunerFrequency,	/* Current tuner frequency (KHz) */
	 TunerOffset,		/* Tuner offset relative to the carrier (Hz) */
	 TunerBW,		/* Current bandwidth of the tuner (Hz) */
	 DemodSymbolRate,	/* Symbol rate (Bds) */
	 DemodSearchRange;	/* Search range (Hz) */

	/* Algorithm for search Blind, Cold or Warm */
	FE_STV0903_SearchAlgo_t DemodSearchAlgo;

	/* Search standard: Auto, DVBS1/DSS only or DVBS2 only */
	FE_STV0903_SearchStandard_t DemodSearchStandard;

	/* I,Q inversion search : auto, auto norma first, normal or inverted */
	FE_STV0903_Search_IQ_Inv_t DemodSearch_IQ_Inv;

	FE_STV0903_ModCod_t DemodModcode;

	FE_STV0903_Modulation_t DemodModulation;

	FE_STV0903_Rate_t DemodPunctureRate;

	FE_STV0903_SearchResult_t DemodResults;	/* Results of the search */

	/* Current state of the search algorithm */
	FE_STV0903_SIGNALTYPE_t DemodState;

	FE_STV0903_Error_t DemodError;	/* Last error encountered */

#ifdef CHIP_STAPI
	U32 TopLevelHandle;

#endif				/*
				 */
	U8 DiSEqC_ENV_Mode_Selection;

} FE_STV0903_InternalParams_t;

/****************************************************************
			API FUNCTIONS
****************************************************************/

ST_Revision_t FE_STV0903_GetRevision(void);

FE_STV0903_Error_t FE_STV0903_SetStandby(FE_STV0903_Handle_t Handle,
					 U8 StandbyOn);

FE_STV0903_Error_t FE_STV0903_SetAbortFlag(FE_STV0903_Handle_t Handle,
					   BOOL Abort);

FE_STV0903_Error_t FE_STV0903_Init(FE_STV0903_InitParams_t *pInit,
				   FE_STV0903_Handle_t *Handle);

FE_STV0903_Error_t FE_STV0903_SetTSConfig(FE_STV0903_Handle_t Handle,
					  FE_STV0903_TSConfig_t *pTSConfig,
					  U32 *TSSpeed_Hz);

FE_STV0903_Error_t FE_STV0903_Search(FE_STV0903_Handle_t Handle,
				     FE_STV0903_SearchParams_t *pSearch,
				     FE_STV0903_SearchResult_t *pResult);

BOOL FE_STV0903_Status(FE_STV0903_Handle_t Handle);

FE_STV0903_Error_t FE_STV0903_Unlock(FE_STV0903_Handle_t Handle);

FE_STV0903_Error_t FE_STV0903_ResetDevicesErrors(FE_STV0903_Handle_t Handle);

FE_STV0903_Error_t FE_STV0903_GetSignalInfo(FE_STV0903_Handle_t Handle,
					    FE_STV0903_SignalInfo_t *pInfo);

FE_STV0903_Error_t FE_STV0903_Tracking(FE_STV0903_Handle_t Handle,
				       FE_Sat_TrackingInfo_t *pTrackingInfo);

FE_STV0903_Error_t FE_STV903_GetPacketErrorRate(FE_STV0903_Handle_t Handle,
						U32 *PacketsErrorCount,
						U32 *TotalPacketsCount);

FE_STV0903_Error_t FE_STV903_GetPreViterbiBER(FE_STV0903_Handle_t Handle,
					      U32 *PreVirebiBER);

FE_STV0903_Error_t FE_STV0903_SetMclk(FE_STV0903_Handle_t Handle, U32 Mclk,
				      U32 ExtClk);

FE_STV0903_Error_t FE_STV0903_DiseqcInit(FE_STV0903_Handle_t Handle,
					 FE_STV0903_DiseqC_TxMode DiseqCMode);

FE_STV0903_Error_t FE_STV0903_Set22KHzContinues(FE_STV0903_Handle_t Handle,
						BOOL Enable);

FE_STV0903_Error_t FE_STV0903_DiseqcSend(FE_STV0903_Handle_t Handle,
					 U8 *Data, U32 NbData);

FE_STV0903_Error_t FE_STV0903_DiseqcReceive(FE_STV0903_Handle_t Handle,
					    U8 *Data, U32 *NbData);

FE_STV0903_Error_t FE_STV0903_SetupFSK(FE_STV0903_Handle_t Handle,
				       U32 TransmitCarrier, U32 ReceiveCarrier,
				       U32 Deltaf);

FE_STV0903_Error_t FE_STV0903_EnableFSK(FE_STV0903_Handle_t Handle,
					BOOL EnableModulation,
					BOOL EnableDemodulation);

S32 FE_STV0903_LDPCPowerTracking(FE_STV0903_Handle_t Handle);

FE_STV0903_Error_t FE_STV0903_8PSKCarrierAdaptation(FE_STV0903_Handle_t Handle,
						    U32 SymbolRate);

FE_STV0903_Error_t FE_STV0903_DVBS2_SetGoldCodeX(FE_STV0903_Handle_t Handle,
						 U32 GoldCode);

FE_STV0903_Error_t FE_STV0903_Term(FE_STV0903_Handle_t Handle);

#ifdef STM_FE_DEFINED_FOR_FUTURE_USE
FE_STV0903_Error_t FE_STV0903_SetReg(FE_STV0903_Handle_t Handle, U16 Reg,
				     U32 Val);

FE_STV0903_Error_t FE_STV0903_GetReg(FE_STV0903_Handle_t Handle, U16 Reg,
				     U32 *Val);

FE_STV0903_Error_t FE_STV0903_SetField(FE_STV0903_Handle_t Handle, U32 Field,
				       S32 Val);

FE_STV0903_Error_t FE_STV0903_GetField(FE_STV0903_Handle_t Handle, U32 Field,
				       S32 *Val);
#endif
FE_STV0903_Error_t FE_STV0903_DiseqcFSKModeSelect(FE_STV0903_Handle_t Handle,
						  BOOL ModeSelect);

#endif /* _STV0903_DRV_H */
