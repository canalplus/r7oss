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

Source file name : stm_frontend_demod.h
Author :           Rahul.V

stm_fe component header

Date        Modification                                    Name
----        ------------                                    --------
16-Jun-11   Created                                         Rahul.V

************************************************************************/

#ifndef _STM_FRONTEND_DEOMD_H
#define _STM_FRONTEND_DEOMD_H

typedef enum stm_fe_demod_fec_rate_e {
	STM_FE_DEMOD_FEC_RATE_NONE = 0,
	STM_FE_DEMOD_FEC_RATE_1_2 = (1 << 0),
	STM_FE_DEMOD_FEC_RATE_2_3 = (1 << 1),
	STM_FE_DEMOD_FEC_RATE_3_4 = (1 << 2),
	STM_FE_DEMOD_FEC_RATE_4_5 = (1 << 3),
	STM_FE_DEMOD_FEC_RATE_5_6 = (1 << 4),
	STM_FE_DEMOD_FEC_RATE_6_7 = (1 << 5),
	STM_FE_DEMOD_FEC_RATE_7_8 = (1 << 6),
	STM_FE_DEMOD_FEC_RATE_8_9 = (1 << 7),
	STM_FE_DEMOD_FEC_RATE_1_4 = (1 << 8),
	STM_FE_DEMOD_FEC_RATE_1_3 = (1 << 9),
	STM_FE_DEMOD_FEC_RATE_2_5 = (1 << 10),
	STM_FE_DEMOD_FEC_RATE_3_5 = (1 << 11),
	STM_FE_DEMOD_FEC_RATE_9_10 = (1 << 12),
	STM_FE_DEMOD_FEC_RATE_AUTO = (1 << 31)
} stm_fe_demod_fec_rate_t;

typedef enum stm_fe_demod_spectral_inversion_e {
	STM_FE_DEMOD_SPECTRAL_INVERSION_OFF = 0,
	STM_FE_DEMOD_SPECTRAL_INVERSION_ON = (1 << 0),
	STM_FE_DEMOD_SPECTRAL_INVERSION_AUTO = (1 << 31)
} stm_fe_demod_spectral_inversion_t;

typedef enum stm_fe_demod_modulation_e {
	STM_FE_DEMOD_MODULATION_NONE = 0,
	STM_FE_DEMOD_MODULATION_BPSK = (1 << 0),
	STM_FE_DEMOD_MODULATION_QPSK = (1 << 1),
	STM_FE_DEMOD_MODULATION_8PSK = (1 << 2),
	STM_FE_DEMOD_MODULATION_16APSK = (1 << 3),
	STM_FE_DEMOD_MODULATION_32APSK = (1 << 4),
	STM_FE_DEMOD_MODULATION_4QAM = (1 << 5),
	STM_FE_DEMOD_MODULATION_16QAM = (1 << 6),
	STM_FE_DEMOD_MODULATION_32QAM = (1 << 7),
	STM_FE_DEMOD_MODULATION_64QAM = (1 << 8),
	STM_FE_DEMOD_MODULATION_128QAM = (1 << 9),
	STM_FE_DEMOD_MODULATION_256QAM = (1 << 10),
	STM_FE_DEMOD_MODULATION_512QAM = (1 << 11),
	STM_FE_DEMOD_MODULATION_1024QAM = (1 << 12),
	STM_FE_DEMOD_MODULATION_2VSB = (1 << 13),
	STM_FE_DEMOD_MODULATION_4VSB = (1 << 14),
	STM_FE_DEMOD_MODULATION_8VSB = (1 << 15),
	STM_FE_DEMOD_MODULATION_16VSB = (1 << 16),
	STM_FE_DEMOD_MODULATION_AUTO = (1 << 31)
} stm_fe_demod_modulation_t;

typedef enum stm_fe_demod_roll_off_e {
	STM_FE_DEMOD_ROLL_OFF_0_20 = (1 << 0),
	STM_FE_DEMOD_ROLL_OFF_0_25 = (1 << 1),
	STM_FE_DEMOD_ROLL_OFF_0_35 = (1 << 2)
} stm_fe_demod_roll_off_t;

typedef enum stm_fe_demod_hierarchy_e {
	STM_FE_DEMOD_HIERARCHY_NONE = 0,
	STM_FE_DEMOD_HIERARCHY_LOW = (1 << 0),
	STM_FE_DEMOD_HIERARCHY_HIGH = (1 << 1),
	STM_FE_DEMOD_HIERARCHY_A = (1 << 2),
	STM_FE_DEMOD_HIERARCHY_B = (1 << 3),
	STM_FE_DEMOD_HIERARCHY_C = (1 << 4)
} stm_fe_demod_hierarchy_t;

typedef enum stm_fe_demod_hierarchy_alpha_e {
	STM_FE_DEMOD_HIERARCHY_ALPHA_NONE = 0,
	STM_FE_DEMOD_HIERARCHY_ALPHA_1 = (1 << 0),
	STM_FE_DEMOD_HIERARCHY_ALPHA_2 = (1 << 1),
	STM_FE_DEMOD_HIERARCHY_ALPHA_4 = (1 << 2)
} stm_fe_demod_hierarchy_alpha_t;

