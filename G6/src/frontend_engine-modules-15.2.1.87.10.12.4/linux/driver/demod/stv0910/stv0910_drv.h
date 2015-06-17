/************************************************************************
Copyright (C) 2012 STMicroelectronics. All Rights Reserved.

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

Source file name : stv0910_drv.h
Author :

Header for low level driver of STV0910

Date        Modification                                    Name
----        ------------                                    --------
23-Apr-12   Created

************************************************************************/
#ifndef _STV0910_DRV_H
#define _STV0910_DRV_H

#include <fesat_commlla_str.h>
#include <broadcast/stv0910_init.h>
/*#include "fe_sat_tuner.h"*/
#include <stfe_utilities.h>

/****************************************************************
  COMMON STRUCTURES AND TYPEDEF
 ****************************************************************/

typedef InternalParamsPtr FE_STV0910_Handle_t;	/*Handle to the FE */

typedef FE_LLA_Error_t FE_STV0910_Error_t;	/*Error Type */

typedef FE_LLA_LOOKPOINT_t FE_STV0910_LOOKPOINT_t;
typedef FE_LLA_LOOKUP_t FE_STV0910_LOOKUP_t;

/*Current demod search state */
typedef FE_Sat_SEARCHSTATE_t FE_STV0910_SEARCHSTATE_t;
typedef FE_Sat_SIGNALTYPE_t FE_STV0910_SIGNALTYPE_t;
typedef FE_Sat_DemodPath_t FE_STV0910_DEMOD_t;	/*Current Demod */
typedef FE_Sat_Tuner_t FE_STV0910_Tuner_t;
typedef FE_Sat_TrackingStandard_t FE_STV0910_TrackingStandard_t;
typedef FE_Sat_SearchStandard_t FE_STV0910_SearchStandard_t;
typedef FE_Sat_SearchAlgo_t FE_STV0910_SearchAlgo_t;
typedef FE_Sat_Modulation_t FE_STV0910_Modulation_t;
typedef FE_Sat_ModCod_t FE_STV0910_ModCod_t;	/*ModCod */
typedef FE_Sat_FRAME_t FE_STV0910_FRAME_t;	/*Frame Type */
typedef FE_Sat_PILOTS_t FE_STV0910_PILOTS_t;
typedef FE_Sat_Rate_t FE_STV0910_Rate_t;
typedef FE_Sat_RollOff_t FE_STV0910_RollOff_t;
typedef FE_Sat_Search_IQ_Inv_t FE_STV0910_Search_IQ_Inv_t;
typedef FE_Sat_IQ_Inversion_t FE_STV0910_IQ_Inversion_t;

typedef FE_TS_OutputMode_t FE_STV0910_Clock_t;
typedef FE_TS_Config_t FE_STV0910_TSConfig_t;

typedef FE_Sat_DiseqC_TxMode FE_STV0910_DiseqC_TxMode;

typedef enum {
	FE_STV0910_PATH1_OFF_PATH2_OFF = 0,
	FE_STV0910_PATH1_ON_PATH2_OFF = 1,
	FE_STV0910_PATH1_OFF_PATH2_ON = 2,
	FE_STV0910_PATH1_ON_PATH2_ON = 3
} FE_STV0910_LDPC_State;

typedef enum {
	FE_STV0910_TS1 = FE_TS_OUTPUT1,
	FE_STV0910_TS2 = FE_TS_OUTPUT2,
	FE_STV0910_TS3 = FE_TS_OUTPUT3,
	FE_STV0910_MUXTS = FE_TS_MUX
} FE_STV0910_TS_Output;

/****************************************************************
  INIT STRUCTURES
  structure passed to the FE_STV0910_Init() function
 ****************************************************************/

typedef FE_Sat_InitParams_t FE_STV0910_InitParams_t;

/****************************************************************
  SEARCH STRUCTURES
 ****************************************************************/

typedef FE_Sat_SearchParams_t FE_STV0910_SearchParams_t;

typedef FE_Sat_SearchResult_t FE_STV0910_SearchResult_t;

/************************
  INFO STRUCTURE
 ************************/
typedef FE_Sat_SignalInfo_t FE_STV0910_SignalInfo_t;

/****************************************************************
  INTERNAL PARAMS STRUCTURES
 ****************************************************************/
