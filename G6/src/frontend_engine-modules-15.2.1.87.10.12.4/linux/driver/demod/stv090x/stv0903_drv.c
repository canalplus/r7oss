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

Source file name : stv0903.c
Author :           LLA

903 lla drv file

Date        Modification                                    Name
----        ------------                                    --------
20-Jun-11   Created

************************************************************************/

/* includes ---------------------------------------------------------------- */
/*#ifndef ST_OSLINUX*/
/*      #include <stdlib.h>*/
/*      #include <stdio.h>*/
/*      #include <string.h>*/
/*#endif*/
/*#include <stfe_utilities.h>*/
#include <linux/kernel.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 2, 2)
#include <linux/export.h>
#else
#include <linux/module.h>
#endif
#include <linux/list.h>
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
#include <stm_fe_demod.h>
#include <stm_fe_diseqc.h>
#include <fesat_commlla_str.h>
#include "stv0903_drv.h"
#include <fe_sat_tuner.h>
#include "stv0903_util.h"
#include "stv0903_init.h"
#include <gen_macros.h>
/*#include <globaldefs.h>*/

#ifdef HOST_PC
#ifndef NO_GUI
#include <STV0903_GUI.h>
#include <Appl.h>
#include <Pnl_Report.h>
#include <formatio.h>
#include <UserPar.h>
#endif
#endif

#define STV0903_BLIND_SEARCH_AGC2_TH  700
#define STV0903_BLIND_SEARCH_AGC2_TH_CUT30 1000/*1400*/	/*Blind Optim */

#define STV0903_IQPOWER_THRESHOLD	30

/****************************************************************
 ****************************************************************
 ****************************************************************
 ****
 ***************************************************
 **Static Data**
 ***************************************************
 ****
 ****
 ****************************************************************
 ****************************************************************
 ****************************************************************/

/************************
Current LLA revision
*************************/
static const ST_Revision_t RevisionSTV0903 = "STV0903-LLA_REL_4.4_JUN_18_2010";

