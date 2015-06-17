/************************************************************************
Copyright(C) 2013 STMicroelectronics. All Rights Reserved.

This file is part of the stm_fe Library.

stm_fe is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

stm_fe is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY ; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with stm_fe ; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111 - 1307, USA.

The stm_fe Library may alternatively be licensed under a proprietary
license from ST.

Source file name : stv0913_drv.c
Author :

Low level driver for STV0913

Date		Modification	Name
----		------------	--------
23 - Apr - 12	Created

************************************************************************/
/* includes ---------------------------------------------------------- */
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 2, 2)
#include <linux/export.h>
#else
#include <linux/module.h>
#endif
#include "stv0913_util.h"
#include "stv0913_drv.h"
#include <gen_macros.h>
#include <stvvglna.h>

#ifdef HOST_PC
#ifndef NO_GUI
#include <STV0913_GUI.h>
#include <Appl.h>
#include <Pnl_Report.h>
#include <formatio.h>
#include <UserPar.h>
#endif
#endif

#define STV0913_BLIND_SEARCH_AGC2BANDWIDTH	40
#define DmdLock_TIMEOUT_LIMIT	550
#define STV0913_IQPOWER_THRESHOLD	30

/****************************************************************
 ****************************************************************
 ****************************************************************
 ****
 ***************************************************
 **	Static Data
 ***************************************************
 ****
 ****************************************************************
 ****************************************************************
 ****************************************************************/

/************************
  Current LLA revision
 *************************/
static const ST_Revision_t RevisionSTV0913 =
			"STV0913 - LLA_REL_2.7_August_2013";

/***********************************
VTH vs. CNR(data normalized module)
************************************/
static FE_STV0913_LOOKUP_t FE_STV0913_VTH_Lookup = {
	5,
	{
	 {250, 8780},		/*C/N = 1.5dB */
	 {100, 7405},		/*C/N = 4.5dB */
	 {40, 6330},		/*C/N = 6.5dB */
	 {12, 5224},		/*C/N = 8.5dB */
	 {5, 4236}		/*C/N = 10.5dB */
	 }
};

/******************************
PADC(dBm) vs. (POWERI + POWERQ)
*******************************/
FE_STV0913_LOOKUP_t FE_STV0913_PADC_Lookup = {
	15,
	{
	 {-2000, 1179},		/*PADC = -20dBm */
	 {-1900, 1485},		/*PADC = -19dBm */
	 {-1700, 2354},		/*PADC = -17dBm */
	 {-1500, 3730},		/*PADC = -15dBm */
	 {-1300, 5910},		/*PADC = -13dBm */
	 {-1100, 9380},		/*PADC = -11dBm */
	 {-900, 14850},		/*PADC = -9dBm */
	 {-700, 23520},		/*PADC = -7dBm */
	 {-600, 29650},		/*PADC = -6dBm */
	 {-500, 37300},		/*PADC = -5dBm */
	 {-400, 47000},		/*PADC = -4dBm */
	 {-300, 59100},		/*PADC = -3dBm */
	 {-200, 74500},		/*PADC = -2dBm */
	 {-100, 93600},		/*PADC = -1dBm */
	 {0, 118000}		/*PADC=+0dBm */
	 }
};

/******************************
DVBS1 and DSS C/N Look - Up table
*******************************/
FE_STV0913_LOOKUP_t FE_STV0913_S2_CN_LookUp = {
	60,
	{
	 {-30, 13950},		/*C/N = -2.5dB */
	 {-25, 13580},		/*C/N = -2.5dB */
	 {-20, 13150},		/*C/N = -2.0dB */
	 {-15, 12760},		/*C/N = -1.5dB */
	 {-10, 12345},		/*C/N = -1.0dB */
	 {-05, 11900},		/*C/N = -0.5dB */
	 {0, 11520},		/*C/N = 0dB */
	 {05, 11080},		/*C/N = 0.5dB */
	 {10, 10630},		/*C/N = 1.0dB */
	 {15, 10210},		/*C/N = 1.5dB */
	 {20, 9790},		/*C/N = 2.0dB */
	 {25, 9390},		/*C/N = 2.5dB */
	 {30, 8970},		/*C/N = 3.0dB */
	 {35, 8575},		/*C/N = 3.5dB */
	 {40, 8180},		/*C/N = 4.0dB */
	 {45, 7800},		/*C/N = 4.5dB */
	 {50, 7430},		/*C/N = 5.0dB */
	 {55, 7080},		/*C/N = 5.5dB */
	 {60, 6720},		/*C/N = 6.0dB */
	 {65, 6320},		/*C/N = 6.5dB */
	 {70, 6060},		/*C/N = 7.0dB */
	 {75, 5760},		/*C/N = 7.5dB */
	 {80, 5480},		/*C/N = 8.0dB */
	 {85, 5200},		/*C/N = 8.5dB */
	 {90, 4930},		/*C/N = 9.0dB */
	 {95, 4680},		/*C/N = 9.5dB */
	 {100, 4425},		/*C/N = 10.0dB */
	 {105, 4210},		/*C/N = 10.5dB */
	 {110, 3980},		/*C/N = 11.0dB */
	 {115, 3765},		/*C/N = 11.5dB */
	 {120, 3570},		/*C/N = 12.0dB */
	 {125, 3315},		/*C/N = 12.5dB */
	 {130, 3140},		/*C/N = 13.0dB */
	 {135, 2980},		/*C/N = 13.5dB */
	 {140, 2820},		/*C/N = 14.0dB */
	 {145, 2670},		/*C/N = 14.5dB */
	 {150, 2535},		/*C/N = 15.0dB */
	 {160, 2270},		/*C/N = 16.0dB */
	 {170, 2035},		/*C/N = 17.0dB */
	 {180, 1825},		/*C/N = 18.0dB */
	 {190, 1650},		/*C/N = 19.0dB */
	 {200, 1485},		/*C/N = 20.0dB */
	 {210, 1340},		/*C/N = 21.0dB */
	 {220, 1212},		/*C/N = 22.0dB */
	 {230, 1100},		/*C/N = 23.0dB */
	 {240, 1000},		/*C/N = 24.0dB */
	 {250, 910},		/*C/N = 25.0dB */
	 {260, 836},		/*C/N = 26.0dB */
	 {270, 772},		/*C/N = 27.0dB */
	 {280, 718},		/*C/N = 28.0dB */
	 {290, 671},		/*C/N = 29.0dB */
	 {300, 635},		/*C/N = 30.0dB */
	 {310, 602},		/*C/N = 31.0dB */
	 {320, 575},		/*C/N = 32.0dB */
	 {330, 550},		/*C/N = 33.0dB */
	 {350, 517},		/*C/N = 35.0dB */
	 {400, 480},		/*C/N = 40.0dB */
	 {450, 466},		/*C/N = 45.0dB */
	 {500, 464},		/*C/N = 50.0dB */
	 {510, 463},		/*C/N = 51.0dB */
	 }
};

/**********************
DVBS2 C/N Look - Up table
***********************/
FE_STV0913_LOOKUP_t FE_STV0913_S1_CN_LookUp = {
	54,
	{
	 {0, 9242},		/*C/N = -0dB */
	 {05, 9105},		/*C/N = 0.5dB */
	 {10, 8950},		/*C/N = 1.0dB */
	 {15, 8780},		/*C/N = 1.5dB */
	 {20, 8566},		/*C/N = 2.0dB */
	 {25, 8366},		/*C/N = 2.5dB */
	 {30, 8146},		/*C/N = 3.0dB */
	 {35, 7908},		/*C/N = 3.5dB */
	 {40, 7666},		/*C/N = 4.0dB */
	 {45, 7405},		/*C/N = 4.5dB */
	 {50, 7136},		/*C/N = 5.0dB */
	 {55, 6861},		/*C/N = 5.5dB */
	 {60, 6576},		/*C/N = 6.0dB */
	 {65, 6330},		/*C/N = 6.5dB */
	 {70, 6048},		/*C/N = 7.0dB */
	 {75, 5768},		/*C/N = 7.5dB */
	 {80, 5492},		/*C/N = 8.0dB */
	 {85, 5224},		/*C/N = 8.5dB */
	 {90, 4959},		/*C/N = 9.0dB */
	 {95, 4709},		/*C/N = 9.5dB */
	 {100, 4467},		/*C/N = 10.0dB */
	 {105, 4236},		/*C/N = 10.5dB */
	 {110, 4013},		/*C/N = 11.0dB */
	 {115, 3800},		/*C/N = 11.5dB */
	 {120, 3598},		/*C/N = 12.0dB */
	 {125, 3406},		/*C/N = 12.5dB */
	 {130, 3225},		/*C/N = 13.0dB */
	 {135, 3052},		/*C/N = 13.5dB */
	 {140, 2889},		/*C/N = 14.0dB */
	 {145, 2733},		/*C/N = 14.5dB */
	 {150, 2587},		/*C/N = 15.0dB */
	 {160, 2318},		/*C/N = 16.0dB */
	 {170, 2077},		/*C/N = 17.0dB */
	 {180, 1862},		/*C/N = 18.0dB */
	 {190, 1670},		/*C/N = 19.0dB */
	 {200, 1499},		/*C/N = 20.0dB */
	 {210, 1347},		/*C/N = 21.0dB */
	 {220, 1213},		/*C/N = 22.0dB */
	 {230, 1095},		/*C/N = 23.0dB */
	 {240, 992},		/*C/N = 24.0dB */
	 {250, 900},		/*C/N = 25.0dB */
	 {260, 826},		/*C/N = 26.0dB */
	 {270, 758},		/*C/N = 27.0dB */
	 {280, 702},		/*C/N = 28.0dB */
	 {290, 653},		/*C/N = 29.0dB */
	 {300, 613},		/*C/N = 30.0dB */
	 {310, 579},		/*C/N = 31.0dB */
	 {320, 550},		/*C/N = 32.0dB */
	 {330, 526},		/*C/N = 33.0dB */
	 {350, 490},		/*C/N = 33.0dB */
	 {400, 445},		/*C/N = 40.0dB */
	 {450, 430},		/*C/N = 45.0dB */
	 {500, 426},		/*C/N = 50.0dB */
	 {510, 425}		/*C/N = 51.0dB */
	 }
};

struct FE_STV0913_CarLoopVsModCod {
	FE_STV0913_ModCod_t ModCode;
	U8 CarLoopPilotsOn_2;
	U8 CarLoopPilotsOff_2;
	U8 CarLoopPilotsOn_5;
	U8 CarLoopPilotsOff_5;
	U8 CarLoopPilotsOn_10;
	U8 CarLoopPilotsOff_10;
	U8 CarLoopPilotsOn_20;
	U8 CarLoopPilotsOff_20;
	U8 CarLoopPilotsOn_30;
	U8 CarLoopPilotsOff_30;

};

/*****************************************************************
Tracking carrier loop carrier QPSK 1/4 to QPSK 2/5 Normal Frame
Tracking carrier loop carrier QPSK 1/2 to 8PSK 9/10 Normal Frame
******************************************************************/
#define NB_SAT_MODCOD 17
static struct FE_STV0913_CarLoopVsModCod FE_STV0913_S2CarLoop[NB_SAT_MODCOD] = {
/*Modcod 2MPon	2MPoff 5MPon 5MPoff 10MPon 10MPoff 20MPon 20MPoff 30MPon
30MPoff */
/* Low CR QPSK */
	{FE_SAT_QPSK_14, 0x0B, 0x2A, 0x2A, 0x2A, 0x1A, 0x2A, 0x0A, 0x2A, 0x39,
	0x1A},
	{FE_SAT_QPSK_13, 0x0B, 0x2A, 0x2A, 0x2A, 0x1A, 0x2A, 0x1A, 0x2A, 0x0A,
	 0x1A},
	{FE_SAT_QPSK_25, 0x1B, 0x2A, 0x3A, 0x2A, 0x2A, 0x2A, 0x2A, 0x2A, 0x1A,
	 0x1A},
/* QSPSK */
	{FE_SAT_QPSK_12, 0x0C, 0x1C, 0x2B, 0x1C, 0x0B, 0x2C, 0x0B, 0x0C, 0x2A,
	 0x2B},
	{FE_SAT_QPSK_35, 0x1C, 0x1C, 0x2B, 0x1C, 0x0B, 0x2C, 0x0B, 0x0C, 0x2A,
	 0x2B},
	{FE_SAT_QPSK_23, 0x2C, 0x2C, 0x2B, 0x1C, 0x0B, 0x2C, 0x0B, 0x0C, 0x2A,
	 0x2B},
	{FE_SAT_QPSK_34, 0x3C, 0x2C, 0x3B, 0x2C, 0x1B, 0x1C, 0x1B, 0x3B, 0x3A,
	 0x1B},
	{FE_SAT_QPSK_45, 0x0D, 0x3C, 0x3B, 0x2C, 0x1B, 0x1C, 0x1B, 0x3B, 0x3A,
	 0x1B},
	{FE_SAT_QPSK_56, 0x1D, 0x3C, 0x0C, 0x2C, 0x2B, 0x1C, 0x1B, 0x3B, 0x0B,
	 0x1B},
	{FE_SAT_QPSK_89, 0x3D, 0x0D, 0x0C, 0x2C, 0x2B, 0x0C, 0x2B, 0x2B, 0x0B,
	 0x0B},
	{FE_SAT_QPSK_910, 0x1E, 0x0D, 0x1C, 0x2C, 0x3B, 0x0C, 0x2B, 0x2B, 0x1B,
	 0x0B},
/* 8SPSK */
	{FE_SAT_8PSK_35, 0x28, 0x09, 0x28, 0x09, 0x28, 0x09, 0x28, 0x08, 0x28,
	 0x27},
	{FE_SAT_8PSK_23, 0x19, 0x29, 0x19, 0x29, 0x19, 0x29, 0x38, 0x19, 0x28,
	 0x09},
	{FE_SAT_8PSK_34, 0x1A, 0x0B, 0x1A, 0x3A, 0x0A, 0x2A, 0x39, 0x2A, 0x39,
	 0x1A},
	{FE_SAT_8PSK_56, 0x2B, 0x2B, 0x1B, 0x1B, 0x0B, 0x1B, 0x1A, 0x0B, 0x1A,
	 0x1A},
	{FE_SAT_8PSK_89, 0x0C, 0x0C, 0x3B, 0x3B, 0x1B, 0x1B, 0x2A, 0x0B, 0x2A,
	 0x2A},
	{FE_SAT_8PSK_910, 0x0C, 0x1C, 0x0C, 0x3B, 0x2B, 0x1B, 0x3A, 0x0B, 0x2A,
	 0x2A},
};

/*************************************************************
 *************************************************************
 ****
 ****
 **************************************************
 **	Private Routines
 **************************************************
 ****
 ****
 *************************************************************
 *************************************************************
 *************************************************************/

/***************************************************************
  Static FUNCTIONS
 ***************************************************************/

static S32 FE_STV0913_TimingGetOffset(STCHIP_Handle_t hChip, U32 SymbolRate);
static U32 FE_STV0913_GetBer(STCHIP_Handle_t hChip);
static FE_STV0913_Rate_t FE_STV0913_GetViterbiPunctureRate(STCHIP_Handle_t
							   hChip);
static void FE_STV0913_GetLockTimeout(S32 *DemodTimeout, S32 *FecTimeout,
				      S32 SymbolRate,
				      FE_STV0913_SearchAlgo_t Algo);
static void FE_STV0913_setVTH(STCHIP_Handle_t hChip,
					FE_STV0913_LOOKUP_t *lookup);
static BOOL FE_STV0913_GetDemodLock(STCHIP_Handle_t hChip, S32 TimeOut);
static BOOL FE_STV0913_GetFECLock(STCHIP_Handle_t hChip, S32 TimeOut);
static void FE_STV0913_SetViterbiStandard(STCHIP_Handle_t hChip,
					  FE_STV0913_SearchStandard_t Standard,
					  FE_STV0913_Rate_t PunctureRate);
static S32 FE_STV0913_CheckVGLNAPresent(STCHIP_Handle_t hChipVGLNA);
static void FE_STV0913_SetSearchStandard(FE_STV0913_InternalParams_t *pParams);
static void FE_STV0913_StartSearch(FE_STV0913_InternalParams_t *pParams);
static void FE_STV0913_TrackingOptimization(FE_STV0913_InternalParams_t
							*pParams);
static FE_STV0913_SIGNALTYPE_t
FE_STV0913_GetSignalParams(FE_STV0913_InternalParams_t *pParams,
							U32 Satellite_Scan);
static BOOL FE_STV0913_GetDemodLockCold(FE_STV0913_InternalParams_t *pParams,
							S32 DemodTimeout);
static U16 FE_STV0913_BlindCheckAGC2BandWidth(FE_STV0913_InternalParams_t
							*pParams);
static BOOL FE_STV0913_BlindSearchAlgo(FE_STV0913_InternalParams_t *pParams,
							S32 demodTimeout,
							U32 Satellite_Scan);
static FE_STV0913_Error_t FE_STV0913_8PSKCarrierAdaptation(FE_STV0913_Handle_t
							Handle);
static FE_STV0913_SIGNALTYPE_t FE_STV0913_Algo(FE_STV0913_InternalParams_t
						*pParams,
						U32 Satellite_Scan);
static void FE_STV0913_SetChargePump(FE_STV0913_Handle_t Handle, U32 n_div);

/*****************************************************
**FUNCTION	::	GetMclkFreq
**ACTION	::	Set the STV0913 master clock frequency
**PARAMS IN	::  hChip		==>	handle to the chip
**			ExtClk		==>	External clock frequency(Hz)
**PARAMS OUT::	NONE
**RETURN	::	Synthesizer frequency(Hz)
*****************************************************/
U32 FE_STV0913_GetMclkFreq(STCHIP_Handle_t hChip, U32 ExtClk)
{
	U32 mclk = 135000000, ndiv = 1, idf = 1, ldf = 1, odf = 1, fvco = 1;

	odf = ChipGetField(hChip, FSTV0913_ODF);
	ndiv = ChipGetOneRegister(hChip, RSTV0913_NCOARSE1);
	idf = ChipGetField(hChip, FSTV0913_IDF);

	if (ndiv < 8)
		ndiv = 8;

	ldf = 2 * ndiv;

	if (idf == 0)
		idf = 1;
	if (odf == 0)
		odf = 1;

	fvco = ((ExtClk / 1000000) * ldf) / (idf);
	mclk = (fvco) / (2 * odf);

	return mclk * 1000000;
	/*printf("\nMclk =%d", mclk); */
}

/*****************************************************
**FUNCTION	::	GetErrorCount
**ACTION	::	return the number of errors from a given counter
**PARAMS IN	::  hChip		==>	handle to the chip
**			::	Counter		==>	used counter 1 or 2.
			::
**PARAMS OUT::	NONE
**RETURN	::	Synthesizer frequency(Hz)
*****************************************************/
U32 FE_STV0913_GetErrorCount(STCHIP_Handle_t hChip,
			     FE_STV0913_ERRORCOUNTER Counter)
{
	U32 lsb, msb, hsb, errValue;

	/*Read the Error value */
	switch (Counter) {
	case FE_STV0913_COUNTER1:
	default:
		ChipGetRegisters(hChip, RSTV0913_ERRCNT12, 3);
		hsb = ChipGetFieldImage(hChip, FSTV0913_ERR_CNT12);
		msb = ChipGetFieldImage(hChip, FSTV0913_ERR_CNT11);
		lsb = ChipGetFieldImage(hChip, FSTV0913_ERR_CNT10);
		break;

	case FE_STV0913_COUNTER2:
		ChipGetRegisters(hChip, RSTV0913_ERRCNT22, 3);
		hsb = ChipGetFieldImage(hChip, FSTV0913_ERR_CNT22);
		msb = ChipGetFieldImage(hChip, FSTV0913_ERR_CNT21);
		lsb = ChipGetFieldImage(hChip, FSTV0913_ERR_CNT20);
		break;
	}

	/*Cupute the Error value 3 bytes(HSB, MSB, LSB) */
	errValue = (hsb << 16) + (msb << 8) + (lsb);
	return errValue;
}

/*****************************************************
**FUNCTION	::	STV0913_RepeaterFn  (First repeater)
**ACTION	::	Set the repeater On or OFF
**PARAMS IN	::  hChip		==>	handle to the chip
			::	State		==> repeater state On/Off.
**PARAMS OUT::	NONE
**RETURN	::	Error(if any)
*****************************************************/

#ifdef CHIP_STAPI
STCHIP_Error_t FE_STV0913_RepeaterFn(STCHIP_Handle_t hChip, BOOL State,
				     unsigned char *Buffer)
#else
STCHIP_Error_t FE_STV0913_RepeaterFn(STCHIP_Handle_t hChip, BOOL State)
#endif
{
	STCHIP_Error_t error = CHIPERR_NO_ERROR;

	if (hChip != NULL) {
		if (State == TRUE)
#ifdef CHIP_STAPI
			ChipFillRepeaterMessage(hChip, FSTV0913_I2CT_ON, 1,
						Buffer);
#else
			ChipSetField(hChip, FSTV0913_I2CT_ON, 1);
#endif
		error = hChip->Error;
	}
	return error;
}

