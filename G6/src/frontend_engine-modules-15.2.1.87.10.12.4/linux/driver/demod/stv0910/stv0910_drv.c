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

Source file name : stv0910_drv.c
Author :

Low level driver for STV0910

Date        Modification                                    Name
----        ------------                                    --------
23-Apr-12   Created

************************************************************************/

/* includes ---------------------------------------------------------------- */
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 2, 2)
#include <linux/export.h>
#else
#include <linux/module.h>
#endif
#include "stv0910_util.h"
#include "stv0910_drv.h"
#include <gen_macros.h>
#include <stvvglna.h>

#ifdef HOST_PC
#ifndef NO_GUI
#include <STV0910_GUI.h>
#include <Appl.h>
#include <Pnl_Report.h>
#include <formatio.h>
#include <UserPar.h>
#include "TCLNA_init.h"
#else
#include "stvvglna.h"
#endif

#else
#include "stvvglna.h"
#endif

#define DmdLock_TIMEOUT_LIMIT 550

#define STV0910_BLIND_SEARCH_AGC2BANDWIDTH 40

#define STV0910_IQPOWER_THRESHOLD 30

unsigned long search_start_time;

/****************************************************************
 ****************************************************************
 ****************************************************************
 ****
 ***************************************************
 **	Static Data
 ***************************************************
 ****
 ****
 ****************************************************************
 ****************************************************************
 ****************************************************************/

/************************
  Current LLA revision
 *************************/
static const ST_Revision_t RevisionSTV0910 = "STV0910-LLA_REL_2.7_August_2013";

/***********************************
  VTH vs. CNR(data normalized module)
 ***********************************/

static FE_STV0910_LOOKUP_t FE_STV0910_VTH_Lookup = {

	5,
	{
	 {250, 8780},		/*C/N = 1.5dB */
	 {100, 7405},		/*C/N = 4.5dB */
	 {40, 6330},		/*C/N = 6.5dB */
	 {12, 5224},		/*C/N = 8.5dB */
	 {5, 4236}		/*C/N = 10.5dB */

	 }
};

/****************************
  PADC(dBm) vs. (POWERI^2 + POWERQ^2) with 1Vpp ADC range
 ******************************/

FE_STV0910_LOOKUP_t FE_STV0910_PADC_Lookup = {

	15,
	{
	 {-2000, 1179}, /*PADC=-20dBm*/
	 {-1900, 1485}, /*PADC=-19dBm*/
	 {-1700, 2354}, /*PADC=-17dBm*/
	 {-1500, 3730}, /*PADC=-15dBm*/
	 {-1300, 5910}, /*PADC=-13dBm*/
	 {-1100, 9380}, /*PADC=-11dBm*/
	 {-900, 14850}, /*PADC=-9dBm*/
	 {-700, 23520}, /*PADC=-7dBm*/
	 {-600, 29650}, /*PADC=-6dBm*/
	 {-500, 37300}, /*PADC=-5dBm*/
	 {-400, 47000}, /*PADC=-4dBm*/
	 {-300, 59100}, /*PADC=-3dBm*/
	 {-200, 74500}, /*PADC=-2dBm*/
	 {-100, 93600}, /*PADC=-1dBm*/
	 {   0, 118000}  /*PADC=+0dBm*/
	}
};

/**********************
  DVBS2 C/N Look - Up table
 ***********************/
/*! \def FE_STV0910_S2_CN_LookUp
  \brief FE_STV0910_S2_CN_LookUp lockup table is used to convert register value
  to C/N value in case of DVBS2.
  */
FE_STV0910_LOOKUP_t FE_STV0910_S2_CN_LookUp = {

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
	 {510, 463}		/*C/N = 51.0dB */

	 }
};

/************************
  DVBS1 and DSS C/N Look - Up table
 *************************/
/*! \def FE_STV0910_S1_CN_LookUp
  \brief FE_STV0910_S1_CN_LookUp lockup table is used to convert register value
  to C/N value in case of DVBS/DSS.
  */
FE_STV0910_LOOKUP_t FE_STV0910_S1_CN_LookUp = {

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

/*! \struct FE_STV0910_CarLoopVsModeCode_s
  \brief FE_STV0910_CarLoopVsModeCode_s ...
  */
struct FE_STV0910_CarLoopVsModeCode_s {
	FE_STV0910_ModCod_t ModCode;
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

/*! \struct FE_STV0910_CarLoopVsModulation_s
  \brief FE_STV0910_CarLoopVsMModulation_s ...
  */

struct FE_STV0910_CarLoopVsModulation_s {
	FE_STV0910_Modulation_t Modulation;
	U8 CarLoop_2;		/* SR < 3msps   */
	U8 CarLoop_5;		/* 3 < SR <= 7msps    */
	U8 CarLoop_10;		/* 7 < SR <= 15msps  */
	U8 CarLoop_20;		/* 10 < SR <= 25msps */
	U8 CarLoop_30;		/* 10 < SR <= 45msps */

};

/*****************************************************************
  Tracking carrier loop carrier QPSK 1/4 to QPSK 2/5 long Frame
  Tracking carrier loop carrier QPSK 1/2 to 8PSK 9/10 long Frame
  Tracking carrier loop carrier 16APSK 2/3 to 32APSK 9/10 long Frame
 ******************************************************************/
#define NB_SAT_MODCOD 28
static
struct FE_STV0910_CarLoopVsModeCode_s FE_STV0910_S2CarLoop[NB_SAT_MODCOD] = {
/*Modcod, 2MPon, 2MPoff, 5MPon, 5MPoff, 10MPon, 10MPoff, 20MPon, 20MPoff,
 * 30MPon, 30MPoff, */

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
	/* 16ASPSK */
	{FE_SAT_16APSK_23, 0x09, 0x29, 0x09, 0x29, 0x29, 0x29, 0x29, 0x29, 0x29,
	 0x29},
	{FE_SAT_16APSK_34, 0x19, 0x29, 0x19, 0x29, 0x39, 0x29, 0x0A, 0x29, 0x29,
	 0x29},
	{FE_SAT_16APSK_45, 0x0A, 0x39, 0x0A, 0x39, 0x1A, 0x39, 0x1A, 0x39, 0x1A,
	 0x39},
	{FE_SAT_16APSK_56, 0x1A, 0x39, 0x1A, 0x39, 0x0B, 0x39, 0x2A, 0x39, 0x2A,
	 0x39},
	{FE_SAT_16APSK_89, 0x2A, 0x39, 0x2A, 0x39, 0x1B, 0x39, 0x3A, 0x39, 0x3A,
	 0x39},
	{FE_SAT_16APSK_910, 0x3A, 0x0A, 0x3A, 0x0A, 0x1B, 0x0A, 0x3A, 0x0A,
	 0x3A, 0x0A},
	/* 32ASPSK */
	{FE_SAT_32APSK_34, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
	 0x09},
	{FE_SAT_32APSK_45, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
	 0x09},
	{FE_SAT_32APSK_56, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
	 0x09},
	{FE_SAT_32APSK_89, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
	 0x09},
	{FE_SAT_32APSK_910, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09, 0x09,
	 0x09, 0x09}
};

#if 0
/***********************************************************************
  Tracking carrier loop carrier  normal Frame,
 ************************************************************************/
static struct FE_STV0910_CarLoopVsModulation_s FE_STV0910_S2NormalCarLoop[4] = {
	/* Mod, 2M, 5M, 10M, 20M, 30M */

	{FE_SAT_MOD_QPSK, 0x2C, 0x2B, 0x0B, 0x0B, 0x3A},
	{FE_SAT_MOD_8PSK, 0x3B, 0x0B, 0x2A, 0x0A, 0x39},
	{FE_SAT_MOD_16APSK, 0x1B, 0x1B, 0x1B, 0x3A, 0x2A},
	{FE_SAT_MOD_32APSK, 0x09, 0x09, 0x09, 0x09, 0x09}
};

/***********************************************************************
  Tracking carrier loop carrier  short Frame,
 ************************************************************************/
static struct FE_STV0910_CarLoopVsModulation_s FE_STV0910_S2ShortCarLoop[4] = {
	/* Mod, 2M, 5M, 10M, 20M, 30M */

	{FE_SAT_MOD_QPSK, 0x2C, 0x2B, 0x0B, 0x0B, 0x3A},
	{FE_SAT_MOD_8PSK, 0x3B, 0x0B, 0x2A, 0x0A, 0x39},
	{FE_SAT_MOD_16APSK, 0x1B, 0x1B, 0x1B, 0x3A, 0x2A},
	{FE_SAT_MOD_32APSK, 0x09, 0x09, 0x09, 0x09, 0x09}
};
#endif

/*************************************************************
 *************************************************************
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

static S32 FE_STV0910_TimingGetOffset(STCHIP_Handle_t hChip, U32 SymbolRate,
				      FE_STV0910_DEMOD_t Demod);
static U32 FE_STV0910_GetBer(STCHIP_Handle_t hChip, FE_STV0910_DEMOD_t Demod);
static FE_STV0910_Rate_t FE_STV0910_GetViterbiPunctureRate(STCHIP_Handle_t
							   hChip,
							   FE_STV0910_DEMOD_t
							   Demod);
static void FE_STV0910_GetLockTimeout(S32 *DemodTimeout, S32 *FecTimeout,
				      S32 SymbolRate,
				      FE_STV0910_SearchAlgo_t Algo);
static void FE_STV0910_setVTH(STCHIP_Handle_t hChip, FE_STV0910_DEMOD_t Demod,
			      FE_STV0910_LOOKUP_t *lookup);
static BOOL FE_STV0910_GetDemodLock(STCHIP_Handle_t hChip,
				    FE_STV0910_DEMOD_t Demod, S32 TimeOut);
static BOOL FE_STV0910_GetFECLock(STCHIP_Handle_t hChip,
				  FE_STV0910_DEMOD_t Demod, S32 TimeOut);
static void FE_STV0910_SetViterbiStandard(STCHIP_Handle_t hChip,
					  FE_STV0910_SearchStandard_t Standard,
					  FE_STV0910_Rate_t PunctureRate,
					  FE_STV0910_DEMOD_t Demod);
static S32 FE_STV0910_CheckVGLNAPresent(STCHIP_Handle_t hChipVGLNA);
static void FE_STV0910_SetSearchStandard(FE_STV0910_InternalParams_t *pParams,
					 FE_STV0910_DEMOD_t Demod);
static void FE_STV0910_StartSearch(FE_STV0910_InternalParams_t *pParams,
				   FE_STV0910_DEMOD_t Demod);
static void FE_STV0910_TrackingOptimization(FE_STV0910_InternalParams_t *
					    pParams, FE_STV0910_DEMOD_t Demod);
static FE_STV0910_SIGNALTYPE_t
FE_STV0910_GetSignalParams(FE_STV0910_InternalParams_t *pParams,
			   FE_STV0910_DEMOD_t Demod, U32 Satellite_Scan);
static BOOL FE_STV0910_GetDemodLockCold(FE_STV0910_InternalParams_t *pParams,
					S32 DemodTimeout,
					FE_STV0910_DEMOD_t Demod);
static U16 FE_STV0910_BlindCheckAGC2BandWidth(FE_STV0910_InternalParams_t *
					      pParams,
					      FE_STV0910_DEMOD_t Demod);
static BOOL FE_STV0910_BlindSearchAlgo(FE_STV0910_InternalParams_t *pParams,
				       S32 demodTimeout,
				       FE_STV0910_DEMOD_t Demod,
				       U32 Satellite_Scan);
static FE_STV0910_Error_t FE_STV0910_8PSKCarrierAdaptation(FE_STV0910_Handle_t
							   Handle,
							   FE_STV0910_DEMOD_t
							   Demod);
static FE_STV0910_SIGNALTYPE_t FE_STV0910_Algo(FE_STV0910_InternalParams_t *
					       pParams,
					       FE_STV0910_DEMOD_t Demod,
					       U32 Satellite_Scan);
static void FE_STV0910_SetChargePump(FE_STV0910_Handle_t Handle, U32 n_div);

/*****************************************************
 **FUNCTION	::	FE_STV0910_GetMclkFreq
 **ACTION	::	Returns the STV0910 master clock frequency
 **PARAMS IN	::  hChip		==>	handle to the chip
 **				ExtClk	==>	External clock frequency(Hz)
 **PARAMS OUT::	NONE
 **RETURN	::	Synthesizer frequency(Hz)
 *****************************************************/
/*! \fn FE_STV0910_GetMclkFreq(STCHIP_Handle_t hChip, U32 ExtClk)
  \brief This function return s the master clock frequency.
  */
U32 FE_STV0910_GetMclkFreq(STCHIP_Handle_t hChip, U32 ExtClk)
{
	U32 mclk = 135000000, ndiv = 1, idf = 1, ldf = 1, odf = 1, fvco = 1;

	odf = ChipGetField(hChip, FSTV0910_ODF);
	ndiv = ChipGetOneRegister(hChip, RSTV0910_NCOARSE1);
	idf = ChipGetField(hChip, FSTV0910_IDF);

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
}

/*****************************************************
 **FUNCTION	::	FE_STV0910_GetErrorCount
 **ACTION	::	return the number of errors from a given counter
 **PARAMS IN	::  hChip		==>	handle to the chip
 **			::	Counter		==>	used counter 1 or 2.
 ::  Demod		==> cirrent demod 1 or 2.
 **PARAMS OUT::	NONE
 **RETURN	::	Synthesizer frequency(Hz)
 *****************************************************/
/*! \fn U32 FE_STV0910_GetErrorCount(STCHIP_Handle_t hChip,
		FE_STV0910_ERRORCOUNTER Counter, FE_STV0910_DEMOD_t Demod)
  \brief This function return s the error register value.
  */
U32 FE_STV0910_GetErrorCount(STCHIP_Handle_t hChip,
			     FE_STV0910_ERRORCOUNTER Counter,
			     FE_STV0910_DEMOD_t Demod)
{
	U32 lsb, msb, hsb, errValue;

	S32 err1Field_hsb,
	    err1Field_msb,
	    err1Field_lsb,
	    err2Field_hsb, err2Field_msb, err2Field_lsb, err1Reg, err2Reg;

	/*Define Error fields depending of the used Demod */
	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		err1Field_hsb = FSTV0910_P1_ERR_CNT12;
		err1Field_msb = FSTV0910_P1_ERR_CNT11;
		err1Field_lsb = FSTV0910_P1_ERR_CNT10;
		err1Reg = RSTV0910_P1_ERRCNT12;

		err2Field_hsb = FSTV0910_P1_ERR_CNT22;
		err2Field_msb = FSTV0910_P1_ERR_CNT21;
		err2Field_lsb = FSTV0910_P1_ERR_CNT20;
		err2Reg = RSTV0910_P1_ERRCNT22;
		break;

	case FE_SAT_DEMOD_2:
		err1Field_hsb = FSTV0910_P2_ERR_CNT12;
		err1Field_msb = FSTV0910_P2_ERR_CNT11;
		err1Field_lsb = FSTV0910_P2_ERR_CNT10;
		err1Reg = RSTV0910_P2_ERRCNT12;

		err2Field_hsb = FSTV0910_P2_ERR_CNT22;
		err2Field_msb = FSTV0910_P2_ERR_CNT21;
		err2Field_lsb = FSTV0910_P2_ERR_CNT20;
		err2Reg = RSTV0910_P2_ERRCNT22;
		break;
	}

	/*Read the Error value */
	switch (Counter) {
	case FE_STV0910_COUNTER1:
	default:
		ChipGetRegisters(hChip, err1Reg, 3);
		hsb = ChipGetFieldImage(hChip, err1Field_hsb);
		msb = ChipGetFieldImage(hChip, err1Field_msb);
		lsb = ChipGetFieldImage(hChip, err1Field_lsb);
		break;

	case FE_STV0910_COUNTER2:
		ChipGetRegisters(hChip, err2Reg, 3);
		hsb = ChipGetFieldImage(hChip, err2Field_hsb);
		msb = ChipGetFieldImage(hChip, err2Field_msb);
		lsb = ChipGetFieldImage(hChip, err2Field_lsb);
		break;
	}

	/*Cupute the Error value 3 bytes(HSB, MSB, LSB) */
	errValue = (hsb << 16) + (msb << 8) + (lsb);
	return errValue;
}

/*****************************************************
 **FUNCTION	::	STV0910_RepeaterFn  (First repeater)
 **ACTION	::	Set the repeater On or OFF
 **PARAMS IN	::  hChip		==>	handle to the chip
 ::	State		==> repeater state On/Off.
 **PARAMS OUT::	NONE
 **RETURN	::	Error(if any)
 *****************************************************/
/*! \fn STCHIP_Error_t FE_STV0910_RepeaterFn(STCHIP_Handle_t hChip, BOOL State)
  \brief This function is used to set the repeater to On or OFF
  */

#ifdef CHIP_STAPI
STCHIP_Error_t FE_STV0910_RepeaterFn(STCHIP_Handle_t hChip, BOOL State,
				     U8 *Buffer)
#else
STCHIP_Error_t FE_STV0910_RepeaterFn(STCHIP_Handle_t hChip, BOOL State)
#endif
{
	STCHIP_Error_t error = CHIPERR_NO_ERROR;

	if (hChip != NULL) {
#ifdef CONFIG_ARCH_STI
		ChipSetField(hChip, FSTV0910_P1_STOP_ENABLE, 0);
#endif
		if (State == TRUE)
#ifdef CHIP_STAPI
			ChipFillRepeaterMessage(hChip, FSTV0910_P1_I2CT_ON, 1,
						Buffer);
#else
			ChipSetField(hChip, FSTV0910_P1_I2CT_ON, 1);
#endif
	}

	return error;
}

/*****************************************************
 **FUNCTION	::	FE_STV0910_Repeater2Fn(Second repeater)
 **ACTION	::	Set the repeater On or OFF
 **PARAMS IN	::  hChip		==>	handle to the chip
 ::	State		==> repeater state On/Off.
 **PARAMS OUT::	NONE
 **RETURN	::	Error(if any)
 *****************************************************/
/*! \fn STCHIP_Error_t FE_STV0910_Repeater2Fn(STCHIP_Handle_t hChip, BOOL State)
  \brief This function is used to set the 2nd repeater to On or OFF
  */

#ifdef CHIP_STAPI
STCHIP_Error_t FE_STV0910_Repeater2Fn(STCHIP_Handle_t hChip, BOOL State,
				      U8 *Buffer)
#else
STCHIP_Error_t FE_STV0910_Repeater2Fn(STCHIP_Handle_t hChip, BOOL State)
#endif
{
	STCHIP_Error_t error = CHIPERR_NO_ERROR;

	if (hChip != NULL) {
#ifdef CONFIG_ARCH_STI
		ChipSetField(hChip, FSTV0910_P2_STOP_ENABLE, 0);
#endif
		if (State == TRUE)
#ifdef CHIP_STAPI
			ChipFillRepeaterMessage(hChip, FSTV0910_P2_I2CT_ON, 1,
						Buffer);
#else
			ChipSetField(hChip, FSTV0910_P2_I2CT_ON, 1);
#endif
	}

	return error;
}

/*****************************************************
  --FUNCTION	::	CarrierWidth
  --ACTION	::	Compute the width of the carrier
  --PARAMS IN	::	SymbolRate->Symbol rate of the carrier(Kbauds or Mbauds)
  --			RollOff	->Rolloff * 100
  --PARAMS OUT::	NONE
  --RETURN	::	Width of the carrier(KHz or MHz)
  --***************************************************/
/*! \fn U32 FE_STV0910_CarrierWidth(U32 SymbolRate,
					FE_STV0910_RollOff_t RollOff)
  \brief This function compute the the carrier width in fucntion of the symbol
  rate and the roll off
  */
U32 FE_STV0910_CarrierWidth(U32 SymbolRate, FE_STV0910_RollOff_t RollOff)
{
	U32 rolloff;
	U32 ret = 0;

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
	ret = (SymbolRate + (SymbolRate * rolloff) / 100);

	return ret;
}

/*****************************************************
  --FUNCTION	::	GetCarrierFrequency
  --ACTION	::	Return the carrier frequency offset
  --PARAMS IN	::	MasterClock	->	Masterclock frequency(Hz)
  ::	Demod	->	current demod 1 or 2
  --PARAMS OUT::	NONE
  --RETURN	::	Frequency offset in Hz.
  --***************************************************/
/*! \fn S32 FE_STV0910_GetCarrierFrequency(STCHIP_Handle_t hChip,
				U32 MasterClock, FE_STV0910_DEMOD_t Demod)
  \brief This function return s the carrier frequency offset
  */

S32 FE_STV0910_GetCarrierFrequency(STCHIP_Handle_t hChip, U32 MasterClock,
				   FE_STV0910_DEMOD_t Demod)
{
	S32 cfrField2,
	    cfrField1,
	    cfrField0,
	    derot, rem1, rem2, intval1, intval2, carrierFrequency, cfrReg;

	BOOL sign = 1;
	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		/* carrier frequency Byte 2 */
		cfrField2 = FSTV0910_P1_CAR_FREQ2;
		/* carrier frequency Byte 1 */
		cfrField1 = FSTV0910_P1_CAR_FREQ1;
		/* carrier frequency Byte 0 */
		cfrField0 = FSTV0910_P1_CAR_FREQ0;
		cfrReg = RSTV0910_P1_CFR2;
		break;

	case FE_SAT_DEMOD_2:
		/* carrier frequency Byte 2 */
		cfrField2 = FSTV0910_P2_CAR_FREQ2;
		/* carrier frequency Byte 1 */
		cfrField1 = FSTV0910_P2_CAR_FREQ1;
		/* carrier frequency Byte 0 */
		cfrField0 = FSTV0910_P2_CAR_FREQ0;
		cfrReg = RSTV0910_P2_CFR2;
		break;
	}

	/* Read the carrier frequency regs value */
	ChipGetRegisters(hChip, cfrReg, 3);
	derot = (ChipGetFieldImage(hChip, cfrField2) << 16) +
		(ChipGetFieldImage(hChip, cfrField1) << 8) +
		(ChipGetFieldImage(hChip, cfrField0));

	/* cumpute the signed value */
	derot = Get2Comp(derot, 24);
	if (derot < 0) {
		sign = 0;
		/* Use +ve values only to avoid -ve value truncation */
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
	/* only for integer calculation */
	derot = (intval1 * intval2) + ((intval1 * rem2) >> 12) + ((intval2 *
								   rem1) >> 12);

	if (sign == 1)
		carrierFrequency = derot;	/* positive offset */
	else
		carrierFrequency = -derot;	/* negative offset */

	return carrierFrequency;
}

/*! \fn S32 FE_STV0910_TimingGetOffset(STCHIP_Handle_t hChip, U32 SymbolRate,
						FE_STV0910_DEMOD_t Demod)
  \brief This function return s the timming offset
  */
static S32 FE_STV0910_TimingGetOffset(STCHIP_Handle_t hChip, U32 SymbolRate,
				      FE_STV0910_DEMOD_t Demod)
{
	S32 tmgreg, tmgField2, tmgField1, tmgField0, timingoffset;

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		tmgreg = RSTV0910_P1_TMGREG2;	/*Found timing offset reg HSB */

		tmgField2 = FSTV0910_P1_TMGREG2;
		tmgField1 = FSTV0910_P1_TMGREG1;
		tmgField0 = FSTV0910_P1_TMGREG0;
		break;
	case FE_SAT_DEMOD_2:
		tmgreg = RSTV0910_P2_TMGREG2;	/*Found timing offset reg HSB */

		tmgField2 = FSTV0910_P2_TMGREG2;
		tmgField1 = FSTV0910_P2_TMGREG1;
		tmgField0 = FSTV0910_P2_TMGREG0;

		break;
	}

	/*      Formulat :
	   SR_Offset = TMGRREG * SR /2^29
	   TMGREG is 3 bytes registers value SR is the current symbol rate
	 */
	ChipGetRegisters(hChip, tmgreg, 3);

	timingoffset = (ChipGetFieldImage(hChip, tmgField2) << 16) +
		(ChipGetFieldImage(hChip, tmgField1) << 8) +
		(ChipGetFieldImage(hChip, tmgField0));

	timingoffset = Get2Comp(timingoffset, 24);

	if (timingoffset == 0)
		timingoffset = 1;
	timingoffset = ((S32)SymbolRate * 10) / ((S32) 0x1000000 /
			timingoffset);
	timingoffset /= 320;

	return timingoffset;

}

/*****************************************************
  --FUNCTION	::	GetSymbolRate
  --ACTION	::	Get the current symbol rate
  --PARAMS IN	::	hChip		->	handle to the chip
  ::	MasterClock	->	Masterclock frequency(Hz)
  ::	Demod		->	current demod 1 or 2
  --PARAMS OUT::	NONE
  --RETURN	::	Symbol rate in Symbol/s
 *****************************************************/
/*! \fn U32 FE_STV0910_GetSymbolRate(STCHIP_Handle_t hChip, U32 MasterClock,
						FE_STV0910_DEMOD_t Demod)
  \brief This function return s the current symbole rate
  */
U32 FE_STV0910_GetSymbolRate(STCHIP_Handle_t hChip, U32 MasterClock,
			     FE_STV0910_DEMOD_t Demod)
{
	S32 sfrField3,
	    sfrField2,
	    sfrField1,
	    sfrField0, rem1, rem2, intval1, intval2, symbolRate, sfrReg;

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		sfrField3 = FSTV0910_P1_SYMB_FREQ3;	/*Found SR, byte 3 */
		sfrField2 = FSTV0910_P1_SYMB_FREQ2;	/*Found SR, byte 2 */
		sfrField1 = FSTV0910_P1_SYMB_FREQ1;	/*Found SR, byte 1 */
		sfrField0 = FSTV0910_P1_SYMB_FREQ0;	/*Found SR, byte 0 */

		sfrReg = RSTV0910_P1_SFR3;
		break;
	case FE_SAT_DEMOD_2:
		sfrField3 = FSTV0910_P2_SYMB_FREQ3;	/*Found SR, byte 3 */
		sfrField2 = FSTV0910_P2_SYMB_FREQ2;	/*Found SR, byte 2 */
		sfrField1 = FSTV0910_P2_SYMB_FREQ1;	/*Found SR, byte 1 */
		sfrField0 = FSTV0910_P2_SYMB_FREQ0;	/*Found SR, byte 0 */

		sfrReg = RSTV0910_P2_SFR3;
		break;
	}

	ChipGetRegisters(hChip, sfrReg, 4);
	symbolRate = (ChipGetFieldImage(hChip, sfrField3) << 24) +
		(ChipGetFieldImage(hChip, sfrField2) << 16) +
		(ChipGetFieldImage(hChip, sfrField1) << 8) +
		(ChipGetFieldImage(hChip, sfrField0));

	/*      Formulat :
	   Found_SR = Reg * MasterClock /2^32
	 */

	intval1 = (MasterClock) >> 16;
	intval2 = (symbolRate) >> 16;

	rem1 = (MasterClock) % 0x10000;
	rem2 = (symbolRate) % 0x10000;

	/* only for integer calculation */
	symbolRate = (intval1 * intval2) + ((intval1 * rem2) >> 16) +
			((intval2 * rem1) >> 16);

	return symbolRate;

}

/*****************************************************
  --FUNCTION	::	SetSymbolRate
  --ACTION	::	Get the Symbol rate
  --PARAMS IN	::	hChip		->	handle to the chip
  ::	MasterClock	->	Masterclock frequency(Hz)
  ::	SymbolRate	->	Symbol Rate(Symbol/s)
  ::	Demod		->	current demod 1 or 2
  --PARAMS OUT::	NONE
  --RETURN	::	None
 *****************************************************/
/*! \fn void FE_STV0910_SetSymbolRate(STCHIP_Handle_t hChip, U32 MasterClock,
				U32 SymbolRate, FE_STV0910_DEMOD_t Demod)
  \brief This function sets the sfrInitReg in fn of the current symbole rate
  */
void FE_STV0910_SetSymbolRate(STCHIP_Handle_t hChip, U32 MasterClock,
			      U32 SymbolRate, FE_STV0910_DEMOD_t Demod)
{
	S32 sfrInitReg;

	U32 symb;

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		/* Symbol rate init reg */
		sfrInitReg = RSTV0910_P1_SFRINIT1;
		break;
	case FE_SAT_DEMOD_2:
		/* Symbol rate init reg */
		sfrInitReg = RSTV0910_P2_SFRINIT1;
		break;
	}

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

	/* Write the MSB */
	ChipSetOneRegister(hChip, sfrInitReg, (symb >> 8) & 0x7F);
	/* Write the LSB */
	ChipSetOneRegister(hChip, sfrInitReg + 1, (symb & 0xFF));

}

/*****************************************************
  --FUNCTION	::	GetStandard
  --ACTION	::	Return the current standrad(DVBS1, DSS or DVBS2
  --PARAMS IN	::	hChip		->	handle to the chip
  ::	Demod		->	current demod 1 or 2
  --PARAMS OUT::	NONE
  --RETURN	::	standard(DVBS1, DVBS2 or DSS
 *****************************************************/
/*! \fn FE_STV0910_TrackingStandard_t FE_STV0910_GetStandard(STCHIP_Handle_t
						hChip, FE_STV0910_DEMOD_t Demod)
  \brief This function return s the current standrad(DVBS1, DSS or DVBS2)
  */
FE_STV0910_TrackingStandard_t FE_STV0910_GetStandard(STCHIP_Handle_t hChip,
						     FE_STV0910_DEMOD_t Demod)
{
	FE_STV0910_TrackingStandard_t foundStandard;
	S32 stateField, dssdvbField, state;

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		/* state machine search status field */
		stateField = FSTV0910_P1_HEADER_MODE;
		/* Viterbi standard field */
		dssdvbField = FSTV0910_P1_DSS_DVB;
		break;
	case FE_SAT_DEMOD_2:
		/* state machine search status field */
		stateField = FSTV0910_P2_HEADER_MODE;
		/* Viterbi standard field */
		dssdvbField = FSTV0910_P2_DSS_DVB;
		break;
	}

	state = ChipGetField(hChip, stateField);

	if (state == 2)
		/* The demod Find DVBS2 */
		foundStandard = FE_SAT_DVBS2_STANDARD;

	else if (state == 3) {	/* The demod Find DVBS1/DSS */
		if (ChipGetField(hChip, dssdvbField) == 1)
			/* Viterbi Find DSS */
			foundStandard = FE_SAT_DSS_STANDARD;
		else
			/* Viterbi Find DVBS1 */
			foundStandard = FE_SAT_DVBS1_STANDARD;
	} else
		/* The demod find nothing, unknown standard */
		foundStandard = FE_SAT_UNKNOWN_STANDARD;
	return foundStandard;
}

/*****************************************************
  --FUNCTION	::	CarrierGetQuality
  --ACTION	::	Return the carrier to noise of the current carrier
  --PARAMS IN	::	hChip	->	handle to the chip
  ::	lookup	->	LUT for CNR level estimation.
  ::	Demod	->	current demod 1 or 2
  --PARAMS OUT::	NONE
  --RETURN	::	C/N of the carrier, 0 if no carrier
  --***************************************************/
/*! \fn S32 FE_STV0910_CarrierGetQuality(STCHIP_Handle_t hChip,
			FE_STV0910_LOOKUP_t *lookup, FE_STV0910_DEMOD_t Demod)
  \brief This function return s the carrier to noise of the current carrier
  */
S32 FE_STV0910_CarrierGetQuality(STCHIP_Handle_t hChip,
				 FE_STV0910_LOOKUP_t *lookup,
				 FE_STV0910_DEMOD_t Demod)
{
	S32 c_n = -100,
	    regval,
	    Imin, Imax, i, lockFlagField, noiseField1, noiseField0, noiseReg;

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		lockFlagField = FSTV0910_P1_LOCK_DEFINITIF;
		if (FE_STV0910_GetStandard(hChip, Demod) ==
		    FE_SAT_DVBS2_STANDARD) {
			/*If DVBS2 use PLH normilized noise indicators */
			noiseField1 = FSTV0910_P1_NOSPLHT_NORMED1;
			noiseField0 = FSTV0910_P1_NOSPLHT_NORMED0;
			noiseReg = RSTV0910_P1_NNOSPLHT1;
		} else {
			/*if not DVBS2 use symbol normalized noise indicators */
			noiseField1 = FSTV0910_P1_NOSDATAT_NORMED1;
			noiseField0 = FSTV0910_P1_NOSDATAT_NORMED0;
			noiseReg = RSTV0910_P1_NNOSDATAT1;
		}
		break;

	case FE_SAT_DEMOD_2:
		lockFlagField = FSTV0910_P2_LOCK_DEFINITIF;

		if (FE_STV0910_GetStandard(hChip, Demod) ==
		    FE_SAT_DVBS2_STANDARD) {
			/*If DVBS2 use PLH normilized noise indicators */
			noiseField1 = FSTV0910_P2_NOSPLHT_NORMED1;
			noiseField0 = FSTV0910_P2_NOSPLHT_NORMED0;
			noiseReg = RSTV0910_P2_NNOSPLHT1;
		} else {
			/*if not DVBS2 use symbol normalized noise indicators */
			noiseField1 = FSTV0910_P2_NOSDATAT_NORMED1;
			noiseField0 = FSTV0910_P2_NOSDATAT_NORMED0;
			noiseReg = RSTV0910_P2_NNOSDATAT1;
		}
		break;
	}

	if (!ChipGetField(hChip, lockFlagField))
		return c_n;

	if ((lookup == NULL) || !lookup->size)
		return c_n;

	regval = 0;
	/* ChipWaitOrAbort(hChip, 5); */
	for (i = 0; i < /*16 */ 8; i++) {
		ChipGetRegisters(hChip, noiseReg, 2);
		regval += MAKEWORD(ChipGetFieldImage(hChip, noiseField1),
				    ChipGetFieldImage(hChip, noiseField0));
		ChipWaitOrAbort(hChip, 1);
	}
	regval /= /*16 */ 8;

	Imin = 0;
	Imax = lookup->size - 1;

	if (INRANGE(lookup->table[Imin].regval, regval,
				lookup->table[Imax].regval)) {
		while ((Imax - Imin) > 1) {
			i = (Imax + Imin) >> 1;
			if (INRANGE(lookup->table[Imin].regval, regval,
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

	return c_n;
}

/*****************************************************
  --FUNCTION	::	GetBer
  --ACTION	::	Return the Viterbi BER if DVBS1/DSS or the PER if DVBS2
  --PARAMS IN	::	hChip	->	handle to the chip
  ::	Demod	->	current demod 1 or 2
  --PARAMS OUT::	NONE
  --RETURN	::	ber/per scalled to 1e7
  --***************************************************/
/*! \fn U32 FE_STV0910_GetBer(STCHIP_Handle_t hChip, FE_STV0910_DEMOD_t Demod)
  \brief This function return the Viterbi BER if DVBS1/DSS or the PER if DVBS2
  */
static U32 FE_STV0910_GetBer(STCHIP_Handle_t hChip, FE_STV0910_DEMOD_t Demod)
{
	U32 ber = 10000000, i;
	S32 demdStateReg, demodState, prVitField, pdellockField;

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		demdStateReg = FSTV0910_P1_HEADER_MODE;
		prVitField = FSTV0910_P1_PRFVIT;
		pdellockField = FSTV0910_P1_PKTDELIN_LOCK;
		break;

	case FE_SAT_DEMOD_2:
		demdStateReg = FSTV0910_P2_HEADER_MODE;
		prVitField = FSTV0910_P2_PRFVIT;
		pdellockField = FSTV0910_P2_PKTDELIN_LOCK;
		break;
	}

	demodState = ChipGetField(hChip, demdStateReg);

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
			    FE_STV0910_GetErrorCount(hChip, FE_STV0910_COUNTER1,
						     Demod);
		}

		ber /= 5;

		/* Check for carrier */
		if (ChipGetField(hChip, prVitField)) {
			ber *= 9766;	/* ber = ber * 10^7/2^10 */
			/* theses two lines => ber = ber * 10^7/2^20 */
			ber = ber >> 13;
		}
		break;

	case FE_SAT_DVBS2_FOUND:

		ber = 0;
		/* Average 5 ber values */
		for (i = 0; i < 5; i++) {
			ChipWaitOrAbort(hChip, 5);
			ber +=
			    FE_STV0910_GetErrorCount(hChip, FE_STV0910_COUNTER1,
						     Demod);
		}
		ber /= 5;

		/* Check for S2 FEC Lock */
		if (ChipGetField(hChip, pdellockField)) {
			ber *= 9766;	/* ber = ber * 10^7/2^10 */
			/* theses two lines => PER = ber * 10^7/2^23 */
			ber = ber >> 13;
		}

		break;
	}

	return ber;
}

