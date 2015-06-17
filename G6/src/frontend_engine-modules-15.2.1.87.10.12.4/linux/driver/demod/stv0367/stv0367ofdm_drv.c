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

Source file name : stv0367ofdm_drv.c
Author :           LLA

367 lla drv file

Date        Modification                                    Name
----        ------------                                    --------
01-Aug-11   Created
04-Jul-12   Updated to v3.5
************************************************************************/
#ifndef ST_OSLINUX
/* #include  <stdlib.h> */
#endif
/* #include <chip.h> */
#ifdef HOST_PC
#include <I2C.h>
#else
/* #include <sti2c.h> */
#endif

/*	367 includes	*/
#if defined(HOST_PC) && !defined(NO_GUI)
#include <367ofdm_GUI.h>
#include <Appl.h>
FILE *fp;
#include <Pnl_Report.h>
#include <367ofdm_Usr.h>
#include <formatio.h>
#endif

#ifdef CHIP_STAPI
/* #include <globaldefs.h> */
/* #include <feter_commlla_str.h> */
/* #include <fecab_commlla_str.h> */
/* #include <fe_tc_tuner.h> */
#define UTIL_Delay(micro_sec) task_delay((unsigned int)(((micro_sec) + 999) / \
				1000) * ST_GetClocksPerSecond() / 1000)
#define SystemWaitFor(x) UTIL_Delay((x*1000))
#define Delay(x) task_delay((unsigned int)((ST_GetClocksPerSecond()*x)/1000));
#endif

#ifdef HOST_PC
/*	generic includes	*/
#include <utility.h>
#include <ansi_c.h>
#include <gen_macros.h>
#include <gen_types.h>
#include <gen_csts.h>
#endif

#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/platform_device.h>
#include <linux/stm/plat_dev.h>
#include <linux/stm/demod.h>
#include <linux/stm/lnb.h>
#include <linux/stm/diseqc.h>
#include <gen_macros.h>
#include <stm_registry.h>
#include <stm_fe.h>
#include <stfe_utilities.h>
#include <stm_fe_os.h>
#include <i2c_wrapper.h>
#include <stm_fe_demod.h>
/* #include <stm_fe_diseqc.h> */
#include "stv0367ofdm_init.h"
#include "stv0367ofdm_drv.h"
#include <fe_tc_tuner.h>

#define NINV 0
#define INV 1

/* dcdc #define CPQ_LIMIT 23*/
#define CPQ_LIMIT 23

/* Current LLA revision	*/
static const ST_Revision_t Revision367ofdm = "STV367ofdm-LLA_REL_3.5";
/*global variables */
/*U32 PreviousBitErrorRate=0;
U32 PreviousPacketErrorRate=0;*/
#if defined(HOST_PC) && !defined(NO_GUI)
U8 IIR_Filter_Freq_setting;
#endif

/*local functions definition*/
static int FE_367TER_FilterCoeffInit(STCHIP_Handle_t hChip,
			U16 CellsCoeffs[2][6][5], U32 DemodXtal);
static void FE_367TER_AGC_IIR_LOCK_DETECTOR_SET(STCHIP_Handle_t hChip);
#if defined(HOST_PC) && defined(NO_GUI)
static int FE_367TER_IIR_FILTER_INIT(STCHIP_Handle_t hChip, U8 Bandwidth,
			      U32 DemodXtalValue);
#endif
static void FE_367TER_AGC_IIR_RESET(STCHIP_Handle_t hChip);
static FE_367dvbt_SIGNALTYPE_t FE_367TER_CheckSYR(STCHIP_Handle_t hChip);
static FE_367dvbt_SIGNALTYPE_t FE_367TER_CheckCPAMP(STCHIP_Handle_t hChip,
					     S32 FFTmode);
static FE_367dvbt_SIGNALTYPE_t FE_367ofdm_LockAlgo(FE_367ofdm_InternalParams_t *
					    pParams);
static void FE_STV0367ofdm_SetTS_Parallel_Serial(STCHIP_Handle_t hChip,
					  FE_TS_OutputMode_t PathTS);
static void FE_STV0367ofdm_SetCLK_Polarity(STCHIP_Handle_t hChip,
				    FE_TS_ClockPolarity_t clock);
static FE_LLA_Error_t FE_367ofdm_Algo(FE_367ofdm_Handle_t Handle,
			       FE_TER_SearchParams_t *pSearch,
			       FE_TER_SearchResult_t *pResult);

U16 CellsCoeffs_8MHz_367cofdm[3][6][5] = {
	{
	 /* InternalFreq==54000000 */
	 {0x10EF, 0xE205, 0x10EF, 0xCE49, 0x6DA7},
	 {0x2151, 0xc557, 0x2151, 0xc705, 0x6f93},
	 {0x2503, 0xc000, 0x2503, 0xc375, 0x7194},
	 {0x20E9, 0xca94, 0x20e9, 0xc153, 0x7194},
	 {0x06EF, 0xF852, 0x06EF, 0xC057, 0x7207},
	 {0x0000, 0x0ECC, 0x0ECC, 0x0000, 0x3647}
	 },
	{
	 /* InternalFreq==53125000 */
	 {0x10A0, 0xE2AF, 0x10A1, 0xCE76, 0x6D6D},
	 {0x20DC, 0xC676, 0x20D9, 0xC80A, 0x6F29},
	 {0x2532, 0xC000, 0x251D, 0xC391, 0x706F},
	 {0x1F7A, 0xCD2B, 0x2032, 0xC15E, 0x711F},
	 {0x0698, 0xFA5E, 0x0568, 0xC059, 0x7193},
	 {0x0000, 0x0918, 0x149C, 0x0000, 0x3642}
	 },
	{
	 /* InternalFreq==52500000 */
	 {0x0000, 0x0000, 0x0000, 0x0000, 0x0000},
	 {0x0000, 0x0000, 0x0000, 0x0000, 0x0000},
	 {0x0000, 0x0000, 0x0000, 0x0000, 0x0000},
	 {0x0000, 0x0000, 0x0000, 0x0000, 0x0000},
	 {0x0000, 0x0000, 0x0000, 0x0000, 0x0000},
	 {0x0000, 0x0000, 0x0000, 0x0000, 0x0000}
	 }

};

U16 CellsCoeffs_7MHz_367cofdm[3][6][5] = {
	{
	 /*InternalFreq==54000000 */
	 {0x12CA, 0xDDAF, 0x12CA, 0xCCEB, 0x6FB1},
	 {0x2329, 0xC000, 0x2329, 0xC6B0, 0x725F},
	 {0x2394, 0xC000, 0x2394, 0xC2C7, 0x7410},
	 {0x251C, 0xC000, 0x251C, 0xC103, 0x74D9},
	 {0x0804, 0xF546, 0x0804, 0xC040, 0x7544},
	 {0x0000, 0x0CD9, 0x0CD9, 0x0000, 0x370A}
	 },
	{
	 /*InternalFreq==53125000 */
	 {0x1285, 0xDE47, 0x1285, 0xCD17, 0x6F76},
	 {0x234C, 0xC000, 0x2348, 0xC6DA, 0x7206},
	 {0x23B4, 0xC000, 0x23AC, 0xC2DB, 0x73B3},
	 {0x253D, 0xC000, 0x25B6, 0xC10B, 0x747F},
	 {0x0721, 0xF79C, 0x065F, 0xC041, 0x74EB},
	 {0x0000, 0x08FA, 0x1162, 0x0000, 0x36FF}
	 },
	{
	 /* InternalFreq==52500000 */
	 {0x0000, 0x0000, 0x0000, 0x0000, 0x0000},
	 {0x0000, 0x0000, 0x0000, 0x0000, 0x0000},
	 {0x0000, 0x0000, 0x0000, 0x0000, 0x0000},
	 {0x0000, 0x0000, 0x0000, 0x0000, 0x0000},
	 {0x0000, 0x0000, 0x0000, 0x0000, 0x0000},
	 {0x0000, 0x0000, 0x0000, 0x0000, 0x0000}
	 }

};

U16 CellsCoeffs_6MHz_367cofdm[3][6][5] = {
	{
	 /*InternalFreq==54000000 */
	 {0x1699, 0xD5B8, 0x1699, 0xCBC3, 0x713B},
	 {0x2245, 0xC000, 0x2245, 0xC568, 0x74D5},
	 {0x227F, 0xC000, 0x227F, 0xC1FC, 0x76C6},
	 {0x235E, 0xC000, 0x235E, 0xC0A7, 0x778A},
	 {0x0ECB, 0xEA0B, 0x0ECB, 0xC027, 0x77DD},
	 {0x0000, 0x0B68, 0x0B68, 0x0000, 0xC89A},
	 },
	{
	 /*InternalFreq==53125000 */
	 {0x1655, 0xD64E, 0x1658, 0xCBEF, 0x70FE},
	 {0x225E, 0xC000, 0x2256, 0xC589, 0x7489},
	 {0x2293, 0xC000, 0x2295, 0xC209, 0x767E},
	 {0x2377, 0xC000, 0x23AA, 0xC0AB, 0x7746},
	 {0x0DC7, 0xEBC8, 0x0D07, 0xC027, 0x7799},
	 {0x0000, 0x0888, 0x0E9C, 0x0000, 0x3757}

	 },
	{
	 /* InternalFreq==52500000 */
	 {0x0000, 0x0000, 0x0000, 0x0000, 0x0000},
	 {0x0000, 0x0000, 0x0000, 0x0000, 0x0000},
	 {0x0000, 0x0000, 0x0000, 0x0000, 0x0000},
	 {0x0000, 0x0000, 0x0000, 0x0000, 0x0000},
	 {0x0000, 0x0000, 0x0000, 0x0000, 0x0000},
	 {0x0000, 0x0000, 0x0000, 0x0000, 0x0000}
	 }

};

/*********************************************************
--FUNCTION	::	FE_367TER_FilterCoeffInit
--ACTION	::	Apply filter coeffs values

--PARAMS IN	::	Handle to the Chip
				CellsCoeffs[3][6][5]
--PARAMS OUT::	NONE
--RETURN	::  0 error , 1 no error
--********************************************************/
int FE_367TER_FilterCoeffInit(STCHIP_Handle_t hChip, U16 CellsCoeffs[3][6][5],
			      U32 DemodXtal)
{

	int i, j, k, InternalFreq;
	if (!hChip) {
#if defined(HOST_PC) && !defined(NO_GUI)
		ReportInsertMessage("NRST_IIR uncorrectly set 2");
#endif
		return 0;
	}
	InternalFreq = FE_367ofdm_GetMclkFreq(hChip, DemodXtal);

	if (InternalFreq == 53125000)
		k = 1;	/* equivalent to Xtal 25M on 362 */
	else if (InternalFreq == 54000000)
		k = 0;	/* equivalent to Xtal 27M on 362 */
	else if (InternalFreq == 52500000)
		k = 2;	/* equivalent to Xtal 30M on 362 */
	else {
#if defined(HOST_PC) && !defined(NO_GUI)
		ReportInsertMessage("NRST_IIR uncorrectly set 1");
#endif
		return 0;
	}

	for (i = 1; i <= 6; i++) {
		ChipSetField(hChip, F367ofdm_IIR_CELL_NB, i - 1);

		for (j = 1; j <= 5; j++) {
			ChipSetOneRegister(hChip,
				(R367ofdm_IIRCX_COEFF1_MSB + 2 * (j - 1)),
				MSB(CellsCoeffs[k][i - 1][j - 1]));
			ChipSetOneRegister(hChip,
				(R367ofdm_IIRCX_COEFF1_LSB + 2 * (j - 1)),
				LSB(CellsCoeffs[k][i - 1][j - 1]));
		}
	}
	return 1;
}

/**********************************************************/
/*********************************************************
--FUNCTION	::	FE_367TER_SpeedInit
--ACTION	::	calculate I2C speed (for SystemWait ...)

--PARAMS IN	::	Handle to the Chip
--PARAMS OUT::	NONE
--RETURN	::	#ms for an I2C reading access ..
--********************************************************/
int FE_367TER_SpeedInit(STCHIP_Handle_t hChip)
{
#ifdef HOST_PC
	unsigned int i;
	int tempo;

	tempo = clock();
	for (i = 0; i < 16; i++)
		ChipGetField(hChip, F367ofdm_EPQ1);

	tempo = clock() - tempo;
	tempo = (tempo * 1000) / CLOCKS_PER_SEC;
	 /* 4 not 8 to avoid too rough rounding */
	tempo = (tempo + 4) >> 4;

	return tempo;
#else
	unsigned int i, tempo;
	clock_t time1, time2, time_diff;

	time1 = stm_fe_time_now();
	for (i = 0; i < 16; i++)
		ChipGetField(hChip, F367ofdm_EPQ1);
	time2 = stm_fe_time_now();
	time_diff = stm_fe_time_minus(time2, time1);
	 /* Duration in milliseconds, + 4 for rounded value */
	tempo = (time_diff * 1000) / stm_fe_getclocks_persec() + 4;
	tempo = tempo << 4;
	return tempo;
#endif

}

#if defined(HOST_PC) && !defined(NO_GUI)
/***** SET TRLNOMRATE REGISTERS *******************************/
/*****************************************************
--FUNCTION		::	SET_TRLNOMRATE_REGS
--ACTION		::	Sets TRL nominal rate reg
--PARAMS IN		::	Handle to the Chip, value
--PARAMS OUT	::	None
--***************************************************/

void SET_TRLNOMRATE_REGS(STCHIP_Handle_t hChip, short unsigned int value)
{
	div_t divi;

	divi = div(value, 512);
	ChipSetField(hChip, F367ofdm_TRL_NOMRATE_HI, divi.quot);
	ChipSetField(hChip, F367ofdm_TRL_NOMRATE_LO, (divi.rem / 2));
	ChipSetField(hChip, F367ofdm_TRL_NOMRATE_LSB, (divi.rem % 2));

}

/*****************************************************
--FUNCTION		::	GET_TRLNOMRATE_VALUE
--ACTION		::	Gets TRL nominal rate reg
--PARAMS IN		::	Handle to the Chip
--PARAMS OUT	::	Value
--***************************************************/

short unsigned int GET_TRLNOMRATE_VALUE(STCHIP_Handle_t hChip)
{
	short unsigned int value;
	value =
	    ChipGetField(hChip, F367ofdm_TRL_NOMRATE_HI) * 512 +
	    ChipGetField(hChip, F367ofdm_TRL_NOMRATE_LO) * 2 +
	    ChipGetField(hChip, F367ofdm_TRL_NOMRATE_LSB);

	return value;
}

/*****************************************************
--FUNCTION		::	GET_TRL_OFFSET
--ACTION		::	Gets TRL nominal rate offset
--PARAMS IN		::	Handle to the Chip
--PARAMS OUT	::	i_int
--***************************************************/

