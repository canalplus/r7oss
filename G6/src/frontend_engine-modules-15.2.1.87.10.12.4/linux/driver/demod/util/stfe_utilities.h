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

Source file name : stfe_utilities.h
Author :           LLA

LLA generic file

Date        Modification                                    Name
----        ------------                                    --------
20-Jun-11   Created                                         Shobhit

************************************************************************/
#ifndef _STFE_UTILITIES_H
#define _STFE_UTILITIES_H

/*#include <stddefs.h>*/
#if defined(CHIP_STAPI)
/*#include <stfrontend.h>*/
/*#include <chip.h>*/
#endif
typedef uint64_t U64;
typedef uint32_t U32;
typedef uint16_t U16;
typedef uint8_t U8;

typedef int32_t S32;
typedef int16_t S16;
typedef int8_t S8;
#define BOOL bool
#define FALSE false
#define TRUE  true
typedef char *ST_String_t;
typedef void *InternalParamsPtr;	/*Handle to the FE */
typedef char *ST_Revision_t;

/*
 * Common Macro used for dvbs/dvbs2 (stm_fe_demod_dvbs_channel_info_s
 * and stm_fe_demod_dvbs2_channel_info_s) to exploit the characteristic
 * of "union"
 */
#define RESET_INFO_SAT_S1_S2(p) {\
	p->status = STM_FE_DEMOD_STATUS_UNKNOWN; \
	p->freq = 0; \
	p->sr = 0; \
	p->fec = STM_FE_DEMOD_FEC_RATE_NONE; \
	p->inv = STM_FE_DEMOD_SPECTRAL_INVERSION_OFF; \
	p->mod = STM_FE_DEMOD_MODULATION_NONE; \
	p->signal_strength = -100; \
	p->snr = 0; \
	p->per = 0; \
	p->roll_off = 0; \
	p->frame_len = 0; \
	p->pilots = STM_FE_DEMOD_PILOT_STATUS_OFF; \
}

#define STM_FE_DEMOD_TX_STD_S1_S2 (STM_FE_DEMOD_TX_STD_DVBS | \
		STM_FE_DEMOD_TX_STD_DVBS2)

#define FILL_SAT_SEARCH_PARAMS_S1_S2(priv, std_in, s_params, params) {\
	s_params.SearchRange = 10000000; \
	s_params.SearchAlgo = FE_SAT_COLD_START; \
	s_params.Path = THIS_PATH(priv); \
	s_params.Modulation = FE_SAT_MOD_UNKNOWN; \
	s_params.Modcode = FE_SAT_MODCODE_UNKNOWN; \
\
	if (std_in == STM_FE_DEMOD_TX_STD_DVBS) \
		s_params.Standard = FE_SAT_SEARCH_DVBS1; \
	else if (std_in == STM_FE_DEMOD_TX_STD_DVBS2) \
		s_params.Standard = FE_SAT_SEARCH_DVBS2; \
\
	if (std_in & STM_FE_DEMOD_TX_STD_S1_S2) { \
		s_params.Frequency = params->freq; \
		s_params.SymbolRate = params->sr; \
		s_params.PunctureRate = \
				stmfe_set_sat_fec_rate(params->fec); \
		s_params.IQ_Inversion = FE_SAT_IQ_AUTO; \
	} \
\
	if (std_in == STM_FE_DEMOD_TX_STD_S1_S2) { \
		s_params.Standard = FE_SAT_AUTO_SEARCH; \
		s_params.IQ_Inversion = FE_SAT_IQ_AUTO; \
	} \
}

#define FILL_CAB_SEARCH_PARAMS_DVBC(cab_search_params, dvbc) {\
	cab_search_params.Frequency_kHz = dvbc->freq; \
	cab_search_params.SearchRange_Hz = 280000; /*280 MHz */ \
	cab_search_params.SymbolRate_Bds = dvbc->sr; \
	cab_search_params.Modulation = modulationmap(dvbc->mod); \
