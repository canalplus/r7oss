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

Source file name :mxl584_drv.h
Author :

Header for low level driver of mxl584_drv

Date        Modification                                    Name
----        ------------                                    --------
23-Apr-12   Created						HS

************************************************************************/
#ifndef _MXL584_DRV_H
#define _MXL584_DRV_H

#include <fesat_commlla_str.h>
#include <mxl584_init.h>
/*#include "fe_sat_tuner.h"*/
#include <stfe_utilities.h>
#include "maxlineardatatypes.h"
#include "mxlware_hydra_demodtunerapi.h"

#define MXL584_HYDRA_NBREGS 2500
#define MXL584_HYDRA_NBFIELDS 7800

/****************************************************************
					COMMON STRUCTURES AND TYPEDEF
 ****************************************************************/


typedef InternalParamsPtr FE_MXL584_Handle_t;	/*Handle to the FE */
typedef FE_LLA_Error_t FE_MXL584_Error_t;	/*Error Type*/

typedef FE_LLA_LOOKPOINT_t	FE_MXL584_LOOKPOINT_t;
typedef FE_LLA_LOOKUP_t	FE_MXL584_LOOKUP_t;

typedef FE_Sat_SEARCHSTATE_t FE_MXL584_SEARCHSTATE_t;
typedef FE_Sat_SIGNALTYPE_t FE_MXL584_SIGNALTYPE_t;
typedef U8	FE_MXL584_DEMOD_t; /*Current Demod*/
typedef FE_Sat_Tuner_t FE_MXL584_Tuner_t;
typedef FE_Sat_TrackingStandard_t FE_MXL584_TrackingStandard_t;
typedef FE_Sat_SearchStandard_t FE_MXL584_SearchStandard_t;
typedef FE_Sat_SearchAlgo_t FE_MXL584_SearchAlgo_t;
typedef	FE_Sat_Modulation_t	FE_MXL584_Modulation_t;
typedef FE_Sat_ModCod_t	FE_MXL584_ModCod_t; /*ModCod*/
typedef FE_Sat_FRAME_t	FE_MXL584_FRAME_t; /*Frame Type*/
typedef FE_Sat_PILOTS_t	FE_MXL584_PILOTS_t;
typedef FE_Sat_Rate_t	FE_MXL584_Rate_t;
typedef FE_Sat_RollOff_t	FE_MXL584_RollOff_t;
typedef	FE_Sat_Search_IQ_Inv_t	FE_MXL584_Search_IQ_Inv_t;
typedef FE_Sat_IQ_Inversion_t	FE_MXL584_IQ_Inversion_t;

typedef FE_TS_OutputMode_t    FE_MXL584_Clock_t;
typedef	FE_TS_Config_t     FE_MXL584_TSConfig_t;


typedef FE_Sat_DiseqC_TxMode FE_MXL584_DiseqC_TxMode;

typedef enum {
	FE_MXL584_PATH1_OFF_PATH2_OFF = 0,
	FE_MXL584_PATH1_ON_PATH2_OFF,
	FE_MXL584_PATH1_OFF_PATH2_ON,
	FE_MXL584_PATH1_ON_PATH2_ON
} FE_MXL584_LDPC_State;

typedef enum {
	FE_MXL584_TS1 = FE_TS_OUTPUT1,
	FE_MXL584_TS2 = FE_TS_OUTPUT2,
	FE_MXL584_TS3 = FE_TS_OUTPUT3,
	FE_MXL584_MUXTS = FE_TS_MUX

} FE_MXL584_TS_Output;

/****************************************************************
		INIT STRUCTURES
		structure passed to the FE_MXL584_Init() function
 ****************************************************************/

typedef FE_Sat_InitParams_t FE_MXL584_InitParams_t;

/****************************************************************
					SEARCH STRUCTURES
 ****************************************************************/
typedef FE_Sat_SearchParams_t FE_MXL584_SearchParams_t;

typedef FE_Sat_SearchResult_t FE_MXL584_SearchResult_t;

