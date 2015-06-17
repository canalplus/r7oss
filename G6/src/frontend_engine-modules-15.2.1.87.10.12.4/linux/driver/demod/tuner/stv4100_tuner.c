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

Source file name :stv4100_tuner.c
Author :           LLA

tuner lla file

Date        Modification                                    Name
----        ------------                                    --------
01-Aug-11   Created
04-Jul-12   Updated
************************************************************************/

/* #ifndef ST_OSLINUX */
/* #include <string.h> */
/* #include <stdlib.h> */
/* #endif */
/* #include <globaldefs.h> */
/* #include "stv4100_tuner.h" */
#include <linux/kernel.h>
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
#include <fesat_commlla_str.h>
#include "fe_tc_tuner.h"
#include "stv4100_tuner.h"
#include <gen_macros.h>

/*STV4100 tuner definition*/
	/*      RSTV4100_ID     */
#define RSTV4100_ID						0x0000
#define FSTV4100_CHIP_IDENT				0x000000f0
#define FSTV4100_RELEASE				0x0000000f

	/*      RSTV4100_STATUS */
#define RSTV4100_STATUS					0x0001
#define FSTV4100_COMP					0x00010080
#define FSTV4100_PLLLOCK				0x00010040
#define FSTV4100_RFAGCHIGH				0x00010020
#define FSTV4100_RFAGCLOW				0x00010010
#define FSTV4100_RFAGCUPDATE			0x00010008
#define FSTV4100_CALSTART				0x00010004
#define FSTV4100_VCOCALSTART			0x00010002
#define FSTV4100_RFAGCCALSTART			0x00010001

	/*      CTRL    */
#define RSTV4100_PLL1					0x0002
#define FSTV4100_N_LSB					0x000200ff

	/*      RSTV4100_PLL2   */
#define RSTV4100_PLL2		0x0003
#define FSTV4100_DIV					0x000300c0
#define FSTV4100_TCAL				0x00030030
#define FSTV4100_ICP					0x0003000e
#define FSTV4100_N_MSB					0x00030001

	/*      RSTV4100_PLL3   */
#define RSTV4100_PLL3					0x0004
#define FSTV4100_F_L					0x000400ff

	/*      RSTV4100_PLL4   */
#define RSTV4100_PLL4					0x0005
#define FSTV4100_F_M					0x000500ff

	/*      RSTV4100_PLL5   */
#define RSTV4100_PLL5					0x0006
#define FSTV4100_PD						0x00060080
#define FSTV4100_PDPLL					0x00060040
#define FSTV4100_SDPDN					0x00060020
#define FSTV4100_VCOCALSTART2			0x00060010
#define FSTV4100_DIVD					0x0006000c
#define FSTV4100_F_H					0x00060003

	/*      RSTV4100_VCO    */
#define RSTV4100_VCO					0x0007
#define FSTV4100_VCOISAVE				0x00070080
#define FSTV4100_VCOCALOFF				0x00070040
#define FSTV4100_SEL					0x0007003f

	/*      RSTV4100_LNA1   */
#define RSTV4100_LNA1					0x0008
#define FSTV4100_LNASEL					0x00080080
#define FSTV4100_RFAGCMODE				0x00080070
#define FSTV4100_LNAGAIN				0x00080003

/*	RSTV4100_LNA2	*/
#define RSTV4100_LNA2					0x0009
#define FSTV4100_LCAL					0x000900e0
#define FSTV4100_AGCTUPD				0x00090010
#define FSTV4100_AGCTLOCK				0x00090008
#define FSTV4100_RFAGCREF				0x00090007

	/*      RSTV4100_CAL1   */
#define RSTV4100_CAL1					0x000A
#define FSTV4100_PLLCALOFF				0x000A0020
#define FSTV4100_CPLL					0x000A001f

	/*      RSTV4100_CAL2   */
#define RSTV4100_CAL2					0x000B
#define FSTV4100_BBATT					0x000B0080
#define FSTV4100_BANDSEL				0x000B0040
#define FSTV4100_CALOFF					0x000B0020
#define FSTV4100_CLPF						0x000B001f

	/*      RSTV4100_CAL3   */
#define RSTV4100_CAL3					0x000C
#define FSTV4100_DCLOOPEN				0x000C0080
#define FSTV4100_TESTCP					0x000C0040
#define FSTV4100_TESTUP					0x000C0020
#define FSTV4100_TESTDN					0x000C0010
#define FSTV4100_PMACALOFF				0x000C0008
#define FSTV4100_CPMA						0x000C0007

	/*      RSTV4100_CONFIG */
#define RSTV4100_CONFIG					0x000D
#define FSTV4100_DIV2					0x000D0080
#define FSTV4100_MDIV					0x000D0070
#define FSTV4100_RDIV					0x000D000f

	/*      RSTV4100_TST    */
#define RSTV4100_TST					0x000E
#define FSTV4100_VGA1TST				0x000E00c0
#define FSTV4100_VGA2TST				0x000E0030
#define FSTV4100_TST					0x000E000f

#define RF_LOOKUP_TABLE_SIZE_0  19
#define RF_LOOKUP_TABLE_SIZE_1  7
#define RF_LOOKUP_TABLE_SIZE_2  7
#define RF_LOOKUP_TABLE_SIZE_3  11
#define IF_LOOKUP_TABLE_SIZE_4  43
	/* RF Level  Lookup Table, depends on the board and tuner. */
	/*0dBm to -18dBm lnagain=0 */
U32 FE_STV4100_RF_LookUp0[RF_LOOKUP_TABLE_SIZE_0] = {
	3, 3, 4, 4, 5, 6, 7, 8, 10, 14, 51, 70, 81, 91, 99, 107, 117, 129, 143
};
	/* -19dBm to -25dBm lnagain=1 */
U32 FE_STV4100_RF_LookUp1[RF_LOOKUP_TABLE_SIZE_1] = {
	65, 76, 87, 97, 108, 119, 128
};
	/*-26dBm to -32dBm lnagain=2*/
U32 FE_STV4100_RF_LookUp2[RF_LOOKUP_TABLE_SIZE_2] = {
	59, 75, 86, 96, 104, 116, 129
};
	/*-33dBm to -43dBm lnagain=3*/
