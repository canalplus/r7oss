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

Source file name : stv0900_drv.h
Author :

Header for low level initialisation driver of STV0900

Date        Modification                                    Name
----        ------------                                    --------
21-May-12   Created

************************************************************************/
#ifndef _STV0900_DRV_H
#define _STV0900_DRV_H

#include <fesat_commlla_str.h>
#include <broadcast/stv0900_init.h>
#include <stfe_utilities.h>

/****************************************************************
COMMON STRUCTURES AND TYPEDEF
****************************************************************/

typedef InternalParamsPtr FE_STV0900_Handle_t;	/*Handle to the FE */

typedef FE_LLA_Error_t FE_STV0900_Error_t;	/*Error Type */

typedef FE_LLA_LOOKPOINT_t FE_STV0900_LOOKPOINT_t;
typedef FE_LLA_LOOKUP_t FE_STV0900_LOOKUP_t;

/*Current demod search state */
typedef FE_Sat_SEARCHSTATE_t FE_STV0900_SEARCHSTATE_t;
typedef FE_Sat_SIGNALTYPE_t FE_STV0900_SIGNALTYPE_t;
typedef FE_Sat_DemodPath_t FE_STV0900_DEMOD_t;	/*Current Demod */
typedef FE_Sat_Tuner_t FE_STV0900_Tuner_t;
typedef FE_Sat_TrackingStandard_t FE_STV0900_TrackingStandard_t;
typedef FE_Sat_SearchStandard_t FE_STV0900_SearchStandard_t;
typedef FE_Sat_SearchAlgo_t FE_STV0900_SearchAlgo_t;
typedef FE_Sat_Modulation_t FE_STV0900_Modulation_t;
typedef FE_Sat_ModCod_t FE_STV0900_ModCod_t;	/*ModCod */
typedef FE_Sat_FRAME_t FE_STV0900_FRAME_t;	/*Frame Type */
typedef FE_Sat_PILOTS_t FE_STV0900_PILOTS_t;
typedef FE_Sat_Rate_t FE_STV0900_Rate_t;
typedef FE_Sat_RollOff_t FE_STV0900_RollOff_t;
typedef FE_Sat_Search_IQ_Inv_t FE_STV0900_Search_IQ_Inv_t;
typedef FE_Sat_IQ_Inversion_t FE_STV0900_IQ_Inversion_t;

typedef FE_TS_OutputMode_t FE_STV0900_Clock_t;
typedef FE_TS_Config_t FE_STV0900_TSConfig_t;

typedef FE_Sat_DiseqC_TxMode FE_STV0900_DiseqC_TxMode;

typedef enum {
	FE_STV0900_PATH1_OFF_PATH2_OFF = 0,
	FE_STV0900_PATH1_ON_PATH2_OFF = 1,
	FE_STV0900_PATH1_OFF_PATH2_ON = 2,
	FE_STV0900_PATH1_ON_PATH2_ON = 3
} FE_STV0900_LDPC_State;

typedef enum {
	FE_STV0900_TS1 = FE_TS_OUTPUT1,
	FE_STV0900_TS2 = FE_TS_OUTPUT2,
	FE_STV0900_TS3 = FE_TS_OUTPUT3,
	FE_STV0900_MUXTS = FE_TS_MUX
} FE_STV0900_TS_Output;

/****************************************************************
INIT STRUCTURES
structure passed to the FE_STV0900_Init() function
****************************************************************/

typedef FE_Sat_InitParams_t FE_STV0900_InitParams_t;

/****************************************************************
SEARCH STRUCTURES
****************************************************************/

typedef FE_Sat_SearchParams_t FE_STV0900_SearchParams_t;

typedef FE_Sat_SearchResult_t FE_STV0900_SearchResult_t;