/*! \fn FE_STV0910_Rate_t FE_STV0910_GetViterbiPunctureRate(STCHIP_Handle_t
						hChip, FE_STV0910_DEMOD_t Demod)
  \brief This function return s the puncture rate
  */
static FE_STV0910_Rate_t FE_STV0910_GetViterbiPunctureRate(STCHIP_Handle_t
							   hChip,
							   FE_STV0910_DEMOD_t
							   Demod)
{
	S32 rateField;
	FE_STV0910_Rate_t punctureRate;

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		rateField = ChipGetField(hChip, FSTV0910_P1_VIT_CURPUN);
		break;

	case FE_SAT_DEMOD_2:
		rateField = ChipGetField(hChip, FSTV0910_P2_VIT_CURPUN);
		break;
	}

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

/*! \fn void FE_STV0910_GetLockTimeout(S32 *DemodTimeout, S32 *FecTimeout,
				S32 SymbolRate, FE_STV0910_SearchAlgo_t Algo)
  \brief This function return s the demod and fec lock timeout in function of
  the symblo rate
  */
static void FE_STV0910_GetLockTimeout(S32 *DemodTimeout, S32 *FecTimeout,
				      S32 SymbolRate,
				      FE_STV0910_SearchAlgo_t Algo)
{
	switch (Algo) {
	case FE_SAT_BLIND_SEARCH:

		/* 10Msps < SR <= 15Msps */
		if (SymbolRate <= 1500000) {
			(*DemodTimeout) = 2000;
			(*FecTimeout) = 500;
			/* 10Msps < SR <= 15Msps */
		} else if (SymbolRate <= 5000000) {
			(*DemodTimeout) = 1000;
			(*FecTimeout) = 300;
			/* 5Msps < SR <= 30Msps */
		} else if (SymbolRate <= 30000000) {
			(*DemodTimeout) = 700;
			(*FecTimeout) = 300;
			/* 30Msps < SR <= 45Msps */
		} else if (SymbolRate <= 45000000) {
			(*DemodTimeout) = 400;
			(*FecTimeout) = 200;
		} else {	/* SR > 45Msps */

			(*DemodTimeout) = 300;
			(*FecTimeout) = 100;
		}

		break;

	case FE_SAT_COLD_START:
	case FE_SAT_WARM_START:
	default:
		if (SymbolRate <= 1000000) {	/* SR <= 1Msps */
			(*DemodTimeout) = 3000;
			(*FecTimeout) = 2000;	/* 1700 */
			/* 1Msps < SR <= 2Msps */
		} else if (SymbolRate <= 2000000) {
			(*DemodTimeout) = 2500;
			(*FecTimeout) = 1300;	/* 1100 */
			/* 2Msps < SR <= 5Msps */
		} else if (SymbolRate <= 5000000) {
			(*DemodTimeout) = 1000;
			(*FecTimeout) = 650;	/* 550 */
			/* 5Msps < SR <= 10Msps */
		} else if (SymbolRate <= 10000000) {
			(*DemodTimeout) = 700;
			(*FecTimeout) = 350;	/* 250 */
			/* 10Msps < SR <= 20Msps */
		} else if (SymbolRate <= 20000000) {
			(*DemodTimeout) = 400;
			(*FecTimeout) = 200;	/* 130 */
		} else {	/* SR > 20Msps */

			(*DemodTimeout) = 300;
			(*FecTimeout) = 200;	/* 150 */
		}
		break;

	}
	if (Algo == FE_SAT_WARM_START)
		/*if warm start
		   demod timeout = coldtimeout/3
		   fec timeout = same as cold */
		(*DemodTimeout) /= 2;
}

static void FE_STV0910_setVTH(STCHIP_Handle_t hChip, FE_STV0910_DEMOD_t Demod,
			      FE_STV0910_LOOKUP_t *lookup)
{
	S32 vth = 250,		/* Min = 5, max = 250 */
	    regval, Imin, Imax, i, noiseField1, noiseField0, noiseReg;

#ifdef HOST_PC
#ifndef NO_GUI
	double lockTime;
	char message[100];
	lockTime = Timer();
#endif
#endif

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		noiseField1 = FSTV0910_P1_NOSDATAT_NORMED1;
		noiseField0 = FSTV0910_P1_NOSDATAT_NORMED0;
		noiseReg = RSTV0910_P1_NNOSDATAT1;
		break;
	case FE_SAT_DEMOD_2:
		noiseField1 = FSTV0910_P2_NOSDATAT_NORMED1;
		noiseField0 = FSTV0910_P2_NOSDATAT_NORMED0;
		noiseReg = RSTV0910_P2_NNOSDATAT1;
		break;
	}

	if ((lookup != NULL) && lookup->size) {
		ChipGetRegisters(hChip, noiseReg, 2);
		regval = MAKEWORD(ChipGetFieldImage(hChip, noiseField1),
			     ChipGetFieldImage(hChip, noiseField0));

		Imin = 0;
		Imax = lookup->size - 1;

		if (INRANGE(lookup->table[Imin].regval, regval,
					lookup->table[Imax].regval)) {
			while ((Imax - Imin) > 1) {
				i = (Imax + Imin) >> 1;
				/*equivalent to i = (Imax+Imin)/2; */
				if (INRANGE(lookup->table[Imin].regval, regval,
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

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		if (ChipGetFieldImage(hChip, FSTV0910_P1_VTH12) > vth)
			ChipSetFieldImage(hChip, FSTV0910_P1_VTH12, vth);
		if (ChipGetFieldImage(hChip, FSTV0910_P1_VTH23) > vth)
			ChipSetFieldImage(hChip, FSTV0910_P1_VTH23, vth);
		if (ChipGetFieldImage(hChip, FSTV0910_P1_VTH34) > vth)
			ChipSetFieldImage(hChip, FSTV0910_P1_VTH34, vth);
		if (ChipGetFieldImage(hChip, FSTV0910_P1_VTH56) > vth)
			ChipSetFieldImage(hChip, FSTV0910_P1_VTH56, vth);
		if (ChipGetFieldImage(hChip, FSTV0910_P1_VTH67) > vth)
			ChipSetFieldImage(hChip, FSTV0910_P1_VTH67, vth);
		if (ChipGetFieldImage(hChip, FSTV0910_P1_VTH78) > vth)
			ChipSetFieldImage(hChip, FSTV0910_P1_VTH78, vth);
		ChipSetRegisters(hChip, RSTV0910_P1_VTH12, 6);
		break;

	case FE_SAT_DEMOD_2:
		if (ChipGetFieldImage(hChip, FSTV0910_P2_VTH12) > vth)
			ChipSetFieldImage(hChip, FSTV0910_P2_VTH12, vth);
		if (ChipGetFieldImage(hChip, FSTV0910_P2_VTH23) > vth)
			ChipSetFieldImage(hChip, FSTV0910_P2_VTH23, vth);
		if (ChipGetFieldImage(hChip, FSTV0910_P2_VTH34) > vth)
			ChipSetFieldImage(hChip, FSTV0910_P2_VTH34, vth);
		if (ChipGetFieldImage(hChip, FSTV0910_P2_VTH56) > vth)
			ChipSetFieldImage(hChip, FSTV0910_P2_VTH56, vth);
		if (ChipGetFieldImage(hChip, FSTV0910_P2_VTH67) > vth)
			ChipSetFieldImage(hChip, FSTV0910_P2_VTH67, vth);
		if (ChipGetFieldImage(hChip, FSTV0910_P2_VTH78) > vth)
			ChipSetFieldImage(hChip, FSTV0910_P2_VTH78, vth);
		ChipSetRegisters(hChip, RSTV0910_P2_VTH12, 6);
		break;
	}

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

/*! \fn  BOOL FE_STV0910_GetDemodLock(STCHIP_Handle_t hChip,
					FE_STV0910_DEMOD_t Demod, S32 TimeOut)
  \brief This function return s the demod lock status
  */
static BOOL FE_STV0910_GetDemodLock(STCHIP_Handle_t hChip,
				    FE_STV0910_DEMOD_t Demod, S32 TimeOut)
{
	S32 timer = 0, lock = 0, headerField, lockField, symbFreq2, symbFreq3;
	U32 symbolRate;
	U32 TimeOut_SymbRate, SRate_1MSymb_Sec;

	FE_STV0910_SEARCHSTATE_t demodState;

#ifdef HOST_PC
#ifndef NO_GUI
	double lockTime;
	char message[100];
	lockTime = Timer();
#endif
#endif

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		/* state machine search status field */
		headerField = FSTV0910_P1_HEADER_MODE;
		/* Demod lock status field */
		lockField = FSTV0910_P1_LOCK_DEFINITIF;
		symbFreq2 = FSTV0910_P1_SYMB_FREQ2;
		symbFreq3 = FSTV0910_P1_SYMB_FREQ3;
		break;

	case FE_SAT_DEMOD_2:
		/* state machine search status field */
		headerField = FSTV0910_P2_HEADER_MODE;
		/* Demod lock status field */
		lockField = FSTV0910_P2_LOCK_DEFINITIF;
		symbFreq2 = FSTV0910_P2_SYMB_FREQ2;
		symbFreq3 = FSTV0910_P2_SYMB_FREQ3;
		break;
	}

	TimeOut_SymbRate = TimeOut;
	SRate_1MSymb_Sec = 0x01e5;

	while ((timer < TimeOut_SymbRate) && (lock == 0)) {
		/* TimeOut = f(Symbolrate) */
		symbolRate =
		    (ChipGetFieldImage(hChip, symbFreq3) << 8) +
		    (ChipGetFieldImage(hChip, symbFreq2));
		if (TimeOut < DmdLock_TIMEOUT_LIMIT)
			TimeOut_SymbRate = TimeOut;
		else {
			/* no division by 0 */
			if (symbolRate < SRate_1MSymb_Sec)
				symbolRate = SRate_1MSymb_Sec;
			else if (symbolRate > (5 * SRate_1MSymb_Sec))
				symbolRate = 5 * SRate_1MSymb_Sec;

			TimeOut_SymbRate =
			    TimeOut / (symbolRate / SRate_1MSymb_Sec);

			/* no weird results */
			if (TimeOut_SymbRate < DmdLock_TIMEOUT_LIMIT)
				TimeOut_SymbRate = DmdLock_TIMEOUT_LIMIT;
			else if (TimeOut_SymbRate > TimeOut)
				TimeOut_SymbRate = TimeOut;

			/*new timeout is between 200 ms & original TimeOut */
		}
		demodState = ChipGetField(hChip, headerField);

		switch (demodState) {
		case FE_SAT_SEARCH:
		case FE_SAT_PLH_DETECTED:	/* no signal */
		default:
			lock = 0;
			break;

		case FE_SAT_DVBS2_FOUND:	/*found a DVBS2 signal */
		case FE_SAT_DVBS_FOUND:
			lock = ChipGetField(hChip, lockField);
			break;
		}

		/* Monitor CNR and Set VTH accordingly when DVB - S1(or DSS)
		 * search standard is selected */

		switch (Demod) {
		case FE_SAT_DEMOD_1:
		default:
			if (ChipGetFieldImage(hChip, FSTV0910_P1_DVBS1_ENABLE))
				FE_STV0910_setVTH(hChip, Demod,
						  &FE_STV0910_VTH_Lookup);
			break;

		case FE_SAT_DEMOD_2:
			if (ChipGetFieldImage(hChip, FSTV0910_P2_DVBS1_ENABLE))
				FE_STV0910_setVTH(hChip, Demod,
						  &FE_STV0910_VTH_Lookup);
			break;
		}

		if (lock == 0)
			ChipWaitOrAbort(hChip, 10);	/* wait 10ms */
		timer += 10;

	}

#ifdef HOST_PC
#ifndef NO_GUI
	lockTime = Timer() - lockTime;
	symbolRate = (ChipGetFieldImage(hChip, symbFreq3) << 8) +
	    (ChipGetFieldImage(hChip, symbFreq2));
	Fmt(message, "Demod Lock Time = %f ms ", lockTime * 1000.0);

	if (lock)
		ReportInsertMessage("DEMOD LOCK OK");
	else
		ReportInsertMessage("DEMOD LOCK FAIL");

	ReportInsertMessage(message);
#endif
#endif
	return lock;
}

/*! \fn BOOL FE_STV0910_GetFECLock(STCHIP_Handle_t hChip,
					FE_STV0910_DEMOD_t Demod, S32 TimeOut)
  \brief This function return s the fec lock status
  */
static BOOL FE_STV0910_GetFECLock(STCHIP_Handle_t hChip,
				  FE_STV0910_DEMOD_t Demod, S32 TimeOut)
{
	S32 timer = 0, lock = 0, headerField, pktdelinField, lockVitField;

	FE_STV0910_SEARCHSTATE_t demodState;

#ifdef HOST_PC
#ifndef NO_GUI
	double lockTime;
	char message[100];
	lockTime = Timer();

#endif
#endif

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		/* state machine search status field */
		headerField = FSTV0910_P1_HEADER_MODE;
		/* packet delin(DVBS 2) lock status field */
		pktdelinField = FSTV0910_P1_PKTDELIN_LOCK;
		/* Viterbi(DVBS1/DSS) lock status field */
		lockVitField = FSTV0910_P1_LOCKEDVIT;
		break;

	case FE_SAT_DEMOD_2:
		/* state machine search status field */
		headerField = FSTV0910_P2_HEADER_MODE;
		/* packet delin(DVBS 2) lock status field */
		pktdelinField = FSTV0910_P2_PKTDELIN_LOCK;
		/* Viterbi(DVBS1/DSS) lock status field */
		lockVitField = FSTV0910_P2_LOCKEDVIT;
		break;
	}

	demodState = ChipGetField(hChip, headerField);
	while ((timer < TimeOut) && (lock == 0)) {

		switch (demodState) {
		case FE_SAT_SEARCH:
		case FE_SAT_PLH_DETECTED:	/* no signal */
		default:
			lock = 0;
			break;

		case FE_SAT_DVBS2_FOUND:	/*found a DVBS2 signal */
			lock = ChipGetField(hChip, pktdelinField);
			break;

		case FE_SAT_DVBS_FOUND:
			lock = ChipGetField(hChip, lockVitField);
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

	if (lock)
		ReportInsertMessage("FEC LOCK OK");
	else
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
  ::	Demod	->	current demod 1 or 2
  ::	TimeOut	->	Time out in ms
  --PARAMS OUT::	NONE
  --RETURN	::	Lock status true or false
  --***************************************************/
/*! \fn BOOL FE_STV0910_WaitForLock(STCHIP_Handle_t hChip,
		FE_STV0910_DEMOD_t Demod, S32 DemodTimeOut, S32 FecTimeOut,
							U32 Satellite_Scan)
  \brief This function waits until the demod lock & fec lock or time out expired
  */
BOOL FE_STV0910_WaitForLock(STCHIP_Handle_t hChip, FE_STV0910_DEMOD_t Demod,
		    S32 DemodTimeOut, S32 FecTimeOut, U32 Satellite_Scan)
{

	S32 lock = 0,
	    timer = 0,
	    strMergerLockField;

#ifdef HOST_PC
#ifndef NO_GUI
	double lockTime;
	char message[100];
	lockTime = Timer();
#endif
#endif

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		/* Stream Merger lock status field) */
		strMergerLockField = FSTV0910_P1_TSFIFO_LINEOK;
		break;

	case FE_SAT_DEMOD_2:
		/* Stream Merger lock status field) */
		strMergerLockField = FSTV0910_P2_TSFIFO_LINEOK;
		break;
	}

	lock = FE_STV0910_GetDemodLock(hChip, Demod, DemodTimeOut);
	if (lock)
		lock = lock && FE_STV0910_GetFECLock(hChip, Demod, FecTimeOut);

	if (Satellite_Scan == FALSE) {
		/* LINEOK check is not performed during Satellite Scan */
		if (lock) {
			lock = 0;
			while ((timer < FecTimeOut) && (lock == 0)) {
				/*Check the stream merger Lock (good packet at
				 * the output)*/
				lock = ChipGetField(hChip, strMergerLockField);
				ChipWaitOrAbort(hChip, 1);
				timer++;
			}
		}
	}

#ifdef HOST_PC
#ifndef NO_GUI
	/*lockTime = Timer() - lockTime;
	   Fmt(message, "Total Lock Time = %f ms", lockTime*1000.0); */
	if (lock)
		ReportInsertMessage("LOCK OK");
	else
		ReportInsertMessage("LOCK FAIL");

	/* ReportInsertMessage(message); */
#endif
#endif
	if (lock)
		return TRUE;
	else
		return FALSE;

}

/*! \fn void FE_STV0910_SetViterbiStandard(STCHIP_Handle_t hChip,
  FE_STV0910_SearchStandard_t Standard,
  FE_STV0910_Rate_t PunctureRate,
  FE_STV0910_DEMOD_t Demod)
  \brief This function ...
  */
static void FE_STV0910_SetViterbiStandard(STCHIP_Handle_t hChip,
					  FE_STV0910_SearchStandard_t Standard,
					  FE_STV0910_Rate_t PunctureRate,
					  FE_STV0910_DEMOD_t Demod)
{

	S32 fecmReg, prvitReg;
	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		fecmReg = RSTV0910_P1_FECM;
		prvitReg = RSTV0910_P1_PRVIT;
		break;

	case FE_SAT_DEMOD_2:
		fecmReg = RSTV0910_P2_FECM;
		prvitReg = RSTV0910_P2_PRVIT;
		break;
	}
	switch (Standard) {
	case FE_SAT_AUTO_SEARCH:
		/* Disable DSS  in auto mode search for DVBS1 and DVBS2 only,
		 * DSS search is on demande */
		ChipSetOneRegister(hChip, fecmReg, 0x00);
		/* Enable All PR exept 6/7 */
		ChipSetOneRegister(hChip, prvitReg, 0x2F);
		break;

	case FE_SAT_SEARCH_DVBS1:
		/* Disable DSS */
		ChipSetOneRegister(hChip, fecmReg, 0x00);
		switch (PunctureRate) {
		case FE_SAT_PR_UNKNOWN:
		default:
			/* Enable All PR exept 6/7 */
			ChipSetOneRegister(hChip, prvitReg, 0x2F);
			break;

		case FE_SAT_PR_1_2:
			/* Enable 1/2 PR only */
			ChipSetOneRegister(hChip, prvitReg, 0x01);
			break;

		case FE_SAT_PR_2_3:
			/* Enable 2/3 PR only */
			ChipSetOneRegister(hChip, prvitReg, 0x02);
			break;

		case FE_SAT_PR_3_4:
			/* Enable 3/4 PR only */
			ChipSetOneRegister(hChip, prvitReg, 0x04);
			break;

		case FE_SAT_PR_5_6:
			/* Enable 5/6 PR only */
			ChipSetOneRegister(hChip, prvitReg, 0x08);
			break;

		case FE_SAT_PR_7_8:
			/* Enable 7/8 PR only */
			ChipSetOneRegister(hChip, prvitReg, 0x20);
			break;
		}

		break;

	case FE_SAT_SEARCH_DSS:
		/* Disable DVBS1 */
		ChipSetOneRegister(hChip, fecmReg, 0x80);
		switch (PunctureRate) {
		case FE_SAT_PR_UNKNOWN:
		default:
			/* Enable 1/2, 2/3 and 6/7 PR */
			ChipSetOneRegister(hChip, prvitReg, 0x13);
			break;

		case FE_SAT_PR_1_2:
			/* Enable 1/2 PR only */
			ChipSetOneRegister(hChip, prvitReg, 0x01);
			break;

		case FE_SAT_PR_2_3:
			/* Enable 2/3 PR only */
			ChipSetOneRegister(hChip, prvitReg, 0x02);
			break;

		case FE_SAT_PR_6_7:
			/* Enable All 7/8 PR only */
			ChipSetOneRegister(hChip, prvitReg, 0x10);
			break;
		}

		break;

	default:
		break;
	}
}

/*! \fn S32 FE_STV0910_CheckVGLNAPresent(STCHIP_Handle_t hChipVGLNA)
  \brief This function chechs the presence of a VGLNA on the board
  */
static S32 FE_STV0910_CheckVGLNAPresent(STCHIP_Handle_t hChipVGLNA)
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

S32 FE_STV0910_GetPADC(STCHIP_Handle_t hChip, FE_STV0910_LOOKUP_t *lookup,
		       FE_STV0910_DEMOD_t Demod)
{

	S32 Padc = 100, regval, Imin, Imax, i;

	if ((lookup == NULL) || !lookup->size)
		return Padc;
	regval = 0;
	/* ChipWaitOrAbort(hChip, 5); */

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		for (i = 0; i < 5; i++) {
			ChipGetRegisters(hChip, RSTV0910_P1_POWERI, 2);
			regval +=
			    ChipGetFieldImage(hChip, FSTV0910_P1_POWER_I) *
			    ChipGetFieldImage(hChip, FSTV0910_P1_POWER_I) +
			    ChipGetFieldImage(hChip, FSTV0910_P1_POWER_Q) *
			    ChipGetFieldImage(hChip, FSTV0910_P1_POWER_Q);
			ChipWaitOrAbort(hChip, 1);
		}

		break;

	case FE_SAT_DEMOD_2:
		for (i = 0; i < 5; i++) {
			ChipGetRegisters(hChip, RSTV0910_P2_POWERI, 2);
			regval +=
			    ChipGetFieldImage(hChip, FSTV0910_P2_POWER_I) *
			    ChipGetFieldImage(hChip, FSTV0910_P2_POWER_I) +
			    ChipGetFieldImage(hChip, FSTV0910_P2_POWER_Q) *
			    ChipGetFieldImage(hChip, FSTV0910_P2_POWER_Q);
			ChipWaitOrAbort(hChip, 1);
		}
		break;
	}

	regval /= 5;

	Imin = 0;
	Imax = lookup->size - 1;

	if (INRANGE(lookup->table[Imin].regval, regval,
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

	return Padc;

}

/*! \fn  S32 FE_STV0910_GetRFLevel(FE_STV0910_InternalParams_t *pParams,
						FE_STV0910_DEMOD_t Demod)
  \brief This function return s the RF level of signal
  */
S32 FE_STV0910_GetRFLevel(FE_STV0910_InternalParams_t *pParams,
			  FE_STV0910_DEMOD_t Demod)
{
	S32 Power = 0, vglnagain = 0, BBgain = 0, agcGain, Padc;

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		ChipGetRegisters(pParams->hDemod, RSTV0910_P1_AGCIQIN1, 2);
		agcGain =
		    MAKEWORD(ChipGetFieldImage
			     (pParams->hDemod, FSTV0910_P1_AGCIQ_VALUE1),
			     ChipGetFieldImage(pParams->hDemod,
					       FSTV0910_P1_AGCIQ_VALUE0));
		BBgain = FE_Sat_TunerGetGain(pParams->hTuner1);
		/* Power = RFGain */
		Power = FE_Sat_TunerGetRFGain(pParams->hTuner1, agcGain,
					      BBgain);

		/* Pin(dBm) = Padc(dBm) - RFGain(dB) */
		/* Padc(dBm) = 10*log10((Vadc^2/75ohm)/10^-3) */
		/* Vadc(dBm) = (VppADC/256)*sqrt(PowerI^2+powerQ^2)/6 */

		Padc =
		    FE_STV0910_GetPADC(pParams->hDemod, &FE_STV0910_PADC_Lookup,
				       Demod);
		/* STV0910 ADC is 1.5Vpp => add 3.52dBm=20*log10(1.5) to power
		 * ADC (based on 1Vpp ADC range) */
		Padc += 352;
		Power = Padc - Power;

		Power /= 100;

		if (FE_STV0910_CheckVGLNAPresent(pParams->hVGLNA1) != 0) {

			if (STVVGLNA_GetGain(pParams->hVGLNA1, &vglnagain) ==
			    CHIPERR_NO_ERROR)
				Power += vglnagain - 15;

		}
		break;

	case FE_SAT_DEMOD_2:
		ChipGetRegisters(pParams->hDemod, RSTV0910_P2_AGCIQIN1, 2);
		agcGain =
		    MAKEWORD(ChipGetFieldImage
			     (pParams->hDemod, FSTV0910_P2_AGCIQ_VALUE1),
			     ChipGetFieldImage(pParams->hDemod,
					       FSTV0910_P2_AGCIQ_VALUE0));
		BBgain = FE_Sat_TunerGetGain(pParams->hTuner2);
		/* Power = RFGain */
		Power =
		    FE_Sat_TunerGetRFGain(pParams->hTuner2, agcGain, BBgain);

		/* Pin(dBm) = Padc(dBm) - RFGain(dB) */
		/* Padc(dBm) = 10*log10((Vadc^2/75ohm)/10^-3) */
		/* Vadc(dBm) = (VppADC/256)*sqrt((PowerI^2+powerQ^2)/6) */

		Padc =
		    FE_STV0910_GetPADC(pParams->hDemod, &FE_STV0910_PADC_Lookup,
				       Demod);
		/* STV0910 ADC is 1.5Vpp => add 3.52dBm=20*log10(1.5) to power
		 * ADC (based on 1Vpp ADC range) */
		Padc += 352;
		Power = Padc - Power;
		Power /= 100;

		if (FE_STV0910_CheckVGLNAPresent(pParams->hVGLNA2) != 0) {

			if (STVVGLNA_GetGain(pParams->hVGLNA2, &vglnagain) ==
			    CHIPERR_NO_ERROR)
				Power += vglnagain - 15;

		}
		break;
	}

	return Power;
}

/*****************************************************
  --FUNCTION	::	SetSearchStandard
  --ACTION	::	Set Search standard(Auto, DVBS2 only or DVBS1/DSS only)
  --PARAMS IN	::	hChip	->	handle to the chip
  ::	Standard	->	Search standard
  ::	Demod	->	current demod 1 or 2
  --PARAMS OUT::	NONE
  --RETURN	::	none
  --***************************************************/
/*! \fn FE_STV0910_SetSearchStandard(FE_STV0910_InternalParams_t *pParams,
						FE_STV0910_DEMOD_t Demod)
  \brief This function sets the search standard to Auto search or DVBS2 only or
  DVBS and DSS only
  */
static void FE_STV0910_SetSearchStandard(FE_STV0910_InternalParams_t *pParams,
					 FE_STV0910_DEMOD_t Demod)
{
	switch (Demod) {
	case FE_SAT_DEMOD_1:

	default:
		/*In case of Path 1 */
		switch (pParams->Demod1SearchStandard) {
		case FE_SAT_SEARCH_DVBS1:
			ChipSetFieldImage(pParams->hDemod,
					FSTV0910_P1_DVBS1_ENABLE, 1);
			ChipSetFieldImage(pParams->hDemod,
					FSTV0910_P1_DVBS2_ENABLE, 0);
			ChipSetRegisters(pParams->hDemod, RSTV0910_P1_DMDCFGMD,
					 1);
			ChipSetField(pParams->hDemod, FSTV0910_TSTRS_DISRS1, 0);

			FE_STV0910_SetViterbiStandard(pParams->hDemod,
				      pParams->Demod1SearchStandard,
				      pParams->Demod1PunctureRate, Demod);

			/* Enable Super FEC */
			ChipSetOneRegister(pParams->hDemod,
					   RSTV0910_P1_SFDLYSET2, 0);

			break;
		case FE_SAT_SEARCH_DSS:
			/*If DVBS1/DSS only disable DVBS2 search */
			ChipSetFieldImage(pParams->hDemod,
					  FSTV0910_P1_DVBS1_ENABLE, 1);
			ChipSetFieldImage(pParams->hDemod,
					  FSTV0910_P1_DVBS2_ENABLE, 0);
			ChipSetRegisters(pParams->hDemod, RSTV0910_P1_DMDCFGMD,
					 1);
			ChipSetField(pParams->hDemod, FSTV0910_TSTRS_DISRS1, 0);

			FE_STV0910_SetViterbiStandard(pParams->hDemod,
				      pParams->Demod1SearchStandard,
				      pParams->Demod1PunctureRate, Demod);

			/* Stop Super FEC */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0910_P1_SFDLYSET2, 2);

			break;

		case FE_SAT_SEARCH_DVBS2:
			/*If DVBS2 only activate the DVBS2 search and stop the
			 * VITERBI */
			ChipSetFieldImage(pParams->hDemod,
					  FSTV0910_P1_DVBS1_ENABLE, 0);
			ChipSetFieldImage(pParams->hDemod,
					  FSTV0910_P1_DVBS2_ENABLE, 1);
			ChipSetRegisters(pParams->hDemod, RSTV0910_P1_DMDCFGMD,
					 1);
			ChipSetField(pParams->hDemod, FSTV0910_TSTRS_DISRS1, 1);

			break;

		case FE_SAT_AUTO_SEARCH:
		default:
			/*If automatic enable both DVBS1/DSS and DVBS2 search */
			ChipSetFieldImage(pParams->hDemod,
					  FSTV0910_P1_DVBS1_ENABLE, 1);
			ChipSetFieldImage(pParams->hDemod,
					  FSTV0910_P1_DVBS2_ENABLE, 1);
			ChipSetRegisters(pParams->hDemod, RSTV0910_P1_DMDCFGMD,
					 1);
			ChipSetField(pParams->hDemod, FSTV0910_TSTRS_DISRS1, 0);

			FE_STV0910_SetViterbiStandard(pParams->hDemod,
					     pParams->Demod1SearchStandard,
					     pParams->Demod1PunctureRate,
					     Demod);

			/* Enable Super FEC */
			ChipSetOneRegister(pParams->hDemod,
					   RSTV0910_P1_SFDLYSET2, 0);

			break;
		}
		break;

	case FE_SAT_DEMOD_2:
		/*In case of Path 2 */
		switch (pParams->Demod2SearchStandard) {
		case FE_SAT_SEARCH_DVBS1:
			ChipSetFieldImage(pParams->hDemod,
					  FSTV0910_P2_DVBS1_ENABLE, 1);
			ChipSetFieldImage(pParams->hDemod,
					  FSTV0910_P2_DVBS2_ENABLE, 0);
			ChipSetRegisters(pParams->hDemod, RSTV0910_P2_DMDCFGMD,
					 1);
			ChipSetField(pParams->hDemod, FSTV0910_TSTRS_DISRS2, 0);

			FE_STV0910_SetViterbiStandard(pParams->hDemod,
					      pParams->Demod2SearchStandard,
					      pParams->Demod2PunctureRate,
					      Demod);
			/* Enable Super FEC */
			ChipSetOneRegister(pParams->hDemod,
					   RSTV0910_P2_SFDLYSET2, 0);

			break;
		case FE_SAT_SEARCH_DSS:
			/*If DVBS1/DSS only disable DVBS2 search */
			ChipSetFieldImage(pParams->hDemod,
					  FSTV0910_P2_DVBS1_ENABLE, 1);
			ChipSetFieldImage(pParams->hDemod,
					  FSTV0910_P2_DVBS2_ENABLE, 0);
			ChipSetRegisters(pParams->hDemod, RSTV0910_P2_DMDCFGMD,
					 1);
			ChipSetField(pParams->hDemod, FSTV0910_TSTRS_DISRS2, 0);

			FE_STV0910_SetViterbiStandard(pParams->hDemod,
					      pParams->Demod2SearchStandard,
					      pParams->Demod2PunctureRate,
					      Demod);
			/* Stop Super FEC */
			ChipSetOneRegister(pParams->hDemod,
					   RSTV0910_P2_SFDLYSET2, 2);

			break;

		case FE_SAT_SEARCH_DVBS2:
			/*If DVBS2 only activate the DVBS2 & stop the VITERBI */
			ChipSetFieldImage(pParams->hDemod,
					  FSTV0910_P2_DVBS1_ENABLE, 0);
			ChipSetFieldImage(pParams->hDemod,
					  FSTV0910_P2_DVBS2_ENABLE, 1);
			ChipSetRegisters(pParams->hDemod, RSTV0910_P2_DMDCFGMD,
					 1);
			ChipSetField(pParams->hDemod, FSTV0910_TSTRS_DISRS2, 1);

			break;

		case FE_SAT_AUTO_SEARCH:
		default:
			/*If automatic enable both DVBS1/DSS and DVBS2 search */
			ChipSetFieldImage(pParams->hDemod,
					  FSTV0910_P2_DVBS1_ENABLE, 1);
			ChipSetFieldImage(pParams->hDemod,
					  FSTV0910_P2_DVBS2_ENABLE, 1);
			ChipSetRegisters(pParams->hDemod, RSTV0910_P2_DMDCFGMD,
					 1);
			ChipSetField(pParams->hDemod, FSTV0910_TSTRS_DISRS2, 0);

			FE_STV0910_SetViterbiStandard(pParams->hDemod,
					      pParams->Demod2SearchStandard,
					      pParams->Demod2PunctureRate,
					      Demod);

			/* Enable Super FEC */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0910_P2_SFDLYSET2, 0);

			break;
		}
		break;
	}

}

/*****************************************************
  --FUNCTION	::	StartSearch
  --ACTION	::	Trig the Demod to start a new search
  --PARAMS IN	::	pParams	->Pointer FE_STV0910_InternalParams_t structer
  ::	Demod	->	current demod 1 or 2
  --PARAMS OUT::	NONE
  --RETURN	::	none
  --***************************************************/
/*! \fn void FE_STV0910_StartSearch(FE_STV0910_InternalParams_t *pParams,
						FE_STV0910_DEMOD_t Demod)
  \brief This function programs the PEA to lauch a hardware search algo.
  */
static void FE_STV0910_StartSearch(FE_STV0910_InternalParams_t *pParams,
				   FE_STV0910_DEMOD_t Demod)
{

	U32 freq;
	S16 freq_s16;

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		/* In case of Path 1 */
		/* Reset CAR3 */
		ChipSetField(pParams->hDemod, FSTV0910_FRESSYM1, 1);
		ChipSetField(pParams->hDemod, FSTV0910_FRESSYM1, 0);

		/*Reset the Demod */
		ChipSetOneRegister(pParams->hDemod, RSTV0910_P1_DMDISTATE,
				   0x1F);

		if (pParams->Demod1SearchAlgo == FE_SAT_WARM_START) {
			/* if warm start CFR min = -1MHz, CFR max = 1MHz */
			freq = 1000 << 16;
			freq /= (pParams->MasterClock / 1000);
			freq_s16 = (S16) freq;
		} else if (pParams->Demod1SearchAlgo == FE_SAT_COLD_START) {
			/* CFR min = - (SearchRange/2 + margin)
			   CFR max = +(SearchRange/2 + margin)
			   (80KHz margin if SR <= 5Msps else margin = 600KHz) */

			if (pParams->Demod1SymbolRate <= 5000000)
				freq = (pParams->Demod1SearchRange / 2000) + 80;
			else
				/*600 *//*Increase search range to improve
				   cold start success rate on tilted signal +
				   frequency offset */
				freq = (pParams->Demod1SearchRange / 2000) +
				    1600;
			freq = freq << 16;
			freq /= (pParams->MasterClock / 1000);
			freq_s16 = (S16) freq;
		} else {
			freq = (pParams->Demod1SearchRange / 2000);
			freq = freq << 16;
			freq /= (pParams->MasterClock / 1000);
			freq_s16 = (S16) freq;
		}

		/*Super FEC limitation  WA, applicable only for cut1 */
		if ((pParams->hDemod->ChipID == 0x50) &&
			(pParams->Demod1SearchStandard == FE_SAT_SEARCH_DVBS1
			|| pParams->Demod1SearchStandard == FE_SAT_AUTO_SEARCH)
			&& (!FE_SAT_BLIND_SEARCH)) {
			if (pParams->Demod1SymbolRate >= 53500000)
				/* Stop superFEC */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0910_P1_SFDLYSET2, 0x02);
		}

		/* search range definition */
		ChipSetOneRegister(pParams->hDemod, RSTV0910_P1_CARCFG, 0x46);

		ChipSetFieldImage(pParams->hDemod, FSTV0910_P1_CFR_UP1,
				  MSB(freq_s16));
		ChipSetFieldImage(pParams->hDemod, FSTV0910_P1_CFR_UP0,
				  LSB(freq_s16));
		ChipSetRegisters(pParams->hDemod, RSTV0910_P1_CFRUP1, 2);
		freq_s16 *= (-1);
		ChipSetFieldImage(pParams->hDemod, FSTV0910_P1_CFR_LOW1,
				  MSB(freq_s16));
		ChipSetFieldImage(pParams->hDemod, FSTV0910_P1_CFR_LOW0,
				  LSB(freq_s16));
		ChipSetRegisters(pParams->hDemod, RSTV0910_P1_CFRLOW1, 2);

		ChipSetFieldImage(pParams->hDemod, FSTV0910_P1_CFR_INIT1, 0);
		ChipSetFieldImage(pParams->hDemod, FSTV0910_P1_CFR_INIT0, 0);
		/*init the demod frequency offset to 0 */
		ChipSetRegisters(pParams->hDemod, RSTV0910_P1_CFRINIT1, 2);

		switch (pParams->Demod1SearchAlgo) {
		/*The symbol rate and the exact carrier Frequency are known */
		case FE_SAT_WARM_START:
			/*Reset the Demod */
			ChipSetOneRegister(pParams->hDemod,
					   RSTV0910_P1_DMDISTATE, 0x1F);
			/*Trig an acquisition(start the search) */
			ChipSetOneRegister(pParams->hDemod,
					   RSTV0910_P1_DMDISTATE, 0x18);
			break;

		case FE_SAT_COLD_START:	/*The symbol rate is known */
			/*Reset the Demod */
			ChipSetOneRegister(pParams->hDemod,
					   RSTV0910_P1_DMDISTATE, 0x1F);
			/*Trig an acquisition(start the search) */
			ChipSetOneRegister(pParams->hDemod,
					   RSTV0910_P1_DMDISTATE, 0x15);
			break;

		case FE_SAT_BLIND_SEARCH:
			/*Reset the Demod */
			ChipSetOneRegister(pParams->hDemod,
					   RSTV0910_P1_DMDISTATE, 0x1F);
			/*Trig an acquisition(start the search) */
			ChipSetOneRegister(pParams->hDemod,
					   RSTV0910_P1_DMDISTATE, 0x00);
			break;

		default:
			/*Nothing to do in case of blind search, blind  is
			 * handled by "FE_STV0910_BlindSearchAlgo" function */
			break;
		}
		break;

	case FE_SAT_DEMOD_2:
		/* In case of Path 2 */
		/*Reset CAR3 */
		ChipSetField(pParams->hDemod, FSTV0910_FRESSYM2, 1);
		ChipSetField(pParams->hDemod, FSTV0910_FRESSYM2, 0);

		/*Reset the Demod */
		ChipSetOneRegister(pParams->hDemod, RSTV0910_P2_DMDISTATE,
				   0x1F);

		if (pParams->Demod2SearchAlgo == FE_SAT_WARM_START) {
			/* if warm start CFR min = -1MHz, CFR max = 1MHz */
			freq = 1000 << 16;
			freq /= (pParams->MasterClock / 1000);
			freq_s16 = (S16) freq;
		} else if (pParams->Demod2SearchAlgo == FE_SAT_COLD_START) {
			/* CFR min = - (SearchRange/2 + margin)
			   CFR max = +(SearchRange/2 + margin)
			   (80KHz margin if SR <= 5Msps else margin = 600KHz) */

			if (pParams->Demod2SymbolRate <= 5000000)
				freq = (pParams->Demod2SearchRange / 2000) + 80;
			else
				/*600 *//* Increase search range to improve
				   cold start success rate on tilted signal
				   + frequency offset */
				freq =
				    (pParams->Demod2SearchRange / 2000) + 1600;
			freq = freq << 16;
			freq /= (pParams->MasterClock / 1000);
			freq_s16 = (S16) freq;
		} else {
			freq = (pParams->Demod2SearchRange / 2000);
			freq = freq << 16;
			freq /= (pParams->MasterClock / 1000);
			freq_s16 = (S16) freq;
		}

		/*Super FEC limitation  WA, applicable only for cut1 */
		if ((pParams->hDemod->ChipID == 0x50) &&
			(pParams->Demod2SearchStandard == FE_SAT_SEARCH_DVBS1
			 || pParams->Demod2SearchStandard == FE_SAT_AUTO_SEARCH)
				&& (!FE_SAT_BLIND_SEARCH)) {
			if (pParams->Demod2SymbolRate >= 53500000)
				/* Stop SuperFEC */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0910_P2_SFDLYSET2, 0x02);
		}

		/* search range definition */
		ChipSetOneRegister(pParams->hDemod, RSTV0910_P2_CARCFG, 0x46);

		ChipSetFieldImage(pParams->hDemod, FSTV0910_P2_CFR_UP1,
				  MSB(freq_s16));
		ChipSetFieldImage(pParams->hDemod, FSTV0910_P2_CFR_UP0,
				  LSB(freq_s16));
		ChipSetRegisters(pParams->hDemod, RSTV0910_P2_CFRUP1, 2);
		freq_s16 *= (-1);
		ChipSetFieldImage(pParams->hDemod, FSTV0910_P2_CFR_LOW1,
				  MSB(freq_s16));
		ChipSetFieldImage(pParams->hDemod, FSTV0910_P2_CFR_LOW0,
				  LSB(freq_s16));
		ChipSetRegisters(pParams->hDemod, RSTV0910_P2_CFRLOW1, 2);

		ChipSetFieldImage(pParams->hDemod, FSTV0910_P2_CFR_INIT1, 0);
		ChipSetFieldImage(pParams->hDemod, FSTV0910_P2_CFR_INIT0, 0);
		/*init the demod frequency offset to 0 */
		ChipSetRegisters(pParams->hDemod, RSTV0910_P2_CFRINIT1, 2);

		switch (pParams->Demod2SearchAlgo) {
		/*The symbol rate and the exact carrier Frequency are known */
		case FE_SAT_WARM_START:
			/*Reset the Demod */
			ChipSetOneRegister(pParams->hDemod,
					   RSTV0910_P2_DMDISTATE, 0x1F);
			/*Trig an acquisition(start the search) */
			ChipSetOneRegister(pParams->hDemod,
					   RSTV0910_P2_DMDISTATE, 0x18);
			break;

		case FE_SAT_COLD_START:	/*The symbol rate is known */
			/*Reset the Demod */
			ChipSetOneRegister(pParams->hDemod,
					   RSTV0910_P2_DMDISTATE, 0x1F);
			/*Trig an acquisition(start the search) */
			ChipSetOneRegister(pParams->hDemod,
					   RSTV0910_P2_DMDISTATE, 0x15);
			break;

		case FE_SAT_BLIND_SEARCH:
			/*Reset the Demod */
			ChipSetOneRegister(pParams->hDemod,
					   RSTV0910_P2_DMDISTATE, 0x1F);
			/*Trig an acquisition(start the search) */
			ChipSetOneRegister(pParams->hDemod,
					   RSTV0910_P2_DMDISTATE, 0x00);
			break;

		default:
			/*Nothing to do in case of blind search, blind  is
			 * handled by "FE_STV0910_BlindSearchAlgo" function */
			break;
		}
		break;
	}
}

