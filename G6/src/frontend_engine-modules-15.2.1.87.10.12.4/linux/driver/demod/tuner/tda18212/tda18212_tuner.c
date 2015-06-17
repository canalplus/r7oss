/************************************************************************
Copyright (C) 2013 STMicroelectronics. All Rights Reserved.

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

Source file name : tda18212_tuner.c
Author :



Date        Modification                                    Name
----        ------------                                    --------
************************************************************************/
/**file tda18212_tuner.c
*/
/* ----------------------------------------------------------------------------
File Name: xxx_Tuner.c

Description: Tuner driver

Last Modification: 26/07/2010

author: TA
---------------------------------------------------------------------------- */

/* #ifndef ST_OSLINUX */
/* #include "string.h" */
/* #include "stdlib.h" */
/* #endif */
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
#include "tda18212_tuner.h"
#include <gen_macros.h>

/* #include "globaldefs.h" */
#include "tda18212_tuner.h"

#include "tmnxtypes.h"
#include "tmcompid.h"
#include "tmfrontend.h"
#include "tmbslfrontendtypes.h"
#include "tmsysfrontendtypes.h"
#include "tmsysscanxpress.h"
#include "tmbsltda18212.h"
#include "tmbsltda18212local.h"
#include "tmbsltda18212instance.h"

static U32 TDA18212TunerHandle[4];

/*------------------------------------------------------------------------*/
/* Prototype of function to be provided by customer */
/*------------------------------------------------------------------------*/
static tmErrorCode_t Tda18212_UserWrittenI2CRead(tmUnitSelect_t tUnit,
		UInt32 AddrSize, UInt8 * pAddr, UInt32 ReadLen, UInt8 * pData);
static tmErrorCode_t Tda18212_UserWrittenI2CWrite(tmUnitSelect_t tUnit,
					   UInt32 AddrSize, UInt8 *pAddr,
					   UInt32 WriteLen, UInt8 *pData);
static tmErrorCode_t Tda18212_UserWrittenWait(tmUnitSelect_t tUnit, UInt32 tms);
static tmErrorCode_t Tda18212_UserWrittenPrint(UInt32 level,
						const char *format, ...);
static tmErrorCode_t
Tda18212_UserWrittenMutexInit(ptmbslFrontEndMutexHandle *ppMutexHandle);

static tmErrorCode_t
Tda18212_UserWrittenMutexDeInit(ptmbslFrontEndMutexHandle pMutex);
static
tmErrorCode_t Tda18212_UserWrittenMutexAcquire(ptmbslFrontEndMutexHandle pMutex,
					       UInt32 timeOut);
static tmErrorCode_t Tda18212_UserWrittenMutexRelease(ptmbslFrontEndMutexHandle
					       pMutex);