\
	if (dvbc->inv == STM_FE_DEMOD_SPECTRAL_INVERSION_OFF) \
		cab_search_params.SpectrumInversion = FE_CAB_SPECT_NORMAL; \
	else if (dvbc->inv == STM_FE_DEMOD_SPECTRAL_INVERSION_ON) \
		cab_search_params.SpectrumInversion = FE_CAB_SPECT_INVERTED; \
}

#define FILL_DVBC_INFO(dvbc_info, cab_results_params) {\
	dvbc_info->freq = cab_results_params.Frequency_kHz; \
	dvbc_info->sr = cab_results_params.SymbolRate_Bds; \
	if (cab_results_params.SpectInv == FE_CAB_SPECT_INVERTED) \
		dvbc_info->inv = STM_FE_DEMOD_SPECTRAL_INVERSION_ON; \
	else if (cab_results_params.SpectInv == FE_CAB_SPECT_NORMAL) \
		dvbc_info->inv = STM_FE_DEMOD_SPECTRAL_INVERSION_OFF; \
\
	dvbc_info->mod = get_mod(cab_results_params.Modulation); \
}

#define FILL_TER_SEARCH_PARAMS_DVBT(ter_search_params, dvbt, priv) {\
	ter_search_params.Frequency_kHz = dvbt->freq; \
	ter_search_params.Optimize.Guard = guardmap(dvbt->guard); \
	ter_search_params.Optimize.FFTSize = fftmodemap(dvbt->fft_mode); \
	if (dvbt->inv == STM_FE_DEMOD_SPECTRAL_INVERSION_OFF) \
		ter_search_params.Optimize.SpectInversion = \
						FE_TER_INVERSION_NONE; \
	else if (dvbt->inv == STM_FE_DEMOD_SPECTRAL_INVERSION_ON) \
		ter_search_params.Optimize.SpectInversion = FE_TER_INVERSION; \
	else if (dvbt->inv == STM_FE_DEMOD_SPECTRAL_INVERSION_AUTO) \
		ter_search_params.Optimize.SpectInversion = \
						FE_TER_INVERSION_Unknown; \
	ter_search_params.ChannelBW_MHz = channelbwmap(dvbt->bw); \
	ter_search_params.Hierarchy = hierarchymap(dvbt->hierarchy); \
	if (priv->config->custom_flags == DEMOD_CUSTOM_IQ_WIRING_INVERTED) \
		ter_search_params.IF_IQ_Mode = FE_TER_IQ_TUNER; \
	else \
		ter_search_params.IF_IQ_Mode = FE_TER_NORMAL_IF_TUNER; \
}

#define FILL_DVBT_INFO(dvbt_info, ter_search_params, ter_results_params) { \
	dvbt_info->freq = ter_results_params.Frequency_kHz; \
	dvbt_info->bw = get_bw(ter_search_params.ChannelBW_MHz); \
	dvbt_info->fec = get_fec(ter_results_params.FECRate); \
	dvbt_info->hierarchy = get_hierarchy(ter_results_params.Hierarchy); \
	dvbt_info->alpha = get_alpha(ter_results_params.Hierarchy_Alpha); \
	dvbt_info->guard = get_guard(ter_results_params.Guard); \
	dvbt_info->fft_mode = get_fftmode(ter_results_params.FFTSize); \
	dvbt_info->inv = (ter_results_params.SpectInversion == 0) \
				? STM_FE_DEMOD_SPECTRAL_INVERSION_OFF : \
				STM_FE_DEMOD_SPECTRAL_INVERSION_ON; \
	dvbt_info->mod = get_ter_mod(ter_results_params.Modulation); \
}

#define FE_LLA_MAXLOOKUPSIZE 500
/* One point of the lookup table */
typedef struct {
	int32_t realval;	/* real value */
	int32_t regval;		/* binary value */
} FE_LLA_LOOKPOINT_t;

/* Lookup table definition */
typedef struct {
	int32_t size;		/* Size of the lookup table */
	FE_LLA_LOOKPOINT_t table[FE_LLA_MAXLOOKUPSIZE];	/* Lookup table */
} FE_LLA_LOOKUP_t;

