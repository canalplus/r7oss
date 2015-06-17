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

Source file name :stv4100_tuner.h
Author :           LLA

tuner lla file

Date        Modification                                    Name
----        ------------                                    --------
01-Aug-11   Created
04-Jul-12   Updated
************************************************************************/
#ifndef _STV4100_TUNER_H
#define _STV4100_TUNER_H

/* #include <chip.h> */
#include "fe_tc_tuner.h"
#if defined(CHIP_STAPI) /*For STAPI use only*/
/* #include <dbtypes.h>	for Install function*/
/* #include <stfrontend.h>    for Install Function*/
#endif
/* constants --------------------------------------------------------------- */
/*STV4100 tuner definition*/
#define STV4100_NBREGS		  15
#define STV4100_RW_REGS		  12
#define STV4100_NBFIELDS	  51

/* structures --------------------------------------------------------------- */
typedef struct {
	U32	Fxtal_Hz;
	U32	LNAConfig;
	TUNER_Band_t	BandSelect;
	U8	CalValue;
	U8	CLPF_CalValue;
	TUNER_IQ_t	IQ_Wiring;	/* hardware I/Q invertion */

} stv4100_config;

/* functions --------------------------------------------------------------- */

TUNER_Error_t STV4100_TunerInit(void *pTunerInit_v,
						STCHIP_Handle_t *TunerHandle);
TUNER_Error_t STV4100_TunerSetFrequency(STCHIP_Handle_t hTuner, U32 Frequency);
U32 STV4100_TunerGetFrequency(STCHIP_Handle_t hTuner);
BOOL STV4100_TunerGetStatus(STCHIP_Handle_t hTuner);
TUNER_Error_t STV4100_TunerWrite(STCHIP_Handle_t hTuner);
TUNER_Error_t STV4100_TunerRead(STCHIP_Handle_t hTuner);
TUNER_Error_t STV4100_TunerTerm(STCHIP_Handle_t hTuner);
TUNER_Model_t STV4100_TunerGetModelName(STCHIP_Handle_t hTuner);
void STV4100_TunerSetIQ_Wiring(STCHIP_Handle_t hChip, TUNER_IQ_t IQ_Wiring);
TUNER_IQ_t STV4100_TunerGetIQ_Wiring(STCHIP_Handle_t hChip);
U32 STV4100_TunerGetReferenceFreq(STCHIP_Handle_t hChip);
void STV4100_TunerSetReferenceFreq(STCHIP_Handle_t hChip, U32 reference);
void STV4100_TunerSetIF_Freq(STCHIP_Handle_t hChip, U32 IF);
U32 STV4100_TunerGetLNAConfig(STCHIP_Handle_t hChip);
void STV4100_TunerSetLNAConfig(STCHIP_Handle_t hChip, U32 LNA_Config);
U32 STV4100_TunerGetUHFSens(STCHIP_Handle_t hChip);
void STV4100_TunerSetUHFSens(STCHIP_Handle_t hChip, U32 UHF_Sens);
/*void STV4100_TunerSetXtalValue(STCHIP_Handle_t hChip, U32 TunerXtal);
U32 STV4100_TunerGetXtalValue(STCHIP_Handle_t hChip);*/
TUNER_Error_t STV4100_TunerStartAndCalibrate(STCHIP_Handle_t hTuner);
TUNER_Error_t STV4100_TunerSetLna(STCHIP_Handle_t hTuner);
TUNER_Error_t STV4100_TunerAdjustRfPower(STCHIP_Handle_t hTuner, int DeltaAGC);
S32 STV4100_TunerEstimateRfPower(STCHIP_Handle_t hTuner, U16 RfAgc, U16 IfAgc);
TUNER_Error_t STV4100_TunerSetStandby(STCHIP_Handle_t hTuner, U8 StandbyOn);
TUNER_Error_t STV4100_TunerSetBandWidth(STCHIP_Handle_t hTuner, U32 Band_Width);
U32 STV4100_TunerGetBandWidth(STCHIP_Handle_t hTuner);
TUNER_Error_t STV4100_SwitchTunerToDVBT(STCHIP_Handle_t hChip);
TUNER_Error_t STV4100_SwitchTunerToDVBC(STCHIP_Handle_t hChip);

/* #if defined(CHIP_STAPI) For STAPI use only*/
/* ST_ErrorCode_t STFRONTEND_TUNER_STV4100_Install(STFRONTEND_tunerDbase_t
 * *Tuner, STFRONTEND_TunerType_t TunerType); */
/* ST_ErrorCode_t STFRONTEND_TUNER_STV4100_Uninstall(STFRONTEND_tunerDbase_t
 * *Tuner); */
/* #endif */
/*End of file stv4100_tuner.h*/
#endif /* _STV4100_TUNER_H */
