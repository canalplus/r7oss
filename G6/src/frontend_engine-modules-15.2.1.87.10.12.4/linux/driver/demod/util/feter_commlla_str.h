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

Source file name : feter_commlla_str.h
Author :           LLA

LLA generic file

Date        Modification                                    Name
----        ------------                                    --------
02-Aug-11   Created                                         Ankur
04-Jul-12   Updated to v3.5
************************************************************************/
#ifndef _FETER_COMMLLA_STR_H
#define _FETER_COMMLLA_STR_H

/*************************************************************
INCLUDES
*************************************************************/
/*	#include <stddefs.h>    Standard definitions */
/*	#include <fe_tc_tuner.h> */

 /* enums common to all TER lla */

typedef enum {
	FE_TER_MOD_BPSK,
	FE_TER_MOD_QPSK,
	FE_TER_MOD_16QAM,
	FE_TER_MOD_64QAM,
	FE_TER_MOD_256QAM,
	FE_TER_MOD_8VSB,
	FE_TER_MOD_Reserved
} FE_TER_Modulation_t;		/* T / T2 */

typedef enum {
	FE_TER_GUARD_1_32,
	FE_TER_GUARD_1_16,
	FE_TER_GUARD_1_8,
	FE_TER_GUARD_1_4,
	/* T2 */
	FE_TER_GUARD_1_128,
	FE_TER_GUARD_19_128,
	FE_TER_GUARD_19_256,
	FE_TER_GUARD_Unknown
} FE_TER_Guard_t;		/* T / T2 */

typedef enum {
	FE_TER_HIER_ALPHA_NONE,	/* Regular modulation */
	FE_TER_HIER_ALPHA_1,	/* Hierarchical modulation a = 1 */
	FE_TER_HIER_ALPHA_2,	/* Hierarchical modulation a = 2 */
	FE_TER_HIER_ALPHA_4	/* Hierarchical modulation a = 4 */
} FE_TER_Hierarchy_Alpha_t;	/* T */

typedef enum {
	FE_TER_HIER_NONE,	/* Hierarchy None */
	FE_TER_HIER_LOW_PRIO,	/* Hierarchy : Low Priority */
	FE_TER_HIER_HIGH_PRIO	/* Hierarchy : High Priority */
} FE_TER_Hierarchy_t;		/* T */

typedef enum {
	FE_TER_INVERSION_NONE,
	FE_TER_INVERSION,
	FE_TER_INVERSION_Unknown
} FE_Spectrum_t;		/* T / T2 */

typedef enum {
	FE_TER_FORCENONE,
	FE_TER_FORCE
} FE_TER_Force_t;		/* T / T2 */

typedef enum {
	FE_TER_CHAN_BW_1M7 = 1,
	FE_TER_CHAN_BW_5M = 5,
	FE_TER_CHAN_BW_6M = 6,
	FE_TER_CHAN_BW_7M = 7,
	FE_TER_CHAN_BW_8M = 8
} FE_TER_ChannelBW_t;		/* T / T2 */

typedef enum {
	FE_TER_Rate_1_2,
	FE_TER_Rate_3_5,
	FE_TER_Rate_2_3,
	FE_TER_Rate_3_4,
	FE_TER_Rate_4_5,
	FE_TER_Rate_5_6,
	FE_TER_Rate_7_8,	/* T only */
	FE_TER_Rate_Reserved,
	FE_TER_Rate_Unknown
} FE_TER_FECRate_t;		/* T / T2 */

typedef enum {
	FE_TER_NORMAL_IF_TUNER,	/* IF/low IF */
	FE_TER_LONGPATH_IF_TUNER,	/* IF/low IF + digital IIR filter */
	FE_TER_IQ_TUNER		/* ZIF (e.g STV4100) */
} FE_TER_IF_IQ_Mode;		/* T / T2 */

typedef enum {
	FE_TER_T2_SISO,
	FE_TER_T2_MISO,
	/* FE_TER_non_T2, */
	FE_TER_TRANSMISSION_Unknown
} FE_TER_TransmissionSystem_t;	/* T2 */

