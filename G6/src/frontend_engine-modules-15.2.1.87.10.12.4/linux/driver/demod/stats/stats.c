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

Source file name : stats.c
Author :           Ashish Gandhi

stm_fe statistics reporting via sysfs

Date        Modification                                    Name
----        ------------                                    --------
10-Feb-12   Created                                         Ashish Gandhi
29-Feb-12   Modified					    Ashish Gandhi
22-Mar-12   Modified					    Ashish Gandhi
18-Sep-12   Modified					    André Draszik
************************************************************************/
#include <linux/bug.h>
#include <linux/kthread.h>
#include <linux/platform_device.h>
#include <linux/stm/plat_dev.h>
#include <linux/stm/demod.h>
#include <linux/stm/lnb.h>
#include <linux/stm/diseqc.h>
#include <stm_registry.h>
#include <stm_fe.h>
#include <stfe_utilities.h>
#include <stm_fe_os.h>
#include <i2c_wrapper.h>
#include <stm_registry.h>
#include <stm_fe_demod.h>
#include <linux/log2.h>
#include "stats.h"

#define STM_REGISTRY_DEMOD_INFO "dt_demod_info"
#define STM_REGISTRY_LAST_ACQ_TIME "dt_last_acq_time_ms"
#define LAST_ACQ_TIME_NAME "last_acq_time_ms"
#define DEMOD_INFO_NAME "demod_info"
#define DEMOD_INFO(std, f) (demod_info->u_channel.std.f)
#define FIRST 0
#define LAST 1

/*!
* @brief	Converts enum value into corresponding index value
*		of the static array implemented in conversion functions
*/
int stmfe_etoi(int enum_val, int arr_size, int pos)
{
	int i, k;
	i = ffs(enum_val);
	/*check if more than one bit is set*/
	k = ffs(enum_val >> i);

	/* if the position of "UNKNOWN" is LAST */
	if (pos) {
		if ((i >= arr_size) || (k))
			i = arr_size - 1;
	}
	/* if the position of "UNKNOWN" is FIRST */
	else {
		if ((i >= arr_size) || (k))
			i = 0;
	}
	return i;
}

/*!
 * * @brief	Converts bandwidth enum value into string value
 * * @return	string value corresponding to input enum value
 * *		return value is "UNKNOWN" when invalid value is
 * *		passed as input
 * */
char *stmfe_bw_conv(stm_fe_demod_bw_t bandwidth)
{
	int i;
	static char bw[][7] = {
		"UNKNOWN", "1.7", "5", "6", "7", "8", "10"
	};

	i = stmfe_etoi(bandwidth, ARRAY_SIZE(bw), FIRST);

	return bw[i];
}

/*!
* @brief	Converts FEC enum value into string value
* @return	string value corresponding to input enum value
*		return value is "UNKNOWN" when invalid value is
*		passed as input
*/
char *stmfe_fec_conv(stm_fe_demod_fec_rate_t fec_rate)
{
	int i;
	static char fec[][10] = {
		"NONE", "1_2", "2_3", "3_4", "4_5", "5_6", "6_7", "7_8", "8_9",
		"1_4", "1_3", "2_5", "3_5", "9_10", "UNKNOWN"
	};

	i = stmfe_etoi(fec_rate, ARRAY_SIZE(fec), LAST);

	return fec[i];
}

/*!
* @brief	Converts Modulation enum value into string value
* @return	string value corresponding to input enum value
*		return value is "UNKNOWN" when invalid value is
*		passed as input
*/
char *stmfe_mod_conv(stm_fe_demod_modulation_t modulation)
{
	int i;
	static char mod[][10] = {
		"NONE", "BPSK", "QPSK", "8PSK", "16APSK", "32APSK", "4QAM",
		"16QAM", "32QAM", "64QAM", "128QAM", "256QAM", "512QAM",
		"1024QAM", "2VSB", "4VSB", "8VSB", "16VSB", "UNKNOWN"
	};

	i = stmfe_etoi(modulation, ARRAY_SIZE(mod), LAST);

	return mod[i];
}

/*!
* @brief	Converts Inversion enum value into string value
* @return	string value corresponding to input enum value
*		return value is "UNKNOWN" when invalid value is
*		passed as input
*/
char *stmfe_inv_conv(stm_fe_demod_spectral_inversion_t inversion)
{
	int i;
	static char inv[][10] = {
		"OFF", "ON", "UNKNOWN"
	};

	i = stmfe_etoi(inversion, ARRAY_SIZE(inv), LAST);

	return inv[i];
}