TUNER_Error_t TDA18212_TunerInit(void *pTunerInit_v,
				 STCHIP_Handle_t *TunerHandle)
{
	STCHIP_Handle_t hTuner = NULL;
	TUNER_Params_t hTunerParams = NULL;
	pTDA18212_TUNER_td TDA18212_td = NULL;
	STCHIP_Info_t ChipInfo;
	TUNER_Error_t error = TUNER_NO_ERR;
	TUNER_Params_t pTunerInit = (TUNER_Params_t) pTunerInit_v;
	ptmbslFrontEndDependency_t sSrvTunerFunc = NULL;
	tmErrorCode_t err = TM_OK;
	STCHIP_Register_t DefTDA18212Val[TDA18212_NBREGS];
	U32 index = 0;
	tmUnitSelect_t Unit = 0;
	ptmTDA182I2Object_t pObj = Null;

#ifdef HOST_PC
	/* Allocation of the chip structure     */
	hTunerParams = calloc(1, sizeof(TUNER_InitParams_t));
	TDA18212_td = calloc(1, sizeof(TDA18212_TUNER_tds));
#endif
#ifdef CHIP_STAPI
	if (*TunerHandle) {
		hTunerParams = (TUNER_Params_t) ((*TunerHandle)->pData);
		if (hTunerParams) {
			TDA18212_td =
			    (pTDA18212_TUNER_td) (hTunerParams->pAddParams);
			memcpy(&ChipInfo, *TunerHandle, sizeof(STCHIP_Info_t));
		}
	}
#endif
	if (hTunerParams) {
		/* Tuner name */
		strncpy((char *)ChipInfo.Name, (char *)pTunerInit->TunerName,
							MAXNAMESIZE - 1);
		/* Repeater host */
		ChipInfo.RepeaterHost = pTunerInit->RepeaterHost;
		ChipInfo.RepeaterFn = pTunerInit->RepeaterFn;	/* see above */
		/* Tuner has no embedded repeater */
		ChipInfo.Repeater = TRUE;
		/* Init tuner I2C address */
		ChipInfo.I2cAddr = pTunerInit->TunerI2cAddress;
		ChipInfo.NbRegs = TDA18212_NBREGS;
		ChipInfo.NbFields = TDA18212_NBFIELDS;
/* if ((pTunerInit->DemodModel==DEMOD_ISDBT_02)||
 * (pTunerInit->DemodModel==DEMOD_SONY2820R)) */
/* ChipInfo.ChipMode = STCHIP_MODE_SUBADR_8_SR; */
/* else */
		ChipInfo.ChipMode = STCHIP_MODE_SUBADR_8;
		ChipInfo.WrStart = RTDA18212_REGISTER00;
		ChipInfo.WrSize = TDA18212_NBREGS;
		ChipInfo.RdStart = RTDA18212_REGISTER00;
		ChipInfo.RdSize = TDA18212_NBREGS;
		ChipInfo.pData = hTunerParams;

		hTunerParams->TransmitStandard = pTunerInit->TransmitStandard;
		hTunerParams->DemodModel = pTunerInit->DemodModel;
		hTunerParams->TunerName = pTunerInit->TunerName;
		hTunerParams->TunerI2cAddress = pTunerInit->TunerI2cAddress;
/* if ((pTunerInit->DemodModel==DEMOD_ISDBT_02)||
 * (pTunerInit->DemodModel==DEMOD_SONY2820R)) */
/* hTunerParams->Tunermode = STCHIP_MODE_SUBADR_8_SR; */
/* else */
		hTunerParams->Tunermode = STCHIP_MODE_SUBADR_8;
		hTunerParams->Fxtal_Hz = pTunerInit->Fxtal_Hz;
		hTunerParams->Model = pTunerInit->Model;
		if ((hTunerParams->TransmitStandard == FE_DVB_T)
		    || (hTunerParams->TransmitStandard == FE_DVB_T2))
			hTunerParams->BandWidth = 8;
		else if (hTunerParams->TransmitStandard == FE_DVB_C)
			hTunerParams->BandWidth = 8;
		else	/* ISDBT */
			hTunerParams->BandWidth = 6;
		hTunerParams->pAddParams = (void *)TDA18212_td;

		sSrvTunerFunc =
		    (ptmbslFrontEndDependency_t) (&TDA18212_td->TDATunerFunc);
		if (sSrvTunerFunc) {
			/* Low layer struct set-up to link with user written
			 * functions */
			sSrvTunerFunc->sIo.Write = Tda18212_UserWrittenI2CWrite;
			sSrvTunerFunc->sIo.Read = Tda18212_UserWrittenI2CRead;
			sSrvTunerFunc->sTime.Get = Null;
			sSrvTunerFunc->sTime.Wait = Tda18212_UserWrittenWait;
			sSrvTunerFunc->sDebug.Print = Tda18212_UserWrittenPrint;
			sSrvTunerFunc->sMutex.Init =
			    Tda18212_UserWrittenMutexInit;
			sSrvTunerFunc->sMutex.DeInit =
			    Tda18212_UserWrittenMutexDeInit;
			sSrvTunerFunc->sMutex.Acquire =
			    Tda18212_UserWrittenMutexAcquire;
			sSrvTunerFunc->sMutex.Release =
			    Tda18212_UserWrittenMutexRelease;
			sSrvTunerFunc->dwAdditionalDataSize = 0;
			sSrvTunerFunc->pAdditionalData = Null;
		}
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

		if (hTuner != NULL) {
			/* This while loop is used to select an "available"
			 * tuner instance number */
			while ((Unit < TDA182I2_MAX_UNITS) && (err == TM_OK)) {

				err = TDA182I2GetInstance(Unit, &pObj);
				Unit++;
			}

			if ((Unit <= TDA182I2_MAX_UNITS) && (err != TM_OK)) {
				Unit = Unit - 1;
				TDA18212_td->tUnit = Unit;
				TDA18212TunerHandle[Unit] = (U32) hTuner;
				for (index = RTDA18212_REGISTER00;
				     index < TDA18212_NBREGS; index++) {
					DefTDA18212Val[index].Addr = index;
					/* default reset value for register
					 * "index" */
					DefTDA18212Val[index].Value = 0;
				}

				ChipUpdateDefaultValues(hTuner, DefTDA18212Val);

				/* TDA18212 Master Driver low layer setup */
				err =
				    tmbslTDA182I2Init(TDA18212_td->tUnit,
						      sSrvTunerFunc);

				if (err != TM_OK)
					error = TUNER_INVALID_HANDLE;
			} else
				error = TUNER_INVALID_HANDLE;

		}
#if defined(CHIP_STAPI) || defined(NO_GUI)
		error = TDA18212_TunerWrite(hTuner);
#endif
	} else
		error = TUNER_INVALID_HANDLE;

	return error;
}