/************************
DVBS1 and DSS C/N Look-Up table
*************************/
static FE_STV0903_LOOKUP_t FE_STV0903_S1_CN_LookUp = {

	52,
	{
	 {0, 8917},		/*C/N=-0dB */
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
static FE_STV0903_LOOKUP_t FE_STV0903_S2_CN_LookUp = {
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
static FE_STV0903_LOOKUP_t FE_STV0903_RF_LookUp = {
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

static FE_STV0903_LOOKUP_t FE_STV0903_RF_LookUpLow = {
	5,
	{
	 {-70, 0x94},
	 {-75, 0x55},
	 {-80, 0x33},
	 {-85, 0x20},
	 {-90, 0x18},
	 }
};

struct FE_STV0903_CarLoopOPtim_s {
	FE_STV0903_ModCod_t ModCode;
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

/*********************************************************************
Cut 1.x Tracking carrier loop carrier QPSK 1/2 to 8PSK 9/10 long Frame
*********************************************************************/
static struct FE_STV0903_CarLoopOPtim_s FE_STV0903_S2CarLoop[14] = {
/*Modcod, 2MPon, 2MPoff, 5MPon, 5MPoff, 10MPon, 10MPoff, 20MPon, 20MPoff,
 * 30MPon, 30MPoff*/
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
Cut 2.0 Tracking carrier loop carrier QPSK 1/2 to 8PSK 9/10 long Frame
*********************************************************************/
static struct FE_STV0903_CarLoopOPtim_s FE_STV0903_S2CarLoopCut20[14] = {
/*Modcod, 2MPon, 2MPoff, 5MPon, 5MPoff, 10MPon, 10MPoff, 20MPon, 20MPoff,
 * 30MPon, 30MPoff */
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
	 0x1d}
};

/*********************************************************************
Cut 3.0 Tracking carrier loop carrier QPSK 1/2 to 8PSK 9/10 long Frame
*********************************************************************/
static struct FE_STV0903_CarLoopOPtim_s FE_STV0903_S2CarLoopCut30[14] = {
/*Modcod, 2MPon, 2MPoff, 5MPon, 5MPoff, 10MPon, 10MPoff, 20MPon, 20MPoff,
 * 30MPon, 30MPoff */
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
	 0x1B}
};

/****************************************************************
 ****************************************************************
 ****************************************************************
 ****
 ***************************************************
 **PRIVATE ROUTINES**
 ***************************************************
 ****
 ****
 ****************************************************************
 ****************************************************************
 ****************************************************************/

/*****************************************************
**FUNCTION	::	GetMclkFreq
**ACTION	::	Set the STV0903 master clock frequency
**PARAMS IN	::  hChip		==>	handle to the chip
**			ExtClk		==>	External clock frequency (Hz)
**PARAMS OUT::	NONE
**RETURN	::	Synthesizer frequency (Hz)
*****************************************************/
U32 FE_STV0903_GetMclkFreq(STCHIP_Handle_t hChip, U32 ExtClk)
{
	U32 mclk = 90000000, div = 0, ad_div = 0;

	/*******************************************
		If the Field SELX1RATIO == 1 then
			MasterClock = (M_DIV +1)*Quartz/4
		else
			MasterClock = (M_DIV +1)*Quartz/6
	********************************************/

	div = ChipGetField(hChip, FSTV0903_M_DIV);
	ad_div = ((ChipGetField(hChip, FSTV0903_SELX1RATIO) == 1) ? 4 : 6);

	mclk = (div + 1) * ExtClk / ad_div;

	return mclk;
}

/*****************************************************
**FUNCTION	::	GetErrorCount
**ACTION	::	return the number of errors from a given counter
**PARAMS IN	::  hChip		==>	handle to the chip
**			::	Counter		==>	used counter 1 or 2.
			::
**PARAMS OUT::	NONE
**RETURN	::	Synthesizer frequency (Hz)
*****************************************************/
U32 FE_STV0903_GetErrorCount(STCHIP_Handle_t hChip,
			     FE_STV0903_ERRORCOUNTER Counter)
{
	U32 lsb, msb, hsb, errValue;

	/*Read the Error value */
	switch (Counter) {
	case FE_STV0903_COUNTER1:
	default:
		ChipGetRegisters(hChip, RSTV0903_ERRCNT12, 3);
		hsb = ChipGetFieldImage(hChip, FSTV0903_ERR_CNT12);
		msb = ChipGetFieldImage(hChip, FSTV0903_ERR_CNT11);
		lsb = ChipGetFieldImage(hChip, FSTV0903_ERR_CNT10);
		break;

	case FE_STV0903_COUNTER2:
		ChipGetRegisters(hChip, RSTV0903_ERRCNT22, 3);
		hsb = ChipGetFieldImage(hChip, FSTV0903_ERR_CNT22);
		msb = ChipGetFieldImage(hChip, FSTV0903_ERR_CNT21);
		lsb = ChipGetFieldImage(hChip, FSTV0903_ERR_CNT20);
		break;
	}

	/*Cupute the Error value 3 bytes (HSB,MSB,LSB) */
	errValue = (hsb << 16) + (msb << 8) + (lsb);
	return errValue;
}

/*****************************************************
**FUNCTION	::	STV0903_RepeaterFn  (First repeater )
**ACTION	::	Set the repeater On or OFF
**PARAMS IN	::  hChip		==>	handle to the chip
			::	State		==> repeater state On/Off.
**PARAMS OUT::	NONE
**RETURN	::	Error (if any)
*****************************************************/
#ifdef CHIP_STAPI
STCHIP_Error_t FE_STV0903_RepeaterFn(STCHIP_Handle_t hChip, BOOL State,
				     U8 *Buffer)
#else
STCHIP_Error_t FE_STV0903_RepeaterFn(STCHIP_Handle_t hChip, BOOL State)
#endif
{
	STCHIP_Error_t error = CHIPERR_NO_ERROR;

	if (hChip != NULL) {
		if (State == TRUE)
#ifdef CHIP_STAPI
			ChipFillRepeaterMessage(hChip, FSTV0903_I2CT_ON, 1,
						Buffer);
#else
			ChipSetField(hChip, FSTV0903_I2CT_ON, 1);
#endif
	}

	return error;
}

/*****************************************************
--FUNCTION	::	CarrierWidth
--ACTION	::	Compute the width of the carrier
--PARAMS IN	::	SymbolRate->Symbol rate of carrier (Kbauds or Mbauds)
--			RollOff		->	Rolloff * 100
--PARAMS OUT::	NONE
--RETURN	::	Width of the carrier (KHz or MHz)
--***************************************************/
U32 FE_STV0903_CarrierWidth(U32 SymbolRate, FE_STV0903_RollOff_t RollOff)
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

void FE_STV0903_SetTS_Parallel_Serial(STCHIP_Handle_t hChip,
				      FE_STV0903_Clock_t PathTS)
{

	switch (PathTS) {
	case FE_TS_SERIAL_PUNCT_CLOCK:
		/*Serial mode */
		ChipSetFieldImage(hChip, FSTV0903_TSFIFO_SERIAL, 0x01);
		/* pucntured clock */
		ChipSetFieldImage(hChip, FSTV0903_TSFIFO_DVBCI, 0);
		break;

	case FE_TS_SERIAL_CONT_CLOCK:
		/*Serial mode */
		ChipSetFieldImage(hChip, FSTV0903_TSFIFO_SERIAL, 0x01);
		/* continues clock */
		ChipSetFieldImage(hChip, FSTV0903_TSFIFO_DVBCI, 1);
		break;

	case FE_TS_PARALLEL_PUNCT_CLOCK:
		/*Parallel mode */
		ChipSetFieldImage(hChip, FSTV0903_TSFIFO_SERIAL, 0x00);
		/* pucntured clock */
		ChipSetFieldImage(hChip, FSTV0903_TSFIFO_DVBCI, 0);
		break;

	case FE_TS_DVBCI_CLOCK:
		/*Parallel mode */
		ChipSetFieldImage(hChip, FSTV0903_TSFIFO_SERIAL, 0x00);
		/* continues clock */
		ChipSetFieldImage(hChip, FSTV0903_TSFIFO_DVBCI, 1);
		break;

	default:
		break;
	}
	ChipSetRegisters(hChip, RSTV0903_TSCFGH, 1);

}

/*****************************************************
--FUNCTION	::	TunerSetType
--ACTION	::	Set the STV0903 Tuner controller
--PARAMS IN	::	Chip Handel
--		::	Tuner Type (control type SW, AUTO 6000 ,AUTO 6100 or
			AUTO 6110
		::
--PARAMS OUT::	NONE
--RETURN	::
--***************************************************/
void FE_STV0903_SetTunerType(STCHIP_Handle_t hChip,
			     FE_STV0903_Tuner_t TunerType,
			     U8 I2cAddr, U32 Reference, U8 OutClkDiv)
{

	switch (TunerType) {
	case FE_SAT_AUTO_STB6000:

		ChipSetOneRegister(hChip, RSTV0903_TNRCFG, 0x1c);
		ChipSetOneRegister(hChip, RSTV0903_TNRCFG2, 0x86);
		ChipSetOneRegister(hChip, RSTV0903_TNRCFG3, 0x18);
		ChipSetOneRegister(hChip, RSTV0903_TNRXTAL,
				   (Reference / 1000000));
		ChipSetOneRegister(hChip, RSTV0903_TNRSTEPS, 0x05);
		ChipSetOneRegister(hChip, RSTV0903_TNRGAIN, 0x17);
		ChipSetOneRegister(hChip, RSTV0903_TNRADJ, 0x1f);
		ChipSetOneRegister(hChip, RSTV0903_TNRCTL2, 0x0);

		ChipSetField(hChip, FSTV0903_TUN_TYPE, 1);

		/*Set the ADC range to 2Vpp */
		ChipSetOneRegister(hChip, RSTV0903_TSTTNR1, 0x27);
		break;

	case FE_SAT_AUTO_STB6100:
		ChipSetOneRegister(hChip, RSTV0903_TNRCFG, 0x3c);
		ChipSetOneRegister(hChip, RSTV0903_TNRCFG2, 0x86);
		ChipSetOneRegister(hChip, RSTV0903_TNRCFG3, 0x18);
		ChipSetOneRegister(hChip, RSTV0903_TNRXTAL,
				   (Reference / 1000000));
		ChipSetOneRegister(hChip, RSTV0903_TNRSTEPS, 0x05);
		ChipSetOneRegister(hChip, RSTV0903_TNRGAIN, 0x17);
		ChipSetOneRegister(hChip, RSTV0903_TNRADJ, 0x1f);
		ChipSetOneRegister(hChip, RSTV0903_TNRCTL2, 0x0);

		ChipSetField(hChip, FSTV0903_TUN_TYPE, 3);

		/*Set the ADC range to 2Vpp */
		ChipSetOneRegister(hChip, RSTV0903_TSTTNR1, 0x27);
		break;

	case FE_SAT_AUTO_STV6110:

		ChipSetOneRegister(hChip, RSTV0903_TNRCFG, 0x4c);
		ChipSetOneRegister(hChip, RSTV0903_TNRCFG2, 0x06);
		ChipSetOneRegister(hChip, RSTV0903_TNRCFG3, 0x18);
		ChipSetOneRegister(hChip, RSTV0903_TNRXTAL,
				   (Reference / 1000000));
		ChipSetOneRegister(hChip, RSTV0903_TNRSTEPS, 0x05);
		ChipSetOneRegister(hChip, RSTV0903_TNRGAIN, 0x41);
		ChipSetOneRegister(hChip, RSTV0903_TNRADJ, 0);
		ChipSetOneRegister(hChip, RSTV0903_TNRCTL2, 0x97);

		ChipSetField(hChip, FSTV0903_TUN_TYPE, 4);

		switch (OutClkDiv) {
		case 1:
		default:
			ChipSetField(hChip, FSTV0903_TUN_KDIVEN, 0);
			break;

		case 2:
			ChipSetField(hChip, FSTV0903_TUN_KDIVEN, 1);
			break;

		case 4:
			ChipSetField(hChip, FSTV0903_TUN_KDIVEN, 2);
			break;

		case 8:
			ChipSetField(hChip, FSTV0903_TUN_KDIVEN, 3);
			break;
		}

		/*Set the ADC range to 1Vpp */
		ChipSetOneRegister(hChip, RSTV0903_TSTTNR1, 0x26);
		break;

	case FE_SAT_SW_TUNER:
	default:
		ChipSetField(hChip, FSTV0903_TUN_TYPE, 6);
		break;

	}

	switch (I2cAddr) {
	case 0xC0:
	default:
		ChipSetField(hChip, FSTV0903_TUN_MADDRESS, 0);
		break;

	case 0xC2:
		ChipSetField(hChip, FSTV0903_TUN_MADDRESS, 1);
		break;

	case 0xC4:
		ChipSetField(hChip, FSTV0903_TUN_MADDRESS, 2);
		break;

	case 0xC6:
		ChipSetField(hChip, FSTV0903_TUN_MADDRESS, 3);
		break;
	}
	ChipSetOneRegister(hChip, RSTV0903_TNRLD, 1);
}

/*****************************************************
--FUNCTION	::	TunerSetBBGain
--ACTION	::	Set the Tuner bbgain using the 900 tuner controller
--PARAMS IN	::	Chip Handel
--		::	Tuner Type (control type SW, AUTO 6100 ,AUTO 600 or
			AUTO SATTUNER
		::  bbgain in dB
--PARAMS OUT::	NONE
--RETURN	::
--***************************************************/
void FE_STV0903_TunerSetBBGain(STCHIP_Handle_t hChip,
			       FE_STV0903_Tuner_t TunerType, S32 BBGain)
{

	switch (TunerType) {
	case FE_SAT_AUTO_STB6000:
	case FE_SAT_AUTO_STB6100:
		ChipSetField(hChip, FSTV0903_TUN_GAIN, (BBGain / 2) + 7);
		ChipSetOneRegister(hChip, RSTV0903_TNRLD, 1);

		break;

	case FE_SAT_AUTO_STV6110:
		ChipSetField(hChip, FSTV0903_TUN_GAIN, (BBGain / 2));
		ChipSetOneRegister(hChip, RSTV0903_TNRLD, 1);

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
			::
--PARAMS OUT::	NONE
--RETURN	::	Tuner Frequency  in KHz.
--***************************************************/
U32 FE_STV0903_GetTunerFrequency(STCHIP_Handle_t hChip,
				 TUNER_Handle_t hTuner,
				 FE_STV0903_Tuner_t TunerType)
{
	U32 tunerFrequency;

	S32 freq, round;

	switch (TunerType) {
	case FE_SAT_AUTO_STB6000:
	case FE_SAT_AUTO_STB6100:
	case FE_SAT_AUTO_STV6110:

		/*      Formulat :
		   Tuner_Frequency(MHz) = Regs /64
		   Tuner_granularity(MHz)       = Regs  /2048

		   real_Tuner_Frequency = Tuner_Frequency(MHz) +
		   Tuner_granularity(MHz)
		 */
		ChipGetRegisters(hChip, RSTV0903_TNRRF1, 3);
		freq = (ChipGetFieldImage(hChip, FSTV0903_TUN_RFFREQ2) << 10) +
		    (ChipGetFieldImage(hChip, FSTV0903_TUN_RFFREQ1) << 2) +
		    ChipGetFieldImage(hChip, FSTV0903_TUN_RFFREQ0);

		/* equivalent to freq=(freq *1000)/64; */
		freq = (freq * 1000) >> 6;

		ChipGetRegisters(hChip, RSTV0903_TNROBSL, 2);
		round = (ChipGetFieldImage(hChip, FSTV0903_TUN_RFRESTE1) / 4) +
		    ChipGetFieldImage(hChip, FSTV0903_TUN_RFRESTE0);

		round = (round * 1000) >> 11;

		tunerFrequency = freq - round;

		if (ChipGetField(hChip, FSTV0903_TUN_ACKFAIL)) {
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
			::	Frequency (Khz)
			::	Bandwidth (Hz)
--PARAMS OUT::	NONE
--RETURN	::	None.
--***************************************************/
void FE_STV0903_SetTuner(STCHIP_Handle_t hChip,
			 TUNER_Handle_t hTuner,
			 FE_STV0903_Tuner_t TunerType,
			 U32 Frequency, U32 Bandwidth)
{
	U32 tunerFrequency;

	switch (TunerType) {
	case FE_SAT_AUTO_STB6000:
	case FE_SAT_AUTO_STB6100:
	case FE_SAT_AUTO_STV6110:

		/*      Formulat:
		   Tuner_frequency_reg= Frequency(MHz)*64
		 */
		/* equivalent to tunerFrequency=(Frequency*64)/1000; */
		tunerFrequency = (Frequency << 6) / 1000;

		ChipSetFieldImage(hChip, FSTV0903_TUN_RFFREQ2,
				  (tunerFrequency >> 10));
		ChipSetFieldImage(hChip, FSTV0903_TUN_RFFREQ1,
				  (tunerFrequency >> 2) & 0xff);
		ChipSetFieldImage(hChip, FSTV0903_TUN_RFFREQ0,
				  (tunerFrequency & 0x03));
		ChipSetRegisters(hChip, RSTV0903_TNRRF1, 3);
		/*Low Pass Filter = BW /2 (MHz) */
		ChipSetField(hChip, FSTV0903_TUN_BW, Bandwidth / 2000000);
		/*Tuner Write trig      */
		ChipSetOneRegister(hChip, RSTV0903_TNRLD, 1);

		ChipWaitOrAbort(hChip, 2);
		if (ChipGetField(hChip, FSTV0903_TUN_ACKFAIL)) {
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
			::
--PARAMS OUT::	NONE
--RETURN	::	Boolean (tuner Lock status).
--***************************************************/
BOOL FE_STV0903_GetTunerStatus(STCHIP_Handle_t hChip,
			       TUNER_Handle_t hTuner,
			       FE_STV0903_Tuner_t TunerType)
{

	BOOL lock;

	switch (TunerType) {
	case FE_SAT_AUTO_STB6000:
	case FE_SAT_AUTO_STB6100:
	case FE_SAT_AUTO_STV6110:
		/* case of automatic controled tuner
		   Read the tuner lock flag from the STV0903
		 */
		if (ChipGetField(hChip, FSTV0903_TUN_I2CLOCKED) == 1)
			lock = TRUE;
		else
			lock = FALSE;

		if (ChipGetField(hChip, FSTV0903_TUN_ACKFAIL)) {
			if (hTuner != NULL)
				hTuner->Error = CHIPERR_I2C_NO_ACK;
		}

		break;

	case FE_SAT_SW_TUNER:
	default:
		/*      case of SW controled tuner
		   Read the lock flag from the tuner (SW Tuner API )
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
			::
--PARAMS OUT::	NONE
--RETURN	::	Frequency offset in Hz.
--***************************************************/
S32 FE_STV0903_GetCarrierFrequency(STCHIP_Handle_t hChip, U32 MasterClock)
{
	S32 derot, rem1, rem2, intval1, intval2, carrierFrequency;

	/*      Read the carrier frequency regs value   */

	ChipGetRegisters(hChip, RSTV0903_CFR2, 3);
	derot = (ChipGetFieldImage(hChip, FSTV0903_CAR_FREQ2) << 16) +
	    (ChipGetFieldImage(hChip, FSTV0903_CAR_FREQ1) << 8) +
	    (ChipGetFieldImage(hChip, FSTV0903_CAR_FREQ0));

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

S32 FE_STV0903_TimingGetOffset(STCHIP_Handle_t hChip, U32 SymbolRate)
{
	S32 timingoffset;

	/*      Formulat :
	   SR_Offset = TMGRREG * SR /2^29
	   TMGREG is 3 bytes registers value
	   SR is the current symbol rate
	 */
	ChipGetRegisters(hChip, RSTV0903_TMGREG2, 3);

	timingoffset = (ChipGetFieldImage(hChip, FSTV0903_TMGREG2) << 16) +
	    (ChipGetFieldImage(hChip, FSTV0903_TMGREG1) << 8) +
	    (ChipGetFieldImage(hChip, FSTV0903_TMGREG0));

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
		::
--PARAMS OUT::	NONE
--RETURN	::	Symbol rate in Symbol/s
*****************************************************/
U32 FE_STV0903_GetSymbolRate(STCHIP_Handle_t hChip, U32 MasterClock)
{
	S32 rem1, rem2, intval1, intval2, symbolRate;

	ChipGetRegisters(hChip, RSTV0903_SFR3, 4);
	symbolRate = (ChipGetFieldImage(hChip, FSTV0903_SYMB_FREQ3) << 24) +
	    (ChipGetFieldImage(hChip, FSTV0903_SYMB_FREQ2) << 16) +
	    (ChipGetFieldImage(hChip, FSTV0903_SYMB_FREQ1) << 8) +
	    (ChipGetFieldImage(hChip, FSTV0903_SYMB_FREQ0));

	/*      Formulat :
	   Found_SR = Reg * MasterClock /2^32
	 */

	intval1 = (MasterClock) >> 16;
	intval2 = (symbolRate) >> 16;

	rem1 = (MasterClock) % 0x10000;
	rem2 = (symbolRate) % 0x10000;

	/*only for integer calculation */
	symbolRate = (intval1 * intval2) + ((intval1 * rem2) >> 16) +
						((intval2 * rem1) >> 16);

	return symbolRate;

}

/*****************************************************
--FUNCTION	::	SetSymbolRate
--ACTION	::	Get the Symbol rate
--PARAMS IN	::	hChip		->	handle to the chip
		::	MasterClock	->	Masterclock frequency (Hz)
		::	SymbolRate	->	Symbol Rate (Symbol/s)
			::
--PARAMS OUT::	NONE
--RETURN	::	None
*****************************************************/
void FE_STV0903_SetSymbolRate(STCHIP_Handle_t hChip, U32 MasterClock,
			      U32 SymbolRate)
{
	U32 symb;

	if (SymbolRate > 60000000) {	/*SR >60Msps */

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

	/* update the MSB */
	ChipSetOneRegister(hChip, RSTV0903_SFRINIT1, (symb >> 8) & 0x7F);
	/* update the LSB */
	ChipSetOneRegister(hChip, RSTV0903_SFRINIT0, (symb & 0xFF));
}

/*****************************************************
--FUNCTION	::	SetMaxSymbolRate
--ACTION	::	Get the Symbol rate
--PARAMS IN	::	hChip		->	handle to the chip
		::	MasterClock	->	Masterclock frequency (Hz)
		::	SymbolRate	->	Symbol Rate Max to SR + 5%
		::
--PARAMS OUT::	NONE
--RETURN	::	None
*****************************************************/
void FE_STV0903_SetMaxSymbolRate(STCHIP_Handle_t hChip, U32 MasterClock,
				 U32 SymbolRate)
{
	U32 symb;

	SymbolRate = 105 * (SymbolRate / 100);

	if (SymbolRate > 60000000) {	/*SR >60Msps */

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
		/* update the MSB */
		ChipSetOneRegister(hChip, RSTV0903_SFRUP1, (symb >> 8) & 0x7F);
		/* update the LSB */
		ChipSetOneRegister(hChip, RSTV0903_SFRUP0, (symb & 0xFF));
	} else {
		/* update the MSB */
		ChipSetOneRegister(hChip, RSTV0903_SFRUP1, 0x7F);
		/* update the LSB */
		ChipSetOneRegister(hChip, RSTV0903_SFRUP0, 0xFF);
	}

}

/*****************************************************
--FUNCTION	::	SetMinSymbolRate
--ACTION	::	Get the Symbol rate
--PARAMS IN	::	hChip->	handle to the chip
		::	MasterClock->	Masterclock frequency (Hz)
		::	SymbolRate->	Symbol Rate Min to SR - 5% (Symbol/s)
		::
--PARAMS OUT::	NONE
--RETURN	::	None
*****************************************************/
void FE_STV0903_SetMinSymbolRate(STCHIP_Handle_t hChip, U32 MasterClock,
				 U32 SymbolRate)
{
	U32 symb;

	SymbolRate = 95 * (SymbolRate / 100);

	if (SymbolRate > 60000000) {	/*SR >60Msps */

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

	/* update the MSB */
	ChipSetOneRegister(hChip, RSTV0903_SFRLOW1, (symb >> 8) & 0x7F);
	/* update the LSB */
	ChipSetOneRegister(hChip, RSTV0903_SFRLOW0, (symb & 0xFF));
}

/*****************************************************
--FUNCTION	::	GetStandard
--ACTION	::	Return the current standrad (DVBS1,DSS or DVBS2
--PARAMS IN	::	hChip		->	handle to the chip
			::
--PARAMS OUT::	NONE
--RETURN	::	standard (DVBS1, DVBS2 or DSS
*****************************************************/
FE_STV0903_TrackingStandard_t FE_STV0903_GetStandard(STCHIP_Handle_t hChip)
{
	FE_STV0903_TrackingStandard_t foundStandard;
	S32 state;

	state = ChipGetField(hChip, FSTV0903_HEADER_MODE);

	if (state == 2)
		/* The demod Find DVBS2 */
		foundStandard = FE_SAT_DVBS2_STANDARD;

	else if (state == 3) {	/* The demod Find DVBS1/DSS */
		if (ChipGetField(hChip, FSTV0903_DSS_DVB) == 1)
			/* Viterbi Find DSS */
			foundStandard = FE_SAT_DSS_STANDARD;
		else
			/* Viterbi Find DVBS1 */
			foundStandard = FE_SAT_DVBS1_STANDARD;
	} else
		/* The demod find nothing, unknown standard        */
		foundStandard = FE_SAT_UNKNOWN_STANDARD;
	return foundStandard;
}

/*****************************************************
--FUNCTION	::	GetRFLevel
--ACTION	::	Return power of the signal
--PARAMS IN	::	hChip	->	handle to the chip
			::	lookup	->	LUT for RF level estimation.
			::
--PARAMS OUT::	NONE
--RETURN	::	Power of the signal (dBm), -100 if no signal
--***************************************************/
S32 FE_STV0903_GetRFLevel(STCHIP_Handle_t hChip, FE_STV0903_LOOKUP_t *lookup)
{
	S32 agcGain = 0, iqpower = 0, Imin, Imax, i, rfLevel = 0;

	if ((lookup == NULL) || !lookup->size)
		return rfLevel;
	/* Read the AGC integrator registers (2 Bytes)  */
	ChipGetRegisters(hChip, RSTV0903_AGCIQIN1, 2);
	agcGain = MAKEWORD(ChipGetFieldImage(hChip, FSTV0903_AGCIQ_VALUE1),
			ChipGetFieldImage(hChip, FSTV0903_AGCIQ_VALUE0));
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

		rfLevel = (((S32) agcGain - lookup->table[Imin].regval)
			   * (lookup->table[Imax].realval -
			      lookup->table[Imin].realval)
			   / (lookup->table[Imax].regval -
			      lookup->table[Imin].regval))
		    + lookup->table[Imin].realval;
		return rfLevel;
	}

	if (iqpower > FE_STV0903_RF_LookUpLow.table[0].regval)
		rfLevel = FE_STV0903_RF_LookUpLow.table[0].regval;
	else if (iqpower >=
		 FE_STV0903_RF_LookUpLow.table[lookup->size - 1].regval) {
		while ((Imax - Imin) > 1) {
			i = (Imax + Imin) >> 1;
			/*equivalent to i=(Imax+Imin)/2; */
			if (INRANGE(FE_STV0903_RF_LookUpLow.table[Imin].regval,
			      iqpower, FE_STV0903_RF_LookUpLow.table[i].regval))
				Imax = i;
			else
				Imin = i;
		}

		rfLevel = (((S32)iqpower -
				FE_STV0903_RF_LookUpLow.table[Imin].regval)
		     * (FE_STV0903_RF_LookUpLow.table[Imax].realval -
		      FE_STV0903_RF_LookUpLow.table[Imin].realval) /
		     (FE_STV0903_RF_LookUpLow.table[Imax].regval -
		      FE_STV0903_RF_LookUpLow.table[Imin].regval)) +
			FE_STV0903_RF_LookUpLow.table[Imin].realval;
	} else
		rfLevel =
		    FE_STV0903_RF_LookUpLow.table[lookup->size - 1].realval;
	return rfLevel;
}

/*****************************************************
--FUNCTION	::	CarrierGetQuality
--ACTION	::	Return the carrier to noise of the current carrier
--PARAMS IN	::	hChip	->	handle to the chip
			::	lookup	->	LUT for CNR level estimation.
--PARAMS OUT::	NONE
--RETURN	::	C/N of the carrier, 0 if no carrier
--***************************************************/
S32 FE_STV0903_CarrierGetQuality(STCHIP_Handle_t hChip,
				 FE_STV0903_LOOKUP_t *lookup)
{
	S32 c_n = -100,
	    regval,
	    Imin, Imax, i, lockFlagField, noiseField1, noiseField0, noiseReg;

	lockFlagField = FSTV0903_LOCK_DEFINITIF;
	if (FE_STV0903_GetStandard(hChip) == FE_SAT_DVBS2_STANDARD) {
		/*If DVBS2 use PLH normilized noise indicators */
		noiseField1 = FSTV0903_NOSPLHT_NORMED1;
		noiseField0 = FSTV0903_NOSPLHT_NORMED0;
		noiseReg = RSTV0903_NNOSPLHT1;
	} else {
		/*if not DVBS2 use symbol normalized noise indicators */
		noiseField1 = FSTV0903_NOSDATAT_NORMED1;
		noiseField0 = FSTV0903_NOSDATAT_NORMED0;
		noiseReg = RSTV0903_NNOSDATAT1;
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
			/*equivalent to i=(Imax+Imin)/2; */
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
--PARAMS OUT::	NONE
--RETURN	::	ber/per scalled to 1e7
--***************************************************/
U32 FE_STV0903_GetBer(STCHIP_Handle_t hChip)
{
	U32 ber = 10000000, i;
	S32 demodState;

	demodState = ChipGetField(hChip, FSTV0903_HEADER_MODE);

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
			   FE_STV0903_GetErrorCount(hChip, FE_STV0903_COUNTER1);
		}

		ber /= 5;

		if (ChipGetField(hChip, FSTV0903_PRFVIT)) {/*Check for carrier*/
			/* Error Rate */
			ber *= 9766;	/*ber = ber * 10^7/2^10 */
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
			   FE_STV0903_GetErrorCount(hChip, FE_STV0903_COUNTER1);
		}
		ber /= 5;

		/* Check for S2 FEC Lock*/
		if (ChipGetField(hChip, FSTV0903_PKTDELIN_LOCK)) {
			ber *= 9766;	/*      ber = ber * 10^7/2^10 */
			/*  theses two lines => PER = ber * 10^7/2^23   */
			ber = ber >> 13;
		}

		break;
	}

	return ber;
}

FE_STV0903_Rate_t FE_STV0903_GetPunctureRate(STCHIP_Handle_t hChip)
{
	S32 rateField;
	FE_STV0903_Rate_t punctureRate;

	rateField = ChipGetField(hChip, FSTV0903_VIT_CURPUN);
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

void FE_STV0903_GetLockTimeout(S32 *DemodTimeout, S32 *FecTimeout,
			       S32 SymbolRate, FE_STV0903_SearchAlgo_t Algo)
{
	switch (Algo) {
	case FE_SAT_BLIND_SEARCH:

		if (SymbolRate <= 1500000) {	/*10Msps< SR <=15Msps */
			(*DemodTimeout) = 1500;
			(*FecTimeout) = 400;
		} else if (SymbolRate <= 5000000) {	/*10Msps< SR <=15Msps */
			(*DemodTimeout) = 1000;
			(*FecTimeout) = 300;
		} else if (SymbolRate <= 30000000) {	/*5Msps< SR <=30Msps */
			(*DemodTimeout) = 700;
			(*FecTimeout) = 300;
		} else if (SymbolRate <= 45000000) {	/*30Msps< SR <=45Msps */
			(*DemodTimeout) = 400;
			(*FecTimeout) = 200;
		} else {	/*SR >45Msps */

			(*DemodTimeout) = 200;
			(*FecTimeout) = 100;
		}

		break;

	case FE_SAT_COLD_START:
	case FE_SAT_WARM_START:
	default:
		if (SymbolRate <= 1000000) {	/*SR <=1Msps */
			(*DemodTimeout) = 3000;
			(*FecTimeout) = 1700;
		} else if (SymbolRate <= 2000000) {	/*1Msps < SR <=2Msps */
			(*DemodTimeout) = 2500;
			(*FecTimeout) = 1100;
		} else if (SymbolRate <= 5000000) {	/*2Msps< SR <=5Msps */
			(*DemodTimeout) = 1000;
			(*FecTimeout) = 550;
		} else if (SymbolRate <= 10000000) {	/*5Msps< SR <=10Msps */
			(*DemodTimeout) = 700;
			(*FecTimeout) = 250;
		} else if (SymbolRate <= 20000000) {	/*10Msps< SR <=20Msps */
			(*DemodTimeout) = 400;
			(*FecTimeout) = 130;
		}

		else {		/*SR >20Msps */

			(*DemodTimeout) = 300;
			(*FecTimeout) = 100;
		}
		break;

	}
	if (Algo == FE_SAT_WARM_START)
		/*if warm start
		   demod timeout = coldtimeout/3
		   fec timeout = same as cold */
		(*DemodTimeout) /= 2;

}

BOOL FE_STV0903_CheckTimingLock(STCHIP_Handle_t hChip)
{
	BOOL timingLock = FALSE;
	S32 i, timingcpt = 0;
	U8 carFreq, tmgTHhigh, tmgTHLow;

	carFreq = ChipGetOneRegister(hChip, RSTV0903_CARFREQ);
	tmgTHhigh = ChipGetOneRegister(hChip, RSTV0903_TMGTHRISE);
	tmgTHLow = ChipGetOneRegister(hChip, RSTV0903_TMGTHFALL);

	/*use higher timing lock TH */
	ChipSetOneRegister(hChip, RSTV0903_TMGTHRISE, 0x20);
	/*use higher timing lock TH */
	ChipSetOneRegister(hChip, RSTV0903_TMGTHFALL, 0);

	/* stop the carrier offset search */
	ChipSetField(hChip, FSTV0903_CFR_AUTOSCAN, 0);
	/*set the DVBS1 timing loop beta value to 0 (stop timing loop) */
	ChipSetOneRegister(hChip, RSTV0903_RTC, 0x80);
	/*set the DVBS2 timing loop beta value to 0 (stop timing loop) */
	ChipSetOneRegister(hChip, RSTV0903_RTCS2, 0x40);
	/*Stop the carrier loop */
	ChipSetOneRegister(hChip, RSTV0903_CARFREQ, 0x0);

	/*set the carrier offset to 0 */
	ChipSetOneRegister(hChip, RSTV0903_CFRINIT1, 0x0);
	/*set the carrier offset to 0 */
	ChipSetOneRegister(hChip, RSTV0903_CFRINIT0, 0x0);
	ChipSetOneRegister(hChip, RSTV0903_AGC2REF, 0x65);
	/*Trig an acquisition (start the search) only for timing check no need
	 * to white for lock */
	ChipSetOneRegister(hChip, RSTV0903_DMDISTATE, 0x18);
	ChipWaitOrAbort(hChip, 5);
	for (i = 0; i < 10; i++) {
		/*read the timing lock quality */
		if (ChipGetField(hChip, FSTV0903_TMGLOCK_QUALITY) >= 2)
			timingcpt++;
		ChipWaitOrAbort(hChip, 1);
	}
	if (timingcpt >= 3)
		/*timing locked when timing quality is higher than 2 for 3
		 * times */
		timingLock = TRUE;

	ChipSetOneRegister(hChip, RSTV0903_AGC2REF, 0x38);
	/*enable the DVBS1 timing loop */
	ChipSetOneRegister(hChip, RSTV0903_RTC, 0x88);
	/*enable the DVBS2 timing loop */
	ChipSetOneRegister(hChip, RSTV0903_RTCS2, 0x68);

	/*enable the carrier loop */
	ChipSetOneRegister(hChip, RSTV0903_CARFREQ, carFreq);

	/*set back the previous tming TH */
	ChipSetOneRegister(hChip, RSTV0903_TMGTHRISE, tmgTHhigh);
	/*set back the previous tming TH */
	ChipSetOneRegister(hChip, RSTV0903_TMGTHFALL, tmgTHLow);

	return timingLock;

}

BOOL FE_STV0903_GetDemodLock(STCHIP_Handle_t hChip, S32 TimeOut)
{
	S32 timer = 0, lock = 0;

	FE_STV0903_SEARCHSTATE_t demodState;

#ifdef HOST_PC
#ifndef NO_GUI
	double lockTime;
	char message[100];
	lockTime = Timer();

#endif
#endif

	while ((timer < TimeOut) && (lock == 0)) {

		demodState = ChipGetField(hChip, FSTV0903_HEADER_MODE);
		switch (demodState) {
		case FE_SAT_SEARCH:
		case FE_SAT_PLH_DETECTED:	/* no signal */
		default:
			lock = 0;
			break;

		case FE_SAT_DVBS2_FOUND:	/*found a DVBS2 signal */
		case FE_SAT_DVBS_FOUND:
			lock = ChipGetField(hChip, FSTV0903_LOCK_DEFINITIF);
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

BOOL GetFECLock(STCHIP_Handle_t hChip, S32 TimeOut)
{
	S32 timer = 0, lock = 0;

	FE_STV0903_SEARCHSTATE_t demodState;
#ifdef HOST_PC
#ifndef NO_GUI
	double lockTime;
	char message[100];
	lockTime = Timer();

#endif
#endif

	demodState = ChipGetField(hChip, FSTV0903_HEADER_MODE);
	while ((timer < TimeOut) && (lock == 0)) {

		switch (demodState) {
		case FE_SAT_SEARCH:
		case FE_SAT_PLH_DETECTED:	/* no signal */
		default:
			lock = 0;
			break;

		case FE_SAT_DVBS2_FOUND:	/*found a DVBS2 signal */
			lock = ChipGetField(hChip, FSTV0903_PKTDELIN_LOCK);
			break;

		case FE_SAT_DVBS_FOUND:
			lock = ChipGetField(hChip, FSTV0903_LOCKEDVIT);
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
BOOL FE_STV0903_WaitForLock(STCHIP_Handle_t hChip, S32 DemodTimeOut,
			    S32 FecTimeOut)
{

	S32 timer = 0, lock = 0;

#ifdef HOST_PC
#ifndef NO_GUI
	double lockTime;
	char message[100];
	lockTime = Timer();

#endif
#endif

	lock = FE_STV0903_GetDemodLock(hChip, DemodTimeOut);
	if (lock)
		lock = lock && GetFECLock(hChip, FecTimeOut);
	if (lock) {
		lock = 0;
		while ((timer < FecTimeOut) && (lock == 0)) {
			/*Check the stream merger Lock (good packet at the
			 * output) */
			lock = ChipGetField(hChip, FSTV0903_TSFIFO_LINEOK);
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

void FE_STV0903_StopALL_S2_Modcod(STCHIP_Handle_t hChip)
{

	S32 i;

	for (i = 0; i < 16; i++)
		ChipSetOneRegister(hChip, RSTV0903_MODCODLST0 + i, 0xFF);
}

void FE_STV0903_ActivateS2ModCode(STCHIP_Handle_t hChip)
{
	S32 RegIndex;

	for (RegIndex = 0; RegIndex < 8; RegIndex++)
		ChipSetOneRegister(hChip, RSTV0903_MODCODLST7 + RegIndex, 0xcc);
}

void FE_STV0903_SetViterbiThAcq(STCHIP_Handle_t hChip)
{

	ChipSetFieldImage(hChip, FSTV0903_VTH12, 0x96);
	ChipSetFieldImage(hChip, FSTV0903_VTH23, 0x64);
	ChipSetFieldImage(hChip, FSTV0903_VTH34, 0x36);
	ChipSetFieldImage(hChip, FSTV0903_VTH56, 0x23);
	ChipSetFieldImage(hChip, FSTV0903_VTH67, 0x1E);
	ChipSetFieldImage(hChip, FSTV0903_VTH78, 0x19);

	ChipSetRegisters(hChip, RSTV0903_VTH12, 6);

}

void FE_STV0903_SetViterbiThTracq(STCHIP_Handle_t hChip)
{
	ChipSetFieldImage(hChip, FSTV0903_VTH12, 0xd0);
	ChipSetFieldImage(hChip, FSTV0903_VTH23, 0x7D);
	ChipSetFieldImage(hChip, FSTV0903_VTH34, 0x53);
	ChipSetFieldImage(hChip, FSTV0903_VTH56, 0x2F);
	ChipSetFieldImage(hChip, FSTV0903_VTH67, 0x24);
	ChipSetFieldImage(hChip, FSTV0903_VTH78, 0x1F);

	ChipSetRegisters(hChip, RSTV0903_VTH12, 6);

}

void FE_STV0903_SetViterbiStandard(STCHIP_Handle_t hChip,
				   FE_STV0903_SearchStandard_t Standard,
				   FE_STV0903_Rate_t PunctureRate)
{
	switch (Standard) {
	case FE_SAT_AUTO_SEARCH:
		/* Enable only DVBS1 ,DSS seach mast be made on demande by
		 * giving Std DSS */
		ChipSetOneRegister(hChip, RSTV0903_FECM, 0x00);
		/* Enable all PR exept 6/7 */
		ChipSetOneRegister(hChip, RSTV0903_PRVIT, 0x2F);
		break;

	case FE_SAT_SEARCH_DVBS1:
		/* Disable DSS */
		ChipSetOneRegister(hChip, RSTV0903_FECM, 0x00);
		switch (PunctureRate) {
		case FE_SAT_PR_UNKNOWN:
		default:
			/* Enable All PR */
			ChipSetOneRegister(hChip, RSTV0903_PRVIT, 0x2F);
			break;

		case FE_SAT_PR_1_2:
			/* Enable 1/2 PR only */
			ChipSetOneRegister(hChip, RSTV0903_PRVIT, 0x01);
			break;

		case FE_SAT_PR_2_3:
			/* Enable 2/3 PR only */
			ChipSetOneRegister(hChip, RSTV0903_PRVIT, 0x02);
			break;

		case FE_SAT_PR_3_4:
			/* Enable 3/4 PR only */
			ChipSetOneRegister(hChip, RSTV0903_PRVIT, 0x04);
			break;

		case FE_SAT_PR_5_6:
			/* Enable 5/6 PR only */
			ChipSetOneRegister(hChip, RSTV0903_PRVIT, 0x08);
			break;

		case FE_SAT_PR_7_8:
			/* Enable 7/8 PR only */
			ChipSetOneRegister(hChip, RSTV0903_PRVIT, 0x20);
			break;
		}

		break;

	case FE_SAT_SEARCH_DSS:
		/* Disable DVBS1 */
		ChipSetOneRegister(hChip, RSTV0903_FECM, 0x80);
		switch (PunctureRate) {
		case FE_SAT_PR_UNKNOWN:
		default:
			/* Enable 1/2, 2/3 and 6/7 PR */
			ChipSetOneRegister(hChip, RSTV0903_PRVIT, 0x13);
			break;

		case FE_SAT_PR_1_2:
			/* Enable 1/2 PR only */
			ChipSetOneRegister(hChip, RSTV0903_PRVIT, 0x01);
			break;

		case FE_SAT_PR_2_3:
			/* Enable 2/3 PR only */
			ChipSetOneRegister(hChip, RSTV0903_PRVIT, 0x02);
			break;

		case FE_SAT_PR_6_7:
			/* Enable 7/8 PR only */
			ChipSetOneRegister(hChip, RSTV0903_PRVIT, 0x10);
			break;
		}

		break;

	default:
		break;
	}
}

void FE_STV0903_SetDVBS1_TrackCarLoop(STCHIP_Handle_t hChip, U32 SymbolRate)
{

	if (hChip->ChipID >= 0x30) {
		/* Set ACLC BCLC optimised value vs SR */
		if (SymbolRate >= 15000000) {
			ChipSetOneRegister(hChip, RSTV0903_ACLC, 0x2b);
			ChipSetOneRegister(hChip, RSTV0903_BCLC, 0x1a);
		} else if ((SymbolRate >= 7000000) && (15000000 > SymbolRate)) {
			ChipSetOneRegister(hChip, RSTV0903_ACLC, 0x0c);
			ChipSetOneRegister(hChip, RSTV0903_BCLC, 0x1b);
		} else if (SymbolRate < 7000000) {
			ChipSetOneRegister(hChip, RSTV0903_ACLC, 0x2c);
			ChipSetOneRegister(hChip, RSTV0903_BCLC, 0x1c);
		}

	} else {			/*cut 2.0 and 1.x */
		ChipSetOneRegister(hChip, RSTV0903_ACLC, 0x1a);
		ChipSetOneRegister(hChip, RSTV0903_BCLC, 0x09);
	}

}

/*****************************************************
--FUNCTION	::	SetSearchStandard
--ACTION	::	Set the Search standard (Auto, DVBS2 only or DVBS1/DSS
			only)
--PARAMS IN	::	hChip	->	handle to the chip
			::	Standard	->	Search standard
--PARAMS OUT::	NONE
--RETURN	::	none
--***************************************************/
void FE_STV0903_SetSearchStandard(FE_STV0903_InternalParams_t *pParams)
{
	switch (pParams->DemodSearchStandard) {
	case FE_SAT_SEARCH_DVBS1:
	case FE_SAT_SEARCH_DSS:
		/*If DVBS1/DSS only disable DVBS2 search */
		ChipSetFieldImage(pParams->hDemod, FSTV0903_DVBS1_ENABLE, 1);
		ChipSetFieldImage(pParams->hDemod, FSTV0903_DVBS2_ENABLE, 0);
		ChipSetRegisters(pParams->hDemod, RSTV0903_DMDCFGMD, 1);

		/*Activate Viterbi decoder clock */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_STOPCLK2, 0x14);
		/*Activate Viterbi decoder */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_TSTRES0, 0);

		FE_STV0903_SetDVBS1_TrackCarLoop(pParams->hDemod,
						 pParams->DemodSymbolRate);
		/* disable the DVBS2 carrier loop */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_CAR2CFG, 0x22);

		FE_STV0903_SetViterbiThAcq(pParams->hDemod);
		FE_STV0903_SetViterbiStandard(pParams->hDemod,
					      pParams->DemodSearchStandard,
					      pParams->DemodPunctureRate);
		break;

	case FE_SAT_SEARCH_DVBS2:
		/*If DVBS2 only activate the DVBS2 and stop the VITERBI */
		ChipSetFieldImage(pParams->hDemod, FSTV0903_DVBS1_ENABLE, 0);
		ChipSetFieldImage(pParams->hDemod, FSTV0903_DVBS2_ENABLE, 1);
		ChipSetRegisters(pParams->hDemod, RSTV0903_DMDCFGMD, 1);

		/*Stop the Viterbi decoder */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_TSTRES0, 0x20);
		/*Stop Viterbi decoder clock */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_STOPCLK2, 0x16);

		/*stop DVBS1/DSS carrier loop  */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_ACLC, 0);
		/*stop DVBS1/DSS carrier loop  */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_BCLC, 0);

		if (pParams->hDemod->ChipID <= 0x20) /*cut 1.x and 2.0 */
			/* enable the DVBS2 carrier loop */
			ChipSetOneRegister(pParams->hDemod, RSTV0903_CAR2CFG,
					   0x26);
		else	/* cut 3.0 above (stop carrier 3) to be reviewed */
			ChipSetOneRegister(pParams->hDemod, RSTV0903_CAR2CFG,
					   0x66);

		/*Activate all S2 modcode for search */
		FE_STV0903_ActivateS2ModCode(pParams->hDemod);
		/*if DVBS2 search reset VIREBI THresholds to tracking values */
		FE_STV0903_SetViterbiThTracq(pParams->hDemod);
		break;

	case FE_SAT_AUTO_SEARCH:
	default:
		if (pParams->hDemod->ChipID <= 0x20) {
			ChipSetFieldImage(pParams->hDemod,
					  FSTV0903_DVBS1_ENABLE, 0);
			ChipSetFieldImage(pParams->hDemod,
					  FSTV0903_DVBS2_ENABLE, 1);
			ChipSetRegisters(pParams->hDemod, RSTV0903_DMDCFGMD, 1);

			ChipSetFieldImage(pParams->hDemod,
					  FSTV0903_DVBS1_ENABLE, 1);
			ChipSetFieldImage(pParams->hDemod,
					  FSTV0903_DVBS2_ENABLE, 0);
			ChipSetRegisters(pParams->hDemod, RSTV0903_DMDCFGMD, 1);
		}
		/*If automatic enable both DVBS1/DSS and DVBS2 search */
		ChipSetFieldImage(pParams->hDemod, FSTV0903_DVBS1_ENABLE, 1);
		ChipSetFieldImage(pParams->hDemod, FSTV0903_DVBS2_ENABLE, 1);
		ChipSetRegisters(pParams->hDemod, RSTV0903_DMDCFGMD, 1);

		/*Activate Viterbi decoder clock */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_STOPCLK2, 0x14);
		/*Activate Viterbi decoder */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_TSTRES0, 0);

		FE_STV0903_SetDVBS1_TrackCarLoop(pParams->hDemod,
						 pParams->DemodSymbolRate);

		if (pParams->hDemod->ChipID <= 0x20)
			/*cut 1.x and 2.0 */
			/* enable the DVBS2 carrier loop */
			ChipSetOneRegister(pParams->hDemod, RSTV0903_CAR2CFG,
					   0x26);
		else	/* cut 3.0 above (stop carrier 3) to be reviewed */
			ChipSetOneRegister(pParams->hDemod, RSTV0903_CAR2CFG,
					   0x66);

		/*Activate all S2 modcode for search */
		FE_STV0903_ActivateS2ModCode(pParams->hDemod);

		if (pParams->DemodSymbolRate >= 2000000)
			/*SR>=2Msps set Viterbi Thresholds to acqui */
			FE_STV0903_SetViterbiThAcq(pParams->hDemod);
		else
			/*SR <2Msps reset VITERBI thresholds to tracking and
			 * acquistions */
			FE_STV0903_SetViterbiThTracq(pParams->hDemod);

		FE_STV0903_SetViterbiStandard(pParams->hDemod,
					      pParams->DemodSearchStandard,
					      pParams->DemodPunctureRate);

		break;
	}
}

/*****************************************************
--FUNCTION	::	StartSearch
--ACTION	::	Trig the Demod to start a new search
--PARAMS IN	::	pParams	->Pointer FE_STV0903_InternalParams_t structer
--PARAMS OUT::	NONE
--RETURN	::	none
--***************************************************/
void FE_STV0903_StartSearch(FE_STV0903_InternalParams_t *pParams)
{

	U32 freq;
	S16 freq_s16;

	/*Reset the Demod */
	ChipSetOneRegister(pParams->hDemod, RSTV0903_DMDISTATE, 0x1F);

	/*Set Correlation frames reg for acquisition for cut 1.0 only */
	if (pParams->hDemod->ChipID == 0x10)
		ChipSetOneRegister(pParams->hDemod, RSTV0903_CORRELEXP, 0xAA);

	/*Not applicable for cut 2.0 */
	if (pParams->hDemod->ChipID < 0x20)
		ChipSetOneRegister(pParams->hDemod, RSTV0903_CARHDR, 0x55);

	if (pParams->hDemod->ChipID <= 0x20) {

		if (pParams->DemodSymbolRate <= 5000000) {

			ChipSetOneRegister(pParams->hDemod, RSTV0903_CARCFG,
					   0x44);
			/* If the symbol rate is <= 5Msps */

			/*Set The carrier search up and low to manual mode
			 *[-8Mhz,+8Mhz] */
			/*CFR UP Setting */
			ChipSetOneRegister(pParams->hDemod, RSTV0903_CFRUP1,
					   0x0F);
			ChipSetOneRegister(pParams->hDemod, RSTV0903_CFRUP0,
					   0xFF);

			/*CFR Low Setting */
			ChipSetOneRegister(pParams->hDemod, RSTV0903_CFRLOW1,
					   0xF0);
			ChipSetOneRegister(pParams->hDemod, RSTV0903_CFRLOW0,
					   0x00);

			/*enlarge the timing bandwith for Low SR */
			ChipSetOneRegister(pParams->hDemod, RSTV0903_RTCS2,
					   0x68);
		} else {
			/* If the symbol rate is >5 Msps
			   Set The carrier search up and low to auto mode */
			ChipSetOneRegister(pParams->hDemod, RSTV0903_CARCFG,
					   0xC4);
			/*reduce the timing bandwith for high SR */
			ChipSetOneRegister(pParams->hDemod, RSTV0903_RTCS2,
					   0x44);
		}
	} else {

		if (pParams->DemodSymbolRate <= 5000000)
			/*enlarge the timing bandwith for Low SR */
			ChipSetOneRegister(pParams->hDemod, RSTV0903_RTCS2,
					   0x68);
		else
			/*reduce the timing bandwith for high SR */
			ChipSetOneRegister(pParams->hDemod, RSTV0903_RTCS2,
					   0x44);

		/*Set CFR min and max to manual mode */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_CARCFG, 0x46);

		if (pParams->DemodSearchAlgo == FE_SAT_WARM_START) {
			/* if warm start CFR min =-1MHz , CFR max = 1MHz */
			freq = 1000 << 16;
			freq /= (pParams->MasterClock / 1000);
			freq_s16 = (S16) freq;
		} else {

			/* CFR min =- (SearchRange/2 + margin )
			   CFR max = +(SearchRange/2 + margin)
			   (80KHz margin if SR <=5Msps else margin =600KHz ) */

			if (pParams->DemodSymbolRate <= 5000000)
				freq = (pParams->DemodSearchRange / 2000) + 80;
			else
				freq = (pParams->DemodSearchRange / 2000) + 600;

			freq = freq << 16;
			freq /= (pParams->MasterClock / 1000);
			freq_s16 = (S16) freq;
		}

		ChipSetFieldImage(pParams->hDemod, FSTV0903_CFR_UP1,
				  MSB(freq_s16));
		ChipSetFieldImage(pParams->hDemod, FSTV0903_CFR_UP0,
				  LSB(freq_s16));
		ChipSetRegisters(pParams->hDemod, RSTV0903_CFRUP1, 2);
		freq_s16 *= (-1);
		ChipSetFieldImage(pParams->hDemod, FSTV0903_CFR_LOW1,
				  MSB(freq_s16));
		ChipSetFieldImage(pParams->hDemod, FSTV0903_CFR_LOW0,
				  LSB(freq_s16));
		ChipSetRegisters(pParams->hDemod, RSTV0903_CFRLOW1, 2);

	}

	ChipSetFieldImage(pParams->hDemod, FSTV0903_CFR_INIT1, 0);
	ChipSetFieldImage(pParams->hDemod, FSTV0903_CFR_INIT0, 0);
	/*init the demod frequency offset to 0 */
	ChipSetRegisters(pParams->hDemod, RSTV0903_CFRINIT1, 2);

	if (pParams->hDemod->ChipID >= 0x20) {
		/*enable the equalizer for search */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_EQUALCFG, 0x41);
		ChipSetOneRegister(pParams->hDemod, RSTV0903_FFECFG, 0x41);

		if ((pParams->DemodSearchStandard == FE_SAT_SEARCH_DVBS1)
		    || (pParams->DemodSearchStandard == FE_SAT_SEARCH_DSS)
		    || (pParams->DemodSearchStandard == FE_SAT_AUTO_SEARCH)) {
			/* open the ReedSolomon to viterbi feed back until
			 * demod lock */
			ChipSetOneRegister(pParams->hDemod, RSTV0903_VITSCALE,
					   0x82);
			/* set Viterbi hysteresis for search */
			ChipSetOneRegister(pParams->hDemod, RSTV0903_VAVSRVIT,
					   0x0);
		}
	}

	/* Slow down the timing loop */
	ChipSetOneRegister(pParams->hDemod, RSTV0903_SFRSTEP, 0x00);
	/* timing stting for warm/cold start */
	ChipSetOneRegister(pParams->hDemod, RSTV0903_TMGTHRISE, 0xE0);
	/* timing stting for warm/cold start */
	ChipSetOneRegister(pParams->hDemod, RSTV0903_TMGTHFALL, 0xC0);

	/* stop the carrier frequency search loop */
	ChipSetFieldImage(pParams->hDemod, FSTV0903_CFR_AUTOSCAN, 0);
	/* stop the carrier frequency search loop */
	ChipSetFieldImage(pParams->hDemod, FSTV0903_SCAN_ENABLE, 0);
	ChipSetRegisters(pParams->hDemod, RSTV0903_DMDCFGMD, 1);

	/* S1 and  S2 search in // if both enabled */
	ChipSetFieldImage(pParams->hDemod, FSTV0903_S1S2_SEQUENTIAL, 0);
	ChipSetRegisters(pParams->hDemod, RSTV0903_DMDCFG2, 1);

	/*enable the DVBS1 timing loop */
	ChipSetOneRegister(pParams->hDemod, RSTV0903_RTC, 0x88);

	if (pParams->hDemod->ChipID >= 0x20) {
		/*Frequency offset detector setting */
		if (pParams->DemodSymbolRate < 2000000) {	/*SR <2Msps */
			if (pParams->hDemod->ChipID <= 0x20)	/* cut 2.0 */
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0903_CARFREQ, 0x39);
			else	/*cut 3.0 */
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0903_CARFREQ, 0x89);

			ChipSetOneRegister(pParams->hDemod, RSTV0903_CARHDR,
					   0x40);
		} else if (pParams->DemodSymbolRate < 10000000) {
			/* 2Msps <=SR <10Msps */
			ChipSetOneRegister(pParams->hDemod, RSTV0903_CARFREQ,
					   0x4C);
			ChipSetOneRegister(pParams->hDemod, RSTV0903_CARHDR,
					   0x20);
		} else {	/*SR >=10Msps */

			ChipSetOneRegister(pParams->hDemod, RSTV0903_CARFREQ,
					   0x4B);
			ChipSetOneRegister(pParams->hDemod, RSTV0903_CARHDR,
					   0x20);
		}
	} else {
		if (pParams->DemodSymbolRate < 10000000)
			ChipSetOneRegister(pParams->hDemod, RSTV0903_CARFREQ,
					   0xef);
		else
			ChipSetOneRegister(pParams->hDemod, RSTV0903_CARFREQ,
					   0xed);
	}
	switch (pParams->DemodSearchAlgo) {
	case FE_SAT_WARM_START:
		/*The symbol rate and the exact carrier Frequency are known */

		/*Reset the Demod */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_DMDISTATE, 0x1F);
		/*Trig an acquisition (start the search) */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_DMDISTATE, 0x18);
		break;

	case FE_SAT_COLD_START:	/*The symbol rate is known */

		/*Reset the Demod */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_DMDISTATE, 0x1F);
		/*Trig an acquisition (start the search) */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_DMDISTATE, 0x15);
		break;

	default:
		/*Nothing to do in case of blind search, blind  is handled by
		 * "FE_STV0903_BlindSearchAlgo" function */
		break;
	}
}

U8 FE_STV0903_GetOptimCarrierLoop(S32 SymbolRate, FE_STV0903_ModCod_t ModCode,
				  S32 Pilots, U8 ChipID)
{
	U8 aclcValue = 0x29;
	S32 i;
	struct FE_STV0903_CarLoopOPtim_s *carLoopS2;

	if (ChipID <= 0x12)
		carLoopS2 = FE_STV0903_S2CarLoop;
	else if (ChipID == 0x20)
		carLoopS2 = FE_STV0903_S2CarLoopCut20;
	else
		carLoopS2 = FE_STV0903_S2CarLoopCut30;

	if (ModCode < FE_SAT_QPSK_12)
		i = 0;
	else {
		i = 0;
		while ((i < 14) && (ModCode != carLoopS2[i].ModCode))
			i++;

		if (i >= 14)
			i = 13;
	}

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

	return aclcValue;
}

/*****************************************************
--FUNCTION	::	TrackingOptimization
--ACTION	::	Set Optimized parameters for tracking
--PARAMS IN	::	pParams	->Pointer FE_STV0903_InternalParams_t structer
--PARAMS OUT::	NONE
--RETURN	::	none
--***************************************************/
void FE_STV0903_TrackingOptimization(FE_STV0903_InternalParams_t *pParams)
{
	S32 symbolRate,
	    pilots, aclc, freq1, freq0, i = 0, timed, timef, blindTunSw = 0;

	FE_STV0903_RollOff_t rolloff;
	FE_STV0903_ModCod_t foundModcod;

	/*      Read the Symbol rate    */
	symbolRate =
	    FE_STV0903_GetSymbolRate(pParams->hDemod, pParams->MasterClock);
	symbolRate += FE_STV0903_TimingGetOffset(pParams->hDemod, symbolRate);
	switch (pParams->DemodResults.Standard) {
	case FE_SAT_DVBS1_STANDARD:
		if (pParams->DemodSearchStandard == FE_SAT_AUTO_SEARCH) {
			ChipSetFieldImage(pParams->hDemod,
					  FSTV0903_DVBS1_ENABLE, 1);
			ChipSetFieldImage(pParams->hDemod,
					  FSTV0903_DVBS2_ENABLE, 0);
			ChipSetRegisters(pParams->hDemod, RSTV0903_DMDCFGMD, 1);
		}
		/*Set the rolloff to the manual value (given at the
		 * initialization) */
		ChipSetFieldImage(pParams->hDemod, FSTV0903_ROLLOFF_CONTROL,
				  pParams->RollOff);
		ChipSetFieldImage(pParams->hDemod, FSTV0903_MANUALSX_ROLLOFF,
				  1);
		ChipSetRegisters(pParams->hDemod, RSTV0903_DEMOD, 1);

		if (pParams->hDemod->ChipID >= 0x30) {
			if (FE_STV0903_GetPunctureRate(pParams->hDemod) ==
			    FE_SAT_PR_1_2) {
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0903_GAUSSR0, 0x98);
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0903_CCIR0, 0x18);
			} else {
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0903_GAUSSR0, 0x18);
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0903_CCIR0, 0x18);
			}
		}
		/* force to viterbi bit error */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_ERRCTRL1, 0x75);
		break;

	case FE_SAT_DSS_STANDARD:
		if (pParams->DemodSearchStandard == FE_SAT_AUTO_SEARCH) {
			ChipSetFieldImage(pParams->hDemod,
					  FSTV0903_DVBS1_ENABLE, 1);
			ChipSetFieldImage(pParams->hDemod,
					  FSTV0903_DVBS2_ENABLE, 0);
			ChipSetRegisters(pParams->hDemod, RSTV0903_DMDCFGMD, 1);
		}
		/*Set the rolloff to the manual value (given at the
		 * initialization) */
		ChipSetFieldImage(pParams->hDemod, FSTV0903_ROLLOFF_CONTROL,
				  pParams->RollOff);
		ChipSetFieldImage(pParams->hDemod, FSTV0903_MANUALSX_ROLLOFF,
				  1);
		ChipSetRegisters(pParams->hDemod, RSTV0903_DEMOD, 1);

		if (pParams->hDemod->ChipID >= 0x30) {
			if (FE_STV0903_GetPunctureRate(pParams->hDemod) ==
			    FE_SAT_PR_1_2) {
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0903_GAUSSR0, 0x98);
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0903_CCIR0, 0x18);
			} else {
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0903_GAUSSR0, 0x18);
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0903_CCIR0, 0x18);
			}
		}
		/* force to viterbi bit error */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_ERRCTRL1, 0x75);
		break;

	case FE_SAT_DVBS2_STANDARD:

		/*stop DVBS1/DSS carrier loop  */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_ACLC, 0);
		/*stop DVBS1/DSS carrier loop  */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_BCLC, 0);

		ChipSetFieldImage(pParams->hDemod, FSTV0903_DVBS1_ENABLE, 0);
		ChipSetFieldImage(pParams->hDemod, FSTV0903_DVBS2_ENABLE, 1);
		ChipSetRegisters(pParams->hDemod, RSTV0903_DMDCFGMD, 1);
		/*Carrier loop setting */
		foundModcod =
		    ChipGetField(pParams->hDemod, FSTV0903_DEMOD_MODCOD);
		pilots =
		    ChipGetFieldImage(pParams->hDemod,
				      FSTV0903_DEMOD_TYPE) & 0x01;

		aclc =
		    FE_STV0903_GetOptimCarrierLoop(symbolRate, foundModcod,
						   pilots,
						   pParams->hDemod->ChipID);
		if (foundModcod <= FE_SAT_QPSK_910)
			ChipSetOneRegister(pParams->hDemod, RSTV0903_ACLC2S2Q,
					   aclc);
		else if (foundModcod <= FE_SAT_8PSK_910) {
			/*stop the carrier update for QPSK */
			ChipSetOneRegister(pParams->hDemod, RSTV0903_ACLC2S2Q,
					   0x2a);
			ChipSetOneRegister(pParams->hDemod, RSTV0903_ACLC2S28,
					   aclc);
		}

		if (pParams->hDemod->ChipID <= 0x11)
			/*Stop Viterbi decoder if DVBS2 found */
			ChipSetOneRegister(pParams->hDemod, RSTV0903_TSTRES0,
					   0x20);
		/* error counter to  DVBS2 packet error rate */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_ERRCTRL1, 0x67);

		if (pParams->hDemod->ChipID >= 0x30) {
			ChipSetOneRegister(pParams->hDemod, RSTV0903_GAUSSR0,
					   0xac);
			ChipSetOneRegister(pParams->hDemod, RSTV0903_CCIR0,
					   0x2c);
		}

		break;

	case FE_SAT_UNKNOWN_STANDARD:
	default:
		ChipSetFieldImage(pParams->hDemod, FSTV0903_DVBS1_ENABLE, 1);
		ChipSetFieldImage(pParams->hDemod, FSTV0903_DVBS2_ENABLE, 1);
		ChipSetRegisters(pParams->hDemod, RSTV0903_DMDCFGMD, 1);
		break;
	}

	/*read the carrier frequency */
	freq1 = ChipGetOneRegister(pParams->hDemod, RSTV0903_CFR2);
	freq0 = ChipGetOneRegister(pParams->hDemod, RSTV0903_CFR1);
	/*      Read the rolloff                */
	rolloff = ChipGetField(pParams->hDemod, FSTV0903_ROLLOFF_STATUS);

	if (pParams->DemodSearchAlgo == FE_SAT_BLIND_SEARCH) {
		ChipSetOneRegister(pParams->hDemod, RSTV0903_SFRSTEP, 0x00);

		/* stop the carrier frequency search loop */
		ChipSetFieldImage(pParams->hDemod, FSTV0903_CFR_AUTOSCAN, 0);
		/* stop the carrier frequency search loop */
		ChipSetFieldImage(pParams->hDemod, FSTV0903_SCAN_ENABLE, 0);
		ChipSetRegisters(pParams->hDemod, RSTV0903_DMDCFGMD, 1);

		ChipSetOneRegister(pParams->hDemod, RSTV0903_TMGCFG2, 0x01);
		/*Set the init symbol rate to the found value */
		FE_STV0903_SetSymbolRate(pParams->hDemod, pParams->MasterClock,
					 symbolRate);
		blindTunSw = 1;

		if (pParams->DemodResults.Standard != FE_SAT_DVBS2_STANDARD) {
			FE_STV0903_SetDVBS1_TrackCarLoop(pParams->hDemod,
							 symbolRate);
		}
	}

	if (pParams->hDemod->ChipID < 0x20)
		/*Not applicable for cut 2.0 */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_CARHDR, 0x08);

	if (pParams->hDemod->ChipID == 0x10)
		/*Set Correlation frames reg for Tracking for cut 1.0 only */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_CORRELEXP, 0x0A);

	/*Set the AGC2 ref to the tracking value */
	ChipSetOneRegister(pParams->hDemod, RSTV0903_AGC2REF, 0x38);

	/*Set SFR Up to auto mode for tracking */
	ChipSetOneRegister(pParams->hDemod, RSTV0903_SFRUP1, 0x80);
	/*Set SFR Low to auto mode for tracking */
	ChipSetOneRegister(pParams->hDemod, RSTV0903_SFRLOW1, 0x80);

	if ((pParams->hDemod->ChipID >= 0x20) || (blindTunSw == 1)
	    || (pParams->DemodSymbolRate < 10000000)) {
		/*update the init carrier freq with the found freq offset */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_CFRINIT1, freq1);
		ChipSetOneRegister(pParams->hDemod, RSTV0903_CFRINIT0, freq0);
		pParams->TunerBW =
		    FE_STV0903_CarrierWidth(symbolRate,
					    pParams->RollOff) + 10000000;

		if (((pParams->hDemod->ChipID >= 0x20) || (blindTunSw == 1))
			&& (pParams->DemodSearchAlgo != FE_SAT_WARM_START)) {
			if (pParams->TunerType == FE_SAT_SW_TUNER)
				FE_Sat_TunerSetBandwidth(pParams->hTuner,
						pParams->TunerBW);
			else
				/*Update the Tuner BW */
				FE_STV0903_SetTuner(pParams->hDemod,
					pParams->hTuner, pParams->TunerType,
					pParams->TunerFrequency,
					pParams->TunerBW);
		}
		if ((pParams->DemodSearchAlgo == FE_SAT_BLIND_SEARCH)
		    || (pParams->DemodResults.SymbolRate < 10000000))
			/*if blind search wait 50ms for SR stabilization */
			ChipWaitOrAbort(pParams->hDemod, 50);
		else
			ChipWaitOrAbort(pParams->hDemod, 5);

		FE_STV0903_GetLockTimeout(&timed, &timef, symbolRate,
					  FE_SAT_WARM_START);

		if (FE_STV0903_GetDemodLock(pParams->hDemod, timed / 2) ==
		    FALSE) {
			/*stop the demod */
			ChipSetOneRegister(pParams->hDemod, RSTV0903_DMDISTATE,
					   0x1F);
			/*init the frequency offset */
			ChipSetOneRegister(pParams->hDemod, RSTV0903_CFRINIT1,
					   freq1);
			ChipSetOneRegister(pParams->hDemod, RSTV0903_CFRINIT0,
					   freq0);
			/*stop the demod */
			ChipSetOneRegister(pParams->hDemod, RSTV0903_DMDISTATE,
					   0x18);
			i = 0;
			while ((FE_STV0903_GetDemodLock
				(pParams->hDemod, timed / 2) == FALSE)
			       && (i <= 2)) {
				/*if the demod lose lock because of tuner BW
				 * ref change */
				/*stop the demod */
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0903_DMDISTATE, 0x1F);
				/*init the frequency offset */
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0903_CFRINIT1, freq1);
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0903_CFRINIT0, freq0);

				/*stop the demod */
				ChipSetOneRegister(pParams->hDemod,
						   RSTV0903_DMDISTATE, 0x18);
				i++;
			}
		}
	}

	if (pParams->hDemod->ChipID >= 0x20)
		ChipSetOneRegister(pParams->hDemod, RSTV0903_CARFREQ, 0x49);

	if ((pParams->DemodResults.Standard == FE_SAT_DVBS1_STANDARD)
	    || (pParams->DemodResults.Standard == FE_SAT_DSS_STANDARD)) {
		FE_STV0903_SetViterbiThTracq(pParams->hDemod);
		if (pParams->hDemod->ChipID >= 0x20) {
			/*set Viterbi hysteresis for tracking */
			ChipSetOneRegister(pParams->hDemod, RSTV0903_VAVSRVIT,
					   0x0a);
			/* close the ReedSolomon to viterbi feed back until
			 * demod lock */
			ChipSetOneRegister(pParams->hDemod, RSTV0903_VITSCALE,
					   0x0);
		}
	} else if (pParams->hDemod->ChipID >= 0x30)
		/*applicable for cut 3.0 */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_CARHDR, 0x10);

}