/*!
* @brief	Converts status enum value into string value
* @return	string value corresponding to input enum value
*		return value is "UNKNOWN" when invalid value is
*		passed as input
*/
char *stmfe_status_conv(stm_fe_demod_status_t status)
{
	int i;
	static char stat[][15] = {
		"STATUS_UNKNOWN", "SIGNAL_FOUND", "NO_SIGNAL", "SYNC_OK",
		"UNKNOWN"
	};

	i = stmfe_etoi(status, ARRAY_SIZE(stat), LAST);

	return stat[i];
}

/*!
* @brief	Converts Roll off enum value into string value
* @return	string value corresponding to input enum value
*		return value is "UNKNOWN" when invalid value is
*		passed as input
*/
char *stmfe_roll_off_conv(stm_fe_demod_roll_off_t rf)
{
	int i;
	static char roll_off[][10] = {
		"UNKNOWN", "0_20", "0_25", "0_35"
	};

	i = stmfe_etoi(rf, ARRAY_SIZE(roll_off), FIRST);

	return roll_off[i];
}

/*!
* @brief	Converts frame length enum value into string value
* @return	string value corresponding to input enum value
*		return value is "UNKNOWN" when invalid value is
*		passed as input
*/
char *stmfe_frame_len_conv(stm_fe_demod_frame_length_t fl)
{
	int i;
	static char frame_length[][10] = {
		"UNKNOWN", "16200", "64800"
	};

	i = stmfe_etoi(fl, ARRAY_SIZE(frame_length), FIRST);

	return frame_length[i];
}

/*!
* @brief	Converts hierarchy enum value into string value
* @return	string value corresponding to input enum value
*		return value is "UNKNOWN" when invalid value is
*		passed as input
*/
char *stmfe_hier_conv(stm_fe_demod_hierarchy_t hierarchy)
{
	int i;
	static char hier[][10] = {
		"NONE", "LOW", "HIGH", "A", "B", "C", "UNKNOWN"
	};

	i = stmfe_etoi(hierarchy, ARRAY_SIZE(hier), LAST);

	return hier[i];
}

/*!
* @brief	Converts alpha enum value into string value
* @return	string value corresponding to input enum value
*		return value is "UNKNOWN" when invalid value is
*		passed as input
*/
char *stmfe_alpha_conv(stm_fe_demod_hierarchy_alpha_t alpha)
{
	int i;
	static char hier_alpha[][10] = {
		"NONE", "ALPHA_1", "ALPHA_2", "ALPHA_4", "UNKNOWN"
	};

	i = stmfe_etoi(alpha, ARRAY_SIZE(hier_alpha), LAST);

	return hier_alpha[i];
}

/*!
* @brief	Converts guard enum value into string value
* @return	string value corresponding to input enum value
*		return value is "UNKNOWN" when invalid value is
*		passed as input
*/
char *stmfe_guard_conv(stm_fe_demod_guard_t gd)
{
	int i;
	static char guard[][10] = {
		"UNKNOWN", "1_4", "1_8", "1_16", "1_32", "1_128", "19_128",
		"19_256"
	};

	i = stmfe_etoi(gd, ARRAY_SIZE(guard), FIRST);

	return guard[i];
}

/*!
* @brief	Converts fft mode enum value into string value
* @return	string value corresponding to input enum value
*		return value is "UNKNOWN" when invalid value is
*		passed as input
*/
char *stmfe_fft_mode_conv(stm_fe_demod_fft_mode_t fft_mode)
{
	int i;
	static char mode[][10] = {
		"UNKNOWN", "1K", "2K", "4K", "8K", "16K", "32K"
	};

	i = stmfe_etoi(fft_mode, ARRAY_SIZE(mode), FIRST);

	return mode[i];
}