/*****************************************************
--FUNCTION	::	CarrierWidth
--ACTION	::	Compute the width of the carrier
--PARAMS IN	::	SymbolRate->Symbol rate of the carrier(Kbauds or Mbauds)
--			RollOff		->	Rolloff * 100
--PARAMS OUT::	NONE
--RETURN	::	Width of the carrier(KHz or MHz)
--***************************************************/
U32 FE_STV0913_CarrierWidth(U32 SymbolRate, FE_STV0913_RollOff_t RollOff)
{
	U32 rolloff;

	switch (RollOff) {
	case FE_SAT_20:
		rolloff = 20;
		break;

	case FE_SAT_25:
		rolloff = 25;
		break;

	case FE_SAT_35:
	default:
		rolloff = 35;
		break;
	}
	return SymbolRate + (SymbolRate * rolloff) / 100;
}

void FE_STV0913_SetTS_Parallel_Serial(STCHIP_Handle_t hChip,
				      FE_STV0913_Clock_t PathTS)
{

	switch (PathTS) {
	case FE_TS_SERIAL_PUNCT_CLOCK:
		/*Serial mode */
		ChipSetFieldImage(hChip, FSTV0913_TSFIFO_SERIAL, 0x01);
		/*punctur clk */
		ChipSetFieldImage(hChip, FSTV0913_TSFIFO_DVBCI, 0);
		break;

	case FE_TS_SERIAL_CONT_CLOCK:
		/*Serial mode */
		ChipSetFieldImage(hChip, FSTV0913_TSFIFO_SERIAL, 0x01);
		/*continues clock */
		ChipSetFieldImage(hChip, FSTV0913_TSFIFO_DVBCI, 1);
		break;

	case FE_TS_PARALLEL_PUNCT_CLOCK:
		/*Parallel mode*/
		ChipSetFieldImage(hChip, FSTV0913_TSFIFO_SERIAL, 0x00);
		/*pucntured clock */
		ChipSetFieldImage(hChip, FSTV0913_TSFIFO_DVBCI, 0);
		break;

	case FE_TS_DVBCI_CLOCK:
		/*Parallel mode*/
		ChipSetFieldImage(hChip, FSTV0913_TSFIFO_SERIAL, 0x00);
		/*continous clock*/
		ChipSetFieldImage(hChip, FSTV0913_TSFIFO_DVBCI, 1);
		break;

	default:
		break;
	}
	ChipSetRegisters(hChip, RSTV0913_TSCFGH, 1);

}

/*****************************************************
--FUNCTION	::	GetCarrierFrequency
--ACTION	::	Return the carrier frequency offset
--PARAMS IN	::	MasterClock	->Masterclock frequency(Hz)
			::
--PARAMS OUT::	NONE
--RETURN	::	Frequency offset in Hz.
--***************************************************/
S32 FE_STV0913_GetCarrierFrequency(STCHIP_Handle_t hChip, U32 MasterClock)
{
	S32 derot, rem1, rem2, intval1, intval2, carrierFrequency;

	BOOL sign = 1;

	/*      Read the carrier frequency regs value   */

	ChipGetRegisters(hChip, RSTV0913_CFR2, 3);
	derot = (ChipGetFieldImage(hChip, FSTV0913_CAR_FREQ2) << 16) +
	    (ChipGetFieldImage(hChip, FSTV0913_CAR_FREQ1) << 8) +
	    (ChipGetFieldImage(hChip, FSTV0913_CAR_FREQ0));

	/*      cumpute the signed value        */
	derot = Get2Comp(derot, 24);

	if (derot < 0) {
		sign = 0;
		/* Use positive values only =>avoid (-)tive value truncation */
		derot = -derot;
	}

	/*
	   Formulat:
	   carrier_frequency = MasterClock * Reg / 2^24
	 */

	intval1 = MasterClock >> 12;

	intval2 = derot >> 12;

	rem1 = MasterClock % 0x1000;
	rem2 = derot % 0x1000;

	derot = (intval1 * intval2) + ((intval1 * rem2) >> 12) +
			((intval2 * rem1) >> 12);/*only for integer calc */

	if (sign == 1)
		carrierFrequency = derot;	/* positive offset */
	else
		carrierFrequency = -derot;	/* negative offset */

	return carrierFrequency;
}

static S32 FE_STV0913_TimingGetOffset(STCHIP_Handle_t hChip, U32 SymbolRate)
{
	S32 timingoffset;

	/*      Formulat :
	   SR_Offset = TMGRREG * SR /2^29
	   TMGREG is 3 bytes registers value
	   SR is the current symbol rate
	 */
	ChipGetRegisters(hChip, RSTV0913_TMGREG2, 3);

	timingoffset = (ChipGetFieldImage(hChip, FSTV0913_TMGREG2) << 16) +
	    (ChipGetFieldImage(hChip, FSTV0913_TMGREG1) << 8) +
	    (ChipGetFieldImage(hChip, FSTV0913_TMGREG0));

	timingoffset = Get2Comp(timingoffset, 24);

	if (timingoffset == 0)
		timingoffset = 1;
	timingoffset =
	    ((S32) SymbolRate * 10) / ((S32) 0x1000000 / timingoffset);
	timingoffset /= 320;

	return timingoffset;

}

/*****************************************************
--FUNCTION	::	GetSymbolRate
--ACTION	::	Get the current symbol rate
--PARAMS IN	::	hChip		->	handle to the chip
			::	MasterClock	->Masterclock frequency(Hz)
			::
--PARAMS OUT::	NONE
--RETURN	::	Symbol rate in Symbol/s
*****************************************************/
U32 FE_STV0913_GetSymbolRate(STCHIP_Handle_t hChip, U32 MasterClock)
{
	S32 rem1, rem2, intval1, intval2, symbolRate;

	ChipGetRegisters(hChip, RSTV0913_SFR3, 4);
	symbolRate = (ChipGetFieldImage(hChip, FSTV0913_SYMB_FREQ3) << 24) +
	    (ChipGetFieldImage(hChip, FSTV0913_SYMB_FREQ2) << 16) +
	    (ChipGetFieldImage(hChip, FSTV0913_SYMB_FREQ1) << 8) +
	    (ChipGetFieldImage(hChip, FSTV0913_SYMB_FREQ0));

	/*      Formulat :
	   Found_SR = Reg * MasterClock /2^32
	 */

	intval1 = (MasterClock) >> 16;
	intval2 = (symbolRate) >> 16;

	rem1 = (MasterClock) % 0x10000;
	rem2 = (symbolRate) % 0x10000;

	symbolRate = (intval1 * intval2) + ((intval1 * rem2) >> 16) +
				((intval2 * rem1) >> 16);/*only for int calc */

	return symbolRate;

}

/*****************************************************
--FUNCTION	::	SetSymbolRate
--ACTION	::	Get the Symbol rate
--PARAMS IN	::	hChip		->	handle to the chip
		::	MasterClock	->	Masterclock frequency(Hz)
		::	SymbolRate	->	Symbol Rate(Symbol/s)
		::
--PARAMS OUT::	NONE
--RETURN	::	None
*****************************************************/
void FE_STV0913_SetSymbolRate(STCHIP_Handle_t hChip, U32 MasterClock,
			      U32 SymbolRate)
{
	U32 symb;

	if (SymbolRate > 60000000) {	/*SR > 60Msps */
		/* Formulat :init_symbol_rate_regs = SR * 2^16/MasterClock */
		symb = SymbolRate << 4;
		symb /= (MasterClock >> 12);

		/*
		   equivalent to :
		   symb = (SymbolRate/2000)*65536;
		   symb /= (MasterClock/2000);
		 */
	} else if (SymbolRate > 6000000) {
		/* Formulat :init_symbol_rate_regs = SR * 2^16/MasterClock */
		symb = SymbolRate << 6;
		symb /= (MasterClock >> 10);

		/*
		   equivalent to :
		   symb = (SymbolRate/1000)*65536;
		   symb /= (MasterClock/1000); */
	} else {
		/* Formulat :init_symbol_rate_regs = SR * 2^16/MasterClock */
		symb = SymbolRate << 9;
		symb /= (MasterClock >> 7);

		/*
		   equivalent to :
		   symb = (SymbolRate/100)*65536;
		   symb /= (MasterClock/100);
		 */
	}
	/* update the MSB */
	ChipSetOneRegister(hChip, RSTV0913_SFRINIT1, (symb >> 8) & 0x7F);
	/* update the LSB */
	ChipSetOneRegister(hChip, RSTV0913_SFRINIT0, (symb & 0xFF));

}

/*****************************************************
--FUNCTION	::	GetStandard
--ACTION	::	Return the current standrad(DVBS1, DSS or DVBS2
--PARAMS IN	::	hChip		->	handle to the chip
			::
--PARAMS OUT::	NONE
--RETURN	::	standard(DVBS1, DVBS2 or DSS
*****************************************************/
FE_STV0913_TrackingStandard_t FE_STV0913_GetStandard(STCHIP_Handle_t hChip)
{
	FE_STV0913_TrackingStandard_t foundStandard;
	S32 state;

	state = ChipGetField(hChip, FSTV0913_HEADER_MODE);

	if (state == 2)
		foundStandard = FE_SAT_DVBS2_STANDARD;	/*Find DVBS2 */

	else if (state == 3) {	/*        The demod Find DVBS1/DSS */
		if (ChipGetField(hChip, FSTV0913_DSS_DVB) == 1)
			foundStandard = FE_SAT_DSS_STANDARD;/*Vit Find DSS */
		else
			foundStandard = FE_SAT_DVBS1_STANDARD;/*Vit Find DVBS1*/
	} else
		foundStandard = FE_SAT_UNKNOWN_STANDARD;/* find nothing */
	return foundStandard;
}

/*****************************************************
--FUNCTION	::	CarrierGetQuality
--ACTION	::	Return the carrier to noise of the current carrier
--PARAMS IN	::	hChip	->	handle to the chip
		::	lookup	->	LUT for CNR level estimation.
--PARAMS OUT::	NONE
--RETURN	::	C/N of the carrier, 0 if no carrier
--***************************************************/
S32 FE_STV0913_CarrierGetQuality(STCHIP_Handle_t hChip,
				 FE_STV0913_LOOKUP_t *lookup)
{
	S32 c_n = -100,
	    regval,
	    Imin, Imax, i, lockFlagField, noiseField1, noiseField0, noiseReg;

	lockFlagField = FSTV0913_LOCK_DEFINITIF;
	if (FE_STV0913_GetStandard(hChip) == FE_SAT_DVBS2_STANDARD) {
		/*If DVBS2 use PLH normilized noise indicators */
		noiseField1 = FSTV0913_NOSPLHT_NORMED1;
		noiseField0 = FSTV0913_NOSPLHT_NORMED0;
		noiseReg = RSTV0913_NNOSPLHT1;
	} else {
		/*if not DVBS2 use symbol normalized noise indicators */
		noiseField1 = FSTV0913_NOSDATAT_NORMED1;
		noiseField0 = FSTV0913_NOSDATAT_NORMED0;
		noiseReg = RSTV0913_NNOSDATAT1;
	}

	if (ChipGetField(hChip, lockFlagField)) {
		if ((lookup != NULL) && lookup->size) {
			regval = 0;
			/* ChipWaitOrAbort(hChip, 5); */
			for (i = 0; i < /*16 */ 8; i++) {
				ChipGetRegisters(hChip, noiseReg, 2);
				regval +=
				    MAKEWORD(ChipGetFieldImage
					     (hChip, noiseField1),
					     ChipGetFieldImage(hChip,
							       noiseField0));
				ChipWaitOrAbort(hChip, 1);
			}
			regval /= /*16 */ 8;

			Imin = 0;
			Imax = lookup->size - 1;

			if (INRANGE
			    (lookup->table[Imin].regval, regval,
			     lookup->table[Imax].regval)) {
				while ((Imax - Imin) > 1) {
					i = (Imax + Imin) >> 1;
					/*equivalent to i = (Imax+Imin)/2; */
					if (INRANGE
					    (lookup->table[Imin].regval, regval,
					     lookup->table[i].regval))
						Imax = i;
					else
						Imin = i;
				}

				c_n = ((regval - lookup->table[Imin].regval)
				       * (lookup->table[Imax].realval -
					  lookup->table[Imin].realval)
				       / (lookup->table[Imax].regval -
					  lookup->table[Imin].regval))
				    + lookup->table[Imin].realval;
			} else if (regval < lookup->table[Imin].regval)
				c_n = 1000;
		}
	}

	return c_n;
}

/*****************************************************
--FUNCTION	::	GetBer
--ACTION	::	Return the Viterbi BER if DVBS1/DSS or the PER if DVBS2
--PARAMS IN	::	hChip	->	handle to the chip
--PARAMS OUT::	NONE
--RETURN	::	ber/per scalled to 1e7
--***************************************************/
static U32 FE_STV0913_GetBer(STCHIP_Handle_t hChip)
{
	U32 ber = 10000000, i;
	S32 demodState;

	demodState = ChipGetField(hChip, FSTV0913_HEADER_MODE);

	switch (demodState) {
	case FE_SAT_SEARCH:
	case FE_SAT_PLH_DETECTED:
	default:
		ber = 10000000;	/*demod Not locked ber = 1 */
		break;

	case FE_SAT_DVBS_FOUND:

		ber = 0;
		/* Average 5 ber values */
		for (i = 0; i < 5; i++) {
			ChipWaitOrAbort(hChip, 5);
			ber +=
			    FE_STV0913_GetErrorCount(hChip,
						     FE_STV0913_COUNTER1);
		}

		ber /= 5;

		if (ChipGetField(hChip, FSTV0913_PRFVIT)) {/*Check for carrier*/
			/* Error Rate */
			/*  theses two lines => ber = ber * 10^7/2^20 */
			ber *= 9766;
			ber = ber >> 13;
		}
		break;

	case FE_SAT_DVBS2_FOUND:

		ber = 0;
		/* Average 5 ber values */
		for (i = 0; i < 5; i++) {
			ChipWaitOrAbort(hChip, 5);
			ber +=
			    FE_STV0913_GetErrorCount(hChip,
						     FE_STV0913_COUNTER1);
		}
		ber /= 5;
		/* Check for S2 FEC Lock   */
		if (ChipGetField(hChip, FSTV0913_PKTDELIN_LOCK)) {
			/* theses two lines => PER = ber * 10^7/2^23 */
			ber *= 9766;	/*      ber = ber * 10^7/2^10 */
			ber = ber >> 13;
		}
		break;
	}
	return ber;
}

static FE_STV0913_Rate_t FE_STV0913_GetViterbiPunctureRate(STCHIP_Handle_t
							   hChip)
{
	S32 rateField;
	FE_STV0913_Rate_t punctureRate;

	rateField = ChipGetField(hChip, FSTV0913_VIT_CURPUN);
	switch (rateField) {
	case 13:
		punctureRate = FE_SAT_PR_1_2;
		break;

	case 18:
		punctureRate = FE_SAT_PR_2_3;
		break;

	case 21:
		punctureRate = FE_SAT_PR_3_4;
		break;

	case 24:
		punctureRate = FE_SAT_PR_5_6;
		break;

	case 25:
		punctureRate = FE_SAT_PR_6_7;
		break;

	case 26:
		punctureRate = FE_SAT_PR_7_8;
		break;

	default:
		punctureRate = FE_SAT_PR_UNKNOWN;
		break;
	}

	return punctureRate;
}

static void FE_STV0913_GetLockTimeout(S32 *DemodTimeout, S32 *FecTimeout,
				      S32 SymbolRate,
				      FE_STV0913_SearchAlgo_t Algo)
{
	switch (Algo) {
	case FE_SAT_BLIND_SEARCH:

		if (SymbolRate <= 1500000) {	/*10Msps < SR <= 15Msps */
			(*DemodTimeout) = 1000;	/*1500 */
			(*FecTimeout) = 400;	/*400 */
		} else if (SymbolRate <= 5000000) { /*1.5Msps < SR <= 5Msps */
			(*DemodTimeout) = 1000;
			(*FecTimeout) = 300;
		} else if (SymbolRate <= 30000000) { /*5Msps < SR <= 30Msps */
			(*DemodTimeout) = 700;
			(*FecTimeout) = 300;
		} else if (SymbolRate <= 45000000) { /*30Msps < SR <= 45Msps */
			(*DemodTimeout) = 400;
			(*FecTimeout) = 200;
		} else {	/*SR > 45Msps */

			(*DemodTimeout) = 200;
			(*FecTimeout) = 100;
		}

		break;

	case FE_SAT_COLD_START:
	case FE_SAT_WARM_START:
	default:
		if (SymbolRate <= 1000000) {	/*SR <= 1Msps */
			(*DemodTimeout) = 3000;
			(*FecTimeout) = 2000;	/*1700 */
		} else if (SymbolRate <= 2000000) { /*1Msps < SR <= 2Msps */
			(*DemodTimeout) = 2500;
			(*FecTimeout) = 1300;	/*1100 */
		} else if (SymbolRate <= 5000000) { /*2Msps < SR <= 5Msps */
			(*DemodTimeout) = 1000;
			(*FecTimeout) = 650;	/* 550 */
		} else if (SymbolRate <= 10000000) { /*5Msps < SR <= 10Msps */
			(*DemodTimeout) = 700;
			(*FecTimeout) = 350;	/*250 */
		} else if (SymbolRate <= 20000000) { /*10Msps < SR <= 20Msps */
			(*DemodTimeout) = 400;
			(*FecTimeout) = 200;	/* 130 */
		} else {	/*SR > 20Msps */

			(*DemodTimeout) = 300;
			(*FecTimeout) = 200;	/* 150 */
		}
		break;

	}
	if (Algo == FE_SAT_WARM_START) {
		/*if warm start
		   demod timeout = coldtimeout/3
		   fec timeout = same as cold */
		(*DemodTimeout) /= 2;
	}

}

static void FE_STV0913_setVTH(STCHIP_Handle_t hChip,
			      FE_STV0913_LOOKUP_t *lookup)
{
	S32 vth = 250,		/* Min = 5, max = 250 */
	    regval, Imin, Imax, i;

#ifdef HOST_PC
#ifndef NO_GUI
	double lockTime;
	char message[100];
	lockTime = Timer();
#endif
#endif

	if ((lookup != NULL) && lookup->size) {

		ChipGetRegisters(hChip, RSTV0913_NNOSDATAT1, 2);
		regval =
		    MAKEWORD(ChipGetFieldImage
			     (hChip, FSTV0913_NOSDATAT_NORMED1),
			     ChipGetFieldImage(hChip,
					       FSTV0913_NOSDATAT_NORMED0));

		Imin = 0;
		Imax = lookup->size - 1;

		if (INRANGE
		    (lookup->table[Imin].regval, regval,
		     lookup->table[Imax].regval)) {
			while ((Imax - Imin) > 1) {
				i = (Imax + Imin) >> 1;
				/*equivalent to i = (Imax+Imin)/2; */
				if (INRANGE
				    (lookup->table[Imin].regval, regval,
				     lookup->table[i].regval))
					Imax = i;
				else
					Imin = i;
			}

			vth = ((regval - lookup->table[Imin].regval)
			       * (lookup->table[Imax].realval -
				  lookup->table[Imin].realval)
			       / (lookup->table[Imax].regval -
				  lookup->table[Imin].regval))
			    + lookup->table[Imin].realval;
		} else if (regval < lookup->table[Imin].regval)
			vth = 5;
	}

	/* Write VTH registers */

	if (ChipGetFieldImage(hChip, FSTV0913_VTH12) > vth)
		ChipSetFieldImage(hChip, FSTV0913_VTH12, vth);
	if (ChipGetFieldImage(hChip, FSTV0913_VTH23) > vth)
		ChipSetFieldImage(hChip, FSTV0913_VTH23, vth);
	if (ChipGetFieldImage(hChip, FSTV0913_VTH34) > vth)
		ChipSetFieldImage(hChip, FSTV0913_VTH34, vth);
	if (ChipGetFieldImage(hChip, FSTV0913_VTH56) > vth)
		ChipSetFieldImage(hChip, FSTV0913_VTH56, vth);
	if (ChipGetFieldImage(hChip, FSTV0913_VTH67) > vth)
		ChipSetFieldImage(hChip, FSTV0913_VTH67, vth);
	if (ChipGetFieldImage(hChip, FSTV0913_VTH78) > vth)
		ChipSetFieldImage(hChip, FSTV0913_VTH78, vth);

	ChipSetRegisters(hChip, RSTV0913_VTH12, 6);

#if 0

#ifdef HOST_PC
#ifndef NO_GUI
	lockTime = Timer() - lockTime;
	Fmt(message, "VTH setting time = %f ms", lockTime * 1000.0);
	ReportInsertMessage(message);
	Fmt(message, "CNR = %d	VTH = %d", regval, vth);
	ReportInsertMessage(message);

#endif
#endif

#endif

}

