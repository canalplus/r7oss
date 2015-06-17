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

Source file name :stv6110_tuner.c
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
#include "tuner_wrapper.h"
#include <stm_fe_demod.h>
#include <fesat_commlla_str.h>
#include "fe_sat_tuner.h"
#include "stv6110_tuner.h"
#include <gen_macros.h>

	/* STV6110 tuner definition */
/*CTRL1*/
#define RSTV6110_CTRL1  0x0000
#define FSTV6110_K  0x000000f8
#define FSTV6110_LPT  0x00000004
#define FSTV6110_RX  0x00000002
#define FSTV6110_SYN  0x00000001

/*CTRL2*/
#define RSTV6110_CTRL2  0x0001
#define FSTV6110_CO_DIV  0x000100c0
#define FSTV6110_REFOUTSEL  0x00010010
#define FSTV6110_BBGAIN  0x0001000f

/*TUNING0*/
#define RSTV6110_TUNING0  0x0002
#define FSTV6110_NDIV_LSB  0x000200ff

/*TUNING1*/
#define RSTV6110_TUNING1  0x0003
#define FSTV6110_RDIV  0x000300c0
#define FSTV6110_PRESC32ON  0x00030020
#define FSTV6110_DIV4SEL  0x00030010
#define FSTV6110_NDIV_MSB  0x0003000f

/*CTRL3*/
#define RSTV6110_CTRL3  0x0004
#define FSTV6110_DCLOOP_OFF  0x00040080
#define FSTV6110_RCCLKOFF  0x00040040
#define FSTV6110_ICP  0x00040020
#define FSTV6110_CF  0x0004001f

/*STAT1*/
#define RSTV6110_STAT1  0x0005
#define FSTV6110_TEST1  0x000500f8
#define FSTV6110_CALVCOSTRT  0x00050004
#define FSTV6110_CALRCSTRT  0x00050002
#define FSTV6110_LOCKPLL  0x00050001

/*STAT2*/
#define RSTV6110_STAT2  0x0006
#define FSTV6110_TEST2  0x000600ff

/*STAT3*/
#define RSTV6110_STAT3  0x0007
#define FSTV6110_TEST3  0x000700ff

/*Nbregs and Nbfields moved to stv6110_tuner.h: Change ///AG */