/*!
* @brief	prints statistics for the dvbs standard into the user buffer
* @return	number of characters put in user buffer
*/
int stmfe_print_dvbs_stats(stm_fe_demod_channel_info_t *demod_info,
						char **user_buf, int user_size)
{
	int total_write = 0;
	total_write = scnprintf(*user_buf,
				user_size,
				"Status:%s\n",
				stmfe_status_conv(DEMOD_INFO(dvbs, status)));

	if (DEMOD_INFO(dvbs, status) == STM_FE_DEMOD_NO_SIGNAL)
		total_write += scnprintf(*user_buf + total_write,
					user_size - total_write,
					"**** The information below belongs to "
					"the last lock ****");

	total_write += scnprintf(*user_buf + total_write,
				user_size - total_write,
				"\n");
	total_write += scnprintf(*user_buf + total_write,
				user_size - total_write,
				"Frequency(kHz):%d\n"
				"Symbol Rate(sps):%d\n"
				"FEC:%s\n"
				"INV:%s\n"
				"MOD:%s\n"
				"Signal Strength(dBm):%d\n"
				"SNR:%d\n"
				"BER(*10^-7):%d\n",
				DEMOD_INFO(dvbs, freq),
				DEMOD_INFO(dvbs, sr),
				stmfe_fec_conv(DEMOD_INFO(dvbs, fec)),
				stmfe_inv_conv(DEMOD_INFO(dvbs, inv)),
				stmfe_mod_conv(DEMOD_INFO(dvbs, mod)),
				DEMOD_INFO(dvbs, signal_strength),
				DEMOD_INFO(dvbs, snr),
				DEMOD_INFO(dvbs, ber));
	return total_write;
}

/*!
* @brief	prints statistics for the dvbs2 standard into the user buffer
* @return	number of characters put in user buffer
*/
int stmfe_print_dvbs2_stats(stm_fe_demod_channel_info_t *demod_info,
						char **user_buf, int user_size)
{
	int total_write = 0;
	total_write = scnprintf(*user_buf,
				user_size,
				"Status:%s\n",
				stmfe_status_conv(DEMOD_INFO(dvbs2, status)));

	if (DEMOD_INFO(dvbs2, status) == STM_FE_DEMOD_NO_SIGNAL)
		total_write += scnprintf(*user_buf + total_write,
					user_size - total_write,
					"**** The information below belongs to "
					"the last lock ****");

	total_write += scnprintf(*user_buf + total_write,
				user_size - total_write,
				"\n");
	total_write += scnprintf(*user_buf + total_write,
			user_size - total_write,
			"Frequency(kHz):%d\n"
			"Symbol Rate(sps):%d\n"
			"FEC:%s\n"
			"INV:%s\n"
			"MOD:%s\n"
			"ROLL OFF:%s\n"
			"Signal Strength(dBm):%d\n"
			"SNR:%d\n"
			"PER(*10^-7):%d\n"
			"Frame Length:%s\n"
			"Pilot Status:%s\n",
			DEMOD_INFO(dvbs2, freq),
			DEMOD_INFO(dvbs2, sr),
			stmfe_fec_conv(DEMOD_INFO(dvbs2, fec)),
			stmfe_inv_conv(DEMOD_INFO(dvbs2, inv)),
			stmfe_mod_conv(DEMOD_INFO(dvbs2, mod)),
			stmfe_roll_off_conv(DEMOD_INFO(dvbs2, roll_off)),
			DEMOD_INFO(dvbs2, signal_strength),
			DEMOD_INFO(dvbs2, snr),
			DEMOD_INFO(dvbs2, per),
			stmfe_frame_len_conv(DEMOD_INFO(dvbs2, frame_len)),
			(DEMOD_INFO(dvbs2, pilots) ? "ON" : "OFF"));
	return total_write;
}

/*!
* @brief	prints statistics for the dvbc standard into the user buffer
* @return	number of characters put in user buffer
*/
int stmfe_print_dvbc_stats(stm_fe_demod_channel_info_t *demod_info,
						char **user_buf, int user_size)
{
	int total_write = 0;
	total_write = scnprintf(*user_buf,
				user_size,
				"Status:%s\n",
				stmfe_status_conv(DEMOD_INFO(dvbc, status)));

	if (DEMOD_INFO(dvbc, status) == STM_FE_DEMOD_NO_SIGNAL)
		total_write += scnprintf(*user_buf + total_write,
					user_size - total_write,
					"**** The information below belongs to "
					"the last lock ****");

	total_write += scnprintf(*user_buf + total_write,
				user_size - total_write,
				"\n");
	total_write += scnprintf(*user_buf + total_write,
				user_size - total_write,
				"Frequency(kHz):%d\n"
				"Symbol Rate(sps):%d\n"
				"INV:%s\n"
				"MOD:%s\n"
				"Signal Strength(dBm):%d\n"
				"SNR:%d\n"
				"BER(*10^-7):%d\n"
				"UCB:%d\n",
				DEMOD_INFO(dvbc, freq),
				DEMOD_INFO(dvbc, sr),
				stmfe_inv_conv(DEMOD_INFO(dvbc, inv)),
				stmfe_mod_conv(DEMOD_INFO(dvbc, mod)),
				DEMOD_INFO(dvbc, signal_strength),
				DEMOD_INFO(dvbc, snr),
				DEMOD_INFO(dvbc, ber),
				DEMOD_INFO(dvbc, ucb));
	return total_write;
}

