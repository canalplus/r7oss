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

Source file name : lla_utils.c
Author :           LLA

LLA generic file

Date        Modification                                    Name
----        ------------                                    --------
23-Apr-12   Created                                         Rahul
18-Sep-12   Modified                                        André

************************************************************************/
#include <linux/types.h>
#include <linux/bug.h>
#include <linux/string.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 2, 2)
#include <linux/export.h>
#else
#include <linux/module.h>
#endif
#include <linux/stm/demod.h>
#include <stm_fe.h>
#include "stfe_utilities.h"
#include "lla_utils.h"

#define IS_NAME_MATCH(_x, _name) (!strncmp((_x), (_name), strlen(_name)))

static enum demod_roll_off_e roll_off_lu[3] = {
	STM_FE_DEMOD_ROLL_OFF_0_35,	/* FE_SAT_35 */
	STM_FE_DEMOD_ROLL_OFF_0_25,	/* FE_SAT_25 */
	STM_FE_DEMOD_ROLL_OFF_0_20	/* FE_SAT_20 */
};

TUNER_Model_t stmfe_set_tuner_model(char *tuner_name)
{
	if (IS_NAME_MATCH(tuner_name, "STB6000"))
		return TUNER_STB6000;
	else if (IS_NAME_MATCH(tuner_name, "STV6110"))
		return TUNER_STV6110;
	else if (IS_NAME_MATCH(tuner_name, "STB6100"))
		return TUNER_STB6100;
	else if (IS_NAME_MATCH(tuner_name, "STB6111"))
		return TUNER_STV6111;
	else if (IS_NAME_MATCH(tuner_name, "STV6120_1"))
		return TUNER_STV6120_Tuner1;
	else if (IS_NAME_MATCH(tuner_name, "STV6120_2"))
		return TUNER_STV6120_Tuner2;
	else if (IS_NAME_MATCH(tuner_name, "STV6140_Tuner1"))
		return TUNER_STV6140_Tuner1;
	else if (IS_NAME_MATCH(tuner_name, "STV6140_Tuner2"))
		return TUNER_STV6140_Tuner2;
	else if (IS_NAME_MATCH(tuner_name, "STV6130"))
		return TUNER_STV6130;
	else if (IS_NAME_MATCH(tuner_name, "STV4100"))
		return TUNER_STV4100;
	else if (IS_NAME_MATCH(tuner_name, "DTT7546"))
		return TUNER_DTT7546;
	else if (IS_NAME_MATCH(tuner_name, "TDA18212"))
		return TUNER_TDA18212;

	return -1;
}
EXPORT_SYMBOL(stmfe_set_tuner_model);

FE_TS_OutputMode_t stmfe_set_ts_output_mode(enum demod_tsout_config_e ts_out)
{
	if (ts_out & DEMOD_TS_SERIAL_PUNCT_CLOCK)
		return FE_TS_SERIAL_PUNCT_CLOCK;

	if (ts_out & DEMOD_TS_SERIAL_CONT_CLOCK)
		return FE_TS_SERIAL_CONT_CLOCK;

	if (ts_out & DEMOD_TS_PARALLEL_PUNCT_CLOCK)
		return FE_TS_PARALLEL_PUNCT_CLOCK;

	if (ts_out & DEMOD_TS_DVBCI_CLOCK)
		return FE_TS_DVBCI_CLOCK;

	return FE_TS_OUTPUTMODE_DEFAULT;
}
EXPORT_SYMBOL(stmfe_set_ts_output_mode);

FE_Sat_IQ_Inversion_t stmfe_set_tuner_iq_inv(bool is_inv)
{
	if (is_inv)
		return FE_SAT_IQ_SWAPPED;
	else
		return FE_SAT_IQ_NORMAL;
}
EXPORT_SYMBOL(stmfe_set_tuner_iq_inv);

int32_t stmfe_set_tuner_bbgain(bool is_vglna, TUNER_Model_t TunerModel)
{
	if (is_vglna)
		return 12;	/* Default if vglna is present */

	return TUNER_BBGAIN(TunerModel);
}
EXPORT_SYMBOL(stmfe_set_tuner_bbgain);

TUNER_WIDEBandS_t stmfe_set_tuner_band(TUNER_Model_t TunerModel)
{
	/* Currently fixed; should come from config */
	if (TunerModel == TUNER_STV6120_Tuner1)
		return LBRFA_HBRFB;
	else
		return LBRFD_HBRFC;

	/* default */
	return LBRFA_HBRFB;
}
EXPORT_SYMBOL(stmfe_set_tuner_band);