U32 FE_STV4100_RF_LookUp3[RF_LOOKUP_TABLE_SIZE_3] = {
	66, 79, 87, 97, 108, 119, 132, 144, 160, 182, 217
};
	/*-44dBm to -86dBm lnagain=3 IF AGC*/
U32 FE_STV4100_IF_LookUp4[IF_LOOKUP_TABLE_SIZE_4] = {
	990, 1020, 1070, 1160, 1220, 1275, 1350, 1410, 1475, 1530, 1600, 1690,
	1785, 1805, 1825, 1885, 1910, 1950, 2000, 2040, 2090, 2130, 2200, 2295,
	2335, 2400, 2450, 2550, 2600, 2660, 2730, 2805, 2840, 2905, 2955, 3060,
	3080, 3150, 3180, 3315, 3405, 4065, 4080
};

STCHIP_Register_t DefSTV4100_16MHz_Val[STV4100_NBREGS] = {
	{RSTV4100_ID, 0x01},
	{RSTV4100_STATUS, 0x40},
	{RSTV4100_PLL1, 0xed},
	{RSTV4100_PLL2, 0x7a},
	{RSTV4100_PLL3, 0x33},
	{RSTV4100_PLL4, 0x33},
	{RSTV4100_PLL5, 0x28},
	{RSTV4100_VCO, 0xa1},
	{RSTV4100_LNA1, 0x83},
	{RSTV4100_LNA2, 0xa7},
	{RSTV4100_CAL1, 0x13},
	{RSTV4100_CAL2, 0xc0},
	{RSTV4100_CAL3, 0x0f},
	{RSTV4100_CONFIG, 0x60},
	{RSTV4100_TST, 0x01}
};

STCHIP_Register_t DefSTV4100_25MHz_Val[STV4100_NBREGS] = {
	{RSTV4100_ID, 0x00},
	{RSTV4100_STATUS, 0x40},
	{RSTV4100_PLL1, 0x97},
	{RSTV4100_PLL2, 0x7a},
	{RSTV4100_PLL3, 0x4f},
	{RSTV4100_PLL4, 0xB8},
	{RSTV4100_PLL5, 0x22},
	{RSTV4100_VCO, 0x9F},
	{RSTV4100_LNA1, 0x83},
	{RSTV4100_LNA2, 0xA7},
	{RSTV4100_CAL1, 0x13},
	{RSTV4100_CAL2, 0x50},
	{RSTV4100_CAL3, 0x0F},
	{RSTV4100_CONFIG, 0x69},
	{RSTV4100_TST, 0x01}

};

STCHIP_Register_t DefSTV4100_27MHz_Val[STV4100_NBREGS] = {
	{RSTV4100_ID, 0x00},
	{RSTV4100_STATUS, 0x40},
	{RSTV4100_PLL1, 0x18},
	{RSTV4100_PLL2, 0x7b},
	{RSTV4100_PLL3, 0x4e},
	{RSTV4100_PLL4, 0x8d},
	{RSTV4100_PLL5, 0x2b},
	{RSTV4100_VCO, 0xa0},
	{RSTV4100_LNA1, 0x83},
	{RSTV4100_LNA2, 0xA7},
	{RSTV4100_CAL1, 0x14},
	{RSTV4100_CAL2, 0x50},
	{RSTV4100_CAL3, 0x0F},
	{RSTV4100_CONFIG, 0xeb},
	{RSTV4100_TST, 0x01}

};

STCHIP_Register_t DefSTV4100_30MHz_Val[STV4100_NBREGS] = {
	{RSTV4100_ID, 0x00},
	{RSTV4100_STATUS, 0x40},
	{RSTV4100_PLL1, 0xfc},
	{RSTV4100_PLL2, 0x7a},
	{RSTV4100_PLL3, 0x26 /*0x32 */ },
	{RSTV4100_PLL4, 0x31 /*33 */ },
	{RSTV4100_PLL5, 0x2b},
	{RSTV4100_VCO, 0xa0},
	{RSTV4100_LNA1, 0x83},
	{RSTV4100_LNA2, 0xA7},
	{RSTV4100_CAL1, 0x10},
	{RSTV4100_CAL2, 0x4d},
	{RSTV4100_CAL3, 0x0F},
	{RSTV4100_CONFIG, 0xfe},
	{RSTV4100_TST, 0x01}

};

