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

Source file name : stv6110_tuner.h
Author :           LLA

tuner lla file

Date        Modification                                    Name
----        ------------                                    --------
20-Jun-11   Created                                         Shobhit

************************************************************************/
#ifndef _STV6110_TUNER_H
#define _STV6110_TUNER_H

/* #include <chip.h> */
/* #include "fe_sat_tuner.h" */
#if defined(CHIP_STAPI)		/*For STAPI use only */
/* #include <dbtypes.h>  for Install function*/
/* #include <stfrontend.h>    for Install Function*/
#endif

#define	STV6110_NBREGS 8	/*Number fo registers in STV6110 */
#define STV6110_NBFIELDS 22	/*Number of fields in STV6110 */

/* functions --------------------------------------------------------------- */

/* create instance of tuner register mappings */

TUNER_Error_t STV6110_TunerInit(void *pTunerInit,
				STCHIP_Handle_t *TunerHandle);
TUNER_Error_t STV6110_TunerSetStandby(STCHIP_Handle_t tuner_h, U8 StandbyOn);

TUNER_Error_t STV6110_TunerSetFrequency(STCHIP_Handle_t tuner_h, U32 Frequency);
U32 STV6110_TunerGetFrequency(STCHIP_Handle_t tuner_h);
TUNER_Error_t STV6110_TunerSetBandwidth(STCHIP_Handle_t tuner_h, U32 Bandwidth);
U32 STV6110_TunerGetBandwidth(STCHIP_Handle_t tuner_h);
BOOL STV6110_TunerGetStatus(STCHIP_Handle_t tuner_h);
TUNER_Error_t STV6110_TunerWrite(STCHIP_Handle_t tuner_h);
TUNER_Error_t STV6110_TunerRead(STCHIP_Handle_t tuner_h);
TUNER_Error_t STV6110_TunerSetGain(STCHIP_Handle_t tuner_h, S32 Gain);
S32 STV6110_TunerGetGain(STCHIP_Handle_t tuner_h);
TUNER_Error_t STV6110_TunerSetOutputClock(STCHIP_Handle_t tuner_h, S32 Divider);

TUNER_Error_t STV6110_TunerSetAttenuator(STCHIP_Handle_t tuner_h,
					 BOOL AttenuatorOn);
BOOL STV6110_TunerGetAttenuator(STCHIP_Handle_t tuner_h);

TUNER_Model_t STV6110_TunerGetModel(STCHIP_Handle_t tuner_h);
void STV6110_TunerSetIQ_Wiring(STCHIP_Handle_t tuner_h, S8 IQ_Wiring);
S8 STV6110_TunerGetIQ_Wiring(STCHIP_Handle_t tuner_h);
void STV6110_TunerSetReferenceFreq(STCHIP_Handle_t tuner_h, U32 Reference);
U32 STV6110_TunerGetReferenceFreq(STCHIP_Handle_t tuner_h);
void STV6110_TunerSetIF_Freq(STCHIP_Handle_t tuner_h, U32 IF);
U32 STV6110_TunerGetIF_Freq(STCHIP_Handle_t tuner_h);
void STV6110_TunerSetBandSelect(STCHIP_Handle_t hChip, U8 BandSelect);
U8 STV6110_TunerGetBandSelect(STCHIP_Handle_t hChip);

TUNER_Error_t STV6110_TunerTerm(STCHIP_Handle_t tuner_h);

#if defined(CHIP_STAPI)		/*For STAPI use only */
/* ST_ErrorCode_t STFRONTEND_TUNER_STV6110_Install(STFRONTEND_tunerDbase_t */
/* *Tuner, STFRONTEND_TunerType_t TunerType); */
/* ST_ErrorCode_t STFRONTEND_TUNER_STV6110_Uninstall(STFRONTEND_tunerDbase_t */
/* *Tuner); */
#endif

#endif /* _STV6110_TUNER_H */
