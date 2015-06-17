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

Source file name :stv6120_tuner.c
Author :           LLA

tuner lla file

Date        Modification                                    Name
----        ------------                                    --------
20-Jun-11   Created                                         Shobhit

************************************************************************/

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
#include "fe_sat_tuner.h"
#include "stv6120_tuner.h"
#include <gen_macros.h>

	/* STV6120 tuner definition */
	/*STV6120 */

/*CTRL1*/
#define RSTV6120_CTRL1  0x0000
#define FSTV6120_K  0x000000f8
#define FSTV6120_RDIV  0x00000004
#define FSTV6120_ODIV  0x00000002
#define FSTV6120_MCLKDIV  0x00000001

/*CTRL2*/
#define RSTV6120_CTRL2  0x0001
#define FSTV6120_DCLOOPOFF_1  0x00010080
#define FSTV6120_SDOFF_1  0x00010040
#define FSTV6120_SYN_1  0x00010020
#define FSTV6120_REFOUTSEL_1  0x00010010
#define FSTV6120_BBGAIN_1  0x0001000f

/*CTRL3*/
#define RSTV6120_CTRL3  0x0002
#define FSTV6120_NDIV_1_LSB  0x000200ff

/*CTRL4*/
#define RSTV6120_CTRL4  0x0003
#define FSTV6120_F_1_L  0x000300fe
#define FSTV6120_NDIV_1_MSB  0x00030001

/*CTRL5*/
#define RSTV6120_CTRL5  0x0004
#define FSTV6120_F_1_M  0x000400ff

/*CTRL6*/
#define RSTV6120_CTRL6  0x0005
#define FSTV6120_ICP_1  0x00050070
#define FSTV6120_VCOILOW_1  0x00050008
#define FSTV6120_F_1_H  0x00050007

/*CTRL7*/
#define RSTV6120_CTRL7  0x0006
#define FSTV6120_RCCLFOFF_1  0x00060080
#define FSTV6120_PDIV_1  0x00060060
#define FSTV6120_CF_1  0x0006001f

/*CTRL8*/
#define RSTV6120_CTRL8  0x0007
#define FSTV6120_TCAL  0x000700c0
#define FSTV6120_CALTIME_1  0x00070020
#define FSTV6120_CFHF_1  0x0007001f

/*STAT1*/
#define RSTV6120_STAT1  0x0008
#define FSTV6120_CALVCOSTRT_1  0x00080004
#define FSTV6120_CALRCSTRT_1  0x00080002
#define FSTV6120_LOCK_1  0x00080001

/*CTRL9*/
#define RSTV6120_CTRL9  0x0009
#define FSTV6120_RFSEL_2  0x0009000c
#define FSTV6120_RFSEL_1  0x00090003

/*CTRL10*/
#define RSTV6120_CTRL10  0x000a
#define FSTV6120_LNADON  0x000a0020
#define FSTV6120_LNACON  0x000a0010
#define FSTV6120_LNABON  0x000a0008
#define FSTV6120_LNAAON  0x000a0004
#define FSTV6120_PATHON_2  0x000a0002
#define FSTV6120_PATHON_1  0x000a0001

/*CTRL11*/
#define RSTV6120_CTRL11  0x000b
#define FSTV6120_DCLOOPOFF_2  0x000b0080
#define FSTV6120_SDOFF_2  0x000b0040
#define FSTV6120_SYN_2  0x000b0020
#define FSTV6120_REFOUTSEL_2  0x000b0010
#define FSTV6120_BBGAIN_2  0x000b000f

/*CTRL12*/
#define RSTV6120_CTRL12  0x000c
#define FSTV6120_NDIV_2_LSB  0x000c00ff

/*CTRL13*/
#define RSTV6120_CTRL13  0x000d
#define FSTV6120_F_2_L  0x000d00fe
#define FSTV6120_NDIV_2_MSB  0x000d0001

/*CTRL14*/
#define RSTV6120_CTRL14  0x000e
#define FSTV6120_F_2_M  0x000e00ff

/*CTRL15*/
#define RSTV6120_CTRL15  0x000f
#define FSTV6120_ICP_2  0x000f0070
#define FSTV6120_VCOILOW_2  0x000f0008
#define FSTV6120_F_2_H  0x000f0007

/*CTRL16*/
#define RSTV6120_CTRL16  0x0010
#define FSTV6120_RCCLFOFF_2  0x00100080
#define FSTV6120_PDIV_2  0x00100060
#define FSTV6120_CF_2  0x0010001f

/*CTRL17*/
#define RSTV6120_CTRL17  0x0011
#define FSTV6120_CALTIME_2  0x00110020
#define FSTV6120_CFHF_2  0x0011001f

/*STAT2*/
#define RSTV6120_STAT2  0x0012
#define FSTV6120_CALVCOSTRT_2  0x00120004
#define FSTV6120_CALRCSTRT_2  0x00120002
#define FSTV6120_LOCK_2  0x00120001

/*CTRL18*/
#define RSTV6120_CTRL18  0x0013
#define FSTV6120_TEST1  0x001300ff

/*CTRL19*/
#define RSTV6120_CTRL19  0x0014
#define FSTV6120_TEST2  0x001400ff

/*CTRL20*/
#define RSTV6120_CTRL20  0x0015
#define FSTV6120_TEST3  0x001500ff

/*CTRL21*/
#define RSTV6120_CTRL21  0x0016
#define FSTV6120_TEST4  0x001600ff

/*CTRL22*/
#define RSTV6120_CTRL22  0x0017
#define FSTV6120_TEST5  0x001700ff

/*CTRL23*/
#define RSTV6120_CTRL23  0x0018
#define FSTV6120_TEST6  0x001800ff

static U32 STB6120_LOOKUP[7][3] = {
	/* low, high, lcp */
	{2300000, 2388000, 0},
	{2389000, 2610000, 1},
	{2611000, 2919000, 2},
	{2920000, 3273000, 3},
	{3274000, 3715000, 5},
	{3716000, 4246000, 6},
	{4247000, 4600000, 7}
};

