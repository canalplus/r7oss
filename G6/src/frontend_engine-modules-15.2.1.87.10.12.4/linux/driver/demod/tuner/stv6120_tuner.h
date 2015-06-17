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

Source file name : stv6120_tuner.h
Author :           LLA

tuner lla file

Date        Modification                                    Name
----        ------------                                    --------
20-Jun-11   Created                                         Shobhit

************************************************************************/
#ifndef _STV6120_TUNER_H
#define _STV6120_TUNER_H

#define	STV6120_NBREGS 25 /*Number fo registers in STV6120*/
#define STV6120_NBFIELDS 59 /*Number of fields in STV6120*/

/* functions --------------------------------------------------------------- */

/* create instance of tuner register mappings */

TUNER_Error_t STV6120_TunerInit(void *pTunerInit, STCHIP_Handle_t *TunerHandle);
TUNER_Error_t STV6120_TunerSetStandby(STCHIP_Handle_t tuner_h, U8 StandbyOn);

TUNER_Error_t STV6120_TunerSetFrequency(STCHIP_Handle_t tuner_h, U32 Frequency);
U32 STV6120_TunerGetFrequency(STCHIP_Handle_t tuner_h);
TUNER_Error_t STV6120_TunerSetBandwidth(STCHIP_Handle_t tuner_h, U32 Bandwidth);
U32 STV6120_TunerGetBandwidth(STCHIP_Handle_t tuner_h);
BOOL STV6120_TunerGetStatus(STCHIP_Handle_t tuner_h);
TUNER_Error_t STV6120_TunerWrite(STCHIP_Handle_t tuner_h);
TUNER_Error_t STV6120_TunerRead(STCHIP_Handle_t tuner_h);
TUNER_Error_t STV6120_TunerSetGain(STCHIP_Handle_t tuner_h, S32 Gain);
S32 STV6120_TunerGetGain(STCHIP_Handle_t tuner_h);
TUNER_Error_t STV6120_TunerSetOutputClock(STCHIP_Handle_t tuner_h, S32 Divider);

TUNER_Error_t STV6120_TunerSetAttenuator(STCHIP_Handle_t tuner_h,
							     BOOL AttenuatorOn);
BOOL STV6120_TunerGetAttenuator(STCHIP_Handle_t tuner_h);

TUNER_Model_t STV6120_TunerGetModel(STCHIP_Handle_t tuner_h);
void STV6120_TunerSetIQ_Wiring(STCHIP_Handle_t tuner_h, S8 IQ_Wiring);
S8 STV6120_TunerGetIQ_Wiring(STCHIP_Handle_t tuner_h);
void STV6120_TunerSetReferenceFreq(STCHIP_Handle_t tuner_h, U32 Reference);
U32 STV6120_TunerGetReferenceFreq(STCHIP_Handle_t tuner_h);
void STV6120_TunerSetIF_Freq(STCHIP_Handle_t tuner_h, U32 IF);
U32 STV6120_TunerGetIF_Freq(STCHIP_Handle_t tuner_h);
void STV6120_TunerSetBandSelect(STCHIP_Handle_t hChip, U8 BandSelect);
U8 STV6120_TunerGetBandSelect(STCHIP_Handle_t hChip);

TUNER_Error_t STV6120_TunerSwitchInput(STCHIP_Handle_t hChip, U8 Input);
TUNER_Error_t STV6120_TunerSetHMR_Filter(STCHIP_Handle_t tuner_h,
							       S32 filterValue);

TUNER_Error_t STV6120_TunerTerm(STCHIP_Handle_t tuner_h);
TUNER_Error_t STV6120_TunerSetLNAInput(STCHIP_Handle_t hTuner,
						   TUNER_RFSource_t TunerInput);

#endif /* _STV6120_TUNER_H */