/*! \fn U8 FE_STV0910_GetOptimCarrierLoop(S32 SymbolRate,
				FE_STV0910_ModCod_t ModCode, S32 Pilots)
  \brief This function sets the optimized alpha coefficient value for tracking.
  */
U8 FE_STV0910_GetOptimCarrierLoop(S32 SymbolRate, FE_STV0910_ModCod_t ModCode,
				  S32 Pilots)
{
	U8 aclcValue = 0x29;
	U32 i = 0;

	/* Find the index parameters for the Modulation */
	while ((i < NB_SAT_MODCOD)
	       && (ModCode != FE_STV0910_S2CarLoop[i].ModCode))
		i++;

	if (i < NB_SAT_MODCOD) {
		if (Pilots) {
			if (SymbolRate <= 3000000)
				aclcValue =
				    FE_STV0910_S2CarLoop[i].CarLoopPilotsOn_2;
			else if (SymbolRate <= 7000000)
				aclcValue =
				    FE_STV0910_S2CarLoop[i].CarLoopPilotsOn_5;
			else if (SymbolRate <= 15000000)
				aclcValue =
				    FE_STV0910_S2CarLoop[i].CarLoopPilotsOn_10;
			else if (SymbolRate <= 25000000)
				aclcValue =
				    FE_STV0910_S2CarLoop[i].CarLoopPilotsOn_20;
			else
				aclcValue =
				    FE_STV0910_S2CarLoop[i].CarLoopPilotsOn_30;
		} else {
			if (SymbolRate <= 3000000)
				aclcValue =
				    FE_STV0910_S2CarLoop[i].CarLoopPilotsOff_2;
			else if (SymbolRate <= 7000000)
				aclcValue =
				    FE_STV0910_S2CarLoop[i].CarLoopPilotsOff_5;
			else if (SymbolRate <= 15000000)
				aclcValue =
				    FE_STV0910_S2CarLoop[i].CarLoopPilotsOff_10;
			else if (SymbolRate <= 25000000)
				aclcValue =
				    FE_STV0910_S2CarLoop[i].CarLoopPilotsOff_20;
			else
				aclcValue =
				    FE_STV0910_S2CarLoop[i].CarLoopPilotsOff_30;
		}
	} else {
		/* Modulation Unknown */
	}
	return aclcValue;
}

/*****************************************************
  --FUNCTION	::	TrackingOptimization
  --ACTION	::	Set Optimized parameters for tracking
  --PARAMS IN	::	pParams	->Pointer FE_STV0910_InternalParams_t structer
  ::	Demod	->	current demod 1 or 2
  --PARAMS OUT::	NONE
  --RETURN	::	none
  --***************************************************/
/*! \fn void FE_STV0910_TrackingOptimization(FE_STV0910_InternalParams_t
					*pParams, FE_STV0910_DEMOD_t Demod)
  \brief This function is used to set the optimized parameters for tracking.
  */
static void FE_STV0910_TrackingOptimization(FE_STV0910_InternalParams_t *
					    pParams, FE_STV0910_DEMOD_t Demod)
{
	S32 symbolRate, pilots, aclc;

	FE_STV0910_ModCod_t foundModcod;

	/* Read the Symbol rate    */
	symbolRate = FE_STV0910_GetSymbolRate(pParams->hDemod,
					      pParams->MasterClock, Demod);
	symbolRate +=
	    FE_STV0910_TimingGetOffset(pParams->hDemod, symbolRate, Demod);

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		switch (pParams->Demod1Results.Standard) {
		case FE_SAT_DVBS1_STANDARD:
			if (pParams->Demod1SearchStandard ==
							FE_SAT_AUTO_SEARCH) {
				ChipSetFieldImage(pParams->hDemod,
						  FSTV0910_P1_DVBS1_ENABLE, 1);
				ChipSetFieldImage(pParams->hDemod,
						  FSTV0910_P1_DVBS2_ENABLE, 0);
				ChipSetRegisters(pParams->hDemod,
						 RSTV0910_P1_DMDCFGMD, 1);
			}
			/*Set the rolloff to the manual value(given at the
			 * initialization) */
			ChipSetFieldImage(pParams->hDemod,
					  FSTV0910_P1_ROLLOFF_CONTROL,
					  pParams->RollOff);
			ChipSetFieldImage(pParams->hDemod,
					  FSTV0910_P1_MANUALSX_ROLLOFF, 1);
			ChipSetRegisters(pParams->hDemod, RSTV0910_P1_DEMOD, 1);

			/* force to viterbi bit error */
			ChipSetOneRegister(pParams->hDemod,
					   RSTV0910_P1_ERRCTRL1, 0x75);

			break;

		case FE_SAT_DSS_STANDARD:
			if (pParams->Demod1SearchStandard ==
							FE_SAT_AUTO_SEARCH) {
				ChipSetFieldImage(pParams->hDemod,
						  FSTV0910_P1_DVBS1_ENABLE, 1);
				ChipSetFieldImage(pParams->hDemod,
						  FSTV0910_P1_DVBS2_ENABLE, 0);
				ChipSetRegisters(pParams->hDemod,
						 RSTV0910_P1_DMDCFGMD, 1);
			}
			/*Set the rolloff to the manual value(given at the
			 * initialization) */
			ChipSetFieldImage(pParams->hDemod,
					  FSTV0910_P1_ROLLOFF_CONTROL,
					  pParams->RollOff);
			ChipSetFieldImage(pParams->hDemod,
					  FSTV0910_P1_MANUALSX_ROLLOFF, 1);
			ChipSetRegisters(pParams->hDemod, RSTV0910_P1_DEMOD, 1);

			/* force to viterbi bit error */
			ChipSetOneRegister(pParams->hDemod,
					   RSTV0910_P1_ERRCTRL1, 0x75);
			break;

		case FE_SAT_DVBS2_STANDARD:

			ChipSetFieldImage(pParams->hDemod,
					  FSTV0910_P1_DVBS1_ENABLE, 0);
			ChipSetFieldImage(pParams->hDemod,
					  FSTV0910_P1_DVBS2_ENABLE, 1);
			ChipSetRegisters(pParams->hDemod, RSTV0910_P1_DMDCFGMD,
					 1);
			/*Disable Reed-Solomon */
			ChipSetField(pParams->hDemod, FSTV0910_TSTRS_DISRS2, 1);

			/* force to DVBS2 PER  */
			ChipSetOneRegister(pParams->hDemod,
					   RSTV0910_P1_ERRCTRL1, 0x67);

			if (pParams->Demod1Results.FrameLength !=
					FE_SAT_LONG_FRAME)
				break;

			/*Carrier loop setting for lon frame */
			foundModcod = ChipGetField(pParams->hDemod,
					    FSTV0910_P1_DEMOD_MODCOD);
			pilots = ChipGetFieldImage(pParams->hDemod,
				      FSTV0910_P1_DEMOD_TYPE) & 0x01;

			aclc = FE_STV0910_GetOptimCarrierLoop(symbolRate,
					   foundModcod, pilots);
			if (foundModcod <= FE_SAT_QPSK_910)
				ChipSetOneRegister(pParams->hDemod,
					   RSTV0910_P1_ACLC2S2Q, aclc);
			else if (foundModcod <= FE_SAT_8PSK_910) {
				ChipSetOneRegister(pParams->hDemod,
					   RSTV0910_P1_ACLC2S2Q, 0x2a);
				ChipSetOneRegister(pParams->hDemod,
					   RSTV0910_P1_ACLC2S28, aclc);
			} else {
				if (foundModcod <= FE_SAT_16APSK_910) {
					ChipSetOneRegister(pParams->hDemod,
						RSTV0910_P1_ACLC2S2Q, 0x2a);
					ChipSetOneRegister(pParams->hDemod,
						RSTV0910_P1_ACLC2S216A, aclc);
				} else if (foundModcod <= FE_SAT_32APSK_910) {
					ChipSetOneRegister(pParams->hDemod,
						RSTV0910_P1_ACLC2S2Q, 0x2a);
					ChipSetOneRegister(pParams->hDemod,
						RSTV0910_P1_ACLC2S232A, aclc);
				}
			}
			break;

		case FE_SAT_UNKNOWN_STANDARD:
		default:
			ChipSetFieldImage(pParams->hDemod,
					FSTV0910_P1_DVBS1_ENABLE, 1);
			ChipSetFieldImage(pParams->hDemod,
					FSTV0910_P1_DVBS2_ENABLE, 1);
			ChipSetRegisters(pParams->hDemod, RSTV0910_P1_DMDCFGMD,
					1);
			break;
		}

		break;

	case FE_SAT_DEMOD_2:

		switch (pParams->Demod2Results.Standard) {
		case FE_SAT_DVBS1_STANDARD:
			if (pParams->Demod2SearchStandard ==
							FE_SAT_AUTO_SEARCH) {
				ChipSetFieldImage(pParams->hDemod,
						  FSTV0910_P2_DVBS1_ENABLE, 1);
				ChipSetFieldImage(pParams->hDemod,
						  FSTV0910_P2_DVBS2_ENABLE, 0);
				ChipSetRegisters(pParams->hDemod,
						 RSTV0910_P2_DMDCFGMD, 1);
			}
			/*Set the rolloff to the manual value(given at the
			 * initialization) */
			ChipSetFieldImage(pParams->hDemod,
					  FSTV0910_P2_ROLLOFF_CONTROL,
					  pParams->RollOff);
			ChipSetFieldImage(pParams->hDemod,
					  FSTV0910_P2_MANUALSX_ROLLOFF, 1);
			ChipSetRegisters(pParams->hDemod, RSTV0910_P2_DEMOD, 1);

			/* force to viterbi bit error */
			ChipSetOneRegister(pParams->hDemod,
					   RSTV0910_P2_ERRCTRL1, 0x75);
			break;

		case FE_SAT_DSS_STANDARD:
			if (pParams->Demod2SearchStandard ==
							FE_SAT_AUTO_SEARCH) {
				ChipSetFieldImage(pParams->hDemod,
						  FSTV0910_P2_DVBS1_ENABLE, 1);
				ChipSetFieldImage(pParams->hDemod,
						  FSTV0910_P2_DVBS2_ENABLE, 0);
				ChipSetRegisters(pParams->hDemod,
						 RSTV0910_P2_DMDCFGMD, 1);
			}
			/*Set the rolloff to the manual value(given at the
			 * initialization) */
			ChipSetFieldImage(pParams->hDemod,
				 FSTV0910_P2_ROLLOFF_CONTROL, pParams->RollOff);
			ChipSetFieldImage(pParams->hDemod,
					  FSTV0910_P2_MANUALSX_ROLLOFF, 1);
			ChipSetRegisters(pParams->hDemod, RSTV0910_P2_DEMOD, 1);

			/* force to viterbi bit error */
			ChipSetOneRegister(pParams->hDemod,
					   RSTV0910_P2_ERRCTRL1, 0x75);
			break;

		case FE_SAT_DVBS2_STANDARD:

			ChipSetFieldImage(pParams->hDemod,
					  FSTV0910_P2_DVBS1_ENABLE, 0);
			ChipSetFieldImage(pParams->hDemod,
					  FSTV0910_P2_DVBS2_ENABLE, 1);
			ChipSetRegisters(pParams->hDemod, RSTV0910_P2_DMDCFGMD,
					 1);

			/* force to DVBS2 PER  */
			ChipSetOneRegister(pParams->hDemod,
					   RSTV0910_P2_ERRCTRL1, 0x67);

			if (pParams->Demod2Results.FrameLength !=
					FE_SAT_LONG_FRAME)
				break;

			/*Carrier loop setting for lon frame */
			foundModcod = ChipGetField(pParams->hDemod,
					 FSTV0910_P2_DEMOD_MODCOD);
			pilots = ChipGetFieldImage(pParams->hDemod,
					      FSTV0910_P2_DEMOD_TYPE) & 0x01;

			aclc = FE_STV0910_GetOptimCarrierLoop(symbolRate,
					foundModcod, pilots);
			if (foundModcod <= FE_SAT_QPSK_910)
				ChipSetOneRegister(pParams->hDemod,
						RSTV0910_P2_ACLC2S2Q, aclc);
			else if (foundModcod <= FE_SAT_8PSK_910) {
				ChipSetOneRegister(pParams->hDemod,
						RSTV0910_P2_ACLC2S2Q, 0x2a);
				ChipSetOneRegister(pParams->hDemod,
						RSTV0910_P2_ACLC2S28, aclc);
			} else {
				if (foundModcod <= FE_SAT_16APSK_910) {
					ChipSetOneRegister(pParams->hDemod,
						RSTV0910_P2_ACLC2S2Q, 0x2a);
					ChipSetOneRegister(pParams->hDemod,
						RSTV0910_P2_ACLC2S216A, aclc);
				} else if (foundModcod <= FE_SAT_32APSK_910) {
					ChipSetOneRegister(pParams->hDemod,
						RSTV0910_P2_ACLC2S2Q, 0x2a);
					ChipSetOneRegister(pParams->hDemod,
						RSTV0910_P2_ACLC2S232A, aclc);
				}
			}
			break;
		case FE_SAT_UNKNOWN_STANDARD:
		default:
			ChipSetFieldImage(pParams->hDemod,
					  FSTV0910_P2_DVBS1_ENABLE, 1);
			ChipSetFieldImage(pParams->hDemod,
					  FSTV0910_P2_DVBS2_ENABLE, 1);
			ChipSetRegisters(pParams->hDemod, RSTV0910_P2_DMDCFGMD,
					 1);
			break;
		}

		break;
	}
}

/*****************************************************
  --FUNCTION	::	GetSignalParams
  --ACTION	::	Read signal caracteristics
  --PARAMS IN	::	pParams	->Pointer FE_STV0910_InternalParams_t structer
  ::	Demod	->	current demod 1 or 2
  --PARAMS OUT::	NONE
  --RETURN	::	RANGE Ok or not
  --***************************************************/
/*! \fn FE_STV0910_SIGNALTYPE_t FE_STV0910_GetSignalParams(
		FE_STV0910_InternalParams_t *pParams, FE_STV0910_DEMOD_t Demod)
  \brief This function gets signal caracteristics
  */
static FE_STV0910_SIGNALTYPE_t
FE_STV0910_GetSignalParams(FE_STV0910_InternalParams_t *pParams,
			   FE_STV0910_DEMOD_t Demod, U32 Satellite_Scan)
{
	FE_STV0910_SIGNALTYPE_t range = FE_SAT_OUTOFRANGE;
	S32 offsetFreq, symbolRateOffset, i = 0;

	U8 timing;
	ChipWaitOrAbort(pParams->hDemod, 5);
	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		if (pParams->Demod1SearchAlgo == FE_SAT_BLIND_SEARCH) {
			/* if Blind search wait for symbol rate offset
			 * information transfert from the timing loop to the
			 * demodulator symbol rate
			 */
			timing = ChipGetOneRegister(pParams->hDemod,
					RSTV0910_P1_TMGREG2);
			i = 0;
			while ((i <= 50) && (timing != 0) && (timing != 0xFF)) {
				timing = ChipGetOneRegister(pParams->hDemod,
						RSTV0910_P1_TMGREG2);
				ChipWaitOrAbort(pParams->hDemod, 5);
				i += 5;
			}
		}
		pParams->Demod1Results.Standard =
		    FE_STV0910_GetStandard(pParams->hDemod, Demod);

		pParams->Demod1Results.Frequency =
		    FE_Sat_TunerGetFrequency(pParams->hTuner1);

		offsetFreq =
		    FE_STV0910_GetCarrierFrequency(pParams->hDemod,
					   pParams->MasterClock, Demod) / 1000;
		pParams->Demod1Results.Frequency += offsetFreq;

		pParams->Demod1Results.SymbolRate =
		    FE_STV0910_GetSymbolRate(pParams->hDemod,
					     pParams->MasterClock, Demod);
		symbolRateOffset =
		    FE_STV0910_TimingGetOffset(pParams->hDemod,
			       pParams->Demod1Results.SymbolRate, Demod);
		pParams->Demod1Results.SymbolRate += symbolRateOffset;
		pParams->Demod1Results.PunctureRate =
		    FE_STV0910_GetViterbiPunctureRate(pParams->hDemod, Demod);
		pParams->Demod1Results.ModCode =
		    ChipGetField(pParams->hDemod, FSTV0910_P1_DEMOD_MODCOD);
		pParams->Demod1Results.Pilots =
		    ChipGetFieldImage(pParams->hDemod, FSTV0910_P1_DEMOD_TYPE)
									& 0x01;
		pParams->Demod1Results.FrameLength =
			((U32)ChipGetFieldImage(pParams->hDemod,
						FSTV0910_P1_DEMOD_TYPE)) >> 1;

		pParams->Demod1Results.RollOff =
		    ChipGetField(pParams->hDemod, FSTV0910_P1_ROLLOFF_STATUS);

		switch (pParams->Demod1Results.Standard) {
		case FE_SAT_DVBS2_STANDARD:
			pParams->Demod1Results.Spectrum =
				ChipGetField(pParams->hDemod,
					    FSTV0910_P1_SPECINV_DEMOD);

			if (pParams->Demod1Results.ModCode <= FE_SAT_QPSK_910)
				pParams->Demod1Results.Modulation =
								FE_SAT_MOD_QPSK;
			else if (pParams->Demod1Results.ModCode <=
					FE_SAT_8PSK_910)
				pParams->Demod1Results.Modulation =
							    FE_SAT_MOD_8PSK;
			else if (pParams->Demod1Results.ModCode <=
				 FE_SAT_16APSK_910)
				pParams->Demod1Results.Modulation =
							    FE_SAT_MOD_16APSK;
			else if (pParams->Demod1Results.ModCode <=
				 FE_SAT_32APSK_910)
				pParams->Demod1Results.Modulation =
							    FE_SAT_MOD_32APSK;
			else
				pParams->Demod1Results.Modulation =
							    FE_SAT_MOD_UNKNOWN;

			break;

		case FE_SAT_DVBS1_STANDARD:
		case FE_SAT_DSS_STANDARD:
			pParams->Demod1Results.Spectrum =
			    ChipGetField(pParams->hDemod, FSTV0910_P1_IQINV);

			pParams->Demod1Results.Modulation = FE_SAT_MOD_QPSK;

			break;

		default:
			break;
		}

		if ((pParams->Demod1SearchAlgo == FE_SAT_BLIND_SEARCH)
		    || (pParams->Demod1SymbolRate < 10000000)) {
			/*in case of blind search the tuner freq may has been
			 * moven, read the new tuner freq and cupute the freq
			 * offset from the original value */
			offsetFreq = pParams->Demod1Results.Frequency -
				pParams->Tuner1Frequency;
			pParams->Tuner1Frequency =
			    FE_Sat_TunerGetFrequency(pParams->hTuner1);

			if ((pParams->Demod1SearchAlgo == FE_SAT_BLIND_SEARCH)
			    && (Satellite_Scan > 0))
				range = FE_SAT_RANGEOK;	/* No check needed */
			else if (ABS(offsetFreq) <=
				   ((pParams->Demod1SearchRange / 2000) + 500))
				range = FE_SAT_RANGEOK;
			else if (ABS(offsetFreq) <=
				 (FE_STV0910_CarrierWidth
				  (pParams->Demod1Results.SymbolRate,
				   pParams->Demod1Results.RollOff) / 2000))
				range = FE_SAT_RANGEOK;
			else
				range = FE_SAT_OUTOFRANGE;
		} else {
			if (ABS(offsetFreq) <=
			    ((pParams->Demod1SearchRange / 2000) + 500))
				range = FE_SAT_RANGEOK;
			else
				range = FE_SAT_OUTOFRANGE;
		}

		break;

	case FE_SAT_DEMOD_2:

		if (pParams->Demod2SearchAlgo == FE_SAT_BLIND_SEARCH) {
			/* if Blind search wait for symbol rate offset
			 * information transfert from the timing loop to the
			 * demodulator symbol rate
			 */
			/*BUG HERE */
			timing = ChipGetOneRegister(pParams->hDemod,
						    RSTV0910_P2_TMGREG2);
			i = 0;
			while ((i <= 50) && (timing != 0) && (timing != 0xFF)) {
				timing =
				    ChipGetOneRegister(pParams->hDemod,
						       RSTV0910_P2_TMGREG2);
				ChipWaitOrAbort(pParams->hDemod, 5);
				i += 5;
			}
		}

		pParams->Demod2Results.Standard =
		    FE_STV0910_GetStandard(pParams->hDemod, Demod);

		pParams->Demod2Results.Frequency =
		    FE_Sat_TunerGetFrequency(pParams->hTuner2);
		offsetFreq =
		    FE_STV0910_GetCarrierFrequency(pParams->hDemod,
					   pParams->MasterClock, Demod) / 1000;
		pParams->Demod2Results.Frequency += offsetFreq;

		pParams->Demod2Results.SymbolRate =
		    FE_STV0910_GetSymbolRate(pParams->hDemod,
					     pParams->MasterClock, Demod);
		symbolRateOffset =
		    FE_STV0910_TimingGetOffset(pParams->hDemod,
			       pParams->Demod2Results.SymbolRate, Demod);
		pParams->Demod2Results.SymbolRate += symbolRateOffset;
		pParams->Demod2Results.PunctureRate =
		    FE_STV0910_GetViterbiPunctureRate(pParams->hDemod, Demod);
		pParams->Demod2Results.ModCode =
		    ChipGetField(pParams->hDemod, FSTV0910_P2_DEMOD_MODCOD);
		pParams->Demod2Results.Pilots =
			ChipGetFieldImage(pParams->hDemod,
					FSTV0910_P2_DEMOD_TYPE) & 0x01;
		pParams->Demod2Results.FrameLength =
			((U32)ChipGetFieldImage(pParams->hDemod,
				FSTV0910_P2_DEMOD_TYPE)) >> 1;
		pParams->Demod2Results.RollOff =
		    ChipGetField(pParams->hDemod, FSTV0910_P2_ROLLOFF_STATUS);

		switch (pParams->Demod2Results.Standard) {
		case FE_SAT_DVBS2_STANDARD:
			pParams->Demod2Results.Spectrum =
			    ChipGetField(pParams->hDemod,
					 FSTV0910_P2_SPECINV_DEMOD);

			if (pParams->Demod2Results.ModCode <= FE_SAT_QPSK_910)
				pParams->Demod2Results.Modulation =
								FE_SAT_MOD_QPSK;
			else if (pParams->Demod2Results.ModCode <=
				 FE_SAT_8PSK_910)
				pParams->Demod2Results.Modulation =
								FE_SAT_MOD_8PSK;
			else if (pParams->Demod2Results.ModCode <=
				 FE_SAT_16APSK_910)
				pParams->Demod2Results.Modulation =
							      FE_SAT_MOD_16APSK;
			else if (pParams->Demod2Results.ModCode <=
				 FE_SAT_32APSK_910)
				pParams->Demod2Results.Modulation =
							FE_SAT_MOD_32APSK;
			else
				pParams->Demod2Results.Modulation =
							FE_SAT_MOD_UNKNOWN;

			break;

		case FE_SAT_DVBS1_STANDARD:
		case FE_SAT_DSS_STANDARD:
			pParams->Demod2Results.Spectrum =
			    ChipGetField(pParams->hDemod, FSTV0910_P2_IQINV);

			pParams->Demod2Results.Modulation = FE_SAT_MOD_QPSK;
			break;

		default:
			break;
		}

		if ((pParams->Demod2SearchAlgo == FE_SAT_BLIND_SEARCH)
		    || (pParams->Demod2SymbolRate < 10000000)) {
			/*in case of blind search the tuner freq may has been
			 * moven, read the new tuner freq and cupute the freq
			 * offset from the original value */
			offsetFreq = pParams->Demod2Results.Frequency -
				pParams->Tuner2Frequency;
			pParams->Tuner2Frequency =
			    FE_Sat_TunerGetFrequency(pParams->hTuner2);

			if ((pParams->Demod2SearchAlgo == FE_SAT_BLIND_SEARCH)
			    && (Satellite_Scan > 0))
				range = FE_SAT_RANGEOK;	/* No check needed */
			else if (ABS(offsetFreq) <=
				   ((pParams->Demod2SearchRange / 2000) + 500))
				range = FE_SAT_RANGEOK;
			else if (ABS(offsetFreq) <=
				 (FE_STV0910_CarrierWidth
				  (pParams->Demod2Results.SymbolRate,
				   pParams->Demod2Results.RollOff) / 2000))
				range = FE_SAT_RANGEOK;
			else
				range = FE_SAT_OUTOFRANGE;
		} else {
			if (ABS(offsetFreq) <=
			    ((pParams->Demod2SearchRange / 2000) + 500))
				range = FE_SAT_RANGEOK;
			else
				range = FE_SAT_OUTOFRANGE;
		}

		break;
	}

	return range;

}

/*! \fn BOOL FE_STV0910_GetDemodLockCold(FE_STV0910_InternalParams_t *pParams,
				S32 DemodTimeout, FE_STV0910_DEMOD_t Demod)
  \brief This function is not used
  */

static BOOL FE_STV0910_GetDemodLockCold(FE_STV0910_InternalParams_t *pParams,
					S32 DemodTimeout,
					FE_STV0910_DEMOD_t Demod)
{

	BOOL lock = FALSE;
	S32 symbolRate,
	    searchRange,
	    nbSteps,
	    currentStep, carrierStep, direction, tunerFreq, timeout, freq;

#ifdef HOST_PC
#ifndef NO_GUI
	char message[300];
#endif
#endif

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		symbolRate = pParams->Demod1SymbolRate;
		searchRange = pParams->Demod1SearchRange;
		break;

	case FE_SAT_DEMOD_2:
		symbolRate = pParams->Demod2SymbolRate;
		searchRange = pParams->Demod2SearchRange;
		break;
	}

#ifdef HOST_PC
#ifndef NO_GUI
	Fmt(message, "ENTERING GetDemodLockCold TIMEOUT = %d", DemodTimeout);
	ReportInsertMessage(message);