typedef enum {
	FE_TER_FFT_2K,
	FE_TER_FFT_8K,
	FE_TER_FFT_4K,
	FE_TER_FFT_1K,
	FE_TER_FFT_16K,
	FE_TER_FFT_32K,
	FE_TER_FFT_Unknown
} FE_TER_FFT_t;			/* T / T2 */

typedef enum {
	FE_TER_No_Reduction,
	FE_TER_ACE_PAPR,
	FE_TER_TR_PAPR,
	FE_TER_ACE_TR_PAPR,
	FE_TER_PAPR_Unknown
} FE_TER_PAPR_t;		/* T2 */

typedef enum {
	FE_TER_No_EXT_Carrier,
	FE_TER_EXT_Carrier,
	FE_TER_EXT_Carrier_Unknown
} FE_TER_EXT_Carrier_t;		/* T2 */

typedef enum {
	FE_TER_PP1,
	FE_TER_PP2,
	FE_TER_PP3,
	FE_TER_PP4,
	FE_TER_PP5,
	FE_TER_PP6,
	FE_TER_PP7,
	FE_TER_PP8,
	FE_TER_Reserved,
	FE_TER_PP_Unknown
} FE_TER_PP_t;			/* T2 */

typedef enum {			/* T2 */
	    FE_TER_GFPS,
	FE_TER_GCE,
	FE_TER_GSE,
	FE_TER_TS
} FE_TER_StreamType_t;

typedef enum {			/* T2 */
	    FE_TER_T2_V1_1_1,
	FE_TER_T2_V1_2_1
} FE_TER_T2Version_t;

typedef enum {			/* T2 */
	    FE_TER_Plp_Common,
	FE_TER_Plp_DataType1,
	FE_TER_Plp_DataType2,
	FE_TER_Plp_DataTypeReserved
} FE_TER_PlpType_t;

typedef enum {			/* T2 */
	    FE_TER_Frame_Short,
	FE_TER_Frame_Normal
} FE_TER_FECFrame_t;

typedef enum {			/* T2 */
	    FE_TER_TimeInterleave_0,
	FE_TER_TimeInterleave_1
} FE_TER_TimeInterleave_t;

#if 0
typedef enum {			/* T2 */
	FE_TER_Optimize_NONE
} FE_TER_OptimizeTopic_t;
#endif

/****************************************************************
SEARCH STRUCTURES
****************************************************************/

typedef struct {
	/* additional lock params to help CHC lock better/faster TA/YM 07/11 */

	FE_TER_FFT_t FFTSize;	/* T/T2 */
	FE_TER_PP_t PilotPattern;	/* T2 generic */
	FE_TER_Guard_t Guard;	/* T/T2 */
	FE_Spectrum_t SpectInversion;	/* T/T2 */
	FE_TER_FECRate_t CodeRate;	/* T */
} FE_TER_OptimizeSearch_t;	/* T/T2 */

typedef struct {
	U32 Frequency_kHz;	/* T/T2 */
	FE_TER_IF_IQ_Mode IF_IQ_Mode;	/* T/T2 */
	FE_TER_Hierarchy_t Hierarchy;	/* T */
	FE_TER_ChannelBW_t ChannelBW_MHz;	/* T/T2 */
	/* T, T2 or unknown (let F/W decide) */
	FE_TransmissionStandard_t TransmitStandard;
	S16 PlpId;		/* T2, 0..255, (-1) = unknown Plp */
	FE_TER_OptimizeSearch_t Optimize;	/* T2 */
	/* T2 in multiples of <WAIT_LOCK_TIME_UNIT>ms, see Algo() */
	U16 LockTimeout;
	U16 SlowI2CPolling;	/* T2 */
	U8 AGCTrackingTime;	/* T2 temporary */
	U8 AGCAcquisitionTime;	/* T2 temporary */
	U8 AGCTarget;		/* T2 temporary */
	S32 TrlOffset;		/* T2 temporary */
	BOOL L1_Tune;		/* T2 */
} FE_TER_SearchParams_t;

