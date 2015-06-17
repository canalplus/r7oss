/**********************************************************************
Copyright (C) 2006-2009 NXP B.V.

This program is free software: you can redistribute it and/or modify

it under the terms of the GNU General Public License as published by

the Free Software Foundation, either version 3 of the License, or

(at your option) any later version.

This program is distributed in the hope that it will be useful,

but WITHOUT ANY WARRANTY; without even the implied warranty of

MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the

GNU General Public License for more details.

You should have received a copy of the GNU General Public License

along with this program.  If not, see <http://www.gnu.org/licenses/>.
*************************************************************************/
/* ------------------------------------------------------------------------ */
/* Standard include files: */
/* ------------------------------------------------------------------------ */
/*  */
#include <linux/kernel.h>
#include "tmnxtypes.h"
#include "tmcompid.h"
#include "tmfrontend.h"
#include "tmbslfrontendtypes.h"
#include "tmddtda18212.h"

#ifdef TMBSL_TDA18272
#include "tmbsltda18272.h"
#else /* TMBSL_TDA18272 */
#include "tmbsltda18212.h"
#endif /* TMBSL_TDA18272 */

/* ------------------------------------------------------------------------ */
/* Project include files: */
/* ------------------------------------------------------------------------ */
#include "tmbsltda18212local.h"
#include "tmbsltda18212instance.h"

/* ------------------------------------------------------------------------ */
/* Types and defines: */
/* ------------------------------------------------------------------------ */
static tmErrorCode_t TDA182I2Init(tmUnitSelect_t tUnit);
static tmErrorCode_t TDA182I2Wait(ptmTDA182I2Object_t pObj, UInt32 Time);
static tmErrorCode_t TDA182I2WaitXtalCal_End(ptmTDA182I2Object_t pObj,
					     UInt32 timeOut, UInt32 waitStep);

/* -------------------------------------------------------------------------
 * Global data:
 * -------------------------------------------------------------------------
 *
 * -------------------------------------------------------------------------
 * Exported functions:
 * -------------------------------------------------------------------------
 *
 * -------------------------------------------------------------------------
 * FUNCTION:    tmbslTDA18211Init:
 *
 * DESCRIPTION: create an instance of a TDA182I2 Tuner
 *
 * RETURN:      TMBSL_ERR_TUNER_BAD_UNIT_NUMBER
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------- */
tmErrorCode_t tmbslTDA182I2Init(tmUnitSelect_t tUnit,	/* I: Unit number */
		tmbslFrontEndDependency_t *psSrvFunc)	/* I: setup parameters*/
{
	ptmTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	if (psSrvFunc == Null)
		err = TDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* ---------------------- */
		/* initialize the Object */
		/* ---------------------- */
		/* pObj initialization */
		err = TDA182I2GetInstance(tUnit, &pObj);
	}

	/* check driver state */
	if (err == TM_OK || err == TDA182I2_ERR_NOT_INITIALIZED) {
		if (pObj != Null && pObj->init == True) {
			err = TDA182I2_ERR_NOT_INITIALIZED;
		} else {
			/* initialize the Object */
			if (pObj == Null) {
				err = TDA182I2AllocInstance(tUnit, &pObj);
				if (err != TM_OK || pObj == Null)
					err = TDA182I2_ERR_NOT_INITIALIZED;
			}

			if (err == TM_OK) {
				/* initialize the Object by default values */
				pObj->sRWFunc = psSrvFunc->sIo;
				pObj->sTime = psSrvFunc->sTime;
				pObj->sDebug = psSrvFunc->sDebug;

				if (psSrvFunc->sMutex.Init != Null
				    && psSrvFunc->sMutex.DeInit != Null
				    && psSrvFunc->sMutex.Acquire != Null
				    && psSrvFunc->sMutex.Release != Null) {
					pObj->sMutex = psSrvFunc->sMutex;

					err = pObj->sMutex.Init(&pObj->pMutex);
				}

				pObj->init = True;
				err = TM_OK;

				err = tmddTDA182I2Init(tUnit, psSrvFunc);
				tmASSERTExT(err, TM_OK,
					    (DEBUGLVL_ERROR,
					     "tmddTDA182I2Init(0x%08X) failed.",
					     pObj->tUnit));
			}
		}
	}

	return err;
}

/* -------------------------------------------------------------------------
 * FUNCTION:    tmbslTDA182I2DeInit:
 *
 * DESCRIPTION: destroy an instance of a TDA182I2 Tuner
 *
 * RETURN:      TMBSL_ERR_TUNER_BAD_UNIT_NUMBER
 * TDA182I2_ERR_NOT_INITIALIZED
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------- */
tmErrorCode_t tmbslTDA182I2DeInit(tmUnitSelect_t tUnit	/* I: Unit number */)
{
	ptmTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* check input parameters */
	err = TDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "TDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = TDA182I2MutexAcquire(pObj, TDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		err = tmddTDA182I2DeInit(tUnit);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "tmddTDA182I2DeInit(0x%08X) failed.",
			     pObj->tUnit));

		(void)TDA182I2MutexRelease(pObj);

		if (pObj->sMutex.DeInit != Null) {
			if (pObj->pMutex != Null)
				err = pObj->sMutex.DeInit(pObj->pMutex);

			pObj->sMutex.Init = Null;
			pObj->sMutex.DeInit = Null;
			pObj->sMutex.Acquire = Null;
			pObj->sMutex.Release = Null;

			pObj->pMutex = Null;
		}
	}

	err = TDA182I2DeAllocInstance(tUnit);

	return err;
}

/* --------------------------------------------------------------------------
 * FUNCTION:    tmbslTDA182I2GetSWVersion:
 *
 * DESCRIPTION: Return the version of this device
 *
 * RETURN:      TM_OK
 *
 * NOTES:       Values defined in the tmTDA182I2local.h file
 * -------------------------------------------------------------------------- */
/* I: Receives SW Version */
tmErrorCode_t tmbslTDA182I2GetSWVersion(ptmSWVersion_t pSWVersion)
{
	tmErrorCode_t err = TDA182I2_ERR_NOT_INITIALIZED;

	err = tmddTDA182I2GetSWVersion(pSWVersion);

	return err;
}

/* --------------------------------------------------------------------------
 * FUNCTION:    tmbslTDA182I2CheckHWVersion:
 *
 * DESCRIPTION: Check HW version
 *
 * RETURN:      TM_OK if no error
 *
 * NOTES:       Values defined in the tmTDA182I2local.h file
 * -------------------------------------------------------------------------- */
/* I: Unit number */
tmErrorCode_t tmbslTDA182I2CheckHWVersion(tmUnitSelect_t tUnit)
{
	ptmTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TDA182I2_ERR_NOT_INITIALIZED;
	UInt16 uIdentity = 0;
	UInt8 majorRevision = 0;

	err = TDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "TDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = TDA182I2MutexAcquire(pObj, TDA182I2_MUTEX_TIMEOUT);

	if (err != TM_OK)
		return err;

	err = tmddTDA182I2GetIdentity(tUnit, &uIdentity);

	if (err == TM_OK) {
		if (uIdentity == 18272 || uIdentity == 18212) {
			/* TDA18272/12 found. Check Major Revision */
			err =
			    tmddTDA182I2GetMajorRevision(tUnit, &majorRevision);
			tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR,
			     "tmddTDA182I2GetMajorRevision(0x%08X) failed.",
				     tUnit));

			if (err == TM_OK && majorRevision != 1)
				/* Only TDA18272/12 ES2 are supported */
				err = TDA182I2_ERR_BAD_VERSION;
		} else
			err = TDA182I2_ERR_BAD_VERSION;
	}

	(void)TDA182I2MutexRelease(pObj);

	return err;
}