/************************
	INFO STRUCTURE
************************/
typedef FE_Sat_SignalInfo_t FE_MXL584_SignalInfo_t;

/****************************************************************
					INTERNAL PARAMS STRUCTURES
 ****************************************************************/
typedef struct {
	/*HAL*/
	STCHIP_Handle_t hDemod;	/*Handle to the chip*/
	TUNER_Handle_t	hTuner1;/*Handle to the tuner 1	*/
	TUNER_Handle_t	hTuner2;/*Handle to the tuner 2	*/
	STCHIP_Handle_t   hVglna1;
	STCHIP_Handle_t   hVglna2;
	/*Clocks and quartz General Params*/
	S32		Quartz,
			MasterClock;
	FE_MXL584_RollOff_t	RollOff;
	FE_Sat_DemodType	DemodType;
	/*Demod 1*/
	FE_MXL584_Tuner_t	Tuner1Type;
	FE_MXL584_IQ_Inversion_t	TunerGlobal_IQ_Inv;
	S32				TunerFrequency,
					TunerOffset,
					TunerBW,
					DemodSymbolRate,
					DemodSearchRange;
	FE_MXL584_SearchAlgo_t		DemodSearchAlgo;
	FE_MXL584_SearchStandard_t	DemodSearchStandard;
	FE_MXL584_Search_IQ_Inv_t	DemodSearch_IQ_Inv;
	FE_MXL584_ModCod_t		DemodModcode;
	FE_MXL584_Modulation_t		DemodModulation;
	FE_MXL584_Rate_t		DemodPunctureRate;

	FE_MXL584_SearchResult_t	DemodResults;
	FE_MXL584_SIGNALTYPE_t	DemodState;
	FE_MXL584_Error_t	DemodError;
	/*Demod 2*/
	FE_MXL584_Tuner_t	Tuner2Type;
	FE_MXL584_IQ_Inversion_t	Tuner2Global_IQ_Inv;
	S32				Tuner2Frequency,
					Tuner2Offset,
					Tuner2BW,
					Demod2SymbolRate,
					Demod2SearchRange;
	FE_MXL584_SearchAlgo_t		Demod2SearchAlgo;
	FE_MXL584_SearchStandard_t	Demod2SearchStandard;
	FE_MXL584_Search_IQ_Inv_t	Demod2Search_IQ_Inv;
	FE_MXL584_ModCod_t		Demod2Modcode;
	FE_MXL584_Modulation_t		Demod2Modulation;
	FE_MXL584_Rate_t		Demod2PunctureRate;
	FE_MXL584_SearchResult_t	Demod2Results;
	FE_MXL584_SIGNALTYPE_t		Demod2State;
	FE_MXL584_TS_Output		Path2TSoutpt;
	FE_MXL584_Error_t		Demod2Error;
	MXL_STATUS_E status;
	U32 TopLevelHandle;
	U32   DiSEqC_ENV_Mode_Selection;
} FE_MXL584_InternalParams_t;


/****************************************************************
					API FUNCTIONS
****************************************************************/

MXL_STATUS_E FE_MXL584_downloadFirmware(UINT8 DEV_ID,
				STCHIP_Handle_t *hChipHandle);


ST_Revision_t FE_MXL584_GetRevision(void);

FE_MXL584_Error_t FE_MXL584_SetStandby(FE_MXL584_Handle_t Handle,
					 U8 StandbyOn, FE_MXL584_DEMOD_t Demod);

FE_MXL584_Error_t FE_MXL584_SetAbortFlag(FE_MXL584_Handle_t Handle,
				BOOL Abort, FE_MXL584_DEMOD_t Demod);

FE_MXL584_Error_t FE_MXL584_Init(FE_MXL584_InitParams_t	*pInit,
					FE_MXL584_Handle_t *Handle);

FE_MXL584_Error_t FE_MXL584_SetTSoutput(FE_MXL584_Handle_t Handle,
				FE_MXL584_TSConfig_t *Path1pTSConfig,
				FE_MXL584_TSConfig_t *Path2pTSConfig);