typedef enum stm_fe_demod_guard_e {
	STM_FE_DEMOD_GUARD_1_4 = (1 << 0),
	STM_FE_DEMOD_GUARD_1_8 = (1 << 1),
	STM_FE_DEMOD_GUARD_1_16 = (1 << 2),
	STM_FE_DEMOD_GUARD_1_32 = (1 << 3),
	STM_FE_DEMOD_GUARD_1_128 = (1 << 4),
	STM_FE_DEMOD_GUARD_19_128 = (1 << 5),
	STM_FE_DEMOD_GUARD_19_256 = (1 << 6),
	STM_FE_DEMOD_GUARD_AUTO = (1 << 31)
} stm_fe_demod_guard_t;

typedef enum stm_fe_demod_fft_mode_e {
	STM_FE_DEMOD_FFT_MODE_1K = (1 << 0),
	STM_FE_DEMOD_FFT_MODE_2K = (1 << 1),
	STM_FE_DEMOD_FFT_MODE_4K = (1 << 2),
	STM_FE_DEMOD_FFT_MODE_8K = (1 << 3),
	STM_FE_DEMOD_FFT_MODE_16K = (1 << 4),
	STM_FE_DEMOD_FFT_MODE_32K = (1 << 5),
	STM_FE_DEMOD_FFT_MODE_AUTO = (1 << 31)
} stm_fe_demod_fft_mode_t;

typedef enum stm_fe_demod_bw_e {
	STM_FE_DEMOD_BW_1_7 = (1 << 0),
	STM_FE_DEMOD_BW_5_0 = (1 << 1),
	STM_FE_DEMOD_BW_6_0 = (1 << 2),
	STM_FE_DEMOD_BW_7_0 = (1 << 3),
	STM_FE_DEMOD_BW_8_0 = (1 << 4),
	STM_FE_DEMOD_BW_10_0 = (1 << 5)
} stm_fe_demod_bw_t;

typedef enum stm_fe_demod_papr_mode_e {
	STM_FE_DEMOD_PAPR_MODE_NONE = 0,
	STM_FE_DEMOD_PAPR_MODE_ACE = (1 << 0),
	STM_FE_DEMOD_PAPR_MODE_TR = (1 << 1)
} stm_fe_demod_papr_mode_t;

typedef enum stm_fe_demod_pilot_pattern_e {
	STM_FE_DEMOD_PILOT_PATTERN_1 = (1 << 0),
	STM_FE_DEMOD_PILOT_PATTERN_2 = (1 << 1),
	STM_FE_DEMOD_PILOT_PATTERN_3 = (1 << 2),
	STM_FE_DEMOD_PILOT_PATTERN_4 = (1 << 3),
	STM_FE_DEMOD_PILOT_PATTERN_5 = (1 << 4),
	STM_FE_DEMOD_PILOT_PATTERN_6 = (1 << 5),
	STM_FE_DEMOD_PILOT_PATTERN_7 = (1 << 6),
	STM_FE_DEMOD_PILOT_PATTERN_8 = (1 << 7)
} stm_fe_demod_pilot_pattern_t;

typedef enum stm_fe_demod_preamble_form_e {
	STM_FE_DEMOD_PREAMBLE_FORM_SISO = (1 << 0),
	STM_FE_DEMOD_PREAMBLE_FORM_MISO = (1 << 1)
} stm_fe_demod_preamble_form_t;

typedef enum stm_fe_demod_frame_length_e {
	STM_FE_DEMOD_FRAME_LENGTH_16200 = (1 << 0),
	STM_FE_DEMOD_FRAME_LENGTH_64800 = (1 << 1)
} stm_fe_demod_frame_length_t;

typedef enum stm_fe_demod_input_mode_e {
	STM_FE_DEMOD_INPUT_MODE_SINGLE_PLP = (1 << 0),
	STM_FE_DEMOD_INPUT_MODE_MULTI_PLP = (1 << 1)
} stm_fe_demod_input_mode_t;

typedef enum stm_fe_demod_tfs_e {
	STM_FE_DEMOD_TFS_ON = (1 << 0),
	STM_FE_DEMOD_TFS_OFF = (1 << 1)
} stm_fe_demod_tfs_t;

typedef enum stm_fe_demod_adaptation_mode_e {
	STM_FE_DEMOD_ADAPTATION_MODE_NORMAL = (1 << 0),
	STM_FE_DEMOD_ADAPTATION_MODE_HIGH_EFFICIENCY = (1 << 1)
} stm_fe_demod_adaptation_mode_t;

typedef enum stm_fe_demod_pilot_status_e {
	STM_FE_DEMOD_PILOT_STATUS_OFF = 0,
	STM_FE_DEMOD_PILOT_STATUS_ON = (1 << 0)
} stm_fe_demod_pilot_status_t;