/************************
INFO STRUCTURE
************************/
typedef FE_Sat_SignalInfo_t FE_STV0900_SignalInfo_t;
typedef FE_Sat_BER_Algo_t FE_STV0900_BerMeasure_t;
/****************************************************************
INTERNAL PARAMS STRUCTURES
****************************************************************/
typedef struct {
	 /*HAL*/ STCHIP_Handle_t hDemod;	/* Handle to the chip */
	TUNER_Handle_t hTuner1;	/* Handle to the tuner 1 */
	TUNER_Handle_t hTuner2;	/* Handle to the tuner 2 */

	STCHIP_Handle_t hVglna1;
	STCHIP_Handle_t hVglna2;	/* current IC version */

	/*Clocks and quartz General Params */
	S32 Quartz,		/* Demod Refernce frequency frequency (Hz) */
	 MasterClock;		/* Master clock frequency (Hz) */
	FE_STV0900_RollOff_t RollOff;	/* manual RollOff for DVBS1/DSS only */

	/* 900 Demodulator use for single demod or for dual demod) */
	FE_Sat_DemodType DemodType;

	/*Demod 1 */
	/* Tuner type STB6000,STB6100, STV6110 or SW_TUNER (soft control)  */
	FE_STV0900_Tuner_t Tuner1Type;
	/* Global I,Q inversion I,Q conection from tuner to demod */
	FE_STV0900_IQ_Inversion_t Tuner1Global_IQ_Inv;

	S32 Tuner1Frequency,	/* Current tuner frequency (KHz) */
	 Tuner1Offset,		/* Tuner offset relative to the carrier (Hz) */
	 Tuner1BW,		/* Current bandwidth of the tuner (Hz) */
	 Demod1SymbolRate,	/* Symbol rate (Bds) */
	 Demod1SearchRange;	/* Search range (Hz) */

	/* Algorithm for search Blind, Cold or Warm */
	FE_STV0900_SearchAlgo_t Demod1SearchAlgo;
	/* Search standard: Auto, DVBS1/DSS only or DVBS2 only */
	FE_STV0900_SearchStandard_t Demod1SearchStandard;
	/* I,Q  inversion search : auto, auto norma first, normal or inverted */
	FE_STV0900_Search_IQ_Inv_t Demod1Search_IQ_Inv;
	FE_STV0900_ModCod_t Demod1Modcode;
	FE_STV0900_Modulation_t Demod1Modulation;
	FE_STV0900_Rate_t Demod1PunctureRate;

	FE_STV0900_SearchResult_t Demod1Results;	/* Results of search */
	/* Current state of the search algorithm */
	FE_STV0900_SIGNALTYPE_t Demod1State;

	FE_STV0900_TS_Output Path1TSoutpt;
	FE_STV0900_Error_t Demod1Error;	/* Last error encountered */

	/*Demod 2 */
	/* Tuner type STB600,STB6100, STB6110 or NO_TUNER (soft control) */
	FE_STV0900_Tuner_t Tuner2Type;
	/* Global I,Q inversion I,Q conection from tuner to demod */
	FE_STV0900_IQ_Inversion_t Tuner2Global_IQ_Inv;

	S32 Tuner2Frequency,	/* Current tuner frequency (KHz) */
	 Tuner2Offset,		/* Tuner offset relative to the carrier (Hz) */
	 Tuner2BW,		/* Current bandwidth of the tuner (Hz) */
	 Demod2SymbolRate,	/* Symbol rate (Bds) */
	 Demod2SearchRange;	/* Search range (Hz) */

	/* Algorithm for search Blind, Cold or Warm */
	FE_STV0900_SearchAlgo_t Demod2SearchAlgo;
	/* Search standard: Auto, DVBS1/DSS only or DVBS2 only */
	FE_STV0900_SearchStandard_t Demod2SearchStandard;
	/* I,Q  inversion search : auto, auto norma first, normal or inverted */
	FE_STV0900_Search_IQ_Inv_t Demod2Search_IQ_Inv;
	FE_STV0900_ModCod_t Demod2Modcode;
	FE_STV0900_Modulation_t Demod2Modulation;
	FE_STV0900_Rate_t Demod2PunctureRate;

	FE_STV0900_SearchResult_t Demod2Results;	/* Results of search */
	/* Current state of the search algorithm */
	FE_STV0900_SIGNALTYPE_t Demod2State;

	FE_STV0900_TS_Output Path2TSoutpt;
	FE_STV0900_Error_t Demod2Error;	/* Last error encountered  */

#ifdef CHIP_STAPI
	U32 TopLevelHandle;
#endif
	U32 DiSEqC_ENV_Mode_Selection;
	FE_STV0900_BerMeasure_t BER_Algo;
} FE_STV0900_InternalParams_t;