/*!
* @brief	prints statistics for the dvbc2 standard into the user buffer
* @return	number of characters put in user buffer
*/
int stmfe_print_dvbc2_stats(stm_fe_demod_channel_info_t *demod_info,
						char **user_buf, int user_size)
{
	int total_write = 0;
	return total_write;
}

/*!
* @brief	prints statistics for the j83ac standard into the user buffer
* @return	number of characters put in user buffer
*/
int stmfe_print_j83ac_stats(stm_fe_demod_channel_info_t *demod_info,
						char **user_buf, int user_size)
{
	int total_write = 0;
	total_write = scnprintf(*user_buf,
				user_size,
				"Status:%s\n",
				stmfe_status_conv(DEMOD_INFO(j83ac, status)));

	if (DEMOD_INFO(j83ac, status) == STM_FE_DEMOD_NO_SIGNAL)
		total_write += scnprintf(*user_buf + total_write,
					user_size - total_write,
					"**** The information below belongs to "
					"the last lock ****");

	total_write += scnprintf(*user_buf + total_write,
				user_size - total_write,
				"\n");
	total_write += scnprintf(*user_buf + total_write,
				user_size - total_write,
				"Frequency(kHz):%d\n"
				"Symbol Rate(sps):%d\n"
				"INV:%s\n"
				"MOD:%s\n"
				"Signal Strength(dBm):%d\n"
				"SNR:%d\n"
				"BER(*10^-7):%d\n"
				"UCB:%d\n",
				DEMOD_INFO(j83ac, freq),
				DEMOD_INFO(j83ac, sr),
				stmfe_inv_conv(DEMOD_INFO(j83ac, inv)),
				stmfe_mod_conv(DEMOD_INFO(j83ac, mod)),
				DEMOD_INFO(j83ac, signal_strength),
				DEMOD_INFO(j83ac, snr),
				DEMOD_INFO(j83ac, ber),
				DEMOD_INFO(j83ac, ucb));
	return total_write;
}

/*!
* @brief	prints statistics for the j83b standard into the user buffer
* @return	number of characters put in user buffer
*/
int stmfe_print_j83b_stats(stm_fe_demod_channel_info_t *demod_info,
						char **user_buf, int user_size)
{
	int total_write = 0;
	total_write = scnprintf(*user_buf,
				user_size,
				"Status:%s\n",
				stmfe_status_conv(DEMOD_INFO(j83b, status)));

	if (DEMOD_INFO(j83b, status) == STM_FE_DEMOD_NO_SIGNAL)
		total_write += scnprintf(*user_buf + total_write,
					user_size - total_write,
					"**** The information below belongs to "
					"the last lock ****");

	total_write += scnprintf(*user_buf + total_write,
				user_size - total_write,
				"\n");
	total_write += scnprintf(*user_buf + total_write,
				user_size - total_write,
				"Frequency(kHz):%d\n"
				"Symbol Rate(sps):%d\n"
				"INV:%s\n"
				"MOD:%s\n"
				"Signal Strength(dBm):%d\n"
				"SNR:%d\n"
				"BER(*10^-7):%d\n"
				"UCB:%d\n",
				DEMOD_INFO(j83b, freq),
				DEMOD_INFO(j83b, sr),
				stmfe_inv_conv(DEMOD_INFO(j83b, inv)),
				stmfe_mod_conv(DEMOD_INFO(j83b, mod)),
				DEMOD_INFO(j83b, signal_strength),
				DEMOD_INFO(j83b, snr),
				DEMOD_INFO(j83b, ber),
				DEMOD_INFO(j83b, ucb));
	return total_write;
}

