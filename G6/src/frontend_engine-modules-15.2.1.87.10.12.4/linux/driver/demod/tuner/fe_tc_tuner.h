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

Source file name : fe_sat_tuner.h
Author :           LLA

LLA generic file

Date        Modification                                    Name
----        ------------                                    --------
01-Jun-11   Created                                         Ankur

************************************************************************/
#ifndef _FE_TC_TUNER_H
#define _FE_TC_TUNER_H

/* #include <chip.h> */
/* #include <stfe_utilities.h> */
/* #include <sony_dvb_tuner.h> */
/* #include <sony_dvb_ascot2s.h> */
/* #include <sony_dvb_ascot2d.h> */
/* structures -------------------------------------------------------------- */

typedef struct {
	TUNER_Model_t Model;	/* Tuner model */
	ST_String_t TunerName;	/* Tuner name */
	U8 TunerI2cAddress;	/* Tuner I2C address */
	U8 *DefVal;		/* pointer to table of default values */
	TUNER_Band_t BandSelect;
	U32 Fxtal_Hz;		/* reference frequency (Hz) */
	U32 IF;			/* Intermediate frequency (KHz) */
	TUNER_IQ_t IQ_Wiring;	/* hardware I/Q invertion */
	TUNER_IQ_t SpectrInvert;	/* tuner invert spectrum */
	U32 FirstIF;		/* First IF frequency for Microtune tuner */
	U32 LNAConfig;		/* Setting of the LNA in Microtune tuners */
	U32 UHFSens;		/* Sensitivity Setting in Microtune tuners */
	U32 StepSize;
	U32 BandWidth;
	/* pointer to any specific data of a specific tuner */
	void *pAddParams;
	U32 Tunermode;		/* bitmask to switch tuner configurations */
	TUNER_Lock_t Lock;
	FE_DEMOD_Model_t DemodModel;	/* Demod that is used with the tuner */
	STCHIP_Handle_t RepeaterHost;
	STCHIP_RepeaterFn_t RepeaterFn;
	bool Repeater;		/* Is repeater enabled or not ? */
	FE_TransmissionStandard_t TransmitStandard;
#ifdef CHIP_STAPI
	U32 TopLevelHandle;	/*Used in STTUNER : Change AG */
#endif
/* sony_dvb_tuner_t pTuner; */
/* sony_ascot2s_t ascot2s; */
/* sony_ascot2d_t ascot2d; */
} TER_TUNER_InitParams_t;

typedef TER_TUNER_InitParams_t *TER_TUNER_Params_t;
typedef TER_TUNER_InitParams_t *TUNER_Params_t;	/* for T+C generic purpose */
/* for T+C generic purpose */
typedef TER_TUNER_InitParams_t TUNER_InitParams_t;

TUNER_Error_t FE_TunerInit(void *pTunerInit_v, STCHIP_Handle_t *TunerHandle);
TUNER_Error_t FE_TunerSetFrequency(STCHIP_Handle_t hTuner, U32 Frequency);
U32 FE_TunerGetFrequency(STCHIP_Handle_t hTuner);
TUNER_Error_t FE_TunerSetStepsize(STCHIP_Handle_t hTuner, U32 Stepsize);
U32 FE_TunerGetStepsize(STCHIP_Handle_t hChip);
BOOL FE_TunerGetStatus(STCHIP_Handle_t hTuner);
/* maxim only */
TUNER_Error_t TunerChangeBBBW(STCHIP_Handle_t hTuner, U32 Frequency,
								U8 bIncrement);