TUNER_Error_t TDA18212_TunerSetFrequency(STCHIP_Handle_t hTuner, U32 Frequency)
{
	TUNER_Error_t error = TUNER_NO_ERR;
	TUNER_Params_t hTunerParams = NULL;
	pTDA18212_TUNER_td TDA18212_td = NULL;
	tmErrorCode_t err = TM_OK;
	if (hTuner) {
		hTunerParams = (TUNER_Params_t) hTuner->pData;
		if (hTunerParams) {
			TDA18212_td =
			    (pTDA18212_TUNER_td) (hTunerParams->pAddParams);
			if (TDA18212_td != NULL) {
				/* Set TDA182I2 power state to Normal Mode */
				err =
				  tmbslTDA182I2SetPowerState(TDA18212_td->tUnit,
					       tmTDA182I2_PowerNormalMode);

				if (err != TM_OK) {
					error = TUNER_INVALID_HANDLE;
				} else {
					/* Set Tuner RF */
					err =
					  tmbslTDA182I2SetRf(TDA18212_td->tUnit,
						      1000 * Frequency);

					if (err != TM_OK)
						error = TUNER_INVALID_HANDLE;
				}
			} else
				error = TUNER_INVALID_HANDLE;
		} else
			error = TUNER_INVALID_HANDLE;
	} else
		error = TUNER_INVALID_HANDLE;

	return error;
}

U32 TDA18212_TunerGetFrequency(STCHIP_Handle_t hTuner)
{
	TUNER_Params_t hTunerParams = NULL;
	U32 frequency = 0;
	pTDA18212_TUNER_td TDA18212_td = NULL;
	tmErrorCode_t err = TM_OK;

	if (hTuner) {
		hTunerParams = (TUNER_Params_t) hTuner->pData;
		if (hTunerParams) {
			TDA18212_td =
			    (pTDA18212_TUNER_td) (hTunerParams->pAddParams);
		}
	}
	if (TDA18212_td != NULL) {
		/* TDA18212 Master RF frequency */
		err = tmbslTDA182I2GetRf(TDA18212_td->tUnit, &frequency);
		if (err == TM_OK)
			frequency = frequency / 1000;
	}

	return frequency;
}

BOOL TDA18212_TunerGetStatus(STCHIP_Handle_t hTuner)
{
	TUNER_Params_t hTunerParams = NULL;
	U8 wait = 0;
	BOOL locked = FALSE;
	tmErrorCode_t err = TM_OK;
	pTDA18212_TUNER_td TDA18212_td = NULL;
	tmbslFrontEndState_t PLLLock = tmbslFrontEndStateUnknown;

	if (hTuner)
		hTunerParams = (TUNER_Params_t) hTuner->pData;
	if (hTunerParams) {
		TDA18212_td = (pTDA18212_TUNER_td) (hTunerParams->pAddParams);
		if (TDA18212_td != NULL) {
			wait = 0;
			err =
			    tmbslTDA182I2GetLockStatus(TDA18212_td->tUnit,
						       &PLLLock);
			while ((wait < 100)
			       && (PLLLock != tmbslFrontEndStateLocked)
			       && (err == TM_OK)) {
				err =
				    tmbslTDA182I2GetLockStatus(TDA18212_td->
							       tUnit, &PLLLock);
				ChipWaitOrAbort((STCHIP_Handle_t)
						GetTDA18212TunerHandle
						(TDA18212_td->tUnit), 5);
				wait += 5;
			}

			if (err != TM_OK) {
				locked = FALSE;
			} else {
				if (PLLLock == tmbslFrontEndStateLocked)
					locked = TRUE;
			}
		}
	}

	return locked;
}

TUNER_Error_t TDA18212_TunerSetBandWidth(STCHIP_Handle_t hTuner, U32 Band_Width)
{
	TUNER_Error_t error = TUNER_NO_ERR;
	TUNER_Params_t hTunerParams = NULL;
	pTDA18212_TUNER_td TDA18212_td = NULL;
	tmErrorCode_t err = TM_OK;
	tmTDA182I2StandardMode_t stdModeMaster;
	U32 IF = 0;
	if (hTuner)
		hTunerParams = (TUNER_Params_t) hTuner->pData;

	if (hTunerParams) {
		hTunerParams->BandWidth = Band_Width;
		TDA18212_td = (pTDA18212_TUNER_td) (hTunerParams->pAddParams);

		/* Update stdModeMaster based on bandwidth */
		stdModeMaster =
		    TDA18212_Tuner_GetStandardMode(hTuner, Band_Width);
		err =
		    tmbslTDA182I2SetStandardMode(TDA18212_td->tUnit,
						 stdModeMaster);
		if (err != TM_OK)
			error = TUNER_INVALID_HANDLE;
		IF = TDA18212_TunerGetIF_Freq(hTuner);
		hTunerParams->IF = IF;
	} else
		error = TUNER_INVALID_HANDLE;

	return error;
}

U32 TDA18212_TunerGetBandWidth(STCHIP_Handle_t hTuner)
{
	U32 TunerBW = 0;
	TUNER_Params_t hTunerParams = NULL;

	if (hTuner)
		hTunerParams = (TUNER_Params_t) hTuner->pData;

	if (hTunerParams)
		TunerBW = hTunerParams->BandWidth;
	return TunerBW;
}