static FE_LLA_LOOKUP_t FE_STV6120_Gain_LookUp = {
	75,
	{
	 {7429, 0},		/*  74.61 dB        */
	 {7368, 18711},		/* 74.43 dB        */
	 {7214, 23432},		/* 73.36 dB        */
	 {7090, 25123},		/* 72.4  dB        */
	 {6988, 26305},		/* 71.47 dB        */
	 {6897, 27100},		/* 70.47 dB        */
	 {6809, 27741},		/* 69.5  dB        */
	 {6728, 28271},		/* 68.52 dB        */
	 {6645, 28737},		/* 67.52 dB        */
	 {6571, 29120},		/* 66.53 dB        */
	 {6494, 29504},		/* 65.54 dB        */
	 {6416, 29857},		/* 64.55 dB        */
	 {6341, 30180},		/* 63.55 dB        */
	 {6263, 30490},		/* 62.56 dB        */
	 {6179, 30815},		/* 61.57 dB        */
	 {6101, 31088},		/* 60.58 dB        */
	 {6028, 31345},		/* 59.57 dB        */
	 {5956, 31600},		/* 58.59 dB        */
	 {5883, 31840},		/* 57.6  dB        */
	 {5801, 32096},		/* 56.6  dB        */
	 {5730, 32320},		/* 55.59 dB        */
	 {5659, 32544},		/* 54.59 dB        */
	 {5582, 32752},		/* 53.61 dB        */
	 {5498, 32960},		/* 52.61 dB        */
	 {5414, 33184},		/* 51.61 dB        */
	 {5340, 33392},		/* 50.62 dB        */
	 {5271, 33584},		/* 49.61 dB        */
	 {5198, 33775},		/* 48.61 dB        */
	 {5125, 33967},		/* 47.62 dB        */
	 {5048, 34160},		/* 46.62 dB        */
	 {4963, 34352},		/* 45.62 dB        */
	 {4884, 34543},		/* 44.62 dB        */
	 {4820, 34719},		/* 43.62 dB        */
	 {4740, 34910},		/* 42.62 dB        */
	 {4666, 35103},		/* 41.62 dB        */
	 {4582, 35295},		/* 40.62 dB        */
	 {4500, 35488},		/* 39.62 dB        */
	 {4426, 35680},		/* 38.62 dB        */
	 {4365, 35870},		/* 37.62 dB        */
	 {4279, 36095},		/* 36.62 dB        */
	 {4113, 36289},		/* 35.62 dB        */
	 {4020, 36500},		/* 34.62 dB        */
	 {3930, 36704},		/* 33.62 dB        */
	 {3838, 36912},		/* 32.62 dB        */
	 {3738, 37152},		/* 31.62 dB        */
	 {3648, 37375},		/* 30.62 dB        */
	 {3544, 37600},		/* 29.62 dB        */
	 {3458, 37823},		/* 28.62 dB        */
	 {3358, 38048},		/* 27.62 dB        */
	 {3281, 38240},		/* 26.62 dB        */
	 {3191, 38479},		/* 25.62 dB        */
	 {3091, 38720},		/* 24.62 dB        */
	 {2993, 38976},		/* 23.63 dB        */
	 {2900, 39226},		/* 22.63 dB        */
	 {2792, 39520},		/* 21.62 dB        */
	 {2692, 39792},		/* 20.62 dB        */
	 {2592, 40064},		/* 19.62 dB        */
	 {2497, 40351},		/* 18.62 dB        */
	 {2392, 40640},		/* 17.62 dB        */
	 {2290, 40976},		/* 16.62 dB        */
	 {2189, 41295},		/* 15.62 dB        */
	 {2088, 41631},		/* 14.62 dB        */
	 {1999, 41934},		/* 13.62 dB        */
	 {1875, 42354},		/* 12.62 dB        */
	 {1764, 42815},		/* 11.62 dB        */
	 {1637, 43263},		/* 10.62 dB        */
	 {1537, 43743},		/*  9.62  dB        */
	 {1412, 44288},		/*  8.62  dB        */
	 {1291, 44913},		/*  7.62  dB        */
	 {1188, 45712},		/*  6.62  dB        */
	 {1080, 46720},		/*  5.63  dB        */
	 {976, 48164},		/*  4.63  dB        */
	 {930, 50816},		/*  3.63  dB        */
	 {898, 65534},		/*  2.94  dB        */
	 {880, 65535}		/*   2.95  dB        */

	 }
};