#endif
#endif

	if (symbolRate >= 10000000) {

#ifdef HOST_PC
#ifndef NO_GUI
		Fmt(message, "SR is above 10Msps: CARFREQ is 79 default ");
		ReportInsertMessage(message);
#endif
#endif

		/*/3 */
		lock = FE_STV0910_GetDemodLock(pParams->hDemod, Demod,
					       DemodTimeout);

		if (lock == FALSE) {

#ifdef HOST_PC
#ifndef NO_GUI
			Fmt(message,
			"First lock has failed: Accelerate frequency detector");
			ReportInsertMessage(message);
#endif
#endif

			switch (Demod) {
			case FE_SAT_DEMOD_1:
			default:
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0910_P1_CARFREQ, 0x3B);
				/*Reset the Demod */
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0910_P1_DMDISTATE, 0x1F);
				/*Trig an acquisition(start the search) */
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0910_P1_DMDISTATE, 0x15);
				break;

			case FE_SAT_DEMOD_2:
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0910_P2_CARFREQ, 0x3B);
				/*Reset the Demod */
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0910_P2_DMDISTATE, 0x1F);
				/*Trig an acquisition(start the search) */
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0910_P2_DMDISTATE, 0x15);
				break;
			}
			/*/2 */
			lock = FE_STV0910_GetDemodLock(pParams->hDemod, Demod,
						       DemodTimeout);
		}
	} else
		lock = FE_STV0910_GetDemodLock(pParams->hDemod, Demod,
					       DemodTimeout);

	if (lock != FALSE)
		return lock;

	/* Performs zigzag on CFRINIT with AEP 0x05 */
	if (symbolRate <= 10000000)
		carrierStep = symbolRate / 4000;
	else
		carrierStep = 2500;
	timeout = (DemodTimeout * 3) / 4;

	nbSteps = ((searchRange / 1000) / carrierStep);

	if ((nbSteps % 2) != 0)
		nbSteps += 1;

	if (nbSteps <= 0)
		nbSteps = 2;
	else if (nbSteps > 12)
		nbSteps = 12;

	currentStep = 1;
	direction = 1;
	tunerFreq = 0;

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		/*zigzag demod CFRINIT freq */
		while ((currentStep <= nbSteps) && (lock == FALSE)) {

			if (direction > 0)
				tunerFreq += (currentStep * carrierStep);
			else
				tunerFreq -= (currentStep * carrierStep);

#ifdef HOST_PC
#ifndef NO_GUI
			Fmt(message, "Freq = %d KHz", tunerFreq);
			ReportInsertMessage(message);
#endif
#endif

			ChipSetOneRegister(pParams->hDemod,
					RSTV0910_P1_DMDISTATE, 0x1C);

			/* Formula:
			   CFRINIT_Khz = CFRINIT_Reg * MasterClock_Khz /2^16
			 */

			freq = (tunerFreq * 65536) / (pParams->MasterClock /
					1000);
			ChipSetFieldImage(pParams->hDemod,
					FSTV0910_P1_CFR_INIT1, MSB(freq));
			ChipSetFieldImage(pParams->hDemod,
					FSTV0910_P1_CFR_INIT0, LSB(freq));
			/*init the demod frequency offset to 0 */
			ChipSetRegisters(pParams->hDemod,
					 RSTV0910_P1_CFRINIT1, 2);

			ChipSetOneRegister(pParams->hDemod,
					RSTV0910_P1_DMDISTATE, 0x1F);
			/* use AEP 0x05 instead of 0x15 to not reset CFRINIT to
			 * 0 */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0910_P1_DMDISTATE, 0x05);

			lock = FE_STV0910_GetDemodLock(pParams->hDemod, Demod,
					    timeout);

			direction *= -1;
			currentStep++;
		}
		break;

	case FE_SAT_DEMOD_2:
		/*zigzag demod CFRINIT freq */
		while ((currentStep <= nbSteps) && (lock == FALSE)) {

			if (direction > 0)
				tunerFreq += (currentStep * carrierStep);
			else
				tunerFreq -= (currentStep * carrierStep);

#ifdef HOST_PC
#ifndef NO_GUI
			Fmt(message, "Freq = %d KHz", tunerFreq);
			ReportInsertMessage(message);
#endif
#endif

			ChipSetOneRegister(pParams->hDemod,
					   RSTV0910_P2_DMDISTATE, 0x1C);

			/* Formula:
			   CFRINIT_Khz = CFRINIT_Reg * MasterClock_Khz /2^16
			 */

			freq = (tunerFreq * 65536) / (pParams->MasterClock /
					1000);
			ChipSetFieldImage(pParams->hDemod,
					FSTV0910_P2_CFR_INIT1, MSB(freq));
			ChipSetFieldImage(pParams->hDemod,
					FSTV0910_P2_CFR_INIT0, LSB(freq));
			/*init the demod frequency offset to 0 */
			ChipSetRegisters(pParams->hDemod,
					 RSTV0910_P2_CFRINIT1, 2);

			ChipSetOneRegister(pParams->hDemod,
					   RSTV0910_P2_DMDISTATE, 0x1F);
			/* use AEP 0x05 instead of 0x15 to not reset CFRINIT to
			 * 0 */
			ChipSetOneRegister(pParams->hDemod,
					   RSTV0910_P2_DMDISTATE, 0x05);

			lock = FE_STV0910_GetDemodLock(pParams->hDemod, Demod,
					timeout);

			direction *= -1;
			currentStep++;
		}
		break;
	}
	return lock;
}

static U16 FE_STV0910_BlindCheckAGC2BandWidth(FE_STV0910_InternalParams_t *
					      pParams, FE_STV0910_DEMOD_t Demod)
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
	/*Scan AGC2 de la BandWidth */
	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		ChipSetOneRegister(pParams->hDemod, RSTV0910_P1_DMDISTATE,
				   0x1C);
		tmp2 = ChipGetOneRegister(pParams->hDemod, RSTV0910_P1_CARCFG);
		ChipSetOneRegister(pParams->hDemod, RSTV0910_P1_CARCFG, 0x06);

		tmp3 = ChipGetOneRegister(pParams->hDemod, RSTV0910_P1_BCLC);
		ChipSetOneRegister(pParams->hDemod, RSTV0910_P1_BCLC, 0x00);
		tmp4 = ChipGetOneRegister(pParams->hDemod, RSTV0910_P1_CARFREQ);
		ChipSetOneRegister(pParams->hDemod, RSTV0910_P1_CARFREQ, 0x00);

		ChipSetOneRegister(pParams->hDemod, RSTV0910_P1_AGC2REF, 0x38);

		tmp1 =
		    ChipGetOneRegister(pParams->hDemod, RSTV0910_P1_DMDCFGMD);
		ChipSetFieldImage(pParams->hDemod, FSTV0910_P1_DVBS1_ENABLE, 1);
		ChipSetFieldImage(pParams->hDemod, FSTV0910_P1_DVBS2_ENABLE, 1);
		/*Enable the SR SCAN */
		ChipSetFieldImage(pParams->hDemod, FSTV0910_P1_SCAN_ENABLE, 0);
		/*activate the carrier frequency search loop */
		ChipSetFieldImage(pParams->hDemod, FSTV0910_P1_CFR_AUTOSCAN, 0);
		ChipSetRegisters(pParams->hDemod, RSTV0910_P1_DMDCFGMD, 1);

		/*AGC2 bandwidth is 1/div MHz */
		FE_STV0910_SetSymbolRate(pParams->hDemod, pParams->MasterClock,
					 1000000 / div, Demod);

		nbSteps = (pParams->Tuner1BW / 3000000) * div;
		if (nbSteps <= 0)
			nbSteps = 1;

		/* AGC2 step is 1/div MHz */
		freqStep = ((1000000 << 8) / (pParams->MasterClock >> 8)) / div;

		initFreq = 0;
		j = 0;		/* index after a rising edge is found */

		for (i = 0; i < nbSteps; i++) {
			/* Scan on the positive part of the tuner Bw */
			ChipSetOneRegister(pParams->hDemod,
					   RSTV0910_P1_DMDISTATE, 0x1C);
			ChipSetOneRegister(pParams->hDemod,
			   RSTV0910_P1_CFRINIT1, (initFreq >> 8) & 0xff);
			ChipSetOneRegister(pParams->hDemod,
			   RSTV0910_P1_CFRINIT0, initFreq & 0xff);
			ChipSetOneRegister(pParams->hDemod,
					RSTV0910_P1_DMDISTATE, 0x18);
			WAIT_N_MS(5);

			agc2level = 0;

			ChipGetRegisters(pParams->hDemod, RSTV0910_P1_AGC2I1,
					 2);
			agc2level =
			    (ChipGetFieldImage(pParams->hDemod,
					FSTV0910_P1_AGC2_INTEGRATOR1) << 8)
			    | ChipGetFieldImage(pParams->hDemod,
						FSTV0910_P1_AGC2_INTEGRATOR0);

			if (i == 0) {
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

				if (waitforfall == 0)
					agc2ratio = (maxagc2level -
					     minagc2level) * 128 / midagc2level;
				else
					agc2ratio = (agc2level -
					     minagc2level) * 128 / midagc2level;

				if (agc2ratio > 0xffff)
					agc2ratio = 0xffff;

#ifdef HOST_PC
#ifndef NO_GUI
				/* Fmt(message, "AGC2Band i = %d min = %d Max =
				 * %d Mid = %d Ratio = %d agc2leveltab[%d] =
				 * agc2level = %d waitforfall = %d j = %d
				 * asperity = %d", i, minagc2level,
				 * maxagc2level, midagc2level, agc2ratio, k,
				 * agc2level, waitforfall, j, asperity);
				 ReportInsertMessage(message); */

#endif
#endif

				/* rising edge */
				if ((agc2ratio >
				     STV0910_BLIND_SEARCH_AGC2BANDWIDTH)
				    && (agc2level == minagc2level)) {
					/* The first edge is rising */
					asperity = 1;
					waitforfall = 1;
					for (l = 0; l < 5 * div; l++)
						agc2leveltab[l] = agc2level;
				} else if ((agc2ratio >
					 STV0910_BLIND_SEARCH_AGC2BANDWIDTH)) {
					/* Falling edge */
					if (waitforfall == 0)
						/* the first edge is falling */
						asperity = 2;
					else
						asperity = 1;

					if (j != 1)
						break;
					for (l = 0; l < 5 * div; l++)
						agc2leveltab[l] = agc2level;

					j = 0;	/* All reset */
					waitforfall = 0;
					asperity = 0;
				}

				if ((waitforfall == 1) && j == (5 * div))
					break;
				if (waitforfall == 1)
					j += 1;
			}	/* end of i != 0 */

			initFreq = initFreq + freqStep;

		}		/* End of for (i = 0 ; i < nbSteps) */

		ChipSetOneRegister(pParams->hDemod, RSTV0910_P1_DMDCFGMD, tmp1);
		ChipSetOneRegister(pParams->hDemod, RSTV0910_P1_CARCFG, tmp2);
		ChipSetOneRegister(pParams->hDemod, RSTV0910_P1_BCLC, tmp3);
		ChipSetOneRegister(pParams->hDemod, RSTV0910_P1_CARFREQ, tmp4);
		ChipSetOneRegister(pParams->hDemod, RSTV0910_P1_DMDISTATE,
				   0x1C);
		ChipSetOneRegister(pParams->hDemod, RSTV0910_P1_CFRINIT1, 0);
		ChipSetOneRegister(pParams->hDemod, RSTV0910_P1_CFRINIT0, 0);

		/* rising edge followed by a constant level or a falling edge */
		if (asperity == 1)
			pParams->Tuner1IndexJump = (1000 / div) * (i - (j + 2)
					/ 2);
		else
			/* falling edge */
			pParams->Tuner1IndexJump = (1000 / div) * i;

		break;

	case FE_SAT_DEMOD_2:
		ChipSetOneRegister(pParams->hDemod, RSTV0910_P2_DMDISTATE,
				   0x1C);
		tmp2 = ChipGetOneRegister(pParams->hDemod, RSTV0910_P2_CARCFG);
		ChipSetOneRegister(pParams->hDemod, RSTV0910_P2_CARCFG, 0x06);

		tmp3 = ChipGetOneRegister(pParams->hDemod, RSTV0910_P2_BCLC);
		ChipSetOneRegister(pParams->hDemod, RSTV0910_P2_BCLC, 0x00);
		tmp4 = ChipGetOneRegister(pParams->hDemod, RSTV0910_P2_CARFREQ);
		ChipSetOneRegister(pParams->hDemod, RSTV0910_P2_CARFREQ, 0x00);

		ChipSetOneRegister(pParams->hDemod, RSTV0910_P2_AGC2REF, 0x38);

		tmp1 =
		    ChipGetOneRegister(pParams->hDemod, RSTV0910_P2_DMDCFGMD);
		ChipSetFieldImage(pParams->hDemod, FSTV0910_P2_DVBS1_ENABLE, 1);
		ChipSetFieldImage(pParams->hDemod, FSTV0910_P2_DVBS2_ENABLE, 1);
		/*Enable the SR SCAN */
		ChipSetFieldImage(pParams->hDemod, FSTV0910_P2_SCAN_ENABLE, 0);
		/*activate the carrier frequency search loop */
		ChipSetFieldImage(pParams->hDemod, FSTV0910_P2_CFR_AUTOSCAN, 0);
		ChipSetRegisters(pParams->hDemod, RSTV0910_P2_DMDCFGMD, 1);

		/*AGC2 bandwidth is 1/div MHz */
		FE_STV0910_SetSymbolRate(pParams->hDemod, pParams->MasterClock,
					 1000000 / div, Demod);

		nbSteps = (pParams->Tuner2BW / 3000000) * div;
		if (nbSteps <= 0)
			nbSteps = 1;

		/* AGC2 step is 1/div MHz */
		freqStep = ((1000000 << 8) / (pParams->MasterClock >> 8)) / div;

		initFreq = 0;
		/* index after a rising edge is found */
		j = 0;

		for (i = 0; i < nbSteps; i++) {
			/* Scan on the positive part of the tuner Bw */

			ChipSetOneRegister(pParams->hDemod,
					   RSTV0910_P2_DMDISTATE, 0x1C);
			ChipSetOneRegister(pParams->hDemod,
			   RSTV0910_P2_CFRINIT1, (initFreq >> 8) & 0xff);
			ChipSetOneRegister(pParams->hDemod,
				   RSTV0910_P2_CFRINIT0, initFreq & 0xff);
			ChipSetOneRegister(pParams->hDemod,
					RSTV0910_P2_DMDISTATE, 0x18);
			WAIT_N_MS(5);

			agc2level = 0;

			ChipGetRegisters(pParams->hDemod, RSTV0910_P2_AGC2I1,
					 2);
			agc2level =
			    (ChipGetFieldImage(pParams->hDemod,
				       FSTV0910_P2_AGC2_INTEGRATOR1) << 8)
			    | ChipGetFieldImage(pParams->hDemod,
					FSTV0910_P2_AGC2_INTEGRATOR0);

			if (i == 0) {
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
					agc2ratio = (maxagc2level -
					     minagc2level) * 128 / midagc2level;
				} else {
					agc2ratio = (agc2level -
					     minagc2level) * 128 / midagc2level;
				}

				if (agc2ratio > 0xffff)
					agc2ratio = 0xffff;

#ifdef HOST_PC
#ifndef NO_GUI
				Fmt(message,
				    "AGC2Band i = %d min = %d Max = %d Mid = %d Ratio = %d agc2leveltab[%d] = agc2level = %d waitforfall = %d j = %d asperity = %d",
				    i, minagc2level, maxagc2level, midagc2level,
				    agc2ratio, k, agc2level, waitforfall, j,
				    asperity);
				/*Fmt(message, "%d", agc2level); */
				ReportInsertMessage(message);
				/*Fmt(message, "AGC2Band i      %x %x %d %d",
				 * initFreq,
				 * ChipGetOneRegister(pParams->hDemod,
				 * RSTV0910_CFR2) << 8, agc2level, agc2ratio);
				 * */
				/* ReportInsertMessage(message); */
#endif
#endif

				/* rising edge */
				if ((agc2ratio >
				     STV0910_BLIND_SEARCH_AGC2BANDWIDTH)
				    && (agc2level == minagc2level)) {
					/* The first edge is rising */
					asperity = 1;
					waitforfall = 1;
					for (l = 0; l < 5 * div; l++)
						agc2leveltab[l] = agc2level;
				} else if ((agc2ratio >
					 STV0910_BLIND_SEARCH_AGC2BANDWIDTH)) {
					/* Falling edge */
					if (waitforfall == 0)
						/* the first edge is falling */
						asperity = 2;
					else
						asperity = 1;

					if (j != 1)
						break;
					for (l = 0; l < 5 * div; l++)
						agc2leveltab[l] = agc2level;

					j = 0;	/* All reset */
					waitforfall = 0;
					asperity = 0;
				}

				if ((waitforfall == 1) && j == (5 * div))
					break;
				if (waitforfall == 1)
					j += 1;
			}	/* end of i != 0 */

			initFreq = initFreq + freqStep;

		}		/* End of for (i = 0 ; i < nbSteps) */

		ChipSetOneRegister(pParams->hDemod, RSTV0910_P2_DMDCFGMD, tmp1);
		ChipSetOneRegister(pParams->hDemod, RSTV0910_P2_CARCFG, tmp2);
		ChipSetOneRegister(pParams->hDemod, RSTV0910_P2_BCLC, tmp3);
		ChipSetOneRegister(pParams->hDemod, RSTV0910_P2_CARFREQ, tmp4);
		ChipSetOneRegister(pParams->hDemod, RSTV0910_P2_DMDISTATE,
				   0x1C);
		ChipSetOneRegister(pParams->hDemod, RSTV0910_P2_CFRINIT1, 0);
		ChipSetOneRegister(pParams->hDemod, RSTV0910_P2_CFRINIT0, 0);

		/* rising edge followed by a constant level or a falling edge */
		if (asperity == 1)
			pParams->Tuner2IndexJump = (1000 / div) * (i - (j + 2)
					/ 2);
		else
			/* falling edge */
			pParams->Tuner2IndexJump = (1000 / div) * i;

		break;
	}			/* End of     switch demod */

	return (U16) asperity;
}

/*! \fn BOOL FE_STV0910_BlindSearchAlgo(FE_STV0910_InternalParams_t *pParams,
						FE_STV0910_DEMOD_t Demod)
  \brief This function is used in complementary with the blind search hardware
  to improve the blind search performance
  */
static BOOL FE_STV0910_BlindSearchAlgo(FE_STV0910_InternalParams_t *pParams,
				       S32 demodTimeout,
				       FE_STV0910_DEMOD_t Demod,
				       U32 Satellite_Scan)
{
	BOOL lock = TRUE;

#ifdef HOST_PC
#ifndef NO_GUI
	char message[100];
	double lockTime;
	lockTime = Timer();
#endif
#endif

#ifdef HOST_PC
#ifndef NO_GUI
	Fmt(message, "YES(FE_STV0910_BlindSearchAlgo)");
#endif
#endif

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		if (Satellite_Scan == 0) {
			pParams->Demod1SearchRange = 2 * 36000000;
		} else {
			pParams->Demod1SearchRange =
			    24000000 + pParams->Tuner1IndexJump * 1000;
			if (pParams->Demod1SearchRange > 40000000)
				pParams->Demod1SearchRange = 40000000;
		}
		/* Demod Stop */
		ChipSetOneRegister(pParams->hDemod, RSTV0910_P1_DMDISTATE,
				   0x5C);
		FE_STV0910_SetSymbolRate(pParams->hDemod, pParams->MasterClock,
					 pParams->Demod1SymbolRate, Demod);

		/* open the ReedSolomon to viterbi feedback until demod lock */
		ChipSetField(pParams->hDemod, FSTV0910_P1_DIS_RSFLOCK, 1);
		/* open Viterbi feedback until demod lock */
		ChipSetField(pParams->hDemod, FSTV0910_P1_DIS_VITLOCK, 1);

		break;

	case FE_SAT_DEMOD_2:
		if (Satellite_Scan == 0) {
			pParams->Demod2SearchRange = 2 * 36000000;
		} else {
			pParams->Demod2SearchRange =
			    24000000 + pParams->Tuner2IndexJump * 1000;
			if (pParams->Demod2SearchRange > 40000000)
				pParams->Demod2SearchRange = 40000000;
		}
		/* Demod Stop */
		ChipSetOneRegister(pParams->hDemod, RSTV0910_P2_DMDISTATE,
				   0x5C);
		FE_STV0910_SetSymbolRate(pParams->hDemod, pParams->MasterClock,
					 pParams->Demod2SymbolRate, Demod);
		/* open the ReedSolomon to viterbi feedback until demod lock */
		ChipSetField(pParams->hDemod, FSTV0910_P2_DIS_RSFLOCK, 1);
		/* open Viterbi feedback until demod lock */
		ChipSetField(pParams->hDemod, FSTV0910_P2_DIS_VITLOCK, 1);

		break;
	}
	FE_STV0910_StartSearch(pParams, Demod);
	lock = FE_STV0910_GetDemodLock(pParams->hDemod, Demod, demodTimeout);
	return lock;
}

/*****************************************************
  --FUNCTION	::	FE_STV0910_GetSignalInfoLite
  --ACTION	::	Return C/N only
  --PARAMS IN	::	Handle	==>	Front End Handle
  ::	Demod	==>	Current demod 1 or 2
  --PARAMS OUT::	pInfo	==> Informations(C/N)
  --RETURN	::	Error(if any)
  --***************************************************/
/*! \fn
  \brief This  function return s C/N only
  */
FE_STV0910_Error_t FE_STV0910_GetSignalInfoLite(FE_STV0910_Handle_t Handle,
						FE_STV0910_DEMOD_t Demod,
						FE_STV0910_SignalInfo_t *pInfo)
{
	FE_STV0910_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0910_InternalParams_t *pParams = NULL;

	pParams = (FE_STV0910_InternalParams_t *) Handle;

	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		if ((pParams->hDemod->Error) || (pParams->hTuner1->Error))
			return FE_LLA_I2C_ERROR;

		pInfo->Standard =
		    FE_STV0910_GetStandard(pParams->hDemod, Demod);
		if (pInfo->Standard == FE_SAT_DVBS2_STANDARD)
			pInfo->C_N =
			    FE_STV0910_CarrierGetQuality(pParams->hDemod,
			     &FE_STV0910_S2_CN_LookUp, Demod);
		else	/*DVBS1/DSS */
			pInfo->C_N =
			    FE_STV0910_CarrierGetQuality(pParams->hDemod,
			     &FE_STV0910_S1_CN_LookUp, Demod);

		if ((pParams->hDemod->Error) || (pParams->hTuner1->Error))
			error = FE_LLA_I2C_ERROR;

		break;

	case FE_SAT_DEMOD_2:

		if ((pParams->hDemod->Error) || (pParams->hTuner2->Error))
			return FE_LLA_I2C_ERROR;

		pInfo->Standard =
		    FE_STV0910_GetStandard(pParams->hDemod, Demod);

		if (pInfo->Standard == FE_SAT_DVBS2_STANDARD)
			pInfo->C_N =
			    FE_STV0910_CarrierGetQuality(pParams->hDemod,
			     &FE_STV0910_S2_CN_LookUp, Demod);

		else	/*DVBS1/DSS */
			pInfo->C_N =
			    FE_STV0910_CarrierGetQuality(pParams->hDemod,
			     &FE_STV0910_S1_CN_LookUp, Demod);

		if ((pParams->hDemod->Error) || (pParams->hTuner2->Error))
			error = FE_LLA_I2C_ERROR;

		break;
	}

	return error;
}

/*! \fn FE_STV0910_Error_t FE_STV0910_8PSKCarrierAdaptation(FE_STV0910_Handle_t
					Handle, FE_STV0910_DEMOD_t Demod);
  \brief This private function is used in case of utilization of frequency
  shifter with high jitter and phase noise to improve the tracking
  */
static FE_STV0910_Error_t FE_STV0910_8PSKCarrierAdaptation(FE_STV0910_Handle_t
							   Handle,
							   FE_STV0910_DEMOD_t
							   Demod)
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
	FE_STV0910_SignalInfo_t pInfo;
	FE_STV0910_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0910_InternalParams_t *pParams = NULL;

	if (Handle == NULL)
		return FE_LLA_INVALID_HANDLE;
	pParams = (FE_STV0910_InternalParams_t *) Handle;

	if (pParams->hDemod->Error)
		return FE_LLA_I2C_ERROR;

	FE_STV0910_GetSignalInfoLite(pParams, Demod, &pInfo);
	if (pInfo.C_N < 80) {
		switch (Demod) {
		case FE_SAT_DEMOD_1:
		default:
			ChipSetOneRegister(pParams->hDemod, RSTV0910_P1_CARHDR,
					0x04);
			ChipSetOneRegister(pParams->hDemod,
					RSTV0910_P1_BCLC2S28, 0x31);
			break;
		case FE_SAT_DEMOD_2:
			ChipSetOneRegister(pParams->hDemod, RSTV0910_P2_CARHDR,
					0x04);
			ChipSetOneRegister(pParams->hDemod,
					RSTV0910_P2_BCLC2S28, 0x31);
			break;
		}
	}
	if (pParams->hDemod->Error)
		error = FE_LLA_I2C_ERROR;

	return error;
}

/*! \fn S32 FE_STV0910_SetHMRFilter(STCHIP_Handle_t hChip,
  TUNER_Handle_t hTuner,
  STCHIP_Handle_t hChipVGLNA,
  FE_STV0910_DEMOD_t Demod)
  \brief This function is used only in case of 6120 tuner to filter harmonic
  frequencies.
  */
S32 FE_STV0910_SetHMRFilter(STCHIP_Handle_t hChip,
			    TUNER_Handle_t hTuner,
			    STCHIP_Handle_t hChipVGLNA,
			    FE_STV0910_DEMOD_t Demod)
{
	S32 hmrVal = 0,
	    agc1InitVal,
	    agc1PowerVal2, agc1Reg = 0, agc1Field_msb = 0, agc1Field_lsb = 0;

	U32 tunerFrequency;
	U8 BBgain = 12;

	/* HMR filter calibration, specific 6120 */
	if ((((SAT_TUNER_Params_t)(hTuner->pData))->Model !=
			TUNER_STV6120_Tuner1) &&
			(((SAT_TUNER_Params_t)(hTuner->pData))->Model !=
			 TUNER_STV6120_Tuner2))
		return hmrVal;

	switch (Demod) {
	case FE_SAT_DEMOD_1:
		agc1Reg = RSTV0910_P1_AGCIQIN1;
		agc1Field_msb = FSTV0910_P1_AGCIQ_VALUE1;
		agc1Field_lsb = FSTV0910_P1_AGCIQ_VALUE0;
		break;

	case FE_SAT_DEMOD_2:
		agc1Reg = RSTV0910_P2_AGCIQIN1;
		agc1Field_msb = FSTV0910_P2_AGCIQ_VALUE1;
		agc1Field_lsb = FSTV0910_P2_AGCIQ_VALUE0;
		break;
	}

	if (FE_STV0910_CheckVGLNAPresent(hChipVGLNA) == 0) {
		BBgain = 6;
	} else {
		if ((ChipGetField(hChipVGLNA, FSTVVGLNA_SWLNAGAIN) == 3)
		    && (ChipGetField(hChipVGLNA, FSTVVGLNA_RFAGCLOW) == 1))
			BBgain = 6;
		else
			BBgain = 12;
	}

	FE_Sat_TunerSetGain(hTuner, BBgain);
	FE_Sat_TunerSetHMR_Filter(hTuner, 0);

	tunerFrequency = FE_Sat_TunerGetFrequency(hTuner);

	if (tunerFrequency >= 1590000)
		return hmrVal;

	ChipGetRegisters(hChip, agc1Reg, 2);
	agc1InitVal = MAKEWORD(ChipGetFieldImage(hChip, agc1Field_msb),
			ChipGetFieldImage(hChip, agc1Field_lsb));

	FE_Sat_TunerSetGain(hTuner, BBgain + 2);
	ChipWaitOrAbort(hChip, 5);

	ChipGetRegisters(hChip, agc1Reg, 2);
	agc1PowerVal2 = MAKEWORD(ChipGetFieldImage(hChip, agc1Field_msb),
			ChipGetFieldImage(hChip, agc1Field_lsb));
	hmrVal = 0;

	if (agc1InitVal == 0)
		FE_Sat_TunerSetHMR_Filter(hTuner, 31);
	else {
		while ((agc1PowerVal2 > agc1InitVal) && (hmrVal < 29)) {
			hmrVal += 2;
			FE_Sat_TunerSetHMR_Filter(hTuner, hmrVal);

			ChipWaitOrAbort(hChip, 5);
			ChipGetRegisters(hChip, agc1Reg, 2);
			agc1PowerVal2 =
			    MAKEWORD(ChipGetFieldImage(hChip, agc1Field_msb),
				ChipGetFieldImage(hChip, agc1Field_lsb));
		}
	}

	return hmrVal;
}

/*! \fn FE_STV0910_Error_t FE_STV0910_STV6120_HMRFilter(FE_STV0910_Handle_t
					Handle, FE_STV0910_DEMOD_t Demod);
  \brief This private function is used to optimize the STV6120 tuner
  performance.
  */
FE_STV0910_Error_t FE_STV0910_STV6120_HMRFilter(FE_STV0910_Handle_t Handle,
						FE_STV0910_DEMOD_t Demod)
{
	FE_STV0910_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0910_InternalParams_t *pParams =
	    (FE_STV0910_InternalParams_t *) Handle;

	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;

	switch (Demod) {
	case FE_SAT_DEMOD_1:

		if ((pParams->hDemod->Error) || (pParams->hTuner1->Error))
			return FE_LLA_I2C_ERROR;
		FE_STV0910_SetHMRFilter(pParams->hDemod, pParams->hTuner1,
				pParams->hVGLNA1, Demod);
		/*Check the error at the end of the function */
		if ((pParams->hDemod->Error) || (pParams->hTuner1->Error))
			error = FE_LLA_I2C_ERROR;
		break;

	case FE_SAT_DEMOD_2:
		if ((pParams->hDemod->Error) || (pParams->hTuner2->Error))
			return FE_LLA_I2C_ERROR;

		FE_STV0910_SetHMRFilter(pParams->hDemod, pParams->hTuner2,
				pParams->hVGLNA2, Demod);
		/*Check the error at the end of the function */
		if ((pParams->hDemod->Error) || (pParams->hTuner2->Error))
			error = FE_LLA_I2C_ERROR;
		break;

	default:
		break;
	}

	return error;
}

/*! \fn void FE_STV0910_SetChargePump(FE_STV0910_Handle_t Handle, U32 n_div)
  \brief This private function is used to set the PLL for MCLK.
  */
static void FE_STV0910_SetChargePump(FE_STV0910_Handle_t Handle, U32 n_div)
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

	ChipSetField(Handle, FSTV0910_CP, cp);
}

/*****************************************************
  --FUNCTION	::	FE_STV0910_Algo
  --ACTION	::	Start a search for valid DVBS1/DVBS2 or DSS transponder
  --PARAMS IN	::	pParams	->Pointer FE_STV0910_InternalParams_t structer
  ::	Demod	->	current demod 1 or 2
  --PARAMS OUT::	NONE
  --RETURN	::	SYGNAL TYPE
  --***************************************************/
/*! \fn FE_STV0910_SIGNALTYPE_t	FE_STV0910_Algo(FE_STV0910_InternalParams_t
					*pParams, FE_STV0910_DEMOD_t Demod)
  \brief This function starts a search for a valid DVBS1/DVBS2 or DSS
  transponder.
  */