TUNER_Error_t TDA18212_SwitchTunerToDVBT2(STCHIP_Handle_t hTuner)
{
	TUNER_Error_t error = TUNER_NO_ERR;
	TUNER_Params_t hTunerParams = NULL;
	pTDA18212_TUNER_td TDA18212_td = NULL;

	if (hTuner) {
		hTunerParams = (TUNER_Params_t) hTuner->pData;
		if (hTunerParams) {
			TDA18212_td =
			    (pTDA18212_TUNER_td) hTunerParams->pAddParams;
			if (TDA18212_td) {
				/* Set Tuner Standard mode */
				tmbslTDA182I2SetStandardMode(TDA18212_td->tUnit,
						     tmTDA182I2_DVBT_8MHz);
				TDA18212_TunerGetIF_Freq(hTuner);
			} else
				error = CHIPERR_INVALID_HANDLE;
		} else
			error = CHIPERR_INVALID_HANDLE;
	} else
		error = CHIPERR_INVALID_HANDLE;

	return error;
}

TUNER_Error_t TDA18212_SwitchTunerToDVBT(STCHIP_Handle_t hTuner)
{
	TUNER_Error_t error = TUNER_NO_ERR;
	TUNER_Params_t hTunerParams = NULL;
	pTDA18212_TUNER_td TDA18212_td = NULL;

	if (hTuner) {
		hTunerParams = (TUNER_Params_t) hTuner->pData;
		if (hTunerParams) {
			TDA18212_td =
			    (pTDA18212_TUNER_td) hTunerParams->pAddParams;
			if (TDA18212_td) {
				/* Set Tuner Standard mode */
				tmbslTDA182I2SetStandardMode(TDA18212_td->tUnit,
						     tmTDA182I2_DVBT_8MHz);
				TDA18212_TunerGetIF_Freq(hTuner);
			} else
				error = CHIPERR_INVALID_HANDLE;
		} else
			error = CHIPERR_INVALID_HANDLE;
	} else
		error = CHIPERR_INVALID_HANDLE;

	return error;
}

TUNER_Error_t TDA18212_SwitchTunerToDVBC(STCHIP_Handle_t hTuner)
{
	TUNER_Error_t error = TUNER_NO_ERR;
	TUNER_Params_t hTunerParams = NULL;
	pTDA18212_TUNER_td TDA18212_td = NULL;

	if (hTuner) {
		hTunerParams = (TUNER_Params_t) hTuner->pData;
		if (hTunerParams) {
			TDA18212_td =
			    (pTDA18212_TUNER_td) hTunerParams->pAddParams;
			if (TDA18212_td) {
				/* Set Tuner Standard mode */
				tmbslTDA182I2SetStandardMode(TDA18212_td->tUnit,
						     tmTDA182I2_QAM_8MHz);
				TDA18212_TunerGetIF_Freq(hTuner);
			} else
				error = CHIPERR_INVALID_HANDLE;
		} else
			error = CHIPERR_INVALID_HANDLE;
	} else
		error = CHIPERR_INVALID_HANDLE;

	return error;
}

TUNER_Error_t TDA18212_TunerSetStandby(STCHIP_Handle_t hTuner, U8 StandbyOn)
{
	TUNER_Error_t error = TUNER_NO_ERR;
	TUNER_Params_t hTunerParams = NULL;
	pTDA18212_TUNER_td TDA18212_td = NULL;
	tmErrorCode_t err = TM_OK;

	if (!hTuner)
		return CHIPERR_INVALID_HANDLE;

	hTunerParams = (TUNER_Params_t) hTuner->pData;
	if (!hTunerParams)
		return CHIPERR_INVALID_HANDLE;

	TDA18212_td = (pTDA18212_TUNER_td) hTunerParams->pAddParams;
	if (!TDA18212_td)
		return CHIPERR_INVALID_HANDLE;

	if (StandbyOn)	/*Power down ON */
		err = tmbslTDA182I2SetPowerState(TDA18212_td->tUnit,
				tmTDA182I2_PowerStandbyWithLNAOnAndWithXtalOn);
	else
		err = tmbslTDA182I2SetPowerState(TDA18212_td->tUnit,
				tmTDA182I2_PowerNormalMode);
	if (err)
		error = CHIPERR_INVALID_HANDLE;

	return error;
}