/*****************************************************
--FUNCTION	::	GetSignalParams
--ACTION	::	Read signal caracteristics
--PARAMS IN	::	pParams	->Pointer FE_STV0903_InternalParams_t structer
--PARAMS OUT::	NONE
--RETURN	::	RANGE Ok or not
--***************************************************/
FE_STV0903_SIGNALTYPE_t FE_STV0903_GetSignalParams(FE_STV0903_InternalParams_t *
						   pParams)
{
	FE_STV0903_SIGNALTYPE_t range = FE_SAT_OUTOFRANGE;
	S32 offsetFreq, symbolRateOffset, i = 0;

	U8 timing;

#ifdef HOST_PC
#ifndef NO_GUI
	char message[100];
#endif
#endif

	ChipWaitOrAbort(pParams->hDemod, 5);
	if (pParams->DemodSearchAlgo == FE_SAT_BLIND_SEARCH) {
		/* if Blind search wait for symbol rate offset information
		 * transfert from the timing loop to the
		 demodulator symbol rate */
		timing = ChipGetOneRegister(pParams->hDemod, RSTV0903_TMGREG2);
		i = 0;
		ChipSetOneRegister(pParams->hDemod, RSTV0903_SFRSTEP, 0x5c);
		while ((i <= 50) && (timing != 0) && (timing != 0xFF)) {
			timing =
			    ChipGetOneRegister(pParams->hDemod,
					       RSTV0903_TMGREG2);
			ChipWaitOrAbort(pParams->hDemod, 5);
			i += 5;
		}
	}

	pParams->DemodResults.Standard =
	    FE_STV0903_GetStandard(pParams->hDemod);

	pParams->DemodResults.Frequency =
	    FE_STV0903_GetTunerFrequency(pParams->hDemod, pParams->hTuner,
					 pParams->TunerType);
	offsetFreq =
	    FE_STV0903_GetCarrierFrequency(pParams->hDemod,
					   pParams->MasterClock) / 1000;
	pParams->DemodResults.Frequency += offsetFreq;

	pParams->DemodResults.SymbolRate =
	    FE_STV0903_GetSymbolRate(pParams->hDemod, pParams->MasterClock);
	symbolRateOffset =
	    FE_STV0903_TimingGetOffset(pParams->hDemod,
				       pParams->DemodResults.SymbolRate);
	pParams->DemodResults.SymbolRate += symbolRateOffset;
	pParams->DemodResults.PunctureRate =
	    FE_STV0903_GetPunctureRate(pParams->hDemod);
	pParams->DemodResults.ModCode =
	    ChipGetField(pParams->hDemod, FSTV0903_DEMOD_MODCOD);
	pParams->DemodResults.Pilots =
	    ChipGetFieldImage(pParams->hDemod, FSTV0903_DEMOD_TYPE) & 0x01;
	pParams->DemodResults.FrameLength =
	    ((U32) ChipGetFieldImage(pParams->hDemod, FSTV0903_DEMOD_TYPE)) >>
	    1;

	pParams->DemodResults.RollOff =
	    ChipGetField(pParams->hDemod, FSTV0903_ROLLOFF_STATUS);

	switch (pParams->DemodResults.Standard) {
	case FE_SAT_DVBS2_STANDARD:
		pParams->DemodResults.Spectrum =
		    ChipGetField(pParams->hDemod, FSTV0903_SPECINV_DEMOD);

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
		    ChipGetField(pParams->hDemod, FSTV0903_IQINV);

		break;
	}

	if ((pParams->DemodSearchAlgo == FE_SAT_BLIND_SEARCH)
	    || (pParams->DemodResults.SymbolRate < 10000000)) {
		/*in case of blind search the tuner freq may has been moven,
		 * read the new tuner freq and cumpute the freq offset from the
		 * original value */
		offsetFreq =
		    pParams->DemodResults.Frequency - pParams->TunerFrequency;
		pParams->TunerFrequency =
		    FE_STV0903_GetTunerFrequency(pParams->hDemod,
						 pParams->hTuner,
						 pParams->TunerType);

		if (ABS(offsetFreq) <=
		    ((pParams->DemodSearchRange / 2000) + 500))
			range = FE_SAT_RANGEOK;
		else if (ABS(offsetFreq) <=
			 ((1000 + FE_STV0903_CarrierWidth(pParams->
				DemodResults.SymbolRate,
				pParams->DemodResults.RollOff)) / 2000))
			range = FE_SAT_RANGEOK;
		else
			range = FE_SAT_OUTOFRANGE;
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

FE_STV0903_SIGNALTYPE_t
FE_STV0903_DVBS1AcqWorkAround(FE_STV0903_InternalParams_t *pParams)
{

	S32 symbolRate, demodTimeout, fecTimeout, freq1, freq0;
	FE_STV0903_SIGNALTYPE_t signalType = FE_SAT_NODATA;

	pParams->DemodResults.Locked = FALSE;
	if (ChipGetField(pParams->hDemod, FSTV0903_HEADER_MODE) !=
			FE_SAT_DVBS_FOUND)
		return signalType;
	/*workaround for DVBS1 false lock */
	/*      Read the Symbol rate    */
	symbolRate = FE_STV0903_GetSymbolRate(pParams->hDemod,
			pParams->MasterClock);
	/*      Read the Symbol rate offset     */
	symbolRate += FE_STV0903_TimingGetOffset(pParams->hDemod, symbolRate);

	if (pParams->DemodSearchAlgo == FE_SAT_BLIND_SEARCH)
		/*      Init the symbol rate    */
		FE_STV0903_SetSymbolRate(pParams->hDemod, pParams->MasterClock,
				symbolRate);

	FE_STV0903_GetLockTimeout(&demodTimeout, &fecTimeout, symbolRate,
			FE_SAT_WARM_START);

	freq1 = ChipGetOneRegister(pParams->hDemod, RSTV0903_CFR2);
	freq0 = ChipGetOneRegister(pParams->hDemod, RSTV0903_CFR1);
	/*stop the carrier frequency search loop */
	ChipSetField(pParams->hDemod, FSTV0903_CFR_AUTOSCAN, 0);

	ChipSetField(pParams->hDemod, FSTV0903_SPECINV_CONTROL,
			FE_SAT_IQ_FORCE_SWAPPED);
	/*stop the demod */
	ChipSetOneRegister(pParams->hDemod, RSTV0903_DMDISTATE, 0x1C);
	/*init the frequency offset */
	ChipSetOneRegister(pParams->hDemod, RSTV0903_CFRINIT1, freq1);
	ChipSetOneRegister(pParams->hDemod, RSTV0903_CFRINIT0, freq0);
	/*trig a warm start */
	ChipSetOneRegister(pParams->hDemod, RSTV0903_DMDISTATE, 0x18);

	if (FE_STV0903_WaitForLock
	    (pParams->hDemod, demodTimeout, fecTimeout) == TRUE) {
		/*wait until Lock or timeout */
		pParams->DemodResults.Locked = TRUE;
		/* Read signal caracteristics */
		signalType = FE_STV0903_GetSignalParams(pParams);
		/* Optimization setting for tracking */
		FE_STV0903_TrackingOptimization(pParams);
		return signalType;
	}
	ChipSetField(pParams->hDemod, FSTV0903_SPECINV_CONTROL,
			FE_SAT_IQ_FORCE_NORMAL);
	/*stop the demod */
	ChipSetOneRegister(pParams->hDemod, RSTV0903_DMDISTATE, 0x1C);
	/*init the frequency offset */
	ChipSetOneRegister(pParams->hDemod, RSTV0903_CFRINIT1, freq1);
	ChipSetOneRegister(pParams->hDemod, RSTV0903_CFRINIT0, freq0);
	/*trig a warm start */
	ChipSetOneRegister(pParams->hDemod, RSTV0903_DMDISTATE, 0x18);

	if (FE_STV0903_WaitForLock(pParams->hDemod, demodTimeout, fecTimeout)
			== TRUE) {
		/*wait until Lock or timeout */
		pParams->DemodResults.Locked = TRUE;
		/* Read signal caracteristics */
		signalType = FE_STV0903_GetSignalParams(pParams);
		/* Optimization setting for tracking */
		FE_STV0903_TrackingOptimization(pParams);
	}
	return signalType;
}

void FE_STV0903_Setdvbs2RollOff(STCHIP_Handle_t hChip)
{
	S32 rolloff;

	if (hChip->ChipID == 0x10) {
		/*For cut 1.1 and above the setting of the rolloff in DVBS2 is
		 * automatic in both path 1 and 2 */
		/* for cut 1.0 the setting of the rolloff should be manual from
		 * the MATSTR field of the BB Header */
		/* the setting of the rolloff in DVBS2 , when manual, should be
		 * after FEC lock to be sure that */
		/* at least one BB frame was decoded */
		/*Set the rolloff manually from the BB header value */

		/*Read the MATSTR reg */
		rolloff = ChipGetOneRegister(hChip, RSTV0903_MATSTR1) & 0x03;

		ChipSetFieldImage(hChip, FSTV0903_MANUALSX_ROLLOFF, 1);
		ChipSetFieldImage(hChip, FSTV0903_ROLLOFF_CONTROL, rolloff);
		ChipSetRegisters(hChip, RSTV0903_DEMOD, 1);
	} else if (hChip->ChipID <= 0x20) {
		/*For cut 1.1 to 2.0set rolloff to auto mode if DVBS2 found */
		ChipSetFieldImage(hChip, FSTV0903_MANUALSX_ROLLOFF, 0);
		ChipSetRegisters(hChip, RSTV0903_DEMOD, 1);
	} else {
		/*For cut 3.0 set DVBS2 rolloff to auto mode if DVBS2 found */
		ChipSetFieldImage(hChip, FSTV0903_MANUALS2_ROLLOFF, 0);
		ChipSetRegisters(hChip, RSTV0903_DEMOD, 1);
	}

}

BOOL FE_STV0903_CheckSignalPresence(FE_STV0903_InternalParams_t *pParams)
{
	S32 carrierOffset, agc2Integrateur, maxCarrier;

	BOOL noSignal;

	carrierOffset =
	    (ChipGetOneRegister(pParams->hDemod, RSTV0903_CFR2) << 8) |
	    ChipGetOneRegister(pParams->hDemod, RSTV0903_CFR1);
	carrierOffset = Get2Comp(carrierOffset, 16);
	agc2Integrateur =
	    (ChipGetOneRegister(pParams->hDemod, RSTV0903_AGC2I1) << 8) |
	    ChipGetOneRegister(pParams->hDemod, RSTV0903_AGC2I0);
	maxCarrier = pParams->DemodSearchRange / 1000;	/*convert to KHz */

	maxCarrier += (maxCarrier / 10);	/* add 10% of margin */
	maxCarrier = 65536 * (maxCarrier / 2);	/*Convert to reg value */
	maxCarrier /= pParams->MasterClock / 1000;
	/*Securite anti-bug: */
	if (maxCarrier > 0x4000)
		maxCarrier = 0x4000;	/*maxcarrier should be<= +-1/4 Mclk */

	/*if the AGC2 integrateur > 0x2000: No signal at this position
	   Else, if the AGC2 is <=0x2000 and the |carrier offset| is
	   >=maxcarrier then the demodulator is checking an adjacent channel */
	if ((agc2Integrateur > 0x2000)
	    || (carrierOffset > 2 * maxCarrier)
	    || (carrierOffset < -2 * maxCarrier))
		noSignal = TRUE;
	else
		noSignal = FALSE;

	return noSignal;
}

BOOL FE_STV0903_SearchCarrierSwLoop(FE_STV0903_InternalParams_t *pParams,
				    S32 FreqIncr,
				    S32 Timeout, BOOL Zigzag, S32 MaxStep)
{
	BOOL noSignal, lock = FALSE;

	S32 stepCpt, freqOffset, maxCarrier;

	maxCarrier = pParams->DemodSearchRange / 1000;
	maxCarrier += (maxCarrier / 10);

	maxCarrier = 65536 * (maxCarrier / 2);	/*Convert to reg value */
	maxCarrier /= pParams->MasterClock / 1000;

	if (maxCarrier > 0x4000)
		maxCarrier = 0x4000;	/*maxcarrier should be<= +-1/4 Mclk */

	/*Initialization */
	if (Zigzag == TRUE)
		freqOffset = 0;
	else
		freqOffset = -maxCarrier + FreqIncr;

	stepCpt = 0;

	do {
		/*Stop Demod */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_DMDISTATE, 0x1C);
		/*init carrier freq */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_CFRINIT1,
				   (freqOffset / 256) & 0xFF);
		ChipSetOneRegister(pParams->hDemod, RSTV0903_CFRINIT0,
				   freqOffset & 0xFF);
		/*Trig an acquisition (start the search) */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_DMDISTATE, 0x18);
		/*DVBS2 only: stop the Packet Delineator: */
		/*stop the DVBS2 packet delin */
		ChipSetField(pParams->hDemod, FSTV0903_ALGOSWRST, 1);

		/*Next Step */
		if (Zigzag == TRUE) {
			if (freqOffset >= 0)
				freqOffset = -freqOffset - 2 * FreqIncr;
			else
				freqOffset = -freqOffset;
		} else
			freqOffset += 2 * FreqIncr;
		stepCpt++;

		lock = FE_STV0903_GetDemodLock(pParams->hDemod, Timeout);
		noSignal = FE_STV0903_CheckSignalPresence(pParams);
		/*End of the loop if:
		   -lock,
		   -carrier offset > maxoffset,
		   -step > MaxStep
		 */
	} while ((lock == FALSE)
	       && (noSignal == FALSE)
	       && ((freqOffset - FreqIncr) < maxCarrier)
	       && ((freqOffset + FreqIncr) > -maxCarrier)
	       && (stepCpt < MaxStep));
	/*DVBS2 only: release the Packet Delineator: */
	/*release reset DVBS2 packet delin */
	ChipSetField(pParams->hDemod, FSTV0903_ALGOSWRST, 0);

	return lock;
}