/*!
* @brief	prints statistics for the dvbt standard into the user buffer
* @return	number of characters put in user buffer
*/
int stmfe_print_dvbt_stats(stm_fe_demod_channel_info_t *demod_info,
						char **user_buf, int user_size)
{
	int total_write = 0;
	total_write = scnprintf(*user_buf,
				user_size,
				"Status:%s\n",
				stmfe_status_conv(DEMOD_INFO(dvbt, status)));

	if (DEMOD_INFO(dvbt, status) == STM_FE_DEMOD_NO_SIGNAL)
		total_write += scnprintf(*user_buf + total_write,
					user_size - total_write,
					"**** The information below belongs to "
					"the last lock ****");

	total_write += scnprintf(*user_buf + total_write,
				user_size - total_write,
				"\n");
	total_write += scnprintf(*user_buf + total_write,
				user_size - total_write,
				"Frequency(kHz):%d\n"
				"Bandwidth(MHz):%s\n"
				"FEC:%s\n"
				"INV:%s\n"
				"MOD:%s\n"
				"Hierarchy:%s\n"
				"Alpha:%s\n"
				"Guard Interval:%s\n"
				"FFT Mode:%s\n"
				"Signal Strength(dBm):%d\n"
				"SNR:%d\n"
				"BER(*10^-7):%d\n",
				DEMOD_INFO(dvbt, freq),
				stmfe_bw_conv(DEMOD_INFO(dvbt, bw)),
				stmfe_fec_conv(DEMOD_INFO(dvbt, fec)),
				stmfe_inv_conv(DEMOD_INFO(dvbt, inv)),
				stmfe_mod_conv(DEMOD_INFO(dvbt, mod)),
				stmfe_hier_conv(DEMOD_INFO(dvbt, hierarchy)),
				stmfe_alpha_conv(DEMOD_INFO(dvbt, alpha)),
				stmfe_guard_conv(DEMOD_INFO(dvbt, guard)),
				stmfe_fft_mode_conv(DEMOD_INFO(dvbt, fft_mode)),
				DEMOD_INFO(dvbt, signal_strength),
				DEMOD_INFO(dvbt, snr),
				DEMOD_INFO(dvbt, ber));
	return total_write;
}

/*!
* @brief	prints statistics for the dvbt2 standard into the user buffer
* @return	number of characters put in user buffer
*/
int stmfe_print_dvbt2_stats(stm_fe_demod_channel_info_t *demod_info,
						char **user_buf, int user_size)
{
	int total_write = 0;
	total_write = scnprintf(*user_buf,
				user_size,
				"Status:%s\n",
				stmfe_status_conv(DEMOD_INFO(dvbt2, status)));

	if (DEMOD_INFO(dvbt2, status) == STM_FE_DEMOD_NO_SIGNAL)
		total_write += scnprintf(*user_buf + total_write,
					user_size - total_write,
					"**** The information below belongs to "
					"the last lock ****");

	total_write += scnprintf(*user_buf + total_write,
				user_size - total_write,
				"\n");
	total_write += scnprintf(*user_buf + total_write,
			user_size - total_write,
			"Frequency(kHz):%d\n"
			"Bandwidth(MHz):%s\n"
			"FEC:%s\n"
			"INV:%s\n"
			"MOD:%s\n"
			"Hierarchy:%s\n"
			"Alpha:%s\n"
			"Guard Interval:%s\n"
			"FFT Mode:%s\n"
			"PLP Id:%d\n"
			"Signal Strength(dBm):%d\n"
			"SNR:%d\n"
			"BER(*10^-7):%d\n"
			"FER(*10^-7):%d\n",
			DEMOD_INFO(dvbt2, freq),
			stmfe_bw_conv(DEMOD_INFO(dvbt2, bw)),
			stmfe_fec_conv(DEMOD_INFO(dvbt2, fec)),
			stmfe_inv_conv(DEMOD_INFO(dvbt2, inv)),
			stmfe_mod_conv(DEMOD_INFO(dvbt2, mod)),
			stmfe_hier_conv(DEMOD_INFO(dvbt2, hierarchy)),
			stmfe_alpha_conv(DEMOD_INFO(dvbt2, alpha)),
			stmfe_guard_conv(DEMOD_INFO(dvbt2, guard)),
			stmfe_fft_mode_conv(DEMOD_INFO(dvbt2, fft_mode)),
			DEMOD_INFO(dvbt2, plp_id),
			DEMOD_INFO(dvbt2, signal_strength),
			DEMOD_INFO(dvbt2, snr),
			DEMOD_INFO(dvbt2, ber),
			DEMOD_INFO(dvbt2, fer));
	return total_write;
}