TUNER_Error_t STV6120_TunerInit(void *pTunerInit_v,
				STCHIP_Handle_t *TunerHandle)
{
	SAT_TUNER_Params_t hTunerParams = NULL;
	STCHIP_Info_t ChipInfo;
	STCHIP_Handle_t hTuner = NULL;
	TUNER_Error_t error = TUNER_NO_ERR;
	SAT_TUNER_Params_t pTunerInit = (SAT_TUNER_Params_t) pTunerInit_v;

	STCHIP_Register_t DefSTV6120Val[STV6120_NBREGS] = {
		{RSTV6120_CTRL1, 0x75},
		{RSTV6120_CTRL2, 0x33},
		{RSTV6120_CTRL3, 0xCE},
		{RSTV6120_CTRL4, 0x54},
		{RSTV6120_CTRL5, 0x55},
		{RSTV6120_CTRL6, 0x0D},
		{RSTV6120_CTRL7, 0x32},
		{RSTV6120_CTRL8, 0x44},
		{RSTV6120_STAT1, 0x0E},
		{RSTV6120_CTRL9, 0xF9},
		/*{RSTV6120_CTRL10, 0x5B}, */
		/*stop LNA ABCD to avoid consumption */
		{RSTV6120_CTRL10, /*0x43 */ 0x03},
		{RSTV6120_CTRL11, 0x33},
		{RSTV6120_CTRL12, 0xCE},
		{RSTV6120_CTRL13, 0x54},
		{RSTV6120_CTRL14, 0x55},
		{RSTV6120_CTRL15, 0x0D},
		{RSTV6120_CTRL16, 0x32},
		{RSTV6120_CTRL17, 0x44},
		{RSTV6120_STAT2, 0x0E},
		{RSTV6120_CTRL18, 0x00},
		{RSTV6120_CTRL19, 0x00},
		{RSTV6120_CTRL20, 0x4C},
		{RSTV6120_CTRL21, 0x00},
		{RSTV6120_CTRL22, 0x00},
		{RSTV6120_CTRL23, 0x4C}
	};

	/*
	 **     REGISTER CONFIGURATION
	 **      ----------------------
	 */
#ifdef HOST_PC
	/* Allocation of the chip structure     */
	hTunerParams = calloc(1, sizeof(SAT_TUNER_InitParams_t));
#endif

#ifdef CHIP_STAPI
	if (*TunerHandle) {
		hTunerParams = (SAT_TUNER_Params_t) ((*TunerHandle)->pData);
		memcpy(&ChipInfo, *TunerHandle, sizeof(STCHIP_Info_t));
	} else {
		error = TUNER_INVALID_HANDLE;
	}
#endif

	if (hTunerParams == NULL)
		return TUNER_INVALID_HANDLE;

	hTunerParams->Model = pTunerInit->Model;	/* Tuner model */
	/* Reference Clock in Hz */
	hTunerParams->Reference = pTunerInit->Reference;
	/* IF Hz intermediate frequency */
	hTunerParams->IF = pTunerInit->IF;
	/* hardware IQ invertion */
	hTunerParams->IQ_Wiring = pTunerInit->IQ_Wiring;
	/* Wide band tuner(6130 like, band selection) */
	hTunerParams->BandSelect = pTunerInit->BandSelect;
	/* Wide band tuner LB, HB border in KHz(6120 like) */
	hTunerParams->BandBorder = pTunerInit->BandBorder;
	/*commented in 900 LLA4.6
	 * Path selection is now based on BandSelect */
	/* Used path, Path1 or Path2 for dual tuner */
	/*hTunerParams->TunerPath = pTunerInit->TunerPath; */
	/* Dual Wide band tuner(6120 like, band selection) */
	hTunerParams->BandSelect2 = pTunerInit->BandSelect2;
	/* Input Selection for dual tuner */
	hTunerParams->InputSelect = pTunerInit->InputSelect;
	hTunerParams->RF_Source = pTunerInit->RF_Source;
	/*fix Coverity CID 20142 */
	if (strlen((char *)pTunerInit->TunerName) < MAXNAMESIZE)
		/* Tuner name */
		strcpy((char *)ChipInfo.Name, (char *)pTunerInit->TunerName);
	else
		error = TUNER_TYPE_ERR;

	/* Repeater host */
	ChipInfo.RepeaterHost = pTunerInit->RepeaterHost;
	/* Repeater enable/disable function */
	ChipInfo.RepeaterFn = pTunerInit->RepeaterFn;
	ChipInfo.Repeater = TRUE;	/* Tuner need to enable repeater */
	/* Init tuner I2C address */
	ChipInfo.I2cAddr = pTunerInit->TunerI2cAddress;

	/* Store tunerparams pointer into Chip structure */
	ChipInfo.pData = hTunerParams;

	ChipInfo.NbRegs = STV6120_NBREGS;
	ChipInfo.NbFields = STV6120_NBFIELDS;
	ChipInfo.ChipMode = STCHIP_MODE_SUBADR_8;
	ChipInfo.WrStart = RSTV6120_CTRL1;
	ChipInfo.WrSize = 25;
	ChipInfo.RdStart = RSTV6120_CTRL1;
	ChipInfo.RdSize = 25;

#ifdef HOST_PC
	hTuner = ChipOpen(&ChipInfo);
	(*TunerHandle) = hTuner;
#endif
#ifdef CHIP_STAPI
	hTuner = *TunerHandle;
	memcpy(hTuner, &ChipInfo, sizeof(STCHIP_Info_t));
#endif

	if ((*TunerHandle) == NULL)
		error = TUNER_INVALID_HANDLE;

	if (hTuner != NULL) {
		/*******************************
		 **	CHIP REGISTER MAP IMAGE INITIALIZATION
		 **	 ----------------------
		 ********************************/
		ChipUpdateDefaultValues(hTuner, DefSTV6120Val);

		/*******************************
		 **	REGISTER CONFIGURATION
		 **	 ----------------------
		 ********************************/

		/*Update the clock divider before registers init */
		/*Allowed values 1, 2, 4 and 8 */
		switch (pTunerInit->OutputClockDivider) {
		case 1:
		default:
			ChipSetFieldImage(hTuner, FSTV6120_ODIV, 0);
			break;

		case 2:
			ChipSetFieldImage(hTuner, FSTV6120_ODIV, 1);
			break;

		case 4:
		case 8:
		case 0:
			/* Tuner output clock not used then
			 * divide by 8(the 6120 can not stop
			 * completely the out clock)*/
			break;
		}

		/*update K field acc to the Reference clock(quartz) */
		ChipSetFieldImage(hTuner, FSTV6120_K,
				  ((hTunerParams->Reference / 1000000) - 16));

		/*update RDIV field acc to the Reference clock(quartz) */
		/*if FXTAL >= 27MHz set R to 2
		 * else if FXTAL < 27MHz set R to 1 */
		if (hTunerParams->Reference >= 27000000)
			ChipSetFieldImage(hTuner, FSTV6120_RDIV, 1);
		else
			ChipSetFieldImage(hTuner, FSTV6120_RDIV, 0);

		/*I2C registers initialization */
		/* mandatory read when 2 instance of the
		 * 6120 are used(one for each path)*/
		ChipGetRegisters(hTuner, RSTV6120_CTRL9, 1);

		if (hTuner->Error != CHIPERR_NO_ERROR) {
			error = TUNER_I2C_NO_ACK;
			return error;
		}
		error = STV6120_TunerWrite(hTuner);
		/*************R/W test code**************************
		  STV6120_TunerRead(hTuner);
		  {
		  int i = 0;
		  printf("\n TUNER READ");
		  for (i = 0 ; i < STV6120_NBREGS+1 ; i++) {
		  printf("\nRegister[%d] = 0x%x",
		  hTuner->pRegMapImage[i].Addr,
		  hTuner->pRegMapImage[i].Value);
		  }
		  }
		 ***************************************************/
		/* when the STV6120 is used as wideband tuner set this
		 * value to the upper limit of the low band*/
		hTunerParams->BandBorder = 0;
		switch (hTunerParams->RF_Source) {
		case FE_RF_SOURCE_A:
			if (hTunerParams->InputSelect == TUNER_INPUT1)
				ChipSetField(hTuner, FSTV6120_RFSEL_1, 0);
			else
				ChipSetField(hTuner, FSTV6120_RFSEL_2, 0);
			break;

		case FE_RF_SOURCE_B:
			if (hTunerParams->InputSelect == TUNER_INPUT1)
				ChipSetField(hTuner, FSTV6120_RFSEL_1, 1);
			else
				ChipSetField(hTuner, FSTV6120_RFSEL_2, 1);
			break;

		case FE_RF_SOURCE_C:
			if (hTunerParams->InputSelect == TUNER_INPUT1)
				ChipSetField(hTuner, FSTV6120_RFSEL_1, 2);
			else
				ChipSetField(hTuner, FSTV6120_RFSEL_2, 2);
			break;

		case FE_RF_SOURCE_D:
			if (hTunerParams->InputSelect == TUNER_INPUT1)
				ChipSetField(hTuner, FSTV6120_RFSEL_1, 3);
			else
				ChipSetField(hTuner, FSTV6120_RFSEL_2, 3);
			break;
		default:
			break;
		}
	}

	return error;
}

