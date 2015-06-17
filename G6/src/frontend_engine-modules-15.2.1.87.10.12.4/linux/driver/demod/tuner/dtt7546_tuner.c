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

Source file name :dtt7546_tuner.c
Author :           LLA

tuner lla file

Date        Modification                                    Name
----        ------------                                    --------
01-Aug-11   Created                                         TA

************************************************************************/

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/stm/plat_dev.h>
#include <linux/stm/demod.h>
#include <stm_registry.h>
#include <stm_fe.h>
#include <stfe_utilities.h>
#include <stm_fe_os.h>
#include <i2c_wrapper.h>
#include <stm_fe_demod.h>
#include <fesat_commlla_str.h>
#include "fe_tc_tuner.h"
#include "dtt7546_tuner.h"
#include <gen_macros.h>

/*Thomson DTT 7546X tuner definition*/
	/*      DB1     */
#define RDTT7546_DB1		0x0000
#define FDTT7546_ZERO		0x00000080
#define FDTT7546_N_MSB	0x0000007f

	/*      DB2     */
#define RDTT7546_DB2		0x0001
#define FDTT7546_N_LSB	0x000100ff

	/*      CB1     */
#define RDTT7546_CB1		0x0002
#define FDTT7546_ONE		0x00020080
#define FDTT7546_Z		  0x00020040
#define FDTT7546_ATP		0x00020038
#define FDTT7546_RS		  0x00020007

	/*      BB      */
#define RDTT7546_BB		    0x0003
#define	FDTT7546_CP			0x000300C0
#define FDTT7546_ZO		  0x00030020
#define FDTT7546_P5			0x00030010
#define FDTT7546_BS			0x0003000f

	/*      CB2     */
#define RDTT7546_CB2		0x0004
#define FDTT7546_ONE2		0x000400C0
#define FDTT7546_ATC		0x00040020
#define FDTT7546_CONST	0x0004001e
#define FDTT7546_XTO		0x00040001

	/*      SB      */
#define RDTT7546_SB		0x0005
#define FDTT7546_POR		0x00050080
#define FDTT7546_FL			0x00050040
#define FDTT7546_CST		0x00050038
#define FDTT7546_A			0x00050007

STCHIP_Register_t DefDTT7546ValT[DTT7546_NBREGS] = {
	{RDTT7546_DB1, 0x0B},
	{RDTT7546_DB2, 0xF5},
	{RDTT7546_CB1, 0x88},
	{RDTT7546_BB, 0x08},
	{RDTT7546_CB2, 0xC3},
	{RDTT7546_SB, 0x00}

};

STCHIP_Register_t DefDTT7546ValC[DTT7546_NBREGS] = {
	{RDTT7546_DB1, 0x0E},
	{RDTT7546_DB2, 0xC0},
	{RDTT7546_CB1, 0x93},
	{RDTT7546_BB, 0x06},
	{RDTT7546_CB2, 0xE3},
	{RDTT7546_SB, 0x00}
};