TUNER_Error_t STV6110_TunerInit(void *pTunerInit_v,
				STCHIP_Handle_t *TunerHandle)
{
	SAT_TUNER_Params_t hTunerParams = NULL;
	STCHIP_Info_t ChipInfo;
	STCHIP_Handle_t hTuner = NULL;
	TUNER_Error_t error = TUNER_NO_ERR;
	/*Changed to retain common function prototype for all Tuners: Change
	 * ///AG */
	SAT_TUNER_Params_t pTunerInit = (SAT_TUNER_Params_t) pTunerInit_v;
	STCHIP_Register_t DefSTV6110Val[STV6110_NBREGS] = {
		{RSTV6110_CTRL1, 0x07},
		{RSTV6110_CTRL2, 0x11},
		{RSTV6110_TUNING0, 0xdc},
		{RSTV6110_TUNING1, 0x85},
		{RSTV6110_CTRL3, 0x17},
		{RSTV6110_STAT1, 0x01},
		{RSTV6110_STAT2, 0xe6},
		{RSTV6110_STAT3, 0x1e}
	};

	/*
	 **   REGISTER CONFIGURATION
	 **     ----------------------
	 */
#ifdef HOST_PC
	/* Allocation of the chip structure     */
	hTunerParams = calloc(1, sizeof(SAT_TUNER_InitParams_t));
#endif

#ifdef CHIP_STAPI
	if (*TunerHandle) {
		/*Change ///AG */
		hTunerParams = (SAT_TUNER_Params_t) ((*TunerHandle)->pData);

		/*Copy settings already contained in hTuner to ChipInfo: Change
		 * ///AG */
		memcpy(&ChipInfo, *TunerHandle, sizeof(STCHIP_Info_t));

	} else
		error = TUNER_INVALID_HANDLE;
#endif

	if (hTunerParams != NULL) {

		/* Tuner model */
		hTunerParams->Model = pTunerInit->Model;
		/* Reference Clock in Hz */
		hTunerParams->Reference = pTunerInit->Reference;
		/* IF Hz intermediate frequency */
		hTunerParams->IF = pTunerInit->IF;
		/* hardware IQ invertion */
		hTunerParams->IQ_Wiring = pTunerInit->IQ_Wiring;
		/* Wide band tuner (6130 like, band selection) */
		hTunerParams->BandSelect = pTunerInit->BandSelect;
		/* Demod Model used with this tuner */
		hTunerParams->DemodModel = pTunerInit->DemodModel;

		if (strlen((char *)pTunerInit->TunerName) < MAXNAMESIZE)
			/* Tuner name */
			strcpy((char *)ChipInfo.Name,
						(char *)pTunerInit->TunerName);
		else
			error = TUNER_TYPE_ERR;

		/* Repeater host */
		ChipInfo.RepeaterHost = pTunerInit->RepeaterHost;

		/* Repeater enable/disable function */
		ChipInfo.RepeaterFn = pTunerInit->RepeaterFn;

		/* Tuner need to enable repeater */
		ChipInfo.Repeater = true;
		/* Init tuner I2C address */
		ChipInfo.I2cAddr = pTunerInit->TunerI2cAddress;

		/* Store tunerparams pointer into Chip structure */
		ChipInfo.pData = hTunerParams;

		ChipInfo.NbRegs = STV6110_NBREGS;
		ChipInfo.NbFields = STV6110_NBFIELDS;
		ChipInfo.ChipMode = STCHIP_MODE_SUBADR_8;
		ChipInfo.WrStart = RSTV6110_CTRL1;
		ChipInfo.WrSize = 8;
		ChipInfo.RdStart = RSTV6110_CTRL1;
		ChipInfo.RdSize = 8;
		ChipInfo.Error = 0;

#ifdef HOST_PC			/* Change ///AG */
		hTuner = ChipOpen(&ChipInfo);
		(*TunerHandle) = hTuner;
#endif
#ifdef CHIP_STAPI
		hTuner = *TunerHandle;	/*obtain hTuner : change ///AG */
		/* Copy the ChipInfo to hTuner: Change ///AG */
		memcpy(hTuner, &ChipInfo, sizeof(STCHIP_Info_t));
#endif

		if ((*TunerHandle) == NULL)
			error = TUNER_INVALID_HANDLE;

		if (hTuner != NULL) {
			/*******************************
			 **   CHIP REGISTER MAP IMAGE INITIALIZATION
			 **     ----------------------
			 ********************************/

			ChipUpdateDefaultValues(hTuner, DefSTV6110Val);

			/*******************************
			 **   REGISTER CONFIGURATION
			 **     ----------------------
			 ********************************/

			/*Update the clock divider before registers
			 * initialization */
			/*Allowed values 1,2,4 and 8 */
			switch (pTunerInit->OutputClockDivider) {
			case 1:
			default:
				ChipSetFieldImage(hTuner, FSTV6110_CO_DIV, 0);
				break;

			case 2:
				ChipSetFieldImage(hTuner, FSTV6110_CO_DIV, 1);
				break;

			case 4:
				ChipSetFieldImage(hTuner, FSTV6110_CO_DIV, 2);
				break;

			case 8:
			case 0:
				/*Tuner output clock not used then divide by 8
				 * (the 6110 can not stop completely the out
				 * clock) */
				ChipSetFieldImage(hTuner, FSTV6110_CO_DIV, 3);
				break;
			}

			/*I2C registers initialization */

			error = STV6110_TunerWrite(hTuner);
			if (hTuner->Error != CHIPERR_NO_ERROR) {
				error = TUNER_I2C_NO_ACK;
				return error;
			}
			hTuner->Error = 0;
		}

	} else
		error = TUNER_INVALID_HANDLE;

	return error;
}