static FE_STV0910_SIGNALTYPE_t FE_STV0910_Algo(FE_STV0910_InternalParams_t *
					       pParams,
					       FE_STV0910_DEMOD_t Demod,
					       U32 Satellite_Scan)
{
	S32 demodTimeout = 500,
	    fecTimeout = 50,
	    streamMergerField,
	    iqPower,
	    agc1Power,
	    i, powerThreshold = STV0910_IQPOWER_THRESHOLD, puncturerate;
	U32 asperity = 0;
	BOOL lock = FALSE;

	FE_STV0910_SIGNALTYPE_t signalType = FE_SAT_NOCARRIER;

#ifdef HOST_PC
#ifndef NO_GUI
	char message[100];
	double lockTime;
	lockTime = Timer();
#endif
#endif

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:

		powerThreshold = STV0910_IQPOWER_THRESHOLD;

		/*release reset DVBS2 packet delin */
		ChipSetField(pParams->hDemod, FSTV0910_P1_ALGOSWRST, 0);

		streamMergerField = FSTV0910_P1_RST_HWARE;

		/*Stop the stream merger befor stopping the demod */
		/* Demod Stop */
		ChipSetOneRegister(pParams->hDemod, RSTV0910_P1_DMDISTATE,
				   0x5C);
		/*Get the demod and FEC timeout recomended value depending of
		 * the symbol rate and the search algo */
		FE_STV0910_GetLockTimeout(&demodTimeout, &fecTimeout,
					  pParams->Demod1SymbolRate,
					  pParams->Demod1SearchAlgo);

		/* If the Symbolrate is unknown  set the BW to the maximum */
		if (pParams->Demod1SearchAlgo == FE_SAT_BLIND_SEARCH) {
			pParams->Tuner1BW = 2 * 36000000;
			/* If Blind search set the init symbol rate to 1Msps */
			FE_STV0910_SetSymbolRate(pParams->hDemod,
					 pParams->MasterClock,
					 pParams->Demod1SymbolRate, Demod);
		} else {
			pParams->Tuner1BW =
			    (FE_STV0910_CarrierWidth(pParams->Demod1SymbolRate,
			      pParams->RollOff) + pParams->Demod1SearchRange);
			pParams->Tuner1BW = pParams->Tuner1BW * 15 / 10;
			/* Set the Init Symbol rate */
			FE_STV0910_SetSymbolRate(pParams->hDemod,
					 pParams->MasterClock,
					 pParams->Demod1SymbolRate, Demod);
		}

		FE_Sat_TunerSetFrequency(pParams->hTuner1,
					 pParams->Tuner1Frequency);
		FE_Sat_TunerSetBandwidth(pParams->hTuner1, pParams->Tuner1BW);

/*
			FE_STV0910_STV6120_HMRFilter(pParams, Demod);
*/
		/*      NO signal Detection */
		/*Read PowerI and PowerQ To check the signal Presence */
		ChipWaitOrAbort(pParams->hDemod, 10);
		ChipGetRegisters(pParams->hDemod, RSTV0910_P1_AGCIQIN1, 2);
		agc1Power =
		    MAKEWORD(ChipGetFieldImage(pParams->hDemod,
					    FSTV0910_P1_AGCIQ_VALUE1),
			     ChipGetFieldImage(pParams->hDemod,
					       FSTV0910_P1_AGCIQ_VALUE0));
		iqPower = 0;
		if (agc1Power == 0) {
			/*if AGC1 integrator value is 0 then read POWERI,
			 * POWERQ registers */
			/*Read the IQ power value */
			for (i = 0; i < 5; i++) {
				ChipGetRegisters(pParams->hDemod,
						 RSTV0910_P1_POWERI, 2);
				iqPower +=
				    (ChipGetFieldImage(pParams->hDemod,
						       FSTV0910_P1_POWER_I) +
				     ChipGetFieldImage(pParams->hDemod,
					      FSTV0910_P1_POWER_Q)) / 2;
			}
			iqPower /= 5;
		}
#ifdef HOST_PC
#ifndef NO_GUI
		/*if PC GUI read the IQ power threshold form the GUI user
		 * parameters */
		UsrRdInt("P1NOSignalThreshold", &powerThreshold);
#endif
#endif

		if (((agc1Power != 0) || (iqPower >= powerThreshold)) &&
			      (pParams->Demod1SearchAlgo == FE_SAT_BLIND_SEARCH)
			      && (Satellite_Scan == 1))
			/* Perform edge detection */
			asperity =
			    FE_STV0910_BlindCheckAGC2BandWidth(pParams, Demod);

		if ((agc1Power == 0) && (iqPower < powerThreshold)) {
			/*If(AGC1 = 0 && iqPower < IQThreshold)then no signal */
			/*if AGC1 integrator == 0 and iqPower < Threshold then
			 * NO signal */
			pParams->Demod1Results.Locked = FALSE;
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

		} else if ((pParams->Demod1SearchAlgo == FE_SAT_BLIND_SEARCH)
			   && (Satellite_Scan == 1)
			   && (asperity == 0)) {
			iqPower = 0;
			agc1Power = 0;
			/*if AGC1 integrator == 0 and iqPower < Threshold then
			 * NO signal */
			pParams->Demod1Results.Locked = FALSE;
			/* No edge detected, jump tuner bandwidth */
			signalType = FE_SAT_TUNER_NOSIGNAL;

#ifdef HOST_PC
#ifndef NO_GUI
			ReportInsertMessage
			    ("Nothing inside tuner bandwidth, BW/2 jump ");
			ReportInsertMessage
			    ("-----------------------------------------");
#endif
#endif

		} else if ((pParams->Demod1SearchAlgo == FE_SAT_BLIND_SEARCH)
			   && (Satellite_Scan == 1)
			   && (asperity == 1)) {
			/* rising edge detected, perform blind search at next
			 * step */
			iqPower = 0;
			agc1Power = 0;
			pParams->Demod1Results.Locked = FALSE;
			signalType = FE_SAT_TUNER_JUMP;

#ifdef HOST_PC
#ifndef NO_GUI
			ReportInsertMessage("Tuner Jump");
#endif
#endif
		} else {
			/* falling edge detected or direct blind to be done */

			/*Set the IQ inversion search mode */
			/* Set IQ inversion always in auto */
			ChipSetFieldImage(pParams->hDemod,
					  FSTV0910_P1_SPECINV_CONTROL,
					  /*pParams->Demod1Search_IQ_Inv */
					  FE_SAT_IQ_AUTO);
			ChipSetRegisters(pParams->hDemod, RSTV0910_P1_DEMOD, 1);

			FE_STV0910_SetSearchStandard(pParams, Demod);

			/* 8PSK 3/5, 8PSK 2/3 Poff tracking optimization WA */
			ChipSetOneRegister(pParams->hDemod,
					   RSTV0910_P1_ACLC2S2Q, 0x0B);
			ChipSetOneRegister(pParams->hDemod,
					   RSTV0910_P1_ACLC2S28, 0x0A);
			ChipSetOneRegister(pParams->hDemod,
					   RSTV0910_P1_BCLC2S2Q, 0x84);
			ChipSetOneRegister(pParams->hDemod,
					   RSTV0910_P1_BCLC2S28, 0x84);
			ChipSetOneRegister(pParams->hDemod, RSTV0910_P1_CARHDR,
					   0x1C);
			ChipSetOneRegister(pParams->hDemod, RSTV0910_P1_CARFREQ,
					   0x79);

			ChipGetRegisters(pParams->hDemod, RSTV0910_P1_VTH12, 6);

			if (pParams->Demod1SearchAlgo != FE_SAT_BLIND_SEARCH)
				FE_STV0910_StartSearch(pParams, Demod);

			/* Special algo for blind search only */
			if (pParams->Demod1SearchAlgo == FE_SAT_BLIND_SEARCH)
				lock =
				    FE_STV0910_BlindSearchAlgo(pParams,
					demodTimeout, Demod, Satellite_Scan);
			else if (pParams->Demod1SearchAlgo == FE_SAT_COLD_START)
				/* Case cold start wait for demod lock */
				lock =
				    FE_STV0910_GetDemodLockCold(pParams,
						    demodTimeout, Demod);
			else if (pParams->Demod1SearchAlgo == FE_SAT_WARM_START)
				/* Case warm start wait for demod lock */
				lock =
				    FE_STV0910_GetDemodLock(pParams->hDemod,
						    Demod, demodTimeout);

			if (lock == TRUE) {
				/* Read signal caracteristics and check the
				 * lock range */
				signalType =
					FE_STV0910_GetSignalParams(pParams,
							Demod, Satellite_Scan);
			}
		}

		break;

	case FE_SAT_DEMOD_2:

		powerThreshold = STV0910_IQPOWER_THRESHOLD;

		/*release reset DVBS2 packet delin */
		ChipSetField(pParams->hDemod, FSTV0910_P2_ALGOSWRST, 0);

		streamMergerField = FSTV0910_P2_RST_HWARE;

		/*Stop the stream merger befor stopping the demod */
		/* Demod Stop */
		ChipSetOneRegister(pParams->hDemod, RSTV0910_P2_DMDISTATE,
				   0x5C);
		/*Get the demod and FEC timeout recomended value depending of
		 * the symbol rate and the search algo */
		FE_STV0910_GetLockTimeout(&demodTimeout, &fecTimeout,
					  pParams->Demod2SymbolRate,
					  pParams->Demod2SearchAlgo);

		/* If the Symbolrate is unknown  set the BW to the maximum */
		if (pParams->Demod2SearchAlgo == FE_SAT_BLIND_SEARCH) {
			pParams->Tuner2BW = 2 * 36000000;
			/* If Blind search set the init symbol rate to 1Msps */
			FE_STV0910_SetSymbolRate(pParams->hDemod,
					 pParams->MasterClock,
					 pParams->Demod2SymbolRate, Demod);
		} else {
			pParams->Tuner2BW =
			    (FE_STV0910_CarrierWidth(pParams->Demod2SymbolRate,
			      pParams->RollOff) + pParams->Demod2SearchRange);
			pParams->Tuner2BW = pParams->Tuner2BW * 15 / 10;
			/* Set the Init Symbol rate */
			FE_STV0910_SetSymbolRate(pParams->hDemod,
					 pParams->MasterClock,
					 pParams->Demod2SymbolRate, Demod);
		}

		FE_Sat_TunerSetFrequency(pParams->hTuner2,
					 pParams->Tuner2Frequency);
		FE_Sat_TunerSetBandwidth(pParams->hTuner2, pParams->Tuner2BW);

/*
		FE_STV0910_STV6120_HMRFilter(pParams, Demod);
*/
		/*      NO signal Detection */
		/*Read PowerI and PowerQ To check the signal Presence */
		ChipWaitOrAbort(pParams->hDemod, 10);
		ChipGetRegisters(pParams->hDemod, RSTV0910_P2_AGCIQIN1, 2);
		agc1Power =
		    MAKEWORD(ChipGetFieldImage(pParams->hDemod,
					    FSTV0910_P2_AGCIQ_VALUE1),
			     ChipGetFieldImage(pParams->hDemod,
					       FSTV0910_P2_AGCIQ_VALUE0));
		iqPower = 0;
		if (agc1Power == 0) {
			/*if AGC1 integrator value is 0 then read POWERI,
			 * POWERQ registers */
			/*Read the IQ power value */
			for (i = 0; i < 5; i++) {
				ChipGetRegisters(pParams->hDemod,
						 RSTV0910_P2_POWERI, 2);
				iqPower +=
				    (ChipGetFieldImage(pParams->hDemod,
						FSTV0910_P2_POWER_I) +
				     ChipGetFieldImage(pParams->hDemod,
						FSTV0910_P2_POWER_Q)) / 2;
			}
			iqPower /= 5;
		}
#ifdef HOST_PC
#ifndef NO_GUI
		/*if PC GUI read the IQ power threshold form the GUI user
		 * parameters */
		UsrRdInt("P1NOSignalThreshold", &powerThreshold);
#endif
#endif

		if (((agc1Power != 0) || (iqPower >= powerThreshold))
		    && (pParams->Demod2SearchAlgo == FE_SAT_BLIND_SEARCH)
		    && (Satellite_Scan == 1))	/* Perform edge detection */
			asperity =
			    FE_STV0910_BlindCheckAGC2BandWidth(pParams, Demod);

		if ((agc1Power == 0) && (iqPower < powerThreshold)) {
			/*If(AGC1 = 0 and iqPower < IQThreshold)  then no
			 * signal  */
			/*if AGC1 integrator == 0 and iqPower < Threshold then
			 * NO signal */
			pParams->Demod2Results.Locked = FALSE;
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

		} else if ((pParams->Demod2SearchAlgo == FE_SAT_BLIND_SEARCH)
			   && (Satellite_Scan == 1)
			   && (asperity == 0)) {
			iqPower = 0;
			agc1Power = 0;
			/*if AGC1 integrator == 0 and iqPower < Threshold then
			 * NO signal */
			pParams->Demod2Results.Locked = FALSE;
			/* No edge detected, jump tuner bandwidth */
			signalType = FE_SAT_TUNER_NOSIGNAL;

#ifdef HOST_PC
#ifndef NO_GUI
			ReportInsertMessage("BW/2 tuner jump ");
#endif
#endif

		} else if ((pParams->Demod2SearchAlgo == FE_SAT_BLIND_SEARCH)
			   && (Satellite_Scan == 1)
			   && (asperity == 1)) {
			/* rising edge detected, perform blind search at next
			 * step */
			iqPower = 0;
			agc1Power = 0;
			pParams->Demod2Results.Locked = FALSE;
			signalType = FE_SAT_TUNER_JUMP;

#ifdef HOST_PC
#ifndef NO_GUI
			ReportInsertMessage("Rising edge detected");
#endif
#endif
		} else {
			/* falling edge detected or direct blind to be done */

			/*Set the IQ inversion search mode */
			ChipSetFieldImage(pParams->hDemod,
					  FSTV0910_P2_SPECINV_CONTROL,
					  /*pParams->Demod2Search_IQ_Inv */
					  FE_SAT_IQ_AUTO);
			ChipSetRegisters(pParams->hDemod, RSTV0910_P2_DEMOD, 1);

			FE_STV0910_SetSearchStandard(pParams, Demod);

			/* 8PSK 3/5, 8PSK 2/3 Poff tracking optimization WA */
			ChipSetOneRegister(pParams->hDemod,
					   RSTV0910_P2_ACLC2S2Q, 0x0B);
			ChipSetOneRegister(pParams->hDemod,
					   RSTV0910_P2_ACLC2S28, 0x0A);
			ChipSetOneRegister(pParams->hDemod,
					   RSTV0910_P2_BCLC2S2Q, 0x84);
			ChipSetOneRegister(pParams->hDemod,
					   RSTV0910_P2_BCLC2S28, 0x84);
			ChipSetOneRegister(pParams->hDemod, RSTV0910_P2_CARHDR,
					   0x1C);
			ChipSetOneRegister(pParams->hDemod, RSTV0910_P2_CARFREQ,
					   0x79);

			ChipGetRegisters(pParams->hDemod, RSTV0910_P2_VTH12, 6);

			if (pParams->Demod2SearchAlgo != FE_SAT_BLIND_SEARCH)
				FE_STV0910_StartSearch(pParams, Demod);

			/* Special algo for blind search only */
			if (pParams->Demod2SearchAlgo == FE_SAT_BLIND_SEARCH)
				lock =
				    FE_STV0910_BlindSearchAlgo(pParams,
							       demodTimeout,
							       Demod,
							       Satellite_Scan);
			else if (pParams->Demod2SearchAlgo == FE_SAT_COLD_START)
				/* Case cold start wait for demod lock */
				lock =
				    FE_STV0910_GetDemodLockCold(pParams,
								demodTimeout,
								Demod);
			else if (pParams->Demod2SearchAlgo == FE_SAT_WARM_START)
				/* Case warm start wait for demod lock */
				lock =
				    FE_STV0910_GetDemodLock(pParams->hDemod,
							    Demod,
							    demodTimeout);

			if (lock == TRUE)
				/* Read signal caracteristics and check the
				 * lock range */
				signalType =
				    FE_STV0910_GetSignalParams(pParams, Demod,
							       Satellite_Scan);
		}

		break;
	}			/* end of     switch demod */

	if ((lock == TRUE) && (signalType == FE_SAT_RANGEOK)) {
		/*The tracking optimization and the FEC lock check are perfomed
		 * only if: demod is locked and signal type is RANGEOK i.e a TP
		 * foun within the given search range
		 */

		switch (Demod) {
		case FE_SAT_DEMOD_1:
		default:
			FE_Sat_TunerSetBandwidth(pParams->hTuner1,
						 pParams->Tuner1BW * 10 / 15);
			/* Optimization setting for tracking */
			FE_STV0910_TrackingOptimization(pParams, Demod);

			if (pParams->Demod1Results.Standard ==
			    FE_SAT_DVBS2_STANDARD) {
				if ((pParams->Demod1Results.Pilots == 0)
				    &&
				    ((pParams->Demod1Results.ModCode ==
				      FE_SAT_8PSK_23)
				     || (pParams->Demod1Results.ModCode ==
					 FE_SAT_8PSK_35))) {
					FE_STV0910_8PSKCarrierAdaptation
					    (pParams, Demod);
				}
			}
			break;

		case FE_SAT_DEMOD_2:
			FE_Sat_TunerSetBandwidth(pParams->hTuner2,
						 pParams->Tuner2BW * 10 / 15);
			/* Optimization setting for tracking */
			FE_STV0910_TrackingOptimization(pParams, Demod);
			if (pParams->Demod2Results.Standard ==
			    FE_SAT_DVBS2_STANDARD) {
				if ((pParams->Demod2Results.Pilots == 0)
				    &&
				    ((pParams->Demod2Results.ModCode ==
				      FE_SAT_8PSK_23)
				     || (pParams->Demod2Results.ModCode ==
					 FE_SAT_8PSK_35))) {
					FE_STV0910_8PSKCarrierAdaptation
					    (pParams, Demod);
				}
			}
			break;
		}

		/*Release stream merger reset */
		ChipSetField(pParams->hDemod, streamMergerField, 0);
		ChipWaitOrAbort(pParams->hDemod, 3);
		/* Stream merger reset */
		ChipSetField(pParams->hDemod, streamMergerField, 1);
		/* Release stream merger reset */
		ChipSetField(pParams->hDemod, streamMergerField, 0);

		if (FE_STV0910_WaitForLock(pParams->hDemod, Demod, fecTimeout,
					fecTimeout, Satellite_Scan) == TRUE) {
			lock = TRUE;
			switch (Demod) {
			case FE_SAT_DEMOD_1:
			default:
				pParams->Demod1Results.Locked = TRUE;

				/* FE_Sat_TunerSetBandwidth(pParams->hTuner1,
				 * pParams->Tuner1BW*10/15); */

				if (pParams->Demod1Results.Standard ==
				    FE_SAT_DVBS2_STANDARD) {
					/*reset DVBS2 packet delinator error
					 * counter */
					ChipSetFieldImage(pParams->hDemod,
					       FSTV0910_P1_RESET_UPKO_COUNT, 1);
					ChipSetRegisters(pParams->hDemod,
						 RSTV0910_P1_PDELCTRL2, 1);

					/*reset DVBS2 packet delinator error
					 * counter */
					ChipSetFieldImage(pParams->hDemod,
					       FSTV0910_P1_RESET_UPKO_COUNT, 0);
					ChipSetRegisters(pParams->hDemod,
						 RSTV0910_P1_PDELCTRL2, 1);

					/* reset the error counter to  DVBS2
					 * packet error rate */
					ChipSetOneRegister(pParams->hDemod,
						   RSTV0910_P1_ERRCTRL1, 0x67);
				} else {
					/* reset the viterbi bit error rate */
					ChipSetOneRegister(pParams->hDemod,
						   RSTV0910_P1_ERRCTRL1, 0x75);
				}
				/*Reset the Total packet counter */
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0910_P1_FBERCPT4, 0);
				/*Reset the packet Error counter2(and Set it to
				 * infinit error count mode) */
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0910_P1_ERRCTRL2, 0xc1);
				break;

			case FE_SAT_DEMOD_2:
				pParams->Demod2Results.Locked = TRUE;
				/*FE_Sat_TunerSetBandwidth(pParams->hTuner2,
				 * pParams->Tuner2BW*10/15); */

				if (pParams->Demod2Results.Standard ==
				    FE_SAT_DVBS2_STANDARD) {
					/*reset DVBS2 packet delinator error
					 * counter */
					ChipSetFieldImage(pParams->hDemod,
					       FSTV0910_P2_RESET_UPKO_COUNT, 1);
					ChipSetRegisters(pParams->hDemod,
						 RSTV0910_P2_PDELCTRL2, 1);

					/*reset DVBS2 packet delinator error
					 * counter */
					ChipSetFieldImage(pParams->hDemod,
					       FSTV0910_P2_RESET_UPKO_COUNT, 0);
					ChipSetRegisters(pParams->hDemod,
						RSTV0910_P2_PDELCTRL2, 1);

					/* reset the error counter to  DVBS2
					 * packet error rate */
					ChipSetOneRegister(pParams->hDemod,
						   RSTV0910_P2_ERRCTRL1, 0x67);
				} else {
					/* reset the viterbi bit error rate */
					ChipSetOneRegister(pParams->hDemod,
						  RSTV0910_P2_ERRCTRL1, 0x75);
				}
				/*Reset the Total packet counter */
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0910_P2_FBERCPT4, 0);
				/*Reset the packet Error counter2(and Set it to
				 * infinit error count mode) */
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0910_P2_ERRCTRL2, 0xc1);

				break;

			}
		} else {
			lock = FALSE;
			/*if the demod is locked and not the FEC signal type is
			 * no DATA */
			signalType = FE_SAT_NODATA;
			switch (Demod) {
			case FE_SAT_DEMOD_1:
			default:
				pParams->Demod1Results.Locked = FALSE;
				break;
			case FE_SAT_DEMOD_2:
				pParams->Demod2Results.Locked = FALSE;
				break;
			}
		}
	}

	/* End of if ((lock == TRUE) && (signalType == FE_SAT_RANGEOK)) */
	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		/* VTH for tracking */
		ChipSetFieldImage(pParams->hDemod, FSTV0910_P1_VTH12, 0xD7);
		ChipSetFieldImage(pParams->hDemod, FSTV0910_P1_VTH23, 0x85);
		ChipSetFieldImage(pParams->hDemod, FSTV0910_P1_VTH34, 0x58);
		ChipSetFieldImage(pParams->hDemod, FSTV0910_P1_VTH56, 0x3A);
		ChipSetFieldImage(pParams->hDemod, FSTV0910_P1_VTH67, 0x34);
		ChipSetFieldImage(pParams->hDemod, FSTV0910_P1_VTH78, 0x28);
		ChipSetRegisters(pParams->hDemod, RSTV0910_P1_VTH12, 6);

		/* enable the found puncture rate only */
		puncturerate =
		    FE_STV0910_GetViterbiPunctureRate(pParams->hDemod, Demod);
		if (pParams->Demod1Results.Standard == FE_SAT_DVBS1_STANDARD)
			FE_STV0910_SetViterbiStandard(pParams->hDemod,
						      FE_SAT_SEARCH_DVBS1,
						      puncturerate, Demod);
		else if (pParams->Demod1Results.Standard == FE_SAT_DSS_STANDARD)
			FE_STV0910_SetViterbiStandard(pParams->hDemod,
						      FE_SAT_SEARCH_DSS,
						      puncturerate, Demod);

		if (pParams->Demod1SearchAlgo == FE_SAT_BLIND_SEARCH) {
			/* close ReedSolomon to viterbi feedback after lock */
			ChipSetField(pParams->hDemod, FSTV0910_P1_DIS_RSFLOCK,
				     0);
			/* close Viterbi feedback after lock */
			ChipSetField(pParams->hDemod, FSTV0910_P1_DIS_VITLOCK,
				     0);
		}
		break;
	case FE_SAT_DEMOD_2:
		/* VTH for tracking */
		ChipSetFieldImage(pParams->hDemod, FSTV0910_P2_VTH12, 0xD7);
		ChipSetFieldImage(pParams->hDemod, FSTV0910_P2_VTH23, 0x85);
		ChipSetFieldImage(pParams->hDemod, FSTV0910_P2_VTH34, 0x58);
		ChipSetFieldImage(pParams->hDemod, FSTV0910_P2_VTH56, 0x3A);
		ChipSetFieldImage(pParams->hDemod, FSTV0910_P2_VTH67, 0x34);
		ChipSetFieldImage(pParams->hDemod, FSTV0910_P2_VTH78, 0x28);
		ChipSetRegisters(pParams->hDemod, RSTV0910_P2_VTH12, 6);

		/* enable the found puncture rate only */
		puncturerate =
		    FE_STV0910_GetViterbiPunctureRate(pParams->hDemod, Demod);
		if (pParams->Demod2Results.Standard == FE_SAT_DVBS1_STANDARD)
			FE_STV0910_SetViterbiStandard(pParams->hDemod,
						      FE_SAT_SEARCH_DVBS1,
						      puncturerate, Demod);
		else if (pParams->Demod2Results.Standard == FE_SAT_DSS_STANDARD)
			FE_STV0910_SetViterbiStandard(pParams->hDemod,
						      FE_SAT_SEARCH_DSS,
						      puncturerate, Demod);

		if (pParams->Demod2SearchAlgo == FE_SAT_BLIND_SEARCH) {
			/* close ReedSolomon to viterbi feedback after lock */
			ChipSetField(pParams->hDemod, FSTV0910_P2_DIS_RSFLOCK,
				     0);
			/* close Viterbi feedback after lock */
			ChipSetField(pParams->hDemod, FSTV0910_P2_DIS_VITLOCK,
				     0);
		}
		break;
	}

	return signalType;
}

#ifdef STM_FE_DEFINED_FOR_FUTURE_USE
/*****************************************************
  --FUNCTION	::	FE_STV0910_SetReg
  --ACTION	::	write one register
  --PARAMS IN	::	Handle	==>	Front End Handle
  ::	Reg		==> register Index in the register Map
  ::	Val	==> Val to be writen
  --PARAMS OUT::	NONE.
  --RETURN	::	Error(if any)
  --***************************************************/
/*! \fn  FE_STV0910_Error_t FE_STV0910_SetReg(FE_STV0910_Handle_t Handle, U16
								Reg, U32 Val)
  \brief This public function is used to write into a register.
  */
FE_STV0910_Error_t FE_STV0910_SetReg(FE_STV0910_Handle_t Handle, U16 Reg,
				     U32 Val)
{
	FE_STV0910_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0910_InternalParams_t *pParams =
	    (FE_STV0910_InternalParams_t *) Handle;

	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;

	ChipSetOneRegister(pParams->hDemod, Reg, Val);

	if (pParams->hDemod->Error)
		error = FE_LLA_I2C_ERROR;

	return error;
}

/*****************************************************
  --FUNCTION	::	FE_STV0910_GetReg
  --ACTION	::	read one register
  --PARAMS IN	::	Handle	==>	Front End Handle
  ::	Reg		==> register Index in the register Map
  --PARAMS OUT::	Val		==> Read value.
  --RETURN	::	Error(if any)
  --***************************************************/
/*! \fn FE_STV0910_Error_t FE_STV0910_GetReg(FE_STV0910_Handle_t Handle, U16
								Reg, U32 *Val)
  \brief This public function is used to read a register.
  */
FE_STV0910_Error_t FE_STV0910_GetReg(FE_STV0910_Handle_t Handle, U16 Reg,
				     U32 *Val)
{
	FE_STV0910_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0910_InternalParams_t *pParams =
	    (FE_STV0910_InternalParams_t *) Handle;

	if ((pParams == NULL) || (Val == NULL))
		return FE_LLA_INVALID_HANDLE;

	*Val = ChipGetOneRegister(pParams->hDemod, Reg);
	if (pParams->hDemod->Error)
		error = FE_LLA_I2C_ERROR;

	return error;
}

/*****************************************************
  --FUNCTION	::	FE_STV0910_SetField
  --ACTION	::	write a value to a Field
  --PARAMS IN	::	Handle	==>	Front End Handle
  ::	Reg		==> Field Index in the register Map
  ::	Val	==> Val to be writen
  --PARAMS OUT::	NONE.
  --RETURN	::	Error(if any)
  --***************************************************/
/*! \fn FE_STV0910_Error_t FE_STV0910_SetField(FE_STV0910_Handle_t Handle, U32
								Field, S32 Val)
  \brief This public function is used to write into a field.
  */
FE_STV0910_Error_t FE_STV0910_SetField(FE_STV0910_Handle_t Handle, U32 Field,
				       S32 Val)
{
	FE_STV0910_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0910_InternalParams_t *pParams =
	    (FE_STV0910_InternalParams_t *) Handle;

	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;

	ChipSetField(pParams->hDemod, Field, Val);
	if (pParams->hDemod->Error)
		error = FE_LLA_I2C_ERROR;

	return error;
}

/*****************************************************
  --FUNCTION	::	FE_STV0910_GetField
  --ACTION	::	Read A Field
  --PARAMS IN	::	Handle	==>	Front End Handle
  ::	Field	==> Field Index in the register Map
  --PARAMS OUT::	Val		==> Read value.
  --RETURN	::	Error(if any)
  --***************************************************/
/*! \fn FE_STV0910_Error_t FE_STV0910_GetField(FE_STV0910_Handle_t Handle, U32
								Field, S32 *Val)
  \brief This public function is used to read from a field.
  */
