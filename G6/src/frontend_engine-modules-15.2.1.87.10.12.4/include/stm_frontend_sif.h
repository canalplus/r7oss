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

Source file name : stm_frontend_sif.h
Author :           Rahul.V

stm_fe component header

Date        Modification                                    Name
----        ------------                                    --------
16-Jun-11   Created                                         Rahul.V

************************************************************************/

#ifndef _STM_FRONTEND_SIF_H
#define _STM_FRONTEND_SIF_H

typedef enum stm_fe_sif_audio_std_e {
	STM_FE_SIF_AUDIO_STD_UNKNOWN = (0),
	STM_FE_SIF_AUDIO_STD_AM_MONO = (1 << 0),
	STM_FE_SIF_AUDIO_STD_FM_MONO = (1 << 1),
	STM_FE_SIF_AUDIO_STD_NICAM_DUAL = (1 << 2),
	STM_FE_SIF_AUDIO_STD_NICAM_STEREO = (1 << 3),
	STM_FE_SIF_AUDIO_STD_NICAM_DATA = (1 << 4),
	STM_FE_SIF_AUDIO_STD_NICAM_MONO_DATA = (1 << 5),
	STM_FE_SIF_AUDIO_STD_ZWEITON_MONO = (1 << 6),
	STM_FE_SIF_AUDIO_STD_ZWEITON_DUAL = (1 << 7),
	STM_FE_SIF_AUDIO_STD_ZWEITON_STEREO = (1 << 8),
	STM_FE_SIF_AUDIO_STD_BTSC_MONO = (1 << 9),
	STM_FE_SIF_AUDIO_STD_BTSC_STEREO = (1 << 10),
	STM_FE_SIF_AUDIO_STD_BTSC_SAP = (1 << 11),
	STM_FE_SIF_AUDIO_STD_EIAJ_MONO = (1 << 12),
	STM_FE_SIF_AUDIO_STD_EIAJ_DUAL = (1 << 13),
	STM_FE_SIF_AUDIO_STD_EIAJ_STEREO = (1 << 14),
	STM_FE_SIF_AUDIO_STD_FM_RADIO_MONO = (1 << 15),
	STM_FE_SIF_AUDIO_STD_FM_RADIO_STEREO = (1 << 16),
	STM_FE_SIF_AUDIO_STD_AUTO
} stm_fe_sif_audio_std_t;

typedef enum stm_fe_sif_audio_output_mode_e {
	STM_FE_SIF_AUDIO_OUTPUT_MODE_MONO,
	STM_FE_SIF_AUDIO_OUTPUT_MODE_STEREO,
	STM_FE_SIF_AUDIO_OUTPUT_MODE_TRACK_A,
	STM_FE_SIF_AUDIO_OUTPUT_MODE_TRACK_B,
	STM_FE_SIF_AUDIO_OUTPUT_MODE_TRACK_AB_SUM,
	STM_FE_SIF_AUDIO_OUTPUT_MODE_TRACK_A_AND_B,
} stm_fe_sif_audio_output_mode_t;

typedef struct stm_fe_sif_config_s {
	uint32_t mute_on_carier_loss;
	uint32_t enable_nicam_auto_switch;
	uint16_t nicam_ber_measure_period;
	uint16_t nicam_upper_ber_threshold;
	uint16_t nicam_lower_ber_threshold;
} stm_fe_sif_config_t;

typedef struct stm_fe_sif_audio_info_s {
	stm_fe_sif_audio_std_t std;
	stm_fe_sif_audio_output_mode_t output_mode;
	uint32_t stereo_present;
	uint32_t sap_present;
	uint32_t data_present;
} stm_fe_sif_audio_info_t;

#endif /*_STM_FRONTEND_SIF_H*/