TUNER_Error_t STV6110_TunerSetStandby(STCHIP_Handle_t hTuner, U8 StandbyOn)
{
	TUNER_Error_t error = TUNER_NO_ERR;
	SAT_TUNER_Params_t hTunerParams = NULL;

	if (hTuner == NULL)
		return TUNER_INVALID_HANDLE;

	hTunerParams = (SAT_TUNER_Params_t) hTuner->pData;

	if (hTuner && hTunerParams && !hTuner->Error) {
		if (StandbyOn) {	/*Power down ON */
			ChipSetFieldImage(hTuner, FSTV6110_SYN, 0);
			ChipSetFieldImage(hTuner, FSTV6110_RX, 0);
			ChipSetFieldImage(hTuner, FSTV6110_LPT, 0);
		} else {	/*Power down OFF */

			ChipSetFieldImage(hTuner, FSTV6110_SYN, 1);
			ChipSetFieldImage(hTuner, FSTV6110_RX, 1);
			ChipSetFieldImage(hTuner, FSTV6110_LPT, 1);
		}
		error = STV6110_TunerWrite(hTuner);

	}
	return error;
}

TUNER_Error_t STV6110_TunerSetFrequency(STCHIP_Handle_t hTuner, U32 Frequency)
{
	TUNER_Error_t error = TUNER_NO_ERR;
	SAT_TUNER_Params_t hTunerParams = NULL;

	/*Deleted unused var 'frequency' to remove warnings: Change ///AG */
	U32 divider;
	U32 p, Presc, rDiv, r;

	S32 rDivOpt = 0, pCalcOpt = 1000, pVal, pCalc, i;

	/*Coverity CID 15804 fix */
	if (hTuner)
		hTunerParams = (SAT_TUNER_Params_t) hTuner->pData;
	else
		error = TUNER_INVALID_HANDLE;

	if (hTunerParams && !hTuner->Error) {

		ChipSetField(hTuner, FSTV6110_K,
			     ((hTunerParams->Reference / 1000000) - 16));
		if (Frequency <= 1023000) {
			p = 1;
			Presc = 0;
		} else if (Frequency <= 1300000) {
			p = 1;
			Presc = 1;
		} else if (Frequency <= 2046000) {
			p = 0;
			Presc = 0;
		} else {
			p = 0;
			Presc = 1;
		}
		ChipSetField(hTuner, FSTV6110_DIV4SEL, p);
		ChipSetField(hTuner, FSTV6110_PRESC32ON, Presc);

		pVal = (int)PowOf2(p + 1) * 10;
		for (rDiv = 0; rDiv <= 3; rDiv++) {
			pCalc =
			    (hTunerParams->Reference / 100000) /
			    (PowOf2(rDiv + 1));

			if ((ABS((S32) (pCalc - pVal))) <
			    (ABS((S32) (pCalcOpt - pVal))))
				rDivOpt = rDiv;

			pCalcOpt =
			    (hTunerParams->Reference / 100000) /
			    (PowOf2(rDivOpt + 1));

		}

		r = PowOf2(rDivOpt + 1);
		divider =
		    (Frequency * r * PowOf2(p + 1) * 10) /
		    (hTunerParams->Reference / 1000);
		divider = (divider + 5) / 10;

		ChipSetField(hTuner, FSTV6110_RDIV, rDivOpt);
		ChipSetField(hTuner, FSTV6110_NDIV_MSB, MSB(divider));
		ChipSetField(hTuner, FSTV6110_NDIV_LSB, LSB(divider));

		/* VCO Auto Calibration */
		ChipSetField(hTuner, FSTV6110_CALVCOSTRT, 1);

		i = 0;
		while ((i < 10)
		       && (ChipGetField(hTuner, FSTV6110_CALVCOSTRT) != 0)) {
			/* wait for VCO auto calibration */
			ChipWaitOrAbort(hTuner, 1);
			i++;
		}

	}

	return error;
}