static BOOL FE_STV0913_GetDemodLock(STCHIP_Handle_t hChip, S32 TimeOut)
{
	S32 timer = 0, lock = 0;
	U32 symbolRate;
	U32 TimeOut_SymbRate, SRate_1MSymb_Sec;

	FE_STV0913_SEARCHSTATE_t demodState;

#ifdef HOST_PC
#ifndef NO_GUI
	double lockTime;
	char message[100];
	lockTime = Timer();

#endif
#endif

	TimeOut_SymbRate = TimeOut;
	SRate_1MSymb_Sec = 0x01e5;

	while ((timer < TimeOut_SymbRate) && (lock == 0)) {

		/*Calcul du TimeOut reel en fonction du Symbolrate */
		symbolRate =
		    (ChipGetField(hChip, FSTV0913_SYMB_FREQ3) << 8) +
		    (ChipGetField(hChip, FSTV0913_SYMB_FREQ2));

		if (TimeOut < DmdLock_TIMEOUT_LIMIT)
			TimeOut_SymbRate = TimeOut;
		else {
			/*Securite anti division par 0 */
			if (symbolRate < SRate_1MSymb_Sec)
				symbolRate = SRate_1MSymb_Sec;
			else if (symbolRate > (5 * SRate_1MSymb_Sec))
				symbolRate = 5 * SRate_1MSymb_Sec;

			TimeOut_SymbRate =
			    TimeOut / (symbolRate / SRate_1MSymb_Sec);

			/*Securite anti resultat aberrant */
			if (TimeOut_SymbRate < DmdLock_TIMEOUT_LIMIT)
				TimeOut_SymbRate = DmdLock_TIMEOUT_LIMIT;
			else if (TimeOut_SymbRate > TimeOut)
				TimeOut_SymbRate = TimeOut;

			/*new timeout is between 200 ms & original TimeOut */
		}
		demodState = ChipGetField(hChip, FSTV0913_HEADER_MODE);

		switch (demodState) {
		case FE_SAT_SEARCH:
		case FE_SAT_PLH_DETECTED:	/* no signal */
		default:
			lock = 0;
			break;

		case FE_SAT_DVBS2_FOUND:	/*found a DVBS2 signal */
		case FE_SAT_DVBS_FOUND:
			lock = ChipGetField(hChip, FSTV0913_LOCK_DEFINITIF);
			break;
		}

		/* Monitor CNR and Set VTH accordingly when DVB - S1(or DSS)
						search standard is selected */
		if (ChipGetFieldImage(hChip, FSTV0913_DVBS1_ENABLE))
			FE_STV0913_setVTH(hChip, &FE_STV0913_VTH_Lookup);

		if (lock == 0)
			ChipWaitOrAbort(hChip, 10);
		timer += 10;
	}

#ifdef HOST_PC
#ifndef NO_GUI
	lockTime = Timer() - lockTime;
	Fmt(message, "Demod Lock Time = %f ms", lockTime * 1000.0);
	if (lock)
		ReportInsertMessage("DEMOD LOCK OK");
	else
		ReportInsertMessage("DEMOD LOCK FAIL");

	ReportInsertMessage(message);
#endif
#endif

	return lock;
}

static BOOL FE_STV0913_GetFECLock(STCHIP_Handle_t hChip, S32 TimeOut)
{
	S32 timer = 0, lock = 0;

	FE_STV0913_SEARCHSTATE_t demodState;
#ifdef HOST_PC
#ifndef NO_GUI
	double lockTime;
	char message[100];
	lockTime = Timer();

#endif
#endif

	demodState = ChipGetField(hChip, FSTV0913_HEADER_MODE);
	while ((timer < TimeOut) && (lock == 0)) {

		switch (demodState) {
		case FE_SAT_SEARCH:
		case FE_SAT_PLH_DETECTED:	/* no signal */
		default:
			lock = 0;
			break;

		case FE_SAT_DVBS2_FOUND:	/*found a DVBS2 signal */
			lock = ChipGetField(hChip, FSTV0913_PKTDELIN_LOCK);
			break;

		case FE_SAT_DVBS_FOUND:
			lock = ChipGetField(hChip, FSTV0913_LOCKEDVIT);
			break;
		}

		if (lock == 0) {

			ChipWaitOrAbort(hChip, 10);
			timer += 10;
		}
	}

#ifdef HOST_PC
#ifndef NO_GUI
	lockTime = Timer() - lockTime;
	Fmt(message, "FEC Lock Time = %f ms", lockTime * 1000.0);

	if (lock) {
		ReportInsertMessage("FEC LOCK OK");
		if (demodState == FE_SAT_DVBS2_FOUND)
			ReportInsertMessage("DVBS2 found");
	} else
		ReportInsertMessage("FEC LOCK FAIL");

	ReportInsertMessage(message);
#endif
#endif

	return lock;
}

/*****************************************************
--FUNCTION	::	WaitForLock
--ACTION	::	Wait until Demod+ FEC locked or timout
--PARAMS IN	::	hChip	->	handle to the chip
			::	TimeOut	->	Time out in ms
--PARAMS OUT::	NONE
--RETURN	::	Lock status true or false
--***************************************************/
BOOL FE_STV0913_WaitForLock(STCHIP_Handle_t hChip, S32 DemodTimeOut,
			    S32 FecTimeOut, U32 Satellite_Scan)
{

	S32 lock = 0, timer = 0;

#ifdef HOST_PC
#ifndef NO_GUI
	double lockTime;
	char message[100];
	lockTime = Timer();

#endif
#endif

	lock = FE_STV0913_GetDemodLock(hChip, DemodTimeOut);
	if (lock)
		lock = lock && FE_STV0913_GetFECLock(hChip, FecTimeOut);

	/* LINEOK check is not performed during Satellite Scan */
	if (Satellite_Scan == FALSE) {
		if (lock) {
			lock = 0;
			while ((timer < FecTimeOut) && (lock == 0)) {
				/*Check the stream merger Lock(good packet at
				the output) */
				lock = ChipGetField(hChip,
						FSTV0913_TSFIFO_LINEOK);
				ChipWaitOrAbort(hChip, 1);
				timer++;
			}
		}
	}
#ifdef HOST_PC
#ifndef NO_GUI
	/* lockTime = Timer() - lockTime;
	   Fmt(message, "Total Lock Time = %f ms", lockTime*1000.0); */
	if (lock)
		ReportInsertMessage("LOCK OK");
	else
		ReportInsertMessage("LOCK FAIL");

	/* ReportInsertMessage(message); */

#endif
#endif
/*printf("\n<%s,%d > lock =%d",__func__,__LINE__, lock);*/
	if (lock)
		return TRUE;
	else
		return FALSE;
}

static void FE_STV0913_SetViterbiStandard(STCHIP_Handle_t hChip,
					  FE_STV0913_SearchStandard_t Standard,
					  FE_STV0913_Rate_t PunctureRate)
{
	switch (Standard) {
	case FE_SAT_AUTO_SEARCH:
		/* Enable only DVBS1,
		DSS search must only on demand by giving Std DSS*/
		ChipSetOneRegister(hChip, RSTV0913_FECM, 0x00);
		/* Enable all PR exept 6/7 */
		ChipSetOneRegister(hChip, RSTV0913_PRVIT, 0x2F);
		break;

	case FE_SAT_SEARCH_DVBS1:
		ChipSetOneRegister(hChip, RSTV0913_FECM, 0x00);/* Disable DSS*/
		switch (PunctureRate) {
		case FE_SAT_PR_UNKNOWN:
		default:
			/*Enable All PR */
			ChipSetOneRegister(hChip, RSTV0913_PRVIT, 0x2F);
			break;

		case FE_SAT_PR_1_2:	/*1/2 only */
			ChipSetOneRegister(hChip, RSTV0913_PRVIT, 0x01);
			break;

		case FE_SAT_PR_2_3:	/* 2/3 PR only */
			ChipSetOneRegister(hChip, RSTV0913_PRVIT, 0x02);
			break;

		case FE_SAT_PR_3_4:	/* 3/4 PR only */
			ChipSetOneRegister(hChip, RSTV0913_PRVIT, 0x04);
			break;

		case FE_SAT_PR_5_6:	/* 5/6 PR only */
			ChipSetOneRegister(hChip, RSTV0913_PRVIT, 0x08);
			break;

		case FE_SAT_PR_7_8:	/* 7/8 PR only */
			ChipSetOneRegister(hChip, RSTV0913_PRVIT, 0x20);
			break;
		}

		break;

	case FE_SAT_SEARCH_DSS:	/* Disable DVBS1 */
		ChipSetOneRegister(hChip, RSTV0913_FECM, 0x80);
		switch (PunctureRate) {
		case FE_SAT_PR_UNKNOWN:
		default:
			/* Enable 1/2, 2/3 and 6/7 PR */
			ChipSetOneRegister(hChip, RSTV0913_PRVIT, 0x13);
			break;

		case FE_SAT_PR_1_2:	/* 1/2 PR only */
			ChipSetOneRegister(hChip, RSTV0913_PRVIT, 0x01);
			break;

		case FE_SAT_PR_2_3:	/* 2/3 PR only */
			ChipSetOneRegister(hChip, RSTV0913_PRVIT, 0x02);
			break;

		case FE_SAT_PR_6_7:	/* 7/8 PR only */
			ChipSetOneRegister(hChip, RSTV0913_PRVIT, 0x10);
			break;
		}

		break;

	default:
		break;
	}
}

static S32 FE_STV0913_CheckVGLNAPresent(STCHIP_Handle_t hChipVGLNA)
{
	S32 isVGLNA = 0;

#if !defined(HOST_PC) || defined(NO_GUI)
	if (hChipVGLNA != NULL) {
		if (ChipCheckAck(hChipVGLNA) == CHIPERR_I2C_NO_ACK)
			isVGLNA = 0;
		else
			isVGLNA = 1;
	}
#else
	if (ChipCheckAck(hChipVGLNA) != 0)
		isVGLNA = 0;
	else
		isVGLNA = 1;
#endif

	return isVGLNA;
}

S32 FE_STV0913_GetPADC(STCHIP_Handle_t hChip, FE_STV0913_LOOKUP_t *lookup)
{

	S32 Padc = 100, regval, Imin, Imax, i;

	if ((lookup != NULL) && lookup->size) {
		regval = 0;
		/* ChipWaitOrAbort(hChip, 5); */
		for (i = 0; i < 5; i++) {
			ChipGetRegisters(hChip, RSTV0913_POWERI, 2);
			regval +=
			    ChipGetFieldImage(hChip,
					      FSTV0913_POWER_I) *
			    ChipGetFieldImage(hChip, FSTV0913_POWER_I)
			    + ChipGetFieldImage(hChip,
						FSTV0913_POWER_Q) *
			    ChipGetFieldImage(hChip, FSTV0913_POWER_Q);
			ChipWaitOrAbort(hChip, 1);
		}
		regval /= 5;

		Imin = 0;
		Imax = lookup->size - 1;

		if (INRANGE
		    (lookup->table[Imin].regval, regval,
		     lookup->table[Imax].regval)) {
			while ((Imax - Imin) > 1) {
				i = (Imax + Imin) >> 1;
				/*equivalent to i = (Imax+Imin)/2; */
				if (INRANGE
				    (lookup->table[Imin].regval, regval,
				     lookup->table[i].regval))
					Imax = i;
				else
					Imin = i;
			}

			Padc = ((regval - lookup->table[Imin].regval)
				* (lookup->table[Imax].realval -
				   lookup->table[Imin].realval)
				/ (lookup->table[Imax].regval -
				   lookup->table[Imin].regval))
			    + lookup->table[Imin].realval;
		} else if (regval < lookup->table[Imin].regval)
			Padc = -2100;
	}

	return Padc;
}

S32 FE_STV0913_GetRFLevel(FE_STV0913_InternalParams_t *pParams)
{
	S32 Power = 0, vglnagain = 0, BBgain = 0, agcGain, Padc;

	ChipGetRegisters(pParams->hDemod, RSTV0913_AGCIQIN1, 2);
	agcGain =
	    MAKEWORD(ChipGetFieldImage(pParams->hDemod, FSTV0913_AGCIQ_VALUE1),
		     ChipGetFieldImage(pParams->hDemod, FSTV0913_AGCIQ_VALUE0));
	BBgain = FE_Sat_TunerGetGain(pParams->hTuner);
	Power = FE_Sat_TunerGetRFGain(pParams->hTuner, agcGain, BBgain);

	/* Pin(dBm) = Padc(dBm) - RFGain(dB) */
	/* Padc(dBm) = 10*log10((Vadc^2/75ohm)/10^-3) */
	/* Vadc(dBm) = (VppADC/256)*(10/12)*1/(2*sqrt(2))*(PowerI+powerQ)/2  */

	Padc = FE_STV0913_GetPADC(pParams->hDemod, &FE_STV0913_PADC_Lookup);
	/*913 ADC is 1.5Vpp=>add 3.52dBm to power ADC(based on 1Vpp ADC range)*/
	Padc += 352;
	Power = Padc - Power;
	Power /= 100;

	if (FE_STV0913_CheckVGLNAPresent(pParams->hVGLNA) != 0) {
		if (STVVGLNA_GetGain(pParams->hVGLNA, &vglnagain) ==
		    CHIPERR_NO_ERROR)
			Power += vglnagain - 15;
	}

	return Power;
}

/*****************************************************
--FUNCTION	:: SetSearchStandard
--ACTION	:: Set the Search standard(Auto, DVBS2 only or DVBS1/DSS only)
--PARAMS IN	:: hChip	->	handle to the chip
		:: Standard	->	Search standard
--PARAMS OUT::	NONE
--RETURN	:: none
--***************************************************/
static void FE_STV0913_SetSearchStandard(FE_STV0913_InternalParams_t *pParams)
{
	switch (pParams->DemodSearchStandard) {
	case FE_SAT_SEARCH_DVBS1:
		/*If DVB - S only disable DVBS2 search */
		ChipSetFieldImage(pParams->hDemod, FSTV0913_DVBS1_ENABLE, 1);
		ChipSetFieldImage(pParams->hDemod, FSTV0913_DVBS2_ENABLE, 0);
		ChipSetRegisters(pParams->hDemod, RSTV0913_DMDCFGMD, 1);

		FE_STV0913_SetViterbiStandard(pParams->hDemod,
					      pParams->DemodSearchStandard,
					      pParams->DemodPunctureRate);
		/* Enable Super FEC */
		ChipSetOneRegister(pParams->hDemod, RSTV0913_SFDLYSET2, 0);

		break;

	case FE_SAT_SEARCH_DSS:
		/*If DSS only disable DVBS2 search */
		ChipSetFieldImage(pParams->hDemod, FSTV0913_DVBS1_ENABLE, 1);
		ChipSetFieldImage(pParams->hDemod, FSTV0913_DVBS2_ENABLE, 0);
		ChipSetRegisters(pParams->hDemod, RSTV0913_DMDCFGMD, 1);

		FE_STV0913_SetViterbiStandard(pParams->hDemod,
					      pParams->DemodSearchStandard,
					      pParams->DemodPunctureRate);
		/* Stop Super FEC */
		ChipSetOneRegister(pParams->hDemod, RSTV0913_SFDLYSET2, 2);

		break;

	case FE_SAT_SEARCH_DVBS2:
		/*If DVBS2 only activate the DVBS2 search & stop the VITERBI*/
		ChipSetFieldImage(pParams->hDemod, FSTV0913_DVBS1_ENABLE, 0);
		ChipSetFieldImage(pParams->hDemod, FSTV0913_DVBS2_ENABLE, 1);
		ChipSetRegisters(pParams->hDemod, RSTV0913_DMDCFGMD, 1);

		break;

	case FE_SAT_AUTO_SEARCH:
	default:
		/*If automatic enable both DVBS1/DSS and DVBS2 search */
		ChipSetFieldImage(pParams->hDemod, FSTV0913_DVBS1_ENABLE, 1);
		ChipSetFieldImage(pParams->hDemod, FSTV0913_DVBS2_ENABLE, 1);
		ChipSetRegisters(pParams->hDemod, RSTV0913_DMDCFGMD, 1);

		FE_STV0913_SetViterbiStandard(pParams->hDemod,
					      pParams->DemodSearchStandard,
					      pParams->DemodPunctureRate);
		/* Enable Super FEC */
		ChipSetOneRegister(pParams->hDemod, RSTV0913_SFDLYSET2, 0);

		break;
	}

}

/*****************************************************
--FUNCTION	::	StartSearch
--ACTION	::	Trig the Demod to start a new search
--PARAMS IN	::	pParams	->Pointer FE_STV0913_InternalParams_t structer
--PARAMS OUT::	NONE
--RETURN	::	none
--***************************************************/
static void FE_STV0913_StartSearch(FE_STV0913_InternalParams_t *pParams)
{

	U32 freq;
	S16 freq_s16;

	ChipSetField(pParams->hDemod, FSTV0913_FRESSYM1, 1);	/* Reset CAR3 */
	ChipSetField(pParams->hDemod, FSTV0913_FRESSYM1, 0);
	/*Reset the Demod */
	ChipSetOneRegister(pParams->hDemod, RSTV0913_DMDISTATE, 0x1F);

	if (pParams->DemodSearchAlgo == FE_SAT_WARM_START) {
		/* if warm start CFR min = -1MHz, CFR max = 1MHz */
		freq = 1000 << 16;
		freq /= (pParams->MasterClock / 1000);
		freq_s16 = (S16) freq;
	} else if (pParams->DemodSearchAlgo == FE_SAT_COLD_START) {
		/* CFR min = - (SearchRange/2 + margin)
		   CFR max = +(SearchRange/2 + margin)
		   (80KHz margin if SR <= 5Msps else margin = 600KHz) */

		if (pParams->DemodSymbolRate <= 5000000)
			freq = (pParams->DemodSearchRange / 2000) + 80;
		else
			freq = (pParams->DemodSearchRange / 2000) + 1600;
			/* Increase search range to improve cold start success
			rate on tilted signal  + frequency offset */

		freq = freq << 16;
		freq /= (pParams->MasterClock / 1000);
		freq_s16 = (S16) freq;
	} else {
		freq = (pParams->DemodSearchRange / 2000);
		freq = freq << 16;
		freq /= (pParams->MasterClock / 1000);
		freq_s16 = (S16) freq;
	}

	if (pParams->hDemod->ChipID == 0x50) {
		/*Super FEC limitation  WA, applicable only for cut1 */
		if (pParams->DemodSearchStandard == FE_SAT_SEARCH_DVBS1
		    || pParams->DemodSearchStandard == FE_SAT_AUTO_SEARCH) {
			if (!FE_SAT_BLIND_SEARCH) {
				if (pParams->DemodSymbolRate >= 53500000)
					/* Stop superFEC */
					ChipSetOneRegister(pParams->hDemod,
					RSTV0913_SFDLYSET2, 0x02);
			}
		}
	}
	/* search range definition */
	ChipSetOneRegister(pParams->hDemod, RSTV0913_CARCFG, 0x46);
	ChipSetFieldImage(pParams->hDemod, FSTV0913_CFR_UP1, MSB(freq_s16));
	ChipSetFieldImage(pParams->hDemod, FSTV0913_CFR_UP0, LSB(freq_s16));
	ChipSetRegisters(pParams->hDemod, RSTV0913_CFRUP1, 2);
	freq_s16 *= (-1);
	ChipSetFieldImage(pParams->hDemod, FSTV0913_CFR_LOW1, MSB(freq_s16));
	ChipSetFieldImage(pParams->hDemod, FSTV0913_CFR_LOW0, LSB(freq_s16));
	ChipSetRegisters(pParams->hDemod, RSTV0913_CFRLOW1, 2);

	ChipSetFieldImage(pParams->hDemod, FSTV0913_CFR_INIT1, 0);
	ChipSetFieldImage(pParams->hDemod, FSTV0913_CFR_INIT0, 0);
		/*init the demod frequency offset to 0 */
	ChipSetRegisters(pParams->hDemod, RSTV0913_CFRINIT1, 2);

	switch (pParams->DemodSearchAlgo) {
	/*The symbol rate and the exact carrier Frequency are known */
	case FE_SAT_WARM_START:
		/*Reset the Demod */
		ChipSetOneRegister(pParams->hDemod, RSTV0913_DMDISTATE, 0x1F);
		/*Trig an acquisition(start the search) */
		ChipSetOneRegister(pParams->hDemod, RSTV0913_DMDISTATE, 0x18);
		break;

	case FE_SAT_COLD_START:	/*The symbol rate is known */
		/*Reset the Demod */
		ChipSetOneRegister(pParams->hDemod, RSTV0913_DMDISTATE, 0x1F);
		/*Trig an acquisition(start the search) */
		ChipSetOneRegister(pParams->hDemod, RSTV0913_DMDISTATE, 0x15);
		break;

	case FE_SAT_BLIND_SEARCH:
		/*Reset the Demod */
		ChipSetOneRegister(pParams->hDemod, RSTV0913_DMDISTATE, 0x1F);
		/*Trig an acquisition(start the search) */
		ChipSetOneRegister(pParams->hDemod, RSTV0913_DMDISTATE, 0x00);
		break;

	default:
		/*Nothing to do in case of blind search,
		blind  is handled by "FE_STV0913_BlindSearchAlgo" function */
		break;
	}
}

