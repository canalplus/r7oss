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

Source file name : demod_caps.h
Author :           Rahul.V

Header for setting the capabilities of demodulator devices

Date        Modification                                    Name
----        ------------                                    --------
04-Jul-11   Created                                         Rahul.V
01-Aug-11   Added stv0367                                   Ankur
************************************************************************/
#ifndef _DEMOD_CAPS_H
#define _DEMOD_CAPS_H

stm_fe_demod_caps_t stv0903_caps = {
	.num_stds = 2,
	.tune_caps[0] = {
			 .std = STM_FE_DEMOD_TX_STD_DVBS,
			 .u_tune.dvbs.freq_min = 950000,
			 .u_tune.dvbs.freq_max = 2150000,
			 .u_tune.dvbs.fecs = (STM_FE_DEMOD_FEC_RATE_1_2 |
					      STM_FE_DEMOD_FEC_RATE_2_3 |
					      STM_FE_DEMOD_FEC_RATE_3_4 |
					      STM_FE_DEMOD_FEC_RATE_5_6 |
					      STM_FE_DEMOD_FEC_RATE_7_8),
			 .u_tune.dvbs.mods = STM_FE_DEMOD_MODULATION_QPSK,
			 .u_tune.dvbs.features =
			 (STM_FE_DEMOD_FEATURE_AUTO_FEC |
			  STM_FE_DEMOD_FEATURE_AUTO_SR |
			  STM_FE_DEMOD_FEATURE_AUTO_MODULATION |
			  STM_FE_DEMOD_FEATURE_AUTO_STANDARD_DETECT),
			 .u_tune.dvbs.sr_min = 1000000,
			 .u_tune.dvbs.sr_max = 45000000,
			 },
	.tune_caps[1] = {
			 .std = STM_FE_DEMOD_TX_STD_DVBS2,
			 .u_tune.dvbs2.freq_min = 950000,
			 .u_tune.dvbs2.freq_max = 2150000,
			 .u_tune.dvbs2.fecs = (STM_FE_DEMOD_FEC_RATE_1_2 |
					       STM_FE_DEMOD_FEC_RATE_3_5 |
					       STM_FE_DEMOD_FEC_RATE_2_3 |
					       STM_FE_DEMOD_FEC_RATE_3_4 |
					       STM_FE_DEMOD_FEC_RATE_4_5 |
					       STM_FE_DEMOD_FEC_RATE_5_6 |
					       STM_FE_DEMOD_FEC_RATE_8_9 |
					       STM_FE_DEMOD_FEC_RATE_9_10),
			 .u_tune.dvbs2.mods = (STM_FE_DEMOD_MODULATION_QPSK |
					       STM_FE_DEMOD_MODULATION_8PSK |
					       STM_FE_DEMOD_MODULATION_16APSK),
			 .u_tune.dvbs2.features =
			 (STM_FE_DEMOD_FEATURE_AUTO_FEC |
			  STM_FE_DEMOD_FEATURE_AUTO_SR |
			  STM_FE_DEMOD_FEATURE_AUTO_MODULATION |
			  STM_FE_DEMOD_FEATURE_AUTO_STANDARD_DETECT),
			 .u_tune.dvbs2.sr_min = 1000000,
			 .u_tune.dvbs2.sr_max = 45000000,
			 },
	.scan_features = (STM_FE_DEMOD_FEATURE_AUTO_FEC |
			  STM_FE_DEMOD_FEATURE_AUTO_SR |
			  STM_FE_DEMOD_FEATURE_AUTO_MODULATION |
			  STM_FE_DEMOD_FEATURE_AUTO_STANDARD_DETECT),
};