typedef enum stm_fe_demod_analog_tv_system_e {
	STM_FE_DEMOD_ANALOG_TV_SYSTEM_UNKNOWN = 0,
	STM_FE_DEMOD_ANALOG_TV_SYSTEM_BG = (1 << 0),
	STM_FE_DEMOD_ANALOG_TV_SYSTEM_DK = (1 << 1),
	STM_FE_DEMOD_ANALOG_TV_SYSTEM_I = (1 << 2),
	STM_FE_DEMOD_ANALOG_TV_SYSTEM_H = (1 << 3),
	STM_FE_DEMOD_ANALOG_TV_SYSTEM_L = (1 << 4),
	STM_FE_DEMOD_ANALOG_TV_SYSTEM_L1 = (1 << 5),
	STM_FE_DEMOD_ANALOG_TV_SYSTEM_M = (1 << 6),
	STM_FE_DEMOD_ANALOG_TV_SYSTEM_N = (1 << 7)
} stm_fe_demod_analog_tv_system_t;

typedef enum stm_fe_demod_status_e {
	STM_FE_DEMOD_STATUS_UNKNOWN = 0,
	STM_FE_DEMOD_SIGNAL_FOUND = (1 << 0),
	STM_FE_DEMOD_NO_SIGNAL = (1 << 1),
	STM_FE_DEMOD_SYNC_OK = (1 << 2)
} stm_fe_demod_status_t;

typedef enum stm_fe_demod_event_e {
	STM_FE_DEMOD_EVENT_TUNE_LOCKED = (1 << 0),
	STM_FE_DEMOD_EVENT_TUNE_UNLOCKED = (1 << 1),
	STM_FE_DEMOD_EVENT_TUNE_FAILED = (1 << 2),
	STM_FE_DEMOD_EVENT_SCAN_LOCKED = (1 << 3),
	STM_FE_DEMOD_EVENT_SCAN_COMPLETE = (1 << 4),
	STM_FE_DEMOD_EVENT_LNB_FAILURE = (1 << 5),
	STM_FE_DEMOD_EVENT_HW_FAILURE = (1 << 6)
} stm_fe_demod_event_t;

typedef enum stm_fe_demod_tx_std_e {
	STM_FE_DEMOD_TX_STD_DVBS = (1 << 0),
	STM_FE_DEMOD_TX_STD_DVBS2 = (1 << 1),
	STM_FE_DEMOD_TX_STD_DVBC = (1 << 2),
	STM_FE_DEMOD_TX_STD_DVBC2 = (1 << 3),
	STM_FE_DEMOD_TX_STD_J83_AC = (1 << 4),
	STM_FE_DEMOD_TX_STD_J83_B = (1 << 5),
	STM_FE_DEMOD_TX_STD_DVBT = (1 << 6),
	STM_FE_DEMOD_TX_STD_DVBT2 = (1 << 7),
	STM_FE_DEMOD_TX_STD_ATSC = (1 << 8),
	STM_FE_DEMOD_TX_STD_ISDBT = (1 << 9),
	STM_FE_DEMOD_TX_STD_PAL = (1 << 10),
	STM_FE_DEMOD_TX_STD_SECAM = (1 << 11),
	STM_FE_DEMOD_TX_STD_NTSC = (1 << 12),
} stm_fe_demod_tx_std_t;

typedef enum stm_fe_demod_scan_tx_std_e {
	STM_FE_DEMOD_SCAN_TX_STD_DVB_SAT =
	    (STM_FE_DEMOD_TX_STD_DVBS | STM_FE_DEMOD_TX_STD_DVBS2),
	STM_FE_DEMOD_SCAN_TX_STD_DVB_CAB = (STM_FE_DEMOD_TX_STD_DVBC),
	STM_FE_DEMOD_SCAN_TX_STD_DVB_TER =
	    (STM_FE_DEMOD_TX_STD_DVBT | STM_FE_DEMOD_TX_STD_DVBT2),
	STM_FE_DEMOD_SCAN_TX_STD_ITUT_J83_CAB =
	    (STM_FE_DEMOD_TX_STD_J83_AC | STM_FE_DEMOD_TX_STD_J83_B),
	STM_FE_DEMOD_SCAN_TX_STD_ATSC_TER = (STM_FE_DEMOD_TX_STD_ATSC),
	STM_FE_DEMOD_SCAN_TX_STD_ISDB_TER = (STM_FE_DEMOD_TX_STD_ISDBT),
	STM_FE_DEMOD_SCAN_TX_STD_PAL_ = (STM_FE_DEMOD_TX_STD_PAL),
	STM_FE_DEMOD_SCAN_TX_STD_NTSC_ = (STM_FE_DEMOD_TX_STD_NTSC),
	STM_FE_DEMOD_SCAN_TX_STD_SECAM_ = (STM_FE_DEMOD_TX_STD_SECAM),
} stm_fe_demod_scan_tx_std_t;