/*! \fn U8 FE_STV0913_GetOptimCarrierLoop(S32 SymbolRate,
					FE_STV0913_ModCod_t ModCode, S32 Pilots)
\brief This function sets the optimized alpha coefficient value for tracking.
*/
U8 FE_STV0913_GetOptimCarrierLoop(S32 SymbolRate, FE_STV0913_ModCod_t ModCode,
								S32 Pilots)
{
	U8 aclcValue = 0x29;
	U32 i = 0;

	/* Find the index parameters for the Modulation */
	while ((i < NB_SAT_MODCOD)
	       && (ModCode != FE_STV0913_S2CarLoop[i].ModCode))
		i++;

	if (i < NB_SAT_MODCOD) {
		if (Pilots) {
			if (SymbolRate <= 3000000)
				aclcValue =
				    FE_STV0913_S2CarLoop[i].CarLoopPilotsOn_2;
			else if (SymbolRate <= 7000000)
				aclcValue =
				    FE_STV0913_S2CarLoop[i].CarLoopPilotsOn_5;
			else if (SymbolRate <= 15000000)
				aclcValue =
				    FE_STV0913_S2CarLoop[i].CarLoopPilotsOn_10;
			else if (SymbolRate <= 25000000)
				aclcValue =
				    FE_STV0913_S2CarLoop[i].CarLoopPilotsOn_20;
			else
				aclcValue =
				    FE_STV0913_S2CarLoop[i].CarLoopPilotsOn_30;
		} else {
			if (SymbolRate <= 3000000)
				aclcValue =
				    FE_STV0913_S2CarLoop[i].CarLoopPilotsOff_2;
			else if (SymbolRate <= 7000000)
				aclcValue =
				    FE_STV0913_S2CarLoop[i].CarLoopPilotsOff_5;
			else if (SymbolRate <= 15000000)
				aclcValue =
				    FE_STV0913_S2CarLoop[i].CarLoopPilotsOff_10;
			else if (SymbolRate <= 25000000)
				aclcValue =
				    FE_STV0913_S2CarLoop[i].CarLoopPilotsOff_20;
			else
				aclcValue =
				    FE_STV0913_S2CarLoop[i].CarLoopPilotsOff_30;
		}
	} else {
		/* Modulation Unknown */
	}
	return aclcValue;
}

/*****************************************************
--FUNCTION	::	TrackingOptimization
--ACTION	::	Set Optimized parameters for tracking
--PARAMS IN	::	pParams	->Pointer FE_STV0913_InternalParams_t structer
--PARAMS OUT::	NONE
--RETURN	::	none
--***************************************************/
static void FE_STV0913_TrackingOptimization(FE_STV0913_InternalParams_t
								*pParams)
{
	S32 symbolRate, pilots, aclc;

	FE_STV0913_ModCod_t foundModcod;
	/*        Read the Symbol rate    */
	symbolRate = FE_STV0913_GetSymbolRate(pParams->hDemod,
							pParams->MasterClock);
	symbolRate += FE_STV0913_TimingGetOffset(pParams->hDemod, symbolRate);

	switch (pParams->DemodResults.Standard) {
	case FE_SAT_DVBS1_STANDARD:
		if (pParams->DemodSearchStandard == FE_SAT_AUTO_SEARCH) {
			ChipSetFieldImage(pParams->hDemod,
					  FSTV0913_DVBS1_ENABLE, 1);
			ChipSetFieldImage(pParams->hDemod,
					  FSTV0913_DVBS2_ENABLE, 0);
			ChipSetRegisters(pParams->hDemod, RSTV0913_DMDCFGMD, 1);
		}
		/*Set the rolloff to the manual value(given at initialization)*/
		ChipSetFieldImage(pParams->hDemod, FSTV0913_ROLLOFF_CONTROL,
				  pParams->RollOff);
		ChipSetFieldImage(pParams->hDemod, FSTV0913_MANUALSX_ROLLOFF,
				  1);
		ChipSetRegisters(pParams->hDemod, RSTV0913_DEMOD, 1);
		/* force to viterbi bit error */
		ChipSetOneRegister(pParams->hDemod, RSTV0913_ERRCTRL1, 0x75);

		break;

	case FE_SAT_DSS_STANDARD:
		if (pParams->DemodSearchStandard == FE_SAT_AUTO_SEARCH) {
			ChipSetFieldImage(pParams->hDemod,
					  FSTV0913_DVBS1_ENABLE, 1);
			ChipSetFieldImage(pParams->hDemod,
					  FSTV0913_DVBS2_ENABLE, 0);
			ChipSetRegisters(pParams->hDemod, RSTV0913_DMDCFGMD, 1);
		}
		/*Set the rolloff to the manual value(given at initialization)*/
		ChipSetFieldImage(pParams->hDemod, FSTV0913_ROLLOFF_CONTROL,
				pParams->RollOff);
		ChipSetFieldImage(pParams->hDemod, FSTV0913_MANUALSX_ROLLOFF,
				1);
		ChipSetRegisters(pParams->hDemod, RSTV0913_DEMOD, 1);
		/* force to viterbi bit error */
		ChipSetOneRegister(pParams->hDemod, RSTV0913_ERRCTRL1, 0x75);
		break;

	case FE_SAT_DVBS2_STANDARD:

		ChipSetFieldImage(pParams->hDemod, FSTV0913_DVBS1_ENABLE, 0);
		ChipSetFieldImage(pParams->hDemod, FSTV0913_DVBS2_ENABLE, 1);
		ChipSetRegisters(pParams->hDemod, RSTV0913_DMDCFGMD, 1);
			/* force to DVBS2 PER  */
		ChipSetOneRegister(pParams->hDemod, RSTV0913_ERRCTRL1, 0x67);
		if (pParams->DemodResults.FrameLength == FE_SAT_LONG_FRAME) {
			/*Carrier loop setting for lon frame */
			foundModcod =
			    ChipGetField(pParams->hDemod,
					 FSTV0913_DEMOD_MODCOD);
			pilots =
			    ChipGetFieldImage(pParams->hDemod,
					      FSTV0913_DEMOD_TYPE) & 0x01;

			aclc =
			    FE_STV0913_GetOptimCarrierLoop(symbolRate,
							   foundModcod, pilots);
			if (foundModcod <= FE_SAT_QPSK_910)
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0913_ACLC2S2Q, aclc);
			else if (foundModcod <= FE_SAT_8PSK_910) {
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0913_ACLC2S2Q, 0x2a);
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0913_ACLC2S28, aclc);
			}
		}
		break;

	case FE_SAT_UNKNOWN_STANDARD:
	default:
		ChipSetFieldImage(pParams->hDemod, FSTV0913_DVBS1_ENABLE, 1);
		ChipSetFieldImage(pParams->hDemod, FSTV0913_DVBS2_ENABLE, 1);
		ChipSetRegisters(pParams->hDemod, RSTV0913_DMDCFGMD, 1);
		break;
	}
}

/*****************************************************
--FUNCTION	::	GetSignalParams
--ACTION	::	Read signal caracteristics
--PARAMS IN	::	pParams	->Pointer FE_STV0913_InternalParams_t structer
--PARAMS OUT::	NONE
--RETURN	::	RANGE Ok or not
--***************************************************/
static FE_STV0913_SIGNALTYPE_t
FE_STV0913_GetSignalParams(FE_STV0913_InternalParams_t *pParams,
			   U32 Satellite_Scan)
{
	FE_STV0913_SIGNALTYPE_t range = FE_SAT_OUTOFRANGE;
	S32 offsetFreq, symbolRateOffset, i = 0;

	U8 timing;

#ifdef HOST_PC
#ifndef NO_GUI
	char message[100];
#endif
#endif

	ChipWaitOrAbort(pParams->hDemod, 5);
	if (pParams->DemodSearchAlgo == FE_SAT_BLIND_SEARCH) {
		/*if Blind search wait for symbol rate offset information
		transfered from the timing loop to the demodulator symbol rate*/
		timing = ChipGetOneRegister(pParams->hDemod, RSTV0913_TMGREG2);
		i = 0;
		while ((i <= 50) && (timing != 0) && (timing != 0xFF)) {
			timing =
			    ChipGetOneRegister(pParams->hDemod,
					       RSTV0913_TMGREG2);
			ChipWaitOrAbort(pParams->hDemod, 5);
			i += 5;
		}
	}

	pParams->DemodResults.Standard =
	    FE_STV0913_GetStandard(pParams->hDemod);

	pParams->DemodResults.Frequency =
	    FE_Sat_TunerGetFrequency(pParams->hTuner);
	offsetFreq =
	    FE_STV0913_GetCarrierFrequency(pParams->hDemod,
					   pParams->MasterClock) / 1000;
	pParams->DemodResults.Frequency += offsetFreq;

	pParams->DemodResults.SymbolRate =
	    FE_STV0913_GetSymbolRate(pParams->hDemod, pParams->MasterClock);
	symbolRateOffset =
	    FE_STV0913_TimingGetOffset(pParams->hDemod,
				       pParams->DemodResults.SymbolRate);
	pParams->DemodResults.SymbolRate += symbolRateOffset;
	pParams->DemodResults.PunctureRate =
	    FE_STV0913_GetViterbiPunctureRate(pParams->hDemod);
	pParams->DemodResults.ModCode =
	    ChipGetField(pParams->hDemod, FSTV0913_DEMOD_MODCOD);
	pParams->DemodResults.Pilots =
	    ChipGetFieldImage(pParams->hDemod, FSTV0913_DEMOD_TYPE) & 0x01;
	pParams->DemodResults.FrameLength =
	    ((U32) ChipGetFieldImage(pParams->hDemod, FSTV0913_DEMOD_TYPE)) >>
	    1;

	pParams->DemodResults.RollOff =
	    ChipGetField(pParams->hDemod, FSTV0913_ROLLOFF_STATUS);

	switch (pParams->DemodResults.Standard) {
	case FE_SAT_DVBS2_STANDARD:
		pParams->DemodResults.Spectrum =
		    ChipGetField(pParams->hDemod, FSTV0913_SPECINV_DEMOD);

		if (pParams->DemodResults.ModCode <= FE_SAT_QPSK_910)
			pParams->DemodResults.Modulation = FE_SAT_MOD_QPSK;
		else if (pParams->DemodResults.ModCode <= FE_SAT_8PSK_910)
			pParams->DemodResults.Modulation = FE_SAT_MOD_8PSK;
		else if (pParams->DemodResults.ModCode <= FE_SAT_16APSK_910)
			pParams->DemodResults.Modulation = FE_SAT_MOD_16APSK;
		else if (pParams->DemodResults.ModCode <= FE_SAT_32APSK_910)
			pParams->DemodResults.Modulation = FE_SAT_MOD_32APSK;
		else
			pParams->DemodResults.Modulation = FE_SAT_MOD_UNKNOWN;

		break;

	case FE_SAT_DVBS1_STANDARD:
	case FE_SAT_DSS_STANDARD:
	default:
		pParams->DemodResults.Modulation = FE_SAT_MOD_QPSK;
		pParams->DemodResults.Spectrum =
		    ChipGetField(pParams->hDemod, FSTV0913_IQINV);

		break;
	}

	if ((pParams->DemodSearchAlgo == FE_SAT_BLIND_SEARCH)
	    || (pParams->DemodResults.SymbolRate < 10000000)) {
		/*in case of blind search the tuner freq may has been moven,read
		the new tuner freq and cumpute the freq offset from the original
		value */
		offsetFreq =
		    pParams->DemodResults.Frequency - pParams->TunerFrequency;
		pParams->TunerFrequency =
		    FE_Sat_TunerGetFrequency(pParams->hTuner);

		if ((pParams->DemodSearchAlgo == FE_SAT_BLIND_SEARCH)
		    && (Satellite_Scan > 0)) {
			range = FE_SAT_RANGEOK;	/*No check needed */
		} else {
			if (ABS(offsetFreq) <=
			    ((pParams->DemodSearchRange / 2000) + 500))
				range = FE_SAT_RANGEOK;
			else {
				if (ABS(offsetFreq) <=
				    ((FE_STV0913_CarrierWidth
				      (pParams->DemodResults.SymbolRate,
				       pParams->DemodResults.RollOff)) / 2000))
					range = FE_SAT_RANGEOK;
				else
					range = FE_SAT_OUTOFRANGE;
			}
		}
	} else {
		if (ABS(offsetFreq) <=
		    ((pParams->DemodSearchRange / 2000) + 500))
			range = FE_SAT_RANGEOK;
		else
			range = FE_SAT_OUTOFRANGE;
	}

#ifdef HOST_PC
#ifndef NO_GUI
	if (range == FE_SAT_OUTOFRANGE) {
		Fmt(message, "offsetFreq = %d KHz\n ", offsetFreq);
		ReportInsertMessage("Out Of Range");
		ReportInsertMessage(message);
	}
#endif
#endif

	return range;
}

static BOOL FE_STV0913_GetDemodLockCold(FE_STV0913_InternalParams_t *pParams,
					S32 DemodTimeout)
{

	BOOL lock = FALSE;
	S32 nbSteps,
	    currentStep, carrierStep, direction, tunerFreq, timeout, freq;

#ifdef HOST_PC
#ifndef NO_GUI
	char message[300];
#endif
#endif

#ifdef HOST_PC
#ifndef NO_GUI
	Fmt(message, "ENTERING GetDemodLockCold TIMEOUT = %d", DemodTimeout);
	ReportInsertMessage(message);
#endif
#endif

	if (pParams->DemodSymbolRate >= 10000000) {

#ifdef HOST_PC
#ifndef NO_GUI
		Fmt(message, "SR is above 10Msps: CARFREQ is 79 default ");
		ReportInsertMessage(message);
#endif
#endif

		lock = FE_STV0913_GetDemodLock(pParams->hDemod, DemodTimeout);
		if (lock == FALSE) {

#ifdef HOST_PC
#ifndef NO_GUI
			Fmt(message,
			    "FIRST DEMOD LOCK FAIL: CARFREQ is set to 0x3B");
			ReportInsertMessage(message);
#endif
#endif

			ChipSetOneRegister(pParams->hDemod,
						RSTV0913_CARFREQ, 0x3B);
			ChipSetOneRegister(pParams->hDemod,
						RSTV0913_DMDISTATE, 0x1F);
			/*Trig an acquisition(start the search) */
			ChipSetOneRegister(pParams->hDemod,
						RSTV0913_DMDISTATE, 0x15);
			lock = FE_STV0913_GetDemodLock(pParams->hDemod,
						DemodTimeout);
		}
	} else {
		lock =
		    FE_STV0913_GetDemodLock(pParams->hDemod,
					    DemodTimeout /*/2 */);
	}
	if (lock == FALSE) {
		/* Performs zigzag on CFRINIT with AEP 0x05 */
		if (pParams->DemodSymbolRate <= 10000000)
			carrierStep = pParams->DemodSymbolRate / 4000;
		else
			carrierStep = 2500;

		timeout = (DemodTimeout * 3) / 4;

		nbSteps = ((pParams->DemodSearchRange / 1000) / carrierStep);

		if ((nbSteps % 2) != 0)
			nbSteps += 1;

		if (nbSteps <= 0)
			nbSteps = 2;
		else if (nbSteps > 12)
			nbSteps = 12;

		currentStep = 1;
		direction = 1;
		tunerFreq = 0;

		while ((currentStep <= nbSteps) && (lock == FALSE)) {
			if (direction > 0)
				tunerFreq += (currentStep * carrierStep);
			else
				tunerFreq -= (currentStep * carrierStep);
#ifdef HOST_PC
#ifndef NO_GUI
			Fmt(message, "Tuner freq = %d", (S32) tunerFreq);
			ReportInsertMessage(message);
#endif
#endif
			ChipSetOneRegister(pParams->hDemod, RSTV0913_DMDISTATE,
					   0x1C);
			/*      Formula :
			   CFRINIT_Khz = CFRINIT_Reg * MasterClock_Khz /2^16
			 */
			freq =
			    (tunerFreq * 65536) / (pParams->MasterClock / 1000);
			ChipSetFieldImage(pParams->hDemod, FSTV0913_CFR_INIT1,
					MSB(freq));
			ChipSetFieldImage(pParams->hDemod, FSTV0913_CFR_INIT0,
					LSB(freq));
			/*init the demod frequency offset to 0 */
			ChipSetRegisters(pParams->hDemod, RSTV0913_CFRINIT1, 2);

			ChipSetOneRegister(pParams->hDemod, RSTV0913_DMDISTATE,
						0x1F);
			/*use AEP 0x05 instead of 0x15 to not reset CFRINIT
			to 0 */
			ChipSetOneRegister(pParams->hDemod, RSTV0913_DMDISTATE,
						0x05);

			lock = FE_STV0913_GetDemodLock(pParams->hDemod,
						timeout);

			direction *= -1;
			currentStep++;
		}
	}
	return lock;
}

static U16 FE_STV0913_BlindCheckAGC2BandWidth(FE_STV0913_InternalParams_t
								*pParams)
{
	U32 minagc2level = 0xffff, maxagc2level = 0x0000, midagc2level,
	    agc2level, agc2ratio;
	S32 initFreq, freqStep;
	U32 tmp1, tmp2, tmp3, tmp4;
	U32 asperity = 0;
	U32 waitforfall = 0;
	U32 acculevel = 0;
	U32 div = 2;
	U32 agc2leveltab[20];

	S32 i, j, k, l, nbSteps;

#ifdef HOST_PC
#ifndef NO_GUI
	double lockTime;
	char message[300];
	lockTime = Timer();
#endif
#endif
	/*AGC2 Scan*/
	/* lockTime = Timer() ;  */
	ChipSetOneRegister(pParams->hDemod, RSTV0913_DMDISTATE, 0x1C);
	tmp2 = ChipGetOneRegister(pParams->hDemod, RSTV0913_CARCFG);
	ChipSetOneRegister(pParams->hDemod, RSTV0913_CARCFG, 0x06);

	tmp3 = ChipGetOneRegister(pParams->hDemod, RSTV0913_BCLC);
	ChipSetOneRegister(pParams->hDemod, RSTV0913_BCLC, 0x00);
	tmp4 = ChipGetOneRegister(pParams->hDemod, RSTV0913_CARFREQ);
	ChipSetOneRegister(pParams->hDemod, RSTV0913_CARFREQ, 0x00);

	ChipSetOneRegister(pParams->hDemod, RSTV0913_AGC2REF, 0x38);

	tmp1 = ChipGetOneRegister(pParams->hDemod, RSTV0913_DMDCFGMD);
	ChipSetFieldImage(pParams->hDemod, FSTV0913_DVBS1_ENABLE, 1);
	ChipSetFieldImage(pParams->hDemod, FSTV0913_DVBS2_ENABLE, 1);
		/*Enable the SR SCAN */
	ChipSetFieldImage(pParams->hDemod, FSTV0913_SCAN_ENABLE, 0);
		/*activate the carrier frequency search loop */
	ChipSetFieldImage(pParams->hDemod, FSTV0913_CFR_AUTOSCAN, 0);
	ChipSetRegisters(pParams->hDemod, RSTV0913_DMDCFGMD, 1);
	/*AGC2 bandwidth is 1/div MHz */
	FE_STV0913_SetSymbolRate(pParams->hDemod, pParams->MasterClock,
								1000000 / div);

	nbSteps = (pParams->TunerBW / 3000000) * div;
	if (nbSteps <= 0)
		nbSteps = 1;

		/* AGC2 step is 1/div MHz */
	freqStep = ((1000000 << 8) / (pParams->MasterClock >> 8)) / div;

	initFreq = 0;
	j = 0;			/* index after a rising edge is found */
	for (i = 0; i < nbSteps; i++) {
		/*Scan on the positive part of the tuner Bw */
		ChipSetOneRegister(pParams->hDemod, RSTV0913_DMDISTATE, 0x1C);
		ChipSetOneRegister(pParams->hDemod, RSTV0913_CFRINIT1,
				   (initFreq >> 8) & 0xff);
		ChipSetOneRegister(pParams->hDemod, RSTV0913_CFRINIT0,
				   initFreq & 0xff);
		ChipSetOneRegister(pParams->hDemod, RSTV0913_DMDISTATE, 0x18);

		WAIT_N_MS(5);
		agc2level = 0;

		ChipGetRegisters(pParams->hDemod, RSTV0913_AGC2I1, 2);

		agc2level =
		    (ChipGetFieldImage
		     (pParams->hDemod, FSTV0913_AGC2_INTEGRATOR1) << 8)
		    | ChipGetFieldImage(pParams->hDemod,
					FSTV0913_AGC2_INTEGRATOR0);

		if (i == 0) {
			/*agc2level_step0 = agc2level ; */
			minagc2level = agc2level;
			maxagc2level = agc2level;
			midagc2level = agc2level;
			for (k = 0; k < 5 * div; k++)
				agc2leveltab[k] = agc2level;

		} else {

			k = i % (5 * div);
			agc2leveltab[k] = agc2level;
			minagc2level = 0xffff;
			maxagc2level = 0x0000;
			acculevel = 0;

			for (l = 0; l < 5 * div; l++) {
				/* Min and max detection */

				if (agc2leveltab[l] < minagc2level)
					minagc2level = agc2leveltab[l];
				else if (agc2leveltab[l] > maxagc2level)
					maxagc2level = agc2leveltab[l];

				acculevel = acculevel + agc2leveltab[l];
			}
			midagc2level = acculevel / (5 * div);

			if (waitforfall == 0) {
				agc2ratio =
				    (maxagc2level -
				     minagc2level) * 128 / midagc2level;
			} else {
				agc2ratio =
				    (agc2level -
				     minagc2level) * 128 / midagc2level;
			}

			if (agc2ratio > 0xffff)
				agc2ratio = 0xffff;

			if ((agc2ratio > STV0913_BLIND_SEARCH_AGC2BANDWIDTH)
				&& (agc2level == minagc2level)) {/*rising edge*/

				asperity = 1;	/* The first edge is rising */
				waitforfall = 1;

				for (l = 0; l < 5 * div; l++)
					agc2leveltab[l] = agc2level;

			} else
			    if ((agc2ratio >
				 STV0913_BLIND_SEARCH_AGC2BANDWIDTH)) {
				/* Front descendant */
				if (waitforfall == 0)
					asperity = 2;/*the first edge=>falling*/
				else
					asperity = 1;

				if (j == 1) {
					/* minagc2level = previousmin; */
					/* it was a spur at previous step  */
					for (l = 0; l < 5 * div; l++)
						agc2leveltab[l] = agc2level;


					j = 0;	/* All reset */
					waitforfall = 0;
					asperity = 0;
				} else {
					break;
				}
			}

			if ((waitforfall == 1) && j == (5 * div))
				break;

			if (waitforfall == 1)
				j += 1;
		}
		initFreq = initFreq + freqStep;

	}

	ChipSetOneRegister(pParams->hDemod, RSTV0913_DMDCFGMD, tmp1);
	ChipSetOneRegister(pParams->hDemod, RSTV0913_CARCFG, tmp2);
	ChipSetOneRegister(pParams->hDemod, RSTV0913_BCLC, tmp3);
	ChipSetOneRegister(pParams->hDemod, RSTV0913_CARFREQ, tmp4);
	ChipSetOneRegister(pParams->hDemod, RSTV0913_DMDISTATE, 0x1C);
	ChipSetOneRegister(pParams->hDemod, RSTV0913_CFRINIT1, 0);
	ChipSetOneRegister(pParams->hDemod, RSTV0913_CFRINIT0, 0);
	/*ChipSetOneRegister(pParams->hDemod, RSTV0913_DMDT0M, tmp2); */

	if (asperity == 1) {
		/* rising edge followed by a constant level or a falling edge*/
		pParams->TunerIndexJump = (1000 / div) * (i - (j + 2) / 2);
	} else {
		pParams->TunerIndexJump = (1000 / div) * i;/* falling edge */
	}
	return (U16) asperity;
}