void stmfe_set_ts_config(enum demod_tsout_config_e ts_out,
			     uint32_t ts_clock, int path,
			     FE_TS_Config_t *llaconf)
{
	if (ts_out) {
		if (ts_out & DEMOD_TS_SERIAL_PUNCT_CLOCK)
			llaconf->TSMode = FE_TS_SERIAL_PUNCT_CLOCK;
		else if (ts_out & DEMOD_TS_SERIAL_CONT_CLOCK)
			llaconf->TSMode = FE_TS_SERIAL_CONT_CLOCK;
		else if (ts_out & DEMOD_TS_PARALLEL_PUNCT_CLOCK)
			llaconf->TSMode = FE_TS_PARALLEL_PUNCT_CLOCK;
		else if (ts_out & DEMOD_TS_DVBCI_CLOCK)
			llaconf->TSMode = FE_TS_DVBCI_CLOCK;

		llaconf->TSSpeedControl = (ts_out & DEMOD_TS_MANUAL_SPEED) ?
		    FE_TS_MANUAL_SPEED : FE_TS_AUTO_SPEED;

		llaconf->TSClockPolarity =
		    (ts_out & DEMOD_TS_RISINGEDGE_CLOCK) ?
		    FE_TS_RISINGEDGE_CLOCK : FE_TS_FALLINGEDGE_CLOCK;

		llaconf->TSSyncByteEnable = (ts_out & DEMOD_TS_SYNCBYTE_ON) ?
		    FE_TS_SYNCBYTE_ON : FE_TS_SYNCBYTE_OFF;

		llaconf->TSParityBytes = (ts_out & DEMOD_TS_PARITYBYTES_ON) ?
		    FE_TS_PARITYBYTES_ON : FE_TS_PARITYBYTES_OFF;

		llaconf->TSSwap = (ts_out & DEMOD_TS_SWAP_ON) ?
		    FE_TS_SWAP_ON : FE_TS_SWAP_OFF;

		llaconf->TSSmoother = (ts_out & DEMOD_TS_SMOOTHER_ON) ?
		    FE_TS_SMOOTHER_ON : FE_TS_SMOOTHER_OFF;

		llaconf->TSClockRate = ts_clock;
		llaconf->TSDataRate = 0;	/* unused */
		if (!path)
			llaconf->TSOutput = FE_TS_OUTPUT1;
		else
			llaconf->TSOutput = FE_TS_OUTPUT2;
	} else {
		llaconf->TSMode = FE_TS_OUTPUTMODE_DEFAULT;
		llaconf->TSSpeedControl = FE_TS_DATARATECONTROL_DEFAULT;
		llaconf->TSClockPolarity = FE_TS_CLOCKPOLARITY_DEFAULT;
		llaconf->TSClockRate = 90000000;
		llaconf->TSSyncByteEnable = FE_TS_SYNCBYTE_DEFAULT;
		llaconf->TSParityBytes = FE_TS_PARTITYBYTES_DEFAULT;
		llaconf->TSSwap = FE_TS_SWAP_DEFAULT;
		llaconf->TSSmoother = FE_TS_SMOOTHER_DEFAULT;
		llaconf->TSDataRate = 0;	/* unused */
		/* To avoid error from LLA */
		llaconf->TSOutput = FE_TS_OUTPUT3;
	}
}
EXPORT_SYMBOL(stmfe_set_ts_config);

FE_Sat_Tuner_t stmfe_set_sat_tuner_type(char *tuner_name, bool is_auto)
{
	if (!is_auto)
		return FE_SAT_SW_TUNER;

	if (IS_NAME_MATCH(tuner_name, "STB6000"))
		return FE_SAT_AUTO_STB6000;
	else if (IS_NAME_MATCH(tuner_name, "STV6110"))
		return FE_SAT_AUTO_STV6110;
	else if (IS_NAME_MATCH(tuner_name, "STB6100"))
		return FE_SAT_AUTO_STB6100;
	else if ((IS_NAME_MATCH(tuner_name, "STV6120_1"))
		   || (IS_NAME_MATCH(tuner_name, "STV6120_2")))
		return FE_SAT_AUTO_STV6120;
	else
		return FE_SAT_SW_TUNER;
}
EXPORT_SYMBOL(stmfe_set_sat_tuner_type);