/* --------------------------------------------------------------------------
 * FUNCTION:    tmbslTDA182I2SetPowerState:
 *
 * DESCRIPTION: Set the power state of this device.
 *
 * RETURN:      TMBSL_ERR_TUNER_BAD_UNIT_NUMBER
 * TDA182I2_ERR_NOT_INITIALIZED
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------ */
tmErrorCode_t tmbslTDA182I2SetPowerState(tmUnitSelect_t tUnit,/* I: Unit no. */
	 tmTDA182I2PowerState_t powerState)	/* I: Power state of device */
{
	ptmTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;
	tmddTDA182I2PowerState_t ddPowerState;

	/* pObj initialization */
	err = TDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "TDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = TDA182I2MutexAcquire(pObj, TDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		if (powerState > pObj->minPowerState) {
			ddPowerState =
			    (tmddTDA182I2PowerState_t) pObj->minPowerState;
		} else
			ddPowerState = (tmddTDA182I2PowerState_t) powerState;

		/* Call tmddTDA182I2SetPowerState */
		err = tmddTDA182I2SetPowerState(tUnit, ddPowerState);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "tmddTDA182I2SetPowerState(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* set power state */
			pObj->curPowerState = powerState;
		}

		(void)TDA182I2MutexRelease(pObj);
	}

	return err;
}

/* -------------------------------------------------------------------------
 * FUNCTION:    tmbslTDA182I2GetPowerState:
 *
 * DESCRIPTION: Get the power state of this device.
 *
 * RETURN:      TMBSL_ERR_TUNER_BAD_UNIT_NUMBER
 * TDA182I2_ERR_NOT_INITIALIZED
 * TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------- */
tmErrorCode_t tmbslTDA182I2GetPowerState(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	 tmTDA182I2PowerState_t *pPowerState)	/* O: Power state of device */
{
	ptmTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	if (pPowerState == Null)
		err = TDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* pObj initialization */
		err = TDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "TDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = TDA182I2MutexAcquire(pObj, TDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* get power state */
		*pPowerState = pObj->curPowerState;

		(void)TDA182I2MutexRelease(pObj);
	}

	return err;
}

/* --------------------------------------------------------------------------
 * FUNCTION:    tmbslTDA182I2SetStandardMode:
 *
 * DESCRIPTION: Set the standard mode of this device.
 *
 * RETURN:      TMBSL_ERR_TUNER_BAD_UNIT_NUMBER
 * TDA182I2_ERR_NOT_INITIALIZED
 * TM_OK
 *
 * NOTES:
 * -------------------------------------------------------------------------- */
tmErrorCode_t tmbslTDA182I2SetStandardMode(tmUnitSelect_t tUnit,/* I: Unit no.*/
	tmTDA182I2StandardMode_t StandardMode)	/* I: Standard mode of device */
{
	ptmTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* pObj initialization */
	err = TDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "TDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = TDA182I2MutexAcquire(pObj, TDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* store standard mode */
		pObj->StandardMode = StandardMode;

		(void)TDA182I2MutexRelease(pObj);
	}

	return err;
}

/* --------------------------------------------------------------------------
 * FUNCTION:    tmbslTDA182I2GetStandardMode:
 *
 * DESCRIPTION: Get the standard mode of this device.
 *
 * RETURN:      TMBSL_ERR_TUNER_BAD_UNIT_NUMBER
 * TDA182I2_ERR_NOT_INITIALIZED
 * TM_OK
 *
 * NOTES:
 * -------------------------------------------------------------------------- */
tmErrorCode_t tmbslTDA182I2GetStandardMode(tmUnitSelect_t tUnit,/* I: Unit no.*/
	tmTDA182I2StandardMode_t *pStandardMode)/* O: Standard mode of device */
{
	ptmTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	if (pStandardMode == Null)
		err = TDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* pObj initialization */
		err = TDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "TDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = TDA182I2MutexAcquire(pObj, TDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Get standard mode */
		*pStandardMode = pObj->StandardMode;

		(void)TDA182I2MutexRelease(pObj);
	}

	return err;
}

/* --------------------------------------------------------------------------
 * FUNCTION:    tmbslTDA182I2SetRf:
 *
 * DESCRIPTION: Calculate i2c I2CMap & write in TDA182I2
 *
 * RETURN:      TMBSL_ERR_TUNER_BAD_UNIT_NUMBER
 * TDA182I2_ERR_NOT_INITIALIZED
 * TDA182I2_ERR_BAD_PARAMETER
 * TMBSL_ERR_IIC_ERR
 * TM_OK
 *
 * NOTES:
 * -------------------------------------------------------------------------- */