/*!
* @brief	prints statistics for the atsc standard into the user buffer
* @return	number of characters put in user buffer
*/
int stmfe_print_atsc_stats(stm_fe_demod_channel_info_t *demod_info,
						char **user_buf, int user_size)
{
	int total_write = 0;
	total_write = scnprintf(*user_buf,
				user_size,
				"Status:%s\n",
				stmfe_status_conv(DEMOD_INFO(atsc, status)));

	if (DEMOD_INFO(atsc, status) == STM_FE_DEMOD_NO_SIGNAL)
		total_write += scnprintf(*user_buf + total_write,
					user_size - total_write,
					"**** The information below belongs to "
					"the last lock ****");

	total_write += scnprintf(*user_buf + total_write,
				user_size - total_write,
				"\n");
	total_write += scnprintf(*user_buf + total_write,
				user_size - total_write,
				"Frequency(kHz):%d\n"
				"Symbol Rate(sps):%d\n"
				"FEC:%s\n"
				"INV:%s\n"
				"MOD:%s\n"
				"Signal Strength(dBm):%d\n"
				"SNR:%d\n"
				"BER(*10^-7):%d\n",
				DEMOD_INFO(atsc, freq),
				DEMOD_INFO(atsc, sr),
				stmfe_fec_conv(DEMOD_INFO(atsc, fec)),
				stmfe_inv_conv(DEMOD_INFO(atsc, inv)),
				stmfe_mod_conv(DEMOD_INFO(atsc, mod)),
				DEMOD_INFO(atsc, signal_strength),
				DEMOD_INFO(atsc, snr),
				DEMOD_INFO(atsc, ber));
	return total_write;
}

/*!
* @brief	prints statistics for the isdbt standard into the user buffer
* @return	number of characters put in user buffer
*/
int stmfe_print_isdbt_stats(stm_fe_demod_channel_info_t *demod_info,
						char **user_buf, int user_size)
{
	int total_write = 0;
	total_write = scnprintf(*user_buf,
				user_size,
				"Status:%s\n",
				stmfe_status_conv(DEMOD_INFO(isdbt, status)));

	if (DEMOD_INFO(isdbt, status) == STM_FE_DEMOD_NO_SIGNAL)
		total_write += scnprintf(*user_buf + total_write,
					user_size - total_write,
					"**** The information below belongs to "
					"the last lock ****");

	total_write += scnprintf(*user_buf + total_write,
				user_size - total_write,
				"\n");
	total_write += scnprintf(*user_buf + total_write,
			user_size - total_write,
			"Frequency(kHz):%d\n"
			"Bandwidth(MHz):%s\n"
			"FEC:%s\n"
			"INV:%s\n"
			"MOD:%s\n"
			"Hierarchy:%d\t%s\n"
			"Guard Interval:%s\n"
			"FFT Mode:%s\n"
			"Signal Strength(dBm):%d\n"
			"SNR:%d\n"
			"BER(*10^-7):%d\n",
			DEMOD_INFO(isdbt, freq),
			stmfe_bw_conv(DEMOD_INFO(isdbt, bw)),
			stmfe_fec_conv(DEMOD_INFO(isdbt, fec)),
			stmfe_inv_conv(DEMOD_INFO(isdbt, inv)),
			stmfe_mod_conv(DEMOD_INFO(isdbt, mod)),
			DEMOD_INFO(isdbt, hierarchy),
			stmfe_hier_conv(DEMOD_INFO(isdbt, hierarchy)),
			stmfe_guard_conv(DEMOD_INFO(isdbt, guard)),
			stmfe_fft_mode_conv(DEMOD_INFO(isdbt, fft_mode)),
			DEMOD_INFO(isdbt, signal_strength),
			DEMOD_INFO(isdbt, snr),
			DEMOD_INFO(isdbt, ber));
	return total_write;
}

/*!
* @brief	prints statistics for the analog standard into the user buffer
* @return	number of characters put in user buffer
*/
int stmfe_print_analog_stats(stm_fe_demod_channel_info_t *demod_info,
						char **user_buf, int user_size)
{
	int total_write = 0;
	total_write = scnprintf(*user_buf,
				user_size,
				"Status:%s\n",
				stmfe_status_conv(DEMOD_INFO(analog, status)));

	if (DEMOD_INFO(analog, status) == STM_FE_DEMOD_NO_SIGNAL)
		total_write += scnprintf(*user_buf + total_write,
					user_size - total_write,
					"**** The information below belongs to "
					"the last lock ****");

	total_write += scnprintf(*user_buf + total_write,
				user_size - total_write,
				"\n");
	total_write += scnprintf(*user_buf + total_write,
				user_size - total_write,
				"Frequency(kHz):%d\n"
				"Signal Strength(dBm):%d\n"
				"SNR:%d\n",
				DEMOD_INFO(analog, freq),
				DEMOD_INFO(analog, signal_strength),
				DEMOD_INFO(analog, snr));
	return total_write;
}