U32 STV6110_TunerGetFrequency(STCHIP_Handle_t hTuner)
{
	SAT_TUNER_Params_t hTunerParams = NULL;
	U32 frequency = 0;
	/*Deleted unused variable 'stepsize' to remove warnings: Change ///AG */
	U32 nbsteps;
	U32 divider = 0;
	/*Deleted unused variable 'swallow' to remove warnings: Change ///AG */
	U32 psd2;

	if (hTuner == NULL)
		return TUNER_INVALID_HANDLE;

	hTunerParams = (SAT_TUNER_Params_t) hTuner->pData;

	if (hTuner && hTunerParams && !hTuner->Error) {

		divider =
		    MAKEWORD(ChipGetField(hTuner, FSTV6110_NDIV_MSB),
			     ChipGetField(hTuner, FSTV6110_NDIV_LSB));
		nbsteps = ChipGetFieldImage(hTuner, FSTV6110_RDIV); /*Rdiv */
		psd2 = ChipGetFieldImage(hTuner, FSTV6110_DIV4SEL);	/*p */

		frequency = divider * (hTunerParams->Reference / 1000);
		divider = PowOf2(nbsteps + psd2);
		if (divider > 0)
			frequency = frequency / divider;
		frequency /= 4;

	}
	return frequency;
}

TUNER_Error_t STV6110_TunerSetBandwidth(STCHIP_Handle_t hTuner, U32 Bandwidth)
{
	TUNER_Error_t error = TUNER_NO_ERR;
	SAT_TUNER_Params_t hTunerParams = NULL;
	U8 u8;
	/*Deleted unused variable 'filter' to remove warnings: Change ///AG */
	S32 i = 0;

	if (hTuner == NULL)
		return TUNER_INVALID_HANDLE;

	hTunerParams = (SAT_TUNER_Params_t) hTuner->pData;

	if (hTuner && hTunerParams && !hTuner->Error) {

		if ((Bandwidth / 2) > 36000000)
			/*F[4:0] BW/2 max =31+5=36 mhz for F=31 */
			u8 = 31;
		else if ((Bandwidth / 2) < 5000000)
			/* BW/2 min = 5Mhz for F=0 */
			u8 = 0;
		else		/*if 5 < BW/2 < 36 */
			u8 = (Bandwidth / 2) / 1000000 - 5;

		/* Activate the calibration Clock */
		ChipSetField(hTuner, FSTV6110_RCCLKOFF, 0);
		ChipSetField(hTuner, FSTV6110_CF, u8);	/* Set the LPF value */
		/* Start LPF auto calibration */
		ChipSetField(hTuner, FSTV6110_CALRCSTRT, 1);

		i = 0;
		while ((i < 10)
		       && (ChipGetField(hTuner, FSTV6110_CALRCSTRT) != 0)) {
			/* wait for LPF auto calibration */
			ChipWaitOrAbort(hTuner, 1);
			i++;
		}
		/* calibration done, desactivate the calibration Clock */
		ChipSetField(hTuner, FSTV6110_RCCLKOFF, 1);

	}

	return error;
}

U32 STV6110_TunerGetBandwidth(STCHIP_Handle_t hTuner)
{
	SAT_TUNER_Params_t hTunerParams = NULL;
	U32 bandwidth = 0;
	U8 u8 = 0;

	if (hTuner == NULL)
		return TUNER_INVALID_HANDLE;

	hTunerParams = (SAT_TUNER_Params_t) hTuner->pData;

	if (hTuner && hTunerParams && !hTuner->Error) {

		u8 = ChipGetField(hTuner, FSTV6110_CF);

		/* x2 for ZIF tuner BW/2=F+5 Mhz */
		bandwidth = (u8 + 5) * 2000000;

	}

	return bandwidth;
}