TUNER_Error_t DTT7546_TunerInit(void *pTunerInit_v,
				STCHIP_Handle_t *TunerHandle)
{
	TUNER_Params_t hTunerParams = NULL;
	STCHIP_Info_t ChipInfo;
	STCHIP_Handle_t hTuner = NULL;
	TUNER_Error_t error = TUNER_NO_ERR;
	TUNER_Params_t pTunerInit = (TUNER_Params_t) pTunerInit_v;

	/*
	 **   REGISTER CONFIGURATION
	 **     ----------------------
	 */
#ifdef HOST_PC
	/* Allocation of the chip structure     */
	hTunerParams = calloc(1, sizeof(TUNER_InitParams_t));
#endif
#ifdef CHIP_STAPI
	/* fix PJ 11/2009 coverity */
	if (*TunerHandle) {
		hTunerParams = (TUNER_Params_t) ((*TunerHandle)->pData);
		memcpy(&ChipInfo, *TunerHandle, sizeof(STCHIP_Info_t));
	}
#endif

	if (hTunerParams) {
		hTunerParams->Model = pTunerInit->Model; /* Tuner model */
		/* Tuner name */
		strncpy((char *)ChipInfo.Name,
				(char *)pTunerInit->TunerName, MAXNAMESIZE - 1);
		/* Repeater host */
		ChipInfo.RepeaterHost = pTunerInit->RepeaterHost;
		/* Tuner has no embedded repeater */
		ChipInfo.Repeater = TRUE;
		ChipInfo.RepeaterFn = pTunerInit->RepeaterFn;	/* see above */
		/* Init tuner I2C address */
		ChipInfo.I2cAddr = pTunerInit->TunerI2cAddress;
		/* Store tunerparams pointer into Chip structure */
		ChipInfo.pData = hTunerParams;

		/* 166,667 Khz StepSize */
		hTunerParams->StepSize = 166667;
		hTunerParams->Fxtal_Hz = 0;	/* Not used */
		/* 36 MHz intermediate frequency */
		hTunerParams->IF = 36000;
		hTunerParams->BandWidth = 8;
		hTunerParams->BandSelect = Band_UHF;
		/* No hardware IQ invertion */
		hTunerParams->IQ_Wiring = TUNER_IQ_NORMAL;
		hTunerParams->TransmitStandard = pTunerInit->TransmitStandard;
		hTunerParams->DemodModel = pTunerInit->DemodModel;
		/* Tuner invert IF or IQ spectrum */
		hTunerParams->SpectrInvert = TUNER_IQ_NORMAL;
		/* fill elements of external chip data structure */
		ChipInfo.NbRegs = DTT7546_NBREGS;
		ChipInfo.NbFields = DTT7546_NBFIELDS;
		ChipInfo.ChipMode = STCHIP_MODE_NOSUBADR;
		ChipInfo.WrStart = RDTT7546_DB1;
		ChipInfo.WrSize = 5;
		ChipInfo.RdStart = RDTT7546_SB;
		ChipInfo.RdSize = 1;
#ifdef HOST_PC
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

		if (hTuner) {
			if (hTunerParams->TransmitStandard == FE_DVB_T)
				/*init in ter by default */
				ChipUpdateDefaultValues(hTuner, DefDTT7546ValT);
			else if (hTunerParams->TransmitStandard == FE_DVB_C)
				ChipUpdateDefaultValues(hTuner, DefDTT7546ValC);
			else
				/*should not pass here */
				error = TUNER_TYPE_ERR;
		}
#if defined(CHIP_STAPI) || defined(NO_GUI)
		error = DTT7546_TunerWrite(hTuner);
#endif
	} else
		error = TUNER_INVALID_HANDLE;
	return error;
}

TUNER_Error_t DTT7546_TunerSetFrequency(STCHIP_Handle_t hTuner, U32 Frequency)
{
	TUNER_Error_t error = TUNER_NO_ERR;
	TUNER_Params_t hTunerParams = NULL;
	U32 frequency;
	U32 stepsize = 0;
	U32 divider;

	/* fix PJ 11/2009 coverity */
	if (hTuner)
		hTunerParams = (TUNER_Params_t) hTuner->pData;

	if (hTunerParams) {
		/* This driver was for Thomson SGP, This is the normal driver */
		switch (ChipGetFieldImage(hTuner, FDTT7546_RS)) {
		case 0:
			stepsize = 166667;
			break;
		case 1:
			stepsize = 142857;
			break;
		case 2:
			stepsize = 80000;
			break;
		case 3:
			stepsize = 62500;
			break;
		case 4:
			stepsize = 31250;
			break;
		default:
			stepsize = 50000;
			break;
		}
		frequency = Frequency + DTT7546_TunerGetIF_Freq(hTuner);
		divider = (frequency * 100) / (stepsize / 10);

		if (frequency <= 182000) {
			ChipSetFieldImage(hTuner, FDTT7546_BS, 0x05);
			if (frequency <= 121000)
				ChipSetFieldImage(hTuner, FDTT7546_CP, 0x00);
			else if (frequency <= 141000)
				ChipSetFieldImage(hTuner, FDTT7546_CP, 0x01);
			else if (frequency <= 166000)
				ChipSetFieldImage(hTuner, FDTT7546_CP, 0x02);
			else
				ChipSetFieldImage(hTuner, FDTT7546_CP, 0x03);
		} else if (frequency <= 466000) {
			ChipSetFieldImage(hTuner, FDTT7546_BS, 0x06);
			if (frequency <= 286000)
				ChipSetFieldImage(hTuner, FDTT7546_CP, 0x00);
			else if (frequency <= 386000)
				ChipSetFieldImage(hTuner, FDTT7546_CP, 0x01);
			else if (frequency <= 446000)
				ChipSetFieldImage(hTuner, FDTT7546_CP, 0x02);
			else
				ChipSetFieldImage(hTuner, FDTT7546_CP, 0x03);
		} else {
			ChipSetFieldImage(hTuner, FDTT7546_BS, 0x0C);
			if (frequency <= 506000)
				ChipSetFieldImage(hTuner, FDTT7546_CP, 0x00);
			else if (frequency <= 761000)
				ChipSetFieldImage(hTuner, FDTT7546_CP, 0x01);
			else if (frequency <= 846000)
				ChipSetFieldImage(hTuner, FDTT7546_CP, 0x02);
			else
				ChipSetFieldImage(hTuner, FDTT7546_CP, 0x03);
		}

		ChipSetFieldImage(hTuner, FDTT7546_ATC, 0x01);

		ChipSetFieldImage(hTuner, FDTT7546_N_MSB,
				  ((divider >> 8) & 0x7F));
		ChipSetFieldImage(hTuner, FDTT7546_N_LSB, ((divider) & 0xFF));

		error = DTT7546_TunerWrite(hTuner);
	} else
		error = TUNER_INVALID_HANDLE;

	return error;
}

