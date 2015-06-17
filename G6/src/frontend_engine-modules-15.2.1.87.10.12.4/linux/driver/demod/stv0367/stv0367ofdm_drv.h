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

Source file name : stv0367ofdm_drv.h
Author :           LLA

367 lla drv file

Date        Modification                                    Name
----        ------------                                    --------
30-Jul-11   Created
04-Jul-12   Updated to v3.5
************************************************************************/
#ifndef _STV0367OFDM_DRV_H
#define _STV0367OFDM_DRV_H

#include "stv0367ofdm_init.h"
#ifdef HOST_PC
#include <gen_types.h>
#include <feter_commlla_str.h>
#include <fecab_commlla_str.h>
#include <fe_tc_tuner.h>

#ifndef NO_GUI
/*#include <RF4000Init.h> */
/* Contains RF4000 initial values + library function proto-types. */
#endif
#else
/*
#include <stddefs.h>
#include <dbtypes.h>
#include <sttuner.h>
*/
#endif
#ifdef CHIP_STAPI
#include <stfe_utilities.h>
#include <fe_tc_tuner.h>
#include <feter_commlla_str.h>
#endif
typedef InternalParamsPtr FE_367ofdm_Handle_t;

/****************************************************************
	COMMON STRUCTURES AND TYPEDEFS
 ****************************************************************/

/*STCHIP_Error_t STV367ofdm_RepeaterFn(STCHIP_Handle_t hChip,BOOL State); */
#ifdef STFRONTEND_USE_NEW_I2C
STCHIP_Error_t STV367ofdm_RepeaterFn(STCHIP_Handle_t hChip, BOOL State,
				     unsigned char *Buffer);
#else
STCHIP_Error_t STV367ofdm_RepeaterFn(STCHIP_Handle_t hChip, BOOL State);
#endif

typedef enum {
	FE_TER_NOAGC = 0,
	FE_TER_AGCOK = 5,
	FE_TER_NOTPS = 6,
	FE_TER_TPSOK = 7,
	FE_TER_NOSYMBOL = 8,
	FE_TER_BAD_CPQ = 9,
	FE_TER_PRFOUNDOK = 10,
	FE_TER_NOPRFOUND = 11,
	FE_TER_LOCKOK = 12,
	FE_TER_NOLOCK = 13,
	FE_TER_SYMBOLOK = 15,
	FE_TER_CPAMPOK = 16,
	FE_TER_NOCPAMP = 17,
	FE_TER_SWNOK = 18
} FE_367dvbt_SIGNALTYPE_t;

typedef enum {
	FE_TER_NOT_FORCED,
	FE_TER_WAIT_TRL,
	FE_TER_WAIT_AGC,
	FE_TER_WAIT_SYR,
	FE_TER_WAIT_PPM,
	FE_TER_WAIT_TPS,
	FE_TER_MONITOR_TPS,
	FE_TER_RESERVED
} FE_367dvbt_State_Machine_t;

typedef struct {
	char DemodName[20];	/* demod name */
	U8 DemodI2cAddr;	/* demod I2C address */
	TUNER_Model_t TunerModel;	/* Tuner model */
	char TunerName[20];	/* Tuner name */
	U8 TunerI2cAddr;	/* Tuner I2c address */
	U8 TunerI2cAddr2;	/* Tuner I2c address */
	FE_TS_ClockPolarity_t ClockPolarity;	/* Parity ytes */
	FE_TS_Config_t PathTSMode;	/* TS Format */
	FE_TS_OutputMode_t PathTSClock;
	U32 Demod_Crystal_Hz;	/* XTAL value */
	U32 Tuner_Crystal_Hz;	/* XTAL value */
	U32 TunerIF_kHz;	/* Tuner IF value */
	FE_TER_IF_IQ_Mode IFmode;
} FE_TER_InitParams_t;

typedef struct {
	/* hChip must be the first item in the structure !!! */
	STCHIP_Handle_t hDemod;	/* Handle to the chip */
	STCHIP_Handle_t hTuner;	/* Handle to the tuner */
	STCHIP_Handle_t hTuner2;	/* Handle to the tuner */
	FE_367dvbt_SIGNALTYPE_t State;	/* Current state of the search algo */
	FE_367dvbt_State_Machine_t StateMachine;	/* State machine */
	FE_TER_IF_IQ_Mode IF_IQ_Mode;
	FE_TER_FFT_t FFTSize;	/* Mode 2K or 8K */
	FE_TER_Guard_t Guard;	/* Guard interval */
	FE_TER_Hierarchy_t Hierarchy;
	U32 Frequency;		/* Current tuner frequency (KHz) */
	U8 I2Cspeed;		/* */
	FE_Spectrum_t Inv; /* 0 no spectrum inverted search to be perfomed */
	U8 Sense;		/* current search,spectrum not inveerted */
	U8 Force;		/* force mode/guard */
	U8 ChannelBW_MHz;	/* channel width */
	U8 PreviousChannelBW;	/* channel width used during previous lock */
	U32 PreviousBER;
	U32 PreviousPER;
	S8 EchoPos;		/* echo position */
	U8 first_lock;		/* */
	S32 Crystal_Hz;		/* XTAL value */
	FE_TER_SearchResult_t Results;	/* Results of the search */
	U8 Unlockcounter;
	BOOL SpectrumInversion;
	FE_DEMOD_Model_t DemodModel;	/* Demod that is used with the tuner */
} FE_367ofdm_InternalParams_t;