TUNER_Error_t STV6120_TunerSetStandby(STCHIP_Handle_t hTuner, U8 StandbyOn)
{
	TUNER_Error_t error = TUNER_NO_ERR;
	SAT_TUNER_Params_t hTunerParams = NULL;

	/*fix Coverity CID: 20138 */
	if (hTuner)
		hTunerParams = (SAT_TUNER_Params_t) hTuner->pData;
	else
		error = TUNER_INVALID_HANDLE;

	if (!hTunerParams || hTuner->Error)
		return error;
	/*Power down ON */
	if (StandbyOn) {
		if (hTunerParams->Model == TUNER_STV6120_Tuner1) {
			ChipSetField(hTuner, FSTV6120_SYN_1, 0);
			ChipSetField(hTuner, FSTV6120_SDOFF_1, 1);
			ChipSetField(hTuner, FSTV6120_PATHON_1, 0);
		} else if (hTunerParams->Model == TUNER_STV6120_Tuner2) {
			ChipSetField(hTuner, FSTV6120_SYN_2, 0);
			ChipSetField(hTuner, FSTV6120_SDOFF_2, 1);
			ChipSetField(hTuner, FSTV6120_PATHON_2, 0);
		}
	} else {	/*Power down OFF */
		if (hTunerParams->Model == TUNER_STV6120_Tuner1) {
			ChipSetField(hTuner, FSTV6120_SYN_1, 1);
			ChipSetField(hTuner, FSTV6120_SDOFF_1, 0);
			ChipSetField(hTuner, FSTV6120_PATHON_1, 1);
		} else if (hTunerParams->Model == TUNER_STV6120_Tuner2) {
			ChipSetField(hTuner, FSTV6120_SYN_2, 1);
			ChipSetField(hTuner, FSTV6120_SDOFF_2, 0);
			ChipSetField(hTuner, FSTV6120_PATHON_2, 1);
		}
	}
	return error;
}

TUNER_Error_t STV6120_TunerSetFrequency(STCHIP_Handle_t hTuner, U32 Frequency)
{
	TUNER_Error_t error = TUNER_NO_ERR;
	SAT_TUNER_Params_t hTunerParams = NULL;

	U32 divider,
	    swallow, p, p2, rDiv, frequency2, fequencyVco, fequencyVco2, delta;

	S32 i;
	U8 u8;

	/*TUNER_WIDEBandS_t BandSelect; */
	/*fix Coverity CID: 20135 */
	if (!hTuner)
		return TUNER_INVALID_HANDLE;

	hTunerParams = (SAT_TUNER_Params_t)hTuner->pData;

	if (!hTunerParams || hTuner->Error)
		return TUNER_INVALID_HANDLE;

	rDiv = ChipGetFieldImage(hTuner, FSTV6120_RDIV);

	if (Frequency <= 325000)	/* in kHz */
		p = 3;
	else if (Frequency <= 650000)
		p = 2;
	else if (Frequency < /*1300000 */ 1151000) /*6120 T50 */
		p = 1;
	else
		p = 0;
	/* Read second path frequency */
	if (hTunerParams->Model == TUNER_STV6120_Tuner2) {
		hTunerParams->Model = TUNER_STV6120_Tuner1;
		frequency2 = FE_Sat_TunerGetFrequency(hTuner);
		hTunerParams->Model = TUNER_STV6120_Tuner2;
		p2 = ChipGetField(hTuner, FSTV6120_PDIV_1);
	} else {
		hTunerParams->Model = TUNER_STV6120_Tuner2;
		frequency2 = FE_Sat_TunerGetFrequency(hTuner);
		hTunerParams->Model = TUNER_STV6120_Tuner1;

		p2 = ChipGetField(hTuner, FSTV6120_PDIV_2);
	}

	/*Path1 Frequency and Path2 Frequency must distant by 2MHz at least */
	/* if Not move wanted Freqency by 2MHz from the other path frequency */
	/*
	   if (ABS((S32)Frequency - (S32)frequency2) < 2000) {
	   if (Frequency > frequency2) {
	   Frequency = frequency2+2000;
	   } else {
	   Frequency = frequency2 - 2000;
	   }

	   }
	 */
	fequencyVco = Frequency * PowOf2(p + 1);
	fequencyVco2 = frequency2 * PowOf2(p2 + 1);

	delta = 4000 * PowOf2(ABS((S32) p2 - (S32) p));

	if (ABS((S32)fequencyVco - (S32)fequencyVco2) < delta) {
		if (fequencyVco > fequencyVco2) {
			if (p > p2)
				Frequency =
				    (frequency2 / (delta / 4000)) + 2000;
			else
				Frequency =
				    (frequency2 * (delta / 4000)) + 2000;
		} else {
			if (p > p2)
				Frequency =
				    (frequency2 / (delta / 4000)) - 2000;
			else
				Frequency =
				    (frequency2 * (delta / 4000)) - 2000;
		}

	}

	swallow = (Frequency * PowOf2(p + rDiv + 1));
	/*N = (Frequency *P*R)/Fxtal */
	divider = swallow / (hTunerParams->Reference / 1000);
	/*F = (2^18 *(Frequency *P*R)%Fxtal)/Fxtal */
	swallow = swallow % (hTunerParams->Reference / 1000);
	if (swallow < 0x3FFF)	/*to avoid U32 bits saturation */
		swallow = (0x40000 * swallow) /
		    (hTunerParams->Reference / 1000);
	else if (swallow < 0x3FFF0)
		/*to avoid U32 bits saturation */
		swallow = (0x8000 * swallow) /
		    (hTunerParams->Reference / 8000);
	else	/*to avoid U32 bits saturation */
		swallow = (0x2000 * swallow) /
		    (hTunerParams->Reference / 32000);

	/*      switch (hTunerParams->InputSelect) {
	   case TUNER_INPUT1:
	   default:
	   BandSelect = hTunerParams->BandSelect;
	   break;

	   case TUNER_INPUT2:
	   BandSelect = hTunerParams->BandSelect2;
	   break;
	   } */

	if (hTunerParams->Model == TUNER_STV6120_Tuner1) {
		u8 = 0;	/*6120 T50 */
		while (!INRANGE(STB6120_LOOKUP[u8][0], fequencyVco,
					STB6120_LOOKUP[u8][1]) && (u8 < 6))
			u8++;	/*6120 T50 */
		/*6120 T50 */
		ChipSetFieldImage(hTuner, FSTV6120_ICP_1,
				STB6120_LOOKUP[u8][2]);
		/*6120 T50 */
		ChipSetRegisters(hTuner, RSTV6120_CTRL6, 1);

		ChipSetFieldImage(hTuner, FSTV6120_PDIV_1, p);
		ChipSetFieldImage(hTuner, FSTV6120_NDIV_1_LSB,
				  (divider & 0xFF));
		ChipSetFieldImage(hTuner, FSTV6120_NDIV_1_MSB,
				  ((divider >> 8) & 0x01));
		ChipSetFieldImage(hTuner, FSTV6120_F_1_H,
				  ((swallow >> 15) & 0x07));
		ChipSetFieldImage(hTuner, FSTV6120_F_1_M,
				  ((swallow >> 7) & 0xFF));
		ChipSetFieldImage(hTuner, FSTV6120_F_1_L, (swallow & 0x7F));

		ChipSetRegisters(hTuner, RSTV6120_CTRL3, 5);

		/* VCO Auto Calibration */
		ChipSetField(hTuner, FSTV6120_CALVCOSTRT_1, 1);

		i = 0;
		while ((i < 10) &&
		       (ChipGetField(hTuner, FSTV6120_CALVCOSTRT_1) != 0)) {
			/* wait for VCO auto calibration */
			ChipWaitOrAbort(hTuner, 1);
			i++;
		}

	} else if (hTunerParams->Model == TUNER_STV6120_Tuner2) {
		u8 = 0;	/*6120 T50 */
		while (!INRANGE(STB6120_LOOKUP[u8][0], fequencyVco,
					STB6120_LOOKUP[u8][1]) && (u8 < 6))
			u8++;	/*6120 T50 */
		/*6120 T50 */
		ChipSetFieldImage(hTuner, FSTV6120_ICP_2,
				STB6120_LOOKUP[u8][2]);
		/*6120 T50 */
		ChipSetRegisters(hTuner, RSTV6120_CTRL15, 1);

		ChipSetFieldImage(hTuner, FSTV6120_PDIV_2, p);
		ChipSetFieldImage(hTuner, FSTV6120_NDIV_2_LSB,
				  (divider & 0xFF));
		ChipSetFieldImage(hTuner, FSTV6120_NDIV_2_MSB,
				  ((divider >> 8) & 0x01));
		ChipSetFieldImage(hTuner, FSTV6120_F_2_H,
				  ((swallow >> 15) & 0x07));
		ChipSetFieldImage(hTuner, FSTV6120_F_2_M,
				  ((swallow >> 7) & 0xFF));
		ChipSetFieldImage(hTuner, FSTV6120_F_2_L, (swallow & 0x7F));

		ChipSetRegisters(hTuner, RSTV6120_CTRL12, 5);

		/* VCO Auto Calibration */
		ChipSetField(hTuner, FSTV6120_CALVCOSTRT_2, 1);

		i = 0;
		while ((i < 10) &&
		       (ChipGetField(hTuner, FSTV6120_CALVCOSTRT_2) != 0)) {
			/* wait for VCO auto calibration */
			ChipWaitOrAbort(hTuner, 1);
			i++;
		}

	}

	return error;
}