/****************************************************************
API FUNCTIONS
****************************************************************/

ST_Revision_t FE_STV0900_GetRevision(void);

FE_STV0900_Error_t FE_STV0900_SetStandby(FE_STV0900_Handle_t Handle,
					 U8 StandbyOn,
					 FE_STV0900_DEMOD_t Demod);

FE_STV0900_Error_t FE_STV0900_SetAbortFlag(FE_STV0900_Handle_t Handle,
					   BOOL Abort,
					   FE_STV0900_DEMOD_t Demod);

FE_STV0900_Error_t FE_STV0900_Init(FE_STV0900_InitParams_t *pInit,
				   FE_STV0900_Handle_t *Handle);

FE_STV0900_Error_t FE_STV0900_SetTSOutput(FE_STV0900_Handle_t Handle,
					  FE_STV0900_TS_Output Path1TS,
					  FE_STV0900_Clock_t Path1TSData,
					  FE_STV0900_TS_Output Path2TS,
					  FE_STV0900_Clock_t Path2TSData);
FE_STV0900_Error_t FE_STV0900_SetTSConfig(FE_STV0900_Handle_t Handle,
					  FE_STV0900_TSConfig_t *
					  Path1pTSConfig,
					  FE_STV0900_TSConfig_t *
					  Path2pTSConfig, U32 *Path1TSSpeed_Hz,
					  U32 *Path2TSSpeed_Hz);

FE_STV0900_Error_t FE_STV0900_SetDVBS2_Single(FE_STV0900_Handle_t Handle,
					      FE_Sat_DemodType LDPC_Mode,
					      FE_STV0900_DEMOD_t Demod);

FE_STV0900_Error_t FE_STV0900_Search(FE_STV0900_Handle_t Handle,
				     FE_STV0900_SearchParams_t *pSearch,
				     FE_STV0900_SearchResult_t *pResult);

BOOL FE_STV0900_Status(FE_STV0900_Handle_t Handle, FE_STV0900_DEMOD_t Demod);

FE_STV0900_Error_t FE_STV0900_Unlock(FE_STV0900_Handle_t Handle,
				     FE_STV0900_DEMOD_t Demod);

FE_STV0900_Error_t FE_STV0900_ResetDevicesErrors(FE_STV0900_Handle_t Handle,
						 FE_STV0900_DEMOD_t Demod);

FE_STV0900_Error_t FE_STV0900_GetSignalInfo(FE_STV0900_Handle_t Handle,
					    FE_STV0900_DEMOD_t Demod,
					    FE_STV0900_SignalInfo_t *pInfo);

FE_STV0900_Error_t FE_STV0900_Tracking(FE_STV0900_Handle_t Handle,
				       FE_STV0900_DEMOD_t Demod,
				       FE_Sat_TrackingInfo_t *pTrackingInfo);

FE_STV0900_Error_t FE_STV900_GetPacketErrorRate(FE_STV0900_Handle_t Handle,
						FE_STV0900_DEMOD_t Demod,
						U32 *PacketsErrorCount,
						U32 *TotalPacketsCount);

FE_STV0900_Error_t FE_STV0900_SetMclk(FE_STV0900_Handle_t Handle,
				      U32 Mclk, U32 ExtClk);