stm_fe_demod_caps_t stv0367_caps = {
	.num_stds = 2,
	.tune_caps[0] = {
			 .std = STM_FE_DEMOD_TX_STD_DVBC,
			 .u_tune.dvbc.freq_min = 46000,
			 .u_tune.dvbc.freq_max = 862000,
			 .u_tune.dvbc.fecs = STM_FE_DEMOD_FEC_RATE_NONE,
			 .u_tune.dvbc.mods = (STM_FE_DEMOD_MODULATION_4QAM |
					      STM_FE_DEMOD_MODULATION_16QAM |
					      STM_FE_DEMOD_MODULATION_32QAM |
					      STM_FE_DEMOD_MODULATION_64QAM |
					      STM_FE_DEMOD_MODULATION_128QAM |
					      STM_FE_DEMOD_MODULATION_256QAM),
			 .u_tune.dvbc.features =
					   STM_FE_DEMOD_FEATURE_AUTO_MODULATION,
			 .u_tune.dvbc.sr_min = 1000000,
			 .u_tune.dvbc.sr_max = 7000000,
			 },
	.tune_caps[1] = {
			 .std = STM_FE_DEMOD_TX_STD_DVBT,
			 .u_tune.dvbt.freq_min = 53800,
			 .u_tune.dvbt.freq_max = 858200,
			 .u_tune.dvbt.fecs = (STM_FE_DEMOD_FEC_RATE_1_2 |
					      STM_FE_DEMOD_FEC_RATE_2_3 |
					      STM_FE_DEMOD_FEC_RATE_3_4 |
					      STM_FE_DEMOD_FEC_RATE_4_5 |
					      STM_FE_DEMOD_FEC_RATE_5_6 |
					      STM_FE_DEMOD_FEC_RATE_6_7 |
					      STM_FE_DEMOD_FEC_RATE_7_8),
			 .u_tune.dvbt.mods = (STM_FE_DEMOD_MODULATION_QPSK |
						STM_FE_DEMOD_MODULATION_16QAM |
						STM_FE_DEMOD_MODULATION_64QAM),
			 .u_tune.dvbt.features =
					(STM_FE_DEMOD_FEATURE_AUTO_FEC |
					 STM_FE_DEMOD_FEATURE_AUTO_MODULATION),
			 .u_tune.dvbt.fft_modes = (STM_FE_DEMOD_FFT_MODE_2K |
						   STM_FE_DEMOD_FFT_MODE_4K |
						   STM_FE_DEMOD_FFT_MODE_8K),
			 .u_tune.dvbt.guards = (STM_FE_DEMOD_GUARD_1_4 |
						STM_FE_DEMOD_GUARD_1_8  |
						STM_FE_DEMOD_GUARD_1_16 |
						STM_FE_DEMOD_GUARD_1_32),
			 .u_tune.dvbt.hierarchys =
						(STM_FE_DEMOD_HIERARCHY_LOW |
						 STM_FE_DEMOD_HIERARCHY_HIGH),
			 },
	.scan_features = STM_FE_DEMOD_FEATURE_NONE,
};

stm_fe_demod_caps_t stv0367c_caps = {
	.num_stds = 1,
	.tune_caps[0] = {
			 .std = STM_FE_DEMOD_TX_STD_DVBC,
			 .u_tune.dvbc.freq_min = 46000,
			 .u_tune.dvbc.freq_max = 862000,
			 .u_tune.dvbc.fecs = STM_FE_DEMOD_FEC_RATE_NONE,
			 .u_tune.dvbc.mods = (STM_FE_DEMOD_MODULATION_4QAM |
					      STM_FE_DEMOD_MODULATION_16QAM |
					      STM_FE_DEMOD_MODULATION_32QAM |
					      STM_FE_DEMOD_MODULATION_64QAM |
					      STM_FE_DEMOD_MODULATION_128QAM |
					      STM_FE_DEMOD_MODULATION_256QAM),
			 .u_tune.dvbc.features =
					   STM_FE_DEMOD_FEATURE_AUTO_MODULATION,
			 .u_tune.dvbc.sr_min = 1000000,
			 .u_tune.dvbc.sr_max = 7000000,
			 },
	.scan_features = STM_FE_DEMOD_FEATURE_NONE,
};