static BOOL FE_STV0913_BlindSearchAlgo(FE_STV0913_InternalParams_t *pParams,
				       S32 demodTimeout, U32 Satellite_Scan)
{

	BOOL lock = TRUE;
#ifdef HOST_PC
#ifndef NO_GUI
	double lockTime;
	char message[100];
	lockTime = Timer();
#endif
#endif

	/* lockTime = Timer() ; */
	if (Satellite_Scan == 0) {
		pParams->DemodSearchRange = 2 * 36000000;
	} else {
		pParams->DemodSearchRange =
		    24000000 + pParams->TunerIndexJump * 1000;
		if (pParams->DemodSearchRange > 40000000)
			pParams->DemodSearchRange = 40000000;

	}
	/*Demod Stop*/
	ChipSetOneRegister(pParams->hDemod, RSTV0913_DMDISTATE, 0x5C);
	FE_STV0913_SetSymbolRate(pParams->hDemod, pParams->MasterClock,
				 pParams->DemodSymbolRate);
	/* open the ReedSolomon to viterbi feedback until demod lock */
	ChipSetField(pParams->hDemod, FSTV0913_DIS_RSFLOCK, 1);
	/* open Viterbi feedback until demod lock */
	ChipSetField(pParams->hDemod, FSTV0913_DIS_VITLOCK, 1);

	FE_STV0913_StartSearch(pParams);
	lock = FE_STV0913_GetDemodLock(pParams->hDemod, demodTimeout);

	return lock;
}

/*****************************************************
--FUNCTION	::	FE_STV0913_GetSignalInfoLite
--ACTION	::	Return C/N only
--PARAMS IN	::	Handle	==>	Front End Handle
--PARAMS OUT::	pInfo	==> Informations(BER, C/N, power ...)
--RETURN	::	Error(if any)
--***************************************************/
FE_STV0913_Error_t FE_STV0913_GetSignalInfoLite(FE_STV0913_Handle_t Handle,
						FE_STV0913_SignalInfo_t *pInfo)
{
	FE_STV0913_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0913_InternalParams_t *pParams = NULL;

	pParams = (FE_STV0913_InternalParams_t *) Handle;

	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;

	if ((pParams->hDemod->Error) || (pParams->hTuner->Error))
		return FE_LLA_I2C_ERROR;

	pInfo->Standard = FE_STV0913_GetStandard(pParams->hDemod);
	if (pInfo->Standard == FE_SAT_DVBS2_STANDARD) {
		pInfo->C_N =
		    FE_STV0913_CarrierGetQuality(pParams->hDemod,
						 &FE_STV0913_S2_CN_LookUp);
	} else {		/*DVBS1/DSS */

		pInfo->C_N =
		    FE_STV0913_CarrierGetQuality(pParams->hDemod,
						 &FE_STV0913_S1_CN_LookUp);
	}

	if ((pParams->hDemod->Error) || (pParams->hTuner->Error))
		error = FE_LLA_I2C_ERROR;

	return error;
}

/*****************************************************
--FUNCTION	:: FE_STV0913_8PSKCarrierAdaptation
--ACTION	:: WA for frequency shifter with high jitter and phase noise
--PARAMS IN	:: Handle	==>	Front End Handle

--PARAMS OUT::	NONE
--RETURN	::	Error(if any)
--***************************************************/
static FE_STV0913_Error_t FE_STV0913_8PSKCarrierAdaptation(FE_STV0913_Handle_t
									Handle)
{
	/*
	   Before acquisition:
	   Px_CARHDR = 0x1C
	   Px_BCLC2S28 = 0x84

	   After FEC lock:

	   If  ((8PSK 3/5 or  2/3 Poff) && CNR < 8dB)
	   Px_CARHDR = 0x04
	   Px_BCLC2S28 = 0x31
	 */
	FE_STV0913_SignalInfo_t pInfo;
	FE_STV0913_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0913_InternalParams_t *pParams = NULL;

	if (Handle == NULL)
		return FE_LLA_INVALID_HANDLE;

	pParams = (FE_STV0913_InternalParams_t *) Handle;

	if (pParams->hDemod->Error)
		return FE_LLA_I2C_ERROR;

	FE_STV0913_GetSignalInfoLite(pParams, &pInfo);

	if (pInfo.C_N < 80) {
		ChipSetOneRegister(pParams->hDemod, RSTV0913_CARHDR, 0x04);
		ChipSetOneRegister(pParams->hDemod, RSTV0913_BCLC2S28, 0x31);
	}

	if (pParams->hDemod->Error)
		error = FE_LLA_I2C_ERROR;

	return error;
}

static void FE_STV0913_SetChargePump(FE_STV0913_Handle_t Handle, U32 n_div)
{
	U32 cp;
	if (n_div >= 7 && n_div <= 71)
		cp = 7;
	else if (n_div >= 72 && n_div <= 79)
		cp = 8;
	else if (n_div >= 80 && n_div <= 87)
		cp = 9;
	else if (n_div >= 88 && n_div <= 95)
		cp = 10;
	else if (n_div >= 96 && n_div <= 103)
		cp = 11;
	else if (n_div >= 104 && n_div <= 111)
		cp = 12;
	else if (n_div >= 112 && n_div <= 119)
		cp = 13;
	else if (n_div >= 120 && n_div <= 127)
		cp = 14;
	else if (n_div >= 128 && n_div <= 135)
		cp = 15;
	else if (n_div >= 136 && n_div <= 143)
		cp = 16;
	else if (n_div >= 144 && n_div <= 151)
		cp = 17;
	else if (n_div >= 152 && n_div <= 159)
		cp = 18;
	else if (n_div >= 160 && n_div <= 167)
		cp = 19;
	else if (n_div >= 168 && n_div <= 175)
		cp = 20;
	else if (n_div >= 176 && n_div <= 183)
		cp = 21;
	else if (n_div >= 184 && n_div <= 191)
		cp = 22;
	else if (n_div >= 192 && n_div <= 199)
		cp = 23;
	else if (n_div >= 200 && n_div <= 207)
		cp = 24;
	else if (n_div >= 208 && n_div <= 215)
		cp = 25;
	else if (n_div >= 216 && n_div <= 223)
		cp = 26;
	else if (n_div >= 224 && n_div <= 225)
		cp = 27;
	else
		cp = 7;

	ChipSetField(Handle, FSTV0913_CP, cp);
}

/*****************************************************
--FUNCTION	:: FE_STV0913_Algo
--ACTION	:: Start a search for a valid DVBS1/DVBS2 or DSS transponder
--PARAMS IN	:: pParams ->	Pointer FE_STV0913_InternalParams_t structer
--PARAMS OUT::	NONE
--RETURN	:: SYGNAL TYPE
--***************************************************/
static FE_STV0913_SIGNALTYPE_t FE_STV0913_Algo(FE_STV0913_InternalParams_t
						*pParams, U32 Satellite_Scan)
{
	S32 demodTimeout = 500,
	    fecTimeout = 50,
	    iqPower,
	    agc1Power,
	    i, powerThreshold = STV0913_IQPOWER_THRESHOLD, puncturerate;
	U32 asperity = 0;

	BOOL lock = FALSE;
	FE_STV0913_SIGNALTYPE_t signalType = FE_SAT_NOCARRIER;

#ifdef HOST_PC
#ifndef NO_GUI
	double lockTime;
	char message[100];
	lockTime = Timer();	/* SGA */
#endif
#endif
	/*release reset DVBS2 packet delin */
	ChipSetField(pParams->hDemod, FSTV0913_ALGOSWRST, 0);
	/*Demod Stop*/
	ChipSetOneRegister(pParams->hDemod, RSTV0913_DMDISTATE, 0x5C);
	/*Get the demod and FEC timeout recomended value depending of the
	symbol rate and the search algo */
	FE_STV0913_GetLockTimeout(&demodTimeout, &fecTimeout,
				  pParams->DemodSymbolRate,
				  pParams->DemodSearchAlgo);
	/* If the Symbolrate is unknown  set the BW to the maximum */
	if (pParams->DemodSearchAlgo == FE_SAT_BLIND_SEARCH) {
		pParams->TunerBW = 2 * 36000000;
		/* If Blind search set the init symbol rate to 1Msps */
		FE_STV0913_SetSymbolRate(pParams->hDemod, pParams->MasterClock,
					 1000000);
	} else {
		pParams->TunerBW =
		    (FE_STV0913_CarrierWidth
		     (pParams->DemodSymbolRate,
		      pParams->RollOff) + pParams->DemodSearchRange);
		pParams->TunerBW = pParams->TunerBW * 15 / 10;
		/* Set the Init Symbol rate */
		FE_STV0913_SetSymbolRate(pParams->hDemod, pParams->MasterClock,
					 pParams->DemodSymbolRate);
	}

	FE_Sat_TunerSetFrequency(pParams->hTuner, pParams->TunerFrequency);
	/*      Set tuner BW by SW(SW tuner API)        */
	FE_Sat_TunerSetBandwidth(pParams->hTuner, pParams->TunerBW);

	/*      NO signal Detection */
	/*Read PowerI and PowerQ To check the signal Presence */
	ChipWaitOrAbort(pParams->hDemod, 10);
	ChipGetRegisters(pParams->hDemod, RSTV0913_AGCIQIN1, 2);
	agc1Power =
	    MAKEWORD(ChipGetFieldImage(pParams->hDemod, FSTV0913_AGCIQ_VALUE1),
		     ChipGetFieldImage(pParams->hDemod, FSTV0913_AGCIQ_VALUE0));
	iqPower = 0;
	if (agc1Power == 0) {
		/*if AGC1 integrator value is 0 then read POWERI, POWERQ regs*/
		/*Read the IQ power value */
		for (i = 0; i < 5; i++) {
			ChipGetRegisters(pParams->hDemod, RSTV0913_POWERI, 2);
			iqPower +=
			    (ChipGetFieldImage
			     (pParams->hDemod,
			      FSTV0913_POWER_I) +
			     ChipGetFieldImage(pParams->hDemod,
					       FSTV0913_POWER_Q)) / 2;
		}
		iqPower /= 5;
	}
#ifdef HOST_PC
#ifndef NO_GUI
	/*if PC GUI read the IQ power threshold form the GUI user parameters */
	UsrRdInt("NOSignalThreshold", &powerThreshold);
#endif
#endif

	if (((agc1Power != 0) || (iqPower >= powerThreshold))
	    && (pParams->DemodSearchAlgo == FE_SAT_BLIND_SEARCH)
	    && (Satellite_Scan == 1)	/* AGC2 scan is launched */
	    ) {
		asperity = FE_STV0913_BlindCheckAGC2BandWidth(pParams);
		/*printf("asperity =%d\n", asperity); */
	}

	if ((agc1Power == 0) && (iqPower < powerThreshold)) {
		/*If(AGC1 = 0 and iqPower < IQThreshold)  then no signal  */
		pParams->DemodResults.Locked = FALSE;
		/* Jump of distance of the bandwith tuner */
		signalType = FE_SAT_TUNER_NOSIGNAL;
#ifdef HOST_PC
#ifndef NO_GUI
		ReportInsertMessage
		("NO AGC1 signal, NO POWERI, POWERQ Signal ");
		ReportInsertMessage
		("---------------------------------------------");
#endif
#endif
	} else if ((pParams->DemodSearchAlgo == FE_SAT_BLIND_SEARCH)
		   && (Satellite_Scan == 1)
		   && (asperity == 0)) {
		iqPower = 0;
		agc1Power = 0;
		/*if AGC1 integrator==0 & iqPower < Threshold then NO signal */
		pParams->DemodResults.Locked = FALSE;
		/*To indicate that it must jump wide receiver */
		signalType = FE_SAT_TUNER_NOSIGNAL;
#ifdef HOST_PC
#ifndef NO_GUI
		ReportInsertMessage("BW/2 tuner jump ");
#endif
#endif
		/*printf("<913_drv.c > Nothing in the band, jump BW/ 2\n"); */

	} else if ((pParams->DemodSearchAlgo == FE_SAT_BLIND_SEARCH)
		   && (Satellite_Scan == 1)
		   && (asperity == 1)
		   /*First rising edge detected:Make the blind tuner next step*/

	    ) {
		iqPower = 0;
		agc1Power = 0;
		/*if AGC1 integrator == 0 & iqPower<Threshold then NO signal */
		pParams->DemodResults.Locked = FALSE;
		/*To indicate that it must jump to a width of 4, 8 or 16 steps*/
		signalType = FE_SAT_TUNER_JUMP;

#ifdef HOST_PC
#ifndef NO_GUI
		ReportInsertMessage("Rising edge detected");
#endif
#endif
	} else {
		/* First falling edge detected or asked a blind direct:
		It is blind */
		/*Set the IQ inversion search mode ::FORCE to AUTO*/
		ChipSetFieldImage(pParams->hDemod, FSTV0913_SPECINV_CONTROL,
						FE_SAT_IQ_AUTO);
		ChipSetRegisters(pParams->hDemod, RSTV0913_DEMOD, 1);
		FE_STV0913_SetSearchStandard(pParams);

		/* 8PSK 3/5, 8PSK 2/3 Poff tracking optimization */
		ChipSetOneRegister(pParams->hDemod, RSTV0913_ACLC2S2Q, 0x0B);
		ChipSetOneRegister(pParams->hDemod, RSTV0913_ACLC2S28, 0x0A);
		ChipSetOneRegister(pParams->hDemod, RSTV0913_BCLC2S2Q, 0x84);
		ChipSetOneRegister(pParams->hDemod, RSTV0913_BCLC2S28, 0x84);
		ChipSetOneRegister(pParams->hDemod, RSTV0913_CARHDR, 0x1c);
		ChipSetOneRegister(pParams->hDemod, RSTV0913_CARFREQ, 0x79);

		ChipGetRegisters(pParams->hDemod, RSTV0913_VTH12, 6);

		if (pParams->DemodSearchAlgo != FE_SAT_BLIND_SEARCH)
			FE_STV0913_StartSearch(pParams);

		if (pParams->DemodSearchAlgo == FE_SAT_BLIND_SEARCH)
			/* Special algo for blind search only */
			lock = FE_STV0913_BlindSearchAlgo(pParams, demodTimeout,
						Satellite_Scan);
		else if (pParams->DemodSearchAlgo == FE_SAT_COLD_START)
			lock = FE_STV0913_GetDemodLockCold(pParams,
								demodTimeout);
		else if (pParams->DemodSearchAlgo == FE_SAT_WARM_START)
			lock = FE_STV0913_GetDemodLock(pParams->hDemod,
								demodTimeout);

		if (lock == TRUE) {
			/* Read signal caracteristics and check the lock range*/
			signalType = FE_STV0913_GetSignalParams(pParams,
								Satellite_Scan);
		}

		if ((lock == TRUE) && (signalType == FE_SAT_RANGEOK)) {
			/*The tracking optimization and the FEC lock check are
			perfomed only if: demod is locked and signal type is
			RANGEOK i.e a TP found within the given search range*/
			/* Reduce tuner Bw to tracking value */
			FE_Sat_TunerSetBandwidth(pParams->hTuner,
						pParams->TunerBW * 10 / 15);
			/* Optimization setting for tracking */
			FE_STV0913_TrackingOptimization(pParams);
			if (pParams->DemodResults.Standard ==
			    FE_SAT_DVBS2_STANDARD) {
				if ((pParams->DemodResults.Pilots == 0)
					&& ((pParams->DemodResults.ModCode ==
							FE_SAT_8PSK_23)
					|| (pParams->DemodResults.ModCode ==
							FE_SAT_8PSK_35))) {
					/*8PSK 35 23 pilot off adaptation */
					FE_STV0913_8PSKCarrierAdaptation(
								pParams);
				}
			}
			/*Release stream merger reset */
			ChipSetField(pParams->hDemod, FSTV0913_RST_HWARE, 0);
			ChipWaitOrAbort(pParams->hDemod, 3);
			/* Stream merger reset */
			ChipSetField(pParams->hDemod, FSTV0913_RST_HWARE, 1);
			/* Release stream merger reset */
			ChipSetField(pParams->hDemod, FSTV0913_RST_HWARE, 0);

			if (FE_STV0913_WaitForLock
			    (pParams->hDemod, fecTimeout, fecTimeout,
			     Satellite_Scan) == TRUE) {
				lock = TRUE;
				pParams->DemodResults.Locked = TRUE;

				if (pParams->DemodResults.Standard ==
				    FE_SAT_DVBS2_STANDARD) {

					ChipSetFieldImage(pParams->hDemod,
						FSTV0913_RESET_UPKO_COUNT, 1);
					ChipSetRegisters(pParams->hDemod,
						RSTV0913_PDELCTRL2, 1);
					/*reset DVBS2 pkt delinator error ctr */
					ChipSetFieldImage(pParams->hDemod,
						FSTV0913_RESET_UPKO_COUNT, 0);
					ChipSetRegisters(pParams->hDemod,
						RSTV0913_PDELCTRL2, 1);
					/* reset the error counter to  DVBS2
					packet error rate */
					ChipSetOneRegister(pParams->hDemod,
						RSTV0913_ERRCTRL1, 0x67);
				} else {/* reset the viterbi bit error rate */
					ChipSetOneRegister(pParams->hDemod,
						RSTV0913_ERRCTRL1, 0x75);
				}
				/*Reset the Total packet counter */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0913_FBERCPT4, 0);
				/*Reset the packet Error counter2(and Set it to
				infinite error count mode) */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0913_ERRCTRL2, 0xc1);

			} else {
				lock = FALSE;
				/*if the demod is locked and not the
				FEC signal type=>no DATA */
				signalType = FE_SAT_NODATA;
				pParams->DemodResults.Locked = FALSE;
			}

		}

	}
	/* VTH for tracking */
	ChipSetFieldImage(pParams->hDemod, FSTV0913_VTH12, 0xD7);
	ChipSetFieldImage(pParams->hDemod, FSTV0913_VTH23, 0x85);
	ChipSetFieldImage(pParams->hDemod, FSTV0913_VTH34, 0x58);
	ChipSetFieldImage(pParams->hDemod, FSTV0913_VTH56, 0x3A);
	ChipSetFieldImage(pParams->hDemod, FSTV0913_VTH67, 0x34);
	ChipSetFieldImage(pParams->hDemod, FSTV0913_VTH78, 0x28);
	ChipSetRegisters(pParams->hDemod, RSTV0913_VTH12, 6);

	/* enable the found puncture rate only */
	puncturerate = FE_STV0913_GetViterbiPunctureRate(pParams->hDemod);
	if (pParams->DemodResults.Standard == FE_SAT_DVBS1_STANDARD)
		FE_STV0913_SetViterbiStandard(pParams->hDemod,
					      FE_SAT_SEARCH_DVBS1,
					      puncturerate);
	else if (pParams->DemodResults.Standard == FE_SAT_DSS_STANDARD)
		FE_STV0913_SetViterbiStandard(pParams->hDemod,
					      FE_SAT_SEARCH_DSS, puncturerate);

	if (pParams->DemodSearchAlgo == FE_SAT_BLIND_SEARCH) {
		/* close the ReedSolomon to viterbi feedback after lock */
		ChipSetField(pParams->hDemod, FSTV0913_DIS_RSFLOCK, 0);
		/* close Viterbi feedback after lock */
		ChipSetField(pParams->hDemod, FSTV0913_DIS_VITLOCK, 0);
	}

	return signalType;
}
#ifdef STM_FE_DEFINED_FOR_FUTURE_USE
/*****************************************************
--FUNCTION	::	FE_STV0913_SetReg
--ACTION	::	write one register
--PARAMS IN	::	Handle	==>	Front End Handle
		::Reg		==> register Index in the register Map
		::	Val	==> Val to be writen
--PARAMS OUT::	NONE.
--RETURN	::	Error(if any)
--***************************************************/
FE_STV0913_Error_t FE_STV0913_SetReg(FE_STV0913_Handle_t Handle, U16 Reg,
				     U32 Val)
{
	FE_STV0913_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0913_InternalParams_t *pParams =
	    (FE_STV0913_InternalParams_t *) Handle;

	if (pParams == NULL)
		error = FE_LLA_INVALID_HANDLE;
	else {
		ChipSetOneRegister(pParams->hDemod, Reg, Val);
		if (pParams->hDemod->Error)
			error = FE_LLA_I2C_ERROR;
	}
	return error;
}