/*  */
tmErrorCode_t tmbslTDA182I2SetRf(tmUnitSelect_t tUnit,	/* I: Unit number */
				 UInt32 uRF)	/* I: RF frequency in hertz */
{
	ptmTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;
	bool bIRQWait = True;
	UInt8 ratioL, ratioH;
	UInt32 DeltaL, DeltaH;
	UInt8 uAGC1_loop_off;
	UInt8 uRF_Filter_Gv = 0;

	/* ------------------------------ */
	/* test input parameters */
	/* ------------------------------ */
	/* pObj initialization */
	err = TDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "TDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = TDA182I2MutexAcquire(pObj, TDA182I2_MUTEX_TIMEOUT);

	if (err != TM_OK)
		return err;

	err = tmddTDA182I2GetIRQWait(tUnit, &bIRQWait);
	tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR,
		     "tmddTDA182I2GetIRQWait(0x%08X) failed.", tUnit));

	pObj->uRF = uRF;

	if (err == TM_OK) {
		/* Set LPF */
		err = tmddTDA182I2SetLP_FC(tUnit, pObj->Std_Array
				    [pObj->StandardMode].LPF);
		tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR,
			     "tmddTDA182I2SetLP_FC(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK) {
		/* Set LPF Offset */
		err = tmddTDA182I2SetLP_FC_Offset(tUnit, pObj->Std_Array
				    [pObj->StandardMode].LPF_Offset);
		tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR,
			     "tmddTDA182I2SetLP_FC_Offset(0x%08X) failed.",
			     tUnit));
	}

	if (err == TM_OK) {
		/* Set IF Gain */
		err = tmddTDA182I2SetIF_Level(tUnit, pObj->Std_Array
				    [pObj->StandardMode].IF_Gain);
		tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR,
			     "tmddTDA182I2SetIF_Level(0x%08X) failed.",
			     tUnit));
	}

	if (err == TM_OK) {
		/* Set IF Notch */
		err = tmddTDA182I2SetIF_ATSC_Notch(tUnit, pObj->Std_Array
				[pObj->StandardMode].IF_Notch);
		tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR,
			     "tmddTDA182I2SetIF_ATSC_Notch(0x%08X) failed.",
			     tUnit));
	}

	if (err == TM_OK) {
		/* Enable/disable HPF */
		if (pObj->Std_Array[pObj->StandardMode].IF_HPF ==
				tmTDA182I2_IF_HPF_Disabled) {
			err = tmddTDA182I2SetHi_Pass(tUnit, 0);
			tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR,
				    "tmddTDA182I2SetHi_Pass(0x%08X, 0) failed.",
				     tUnit));
		} else {
			err = tmddTDA182I2SetHi_Pass(tUnit, 1);
			tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR,
				    "tmddTDA182I2SetHi_Pass(0x%08X, 1) failed.",
				     tUnit));

			if (err == TM_OK) {
				/* Set IF HPF */
				err = tmddTDA182I2SetIF_HP_Fc(tUnit, (UInt8)
				     (pObj->Std_Array[pObj->StandardMode].IF_HPF
									 - 1));
				tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR,
				     "tmddTDA182I2SetIF_HP_Fc(0x%08X) failed.",
					     tUnit));
			}
		}
	}

	if (err == TM_OK) {
		/* Set DC Notch */
		err = tmddTDA182I2SetIF_Notch(tUnit, pObj->Std_Array
					    [pObj->StandardMode].DC_Notch);
		tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR,
			     "tmddTDA182I2SetIF_Notch(0x%08X) failed.",
			     tUnit));
	}

	if (err == TM_OK) {
		/* Set AGC1 LNA Top */
		err = tmddTDA182I2SetAGC1_TOP(tUnit, pObj->Std_Array
				[pObj->StandardMode].AGC1_LNA_TOP);
		tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR,
			     "tmddTDA182I2SetAGC1_TOP(0x%08X) failed.",
			     tUnit));
	}

	if (err == TM_OK) {
		/* Set AGC1 DO Step */
		err = tmddTDA182I2SetAGC1_Do_step(tUnit, pObj->Std_Array
					[pObj->StandardMode].AGC1_Do_step);
		tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR,
			     "tmddTDA182I2SetAGC1_Do_step(0x%08X) failed.",
			     tUnit));
	}

	if (err == TM_OK) {
		/* Set AGC2 RF Top */
		err = tmddTDA182I2SetAGC2_TOP(tUnit, pObj->Std_Array
				[pObj->StandardMode].AGC2_RF_Attenuator_TOP);
		tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR,
			     "tmddTDA182I2SetAGC2_TOP(0x%08X) failed.",
			     tUnit));
	}

	if (err == TM_OK) {
		/* Set AGC2 DO Step */
		err = tmddTDA182I2SetAGC2_Do_step(tUnit, pObj->Std_Array
				[pObj->StandardMode].AGC2_Do_step);
		tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR,
			     "tmddTDA182I2SetAGC2_Do_step(0x%08X) failed.",
			     tUnit));
	}

	if (err == TM_OK) {
		/* Set AGC3 RF AGC Top */
		if (pObj->uRF <
		    pObj->Std_Array[pObj->StandardMode].Freq_Start_LTE) {
			if (pObj->uRF < tmTDA182I2_AGC3_RF_AGC_TOP_FREQ_LIM) {
				err = tmddTDA182I2SetRFAGC_Top(tUnit,
					pObj->Std_Array[pObj->StandardMode].
					AGC3_RF_AGC_TOP_Low_band);
				tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR,
				    "tmddTDA182I2SetRFAGC_Top(0x%08X) failed.",
					     tUnit));
			} else {
				err = tmddTDA182I2SetRFAGC_Top(tUnit,
					pObj->Std_Array[pObj->StandardMode].
					AGC3_RF_AGC_TOP_High_band);
				tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR,
				     "tmddTDA182I2SetRFAGC_Top(0x%08X) failed.",
					     tUnit));
			}
		} else {	/* LTE settings */

			err = tmddTDA182I2SetRFAGC_Top(tUnit,
				pObj->Std_Array[pObj->StandardMode].
				AGC3_RF_AGC_TOP_LTE);
			tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR,
			     "tmddTDA182I2SetRFAGC_Adapt_TOP(0x%08X) failed.",
				     tUnit));
		}
	}

	if (err == TM_OK) {
		/* Set AGC4 IR Mixer Top */
		err = tmddTDA182I2SetIR_Mixer_Top(tUnit,
			pObj->Std_Array[pObj->StandardMode].AGC4_IR_Mixer_TOP);
		tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR,
		     "tmddTDA182I2SetIR_Mixer_Top(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK) {
		/* Set AGC5 IF AGC Top */
		err = tmddTDA182I2SetAGC5_TOP(tUnit,
			pObj->Std_Array[pObj->StandardMode].AGC5_IF_AGC_TOP);
		tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR,
		    "tmddTDA182I2SetAGC5_TOP(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK) {
		/* Set AGC3 Adapt */
		err = tmddTDA182I2SetPD_RFAGC_Adapt(tUnit,
			  pObj->Std_Array[pObj->StandardMode].AGC3_Adapt);
		tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR,
		     "tmddTDA182I2SetPD_RFAGC_Adapt(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK) {
		/* Set AGC3 RF AGC Top */
		if (pObj->uRF <
		    pObj->Std_Array[pObj->StandardMode].Freq_Start_LTE) {
			err = tmddTDA182I2SetRFAGC_Adapt_TOP(tUnit,
			    pObj->Std_Array[pObj->StandardMode].AGC3_Adapt_TOP);
			tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR,
			     "tmddTDA182I2SetRFAGC_Adapt_TOP(0x%08X) failed.",
				     tUnit));
		} else {	/* LTE settings */
			err = tmddTDA182I2SetRFAGC_Adapt_TOP(tUnit,
				pObj->Std_Array[pObj->StandardMode].
				AGC3_Adapt_TOP_LTE);
			tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR,
			     "tmddTDA182I2SetRFAGC_Adapt_TOP(0x%08X) failed.",
				     tUnit));
		}
	}

	if (err == TM_OK) {
		/* Set AGC5 Atten 3dB */
		err = tmddTDA182I2SetRF_Atten_3dB(tUnit,
			pObj->Std_Array[pObj->StandardMode].AGC5_Atten_3dB);
		tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR,
		     "tmddTDA182I2SetRF_Atten_3dB(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK) {
		/* Set AGC5 Detector HPF */
		err = tmddTDA182I2SetAGC5_Ana(tUnit,
			pObj->Std_Array[pObj->StandardMode].AGC5_Detector_HPF);
		tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR,
		     "tmddTDA182I2SetAGC5_Ana(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK) {
		/* Set AGCK Mode */
		err = tmddTDA182I2SetAGCK_Mode(tUnit,
			pObj->Std_Array[pObj->StandardMode].GSK & 0x03);
		tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR,
		     "tmddTDA182I2SetAGCK_Mode(0x%08X) failed.", tUnit));

		err = tmddTDA182I2SetAGCK_Step(tUnit,
		     (pObj->Std_Array[pObj->StandardMode].GSK & 0x0C) >> 2);
		tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR,
		     "tmddTDA182I2SetAGCK_Step(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK) {
		/* Set H3H5 VHF Filter 6 */
		err = tmddTDA182I2SetPSM_StoB(tUnit,
			pObj->Std_Array[pObj->StandardMode].H3H5_VHF_Filter6);
		tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR,
		     "tmddTDA182I2SetPSM_StoB(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK) {
		/* Set IF */
		err = tmddTDA182I2SetIF_Freq(tUnit,
			pObj->Std_Array[pObj->StandardMode].IF -
			pObj->Std_Array[pObj->StandardMode].CF_Offset);
		tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR,
		     "tmddTDA182I2SetIF_Freq(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK) {
		/* Set IF */
		err = tmddTDA182I2SetPD_Udld(tUnit,
			pObj->Std_Array[pObj->StandardMode].PD_Udld);
		tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR,
		     "tmddTDA182I2SetPD_Udld(0x%08X) failed.", tUnit));
	}

	if ((pObj->Std_Array[pObj->StandardMode].LTO_STO_immune
	    && pObj->Master) && (err == TM_OK)) {
		/* save RF_Filter_Gv current value */
		err = tmddTDA182I2GetAGC2_Gain_Read(tUnit, &uRF_Filter_Gv);
		tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR,
		     "tmddTDA182I2GetRF_Filter_Gv(0x%08X) failed.", tUnit));
		if (err == TM_OK)
			err = tmddTDA182I2SetRF_Filter_Gv(tUnit, uRF_Filter_Gv);
		if (err == TM_OK)
			err = tmddTDA182I2SetForce_AGC2_gain(tUnit, 0x1);
		/* smooth RF_Filter_Gv to min value */
		if ((err == TM_OK) && (uRF_Filter_Gv != 0)) {
			do {
				err = tmddTDA182I2SetRF_Filter_Gv(tUnit,
							uRF_Filter_Gv - 1);
				tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR,
				  "tmddTDA182I2GetRF_Filter_Gv(0x%08X) failed.",
					     tUnit));
				if (err == TM_OK) {
					err = TDA182I2Wait(pObj, 10);
					tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR,
					     "tmddTDA182I2SetRF_Filter_Cap(0x%08X) failed.",
						     tUnit));
				}
				uRF_Filter_Gv = uRF_Filter_Gv - 1;
			} while (uRF_Filter_Gv > 0);
		}
		if (err == TM_OK) {
			err = tmddTDA182I2SetRF_Atten_3dB(tUnit, 0x01);
			tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR,
			     "tmddTDA182I2GetRF_Filter_Gv(0x%08X) failed.",
				     tUnit));
		}
	}
	if (err == TM_OK) {
		/* Set RF */
		err = tmddTDA182I2SetRF_Freq(tUnit,
			uRF + pObj->Std_Array[pObj->StandardMode].CF_Offset);
		tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR,
			     "tmddTDA182I2SetRF_Freq(0x%08X) failed.", tUnit));

	}

	if (pObj->Std_Array[pObj->StandardMode].LTO_STO_immune
	    && pObj->Master) {

		if (err == TM_OK) {
			err = tmddTDA182I2SetRF_Atten_3dB(tUnit, 0x00);
			tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR,
			     "tmddTDA182I2GetRF_Filter_Gv(0x%08X) failed.",
				     tUnit));
		}
		err = TDA182I2Wait(pObj, 50);
		if (err == TM_OK)
			tmddTDA182I2SetForce_AGC2_gain(tUnit, 0x0);
	}

	if (err == TM_OK) {
		/*  Spurious reduction begin */
		ratioL = (UInt8) (uRF / 16000000);
		ratioH = (UInt8) (uRF / 16000000) + 1;
		DeltaL = (uRF - (ratioL * 16000000));
		DeltaH = ((ratioH * 16000000) - uRF);

		if (uRF < 72000000) {
			/* Set sigma delta clock */
			err = tmddTDA182I2SetDigital_Clock_Mode(tUnit, 1);
			tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR,
			     "tmddTDA182I2SetDigital_Clock_Mode(0x%08X, sigma delta clock) failed.",
				     tUnit));
		} else if (uRF < 104000000) {
			/* Set 16 Mhz Xtal clock */
			err = tmddTDA182I2SetDigital_Clock_Mode(tUnit, 0);
			tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR,
			     "tmddTDA182I2SetDigital_Clock_Mode(0x%08X, 16 Mhz xtal clock) failed.",
				     tUnit));
		} else if (uRF <= 120000000) {
			/* Set sigma delta clock */
			err = tmddTDA182I2SetDigital_Clock_Mode(tUnit, 1);
			tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR,
			     "tmddTDA182I2SetDigital_Clock_Mode(0x%08X, sigma delta clock) failed.",
				     tUnit));
		} else {	/* RF above 120 MHz */

			if (DeltaL <= DeltaH) {
				if (ratioL & 0x000001) {	/* ratioL odd */
					/* Set 16 Mhz Xtal clock */
					err = tmddTDA182I2SetDigital_Clock_Mode
					    (tUnit, 0);
					tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR,
					     "tmddTDA182I2SetDigital_Clock_Mode(0x%08X, 16 Mhz xtal clock) failed.",
						     tUnit));
				} else {	/* ratioL even */

					/* Set sigma delta clock */
					err = tmddTDA182I2SetDigital_Clock_Mode
					    (tUnit, 1);
					tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR,
					     "tmddTDA182I2SetDigital_Clock_Mode(0x%08X, sigma delta clock) failed.",
						     tUnit));
				}

			} else {	/* (DeltaL > DeltaH ) */

				if (ratioL & 0x000001) {	/*(ratioL odd)*/
					/* Set sigma delta clock */
					err = tmddTDA182I2SetDigital_Clock_Mode
					    (tUnit, 1);
					tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR,
					     "tmddTDA182I2SetDigital_Clock_Mode(0x%08X, sigma delta clock) failed.",
						     tUnit));
				} else {
					/* Set 16 Mhz Xtal clock */
					err = tmddTDA182I2SetDigital_Clock_Mode
					    (tUnit, 0);
					tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR,
					     "tmddTDA182I2SetDigital_Clock_Mode(0x%08X, 16 Mhz xtal clock) failed.",
						     tUnit));
				}
			}
		}
	}
	/*  Spurious reduction end */

	if (err == TM_OK) {
		if (pObj->Std_Array[pObj->StandardMode].AGC1_Freeze == True) {
			err = tmddTDA182I2GetAGC1_loop_off(tUnit,
							 &uAGC1_loop_off);

			if (uAGC1_loop_off == 0) {
				/* first AGC1 freeze */
				err = tmddTDA182I2SetAGC1_loop_off(tUnit, 0x1);
				if (err == TM_OK)
					err =
					   tmddTDA182I2SetForce_AGC1_gain(tUnit,
								0x1);
			}
			if (err == TM_OK)
				/* Adapt AGC1gain from Last SetRF */
				err = tmddTDA182I2AGC1_Adapt(tUnit);
		} else {
			err = tmddTDA182I2SetForce_AGC1_gain(tUnit, 0x0);
			if (err == TM_OK)
				err = tmddTDA182I2SetAGC1_loop_off(tUnit, 0x0);
		}
	}

	(void)TDA182I2MutexRelease(pObj);

	return err;
}