typedef enum stm_fe_demod_feature_e {
	STM_FE_DEMOD_FEATURE_NONE = 0,
	STM_FE_DEMOD_FEATURE_AUTO_FEC = (1 << 0),
	STM_FE_DEMOD_FEATURE_AUTO_SR = (1 << 1),
	STM_FE_DEMOD_FEATURE_AUTO_MODULATION = (1 << 2),
	STM_FE_DEMOD_FEATURE_AUTO_STANDARD_DETECT = (1 << 3),
	STM_FE_DEMOD_FEATURE_AUTO_GUARD = (1 << 4),
	STM_FE_DEMOD_FEATURE_AUTO_MODE = (1 << 5),
	STM_FE_DEMOD_FEATURE_AUTO_STEPSIZE = (1 << 6)
} stm_fe_demod_feature_t;

typedef enum stm_fe_demod_scan_table_form_e {
	STM_FE_DEMOD_SCAN_TABLE_FORM_LIST = (1 << 0),
	STM_FE_DEMOD_SCAN_TABLE_FORM_BOUNDARIES = (1 << 1)
} stm_fe_demod_scan_table_form_t;

typedef enum stm_fe_demod_context_e {
	STM_FE_DEMOD_SCAN = (1 << 0),
	STM_FE_DEMOD_TUNE = (1 << 1)
} stm_fe_demod_context_t;

typedef enum stm_fe_demod_scan_context_e {
	STM_FE_DEMOD_SCAN_NEW = (1 << 0),
	STM_FE_DEMOD_SCAN_RESUME = (1 << 1)
} stm_fe_demod_scan_context_t;

typedef struct stm_fe_demod_dvbs_tune_caps_s {
	uint32_t freq_min;
	uint32_t freq_max;
	stm_fe_demod_fec_rate_t fecs;
	stm_fe_demod_modulation_t mods;
	stm_fe_demod_feature_t features;
	uint32_t sr_min;
	uint32_t sr_max;
} stm_fe_demod_dvbs_tune_caps_t;

typedef struct stm_fe_demod_dvbs2_tune_caps_s {
	uint32_t freq_min;
	uint32_t freq_max;
	stm_fe_demod_fec_rate_t fecs;
	stm_fe_demod_modulation_t mods;
	stm_fe_demod_feature_t features;
	uint32_t sr_min;
	uint32_t sr_max;
	stm_fe_demod_roll_off_t roll_off;
} stm_fe_demod_dvbs2_tune_caps_t;

typedef struct stm_fe_demod_dvbc_tune_caps_s {
	uint32_t freq_min;
	uint32_t freq_max;
	stm_fe_demod_fec_rate_t fecs;
	stm_fe_demod_modulation_t mods;
	stm_fe_demod_feature_t features;
	uint32_t sr_min;
	uint32_t sr_max;
} stm_fe_demod_dvbc_tune_caps_t;

typedef struct stm_fe_demod_itut_j83_ac_tune_caps_s {
	uint32_t freq_min;
	uint32_t freq_max;
	stm_fe_demod_fec_rate_t fecs;
	stm_fe_demod_modulation_t mods;
	stm_fe_demod_feature_t features;
	uint32_t sr_min;
	uint32_t sr_max;
} stm_fe_demod_itut_j83_ac_tune_caps_t;

typedef struct stm_fe_demod_itut_j83_b_tune_caps_s {
	uint32_t freq_min;
	uint32_t freq_max;
	stm_fe_demod_fec_rate_t fecs;
	stm_fe_demod_modulation_t mods;
	stm_fe_demod_feature_t features;
	uint32_t sr_min;
	uint32_t sr_max;
} stm_fe_demod_itut_j83_b_tune_caps_t;

typedef struct stm_fe_demod_dvbt_tune_caps_s {
	uint32_t freq_min;
	uint32_t freq_max;
	stm_fe_demod_fec_rate_t fecs;
	stm_fe_demod_modulation_t mods;
	stm_fe_demod_feature_t features;
	stm_fe_demod_fft_mode_t fft_modes;
	stm_fe_demod_guard_t guards;
	stm_fe_demod_hierarchy_t hierarchys;
} stm_fe_demod_dvbt_tune_caps_t;

typedef struct stm_fe_demod_dvbt2_tune_caps_s {
	uint32_t freq_min;
	uint32_t freq_max;
	stm_fe_demod_fec_rate_t fecs;
	stm_fe_demod_modulation_t mods;
	stm_fe_demod_feature_t features;
	stm_fe_demod_fft_mode_t fft_modes;
	stm_fe_demod_guard_t guards;
	stm_fe_demod_hierarchy_t hierarchys;
	stm_fe_demod_papr_mode_t papr_modes;
	stm_fe_demod_pilot_pattern_t pilot_patterns;
	stm_fe_demod_preamble_form_t preamble_forms;
	stm_fe_demod_input_mode_t input_modes;
	stm_fe_demod_tfs_t tfs_used;
	stm_fe_demod_adaptation_mode_t adaptation_modes;
} stm_fe_demod_dvbt2_tune_caps_t;

