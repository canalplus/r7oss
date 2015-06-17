/************************************************************************
Copyright(C) 2013 STMicroelectronics. All Rights Reserved.

This file is part of the stm_fe Library.

stm_fe is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

stm_fe is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY ; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with stm_fe ; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111 - 1307, USA.

The stm_fe Library may alternatively be licensed under a proprietary
license from ST.

Source file name : stv0913_drv.h
Author :

Header for low level driver of STV0913

Date		Modification		Name
----		------------		 --------
23 - Apr - 12	Created

************************************************************************/
#ifndef _STV0913_DRV_H
#define _STV0913_DRV_H

#include <fesat_commlla_str.h>
#include "stv0913_init.h"
#include <stfe_utilities.h>

/****************************************************************
		COMMON STRUCTURES AND TYPEDEF
****************************************************************/


	typedef InternalParamsPtr FE_STV0913_Handle_t; /*Handle to the FE */

	typedef FE_LLA_Error_t FE_STV0913_Error_t; /*Error Type*/

	typedef FE_LLA_LOOKPOINT_t	FE_STV0913_LOOKPOINT_t;
	typedef FE_LLA_LOOKUP_t	FE_STV0913_LOOKUP_t;
	/*Current demod search state*/
	typedef FE_Sat_SEARCHSTATE_t FE_STV0913_SEARCHSTATE_t;
	typedef FE_Sat_SIGNALTYPE_t FE_STV0913_SIGNALTYPE_t;
	typedef FE_Sat_DemodPath_t	FE_STV0913_DEMOD_t; /*Current Demod*/
	typedef FE_Sat_Tuner_t FE_STV0913_Tuner_t;
	typedef FE_Sat_TrackingStandard_t FE_STV0913_TrackingStandard_t;
	typedef FE_Sat_SearchStandard_t FE_STV0913_SearchStandard_t;
	typedef FE_Sat_SearchAlgo_t FE_STV0913_SearchAlgo_t;
	typedef	FE_Sat_Modulation_t	FE_STV0913_Modulation_t;
	typedef FE_Sat_ModCod_t	FE_STV0913_ModCod_t; /*ModCod*/
	typedef FE_Sat_FRAME_t	FE_STV0913_FRAME_t; /*Frame Type*/
	typedef FE_Sat_PILOTS_t	FE_STV0913_PILOTS_t;
	typedef FE_Sat_Rate_t	FE_STV0913_Rate_t;
	typedef FE_Sat_RollOff_t	FE_STV0913_RollOff_t;
	typedef	FE_Sat_Search_IQ_Inv_t	FE_STV0913_Search_IQ_Inv_t;
	typedef FE_Sat_IQ_Inversion_t	FE_STV0913_IQ_Inversion_t;

	typedef FE_TS_OutputMode_t FE_STV0913_Clock_t;
	typedef	FE_TS_Config_t	 FE_STV0913_TSConfig_t;

	typedef FE_Sat_DiseqC_TxMode FE_STV0913_DiseqC_TxMode;



	/****************************************************************
						INIT STRUCTURES
			structure passed to the FE_STV0900_Init() function
	 ****************************************************************/

	typedef FE_Sat_InitParams_t FE_STV0913_InitParams_t;



	/****************************************************************
						SEARCH STRUCTURES
	 ****************************************************************/



	typedef FE_Sat_SearchParams_t FE_STV0913_SearchParams_t;


	typedef FE_Sat_SearchResult_t FE_STV0913_SearchResult_t;


	/************************
		INFO STRUCTURE
	************************/
	typedef FE_Sat_SignalInfo_t FE_STV0913_SignalInfo_t;


	/****************************************************************
						INTERNAL PARAMS STRUCTURES
	 ****************************************************************/
	typedef struct  {
		/*HAL*/
		STCHIP_Handle_t hDemod; /*	Handle to the chip*/
		TUNER_Handle_t	hTuner; /*	Handle to the tuner 1*/
		STCHIP_Handle_t hVGLNA; /*	Handle to the chip VGLNA */

		/*Clocks and quartz General Params*/
		S32	Quartz,		/*	Demod Refernce frequency(Hz) */
			MasterClock; /*	Master clock frequency(Hz) */
		/* manual RollOff for DVBS1/DSS only*/
		FE_STV0913_RollOff_t	RollOff;
		/*Demod 1*/
		FE_STV0913_Tuner_t	TunerType; /* STV611x or SW_TUNER */
		/* Global I, Q inversion I, Q conection from tuner to demod*/
		FE_STV0913_IQ_Inversion_t	TunerGlobal_IQ_Inv;

		S32		TunerFrequency,	/*Current tuner freq(KHz) */
				TunerOffset,	/*relative to carrier(Hz)*/
				TunerBW,	/*Current bandwidth tun(Hz)*/
				DemodSymbolRate,/*	Symbol rate(Bds) */
				DemodSearchRange,/*	Search range(Hz) */
				TunerIndexJump;
		/* Algorithm for search Blind, Cold or Warm*/
		FE_STV0913_SearchAlgo_t		DemodSearchAlgo;
		/* Search standard: Auto, DVBS1/DSS only or DVBS2 only*/
		FE_STV0913_SearchStandard_t	DemodSearchStandard;
		/* I,Q  inversion: auto,auto norma first,normal/inverted*/
		FE_STV0913_Search_IQ_Inv_t	DemodSearch_IQ_Inv;
		FE_STV0913_ModCod_t		DemodModcode;
		FE_STV0913_Modulation_t		DemodModulation;
		FE_STV0913_Rate_t		DemodPunctureRate;
		/* Results of the search	*/
		FE_STV0913_SearchResult_t	DemodResults;
		/*Current state of the search algo*/
		FE_STV0913_SIGNALTYPE_t	DemodState;

		FE_STV0913_Error_t	DemodError; /*Last error encountered*/

		#ifdef CHIP_STAPI
			U32 TopLevelHandle;
		#endif
		U32	DiSEqC_ENV_Mode_Selection;
} FE_STV0913_InternalParams_t;



	/****************************************************************
						API FUNCTIONS
	****************************************************************/

	ST_Revision_t FE_STV0913_GetRevision(void);

	#ifdef CHIP_STAPI
		STCHIP_Error_t FE_STV0913_RepeaterFn(STCHIP_Handle_t hChip,
					BOOL State, unsigned char *Buffer);
	#else
		STCHIP_Error_t FE_STV0913_RepeaterFn(STCHIP_Handle_t hChip,
								BOOL State);
	#endif

	FE_STV0913_Error_t FE_STV0913_SetStandby(FE_STV0913_Handle_t Handle,
								U8 StandbyOn);

	FE_STV0913_Error_t FE_STV0913_SetAbortFlag(FE_STV0913_Handle_t Handle,
								BOOL Abort);

	FE_STV0913_Error_t	FE_STV0913_Init(FE_STV0913_InitParams_t	*pInit,
						FE_STV0913_Handle_t *Handle);

	FE_STV0913_Error_t	FE_STV0913_SetTSoutput(FE_STV0913_Handle_t
					Handle,
					FE_STV0913_TSConfig_t *PathpTSConfig);

	FE_STV0913_Error_t FE_STV0913_SetTSConfig(FE_STV0913_Handle_t Handle,
					FE_STV0913_TSConfig_t *PathpTSConfig,
					U32 *PathTSSpeed_Hz);

	FE_STV0913_Error_t	FE_STV0913_Search(FE_STV0913_Handle_t	Handle,
					FE_STV0913_SearchParams_t *pSearch,
					FE_STV0913_SearchResult_t *pResult,
					U32	Satellite_Scan);

	BOOL	FE_STV0913_Status(FE_STV0913_Handle_t	Handle);

	FE_STV0913_Error_t FE_STV0913_Unlock(FE_STV0913_Handle_t Handle);

	FE_STV0913_Error_t FE_STV0913_ResetDevicesErrors(FE_STV0913_Handle_t
								Handle);

	FE_STV0913_Error_t	FE_STV0913_GetSignalInfo(FE_STV0913_Handle_t
					Handle,
					FE_STV0913_SignalInfo_t	*pInfo,
					U32 Satellite_Scan);

	FE_STV0913_Error_t FE_STV0913_Tracking(FE_STV0913_Handle_t Handle,
					FE_Sat_TrackingInfo_t *pTrackingInfo);

	FE_STV0913_Error_t FE_STV913_GetPacketErrorRate(FE_STV0913_Handle_t
					Handle,
					U32 *PacketsErrorCount,
					U32 *TotalPacketsCount);

	FE_STV0913_Error_t FE_STV913_GetPreViterbiBER(FE_STV0913_Handle_t
					Handle,
					U32 *PreViterbiBER);

	FE_STV0913_Error_t FE_STV0913_SetMclk(FE_STV0913_Handle_t Handle,
					U32 Mclk, U32 ExtClk);

	FE_STV0913_Error_t FE_STV0913_DVBS2_SetGoldCodeX(FE_STV0913_Handle_t
					Handle,
					U32 GoldCode);

	FE_STV0913_Error_t FE_STV0913_SetupFSK(FE_STV0913_Handle_t Handle,
					U32 TransmitCarrier,
					U32 ReceiveCarrier, U32 Deltaf);

	FE_STV0913_Error_t FE_STV0913_EnableFSK(FE_STV0913_Handle_t Handle,
					BOOL EnableModulation,
					BOOL EnableDemodulation);

	FE_STV0913_Error_t FE_STV0913_DiseqcInit(FE_STV0913_Handle_t Handle,
					FE_STV0913_DiseqC_TxMode DiseqCMode);

	FE_STV0913_Error_t FE_STV0913_Set22KHzContinues(FE_STV0913_Handle_t
					Handle, BOOL Enable);

	FE_STV0913_Error_t FE_STV0913_DiseqcSend(FE_STV0913_Handle_t Handle,
					U8 *Data, U32 NbData);

	FE_STV0913_Error_t FE_STV0913_DiseqcReceive(FE_STV0913_Handle_t Handle,
					U8 *Data, U32 *NbData);


	FE_STV0913_Error_t	FE_STV0913_Term(FE_STV0913_Handle_t	Handle);


	/*FE_STV0913_Error_t FE_STV0913_SetContinous_Tone(FE_STV0913_Handle_t
				Handle, STFRONTEND_LNBToneState_t tonestate);*/

#endif /* _STV0913_DRV_H */