typedef enum {
	FE_LLA_NO_ERROR,
	FE_LLA_INVALID_HANDLE,
	FE_LLA_ALLOCATION,
	FE_LLA_BAD_PARAMETER,
	FE_LLA_ALREADY_INITIALIZED,
	FE_LLA_I2C_ERROR,
	FE_LLA_SEARCH_FAILED,
	FE_LLA_TRACKING_FAILED,
	FE_LLA_NODATA,
	FE_LLA_TUNER_NOSIGNAL,
	FE_LLA_TUNER_JUMP,
	FE_LLA_TUNER_4_STEP,
	FE_LLA_TUNER_8_STEP,
	FE_LLA_TUNER_16_STEP,
	FE_LLA_TERM_FAILED,
	FE_LLA_DISEQC_FAILED
} FE_LLA_Error_t;		/*Error Type */

typedef enum {
	TUNER_IQ_NORMAL = 1,
	TUNER_IQ_INVERT = -1
} TUNER_IQ_t;

typedef enum {
	/* tuner address set by MT2068_Open() */
	Tuner_IC_ADDR,

	/* max number of MT2068 tuners set by MT_TUNER_CNT in mt_userdef.h */
	Tuner_MAX_OPEN,

	/* current number of open MT2068 tuners set by MT2068_Open() */
	Tuner_NUM_OPEN,

	/* crystal frequency (default: 16000000 Hz) */
	Tuner_SRO_FREQ,

	/* min tuning step size (default: 50000 Hz) */
	Tuner_STEPSIZE,

	/* input center frequency set by MT2068_Tune() */
	Tuner_INPUT_FREQ,

	/* LO1 Frequency set by MT2068_Tune() */
	Tuner_LO1_FREQ,

	/* LO1 minimum step size (default: 250000 Hz) */
	Tuner_LO1_STEPSIZE,

	/* LO1 FracN keep-out region (default: 999999 Hz) */
	Tuner_LO1_FRACN_AVOID,

	/*  Current 1st IF in use set by MT2068_Tune() */
	Tuner_IF1_ACTUAL,

	/* Requested 1st IF set by MT2068_Tune() */
	Tuner_IF1_REQUEST,

	/* Center of 1st IF SAW filter (default: tuner-specific) */
	Tuner_IF1_CENTER,

	/* Bandwidth of 1st IF SAW filter (default: 20000000 Hz) */
	Tuner_IF1_BW,

	/* zero-IF bandwidth (default: 2000000 Hz) */
	Tuner_ZIF_BW,

	/* LO2 Frequency set by MT2068_Tune() */
	Tuner_LO2_FREQ,

	/* LO2 minimum step size (default: 50000 Hz) */
	Tuner_LO2_STEPSIZE,

	/* LO2 FracN keep-out region (default: 374999 Hz) */
	Tuner_LO2_FRACN_AVOID,

	/* output center frequency set by MT2068_Tune() */
	Tuner_OUTPUT_FREQ,

	/* output bandwidth set by MT2068_Tune() */
	Tuner_OUTPUT_BW,

	/* min inter-tuner LO separation (default: 1000000 Hz) */
	Tuner_LO_SEPARATION,

	/* ID of avoid-spurs algorithm in use compile-time constant */
	Tuner_AS_ALG,

	/* max # of intra-tuner harmonics (default: 15) */
	Tuner_MAX_HARM1,

	/* max # of inter-tuner harmonics (default: 7) */
	Tuner_MAX_HARM2,

	/* # of 1st IF exclusion zones used set by MT2068_Tune() */
	Tuner_EXCL_ZONES,

	/* # of spurs found/avoided set by MT2068_Tune() */
	Tuner_NUM_SPURS,

	/* >0 spurs avoided set by MT2068_Tune() */
	Tuner_SPUR_AVOIDED,

	/* >0 spurs in output (mathematically) set by Tuner_Tune() */
	Tuner_SPUR_PRESENT,

	/* GCU Mode: 1 - Automatic, 0- Manual */
	Tuner_GCU_AUTO,

	/* Receiver Mode for some parameters. 1 is DVB-T */
	Tuner_RCVR_MODE,

	/* Set target power level at PD1. Paremeter is value to set. */
	Tuner_PD1_TGT,

	/* Get RF attenuator code */
	Tuner_ACRF,

	/* Set target power level at PD2. Paremeter is value to set. */
	Tuner_PD2_TGT,

	/* Get FIF attenuator code */
	Tuner_ACFIF,

	/* CapTrim code */
	Tuner_CTRIM,

	/* ClearTune filter number in use set by MT2068_Tune() */
	Tuner_CTFILT,

	/* Flag for 1=automatic/0=manual RF AGC */
	Tuner_AUTOAGC,

	/* Set the limit values for FIF Atten Code and RF Atten Code */
	Tuner_FORCEAGC,

	/* Flag to 1=freeze/0=run RF AGC */
	Tuner_FREEZEAGC,

	/* Control setting to avoid DECT freqs (default: MT_AVOID_US_DECT) */
	Tuner_AVOID_DECT,

	/* Set the VGA Gain Control value (default: 0 40dB ) */
	Tuner_VGAGC,

	/* last entry in enumerated list */
	Tuner_EOP,

	/* Dummy Entry */
	Tuner_STANDBY_WITH_VCO,

	Tuner_AGC_LOOP_BW,	/*for Mxl201 */

	Tuner_LOCK_STATUS	/*for Mxl201 */
} Tuner_Param_t;