signed int GET_TRL_OFFSET(STCHIP_Handle_t hChip)
{
	unsigned int u_var;
	signed int s_var;
	signed int i_int;

/*	ChipSetField(hChip, F367ofdm_FREEZE, 1);	 */
	/*ChipGetRegisters(hChip, F367ofdm_TRL_NOMRATE_LO, 4); */
	ChipGetRegisters(hChip, R367ofdm_TRL_CTL, 5);
/*	ChipSetField(hChip, F367ofdm_FREEZE, 0);	    */

	s_var = 256 * ChipGetFieldImage(hChip, F367ofdm_TRL_TOFFSET_HI) +
	    ChipGetFieldImage(hChip, F367ofdm_TRL_TOFFSET_LO);
	if (s_var > 32768)
		s_var = s_var - 65536;

	u_var = (512 * ChipGetFieldImage(hChip, F367ofdm_TRL_NOMRATE_HI) +
	     ChipGetFieldImage(hChip, F367ofdm_TRL_NOMRATE_LO) * 2 +
	     ChipGetFieldImage(hChip, F367ofdm_TRL_NOMRATE_LSB));
	i_int = ((signed)(1000000 / u_var) * s_var) / 2048;

	return i_int;
}

#endif
/*****************************************************
--FUNCTION		::	FE_367TER_AGC_IIR_LOCK_DETECTOR_SET
--ACTION		::	Sets Good values for AGC IIR lock detector
--PARAMS IN		::	Handle to the Chip
--PARAMS OUT	::	None
--***************************************************/
void FE_367TER_AGC_IIR_LOCK_DETECTOR_SET(STCHIP_Handle_t hChip)
{

	ChipSetField(hChip, F367ofdm_LOCK_DETECT_LSB, 0x00);

	/* Lock detect 1 */
	ChipSetField(hChip, F367ofdm_LOCK_DETECT_CHOICE, 0x00);
	ChipSetField(hChip, F367ofdm_LOCK_DETECT_MSB, 0x06);
	ChipSetField(hChip, F367ofdm_AUT_AGC_TARGET_LSB, 0x04);

	/* Lock detect 2 */
	ChipSetField(hChip, F367ofdm_LOCK_DETECT_CHOICE, 0x01);
	ChipSetField(hChip, F367ofdm_LOCK_DETECT_MSB, 0x06);
	ChipSetField(hChip, F367ofdm_AUT_AGC_TARGET_LSB, 0x04);

	/* Lock detect 3 */
	ChipSetField(hChip, F367ofdm_LOCK_DETECT_CHOICE, 0x02);
	ChipSetField(hChip, F367ofdm_LOCK_DETECT_MSB, 0x01);
	ChipSetField(hChip, F367ofdm_AUT_AGC_TARGET_LSB, 0x00);

	/* Lock detect 4 */
	ChipSetField(hChip, F367ofdm_LOCK_DETECT_CHOICE, 0x03);
	ChipSetField(hChip, F367ofdm_LOCK_DETECT_MSB, 0x01);
	ChipSetField(hChip, F367ofdm_AUT_AGC_TARGET_LSB, 0x00);

}

/*****************************************************
--FUNCTION		::	FE_367TER_IIR_FILTER_INIT
--ACTION		::	Sets Good IIR Filters coefficients
--PARAMS IN		::	Handle to the Chip
					selected bandwidth
--PARAMS OUT	::	None
--***************************************************/
int FE_367TER_IIR_FILTER_INIT(STCHIP_Handle_t hChip, U8 Bandwidth,
			      U32 DemodXtalValue)
{

	if (hChip != NULL) {
		ChipSetField(hChip, F367ofdm_NRST_IIR, 0);

		switch (Bandwidth) {

		case 6:
			if (!FE_367TER_FilterCoeffInit
			    (hChip, CellsCoeffs_6MHz_367cofdm, DemodXtalValue))
				return 0;
			break;

		case 7:
			if (!FE_367TER_FilterCoeffInit
			    (hChip, CellsCoeffs_7MHz_367cofdm, DemodXtalValue))
				return 0;
			break;

		case 8:
			if (!FE_367TER_FilterCoeffInit
			    (hChip, CellsCoeffs_8MHz_367cofdm, DemodXtalValue))
				return 0;
			break;
		default:
#if defined(HOST_PC) && !defined(NO_GUI)
			ReportInsertMessage("NRST_IIR uncorrectly set 3");
#endif
			return 0;

		}

		ChipSetField(hChip, F367ofdm_NRST_IIR, 1);
		return 1;
	} else {
#if defined(HOST_PC) && !defined(NO_GUI)
		ReportInsertMessage("NRST_IIR uncorrectly set 4");
#endif
		return 0;
	}

}

/*****************************************************
--FUNCTION		::	FE_367TER_AGC_IIR_RESET
--ACTION		::	AGC reset procedure
--PARAMS IN		::	Handle to the Chip
--PARAMS OUT	::	None
--***************************************************/
void FE_367TER_AGC_IIR_RESET(STCHIP_Handle_t hChip)
{

	U8 com_n;

	com_n = ChipGetField(hChip, F367ofdm_COM_N);

	ChipSetField(hChip, F367ofdm_COM_N, 0x07);

	ChipSetField(hChip, F367ofdm_COM_SOFT_RSTN, 0x00);
	ChipSetField(hChip, F367ofdm_COM_AGC_ON, 0x00);

	ChipSetField(hChip, F367ofdm_COM_SOFT_RSTN, 0x01);
	ChipSetField(hChip, F367ofdm_COM_AGC_ON, 0x01);

	ChipSetField(hChip, F367ofdm_COM_N, com_n);

}

/*********************************************************
--FUNCTION	::	FE_367ofdm_duration
--ACTION	::	return a duration regarding mode
--PARAMS IN	::	mode, tempo1, tempo2, tempo3
--PARAMS OUT::	none
--********************************************************/
int FE_367ofdm_duration(S32 mode, int tempo1, int tempo2, int tempo3)
{
	int local_tempo = 0;
	switch (mode) {
	case 0:
		local_tempo = tempo1;
		break;
	case 1:
		local_tempo = tempo2;
		break;

	case 2:
		local_tempo = tempo3;
		break;

	default:
#if defined(HOST_PC) && !defined(NO_GUI)
		ReportInsertMessage("mode uncorrectly set");
		local_tempo = 0;
#endif
		break;
	}
/*	WAIT_N_MS(local_tempo);  */
	return local_tempo;
}

/*********************************************************
--FUNCTION	::	FE_367TER_CheckSYR
--ACTION	::	Get SYR status
--PARAMS IN	::	Handle to the Chip
--PARAMS OUT::	CPAMP status
--********************************************************/
FE_367dvbt_SIGNALTYPE_t FE_367TER_CheckSYR(STCHIP_Handle_t hChip)
{
	int wd = 100;
	unsigned short int SYR_var;
	S32 SYRStatus;

	SYR_var = ChipGetField(hChip, F367ofdm_SYR_LOCK);

	while ((!SYR_var) && (wd > 0)) {
		ChipWaitOrAbort(hChip, 2);
		wd -= 2;
		SYR_var = ChipGetField(hChip, F367ofdm_SYR_LOCK);
	}
	if (!SYR_var) {
#if defined(HOST_PC) && !defined(NO_GUI)
		ReportInsertMessage("SYR_LOCK failed");
#endif
		SYRStatus = FE_TER_NOSYMBOL;
	} else
		SYRStatus = FE_TER_SYMBOLOK;
	return SYRStatus;
}

/*********************************************************
--FUNCTION	::	FE_367TER_CheckCPAMP
--ACTION	::	Get CPAMP status
--PARAMS IN	::	Handle to the Chip
--PARAMS OUT::	CPAMP status
--********************************************************/
FE_367dvbt_SIGNALTYPE_t FE_367TER_CheckCPAMP(STCHIP_Handle_t hChip, S32 FFTmode)
{

	S32 CPAMPvalue = 0, CPAMPStatus, CPAMPMin;
	int wd = 0;

	switch (FFTmode) {
	case 0:		/*2k mode */
		CPAMPMin = 20;
		wd = 10;
		break;

	case 1:		/*8k mode */
		CPAMPMin = 80;
		wd = 55;
		break;

	case 2:		/*4k mode */
		CPAMPMin = 40;
		wd = 30;
		break;

	default:
#if defined(HOST_PC) && !defined(NO_GUI)
		ReportInsertMessage("SYR_MODE uncorrectly set");
#endif
		CPAMPMin = 0xffff;	/*drives to NOCPAMP   */
		break;
	}

	CPAMPvalue = ChipGetField(hChip, F367ofdm_PPM_CPAMP_DIRECT);
	while ((CPAMPvalue < CPAMPMin) && (wd > 0)) {
		ChipWaitOrAbort(hChip, 1);
		wd -= 1;
		CPAMPvalue = ChipGetField(hChip, F367ofdm_PPM_CPAMP_DIRECT);
		/*printf("CPAMPvalue= %d at wd=%d\n", CPAMPvalue, wd); */
	}
	/*printf("******last CPAMPvalue= %d at wd=%d\n", CPAMPvalue, wd); */
	if (CPAMPvalue < CPAMPMin) {
		CPAMPStatus = FE_TER_NOCPAMP;
#if defined(HOST_PC) && !defined(NO_GUI)
		ReportInsertMessage("CPAMP failed");
#endif
	} else
		CPAMPStatus = FE_TER_CPAMPOK;

	return CPAMPStatus;
}

/*****************************************************
--FUNCTION	::	FE_367ofdm_LockAlgo
--ACTION	::	Search for Signal, Timing, Carrier and then data at a
			given Frequency, in a given range:

--PARAMS IN	::	NONE
--PARAMS OUT::	NONE
--RETURN	::	Type of the founded signal (if any)

--REMARKS   ::  This function is supposed to replace FE_367ofdm_Algo according
--				to last findings on SYR block
--***************************************************/
FE_367dvbt_SIGNALTYPE_t FE_367ofdm_LockAlgo(FE_367ofdm_InternalParams_t *
					    pParams)
{

	TER_TUNER_Params_t hTunerParams = NULL;
	STCHIP_Handle_t hChip;
	FE_367dvbt_SIGNALTYPE_t ret_flag;
	short int wd, tempo;
	unsigned short int try, u_var1 = 0, u_var2 = 0, u_var3 = 0, u_var4 =
	    0, mode, guard;

#if defined(HOST_PC) && !defined(NO_GUI)
	/* char Registers[471] = {0};
	   char str_tmp[100];  */
#endif
	if ((FE_367ofdm_InternalParams_t *) pParams != NULL) {
		hTunerParams = (TER_TUNER_Params_t) pParams->hTuner->pData;
		hChip = pParams->hDemod;
		try = 0;
		do {
			ret_flag = FE_TER_LOCKOK;

			ChipSetField(hChip, F367ofdm_CORE_ACTIVE, 0);

			if (pParams->IF_IQ_Mode != 0)
				ChipSetField(hChip, F367ofdm_COM_N, 0x07);

			 /* suggested mode is 2k 1/4  */
			ChipSetField(hChip, F367ofdm_GUARD, 3);
			ChipSetField(hChip, F367ofdm_MODE, 0);
			ChipSetField(hChip, F367ofdm_SYR_TR_DIS, 0);
			ChipWaitOrAbort(hChip, 5);

			ChipSetField(hChip, F367ofdm_CORE_ACTIVE, 1);

			if (FE_367TER_CheckSYR(hChip) == FE_TER_NOSYMBOL) {
				return FE_TER_NOSYMBOL;
			} else {
				/* if chip locked on wrong mode first try, it
				 * must lock correctly second try *db */
				mode = ChipGetField(hChip, F367ofdm_SYR_MODE);
				if (FE_367TER_CheckCPAMP(hChip, mode) ==
				    FE_TER_NOCPAMP) {
					if (try == 0)
						ret_flag = FE_TER_NOCPAMP;

				}
			}

			try++;
		} while ((try < 2) && (ret_flag != FE_TER_LOCKOK));

		if ((mode != 0) && (mode != 1) && (mode != 2))
			return FE_TER_SWNOK;
		/*guard=ChipGetField(hChip, F367ofdm_SYR_GUARD); */

		/*reset fec an reedsolo FOR 367 only */
		ChipSetField(hChip, F367ofdm_RST_SFEC, 1);
		ChipSetField(hChip, F367ofdm_RST_REEDSOLO, 1);
		ChipWaitOrAbort(hChip, 1);
		ChipSetField(hChip, F367ofdm_RST_SFEC, 0);
		ChipSetField(hChip, F367ofdm_RST_REEDSOLO, 0);

		u_var1 = ChipGetField(hChip, F367ofdm_LK);
		u_var2 = ChipGetField(hChip, F367ofdm_PRF);
		u_var3 = ChipGetField(hChip, F367ofdm_TPS_LOCK);
		/* u_var4=ChipGetField(hChip, F367ofdm_TSFIFO_LINEOK); */

		wd = FE_367ofdm_duration(mode, 150, 600, 300);
		tempo = FE_367ofdm_duration(mode, 4, 16, 8);

		/*while (((!u_var1)||(!u_var2)||(!u_var3)||(!u_var4)) &&
								(wd>=0)) */
		while (((!u_var1) || (!u_var2) || (!u_var3)) && (wd >= 0)) {
			ChipWaitOrAbort(hChip, tempo);
			wd -= tempo;
			u_var1 = ChipGetField(hChip, F367ofdm_LK);
			u_var2 = ChipGetField(hChip, F367ofdm_PRF);
			u_var3 = ChipGetField(hChip, F367ofdm_TPS_LOCK);
			/*u_var4=ChipGetField(hChip, F367_TSFIFO_LINEOK); */
		}

		if (!u_var1) {
#if defined(HOST_PC) && !defined(NO_GUI)
			ReportInsertMessage("LK failed");
#endif
			return FE_TER_NOLOCK;
		}

		if (!u_var2) {
#if defined(HOST_PC) && !defined(NO_GUI)
			ReportInsertMessage("PRFOUND failed");
#endif
			return FE_TER_NOPRFOUND;
		}

		if (!u_var3) {
#if defined(HOST_PC) && !defined(NO_GUI)
			ReportInsertMessage("TPS_LOCK failed");
#endif
			return FE_TER_NOTPS;
		}

		guard = ChipGetField(hChip, F367ofdm_SYR_GUARD);
		ChipSetOneRegister(hChip, R367ofdm_CHC_CTL, 0x11);
		/*supress EPQ auto for SYR_GARD 1/16 or 1/32 and set channel
		 * predictor in automatic */
		switch (guard) {
		case 0:
		case 1:
			ChipSetField(hChip, F367ofdm_AUTO_LE_EN, 0);
			/*ChipSetOneRegister(hChip, R367ofdm_CHC_CTL1, 0x1); */
			ChipSetField(hChip, F367ofdm_SYR_FILTER, 0);
			break;
		case 2:
		case 3:
			ChipSetField(hChip, F367ofdm_AUTO_LE_EN, 1);
			/*ChipSetOneRegister(hChip, R367ofdm_CHC_CTL1, 0x11); */
			ChipSetField(hChip, F367ofdm_SYR_FILTER, 1);
			break;

		default:
#if defined(HOST_PC) && !defined(NO_GUI)
			ReportInsertMessage("SYR_GUARD uncorrectly set");
#endif
			return FE_TER_SWNOK;
		}

		/* apply Sfec workaround if 8K 64QAM CR!=1/2 */
		if ((ChipGetField(hChip, F367ofdm_TPS_CONST) == 2) &&
		    (mode == 1) &&
		    (ChipGetField(hChip, F367ofdm_TPS_HPCODE) != 0)) {
			ChipSetOneRegister(hChip, R367ofdm_SFDLYSETH, 0xc0);
			ChipSetOneRegister(hChip, R367ofdm_SFDLYSETM, 0x60);
			ChipSetOneRegister(hChip, R367ofdm_SFDLYSETL, 0x0);
		} else
			ChipSetOneRegister(hChip, R367ofdm_SFDLYSETH, 0x0);

		wd = FE_367ofdm_duration(mode, 125, 500, 250);
		u_var4 = ChipGetField(hChip, F367ofdm_TSFIFO_LINEOK);

		while ((!u_var4) && (wd >= 0)) {
			ChipWaitOrAbort(hChip, tempo);
			wd -= tempo;
			u_var4 = ChipGetField(hChip, F367ofdm_TSFIFO_LINEOK);
		}

		if (!u_var4) {
#if defined(HOST_PC) && !defined(NO_GUI)
			ReportInsertMessage("TSFIFO_LINEOK failed");
#endif
			return FE_TER_NOLOCK;
		}

		/*accroit la stabber to be done in monitoring task */
		/*ChipSetField(hChip, F367ofdm_SYR_TR_DIS, 1); */
		return FE_TER_LOCKOK;
	} else
		return FE_TER_SWNOK;
}