U32 STV6120_TunerGetFrequency(STCHIP_Handle_t hTuner)
{

	SAT_TUNER_Params_t hTunerParams = NULL;
	U32 frequency = 0;
	U32 swallow;
	U32 divider = 0;
	U32 psd2;

	/*fix Coverity CID: 20128 */
	if (!hTuner)
		return frequency;
	hTunerParams = (SAT_TUNER_Params_t) hTuner->pData;

	if (!hTunerParams || hTuner->Error)
		return frequency;
	psd2 = ChipGetFieldImage(hTuner, FSTV6120_RDIV);
	if (hTunerParams->Model == TUNER_STV6120_Tuner1) {
		ChipGetRegisters(hTuner, RSTV6120_CTRL3, 5);
		divider =
		    MAKEWORD(ChipGetFieldImage(hTuner, FSTV6120_NDIV_1_MSB),
			     ChipGetFieldImage(hTuner, FSTV6120_NDIV_1_LSB));
		swallow =
		    ((ChipGetFieldImage(hTuner, FSTV6120_F_1_H) & 0x07) << 15) +
		    ((ChipGetFieldImage(hTuner, FSTV6120_F_1_M) & 0xFF) << 7) +
		    (ChipGetFieldImage(hTuner, FSTV6120_F_1_L) & 0x7F);

		frequency = (hTunerParams->Reference / 1000) /
			PowOf2(ChipGetFieldImage(hTuner, FSTV6120_RDIV));
		frequency =
		    (frequency * divider) + ((frequency * swallow) / 0x40000);
		frequency /= PowOf2(1 +
				    ChipGetFieldImage(hTuner, FSTV6120_PDIV_1));

	} else if (hTunerParams->Model == TUNER_STV6120_Tuner2) {
		ChipGetRegisters(hTuner, RSTV6120_CTRL12, 5);
		divider =
		    MAKEWORD(ChipGetFieldImage(hTuner, FSTV6120_NDIV_2_MSB),
			     ChipGetFieldImage(hTuner, FSTV6120_NDIV_2_LSB));
		swallow =
		    ((ChipGetFieldImage(hTuner, FSTV6120_F_2_H) & 0x07) << 15) +
		    ((ChipGetFieldImage(hTuner, FSTV6120_F_2_M) & 0xFF) << 7) +
		    (ChipGetFieldImage(hTuner, FSTV6120_F_2_L) & 0x7F);

		frequency = (hTunerParams->Reference / 1000) /
			PowOf2(ChipGetFieldImage(hTuner, FSTV6120_RDIV));
		frequency =
		    (frequency * divider) + ((frequency * swallow) / 0x40000);
		frequency /= PowOf2(1 +
				    ChipGetFieldImage(hTuner, FSTV6120_PDIV_2));

	}

	return frequency;
}