U32 DTT7546_TunerGetFrequency(STCHIP_Handle_t hTuner)
{
	TUNER_Params_t hTunerParams = NULL;
	U32 frequency = 0;
	U32 stepsize = 0;
	U32 divider;

	/* fix PJ 11/2009 coverity */
	if (hTuner)
		hTunerParams = (TUNER_Params_t) hTuner->pData;
	if (hTunerParams) {

		if (ChipGetFieldImage(hTuner, FDTT7546_RS) == 0x00)
			stepsize = 166667;
		else if (ChipGetFieldImage(hTuner, FDTT7546_RS) == 0x01)
			stepsize = 142857;
		else if (ChipGetFieldImage(hTuner, FDTT7546_RS) == 0x02)
			stepsize = 80000;
		else if (ChipGetFieldImage(hTuner, FDTT7546_RS) == 0x03)
			stepsize = 62500;
		else if (ChipGetFieldImage(hTuner, FDTT7546_RS) == 0x04)
			stepsize = 31250;
		else
			stepsize = 50000;

		divider = (ChipGetFieldImage(hTuner, FDTT7546_N_MSB) << 8)
		    + (ChipGetFieldImage(hTuner, FDTT7546_N_LSB));

		frequency =
		    (divider * stepsize) / 1000 -
		    DTT7546_TunerGetIF_Freq(hTuner);
	}

	return frequency;
}

TUNER_Error_t DTT7546_TunerSetStepsize(STCHIP_Handle_t hTuner, U32 Stepsize)
{
	TUNER_Params_t hTunerParams = NULL;
	TUNER_Error_t error = TUNER_NO_ERR;

	if (hTuner)
		hTunerParams = (TUNER_Params_t) hTuner->pData;
	if (hTunerParams)
		error = ChipSetFieldImage(hTuner, FDTT7546_RS, Stepsize);
	else
		error = TUNER_INVALID_HANDLE;

	return error;		/*TunerSetStepsize */
}

U32 DTT7546_TunerGetStepsize(STCHIP_Handle_t hTuner)
{

	TUNER_Params_t hTunerParams = NULL;
	U32 Stepsize = 0;
	if (hTuner)
		hTunerParams = (TUNER_Params_t) hTuner->pData;
	if (hTunerParams)
		Stepsize = ChipGetFieldImage(hTuner, FDTT7546_RS);

	return Stepsize;	/*TunerGetStepsize */
}