void FE_STV0903_GetSwLoopParams(FE_STV0903_InternalParams_t *pParams,
				S32 *FreqIncrement,
				S32 *SwTimeout, S32 *MaxSteps)
{
	S32 timeOut, freqInrement, maxSteps, maxCarrier;

	FE_STV0903_SearchStandard_t Standard;

	maxCarrier = pParams->DemodSearchRange / 1000;
	maxCarrier += maxCarrier / 10;
	Standard = pParams->DemodSearchStandard;

	maxCarrier = 65536 * (maxCarrier / 2);	/*Convert to reg value */
	maxCarrier /= pParams->MasterClock / 1000;

	if (maxCarrier > 0x4000)
		maxCarrier = 0x4000;	/*maxcarrier should be<= +-1/4 Mclk */

	freqInrement = pParams->DemodSymbolRate;
	freqInrement /= (pParams->MasterClock >> 10);
	freqInrement = freqInrement << 6;

	switch (Standard) {
	case FE_SAT_SEARCH_DVBS1:
	case FE_SAT_SEARCH_DSS:
		freqInrement *= 3;	/*frequ step = 3% of symbol rate */
		timeOut = 20;
		break;

	case FE_SAT_SEARCH_DVBS2:
		freqInrement *= 4;	/*frequ step = 4% of symbol rate */
		timeOut = 25;
		break;

	case FE_SAT_AUTO_SEARCH:
	default:
		freqInrement *= 3;	/*frequ step = 3% of symbol rate */
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
	if (pParams->DemodSymbolRate > 0)
		timeOut /= pParams->DemodSymbolRate / 1000;

	if ((timeOut > 100) || (timeOut < 0))
		timeOut = 100;	/*timeout<= 100ms */

	maxSteps = (maxCarrier / freqInrement) + 1;	/*minimum step = 3 */
	if ((maxSteps > 100) || (maxSteps < 0)) {
		maxSteps = 100;	/*maxstep <= 100 */
		/*if max step > 100 then max step = 100 and the increment is
		 * radjasted to cover the total range */
		freqInrement = maxCarrier / maxSteps;
	}

	*FreqIncrement = freqInrement;
	*SwTimeout = timeOut;
	*MaxSteps = maxSteps;

}

BOOL FE_STV0903_SWAlgo(FE_STV0903_InternalParams_t *pParams)
{
	BOOL lock = FALSE;

	BOOL noSignal, zigzag;
	S32 dvbs2_fly_wheel;

	S32 freqIncrement, softStepTimeout, trialCounter, maxSteps;

	/*Get the SW algo params, Increment, timeout and max steps */
	FE_STV0903_GetSwLoopParams(pParams, &freqIncrement, &softStepTimeout,
				   &maxSteps);
	switch (pParams->DemodSearchStandard) {
	case FE_SAT_SEARCH_DVBS1:
	case FE_SAT_SEARCH_DSS:
		/*accelerate the frequency detector */
		if (pParams->hDemod->ChipID >= 0x20)
			/* value for cut 2.0 above */
			ChipSetOneRegister(pParams->hDemod, RSTV0903_CARFREQ,
					   0x3B);
		else
			/* value for cut 1.2 below */
			ChipSetOneRegister(pParams->hDemod, RSTV0903_CARFREQ,
					   0xEF);

		/*stop DVBS2 search, stop freq auto scan, stop SR auto scan */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_DMDCFGMD, 0x49);
		zigzag = FALSE;
		break;

	case FE_SAT_SEARCH_DVBS2:
		if (pParams->hDemod->ChipID >= 0x20)
			/*value for the SW search for cut 2.0 */
			ChipSetOneRegister(pParams->hDemod, RSTV0903_CORRELABS,
					   0x79);
		else
			/*value for the SW search for cut 1.x */
			ChipSetOneRegister(pParams->hDemod, RSTV0903_CORRELABS,
					   0x68);
		/*stop DVBS1 search, stop freq auto scan, stop SR auto scan */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_DMDCFGMD, 0x89);
		zigzag = TRUE;
		break;

	case FE_SAT_AUTO_SEARCH:
	default:
		if (pParams->hDemod->ChipID >= 0x20) {
			/* value for cut 2.0 above */
			ChipSetOneRegister(pParams->hDemod, RSTV0903_CARFREQ,
					   0x3B);
			/*value for the SW search for cut 2.0 */
			ChipSetOneRegister(pParams->hDemod, RSTV0903_CORRELABS,
					   0x79);
		} else {
			/* value for cut 1.2 below */
			ChipSetOneRegister(pParams->hDemod, RSTV0903_CARFREQ,
					   0xEF);
			/*value for the SW search for cut 1.x */
			ChipSetOneRegister(pParams->hDemod, RSTV0903_CORRELABS,
					   0x68);
		}

		/*enable DVBS2 and DVBS1 search, stop freq auto scan, stop SR
		 * auto scan */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_DMDCFGMD, 0xc9);
		zigzag = FALSE;
		break;
	}

	trialCounter = 0;
	do {
		lock =
		    FE_STV0903_SearchCarrierSwLoop(pParams, freqIncrement,
					   softStepTimeout, zigzag, maxSteps);
		noSignal = FE_STV0903_CheckSignalPresence(pParams);
		trialCounter++;
		/*run the SW search 2 times maximum */
		if ((lock != TRUE)
		    && (noSignal != TRUE) && (trialCounter != 2))
			continue;
		/*remove the SW algo values */
		/*Check if the demod is not losing lock in DVBS2 */
		if (pParams->hDemod->ChipID >= 0x20) {
			/* value for cut 2.0 above */
			ChipSetOneRegister(pParams->hDemod,
					   RSTV0903_CARFREQ, 0x49);
			/* value for cut 2.0 below */
			ChipSetOneRegister(pParams->hDemod,
					   RSTV0903_CORRELABS, 0x9e);
		} else {
			/* value for cut 1.2 below */
			ChipSetOneRegister(pParams->hDemod,
					   RSTV0903_CARFREQ, 0xed);
			/* value for cut 1.2 below */
			ChipSetOneRegister(pParams->hDemod,
					   RSTV0903_CORRELABS, 0x88);
		}
		if ((lock != TRUE) ||
		    (ChipGetField(pParams->hDemod, FSTV0903_HEADER_MODE)
		     != FE_SAT_DVBS2_FOUND))
			continue;
		/*Check if the demod is not losing lock in DVBS2 */
		ChipWaitOrAbort(pParams->hDemod, softStepTimeout);
		/*read the number of correct frames */
		dvbs2_fly_wheel = ChipGetField(pParams->hDemod,
				FSTV0903_FLYWHEEL_CPT);
		/*if correct frames is decrementing */
		if (dvbs2_fly_wheel < 0xd) {
			/*wait */
			ChipWaitOrAbort(pParams->hDemod, softStepTimeout);
			/*read the number of correct frames */
			dvbs2_fly_wheel = ChipGetField(pParams->hDemod,
					FSTV0903_FLYWHEEL_CPT);
		}
		if (dvbs2_fly_wheel >= 0xd)
			continue;
		/*FALSE lock, The demod is lossing lock */
		lock = FALSE;
		if (trialCounter >= 2)
			continue;
		if (pParams->hDemod->ChipID >= 0x20)
			/*value for the SW search for cut 2.0 */
			ChipSetOneRegister(pParams->hDemod, RSTV0903_CORRELABS,
					0x79);
		else
			/*value for the SW search for cut 1.x */
			ChipSetOneRegister(pParams->hDemod, RSTV0903_CORRELABS,
					0x68);
		ChipSetOneRegister(pParams->hDemod, RSTV0903_DMDCFGMD, 0x89);
	/*end of trial if lock or noSignal or more than 2 trial */
	} while ((lock == FALSE)
	       && (trialCounter < 2)
	       && (noSignal == FALSE));
	return lock;
}

BOOL FE_STV0903_GetDemodLockCold(FE_STV0903_InternalParams_t *pParams,
				 S32 DemodTimeout)
{

	BOOL lock = FALSE;
	S32 nbSteps, currentStep, direction, tunerFreq, timeout, stepsg, freq;

	U32 carrierStep = 1200;

	/*if Symbol rate >= 10Msps check for timing lock */
	if (pParams->DemodSymbolRate >= 10000000) {

		/* case cold start wait for demod lock */
		lock =
		    FE_STV0903_GetDemodLock(pParams->hDemod,
					    (DemodTimeout / 3));
		if (lock != FALSE)
			return lock;
		if (FE_STV0903_CheckTimingLock(pParams->hDemod) != TRUE) {
			lock = FALSE;
			return lock;
		}
		/*Reset the Demod */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_DMDISTATE, 0x1F);
		/*Trig an acquisition (start the search) */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_DMDISTATE, 0x15);

		lock = FE_STV0903_GetDemodLock(pParams->hDemod, DemodTimeout);
		return lock;
	}
	/*Symbol Rate <10Msps do not check timing lock */

	/* case cold start wait for demod lock */
	lock = FE_STV0903_GetDemodLock(pParams->hDemod, (DemodTimeout / 2));

	if (lock != FALSE)
		return lock;
	/* cut 2.0 and 1.x perofrms a zigzag on the tuner with AEP 0x15 */
	if (pParams->hDemod->ChipID <= 0x20) {
		if (pParams->DemodSymbolRate <= 1000000)
			carrierStep = 500;
		else if (pParams->DemodSymbolRate <= 4000000)
			carrierStep = 1000;
		else if (pParams->DemodSymbolRate <= 7000000)
			carrierStep = 2000;
		else if (pParams->DemodSymbolRate <= 10000000)
			carrierStep = 3000;
		else
			carrierStep = 5000;

		if (pParams->DemodSymbolRate >= 2000000) {
			timeout = (DemodTimeout / 3);
			if (timeout > 1000)
				/*timeout max for zigzag = 1sec */
				timeout = 1000;
		} else {
			timeout = DemodTimeout / 2;
		}
	} else {
		/*cut 3.0 */
		/* cut 3.0  perofrms zigzag on CFRINIT with the new AEP 0x05 */
		carrierStep = pParams->DemodSymbolRate / 4000;
		timeout = (DemodTimeout * 3) / 4;
	}

	nbSteps = ((pParams->DemodSearchRange / 1000) / carrierStep);

	if ((nbSteps % 2) != 0)
		nbSteps += 1;

	if (nbSteps <= 0)
		nbSteps = 2;
	else if (nbSteps > 12)
		nbSteps = 12;

	currentStep = 1;
	direction = 1;

	stepsg = 0;

	if (pParams->hDemod->ChipID <= 0x20) {
		/*zigzag the tuner freq */
		tunerFreq = pParams->TunerFrequency;
		pParams->TunerBW =
		    FE_STV0903_CarrierWidth(pParams->DemodSymbolRate,
			    pParams->RollOff) + pParams->DemodSymbolRate;

	} else	/*zigzag demod CFRINIT freq */
		tunerFreq = 0;

	while ((currentStep <= nbSteps) && (lock == FALSE)) {

		if (direction > 0)
			tunerFreq += (currentStep * carrierStep);
		else
			tunerFreq -= (currentStep * carrierStep);

		if (pParams->hDemod->ChipID <= 0x20) {
			/* cut 3.0 zigzag on Tuner freq */
			FE_STV0903_SetTuner(pParams->hDemod, pParams->hTuner,
			       pParams->TunerType, tunerFreq, pParams->TunerBW);

			ChipSetOneRegister(pParams->hDemod, RSTV0903_DMDISTATE,
					0x1C);

			ChipSetFieldImage(pParams->hDemod, FSTV0903_CFR_INIT1,
					0);
			ChipSetFieldImage(pParams->hDemod, FSTV0903_CFR_INIT0,
					0);
			/*init the demod frequency offset to 0 */
			ChipSetRegisters(pParams->hDemod, RSTV0903_CFRINIT1, 2);

			ChipSetOneRegister(pParams->hDemod, RSTV0903_DMDISTATE,
					0x1F);
			ChipSetOneRegister(pParams->hDemod, RSTV0903_DMDISTATE,
					0x15);

		} else {
			/* cut 3.0 zigzag on CFRINIT */
			ChipSetOneRegister(pParams->hDemod, RSTV0903_DMDISTATE,
					0x1C);

			/*      Formulat :
			   CFRINIT_Khz = CFRINIT_Reg * MasterClock_Khz /2^16
			 */

			freq = (tunerFreq * 65536) / (pParams->MasterClock /
					1000);
			ChipSetFieldImage(pParams->hDemod, FSTV0903_CFR_INIT1,
					MSB(freq));
			ChipSetFieldImage(pParams->hDemod, FSTV0903_CFR_INIT0,
					LSB(freq));
			/*init the demod frequency offset to 0 */
			ChipSetRegisters(pParams->hDemod, RSTV0903_CFRINIT1, 2);

			ChipSetOneRegister(pParams->hDemod, RSTV0903_DMDISTATE,
					0x1F);
			/* use AEP 0x05 instead of 0x15 to not reset CFRINIT to
			 * 0 */
			ChipSetOneRegister(pParams->hDemod, RSTV0903_DMDISTATE,
					0x05);

		}
		lock = FE_STV0903_GetDemodLock(pParams->hDemod, timeout);

		direction *= -1;
		currentStep++;
	}

	return lock;
}

U32 FE_STV0903_SearchSRCoarse(FE_STV0903_InternalParams_t *pParams)
{
	BOOL timingLock = FALSE;
	S32 i,
	    timingcpt = 0, direction = 1, nbSteps, currentStep = 0, tunerFreq;

	U32 coarseSymbolRate = 0,
	    agc2Integrateur = 0, carrierStep = 1200, agc2Th;

#ifdef HOST_PC
#ifndef NO_GUI
	char message[100];
#endif
#endif

	if (pParams->hDemod->ChipID >= 0x30)
		agc2Th = 0x2E00;
	else
		agc2Th = 0x1F00;

	/*Reset the Demod */
	ChipSetOneRegister(pParams->hDemod, RSTV0903_DMDISTATE, 0x5F);

	ChipSetOneRegister(pParams->hDemod, RSTV0903_TMGCFG, 0x12);
	ChipSetOneRegister(pParams->hDemod, RSTV0903_TMGCFG2, 0xC0);

	/*timing lock TH high setting for blind search */
	ChipSetOneRegister(pParams->hDemod, RSTV0903_TMGTHRISE, 0xf0);
	/*timing lock TH low setting for blind search */
	ChipSetOneRegister(pParams->hDemod, RSTV0903_TMGTHFALL, 0xe0);

	/*activate the carrier frequency search loop */
	ChipSetFieldImage(pParams->hDemod, FSTV0903_CFR_AUTOSCAN, 0);
	/*Enable the SR SCAN */
	ChipSetFieldImage(pParams->hDemod, FSTV0903_SCAN_ENABLE, 1);
	ChipSetRegisters(pParams->hDemod, RSTV0903_DMDCFGMD, 1);

	ChipSetFieldImage(pParams->hDemod, FSTV0903_AUTO_GUP, 1);
	/* set the SR max to 65Msps Write the MSB, auto mode */
	ChipSetRegisters(pParams->hDemod, RSTV0903_SFRUP1, 1);

	ChipSetFieldImage(pParams->hDemod, FSTV0903_AUTO_GLOW, 1);
	/* set the SR min to 400 Ksps Write the MSB */
	ChipSetRegisters(pParams->hDemod, RSTV0903_SFRLOW1, 1);

	/*force the acquisition to stop at coarse carrier state */
	ChipSetOneRegister(pParams->hDemod, RSTV0903_DMDT0M, 0x0);
	ChipSetOneRegister(pParams->hDemod, RSTV0903_AGC2REF, 0x50);

	if (pParams->hDemod->ChipID >= 0x30) {
		/*Frequency offset detector setting *//* Blind optim */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_CARFREQ, /*0x99 */
				   0x00);
		/*set the SR scan step */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_SFRSTEP, 0x98);
	} else if (pParams->hDemod->ChipID >= 0x20) {
		/*Frequency offset detector setting */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_CARFREQ, 0x6a);
		/*set the SR scan step */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_SFRSTEP, 0x95);
	} else {
		/*Frequency offset detector setting */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_CARFREQ, 0xed);
		/*set the SR scan step */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_SFRSTEP, 0x73);
	}

	/* S1 and  S2 search in // if both enabled */
	ChipSetFieldImage(pParams->hDemod, FSTV0903_S1S2_SEQUENTIAL, 0);
	ChipSetRegisters(pParams->hDemod, RSTV0903_DMDCFG2, 1);

	if (pParams->DemodSymbolRate <= 2000000)
		carrierStep = 1000;	/*SR<=2Msps ==> step = 1MHz */
	else if (pParams->DemodSymbolRate <= 5000000)
		carrierStep = 2000;	/*SR<=5Msps ==> step = 2MHz */
	else if (pParams->DemodSymbolRate <= 12000000)
		carrierStep = 3000;	/*SR<=12Msps ==> step = 3MHz */
	else
		carrierStep = 5000;	/*SR>12Msps ==> step = 5MHz */

	nbSteps = -1 + ((pParams->DemodSearchRange / 1000) / carrierStep);
	nbSteps /= 2;
	nbSteps = (2 * nbSteps) + 1;
	if (nbSteps < 0)
		nbSteps = 1;

	else if (nbSteps > 10) {
		nbSteps = 11;
		carrierStep = (pParams->DemodSearchRange / 1000) / 10;
	}

	currentStep = 0;
	direction = 1;
	tunerFreq = pParams->TunerFrequency;
	while ((timingLock == FALSE) && (currentStep < nbSteps)) {
		/*Reset the Demod */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_DMDISTATE, 0x5F);

		/* Write the MSB */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_CFRINIT1, 0);
		/* Write the LSB */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_CFRINIT0, 0);

		/* Write the MSB */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_SFRINIT1, 0);
		/* Write the LSB */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_SFRINIT0, 0);

		/*Trig an acquisition (start the search) */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_DMDISTATE, 0x40);

		/*wait for coarse carrier lock */
		ChipWaitOrAbort(pParams->hDemod, 50);
		timingcpt = 0;
		for (i = 0; i < 10; i++) {
			if (ChipGetField
			    (pParams->hDemod, FSTV0903_TMGLOCK_QUALITY) >= 2)
				/*read the timing lock quality */
				timingcpt++;
			/*read the AGC2 integrator value */
			ChipGetRegisters(pParams->hDemod, RSTV0903_AGC2I1, 2);
			agc2Integrateur +=
			    (ChipGetFieldImage
			     (pParams->hDemod,
			      FSTV0903_AGC2_INTEGRATOR1) << 8) |
			    ChipGetFieldImage(pParams->hDemod,
					      FSTV0903_AGC2_INTEGRATOR0);

		}
		agc2Integrateur /= 10;
		coarseSymbolRate =
		    FE_STV0903_GetSymbolRate(pParams->hDemod,
					     pParams->MasterClock);

		currentStep++;
		direction *= -1;

		if ((timingcpt >= 5) && (agc2Integrateur < agc2Th)
		    && (coarseSymbolRate < 55000000)
		    && (coarseSymbolRate > 850000)) {
			timingLock = TRUE;
		}

		else if (currentStep < nbSteps) {
			/*Set the tuner frequency and bandwidth */
			/*if timing not locked and currentstep < maxstep move
			 * the tuner freq */
			if (direction > 0)
				tunerFreq += (currentStep * carrierStep);
			else
				tunerFreq -= (currentStep * carrierStep);
			FE_STV0903_SetTuner(pParams->hDemod, pParams->hTuner,
					    pParams->TunerType, tunerFreq,
					    pParams->TunerBW);
		}
	}

	if (timingLock == FALSE)
		coarseSymbolRate = 0;
	else
		coarseSymbolRate =
		    FE_STV0903_GetSymbolRate(pParams->hDemod,
					     pParams->MasterClock);

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