/*!
* @brief	prints statistics for the invalid standard into the user buffer
* @return	number of characters put in user buffer
*/
int stmfe_print_invalid_std(stm_fe_demod_channel_info_t *demod_info,
						char **user_buf, int user_size)
{
	int total_write = 0;
	total_write = scnprintf(*user_buf, user_size, "Invalid Standard\n");
	return total_write;
}

/*!
* @brief	prints demod information statistics into the user buffer
* @param[in]	object	stm_object_h type variable which is void*
*		and a handle to the object under which
*		demod info attribute was placed
* @param[in]	buf	buffer to be read from
* @param[in]	size	size of the buffer to be read from
* @param[in]	user_buf	user buffer to be printed into
* @param[in]	user_size	size of the user buffer
* @return	number of characters put in user buffer
*/
int demod_info_print_handler(stm_object_h object, char *buf, size_t size,
					char *user_buf, size_t user_size)
{
	int32_t std = -1;
	int total_write = 0;
	struct stm_fe_demod_s *demod = object;
	stm_fe_demod_channel_info_t demod_info =
					demod->info.u_info.tune.demod_info;
	int (*stmfe_print_std_stats)(stm_fe_demod_channel_info_t *,
							   char **, int) = NULL;

	std = demod_info.std;

	if (0 == std)
		return total_write;

	switch (std) {
	case STM_FE_DEMOD_TX_STD_DVBS:
		stmfe_print_std_stats = stmfe_print_dvbs_stats;
		break;

	case STM_FE_DEMOD_TX_STD_DVBS2:
		stmfe_print_std_stats = stmfe_print_dvbs2_stats;
		break;

	case STM_FE_DEMOD_TX_STD_DVBC:
		stmfe_print_std_stats = stmfe_print_dvbc_stats;
		break;

	case STM_FE_DEMOD_TX_STD_DVBC2:
		stmfe_print_std_stats = stmfe_print_dvbc2_stats;
		break;

	case STM_FE_DEMOD_TX_STD_J83_AC:
		stmfe_print_std_stats = stmfe_print_j83ac_stats;
		break;

	case STM_FE_DEMOD_TX_STD_J83_B:
		stmfe_print_std_stats = stmfe_print_j83b_stats;
		break;

	case STM_FE_DEMOD_TX_STD_DVBT:
		stmfe_print_std_stats = stmfe_print_dvbt_stats;
		break;

	case STM_FE_DEMOD_TX_STD_DVBT2:
		stmfe_print_std_stats = stmfe_print_dvbt2_stats;
		break;

	case STM_FE_DEMOD_TX_STD_ATSC:
		stmfe_print_std_stats = stmfe_print_atsc_stats;
		break;

	case STM_FE_DEMOD_TX_STD_ISDBT:
		stmfe_print_std_stats = stmfe_print_isdbt_stats;
		break;

	case STM_FE_DEMOD_TX_STD_PAL:
		stmfe_print_std_stats = stmfe_print_analog_stats;
		break;

	case STM_FE_DEMOD_TX_STD_SECAM:
		stmfe_print_std_stats = stmfe_print_analog_stats;
		break;

	case STM_FE_DEMOD_TX_STD_NTSC:
		stmfe_print_std_stats = stmfe_print_analog_stats;
		break;

	default:
		stmfe_print_std_stats = stmfe_print_invalid_std;
		break;
	}
	total_write = stmfe_print_std_stats(&demod_info, &user_buf, user_size);

	return total_write;
}

/*!
* @brief	prints last acquisition time into the user buffer
* @param[in]	object	stm_object_h type variable which is void*
*		and a handle to the object under which
*		demod info attribute was placed
* @param[in]	buf	buffer to be read from
* @param[in]	size	size of the buffer to be read from
* @param[in]	user_buf	user buffer to be printed into
* @param[in]	user_size	size of the user buffer
* @return	number of characters put in user buffer
*/
int last_acq_time_print_handler(stm_object_h object, char *buf, size_t size,
					char *user_buf, size_t user_size)
{
	int total_write = 0;
	struct stm_fe_demod_s *demod = object;

	uint32_t last_acq_time_ms =
		 stm_fe_jiffies_to_msecs(demod->stat_attrs.last_acq_time_ticks);
	total_write = scnprintf(user_buf, user_size, "%d\n", last_acq_time_ms);

	return total_write;
}