typedef struct stm_fe_demod_atsc_tune_caps_s {
	uint32_t freq_min;
	uint32_t freq_max;
	stm_fe_demod_modulation_t mods;
	stm_fe_demod_feature_t features;
	uint32_t sr_min;
	uint32_t sr_max;
} stm_fe_demod_atsc_tune_caps_t;

typedef struct stm_fe_demod_isdbt_tune_caps_s {
	uint32_t freq_min;
	uint32_t freq_max;
	stm_fe_demod_fec_rate_t fecs;
	stm_fe_demod_modulation_t mods;
	stm_fe_demod_feature_t features;
	stm_fe_demod_fft_mode_t fft_modes;
	stm_fe_demod_guard_t guards;
	stm_fe_demod_hierarchy_t hierarchys;
} stm_fe_demod_isdbt_tune_caps_t;

typedef struct stm_fe_demod_analog_tune_caps_s {
	stm_fe_demod_analog_tv_system_t tv_systems;
	uint32_t freq_min;
	uint32_t freq_max;
} stm_fe_demod_analog_tune_caps_t;

typedef struct stm_fe_demod_tune_caps_s {
	stm_fe_demod_tx_std_t std;
	union {
		stm_fe_demod_dvbs_tune_caps_t dvbs;
		stm_fe_demod_dvbs2_tune_caps_t dvbs2;
		stm_fe_demod_dvbc_tune_caps_t dvbc;
		stm_fe_demod_itut_j83_ac_tune_caps_t j83ac;
		stm_fe_demod_itut_j83_b_tune_caps_t j83b;
		stm_fe_demod_dvbt_tune_caps_t dvbt;
		stm_fe_demod_dvbt2_tune_caps_t dvbt2;
		stm_fe_demod_atsc_tune_caps_t atsc;
		stm_fe_demod_isdbt_tune_caps_t isdbt;
		stm_fe_demod_analog_tune_caps_t analog;
	} u_tune;
} stm_fe_demod_tune_caps_t;

#define STM_FE_MAX_NUM_TX_STANDARDS 3
typedef struct stm_fe_demod_caps_s {
	uint32_t num_stds;
	stm_fe_demod_tune_caps_t tune_caps[STM_FE_MAX_NUM_TX_STANDARDS];
	stm_fe_demod_feature_t scan_features;
} stm_fe_demod_caps_t;

typedef struct stm_fe_demod_dvbs_tune_params_s {
	uint32_t freq;
	uint32_t sr;
	stm_fe_demod_fec_rate_t fec;
	stm_fe_demod_spectral_inversion_t inv;
} stm_fe_demod_dvbs_tune_params_t;

typedef struct stm_fe_demod_dvbs2_tune_params_s {
	uint32_t freq;
	uint32_t sr;
	stm_fe_demod_fec_rate_t fec;
	stm_fe_demod_spectral_inversion_t inv;
	stm_fe_demod_modulation_t mod;
	stm_fe_demod_roll_off_t roll_off;
} stm_fe_demod_dvbs2_tune_params_t;

typedef struct stm_fe_demod_dvbc_tune_params_s {
	uint32_t freq;
	uint32_t sr;
	stm_fe_demod_spectral_inversion_t inv;
	stm_fe_demod_modulation_t mod;
} stm_fe_demod_dvbc_tune_params_t;

typedef struct stm_fe_demod_itut_j83_ac_tune_params_s {
	uint32_t freq;
	uint32_t sr;
	stm_fe_demod_spectral_inversion_t inv;
	stm_fe_demod_modulation_t mod;
} stm_fe_demod_itut_j83_ac_tune_params_t;

typedef struct stm_fe_demod_itut_j83_b_tune_params_s {
	uint32_t freq;
	uint32_t sr;
	stm_fe_demod_spectral_inversion_t inv;
	stm_fe_demod_modulation_t mod;
} stm_fe_demod_itut_j83_b_tune_params_t;

typedef struct stm_fe_demod_dvbt_tune_params_s {
	uint32_t freq;
	stm_fe_demod_bw_t bw;
	stm_fe_demod_fec_rate_t fec;
	stm_fe_demod_spectral_inversion_t inv;
	stm_fe_demod_modulation_t mod;
	stm_fe_demod_hierarchy_t hierarchy;
	stm_fe_demod_guard_t guard;
	stm_fe_demod_fft_mode_t fft_mode;
} stm_fe_demod_dvbt_tune_params_t;

typedef struct stm_fe_demod_dvbt2_tune_params_s {
	uint32_t freq;
	stm_fe_demod_bw_t bw;
	stm_fe_demod_fec_rate_t fec;
	stm_fe_demod_spectral_inversion_t inv;
	stm_fe_demod_modulation_t mod;
	stm_fe_demod_hierarchy_t hierarchy;
	stm_fe_demod_guard_t guard;
	stm_fe_demod_fft_mode_t fft_mode;
	uint8_t plp_id;
} stm_fe_demod_dvbt2_tune_params_t;