TUNER_Error_t STV6120_TunerSetBandwidth(STCHIP_Handle_t hTuner, U32 Bandwidth)
{
	TUNER_Error_t error = TUNER_NO_ERR;
	SAT_TUNER_Params_t hTunerParams = NULL;
	U8 u8;
	S32 i = 0;

	/*fix Coverity CID: 20134 */
	if (!hTuner)
		return TUNER_INVALID_HANDLE;

	hTunerParams = (SAT_TUNER_Params_t)hTuner->pData;

	if (!hTunerParams || hTuner->Error)
		return TUNER_INVALID_HANDLE;

	if ((Bandwidth / 2) > 36000000)
		/*F[4:0] BW/2 max = 31+5 = 36 mhz for F = 31 */
		u8 = 31;
	else if ((Bandwidth / 2) < 5000000)
		/* BW/2 min = 5Mhz for F = 0 */
		u8 = 0;
	else	/*if 5 < BW/2 < 36 */
		u8 = (Bandwidth / 2) / 1000000 - 5;

	if (hTunerParams->Model == TUNER_STV6120_Tuner1) {
		/* Set the LPF value */
		ChipSetField(hTuner, FSTV6120_CF_1, u8);
		/* Start LPF auto calibration */
		ChipSetField(hTuner, FSTV6120_CALRCSTRT_1, 1);

		i = 0;
		while ((i < 10) &&
		       (ChipGetField(hTuner, FSTV6120_CALRCSTRT_1) != 0)) {
			/* wait for LPF auto calibration */
			ChipWaitOrAbort(hTuner, 1);
			i++;
		}
	} else if (hTunerParams->Model == TUNER_STV6120_Tuner2) {
		/* Set the LPF value */
		ChipSetField(hTuner, FSTV6120_CF_2, u8);
		/* Start LPF auto calibration */
		ChipSetField(hTuner, FSTV6120_CALRCSTRT_2, 1);

		i = 0;
		while ((i < 10) &&
		       (ChipGetField(hTuner, FSTV6120_CALRCSTRT_2) != 0)) {
			/* wait for LPF auto calibration */
			ChipWaitOrAbort(hTuner, 1);
			i++;
		}
	}
	return error;
}

U32 STV6120_TunerGetBandwidth(STCHIP_Handle_t hTuner)
{

	SAT_TUNER_Params_t hTunerParams = NULL;
	U32 bandwidth = 0;
	U8 u8 = 0;

	/*fix Coverity CID: 20127 */
	if (hTuner) {
		hTunerParams = (SAT_TUNER_Params_t) hTuner->pData;

		if (hTunerParams && !hTuner->Error) {
			if (hTunerParams->Model == TUNER_STV6120_Tuner1)
				u8 = ChipGetField(hTuner, FSTV6120_CF_1);
			else if (hTunerParams->Model == TUNER_STV6120_Tuner2)
				u8 = ChipGetField(hTuner, FSTV6120_CF_2);

			/* x2 for ZIF tuner BW/2 = F+5 Mhz */
			bandwidth = (u8 + 5) * 2000000;

		}
	}

	return bandwidth;
}

TUNER_Error_t STV6120_TunerSetGain(STCHIP_Handle_t hTuner, S32 Gain)
{
	TUNER_Error_t error = TUNER_NO_ERR;
	SAT_TUNER_Params_t hTunerParams = NULL;

	/*fix Coverity CID: 20136 */
	if (!hTuner)
		return TUNER_INVALID_HANDLE;
	hTunerParams = (SAT_TUNER_Params_t) hTuner->pData;

	if (!hTunerParams || hTuner->Error)
		return TUNER_INVALID_HANDLE;

	if (hTunerParams->Model == TUNER_STV6120_Tuner1)
		ChipSetField(hTuner, FSTV6120_BBGAIN_1, (Gain / 2));
	else if (hTunerParams->Model == TUNER_STV6120_Tuner2)
		ChipSetField(hTuner, FSTV6120_BBGAIN_2, (Gain / 2));

	return error;
}

S32 STV6120_TunerGetGain(STCHIP_Handle_t hTuner)
{

	SAT_TUNER_Params_t hTunerParams = NULL;
	S32 gain = 0;

	/*fix Coverity CID: 20129 */
	if (!hTuner)
		return gain;
	hTunerParams = (SAT_TUNER_Params_t) hTuner->pData;

	if (!hTunerParams || hTuner->Error)
		return gain;

	if (hTunerParams->Model == TUNER_STV6120_Tuner1)
		gain = 2 * ChipGetField(hTuner, FSTV6120_BBGAIN_1);
	else if (hTunerParams->Model == TUNER_STV6120_Tuner2)
		gain = 2 * ChipGetField(hTuner, FSTV6120_BBGAIN_2);

	return gain;
}

TUNER_Error_t STV6120_TunerSetHMR_Filter(STCHIP_Handle_t hTuner,
					 S32 filterValue)
{
	TUNER_Error_t error = TUNER_NO_ERR;
	SAT_TUNER_Params_t hTunerParams = NULL;

	/*fix Coverity SD30 */
	if (!hTuner)
		return TUNER_INVALID_HANDLE;

	hTunerParams = (SAT_TUNER_Params_t) hTuner->pData;

	if (!hTunerParams || hTuner->Error)
		return TUNER_INVALID_HANDLE;

	if (hTunerParams->Model == TUNER_STV6120_Tuner1)
		ChipSetField(hTuner, FSTV6120_CFHF_1, filterValue);
	else
		ChipSetField(hTuner, FSTV6120_CFHF_2, filterValue);

	return error;
}

TUNER_Error_t STV6120_TunerSetOutputClock(STCHIP_Handle_t hTuner, S32 Divider)
{
	/*sets the crystal oscillator divisor value, for the output clock
	   Divider =:
	   0 ==> Tuner output clock not used
	   1 ==> divide by 1
	   2 ==> divide by 2
	   4 ==> divide by 4
	   8 ==> divide by 8
	 */
	TUNER_Error_t error = TUNER_NO_ERR;
	SAT_TUNER_Params_t hTunerParams = NULL;

	/*fix Coverity CID: 20137 */
	if (hTuner) {
		hTunerParams = (SAT_TUNER_Params_t) hTuner->pData;

		if (hTunerParams && !hTuner->Error) {

			/*Allowed values 1, 2, 4 and 8 */
			switch (Divider) {
			case 1:
			default:
				ChipSetField(hTuner, FSTV6120_ODIV, 0);
				break;

			case 2:
			case 4:
			case 8:
			case 0:
				ChipSetField(hTuner, FSTV6120_ODIV, 1);
				break;
			}
		}
	} else {
		error = TUNER_INVALID_HANDLE;
	}

	return error;
}

TUNER_Error_t STV6120_TunerSetAttenuator(STCHIP_Handle_t hTuner,
					 BOOL AttenuatorOn)
{
	return TUNER_NO_ERR;
}

BOOL STV6120_TunerGetAttenuator(STCHIP_Handle_t hTuner)
{
	return FALSE;
}

BOOL STV6120_TunerGetStatus(STCHIP_Handle_t hTuner)
{

	SAT_TUNER_Params_t hTunerParams = NULL;
	BOOL locked = FALSE;

	/*fix Coverity CID: 20130 */
	if (hTuner) {
		hTunerParams = (SAT_TUNER_Params_t) hTuner->pData;

		if (hTunerParams && !hTuner->Error) {
			if (hTunerParams->Model == TUNER_STV6120_Tuner1)
				locked = ChipGetField(hTuner, FSTV6120_LOCK_1);
			else if (hTunerParams->Model == TUNER_STV6120_Tuner2)
				locked = ChipGetField(hTuner, FSTV6120_LOCK_2);
		}
	}

	return locked;
}

