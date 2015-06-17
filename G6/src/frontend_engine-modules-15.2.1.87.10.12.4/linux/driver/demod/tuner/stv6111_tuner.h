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

Source file name : stv6111_tuner.h
Author :			LLA

tuner lla file

Date		Modification				Name
----		------------				-------

************************************************************************/
#ifndef _STV6111_TUNER_H
#define _STV6111_TUNER_H

	#define	STV6111_NBREGS 16 /*Number fo registers in STV6111*/
	#define STV6111_NBFIELDS 52 /*Number of fields in STV6111*/


	/* functions ------------------------------------------- */

	/* create instance of tuner register mappings */

	TUNER_Error_t STV6111_TunerInit(void *pTunerInit,
						STCHIP_Handle_t *TunerHandle);
	TUNER_Error_t STV6111_TunerSetStandby(STCHIP_Handle_t hTuner,
								U8 StandbyOn);

	TUNER_Error_t STV6111_TunerSetFrequency(STCHIP_Handle_t hTuner,
								U32 Frequency);
	U32 STV6111_TunerGetFrequency(STCHIP_Handle_t hTuner);
	TUNER_Error_t STV6111_TunerSetBandwidth(STCHIP_Handle_t hTuner,
								U32 Bandwidth);
	U32 STV6111_TunerGetBandwidth(STCHIP_Handle_t hTuner);
	BOOL STV6111_TunerGetStatus(STCHIP_Handle_t hTuner);
	TUNER_Error_t STV6111_TunerWrite(STCHIP_Handle_t hTuner);
	TUNER_Error_t STV6111_TunerRead(STCHIP_Handle_t hTuner);
	TUNER_Error_t STV6111_TunerSetGain(STCHIP_Handle_t hTuner, S32 Gain);
	S32 STV6111_TunerGetGain(STCHIP_Handle_t hTuner);
	TUNER_Error_t STV6111_TunerSetOutputClock(STCHIP_Handle_t hTuner,
								S32 Divider);

	TUNER_Error_t STV6111_TunerSetAttenuator(STCHIP_Handle_t hTuner,
							BOOL AttenuatorOn);
	BOOL STV6111_TunerGetAttenuator(STCHIP_Handle_t hTuner);


	TUNER_Model_t	STV6111_TunerGetModel(STCHIP_Handle_t hTuner);
	void	STV6111_TunerSetIQ_Wiring(STCHIP_Handle_t hTuner, S8 IQ_Wiring);
	S8	STV6111_TunerGetIQ_Wiring(STCHIP_Handle_t hTuner);
	void	STV6111_TunerSetReferenceFreq(STCHIP_Handle_t hTuner,
								U32 Reference);
	U32	STV6111_TunerGetReferenceFreq(STCHIP_Handle_t hTuner);
	void	STV6111_TunerSetIF_Freq(STCHIP_Handle_t hTuner, U32 IF);
	U32	STV6111_TunerGetIF_Freq(STCHIP_Handle_t hTuner);
	void STV6111_TunerSetBandSelect(STCHIP_Handle_t hChip, U8 BandSelect);
	U8 STV6111_TunerGetBandSelect(STCHIP_Handle_t hChip);

	TUNER_Error_t STV6111_TunerSetMode(STCHIP_Handle_t hChip);
	void STV6111_TunerStart(STCHIP_Handle_t hTuner);
	S32 STV6111_TunerGetRFGain(STCHIP_Handle_t hTuner, U32 AGCIntegrator,
					S32 BBGain);
	TUNER_Error_t STV6111_TunerTerm(STCHIP_Handle_t hTuner);


#endif /* _STV6111_TUNER_H */