FE_Sat_RollOff_t stmfe_set_sat_roll_off(enum demod_roll_off_e roll_off)
{
	FE_Sat_RollOff_t lla_ro;

	for (lla_ro = FE_SAT_35; lla_ro < ARRAY_SIZE(roll_off_lu); lla_ro++)
		if (roll_off_lu[lla_ro] == roll_off)
			return lla_ro;

	return FE_SAT_35;	/* default */
}
EXPORT_SYMBOL(stmfe_set_sat_roll_off);

FE_Sat_Rate_t stmfe_set_sat_fec_rate(stm_fe_demod_fec_rate_t fec_rate)
{
	FE_Sat_Rate_t lla_fecrate = FE_SAT_PR_UNKNOWN;
	switch (fec_rate) {
	case STM_FE_DEMOD_FEC_RATE_AUTO:
		lla_fecrate = FE_SAT_PR_UNKNOWN;
	default:
		break;
	case STM_FE_DEMOD_FEC_RATE_1_2:
		lla_fecrate = FE_SAT_PR_1_2;
		break;
	case STM_FE_DEMOD_FEC_RATE_2_3:
		lla_fecrate = FE_SAT_PR_2_3;
		break;
	case STM_FE_DEMOD_FEC_RATE_3_4:
		lla_fecrate = FE_SAT_PR_3_4;
		break;
	case STM_FE_DEMOD_FEC_RATE_4_5:
		lla_fecrate = FE_SAT_PR_4_5;
		break;
	case STM_FE_DEMOD_FEC_RATE_5_6:
		lla_fecrate = FE_SAT_PR_5_6;
		break;
	case STM_FE_DEMOD_FEC_RATE_6_7:
		lla_fecrate = FE_SAT_PR_6_7;
		break;
	case STM_FE_DEMOD_FEC_RATE_7_8:
		lla_fecrate = FE_SAT_PR_7_8;
		break;
	case STM_FE_DEMOD_FEC_RATE_8_9:
		lla_fecrate = FE_SAT_PR_8_9;
		break;
	}
	return lla_fecrate;
}
EXPORT_SYMBOL(stmfe_set_sat_fec_rate);

FE_Sat_Search_IQ_Inv_t
stmfe_set_sat_iq_inv(stm_fe_demod_spectral_inversion_t inv)
{
	switch (inv) {
	case STM_FE_DEMOD_SPECTRAL_INVERSION_OFF:
		return FE_SAT_IQ_FORCE_NORMAL;
	case STM_FE_DEMOD_SPECTRAL_INVERSION_ON:
		return FE_SAT_IQ_FORCE_SWAPPED;
	case STM_FE_DEMOD_SPECTRAL_INVERSION_AUTO:
	default:
		return FE_SAT_IQ_AUTO;
	}
	return FE_SAT_IQ_AUTO;
}
EXPORT_SYMBOL(stmfe_set_sat_iq_inv);

stm_fe_demod_tx_std_t stmfe_get_sat_standard(FE_Sat_TrackingStandard_t std)
{
	uint32_t convert_table[] = {
		STM_FE_DEMOD_TX_STD_DVBS,	/* FE_SAT_DVBS1_STANDARD */
		STM_FE_DEMOD_TX_STD_DVBS2,	/* FE_SAT_DVBS2_STANDARD */
		0,		/* FE_SAT_DSS_STANDARD */
		0,		/* FE_SAT_TURBOCODE_STANDARD */
		0		/* FE_SAT_UNKNOWN_STANDARD */
	};
	return convert_table[std];
}
EXPORT_SYMBOL(stmfe_get_sat_standard);