TUNER_Error_t STV4100_TunerInit(void *pTunerInit_v,
				STCHIP_Handle_t *TunerHandle)
{
/*	TUNER_Params_t hTuner = NULL;*/

	TUNER_Params_t hTunerParams = NULL;
	STCHIP_Info_t ChipInfo;
	STCHIP_Handle_t hTuner = NULL;
	TUNER_Error_t error = TUNER_NO_ERR;
	TUNER_Params_t pTunerInit = (TUNER_Params_t) pTunerInit_v;
	stv4100_config *p_stv4100 = NULL;

	/*
	 **   REGISTER CONFIGURATION
	 **     ----------------------
	 */
#ifdef HOST_PC
	/* Allocation of the chip structure     */
	hTunerParams = calloc(1, sizeof(TUNER_InitParams_t));
	p_stv4100 = calloc(1, sizeof(stv4100_config));
#endif
#ifdef CHIP_STAPI
	/* fix PJ 11/2009 coverity */
	if (*TunerHandle) {
		hTunerParams = (TUNER_Params_t) ((*TunerHandle)->pData);
		if (hTunerParams) {
			p_stv4100 =
			    (stv4100_config *) (hTunerParams->pAddParams);
			memcpy(&ChipInfo, *TunerHandle, sizeof(STCHIP_Info_t));
		}
	}
#endif

	if (!hTunerParams || !p_stv4100)
		return TUNER_INVALID_HANDLE;

	/* Tuner model */
	hTunerParams->Model = pTunerInit->Model;
	/* Tuner name */
	strncpy((char *)ChipInfo.Name, (char *)pTunerInit->TunerName,
			MAXNAMESIZE - 1);
	/* Repeater host */
	ChipInfo.RepeaterHost = pTunerInit->RepeaterHost;
	/* Tuner has no embedded repeater */
	ChipInfo.Repeater = TRUE;
	ChipInfo.RepeaterFn = pTunerInit->RepeaterFn;	/* see above */
	/* Init tuner I2C address */
	ChipInfo.I2cAddr = pTunerInit->TunerI2cAddress;
	/* Store tunerparams pointer into Chip structure */
	ChipInfo.pData = hTunerParams;

	hTunerParams->TransmitStandard = pTunerInit->TransmitStandard;
	hTunerParams->DemodModel = pTunerInit->DemodModel;
	hTunerParams->StepSize = 12;	/* 12hz StepSize */
	hTunerParams->IF = 0;	/* NO intermediate frequency */
	if (hTunerParams->TransmitStandard == FE_ISDB_T)
		hTunerParams->BandWidth = 6;
	else
		hTunerParams->BandWidth = 8;

	hTunerParams->BandSelect = Band_UHF;
	p_stv4100->BandSelect = Band_UHF;
	/* No hardware IQ invertion */
	p_stv4100->IQ_Wiring = TUNER_IQ_NORMAL;
	/* Tuner invert IF or IQ spectrum */
	hTunerParams->SpectrInvert = TUNER_IQ_NORMAL;
	/*defined inside 362_dll.c */
	p_stv4100->Fxtal_Hz = pTunerInit->Fxtal_Hz;
	/*manage both struct */
	hTunerParams->Fxtal_Hz = pTunerInit->Fxtal_Hz;
	p_stv4100->CalValue = 0;
	p_stv4100->CLPF_CalValue = 0;
	hTunerParams->pAddParams = (void *)p_stv4100;

	/* fill elements of external chip data structure */
	ChipInfo.NbRegs = STV4100_NBREGS;
	ChipInfo.NbFields = STV4100_NBFIELDS;
	ChipInfo.ChipMode = STCHIP_MODE_SUBADR_8;
	ChipInfo.WrStart = RSTV4100_ID;
	ChipInfo.WrSize = STV4100_NBREGS;
	ChipInfo.RdStart = RSTV4100_ID;
	ChipInfo.RdSize = STV4100_NBREGS;
#ifdef HOST_PC
	hTuner = ChipOpen(&ChipInfo);
	(*TunerHandle) = hTuner;
#endif
#ifdef CHIP_STAPI
	hTuner = *TunerHandle;	/*obtain hTuner : change AG */
	/* Copy the ChipInfo to hTuner: Change AG */
	memcpy(hTuner, &ChipInfo, sizeof(STCHIP_Info_t));
#endif
	if ((*TunerHandle) == NULL)
		error = TUNER_INVALID_HANDLE;

	if (hTuner) {
		if (p_stv4100->Fxtal_Hz == 27000000)
			ChipUpdateDefaultValues(hTuner, DefSTV4100_27MHz_Val);
		else if (p_stv4100->Fxtal_Hz == 30000000)
			ChipUpdateDefaultValues(hTuner, DefSTV4100_30MHz_Val);
		else if (p_stv4100->Fxtal_Hz == 16000000)
			ChipUpdateDefaultValues(hTuner, DefSTV4100_16MHz_Val);
		else
			ChipUpdateDefaultValues(hTuner, DefSTV4100_25MHz_Val);

		if (p_stv4100->Fxtal_Hz == 25000000) {
			/*Xtal div */
			ChipSetFieldImage(hTuner, FSTV4100_RDIV, 0x9);
			/* I2c div */
			ChipSetFieldImage(hTuner, FSTV4100_MDIV, 0x6);
			ChipSetFieldImage(hTuner, FSTV4100_DIV2, 0);

		} else if (p_stv4100->Fxtal_Hz == 27000000) {
			ChipSetFieldImage(hTuner, FSTV4100_RDIV, 0xb);
			ChipSetFieldImage(hTuner, FSTV4100_MDIV, 0x6);
			ChipSetFieldImage(hTuner, FSTV4100_DIV2, 1);

		} else if (p_stv4100->Fxtal_Hz == 30000000) {
			ChipSetFieldImage(hTuner, FSTV4100_RDIV, 0xe);
			ChipSetFieldImage(hTuner, FSTV4100_MDIV, 0x7);
			ChipSetFieldImage(hTuner, FSTV4100_DIV2, 1);

		} else if (p_stv4100->Fxtal_Hz == 16000000) {
			ChipSetFieldImage(hTuner, FSTV4100_RDIV, 0x0);
			ChipSetFieldImage(hTuner, FSTV4100_MDIV, 0x6);
			ChipSetFieldImage(hTuner, FSTV4100_DIV2, 0);

		} else {
			error = TUNER_INVALID_HANDLE;
		}
	}
#if defined(CHIP_STAPI) || defined(NO_GUI)
	error = STV4100_TunerWrite(hTuner);
#endif

	return error;
}