TUNER_Error_t STV6110_TunerSetGain(STCHIP_Handle_t hTuner, S32 Gain)
{
	TUNER_Error_t error = TUNER_NO_ERR;
	SAT_TUNER_Params_t hTunerParams = NULL;

	if (hTuner == NULL)
		return TUNER_INVALID_HANDLE;

	hTunerParams = (SAT_TUNER_Params_t) hTuner->pData;

	if (hTuner && hTunerParams && !hTuner->Error)
		ChipSetField(hTuner, FSTV6110_BBGAIN, (Gain / 2));

	return error;
}

S32 STV6110_TunerGetGain(STCHIP_Handle_t hTuner)
{
	SAT_TUNER_Params_t hTunerParams = NULL;
	S32 gain = 0;

	if (hTuner == NULL)
		return TUNER_INVALID_HANDLE;

	hTunerParams = (SAT_TUNER_Params_t) hTuner->pData;

	if (hTuner && hTunerParams && !hTuner->Error)
		gain = 2 * ChipGetField(hTuner, FSTV6110_BBGAIN);

	return gain;
}

TUNER_Error_t STV6110_TunerSetOutputClock(STCHIP_Handle_t hTuner, S32 Divider)
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

	if (hTuner == NULL)
		return TUNER_INVALID_HANDLE;

	hTunerParams = (SAT_TUNER_Params_t) hTuner->pData;

	if (hTuner && hTunerParams && !hTuner->Error) {

		/*Allowed values 1,2,4 and 8 */
		switch (Divider) {
		case 1:
		default:
			ChipSetField(hTuner, FSTV6110_CO_DIV, 0);
			break;

		case 2:
			ChipSetField(hTuner, FSTV6110_CO_DIV, 1);
			break;

		case 4:
			ChipSetField(hTuner, FSTV6110_CO_DIV, 2);
			break;

		case 8:
		case 0:
			/*Tuner output clock not used then divide by 8 (the
			 * 6110 can not stop completely the out clock) */
			ChipSetField(hTuner, FSTV6110_CO_DIV, 3);
			break;
		}

	}

	return error;
}

TUNER_Error_t STV6110_TunerSetAttenuator(STCHIP_Handle_t hTuner,
					 BOOL AttenuatorOn)
{
	return TUNER_NO_ERR;
}

BOOL STV6110_TunerGetAttenuator(STCHIP_Handle_t hTuner)
{
	return FALSE;
}

BOOL STV6110_TunerGetStatus(STCHIP_Handle_t hTuner)
{
	/*Deleted unused variable 'error' to remove warnings: Change ///AG */
	SAT_TUNER_Params_t hTunerParams = NULL;
	BOOL locked = FALSE;
	/*Deleted unused variable 'u8' to remove warnings: Change ///AG */

	if (hTuner == NULL)
		return TUNER_INVALID_HANDLE;

	hTunerParams = (SAT_TUNER_Params_t) hTuner->pData;

	if (hTuner && hTunerParams && !hTuner->Error) {

		if (!hTuner->Error)
			locked = ChipGetField(hTuner, FSTV6110_LOCKPLL);

	}

	return locked;
}