stm_fe_demod_fec_rate_t stmfe_get_sat_fec_rate(FE_Sat_TrackingStandard_t std,
			     FE_Sat_Rate_t puncture_rate,
			     FE_Sat_ModCod_t modCode)
{
#define SIZE_TABLE 8

	int sat_modcod_lookup[30] = {
	    /*STFRONTEND_MODECODE_DUMMY_PLF, STFRONTEND_MOD_NONE, */
	    STM_FE_DEMOD_FEC_RATE_NONE,
	    /*STFRONTEND_MODECODE_QPSK_14, STM_FE_DEMOD_MODULATION_QPSK, */
	    STM_FE_DEMOD_FEC_RATE_1_4,
	    /*STFRONTEND_MODECODE_QPSK_13, STM_FE_DEMOD_MODULATION_QPSK, */
	    STM_FE_DEMOD_FEC_RATE_1_3,
	    /*STFRONTEND_MODECODE_QPSK_25, STM_FE_DEMOD_MODULATION_QPSK, */
	    STM_FE_DEMOD_FEC_RATE_2_5,
	    /*STFRONTEND_MODECODE_QPSK_12, STM_FE_DEMOD_MODULATION_QPSK, */
	    STM_FE_DEMOD_FEC_RATE_1_2,
	    /*STFRONTEND_MODECODE_QPSK_35, STM_FE_DEMOD_MODULATION_QPSK, */
	    STM_FE_DEMOD_FEC_RATE_3_5,
	    /*STFRONTEND_MODECODE_QPSK_23, STM_FE_DEMOD_MODULATION_QPSK, */
	    STM_FE_DEMOD_FEC_RATE_2_3,
	    /*STFRONTEND_MODECODE_QPSK_34, STM_FE_DEMOD_MODULATION_QPSK, */
	    STM_FE_DEMOD_FEC_RATE_3_4,
	    /*STFRONTEND_MODECODE_QPSK_45, STM_FE_DEMOD_MODULATION_QPSK, */
	    STM_FE_DEMOD_FEC_RATE_4_5,
	    /*STFRONTEND_MODECODE_QPSK_56, STM_FE_DEMOD_MODULATION_QPSK, */
	    STM_FE_DEMOD_FEC_RATE_5_6,
	    /*STFRONTEND_MODECODE_QPSK_89, STM_FE_DEMOD_MODULATION_QPSK, */
	    STM_FE_DEMOD_FEC_RATE_8_9,
	    /*STFRONTEND_MODECODE_QPSK_910,STM_FE_DEMOD_MODULATION_QPSK, */
	    STM_FE_DEMOD_FEC_RATE_9_10,
	    /*STFRONTEND_MODECODE_8PSK_35, STM_FE_DEMOD_MODULATION_8PSK, */
	    STM_FE_DEMOD_FEC_RATE_3_5,
	    /*STFRONTEND_MODECODE_8PSK_23, STM_FE_DEMOD_MODULATION_8PSK, */
	    STM_FE_DEMOD_FEC_RATE_2_3,
	    /*STFRONTEND_MODECODE_8PSK_34, STM_FE_DEMOD_MODULATION_8PSK, */
	    STM_FE_DEMOD_FEC_RATE_3_4,
	    /*STFRONTEND_MODECODE_8PSK_56, STM_FE_DEMOD_MODULATION_8PSK, */
	    STM_FE_DEMOD_FEC_RATE_5_6,
	    /*STFRONTEND_MODECODE_8PSK_89, STM_FE_DEMOD_MODULATION_8PSK, */
	    STM_FE_DEMOD_FEC_RATE_8_9,
	    /*STFRONTEND_MODECODE_8PSK_910, STM_FE_DEMOD_MODULATION_8PSK, */
	    STM_FE_DEMOD_FEC_RATE_9_10,
	    /*STFRONTEND_MODECODE_16APSK_23, STM_FE_DEMOD_MODULATION_16APSK, */
	    STM_FE_DEMOD_FEC_RATE_2_3,
	    /*STFRONTEND_MODECODE_16APSK_34, STM_FE_DEMOD_MODULATION_16APSK, */
	    STM_FE_DEMOD_FEC_RATE_3_4,
	    /*STFRONTEND_MODECODE_16APSK_45, STM_FE_DEMOD_MODULATION_16APSK, */
	    STM_FE_DEMOD_FEC_RATE_4_5,
	    /*STFRONTEND_MODECODE_16APSK_56, STM_FE_DEMOD_MODULATION_16APSK, */
	    STM_FE_DEMOD_FEC_RATE_5_6,
	    /*STFRONTEND_MODECODE_16APSK_89, STM_FE_DEMOD_MODULATION_16APSK, */
	    STM_FE_DEMOD_FEC_RATE_8_9,
	    /*STFRONTEND_MODECODE_16APSK_910,STM_FE_DEMOD_MODULATION_16APSK, */
	    STM_FE_DEMOD_FEC_RATE_9_10,
	    /*STFRONTEND_MODECODE_32APSK_34, STM_FE_DEMOD_MODULATION_32APSK, */
	    STM_FE_DEMOD_FEC_RATE_3_4,
	    /*STFRONTEND_MODECODE_32APSK_45, STM_FE_DEMOD_MODULATION_32APSK, */
	    STM_FE_DEMOD_FEC_RATE_4_5,
	    /*STFRONTEND_MODECODE_32APSK_56, STM_FE_DEMOD_MODULATION_32APSK, */
	    STM_FE_DEMOD_FEC_RATE_5_6,
	    /*STFRONTEND_MODECODE_32APSK_89, STM_FE_DEMOD_MODULATION_32APSK, */
	    STM_FE_DEMOD_FEC_RATE_8_9,
	    /*STFRONTEND_MODECODE_32APSK_910,STM_FE_DEMOD_MODULATION_32APSK,*/
	    STM_FE_DEMOD_FEC_RATE_9_10,
	    /* STFRONTEND_MODECODE_UNKNOWN, STFRONTEND_MOD_NONE, */
	    STM_FE_DEMOD_FEC_RATE_NONE
	};

	uint32_t convert_table[SIZE_TABLE] = {
		/*Only these FEC Rates are supported by sat */
		STM_FE_DEMOD_FEC_RATE_1_2,
		STM_FE_DEMOD_FEC_RATE_2_3,
		STM_FE_DEMOD_FEC_RATE_3_4,
		STM_FE_DEMOD_FEC_RATE_4_5,
		STM_FE_DEMOD_FEC_RATE_5_6,
		STM_FE_DEMOD_FEC_RATE_6_7,
		STM_FE_DEMOD_FEC_RATE_7_8,
		STM_FE_DEMOD_FEC_RATE_8_9
	};

	switch (std) {
	case FE_SAT_DVBS2_STANDARD:
		return sat_modcod_lookup[modCode];

	case FE_SAT_DVBS1_STANDARD:
	case FE_SAT_DSS_STANDARD:
	default:
		if (puncture_rate > SIZE_TABLE - 1)
			return STM_FE_DEMOD_FEC_RATE_NONE;
		else
			return convert_table[puncture_rate];

	}
	return STM_FE_DEMOD_FEC_RATE_NONE;
}
EXPORT_SYMBOL(stmfe_get_sat_fec_rate);