stm_fe_demod_caps_t remote_anx_b_caps = {
	.num_stds = 1,
	.tune_caps[0] = {
			 .std = STM_FE_DEMOD_TX_STD_J83_B,
			 .u_tune.dvbc.freq_min = 46000,
			 .u_tune.dvbc.freq_max = 862000,
			 .u_tune.dvbc.fecs = STM_FE_DEMOD_FEC_RATE_NONE,
			 .u_tune.dvbc.mods = (STM_FE_DEMOD_MODULATION_4QAM |
					      STM_FE_DEMOD_MODULATION_16QAM |
					      STM_FE_DEMOD_MODULATION_32QAM |
					      STM_FE_DEMOD_MODULATION_64QAM |
					      STM_FE_DEMOD_MODULATION_128QAM |
					      STM_FE_DEMOD_MODULATION_256QAM),
			 .u_tune.dvbc.features = STM_FE_DEMOD_FEATURE_NONE,
			 .u_tune.dvbc.sr_min = 1000000,
			 .u_tune.dvbc.sr_max = 7000000,
			 },
	.scan_features = STM_FE_DEMOD_FEATURE_NONE,
};

stm_fe_demod_caps_t remote_dvb_c_caps = {
	.num_stds = 1,
	.tune_caps[0] = {
			 .std = STM_FE_DEMOD_TX_STD_DVBC,
			 .u_tune.dvbc.freq_min = 46000,
			 .u_tune.dvbc.freq_max = 862000,
			 .u_tune.dvbc.fecs = STM_FE_DEMOD_FEC_RATE_NONE,
			 .u_tune.dvbc.mods = (STM_FE_DEMOD_MODULATION_4QAM |
						STM_FE_DEMOD_MODULATION_16QAM |
					      STM_FE_DEMOD_MODULATION_32QAM |
					      STM_FE_DEMOD_MODULATION_64QAM |
					      STM_FE_DEMOD_MODULATION_128QAM |
					      STM_FE_DEMOD_MODULATION_256QAM) ,
			 .u_tune.dvbc.features = STM_FE_DEMOD_FEATURE_NONE,
			 .u_tune.dvbc.sr_min = 1000000,
			 .u_tune.dvbc.sr_max = 7000000,
			 },
	.scan_features = STM_FE_DEMOD_FEATURE_NONE,
};
stm_fe_demod_caps_t remote_caps = {
	.num_stds = 2,
	.tune_caps[0] = {
			 .std = STM_FE_DEMOD_TX_STD_J83_B,
			 .u_tune.dvbc.freq_min = 46000,
			 .u_tune.dvbc.freq_max = 862000,
			 .u_tune.dvbc.fecs = STM_FE_DEMOD_FEC_RATE_NONE,
			 .u_tune.dvbc.mods = (STM_FE_DEMOD_MODULATION_4QAM |
						STM_FE_DEMOD_MODULATION_16QAM |
					      STM_FE_DEMOD_MODULATION_32QAM |
					      STM_FE_DEMOD_MODULATION_64QAM |
					      STM_FE_DEMOD_MODULATION_128QAM |
					      STM_FE_DEMOD_MODULATION_256QAM) ,
			 .u_tune.dvbc.features = STM_FE_DEMOD_FEATURE_NONE,
			 .u_tune.dvbc.sr_min = 1000000,
			 .u_tune.dvbc.sr_max = 7000000,
			 },
	.tune_caps[1] = {
			 .std = STM_FE_DEMOD_TX_STD_DVBC,
			 .u_tune.dvbc.freq_min = 46000,
			 .u_tune.dvbc.freq_max = 862000,
			 .u_tune.dvbc.fecs = STM_FE_DEMOD_FEC_RATE_NONE,
			 .u_tune.dvbc.mods = (STM_FE_DEMOD_MODULATION_4QAM |
						STM_FE_DEMOD_MODULATION_16QAM |
					      STM_FE_DEMOD_MODULATION_32QAM |
					      STM_FE_DEMOD_MODULATION_64QAM |
					      STM_FE_DEMOD_MODULATION_128QAM |
					      STM_FE_DEMOD_MODULATION_256QAM) ,
			 .u_tune.dvbc.features = STM_FE_DEMOD_FEATURE_NONE,
			 .u_tune.dvbc.sr_min = 1000000,
			 .u_tune.dvbc.sr_max = 7000000,
			 },
	.scan_features = STM_FE_DEMOD_FEATURE_NONE,
};