/*!
* @brief	initialisation of the statistics (adding attributes)
* @param[in]	demod	pointer to stm_fe_demod_s structure
*/
int stmfe_stats_init(struct stm_fe_demod_s *demod)
{
	int err = -1;
	int ret = 0;
	char dt_demod_info[20], dt_last_acq_time_ms[30];
	stm_registry_type_def_t def1, def2;

	snprintf(dt_demod_info, sizeof(dt_demod_info),
				STM_REGISTRY_DEMOD_INFO "_%d", demod->demod_id);
	snprintf(dt_last_acq_time_ms, sizeof(dt_last_acq_time_ms),
			     STM_REGISTRY_LAST_ACQ_TIME "_%d", demod->demod_id);

	def1.print_handler = demod_info_print_handler;
	err = stm_registry_add_data_type(dt_demod_info, &def1);
	if (err) {
		pr_err("%s: reg add_data_type failed: %d\n", __func__, err);
		return err;
	}

	err = stm_registry_add_attribute((stm_object_h)demod,
						DEMOD_INFO_NAME,
						dt_demod_info,
						&(demod->info),
						sizeof(stm_fe_demod_info_t));
	if (err) {
		ret = stm_registry_remove_data_type(dt_demod_info);
		pr_err("%s: reg add_attribute failed: %d\n", __func__, err);
		return err;
	}

	def2.print_handler = last_acq_time_print_handler;
	err = stm_registry_add_data_type(dt_last_acq_time_ms, &def2);
	if (err) {
		ret = stm_registry_remove_attribute((stm_object_h)demod,
							       DEMOD_INFO_NAME);
		ret = stm_registry_remove_data_type(dt_demod_info);
		pr_err("%s: reg add_data_type failed: %d\n", __func__, err);
		return err;
	}

	err = stm_registry_add_attribute((stm_object_h)demod,
				LAST_ACQ_TIME_NAME,
				dt_last_acq_time_ms,
				&(demod->stat_attrs.last_acq_time_ticks),
				sizeof(uint32_t));
	if (err) {
		ret = stm_registry_remove_data_type(dt_last_acq_time_ms);
		ret = stm_registry_remove_attribute((stm_object_h)demod,
							       DEMOD_INFO_NAME);
		ret = stm_registry_remove_data_type(dt_demod_info);
		pr_err("%s: reg add_attribute failed: %d\n", __func__, err);
		return err;
	}

	return err;
}

/*!
* @brief	termination of the statistics (removal of attributes)
* @param[in]	demod	pointer to stm_fe_demod_s structure
*/
int stmfe_stats_term(struct stm_fe_demod_s *demod)
{
	int ret = -1, err = 0;
	char dt_demod_info[20], dt_last_acq_time_ms[30];

	snprintf(dt_demod_info, sizeof(dt_demod_info),
				STM_REGISTRY_DEMOD_INFO "_%d", demod->demod_id);
	snprintf(dt_last_acq_time_ms, sizeof(dt_last_acq_time_ms),
			     STM_REGISTRY_LAST_ACQ_TIME "_%d", demod->demod_id);

	ret = stm_registry_remove_attribute((stm_object_h)demod,
							    LAST_ACQ_TIME_NAME);
	if (ret) {
		pr_err("%s: reg remove_attribute failed: %d\n", __func__, ret);
		err = ret;
	}

	ret = stm_registry_remove_attribute((stm_object_h)demod,
							       DEMOD_INFO_NAME);
	if (ret) {
		pr_err("%s: reg remove_attribute failed: %d\n", __func__, ret);
		err = ret;
	}

	ret = stm_registry_remove_data_type(dt_last_acq_time_ms);
	if (ret) {
		pr_err("%s: reg remove_data_type failed: %d\n", __func__, ret);
		err = ret;
	}

	ret = stm_registry_remove_data_type(dt_demod_info);
	if (ret) {
		pr_err("%s: reg remove_data_type failed: %d\n", __func__, ret);
		err = ret;
	}

	return err;
}