FE_STV0910_Error_t FE_STV0910_GetField(FE_STV0910_Handle_t Handle, U32 Field,
				       S32 *Val)
{
	FE_STV0910_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0910_InternalParams_t *pParams =
	    (FE_STV0910_InternalParams_t *) Handle;

	if ((pParams == NULL) || (Val == NULL))
		return FE_LLA_INVALID_HANDLE;

	*Val = ChipGetField(pParams->hDemod, Field);
	if (pParams->hDemod->Error)
		error = FE_LLA_I2C_ERROR;

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
  --FUNCTION	::	FE_STV0910_GetRevision
  --ACTION	::	Return current LLA version
  --PARAMS IN	::	NONE
  --PARAMS OUT::	NONE
  --RETURN	::	Revision ==> Text string containing LLA version
  --***************************************************/
/*! \fn ST_Revision_t FE_STV0910_GetRevision(void)
  \brief This public function return s the current revision of the
  LLA(characters chain)
  */
ST_Revision_t FE_STV0910_GetRevision(void)
{
	return RevisionSTV0910;
}

/*****************************************************
  --FUNCTION	::	FE_STV0910_Init
  --ACTION	::	Initialisation of the STV0910 chip
  --PARAMS IN	::	pInit	==>	Front End Init parameters
  --PARAMS OUT::	NONE
  --RETURN	::	Handle to STV0910
  --***************************************************/
/*! \fn FE_STV0910_Error_t FE_STV0910_Init(FE_STV0910_InitParams_t *pInit,
						FE_STV0910_Handle_t *Handle)
  \brief This function is used to initialize the demod and tuners.
  */
FE_STV0910_Error_t FE_STV0910_Init(FE_STV0910_InitParams_t *pInit,
				   FE_STV0910_Handle_t *Handle)
{
	FE_STV0910_InternalParams_t *pParams = NULL;

	/* Demodulator chip initialisation parameters */
	Demod_InitParams_t DemodInitParams;
	SAT_TUNER_InitParams_t TunerInitParams;
	SAT_VGLNA_InitParams_t VGLNAInitParams;	/* VGLNA init params */

	FE_STV0910_Error_t error = FE_LLA_NO_ERROR;

	TUNER_Error_t tuner1Error = TUNER_NO_ERR, tuner2Error = TUNER_NO_ERR;
	STCHIP_Error_t demodError = CHIPERR_NO_ERROR;

	/* Internal params structure allocation */
#ifdef HOST_PC
	STCHIP_Info_t DemodChip;
	pParams = calloc(1, sizeof(FE_STV0910_InternalParams_t));
	(*Handle) = (FE_STV0910_Handle_t) pParams;
#endif

#ifdef CHIP_STAPI
	pParams = (FE_STV0910_InternalParams_t *) (*Handle);
#endif

	if (pParams == NULL || pParams->hDemod == NULL)
		return FE_LLA_INVALID_HANDLE;

	/* Chip initialisation */
#if defined(HOST_PC) && !defined(NO_GUI)
	/*Copy GUI pointers */
	pParams->hDemod = DEMOD;
	pParams->hTuner1 = TUNER_A;
	pParams->hTuner2 = TUNER_B;
	pParams->Quartz = EXTCLK;
	pParams->hVGLNA1 = ChipGetHandleFromName("STVVGLNA1");
	pParams->hVGLNA2 = ChipGetHandleFromName("STVVGLNA2");

#else
	/* Demodulator(STV0910) Init */
#ifdef CHIP_STAPI
	DemodInitParams.Chip = (pParams->hDemod);
#else
	DemodInitParams.Chip = &DemodChip;
#endif

	DemodInitParams.NbDefVal = STV0910_NBREGS;
	DemodInitParams.Chip->RepeaterHost = NULL;
	DemodInitParams.Chip->RepeaterFn = NULL;
	DemodInitParams.Chip->Repeater = FALSE;
	DemodInitParams.Chip->I2cAddr = pInit->DemodI2cAddr;
	strcpy((char *)DemodInitParams.Chip->Name, pInit->DemodName);

	demodError = STV0910_Init(&DemodInitParams, &pParams->hDemod);

	if (pInit->TunerModel != TUNER_NULL) {
		/*Tuner 1 Init */
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
		TunerInitParams.RepeaterFn = FE_STV0910_RepeaterFn;
		TunerInitParams.DemodModel = DEMOD_STV0910;
		TunerInitParams.OutputClockDivider = pInit->TunerOutClkDivider;
#ifdef DLL
		TunerInitParams.Lna_agc_mode = pInit->TunerLnaAgcMode;
		TunerInitParams.Lna_agc_ref = pInit->TunerLnaAgcRef;
#endif
		TunerInitParams.RF_Source = pInit->TunerRF_Source;
		TunerInitParams.InputSelect = pInit->TunerInputSelect;

		tuner1Error =
		    FE_Sat_TunerInit(&TunerInitParams, &(pParams->hTuner1));
		/*Init the VGLNA1 */
		VGLNAInitParams.I2cAddress = pInit->VGLNA1I2cAddr;
		VGLNAInitParams.Name = "VGLNA1";
		strcpy((char *)VGLNAInitParams.Name, "VGLNA1");
		VGLNAInitParams.Reference = pInit->TunerRefClock;
		VGLNAInitParams.DemodModel = DEMOD_STV0910;
		VGLNAInitParams.RepeaterHost = pParams->hDemod;
		VGLNAInitParams.RepeaterFn = FE_STV0910_RepeaterFn;
		VGLNAInitParams.Repeater = TRUE;

		if (STVVGLNA_Init(&VGLNAInitParams, &(pParams->hVGLNA1)) ==
				CHIPERR_I2C_NO_ACK) {
			STVVGLNA_Term(pParams->hVGLNA1);
			pParams->hVGLNA1 = NULL;
		}

		FE_Sat_TunerSetLNAInput(pParams->hTuner1,
				pInit->TunerRF_Source);
	}

	if (pInit->Tuner2Model != TUNER_NULL) {
		/*Tuner 2 Init */
		TunerInitParams.Model = pInit->Tuner2Model;
		TunerInitParams.TunerName = pInit->Tuner2Name;
		strcpy((char *)TunerInitParams.TunerName,
		       pInit->Tuner2Name);
		TunerInitParams.TunerI2cAddress = pInit->Tuner2_I2cAddr;
		TunerInitParams.Reference = pInit->Tuner2RefClock;
		TunerInitParams.IF = 0;
		if (pInit->Tuner2IQ_Inversion == FE_SAT_IQ_NORMAL)
			TunerInitParams.IQ_Wiring = TUNER_IQ_NORMAL;
		else
			TunerInitParams.IQ_Wiring = TUNER_IQ_INVERT;

		TunerInitParams.RepeaterHost = pParams->hDemod;

		if ((pInit->Tuner2Model == TUNER_STV6120_Tuner1)
		    || (pInit->Tuner2Model == TUNER_STV6120_Tuner2))
			TunerInitParams.RepeaterFn = FE_STV0910_RepeaterFn;
		else
			TunerInitParams.RepeaterFn = FE_STV0910_Repeater2Fn;

		TunerInitParams.DemodModel = DEMOD_STV0910;
		TunerInitParams.OutputClockDivider = pInit->Tuner2OutClkDivider;
#ifdef DLL
		TunerInitParams.Lna_agc_mode = pInit->Tuner2LnaAgcMode;
		TunerInitParams.Lna_agc_ref = pInit->Tuner2LnaAgcRef;
#endif
		TunerInitParams.RF_Source = pInit->Tuner2RF_Source;
		TunerInitParams.InputSelect = pInit->Tuner2InputSelect;

		tuner2Error =
		    FE_Sat_TunerInit(&TunerInitParams, &(pParams->hTuner2));

		/*Init the VGLNA2 */
		VGLNAInitParams.I2cAddress = pInit->VGLNA2I2cAddr;
		VGLNAInitParams.Name = "VGLNA2";
		strcpy((char *)VGLNAInitParams.Name, "VGLNA2");
		VGLNAInitParams.Reference = pInit->TunerRefClock;
		VGLNAInitParams.DemodModel = DEMOD_STV0910;
		VGLNAInitParams.RepeaterHost = pParams->hDemod;
		VGLNAInitParams.RepeaterFn = FE_STV0910_RepeaterFn;
		VGLNAInitParams.Repeater = TRUE;

		if (STVVGLNA_Init(&VGLNAInitParams, &(pParams->hVGLNA2)) ==
				CHIPERR_I2C_NO_ACK) {
			STVVGLNA_Term(pParams->hVGLNA2);
			pParams->hVGLNA2 = NULL;
		}

		FE_Sat_TunerSetLNAInput(pParams->hTuner2,
				pInit->Tuner2RF_Source);

	}
	/*Check the demod error first */
	if (demodError == CHIPERR_NO_ERROR) {
		/*If no Error on the demod then check the Tuners */
		if ((tuner1Error == TUNER_NO_ERR)
		    && (tuner2Error == TUNER_NO_ERR))
			/*if no error on the tuner1 and 2 */
			error = FE_LLA_NO_ERROR;
		else if ((tuner1Error == TUNER_INVALID_HANDLE)
			 || (tuner2Error == TUNER_INVALID_HANDLE))
			/*if tuner1 or 2 type error */
			error = FE_LLA_INVALID_HANDLE;
		else if ((tuner1Error == TUNER_TYPE_ERR)
			 || (tuner2Error == TUNER_TYPE_ERR))
			/*if tuner1 or 2 NULL Handle error =
			 * FE_LLA_INVALIDE_HANDLE */
			error = FE_LLA_BAD_PARAMETER;
		else if ((tuner1Error != TUNER_NO_ERR)
			 || (tuner2Error != TUNER_NO_ERR))
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

	/*Demod type single/Dual */
	pParams->DemodType = pInit->DemodType;

	/*If the single mode is selected then init the device to single mode by
	 * default on path1 */
	/* the function "FE_STV0910_SetDVBS2_Single" could be used then to
	 * switch from single on path1 to single path2 or to dual any time
	 * during the diffrent zapping
	 */
	FE_STV0910_SetDVBS2_Single(pParams, pParams->DemodType, FE_SAT_DEMOD_1);

	/*Read IC cut ID */
	pParams->hDemod->ChipID =
			ChipGetOneRegister(pParams->hDemod, RSTV0910_MID);

	/*Tuner parameters */
	pParams->Tuner1Type = pInit->TunerHWControlType;
	pParams->Tuner2Type = pInit->Tuner2HWControlType;
	pParams->Tuner1Global_IQ_Inv = pInit->TunerIQ_Inversion;
	pParams->Tuner2Global_IQ_Inv = pInit->Tuner2IQ_Inversion;

	pParams->RollOff = pInit->RollOff;
	/*settings for STTUNER or non - Gui applications or Auto test */
#if defined(CHIP_STAPI) || defined(NO_GUI)
	/*Ext clock in Hz */
	pParams->Quartz = pInit->DemodRefClock;

	ChipSetField(pParams->hDemod, FSTV0910_P1_ROLLOFF_CONTROL,
			pInit->RollOff);
	ChipSetField(pParams->hDemod, FSTV0910_P2_ROLLOFF_CONTROL,
			pInit->RollOff);

	switch (pInit->TunerHWControlType) {
	case FE_SAT_SW_TUNER:
	default:
		if (pParams->hTuner1) {
			/* Set tuner reference clock */
			FE_Sat_TunerSetReferenceFreq(pParams->hTuner1,
					pInit->TunerRefClock);

			/*Set tuner base band gain */
			FE_Sat_TunerSetGain(pParams->hTuner1,
				    pInit->TunerBasebandGain);
		}
		/*Set the ADC's range according to the tuner Model */
		break;

	}

	switch (pInit->Tuner2HWControlType) {
	case FE_SAT_SW_TUNER:
	default:
		if (pParams->hTuner2) {
			/* Set tuner2 reference clock */
			FE_Sat_TunerSetReferenceFreq(pParams->hTuner2,
					pInit->Tuner2RefClock);

			/*Set tuner base band gain */
			FE_Sat_TunerSetGain(pParams->hTuner2,
					pInit->Tuner2BasebandGain);
		}
		break;

	}

	/*IQSWAP setting must be after FE_STV0910_P1_SetTunerType */
	ChipSetField(pParams->hDemod, FSTV0910_P1_TUN_IQSWAP,
			pInit->TunerIQ_Inversion);
	/*IQSWAP setting must be after FE_STV0910_P1_SetTunerType */
	ChipSetField(pParams->hDemod, FSTV0910_P2_TUN_IQSWAP,
			pInit->Tuner2IQ_Inversion);

	/*Set the Mclk value to 135MHz */
	FE_STV0910_SetMclk((FE_STV0910_Handle_t) pParams, 135000000,
			pParams->Quartz);
	/*wait for PLL lock */
	ChipWaitOrAbort(pParams->hDemod, 3);
	/* switch the 910 to the PLL */
	ChipSetOneRegister(pParams->hDemod, RSTV0910_SYNTCTRL, 0x22);
#endif

	/*Read the cuurent Mclk */
	pParams->MasterClock = FE_STV0910_GetMclkFreq(pParams->hDemod,
			pParams->Quartz);

	/*Check the error at the end of the init */
	if (pParams->hDemod->Error)
		error = FE_LLA_I2C_ERROR;
	else if (pParams->hTuner1 != NULL) {
		if (pParams->hTuner1->Error)
			error = FE_LLA_I2C_ERROR;
	} else if (pParams->hTuner2 != NULL) {
		if (pParams->hTuner2->Error)
			error = FE_LLA_I2C_ERROR;
	}

	return error;
}

/*! \fn FE_STV0910_Error_t FE_STV0910_SetTSoutput(FE_STV0910_Handle_t Handle,
  FE_STV0910_TSConfig_t *Path1pTSConfig,
  FE_STV0910_TSConfig_t *Path2pTSConfig);
  \brief This public function is used to set the transport stream mode(serial,
  parallel, DVBCI)
  */
FE_STV0910_Error_t FE_STV0910_SetTSoutput(FE_STV0910_Handle_t Handle,
					  FE_STV0910_TSConfig_t *
					  Path1pTSConfig,
					  FE_STV0910_TSConfig_t *
					  Path2pTSConfig)
{

	FE_STV0910_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0910_InternalParams_t *pParams = NULL;

	pParams = (FE_STV0910_InternalParams_t *) Handle;
	if (pParams == NULL)
		return error;

	if (pParams->hDemod->Error)
		return FE_LLA_I2C_ERROR;

	if (Path1pTSConfig->TSOutput == Path2pTSConfig->TSOutput)
		/*error path1 & path2 cannot use the same TS */
		error = FE_LLA_BAD_PARAMETER;

	if (error != FE_LLA_NO_ERROR)
		return error;

	switch (Path1pTSConfig->TSMode) {
	case FE_TS_PARALLEL_PUNCT_CLOCK:
		/* D7:msb, D0:lsb */
		ChipSetField(pParams->hDemod, FSTV0910_P1_TSFIFO_PERMDATA, 0);
		ChipSetField(pParams->hDemod, FSTV0910_TS1_OUTPAR_HZ, 0x00);
		ChipSetField(pParams->hDemod, FSTV0910_P1_TSFIFO_SERIAL, 0x00);
		ChipSetField(pParams->hDemod, FSTV0910_P1_TSFIFO_DVBCI, 0x00);
		break;

	case FE_TS_DVBCI_CLOCK:
		/* D7:msb, D0:lsb */
		ChipSetField(pParams->hDemod, FSTV0910_P1_TSFIFO_PERMDATA, 0);
		ChipSetField(pParams->hDemod, FSTV0910_TS1_OUTPAR_HZ, 0x00);
		ChipSetField(pParams->hDemod, FSTV0910_P1_TSFIFO_SERIAL, 0x00);
		ChipSetField(pParams->hDemod, FSTV0910_P1_TSFIFO_DVBCI, 0x01);

		break;

	case FE_TS_SERIAL_PUNCT_CLOCK:
		/* D7:msb, D0:lsb */
		ChipSetField(pParams->hDemod, FSTV0910_P1_TSFIFO_PERMDATA, 0);
		ChipSetField(pParams->hDemod, FSTV0910_TS1_OUTSER_HZ, 0x00);
		ChipSetField(pParams->hDemod, FSTV0910_TS1_OUTPAR_HZ, 0x01);
		ChipSetField(pParams->hDemod, FSTV0910_P1_TSFIFO_SERIAL, 0x01);
		ChipSetField(pParams->hDemod, FSTV0910_P1_TSFIFO_DVBCI, 0x00);
		break;

	case FE_TS_SERIAL_CONT_CLOCK:
		/* D7:msb, D0:lsb */
		ChipSetField(pParams->hDemod, FSTV0910_P1_TSFIFO_PERMDATA, 0);
		ChipSetField(pParams->hDemod, FSTV0910_TS1_OUTSER_HZ, 0x00);
		ChipSetField(pParams->hDemod, FSTV0910_TS1_OUTPAR_HZ, 0x01);
		ChipSetField(pParams->hDemod, FSTV0910_P1_TSFIFO_SERIAL, 0x01);
		ChipSetField(pParams->hDemod, FSTV0910_P1_TSFIFO_DVBCI, 0x01);
		break;

	default:
		break;
	}

	switch (Path2pTSConfig->TSMode) {
	case FE_TS_PARALLEL_PUNCT_CLOCK:
		/* D7:msb, D0:lsb */
		ChipSetField(pParams->hDemod, FSTV0910_P2_TSFIFO_PERMDATA, 0);
		ChipSetField(pParams->hDemod, FSTV0910_TS2_OUTPAR_HZ, 0x00);
		/* parallel mode        */
		ChipSetField(pParams->hDemod, FSTV0910_P2_TSFIFO_SERIAL, 0x00);
		ChipSetField(pParams->hDemod, FSTV0910_P2_TSFIFO_DVBCI, 0x00);
		break;

	case FE_TS_DVBCI_CLOCK:
		/* D7:msb, D0:lsb */
		ChipSetField(pParams->hDemod, FSTV0910_P2_TSFIFO_PERMDATA, 0);
		ChipSetField(pParams->hDemod, FSTV0910_TS2_OUTPAR_HZ, 0x00);
		/* parallel mode        */
		ChipSetField(pParams->hDemod, FSTV0910_P2_TSFIFO_SERIAL, 0x00);
		ChipSetField(pParams->hDemod, FSTV0910_P2_TSFIFO_DVBCI, 0x01);
		break;

	case FE_TS_SERIAL_PUNCT_CLOCK:
		/* D7:msb, D0:lsb */
		ChipSetField(pParams->hDemod, FSTV0910_P2_TSFIFO_PERMDATA, 0);
		ChipSetField(pParams->hDemod, FSTV0910_TS2_OUTSER_HZ, 0x00);
		ChipSetField(pParams->hDemod, FSTV0910_TS2_OUTPAR_HZ, 0x01);
		/* serial mode  */
		ChipSetField(pParams->hDemod, FSTV0910_P2_TSFIFO_SERIAL, 0x01);
		ChipSetField(pParams->hDemod, FSTV0910_P2_TSFIFO_DVBCI, 0x00);
		break;

	case FE_TS_SERIAL_CONT_CLOCK:
		/* D7:msb, D0:lsb */
		ChipSetField(pParams->hDemod, FSTV0910_P2_TSFIFO_PERMDATA, 0);
		ChipSetField(pParams->hDemod, FSTV0910_TS2_OUTSER_HZ, 0x00);
		ChipSetField(pParams->hDemod, FSTV0910_TS2_OUTPAR_HZ, 0x01);
		/* serial mode  */
		ChipSetField(pParams->hDemod, FSTV0910_P2_TSFIFO_SERIAL, 0x01);
		ChipSetField(pParams->hDemod, FSTV0910_P2_TSFIFO_DVBCI, 0x01);
		break;

	default:
		break;
	}
	/*Reset stream merger */
	ChipSetField(pParams->hDemod, FSTV0910_P2_RST_HWARE, 1);
	ChipSetField(pParams->hDemod, FSTV0910_P2_RST_HWARE, 0);

	/*Reset stream merger */
	ChipSetField(pParams->hDemod, FSTV0910_P1_RST_HWARE, 1);
	ChipSetField(pParams->hDemod, FSTV0910_P1_RST_HWARE, 0);

	return error;
}

/*****************************************************
  --FUNCTION	::	FE_STV0910_SetTSConfig
  --ACTION	::	TS configuration
  --PARAMS IN	::	Handle		==>	Front End Handle
  ::	Path1pTSConfig	==> path1 TS config parameters
  ::	Path2pTSConfig	==> path2 TS config parameters
  --PARAMS OUT::	Path1TSSpeed_Hz	==> path1 Current TS speed in Hz.
  Path2TSSpeed_Hz	==> path2 Current TS speed in Hz.
  --RETURN	::	Error(if any)
  --***************************************************/
/*! \fn FE_STV0910_Error_t FE_STV0910_SetTSConfig(FE_STV0910_Handle_t Handle,
  FE_STV0910_TSConfig_t *Path1pTSConfig,
  FE_STV0910_TSConfig_t *Path2pTSConfig,
  U32 *Path1TSSpeed_Hz,
  U32 *Path2TSSpeed_Hz
  )
  \brief This public function is used to configure the TS clock polarity, speed
  */
FE_STV0910_Error_t FE_STV0910_SetTSConfig(FE_STV0910_Handle_t Handle,
					  FE_STV0910_TSConfig_t *
					  Path1pTSConfig,
					  FE_STV0910_TSConfig_t *
					  Path2pTSConfig, U32 *Path1TSSpeed_Hz,
					  U32 *Path2TSSpeed_Hz)
{
	FE_STV0910_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0910_InternalParams_t *pParams = NULL;

	U32 tsspeed;

	pParams = (FE_STV0910_InternalParams_t *) Handle;
	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;

	if (pParams->hDemod->Error)
		return FE_LLA_I2C_ERROR;

	ChipSetOneRegister(pParams->hDemod, RSTV0910_TSGENERAL, 0);

	/* enable/Disable SYNC byte */
	if (Path1pTSConfig->TSSyncByteEnable == FE_TS_SYNCBYTE_OFF)
		ChipSetField(pParams->hDemod, FSTV0910_P1_TSDEL_SYNCBYTE, 1);
	else
		ChipSetField(pParams->hDemod, FSTV0910_P1_TSDEL_SYNCBYTE, 0);

	/*DVBS1 Data parity bytes enabling/disabling */
	if (Path1pTSConfig->TSParityBytes == FE_TS_PARITYBYTES_ON)
		ChipSetField(pParams->hDemod, FSTV0910_P1_TSINSDEL_RSPARITY, 1);
	else
		ChipSetField(pParams->hDemod, FSTV0910_P1_TSINSDEL_RSPARITY, 0);

	/* enable/Disable SYNC byte */
	if (Path2pTSConfig->TSSyncByteEnable == FE_TS_SYNCBYTE_OFF)
		ChipSetField(pParams->hDemod, FSTV0910_P2_TSDEL_SYNCBYTE, 1);
	else
		ChipSetField(pParams->hDemod, FSTV0910_P2_TSDEL_SYNCBYTE, 0);

	/*DVBS1 Data parity bytes enabling/disabling */
	if (Path2pTSConfig->TSParityBytes == FE_TS_PARITYBYTES_ON)
		ChipSetField(pParams->hDemod, FSTV0910_P2_TSINSDEL_RSPARITY, 1);
	else
		ChipSetField(pParams->hDemod, FSTV0910_P2_TSINSDEL_RSPARITY, 0);

	/*TS clock Polarity setting: rising edge/falling edge */
	if (Path1pTSConfig->TSClockPolarity == FE_TS_RISINGEDGE_CLOCK)
		ChipSetField(pParams->hDemod, FSTV0910_TS1_CLOCKOUT_XOR, 0);
	else
		ChipSetField(pParams->hDemod, FSTV0910_TS1_CLOCKOUT_XOR, 1);

	/*TS clock Polarity setting: rising edge/falling edge */
	if (Path2pTSConfig->TSClockPolarity == FE_TS_RISINGEDGE_CLOCK)
		ChipSetField(pParams->hDemod, FSTV0910_TS2_CLOCKOUT_XOR, 0);
	else
		ChipSetField(pParams->hDemod, FSTV0910_TS2_CLOCKOUT_XOR, 1);

	if (Path1pTSConfig->TSSpeedControl == FE_TS_MANUAL_SPEED) {
		/*path 1 TS speed setting */
		ChipSetField(pParams->hDemod, FSTV0910_P1_TSFIFO_MANSPEED, 3);
		switch (Path1pTSConfig->TSMode) {
		case FE_TS_PARALLEL_PUNCT_CLOCK:
		case FE_TS_DVBCI_CLOCK:
			/*Formulat :
			   TS_Speed_Hz = 4 * Msterclock_Hz / TSSPEED_REG */
			tsspeed = (4 * pParams->MasterClock) /
				Path1pTSConfig->TSClockRate;
			if (tsspeed <= 16)
				/*in parallel clock the TS speed is limited <
				 * MasterClock/4 */
				tsspeed = 16;

			ChipSetOneRegister(pParams->hDemod,
					RSTV0910_P1_TSSPEED, tsspeed);

			break;

		case FE_TS_SERIAL_PUNCT_CLOCK:
		case FE_TS_SERIAL_CONT_CLOCK:
		case FE_TS_OUTPUTMODE_DEFAULT:
		default:
			/*Formulat :
			   TS_Speed_Hz = 32 * Msterclock_Hz / TSSPEED_REG */
			tsspeed = (16 * pParams->MasterClock) /
				(Path1pTSConfig->TSClockRate / 2);
			if (tsspeed <= 32)
				/*in serial clock the TS speed is limited <=
				 * MasterClock */
				tsspeed = 32;

			ChipSetOneRegister(pParams->hDemod,
					RSTV0910_P1_TSSPEED, tsspeed);
			break;
		}

	}
	if (Path1pTSConfig->TSMode == FE_TS_DVBCI_CLOCK) {
		ChipSetField(pParams->hDemod, FSTV0910_P1_TSFIFO_MANSPEED, 1);
		/*Formulat :
		   TS_Speed_Hz = 32 * Msterclock_Hz / TSSPEED_REG */
		/*if DVBCI set TS clock to 9MHz */
		tsspeed = (16 * pParams->MasterClock) / 9000000;
		if (tsspeed <= 32)
			/*in serial clock the TS speed is limited <=
			 * MasterClock */
			tsspeed = 32;

		ChipSetOneRegister(pParams->hDemod, RSTV0910_P1_TSSPEED,
				tsspeed);

	} else
		ChipSetField(pParams->hDemod, FSTV0910_P1_TSFIFO_MANSPEED, 0);

	if (Path2pTSConfig->TSSpeedControl == FE_TS_MANUAL_SPEED) {
		/*path 2 TS speed setting */
		ChipSetField(pParams->hDemod, FSTV0910_P2_TSFIFO_MANSPEED, 3);
		switch (Path2pTSConfig->TSMode) {
		case FE_TS_PARALLEL_PUNCT_CLOCK:
		case FE_TS_DVBCI_CLOCK:
			/*Formulat :
			   TS_Speed_Hz = 4 * Msterclock_Hz / TSSPEED_REG */
			tsspeed = (4 * pParams->MasterClock) /
				Path2pTSConfig->TSClockRate;
			if (tsspeed <= 16)
				/*in parallel clock the TS speed is limited <
				 * MasterClock/4 */
				tsspeed = 16;

			ChipSetOneRegister(pParams->hDemod,
					RSTV0910_P2_TSSPEED, tsspeed);

			break;

		case FE_TS_SERIAL_PUNCT_CLOCK:
		case FE_TS_SERIAL_CONT_CLOCK:
		case FE_TS_OUTPUTMODE_DEFAULT:
		default:

			/*Formulat :
			   TS_Speed_Hz = 32 * Msterclock_Hz / TSSPEED_REG */
			tsspeed = (16 * pParams->MasterClock) /
				(Path2pTSConfig->TSClockRate / 2);
			if (tsspeed <= 32)
				/*in serial clock the TS speed is limited <=
				 * MasterClock */
				tsspeed = 32;

			ChipSetOneRegister(pParams->hDemod,
					RSTV0910_P2_TSSPEED, tsspeed);
			break;
		}

	}
	if (Path2pTSConfig->TSMode == FE_TS_DVBCI_CLOCK) {
		ChipSetField(pParams->hDemod, FSTV0910_P2_TSFIFO_MANSPEED, 1);
		/*Formulat :
		   TS_Speed_Hz = 32 * Msterclock_Hz / TSSPEED_REG */
		/*if DVBCI set TS clock to 9MHz */
		tsspeed = (16 * pParams->MasterClock) / 9000000;
		if (tsspeed <= 32)
			/*in serial clock the TS speed is limited <=
			 * MasterClock */
			tsspeed = 32;

		ChipSetOneRegister(pParams->hDemod, RSTV0910_P2_TSSPEED,
				tsspeed);

	} else
		ChipSetField(pParams->hDemod, FSTV0910_P2_TSFIFO_MANSPEED, 0);

	/* D7 to D0 pin permutation */
	switch (Path1pTSConfig->TSMode) {
	case FE_TS_SERIAL_PUNCT_CLOCK:
	case FE_TS_SERIAL_CONT_CLOCK:
	case FE_TS_OUTPUTMODE_DEFAULT:
	default:
		/* Serial o/p on D0. WARNING: Swap Path2 too! */
		if (Path1pTSConfig->TSSwap == FE_TS_SWAP_ON)
			ChipSetField(pParams->hDemod, FSTV0910_TS_SERDATA0, 1);
		else	/* Serial output on D7 */
			ChipSetField(pParams->hDemod, FSTV0910_TS_SERDATA0, 0);
		break;

	case FE_TS_PARALLEL_PUNCT_CLOCK:
	case FE_TS_DVBCI_CLOCK:
		if (Path1pTSConfig->TSSwap == FE_TS_SWAP_ON)
			ChipSetField(pParams->hDemod,
					FSTV0910_P1_TSFIFO_PERMDATA, 1);
		else
			ChipSetField(pParams->hDemod,
					FSTV0910_P1_TSFIFO_PERMDATA, 0);
		break;
	}

	/* D7 to D0 pin permutation */
	switch (Path2pTSConfig->TSMode) {
	case FE_TS_SERIAL_PUNCT_CLOCK:
	case FE_TS_SERIAL_CONT_CLOCK:
	case FE_TS_OUTPUTMODE_DEFAULT:
	default:
		/* Serial o/p on D0. WARNING: Swap Path1 too! */
		if (Path2pTSConfig->TSSwap == FE_TS_SWAP_ON)
			ChipSetField(pParams->hDemod, FSTV0910_TS_SERDATA0, 1);
		else	/* Serial output on D7 */
			ChipSetField(pParams->hDemod, FSTV0910_TS_SERDATA0, 0);
		break;

	case FE_TS_PARALLEL_PUNCT_CLOCK:
	case FE_TS_DVBCI_CLOCK:
		if (Path2pTSConfig->TSSwap == FE_TS_SWAP_ON)
			ChipSetField(pParams->hDemod,
					FSTV0910_P2_TSFIFO_PERMDATA, 0);
		else
			ChipSetField(pParams->hDemod,
					FSTV0910_P2_TSFIFO_PERMDATA, 0);
		break;
	}

	/*Parallel mode */
	if (ChipGetField(pParams->hDemod, FSTV0910_P1_TSFIFO_SERIAL) == 0)
		*Path1TSSpeed_Hz = (4 * pParams->MasterClock) /
		       ChipGetOneRegister(pParams->hDemod, RSTV0910_P1_TSSPEED);
	else {	/*serial mode */

		*Path1TSSpeed_Hz = (16 * pParams->MasterClock) /
		       ChipGetOneRegister(pParams->hDemod, RSTV0910_P1_TSSPEED);
		(*Path1TSSpeed_Hz) *= 2;
	}

	/*Parallel mode */
	if (ChipGetField(pParams->hDemod, FSTV0910_P2_TSFIFO_SERIAL) == 0)
		*Path2TSSpeed_Hz = (4 * pParams->MasterClock) /
		       ChipGetOneRegister(pParams->hDemod, RSTV0910_P2_TSSPEED);
	else {	/*serial mode */

		*Path2TSSpeed_Hz = (16 * pParams->MasterClock) /
		       ChipGetOneRegister(pParams->hDemod, RSTV0910_P2_TSSPEED);
		(*Path2TSSpeed_Hz) *= 2;
	}

	/*Check the error at the end of the function */
	if (pParams->hDemod->Error)
		error = FE_LLA_I2C_ERROR;

	return error;
}

/*****************************************************
  --FUNCTION	::	FE_STV0910_GetSignalInfo
  --ACTION	::	Return informations on the locked transponder
  --PARAMS IN	::	Handle	==>	Front End Handle
  ::	Demod	==>	Cuurent demod 1 or 2
  --PARAMS OUT::	pInfo	==> Informations(BER, C/N, power ...)
  --RETURN	::	Error(if any)
  --***************************************************/
/*! \fn FE_STV0910_Error_t FE_STV0910_GetSignalInfo(FE_STV0910_Handle_t Handle,
  FE_STV0910_DEMOD_t Demod,
  FE_STV0910_SignalInfo_t	*pInfo)
  \brief This public function returns all info about the demodulated signal
  */
FE_STV0910_Error_t FE_STV0910_GetSignalInfo(FE_STV0910_Handle_t Handle,
					    FE_STV0910_DEMOD_t Demod,
					    FE_STV0910_SignalInfo_t *pInfo,
					    U32 Satellite_Scan)
{
	FE_STV0910_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0910_InternalParams_t *pParams = NULL;
	FE_STV0910_SEARCHSTATE_t demodState;
	S32 symbolRateOffset;

	pParams = (FE_STV0910_InternalParams_t *) Handle;

	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		if ((pParams->hDemod->Error) || (pParams->hTuner1->Error))
			return FE_LLA_I2C_ERROR;

		demodState = ChipGetField(pParams->hDemod,
				FSTV0910_P1_HEADER_MODE);
		switch (demodState) {
		case FE_SAT_SEARCH:
		case FE_SAT_PLH_DETECTED:
		default:
			pInfo->Locked = FALSE;
			break;
		case FE_SAT_DVBS2_FOUND:
			pInfo->Locked =
			    ChipGetField(pParams->hDemod,
					    FSTV0910_P1_LOCK_DEFINITIF) &&
			    ChipGetField(pParams->hDemod,
					    FSTV0910_P1_PKTDELIN_LOCK) &&
			    ChipGetField(pParams->hDemod,
					    FSTV0910_P1_TSFIFO_LINEOK);
			break;

		case FE_SAT_DVBS_FOUND:
			pInfo->Locked =
			    ChipGetField(pParams->hDemod,
					    FSTV0910_P1_LOCK_DEFINITIF) &&
			    ChipGetField(pParams->hDemod,
					    FSTV0910_P1_LOCKEDVIT) &&
			    ChipGetField(pParams->hDemod,
					    FSTV0910_P1_TSFIFO_LINEOK);
			break;
		}

		/* transponder_frequency = tuner +  demod carrier frequency */
		pInfo->Frequency = FE_Sat_TunerGetFrequency(pParams->hTuner1);
		pInfo->Frequency += FE_STV0910_GetCarrierFrequency
			(pParams->hDemod, pParams->MasterClock, Demod)
			/ 1000;

		/* Get symbol rate */
		pInfo->SymbolRate = FE_STV0910_GetSymbolRate(pParams->hDemod,
					pParams->MasterClock, Demod);
		symbolRateOffset = FE_STV0910_TimingGetOffset(pParams->hDemod,
					pInfo->SymbolRate, Demod);
		/* Get timing loop offset */
		pInfo->SymbolRate += symbolRateOffset;

		pInfo->Standard =
			FE_STV0910_GetStandard(pParams->hDemod, Demod);

		pInfo->PunctureRate =
			FE_STV0910_GetViterbiPunctureRate(pParams->hDemod,
					Demod);
		pInfo->ModCode = ChipGetField(pParams->hDemod,
					FSTV0910_P1_DEMOD_MODCOD);
		pInfo->Pilots = ChipGetFieldImage(pParams->hDemod,
					FSTV0910_P1_DEMOD_TYPE) & 0x01;
		pInfo->FrameLength = ((U32)ChipGetFieldImage
				(pParams->hDemod,
				 FSTV0910_P1_DEMOD_TYPE)) >> 1;
		pInfo->RollOff = ChipGetField(pParams->hDemod,
				FSTV0910_P1_ROLLOFF_STATUS);

		if (Satellite_Scan == 0) {
			pInfo->BER = FE_STV0910_GetBer(pParams->hDemod,
					Demod);
			pInfo->Power = FE_STV0910_GetRFLevel(pParams,
					Demod);

			if (pInfo->Standard == FE_SAT_DVBS2_STANDARD)
				pInfo->C_N =
				   FE_STV0910_CarrierGetQuality(pParams->hDemod,
					&FE_STV0910_S2_CN_LookUp, Demod);
			else
				pInfo->C_N =
				   FE_STV0910_CarrierGetQuality(pParams->hDemod,
					&FE_STV0910_S1_CN_LookUp, Demod);
		} else {
			/* no BER, Power and CNR measurement
			 * during scan */
			pInfo->BER = 0;
			pInfo->Power = 0;
			pInfo->C_N = 100;
		}

		if (pInfo->Standard == FE_SAT_DVBS2_STANDARD) {
			pInfo->Spectrum = ChipGetField(pParams->hDemod,
					FSTV0910_P1_SPECINV_DEMOD);

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
			/* reset the error counter to  DVBS2 packet error rate*/
			ChipSetOneRegister(pParams->hDemod,
					RSTV0910_P1_ERRCTRL1, 0x67);
		} else {	/*DVBS1/DSS */

			pInfo->Spectrum = ChipGetField(pParams->hDemod,
						FSTV0910_P1_IQINV);
			pInfo->Modulation = FE_SAT_MOD_QPSK;
		}

		if ((pParams->hDemod->Error) || (pParams->hTuner1->Error))
			error = FE_LLA_I2C_ERROR;

		break;

	case FE_SAT_DEMOD_2:
		if ((pParams->hDemod->Error) || (pParams->hTuner2->Error))
			return FE_LLA_I2C_ERROR;

		demodState = ChipGetField(pParams->hDemod,
				FSTV0910_P2_HEADER_MODE);
		switch (demodState) {
		case FE_SAT_SEARCH:
		case FE_SAT_PLH_DETECTED:
		default:
			pInfo->Locked = FALSE;
			break;
		case FE_SAT_DVBS2_FOUND:
			pInfo->Locked =
			    ChipGetField(pParams->hDemod,
					    FSTV0910_P2_LOCK_DEFINITIF) &&
			    ChipGetField(pParams->hDemod,
					    FSTV0910_P2_PKTDELIN_LOCK) &&
			    ChipGetField(pParams->hDemod,
					    FSTV0910_P2_TSFIFO_LINEOK);
			break;

		case FE_SAT_DVBS_FOUND:
			pInfo->Locked =
			    ChipGetField(pParams->hDemod,
					    FSTV0910_P2_LOCK_DEFINITIF) &&
			    ChipGetField(pParams->hDemod,
					    FSTV0910_P2_LOCKEDVIT) &&
			    ChipGetField(pParams->hDemod,
					    FSTV0910_P2_TSFIFO_LINEOK);
			break;
		}

		/* transponder_frequency = tuner +
		 * demod carrier frequency */
		pInfo->Frequency = FE_Sat_TunerGetFrequency(pParams->hTuner2);
		pInfo->Frequency +=
			FE_STV0910_GetCarrierFrequency(pParams->hDemod,
					pParams->MasterClock, Demod) / 1000;
		/* Get symbol rate */
		pInfo->SymbolRate =
			FE_STV0910_GetSymbolRate(pParams->hDemod,
					pParams->MasterClock, Demod);
		symbolRateOffset =
		    FE_STV0910_TimingGetOffset(pParams->hDemod,
				    pInfo->SymbolRate, Demod);
		/* Get timing loop offset */
		pInfo->SymbolRate += symbolRateOffset;

		pInfo->Standard = FE_STV0910_GetStandard(pParams->hDemod,
				Demod);

		pInfo->PunctureRate =
			FE_STV0910_GetViterbiPunctureRate(pParams->hDemod,
					Demod);
		pInfo->ModCode = ChipGetField(pParams->hDemod,
				FSTV0910_P2_DEMOD_MODCOD);
		pInfo->Pilots =
			ChipGetFieldImage(pParams->hDemod,
					FSTV0910_P2_DEMOD_TYPE) & 0x01;
		pInfo->FrameLength = ((U32)ChipGetFieldImage(pParams->hDemod,
					FSTV0910_P2_DEMOD_TYPE)) >> 1;
		pInfo->RollOff = ChipGetField(pParams->hDemod,
				FSTV0910_P2_ROLLOFF_STATUS);

		if (Satellite_Scan == 0) {
			pInfo->BER = FE_STV0910_GetBer(pParams->hDemod, Demod);
			pInfo->Power = FE_STV0910_GetRFLevel(pParams, Demod);

			if (pInfo->Standard == FE_SAT_DVBS2_STANDARD)
				pInfo->C_N =
				   FE_STV0910_CarrierGetQuality(pParams->hDemod,
					   &FE_STV0910_S2_CN_LookUp, Demod);
			else
				pInfo->C_N =
				   FE_STV0910_CarrierGetQuality(pParams->hDemod,
					   &FE_STV0910_S1_CN_LookUp, Demod);
		} else {
			/* no BER, Power and CNR measurement during scan */
			pInfo->BER = 0;
			pInfo->Power = 0;
			pInfo->C_N = 100;
		}

		if (pInfo->Standard == FE_SAT_DVBS2_STANDARD) {
			pInfo->Spectrum = ChipGetField(pParams->hDemod,
					FSTV0910_P2_SPECINV_DEMOD);

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

			/*reset error counter to PER */
			/* reset the error counter to DVBS2 packet error rate*/
			ChipSetOneRegister(pParams->hDemod,
					RSTV0910_P2_ERRCTRL1, 0x67);
		} else {	/*DVBS1/DSS */

			pInfo->Spectrum = ChipGetField(pParams->hDemod,
					FSTV0910_P2_IQINV);
			pInfo->Modulation = FE_SAT_MOD_QPSK;
		}

		if ((pParams->hDemod->Error) || (pParams->hTuner2->Error))
			error = FE_LLA_I2C_ERROR;

		break;
	}

	return error;
}

/*****************************************************
--FUNCTION	::	FE_STV0910_Tracking
--ACTION	::	Return Tracking informations : lock status, RF level,
			C/N and BER.
--PARAMS IN	::	Handle	==>	Front End Handle
		::	Demod	==>	Cuurent demod 1 or 2
--PARAMS OUT	::	pTrackingInfo==>pointer to FE_Sat_TrackingInfo_t struct.
		::
--RETURN	::	Error(if any)
--***************************************************/
/*! \fn FE_STV0910_Error_t FE_STV0910_Tracking(FE_STV0910_Handle_t Handle,
  FE_STV0910_DEMOD_t Demod,
  FE_Sat_TrackingInfo_t *pTrackingInfo);
  \brief The purpose of this public function is to provide the minimum
  necessary information for tracking and signal monitoring in order to reduce
  the I2C access during this task This function fills a structure
  FE_Sat_TrackingInfo_t with the parameters Lock status, C/N, RF Level BER and
  the current frequency including the LNB drift.
  */
FE_STV0910_Error_t FE_STV0910_Tracking(FE_STV0910_Handle_t Handle,
				       FE_STV0910_DEMOD_t Demod,
				       FE_Sat_TrackingInfo_t *pTrackingInfo)
{
	FE_STV0910_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0910_InternalParams_t *pParams = NULL;
	FE_STV0910_SEARCHSTATE_t demodState;

	pParams = (FE_STV0910_InternalParams_t *) Handle;

	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		if ((pParams->hDemod->Error) || (pParams->hTuner1->Error))
			return FE_LLA_I2C_ERROR;

		pTrackingInfo->Power = FE_STV0910_GetRFLevel(pParams, Demod);
		pTrackingInfo->BER = FE_STV0910_GetBer(pParams->hDemod, Demod);
		demodState = ChipGetField(pParams->hDemod,
				FSTV0910_P1_HEADER_MODE);
		switch (demodState) {
		case FE_SAT_SEARCH:
		case FE_SAT_PLH_DETECTED:
		default:
			pTrackingInfo->Locked = FALSE;
			break;
		case FE_SAT_DVBS2_FOUND:
			pTrackingInfo->Locked =
			    ChipGetField(pParams->hDemod,
					    FSTV0910_P1_LOCK_DEFINITIF) &&
			    ChipGetField(pParams->hDemod,
					    FSTV0910_P1_PKTDELIN_LOCK) &&
			    ChipGetField(pParams->hDemod,
					    FSTV0910_P1_TSFIFO_LINEOK);

			pTrackingInfo->C_N =
				FE_STV0910_CarrierGetQuality(pParams->hDemod,
					&FE_STV0910_S2_CN_LookUp, Demod);
			/*reset The error counter to PER */
			/* reset the error counter to  DVBS2 packet error rate*/
			ChipSetOneRegister(pParams->hDemod,
					RSTV0910_P1_ERRCTRL1, 0x67);

			break;

		case FE_SAT_DVBS_FOUND:
			pTrackingInfo->Locked =
			    ChipGetField(pParams->hDemod,
					    FSTV0910_P1_LOCK_DEFINITIF) &&
			    ChipGetField(pParams->hDemod,
					    FSTV0910_P1_LOCKEDVIT) &&
			    ChipGetField(pParams->hDemod,
					    FSTV0910_P1_TSFIFO_LINEOK);

			pTrackingInfo->C_N =
				FE_STV0910_CarrierGetQuality(pParams->hDemod,
					&FE_STV0910_S1_CN_LookUp, Demod);
			break;
		}

		pTrackingInfo->Frequency_IF =
			FE_Sat_TunerGetFrequency(pParams->hTuner1);
		pTrackingInfo->Frequency_IF +=
			FE_STV0910_GetCarrierFrequency(pParams->hDemod,
					pParams->MasterClock, Demod) / 1000;

		if ((pParams->hDemod->Error) || (pParams->hTuner1->Error))
			error = FE_LLA_I2C_ERROR;

		break;

	case FE_SAT_DEMOD_2:

		if ((pParams->hDemod->Error) || (pParams->hTuner2->Error))
			return FE_LLA_I2C_ERROR;

		pTrackingInfo->Power = FE_STV0910_GetRFLevel(pParams, Demod);
		pTrackingInfo->BER = FE_STV0910_GetBer(pParams->hDemod, Demod);
		demodState = ChipGetField(pParams->hDemod,
				FSTV0910_P2_HEADER_MODE);
		switch (demodState) {
		case FE_SAT_SEARCH:
		case FE_SAT_PLH_DETECTED:
		default:
			pTrackingInfo->Locked = FALSE;
			break;
		case FE_SAT_DVBS2_FOUND:
			pTrackingInfo->Locked =
			    ChipGetField(pParams->hDemod,
					    FSTV0910_P2_LOCK_DEFINITIF) &&
			    ChipGetField(pParams->hDemod,
					    FSTV0910_P2_PKTDELIN_LOCK) &&
			    ChipGetField(pParams->hDemod,
					    FSTV0910_P2_TSFIFO_LINEOK);

			pTrackingInfo->C_N =
				FE_STV0910_CarrierGetQuality(pParams->hDemod,
					&FE_STV0910_S2_CN_LookUp, Demod);

			/*reset The error counter to PER */
			/* reset the error counter to  DVBS2 packet error rate*/
			ChipSetOneRegister(pParams->hDemod,
					RSTV0910_P2_ERRCTRL1, 0x67);

			break;

		case FE_SAT_DVBS_FOUND:
			pTrackingInfo->Locked =
			    ChipGetField(pParams->hDemod,
					    FSTV0910_P2_LOCK_DEFINITIF) &&
			    ChipGetField(pParams->hDemod,
					    FSTV0910_P2_LOCKEDVIT) &&
			    ChipGetField(pParams->hDemod,
					    FSTV0910_P2_TSFIFO_LINEOK);

			pTrackingInfo->C_N =
				FE_STV0910_CarrierGetQuality(pParams->hDemod,
					&FE_STV0910_S1_CN_LookUp, Demod);
			break;
		}

		pTrackingInfo->Frequency_IF =
			FE_Sat_TunerGetFrequency(pParams->hTuner2);
		pTrackingInfo->Frequency_IF +=
			FE_STV0910_GetCarrierFrequency(pParams->hDemod,
					pParams->MasterClock, Demod) / 1000;

		if ((pParams->hDemod->Error) || (pParams->hTuner2->Error))
			error = FE_LLA_I2C_ERROR;
		break;
	}

	return error;
}

