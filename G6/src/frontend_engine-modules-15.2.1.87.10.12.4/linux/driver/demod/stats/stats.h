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

Source file name : stats.h
Author :           Ashish Gandhi

stm_fe statistics reporting via sysfs

Date        Modification                                    Name
----        ------------                                    --------
10-Feb-12   Created                                         Ashish Gandhi
22-Mar-12   Modified					    Ashish Gandhi
************************************************************************/

#ifndef _STATS_H
#define _STATS_H

int stmfe_etoi(int enum_val, int arr_size, int pos);

char *stmfe_bw_conv(stm_fe_demod_bw_t bandwidth);
char *stmfe_fec_conv(stm_fe_demod_fec_rate_t fec_rate);
char *stmfe_mod_conv(stm_fe_demod_modulation_t modulation);
char *stmfe_inv_conv(stm_fe_demod_spectral_inversion_t inversion);
char *stmfe_status_conv(stm_fe_demod_status_t status);
char *stmfe_roll_off_conv(stm_fe_demod_roll_off_t rf);
char *stmfe_frame_len_conv(stm_fe_demod_frame_length_t fl);
char *stmfe_hier_conv(stm_fe_demod_hierarchy_t hierarchy);
char *stmfe_alpha_conv(stm_fe_demod_hierarchy_alpha_t alpha);
char *stmfe_guard_conv(stm_fe_demod_guard_t gd);
char *stmfe_fft_mode_conv(stm_fe_demod_fft_mode_t fft_mode);

int stmfe_print_dvbs_stats(stm_fe_demod_channel_info_t *demod_info,
						char **user_buf, int user_size);
int stmfe_print_dvbs2_stats(stm_fe_demod_channel_info_t *demod_info,
						char **user_buf, int user_size);
int stmfe_print_dvbc_stats(stm_fe_demod_channel_info_t *demod_info,
						char **user_buf, int user_size);
int stmfe_print_dvbc2_stats(stm_fe_demod_channel_info_t *demod_info,
						char **user_buf, int user_size);
int stmfe_print_j83ac_stats(stm_fe_demod_channel_info_t *demod_info,
						char **user_buf, int user_size);
int stmfe_print_j83b_stats(stm_fe_demod_channel_info_t *demod_info,
						char **user_buf, int user_size);
int stmfe_print_dvbt_stats(stm_fe_demod_channel_info_t *demod_info,
						char **user_buf, int user_size);
int stmfe_print_dvbt2_stats(stm_fe_demod_channel_info_t *demod_info,
						char **user_buf, int user_size);
int stmfe_print_atsc_stats(stm_fe_demod_channel_info_t *demod_info,
						char **user_buf, int user_size);
int stmfe_print_isdbt_stats(stm_fe_demod_channel_info_t *demod_info,
						char **user_buf, int user_size);
int stmfe_print_analog_stats(stm_fe_demod_channel_info_t *demod_info,
						char **user_buf, int user_size);
int stmfe_print_invalid_std(stm_fe_demod_channel_info_t *demod_info,
						char **user_buf, int user_size);

int demod_info_print_handler(stm_object_h object, char *buf, size_t size,
					char *user_buf, size_t user_size);
int last_acq_time_print_handler(stm_object_h object, char *buf, size_t size,
					char *user_buf, size_t user_size);

int stmfe_stats_init(struct stm_fe_demod_s *demod);
int stmfe_stats_term(struct stm_fe_demod_s *demod);

#endif /* _STATS_H */