/* --------------------------------------------------------------------------
 * FUNCTION:    tmbslTDA182I2GetRf:
 *
 * DESCRIPTION: Get the frequency programmed in the tuner
 *
 * RETURN:      TMBSL_ERR_TUNER_BAD_UNIT_NUMBER
 * TDA182I2_ERR_NOT_INITIALIZED
 * TM_OK
 *
 * NOTES:       The value returned is the one stored in the Object
 * -------------------------------------------------------------------------- */
tmErrorCode_t tmbslTDA182I2GetRf(tmUnitSelect_t tUnit,	/* I: Unit number */
				 UInt32 *puRF)	/* O: RF frequency in hertz */
{
	ptmTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	if (puRF == Null)
		err = TDA182I2_ERR_BAD_PARAMETER;

	/* ------------------------------ */
	/* test input parameters */
	/* ------------------------------ */
	/* pObj initialization */
	if (err == TM_OK) {
		err = TDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "TDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = TDA182I2MutexAcquire(pObj, TDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Get RF */
		*puRF =
		    pObj->uRF
		    /* - pObj->Std_Array[pObj->StandardMode].CF_Offset */ ;

		(void)TDA182I2MutexRelease(pObj);
	}
	return err;
}

/*============================================================================*/
/* tmbslTDA182I2Reset                                                         */
/*============================================================================*/
tmErrorCode_t tmbslTDA182I2Reset(tmUnitSelect_t tUnit)	/* I: Unit number */
{
	ptmTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;
	bool bIRQWait = True;

	/* ------------------------------ */
	/* test input parameters */
	/* ------------------------------ */
	/* pObj initialization */
	err = TDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "TDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = TDA182I2MutexAcquire(pObj, TDA182I2_MUTEX_TIMEOUT);

	if (err != TM_OK)
		return err;

	err = tmddTDA182I2GetIRQWait(tUnit, &bIRQWait);
	tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR,
		     "tmddTDA182I2GetIRQWait(0x%08X) failed.", tUnit));

	if (err != TM_OK)
		goto ret_err;

	err = TDA182I2Init(tUnit);
	tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR,
		     "TDA182I2Init(0x%08X) failed.", tUnit));

	if (err != TM_OK)
		goto ret_err;

	err = tmddTDA182I2SetXTout(tUnit, 3);
	tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR,
		     "TDA182I2Init(0x%08X) failed.", tUnit));

	if (err != TM_OK)
		goto ret_err;

	/* Wait for  XtalCal End ( Master only ) */
	if (pObj->Master) {
		err = TDA182I2WaitXtalCal_End(pObj, 100, 5);
	} else {
		/* Initialize Fmax_LO and N_CP_Current */
		err = tmddTDA182I2SetFmax_Lo(tUnit, 0x00);
		tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR,
		     "tmddTDA182I2SetFmax_Lo(0x%08X, 0x0A) failed.", tUnit));

		if (err == TM_OK) {
			err = tmddTDA182I2SetN_CP_Current(tUnit, 0x68);
			tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR,
			    "tmddTDA182I2SetN_CP_Current(0x%08X, 0x68) failed.",
				     tUnit));
		}
	}
	if (err != TM_OK)
		goto ret_err;

	/* initialize tuner */
	err = tmddTDA182I2Reset(tUnit);
	tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR,
		     "tmddTDA182I2Reset(0x%08X) failed.", tUnit));
	if (err != TM_OK)
		goto ret_err;

	/* Initialize Fmax_LO and N_CP_Current */
	err = tmddTDA182I2SetFmax_Lo(tUnit, 0x0A);
	tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR,
		     "tmddTDA182I2SetFmax_Lo(0x%08X, 0x0A) failed.", tUnit));
	if (err != TM_OK)
		goto ret_err;

	err = tmddTDA182I2SetLT_Enable(tUnit, pObj->LT_Enable);
	tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR,
			"tmddTDA182I2SetLT_Enable(0x%08X, 0) failed.", tUnit));
	if (err != TM_OK)
		goto ret_err;

	err = tmddTDA182I2SetPSM_AGC1(tUnit, pObj->PSM_AGC1);
	tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR,
		     "tmddTDA182I2SetPSM_AGC1(0x%08X, 1) failed.", tUnit));
	if (err != TM_OK)
		goto ret_err;

	err = tmddTDA182I2SetAGC1_6_15dB(tUnit, pObj->AGC1_6_15dB);
	tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR,
		     "tmddTDA182I2SetAGC1_6_15dB(0x%08X, 0) failed.", tUnit));