stm_fe_demod_caps_t stv0367t_caps = {
	.num_stds = 1,
	.tune_caps[0] = {
			 .std = STM_FE_DEMOD_TX_STD_DVBT,
			 .u_tune.dvbt.freq_min = 53800,
			 .u_tune.dvbt.freq_max = 858200,
			 .u_tune.dvbt.fecs = (STM_FE_DEMOD_FEC_RATE_1_2 |
					      STM_FE_DEMOD_FEC_RATE_2_3 |
					      STM_FE_DEMOD_FEC_RATE_3_4 |
					      STM_FE_DEMOD_FEC_RATE_4_5 |
					      STM_FE_DEMOD_FEC_RATE_5_6 |
					      STM_FE_DEMOD_FEC_RATE_6_7 |
					      STM_FE_DEMOD_FEC_RATE_7_8),
			 .u_tune.dvbt.mods = (STM_FE_DEMOD_MODULATION_QPSK |
					      STM_FE_DEMOD_MODULATION_16QAM |
					      STM_FE_DEMOD_MODULATION_64QAM),
			 .u_tune.dvbt.features =
					(STM_FE_DEMOD_FEATURE_AUTO_FEC |
					 STM_FE_DEMOD_FEATURE_AUTO_MODULATION),
			 .u_tune.dvbt.fft_modes = (STM_FE_DEMOD_FFT_MODE_2K |
						   STM_FE_DEMOD_FFT_MODE_4K |
						   STM_FE_DEMOD_FFT_MODE_8K),
			 .u_tune.dvbt.guards = (STM_FE_DEMOD_GUARD_1_4 |
						STM_FE_DEMOD_GUARD_1_8  |
						STM_FE_DEMOD_GUARD_1_16 |
						STM_FE_DEMOD_GUARD_1_32),
			 .u_tune.dvbt.hierarchys =
					(STM_FE_DEMOD_HIERARCHY_LOW |
					 STM_FE_DEMOD_HIERARCHY_HIGH),
			 },
	.scan_features = STM_FE_DEMOD_FEATURE_NONE,
};

stm_fe_demod_caps_t dummy_demod_caps = {
	.num_stds = 1,
	.tune_caps[0] = {
			 .std = STM_FE_DEMOD_TX_STD_DVBT,
			 .u_tune.dvbt.freq_min = 0,
			 .u_tune.dvbt.freq_max = 858200,
			 .u_tune.dvbt.fecs = (STM_FE_DEMOD_FEC_RATE_1_2 |
					      STM_FE_DEMOD_FEC_RATE_2_3 |
					      STM_FE_DEMOD_FEC_RATE_3_4 |
					      STM_FE_DEMOD_FEC_RATE_4_5 |
					      STM_FE_DEMOD_FEC_RATE_5_6 |
					      STM_FE_DEMOD_FEC_RATE_6_7 |
					      STM_FE_DEMOD_FEC_RATE_7_8),
			 .u_tune.dvbt.mods = (STM_FE_DEMOD_MODULATION_QPSK |
					      STM_FE_DEMOD_MODULATION_16QAM |
					      STM_FE_DEMOD_MODULATION_64QAM),
			 .u_tune.dvbt.features =
					(STM_FE_DEMOD_FEATURE_AUTO_FEC |
					 STM_FE_DEMOD_FEATURE_AUTO_MODULATION),
			 .u_tune.dvbt.fft_modes = (STM_FE_DEMOD_FFT_MODE_2K |
						   STM_FE_DEMOD_FFT_MODE_4K |
						   STM_FE_DEMOD_FFT_MODE_8K),
			 .u_tune.dvbt.guards = (STM_FE_DEMOD_GUARD_1_4 |
						STM_FE_DEMOD_GUARD_1_8  |
						STM_FE_DEMOD_GUARD_1_16 |
						STM_FE_DEMOD_GUARD_1_32),
			 .u_tune.dvbt.hierarchys =
					(STM_FE_DEMOD_HIERARCHY_LOW |
					 STM_FE_DEMOD_HIERARCHY_HIGH),
			 },
	.scan_features = STM_FE_DEMOD_FEATURE_NONE,
};