BOOL DTT7546_TunerGetStatus(STCHIP_Handle_t hTuner)
{
	TUNER_Error_t error = TUNER_NO_ERR;
	TUNER_Params_t hTunerParams = NULL;
	BOOL locked = FALSE;
	U8 wait;

	/* fix PJ 11/2009 coverity */
	if (hTuner)
		hTunerParams = (TUNER_Params_t) hTuner->pData;

	if (hTunerParams) {
		wait = 0;
		do {
			error = DTT7546_TunerRead(hTuner);
			WAIT_N_MS(5);
			wait += 5;
		} while ((ChipGetFieldImage(hTuner, FDTT7546_FL) !=
			0x01) & (wait < 120));

		if (ChipGetFieldImage(hTuner, FDTT7546_FL) == 0x01) {
			locked = TRUE;
			ChipSetFieldImage(hTuner, FDTT7546_ATC, 0x00);
			error = DTT7546_TunerWrite(hTuner);
		}
	}

	return locked;
}

TUNER_Error_t DTT7546_TunerWrite(STCHIP_Handle_t hTuner)
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
	if (hTunerParams)
		error =
		    (TUNER_Error_t) ChipSetRegisters(hTuner, hTuner->WrStart,
				    hTuner->WrSize);
	else
		error = TUNER_INVALID_HANDLE;

	return error;
}

TUNER_Error_t DTT7546_TunerRead(STCHIP_Handle_t hTuner)
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
	if (hTunerParams)
		error =
		    (TUNER_Error_t) ChipGetRegisters(hTuner, hTuner->RdStart,
				    hTuner->RdSize);
	else
		error = TUNER_INVALID_HANDLE;

	return error;
}

TUNER_Model_t DTT7546_TunerGetModelName(STCHIP_Handle_t hTuner)
{
	TUNER_Model_t model = TUNER_NULL;
	if (hTuner && hTuner->pData)
		model = ((TUNER_Params_t) (hTuner->pData))->Model;

	return model;
}

void DTT7546_TunerSetIQ_Wiring(STCHIP_Handle_t hTuner, TUNER_IQ_t IQ_Wiring)
{
	if (hTuner && hTuner->pData)
		((TUNER_Params_t) (hTuner->pData))->IQ_Wiring = IQ_Wiring;
}

TUNER_IQ_t DTT7546_TunerGetIQ_Wiring(STCHIP_Handle_t hTuner)
{
	TUNER_IQ_t wiring = TUNER_IQ_NORMAL;

	if (hTuner && hTuner->pData)
		wiring = ((TUNER_Params_t) (hTuner->pData))->IQ_Wiring;

	return (TUNER_IQ_t) wiring;
}

void DTT7546_TunerSetReferenceFreq(STCHIP_Handle_t hTuner, U32 Reference)
{
	if (hTuner && hTuner->pData)
		((TUNER_Params_t) (hTuner->pData))->Fxtal_Hz = Reference;
}

U32 DTT7546_TunerGetReferenceFreq(STCHIP_Handle_t hTuner)
{
	U32 reference = 0;

	if (hTuner && hTuner->pData)
		reference = ((TUNER_Params_t) (hTuner->pData))->Fxtal_Hz;

	return reference;
}

void DTT7546_TunerSetIF_Freq(STCHIP_Handle_t hTuner, U32 IF)
{
	if (hTuner && hTuner->pData)
		((TUNER_Params_t) (hTuner->pData))->IF = IF;
}

U32 DTT7546_TunerGetIF_Freq(STCHIP_Handle_t hTuner)
{
	U32 ifreq = 0;

	if (hTuner && hTuner->pData)
		ifreq = ((TUNER_Params_t) (hTuner->pData))->IF;

	return ifreq;
}

TUNER_Error_t DTT7546_TunerTerm(STCHIP_Handle_t hTuner)
{
	TUNER_Error_t error = TUNER_NO_ERR;

	if (hTuner) {
#ifndef CHIP_STAPI
		if (hTuner->pData)
			free(hTuner->pData);
		ChipClose(hTuner);
#endif
	} else
		error = TUNER_INVALID_HANDLE;

	return error;
}

U32 DTT7546_TunerGetATC(STCHIP_Handle_t hTuner)
{
	if (hTuner)
		/*BANDS indicator in pnl_tuner.c */
		return ChipGetFieldImage(hTuner, FDTT7546_BS);
	else
		return 0;

}

TUNER_Error_t DTT7546_TunerSetATC(STCHIP_Handle_t hTuner, U32 atc)
{
	TUNER_Error_t error = TUNER_NO_ERR;

	if (hTuner)
		/*BANDS indicator in pnl_tuner.c */
		error = ChipSetFieldImage(hTuner, FDTT7546_BS, atc);

	return error;
}