TUNER_Error_t TDA18212_TunerWrite(STCHIP_Handle_t hTuner)
{
	TUNER_Error_t error = TUNER_NO_ERR;
	TUNER_Params_t hTunerParams = NULL;
	tmErrorCode_t err = TM_OK;
	pTDA18212_TUNER_td TDA18212_td = NULL;
	U32 puIF = 0;
	tmTDA182I2StandardMode_t stdModeMaster = tmTDA182I2_DVBT_8MHz;
	if (!hTuner)
		return TUNER_INVALID_HANDLE;

	if (hTuner->Error)
		return hTuner->Error;

	hTunerParams = (TUNER_Params_t) hTuner->pData;
	if (!hTunerParams)
		return TUNER_INVALID_HANDLE;

	TDA18212_td = (pTDA18212_TUNER_td)hTunerParams->pAddParams;
	stdModeMaster = TDA18212_Tuner_GetStandardMode(hTuner,
					hTunerParams->BandWidth);

	if (!TDA18212_td)
		return TUNER_INVALID_HANDLE;
	/* TDA18212 Master Hardware initialization */
	err = tmbslTDA182I2Reset(TDA18212_td->tUnit);

	if (err == TM_OK) {
		/* TDA18212 Master Hardware power state */
		err = tmbslTDA182I2SetPowerState(TDA18212_td->tUnit,
				tmTDA182I2_PowerNormalMode);
		if (err == TM_OK) {
			/* Set Tuner Standard mode */
			err = tmbslTDA182I2SetStandardMode(TDA18212_td->tUnit,
					stdModeMaster);
			if (err != TM_OK)
				error = TUNER_INVALID_HANDLE;
			err = tmbslTDA182I2GetIF(TDA18212_td->tUnit, &puIF);
			if (err != TM_OK)
				error = TUNER_INVALID_HANDLE;
			hTunerParams->IF = puIF / 1000;
		} else {
			error = TUNER_INVALID_HANDLE;
		}
	} else
		error = TUNER_INVALID_HANDLE;

	return error;
}

TUNER_Error_t TDA18212_TunerRead(STCHIP_Handle_t hTuner)
{
	TUNER_Error_t error = TUNER_NO_ERR;

	/* This tuner has no read capability */

	if (hTuner) {
		if (hTuner->Error)
			error = (TUNER_Error_t) hTuner->Error;
	} else
		error = TUNER_INVALID_HANDLE;

	return error;
}

TUNER_Model_t TDA18212_TunerGetModel(STCHIP_Handle_t hTuner)
{
	TUNER_Model_t model = TUNER_NULL;

	if (hTuner && hTuner->pData)
		model = ((TUNER_Params_t) (hTuner->pData))->Model;

	return model;
}

void TDA18212_TunerSetIQ_Wiring(STCHIP_Handle_t hTuner, S8 IQ_Wiring)
{
	if (hTuner && hTuner->pData)
		((TUNER_Params_t) (hTuner->pData))->IQ_Wiring = IQ_Wiring;
}

S8 TDA18212_TunerGetIQ_Wiring(STCHIP_Handle_t hTuner)
{
	TUNER_IQ_t wiring = TUNER_IQ_NORMAL;

	if (hTuner && hTuner->pData)
		wiring = ((TUNER_Params_t) (hTuner->pData))->IQ_Wiring;

	return (S8) wiring;
}

void TDA18212_TunerSetReferenceFreq(STCHIP_Handle_t hTuner, U32 Reference)
{
	if (hTuner && hTuner->pData)
		((TUNER_Params_t) (hTuner->pData))->Fxtal_Hz = Reference;
}

U32 TDA18212_TunerGetReferenceFreq(STCHIP_Handle_t hTuner)
{
	U32 reference = 0;

	if (hTuner && hTuner->pData)
		reference = ((TUNER_Params_t) (hTuner->pData))->Fxtal_Hz;

	return reference;
}

void TDA18212_TunerSetIF_Freq(STCHIP_Handle_t hTuner, U32 IF)
{
	TUNER_Params_t hTunerParams = NULL;

	if (hTuner) {
		hTunerParams = (TUNER_Params_t) hTuner->pData;
		if (hTunerParams)
			hTunerParams->IF = IF;

	}
}

U32 TDA18212_TunerGetIF_Freq(STCHIP_Handle_t hTuner)
{
	TUNER_Params_t hTunerParams = NULL;
	U32 puIF = 0;
	tmErrorCode_t err = TM_OK;
	pTDA18212_TUNER_td TDA18212_td = NULL;

	if (hTuner) {
		hTunerParams = (TUNER_Params_t) hTuner->pData;
		if (hTunerParams) {
			TDA18212_td =
			    (pTDA18212_TUNER_td) (hTunerParams->pAddParams);
			if (TDA18212_td != NULL) {
				err =
				    tmbslTDA182I2GetIF(TDA18212_td->tUnit,
						       &puIF);
				if (err == TM_OK)
					hTunerParams->IF = puIF / 1000;
			}
		}
	}

	return puIF / 1000;
}

TUNER_Error_t TDA18212_TunerTerm(STCHIP_Handle_t hTuner)
{
	TUNER_Params_t hTunerParams = NULL;
	tmErrorCode_t err = TM_OK;
	TUNER_Error_t error = TUNER_NO_ERR;
	pTDA18212_TUNER_td TDA18212_td = NULL;

	if (hTuner) {
		hTunerParams = (TUNER_Params_t) hTuner->pData;
		if (hTunerParams) {
			TDA18212_td =
			    (pTDA18212_TUNER_td) (hTunerParams->pAddParams);
			if (TDA18212_td != NULL) {
				/* DeInitialize TDA18212 Master Driver */
				err = tmbslTDA182I2DeInit(TDA18212_td->tUnit);
				TDA18212TunerHandle[TDA18212_td->tUnit] = 0;
			} else
				error = TUNER_INVALID_HANDLE;
		} else
			error = TUNER_INVALID_HANDLE;
#ifndef CHIP_STAPI
		if (hTunerParams)
			free(hTunerParams);
		ChipClose(hTuner);
#endif
	} else
		error = TUNER_INVALID_HANDLE;
	if (err != TM_OK)
		error = TUNER_INVALID_HANDLE;

	return error;
}