/* TCH_FIX - BEGIN - FE SiLabs_2xSi2147_Si21652 */
stm_fe_demod_caps_t SiLabs_2xSi2147_Si21652_caps = {
	.num_stds = 1,
	.tune_caps[0] = {
        .std = STM_FE_DEMOD_TX_STD_DVBT,
        .u_tune.dvbt.freq_min = 104000,
        .u_tune.dvbt.freq_max = 870000,
        .u_tune.dvbt.fecs = (STM_FE_DEMOD_FEC_RATE_1_2 |
                             STM_FE_DEMOD_FEC_RATE_2_3 |
                             STM_FE_DEMOD_FEC_RATE_3_4 |
                             STM_FE_DEMOD_FEC_RATE_5_6 |
                             STM_FE_DEMOD_FEC_RATE_7_8 |
                             STM_FE_DEMOD_FEC_RATE_AUTO),
        .u_tune.dvbt.mods = (STM_FE_DEMOD_MODULATION_QPSK   |
                             STM_FE_DEMOD_MODULATION_16QAM  |
                             STM_FE_DEMOD_MODULATION_64QAM  |
                             STM_FE_DEMOD_MODULATION_AUTO),
        .u_tune.dvbt.features = (STM_FE_DEMOD_FEATURE_AUTO_FEC        |
                                 STM_FE_DEMOD_FEATURE_AUTO_MODULATION |
                                 STM_FE_DEMOD_FEATURE_AUTO_GUARD      |
                                 STM_FE_DEMOD_FEATURE_AUTO_MODE), /* Transmission Mode */
        .u_tune.dvbt.fft_modes = (STM_FE_DEMOD_FFT_MODE_2K |
                                  /* 4K is only defined in DVB-H standard, not in DVB-T.
                                   * And Si21652 demod only supports DVB-T.
                                   */
                                  /* STM_FE_DEMOD_FFT_MODE_4K | */
                                  STM_FE_DEMOD_FFT_MODE_8K),
        .u_tune.dvbt.guards = 	(STM_FE_DEMOD_GUARD_1_4 |
                                 STM_FE_DEMOD_GUARD_1_8  |
                                 STM_FE_DEMOD_GUARD_1_16 |
                                 STM_FE_DEMOD_GUARD_1_32),
        .u_tune.dvbt.hierarchys = (STM_FE_DEMOD_HIERARCHY_LOW |
                                   STM_FE_DEMOD_HIERARCHY_HIGH),
    },
	.scan_features = (STM_FE_DEMOD_FEATURE_AUTO_FEC        |
                      STM_FE_DEMOD_FEATURE_AUTO_MODULATION |
                      STM_FE_DEMOD_FEATURE_AUTO_GUARD      |
                      STM_FE_DEMOD_FEATURE_AUTO_MODE),
};
/* TCH_FIX - END - FE SiLabs_2xSi2147_Si21652 */

#endif /* _DEMOD_CAPS_H */