TUNER_Error_t STV4100_TunerSetFrequency(STCHIP_Handle_t hTuner, U32 Frequency)
{
	TUNER_Error_t error = TUNER_NO_ERR;
	TUNER_Params_t hTunerParams = NULL;
	stv4100_config *p_stv4100 = NULL;
	U8 cpma = 0;
	U32 N = 0, F = 0;
	U32 result = 0, InternalClock = 0;
	int i = 0;

	/* fix PJ 11/2009 coverity */
	if (hTuner) {
		hTunerParams = (TUNER_Params_t) hTuner->pData;
		if (hTunerParams)
			p_stv4100 =
			    (stv4100_config *) (hTunerParams->pAddParams);
	}

	if (!p_stv4100)
		return TUNER_INVALID_HANDLE;

	if (Frequency <= 325000) {
		result = (U32) (Frequency * 16);
		/*ChipSetFieldImage(hTuner,FSTV4100_BANDSEL,0); */
		ChipSetFieldImage(hTuner, FSTV4100_LNASEL, 0);
		ChipSetFieldImage(hTuner, FSTV4100_DIV, 2);
		ChipSetFieldImage(hTuner, FSTV4100_DIVD, 1);
	} else if (Frequency <= 650000) {
		result = (U32) (Frequency * 8);
		/*ChipSetFieldImage(hTuner,FSTV4100_BANDSEL,1) ; */
		ChipSetFieldImage(hTuner, FSTV4100_LNASEL, 1);
		ChipSetFieldImage(hTuner, FSTV4100_DIV, 1);
		ChipSetFieldImage(hTuner, FSTV4100_DIVD, 2);
	} else if (Frequency <= 875000) {
		result = (U32) (Frequency * 4);
		/*ChipSetFieldImage(hTuner,FSTV4100_BANDSEL,1) ; */
		ChipSetFieldImage(hTuner, FSTV4100_LNASEL, 1);
		ChipSetFieldImage(hTuner, FSTV4100_DIV, 0);
		ChipSetFieldImage(hTuner, FSTV4100_DIVD, 2);
	}

			/****freq calculation *****/

	/* fill  CP current */
	if ((int)(result / 1000) < 2700)
		ChipSetFieldImage(hTuner, FSTV4100_ICP, 0);
	else if ((int)(result / 1000) < 2950)
		ChipSetFieldImage(hTuner, FSTV4100_ICP, 1);
	else if ((int)(result / 1000) < 3300)
		ChipSetFieldImage(hTuner, FSTV4100_ICP, 2);
	else if ((int)(result / 1000) < 3700)
		ChipSetFieldImage(hTuner, FSTV4100_ICP, 3);
	else if ((int)(result / 1000) < 4200)
		ChipSetFieldImage(hTuner, FSTV4100_ICP, 5);
	else if ((int)(result / 1000) < 4800)
		ChipSetFieldImage(hTuner, FSTV4100_ICP, 6);
	else		/*if ((int)(result/1000)<5200) */
		ChipSetFieldImage(hTuner, FSTV4100_ICP, 7);

	if (p_stv4100->Fxtal_Hz == 25000000) {
		/*Xtal div */
		ChipSetFieldImage(hTuner, FSTV4100_RDIV, 0x9);
		/* I2c div */
		ChipSetFieldImage(hTuner, FSTV4100_MDIV, 0x6);
		ChipSetFieldImage(hTuner, FSTV4100_DIV2, 0);

	} else if (p_stv4100->Fxtal_Hz == 27000000) {
		ChipSetFieldImage(hTuner, FSTV4100_RDIV, 0xb);
		ChipSetFieldImage(hTuner, FSTV4100_MDIV, 0x6);
		ChipSetFieldImage(hTuner, FSTV4100_DIV2, 1);

	} else if (p_stv4100->Fxtal_Hz == 30000000) {
		ChipSetFieldImage(hTuner, FSTV4100_RDIV, 0xe);
		ChipSetFieldImage(hTuner, FSTV4100_MDIV, 0x7);
		ChipSetFieldImage(hTuner, FSTV4100_DIV2, 1);

	} else if (p_stv4100->Fxtal_Hz == 16000000) {
		ChipSetFieldImage(hTuner, FSTV4100_RDIV, 0x0);
		ChipSetFieldImage(hTuner, FSTV4100_MDIV, 0x6);
		ChipSetFieldImage(hTuner, FSTV4100_DIV2, 0);

	} else {
		/*should never pass here */
		return TUNER_TYPE_ERR;
	}

	/* set internal clock */
	if (p_stv4100->Fxtal_Hz > 25000000)
		InternalClock = p_stv4100->Fxtal_Hz / 2000;
	else
		InternalClock = p_stv4100->Fxtal_Hz / 1000;
	/*write N value */
	N = (int)(result / InternalClock);
	ChipSetFieldImage(hTuner, FSTV4100_N_LSB, N & 0xff);
	ChipSetFieldImage(hTuner, FSTV4100_N_MSB, N >> 8);

	if (result <= 4294967) {
		result = (U32) ((1000 * result) / InternalClock);
		F = (result - 1000 * N) * (PowOf2(18) - 1) / 1000;
	} else {
		result = (U32) ((100 * result) / InternalClock);
		F = (result - 100 * N) * (PowOf2(18) - 1) / 100;
	}

	/*write F value */

	ChipSetFieldImage(hTuner, FSTV4100_F_L, F & 0xff);
	ChipSetFieldImage(hTuner, FSTV4100_F_M, (F >> 8) & 0xff);
	ChipSetFieldImage(hTuner, FSTV4100_F_H, (F >> 16) & 0xff);

	/* don't use sigma delta if not needed */
	if (F == 0)
		ChipSetFieldImage(hTuner, FSTV4100_SDPDN, 0);
	else
		ChipSetFieldImage(hTuner, FSTV4100_SDPDN, 1);

	/* calibrate vco for each frequency change */
	ChipSetFieldImage(hTuner, FSTV4100_VCOCALSTART2, 1);

	/*cpma compensation */
	if (p_stv4100->CalValue == 0)
		cpma = 1;
	else if (p_stv4100->CalValue < 4) {
		if (hTunerParams->BandWidth == 8)
			cpma = 2 * p_stv4100->CalValue;
		else
			cpma = (2 * p_stv4100->CalValue) + 1;
	} else
		cpma = 7;

	ChipSetFieldImage(hTuner, FSTV4100_CPMA, cpma);
	ChipSetFieldImage(hTuner, FSTV4100_BBATT, 1);

	ChipSetFieldImage(hTuner, FSTV4100_PMACALOFF, 1);
	/*set RFAGC to def value */
	/*was 4 */
	ChipSetFieldImage(hTuner, FSTV4100_RFAGCREF, 7);

	     /******write 12 rw registers********/
	error = ChipSetRegisters(hTuner, RSTV4100_PLL1, STV4100_RW_REGS);

	if (error != TUNER_NO_ERR)
		return error;
	/*check tuner lock but does not affect error */
	i = 0;
	while (!(ChipGetField(hTuner, FSTV4100_PLLLOCK)) && (i < 6)) {
		/*WAIT_N_MS(1);*/
		ChipWaitOrAbort(hTuner, 1);
		i++;
	}
	/*if (i==6) printf("STv4100 PLL not locked/n"); */

	return error;		/*TunerSetFrequency */
}