/****************************************************************
					API FUNCTIONS
****************************************************************/

FE_LLA_Error_t FE_367ofdm_Init(FE_TER_InitParams_t *pInit,
			       FE_367ofdm_Handle_t *Handle);

FE_LLA_Error_t FE_367ofdm_Search(FE_367ofdm_Handle_t Handle,
				 FE_TER_SearchParams_t *pSearch,
				 FE_TER_SearchResult_t *pResult);

FE_LLA_Error_t FE_367ofdm_LookFor(FE_367ofdm_Handle_t Handle,
				  FE_TER_SearchParams_t *pParams,
				  FE_TER_SearchResult_t *pResult);
FE_LLA_Error_t GetTunerInfo(FE_367ofdm_Handle_t Handle,
			    FE_TER_SearchParams_t *pParams,
			    FE_TER_SearchResult_t *pResult);
BOOL FE_367ofdm_Status(FE_367ofdm_Handle_t Handle);
FE_LLA_Error_t FE_367ofdm_GetSignalInfo(FE_367ofdm_Handle_t Handle,
					FE_TER_SignalInfo_t *pInfo);

FE_LLA_Error_t FE_367ofdm_Term(FE_367ofdm_Handle_t Handle);
void FE_367ofdm_Tracking(FE_367ofdm_InternalParams_t *pParams);
int SpeedInit(STCHIP_Handle_t hChip);
void SET_TRLNOMRATE_REGS(STCHIP_Handle_t hChip, short unsigned int value);
short unsigned int GET_TRLNOMRATE_VALUE(STCHIP_Handle_t hChip);
signed int GET_TRL_OFFSET(STCHIP_Handle_t hChip);
int duration(S32 mode, int tempo1, int tempo2, int tempo3);
U32 FE_367ofdm_GetMclkFreq(STCHIP_Handle_t hChip, U32 ExtClk_Hz);
U32 FE_367ofdm_GetBerErrors(FE_367ofdm_InternalParams_t *pParams);
U32 FE_367ofdm_GetPerErrors(FE_367ofdm_InternalParams_t *pParams);
U32 FE_367ofdm_GetErrors(FE_367ofdm_InternalParams_t *pParams);
FE_LLA_Error_t FE_367ofdm_SetAbortFlag(FE_367ofdm_Handle_t Handle, BOOL Abort);
FE_LLA_Error_t FE_STV0367ofdm_SetStandby(FE_367ofdm_Handle_t Handle,
					 U8 StandbyOn);
FE_LLA_Error_t FE_367ofdm_SwitchDemodToDVBT(FE_367ofdm_Handle_t Handle);
BOOL FE_367ofdm_lock(FE_367ofdm_Handle_t Handle);

void demod_get_pd(void *dummy_handle, unsigned short *level,
		  STCHIP_Handle_t hTuner);
void demod_get_agc(void *dummy_handle, U16 *rf_agc, U16 *bb_agc,
		   STCHIP_Handle_t hTuner);
void demod_set_agclim(void *dummy_handle, U16 dir_up, STCHIP_Handle_t hTuner);

#if defined(HOST_PC) && !defined(NO_GUI)
int FE_367_IIR_FILTER_INIT(STCHIP_Handle_t hChip, U8 Bandwidth,
			   U32 DemodXtalValue);
#endif

#ifdef HOST_PC
void FE_367ofdm_Core_Switch(STCHIP_Handle_t hChip);
FE_LLA_Error_t FE_367ofdm_TunerSet(FE_367ofdm_InternalParams_t *pParams);

STCHIP_Error_t STV367ofdm_RepeaterFn(STCHIP_Handle_t hChip, BOOL State);
/*int FilterCoeffInit(STCHIP_Handle_t hChip,U16 CellsCoeffs[6][5]); */
#endif

#endif /* _STV0367OFDM_DRV_H */