stm_fe_demod_modulation_t stmfe_get_sat_modulation(stm_fe_demod_tx_std_t std,
			       FE_Sat_ModCod_t modCode)
{
	int sat_modcod_lookup[30][2] = {
		{		/*STFRONTEND_MODECODE_DUMMY_PLF, */
		 STM_FE_DEMOD_MODULATION_NONE, STM_FE_DEMOD_FEC_RATE_NONE},
		{		/*STFRONTEND_MODECODE_QPSK_14, */
		 STM_FE_DEMOD_MODULATION_QPSK, STM_FE_DEMOD_FEC_RATE_1_4},
		{		/*STFRONTEND_MODECODE_QPSK_13, */
		 STM_FE_DEMOD_MODULATION_QPSK, STM_FE_DEMOD_FEC_RATE_1_3},
		{		/*STFRONTEND_MODECODE_QPSK_25, */
		 STM_FE_DEMOD_MODULATION_QPSK, STM_FE_DEMOD_FEC_RATE_2_5},
		{		/*STFRONTEND_MODECODE_QPSK_12, */
		 STM_FE_DEMOD_MODULATION_QPSK, STM_FE_DEMOD_FEC_RATE_1_2},
		{		/*STFRONTEND_MODECODE_QPSK_35, */
		 STM_FE_DEMOD_MODULATION_QPSK, STM_FE_DEMOD_FEC_RATE_3_5},
		{		/*STFRONTEND_MODECODE_QPSK_23, */
		 STM_FE_DEMOD_MODULATION_QPSK, STM_FE_DEMOD_FEC_RATE_2_3},
		{		/*STFRONTEND_MODECODE_QPSK_34, */
		 STM_FE_DEMOD_MODULATION_QPSK, STM_FE_DEMOD_FEC_RATE_3_4},
		{		/*STFRONTEND_MODECODE_QPSK_45, */
		 STM_FE_DEMOD_MODULATION_QPSK, STM_FE_DEMOD_FEC_RATE_4_5},
		{		/*STFRONTEND_MODECODE_QPSK_56, */
		 STM_FE_DEMOD_MODULATION_QPSK, STM_FE_DEMOD_FEC_RATE_5_6},
		{		/*STFRONTEND_MODECODE_QPSK_89, */
		 STM_FE_DEMOD_MODULATION_QPSK, STM_FE_DEMOD_FEC_RATE_8_9},
		{		/*STFRONTEND_MODECODE_QPSK_910, */
		 STM_FE_DEMOD_MODULATION_QPSK, STM_FE_DEMOD_FEC_RATE_9_10},
		{		/*STFRONTEND_MODECODE_8PSK_35, */
		 STM_FE_DEMOD_MODULATION_8PSK, STM_FE_DEMOD_FEC_RATE_3_5},
		{		/*STFRONTEND_MODECODE_8PSK_23, */
		 STM_FE_DEMOD_MODULATION_8PSK, STM_FE_DEMOD_FEC_RATE_2_3},
		{		/*STFRONTEND_MODECODE_8PSK_34, */
		 STM_FE_DEMOD_MODULATION_8PSK, STM_FE_DEMOD_FEC_RATE_3_4},
		{		/*STFRONTEND_MODECODE_8PSK_56, */
		 STM_FE_DEMOD_MODULATION_8PSK, STM_FE_DEMOD_FEC_RATE_5_6},
		{		/*STFRONTEND_MODECODE_8PSK_89, */
		 STM_FE_DEMOD_MODULATION_8PSK, STM_FE_DEMOD_FEC_RATE_8_9},
		{		/*STFRONTEND_MODECODE_8PSK_910, */
		 STM_FE_DEMOD_MODULATION_8PSK, STM_FE_DEMOD_FEC_RATE_9_10},
		{		/*STFRONTEND_MODECODE_16APSK_23, */
		 STM_FE_DEMOD_MODULATION_16APSK, STM_FE_DEMOD_FEC_RATE_2_3},
		{		/*STFRONTEND_MODECODE_16APSK_34, */
		 STM_FE_DEMOD_MODULATION_16APSK, STM_FE_DEMOD_FEC_RATE_3_4},
		{		/*STFRONTEND_MODECODE_16APSK_45, */
		 STM_FE_DEMOD_MODULATION_16APSK, STM_FE_DEMOD_FEC_RATE_4_5},
		{		/*STFRONTEND_MODECODE_16APSK_56, */
		 STM_FE_DEMOD_MODULATION_16APSK, STM_FE_DEMOD_FEC_RATE_5_6},
		{		/*STFRONTEND_MODECODE_16APSK_89, */
		 STM_FE_DEMOD_MODULATION_16APSK, STM_FE_DEMOD_FEC_RATE_8_9},
		{		/*STFRONTEND_MODECODE_16APSK_910, */
		 STM_FE_DEMOD_MODULATION_16APSK, STM_FE_DEMOD_FEC_RATE_9_10},
		{		/*STFRONTEND_MODECODE_32APSK_34, */
		 STM_FE_DEMOD_MODULATION_32APSK, STM_FE_DEMOD_FEC_RATE_3_4},
		{		/*STFRONTEND_MODECODE_32APSK_45, */
		 STM_FE_DEMOD_MODULATION_32APSK, STM_FE_DEMOD_FEC_RATE_4_5},
		{		/*STFRONTEND_MODECODE_32APSK_56, */
		 STM_FE_DEMOD_MODULATION_32APSK, STM_FE_DEMOD_FEC_RATE_5_6},
		{		/*STFRONTEND_MODECODE_32APSK_89, */
		 STM_FE_DEMOD_MODULATION_32APSK, STM_FE_DEMOD_FEC_RATE_8_9},
		{		/*STFRONTEND_MODECODE_32APSK_910, */
		 STM_FE_DEMOD_MODULATION_32APSK, STM_FE_DEMOD_FEC_RATE_9_10},
		{		/*  STFRONTEND_MODECODE_UNKNOWN, */
		 STM_FE_DEMOD_MODULATION_NONE, STM_FE_DEMOD_FEC_RATE_NONE}
	};

	switch (std) {
	case STM_FE_DEMOD_TX_STD_DVBS2:
		return sat_modcod_lookup[modCode][0];

	case STM_FE_DEMOD_TX_STD_DVBS:
	default:
		return STM_FE_DEMOD_MODULATION_QPSK;
	}
}
EXPORT_SYMBOL(stmfe_get_sat_modulation);

stm_fe_demod_roll_off_t stmfe_get_sat_roll_off(FE_Sat_RollOff_t roll_off)
{
	stm_fe_demod_roll_off_t ret = -1;
	switch (roll_off) {
	case FE_SAT_35:
		ret = STM_FE_DEMOD_ROLL_OFF_0_35;
		break;
	case FE_SAT_25:
		ret = STM_FE_DEMOD_ROLL_OFF_0_25;
		break;
	case FE_SAT_20:
		ret = STM_FE_DEMOD_ROLL_OFF_0_20;
		break;
	default:
		break;
	}
	return ret;
}
EXPORT_SYMBOL(stmfe_get_sat_roll_off);