/*--------------------------------------------------------------------*/
/* Added to get htuner handle for UserWrittenI2CRead and  UserWrittenI2CWrite
 * function by AT
 *---------------------------------------------------------------------*/
U32 GetTDA18212TunerHandle(tmUnitSelect_t tUnit)
{

	tmUnitSelect_t Unit = 0;
	for (Unit = 0; Unit <= TDA182I2_MAX_UNITS; Unit++) {
		if (Unit == tUnit)
			return TDA18212TunerHandle[Unit];

	}
	return 0;
}

/*------------------------------------------------------------------------*/
/* Template of function to be provided by customer	*/
/*----------------------------------------------------------------------------*/
/*---------------------------------------------------------------------*/
/* Function Name       : UserWrittenI2CRead	*/
/* Object              :	*/
/* Input Parameters    :	tmUnitSelect_t tUnit	*/
/*						UInt32 AddrSize,	*/
/*						UInt8* pAddr,	*/
/*						UInt32 ReadLen,	*/
/*						UInt8* pData	*/
/* Output Parameters   : tmErrorCode_t.	*/
/*-----------------------------------------------------------------------*/
tmErrorCode_t Tda18212_UserWrittenI2CRead(tmUnitSelect_t tUnit, UInt32 AddrSize,
					  UInt8 *pAddr, UInt32 ReadLen,
					  UInt8 *pData)
{
	STCHIP_Handle_t hTuner = NULL;
	tmErrorCode_t err = TM_OK;
	U32 i = 0;
	STCHIP_Error_t Error = TUNER_NO_ERR;
	hTuner = (STCHIP_Handle_t) GetTDA18212TunerHandle(tUnit);

	if (hTuner) {
		Error = ChipGetRegisters(hTuner, pAddr[0], ReadLen);
		if (Error)
			err = TDA182I2_ERR_BAD_PARAMETER;

		for (i = 0; i < ReadLen; i++)
			pData[i] = hTuner->pRegMapImage[pAddr[0] + i].Value;
	} else
		err = TDA182I2_ERR_NOT_SUPPORTED;
	return err;
}

/*---------------------------------------------------------------------*/
/* Function Name       : UserWrittenI2CWrite	*/
/* Object              :	*/
/* Input Parameters    :	tmUnitSelect_t tUnit	*/
/*						UInt32 AddrSize,	*/
/*						UInt8* pAddr,	*/
/*						UInt32 WriteLen,	*/
/*						UInt8* pData	*/
/* Output Parameters   : tmErrorCode_t.	*/
/*---------------------------------------------------------------------*/
tmErrorCode_t Tda18212_UserWrittenI2CWrite(tmUnitSelect_t tUnit,
					   UInt32 AddrSize, UInt8 *pAddr,
					   UInt32 WriteLen, UInt8 *pData)
{
	STCHIP_Handle_t hTuner = NULL;
	tmErrorCode_t err = TM_OK;
	U32 i = 0;

	hTuner = (STCHIP_Handle_t) GetTDA18212TunerHandle(tUnit);
	if (hTuner) {
		for (i = 0; i < WriteLen; i++) {
			hTuner->pRegMapImage[pAddr[0] + i].Addr = pAddr[0] + i;
			hTuner->pRegMapImage[pAddr[0] + i].Value = pData[i];
		}
		ChipSetRegisters(hTuner, pAddr[0], WriteLen);
	} else
		err = TDA182I2_ERR_NOT_SUPPORTED;

	return err;
}

/*---------------------------------------------------------------------*/
/* Function Name       : UserWrittenWait	*/
/* Object              :	*/
/* Input Parameters    :	tmUnitSelect_t tUnit	*/
/*						UInt32 tms	*/
/* Output Parameters   : tmErrorCode_t.	*/
/*---------------------------------------------------------------------*/
tmErrorCode_t Tda18212_UserWrittenWait(tmUnitSelect_t tUnit, UInt32 tms)
{
	/* Variable declarations */
	tmErrorCode_t err = TM_OK;

/* Customer code here */
/* ...*/
	ChipWaitOrAbort((STCHIP_Handle_t) GetTDA18212TunerHandle(tUnit), tms);
/* ...*/
/* End of Customer code here */

	return err;
}