U32 STV4100_TunerGetFrequency(STCHIP_Handle_t hTuner)
{
	TUNER_Params_t hTunerParams = NULL;
	stv4100_config *p_stv4100 = NULL;

	U32 frequency = 0;
	U32 F = 0, Fmax = 0;
	U32 N = 0, InternalClock = 0;

	/* fix PJ 11/2009 coverity */
	if (hTuner) {
		hTunerParams = (TUNER_Params_t) hTuner->pData;
		if (hTunerParams) {
			p_stv4100 =
			    (stv4100_config *) (hTunerParams->pAddParams);
		}
	}
	if (p_stv4100) {

		F = ChipGetFieldImage(hTuner, FSTV4100_F_L)
		    + ChipGetFieldImage(hTuner, FSTV4100_F_M) * 256
		    + ChipGetFieldImage(hTuner, FSTV4100_F_H) * 65536;

		Fmax = PowOf2(18) - 1;
		N = ChipGetFieldImage(hTuner,
				      FSTV4100_N_LSB) +
		    ChipGetFieldImage(hTuner, FSTV4100_N_MSB) * 256;

		if (p_stv4100->Fxtal_Hz > 25000000)
			InternalClock = p_stv4100->Fxtal_Hz / 2000;
		else
			InternalClock = p_stv4100->Fxtal_Hz / 1000;

		frequency =
		    ((InternalClock / 4) /
		     PowOf2(ChipGetFieldImage(hTuner, FSTV4100_DIV)));
		frequency = (frequency * N) + ((frequency * F) / Fmax);

	}

	return frequency;	/*TunerGetFrequency */
}

BOOL STV4100_TunerGetStatus(STCHIP_Handle_t hTuner)
{

	TUNER_Params_t hTunerParams = NULL;
	BOOL locked = FALSE;

	/* fix PJ 11/2009 coverity */
	if (hTuner)
		hTunerParams = (TUNER_Params_t) hTuner->pData;

	if (hTunerParams) {
		STV4100_TunerRead(hTuner);
		locked = ChipGetFieldImage(hTuner, FSTV4100_PLLLOCK);
	}

	return locked;
}

TUNER_Error_t STV4100_TunerWrite(STCHIP_Handle_t hTuner)
{
	TUNER_Error_t error = TUNER_NO_ERR;
	TUNER_Params_t hTunerParams = NULL;

	/* fix PJ/TA 12/2009 coverity: both STCHIP_Error_t & TUNER_Error_t must
	 * be in line */

	if (!hTuner)
		return TUNER_INVALID_HANDLE;
	if (hTuner->Error)
		return hTuner->Error;
	hTunerParams = (TUNER_Params_t) hTuner->pData;
	if (!hTunerParams)
		return TUNER_INVALID_HANDLE;

	error = (TUNER_Error_t)ChipSetRegisters(hTuner, RSTV4100_STATUS,
			    STV4100_NBREGS - 1);

	return error;
}

TUNER_Error_t STV4100_TunerRead(STCHIP_Handle_t hTuner)
{
	TUNER_Error_t error = TUNER_NO_ERR;

	TUNER_Params_t hTunerParams = NULL;

	/* fix PJ/TA 12/2009 coverity: both STCHIP_Error_t & TUNER_Error_t must
	 * be in line */

	if (!hTuner)
		return TUNER_INVALID_HANDLE;
	if (hTuner->Error)
		return hTuner->Error;
	hTunerParams = (TUNER_Params_t)hTuner->pData;
	if (!hTunerParams)
		return TUNER_INVALID_HANDLE;
	error = (TUNER_Error_t)ChipGetRegisters(hTuner, RSTV4100_ID,
			STV4100_NBREGS);
	return error;
}

TUNER_Model_t STV4100_TunerGetModelName(STCHIP_Handle_t hTuner)
{
	TUNER_Params_t hTunerParams = NULL;

	if (hTuner && hTuner->pData) {
		hTunerParams = (TUNER_Params_t) hTuner->pData;
		return hTunerParams->Model;
	} else
		return 0;

}

void STV4100_TunerSetIQ_Wiring(STCHIP_Handle_t hChip, TUNER_IQ_t IQ_Wiring)
{
	if (hChip && hChip->pData)
		((TUNER_Params_t) (hChip->pData))->IQ_Wiring = IQ_Wiring;
}

TUNER_IQ_t STV4100_TunerGetIQ_Wiring(STCHIP_Handle_t hChip)
{
	TUNER_IQ_t wiring = TUNER_IQ_NORMAL;

	if (hChip && hChip->pData)
		wiring = ((TUNER_Params_t) (hChip->pData))->IQ_Wiring;

	return wiring;
}

U32 STV4100_TunerGetReferenceFreq(STCHIP_Handle_t hChip)
{
	U32 reference = 0;

	if (hChip && hChip->pData)
		reference = ((TUNER_Params_t) (hChip->pData))->Fxtal_Hz;

	return reference;
}

void STV4100_TunerSetReferenceFreq(STCHIP_Handle_t hTuner, U32 reference)
{
	TUNER_Params_t hTunerParams = NULL;

	if (hTuner && hTuner->pData) {
		hTunerParams = (TUNER_Params_t) hTuner->pData;
		hTunerParams->Fxtal_Hz = reference;
	}
	if (hTunerParams && hTunerParams->pAddParams)
		((stv4100_config *) (hTunerParams->pAddParams))->Fxtal_Hz =
		    reference;
}

void STV4100_TunerSetIF_Freq(STCHIP_Handle_t hChip, U32 IF)
{
	if (hChip && hChip->pData)
		((TUNER_Params_t) (hChip->pData))->IF = IF;
}

U32 STV4100_TunerGetLNAConfig(STCHIP_Handle_t hChip)
{
	U32 LNA_Config = 0;

	if (hChip && hChip->pData)
		LNA_Config = ((TUNER_Params_t) (hChip->pData))->LNAConfig;

	return LNA_Config;
}

void STV4100_TunerSetLNAConfig(STCHIP_Handle_t hChip, U32 LNA_Config)
{
	if (hChip && hChip->pData)
		((TUNER_Params_t) (hChip->pData))->LNAConfig = LNA_Config;
}

U32 STV4100_TunerGetUHFSens(STCHIP_Handle_t hChip)
{
	U32 UHF_Sens = 0;

	if (hChip && hChip->pData)
		UHF_Sens = ((TUNER_Params_t) (hChip->pData))->UHFSens;

	return UHF_Sens;
}