TUNER_Error_t STV6120_TunerSwitchInput(STCHIP_Handle_t hTuner, U8 Input)
{
	/* Switch the tuner input to input A, B, C or D

	   when the tuner is used as sample band i.e BandBorder = 0 then
	   0 ==> input A
	   1 ==> input B
	   2 ==> input C
	   3 ==> input D

	   when the tuner is used as wide band i.e BandBorder > 0 then
	   0 ==> input 1(A, B)
	   1 ==> input 1(A, B)
	   2 ==> input 2(C, D)
	   3 ==> input 2(C, D)

	   The selection between A and B when input1 or C and D when input2
	   depend on the frequency(>BandBorder or < BandBorder) and made when
	   setting the ffrequency

	 */

	TUNER_Error_t error = TUNER_NO_ERR;
	SAT_TUNER_Params_t hTunerParams = NULL;
	/*fix Coverity CID: 20139 */
	if (hTuner) {
		hTunerParams = (SAT_TUNER_Params_t) hTuner->pData;

		if (hTunerParams && !hTuner->Error) {
			switch (Input) {
			case 0:	/* input A */
			default:
				hTunerParams->InputSelect = TUNER_INPUT1;
				break;

			case 1:	/* input B */
				hTunerParams->InputSelect = TUNER_INPUT1;
				break;

			case 2:	/* input C */
				hTunerParams->InputSelect = TUNER_INPUT2;
				break;

			case 3:	/* input D */
				hTunerParams->InputSelect = TUNER_INPUT2;
				break;
			}

			if (hTunerParams->Model == TUNER_STV6120_Tuner1)
				ChipSetFieldImage(hTuner, FSTV6120_RFSEL_1,
						  Input);
			else if (hTunerParams->Model == TUNER_STV6120_Tuner2)
				ChipSetFieldImage(hTuner, FSTV6120_RFSEL_2,
						  Input);

			ChipSetRegisters(hTuner, RSTV6120_CTRL9, 1);

		}
	} else {
		error = TUNER_INVALID_HANDLE;
	}

	return error;

}

S32 STV6120_TunerGetRFGain(STCHIP_Handle_t hTuner, U32 AGCIntegrator,
			   S32 BBGain)
{
	S32 Gain100dB = 1, RefBBgain = 12, Tilt = 6, Imin, Imax, i, Freq;

	SAT_TUNER_Params_t hTunerParams = NULL;

	if (hTuner) {
		hTunerParams = (SAT_TUNER_Params_t) hTuner->pData;
		if (hTunerParams && !hTuner->Error) {
			Imin = 0;
			Imax = FE_STV6120_Gain_LookUp.size - 1;

			if (AGCIntegrator <=
			    FE_STV6120_Gain_LookUp.table[0].regval)
				Gain100dB =
				    FE_STV6120_Gain_LookUp.table[0].realval;
			else if (AGCIntegrator >=
				 FE_STV6120_Gain_LookUp.table[Imax].regval)
				Gain100dB =
				    FE_STV6120_Gain_LookUp.table[Imax].realval;

			else {
				while ((Imax - Imin) > 1) {
					i = (Imax + Imin) >> 1;
					/*equivalent to i = (Imax+Imin)/2; */
					if (INRANGE
					    (FE_STV6120_Gain_LookUp.table[Imin].
					     regval, AGCIntegrator,
					     FE_STV6120_Gain_LookUp.table[i].
					     regval))
						Imax = i;
					else
						Imin = i;
				}

				Gain100dB =
				    (((S32) AGCIntegrator -
				      FE_STV6120_Gain_LookUp.table[Imin].regval)
				     *
				     (FE_STV6120_Gain_LookUp.table[Imax].
				      realval -
				      FE_STV6120_Gain_LookUp.table[Imin].
				      realval)
				     /
				     (FE_STV6120_Gain_LookUp.table[Imax].
				      regval -
				      FE_STV6120_Gain_LookUp.table[Imin].
				      regval))
				    +
				    FE_STV6120_Gain_LookUp.table[Imin].realval;
			}
		}
	}

	Gain100dB = Gain100dB + 100 * (BBGain - RefBBgain);
	Freq = STV6120_TunerGetFrequency(hTuner);
	Freq /= 10000;

	if (Freq < 159)
		/* HMR filter 2dB gain compensation below freq = 1590MHz */
		Gain100dB -= 200;

	Gain100dB -= (((Freq - 155) * Tilt) / 12) * 10;
	return Gain100dB;

}

TUNER_Error_t STV6120_TunerWrite(STCHIP_Handle_t hTuner)
{
	TUNER_Error_t error = TUNER_NO_ERR;
	STCHIP_Error_t chipError = CHIPERR_NO_ERROR;
	SAT_TUNER_Params_t hTunerParams = NULL;
	U8 i = 0;

	/*fix Coverity CID: 20131 */
	if (!hTuner)
		return TUNER_INVALID_HANDLE;

	hTunerParams = (SAT_TUNER_Params_t) hTuner->pData;

	if (hTunerParams && !hTuner->Error) {
		if ((hTunerParams->DemodModel == DEMOD_STI7111)
		    && (hTuner->RepeaterHost->ChipID < 0x20))
			/*Write one register at a time for 7111 cut1.0 */
			for (i = hTuner->WrStart; i < hTuner->WrSize; i++)
				chipError |= ChipSetRegisters(hTuner, i, 1);
		else
			chipError = ChipSetRegisters(hTuner, hTuner->WrStart,
						     hTuner->WrSize);

		switch (chipError) {
		case CHIPERR_NO_ERROR:
			error = TUNER_NO_ERR;
			break;

		case CHIPERR_INVALID_HANDLE:
			error = TUNER_INVALID_HANDLE;
			break;

		case CHIPERR_INVALID_REG_ID:
			error = TUNER_INVALID_REG_ID;
			break;

		case CHIPERR_INVALID_FIELD_ID:
			error = TUNER_INVALID_FIELD_ID;
			break;

		case CHIPERR_INVALID_FIELD_SIZE:
			error = TUNER_INVALID_FIELD_SIZE;
			break;

		case CHIPERR_I2C_NO_ACK:
		default:
			error = TUNER_I2C_NO_ACK;
			break;

		case CHIPERR_I2C_BURST:
			error = TUNER_I2C_BURST;
			break;
		}
	}

	return error;
}

