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

Source file name :dtt7546_tuner.h
Author :           LLA

tuner lla file

Date        Modification                                    Name
----        ------------                                    --------
01-Aug-11   Created

************************************************************************/
#ifndef _DTT7546_TUNER_H
#define _DTT7546_TUNER_H

/* #include <chip.h> */
#include "fe_tc_tuner.h"
#if defined(CHIP_STAPI)		/*For STAPI use only */
/* #include <dbtypes.h>  for Install function*/
/* #include <stfrontend.h>    for Install Function*/
#endif
/* constants --------------------------------------------------------------- */
/*Thomson DTT7546X tuner definition*/
#define DTT7546_NBREGS		6
#define DTT7546_NBFIELDS	19

/* functions --------------------------------------------------------------- */

TUNER_Error_t DTT7546_TunerInit(void *pTunerInit,
				STCHIP_Handle_t *TunerHandle);
TUNER_Error_t DTT7546_TunerSetFrequency(STCHIP_Handle_t hTuner, U32 Frequency);
U32 DTT7546_TunerGetFrequency(STCHIP_Handle_t hTuner);
TUNER_Error_t DTT7546_TunerSetStepsize(STCHIP_Handle_t hTuner, U32 Stepsize);
U32 DTT7546_TunerGetStepsize(STCHIP_Handle_t hTuner);
BOOL DTT7546_TunerGetStatus(STCHIP_Handle_t hTuner);
TUNER_Error_t DTT7546_TunerWrite(STCHIP_Handle_t hTuner);
TUNER_Error_t DTT7546_TunerRead(STCHIP_Handle_t hTuner);
U32 DTT7546_TunerGetATC(STCHIP_Handle_t hTuner);
TUNER_Error_t DTT7546_TunerSetATC(STCHIP_Handle_t hTuner, U32 atc);
TUNER_Error_t DTT7546_TunerTerm(STCHIP_Handle_t hTuner);
U32 DTT7546_TunerGetIF_Freq(STCHIP_Handle_t hTuner);
TUNER_Model_t DTT7546_TunerGetModelName(STCHIP_Handle_t hTuner);
void DTT7546_TunerSetIQ_Wiring(STCHIP_Handle_t hTuner, TUNER_IQ_t IQ_Wiring);
TUNER_IQ_t DTT7546_TunerGetIQ_Wiring(STCHIP_Handle_t hTuner);
U32 DTT7546_TunerGetReferenceFreq(STCHIP_Handle_t hTuner);
void DTT7546_TunerSetReferenceFreq(STCHIP_Handle_t hTuner, U32 Reference);
void DTT7546_TunerSetIF_Freq(STCHIP_Handle_t hTuner, U32 IF);
U32 DTT7546_TunerGetLNAConfig(STCHIP_Handle_t hChip);
void DTT7546_TunerSetLNAConfig(STCHIP_Handle_t hChip, U32 LNA_Config);
U32 DTT7546_TunerGetUHFSens(STCHIP_Handle_t hChip);
void DTT7546_TunerSetUHFSens(STCHIP_Handle_t hChip, U32 UHF_Sens);
U32 DTT7546_TunerGetBandWidth(STCHIP_Handle_t hChip);
TUNER_Error_t DTT7546_TunerSetBandWidth(STCHIP_Handle_t hChip, U32 Band_Width);
/* DTT7546 specific fcts*/
U32 DTT7546_TunerGetCP(STCHIP_Handle_t hChip);
TUNER_Error_t DTT7546_TunerSetCP(STCHIP_Handle_t hChip, U32 Cp);
U32 DTT7546_TunerGetTOP(STCHIP_Handle_t hChip);
TUNER_Error_t DTT7546_TunerSetTOP(STCHIP_Handle_t hChip, U32 top);
TUNER_Error_t DTT7546_SwitchTunerToDVBT(STCHIP_Handle_t hChip);
TUNER_Error_t DTT7546_SwitchTunerToDVBC(STCHIP_Handle_t hChip);

#if defined(CHIP_STAPI)		/*For STAPI use only */
/* ST_ErrorCode_t STFRONTEND_TUNER_DTT7546_Install(STFRONTEND_tunerDbase_t
 * *Tuner, STFRONTEND_TunerType_t TunerType); */
/* ST_ErrorCode_t STFRONTEND_TUNER_DTT7546_Uninstall(STFRONTEND_tunerDbase_t
 * *Tuner); */
#endif

#endif /* _DTT7546_TUNER_H */
/* End of dtt7546_tuner.h */