TUNER_Error_t FE_TunerWrite(STCHIP_Handle_t hTuner);
TUNER_Error_t FE_TunerRead(STCHIP_Handle_t hTuner);
TUNER_Error_t FE_TunerTerm(STCHIP_Handle_t hChip);
U32 FE_TunerGetATC(STCHIP_Handle_t hTuner);
TUNER_Error_t FE_TunerSetATC(STCHIP_Handle_t hTuner, U32 atc);
U32 FE_TunerGetAGC(STCHIP_Handle_t hTuner);
TUNER_Error_t FE_TunerSetAGC(STCHIP_Handle_t hTuner, U32 agc);
/* U32           FE_TunerGetXtalValue(STCHIP_Handle_t hTuner);
TUNER_Error_t FE_TunerSetXtalValue(STCHIP_Handle_t hTuner,U32 xtal_tuner);*/
U32 FE_TunerGetCP(STCHIP_Handle_t hTuner);
TUNER_Error_t FE_TunerSetCP(STCHIP_Handle_t hTuner, U32 cp);
U32 FE_TunerGetTOP(STCHIP_Handle_t hTuner);
TUNER_Error_t FE_TunerSetTOP(STCHIP_Handle_t hTuner, U32 top);
U32 FE_TunerGetIF_Freq(STCHIP_Handle_t hChip);
TUNER_Model_t FE_TunerGetModelName(STCHIP_Handle_t hChip);
void FE_TunerSetIQ_Wiring(STCHIP_Handle_t hChip, TUNER_IQ_t IQ_Wiring);
TUNER_IQ_t FE_TunerGetIQ_Wiring(STCHIP_Handle_t hChip);
U32 FE_TunerGetReferenceFreq(STCHIP_Handle_t hTuner);
void FE_TunerSetReferenceFreq(STCHIP_Handle_t hTuner, U32 reference);
void FE_TunerSetIF_Freq(STCHIP_Handle_t hChip, U32 IF);
U32 FE_TunerGetLNAConfig(STCHIP_Handle_t hChip);
void FE_TunerSetLNAConfig(STCHIP_Handle_t hChip, U32 LNA_Config);
U32 FE_TunerGetUHFSens(STCHIP_Handle_t hChip);
void FE_TunerSetUHFSens(STCHIP_Handle_t hChip, U32 UHF_Sens);
U32 FE_TunerGetBandWidth(STCHIP_Handle_t hChip);
TUNER_Error_t FE_TunerSetBandWidth(STCHIP_Handle_t hChip, U32 Band_Width);
TUNER_Error_t FE_TunerStartAndCalibrate(STCHIP_Handle_t hTuner);
TUNER_Error_t FE_TunerSetLna(STCHIP_Handle_t hTuner);
TUNER_Error_t FE_TunerAdjustRfPower(STCHIP_Handle_t hTuner, int Delta);
S32 FE_TunerEstimateRfPower(STCHIP_Handle_t hTuner, U16 Rf_Agc, U16 If_Agc);
TUNER_Error_t FE_TunerSetStandbyMode(STCHIP_Handle_t hTuner, U8 StandbyOn);
/* TUNER_Error_t FE_TunerSetStandBy(STCHIP_Handle_t hTuner,U8 StandbyOn); */
int32_t FE_TunerGetRFLevel(STCHIP_Handle_t hTuner, u_int8_t RFLevel,
			   int IFLevel);
BOOL FE_TunerGetPowerLevel(STCHIP_Handle_t hTuner, S32 *power);
TUNER_Error_t FE_TunerGetParam(STCHIP_Handle_t hTuner, Tuner_Param_t Param,
			       U32 *Value);
TUNER_Error_t FE_SwitchTunerToDVBT(STCHIP_Handle_t hTuner);
TUNER_Error_t FE_SwitchTunerToDVBC(STCHIP_Handle_t hTuner);
int stm_fe_stv4100_attach(struct stm_fe_demod_s *priv);
int stm_fe_stv4100_detach(STCHIP_Handle_t tuner_h);
int stm_fe_dtt7546_attach(struct stm_fe_demod_s *priv);
int stm_fe_dtt7546_detach(STCHIP_Handle_t tuner_h);
int stm_fe_tda18212_attach(struct stm_fe_demod_s *priv);
int stm_fe_tda18212_detach(STCHIP_Handle_t tuner_h);

#endif /* _FE_TC_TUNER_H */