/*---------------------------------------------------------------------*/
/* Function Name       : UserWrittenPrint	*/
/* Object              :	*/
/* Input Parameters    :	UInt32 level, const char* format, ...	*/
/*	*/
/* Output Parameters   : tmErrorCode_t.	*/
/*---------------------------------------------------------------------*/
tmErrorCode_t Tda18212_UserWrittenPrint(UInt32 level, const char *format, ...)
{
	/* Variable declarations */
	tmErrorCode_t err = TM_OK;

/* Customer code here */
/* ...*/

/* ...*/
/* End of Customer code here */

	return err;
}

/*------------------------------------------------------------------------*/
/* Function Name       : UserWrittenMutexInit*/
/* Object              :		*/
/* Input Parameters    :	ptmbslFrontEndMutexHandle *ppMutexHandle*/
/* Output Parameters   : tmErrorCode_t.					*/
/*-------------------------------------------------------------------------*/
tmErrorCode_t Tda18212_UserWrittenMutexInit(ptmbslFrontEndMutexHandle *
					    ppMutexHandle)
{
	/* Variable declarations */
	tmErrorCode_t err = TM_OK;

/* Customer code here */
/* ...*/

/* ...*/
/* End of Customer code here */

	return err;
}

/*---------------------------------------------------------------------------*/
/* Function Name       : UserWrittenMutexDeInit		*/
/* Object              :				*/
/* Input Parameters    :	 ptmbslFrontEndMutexHandle pMutex*/
/* Output Parameters   : tmErrorCode_t.				*/
/*------------------------------------------------------------------------*/
tmErrorCode_t Tda18212_UserWrittenMutexDeInit(ptmbslFrontEndMutexHandle pMutex)
{
	/* Variable declarations */
	tmErrorCode_t err = TM_OK;

/* Customer code here */
/* ...*/

/* ...*/
/* End of Customer code here */

	return err;
}

/*--------------------------------------------------------------------------*/
/* Function Name       : UserWrittenMutexAcquire			    */
/* Object              :						    */
/* Input Parameters    : ptmbslFrontEndMutexHandle pMutex, UInt32 timeOut   */
/* Output Parameters   : tmErrorCode_t.					    */
/*--------------------------------------------------------------------------*/
tmErrorCode_t Tda18212_UserWrittenMutexAcquire(ptmbslFrontEndMutexHandle pMutex,
					       UInt32 timeOut)
{
	/* Variable declarations */
	tmErrorCode_t err = TM_OK;

/* Customer code here */
/* ...*/

/* ...*/
/* End of Customer code here */

	return err;
}

/*---------------------------------------------------------------------------*/
/* Function Name       : UserWrittenMutexRelease			     */
/* Object              :						     */
/* Input Parameters    :	ptmbslFrontEndMutexHandle pMutex	     */
/* Output Parameters   : tmErrorCode_t.					     */
/*---------------------------------------------------------------------------*/
tmErrorCode_t Tda18212_UserWrittenMutexRelease(ptmbslFrontEndMutexHandle pMutex)
{
	/* Variable declarations */
	tmErrorCode_t err = TM_OK;

/* Customer code here */
/* ...*/

/* ...*/
/* End of Customer code here */

	return err;
}

BOOL TDA18212_TunerGetPowerLevel(STCHIP_Handle_t hTuner, S32 *RF_Input_Level)
{

	/*warning: variable 'error' set but not used */
	/* TUNER_Error_t error = TUNER_NO_ERR; */
	TUNER_Params_t hTunerParams = NULL;
	pTDA18212_TUNER_td TDA18212_td = NULL;
	tmErrorCode_t err = TM_OK;
	UInt32 powerLevel_dBuV;

	if (hTuner) {
		hTunerParams = (TUNER_Params_t) hTuner->pData;
		if (hTunerParams)
			TDA18212_td =
			    (pTDA18212_TUNER_td) (hTunerParams->pAddParams);
	}
	if (TDA18212_td != NULL) {
		err =
		    tmbslTDA182I2GetPowerLevel(TDA18212_td->tUnit,
					       &powerLevel_dBuV);
		if (err != TM_OK)
			/*warning: variable 'error' set but not used */
			/*error = TUNER_INVALID_HANDLE; */
			return FALSE;

		*RF_Input_Level = (((S32) powerLevel_dBuV) - 109);
	}
	return TRUE;
}