FE_MXL584_Error_t FE_MXL584_SetTSConfig(FE_MXL584_Handle_t Handle ,
				FE_MXL584_TSConfig_t *Path1pTSConfig,
				FE_MXL584_TSConfig_t *Path2pTSConfig,
				U32 *Path1TSSpeed_Hz,
				U32 *Path2TSSpeed_Hz);

FE_MXL584_Error_t FE_MXL584_SetDVBS2_Single(FE_MXL584_Handle_t Handle,
					  FE_Sat_DemodType LDPC_Mode ,
					  FE_MXL584_DEMOD_t Demod);

FE_MXL584_Error_t FE_MXL584_Search(FE_MXL584_Handle_t	Handle,
				FE_MXL584_DEMOD_t Demod,
				FE_MXL584_SearchParams_t *pSearch,
				FE_MXL584_SearchResult_t *pResult);

BOOL	FE_MXL584_Status(FE_MXL584_Handle_t	Handle,
				FE_MXL584_DEMOD_t Demod);

FE_MXL584_Error_t FE_MXL584_Unlock(FE_MXL584_Handle_t Handle,
					FE_MXL584_DEMOD_t Demod);

FE_MXL584_Error_t FE_MXL584_ResetDevicesErrors(FE_MXL584_Handle_t Handle);

FE_MXL584_Error_t FE_MXL584_GetSignalInfo(FE_MXL584_Handle_t Handle,
						FE_MXL584_DEMOD_t Demod,
						FE_MXL584_SignalInfo_t	*pInfo);

FE_MXL584_Error_t FE_MXL584_Tracking(FE_MXL584_Handle_t Handle,
					FE_MXL584_DEMOD_t Demod,
					FE_Sat_TrackingInfo_t *pTrackingInfo);

FE_MXL584_Error_t FE_STV910_GetPacketErrorRate(FE_MXL584_Handle_t Handle,
						U32 *PacketsErrorCount,
						U32 *TotalPacketsCount);

FE_MXL584_Error_t FE_MXL584_SetMclk(FE_MXL584_Handle_t Handle, U32 Mclk,
								U32 ExtClk);

FE_MXL584_Error_t FE_MXL584_Term(FE_MXL584_Handle_t Handle);

FE_MXL584_Error_t FE_MXL584_cfg_pid_fltr(FE_MXL584_Handle_t Handle,
					MXL_HYDRA_DEMOD_ID_E Demod_Number,
					uint32_t pid, BOOL filter_state);

FE_MXL584_Error_t FE_MXL584_Taging_Muxing(struct stm_fe_demod_s *priv);
FE_MXL584_Error_t FE_MXL584_SetTSout(struct stm_fe_demod_s *priv);
int config_mpeg_interface_ts3(void);

MXL_HYDRA_FEC_E
	stfe_to_mxl_fec_map(FE_Sat_Rate_t FECRate);
FE_Sat_Rate_t
	mxl_to_stfe_fec_map(MXL_HYDRA_FEC_E FECRate);
MXL_HYDRA_BCAST_STD_E
	stfe_to_mxl_standard(FE_Sat_SearchStandard_t Standard);
FE_Sat_TrackingStandard_t
	mxl_to_stfe_standard(MXL_HYDRA_BCAST_STD_E Standard);
FE_Sat_RollOff_t
	mxl_to_stfe_r_off(MXL_HYDRA_ROLLOFF_E RollOff_Hydra);
FE_Sat_IQ_Inversion_t
	mxl_to_stfe_spectrum(MXL_HYDRA_SPECTRUM_E Spectrum_Hydra);
MXL_HYDRA_MODULATION_E
	stfe_to_mxl_modulation(FE_Sat_Modulation_t Modulation);
FE_Sat_Modulation_t
	mxl_to_stfe_modulation(MXL_HYDRA_MODULATION_E Modulation);


#endif /* _MXL584_DRV_H */