void STV4100_TunerSetUHFSens(STCHIP_Handle_t hChip, U32 UHF_Sens)
{
	if (hChip && hChip->pData)
		((TUNER_Params_t) (hChip->pData))->UHFSens = UHF_Sens;
}

TUNER_Error_t STV4100_TunerStartAndCalibrate(STCHIP_Handle_t hTuner)
{
	TUNER_Error_t error = TUNER_NO_ERR;
	TUNER_Params_t hTunerParams = NULL;
	stv4100_config *p_stv4100 = NULL;
	if (hTuner) {
		hTunerParams = (TUNER_Params_t)hTuner->pData;
		if (hTunerParams)
			p_stv4100 =
			    (stv4100_config *)(hTunerParams->pAddParams);
	}

	if (p_stv4100) {

		/*4100 power up done once (or done by config file) */
		if (ChipGetField(hTuner, FSTV4100_PD)) {

			ChipSetFieldImage(hTuner, FSTV4100_TCAL, 2);
			error = ChipSetRegisters(hTuner, RSTV4100_PLL2, 1);

			if (error != TUNER_NO_ERR)
				return error;

			ChipSetFieldImage(hTuner, FSTV4100_SDPDN, 1);
			ChipSetFieldImage(hTuner, FSTV4100_PD, 0);
			ChipSetFieldImage(hTuner, FSTV4100_PDPLL, 0);
			error = ChipSetRegisters(hTuner, RSTV4100_PLL5, 1);

			if (error != TUNER_NO_ERR)
				return error;

			WAIT_N_MS(25);	/*startup time */

		}
		/*cal BB filter each time */
		ChipSetField(hTuner, FSTV4100_CALSTART, 1);
		error = ChipSetRegisters(hTuner, RSTV4100_STATUS, 1);

		if (error != TUNER_NO_ERR)
			return error;

		ChipSetField(hTuner, FSTV4100_PMACALOFF, 0);
		error = ChipSetRegisters(hTuner, RSTV4100_CAL3, 1);

		if (error != TUNER_NO_ERR)
			return error;
		/*store cpma value each frequency change */
		p_stv4100->CalValue = ChipGetField(hTuner, FSTV4100_CPMA);

		ChipSetField(hTuner, FSTV4100_CALOFF, 0);
		error = ChipSetRegisters(hTuner, RSTV4100_CAL2, 1);

		if (error != TUNER_NO_ERR)
			return error;

		/*store cpma value each frequency change */
		p_stv4100->CLPF_CalValue = ChipGetField(hTuner, FSTV4100_CLPF);
	} else {
		error = TUNER_INVALID_HANDLE;
	}
	return error;
}

TUNER_Error_t STV4100_TunerSetLna(STCHIP_Handle_t hTuner)
{
	TUNER_Error_t error = TUNER_NO_ERR;
	TUNER_Params_t hTunerParams = NULL;
	stv4100_config *p_stv4100 = NULL;

	if (hTuner) {
		hTunerParams = (TUNER_Params_t) hTuner->pData;
		if (hTunerParams)
			p_stv4100 =
			    (stv4100_config *)(hTunerParams->pAddParams);
	}

	if (!p_stv4100)
		return TUNER_INVALID_HANDLE;

	if ((hTunerParams->BandWidth == 7)
	    || (hTunerParams->BandWidth == 8)) {
		if (p_stv4100->CLPF_CalValue > 31)
			ChipSetFieldImage(hTuner, FSTV4100_CLPF, 31);
		else if (p_stv4100->CLPF_CalValue > 17) {
			if ((p_stv4100->CLPF_CalValue + 0x3) > 31)
				ChipSetFieldImage(hTuner, FSTV4100_CLPF, 31);
			else
				ChipSetFieldImage(hTuner, FSTV4100_CLPF,
					  p_stv4100->CLPF_CalValue + 0x3);
		} else if (p_stv4100->CLPF_CalValue >= 6)
			ChipSetFieldImage(hTuner, FSTV4100_CLPF,
					  p_stv4100->CLPF_CalValue + 0x2);
		else
			ChipSetFieldImage(hTuner, FSTV4100_CLPF,
					  p_stv4100->CLPF_CalValue + 0x1);
	} else {	/*bw 6M */

		if (p_stv4100->CLPF_CalValue > 31)
			ChipSetFieldImage(hTuner, FSTV4100_CLPF, 31);
		else if (p_stv4100->CLPF_CalValue > 17) {
			if ((p_stv4100->CLPF_CalValue + 0x8) > 31)
				ChipSetFieldImage(hTuner, FSTV4100_CLPF, 31);
			else
				ChipSetFieldImage(hTuner, FSTV4100_CLPF,
					  p_stv4100->CLPF_CalValue + 0x8);
		} else if (p_stv4100->CLPF_CalValue >= 6)
			ChipSetFieldImage(hTuner, FSTV4100_CLPF,
					  p_stv4100->CLPF_CalValue + 0x7);
		else
			ChipSetFieldImage(hTuner, FSTV4100_CLPF,
					  p_stv4100->CLPF_CalValue + 0x6);
	}

	/*release cal */
	ChipSetFieldImage(hTuner, FSTV4100_CALOFF, 1);
	error = ChipSetRegisters(hTuner, RSTV4100_CAL2, 1);

	if (error != TUNER_NO_ERR)
		return error;

	ChipSetFieldImage(hTuner, FSTV4100_RFAGCREF, 0x3);
	error = ChipSetRegisters(hTuner, RSTV4100_LNA2, 1);

	if (error != TUNER_NO_ERR)
		return error;

	return error;
}