typedef enum {
	TUNER_LOCKED,
	TUNER_NOTLOCKED
} TUNER_Lock_t;

typedef enum {
	TUNER_INPUT1 = 0,	/*input 1 (A,B for 6120) */
	TUNER_INPUT2 = 1,	/*input 2 (C,D for 6120) */
} TUNER_DualInput_t;		/*Dual Tuner (6120 like) input selection */

typedef enum {
	FE_RF_SOURCE_A = 0,
	FE_RF_SOURCE_B = 1,
	FE_RF_SOURCE_C = 2,	/* No applicable for 6140 */
	FE_RF_SOURCE_D = 3	/* No applicable for 6140 */
} TUNER_RFSource_t;

typedef enum {
	LBRFA_HBRFB = 0,	/*Low band on RFA, High band on RFB */
	LBRFB_HBRFA = 1,	/*Low band on RFB, High band on RFA */
	LBRFC_HBRFD = 2,	/*Low band on RFC, High band on RFD */
	LBRFD_HBRFC = 3		/*Low band on RFD, High band on RFC */
} TUNER_WIDEBandS_t;		/* for wide band tuner 6130 and 6120 */
typedef enum {			/* Path selection for dual tuner */
	TUNER_PATH1 = 0,	/* Path1 i.e tuner1 */
	TUNER_PATH2 = 1,	/* Path1 i.e tuner2 */
} TUNER_Dual_t;			/*Dual Tuner (6120 like) path selection */

typedef enum {
	TUNER_NO_ERR,
	TUNER_INVALID_HANDLE,
	TUNER_INVALID_REG_ID,	/* Using of an invalid register */
	TUNER_INVALID_FIELD_ID,	/* Using of an Invalid field */
	TUNER_INVALID_FIELD_SIZE, /* Using of a field with an invalid size */
	TUNER_I2C_NO_ACK,	/* No acknowledge from the chip */
	TUNER_I2C_BURST,	/* Two many registers accessed in burst mode */
	TUNER_TYPE_ERR
} TUNER_Error_t;

typedef enum {
	FE_BER_Algo_Default,
	FE_BER_Algo_Task_Berwindow,
	FE_BER_Algo_Appli_Berwindow
} FE_Sat_BER_Algo_t;

/****************************************************************
ST FE Demodulators List
****************************************************************/

typedef enum {
	FE_DVB_T = 0,
	FE_DVB_T2,
	FE_DVB_C,
	FE_DVB_C2,
	FE_DVB_S,
	FE_DVB_S2,
	FE_ISDB_T,
	FE_ATSC,
	FE_J83B,
	FE_TransmissionStandard_Unknown = 0xFF
} FE_TransmissionStandard_t;