/*****************************************************
--FUNCTION	::	FE_STV0913_GetReg
--ACTION	::	write one register
--PARAMS IN	::	Handle	==>	Front End Handle
		::	Reg		==> register Index in the register Map
--PARAMS OUT	::	Val		==> Read value.
--RETURN	::	Error(if any)
--***************************************************/
FE_STV0913_Error_t FE_STV0913_GetReg(FE_STV0913_Handle_t Handle, U16 Reg,
				     U32 *Val)
{
	FE_STV0913_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0913_InternalParams_t *pParams =
	    (FE_STV0913_InternalParams_t *) Handle;

	if ((pParams == NULL) || (Val == NULL))
		error = FE_LLA_INVALID_HANDLE;
	else {
		*Val = ChipGetOneRegister(pParams->hDemod, Reg);
		if (pParams->hDemod->Error)
			error = FE_LLA_I2C_ERROR;
	}
	return error;
}

/*****************************************************
--FUNCTION	::	FE_STV0913_SetField
--ACTION	::	write a value to a Field
--PARAMS IN	::	Handle	==>	Front End Handle
		::	Reg		==> Field Index in the register Map
		::	Val	==> Val to be writen
--PARAMS OUT::	NONE.
--RETURN	::	Error(if any)
--***************************************************/
FE_STV0913_Error_t FE_STV0913_SetField(FE_STV0913_Handle_t Handle, U32 Field,
				       S32 Val)
{
	FE_STV0913_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0913_InternalParams_t *pParams =
	    (FE_STV0913_InternalParams_t *) Handle;

	if (pParams == NULL)
		error = FE_LLA_INVALID_HANDLE;
	else {
		ChipSetField(pParams->hDemod, Field, Val);
		if (pParams->hDemod->Error)
			error = FE_LLA_I2C_ERROR;
	}
	return error;
}

/*****************************************************
--FUNCTION	::	FE_STV0913_GetField
--ACTION	::	Read A Field
--PARAMS IN	::	Handle	==>	Front End Handle
		::	Field	==> Field Index in the register Map
--PARAMS OUT	::	Val		==> Read value.
--RETURN	::	Error(if any)
--***************************************************/
FE_STV0913_Error_t FE_STV0913_GetField(FE_STV0913_Handle_t Handle, U32 Field,
				       S32 *Val)
{
	FE_STV0913_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0913_InternalParams_t *pParams =
	    (FE_STV0913_InternalParams_t *) Handle;

	if ((pParams == NULL) || (Val == NULL))
		error = FE_LLA_INVALID_HANDLE;
	else {
		*Val = ChipGetField(pParams->hDemod, Field);
		if (pParams->hDemod->Error)
			error = FE_LLA_I2C_ERROR;
	}
	return error;
}
#endif
/****************************************************************
 ****************************************************************
 ****************************************************************
 ****
 ***************************************************
 **				PUBLIC ROUTINES
 ***************************************************
 ****
 ****************************************************************
 ****************************************************************
 ****************************************************************/

/*****************************************************
--FUNCTION	::	FE_STV0913_GetRevision
--ACTION	::	Return current LLA version
--PARAMS IN	::	NONE
--PARAMS OUT::	NONE
--RETURN	::	Revision ==> Text string containing LLA version
--***************************************************/
ST_Revision_t FE_STV0913_GetRevision(void)
{
	return RevisionSTV0913;
}

 /*****************************************************
--FUNCTION	::	FE_STV0913_Init
--ACTION	::	Initialisation of the STV0913 chip
--PARAMS IN	::	Handle	==>	Front End Handle
--PARAMS OUT::	NONE
--RETURN	::	Handle to STV0913
--***************************************************/
FE_STV0913_Error_t FE_STV0913_Init(FE_STV0913_InitParams_t *pInit,
				   FE_STV0913_Handle_t *Handle)
{
	FE_STV0913_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0913_InternalParams_t *pParams = NULL;

	TUNER_Error_t tunerError = TUNER_NO_ERR;

	STCHIP_Error_t demodError = CHIPERR_NO_ERROR;
	/* Demodulator chip initialisation parameters */
	Demod_InitParams_t DemodInitParams;
	SAT_TUNER_InitParams_t TunerInitParams;

	/* Internal params structure allocation */
#ifdef HOST_PC
	STCHIP_Info_t DemodChip;
	pParams = calloc(1, sizeof(FE_STV0913_InternalParams_t));
	(*Handle) = (FE_STV0913_Handle_t) pParams;
#endif
#ifdef CHIP_STAPI
	pParams = (FE_STV0913_InternalParams_t *) (*Handle);
#endif

	if (pParams == NULL || pParams->hDemod == NULL)
		return FE_LLA_INVALID_HANDLE;

	/* Chip initialisation */
#if defined(HOST_PC) && !defined(NO_GUI)

	pParams->hDemod = DEMOD;
	pParams->hTuner = TUNER_A;
	pParams->Quartz = EXTCLK;

	ChipSetField(pParams->hDemod, FSTV0913_TUN_IQSWAP,
		     pInit->TunerIQ_Inversion);
	ChipSetField(pParams->hDemod, FSTV0913_ROLLOFF_CONTROL, pInit->RollOff);

#else

#ifdef CHIP_STAPI
	DemodInitParams.Chip = (pParams->hDemod);
#else
	DemodInitParams.Chip = &DemodChip;
#endif

	/* Demodulator(STV0913) */
	DemodInitParams.NbDefVal = STV0913_NBREGS;
	DemodInitParams.Chip->RepeaterHost = NULL;
	DemodInitParams.Chip->RepeaterFn = NULL;
	DemodInitParams.Chip->Repeater = FALSE;
	DemodInitParams.Chip->I2cAddr = pInit->DemodI2cAddr;
	strcpy((char *)DemodInitParams.Chip->Name, pInit->DemodName);

	demodError = STV0913_Init(&DemodInitParams, &pParams->hDemod);

	if (pInit->TunerModel == TUNER_NULL)
		return FE_LLA_INVALID_HANDLE;

	TunerInitParams.Model = pInit->TunerModel;
	TunerInitParams.TunerName = pInit->TunerName;
	strcpy((char *)TunerInitParams.TunerName, pInit->TunerName);
	TunerInitParams.TunerI2cAddress = pInit->Tuner_I2cAddr;
	TunerInitParams.Reference = pInit->TunerRefClock;
	TunerInitParams.IF = 0;
	if (pInit->TunerIQ_Inversion == FE_SAT_IQ_NORMAL)
		TunerInitParams.IQ_Wiring = TUNER_IQ_NORMAL;
	else
		TunerInitParams.IQ_Wiring = TUNER_IQ_INVERT;
	TunerInitParams.RepeaterHost = pParams->hDemod;
	TunerInitParams.RepeaterFn = FE_STV0913_RepeaterFn;
	TunerInitParams.DemodModel = DEMOD_STV0913;
	TunerInitParams.OutputClockDivider = pInit->TunerOutClkDivider;
#ifdef DLL
	TunerInitParams.Lna_agc_mode = pInit->TunerLnaAgcMode;
	TunerInitParams.Lna_agc_ref = pInit->TunerLnaAgcRef;
#endif
	TunerInitParams.InputSelect = pInit->TunerInputSelect;

	tunerError = FE_Sat_TunerInit((&TunerInitParams), &pParams->hTuner);

	if (demodError == CHIPERR_NO_ERROR) {	/*Check the demod error first */
		/*If no Error on the demod the check the Tuners */
		if (tunerError == TUNER_NO_ERR)
			error = FE_LLA_NO_ERROR;/*if no error on the tuner */
		else if (tunerError == TUNER_INVALID_HANDLE)
			error = FE_LLA_INVALID_HANDLE;
		else if (tunerError == TUNER_TYPE_ERR)
			error = FE_LLA_BAD_PARAMETER;	/*if tuner type error */
		else
			error = FE_LLA_I2C_ERROR;
	} else {
		if (demodError == CHIPERR_INVALID_HANDLE)
			error = FE_LLA_INVALID_HANDLE;
		else
			error = FE_LLA_I2C_ERROR;
	}

#endif

	if (error != FE_LLA_NO_ERROR)
		return error;

	/*Read IC cut ID */
	pParams->hDemod->ChipID =
	    ChipGetOneRegister(pParams->hDemod, RSTV0913_MID);

	/*Tuner parameters */
	pParams->TunerType = pInit->TunerHWControlType;
	pParams->TunerGlobal_IQ_Inv = pInit->TunerIQ_Inversion;

	pParams->RollOff = pInit->RollOff;
#if defined(CHIP_STAPI) || defined(NO_GUI)
	pParams->Quartz = pInit->DemodRefClock;	/*Ext clock in Hz */

	ChipSetField(pParams->hDemod, FSTV0913_ROLLOFF_CONTROL, pInit->RollOff);
	/*Set TS1 and TS2 to serial or parallel mode */
	FE_STV0913_SetTS_Parallel_Serial(pParams->hDemod, pInit->PathTSClock);

	switch (pInit->TunerHWControlType) {
	case FE_SAT_SW_TUNER:
	default:
		/* Set the tuner ref clock */
		FE_Sat_TunerSetReferenceFreq(pParams->hTuner,
							pInit->TunerRefClock);
		/*Set the tuner BBgain */
		FE_Sat_TunerSetGain(pParams->hTuner, pInit->TunerBasebandGain);
		break;

	}
		/*IQSWAP setting mast be after FE_STV0913_SetTunerType */
	ChipSetField(pParams->hDemod, FSTV0913_TUN_IQSWAP,
						pInit->TunerIQ_Inversion);
		/*Set the Mclk value to 135MHz */
	FE_STV0913_SetMclk((FE_STV0913_Handle_t) pParams, 135000000,
						pParams->Quartz);
	ChipWaitOrAbort(pParams->hDemod, 3);	/*wait for PLL lock */
		/*switch the 913 to the PLL */
	ChipSetOneRegister(pParams->hDemod, RSTV0913_SYNTCTRL, 0x22);

#endif
	/*Read the cuurent Mclk */
	pParams->MasterClock = FE_STV0913_GetMclkFreq(pParams->hDemod,
					pParams->Quartz);

	/*Check the error at the end of the Init */
	if ((pParams->hDemod->Error) || (pParams->hTuner->Error))
		error = FE_LLA_I2C_ERROR;

	return error;
}

FE_STV0913_Error_t FE_STV0913_SetTSoutput(FE_STV0913_Handle_t Handle,
					  FE_STV0913_TSConfig_t *PathpTSConfig)
{

	FE_STV0913_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0913_InternalParams_t *pParams = NULL;

	pParams = (FE_STV0913_InternalParams_t *) Handle;

	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;

	if (pParams->hDemod->Error)
		return FE_LLA_I2C_ERROR;

	switch (PathpTSConfig->TSMode) {
	case FE_TS_PARALLEL_PUNCT_CLOCK:
		/* D7:msb,D0:lsb */
		ChipSetField(pParams->hDemod, FSTV0913_TSFIFO_PERMDATA, 0);
		ChipSetField(pParams->hDemod, FSTV0913_TS1_OUTPAR_HZ, 0x00);
		/*|| mode*/
		ChipSetField(pParams->hDemod, FSTV0913_TSFIFO_SERIAL, 0x00);
		ChipSetField(pParams->hDemod, FSTV0913_TSFIFO_DVBCI, 0x00);
		break;

	case FE_TS_DVBCI_CLOCK:
		/* D7:msb, D0:lsb */
		ChipSetField(pParams->hDemod, FSTV0913_TSFIFO_PERMDATA, 0);
		ChipSetField(pParams->hDemod, FSTV0913_TS1_OUTPAR_HZ, 0x00);
		/*|| mode*/
		ChipSetField(pParams->hDemod, FSTV0913_TSFIFO_SERIAL, 0x00);
		ChipSetField(pParams->hDemod, FSTV0913_TSFIFO_DVBCI, 0x01);
		break;

	case FE_TS_SERIAL_PUNCT_CLOCK:
		/*msb first*/
		ChipSetField(pParams->hDemod, FSTV0913_TSFIFO_PERMDATA, 0);
		ChipSetField(pParams->hDemod, FSTV0913_TS1_OUTSER_HZ, 0x00);
		ChipSetField(pParams->hDemod, FSTV0913_TS1_OUTPAR_HZ, 0x01);
		/*serial */
		ChipSetField(pParams->hDemod, FSTV0913_TSFIFO_SERIAL, 0x01);
		ChipSetField(pParams->hDemod, FSTV0913_TSFIFO_DVBCI, 0x00);
		break;

	case FE_TS_SERIAL_CONT_CLOCK:
		ChipSetField(pParams->hDemod, FSTV0913_TSFIFO_PERMDATA, 0);
		ChipSetField(pParams->hDemod, FSTV0913_TS1_OUTSER_HZ, 0x00);
		ChipSetField(pParams->hDemod, FSTV0913_TS1_OUTPAR_HZ, 0x01);
		/* serial*/
		ChipSetField(pParams->hDemod, FSTV0913_TSFIFO_SERIAL, 0x01);
		ChipSetField(pParams->hDemod, FSTV0913_TSFIFO_DVBCI, 0x01);
		break;

	default:
		break;
	}
		/*Reset stream merger */
		ChipSetField(pParams->hDemod, FSTV0913_RST_HWARE, 1);
		ChipSetField(pParams->hDemod, FSTV0913_RST_HWARE, 0);

return error;
}

/*****************************************************
--FUNCTION	::	FE_STV0913_SetTSConfig
--ACTION	::	TS configuration
--PARAMS IN	::	Handle		==>	Front End Handle
		::	Path1pTSConfig	==> path1 TS config parameters
--PARAMS OUT::	Path1TSSpeed_Hz	==> path1 Current TS speed in Hz.
--RETURN	::	Error(if any)
--***************************************************/
FE_STV0913_Error_t FE_STV0913_SetTSConfig(FE_STV0913_Handle_t Handle,
					  FE_STV0913_TSConfig_t *PathpTSConfig,
					  U32 *PathTSSpeed_Hz)
{
	FE_STV0913_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0913_InternalParams_t *pParams = NULL;

	U32 tsspeed;

	pParams = (FE_STV0913_InternalParams_t *) Handle;

	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;

	if (pParams->hDemod->Error)
		return FE_LLA_I2C_ERROR;

	ChipSetOneRegister(pParams->hDemod, RSTV0913_TSGENERAL, 0);

		/* enable/Disable SYNC byte */
	if (PathpTSConfig->TSSyncByteEnable == FE_TS_SYNCBYTE_OFF)
		ChipSetField(pParams->hDemod, FSTV0913_TSDEL_SYNCBYTE, 1);
	else
		ChipSetField(pParams->hDemod, FSTV0913_TSDEL_SYNCBYTE, 0);
		/*DVBS1 Data parity bytes enabling/disabling */
	if (PathpTSConfig->TSParityBytes == FE_TS_PARITYBYTES_ON)
		ChipSetField(pParams->hDemod, FSTV0913_TSINSDEL_RSPARITY, 1);
	else
		ChipSetField(pParams->hDemod, FSTV0913_TSINSDEL_RSPARITY, 0);
	/*TS clock Polarity setting : rising edge/falling edge */
	if (PathpTSConfig->TSClockPolarity == FE_TS_RISINGEDGE_CLOCK)
		ChipSetField(pParams->hDemod, FSTV0913_TS1_CLOCKOUT_XOR, 0);
	else
		ChipSetField(pParams->hDemod, FSTV0913_TS1_CLOCKOUT_XOR, 1);

	if (PathpTSConfig->TSSpeedControl == FE_TS_MANUAL_SPEED) {
		/*path 2 TS speed setting */
		ChipSetField(pParams->hDemod, FSTV0913_TSFIFO_MANSPEED, 3);
		switch (PathpTSConfig->TSMode) {
		case FE_TS_PARALLEL_PUNCT_CLOCK:
		case FE_TS_DVBCI_CLOCK:
			/*Formulat :
			   TS_Speed_Hz = 4 * Msterclock_Hz / TSSPEED_REG
			 */
			tsspeed = (4 * pParams->MasterClock) /
						PathpTSConfig->TSClockRate;
			/*in || clock the TS speed is limited< MasterClock/4 */
			if (tsspeed <= 16)
				tsspeed = 16;

			ChipSetOneRegister(pParams->hDemod, RSTV0913_TSSPEED,
					   tsspeed);
			break;
		case FE_TS_SERIAL_PUNCT_CLOCK:
		case FE_TS_SERIAL_CONT_CLOCK:
		case FE_TS_OUTPUTMODE_DEFAULT:
		default:

			/*Formulat :
			   TS_Speed_Hz = 32 * Msterclock_Hz / TSSPEED_REG
			 */
			tsspeed = (16 * pParams->MasterClock) /
					(PathpTSConfig->TSClockRate / 2);
			/*in serial clk the TS speed is limited <= MasterClock*/
			if (tsspeed <= 32)
				tsspeed = 32;

			ChipSetOneRegister(pParams->hDemod, RSTV0913_TSSPEED,
					   tsspeed);
			break;
		}

	}
	if (PathpTSConfig->TSMode == FE_TS_DVBCI_CLOCK) {

		ChipSetField(pParams->hDemod, FSTV0913_TSFIFO_MANSPEED, 1);
		/*Formulat :
		   TS_Speed_Hz = 32 * Msterclock_Hz / TSSPEED_REG
		 */
		 /*if DVBCI set TS clock to 9MHz */
		tsspeed = (16 * pParams->MasterClock) / 9000000;
		/*in serial clock the TS speed is limited <= MasterClock */
		if (tsspeed <= 32)
			tsspeed = 32;

		ChipSetOneRegister(pParams->hDemod, RSTV0913_TSSPEED, tsspeed);

	} else
		ChipSetField(pParams->hDemod, FSTV0913_TSFIFO_MANSPEED, 0);

	switch (PathpTSConfig->TSMode) {	/*D7/D0 permute if serial */
	case FE_TS_SERIAL_PUNCT_CLOCK:
	case FE_TS_SERIAL_CONT_CLOCK:
	case FE_TS_OUTPUTMODE_DEFAULT:
	default:
		if (PathpTSConfig->TSSwap == FE_TS_SWAP_ON)/*Serial o/p on D0*/
			ChipSetField(pParams->hDemod, FSTV0913_TS_SERDATA0, 1);
		else		/* Serial output on D7 */
			ChipSetField(pParams->hDemod, FSTV0913_TS_SERDATA0, 0);
		break;

	case FE_TS_PARALLEL_PUNCT_CLOCK:
	case FE_TS_DVBCI_CLOCK:
		if (PathpTSConfig->TSSwap == FE_TS_SWAP_ON)
			ChipSetField(pParams->hDemod, FSTV0913_TSFIFO_PERMDATA,
				     1);
		else
			ChipSetField(pParams->hDemod, FSTV0913_TSFIFO_PERMDATA,
				     0);
		break;
	}
	/*Parallel mode */
	if (ChipGetField(pParams->hDemod, FSTV0913_TSFIFO_SERIAL) == 0)
		*PathTSSpeed_Hz =
		    (4 * pParams->MasterClock) /
		    ChipGetOneRegister(pParams->hDemod, RSTV0913_TSSPEED);
	else {			/*serial mode */

		*PathTSSpeed_Hz =
		    (16 * pParams->MasterClock) /
		    ChipGetOneRegister(pParams->hDemod, RSTV0913_TSSPEED);
		(*PathTSSpeed_Hz) *= 2;
	}

	if (pParams->hDemod->Error)/*Check the error at the end of the f/n */
		error = FE_LLA_I2C_ERROR;

	return error;
}