TUNER_Error_t STV4100_TunerAdjustRfPower(STCHIP_Handle_t hTuner, int AGC2VAL2)
{
	TUNER_Error_t error = TUNER_NO_ERR;
	TUNER_Params_t hTunerParams = NULL;
	stv4100_config *p_stv4100 = NULL;
	if (hTuner) {
		hTunerParams = (TUNER_Params_t) hTuner->pData;
		if (hTunerParams)
			p_stv4100 =
			    (stv4100_config *)(hTunerParams->pAddParams);
	}

	if (!p_stv4100)
		return TUNER_INVALID_HANDLE;

	if ((hTunerParams->BandWidth == 7)
	    || (hTunerParams->BandWidth == 8)) {
		if (p_stv4100->CLPF_CalValue > 31)
			ChipSetFieldImage(hTuner, FSTV4100_CLPF, 31);
		else if (p_stv4100->CLPF_CalValue > 17) {
			if ((p_stv4100->CLPF_CalValue + 0x3) > 31)
				ChipSetFieldImage(hTuner, FSTV4100_CLPF, 31);
			else
				ChipSetFieldImage(hTuner, FSTV4100_CLPF,
					  p_stv4100->CLPF_CalValue + 0x3); }
		else if (p_stv4100->CLPF_CalValue >= 6)
			ChipSetFieldImage(hTuner, FSTV4100_CLPF,
					  p_stv4100->CLPF_CalValue + 0x2);
		else
			ChipSetFieldImage(hTuner, FSTV4100_CLPF,
					  p_stv4100->CLPF_CalValue + 0x1);
	} else {	/*bw 6M */

		if (p_stv4100->CLPF_CalValue > 31)
			ChipSetFieldImage(hTuner, FSTV4100_CLPF, 31);
		else if (p_stv4100->CLPF_CalValue > 17) {
			if ((p_stv4100->CLPF_CalValue + 0x8) > 31)
				ChipSetFieldImage(hTuner, FSTV4100_CLPF, 31);
			else
				ChipSetFieldImage(hTuner, FSTV4100_CLPF,
					  p_stv4100->CLPF_CalValue + 0x8);
		} else if (p_stv4100->CLPF_CalValue >= 6)
			ChipSetFieldImage(hTuner, FSTV4100_CLPF,
					  p_stv4100->CLPF_CalValue + 0x7);
		else
			ChipSetFieldImage(hTuner, FSTV4100_CLPF,
					  p_stv4100->CLPF_CalValue + 0x6);
	}
	/*release cal */
	ChipSetFieldImage(hTuner, FSTV4100_CALOFF, 1);
	error = ChipSetRegisters(hTuner, RSTV4100_CAL2, 1);

	if (error != TUNER_NO_ERR)
		return error;

	if ((ChipGetField(hTuner, FSTV4100_RFAGCLOW))
	    && (ChipGetField(hTuner, FSTV4100_LNAGAIN) == 0x3)
	    && (AGC2VAL2 > 0xc00)) {
		ChipSetFieldImage(hTuner, FSTV4100_BBATT, 0);
		error = ChipSetRegisters(hTuner, RSTV4100_CAL2, 1);

		if (error != TUNER_NO_ERR)
			return error;
	}

	/*ChipSetFieldImage(hTuner,FSTV4100_RFAGCREF,0x7); already done */
	return error;
}

/*****************************************************
--FUNCTION	::	STV4100_TunerEstimateRfPower
--ACTION	::	Estimate the RF input level regarding the RF and IF agc
			values
--PARAMS IN	::	Handle	==>	Front End Handle
			RfAgc ==> 8bits readen from F367_RF_AGC1_LEVEL_HI
			(@0xf0d4)
			IfAgc ==> 12bits readen from F367_AGC2_VAL_LO (@0xf01b)
			and F367_AGC2_VAL_HI  (@0xf01c)
--PARAMS OUT::	NONE
--RETURN	::	estimated value extracted from LUT (S32)
--***************************************************/
S32 STV4100_TunerEstimateRfPower(STCHIP_Handle_t hTuner, U16 RfAgc, U16 IfAgc)
{
	TUNER_Params_t hTunerParams = NULL;
	int Stv4100LnaGain = -100, i = 0;
	S32 RfEstimatePower = 0;
	if (hTuner)
		hTunerParams = (TUNER_Params_t) hTuner->pData;

	if (!hTunerParams)
		return RfEstimatePower;
	Stv4100LnaGain = ChipGetField(hTuner, FSTV4100_LNAGAIN);

	if ((Stv4100LnaGain == 0x0) && (RfAgc < 255)) {
		for (i = 0; i < RF_LOOKUP_TABLE_SIZE_0; i++) {
			if (RfAgc <= FE_STV4100_RF_LookUp0[i]) {
				/*0dBm to -18dBm */
				RfEstimatePower = -1 * i;
				break;
			}
		}
		if (i == RF_LOOKUP_TABLE_SIZE_0)
			RfEstimatePower = -19;
	} else if ((Stv4100LnaGain == 0x1) && (RfAgc < 255)) {
		for (i = 0; i < RF_LOOKUP_TABLE_SIZE_1; i++) {
			if (RfAgc <= FE_STV4100_RF_LookUp1[i]) {
				/* -19dBm to -25dBm */
				RfEstimatePower = -19 + (-1) * i;
				break;
			}
		}
		if (i == RF_LOOKUP_TABLE_SIZE_1)
			RfEstimatePower = -26;
	} else if ((Stv4100LnaGain == 0x2) && (RfAgc < 255)) {
		for (i = 0; i < RF_LOOKUP_TABLE_SIZE_2; i++) {
			if (RfAgc <= FE_STV4100_RF_LookUp2[i]) {
				/*-26dBm to -32dBm*/
				RfEstimatePower = -26 + (-1) * i;
				break;
			}
		}
		if (i == RF_LOOKUP_TABLE_SIZE_2)
			RfEstimatePower = -33;
	} else if ((Stv4100LnaGain == 0x3) && (RfAgc < 255)) {
		for (i = 0; i < RF_LOOKUP_TABLE_SIZE_3; i++) {
			if (RfAgc <= FE_STV4100_RF_LookUp3[i]) {
				/*-33dBm to -43dBm*/
				RfEstimatePower = -33 + (-1) * i;
				break;
			}
		}
		if (i == RF_LOOKUP_TABLE_SIZE_3)
			RfEstimatePower = -44;
	} else if ((Stv4100LnaGain == 0x3) && (RfAgc == 255)) {
		for (i = 0; i < IF_LOOKUP_TABLE_SIZE_4; i++) {
			if (IfAgc <= FE_STV4100_IF_LookUp4[i]) {
				/*-44dBm to -86dBm*/
				RfEstimatePower = -44 + (-1) * i;
				break;
			}
		}
		if (i == IF_LOOKUP_TABLE_SIZE_4)
			RfEstimatePower = -88;
	} else
		/*should never pass here */
		RfEstimatePower = -90;

	return RfEstimatePower;
}