TUNER_Error_t STV6120_TunerRead(STCHIP_Handle_t hTuner)
{
	TUNER_Error_t error = TUNER_NO_ERR;
	STCHIP_Error_t chipError = CHIPERR_NO_ERROR;
	SAT_TUNER_Params_t hTunerParams = NULL;
	U8 i = 0;

	/*fix Coverity CID: 20133 */
	if (!hTuner)
		return TUNER_INVALID_HANDLE;

	hTunerParams = (SAT_TUNER_Params_t) hTuner->pData;

	if (hTunerParams && !hTuner->Error) {
		if ((hTunerParams->DemodModel == DEMOD_STI7111)
		    && (hTuner->RepeaterHost->ChipID < 0x20))
			/*Read one register at a time for cut1.0 */
			for (i = hTuner->RdStart; i < hTuner->RdSize; i++)
				chipError |= ChipGetRegisters(hTuner, i, 1);
		else
			chipError = ChipGetRegisters(hTuner,
						     hTuner->RdStart,
						     hTuner->RdSize);

		switch (chipError) {
		case CHIPERR_NO_ERROR:
			error = TUNER_NO_ERR;
			break;

		case CHIPERR_INVALID_HANDLE:
			error = TUNER_INVALID_HANDLE;
			break;

		case CHIPERR_INVALID_REG_ID:
			error = TUNER_INVALID_REG_ID;
			break;

		case CHIPERR_INVALID_FIELD_ID:
			error = TUNER_INVALID_FIELD_ID;
			break;

		case CHIPERR_INVALID_FIELD_SIZE:
			error = TUNER_INVALID_FIELD_SIZE;
			break;

		case CHIPERR_I2C_NO_ACK:
		default:
			error = TUNER_I2C_NO_ACK;
			break;

		case CHIPERR_I2C_BURST:
			error = TUNER_I2C_BURST;
			break;
		}

	}

	return error;
}

TUNER_Model_t STV6120_TunerGetModel(STCHIP_Handle_t hChip)
{
	TUNER_Model_t model = TUNER_NULL;

	if (hChip && hChip->pData)
		model = ((SAT_TUNER_Params_t) (hChip->pData))->Model;

	return model;
}

void STV6120_TunerSetIQ_Wiring(STCHIP_Handle_t hChip, S8 IQ_Wiring)
{
	if (hChip && hChip->pData)
		((SAT_TUNER_Params_t) (hChip->pData))->IQ_Wiring =
		    (TUNER_IQ_t) IQ_Wiring;
}

S8 STV6120_TunerGetIQ_Wiring(STCHIP_Handle_t hChip)
{
	TUNER_IQ_t wiring = TUNER_IQ_NORMAL;

	if (hChip && hChip->pData)
		wiring = ((SAT_TUNER_Params_t) (hChip->pData))->IQ_Wiring;

	return (S8) wiring;
}

void STV6120_TunerSetReferenceFreq(STCHIP_Handle_t hChip, U32 Reference)
{
	if (hChip && hChip->pData)
		((SAT_TUNER_Params_t) (hChip->pData))->Reference = Reference;
}

U32 STV6120_TunerGetReferenceFreq(STCHIP_Handle_t hChip)
{
	U32 reference = 0;

	if (hChip && hChip->pData)
		reference = ((SAT_TUNER_Params_t) (hChip->pData))->Reference;

	return reference;
}

void STV6120_TunerSetIF_Freq(STCHIP_Handle_t hChip, U32 IF)
{
	if (hChip && hChip->pData)
		((SAT_TUNER_Params_t) (hChip->pData))->IF = IF;
}

U32 STV6120_TunerGetIF_Freq(STCHIP_Handle_t hChip)
{
	U32 ifreq = 0;

	if (hChip && hChip->pData)
		ifreq = ((SAT_TUNER_Params_t) (hChip->pData))->IF;

	return ifreq;
}

void STV6120_TunerSetBandSelect(STCHIP_Handle_t hChip, U8 BandSelect)
{
	if (!hChip || !hChip->pData)
		return;

	if (((SAT_TUNER_Params_t) (hChip->pData))->InputSelect == TUNER_INPUT1)
		((SAT_TUNER_Params_t) (hChip->pData))->BandSelect =
		    (TUNER_WIDEBandS_t) BandSelect;
	else if (((SAT_TUNER_Params_t)(hChip->pData))->InputSelect ==
			TUNER_INPUT2)
		((SAT_TUNER_Params_t) (hChip->pData))->BandSelect2 =
		    (TUNER_WIDEBandS_t) BandSelect;
}

U8 STV6120_TunerGetBandSelect(STCHIP_Handle_t hChip)
{
	U32 ifreq = 0;

	if (hChip && hChip->pData)
		ifreq = ((SAT_TUNER_Params_t) (hChip->pData))->BandSelect;

	return (U8) ifreq;
}

TUNER_Error_t STV6120_TunerTerm(STCHIP_Handle_t hTuner)
{
	TUNER_Error_t error = TUNER_NO_ERR;

	if (hTuner) {
#ifndef CHIP_STAPI
		if (hTuner->pData)
			free(hTuner->pData);

		ChipClose(hTuner);
#endif
	}

	return error;
}

TUNER_Error_t STV6120_TunerSetLNAInput(STCHIP_Handle_t hTuner,
				       TUNER_RFSource_t TunerInput)
{

	TUNER_Error_t error = TUNER_NO_ERR;
	SAT_TUNER_Params_t hTunerParams = NULL;

	if (hTuner) {
		hTunerParams = (SAT_TUNER_Params_t) hTuner->pData;
		if (hTunerParams && !hTuner->Error) {

			switch (TunerInput) {
			case FE_RF_SOURCE_A:
				ChipSetField(hTuner, FSTV6120_LNAAON, 1);
				break;
			case FE_RF_SOURCE_B:
				ChipSetField(hTuner, FSTV6120_LNABON, 1);

				break;
			case FE_RF_SOURCE_C:
				ChipSetField(hTuner, FSTV6120_LNACON, 1);

				break;
			case FE_RF_SOURCE_D:
				ChipSetField(hTuner, FSTV6120_LNADON, 1);
				break;
			}

		}
	} else {
		error = TUNER_INVALID_HANDLE;
	}

	/*************R/W test code*************************
	  STV6120_TunerRead(hTuner);
	  {
	  int i = 0;
	  printf("\n TUNER READ");
	  for (i = 0 ; i < STV6120_NBREGS - 2 ; i++) {
	  printf("\nRegister[%d] = 0x%x",
	  hTuner->pRegMapImage[i].Addr,
	  hTuner->pRegMapImage[i].Value);
	  }
	  }
	 ***************************************************/
	return error;
}