/*****************************************************
--FUNCTION	::	FE_STV0367ofdm_SetTS_Parallel_Serial
--ACTION	::	TSOutput setting
--PARAMS IN	::	hChip, PathTS
--PARAMS OUT::	NONE
--RETURN	::

--***************************************************/
void FE_STV0367ofdm_SetTS_Parallel_Serial(STCHIP_Handle_t hChip,
					  FE_TS_OutputMode_t PathTS)
{
	if (hChip != NULL) {

		ChipSetField(hChip, F367_TS_DIS, 0);
		switch (PathTS) {
		/*for removing warning:default we can assume in parallel mode */
		case FE_TS_PARALLEL_PUNCT_CLOCK:
			ChipSetField(hChip, F367ofdm_TSFIFO_SERIAL, 0);
			ChipSetField(hChip, F367ofdm_TSFIFO_DVBCI, 0);
			break;

		case FE_TS_SERIAL_PUNCT_CLOCK:
		default:
			ChipSetField(hChip, F367ofdm_TSFIFO_SERIAL, 1);
			ChipSetField(hChip, F367ofdm_TSFIFO_DVBCI, 1);
			break;

		}
	}
}

/*****************************************************
--FUNCTION	::	FE_STV0367_SetCLK_Polarity
--ACTION	::	clock polarity setting
--PARAMS IN	::	hChip, PathTS
--PARAMS OUT::	NONE
--RETURN	::

--***************************************************/
void FE_STV0367ofdm_SetCLK_Polarity(STCHIP_Handle_t hChip,
				    FE_TS_ClockPolarity_t clock)
{

	if (hChip != NULL) {

		switch (clock) {

		case FE_TS_RISINGEDGE_CLOCK:
			ChipSetField(hChip, F367_TS_BYTE_CLK_INV, 0);

			break;
		case FE_TS_FALLINGEDGE_CLOCK:
			ChipSetField(hChip, F367_TS_BYTE_CLK_INV, 1);

			break;
			/*case FE_TER_CLOCK_POLARITY_DEFAULT: */
		default:
			ChipSetField(hChip, F367_TS_BYTE_CLK_INV, 0);
			break;
		}
	}
}

/*****************************************************
--FUNCTION	::	FE_STV0367TER_SetCLKgen
--ACTION	::	PLL divider setting
--PARAMS IN	::	hChip, PathTS
--PARAMS OUT::	NONE
--RETURN	::

--***************************************************/
FE_LLA_Error_t FE_STV0367TER_SetCLKgen(STCHIP_Handle_t hChip, U32 DemodXtalFreq)
{
	STCHIP_Error_t error;
	if (hChip != NULL) {

		switch (DemodXtalFreq) {
			/*set internal freq to 53.125MHz */
		case 25000000:
			ChipSetOneRegister(hChip, R367_PLLMDIV, 0xA);
			ChipSetOneRegister(hChip, R367_PLLNDIV, 0x55);
			ChipSetOneRegister(hChip, R367_PLLSETUP, 0x18);
			error = hChip->Error;

			break;
		case 27000000:
			ChipSetOneRegister(hChip, R367_PLLMDIV, 0x1);
			ChipSetOneRegister(hChip, R367_PLLNDIV, 0x8);
			ChipSetOneRegister(hChip, R367_PLLSETUP, 0x18);
			error = hChip->Error;

			break;

		case 30000000:
			ChipSetOneRegister(hChip, R367_PLLMDIV, 0xc);
			ChipSetOneRegister(hChip, R367_PLLNDIV, 0x55);
			ChipSetOneRegister(hChip, R367_PLLSETUP, 0x18);
			error = hChip->Error;

			break;

		case 16000000:
			ChipSetOneRegister(hChip, R367_PLLMDIV, 0x2);
			ChipSetOneRegister(hChip, R367_PLLNDIV, 0x1b);
			ChipSetOneRegister(hChip, R367_PLLSETUP, 0x18);
			error = hChip->Error;
			break;
		default:
			error = CHIPERR_INVALID_HANDLE;
			break;
		}
		/*ChipSetOneRegister(hChip, R367ofdm_ANACTRL, 0);  */
		/*error=hChip->Error; */
	} else
		error = CHIPERR_INVALID_HANDLE;
	return error;
}

/*****************************************************
--FUNCTION	::	FE_367ofdm_Init
--ACTION	::	Initialisation of the STV0367 chip
--PARAMS IN	::	pInit	==>	pointer to FE_TER_InitParams_t structure
--PARAMS OUT::	NONE
--RETURN	::	Handle to STV0367
--***************************************************/

FE_LLA_Error_t FE_367ofdm_Init(FE_TER_InitParams_t *pInit,
			       FE_367ofdm_Handle_t *Handle)
{
	FE_367ofdm_InternalParams_t *pParams = NULL;

#ifndef CHIP_STAPI
	STCHIP_Info_t DemodChip;
#endif
	Demod_InitParams_t DemodInit;
	TER_TUNER_InitParams_t TunerInitParams;
	FE_LLA_Error_t error = FE_LLA_NO_ERROR;
	STCHIP_Error_t StChipError = CHIPERR_NO_ERROR;
	/* int cpt=0; */
#if !defined(HOST_PC) || defined(NO_GUI)
	STCHIP_Error_t DemodError;
	TUNER_Error_t TunerError;
	TUNER_Error_t Tuner2Error;
#endif
#ifdef HOST_PC
	pParams = malloc(sizeof(FE_367ofdm_InternalParams_t));
	(*Handle) = (FE_367ofdm_Handle_t) pParams;
#endif
#ifdef CHIP_STAPI
	pParams = (FE_367ofdm_InternalParams_t *) (*Handle);
#endif

	/* pParams->Echo = EchoParams;*//* POUR STAPI passer un multiinstance */
	if (pParams == NULL || pParams->hDemod == NULL)
		return FE_LLA_INVALID_HANDLE;
	/* Chip initialisation */
	/* in internal struct chip is stored */

/* #ifdef HOST_PC */
#if defined(HOST_PC) && !defined(NO_GUI)
	pParams->hDemod = DEMOD;
	pParams->hTuner = TUNER;
	pParams->hTuner2 = TUNER_B;
	pParams->Crystal_Hz = EXTCLK;
	pParams->PreviousBER = 0;
#else
	DemodError = CHIPERR_NO_ERROR;
	TunerError = TUNER_NO_ERR;
	Tuner2Error = TUNER_NO_ERR;
#ifdef CHIP_STAPI
	DemodInit.Chip = (pParams->hDemod);	/*Change /AG  */
	DemodInit.NbDefVal = STV0367ofdm_NBREGS;
#else
	DemodInit.Chip = &DemodChip;
#endif
	DemodInit.Chip->RepeaterFn = NULL;
	DemodInit.Chip->RepeaterHost = NULL;
	DemodInit.Chip->Repeater = FALSE;
	DemodInit.Chip->pData = NULL;
	DemodInit.Chip->I2cAddr = pInit->DemodI2cAddr;
	strcpy((char *)DemodInit.Chip->Name,
	       (char *)(pInit->DemodName));
	pParams->Crystal_Hz = pInit->Demod_Crystal_Hz;
	/*DemodInit.Chip->XtalFreq=  pInit->Demod_Crystal_Hz; */
	/*DemodInit.Chip->IFmode= pInit->IFmode; */
	pParams->PreviousBER = 0;
	pParams->hTuner2 = NULL;

	DemodError =
	    STV0367ofdm_Init(&DemodInit, (STCHIP_Handle_t *)&pParams->hDemod);
	if ((pParams->hDemod == NULL) || (pParams->hDemod->Error)) {
#ifdef HOST_PC
		FE_367ofdm_Term(Handle);
#endif
		return DemodError;
	}

	/* Here we make the necessary changes to the demod's
	 * registers depending on the tuner */
	switch (pInit->TunerModel) {
	case TUNER_DTT7546:
		/* ChipSetOneRegister(pParams->hDemod, R367_I2CRPT, 0x42); */
		/* PLL bypassed and disabled */
		ChipSetOneRegister(pParams->hDemod, R367_ANACTRL, 0x0D);
		/* IC runs at 54MHz with a 27MHz crystal */
		ChipSetOneRegister(pParams->hDemod, R367_PLLMDIV, 0x01);
		ChipSetOneRegister(pParams->hDemod, R367_PLLNDIV, 0x08);
		/* ADC clock is equal to system clock */
		ChipSetOneRegister(pParams->hDemod, R367_PLLSETUP, 0x18);
		/* Buffer Q disabled */
		ChipSetOneRegister(pParams->hDemod, R367_TOPCTRL, 0x0);
		/* ADCQ disabled */
		ChipSetOneRegister(pParams->hDemod, R367_DUAL_AD12, 0x04);
		ChipSetOneRegister(pParams->hDemod, R367ofdm_GAIN_SRC1, 0x2A);
		ChipSetOneRegister(pParams->hDemod, R367ofdm_GAIN_SRC2, 0xD6);
		ChipSetOneRegister(pParams->hDemod, R367ofdm_INC_DEROT1, 0x55);
		ChipSetOneRegister(pParams->hDemod, R367ofdm_INC_DEROT2, 0x55);
		ChipSetOneRegister(pParams->hDemod, R367ofdm_TRL_CTL, 0x14);
		ChipSetOneRegister(pParams->hDemod, R367ofdm_TRL_NOMRATE1,
				0xAE);
		ChipSetOneRegister(pParams->hDemod, R367ofdm_TRL_NOMRATE2,
				0x56);
		ChipSetOneRegister(pParams->hDemod, R367ofdm_FEPATH_CFG, 0x0);
		ChipSetOneRegister(pParams->hDemod, R367_ANACTRL, 0x00);
		break;

	case TUNER_STV4100:
		/* Buffer Q enabled */
		ChipSetOneRegister(pParams->hDemod, R367_TOPCTRL, 0x2);
		/* ADCQ enabled */
		ChipSetOneRegister(pParams->hDemod, R367_DUAL_AD12, 0x00);
		ChipSetOneRegister(pParams->hDemod, R367_ANACTRL, 0x00);

		break;

	case TUNER_TDA18212:
		/* I2C repeater configuration, value changes with I2C master
		 * clock */
		ChipSetOneRegister(pParams->hDemod, R367_I2CRPT, 0x22);
		/* PLL bypassed and disabled */
		ChipSetOneRegister(pParams->hDemod, R367_ANACTRL, 0x0D);
		/* IC runs at 54MHz with a 30MHz crystal */
		ChipSetOneRegister(pParams->hDemod, R367_PLLMDIV, 0x02);
		ChipSetOneRegister(pParams->hDemod, R367_PLLNDIV, 0x1b);
		/* ADC clock is equal to system clock */
		ChipSetOneRegister(pParams->hDemod, R367_PLLSETUP, 0x18);
		/* Buffer Q disabled */
		ChipSetOneRegister(pParams->hDemod, R367_ANADIGCTRL, 0x09);
		/* ADCQ disabled */
		ChipSetOneRegister(pParams->hDemod, R367_DUAL_AD12, 0x04);
		/* I2C repeater configuration, value changes with I2C master
		 * clock */
		ChipSetOneRegister(pParams->hDemod, R367_I2CRPT, 0x22);
		ChipSetOneRegister(pParams->hDemod, R367ofdm_GAIN_SRC1, 0x2A);
		ChipSetOneRegister(pParams->hDemod, R367ofdm_GAIN_SRC2, 0xD6);
		ChipSetOneRegister(pParams->hDemod, R367ofdm_INC_DEROT1, 0xed);
		ChipSetOneRegister(pParams->hDemod, R367ofdm_INC_DEROT2, 0x09);
		ChipSetOneRegister(pParams->hDemod, R367ofdm_TRL_CTL, 0xac);
		ChipSetOneRegister(pParams->hDemod, R367ofdm_TRL_NOMRATE1,
				0xb0);
		ChipSetOneRegister(pParams->hDemod, R367ofdm_TRL_NOMRATE2,
				0x56);
		ChipSetOneRegister(pParams->hDemod, R367ofdm_FEPATH_CFG, 0x0);
		/* PLL enabled and used */
		ChipSetOneRegister(pParams->hDemod, R367_ANACTRL, 0x00);

		break;
	default:
		break;
	}
	FE_STV0367TER_SetCLKgen(pParams->hDemod, pInit->Demod_Crystal_Hz);

	TunerInitParams.Model = pInit->TunerModel;
	TunerInitParams.TunerName = pInit->TunerName;
	strcpy((char *)TunerInitParams.TunerName, pInit->TunerName);
	TunerInitParams.TunerI2cAddress = pInit->TunerI2cAddr;
	TunerInitParams.Fxtal_Hz = 0;
	TunerInitParams.IF = pInit->TunerIF_kHz;
	TunerInitParams.Fxtal_Hz = pInit->Tuner_Crystal_Hz;
	TunerInitParams.Repeater = TRUE;
	TunerInitParams.RepeaterHost = pParams->hDemod;
	TunerInitParams.RepeaterFn = (STCHIP_RepeaterFn_t)
		STV367ofdm_RepeaterFn;
	TunerInitParams.TransmitStandard = FE_DVB_T;
	pParams->DemodModel = DEMOD_STV0367DVBT;

	if (!pParams->hTuner)
		return FE_LLA_INVALID_HANDLE;

	TunerError = FE_TunerInit(&TunerInitParams, &(pParams->hTuner));

	if (TunerError != TUNER_NO_ERR) {
		FE_TunerTerm(pParams->hTuner);
		TunerError = FE_TunerInit(&TunerInitParams,
				&(pParams->hTuner));
	}
	/* Note that the below tuner with handlte hTuner2 is not a tuner but an
	 * analog demodulator needed in some cases only */
	if ((pInit->TunerModel == TUNER_FQD1116)
	    || (pInit->TunerModel == TUNER_DNOS40AS)) {
		TunerInitParams.Model = TUNER_TDA9898;
		TunerInitParams.TunerName = "TDA9898";
		strcpy((char *)TunerInitParams.TunerName, "TDA9898");
		TunerInitParams.TunerI2cAddress = pInit->TunerI2cAddr2;
		TunerInitParams.RepeaterHost = pParams->hDemod;
		TunerInitParams.RepeaterFn = (STCHIP_RepeaterFn_t)
			STV367ofdm_RepeaterFn;
		pParams->DemodModel = DEMOD_STV0367DVBT;
		Tuner2Error = FE_TunerInit(&TunerInitParams,
				&(pParams->hTuner2));
	}
	if (DemodError == CHIPERR_NO_ERROR) {
		/* If no Error on the demod then check the Tuners */
		if (TunerError == TUNER_INVALID_HANDLE)
			/* if tuner type error */
			return FE_LLA_INVALID_HANDLE;
		else if (TunerError == TUNER_TYPE_ERR)
			/*if tuner NULL Handle error = FE_LLA_INVALIDE_HANDLE */
			return FE_LLA_BAD_PARAMETER;
		else if (TunerError != TUNER_NO_ERR)
			return FE_LLA_I2C_ERROR;
	} else {
		if (DemodError == CHIPERR_INVALID_HANDLE)
			return FE_LLA_INVALID_HANDLE;
		else
			return FE_LLA_I2C_ERROR;
	}
	/* pParams->hTuner = FE_Ter_TunerInit(pInit->TunerModel,
						pInit->TunerName,
						pInit->TunerI2cAddr,
						pInit->TunerIF_kHz,
						pParams->hDemod,
						(STCHIP_RepeaterFn_t)
						STV362_RepeaterFn);	*/

#endif

/* #endif */
#if defined(CHIP_STAPI) || defined(NO_GUI)
	/*Set TS1 and TS2 to serial or parallel mode */
	FE_STV0367ofdm_SetTS_Parallel_Serial(pParams->hDemod,
							pInit->PathTSClock);
	/*Set TS1 and TS2 to serial or parallel mode */
	FE_STV0367ofdm_SetCLK_Polarity(pParams->hDemod, pInit->ClockPolarity);
#endif

	if (pParams->hDemod != NULL) {
		/*check I2c errors */
		StChipError =
		    ChipGetRegisters(pParams->hDemod, R367_ID, 1);
		if (StChipError == CHIPERR_NO_ERROR)
			error = FE_LLA_NO_ERROR;
		else if (StChipError == CHIPERR_I2C_NO_ACK)
			error = FE_LLA_I2C_ERROR;
		else if (StChipError == CHIPERR_I2C_BURST)
			error = FE_LLA_I2C_ERROR;
		else if (StChipError == CHIPERR_INVALID_HANDLE)
			error = FE_LLA_INVALID_HANDLE;
		else
			error = FE_LLA_BAD_PARAMETER;

		if (error == FE_LLA_NO_ERROR) {
			/*Read IC cut ID */
			pParams->hDemod->ChipID =
				ChipGetOneRegister(pParams->hDemod, R367_ID);

			pParams->I2Cspeed =
				FE_367TER_SpeedInit(pParams->hDemod);
			pParams->first_lock = 0;
			pParams->Unlockcounter = 0;
		ChipSetField(pParams->hDemod, F367_STDBY_ADCGP, 1);
		ChipSetField(pParams->hDemod, F367_STDBY_ADCGP, 0);
		} else {

#ifdef HOST_PC
			free(pParams);
#endif

			pParams = NULL;

		}

	}		/*if (pParams->hDemod != NULL) */

	/*if (pParams != NULL) */
	return error;
}