TUNER_Error_t STV4100_TunerSetStandby(STCHIP_Handle_t hTuner, U8 StandbyOn)
{
	TUNER_Error_t error = TUNER_NO_ERR;
	TUNER_Params_t hTunerParams = NULL;

	/* fix PJ 11/2009 coverity */
	if (hTuner)
		hTunerParams = (TUNER_Params_t) hTuner->pData;

	if (hTunerParams) {
		if (StandbyOn) {	/*Power down ON */
			ChipSetFieldImage(hTuner, FSTV4100_PD, 1);
			ChipSetFieldImage(hTuner, FSTV4100_PDPLL, 1);
			ChipSetFieldImage(hTuner, FSTV4100_SDPDN, 0);

		} else {	/*Power down OFF */

			ChipSetFieldImage(hTuner, FSTV4100_PD, 0);
			ChipSetFieldImage(hTuner, FSTV4100_PDPLL, 0);
			ChipSetFieldImage(hTuner, FSTV4100_SDPDN, 1);

		}
		error = STV4100_TunerWrite(hTuner);

	} else
		error = TUNER_INVALID_HANDLE;

	return error;
}

TUNER_Error_t STV4100_TunerSetBandWidth(STCHIP_Handle_t hTuner, U32 Band_Width)
{
	TUNER_Error_t error = TUNER_NO_ERR;
	TUNER_Params_t hTunerParams = NULL;

	if (hTuner)
		hTunerParams = (TUNER_Params_t) hTuner->pData;

	if (hTunerParams) {
		if (Band_Width == 6 || Band_Width == 7)
			error = ChipSetFieldImage(hTuner, FSTV4100_BANDSEL, 0);
		else if (Band_Width == 8)
			error = ChipSetFieldImage(hTuner, FSTV4100_BANDSEL, 1);
		else
			return TUNER_INVALID_HANDLE;

		hTunerParams->BandWidth = Band_Width;

	} else
		error = TUNER_INVALID_HANDLE;

	return error;
}

U32 STV4100_TunerGetBandWidth(STCHIP_Handle_t hTuner)
{
	U32 TunerBW = 0.0;
	TUNER_Params_t hTunerParams = NULL;

	if (hTuner)
		hTunerParams = (TUNER_Params_t) hTuner->pData;

	if (hTunerParams) {
		TunerBW = hTunerParams->BandWidth;
		if ((TunerBW == 6) || (TunerBW == 7) || (TunerBW == 8)) {
			return TunerBW;
		} else {
			hTuner->Error = CHIPERR_INVALID_FIELD_SIZE;
			return 0.0;
		}
	}
	return 0.0;
}

TUNER_Error_t STV4100_SwitchTunerToDVBT(STCHIP_Handle_t hTuner)
{
	TUNER_Error_t error = TUNER_NO_ERR;
	TUNER_Params_t hTunerParams = NULL;
	stv4100_config *p_stv4100 = NULL;

	/* fix PJ 11/2009 coverity */
	if (!hTuner)
		return CHIPERR_INVALID_HANDLE;
	hTunerParams = (TUNER_Params_t) hTuner->pData;
	if (!hTunerParams)
		return CHIPERR_INVALID_HANDLE;

	p_stv4100 = (stv4100_config *) (hTunerParams->pAddParams);
	if (!p_stv4100)
		return CHIPERR_INVALID_HANDLE;
	if (p_stv4100->Fxtal_Hz == 27000000)
		ChipUpdateDefaultValues(hTuner, DefSTV4100_27MHz_Val);
	else if (p_stv4100->Fxtal_Hz == 30000000)
		ChipUpdateDefaultValues(hTuner, DefSTV4100_30MHz_Val);
	else if (p_stv4100->Fxtal_Hz == 16000000)
		ChipUpdateDefaultValues(hTuner, DefSTV4100_16MHz_Val);
	else
		ChipUpdateDefaultValues(hTuner, DefSTV4100_25MHz_Val);

#if defined(CHIP_STAPI) || defined(NO_GUI)
		error = STV4100_TunerWrite(hTuner);
#endif

	return error;
}

TUNER_Error_t STV4100_SwitchTunerToDVBC(STCHIP_Handle_t hTuner)
{
	TUNER_Error_t error = TUNER_NO_ERR;
	TUNER_Params_t hTunerParams = NULL;
	stv4100_config *p_stv4100 = NULL;

	/* fix PJ 11/2009 coverity */
	if (!hTuner)
		return CHIPERR_INVALID_HANDLE;
	hTunerParams = (TUNER_Params_t) hTuner->pData;
	if (!hTunerParams)
		return CHIPERR_INVALID_HANDLE;

	p_stv4100 = (stv4100_config *) (hTunerParams->pAddParams);
	if (!p_stv4100)
		return CHIPERR_INVALID_HANDLE;

	if (p_stv4100->Fxtal_Hz == 27000000)
		ChipUpdateDefaultValues(hTuner, DefSTV4100_27MHz_Val);
	else if (p_stv4100->Fxtal_Hz == 30000000)
		ChipUpdateDefaultValues(hTuner, DefSTV4100_30MHz_Val);
	else if (p_stv4100->Fxtal_Hz == 16000000)
		ChipUpdateDefaultValues(hTuner, DefSTV4100_16MHz_Val);
	else
		ChipUpdateDefaultValues(hTuner, DefSTV4100_25MHz_Val);
#if defined(CHIP_STAPI) || defined(NO_GUI)
	error = STV4100_TunerWrite(hTuner);
#endif

	return error;
}

TUNER_Error_t STV4100_TunerTerm(STCHIP_Handle_t hTuner)
{
	TUNER_Error_t error = TUNER_NO_ERR;

	if (hTuner) {
#ifndef CHIP_STAPI
		if (hTuner->pData)
			free(hTuner->pData);
		ChipClose(hTuner);
#endif

	} else {
		error = TUNER_INVALID_HANDLE;
	}
	return error;
}
