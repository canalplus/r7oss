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

Source file name : stv0900_drv.c
Author :

Low level driver for STV0900

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
#include "stv0900_util.h"
#include "stv0900_drv.h"
/* #include "globaldefs.h" */
#include <gen_macros.h>
#include <stvvglna.h>

#ifdef HOST_PC
#ifndef NO_GUI
#include "STV0900_GUI.h"
#include "Appl.h"
#include "Pnl_Report.h"
#include <formatio.h>
#include "UserPar.h"
#endif
#endif

#define STV0900_BLIND_SEARCH_AGC2_TH 700
#define STV0900_BLIND_SEARCH_AGC2_TH_CUT30 1000/*1400*/	/*Blind Optim */

#define STV0900_P1_IQPOWER_THRESHOLD  30
#define STV0900_P2_IQPOWER_THRESHOLD  30

/****************************************************************
 ****************************************************************
 ****************************************************************
 ****
 ***************************************************
 **		Static Data
 ***************************************************
 ****
 ****
 ****************************************************************
 ****************************************************************
 ****************************************************************/

/************************
Current LLA revision
*************************/
static const ST_Revision_t RevisionSTV0900 =
					"STV0900-LLA_REL_4.8_DECEMBER_2010";

/************************
DVBS1 and DSS C/N Look-Up table
*************************/

static FE_STV0900_LOOKUP_t FE_STV0900_S1_CN_LookUp = {
	52,
	{
	 {0, 8917},	/*C/N=-0dB */
	 {05, 8801},		/*C/N=0.5dB */
	 {10, 8667},		/*C/N=1.0dB */
	 {15, 8522},		/*C/N=1.5dB */
	 {20, 8355},		/*C/N=2.0dB */
	 {25, 8175},		/*C/N=2.5dB */
	 {30, 7979},		/*C/N=3.0dB */
	 {35, 7763},		/*C/N=3.5dB */
	 {40, 7530},		/*C/N=4.0dB */
	 {45, 7282},		/*C/N=4.5dB */
	 {50, 7026},		/*C/N=5.0dB */
	 {55, 6781},		/*C/N=5.5dB */
	 {60, 6514},		/*C/N=6.0dB */
	 {65, 6241},		/*C/N=6.5dB */
	 {70, 5965},		/*C/N=7.0dB */
	 {75, 5690},		/*C/N=7.5dB */
	 {80, 5424},		/*C/N=8.0dB */
	 {85, 5161},		/*C/N=8.5dB */
	 {90, 4902},		/*C/N=9.0dB */
	 {95, 4654},		/*C/N=9.5dB */
	 {100, 4417},		/*C/N=10.0dB */
	 {105, 4186},		/*C/N=10.5dB */
	 {110, 3968},		/*C/N=11.0dB */
	 {115, 3757},		/*C/N=11.5dB */
	 {120, 3558},		/*C/N=12.0dB */
	 {125, 3366},		/*C/N=12.5dB */
	 {130, 3185},		/*C/N=13.0dB */
	 {135, 3012},		/*C/N=13.5dB */
	 {140, 2850},		/*C/N=14.0dB */
	 {145, 2698},		/*C/N=14.5dB */
	 {150, 2550},		/*C/N=15.0dB */
	 {160, 2283},		/*C/N=16.0dB */
	 {170, 2042},		/*C/N=17.0dB */
	 {180, 1827},		/*C/N=18.0dB */
	 {190, 1636},		/*C/N=19.0dB */
	 {200, 1466},		/*C/N=20.0dB */
	 {210, 1315},		/*C/N=21.0dB */
	 {220, 1181},		/*C/N=22.0dB */
	 {230, 1064},		/*C/N=23.0dB */
	 {240, 960},		/*C/N=24.0dB */
	 {250, 869},		/*C/N=25.0dB */
	 {260, 792},		/*C/N=26.0dB */
	 {270, 724},		/*C/N=27.0dB */
	 {280, 665},		/*C/N=28.0dB */
	 {290, 616},		/*C/N=29.0dB */
	 {300, 573},		/*C/N=30.0dB */
	 {310, 537},		/*C/N=31.0dB */
	 {320, 507},		/*C/N=32.0dB */
	 {330, 483},		/*C/N=33.0dB */
	 {400, 398},		/*C/N=40.0dB */
	 {450, 381},		/*C/N=45.0dB */
	 {500, 377}		/*C/N=50.0dB */
	 }
};

/************************
DVBS2 C/N Look-Up table
*************************/
static FE_STV0900_LOOKUP_t FE_STV0900_S2_CN_LookUp = {
	55,
	{
	 {-30, 13348},		/*C/N=-3dB */
	 {-20, 12640},		/*C/N=-2dB */
	 {-10, 11883},		/*C/N=-1dB */
	 {0, 11101},		/*C/N=-0dB */
	 {05, 10718},		/*C/N=0.5dB */
	 {10, 10339},		/*C/N=1.0dB */
	 {15, 9947},		/*C/N=1.5dB */
	 {20, 9552},		/*C/N=2.0dB */
	 {25, 9183},		/*C/N=2.5dB */
	 {30, 8799},		/*C/N=3.0dB */
	 {35, 8422},		/*C/N=3.5dB */
	 {40, 8062},		/*C/N=4.0dB */
	 {45, 7707},		/*C/N=4.5dB */
	 {50, 7353},		/*C/N=5.0dB */
	 {55, 7025},		/*C/N=5.5dB */
	 {60, 6684},		/*C/N=6.0dB */
	 {65, 6331},		/*C/N=6.5dB */
	 {70, 6036},		/*C/N=7.0dB */
	 {75, 5727},		/*C/N=7.5dB */
	 {80, 5437},		/*C/N=8.0dB */
	 {85, 5164},		/*C/N=8.5dB */
	 {90, 4902},		/*C/N=9.0dB */
	 {95, 4653},		/*C/N=9.5dB */
	 {100, 4408},		/*C/N=10.0dB */
	 {105, 4187},		/*C/N=10.5dB */
	 {110, 3961},		/*C/N=11.0dB */
	 {115, 3751},		/*C/N=11.5dB */
	 {120, 3558},		/*C/N=12.0dB */
	 {125, 3368},		/*C/N=12.5dB */
	 {130, 3191},		/*C/N=13.0dB */
	 {135, 3017},		/*C/N=13.5dB */
	 {140, 2862},		/*C/N=14.0dB */
	 {145, 2710},		/*C/N=14.5dB */
	 {150, 2565},		/*C/N=15.0dB */
	 {160, 2300},		/*C/N=16.0dB */
	 {170, 2058},		/*C/N=17.0dB */
	 {180, 1849},		/*C/N=18.0dB */
	 {190, 1663},		/*C/N=19.0dB */
	 {200, 1495},		/*C/N=20.0dB */
	 {210, 1349},		/*C/N=21.0dB */
	 {220, 1222},		/*C/N=22.0dB */
	 {230, 1110},		/*C/N=23.0dB */
	 {240, 1011},		/*C/N=24.0dB */
	 {250, 925},		/*C/N=25.0dB */
	 {260, 853},		/*C/N=26.0dB */
	 {270, 789},		/*C/N=27.0dB */
	 {280, 734},		/*C/N=28.0dB */
	 {290, 690},		/*C/N=29.0dB */
	 {300, 650},		/*C/N=30.0dB */
	 {310, 619},		/*C/N=31.0dB */
	 {320, 593},		/*C/N=32.0dB */
	 {330, 571},		/*C/N=33.0dB */
	 {400, 498},		/*C/N=40.0dB */
	 {450, 484},		/*C/N=45.0dB */
	 {500, 481}		/*C/N=50.0dB */
	 }
};

/************************
RF level Look-Up table
*************************/
static FE_STV0900_LOOKUP_t FE_STV0900_RF_LookUp = {
	14,
	{
	 {-5, 0xCCA0},	/*-5dBm*/
	 {-10, 0xC6BF},	/*-10dBm*/
	 {-15, 0xC09F},	/*-15dBm*/
	 {-20, 0xBA5F},	/*-20dBm*/
	 {-25, 0xB3DF},	/*-25dBm*/
	 {-30, 0xAD1F},	/*-30dBm*/
	 {-35, 0xA5F0},	/*-35dBm*/
	 {-40, 0x9E40},	/*-40dBm*/
	 {-45, 0x950C},	/*-45dBm*/
	 {-50, 0x8BFF},	/*-50dBm*/
	 {-55, 0x7F5F},	/*-55dBm*/
	 {-60, 0x7170},	/*-60dBm*/
	 {-65, 0x5BDD},	/*-65dBm*/
	 {-70, 0x34C0}	/*-70dBm*/
	 }
};

static FE_STV0900_LOOKUP_t FE_STV0900_RF_LookUpLow = {
	5,
	{
	 {-70, 0x94},
	 {-75, 0x55},
	 {-80, 0x33},
	 {-85, 0x20},
	 {-90, 0x18},
	 }
};

static FE_STV0900_LOOKUP_t FE_STV0900_RF_LookUpVGLNA = {
	21,
	{
	 {-33, 42104},
	 {-34, 41775},
	 {-35, 41437},
	 {-36, 41152},
	 {-41, 39727},
	 {-46, 38495},
	 {-51, 37454},
	 {-56, 36463},
	 {-61, 35551},
	 {-66, 34607},
	 {-71, 33567},
	 {-76, 32415},
	 {-81, 31030},
	 {-86, 29392},
	 {-91, 27374},
	 {-96, 25200},
	 {-101, 23584},
	 {-106, 22651},
	 {-111, 22333},
	 {-116, 22219},
	 {-120, 0}
	 }
};

struct FE_STV0900_CarLoopOPtim_s {
	FE_STV0900_ModCod_t ModCode;
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

struct FE_STV0900_ShortFramesCarLoopOPtim_s {
	FE_STV0900_Modulation_t Modulation;
	U8 CarLoopCut12_2;	/* Cut 1.2,   SR<=3msps     */
	U8 CarLoopCut20_2;	/* Cut 2.0,   SR<3msps      */
	U8 CarLoopCut12_5;	/* Cut 1.2,   3<SR<=7msps   */
	U8 CarLoopCut20_5;	/* Cut 2.0,   3<SR<=7msps   */
	U8 CarLoopCut12_10;	/* Cut 1.2,   7<SR<=15msps  */
	U8 CarLoopCut20_10;	/* Cut 2.0,   7<SR<=15msps  */
	U8 CarLoopCut12_20;	/* Cut 1.2,   10<SR<=25msps */
	U8 CarLoopCut20_20;	/* Cut 2.0,   10<SR<=25msps */
	U8 CarLoopCut12_30;	/* Cut 1.2,   25<SR<=45msps */
	U8 CarLoopCut20_30;	/* Cut 2.0,   10<SR<=45msps */

};

struct FE_STV0900_ShortFramesCarLoopOPtimVsMod_s {
	FE_STV0900_Modulation_t Modulation;
	U8 CarLoop_2;		/* SR<3msps      */
	U8 CarLoop_5;		/* 3<SR<=7msps   */
	U8 CarLoop_10;		/* 7<SR<=15msps  */
	U8 CarLoop_20;		/* 10<SR<=25msps */
	U8 CarLoop_30;		/* 10<SR<=45msps */
};

/*********************************************************************
Cut 1.x Tracking carrier loop carrier QPSK 1/2 to 8PSK 9/10 long Frame
*********************************************************************/
#define NB_SAT_MODCOD_900CUT1X 14
static struct FE_STV0900_CarLoopOPtim_s
FE_STV0900_S2CarLoop[NB_SAT_MODCOD_900CUT1X] = {
/*Modcod, 2MPon, 2MPoff, 5MPon, 5MPoff, 10MPon, 10MPoff, 20MPon, 20MPoff,
 * 30MPon, 30MPoff, */
	{FE_SAT_QPSK_12, 0x1C, 0x0D, 0x1B, 0x2C, 0x3A, 0x1C, 0x2A, 0x3B, 0x2A,
	 0x1B},
	{FE_SAT_QPSK_35, 0x2C, 0x0D, 0x2B, 0x2C, 0x3A, 0x0C, 0x3A, 0x2B, 0x2A,
	 0x0B},
	{FE_SAT_QPSK_23, 0x2C, 0x0D, 0x2B, 0x2C, 0x0B, 0x0C, 0x3A, 0x1B, 0x2A,
	 0x3A},
	{FE_SAT_QPSK_34, 0x3C, 0x0D, 0x3B, 0x1C, 0x0B, 0x3B, 0x3A, 0x0B, 0x2A,
	 0x3A},
	{FE_SAT_QPSK_45, 0x3C, 0x0D, 0x3B, 0x1C, 0x0B, 0x3B, 0x3A, 0x0B, 0x2A,
	 0x3A},
	{FE_SAT_QPSK_56, 0x0D, 0x0D, 0x3B, 0x1C, 0x0B, 0x3B, 0x3A, 0x0B, 0x2A,
	 0x3A},
	{FE_SAT_QPSK_89, 0x0D, 0x0D, 0x3B, 0x1C, 0x1B, 0x3B, 0x3A, 0x0B, 0x2A,
	 0x3A},
	{FE_SAT_QPSK_910, 0x1D, 0x0D, 0x3B, 0x1C, 0x1B, 0x3B, 0x3A, 0x0B, 0x2A,
	 0x3A},
	{FE_SAT_8PSK_35, 0x29, 0x3B, 0x09, 0x2B, 0x38, 0x0B, 0x18, 0x1A, 0x08,
	 0x0A},
	{FE_SAT_8PSK_23, 0x0A, 0x3B, 0x29, 0x2B, 0x19, 0x0B, 0x38, 0x1A, 0x18,
	 0x0A},
	{FE_SAT_8PSK_34, 0x3A, 0x3B, 0x2A, 0x2B, 0x39, 0x0B, 0x19, 0x1A, 0x38,
	 0x0A},
	{FE_SAT_8PSK_56, 0x1B, 0x3B, 0x0B, 0x2B, 0x1A, 0x0B, 0x39, 0x1A, 0x19,
	 0x0A},
	{FE_SAT_8PSK_89, 0x3B, 0x3B, 0x0B, 0x2B, 0x2A, 0x0B, 0x39, 0x1A, 0x29,
	 0x39},
	{FE_SAT_8PSK_910, 0x3B, 0x3B, 0x0B, 0x2B, 0x2A, 0x0B, 0x39, 0x1A, 0x29,
	 0x39}
};

/*********************************************************************
Cut 2.0 Tracking carrier loop carrier QPSK 1/4 to 32APSK 9/10 long Frame
*********************************************************************/
#define NB_SAT_MODCOD_900CUT2_CUT3 28
static struct FE_STV0900_CarLoopOPtim_s
FE_STV0900_S2CarLoopCut20[NB_SAT_MODCOD_900CUT2_CUT3] = {
/*Modcod, 2MPon, 2MPoff, 5MPon, 5MPoff, 10MPon, 10MPoff, 20MPon, 20MPoff,
 * 30MPon, 30MPoff*/
	{FE_SAT_QPSK_14, 0x0F, 0x3F, 0x0E, 0x3F, 0x2D, 0x2F, 0x2D, 0x1F, 0x3D,
	 0x3E},
	{FE_SAT_QPSK_13, 0x0F, 0x3F, 0x0E, 0x3F, 0x2D, 0x2F, 0x3D, 0x0F, 0x3D,
	 0x2E},
	{FE_SAT_QPSK_25, 0x1F, 0x3F, 0x1E, 0x3F, 0x3D, 0x1F, 0x3D, 0x3E, 0x3D,
	 0x2E},
	{FE_SAT_QPSK_12, 0x1F, 0x3F, 0x1E, 0x3F, 0x3D, 0x1F, 0x3D, 0x3E, 0x3D,
	 0x1E},
	{FE_SAT_QPSK_35, 0x2F, 0x3F, 0x2E, 0x2F, 0x3D, 0x0F, 0x0E, 0x2E, 0x3D,
	 0x0E},
	{FE_SAT_QPSK_23, 0x2F, 0x3F, 0x2E, 0x2F, 0x0E, 0x0F, 0x0E, 0x1E, 0x3D,
	 0x3D},
	{FE_SAT_QPSK_34, 0x3F, 0x3F, 0x3E, 0x1F, 0x0E, 0x3E, 0x0E, 0x1E, 0x3D,
	 0x3D},
	{FE_SAT_QPSK_45, 0x3F, 0x3F, 0x3E, 0x1F, 0x0E, 0x3E, 0x0E, 0x1E, 0x3D,
	 0x3D},
	{FE_SAT_QPSK_56, 0x3F, 0x3F, 0x3E, 0x1F, 0x0E, 0x3E, 0x0E, 0x1E, 0x3D,
	 0x3D},
	{FE_SAT_QPSK_89, 0x3F, 0x3F, 0x3E, 0x1F, 0x1E, 0x3E, 0x0E, 0x1E, 0x3D,
	 0x3D},
	{FE_SAT_QPSK_910, 0x3F, 0x3F, 0x3E, 0x1F, 0x1E, 0x3E, 0x0E, 0x1E, 0x3D,
	 0x3D},
	{FE_SAT_8PSK_35, 0x3c, 0x0c, 0x1c, 0x3b, 0x0c, 0x3b, 0x2b, 0x2b, 0x1b,
	 0x2b},
	{FE_SAT_8PSK_23, 0x1d, 0x0c, 0x3c, 0x0c, 0x2c, 0x3b, 0x0c, 0x2b, 0x2b,
	 0x2b},
	{FE_SAT_8PSK_34, 0x0e, 0x1c, 0x3d, 0x0c, 0x0d, 0x3b, 0x2c, 0x3b, 0x0c,
	 0x2b},
	{FE_SAT_8PSK_56, 0x2e, 0x3e, 0x1e, 0x2e, 0x2d, 0x1e, 0x3c, 0x2d, 0x2c,
	 0x1d},
	{FE_SAT_8PSK_89, 0x3e, 0x3e, 0x1e, 0x2e, 0x3d, 0x1e, 0x0d, 0x2d, 0x3c,
	 0x1d},
	{FE_SAT_8PSK_910, 0x3e, 0x3e, 0x1e, 0x2e, 0x3d, 0x1e, 0x1d, 0x2d, 0x0d,
	 0x1d},
	{FE_SAT_16APSK_23, 0x0C, 0x0C, 0x0C, 0x0C, 0x1D, 0x0C, 0x3C, 0x0C, 0x2C,
	 0x0C},
	{FE_SAT_16APSK_34, 0x0C, 0x0C, 0x0C, 0x0C, 0x0E, 0x0C, 0x2D, 0x0C, 0x1D,
	 0x0C},
	{FE_SAT_16APSK_45, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x0C, 0x3D, 0x0C, 0x2D,
	 0x0C},
	{FE_SAT_16APSK_56, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x0C, 0x3D, 0x0C, 0x2D,
	 0x0C},
	{FE_SAT_16APSK_89, 0x0C, 0x0C, 0x0C, 0x0C, 0x2E, 0x0C, 0x0E, 0x0C, 0x3D,
	 0x0C},
	{FE_SAT_16APSK_910, 0x0C, 0x0C, 0x0C, 0x0C, 0x2E, 0x0C, 0x0E, 0x0C,
	 0x3D, 0x0C},
	{FE_SAT_32APSK_34, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C,
	 0x0C},
	{FE_SAT_32APSK_45, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C,
	 0x0C},
	{FE_SAT_32APSK_56, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C,
	 0x0C},
	{FE_SAT_32APSK_89, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C,
	 0x0C},
	{FE_SAT_32APSK_910, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C,
	 0x0C, 0x0C}
};

/************************************************************************
	Cut 2.0 Tracking carrier loop carrier  short Frame, cut 1.2 and 2.0
************************************************************************/
static
struct FE_STV0900_ShortFramesCarLoopOPtim_s FE_STV0900_S2ShortCarLoop[4] = {
/*Mod, 2M_cut1., 2M_cut2.0, 5M_cut1.2, 5M_cut2.0, 10M_cut1.2, 10M_cut2.0,
 * 20M_cut1.2, 20M_cut2.0, 30M_cut1.2, 30M_cut2.0*/
	{FE_SAT_MOD_QPSK, 0x3C, 0x2F, 0x2B, 0x2E, 0x0B, 0x0E, 0x3A, 0x0E, 0x2A,
	 0x3D},
	{FE_SAT_MOD_8PSK, 0x0B, 0x3E, 0x2A, 0x0E, 0x0A, 0x2D, 0x19, 0x0D, 0x09,
	 0x3C},
	{FE_SAT_MOD_16APSK, 0x1B, 0x1E, 0x1B, 0x1E, 0x1B, 0x1E, 0x3A, 0x3D,
	 0x2A, 0x2D},
	{FE_SAT_MOD_32APSK, 0x1B, 0x1E, 0x1B, 0x1E, 0x1B, 0x1E, 0x3A, 0x3D,
	 0x2A, 0x2D}
};

/*******************************************************************************
 ******************************************************************************/
/*  CUT 3.0 */
/*******************************************************************************
 ******************************************************************************/

/*********************************************************************
Cut 3.0 Tracking carrier loop carrier QPSK 1/2 to 8PSK 9/10 long Frame
*********************************************************************/
static struct FE_STV0900_CarLoopOPtim_s
FE_STV0900_S2CarLoopCut30[NB_SAT_MODCOD_900CUT2_CUT3] = {
/*Modcod, 2MPon, 2MPoff, 5MPon, 5MPoff, 10MPon, 10MPoff, 20MPon, 20MPoff,
 * 30MPon, 30MPoff*/
	{FE_SAT_QPSK_14, 0x0C, 0x3C, 0x0B, 0x3C, 0x2A, 0x2C, 0x2A, 0x1C, 0x3A,
	 0x3B},
	{FE_SAT_QPSK_13, 0x0C, 0x3C, 0x0B, 0x3C, 0x2A, 0x2C, 0x3A, 0x0C, 0x3A,
	 0x2B},
	{FE_SAT_QPSK_25, 0x1C, 0x3C, 0x1B, 0x3C, 0x3A, 0x1C, 0x3A, 0x3B, 0x3A,
	 0x2B},
	{FE_SAT_QPSK_12, 0x3C, 0x2C, 0x0C, 0x2C, 0x1B, 0x2C, 0x1B, 0x1C, 0x0B,
	 0x3B},
	{FE_SAT_QPSK_35, 0x0D, 0x0D, 0x0C, 0x0D, 0x1B, 0x3C, 0x1B, 0x1C, 0x0B,
	 0x3B},
	{FE_SAT_QPSK_23, 0x1D, 0x0D, 0x0C, 0x1D, 0x2B, 0x3C, 0x1B, 0x1C, 0x0B,
	 0x3B},
	{FE_SAT_QPSK_34, 0x1D, 0x1D, 0x0C, 0x1D, 0x2B, 0x3C, 0x1B, 0x1C, 0x0B,
	 0x3B},
	{FE_SAT_QPSK_45, 0x2D, 0x1D, 0x1C, 0x1D, 0x2B, 0x3C, 0x2B, 0x0C, 0x1B,
	 0x3B},
	{FE_SAT_QPSK_56, 0x2D, 0x1D, 0x1C, 0x1D, 0x2B, 0x3C, 0x2B, 0x0C, 0x1B,
	 0x3B},
	{FE_SAT_QPSK_89, 0x3D, 0x2D, 0x1C, 0x1D, 0x3B, 0x3C, 0x2B, 0x0C, 0x1B,
	 0x3B},
	{FE_SAT_QPSK_910, 0x3D, 0x2D, 0x1C, 0x1D, 0x3B, 0x3C, 0x2B, 0x0C, 0x1B,
	 0x3B},
	{FE_SAT_8PSK_35, 0x39, 0x19, 0x39, 0x19, 0x19, 0x19, 0x19, 0x19, 0x09,
	 0x19},
	{FE_SAT_8PSK_23, 0x2A, 0x39, 0x1A, 0x0A, 0x39, 0x0A, 0x29, 0x39, 0x29,
	 0x0A},
	{FE_SAT_8PSK_34, 0x0B, 0x3A, 0x0B, 0x0B, 0x3A, 0x1B, 0x1A, 0x0B, 0x1A,
	 0x3A},
	{FE_SAT_8PSK_56, 0x0C, 0x1B, 0x3B, 0x2B, 0x1B, 0x3B, 0x3A, 0x3B, 0x3A,
	 0x1B},
	{FE_SAT_8PSK_89, 0x2C, 0x2C, 0x2C, 0x1C, 0x2B, 0x0C, 0x0B, 0x3B, 0x0B,
	 0x1B},
	{FE_SAT_8PSK_910, 0x2C, 0x3C, 0x2C, 0x1C, 0x3B, 0x1C, 0x0B, 0x3B, 0x0B,
	 0x1B},
	{FE_SAT_16APSK_23, 0x0A, 0x0A, 0x0A, 0x0A, 0x1A, 0x0A, 0x39, 0x0A, 0x29,
	 0x0A},
	{FE_SAT_16APSK_34, 0x0A, 0x0A, 0x0A, 0x0A, 0x0B, 0x0A, 0x2A, 0x0A, 0x1A,
	 0x0A},
	{FE_SAT_16APSK_45, 0x0A, 0x0A, 0x0A, 0x0A, 0x1B, 0x0A, 0x3A, 0x0A, 0x2A,
	 0x0A},
	{FE_SAT_16APSK_56, 0x0A, 0x0A, 0x0A, 0x0A, 0x1B, 0x0A, 0x3A, 0x0A, 0x2A,
	 0x0A},
	{FE_SAT_16APSK_89, 0x0A, 0x0A, 0x0A, 0x0A, 0x2B, 0x0A, 0x0B, 0x0A, 0x3A,
	 0x0A},
	{FE_SAT_16APSK_910, 0x0A, 0x0A, 0x0A, 0x0A, 0x2B, 0x0A, 0x0B, 0x0A,
	 0x3A, 0x0A},
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

/************************************************************************
	Cut 3.0 Tracking carrier loop carrier  short Frame, cut 3.0
************************************************************************/
static struct
FE_STV0900_ShortFramesCarLoopOPtimVsMod_s FE_STV0900_S2ShortCarLoopCut30[4] = {
/*Mod, 2M_cut3.0, 5M_cut3.0, 10M_cut3.0, 20M_cut3.0, 30M_cut3.0, */
	{FE_SAT_MOD_QPSK, 0x2C, 0x2B, 0x0B, 0x0B, 0x3A},
	{FE_SAT_MOD_8PSK, 0x3B, 0x0B, 0x2A, 0x0A, 0x39},
	{FE_SAT_MOD_16APSK, 0x1B, 0x1B, 0x1B, 0x3A, 0x2A},
	{FE_SAT_MOD_32APSK, 0x1B, 0x1B, 0x1B, 0x3A, 0x2A}
};


/****************************************************************
****************************************************************
****************************************************************
****
***************************************************
**		PRIVATE ROUTINES
***************************************************
****
****
****************************************************************
****************************************************************
****************************************************************/


/*****************************************************
**FUNCTION	::	FE_STV0900_GetMclkFreq
**ACTION	::	Set the STV0900 master clock frequency
**PARAMS IN	::  hChip		==>	handle to the chip
**			ExtClk		==>	External clock frequency (Hz)
**PARAMS OUT::	NONE
**RETURN	::	Synthesizer frequency (Hz)
*****************************************************/
U32 FE_STV0900_GetMclkFreq(STCHIP_Handle_t hChip, U32 ExtClk)
{
	U32 mclk = 90000000, div = 0, ad_div = 0;

	/*******************************************
		If the Field SELX1RATIO == 1 then
			MasterClock = (M_DIV +1)*Quartz/4
		else
			MasterClock = (M_DIV +1)*Quartz/6
	********************************************/

	div = ChipGetField(hChip, FSTV0900_M_DIV);
	ad_div = ((ChipGetField(hChip, FSTV0900_SELX1RATIO) == 1) ? 4 : 6);

	mclk = (div + 1) * ExtClk / ad_div;

	return mclk;
}

/*****************************************************
**FUNCTION	::	FE_STV0900_GetErrorCount
**ACTION	::	return the number of errors from a given counter
**PARAMS IN	::  hChip		==>	handle to the chip
**			::	Counter		==>	used counter 1 or 2.
			::  Demod		==> cirrent demod 1 or 2.
**PARAMS OUT::	NONE
**RETURN	::	Synthesizer frequency (Hz)
*****************************************************/
U32 FE_STV0900_GetErrorCount(STCHIP_Handle_t hChip,
			     FE_STV0900_ERRORCOUNTER Counter,
			     FE_STV0900_DEMOD_t Demod)
{
	U32 lsb = 0, msb = 0, hsb = 0, errValue;

	S32 err1Field_hsb,
	    err1Field_msb,
	    err1Field_lsb,
	    err2Field_hsb, err2Field_msb, err2Field_lsb, err1Reg, err2Reg;

	/*Define Error fields depending of the used Demod */
	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		err1Field_hsb = FSTV0900_P1_ERR_CNT12;
		err1Field_msb = FSTV0900_P1_ERR_CNT11;
		err1Field_lsb = FSTV0900_P1_ERR_CNT10;
		err1Reg = RSTV0900_P1_ERRCNT12;

		err2Field_hsb = FSTV0900_P1_ERR_CNT22;
		err2Field_msb = FSTV0900_P1_ERR_CNT21;
		err2Field_lsb = FSTV0900_P1_ERR_CNT20;
		err2Reg = RSTV0900_P1_ERRCNT22;
		break;

	case FE_SAT_DEMOD_2:
		err1Field_hsb = FSTV0900_P2_ERR_CNT12;
		err1Field_msb = FSTV0900_P2_ERR_CNT11;
		err1Field_lsb = FSTV0900_P2_ERR_CNT10;
		err1Reg = RSTV0900_P2_ERRCNT12;

		err2Field_hsb = FSTV0900_P2_ERR_CNT22;
		err2Field_msb = FSTV0900_P2_ERR_CNT21;
		err2Field_lsb = FSTV0900_P2_ERR_CNT20;
		err2Reg = RSTV0900_P2_ERRCNT22;
		break;
	}

	/*Read the Error value */
	switch (Counter) {
	case FE_STV0900_COUNTER1:
	default:
		ChipGetRegisters(hChip, err1Reg, 3);
		hsb = ChipGetFieldImage(hChip, err1Field_hsb);
		msb = ChipGetFieldImage(hChip, err1Field_msb);
		lsb = ChipGetFieldImage(hChip, err1Field_lsb);
		break;

	case FE_STV0900_COUNTER2:
		ChipGetRegisters(hChip, err2Reg, 3);
		hsb = ChipGetFieldImage(hChip, err2Field_hsb);
		msb = ChipGetFieldImage(hChip, err2Field_msb);
		lsb = ChipGetFieldImage(hChip, err2Field_lsb);
		break;
	}
	/*printf("\n ErrorneousBytes is %x %x %x", hsb, msb, lsb); */
	/*Cupute the Error value 3 bytes (HSB, MSB, LSB) */
	errValue = (hsb << 16) + (msb << 8) + (lsb);
	return errValue;
}

/*****************************************************
**FUNCTION	::	STV0900_RepeaterFn  (First repeater)
**ACTION	::	Set the repeater On or OFF
**PARAMS IN	::  hChip		==>	handle to the chip
			::	State		==> repeater state On/Off.
**PARAMS OUT::	NONE
**RETURN	::	Error (if any)
*****************************************************/
#ifdef CHIP_STAPI
STCHIP_Error_t FE_STV0900_RepeaterFn(STCHIP_Handle_t hChip, BOOL State,
				     U8 *Buffer)
#else
STCHIP_Error_t FE_STV0900_RepeaterFn(STCHIP_Handle_t hChip, BOOL State)
#endif
{
	STCHIP_Error_t error = CHIPERR_NO_ERROR;

	if (hChip == NULL)
		return FE_LLA_INVALID_HANDLE;

	if (State == TRUE)
#ifdef CHIP_STAPI
		ChipFillRepeaterMessage(hChip, FSTV0900_P1_I2CT_ON, 1, Buffer);
#else
		ChipSetField(hChip, FSTV0900_P1_I2CT_ON, 1);
#endif

	return error;
}

/*****************************************************
**FUNCTION	::	FE_STV0900_Repeater2Fn (Second repeater)
**ACTION	::	Set the repeater On or OFF
**PARAMS IN	::  hChip		==>	handle to the chip
			::	State		==> repeater state On/Off.
**PARAMS OUT::	NONE
**RETURN	::	Error (if any)
*****************************************************/
#ifdef CHIP_STAPI
STCHIP_Error_t FE_STV0900_Repeater2Fn(STCHIP_Handle_t hChip, BOOL State,
				      U8 *Buffer)
#else
STCHIP_Error_t FE_STV0900_Repeater2Fn(STCHIP_Handle_t hChip, BOOL State)
#endif
{
	STCHIP_Error_t error = CHIPERR_NO_ERROR;

	if (hChip == NULL)
		return FE_LLA_INVALID_HANDLE;

	if (State == TRUE)
#ifdef CHIP_STAPI
		ChipFillRepeaterMessage(hChip, FSTV0900_P2_I2CT_ON, 1, Buffer);
#else
		ChipSetField(hChip, FSTV0900_P2_I2CT_ON, 1);
#endif

	return error;
}

/*****************************************************
--FUNCTION	::	CarrierWidth
--ACTION	::	Compute the width of the carrier
--PARAMS IN	::	SymbolRate->Symbol rate of carrier (Kbauds or Mbauds)
--			RollOff	->Rolloff * 100
--PARAMS OUT::	NONE
--RETURN	::	Width of the carrier (KHz or MHz)
--***************************************************/
U32 FE_STV0900_CarrierWidth(U32 SymbolRate, FE_STV0900_RollOff_t RollOff)
{
	U32 rolloff;
	U32 width;

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
	width = (SymbolRate + (SymbolRate * rolloff) / 100);
	return width;
}
/*****************************************************
--FUNCTION	::	TunerSetType
--ACTION	::	Set the STV0900 Tuner controller
--PARAMS IN	::	Chip Handel
--		::	Tuner Type (control type SW, AUTO 6100 , AUTO 600 or
			AUTO SATTUNER
		::	Path (path1 or 2)
--PARAMS OUT::	NONE
--RETURN	::
--***************************************************/
void FE_STV0900_SetTunerType(STCHIP_Handle_t hChip,
			     FE_STV0900_Tuner_t TunerType,
			     U8 I2cAddr,
			     U32 Reference,
			     U8 OutClkDiv, FE_STV0900_DEMOD_t Demod)
{
	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		switch (TunerType) {
		case FE_SAT_AUTO_STB6000:

			ChipSetOneRegister(hChip, RSTV0900_P1_TNRCFG, 0x1c);
			ChipSetOneRegister(hChip, RSTV0900_P1_TNRCFG2, 0x86);
			ChipSetOneRegister(hChip, RSTV0900_P1_TNRCFG3, 0x18);
			ChipSetOneRegister(hChip, RSTV0900_P1_TNRXTAL,
					   (Reference / 1000000));
			ChipSetOneRegister(hChip, RSTV0900_P1_TNRSTEPS, 0x05);
			ChipSetOneRegister(hChip, RSTV0900_P1_TNRGAIN, 0x17);
			ChipSetOneRegister(hChip, RSTV0900_P1_TNRADJ, 0x1f);
			ChipSetOneRegister(hChip, RSTV0900_P1_TNRCTL2, 0x0);

			ChipSetField(hChip, FSTV0900_P1_TUN_TYPE, 1);

			/*Set the ADC range to 2Vpp */
			ChipSetOneRegister(hChip, RSTV0900_TSTTNR1, 0x27);
			break;

		case FE_SAT_AUTO_STB6100:
			ChipSetOneRegister(hChip, RSTV0900_P1_TNRCFG, 0x3c);
			ChipSetOneRegister(hChip, RSTV0900_P1_TNRCFG2, 0x86);
			ChipSetOneRegister(hChip, RSTV0900_P1_TNRCFG3, 0x18);
			ChipSetOneRegister(hChip, RSTV0900_P1_TNRXTAL,
					   (Reference / 1000000));
			ChipSetOneRegister(hChip, RSTV0900_P1_TNRSTEPS, 0x05);
			ChipSetOneRegister(hChip, RSTV0900_P1_TNRGAIN, 0x17);
			ChipSetOneRegister(hChip, RSTV0900_P1_TNRADJ, 0x1f);
			ChipSetOneRegister(hChip, RSTV0900_P1_TNRCTL2, 0x0);

			ChipSetField(hChip, FSTV0900_P1_TUN_TYPE, 3);

			/*Set the ADC range to 2Vpp */
			ChipSetOneRegister(hChip, RSTV0900_TSTTNR1, 0x27);
			break;

		case FE_SAT_AUTO_STV6110:

			ChipSetOneRegister(hChip, RSTV0900_P1_TNRCFG, 0x4c);
			ChipSetOneRegister(hChip, RSTV0900_P1_TNRCFG2, 0x06);
			ChipSetOneRegister(hChip, RSTV0900_P1_TNRCFG3, 0x18);
			ChipSetOneRegister(hChip, RSTV0900_P1_TNRXTAL,
					   (Reference / 1000000));
			ChipSetOneRegister(hChip, RSTV0900_P1_TNRSTEPS, 0x05);
			ChipSetOneRegister(hChip, RSTV0900_P1_TNRGAIN, 0x41);
			ChipSetOneRegister(hChip, RSTV0900_P1_TNRADJ, 0);
			ChipSetOneRegister(hChip, RSTV0900_P1_TNRCTL2, 0x97);

			ChipSetField(hChip, FSTV0900_P1_TUN_TYPE, 4);
			switch (OutClkDiv) {
			case 1:
			default:
				ChipSetField(hChip, FSTV0900_P1_TUN_KDIVEN, 0);
				break;

			case 2:
				ChipSetField(hChip, FSTV0900_P1_TUN_KDIVEN, 1);
				break;

			case 4:
				ChipSetField(hChip, FSTV0900_P1_TUN_KDIVEN, 2);
				break;

			case 8:
				ChipSetField(hChip, FSTV0900_P1_TUN_KDIVEN, 3);
				break;
			}

			/*Set the ADC range to 1Vpp */
			ChipSetOneRegister(hChip, RSTV0900_TSTTNR1, 0x26);

			break;

		case FE_SAT_SW_TUNER:
		default:
			ChipSetField(hChip, FSTV0900_P1_TUN_TYPE, 6);
			break;
		}

		switch (I2cAddr) {
		case 0xC0:
		default:
			ChipSetField(hChip, FSTV0900_P1_TUN_MADDRESS, 0);
			break;

		case 0xC2:
			ChipSetField(hChip, FSTV0900_P1_TUN_MADDRESS, 1);
			break;

		case 0xC4:
			ChipSetField(hChip, FSTV0900_P1_TUN_MADDRESS, 2);
			break;

		case 0xC6:
			ChipSetField(hChip, FSTV0900_P1_TUN_MADDRESS, 3);
			break;
		}
		ChipSetOneRegister(hChip, RSTV0900_P1_TNRLD, 1);

		break;

	case FE_SAT_DEMOD_2:
		switch (TunerType) {
		case FE_SAT_AUTO_STB6000:

			ChipSetOneRegister(hChip, RSTV0900_P2_TNRCFG, 0x1c);
			ChipSetOneRegister(hChip, RSTV0900_P2_TNRCFG2, 0x06);
			ChipSetOneRegister(hChip, RSTV0900_P2_TNRCFG3, 0x18);
			ChipSetOneRegister(hChip, RSTV0900_P2_TNRXTAL,
					   (Reference / 1000000));
			ChipSetOneRegister(hChip, RSTV0900_P2_TNRSTEPS, 0x05);
			ChipSetOneRegister(hChip, RSTV0900_P2_TNRGAIN, 0x17);
			ChipSetOneRegister(hChip, RSTV0900_P2_TNRADJ, 0x1f);
			ChipSetOneRegister(hChip, RSTV0900_P2_TNRCTL2, 0x0);

			ChipSetField(hChip, FSTV0900_P2_TUN_TYPE, 1);

			/*Set the ADC range to 2Vpp */
			ChipSetOneRegister(hChip, RSTV0900_TSTTNR3, 0x27);
			break;

		case FE_SAT_AUTO_STB6100:
			ChipSetOneRegister(hChip, RSTV0900_P2_TNRCFG, 0x3c);
			ChipSetOneRegister(hChip, RSTV0900_P2_TNRCFG2, 0x06);
			ChipSetOneRegister(hChip, RSTV0900_P2_TNRCFG3, 0x18);
			ChipSetOneRegister(hChip, RSTV0900_P2_TNRXTAL,
					   (Reference / 1000000));
			ChipSetOneRegister(hChip, RSTV0900_P2_TNRSTEPS, 0x05);
			ChipSetOneRegister(hChip, RSTV0900_P2_TNRGAIN, 0x17);
			ChipSetOneRegister(hChip, RSTV0900_P2_TNRADJ, 0x1f);
			ChipSetOneRegister(hChip, RSTV0900_P2_TNRCTL2, 0x0);

			ChipSetField(hChip, FSTV0900_P2_TUN_TYPE, 3);

			/*Set the ADC range to 2Vpp */
			ChipSetOneRegister(hChip, RSTV0900_TSTTNR3, 0x27);
			break;

		case FE_SAT_AUTO_STV6110:

			ChipSetOneRegister(hChip, RSTV0900_P2_TNRCFG, 0x4c);
			ChipSetOneRegister(hChip, RSTV0900_P2_TNRCFG2, 0x86);
			ChipSetOneRegister(hChip, RSTV0900_P2_TNRCFG3, 0x18);
			ChipSetOneRegister(hChip, RSTV0900_P2_TNRXTAL,
					   (Reference / 1000000));
			ChipSetOneRegister(hChip, RSTV0900_P2_TNRSTEPS, 0x05);
			ChipSetOneRegister(hChip, RSTV0900_P2_TNRGAIN, 0x41);
			ChipSetOneRegister(hChip, RSTV0900_P2_TNRADJ, 0);
			ChipSetOneRegister(hChip, RSTV0900_P2_TNRCTL2, 0x97);

			ChipSetField(hChip, FSTV0900_P2_TUN_TYPE, 4);
			switch (OutClkDiv) {
			case 1:
			default:
				ChipSetField(hChip, FSTV0900_P2_TUN_KDIVEN, 0);
				break;

			case 2:
				ChipSetField(hChip, FSTV0900_P2_TUN_KDIVEN, 1);
				break;

			case 4:
				ChipSetField(hChip, FSTV0900_P2_TUN_KDIVEN, 2);
				break;

			case 8:
				ChipSetField(hChip, FSTV0900_P2_TUN_KDIVEN, 3);
				break;
			}

			/*Set the ADC range to 1Vpp */
			ChipSetOneRegister(hChip, RSTV0900_TSTTNR3, 0x26);
			break;

		case FE_SAT_SW_TUNER:
		default:
			ChipSetField(hChip, FSTV0900_P2_TUN_TYPE, 6);
			break;
		}

		switch (I2cAddr) {
		case 0xC0:
		default:
			ChipSetField(hChip, FSTV0900_P2_TUN_MADDRESS, 0);
			break;

		case 0xC2:
			ChipSetField(hChip, FSTV0900_P2_TUN_MADDRESS, 1);
			break;

		case 0xC4:
			ChipSetField(hChip, FSTV0900_P2_TUN_MADDRESS, 2);
			break;

		case 0xC6:
			ChipSetField(hChip, FSTV0900_P2_TUN_MADDRESS, 3);
			break;
		}
		ChipSetOneRegister(hChip, RSTV0900_P2_TNRLD, 1);
		break;

	}
}

/*****************************************************
--FUNCTION	::	TunerSetBBGain
--ACTION	::	Set the Tuner bbgain using the 900 tuner controller
--PARAMS IN	::	Chip Handel
--		::	Tuner Type (control type SW, AUTO 6100 , AUTO 600 or
			AUTO SATTUNER
		::  bbgain in dB
		::	Path (path1 or 2)
--PARAMS OUT::	NONE
--RETURN	::
--***************************************************/
void FE_STV0900_TunerSetBBGain(STCHIP_Handle_t hChip,
			       FE_STV0900_Tuner_t TunerType,
			       S32 BBGain, FE_STV0900_DEMOD_t Demod)
{
	S32 bbGainField, updateReg;

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		bbGainField = FSTV0900_P1_TUN_GAIN;
		updateReg = RSTV0900_P1_TNRLD;
		break;

	case FE_SAT_DEMOD_2:
		bbGainField = FSTV0900_P2_TUN_GAIN;
		updateReg = RSTV0900_P2_TNRLD;
		break;
	}

	switch (TunerType) {
	case FE_SAT_AUTO_STB6000:
	case FE_SAT_AUTO_STB6100:
		ChipSetField(hChip, bbGainField, (BBGain / 2) + 7);
		ChipSetOneRegister(hChip, updateReg, 1);

		break;

	case FE_SAT_AUTO_STV6110:
		ChipSetField(hChip, bbGainField, (BBGain / 2));
		ChipSetOneRegister(hChip, updateReg, 1);

		break;

	case FE_SAT_SW_TUNER:
	default:
		break;
	}
}

/*****************************************************
--FUNCTION	::	GetTunerFrequency
--ACTION	::	Return the current tuner frequency (KHz)
--PARAMS IN	::	Chip Handel
			::	Tuner Handel
			::	Tuner Type
			::	Demod used 1 or 2
--PARAMS OUT::	NONE
--RETURN	::	Tuner Frequency  in KHz.
--***************************************************/
U32 FE_STV0900_GetTunerFrequency(STCHIP_Handle_t hChip,
				 TUNER_Handle_t hTuner,
				 FE_STV0900_Tuner_t TunerType,
				 FE_STV0900_DEMOD_t Demod)
{
	U32 tunerFrequency;

	S32 freq, round,
	    freqField2,
	    freqField1,
	    freqField0,
	    roundField1, roundField0, tunI2CackField, freqReg, roundReg;

	switch (TunerType) {
	case FE_SAT_AUTO_STB6000:
	case FE_SAT_AUTO_STB6100:
	case FE_SAT_AUTO_STV6110:
		switch (Demod) {
		case FE_SAT_DEMOD_1:
		default:
			/*      Tuner frequency, byte 2 */
			freqField2 = FSTV0900_P1_TUN_RFFREQ2;
			/*      Tuner frequency, byte 1 */
			freqField1 = FSTV0900_P1_TUN_RFFREQ1;
			/*      Tuner frequency, byte 0 */
			freqField0 = FSTV0900_P1_TUN_RFFREQ0;
			freqReg = RSTV0900_P1_TNRRF1;

			/*      Tuner granularity offset, byte 1        */
			roundField1 = FSTV0900_P1_TUN_RFRESTE1;
			/*      Tuner granularity offset, byte 0        */
			roundField0 = FSTV0900_P1_TUN_RFRESTE0;
			roundReg = RSTV0900_P1_TNROBSL;

			tunI2CackField = FSTV0900_P1_TUN_ACKFAIL;

			break;

		case FE_SAT_DEMOD_2:
			/*      Tuner frequency, byte 2 */
			freqField2 = FSTV0900_P2_TUN_RFFREQ2;
			/*      Tuner frequency, byte 1 */
			freqField1 = FSTV0900_P2_TUN_RFFREQ1;
			/*      Tuner frequency, byte 0 */
			freqField0 = FSTV0900_P2_TUN_RFFREQ0;
			freqReg = RSTV0900_P2_TNRRF1;

			/*      Tuner granularity offset, byte 1        */
			roundField1 = FSTV0900_P2_TUN_RFRESTE1;
			/*      Tuner granularity offset, byte 0        */
			roundField0 = FSTV0900_P2_TUN_RFRESTE0;
			roundReg = RSTV0900_P2_TNROBSL;

			tunI2CackField = FSTV0900_P2_TUN_ACKFAIL;
			break;

		}

		/*      Formulat :
		   Tuner_Frequency(MHz) = Regs /64
		   Tuner_granularity(MHz) = Regs  /2048

		   real_Tuner_Frequency = Tuner_Frequency(MHz) -
							Tuner_granularity(MHz)

		 */

		ChipGetRegisters(hChip, freqReg, 3);
		freq = (ChipGetFieldImage(hChip, freqField2) << 10) +
		    (ChipGetFieldImage(hChip, freqField1) << 2) +
		    ChipGetFieldImage(hChip, freqField0);

		freq = (freq * 1000) >> 6;

		ChipGetRegisters(hChip, roundReg, 2);
		round = (ChipGetFieldImage(hChip, roundField1) / 4) +
		    ChipGetFieldImage(hChip, roundField0);

		round = (round * 1000) >> 11;

		tunerFrequency = freq + round;

		if (ChipGetField(hChip, tunI2CackField)) {
			if (hTuner != NULL)
				hTuner->Error = CHIPERR_I2C_NO_ACK;
		}
		break;

	case FE_SAT_SW_TUNER:
	default:
		/* Read the tuner frequency by SW (SW Tuner API) */
		tunerFrequency = FE_Sat_TunerGetFrequency(hTuner);
		break;
	}

	return tunerFrequency;

}

/*****************************************************
--FUNCTION	::	SetTuner
--ACTION	::	Set tuner frequency (KHz) and the low pass filter
			(Bandwidth in Hz)
--PARAMS IN	::	Chip Handel
		::	Tuner Handel
		::	Tuner Type
		::	Demod used 1 or 2
		::	Frequency (Khz)
		::	Bandwidth (Hz)
--PARAMS OUT::	NONE
--RETURN	::	None.
--***************************************************/
void FE_STV0900_SetTuner(STCHIP_Handle_t hChip,
			 TUNER_Handle_t hTuner,
			 FE_STV0900_Tuner_t TunerType,
			 FE_STV0900_DEMOD_t Demod, U32 Frequency, U32 Bandwidth)
{
	U32 tunerFrequency;

	S32 freqField2,
	    freqField1, freqField0, bWField, updateReg, tunI2CackField, freqReg;

	switch (TunerType) {
	case FE_SAT_AUTO_STB6000:
	case FE_SAT_AUTO_STB6100:
	case FE_SAT_AUTO_STV6110:

		switch (Demod) {
		case FE_SAT_DEMOD_1:
		default:
			/*      Tuner frequency, byte 2 */
			freqField2 = FSTV0900_P1_TUN_RFFREQ2;
			/*      Tuner frequency, byte 1 */
			freqField1 = FSTV0900_P1_TUN_RFFREQ1;
			/*      Tuner frequency, byte 0 */
			freqField0 = FSTV0900_P1_TUN_RFFREQ0;
			/*      Tuner bandwidth field   */
			bWField = FSTV0900_P1_TUN_BW;
			/*      tuner write trig reg    */
			updateReg = RSTV0900_P1_TNRLD;
			tunI2CackField = FSTV0900_P1_TUN_ACKFAIL;

			freqReg = RSTV0900_P1_TNRRF1;
			break;

		case FE_SAT_DEMOD_2:
			/*      Tuner frequency, byte 2 */
			freqField2 = FSTV0900_P2_TUN_RFFREQ2;
			/*      Tuner frequency, byte 1 */
			freqField1 = FSTV0900_P2_TUN_RFFREQ1;
			/*      Tuner frequency, byte 0 */
			freqField0 = FSTV0900_P2_TUN_RFFREQ0;
			/*      Tuner bandwidth field   */
			bWField = FSTV0900_P2_TUN_BW;
			/*      tuner write trig reg    */
			updateReg = RSTV0900_P2_TNRLD;
			tunI2CackField = FSTV0900_P2_TUN_ACKFAIL;

			freqReg = RSTV0900_P2_TNRRF1;
			break;

		}

		/*      Formulat:
		   Tuner_frequency_reg= Frequency(MHz)*64
		 */
		tunerFrequency = (Frequency << 6) / 1000;

		ChipSetFieldImage(hChip, freqField2, (tunerFrequency >> 10));
		ChipSetFieldImage(hChip, freqField1,
				  (tunerFrequency >> 2) & 0xff);
		ChipSetFieldImage(hChip, freqField0, (tunerFrequency & 0x03));
		/*Low Pass Filter = BW /2 (MHz) */
		ChipSetFieldImage(hChip, bWField, Bandwidth / 2000000);
		/*Tuner Write trig      */
		ChipSetRegisters(hChip, freqReg, 3);
		ChipSetOneRegister(hChip, updateReg, 1);

		ChipWaitOrAbort(hChip, 2);
		if (ChipGetField(hChip, tunI2CackField)) {
			if (hTuner != NULL)
				hTuner->Error = CHIPERR_I2C_NO_ACK;
		}
		break;

	case FE_SAT_SW_TUNER:
	default:
		/*      Set tuner Frequency by SW       (SW Tuner API) */
		FE_Sat_TunerSetFrequency(hTuner, Frequency);
		/*      Set tuner BW by SW (SW tuner API)       */
		FE_Sat_TunerSetBandwidth(hTuner, Bandwidth);
		break;
	}

}

/*****************************************************
--FUNCTION	::	GetTunerStatus
--ACTION	::	Return the status of the tuner Locked or not
--PARAMS IN	::	Chip Handel
			::	Tuner Handel
			::	Tuner Type
			::	Demod used 1 or 2
--PARAMS OUT::	NONE
--RETURN	::	Boolean (tuner Lock status).
--***************************************************/
BOOL FE_STV0900_GetTunerStatus(STCHIP_Handle_t hChip,
			       TUNER_Handle_t hTuner,
			       FE_STV0900_Tuner_t TunerType,
			       FE_STV0900_DEMOD_t Demod)
{

	S32 lockField, tunI2CackField;
	BOOL lock;

	switch (TunerType) {
	case FE_SAT_AUTO_STB6000:
	case FE_SAT_AUTO_STB6100:
	case FE_SAT_AUTO_STV6110:
		/* case of automatic controled tuner
		   Read the tuner lock flag from the STV0900
		 */
		switch (Demod) {
		case FE_SAT_DEMOD_1:
		default:
			lockField = FSTV0900_P1_TUN_I2CLOCKED;
			tunI2CackField = FSTV0900_P1_TUN_ACKFAIL;
			break;

		case FE_SAT_DEMOD_2:
			lockField = FSTV0900_P2_TUN_I2CLOCKED;
			tunI2CackField = FSTV0900_P2_TUN_ACKFAIL;
			break;
		}

		if (ChipGetField(hChip, lockField) == 1)
			lock = TRUE;
		else
			lock = FALSE;

		if (ChipGetField(hChip, tunI2CackField)) {
			if (hTuner != NULL)
				hTuner->Error = CHIPERR_I2C_NO_ACK;
		}
		break;

	case FE_SAT_SW_TUNER:
	default:
		/*      case of SW controled tuner
		   Read the lock flag from the tuner (SW Tuner API)
		 */
		lock = FE_Sat_TunerGetStatus(hTuner);
		break;
	}
	return lock;
}

/*****************************************************
--FUNCTION	::	GetCarrierFrequency
--ACTION	::	Return the carrier frequency offset
--PARAMS IN	::	MasterClock	->	Masterclock frequency (Hz)
			::	Demod	->	current demod 1 or 2
--PARAMS OUT::	NONE
--RETURN	::	Frequency offset in Hz.
--***************************************************/
S32 FE_STV0900_GetCarrierFrequency(STCHIP_Handle_t hChip, U32 MasterClock,
				   FE_STV0900_DEMOD_t Demod)
{
	S32 cfrField2,
	    cfrField1,
	    cfrField0,
	    derot, rem1, rem2, intval1, intval2, carrierFrequency, cfrReg;

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		/*      carrier frequency Byte 2        */
		cfrField2 = FSTV0900_P1_CAR_FREQ2;
		/*      carrier frequency Byte 1        */
		cfrField1 = FSTV0900_P1_CAR_FREQ1;
		/*      carrier frequency Byte 0        */
		cfrField0 = FSTV0900_P1_CAR_FREQ0;
		cfrReg = RSTV0900_P1_CFR2;
		break;

	case FE_SAT_DEMOD_2:
		/*      carrier frequency Byte 2        */
		cfrField2 = FSTV0900_P2_CAR_FREQ2;
		/*      carrier frequency Byte 1        */
		cfrField1 = FSTV0900_P2_CAR_FREQ1;
		/*      carrier frequency Byte 0        */
		cfrField0 = FSTV0900_P2_CAR_FREQ0;
		cfrReg = RSTV0900_P2_CFR2;
		break;
	}

	/*      Read the carrier frequency regs value   */
	ChipGetRegisters(hChip, cfrReg, 3);
	derot = (ChipGetFieldImage(hChip, cfrField2) << 16) +
	    (ChipGetFieldImage(hChip, cfrField1) << 8) +
	    (ChipGetFieldImage(hChip, cfrField0));

	/*      cumpute the signed value        */
	derot = Get2Comp(derot, 24);

	/*
	   Formulat:
	   carrier_frequency = MasterClock * Reg / 2^24
	 */
	intval1 = MasterClock >> 12;
	intval2 = derot >> 12;

	rem1 = MasterClock % 0x1000;
	rem2 = derot % 0x1000;
	/*only for integer calculation */
	derot = (intval1 * intval2) + ((intval1 * rem2) >> 12) + ((intval2 *
				rem1) >> 12);

	carrierFrequency = derot;

	return carrierFrequency;
}

S32 FE_STV0900_TimingGetOffset(STCHIP_Handle_t hChip, U32 SymbolRate,
			       FE_STV0900_DEMOD_t Demod)
{
	S32 tmgreg, tmgField2, tmgField1, tmgField0, timingoffset;

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		/*Found timing offset reg HSB */
		tmgreg = RSTV0900_P1_TMGREG2;

		tmgField2 = FSTV0900_P1_TMGREG2;
		tmgField1 = FSTV0900_P1_TMGREG1;
		tmgField0 = FSTV0900_P1_TMGREG0;
		break;
	case FE_SAT_DEMOD_2:
		/*Found timing offset reg HSB */
		tmgreg = RSTV0900_P2_TMGREG2;

		tmgField2 = FSTV0900_P2_TMGREG2;
		tmgField1 = FSTV0900_P2_TMGREG1;
		tmgField0 = FSTV0900_P2_TMGREG0;

		break;
	}

	/*      Formulat :
	   SR_Offset = TMGRREG * SR /2^29
	   TMGREG is 3 bytes registers value
	   SR is the current symbol rate
	 */
	ChipGetRegisters(hChip, tmgreg, 3);

	timingoffset = (ChipGetFieldImage(hChip, tmgField2) << 16) +
	    (ChipGetFieldImage(hChip, tmgField1) << 8) +
	    (ChipGetFieldImage(hChip, tmgField0));

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
		::	MasterClock	->	Masterclock frequency (Hz)
		::	Demod		->	current demod 1 or 2
--PARAMS OUT::	NONE
--RETURN	::	Symbol rate in Symbol/s
*****************************************************/
U32 FE_STV0900_GetSymbolRate(STCHIP_Handle_t hChip, U32 MasterClock,
			     FE_STV0900_DEMOD_t Demod)
{
	S32 sfrField3,
	    sfrField2,
	    sfrField1,
	    sfrField0, rem1, rem2, intval1, intval2, symbolRate, sfrReg;

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		/*Found SR, byte 3 */
		sfrField3 = FSTV0900_P1_SYMB_FREQ3;
		/*Found SR, byte 2 */
		sfrField2 = FSTV0900_P1_SYMB_FREQ2;
		/*Found SR, byte 1 */
		sfrField1 = FSTV0900_P1_SYMB_FREQ1;
		/*Found SR, byte 0 */
		sfrField0 = FSTV0900_P1_SYMB_FREQ0;

		sfrReg = RSTV0900_P1_SFR3;
		break;
	case FE_SAT_DEMOD_2:
		/*Found SR, byte 3 */
		sfrField3 = FSTV0900_P2_SYMB_FREQ3;
		/*Found SR, byte 2 */
		sfrField2 = FSTV0900_P2_SYMB_FREQ2;
		/*Found SR, byte 1 */
		sfrField1 = FSTV0900_P2_SYMB_FREQ1;
		/*Found SR, byte 0 */
		sfrField0 = FSTV0900_P2_SYMB_FREQ0;

		sfrReg = RSTV0900_P2_SFR3;
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

	/*only for integer calculation */
	symbolRate = (intval1 * intval2) + ((intval1 * rem2) >> 16) + ((intval2
				* rem1) >> 16);

	return symbolRate;

}

/*****************************************************
--FUNCTION	::	SetSymbolRate
--ACTION	::	Get the Symbol rate
--PARAMS IN	::	hChip		->	handle to the chip
		::	MasterClock	->	Masterclock frequency (Hz)
		::	SymbolRate	->	Symbol Rate (Symbol/s)
		::	Demod		->	current demod 1 or 2
--PARAMS OUT::	NONE
--RETURN	::	None
*****************************************************/
void FE_STV0900_SetSymbolRate(STCHIP_Handle_t hChip, U32 MasterClock,
			      U32 SymbolRate, FE_STV0900_DEMOD_t Demod)
{
	S32 sfrInitReg;

	U32 symb;

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		/*Symbol rate init reg */
		sfrInitReg = RSTV0900_P1_SFRINIT1;
		break;
	case FE_SAT_DEMOD_2:
		/*Symbol rate init reg */
		sfrInitReg = RSTV0900_P2_SFRINIT1;
		break;
	}

	if (SymbolRate > 60000000) {
		/*SR >60Msps */

		/* Formulat :init_symbol_rate_regs = SR * 2^16/MasterClock */
		symb = SymbolRate << 4;
		symb /= (MasterClock >> 12);

		/*
		   equivalent to :
		   symb=(SymbolRate/2000)*65536;
		   symb/=(MasterClock/2000);
		 */
	} else if (SymbolRate > 6000000) {
		/* Formulat :init_symbol_rate_regs = SR * 2^16/MasterClock */
		symb = SymbolRate << 6;
		symb /= (MasterClock >> 10);

		/*
		   equivalent to :
		   symb=(SymbolRate/1000)*65536;
		   symb/=(MasterClock/1000); */
	} else {
		/* Formulat :init_symbol_rate_regs = SR * 2^16/MasterClock */
		symb = SymbolRate << 9;
		symb /= (MasterClock >> 7);

		/*
		   equivalent to :
		   symb=(SymbolRate/100)*65536;
		   symb/=(MasterClock/100);
		 */
	}

	/* Write the MSB */
	ChipSetOneRegister(hChip, sfrInitReg, (symb >> 8) & 0x7F);
	/* Write the LSB */
	ChipSetOneRegister(hChip, sfrInitReg + 1, (symb & 0xFF));

}

/*****************************************************
--FUNCTION	::	SetMaxSymbolRate
--ACTION	::	Get the Symbol rate
--PARAMS IN	::	hChip		->	handle to the chip
		::	MasterClock	->	Masterclock frequency (Hz)
		::	SymbolRate	->	Symbol Rate Max to SR + Msps
		::	Demod		->	current demod 1 or 2
--PARAMS OUT::	NONE
--RETURN	::	None
*****************************************************/
void FE_STV0900_SetMaxSymbolRate(STCHIP_Handle_t hChip, U32 MasterClock,
				 U32 SymbolRate, FE_STV0900_DEMOD_t Demod)
{
	S32 sfrMaxReg;
	U32 symb;

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		/*Symbol rate init reg */
		sfrMaxReg = RSTV0900_P1_SFRUP1;
		break;
	case FE_SAT_DEMOD_2:
		/*Symbol rate init reg */
		sfrMaxReg = RSTV0900_P2_SFRUP1;
		break;
	}

	SymbolRate = 105 * (SymbolRate / 100);

	if (SymbolRate > 60000000) {
		/*SR >60Msps */

		/* Formulat :init_symbol_rate_regs = SR * 2^16/MasterClock */
		symb = SymbolRate << 4;
		symb /= (MasterClock >> 12);

		/*
		   equivalent to :
		   symb=(SymbolRate/2000)*65536;
		   symb/=(MasterClock/2000);
		 */
	} else if (SymbolRate > 6000000) {
		/* Formulat :init_symbol_rate_regs = SR * 2^16/MasterClock */
		symb = SymbolRate << 6;
		symb /= (MasterClock >> 10);

		/*
		   equivalent to :
		   symb=(SymbolRate/1000)*65536;
		   symb/=(MasterClock/1000); */
	} else {
		/* Formulat :init_symbol_rate_regs = SR * 2^16/MasterClock */
		symb = SymbolRate << 9;
		symb /= (MasterClock >> 7);

		/*
		   equivalent to :
		   symb=(SymbolRate/100)*65536;
		   symb/=(MasterClock/100);
		 */
	}

	if (symb < 0x7FFF) {
		/* Write the MSB */
		ChipSetOneRegister(hChip, sfrMaxReg, (symb >> 8) & 0x7F);
		/* Write the LSB */
		ChipSetOneRegister(hChip, sfrMaxReg + 1, (symb & 0xFF));
	} else {
		/* Write the MSB */
		ChipSetOneRegister(hChip, sfrMaxReg, 0x7F);
		/* Write the LSB */
		ChipSetOneRegister(hChip, sfrMaxReg + 1, 0xFF);
	}

}

/*****************************************************
--FUNCTION	::	SetMinSymbolRate
--ACTION	::	Get the Symbol rate
--PARAMS IN	::	hChip		->handle to the chip
		::	MasterClock	->Masterclock frequency (Hz)
		::	SymbolRate	->Symbol Rate Min to SR *95% (Symbol/s)
		::	Demod		->current demod 1 or 2
--PARAMS OUT::	NONE
--RETURN	::	None
*****************************************************/
void FE_STV0900_SetMinSymbolRate(STCHIP_Handle_t hChip, U32 MasterClock,
				 U32 SymbolRate, FE_STV0900_DEMOD_t Demod)
{
	S32 sfrMinReg;

	U32 symb;

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		/*Symbol rate init reg */
		sfrMinReg = RSTV0900_P1_SFRLOW1;

		break;
	case FE_SAT_DEMOD_2:
		/*Symbol rate init reg */
		sfrMinReg = RSTV0900_P2_SFRLOW1;
		break;
	}

	SymbolRate = 95 * (SymbolRate / 100);

	if (SymbolRate > 60000000) {
		/*SR >60Msps */

		/* Formulat :init_symbol_rate_regs = SR * 2^16/MasterClock */
		symb = SymbolRate << 4;
		symb /= (MasterClock >> 12);

		/*
		   equivalent to :
		   symb=(SymbolRate/2000)*65536;
		   symb/=(MasterClock/2000);
		 */
	} else if (SymbolRate > 6000000) {
		/* Formulat :init_symbol_rate_regs = SR * 2^16/MasterClock */
		symb = SymbolRate << 6;
		symb /= (MasterClock >> 10);

		/*
		   equivalent to :
		   symb=(SymbolRate/1000)*65536;
		   symb/=(MasterClock/1000); */
	} else {
		/* Formulat :init_symbol_rate_regs = SR * 2^16/MasterClock */
		symb = SymbolRate << 9;
		symb /= (MasterClock >> 7);

		/*
		   equivalent to :
		   symb=(SymbolRate/100)*65536;
		   symb/=(MasterClock/100);
		 */
	}

	/* Write the MSB */
	ChipSetOneRegister(hChip, sfrMinReg, (symb >> 8) & 0x7F);
	/* Write the LSB */
	ChipSetOneRegister(hChip, sfrMinReg + 1, (symb & 0xFF));

}

/*****************************************************
--FUNCTION	::	GetStandard
--ACTION	::	Return the current standrad (DVBS1, DSS or DVBS2
--PARAMS IN	::	hChip		->	handle to the chip
			::	Demod		->	current demod 1 or 2
--PARAMS OUT::	NONE
--RETURN	::	standard (DVBS1, DVBS2 or DSS
*****************************************************/
FE_STV0900_TrackingStandard_t FE_STV0900_GetStandard(STCHIP_Handle_t hChip,
						     FE_STV0900_DEMOD_t Demod)
{
	FE_STV0900_TrackingStandard_t foundStandard;
	S32 stateField, dssdvbField, state;

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		/* state machine search status field */
		stateField = FSTV0900_P1_HEADER_MODE;
		dssdvbField = FSTV0900_P1_DSS_DVB;/* Viterbi standard field */
		break;
	case FE_SAT_DEMOD_2:
		/* state machine search status field */
		stateField = FSTV0900_P2_HEADER_MODE;
		dssdvbField = FSTV0900_P2_DSS_DVB;/* Viterbi standard field */
		break;
	}

	state = ChipGetField(hChip, stateField);

	if (state == 2)
		foundStandard = FE_SAT_DVBS2_STANDARD;/* The demod Find DVBS2 */

	else if (state == 3) {
		/*        The demod Find DVBS1/DSS */
		if (ChipGetField(hChip, dssdvbField) == 1)
			foundStandard = FE_SAT_DSS_STANDARD;/*Viterbi Find DSS*/
		else
			/* Viterbi Find DVBS1 */
			foundStandard = FE_SAT_DVBS1_STANDARD;
	} else
		/*      The demod find nothing, unknown standard        */
		foundStandard = FE_SAT_UNKNOWN_STANDARD;
	return foundStandard;
}

/*****************************************************
--FUNCTION	::	GetRFLevel
--ACTION	::	Return power of the signal
--PARAMS IN	::	hChip	->	handle to the chip
			::	lookup	->	LUT for RF level estimation.
			::	Demod	->	current demod 1 or 2
--PARAMS OUT::	NONE
--RETURN	::	Power of the signal (dBm), -100 if no signal
--***************************************************/
S32 FE_STV0900_GetRFLevel(STCHIP_Handle_t hChip, FE_STV0900_LOOKUP_t *lookup,
			  FE_STV0900_DEMOD_t Demod)
{
	S32 agcGain = 0, iqpower = 0, Imin, Imax, i, rfLevel = 0;

	if ((lookup == NULL) || !lookup->size)
		return rfLevel;

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		/* Read the AGC integrator registers (2 Bytes)  */
		ChipGetRegisters(hChip, RSTV0900_P1_AGCIQIN1, 2);
		agcGain = MAKEWORD(ChipGetFieldImage(hChip,
					FSTV0900_P1_AGCIQ_VALUE1),
				ChipGetFieldImage(hChip,
					FSTV0900_P1_AGCIQ_VALUE0));
		break;

	case FE_SAT_DEMOD_2:
		/* Read the AGC integrator registers (2 Bytes)  */
		ChipGetRegisters(hChip, RSTV0900_P2_AGCIQIN1, 2);
		agcGain = MAKEWORD(ChipGetFieldImage(hChip,
					FSTV0900_P2_AGCIQ_VALUE1),
				ChipGetFieldImage(hChip,
					FSTV0900_P2_AGCIQ_VALUE0));
		break;
	}
	Imin = 0;
	Imax = lookup->size - 1;

	if (agcGain > lookup->table[0].regval) {
		rfLevel = lookup->table[0].realval;
		return rfLevel;
	} else if (agcGain >= lookup->table[lookup->size - 1].regval) {
		while ((Imax - Imin) > 1) {
			i = (Imax + Imin) >> 1;
			/*equivalent to i=(Imax+Imin)/2; */
			if (INRANGE(lookup->table[Imin].regval, agcGain,
						lookup->table[i].regval))
				Imax = i;
			else
				Imin = i;
		}

		rfLevel = (((S32)agcGain - lookup->table[Imin].regval)
			   * (lookup->table[Imax].realval -
			      lookup->table[Imin].realval) /
			   (lookup->table[Imax].regval -
			    lookup->table[Imin].regval)) +
			lookup->table[Imin].realval;
		return rfLevel;
	}
	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		/* Read the AGC integrator registers (2 Bytes)  */
		ChipGetRegisters(hChip, RSTV0900_P1_POWERI, 2);
		iqpower = (ChipGetFieldImage(hChip, FSTV0900_P1_POWER_I) +
			ChipGetFieldImage(hChip, FSTV0900_P1_POWER_Q)) >> 1;
		break;

	case FE_SAT_DEMOD_2:
		/* Read the AGC integrator registers (2 Bytes)  */
		ChipGetRegisters(hChip, RSTV0900_P2_POWERI, 2);
		iqpower = (ChipGetFieldImage(hChip, FSTV0900_P2_POWER_I) +
			ChipGetFieldImage(hChip, FSTV0900_P2_POWER_Q)) >> 1;
		break;
	}

	if (iqpower > FE_STV0900_RF_LookUpLow.table[0].regval)
		rfLevel = FE_STV0900_RF_LookUpLow.table[0].regval;
	else if (iqpower >=
		 FE_STV0900_RF_LookUpLow.table[lookup->size - 1].regval) {
		while ((Imax - Imin) > 1) {
			i = (Imax + Imin) >> 1;
			/*equivalent to i=(Imax+Imin)/2; */
			if (INRANGE(FE_STV0900_RF_LookUpLow.table[Imin].regval,
			      iqpower, FE_STV0900_RF_LookUpLow.table[i].regval))
				Imax = i;
			else
				Imin = i;
		}

		rfLevel = (((S32)iqpower -
				FE_STV0900_RF_LookUpLow.table[Imin].regval) *
		     (FE_STV0900_RF_LookUpLow.table[Imax].realval -
				FE_STV0900_RF_LookUpLow.table[Imin].realval) /
		     (FE_STV0900_RF_LookUpLow.table[Imax].regval -
			      FE_STV0900_RF_LookUpLow.table[Imin].regval)) +
			FE_STV0900_RF_LookUpLow.table[Imin].realval;
	} else
		rfLevel =
			FE_STV0900_RF_LookUpLow.table[lookup->size - 1].realval;
	return rfLevel;
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
S32 FE_STV0900_CarrierGetQuality(STCHIP_Handle_t hChip,
				 FE_STV0900_LOOKUP_t *lookup,
				 FE_STV0900_DEMOD_t Demod)
{
	S32 c_n = -100,
	    regval,
	    Imin, Imax, i, lockFlagField, noiseField1, noiseField0, noiseReg;

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		lockFlagField = FSTV0900_P1_LOCK_DEFINITIF;
		if (FE_STV0900_GetStandard(hChip, Demod) ==
		    FE_SAT_DVBS2_STANDARD) {
			/*If DVBS2 use PLH normilized noise indicators */
			noiseField1 = FSTV0900_P1_NOSPLHT_NORMED1;
			noiseField0 = FSTV0900_P1_NOSPLHT_NORMED0;
			noiseReg = RSTV0900_P1_NNOSPLHT1;
		} else {
			/*if not DVBS2 use symbol normalized noise indicators */
			noiseField1 = FSTV0900_P1_NOSDATAT_NORMED1;
			noiseField0 = FSTV0900_P1_NOSDATAT_NORMED0;
			noiseReg = RSTV0900_P1_NNOSDATAT1;
		}
		break;

	case FE_SAT_DEMOD_2:
		lockFlagField = FSTV0900_P2_LOCK_DEFINITIF;

		if (FE_STV0900_GetStandard(hChip, Demod) ==
		    FE_SAT_DVBS2_STANDARD) {
			/*If DVBS2 use PLH normilized noise indicators */
			noiseField1 = FSTV0900_P2_NOSPLHT_NORMED1;
			noiseField0 = FSTV0900_P2_NOSPLHT_NORMED0;
			noiseReg = RSTV0900_P2_NNOSPLHT1;
		} else {
			/*if not DVBS2 use symbol normalized noise indicators */
			noiseField1 = FSTV0900_P2_NOSDATAT_NORMED1;
			noiseField0 = FSTV0900_P2_NOSDATAT_NORMED0;
			noiseReg = RSTV0900_P2_NNOSDATAT1;
		}
		break;
	}

	if (!ChipGetField(hChip, lockFlagField))
		return c_n;

	if ((lookup == NULL) || !lookup->size)
		return c_n;

	regval = 0;
	ChipWaitOrAbort(hChip, 5);
	for (i = 0; i < 16; i++) {
		ChipGetRegisters(hChip, noiseReg, 2);
		regval += MAKEWORD(ChipGetFieldImage(hChip, noiseField1),
				ChipGetFieldImage(hChip, noiseField0));
		ChipWaitOrAbort(hChip, 1);
	}
	regval /= 16;

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
			  lookup->table[Imin].realval) /
		       (lookup->table[Imax].regval -
			lookup->table[Imin].regval)) +
			lookup->table[Imin].realval;
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
U32 FE_STV0900_GetBer(STCHIP_Handle_t hChip, FE_STV0900_DEMOD_t Demod)
{
	U32 ber = 10000000, i;
	S32 demdStateReg,
	    demodState, vstatusReg, prVitField, pdelStatusReg, pdellockField;

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		demdStateReg = FSTV0900_P1_HEADER_MODE;
		vstatusReg = RSTV0900_P1_VSTATUSVIT;
		prVitField = FSTV0900_P1_PRFVIT;
		pdelStatusReg = RSTV0900_P1_PDELSTATUS1;
		pdellockField = FSTV0900_P1_PKTDELIN_LOCK;
		break;

	case FE_SAT_DEMOD_2:
		demdStateReg = FSTV0900_P2_HEADER_MODE;
		vstatusReg = RSTV0900_P2_VSTATUSVIT;
		prVitField = FSTV0900_P2_PRFVIT;
		pdelStatusReg = RSTV0900_P2_PDELSTATUS1;
		pdellockField = FSTV0900_P2_PKTDELIN_LOCK;
		break;
	}

	demodState = ChipGetField(hChip, demdStateReg);

	switch (demodState) {
	case FE_SAT_SEARCH:
	case FE_SAT_PLH_DETECTED:
	default:
		/*demod Not locked ber = 1 */
		ber = 10000000;
		break;

	case FE_SAT_DVBS_FOUND:

		ber = 0;
		/* Average 5 ber values */
		for (i = 0; i < 5; i++) {
			ChipWaitOrAbort(hChip, 5);
			ber +=
			    FE_STV0900_GetErrorCount(hChip, FE_STV0900_COUNTER1,
						     Demod);
		}

		ber /= 5;

		if (ChipGetField(hChip, prVitField)) {
			/*        Check for carrier       */
			/*ber = ber * 10^7/2^10 */
			ber *= 9766;
			/*  theses two lines => ber = ber * 10^7/2^20   */
			ber = ber >> 13;
		}
		break;

	case FE_SAT_DVBS2_FOUND:

		ber = 0;
		/* Average 5 ber values */
		for (i = 0; i < 5; i++) {
			ChipWaitOrAbort(hChip, 5);
			ber +=
			    FE_STV0900_GetErrorCount(hChip, FE_STV0900_COUNTER1,
						     Demod);
		}
		ber /= 5;

		if (ChipGetField(hChip, pdellockField)) {
			/*        Check for S2 FEC Lock   */
			/*      ber = ber * 10^7/2^10 */
			ber *= 9766;
			/*  theses two lines => PER = ber * 10^7/2^23   */
			ber = ber >> 13;
		}

		break;
	}

	return ber;
}

/*****************************************************
--FUNCTION	::	GetBer_BERMeter
--ACTION	::	Return the Viterbi BER if DVBS1/DSS
--PARAMS IN	::	hChip	->	handle to the chip
			::	Demod	->	current demod 1 or 2
--PARAMS OUT::	NONE
--RETURN	::	ber/per scalled to 1e7
--***************************************************/
U32 FE_STV0900_GetBer_BerMeter(FE_STV0900_Handle_t Handle,
			       FE_STV0900_DEMOD_t Demod, BOOL count)
{

	S32 ByteCountReg, errorCntrlReg2, berResetField, berErrResetField;

	FE_STV0900_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0900_InternalParams_t *pParams = NULL;
	U8 ByteCount4 = 0,
	    ByteCount3 = 0, ByteCount2 = 0, ByteCount1 = 0, ByteCount0 = 0;
	U32 ber = 0, bercount = 0, dummy_32 = 0;
	U64 berwindow = 0;

	pParams = (FE_STV0900_InternalParams_t *)Handle;

	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		ByteCountReg = RSTV0900_P1_FBERCPT4;
		errorCntrlReg2 = RSTV0900_P1_ERRCTRL1;
		berResetField = FSTV0900_P1_BERMETER_RESET;
		berErrResetField = RSTV0900_P1_ERRCNT12;
		if ((pParams->hDemod->Error) || (pParams->hTuner1->Error))
			error = FE_LLA_I2C_ERROR;

		break;

	case FE_SAT_DEMOD_2:
		ByteCountReg = RSTV0900_P2_FBERCPT4;
		errorCntrlReg2 = RSTV0900_P2_ERRCTRL1;
		berResetField = FSTV0900_P2_BERMETER_RESET;
		berErrResetField = RSTV0900_P2_ERRCNT12;

		if ((pParams->hDemod->Error) || (pParams->hTuner2->Error))
			error = FE_LLA_I2C_ERROR;

		break;

	}
	if (error != FE_LLA_NO_ERROR)
		return ber;

	if (FE_STV0900_Status(Handle, Demod) == FALSE) {
		/*if Demod+FEC not locked */
		ber = 0;
		return ber;
	}

	/*stop and freeze the counters */
	ChipSetField(pParams->hDemod, berResetField, 1);

	if (count != 0) {
		/* second and subsequenct call to GetTunerInfo calculate BER */

		/*Read the total packet counter 40 bits, read 5 bytes is
		 * mondatory */
		/*Read the Total packet counter byte 5 MSB */
		ByteCount4 = ChipGetOneRegister(pParams->hDemod, ByteCountReg);
		/*Read the Total packet counter byte 4 */
		ByteCount3 = ChipGetOneRegister(pParams->hDemod, ByteCountReg +
				1);
		/*Read the Total packet counter byte 3 */
		ByteCount2 = ChipGetOneRegister(pParams->hDemod, ByteCountReg +
				2);
		/*Read the Total packet counter byte 2 */
		ByteCount1 = ChipGetOneRegister(pParams->hDemod, ByteCountReg +
				3);
		/*Read the Total packet counter byte 1 LSB */
		ByteCount0 = ChipGetOneRegister(pParams->hDemod, ByteCountReg +
				4);
		/*printf("\nTotal number of Bytes %x %x %x %x %x\n",
		 * ByteCount4, ByteCount3, ByteCount2, ByteCount1, ByteCount0);
		 * */
		/*Use the counter for a maximum of 2^40 Bytes */

		if (ByteCount4 | ByteCount3 | ByteCount2 | ByteCount1 |
				ByteCount0) {
			berwindow = ((ByteCount3 << 24) |
					(ByteCount2 << 16) |
					(ByteCount1 << 8) | ByteCount0) * 8;
		} else {
			ber = 0;
			return ber;
		}
		bercount = FE_STV0900_GetErrorCount(pParams->hDemod,
				FE_STV0900_COUNTER1, Demod);

		/*Calculate the BER */

		/*10^7 is the granularity factor */
		/*the below operations lead to (bercount*10^7)/(TotalBytes *8)*/
		dummy_32 = (U32) (berwindow >> 10);
		/*this is approximation of dummy_32/10^4 */
		dummy_32 = dummy_32 / 10;
		ber = bercount * 100 / dummy_32; /*bercount*10^2/berwindow */
		ber = ber * 1024 / 100;

	} else {
		/*printf("\n drv.c the count is 0..theber =0\n"); */
		ber = 0;
	}
	/*Reset the Total Byte counter */
	ChipSetOneRegister(pParams->hDemod, ByteCountReg, 0);
	/*Reset the Total Byte counter */
	ChipSetOneRegister(pParams->hDemod, berErrResetField, 0);

	/* unfreeze the counters */
	ChipSetField(pParams->hDemod, berResetField, 0);

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

	return ber;
}

FE_STV0900_Rate_t FE_STV0900_GetViterbiPunctureRate(STCHIP_Handle_t hChip,
						    FE_STV0900_DEMOD_t Demod)
{
	S32 rateField;
	FE_STV0900_Rate_t punctureRate;

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		rateField = ChipGetField(hChip, FSTV0900_P1_VIT_CURPUN);
		break;

	case FE_SAT_DEMOD_2:
		rateField = ChipGetField(hChip, FSTV0900_P2_VIT_CURPUN);
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

void FE_STV0900_GetLockTimeout(S32 *DemodTimeout, S32 *FecTimeout,
			       S32 SymbolRate, FE_STV0900_SearchAlgo_t Algo)
{
	switch (Algo) {
	case FE_SAT_BLIND_SEARCH:

		if (SymbolRate <= 1500000) {
			/*10Msps< SR <=15Msps */
			(*DemodTimeout) = 1500;
			(*FecTimeout) = 400;
		} else if (SymbolRate <= 5000000) {
			/*10Msps< SR <=15Msps */
			(*DemodTimeout) = 1000;
			(*FecTimeout) = 300;
		} else if (SymbolRate <= 30000000) {
			/*5Msps< SR <=30Msps */
			(*DemodTimeout) = 700;
			(*FecTimeout) = 300;
		} else if (SymbolRate <= 45000000) {
			/*30Msps< SR <=45Msps */
			(*DemodTimeout) = 400;
			(*FecTimeout) = 200;
		} else {
			/*SR >45Msps */
			(*DemodTimeout) = 300;
			(*FecTimeout) = 100;
		}

		break;

	case FE_SAT_COLD_START:
	case FE_SAT_WARM_START:
	default:
		if (SymbolRate <= 1000000) {
			/*SR <=1Msps */
			(*DemodTimeout) = 3000;
			(*FecTimeout) = 1700;
		} else if (SymbolRate <= 2000000) {
			/*1Msps < SR <=2Msps */
			(*DemodTimeout) = 2500;
			(*FecTimeout) = 1100;
		} else if (SymbolRate <= 5000000) {
			/*2Msps< SR <=5Msps */
			(*DemodTimeout) = 1000;
			(*FecTimeout) = 550;
		} else if (SymbolRate <= 10000000) {
			/*5Msps< SR <=10Msps */
			(*DemodTimeout) = 700;
			(*FecTimeout) = 250;
		} else if (SymbolRate <= 20000000) {
			/*10Msps< SR <=20Msps */
			(*DemodTimeout) = 400;
			(*FecTimeout) = 130;
		}

		else {
			/*SR >20Msps */

			(*DemodTimeout) = 300;
			(*FecTimeout) = 100;
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

BOOL FE_STV0900_CheckTimingLock(STCHIP_Handle_t hChip, FE_STV0900_DEMOD_t Demod)
{
	BOOL timingLock = FALSE;
	S32 i, timingcpt = 0;
	U8 carFreq, tmgTHhigh, tmgTHLow;

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		carFreq = ChipGetOneRegister(hChip, RSTV0900_P1_CARFREQ);
		tmgTHhigh = ChipGetOneRegister(hChip, RSTV0900_P1_TMGTHRISE);
		tmgTHLow = ChipGetOneRegister(hChip, RSTV0900_P1_TMGTHFALL);

		/*use higher timing lock TH */
		ChipSetOneRegister(hChip, RSTV0900_P1_TMGTHRISE, 0x20);
		/*use higher timing lock TH */
		ChipSetOneRegister(hChip, RSTV0900_P1_TMGTHFALL, 0x0);

		/* stop the carrier offset search */
		ChipSetFieldImage(hChip, FSTV0900_P1_CFR_AUTOSCAN, 0);
		ChipSetRegisters(hChip, RSTV0900_P1_DMDCFGMD, 1);

		/*set the DVBS1 timing loop beta value to 0 (stop timing loop)
		 * */
		ChipSetOneRegister(hChip, RSTV0900_P1_RTC, 0x80);
		/*set the DVBS2 timing loop beta value to 0 (stop timing loop)
		 * */
		ChipSetOneRegister(hChip, RSTV0900_P1_RTCS2, 0x40);
		/*Stop the carrier loop */
		ChipSetOneRegister(hChip, RSTV0900_P1_CARFREQ, 0x0);

		ChipSetFieldImage(hChip, FSTV0900_P1_CFR_INIT1, 0);
		ChipSetFieldImage(hChip, FSTV0900_P1_CFR_INIT0, 0);
		/*init the demod frequency offset to 0 */
		ChipSetRegisters(hChip, RSTV0900_P1_CFRINIT1, 2);

		ChipSetOneRegister(hChip, RSTV0900_P1_AGC2REF, 0x65);
		/*Trig an acquisition (start the search) only for timing check
		 * no need to white for lock */
		ChipSetOneRegister(hChip, RSTV0900_P1_DMDISTATE, 0x18);
		ChipWaitOrAbort(hChip, 7);
		for (i = 0; i < 10; i++) {
			if (ChipGetField(hChip, FSTV0900_P1_TMGLOCK_QUALITY) >=
					2)
				/*read the timing lock quality */
				timingcpt++;
			ChipWaitOrAbort(hChip, 1);
		}
		if (timingcpt >= 3) /*timing locked when timing quality is
				      higer than 2 for 3 times */
			timingLock = TRUE;

		ChipSetOneRegister(hChip, RSTV0900_P1_AGC2REF, 0x38);
		/*enable the DVBS1 timing loop */
		ChipSetOneRegister(hChip, RSTV0900_P1_RTC, 0x88);
		/*enable the DVBS2 timing loop */
		ChipSetOneRegister(hChip, RSTV0900_P1_RTCS2, 0x68);

		/*enable the carrier loop */
		ChipSetOneRegister(hChip, RSTV0900_P1_CARFREQ, carFreq);

		/*set back the previous tming TH */
		ChipSetOneRegister(hChip, RSTV0900_P1_TMGTHRISE, tmgTHhigh);
		/*set back the previous tming TH */
		ChipSetOneRegister(hChip, RSTV0900_P1_TMGTHFALL, tmgTHLow);
		break;

	case FE_SAT_DEMOD_2:
		carFreq = ChipGetOneRegister(hChip, RSTV0900_P2_CARFREQ);
		tmgTHhigh = ChipGetOneRegister(hChip, RSTV0900_P2_TMGTHRISE);
		tmgTHLow = ChipGetOneRegister(hChip, RSTV0900_P2_TMGTHFALL);

		/*use higher timing lock TH */
		ChipSetOneRegister(hChip, RSTV0900_P2_TMGTHRISE, 0x20);
		/*use higher timing lock TH */
		ChipSetOneRegister(hChip, RSTV0900_P2_TMGTHFALL, 0);

		/* stop the carrier offset search */
		ChipSetFieldImage(hChip, FSTV0900_P2_CFR_AUTOSCAN, 0);
		ChipSetRegisters(hChip, RSTV0900_P2_DMDCFGMD, 1);

		/*set the DVBS1 timing loop beta value to 0 (stop timing loop)*/
		ChipSetOneRegister(hChip, RSTV0900_P2_RTC, 0x80);
		/*set the DVBS2 timing loop beta value to 0 (stop timing loop)*/
		ChipSetOneRegister(hChip, RSTV0900_P2_RTCS2, 0x40);
		/*Stop the carrier loop */
		ChipSetOneRegister(hChip, RSTV0900_P2_CARFREQ, 0x0);

		ChipSetFieldImage(hChip, FSTV0900_P2_CFR_INIT1, 0);
		ChipSetFieldImage(hChip, FSTV0900_P2_CFR_INIT0, 0);
		/*init the demod frequency offset to 0 */
		ChipSetRegisters(hChip, RSTV0900_P2_CFRINIT1, 2);

		ChipSetOneRegister(hChip, RSTV0900_P2_AGC2REF, 0x65);
		/*Trig an acquisition (start the search) only for timing check
		 * no need to white for lock */
		ChipSetOneRegister(hChip, RSTV0900_P2_DMDISTATE, 0x18);
		ChipWaitOrAbort(hChip, 5);
		for (i = 0; i < 10; i++) {
			if (ChipGetField(hChip, FSTV0900_P2_TMGLOCK_QUALITY) >=
					2)
				/*read the timing lock quality */
				timingcpt++;
			ChipWaitOrAbort(hChip, 1);
		}
		if (timingcpt >= 3)
			/*timing locked when timing quality is higer than 2 for
			 * 3 times */
			timingLock = TRUE;

		ChipSetOneRegister(hChip, RSTV0900_P2_AGC2REF, 0x38);
		/*enable the DVBS1 timing loop */
		ChipSetOneRegister(hChip, RSTV0900_P2_RTC, 0x88);
		/*enable the DVBS2 timing loop */
		ChipSetOneRegister(hChip, RSTV0900_P2_RTCS2, 0x68);

		/*enable the carrier loop */
		ChipSetOneRegister(hChip, RSTV0900_P2_CARFREQ, carFreq);

		/*set back the previous tming TH */
		ChipSetOneRegister(hChip, RSTV0900_P2_TMGTHRISE, tmgTHhigh);
		/*set back the previous tming TH */
		ChipSetOneRegister(hChip, RSTV0900_P2_TMGTHFALL, tmgTHLow);

		break;
	}

	return timingLock;
}

BOOL FE_STV0900_GetDemodLock(STCHIP_Handle_t hChip, FE_STV0900_DEMOD_t Demod,
			     S32 TimeOut)
{
	S32 timer = 0, lock = 0, headerField, lockField;

	FE_STV0900_SEARCHSTATE_t demodState;

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
		headerField = FSTV0900_P1_HEADER_MODE;
		/* Demod lock status field */
		lockField = FSTV0900_P1_LOCK_DEFINITIF;
		break;

	case FE_SAT_DEMOD_2:
		/* state machine search status field */
		headerField = FSTV0900_P2_HEADER_MODE;
		/* Demod lock status field */
		lockField = FSTV0900_P2_LOCK_DEFINITIF;
		break;
	}

	while ((timer < TimeOut) && (lock == 0)) {

		demodState = ChipGetField(hChip, headerField);
		switch (demodState) {
		case FE_SAT_SEARCH:
		case FE_SAT_PLH_DETECTED:
		default:
			/* no signal */
			lock = 0;
			break;

		case FE_SAT_DVBS2_FOUND:
		case FE_SAT_DVBS_FOUND:
			/*found a DVBS2 signal */
			lock = ChipGetField(hChip, lockField);
			break;
		}

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

BOOL FE_STV0900_GetFECLock(STCHIP_Handle_t hChip, FE_STV0900_DEMOD_t Demod,
			   S32 TimeOut)
{
	S32 timer = 0, lock = 0, headerField, pktdelinField, lockVitField;

	FE_STV0900_SEARCHSTATE_t demodState;

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
		headerField = FSTV0900_P1_HEADER_MODE;
		/* packet delin (DVBS 2) lock status field */
		pktdelinField = FSTV0900_P1_PKTDELIN_LOCK;
		/* Viterbi (DVBS1/DSS) lock status field */
		lockVitField = FSTV0900_P1_LOCKEDVIT;
		break;

	case FE_SAT_DEMOD_2:
		/* state machine search status field */
		headerField = FSTV0900_P2_HEADER_MODE;
		/* packet delin (DVBS 2) lock status field */
		pktdelinField = FSTV0900_P2_PKTDELIN_LOCK;
		/* Viterbi (DVBS1/DSS) lock status field */
		lockVitField = FSTV0900_P2_LOCKEDVIT;
		break;
	}

	demodState = ChipGetField(hChip, headerField);
	while ((timer < TimeOut) && (lock == 0)) {

		switch (demodState) {
		case FE_SAT_SEARCH:
		case FE_SAT_PLH_DETECTED:
		default:
			/* no signal */
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
BOOL FE_STV0900_WaitForLock(STCHIP_Handle_t hChip, FE_STV0900_DEMOD_t Demod,
			    S32 DemodTimeOut, S32 FecTimeOut)
{

	S32 timer = 0, lock = 0, strMergerRSTField, strMergerLockField;

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
		/* Stream Merger reset field) */
		strMergerRSTField = FSTV0900_P1_RST_HWARE;
		/* Stream Merger lock status field) */
		strMergerLockField = FSTV0900_P1_TSFIFO_LINEOK;
		break;

	case FE_SAT_DEMOD_2:
		/* Stream Merger reset field) */
		strMergerRSTField = FSTV0900_P2_RST_HWARE;
		/* Stream Merger lock status field) */
		strMergerLockField = FSTV0900_P2_TSFIFO_LINEOK;
		break;
	}

	lock = FE_STV0900_GetDemodLock(hChip, Demod, DemodTimeOut);
	if (lock)
		lock = lock && FE_STV0900_GetFECLock(hChip, Demod, FecTimeOut);

	if (lock) {
		lock = 0;

		while ((timer < FecTimeOut) && (lock == 0)) {
			/*Check the stream merger Lock (good packet at the
			 * output) */
			lock = ChipGetField(hChip, strMergerLockField);
			ChipWaitOrAbort(hChip, 1);
			timer++;
		}
	}
#ifdef HOST_PC
#ifndef NO_GUI
	lockTime = Timer() - lockTime;
	Fmt(message, "Total Lock Time = %f ms", lockTime * 1000.0);
	if (lock)
		ReportInsertMessage("LOCK OK");
	else
		ReportInsertMessage("LOCK FAIL");

	ReportInsertMessage(message);

#endif
#endif
	if (lock)
		return TRUE;
	else
		return FALSE;

}

void FE_STV0900_StopALL_S2_Modcod(STCHIP_Handle_t hChip,
				  FE_STV0900_DEMOD_t Demod)
{
	/*This function is to be used for CCM only not ACM or VCM */
	S32 regflist, i;

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		regflist = RSTV0900_P1_MODCODLST0;
		break;

	case FE_SAT_DEMOD_2:
		regflist = RSTV0900_P2_MODCODLST0;
		break;
	}
	for (i = 0; i < 16; i++)
		ChipSetOneRegister(hChip, regflist + i, 0xFF);
}

void FE_STV0900_ActivateS2ModCode(STCHIP_Handle_t hChip,
				  FE_STV0900_DEMOD_t Demod)
{
	U32 matype, modCode, fmod, RegIndex, fieldIndex;

	/*This function is to be used for CCM only not ACM or VCM */
	if (hChip->ChipID <= 0x11) {
		ChipWaitOrAbort(hChip, 5);
		switch (Demod) {
		case FE_SAT_DEMOD_1:
		default:
			modCode =
			    ChipGetOneRegister(hChip, RSTV0900_P1_PLHMODCOD);
			matype = modCode & 0x3;
			modCode = (modCode & 0x7f) >> 2;

			RegIndex = RSTV0900_P1_MODCODLSTF - modCode / 2;
			fieldIndex = modCode % 2;
			break;

		case FE_SAT_DEMOD_2:
			modCode =
			    ChipGetOneRegister(hChip, RSTV0900_P2_PLHMODCOD);
			matype = modCode & 0x3;
			modCode = (modCode & 0x7f) >> 2;

			RegIndex = RSTV0900_P2_MODCODLSTF - modCode / 2;
			fieldIndex = modCode % 2;
			break;
		}

		switch (matype) {
		case 0:
		default:
			fmod = 14;
			break;

		case 1:
			fmod = 13;
			break;

		case 2:
			fmod = 11;
			break;

		case 3:
			fmod = 7;
			break;
		}

		if ((INRANGE(FE_SAT_QPSK_12, modCode, FE_SAT_8PSK_910))
		    && (matype <= 1)) {
			if (fieldIndex == 0)
				ChipSetOneRegister(hChip, RegIndex,
						   0xF0 | fmod);
			else
				ChipSetOneRegister(hChip, RegIndex,
						   (fmod << 4) | 0xF);
		}
	} else if (hChip->ChipID >= 0x12) {
		switch (Demod) {
		case FE_SAT_DEMOD_1:
		default:
			for (RegIndex = 0; RegIndex < 8; RegIndex++)
				ChipSetOneRegister(hChip,
						   RSTV0900_P1_MODCODLST7 +
						   RegIndex, 0xcc);
			break;

		case FE_SAT_DEMOD_2:
			for (RegIndex = 0; RegIndex < 8; RegIndex++)
				ChipSetOneRegister(hChip,
						   RSTV0900_P2_MODCODLST7 +
						   RegIndex, 0xcc);
			break;
		}

	}
}

void FE_STV0900_ActivateS2ModCodeSingle(STCHIP_Handle_t hChip,
					FE_STV0900_DEMOD_t Demod)
{
	U32 RegIndex;
	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		ChipSetOneRegister(hChip, RSTV0900_P1_MODCODLST0, 0xFF);
		ChipSetOneRegister(hChip, RSTV0900_P1_MODCODLST1, 0xF0);
		ChipSetOneRegister(hChip, RSTV0900_P1_MODCODLSTF, 0x0F);
		for (RegIndex = 0; RegIndex < 13; RegIndex++)
			ChipSetOneRegister(hChip,
					   RSTV0900_P1_MODCODLST2 + RegIndex,
					   0x00);
		break;

	case FE_SAT_DEMOD_2:
		ChipSetOneRegister(hChip, RSTV0900_P2_MODCODLST0, 0xFF);
		ChipSetOneRegister(hChip, RSTV0900_P2_MODCODLST1, 0xF0);
		ChipSetOneRegister(hChip, RSTV0900_P2_MODCODLSTF, 0x0F);
		for (RegIndex = 0; RegIndex < 13; RegIndex++)
			ChipSetOneRegister(hChip,
					   RSTV0900_P2_MODCODLST2 + RegIndex,
					   0x00);
		break;
	}
}

void FE_STV0900_SetViterbiThAcq(STCHIP_Handle_t hChip, FE_STV0900_DEMOD_t Demod)
{
	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		/*VTH 1/2 */
		ChipSetFieldImage(hChip, FSTV0900_P1_VTH12, 0x96);
		/*VTH 2/3 */
		ChipSetFieldImage(hChip, FSTV0900_P1_VTH23, 0x64);
		/*VTH 3/4 */
		ChipSetFieldImage(hChip, FSTV0900_P1_VTH34, 0x36);
		/*VTH 5/6 */
		ChipSetFieldImage(hChip, FSTV0900_P1_VTH56, 0x23);
		/*VTH 6/7 */
		ChipSetFieldImage(hChip, FSTV0900_P1_VTH67, 0x1E);
		/*VTH 7/8 */
		ChipSetFieldImage(hChip, FSTV0900_P1_VTH78, 0x19);

		ChipSetRegisters(hChip, RSTV0900_P1_VTH12, 6);
		break;

	case FE_SAT_DEMOD_2:
		/*VTH 1/2 */
		ChipSetFieldImage(hChip, FSTV0900_P2_VTH12, 0x96);
		/*VTH 2/3 */
		ChipSetFieldImage(hChip, FSTV0900_P2_VTH23, 0x64);
		/*VTH 3/4 */
		ChipSetFieldImage(hChip, FSTV0900_P2_VTH34, 0x36);
		/*VTH 5/6 */
		ChipSetFieldImage(hChip, FSTV0900_P2_VTH56, 0x23);
		/*VTH 6/7 */
		ChipSetFieldImage(hChip, FSTV0900_P2_VTH67, 0x1E);
		/*VTH 7/8 */
		ChipSetFieldImage(hChip, FSTV0900_P2_VTH78, 0x19);

		ChipSetRegisters(hChip, RSTV0900_P2_VTH12, 6);
		break;
	}
}

void FE_STV0900_SetViterbiThTracq(STCHIP_Handle_t hChip,
				  FE_STV0900_DEMOD_t Demod)
{
	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		/*VTH 1/2 */
		ChipSetFieldImage(hChip, FSTV0900_P1_VTH12, 0xd0);
		/*VTH 2/3 */
		ChipSetFieldImage(hChip, FSTV0900_P1_VTH23, 0x7d);
		/*VTH 3/4 */
		ChipSetFieldImage(hChip, FSTV0900_P1_VTH34, 0x53);
		/*VTH 5/6 */
		ChipSetFieldImage(hChip, FSTV0900_P1_VTH56, 0x2f);
		/*VTH 6/7 */
		ChipSetFieldImage(hChip, FSTV0900_P1_VTH67, 0x24);
		/*VTH 7/8 */
		ChipSetFieldImage(hChip, FSTV0900_P1_VTH78, 0x1f);

		ChipSetRegisters(hChip, RSTV0900_P1_VTH12, 6);
		break;

	case FE_SAT_DEMOD_2:
		/*VTH 1/2 */
		ChipSetFieldImage(hChip, FSTV0900_P2_VTH12, 0xd0);
		/*VTH 2/3 */
		ChipSetFieldImage(hChip, FSTV0900_P2_VTH23, 0x7d);
		/*VTH 3/4 */
		ChipSetFieldImage(hChip, FSTV0900_P2_VTH34, 0x53);
		/*VTH 5/6 */
		ChipSetFieldImage(hChip, FSTV0900_P2_VTH56, 0x2f);
		/*VTH 6/7 */
		ChipSetFieldImage(hChip, FSTV0900_P2_VTH67, 0x24);
		/*VTH 7/8 */
		ChipSetFieldImage(hChip, FSTV0900_P2_VTH78, 0x1f);

		ChipSetRegisters(hChip, RSTV0900_P2_VTH12, 6);
		break;
	}
}

void FE_STV0900_SetViterbiStandard(STCHIP_Handle_t hChip,
				   FE_STV0900_SearchStandard_t Standard,
				   FE_STV0900_Rate_t PunctureRate,
				   FE_STV0900_DEMOD_t Demod)
{

	S32 fecmReg, prvitReg;
	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		fecmReg = RSTV0900_P1_FECM;
		prvitReg = RSTV0900_P1_PRVIT;
		break;

	case FE_SAT_DEMOD_2:
		fecmReg = RSTV0900_P2_FECM;
		prvitReg = RSTV0900_P2_PRVIT;
		break;
	}
	switch (Standard) {
	case FE_SAT_AUTO_SEARCH:
		/* Disable DSS  in auto mode search for DVBS1 and DVBS2 only ,
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

void FE_STV0900_SetDVBS1_TrackCarLoop(STCHIP_Handle_t hChip,
				      FE_STV0900_DEMOD_t Demod, U32 SymbolRate)
{

	U16 aclcReg, bclcReg;

	switch (Demod) {
	case FE_SAT_DEMOD_2:
		aclcReg = RSTV0900_P2_ACLC;
		bclcReg = RSTV0900_P2_BCLC;
		break;

	case FE_SAT_DEMOD_1:
	default:
		aclcReg = RSTV0900_P1_ACLC;
		bclcReg = RSTV0900_P1_BCLC;
		break;

	}

	if (hChip->ChipID >= 0x30) {
		/* Set ACLC BCLC optimised value vs SR */
		if (SymbolRate >= 15000000) {
			ChipSetOneRegister(hChip, aclcReg, 0x2b);
			ChipSetOneRegister(hChip, bclcReg, 0x1a);
		} else if ((SymbolRate >= 7000000) && (15000000 > SymbolRate)) {
			ChipSetOneRegister(hChip, aclcReg, 0x0c);
			ChipSetOneRegister(hChip, bclcReg, 0x1b);
		} else if (SymbolRate < 7000000) {
			ChipSetOneRegister(hChip, aclcReg, 0x2c);
			ChipSetOneRegister(hChip, bclcReg, 0x1c);
		}

	} else {
		/*cut 2.0 and 1.x */

		ChipSetOneRegister(hChip, aclcReg, 0x1a);
		ChipSetOneRegister(hChip, bclcReg, 0x09);
	}

}

S32 FE_STV0900_CheckVGLNAPresent(STCHIP_Handle_t hChipVGLNA)
{
	S32 isVGLNA = 0;

#if !defined(HOST_PC) || defined(NO_GUI)
	if (hChipVGLNA == NULL)
		isVGLNA = 0;
	else
		isVGLNA = 1;
#else
	if (ChipCheckAck(hChipVGLNA) == CHIPERR_I2C_NO_ACK)
		isVGLNA = 0;
	else
		isVGLNA = 1;
#endif

	return isVGLNA;
}

S32 FE_STV0900_GetRFLevelWithVGLNA(FE_STV0900_InternalParams_t *pParams,
				   FE_STV0900_DEMOD_t Demod)
{
	S32 Power = 0, vglnagain = 0, BBgain = 0;

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		if (FE_STV0900_CheckVGLNAPresent(pParams->hVglna1) == 0) {
			Power =
			    FE_STV0900_GetRFLevel(pParams->hDemod,
						  &FE_STV0900_RF_LookUp, Demod);
		} else {
			Power =
			    FE_STV0900_GetRFLevel(pParams->hDemod,
						  &FE_STV0900_RF_LookUpVGLNA,
						  Demod);
			if (STVVGLNA_GetGain(pParams->hVglna1, &vglnagain) ==
			    CHIPERR_NO_ERROR) {
				BBgain = FE_Sat_TunerGetGain(pParams->hTuner1);
				Power += vglnagain;
				Power += ((12 - BBgain) + 2);
			}

		}
		break;

	case FE_SAT_DEMOD_2:
		if (FE_STV0900_CheckVGLNAPresent(pParams->hVglna2) == 0) {
			Power =
			    FE_STV0900_GetRFLevel(pParams->hDemod,
						  &FE_STV0900_RF_LookUp, Demod);
		} else {
			Power =
			    FE_STV0900_GetRFLevel(pParams->hDemod,
						  &FE_STV0900_RF_LookUpVGLNA,
						  Demod);
			if (STVVGLNA_GetGain(pParams->hVglna2, &vglnagain) ==
			    CHIPERR_NO_ERROR) {
				BBgain = FE_Sat_TunerGetGain(pParams->hTuner2);
				Power += vglnagain;
				Power += ((12 - BBgain) + 2);
			}
		}

		break;
	}

	return Power;
}

/*****************************************************
--FUNCTION	::	SetSearchStandard
--ACTION	::	Set the Search standard (Auto, DVBS2 only or DVBS1/DSS
			only)
--PARAMS IN	::	hChip	->	handle to the chip
			::	Standard	->	Search standard
			::	Demod	->	current demod 1 or 2
--PARAMS OUT::	NONE
--RETURN	::	none
--***************************************************/
void FE_STV0900_SetSearchStandard(FE_STV0900_InternalParams_t *pParams,
				  FE_STV0900_DEMOD_t Demod)
{
	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		/*In case of Path 1 */
		switch (pParams->Demod1SearchStandard) {
		case FE_SAT_SEARCH_DVBS1:
		case FE_SAT_SEARCH_DSS:
			/*If DVBS1/DSS only disable DVBS2 search */
			ChipSetFieldImage(pParams->hDemod,
					  FSTV0900_P1_DVBS1_ENABLE, 1);
			ChipSetFieldImage(pParams->hDemod,
					  FSTV0900_P1_DVBS2_ENABLE, 0);
			ChipSetRegisters(pParams->hDemod, RSTV0900_P1_DMDCFGMD,
					 1);

			/*Activate Viterbi decoder if DVBS1 or DSS search, do
			 * not use FRESVIT1 field it may have an impacte on the
			 * VITERBI2 */
			ChipSetFieldImage(pParams->hDemod,
					  FSTV0900_STOP_CLKVIT1, 0);
			ChipSetRegisters(pParams->hDemod, RSTV0900_STOPCLK2, 1);

			FE_STV0900_SetDVBS1_TrackCarLoop(pParams->hDemod, Demod,
							 pParams->
							 Demod1SymbolRate);

			/* disable the DVBS2 carrier loop */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P1_CAR2CFG, 0x22);

			FE_STV0900_SetViterbiThAcq(pParams->hDemod, Demod);
			FE_STV0900_SetViterbiStandard(pParams->hDemod,
						      pParams->
						      Demod1SearchStandard,
						      pParams->
						      Demod1PunctureRate,
						      Demod);

			break;

		case FE_SAT_SEARCH_DVBS2:
			/*If DVBS2 only activate the DVBS2 search and stop the
			 * VITERBI */
			ChipSetFieldImage(pParams->hDemod,
					  FSTV0900_P1_DVBS1_ENABLE, 0);
			ChipSetFieldImage(pParams->hDemod,
					  FSTV0900_P1_DVBS2_ENABLE, 1);
			ChipSetRegisters(pParams->hDemod, RSTV0900_P1_DMDCFGMD,
					 1);

			/*Stop the Viterbi decoder if DVBS2 search, do not use
			 * FRESVIT1 field it may have an impacte on the
			 * VITERBI2 */
			/*Stop Viterbi decoder if DVBS2 search */
			ChipSetFieldImage(pParams->hDemod,
					FSTV0900_STOP_CLKVIT1, 1);
			ChipSetRegisters(pParams->hDemod, RSTV0900_STOPCLK2, 1);

			/*stop DVBS1/DSS carrier loop  */
			ChipSetOneRegister(pParams->hDemod, RSTV0900_P1_ACLC,
					0x1a);
			/*stop DVBS1/DSS carrier loop  */
			ChipSetOneRegister(pParams->hDemod, RSTV0900_P1_BCLC,
					0x09);

			if (pParams->hDemod->ChipID <= 0x20)
				/*cut 1.x and 2.0 */
				/* enable the DVBS2 carrier loop */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P1_CAR2CFG, 0x26);
			else
				/* cut 3.0 above (stop carrier 3) to be
				 * reviewed */
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0900_P1_CAR2CFG, 0x66);

			if (pParams->DemodType != FE_SAT_SINGLE) {
				/*if the 900 is used in dual demod */
				if (pParams->hDemod->ChipID <= 0x11)
					/* if cut 1.1 stop the link betwen the
					 * demod and the FEC during demod
					 * search */
					/*Stop all DVBS2 modcod for PATH1 cut
					 * 1.1 and 1.0 only */
					FE_STV0900_StopALL_S2_Modcod(
						pParams->hDemod, Demod);

				else
					/*cut 2.0 enable the link even during
					 * the search */
					FE_STV0900_ActivateS2ModCode(
						pParams->hDemod, Demod);
			} else {
				/*STV0900 used as single demod , Authorize
				 * short and long frames, QP, 8P 16AP and
				 * 32APSK */
				FE_STV0900_ActivateS2ModCodeSingle(
						pParams->hDemod, Demod);
			}
			/*if DVBS2 change reset VIREBI THresholds to tracking
			 * values */
			FE_STV0900_SetViterbiThTracq(pParams->hDemod, Demod);

			break;

		case FE_SAT_AUTO_SEARCH:
		default:
			/*If automatic enable both DVBS1/DSS and DVBS2 search */
			if (pParams->hDemod->ChipID <= 0x20) {
				ChipSetFieldImage(pParams->hDemod,
						  FSTV0900_P1_DVBS1_ENABLE, 0);
				ChipSetFieldImage(pParams->hDemod,
						  FSTV0900_P1_DVBS2_ENABLE, 1);
				ChipSetRegisters(pParams->hDemod,
						 RSTV0900_P1_DMDCFGMD, 1);

				ChipSetFieldImage(pParams->hDemod,
						  FSTV0900_P1_DVBS1_ENABLE, 1);
				ChipSetFieldImage(pParams->hDemod,
						  FSTV0900_P1_DVBS2_ENABLE, 0);
				ChipSetRegisters(pParams->hDemod,
						 RSTV0900_P1_DMDCFGMD, 1);
			}
			ChipSetFieldImage(pParams->hDemod,
					  FSTV0900_P1_DVBS1_ENABLE, 1);
			ChipSetFieldImage(pParams->hDemod,
					  FSTV0900_P1_DVBS2_ENABLE, 1);
			ChipSetRegisters(pParams->hDemod, RSTV0900_P1_DMDCFGMD,
					 1);

			/*Activate Viterbi decoder if DVBS1 or DSS search, do
			 * not use FRESVIT1 field it may have an impacte on the
			 * VITERBI2 */
			/*Activate Viterbi decoder if auto STD search */
			ChipSetFieldImage(pParams->hDemod,
					FSTV0900_STOP_CLKVIT1, 0);
			ChipSetRegisters(pParams->hDemod, RSTV0900_STOPCLK2, 1);

			FE_STV0900_SetDVBS1_TrackCarLoop(pParams->hDemod, Demod,
						 pParams->Demod1SymbolRate);

			if (pParams->hDemod->ChipID <= 0x20)
				/*cut 1.x and 2.0 */
				/* enable the DVBS2 carrier loop */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P1_CAR2CFG, 0x26);
			else
				/* cut 3.0 above (stop carrier 3) to be
				 * reviewed */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P1_CAR2CFG, 0x66);

			if (pParams->DemodType != FE_SAT_SINGLE) {
				if (pParams->hDemod->ChipID <= 0x11)
					/* if cut 1.1 stop the link betwen the
					 * demod and the FEC during demod
					 * search */
					/*Stop all DVBS2 modcod for PATH1 cut
					 * 1.1 and 1.0 only */
					FE_STV0900_StopALL_S2_Modcod(
						pParams->hDemod, Demod);

				else
					/*cut 2.0 enable the link even during
					 * the search */
					FE_STV0900_ActivateS2ModCode(
						pParams->hDemod, Demod);
			} else {
				/*STV0900 used as single demod , Authorize
				 * short and long frames, QP, 8P 16AP and
				 * 32APSK */
				FE_STV0900_ActivateS2ModCodeSingle(
						pParams->hDemod, Demod);
			}

			FE_STV0900_SetViterbiThAcq(pParams->hDemod, Demod);

			FE_STV0900_SetViterbiStandard(pParams->hDemod,
					pParams->Demod1SearchStandard,
					pParams->Demod1PunctureRate, Demod);

			break;
		}
		break;

	case FE_SAT_DEMOD_2:
		/*In case of Path 2 */
		switch (pParams->Demod2SearchStandard) {
		case FE_SAT_SEARCH_DVBS1:
		case FE_SAT_SEARCH_DSS:
			/*If DVBS1/DSS only disable DVBS2 search */
			ChipSetFieldImage(pParams->hDemod,
					  FSTV0900_P2_DVBS1_ENABLE, 1);
			ChipSetFieldImage(pParams->hDemod,
					  FSTV0900_P2_DVBS2_ENABLE, 0);
			ChipSetRegisters(pParams->hDemod, RSTV0900_P2_DMDCFGMD,
					 1);

			/*Activate Viterbi decoder if DVBS1 or DSS search, do
			 * not use FRESVIT1 field it may have an impacte on the
			 * VITERBI2 */
			ChipSetFieldImage(pParams->hDemod,
					  FSTV0900_STOP_CLKVIT2, 0);
			ChipSetRegisters(pParams->hDemod, RSTV0900_STOPCLK2, 1);

			FE_STV0900_SetDVBS1_TrackCarLoop(pParams->hDemod, Demod,
						 pParams->Demod2SymbolRate);

			/* disable the DVBS2 carrier loop */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P2_CAR2CFG, 0x22);

			FE_STV0900_SetViterbiThAcq(pParams->hDemod, Demod);
			FE_STV0900_SetViterbiStandard(pParams->hDemod,
					pParams->Demod2SearchStandard,
					pParams->Demod2PunctureRate, Demod);
			break;

		case FE_SAT_SEARCH_DVBS2:
			/*If DVBS2 only activate the DVBS2 & stop the VITERBI*/
			ChipSetFieldImage(pParams->hDemod,
					  FSTV0900_P2_DVBS1_ENABLE, 0);
			ChipSetFieldImage(pParams->hDemod,
					  FSTV0900_P2_DVBS2_ENABLE, 1);
			ChipSetRegisters(pParams->hDemod, RSTV0900_P2_DMDCFGMD,
					 1);

			/*Stop the Viterbi decoder if DVBS2 search, do not use
			 * FRESVIT1 field it may have an impacte on the
			 * VITERBI2 */
			/*Stop Viterbi decoder if DVBS2 search */
			ChipSetFieldImage(pParams->hDemod,
					FSTV0900_STOP_CLKVIT2, 1);
			ChipSetRegisters(pParams->hDemod, RSTV0900_STOPCLK2, 1);

			/*stop DVBS1/DSS carrier loop  */
			ChipSetOneRegister(pParams->hDemod, RSTV0900_P2_ACLC,
					0x1a);
			/*stop DVBS1/DSS carrier loop  */
			ChipSetOneRegister(pParams->hDemod, RSTV0900_P2_BCLC,
					0x09);

			if (pParams->hDemod->ChipID <= 0x20)
				/*cut 1.x and 2.0 */
				/* enable the DVBS2 carrier loop */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P2_CAR2CFG, 0x26);
			else
				/* cut 3.0 above (stop carrier 3) to be
				 * reviewed */
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0900_P2_CAR2CFG, 0x66);

			if (pParams->DemodType != FE_SAT_SINGLE)
				FE_STV0900_ActivateS2ModCode(pParams->hDemod,
							     Demod);
			else {
				/*STV0900 used as single demod , Authorize
				 * short and long frames, QP, 8P 16AP and
				 * 32APSK */
				FE_STV0900_ActivateS2ModCodeSingle(
						pParams->hDemod, Demod);
			}

			/*if DVBS2 change reset VIREBI THresholds to tracking
			 * values */
			FE_STV0900_SetViterbiThTracq(pParams->hDemod, Demod);
			break;

		case FE_SAT_AUTO_SEARCH:
		default:
			/*If automatic enable both DVBS1/DSS and DVBS2 search */
			ChipSetFieldImage(pParams->hDemod,
					  FSTV0900_P2_DVBS1_ENABLE, 1);
			ChipSetFieldImage(pParams->hDemod,
					  FSTV0900_P2_DVBS2_ENABLE, 1);
			ChipSetRegisters(pParams->hDemod, RSTV0900_P2_DMDCFGMD,
					 1);

			/*Activate Viterbi decoder if DVBS1 or DSS search, do
			 * not use FRESVIT1 field it may have an impacte on the
			 * VITERBI2 */
			/*Activate Viterbi decoder if auto STD search */
			ChipSetFieldImage(pParams->hDemod,
					FSTV0900_STOP_CLKVIT2, 0);
			ChipSetRegisters(pParams->hDemod, RSTV0900_STOPCLK2, 1);

			FE_STV0900_SetDVBS1_TrackCarLoop(pParams->hDemod, Demod,
						 pParams->Demod2SymbolRate);

			if (pParams->hDemod->ChipID <= 0x20)
				/*cut 1.x and 2.0 */
				/* enable the DVBS2 carrier loop */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P2_CAR2CFG, 0x26);
			else
				/* cut 3.0 above (stop carrier 3) to be
				 * reviewed */
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0900_P2_CAR2CFG, 0x66);

			if (pParams->DemodType != FE_SAT_SINGLE)
				FE_STV0900_ActivateS2ModCode(pParams->hDemod,
							     Demod);
			else {
				/*STV0900 used as single demod , Authorize
				 * short and long frames, QP, 8P 16AP and
				 * 32APSK */
				FE_STV0900_ActivateS2ModCodeSingle(pParams->
							   hDemod, Demod);
			}

			FE_STV0900_SetViterbiThAcq(pParams->hDemod, Demod);

			FE_STV0900_SetViterbiStandard(pParams->hDemod,
				      pParams->Demod2SearchStandard,
				      pParams->Demod2PunctureRate, Demod);
			break;
		}
		break;
	}

}

/*****************************************************
--FUNCTION	::	StartSearch
--ACTION	::	Trig the Demod to start a new search
--PARAMS IN	::	pParams	->Pointer FE_STV0900_InternalParams_t structer
			::	Demod	->	current demod 1 or 2
--PARAMS OUT::	NONE
--RETURN	::	none
--***************************************************/
void FE_STV0900_StartSearch(FE_STV0900_InternalParams_t *pParams,
			    FE_STV0900_DEMOD_t Demod)
{

	U32 freq;
	S16 freq_s16;

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		/* In case of Path 1 */
		/*Reset the Demod */
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P1_DMDISTATE,
				0x1F);

		if (pParams->hDemod->ChipID == 0x10)
			/*Set Correlation frames reg for acquisition for cut
			 * 1.0 only */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P1_CORRELEXP, 0xAA);

		if (pParams->hDemod->ChipID < 0x20)
			/*Not applicable for cut 2.0 */
			ChipSetOneRegister(pParams->hDemod, RSTV0900_P1_CARHDR,
					0x55);

		if (pParams->hDemod->ChipID <= 0x20) {
			if (pParams->Demod1SymbolRate <= 5000000) {
				/* If the symbol rate is <= 5Msps
				   Set The carrier search up and low to manual
				   mode[-8Mhz,+8Mhz] */
				/*applicable for cut 1.x and 2.0 */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P1_CARCFG, 0x44);
				/*CFR UP Setting */
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0900_P1_CFRUP1, 0x0F);
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0900_P1_CFRUP0, 0xFF);

				/*CFR Low Setting */
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0900_P1_CFRLOW1, 0xF0);
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0900_P1_CFRLOW0, 0x00);

				/*enlarge the timing bandwith for Low SR */
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0900_P1_RTCS2, 0x68);

			} else {
				/* If the symbol rate is >5 Msps
				   Set The carrier search up and low to auto
				   mode */
				/*applicable for cut 1.x and 2.0 */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P1_CARCFG, 0xC4);
				/*reduce the timing bandwith for high SR */
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0900_P1_RTCS2, 0x44);
			}
		} else {
			/*cut 3.0 above */

			if (pParams->Demod1SymbolRate <= 5000000) {
				/*enlarge the timing bandwith for Low SR */
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0900_P1_RTCS2, 0x68);
			} else {
				/*reduce the timing bandwith for high SR */
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0900_P1_RTCS2, 0x44);
			}

			/*Set CFR min and max to manual mode */
			ChipSetOneRegister(pParams->hDemod, RSTV0900_P1_CARCFG,
					   0x46);

			if (pParams->Demod1SearchAlgo == FE_SAT_WARM_START) {
				/* if warm start CFR min =-1MHz , CFR max =
				 * 1MHz */
				freq = 1000 << 16;
				freq /= (pParams->MasterClock / 1000);
				freq_s16 = (S16) freq;
			} else {

				/* CFR min =- (SearchRange/2 + margin)
				   CFR max = +(SearchRange/2 + margin)
				   (80KHz margin if SR <=5Msps else margin
				   =600KHz) */

				if (pParams->Demod1SymbolRate <= 5000000)
					freq =
					    (pParams->Demod1SearchRange /
					     2000) + 80;
				else
					freq =
					    (pParams->Demod1SearchRange /
					     2000) + 600;
				freq = freq << 16;
				freq /= (pParams->MasterClock / 1000);
				freq_s16 = (S16) freq;
			}

			ChipSetFieldImage(pParams->hDemod, FSTV0900_P1_CFR_UP1,
					  MSB(freq_s16));
			ChipSetFieldImage(pParams->hDemod, FSTV0900_P1_CFR_UP0,
					  LSB(freq_s16));
			ChipSetRegisters(pParams->hDemod, RSTV0900_P1_CFRUP1,
					 2);
			freq_s16 *= (-1);
			ChipSetFieldImage(pParams->hDemod, FSTV0900_P1_CFR_LOW1,
					  MSB(freq_s16));
			ChipSetFieldImage(pParams->hDemod, FSTV0900_P1_CFR_LOW0,
					  LSB(freq_s16));
			ChipSetRegisters(pParams->hDemod, RSTV0900_P1_CFRLOW1,
					 2);

		}
		ChipSetFieldImage(pParams->hDemod, FSTV0900_P1_CFR_INIT1, 0);
		ChipSetFieldImage(pParams->hDemod, FSTV0900_P1_CFR_INIT0, 0);
		/*init the demod frequency offset to 0 */
		ChipSetRegisters(pParams->hDemod, RSTV0900_P1_CFRINIT1, 2);

		if (pParams->hDemod->ChipID >= 0x20) {
			ChipSetOneRegister(pParams->hDemod,
					   RSTV0900_P1_EQUALCFG, 0x41);
			ChipSetOneRegister(pParams->hDemod, RSTV0900_P1_FFECFG,
					   0x41);

			if ((pParams->Demod1SearchStandard ==
			     FE_SAT_SEARCH_DVBS1)
			    || (pParams->Demod1SearchStandard ==
				FE_SAT_SEARCH_DSS)
			    || (pParams->Demod1SearchStandard ==
				FE_SAT_AUTO_SEARCH)) {
				/* open the ReedSolomon to viterbi feed back
				 * until demod lock */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P1_VITSCALE, 0x82);
				/*set Viterbi hysteresis for search */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P1_VAVSRVIT, 0x0);
			}
		}

		/*Slow down the timing loop */
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P1_SFRSTEP, 0x00);
		/*timing stting for warm/cold start */
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P1_TMGTHRISE,
				0xE0);
		/*timing stting for warm/cold start */
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P1_TMGTHFALL,
				0xC0);

		/* stop the carrier frequency search loop */
		ChipSetFieldImage(pParams->hDemod, FSTV0900_P1_CFR_AUTOSCAN, 0);
		/* stop the carrier frequency search loop */
		ChipSetFieldImage(pParams->hDemod, FSTV0900_P1_SCAN_ENABLE, 0);
		ChipSetRegisters(pParams->hDemod, RSTV0900_P1_DMDCFGMD, 1);

		/* S1 and  S2 search in */	/* if both enabled */
		ChipSetFieldImage(pParams->hDemod, FSTV0900_P1_S1S2_SEQUENTIAL,
				0);
		ChipSetRegisters(pParams->hDemod, RSTV0900_P1_DMDCFG2, 1);

		/*enable the DVBS1 timing loop */
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P1_RTC, 0x88);

		if (pParams->hDemod->ChipID >= 0x20) {
			/*Frequency offset detector setting */
			if (pParams->Demod1SymbolRate < 2000000) {
				/*SR <2Msps */
				if (pParams->hDemod->ChipID <= 0x20)
					/* cut 2.0 */
					ChipSetOneRegister(pParams->hDemod,
							   RSTV0900_P1_CARFREQ,
							   0x39);
				else
					/*cut 3.0 */

					ChipSetOneRegister(pParams->hDemod,
							   RSTV0900_P1_CARFREQ,
							   0x89);
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0900_P1_CARHDR, 0x40);

			} else if (pParams->Demod1SymbolRate < 10000000) {
				/*2Msps<=SR <10Msps */
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0900_P1_CARFREQ, 0x4C);
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0900_P1_CARHDR, 0x20);

			} else {
				/*SR >=10Msps */

				ChipSetOneRegister(pParams->hDemod,
						   RSTV0900_P1_CARFREQ, 0x4B);
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0900_P1_CARHDR, 0x20);
			}
		} else {
			if (pParams->Demod1SymbolRate < 10000000)
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0900_P1_CARFREQ, 0xef);
			else
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0900_P1_CARFREQ, 0xed);
		}

		switch (pParams->Demod1SearchAlgo) {
		case FE_SAT_WARM_START:
			/*The symbol rate and the exact carrier Frequency are
			 * known */
			/*Reset the Demod */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P1_DMDISTATE, 0x1F);
			/*Trig an acquisition (start the search) */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P1_DMDISTATE, 0x18);
			break;

		case FE_SAT_COLD_START:
			/*The symbol rate is known */
			/*Reset the Demod */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P1_DMDISTATE, 0x1F);
			/*Trig an acquisition (start the search) */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P1_DMDISTATE, 0x15);
			break;

		default:
			/*Nothing to do in case of blind search, blind  is
			 * handled by "FE_STV0900_BlindSearchAlgo" function */
			break;
		}
		break;

	case FE_SAT_DEMOD_2:
		/* In case of Path 2 */
		/*Reset the Demod */
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P2_DMDISTATE,
				0x1F);

		if (pParams->hDemod->ChipID == 0x10)
			/*Set Correlation frames reg for acquisition for cut
			 * 1.0 only */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P2_CORRELEXP, 0xAA);

		if (pParams->hDemod->ChipID < 0x20)
			/*Not applicable for cut 2.0 */
			ChipSetOneRegister(pParams->hDemod, RSTV0900_P2_CARHDR,
					0x55);

		if (pParams->hDemod->ChipID <= 0x20) {
			if (pParams->Demod2SymbolRate <= 5000000) {
				/* If the symbol rate is <= 5Msps
				   Set The carrier search up and low to manual
				   mode[-8Mhz,+8Mhz] */
				/*applicable for cut 1.x and 2.0 */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P2_CARCFG, 0x44);
				/*CFR UP Setting */
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0900_P2_CFRUP1, 0x0F);
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0900_P2_CFRUP0, 0xFF);

				/*CFR Low Setting */
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0900_P2_CFRLOW1, 0xF0);
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0900_P2_CFRLOW0, 0x00);

				/*enlarge the timing bandwith for Low SR */
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0900_P2_RTCS2, 0x68);

			} else {
				/* If the symbol rate is >5 Msps
				   Set The carrier search up and low to auto
				   mode */
				/*applicable for cut 1.x and 2.0 */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P2_CARCFG, 0xC4);
				/*reduce the timing bandwith for high SR */
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0900_P2_RTCS2, 0x44);

			}
		} else {
			/*cut 3.0 above */

			if (pParams->Demod2SymbolRate <= 5000000) {
				/*enlarge the timing bandwith for Low SR */
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0900_P2_RTCS2, 0x68);
			} else {
				/*reduce the timing bandwith for high SR */
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0900_P2_RTCS2, 0x44);
			}

			/*Set CFR min and max to manual mode */
			ChipSetOneRegister(pParams->hDemod, RSTV0900_P2_CARCFG,
					   0x46);

			if (pParams->Demod2SearchAlgo == FE_SAT_WARM_START) {
				/* if warm start CFR min =-1MHz , CFR max =
				 * 1MHz */
				freq = 1000 << 16;
				freq /= (pParams->MasterClock / 1000);
				freq_s16 = (S16) freq;
			} else {

				/* CFR min =- (SearchRange/2 + margin)
				   CFR max = +(SearchRange/2 + margin)
				   (80KHz margin if SR <=5Msps else margin
				   =600KHz) */

				if (pParams->Demod2SymbolRate <= 5000000)
					freq =
					    (pParams->Demod2SearchRange /
					     2000) + 80;
				else
					freq =
					    (pParams->Demod2SearchRange /
					     2000) + 600;
				freq = freq << 16;
				freq /= (pParams->MasterClock / 1000);
				freq_s16 = (S16) freq;
			}

			ChipSetFieldImage(pParams->hDemod, FSTV0900_P2_CFR_UP1,
					  MSB(freq_s16));
			ChipSetFieldImage(pParams->hDemod, FSTV0900_P2_CFR_UP0,
					  LSB(freq_s16));
			ChipSetRegisters(pParams->hDemod, RSTV0900_P2_CFRUP1,
					 2);
			freq_s16 *= (-1);
			ChipSetFieldImage(pParams->hDemod, FSTV0900_P2_CFR_LOW1,
					  MSB(freq_s16));
			ChipSetFieldImage(pParams->hDemod, FSTV0900_P2_CFR_LOW0,
					  LSB(freq_s16));
			ChipSetRegisters(pParams->hDemod, RSTV0900_P2_CFRLOW1,
					 2);

		}

		ChipSetFieldImage(pParams->hDemod, FSTV0900_P2_CFR_INIT1, 0);
		ChipSetFieldImage(pParams->hDemod, FSTV0900_P2_CFR_INIT0, 0);
		/*init the demod frequency offset to 0 */
		ChipSetRegisters(pParams->hDemod, RSTV0900_P2_CFRINIT1, 2);

		if (pParams->hDemod->ChipID >= 0x20) {
			ChipSetOneRegister(pParams->hDemod,
					   RSTV0900_P2_EQUALCFG, 0x41);
			ChipSetOneRegister(pParams->hDemod, RSTV0900_P2_FFECFG,
					   0x41);

			if ((pParams->Demod2SearchStandard ==
			     FE_SAT_SEARCH_DVBS1)
			    || (pParams->Demod2SearchStandard ==
				FE_SAT_SEARCH_DSS)
			    || (pParams->Demod2SearchStandard ==
				FE_SAT_AUTO_SEARCH)) {
				/* open the ReedSolomon to viterbi feed back
				 * until demod lock */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P2_VITSCALE, 0x82);
				/*set Viterbi hysteresis for search */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P2_VAVSRVIT, 0x0);
			}
		}

		/*Slow down the timing loop */
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P2_SFRSTEP, 0x00);
		/*timing stting for warm/cold start */
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P2_TMGTHRISE,
				0xE0);
		/*timing stting for warm/cold start */
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P2_TMGTHFALL,
				0xC0);

		/* stop the carrier frequency search loop */
		ChipSetFieldImage(pParams->hDemod, FSTV0900_P2_CFR_AUTOSCAN, 0);
		/* stop the carrier frequency search loop */
		ChipSetFieldImage(pParams->hDemod, FSTV0900_P2_SCAN_ENABLE, 0);
		ChipSetRegisters(pParams->hDemod, RSTV0900_P2_DMDCFGMD, 1);

		/* S1 and  S2 search in*//* if both enabled */
		ChipSetFieldImage(pParams->hDemod, FSTV0900_P2_S1S2_SEQUENTIAL,
				0);
		ChipSetRegisters(pParams->hDemod, RSTV0900_P2_DMDCFG2, 1);

		/*enable the DVBS1 timing loop */
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P2_RTC, 0x88);

		if (pParams->hDemod->ChipID >= 0x20) {
			/*Frequency offset detector setting */
			if (pParams->Demod2SymbolRate < 2000000) {
				/*SR <2Msps */
				if (pParams->hDemod->ChipID <= 0x20)
					/* cut 2.0 */
					ChipSetOneRegister(pParams->hDemod,
							   RSTV0900_P2_CARFREQ,
							   0x39);
				else /*cut 3.0 */
					ChipSetOneRegister(pParams->hDemod,
							   RSTV0900_P2_CARFREQ,
							   0x89);
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0900_P2_CARHDR, 0x40);

			} else if (pParams->Demod2SymbolRate < 10000000) {
				/*2Msps<=SR <10Msps */
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0900_P2_CARFREQ, 0x4C);
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0900_P2_CARHDR, 0x20);
			} else {	/*SR >=10Msps */
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0900_P2_CARFREQ, 0x4B);
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0900_P2_CARHDR, 0x20);
			}
		} else {
			if (pParams->Demod2SymbolRate < 10000000)
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0900_P2_CARFREQ, 0xef);
			else
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0900_P2_CARFREQ, 0xed);
		}

		switch (pParams->Demod2SearchAlgo) {
		case FE_SAT_WARM_START:
			/*The symbol rate and the exact carrier Frequency are
			* known */
			/*Reset the Demod */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P2_DMDISTATE, 0x1F);
			/*Trig an acquisition (start the search) */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P2_DMDISTATE, 0x18);
			break;

		case FE_SAT_COLD_START:
			/*The symbol rate is known */
			/*Reset the Demod */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P2_DMDISTATE, 0x1F);
			/*Trig an acquisition (start the search) */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P2_DMDISTATE, 0x15);
			break;

		default:
			/*Nothing to do in case of blind search, blind  is
			 * handled by "FE_STV0900_BlindSearchAlgo" function */
			break;
		}
		break;
	}
}

U8 FE_STV0900_GetOptimCarrierLoop(S32 SymbolRate, FE_STV0900_ModCod_t ModCode,
				  S32 Pilots, U8 ChipID)
{
	U8 aclcValue = 0x29;
	U32 i = 0;
	struct FE_STV0900_CarLoopOPtim_s *carLoopS2;

	if (ChipID <= 0x12) {
		carLoopS2 = FE_STV0900_S2CarLoop;
		/* Find the index parameters for the Modulation */
		while ((i < NB_SAT_MODCOD_900CUT1X)
		       && (ModCode != FE_STV0900_S2CarLoop[i].ModCode))
			i++;
	} else if (ChipID == 0x20) {
		carLoopS2 = FE_STV0900_S2CarLoopCut20;
		while ((i < NB_SAT_MODCOD_900CUT2_CUT3)
		       && (ModCode != FE_STV0900_S2CarLoopCut20[i].ModCode))
			i++;
	} else {
		carLoopS2 = FE_STV0900_S2CarLoopCut30;
		while ((i < NB_SAT_MODCOD_900CUT2_CUT3)
		       && (ModCode != FE_STV0900_S2CarLoopCut30[i].ModCode))
			i++;
	}

	if (((ChipID <= 0x12) && (i < NB_SAT_MODCOD_900CUT1X))
	    || ((ChipID >= 0x20) && (i < NB_SAT_MODCOD_900CUT2_CUT3))) {
		if (Pilots) {
			if (SymbolRate <= 3000000)
				aclcValue = carLoopS2[i].CarLoopPilotsOn_2;
			else if (SymbolRate <= 7000000)
				aclcValue = carLoopS2[i].CarLoopPilotsOn_5;
			else if (SymbolRate <= 15000000)
				aclcValue = carLoopS2[i].CarLoopPilotsOn_10;
			else if (SymbolRate <= 25000000)
				aclcValue = carLoopS2[i].CarLoopPilotsOn_20;
			else
				aclcValue = carLoopS2[i].CarLoopPilotsOn_30;
		} else {
			if (SymbolRate <= 3000000)
				aclcValue = carLoopS2[i].CarLoopPilotsOff_2;
			else if (SymbolRate <= 7000000)
				aclcValue = carLoopS2[i].CarLoopPilotsOff_5;
			else if (SymbolRate <= 15000000)
				aclcValue = carLoopS2[i].CarLoopPilotsOff_10;
			else if (SymbolRate <= 25000000)
				aclcValue = carLoopS2[i].CarLoopPilotsOff_20;
			else
				aclcValue = carLoopS2[i].CarLoopPilotsOff_30;
		}
	} else {
		/* Modulation Unknown */
	}
	return aclcValue;
}

U8 FE_STV0900_GetOptimShortCarrierLoop(S32 SymbolRate,
				       FE_STV0900_Modulation_t Modulation,
				       U8 ChipID)
{
	S32 ModIndex = 0;

	U8 aclcValue = 0x0b;

	switch (Modulation) {
	case FE_SAT_MOD_QPSK:
	default:
		ModIndex = 0;
		break;

	case FE_SAT_MOD_8PSK:
		ModIndex = 1;
		break;

	case FE_SAT_MOD_16APSK:
		ModIndex = 2;
		break;

	case FE_SAT_MOD_32APSK:
		ModIndex = 3;
		break;
	}

	if (ChipID >= 0x30) {
		if (SymbolRate <= 3000000)
			/*3Msps */
			aclcValue =
			    FE_STV0900_S2ShortCarLoopCut30[ModIndex].CarLoop_2;
		else if (SymbolRate <= 7000000)
			/*7Msps */
			aclcValue =
			    FE_STV0900_S2ShortCarLoopCut30[ModIndex].CarLoop_5;
		else if (SymbolRate <= 15000000)
			/*15Msps */
			aclcValue =
			    FE_STV0900_S2ShortCarLoopCut30[ModIndex].CarLoop_10;
		else if (SymbolRate <= 25000000)
			/*25Msps */
			aclcValue =
			    FE_STV0900_S2ShortCarLoopCut30[ModIndex].CarLoop_20;
		else
			aclcValue =
			    FE_STV0900_S2ShortCarLoopCut30[ModIndex].CarLoop_30;

	} else if (ChipID >= 0x20) {
		if (SymbolRate <= 3000000)
			aclcValue =
			    FE_STV0900_S2ShortCarLoop[ModIndex].CarLoopCut20_2;
		else if (SymbolRate <= 7000000)
			aclcValue =
			    FE_STV0900_S2ShortCarLoop[ModIndex].CarLoopCut20_5;
		else if (SymbolRate <= 15000000)
			aclcValue =
			    FE_STV0900_S2ShortCarLoop[ModIndex].CarLoopCut20_10;
		else if (SymbolRate <= 25000000)
			aclcValue =
			    FE_STV0900_S2ShortCarLoop[ModIndex].CarLoopCut20_20;
		else
			aclcValue =
			    FE_STV0900_S2ShortCarLoop[ModIndex].CarLoopCut20_30;

	}

	else {

		if (SymbolRate <= 3000000)
			aclcValue =
			    FE_STV0900_S2ShortCarLoop[ModIndex].CarLoopCut12_2;
		else if (SymbolRate <= 7000000)
			aclcValue =
			    FE_STV0900_S2ShortCarLoop[ModIndex].CarLoopCut12_5;
		else if (SymbolRate <= 15000000)
			aclcValue =
			    FE_STV0900_S2ShortCarLoop[ModIndex].CarLoopCut12_10;
		else if (SymbolRate <= 25000000)
			aclcValue =
			    FE_STV0900_S2ShortCarLoop[ModIndex].CarLoopCut12_20;
		else
			aclcValue =
			    FE_STV0900_S2ShortCarLoop[ModIndex].CarLoopCut12_30;
	}

	return aclcValue;
}

/*****************************************************
--FUNCTION	::	TrackingOptimization
--ACTION	::	Set Optimized parameters for tracking
--PARAMS IN	::	pParams	->Pointer FE_STV0900_InternalParams_t structer
		::	Demod	->current demod 1 or 2
--PARAMS OUT::	NONE
--RETURN	::	none
--***************************************************/
void FE_STV0900_TrackingOptimization(FE_STV0900_InternalParams_t *pParams,
				     FE_STV0900_DEMOD_t Demod)
{
	S32 symbolRate,
	    pilots, aclc, freq1, freq0, i = 0, timed, timef, blindTunSw = 0;

	FE_STV0900_RollOff_t rolloff;
	FE_STV0900_ModCod_t foundModcod;

	/*      Read the Symbol rate    */
	symbolRate = FE_STV0900_GetSymbolRate(pParams->hDemod,
			pParams->MasterClock, Demod);
	symbolRate += FE_STV0900_TimingGetOffset(pParams->hDemod, symbolRate,
									Demod);
	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		switch (pParams->Demod1Results.Standard) {
		case FE_SAT_DVBS1_STANDARD:
			if (pParams->Demod1SearchStandard ==
							FE_SAT_AUTO_SEARCH) {
				ChipSetFieldImage(pParams->hDemod,
						  FSTV0900_P1_DVBS1_ENABLE, 1);
				ChipSetFieldImage(pParams->hDemod,
						  FSTV0900_P1_DVBS2_ENABLE, 0);
				ChipSetRegisters(pParams->hDemod,
						 RSTV0900_P1_DMDCFGMD, 1);
			}
			/*Set the rolloff to the manual value (given at the
			 * initialization) */
			ChipSetFieldImage(pParams->hDemod,
				FSTV0900_P1_ROLLOFF_CONTROL, pParams->RollOff);
			ChipSetFieldImage(pParams->hDemod,
					  FSTV0900_P1_MANUALSX_ROLLOFF, 1);
			ChipSetRegisters(pParams->hDemod, RSTV0900_P1_DEMOD, 1);

			if (pParams->hDemod->ChipID >= 0x30) {
				if (FE_STV0900_GetViterbiPunctureRate
				    (pParams->hDemod, Demod) == FE_SAT_PR_1_2) {
					ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P1_GAUSSR0, 0x98);
					ChipSetOneRegister(pParams->hDemod,
						   RSTV0900_P1_CCIR0, 0x18);
				} else {
					ChipSetOneRegister(pParams->hDemod,
						   RSTV0900_P1_GAUSSR0, 0x18);
					ChipSetOneRegister(pParams->hDemod,
						   RSTV0900_P1_CCIR0, 0x18);
				}
			}
			if (pParams->BER_Algo != FE_BER_Algo_Appli_Berwindow)
				/* force to viterbi bit error, Average window
				 * mode */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P1_ERRCTRL1, 0x75);
			else
				/* force to viterbi bit error, Infinite window
				 * mode */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P1_ERRCTRL1, 0x71);

			break;

		case FE_SAT_DSS_STANDARD:
			if (pParams->Demod1SearchStandard ==
							FE_SAT_AUTO_SEARCH) {
				ChipSetFieldImage(pParams->hDemod,
						  FSTV0900_P1_DVBS1_ENABLE, 1);
				ChipSetFieldImage(pParams->hDemod,
						  FSTV0900_P1_DVBS2_ENABLE, 0);
				ChipSetRegisters(pParams->hDemod,
						 RSTV0900_P1_DMDCFGMD, 1);
			}
			/*Set the rolloff to the manual value (given at the
			 * initialization) */
			ChipSetFieldImage(pParams->hDemod,
				FSTV0900_P1_ROLLOFF_CONTROL, pParams->RollOff);
			ChipSetFieldImage(pParams->hDemod,
					  FSTV0900_P1_MANUALSX_ROLLOFF, 1);
			ChipSetRegisters(pParams->hDemod, RSTV0900_P1_DEMOD, 1);

			if (pParams->hDemod->ChipID >= 0x30) {
				if (FE_STV0900_GetViterbiPunctureRate
				    (pParams->hDemod, Demod) == FE_SAT_PR_1_2) {
					ChipSetOneRegister(pParams->hDemod,
						   RSTV0900_P1_GAUSSR0, 0x98);
					ChipSetOneRegister(pParams->hDemod,
						   RSTV0900_P1_CCIR0, 0x18);
				} else {
					ChipSetOneRegister(pParams->hDemod,
						   RSTV0900_P1_GAUSSR0, 0x18);
					ChipSetOneRegister(pParams->hDemod,
						   RSTV0900_P1_CCIR0, 0x18);
				}
			}
			if (pParams->BER_Algo != FE_BER_Algo_Appli_Berwindow)
				/* force to viterbi bit error, Average window
				 * mode */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P1_ERRCTRL1, 0x75);
			else
				/* force to viterbi bit error, Infinite window
				 * mode */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P1_ERRCTRL1, 0x71);

			break;

		case FE_SAT_DVBS2_STANDARD:

			ChipSetFieldImage(pParams->hDemod,
					  FSTV0900_P1_DVBS1_ENABLE, 0);
			ChipSetFieldImage(pParams->hDemod,
					  FSTV0900_P1_DVBS2_ENABLE, 1);
			ChipSetRegisters(pParams->hDemod, RSTV0900_P1_DMDCFGMD,
					 1);

			/*stop DVBS1/DSS carrier loop  */
			ChipSetOneRegister(pParams->hDemod, RSTV0900_P1_ACLC,
					0);
			/*stop DVBS1/DSS carrier loop  */
			ChipSetOneRegister(pParams->hDemod, RSTV0900_P1_BCLC,
					0);

			if (pParams->Demod1Results.FrameLength ==
					FE_SAT_LONG_FRAME) {
				/*Carrier loop setting for lon frame */
				foundModcod = ChipGetField(pParams->hDemod,
						FSTV0900_P1_DEMOD_MODCOD);
				pilots = ChipGetFieldImage(pParams->hDemod,
						FSTV0900_P1_DEMOD_TYPE) & 0x01;

				aclc =
				      FE_STV0900_GetOptimCarrierLoop(symbolRate,
						foundModcod, pilots,
						pParams->hDemod->ChipID);
				if (foundModcod <= FE_SAT_QPSK_910)
					ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P1_ACLC2S2Q, aclc);
				else if (foundModcod <= FE_SAT_8PSK_910) {
					ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P1_ACLC2S2Q, 0x2a);
					ChipSetOneRegister(pParams->hDemod,
						   RSTV0900_P1_ACLC2S28, aclc);
				}

				if ((pParams->DemodType == FE_SAT_SINGLE)
				    && (foundModcod > FE_SAT_8PSK_910)) {
					if (foundModcod <= FE_SAT_16APSK_910) {
						ChipSetOneRegister(
						    pParams->hDemod,
						    RSTV0900_P1_ACLC2S2Q, 0x2a);
						ChipSetOneRegister(
						  pParams->hDemod,
						  RSTV0900_P1_ACLC2S216A, aclc);
					} else if (foundModcod <=
						   FE_SAT_32APSK_910) {
						ChipSetOneRegister(
						    pParams->hDemod,
						    RSTV0900_P1_ACLC2S2Q, 0x2a);
						ChipSetOneRegister(
						  pParams->hDemod,
						  RSTV0900_P1_ACLC2S232A, aclc);
					}
				}
			} else {
				/*Carrier loop setting for short frame */
				aclc =
				    FE_STV0900_GetOptimShortCarrierLoop
				    (symbolRate,
				     pParams->Demod1Results.Modulation,
				     pParams->hDemod->ChipID);
				if (pParams->Demod1Results.Modulation ==
				    FE_SAT_MOD_QPSK)
					ChipSetOneRegister(pParams->hDemod,
							   RSTV0900_P1_ACLC2S2Q,
							   aclc);

				else if (pParams->Demod1Results.Modulation ==
					 FE_SAT_MOD_8PSK) {
					ChipSetOneRegister(pParams->hDemod,
							   RSTV0900_P1_ACLC2S2Q,
							   0x2a);
					ChipSetOneRegister(pParams->hDemod,
							   RSTV0900_P1_ACLC2S28,
							   aclc);
				} else if (pParams->Demod1Results.Modulation ==
					   FE_SAT_MOD_16APSK) {
					ChipSetOneRegister(pParams->hDemod,
							   RSTV0900_P1_ACLC2S2Q,
							   0x2a);
					ChipSetOneRegister(pParams->hDemod,
							RSTV0900_P1_ACLC2S216A,
							aclc);
				} else if (pParams->Demod1Results.Modulation ==
					   FE_SAT_MOD_32APSK) {
					ChipSetOneRegister(pParams->hDemod,
							   RSTV0900_P1_ACLC2S2Q,
							   0x2a);
					ChipSetOneRegister(pParams->hDemod,
							RSTV0900_P1_ACLC2S232A,
							aclc);
				}

			}
			/* for cut 1.1 and 1.0 If the 900 is used in dual
			 * enable the link demod to LDPC only after demod lock*/
			if ((pParams->hDemod->ChipID <= 0x11) &&
					(pParams->DemodType != FE_SAT_SINGLE))
				/*For PATH1 cut 1.0 and 1.1 only */
				/*do not use FRESVIT1 field it may have an
				 * impacte on the VITERBI2 */
				FE_STV0900_ActivateS2ModCode(pParams->hDemod,
						Demod);
			if (pParams->BER_Algo != FE_BER_Algo_Appli_Berwindow)
				/* force to DVBS2 PER, Average window mode */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P1_ERRCTRL1, 0x67);
			else
				/* force to DVBS2 PER, Infinite window mode */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P1_ERRCTRL1, 0x61);

			if (pParams->hDemod->ChipID >= 0x30) {
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0900_P1_GAUSSR0, 0xac);
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0900_P1_CCIR0, 0x2c);
			}

			break;

		case FE_SAT_UNKNOWN_STANDARD:
		default:
			ChipSetFieldImage(pParams->hDemod,
					  FSTV0900_P1_DVBS1_ENABLE, 1);
			ChipSetFieldImage(pParams->hDemod,
					  FSTV0900_P1_DVBS2_ENABLE, 1);
			ChipSetRegisters(pParams->hDemod, RSTV0900_P1_DMDCFGMD,
					 1);
			break;
		}

		/*read the carrier frequency */
		freq1 = ChipGetOneRegister(pParams->hDemod, RSTV0900_P1_CFR2);
		freq0 = ChipGetOneRegister(pParams->hDemod, RSTV0900_P1_CFR1);
		/*      Read the rolloff                */
		rolloff = ChipGetField(pParams->hDemod,
				FSTV0900_P1_ROLLOFF_STATUS);

		if (pParams->Demod1SearchAlgo == FE_SAT_BLIND_SEARCH) {
			ChipSetOneRegister(pParams->hDemod, RSTV0900_P1_SFRSTEP,
					   0x00);

			/* stop the carrier frequency search loop */
			ChipSetFieldImage(pParams->hDemod,
					FSTV0900_P1_CFR_AUTOSCAN, 0);
			/* stop the carrier frequency search loop */
			ChipSetFieldImage(pParams->hDemod,
					FSTV0900_P1_SCAN_ENABLE, 0);
			ChipSetRegisters(pParams->hDemod, RSTV0900_P1_DMDCFGMD,
					 1);

			ChipSetOneRegister(pParams->hDemod, RSTV0900_P1_TMGCFG2,
					   0xC1);
			/*Set the init symbol rate to the found value */
			FE_STV0900_SetSymbolRate(pParams->hDemod,
					pParams->MasterClock, symbolRate,
					Demod);
			blindTunSw = 1;

			if (pParams->Demod1Results.Standard !=
					FE_SAT_DVBS2_STANDARD)
				/* case DVBS1 or DDSS update carrier 1 loop */
				FE_STV0900_SetDVBS1_TrackCarLoop(pParams->
						hDemod, Demod, symbolRate);
		}

		if (pParams->hDemod->ChipID < 0x20)
			/*Not applicable for cut 2.0 */
			ChipSetOneRegister(pParams->hDemod, RSTV0900_P1_CARHDR,
					0x08);

		if (pParams->hDemod->ChipID == 0x10)
			/*Set Correlation frames reg for Tracking for cut 1.0
			 * only */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P1_CORRELEXP, 0x0A);

		/*set the AGC2 ref to the tracking value */
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P1_AGC2REF, 0x38);

		/*Set SFR Up to auto mode for tracking */
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P1_SFRUP1, 0x80);
		/*Set SFR Low to auto mode for tracking */
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P1_SFRLOW1, 0x80);

		if ((pParams->hDemod->ChipID >= 0x20) || (blindTunSw == 1)
		    || (pParams->Demod1SymbolRate < 10000000)) {
			/*update the init carrier freq with the found freq
			 * offset */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P1_CFRINIT1, freq1);
			ChipSetOneRegister(pParams->hDemod,
					   RSTV0900_P1_CFRINIT0, freq0);
			pParams->Tuner1BW =
			    FE_STV0900_CarrierWidth(symbolRate,
					    pParams->RollOff) + 10000000;

			if (((pParams->hDemod->ChipID >= 0x20) ||
			    (blindTunSw == 1)) &&
			     (pParams->Demod1SearchAlgo != FE_SAT_WARM_START)) {
				if (pParams->Tuner1Type == FE_SAT_SW_TUNER)
					FE_Sat_TunerSetBandwidth
						(pParams->hTuner1,
						 pParams->Tuner1BW);
				else
					/*Update the Tuner BW */
					FE_STV0900_SetTuner(pParams->hDemod,
						     pParams->hTuner1,
						     pParams->Tuner1Type, Demod,
						     pParams->Tuner1Frequency,
						     pParams->Tuner1BW);
			}
			if ((pParams->Demod1SearchAlgo == FE_SAT_BLIND_SEARCH)
			    || (pParams->Demod1SymbolRate < 10000000))
				/*if blind search wait 50ms for SR
				 * stabilization */
				ChipWaitOrAbort(pParams->hDemod, 50);
			else
				ChipWaitOrAbort(pParams->hDemod, 5);

			FE_STV0900_GetLockTimeout(&timed, &timef, symbolRate,
						  FE_SAT_WARM_START);

			if (FE_STV0900_GetDemodLock(pParams->hDemod, Demod,
						timed / 2) == FALSE) {
				/*stop the demod */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P1_DMDISTATE, 0x1F);
				/*init the frequency offset */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P1_CFRINIT1, freq1);
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0900_P1_CFRINIT0, freq0);
				/*stop the demod */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P1_DMDISTATE, 0x18);
				i = 0;
				while ((FE_STV0900_GetDemodLock(pParams->hDemod,
				      Demod, timed / 2) == FALSE) && (i <= 2)) {
					/*if the demod lose lock because of
					 * tuner BW ref change */
					/*stop the demod */
					ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P1_DMDISTATE, 0x1F);
					/*init the frequency offset */
					ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P1_CFRINIT1, freq1);
					ChipSetOneRegister(pParams->hDemod,
						   RSTV0900_P1_CFRINIT0, freq0);

					/*stop the demod */
					ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P1_DMDISTATE, 0x18);
					i++;
				}
			}

		}

		if (pParams->hDemod->ChipID >= 0x20) {
			ChipSetOneRegister(pParams->hDemod, RSTV0900_P1_CARFREQ,
					   0x49);
			if ((pParams->Demod1SearchStandard ==
			     FE_SAT_SEARCH_DVBS1)
			    || (pParams->Demod1SearchStandard ==
				FE_SAT_SEARCH_DSS)
			    || (pParams->Demod1SearchStandard ==
				FE_SAT_AUTO_SEARCH)) {
				/*set Viterbi hysteresis for tracking */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P1_VAVSRVIT, 0x0a);
				/* close the ReedSolomon to viterbi feed back
				 * until demod lock */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P1_VITSCALE, 0x0);
			}
		}

		if ((pParams->Demod1Results.Standard == FE_SAT_DVBS1_STANDARD)
		    || (pParams->Demod1Results.Standard == FE_SAT_DSS_STANDARD))
			FE_STV0900_SetViterbiThTracq(pParams->hDemod, Demod);
		else if (pParams->hDemod->ChipID >= 0x30)
			/*applicable for cut 3.0 */
			ChipSetOneRegister(pParams->hDemod, RSTV0900_P1_CARHDR,
					0x10);

		break;

	case FE_SAT_DEMOD_2:

		switch (pParams->Demod2Results.Standard) {
		case FE_SAT_DVBS1_STANDARD:
			if (pParams->Demod2SearchStandard ==
					FE_SAT_AUTO_SEARCH) {
				ChipSetFieldImage(pParams->hDemod,
						  FSTV0900_P2_DVBS1_ENABLE, 1);
				ChipSetFieldImage(pParams->hDemod,
						  FSTV0900_P2_DVBS2_ENABLE, 0);
				ChipSetRegisters(pParams->hDemod,
						 RSTV0900_P2_DMDCFGMD, 1);
			}
			/*Set the rolloff to the manual value (given at the
			 * initialization) */
			ChipSetFieldImage(pParams->hDemod,
					  FSTV0900_P2_ROLLOFF_CONTROL,
					  pParams->RollOff);
			ChipSetFieldImage(pParams->hDemod,
					  FSTV0900_P2_MANUALSX_ROLLOFF, 1);
			ChipSetRegisters(pParams->hDemod, RSTV0900_P2_DEMOD, 1);

			if (pParams->hDemod->ChipID >= 0x30) {
				if (FE_STV0900_GetViterbiPunctureRate
				    (pParams->hDemod, Demod) == FE_SAT_PR_1_2) {
					ChipSetOneRegister(pParams->hDemod,
							   RSTV0900_P2_GAUSSR0,
							   0x98);
					ChipSetOneRegister(pParams->hDemod,
							   RSTV0900_P2_CCIR0,
							   0x18);
				} else {
					ChipSetOneRegister(pParams->hDemod,
							   RSTV0900_P2_GAUSSR0,
							   0x18);
					ChipSetOneRegister(pParams->hDemod,
							   RSTV0900_P2_CCIR0,
							   0x18);
				}
			}
			if (pParams->BER_Algo != FE_BER_Algo_Appli_Berwindow)
				/* force to viterbi bit error, average window
				 * mode */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P2_ERRCTRL1, 0x75);
			else
				/* force to viterbi bit error, average window
				 * mode */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P2_ERRCTRL1, 0x71);

			break;

		case FE_SAT_DSS_STANDARD:
			if (pParams->Demod2SearchStandard ==
					FE_SAT_AUTO_SEARCH) {
				ChipSetFieldImage(pParams->hDemod,
						  FSTV0900_P2_DVBS1_ENABLE, 1);
				ChipSetFieldImage(pParams->hDemod,
						  FSTV0900_P2_DVBS2_ENABLE, 0);
				ChipSetRegisters(pParams->hDemod,
						 RSTV0900_P2_DMDCFGMD, 1);
			}
			/*Set the rolloff to the manual value (given at the
			 * initialization) */
			ChipSetFieldImage(pParams->hDemod,
					  FSTV0900_P2_ROLLOFF_CONTROL,
					  pParams->RollOff);
			ChipSetFieldImage(pParams->hDemod,
					  FSTV0900_P2_MANUALSX_ROLLOFF, 1);
			ChipSetRegisters(pParams->hDemod, RSTV0900_P2_DEMOD, 1);

			if (pParams->hDemod->ChipID >= 0x30) {
				if (FE_STV0900_GetViterbiPunctureRate
				    (pParams->hDemod, Demod) == FE_SAT_PR_1_2) {
					ChipSetOneRegister(pParams->hDemod,
							   RSTV0900_P2_GAUSSR0,
							   0x98);
					ChipSetOneRegister(pParams->hDemod,
							   RSTV0900_P2_CCIR0,
							   0x18);
				} else {
					ChipSetOneRegister(pParams->hDemod,
							   RSTV0900_P2_GAUSSR0,
							   0x18);
					ChipSetOneRegister(pParams->hDemod,
							   RSTV0900_P2_CCIR0,
							   0x18);
				}
			}
			if (pParams->BER_Algo != FE_BER_Algo_Appli_Berwindow)
				/* force to viterbi bit error, average window
				 * mode */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P2_ERRCTRL1, 0x75);
			else
				/* force to viterbi bit error, infinite window
				 * mode */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P2_ERRCTRL1, 0x71);

			break;

		case FE_SAT_DVBS2_STANDARD:
			ChipSetFieldImage(pParams->hDemod,
					  FSTV0900_P2_DVBS1_ENABLE, 0);
			ChipSetFieldImage(pParams->hDemod,
					  FSTV0900_P2_DVBS2_ENABLE, 1);
			ChipSetRegisters(pParams->hDemod, RSTV0900_P2_DMDCFGMD,
					 1);

			/*stop DVBS1/DSS carrier loop  */
			ChipSetOneRegister(pParams->hDemod, RSTV0900_P2_ACLC,
					0);
			/*stop DVBS1/DSS carrier loop  */
			ChipSetOneRegister(pParams->hDemod, RSTV0900_P2_BCLC,
					0);

			if (pParams->Demod2Results.FrameLength ==
			    FE_SAT_LONG_FRAME) {
				/*Carrier loop setting for long frame */
				foundModcod =
				    ChipGetField(pParams->hDemod,
						 FSTV0900_P2_DEMOD_MODCOD);
				pilots =
				    ChipGetFieldImage(pParams->hDemod,
					      FSTV0900_P2_DEMOD_TYPE) & 0x01;

				aclc =
				    FE_STV0900_GetOptimCarrierLoop(symbolRate,
						   foundModcod, pilots,
						   pParams->hDemod->ChipID);
				if (foundModcod <= FE_SAT_QPSK_910)
					ChipSetOneRegister(pParams->hDemod,
						   RSTV0900_P2_ACLC2S2Q, aclc);
				else if (foundModcod <= FE_SAT_8PSK_910) {
					/*stop the carrier update for QPSK */
					ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P2_ACLC2S2Q, 0x2a);
					ChipSetOneRegister(pParams->hDemod,
						   RSTV0900_P2_ACLC2S28, aclc);
				}

				if ((pParams->DemodType == FE_SAT_SINGLE)
				    && (foundModcod > FE_SAT_8PSK_910)) {
					if (foundModcod <= FE_SAT_16APSK_910) {
						/*stop the carrier update for
						 * QPSK */
						ChipSetOneRegister(
						    pParams->hDemod,
						    RSTV0900_P2_ACLC2S2Q, 0x2a);
						ChipSetOneRegister(
						  pParams->hDemod,
						  RSTV0900_P2_ACLC2S216A, aclc);
					} else if (foundModcod <=
						   FE_SAT_32APSK_910) {
						/*stop the carrier update for
						 * QPSK */
						ChipSetOneRegister(
						    pParams->hDemod,
						    RSTV0900_P2_ACLC2S2Q, 0x2a);
						ChipSetOneRegister(
						  pParams->hDemod,
						  RSTV0900_P2_ACLC2S232A, aclc);
					}

				}
			} else {
				/*Carrier loop setting for short frame */
				aclc =
				    FE_STV0900_GetOptimShortCarrierLoop
				    (symbolRate,
				     pParams->Demod2Results.Modulation,
				     pParams->hDemod->ChipID);
				if (pParams->Demod2Results.Modulation ==
				    FE_SAT_MOD_QPSK)
					ChipSetOneRegister(pParams->hDemod,
							   RSTV0900_P2_ACLC2S2Q,
							   aclc);

				else if (pParams->Demod2Results.Modulation ==
					 FE_SAT_MOD_8PSK) {
					ChipSetOneRegister(pParams->hDemod,
						   RSTV0900_P2_ACLC2S2Q, 0x2a);
					ChipSetOneRegister(pParams->hDemod,
						   RSTV0900_P2_ACLC2S28, aclc);
				} else if (pParams->Demod2Results.Modulation ==
					   FE_SAT_MOD_16APSK) {
					ChipSetOneRegister(pParams->hDemod,
						   RSTV0900_P2_ACLC2S2Q, 0x2a);
					ChipSetOneRegister(pParams->hDemod,
					   RSTV0900_P2_ACLC2S216A, aclc);
				} else if (pParams->Demod2Results.Modulation ==
					   FE_SAT_MOD_32APSK) {
					ChipSetOneRegister(pParams->hDemod,
						   RSTV0900_P2_ACLC2S2Q, 0x2a);
					ChipSetOneRegister(pParams->hDemod,
					   RSTV0900_P2_ACLC2S232A, aclc);
				}
			}
			/*do not use FRESVIT2 field it may have an impacte on
			 * the VITERBI1 */
			if (pParams->BER_Algo != FE_BER_Algo_Appli_Berwindow)
				/* force to DVBS2 PER, Average window mode  */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P2_ERRCTRL1, 0x67);
			else
				/* force to DVBS2 PER, Infinite window mode  */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P2_ERRCTRL1, 0x61);

			if (pParams->hDemod->ChipID >= 0x30) {
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0900_P2_GAUSSR0, 0xac);
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0900_P2_CCIR0, 0x2c);
			}

			break;

		case FE_SAT_UNKNOWN_STANDARD:
		default:
			ChipSetFieldImage(pParams->hDemod,
					  FSTV0900_P2_DVBS1_ENABLE, 1);
			ChipSetFieldImage(pParams->hDemod,
					  FSTV0900_P2_DVBS2_ENABLE, 1);
			ChipSetRegisters(pParams->hDemod, RSTV0900_P2_DMDCFGMD,
					 1);
			break;
		}

		/*read the carrier frequency */
		freq1 = ChipGetOneRegister(pParams->hDemod, RSTV0900_P2_CFR2);
		freq0 = ChipGetOneRegister(pParams->hDemod, RSTV0900_P2_CFR1);
		/*      Read the rolloff                */
		rolloff = ChipGetField(pParams->hDemod,
				FSTV0900_P2_ROLLOFF_STATUS);

		if (pParams->Demod2SearchAlgo == FE_SAT_BLIND_SEARCH) {
			ChipSetOneRegister(pParams->hDemod, RSTV0900_P2_SFRSTEP,
					   0x00);

			/* stop the carrier frequency search loop */
			ChipSetFieldImage(pParams->hDemod,
					FSTV0900_P2_CFR_AUTOSCAN, 0);
			/* stop the carrier frequency search loop */
			ChipSetFieldImage(pParams->hDemod,
					FSTV0900_P2_SCAN_ENABLE, 0);
			ChipSetRegisters(pParams->hDemod, RSTV0900_P2_DMDCFGMD,
					 1);

			ChipSetOneRegister(pParams->hDemod, RSTV0900_P2_TMGCFG2,
					   0xC1);
			/*Set the init symbol rate to the found value */
			FE_STV0900_SetSymbolRate(pParams->hDemod,
					pParams->MasterClock, symbolRate,
					Demod);
			blindTunSw = 1;

			if (pParams->Demod2Results.Standard !=
					FE_SAT_DVBS2_STANDARD)
				/* case DVBS1 or DDSS update carrier 1 loop */
				FE_STV0900_SetDVBS1_TrackCarLoop(
					pParams->hDemod, Demod, symbolRate);
		}

		if (pParams->hDemod->ChipID < 0x20)
			/*Not applicable for cut 2.0 */
			ChipSetOneRegister(pParams->hDemod, RSTV0900_P2_CARHDR,
					0x08);

		if (pParams->hDemod->ChipID == 0x10)
			/*Set Correlation frames reg for Tracking for cut 1.0
			 * only */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P2_CORRELEXP, 0x0A);

		/*Set the AGC2 ref to the tracking value */
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P2_AGC2REF, 0x38);

		/*Set SFR Up to auto mode for tracking */
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P2_SFRUP1, 0x80);
		/*Set SFR Low to auto mode for tracking */
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P2_SFRLOW1, 0x80);

		if ((pParams->hDemod->ChipID >= 0x20) || (blindTunSw == 1)
		    || (pParams->Demod2SymbolRate < 10000000)) {
			/*update the init carrier freq with the found freq
			 * offset */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P2_CFRINIT1, freq1);
			ChipSetOneRegister(pParams->hDemod,
					   RSTV0900_P2_CFRINIT0, freq0);
			pParams->Tuner2BW = FE_STV0900_CarrierWidth(symbolRate,
						pParams->RollOff) + 10000000;

			if (((pParams->hDemod->ChipID >= 0x20)
			    || (blindTunSw == 1)) &&
			     (pParams->Demod2SearchAlgo != FE_SAT_WARM_START)) {
				if (pParams->Tuner2Type == FE_SAT_SW_TUNER)
					FE_Sat_TunerSetBandwidth
						(pParams->hTuner2,
						 pParams->Tuner2BW);
				else
					/*Update the Tuner BW */
					FE_STV0900_SetTuner(pParams->hDemod,
						pParams->hTuner2,
						pParams->Tuner2Type, Demod,
						pParams->Tuner2Frequency,
						pParams->Tuner2BW);
			}
			if ((pParams->Demod2SearchAlgo == FE_SAT_BLIND_SEARCH)
			    || (pParams->Demod2SymbolRate < 10000000))
				/*if blind search wait 50ms for SR
				 * stabilization */
				ChipWaitOrAbort(pParams->hDemod, 50);
			else
				ChipWaitOrAbort(pParams->hDemod, 5);

			FE_STV0900_GetLockTimeout(&timed, &timef, symbolRate,
						  FE_SAT_WARM_START);

			if (FE_STV0900_GetDemodLock
			    (pParams->hDemod, Demod, timed / 2) == FALSE) {
				/*stop the demod */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P2_DMDISTATE, 0x1F);
				/*init the frequency offset */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P2_CFRINIT1, freq1);
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0900_P2_CFRINIT0, freq0);
				/*stop the demod */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P2_DMDISTATE, 0x18);
				i = 0;
				while ((FE_STV0900_GetDemodLock(pParams->hDemod,
				      Demod, timed / 2) == FALSE) && (i <= 2)) {
					/*if the demod lose lock because of
					 * tuner BW ref change */
					/*stop the demod */
					ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P2_DMDISTATE, 0x1F);
					/*init the frequency offset */
					ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P2_CFRINIT1, freq1);
					ChipSetOneRegister(pParams->hDemod,
						   RSTV0900_P2_CFRINIT0, freq0);

					/*stop the demod */
					ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P2_DMDISTATE, 0x18);
					i++;
				}
			}
		}

		if (pParams->hDemod->ChipID >= 0x20) {
			ChipSetOneRegister(pParams->hDemod, RSTV0900_P2_CARFREQ,
					   0x49);

			if ((pParams->Demod2SearchStandard ==
							FE_SAT_SEARCH_DVBS1)
			    || (pParams->Demod2SearchStandard ==
							FE_SAT_SEARCH_DSS)
			    || (pParams->Demod2SearchStandard ==
							FE_SAT_AUTO_SEARCH)) {
				/*set Viterbi hysteresis for tracking */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P2_VAVSRVIT, 0x0a);
				/* close the ReedSolomon to viterbi feed back
				 * until demod lock */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P2_VITSCALE, 0x0);
			}
		}
		if ((pParams->Demod2Results.Standard == FE_SAT_DVBS1_STANDARD)
		    || (pParams->Demod2Results.Standard == FE_SAT_DSS_STANDARD))
			FE_STV0900_SetViterbiThTracq(pParams->hDemod, Demod);
		else if (pParams->hDemod->ChipID >= 0x30)
			/*applicable for cut 3.0 */
			ChipSetOneRegister(pParams->hDemod, RSTV0900_P2_CARHDR,
					0x10);

		break;
	}

}

/*****************************************************
--FUNCTION	::	GetSignalParams
--ACTION	::	Read signal caracteristics
--PARAMS IN	::	pParams	->Pointer FE_STV0900_InternalParams_t structer
		::	Demod	->current demod 1 or 2
--PARAMS OUT::	NONE
--RETURN	::	RANGE Ok or not
--***************************************************/
FE_STV0900_SIGNALTYPE_t FE_STV0900_GetSignalParams(FE_STV0900_InternalParams_t *
						   pParams,
						   FE_STV0900_DEMOD_t Demod)
{
	FE_STV0900_SIGNALTYPE_t range = FE_SAT_OUTOFRANGE;
	S32 offsetFreq, symbolRateOffset, i = 0;

	U8 timing;

	ChipWaitOrAbort(pParams->hDemod, 5);
	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:

		if (pParams->Demod1SearchAlgo == FE_SAT_BLIND_SEARCH) {
			/* if Blind search wait for symbol rate offset
			 * information transfert from the timing loop to the
			   demodulator symbol rate
			 */
			timing =
			    ChipGetOneRegister(pParams->hDemod,
					       RSTV0900_P1_TMGREG2);
			i = 0;
			ChipSetOneRegister(pParams->hDemod, RSTV0900_P1_SFRSTEP,
					   0x5c);
			while ((i <= 50) && (timing != 0) && (timing != 0xFF)) {
				timing =
				    ChipGetOneRegister(pParams->hDemod,
						       RSTV0900_P1_TMGREG2);
				ChipWaitOrAbort(pParams->hDemod, 5);
				i += 5;
			}
		}
		pParams->Demod1Results.Standard =
		    FE_STV0900_GetStandard(pParams->hDemod, Demod);

		pParams->Demod1Results.Frequency =
		    FE_STV0900_GetTunerFrequency(pParams->hDemod,
						 pParams->hTuner1,
						 pParams->Tuner1Type, Demod);
		offsetFreq =
		    FE_STV0900_GetCarrierFrequency(pParams->hDemod,
						   pParams->MasterClock,
						   Demod) / 1000;
		pParams->Demod1Results.Frequency += offsetFreq;

		pParams->Demod1Results.SymbolRate =
		    FE_STV0900_GetSymbolRate(pParams->hDemod,
					     pParams->MasterClock, Demod);
		symbolRateOffset =
		    FE_STV0900_TimingGetOffset(pParams->hDemod,
					       pParams->Demod1Results.
					       SymbolRate, Demod);
		pParams->Demod1Results.SymbolRate += symbolRateOffset;
		pParams->Demod1Results.PunctureRate =
		    FE_STV0900_GetViterbiPunctureRate(pParams->hDemod, Demod);
		pParams->Demod1Results.ModCode =
		    ChipGetField(pParams->hDemod, FSTV0900_P1_DEMOD_MODCOD);
		pParams->Demod1Results.Pilots =
		    ChipGetFieldImage(pParams->hDemod,
				      FSTV0900_P1_DEMOD_TYPE) & 0x01;
		pParams->Demod1Results.FrameLength =
		    ((U32)
		     ChipGetFieldImage(pParams->hDemod,
				       FSTV0900_P1_DEMOD_TYPE)) >> 1;

		pParams->Demod1Results.RollOff =
		    ChipGetField(pParams->hDemod, FSTV0900_P1_ROLLOFF_STATUS);

		switch (pParams->Demod1Results.Standard) {
		case FE_SAT_DVBS2_STANDARD:
			pParams->Demod1Results.Spectrum =
			    ChipGetField(pParams->hDemod,
					 FSTV0900_P1_SPECINV_DEMOD);

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
			    ChipGetField(pParams->hDemod, FSTV0900_P1_IQINV);

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
			offsetFreq =
			    pParams->Demod1Results.Frequency -
			    pParams->Tuner1Frequency;
			pParams->Tuner1Frequency =
			    FE_STV0900_GetTunerFrequency(pParams->hDemod,
							 pParams->hTuner1,
							 pParams->Tuner1Type,
							 FE_SAT_DEMOD_1);

			if (ABS(offsetFreq) <=
			    ((pParams->Demod1SearchRange / 2000) + 500))
				range = FE_SAT_RANGEOK;
			else if (ABS(offsetFreq) <=
				 (FE_STV0900_CarrierWidth
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
			   demodulator symbol rate
			 */
			timing =
			    ChipGetOneRegister(pParams->hDemod,
					       RSTV0900_P2_TMGREG2);
			i = 0;
			ChipSetOneRegister(pParams->hDemod, RSTV0900_P2_SFRSTEP,
					   0x5c);
			while ((i <= 50) && (timing != 0) && (timing != 0xFF)) {
				timing =
				    ChipGetOneRegister(pParams->hDemod,
						       RSTV0900_P2_TMGREG2);
				ChipWaitOrAbort(pParams->hDemod, 5);
				i += 5;
			}
		}

		pParams->Demod2Results.Standard =
		    FE_STV0900_GetStandard(pParams->hDemod, Demod);

		pParams->Demod2Results.Frequency =
		    FE_STV0900_GetTunerFrequency(pParams->hDemod,
						 pParams->hTuner2,
						 pParams->Tuner2Type, Demod);
		offsetFreq =
		    FE_STV0900_GetCarrierFrequency(pParams->hDemod,
						   pParams->MasterClock,
						   Demod) / 1000;
		pParams->Demod2Results.Frequency += offsetFreq;

		pParams->Demod2Results.SymbolRate =
		    FE_STV0900_GetSymbolRate(pParams->hDemod,
					     pParams->MasterClock, Demod);
		symbolRateOffset =
		    FE_STV0900_TimingGetOffset(pParams->hDemod,
					       pParams->Demod2Results.
					       SymbolRate, Demod);
		pParams->Demod2Results.SymbolRate += symbolRateOffset;
		pParams->Demod2Results.PunctureRate =
		    FE_STV0900_GetViterbiPunctureRate(pParams->hDemod, Demod);
		pParams->Demod2Results.ModCode =
		    ChipGetField(pParams->hDemod, FSTV0900_P2_DEMOD_MODCOD);
		pParams->Demod2Results.Pilots =
		    ChipGetFieldImage(pParams->hDemod,
				      FSTV0900_P2_DEMOD_TYPE) & 0x01;
		pParams->Demod2Results.FrameLength =
		    ((U32)
		     ChipGetFieldImage(pParams->hDemod,
				       FSTV0900_P2_DEMOD_TYPE)) >> 1;
		pParams->Demod2Results.RollOff =
		    ChipGetField(pParams->hDemod, FSTV0900_P2_ROLLOFF_STATUS);

		switch (pParams->Demod2Results.Standard) {
		case FE_SAT_DVBS2_STANDARD:
			pParams->Demod2Results.Spectrum =
			    ChipGetField(pParams->hDemod,
					 FSTV0900_P2_SPECINV_DEMOD);

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
			    ChipGetField(pParams->hDemod, FSTV0900_P2_IQINV);

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
			offsetFreq =
			    pParams->Demod2Results.Frequency -
			    pParams->Tuner2Frequency;
			pParams->Tuner2Frequency =
			    FE_STV0900_GetTunerFrequency(pParams->hDemod,
							 pParams->hTuner2,
							 pParams->Tuner2Type,
							 FE_SAT_DEMOD_2);

			if (ABS(offsetFreq) <=
			    ((pParams->Demod2SearchRange / 2000) + 500))
				range = FE_SAT_RANGEOK;
			else if (ABS(offsetFreq) <=
				 (FE_STV0900_CarrierWidth
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

FE_STV0900_SIGNALTYPE_t
FE_STV0900_DVBS1AcqWorkAround(FE_STV0900_InternalParams_t *pParams,
			      FE_STV0900_DEMOD_t Demod)
{

	S32 symbolRate, demodTimeout, fecTimeout, freq1, freq0;
	FE_STV0900_SIGNALTYPE_t signalType = FE_SAT_NODATA;

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		pParams->Demod1Results.Locked = FALSE;
		if (ChipGetField(pParams->hDemod, FSTV0900_P1_HEADER_MODE) !=
				FE_SAT_DVBS_FOUND)
			return signalType;
		/*workaround for DVBS1 false lock */
		/*      Read the Symbol rate    */
		symbolRate = FE_STV0900_GetSymbolRate(pParams->hDemod,
				pParams->MasterClock, Demod);
		/*      Read the Symbol rate offset     */
		symbolRate += FE_STV0900_TimingGetOffset(pParams->hDemod,
				symbolRate, Demod);

		if (pParams->Demod1SearchAlgo == FE_SAT_BLIND_SEARCH)
			/*      Init the symbol rate    */
			FE_STV0900_SetSymbolRate(pParams->hDemod,
					pParams->MasterClock, symbolRate,
					Demod);

		FE_STV0900_GetLockTimeout(&demodTimeout, &fecTimeout,
				symbolRate, FE_SAT_WARM_START);
		freq1 = ChipGetOneRegister(pParams->hDemod, RSTV0900_P1_CFR2);
		freq0 = ChipGetOneRegister(pParams->hDemod, RSTV0900_P1_CFR1);
		/*stop the carrier frequency search loop */
		ChipSetField(pParams->hDemod, FSTV0900_P1_CFR_AUTOSCAN, 0);

		ChipSetField(pParams->hDemod, FSTV0900_P1_SPECINV_CONTROL,
				FE_SAT_IQ_FORCE_SWAPPED);
		/*stop the demod */
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P1_DMDISTATE,
				0x1C);
		/*init the frequency offset */
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P1_CFRINIT1,
				freq1);
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P1_CFRINIT0,
				freq0);
		/*trig a warm start */
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P1_DMDISTATE,
				0x18);

		if (FE_STV0900_WaitForLock(pParams->hDemod, Demod,
					demodTimeout, fecTimeout) == TRUE) {
			/*wait until Lock or timeout */
			pParams->Demod1Results.Locked = TRUE;
			/* Read signal caracteristics */
			signalType = FE_STV0900_GetSignalParams(pParams, Demod);
			/* Optimization setting for tracking */
			FE_STV0900_TrackingOptimization(pParams, Demod);
			return signalType;
		}
		ChipSetField(pParams->hDemod, FSTV0900_P1_SPECINV_CONTROL,
				FE_SAT_IQ_FORCE_NORMAL);
		/*stop the demod */
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P1_DMDISTATE,
				0x1C);
		/*init the frequency offset */
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P1_CFRINIT1,
				freq1);
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P1_CFRINIT0,
				freq0);
		/*trig a warm start */
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P1_DMDISTATE,
				0x18);

		if (FE_STV0900_WaitForLock(pParams->hDemod, Demod,
					demodTimeout, fecTimeout) == TRUE) {
			/*wait until Lock or timeout */
			pParams->Demod1Results.Locked = TRUE;
			/* Read signal caracteristics */
			signalType = FE_STV0900_GetSignalParams(pParams, Demod);
			/* Optimization setting for tracking */
			FE_STV0900_TrackingOptimization(pParams, Demod);
		}
		break;

	case FE_SAT_DEMOD_2:
		pParams->Demod2Results.Locked = FALSE;
		if (ChipGetField(pParams->hDemod, FSTV0900_P2_HEADER_MODE) !=
				FE_SAT_DVBS_FOUND)
			return signalType;

		/*workaround for DVBS1 false mock */
		/*      Read the Symbol rate    */
		symbolRate = FE_STV0900_GetSymbolRate(pParams->hDemod,
				pParams->MasterClock, Demod);
		/*      Read the Symbol rate offset     */
		symbolRate += FE_STV0900_TimingGetOffset(pParams->hDemod,
				symbolRate, Demod);
		if (pParams->Demod2SearchAlgo == FE_SAT_BLIND_SEARCH)
			/*      Init the symbol rate    */
			FE_STV0900_SetSymbolRate(pParams->hDemod,
				pParams->MasterClock, symbolRate, Demod);

		FE_STV0900_GetLockTimeout(&demodTimeout, &fecTimeout,
				symbolRate, FE_SAT_WARM_START);
		freq1 = ChipGetOneRegister(pParams->hDemod, RSTV0900_P2_CFR2);
		freq0 = ChipGetOneRegister(pParams->hDemod, RSTV0900_P2_CFR1);
		/*stop the carrier frequency search loop */
		ChipSetField(pParams->hDemod, FSTV0900_P2_CFR_AUTOSCAN, 0);

		ChipSetField(pParams->hDemod, FSTV0900_P2_SPECINV_CONTROL,
				FE_SAT_IQ_FORCE_SWAPPED);

		/*stop the demod */
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P2_DMDISTATE,
				0x1C);
		/*init the frequency offset */
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P2_CFRINIT1,
				freq1);
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P2_CFRINIT0,
				freq0);
		/*trig a warm start */
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P2_DMDISTATE,
				0x18);

		if (FE_STV0900_WaitForLock(pParams->hDemod, Demod,
					demodTimeout, fecTimeout) == TRUE) {
			/*wait until Lock or timeout */
			pParams->Demod2Results.Locked = TRUE;
			/* Read signal caracteristics */
			signalType = FE_STV0900_GetSignalParams(pParams, Demod);
			/* Optimization setting for tracking */
			FE_STV0900_TrackingOptimization(pParams, Demod);
			return signalType;
		}
		ChipSetField(pParams->hDemod, FSTV0900_P2_SPECINV_CONTROL,
				FE_SAT_IQ_FORCE_NORMAL);
		/*stop the demod */
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P2_DMDISTATE,
				0x1C);
		/*init the frequency offset */
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P2_CFRINIT1,
				freq1);
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P2_CFRINIT0,
				freq0);
		/*trig a warm start */
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P2_DMDISTATE,
				0x18);

		if (FE_STV0900_WaitForLock(pParams->hDemod, Demod,
					demodTimeout, fecTimeout) == TRUE) {
			/*wait until Lock or timeout */
			pParams->Demod2Results.Locked = TRUE;
			/* Read signal caracteristics */
			signalType = FE_STV0900_GetSignalParams(pParams, Demod);
			/* Optimization setting for tracking */
			FE_STV0900_TrackingOptimization(pParams, Demod);
		}
		break;
	}

	return signalType;

}

void FE_STV0900_Setdvbs2RollOff(STCHIP_Handle_t hChip, FE_STV0900_DEMOD_t Demod)
{
	S32 rolloff;
	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		if (hChip->ChipID == 0x10) {
			/*For cut 1.1 and above the setting of the rolloff in
			 * DVBS2 is automatic in both path 1 and 2 */
			/* for cut 1.0 the setting of the rolloff should be
			 * manual from the MATSTR field of the BB Header */
			/* the setting of the rolloff in DVBS2 , when manual,
			 * should be after FEC lock to be sure that */
			/* at least one BB frame was decoded */
			/*Set the rolloff manually from the BB header value */
			/*Read the MATSTR reg */
			rolloff = ChipGetOneRegister(hChip,
					RSTV0900_P1_MATSTR1) & 0x03;

			ChipSetFieldImage(hChip, FSTV0900_P1_MANUALSX_ROLLOFF,
					  1);
			ChipSetFieldImage(hChip, FSTV0900_P1_ROLLOFF_CONTROL,
					  rolloff);
			ChipSetRegisters(hChip, RSTV0900_P1_DEMOD, 1);
		} else if (hChip->ChipID <= 0x20) {
			/*For cut 1.1 to 2.0set the rolloff to auto mode if
			 * DVBS2 found */
			ChipSetFieldImage(hChip, FSTV0900_P1_MANUALSX_ROLLOFF,
					  0);
			ChipSetRegisters(hChip, RSTV0900_P1_DEMOD, 1);
		} else {
			/*For cut 3.0 set the DVBS2 rolloff to auto mode if
			 * DVBS2 found */
			ChipSetFieldImage(hChip, FSTV0900_P1_MANUALS2_ROLLOFF,
					  0);
			ChipSetRegisters(hChip, RSTV0900_P1_DEMOD, 1);
		}

		break;

	case FE_SAT_DEMOD_2:
		if (hChip->ChipID == 0x10) {
			/*For cut 1.1 and above the setting of the rolloff in
			 * DVBS2 is automatic in both path 1 and 2 */
			/* for cut 1.0 the setting of the rolloff should be
			 * manual from the MATSTR field of the BB Header */
			/* the setting of the rolloff in DVBS2 , when manual,
			 * should be after FEC lock to be sure that */
			/* at least one BB frame was decoded */
			/*Set the rolloff manually from the BB header value */
			/*Read the MATSTR reg */
			rolloff = ChipGetOneRegister(hChip,
					RSTV0900_P2_MATSTR1) & 0x03;

			ChipSetFieldImage(hChip, FSTV0900_P2_MANUALSX_ROLLOFF,
					  1);
			ChipSetFieldImage(hChip, FSTV0900_P2_ROLLOFF_CONTROL,
					  rolloff);
			ChipSetRegisters(hChip, RSTV0900_P2_DEMOD, 1);
		} else if (hChip->ChipID <= 0x20) {
			/*For cut 1.1 to 2.0set the rolloff to auto mode if
			 * DVBS2 found */
			ChipSetFieldImage(hChip, FSTV0900_P2_MANUALSX_ROLLOFF,
					  0);
			ChipSetRegisters(hChip, RSTV0900_P2_DEMOD, 1);
		} else {
			/*For cut 3.0 set the DVBS2 rolloff to auto mode if
			 * DVBS2 found */
			ChipSetFieldImage(hChip, FSTV0900_P2_MANUALS2_ROLLOFF,
					  0);
			ChipSetRegisters(hChip, RSTV0900_P2_DEMOD, 1);
		}
		break;
	}
}

BOOL FE_STV0900_CheckSignalPresence(FE_STV0900_InternalParams_t *pParams,
				    FE_STV0900_DEMOD_t Demod)
{
	S32 carrierOffset, agc2Integrateur, maxCarrier;

	BOOL noSignal;

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		carrierOffset =
		    (ChipGetOneRegister(pParams->hDemod, RSTV0900_P1_CFR2) << 8)
		    | ChipGetOneRegister(pParams->hDemod, RSTV0900_P1_CFR1);
		carrierOffset = Get2Comp(carrierOffset, 16);
		agc2Integrateur =
		    (ChipGetOneRegister(pParams->hDemod, RSTV0900_P1_AGC2I1) <<
		     8) | ChipGetOneRegister(pParams->hDemod,
					     RSTV0900_P1_AGC2I0);
		/*convert to KHz */
		maxCarrier = pParams->Demod1SearchRange / 1000;
		break;

	case FE_SAT_DEMOD_2:
		carrierOffset =
		    (ChipGetOneRegister(pParams->hDemod, RSTV0900_P2_CFR2) << 8)
		    | ChipGetOneRegister(pParams->hDemod, RSTV0900_P2_CFR1);
		carrierOffset = Get2Comp(carrierOffset, 16);
		agc2Integrateur =
		    (ChipGetOneRegister(pParams->hDemod, RSTV0900_P2_AGC2I1) <<
		     8) | ChipGetOneRegister(pParams->hDemod,
					     RSTV0900_P2_AGC2I0);
		maxCarrier = pParams->Demod2SearchRange / 1000;
		break;

	}

	/* add 10% of margin */
	maxCarrier += (maxCarrier / 10);
	/*Convert to reg value */
	maxCarrier = 65536 * (maxCarrier / 2);
	maxCarrier /= pParams->MasterClock / 1000;
	/*Securite anti-bug: */
	if (maxCarrier > 0x4000)
		/*maxcarrier should be<= +-1/4 Mclk */
		maxCarrier = 0x4000;

	/*if the AGC2 integrateur > 0x2000: No signal at this position
	   Elsez, if the AGC2 is <=0x2000 and the |carrier offset| is
	   >=maxcarrier then the demodulator is checking an adjacent channel */
	if ((agc2Integrateur > 0x2000) || (carrierOffset > 2 * maxCarrier) ||
			(carrierOffset < -2 * maxCarrier))
		noSignal = TRUE;
	else
		noSignal = FALSE;

	return noSignal;
}

BOOL FE_STV0900_SearchCarrierSwLoop(FE_STV0900_InternalParams_t *pParams,
				    S32 FreqIncr,
				    S32 Timeout,
				    BOOL Zigzag,
				    S32 MaxStep, FE_STV0900_DEMOD_t Demod)
{
	BOOL noSignal, lock = FALSE;

	S32 stepCpt, freqOffset, maxCarrier;

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		maxCarrier = pParams->Demod1SearchRange / 1000;
		maxCarrier += (maxCarrier / 10);

		break;

	case FE_SAT_DEMOD_2:
		maxCarrier = pParams->Demod2SearchRange / 1000;
		maxCarrier += (maxCarrier / 10);

		break;
	}

	/*Convert to reg value */
	maxCarrier = 65536 * (maxCarrier / 2);
	maxCarrier /= pParams->MasterClock / 1000;

	if (maxCarrier > 0x4000)
		/*maxcarrier should be<= +-1/4 Mclk */
		maxCarrier = 0x4000;

	/*Initialization */
	if (Zigzag == TRUE)
		freqOffset = 0;
	else
		freqOffset = -maxCarrier + FreqIncr;

	stepCpt = 0;
	do {
		switch (Demod) {
		case FE_SAT_DEMOD_1:
		default:
			/*Stop Demod */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P1_DMDISTATE, 0x1C);
			/*init carrier freq */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P1_CFRINIT1,
					(freqOffset / 256) & 0xFF);
			ChipSetOneRegister(pParams->hDemod,
					   RSTV0900_P1_CFRINIT0,
					   freqOffset & 0xFF);
			/*Trig an acquisition (start the search) */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P1_DMDISTATE, 0x18);
			/*DVBS2 only: stop the Packet Delineator: */
			/*stop the DVBS2 packet delin */
			ChipSetField(pParams->hDemod, FSTV0900_P1_ALGOSWRST, 1);
			/*Reset stream Merger: for cut 1.2 */
			if (pParams->hDemod->ChipID == 0x12) {
				/* Stream merger reset */
				ChipSetField(pParams->hDemod,
						FSTV0900_P1_RST_HWARE, 1);
				/* Release stream merger reset */
				ChipSetField(pParams->hDemod,
						FSTV0900_P1_RST_HWARE, 0);
			}
			break;

		case FE_SAT_DEMOD_2:
			/*Stop Demod */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P2_DMDISTATE, 0x1C);
			/*init carrier freq */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P2_CFRINIT1,
					(freqOffset / 256) & 0xFF);
			ChipSetOneRegister(pParams->hDemod,
					   RSTV0900_P2_CFRINIT0,
					   freqOffset & 0xFF);
			/*Trig an acquisition (start the search) */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P2_DMDISTATE, 0x18);
			/*DVBS2 only: stop the Packet Delineator: */
			/*release reset DVBS2 packet delin */
			ChipSetField(pParams->hDemod, FSTV0900_P2_ALGOSWRST, 1);
			/*Reset stream Merger: for cut 1.2 */
			if (pParams->hDemod->ChipID == 0x12) {
				/* Stream merger reset */
				ChipSetField(pParams->hDemod,
						FSTV0900_P2_RST_HWARE, 1);
				/* Release stream merger reset */
				ChipSetField(pParams->hDemod,
						FSTV0900_P2_RST_HWARE, 0);
			}
			break;
		}
		/*Next Step */
		if (Zigzag == TRUE) {
			if (freqOffset >= 0)
				freqOffset = -freqOffset - 2 * FreqIncr;
			else
				freqOffset = -freqOffset;
		} else
			freqOffset += 2 * FreqIncr;
		stepCpt++;

		lock = FE_STV0900_GetDemodLock(pParams->hDemod, Demod, Timeout);
		noSignal = FE_STV0900_CheckSignalPresence(pParams, Demod);

		/*End of the loop if:
		   -lock,
		   -carrier offset > maxoffset,
		   -step > MaxStep
		 */
	} while ((lock == FALSE) && (noSignal == FALSE)
		&& ((freqOffset - FreqIncr) < maxCarrier)
		&& ((freqOffset + FreqIncr) > -maxCarrier)
		&& (stepCpt < MaxStep));
	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		/*DVBS2 only: release the Packet Delineator: */
		/*release reset DVBS2 packet delin */
		ChipSetField(pParams->hDemod, FSTV0900_P1_ALGOSWRST, 0);
		break;

	case FE_SAT_DEMOD_2:
		/*DVBS2 only: release the Packet Delineator: */
		/*release reset DVBS2 packet delin */
		ChipSetField(pParams->hDemod, FSTV0900_P2_ALGOSWRST, 0);
		break;
	}

	return lock;
}

void FE_STV0900_GetSwLoopParams(FE_STV0900_InternalParams_t *pParams,
				S32 *FreqIncrement,
				S32 *SwTimeout,
				S32 *MaxSteps, FE_STV0900_DEMOD_t Demod)
{
	S32 timeOut, freqInrement, maxSteps, symbolRate, maxCarrier;

	FE_STV0900_SearchStandard_t Standard;

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		symbolRate = pParams->Demod1SymbolRate;
		maxCarrier = pParams->Demod1SearchRange / 1000;
		maxCarrier += maxCarrier / 10;
		Standard = pParams->Demod1SearchStandard;
		break;

	case FE_SAT_DEMOD_2:
		symbolRate = pParams->Demod2SymbolRate;
		maxCarrier = pParams->Demod2SearchRange / 1000;
		maxCarrier += maxCarrier / 10;
		Standard = pParams->Demod2SearchStandard;
		break;
	}

	/*Convert to reg value */
	maxCarrier = 65536 * (maxCarrier / 2);
	maxCarrier /= pParams->MasterClock / 1000;

	if (maxCarrier > 0x4000)
		/*maxcarrier should be<= +-1/4 Mclk */
		maxCarrier = 0x4000;

	freqInrement = symbolRate;
	freqInrement /= pParams->MasterClock >> 10;
	freqInrement = freqInrement << 6;

	switch (Standard) {
	case FE_SAT_SEARCH_DVBS1:
	case FE_SAT_SEARCH_DSS:
		/*frequ step = 3% of symbol rate */
		freqInrement *= 3;
		timeOut = 20;
		break;

	case FE_SAT_SEARCH_DVBS2:
		/*frequ step = 4% of symbol rate */
		freqInrement *= 4;
		timeOut = 25;
		break;

	case FE_SAT_AUTO_SEARCH:
	default:
		/*frequ step = 3% of symbol rate */
		freqInrement *= 3;
		timeOut = 25;
		break;
	}
	/*Increment = n% of SR, n =3 for DVBS1 or auto, n= 4 for DVBS2 */
	freqInrement /= 100;

	if ((freqInrement > maxCarrier) || (freqInrement < 0))
		/*Freq Increment should be <= 1/8 Mclk */
		freqInrement = maxCarrier / 2;

	/* cumpute the timout according to the SR with 27.5Msps as reference */
	timeOut *= 27500;
	if (symbolRate > 0)
		timeOut /= symbolRate / 1000;

	if ((timeOut > 100) || (timeOut < 0))
		/*timeout<= 100ms */
		timeOut = 100;

	/*minimum step = 3 */
	maxSteps = (maxCarrier / freqInrement) + 1;
	if ((maxSteps > 100) || (maxSteps < 0)) {
		/*maxstep <= 100 */
		maxSteps = 100;
		/*if max step > 100 then max step = 100 and the increment is
		 * radjasted to cover the total range */
		freqInrement = maxCarrier / maxSteps;
	}

	*FreqIncrement = freqInrement;
	*SwTimeout = timeOut;
	*MaxSteps = maxSteps;

}

BOOL FE_STV0900_SWAlgo(FE_STV0900_InternalParams_t *pParams,
		       FE_STV0900_DEMOD_t Demod)
{
	BOOL lock = FALSE;

	BOOL noSignal, zigzag;
	S32 dvbs2_fly_wheel;

	S32 freqIncrement, softStepTimeout, trialCounter, maxSteps;

	/*Get the SW algo params, Increment, timeout and max steps */
	FE_STV0900_GetSwLoopParams(pParams, &freqIncrement, &softStepTimeout,
				   &maxSteps, Demod);
	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:

		switch (pParams->Demod1SearchStandard) {
		case FE_SAT_SEARCH_DVBS1:
		case FE_SAT_SEARCH_DSS:
			/*accelerate the frequency detector */
			if (pParams->hDemod->ChipID >= 0x20)
				/* value for cut 2.0 above */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P1_CARFREQ, 0x3B);
			else
				/* value for cut 1.2 below */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P1_CARFREQ, 0xef);
			/*stop DVBS2 search, stop freq auto scan, stop SR auto
			 * scan */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P1_DMDCFGMD, 0x49);
			zigzag = FALSE;
			break;

		case FE_SAT_SEARCH_DVBS2:
			if (pParams->hDemod->ChipID >= 0x20)
				/*value for the SW search cut 2.0 */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P1_CORRELABS, 0x79);
			else
				/*value for the SW search cut 1.x */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P1_CORRELABS, 0x68);
			/*stop DVBS1 search, stop freq auto scan, stop SR auto
			 * scan */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P1_DMDCFGMD, 0x89);
			zigzag = TRUE;
			break;

		case FE_SAT_AUTO_SEARCH:
		default:
			/*accelerate the frequency detector */
			if (pParams->hDemod->ChipID >= 0x20) {
				/* value for cut 2.0 above */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P1_CARFREQ, 0x3B);
				/*value for the SW search cut 2.0 */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P1_CORRELABS, 0x79);
			} else {
				/* value for cut 1.x below */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P1_CARFREQ, 0xef);
				/*value for the SW search cut 1.x */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P1_CORRELABS, 0x68);
			}

			/*enable DVBS2 and DVBS1 search, stop freq auto scan,
			 * stop SR auto scan */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P1_DMDCFGMD, 0xc9);
			zigzag = FALSE;
			break;
		}

		trialCounter = 0;
		do {
			lock =
			    FE_STV0900_SearchCarrierSwLoop(pParams,
							   freqIncrement,
							   softStepTimeout,
							   zigzag, maxSteps,
							   Demod);
			noSignal =
			    FE_STV0900_CheckSignalPresence(pParams, Demod);
			trialCounter++;
			/*run the SW search 2 times maximum */

			if ((lock != TRUE) && (noSignal != TRUE) &&
					(trialCounter != 2))
				continue;
			/*remove the SW algo values */
			/*Check if demod is not losing lock in DVBS2 */
			if (pParams->hDemod->ChipID >= 0x20) {
				/* value for cut 2.0 above */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P1_CARFREQ, 0x49);
				/* value for cut 2.0 above */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P1_CORRELABS, 0x9e);
			} else {
				/* value for cut 1.x below */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P1_CARFREQ, 0xed);
				/* value for cut 1.x above */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P1_CORRELABS, 0x88);
			}

			if ((lock != TRUE) ||
			     (ChipGetField(pParams->hDemod,
			       FSTV0900_P1_HEADER_MODE) != FE_SAT_DVBS2_FOUND))
				continue;

			/*Check if the demod is not losing lock in DVBS2 */
			ChipWaitOrAbort(pParams->hDemod, softStepTimeout);
			/*read the number of correct frames */
			dvbs2_fly_wheel = ChipGetField(pParams->hDemod,
					FSTV0900_P1_FLYWHEEL_CPT);
			if (dvbs2_fly_wheel < 0xd) {
				/*if correct frames is decrementing */
				/*wait */
				ChipWaitOrAbort(pParams->hDemod,
						softStepTimeout);
				/*read the number of correct frames */
				dvbs2_fly_wheel = ChipGetField(pParams->hDemod,
						FSTV0900_P1_FLYWHEEL_CPT);
			}
			if (dvbs2_fly_wheel >= 0xd)
				continue;

			/*FALSE lock, The demod is lossing lock */
			lock = FALSE;
			if (trialCounter >= 2)
				continue;

			if (pParams->hDemod->ChipID >= 0x20)
				/*value for the SW search cut 2.0 */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P1_CORRELABS, 0x79);
			else
				/*value for the SW search cut 1.x */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P1_CORRELABS, 0x68);

			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P1_DMDCFGMD, 0x89);
		/*end of trial if lock or noSignal or more than 2 trial */
		} while ((lock == FALSE)
		       && (trialCounter < 2)
		       && (noSignal == FALSE));
		break;

	case FE_SAT_DEMOD_2:

		switch (pParams->Demod2SearchStandard) {
		case FE_SAT_SEARCH_DVBS1:
		case FE_SAT_SEARCH_DSS:
			/*accelerate the frequency detector */
			if (pParams->hDemod->ChipID >= 0x20)
				/* value for cut 2.0 above */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P2_CARFREQ, 0x3B);
			else
				/* value for cut 1.2 below */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P2_CARFREQ, 0xef);

			/*stop DVBS2 search, stop freq auto scan, stop SR auto
			 * scan */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P2_DMDCFGMD, 0x49);
			zigzag = FALSE;
			break;

		case FE_SAT_SEARCH_DVBS2:
			if (pParams->hDemod->ChipID >= 0x20)
				/*value for the SW search cut 2.0 */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P2_CORRELABS, 0x79);
			else
				/*value for the SW search cut 1.x */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P2_CORRELABS, 0x68);
			/*stop DVBS1 search, stop freq auto scan, stop SR auto
			 * scan */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P2_DMDCFGMD, 0x89);
			zigzag = TRUE;
			break;

		case FE_SAT_AUTO_SEARCH:
		default:
			/*accelerate the frequency detector */
			if (pParams->hDemod->ChipID >= 0x20) {
				/* value for cut 2.0 above */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P2_CARFREQ, 0x3B);
				/*value for the SW search cut 2.0 */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P2_CORRELABS, 0x79);
			} else {
				/* value for cut 1.x below */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P2_CARFREQ, 0xef);
				/*value for the SW search cut 1.x */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P2_CORRELABS, 0x68);
			}

			/*enable DVBS2 and DVBS1 search, stop freq auto scan,
			 * stop SR auto scan */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P2_DMDCFGMD, 0xc9);
			zigzag = FALSE;
			break;
		}

		trialCounter = 0;
		do {
			lock =
			    FE_STV0900_SearchCarrierSwLoop(pParams,
							   freqIncrement,
							   softStepTimeout,
							   zigzag, maxSteps,
							   Demod);
			noSignal =
			    FE_STV0900_CheckSignalPresence(pParams, Demod);
			trialCounter++;
			/*run the SW search 2 times maximum */

			if ((lock != TRUE) && (noSignal != TRUE) &&
					(trialCounter != 2))
				continue;
			/*remove the SW algo values */
			/*Check if the demod is not losing lock in DVBS2 */
			if (pParams->hDemod->ChipID >= 0x20) {
				/* value for cut 2.0 above */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P2_CARFREQ, 0x49);
				/* value for cut 2.0 above */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P2_CORRELABS, 0x9e);
			} else {
				/* value for cut 1.x below */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P2_CARFREQ, 0xed);
				/* value for cut 1.x above */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P2_CORRELABS, 0x88);
			}

			if ((lock != TRUE) ||
			     (ChipGetField(pParams->hDemod,
			      FSTV0900_P2_HEADER_MODE) != FE_SAT_DVBS2_FOUND))
				continue;

			/*Check if the demod is not losing lock in DVBS2 */
			ChipWaitOrAbort(pParams->hDemod, softStepTimeout);
			/*read the number of correct frames */
			dvbs2_fly_wheel = ChipGetField(pParams->hDemod,
					FSTV0900_P2_FLYWHEEL_CPT);
			if (dvbs2_fly_wheel < 0xd) {
				/*if correct frames is decrementing */
				/*wait */
				ChipWaitOrAbort(pParams->hDemod,
						softStepTimeout);
				/*read the number of correct frames */
				dvbs2_fly_wheel = ChipGetField(pParams->hDemod,
						FSTV0900_P2_FLYWHEEL_CPT);
			}
			if (dvbs2_fly_wheel >= 0xd)
				continue;

			/*FALSE lock, The demod is lossing lock */
			lock = FALSE;
			if (trialCounter >= 2)
				continue;

			if (pParams->hDemod->ChipID >= 0x20)
				/*value for the SW
				 * search cut 2.0 */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P2_CORRELABS, 0x79);
			else
				/*value for the SW
				 * search cut 1.x */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P2_CORRELABS, 0x68);

			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P2_DMDCFGMD, 0x89);
		/*end of trial if lock or noSignal or more than 2 trial */
		} while ((lock == FALSE)
		       && (trialCounter < 2)
		       && (noSignal == FALSE));
		break;
	}

	return lock;

}

BOOL FE_STV0900_GetDemodLockCold(FE_STV0900_InternalParams_t *pParams,
				 S32 DemodTimeout, FE_STV0900_DEMOD_t Demod)
{

	BOOL lock = FALSE;

	S32 symbolRate,
	    searchRange,
	    locktimeout,
	    carrierStep,
	    nbSteps, currentStep, direction, tunerFreq, timeout, stepsg, freq;

#ifdef HOST_PC
#ifndef NO_GUI
	char message[100];
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

	if (symbolRate >= 10000000)
		locktimeout = DemodTimeout / 3;
	else
		locktimeout = DemodTimeout / 2;

	/* case cold start wait for demod lock */
	lock = FE_STV0900_GetDemodLock(pParams->hDemod, Demod, locktimeout);

	if (lock != FALSE)
		return lock;
	if (symbolRate >= 10000000) {
		/*if Symbol rate >= 10Msps check for timing lock */
		if (!FE_STV0900_CheckTimingLock(pParams->hDemod, Demod))
			return FALSE;

		switch (Demod) {
		case FE_SAT_DEMOD_1:
		default:
			/*Reset the Demod */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P1_DMDISTATE, 0x1F);
			/*Trig an acquisition (start the search) */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P1_DMDISTATE, 0x15);
			break;

		case FE_SAT_DEMOD_2:
			/*Reset the Demod */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P2_DMDISTATE, 0x1F);
			/*Trig an acquisition (start the search) */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P2_DMDISTATE, 0x15);
			break;
		}
		lock = FE_STV0900_GetDemodLock(pParams->hDemod, Demod,
				DemodTimeout);
		return lock;
	}
	/*Symbol Rate <10Msps do not check timing lock */

	/* cut 2.0 and 1.x perofrms a zigzag on the tuner with AEP 0x15 */
	if (pParams->hDemod->ChipID <= 0x20) {
		if (symbolRate <= 1000000)
			carrierStep = 500;
		else if (symbolRate <= 4000000)
			carrierStep = 1000;
		else if (symbolRate <= 7000000)
			carrierStep = 2000;
		else if (symbolRate <= 10000000)
			carrierStep = 3000;
		/*else
			carrierStep = 5000; */

		if (symbolRate >= 2000000) {
			timeout = (DemodTimeout / 3);
			if (timeout > 1000)
				/*timeout max for zigzag = 1sec */
				timeout = 1000;
		} else
			timeout = DemodTimeout / 2;
	} else {
		/*cut 3.0 */
		/* cut 3.0  perofrms zigzag on CFRINIT with the new AEP 0x05 */
		carrierStep = symbolRate / 4000;

		/*
		   if (symbolRate >=2000000) {
		   timeout=(DemodTimeout/3);
		   if (timeout>1000)
		   timeout=1000;
		   } else
		   timeout=(DemodTimeout*3)/4;
		 */
		timeout = (DemodTimeout * 3) / 4;
	}

	nbSteps = ((searchRange / 1000) / carrierStep);

	if ((nbSteps % 2) != 0)
		nbSteps += 1;

	if (nbSteps <= 0)
		nbSteps = 2;
	else if (nbSteps > 12)
		nbSteps = 12;

	currentStep = 1;
	direction = 1;

	stepsg = 0;
	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		if (pParams->hDemod->ChipID <= 0x20) {
			/*zigzag the tuner freq */
			tunerFreq = pParams->Tuner1Frequency;
			pParams->Tuner1BW =
			      FE_STV0900_CarrierWidth(pParams->Demod1SymbolRate,
			      pParams->RollOff) + pParams->Demod1SymbolRate;

		} else
			/*zigzag demod CFRINIT freq */
			tunerFreq = 0;

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

			if (pParams->hDemod->ChipID <= 0x20) {
				/* cut 3.0 zigzag on Tuner freq */
				FE_STV0900_SetTuner(pParams->hDemod,
						pParams->hTuner1,
						pParams->Tuner1Type,
						Demod, tunerFreq,
						pParams->Tuner1BW);

				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P1_DMDISTATE, 0x1C);

				ChipSetFieldImage(pParams->hDemod,
						FSTV0900_P1_CFR_INIT1, 0);
				ChipSetFieldImage(pParams->hDemod,
						FSTV0900_P1_CFR_INIT0, 0);
				/*init the demod frequency offset to 0 */
				ChipSetRegisters(pParams->hDemod,
						RSTV0900_P1_CFRINIT1, 2);

				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P1_DMDISTATE, 0x1F);
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P1_DMDISTATE, 0x15);

			} else {
				/* cut 3.0 zigzag on CFRINIT */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P1_DMDISTATE, 0x1C);

				/* Formulat :
				   CFRINIT_Khz = CFRINIT_Reg * MasterClock_Khz
				   /2^16
				 */

				freq = (tunerFreq * 65536) / (pParams->
						MasterClock / 1000);
				ChipSetFieldImage(pParams->hDemod,
					FSTV0900_P1_CFR_INIT1, MSB(freq));
				ChipSetFieldImage(pParams->hDemod,
					FSTV0900_P1_CFR_INIT0, LSB(freq));
				/*init the demod frequency offset to 0 */
				ChipSetRegisters(pParams->hDemod,
						RSTV0900_P1_CFRINIT1, 2);

				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P1_DMDISTATE, 0x1F);
				/* use AEP 0x05 instead of 0x15 to not reset
				 * CFRINIT to 0 */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P1_DMDISTATE, 0x05);

			}
			lock = FE_STV0900_GetDemodLock(pParams->hDemod, Demod,
					timeout);

			direction *= -1;
			currentStep++;
		}

		break;

	case FE_SAT_DEMOD_2:
		if (lock != FALSE)
			return lock;
		if (pParams->hDemod->ChipID <= 0x20) {
			/*zigzag the tuner freq */
			tunerFreq = pParams->Tuner2Frequency;
			pParams->Tuner2BW =
			      FE_STV0900_CarrierWidth(pParams->Demod2SymbolRate,
			      pParams->RollOff) + pParams->Demod2SymbolRate;

		} else
			/*zigzag demod CFRINIT freq */
			tunerFreq = 0;

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

			if (pParams->hDemod->ChipID <= 0x20) {
				/* cut 3.0 zigzag on Tuner freq */
				FE_STV0900_SetTuner(pParams->hDemod,
						pParams->hTuner2,
						pParams->Tuner2Type,
						Demod, tunerFreq,
						pParams->Tuner2BW);

				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P2_DMDISTATE, 0x1C);

				ChipSetFieldImage(pParams->hDemod,
						FSTV0900_P2_CFR_INIT1, 0);
				ChipSetFieldImage(pParams->hDemod,
						FSTV0900_P2_CFR_INIT0, 0);
				/*init the demod frequency offset to 0 */
				ChipSetRegisters(pParams->hDemod,
						RSTV0900_P2_CFRINIT1, 2);

				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P2_DMDISTATE, 0x1F);
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P2_DMDISTATE, 0x15);

			} else {
				/* cut 3.0 zigzag on CFRINIT */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P2_DMDISTATE, 0x1C);

				/*      Formulat :
				   CFRINIT_Khz = CFRINIT_Reg * MasterClock_Khz
				   /2^16
				 */

				freq = (tunerFreq * 65536) / (pParams->
						MasterClock / 1000);
				ChipSetFieldImage(pParams->hDemod,
					FSTV0900_P2_CFR_INIT1, MSB(freq));
				ChipSetFieldImage(pParams->hDemod,
					FSTV0900_P2_CFR_INIT0, LSB(freq));
				/*init the demod frequency offset to 0 */
				ChipSetRegisters(pParams->hDemod,
						RSTV0900_P2_CFRINIT1, 2);

				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P2_DMDISTATE, 0x1F);
				/* use AEP 0x05 instead of 0x15 to not reset
				 * CFRINIT to 0 */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P2_DMDISTATE, 0x05);

			}
			lock = FE_STV0900_GetDemodLock(pParams->hDemod, Demod,
					timeout);

			direction *= -1;
			currentStep++;
		}

		break;
	}

	return lock;

}

U32 FE_STV0900_SearchSRCoarse(FE_STV0900_InternalParams_t *pParams,
			      FE_STV0900_DEMOD_t Demod)
{
	BOOL timingLock = FALSE;
	S32 i,
	    timingcpt = 0, direction = 1, nbSteps, currentStep = 0, tunerFreq;

	U32 coarseSymbolRate = 0, agc2Integrateur = 0, carrierStep = 1200;

	U32 agc2Th;

#ifdef HOST_PC
#ifndef NO_GUI
	char message[100];
#endif
#endif

	if (pParams->hDemod->ChipID >= 0x30)
		agc2Th = 0x2E00;
	else
		agc2Th = 0x1F00;

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		/* In case of Path 1 */
		/*Reset the Demod */
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P1_DMDISTATE,
				0x5F);

		ChipSetOneRegister(pParams->hDemod, RSTV0900_P1_TMGCFG, 0x12);
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P1_TMGCFG2, 0xC0);

		/*timing lock TH high setting for blind search */
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P1_TMGTHRISE,
				0xf0);
		/*timing lock TH Low setting for blind search */
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P1_TMGTHFALL,
				0xe0);

		/*activate the carrier frequency search loop */
		ChipSetFieldImage(pParams->hDemod, FSTV0900_P1_CFR_AUTOSCAN, 0);
		/*Enable the SR SCAN */
		ChipSetFieldImage(pParams->hDemod, FSTV0900_P1_SCAN_ENABLE, 1);
		ChipSetRegisters(pParams->hDemod, RSTV0900_P1_DMDCFGMD, 1);

		/* set the SR max to 65Msps Write the MSB */
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P1_SFRUP1, 0x83);
		/* set the SR max to 65Msps Write the LSB */
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P1_SFRUP0, 0xc0);

		/* set the SR min to 400 Ksps Write the MSB */
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P1_SFRLOW1, 0x82);
		/* set the SR min to 400 Ksps Write the LSB */
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P1_SFRLOW0, 0xa0);

		/*force the acquisition to stop at coarse carrier state */
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P1_DMDT0M, 0x00);
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P1_AGC2REF,
				   /*0x50 */ 0x38);

		if (pParams->hDemod->ChipID >= 0x30) {
			/*Frequency offset detector setting *//* Blind optim*/
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P1_CARFREQ, /*0x99 */ 0x00);
			/*set the SR scan step */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P1_SFRSTEP, 0x98);

		}

		else if (pParams->hDemod->ChipID >= 0x20) {
			/*Frequency offset detector setting */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P1_CARFREQ, 0x6a);
			/*set the SR scan step */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P1_SFRSTEP, 0x95);
		} else {
			/*Frequency offset detector setting */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P1_CARFREQ, 0xed);
			/*set the SR scan step */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P1_SFRSTEP, 0x73);
		}

		if (pParams->Demod1SymbolRate <= 2000000)
			carrierStep = 1000;
		else if (pParams->Demod1SymbolRate <= 5000000)
			carrierStep = 2000;
		else if (pParams->Demod1SymbolRate <= 12000000)
			carrierStep = 3000;
		else
			carrierStep = 5000;

		nbSteps =
		    -1 + ((pParams->Demod1SearchRange / 1000) / carrierStep);
		nbSteps /= 2;
		nbSteps = (2 * nbSteps) + 1;
		if (nbSteps < 0)
			nbSteps = 1;

		else if (nbSteps > 10) {
			nbSteps = 11;
			carrierStep = (pParams->Demod1SearchRange / 1000) / 10;
		}

		currentStep = 0;
		direction = 1;
		tunerFreq = pParams->Tuner1Frequency;
		while ((timingLock == FALSE) && (currentStep < nbSteps)) {
			/*Reset the Demod */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P1_DMDISTATE, 0x5F);

			/* Write the MSB */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P1_CFRINIT1, 0);
			/* Write the LSB */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P1_CFRINIT0, 0);

			/* Write the MSB */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P1_SFRINIT1, 0);
			/* Write the LSB */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P1_SFRINIT0, 0);

			/*Trig an acquisition (start the search) */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P1_DMDISTATE, 0x40);

			/*wait for coarse carrier lock */
			WAIT_N_MS(50);

			for (i = 0; i < 10; i++) {
				if (ChipGetField(pParams->hDemod,
					FSTV0900_P1_TMGLOCK_QUALITY) >= 2)
					/*read the timing lock quality */
					timingcpt++;

				ChipGetRegisters(pParams->hDemod,
						 RSTV0900_P1_AGC2I1, 2);
				agc2Integrateur +=
				    (ChipGetFieldImage(pParams->hDemod,
				      FSTV0900_P1_AGC2_INTEGRATOR1) << 8) |
				    ChipGetFieldImage(pParams->hDemod,
						  FSTV0900_P1_AGC2_INTEGRATOR0);

			}
			agc2Integrateur /= 10;
			coarseSymbolRate =
			    FE_STV0900_GetSymbolRate(pParams->hDemod,
						     pParams->MasterClock,
						     Demod);

			currentStep++;
			direction *= -1;

			if ((timingcpt >= 5) && (agc2Integrateur < agc2Th)
			    && (coarseSymbolRate < 50000000)
			    && (coarseSymbolRate > 850000))
				timingLock = TRUE;

			else if (currentStep < nbSteps) {
				/*Set the tuner frequency and bandwidth */
				/*if timing not locked and currentstep <
				 * maxstep move the tuner freq */
				if (direction > 0)
					tunerFreq +=
					    (currentStep * carrierStep);
				else
					tunerFreq -=
					    (currentStep * carrierStep);
				FE_STV0900_SetTuner(pParams->hDemod,
						    pParams->hTuner1,
						    pParams->Tuner1Type, Demod,
						    tunerFreq,
						    pParams->Tuner1BW);

			}
		}

		if (timingLock == FALSE)
			coarseSymbolRate = 0;
		else
			coarseSymbolRate =
			    FE_STV0900_GetSymbolRate(pParams->hDemod,
						     pParams->MasterClock,
						     Demod);

		break;

	case FE_SAT_DEMOD_2:
		/* In case of Path 2 */
		/*Reset the Demod */
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P2_DMDISTATE,
				0x5F);

		ChipSetOneRegister(pParams->hDemod, RSTV0900_P2_TMGCFG, 0x12);
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P2_TMGCFG2, 0xC0);

		/*timing lock TH high setting for blind search */
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P2_TMGTHRISE,
				0xf0);
		/*timing lock TH Low setting for blind search */
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P2_TMGTHFALL,
				0xe0);

		/*activate the carrier frequency search loop */
		ChipSetFieldImage(pParams->hDemod, FSTV0900_P2_CFR_AUTOSCAN, 0);
		/*Enable the SR SCAN */
		ChipSetFieldImage(pParams->hDemod, FSTV0900_P2_SCAN_ENABLE, 1);
		ChipSetRegisters(pParams->hDemod, RSTV0900_P2_DMDCFGMD, 1);

		/* set the SR max to 65Msps Write the MSB */
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P2_SFRUP1, 0x83);
		/* set the SR max to 65Msps Write the LSB */
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P2_SFRUP0, 0xc0);

		/* set the SR min to 400 Ksps Write the MSB */
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P2_SFRLOW1, 0x82);
		/* set the SR min to 400 Ksps Write the LSB */
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P2_SFRLOW0, 0xa0);

		/*force the acquisition to stop at coarse carrier state */
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P2_DMDT0M, 0x0);
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P2_AGC2REF,
				   /*0x50 */ 0x38);

		if (pParams->hDemod->ChipID >= 0x30) {
			/*Frequency offset detector setting *//* *Blind Optim */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P2_CARFREQ, /*0x99 */ 0x00);
			/*set the SR scan step */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P2_SFRSTEP, 0x98);
		} else if (pParams->hDemod->ChipID >= 0x20) {
			/*Frequency offset detector setting */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P2_CARFREQ, 0x6a);
			/*set the SR scan step */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P2_SFRSTEP, 0x95);
		} else {
			/*Frequency offset detector setting */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P2_CARFREQ, 0xed);
			/*set the SR scan step */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P2_SFRSTEP, 0x73);
		}

		if (pParams->Demod2SymbolRate <= 2000000)
			carrierStep = 1000;
		else if (pParams->Demod2SymbolRate <= 5000000)
			carrierStep = 2000;
		else if (pParams->Demod2SymbolRate <= 12000000)
			carrierStep = 3000;
		else
			carrierStep = 5000;

		nbSteps =
		    -1 + ((pParams->Demod2SearchRange / 1000) / carrierStep);
		nbSteps /= 2;
		nbSteps = (2 * nbSteps) + 1;
		if (nbSteps < 0)
			nbSteps = 1;
		else if (nbSteps > 10) {
			nbSteps = 11;
			carrierStep = (pParams->Demod2SearchRange / 1000) / 10;
		}

		currentStep = 0;
		direction = 1;
		tunerFreq = pParams->Tuner2Frequency;
		while ((timingLock == FALSE) && (currentStep < nbSteps)) {
			/*Reset the Demod */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P2_DMDISTATE, 0x5F);

			/* Write the MSB */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P2_CFRINIT1, 0);
			/* Write the LSB */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P2_CFRINIT0, 0);

			/* Write the MSB */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P2_SFRINIT1, 0);
			/* Write the LSB */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P2_SFRINIT0, 0);

			/*Trig an acquisition (start the search) */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P2_DMDISTATE, 0x40);

			/*wait for coarse carrier lock */
			WAIT_N_MS(50);
			timingcpt = 0;
			for (i = 0; i < 10; i++) {
				if (ChipGetField(pParams->hDemod,
					FSTV0900_P2_TMGLOCK_QUALITY) >= 2)
					/*read the timing lock quality */
					timingcpt++;
				ChipGetRegisters(pParams->hDemod,
						 RSTV0900_P2_AGC2I1, 2);
				agc2Integrateur +=
				    (ChipGetFieldImage(pParams->hDemod,
				      FSTV0900_P2_AGC2_INTEGRATOR1) << 8) |
				    ChipGetFieldImage(pParams->hDemod,
					      FSTV0900_P2_AGC2_INTEGRATOR0);

			}
			agc2Integrateur /= 10;
			coarseSymbolRate =
			    FE_STV0900_GetSymbolRate(pParams->hDemod,
						     pParams->MasterClock,
						     Demod);

			if ((timingcpt >= 5) && (agc2Integrateur < agc2Th)
			    && (coarseSymbolRate < 55000000)
			    && (coarseSymbolRate > 850000))
				timingLock = TRUE;
			else {
				/*Set the tuner frequency and bandwidth */
				/*if timing not locked and currentstep <
				 * maxstep move the tuner freq */
				currentStep++;
				direction *= -1;

				if (direction > 0)
					tunerFreq +=
					    (currentStep * carrierStep);
				else
					tunerFreq -=
					    (currentStep * carrierStep);
				FE_STV0900_SetTuner(pParams->hDemod,
						    pParams->hTuner2,
						    pParams->Tuner2Type, Demod,
						    tunerFreq,
						    pParams->Tuner2BW);

			}
		}

		if (timingLock == FALSE)
			coarseSymbolRate = 0;
		else
			coarseSymbolRate =
			    FE_STV0900_GetSymbolRate(pParams->hDemod,
						     pParams->MasterClock,
						     Demod);

		break;

	}

#ifdef HOST_PC
#ifndef NO_GUI
	if (timingLock == FALSE)
		ReportInsertMessage("Coarse carrier Not Locked");
	else {
		ReportInsertMessage("Coarse carrier Locked");
		Fmt(message, "coarse SR = %d", coarseSymbolRate);
		ReportInsertMessage(message);

	}

#endif
#endif

	return coarseSymbolRate;
}

U32 FE_STV0900_SearchSRFine(FE_STV0900_InternalParams_t *pParams,
			    FE_STV0900_DEMOD_t Demod)
{
	U32 coarseSymbolRate, coarseFreq, symb, symbmax, symbmin, symbcomp;

	coarseSymbolRate =
	    FE_STV0900_GetSymbolRate(pParams->hDemod, pParams->MasterClock,
				     Demod);

	if (coarseSymbolRate > 3000000) {
		/*SFRUP = SFR + 30% *//* * Blind optim */
		symbmax = 14 * (coarseSymbolRate / 10);
		/* Formulat :init_symbol_rate_regs = SR * 2^16/MasterClock */
		symbmax = (symbmax / 1000) * 65536;
		symbmax /= (pParams->MasterClock / 1000);

		/*SFRLOW = SFR - 30% */	/* * Blind optim */
		symbmin = 10 * (coarseSymbolRate / 16);
		/* Formulat :init_symbol_rate_regs = SR * 2^16/MasterClock */
		symbmin = (symbmin / 1000) * 65536;
		symbmin /= (pParams->MasterClock / 1000);

		/* Formulat :init_symbol_rate_regs = SR * 2^16/MasterClock */
		symb = (coarseSymbolRate / 1000) * 65536;
		symb /= (pParams->MasterClock / 1000);
	} else {
		/*SFRUP = SFR + 30% *//* * Blind optim */
		symbmax = 14 * (coarseSymbolRate / 10);
		/* Formulat :init_symbol_rate_regs = SR * 2^16/MasterClock */
		symbmax = (symbmax / 100) * 65536;
		symbmax /= (pParams->MasterClock / 100);

		/*SFRLOW = SFR - 30% *//* * Blind optim */
		symbmin = 10 * (coarseSymbolRate / 16);
		/* Formulat :init_symbol_rate_regs = SR * 2^16/MasterClock */
		symbmin = (symbmin / 100) * 65536;
		symbmin /= (pParams->MasterClock / 100);

		/* Formulat :init_symbol_rate_regs = SR * 2^16/MasterClock */
		symb = (coarseSymbolRate / 100) * 65536;
		symb /= (pParams->MasterClock / 100);
	}

	/*SFRUP = SFR + 30% */
	symbcomp = 14 * (coarseSymbolRate / 10);
	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		coarseFreq =
		    (ChipGetOneRegister(pParams->hDemod, RSTV0900_P1_CFR2) << 8)
		    | ChipGetOneRegister(pParams->hDemod, RSTV0900_P1_CFR1);

		if (symbcomp < pParams->Demod1SymbolRate) {
			/* if coarsSR + 30% < symbol do not continue */
			/*if coarse SR +30% < user given SR ==> end the SR
			 * given by the user in blind search is the minimum
			 * expected SR  */
			coarseSymbolRate = 0;

		} else {
			/* In case of Path 1 */

			/*Reset the Demod */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P1_DMDISTATE, 0x1F);

			/*Slow down the timing loop */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P1_TMGCFG2, 0xC1);
			/*timing stting for warm/cold start */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P1_TMGTHRISE, 0x20);
			/*timing stting for warm/cold start */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P1_TMGTHFALL, 0x00);
			ChipSetOneRegister(pParams->hDemod, RSTV0900_P1_TMGCFG,
					   0xd2);

			ChipSetFieldImage(pParams->hDemod,
					  FSTV0900_P1_CFR_AUTOSCAN, 0);
			ChipSetRegisters(pParams->hDemod, RSTV0900_P1_DMDCFGMD,
					 1);

			ChipSetOneRegister(pParams->hDemod, RSTV0900_P1_AGC2REF,
					   0x38);

			if (pParams->hDemod->ChipID >= 0x30)
				/*Frequency offset detector setting */
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0900_P1_CARFREQ, 0x79);

			else if (pParams->hDemod->ChipID >= 0x20)
				/*Frequency offset detector setting */
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0900_P1_CARFREQ, 0x49);
			else
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0900_P1_CARFREQ, 0xed);

			/* Write the MSB */
			ChipSetOneRegister(pParams->hDemod, RSTV0900_P1_SFRUP1,
					(symbmax >> 8) & 0x7F);
			/* Write the LSB */
			ChipSetOneRegister(pParams->hDemod, RSTV0900_P1_SFRUP0,
					(symbmax & 0xFF));

			/* Write the MSB */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P1_SFRLOW1, (symbmin >> 8) &
					0x7F);
			/* Write the LSB */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P1_SFRLOW0, (symbmin & 0xFF));

			/* Write the MSB */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P1_SFRINIT1, (symb >> 8) &
					0xFF);
			/* Write the LSB */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P1_SFRINIT0, (symb & 0xFF));

			/*activate the carrier frequency search loop */
			ChipSetOneRegister(pParams->hDemod, RSTV0900_P1_DMDT0M,
					0x20);
			ChipSetOneRegister(pParams->hDemod,
					   RSTV0900_P1_CFRINIT1,
					   (coarseFreq >> 8) & 0xff);
			ChipSetOneRegister(pParams->hDemod,
					   RSTV0900_P1_CFRINIT0,
					   coarseFreq & 0xff);
			/*Trig an acquisition (start the search) */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P1_DMDISTATE, 0x15);
		}
		break;

	case FE_SAT_DEMOD_2:
		/* In case of Path 2 */
		coarseFreq =
		    (ChipGetOneRegister(pParams->hDemod, RSTV0900_P2_CFR2) << 8)
		    | ChipGetOneRegister(pParams->hDemod, RSTV0900_P2_CFR1);

		if (symbcomp < pParams->Demod2SymbolRate) {
			/* if coarsSR + 30% < symbol do not continue */
			/*if coarse SR +30% < user given SR ==> end the SR
			 * given by the user in blind search is the minimum
			 * expected SR  */
			coarseSymbolRate = 0;

		} else {

			/*Reset the Demod */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P2_DMDISTATE, 0x1F);

			/*Slow down the timing loop */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P2_TMGCFG2, 0xC1);
			/*timing stting for warm/cold start */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P2_TMGTHRISE, 0x20);
			/*timing stting for warm/cold start */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P2_TMGTHFALL, 0x00);
			ChipSetOneRegister(pParams->hDemod, RSTV0900_P2_TMGCFG,
					   0xd2);

			ChipSetFieldImage(pParams->hDemod,
					  FSTV0900_P2_CFR_AUTOSCAN, 0);
			ChipSetRegisters(pParams->hDemod, RSTV0900_P2_DMDCFGMD,
					 1);

			ChipSetOneRegister(pParams->hDemod, RSTV0900_P2_AGC2REF,
					   0x38);

			if (pParams->hDemod->ChipID >= 0x30)
				/*Frequency offset detector setting */
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0900_P2_CARFREQ, 0x79);

			else if (pParams->hDemod->ChipID >= 0x20)
				/*Frequency offset detector setting */
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0900_P2_CARFREQ, 0x49);
			else
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0900_P2_CARFREQ, 0xed);

			/* Write the MSB */
			ChipSetOneRegister(pParams->hDemod, RSTV0900_P2_SFRUP1,
					(symbmax >> 8) & 0x7F);
			/* Write the LSB */
			ChipSetOneRegister(pParams->hDemod, RSTV0900_P2_SFRUP0,
					(symbmax & 0xFF));

			/* Write the MSB */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P2_SFRLOW1, (symbmin >> 8) &
					0x7F);
			/* Write the LSB */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P2_SFRLOW0, (symbmin & 0xFF));

			/* Write the MSB */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P2_SFRINIT1, (symb >> 8) &
					0xFF);
			/* Write the LSB */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P2_SFRINIT0, (symb & 0xFF));

			/*activate the carrier frequency search loop */
			ChipSetOneRegister(pParams->hDemod, RSTV0900_P2_DMDT0M,
					0x20);
			ChipSetOneRegister(pParams->hDemod,
					   RSTV0900_P2_CFRINIT1,
					   (coarseFreq >> 8) & 0xff);
			ChipSetOneRegister(pParams->hDemod,
					   RSTV0900_P2_CFRINIT0,
					   coarseFreq & 0xff);
			/*Trig an acquisition (start the search) */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P2_DMDISTATE, 0x15);
		}
		break;
	}
	return coarseSymbolRate;
}

U16 FE_STV0900_BlindCheckAGC2MinLevel(FE_STV0900_InternalParams_t *pParams,
				      FE_STV0900_DEMOD_t Demod)
{
	U32 minagc2level = 0xffff, agc2level, initFreq, freqStep;

	S32 i, j, nbSteps, direction;

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:

		ChipSetOneRegister(pParams->hDemod, RSTV0900_P1_AGC2REF, 0x38);

		/*Enable the SR SCAN */
		ChipSetFieldImage(pParams->hDemod, FSTV0900_P1_SCAN_ENABLE, 0);
		/*activate the carrier frequency search loop */
		ChipSetFieldImage(pParams->hDemod, FSTV0900_P1_CFR_AUTOSCAN, 0);
		ChipSetRegisters(pParams->hDemod, RSTV0900_P1_DMDCFGMD, 1);

		ChipSetFieldImage(pParams->hDemod, FSTV0900_P1_AUTO_GUP, 1);
		/* set the SR max to 65Msps Write the MSB, auto mode */
		ChipSetRegisters(pParams->hDemod, RSTV0900_P1_SFRUP1, 1);

		ChipSetFieldImage(pParams->hDemod, FSTV0900_P1_AUTO_GLOW, 1);
		/* set the SR min to 400 Ksps Write the MSB */
		ChipSetRegisters(pParams->hDemod, RSTV0900_P1_SFRLOW1, 1);

		/*force the acquisition to stop at coarse carrier state */
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P1_DMDT0M, 0x0);

		FE_STV0900_SetSymbolRate(pParams->hDemod, pParams->MasterClock,
					 1000000, Demod);

		nbSteps = (pParams->Demod1SearchRange / 1000000);
		if (nbSteps <= 0)
			nbSteps = 1;

		direction = 1;
		freqStep = (1000000 << 8) / (pParams->MasterClock >> 8);
		initFreq = 0;
		for (i = 0; i < nbSteps; i++) {
			if (direction > 0)
				initFreq = initFreq + (freqStep * i);
			else
				initFreq = initFreq - (freqStep * i);
			direction *= -1;

			/*Reset the Demod */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P1_DMDISTATE, 0x5C);
			/* set the init freq offset */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P1_CFRINIT1, (initFreq >> 8) &
					0xff);
			/* set the init freq offset to the coarse freq offsset
			 * */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P1_CFRINIT0, initFreq & 0xff);
			/*Reset the Demod */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P1_DMDISTATE, 0x58);
			WAIT_N_MS(10);

			agc2level = 0;
			for (j = 0; j < 10; j++) {
				ChipGetRegisters(pParams->hDemod,
						 RSTV0900_P1_AGC2I1, 2);
				agc2level +=
				    (ChipGetFieldImage(pParams->hDemod,
				      FSTV0900_P1_AGC2_INTEGRATOR1) << 8) |
				    ChipGetFieldImage(pParams->hDemod,
					      FSTV0900_P1_AGC2_INTEGRATOR0);

			}
			agc2level /= 10;

			if (agc2level < minagc2level)
				minagc2level = agc2level;

		}

		break;

	case FE_SAT_DEMOD_2:

		ChipSetOneRegister(pParams->hDemod, RSTV0900_P2_AGC2REF, 0x38);

		/*Enable the SR SCAN */
		ChipSetFieldImage(pParams->hDemod, FSTV0900_P2_SCAN_ENABLE, 0);
		/*activate the carrier frequency search loop */
		ChipSetFieldImage(pParams->hDemod, FSTV0900_P2_CFR_AUTOSCAN, 0);
		ChipSetRegisters(pParams->hDemod, RSTV0900_P2_DMDCFGMD, 1);

		ChipSetFieldImage(pParams->hDemod, FSTV0900_P2_AUTO_GUP, 1);
		/* set the SR max to 65Msps Write the MSB, auto mode */
		ChipSetRegisters(pParams->hDemod, RSTV0900_P2_SFRUP1, 1);

		ChipSetFieldImage(pParams->hDemod, FSTV0900_P2_AUTO_GLOW, 1);
		/* set the SR min to 400 Ksps Write the MSB */
		ChipSetRegisters(pParams->hDemod, RSTV0900_P2_SFRLOW1, 1);

		/*force the acquisition to stop at coarse carrier state */
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P2_DMDT0M, 0x0);

		FE_STV0900_SetSymbolRate(pParams->hDemod, pParams->MasterClock,
					 1000000, Demod);
		nbSteps = (pParams->Demod2SearchRange / 1000000);
		if (nbSteps <= 0)
			nbSteps = 1;

		direction = 1;
		freqStep = (1000000 << 8) / (pParams->MasterClock >> 8);
		initFreq = 0;
		for (i = 0; i < nbSteps; i++) {
			if (direction > 0)
				initFreq = initFreq + (freqStep * i);
			else
				initFreq = initFreq - (freqStep * i);
			direction *= -1;

			/*Reset the Demod */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P2_DMDISTATE, 0x5C);
			/* set the init freq offset */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P2_CFRINIT1, (initFreq >> 8) &
					0xff);
			/* set the init freq offset to the coarse freq offsset
			 * */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P2_CFRINIT0, initFreq & 0xff);
			/*Reset the Demod */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P2_DMDISTATE, 0x58);
			WAIT_N_MS(10);

			agc2level = 0;
			for (j = 0; j < 10; j++) {
				ChipGetRegisters(pParams->hDemod,
						 RSTV0900_P2_AGC2I1, 2);
				agc2level +=
				    (ChipGetFieldImage(pParams->hDemod,
				      FSTV0900_P2_AGC2_INTEGRATOR1) << 8) |
				    ChipGetFieldImage(pParams->hDemod,
						 FSTV0900_P2_AGC2_INTEGRATOR0);
			}

			agc2level /= 10;

			if (agc2level < minagc2level)
				minagc2level = agc2level;

		}
		break;

	}

	return (U16)minagc2level;
}

BOOL FE_STV0900_BlindSearchAlgo(FE_STV0900_InternalParams_t *pParams,
				FE_STV0900_DEMOD_t Demod)
{

	U8 kRefTmgMax, kRefTmgMin;

	U32 coarseSymbolRate, agc2Threshold;
	BOOL lock = FALSE, coarseFail = FALSE;
	S32 demodTimeout = 500,
	    fecTimeout = 50, kRefTmgReg, failCpt, i, agc2OverFlow;

	S32 kRefTmg;

	U16 agc2Integrateur;
	U8 dstatus2;

#ifdef HOST_PC
#ifndef NO_GUI
	char message[100];
#endif
#endif

	if (pParams->hDemod->ChipID < 0x20) {
		kRefTmgMax = 233;/*KT max */
		kRefTmgMin = 143;/*KT min */

	} else {
		kRefTmgMax = 110;/*KT max */
		kRefTmgMin = 10;/*KT min */

	}

	if (pParams->hDemod->ChipID <= 0x20)
		agc2Threshold = STV0900_BLIND_SEARCH_AGC2_TH;
	else
		agc2Threshold = STV0900_BLIND_SEARCH_AGC2_TH_CUT30;

	agc2Integrateur = FE_STV0900_BlindCheckAGC2MinLevel(pParams, Demod);
#ifdef HOST_PC
#ifndef NO_GUI
	Fmt(message, "AGC2 MIN  = %d", agc2Integrateur);
	ReportInsertMessage(message);

#endif
#endif

	if (agc2Integrateur > agc2Threshold) {
		/*if the AGC2 > 700 then no signal around current frequency*/
		lock = FALSE;

#ifdef HOST_PC
#ifndef NO_GUI
		ReportInsertMessage("NO AGC2 : NO Signal");
#endif
#endif
		return lock;
	}

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:

		if (pParams->hDemod->ChipID == 0x10)
			/*Set Correlation frames reg for acquisition for cut
			 * 1.0 only */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P1_CORRELEXP, 0xAA);

		if (pParams->hDemod->ChipID < 0x20)
			/*cut 1.x */
			ChipSetOneRegister(pParams->hDemod, RSTV0900_P1_CARHDR,
					0x55);
		else
			/*cut 2.0 and 3.0 */
			ChipSetOneRegister(pParams->hDemod, RSTV0900_P1_CARHDR,
					0x20);

		if (pParams->hDemod->ChipID <= 0x20)
			/*Set The carrier search up and low to auto mode */
			/*applicable for cut 1.x and 2.0 */
			ChipSetOneRegister(pParams->hDemod, RSTV0900_P1_CARCFG,
					0xC4);
		else
			/*cut3.0 above */
			ChipSetOneRegister(pParams->hDemod, RSTV0900_P1_CARCFG,
					0x06);

		/*reduce the timing bandwith for high SR */
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P1_RTCS2, 0x44);

		if (pParams->hDemod->ChipID >= 0x20) {
			if (pParams->hDemod->ChipID == 0x30) {
				/* Blind optim */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P1_KTTMG, 0x55);
				if (pParams->Demod1SymbolRate < 10000000)
					/* Blind optim */
					ChipSetOneRegister(pParams->hDemod,
							RSTV0900_P1_AGC2O,
							0x5B);
				else
					/* Blind optim */
					ChipSetOneRegister(pParams->hDemod,
							RSTV0900_P1_AGC2O,
							0x5F);
			}
			/*Enable the equalizer */
			ChipSetOneRegister(pParams->hDemod,
					   RSTV0900_P1_EQUALCFG, 0x41);
			ChipSetOneRegister(pParams->hDemod,
					   RSTV0900_P1_FFECFG, 0x41);

			/* open the ReedSolomon to viterbi feed back until
			 * demod lock */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P1_VITSCALE, 0x82);
			/*set Viterbi hysteresis for search */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P1_VAVSRVIT, 0x0);
		}

		kRefTmgReg = RSTV0900_P1_KREFTMG;
		break;

	case FE_SAT_DEMOD_2:

		if (pParams->hDemod->ChipID == 0x10)
			/*Set Correlation frames reg for acquisition for cut
			 * 1.0 only */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P2_CORRELEXP, 0xAA);

		if (pParams->hDemod->ChipID < 0x20)
			/*cut 1.x */
			ChipSetOneRegister(pParams->hDemod, RSTV0900_P2_CARHDR,
					0x55);
		else
			/*cut 2.0 and 3.0 */
			ChipSetOneRegister(pParams->hDemod, RSTV0900_P2_CARHDR,
					0x20);

		if (pParams->hDemod->ChipID <= 0x20)
			/*Set The carrier search up and low to auto mode */
			/*applicable for cut 1.x and 2.0 */
			ChipSetOneRegister(pParams->hDemod, RSTV0900_P2_CARCFG,
					0xC4);
		else
			/*cut 3.0 above */
			ChipSetOneRegister(pParams->hDemod, RSTV0900_P2_CARCFG,
					0x06);
		/*reduce the timing bandwith for high SR */
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P2_RTCS2, 0x44);

		if (pParams->hDemod->ChipID >= 0x20) {
			if (pParams->hDemod->ChipID == 0x30) {
				/* Blind optim */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P2_KTTMG, 0x55);
				if (pParams->Demod2SymbolRate < 10000000)
					/* Blind optim */
					ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P2_AGC2O, 0x5B);
				else
					/* Blind optim */
					ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P2_AGC2O, 0x5F);
			}
			/*Enable the equalizer */
			ChipSetOneRegister(pParams->hDemod,
					   RSTV0900_P2_EQUALCFG, 0x41);
			ChipSetOneRegister(pParams->hDemod,
					   RSTV0900_P2_FFECFG, 0x41);

			/* open the ReedSolomon to viterbi feed back until
			 * demod lock */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P2_VITSCALE, 0x82);
			/*set Viterbi hysteresis for search */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P2_VAVSRVIT, 0x0);
		}

		kRefTmgReg = RSTV0900_P2_KREFTMG;
		break;
	}

	kRefTmg = kRefTmgMax;
	do {
		/*while no full lock & KT > KT min run coarse and fine search */
#ifdef HOST_PC
#ifndef NO_GUI
		Fmt(message, "KREFTMG = %x", kRefTmg);
		ReportInsertMessage(message);

#endif
#endif

		ChipSetOneRegister(pParams->hDemod, kRefTmgReg, kRefTmg);

		if (FE_STV0900_SearchSRCoarse(pParams, Demod) != 0) {
			/*if coarse SR != 0 then the coarse state is locked
			 * continue with the fine */
			coarseSymbolRate = FE_STV0900_SearchSRFine(pParams,
									Demod);
			if (coarseSymbolRate != 0) {
				/*if if coarse SR != 0then the SR found by the
				 * coarse is >= to the minimum expected SR
				 * (given by the user) in this case wait until
				 * full lock or timout*/
				FE_STV0900_GetLockTimeout(&demodTimeout,
							  &fecTimeout,
							  coarseSymbolRate,
							  FE_SAT_BLIND_SEARCH);
				lock =
				    FE_STV0900_GetDemodLock(pParams->hDemod,
						    Demod, demodTimeout);

			} else
				lock = FALSE;

		} else {
			failCpt = 0;
			agc2OverFlow = 0;
			switch (Demod) {
			case FE_SAT_DEMOD_1:
			default:

				for (i = 0; i < 10; i++) {
					ChipGetRegisters(pParams->hDemod,
							 RSTV0900_P1_AGC2I1, 2);
					agc2Integrateur +=
					    (ChipGetFieldImage(pParams->hDemod,
					      FSTV0900_P1_AGC2_INTEGRATOR1)
					     << 8) |
					    ChipGetFieldImage(pParams->hDemod,
						FSTV0900_P1_AGC2_INTEGRATOR0);
					if (agc2Integrateur >= 0xff00)
						agc2OverFlow++;

					dstatus2 =
					    ChipGetOneRegister(pParams->hDemod,
							  RSTV0900_P1_DSTATUS2);
					if (((dstatus2 & 0x1) == 0x1) &&
							((dstatus2 >> 7) == 1))
						/*if CFR over flow and timing
						 * overflow nothing here */
						failCpt++;
				}
				break;

			case FE_SAT_DEMOD_2:

				for (i = 0; i < 10; i++) {
					ChipGetRegisters(pParams->hDemod,
							RSTV0900_P2_AGC2I1, 2);
					agc2Integrateur +=
					    (ChipGetFieldImage(pParams->hDemod,
					      FSTV0900_P2_AGC2_INTEGRATOR1)
					     << 8) |
					    ChipGetFieldImage(pParams->hDemod,
						FSTV0900_P2_AGC2_INTEGRATOR0);
					if (agc2Integrateur >= 0xff00)
						agc2OverFlow++;

					dstatus2 =
					    ChipGetOneRegister(pParams->hDemod,
							  RSTV0900_P2_DSTATUS2);
					if (((dstatus2 & 0x1) == 0x1) &&
							((dstatus2 >> 7) == 1))
						/*if CFR over flow and timing
						 * overflow nothing here */
						failCpt++;
				}
				break;

			}

#ifdef HOST_PC
#ifndef NO_GUI
				Fmt(message, "failCpt = %d", failCpt);
				ReportInsertMessage(message);

#endif
#endif

			if ((failCpt > 7) || (agc2OverFlow > 7))
				coarseFail = TRUE;

			lock = FALSE;

		}
		kRefTmg -= 20;

	} while ((kRefTmg >= kRefTmgMin) && (lock == FALSE)
		 && (coarseFail == FALSE));
	if (pParams->hDemod->ChipID == 0x30) {
		switch (Demod) {
		case FE_SAT_DEMOD_1:
		default:
			/* Blind optim */
			ChipSetOneRegister(pParams->hDemod, RSTV0900_P1_AGC2O,
					0x5b);
			break;

		case FE_SAT_DEMOD_2:
			/* Blind optim */
			ChipSetOneRegister(pParams->hDemod, RSTV0900_P2_AGC2O,
					0x5b);
			break;
		}
	}

	return lock;

}

U32 FE_STV0900_CalcBitRate(FE_STV0900_TrackingStandard_t Standard,
			   U32 SymbolRate, FE_STV0900_Rate_t PunctureRate,
			   FE_STV0900_ModCod_t ModCode)
{
	U32 dataRate = 0, div = 100;

	switch (Standard) {
	case FE_SAT_DVBS1_STANDARD:
	case FE_SAT_DSS_STANDARD:
		/*
		   Formula:
		   DataRate = SR*2*PR*188/204
		 */
		if (SymbolRate > 15000000)
			div = 100;
		else
			div = 10;
		SymbolRate /= div;
		switch (PunctureRate) {

		case FE_SAT_PR_1_2:
			dataRate = ((SymbolRate) * 188) / 204;
			break;

		case FE_SAT_PR_2_3:
			dataRate = ((SymbolRate) * 188 * 4) / (204 * 3);
			break;

		case FE_SAT_PR_3_4:
			dataRate = ((SymbolRate) * 188 * 6) / (204 * 4);
			break;

		case FE_SAT_PR_5_6:
			dataRate = ((SymbolRate) * 188 * 10) / (204 * 6);
			break;

		case FE_SAT_PR_6_7:
			dataRate = ((SymbolRate) * 188 * 12) / (204 * 7);
			break;

		case FE_SAT_PR_7_8:
			dataRate = ((SymbolRate) * 188 * 14) / (204 * 8);
			break;

		default:
			dataRate = 0;
			break;

		}

		dataRate *= div;
		break;

	case FE_SAT_DVBS2_STANDARD:
		switch (ModCode) {
		case FE_SAT_QPSK_14:
			dataRate = 491 * (SymbolRate / 1000);
			break;

		case FE_SAT_QPSK_13:
			dataRate = 657 * (SymbolRate / 1000);
			break;

		case FE_SAT_QPSK_25:
			dataRate = 790 * (SymbolRate / 1000);
			break;

		case FE_SAT_QPSK_12:
			dataRate = 989 * (SymbolRate / 1000);
			break;

		case FE_SAT_QPSK_35:
			dataRate = 1189 * (SymbolRate / 1000);
			break;

		case FE_SAT_QPSK_23:
			dataRate = 1323 * (SymbolRate / 1000);
			break;

		case FE_SAT_QPSK_34:
			dataRate = 1488 * (SymbolRate / 1000);
			break;

		case FE_SAT_QPSK_45:
			dataRate = 1588 * (SymbolRate / 1000);
			break;

		case FE_SAT_QPSK_56:
			dataRate = 1655 * (SymbolRate / 1000);
			break;

		case FE_SAT_QPSK_89:
			dataRate = 1767 * (SymbolRate / 1000);
			break;

		case FE_SAT_QPSK_910:
			dataRate = 1789 * (SymbolRate / 1000);
			break;

		case FE_SAT_8PSK_35:
			dataRate = 1780 * (SymbolRate / 1000);
			break;

		case FE_SAT_8PSK_23:
			dataRate = 1981 * (SymbolRate / 1000);
			break;

		case FE_SAT_8PSK_34:
			dataRate = 2229 * (SymbolRate / 1000);
			break;

		case FE_SAT_8PSK_56:
			dataRate = 2479 * (SymbolRate / 1000);
			break;

		case FE_SAT_8PSK_89:
			dataRate = 2647 * (SymbolRate / 1000);
			break;

		case FE_SAT_8PSK_910:
			dataRate = 2680 * (SymbolRate / 1000);
			break;

		default:
			dataRate = 0;
			break;

		}
		break;

	case FE_SAT_UNKNOWN_STANDARD:
		dataRate = 0;
		break;
	default:
		break;
	}

	return dataRate;
}

/*****************************************************
--FUNCTION	::	FE_STV0900_Algo
--ACTION	::	Start search for a valid DVBS1/DVBS2 or DSS transponder
--PARAMS IN	::	pParams	->Pointer FE_STV0900_InternalParams_t structer
		::	Demod	->	current demod 1 or 2
--PARAMS OUT::	NONE
--RETURN	::	SYGNAL TYPE
--***************************************************/
FE_STV0900_SIGNALTYPE_t FE_STV0900_Algo(FE_STV0900_InternalParams_t *pParams,
					FE_STV0900_DEMOD_t Demod)
{
	S32 demodTimeout = 500,
	    fecTimeout = 50,
	    streamMergerField, iqPower, agc1Power, i, powerThreshold;

	U32 dataRate = 0, TSRate = 0;

	BOOL lock = FALSE, lowSR = FALSE, isDVBCI = FALSE;

	FE_STV0900_SIGNALTYPE_t signalType = FE_SAT_NOCARRIER;
	FE_STV0900_SearchAlgo_t algo;
	BOOL noSignal = FALSE;

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:

		algo = pParams->Demod1SearchAlgo;
		powerThreshold = STV0900_P1_IQPOWER_THRESHOLD;

		/*check if DVBCI mode */
		if (ChipGetFieldImage(pParams->hDemod,
					FSTV0900_P1_TSFIFO_DVBCI) == 1) {
			/*&& (ChipGetFieldImage (pParams->hDemod,
			 * FSTV0900_P1_TSFIFO_SERIAL)==0) */
			/*if DVBCI mode then set the TS clock to manual mode
			 * 8.933MHz before acquistion */
			ChipSetField(pParams->hDemod,
					FSTV0900_P1_TSFIFO_MANSPEED, 3);
			ChipSetOneRegister(pParams->hDemod, RSTV0900_P1_TSSPEED,
					0x3C);
			ChipSetFieldImage(pParams->hDemod,
					FSTV0900_P1_TSFIFO_DUTY50, 1);
			isDVBCI = TRUE;
		}

		/*Stop the Path 1 Stream Merger during acquisition */
		ChipSetFieldImage(pParams->hDemod, FSTV0900_P1_RST_HWARE, 1);
		ChipSetRegisters(pParams->hDemod, RSTV0900_P1_TSCFGH, 1);
		/*release reset DVBS2 packet delin */
		ChipSetField(pParams->hDemod, FSTV0900_P1_ALGOSWRST, 0);
		/* Blind optim */
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P1_AGC2O, 0x5b);
		streamMergerField = FSTV0900_P1_RST_HWARE;
		/*Stop the stream merger befor stopping the demod */
		/* Demod Stop */
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P1_DMDISTATE,
									0x5C);
		if (pParams->hDemod->ChipID >= 0x20) {
#ifdef GUI_ACM_REGS
			/*nominal value for cut 2.0 and 3.0 to lock down to
			 * -2.5dB C/N */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P1_CORRELABS, 0x82);
#else
			if (pParams->Demod1SymbolRate > 5000000)
				/*nominal value for cut 2.0 */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P1_CORRELABS, 0x9e);
			else
				/*nominal value for cut 2.0 */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P1_CORRELABS, 0x82);
#endif
		} else
			/*nominal value for cut 1.x */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P1_CORRELABS, 0x88);

		/*Get the demod and FEC timeout recomended value depending of
		 * the symbol rate and the search algo */
		FE_STV0900_GetLockTimeout(&demodTimeout, &fecTimeout,
					  pParams->Demod1SymbolRate,
					  pParams->Demod1SearchAlgo);

		if (pParams->Demod1SearchAlgo == FE_SAT_BLIND_SEARCH) {
			/* If the Symbolrate is unknown  set the BW to the max*/
			pParams->Tuner1BW = 2 * 36000000;
			/* if Blind search (Symbolrate is unknown) use a wider
			 * symbol rate scan range */
			ChipSetOneRegister(pParams->hDemod, RSTV0900_P1_TMGCFG2,
					   0xC0);
			ChipSetOneRegister(pParams->hDemod,
					   RSTV0900_P1_CORRELMANT, 0x70);
			/* If Blind search set the init symbol rate to 1Msps */
			FE_STV0900_SetSymbolRate(pParams->hDemod,
						 pParams->MasterClock, 1000000,
						 Demod);
		} else {
			/* If Symbolrate is known  set the BW to appropriate
			 * value */
			ChipSetOneRegister(pParams->hDemod, RSTV0900_P1_DMDT0M,
					   0x20);
			ChipSetOneRegister(pParams->hDemod, RSTV0900_P1_TMGCFG,
					   0xd2);

			if (pParams->Demod1SymbolRate < 2000000)
				/*If SR<2Msps set CORRELMANT to 0x63 */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P1_CORRELMANT, 0x63);
			else
				/*If SR>=2Msps set CORRELMANT to 0x70 */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P1_CORRELMANT, 0x70);

			/*All SR, AGC2REF acquistion = AGC2REF tracking = 0x38*/
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P1_AGC2REF, 0x38);

			if (pParams->hDemod->ChipID >= 0x20) {
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0900_P1_KREFTMG, 0x5a);
				if (pParams->Demod1SearchAlgo ==
				    FE_SAT_COLD_START)
					pParams->Tuner1BW = (15 *
						(FE_STV0900_CarrierWidth
						 (pParams->Demod1SymbolRate,
						  pParams->RollOff) +
						 10000000)) / 10;
				else if (pParams->Demod1SearchAlgo ==
						FE_SAT_WARM_START)
					/*WARM START */
					pParams->Tuner1BW =
					    FE_STV0900_CarrierWidth(pParams->
							    Demod1SymbolRate,
							    pParams->
							    RollOff) + 10000000;
			} else {
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0900_P1_KREFTMG, 0xc1);
				pParams->Tuner1BW = (15 *
						(FE_STV0900_CarrierWidth
						 (pParams->Demod1SymbolRate,
						  pParams->RollOff) +
						 10000000)) / 10;
			}
			/* if cold start or warm  (Symbolrate is known) use a
			 * Narrow  symbol rate scan range */
			ChipSetOneRegister(pParams->hDemod, RSTV0900_P1_TMGCFG2,
					   0xC1);
			/* Set the Init Symbol rate */
			FE_STV0900_SetSymbolRate(pParams->hDemod,
						 pParams->MasterClock,
						 pParams->Demod1SymbolRate,
						 Demod);
			FE_STV0900_SetMaxSymbolRate(pParams->hDemod,
						    pParams->MasterClock,
						    pParams->Demod1SymbolRate,
						    Demod);
			FE_STV0900_SetMinSymbolRate(pParams->hDemod,
						    pParams->MasterClock,
						    pParams->Demod1SymbolRate,
						    Demod);
			if (pParams->Demod1SymbolRate >= 10000000)
				/*If SR >=10Msps */
				lowSR = FALSE;
			else
				lowSR = TRUE;

		}

		/*Set the tuner frequency and bandwidth */
		FE_STV0900_SetTuner(pParams->hDemod, pParams->hTuner1,
				    pParams->Tuner1Type, Demod,
				    pParams->Tuner1Frequency,
				    pParams->Tuner1BW);

		/*      NO signal Detection */
		/*Read PowerI and PowerQ To check the signal Presence */
		ChipWaitOrAbort(pParams->hDemod, 10);
		ChipGetRegisters(pParams->hDemod, RSTV0900_P1_AGCIQIN1, 2);
		agc1Power =
		    MAKEWORD(ChipGetFieldImage
			     (pParams->hDemod, FSTV0900_P1_AGCIQ_VALUE1),
			     ChipGetFieldImage(pParams->hDemod,
					       FSTV0900_P1_AGCIQ_VALUE0));
		iqPower = 0;
		if (agc1Power == 0) {
			/*if AGC1 integrator value is 0 then read POWERI,
			 * POWERQ registers */
			/*Read the IQ power value */
			for (i = 0; i < 5; i++) {
				ChipGetRegisters(pParams->hDemod,
						 RSTV0900_P1_POWERI, 2);
				iqPower += (ChipGetFieldImage(pParams->hDemod,
							FSTV0900_P1_POWER_I) +
					      ChipGetFieldImage(pParams->hDemod,
						FSTV0900_P1_POWER_Q)) / 2;
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

		if ((agc1Power == 0) && (iqPower < powerThreshold)) {
			/*If (AGC1=0 and iqPower<IQThreshold)  then no signal */
			/*if AGC1 integrator ==0 and iqPower < Threshold then
			 * NO signal */
			pParams->Demod1Results.Locked = FALSE;
			signalType = FE_SAT_NOAGC1;

#ifdef HOST_PC
#ifndef NO_GUI
			ReportInsertMessage
			    ("NO AGC1 signal, NO POWERI, POWERQ Signal ");
			ReportInsertMessage
			    ("---------------------------------------------");
#endif
#endif

		} else {
			/*Set the IQ inversion search mode */
			ChipSetFieldImage(pParams->hDemod,
					  FSTV0900_P1_SPECINV_CONTROL,
					  pParams->Demod1Search_IQ_Inv);
			/*Set the rolloff to manual search mode */
			if (pParams->hDemod->ChipID <= 0x20)
				/*cut 2.0 */
				ChipSetFieldImage(pParams->hDemod,
						  FSTV0900_P1_MANUALSX_ROLLOFF,
						  1);
			else
				/*cut 3.0 */
				ChipSetFieldImage(pParams->hDemod,
						  FSTV0900_P1_MANUALS2_ROLLOFF,
						  1);

			ChipSetRegisters(pParams->hDemod, RSTV0900_P1_DEMOD, 1);

			FE_STV0900_SetSearchStandard(pParams, Demod);
			if (pParams->Demod1SearchAlgo != FE_SAT_BLIND_SEARCH)
				FE_STV0900_StartSearch(pParams, Demod);
		}

		break;

	case FE_SAT_DEMOD_2:

		algo = pParams->Demod2SearchAlgo;
		powerThreshold = STV0900_P2_IQPOWER_THRESHOLD;

		/*check if DVBCI mode */
		if (ChipGetFieldImage(pParams->hDemod,
					FSTV0900_P2_TSFIFO_DVBCI) == 1) {
			/*&& (ChipGetFieldImage (pParams->hDemod,
			 * FSTV0900_P2_TSFIFO_SERIAL)==0) */
			/*if DVBCI mode then set the TS clock to manual mode
			 * 8.933MHz before acquistion */
			ChipSetField(pParams->hDemod,
					FSTV0900_P2_TSFIFO_MANSPEED, 3);
			ChipSetOneRegister(pParams->hDemod, RSTV0900_P2_TSSPEED,
					0x3C);
			ChipSetFieldImage(pParams->hDemod,
					FSTV0900_P2_TSFIFO_DUTY50, 1);
			isDVBCI = TRUE;
		}

		/*Stop the Path 1 Stream Merger during acquisition */
		ChipSetFieldImage(pParams->hDemod, FSTV0900_P2_RST_HWARE, 1);
		ChipSetRegisters(pParams->hDemod, RSTV0900_P2_TSCFGH, 1);
		streamMergerField = FSTV0900_P2_RST_HWARE;
		/*release reset DVBS2 packet delin */
		ChipSetField(pParams->hDemod, FSTV0900_P2_ALGOSWRST, 0);
		/* Blind optim */
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P2_AGC2O, 0x5b);

		/*Stop the stream merger befor stopping the demod */
		/* Demod Stop */
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P2_DMDISTATE,
				0x5C);
		if (pParams->hDemod->ChipID >= 0x20) {
#ifdef GUI_ACM_REGS
			/*nominal value for cut 2.0 and 3.0 to lock down to
			 * -2.5dB C/N */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P2_CORRELABS, 0x82);
#else
			if (pParams->Demod2SymbolRate > 5000000)
				/*nominal value for cut 2.0 */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P2_CORRELABS, 0x9e);
			else
				/*nominal value for cut 2.0 */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P2_CORRELABS, 0x82);
#endif
		} else
			/*nominal value for cut 1.x */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P2_CORRELABS, 0x88);

		/*Get the demod and FEC timeout recomended value depending of
		 * the symbol rate and the search algo */
		FE_STV0900_GetLockTimeout(&demodTimeout, &fecTimeout,
					  pParams->Demod2SymbolRate,
					  pParams->Demod2SearchAlgo);

		if (pParams->Demod2SearchAlgo == FE_SAT_BLIND_SEARCH) {
			/*If the Symbolrate is unknown  set the BW to the
			 * maximum */
			pParams->Tuner2BW = 2 * 36000000;
			/* if Blind search (Symbolrate is unknown) use a wider
			 * symbol rate scan range */
			ChipSetOneRegister(pParams->hDemod, RSTV0900_P2_TMGCFG2,
					   0xC0);
			ChipSetOneRegister(pParams->hDemod,
					   RSTV0900_P2_CORRELMANT, 0x70);
			/*If blind search set the init symbol rate to 1Msps */
			FE_STV0900_SetSymbolRate(pParams->hDemod,
						 pParams->MasterClock, 1000000,
						 Demod);
		} else {
			/*If Symbolrate is known set the BW to appropriate val*/
			ChipSetOneRegister(pParams->hDemod, RSTV0900_P2_DMDT0M,
					   0x20);
			ChipSetOneRegister(pParams->hDemod, RSTV0900_P2_TMGCFG,
					   0xd2);

			if (pParams->Demod2SymbolRate < 2000000)
				/*If SR<2Msps set CORRELMANT to 0x63 */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P2_CORRELMANT, 0x63);
			else
				/*If SR>=2Msps set CORRELMANT to 0x70 */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P2_CORRELMANT, 0x70);

			/*All SR, AGC2REF acquistion = AGC2REF tracking = 0x38*/
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P2_AGC2REF, 0x38);

			if (pParams->hDemod->ChipID >= 0x20) {
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0900_P2_KREFTMG, 0x5a);
				if (pParams->Demod2SearchAlgo ==
				    FE_SAT_COLD_START)
					pParams->Tuner2BW = (15 *
						(FE_STV0900_CarrierWidth
						 (pParams->Demod2SymbolRate,
						  pParams->RollOff) +
						 10000000)) / 10;
				else if (pParams->Demod2SearchAlgo ==
						FE_SAT_WARM_START)
					/*WARM START */
					pParams->Tuner2BW =
					       FE_STV0900_CarrierWidth(pParams->
						Demod2SymbolRate,
						pParams->RollOff) + 10000000;
			} else {
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0900_P2_KREFTMG, 0xc1);
				pParams->Tuner2BW = (15 *
						(FE_STV0900_CarrierWidth
						 (pParams->Demod2SymbolRate,
						  pParams->RollOff) +
						 10000000)) / 10;
			}
			/* if Blind search (Symbolrate is unknown) use a Narrow
			 * symbol rate scan range */
			ChipSetOneRegister(pParams->hDemod, RSTV0900_P2_TMGCFG2,
					   0xC1);
			/* Set the Init Symbol rate */
			FE_STV0900_SetSymbolRate(pParams->hDemod,
						 pParams->MasterClock,
						 pParams->Demod2SymbolRate,
						 Demod);
			FE_STV0900_SetMaxSymbolRate(pParams->hDemod,
						    pParams->MasterClock,
						    pParams->Demod2SymbolRate,
						    Demod);
			FE_STV0900_SetMinSymbolRate(pParams->hDemod,
						    pParams->MasterClock,
						    pParams->Demod2SymbolRate,
						    Demod);

			if (pParams->Demod2SymbolRate >= 10000000)
				/*If SR >=10Msps */
				lowSR = FALSE;
			else
				lowSR = TRUE;

		}

		/*Set the tuner frequency and bandwidth */
		FE_STV0900_SetTuner(pParams->hDemod, pParams->hTuner2,
				    pParams->Tuner2Type, Demod,
				    pParams->Tuner2Frequency,
				    pParams->Tuner2BW);

		/*      NO signal Detection */
		/*Read PowerI and PowerQ To check the signal Presence */
		ChipWaitOrAbort(pParams->hDemod, 10);
		ChipGetRegisters(pParams->hDemod, RSTV0900_P2_AGCIQIN1, 2);
		agc1Power =
		    MAKEWORD(ChipGetFieldImage
			     (pParams->hDemod, FSTV0900_P2_AGCIQ_VALUE1),
			     ChipGetFieldImage(pParams->hDemod,
					       FSTV0900_P2_AGCIQ_VALUE0));
		iqPower = 0;
		if (agc1Power == 0) {
			/*if AGC1 integrator value is 0 then read POWERI,
			 * POWERQ registers */
			/*Read the IQ power value */
			for (i = 0; i < 5; i++) {
				ChipGetRegisters(pParams->hDemod,
						 RSTV0900_P2_POWERI, 2);
				iqPower += (ChipGetFieldImage(pParams->hDemod,
							FSTV0900_P2_POWER_I) +
					ChipGetFieldImage(pParams->hDemod,
						FSTV0900_P2_POWER_Q)) / 2;
			}
			iqPower /= 5;
		}
#ifdef HOST_PC
#ifndef NO_GUI
		/*if PC GUI read the IQ power threshold form the GUI user
		 * parameters */
		UsrRdInt("P2NOSignalThreshold", &powerThreshold);
#endif
#endif

		if ((agc1Power == 0) && (iqPower < powerThreshold)) {
			/*If (AGC1=0 and iqPower<IQThreshold)  then no signal*/
			/*if AGC1 integrator ==0 and iqPower < Threshold then
			 * NO signal */
			pParams->Demod2Results.Locked = FALSE;
			signalType = FE_SAT_NOAGC1;

#ifdef HOST_PC
#ifndef NO_GUI
			ReportInsertMessage
			    ("NO AGC1 signal, NO POWERI, POWERQ Signal ");
			ReportInsertMessage
			    ("---------------------------------------------");
#endif
#endif

		} else {
			/*Set the IQ inversion search mode */
			ChipSetFieldImage(pParams->hDemod,
					  FSTV0900_P2_SPECINV_CONTROL,
					  pParams->Demod2Search_IQ_Inv);
			/*Set the rolloff to manual search mode */
			if (pParams->hDemod->ChipID <= 0x20)
				/*cut 2.0 */
				ChipSetFieldImage(pParams->hDemod,
						  FSTV0900_P2_MANUALSX_ROLLOFF,
						  1);
			else
				/*cut 3.0 */
				ChipSetFieldImage(pParams->hDemod,
						  FSTV0900_P2_MANUALS2_ROLLOFF,
						  1);

			ChipSetRegisters(pParams->hDemod, RSTV0900_P2_DEMOD, 1);

			FE_STV0900_SetSearchStandard(pParams, Demod);
			if (pParams->Demod2SearchAlgo != FE_SAT_BLIND_SEARCH)
				FE_STV0900_StartSearch(pParams, Demod);
		}

		break;
	}

	if (signalType == FE_SAT_NOAGC1)
		return signalType;

	if (pParams->hDemod->ChipID == 0x12) {
		/*cut 1.2 only release TS reset */

		/*Release stream merger reset */
		ChipSetField(pParams->hDemod, streamMergerField, 0);
		ChipWaitOrAbort(pParams->hDemod, 3);
		/* Stream merger reset */
		ChipSetField(pParams->hDemod, streamMergerField, 1);
		/* Release stream merger reset */
		ChipSetField(pParams->hDemod, streamMergerField, 0);
	}

	if (algo == FE_SAT_BLIND_SEARCH) {
		/*special algo for blind search only */
		lock = FE_STV0900_BlindSearchAlgo(pParams, Demod);

	} else if (algo == FE_SAT_COLD_START)
		/*wait for demod lock specific for blind search */
		lock = FE_STV0900_GetDemodLockCold(pParams,
				demodTimeout, Demod);

	else if (algo == FE_SAT_WARM_START)
		/* case warm or cold start wait for demod lock */
		lock = FE_STV0900_GetDemodLock(pParams->hDemod, Demod,
				demodTimeout);

	if ((lock == FALSE) && (algo == FE_SAT_COLD_START)) {
		if (lowSR == FALSE) {
			/*If Timing OK & SR >= 10Msps run the SW Algo*/
			if (FE_STV0900_CheckTimingLock
			    (pParams->hDemod, Demod) == TRUE)
				/* if demod lock fail and cold start
				 * (zapping mode) run the SW
				 * acquisition algo */
				lock = FE_STV0900_SWAlgo(pParams,
								Demod);
		}
	}

	if (lock == TRUE)
		/* Read signal caracteristics and check the lock range*/
		signalType = FE_STV0900_GetSignalParams(pParams, Demod);
	if (!(lock == TRUE) || !(signalType == FE_SAT_RANGEOK))
		goto no_lock_or_signal;

	/*The tracking optimization and the FEC lock check are perfomed only if:
	   demod is locked and signal type is RANGEOK i.e found a TP within the
	   given search range
	 */

	/* Optimization setting for tracking */
	FE_STV0900_TrackingOptimization(pParams, Demod);

	if (pParams->hDemod->ChipID <= 0x11) {
		/*workaround for dual DVBS1 cut 1.1 and 1.0
		 * only */
		if ((FE_STV0900_GetStandard(pParams->hDemod, FE_SAT_DEMOD_1) ==
					FE_SAT_DVBS1_STANDARD) &&
			(FE_STV0900_GetStandard(pParams->hDemod, FE_SAT_DEMOD_2)
						== FE_SAT_DVBS1_STANDARD)) {
			/*if DVBS1 on path 1 and path 2 wait 20 ms before
			 * stream merger release */
			ChipWaitOrAbort(pParams->hDemod, 20);
			/*Release stream merger reset */
			ChipSetField(pParams->hDemod, streamMergerField, 0);
		} else {
			/*Release stream merger reset */
			ChipSetField(pParams->hDemod, streamMergerField, 0);
			ChipWaitOrAbort(pParams->hDemod, 3);
			/* Stream merger reset */
			ChipSetField(pParams->hDemod, streamMergerField, 1);
			/* Release stream merger reset */
			ChipSetField(pParams->hDemod, streamMergerField, 0);
		}
	} else if (pParams->hDemod->ChipID >= 0x20) {
		/*cut 2.0 above :release TS reset after demod lock and
		 * TrackingOptimization */

		/*Release stream merger reset */
		ChipSetField(pParams->hDemod, streamMergerField, 0);
		ChipWaitOrAbort(pParams->hDemod, 3);
		/* Stream merger reset */
		ChipSetField(pParams->hDemod, streamMergerField, 1);
		/* Release stream merger reset */
		ChipSetField(pParams->hDemod, streamMergerField, 0);
	}

	if (isDVBCI) {
		/*
		   if DVBCI mode then set the TS clock to auto instantaneous to
		   establish the correct TS clock for the found bit rate* the
		   set back the TS clock to manual so it will note change.
		 */

		switch (Demod) {
		case FE_SAT_DEMOD_1:
		default:
			dataRate =
			 FE_STV0900_CalcBitRate(pParams->Demod1Results.Standard,
					pParams->Demod1Results.SymbolRate,
					pParams->Demod1Results.PunctureRate,
					pParams->Demod1Results.ModCode);
			break;

		case FE_SAT_DEMOD_2:
			dataRate =
			 FE_STV0900_CalcBitRate(pParams->Demod2Results.Standard,
					 pParams->Demod2Results.SymbolRate,
					 pParams->Demod2Results.PunctureRate,
					 pParams->Demod2Results.ModCode);
			break;
		}

		if (dataRate != 0) {
			/* TS setting when TS Clock must be > 8.933 , in this
			 * case set the TS clock to manual before checking the
			 * final lock */
			dataRate /= 8;
			if (pParams->hDemod->ChipID >= 0x30)
				/*
				   in cut 3.0 the required TS clock is equal to
				   the theoritical byte clock increased by
				   0.34% times the theoritical byte clock
				   Formula: TS_Clock = TS_ClockTH[1+
				   (TS_ClockTH*0.0034)] with TS_Clock is the
				   required 903 TS clock and TS_ClockTH is the
				   tehorical byte clock.
				 */
				TSRate = (dataRate / 10000) * ((34 * dataRate /
							1000000) + 10000);
			else
				/*
				   in cut 2.0 the required TS clock is equal to
				   the theoritical byte clock increased by
				   0.64% times the theoritical byte clock
				   TS_Clock = TS_ClockTH[1+
				   (TS_ClockTH*0.0064)] with TS_Clock is the
				   required 903 TS clock and TS_ClockTH is the
				   tehorical byte clock.
				 */
				TSRate = (dataRate / 10000) * ((64 * dataRate /
							1000000) + 10000);

			TSRate = 4 * (pParams->MasterClock / TSRate);
			if (TSRate < 0x3C) {
				switch (Demod) {
				case FE_SAT_DEMOD_1:
				default:
					ChipSetOneRegister(pParams->hDemod,
							RSTV0900_P1_TSSPEED,
							TSRate);
					break;

				case FE_SAT_DEMOD_2:
					ChipSetOneRegister(pParams->hDemod,
							RSTV0900_P2_TSSPEED,
							TSRate);
					break;
				}
			}

		}

	}

	if (FE_STV0900_WaitForLock(pParams->hDemod, Demod, fecTimeout,
				fecTimeout) == TRUE) {
		lock = TRUE;
		switch (Demod) {
		case FE_SAT_DEMOD_1:
		default:
			pParams->Demod1Results.Locked = TRUE;
			if (pParams->Demod1Results.Standard ==
					FE_SAT_DVBS2_STANDARD) {
				/*case DVBS2 found set the rolloff, the rolloff
				 * setting for DVBS2 should be done after full
				 * lock (demod+FEC) */
				FE_STV0900_Setdvbs2RollOff(pParams->hDemod,
						Demod);

				/*reset DVBS2 packet delinator error counter */
				ChipSetFieldImage(pParams->hDemod,
					FSTV0900_P1_RESET_UPKO_COUNT, 1);
				ChipSetRegisters(pParams->hDemod,
						RSTV0900_P1_PDELCTRL2, 1);

				/*reset DVBS2 packet delinator error counter */
				ChipSetFieldImage(pParams->hDemod,
					FSTV0900_P1_RESET_UPKO_COUNT, 0);
				ChipSetRegisters(pParams->hDemod,
						RSTV0900_P1_PDELCTRL2, 1);

				if (pParams->BER_Algo !=
						FE_BER_Algo_Appli_Berwindow)
					/*Reset the error counter to DVBS2
					 * packet error rate, Average window
					 * mode */
					ChipSetOneRegister(pParams->hDemod,
							RSTV0900_P1_ERRCTRL1,
							0x67);
				else
					/*Reset the error counter to DVBS2
					 * packet error rate, Infinite window
					 * mode */
					ChipSetOneRegister(pParams->hDemod,
							RSTV0900_P1_ERRCTRL1,
							0x61);
			} else {
				if (pParams->BER_Algo !=
						FE_BER_Algo_Appli_Berwindow)
					/* reset the viterbi bit error rate,
					 * Average window mode */
					ChipSetOneRegister(pParams->hDemod,
							RSTV0900_P1_ERRCTRL1,
							0x75);
				else
					/* reset the viterbi bit error rate,
					 * Infinite window mode */
					ChipSetOneRegister(pParams->hDemod,
							RSTV0900_P1_ERRCTRL1,
							0x71);
			}

			/*Reset the Total packet counter */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P1_FBERCPT4, 0);
			/*Reset the packet Error counter2 (and Set it to
			 * infinit error count mode) */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P1_ERRCTRL2, 0xc1);

			break;

		case FE_SAT_DEMOD_2:
			pParams->Demod2Results.Locked = TRUE;
			if (pParams->Demod2Results.Standard ==
					FE_SAT_DVBS2_STANDARD) {
				/*case DVBS2 found set the rolloff, the rolloff
				 * setting for DVBS2 should be done after full
				 * lock (demod+FEC) */
				FE_STV0900_Setdvbs2RollOff(pParams->hDemod,
						Demod);

				/*reset DVBS2 packet delinator error counter */
				ChipSetFieldImage(pParams->hDemod,
					FSTV0900_P2_RESET_UPKO_COUNT, 1);
				ChipSetRegisters(pParams->hDemod,
						RSTV0900_P2_PDELCTRL2, 1);

				/*reset DVBS2 packet delinator error counter */
				ChipSetFieldImage(pParams->hDemod,
					FSTV0900_P2_RESET_UPKO_COUNT, 0);
				ChipSetRegisters(pParams->hDemod,
						RSTV0900_P2_PDELCTRL2, 1);

				if (pParams->BER_Algo !=
						FE_BER_Algo_Appli_Berwindow)
					/* reset the error counter to DVBS2
					 * packet error rate, Average window
					 * mode */
					ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P2_ERRCTRL1, 0x67);
				else
					/* reset the error counter to DVBS2
					 * packet error rate, Infinite window
					 * mode */
					ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P2_ERRCTRL1, 0x61);
			} else {
				if (pParams->BER_Algo !=
						FE_BER_Algo_Appli_Berwindow)
					/* reset the viterbi bit error rate,
					 * Average window mode */
					ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P2_ERRCTRL1, 0x75);
				else
					/* reset the viterbi bit error rate,
					 * Infinite window mode */
					ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P2_ERRCTRL1, 0x71);
			}
			/*Reset the Total packet counter */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P2_FBERCPT4, 0);
			/*Reset the packet Error counter2 (and Set it to
			 * infinit error count mode) */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P2_ERRCTRL2, 0xc1);

			break;
		}
		if (isDVBCI) {
			/*
			   if DVBCI mode then set the TS clock to auto
			   instantaneous to establish the correct TS clock for
			   the found bit rate* the set back the TS clock to
			   manual so it will note change.
			 */

			if (dataRate != 0) {
				/* TS setting when TS Clock must be <= 8.933 ,
				 * in this case set the TS clock after checking
				 * the final lock */
				if (TSRate >= 0x3C) {
					/* the operation manual->auto->manual
					 * can only be done after the final
					 * lock */
					switch (Demod) {
					case FE_SAT_DEMOD_1:
					default:
						ChipSetField(pParams->hDemod,
						    FSTV0900_P1_TSFIFO_MANSPEED,
						    2);
						ChipSetField(pParams->hDemod,
						    FSTV0900_P1_TSFIFO_MANSPEED,
						    3);
						break;

					case FE_SAT_DEMOD_2:
						ChipSetField(pParams->hDemod,
						    FSTV0900_P2_TSFIFO_MANSPEED,
						    2);
						ChipSetField(pParams->hDemod,
						    FSTV0900_P2_TSFIFO_MANSPEED,
						    3);
						break;
					}
				}
			}
		}

	} else {
		lock = FALSE;
		/*if the demod is locked & not the FEC signal type is no DATA*/
		signalType = FE_SAT_NODATA;
		noSignal = FE_STV0900_CheckSignalPresence(pParams, Demod);
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

no_lock_or_signal:
	if (!(signalType == FE_SAT_NODATA) || !(noSignal == FALSE))
		return signalType;

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		if (pParams->hDemod->ChipID <= 0x11) {
			/*workaround for cut 1.1 and 1.0 only*/
			if ((ChipGetField(pParams->hDemod,
				FSTV0900_P1_HEADER_MODE) == FE_SAT_DVBS_FOUND)
				 && (pParams->Demod1Search_IQ_Inv <=
					 FE_SAT_IQ_AUTO_NORMAL_FIRST))
				/*case of false lock DVBS1 in IQ auto mode */
				signalType = FE_STV0900_DVBS1AcqWorkAround
					(pParams, Demod);
		} else
			pParams->Demod1Results.Locked = FALSE;
		break;

	case FE_SAT_DEMOD_2:
		if (pParams->hDemod->ChipID <= 0x11) {
			/*workaround for cut 1.1 and 1.0 only*/
			if ((ChipGetField(pParams->hDemod,
				FSTV0900_P2_HEADER_MODE) == FE_SAT_DVBS_FOUND)
				 && (pParams->Demod2Search_IQ_Inv <=
					 FE_SAT_IQ_AUTO_NORMAL_FIRST))
				/*case of false lock DVBS1 in IQ auto mode */
				signalType = FE_STV0900_DVBS1AcqWorkAround
					(pParams, Demod);
		} else
			pParams->Demod2Results.Locked = FALSE;
		break;
	}

	return signalType;
}

S32 FE_STV0900_SetHMRFilter(STCHIP_Handle_t hChip,
			    TUNER_Handle_t hTuner,
			    STCHIP_Handle_t hChipVGLNA,
			    FE_STV0900_DEMOD_t Demod)
{
	S32 hmrVal = 0,
	    agc1InitVal,
	    agc1PowerVal2, agc1Reg = 0, agc1Field_msb = 0, agc1Field_lsb = 0;

	U32 tunerFrequency;
	U8 BBgain = 12;

	/* HMR filter calibration , specific 6120 */
	if (!(((SAT_TUNER_Params_t)(hTuner->pData))->Model ==
						TUNER_STV6120_Tuner1)
	    && !(((SAT_TUNER_Params_t)(hTuner->pData))->Model ==
						TUNER_STV6120_Tuner2))
		return hmrVal;

	switch (Demod) {
	case FE_SAT_DEMOD_1:
		agc1Reg = RSTV0900_P1_AGCIQIN1;
		agc1Field_msb = FSTV0900_P1_AGCIQ_VALUE1;
		agc1Field_lsb = FSTV0900_P1_AGCIQ_VALUE0;
		break;

	case FE_SAT_DEMOD_2:
		agc1Reg = RSTV0900_P2_AGCIQIN1;
		agc1Field_msb = FSTV0900_P2_AGCIQ_VALUE1;
		agc1Field_lsb = FSTV0900_P2_AGCIQ_VALUE0;
		break;
	}

	if ((FE_STV0900_CheckVGLNAPresent(hChipVGLNA) == 0) || (!hChipVGLNA)) {
		BBgain = 6;
	} else {
		if ((ChipGetField(hChipVGLNA, FSTVVGLNA_SWLNAGAIN) == 3) &&
			    (ChipGetField(hChipVGLNA, FSTVVGLNA_RFAGCLOW) == 1))
			BBgain = 6;
		else
			BBgain = 12;
	}
	FE_Sat_TunerSetGain(hTuner, /*6 */ BBgain);
	FE_Sat_TunerSetHMR_Filter(hTuner, 0);

	tunerFrequency = FE_Sat_TunerGetFrequency(hTuner);
	if (tunerFrequency >= 1590000)
		return hmrVal;

	ChipWaitOrAbort(hChip, 5);

	ChipGetRegisters(hChip, agc1Reg, 2);
	agc1InitVal = MAKEWORD(ChipGetFieldImage(hChip, agc1Field_msb),
				ChipGetFieldImage(hChip, agc1Field_lsb));

	FE_Sat_TunerSetGain(hTuner, /*8 */ BBgain + 2);
	ChipWaitOrAbort(hChip, 5);

	ChipGetRegisters(hChip, agc1Reg, 2);
	agc1PowerVal2 = MAKEWORD(ChipGetFieldImage(hChip, agc1Field_msb),
				ChipGetFieldImage(hChip, agc1Field_lsb));
	hmrVal = 0;

	if (agc1InitVal == 0) {
		FE_Sat_TunerSetHMR_Filter(hTuner, 31);
		return hmrVal;
	}

	while ((agc1PowerVal2 > agc1InitVal) && (hmrVal < 29)) {
		hmrVal += 2;
		FE_Sat_TunerSetHMR_Filter(hTuner, hmrVal);

		ChipWaitOrAbort(hChip, 5);
		ChipGetRegisters(hChip, agc1Reg, 2);
		agc1PowerVal2 = MAKEWORD(ChipGetFieldImage(hChip, agc1Field_msb)
				, ChipGetFieldImage(hChip, agc1Field_lsb));
	}

	return hmrVal;
}

BOOL FE_STV0900_LDPCPowerMonitoring(STCHIP_Handle_t hChip,
				    FE_STV0900_DEMOD_t Demod)
{
	/*This function is to be used for CCM only not ACM or VCM */
	S32 packetErrorCount, frameErrorCount;

	static BOOL path1LdpcStopped = FALSE;
	static BOOL path2LdpcStopped = FALSE;

	BOOL ldpcStopped = FALSE;

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		/*read the error counter */
		packetErrorCount =
		    MAKEWORD(ChipGetOneRegister(hChip, RSTV0900_P1_UPCRCKO1),
			     ChipGetOneRegister(hChip, RSTV0900_P1_UPCRCKO0));
		frameErrorCount =
		    MAKEWORD(ChipGetOneRegister(hChip, RSTV0900_P1_BBFCRCKO1),
			     ChipGetOneRegister(hChip, RSTV0900_P1_BBFCRCKO0));

		/*reset DVBS2 packet delinator error counter */
		ChipSetFieldImage(hChip, FSTV0900_P1_RESET_UPKO_COUNT, 1);
		ChipSetRegisters(hChip, RSTV0900_P1_PDELCTRL2, 1);

		/*reset DVBS2 packet delinator error counter */
		ChipSetFieldImage(hChip, FSTV0900_P1_RESET_UPKO_COUNT, 0);
		/*reset DVBS2 packet delineator error counter */
		ChipSetRegisters(hChip, RSTV0900_P1_PDELCTRL2, 1);

		if ((packetErrorCount >= 200) || (frameErrorCount >= 5)) {
			/*disable all frame type (stop the LDPC for path1)) */
			FE_STV0900_StopALL_S2_Modcod(hChip, Demod);
			path1LdpcStopped = TRUE;
		} else {
			if (path1LdpcStopped == TRUE)
				/*enable the LDPC for path1) */
				FE_STV0900_ActivateS2ModCode(hChip, Demod);
			path1LdpcStopped = FALSE;
		}
		ldpcStopped = path1LdpcStopped;
		break;

	case FE_SAT_DEMOD_2:
		/*read the error counter */
		packetErrorCount =
		    MAKEWORD(ChipGetOneRegister(hChip, RSTV0900_P2_UPCRCKO1),
			     ChipGetOneRegister(hChip, RSTV0900_P2_UPCRCKO0));
		frameErrorCount =
		    MAKEWORD(ChipGetOneRegister(hChip, RSTV0900_P2_BBFCRCKO1),
			     ChipGetOneRegister(hChip, RSTV0900_P2_BBFCRCKO0));

		/*reset DVBS2 packet delinator error counter */
		ChipSetFieldImage(hChip, FSTV0900_P2_RESET_UPKO_COUNT, 1);
		ChipSetRegisters(hChip, RSTV0900_P2_PDELCTRL2, 1);

		/*reset DVBS2 packet delinator error counter */
		ChipSetFieldImage(hChip, FSTV0900_P2_RESET_UPKO_COUNT, 0);
		/*reset DVBS2 packet delineator error counter */
		ChipSetRegisters(hChip, RSTV0900_P2_PDELCTRL2, 1);

		if ((packetErrorCount >= 200) || (frameErrorCount >= 5)) {
			/*disable all frame type (stop the LDPC for path2)) */
			FE_STV0900_StopALL_S2_Modcod(hChip, Demod);
			path2LdpcStopped = TRUE;
		} else {
			if (path2LdpcStopped == TRUE)
				/*enable the LDPC for path2) */
				FE_STV0900_ActivateS2ModCode(hChip, Demod);
			path2LdpcStopped = FALSE;
		}
		ldpcStopped = path2LdpcStopped;
		break;

	}
	return ldpcStopped;
}


/****************************************************************
****************************************************************
****************************************************************
****
***************************************************
**			PUBLIC ROUTINES		**
***************************************************
****
****************************************************************
****************************************************************
****************************************************************/


/*****************************************************
--FUNCTION	::	FE_STV0900_GetRevision
--ACTION	::	Return current LLA version
--PARAMS IN	::	NONE
--PARAMS OUT::	NONE
--RETURN	::	Revision ==> Text string containing LLA version
--***************************************************/
ST_Revision_t FE_STV0900_GetRevision(void)
{
	return RevisionSTV0900;
}

/*****************************************************
--FUNCTION	::	FE_STV0900_Init
--ACTION	::	Initialisation of the STV0900 chip
--PARAMS IN	::	pInit	==>	Front End Init parameters
--PARAMS OUT::	NONE
--RETURN	::	Handle to STV0900
--***************************************************/
FE_STV0900_Error_t FE_STV0900_Init(FE_STV0900_InitParams_t *pInit,
				   FE_STV0900_Handle_t *Handle)
{
	FE_STV0900_InternalParams_t *pParams = NULL;

	/* Demodulator chip initialisation parameters */
	Demod_InitParams_t DemodInitParams;
	SAT_TUNER_InitParams_t TunerInitParams;

	FE_STV0900_Error_t error = FE_LLA_NO_ERROR;

	TUNER_Error_t tuner1Error = TUNER_NO_ERR, tuner2Error = TUNER_NO_ERR;

	STCHIP_Error_t demodError = CHIPERR_NO_ERROR;

	/* Internal params structure allocation */
#ifdef HOST_PC
	STCHIP_Info_t DemodChip;
	pParams = calloc(1, sizeof(FE_STV0900_InternalParams_t));
	(*Handle) = (FE_STV0900_Handle_t) pParams;
#endif

#ifdef CHIP_STAPI
	pParams = (FE_STV0900_InternalParams_t *)(*Handle);
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

	FE_STV0900_SetTunerType(pParams->hDemod, pInit->TunerHWControlType,
			pInit->Tuner_I2cAddr, pInit->TunerRefClock,
			pInit->TunerOutClkDivider, FE_SAT_DEMOD_1);
	FE_STV0900_SetTunerType(pParams->hDemod, pInit->Tuner2HWControlType,
			pInit->Tuner2_I2cAddr, pInit->Tuner2RefClock,
			pInit->Tuner2OutClkDivider, FE_SAT_DEMOD_2);

	ChipSetField(pParams->hDemod, FSTV0900_P1_TUN_IQSWAP,
			pInit->TunerIQ_Inversion);
	ChipSetField(pParams->hDemod, FSTV0900_P2_TUN_IQSWAP,
			pInit->Tuner2IQ_Inversion);

#else
	/* Demodulator (STV0900) Init */
#ifdef CHIP_STAPI
	DemodInitParams.Chip = (pParams->hDemod);
#else
	DemodInitParams.Chip = &DemodChip;
#endif

	DemodInitParams.NbDefVal = STV0900_NBREGS;
	DemodInitParams.Chip->RepeaterHost = NULL;
	DemodInitParams.Chip->RepeaterFn = NULL;
	DemodInitParams.Chip->Repeater = FALSE;
	DemodInitParams.Chip->I2cAddr = pInit->DemodI2cAddr;
	strcpy((char *)DemodInitParams.Chip->Name, pInit->DemodName);

	demodError = STV0900_Init(&DemodInitParams, &(pParams->hDemod));

	if (pInit->TunerModel != TUNER_NULL) {
		/*Tuner 1 Init */
		TunerInitParams.Model = pInit->TunerModel;
		TunerInitParams.TunerName = pInit->TunerName;
		strcpy((char *)TunerInitParams.TunerName,
		       pInit->TunerName);
		TunerInitParams.TunerI2cAddress = pInit->Tuner_I2cAddr;
		TunerInitParams.Reference = pInit->TunerRefClock;
		TunerInitParams.IF = 0;
		if (pInit->TunerIQ_Inversion == FE_SAT_IQ_NORMAL)
			TunerInitParams.IQ_Wiring = TUNER_IQ_NORMAL;
		else
			TunerInitParams.IQ_Wiring = TUNER_IQ_INVERT;
#if 0	/* formerly excluded lines */
		TunerInitParams.BandSelect = pInit->TunerBandSelect;
#endif /* formerly excluded lines */
		TunerInitParams.RepeaterHost = pParams->hDemod;
		/* printf("<\n******%s><%d>..Tuner Model..=%d", __func__,
		 * __LINE__, pInit->TunerModel); */
		TunerInitParams.RepeaterFn = FE_STV0900_RepeaterFn;
		TunerInitParams.DemodModel = DEMOD_STV0900;
		TunerInitParams.OutputClockDivider =
		    pInit->TunerOutClkDivider;
		/*TunerInitParams.Lna_agc_mode = pInit->TunerLnaAgcMode; */
		/* * for 6111 tuner */
		/*TunerInitParams.Lna_agc_ref = pInit->TunerLnaAgcRef; */
		if ((TunerInitParams.Model == TUNER_STV6120_Tuner1)
		    || (TunerInitParams.Model ==
			TUNER_STV6120_Tuner2)) {
			TunerInitParams.RF_Source =
			    pInit->TunerRF_Source;
			TunerInitParams.InputSelect =
			    pInit->TunerInputSelect;
		}
		tuner1Error =
		    FE_Sat_TunerInit(&TunerInitParams,
				     &(pParams->hTuner1));
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
#if 0	/* formerly excluded lines */
		TunerInitParams.BandSelect = pInit->Tuner2BandSelect;
#endif /* formerly excluded lines */
		TunerInitParams.RepeaterHost = pParams->hDemod;
		/* printf("\n**************<%s><%d>..Tuner Model..=%d",
		 * __func__, __LINE__, pInit->Tuner2Model); */
		if ((TunerInitParams.Model == TUNER_STV6120_Tuner1)
		    || (TunerInitParams.Model ==
			TUNER_STV6120_Tuner2)) {
			TunerInitParams.RepeaterFn =
			    FE_STV0900_RepeaterFn;
		} else {
			TunerInitParams.RepeaterFn =
			    FE_STV0900_Repeater2Fn;
		}
		TunerInitParams.DemodModel = DEMOD_STV0900;
		TunerInitParams.OutputClockDivider =
		    pInit->Tuner2OutClkDivider;
		/*TunerInitParams.Lna_agc_mode = pInit->Tuner2LnaAgcMode;
		   TunerInitParams.Lna_agc_ref = pInit->Tuner2LnaAgcRef;
		 */
		if ((TunerInitParams.Model == TUNER_STV6120_Tuner1)
		    || (TunerInitParams.Model ==
			TUNER_STV6120_Tuner2)) {
			TunerInitParams.RF_Source =
			    pInit->Tuner2RF_Source;
			TunerInitParams.InputSelect =
			    pInit->Tuner2InputSelect;
		}
		tuner2Error =
		    FE_Sat_TunerInit(&TunerInitParams,
				     &(pParams->hTuner2));
	}

	if (demodError == CHIPERR_NO_ERROR) {
		/*Check the demod error first */
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
	/* the function "FE_STV0900_SetDVBS2_Single" could be used then
	   to switch from single on path1 to single path2 or to dual any time
	   during the diffrent zapping
	 */
	FE_STV0900_SetDVBS2_Single(pParams, pParams->DemodType, FE_SAT_DEMOD_1);

	/*Read IC cut ID */
	pParams->hDemod->ChipID = ChipGetOneRegister(pParams->hDemod,
								RSTV0900_MID);

	/*Tuner parameters */
	pParams->Tuner1Type = pInit->TunerHWControlType;
	pParams->Tuner2Type = pInit->Tuner2HWControlType;
	pParams->Tuner1Global_IQ_Inv = pInit->TunerIQ_Inversion;
	pParams->Tuner2Global_IQ_Inv = pInit->Tuner2IQ_Inversion;

	pParams->RollOff = pInit->RollOff;
#if defined(CHIP_STAPI) || defined(NO_GUI)
	/*settings for STTUNER or non-Gui applications or Auto test */
	/*Ext clock in Hz */
	pParams->Quartz = pInit->DemodRefClock;

	ChipSetField(pParams->hDemod, FSTV0900_P1_ROLLOFF_CONTROL,
								pInit->RollOff);
	ChipSetField(pParams->hDemod, FSTV0900_P2_ROLLOFF_CONTROL,
								pInit->RollOff);

	switch (pInit->TunerHWControlType) {
	case FE_SAT_SW_TUNER:
	default:
		if (pParams->hTuner1) {
			/* Set the tuner reference clock */
			FE_Sat_TunerSetReferenceFreq(pParams->hTuner1,
							pInit->TunerRefClock);

			/*printf("\n IN 900_init.c P1 the Bbgain that will be
			 * set is %d", pInit->TunerBasebandGain); */
			/*Set the tuner base band gain */
			FE_Sat_TunerSetGain(pParams->hTuner1,
						pInit->TunerBasebandGain);
		}
		/*Set the ADC's range according to the tuner Model */
		switch (pInit->TunerModel) {
		case TUNER_STB6000:
		case TUNER_STB6100:
			/*Set the ADC range to 2Vpp */
			ChipSetOneRegister(pParams->hDemod, RSTV0900_TSTTNR1,
									0x27);
			break;

		case TUNER_STV6110:
		case TUNER_STV6130:
			/*Set the ADC range to 1Vpp */
			ChipSetOneRegister(pParams->hDemod, RSTV0900_TSTTNR1,
									0x26);
			break;

		default:
			break;

		}

		break;

	case FE_SAT_AUTO_STB6000:
	case FE_SAT_AUTO_STB6100:
	case FE_SAT_AUTO_STV6110:
		/*init the 900 Tuner controller */
		FE_STV0900_SetTunerType(pParams->hDemod,
				pInit->TunerHWControlType,
				pInit->Tuner_I2cAddr,
				pInit->TunerRefClock,
				pInit->TunerOutClkDivider, FE_SAT_DEMOD_1);
		/*Set the BB gain using the 900 Tuner controller */
		FE_STV0900_TunerSetBBGain(pParams->hDemod,
				pInit->TunerHWControlType,
				pInit->TunerBasebandGain, FE_SAT_DEMOD_1);

		if (ChipGetField(pParams->hDemod, FSTV0900_P1_TUN_ACKFAIL)) {
			if (pParams->hTuner1 != NULL)
				pParams->hTuner1->Error = CHIPERR_I2C_NO_ACK;
			error = FE_LLA_I2C_ERROR;

		}
		break;

	}

	switch (pInit->Tuner2HWControlType) {
	case FE_SAT_SW_TUNER:
	default:
		if (pParams->hTuner2) {
			/* Set the tuner 2 reference clock */
			FE_Sat_TunerSetReferenceFreq(pParams->hTuner2,
							pInit->Tuner2RefClock);
			/*printf("\n IN 900_init.c the P2 Bbgain that will be
			 * set is %d", pInit->TunerBasebandGain); */
			/*Set the tuner base band gain */
			FE_Sat_TunerSetGain(pParams->hTuner2,
						pInit->Tuner2BasebandGain);
		}
		/*Set the ADC's range according to the tuner Model */
		switch (pInit->Tuner2Model) {
		case TUNER_STB6000:
		case TUNER_STB6100:
			/*Set the ADC range to 2Vpp */
			ChipSetOneRegister(pParams->hDemod, RSTV0900_TSTTNR3,
									0x27);
			break;

		case TUNER_STV6110:
		case TUNER_STV6130:
			/*Set the ADC range to 1Vpp */
			ChipSetOneRegister(pParams->hDemod, RSTV0900_TSTTNR3,
									0x26);
			break;

		default:
			break;

		}

		break;

	case FE_SAT_AUTO_STB6000:
	case FE_SAT_AUTO_STB6100:
	case FE_SAT_AUTO_STV6110:
		/*init the 900 Tuner controller */
		FE_STV0900_SetTunerType(pParams->hDemod,
				pInit->Tuner2HWControlType,
				pInit->Tuner2_I2cAddr,
				pInit->Tuner2RefClock,
				pInit->Tuner2OutClkDivider, FE_SAT_DEMOD_2);
		/*Set the BB gain using the 900 Tuner controller */
		FE_STV0900_TunerSetBBGain(pParams->hDemod,
				pInit->Tuner2HWControlType,
				pInit->Tuner2BasebandGain, FE_SAT_DEMOD_2);

		if (ChipGetField(pParams->hDemod, FSTV0900_P2_TUN_ACKFAIL)) {
			if (pParams->hTuner2 != NULL)
				pParams->hTuner2->Error = CHIPERR_I2C_NO_ACK;
			error = FE_LLA_I2C_ERROR;
		}
		break;

	}

	/*IQSWAP setting must be after FE_STV0900_P1_SetTunerType */
	ChipSetField(pParams->hDemod, FSTV0900_P1_TUN_IQSWAP,
						pInit->TunerIQ_Inversion);
	/*IQSWAP setting must be after FE_STV0900_P1_SetTunerType */
	ChipSetField(pParams->hDemod, FSTV0900_P2_TUN_IQSWAP,
						pInit->Tuner2IQ_Inversion);

	/*Set the Mclk value to 135MHz */
	FE_STV0900_SetMclk((FE_STV0900_Handle_t) pParams, 135000000,
							pParams->Quartz);
	/*wait for PLL lock */
	ChipWaitOrAbort(pParams->hDemod, 3);
	/* switch the 900 to the PLL */
	ChipSetOneRegister(pParams->hDemod, RSTV0900_SYNTCTRL, 0x22);

#endif

	/*Read the cuurent Mclk */
	pParams->MasterClock = FE_STV0900_GetMclkFreq(pParams->hDemod,
							pParams->Quartz);

	/*Check the error at the end of the init */
	if (pParams->hDemod->Error)
		error = FE_LLA_I2C_ERROR;
	else if (pParams->hTuner1 != NULL)
		if (pParams->hTuner1->Error)
			error = FE_LLA_I2C_ERROR;
	/* else if (pParams->hTuner2 !=NULL) {
	   if (pParams->hTuner2->Error)
	   error=FE_LLA_I2C_ERROR;
	   } */

	return error;
}

FE_STV0900_Error_t FE_STV0900_SetTSOutput(FE_STV0900_Handle_t Handle,
					  FE_STV0900_TS_Output Path1TS,
					  FE_STV0900_Clock_t Path1TSData,
					  FE_STV0900_TS_Output Path2TS,
					  FE_STV0900_Clock_t Path2TSData)
{

	FE_STV0900_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0900_InternalParams_t *pParams = NULL;

	pParams = (FE_STV0900_InternalParams_t *) Handle;
	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;
	if (pParams->hDemod->Error)
		return FE_LLA_I2C_ERROR;

	pParams->Path1TSoutpt = Path1TS;
	pParams->Path2TSoutpt = Path2TS;

	if (Path1TS == Path2TS)
		/*error path1 and path2 can not use the same TSout path */
		error = FE_LLA_BAD_PARAMETER;

	else if ((Path1TS == FE_STV0900_TS1)
		 && ((Path1TSData == FE_TS_PARALLEL_PUNCT_CLOCK)
		     || (Path1TSData == FE_TS_DVBCI_CLOCK)))
		/*error TS1 can not be parallel */
		error = FE_LLA_BAD_PARAMETER;

	else if ((Path1TS == FE_STV0900_TS2)
		 && ((Path1TSData == FE_TS_PARALLEL_PUNCT_CLOCK)
		     || (Path1TSData == FE_TS_DVBCI_CLOCK))) {
		if (pParams->hDemod->ChipID <= 0x20)
			error = FE_LLA_BAD_PARAMETER;
	} else if ((Path2TS == FE_STV0900_TS1)
		   && ((Path2TSData == FE_TS_PARALLEL_PUNCT_CLOCK)
		    || (Path2TSData == FE_TS_DVBCI_CLOCK)))
		/*error TS1 can not be parallel */
		error = FE_LLA_BAD_PARAMETER;

	else if ((Path2TS == FE_STV0900_TS2)
		 && ((Path2TSData == FE_TS_PARALLEL_PUNCT_CLOCK)
		     || (Path2TSData == FE_TS_DVBCI_CLOCK))) {
		if (pParams->hDemod->ChipID <= 0x20)
			error = FE_LLA_BAD_PARAMETER;
	}

	if (error != FE_LLA_NO_ERROR)
		return error;

	if ((Path1TS == FE_STV0900_MUXTS) && (Path2TS == FE_STV0900_MUXTS)) {
		if (pParams->hDemod->ChipID >= 0x30)
			/*Cut 3.0 TS setting */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_TSGENERAL, 0x46);
		else if (pParams->hDemod->ChipID >= 0x20)
			/*Cut 2.0 TS setting */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_TSGENERAL, 0x06);
		else
			/*Cut 1.x TS setting */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_TSGENERAL1X, 0x16);
		goto err_chk_ret;
	}

	switch (Path1TS) {
	case FE_STV0900_TS1:
		switch (Path2TS) {
		case FE_STV0900_TS2:
			if (pParams->hDemod->ChipID >= 0x30)
				/*Cut 3.0 TS setting */
				if ((Path2TSData == FE_TS_PARALLEL_PUNCT_CLOCK)
					  || (Path2TSData == FE_TS_DVBCI_CLOCK))
					ChipSetOneRegister(pParams->hDemod,
						      RSTV0900_TSGENERAL, 0x0C);
				else
					ChipSetOneRegister(pParams->hDemod,
						      RSTV0900_TSGENERAL, 0x4C);
			else if (pParams->hDemod->ChipID >= 0x20)
				/*Cut 2.0 TS setting */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_TSGENERAL, 0x0C);
			else
				/*Cut 1.x TS setting */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_TSGENERAL1X, 0x14);
			break;

		case FE_STV0900_TS3:
			error =
			    FE_LLA_BAD_PARAMETER;
			break;

		default:
			break;
		}

		break;

	case FE_STV0900_TS2:

		switch (Path2TS) {
		case FE_STV0900_TS1:
			error = FE_LLA_BAD_PARAMETER;
			break;

		case FE_STV0900_TS3:
			if (pParams->hDemod->ChipID >= 0x30)
				/*Cut 3.0 TS setting */
				if ((Path1TSData == FE_TS_PARALLEL_PUNCT_CLOCK)
					  || (Path1TSData == FE_TS_DVBCI_CLOCK))
					ChipSetOneRegister(pParams->hDemod,
						      RSTV0900_TSGENERAL, 0x0A);
				else
					ChipSetOneRegister(pParams->hDemod,
						      RSTV0900_TSGENERAL, 0x4A);
			else if (pParams->hDemod->ChipID >= 0x20)
				/*Cut 2.0 TS setting */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_TSGENERAL, 0x0A);
			else
				/*Cut 1.x TS setting */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_TSGENERAL1X, 0x12);
			break;

		default:
			break;
		}

		break;

	case FE_STV0900_TS3:

		switch (Path2TS) {
		case FE_STV0900_TS1:
			error = FE_LLA_BAD_PARAMETER;
			break;

		case FE_STV0900_TS2:
			if (pParams->hDemod->ChipID >= 0x30)
				/*Cut 3.0 TS setting */
				if ((Path2TSData == FE_TS_PARALLEL_PUNCT_CLOCK)
					  || (Path2TSData == FE_TS_DVBCI_CLOCK))
					ChipSetOneRegister(pParams->hDemod,
						      RSTV0900_TSGENERAL, 0x00);
				else
					ChipSetOneRegister(pParams->hDemod,
						      RSTV0900_TSGENERAL, 0x40);
			else if (pParams->hDemod->ChipID >= 0x20)
				/*Cut 2.0 TS setting */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_TSGENERAL, 0x00);
			else
				/*Cut 1.x TS setting */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_TSGENERAL1X, 0x10);

			break;

		default:
			break;
		}

		break;

	default:
		break;
	}

err_chk_ret:
	if (error != FE_LLA_NO_ERROR)
		return error;

	switch (Path1TSData) {
	case FE_TS_PARALLEL_PUNCT_CLOCK:
		ChipSetField(pParams->hDemod, FSTV0900_P1_TSFIFO_SERIAL, 0x00);
		ChipSetField(pParams->hDemod, FSTV0900_P1_TSFIFO_DVBCI, 0x00);
		break;

	case FE_TS_DVBCI_CLOCK:
		ChipSetField(pParams->hDemod, FSTV0900_P1_TSFIFO_SERIAL, 0x00);
		ChipSetField(pParams->hDemod, FSTV0900_P1_TSFIFO_DVBCI, 0x01);

		break;

	case FE_TS_SERIAL_PUNCT_CLOCK:
		ChipSetField(pParams->hDemod, FSTV0900_P1_TSFIFO_SERIAL, 0x01);
		ChipSetField(pParams->hDemod, FSTV0900_P1_TSFIFO_DVBCI, 0x00);
		break;

	case FE_TS_SERIAL_CONT_CLOCK:
		ChipSetField(pParams->hDemod, FSTV0900_P1_TSFIFO_SERIAL, 0x01);
		ChipSetField(pParams->hDemod, FSTV0900_P1_TSFIFO_DVBCI, 0x01);
		break;

	default:
		break;
	}

	switch (Path2TSData) {
	case FE_TS_PARALLEL_PUNCT_CLOCK:
		/* parallel mode   */
		ChipSetField(pParams->hDemod, FSTV0900_P2_TSFIFO_SERIAL, 0x00);
		ChipSetField(pParams->hDemod, FSTV0900_P2_TSFIFO_DVBCI, 0x00);
		break;

	case FE_TS_DVBCI_CLOCK:
		/* parallel mode   */
		ChipSetField(pParams->hDemod, FSTV0900_P2_TSFIFO_SERIAL, 0x00);
		ChipSetField(pParams->hDemod, FSTV0900_P2_TSFIFO_DVBCI, 0x01);
		break;

	case FE_TS_SERIAL_PUNCT_CLOCK:
		/* serial mode   */
		ChipSetField(pParams->hDemod, FSTV0900_P2_TSFIFO_SERIAL, 0x01);
		ChipSetField(pParams->hDemod, FSTV0900_P2_TSFIFO_DVBCI, 0x00);
		break;

	case FE_TS_SERIAL_CONT_CLOCK:
		/* serial mode   */
		ChipSetField(pParams->hDemod, FSTV0900_P2_TSFIFO_SERIAL, 0x01);
		ChipSetField(pParams->hDemod, FSTV0900_P2_TSFIFO_DVBCI, 0x01);
		break;

	default:
		break;
	}
	/*Reset stream merger */
	ChipSetField(pParams->hDemod, FSTV0900_P2_RST_HWARE, 1);
	ChipSetField(pParams->hDemod, FSTV0900_P2_RST_HWARE, 0);

	/*Reset stream merger */
	ChipSetField(pParams->hDemod, FSTV0900_P1_RST_HWARE, 1);
	ChipSetField(pParams->hDemod, FSTV0900_P1_RST_HWARE, 0);

	return error;
}

/*****************************************************
--FUNCTION	::	FE_STV0900_SetTSConfig
--ACTION	::	TS configuration
--PARAMS IN	::	Handle		==>	Front End Handle
			::	Path1pTSConfig	==> path1 TS config parameters
			::	Path2pTSConfig	==> path2 TS config parameters
--PARAMS OUT::	Path1TSSpeed_Hz	==> path1 Current TS speed in Hz.
			Path2TSSpeed_Hz	==> path2 Current TS speed in Hz.
--RETURN	::	Error (if any)
--***************************************************/
FE_STV0900_Error_t FE_STV0900_SetTSConfig(FE_STV0900_Handle_t Handle,
					  FE_STV0900_TSConfig_t *
					  Path1pTSConfig,
					  FE_STV0900_TSConfig_t *
					  Path2pTSConfig, U32 *Path1TSSpeed_Hz,
					  U32 *Path2TSSpeed_Hz)
{
	FE_STV0900_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0900_InternalParams_t *pParams = NULL;

	U32 tsspeed;

	pParams = (FE_STV0900_InternalParams_t *) Handle;
	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;

	if (pParams->hDemod->Error)
		return FE_LLA_I2C_ERROR;

	if (Path1pTSConfig->TSSyncByteEnable == FE_TS_SYNCBYTE_OFF)
		/* enable/Disable SYNC byte */
		ChipSetField(pParams->hDemod, FSTV0900_P1_TSDEL_SYNCBYTE, 1);
	else
		ChipSetField(pParams->hDemod, FSTV0900_P1_TSDEL_SYNCBYTE, 0);

	if (Path1pTSConfig->TSParityBytes == FE_TS_PARITYBYTES_ON)
		/*DVBS1 Data parity bytes enabling/disabling */
		ChipSetField(pParams->hDemod, FSTV0900_P1_TSINSDEL_RSPARITY, 1);
	else
		ChipSetField(pParams->hDemod, FSTV0900_P1_TSINSDEL_RSPARITY, 0);

	if (Path2pTSConfig->TSSyncByteEnable == FE_TS_SYNCBYTE_OFF)
		/* enable/Disable SYNC byte */
		ChipSetField(pParams->hDemod, FSTV0900_P2_TSDEL_SYNCBYTE, 1);
	else
		ChipSetField(pParams->hDemod, FSTV0900_P2_TSDEL_SYNCBYTE, 0);

	if (Path2pTSConfig->TSParityBytes == FE_TS_PARITYBYTES_ON)
		/*DVBS1 Data parity bytes enabling/disabling */
		ChipSetField(pParams->hDemod, FSTV0900_P2_TSINSDEL_RSPARITY, 1);
	else
		ChipSetField(pParams->hDemod, FSTV0900_P2_TSINSDEL_RSPARITY, 0);

	if (pParams->hDemod->ChipID <= 0x20) {
		/*Cut 2.0 and 1.x ==> Clock edge setting */
		/*Clock Edge setting for cut 1.x and 2.0 */

		if ((Path1pTSConfig->TSMode == FE_TS_SERIAL_PUNCT_CLOCK) ||
			(Path1pTSConfig->TSMode == FE_TS_SERIAL_CONT_CLOCK)) {
			/*if path1 is serial then :
			   if path2 is serial ==> path1 is on TS1 pins
			   if path2 is ==> path1 is on TS2 pins */
			if ((Path2pTSConfig->TSMode == FE_TS_SERIAL_PUNCT_CLOCK)
				|| (Path2pTSConfig->TSMode ==
					 FE_TS_SERIAL_CONT_CLOCK)) {
				if (Path1pTSConfig->TSClockPolarity ==
						FE_TS_RISINGEDGE_CLOCK)
					/*TS clock Polarity setting : rising
					 * edge/falling edge */
					ChipSetField(pParams->hDemod,
							FSTV0900_CLKOUT1_XOR,
							0);
				else
					ChipSetField(pParams->hDemod,
							FSTV0900_CLKOUT1_XOR,
							1);
			} else {
				if (Path1pTSConfig->TSClockPolarity ==
						FE_TS_RISINGEDGE_CLOCK)
					/*TS clock Polarity setting : rising
					 * edge/falling edge */
					ChipSetField(pParams->hDemod,
							FSTV0900_CLKOUT2_XOR,
							0);
				else
					ChipSetField(pParams->hDemod,
							FSTV0900_CLKOUT2_XOR,
							1);
			}
		} else {
			/*if path1 is then path1 is on TS3 pins */
			if (Path1pTSConfig->TSClockPolarity ==
					FE_TS_RISINGEDGE_CLOCK)
				/*TS clock Polarity setting : rising
				 * edge/falling edge */
				ChipSetField(pParams->hDemod,
						FSTV0900_CLKOUT3_XOR, 0);
			else
				ChipSetField(pParams->hDemod,
						FSTV0900_CLKOUT3_XOR, 1);
		}

		if ((Path2pTSConfig->TSMode == FE_TS_SERIAL_PUNCT_CLOCK) ||
			(Path2pTSConfig->TSMode == FE_TS_SERIAL_CONT_CLOCK)) {
			/*if path2 is serial then path2 is on TS2 pins */
			if (Path2pTSConfig->TSClockPolarity ==
					FE_TS_RISINGEDGE_CLOCK)
				/*TS clock Polarity setting : rising
				 * edge/falling edge */
				ChipSetField(pParams->hDemod,
						FSTV0900_CLKOUT2_XOR, 0);
			else
				ChipSetField(pParams->hDemod,
						FSTV0900_CLKOUT2_XOR, 1);
		} else {

			/*if path2 is then :
			   if path1 is serial  ==> path2 is on
			   TS3 pins if path1 is ==> path2 clock
			   pin is on TS2 pins */
			if ((Path1pTSConfig->TSMode == FE_TS_SERIAL_PUNCT_CLOCK)
					|| (Path1pTSConfig->TSMode ==
						FE_TS_SERIAL_CONT_CLOCK)) {
				if (Path2pTSConfig->TSClockPolarity ==
						FE_TS_RISINGEDGE_CLOCK)
					/*TS clock Polarity setting : rising
					 * edge/falling edge */
					ChipSetField(pParams->hDemod,
							FSTV0900_CLKOUT3_XOR,
							0);
				else
					ChipSetField(pParams->hDemod,
							FSTV0900_CLKOUT3_XOR,
							1);
			} else {
				/*path1 is (case of dual // : mux stream)) */
				if (Path2pTSConfig->TSClockPolarity ==
						FE_TS_RISINGEDGE_CLOCK)
					/*TS clock Polarity setting : rising
					 * edge/falling edge */
					ChipSetField(pParams->hDemod,
							FSTV0900_CLKOUT2_XOR,
							0);
				else
					ChipSetField(pParams->hDemod,
							FSTV0900_CLKOUT2_XOR,
							1);
			}
		}

	} else if (pParams->hDemod->ChipID >= 0x30) {
		/* cut 3.0 above Clock Edge setting */
		/*Clock Edge setting for cut 3.0 */

		/*for cut 3.0 path1 uses always TS3 pins */
		if (Path1pTSConfig->TSClockPolarity == FE_TS_RISINGEDGE_CLOCK)
			/*TS clock Polarity setting : rising edge/falling edge*/
			ChipSetField(pParams->hDemod, FSTV0900_CLKOUT3_XOR, 0);
		else
			ChipSetField(pParams->hDemod, FSTV0900_CLKOUT3_XOR, 1);

		/*for cut 3.0 path2 uses always TS2 pins */
		if (Path2pTSConfig->TSClockPolarity == FE_TS_RISINGEDGE_CLOCK)
			/*TS clock Polarity setting : rising edge/falling edge*/
			ChipSetField(pParams->hDemod, FSTV0900_CLKOUT2_XOR, 0);
		else
			ChipSetField(pParams->hDemod, FSTV0900_CLKOUT2_XOR, 1);
	}

	switch (pParams->Path1TSoutpt) {
	case FE_STV0900_TS1:
		if (Path1pTSConfig->TSClockPolarity == FE_TS_RISINGEDGE_CLOCK)
			/*TS clock Polarity setting : rising edge/falling edge*/
			ChipSetField(pParams->hDemod, FSTV0900_CLKOUT1_XOR, 0);
		else
			ChipSetField(pParams->hDemod, FSTV0900_CLKOUT1_XOR, 1);

		break;

	case FE_STV0900_TS2:
		if (Path1pTSConfig->TSClockPolarity == FE_TS_RISINGEDGE_CLOCK)
			/*TS clock Polarity setting : rising edge/falling edge*/
			ChipSetField(pParams->hDemod, FSTV0900_CLKOUT2_XOR, 0);
		else
			ChipSetField(pParams->hDemod, FSTV0900_CLKOUT2_XOR, 1);

		break;

	case FE_STV0900_TS3:
	case FE_STV0900_MUXTS:
		if (Path1pTSConfig->TSClockPolarity == FE_TS_RISINGEDGE_CLOCK)
			/*TS clock Polarity setting : rising edge/falling edge*/
			ChipSetField(pParams->hDemod, FSTV0900_CLKOUT3_XOR, 0);
		else
			ChipSetField(pParams->hDemod, FSTV0900_CLKOUT3_XOR, 1);
		break;

	default:
		break;
	}

	switch (pParams->Path2TSoutpt) {
	case FE_STV0900_TS2:
	case FE_STV0900_MUXTS:
		if (Path2pTSConfig->TSClockPolarity == FE_TS_RISINGEDGE_CLOCK)
			/*TS clock Polarity setting : rising edge/falling edge*/
			ChipSetField(pParams->hDemod, FSTV0900_CLKOUT2_XOR, 0);
		else
			ChipSetField(pParams->hDemod, FSTV0900_CLKOUT2_XOR, 1);

		break;

	case FE_STV0900_TS3:
		if (Path2pTSConfig->TSClockPolarity == FE_TS_RISINGEDGE_CLOCK)
			/*TS clock Polarity setting : rising edge/falling edge*/
			ChipSetField(pParams->hDemod, FSTV0900_CLKOUT3_XOR, 0);
		else
			ChipSetField(pParams->hDemod, FSTV0900_CLKOUT3_XOR, 1);
		break;

	case FE_STV0900_TS1:
	default:
		break;
	}
	if (Path1pTSConfig->TSSpeedControl == FE_TS_MANUAL_SPEED) {
		/*path 1 TS speed setting */
		ChipSetField(pParams->hDemod, FSTV0900_P1_TSFIFO_MANSPEED, 3);
		switch (Path1pTSConfig->TSMode) {
		case FE_TS_PARALLEL_PUNCT_CLOCK:
		case FE_TS_DVBCI_CLOCK:
			/*Formulat :
			   TS_Speed_Hz = 4 * Msterclock_Hz / TSSPEED_REG
			 */
			tsspeed = (4 * pParams->MasterClock) /
				Path1pTSConfig->TSClockRate;
			if (tsspeed <= 16)
				/*in parallel clock the TS speed is limited <
				 * MasterClock/4 */
				tsspeed = 16;

			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P1_TSSPEED, tsspeed);

			break;

		case FE_TS_SERIAL_PUNCT_CLOCK:
		case FE_TS_SERIAL_CONT_CLOCK:
		case FE_TS_OUTPUTMODE_DEFAULT:
		default:

			/*Formulat :
			   TS_Speed_Hz = 32 * Msterclock_Hz / TSSPEED_REG
			 */
			tsspeed =
			    (16 * pParams->MasterClock) /
			    (Path1pTSConfig->TSClockRate / 2);
			if (tsspeed <= 32)
				/*in serial clock the TS speed is
				 * limited <= MasterClock */
				tsspeed = 32;

			ChipSetOneRegister(pParams->hDemod,
					   RSTV0900_P1_TSSPEED,
					   tsspeed);
			break;
		}

	} else {
		if (Path1pTSConfig->TSMode == FE_TS_DVBCI_CLOCK) {

			ChipSetField(pParams->hDemod,
				     FSTV0900_P1_TSFIFO_MANSPEED,
				     1);
			/*Formulat :
			   TS_Speed_Hz = 32 * Msterclock_Hz /
			   TSSPEED_REG
			 */
			/*if DVBCI set TS clock to 9MHz */
			tsspeed = (16 * pParams->MasterClock) / 9000000;
			if (tsspeed <= 32)
				/*in serial clock the TS speed
				 * is limited <= MasterClock */
				tsspeed = 32;

			ChipSetOneRegister(pParams->hDemod,
					   RSTV0900_P1_TSSPEED,
					   tsspeed);

		} else
			ChipSetField(pParams->hDemod,
				     FSTV0900_P1_TSFIFO_MANSPEED,
				     0);
	}

	if (Path2pTSConfig->TSSpeedControl ==
	    FE_TS_MANUAL_SPEED) {
		/*path 2 TS speed setting */
		ChipSetField(pParams->hDemod,
			     FSTV0900_P2_TSFIFO_MANSPEED, 3);
		switch (Path2pTSConfig->TSMode) {
		case FE_TS_PARALLEL_PUNCT_CLOCK:
		case FE_TS_DVBCI_CLOCK:
			/*Formulat :
			   TS_Speed_Hz = 4 * Msterclock_Hz / TSSPEED_REG
			 */
			tsspeed =
			    (4 * pParams->MasterClock) /
			    Path2pTSConfig->TSClockRate;
			if (tsspeed <= 16)
				/*in parallel clock the TS speed is
				 * limited < MasterClock/4 */
				tsspeed = 16;

			ChipSetOneRegister(pParams->hDemod,
					   RSTV0900_P2_TSSPEED,
					   tsspeed);

			break;

		case FE_TS_SERIAL_PUNCT_CLOCK:
		case FE_TS_SERIAL_CONT_CLOCK:
		case FE_TS_OUTPUTMODE_DEFAULT:
		default:

			/*Formulat :
			   TS_Speed_Hz = 32 * Msterclock_Hz / TSSPEED_REG
			 */
			tsspeed =
			    (16 * pParams->MasterClock) /
			    (Path2pTSConfig->TSClockRate / 2);
			if (tsspeed <= 32)
				/*in serial clock the TS speed is
				 * limited <= MasterClock */
				tsspeed = 32;

			ChipSetOneRegister(pParams->hDemod,
					   RSTV0900_P2_TSSPEED,
					   tsspeed);
			break;
		}

	} else {
		if (Path2pTSConfig->TSMode == FE_TS_DVBCI_CLOCK) {

			ChipSetField(pParams->hDemod,
				     FSTV0900_P2_TSFIFO_MANSPEED,
				     1);
			/*Formulat :
			   TS_Speed_Hz = 32 * Msterclock_Hz / TSSPEED_REG
			 */
			/*if DVBCI set TS clock to 9MHz */
			tsspeed = (16 * pParams->MasterClock) / 9000000;
			if (tsspeed <= 32)
				/*in serial clock the TS speed is
				 * limited <= MasterClock */
				tsspeed = 32;

			ChipSetOneRegister(pParams->hDemod,
					   RSTV0900_P2_TSSPEED,
					   tsspeed);

		} else
			ChipSetField(pParams->hDemod,
				     FSTV0900_P2_TSFIFO_MANSPEED,
				     0);
	}

	switch (Path1pTSConfig->TSMode) {
	/*D7/D0 permute if serial */
	case FE_TS_SERIAL_PUNCT_CLOCK:
	case FE_TS_SERIAL_CONT_CLOCK:
	case FE_TS_OUTPUTMODE_DEFAULT:
	default:
		if (Path1pTSConfig->TSSwap == FE_TS_SWAP_ON)
			ChipSetField(pParams->hDemod,
				     FSTV0900_P1_TSFIFO_PERMDATA,
				     1);
		else
			ChipSetField(pParams->hDemod,
				     FSTV0900_P1_TSFIFO_PERMDATA,
				     0);
		break;

	case FE_TS_PARALLEL_PUNCT_CLOCK:
	case FE_TS_DVBCI_CLOCK:
		ChipSetField(pParams->hDemod,
			     FSTV0900_P1_TSFIFO_PERMDATA, 0);
		break;
	}

	switch (Path2pTSConfig->TSMode) {
	/*D7/D0 permute if serial */
	case FE_TS_SERIAL_PUNCT_CLOCK:
	case FE_TS_SERIAL_CONT_CLOCK:
	case FE_TS_OUTPUTMODE_DEFAULT:
	default:
		if (Path2pTSConfig->TSSwap == FE_TS_SWAP_ON)
			ChipSetField(pParams->hDemod,
				     FSTV0900_P2_TSFIFO_PERMDATA,
				     1);
		else
			ChipSetField(pParams->hDemod,
				     FSTV0900_P2_TSFIFO_PERMDATA,
				     0);
		break;

	case FE_TS_PARALLEL_PUNCT_CLOCK:
	case FE_TS_DVBCI_CLOCK:
		ChipSetField(pParams->hDemod,
			     FSTV0900_P2_TSFIFO_PERMDATA, 0);
		break;
	}

	if (ChipGetField(pParams->hDemod, FSTV0900_P1_TSFIFO_SERIAL) == 0)
		/*Parallel mode */
		*Path1TSSpeed_Hz =
		    (4 * pParams->MasterClock) /
		    ChipGetOneRegister(pParams->hDemod,
				       RSTV0900_P1_TSSPEED);
	else {	/*serial mode */

		*Path1TSSpeed_Hz =
		    (16 * pParams->MasterClock) /
		    ChipGetOneRegister(pParams->hDemod,
				       RSTV0900_P1_TSSPEED);
		(*Path1TSSpeed_Hz) *= 2;
	}

	if (ChipGetField(pParams->hDemod, FSTV0900_P2_TSFIFO_SERIAL) == 0)
		/*Parallel mode */
		*Path2TSSpeed_Hz =
		    (4 * pParams->MasterClock) /
		    ChipGetOneRegister(pParams->hDemod,
				       RSTV0900_P2_TSSPEED);
	else {	/*serial mode */

		*Path2TSSpeed_Hz =
		    (16 * pParams->MasterClock) /
		    ChipGetOneRegister(pParams->hDemod,
				       RSTV0900_P2_TSSPEED);
		(*Path2TSSpeed_Hz) *= 2;
	}

	if (pParams->hDemod->Error)
		/*Check the error at the end of the function */
		error = FE_LLA_I2C_ERROR;

	return error;
}

/*****************************************************
--FUNCTION	::	FE_STV0900_GetSignalInfo
--ACTION	::	Return informations on the locked transponder
--PARAMS IN	::	Handle	==>	Front End Handle
			::	Demod	==>	Cuurent demod 1 or 2
--PARAMS OUT::	pInfo	==> Informations (BER, C/N, power ...)
--RETURN	::	Error (if any)
--***************************************************/
FE_STV0900_Error_t FE_STV0900_GetSignalInfo(FE_STV0900_Handle_t Handle,
					    FE_STV0900_DEMOD_t Demod,
					    FE_STV0900_SignalInfo_t *pInfo)
{
	FE_STV0900_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0900_InternalParams_t *pParams = NULL;
	FE_STV0900_SEARCHSTATE_t demodState;

	S32 symbolRateOffset;

	pParams = (FE_STV0900_InternalParams_t *) Handle;

	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		if ((pParams->hDemod->Error) || (pParams->hTuner1->Error))
			return FE_LLA_I2C_ERROR;

		demodState = ChipGetField(pParams->hDemod,
						FSTV0900_P1_HEADER_MODE);
		switch (demodState) {
		case FE_SAT_SEARCH:
		case FE_SAT_PLH_DETECTED:
		default:
			pInfo->Locked = FALSE;
			break;
		case FE_SAT_DVBS2_FOUND:
			pInfo->Locked = ChipGetField(pParams->hDemod,
						FSTV0900_P1_LOCK_DEFINITIF)
					&& ChipGetField(pParams->hDemod,
						FSTV0900_P1_PKTDELIN_LOCK)
					&& ChipGetField(pParams->hDemod,
						FSTV0900_P1_TSFIFO_LINEOK);
			break;

		case FE_SAT_DVBS_FOUND:
			pInfo->Locked = ChipGetField(pParams->hDemod,
						FSTV0900_P1_LOCK_DEFINITIF)
					&& ChipGetField(pParams->hDemod,
							FSTV0900_P1_LOCKEDVIT)
					&& ChipGetField(pParams->hDemod,
						FSTV0900_P1_TSFIFO_LINEOK);
			break;
		}

		/* transponder_frequency = tuner +  demod carrier frequency */
		pInfo->Frequency = FE_STV0900_GetTunerFrequency
			(pParams->hDemod, pParams->hTuner1,
			 pParams->Tuner1Type, Demod);
		pInfo->Frequency += FE_STV0900_GetCarrierFrequency
			(pParams->hDemod, pParams->MasterClock, Demod) / 1000;

		/* Get symbol rate */
		pInfo->SymbolRate = FE_STV0900_GetSymbolRate(pParams->hDemod,
				pParams->MasterClock, Demod);
		symbolRateOffset = FE_STV0900_TimingGetOffset(pParams->hDemod,
				pInfo->SymbolRate, Demod);
		/* Get timing loop offset */
		pInfo->SymbolRate += symbolRateOffset;

		pInfo->Standard = FE_STV0900_GetStandard(pParams->hDemod,
				Demod);

		pInfo->PunctureRate = FE_STV0900_GetViterbiPunctureRate
			(pParams->hDemod, Demod);
		pInfo->ModCode = ChipGetField(pParams->hDemod,
				FSTV0900_P1_DEMOD_MODCOD);
		pInfo->Pilots = ChipGetFieldImage(pParams->hDemod,
				FSTV0900_P1_DEMOD_TYPE) & 0x01;
		pInfo->FrameLength = ((U32)ChipGetFieldImage(pParams->hDemod,
					FSTV0900_P1_DEMOD_TYPE)) >> 1;
		pInfo->RollOff = ChipGetField(pParams->hDemod,
				FSTV0900_P1_ROLLOFF_STATUS);

		if (pParams->BER_Algo !=
		    FE_BER_Algo_Appli_Berwindow)
			pInfo->BER = FE_STV0900_GetBer(pParams->hDemod, Demod);

		pInfo->Power = FE_STV0900_GetRFLevelWithVGLNA(pParams, Demod);
		if (pInfo->Standard == FE_SAT_DVBS2_STANDARD) {
			pInfo->C_N = FE_STV0900_CarrierGetQuality
				(pParams->hDemod, &FE_STV0900_S2_CN_LookUp,
				 Demod);

			pInfo->Spectrum = ChipGetField(pParams->hDemod,
					FSTV0900_P1_SPECINV_DEMOD);

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
			if (pParams->BER_Algo != FE_BER_Algo_Appli_Berwindow)
				/* reset the error counter to DVBS2 packet
				 * error rate, Average window mode */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P1_ERRCTRL1, 0x67);
			else
				/* reset the error counter to DVBS2 packet
				 * error rate, Infinite wwindow mode */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P1_ERRCTRL1, 0x61);
			/*DVBS1/DSS */
		} else {

			pInfo->C_N = FE_STV0900_CarrierGetQuality
				(pParams->hDemod, &FE_STV0900_S1_CN_LookUp,
				 Demod);

			pInfo->Spectrum = ChipGetField(pParams->hDemod,
							FSTV0900_P1_IQINV);

			pInfo->Modulation = FE_SAT_MOD_QPSK;
		}

		if ((pParams->hDemod->Error) || (pParams->hTuner1->Error))
			error = FE_LLA_I2C_ERROR;

		break;

	case FE_SAT_DEMOD_2:

		if ((pParams->hDemod->Error) || (pParams->hTuner2->Error))
			return FE_LLA_I2C_ERROR;

		demodState = ChipGetField(pParams->hDemod,
				FSTV0900_P2_HEADER_MODE);
		switch (demodState) {
		case FE_SAT_SEARCH:
		case FE_SAT_PLH_DETECTED:
		default:
			pInfo->Locked = FALSE;
			break;
		case FE_SAT_DVBS2_FOUND:
			pInfo->Locked = ChipGetField(pParams->hDemod,
					 FSTV0900_P2_LOCK_DEFINITIF)
					&& ChipGetField(pParams->hDemod,
					    FSTV0900_P2_PKTDELIN_LOCK)
					&& ChipGetField(pParams->hDemod,
					    FSTV0900_P2_TSFIFO_LINEOK);
			break;

		case FE_SAT_DVBS_FOUND:
			pInfo->Locked = ChipGetField(pParams->hDemod,
					 FSTV0900_P2_LOCK_DEFINITIF)
					&& ChipGetField(pParams->hDemod,
					    FSTV0900_P2_LOCKEDVIT)
					&& ChipGetField(pParams->hDemod,
					    FSTV0900_P2_TSFIFO_LINEOK);
			break;
		}

		/* transponder_frequency = tuner +  demod carrier frequency */
		pInfo->Frequency = FE_STV0900_GetTunerFrequency
			(pParams->hDemod, pParams->hTuner2,
			 pParams->Tuner2Type, Demod);
		pInfo->Frequency += FE_STV0900_GetCarrierFrequency
			(pParams->hDemod, pParams->MasterClock, Demod) / 1000;

		/* Get symbol rate */
		pInfo->SymbolRate = FE_STV0900_GetSymbolRate(pParams->hDemod,
				pParams->MasterClock, Demod);
		symbolRateOffset = FE_STV0900_TimingGetOffset(pParams->hDemod,
				pInfo->SymbolRate, Demod);
		/* Get timing loop offset */
		pInfo->SymbolRate += symbolRateOffset;

		pInfo->Standard = FE_STV0900_GetStandard(pParams->hDemod,
				Demod);

		pInfo->PunctureRate = FE_STV0900_GetViterbiPunctureRate
			(pParams->hDemod, Demod);
		pInfo->ModCode = ChipGetField(pParams->hDemod,
				FSTV0900_P2_DEMOD_MODCOD);
		pInfo->Pilots = ChipGetFieldImage(pParams->hDemod,
				FSTV0900_P2_DEMOD_TYPE) & 0x01;
		pInfo->FrameLength = ((U32)ChipGetFieldImage(pParams->hDemod,
					FSTV0900_P2_DEMOD_TYPE)) >> 1;
		pInfo->RollOff = ChipGetField(pParams->hDemod,
				FSTV0900_P2_ROLLOFF_STATUS);

		if (pParams->BER_Algo != FE_BER_Algo_Appli_Berwindow)
			pInfo->BER = FE_STV0900_GetBer(pParams->hDemod, Demod);

		pInfo->Power = FE_STV0900_GetRFLevelWithVGLNA(pParams, Demod);

		if (pInfo->Standard == FE_SAT_DVBS2_STANDARD) {
			pInfo->C_N = FE_STV0900_CarrierGetQuality
				(pParams->hDemod, &FE_STV0900_S2_CN_LookUp,
				 Demod);

			pInfo->Spectrum = ChipGetField(pParams->hDemod,
					FSTV0900_P2_SPECINV_DEMOD);

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
			if (pParams->BER_Algo != FE_BER_Algo_Appli_Berwindow)
				/* reset the error counter to DVBS2 packet
				 * error rate, Average window mode */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P2_ERRCTRL1, 0x67);
			else
				/* reset the error counter to DVBS2 packet
				 * error rate, infinite window mode */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P2_ERRCTRL1, 0x61);
		} else {
			/*DVBS1/DSS */

			pInfo->C_N = FE_STV0900_CarrierGetQuality
				(pParams->hDemod, &FE_STV0900_S1_CN_LookUp,
				 Demod);

			pInfo->Spectrum = ChipGetField(pParams->hDemod,
					FSTV0900_P2_IQINV);

			pInfo->Modulation = FE_SAT_MOD_QPSK;
		}

		if ((pParams->hDemod->Error) || (pParams->hTuner2->Error))
			error = FE_LLA_I2C_ERROR;

		break;
	}

	return error;
}

/*****************************************************
--FUNCTION	::	FE_STV0900_Tracking
--ACTION	::	Return Tracking informations : lock status, RF level,
			C/N and BER.
--PARAMS IN	::	Handle	==>	Front End Handle
			::	Demod	==>	Cuurent demod 1 or 2
--PARAMS OUT::	pTrackingInfo	==> pointer to FE_Sat_TrackingInfo_t struct.
			::
--RETURN	::	Error (if any)
--***************************************************/
FE_STV0900_Error_t FE_STV0900_Tracking(FE_STV0900_Handle_t Handle,
				       FE_STV0900_DEMOD_t Demod,
				       FE_Sat_TrackingInfo_t *pTrackingInfo)
{
	FE_STV0900_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0900_InternalParams_t *pParams = NULL;
	FE_STV0900_SEARCHSTATE_t demodState;

	pParams = (FE_STV0900_InternalParams_t *) Handle;

	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:

		if ((pParams->hDemod->Error) || (pParams->hTuner1->Error))
			return FE_LLA_I2C_ERROR;

		pTrackingInfo->Power = FE_STV0900_GetRFLevelWithVGLNA(pParams,
									Demod);

		if (pParams->BER_Algo != FE_BER_Algo_Appli_Berwindow)
			pTrackingInfo->BER = FE_STV0900_GetBer(pParams->hDemod,
									Demod);

		demodState = ChipGetField(pParams->hDemod,
						FSTV0900_P1_HEADER_MODE);
		switch (demodState) {
		case FE_SAT_SEARCH:
		case FE_SAT_PLH_DETECTED:
		default:
			pTrackingInfo->Locked = FALSE;
			break;
		case FE_SAT_DVBS2_FOUND:
			pTrackingInfo->Locked = ChipGetField(pParams->hDemod,
						     FSTV0900_P1_LOCK_DEFINITIF)
						&& ChipGetField(pParams->hDemod,
						      FSTV0900_P1_PKTDELIN_LOCK)
						&& ChipGetField(pParams->hDemod,
						     FSTV0900_P1_TSFIFO_LINEOK);

			pTrackingInfo->C_N = FE_STV0900_CarrierGetQuality
				(pParams->hDemod, &FE_STV0900_S2_CN_LookUp,
				 Demod);
			/*reset The error counter to PER */
			if (pParams->BER_Algo != FE_BER_Algo_Appli_Berwindow)
				/* reset the error counter to  DVBS2 packet
				 * error rate, Average BERwindow */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P1_ERRCTRL1, 0x67);
			else
				/* reset the error counter to  DVBS2 packet
				 * error rate, Infinite berwindow mode */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P1_ERRCTRL1, 0x61);

			break;

		case FE_SAT_DVBS_FOUND:
			pTrackingInfo->Locked = ChipGetField(pParams->hDemod,
						     FSTV0900_P1_LOCK_DEFINITIF)
						&& ChipGetField(pParams->hDemod,
							  FSTV0900_P1_LOCKEDVIT)
						&& ChipGetField(pParams->hDemod,
						     FSTV0900_P1_TSFIFO_LINEOK);

			pTrackingInfo->C_N = FE_STV0900_CarrierGetQuality
				(pParams->hDemod, &FE_STV0900_S1_CN_LookUp,
				 Demod);
			break;
		}

		pTrackingInfo->Frequency_IF =
			FE_STV0900_GetTunerFrequency(pParams->hDemod,
				pParams->hTuner1, pParams->Tuner1Type, Demod);
		pTrackingInfo->Frequency_IF +=
			FE_STV0900_GetCarrierFrequency(pParams->hDemod,
					pParams->MasterClock, Demod) / 1000;

		if ((pParams->hDemod->Error) || (pParams->hTuner1->Error))
			error = FE_LLA_I2C_ERROR;

		break;

	case FE_SAT_DEMOD_2:

		if ((pParams->hDemod->Error) || (pParams->hTuner2->Error))
			return FE_LLA_I2C_ERROR;

		pTrackingInfo->Power = FE_STV0900_GetRFLevelWithVGLNA(pParams,
									Demod);

		if (pParams->BER_Algo != FE_BER_Algo_Appli_Berwindow)
			pTrackingInfo->BER = FE_STV0900_GetBer(pParams->hDemod,
									Demod);

		demodState = ChipGetField(pParams->hDemod,
						FSTV0900_P2_HEADER_MODE);
		switch (demodState) {
		case FE_SAT_SEARCH:
		case FE_SAT_PLH_DETECTED:
		default:
			pTrackingInfo->Locked = FALSE;
			break;
		case FE_SAT_DVBS2_FOUND:
			pTrackingInfo->Locked = ChipGetField(pParams->hDemod,
						     FSTV0900_P2_LOCK_DEFINITIF)
						&& ChipGetField(pParams->hDemod,
						      FSTV0900_P2_PKTDELIN_LOCK)
						&& ChipGetField(pParams->hDemod,
						     FSTV0900_P2_TSFIFO_LINEOK);

			pTrackingInfo->C_N = FE_STV0900_CarrierGetQuality
				(pParams->hDemod, &FE_STV0900_S2_CN_LookUp,
				 Demod);

			/*reset The error counter to PER */
			if (pParams->BER_Algo != FE_BER_Algo_Appli_Berwindow)
				/* reset the error counter to  DVBS2 packet
				 * error rate, Average window mode */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P2_ERRCTRL1, 0x67);
			else
				/* reset the error counter to  DVBS2 packet
				 * error rate, Infinite window mode */
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P2_ERRCTRL1, 0x61);

			break;

		case FE_SAT_DVBS_FOUND:
			pTrackingInfo->Locked = ChipGetField(pParams->hDemod,
						     FSTV0900_P2_LOCK_DEFINITIF)
						&& ChipGetField(pParams->hDemod,
							  FSTV0900_P2_LOCKEDVIT)
						&& ChipGetField(pParams->hDemod,
						     FSTV0900_P2_TSFIFO_LINEOK);

			pTrackingInfo->C_N = FE_STV0900_CarrierGetQuality
				(pParams->hDemod, &FE_STV0900_S1_CN_LookUp,
				 Demod);
			break;
		}

		pTrackingInfo->Frequency_IF =
			FE_STV0900_GetTunerFrequency(pParams->hDemod,
				pParams->hTuner2, pParams->Tuner2Type, Demod);
		pTrackingInfo->Frequency_IF +=
			FE_STV0900_GetCarrierFrequency(pParams->hDemod,
					pParams->MasterClock, Demod) / 1000;

		if ((pParams->hDemod->Error) || (pParams->hTuner2->Error))
			error = FE_LLA_I2C_ERROR;
		break;
	}

	return error;
}

/*****************************************************
--FUNCTION	::	FE_STV900_GetPacketErrorRate
--ACTION	::	Return informations the number of error packet and the
			window packet count
--PARAMS IN	::	Handle	==>	Front End Handle
		::	Demod	==>	Cuurent demod 1 or 2
--PARAMS OUT::	PacketsErrorCount==> Number of packet error, max is 2^23 packet.
		::  TotalPacketsCount==> total window packets, max is 2^24 pkt.
--RETURN	::	Error (if any)
--***************************************************/
FE_STV0900_Error_t FE_STV900_GetPacketErrorRate(FE_STV0900_Handle_t Handle,
						FE_STV0900_DEMOD_t Demod,
						U32 *PacketsErrorCount,
						U32 *TotalPacketsCount)
{
	FE_STV0900_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0900_InternalParams_t *pParams = NULL;

	U8 packetsCount4 = 0,
	    packetsCount3 = 0,
	    packetsCount2 = 0, packetsCount1 = 0, packetsCount0 = 0;

	S32 packetsCountReg, errorCntrlReg2, berResetField;

	pParams = (FE_STV0900_InternalParams_t *) Handle;

	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		packetsCountReg = RSTV0900_P1_FBERCPT4;
		errorCntrlReg2 = RSTV0900_P1_ERRCTRL2;
		berResetField = FSTV0900_P1_BERMETER_RESET;
		if ((pParams->hDemod->Error) || (pParams->hTuner1->Error))
			error = FE_LLA_I2C_ERROR;

		break;

	case FE_SAT_DEMOD_2:
		packetsCountReg = RSTV0900_P2_FBERCPT4;
		errorCntrlReg2 = RSTV0900_P2_ERRCTRL2;
		berResetField = FSTV0900_P2_BERMETER_RESET;

		if ((pParams->hDemod->Error) || (pParams->hTuner2->Error))
			error = FE_LLA_I2C_ERROR;

		break;

	}
	if (error != FE_LLA_NO_ERROR)
		return error;

	if (FE_STV0900_Status(Handle, Demod) == FALSE) {
		/*if Demod+FEC not locked */
		/*Packet error count is set to the maximum
		 * value */
		*PacketsErrorCount = 1 << 23;
		/*Total Packet count is set to the maximum
		 * value */
		*TotalPacketsCount = 1 << 24;
	} else {
		/*Read the error counter 2 (23 bits) */
		*PacketsErrorCount = FE_STV0900_GetErrorCount(pParams->hDemod,
					FE_STV0900_COUNTER2, Demod) & 0x7FFFFF;

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

		if ((packetsCount4 == 0) && (packetsCount3 == 0)) {
			/*Use the counter for a maximum of 2^24 packets */
			*TotalPacketsCount = ((packetsCount2 & 0xFF) << 16) +
						((packetsCount1 & 0xFF) << 8) +
						(packetsCount0 & 0xFF);
		} else
			*TotalPacketsCount = 1 << 24;

		if (*TotalPacketsCount == 0) {
			/*if the packets count doesn't start yet the packet
			 * error =1 and total packets =1 */
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
	/*Reset the packet Error counter2 (and Set it to infinit error count
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

FE_STV0900_Error_t FE_STV900_GetPreViterbiBER(FE_STV0900_Handle_t Handle,
					      FE_STV0900_DEMOD_t Demod,
					      U32 *PreVirebiBER)
{
	/* warning, when using this function the error counter number 1 is set
	 * to pre-Viterbi BER, the post viterbi BER info (given in
	 * FE_STV0900_GetSignalInfo function or FE_STV0900_Tracking is not
	 * significant any more
	 */

	FE_STV0900_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0900_InternalParams_t *pParams = NULL;

	U32 errorCount = 10000000, frameSize;
	FE_STV0900_Rate_t punctureRate;

	BOOL lockedVit = FALSE;

	pParams = (FE_STV0900_InternalParams_t *) Handle;

	if (pParams == NULL) {
		*PreVirebiBER = errorCount;
		return FE_LLA_INVALID_HANDLE;
	}
	if (pParams->hDemod->Error) {
		*PreVirebiBER = errorCount;
		return FE_LLA_I2C_ERROR;
	}
	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		/* check for VITERBI lock */
		lockedVit = ChipGetField(pParams->hDemod,
				FSTV0900_P1_LOCKEDVIT);
		break;

	case FE_SAT_DEMOD_2:
		/* check for VITERBI lock */
		lockedVit = ChipGetField(pParams->hDemod,
				FSTV0900_P2_LOCKEDVIT);
		break;
	}
	if (lockedVit) {
		/* check for VITERBI lock */
		switch (Demod) {
		case FE_SAT_DEMOD_1:
		default:
			/* Set and reset the error counter1 to Pre-Viterbi BER
			 * with observation window =2^6 frames */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P1_ERRCTRL1, 0x13);
			/* wait for error accumulation */
			WAIT_N_MS(50);

			/* Read the puncture rate */
			punctureRate =
			      FE_STV0900_GetViterbiPunctureRate(pParams->hDemod,
						Demod);

			/* Read the error counter */
			errorCount = ((ChipGetOneRegister(pParams->hDemod,
					RSTV0900_P1_ERRCNT12) &	0x7F) << 16) +
				((ChipGetOneRegister(pParams->hDemod,
				      RSTV0900_P1_ERRCNT11) & 0xFF) << 8) +
				((ChipGetOneRegister(pParams->hDemod,
					   RSTV0900_P1_ERRCNT10) & 0xFF));
			break;

		case FE_SAT_DEMOD_2:
			/* Set and reset the error counter1 to Pre-Viterbi BER
			 * with observation window =2^6 frames */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P2_ERRCTRL1, 0x13);
			/* wait for error accumulation */
			WAIT_N_MS(50);

			/* Read the puncture rate */
			punctureRate =
			      FE_STV0900_GetViterbiPunctureRate(pParams->hDemod,
						Demod);

			/* Read the error counter */
			errorCount =
			    ((ChipGetOneRegister(pParams->hDemod,
					RSTV0900_P2_ERRCNT12) & 0x7F) << 16) +
			    ((ChipGetOneRegister(pParams->hDemod,
					RSTV0900_P2_ERRCNT11) & 0xFF) << 8) +
			    ((ChipGetOneRegister(pParams->hDemod,
					RSTV0900_P2_ERRCNT10) & 0xFF));
			break;
		}

		switch (punctureRate) {
			/*
			   compute the frame size frame size = 2688*2*PR;
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
			/* if PR is unknown BER=1 */
			errorCount = 10000000;
		}
	} else
		/*if VITERBI is not locked BER=1 */
		errorCount = 10000000;

	if (pParams->hDemod->Error)
		error = FE_LLA_I2C_ERROR;

	*PreVirebiBER = errorCount;

	return error;
}

/*****************************************************
--FUNCTION	::	FE_STV0900_Status
--ACTION	::	Return locked status
--PARAMS IN	::	Handle	==>	Front End Handle
			::	Demod	==>	Current demod 1 or 2
--PARAMS OUT::	NONE
--RETURN	::	Bool (locked or not)
--***************************************************/
BOOL FE_STV0900_Status(FE_STV0900_Handle_t Handle, FE_STV0900_DEMOD_t Demod)
{

	FE_STV0900_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0900_InternalParams_t *pParams = NULL;
	BOOL Locked = FALSE;
	FE_STV0900_SEARCHSTATE_t demodState;

	pParams = (FE_STV0900_InternalParams_t *)Handle;

	if (pParams == NULL)
		return FALSE;

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		if ((pParams->hDemod->Error) || (pParams->hTuner1->Error))
			return FALSE;

		demodState = ChipGetField(pParams->hDemod,
						FSTV0900_P1_HEADER_MODE);
		switch (demodState) {

		case FE_SAT_SEARCH:
		case FE_SAT_PLH_DETECTED:
		default:
			Locked = FALSE;
			break;
		case FE_SAT_DVBS2_FOUND:
			Locked = ChipGetField(pParams->hDemod,
					FSTV0900_P1_LOCK_DEFINITIF) &&
				ChipGetField(pParams->hDemod,
						FSTV0900_P1_PKTDELIN_LOCK) &&
				ChipGetField(pParams->hDemod,
						FSTV0900_P1_TSFIFO_LINEOK);
			break;

		case FE_SAT_DVBS_FOUND:
			Locked = ChipGetField(pParams->hDemod,
					FSTV0900_P1_LOCK_DEFINITIF) &&
				ChipGetField(pParams->hDemod,
						FSTV0900_P1_LOCKEDVIT) &&
				ChipGetField(pParams->hDemod,
						FSTV0900_P1_TSFIFO_LINEOK);
			break;
		}
		if ((pParams->hDemod->Error) || (pParams->hTuner1->Error))
			/*Check the error at the end of the function */
			error = FE_LLA_I2C_ERROR;
		break;

	case FE_SAT_DEMOD_2:

		if ((pParams->hDemod->Error) || (pParams->hTuner2->Error))
			return FALSE;

		demodState = ChipGetField(pParams->hDemod,
						FSTV0900_P2_HEADER_MODE);
		switch (demodState) {

		case FE_SAT_SEARCH:
		case FE_SAT_PLH_DETECTED:
		default:
			Locked = FALSE;
			break;
		case FE_SAT_DVBS2_FOUND:
			Locked = ChipGetField(pParams->hDemod,
					FSTV0900_P2_LOCK_DEFINITIF) &&
				ChipGetField(pParams->hDemod,
						FSTV0900_P2_PKTDELIN_LOCK) &&
				ChipGetField(pParams->hDemod,
						FSTV0900_P2_TSFIFO_LINEOK);
			break;

		case FE_SAT_DVBS_FOUND:
			Locked = ChipGetField(pParams->hDemod,
					FSTV0900_P2_LOCK_DEFINITIF) &&
				ChipGetField(pParams->hDemod,
						FSTV0900_P2_LOCKEDVIT) &&
				ChipGetField(pParams->hDemod,
						FSTV0900_P2_TSFIFO_LINEOK);
			break;
		}
		if ((pParams->hDemod->Error) || (pParams->hTuner2->Error))
			/*Check the error at the end of the function */
			error = FE_LLA_I2C_ERROR;

		break;
	}

	return Locked;

}

/*****************************************************
--FUNCTION	::	FE_STV0900_Unlock
--ACTION	::	Unlock the demodulator , set the demod to idle state
--PARAMS IN	::	Handle	==>	Front End Handle
			::	Demod	==>	Current demod 1 or 2
-PARAMS OUT::	NONE.
--RETURN	::	Error (if any)
--***************************************************/
FE_STV0900_Error_t FE_STV0900_Unlock(FE_STV0900_Handle_t Handle,
				     FE_STV0900_DEMOD_t Demod)
{
	FE_STV0900_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0900_InternalParams_t *pParams =
	    (FE_STV0900_InternalParams_t *) Handle;

	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;

	switch (Demod) {
	case FE_SAT_DEMOD_1:
		if ((pParams->hDemod->Error) || (pParams->hTuner1->Error))
			return FE_LLA_I2C_ERROR;
		/* Demod Stop */
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P1_DMDISTATE,
				0x5C);

		FE_Sat_TunerSetFrequency(pParams->hTuner1, 950);

		if ((pParams->hDemod->Error) || (pParams->hTuner1->Error))
			/*Check the error at the end of the function */
			error = FE_LLA_I2C_ERROR;
		break;

	case FE_SAT_DEMOD_2:
		if ((pParams->hDemod->Error) || (pParams->hTuner2->Error))
			return FE_LLA_I2C_ERROR;
		/* Demod Stop */
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P2_DMDISTATE,
				0x5C);

		FE_Sat_TunerSetFrequency(pParams->hTuner2, 950);
		if ((pParams->hDemod->Error) || (pParams->hTuner2->Error))
			/*Check the error at the end of the function */
			error = FE_LLA_I2C_ERROR;
		break;

	default:
		break;
	}

	return error;
}

/*****************************************************
--FUNCTION	::	FE_STV0900_ResetDevicesErrors
--ACTION	::	Reset Devices I2C error status
--PARAMS IN	::	Handle	==>	Front End Handle
			::
-PARAMS OUT::	NONE.
--RETURN	::	Error (if any)
--***************************************************/
FE_STV0900_Error_t FE_STV0900_ResetDevicesErrors(FE_STV0900_Handle_t Handle,
						 FE_STV0900_DEMOD_t Demod)
{
	FE_STV0900_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0900_InternalParams_t *pParams =
			(FE_STV0900_InternalParams_t *)Handle;

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
--FUNCTION	::	FE_STV0900_STV6120_HMRFilter
--ACTION	::	use this function after the acquisition to calibrate
			the STV6120 tuner HMR filter
--PARAMS IN	::	Handle	==>	Front End Handle
			::	Demod	==>	Current demod 1 or 2
-PARAMS OUT::	NONE.
--RETURN	::	Error (if any)
--***************************************************/
FE_STV0900_Error_t FE_STV0900_STV6120_HMRFilter(FE_STV0900_Handle_t Handle,
						FE_STV0900_DEMOD_t Demod)
{
	FE_STV0900_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0900_InternalParams_t *pParams =
				(FE_STV0900_InternalParams_t *)Handle;

	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;

	switch (Demod) {
	case FE_SAT_DEMOD_1:
		if ((pParams->hDemod->Error) || (pParams->hTuner1->Error))
			return FE_LLA_I2C_ERROR;

		FE_STV0900_SetHMRFilter(pParams->hDemod, pParams->hTuner1,
						pParams->hVglna1, Demod);
		if ((pParams->hDemod->Error) || (pParams->hTuner1->Error))
			/*Check the error at the end of the function */
			error = FE_LLA_I2C_ERROR;
		break;

	case FE_SAT_DEMOD_2:
		if ((pParams->hDemod->Error) || (pParams->hTuner2->Error))
			return FE_LLA_I2C_ERROR;

		FE_STV0900_SetHMRFilter(pParams->hDemod, pParams->hTuner2,
						pParams->hVglna2, Demod);
		if ((pParams->hDemod->Error) || (pParams->hTuner2->Error))
			/*Check the error at the end of the function */
			error = FE_LLA_I2C_ERROR;
		break;

	default:
		break;
	}

	return error;
}

/*****************************************************
--FUNCTION	::	FE_STV0900_LDPCPowerTracking
--ACTION	::	track the LDPC activity and stop it when bad CNR (< QEF)
for the given modcode.
this function should be called every 500ms from the upper layer.

THIS FUNCTION SHOULD NOT BE USED IN ACM or VCM : THIS FUNCTION IS FOR CCM ONLY

--PARAMS IN	::	Handle	==>	Front End Handle
--PARAMS OUT::	NONE.
--RETURN	::	LDPC state ==> FE_STV0900_LDPC_State.
			0: stopped for path1 and 2
			1: on for path1 , off for path2
			2: off for path1, on for path2
			3: on for path1 and 2.
			Ajouter un parametre demod et traiter separareemnt
--***************************************************/
FE_STV0900_LDPC_State FE_STV0900_LDPCPowerTracking(FE_STV0900_Handle_t Handle,
						   FE_STV0900_DEMOD_t Demod)
{

	FE_STV0900_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0900_InternalParams_t *pParams =
				(FE_STV0900_InternalParams_t *)Handle;
	FE_STV0900_LDPC_State ldpcState = 3;

	/*THIS FUNCTION SHOULD NOT BE USED IN ACM or VCM : THIS FUNCTION IS FOR
	 * CCM ONLY */

	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		if ((pParams->hDemod->Error) || (pParams->hTuner1->Error))
			return FE_LLA_I2C_ERROR;

		if (ChipGetField(pParams->hDemod, FSTV0900_P1_HEADER_MODE) ==
				FE_SAT_DVBS2_FOUND)
			if (FE_STV0900_LDPCPowerMonitoring(pParams->hDemod,
						FE_SAT_DEMOD_1) == TRUE)
				ldpcState = 0;
			else
				ldpcState = 1;
		else
			ldpcState = 0;

		if ((pParams->hDemod->Error) || (pParams->hTuner1->Error))
			/*Check the error at the end of the function */
			error = FE_LLA_I2C_ERROR;
		break;

	case FE_SAT_DEMOD_2:

		if ((pParams->hDemod->Error) || (pParams->hTuner2->Error))
			return FE_LLA_I2C_ERROR;
		if (ChipGetField(pParams->hDemod, FSTV0900_P2_HEADER_MODE) ==
				FE_SAT_DVBS2_FOUND)
			if (FE_STV0900_LDPCPowerMonitoring(pParams->hDemod,
						FE_SAT_DEMOD_2) == TRUE)
				ldpcState = ldpcState & 1;
			else
				ldpcState = ldpcState | 2;
		else
			ldpcState = ldpcState & 1;

		if ((pParams->hDemod->Error) || (pParams->hTuner2->Error))
			/*Check the error at the end of the function */
			error = FE_LLA_I2C_ERROR;
		break;
	}

	return ldpcState;
}

/*****************************************************
--FUNCTION	::	FE_STV0900_SetMclk
--ACTION	::	Set demod Master Clock
--PARAMS IN	::	Handle	==>	Front End Handle
			::	Mclk	==> demod master clock
			::	ExtClk	==>	external Quartz
--PARAMS OUT::	NONE.
--RETURN	::	Error (if any)
--***************************************************/
FE_STV0900_Error_t FE_STV0900_SetMclk(FE_STV0900_Handle_t Handle, U32 Mclk,
				      U32 ExtClk)
{
	FE_STV0900_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0900_InternalParams_t *pParams =
	    (FE_STV0900_InternalParams_t *) Handle;

	U32 mDiv, clkSel;
	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;
	if (pParams->hDemod->Error)
		return FE_LLA_I2C_ERROR;

	clkSel = ((ChipGetField(pParams->hDemod, FSTV0900_SELX1RATIO) == 1) ? 4
			: 6);
	mDiv = ((clkSel * Mclk) / pParams->Quartz) - 1;
	ChipSetField(pParams->hDemod, FSTV0900_M_DIV, mDiv);
	pParams->MasterClock = FE_STV0900_GetMclkFreq(pParams->hDemod,
							pParams->Quartz);

	/*Set the DiseqC frequency to 22KHz */
	/*
	   Formula:
	   DiseqC_TX_Freq= MasterClock/(32*F22TX_Reg)
	   DiseqC_RX_Freq= MasterClock/(32*F22RX_Reg)
	 */
	mDiv = pParams->MasterClock / 704000;
	ChipSetOneRegister(pParams->hDemod, RSTV0900_P1_F22TX, mDiv);
	ChipSetOneRegister(pParams->hDemod, RSTV0900_P1_F22RX, mDiv);

	ChipSetOneRegister(pParams->hDemod, RSTV0900_P2_F22TX, mDiv);
	ChipSetOneRegister(pParams->hDemod, RSTV0900_P2_F22RX, mDiv);

	if (pParams->hDemod->Error)
		/*Check the error at the end of the function */
		error = FE_LLA_I2C_ERROR;

	return error;
}

/*****************************************************
--FUNCTION	::	FE_STV0900_SetStandby
--ACTION	::	Set demod STANDBAY mode On/Off
--PARAMS IN	::	Handle	==>	Front End Handle

-PARAMS OUT::	NONE.
--RETURN	::	Error (if any)
- Ajout demod et separe les 2
--***************************************************/
FE_STV0900_Error_t FE_STV0900_SetStandby(FE_STV0900_Handle_t Handle,
					 U8 StandbyOn, FE_STV0900_DEMOD_t Demod)
{
	FE_STV0900_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0900_InternalParams_t *pParams =
				(FE_STV0900_InternalParams_t *)Handle;

	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		if ((pParams->hDemod->Error) || (pParams->hTuner1->Error))
			return FE_LLA_I2C_ERROR;

		if (StandbyOn) {
			/*ADC1 power off */
			ChipSetField(pParams->hDemod, FSTV0900_ADC1_PON, 0);
			/*DiseqC 1 power off */
			ChipSetField(pParams->hDemod, FSTV0900_DISEQC1_PON, 0);

			/*Stop Path1 Clocks */
			/*Stop packet delineator 1 clock */
			ChipSetField(pParams->hDemod, FSTV0900_STOP_CLKPKDT1,
					1);
			/*Stop ADC 1 clock */
			ChipSetField(pParams->hDemod, FSTV0900_STOP_CLKADCI1,
					1);
			/*Stop Sampling clock */
			ChipSetField(pParams->hDemod, FSTV0900_STOP_CLKSAMP1,
					1);
			/*Stop VITERBI 1 clock */
			ChipSetField(pParams->hDemod, FSTV0900_STOP_CLKVIT1, 1);

			if (ChipGetField(pParams->hDemod, FSTV0900_ADC2_PON) ==
					0) {
				/* check if path2 is in low power mode */
				/*
				   The STANDBY as well as STOP_CLKFEC field and
				   STOP_CLKTS field are common for both path

				   these field are set to '1' only when both
				   path's are in low power mode a path is in
				   low power mode when it's ADC is stoped
				 */

				/*FSK power off */
				ChipSetField(pParams->hDemod, FSTV0900_FSK_PON,
						0);
				/*Stop FEC clock */
			/*	ChipSetField(pParams->hDemod,
						FSTV0900_STOP_CLKFEC, 1);*/
				/*Stop TS clock */
			/*	ChipSetField(pParams->hDemod,
						FSTV0900_STOP_CLKTS, 1);*/
				/* general power OFF */
				ChipSetField(pParams->hDemod, FSTV0900_STANDBY,
						1);
			}

			/* Set tuner1 to standby On */
			FE_Sat_TunerSetStandby(pParams->hTuner1, 1);
			if (FE_STV0900_CheckVGLNAPresent(pParams->hVglna1))
				STVVGLNA_SetStandby(pParams->hVglna1, 1);
		} else {
			/* general power ON */
			ChipSetField(pParams->hDemod, FSTV0900_STANDBY, 0);

			/*FSK power on */
			ChipSetField(pParams->hDemod, FSTV0900_FSK_PON, 1);
			/*ADC1 power on */
			ChipSetField(pParams->hDemod, FSTV0900_ADC1_PON, 1);
			/*DiseqC 1 power on */
			ChipSetField(pParams->hDemod, FSTV0900_DISEQC1_PON, 1);

			/*Activate Path1 Clocks */
			/*Activate packet delineator 1 clock */
			ChipSetField(pParams->hDemod, FSTV0900_STOP_CLKPKDT1,
					0);
			/*Activate ADC 1 clock */
			ChipSetField(pParams->hDemod, FSTV0900_STOP_CLKADCI1,
					0);
			/*Activate Sampling clock */
			ChipSetField(pParams->hDemod, FSTV0900_STOP_CLKSAMP1,
					0);
			/*Activate VITERBI 1 clock */
			ChipSetField(pParams->hDemod, FSTV0900_STOP_CLKVIT1, 0);

			/*Activate FEC clock */
			/* ChipSetField(pParams->hDemod, FSTV0900_STOP_CLKFEC,
			 * 0);
			 */
			/*Activate TS clock */
			/*
			 *ChipSetField(pParams->hDemod, FSTV0900_STOP_CLKTS, 0);
			 */

			/* Set tuner1 to standby OFF */
			FE_Sat_TunerSetStandby(pParams->hTuner1, 0);
			if (FE_STV0900_CheckVGLNAPresent(pParams->hVglna1))
				STVVGLNA_SetStandby(pParams->hVglna1, 1);
		}

		if ((pParams->hDemod->Error) || (pParams->hTuner1->Error))
			/*Check the error at the end of the function */
			error = FE_LLA_I2C_ERROR;
		break;

	case FE_SAT_DEMOD_2:

		if ((pParams->hDemod->Error) || (pParams->hTuner2->Error))
			return FE_LLA_I2C_ERROR;
		if (StandbyOn) {
			/* ChipSetField(pParams->hDemod, FSTV0900_STANDBY, 1);*/
			/*ADC2 power off */
			ChipSetField(pParams->hDemod, FSTV0900_ADC2_PON, 0);
			/*DiseqC 2 power off */
			ChipSetField(pParams->hDemod, FSTV0900_DISEQC2_PON, 0);

			/*Stop Path2 Clocks */
			/*Stop packet delineator 2 clock */
			ChipSetField(pParams->hDemod, FSTV0900_STOP_CLKPKDT2,
					1);
			/*Stop ADC 2 clock */
			ChipSetField(pParams->hDemod, FSTV0900_STOP_CLKADCI2,
					1);
			/*Stop Sampling clock */
			ChipSetField(pParams->hDemod, FSTV0900_STOP_CLKSAMP2,
					1);
			/*Stop VITERBI 2 clock */
			ChipSetField(pParams->hDemod, FSTV0900_STOP_CLKVIT2, 1);

			if (ChipGetField(pParams->hDemod, FSTV0900_ADC1_PON) ==
					0) {
				/* check if path1 is in low power mode */
				/*
				   The STANDBY as well as STOP_CLKFEC field and
				   STOP_CLKTS field are common for both path

				   these field are set to '1' only when both
				   path's are in low power mode a path is in
				   low power mode when it's ADC is stoped
				 */

				/*FSK power off */
				ChipSetField(pParams->hDemod, FSTV0900_FSK_PON,
						0);
				/*Stop FEC clock */
			/*	ChipSetField(pParams->hDemod,
						FSTV0900_STOP_CLKFEC, 1);*/
				/*Stop TS clock */
			/*	ChipSetField(pParams->hDemod,
						FSTV0900_STOP_CLKTS, 1);*/
				/* general power ON */
				ChipSetField(pParams->hDemod, FSTV0900_STANDBY,
						1);
			}

			/* Set tuner2 to standby On */
			FE_Sat_TunerSetStandby(pParams->hTuner2, 1);
			if (FE_STV0900_CheckVGLNAPresent(pParams->hVglna2))
				STVVGLNA_SetStandby(pParams->hVglna2, 1);
		} else {
			/* general power ON */
			ChipSetField(pParams->hDemod, FSTV0900_STANDBY, 0);

			/*FSK power on */
			ChipSetField(pParams->hDemod, FSTV0900_FSK_PON, 1);
			/*ADC2 power on */
			ChipSetField(pParams->hDemod, FSTV0900_ADC2_PON, 1);
			/*DiseqC 2 power on */
			ChipSetField(pParams->hDemod, FSTV0900_DISEQC2_PON, 1);

			/*Activate Path2 Clocks */
			/*Activate packet delineator 2 clock */
			ChipSetField(pParams->hDemod, FSTV0900_STOP_CLKPKDT2,
					0);
			/*Activate ADC 2 clock */
			ChipSetField(pParams->hDemod, FSTV0900_STOP_CLKADCI2,
					0);
			/*Activate Sampling clock */
			ChipSetField(pParams->hDemod, FSTV0900_STOP_CLKSAMP2,
					0);
			/*Activate VITERBI 2 clock */
			ChipSetField(pParams->hDemod, FSTV0900_STOP_CLKVIT2, 0);

			/*Activate FEC clock */
			/* ChipSetField(pParams->hDemod, FSTV0900_STOP_CLKFEC,
			 * 0);
			 */
			/*Activate TS clock */
			/*
			 *ChipSetField(pParams->hDemod, FSTV0900_STOP_CLKTS, 0);
			 */

			/* Set tuner2 to standby OFF */
			FE_Sat_TunerSetStandby(pParams->hTuner2, 0);
			if (FE_STV0900_CheckVGLNAPresent(pParams->hVglna2))
				STVVGLNA_SetStandby(pParams->hVglna2, 0);
		}

		if ((pParams->hDemod->Error) || (pParams->hTuner2->Error))
			/*Check the error at the end of the function */
			error = FE_LLA_I2C_ERROR;
		break;
	}

	return error;
}

/*****************************************************
--FUNCTION	::	FE_STV0900_SetAbortFlag
--ACTION	::	Set Abort flag On/Off
--PARAMS IN	::	Handle	==>	Front End Handle

-PARAMS OUT::	NONE.
--RETURN	::	Error (if any)
ajoute demod et separe les 2
--***************************************************/
FE_STV0900_Error_t FE_STV0900_SetAbortFlag(FE_STV0900_Handle_t Handle,
					   BOOL Abort, FE_STV0900_DEMOD_t Demod)
{
	FE_STV0900_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0900_InternalParams_t *pParams =
				(FE_STV0900_InternalParams_t *)Handle;

	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		if ((pParams->hDemod->Error) || (pParams->hTuner1->Error))
			return FE_LLA_I2C_ERROR;

		ChipAbort(pParams->hTuner1, Abort);
		ChipAbort(pParams->hDemod, Abort);

		if ((pParams->hDemod->Error) || (pParams->hTuner1->Error))
			/*Check the error at the end of the function */
			error = FE_LLA_I2C_ERROR;
		break;

	case FE_SAT_DEMOD_2:
		if ((pParams->hDemod->Error) || (pParams->hTuner2->Error))
			return FE_LLA_I2C_ERROR;

		ChipAbort(pParams->hTuner2, Abort);
		ChipAbort(pParams->hDemod, Abort);

		if ((pParams->hDemod->Error) || (pParams->hTuner2->Error))
			/*Check the error at the end of the function */
			error = FE_LLA_I2C_ERROR;
		break;
	}

	return error;
}

/*****************************************************
--FUNCTION	::	FE_STV0900_SetDual8PSK45MBs
--ACTION	::	Reduce the LDPC iterations in order to acheve dual
			45Msps 8PSK
--PARAMS IN	::	Handle	==>	Front End Handle
			    SymbolRate ==> max operational symbol rate
-PARAMS OUT::	NONE.
--RETURN	::	Error (if any)
--***************************************************/
FE_STV0900_Error_t FE_STV0900_SetDual8PSK45MBs(FE_STV0900_Handle_t Handle,
					       U32 SymbolRate,
					       FE_STV0900_DEMOD_t Demod)
{
	FE_STV0900_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0900_InternalParams_t *pParams =
				(FE_STV0900_InternalParams_t *)Handle;

	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;
	/* if dual 8PSK 45Msps required then reduce the LDPC iterations */
	if (SymbolRate > 31500000) {
		ChipSetOneRegister(pParams->hDemod, RSTV0900_NBITER_NF12, 0x1D);
		ChipSetOneRegister(pParams->hDemod, RSTV0900_NBITER_NF13, 0x26);
		ChipSetOneRegister(pParams->hDemod, RSTV0900_NBITER_NF14, 0x23);
		ChipSetOneRegister(pParams->hDemod, RSTV0900_NBITER_NF15, 0x21);
		ChipSetOneRegister(pParams->hDemod, RSTV0900_NBITER_NF16, 0x28);
		ChipSetOneRegister(pParams->hDemod, RSTV0900_NBITER_NF17, 0x28);
	} else {
		ChipSetOneRegister(pParams->hDemod, RSTV0900_NBITER_NF12, 0x29);
		ChipSetOneRegister(pParams->hDemod, RSTV0900_NBITER_NF13, 0x37);
		ChipSetOneRegister(pParams->hDemod, RSTV0900_NBITER_NF14, 0x33);
		ChipSetOneRegister(pParams->hDemod, RSTV0900_NBITER_NF15, 0x2F);
		ChipSetOneRegister(pParams->hDemod, RSTV0900_NBITER_NF16, 0x39);
		ChipSetOneRegister(pParams->hDemod, RSTV0900_NBITER_NF17, 0x3A);

	}

	return error;

}

/*****************************************************
--FUNCTION	::	FE_STV0900_SetDVBS2_Single
--ACTION	::	Set 900 LDPC to Single or dual
--PARAMS IN	::	Handle	==>	Front End Handle
		::	LDPC_Mode ==> LDPC setting single or dual
		::	Demod ==> in single mode this parameter is used to set
			the LDPC in single mode to the given demod
-PARAMS OUT::	NONE.
--RETURN	::	Error (if any)
--***************************************************/
FE_STV0900_Error_t FE_STV0900_SetDVBS2_Single(FE_STV0900_Handle_t Handle,
					      FE_Sat_DemodType LDPC_Mode,
					      FE_STV0900_DEMOD_t Demod)
{
	FE_STV0900_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0900_InternalParams_t *pParams =
					(FE_STV0900_InternalParams_t *)Handle;
	S32 RegIndex;

	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;

	if (pParams->hDemod->Error)
		return FE_LLA_I2C_ERROR;

	switch (LDPC_Mode) {
	case FE_SAT_DUAL:
	default:
		/*If the LDPC is already in dual mode do nothing */
		if ((pParams->DemodType == FE_SAT_DUAL) &&
			 (ChipGetField(pParams->hDemod, FSTV0900_DDEMOD) == 1))
			goto err_chk_ret;
		/*Set the LDPC to dual mode */
		ChipSetOneRegister(pParams->hDemod, RSTV0900_GENCFG, 0x1d);
		pParams->DemodType = FE_SAT_DUAL;

		/*reset the LDPC */
		ChipSetField(pParams->hDemod, FSTV0900_FRESFEC, 1);
		/*reset the LDPC */
		ChipSetField(pParams->hDemod, FSTV0900_FRESFEC, 0);

		/*modcode list mask for demod1 (only QPSK's and 8PSK's) */
		for (RegIndex = 0; RegIndex < 7; RegIndex++)
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P1_MODCODLST0 + RegIndex,
					0xFF);
		for (RegIndex = 0; RegIndex < 8; RegIndex++)
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P1_MODCODLST7 + RegIndex,
					0xcc);

		ChipSetOneRegister(pParams->hDemod, RSTV0900_P1_MODCODLSTE,
				0xFF);
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P1_MODCODLSTF,
				0xCF);

		/*modcode list mask for demod2 (only QPSK's and 8PSK's) */
		for (RegIndex = 0; RegIndex < 7; RegIndex++)
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P2_MODCODLST0 + RegIndex,
					0xFF);
		for (RegIndex = 0; RegIndex < 8; RegIndex++)
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P2_MODCODLST7 + RegIndex,
					0xcc);

		ChipSetOneRegister(pParams->hDemod, RSTV0900_P2_MODCODLSTE,
				0xFF);
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P2_MODCODLSTF,
				0xCF);
		break;

	case FE_SAT_SINGLE:
		if (Demod == FE_SAT_DEMOD_2) {
			/*disable all modcode for demod1 */
			FE_STV0900_StopALL_S2_Modcod(pParams->hDemod,
					FE_SAT_DEMOD_1);
			/*enable all modcode for demod2 */
			FE_STV0900_ActivateS2ModCodeSingle(pParams->hDemod,
					FE_SAT_DEMOD_2);

			/*Set the LDPC to single mode on path 2 */
			ChipSetOneRegister(pParams->hDemod, RSTV0900_GENCFG,
					0x06);
		} else {
			/*disable all modcode for demod2 */
			FE_STV0900_StopALL_S2_Modcod(pParams->hDemod,
					FE_SAT_DEMOD_2);
			/*enable all modcode for demod1 */
			FE_STV0900_ActivateS2ModCodeSingle(pParams->hDemod,
					FE_SAT_DEMOD_1);

			/*Set the LDPC to single mode on path 1 */
			ChipSetOneRegister(pParams->hDemod, RSTV0900_GENCFG,
					0x04);
		}
		pParams->DemodType = FE_SAT_SINGLE;

		/*reset the LDPC */
		ChipSetField(pParams->hDemod, FSTV0900_FRESFEC, 1);
		/*reset the LDPC */
		ChipSetField(pParams->hDemod, FSTV0900_FRESFEC, 0);

		/*reset the packet delin */
		ChipSetField(pParams->hDemod, FSTV0900_P1_ALGOSWRST, 1);
		/*reset the packet delin */
		ChipSetField(pParams->hDemod, FSTV0900_P1_ALGOSWRST, 0);

		/*reset the packet delin */
		ChipSetField(pParams->hDemod, FSTV0900_P2_ALGOSWRST, 1);
		/*reset the packet delin */
		ChipSetField(pParams->hDemod, FSTV0900_P2_ALGOSWRST, 0);

		break;
	}

err_chk_ret:
	if (pParams->hDemod->Error)
		/*Check the error at the end of the function */
		error = FE_LLA_I2C_ERROR;

	return error;
}

/*****************************************************
--FUNCTION	::	FE_STV0900_Search
--ACTION	::	Search for a valid transponder
--PARAMS IN	::	Handle	==>	Front End Handle
				pSearch ==> Search parameters
				pResult ==> Result of the search
--PARAMS OUT::	NONE
--RETURN	::	Error (if any)
--***************************************************/
FE_STV0900_Error_t FE_STV0900_Search(FE_STV0900_Handle_t Handle,
				     FE_STV0900_SearchParams_t *pSearch,
				     FE_STV0900_SearchResult_t *pResult)
{
	FE_STV0900_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0900_InternalParams_t *pParams;

	if (Handle == NULL)
		return FE_LLA_INVALID_HANDLE;

	pParams = (FE_STV0900_InternalParams_t *)Handle;

	if (!(INRANGE(100000, pSearch->SymbolRate, 70000000)) ||
	    !(INRANGE(100000, pSearch->SearchRange, 50000000)))
		return FE_LLA_BAD_PARAMETER;

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
		}

		break;
	}

	if (error != FE_LLA_NO_ERROR)
		return error;

	if (!(FE_STV0900_Algo(pParams, pSearch->Path) == FE_SAT_RANGEOK)
		    || !(pParams->hDemod->Error == CHIPERR_NO_ERROR)) {
		pResult->Locked = FALSE;

		switch (pSearch->Path) {
		case FE_SAT_DEMOD_1:
			switch (pParams->Demod1Error) {
			case FE_LLA_I2C_ERROR:
				/*I2C error */
				error = FE_LLA_I2C_ERROR;
				break;

			case FE_LLA_NO_ERROR:
			default:
				error = FE_LLA_SEARCH_FAILED;
				break;
			}
			break;

		case FE_SAT_DEMOD_2:
			switch (pParams->Demod2Error) {
			case FE_LLA_I2C_ERROR:
				/*I2C error     */
				error = FE_LLA_I2C_ERROR;
				break;

			case FE_LLA_NO_ERROR:
			default:
				error = FE_LLA_SEARCH_FAILED;
				break;
			}

			break;
		}
		return error;
	}
	switch (pSearch->Path) {
	case FE_SAT_DEMOD_1:
	default:
		pResult->Locked = pParams->Demod1Results.Locked;

		/* update results */
		pResult->Standard = pParams->Demod1Results.Standard;
		pResult->Frequency = pParams->Demod1Results.Frequency;
		pResult->SymbolRate = pParams->Demod1Results.SymbolRate;
		pResult->PunctureRate = pParams->Demod1Results.PunctureRate;
		pResult->ModCode = pParams->Demod1Results.ModCode;
		pResult->Pilots = pParams->Demod1Results.Pilots;
		pResult->FrameLength = pParams->Demod1Results.FrameLength;
		pResult->Spectrum = pParams->Demod1Results.Spectrum;
		pResult->RollOff = pParams->Demod1Results.RollOff;
		pResult->Modulation = pParams->Demod1Results.Modulation;

		if ((pParams->hDemod->Error) || (pParams->hTuner1->Error))
			error = FE_LLA_I2C_ERROR;

		break;

	case FE_SAT_DEMOD_2:
		pResult->Locked = pParams->Demod2Results.Locked;

		/* update results */
		pResult->Standard = pParams->Demod2Results.Standard;
		pResult->Frequency = pParams->Demod2Results.Frequency;
		pResult->SymbolRate = pParams->Demod2Results.SymbolRate;
		pResult->PunctureRate = pParams->Demod2Results.PunctureRate;
		pResult->ModCode = pParams->Demod2Results.ModCode;
		pResult->Pilots = pParams->Demod2Results.Pilots;
		pResult->FrameLength = pParams->Demod2Results.FrameLength;
		pResult->Spectrum = pParams->Demod2Results.Spectrum;
		pResult->RollOff = pParams->Demod2Results.RollOff;
		pResult->Modulation = pParams->Demod2Results.Modulation;

		if ((pParams->hDemod->Error) || (pParams->hTuner2->Error))
			error = FE_LLA_I2C_ERROR;

		break;

	}

	return error;

}

/*****************************************************
--FUNCTION	::	FE_STX7111_SetTunerAttenuator
--ACTION	::	Set 6130 tuner attenuator ON/OFF
--PARAMS IN	::	Handle	   ==> Front End Handle
				Path       ==> Path1/2
				Attenuator ==> ON/OFF
--PARAMS OUT::	NONE
--RETURN	::	Error (if any)
--***************************************************/
FE_STV0900_Error_t FE_STV0900_SetTunerAttenuator(FE_STV0900_Handle_t Handle,
						 FE_STV0900_DEMOD_t Demod,
						 BOOL Attenuator)
{
	FE_STV0900_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0900_InternalParams_t *pParams;
	if (Handle == NULL)
		return FE_LLA_INVALID_HANDLE;

	pParams = (FE_STV0900_InternalParams_t *)Handle;
	switch (Demod) {
	case FE_SAT_DEMOD_1:
		/*Path1 */
		if ((pParams->hDemod->Error) || (pParams->hTuner1->Error))
			return FE_LLA_I2C_ERROR;
		/* FE_Sat_TunerSetAttenuator(pParams->hTuner1, Attenuator)*/;
		if ((pParams->hDemod->Error) || (pParams->hTuner1->Error))
			/*Check the error at the end of the function */
			error = FE_LLA_I2C_ERROR;
		break;

	case FE_SAT_DEMOD_2:
		/*Path2 */
		if ((pParams->hDemod->Error) || (pParams->hTuner2->Error))
			return FE_LLA_I2C_ERROR;
		/* FE_Sat_TunerSetAttenuator(pParams->hTuner2, Attenuator)*/;
		if ((pParams->hDemod->Error) || (pParams->hTuner2->Error))
			/*Check the error at the end of the function */
			error = FE_LLA_I2C_ERROR;
		break;

	default:
		error = FE_LLA_BAD_PARAMETER;
		break;
	}

	return error;
}

/*****************************************************
--FUNCTION	::	FE_STV0900_SetTunerBBGain
--ACTION	::	Set tuner BBGain
--PARAMS IN	::	Handle	   ==> Front End Handle
				Path       ==> Path1/2
				Attenuator ==> ON/OFF
--PARAMS OUT::	NONE
--RETURN	::	Error (if any)
--***************************************************/
FE_STV0900_Error_t FE_STV0900_SetTunerBBGain(FE_STV0900_Handle_t Handle,
					     FE_STV0900_DEMOD_t Demod, S32 Gain)
{
	FE_STV0900_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0900_InternalParams_t *pParams;
	if (Handle == NULL)
		return FE_LLA_INVALID_HANDLE;

	pParams = (FE_STV0900_InternalParams_t *)Handle;
	switch (Demod) {
	case FE_SAT_DEMOD_1:
		/*Path1 */
		if ((pParams->hDemod->Error) || (pParams->hTuner1->Error))
			return FE_LLA_I2C_ERROR;

		FE_Sat_TunerSetGain(pParams->hTuner1, Gain);

		/*Check the error at the end of the function */
		if ((pParams->hDemod->Error) || (pParams->hTuner1->Error))
			error = FE_LLA_I2C_ERROR;
		break;

	case FE_SAT_DEMOD_2:
		/*Path2 */
		if ((pParams->hDemod->Error) || (pParams->hTuner2->Error))
			return FE_LLA_I2C_ERROR;

		FE_Sat_TunerSetGain(pParams->hTuner2, Gain);

		/*Check the error at the end of the function */
		if ((pParams->hDemod->Error) || (pParams->hTuner2->Error))
			error = FE_LLA_I2C_ERROR;
		break;

	default:
		error = FE_LLA_BAD_PARAMETER;
		break;
	}

	return error;
}

/*****************************************************
--FUNCTION	::	FE_STV0900_DVBS2_SetGoldCode
--ACTION	::	Set the DVBS2 Gold Code
--PARAMS IN	::	Handle	==>	Front End Handle
				U32		==>	cold code value (18bits)
--PARAMS OUT::	NONE
--RETURN	::	Error (if any)
comme getSignalInfo
--***************************************************/
FE_STV0900_Error_t FE_STV0900_DVBS2_SetGoldCodeX(FE_STV0900_Handle_t Handle,
						 FE_STV0900_DEMOD_t Demod,
						 U32 GoldCode)
{
	FE_STV0900_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0900_InternalParams_t *pParams;

	if ((void *)Handle == NULL)
		return FE_LLA_INVALID_HANDLE;

	pParams = (FE_STV0900_InternalParams_t *)Handle;

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		if ((pParams->hDemod->Error) || (pParams->hTuner1->Error))
			return FE_LLA_I2C_ERROR;
		/*Gold code X setting */
		/* bit[3:4] of register PLROOT2
		   3..2 : P1_PLSCRAMB_MODE[1:0] |URW| entry mode
		   p1_plscramb_root 00: p1_plscramb_root is the root of
		   PRBS X.01: p1_plscramb_root is the DVBS2 gold code.
		 */
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P1_PLROOT2, 0x04 |
				((GoldCode >> 16) & 0x3));
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P1_PLROOT1,
				(GoldCode >> 8) & 0xff);
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P1_PLROOT0,
				(GoldCode) & 0xff);
		if ((pParams->hDemod->Error) || (pParams->hTuner1->Error))
			error = FE_LLA_I2C_ERROR;
		break;

	case FE_SAT_DEMOD_2:
		if ((pParams->hDemod->Error) || (pParams->hTuner2->Error))
			return FE_LLA_I2C_ERROR;
		/*Gold code X setting */
		/* bit[3:4] of register PLROOT2
		   3..2 : P1_PLSCRAMB_MODE[1:0] |URW| entry mode
		   p1_plscramb_root 00: p1_plscramb_root is the root of
		   PRBS X.01: p1_plscramb_root is the DVBS2 gold code.
		 */

		ChipSetOneRegister(pParams->hDemod, RSTV0900_P2_PLROOT2, 0x04 |
				((GoldCode >> 16) & 0x3));
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P2_PLROOT1,
				(GoldCode >> 8) & 0xff);
		ChipSetOneRegister(pParams->hDemod, RSTV0900_P2_PLROOT0,
				(GoldCode) & 0xff);
		if ((pParams->hDemod->Error) || (pParams->hTuner2->Error))
			error = FE_LLA_I2C_ERROR;
	}

	return error;
}

/*****************************************************
--FUNCTION	::	FE_STV0900_StopTransportStream
--ACTION	::	Disable or Enable the transport stream
--PARAMS IN	::	Handle	==>	Front End Handle
			BOOL	==>	if TRUE stop the TS else Activate the TS
--PARAMS OUT::	NONE
--RETURN	::	Error (if any)
--***************************************************/
FE_STV0900_Error_t FE_STV0900_StopTransportStream(FE_STV0900_Handle_t Handle,
						  FE_STV0900_DEMOD_t Demod,
						  BOOL StopTS)
{
	/*
	   For DUAL S1 This function should be called (to stop the TS) before
	   changing the LNB setting, diseqC setting or any other device which
	   make the demod lose lock The TS will be enabled then during the
	   acquisition function "FE_STV0900_Algo"

	   To simplify the procedure in the upper layer (STTUNER) this function
	   could be called by default (for all cases) as the first step of the
	   acquisition (before any other setting (LNB, diseqC, FSK,...)

	   For the scan mode this function should be called as the first step
	   of each iteration (LNB move, frequency move).
	 */

	FE_STV0900_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0900_InternalParams_t *pParams;

	if ((void *)Handle == NULL)
		return FE_LLA_INVALID_HANDLE;
	pParams = (FE_STV0900_InternalParams_t *) Handle;

	switch (Demod) {
	case FE_SAT_DEMOD_1:
		if ((pParams->hDemod->Error) || (pParams->hTuner1->Error))
			return FE_LLA_I2C_ERROR;

		if (StopTS == TRUE)
			/*Stop the Path 1 Stream Merger (TS) */
			ChipSetField(pParams->hDemod, FSTV0900_P1_RST_HWARE, 1);
		else
			/*release the Path 1 Stream Merger (TS) */
			ChipSetField(pParams->hDemod, FSTV0900_P1_RST_HWARE, 0);

		if ((pParams->hDemod->Error) || (pParams->hTuner1->Error))
			/*Check the error at the end of the function */
			error = FE_LLA_I2C_ERROR;
		break;

	case FE_SAT_DEMOD_2:
		if ((pParams->hDemod->Error) || (pParams->hTuner2->Error))
			return FE_LLA_I2C_ERROR;

		if (StopTS == TRUE)
			/*Stop the Path 2 Stream Merger (TS) */
			ChipSetField(pParams->hDemod, FSTV0900_P2_RST_HWARE, 1);
		else
			/*release the Path 2 Stream Merger (TS) */
			ChipSetField(pParams->hDemod, FSTV0900_P2_RST_HWARE, 0);

		if ((pParams->hDemod->Error) || (pParams->hTuner2->Error))
			/*Check the error at the end of the function */
			error = FE_LLA_I2C_ERROR;
		break;

	default:
		break;
	}

	return error;
}

/*****************************************************
--FUNCTION	::	FE_STV0900_8PSKCarrierAdaptation
--ACTION	::	workaround for frequency shifter with high jitter and
			phase noise
--PARAMS IN	::	Handle	==>	Front End Handle

--PARAMS OUT::	NONE
--RETURN	::	Error (if any)
--***************************************************/
FE_STV0900_Error_t FE_STV0900_8PSKCarrierAdaptation(FE_STV0900_Handle_t Handle,
						    U32 SymbolRate,
						    FE_STV0900_DEMOD_t Demod)
{
	/* This is a workaround for DVBS2 8PSK pilots ON modes with a frequency
	 * shifter adding carrier jitter and phase noise */
	/*
	   this function must be called at least one time after each acquistion
	   to apply the workaround if needed

	   This workaround is applicable for cut 2.0 and 1.x only (NOT cut 3.0)

	   The workaround is the following: Read the modcode, read pilots
	   status: if (8PSK and pilots ON) then set registers ACLC and BCLC
	   respectively to 0x1A and 0x09 if one of these conditions:

	   1/ 8PSK 3/5 and C/N>=7.5dB.  2/ 8PSK 3/5 OR 2/3 and C/N>=8.5dB.  3/
	   8PSK 3/5 , 2/3, OR 3/4 and C/N>=10dB.  4/ 8PSK 3/5, 2/3, 3/4 OR 5/6
	   and C/N>=11dB.  5/ any 8PSK and  C/N>=12dB

	 */

	FE_STV0900_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0900_InternalParams_t *pParams;

	BOOL adaptCut20 = FALSE;

	FE_Sat_TrackingStandard_t standard;
	FE_Sat_ModCod_t modcode;
	FE_Sat_PILOTS_t pilots;

	S32 c_n;	/* 10*C/N in dB */
	U8 aclc, nco2Th;

	static BOOL IFConvertorPresent = TRUE;
	static BOOL IFConvertorDetection = TRUE;

	if (Handle == NULL)
		return FE_LLA_INVALID_HANDLE;

	pParams = (FE_STV0900_InternalParams_t *)Handle;

	if (pParams->hDemod->Error)
		return FE_LLA_I2C_ERROR;

	standard = FE_STV0900_GetStandard(pParams->hDemod, Demod);
	if (standard != FE_SAT_DVBS2_STANDARD)
		goto err_chk_ret;

	if (pParams->hDemod->ChipID <= 0x20) {
		if (Demod == FE_SAT_DEMOD_1) {
			modcode = ChipGetField(pParams->hDemod,
					FSTV0900_P1_DEMOD_MODCOD);
			pilots = ChipGetFieldImage(pParams->hDemod,
					FSTV0900_P1_DEMOD_TYPE) & 0x01;
		} else {
			modcode = ChipGetField(pParams->hDemod,
					FSTV0900_P2_DEMOD_MODCOD);
			pilots = ChipGetFieldImage(pParams->hDemod,
					FSTV0900_P2_DEMOD_TYPE) & 0x01;
		}

		if (!(pilots == FE_SAT_PILOTS_ON)
				|| !(modcode >= FE_SAT_8PSK_35)
				|| !(modcode <= FE_SAT_8PSK_910))
			goto err_chk_ret;
		/*read C/N FE_STV0900_CarrierGetQuality returns C/N*10dB */
		c_n = FE_STV0900_CarrierGetQuality(pParams->hDemod,
				&FE_STV0900_S2_CN_LookUp, Demod);

		if (c_n >= 120)
			/*C/N >=12dB */
			adaptCut20 = TRUE;
		else if (c_n >= 110) {
			/* 11dB<= C/N < 12dB */
			if (modcode <= FE_SAT_8PSK_56)
				adaptCut20 = TRUE;
		} else if (c_n >= 100) {
			/* 10dB<= C/N < 11dB */
			if (modcode <= FE_SAT_8PSK_34)
				adaptCut20 = TRUE;
		} else if (c_n >= 85) {
			/* 8.5dB<= C/N < 10dB */
			if (modcode <= FE_SAT_8PSK_23)
				adaptCut20 = TRUE;
		} else if (c_n >= 75) {
			/* 7.5dB<= C/N < 8.5dB */
			if (modcode == FE_SAT_8PSK_35)
				adaptCut20 = TRUE;
		}
		/*Cut 1.x and 2.0 */
		if (adaptCut20 == TRUE) {
			if (Demod == FE_SAT_DEMOD_1) {

				ChipSetFieldImage(pParams->hDemod,
						FSTV0900_P1_CAR_ALPHA_MANT, 1);
				/*ACLC registers image update */
				ChipSetFieldImage(pParams->hDemod,
						FSTV0900_P1_CAR_ALPHA_EXP,
						0xA);

				ChipSetFieldImage(pParams->hDemod,
						FSTV0900_P1_CAR_BETA_MANT, 0);
				/*BCLC registers image update */
				ChipSetFieldImage(pParams->hDemod,
						FSTV0900_P1_CAR_BETA_EXP, 0x9);

				/* write both ACLC and BCLC registers in the
				 * same I2C frame */
				/* mondatory to avoide error glitch during when
				 * continuesly monitoring the C/N to adpat
				 * registers values */
				ChipSetRegisters(pParams->hDemod,
						RSTV0900_P1_ACLC, 2);
			} else {

				ChipSetFieldImage(pParams->hDemod,
						FSTV0900_P2_CAR_ALPHA_MANT, 1);
				/*ACLC registers image update */
				ChipSetFieldImage(pParams->hDemod,
						FSTV0900_P2_CAR_ALPHA_EXP, 0xA);

				ChipSetFieldImage(pParams->hDemod,
						FSTV0900_P2_CAR_BETA_MANT, 0);
				/*BCLC registers image update */
				ChipSetFieldImage(pParams->hDemod,
						FSTV0900_P2_CAR_BETA_EXP, 0x9);

				/* write both ACLC and BCLC registers in the
				 * same I2C frame */
				/* mondatory to avoide error glitch during when
				 * continuesly monitoring the C/N to adpat
				 * registers values */
				ChipSetRegisters(pParams->hDemod,
						RSTV0900_P2_ACLC, 2);
			}

			goto err_chk_ret;
		}
		if (Demod == FE_SAT_DEMOD_1) {
			ChipSetFieldImage(pParams->hDemod,
					FSTV0900_P1_CAR_ALPHA_MANT, 0);
			/*ACLC registers image update */
			ChipSetFieldImage(pParams->hDemod,
					FSTV0900_P1_CAR_ALPHA_EXP, 0);
			ChipSetFieldImage(pParams->hDemod,
					FSTV0900_P1_CAR_BETA_MANT, 0);
			/*BCLC registers image update */
			ChipSetFieldImage(pParams->hDemod,
					FSTV0900_P1_CAR_BETA_EXP, 0);

			/* write both ACLC and BCLC registers in the same I2C
			 * frame */
			/* mondatory to avoide error glitch during when
			 * continuesly monitoring the C/N to adpat registers
			 * values */
			ChipSetRegisters(pParams->hDemod, RSTV0900_P1_ACLC, 2);
		} else {
			ChipSetFieldImage(pParams->hDemod,
					FSTV0900_P2_CAR_ALPHA_MANT, 0);
			/*ACLC registers image update */
			ChipSetFieldImage(pParams->hDemod,
					FSTV0900_P2_CAR_ALPHA_EXP, 0);
			ChipSetFieldImage(pParams->hDemod,
					FSTV0900_P2_CAR_BETA_MANT, 0);
			/*BCLC registers image update */
			ChipSetFieldImage(pParams->hDemod,
					FSTV0900_P2_CAR_BETA_EXP, 0);

			/* write both ACLC and BCLC registers in the same I2C
			 * frame */
			/* mondatory to avoide error glitch during when
			 * continuesly monitoring the C/N to adpat registers
			 * values */
			ChipSetRegisters(pParams->hDemod, RSTV0900_P2_ACLC, 2);
		}
		goto err_chk_ret;
	}
	/* Cut 3.0 */
	if (IFConvertorDetection == TRUE) {
		if (Demod == FE_SAT_DEMOD_1) {
			modcode = ChipGetField(pParams->hDemod,
					FSTV0900_P1_DEMOD_MODCOD);
			pilots = ChipGetFieldImage(pParams->hDemod,
					FSTV0900_P1_DEMOD_TYPE) & 0x01;
		} else {
			modcode = ChipGetField(pParams->hDemod,
					FSTV0900_P2_DEMOD_MODCOD);
			pilots = ChipGetFieldImage(pParams->hDemod,
					FSTV0900_P2_DEMOD_TYPE) & 0x01;
		}

		if (modcode <= FE_SAT_QPSK_910)
			nco2Th = 0x30;
		else
			nco2Th = 0x30;
		IFConvertorDetection = FALSE;

		if (Demod == FE_SAT_DEMOD_1) {
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P1_NCO2MAX1, 0);
			ChipWaitOrAbort(pParams->hDemod, 100);
			if (ChipGetOneRegister(pParams->hDemod,
					RSTV0900_P1_NCO2MAX1) >= nco2Th)
				IFConvertorPresent = TRUE;
			else
				IFConvertorPresent = FALSE;
		} else {
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P2_NCO2MAX1, 0);
			ChipWaitOrAbort(pParams->hDemod, 100);
			if (ChipGetOneRegister(pParams->hDemod,
					RSTV0900_P2_NCO2MAX1) >= nco2Th)
				IFConvertorPresent = TRUE;
			else
				IFConvertorPresent = FALSE;

		}
	}
	if (IFConvertorPresent == TRUE) {
		if (Demod == FE_SAT_DEMOD_1) {
			modcode = ChipGetField(pParams->hDemod,
					FSTV0900_P1_DEMOD_MODCOD);
			pilots = ChipGetFieldImage(pParams->hDemod,
					FSTV0900_P1_DEMOD_TYPE) & 0x01;
		} else {
			modcode = ChipGetField(pParams->hDemod,
					FSTV0900_P2_DEMOD_MODCOD);
			pilots = ChipGetFieldImage(pParams->hDemod,
					FSTV0900_P2_DEMOD_TYPE) & 0x01;
		}

		if (!(modcode >= FE_SAT_8PSK_35)
				|| !(modcode <= FE_SAT_8PSK_910))
			goto err_chk_ret;

		if (pilots == FE_SAT_PILOTS_ON) {
			c_n = FE_STV0900_CarrierGetQuality(pParams->hDemod,
					&FE_STV0900_S2_CN_LookUp, Demod);
			if (c_n > 103) {
				if (Demod == FE_SAT_DEMOD_1) {
					ChipSetOneRegister(pParams->hDemod,
							RSTV0900_P1_ACLC2S28,
							0x3A);
					ChipSetOneRegister(pParams->hDemod,
							RSTV0900_P1_BCLC2S28,
							0x0A);

					ChipSetOneRegister(pParams->hDemod,
							RSTV0900_P1_CAR2CFG,
							0x76);
				} else {
					ChipSetOneRegister(pParams->hDemod,
							RSTV0900_P2_ACLC2S28,
							0x3A);
					ChipSetOneRegister(pParams->hDemod,
							RSTV0900_P2_BCLC2S28,
							0x0A);

					ChipSetOneRegister(pParams->hDemod,
							RSTV0900_P2_CAR2CFG,
							0x76);
				}
			} else {
				aclc = FE_STV0900_GetOptimCarrierLoop
					(SymbolRate, modcode, pilots, 0x30);
				if (Demod == FE_SAT_DEMOD_1) {
					ChipSetOneRegister(pParams->hDemod,
							RSTV0900_P1_CAR2CFG,
							0x66);

					ChipSetOneRegister(pParams->hDemod,
							RSTV0900_P1_ACLC2S28,
							aclc);
					ChipSetOneRegister(pParams->hDemod,
							RSTV0900_P1_BCLC2S28,
							0x86);

				} else {
					ChipSetOneRegister(pParams->hDemod,
							RSTV0900_P2_CAR2CFG,
							0x66);

					ChipSetOneRegister(pParams->hDemod,
							RSTV0900_P2_ACLC2S28,
							aclc);
					ChipSetOneRegister(pParams->hDemod,
							RSTV0900_P2_BCLC2S28,
							0x86);
				}

			}
		} else {
			/*Pilots OFF */
			if (Demod == FE_SAT_DEMOD_1) {
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P1_ACLC2S28, 0x1B);
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P1_BCLC2S28, 0x8A);

			} else {
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P2_ACLC2S28, 0x1B);
				ChipSetOneRegister(pParams->hDemod,
						RSTV0900_P2_BCLC2S28, 0x8A);
			}

		}
	}
err_chk_ret:
	if (pParams->hDemod->Error)
		error = FE_LLA_I2C_ERROR;

	return error;
}


#ifdef STM_FE_DEFINED_FOR_FUTURE_USE
/***********************************************************************
**FUNCTION	::	FE_STV0900_SetupFSK
**ACTION	::	Setup FSK
**PARAMS IN	::	hChip-> handle to the chip
**			fskt_carrier-> FSK modulator carrier frequency(Hz)
**			fskt_deltaf-> FSK frequency deviation(Hz)
**			fskr_carrier-> FSK demodulator carrier frequency(Hz)
**PARAMS OUT::	NONE
**RETURN	::	Symbol frequency
supprimer les tuner
***********************************************************************/
FE_STV0900_Error_t FE_STV0900_SetupFSK(FE_STV0900_Handle_t Handle,
				       U32 TransmitCarrier, U32 ReceiveCarrier,
				       U32 Deltaf)
{
	FE_STV0900_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0900_InternalParams_t *pParams;
	U32 fskt_carrier, fskt_deltaf, fskr_carrier;

	if ((void *)Handle == NULL)
		return FE_LLA_INVALID_HANDLE;
	pParams = (FE_STV0900_InternalParams_t *) Handle;

	if (pParams->hDemod->Error)
		return FE_LLA_I2C_ERROR;

	ChipSetField(pParams->hDemod, FSTV0900_FSKT_KMOD, 0x7);

	/*Formulat: FSKT_CAR =  2^20*transmit_carrier/MasterClock; */

	fskt_carrier = (TransmitCarrier << 8) / (pParams->MasterClock >> 12);

	ChipSetField(pParams->hDemod, FSTV0900_FSKT_CAR2, ((fskt_carrier >> 16)
				& 0x03));
	ChipSetField(pParams->hDemod, FSTV0900_FSKT_CAR1, ((fskt_carrier >> 8)
				& 0xFF));
	ChipSetField(pParams->hDemod, FSTV0900_FSKT_CAR0, (fskt_carrier &
				0xFF));

	/* Formulat: FSKT_DELTAF =  2^20*fskt_deltaf/MasterClock;       */

	/* 2^20=1048576 */
	fskt_deltaf = (Deltaf << 8) / (pParams->MasterClock >> 12);

	ChipSetField(pParams->hDemod, FSTV0900_FSKT_DELTAF1, ((fskt_deltaf >>
					8) & 0x0F));
	ChipSetField(pParams->hDemod, FSTV0900_FSKT_DELTAF0, (fskt_deltaf &
				0xFF));

	/*FSK Modulator OFF */
	ChipSetOneRegister(pParams->hDemod, RSTV0900_FSKTCTRL, 0x00);
	/*FSK Modulator cpntrol by TX_EN */
	/*ChipSetOneRegister(pParams->hDemod, RSTV0900_FSKTCTRL, 0x00);*/

	ChipSetOneRegister(pParams->hDemod, RSTV0900_FSKRFC2, 0x10);
	/*
	   Formulat: FSKR_CAR =  2^20*receive_carrier/MasterClock;
	 */

	fskr_carrier = (ReceiveCarrier << 8) / (pParams->MasterClock >> 12);

	ChipSetField(pParams->hDemod, FSTV0900_FSKR_CAR2, ((fskr_carrier >> 16)
				& 0x03));
	ChipSetField(pParams->hDemod, FSTV0900_FSKR_CAR1, ((fskr_carrier >> 8)
				& 0xFF));
	ChipSetField(pParams->hDemod, FSTV0900_FSKR_CAR0, (fskr_carrier &
				0xFF));

	ChipSetOneRegister(pParams->hDemod, RSTV0900_GPIO3CFG, 0x20);
	ChipSetOneRegister(pParams->hDemod, RSTV0900_GPIO4CFG, 0x9c);
	ChipSetOneRegister(pParams->hDemod, RSTV0900_GPIO5CFG, 0x9E);

	ChipSetOneRegister(pParams->hDemod, RSTV0900_DISEQCO2CFG, 0x18);
	/*by default, modulator disabled */
	ChipSetOneRegister(pParams->hDemod, RSTV0900_FSKTCTRL, 0x00);
	ChipSetField(pParams->hDemod, FSTV0900_FSKT_MOD_EN, 2);

	ChipSetField(pParams->hDemod, FSTV0900_FSKR_DETSGN, 0x00);
	ChipSetField(pParams->hDemod, FSTV0900_FSKR_OUTSGN, 0x00);
	ChipSetField(pParams->hDemod, FSTV0900_FSKR_KAGC, 0x07);
	/*For  internal uart RXout */
	/* *Rx  FSKRXOUT */
	/*ChipSetOneRegister(pParams->hDemod, RSTV0900_GPIO20CFG, 0x9C); */
	/* *Tx FSKTXIN */
	/*ChipSetOneRegister(pParams->hDemod, RSTV0900_GPIO21CFG, 0x20); */
	/* *Tx Enable  FSKTXEN */
	/*ChipSetOneRegister(pParams->hDemod, RSTV0900_GPIO3CFG, 0x9E); */
	ChipSetOneRegister(pParams->hDemod, RSTV0900_FSKRK1, 0x54);
	ChipSetOneRegister(pParams->hDemod, RSTV0900_FSKRK2, 0x93);

	/*normal mode */
	ChipSetOneRegister(pParams->hDemod, RSTV0900_FSKRAGCR, 0x68);
	ChipSetOneRegister(pParams->hDemod, RSTV0900_FSKRAGC, 0xd2);
	ChipSetOneRegister(pParams->hDemod, RSTV0900_FSKRALPHA, 0x17);
	ChipSetOneRegister(pParams->hDemod, RSTV0900_FSKRPLTH1, 0x90);
	ChipSetOneRegister(pParams->hDemod, RSTV0900_FSKRPLTH0, 0xc);

	ChipSetOneRegister(pParams->hDemod, RSTV0900_FSKRDF1, 0x81);
	ChipSetOneRegister(pParams->hDemod, RSTV0900_FSKRDF0, 0x92);
	ChipSetOneRegister(pParams->hDemod, RSTV0900_FSKRSTEPP, 0x6);
	ChipSetOneRegister(pParams->hDemod, RSTV0900_FSKRSTEPM, 0x4a);
	ChipSetOneRegister(pParams->hDemod, RSTV0900_FSKRDET1, 0x80 /*0x81 */);
	ChipSetOneRegister(pParams->hDemod, RSTV0900_FSKRDET0, 0xdb /*0x4 */);

	ChipSetOneRegister(pParams->hDemod, RSTV0900_FSKRDTH1, 0x0);
	ChipSetOneRegister(pParams->hDemod, RSTV0900_FSKRDTH0, 0x4d);

	ChipSetOneRegister(pParams->hDemod, RSTV0900_FSKRLOSS, 0xb);
	ChipSetOneRegister(pParams->hDemod, RSTV0900_TSTTNR0, 0x84);

	return error;
}

/***********************************************************
**FUNCTION	::	FE_STV0900_EnableFSK
**ACTION	::	Enable - Disable FSK modulator /Demodulator
**PARAMS IN	::	hChip	 -> handle to the chip
**				mod_en	 -> FSK modulator on/off
**				demod_en -> FSK demodulator on/off
**PARAMS OUT::	NONE
**RETURN	::	error
commm setFSK
***********************************************************/
FE_STV0900_Error_t FE_STV0900_EnableFSK(FE_STV0900_Handle_t Handle,
					BOOL EnableModulation,
					BOOL EnableDemodulation)
{
	FE_STV0900_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0900_InternalParams_t *pParams;

	if ((void *)Handle == NULL)
		return FE_LLA_INVALID_HANDLE;
	pParams = (FE_STV0900_InternalParams_t *) Handle;

	if (pParams->hDemod->Error)
		return FE_LLA_I2C_ERROR;

	if (EnableModulation == TRUE)
		ChipSetField(pParams->hDemod, FSTV0900_FSKT_MOD_EN, 1);
	else
		ChipSetField(pParams->hDemod, FSTV0900_FSKT_MOD_EN, 0);

	if (pParams->hDemod->Error)
		/*Check the error at the end of the function */
		error = FE_LLA_I2C_ERROR;

	return error;
}
#endif

/***********************************************************
**FUNCTION	::	FE_STV0900_DiseqcFSKModeSelect
**ACTION	::	switch from diseqC mode to FSK mode
**PARAMS IN	::	hChip	 -> handle to the chip
**				ModeSelect	 -> 0: for FSK, 1:For DiseqC
**PARAMS OUT::	NONE
**RETURN	::	error if any
***********************************************************/
FE_STV0900_Error_t FE_STV0900_DiseqcFSKModeSelect(FE_STV0900_Handle_t Handle,
						  BOOL ModeSelect)
{
	FE_STV0900_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0900_InternalParams_t *pParams;

	if ((void *)Handle == NULL)
		return FE_LLA_INVALID_HANDLE;

	pParams = (FE_STV0900_InternalParams_t *)Handle;

	if (pParams->hDemod->Error)
		return FE_LLA_I2C_ERROR;

	if (ModeSelect == 0) {
		/* ModeSelect=0 for using FSK */
		/*Set the DiseqC2 TX pin to FSK Tx */
		ChipSetOneRegister(pParams->hDemod, RSTV0900_DISEQCO2CFG, 0x18);
		/*Set the Diseqc2 analog pin to FSK Rx */
		ChipSetField(pParams->hDemod, FSTV0900_SEL_FSK, 1);
		/*if u wish to use DISEQC then else case */
	} else {
		/*Set the Diseqc2 Tx pin to DiseqC Tx */
		ChipSetOneRegister(pParams->hDemod, RSTV0900_DISEQCO2CFG, 0x16);
		/*Set the Diseqc2 analog pin to DiseqC Rx */
		ChipSetField(pParams->hDemod, FSTV0900_SEL_FSK, 0);
	}
	if (pParams->hDemod->Error)
		/*Check the error at the end of the function */
		error = FE_LLA_I2C_ERROR;

	return error;
}

/*****************************************************
--FUNCTION	::	FE_STV0900_DiseqcTxInit
--ACTION	::	Set the diseqC Tx mode
--PARAMS IN	::	Handle==>Front End Handle
		::	DiseqCMode==>diseqc Mode : continues tone, 2/3 PWM, 3/3
			PWM , 2/3 envelop or 3/3 envelop.
		::	Demod==>Current demod 1 or 2
--PARAMS OUT::None
--RETURN	::	Error (if any)
--***************************************************/
FE_STV0900_Error_t FE_STV0900_DiseqcInit(FE_STV0900_Handle_t Handle,
					 FE_STV0900_DiseqC_TxMode DiseqCMode,
					 FE_STV0900_DEMOD_t Demod)
{
	FE_STV0900_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0900_InternalParams_t *pParams = NULL;
	S32 modeField = 0, resetField = 0;

	pParams = (FE_STV0900_InternalParams_t *)Handle;
	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:

		if ((pParams->hDemod->Error) || (pParams->hTuner1->Error))
			error = FE_LLA_I2C_ERROR;
		else {
			modeField = FSTV0900_P1_DISTX_MODE;
			resetField = FSTV0900_P1_DISEQC_RESET;

			if ((pParams->hDemod->Error) ||
					(pParams->hTuner1->Error))
				/*Check the error at the end of the function */
				error = FE_LLA_I2C_ERROR;
		}
		break;

	case FE_SAT_DEMOD_2:
		if ((pParams->hDemod->Error) || (pParams->hTuner2->Error))
			error = FE_LLA_I2C_ERROR;
		else {
			modeField = FSTV0900_P2_DISTX_MODE;
			resetField = FSTV0900_P2_DISEQC_RESET;

			if ((pParams->hDemod->Error) ||
					(pParams->hTuner2->Error))
				/*Check the error at the end of the function */
				error = FE_LLA_I2C_ERROR;
		}
		break;
	}

	ChipSetField(pParams->hDemod, modeField, DiseqCMode);

	ChipSetField(pParams->hDemod, resetField, 1);
	ChipSetField(pParams->hDemod, resetField, 0);

	return error;
}
EXPORT_SYMBOL(FE_STV0900_DiseqcInit);
/*****************************************************
--FUNCTION	::	FE_STV0900_DiseqcSend
--ACTION	::	Read receved bytes from DiseqC FIFO
--PARAMS IN	::	Handle	==>	Front End Handle
			::	Data	==> Table of bytes to send.
			::	NbData	==> Number of bytes to send.
			::	Demod	==>	Current demod 1 or 2
--PARAMS OUT::None
--RETURN	::	Error (if any)
--***************************************************/
FE_STV0900_Error_t FE_STV0900_DiseqcSend(FE_STV0900_Handle_t Handle, U8 *Data,
					 U32 NbData, FE_STV0900_DEMOD_t Demod)
{
	FE_STV0900_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0900_InternalParams_t *pParams = NULL;
	S32 i = 0;

	pParams = (FE_STV0900_InternalParams_t *)Handle;
	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		if ((pParams->hDemod->Error) || (pParams->hTuner1->Error))
			return FE_LLA_I2C_ERROR;

		ChipSetField(pParams->hDemod, FSTV0900_P1_DIS_PRECHARGE, 1);
		while (i < NbData) {
			while (ChipGetField(pParams->hDemod,
						FSTV0900_P1_FIFO_FULL))
				;
			/* wait for FIFO empty  */

			/* send byte to FIFO :: WARNING don't use set field!! */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P1_DISTXDATA, Data[i]);
			i++;
		}
		ChipSetField(pParams->hDemod, FSTV0900_P1_DIS_PRECHARGE, 0);

		i = 0;
		while ((ChipGetField(pParams->hDemod, FSTV0900_P1_TX_IDLE) != 1)
				&& (i < 10)) {
			/*wait until the end of diseqc send operation */
			ChipWaitOrAbort(pParams->hDemod, 10);
			i++;
		}

		if ((pParams->hDemod->Error) || (pParams->hTuner1->Error))
			/*Check the error at the end of the function */
			error = FE_LLA_I2C_ERROR;
		break;

	case FE_SAT_DEMOD_2:
		if ((pParams->hDemod->Error) || (pParams->hTuner2->Error))
			return FE_LLA_I2C_ERROR;

		ChipSetField(pParams->hDemod, FSTV0900_P2_DIS_PRECHARGE, 1);
		while (i < NbData) {
			while (ChipGetField(pParams->hDemod,
						FSTV0900_P2_FIFO_FULL))
				;
			/* wait for FIFO empty  */

			/* send byte to FIFO :: WARNING don't use set field!! */
			ChipSetOneRegister(pParams->hDemod,
					RSTV0900_P2_DISTXDATA, Data[i]);
			i++;
		}
		ChipSetField(pParams->hDemod, FSTV0900_P2_DIS_PRECHARGE, 0);

		i = 0;
		while ((ChipGetField(pParams->hDemod, FSTV0900_P2_TX_IDLE) != 1)
				&& (i < 10)) {
			/*wait until the end of diseqc send operation */
			ChipWaitOrAbort(pParams->hDemod, 10);
			i++;
		}

		if ((pParams->hDemod->Error) || (pParams->hTuner2->Error))
			/*Check the error at the end of the function */
			error = FE_LLA_I2C_ERROR;
		break;
	}

	return error;
}
EXPORT_SYMBOL(FE_STV0900_DiseqcSend);

/*****************************************************
--FUNCTION	::	FE_STV0900_DiseqcReceive
--ACTION	::	Read receved bytes from DiseqC FIFO
--PARAMS IN	::	Handle	==>	Front End Handle
--PARAMS OUT::	Data	==> Table of received bytes.
			::	NbData	==> Number of received bytes.
			::	Demod	==>	Current demod 1 or 2
--RETURN	::	Error (if any)
--***************************************************/
FE_STV0900_Error_t FE_STV0900_DiseqcReceive(FE_STV0900_Handle_t Handle,
					    U8 *Data, U32 *NbData,
					    FE_STV0900_DEMOD_t Demod)
{
	FE_STV0900_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0900_InternalParams_t *pParams = NULL;
	S32 i = 0;

	pParams = (FE_STV0900_InternalParams_t *)Handle;
	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		if ((pParams->hDemod->Error) || (pParams->hTuner1->Error))
			return FE_LLA_I2C_ERROR;

		*NbData = 0;
		while ((ChipGetField(pParams->hDemod, FSTV0900_P1_RX_END) != 1)
				&& (i < 10)) {
			ChipWaitOrAbort(pParams->hDemod, 10);
			i++;
		}
		if (ChipGetField(pParams->hDemod, FSTV0900_P1_RX_END)) {
			*NbData = ChipGetField(pParams->hDemod,
					FSTV0900_P1_FIFO_BYTENBR);
			for (i = 0; i < (*NbData); i++)
				Data[i] = ChipGetOneRegister(pParams->hDemod,
						RSTV0900_P1_DISRXDATA);
		}

		if ((pParams->hDemod->Error) || (pParams->hTuner1->Error))
			/*Check the error at the end of the function */
			error = FE_LLA_I2C_ERROR;
		break;

	case FE_SAT_DEMOD_2:
		if ((pParams->hDemod->Error) || (pParams->hTuner2->Error))
			return FE_LLA_I2C_ERROR;

		*NbData = 0;
		while ((ChipGetField(pParams->hDemod, FSTV0900_P2_RX_END) != 1)
				&& (i < 10)) {
			ChipWaitOrAbort(pParams->hDemod, 10);
			i++;
		}

		if (ChipGetField(pParams->hDemod, FSTV0900_P2_RX_END)) {
			*NbData = ChipGetField(pParams->hDemod,
					FSTV0900_P2_FIFO_BYTENBR);
			for (i = 0; i < (*NbData); i++)
				Data[i] = ChipGetOneRegister(pParams->hDemod,
						RSTV0900_P2_DISRXDATA);
		}

		if ((pParams->hDemod->Error) || (pParams->hTuner2->Error))
			/*Check the error at the end of the function */
			error = FE_LLA_I2C_ERROR;
		break;
	}

	return error;
}
EXPORT_SYMBOL(FE_STV0900_DiseqcReceive);
/*****************************************************
--FUNCTION	::	FE_STV0900_Set22KHzContinues
--ACTION	::	enable/disable the diseqc 22Khz continues tone
--PARAMS IN	::	Handle	==>	Front End Handle
			::	Demod	==>	Current demod 1 or 2
--PARAMS OUT::None
--RETURN	::	Error (if any)
--***************************************************/
FE_STV0900_Error_t FE_STV0900_Set22KHzContinues(FE_STV0900_Handle_t Handle,
						BOOL Enable,
						FE_STV0900_DEMOD_t Demod)
{
	FE_STV0900_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0900_InternalParams_t *pParams = NULL;
	S32 modeField, resetField;

	pParams = (FE_STV0900_InternalParams_t *)Handle;
	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		modeField = FSTV0900_P1_DISTX_MODE;
		resetField = FSTV0900_P1_DISEQC_RESET;

		if ((pParams->hDemod->Error) || (pParams->hTuner1->Error))
			error = FE_LLA_I2C_ERROR;

		break;

	case FE_SAT_DEMOD_2:
		modeField = FSTV0900_P2_DISTX_MODE;
		resetField = FSTV0900_P2_DISEQC_RESET;

		if ((pParams->hDemod->Error) || (pParams->hTuner2->Error))
			error = FE_LLA_I2C_ERROR;

		break;
	}

	if (error != FE_LLA_NO_ERROR)
		return error;

	if (Enable == TRUE) {
		ChipSetField(pParams->hDemod, resetField, 0);
		ChipSetField(pParams->hDemod, resetField, 1);
		/*release DiseqC reset to enable the 22KHz tone
		 * */
		ChipSetField(pParams->hDemod, resetField, 0);

		/*Set the DiseqC mode to 22Khz continues tone
		 * */
		ChipSetField(pParams->hDemod, modeField, 0);
	} else {
		ChipSetField(pParams->hDemod, modeField, 0);

		/*maintain the DiseqC reset to disable the
		 * 22KHz tone */
		ChipSetField(pParams->hDemod, resetField, 1);
	}

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

FE_STV0900_Error_t FE_STV0900_BER_Mode(FE_STV0900_Handle_t Handle,
				       FE_STV0900_DEMOD_t Demod,
				       FE_STV0900_BerMeasure_t setBERMeter)
{
	S32 BER_PER_Meter, error_counter;
	U8 set_BER_PER_Meter;
	FE_STV0900_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0900_InternalParams_t *pParams = NULL;

	pParams = (FE_STV0900_InternalParams_t *) Handle;
	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;

	switch (Demod) {
	case FE_SAT_DEMOD_1:
	default:
		BER_PER_Meter = FSTV0900_P1_BERMETER_DATAMODE;
		error_counter = FSTV0900_P1_NUM_EVENT1;
		if ((pParams->hDemod->Error) || (pParams->hTuner1->Error))
			error = FE_LLA_I2C_ERROR;

		break;

	case FE_SAT_DEMOD_2:
		BER_PER_Meter = FSTV0900_P2_BERMETER_DATAMODE;
		error_counter = FSTV0900_P2_NUM_EVENT1;

		if ((pParams->hDemod->Error) || (pParams->hTuner2->Error))
			error = FE_LLA_I2C_ERROR;

		break;
	}

	if (error != FE_LLA_NO_ERROR)
		return error;

	if (setBERMeter == FE_BER_Algo_Appli_Berwindow)
		set_BER_PER_Meter = 0;
	else
		set_BER_PER_Meter = 1;
	/*By default the BER Meter is in PER mode */
	/*Set the BER Meter in BER/PER mode */
	ChipSetField(pParams->hDemod, BER_PER_Meter, set_BER_PER_Meter);
	/*Set the ErrorCOunter in continous mode */
	if (setBERMeter == FE_BER_Algo_Appli_Berwindow)
		/*Set the eror counter in infinite counter mode */
		ChipSetField(pParams->hDemod, error_counter, 1);
	else
		/*Set the eror counter in as in tracking, the Avergae counting
		 * mode */
		ChipSetField(pParams->hDemod, error_counter, 7);

	return error;

}

/*****************************************************
--FUNCTION	::	FE_STV0900_Term
--ACTION	::	Terminate STV0900 chip connection
--PARAMS IN	::	Handle	==>	Front End Handle
--PARAMS OUT::	NONE
--RETURN	::	Error (if any)
--***************************************************/
FE_STV0900_Error_t FE_STV0900_Term(FE_STV0900_Handle_t Handle)
{
	FE_STV0900_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0900_InternalParams_t *pParams = NULL;

	pParams = (FE_STV0900_InternalParams_t *)Handle;

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

#ifdef STM_FE_DEFINED_FOR_FUTURE_USE
/*****************************************************
--FUNCTION	::	FE_STV0900_SetReg
--ACTION	::	write one register
--PARAMS IN	::	Handle	==>	Front End Handle
		::	Reg	==> register Index in the register Map
		::	Val	==> Val to be writen
--PARAMS OUT::	NONE.
--RETURN	::	Error (if any)
--***************************************************/
FE_STV0900_Error_t FE_STV0900_SetReg(FE_STV0900_Handle_t Handle, U16 Reg,
				     U32 Val)
{
	FE_STV0900_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0900_InternalParams_t *pParams =
	    (FE_STV0900_InternalParams_t *) Handle;

	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;

	ChipSetOneRegister(pParams->hDemod, Reg, Val);

	if (pParams->hDemod->Error)
		error = FE_LLA_I2C_ERROR;
	return error;
}

/*****************************************************
--FUNCTION	::	FE_STV0900_GetReg
--ACTION	::	write one register
--PARAMS IN	::	Handle	==>	Front End Handle
		::	Reg		==> register Index in the register Map
--PARAMS OUT	::	Val		==> Read value.
--RETURN	::	Error (if any)
--***************************************************/
FE_STV0900_Error_t FE_STV0900_GetReg(FE_STV0900_Handle_t Handle, U16 Reg,
				     U32 *Val)
{
	FE_STV0900_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0900_InternalParams_t *pParams =
	    (FE_STV0900_InternalParams_t *) Handle;

	if ((pParams == NULL) || (Val == NULL))
		return FE_LLA_INVALID_HANDLE;

	*Val = ChipGetOneRegister(pParams->hDemod, Reg);
	if (pParams->hDemod->Error)
		error = FE_LLA_I2C_ERROR;

	return error;
}

/*****************************************************
--FUNCTION	::	FE_STV0900_SetField
--ACTION	::	write a value to a Field
--PARAMS IN	::	Handle	==>	Front End Handle
		::	Reg		==> Field Index in the register Map
		::	Val	==> Val to be writen
--PARAMS OUT::	NONE.
--RETURN	::	Error (if any)
--***************************************************/
FE_STV0900_Error_t FE_STV0900_SetField(FE_STV0900_Handle_t Handle, U32 Field,
				       S32 Val)
{
	FE_STV0900_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0900_InternalParams_t *pParams =
	    (FE_STV0900_InternalParams_t *) Handle;

	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;

	ChipSetField(pParams->hDemod, Field, Val);
	if (pParams->hDemod->Error)
		error = FE_LLA_I2C_ERROR;

	return error;
}

/*****************************************************
--FUNCTION	::	FE_STV0900_GetField
--ACTION	::	Read A Field
--PARAMS IN	::	Handle	==>	Front End Handle
			::	Field	==> Field Index in the register Map
--PARAMS OUT::	Val		==> Read value.
--RETURN	::	Error (if any)
--***************************************************/
FE_STV0900_Error_t FE_STV0900_GetField(FE_STV0900_Handle_t Handle, U32 Field,
				       S32 *Val)
{
	FE_STV0900_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0900_InternalParams_t *pParams =
	    (FE_STV0900_InternalParams_t *) Handle;

	if ((pParams == NULL) || (Val == NULL))
		return FE_LLA_INVALID_HANDLE;

	*Val = ChipGetField(pParams->hDemod, Field);
	if (pParams->hDemod->Error)
		error = FE_LLA_I2C_ERROR;

	return error;
}
#endif