typedef struct stm_fe_demod_atsc_tune_params_s {
	uint32_t freq;
	stm_fe_demod_fec_rate_t fec;
	stm_fe_demod_spectral_inversion_t inv;
	stm_fe_demod_modulation_t mod;
} stm_fe_demod_atsc_tune_params_t;

typedef struct stm_fe_demod_isdbt_tune_params_s {
	uint32_t freq;
	stm_fe_demod_bw_t bw;
	stm_fe_demod_fec_rate_t fec;
	stm_fe_demod_spectral_inversion_t inv;
	stm_fe_demod_modulation_t mod;
	stm_fe_demod_hierarchy_t hierarchy;
	stm_fe_demod_guard_t guard;
	stm_fe_demod_fft_mode_t fft_mode;
} stm_fe_demod_isdbt_tune_params_t;

typedef struct stm_fe_demod_analog_tune_params_s {
	uint32_t freq;
	stm_fe_demod_analog_tv_system_t tv_system;
	uint32_t afc_enable;
	uint32_t afc_window;
} stm_fe_demod_analog_tune_params_t;

typedef struct stm_fe_demod_tune_params_s {
	stm_fe_demod_tx_std_t std;
	union {
		stm_fe_demod_dvbs_tune_params_t dvbs;
		stm_fe_demod_dvbs2_tune_params_t dvbs2;
		stm_fe_demod_dvbc_tune_params_t dvbc;
		stm_fe_demod_itut_j83_ac_tune_params_t j83ac;
		stm_fe_demod_itut_j83_b_tune_params_t j83b;
		stm_fe_demod_dvbt_tune_params_t dvbt;
		stm_fe_demod_dvbt2_tune_params_t dvbt2;
		stm_fe_demod_atsc_tune_params_t atsc;
		stm_fe_demod_isdbt_tune_params_t isdbt;
		stm_fe_demod_analog_tune_params_t analog;
	} u_tune;
} stm_fe_demod_tune_params_t;

typedef struct stm_fe_demod_dvbs_channel_info_s {
	uint32_t freq;
	uint32_t sr;
	stm_fe_demod_fec_rate_t fec;
	stm_fe_demod_spectral_inversion_t inv;
	stm_fe_demod_modulation_t mod;
	stm_fe_demod_status_t status;
	int32_t signal_strength;
	uint32_t snr;
	uint32_t ber;
} stm_fe_demod_dvbs_channel_info_t;

typedef struct stm_fe_demod_dvbs2_channel_info_s {
	uint32_t freq;
	uint32_t sr;
	stm_fe_demod_fec_rate_t fec;
	stm_fe_demod_spectral_inversion_t inv;
	stm_fe_demod_modulation_t mod;
	stm_fe_demod_status_t status;
	int32_t signal_strength;
	uint32_t snr;
	uint32_t per;
	stm_fe_demod_roll_off_t roll_off;
	stm_fe_demod_frame_length_t frame_len;
	stm_fe_demod_pilot_status_t pilots;
} stm_fe_demod_dvbs2_channel_info_t;

typedef struct stm_fe_demod_dvbc_channel_info_s {
	uint32_t freq;
	uint32_t sr;
	stm_fe_demod_spectral_inversion_t inv;
	stm_fe_demod_modulation_t mod;
	stm_fe_demod_status_t status;
	int32_t signal_strength;
	uint32_t snr;
	uint32_t ber;
	uint32_t ucb;
} stm_fe_demod_dvbc_channel_info_t;

typedef struct stm_fe_demod_itut_j83_ac_channel_info_s {
	uint32_t freq;
	uint32_t sr;
	stm_fe_demod_spectral_inversion_t inv;
	stm_fe_demod_modulation_t mod;
	stm_fe_demod_status_t status;
	int32_t signal_strength;
	uint32_t snr;
	uint32_t ber;
	uint32_t ucb;
} stm_fe_demod_itut_j83_ac_channel_info_t;

typedef struct stm_fe_demod_itut_j83_b_channel_info_s {
	uint32_t freq;
	uint32_t sr;
	stm_fe_demod_spectral_inversion_t inv;
	stm_fe_demod_modulation_t mod;
	stm_fe_demod_status_t status;
	int32_t signal_strength;
	uint32_t snr;
	uint32_t ber;
	uint32_t ucb;
} stm_fe_demod_itut_j83_b_channel_info_t;

typedef struct stm_fe_demod_dvbt_channel_info_s {
	uint32_t freq;
	stm_fe_demod_bw_t bw;
	stm_fe_demod_fec_rate_t fec;
	stm_fe_demod_spectral_inversion_t inv;
	stm_fe_demod_modulation_t mod;
	stm_fe_demod_hierarchy_t hierarchy;
	stm_fe_demod_hierarchy_alpha_t alpha;
	stm_fe_demod_guard_t guard;
	stm_fe_demod_fft_mode_t fft_mode;
	stm_fe_demod_status_t status;
	int32_t signal_strength;
	uint32_t snr;
	uint32_t ber;
	uint32_t ucb;
} stm_fe_demod_dvbt_channel_info_t;