/*****************************************************
--FUNCTION	::	FE_367ofdm_CableToTerrestrial
--ACTION	::	switch from Cable config to terr. config
--PARAMS IN	::	Handle	==>	Front End Handle
--PARAMS OUT::	NONE
--RETURN	::	Error (if any)
--***************************************************/
FE_LLA_Error_t FE_367ofdm_SwitchDemodToDVBT(FE_367ofdm_Handle_t Handle)
{
	FE_LLA_Error_t error = FE_LLA_NO_ERROR;
	TUNER_Params_t hTunerParams = NULL;
	FE_367ofdm_InternalParams_t *pParams;

	if ((void *)Handle == NULL)
		return FE_LLA_INVALID_HANDLE;

	pParams = (FE_367ofdm_InternalParams_t *) Handle;

	if ((pParams->hTuner == NULL) || (pParams->hDemod == NULL)
	    || (pParams->hTuner->pData == NULL))
		/* tuner not allowed in ter mode */
		return FE_LLA_BAD_PARAMETER;

#if defined(HOST_PC) && !defined(NO_GUI)
	pParams->hDemod = DEMOD;	/* use current selected 367qam */
	pParams->Crystal_Hz = EXTCLK;
#endif

	/*set the values from demod init inside generic tuners */
	ChipSetOneRegister(pParams->hDemod, R367_I2CRPT, 0x22);
	ChipSetOneRegister(pParams->hDemod, R367_TOPCTRL, 0x02);
	ChipSetOneRegister(pParams->hDemod, R367_IOCFG0, 0x40);
	ChipSetOneRegister(pParams->hDemod, R367_DAC0R, 0x00);
	ChipSetOneRegister(pParams->hDemod, R367_IOCFG1, 0x00);
	ChipSetOneRegister(pParams->hDemod, R367_DAC1R, 0x00);
	ChipSetOneRegister(pParams->hDemod, R367_IOCFG2, 0x00);
	ChipSetOneRegister(pParams->hDemod, R367_SDFR, 0x00);
	ChipSetOneRegister(pParams->hDemod, R367_AUX_CLK, 0x0a);
	ChipSetOneRegister(pParams->hDemod, R367_FREESYS1, 0x00);
	ChipSetOneRegister(pParams->hDemod, R367_FREESYS2, 0x00);
	ChipSetOneRegister(pParams->hDemod, R367_FREESYS3, 0x00);
	ChipSetOneRegister(pParams->hDemod, R367_GPIO_CFG, 0x55);
	ChipSetOneRegister(pParams->hDemod, R367_GPIO_CMD, 0x00);
	ChipSetOneRegister(pParams->hDemod, R367_TSTRES, 0x00);
	/*was d caution PLL stopped, to be restarted at init!!! */
	ChipSetOneRegister(pParams->hDemod, R367_ANACTRL, 0x00);
	ChipSetOneRegister(pParams->hDemod, R367_TSTBUS, 0x00);
	ChipSetOneRegister(pParams->hDemod, R367_RF_AGC1, 0xff);
	ChipSetOneRegister(pParams->hDemod, R367_RF_AGC2, 0x83);
	ChipSetOneRegister(pParams->hDemod, R367_ANADIGCTRL, 0x19);
	ChipSetOneRegister(pParams->hDemod, R367_PLLMDIV, 0x0c);
	ChipSetOneRegister(pParams->hDemod, R367_PLLNDIV, 0x55);
	ChipSetOneRegister(pParams->hDemod, R367_PLLSETUP, 0x18);
	ChipSetOneRegister(pParams->hDemod, R367_DUAL_AD12, 0x00);
	ChipSetOneRegister(pParams->hDemod, R367_TSTBIST, 0x00);

	hTunerParams = (TUNER_Params_t) pParams->hTuner->pData;
	switch (hTunerParams->Model) {
	case TUNER_DTT7546:
		pParams->DemodModel = DEMOD_STV0367DVBT;

		/*set here the necessary registers to switch from cab to ter */
		ChipSetOneRegister(pParams->hDemod, R367_I2CRPT, 0x42);
		/* Buffer Q disabled */
		ChipSetOneRegister(pParams->hDemod, R367_ANADIGCTRL, 0x09);
		/* IC runs at 54MHz with a 27MHz crystal */
		ChipSetOneRegister(pParams->hDemod, R367_PLLMDIV, 0x01);
		ChipSetOneRegister(pParams->hDemod, R367_PLLNDIV, 0x08);
		/* ADC clock is equal to system clock */
		ChipSetOneRegister(pParams->hDemod, R367_PLLSETUP, 0x18);
		/* Buffer Q disabled */
		ChipSetOneRegister(pParams->hDemod, R367_TOPCTRL, 0x0);
		/* ADCQ disabled */
		ChipSetOneRegister(pParams->hDemod, R367_DUAL_AD12, 0x04);
		ChipSetOneRegister(pParams->hDemod, R367ofdm_GAIN_SRC1, 0x2A);
		ChipSetOneRegister(pParams->hDemod, R367ofdm_GAIN_SRC2, 0xD6);
		ChipSetOneRegister(pParams->hDemod, R367ofdm_INC_DEROT1, 0x55);
		ChipSetOneRegister(pParams->hDemod, R367ofdm_INC_DEROT2, 0x55);
		ChipSetOneRegister(pParams->hDemod, R367ofdm_TRL_CTL, 0x2c);
		ChipSetOneRegister(pParams->hDemod, R367ofdm_TRL_NOMRATE1,
				0xAE);
		ChipSetOneRegister(pParams->hDemod, R367ofdm_TRL_NOMRATE2,
				0x56);
		ChipSetOneRegister(pParams->hDemod, R367ofdm_FEPATH_CFG, 0x0);
		ChipSetOneRegister(pParams->hDemod, R367_ANACTRL, 0x00);

		break;

	case TUNER_FQD1116:
		pParams->DemodModel = DEMOD_STV0367DVBT;
		/*set here the necessary registers to switch from cab to ter */
		break;

	case TUNER_STV4100:
		pParams->DemodModel = DEMOD_STV0367DVBT;
		/*set here the necessary registers to switch from cab to ter */
		break;
	default:
		break;
	}
	/*set tuner to terrestrial conf */
	error = FE_SwitchTunerToDVBT(pParams->hTuner);

	return error;
}