U32 FE_STV0903_SearchSRFine(FE_STV0903_InternalParams_t *pParams)
{
	U32 coarseSymbolRate, coarseFreq, symb, symbmax, symbmin, symbcomp;

	/*read the coarse SR */
	coarseSymbolRate =
	    FE_STV0903_GetSymbolRate(pParams->hDemod, pParams->MasterClock);
	/* read the coarse freq offset */
	coarseFreq =
	    (ChipGetOneRegister(pParams->hDemod, RSTV0903_CFR2) << 8) |
	    ChipGetOneRegister(pParams->hDemod, RSTV0903_CFR1);

	/*SFRUP = SFR + 30% *//*Blind Optim */
	symbcomp = 14 * (coarseSymbolRate / 10);
	if (symbcomp < pParams->DemodSymbolRate) {
		/* if coarsSR + 30% < symbol do not continue */
		/*if coarse SR +30% < user given SR ==> end, the SR given by
		 * the user in blind search is the minimum expected SR  */
		coarseSymbolRate = 0;

	} else {

		/* In case of Path 1 */

		/*Reset the Demod */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_DMDISTATE, 0x1F);

		/*Slow down the timing loop */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_TMGCFG2, 0xC1);
		/*timing lock TH high setting for fine state */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_TMGTHRISE, 0x20);
		/*timing lock TH low setting for fine state */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_TMGTHFALL, 0x00);
		ChipSetOneRegister(pParams->hDemod, RSTV0903_TMGCFG, 0xd2);

		ChipSetFieldImage(pParams->hDemod, FSTV0903_CFR_AUTOSCAN, 0);
		ChipSetRegisters(pParams->hDemod, RSTV0903_DMDCFGMD, 1);

		ChipSetOneRegister(pParams->hDemod, RSTV0903_AGC2REF, 0x38);
		if (pParams->hDemod->ChipID >= 0x30)
			/*Frequency offset detector setting */
			ChipSetOneRegister(pParams->hDemod, RSTV0903_CARFREQ,
					   0x79);
		else if (pParams->hDemod->ChipID >= 0x20)
			/*Frequency offset detector setting */
			ChipSetOneRegister(pParams->hDemod, RSTV0903_CARFREQ,
					   0x49);
		else
			ChipSetOneRegister(pParams->hDemod, RSTV0903_CARFREQ,
					   0xed);

		if (coarseSymbolRate > 3000000) {
			/*SFRUP = SFR + 30% */
			symbmax = 14 * (coarseSymbolRate / 10);
			/* Formulat :init_symbol_rate_regs = SR *
			 * 2^16/MasterClock */
			symbmax = (symbmax / 1000) * 65536;
			symbmax /= (pParams->MasterClock / 1000);

			/*SFRLOW = SFR - 30% */
			symbmin = 10 * (coarseSymbolRate / 16);
			/* Formulat :init_symbol_rate_regs = SR *
			 * 2^16/MasterClock */
			symbmin = (symbmin / 1000) * 65536;
			symbmin /= (pParams->MasterClock / 1000);

			/* Formulat :init_symbol_rate_regs = SR *
			 * 2^16/MasterClock */
			symb = (coarseSymbolRate / 1000) * 65536;
			symb /= (pParams->MasterClock / 1000);
		} else {
			/*SFRUP = SFR + 30% */
			symbmax = 14 * (coarseSymbolRate / 10);
			/* Formulat :init_symbol_rate_regs = SR *
			 * 2^16/MasterClock */
			symbmax = (symbmax / 100) * 65536;
			symbmax /= (pParams->MasterClock / 100);

			/*SFRLOW = SFR - 30% */
			symbmin = 10 * (coarseSymbolRate / 16);
			/* Formulat :init_symbol_rate_regs = SR *
			 * 2^16/MasterClock */
			symbmin = (symbmin / 100) * 65536;
			symbmin /= (pParams->MasterClock / 100);

			/* Formulat :init_symbol_rate_regs = SR *
			 * 2^16/MasterClock */
			symb = (coarseSymbolRate / 100) * 65536;
			symb /= (pParams->MasterClock / 100);
		}

		/* Write the MSB */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_SFRUP1,
				   (symbmax >> 8) & 0x7F);
		/* Write the LSB */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_SFRUP0,
				   (symbmax & 0xFF));

		/* Write the MSB */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_SFRLOW1,
				   (symbmin >> 8) & 0x7F);
		/* Write the LSB */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_SFRLOW0,
				   (symbmin & 0xFF));

		/* Write the MSB */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_SFRINIT1,
				   (symb >> 8) & 0xFF);
		/* Write the LSB */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_SFRINIT0,
				   (symb & 0xFF));

		/*continue the fine state after coarse state convergence */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_DMDT0M, 0x20);
		/* set the init freq offset to the coarse freq offsset */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_CFRINIT1,
				   (coarseFreq >> 8) & 0xff);
		/* set the init freq offset to the coarse freq offsset */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_CFRINIT0,
				   coarseFreq & 0xff);
		/*Trig an acquisition (start the search) with cold start */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_DMDISTATE, 0x15);
	}
	return coarseSymbolRate;
}

U16 FE_STV0903_BlindCheckAGC2MinLevel(FE_STV0903_InternalParams_t *pParams)
{
	U32 minagc2level = 0xffff, agc2level, initFreq, freqStep;

	S32 i, j, nbSteps, direction;

	ChipSetOneRegister(pParams->hDemod, RSTV0903_AGC2REF, 0x38);

	/*Enable the SR SCAN */
	ChipSetFieldImage(pParams->hDemod, FSTV0903_SCAN_ENABLE, 0);
	/*activate the carrier frequency search loop */
	ChipSetFieldImage(pParams->hDemod, FSTV0903_CFR_AUTOSCAN, 0);
	ChipSetRegisters(pParams->hDemod, RSTV0903_DMDCFGMD, 1);

	ChipSetFieldImage(pParams->hDemod, FSTV0903_AUTO_GUP, 1);
	/* set the SR max to 65Msps Write the MSB, auto mode */
	ChipSetRegisters(pParams->hDemod, RSTV0903_SFRUP1, 1);

	ChipSetFieldImage(pParams->hDemod, FSTV0903_AUTO_GLOW, 1);
	/* set the SR min to 400 Ksps Write the MSB */
	ChipSetRegisters(pParams->hDemod, RSTV0903_SFRLOW1, 1);

	/*force the acquisition to stop at coarse carrier state */
	ChipSetOneRegister(pParams->hDemod, RSTV0903_DMDT0M, 0x0);

	FE_STV0903_SetSymbolRate(pParams->hDemod, pParams->MasterClock,
				 1000000);

	nbSteps = (pParams->DemodSearchRange / 1000000);
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
		ChipSetOneRegister(pParams->hDemod, RSTV0903_DMDISTATE, 0x5C);
		/* set the init freq offset */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_CFRINIT1,
				   (initFreq >> 8) & 0xff);
		/* set the init freq offset to the coarse freq offsset */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_CFRINIT0,
				   initFreq & 0xff);
		/*Reset the Demod */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_DMDISTATE, 0x58);
		ChipWaitOrAbort(pParams->hDemod, 10);

		agc2level = 0;
		for (j = 0; j < 10; j++) {
			/*read the AGC2 integrator value */
			ChipGetRegisters(pParams->hDemod, RSTV0903_AGC2I1, 2);
			agc2level +=
			    (ChipGetFieldImage
			     (pParams->hDemod,
			      FSTV0903_AGC2_INTEGRATOR1) << 8) |
			    ChipGetFieldImage(pParams->hDemod,
					      FSTV0903_AGC2_INTEGRATOR0);
		}
		agc2level /= 10;

		if (agc2level < minagc2level)
			minagc2level = agc2level;
	}

	return (U16) minagc2level;
}

BOOL FE_STV0903_BlindSearchAlgo(FE_STV0903_InternalParams_t *pParams)
{

	S32 kRefTmg, kRefTmgMax, kRefTmgMin;

	U32 coarseSymbolRate, agc2Threshold;
	BOOL lock = FALSE, coarseFail = FALSE;
	S32 demodTimeout = 500, failCpt, i, agc2OverFlow, fecTimeout = 50;
	U16 agc2Integrateur;

	U8 dstatus2;

#ifdef HOST_PC
#ifndef NO_GUI
	char message[100];
#endif
#endif

	if (pParams->hDemod->ChipID == 0x10)
		/*Set Correlation frames reg for acquisition for cut 1.0 only */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_CORRELEXP, 0xAA);

	if (pParams->hDemod->ChipID < 0x20) {
		/*Not applicable for cut 2.0 */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_CARHDR, 0x55);
		kRefTmgMax = 233;	/*KT max */
		kRefTmgMin = 143;	/*KT min */
	} else {
		/*Not applicable for cut 2.0 */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_CARHDR, 0x20);
		kRefTmgMax = 110;	/*KT max */
		kRefTmgMin = 10;	/*KT min */

	}

	if (pParams->hDemod->ChipID <= 0x20)
		agc2Threshold = STV0903_BLIND_SEARCH_AGC2_TH;
	else
		agc2Threshold = STV0903_BLIND_SEARCH_AGC2_TH_CUT30;

	if (pParams->hDemod->ChipID <= 0x20)
		/*Set The carrier search up and low to auto mode */
		/*applicable for cut 1.x and 2.0 */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_CARCFG, 0xC4);
	else
		/*cut 3.0 above */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_CARCFG, 0x06);

	/*reduce the timing bandwith for high SR */
	ChipSetOneRegister(pParams->hDemod, RSTV0903_RTCS2, 0x44);

	agc2Integrateur = FE_STV0903_BlindCheckAGC2MinLevel(pParams);
#ifdef HOST_PC
#ifndef NO_GUI
	Fmt(message, "AGC2 MIN  = %d", agc2Integrateur);
	ReportInsertMessage(message);

#endif
#endif

	if (agc2Integrateur > agc2Threshold) {
		/*if the AGC2 > 700 then no signal around the current frequency
		 * */
		lock = FALSE;

#ifdef HOST_PC
#ifndef NO_GUI
		ReportInsertMessage("NO AGC2 : NO Signal");
#endif
#endif
		return lock;
	}

	if (pParams->hDemod->ChipID >= 0x20) {
		if (pParams->hDemod->ChipID == 0x30) {
			/* Blind optim */
			ChipSetOneRegister(pParams->hDemod,
					   RSTV0903_KTTMG, 0x55);
			/* Blind optim */
			ChipSetOneRegister(pParams->hDemod,
					   RSTV0903_AGC2O, 0x5f);
		}
		ChipSetOneRegister(pParams->hDemod, RSTV0903_EQUALCFG,
				   0x41);
		ChipSetOneRegister(pParams->hDemod, RSTV0903_FFECFG,
				   0x41);

		if ((pParams->DemodSearchStandard ==
		     FE_SAT_SEARCH_DVBS1)
		    || (pParams->DemodSearchStandard ==
			FE_SAT_SEARCH_DSS)
		    || (pParams->DemodSearchStandard ==
			FE_SAT_AUTO_SEARCH)) {
			/* open the ReedSolomon to viterbi feed back
			 * until demod lock */
			ChipSetOneRegister(pParams->hDemod,
					   RSTV0903_VITSCALE, 0x82);
			/*set Viterbi hysteresis for search */
			ChipSetOneRegister(pParams->hDemod,
					   RSTV0903_VAVSRVIT, 0x0);
		}
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

		ChipSetOneRegister(pParams->hDemod, RSTV0903_KREFTMG, kRefTmg);

		if (FE_STV0903_SearchSRCoarse(pParams) != 0) {
			/*if coarse SR != 0 then the coarse state is locked
			 * continue with the fine */
			coarseSymbolRate = FE_STV0903_SearchSRFine(pParams);
			if (coarseSymbolRate != 0) {
				/*if if coarse SR != 0then the SR found by the
				 * coarse is >= to the minimum expected SR
				 * (given by the user)
				 in this case wait until full lock or timout
				 */
				FE_STV0903_GetLockTimeout(&demodTimeout,
						&fecTimeout, coarseSymbolRate,
						FE_SAT_BLIND_SEARCH);
				lock = FE_STV0903_GetDemodLock(pParams->hDemod,
						demodTimeout);
			} else {
				lock = FALSE;
			}
			kRefTmg = kRefTmg - 20;
			continue;
		}
		failCpt = 0;
		agc2OverFlow = 0;
		for (i = 0; i < 10; i++) {
			ChipGetRegisters(pParams->hDemod, RSTV0903_AGC2I1, 2);
			agc2Integrateur = (ChipGetFieldImage(pParams->hDemod,
					     FSTV0903_AGC2_INTEGRATOR1) << 8) |
					ChipGetFieldImage(pParams->hDemod,
						FSTV0903_AGC2_INTEGRATOR0);

			if (agc2Integrateur >= 0xff00)
				agc2OverFlow++;

			dstatus2 = ChipGetOneRegister(pParams->hDemod,
					RSTV0903_DSTATUS2);
			if (((dstatus2 & 0x1) == 0x1)
			    && ((dstatus2 >> 7) == 1))
				/*if CFR over flow and timing overflow nothing
				 * here */
				failCpt++;
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

		kRefTmg = kRefTmg - 20;

	} while ((kRefTmg >= kRefTmgMin) && (lock == FALSE)
		 && (coarseFail == FALSE));

	if (pParams->hDemod->ChipID == 0x30)
		/* Blind optim */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_AGC2O, 0x5b);

	return lock;

}

U32 FE_STV0903_CalcBitRate(FE_STV0903_TrackingStandard_t Standard,
			   U32 SymbolRate, FE_STV0903_Rate_t PunctureRate,
			   FE_STV0903_ModCod_t ModCode)
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
--FUNCTION	::	FE_STV0903_Algo
--ACTION	::	Start search for a valid DVBS1/DVBS2 or DSS transponder
--PARAMS IN	::	pParams	->Pointer FE_STV0903_InternalParams_t structer
--PARAMS OUT::	NONE
--RETURN	::	SYGNAL TYPE
--***************************************************/
FE_STV0903_SIGNALTYPE_t FE_STV0903_Algo(FE_STV0903_InternalParams_t *pParams)
{
	S32 demodTimeout = 500,
	    fecTimeout = 50,
	    iqPower, agc1Power, i, powerThreshold = STV0903_IQPOWER_THRESHOLD;

	BOOL lock = FALSE;
	BOOL noSignal = FALSE;

	U32 dataRate = 0, TSRate = 0;
	FE_STV0903_SIGNALTYPE_t signalType = FE_SAT_NOCARRIER;

	/*check if DVBCI mode */
	if (ChipGetFieldImage(pParams->hDemod, FSTV0903_TSFIFO_DVBCI) == 1) {
		/*if DVBCI mode then set the TS clock to manual mode 8.933MHz
		 * before acquistion */
		ChipSetField(pParams->hDemod, FSTV0903_TSFIFO_MANSPEED, 3);
		ChipSetOneRegister(pParams->hDemod, RSTV0903_TSSPEED, 0x3C);
		ChipSetFieldImage(pParams->hDemod, FSTV0903_TSFIFO_DUTY50, 1);
	}
	/*release reset DVBS2 packet delin */
	ChipSetField(pParams->hDemod, FSTV0903_ALGOSWRST, 0);
	/* Blind optim */
	ChipSetOneRegister(pParams->hDemod, RSTV0903_AGC2O, 0x5b);

	/*Stop the Path 1 Stream Merger during acquisition */
	ChipSetFieldImage(pParams->hDemod, FSTV0903_RST_HWARE, 1);
	ChipSetRegisters(pParams->hDemod, RSTV0903_TSCFGH, 1);

	/* Demod Stop */
	ChipSetOneRegister(pParams->hDemod, RSTV0903_DMDISTATE, 0x5C);

	if (pParams->hDemod->ChipID >= 0x20) {
		if (pParams->DemodSymbolRate > 5000000)
			/*nominal value for cut 2.0 */
			ChipSetOneRegister(pParams->hDemod, RSTV0903_CORRELABS,
					   0x9e);
		else
			/*nominal value for cut 2.0 */
			ChipSetOneRegister(pParams->hDemod, RSTV0903_CORRELABS,
					   0x82);
	} else
		/*nominal value for cut 1.x */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_CORRELABS, 0x88);

	/*Get the demod and FEC timeout recomended value depending of the
	 * symbol rate and the search algo */
	FE_STV0903_GetLockTimeout(&demodTimeout, &fecTimeout,
				  pParams->DemodSymbolRate,
				  pParams->DemodSearchAlgo);

	if (pParams->DemodSearchAlgo == FE_SAT_BLIND_SEARCH) {
		/* If the Symbolrate is unknown  set the BW to the maximum */
		pParams->TunerBW = 2 * 36000000;
		/* if Blind search (Symbolrate is unknown) use a wider symbol
		 * rate scan range */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_TMGCFG2, 0x00);
		ChipSetOneRegister(pParams->hDemod, RSTV0903_CORRELMANT, 0x70);
		/* If Blind search set the init symbol rate to 1Msps */
		FE_STV0903_SetSymbolRate(pParams->hDemod, pParams->MasterClock,
					 1000000);
	} else {
		/* If Symbolrate is known  set the BW to appropriate value */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_DMDT0M, 0x20);

		ChipSetOneRegister(pParams->hDemod, RSTV0903_TMGCFG, 0xd2);

		if (pParams->DemodSymbolRate < 2000000)
			/*If SR<2Msps set CORRELMANT to 0x63 */
			ChipSetOneRegister(pParams->hDemod, RSTV0903_CORRELMANT,
					   0x63);
		else
			/*If SR>=2Msps set CORRELMANT to 0x70 */
			ChipSetOneRegister(pParams->hDemod, RSTV0903_CORRELMANT,
					   0x70);

		/*AGC2REF acquistion = AGC2REF tracking = 0x38 */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_AGC2REF, 0x38);

		if (pParams->hDemod->ChipID >= 0x20) {
			ChipSetOneRegister(pParams->hDemod, RSTV0903_KREFTMG,
					   0x5a);
			if (pParams->DemodSearchAlgo == FE_SAT_COLD_START)
				pParams->TunerBW = (15 *
					(FE_STV0903_CarrierWidth(
					    pParams->DemodSymbolRate,
					    pParams->RollOff) + 10000000)) / 10;
			else if (pParams->DemodSearchAlgo == FE_SAT_WARM_START)
				/*WARM START */
				pParams->TunerBW = FE_STV0903_CarrierWidth(
						     pParams->DemodSymbolRate,
						     pParams->RollOff) +
						   10000000;
		} else {
			ChipSetOneRegister(pParams->hDemod, RSTV0903_KREFTMG,
					   0xc1);
			pParams->TunerBW = (15 * (FE_STV0903_CarrierWidth(
						     pParams->DemodSymbolRate,
						     pParams->RollOff) +
						10000000)) / 10;
		}
		/* if cold start or warm  (Symbolrate is known) use a Narrow
		 * symbol rate scan range */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_TMGCFG2, 0x01);
		/* Set the Init Symbol rate */
		FE_STV0903_SetSymbolRate(pParams->hDemod, pParams->MasterClock,
					 pParams->DemodSymbolRate);
		FE_STV0903_SetMaxSymbolRate(pParams->hDemod,
					    pParams->MasterClock,
					    pParams->DemodSymbolRate);
		FE_STV0903_SetMinSymbolRate(pParams->hDemod,
					    pParams->MasterClock,
					    pParams->DemodSymbolRate);
	}

	/*Set the tuner frequency and bandwidth */
	FE_STV0903_SetTuner(pParams->hDemod, pParams->hTuner,
			    pParams->TunerType, pParams->TunerFrequency,
			    pParams->TunerBW);

	/*      NO signal Detection */
	/*Read PowerI and PowerQ To check the signal Presence */
	ChipWaitOrAbort(pParams->hDemod, 10);
	ChipGetRegisters(pParams->hDemod, RSTV0903_AGCIQIN1, 2);
	agc1Power =
	    MAKEWORD(ChipGetFieldImage(pParams->hDemod, FSTV0903_AGCIQ_VALUE1),
		     ChipGetFieldImage(pParams->hDemod, FSTV0903_AGCIQ_VALUE0));
	iqPower = 0;
	if (agc1Power == 0) {
		/*if AGC1 integrator value is 0 then read POWERI, POWERQ
		 * registers */
		/*Read the IQ power value */
		for (i = 0; i < 5; i++) {
			ChipGetRegisters(pParams->hDemod, RSTV0903_POWERI, 2);
			iqPower +=
			    (ChipGetFieldImage(pParams->hDemod,
					       FSTV0903_POWER_I) +
			     ChipGetFieldImage(pParams->hDemod,
					       FSTV0903_POWER_Q)) / 2;
		}
		iqPower /= 5;
	}
#ifdef HOST_PC
#ifndef NO_GUI
	/*if PC GUI read the IQ power threshold form the GUI user parameters */
	UsrRdInt("NOSignalThreshold", &powerThreshold);