typedef struct {
	BOOL Locked;		/* T/T2 T2: if both DEMOD and BB locked ok */
	BOOL DemodLocked;	/* T2: if DEMOD locked ok */
	/* T2: time spent in innermost lock loop after startdvbt2 */
	U16 InnerLockTime;
	U32 Frequency_kHz;	/* T/T2 including offset */
	FE_TER_FFT_t FFTSize;	/* T/T2 */
	FE_TER_Guard_t Guard;	/* T/T2 */
	FE_TER_Modulation_t Modulation;	/* T/T2 */
	FE_Spectrum_t SpectInversion;	/* T/T2 */
	FE_TER_Hierarchy_t Hierarchy;	/* T */
	FE_TER_Hierarchy_Alpha_t Hierarchy_Alpha;	/* T */
	FE_TER_FECRate_t HPRate;	/* T */
	FE_TER_FECRate_t LPRate;	/* T */
	FE_TER_FECRate_t FECRate;	/* T/T2 */
	FE_TER_TransmissionSystem_t TransmissionSystem;	/* T2 */
	FE_TER_EXT_Carrier_t ExtendedCarrier;	/* T2 */
	FE_TER_PAPR_t Papr;	/* T2 */
	U16 NbOfPlp;		/* T2 0..256 */
	S16 Plp_Id;		/* T2 <= NbOfPlp, (-1) = unknown Plp */
	U8 Plp_Group;		/* T2 0..255 */
	FE_TER_Modulation_t L1Post_Modulation;	/* T2 */
	U8 NbOfT2Frames;	/* T2 */
	S16 NbOfDataSymbols;	/* T2 OFDM per T2 frame, (-1) = unknown */
	BOOL PlpRotation;	/* T2 */
	FE_TER_ChannelBW_t ChannelBW_MHz;	/* T/T2 generic */
	FE_TER_PP_t PilotPattern;	/* T2 generic */
	/* T2 generic T, T2 or unknown (let F/W decide) */
	FE_TransmissionStandard_t TransmitStandard;
	FE_TER_StreamType_t StreamType;	/* T2 generic */
	BOOL FEF;		/* T2 generic */
	FE_TER_T2Version_t L1_T2Version;	/* T2 L1 Post */
	BOOL L1Post_Extension;	/* T2 L1 Post */
	FE_TER_PlpType_t Plp_Type;	/* T2 decoded PLP */
	FE_TER_FECFrame_t FEC_Frame;	/* T2 decoded PLP */
	FE_TER_TimeInterleave_t Time_Interleave;	/* T2 decoded PLP */
	U8 IJump;		/* T2 frame interval L1Post */
	U16 Cell_Id;		/* T2 L1 (hex) */
	U16 Network_Id;		/* T2 L1 (hex) */
	U16 System_Id;		/* T2 L1 (hex) */
	S32 TrlOffset;		/* T2 */
} FE_TER_SearchResult_t;

	/************************
		INFO STRUCTURE
	************************/

typedef struct {
	BOOL Locked;		/* T/T2 */
	U32 Frequency_kHz;	/* T/T2 including offset */
	FE_TER_Hierarchy_t Hierarchy;	/* T */
	S16 Plp_Id;		/* T2 <= NbOfPlp, (-1) = unknown Plp */
	S32 RFLevel_x10dBm_s32;	/* T/T2 Power of the RF signal (dBm x 10) */
	U32 RFLevel_x100percent_u32;	/* T/T2 Power percentage 0..100% */
	U32 SNR_x10dB_u32;	/* T/T2 Carrier to noise ratio (dB x 10) */
	U32 SNR_x100percent_u32; /* T/T2 Carrier to noise ratio 0..100% */
	U8 LDPC_Iterations;	/* T2 */
	U32 BER_xE7_u32;	/* T/T2 */
	U32 CorrectedBitsNumber;	/* T/T2 */
	U32 PER_xE7_u32;	/* T/T2 PER or FER */
	U32 CorruptedPacketsNumber;	/* T:packets, T2:frames */
	U8 SSI;	/* T/T2 Signal Strength Indicator 0..100% (Nordig 2.2) */
	U8 SQI;	/* T/T2 Signal Quality  Indicator 0..100% (Nordig 2.2) */
} FE_TER_SignalInfo_t;

#endif /* _FETER_COMMLLA_STR_H */