/*****************************************************
--FUNCTION	::	FE_367ofdm_Algo
--ACTION	::	Search for a valid channel
--PARAMS IN	::	Handle	==>	Front End Handle
				pSearch ==> Search parameters
				pResult ==> Result of the search
--PARAMS OUT::	NONE
--RETURN	::	Error (if any)
--***************************************************/
FE_LLA_Error_t FE_367ofdm_Algo(FE_367ofdm_Handle_t Handle,
			       FE_TER_SearchParams_t *pSearch,
			       FE_TER_SearchResult_t *pResult)
{				/*, STTUNER_tuner_instance_t *TunerInstance) */
	FE_367ofdm_InternalParams_t *pIntParams;
	TER_TUNER_Params_t hTunerParams = NULL;
	int offset = 0, tempo = 0, AgcIF = 0, count = 0;
	unsigned short int u_var;	/*added for HM *db */
	U8 constell, counter, tps_rcvd[2], HighProfileRate, LowProfileRate;
	S8 step;
	/*BOOL SpectrumInversion=FALSE; */
	S32 timing_offset = 0;
	U32 trl_nomrate = 0, intX, InternalFreq = 0, temp = 0, TunerLock = 0;
#ifdef HOST_PC
#ifndef NO_GUI
	int EnableTRLSRCSettings, EnableFilterSettings, TRLcentring;
	int i, j, cell_coeff, cell_coeff_msb, cell_coeff_lsb;
	char string[100];
#endif
#endif

	FE_LLA_Error_t error = FE_LLA_NO_ERROR;

	if ((void *)Handle == NULL)
		return FE_LLA_BAD_PARAMETER;

	pIntParams = (FE_367ofdm_InternalParams_t *) Handle;

	hTunerParams = (TUNER_Params_t) pIntParams->hTuner->pData;
	/* Fill pParams structure with search parameters */
	pIntParams->Frequency = pSearch->Frequency_kHz;
	pIntParams->FFTSize = pSearch->Optimize.FFTSize;
	pIntParams->Guard = pSearch->Optimize.Guard;
	pIntParams->Inv = pSearch->Optimize.SpectInversion;
	/*pIntParams->Force=pSearch->Force + ChipGetField(pIntParams->hDemod,
							F367ofdm_FORCE)*2; */
	pIntParams->ChannelBW_MHz = pSearch->ChannelBW_MHz;
	pIntParams->IF_IQ_Mode = pSearch->IF_IQ_Mode;
#ifdef HOST_PC
#ifndef NO_GUI
	/*pIntParams->hDemod->XtalFreq = EXTCLK; */
	pIntParams->Crystal_Hz = EXTCLK;
#endif
#endif
	ChipSetField(pIntParams->hDemod, F367ofdm_CCS_ENABLE, 0);
	/* reset fine agc only before lock to avoid agc coarse+ fine
	 * oscillations */
	ChipSetField(pIntParams->hDemod, F367ofdm_AUT_EARLY, 0);
	ChipSetField(pIntParams->hDemod, F367ofdm_TPS_BCHDIS, 0);

	switch (pIntParams->IF_IQ_Mode) {

	case FE_TER_NORMAL_IF_TUNER:	/* Normal IF mode */
		ChipSetField(pIntParams->hDemod, F367_TUNER_BB, 0);
		ChipSetField(pIntParams->hDemod, F367ofdm_LONGPATH_IF, 0);
		ChipSetField(pIntParams->hDemod, F367ofdm_DEMUX_SWAP, 0);
		break;

	case FE_TER_LONGPATH_IF_TUNER:	/* Long IF mode */
		ChipSetField(pIntParams->hDemod, F367_TUNER_BB, 0);
		ChipSetField(pIntParams->hDemod, F367ofdm_LONGPATH_IF, 1);
		ChipSetField(pIntParams->hDemod, F367ofdm_DEMUX_SWAP, 1);
#if defined(HOST_PC) && !defined(NO_GUI)
			/* in LP IF filter must be set *db */
		UsrWrInt("EnableFilterSettings", 1);
#endif
		break;

	case FE_TER_IQ_TUNER:	/* IQ mode */
		ChipSetField(pIntParams->hDemod, F367_TUNER_BB, 1);
		/*spectrum inversion hw detection off *db */
		ChipSetField(pIntParams->hDemod, F367ofdm_PPM_INVSEL, 0);
		break;

	default:
#if defined(HOST_PC) && !defined(NO_GUI)
		ReportInsertMessage("IF_IQ_Mode uncorrectly set");
#endif
		return FE_LLA_SEARCH_FAILED;
	}

#if defined(HOST_PC) && !defined(NO_GUI)
	UsrRdInt("EnableTRLSRCSettings", &EnableTRLSRCSettings);

	UsrRdInt("EnableFilterSettings", &EnableFilterSettings);
#endif
	if ((pSearch->Optimize.SpectInversion == FE_TER_INVERSION_NONE)) {
		if (pIntParams->IF_IQ_Mode == FE_TER_IQ_TUNER) {
			ChipSetField(pIntParams->hDemod, F367ofdm_IQ_INVERT,
					0);
			pIntParams->SpectrumInversion = FALSE;
		} else {
			ChipSetField(pIntParams->hDemod, F367ofdm_INV_SPECTR,
					0);
			pIntParams->SpectrumInversion = FALSE;
		}
	} else if (pSearch->Optimize.SpectInversion == FE_TER_INVERSION) {
		if (pIntParams->IF_IQ_Mode == FE_TER_IQ_TUNER) {
			ChipSetField(pIntParams->hDemod, F367ofdm_IQ_INVERT,
					1);
			pIntParams->SpectrumInversion = TRUE;
		} else {
			ChipSetField(pIntParams->hDemod, F367ofdm_INV_SPECTR,
					1);
			pIntParams->SpectrumInversion = TRUE;
		}
	} else if (pSearch->Optimize.SpectInversion ==
			FE_TER_INVERSION_Unknown) {
		if (pIntParams->IF_IQ_Mode == FE_TER_IQ_TUNER) {
			if (pIntParams->Sense == TRUE) {
				ChipSetField(pIntParams->hDemod,
						F367ofdm_IQ_INVERT, 1);
				pIntParams->SpectrumInversion = TRUE;
			} else {
				ChipSetField(pIntParams->hDemod,
						F367ofdm_IQ_INVERT, 0);
				pIntParams->SpectrumInversion = FALSE;
			}
		} else {
			if (pIntParams->Sense == 1) {
				ChipSetField(pIntParams->hDemod,
						F367ofdm_INV_SPECTR, 1);
				pIntParams->SpectrumInversion = TRUE;
			} else {
				ChipSetField(pIntParams->hDemod,
						F367ofdm_INV_SPECTR, 0);
				pIntParams->SpectrumInversion = FALSE;
			}
		}
	}

#if defined(HOST_PC) && !defined(NO_GUI)
	if ((pIntParams->IF_IQ_Mode != FE_TER_NORMAL_IF_TUNER)
	    && (IIR_Filter_Freq_setting != pIntParams->ChannelBW_MHz)) {
#else
	if ((pIntParams->IF_IQ_Mode != FE_TER_NORMAL_IF_TUNER)) {
		/*&& (pIntParams->PreviousChannelBW_MHz !=
			pIntParams->ChannelBW_MHz)*/
#endif

		FE_367TER_AGC_IIR_LOCK_DETECTOR_SET(pIntParams->hDemod);

		/*set fine agc target to 180 for LPIF or IQ mode */
		/* set Q_AGCTarget */
		ChipSetField(pIntParams->hDemod, F367ofdm_SEL_IQNTAR, 1);
		ChipSetField(pIntParams->hDemod, F367ofdm_AUT_AGC_TARGET_MSB,
				0xB);
		/*ChipSetField(pIntParams->hDemod, AUT_AGC_TARGET_LSB, 0x04); */

		/* set Q_AGCTarget */
		ChipSetField(pIntParams->hDemod, F367ofdm_SEL_IQNTAR, 0);
		ChipSetField(pIntParams->hDemod, F367ofdm_AUT_AGC_TARGET_MSB,
				0xB);
		/*ChipSetField(pIntParams->hDemod, AUT_AGC_TARGET_LSB, 0x04); */

#if defined(HOST_PC) && !defined(NO_GUI)
		if (EnableFilterSettings) {
			if (!FE_367TER_IIR_FILTER_INIT(pIntParams->hDemod,
			     pIntParams->ChannelBW_MHz, EXTCLK))
				return FE_LLA_BAD_PARAMETER;
			IIR_Filter_Freq_setting = pIntParams->ChannelBW_MHz;
		}
		for (i = 1; i <= 6; i++) {
			ChipSetField(DEMOD, F367ofdm_IIR_CELL_NB, i - 1);

			for (j = 1; j <= 5; j++) {
				cell_coeff = 0;
				Fmt(string, "%s<%s%i%s%i%s", "Cell", i, "[", j,
						"]");

				cell_coeff_msb =
				    ChipGetOneRegister(DEMOD,
						    (R367ofdm_IIRCX_COEFF1_MSB
						     + 2 * (j - 1)));
				cell_coeff_lsb =
				    ChipGetOneRegister(DEMOD,
						    (R367ofdm_IIRCX_COEFF1_LSB
						     + 2 * (j - 1)));
				cell_coeff =
				    (cell_coeff_lsb) + (cell_coeff_msb << 8);
				UsrWrInt(string, cell_coeff);
			}

		}
#else
		if (!FE_367TER_IIR_FILTER_INIT(pIntParams->hDemod,
					pIntParams->ChannelBW_MHz,
					pIntParams->Crystal_Hz))
			return FE_LLA_BAD_PARAMETER;
		/*set IIR filter once for 6, 7 or 8MHz BW */
		pIntParams->PreviousChannelBW = pIntParams->ChannelBW_MHz;
#endif

		FE_367TER_AGC_IIR_RESET(pIntParams->hDemod);

	}

	/*********Code Added For Hierarchical Modulation****************/

	if (pIntParams->Hierarchy == FE_TER_HIER_LOW_PRIO)
		/*printf(" I am in Hierarchy low priority\n"); */
		ChipSetField(pIntParams->hDemod, F367ofdm_BDI_LPSEL, 0x01);
	else
		/*printf(" I am in Hierarchy high priority\n"); */
		ChipSetField(pIntParams->hDemod, F367ofdm_BDI_LPSEL, 0x00);
	/*************************/
#if defined(HOST_PC) && !defined(NO_GUI)
	if (EnableTRLSRCSettings) {
		InternalFreq = FE_367ofdm_GetMclkFreq(pIntParams->hDemod, (U32)
				EXTCLK) / 1000;
		temp = RoundRealToNearestInteger((((pIntParams->ChannelBW_MHz
						* 64 * pow(2, 15) * 100) /
						(InternalFreq)) * 10) / 7);
#else
	InternalFreq = FE_367ofdm_GetMclkFreq(pIntParams->hDemod,
			pIntParams->Crystal_Hz) / 1000;
	temp = (int)((((pIntParams->ChannelBW_MHz * 64 * PowOf2(15) * 100) /
					(InternalFreq)) * 10) / 7);
#endif

	ChipSetFieldImage(pIntParams->hDemod, F367ofdm_TRL_NOMRATE_LSB, temp %
			2);
	temp = temp / 2;
	ChipSetFieldImage(pIntParams->hDemod, F367ofdm_TRL_NOMRATE_HI, temp /
			256);
	ChipSetFieldImage(pIntParams->hDemod, F367ofdm_TRL_NOMRATE_LO, temp %
			256);
	ChipSetRegisters(pIntParams->hDemod, R367ofdm_TRL_NOMRATE1, 2);
	ChipGetRegisters(pIntParams->hDemod, R367ofdm_TRL_CTL, 3);

	temp = ChipGetFieldImage(pIntParams->hDemod,
			      F367ofdm_TRL_NOMRATE_HI) * 512 +
		ChipGetFieldImage(pIntParams->hDemod,
			      F367ofdm_TRL_NOMRATE_LO) * 2 +
		ChipGetFieldImage(pIntParams->hDemod,
				F367ofdm_TRL_NOMRATE_LSB);
#if defined(HOST_PC) && !defined(NO_GUI)
	temp =
	    RoundRealToNearestInteger((pow(2, 17) * pIntParams->ChannelBW_MHz *
				       1000) / (7 * (InternalFreq)));
#else
	temp = (int)((PowOf2(17) * pIntParams->ChannelBW_MHz * 1000) / (7 *
				(InternalFreq)));
#endif
	ChipSetFieldImage(pIntParams->hDemod, F367ofdm_GAIN_SRC_HI, temp /
			256);
	ChipSetFieldImage(pIntParams->hDemod, F367ofdm_GAIN_SRC_LO, temp %
			256);
	ChipSetRegisters(pIntParams->hDemod, R367ofdm_GAIN_SRC1, 2);
	ChipGetRegisters(pIntParams->hDemod, R367ofdm_GAIN_SRC1, 2);
	temp =
	    ChipGetFieldImage(pIntParams->hDemod, F367ofdm_GAIN_SRC_HI) * 256 +
	    ChipGetFieldImage(pIntParams->hDemod, F367ofdm_GAIN_SRC_LO);

#if defined(HOST_PC) && !defined(NO_GUI)
	temp =
	    RoundRealToNearestInteger((InternalFreq - hTunerParams->IF) *
			    pow(2, 16) / (InternalFreq));
#else
	temp = (int)((InternalFreq - hTunerParams->IF) * PowOf2(16) /
			    (InternalFreq));
#endif

	ChipSetFieldImage(pIntParams->hDemod, F367ofdm_INC_DEROT_HI, temp /
			256);
	ChipSetFieldImage(pIntParams->hDemod, F367ofdm_INC_DEROT_LO, temp %
			256);
	ChipSetRegisters(pIntParams->hDemod, R367ofdm_INC_DEROT1, 2);

#if defined(HOST_PC) && !defined(NO_GUI)
	}
#endif

	/*      pIntParams->EchoPos   = pSearch->EchoPos;
	ChipSetField(pIntParams->hDemod, F367ofdm_LONG_ECHO,
				pIntParams->EchoPos); not used in auto mode */
	if (hTunerParams->Model == TUNER_STV4100) {
		ChipSetOneRegister(pIntParams->hDemod, R367ofdm_AGC_TARG, 0x16);

		error = FE_TunerStartAndCalibrate(pIntParams->hTuner);
		/* #if defined(NO_GUI)  */
		if (error != FE_LLA_NO_ERROR)
			return error;
		error = FE_TunerSetBandWidth(pIntParams->hTuner,
				pIntParams->ChannelBW_MHz);
		/*#endif   */
		if (error != FE_LLA_NO_ERROR)
			return error;

		error = FE_TunerSetFrequency(pIntParams->hTuner,
				pIntParams->Frequency + 100);
		if (error != FE_LLA_NO_ERROR)
			return error;
		pIntParams->Frequency =
			FE_TunerGetFrequency(pIntParams->hTuner);

		ChipWaitOrAbort(pIntParams->hDemod, 66);
		AgcIF = ChipGetField(pIntParams->hDemod, F367ofdm_AGC2_VAL_LO) +
		    (ChipGetField(pIntParams->hDemod, F367ofdm_AGC2_VAL_HI) <<
		     8);
		intX =
		    (ChipGetField(pIntParams->hDemod, F367ofdm_INT_X3) << 24) +
		    (ChipGetField(pIntParams->hDemod, F367ofdm_INT_X2) << 16) +
		    (ChipGetField(pIntParams->hDemod, F367ofdm_INT_X1) << 8) +
		    ChipGetField(pIntParams->hDemod, F367ofdm_INT_X0);

		if ((AgcIF > 0x500) && (intX > 0x50000) && (AgcIF < 0xc00)) {
			error = FE_TunerSetLna(pIntParams->hTuner);
			if (error != FE_LLA_NO_ERROR)
				return error;
		} else {
			error =
			    FE_TunerAdjustRfPower(pIntParams->hTuner, AgcIF);
			if (error != FE_LLA_NO_ERROR)
				return error;
		}
		/*      printf("estimated power=%03d\n",
		FE_Ter_TunerEstimateRfPower(pIntParams->hTuner,
			ChipGetField(pIntParams->hDemod, F362_RF_AGC1_LEVEL_HI),
			AgcIF)); */
		/* core active */
		/*reset filter to avoid saturation */
		ChipSetField(pIntParams->hDemod, F367ofdm_NRST_IIR, 0);
		/*ChipSetField(pIntParams->hDemod, F367ofdm_CORE_ACTIVE, 0); */
		/*ChipWaitOrAbort(pIntParams->hDemod, 20); */
		ChipSetField(pIntParams->hDemod, F367ofdm_NRST_IIR, 1);
		/*ChipSetField(pIntParams->hDemod, F367ofdm_CORE_ACTIVE, 1); */
	} else {
		error = FE_TunerSetFrequency(pIntParams->hTuner,
				pIntParams->Frequency);
		if (error != FE_LLA_NO_ERROR)
			return error;
		pIntParams->Frequency =
			FE_TunerGetFrequency(pIntParams->hTuner);
	}

		/**********************************/
	/*   Check if the tuner is locked */
	TunerLock = FE_TunerGetStatus(pIntParams->hTuner);
	if (TunerLock == 0) {
#ifdef HOST_PC
#if defined(DBG_SEARCHALGO) && !defined(NO_GUI)
		snprintf(text, sizeof(text),
				 "SEARCH>> FE_367ter_Algo::Tuner did not lock");
		ReportInsertMessage(text);
#endif
#endif
		return FE_LLA_SEARCH_FAILED;
	}
	   /*********************************/
	if (FE_367ofdm_LockAlgo(pIntParams) == FE_TER_LOCKOK) {
		pResult->Locked = TRUE;

		pIntParams->State = FE_TER_LOCKOK;
		/* update results */

		ChipSetField(pIntParams->hDemod, F367ofdm_TPS_BCHDIS, 1);
			/***********  dans search term auparavant **********/
		tps_rcvd[0] =
		    ChipGetOneRegister(pIntParams->hDemod, R367ofdm_TPS_RCVD2);
		tps_rcvd[1] =
		    ChipGetOneRegister(pIntParams->hDemod, R367ofdm_TPS_RCVD3);

		ChipGetRegisters(pIntParams->hDemod, R367ofdm_SYR_STAT, 1);
		pResult->FFTSize =
		    ChipGetFieldImage(pIntParams->hDemod, F367ofdm_SYR_MODE);
		pResult->Guard =
		    ChipGetFieldImage(pIntParams->hDemod, F367ofdm_SYR_GUARD);

		constell = ChipGetField(pIntParams->hDemod, F367ofdm_TPS_CONST);
		if (constell == 0)
			pResult->Modulation = FE_TER_MOD_QPSK;
		else if (constell == 1)
			pResult->Modulation = FE_TER_MOD_16QAM;
		else if (constell == 2)
			pResult->Modulation = FE_TER_MOD_64QAM;
		else
			error = FE_LLA_BAD_PARAMETER;

			/***Code replced and changed  for HM**/
		pResult->Hierarchy = pIntParams->Hierarchy;
		pResult->Hierarchy_Alpha = (tps_rcvd[0] & 0x70) >> 4;
			/****/
		HighProfileRate = (tps_rcvd[1] & 0x07);
		if (HighProfileRate == 0)
			pResult->LPRate = FE_TER_Rate_1_2;
		else if (HighProfileRate == 1)
			pResult->LPRate = FE_TER_Rate_2_3;
		else if (HighProfileRate == 2)
			pResult->LPRate = FE_TER_Rate_3_4;
		else if (HighProfileRate == 3)
			pResult->LPRate = FE_TER_Rate_5_6;
		else if (HighProfileRate == 4)
			pResult->LPRate = FE_TER_Rate_7_8;
		else
			error = FE_LLA_BAD_PARAMETER;
		/*pResult->HPRate=tps_rcvd[1] & 0x07; */

		LowProfileRate = (tps_rcvd[1] & 0x70) >> 4;
		if (LowProfileRate == 0)
			pResult->LPRate = FE_TER_Rate_1_2;
		else if (LowProfileRate == 1)
			pResult->LPRate = FE_TER_Rate_2_3;
		else if (LowProfileRate == 2)
			pResult->LPRate = FE_TER_Rate_3_4;
		else if (LowProfileRate == 3)
			pResult->LPRate = FE_TER_Rate_5_6;
		else if (LowProfileRate == 4)
			pResult->LPRate = FE_TER_Rate_7_8;
		else
			error = FE_LLA_BAD_PARAMETER;
		/*pResult->LPRate=(tps_rcvd[1] &0x70)>>4; */
			/****/
		constell = ChipGetField(pIntParams->hDemod, F367ofdm_PR);
		if (constell == 5)
			constell = 4;
		pResult->FECRate = (FE_TER_FECRate_t) constell;

		if (pIntParams->IF_IQ_Mode == FE_TER_IQ_TUNER) {
			pResult->SpectInversion =
			    ChipGetField(pIntParams->hDemod,
					 F367ofdm_IQ_INVERT);
		} else {
			pResult->SpectInversion =
			    ChipGetField(pIntParams->hDemod,
					 F367ofdm_INV_SPECTR);
		}

		/* dcdc modifs per Gilles */
		pIntParams->first_lock = 1;
		/* modifs Tuner */

		ChipGetRegisters(pIntParams->hDemod, R367ofdm_AGC2MAX, 13);
		/*pResult->Agc_val = (ChipGetFieldImage(pIntParams->hDemod,
					F367ofdm_AGC1_VAL_LO)<<16) +
				     (ChipGetFieldImage(pIntParams->hDemod,
					F367ofdm_AGC1_VAL_HI)<<24) +
				     ChipGetFieldImage(pIntParams->hDemod,
					F367ofdm_AGC2_VAL_LO) +
				     (ChipGetFieldImage(pIntParams->hDemod,
					F367ofdm_AGC2_VAL_HI)<<8); */

		/* Carrier offset calculation */
		ChipSetField(pIntParams->hDemod, F367ofdm_FREEZE, 1);
		ChipGetRegisters(pIntParams->hDemod, R367ofdm_CRL_FREQ1, 3);
		ChipSetField(pIntParams->hDemod, F367ofdm_FREEZE, 0);

		offset = (ChipGetFieldImage(pIntParams->hDemod,
					F367ofdm_CRL_FOFFSET_VHI) << 16);
		offset += (ChipGetFieldImage(pIntParams->hDemod,
					F367ofdm_CRL_FOFFSET_HI) << 8);
		offset += (ChipGetFieldImage(pIntParams->hDemod,
					F367ofdm_CRL_FOFFSET_LO));
		if (offset > 8388607)
			offset -= 16777216;

		offset = offset * 2 / 16384;

		if (pResult->FFTSize == FE_TER_FFT_2K)
			/*** 1 FFT BIN=4.464khz***/
			offset = (offset * 4464) / 1000;
		else if (pResult->FFTSize == FE_TER_FFT_4K)
			/*** 1 FFT BIN=2.23khz***/
			offset = (offset * 223) / 100;
		else if (pResult->FFTSize == FE_TER_FFT_8K)
			/*** 1 FFT BIN=1.1khz***/
			offset = (offset * 111) / 100;

		/* inversion hard auto */
		if (ChipGetField(pIntParams->hDemod, F367ofdm_PPM_INVSEL)
				== 1) {
			if (((ChipGetField(pIntParams->hDemod,
					F367ofdm_INV_SPECTR) !=
			      ChipGetField(pIntParams->hDemod,
					   F367ofdm_STATUS_INV_SPECRUM)) == 1))
				/* no inversion nothing to do */;
			else
				offset = offset * -1;
		} else {	/* manual inversion */

			if (((!pIntParams->SpectrumInversion)
			     && (hTunerParams->SpectrInvert == TUNER_IQ_NORMAL))
			    || ((pIntParams->SpectrumInversion)
				&& (hTunerParams->SpectrInvert ==
				    TUNER_IQ_INVERT)))
				offset = offset * -1;
		}
		if (pIntParams->ChannelBW_MHz == 6)
			offset = (offset * 6) / 8;
		else if (pIntParams->ChannelBW_MHz == 7)
			offset = (offset * 7) / 8;

		pIntParams->Frequency += offset;
		pResult->Frequency_kHz = pIntParams->Frequency;
		/*pResult->ResidualOffset=offset; */

		/* pResult->Echo_pos=ChipGetField(pIntParams->hDemod,
							F367ofdm_LONG_ECHO);  */

		/*For FEC rate return to application */
		/* Get the FEC Rate */
		if (pResult->Hierarchy == FE_TER_HIER_LOW_PRIO) {
			pResult->FECRate =
			    ChipGetField(pIntParams->hDemod,
					 F367ofdm_TPS_LPCODE);

		} else {
			pResult->FECRate =
			    ChipGetField(pIntParams->hDemod,
					 F367ofdm_TPS_HPCODE);
		}

		switch (pResult->FECRate) {
		case 0:
			pResult->FECRate = FE_TER_Rate_1_2;
			break;
		case 1:
			pResult->FECRate = FE_TER_Rate_2_3;
			break;
		case 2:
			pResult->FECRate = FE_TER_Rate_3_4;
			break;
		case 3:
			pResult->FECRate = FE_TER_Rate_5_6;
			break;
		case 4:
			pResult->FECRate = FE_TER_Rate_7_8;
			break;
		default:

			break;	/* error */
		}

#if defined(HOST_PC) && !defined(NO_GUI)
		UsrRdInt("EnableTRLcentring", &TRLcentring);

		if (TRLcentring) {

#endif

			/*WAIT_N_MS(200) ; */
			/* exit even if timing_offset stays null *db* */
			tempo = 10;
			while ((timing_offset == 0) && (tempo > 0)) {
				/*was 20ms  */
				ChipWaitOrAbort(pIntParams->hDemod, 10);
				/* fine tuning of timing offset if required */
				ChipGetRegisters(pIntParams->hDemod,
						 R367ofdm_TRL_CTL, 5);
				timing_offset =
				    ChipGetFieldImage(pIntParams->hDemod,
						      F367ofdm_TRL_TOFFSET_LO) +
				    256 * ChipGetFieldImage(pIntParams->hDemod,
						      F367ofdm_TRL_TOFFSET_HI);
				if (timing_offset >= 32768)
					timing_offset -= 65536;
				trl_nomrate =
				    (512 *
				     ChipGetFieldImage(pIntParams->hDemod,
						       F367ofdm_TRL_NOMRATE_HI)
				     + ChipGetFieldImage(pIntParams->hDemod,
						       F367ofdm_TRL_NOMRATE_LO)
				     * 2 + ChipGetFieldImage(pIntParams->hDemod,
						     F367ofdm_TRL_NOMRATE_LSB));

				timing_offset =
				    ((signed)(1000000 / trl_nomrate) *
				     timing_offset) / 2048;
				tempo--;
			}

			if (timing_offset <= 0) {
				timing_offset = (timing_offset - 11) / 22;
				step = -1;
			} else {
				timing_offset = (timing_offset + 11) / 22;
				step = 1;
			}

			for (counter = 0; counter < abs(timing_offset);
								counter++) {
				trl_nomrate += step;
				ChipSetFieldImage(pIntParams->hDemod,
						  F367ofdm_TRL_NOMRATE_LSB,
						  trl_nomrate % 2);
				ChipSetFieldImage(pIntParams->hDemod,
						  F367ofdm_TRL_NOMRATE_LO,
						  trl_nomrate / 2);
				ChipSetRegisters(pIntParams->hDemod,
						 R367ofdm_TRL_CTL, 2);
				ChipWaitOrAbort(pIntParams->hDemod, 1);
			}

			ChipWaitOrAbort(pIntParams->hDemod, 5);
			/* unlocks could happen in case of trl centring big
			 * step, then a core off/on restarts demod */
			u_var =
			    ChipGetField(pIntParams->hDemod,
					 F367ofdm_TSFIFO_LINEOK);

			if (!u_var) {
				ChipSetField(pIntParams->hDemod,
					     F367ofdm_CORE_ACTIVE, 0);
				ChipWaitOrAbort(pIntParams->hDemod, 20);
				ChipSetField(pIntParams->hDemod,
					     F367ofdm_CORE_ACTIVE, 1);

				/* ChipWaitOrAbort(pIntParams->hDemod, 350); */
				u_var =
				    ChipGetField(pIntParams->hDemod,
						 F367ofdm_TSFIFO_LINEOK);
				while ((!u_var) && (count < 400)) {
					ChipWaitOrAbort(pIntParams->hDemod, 5);

					u_var =
					    ChipGetField(pIntParams->hDemod,
							F367ofdm_TSFIFO_LINEOK);
					count++;
#if defined(HOST_PC) && !defined(NO_GUI)
					if (count == 199)
						ReportInsertMessage
					    ("could not relock after centring");
#endif
				}
			}
#if defined(HOST_PC) && !defined(NO_GUI)
		}
#endif

	} /*  if (FE_367_Algo(pParams) == FE_TER_LOCKOK) */
	else {
		pResult->Locked = FALSE;
		error = FE_LLA_SEARCH_FAILED;
	}

	return error;
}

#if defined(HOST_PC) && !defined(NO_GUI)
 /*****************************************************
--FUNCTION	::	GetTunerInfo
--ACTION	::	manage HM
--PARAMS IN	::	Handle	==>	Front End Handle
				pSearch ==> Search parameters
				pResult ==> Result of the search
--PARAMS OUT::	NONE
--RETURN	::	Error (if any)
--***************************************************/
FE_LLA_Error_t GetTunerInfo(FE_367ofdm_Handle_t Handle,
			    FE_TER_SearchParams_t *pSearch,
			    FE_TER_SearchResult_t *pResult)
{
	U8 Data;
	FE_367ofdm_InternalParams_t *pParams;
	FE_LLA_Error_t error = FE_LLA_NO_ERROR;

	FE_TER_FECRate_t CurFECRates;	/*Added for HM */
	FE_TER_Hierarchy_Alpha_t CurHierMode;	/*Added for HM */
	pParams = (FE_367ofdm_InternalParams_t *) Handle;
	/*printf(" in FE_367_Search () value of pParams.Hierarchy %d\n",
	 * pParams.Hierarchy); */

	/*code added for hierarchical mode by db */
	/* Get the Hierarchical Mode */

	Data = ChipGetField(pParams->hDemod, F367ofdm_TPS_HIERMODE);

	switch (Data) {
	case 0:
		CurHierMode = FE_TER_HIER_ALPHA_NONE;
		break;
	case 1:
		CurHierMode = FE_TER_HIER_ALPHA_1;
		break;
	case 2:
		CurHierMode = FE_TER_HIER_ALPHA_2;
		break;
	case 3:
		CurHierMode = FE_TER_HIER_ALPHA_4;
		break;
	default:
		CurHierMode = Data;
		error = FE_LLA_BAD_PARAMETER;
		break;		/* error */
	}

	/* Get the FEC Rate */

	if (pParams->Hierarchy == FE_TER_HIER_LOW_PRIO)
		Data = ChipGetField(pParams->hDemod, F367ofdm_TPS_LPCODE);
	else
		Data = ChipGetField(pParams->hDemod, F367ofdm_TPS_HPCODE);

	switch (Data) {
	case 0:
		CurFECRates = FE_TER_Rate_1_2;
		break;
	case 1:
		CurFECRates = FE_TER_Rate_2_3;
		break;
	case 2:
		CurFECRates = FE_TER_Rate_3_4;
		break;
	case 3:
		CurFECRates = FE_TER_Rate_5_6;
		break;
	case 4:
		CurFECRates = FE_TER_Rate_7_8;
		break;
	default:
		CurFECRates = Data;
		error = FE_LLA_BAD_PARAMETER;
		break;		/* error */
	}
	/**** end of HM addition ******/
	pResult->HPRate = CurFECRates;
	pResult->Hierarchy_Alpha = CurHierMode;
	return error;
}
#endif
/*****************************************************
--FUNCTION	::	FE_367ofdfm_Search
--ACTION	::	Intermediate layer before launching Search
--PARAMS IN	::	Handle	==>	Front End Handle
				pSearch ==> Search parameters
				pResult ==> Result of the search
--PARAMS OUT::	NONE
--RETURN	::	Error (if any)
--***************************************************/
FE_LLA_Error_t FE_367ofdm_Search(FE_367ofdm_Handle_t Handle,
				 FE_TER_SearchParams_t *pSearch,
				 FE_TER_SearchResult_t *pResult)
{				/*, STTUNER_tuner_instance_t *TunerInstance) */

	/*U8 trials[2]; */
	S8 num_trials, index;
	FE_LLA_Error_t error = FE_LLA_NO_ERROR;
	/*U8 flag_spec_inv;
	   U8 flag; */
	U8 SenseTrials[2];
	U8 SenseTrialsAuto[2];

	FE_367ofdm_InternalParams_t *pParams;
	/*to remove warning on OS20 */
	SenseTrials[0] = INV;
	SenseTrials[1] = NINV;
	SenseTrialsAuto[0] = INV;
	SenseTrialsAuto[1] = NINV;

	if ((void *)Handle != NULL) {

		pParams = (FE_367ofdm_InternalParams_t *) Handle;

		if ((pParams->Inv == FE_TER_INVERSION_NONE)
		    || (pParams->Inv == FE_TER_INVERSION))
			num_trials = 1;

		else		/*UNK */
			num_trials = 2;

		pParams->State = FE_TER_NOLOCK;
		index = 0;

		while (((index) < num_trials)
		       && (pParams->State != FE_TER_LOCKOK)) {

			if (!pParams->first_lock) {
				if (pParams->Inv == FE_TER_INVERSION_Unknown)
					pParams->Sense = SenseTrials[index];
			}
			error = FE_367ofdm_Algo(Handle, pSearch, pResult);

			if ((pParams->State == FE_TER_LOCKOK) && (index == 1)) {
				/* invert spectrum sense */
				SenseTrialsAuto[index] = SenseTrialsAuto[0];
				SenseTrialsAuto[(index + 1) % 2] =
				    (SenseTrialsAuto[1] + 1) % 2;
			}

			index++;
		}

	}

	return error;

}

/*****************************************************
--FUNCTION	::	FE_STV0367_SetStandby
--ACTION	::	Set demod STANDBY mode On/Off
--PARAMS IN	::	Handle	==>	Front End Handle

-PARAMS OUT::	NONE.
--RETURN	::	Error (if any)
--***************************************************/
FE_LLA_Error_t FE_STV0367ofdm_SetStandby(FE_367ofdm_Handle_t Handle,
					 U8 StandbyOn)
{
	FE_LLA_Error_t error = FE_LLA_NO_ERROR;
	FE_367ofdm_InternalParams_t *pParams =
	    (FE_367ofdm_InternalParams_t *) Handle;
	TER_TUNER_Params_t hTunerParams = NULL;

	if (pParams != NULL) {
		hTunerParams = (TER_TUNER_Params_t) pParams->hTuner->pData;
		if ((pParams->hDemod->Error) || (pParams->hTuner->Error)) {
			error = FE_LLA_I2C_ERROR;
		} else {
			if (StandbyOn) {

				if ((hTunerParams->Model == TUNER_STV4100)) {

					error =
					    FE_TunerSetStandbyMode(pParams->
								   hTuner,
								   StandbyOn);
				}
				ChipSetField(pParams->hDemod, F367_STDBY, 1);
				ChipSetField(pParams->hDemod, F367_STDBY_FEC,
					     1);
				ChipSetField(pParams->hDemod, F367_STDBY_CORE,
					     1);
			} else {
				ChipSetField(pParams->hDemod, F367_STDBY, 0);
				ChipSetField(pParams->hDemod, F367_STDBY_FEC,
					     0);
				ChipSetField(pParams->hDemod, F367_STDBY_CORE,
					     0);
				if ((hTunerParams->Model == TUNER_STV4100)) {

					error =
					    FE_TunerSetStandbyMode(pParams->
								   hTuner,
								   StandbyOn);
				}
			}
			if ((pParams->hDemod->Error)
			    || (pParams->hTuner->Error))
				error = FE_LLA_I2C_ERROR;
		}
	} else
		error = FE_LLA_INVALID_HANDLE;

	return error;
}

/*****************************************************
--FUNCTION	::	FE_367ofdm_GetSignalInfo
--ACTION	::	Return informations on the locked channel
--PARAMS IN	::	Handle	==>	Front End Handle
--PARAMS OUT::	pInfo	==> Informations (BER, C/N, power ...)
--RETURN	::	Error (if any)
--***************************************************/
FE_LLA_Error_t FE_367ofdm_GetSignalInfo(FE_367ofdm_Handle_t Handle,
					FE_TER_SignalInfo_t *pInfo)
{
	FE_LLA_Error_t error = FE_LLA_NO_ERROR;
	FE_367ofdm_InternalParams_t *pParams = NULL;
	TER_TUNER_Params_t hTunerParams = NULL;
	int offset = 0;
	FE_TER_FECRate_t CurFECRates;	/*Added for HM */
	FE_TER_Hierarchy_Alpha_t CurHierMode;	/*Added for HM */
/* U32  BitErrRate, PackErrRate, Errors, bits; */
	/* int constell=0; */
	int snr = 0, Data = 0;
	U16 if_agc = 0;
	pParams = (FE_367ofdm_InternalParams_t *) Handle;

	if (pParams != NULL) {
		hTunerParams = (TER_TUNER_Params_t) pParams->hTuner->pData;
		/*There is no need to check LOCK status again, already checked
		 * in FE_362_Status */
		/*pInfo->Locked=FE_362_Status(Handle); */
		/* pParams->Frequency =
				FE_TunerGetFrequency(pParams->hTuner) ; */
		pInfo->Frequency_kHz = FE_TunerGetFrequency(pParams->hTuner);
		/*constell =
			ChipGetFieldImage(pParams->hDemod, F367ofdm_TPS_CONST);
		   if (constell == 0)    pInfo->Modulation= FE_TER_MOD_QPSK;
		   else if (constell==1) pInfo->Modulation= FE_TER_MOD_16QAM;
		   else if (constell==2) pInfo->Modulation= FE_TER_MOD_64QAM;
		   else                  pInfo->Modulation=FE_TER_MOD_Reserved;
		should not pass here */

		if (pParams->IF_IQ_Mode == FE_TER_IQ_TUNER)
			pParams->SpectrumInversion =
			    ChipGetField(pParams->hDemod, F367ofdm_IQ_INVERT);

		else
			pParams->SpectrumInversion =
			    ChipGetField(pParams->hDemod, F367ofdm_INV_SPECTR);

/* Get the Hierarchical Mode */

		Data = ChipGetField(pParams->hDemod, F367ofdm_TPS_HIERMODE);

		switch (Data) {
		case 0:
			CurHierMode = FE_TER_HIER_ALPHA_NONE;
			break;
		case 1:
			CurHierMode = FE_TER_HIER_ALPHA_1;
			break;
		case 2:
			CurHierMode = FE_TER_HIER_ALPHA_2;
			break;
		case 3:
			CurHierMode = FE_TER_HIER_ALPHA_4;
			break;
		default:
			CurHierMode = Data;
			error = FE_LLA_BAD_PARAMETER;
			break;	/* error */
		}

		/* Get the FEC Rate */

		if (pParams->Hierarchy == FE_TER_HIER_LOW_PRIO)
			Data =
			    ChipGetField(pParams->hDemod, F367ofdm_TPS_LPCODE);

		else
			Data =
			    ChipGetField(pParams->hDemod, F367ofdm_TPS_HPCODE);

		switch (Data) {
		case 0:
			CurFECRates = FE_TER_Rate_1_2;
			break;
		case 1:
			CurFECRates = FE_TER_Rate_2_3;
			break;
		case 2:
			CurFECRates = FE_TER_Rate_3_4;
			break;
		case 3:
			CurFECRates = FE_TER_Rate_5_6;
			break;
		case 4:
			CurFECRates = FE_TER_Rate_7_8;
			break;
		default:
			CurFECRates = Data;
			error = FE_LLA_BAD_PARAMETER;
			break;	/* error */
		}

	/**** end of HM addition ******/
		/*pInfo->FECRate=CurFECRates; */
		/*pInfo->Hierarchy_Alpha= CurHierMode; */
		pInfo->Hierarchy = pParams->Hierarchy;
		pParams->FFTSize =
		    ChipGetFieldImage(pParams->hDemod, F367ofdm_SYR_MODE);
		pParams->Guard =
		    ChipGetFieldImage(pParams->hDemod, F367ofdm_SYR_GUARD);

		/* Carrier offset calculation */
		ChipSetField(pParams->hDemod, F367ofdm_FREEZE, 1);
		ChipGetRegisters(pParams->hDemod, R367ofdm_CRL_FREQ1, 3);
		ChipSetField(pParams->hDemod, F367ofdm_FREEZE, 0);

		offset =
		    (ChipGetFieldImage
		     (pParams->hDemod, F367ofdm_CRL_FOFFSET_VHI) << 16);
		offset +=
		    (ChipGetFieldImage(pParams->hDemod, F367ofdm_CRL_FOFFSET_HI)
		     << 8);
		offset +=
		    (ChipGetFieldImage
		     (pParams->hDemod, F367ofdm_CRL_FOFFSET_LO));
		if (offset > 8388607)
			offset -= 16777216;

		offset = offset * 2 / 16384;

		if (pParams->FFTSize == FE_TER_FFT_2K)
			offset = (offset * 4464) / 1000;
		else if (pParams->FFTSize == FE_TER_FFT_4K)
			offset = (offset * 223) / 100;
		else if (pParams->FFTSize == FE_TER_FFT_8K)
			offset = (offset * 111) / 100;

		/* inversion hard auto */
		if (ChipGetField(pParams->hDemod, F367ofdm_PPM_INVSEL) == 1) {
			if (((ChipGetField(pParams->hDemod, F367ofdm_INV_SPECTR)
			      != ChipGetField(pParams->hDemod,
					      F367ofdm_STATUS_INV_SPECRUM)) ==
			     1))
				/* no inversion nothing to do */;
			else
				offset = offset * -1;
		} else {	/* manual inversion */

			if (((!pParams->SpectrumInversion)
			     && (hTunerParams->SpectrInvert == TUNER_IQ_NORMAL))
			    || ((pParams->SpectrumInversion)
				&& (hTunerParams->SpectrInvert ==
				    TUNER_IQ_INVERT)))
				offset = offset * -1;
		}
		if (pParams->ChannelBW_MHz == 6)
			offset = (offset * 6) / 8;
		else if (pParams->ChannelBW_MHz == 7)
			offset = (offset * 7) / 8;

		/*pInfo->ResidualOffset=offset; */

		/*FE_367TER_GetErrors(pParams,  &Errors, &bits, &PackErrRate,
								&BitErrRate); */
		pInfo->PER_xE7_u32 = FE_367ofdm_GetBerErrors(pParams);
		pInfo->BER_xE7_u32 = FE_367ofdm_GetPerErrors(pParams);

		/*MeasureBER(pParams, 0, 1,&BitErrRate,&PackErrRate) ; */
		/*pInfo->BER = FE_367ofdm_GetErrors(pParams); */

		/*For quality return 1000*snr to application  SS */
		/* /4 for STv0367, /8 for STv0362 */
		snr =
		    (1000 * ChipGetField(pParams->hDemod, F367ofdm_CHCSNR)) / 8;

		/**pNoise = (snr*10) >> 3;*/
		/**  fix done here for the bug GNBvd20972 where pNoise is
		 * calculated in right percentage **/
		/*pInfo->CN_dBx10=(((snr*1000)/8)/32)/10; */
		pInfo->SNR_x100percent_u32 = (snr / 32) / 10;
		pInfo->SNR_x10dB_u32 = (U32)(snr / 100);
		if (hTunerParams->Model == TUNER_STV4100) {
			if_agc =
			    ChipGetField(pParams->hDemod,
					 F367ofdm_AGC2_VAL_LO) +
			    (ChipGetField(pParams->hDemod, F367ofdm_AGC2_VAL_HI)
			     << 8);

			pInfo->RFLevel_x10dBm_s32 =
			    FE_TunerGetRFLevel(pParams->hTuner,
					       ChipGetField(pParams->hDemod,
							 F367_RF_AGC1_LEVEL_HI),
					       if_agc);
		}

	} else
		error = FE_LLA_INVALID_HANDLE;

	return error;
}

/*****************************************************
--FUNCTION	::	FE_367ofdm_Term
--ACTION	::	Terminate STV0367 chip connection
--PARAMS IN	::	Handle	==>	Front End Handle
--PARAMS OUT::	NONE
--RETURN	::	Error (if any)
--***************************************************/
FE_LLA_Error_t FE_367ofdm_Term(FE_367ofdm_Handle_t Handle)
{

	FE_LLA_Error_t error = FE_LLA_NO_ERROR;
	FE_367ofdm_InternalParams_t *pParams = NULL;

	pParams = (FE_367ofdm_InternalParams_t *) Handle;

	if (pParams != NULL) {
#ifdef HOST_PC
#ifdef NO_GUI
		if (pParams->hTuner2 != NULL)
			FE_TunerTerm(pParams->hTuner2);
		if (pParams->hTuner != NULL)
			FE_TunerTerm(pParams->hTuner);
		if (pParams->hDemod != NULL)
			ChipClose(pParams->hDemod);
		/*FE_TunerTerm(pParams->hTuner); */

#endif
		if (Handle)
			free(pParams);
#endif
	} else
		error = FE_LLA_INVALID_HANDLE;

	return error;

}

#ifdef HOST_PC
void FE_367ofdm_Core_Switch(STCHIP_Handle_t hChip)
{
	ChipSetField(hChip, F367ofdm_CORE_ACTIVE, 0);
	ChipSetField(hChip, F367ofdm_CORE_ACTIVE, 1);
	ChipWaitOrAbort(hChip, 350);
	return;
}
#endif
/*****************************************************

**FUNCTION :: FE_367qam_GetMclkFreq

**ACTION :: Set the STV0367QAM master clock frequency

**PARAMS IN :: hChip ==> handle to the chip

** ExtClk_Hz ==> External clock frequency (Hz)

**PARAMS OUT:: NONE

**RETURN :: MasterClock frequency (Hz)

*****************************************************/

U32 FE_367ofdm_GetMclkFreq(STCHIP_Handle_t hChip, U32 ExtClk_Hz)
{

	U32 mclk_Hz = 0;	/* master clock frequency (Hz) */

	U32 M, N, P;

	if (ChipGetField(hChip, F367_BYPASS_PLLXN) == 0) {

		ChipGetRegisters(hChip, R367_PLLMDIV, 3);

		N = (U32) ChipGetFieldImage(hChip, F367_PLL_NDIV);

		if (N == 0)
			N = N + 1;

		M = (U32) ChipGetFieldImage(hChip, F367_PLL_MDIV);

		if (M == 0)
			M = M + 1;

		P = (U32) ChipGetFieldImage(hChip, F367_PLL_PDIV);

		if (P > 5)
			P = 5;
		mclk_Hz = ((ExtClk_Hz / 2) * N) / (M * PowOf2(P));
	}

	else

		mclk_Hz = ExtClk_Hz;

	return mclk_Hz;

}

/*****************************************************
**FUNCTION	::	STV367ofdm_RepeaterFn
**ACTION	::	Set the I2C repeater
**PARAMS IN	::	handle to the chip
**				state of the repeater
**PARAMS OUT::	Error
**RETURN	::	NONE
*****************************************************/
#ifdef STFRONTEND_USE_NEW_I2C
STCHIP_Error_t STV367ofdm_RepeaterFn(STCHIP_Handle_t hChip, BOOL State,
				     U8 *Buffer)
#else
STCHIP_Error_t STV367ofdm_RepeaterFn(STCHIP_Handle_t hChip, BOOL State)
#endif
{
	STCHIP_Error_t error = CHIPERR_NO_ERROR;

	if (hChip != NULL) {
#ifdef CONFIG_ARCH_STI
		ChipSetField(hChip, F367_STOP_ENABLE, 0);
#else
		ChipSetField(hChip, F367_STOP_ENABLE, 1);	/*db */
#endif
		if (State == TRUE) {
#ifdef STFRONTEND_USE_NEW_I2C
			ChipFillRepeaterMessage(hChip, F367_I2CT_ON, 1, Buffer);
#else
			ChipSetField(hChip, F367_I2CT_ON, 1);
#endif
		} else {
#ifdef STFRONTEND_USE_NEW_I2C
			ChipFillRepeaterMessage(hChip, F367_I2CT_ON, 0, Buffer);
#else
			ChipSetField(hChip, F367_I2CT_ON, 0);
#endif
		}
	}

	return error;
}

/*STCHIP_Error_t STV367ofdm_RepeaterFn(STCHIP_Handle_t hChip, BOOL State)
{
	STCHIP_Error_t error = CHIPERR_NO_ERROR;

	if (hChip != NULL) {
		ChipSetField(hChip, F367ofdm_STOP_ENABLE, 1);
		if (State == TRUE) {
			ChipSetField(hChip, F367ofdm_I2CT_ON, 1);
		} else
			ChipSetField(hChip, F367ofdm_I2CT_ON, 0);
	}

	return error; */
BOOL FE_367ofdm_lock(FE_367ofdm_Handle_t Handle)
{

	FE_367ofdm_InternalParams_t *pParams = NULL;
	/* FE_LLA_Error_t error=FE_LLA_SEARCH_FAILED; */
	BOOL Locked = FALSE;
	/* U32 ind=0; */
	pParams = (FE_367ofdm_InternalParams_t *) Handle;

	if (pParams != NULL) {
		Locked =
		    ((ChipGetField(pParams->hDemod, F367ofdm_TPS_LOCK) |
		      ChipGetField(pParams->hDemod,
				   F367ofdm_PRF) | ChipGetField(pParams->hDemod,
								F367ofdm_LK) |
		      ChipGetField(pParams->hDemod, F367ofdm_TSFIFO_LINEOK)));
	}
	return Locked;
}

/*****************************************************
**FUNCTION	::	FE_367ofdm_Status
**ACTION	::	checking demod lock in tracking
**PARAMS IN	::	handle to the chip
**PARAMS OUT::	NONE
**RETURN	::  Locked
*****************************************************/

BOOL FE_367ofdm_Status(FE_367ofdm_Handle_t Handle)
{

	FE_367ofdm_InternalParams_t *pParams = NULL;
	FE_TER_SearchParams_t Search;
	FE_TER_SearchResult_t SearchResult;
	FE_LLA_Error_t error = FE_LLA_SEARCH_FAILED;
	BOOL Locked = FALSE;
	U16 count = 0;

	pParams = (FE_367ofdm_InternalParams_t *) Handle;

	if (pParams == NULL)
		return FALSE;

	Locked =
	    (/*(ChipGetField(pParams->hDemod, F367ofdm_TPS_LOCK) && */
	    (ChipGetField(pParams->hDemod, F367ofdm_PRF)) &&
	     (ChipGetField(pParams->hDemod, F367ofdm_LK)) &&
	     (ChipGetField(pParams->hDemod, F367ofdm_TSFIFO_LINEOK)));

	if (Locked) {
		pParams->Unlockcounter = 0;

		/* apply Sfec workaround if 8K 64QAM CR!=1/2 */
		if ((ChipGetField(pParams->hDemod, F367ofdm_TPS_CONST)
		     == 2)
		    && (ChipGetField(pParams->hDemod, F367ofdm_SYR_MODE)
			== 1)
		    &&
		    (ChipGetField(pParams->hDemod, F367ofdm_TPS_HPCODE)
		     != 0)) {
			if (ChipGetOneRegister
			    (pParams->hDemod,
			     R367ofdm_SFDLYSETH) != 0xc0) {
				ChipSetOneRegister(pParams->hDemod,
						   R367ofdm_SFDLYSETH,
						   0xc0);
				ChipSetOneRegister(pParams->hDemod,
						   R367ofdm_SFDLYSETM,
						   0x60);
				ChipSetOneRegister(pParams->hDemod,
						   R367ofdm_SFDLYSETL,
						   0x0);
			}
		} else {
			ChipSetOneRegister(pParams->hDemod,
					   R367ofdm_SFDLYSETH, 0x0);
		}
		return Locked;
	}
	/*check if AGC is clamped ie antenna disconnected */
	if (ChipGetField(pParams->hDemod, F367ofdm_AGC2_VAL_HI) != 0xf) {
		if (pParams->Unlockcounter > 2) {
			Search.Frequency_kHz = pParams->Frequency;
			Search.IF_IQ_Mode = pParams->IF_IQ_Mode;
			Search.Optimize.FFTSize = pParams->FFTSize;
			Search.Optimize.Guard = pParams->Guard;
			/*Search.Force = pParams->Force; */
			Search.Hierarchy = pParams->Hierarchy;
			Search.Optimize.SpectInversion = pParams->Inv;
			/*Search.Force = pParams->Force; */
			Search.ChannelBW_MHz = pParams->ChannelBW_MHz;
			/*Search.EchoPos = pParams->EchoPos; */
			/* Launch the search algorithm  */
			error =
			    FE_367ofdm_Search(Handle, &Search, &SearchResult);
			if (error == FE_LLA_NO_ERROR)
				Locked = TRUE;
			else
				Locked = FALSE;
			pParams->Unlockcounter = 0;
		} else
			pParams->Unlockcounter++;
	} else {	/*AGC is clamped ie antenna disconnected */

		pParams->Unlockcounter++;
		/* printf("\n pParams->Unlockcounter1 = %d\n",
						pParams->Unlockcounter); */
	}

	if (pParams->Unlockcounter > 2) {
		ChipSetField(pParams->hDemod, F367ofdm_CORE_ACTIVE, 0);
		ChipWaitOrAbort(pParams->hDemod, 20);
		ChipSetField(pParams->hDemod, F367ofdm_CORE_ACTIVE, 1);

		Locked =
		    ((/*ChipGetField(pParams->hDemod, F367ofdm_TPS_LOCK) &&  */
		      ChipGetField(pParams->hDemod, F367ofdm_PRF) &&
		      ChipGetField(pParams->hDemod, F367ofdm_LK) &&
		      ChipGetField(pParams->hDemod, F367ofdm_TSFIFO_LINEOK)));
		/* printf("\n Locked = %d\n", Locked); */
		while ((!Locked) && (count < 70)) {
			ChipWaitOrAbort(pParams->hDemod, 5);

			Locked =
			    ((ChipGetField(pParams->hDemod, F367ofdm_TPS_LOCK)
			      && ChipGetField(pParams->hDemod, F367ofdm_PRF)
			      && ChipGetField(pParams->hDemod, F367ofdm_LK)
			      && ChipGetField(pParams->hDemod,
				      F367ofdm_TSFIFO_LINEOK)));

			count++;
		}
	}

	return Locked;
}

/*****************************************************
**FUNCTION	::	FE_367TER_GetLLARevision
**ACTION	::	return LLARevision
**PARAMS IN	::	NONE
**PARAMS OUT::	NONE
**RETURN	::  Revision367
*****************************************************/

ST_Revision_t FE_367TER_GetLLARevision(void)
{
	return Revision367ofdm;
}

/*****************************************************
**FUNCTION	::	FE_367ofdm_GetBerErrors
**ACTION	::	calculating ber using fec2
**PARAMS IN	::	pParams	==> pointer to FE_TER_InternalParams_t structure
**PARAMS OUT::	NONE
**RETURN	::  10e7*Ber
*****************************************************/
U32 FE_367ofdm_GetBerErrors(FE_367ofdm_InternalParams_t *pParams)
{				/*FEC1 */
	U32 Errors = 0, Ber = 0, temporary = 0;
	int abc = 0, def = 0;	/* cpt=0, max_count=0; */
	if (pParams == NULL)
		return Ber;

	ChipSetField(pParams->hDemod, F367ofdm_SFEC_ERR_SOURCE, 0x07);
	ChipSetField(pParams->hDemod, F367ofdm_SFEC_NUM_EVENT, 0x4);
	abc = ChipGetField(pParams->hDemod, F367ofdm_SFEC_ERR_SOURCE);
	def = ChipGetFieldImage(pParams->hDemod, F367ofdm_SFEC_NUM_EVENT);

	ChipWaitOrAbort(pParams->hDemod, 1);
	ChipGetRegisters(pParams->hDemod, R367ofdm_SFERRCTRL, 4);
	Errors =
	    ((U32) ChipGetFieldImage(pParams->hDemod, F367ofdm_SFEC_ERR_CNT) *
	     PowOf2(16)) +
	    ((U32) ChipGetFieldImage(pParams->hDemod, F367ofdm_SFEC_ERR_CNT_HI)
	     * PowOf2(8)) +
	    ((U32) ChipGetFieldImage(pParams->hDemod,
		    F367ofdm_SFEC_ERR_CNT_LO));

	if (Errors == 0)
		Ber = 0;
	else if (abc == 0x7) {
		/* multiply by 10^9 to keep significant digits and stay under
		 * U32 overflow */
		if (Errors <= 4) {
			temporary = (Errors * 1000000000) / (8 * PowOf2(14));
			temporary = temporary;
		} else if (Errors <= 42) {
			temporary = (Errors * 100000000) / (8 * PowOf2(14));
			temporary = temporary * 10;
		} else if (Errors <= 429) {
			temporary = (Errors * 10000000) / (8 * PowOf2(14));
			temporary = temporary * 100;
		} else if (Errors <= 4294) {
			temporary = (Errors * 1000000) / (8 * PowOf2(14));
			temporary = temporary * 1000;
		} else if (Errors <= 42949) {
			temporary = (Errors * 100000) / (8 * PowOf2(14));
			temporary = temporary * 10000;
		} else if (Errors <= 429496) {
			temporary = (Errors * 10000) / (8 * PowOf2(14));
			temporary = temporary * 100000;
		} else {	/*if (Errors<4294967) 2^22 max error */

			temporary = (Errors * 1000) / (8 * PowOf2(14));
			temporary = temporary * 100000;	/* still to *10 */
		}

		/* Byte error */
		if (def == 2)
			/*Ber=Errors/(8*pow(2, 14)); */
			Ber = temporary;
		else if (def == 3)
			/*Ber=Errors/(8*pow(2, 16)); */
			Ber = temporary / 4;
		else if (def == 4)
			/*Ber=Errors/(8*pow(2, 18)); */
			Ber = temporary / 16;
		else if (def == 5)
			/*Ber=Errors/(8*pow(2, 20)); */
			Ber = temporary / 64;
		else if (def == 6)
			/*Ber=Errors/(8*pow(2, 22)); */
			Ber = temporary / 256;
		else
			/* should not pass here */
			Ber = 0;

		/* divide by 100 to get BER*10^7 instead of BER*10^9 */
		if ((Errors < 4294967) && (Errors > 429496))
			Ber /= 10;
		else
			Ber /= 100;

	}

	return Ber;
}

/*****************************************************
**FUNCTION	::	FE_367ofdm_GetPerErrors
**ACTION	::	counting packet errors using fec1
**PARAMS IN	::	pParams	==> pointer to FE_TER_InternalParams_t structure
**PARAMS OUT::	NONE
**RETURN	::  10e7*Per
*****************************************************/
U32 FE_367ofdm_GetPerErrors(FE_367ofdm_InternalParams_t *pParams)
{				/*Fec2 */
	U32 Errors = 0, Per = 0, temporary = 0;
	int abc = 0, def = 0;	/* cpt=0; */
	if (pParams == NULL)
		return Per;

	ChipSetField(pParams->hDemod, F367ofdm_ERR_SRC1, 0x09);
	ChipSetField(pParams->hDemod, F367ofdm_NUM_EVT1, 0x5);
	abc = ChipGetField(pParams->hDemod, F367ofdm_ERR_SRC1);
	def = ChipGetFieldImage(pParams->hDemod, F367ofdm_NUM_EVT1);

	ChipGetRegisters(pParams->hDemod, R367ofdm_ERRCTRL1, 4);
	ChipWaitOrAbort(pParams->hDemod, 1);
	Errors =
	    ((U32)ChipGetFieldImage(pParams->hDemod, F367ofdm_ERR_CNT1)
	     * PowOf2(16)) +
	    ((U32)ChipGetFieldImage(pParams->hDemod, F367ofdm_ERR_CNT1_HI)
	     * PowOf2(8)) +
	    ((U32)ChipGetFieldImage(pParams->hDemod, F367ofdm_ERR_CNT1_LO));

	if (Errors == 0)
		Per = 0;

	else if (abc == 0x9) {
		if (Errors <= 4) {
			temporary =
			    (Errors * 1000000000) / (8 * PowOf2(8));
			temporary = temporary;
		} else if (Errors <= 42) {
			temporary =
			    (Errors * 100000000) / (8 * PowOf2(8));
			temporary = temporary * 10;
		} else if (Errors <= 429) {
			temporary =
			    (Errors * 10000000) / (8 * PowOf2(8));
			temporary = temporary * 100;
		} else if (Errors <= 4294) {
			temporary =
			    (Errors * 1000000) / (8 * PowOf2(8));
			temporary = temporary * 1000;
		} else if (Errors <= 42949) {
			temporary = (Errors * 100000) / (8 * PowOf2(8));
			temporary = temporary * 10000;
		} else {	/*if (Errors<=429496)  2^16 errors max */
			temporary = (Errors * 10000) / (8 * PowOf2(8));
			temporary = temporary * 100000;
		}

		/* pkt error */
		if (def == 2)
			/*Per=Errors/PowOf2(8); */
			Per = temporary;
		else if (def == 3)
			/*Per=Errors/PowOf2(10); */
			Per = temporary / 4;
		else if (def == 4)
			/*Per=Errors/PowOf2(12); */
			Per = temporary / 16;
		else if (def == 5)
			/*Per=Errors/PowOf2(14); */
			Per = temporary / 64;
		else if (def == 6)
			/*Per=Errors/PowOf2(16); */
			Per = temporary / 256;
		else
			Per = 0;

		/* divide by 100 to get PER*10^7 */
		Per /= 100;

	}
	/* save actual value */
	pParams->PreviousPER = Per;

	return Per;
}

/*****************************************************
**FUNCTION	::	FE_367ofdm_GetErrors
**ACTION	::	counting packet errors using fec1
**PARAMS IN	::	pParams	==> pointer to FE_TER_InternalParams_t structure
**PARAMS OUT::	NONE
**RETURN	::  error nb
*****************************************************/

U32 FE_367ofdm_GetErrors(FE_367ofdm_InternalParams_t *pParams)
{				/*Fec2 */
	U32 Errors = 0;
	int cpt = 0;

	if (pParams == NULL)
		return Errors;

	/*wait for counting completion */
	while (((ChipGetField(pParams->hDemod, F367ofdm_ERRCNT1_OLDVALUE) == 1)
			&& (cpt < 400)) || ((Errors == 0) && (cpt < 400))) {
		ChipGetRegisters(pParams->hDemod, R367ofdm_ERRCTRL1, 4);
		ChipWaitOrAbort(pParams->hDemod, 1);
		Errors =
		    ((U32)ChipGetFieldImage(pParams->hDemod, F367ofdm_ERR_CNT1)
		     * PowOf2(16)) +
		    ((U32)ChipGetFieldImage(pParams->hDemod,
			    F367ofdm_ERR_CNT1_HI) * PowOf2(8)) +
		    ((U32)ChipGetFieldImage(pParams->hDemod,
			    F367ofdm_ERR_CNT1_LO));
		cpt++;
	}

	return Errors;
}

/*****************************************************
--FUNCTION :: FE_367TER_SetAbortFlag
--ACTION :: Set Abort flag On/Off
--PARAMS IN :: Handle ==> Front End Handle

-PARAMS OUT:: NONE.
--RETURN :: Error (if any)

--***************************************************/
FE_LLA_Error_t FE_367ofdm_SetAbortFlag(FE_367ofdm_Handle_t Handle, BOOL Abort)
{
	FE_LLA_Error_t error = FE_LLA_NO_ERROR;
	FE_367ofdm_InternalParams_t *pParams =
	    (FE_367ofdm_InternalParams_t *) Handle;

	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;
	if ((pParams->hDemod->Error) || (pParams->hTuner->Error))
		error = FE_LLA_I2C_ERROR;
	else {
		ChipAbort(pParams->hTuner, Abort);
		ChipAbort(pParams->hDemod, Abort);

		/*Check the error at the end of the function */
		if ((pParams->hDemod->Error) || (pParams->hTuner->Error))
			error = FE_LLA_I2C_ERROR;
	}
	return error;
}

void demod_get_pd(void *dummy_handle, unsigned short *level,
		  STCHIP_Handle_t hTuner)
{
	*level = ChipGetField(hTuner, F367_RF_AGC1_LEVEL_HI);
	*level = *level << 2;
	*level |= (ChipGetField(hTuner, F367_RF_AGC1_LEVEL_LO) & 0x03);
}

void demod_get_agc(void *dummy_handle, U16 *rf_agc, U16 *bb_agc,
		   STCHIP_Handle_t hTuner)
{
	U16 rf_low, rf_high, bb_low, bb_high;

	rf_low = ChipGetOneRegister(hTuner, R367ofdm_AGC1VAL1);
	rf_high = ChipGetOneRegister(hTuner, R367ofdm_AGC1VAL2);

	rf_high <<= 8;
	rf_low &= 0x00ff;
	*rf_agc = (rf_high + rf_low) << 4;

	bb_low = ChipGetOneRegister(hTuner, R367ofdm_AGC2VAL1);
	bb_high = ChipGetOneRegister(hTuner, R367ofdm_AGC2VAL2);

	bb_high <<= 8;
	bb_low &= 0x00ff;
	*bb_agc = (bb_high + bb_low) << 4;

}

U16 bbagc_min_start = 0xffff;

void demod_set_agclim(void *dummy_handle, U16 dir_up, STCHIP_Handle_t hTuner)
{
	U8 agc_min = 0;

	agc_min = ChipGetOneRegister(hTuner, R367ofdm_AGC2MIN);

	if (bbagc_min_start == 0xffff)
		bbagc_min_start = agc_min;

	if (dir_up) {
		if ((agc_min >= bbagc_min_start) && (agc_min <= (0xa4 - 0x04)))
			agc_min += 0x04;

	} else {
		if ((agc_min >= (bbagc_min_start + 0x04)) && (agc_min <= 0xa4))
			agc_min -= 0x04;
	}

	ChipSetOneRegister(hTuner, R367ofdm_AGC2MIN, agc_min);
}
