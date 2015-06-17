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

Source file name : lla_utils.h
Author :           Rahul

LLA generic file

Date        Modification                                    Name
----        ------------                                    --------
23-Apr-12   Created                                         Rahul

************************************************************************/
#ifndef _LLA_UTILS_H
#define _LLA_UTILS_H

#include "fesat_commlla_str.h"
#include "fecab_commlla_str.h"
#include "feter_commlla_str.h"

/* General */
TUNER_Model_t stmfe_set_tuner_model(char *tuner_name);
FE_TS_OutputMode_t stmfe_set_ts_output_mode(enum demod_tsout_config_e ts_out);
FE_Sat_IQ_Inversion_t stmfe_set_tuner_iq_inv(bool is_inv);
int32_t stmfe_set_tuner_bbgain(bool is_vglna, TUNER_Model_t TunerModel);
TUNER_WIDEBandS_t stmfe_set_tuner_band(TUNER_Model_t TunerModel);
void stmfe_set_ts_config(enum demod_tsout_config_e ts_out, uint32_t ts_clock,
		int path, FE_TS_Config_t *llaconf);

/* Sat specific */
FE_Sat_Tuner_t stmfe_set_sat_tuner_type(char *tuner_name, bool is_auto);
FE_Sat_RollOff_t stmfe_set_sat_roll_off(enum demod_roll_off_e roll_off);
FE_Sat_Rate_t stmfe_set_sat_fec_rate(stm_fe_demod_fec_rate_t fec_rate);
FE_Sat_Search_IQ_Inv_t
	stmfe_set_sat_iq_inv(stm_fe_demod_spectral_inversion_t inv);

stm_fe_demod_tx_std_t stmfe_get_sat_standard(FE_Sat_TrackingStandard_t std);
stm_fe_demod_fec_rate_t stmfe_get_sat_fec_rate(FE_Sat_TrackingStandard_t std,
			FE_Sat_Rate_t puncture_rate, FE_Sat_ModCod_t modCode);
stm_fe_demod_modulation_t stmfe_get_sat_modulation(stm_fe_demod_tx_std_t std,
		FE_Sat_ModCod_t modCode);
stm_fe_demod_roll_off_t stmfe_get_sat_roll_off(FE_Sat_RollOff_t roll_off);

#endif /* _LLA_UTILS_H */