tmTDA182I2StandardMode_t TDA18212_Tuner_GetStandardMode(STCHIP_Handle_t hTuner,
							U32 Band_Width)
{
	/* warning: variable 'error' set but not used */
	/*TUNER_Error_t error = TUNER_NO_ERR; */
	TUNER_Params_t hTunerParams = NULL;
	pTDA18212_TUNER_td TDA18212_td = NULL;
	tmTDA182I2StandardMode_t stdModeMaster = tmTDA182I2_DVBT_8MHz;

	if (hTuner) {
		hTunerParams = (TUNER_Params_t) hTuner->pData;
		if (hTunerParams)
			TDA18212_td =
			    (pTDA18212_TUNER_td) (hTunerParams->pAddParams);
	}

	if (TDA18212_td != NULL) {
		if (Band_Width == 6) {
			if (hTunerParams->TransmitStandard == FE_ISDB_T)
				stdModeMaster = tmTDA182I2_ISDBT_6MHz;
			else if ((hTunerParams->TransmitStandard == FE_DVB_T)
				   || (hTunerParams->TransmitStandard ==
				       FE_DVB_T2))
				stdModeMaster = tmTDA182I2_DVBT_6MHz;
			else if (hTunerParams->TransmitStandard == FE_J83B)
				stdModeMaster = tmTDA182I2_QAM_6MHz;

		} else if (Band_Width == 7) {
			stdModeMaster = tmTDA182I2_DVBT_7MHz;
		} else if (Band_Width == 8) {
			if ((hTunerParams->TransmitStandard == FE_DVB_T)
			    || (hTunerParams->TransmitStandard == FE_DVB_T2)) {
				stdModeMaster = tmTDA182I2_DVBT_8MHz;
			} else if (hTunerParams->TransmitStandard == FE_DVB_C)
				stdModeMaster = tmTDA182I2_QAM_8MHz;
		} else {
			/*in case  if not 6,7or 8 */
			stdModeMaster = tmTDA182I2_DVBT_8MHz;
		}
	}

	return stdModeMaster;
}

S32 TDA18212_TunerEstimateRfPower(STCHIP_Handle_t hTuner, U16 RfAgc, U16 IfAgc)
{

	/*warning: variable 'error' set but not used */
	/*TUNER_Error_t error = TUNER_NO_ERR; */
	TUNER_Params_t hTunerParams = NULL;
	pTDA18212_TUNER_td TDA18212_td = NULL;
	S32 RfEstimatePower = 0;
	tmErrorCode_t err = TM_OK;
	UInt8 Index_AGC;
	Int16 Total_AGC;
	Int8 AGC1_Gain_Read[10] = { -12, -9, -6, -3, 0, 3, 6, 9, 12, 15 };
	Int8 AGC2_Gain_Read[4] = { -11, -8, -5, -2 };
	Int8 AGC4_Gain_Read[5] = { 2, 5, 8, 11, 14 };
	Int8 AGC5_Gain_Read[4] = { 0, 3, 6, 9 };
	/*UInt8 HW_PLD_VALUE; */
	Bool Use_HW_PLD = FALSE;

	if (hTuner) {
		hTunerParams = (TUNER_Params_t) hTuner->pData;
		if (hTunerParams) {
			TDA18212_td =
			    (pTDA18212_TUNER_td) (hTunerParams->pAddParams);
		} else
			return FALSE;

	} else
		return FALSE;

	if (TDA18212_td != NULL) {
		Total_AGC = 0;
		err =
		    tmddTDA182I2GetAGC1_Gain_Read(TDA18212_td->tUnit,
						  &Index_AGC);

		if (err == TM_OK) {
			Total_AGC = Total_AGC + AGC1_Gain_Read[Index_AGC];
			err =
			    tmddTDA182I2GetAGC2_Gain_Read(TDA18212_td->tUnit,
							  &Index_AGC);
		}
		if (err != TM_OK)
			return FALSE;
		if (err == TM_OK) {
			Total_AGC = Total_AGC + AGC2_Gain_Read[Index_AGC];
			err =
			    tmddTDA182I2GetAGC4_Gain_Read(TDA18212_td->tUnit,
							  &Index_AGC);

		}
		if (err != TM_OK)
			return FALSE;
		if (err == TM_OK) {
			Total_AGC = Total_AGC + AGC4_Gain_Read[Index_AGC];
			err =
			    tmddTDA182I2GetAGC5_Gain_Read(TDA18212_td->tUnit,
							  &Index_AGC);

		}
		if (err != TM_OK)
			return FALSE;
		if (err == TM_OK)
			Total_AGC = Total_AGC + AGC5_Gain_Read[Index_AGC];
/* commented to fix coverity issue*/
/*	if (Use_HW_PLD)
	{
		err = tmddTDA182I2GetRSSI(TDA18212_td->tUnit, &HW_PLD_VALUE);

		if (err!=TM_OK)
			{
				error = TUNER_INVALID_HANDLE;
				return FALSE;
			}
		if (HW_PLD_VALUE < (-6875/100) )

			Use_HW_PLD = False;
		else
		{
			if (HW_PLD_VALUE < (-438/10) )

				Total_AGC = Total_AGC + 23;
			else if (HW_PLD_VALUE < -38 )

				Total_AGC = Total_AGC + 14;
			else

				Total_AGC = 39;
		}
	}        */
		if (!Use_HW_PLD) {
			if (Total_AGC < 20)	/* 22 */
				Total_AGC = Total_AGC + 14;	/* 12 */
			else if (Total_AGC > (225 / 10))	/* 23.4 */
				Total_AGC = Total_AGC + 23;	/* 22 */
			else
				Total_AGC = 39;
		}

		RfEstimatePower = Total_AGC * -1;

	}

	return RfEstimatePower;
}

/* End of tda18212_tuner.c */