ret_err:
	(void)TDA182I2MutexRelease(pObj);

	return err;
}

/*============================================================================*/
/* tmbslTDA182I2GetIF                                                         */
/*============================================================================*/

tmErrorCode_t tmbslTDA182I2GetIF(tmUnitSelect_t tUnit,	/* I: Unit number */
				 UInt32 *puIF)	/* O: IF Frequency in hertz */
{
	ptmTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	if (puIF == Null)
		err = TDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		err = TDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "TDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = TDA182I2MutexAcquire(pObj, TDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		*puIF =
		    pObj->Std_Array[pObj->StandardMode].IF -
		    pObj->Std_Array[pObj->StandardMode].CF_Offset;

		(void)TDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* tmbslTDA182I2GetCF_Offset                                                  */
/*============================================================================*/

tmErrorCode_t tmbslTDA182I2GetCF_Offset(tmUnitSelect_t tUnit,	/* I: Unit no.*/
			UInt32 *puOffset)/*O: Center frequency offset in hertz*/
{
	ptmTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	if (puOffset == Null)
		err = TDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		err = TDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "TDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = TDA182I2MutexAcquire(pObj, TDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		*puOffset = pObj->Std_Array[pObj->StandardMode].CF_Offset;

		(void)TDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* tmbslTDA182I2GetLockStatus                                                 */
/*============================================================================*/

tmErrorCode_t tmbslTDA182I2GetLockStatus(tmUnitSelect_t tUnit,	/* I: Unit no.*/
		 tmbslFrontEndState_t *pLockStatus)/* O: PLL Lock status */
{
	ptmTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;
	UInt8 uValue, uValueLO;

	if (pLockStatus == Null)
		err = TDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		err = TDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "TDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK) {
		err = TDA182I2MutexAcquire(pObj, TDA182I2_MUTEX_TIMEOUT);

		if (err == TM_OK) {
			err = tmddTDA182I2GetLO_Lock(tUnit, &uValueLO);
			tmASSERTExT(err, TM_OK,
				    (DEBUGLVL_ERROR,
				     "tmddTDA182I2GetLO_Lock(0x%08X) failed.",
				     tUnit));
		}
		if (err == TM_OK) {
			err = tmddTDA182I2GetIRQ_status(tUnit, &uValue);
			tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR,
			     "tmddTDA182I2GetIRQ_status(0x%08X) failed.",
			     tUnit));

			uValue = uValue & uValueLO;
		}
		if (err == TM_OK)
			*pLockStatus = (uValue) ? tmbslFrontEndStateLocked :
						tmbslFrontEndStateNotLocked;
		else
			*pLockStatus = tmbslFrontEndStateUnknown;

		(void)TDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* tmbslTDA182I2GetPowerLevel                                                 */
/*============================================================================*/

tmErrorCode_t tmbslTDA182I2GetPowerLevel(tmUnitSelect_t tUnit,	/* I: Unit no.*/
				 UInt32 *pPowerLevel)/*O: Power Level in dBµV */
{
	ptmTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	if (pPowerLevel == Null)
		err = TDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		err = TDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "TDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = TDA182I2MutexAcquire(pObj, TDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		*pPowerLevel = 0;

		err = tmddTDA182I2GetPower_Level(tUnit, (UInt8 *) pPowerLevel);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "tmddTDA182I2GetPower_Level(0x%08X) failed.",
			     tUnit));

		(void)TDA182I2MutexRelease(pObj);
	}
	return err;
}

