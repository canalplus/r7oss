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

Source file name : fecab_commlla_str.h
Author :           LLA

LLA generic file

Date        Modification                                    Name
----        ------------                                    --------
01-Aug-11   Created
04-Jul-12   Updated to v1.9
************************************************************************/
#ifndef _FECAB_COMMLLA_STR_H
#define _FECAB_COMMLLA_STR_H

/*************************************************************
INCLUDES
*************************************************************/
/*      #include <stddefs.h>     Standard definitions */
/*      #include <fe_tc_tuner.h> */

/****************************************************************
Structures/typedefs common to all cable demodulators
****************************************************************/
typedef enum {
	FE_CAB_MOD_QAM4,
	FE_CAB_MOD_QAM16,
	FE_CAB_MOD_QAM32,
	FE_CAB_MOD_QAM64,
	FE_CAB_MOD_QAM128,
	FE_CAB_MOD_QAM256,
	FE_CAB_MOD_QAM512,
	FE_CAB_MOD_QAM1024
} FE_CAB_Modulation_t;

typedef enum {
	FE_CAB_SPECT_NORMAL,
	FE_CAB_SPECT_INVERTED
} FE_CAB_SpectInv_t;

typedef enum {			/* Ankush, 03/2009 */
	    FE_CAB_FEC_A = 1,	/* J83 Annex A */
	FE_CAB_FEC_B = (1 << 1),	/* J83 Annex B */
	FE_CAB_FEC_C = (1 << 2)	/* J83 Annex C */
} FE_CAB_FECType_t;

typedef enum {
	DCT7045_297E_Eval,
	DCT7044_297E_NIM,
	MT2060_297E_NIM,
	MT2060_QAMi5107,
	DTT7546_367qam_Eval,
	MXL203RF_CAB5197_V12,	/* added PJ 10/2009 */
	DCT7040_297J_Eval,
	STI7197HDK,
	DTT7546_368dvbc_Eval
} FE_CAB_Board_t;

typedef struct {
	U32 Frequency_kHz;	/* channel frequency (in kHz) */
	U32 SymbolRate_Bds;	/* channel symbol rate  (in bds) */
	U32 SearchRange_Hz;	/* range of the search (in Hz) */
	U32 SweepRate_Hz;	/* Sweep Rate (in Hz) */
	FE_CAB_Modulation_t Modulation;	/* modulation */
	FE_CAB_SpectInv_t SpectrumInversion;	/* Spectrum Inversion */
	FE_CAB_FECType_t FECType;	/* FEC when a multi FEC is used */
} FE_CAB_SearchParams_t;

typedef struct {
	BOOL Locked;		/* channel found */
	U32 Frequency_kHz;	/* found frequency (in kHz) */
	U32 SymbolRate_Bds;	/* found symbol rate (in Bds) */
	FE_CAB_Modulation_t Modulation;	/* modulation */
	FE_CAB_SpectInv_t SpectInv;	/* Spectrum Inversion */
	U8 Interleaver;		/* Interleaver in FEC B mode */
} FE_CAB_SearchResult_t;

typedef struct {
	char DemodName[20];	/* Cable demodulator name */
	U32 DemodI2cAddr;	/* Cable demodulator I2C address */
	TUNER_Model_t TunerModel;	/* Cable tuner model */
	char TunerName[20];	/* Cable tuner name */
	U8 TunerI2cAddr;	/* Cable tuner I2C address */
	U8 TunerI2cAddr2;	/* Provision to access a second IC if needed */
	FE_TS_Config_t TS_Config;	/* TS Format */
	S32 DemodXtal_Hz;	/* Cable demodulator crystal frequency in Hz */
	U32 TunerXtal_Hz;	/* Cable tuner crystal frequency in Hz */
	U32 TunerIF_kHz;	/* Cable demodulator IF frequency in kHz */
	FE_CAB_Board_t TargetBoard;	/* Targetted board name */
	/* Reception standard (J.83A/B/C) */
	FE_TransmissionStandard_t TransmitStandard;
} FE_CAB_InitParams_t;

	/************************
		INFO STRUCTURE
	************************/
typedef struct {
	BOOL Locked;		/* Channel locked */
	U32 Frequency;		/* Channel frequency (in KHz) */
	U32 SymbolRate;		/* Channel symbol rate  (in Mbds) */
	FE_CAB_Modulation_t Modulation;	/* modulation */
	FE_CAB_SpectInv_t SpectInversion;	/* Spectrum Inversion */
	S32 RFLevelx10dBm_s32;	/* Power of the RF signal (dBm x 10) */
	U32 SNRx10dB_u32;	/* Signal to noise ratio (dB x 10) */
	U32 BERxE7_u32;		/* Bit error rate (x 10000000) */
	U32 PERxE7_u32;		/* Packet error rate (x 10000000) */
	U32 RFLevelx100Percent_u32; /* RF level in percentage for a bargraph */
	U32 SNRx100Percent_u32;	/* SNR in percentage for a bargraph */
	U8 Interleaver;		/* Interleaver in FEC B mode */
#ifdef HOST_PC
	double BER_dbl;		/* BER in double for GUI and DLL */
	double SNRdB_dbl;	/* MER in dB in double for GUI and DLL */
	double PER_dbl;		/* PER in double for GUI and DLL */
	double RFLeveldBm_dbl;	/* RF level in dbm in double for GUI and DLL */
#endif
} FE_CAB_SignalInfo_t;
/* End of fecab_commlla_str.h */
#endif /* _FECAB_COMMLLA_STR_H */