/*****************************************************
--FUNCTION	::	FE_STV910_GetPacketErrorRate
--ACTION	::	Return informations the number of error packet and the
			window packet count
--PARAMS IN	::	Handle	==>	Front End Handle
		::	Demod	==>	Curent demod 1 or 2
--PARAMS OUT	::	PacketsErrorCount==>Number of packet error,
			max is 2^23 packet.
		::  TotalPacketsCount==>total window packets, max is 2^24 packet
--RETURN	::	Error(if any)
--***************************************************/
/*! \fn FE_STV0910_Error_t FE_STV910_GetPacketErrorRate(FE_STV0910_Handle_t
				Handle, FE_STV0910_DEMOD_t Demod,
				U32 *PacketsErrorCount, U32 *TotalPacketsCount);
  \brief Unlike the BER information of the Signal Info structure this public
  function gives the total packets number and the packets error number since
  the last time it was called(this function is useful for all standards DVBS1,
  DVBS2 or DSS), for first call after a search this function gives the total
  packet and error packet since the end of the search.
  */
FE_STV0910_Error_t FE_STV910_GetPacketErrorRate(FE_STV0910_Handle_t Handle,
						FE_STV0910_DEMOD_t Demod,
						U32 *PacketsErrorCount,
						U32 *TotalPacketsCount)
{
	FE_STV0910_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0910_InternalParams_t *pParams = NULL;

	U8 packetsCount4 = 0,
	    packetsCount3 = 0,
	    packetsCount2 = 0, packetsCount1 = 0, packetsCount0 = 0;

	S32 packetsCountReg, errorCntrlReg2, berResetField;

	pParams = (FE_STV0910_InternalParams_t *) Handle;

	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		packetsCountReg = RSTV0910_P1_FBERCPT4;
		errorCntrlReg2 = RSTV0910_P1_ERRCTRL2;
		berResetField = FSTV0910_P1_BERMETER_RESET;
		if ((pParams->hDemod->Error) || (pParams->hTuner1->Error))
			error = FE_LLA_I2C_ERROR;

		break;

	case FE_SAT_DEMOD_2:
		packetsCountReg = RSTV0910_P2_FBERCPT4;
		errorCntrlReg2 = RSTV0910_P2_ERRCTRL2;
		berResetField = FSTV0910_P2_BERMETER_RESET;

		if ((pParams->hDemod->Error) || (pParams->hTuner2->Error))
			error = FE_LLA_I2C_ERROR;

		break;

	}
	if (error != FE_LLA_NO_ERROR)
		return error;

	/*if Demod+FEC not locked */
	if (FE_STV0910_Status(Handle, Demod) == FALSE) {
		/*Packet error count is set to the max value */
		*PacketsErrorCount = 1 << 23;
		/*Total Packet count is set to the max value */
		*TotalPacketsCount = 1 << 24;
	} else {
		/*Read the error counter 2(23 bits) */
		*PacketsErrorCount =
		    FE_STV0910_GetErrorCount(pParams->hDemod,
				    FE_STV0910_COUNTER2, Demod) & 0x7FFFFF;

		/*Read the total packet counter 40 bits, read 5 bytes is
		 * mondatory */
		/*Read the Total packet counter byte 5 */
		packetsCount4 = ChipGetOneRegister(pParams->hDemod,
				packetsCountReg);
		/*Read the Total packet counter byte 4 */
		packetsCount3 = ChipGetOneRegister(pParams->hDemod,
				packetsCountReg + 1);
		/*Read the Total packet counter byte 3 */
		packetsCount2 = ChipGetOneRegister(pParams->hDemod,
				packetsCountReg + 2);
		/*Read the Total packet counter byte 2 */
		packetsCount1 = ChipGetOneRegister(pParams->hDemod,
				packetsCountReg + 3);
		/*Read the Total packet counter byte 1 */
		packetsCount0 = ChipGetOneRegister(pParams->hDemod,
				packetsCountReg + 4);

		/*Use the counter for a max of 2^24 packets */
		if ((packetsCount4 == 0) && (packetsCount3 == 0)) {
			*TotalPacketsCount = ((packetsCount2 & 0xFF) << 16) +
						((packetsCount1 & 0xFF) << 8) +
						(packetsCount0 & 0xFF);
		} else
			*TotalPacketsCount = 1 << 24;

		if (*TotalPacketsCount == 0) {
			/*if the packets count doesn't start yet the packet
			 * error = 1 and total packets = 1 */
			/* if the packet counter doesn't start there is an FEC
			 * error  */
			*TotalPacketsCount = 1;
			*PacketsErrorCount = 1;
		}
	}

	/*Reset the Total packet counter */
	ChipSetOneRegister(pParams->hDemod, packetsCountReg, 0);
	ChipSetField(pParams->hDemod, berResetField, 1);
	ChipSetField(pParams->hDemod, berResetField, 0);
	/*Reset the packet Error counter2(and Set it to infinit error count
	 * mode) */
	ChipSetOneRegister(pParams->hDemod, errorCntrlReg2, 0xc1);

	/*Check the error at the end of the function */
	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		if ((pParams->hDemod->Error) || (pParams->hTuner1->Error))
			error = FE_LLA_I2C_ERROR;

		break;

	case FE_SAT_DEMOD_2:
		if ((pParams->hDemod->Error) || (pParams->hTuner2->Error))
			error = FE_LLA_I2C_ERROR;
		break;

	}

	return error;
}

/*****************************************************
--FUNCTION	::	FE_STV910_GetPreViterbiBER
--ACTION	::	Return DEMOD BER(Pre - VITERBI BER) for DVB - S1 only
--PARAMS IN	::	Handle	==>	Front End Handle
		::  Demod	==>	Cuurent demod 1 or 2
--PARAMS OUT	::  Pre - VITERBI BER x 10^7
--RETURN	::	Error(if any)
--***************************************************/
/*! \fn FE_STV0910_Error_t FE_STV910_GetPreViterbiBER(FE_STV0910_Handle_t
			Handle, FE_STV0910_DEMOD_t Demod, U32 *PreViterbiBER)
  \brief This public function is used to get Pre Viterbi BER in DVBS only.
  */
FE_STV0910_Error_t FE_STV910_GetPreViterbiBER(FE_STV0910_Handle_t Handle,
					      FE_STV0910_DEMOD_t Demod,
					      U32 *PreViterbiBER)
{
	/* warning, when using this function the error counter number 1 is set
	 * to pre - Viterbi BER, the post viterbi BER info(given in
	 * FE_STV0910_GetSignalInfo function or FE_STV0910_Tracking is not
	 * significant any more
	 */

	FE_STV0910_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0910_InternalParams_t *pParams = NULL;

	U32 errorCount = 10000000, frameSize;
	FE_STV0910_Rate_t punctureRate;

	BOOL lockedVit = FALSE;

	pParams = (FE_STV0910_InternalParams_t *) Handle;

	if (pParams == NULL) {
		*PreViterbiBER = errorCount;
		return error;
	}
	if (pParams->hDemod->Error) {
		*PreViterbiBER = errorCount;
		return FE_LLA_I2C_ERROR;
	}

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		/* check for VITERBI lock */
		lockedVit =
		    ChipGetField(pParams->hDemod, FSTV0910_P1_LOCKEDVIT);
		break;

	case FE_SAT_DEMOD_2:
		/* check for VITERBI lock */
		lockedVit =
		    ChipGetField(pParams->hDemod, FSTV0910_P2_LOCKEDVIT);
		break;
	}
	if (lockedVit) {	/* check for VITERBI lock */
		switch (Demod) {
		case FE_SAT_DEMOD_1:
		default:
			/* Set and reset the error counter1 to Pre - Viterbi
			 * BER with observation window = 2^6 frames */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0910_P1_ERRCTRL1, 0x13);
			/* wait for error accumulation */
			WAIT_N_MS(50);

			/* Read the puncture rate */
			punctureRate =
			    FE_STV0910_GetViterbiPunctureRate(pParams->hDemod,
					    Demod);

			/* Read the error counter */
			errorCount =
			    ((ChipGetOneRegister(pParams->hDemod,
					 RSTV0910_P1_ERRCNT12) & 0x7F) << 16) +
			    ((ChipGetOneRegister(pParams->hDemod,
					  RSTV0910_P1_ERRCNT11) & 0xFF) << 8) +
			    ((ChipGetOneRegister(pParams->hDemod,
					  RSTV0910_P1_ERRCNT10) & 0xFF));
			break;

		case FE_SAT_DEMOD_2:
			/* Set and reset the error counter1 to Pre - Viterbi
			 * BER with observation window = 2^6 frames */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0910_P2_ERRCTRL1, 0x13);
			/* wait for error accumulation */
			WAIT_N_MS(50);

			/* Read the puncture rate */
			punctureRate =
			    FE_STV0910_GetViterbiPunctureRate(pParams->hDemod,
					    Demod);

			/* Read the error counter */
			errorCount =
			    ((ChipGetOneRegister(pParams->hDemod,
					 RSTV0910_P2_ERRCNT12) & 0x7F) << 16) +
			    ((ChipGetOneRegister(pParams->hDemod,
					 RSTV0910_P2_ERRCNT11) & 0xFF) << 8) +
			    ((ChipGetOneRegister(pParams->hDemod,
					 RSTV0910_P2_ERRCNT10) & 0xFF));
			break;
		}

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
			/*total window size = frameSize*2^6 */
			frameSize *= 64;
			errorCount = (errorCount * 1000) / (frameSize / 1000);
			errorCount *= 10;
		} else {
			/* if PR is unknown BER = 1 */
			errorCount = 10000000;
		}
	} else
		/*if VITERBI is not locked BER = 1 */
		errorCount = 10000000;

	if (pParams->hDemod->Error)
		error = FE_LLA_I2C_ERROR;

	*PreViterbiBER = errorCount;

	return error;
}

/*****************************************************
  --FUNCTION	::	FE_STV0910_Status
  --ACTION	::	Return locked status
  --PARAMS IN	::	Handle	==>	Front End Handle
  ::	Demod	==>	Current demod 1 or 2
  --PARAMS OUT::	NONE
  --RETURN	::	Bool(locked or not)
  --***************************************************/
/*! \fn BOOL FE_STV0910_Status(FE_STV0910_Handle_t Handle,
						FE_STV0910_DEMOD_t Demod)
  \brief This public function return s the lock status.
  */
BOOL FE_STV0910_Status(FE_STV0910_Handle_t Handle, FE_STV0910_DEMOD_t Demod)
{
	FE_STV0910_InternalParams_t *pParams = NULL;
	BOOL Locked = FALSE;
	FE_STV0910_SEARCHSTATE_t demodState;

	pParams = (FE_STV0910_InternalParams_t *) Handle;

	if (pParams == NULL)
		return FALSE;

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		demodState =
		    ChipGetField(pParams->hDemod, FSTV0910_P1_HEADER_MODE);
		switch (demodState) {
		case FE_SAT_SEARCH:
		case FE_SAT_PLH_DETECTED:
		default:
			Locked = FALSE;
			break;
		case FE_SAT_DVBS2_FOUND:
			Locked =
			    ChipGetField(pParams->hDemod,
					    FSTV0910_P1_LOCK_DEFINITIF) &&
			    ChipGetField(pParams->hDemod,
					    FSTV0910_P1_PKTDELIN_LOCK) &&
			    ChipGetField(pParams->hDemod,
					    FSTV0910_P1_TSFIFO_LINEOK);
			break;

		case FE_SAT_DVBS_FOUND:
			Locked =
			    ChipGetField(pParams->hDemod,
					    FSTV0910_P1_LOCK_DEFINITIF) &&
			    ChipGetField(pParams->hDemod,
					    FSTV0910_P1_LOCKEDVIT) &&
			    ChipGetField(pParams->hDemod,
					    FSTV0910_P1_TSFIFO_LINEOK);
			break;
		}
		break;

	case FE_SAT_DEMOD_2:
		demodState =
		    ChipGetField(pParams->hDemod, FSTV0910_P2_HEADER_MODE);
		switch (demodState) {
		case FE_SAT_SEARCH:
		case FE_SAT_PLH_DETECTED:
		default:
			Locked = FALSE;
			break;
		case FE_SAT_DVBS2_FOUND:
			Locked =
			    ChipGetField(pParams->hDemod,
					    FSTV0910_P2_LOCK_DEFINITIF) &&
			    ChipGetField(pParams->hDemod,
					    FSTV0910_P2_PKTDELIN_LOCK) &&
			    ChipGetField(pParams->hDemod,
					    FSTV0910_P2_TSFIFO_LINEOK);
			break;

		case FE_SAT_DVBS_FOUND:
			Locked =
			    ChipGetField(pParams->hDemod,
					    FSTV0910_P2_LOCK_DEFINITIF) &&
			    ChipGetField(pParams->hDemod,
					    FSTV0910_P2_LOCKEDVIT) &&
			    ChipGetField(pParams->hDemod,
					    FSTV0910_P2_TSFIFO_LINEOK);
			break;
		}
		break;
	}

	return Locked;
}

/*****************************************************
--FUNCTION	::	FE_STV0910_Unlock
--ACTION	::	Unlock the demodulator, set the demod to idle state
--PARAMS IN	::	Handle	==>	Front End Handle
		::	Demod	==>	Current demod 1 or 2
-PARAMS OUT	::	NONE.
--RETURN	::	Error(if any)
--***************************************************/
/*! \fn FE_STV0910_Error_t FE_STV0910_Unlock(FE_STV0910_Handle_t Handle,
						FE_STV0910_DEMOD_t Demod)
  \brief The public function is used to put the demodulator into idle mode, it
  stops the demodulator
  */
FE_STV0910_Error_t FE_STV0910_Unlock(FE_STV0910_Handle_t Handle,
				     FE_STV0910_DEMOD_t Demod)
{
	FE_STV0910_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0910_InternalParams_t *pParams =
	    (FE_STV0910_InternalParams_t *) Handle;

	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;

	switch (Demod) {
	case FE_SAT_DEMOD_1:

		if ((pParams->hDemod->Error) || (pParams->hTuner1->Error))
			return FE_LLA_I2C_ERROR;
		/* Demod Stop */
		ChipSetOneRegister(pParams->hDemod, RSTV0910_P1_DMDISTATE,
				0x5C);

		FE_Sat_TunerSetFrequency(pParams->hTuner1, 950);

		/*Check the error at the end of the function */
		if ((pParams->hDemod->Error) || (pParams->hTuner1->Error))
			error = FE_LLA_I2C_ERROR;
		break;

	case FE_SAT_DEMOD_2:
		if ((pParams->hDemod->Error) || (pParams->hTuner2->Error))
			return FE_LLA_I2C_ERROR;

			/* Demod Stop */
		ChipSetOneRegister(pParams->hDemod, RSTV0910_P2_DMDISTATE,
				0x5C);

		FE_Sat_TunerSetFrequency(pParams->hTuner2, 950);

		/*Check the error at the end of the function */
		if ((pParams->hDemod->Error) || (pParams->hTuner2->Error))
			error = FE_LLA_I2C_ERROR;
		break;

	default:
		break;
	}

	return error;
}

/*****************************************************
--FUNCTION	::	FE_STV0910_ResetDevicesErrors
--ACTION	::	Reset Devices I2C error status
--PARAMS IN	::	Handle	==>	Front End Handle
		::
-PARAMS OUT	::	NONE.
--RETURN	::	Error(if any)
--***************************************************/
/*! \fn FE_STV0910_Error_t FE_STV0910_ResetDevicesErrors(FE_STV0910_Handle_t
					Handle, FE_STV0910_DEMOD_t Demod);
  \brief When an I2C error occurred on a one of the FE devices(demodulator and
  tuners) it is stored in the field Error of the STCHIP_Info_t of the given
  device and all the next I2C access are rejected while this status is not
  cleared. For this purpose, the public function FE_STV0910_ResetDevicesErrors
  is added to reset all the FE devices internal error status. (Demodulator and
  both tuners).
  */
FE_STV0910_Error_t FE_STV0910_ResetDevicesErrors(FE_STV0910_Handle_t Handle,
						 FE_STV0910_DEMOD_t Demod)
{
	FE_STV0910_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0910_InternalParams_t *pParams =
	    (FE_STV0910_InternalParams_t *) Handle;

	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;

	if (pParams->hDemod != NULL)
		pParams->hDemod->Error = CHIPERR_NO_ERROR;
	else
		error = FE_LLA_INVALID_HANDLE;

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		if (pParams->hTuner1 != NULL)
			pParams->hTuner1->Error = CHIPERR_NO_ERROR;
		else
			error = FE_LLA_INVALID_HANDLE;
		break;

	case FE_SAT_DEMOD_2:
		if (pParams->hTuner2 != NULL)
			pParams->hTuner2->Error = CHIPERR_NO_ERROR;
		else
			error = FE_LLA_INVALID_HANDLE;
		break;
	}

	return error;
}

/*****************************************************
--FUNCTION	::	FE_STV0910_SetMclk
--ACTION	::	Set demod Master Clock
--PARAMS IN	::	Handle	==>	Front End Handle
		::	Mclk	==> demod master clock
		::	ExtClk	==>	external Quartz
--PARAMS OUT	::	NONE.
--RETURN	::	Error(if any)
--***************************************************/
/*! \fn FE_STV0910_Error_t FE_STV0910_SetMclk(FE_STV0910_Handle_t Handle,
  U32 Mclk,
  U32 ExtClk);
  \brief This public function set the PLL registers to obtain the desired
  master clock(Mclk, typically 135MHz) in funtion of the external
  quartz(ExtClk, typically 30 MHz)
  */
FE_STV0910_Error_t FE_STV0910_SetMclk(FE_STV0910_Handle_t Handle, U32 Mclk,
				      U32 ExtClk)
{
	FE_STV0910_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0910_InternalParams_t *pParams =
	    (FE_STV0910_InternalParams_t *) Handle;

	U32 ndiv, odf, idf, Fphi, quartz;

	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;
	if (pParams->hDemod->Error)
		return FE_LLA_I2C_ERROR;
	/* 800MHz < Fvco < 1800MHz */
	/* Fvco = (ExtClk * 2 * NDIV) / IDF */
	/* (400 * IDF) / ExtClk < NDIV < (900 * IDF) / ExtClk */

	/* ODF forced to 4 otherwise desynchronization of digital and analog
	 * clock which result to a bad calculated symbolrate */
	odf = 4;
	/* IDF forced to 1 : Optimal value */
	idf = 1;

	ChipSetField(pParams->hDemod, FSTV0910_ODF, odf);
	ChipSetField(pParams->hDemod, FSTV0910_IDF, idf);

	quartz = ExtClk / 1000000;
	Fphi = Mclk / 1000000;

	ndiv = (Fphi * odf * idf) / quartz;

	ChipSetField(pParams->hDemod, FSTV0910_N_DIV, ndiv);

	/* Set CP according to NDIV */
	FE_STV0910_SetChargePump(pParams->hDemod, ndiv);

	/*Check the error at the end of the function */
	if (pParams->hDemod->Error)
		error = FE_LLA_I2C_ERROR;
	return error;
}

/*****************************************************
--FUNCTION	::	FE_STV0910_SetStandby
--ACTION	::	Set demod STANDBAY mode On/Off
--PARAMS IN	::	Handle	==>	Front End Handle

-PARAMS OUT	::	NONE.
--RETURN	::	Error(if any)
- Ajout demod et separe les 2
--***************************************************/
/*! \fn ST_Revision_t FE_STV0910_Error_t FE_STV0910_SetStandby(
			FE_STV0910_Handle_t Handle, U8 StandbyOn,
			FE_STV0910_DEMOD_t Demod)
  \brief This public function is used to set the standby mode(On/Off) by
  accessing to the field STANDBY(bit[7] of the register SYNTCTRL @F1B6). In
  standby mode only the I2C clock is functional in order to walk - up the
  device
  */
FE_STV0910_Error_t FE_STV0910_SetStandby(FE_STV0910_Handle_t Handle,
					 U8 StandbyOn, FE_STV0910_DEMOD_t Demod)
{
	FE_STV0910_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0910_InternalParams_t *pParams =
	    (FE_STV0910_InternalParams_t *) Handle;

	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		if ((pParams->hDemod->Error) || (pParams->hTuner1->Error))
			return FE_LLA_I2C_ERROR;

		if (StandbyOn) {
			/*ADC1 power off */
			ChipSetField(pParams->hDemod, FSTV0910_ADC1_PON, 0);

			/*Stop Path1 Clocks */
			/*Stop Sampling clock */
			ChipSetField(pParams->hDemod, FSTV0910_STOP_DEMOD, 1);
			/*Stop VITERBI 1 clock */
			ChipSetField(pParams->hDemod, FSTV0910_STOP_DVBS1FEC,
					1);
			/*Stop VITERBI 1 clock */
			ChipSetField(pParams->hDemod, FSTV0910_STOP_DVBS2FEC,
					1);

			/* check if path2 in low power mode */
			if (ChipGetField(pParams->hDemod, FSTV0910_ADC2_PON) ==
					0) {
				/*
				   The STANDBY as well as STOP_CLKFEC field and
				   STOP_CLKTS field are common for both path
				   these field are set to '1' only when both
				   path's are in low power mode a path is in
				   low power mode when it's ADC is stoped
				 */

				/*DiseqC 1 power off */
				ChipSetField(pParams->hDemod,
						FSTV0910_I2C_DISEQC_PON, 0);
				/*FSK power off */
				ChipSetField(pParams->hDemod, FSTV0910_FSK_PON,
						0);
				/* general power OFF */
				ChipSetField(pParams->hDemod, FSTV0910_STANDBY,
						1);
			}

			/* Set tuner1 to standby On */
			FE_Sat_TunerSetStandby(pParams->hTuner1, 1);
			if (FE_STV0910_CheckVGLNAPresent(pParams->hVGLNA1))
				STVVGLNA_SetStandby(pParams->hVGLNA1, 1);

		} else {
			/* general power ON */
			ChipSetField(pParams->hDemod, FSTV0910_STANDBY, 0);

			/*FSK power ON */
			ChipSetField(pParams->hDemod, FSTV0910_FSK_PON, 1);
			/*ADC1 power on */
			ChipSetField(pParams->hDemod, FSTV0910_ADC1_PON, 1);
			/*DiseqC 1 power ON */
			ChipSetField(pParams->hDemod, FSTV0910_I2C_DISEQC_PON,
					1);

			/*Stop Path1 Clocks */
			/*Stop Sampling clock */
			ChipSetField(pParams->hDemod, FSTV0910_STOP_DEMOD, 0);
			/*Stop VITERBI 1 clock */
			ChipSetField(pParams->hDemod, FSTV0910_STOP_DVBS1FEC,
					0);
			/*Stop VITERBI 1 clock */
			ChipSetField(pParams->hDemod, FSTV0910_STOP_DVBS2FEC,
					0);

			/* Set tuner1 to standby OFF */
			FE_Sat_TunerSetStandby(pParams->hTuner1, 0);
			if (FE_STV0910_CheckVGLNAPresent(pParams->hVGLNA1))
				STVVGLNA_SetStandby(pParams->hVGLNA1, 1);

		}

		/*Check the error at the end of the function */
		if ((pParams->hDemod->Error) || (pParams->hTuner1->Error))
			error = FE_LLA_I2C_ERROR;
		break;

	case FE_SAT_DEMOD_2:

		if ((pParams->hDemod->Error) || (pParams->hTuner2->Error))
			return FE_LLA_I2C_ERROR;

		if (StandbyOn) {
			/*ADC1 power off */
			ChipSetField(pParams->hDemod, FSTV0910_ADC2_PON, 0);

			/*Stop Path1 Clocks */
			/*Stop Sampling clock */
			ChipSetField(pParams->hDemod, FSTV0910_STOP_DEMOD2, 1);
			/*Stop VITERBI 1 clock */
			ChipSetField(pParams->hDemod, FSTV0910_STOP_DVBS1FEC2,
					1);
			/*Stop VITERBI 1 clock */
			ChipSetField(pParams->hDemod, FSTV0910_STOP_DVBS2FEC2,
					1);

			/* check path1 is in low power mode */
			if (ChipGetField(pParams->hDemod, FSTV0910_ADC1_PON) ==
					0) {
				/* The STANDBY as well as STOP_CLKFEC field and
				 * STOP_CLKTS field are common for both path
				 * these field are set to '1' only when both
				 * path's are in low power mode a path is in
				 * low power mode when it's ADC is stoped */

				/*DiseqC 1 power off */
				ChipSetField(pParams->hDemod,
						FSTV0910_I2C_DISEQC_PON, 0);
				/*FSK power off */
				ChipSetField(pParams->hDemod, FSTV0910_FSK_PON,
						0);
				/* general power ON */
				ChipSetField(pParams->hDemod, FSTV0910_STANDBY,
						1);
			}

			/* Set tuner2 to standby On */
			FE_Sat_TunerSetStandby(pParams->hTuner2, 1);
			if (FE_STV0910_CheckVGLNAPresent(pParams->hVGLNA2))
				STVVGLNA_SetStandby(pParams->hVGLNA2, 1);
		} else {
			/* general power ON */
			ChipSetField(pParams->hDemod, FSTV0910_STANDBY, 0);

			/*FSK power ON */
			ChipSetField(pParams->hDemod, FSTV0910_FSK_PON, 1);
			/*ADC1 power off */
			ChipSetField(pParams->hDemod, FSTV0910_ADC2_PON, 1);
			/*DiseqC 1 power ON */
			ChipSetField(pParams->hDemod, FSTV0910_I2C_DISEQC_PON,
					1);

			/*Stop Path1 Clocks */
			/*Stop Sampling clock */
			ChipSetField(pParams->hDemod, FSTV0910_STOP_DEMOD2, 0);
			/*Stop VITERBI 1 clock */
			ChipSetField(pParams->hDemod, FSTV0910_STOP_DVBS1FEC2,
					0);
			/*Stop VITERBI 1 clock */
			ChipSetField(pParams->hDemod, FSTV0910_STOP_DVBS2FEC2,
					0);

			/* Set tuner2 to standby OFF */
			FE_Sat_TunerSetStandby(pParams->hTuner2, 0);
			if (FE_STV0910_CheckVGLNAPresent(pParams->hVGLNA2))
				STVVGLNA_SetStandby(pParams->hVGLNA2, 0);

		}

		/*Check the error at the end of the function */
		if ((pParams->hDemod->Error) || (pParams->hTuner2->Error))
			error = FE_LLA_I2C_ERROR;
		break;
	}

	return error;
}

/*****************************************************
--FUNCTION	::	FE_STV0910_SetAbortFlag
--ACTION	::	Set Abort flag On/Off
--PARAMS IN	::	Handle	==>	Front End Handle

-PARAMS OUT::	NONE.
--RETURN	::	Error(if any)
ajoute demod et separe les 2
--***************************************************/
/*! \fn FE_STV0910_Error_t FE_STV0910_SetAbortFlag(FE_STV0910_Handle_t Handle,
					BOOL Abort, FE_STV0910_DEMOD_t Demod);
  \brief This public function is used to set the abort flag(True/FALSE) of the
  necessary devices(Demodulator +tuner1 if path1 and demodulator + tuner2 if
  path2).  See the section Abort Mechanism upper
  */
FE_STV0910_Error_t FE_STV0910_SetAbortFlag(FE_STV0910_Handle_t Handle,
					   BOOL Abort, FE_STV0910_DEMOD_t Demod)
{
	FE_STV0910_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0910_InternalParams_t *pParams =
	    (FE_STV0910_InternalParams_t *) Handle;

	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		if ((pParams->hDemod->Error) || (pParams->hTuner1->Error))
			return FE_LLA_I2C_ERROR;

		ChipAbort(pParams->hTuner1, Abort);
		ChipAbort(pParams->hDemod, Abort);

		/*Check the error at the end of the function */
		if ((pParams->hDemod->Error) || (pParams->hTuner1->Error))
			error = FE_LLA_I2C_ERROR;
		break;

	case FE_SAT_DEMOD_2:
		if ((pParams->hDemod->Error) || (pParams->hTuner2->Error))
			return FE_LLA_I2C_ERROR;
		ChipAbort(pParams->hTuner2, Abort);
		ChipAbort(pParams->hDemod, Abort);

		/*Check the error at the end of the function */
		if ((pParams->hDemod->Error) || (pParams->hTuner2->Error))
			error = FE_LLA_I2C_ERROR;
		break;
	}
	return error;
}

/*****************************************************
--FUNCTION	::	FE_STV0910_SetDVBS2_Single
--ACTION	::	Set 900 LDPC to Single or dual
--PARAMS IN	::	Handle	==>	Front End Handle
		::	LDPC_Mode ==> LDPC setting single or dual
		::	Demod ==> in single mode this parameter is used to set
			the LDPC in single mode to the given demod
-PARAMS OUT::	NONE.
--RETURN	::	Error(if any)
--***************************************************/
/*!\fn FE_STV0910_Error_t FE_STV0910_SetDVBS2_Single(FE_STV0910_Handle_t Handle,
  FE_Sat_DemodType LDPC_Mode,
  FE_STV0910_DEMOD_t Demod)
  \brief This public function is used to set the DVBS2 FEC to single/dual mode
  */
FE_STV0910_Error_t FE_STV0910_SetDVBS2_Single(FE_STV0910_Handle_t Handle,
					      FE_Sat_DemodType LDPC_Mode,
					      FE_STV0910_DEMOD_t Demod)
{
	FE_STV0910_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0910_InternalParams_t *pParams =
	    (FE_STV0910_InternalParams_t *) Handle;

	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;

	if (pParams->hDemod->Error)
		return FE_LLA_I2C_ERROR;

	switch (LDPC_Mode) {
	case FE_SAT_DUAL:
	default:
		/*If the LDPC is already in dual mode do nothing */
		if ((pParams->DemodType != FE_SAT_DUAL) ||
			(ChipGetField(pParams->hDemod, FSTV0910_DDEMOD) != 1)) {
			/*Set the LDPC to dual mode */
			ChipSetOneRegister(pParams->hDemod, RSTV0910_GENCFG,
					0x1d);
			pParams->DemodType = FE_SAT_DUAL;

			/*reset the LDPC */
			ChipSetField(pParams->hDemod, FSTV0910_FRESFEC, 1);
			/*reset the LDPC */
			ChipSetField(pParams->hDemod, FSTV0910_FRESFEC, 0);

			/*reset the packet delin */
			ChipSetField(pParams->hDemod, FSTV0910_P1_ALGOSWRST, 1);
			/*reset the packet delin */
			ChipSetField(pParams->hDemod, FSTV0910_P1_ALGOSWRST, 0);

			/*reset the packet delin */
			ChipSetField(pParams->hDemod, FSTV0910_P2_ALGOSWRST, 1);
			/*reset the packet delin */
			ChipSetField(pParams->hDemod, FSTV0910_P2_ALGOSWRST, 0);

		}
		break;

	case FE_SAT_SINGLE:
		if (Demod == FE_SAT_DEMOD_2)
			/*Set LDPC to single mode on path2 */
			ChipSetOneRegister(pParams->hDemod, RSTV0910_GENCFG,
					0x06);
		else
			/*Set LDPC to single mode on path1 */
			ChipSetOneRegister(pParams->hDemod, RSTV0910_GENCFG,
					0x04);
		pParams->DemodType = FE_SAT_SINGLE;

		/*reset the LDPC */
		ChipSetField(pParams->hDemod, FSTV0910_FRESFEC, 1);
		/*reset the LDPC */
		ChipSetField(pParams->hDemod, FSTV0910_FRESFEC, 0);

		/*reset the packet delin */
		ChipSetField(pParams->hDemod, FSTV0910_P1_ALGOSWRST, 1);
		/*reset the packet delin */
		ChipSetField(pParams->hDemod, FSTV0910_P1_ALGOSWRST, 0);

		/*reset the packet delin */
		ChipSetField(pParams->hDemod, FSTV0910_P2_ALGOSWRST, 1);
		/*reset the packet delin */
		ChipSetField(pParams->hDemod, FSTV0910_P2_ALGOSWRST, 0);

		break;
	}

	/*Check the error at the end of the function */
	if (pParams->hDemod->Error)
		error = FE_LLA_I2C_ERROR;

	return error;
}