typedef struct {
	 /*HAL*/ STCHIP_Handle_t hDemod;	/* Handle to the chip */
	TUNER_Handle_t hTuner1;	/* Handle to the tuner 1 */
	TUNER_Handle_t hTuner2;	/* Handle to the tuner 2 */
	/* current IC version */
	STCHIP_Handle_t hVGLNA1;	/* Handle to the chip VGLNA for path1 */
	STCHIP_Handle_t hVGLNA2;	/* Handle to the chip VGLNA for path1 */
	/*Clocks and quartz General Params */
	S32 Quartz,		/* Demod Refernce frequency frequency(Hz) */
	 MasterClock;		/* Master clock frequency(Hz) */
	FE_STV0910_RollOff_t RollOff;	/* manual RollOff for DVBS1/DSS only */

	FE_Sat_DemodType DemodType;	/* 900 Demodulator use for single demod
					   or for dual demod) */

	/*Demod 1 */
	FE_STV0910_Tuner_t Tuner1Type;	/* Tuner type STB6000, STB6100, STV6110
					   or SW_TUNER(soft control) */
	FE_STV0910_IQ_Inversion_t Tuner1Global_IQ_Inv;	/* Global I, Q inversion
							   I, Q conection from
							   tuner to demod */

	S32 Tuner1Frequency,	/* Current tuner frequency(KHz) */
	 Tuner1Offset,		/* Tuner offset relative to the carrier(Hz) */
	 Tuner1BW,		/* Current bandwidth of the tuner(Hz) */
	 Demod1SymbolRate,	/* Symbol rate(Bds) */
	 Demod1SearchRange,	/* Search range(Hz) */
	 Tuner1IndexJump;

	FE_STV0910_SearchAlgo_t Demod1SearchAlgo; /* Algo for search Blind,
						     Cold or Warm */
	FE_STV0910_SearchStandard_t Demod1SearchStandard;	/* Search std:
								   Auto, DVBS1/
								   DSS only or
								   DVBS2 only */
	FE_STV0910_Search_IQ_Inv_t Demod1Search_IQ_Inv;	/* I, Q  inv search :
							   auto, auto normal
							   first, normal or
							   inverted */
	FE_STV0910_ModCod_t Demod1Modcode;
	FE_STV0910_Modulation_t Demod1Modulation;
	FE_STV0910_Rate_t Demod1PunctureRate;

	FE_STV0910_SearchResult_t Demod1Results;	/* Results of search */
	FE_STV0910_SIGNALTYPE_t Demod1State;	/* Current state of the search
						   algorithm */

	FE_STV0910_TS_Output Path1TSoutpt;
	FE_STV0910_Error_t Demod1Error;	/* Last error encountered */

	/*Demod 2 */
	FE_STV0910_Tuner_t Tuner2Type;	/* Tuner type STB600, STB6100, STB6110
					   or NO_TUNER(soft control) */
	FE_STV0910_IQ_Inversion_t Tuner2Global_IQ_Inv; /* Global I, Q inversion
							  I, Q conection from
							  tuner to demod */

	S32 Tuner2Frequency,	/* Current tuner frequency(KHz) */
	 Tuner2Offset,		/* Tuner offset relative to the carrier(Hz) */
	 Tuner2BW,		/* Current bandwidth of the tuner(Hz) */
	 Demod2SymbolRate,	/* Symbol rate(Bds) */
	 Demod2SearchRange,	/* Search range(Hz) */
	 Tuner2IndexJump;

	FE_STV0910_SearchAlgo_t Demod2SearchAlgo; /* Algo for search Blind,
						     Cold or Warm */
	FE_STV0910_SearchStandard_t Demod2SearchStandard; /* Search standard:
							     Auto, DVBS1/DSS
							     only or DVBS2
							     only */
	FE_STV0910_Search_IQ_Inv_t Demod2Search_IQ_Inv;	/* I, Q  inv search :
							   auto, auto normal
							   first, normal or
							   inverted */
	FE_STV0910_ModCod_t Demod2Modcode;
	FE_STV0910_Modulation_t Demod2Modulation;
	FE_STV0910_Rate_t Demod2PunctureRate;

	FE_STV0910_SearchResult_t Demod2Results;	/* Results of search */
	FE_STV0910_SIGNALTYPE_t Demod2State;	/* Current state of search
						   algo */

	FE_STV0910_TS_Output Path2TSoutpt;

	FE_STV0910_Error_t Demod2Error;	/* Last error encountered */

#ifdef CHIP_STAPI
	U32 TopLevelHandle;
#endif

	U32 DiSEqC_ENV_Mode_Selection;
} FE_STV0910_InternalParams_t;

/****************************************************************
  API FUNCTIONS
 ****************************************************************/

ST_Revision_t FE_STV0910_GetRevision(void);

#ifdef CHIP_STAPI
STCHIP_Error_t FE_STV0910_RepeaterFn(STCHIP_Handle_t hChip, BOOL State,
					unsigned char *Buffer);
STCHIP_Error_t FE_STV0910_Repeater2Fn(STCHIP_Handle_t hChip, BOOL State,
					unsigned char *Buffer);
#else
STCHIP_Error_t FE_STV0910_RepeaterFn(STCHIP_Handle_t hChip, BOOL State);
STCHIP_Error_t FE_STV0910_Repeater2Fn(STCHIP_Handle_t hChip, BOOL State);
#endif
FE_STV0910_Error_t FE_STV0910_SetStandby(FE_STV0910_Handle_t Handle,
					 U8 StandbyOn,
					 FE_STV0910_DEMOD_t Demod);

FE_STV0910_Error_t FE_STV0910_SetAbortFlag(FE_STV0910_Handle_t Handle,
					   BOOL Abort,
					   FE_STV0910_DEMOD_t Demod);