#endif
#endif

	if ((agc1Power == 0) && (iqPower < powerThreshold)) {
		/*If (AGC1=0 and iqPower<IQThreshold)  then no signal  */
		/*if AGC1 integrator ==0 and iqPower < Threshold then NO signal
		 * */
		pParams->DemodResults.Locked = FALSE;
#ifdef HOST_PC
#ifndef NO_GUI
		ReportInsertMessage
		    ("NO AGC1 signal, NO POWERI, POWERQ Signal ");
		ReportInsertMessage
		    ("---------------------------------------------");
#endif
#endif
		return signalType;
	}
	/*Set the IQ inversion search mode */
	ChipSetFieldImage(pParams->hDemod, FSTV0903_SPECINV_CONTROL,
			  pParams->DemodSearch_IQ_Inv);
	/*Set the rolloff to manual search mode */
	if (pParams->hDemod->ChipID <= 0x20)	/*cut 2.0 */
		ChipSetFieldImage(pParams->hDemod,
				  FSTV0903_MANUALSX_ROLLOFF, 1);
	else		/*cut 3.0 */
		ChipSetFieldImage(pParams->hDemod,
				  FSTV0903_MANUALS2_ROLLOFF, 1);

	ChipSetRegisters(pParams->hDemod, RSTV0903_DEMOD, 1);

	FE_STV0903_SetSearchStandard(pParams);
	if (pParams->DemodSearchAlgo != FE_SAT_BLIND_SEARCH)
		FE_STV0903_StartSearch(pParams);

	/*Release stream merger reset */
	ChipSetFieldImage(pParams->hDemod, FSTV0903_RST_HWARE, 0);
	ChipSetRegisters(pParams->hDemod, RSTV0903_TSCFGH, 1);

	ChipWaitOrAbort(pParams->hDemod, 3);

	/*Stop the Path 1 Stream Merger during acquisition */
	ChipSetFieldImage(pParams->hDemod, FSTV0903_RST_HWARE, 1);
	ChipSetRegisters(pParams->hDemod, RSTV0903_TSCFGH, 1);

	/*Release stream merger reset */
	ChipSetFieldImage(pParams->hDemod, FSTV0903_RST_HWARE, 0);
	ChipSetRegisters(pParams->hDemod, RSTV0903_TSCFGH, 1);

	if (pParams->DemodSearchAlgo == FE_SAT_BLIND_SEARCH)
		/*special algo for blind search only */
		lock = FE_STV0903_BlindSearchAlgo(pParams);
	else if (pParams->DemodSearchAlgo == FE_SAT_COLD_START)
		/*wait for demod lock specific for blind search */
		lock =
		    FE_STV0903_GetDemodLockCold(pParams, demodTimeout);
	else if (pParams->DemodSearchAlgo == FE_SAT_WARM_START)
		/* case warm or cold start wait for demod lock */
		lock =
		    FE_STV0903_GetDemodLock(pParams->hDemod,
					    demodTimeout);

	if ((lock == FALSE)
	    && (pParams->DemodSearchAlgo == FE_SAT_COLD_START)) {
		if (pParams->DemodSymbolRate >= 10000000) {
			/*If Timing OK & SR >= 10Msps run the SW Algo */
			if (FE_STV0903_CheckTimingLock(pParams->hDemod)
			    == TRUE)
				/* if demod lock fail and cold start
				 * (zapping mode ) run the SW
				 * acquisition algo */
				lock = FE_STV0903_SWAlgo(pParams);
		}
	}

	if (lock == TRUE)
		/* Read signal caracteristics & check the lock range */
		signalType = FE_STV0903_GetSignalParams(pParams);

	if (!(lock == TRUE) || !(signalType == FE_SAT_RANGEOK))
		return signalType;

	/*The tracking optimization and the FEC lock check are
	 * perfomed only if:
	 demod is locked and signal type is RANGEOK i.e a TP
	 found  within the given search range*/

	/* Optimization setting for tracking */
	FE_STV0903_TrackingOptimization(pParams);

	if (pParams->DemodResults.Standard ==
	    FE_SAT_DVBS2_STANDARD) {
		/*if demod is locked , reset the DVBS2 FEC and
		 * wait for lock */
		/*Stop the DVBS2 FEC: */
		ChipSetOneRegister(pParams->hDemod,
				   RSTV0903_TSTRES0, 0xA0);

		/*stop the DVBS2 packet delin */
		ChipSetFieldImage(pParams->hDemod,
				  FSTV0903_ALGOSWRST, 1);
		ChipSetRegisters(pParams->hDemod,
				 RSTV0903_PDELCTRL1, 1);
		/* Stop The stream merger reset */
		ChipSetFieldImage(pParams->hDemod,
				  FSTV0903_RST_HWARE, 1);
		ChipSetRegisters(pParams->hDemod,
				 RSTV0903_TSCFGH, 1);

		/*Release the DVBS2 FEC */
		ChipSetOneRegister(pParams->hDemod,
				   RSTV0903_TSTRES0, 0x20);

		/*Release the DVBS2 packet delin */
		ChipSetFieldImage(pParams->hDemod,
				  FSTV0903_ALGOSWRST, 0);
		ChipSetRegisters(pParams->hDemod,
				 RSTV0903_PDELCTRL1, 1);

		/*Release stream merger reset */
		ChipSetFieldImage(pParams->hDemod,
				  FSTV0903_RST_HWARE, 0);
		ChipSetRegisters(pParams->hDemod,
				 RSTV0903_TSCFGH, 1);
	}

	/*check if DVBCI mode */
	if ((ChipGetFieldImage
	     (pParams->hDemod, FSTV0903_TSFIFO_DVBCI) == 1)
	    &&
	    (ChipGetFieldImage
	     (pParams->hDemod, FSTV0903_TSFIFO_SERIAL) == 0)) {
		/*
		   if DVBCI mode then set the TS clock to auto
		   instantaneous to establish the correct TS
		   clock for the found bit rate* the set back
		   the TS clock to manual so it will note
		   change.
		 */

		dataRate =
		    FE_STV0903_CalcBitRate
		    (pParams->DemodResults.Standard,
		     pParams->DemodResults.SymbolRate,
		     pParams->DemodResults.PunctureRate,
		     pParams->DemodResults.ModCode);
		if (dataRate != 0) {
			/* TS setting when TS Clock must be >
			 * 8.933 ,in this case set the TS clock
			 * to manual before checking the final
			 * lock */
			dataRate /= 8;
			if (pParams->hDemod->ChipID >= 0x30) {
				/*
				   in cut 3.0 the required TS
				   clock is equal to the
				   theoritical byte clock
				   increased by 0.34% times the
				   theoritical byte clock
				   Formula: TS_Clock =
				   TS_ClockTH[1+
				   (TS_ClockTH*0.0034)] with
				   TS_Clock is the required 903
				   TS clock and TS_ClockTH is
				   the tehorical byte clock.
				 */
				TSRate =
				    (dataRate / 10000) *
				    ((34 * dataRate / 1000000) +
				     10000);
			} else {
				/*
				   in cut 2.0 the required TS
				   clock is equal to the
				   theoritical byte clock
				   increased by 0.64% times the
				   theoritical byte clock
				   TS_Clock = TS_ClockTH[1+
				   (TS_ClockTH*0.0064)] with
				   TS_Clock is the required 903
				   TS clock and TS_ClockTH is
				   the tehorical byte clock.
				 */
				TSRate =
				    (dataRate / 10000) *
				    ((64 * dataRate / 1000000) +
				     10000);
			}

			TSRate =
			    4 * (pParams->MasterClock / TSRate);
			if (TSRate < 0x3C) {
				ChipSetOneRegister
				    (pParams->hDemod,
				     RSTV0903_TSSPEED, TSRate);
			}

		}
	}

	if (FE_STV0903_WaitForLock(pParams->hDemod, fecTimeout, fecTimeout) ==
			TRUE) {
		lock = TRUE;
		pParams->DemodResults.Locked = TRUE;

		if (pParams->DemodResults.Standard == FE_SAT_DVBS2_STANDARD) {
			/*case DVBS2 found set the rolloff, rolloff setting for
			 * DVBS2 should be done after full lock (demod+FEC) */
			FE_STV0903_Setdvbs2RollOff(pParams->hDemod);
			pParams->DemodResults.RollOff =
				ChipGetField(pParams->hDemod,
					    FSTV0903_ROLLOFF_STATUS);
			/*reset DVBS2 packet delinator error counter */
			ChipSetFieldImage(pParams->hDemod,
					FSTV0903_RESET_UPKO_COUNT, 1);
			ChipSetRegisters(pParams->hDemod, RSTV0903_PDELCTRL2,
					1);

			/*reset DVBS2 packet delinator error counter */
			ChipSetFieldImage(pParams->hDemod,
					FSTV0903_RESET_UPKO_COUNT, 0);
			ChipSetRegisters(pParams->hDemod, RSTV0903_PDELCTRL2,
					1);

			/* reset error counter to  DVBS2 packet error rate */
			ChipSetOneRegister(pParams->hDemod, RSTV0903_ERRCTRL1,
					0x67);
		} else {
			/* reset the viterbi bit error rate */
			ChipSetOneRegister(pParams->hDemod, RSTV0903_ERRCTRL1,
					0x75);
		}
		/*Reset the Total packet counter */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_FBERCPT4, 0);
		/*Reset the packet Error counter2 (and Set it to infinit error
		 * count mode ) */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_ERRCTRL2, 0xc1);

		/*check if DVBCI mode */
		if ((ChipGetFieldImage(pParams->hDemod,
					FSTV0903_TSFIFO_DVBCI) == 1) &&
		    (ChipGetFieldImage(pParams->hDemod,
					FSTV0903_TSFIFO_SERIAL) == 0)) {
			/*
			   if DVBCI mode then set the TS clock to auto
			   instantaneous to establish the correct TS clock for
			   the found bit rate* the set back the TS clock to
			   manual so it will note change.
			 */

			if (dataRate != 0) {
				/* TS setting when TS Clock must be <= 8.933
				 * ,in this case set the TS clock after
				 * checking the final lock */
				if (TSRate >= 0x3C) {
					/* the operation manual->auto->manual
					 * can only be done after the final
					 * lock */
					ChipSetField(pParams->hDemod,
						FSTV0903_TSFIFO_MANSPEED, 2);
					ChipSetField(pParams->hDemod,
						FSTV0903_TSFIFO_MANSPEED, 3);
				}
			}
		}

	} else {
		lock = FALSE;
		/*if the demod is locked & not the FEC signal type is no DATA */
		signalType = FE_SAT_NODATA;
		noSignal = FE_STV0903_CheckSignalPresence(pParams);
		pParams->DemodResults.Locked = FALSE;
	}

	if (!(signalType == FE_SAT_NODATA) || !(noSignal == FALSE))
		return signalType;
	if (pParams->hDemod->ChipID > 0x11) {
		pParams->DemodResults.Locked = FALSE;
		return signalType;
	}
	/*workaround for cut 1.1 and 1.0 only */
	if ((ChipGetField(pParams->hDemod, FSTV0903_HEADER_MODE) ==
				FE_SAT_DVBS_FOUND) &&
		   (pParams->DemodSearch_IQ_Inv <= FE_SAT_IQ_AUTO_NORMAL_FIRST))
		/*case of false lock DVBS1 in IQ auto mode */
		signalType = FE_STV0903_DVBS1AcqWorkAround(pParams);

	return signalType;
}

BOOL FE_STV0903_LDPCPowerMonitoring(STCHIP_Handle_t hChip)
{
	S32 packetErrorCount, frameErrorCount;

	static BOOL ldpcStopped = FALSE;

	/*read the error counter */
	packetErrorCount =
	    MAKEWORD(ChipGetOneRegister(hChip, RSTV0903_UPCRCKO1),
		     ChipGetOneRegister(hChip, RSTV0903_UPCRCKO0));
	frameErrorCount =
	    MAKEWORD(ChipGetOneRegister(hChip, RSTV0903_BBFCRCKO1),
		     ChipGetOneRegister(hChip, RSTV0903_BBFCRCKO0));

	/*reset DVBS2 packet delinator error counter */
	ChipSetFieldImage(hChip, FSTV0903_RESET_UPKO_COUNT, 1);
	ChipSetRegisters(hChip, RSTV0903_PDELCTRL2, 1);

	/*reset DVBS2 packet delinator error counter */
	ChipSetFieldImage(hChip, FSTV0903_RESET_UPKO_COUNT, 0);
	ChipSetRegisters(hChip, RSTV0903_PDELCTRL2, 1);
	if ((packetErrorCount >= 200) || (frameErrorCount >= 5)) {
		/*disable all frame type (LDPC stoped) */
		FE_STV0903_StopALL_S2_Modcod(hChip);
		ldpcStopped = TRUE;
	} else {
		if (ldpcStopped == TRUE)
			/* activate the LDPC */
			FE_STV0903_ActivateS2ModCode(hChip);
		ldpcStopped = FALSE;
	}

	return ldpcStopped;
}

/****************************************************************
 ****************************************************************
 ****************************************************************
 ****
 ***************************************************
 **PUBLIC ROUTINES**
 ***************************************************
 ****
 ****************************************************************
 ****************************************************************
 ****************************************************************/

/*****************************************************
--FUNCTION	::	FE_STV0903_GetRevision
--ACTION	::	Return current LLA version
--PARAMS IN	::	NONE
--PARAMS OUT::	NONE
--RETURN	::	Revision ==> Text string containing LLA version
--***************************************************/
ST_Revision_t FE_STV0903_GetRevision(void)
{
	return RevisionSTV0903;
}

 /*****************************************************
--FUNCTION	::	FE_STV0903_Init
--ACTION	::	Initialisation of the STV0903 chip
--PARAMS IN	::	Handle	==>	Front End Handle
--PARAMS OUT::	NONE
--RETURN	::	Handle to STV0903
--***************************************************/
FE_STV0903_Error_t FE_STV0903_Init(FE_STV0903_InitParams_t *pInit,
				   FE_STV0903_Handle_t *Handle)
{
	FE_STV0903_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0903_InternalParams_t *pParams = NULL;

	TUNER_Error_t tunerError = TUNER_NO_ERR;

	STCHIP_Error_t demodError;
	/* Demodulator chip initialisation parameters */
	Demod_InitParams_t DemodInitParams;
	SAT_TUNER_InitParams_t TunerInitParams;

	/* Internal params structure allocation */
#ifdef HOST_PC
	STCHIP_Info_t DemodChip;
	pParams = calloc(1, sizeof(FE_STV0903_InternalParams_t));
	(*Handle) = (FE_STV0903_Handle_t) pParams;
#endif
#ifdef CHIP_STAPI
	pParams = (FE_STV0903_InternalParams_t *) (*Handle);
#endif

	if (pParams == NULL || pParams->hDemod == NULL)
		return FE_LLA_INVALID_HANDLE;
	/* Chip initialisation */
#if defined(HOST_PC) && !defined(NO_GUI)

	pParams->hDemod = DEMOD;
	pParams->hTuner = TUNER_A;
	pParams->Quartz = EXTCLK;

	FE_STV0903_SetTunerType(pParams->hDemod,
			pInit->TunerHWControlType, pInit->Tuner_I2cAddr,
			pInit->TunerRefClock, pInit->TunerOutClkDivider);

	ChipSetField(pParams->hDemod, FSTV0903_TUN_IQSWAP,
			pInit->TunerIQ_Inversion);
	ChipSetField(pParams->hDemod, FSTV0903_ROLLOFF_CONTROL, pInit->RollOff);

#else

#ifdef CHIP_STAPI
	DemodInitParams.Chip = (pParams->hDemod);
#else
	DemodInitParams.Chip = &DemodChip;
#endif

	/* Demodulator (STV0903) */
	DemodInitParams.NbDefVal = STV0903_NBREGS;
	DemodInitParams.Chip->RepeaterHost = NULL;
	DemodInitParams.Chip->RepeaterFn = NULL;
	DemodInitParams.Chip->Repeater = FALSE;
	DemodInitParams.Chip->I2cAddr = pInit->DemodI2cAddr;
	strcpy((char *)DemodInitParams.Chip->Name, pInit->DemodName);

	demodError = STV0903_Init(&DemodInitParams, &pParams->hDemod);

	if (pInit->TunerModel != TUNER_NULL)
		TunerInitParams.Model = pInit->TunerModel;
	TunerInitParams.TunerName = pInit->TunerName;
	TunerInitParams.TunerI2cAddress = pInit->Tuner_I2cAddr;
	TunerInitParams.Reference = pInit->TunerRefClock;
	TunerInitParams.IF = 0;
	if (pInit->TunerIQ_Inversion == FE_SAT_IQ_NORMAL)
		TunerInitParams.IQ_Wiring = TUNER_IQ_NORMAL;
	else
		TunerInitParams.IQ_Wiring = TUNER_IQ_INVERT;

	TunerInitParams.BandSelect = pInit->TunerBandSelect;
	TunerInitParams.RepeaterHost = pParams->hDemod;
	TunerInitParams.RepeaterFn = FE_STV0903_RepeaterFn;
	TunerInitParams.DemodModel = DEMOD_STV0903;
	TunerInitParams.OutputClockDivider = pInit->TunerOutClkDivider;

	tunerError = FE_Sat_TunerInit((&TunerInitParams), &pParams->hTuner);

	if (demodError == CHIPERR_NO_ERROR) {
		/*Check the demod error first */
		/*If no Error on the demod the check the Tuners */
		if (tunerError == TUNER_NO_ERR)
			/*if no error on the tuner */
			error = FE_LLA_NO_ERROR;
		else if (tunerError == TUNER_INVALID_HANDLE)
			error = FE_LLA_INVALID_HANDLE;
		else if (tunerError == TUNER_TYPE_ERR)
			/*if tuner type error */
			error = FE_LLA_BAD_PARAMETER;
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
	pParams->hDemod->ChipID = ChipGetOneRegister(pParams->hDemod,
			RSTV0903_MID);

	/*Tuner parameters */
	pParams->TunerType = pInit->TunerHWControlType;
	pParams->TunerGlobal_IQ_Inv = pInit->TunerIQ_Inversion;

	pParams->RollOff = pInit->RollOff;
#if defined(CHIP_STAPI) || defined(NO_GUI)
	/*Ext clock in Hz */
	pParams->Quartz = pInit->DemodRefClock;

	ChipSetField(pParams->hDemod, FSTV0903_ROLLOFF_CONTROL, pInit->RollOff);

	/*Set TS1 and TS2 to serial or parallel mode */
	FE_STV0903_SetTS_Parallel_Serial(pParams->hDemod, pInit->PathTSClock);

	switch (pInit->TunerHWControlType) {
	case FE_SAT_SW_TUNER:
	default:
		/* Set the tuner ref clock */
		FE_Sat_TunerSetReferenceFreq(pParams->hTuner,
							pInit->TunerRefClock);

		/*Set the tuner BBgain */
		FE_Sat_TunerSetGain(pParams->hTuner, pInit->TunerBasebandGain);

		/*Set the ADC's range according to the tuner Model */
		switch (pInit->TunerModel) {
		case TUNER_STB6000:
		case TUNER_STB6100:
			/*Set the ADC range to 2Vpp */
			ChipSetOneRegister(pParams->hDemod, RSTV0903_TSTTNR1,
					0x27);
			break;

		case TUNER_STV6110:
		case TUNER_STV6130:
			/*Set the ADC range to 1Vpp */
			ChipSetOneRegister(pParams->hDemod, RSTV0903_TSTTNR1,
					0x26);
			break;

		default:
			break;

		}

		break;

	case FE_SAT_AUTO_STB6000:
	case FE_SAT_AUTO_STB6100:
	case FE_SAT_AUTO_STV6110:
		/* init the 903 Tuner controller */
		FE_STV0903_SetTunerType(pParams->hDemod,
			pInit->TunerHWControlType, pInit->Tuner_I2cAddr,
			pInit->TunerRefClock, pInit->TunerOutClkDivider);
		/* Set the BB gain using the 903 Tuner controller */
		FE_STV0903_TunerSetBBGain(pParams->hDemod,
			pInit->TunerHWControlType, pInit->TunerBasebandGain);

		if (ChipGetField(pParams->hDemod, FSTV0903_TUN_ACKFAIL)) {
			pParams->hTuner->Error = CHIPERR_I2C_NO_ACK;
			error = FE_LLA_I2C_ERROR;
		}
		break;

	}

	/*IQSWAP setting mast be after FE_STV0903_SetTunerType */
	ChipSetField(pParams->hDemod, FSTV0903_TUN_IQSWAP,
			pInit->TunerIQ_Inversion);

	/*Set the Mclk value to 135MHz */
	FE_STV0903_SetMclk((FE_STV0903_Handle_t)pParams, 135000000,
			pParams->Quartz);
	/*wait for PLL lock */
	ChipWaitOrAbort(pParams->hDemod, 3);
	/* switch the 900 to the PLL */
	ChipSetOneRegister(pParams->hDemod, RSTV0903_SYNTCTRL, 0x22);

#endif

	/*Read the cuurent Mclk */
	pParams->MasterClock = FE_STV0903_GetMclkFreq(pParams->hDemod,
			pParams->Quartz);

	if ((pParams->hDemod->Error) || (pParams->hTuner->Error))
		/*Check the error at the end of the Init */
		error = FE_LLA_I2C_ERROR;

	return error;
}

/*****************************************************
--FUNCTION	::	FE_STV0903_SetTSConfig
--ACTION	::	TS configuration
--PARAMS IN	::	Handle		==>	Front End Handle
			::	pTSConfig	==> TS config parameters
			::
--PARAMS OUT::	TSSpeed_Hz	==> Current TS speed in Hz.
--RETURN	::	Error (if any)
--***************************************************/
FE_STV0903_Error_t FE_STV0903_SetTSConfig(FE_STV0903_Handle_t Handle,
					  FE_STV0903_TSConfig_t *pTSConfig,
					  U32 *TSSpeed_Hz)
{
	FE_STV0903_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0903_InternalParams_t *pParams = NULL;

	U32 tsspeed;

	pParams = (FE_STV0903_InternalParams_t *) Handle;
	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;

	if ((pParams->hDemod->Error) || (pParams->hTuner->Error)) {
		error = FE_LLA_I2C_ERROR;
		return error;
	}
	if (pTSConfig->TSSyncByteEnable == FE_TS_SYNCBYTE_OFF)
		/* enable/Disable SYNC byte */
		ChipSetField(pParams->hDemod, FSTV0903_TSDEL_SYNCBYTE, 1);
	else
		ChipSetField(pParams->hDemod, FSTV0903_TSDEL_SYNCBYTE, 0);

	if (pTSConfig->TSParityBytes == FE_TS_PARITYBYTES_ON)
		/*DVBS1 Data parity bytes enabling/disabling */
		ChipSetField(pParams->hDemod, FSTV0903_TSINSDEL_RSPARITY, 1);
	else
		ChipSetField(pParams->hDemod, FSTV0903_TSINSDEL_RSPARITY, 0);

	if (pTSConfig->TSClockPolarity == FE_TS_RISINGEDGE_CLOCK)
		/*TS clock Polarity setting : rising edge/falling edge */
		ChipSetField(pParams->hDemod, FSTV0903_CLKOUT_XOR, 0);
	else
		ChipSetField(pParams->hDemod, FSTV0903_CLKOUT_XOR, 1);

	/*Set the TS to serial parallel or DVBCI mode */
	FE_STV0903_SetTS_Parallel_Serial(pParams->hDemod, pTSConfig->TSMode);

	if (pTSConfig->TSMode == FE_TS_DVBCI_CLOCK)
		ChipSetField(pParams->hDemod, FSTV0903_TSFIFO_DUTY50, 1);
	if (pTSConfig->TSSpeedControl == FE_TS_MANUAL_SPEED) {
		/*TS speed setting */
		ChipSetField(pParams->hDemod, FSTV0903_TSFIFO_MANSPEED, 3);
		switch (pTSConfig->TSMode) {
		case FE_TS_PARALLEL_PUNCT_CLOCK:
		case FE_TS_DVBCI_CLOCK:
		case FE_TS_OUTPUTMODE_DEFAULT:
		default:
			/*Formulat :
			   TS_Speed_Hz = 4 * Msterclock_Hz / TSSPEED_REG
			 */
			tsspeed = (4 * pParams->MasterClock) /
				pTSConfig->TSClockRate;
			if (tsspeed <= 16)
				/*in parallel clock the TS speed is limited <
				 * MasterClock/4 */
				tsspeed = 16;

			ChipSetOneRegister(pParams->hDemod, RSTV0903_TSSPEED,
					tsspeed);

			break;

		case FE_TS_SERIAL_PUNCT_CLOCK:
		case FE_TS_SERIAL_CONT_CLOCK:
			/*Formulat :
			   TS_Speed_Hz = 32 * Msterclock_Hz / TSSPEED_REG
			 */
			tsspeed = (16 * pParams->MasterClock) /
				(pTSConfig->TSClockRate / 2);
			if (tsspeed <= 32)
				/*in serial clock the TS speed is limited <=
				 * MasterClock */
				tsspeed = 32;

			ChipSetOneRegister(pParams->hDemod, RSTV0903_TSSPEED,
					tsspeed);
			break;

		}

	} else {
		ChipSetField(pParams->hDemod, FSTV0903_TSFIFO_MANSPEED, 0);
	}

	switch (pTSConfig->TSMode) {
		/*D7/D0 permute if serial */
	case FE_TS_SERIAL_PUNCT_CLOCK:
	case FE_TS_SERIAL_CONT_CLOCK:
		if (pTSConfig->TSSwap == FE_TS_SWAP_ON)
			ChipSetField(pParams->hDemod, FSTV0903_TSFIFO_PERMDATA,
					1);
		else
			ChipSetField(pParams->hDemod, FSTV0903_TSFIFO_PERMDATA,
					0);
		break;

	case FE_TS_PARALLEL_PUNCT_CLOCK:
	case FE_TS_DVBCI_CLOCK:
		ChipSetField(pParams->hDemod, FSTV0903_TSFIFO_PERMDATA, 0);
		break;

	default:
		break;
	}

	if (ChipGetField(pParams->hDemod, FSTV0903_TSFIFO_SERIAL) == 0)
		/*Parallel mode */
		*TSSpeed_Hz = (4 * pParams->MasterClock) /
			ChipGetOneRegister(pParams->hDemod, RSTV0903_TSSPEED);
	else {	/*serial mode */

		*TSSpeed_Hz = (16 * pParams->MasterClock) /
			ChipGetOneRegister(pParams->hDemod, RSTV0903_TSSPEED);
		(*TSSpeed_Hz) *= 2;
	}

	if ((pParams->hDemod->Error) || (pParams->hTuner->Error))
		/*Check the error at the end of the function */
		error = FE_LLA_I2C_ERROR;

	return error;
}

/*****************************************************
--FUNCTION	::	FE_STV0903_GetSignalInfo
--ACTION	::	Return informations on the locked transponder
--PARAMS IN	::	Handle	==>	Front End Handle
--PARAMS OUT::	pInfo	==> Informations (BER,C/N,power ...)
--RETURN	::	Error (if any)
--***************************************************/
FE_STV0903_Error_t FE_STV0903_GetSignalInfo(FE_STV0903_Handle_t Handle,
					    FE_STV0903_SignalInfo_t *pInfo)
{
	FE_STV0903_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0903_InternalParams_t *pParams = NULL;
	FE_STV0903_SEARCHSTATE_t demodState;

	S32 symbolRateOffset;

	pParams = (FE_STV0903_InternalParams_t *) Handle;

	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;
	if ((pParams->hDemod->Error) || (pParams->hTuner->Error))
		return FE_LLA_I2C_ERROR;

	demodState = ChipGetField(pParams->hDemod, FSTV0903_HEADER_MODE);

	switch (demodState) {
	case FE_SAT_SEARCH:
	case FE_SAT_PLH_DETECTED:
	default:
		pInfo->Locked = FALSE;
		break;
	case FE_SAT_DVBS2_FOUND:
		pInfo->Locked =
		    ChipGetField(pParams->hDemod, FSTV0903_LOCK_DEFINITIF)
		    && ChipGetField(pParams->hDemod, FSTV0903_PKTDELIN_LOCK)
		    && ChipGetField(pParams->hDemod, FSTV0903_TSFIFO_LINEOK);
		break;

	case FE_SAT_DVBS_FOUND:
		pInfo->Locked =
		    ChipGetField(pParams->hDemod, FSTV0903_LOCK_DEFINITIF)
		    && ChipGetField(pParams->hDemod, FSTV0903_LOCKEDVIT)
		    && ChipGetField(pParams->hDemod, FSTV0903_TSFIFO_LINEOK);
		break;
	}

	/* transponder_frequency = tuner +  demod
	 * carrier frequency */
	pInfo->Frequency = FE_STV0903_GetTunerFrequency(pParams->hDemod,
			pParams->hTuner, pParams->TunerType);
	pInfo->Frequency += FE_STV0903_GetCarrierFrequency(pParams->hDemod,
			pParams->MasterClock) / 1000;

	/* Get symbol rate */
	pInfo->SymbolRate = FE_STV0903_GetSymbolRate(pParams->hDemod, pParams->
			MasterClock);
	symbolRateOffset = FE_STV0903_TimingGetOffset(pParams->hDemod,
			pInfo->SymbolRate);
	/* Get timing loop offset */
	pInfo->SymbolRate += symbolRateOffset;

	pInfo->Standard = FE_STV0903_GetStandard(pParams->hDemod);

	pInfo->PunctureRate = FE_STV0903_GetPunctureRate(pParams->hDemod);
	pInfo->ModCode = ChipGetField(pParams->hDemod, FSTV0903_DEMOD_MODCOD);
	pInfo->Pilots = ChipGetFieldImage(pParams->hDemod, FSTV0903_DEMOD_TYPE)
					& 0x01;
	pInfo->FrameLength = ((U32)ChipGetFieldImage(pParams->hDemod,
				FSTV0903_DEMOD_TYPE)) >> 1;
	pInfo->RollOff = ChipGetField(pParams->hDemod, FSTV0903_ROLLOFF_STATUS);

	pInfo->Power = FE_STV0903_GetRFLevel(pParams->hDemod,
			&FE_STV0903_RF_LookUp);

	pInfo->BER = FE_STV0903_GetBer(pParams->hDemod);

	if (pInfo->Standard == FE_SAT_DVBS2_STANDARD) {
		pInfo->C_N = FE_STV0903_CarrierGetQuality(pParams->hDemod,
				&FE_STV0903_S2_CN_LookUp);

		pInfo->Spectrum = ChipGetField(pParams->hDemod,
				FSTV0903_SPECINV_DEMOD);

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
		ChipSetOneRegister(pParams->hDemod, RSTV0903_ERRCTRL1, 0x67);

	} else {	/*DVBS1/DSS */

		pInfo->C_N = FE_STV0903_CarrierGetQuality(pParams->hDemod,
				&FE_STV0903_S1_CN_LookUp);

		pInfo->Spectrum = ChipGetField(pParams->hDemod, FSTV0903_IQINV);

		pInfo->Modulation = FE_SAT_MOD_QPSK;

	}


	if ((pParams->hDemod->Error) || (pParams->hTuner->Error))
		/*Check the error at the end of the function */
		error = FE_LLA_I2C_ERROR;

	return error;
}

/*****************************************************
--FUNCTION	::	FE_STV0903_Tracking
--ACTION	::	Return Tracking informations : lock status, RF level,
			C/N and BER.
--PARAMS IN	::	Handle	==>	Front End Handle
			::
--PARAMS OUT::	pTrackingInfo	==> pointer to FE_Sat_TrackingInfo_t struct.
			::
--RETURN	::	Error (if any)
--***************************************************/
FE_STV0903_Error_t FE_STV0903_Tracking(FE_STV0903_Handle_t Handle,
				       FE_Sat_TrackingInfo_t *pTrackingInfo)
{
	FE_STV0903_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0903_InternalParams_t *pParams = NULL;
	FE_STV0903_SEARCHSTATE_t demodState;

	pParams = (FE_STV0903_InternalParams_t *)Handle;

	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;

	if ((pParams->hDemod->Error) || (pParams->hTuner->Error))
		return FE_LLA_I2C_ERROR;

	pTrackingInfo->Power =
	    FE_STV0903_GetRFLevel(pParams->hDemod, &FE_STV0903_RF_LookUp);
	pTrackingInfo->BER = FE_STV0903_GetBer(pParams->hDemod);
	demodState = ChipGetField(pParams->hDemod, FSTV0903_HEADER_MODE);
	switch (demodState) {
	case FE_SAT_SEARCH:
	case FE_SAT_PLH_DETECTED:
	default:
		pTrackingInfo->Locked = FALSE;
		pTrackingInfo->C_N = 0;
		break;
	case FE_SAT_DVBS2_FOUND:
		pTrackingInfo->Locked =
		    ChipGetField(pParams->hDemod, FSTV0903_LOCK_DEFINITIF)
		    && ChipGetField(pParams->hDemod, FSTV0903_PKTDELIN_LOCK)
		    && ChipGetField(pParams->hDemod, FSTV0903_TSFIFO_LINEOK);

		pTrackingInfo->C_N =
		    FE_STV0903_CarrierGetQuality(pParams->hDemod,
				    &FE_STV0903_S2_CN_LookUp);
		/*reset The error counter to PER */
		/* reset the error counter to  DVBS2 packet error rate */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_ERRCTRL1, 0x67);

		break;

	case FE_SAT_DVBS_FOUND:
		pTrackingInfo->Locked =
		    ChipGetField(pParams->hDemod, FSTV0903_LOCK_DEFINITIF)
		    && ChipGetField(pParams->hDemod, FSTV0903_LOCKEDVIT)
		    && ChipGetField(pParams->hDemod, FSTV0903_TSFIFO_LINEOK);

		pTrackingInfo->C_N =
			FE_STV0903_CarrierGetQuality(pParams->hDemod,
					&FE_STV0903_S1_CN_LookUp);
		break;
	}

	pTrackingInfo->Frequency_IF =
	    FE_STV0903_GetTunerFrequency(pParams->hDemod, pParams->hTuner,
			    pParams->TunerType);
	pTrackingInfo->Frequency_IF +=
		FE_STV0903_GetCarrierFrequency(pParams->hDemod,
			    pParams->MasterClock) / 1000;

	if ((pParams->hDemod->Error) || (pParams->hTuner->Error))
		error = FE_LLA_I2C_ERROR;

	return error;
}