typedef enum {
	DEMOD_STV0297E,
	DEMOD_STV0297D,
	DEMOD_STV0297J,
	DEMOD_STV0900,
	DEMOD_STV0903,
	DEMOD_STV0910,
	DEMOD_STI7111,
	DEMOD_STV0288,
	DEMOD_STV0289,
	DEMOD_STBSTC1,
	DEMOD_STID333,
	DEMOD_STB0899,
	DEMOD_STBADV1,
	DEMOD_STV0299,
	DEMOD_STV0399,
	DEMOD_STV0367DVBT,
	DEMOD_STV0367DVBC,
	DEMOD_STV0362,
	DEMOD_STV0360,
	DEMOD_STV0361,
	DEMOD_STV0372,
	DEMOD_STV0374ATSC,
	DEMOD_STV0374QAM,
	DEMOD_STV0373,
	DEMOD_STV0371ATSC,
	DEMOD_STV0371J83B,
	DEMOD_STV0370ATSC,
	DEMOD_STV0370J83B,
	DEMOD_STI7141,
	DEMOD_STI5197,
	DEMOD_STV5189,
	DEMOD_STV5289,
	DEMOD_CUSTOM_DEMOD_01,
	DEMOD_STV0913
} FE_DEMOD_Model_t;

typedef enum {
	TUNER_NULL,
	TUNER_DTT73002,
	TUNER_TDQD3,
	TUNER_MT2266,
	TUNER_FQD1216ME,
	TUNER_DTT7546X,
	TUNER_STV4100,
	TUNER_EDT3022,
	TUNER_DTT73200A,
	TUNER_TDAE,
	TUNER_DNOS40,
	TUNER_DTT75300,
	TUNER_EN4020TER,
	TUNER_HC401Z,
	TUNER_DTOS403,
	TUNER_FT3114,
	TUNER_DNOS40AS,
	TUNER_DCT7045,
	TUNER_TDQE3,
	TUNER_DCF8783,
	TUNER_MT2060,
	TUNER_AIT1051,
	TUNER_DCT70700,
	TUNER_TDCHG,
	TUNER_TDCJG,
	TUNER_TDCGG,
	TUNER_TDCCG,
	TUNER_CD1616LF,
	TUNER_RF4800,
	TUNER_ET55DHR,
	TUNER_DCF87900,
	TUNER_DTT7546,
	TUNER_MT2062,
	TUNER_MT2011,
	TUNER_AIT1042,
	TUNER_EN4020,
	TUNER_MT2066,
	TUNER_MXL201RF,
	TUNER_MT2068,
	TUNER_DTT768XX,
	TUNER_DTT7600,
	TUNER_MXL203RF,
	TUNER_FQD1116,
	TUNER_TDA9898,
	TUNER_ENG37E03KF,
	TUNER_DCT7040,
	TUNER_CD1100,
	TUNER_STB6000,
	TUNER_STB6100,		/* RF magic STB6100 tuner */
	TUNER_STV6110,		/* 6110 tuner */
	TUNER_STV6130,		/* 6130 tuner */
	TUNER_STV6120,		/* 6120 tuner */
	TUNER_STV6120_Tuner1,	/* 6120 tuner */
	TUNER_STV6120_Tuner2,	/* 6120 tuner */
	TUNER_STV6140_Tuner1,	/* 6140 tuner */
	TUNER_STV6140_Tuner2,	/* 6140 tuner */
	TUNER_ASCOT2S,
	TUNER_ASCOT2D,
	TUNER_TDA18250,
	TUNER_MT2082,
	TUNER_TDAE3,
	TUNER_TDA18212,
	TUNER_TDA18250A,
	TUNER_MXL603,
	TUNER_TDA18219,
	TUNER_FJ222,
	TUNER_STV6111
} TUNER_Model_t;

typedef enum {
	Band_VHF3 = 2,
	Band_UHF = 8
} TUNER_Band_t;

/****************************************************************
TS CONFIG STRUCTURE
****************************************************************/
/*TS configuration enum */

typedef enum {
	FE_TS_OUTPUTMODE_DEFAULT,	/*Use default register setting */
	FE_TS_SERIAL_PUNCT_CLOCK, /*Serial punctured clock : serial STBackend */
	FE_TS_SERIAL_CONT_CLOCK, /*Serial continues clock */
	/*Parallel punctured clock : Parallel STBackend */
	FE_TS_PARALLEL_PUNCT_CLOCK,
	FE_TS_DVBCI_CLOCK	/*Parallel continues clock : DVBCI */
} FE_TS_OutputMode_t;