/*============================================================================*/
/* tmbslTDA182I2SetIRQWait                                                  */
/*============================================================================*/

tmErrorCode_t tmbslTDA182I2SetIRQWait(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	Bool bWait/* I: Determine if we need to wait IRQ in driver functions */)
{
	ptmTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	err = TDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "TDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = TDA182I2MutexAcquire(pObj, TDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		err = tmddTDA182I2SetIRQWait(tUnit, bWait);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "tmddTDA182I2SetIRQWait(0x%08X) failed.", tUnit));

		(void)TDA182I2MutexRelease(pObj);
	}
	return err;
}

/*============================================================================*/
/* tmbslTDA182I2GetIRQWait                                                  */
/*============================================================================*/

tmErrorCode_t tmbslTDA182I2GetIRQWait(tmUnitSelect_t tUnit,	/* I: Unit no.*/
	bool *pbWait/* O:Determine if we need to wait IRQ in driver functions*/)
{
	ptmTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	if (pbWait == Null)
		err = TDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		err = TDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "TDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = TDA182I2MutexAcquire(pObj, TDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		err = tmddTDA182I2GetIRQWait(tUnit, pbWait);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "tmddTDA182I2GetIRQWait(0x%08X) failed.", tUnit));

		(void)TDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* tmbslTDA182I2GetIRQ                                                        */
/*============================================================================*/

tmErrorCode_t tmbslTDA182I2GetIRQ(tmUnitSelect_t tUnit /* I: Unit number */ ,
				  bool *pbIRQ)	/* O: IRQ triggered */
{
	ptmTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	if (pbIRQ == Null)
		err = TDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		err = TDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "TDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = TDA182I2MutexAcquire(pObj, TDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		*pbIRQ = 0;

		err = tmddTDA182I2GetIRQ_status(tUnit, (UInt8 *) pbIRQ);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "tmddTDA182I2GetIRQ_status(0x%08X) failed.",
			     tUnit));

		(void)TDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* tmbslTDA182I2WaitIRQ                                                       */
/*============================================================================*/