FE_STV0900_Error_t FE_STV0900_DiseqcInit(FE_STV0900_Handle_t Handle,
					 FE_STV0900_DiseqC_TxMode DiseqCMode,
					 FE_STV0900_DEMOD_t Demod);

FE_STV0900_Error_t FE_STV0900_Set22KHzContinues(FE_STV0900_Handle_t Handle,
						BOOL Enable,
						FE_STV0900_DEMOD_t Demod);

FE_STV0900_Error_t FE_STV0900_DiseqcSend(FE_STV0900_Handle_t Handle,
					 U8 *Data,
					 U32 NbData, FE_STV0900_DEMOD_t Demod);

FE_STV0900_Error_t FE_STV0900_DiseqcReceive(FE_STV0900_Handle_t Handle,
					    U8 *Data,
					    U32 *NbData,
					    FE_STV0900_DEMOD_t Demod);

FE_STV0900_Error_t FE_STV0900_SetupFSK(FE_STV0900_Handle_t Handle,
				       U32 TransmitCarrier,
				       U32 ReceiveCarrier, U32 Deltaf);

FE_STV0900_Error_t FE_STV0900_EnableFSK(FE_STV0900_Handle_t Handle,
					BOOL EnableModulation,
					BOOL EnableDemodulation);

FE_STV0900_Error_t FE_STV0900_DiseqcFSKModeSelect(FE_STV0900_Handle_t Handle,
						  BOOL ModeSelect);

FE_STV0900_Error_t FE_STV0900_DVBS2_SetGoldCodeX(FE_STV0900_Handle_t Handle,
						 FE_STV0900_DEMOD_t Demod,
						 U32 GoldCode);

FE_STV0900_Error_t FE_STV0900_StopTransportStream(FE_STV0900_Handle_t Handle,
						  FE_STV0900_DEMOD_t Demod,
						  BOOL StopTS);

FE_STV0900_LDPC_State FE_STV0900_LDPCPowerTracking(FE_STV0900_Handle_t Handle,
						   FE_STV0900_DEMOD_t Demod);
FE_STV0900_Error_t FE_STV0900_8PSKCarrierAdaptation(FE_STV0900_Handle_t Handle,
						    U32 SymbolRate,
						    FE_STV0900_DEMOD_t Demod);
FE_STV0900_Error_t FE_STV0900_STV6120_HMRFilter(FE_STV0900_Handle_t Handle,
						FE_STV0900_DEMOD_t Demod);
FE_STV0900_Error_t FE_STV0900_SetDual8PSK45MBs(FE_STV0900_Handle_t Handle,
					       U32 SymbolRate,
					       FE_STV0900_DEMOD_t Demod);

FE_STV0900_Error_t FE_STV0900_BER_Mode(FE_STV0900_Handle_t Handle,
				       FE_STV0900_DEMOD_t Demod,
				       FE_STV0900_BerMeasure_t setBERMeter);
U32 FE_STV0900_GetBer_BerMeter(FE_STV0900_Handle_t Handle,
			       FE_STV0900_DEMOD_t Demod, BOOL count);

FE_STV0900_Error_t FE_STV0900_Term(FE_STV0900_Handle_t Handle);

#ifdef STM_FE_DEFINED_FOR_FUTURE_USE
FE_STV0900_Error_t FE_STV0900_SetReg(FE_STV0900_Handle_t Handle, U16 Reg,
				     U32 Val);
FE_STV0900_Error_t FE_STV0900_GetReg(FE_STV0900_Handle_t Handle, U16 Reg,
				     U32 *Val);
FE_STV0900_Error_t FE_STV0900_SetField(FE_STV0900_Handle_t Handle, U32 Field,
				       S32 Val);
FE_STV0900_Error_t FE_STV0900_GetField(FE_STV0900_Handle_t Handle, U32 Field,
				       S32 *Val);
#endif

#endif /* _STV0900_DRV_H */
