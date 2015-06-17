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

Source file name : tuner_wrapper.h
Author :           Ankur

Io header for LLA to access the hw registers

Date        Modification                                    Name
----        ------------                                    --------
05-Aug-11   Created                                         Ankur

************************************************************************/
#ifndef _TUNER_WRAPPER_H
#define _TUNER_WRAPPER_H

#include <i2c_wrapper.h>
#include <stm_fe_demod.h>

struct tuner_ops_s {
	TUNER_Error_t (*init) (void *pTunerInit_v,
				    STCHIP_Handle_t *TunerHandle);
	void (*set_referenceFreq) (STCHIP_Handle_t hChip,
					u_int32_t Reference);
	TUNER_Error_t (*set_gain) (STCHIP_Handle_t tuner_h, int32_t Gain);
	int32_t (*get_gain) (STCHIP_Handle_t tuner_h);
	int32_t (*get_rfgain) (STCHIP_Handle_t tuner_h, u_int32_t agcint,
						int32_t bbgain);
	TUNER_Error_t (*set_frequency) (STCHIP_Handle_t tuner_h,
					     u_int32_t Frequency);
	TUNER_Error_t (*set_bandWidth) (STCHIP_Handle_t tuner_h,
					     u_int32_t Bandwidth);
	TUNER_Error_t (*switch_input)(STCHIP_Handle_t tuner_h,
					     u_int8_t src_num);
	TUNER_Error_t (*set_standby) (STCHIP_Handle_t tuner_h,
							    u_int8_t StandbyOn);
	TUNER_Error_t (*set_lna) (STCHIP_Handle_t hTuner);
	TUNER_Error_t (*start_and_calibrate) (STCHIP_Handle_t hTuner);
	TUNER_Error_t (*adjust_rfpower) (STCHIP_Handle_t hTuner,
								  int AGC2VAL2);
	bool (*get_status) (STCHIP_Handle_t tuner_h);
	int32_t (*get_rflevel) (STCHIP_Handle_t hTuner, u_int16_t RFAGC,
							       u_int16_t IFAGC);
	u_int32_t (*get_frequency) (STCHIP_Handle_t tuner_h);
	TUNER_Error_t (*term) (STCHIP_Handle_t tuner_h);
	TUNER_Error_t (*switch_to_dvbt) (STCHIP_Handle_t tuner_h);
	TUNER_Error_t (*switch_to_dvbc) (STCHIP_Handle_t tuner_h);
	TUNER_Error_t (*set_hmr_filter) (STCHIP_Handle_t tuner_h,
								int32_t value);
	TUNER_Error_t (*set_lnainput)(STCHIP_Handle_t tuner_h,
						TUNER_RFSource_t tunerinput);
	int (*detach) (STCHIP_Handle_t tuner_h);
};

#endif /* _TUNER_WRAPPER_H */