FE_STV0910_Error_t FE_STV0910_Init(FE_STV0910_InitParams_t *pInit,
				   FE_STV0910_Handle_t *Handle);

FE_STV0910_Error_t FE_STV0910_SetTSoutput(FE_STV0910_Handle_t Handle,
					  FE_STV0910_TSConfig_t *
					  Path1pTSConfig,
					  FE_STV0910_TSConfig_t *
					  Path2pTSConfig);

FE_STV0910_Error_t FE_STV0910_SetTSConfig(FE_STV0910_Handle_t Handle,
					  FE_STV0910_TSConfig_t *
					  Path1pTSConfig,
					  FE_STV0910_TSConfig_t *
					  Path2pTSConfig, U32 *Path1TSSpeed_Hz,
					  U32 *Path2TSSpeed_Hz);

FE_STV0910_Error_t FE_STV0910_SetDVBS2_Single(FE_STV0910_Handle_t Handle,
					      FE_Sat_DemodType LDPC_Mode,
					      FE_STV0910_DEMOD_t Demod);

FE_STV0910_Error_t FE_STV0910_Search(FE_STV0910_Handle_t Handle,
				     FE_STV0910_SearchParams_t *pSearch,
				     FE_STV0910_SearchResult_t *pResult,
				     U32 Satellite_Scan);

BOOL FE_STV0910_Status(FE_STV0910_Handle_t Handle, FE_STV0910_DEMOD_t Demod);

FE_STV0910_Error_t FE_STV0910_Unlock(FE_STV0910_Handle_t Handle,
				     FE_STV0910_DEMOD_t Demod);

FE_STV0910_Error_t FE_STV0910_ResetDevicesErrors(FE_STV0910_Handle_t Handle,
						 FE_STV0910_DEMOD_t Demod);

FE_STV0910_Error_t FE_STV0910_GetSignalInfo(FE_STV0910_Handle_t Handle,
					    FE_STV0910_DEMOD_t Demod,
					    FE_STV0910_SignalInfo_t *pInfo,
					    U32 Satellite_Scan);

FE_STV0910_Error_t FE_STV0910_Tracking(FE_STV0910_Handle_t Handle,
				       FE_STV0910_DEMOD_t Demod,
				       FE_Sat_TrackingInfo_t *pTrackingInfo);

FE_STV0910_Error_t FE_STV910_GetPacketErrorRate(FE_STV0910_Handle_t Handle,
						FE_STV0910_DEMOD_t Demod,
						U32 *PacketsErrorCount,
						U32 *TotalPacketsCount);

FE_STV0910_Error_t FE_STV910_GetPreViterbiBER(FE_STV0910_Handle_t Handle,
					      FE_STV0910_DEMOD_t Demod,
					      U32 *PreViterbiBER);

FE_STV0910_Error_t FE_STV0910_SetMclk(FE_STV0910_Handle_t Handle,
				      U32 Mclk, U32 ExtClk);

FE_STV0910_Error_t FE_STV0910_DVBS2_SetGoldCodeX(FE_STV0910_Handle_t Handle,
						 FE_STV0910_DEMOD_t Demod,
						 U32 GoldCode);

FE_STV0910_Error_t FE_STV0910_SetupFSK(FE_STV0910_Handle_t Handle,
				       U32 TransmitCarrier,
				       U32 ReceiveCarrier, U32 Deltaf);

FE_STV0910_Error_t FE_STV0910_EnableFSK(FE_STV0910_Handle_t Handle,
					BOOL EnableModulation,
					BOOL EnableDemodulation);

FE_STV0910_Error_t FE_STV0910_DiseqcFSKModeSelect(FE_STV0910_Handle_t Handle,
						  BOOL ModeSelect);

FE_STV0910_Error_t FE_STV0910_DiseqcInit(FE_STV0910_Handle_t Handle,
					 FE_STV0910_DiseqC_TxMode DiseqCMode,
					 FE_STV0910_DEMOD_t Demod);

FE_STV0910_Error_t FE_STV0910_Set22KHzContinues(FE_STV0910_Handle_t Handle,
						BOOL Enable,
						FE_STV0910_DEMOD_t Demod);

FE_STV0910_Error_t FE_STV0910_DiseqcSend(FE_STV0910_Handle_t Handle,
					 U8 *Data,
					 U32 NbData, FE_STV0910_DEMOD_t Demod);

FE_STV0910_Error_t FE_STV0910_DiseqcReceive(FE_STV0910_Handle_t Handle,
					    U8 *Data,
					    U32 *NbData,
					    FE_STV0910_DEMOD_t Demod);

FE_STV0910_Error_t FE_STV0910_Term(FE_STV0910_Handle_t Handle);

FE_STV0910_Error_t FE_STV0910_STV6120_HMRFilter(FE_STV0910_Handle_t Handle,
						FE_STV0910_DEMOD_t Demod);

/*FE_STV0910_Error_t FE_STV0910_SetContinous_Tone(FE_STV0910_Handle_t Handle,
  STFRONTEND_LNBToneState_t tonestate, FE_Sat_DemodPath_t Path);
  */
#endif /* _STV0910_DRV_H */