TUNER_Error_t STV6110_TunerWrite(STCHIP_Handle_t hTuner)
{
	TUNER_Error_t error = TUNER_NO_ERR;
	STCHIP_Error_t chipError = CHIPERR_NO_ERROR;
	SAT_TUNER_Params_t hTunerParams = NULL;
	U8 i = 0;

	if (hTuner == NULL)
		return TUNER_INVALID_HANDLE;

	hTunerParams = (SAT_TUNER_Params_t) hTuner->pData;

	if (hTuner && hTunerParams) {
		if ((hTunerParams->DemodModel == DEMOD_STI7111)
		    && (hTuner->RepeaterHost->ChipID < 0x20)) {
			for (i = hTuner->WrStart; i < hTuner->WrSize; i++)
				/*Write one register at a time for 7111 cut1.0*/
				chipError |= ChipSetRegisters(hTuner, i, 1);
		} else {
			chipError =
			    ChipSetRegisters(hTuner, hTuner->WrStart, 8);
		}

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

TUNER_Error_t STV6110_TunerRead(STCHIP_Handle_t hTuner)
{
	TUNER_Error_t error = TUNER_NO_ERR;
	STCHIP_Error_t chipError = CHIPERR_NO_ERROR;
	SAT_TUNER_Params_t hTunerParams = NULL;
	U8 i = 0;

	if (hTuner == NULL)
		return TUNER_INVALID_HANDLE;

	hTunerParams = (SAT_TUNER_Params_t) hTuner->pData;

	if (hTuner && hTunerParams) {
		if ((hTunerParams->DemodModel == DEMOD_STI7111)
		    && (hTuner->RepeaterHost->ChipID < 0x20)) {
			for (i = hTuner->RdStart; i < hTuner->RdSize; i++)
				/*Read one register at a time for cut1.0 */
				chipError |= ChipGetRegisters(hTuner, i, 1);
		} else
			chipError =
			    ChipGetRegisters(hTuner, hTuner->RdStart,
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

#if 0

TUNER_Model_t STV6110_TunerGetModel(STCHIP_Handle_t hChip)
{
	TUNER_Model_t model = TUNER_NULL;

	if (hChip && hChip->pData)
		model = ((SAT_TUNER_Params_t) (hChip->pData))->Model;

	return model;
}

void STV6110_TunerSetIQ_Wiring(STCHIP_Handle_t hChip, S8 IQ_Wiring)
{				/*To make generic prototypes Change ///AG */
	if (hChip && hChip->pData) {
		((SAT_TUNER_Params_t) (hChip->pData))->IQ_Wiring =
		    (TUNER_IQ_t) IQ_Wiring;
	}
}

S8 STV6110_TunerGetIQ_Wiring(STCHIP_Handle_t hChip)
{				/*To make generic prototypes Change ///AG */
	TUNER_IQ_t wiring = TUNER_IQ_NORMAL;	/*Change ///AG */

	if (hChip && hChip->pData)
		wiring = ((SAT_TUNER_Params_t) (hChip->pData))->IQ_Wiring;

	return (S8) wiring;
}
#endif
void STV6110_TunerSetReferenceFreq(STCHIP_Handle_t hChip, U32 Reference)
{

	if (hChip && hChip->pData)
		((SAT_TUNER_Params_t) (hChip->pData))->Reference = Reference;
}

#if 0
U32 STV6110_TunerGetReferenceFreq(STCHIP_Handle_t hChip)
{
	U32 reference = 0;

	if (hChip && hChip->pData)
		reference = ((SAT_TUNER_Params_t) (hChip->pData))->Reference;

	return reference;
}

void STV6110_TunerSetIF_Freq(STCHIP_Handle_t hChip, U32 IF)
{
	if (hChip && hChip->pData)
		((SAT_TUNER_Params_t) (hChip->pData))->IF = IF;
}

U32 STV6110_TunerGetIF_Freq(STCHIP_Handle_t hChip)
{
	U32 ifreq = 0;

	if (hChip && hChip->pData)
		ifreq = ((SAT_TUNER_Params_t) (hChip->pData))->IF;

	return ifreq;
}

void STV6110_TunerSetBandSelect(STCHIP_Handle_t hChip, U8 BandSelect)
{
	if (hChip && hChip->pData) {
		((SAT_TUNER_Params_t) (hChip->pData))->BandSelect =
		    (TUNER_WIDEBandS_t) BandSelect;
	}
}

U8 STV6110_TunerGetBandSelect(STCHIP_Handle_t hChip)
{
	U32 ifreq = 0;

	if (hChip && hChip->pData)
		ifreq = ((SAT_TUNER_Params_t) (hChip->pData))->BandSelect;

	return (U8) ifreq;
}
#endif
TUNER_Error_t STV6110_TunerTerm(STCHIP_Handle_t hTuner)
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