U32 DTT7546_TunerGetCP(STCHIP_Handle_t hTuner)
{
	if (hTuner)
		return ChipGetFieldImage(hTuner, FDTT7546_CP);
	else
		return 0;

}

TUNER_Error_t DTT7546_TunerSetCP(STCHIP_Handle_t hTuner, U32 Cp)
{
	TUNER_Error_t error = TUNER_NO_ERR;

	if (hTuner)
		error = ChipSetFieldImage(hTuner, FDTT7546_CP, Cp);
	else
		error = TUNER_INVALID_HANDLE;

	return error;
}

U32 DTT7546_TunerGetTOP(STCHIP_Handle_t hTuner)
{
	if (hTuner)
		return ChipGetFieldImage(hTuner, FDTT7546_ATP);
	else
		return 0;
}

TUNER_Error_t DTT7546_TunerSetTOP(STCHIP_Handle_t hTuner, U32 top)
{
	TUNER_Error_t error = TUNER_NO_ERR;

	if (hTuner)
		error = ChipSetFieldImage(hTuner, FDTT7546_ATP, top);
	else
		error = TUNER_INVALID_HANDLE;

	return error;
}

U32 DTT7546_TunerGetLNAConfig(STCHIP_Handle_t hChip)
{
	U32 LNA_Config = 0;

	if (hChip && hChip->pData)
		LNA_Config = ((TUNER_Params_t) (hChip->pData))->LNAConfig;

	return LNA_Config;
}

void DTT7546_TunerSetLNAConfig(STCHIP_Handle_t hChip, U32 LNA_Config)
{
	if (hChip && hChip->pData)
		((TUNER_Params_t) (hChip->pData))->LNAConfig = LNA_Config;
}

U32 DTT7546_TunerGetUHFSens(STCHIP_Handle_t hChip)
{
	U32 UHF_Sens = 0;

	if (hChip && hChip->pData)
		UHF_Sens = ((TUNER_Params_t) (hChip->pData))->UHFSens;

	return (U32) UHF_Sens;
}

void DTT7546_TunerSetUHFSens(STCHIP_Handle_t hChip, U32 UHF_Sens)
{
	if (hChip && hChip->pData)
		((TUNER_Params_t) (hChip->pData))->UHFSens = UHF_Sens;
}

U32 DTT7546_TunerGetBandWidth(STCHIP_Handle_t hChip)
{
	U32 Band_Width = 0;
	if (hChip && hChip->pData) {
		/*Band_Width = ((TUNER_Params_t) (hChip->pData))->BandWidth; */
		Band_Width = ChipGetFieldImage(hChip->pData, FDTT7546_P5);
	}

	return (U32) Band_Width;
}

TUNER_Error_t DTT7546_TunerSetBandWidth(STCHIP_Handle_t hChip, U32 Band_Width)
{
	TUNER_Error_t error = TUNER_NO_ERR;
	if (hChip && hChip->pData)
		error =
		    ChipSetFieldImage(hChip->pData, FDTT7546_P5, Band_Width);
	else
		error = TUNER_INVALID_HANDLE;

	return error;

}

TUNER_Error_t DTT7546_SwitchTunerToDVBT(STCHIP_Handle_t hChip)
{
	TUNER_Error_t error = TUNER_NO_ERR;

	if (hChip != NULL) {
		error = ChipUpdateDefaultValues(hChip, DefDTT7546ValT);
#if defined(CHIP_STAPI) || defined(NO_GUI)
		error = DTT7546_TunerWrite(hChip);
#endif

	} else
		error = CHIPERR_INVALID_HANDLE;

	return error;
}

TUNER_Error_t DTT7546_SwitchTunerToDVBC(STCHIP_Handle_t hChip)
{
	TUNER_Error_t error = TUNER_NO_ERR;

	if (hChip != NULL) {
		error = ChipUpdateDefaultValues(hChip, DefDTT7546ValC);
#if defined(CHIP_STAPI) || defined(NO_GUI)
		error = DTT7546_TunerWrite(hChip);
#endif

	} else
		error = CHIPERR_INVALID_HANDLE;

	return error;
}

/* End of dtt7546_tuner.c */