/*****************************************************
--FUNCTION	::	FE_STV0913_GetSignalInfo
--ACTION	::	Return informations on the locked transponder
--PARAMS IN	::	Handle	==>	Front End Handle
--PARAMS OUT::	pInfo	==> Informations(BER, C/N, power ...)
--RETURN	::	Error(if any)
--***************************************************/
FE_STV0913_Error_t FE_STV0913_GetSignalInfo(FE_STV0913_Handle_t Handle,
					    FE_STV0913_SignalInfo_t *pInfo,
					    U32 Satellite_Scan)
{
	FE_STV0913_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0913_InternalParams_t *pParams = NULL;
	FE_STV0913_SEARCHSTATE_t demodState;
	S32 symbolRateOffset;

	pParams = (FE_STV0913_InternalParams_t *) Handle;

	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;

	if ((pParams->hDemod->Error) || (pParams->hTuner->Error))
		return FE_LLA_I2C_ERROR;

	demodState = ChipGetField(pParams->hDemod, FSTV0913_HEADER_MODE);
	switch (demodState) {

	case FE_SAT_SEARCH:
	case FE_SAT_PLH_DETECTED:
	default:
		pInfo->Locked = FALSE;
		break;
	case FE_SAT_DVBS2_FOUND:
		pInfo->Locked =
		    ChipGetField(pParams->hDemod, FSTV0913_LOCK_DEFINITIF)
		    && ChipGetField(pParams->hDemod, FSTV0913_PKTDELIN_LOCK)
		    && ChipGetField(pParams->hDemod, FSTV0913_TSFIFO_LINEOK);

		break;
	case FE_SAT_DVBS_FOUND:
		pInfo->Locked =
		    ChipGetField(pParams->hDemod, FSTV0913_LOCK_DEFINITIF)
		    && ChipGetField(pParams->hDemod, FSTV0913_LOCKEDVIT)
		    && ChipGetField(pParams->hDemod, FSTV0913_TSFIFO_LINEOK);

		break;
	}

	/* transponder_frequency = tuner +  demod carrier frequency */
	pInfo->Frequency = FE_Sat_TunerGetFrequency(pParams->hTuner);
	pInfo->Frequency +=
	    FE_STV0913_GetCarrierFrequency(pParams->hDemod,
						pParams->MasterClock) / 1000;

	pInfo->SymbolRate = FE_STV0913_GetSymbolRate(pParams->hDemod,
					pParams->MasterClock);/*Get SR */
	symbolRateOffset =
	    FE_STV0913_TimingGetOffset(pParams->hDemod, pInfo->SymbolRate);
	pInfo->SymbolRate += symbolRateOffset;	/* Get timing loop offset */

	pInfo->Standard = FE_STV0913_GetStandard(pParams->hDemod);

	pInfo->PunctureRate =
	    FE_STV0913_GetViterbiPunctureRate(pParams->hDemod);
	pInfo->ModCode = ChipGetField(pParams->hDemod, FSTV0913_DEMOD_MODCOD);
	pInfo->Pilots =
	    ChipGetFieldImage(pParams->hDemod, FSTV0913_DEMOD_TYPE) & 0x01;
	pInfo->FrameLength =
	    ((U32) ChipGetFieldImage(pParams->hDemod,
						FSTV0913_DEMOD_TYPE)) >> 1;
	pInfo->RollOff = ChipGetField(pParams->hDemod, FSTV0913_ROLLOFF_STATUS);

	if (Satellite_Scan == 0) {
		pInfo->BER = FE_STV0913_GetBer(pParams->hDemod);
		pInfo->Power = FE_STV0913_GetRFLevel(pParams);

		if (pInfo->Standard == FE_SAT_DVBS2_STANDARD)
			pInfo->C_N =
			    FE_STV0913_CarrierGetQuality(pParams->hDemod,
						 &FE_STV0913_S2_CN_LookUp);
		else
			pInfo->C_N =
			    FE_STV0913_CarrierGetQuality(pParams->hDemod,
						 &FE_STV0913_S1_CN_LookUp);

	} else {	/* no BER, Power and CNR measurement during scan */
		pInfo->BER = 0;
		pInfo->Power = 0;
		pInfo->C_N = 100;
	}

	if (pInfo->Standard == FE_SAT_DVBS2_STANDARD) {
		pInfo->Spectrum =
		    ChipGetField(pParams->hDemod, FSTV0913_SPECINV_DEMOD);

		if (pInfo->ModCode <= FE_SAT_QPSK_910)
			pInfo->Modulation = FE_SAT_MOD_QPSK;
		else if (pInfo->ModCode <= FE_SAT_8PSK_910)
			pInfo->Modulation = FE_SAT_MOD_8PSK;
		else if (pInfo->ModCode <= FE_SAT_16APSK_910)
			pInfo->Modulation = FE_SAT_MOD_16APSK;
		else if (pInfo->ModCode <= FE_SAT_32APSK_910)
			pInfo->Modulation = FE_SAT_MOD_32APSK;
		else
			pInfo->Modulation = FE_SAT_MOD_UNKNOWN;

		/*reset The error counter to PER */
		/* reset the error counter to  DVBS2 packet error rate */
		ChipSetOneRegister(pParams->hDemod, RSTV0913_ERRCTRL1, 0x67);

	} else {		/*DVBS1/DSS */

		pInfo->Spectrum = ChipGetField(pParams->hDemod, FSTV0913_IQINV);
		pInfo->Modulation = FE_SAT_MOD_QPSK;
	}
	/*Check the error at the end of the function */
	if ((pParams->hDemod->Error) || (pParams->hTuner->Error))
		error = FE_LLA_I2C_ERROR;

	return error;
}

/*****************************************************
--FUNCTION	::	FE_STV0913_Tracking
--ACTION	:: Return Tracking informations:lock status,RF level,C/N and BER
--PARAMS IN	::	Handle	==>	Front End Handle
		::
--PARAMS OUT::	pTrackingInfo	==> pointer to FE_Sat_TrackingInfo_t struct.
			::
--RETURN	::	Error(if any)
--***************************************************/
FE_STV0913_Error_t FE_STV0913_Tracking(FE_STV0913_Handle_t Handle,
				       FE_Sat_TrackingInfo_t *pTrackingInfo)
{
	FE_STV0913_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0913_InternalParams_t *pParams = NULL;
	FE_STV0913_SEARCHSTATE_t demodState;
	pParams = (FE_STV0913_InternalParams_t *) Handle;

	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;

	if ((pParams->hDemod->Error) || (pParams->hTuner->Error))
		return FE_LLA_I2C_ERROR;

	pTrackingInfo->Power = FE_STV0913_GetRFLevel(pParams);
	pTrackingInfo->BER = FE_STV0913_GetBer(pParams->hDemod);

	demodState = ChipGetField(pParams->hDemod, FSTV0913_HEADER_MODE);
	switch (demodState) {
	case FE_SAT_SEARCH:
	case FE_SAT_PLH_DETECTED:
	default:
		pTrackingInfo->Locked = FALSE;
		pTrackingInfo->C_N = 0;
		break;
	case FE_SAT_DVBS2_FOUND:
		pTrackingInfo->Locked =
		    ChipGetField(pParams->hDemod, FSTV0913_LOCK_DEFINITIF)
		    && ChipGetField(pParams->hDemod, FSTV0913_PKTDELIN_LOCK)
		    && ChipGetField(pParams->hDemod, FSTV0913_TSFIFO_LINEOK);

		pTrackingInfo->C_N =
		    FE_STV0913_CarrierGetQuality(pParams->hDemod,
						 &FE_STV0913_S2_CN_LookUp);
		/*reset The error counter to PER */
		/* reset the error counter to  DVBS2 packet error rate */
		ChipSetOneRegister(pParams->hDemod, RSTV0913_ERRCTRL1, 0x67);

		break;

	case FE_SAT_DVBS_FOUND:
		pTrackingInfo->Locked =
		    ChipGetField(pParams->hDemod, FSTV0913_LOCK_DEFINITIF)
		    && ChipGetField(pParams->hDemod, FSTV0913_LOCKEDVIT)
		    && ChipGetField(pParams->hDemod, FSTV0913_TSFIFO_LINEOK);

		pTrackingInfo->C_N =
		    FE_STV0913_CarrierGetQuality(pParams->hDemod,
						 &FE_STV0913_S1_CN_LookUp);
		break;
	}

	pTrackingInfo->Frequency_IF = FE_Sat_TunerGetFrequency(pParams->hTuner);
	pTrackingInfo->Frequency_IF +=
	    FE_STV0913_GetCarrierFrequency(pParams->hDemod,
					   pParams->MasterClock) / 1000;

	if ((pParams->hDemod->Error) || (pParams->hTuner->Error))
		error = FE_LLA_I2C_ERROR;

	return error;
}

/*****************************************************
--FUNCTION	::	FE_STV913_GetPacketErrorRate
--ACTION	:: Return the number of error packet and the window packet count
--PARAMS IN	::Handle	==>	Front End Handle
--PARAMS OUT::	PacketsErrorCount==> Number of packet error, max is 2^23 packet.
	    ::  TotalPacketsCount==> total window packets, max is 2^24 packet.
--RETURN	::	Error(if any)
--***************************************************/
FE_STV0913_Error_t FE_STV913_GetPacketErrorRate(FE_STV0913_Handle_t Handle,
						U32 *PacketsErrorCount,
						U32 *TotalPacketsCount)
{
	FE_STV0913_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0913_InternalParams_t *pParams = NULL;

	U8 packetsCount4 = 0,
	    packetsCount3 = 0,
	    packetsCount2 = 0, packetsCount1 = 0, packetsCount0 = 0;

	pParams = (FE_STV0913_InternalParams_t *) Handle;

	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;

	if ((pParams->hDemod->Error) || (pParams->hTuner->Error))
		return FE_LLA_I2C_ERROR;
	if (FE_STV0913_Status(Handle) == FALSE) {/*if Demod+FEC not locked */
			/*Packet error count is set to the maximum value */
		*PacketsErrorCount = 1 << 23;
			/*Total Packet count is set to the maximum value */
		*TotalPacketsCount = 1 << 24;
	} else {
		/*Read the error counter 2(23 bits) */
		*PacketsErrorCount =
		    FE_STV0913_GetErrorCount(pParams->hDemod,
					     FE_STV0913_COUNTER2) & 0x7FFFFF;

		/*Read the total packet counter 40 bits,
		reading 5 bytes is mandatory */
		/*Read the Total packet counter byte 5 */
		packetsCount4 = ChipGetOneRegister(pParams->hDemod,
							RSTV0913_FBERCPT4);
		/*Read the Total packet counter byte 4 */
		packetsCount3 = ChipGetOneRegister(pParams->hDemod,
							RSTV0913_FBERCPT3);
		/*Read the Total packet counter byte 3 */
		packetsCount2 = ChipGetOneRegister(pParams->hDemod,
							RSTV0913_FBERCPT2);
		/*Read the Total packet counter byte 2 */
		packetsCount1 = ChipGetOneRegister(pParams->hDemod,
							RSTV0913_FBERCPT1);
		/*Read the Total packet counter byte 1 */
		packetsCount0 = ChipGetOneRegister(pParams->hDemod,
							RSTV0913_FBERCPT0);
		/*Use the counter for a maximum of 2^24 packets */
		if ((packetsCount4 == 0) && (packetsCount3 == 0))
			*TotalPacketsCount =
			    ((packetsCount2 & 0xFF) << 16) +
			    ((packetsCount1 & 0xFF) << 8) +
			    (packetsCount0 & 0xFF);
		else
			*TotalPacketsCount = 1 << 24;

		if (*TotalPacketsCount == 0) {
			/*if the packets count doesn't start yet the
			packet error = 1 and total packets = 1 */
			/*if the packet counter doesn't start => FEC error */
			*TotalPacketsCount = 1;
			*PacketsErrorCount = 1;
		}
	}
	/*Reset the Total packet counter */
	ChipSetOneRegister(pParams->hDemod, RSTV0913_FBERCPT4, 0);
	ChipSetField(pParams->hDemod, FSTV0913_BERMETER_RESET, 1);
	ChipSetField(pParams->hDemod, FSTV0913_BERMETER_RESET, 0);
	/*Reset the packet Error counter2(Set it to infinit error count mode)*/
	ChipSetOneRegister(pParams->hDemod, RSTV0913_ERRCTRL2, 0xc1);
	/*Check the error at the end of the function */
	if ((pParams->hDemod->Error) || (pParams->hTuner->Error))
		error = FE_LLA_I2C_ERROR;

	return error;
}

/*****************************************************
--FUNCTION	::	FE_STV913_GetPreViterbiBER
--ACTION	::	Return DEMOD BER(Pre - VITERBI BER) for DVB - S1 only
--PARAMS IN	::	Handle	==>	Front End Handle
--PARAMS OUT::  Pre - VITERBI BER x 10^7
--RETURN	::	Error(if any)
--***************************************************/
FE_STV0913_Error_t FE_STV913_GetPreViterbiBER(FE_STV0913_Handle_t Handle,
					      U32 *PreViterbiBER)
{

	/* warning, when using this function the error counter number 1 is set
	to pre - Viterbi BER, the post viterbi BER info(given in FE_STV0900_
	GetSignalInfo function or FE_STV0900_Tracking is not significant
	any more */

	FE_STV0913_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0913_InternalParams_t *pParams = NULL;

	U32 errorCount = 10000000, frameSize;
	FE_STV0913_Rate_t punctureRate;

	pParams = (FE_STV0913_InternalParams_t *) Handle;

	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;

	if (pParams->hDemod->Error)
		return FE_LLA_I2C_ERROR;
			/* check for VITERBI lock */
	if (ChipGetField(pParams->hDemod, FSTV0913_LOCKEDVIT)) {
		/* Set and reset the error counter1 to Pre - Viterbi BER with
		observation window = 2^6 frames */
		ChipSetOneRegister(pParams->hDemod, RSTV0913_ERRCTRL1, 0x13);
		WAIT_N_MS(50);	/* wait for error accumulation */
		/* Read the puncture rate */
		punctureRate =
			FE_STV0913_GetViterbiPunctureRate(pParams->hDemod);

		/* Read the error counter */
		errorCount =
		    ((ChipGetOneRegister(pParams->hDemod, RSTV0913_ERRCNT12) &
		      0x7F) << 16) + ((ChipGetOneRegister(pParams->hDemod,
							  RSTV0913_ERRCNT11) &
				       0xFF) << 8) +
		    ((ChipGetOneRegister(pParams->hDemod, RSTV0913_ERRCNT10) &
		      0xFF));

		switch (punctureRate) {
			/*
			   compute the frame size
			   frame size = 2688*2*PR;
			 */
		case FE_SAT_PR_1_2:
			frameSize = (5376) / 2;
			break;

		case FE_SAT_PR_2_3:
			frameSize = (5376 * 2) / 3;
			break;

		case FE_SAT_PR_3_4:
			frameSize = (5376 * 3) / 4;
			break;

		case FE_SAT_PR_5_6:
			frameSize = (5376 * 5) / 6;
			break;

		case FE_SAT_PR_6_7:
			frameSize = (5376 * 6) / 7;
			break;

		case FE_SAT_PR_7_8:
			frameSize = (5376 * 7) / 8;
			break;

		default:
			errorCount = 1;
			frameSize = 1;
			break;
		}

		if (frameSize > 1000) {
			frameSize *= 64;/*total window size = frameSize*2^6 */
			errorCount = (errorCount * 1000) / (frameSize / 1000);
			errorCount *= 10;
		} else {
			errorCount = 10000000;	/* if PR is unknown BER = 1 */
		}
	} else
		errorCount = 10000000;	/*if VITERBI is not locked BER = 1 */

	if (pParams->hDemod->Error)
		error = FE_LLA_I2C_ERROR;

	*PreViterbiBER = errorCount;

	return error;
}

/*****************************************************
--FUNCTION	::	FE_STV0913_Status
--ACTION	::	Return locked status
--PARAMS IN	::	Handle	==>	Front End Handle
--PARAMS OUT::	NONE
--RETURN	::	Bool(locked or not)
--***************************************************/
BOOL FE_STV0913_Status(FE_STV0913_Handle_t Handle)
{

	FE_STV0913_InternalParams_t *pParams = NULL;
	BOOL Locked = FALSE;
	FE_STV0913_SEARCHSTATE_t demodState;
	pParams = (FE_STV0913_InternalParams_t *) Handle;

	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;

	demodState = ChipGetField(pParams->hDemod, FSTV0913_HEADER_MODE);
	switch (demodState) {
	case FE_SAT_SEARCH:
	case FE_SAT_PLH_DETECTED:
	default:
		Locked = FALSE;
		break;

	case FE_SAT_DVBS2_FOUND:
		Locked = ChipGetField(pParams->hDemod, FSTV0913_LOCK_DEFINITIF)
		    && ChipGetField(pParams->hDemod, FSTV0913_PKTDELIN_LOCK)
		    && ChipGetField(pParams->hDemod, FSTV0913_TSFIFO_LINEOK);
		break;

	case FE_SAT_DVBS_FOUND:
		Locked = ChipGetField(pParams->hDemod, FSTV0913_LOCK_DEFINITIF)
		    && ChipGetField(pParams->hDemod, FSTV0913_LOCKEDVIT)
		    && ChipGetField(pParams->hDemod, FSTV0913_TSFIFO_LINEOK);
		break;
	}

	return Locked;
}

/*****************************************************
--FUNCTION	::	FE_STV0913_Unlock
--ACTION	::	Unlock the demodulator, set the demod to idle state
--PARAMS IN	::	Handle	==>	Front End Handle

-PARAMS OUT::	NONE.
--RETURN	::	Error(if any)
--***************************************************/
FE_STV0913_Error_t FE_STV0913_Unlock(FE_STV0913_Handle_t Handle)
{
	FE_STV0913_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0913_InternalParams_t *pParams =
	    (FE_STV0913_InternalParams_t *) Handle;

	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;

	if ((pParams->hDemod->Error) || (pParams->hTuner->Error))
		return FE_LLA_I2C_ERROR;
	/*Demod Stop*/
	ChipSetOneRegister(pParams->hDemod, RSTV0913_DMDISTATE, 0x5C);

	if ((pParams->hDemod->Error) || (pParams->hTuner->Error))
		error = FE_LLA_I2C_ERROR;

	return error;
}

/*****************************************************
--FUNCTION	::	FE_STV0913_ResetDevicesErrors
--ACTION	::	Reset Devices I2C error status
--PARAMS IN	::	Handle	==>	Front End Handle
			::
-PARAMS OUT::	NONE.
--RETURN	::	Error(if any)
--***************************************************/
FE_STV0913_Error_t FE_STV0913_ResetDevicesErrors(FE_STV0913_Handle_t Handle)
{
	FE_STV0913_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0913_InternalParams_t *pParams =
	    (FE_STV0913_InternalParams_t *) Handle;

	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;

	if (pParams->hDemod != NULL)
		pParams->hDemod->Error = CHIPERR_NO_ERROR;
	else
		error = FE_LLA_INVALID_HANDLE;

	if (pParams->hTuner != NULL)
		pParams->hTuner->Error = CHIPERR_NO_ERROR;
	else
		error = FE_LLA_INVALID_HANDLE;

	return error;
}

/*****************************************************
--FUNCTION	::	FE_STV0913_SetMclk
--ACTION	::	Set demod Master Clock
--PARAMS IN	::	Handle	==>	Front End Handle
			::	Mclk	==> demod master clock
			::	ExtClk	==>	external Quartz
--PARAMS OUT::	NONE.
--RETURN	::	Error(if any)
--***************************************************/
FE_STV0913_Error_t FE_STV0913_SetMclk(FE_STV0913_Handle_t Handle, U32 Mclk,
				      U32 ExtClk)
{
	FE_STV0913_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0913_InternalParams_t *pParams =
	    (FE_STV0913_InternalParams_t *) Handle;

	U32 ndiv, quartz, idf, odf, Fphi;

	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;

	if (pParams->hDemod->Error)
		return FE_LLA_I2C_ERROR;

	/* 800MHz < Fvco < 1800MHz */
	/* Fvco = (ExtClk * 2 * NDIV) / IDF */
	/* (400 * IDF) / ExtClk < NDIV < (900 * IDF) / ExtClk   */

	/* ODF forced to 4 otherwise desynchronization of digital and analog clk
	which result to a bad calculated symbolrate */
	odf = 4;
	/* IDF forced to 1 : Optimal value */
	idf = 1;

	ChipSetField(pParams->hDemod, FSTV0913_ODF, odf);
	ChipSetField(pParams->hDemod, FSTV0913_IDF, idf);

	quartz = ExtClk / 1000000;
	Fphi = Mclk / 1000000;

	ndiv = (Fphi * odf * idf) / quartz;

	ChipSetField(pParams->hDemod, FSTV0913_N_DIV, ndiv);

	/* Set CP according to NDIV */
	FE_STV0913_SetChargePump(pParams->hDemod, ndiv);

	if (pParams->hDemod->Error)/*Check the error at the end of the f/n */
		error = FE_LLA_I2C_ERROR;

	return error;
}

/*****************************************************
--FUNCTION	::	FE_STV0913_SetStandby
--ACTION	::	Set demod STANDBAY mode On/Off
--PARAMS IN	::	Handle	==>	Front End Handle

-PARAMS OUT::	NONE.
--RETURN	::	Error(if any)
--***************************************************/
FE_STV0913_Error_t FE_STV0913_SetStandby(FE_STV0913_Handle_t Handle,
					 U8 StandbyOn)
{
	FE_STV0913_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0913_InternalParams_t *pParams =
	    (FE_STV0913_InternalParams_t *) Handle;

	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;

	if ((pParams->hDemod->Error) || (pParams->hTuner->Error))
		return FE_LLA_I2C_ERROR;

	/* In demod STANDBY general I2C and I2C repeater remains active */
	/* Set demod general standby ON/OFF */
	ChipSetField(pParams->hDemod, FSTV0913_STANDBY, StandbyOn);
	/* Set the Tuner Standby ON or OFF */
	FE_Sat_TunerSetStandby(pParams->hTuner, StandbyOn);

	/*Check the error at the end of the function */
	if ((pParams->hDemod->Error) || (pParams->hTuner->Error))
		error = FE_LLA_I2C_ERROR;

return error;
}

/*****************************************************
--FUNCTION	::	FE_STV0913_SetAbortFlag
--ACTION	::	Set Abort flag On/Off
--PARAMS IN	::	Handle	==>	Front End Handle

-PARAMS OUT::	NONE.
--RETURN	::	Error(if any)

--***************************************************/
FE_STV0913_Error_t FE_STV0913_SetAbortFlag(FE_STV0913_Handle_t Handle,
					   BOOL Abort)
{
	FE_STV0913_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0913_InternalParams_t *pParams =
	    (FE_STV0913_InternalParams_t *) Handle;

	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;

	if ((pParams->hDemod->Error) || (pParams->hTuner->Error))
		return FE_LLA_I2C_ERROR;

	ChipAbort(pParams->hTuner, Abort);
	ChipAbort(pParams->hDemod, Abort);
	/*Check the error at the end of the function */
	if ((pParams->hDemod->Error) || (pParams->hTuner->Error))
		error = FE_LLA_I2C_ERROR;

	return error;
}