typedef struct stm_fe_demod_dvbt2_channel_info_s {
	uint32_t freq;
	stm_fe_demod_bw_t bw;
	stm_fe_demod_fec_rate_t fec;
	stm_fe_demod_spectral_inversion_t inv;
	stm_fe_demod_modulation_t mod;
	stm_fe_demod_hierarchy_t hierarchy;
	stm_fe_demod_hierarchy_alpha_t alpha;
	stm_fe_demod_guard_t guard;
	stm_fe_demod_fft_mode_t fft_mode;
	stm_fe_demod_status_t status;
	uint8_t plp_id;
	int32_t signal_strength;
	uint32_t snr;
	uint32_t ber;
	uint32_t fer;
} stm_fe_demod_dvbt2_channel_info_t;

typedef struct stm_fe_demod_atsc_channel_info_s {
	uint32_t freq;
	uint32_t sr;
	stm_fe_demod_fec_rate_t fec;
	stm_fe_demod_spectral_inversion_t inv;
	stm_fe_demod_modulation_t mod;
	stm_fe_demod_status_t status;
	int32_t signal_strength;
	uint32_t snr;
	uint32_t ber;
} stm_fe_demod_atsc_channel_info_t;

typedef struct stm_fe_demod_isdbt_channel_info_s {
	uint32_t freq;
	stm_fe_demod_bw_t bw;
	stm_fe_demod_fec_rate_t fec;
	stm_fe_demod_spectral_inversion_t inv;
	stm_fe_demod_modulation_t mod;
	stm_fe_demod_hierarchy_t hierarchy;
	stm_fe_demod_guard_t guard;
	stm_fe_demod_fft_mode_t fft_mode;
	stm_fe_demod_status_t status;
	int32_t signal_strength;
	uint32_t snr;
	uint32_t ber;
} stm_fe_demod_isdbt_channel_info_t;

typedef struct stm_fe_demod_analog_channel_info_s {
	uint32_t freq;
	stm_fe_demod_status_t status;
	int32_t signal_strength;
	uint32_t snr;
} stm_fe_demod_analog_channel_info_t;

typedef struct stm_fe_demod_channel_info_s {
	stm_fe_demod_tx_std_t std;
	union {
		stm_fe_demod_dvbs_channel_info_t dvbs;
		stm_fe_demod_dvbs2_channel_info_t dvbs2;
		stm_fe_demod_dvbc_channel_info_t dvbc;
		stm_fe_demod_itut_j83_ac_channel_info_t j83ac;
		stm_fe_demod_itut_j83_b_channel_info_t j83b;
		stm_fe_demod_dvbt_channel_info_t dvbt;
		stm_fe_demod_dvbt2_channel_info_t dvbt2;
		stm_fe_demod_atsc_channel_info_t atsc;
		stm_fe_demod_isdbt_channel_info_t isdbt;
		stm_fe_demod_analog_channel_info_t analog;
	} u_channel;
} stm_fe_demod_channel_info_t;

typedef struct stm_fe_demod_scan_info_s {
	uint32_t freq_table_index;
	stm_fe_demod_channel_info_t demod_info;
} stm_fe_demod_scan_info_t;

typedef struct stm_fe_demod_tune_info_s {
	stm_fe_demod_channel_info_t demod_info;
} stm_fe_demod_tune_info_t;

typedef struct stm_fe_demod_info_s {
	stm_fe_demod_context_t context;
	union {
		stm_fe_demod_scan_info_t scan;
		stm_fe_demod_tune_info_t tune;
	} u_info;
} stm_fe_demod_info_t;

#define STM_FE_SAT_SCAN_SYMBOLRATES_MAX 4
#define STM_FE_SAT_SCAN_FREQUENCIES_MAX 200
typedef struct stm_fe_demod_dvb_sat_scan_config_s {
	stm_fe_demod_scan_table_form_t table_form;
	uint32_t freq_table[STM_FE_SAT_SCAN_FREQUENCIES_MAX];
	uint32_t freq_table_num;
	uint32_t sr_table[STM_FE_SAT_SCAN_SYMBOLRATES_MAX];
	uint32_t sr_table_num;
	stm_fe_demod_fec_rate_t fecs;
	stm_fe_demod_modulation_t mods;
} stm_fe_demod_dvb_sat_scan_config_t;

#define STM_FE_CAB_SCAN_SYMBOLRATES_MAX 4
#define STM_FE_CAB_SCAN_FREQUENCIES_MAX 200
typedef struct stm_fe_demod_dvb_cab_scan_config_s {
	stm_fe_demod_scan_table_form_t table_form;
	uint32_t freq_table[STM_FE_CAB_SCAN_FREQUENCIES_MAX];
	uint32_t freq_table_num;
	uint32_t sr_table[STM_FE_CAB_SCAN_SYMBOLRATES_MAX];
	uint32_t sr_table_num;
	stm_fe_demod_modulation_t mods;
} stm_fe_demod_dvb_cab_scan_config_t;