/*****************************************************
--FUNCTION	::	FE_STV903_GetPacketErrorRate
--ACTION	::	Return informations the number of error packet and the
			window packet count
--PARAMS IN	::	Handle	==>	Front End Handle
--PARAMS OUT::	PacketsErrorCount==> Number of packet error, max is 2^23 packet.
	    ::  TotalPacketsCount==> total window packets , max is 2^24 packet.
--RETURN	::	Error (if any)
--***************************************************/
FE_STV0903_Error_t FE_STV903_GetPacketErrorRate(FE_STV0903_Handle_t Handle,
						U32 *PacketsErrorCount,
						U32 *TotalPacketsCount)
{
	FE_STV0903_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0903_InternalParams_t *pParams = NULL;

	U8 packetsCount4 = 0,
	    packetsCount3 = 0,
	    packetsCount2 = 0, packetsCount1 = 0, packetsCount0 = 0;

	pParams = (FE_STV0903_InternalParams_t *) Handle;

	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;
	if ((pParams->hDemod->Error) || (pParams->hTuner->Error))
		return FE_LLA_I2C_ERROR;

	if (FE_STV0903_Status(Handle) == FALSE) {
		/*if Demod+FEC not locked */
		/*Packet error count is set to the maximum value */
		*PacketsErrorCount = 1 << 23;
		/*Total Packet count is set to the maximum value */
		*TotalPacketsCount = 1 << 24;
	} else {
		/*Read the error counter 2 (23 bits) */
		*PacketsErrorCount =
		    FE_STV0903_GetErrorCount(pParams->hDemod,
				    FE_STV0903_COUNTER2) & 0x7FFFFF;

		/*Read the total packet counter 40 bits, read 5 bytes is
		 * mondatory */
		/*Read the Total packet counter byte 5 */
		packetsCount4 =
		    ChipGetOneRegister(pParams->hDemod, RSTV0903_FBERCPT4);
		/*Read the Total packet counter byte 4 */
		packetsCount3 =
		    ChipGetOneRegister(pParams->hDemod, RSTV0903_FBERCPT3);
		/*Read the Total packet counter byte 3 */
		packetsCount2 =
		    ChipGetOneRegister(pParams->hDemod, RSTV0903_FBERCPT2);
		/*Read the Total packet counter byte 2 */
		packetsCount1 =
		    ChipGetOneRegister(pParams->hDemod, RSTV0903_FBERCPT1);
		/*Read the Total packet counter byte 1 */
		packetsCount0 =
		    ChipGetOneRegister(pParams->hDemod, RSTV0903_FBERCPT0);

		if ((packetsCount4 == 0) && (packetsCount3 == 0)) {
			/*Use the counter for a maximum of 2^24 packets */
			*TotalPacketsCount =
			    ((packetsCount2 & 0xFF) << 16) +
			    ((packetsCount1 & 0xFF) << 8) +
			    (packetsCount0 & 0xFF);
		} else {
			*TotalPacketsCount = 1 << 24;
		}

		if (*TotalPacketsCount == 0) {
			/*if the packets count doesn't start yet the packet
			 * error =1 and total packets =1 */
			/* if the packet counter doesn't start there is an FEC
			 * error in the */
			*TotalPacketsCount = 1;
			*PacketsErrorCount = 1;
		}
	}

	/*Reset the Total packet counter */
	ChipSetOneRegister(pParams->hDemod, RSTV0903_FBERCPT4, 0);
	ChipSetField(pParams->hDemod, FSTV0903_BERMETER_RESET, 1);
	ChipSetField(pParams->hDemod, FSTV0903_BERMETER_RESET, 0);
	/*Reset the packet Error counter2 (and Set it to infinit error count
	 * mode ) */
	ChipSetOneRegister(pParams->hDemod, RSTV0903_ERRCTRL2, 0xc1);

	if ((pParams->hDemod->Error) || (pParams->hTuner->Error))
		/*Check the error at the end of the function */
		error = FE_LLA_I2C_ERROR;

	return error;
}