/*****************************************************
--FUNCTION	::	FE_STV0913_Search
--ACTION	::	Search for a valid transponder
--PARAMS IN	::	Handle	==>	Front End Handle
				pSearch ==> Search parameters
				pResult ==> Result of the search
--PARAMS OUT::	NONE
--RETURN	::	Error(if any)
--***************************************************/
FE_STV0913_Error_t FE_STV0913_Search(FE_STV0913_Handle_t Handle,
				     FE_STV0913_SearchParams_t *pSearch,
				     FE_STV0913_SearchResult_t *pResult,
				     U32 Satellite_Scan)
{
	FE_STV0913_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0913_InternalParams_t *pParams = NULL;
	FE_STV0913_SIGNALTYPE_t signalType = FE_SAT_NOCARRIER;

#ifdef HOST_PC
#ifndef NO_GUI
	double lockTime;
	char message[200];
	lockTime = Timer();
#endif
#endif

	if (Handle == NULL)
		return FE_LLA_INVALID_HANDLE;

	pParams = (FE_STV0913_InternalParams_t *) Handle;

	if ((pParams->hDemod->Error) || (pParams->hTuner->Error))
		return FE_LLA_I2C_ERROR;

	if ((INRANGE(100000, pSearch->SymbolRate, 70000000)) &&
	    (INRANGE(500000, pSearch->SearchRange, 70000000))
	    ) {
		pParams->DemodSearchStandard = pSearch->Standard;
		pParams->DemodSymbolRate = pSearch->SymbolRate;
		pParams->DemodSearchRange = pSearch->SearchRange;
		pParams->TunerFrequency = pSearch->Frequency;
		pParams->DemodSearchAlgo = pSearch->SearchAlgo;
		pParams->DemodSearch_IQ_Inv = pSearch->IQ_Inversion;
		pParams->DemodPunctureRate = pSearch->PunctureRate;
		pParams->DemodModulation = pSearch->Modulation;
		pParams->DemodModcode = pSearch->Modcode;

		signalType = FE_STV0913_Algo(pParams, Satellite_Scan);
		/*pSearch->TunerIndexJump = pParams->TunerIndexJump; */

		if (signalType == FE_SAT_TUNER_JUMP) {
			error = FE_LLA_TUNER_JUMP;
		} else if (signalType == FE_SAT_TUNER_NOSIGNAL) {
			/* half of the tuner bandwith jump */
			error = FE_LLA_TUNER_NOSIGNAL;
		} else
		    if (((signalType == FE_SAT_RANGEOK)
			 || ((Satellite_Scan > 0)
			     && (signalType == FE_SAT_NODATA)))
			&& (pParams->hDemod->Error == CHIPERR_NO_ERROR)) {
			if ((Satellite_Scan > 0)
			    && (signalType == FE_SAT_NODATA)
			    && (pParams->DemodResults.Standard ==
				FE_SAT_DVBS2_STANDARD)) {
				error = FE_LLA_NODATA;
#ifdef HOST_PC
#ifndef NO_GUI
				Fmt(message, "Nodata Detect");
				ReportInsertMessage(message);
#endif
#endif
			} else {
				error = FE_LLA_NO_ERROR;
			}
			pResult->Locked = pParams->DemodResults.Locked;

			/* update results */
			pResult->Standard = pParams->DemodResults.Standard;
			pResult->Frequency = pParams->DemodResults.Frequency;
			pResult->SymbolRate = pParams->DemodResults.SymbolRate;
			pResult->PunctureRate =
			    pParams->DemodResults.PunctureRate;
			pResult->ModCode = pParams->DemodResults.ModCode;
			pResult->Pilots = pParams->DemodResults.Pilots;
			pResult->FrameLength =
			    pParams->DemodResults.FrameLength;
			pResult->Spectrum = pParams->DemodResults.Spectrum;
			pResult->RollOff = pParams->DemodResults.RollOff;
			pResult->Modulation = pParams->DemodResults.Modulation;

		} else {
			pResult->Locked = FALSE;

			switch (pParams->DemodError) {
			case FE_LLA_I2C_ERROR:	/*I2C error */
				error = FE_LLA_I2C_ERROR;
				break;

			case FE_LLA_NO_ERROR:
			default:
				error = FE_LLA_SEARCH_FAILED;
				break;
			}
		}
		/*Check the error at the end of the function */
		if ((pParams->hDemod->Error) || (pParams->hTuner->Error))
			error = FE_LLA_I2C_ERROR;
	}
return error;
}

/*****************************************************
--FUNCTION	::	FE_STV0913_DVBS2_SetGoldCode
--ACTION	::	Set the DVBS2 Gold Code
--PARAMS IN	::	Handle	==>	Front End Handle
				U32		==>	cold code value(18bits)
--PARAMS OUT::	NONE
--RETURN	::	Error(if any)
--***************************************************/
FE_STV0913_Error_t FE_STV0913_DVBS2_SetGoldCodeX(FE_STV0913_Handle_t Handle,
						 U32 GoldCode)
{
	FE_STV0913_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0913_InternalParams_t *pParams = NULL;

	if ((void *)Handle == NULL)
		return FE_LLA_INVALID_HANDLE;

	pParams = (FE_STV0913_InternalParams_t *) Handle;

	if ((pParams->hDemod->Error) || (pParams->hTuner->Error))
		return FE_LLA_I2C_ERROR;

	/*Gold code X setting */
	/* bit[3:4] of register PLROOT2
	   3..2 : P1_PLSCRAMB_MODE[1:0] |URW|
	   entry mode p1_plscramb_root
	   00: p1_plscramb_root is the root of PRBS X.
	   01: p1_plscramb_root is the DVBS2 gold code.
	 */

	ChipSetOneRegister(pParams->hDemod, RSTV0913_PLROOT2,
			   0x04 | ((GoldCode >> 16) & 0x3));
	ChipSetOneRegister(pParams->hDemod, RSTV0913_PLROOT1,
			   (GoldCode >> 8) & 0xff);
	ChipSetOneRegister(pParams->hDemod, RSTV0913_PLROOT0,
			   (GoldCode) & 0xff);
	/*Check the error at the end of the function */
	if ((pParams->hDemod->Error) || (pParams->hTuner->Error))
		error = FE_LLA_I2C_ERROR;

	return error;
}

#ifdef STM_FE_DEFINED_FOR_FUTURE_USE
/***********************************************************************
**FUNCTION	::	FE_STV0913_SetupFSK
**ACTION	::	Setup FSK
**PARAMS IN	::	hChip			  -> handle to the chip
**fskt_carrier	  -> FSK modulator carrier frequency  (Hz)
**fskt_deltaf		-> FSK frequency deviation		  (Hz)
**fskr_carrier	  -> FSK demodulator carrier frequency(Hz)
**PARAMS OUT::	NONE
**RETURN	::	Symbol frequency
***********************************************************************/
FE_STV0913_Error_t FE_STV0913_SetupFSK(FE_STV0913_Handle_t Handle,
				       U32 TransmitCarrier, U32 ReceiveCarrier,
				       U32 Deltaf)
{
	FE_STV0913_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0913_InternalParams_t *pParams = NULL;
	U32 fskt_carrier, fskt_deltaf, fskr_carrier;

	if ((void *)Handle == NULL)
		return FE_LLA_INVALID_HANDLE;

	pParams = (FE_STV0913_InternalParams_t *) Handle;

	if ((pParams->hDemod->Error) || (pParams->hTuner->Error))
		return FE_LLA_I2C_ERROR;

	ChipSetField(pParams->hDemod, FSTV0913_FSKT_KMOD, 0x23);

	/*
	   Formulat:
	   FSKT_CAR = 2^20*transmit_carrier/MasterClock;

	 */
	fskt_carrier = (TransmitCarrier << 8) / (pParams->MasterClock >> 12);

	ChipSetField(pParams->hDemod, FSTV0913_FSKT_CAR2,
		     ((fskt_carrier >> 16) & 0x03));
	ChipSetField(pParams->hDemod, FSTV0913_FSKT_CAR1,
		     ((fskt_carrier >> 8) & 0xFF));
	ChipSetField(pParams->hDemod, FSTV0913_FSKT_CAR0,
		     (fskt_carrier & 0xFF));

	/*
	   Formulat:
	   FSKT_DELTAF = 2^20*fskt_deltaf/MasterClock;

	 */

	fskt_deltaf = (Deltaf << 20) / pParams->MasterClock;/* 2^20 = 1048576 */

	ChipSetField(pParams->hDemod, FSTV0913_FSKT_DELTAF1,
		     ((fskt_deltaf >> 8) & 0x0F));
	ChipSetField(pParams->hDemod, FSTV0913_FSKT_DELTAF0,
		     (fskt_deltaf & 0xFF));

	ChipSetOneRegister(pParams->hDemod, RSTV0913_FSKTCTRL, 0x04);

	ChipSetOneRegister(pParams->hDemod, RSTV0913_FSKRFC2, 0x10);
	/*
	   Formulat:
	   FSKR_CAR = 2^20*receive_carrier/MasterClock;

	 */

	fskr_carrier = (ReceiveCarrier << 8) / (pParams->MasterClock >> 12);

	ChipSetField(pParams->hDemod, FSTV0913_FSKR_CAR2,
		     ((fskr_carrier >> 16) & 0x03));
	ChipSetField(pParams->hDemod, FSTV0913_FSKR_CAR1,
		     ((fskr_carrier >> 8) & 0xFF));
	ChipSetField(pParams->hDemod, FSTV0913_FSKR_CAR0,
		     (fskr_carrier & 0xFF));

	ChipSetOneRegister(pParams->hDemod, RSTV0913_FSKRK1, 0x53);
	ChipSetOneRegister(pParams->hDemod, RSTV0913_FSKRK2, 0x94);

	ChipSetOneRegister(pParams->hDemod, RSTV0913_FSKRAGCR, 0x28);
	ChipSetOneRegister(pParams->hDemod, RSTV0913_FSKRAGC, 0x5F);

	ChipSetOneRegister(pParams->hDemod, RSTV0913_FSKRALPHA, 0x13);
	ChipSetOneRegister(pParams->hDemod, RSTV0913_FSKRPLTH1, 0x90);
	ChipSetOneRegister(pParams->hDemod, RSTV0913_FSKRPLTH0, 0x45);

	ChipSetOneRegister(pParams->hDemod, RSTV0913_FSKRDF1, 0x9f);
	/*
	   Formulat:
	   FSKR_DELTAF = 2^20*fskt_deltaf/MasterClock;

	 */

	ChipSetField(pParams->hDemod, FSTV0913_FSKR_DELTAF1,
		     ((fskt_deltaf >> 8) & 0x0F));
	ChipSetField(pParams->hDemod, FSTV0913_FSKR_DELTAF0,
		     (fskt_deltaf & 0xFF));

	ChipSetOneRegister(pParams->hDemod, RSTV0913_FSKRSTEPP, 0x02);
	ChipSetOneRegister(pParams->hDemod, RSTV0913_FSKRSTEPM, 0x4A);

	ChipSetOneRegister(pParams->hDemod, RSTV0913_FSKRDET1, 0);
	ChipSetOneRegister(pParams->hDemod, RSTV0913_FSKRDET0, 0x2F);

	ChipSetOneRegister(pParams->hDemod, RSTV0913_FSKRDTH1, 0);
	ChipSetOneRegister(pParams->hDemod, RSTV0913_FSKRDTH0, 0x55);

	ChipSetOneRegister(pParams->hDemod, RSTV0913_FSKRLOSS, 0x1F);
		/*Check the error at the end of the function */
	if ((pParams->hDemod->Error) || (pParams->hTuner->Error))
		error = FE_LLA_I2C_ERROR;

	return error;
}

/***********************************************************
**FUNCTION	::	FE_STV0913_EnableFSK
**ACTION	::	Enable - Disable FSK modulator /Demodulator
**PARAMS IN	::	hChip	 -> handle to the chip
**				mod_en	 -> FSK modulator on/off
**				demod_en -> FSK demodulator on/off
**PARAMS OUT::	NONE
**RETURN	::	Error if any
***********************************************************/
FE_STV0913_Error_t FE_STV0913_EnableFSK(FE_STV0913_Handle_t Handle,
					BOOL EnableModulation,
					BOOL EnableDemodulation)
{
	FE_STV0913_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0913_InternalParams_t *pParams = NULL;

	if ((void *)Handle == NULL)
		return FE_LLA_INVALID_HANDLE;

	pParams = (FE_STV0913_InternalParams_t *) Handle;

	if ((pParams->hDemod->Error) || (pParams->hTuner->Error))
		return FE_LLA_I2C_ERROR;

	if (EnableDemodulation == TRUE)
		ChipSetField(pParams->hDemod, FSTV0913_FSKT_MOD_EN, 1);
	else
		ChipSetField(pParams->hDemod, FSTV0913_FSKT_MOD_EN, 0);

	if (EnableDemodulation == FALSE)
		ChipSetField(pParams->hDemod, FSTV0913_FSK_PON, 0x01);
	else
		ChipSetField(pParams->hDemod, FSTV0913_FSK_PON, 0x00);
	/*Check the error at the end of the function */
	if ((pParams->hDemod->Error) || (pParams->hTuner->Error))
		error = FE_LLA_I2C_ERROR;

	return error;
}
#endif

/*****************************************************
--FUNCTION	::	FE_STV0913_DiseqcTxInit
--ACTION	::	Set the diseqC Tx mode
--PARAMS IN	::	Handle	==>	Front End Handle
::	DiseqCMode	==> diseqc Mode : continues tone, 2/3 PWM, 3/3 PWM,
						2/3 envelop or 3/3 envelop.
--PARAMS OUT::None
--RETURN	::	Error(if any)
--***************************************************/
FE_STV0913_Error_t FE_STV0913_DiseqcInit(FE_STV0913_Handle_t Handle,
					 FE_STV0913_DiseqC_TxMode DiseqCMode)
{
	FE_STV0913_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0913_InternalParams_t *pParams = NULL;

	pParams = (FE_STV0913_InternalParams_t *) Handle;
	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;

	if ((pParams->hDemod->Error) || (pParams->hTuner->Error))
		return FE_LLA_I2C_ERROR;

	ChipSetField(pParams->hDemod, FSTV0913_DISEQC_MODE, DiseqCMode);

	ChipSetField(pParams->hDemod, FSTV0913_DISTX_RESET, 1);
	ChipSetField(pParams->hDemod, FSTV0913_DISTX_RESET, 0);

	if ((pParams->hDemod->Error) || (pParams->hTuner->Error))
		error = FE_LLA_I2C_ERROR;

	return error;
}
EXPORT_SYMBOL(FE_STV0913_DiseqcInit);

/*****************************************************
--FUNCTION	::	FE_STV0913_DiseqcSend
--ACTION	::	Read receved bytes from DiseqC FIFO
--PARAMS IN	::	Handle	==>	Front End Handle
			::	Data	==> Table of bytes to send.
			::	NbData	==> Number of bytes to send.
--PARAMS OUT::None
--RETURN	::	Error(if any)
--***************************************************/
FE_STV0913_Error_t FE_STV0913_DiseqcSend(FE_STV0913_Handle_t Handle, U8 *Data,
					 U32 NbData)
{
	FE_STV0913_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0913_InternalParams_t *pParams = NULL;
	S32 i = 0;

	pParams = (FE_STV0913_InternalParams_t *) Handle;
	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;

	if ((pParams->hDemod->Error) || (pParams->hTuner->Error))
		return FE_LLA_I2C_ERROR;

	/*RX is OFF during tx for cut1.0 */
	ChipSetField(pParams->hDemod, FSTV0913_DISRX_ON, 0);

	ChipSetField(pParams->hDemod, FSTV0913_DIS_PRECHARGE, 1);
	while (i < NbData) {
		/* wait for FIFO empty    */
		while (ChipGetField(pParams->hDemod, FSTV0913_TX_FIFO_FULL))
			;
		/* send byte to FIFO :: WARNING don't use set field   !! */
		ChipSetOneRegister(pParams->hDemod, RSTV0913_DISTXFIFO,
								Data[i]);
		i++;
	}
	ChipSetField(pParams->hDemod, FSTV0913_DIS_PRECHARGE, 0);
	i = 0;
	while ((ChipGetField(pParams->hDemod, FSTV0913_TX_IDLE) != 1)
	       && (i < 10)) {
		/*wait until the end of diseqc send operation */
		ChipWaitOrAbort(pParams->hDemod, 10);
		i++;
	}

	ChipSetField(pParams->hDemod, FSTV0913_DISRX_ON, 1);/*RX ON after tx */

	if ((pParams->hDemod->Error) || (pParams->hTuner->Error))
		error = FE_LLA_I2C_ERROR;

	return error;
}
EXPORT_SYMBOL(FE_STV0913_DiseqcSend);

/*****************************************************
--FUNCTION	::	FE_STV0913_DiseqcReceive
--ACTION	::	Read receved bytes from DiseqC FIFO
--PARAMS IN	::	Handle	==>	Front End Handle
--PARAMS OUT::	Data	==> Table of received bytes.
			::	NbData	==> Number of received bytes.
--RETURN	::	Error(if any)
--***************************************************/
FE_STV0913_Error_t FE_STV0913_DiseqcReceive(FE_STV0913_Handle_t Handle,
					    U8 *Data, U32 *NbData)
{
	FE_STV0913_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0913_InternalParams_t *pParams = NULL;
	S32 i = 0, j = 0;

	pParams = (FE_STV0913_InternalParams_t *) Handle;
	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;

	if ((pParams->hDemod->Error) || (pParams->hTuner->Error))
		return FE_LLA_I2C_ERROR;

	ChipSetField(pParams->hDemod, FSTV0913_EXTENVELOP, 0);
	ChipSetField(pParams->hDemod, FSTV0913_IGNORE_SHORT22K, 1);

	*NbData = 0;

	ChipGetField(pParams->hDemod, FSTV0913_RXACTIVE);

	while ((ChipGetField(pParams->hDemod, FSTV0913_RXEND) != 1) &&
								(i < 10)) {
		ChipWaitOrAbort(pParams->hDemod, 10);
		/*wait while the RX is activity */
		while ((ChipGetField(pParams->hDemod, FSTV0913_RXACTIVE) == 1)
		       && (j < 10)) {
			ChipWaitOrAbort(pParams->hDemod, 5);
			j++;

		}

		i++;
	}
	*NbData = ChipGetField(pParams->hDemod, FSTV0913_RXFIFO_BYTES);
	for (i = 0; i < (*NbData); i++) {
		Data[i] =
		    ChipGetOneRegister(pParams->hDemod, RSTV0913_DISRXFIFO);
		/*printf("\n*************RECEIVEd = 0x%x\n", Data[i]); */
	}
	/*Check the error at the end of the function */
	if ((pParams->hDemod->Error) || (pParams->hTuner->Error))
		error = FE_LLA_I2C_ERROR;

	return error;
}
EXPORT_SYMBOL(FE_STV0913_DiseqcReceive);

/*****************************************************
--FUNCTION	::	FE_STV0913_Set22KHzContinues
--ACTION	::	Set the diseqC Tx mode
--PARAMS IN	::	Handle	==>	Front End Handle
--PARAMS OUT::None
--RETURN	::	Error(if any)
--***************************************************/
FE_STV0913_Error_t FE_STV0913_Set22KHzContinues(FE_STV0913_Handle_t Handle,
						BOOL Enable)
{
	FE_STV0913_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0913_InternalParams_t *pParams = NULL;

	pParams = (FE_STV0913_InternalParams_t *) Handle;
	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;

	if ((pParams->hDemod->Error) || (pParams->hTuner->Error))
		return FE_LLA_I2C_ERROR;

	return error;
}

#if 0
FE_STV0913_Error_t FE_STV0913_SetContinous_Tone(FE_STV0913_Handle_t Handle,
						STFRONTEND_LNBToneState_t
						tonestate)
{
	FE_STV0913_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0913_InternalParams_t *pParams = NULL;
	pParams = (FE_STV0913_InternalParams_t *) Handle;

	if (pParams != NULL) {

		if (tonestate != STFRONTEND_LNB_TONE_OFF) {
			/*continous 22 KHZ tone. */
			ChipSetField(pParams->hDemod,
						FSTV0913_DISEQC_MODE, 0x0);
		} else {
			/*for no tone or tone off. */
			ChipSetField(pParams->hDemod,
						FSTV0913_DISEQC_MODE, 0x2);
		}

	}
	return error;
}
#endif
/*****************************************************
--FUNCTION	::	FE_STV0913_Term
--ACTION	::	Terminate STV0913 chip connection
--PARAMS IN	::	Handle	==>	Front End Handle
--PARAMS OUT::	NONE
--RETURN	::	Error(if any)
--***************************************************/
FE_STV0913_Error_t FE_STV0913_Term(FE_STV0913_Handle_t Handle)
{
	FE_STV0913_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0913_InternalParams_t *pParams = NULL;

	pParams = (FE_STV0913_InternalParams_t *) Handle;

	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;
#ifdef HOST_PC
#ifdef NO_GUI
	ChipClose(pParams->hDemod);
	FE_Sat_TunerTerm(pParams->hTuner);
#endif

	if (Handle)
		free(pParams);
#endif

	return error;
}

/******************************************/