#define STM_FE_TER_SCAN_FREQUENCIES_MAX 200
typedef struct stm_fe_demod_dvb_ter_scan_config_s {
	stm_fe_demod_scan_table_form_t table_form;
	uint32_t freq_table[STM_FE_TER_SCAN_FREQUENCIES_MAX];
	uint32_t freq_table_num;
	stm_fe_demod_fec_rate_t fecs;
	stm_fe_demod_modulation_t mods;
	stm_fe_demod_bw_t bw;
} stm_fe_demod_dvb_ter_scan_config_t;

#define STM_FE_ITUT_J83_SCAN_SYMBOLRATES_MAX 4
#define STM_FE_ITUT_J83_SCAN_FREQUENCIES_MAX 200
typedef struct stm_fe_demod_itut_j83_cab_scan_config_s {
	stm_fe_demod_scan_table_form_t table_form;
	uint32_t freq_table[STM_FE_ITUT_J83_SCAN_FREQUENCIES_MAX];
	uint32_t freq_table_num;
	uint32_t sr_table[STM_FE_ITUT_J83_SCAN_SYMBOLRATES_MAX];
	uint32_t sr_table_num;
	stm_fe_demod_modulation_t mods;
} stm_fe_demod_itut_j83_cab_scan_config_t;

#define STM_FE_ATSC_TER_SCAN_FREQUENCIES_MAX 200
typedef struct stm_fe_demod_atsc_ter_scan_config_s {
	stm_fe_demod_scan_table_form_t table_form;
	uint32_t freq_table[STM_FE_ATSC_TER_SCAN_FREQUENCIES_MAX];
	uint32_t freq_table_num;
	stm_fe_demod_fec_rate_t fecs;
	stm_fe_demod_modulation_t mods;
} stm_fe_demod_atsc_ter_scan_config_t;

#define STM_FE_ISDB_TER_SCAN_FREQUENCIES_MAX 200
typedef struct stm_fe_demod_isdb_ter_scan_config_s {
	stm_fe_demod_scan_table_form_t table_form;
	uint32_t freq_table[STM_FE_ISDB_TER_SCAN_FREQUENCIES_MAX];
	uint32_t freq_table_num;
	stm_fe_demod_fec_rate_t fecs;
	stm_fe_demod_modulation_t mods;
} stm_fe_demod_isdb_ter_scan_config_t;

#define STM_FE_ANALOG_SCAN_FREQUENCIES_MAX 200
typedef struct stm_fe_demod_analog_scan_config_s {
	stm_fe_demod_scan_table_form_t table_form;
	uint32_t freq_table[STM_FE_ANALOG_SCAN_FREQUENCIES_MAX];
	uint32_t freq_table_num;
	stm_fe_demod_analog_tv_system_t tv_systems;
	uint32_t afc_enable;
	uint32_t afc_window;
} stm_fe_demod_analog_scan_config_t;

typedef struct stm_fe_demod_scan_configure_s {
	stm_fe_demod_scan_tx_std_t scan_stds;
	union {
		stm_fe_demod_dvb_sat_scan_config_t dvb_sat;
		stm_fe_demod_dvb_cab_scan_config_t dvb_cab;
		stm_fe_demod_dvb_ter_scan_config_t dvb_ter;
		stm_fe_demod_itut_j83_cab_scan_config_t itut_j83_cab;
		stm_fe_demod_atsc_ter_scan_config_t atsc_ter;
		stm_fe_demod_isdb_ter_scan_config_t isdb_ter;
		stm_fe_demod_analog_scan_config_t analog;
	} u_config;
} stm_fe_demod_scan_configure_t;

typedef struct stm_fe_demod_s *stm_fe_demod_h;

int32_t __must_check stm_fe_demod_new(const char *name, stm_fe_demod_h *obj);
int32_t __must_check stm_fe_demod_delete(stm_fe_demod_h obj);
int32_t __must_check stm_fe_demod_tune(stm_fe_demod_h obj,
						stm_fe_demod_tune_params_t *tp);
int32_t __must_check stm_fe_demod_scan(stm_fe_demod_h obj,
					stm_fe_demod_scan_context_t context,
					stm_fe_demod_scan_configure_t *conf);
int32_t __must_check stm_fe_demod_stop(stm_fe_demod_h obj);
int32_t __must_check stm_fe_demod_get_info(stm_fe_demod_h obj,
						     stm_fe_demod_info_t *info);
int32_t __must_check stm_fe_demod_attach(stm_fe_demod_h obj,
							stm_object_h dest_obj);
int32_t __must_check stm_fe_demod_detach(stm_fe_demod_h obj,
							stm_object_h dest_obj);

#endif /*_STM_FE_DEMOD_H*/