/*****************************************************
--FUNCTION	::	FE_STV903_GetPreViterbiBER
--ACTION	::	Return DEMOD BER (Pre-VITERBI BER) for DVB-S1 only
--PARAMS IN	::	Handle	==>	Front End Handle
--PARAMS OUT::  Pre-VITERBI BER x 10^7
--RETURN	::	Error (if any)
--***************************************************/
FE_STV0903_Error_t FE_STV903_GetPreViterbiBER(FE_STV0903_Handle_t Handle,
					      U32 *PreVirebiBER)
{

	/* warning, when using this function the error counter number 1 is set
	 * to pre-Viterbi BER,
	 the post viterbi BER info (given in FE_STV0900_GetSignalInfo
	 function or FE_STV0900_Tracking is not significant any more
	 */

	FE_STV0903_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0903_InternalParams_t *pParams = NULL;

	U32 errorCount = 10000000, frameSize;
	FE_STV0903_Rate_t punctureRate;

	pParams = (FE_STV0903_InternalParams_t *) Handle;

	if (pParams == NULL) {
		*PreVirebiBER = errorCount;
		return error;
	}
	if (pParams->hDemod->Error) {
		*PreVirebiBER = errorCount;
		return FE_LLA_I2C_ERROR;
	}

	if (ChipGetField(pParams->hDemod, FSTV0903_LOCKEDVIT)) {
		/* check for VITERBI lock */
		/* Set and reset the error counter1 to Pre-Viterbi BER with
		 * observation window =2^6 frames */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_ERRCTRL1, 0x13);
		/* wait for error accumulation */
		ChipWaitOrAbort(pParams->hDemod, 50);

		/* Read the puncture rate */
		punctureRate = FE_STV0903_GetPunctureRate(pParams->hDemod);

		/* Read the error counter */
		errorCount =
		    ((ChipGetOneRegister(pParams->hDemod, RSTV0903_ERRCNT12) &
		      0x7F) << 16) +
		    ((ChipGetOneRegister(pParams->hDemod, RSTV0903_ERRCNT11) &
		      0xFF) << 8) +
		    ((ChipGetOneRegister(pParams->hDemod, RSTV0903_ERRCNT10) &
		      0xFF));

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
--FUNCTION	::	FE_STV0903_Status
--ACTION	::	Return locked status
--PARAMS IN	::	Handle	==>	Front End Handle
--PARAMS OUT::	NONE
--RETURN	::	Bool (locked or not)
--***************************************************/
BOOL FE_STV0903_Status(FE_STV0903_Handle_t Handle)
{

	FE_STV0903_InternalParams_t *pParams = NULL;
	BOOL Locked = FALSE;
	FE_STV0903_SEARCHSTATE_t demodState;
	FE_STV0903_Error_t error = FE_LLA_NO_ERROR;

	pParams = (FE_STV0903_InternalParams_t *) Handle;

	if (pParams == NULL)
		return FALSE;
	if ((pParams->hDemod->Error) || (pParams->hTuner->Error)) {
		error = FE_LLA_I2C_ERROR;
		return FALSE;
	}
	demodState = ChipGetField(pParams->hDemod, FSTV0903_HEADER_MODE);
	switch (demodState) {
	case FE_SAT_SEARCH:
	case FE_SAT_PLH_DETECTED:
	default:
		Locked = FALSE;
		break;

	case FE_SAT_DVBS2_FOUND:
		Locked =
		    ChipGetField(pParams->hDemod, FSTV0903_LOCK_DEFINITIF)
		    && ChipGetField(pParams->hDemod, FSTV0903_PKTDELIN_LOCK)
		    && ChipGetField(pParams->hDemod, FSTV0903_TSFIFO_LINEOK);
		break;

	case FE_SAT_DVBS_FOUND:
		Locked =
		    ChipGetField(pParams->hDemod, FSTV0903_LOCK_DEFINITIF)
		    && ChipGetField(pParams->hDemod, FSTV0903_LOCKEDVIT)
		    && ChipGetField(pParams->hDemod, FSTV0903_TSFIFO_LINEOK);
		break;
	}

	if ((pParams->hDemod->Error) || (pParams->hTuner->Error))
		error = FE_LLA_I2C_ERROR;

	return Locked;

}

/*****************************************************
--FUNCTION	::	FE_STV0903_Unlock
--ACTION	::	Unlock the demodulator , set the demod to idle state
--PARAMS IN	::	Handle	==>	Front End Handle

-PARAMS OUT::	NONE.
--RETURN	::	Error (if any)
--***************************************************/
FE_STV0903_Error_t FE_STV0903_Unlock(FE_STV0903_Handle_t Handle)
{
	FE_STV0903_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0903_InternalParams_t *pParams =
	    (FE_STV0903_InternalParams_t *) Handle;

	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;
	if ((pParams->hDemod->Error) || (pParams->hTuner->Error))
		return FE_LLA_I2C_ERROR;
	/* Demod Stop */
	ChipSetOneRegister(pParams->hDemod, RSTV0903_DMDISTATE, 0x5C);

	if ((pParams->hDemod->Error) || (pParams->hTuner->Error))
		error = FE_LLA_I2C_ERROR;

	return error;
}

/*****************************************************
--FUNCTION	::	FE_STV0903_ResetDevicesErrors
--ACTION	::	Reset Devices I2C error status
--PARAMS IN	::	Handle	==>	Front End Handle
			::
-PARAMS OUT::	NONE.
--RETURN	::	Error (if any)
--***************************************************/
FE_STV0903_Error_t FE_STV0903_ResetDevicesErrors(FE_STV0903_Handle_t Handle)
{
	FE_STV0903_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0903_InternalParams_t *pParams =
	    (FE_STV0903_InternalParams_t *) Handle;

	if (pParams != NULL) {
		if (pParams->hDemod != NULL)
			pParams->hDemod->Error = CHIPERR_NO_ERROR;
		else
			error = FE_LLA_INVALID_HANDLE;

		if (pParams->hTuner != NULL)
			pParams->hTuner->Error = CHIPERR_NO_ERROR;
		else
			error = FE_LLA_INVALID_HANDLE;
	} else
		error = FE_LLA_INVALID_HANDLE;

	return error;
}

/*****************************************************
--FUNCTION	::	FE_STV0903_LDPCPowerTracking
--ACTION	::	track the LDPC activity and stop it when bad CNR < QEF
			for the given modcode.
			In order to reduce the LDPC power consumption it is
			recommended to call this function every 500ms from the
			upper layer.
--PARAMS IN	::	Handle	==>	Front End Handle
--PARAMS OUT::	NONE.
--RETURN	::	S32 0: LDPC stopped
					1: LDPC running.
--***************************************************/
S32 FE_STV0903_LDPCPowerTracking(FE_STV0903_Handle_t Handle)
{

	FE_STV0903_InternalParams_t *pParams =
	    (FE_STV0903_InternalParams_t *) Handle;
	S32 ldpcState = 1;
	FE_STV0903_Error_t error = FE_LLA_NO_ERROR;

	if (pParams == NULL)
		return ldpcState;
	if ((pParams->hDemod->Error) || (pParams->hTuner->Error))
		return ldpcState;
	if (ChipGetField(pParams->hDemod, FSTV0903_HEADER_MODE) ==
			FE_SAT_DVBS2_FOUND)
		if (FE_STV0903_LDPCPowerMonitoring(pParams->hDemod))
			ldpcState = 0;
		else
			ldpcState = 1;
	else
		ldpcState = 0;

	if ((pParams->hDemod->Error) || (pParams->hTuner->Error))
		error = FE_LLA_I2C_ERROR;

	return ldpcState;
}

/*****************************************************
--FUNCTION	::	FE_STV0903_8PSKCarrierAdaptation
--ACTION	::	workaround for frequency shifter with high jitter and
			phase noise
--PARAMS IN	::	Handle	==>	Front End Handle

--PARAMS OUT::	NONE
--RETURN	::	Error (if any)
--***************************************************/
FE_STV0903_Error_t FE_STV0903_8PSKCarrierAdaptation(FE_STV0903_Handle_t Handle,
						    U32 SymbolRate)
{
	/* This is a workaround for DVBS2 8PSK pilots ON modes with a frequency
	 * shifter adding carrier jitter and phase noise */
	/*
	   this function must be called at least one time after each acquistion
	   to apply the workaround if needed

	   This workaround is applicable for cut 2.0 and 1.x only (NOT cut 3.0)

	   The workaround is the following:
	   Read the modcode, read pilots status:
	   if( 8PSK and pilots ON) then set registers ACLC and BCLC
	   respectively to 0x1A and 0x09 if one of these conditions:

	   1/ 8PSK 3/5 and C/N>=7.5dB.
	   2/ 8PSK 3/5 OR 2/3 and C/N>=8.5dB.
	   3/ 8PSK 3/5 ,2/3, OR 3/4 and C/N>=10dB.
	   4/ 8PSK 3/5,2/3,3/4 OR 5/6 and C/N>=11dB.
	   5/ any 8PSK and  C/N>=12dB

	 */

	FE_STV0903_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0903_InternalParams_t *pParams;

	BOOL adaptCut20 = FALSE;

	FE_Sat_TrackingStandard_t standard;
	FE_Sat_ModCod_t modcode;
	FE_Sat_PILOTS_t pilots;

	S32 c_n;		/* 10*C/N in dB */
	U8 aclc, nco2Th;

	static BOOL IFConvertorPresent = TRUE;
	static BOOL IFConvertorDetection = TRUE;

	if (Handle == NULL)
		return FE_LLA_INVALID_HANDLE;

	pParams = (FE_STV0903_InternalParams_t *) Handle;

	if (pParams->hDemod->Error)
		return FE_LLA_I2C_ERROR;

	if (pParams->hDemod->ChipID <= 0x20) {
		standard = FE_STV0903_GetStandard(pParams->hDemod);
		if (standard != FE_SAT_DVBS2_STANDARD)
			goto err_chk_ret;
		modcode =
		    ChipGetField(pParams->hDemod, FSTV0903_DEMOD_MODCOD);
		pilots = ChipGetFieldImage(pParams->hDemod,
				FSTV0903_DEMOD_TYPE) & 0x01;

		if ((pilots != FE_SAT_PILOTS_ON)
				|| !(modcode >= FE_SAT_8PSK_35)
				|| !(modcode <= FE_SAT_8PSK_910))
			goto err_chk_ret;
		/*read C/N FE_STV0903_CarrierGetQuality returns C/N*10dB */
		c_n = FE_STV0903_CarrierGetQuality(pParams->hDemod,
				&FE_STV0903_S2_CN_LookUp);

		if (c_n >= 120) {
			/*C/N >=12dB */
			adaptCut20 = TRUE;
		} else if (c_n >= 110) {
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
			ChipSetFieldImage(pParams->hDemod,
					FSTV0903_CAR_ALPHA_MANT, 1);
			/*ACLC registers image update */
			ChipSetFieldImage(pParams->hDemod,
					FSTV0903_CAR_ALPHA_EXP, 0xA);

			ChipSetFieldImage(pParams->hDemod,
					FSTV0903_CAR_BETA_MANT, 0);
			/*BCLC registers image update */
			ChipSetFieldImage(pParams->hDemod,
					FSTV0903_CAR_BETA_EXP, 0x9);

			/* write both ACLC and BCLC registers in the same I2C
			 * frame */
			/* mondatory to avoide error glitch during when
			 * continuesly monitoring the C/N to adpat registers
			 * values */
			ChipSetRegisters(pParams->hDemod, RSTV0903_ACLC, 2);

		} else {
			ChipSetFieldImage(pParams->hDemod,
					FSTV0903_CAR_ALPHA_MANT, 0);
			/*ACLC registers image update */
			ChipSetFieldImage(pParams->hDemod,
					FSTV0903_CAR_ALPHA_EXP, 0);
			ChipSetFieldImage(pParams->hDemod,
					FSTV0903_CAR_BETA_MANT, 0);
			/*BCLC registers image update */
			ChipSetFieldImage(pParams->hDemod,
					FSTV0903_CAR_BETA_EXP, 0);

			/* write both ACLC and BCLC registers in the same I2C
			 * frame */
			/* mondatory to avoide error glitch during when
			 * continuesly monitoring the C/N to adpat registers
			 * values */
			ChipSetRegisters(pParams->hDemod, RSTV0903_ACLC, 2);

		}
		goto err_chk_ret;
	}
	/* Cut 3.0 */
	if ((IFConvertorDetection == TRUE) || (IFConvertorPresent == TRUE)) {
		standard = FE_STV0903_GetStandard(pParams->hDemod);
		if (standard != FE_SAT_DVBS2_STANDARD)
			goto err_chk_ret;
	}
	if (IFConvertorDetection == TRUE) {
		IFConvertorDetection = FALSE;

		modcode = ChipGetField(pParams->hDemod, FSTV0903_DEMOD_MODCOD);
		if (modcode <= FE_SAT_QPSK_910)
			nco2Th = 0x30;
		else
			nco2Th = 0x20;

		ChipSetOneRegister(pParams->hDemod, RSTV0903_NCO2MAX1, 0);
		ChipWaitOrAbort(pParams->hDemod, 100);
		if (ChipGetOneRegister(pParams->hDemod, RSTV0903_NCO2MAX1) >=
				nco2Th)
			IFConvertorPresent = TRUE;
		else
			IFConvertorPresent = FALSE;
	}
	if (IFConvertorPresent == TRUE) {
		modcode = ChipGetField(pParams->hDemod,
				    FSTV0903_DEMOD_MODCOD);
		pilots = ChipGetFieldImage(pParams->hDemod,
				    FSTV0903_DEMOD_TYPE) & 0x01;

		if (!(modcode >= FE_SAT_8PSK_35)
		    || !(modcode <= FE_SAT_8PSK_910))
			goto err_chk_ret;

		if (pilots == FE_SAT_PILOTS_ON) {
			c_n = FE_STV0903_CarrierGetQuality(pParams->hDemod,
					    &FE_STV0903_S2_CN_LookUp);
			if (c_n > 103) {
				ChipSetOneRegister(pParams->hDemod,
						RSTV0903_ACLC2S28, 0x3A);
				ChipSetOneRegister(pParams->hDemod,
						RSTV0903_BCLC2S28, 0x0A);

				ChipSetOneRegister(pParams->hDemod,
						RSTV0903_CAR2CFG, 0x76);
			} else {
				aclc =
				    FE_STV0903_GetOptimCarrierLoop(SymbolRate,
						    modcode, pilots, 0x30);
				ChipSetOneRegister(pParams->hDemod,
						RSTV0903_CAR2CFG, 0x66);

				ChipSetOneRegister(pParams->hDemod,
						RSTV0903_ACLC2S28, aclc);
				ChipSetOneRegister(pParams->hDemod,
						RSTV0903_BCLC2S28, 0x86);

			}
		} else {	/*Pilots OFF */

			ChipSetOneRegister(pParams->hDemod, RSTV0903_ACLC2S28,
					0x1B);
			ChipSetOneRegister(pParams->hDemod, RSTV0903_BCLC2S28,
					0x8A);

		}
	}
err_chk_ret:
	if (pParams->hDemod->Error)
		error = FE_LLA_I2C_ERROR;


	return error;
}

/*****************************************************
--FUNCTION	::	FE_STV0903_SetMclk
--ACTION	::	Set demod Master Clock
--PARAMS IN	::	Handle	==>	Front End Handle
			::	Mclk	==> demod master clock
			::	ExtClk	==>	external Quartz
--PARAMS OUT::	NONE.
--RETURN	::	Error (if any)
--***************************************************/
FE_STV0903_Error_t FE_STV0903_SetMclk(FE_STV0903_Handle_t Handle, U32 Mclk,
				      U32 ExtClk)
{
	FE_STV0903_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0903_InternalParams_t *pParams =
	    (FE_STV0903_InternalParams_t *) Handle;

	U32 mDiv, clkSel;
	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;
	if ((pParams->hDemod->Error) || (pParams->hTuner->Error))
		return FE_LLA_I2C_ERROR;
	clkSel = ((ChipGetField(pParams->hDemod, FSTV0903_SELX1RATIO) == 1) ? 4
			: 6);
	mDiv = ((clkSel * Mclk) / pParams->Quartz) - 1;
	ChipSetField(pParams->hDemod, FSTV0903_M_DIV, mDiv);
	pParams->MasterClock = FE_STV0903_GetMclkFreq(pParams->hDemod,
			pParams->Quartz);

	/*Set the DiseqC frequency to 22KHz */
	/*
	   Formula:
	   DiseqC_TX_Freq= MasterClock/(32*F22TX_Reg)
	   DiseqC_RX_Freq= MasterClock/(32*F22RX_Reg)
	 */
	mDiv = pParams->MasterClock / 704000;
	ChipSetOneRegister(pParams->hDemod, RSTV0903_F22TX, mDiv);
	ChipSetOneRegister(pParams->hDemod, RSTV0903_F22RX, mDiv);

	/*Check the error at the end of the function */
	if ((pParams->hDemod->Error) || (pParams->hTuner->Error))
		error = FE_LLA_I2C_ERROR;

	return error;
}

/*****************************************************
--FUNCTION	::	FE_STV0903_SetStandby
--ACTION	::	Set demod STANDBAY mode On/Off
--PARAMS IN	::	Handle	==>	Front End Handle

-PARAMS OUT::	NONE.
--RETURN	::	Error (if any)
--***************************************************/
FE_STV0903_Error_t FE_STV0903_SetStandby(FE_STV0903_Handle_t Handle,
					 U8 StandbyOn)
{
	FE_STV0903_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0903_InternalParams_t *pParams =
	    (FE_STV0903_InternalParams_t *) Handle;

	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;
	if ((pParams->hDemod->Error) || (pParams->hTuner->Error))
		return FE_LLA_I2C_ERROR;

	if (StandbyOn) {
		ChipSetField(pParams->hDemod, FSTV0903_STANDBY, 1);

		/*FSK power off */
		ChipSetField(pParams->hDemod, FSTV0903_FSK_PON, 0);
		/*ADC1 power off */
		ChipSetField(pParams->hDemod, FSTV0903_ADC1_PON, 0);
		/*DiseqC 1 power off */
		ChipSetField(pParams->hDemod, FSTV0903_DISEQC1_PON, 0);

		/*Stop Clocks */
		/*Stop packet delineator 1 clock */
		ChipSetField(pParams->hDemod, FSTV0903_STOP_CLKPKDT1, 1);
		/*Stop ADC 1 clock */
		ChipSetField(pParams->hDemod, FSTV0903_STOP_CLKADCI1, 1);
		/*Stop Sampling clock */
		ChipSetField(pParams->hDemod, FSTV0903_STOP_CLKSAMP1, 1);
		/*Stop VITERBI 1 clock */
		ChipSetField(pParams->hDemod, FSTV0903_STOP_CLKVIT1, 1);
		/*Stop FEC clock */
		ChipSetField(pParams->hDemod, FSTV0903_STOP_CLKFEC, 1);
		/*Stop TS clock */
		ChipSetField(pParams->hDemod, FSTV0903_STOP_CLKTS, 1);

	} else {
		ChipSetField(pParams->hDemod, FSTV0903_STANDBY, 0);

		/*FSK power On */
		ChipSetField(pParams->hDemod, FSTV0903_FSK_PON, 1);
		/*ADC1 power On */
		ChipSetField(pParams->hDemod, FSTV0903_ADC1_PON, 1);
		/*DiseqC 1 power On */
		ChipSetField(pParams->hDemod, FSTV0903_DISEQC1_PON, 1);

		/*Stop Clocks */
		/*Activate packet delineator 1 clock */
		ChipSetField(pParams->hDemod, FSTV0903_STOP_CLKPKDT1, 0);
		/*Activate ADC 1 clock */
		ChipSetField(pParams->hDemod, FSTV0903_STOP_CLKADCI1, 0);
		/*Activate Sampling clock */
		ChipSetField(pParams->hDemod, FSTV0903_STOP_CLKSAMP1, 0);
		/*Activate VITERBI 1 clock */
		ChipSetField(pParams->hDemod, FSTV0903_STOP_CLKVIT1, 0);
		/*Activate FEC clock */
		ChipSetField(pParams->hDemod, FSTV0903_STOP_CLKFEC, 0);
		/*Activate TS clock */
		ChipSetField(pParams->hDemod, FSTV0903_STOP_CLKTS, 0);

	}

	/*Set the Tuner to Standby */
	FE_Sat_TunerSetStandby(pParams->hTuner, StandbyOn);

	if ((pParams->hDemod->Error) || (pParams->hTuner->Error))
		/*Check the error at the end of the function */
		error = FE_LLA_I2C_ERROR;

	return error;
}

/*****************************************************
--FUNCTION	::	FE_STV0903_SetAbortFlag
--ACTION	::	Set Abort flag On/Off
--PARAMS IN	::	Handle	==>	Front End Handle

-PARAMS OUT::	NONE.
--RETURN	::	Error (if any)

--***************************************************/
FE_STV0903_Error_t FE_STV0903_SetAbortFlag(FE_STV0903_Handle_t Handle,
					   BOOL Abort)
{
	FE_STV0903_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0903_InternalParams_t *pParams =
	    (FE_STV0903_InternalParams_t *) Handle;

	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;
	if ((pParams->hDemod->Error) || (pParams->hTuner->Error))
		return FE_LLA_I2C_ERROR;
	ChipAbort(pParams->hTuner, Abort);
	ChipAbort(pParams->hDemod, Abort);

	if ((pParams->hDemod->Error) || (pParams->hTuner->Error))
		/*Check the error at the end of the function */
		error = FE_LLA_I2C_ERROR;
	return error;
}

/*****************************************************
--FUNCTION	::	FE_STV0903_Search
--ACTION	::	Search for a valid transponder
--PARAMS IN	::	Handle	==>	Front End Handle
				pSearch ==> Search parameters
				pResult ==> Result of the search
--PARAMS OUT::	NONE
--RETURN	::	Error (if any)
--***************************************************/
FE_STV0903_Error_t FE_STV0903_Search(FE_STV0903_Handle_t Handle,
				     FE_STV0903_SearchParams_t *pSearch,
				     FE_STV0903_SearchResult_t *pResult)
{
	FE_STV0903_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0903_InternalParams_t *pParams;

	if (Handle == NULL)
		return FE_LLA_INVALID_HANDLE;

	pParams = (FE_STV0903_InternalParams_t *) Handle;

	if ((pParams->hDemod->Error) || (pParams->hTuner->Error))
		return FE_LLA_I2C_ERROR;

	if (!(INRANGE(100000, pSearch->SymbolRate, 70000000)) ||
	    !(INRANGE(1000000, pSearch->SearchRange, 50000000)))
		return FE_LLA_BAD_PARAMETER;

	pParams->DemodSearchStandard = pSearch->Standard;
	pParams->DemodSymbolRate = pSearch->SymbolRate;
	pParams->DemodSearchRange = pSearch->SearchRange;
	pParams->TunerFrequency = pSearch->Frequency;
	pParams->DemodSearchAlgo = pSearch->SearchAlgo;
	pParams->DemodSearch_IQ_Inv = pSearch->IQ_Inversion;
	pParams->DemodPunctureRate = pSearch->PunctureRate;
	pParams->DemodModulation = pSearch->Modulation;
	pParams->DemodModcode = pSearch->Modcode;

	if ((FE_STV0903_Algo(pParams) == FE_SAT_RANGEOK)
	    && (pParams->hDemod->Error == CHIPERR_NO_ERROR)) {
		pResult->Locked = pParams->DemodResults.Locked;

		/* update results */
		pResult->Standard = pParams->DemodResults.Standard;
		pResult->Frequency = pParams->DemodResults.Frequency;
		pResult->SymbolRate = pParams->DemodResults.SymbolRate;
		pResult->PunctureRate = pParams->DemodResults.PunctureRate;
		pResult->ModCode = pParams->DemodResults.ModCode;
		pResult->Pilots = pParams->DemodResults.Pilots;
		pResult->FrameLength = pParams->DemodResults.FrameLength;
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

	if ((pParams->hDemod->Error) || (pParams->hTuner->Error))
		/*Check the error at the end of the function */
		error = FE_LLA_I2C_ERROR;

	return error;

}

/*****************************************************
--FUNCTION  ::  FE_STV0903_DVBS2_SetGoldCode
--ACTION    ::  Set the DVBS2 Gold Code
--PARAMS IN ::  Handle  ==> Front End Handle
		U32     ==> cold code value (18bits)
--PARAMS OUT::  NONE
--RETURN    ::  Error (if any )
--***************************************************/
FE_STV0903_Error_t FE_STV0903_DVBS2_SetGoldCodeX(FE_STV0903_Handle_t Handle,
						 U32 GoldCode)
{
	FE_STV0903_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0903_InternalParams_t *pParams;

	pParams = (FE_STV0903_InternalParams_t *) Handle;

	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;
	if ((pParams->hDemod->Error) || (pParams->hTuner->Error))
		return FE_LLA_I2C_ERROR;
	/*Gold code X setting */
	/* bit[3:4] of register PLROOT2
	   3..2 : P1_PLSCRAMB_MODE[1:0] |URW|
	   entry mode p1_plscramb_root
	   00: p1_plscramb_root is the root of PRBS X.
	   01: p1_plscramb_root is the DVBS2 gold code.
	 */

	ChipSetOneRegister(pParams->hDemod, RSTV0903_PLROOT2,
					0x04 | ((GoldCode >> 16) & 0x3));
	ChipSetOneRegister(pParams->hDemod, RSTV0903_PLROOT1,
						(GoldCode >> 8) & 0xff);
	ChipSetOneRegister(pParams->hDemod, RSTV0903_PLROOT0,
							(GoldCode) & 0xff);

	/*Check the error at the end of the function */
	if ((pParams->hDemod->Error) || (pParams->hTuner->Error))
		error = FE_LLA_I2C_ERROR;

	return error;
}

/*****************************************************
--FUNCTION	::	FE_STV0903_DiseqcTxInit
--ACTION	::	Set the diseqC Tx mode
--PARAMS IN	::	Handle	==>	Front End Handle
		::	DiseqCMode	==> diseqc Mode : continues tone, 2/3
			PWM, 3/3 PWM ,2/3 envelop or 3/3 envelop.
--PARAMS OUT::None
--RETURN	::	Error (if any)
--***************************************************/
FE_STV0903_Error_t FE_STV0903_DiseqcInit(FE_STV0903_Handle_t Handle,
					 FE_STV0903_DiseqC_TxMode DiseqCMode)
{
	FE_STV0903_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0903_InternalParams_t *pParams = NULL;

	pParams = (FE_STV0903_InternalParams_t *) Handle;
	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;
	if ((pParams->hDemod->Error) || (pParams->hTuner->Error))
		return FE_LLA_I2C_ERROR;
	ChipSetField(pParams->hDemod, FSTV0903_DISTX_MODE, DiseqCMode);

	ChipSetField(pParams->hDemod, FSTV0903_DISEQC_RESET, 1);
	ChipSetField(pParams->hDemod, FSTV0903_DISEQC_RESET, 0);

	/*Check the error at the end of the function */
	if ((pParams->hDemod->Error) || (pParams->hTuner->Error))
		error = FE_LLA_I2C_ERROR;

	return error;
}
EXPORT_SYMBOL(FE_STV0903_DiseqcInit);

/*****************************************************
--FUNCTION	::	FE_STV0903_DiseqcSend
--ACTION	::	Read receved bytes from DiseqC FIFO
--PARAMS IN	::	Handle	==>	Front End Handle
			::	Data	==> Table of bytes to send.
			::	NbData	==> Number of bytes to send.
--PARAMS OUT::None
--RETURN	::	Error (if any)
--***************************************************/
FE_STV0903_Error_t FE_STV0903_DiseqcSend(FE_STV0903_Handle_t Handle, U8 *Data,
					 U32 NbData)
{
	FE_STV0903_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0903_InternalParams_t *pParams = NULL;
	S32 i = 0;

	pParams = (FE_STV0903_InternalParams_t *) Handle;
	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;
	if ((pParams->hDemod->Error) || (pParams->hTuner->Error))
		return FE_LLA_I2C_ERROR;

	ChipSetField(pParams->hDemod, FSTV0903_DIS_PRECHARGE, 1);
	while (i < NbData) {
		while (ChipGetField(pParams->hDemod, FSTV0903_FIFO_FULL))
			/* wait for FIFO empty  */;

		/* send byte to FIFO :: WARNING don't use set field     !! */
		ChipSetOneRegister(pParams->hDemod, RSTV0903_DISTXDATA,
				Data[i]);
		i++;
	}
	ChipSetField(pParams->hDemod, FSTV0903_DIS_PRECHARGE, 0);
	i = 0;
	while ((ChipGetField(pParams->hDemod, FSTV0903_TX_IDLE) != 1) && (i <
				10)) {
		/*wait until the end of diseqc send operation */
		ChipWaitOrAbort(pParams->hDemod, 10);
		i++;
	}
	if ((pParams->hDemod->Error) || (pParams->hTuner->Error))
		/*Check the error at the end of the function */
		error = FE_LLA_I2C_ERROR;

	return error;
}
EXPORT_SYMBOL(FE_STV0903_DiseqcSend);
/*****************************************************
--FUNCTION	::	FE_STV0903_DiseqcReceive
--ACTION	::	Read receved bytes from DiseqC FIFO
--PARAMS IN	::	Handle	==>	Front End Handle
--PARAMS OUT::	Data	==> Table of received bytes.
			::	NbData	==> Number of received bytes.
--RETURN	::	Error (if any)
--***************************************************/
FE_STV0903_Error_t FE_STV0903_DiseqcReceive(FE_STV0903_Handle_t Handle,
					    U8 *Data, U32 *NbData)
{
	FE_STV0903_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0903_InternalParams_t *pParams = NULL;
	S32 i = 0;

	pParams = (FE_STV0903_InternalParams_t *) Handle;
	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;
	if ((pParams->hDemod->Error) || (pParams->hTuner->Error))
		return FE_LLA_I2C_ERROR;

	*NbData = 0;
	while ((ChipGetField(pParams->hDemod, FSTV0903_RX_END) != 1)
			&& (i < 10)) {
		ChipWaitOrAbort(pParams->hDemod, 10);
		i++;
	}
	if (ChipGetField(pParams->hDemod, FSTV0903_RX_END)) {
		*NbData = ChipGetField(pParams->hDemod, FSTV0903_FIFO_BYTENBR);
		for (i = 0; i < (*NbData); i++)
			Data[i] = ChipGetOneRegister(pParams->hDemod,
					    RSTV0903_DISRXDATA);
	}
	if ((pParams->hDemod->Error) || (pParams->hTuner->Error))
		/*Check the error at the end of the function */
		error = FE_LLA_I2C_ERROR;

	return error;
}
EXPORT_SYMBOL(FE_STV0903_DiseqcReceive);

/*****************************************************
--FUNCTION	::	FE_STV0903_Set22KHzContinues
--ACTION	::	Set the diseqC Tx mode
--PARAMS IN	::	Handle	==>	Front End Handle
--PARAMS OUT::None
--RETURN	::	Error (if any)
--***************************************************/
FE_STV0903_Error_t FE_STV0903_Set22KHzContinues(FE_STV0903_Handle_t Handle,
						BOOL Enable)
{
	FE_STV0903_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0903_InternalParams_t *pParams = NULL;

	pParams = (FE_STV0903_InternalParams_t *) Handle;
	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;
	if ((pParams->hDemod->Error) || (pParams->hTuner->Error))
		return FE_LLA_I2C_ERROR;

	if (Enable == TRUE) {
		ChipSetField(pParams->hDemod, FSTV0903_DISEQC_RESET, 0);
		ChipSetField(pParams->hDemod, FSTV0903_DISEQC_RESET, 1);
		/*release DiseqC reset to enable the 22KHz tone */
		ChipSetField(pParams->hDemod, FSTV0903_DISEQC_RESET, 0);

		/*Set the DiseqC mode to 22Khz continues tone */
		ChipSetField(pParams->hDemod, FSTV0903_DISTX_MODE, 0);
	} else {
		ChipSetField(pParams->hDemod, FSTV0903_DISTX_MODE, 0);

		/*maintain the DiseqC reset to disable the 22KHz tone */
		ChipSetField(pParams->hDemod, FSTV0903_DISEQC_RESET, 1);
	}
	if ((pParams->hDemod->Error) || (pParams->hTuner->Error))
		/*Check the error at the end of the function */
		error = FE_LLA_I2C_ERROR;

	return error;
}
EXPORT_SYMBOL(FE_STV0903_Set22KHzContinues);
#ifdef STM_FE_DEFINED_FOR_FUTURE_USE
/***********************************************************************
**FUNCTION  ::  FE_STV0903_SetupFSK
**ACTION    ::  Setup FSK
**PARAMS IN ::  hChip             -> handle to the chip
**              fskt_carrier      -> FSK modulator carrier frequency  (Hz)
**              fskt_deltaf       -> FSK frequency deviation          (Hz)
**              fskr_carrier      -> FSK demodulator carrier frequency  (Hz)
**PARAMS OUT::  NONE
**RETURN    ::  Symbol frequency
***********************************************************************/
FE_STV0903_Error_t FE_STV0903_SetupFSK(FE_STV0903_Handle_t Handle,
				       U32 TransmitCarrier, U32 ReceiveCarrier,
				       U32 Deltaf)
{
	FE_STV0903_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0903_InternalParams_t *pParams;
	U32 fskt_carrier, fskt_deltaf, fskr_carrier;

	pParams = (FE_STV0903_InternalParams_t *) Handle;
	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;
	if ((pParams->hDemod->Error) || (pParams->hTuner->Error))
		return FE_LLA_I2C_ERROR;

	ChipSetField(pParams->hDemod, FSTV0903_FSKT_KMOD, 0x23);

	/*
	   Formulat:
	   FSKT_CAR =  2^20*transmit_carrier/MasterClock;
	 */
	fskt_carrier = (TransmitCarrier << 8) / (pParams->MasterClock >> 12);

	ChipSetField(pParams->hDemod, FSTV0903_FSKT_CAR2, ((fskt_carrier >> 16)
				& 0x03));
	ChipSetField(pParams->hDemod, FSTV0903_FSKT_CAR1, ((fskt_carrier >> 8)
				& 0xFF));
	ChipSetField(pParams->hDemod, FSTV0903_FSKT_CAR0, (fskt_carrier &
				0xFF));

	/*
	   Formulat:
	   FSKT_DELTAF =  2^20*fskt_deltaf/MasterClock;
	 */

	/* 2^20=1048576 */
	fskt_deltaf = (Deltaf << 20) / pParams->MasterClock;

	ChipSetField(pParams->hDemod, FSTV0903_FSKT_DELTAF1,
			((fskt_deltaf >> 8) & 0x0F));
	ChipSetField(pParams->hDemod, FSTV0903_FSKT_DELTAF0, (fskt_deltaf &
				0xFF));

	ChipSetOneRegister(pParams->hDemod, RSTV0903_FSKTCTRL, 0x04);

	ChipSetOneRegister(pParams->hDemod, RSTV0903_FSKRFC2, 0x10);
	/*
	   Formulat:
	   FSKR_CAR =  2^20*receive_carrier/MasterClock;
	 */

	fskr_carrier = (ReceiveCarrier << 8) / (pParams->MasterClock >> 12);

	ChipSetField(pParams->hDemod, FSTV0903_FSKR_CAR2, ((fskr_carrier >> 16)
				& 0x03));
	ChipSetField(pParams->hDemod, FSTV0903_FSKR_CAR1, ((fskr_carrier >> 8)
				& 0xFF));
	ChipSetField(pParams->hDemod, FSTV0903_FSKR_CAR0, (fskr_carrier &
				0xFF));

	ChipSetOneRegister(pParams->hDemod, RSTV0903_FSKRK1, 0x53);
	ChipSetOneRegister(pParams->hDemod, RSTV0903_FSKRK2, 0x94);

	ChipSetOneRegister(pParams->hDemod, RSTV0903_FSKRAGCR, 0x28);
	ChipSetOneRegister(pParams->hDemod, RSTV0903_FSKRAGC, 0x5F);

	ChipSetOneRegister(pParams->hDemod, RSTV0903_FSKRALPHA, 0x13);
	ChipSetOneRegister(pParams->hDemod, RSTV0903_FSKRPLTH1, 0x90);
	ChipSetOneRegister(pParams->hDemod, RSTV0903_FSKRPLTH0, 0x45);

	ChipSetOneRegister(pParams->hDemod, RSTV0903_FSKRDF1, 0x9f);
	/*
	   Formulat:
	   FSKR_DELTAF =  2^20*fskt_deltaf/MasterClock;
	 */

	ChipSetField(pParams->hDemod, FSTV0903_FSKR_DELTAF1,
			((fskt_deltaf >> 8) & 0x0F));
	ChipSetField(pParams->hDemod, FSTV0903_FSKR_DELTAF0, (fskt_deltaf &
				0xFF));

	ChipSetOneRegister(pParams->hDemod, RSTV0903_FSKRSTEPP, 0x02);
	ChipSetOneRegister(pParams->hDemod, RSTV0903_FSKRSTEPM, 0x4A);

	ChipSetOneRegister(pParams->hDemod, RSTV0903_FSKRDET1, 0);
	ChipSetOneRegister(pParams->hDemod, RSTV0903_FSKRDET0, 0x2F);

	ChipSetOneRegister(pParams->hDemod, RSTV0903_FSKRDTH1, 0);
	ChipSetOneRegister(pParams->hDemod, RSTV0903_FSKRDTH0, 0x55);

	ChipSetOneRegister(pParams->hDemod, RSTV0903_FSKRLOSS, 0x1F);

	if ((pParams->hDemod->Error) || (pParams->hTuner->Error))
		/*Check the error at the end of the function */
		error = FE_LLA_I2C_ERROR;

	return error;
}
EXPORT_SYMBOL(FE_STV0903_SetupFSK);

/***********************************************************
**FUNCTION  ::  FE_STV0903_EnableFSK
**ACTION    ::  Enable - Disable FSK modulator /Demodulator
**PARAMS IN ::  hChip    -> handle to the chip
**              mod_en   -> FSK modulator on/off
**              demod_en -> FSK demodulator on/off
**PARAMS OUT::  NONE
**RETURN    ::  Error if any
***********************************************************/
FE_STV0903_Error_t FE_STV0903_EnableFSK(FE_STV0903_Handle_t Handle,
					BOOL EnableModulation,
					BOOL EnableDemodulation)
{
	FE_STV0903_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0903_InternalParams_t *pParams;

	pParams = (FE_STV0903_InternalParams_t *) Handle;

	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;
	if ((pParams->hDemod->Error) || (pParams->hTuner->Error))
		return FE_LLA_I2C_ERROR;
	if (EnableDemodulation == TRUE)
		ChipSetField(pParams->hDemod, FSTV0903_FSKT_MOD_EN, 1);
	else
		ChipSetField(pParams->hDemod, FSTV0903_FSKT_MOD_EN, 0);

	if (EnableDemodulation == FALSE) {
		ChipSetField(pParams->hDemod, FSTV0903_FSK_PON, 0x01);
		ChipSetField(pParams->hDemod, FSTV0903_SEL_FSK, 0x00);
	} else {
		ChipSetField(pParams->hDemod, FSTV0903_FSK_PON, 0x00);
		ChipSetField(pParams->hDemod, FSTV0903_SEL_FSK, 0x01);
	}

	if ((pParams->hDemod->Error) || (pParams->hTuner->Error))
		/*Check the error at the end of the function */
		error = FE_LLA_I2C_ERROR;

	return error;
}
EXPORT_SYMBOL(FE_STV0903_EnableFSK);
#endif
/*****************************************************
--FUNCTION	::	FE_STV0903_Term
--ACTION	::	Terminate STV0903 chip connection
--PARAMS IN	::	Handle	==>	Front End Handle
--PARAMS OUT::	NONE
--RETURN	::	Error (if any)
--***************************************************/
FE_STV0903_Error_t FE_STV0903_Term(FE_STV0903_Handle_t Handle)
{
	FE_STV0903_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0903_InternalParams_t *pParams = NULL;

	pParams = (FE_STV0903_InternalParams_t *) Handle;

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

#ifdef STM_FE_DEFINED_FOR_FUTURE_USE
/*****************************************************
--FUNCTION	::	FE_STV0903_SetReg
--ACTION	::	write one register
--PARAMS IN	::	Handle	==>	Front End Handle
		::	Reg		==> register Index in the register Map
		::	Val	==> Val to be writen
--PARAMS OUT::	NONE.
--RETURN	::	Error (if any)
--***************************************************/
FE_STV0903_Error_t FE_STV0903_SetReg(FE_STV0903_Handle_t Handle, U16 Reg,
				     U32 Val)
{
	FE_STV0903_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0903_InternalParams_t *pParams =
	    (FE_STV0903_InternalParams_t *) Handle;

	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;
	ChipSetOneRegister(pParams->hDemod, Reg, Val);
	if (pParams->hDemod->Error)
		error = FE_LLA_I2C_ERROR;
	return error;
}

/*****************************************************
--FUNCTION	::	FE_STV0903_GetReg
--ACTION	::	write one register
--PARAMS IN	::	Handle	==>	Front End Handle
		::	Reg		==> register Index in the register Map
--PARAMS OUT::	Val		==> Read value.
--RETURN	::	Error (if any)
--***************************************************/
FE_STV0903_Error_t FE_STV0903_GetReg(FE_STV0903_Handle_t Handle, U16 Reg,
				     U32 *Val)
{
	FE_STV0903_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0903_InternalParams_t *pParams =
	    (FE_STV0903_InternalParams_t *) Handle;

	if ((pParams == NULL) || (Val == NULL))
		return FE_LLA_INVALID_HANDLE;
	*Val = ChipGetOneRegister(pParams->hDemod, Reg);
	if (pParams->hDemod->Error)
		error = FE_LLA_I2C_ERROR;
	return error;
}

/*****************************************************
--FUNCTION	::	FE_STV0903_SetField
--ACTION	::	write a value to a Field
--PARAMS IN	::	Handle	==>	Front End Handle
		::	Reg		==> Field Index in the register Map
		::	Val	==> Val to be writen
--PARAMS OUT::	NONE.
--RETURN	::	Error (if any)
--***************************************************/
FE_STV0903_Error_t FE_STV0903_SetField(FE_STV0903_Handle_t Handle, U32 Field,
				       S32 Val)
{
	FE_STV0903_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0903_InternalParams_t *pParams =
	    (FE_STV0903_InternalParams_t *) Handle;

	if (pParams == NULL)
		return FE_LLA_INVALID_HANDLE;
	ChipSetField(pParams->hDemod, Field, Val);
	if (pParams->hDemod->Error)
		error = FE_LLA_I2C_ERROR;
	return error;
}

/*****************************************************
--FUNCTION	::	FE_STV0903_GetField
--ACTION	::	Read A Field
--PARAMS IN	::	Handle	==>	Front End Handle
			::	Field	==> Field Index in the register Map
--PARAMS OUT::	Val		==> Read value.
--RETURN	::	Error (if any)
--***************************************************/
FE_STV0903_Error_t FE_STV0903_GetField(FE_STV0903_Handle_t Handle, U32 Field,
				       S32 *Val)
{
	FE_STV0903_Error_t error = FE_LLA_NO_ERROR;
	FE_STV0903_InternalParams_t *pParams =
	    (FE_STV0903_InternalParams_t *) Handle;

	if ((pParams == NULL) || (Val == NULL))
		return FE_LLA_INVALID_HANDLE;
	*Val = ChipGetField(pParams->hDemod, Field);
	if (pParams->hDemod->Error)
		error = FE_LLA_I2C_ERROR;
	return error;
}
#endif