typedef enum {
	FE_TS_DATARATECONTROL_DEFAULT,
	FE_TS_MANUAL_SPEED,	/* TS Speed manual */
	FE_TS_AUTO_SPEED	/* TS Speed automatic */
} FE_TS_DataRateControl_t;

typedef enum {
	FE_TS_CLOCKPOLARITY_DEFAULT,
	FE_TS_RISINGEDGE_CLOCK,	/* TS clock rising edge */
	FE_TS_FALLINGEDGE_CLOCK	/* TS clock falling edge */
} FE_TS_ClockPolarity_t;

typedef enum {
	FE_TS_SYNCBYTE_DEFAULT,
	FE_TS_SYNCBYTE_ON,	/* TS synchro byte ON */
	FE_TS_SYNCBYTE_OFF	/* delete TS synchro byte */
} FE_TS_SyncByteEnable_t;

typedef enum {
	FE_TS_PARTITYBYTES_DEFAULT,
	FE_TS_PARITYBYTES_ON,	/* insert TS parity bytes */
	FE_TS_PARITYBYTES_OFF	/* delete TS parity bytes */
} FE_TS_ParityBytes_t;

typedef enum {
	FE_TS_SWAP_DEFAULT,
	FE_TS_SWAP_ON,		/* swap TS bits LSB <-> MSB */
	FE_TS_SWAP_OFF		/* don't swap TS bits LSB <-> MSB */
} FE_TS_Swap_t;

typedef enum {
	FE_TS_SMOOTHER_DEFAULT,
	FE_TS_SMOOTHER_ON,	/* enable TS smoother or fifo */
	FE_TS_SMOOTHER_OFF	/* disable TS smoother or fifo */
} FE_TS_Smoother_t;

typedef enum {
	FE_TS_OUTPUT1,		/*Demod selects TS1(demod's TS) */
	FE_TS_OUTPUT2,		/*Demod selects TS2(demod's TS) */
	FE_TS_OUTPUT3,		/*Demod selects TS3(demod's TS) */
	FE_TS_MUX		/*Demod selects MUX TS (demod's TS) */
} FE_TS_SelectOutput_t;

typedef struct {
	/* TS Serial pucntured, serial continues, parallel punctured or DVBSI */
	FE_TS_OutputMode_t TSMode;
	/* TS speed control : manual or automatic */
	FE_TS_DataRateControl_t TSSpeedControl;
	/* TS clock polarity : rising edge/falling edge */
	FE_TS_ClockPolarity_t TSClockPolarity;
	/* TS clock rate in Hz if manual mode */
	u_int32_t TSClockRate;
	u_int32_t TSDataRate;	/* TS Data rate in Hz if manual mode */
	/* TS sync byte : enable/disabe */
	FE_TS_SyncByteEnable_t TSSyncByteEnable;
	/* TS parity bytes Enable/Disable */
	FE_TS_ParityBytes_t TSParityBytes;
	FE_TS_Swap_t TSSwap;	/* TS bits swap/ unswap */
	FE_TS_Smoother_t TSSmoother;	/* TS smoother enable/disable */
	FE_TS_SelectOutput_t TSOutput;
} FE_TS_Config_t;

/* was: S32 PowOf2(S32 number); */
#define PowOf2(number) (1<<(number))
/* was: S32 PowOf2(S32 number); */
#define PowOf4(number) (1<<((number)<<1))

u_int32_t GetPowOf2(int32_t number);
u_int32_t BinaryFloatDiv(u_int32_t n1, u_int32_t n2, int32_t precision);
int32_t Get2Comp(int32_t a, int32_t width);
int32_t XtoPowerY(int32_t Number, u_int32_t Power);
int32_t RoundToNextHighestInteger(int32_t Number, u_int32_t Digits);
u_int32_t Log2Int(int32_t number);
int32_t SqrtInt(int32_t Sq);
unsigned int Log10Int(u_int32_t value);
u_int32_t Gcdivisor(u_int32_t m, u_int32_t n);

#endif /* _STFE_UTILITIES_H */