tmErrorCode_t tmbslTDA182I2WaitIRQ(tmUnitSelect_t tUnit,/* I: Unit number */
				   UInt32 timeOut,/* I: timeOut for IRQ wait */
				   UInt32 waitStep,	/* I: wait step */
				   UInt8 irqStatus)	/* I: IRQs to wait */
{
	ptmTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	if (err == TM_OK) {
		err = TDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "TDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = TDA182I2MutexAcquire(pObj, TDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		err = tmddTDA182I2WaitIRQ(tUnit, timeOut, waitStep, irqStatus);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "tmddTDA182I2WaitIRQ(0x%08X) failed.", tUnit));

		(void)TDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* tmbslTDA182I2GetXtalCal_End                                                */
/*============================================================================*/

tmErrorCode_t
tmbslTDA182I2GetXtalCal_End(tmUnitSelect_t tUnit /* I: Unit number */ ,
			    bool *pbXtalCal_End)/* O: XtalCal_End triggered */
{
	ptmTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	if (pbXtalCal_End == Null)
		err = TDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		err = TDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "TDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = TDA182I2MutexAcquire(pObj, TDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		*pbXtalCal_End = 0;

		err =
		    tmddTDA182I2GetMSM_XtalCal_End(tUnit,
						   (UInt8 *) pbXtalCal_End);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "tmddTDA182I2GetMSM_XtalCal_End(0x%08X) failed.",
			     tUnit));

		(void)TDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* tmbslTDA182I2WaitXtalCal_End                                               */
/*============================================================================*/

tmErrorCode_t tmbslTDA182I2WaitXtalCal_End(tmUnitSelect_t tUnit,/* I: Unit no.*/
				   UInt32 timeOut,/* I: timeOut for IRQ wait */
				   UInt32 waitStep)/* I: wait step */
{
	ptmTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	if (err == TM_OK) {
		err = TDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "TDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = TDA182I2MutexAcquire(pObj, TDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		err = tmddTDA182I2WaitXtalCal_End(tUnit, timeOut, waitStep);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "tmddTDA182I2WaitXtalCal_End(0x%08X) failed.",
			     tUnit));

		(void)TDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* tmbslTDA182I2CheckRFFilterRobustness                                       */
/*============================================================================*/
#if 0
tmErrorCode_t
tmbslTDA182I2CheckRFFilterRobustness(tmUnitSelect_t tUnit,/* I: Unit number */
	   ptmTDA182I2RFFilterRating rating)	/* O: RF Filter rating */
{
	ptmTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	if (err == TM_OK) {
		err = TDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "TDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = TDA182I2MutexAcquire(pObj, TDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		UInt8 rfcal_log_0 = 0;
		UInt8 rfcal_log_2 = 0;
		UInt8 rfcal_log_3 = 0;
		UInt8 rfcal_log_5 = 0;
		UInt8 rfcal_log_6 = 0;
		UInt8 rfcal_log_8 = 0;
		UInt8 rfcal_log_9 = 0;
		UInt8 rfcal_log_11 = 0;

		double VHFLow_0 = 0.0;
		double VHFLow_1 = 0.0;
		double VHFHigh_0 = 0.0;
		double VHFHigh_1 = 0.0;
		double UHFLow_0 = 0.0;
		double UHFLow_1 = 0.0;
		double UHFHigh_0 = 0.0;
		double UHFHigh_1 = 0.0;

		err = tmddTDA182I2Getrfcal_log_0(tUnit, &rfcal_log_0);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "tmddTDA182I2Getrfcal_log_0(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			err = tmddTDA182I2Getrfcal_log_2(tUnit, &rfcal_log_2);
			tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR,
			     "tmddTDA182I2Getrfcal_log_2(0x%08X) failed.",
				     tUnit));
		}

		if (err == TM_OK) {
			err = tmddTDA182I2Getrfcal_log_3(tUnit, &rfcal_log_3);
			tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR,
			     "tmddTDA182I2Getrfcal_log_3(0x%08X) failed.",
				     tUnit));
		}

		if (err == TM_OK) {
			err = tmddTDA182I2Getrfcal_log_5(tUnit, &rfcal_log_5);
			tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR,
			     "tmddTDA182I2Getrfcal_log_5(0x%08X) failed.",
				     tUnit));
		}

		if (err == TM_OK) {
			err = tmddTDA182I2Getrfcal_log_6(tUnit, &rfcal_log_6);
			tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR,
			     "tmddTDA182I2Getrfcal_log_6(0x%08X) failed.",
				     tUnit));
		}

		if (err == TM_OK) {
			err = tmddTDA182I2Getrfcal_log_8(tUnit, &rfcal_log_8);
			tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR,
			     "tmddTDA182I2Getrfcal_log_8(0x%08X) failed.",
				     tUnit));
		}

		if (err == TM_OK) {
			err = tmddTDA182I2Getrfcal_log_9(tUnit, &rfcal_log_9);
			tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR,
			     "tmddTDA182I2Getrfcal_log_9(0x%08X) failed.",
				     tUnit));
		}

		if (err == TM_OK) {
			err = tmddTDA182I2Getrfcal_log_11(tUnit, &rfcal_log_11);
			tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR,
			     "tmddTDA182I2Getrfcal_log_11(0x%08X) failed.",
				     tUnit));
		}

		if (err == TM_OK) {
			if (rfcal_log_0 & 0x80) {
				rating->VHFLow_0_Margin = 0;
				rating->VHFLow_0_RFFilterRobustness =
				    tmTDA182I2RFFilterRobustness_Error;
			} else {
				/* VHFLow_0 */
				VHFLow_0 =
				    100 * (45 -
					   39.8225 * (1 +
						      (0.31 *
						       (rfcal_log_0 <
							64 ? rfcal_log_0 :
							rfcal_log_0 -
							128)) / 1.0 / 100.0)) /
				    45.0;
				rating->VHFLow_0_Margin =
				    0.0024 * VHFLow_0 * VHFLow_0 * VHFLow_0 -
				    0.101 * VHFLow_0 * VHFLow_0 +
				    1.629 * VHFLow_0 + 1.8266;
				if (rating->VHFLow_0_Margin >= 0) {
					rating->VHFLow_0_RFFilterRobustness =
					    tmTDA182I2RFFilterRobustness_High;
				} else {
					rating->VHFLow_0_RFFilterRobustness =
					    tmTDA182I2RFFilterRobustness_Low;
				}
			}

			if (rfcal_log_2 & 0x80) {
				rating->VHFLow_1_Margin = 0;
				rating->VHFLow_1_RFFilterRobustness =
				    tmTDA182I2RFFilterRobustness_Error;
			} else {
				/* VHFLow_1 */
				VHFLow_1 =
				    100 * (152.1828 *
					   (1 +
					    (1.53 *
					     (rfcal_log_2 <
					      64 ? rfcal_log_2 : rfcal_log_2 -
					      128)) / 1.0 / 100.0) - (144.896 -
								      6)) /
				    (144.896 - 6);
				rating->VHFLow_1_Margin =
				    0.0024 * VHFLow_1 * VHFLow_1 * VHFLow_1 -
				    0.101 * VHFLow_1 * VHFLow_1 +
				    1.629 * VHFLow_1 + 1.8266;
				if (rating->VHFLow_1_Margin >= 0) {
					rating->VHFLow_1_RFFilterRobustness =
					    tmTDA182I2RFFilterRobustness_High;
				} else {
					rating->VHFLow_1_RFFilterRobustness =
					    tmTDA182I2RFFilterRobustness_Low;
				}
			}

			if (rfcal_log_3 & 0x80) {
				rating->VHFHigh_0_Margin = 0;
				rating->VHFHigh_0_RFFilterRobustness =
				    tmTDA182I2RFFilterRobustness_Error;
			} else {
				/* VHFHigh_0 */
				VHFHigh_0 =
				    100 * ((144.896 + 6) -
					   135.4063 * (1 +
						       (0.27 *
							(rfcal_log_3 <
							 64 ? rfcal_log_3 :
							 rfcal_log_3 -
							 128)) / 1.0 / 100.0)) /
				    (144.896 + 6);
				rating->VHFHigh_0_Margin =
				    0.0024 * VHFHigh_0 * VHFHigh_0 * VHFHigh_0 -
				    0.101 * VHFHigh_0 * VHFHigh_0 +
				    1.629 * VHFHigh_0 + 1.8266;
				if (rating->VHFHigh_0_Margin >= 0) {
					rating->VHFHigh_0_RFFilterRobustness =
					    tmTDA182I2RFFilterRobustness_High;
				} else {
					rating->VHFHigh_0_RFFilterRobustness =
					    tmTDA182I2RFFilterRobustness_Low;
				}
			}

			if (rfcal_log_5 & 0x80) {
				rating->VHFHigh_1_Margin = 0;
				rating->VHFHigh_1_RFFilterRobustness =
				    tmTDA182I2RFFilterRobustness_Error;
			} else {
				/* VHFHigh_1 */
				VHFHigh_1 =
				    100 * (383.1455 *
					   (1 +
					    (0.91 *
					     (rfcal_log_5 <
					      64 ? rfcal_log_5 : rfcal_log_5 -
					      128)) / 1.0 / 100.0) - (367.104 -
								      8)) /
				    (367.104 - 8);
				rating->VHFHigh_1_Margin =
				    0.0024 * VHFHigh_1 * VHFHigh_1 * VHFHigh_1 -
				    0.101 * VHFHigh_1 * VHFHigh_1 +
				    1.629 * VHFHigh_1 + 1.8266;
				if (rating->VHFHigh_1_Margin >= 0) {
					rating->VHFHigh_1_RFFilterRobustness =
					    tmTDA182I2RFFilterRobustness_High;
				} else {
					rating->VHFHigh_1_RFFilterRobustness =
					    tmTDA182I2RFFilterRobustness_Low;
				}
			}

			if (rfcal_log_6 & 0x80) {
				rating->UHFLow_0_Margin = 0;
				rating->UHFLow_0_RFFilterRobustness =
				    tmTDA182I2RFFilterRobustness_Error;
			} else {
				/* UHFLow_0 */
				UHFLow_0 =
				    100 * ((367.104 + 8) -
					   342.6224 * (1 +
						       (0.21 *
							(rfcal_log_6 <
							 64 ? rfcal_log_6 :
							 rfcal_log_6 -
							 128)) / 1.0 / 100.0)) /
				    (367.104 + 8);
				rating->UHFLow_0_Margin =
				    0.0024 * UHFLow_0 * UHFLow_0 * UHFLow_0 -
				    0.101 * UHFLow_0 * UHFLow_0 +
				    1.629 * UHFLow_0 + 1.8266;
				if (rating->UHFLow_0_Margin >= 0) {
					rating->UHFLow_0_RFFilterRobustness =
					    tmTDA182I2RFFilterRobustness_High;
				} else {
					rating->UHFLow_0_RFFilterRobustness =
					    tmTDA182I2RFFilterRobustness_Low;
				}
			}

			if (rfcal_log_8 & 0x80) {
				rating->UHFLow_1_Margin = 0;
				rating->UHFLow_1_RFFilterRobustness =
				    tmTDA182I2RFFilterRobustness_Error;
			} else {
				/* UHFLow_1 */
				UHFLow_1 =
				    100 * (662.5595 *
					   (1 +
					    (0.33 *
					     (rfcal_log_8 <
					      64 ? rfcal_log_8 : rfcal_log_8 -
					      128)) / 1.0 / 100.0) - (624.128 -
								      2)) /
				    (624.128 - 2);
				rating->UHFLow_1_Margin =
				    0.0024 * UHFLow_1 * UHFLow_1 * UHFLow_1 -
				    0.101 * UHFLow_1 * UHFLow_1 +
				    1.629 * UHFLow_1 + 1.8266;
				if (rating->UHFLow_1_Margin >= 0) {
					rating->UHFLow_1_RFFilterRobustness =
					    tmTDA182I2RFFilterRobustness_High;
				} else {
					rating->UHFLow_1_RFFilterRobustness =
					    tmTDA182I2RFFilterRobustness_Low;
				}
			}

			if (rfcal_log_9 & 0x80) {
				rating->UHFHigh_0_Margin = 0;
				rating->UHFHigh_0_RFFilterRobustness =
				    tmTDA182I2RFFilterRobustness_Error;
			} else {
				/* UHFHigh_0 */
				UHFHigh_0 =
				    100 * ((624.128 + 2) -
					   508.2747 * (1 +
						       (0.23 *
							(rfcal_log_9 <
							 64 ? rfcal_log_9 :
							 rfcal_log_9 -
							 128)) / 1.0 / 100.0)) /
				    (624.128 + 2);
				rating->UHFHigh_0_Margin =
				    0.0024 * UHFHigh_0 * UHFHigh_0 * UHFHigh_0 -
				    0.101 * UHFHigh_0 * UHFHigh_0 +
				    1.629 * UHFHigh_0 + 1.8266;
				if (rating->UHFHigh_0_Margin >= 0) {
					rating->UHFHigh_0_RFFilterRobustness =
					    tmTDA182I2RFFilterRobustness_High;
				} else {
					rating->UHFHigh_0_RFFilterRobustness =
					    tmTDA182I2RFFilterRobustness_Low;
				}
			}

			if (rfcal_log_11 & 0x80) {
				rating->UHFHigh_1_Margin = 0;
				rating->UHFHigh_1_RFFilterRobustness =
				    tmTDA182I2RFFilterRobustness_Error;
			} else {
				/* UHFHigh_1 */
				UHFHigh_1 =
				    100 * (947.8913 *
					   (1 +
					    (0.3 *
					     (rfcal_log_11 <
					      64 ? rfcal_log_11 : rfcal_log_11 -
					      128)) / 1.0 / 100.0) - (866 -
								      14)) /
				    (866 - 14);
				rating->UHFHigh_1_Margin =
				    0.0024 * UHFHigh_1 * UHFHigh_1 * UHFHigh_1 -
				    0.101 * UHFHigh_1 * UHFHigh_1 +
				    1.629 * UHFHigh_1 + 1.8266;
				if (rating->UHFHigh_1_Margin >= 0) {
					rating->UHFHigh_1_RFFilterRobustness =
					    tmTDA182I2RFFilterRobustness_High;
				} else {
					rating->UHFHigh_1_RFFilterRobustness =
					    tmTDA182I2RFFilterRobustness_Low;
				}
			}
		}

		(void)TDA182I2MutexRelease(pObj);
	}

	return err;
}
#endif
/*============================================================================*/
/* tmbslTDA182I2Write                                                         */
/*============================================================================*/