/*****************************************************
--FUNCTION	::	FE_STV0910_Search
--ACTION	::	Search for a valid transponder
--PARAMS IN	::	Handle	==>	Front End Handle
			pSearch ==> Search parameters
			pResult ==> Result of the search
--PARAMS OUT::	NONE
--RETURN	::	Error(if any)
--***************************************************/
/*! \fn FE_STV0910_Error_t FE_STV0910_Search(FE_STV0910_Handle_t Handle,
			FE_STV0910_SearchParams_t *pSearch,
			FE_STV0910_SearchResult_t *pResult)
  \brief This public function is called to search for a valid transponder
  */
FE_STV0910_Error_t FE_STV0910_Search(FE_STV0910_Handle_t Handle,
				     FE_STV0910_SearchParams_t *pSearch,
				     FE_STV0910_SearchResult_t *pResult,
				     U32 Satellite_Scan)
{
	FE_STV0910_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0910_InternalParams_t *pParams = NULL;
	FE_STV0910_SIGNALTYPE_t signalType = FE_SAT_NOCARRIER;
	FE_Sat_TrackingStandard_t standard = FE_SAT_UNKNOWN_STANDARD;

#ifdef HOST_PC
#ifndef NO_GUI
	char message[200];
	double lockTime;
	lockTime = Timer();
#endif
#endif

	if (Handle == NULL)
		return FE_LLA_INVALID_HANDLE;

	pParams = (FE_STV0910_InternalParams_t *)Handle;

	/*50000000 */
	if (!(INRANGE(100000, pSearch->SymbolRate, 70000000))
	    || !(INRANGE(500000, pSearch->SearchRange, 70000000)))
		return error;

	search_start_time = jiffies;

	switch (pSearch->Path) {
	case FE_SAT_DEMOD_1:
	default:
		if ((pParams->hDemod->Error) || (pParams->hTuner1->Error))
			error = FE_LLA_I2C_ERROR;
		else {
			pParams->Demod1SearchStandard = pSearch->Standard;
			pParams->Demod1SymbolRate = pSearch->SymbolRate;
			pParams->Demod1SearchRange = pSearch->SearchRange;
			pParams->Tuner1Frequency = pSearch->Frequency;
			pParams->Demod1SearchAlgo = pSearch->SearchAlgo;
			pParams->Demod1Search_IQ_Inv = pSearch->IQ_Inversion;

			pParams->Demod1PunctureRate = pSearch->PunctureRate;
			pParams->Demod1Modulation = pSearch->Modulation;
			pParams->Demod1Modcode = pSearch->Modcode;
			/*Depending on the PAth Selected update the
			 * TunerIndexJump in pParams */
			/*pParams->Tuner1IndexJump = pSearch->TunerIndexJump; */
		}
		break;

	case FE_SAT_DEMOD_2:
		if ((pParams->hDemod->Error) || (pParams->hTuner2->Error))
			error = FE_LLA_I2C_ERROR;
		else {
			pParams->Demod2SearchStandard = pSearch->Standard;
			pParams->Demod2SymbolRate = pSearch->SymbolRate;
			pParams->Demod2SearchRange = pSearch->SearchRange;
			pParams->Tuner2Frequency = pSearch->Frequency;
			pParams->Demod2SearchAlgo = pSearch->SearchAlgo;
			pParams->Demod2Search_IQ_Inv = pSearch->IQ_Inversion;

			pParams->Demod2PunctureRate = pSearch->PunctureRate;
			pParams->Demod2Modulation = pSearch->Modulation;
			pParams->Demod2Modcode = pSearch->Modcode;
			/*Depending on the PAth Selected update
			 * the TunerIndexJump in pParams */
			/*pParams->Tuner2IndexJump = pSearch->TunerIndexJump; */
		}

		break;
	}

	if (error != FE_LLA_NO_ERROR)
		return FE_LLA_BAD_PARAMETER;

	signalType = FE_STV0910_Algo(pParams, pSearch->Path, Satellite_Scan);

	/* pParams->TunerIndexjump is updated in Algo.Update it in pSearch; = >
	 * passed to Inst in d0 layer to be used in ScanInfo/sattask
	 if (pSearch->Path == FE_SAT_DEMOD_1)
	 pSearch->TunerIndexJump = pParams->Tuner1IndexJump;
	 else if (pSearch->Path == FE_SAT_DEMOD_2)
	 pSearch->TunerIndexJump = pParams->Tuner2IndexJump; */

	if (signalType == FE_SAT_TUNER_JUMP) {
		error = FE_LLA_TUNER_JUMP;
	} else if (signalType == FE_SAT_TUNER_NOSIGNAL) {
		/* half of the tuner bandwith jump */
		error = FE_LLA_TUNER_NOSIGNAL;
	} else if (((signalType == FE_SAT_RANGEOK) || ((Satellite_Scan > 0) &&
					(signalType == FE_SAT_NODATA))) &&
			(pParams->hDemod->Error == CHIPERR_NO_ERROR)) {

		switch (pSearch->Path) {
		case FE_SAT_DEMOD_1:
		default:
			standard = pParams->Demod1Results.
				Standard;
			break;

		case FE_SAT_DEMOD_2:
			standard = pParams->Demod2Results.
				Standard;
			break;
		}

		if ((Satellite_Scan > 0)
		    && (signalType == FE_SAT_NODATA)
		    && (standard == FE_SAT_DVBS2_STANDARD)) {
			/* DVB-S2 TPs with demod lock only are logged as well*/
			error = FE_LLA_NODATA;
#ifdef HOST_PC
#ifndef NO_GUI
			Fmt(message, "Nodata Detect");
			ReportInsertMessage(message);
#endif
#endif
		} else
			error = FE_LLA_NO_ERROR;

		switch (pSearch->Path) {
		case FE_SAT_DEMOD_1:
		default:
			pResult->Locked = pParams->Demod1Results.Locked;

			/* update results */
			pResult->Standard = pParams->Demod1Results.Standard;
			pResult->Frequency = pParams->Demod1Results.Frequency;
			pResult->SymbolRate = pParams->Demod1Results.SymbolRate;
			pResult->PunctureRate =
					pParams->Demod1Results.PunctureRate;
			pResult->ModCode = pParams->Demod1Results.ModCode;
			pResult->Pilots = pParams->Demod1Results.Pilots;
			pResult->FrameLength =
					pParams->Demod1Results.FrameLength;
			pResult->Spectrum = pParams->Demod1Results.Spectrum;
			pResult->RollOff = pParams->Demod1Results.RollOff;
			pResult->Modulation = pParams->Demod1Results.Modulation;

			if ((pParams->hDemod->Error) ||
					(pParams->hTuner1->Error))
				error = FE_LLA_I2C_ERROR;

			break;

		case FE_SAT_DEMOD_2:
			pResult->Locked = pParams->Demod2Results.Locked;

			/* update results */
			pResult->Standard = pParams->Demod2Results.Standard;
			pResult->Frequency = pParams->Demod2Results.Frequency;
			pResult->SymbolRate = pParams->Demod2Results.SymbolRate;
			pResult->PunctureRate =
					pParams->Demod2Results.PunctureRate;
			pResult->ModCode = pParams->Demod2Results.ModCode;
			pResult->Pilots = pParams->Demod2Results.Pilots;
			pResult->FrameLength =
					pParams->Demod2Results.FrameLength;
			pResult->Spectrum = pParams->Demod2Results.Spectrum;
			pResult->RollOff = pParams->Demod2Results.RollOff;
			pResult->Modulation = pParams->Demod2Results.Modulation;

			if ((pParams->hDemod->Error) ||
					(pParams->hTuner2->Error))
				error = FE_LLA_I2C_ERROR;

			break;

		}
	} else {
		pResult->Locked = FALSE;

		switch (pSearch->Path) {
		case FE_SAT_DEMOD_1:
		default:
			switch (pParams->Demod1Error) {
			/*I2C error */
			case FE_LLA_I2C_ERROR:
				error = FE_LLA_I2C_ERROR;
				break;

			case FE_LLA_NO_ERROR:
			default:
				error = FE_LLA_SEARCH_FAILED;
				break;
			}
			if ((pParams->hDemod->Error) ||
					(pParams->hTuner1->Error))
				error = FE_LLA_I2C_ERROR;
			break;

		case FE_SAT_DEMOD_2:
			switch (pParams->Demod2Error) {
			/*I2C error     */
			case FE_LLA_I2C_ERROR:
				error = FE_LLA_I2C_ERROR;
				break;
			case FE_LLA_NO_ERROR:
			default:
				error = FE_LLA_SEARCH_FAILED;
				break;
			}

			if ((pParams->hDemod->Error) ||
					(pParams->hTuner2->Error))
				error = FE_LLA_I2C_ERROR;
			break;
		}
	}

	return error;

}

/*****************************************************
--FUNCTION	::	FE_STV0910_DVBS2_SetGoldCode
--ACTION	::	Set the DVBS2 Gold Code
--PARAMS IN	::	Handle	==>	Front End Handle
U32		==>	cold code value(18bits)
--PARAMS OUT::	NONE
--RETURN	::	Error(if any)
comme getSignalInfo
--***************************************************/
/*! \fn FE_STV0910_Error_t FE_STV0910_DVBS2_SetGoldCodeX(FE_STV0910_Handle_t
				Handle, FE_STV0910_DEMOD_t Demod, U32 GoldCode);
  \brief This public function is used to set the DVBS2 Gold Code
  */
FE_STV0910_Error_t FE_STV0910_DVBS2_SetGoldCodeX(FE_STV0910_Handle_t Handle,
						 FE_STV0910_DEMOD_t Demod,
						 U32 GoldCode)
{
	FE_STV0910_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0910_InternalParams_t *pParams = NULL;

	if ((void *)Handle == NULL)
		return FE_LLA_INVALID_HANDLE;

	pParams = (FE_STV0910_InternalParams_t *)Handle;

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		if ((pParams->hDemod->Error) || (pParams->hTuner1->Error))
			return FE_LLA_I2C_ERROR;

		/*Gold code X setting */
		/* bit[3:4] of register PLROOT2
		   3..2 : P1_PLSCRAMB_MODE[1:0] |URW|
		   entry mode p1_plscramb_root
		   00: p1_plscramb_root is the root of PRBS X.
		   01: p1_plscramb_root is the DVBS2 gold code.
		 */
		ChipSetOneRegister(pParams->hDemod, RSTV0910_P1_PLROOT2, 0x04 |
				((GoldCode >> 16) & 0x3));
		ChipSetOneRegister(pParams->hDemod, RSTV0910_P1_PLROOT1,
				(GoldCode >> 8) & 0xff);
		ChipSetOneRegister(pParams->hDemod, RSTV0910_P1_PLROOT0,
				(GoldCode) & 0xff);
		if ((pParams->hDemod->Error) || (pParams->hTuner1->Error))
			error = FE_LLA_I2C_ERROR;
		break;

	case FE_SAT_DEMOD_2:
		if ((pParams->hDemod->Error) || (pParams->hTuner2->Error))
			return FE_LLA_I2C_ERROR;

		/*Gold code X setting */
		/* bit[3:4] of register PLROOT2
		   3..2 : P1_PLSCRAMB_MODE[1:0] |URW|
		   entry mode p1_plscramb_root
		   00: p1_plscramb_root is the root of PRBS X.
		   01: p1_plscramb_root is the DVBS2 gold code.
		 */
		ChipSetOneRegister(pParams->hDemod, RSTV0910_P2_PLROOT2, 0x04 |
				((GoldCode >> 16) & 0x3));
		ChipSetOneRegister(pParams->hDemod, RSTV0910_P2_PLROOT1,
				(GoldCode >> 8) & 0xff);
		ChipSetOneRegister(pParams->hDemod, RSTV0910_P2_PLROOT0,
				(GoldCode) & 0xff);
		if ((pParams->hDemod->Error) || (pParams->hTuner2->Error))
			error = FE_LLA_I2C_ERROR;
	}

	return error;
}
#ifdef STM_FE_DEFINED_FOR_FUTURE_USE
/***********************************************************************
 **FUNCTION	::	FE_STV0910_SetupFSK
 **ACTION	::	Setup FSK
 **PARAMS IN	::	hChip-> handle to the chip
 **			fskt_carrie-> FSK modulator carrier frequency  (Hz)
 **			fskt_deltaf-> FSK frequency deviation(Hz)
 **			fskr_carrier-> FSK demodulator carrier frequency(Hz)
 **PARAMS OUT::	NONE
 **RETURN	::	Symbol frequency
 supprimer les tuner
 ***********************************************************************/
/*! \fn FE_STV0910_Error_t FE_STV0910_SetupFSK(FE_STV0910_Handle_t Handle,
  U32 TransmitCarrier,
  U32 ReceiveCarrier, U32 Deltaf);

  \brief This public function is used to initialize the FSK modulator and
  demodulator to a given TX frequency, RX frequency and frequency drift
  */
FE_STV0910_Error_t FE_STV0910_SetupFSK(FE_STV0910_Handle_t Handle,
				       U32 TransmitCarrier, U32 ReceiveCarrier,
				       U32 Deltaf)
{
	FE_STV0910_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0910_InternalParams_t *pParams = NULL;
	U32 fskt_carrier, fskt_deltaf, fskr_carrier;

	if ((void *)Handle == NULL)
		return FE_LLA_INVALID_HANDLE;

	pParams = (FE_STV0910_InternalParams_t *) Handle;

	if (pParams->hDemod->Error)
		return FE_LLA_I2C_ERROR;

	ChipSetField(pParams->hDemod, FSTV0910_FSKT_KMOD, 0x23);

	/*
	   Formula: FSKT_CAR = 2^20*transmit_carrier/MasterClock;
	 */
	fskt_carrier = (TransmitCarrier << 8) / (pParams->MasterClock >> 12);

	ChipSetField(pParams->hDemod, FSTV0910_FSKT_CAR2, ((fskt_carrier >> 16)
				& 0x03));
	ChipSetField(pParams->hDemod, FSTV0910_FSKT_CAR1, ((fskt_carrier >> 8)
				& 0xFF));
	ChipSetField(pParams->hDemod, FSTV0910_FSKT_CAR0, (fskt_carrier &
				0xFF));

	/*
	   Formula: FSKT_DELTAF = 2^20*fskt_deltaf/MasterClock;
	 */

	/* 2^20 = 1048576 */
	fskt_deltaf = (Deltaf << 8) / (pParams->MasterClock >> 12);

	ChipSetField(pParams->hDemod, FSTV0910_FSKT_DELTAF1, ((fskt_deltaf >>
					8) & 0x0F));
	ChipSetField(pParams->hDemod, FSTV0910_FSKT_DELTAF0, (fskt_deltaf &
				0xFF));

	/*FSK Modulator OFF */
	ChipSetOneRegister(pParams->hDemod, RSTV0910_FSKTCTRL, 0x00);
	/*ChipSetOneRegister(pParams->hDemod, RSTV0910_FSKTCTRL, 0x00) ;
	 * FSK Modulator cpntrol by TX_EN */

	ChipSetOneRegister(pParams->hDemod, RSTV0910_FSKRFC2, 0x10);
	/*
	   Formulat FSKR_CAR = 2^20*receive_carrier/MasterClock;
	 */

	fskr_carrier = (ReceiveCarrier << 8) / (pParams->MasterClock >> 12);

	ChipSetField(pParams->hDemod, FSTV0910_FSKR_CAR2, ((fskr_carrier >> 16)
				& 0x03));
	ChipSetField(pParams->hDemod, FSTV0910_FSKR_CAR1, ((fskr_carrier >> 8)
				& 0xFF));
	ChipSetField(pParams->hDemod, FSTV0910_FSKR_CAR0, (fskr_carrier &
				0xFF));

	ChipSetOneRegister(pParams->hDemod, RSTV0910_GPIO3CFG, 0x20);
	ChipSetOneRegister(pParams->hDemod, RSTV0910_GPIO4CFG, 0x9c);
	ChipSetOneRegister(pParams->hDemod, RSTV0910_GPIO5CFG, 0x9E);

	/*by default, modulator disabled */
	ChipSetOneRegister(pParams->hDemod, RSTV0910_FSKTCTRL, 0x00);
	ChipSetOneRegister(pParams->hDemod, RSTV0910_FSKTFC2, 0x30);
	ChipSetOneRegister(pParams->hDemod, RSTV0910_FSKTFC1, 0xc5);
	/*c8 */
	ChipSetOneRegister(pParams->hDemod, RSTV0910_FSKTFC0, 0x3e);
	/*57.6KHz */
	ChipSetOneRegister(pParams->hDemod, RSTV0910_FSKTDELTAF1, 0x1);
	ChipSetOneRegister(pParams->hDemod, RSTV0910_FSKTDELTAF0, 0xBf);

	ChipSetOneRegister(pParams->hDemod, RSTV0910_FSKRFC2, 0x1c);
	ChipSetOneRegister(pParams->hDemod, RSTV0910_FSKRFC1, 0x88);
	ChipSetOneRegister(pParams->hDemod, RSTV0910_FSKRFC0, 0x88);

	ChipSetOneRegister(pParams->hDemod, RSTV0910_FSKRK1, 0x6f);
	ChipSetOneRegister(pParams->hDemod, RSTV0910_FSKRK2, 0xb2);

	/*normal mode */
	ChipSetOneRegister(pParams->hDemod, RSTV0910_FSKRAGCR, 0x12);
	ChipSetOneRegister(pParams->hDemod, RSTV0910_FSKRAGC, 0xff);
	ChipSetOneRegister(pParams->hDemod, RSTV0910_FSKRALPHA, 0x17);
	ChipSetOneRegister(pParams->hDemod, RSTV0910_FSKRPLTH1, 0x90);
	ChipSetOneRegister(pParams->hDemod, RSTV0910_FSKRPLTH0, 0xc);

	ChipSetOneRegister(pParams->hDemod, RSTV0910_FSKRDF1, 0x1e);
	ChipSetOneRegister(pParams->hDemod, RSTV0910_FSKRDF0, 0x94);
	ChipSetOneRegister(pParams->hDemod, RSTV0910_FSKRSTEPP, 0x6);
	ChipSetOneRegister(pParams->hDemod, RSTV0910_FSKRSTEPM, 0xaa);
	/*0x81 */
	ChipSetOneRegister(pParams->hDemod, RSTV0910_FSKRDET1, 0x00);
	/*0x4 */
	ChipSetOneRegister(pParams->hDemod, RSTV0910_FSKRDET0, 0x1c);

	ChipSetOneRegister(pParams->hDemod, RSTV0910_FSKRDTH1, 0x0);
	ChipSetOneRegister(pParams->hDemod, RSTV0910_FSKRDTH0, 0x4d);

	ChipSetOneRegister(pParams->hDemod, RSTV0910_FSKRLOSS, 0xb);
	ChipSetOneRegister(pParams->hDemod, RSTV0910_TSTTNR0, 0x84);
	/*mod controlled by TX_EN */
	ChipSetOneRegister(pParams->hDemod, RSTV0910_FSKTCTRL, 0x08);

	/*Check the error at the end of the function */
	if (pParams->hDemod->Error)
		error = FE_LLA_I2C_ERROR;

	return error;
}

/***********************************************************
 **FUNCTION	::	FE_STV0910_EnableFSK
 **ACTION	::	Enable - Disable FSK modulator /Demodulator
 **PARAMS IN	::	hChip	 -> handle to the chip
 **				mod_en	 -> FSK modulator on/off
 **				demod_en -> FSK demodulator on/off
 **PARAMS OUT::	NONE
 **RETURN	::	error
 commm setFSK
 ***********************************************************/
/*! \fn FE_STV0910_Error_t FE_STV0910_EnableFSK(FE_STV0910_Handle_t Handle,
				BOOL EnableModulation, BOOL EnableDemodulation);
  \brief This public function is used to enable/disable the FSK modulator.
  */
FE_STV0910_Error_t FE_STV0910_EnableFSK(FE_STV0910_Handle_t Handle,
					BOOL EnableModulation,
					BOOL EnableDemodulation)
{
	FE_STV0910_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0910_InternalParams_t *pParams = NULL;

	if ((void *)Handle == NULL)
		return FE_LLA_INVALID_HANDLE;

	pParams = (FE_STV0910_InternalParams_t *)Handle;

	if (pParams->hDemod->Error)
		return FE_LLA_I2C_ERROR;

	if (EnableModulation == TRUE)
		ChipSetField(pParams->hDemod, FSTV0910_FSKT_MOD_EN, 1);
	else
		ChipSetField(pParams->hDemod, FSTV0910_FSKT_MOD_EN, 0);

	/*Check the error at the end of the function */
	if (pParams->hDemod->Error)
		error = FE_LLA_I2C_ERROR;

	return error;
}
#endif

/*****************************************************
--FUNCTION	::	FE_STV0910_DiseqcTxInit
--ACTION	::	Set the diseqC Tx mode
--PARAMS IN	::	Handle	==>	Front End Handle
		::	DiseqCMode==>diseqc Mode : continues tone, 2/3 PWM,
			3/3 PWM, 2/3 envelop or 3/3 envelop.
		::	Demod==>Current demod 1 or 2
--PARAMS OUT::None
--RETURN	::	Error(if any)
--***************************************************/
/*! \fn FE_STV0910_Error_t FE_STV0910_DiseqcInit(FE_STV0910_Handle_t Handle,
		FE_STV0910_DiseqC_TxMode DiseqCMode, FE_STV0910_DEMOD_t Demod)
  \brief This public function is used to set the diseqC Tx mode.
  */
FE_STV0910_Error_t FE_STV0910_DiseqcInit(FE_STV0910_Handle_t Handle,
					 FE_STV0910_DiseqC_TxMode DiseqCMode,
					 FE_STV0910_DEMOD_t Demod)
{
	FE_STV0910_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0910_InternalParams_t *pParams = NULL;
	S32 modeField = 0, resetField = 0;

	pParams = (FE_STV0910_InternalParams_t *) Handle;
	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		if ((pParams->hDemod->Error) || (pParams->hTuner1->Error))
			error = FE_LLA_I2C_ERROR;
		else {
			modeField = FSTV0910_P1_DISEQC_MODE;
			resetField = FSTV0910_P1_DISTX_RESET;

			/*Check the error at the end of the function */
			if ((pParams->hDemod->Error) ||
					(pParams->hTuner1->Error))
				error = FE_LLA_I2C_ERROR;
		}
		break;

	case FE_SAT_DEMOD_2:
		if ((pParams->hDemod->Error) || (pParams->hTuner2->Error))
			error = FE_LLA_I2C_ERROR;
		else {
			modeField = FSTV0910_P2_DISEQC_MODE;
			resetField = FSTV0910_P2_DISTX_RESET;

			/*Check the error at the end of the function */
			if ((pParams->hDemod->Error) ||
					(pParams->hTuner2->Error))
				error = FE_LLA_I2C_ERROR;
		}
		break;
	}

	ChipSetField(pParams->hDemod, modeField, DiseqCMode);

	ChipSetField(pParams->hDemod, resetField, 1);
	ChipSetField(pParams->hDemod, resetField, 0);

	return error;
}
EXPORT_SYMBOL(FE_STV0910_DiseqcInit);

/*****************************************************
  --FUNCTION	::	FE_STV0910_DiseqcSend
  --ACTION	::	Read receved bytes from DiseqC FIFO
  --PARAMS IN	::	Handle	==>	Front End Handle
  ::	Data	==> Table of bytes to send.
  ::	NbData	==> Number of bytes to send.
  ::	Demod	==>	Current demod 1 or 2
  --PARAMS OUT::None
  --RETURN	::	Error(if any)
  --***************************************************/
/*! \fn FE_STV0910_Error_t FE_STV0910_DiseqcSend(FE_STV0910_Handle_t Handle,
  U8 *Data,
  U32 NbData,
  FE_STV0910_DEMOD_t Demod);
  \brief This public function is used to manage the Diseqc data sending
  */
FE_STV0910_Error_t FE_STV0910_DiseqcSend(FE_STV0910_Handle_t Handle, U8 *Data,
					 U32 NbData, FE_STV0910_DEMOD_t Demod)
{
	FE_STV0910_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0910_InternalParams_t *pParams = NULL;
	S32 i = 0;

	pParams = (FE_STV0910_InternalParams_t *)Handle;
	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		if ((pParams->hDemod->Error) || (pParams->hTuner1->Error))
			return FE_LLA_I2C_ERROR;

		ChipSetField(pParams->hDemod, FSTV0910_P1_DISRX_ON, 0);
		ChipSetField(pParams->hDemod, FSTV0910_P1_DIS_PRECHARGE, 1);

		while (i < NbData) {
			/* wait for FIFO empty */
			while (ChipGetField(pParams->hDemod,
						FSTV0910_P1_TX_FIFO_FULL))
				;

			/* send byte to FIFO :: WARNING don't use set field!! */
			ChipSetOneRegister(pParams->hDemod,
					   RSTV0910_P1_DISTXFIFO, Data[i]);
			i++;
		}
		ChipSetField(pParams->hDemod, FSTV0910_P1_DIS_PRECHARGE, 0);

		i = 0;
		while ((ChipGetField(pParams->hDemod, FSTV0910_P1_TX_IDLE) !=
					1) && (i < 10)) {
			/*wait until the end of diseqc send operation */
			ChipWaitOrAbort(pParams->hDemod, 10);
			i++;
		}

		ChipSetField(pParams->hDemod, FSTV0910_P1_DISRX_ON, 1);

		/*Check the error at the end of the function */
		if ((pParams->hDemod->Error) || (pParams->hTuner1->Error))
			error = FE_LLA_I2C_ERROR;

		break;

	case FE_SAT_DEMOD_2:

		if ((pParams->hDemod->Error) || (pParams->hTuner2->Error))
			return FE_LLA_I2C_ERROR;

		ChipSetField(pParams->hDemod, FSTV0910_P2_DISRX_ON, 0);
		ChipSetField(pParams->hDemod, FSTV0910_P2_DIS_PRECHARGE, 1);

		while (i < NbData) {
			/* wait for FIFO empty */
			while (ChipGetField(pParams->hDemod,
						FSTV0910_P2_TX_FIFO_FULL))
				;

			/* send byte to FIFO :: WARNING don't use set field!! */
			ChipSetOneRegister(pParams->hDemod,
					   RSTV0910_P2_DISTXFIFO,
					   Data[i]);
			i++;
		}
		ChipSetField(pParams->hDemod,
				FSTV0910_P2_DIS_PRECHARGE, 0);

		i = 0;
		while ((ChipGetField(pParams->hDemod, FSTV0910_P2_TX_IDLE) !=
					1) && (i < 10)) {
			/*wait until the end of diseqc send operation */
			ChipWaitOrAbort(pParams->hDemod, 10);
			i++;
		}
		ChipSetField(pParams->hDemod, FSTV0910_P2_DISRX_ON, 1);

		/*Check the error at the end of the function */
		if ((pParams->hDemod->Error) || (pParams->hTuner2->Error))
			error = FE_LLA_I2C_ERROR;
		break;
	}

	return error;
}
EXPORT_SYMBOL(FE_STV0910_DiseqcSend);

/*****************************************************
  --FUNCTION	::	FE_STV0910_DiseqcReceive
  --ACTION	::	Read receved bytes from DiseqC FIFO
  --PARAMS IN	::	Handle	==>	Front End Handle
  --PARAMS OUT::	Data	==> Table of received bytes.
  ::	NbData	==> Number of received bytes.
  ::	Demod	==>	Current demod 1 or 2
  --RETURN	::	Error(if any)
  --***************************************************/
/*! \fn FE_STV0910_Error_t FE_STV0910_DiseqcReceive(FE_STV0910_Handle_t Handle,
  U8 *Data,
  U32 *NbData,
  FE_STV0910_DEMOD_t Demod);
  \brief This public function is used to manage the Diseqc data reception
  */
FE_STV0910_Error_t FE_STV0910_DiseqcReceive(FE_STV0910_Handle_t Handle,
					    U8 *Data, U32 *NbData,
					    FE_STV0910_DEMOD_t Demod)
{
	FE_STV0910_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0910_InternalParams_t *pParams = NULL;
	S32 i = 0, j = 0;

	pParams = (FE_STV0910_InternalParams_t *) Handle;
	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		if ((pParams->hDemod->Error) || (pParams->hTuner1->Error))
			return FE_LLA_I2C_ERROR;

		ChipSetField(pParams->hDemod, FSTV0910_P1_EXTENVELOP, 0);
		ChipSetField(pParams->hDemod, FSTV0910_P1_IGNORE_SHORT22K, 1);

		*NbData = 0;

		ChipGetField(pParams->hDemod, FSTV0910_P1_RXACTIVE);

		while ((ChipGetField(pParams->hDemod, FSTV0910_P1_RXEND) != 1)
				&& (i < 10)) {
			ChipWaitOrAbort(pParams->hDemod, 10);
			/*wait while the RX is activity */
			while ((ChipGetField(pParams->hDemod,
				      FSTV0910_P1_RXACTIVE) == 1) && (j < 10)) {
				ChipWaitOrAbort(pParams->hDemod, 5);
				j++;
			}
			i++;
		}

		*NbData = ChipGetField(pParams->hDemod,
				FSTV0910_P1_RXFIFO_BYTES);
		for (i = 0; i < (*NbData); i++)
			Data[i] = ChipGetOneRegister(pParams->hDemod,
					RSTV0910_P1_DISRXFIFO);

		/*Check the error at the end of the function */
		if ((pParams->hDemod->Error) || (pParams->hTuner1->Error))
			error = FE_LLA_I2C_ERROR;

		break;

	case FE_SAT_DEMOD_2:
		if ((pParams->hDemod->Error) || (pParams->hTuner2->Error))
			return FE_LLA_I2C_ERROR;

		ChipSetField(pParams->hDemod, FSTV0910_P2_EXTENVELOP, 0);
		ChipSetField(pParams->hDemod, FSTV0910_P2_IGNORE_SHORT22K, 1);

		*NbData = 0;

		ChipGetField(pParams->hDemod, FSTV0910_P2_RXACTIVE);
		while ((ChipGetField(pParams->hDemod, FSTV0910_P2_RXEND) != 1)
				&& (i < 10)) {
			ChipWaitOrAbort(pParams->hDemod, 10);
			/*wait while the RX is activity */
			while ((ChipGetField(pParams->hDemod,
				      FSTV0910_P2_RXACTIVE) == 1) && (j < 10)) {
				ChipWaitOrAbort(pParams->hDemod, 5);
				j++;
			}
			i++;
		}

		*NbData = ChipGetField(pParams->hDemod,
				FSTV0910_P2_RXFIFO_BYTES);
		for (i = 0; i < (*NbData); i++)
			Data[i] = ChipGetOneRegister(pParams->hDemod,
					RSTV0910_P2_DISRXFIFO);

		/*Check the error at the end of the function */
		if ((pParams->hDemod->Error) || (pParams->hTuner2->Error))
			error = FE_LLA_I2C_ERROR;

		break;

	}

	return error;
}
EXPORT_SYMBOL(FE_STV0910_DiseqcReceive);

/*****************************************************
  --FUNCTION	::	FE_STV0910_Set22KHzContinues
  --ACTION	::	enable/disable the diseqc 22Khz continues tone
  --PARAMS IN	::	Handle	==>	Front End Handle
  ::	Demod	==>	Current demod 1 or 2
  --PARAMS OUT::None
  --RETURN	::	Error(if any)
  --***************************************************/
/*! \fn FE_STV0910_Error_t FE_STV0910_Set22KHzContinues(FE_STV0910_Handle_t
				Handle, BOOL Enable, FE_STV0910_DEMOD_t Demod);
  \brief This public fn enables or disables the diseqc 22Khz continues tone
  */
FE_STV0910_Error_t FE_STV0910_Set22KHzContinues(FE_STV0910_Handle_t Handle,
						BOOL Enable,
						FE_STV0910_DEMOD_t Demod)
{
	FE_STV0910_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0910_InternalParams_t *pParams = NULL;

	pParams = (FE_STV0910_InternalParams_t *) Handle;
	if (pParams != NULL)
		;
	else
		error = FE_LLA_INVALID_HANDLE;

	return error;
}

/*********************************************/
#if 0				/* formerly excluded lines */
FE_STV0910_Error_t FE_STV0910_SetContinous_Tone(FE_STV0910_Handle_t Handle,
						STFRONTEND_LNBToneState_t
						tonestate,
						FE_Sat_DemodPath_t Path)
{
	FE_STV0910_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0910_InternalParams_t *pParams = NULL;
	pParams = (FE_STV0910_InternalParams_t *) Handle;

	/*First imagine only one path is in use, so fill same parameters for
	 * both path1 and path2 parameters from the current handle */
	/* Path1 parameters */

	if (pParams != NULL) {
		switch (Path) {
		case FE_SAT_DEMOD_1:
		default:

			if (tonestate != STFRONTEND_LNB_TONE_OFF)
				ChipSetField(pParams->hDemod,
					     FSTV0910_P1_DISEQC_MODE, 0x0);
			else
				ChipSetField(pParams->hDemod,
					     FSTV0910_P1_DISEQC_MODE, 0x2);

			break;

		case FE_SAT_DEMOD_2:

			if (tonestate != STFRONTEND_LNB_TONE_OFF)
				ChipSetField(pParams->hDemod,
					     FSTV0910_P2_DISEQC_MODE, 0x0);
			else
				ChipSetField(pParams->hDemod,
					     FSTV0910_P2_DISEQC_MODE, 0x2);

			break;
		}

	}

	return error;
}
#endif /* formerly  excluded lines */

/*****************************************************
  --FUNCTION	::	FE_STV0910_Term
  --ACTION	::	Terminate STV0910 chip connection
  --PARAMS IN	::	Handle	==>	Front End Handle
  --PARAMS OUT::	NONE
  --RETURN	::	Error(if any)
  --***************************************************/
/*! \fn FE_STV0910_Error_t	FE_STV0910_Term(FE_STV0910_Handle_t	Handle)
  \brief This public function is used to terminate the STV0910 chip connection.
  */
FE_STV0910_Error_t FE_STV0910_Term(FE_STV0910_Handle_t Handle)
{
	FE_STV0910_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0910_InternalParams_t *pParams = NULL;

	pParams = (FE_STV0910_InternalParams_t *) Handle;

	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;
#ifdef HOST_PC
#ifdef NO_GUI
	ChipClose(pParams->hDemod);
	FE_Sat_TunerTerm(pParams->hTuner1);
	FE_Sat_TunerTerm(pParams->hTuner2);
#endif

	if (Handle)
		free(pParams);
#endif
	return error;
}