tmErrorCode_t tmbslTDA182I2Write(tmUnitSelect_t tUnit,	/* I: Unit number */
				 UInt32 uIndex,	/* I: Start index to write */
				 UInt32 WriteLen,/* I: No. of bytes to write */
				 UInt8 *pData	/* I: Data to write */)
{
	ptmTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* pObj initialization */
	err = TDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "TDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = TDA182I2MutexAcquire(pObj, TDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Call tmddTDA182I2Write */
		(void)TDA182I2MutexRelease(pObj);
	}

	return err;
}

/* -------------------------------------------------------------------------- */
/* Internal functions: */
/* -------------------------------------------------------------------------- */

/* --------------------------------------------------------------------------
 * FUNCTION:    TDA182I2Init:
 *
 * DESCRIPTION: initialization of the Tuner
 *
 * RETURN:      always True
 *
 * NOTES:
 * -------------------------------------------------------------------------- */
static tmErrorCode_t TDA182I2Init(tmUnitSelect_t tUnit)	/* I: Unit number */
{
	ptmTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* ------------------------------ */
	/* test input parameters */
	/* ------------------------------ */
	/* pObj initialization */
	err = TDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "TDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK) {
		/* err = tmddTDA182I2SetIRQWait(tUnit, True); */
		/* if (pObj->bIRQWait) */
		/* { */
		/* err = TDA182I2WaitIRQ(pObj); */
		/* } */
	}

	return err;
}

/* -------------------------------------------------------------------------
 * FUNCTION:    TDA182I2Wait
 *
 * DESCRIPTION: This function waits for requested time
 *
 * RETURN:      True or False
 *
 * NOTES:
 * ------------------------------------------------------------------------- */
static tmErrorCode_t TDA182I2Wait(ptmTDA182I2Object_t pObj,/* I: Driver object*/
				  UInt32 Time	/*  I: Time to wait for */)
{
	tmErrorCode_t err = TM_OK;

	/* wait Time ms */
	err = POBJ_SRVFUNC_STIME.Wait(pObj->tUnit, Time);

	/* Return value */
	return err;
}

/*============================================================================*/
/* TDA182I2MutexAcquire                                                       */
/*============================================================================*/
extern tmErrorCode_t
TDA182I2MutexAcquire(ptmTDA182I2Object_t pObj, UInt32 timeOut)
{
	tmErrorCode_t err = TM_OK;

	if (pObj->sMutex.Acquire != Null && pObj->pMutex != Null)
		err = pObj->sMutex.Acquire(pObj->pMutex, timeOut);

	return err;
}

/*============================================================================*/
/* TDA182I2MutexRelease                                                       */
/*============================================================================*/
extern tmErrorCode_t TDA182I2MutexRelease(ptmTDA182I2Object_t pObj)
{
	tmErrorCode_t err = TM_OK;

	if (pObj->sMutex.Release != Null && pObj->pMutex != Null)
		err = pObj->sMutex.Release(pObj->pMutex);

	return err;
}

/*============================================================================*/
/* FUNCTION:    ddTDA182I2WaitXtalCal_End                                     */
/*                                                                            */
/* DESCRIPTION: Wait for MSM_XtalCal_End to trigger                           */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
static tmErrorCode_t
TDA182I2WaitXtalCal_End(ptmTDA182I2Object_t pObj,/* I: Instance object */
					     UInt32 timeOut,	/* I: timeout */
					     UInt32 waitStep)/* I: wait step */
{
	tmErrorCode_t err = TM_OK;
	UInt32 counter = timeOut / waitStep;	/*Wait max timeOut/waitStepms */
	UInt8 uMSM_XtalCal_End = 0;

	while (err == TM_OK && (--counter) > 0) {
		err =
		    tmddTDA182I2GetMSM_XtalCal_End(pObj->tUnit,
						   &uMSM_XtalCal_End);

		if (uMSM_XtalCal_End == 1) {
			/* MSM_XtalCal_End triggered => Exit */
			break;
		}

		TDA182I2Wait(pObj, waitStep);
	}

	if (counter == 0)
		err = ddTDA182I2_ERR_NOT_READY;

	return err;
}
