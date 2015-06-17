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
/*============================================================================*/
/* Standard include files:                                                    */
/*============================================================================*/
#include "tmnxtypes.h"
#include "tmcompid.h"
#include "tmfrontend.h"
#include "tmbslfrontendtypes.h"
#include "tmunitparams.h"
#include <linux/kernel.h>
/*============================================================================*/
/* Project include files:                                                     */
/*============================================================================*/
#include "tmddtda18212.h"
#include <tmddtda18212local.h>

#include "tmddtda18212instance.h"

/*============================================================================*/
/* Types and defines:                                                         */
/*============================================================================*/

/*============================================================================*/
/* Global data:                                                               */
/*============================================================================*/

/*============================================================================*/
/* Internal Prototypes:                                                       */
/*============================================================================*/

/*============================================================================*/
/* Exported functions:                                                        */
/*============================================================================*/

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2Init                                              */
/*                                                                            */
/* DESCRIPTION: Create an instance of a TDA182I2 Tuner                        */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2Init(tmUnitSelect_t tUnit,/*I:Unit#*/
	tmbslFrontEndDependency_t *psSrvFunc	/* I: setup parameters */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	if (psSrvFunc == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	/* Get Instance Object */
	if (err == TM_OK)
		err = ddTDA182I2GetInstance(tUnit, &pObj);

	/* Check driver state */
	if (err == TM_OK || err == ddTDA182I2_ERR_NOT_INITIALIZED) {
		if (pObj != Null && pObj->init == True) {
			err = ddTDA182I2_ERR_NOT_INITIALIZED;
		} else {
			/* Allocate the Instance Object */
			if (pObj == Null) {
				err = ddTDA182I2AllocInstance(tUnit, &pObj);
				if (err != TM_OK || pObj == Null)
					err = ddTDA182I2_ERR_NOT_INITIALIZED;
			}

			if (err == TM_OK) {
				/* initialize the Instance Object */
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
			}
		}
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2DeInit                                            */
/*                                                                            */
/* DESCRIPTION: Destroy an instance of a TDA182I2 Tuner                       */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2DeInit(tmUnitSelect_t tUnit/*I:Unit#*/)
{
	tmErrorCode_t err = TM_OK;
	ptmddTDA182I2Object_t pObj = Null;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);

	tmDBGPRINTEx(DEBUGLVL_VERBOSE, "tmddTDA182I2DeInit(0x%08X)", tUnit);

	if (err == TM_OK) {
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

	err = ddTDA182I2DeAllocInstance(tUnit);

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2GetSWVersion                                      */
/*                                                                            */
/* DESCRIPTION: Return the version of this device                             */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:       Values defined in the tmddTDA182I2local.h file                */
/*                                                                            */
/*============================================================================*/
/* I: Receives SW Version */
tmErrorCode_t tmddTDA182I2GetSWVersion(ptmSWVersion_t pSWVersion)
{
	pSWVersion->compatibilityNr = TDA182I2_DD_COMP_NUM;
	pSWVersion->majorVersionNr = TDA182I2_DD_MAJOR_VER;
	pSWVersion->minorVersionNr = TDA182I2_DD_MINOR_VER;

	return TM_OK;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2Reset                                             */
/*                                                                            */
/* DESCRIPTION: Initialize TDA182I2 Hardware                                  */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2Reset(tmUnitSelect_t tUnit/*I:Unit#*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/****** I2C map initialization : begin *********/
		if (err == TM_OK) {
			/* read all bytes */
			err =
			    ddTDA182I2Read(pObj, 0x00,
					   TDA182I2_I2C_MAP_NB_BYTES);
			tmASSERTExT(err, TM_OK,
				    (DEBUGLVL_ERROR,
				     "ddTDA182I2Read(0x%08X) failed.", tUnit));
		}
		if (err == TM_OK) {
			/* RSSI_Ck_Speed    31,25 kHz   0 */
			err = tmddTDA182I2SetRSSI_Ck_Speed(tUnit, 0);
			tmASSERTExT(err, TM_OK,
				    (DEBUGLVL_ERROR,
				     "tmddTDA182I2SetRSSI_Ck_Speed(0x%08X, 0) failed.",
				     tUnit));
		}
		if (err == TM_OK) {
			/* AGCs_Up_Step_assym UP 12 Asym / 4 Asym  / 5 Asym 3 */
			err = tmddTDA182I2SetAGCs_Up_Step_assym(tUnit, 3);
			tmASSERTExT(err, TM_OK,
				    (DEBUGLVL_ERROR,
				     "tmddTDA182I2SetAGCs_Up_Step_assym(0x%08X, 3) failed.",
				     tUnit));
		}
		if (err == TM_OK) {
			/* AGCs_Do_Step_assym       DO 12 Asym / 45 Sym   2 */
			err = tmddTDA182I2SetAGCs_Do_Step_assym(tUnit, 2);
			tmASSERTExT(err, TM_OK,
				    (DEBUGLVL_ERROR,
				     "tmddTDA182I2SetAGCs_Do_Step_assym(0x%08X, 2) failed.",
				     tUnit));
		}
		if (err == TM_OK) {
			/*  1.022ms  : LTE enhancement */
			err = tmddTDA182I2SetIR_Mixer_Do_step(tUnit, 3);
			tmASSERTExT(err, TM_OK,
				    (DEBUGLVL_ERROR,
				     "tmddTDA182I2SetIR_Mixer_Do_step(0x%08X, 2) failed.",
				     tUnit));
		}
	/****** I2C map initialization : end *********/

	/*****************************************/
		/* Launch tuner calibration */
		/* State reached after 1.5 s max */
		if (err == TM_OK) {
			/* set IRQ_clear */
			err = tmddTDA182I2SetIRQ_Clear(tUnit, 0x1F);
			tmASSERTExT(err, TM_OK,
				    (DEBUGLVL_ERROR,
				     "tmddTDA182I2SetIRQ_clear(0x%08X, 0x1F) failed.",
				     tUnit));
		}

		if (err == TM_OK) {
			/* set power state on */
			err = tmddTDA182I2SetPowerState(tUnit,
					      tmddTDA182I2_PowerNormalMode);
			tmASSERTExT(err, TM_OK,
				    (DEBUGLVL_ERROR,
				     "tmddTDA182I2SetPowerState(0x%08X, PowerNormalMode) failed.",
				     tUnit));
		}

		if (err == TM_OK) {
			/* set & trigger MSM */
			pObj->I2CMap.uBx19.MSM_byte_1 = 0x3B;
			pObj->I2CMap.uBx1A.MSM_byte_2 = 0x01;

			/* write bytes 0x19 to 0x1A */
			err = ddTDA182I2Write(pObj, 0x19, 2);
			tmASSERTExT(err, TM_OK,
				    (DEBUGLVL_ERROR,
				     "ddTDA182I2Write(0x%08X) failed.", tUnit));

			pObj->I2CMap.uBx1A.MSM_byte_2 = 0x00;

		}

		if (pObj->bIRQWait) {
			if (err == TM_OK) {
				err = ddTDA182I2WaitIRQ(pObj, 1500, 50, 0x1F);
				tmASSERTExT(err, TM_OK,
					    (DEBUGLVL_ERROR,
					     "ddTDA182I2WaitIRQ(0x%08X) failed.",
					     tUnit));
			}
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2SetLPF_Gain_Mode                                  */
/*                                                                            */
/* DESCRIPTION: Free/Freeze LPF Gain                                          */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2SetLPF_Gain_Mode(tmUnitSelect_t tUnit,/*I:Unit#*/
				   UInt8 uMode	/* I: Unknown/Free/Frozen */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		switch (uMode) {
		case tmddTDA182I2_LPF_Gain_Unknown:
		default:
			tmASSERTExT(err, TM_OK,
				    (DEBUGLVL_ERROR,
				     "tmddTDA182I2SetLPF_Gain_Free(0x%08X, tmddTDA182I2_LPF_Gain_Unknown).",
				     tUnit));
			break;

		case tmddTDA182I2_LPF_Gain_Free:
			/* Disable AGC5 loop off */
			err = tmddTDA182I2SetAGC5_loop_off(tUnit, False);
			tmASSERTExT(err, TM_OK,
				    (DEBUGLVL_ERROR,
				     "tmddTDA182I2SetAGC5_loop_off(0x%08X) failed.",
				     tUnit));

			if (err == TM_OK) {
				/* Do not force AGC5 gain */
				err = tmddTDA182I2SetForce_AGC5_gain(tUnit,
									False);
				tmASSERTExT(err, TM_OK,
					    (DEBUGLVL_ERROR,
					     "tmddTDA182I2SetForce_AGC5_gain(0x%08X) failed.",
					     tUnit));
			}
			break;

		case tmddTDA182I2_LPF_Gain_Frozen:
			/* Enable AGC5 loop off */
			err = tmddTDA182I2SetAGC5_loop_off(tUnit, True);
			tmASSERTExT(err, TM_OK,
				    (DEBUGLVL_ERROR,
				     "tmddTDA182I2SetAGC5_loop_off(0x%08X) failed.",
				     tUnit));

			if (err == TM_OK) {
				/* Force AGC5 gain */
				err = tmddTDA182I2SetForce_AGC5_gain(tUnit,
						True);
				tmASSERTExT(err, TM_OK,
					    (DEBUGLVL_ERROR,
					     "tmddTDA182I2SetForce_AGC5_gain(0x%08X) failed.",
					     tUnit));
			}

			if (err == TM_OK) {
				/* Force gain to 0dB */
				err = tmddTDA182I2SetAGC5_Gain(tUnit, 0);
				tmASSERTExT(err, TM_OK,
					    (DEBUGLVL_ERROR,
					     "tmddTDA182I2SetAGC5_Gain(0x%08X) failed.",
					     tUnit));
			}
			break;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2GetLPF_Gain_Mode                                  */
/*                                                                            */
/* DESCRIPTION: Free/Freeze LPF Gain                                          */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2GetLPF_Gain_Mode(tmUnitSelect_t tUnit,/*I:Unit#*/
		   unsigned char *puMode	/* I/O: Unknown/Free/Frozen */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;
	UInt8 AGC5_loop_off = 0;
	UInt8 Force_AGC5_gain = 0;
	UInt8 AGC5_Gain = 0;

	/* Test the parameter */
	if (puMode == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		*puMode = tmddTDA182I2_LPF_Gain_Unknown;

		err = tmddTDA182I2GetAGC5_loop_off(tUnit, &AGC5_loop_off);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "tmddTDA182I2GetAGC5_loop_off(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			err =
			    tmddTDA182I2GetForce_AGC5_gain(tUnit,
							   &Force_AGC5_gain);
			tmASSERTExT(err, TM_OK,
				    (DEBUGLVL_ERROR,
				     "tmddTDA182I2GetForce_AGC5_gain(0x%08X) failed.",
				     tUnit));
		}

		if (err == TM_OK) {
			err = tmddTDA182I2GetAGC5_Gain(tUnit, &AGC5_Gain);
			tmASSERTExT(err, TM_OK,
				    (DEBUGLVL_ERROR,
				     "tmddTDA182I2GetAGC5_Gain(0x%08X) failed.",
				     tUnit));
		}

		if (err == TM_OK) {
			if (AGC5_loop_off == False && Force_AGC5_gain == False)
				*puMode = tmddTDA182I2_LPF_Gain_Free;
			else if (AGC5_loop_off == True
				   && Force_AGC5_gain == True
				   && AGC5_Gain == 0)
				*puMode = tmddTDA182I2_LPF_Gain_Frozen;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2Write                                             */
/*                                                                            */
/* DESCRIPTION: Write in TDA182I2 hardware                                    */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2Write(tmUnitSelect_t tUnit,/*I:Unit#*/
			UInt32 uIndex,	/* I: Start index to write */
			UInt32 uNbBytes,/* I: Number of bytes to write */
			UInt8 *puBytes	/* I: Pointer on an array of bytes */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;
	UInt32 uCounter;
	UInt8 *pI2CMap = Null;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* pI2CMap initialization */
		pI2CMap = &(pObj->I2CMap.uBx00.ID_byte_1) + uIndex;

		/* Save the values written in the Tuner */
		for (uCounter = 0; uCounter < uNbBytes; uCounter++) {
			*pI2CMap = puBytes[uCounter];
			pI2CMap++;
		}

		/* Write in the Tuner */
		err =
		    ddTDA182I2Write(pObj, (UInt8) (uIndex), (UInt8) (uNbBytes));
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2Read                                              */
/*                                                                            */
/* DESCRIPTION: Read in TDA182I2 hardware                                     */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2Read(tmUnitSelect_t tUnit,/*I:Unit#*/
		       UInt32 uIndex,	/* I: Start index to read */
		       UInt32 uNbBytes,	/* I: Number of bytes to read */
		       UInt8 *puBytes	/* I: Pointer on an array of bytes */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;
	UInt32 uCounter = 0;
	UInt8 *pI2CMap = Null;

	/* Test the parameters */
	if (uNbBytes > TDA182I2_I2C_MAP_NB_BYTES)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* pI2CMap initialization */
		pI2CMap = &(pObj->I2CMap.uBx00.ID_byte_1) + uIndex;

		/* Read from the Tuner */
		err =
		    ddTDA182I2Read(pObj, (UInt8) (uIndex), (UInt8) (uNbBytes));
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* Copy read values to puBytes */
			for (uCounter = 0; uCounter < uNbBytes; uCounter++) {
				*puBytes = (*pI2CMap);
				pI2CMap++;
				puBytes++;
			}
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2GetMS                                             */
/*                                                                            */
/* DESCRIPTION: Get the MS bit(s) status                                      */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2GetMS(tmUnitSelect_t tUnit,/*I:Unit#*/
	UInt8 *puValue	/*I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Test the parameters */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Read byte 0x00 */
		err = ddTDA182I2Read(pObj, 0x00, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		/* Get value */
		*puValue = pObj->I2CMap.uBx00.bF.MS;

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2GetIdentity                                       */
/*                                                                            */
/* DESCRIPTION: Get the Identity bit(s) status                                */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2GetIdentity(tmUnitSelect_t tUnit,/*I:Unit#*/
	UInt16 * puValue/* I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Test the parameters */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Read byte 0x00-0x01 */
		err = ddTDA182I2Read(pObj, 0x00, 2);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* Get value */
			*puValue =
			    pObj->I2CMap.uBx00.bF.Ident_1 << 8 | pObj->I2CMap.
			    uBx01.bF.Ident_2;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2GetMinorRevision                                  */
/*                                                                            */
/* DESCRIPTION: Get the Revision bit(s) status                                */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2GetMinorRevision(tmUnitSelect_t tUnit,/*I:Unit#*/
	UInt8 *puValue	/*I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Test the parameters */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Read byte 0x02 */
		err = ddTDA182I2Read(pObj, 0x02, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* Get value */
			*puValue = pObj->I2CMap.uBx02.bF.Minor_rev;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2GetMajorRevision                                  */
/*                                                                            */
/* DESCRIPTION: Get the Revision bit(s) status                                */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2GetMajorRevision(tmUnitSelect_t tUnit,/*I:Unit#*/
	UInt8 *puValue	/*I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Test the parameters */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Read byte 0x02 */
		err = ddTDA182I2Read(pObj, 0x02, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* Get value */
			*puValue = pObj->I2CMap.uBx02.bF.Major_rev;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}
	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2GetLO_Lock                                        */
/*                                                                            */
/* DESCRIPTION: Get the LO_Lock bit(s) status                                 */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2GetLO_Lock(tmUnitSelect_t tUnit,/*I:Unit#*/
	UInt8 *puValue	/*I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Test the parameters */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Read byte 0x05 */
		err = ddTDA182I2Read(pObj, 0x05, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* Get value */
			*puValue = pObj->I2CMap.uBx05.bF.LO_Lock;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}
	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2SetPowerState                                     */
/*                                                                            */
/* DESCRIPTION: Set the power state of the TDA182I2                           */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2SetPowerState(tmUnitSelect_t tUnit,/*I:Unit#*/
		tmddTDA182I2PowerState_t powerState/*I:Power state of device*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err != TM_OK)
		return err;

	/* Read bytes 0x06-0x14 */
	err = ddTDA182I2Read(pObj, 0x06, 15);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.", tUnit));

	if (err != TM_OK) {
		(void)ddTDA182I2MutexRelease(pObj);
		return err;
	}

	/* Set digital clock mode */
	switch (powerState) {
	case tmddTDA182I2_PowerStandbyWithLNAOnAndWithXtalOnAndWithSyntheOn:
	case tmddTDA182I2_PowerStandbyWithLNAOnAndWithXtalOn:
	case tmddTDA182I2_PowerStandbyWithXtalOn:
	case tmddTDA182I2_PowerStandby:
		/* Set 16 Mhz Xtal clock */
		err = tmddTDA182I2SetDigital_Clock_Mode(tUnit, 0);
		tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR,
		     "tmddTDA182I2SetDigital_Clock_Mode(0x%08X, 16 Mhz xtal clock) failed.",
			     tUnit));
		break;

	default:
		break;
	}

	if (err != TM_OK) {
		(void)ddTDA182I2MutexRelease(pObj);
		return err;
	}
	/* Set power state */
	switch (powerState) {
	case tmddTDA182I2_PowerNormalMode:
		pObj->I2CMap.uBx06.bF.SM = 0x00;
		pObj->I2CMap.uBx06.bF.SM_Synthe = 0x00;
		pObj->I2CMap.uBx06.bF.SM_LT = 0x00;
		pObj->I2CMap.uBx06.bF.SM_XT = 0x00;
		break;

	case tmddTDA182I2_PowerStandbyWithLNAOnAndWithXtalOnAndWithSyntheOn:
		pObj->I2CMap.uBx06.bF.SM = 0x01;
		pObj->I2CMap.uBx06.bF.SM_Synthe = 0x00;
		pObj->I2CMap.uBx06.bF.SM_LT = 0x00;
		pObj->I2CMap.uBx06.bF.SM_XT = 0x00;
		break;

	case tmddTDA182I2_PowerStandbyWithLNAOnAndWithXtalOn:
		pObj->I2CMap.uBx06.bF.SM = 0x01;
		pObj->I2CMap.uBx06.bF.SM_Synthe = 0x01;
		pObj->I2CMap.uBx06.bF.SM_LT = 0x00;
		pObj->I2CMap.uBx06.bF.SM_XT = 0x00;
		break;

	case tmddTDA182I2_PowerStandbyWithXtalOn:
		pObj->I2CMap.uBx06.bF.SM = 0x01;
		pObj->I2CMap.uBx06.bF.SM_Synthe = 0x01;
		pObj->I2CMap.uBx06.bF.SM_LT = 0x01;
		pObj->I2CMap.uBx06.bF.SM_XT = 0x00;
		break;

	case tmddTDA182I2_PowerStandby:
		pObj->I2CMap.uBx06.bF.SM = 0x01;
		pObj->I2CMap.uBx06.bF.SM_Synthe = 0x01;
		pObj->I2CMap.uBx06.bF.SM_LT = 0x01;
		pObj->I2CMap.uBx06.bF.SM_XT = 0x01;
		break;

	default:
		/* Power state not supported */
		return ddTDA182I2_ERR_NOT_SUPPORTED;
	}

	/* Write byte 0x06 */
	err = ddTDA182I2Write(pObj, 0x06, 1);
	tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR,
		     "ddTDA182I2Write(0x%08X) failed.", tUnit));

	/* Set digital clock mode */
	if (err == TM_OK) {
		switch (powerState) {
		case tmddTDA182I2_PowerNormalMode:
			/* Set sigma delta clock */
			err = tmddTDA182I2SetDigital_Clock_Mode(tUnit, 1);
			tmASSERTExT(err, TM_OK, (DEBUGLVL_ERROR,
				     "tmddTDA182I2SetDigital_Clock_Mode(0x%08X, sigma delta clock) failed.",
				     tUnit));
			break;

		default:
			break;
		}
	}

	if (err == TM_OK)
		/* Store powerstate */
		pObj->curPowerState = powerState;

	(void)ddTDA182I2MutexRelease(pObj);

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2GetPowerState                                     */
/*                                                                            */
/* DESCRIPTION: Get the power state of the TDA182I2                           */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2GetPowerState(tmUnitSelect_t tUnit,/*I:Unit#*/
	ptmddTDA182I2PowerState_t pPowerState	/* O: Power state of device */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err != TM_OK)
		return err;

	/* Get power state */
	if ((pObj->I2CMap.uBx06.bF.SM == 0x00)
	    && (pObj->I2CMap.uBx06.bF.SM_Synthe == 0x00)
	    && (pObj->I2CMap.uBx06.bF.SM_LT == 0x00)
	    && (pObj->I2CMap.uBx06.bF.SM_XT == 0x00)) {
		*pPowerState = tmddTDA182I2_PowerNormalMode;
	} else if ((pObj->I2CMap.uBx06.bF.SM == 0x01)
		   && (pObj->I2CMap.uBx06.bF.SM_Synthe == 0x00)
		   && (pObj->I2CMap.uBx06.bF.SM_LT == 0x00)
		   && (pObj->I2CMap.uBx06.bF.SM_XT == 0x00)) {
		*pPowerState =
		 tmddTDA182I2_PowerStandbyWithLNAOnAndWithXtalOnAndWithSyntheOn;
	} else if ((pObj->I2CMap.uBx06.bF.SM == 0x01)
		   && (pObj->I2CMap.uBx06.bF.SM_Synthe == 0x01)
		   && (pObj->I2CMap.uBx06.bF.SM_LT == 0x00)
		   && (pObj->I2CMap.uBx06.bF.SM_XT == 0x00)) {
		*pPowerState =
		    tmddTDA182I2_PowerStandbyWithLNAOnAndWithXtalOn;
	} else if ((pObj->I2CMap.uBx06.bF.SM == 0x01)
		   && (pObj->I2CMap.uBx06.bF.SM_Synthe == 0x01)
		   && (pObj->I2CMap.uBx06.bF.SM_LT == 0x01)
		   && (pObj->I2CMap.uBx06.bF.SM_XT == 0x00)) {
		*pPowerState = tmddTDA182I2_PowerStandbyWithXtalOn;
	} else if ((pObj->I2CMap.uBx06.bF.SM == 0x01)
		   && (pObj->I2CMap.uBx06.bF.SM_Synthe == 0x01)
		   && (pObj->I2CMap.uBx06.bF.SM_LT == 0x01)
		   && (pObj->I2CMap.uBx06.bF.SM_XT == 0x01)) {
		*pPowerState = tmddTDA182I2_PowerStandby;
	} else
		*pPowerState = tmddTDA182I2_PowerMax;

	(void)ddTDA182I2MutexRelease(pObj);

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2GetPower_Level                                    */
/*                                                                            */
/* DESCRIPTION: Get the Power_Level bit(s) status                             */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2GetPower_Level(tmUnitSelect_t tUnit,/*I:Unit#*/
	UInt8 *puValue	/*I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;
	UInt8 uValue = 0;

	/* Test the parameters */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);
	if (err == TM_OK) {
		/* Set IRQ_clear */
		err = tmddTDA182I2SetIRQ_Clear(tUnit, 0x10);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "tmddTDA182I2SetIRQ_clear(0x%08X, 0x10) failed.",
			     tUnit));
	}
	if (err == TM_OK) {
		/* Trigger RSSI_Meas */
		pObj->I2CMap.uBx19.MSM_byte_1 = 0x80;
		err = ddTDA182I2Write(pObj, 0x19, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "tmddTDA182I2Write(0x%08X) failed.", tUnit));

		if (err == TM_OK) {
			/*Trigger MSM_Launch */
			pObj->I2CMap.uBx1A.bF.MSM_Launch = 1;

			/* Write byte 0x1A */
			err = ddTDA182I2Write(pObj, 0x1A, 1);
			tmASSERTExT(err, TM_OK,
				    (DEBUGLVL_ERROR,
				     "ddTDA182I2Write(0x%08X) failed.", tUnit));

			pObj->I2CMap.uBx1A.bF.MSM_Launch = 0;
			if (pObj->bIRQWait) {
				if (err == TM_OK) {
					err =
					    ddTDA182I2WaitIRQ(pObj, 700, 1,
							      0x10);
					tmASSERTExT(err, TM_OK,
						    (DEBUGLVL_ERROR,
						     "ddTDA182I2WaitIRQ(0x%08X) failed.",
						     tUnit));
				}
			}
		}

		if (err == TM_OK) {
			/* Read byte 0x07 */
			err = ddTDA182I2Read(pObj, 0x07, 1);
			tmASSERTExT(err, TM_OK,
				    (DEBUGLVL_ERROR,
				     "ddTDA182I2Read(0x%08X) failed.", tUnit));
		}

		if (err == TM_OK) {
			/* Get value (limit range) */
			uValue = pObj->I2CMap.uBx07.bF.Power_Level;
			if (uValue < TDA182I2_POWER_LEVEL_MIN)
				*puValue = 0x00;
			else if (uValue > TDA182I2_POWER_LEVEL_MAX)
				*puValue = 0xFF;
			else
				*puValue = uValue;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2GetIRQ_status                                     */
/*                                                                            */
/* DESCRIPTION: Get the IRQ_status bit(s) status                              */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2GetIRQ_status(tmUnitSelect_t tUnit,/*I:Unit#*/
	UInt8 *puValue	/*I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Test the parameters */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Read byte 0x08 */
		err = ddTDA182I2GetIRQ_status(pObj, puValue);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetIRQ_status(0x%08X) failed.", tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2GetMSM_XtalCal_End                                */
/*                                                                            */
/* DESCRIPTION: Get the MSM_XtalCal_End bit(s) status                         */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2GetMSM_XtalCal_End(tmUnitSelect_t tUnit,/*I:Unit#*/
	UInt8 *puValue	/*I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Test the parameters */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Read byte 0x08 */
		err = ddTDA182I2GetMSM_XtalCal_End(pObj, puValue);

		(void)ddTDA182I2MutexRelease(pObj);
	}
	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2SetIRQ_Clear                                      */
/*                                                                            */
/* DESCRIPTION: Set the IRQ_Clear bit(s) status                               */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2SetIRQ_Clear(tmUnitSelect_t tUnit,/*I:Unit#*/
				       UInt8 irqStatus	/* I: IRQs to clear */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Set IRQ_Clear */
		/*pObj->I2CMap.uBx0A.bF.IRQ_Clear = 1; */
		pObj->I2CMap.uBx0A.IRQ_clear |= (0x80 | (irqStatus & 0x1F));

		/* Write byte 0x0A */
		err = ddTDA182I2Write(pObj, 0x0A, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		/* Reset IRQ_Clear (buffer only, no write) */
		/*pObj->I2CMap.uBx0A.bF.IRQ_Clear = 0; */
		pObj->I2CMap.uBx0A.IRQ_clear &= (~(0x80 | (irqStatus & 0x1F)));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2SetAGC1_TOP                                       */
/*                                                                            */
/* DESCRIPTION: Set the AGC1_TOP bit(s) status                                */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2SetAGC1_TOP(tmUnitSelect_t tUnit,/*I:Unit#*/
				      UInt8 uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Set value */
		pObj->I2CMap.uBx0C.bF.AGC1_TOP = uValue;

		/* write byte 0x0C */
		err = ddTDA182I2Write(pObj, 0x0C, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2GetAGC1_TOP                                       */
/*                                                                            */
/* DESCRIPTION: Get the AGC1_TOP bit(s) status                                */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2GetAGC1_TOP(tmUnitSelect_t tUnit,/*I:Unit#*/
	UInt8 *puValue	/*I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Test the parameters */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	/* Get Instance Object */
	if (err == TM_OK) {
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK) {
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

		if (err == TM_OK) {
			/* Read byte 0x0C */
			err = ddTDA182I2Read(pObj, 0x0C, 1);
			tmASSERTExT(err, TM_OK,
				    (DEBUGLVL_ERROR,
				     "ddTDA182I2Read(0x%08X) failed.", tUnit));

			if (err == TM_OK) {
				/* Get value */
				*puValue = pObj->I2CMap.uBx0C.bF.AGC1_TOP;
			}

			(void)ddTDA182I2MutexRelease(pObj);
		}
	}
	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2SetAGC2_TOP                                       */
/*                                                                            */
/* DESCRIPTION: Set the AGC2_TOP bit(s) status                                */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2SetAGC2_TOP(tmUnitSelect_t tUnit,/*I:Unit#*/
				      UInt8 uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx0D.bF.AGC2_TOP = uValue;

		/* Write byte 0x0D */
		err = ddTDA182I2Write(pObj, 0x0D, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2GetAGC2_TOP                                       */
/*                                                                            */
/* DESCRIPTION: Get the AGC2_TOP bit(s) status                                */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2GetAGC2_TOP(tmUnitSelect_t tUnit,/*I:Unit#*/
	UInt8 *puValue	/*I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Test the parameters */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Read byte 0x0D */
		err = ddTDA182I2Read(pObj, 0x0D, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* Get value */
			*puValue = pObj->I2CMap.uBx0D.bF.AGC2_TOP;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2SetAGCs_Up_Step                                   */
/*                                                                            */
/* DESCRIPTION: Set the AGCs_Up_Step bit(s) status                            */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2SetAGCs_Up_Step(tmUnitSelect_t tUnit,/*I:Unit#*/
					  UInt8 uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Set value */
		pObj->I2CMap.uBx0E.bF.AGCs_Up_Step = uValue;

		/* Write byte 0x0E */
		err = ddTDA182I2Write(pObj, 0x0E, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2GetAGCs_Up_Step                                   */
/*                                                                            */
/* DESCRIPTION: Get the AGCs_Up_Step bit(s) status                            */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2GetAGCs_Up_Step(tmUnitSelect_t tUnit,/*I:Unit#*/
	UInt8 *puValue	/*I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Test the parameters */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Read byte 0x0E */
		err = ddTDA182I2Read(pObj, 0x0E, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* Get value */
			*puValue = pObj->I2CMap.uBx0E.bF.AGCs_Up_Step;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2SetAGCK_Step                                      */
/*                                                                            */
/* DESCRIPTION: Set the AGCK_Step bit(s) status                               */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2SetAGCK_Step(tmUnitSelect_t tUnit,/*I:Unit#*/
				       UInt8 uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Set value */
		pObj->I2CMap.uBx0E.bF.AGCK_Step = uValue;

		/* Write byte 0x0E */
		err = ddTDA182I2Write(pObj, 0x0E, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2GetAGCK_Step                                      */
/*                                                                            */
/* DESCRIPTION: Get the AGCK_Step bit(s) status                               */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2GetAGCK_Step(tmUnitSelect_t tUnit,/*I:Unit#*/
	UInt8 *puValue	/*I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Test the parameters */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Read byte 0x0E */
		err = ddTDA182I2Read(pObj, 0x0E, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* Get value */
			*puValue = pObj->I2CMap.uBx0E.bF.AGCK_Step;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2SetAGCK_Mode                                      */
/*                                                                            */
/* DESCRIPTION: Set the AGCK_Mode bit(s) status                               */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2SetAGCK_Mode(tmUnitSelect_t tUnit,/*I:Unit#*/
				       UInt8 uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Set value */
		pObj->I2CMap.uBx0E.bF.AGCK_Mode = uValue;

		/* Write byte 0x0E */
		err = ddTDA182I2Write(pObj, 0x0E, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2GetAGCK_Mode                                      */
/*                                                                            */
/* DESCRIPTION: Get the AGCK_Mode bit(s) status                               */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2GetAGCK_Mode(tmUnitSelect_t tUnit,/*I:Unit#*/
	UInt8 *puValue	/*I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Test the parameters */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Read byte 0x0E */
		err = ddTDA182I2Read(pObj, 0x0E, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* Get value */
			*puValue = pObj->I2CMap.uBx0E.bF.AGCK_Mode;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2SetPD_RFAGC_Adapt                                 */
/*                                                                            */
/* DESCRIPTION: Set the PD_RFAGC_Adapt bit(s) status                          */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2SetPD_RFAGC_Adapt(tmUnitSelect_t tUnit,/*I:Unit#*/
				    UInt8 uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Set value */
		pObj->I2CMap.uBx0F.bF.PD_RFAGC_Adapt = uValue;

		/* Write byte 0x0F */
		err = ddTDA182I2Write(pObj, 0x0F, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2GetPD_RFAGC_Adapt                                 */
/*                                                                            */
/* DESCRIPTION: Get the PD_RFAGC_Adapt bit(s) status                          */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2GetPD_RFAGC_Adapt(tmUnitSelect_t tUnit,/*I:Unit#*/
	UInt8 *puValue	/*I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Test the parameters */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Read byte 0x0F */
		err = ddTDA182I2Read(pObj, 0x0F, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* Get value */
			*puValue = pObj->I2CMap.uBx0F.bF.PD_RFAGC_Adapt;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2SetRFAGC_Adapt_TOP                                */
/*                                                                            */
/* DESCRIPTION: Set the RFAGC_Adapt_TOP bit(s) status                         */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2SetRFAGC_Adapt_TOP(tmUnitSelect_t tUnit,/*I:Unit#*/
				     UInt8 uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Set value */
		pObj->I2CMap.uBx0F.bF.RFAGC_Adapt_TOP = uValue;

		/* Write byte 0x0F */
		err = ddTDA182I2Write(pObj, 0x0F, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2GetRFAGC_Adapt_TOP                                */
/*                                                                            */
/* DESCRIPTION: Get the RFAGC_Adapt_TOP bit(s) status                         */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2GetRFAGC_Adapt_TOP(tmUnitSelect_t tUnit,/*I:Unit#*/
	UInt8 *puValue	/*I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Test the parameters */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Read byte 0x0F */
		err = ddTDA182I2Read(pObj, 0x0F, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* Get value */
			*puValue = pObj->I2CMap.uBx0F.bF.RFAGC_Adapt_TOP;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2SetRF_Atten_3dB                                   */
/*                                                                            */
/* DESCRIPTION: Set the RF_Atten_3dB bit(s) status                            */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2SetRF_Atten_3dB(tmUnitSelect_t tUnit,/*I:Unit#*/
					  UInt8 uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Set value */
		pObj->I2CMap.uBx0F.bF.RF_Atten_3dB = uValue;

		/* Write byte 0x0F */
		err = ddTDA182I2Write(pObj, 0x0F, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2GetRF_Atten_3dB                                   */
/*                                                                            */
/* DESCRIPTION: Get the RF_Atten_3dB bit(s) status                            */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2GetRF_Atten_3dB(tmUnitSelect_t tUnit,/*I:Unit#*/
	UInt8 *puValue	/*I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Test the parameters */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Read byte 0x0F */
		err = ddTDA182I2Read(pObj, 0x0F, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* Get value */
			*puValue = pObj->I2CMap.uBx0F.bF.RF_Atten_3dB;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2SetRFAGC_Top                                      */
/*                                                                            */
/* DESCRIPTION: Set the RFAGC_Top bit(s) status                               */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2SetRFAGC_Top(tmUnitSelect_t tUnit,/*I:Unit#*/
				       UInt8 uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Set value */
		pObj->I2CMap.uBx0F.bF.RFAGC_Top = uValue;

		/* Write byte 0x0F */
		err = ddTDA182I2Write(pObj, 0x0F, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2GetRFAGC_Top                                      */
/*                                                                            */
/* DESCRIPTION: Get the RFAGC_Top bit(s) status                               */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2GetRFAGC_Top(tmUnitSelect_t tUnit,/*I:Unit#*/
	UInt8 *puValue	/*I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Test the parameters */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Read byte 0x0F */
		err = ddTDA182I2Read(pObj, 0x0F, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* Get value */
			*puValue = pObj->I2CMap.uBx0F.bF.RFAGC_Top;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2SetIR_Mixer_Top                                   */
/*                                                                            */
/* DESCRIPTION: Set the IR_Mixer_Top bit(s) status                            */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2SetIR_Mixer_Top(tmUnitSelect_t tUnit,/*I:Unit#*/
					  UInt8 uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Set value */
		pObj->I2CMap.uBx10.bF.IR_Mixer_Top = uValue;

		/* Write byte 0x10 */
		err = ddTDA182I2Write(pObj, 0x10, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2GetIR_Mixer_Top                                   */
/*                                                                            */
/* DESCRIPTION: Get the IR_Mixer_Top bit(s) status                            */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2GetIR_Mixer_Top(tmUnitSelect_t tUnit,/*I:Unit#*/
	UInt8 *puValue	/*I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Test the parameters */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Read byte 0x10 */
		err = ddTDA182I2Read(pObj, 0x10, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* Get value */
			*puValue = pObj->I2CMap.uBx10.bF.IR_Mixer_Top;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2SetAGC5_Ana                                       */
/*                                                                            */
/* DESCRIPTION: Set the AGC5_Ana bit(s) status                                */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2SetAGC5_Ana(tmUnitSelect_t tUnit,/*I:Unit#*/
				      UInt8 uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Set value */
		pObj->I2CMap.uBx11.bF.AGC5_Ana = uValue;

		/* Write byte 0x11 */
		err = ddTDA182I2Write(pObj, 0x11, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2GetAGC5_Ana                                       */
/*                                                                            */
/* DESCRIPTION: Get the AGC5_Ana bit(s) status                                */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2GetAGC5_Ana(tmUnitSelect_t tUnit,/*I:Unit#*/
	UInt8 *puValue	/*I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Test the parameters */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Read byte 0x11 */
		err = ddTDA182I2Read(pObj, 0x11, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* Get value */
			*puValue = pObj->I2CMap.uBx11.bF.AGC5_Ana;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}
	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2SetAGC5_TOP                                       */
/*                                                                            */
/* DESCRIPTION: Set the AGC5_TOP bit(s) status                                */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2SetAGC5_TOP(tmUnitSelect_t tUnit,/*I:Unit#*/
				      UInt8 uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Set value */
		pObj->I2CMap.uBx11.bF.AGC5_TOP = uValue;

		/* Write byte 0x11 */
		err = ddTDA182I2Write(pObj, 0x11, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2GetAGC5_TOP                                       */
/*                                                                            */
/* DESCRIPTION: Get the AGC5_TOP bit(s) status                                */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2GetAGC5_TOP(tmUnitSelect_t tUnit,/*I:Unit#*/
	UInt8 *puValue	/*I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Test the parameters */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Read byte 0x11 */
		err = ddTDA182I2Read(pObj, 0x11, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* Get value */
			*puValue = pObj->I2CMap.uBx11.bF.AGC5_TOP;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2SetIF_Level                                       */
/*                                                                            */
/* DESCRIPTION: Set the IF_level bit(s) status                                */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2SetIF_Level(tmUnitSelect_t tUnit,/*I:Unit#*/
				      UInt8 uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Set value */
		pObj->I2CMap.uBx12.bF.IF_level = uValue;

		/* Write byte 0x12 */
		err = ddTDA182I2Write(pObj, 0x12, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2GetIF_Level                                       */
/*                                                                            */
/* DESCRIPTION: Get the IF_level bit(s) status                                */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2GetIF_Level(tmUnitSelect_t tUnit,/*I:Unit#*/
	UInt8 *puValue	/*I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Test the parameters */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Read byte 0x12 */
		err = ddTDA182I2Read(pObj, 0x12, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* Get value */
			*puValue = pObj->I2CMap.uBx12.bF.IF_level;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2SetIF_HP_Fc                                       */
/*                                                                            */
/* DESCRIPTION: Set the IF_HP_Fc bit(s) status                                */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2SetIF_HP_Fc(tmUnitSelect_t tUnit,/*I:Unit#*/
				      UInt8 uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Set value */
		pObj->I2CMap.uBx13.bF.IF_HP_Fc = uValue;

		/* Write byte 0x13 */
		err = ddTDA182I2Write(pObj, 0x13, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2GetIF_HP_Fc                                       */
/*                                                                            */
/* DESCRIPTION: Get the IF_HP_Fc bit(s) status                                */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2GetIF_HP_Fc(tmUnitSelect_t tUnit,/*I:Unit#*/
	UInt8 *puValue	/*I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Test the parameters */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Read byte 0x13 */
		err = ddTDA182I2Read(pObj, 0x13, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* Get value */
			*puValue = pObj->I2CMap.uBx13.bF.IF_HP_Fc;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2SetIF_ATSC_Notch                                  */
/*                                                                            */
/* DESCRIPTION: Set the IF_ATSC_Notch bit(s) status                           */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2SetIF_ATSC_Notch(tmUnitSelect_t tUnit,/*I:Unit#*/
					   UInt8 uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Set value */
		pObj->I2CMap.uBx13.bF.IF_ATSC_Notch = uValue;

		/* Write byte 0x13 */
		err = ddTDA182I2Write(pObj, 0x13, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2GetIF_ATSC_Notch                                  */
/*                                                                            */
/* DESCRIPTION: Get the IF_ATSC_Notch bit(s) status                           */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2GetIF_ATSC_Notch(tmUnitSelect_t tUnit,/*I:Unit#*/
	UInt8 *puValue	/*I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Test the parameters */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Read byte 0x13 */
		err = ddTDA182I2Read(pObj, 0x13, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* Get value */
			*puValue = pObj->I2CMap.uBx13.bF.IF_ATSC_Notch;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}
	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2SetLP_FC_Offset                                   */
/*                                                                            */
/* DESCRIPTION: Set the LP_FC_Offset bit(s) status                            */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2SetLP_FC_Offset(tmUnitSelect_t tUnit,/*I:Unit#*/
					  UInt8 uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Set value */
		pObj->I2CMap.uBx13.bF.LP_FC_Offset = uValue;

		/* Write byte 0x13 */
		err = ddTDA182I2Write(pObj, 0x13, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2GetLP_FC_Offset                                   */
/*                                                                            */
/* DESCRIPTION: Get the LP_FC_Offset bit(s) status                            */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2GetLP_FC_Offset(tmUnitSelect_t tUnit,/*I:Unit#*/
	UInt8 *puValue	/*I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Test the parameters */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Read byte 0x13 */
		err = ddTDA182I2Read(pObj, 0x13, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* Get value */
			*puValue = pObj->I2CMap.uBx13.bF.LP_FC_Offset;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}
	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2SetLP_FC                                          */
/*                                                                            */
/* DESCRIPTION: Set the LP_Fc bit(s) status                                   */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2SetLP_FC(tmUnitSelect_t tUnit,/*I:Unit#*/
				   UInt8 uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Set value */
		pObj->I2CMap.uBx13.bF.LP_Fc = uValue;

		/* Write byte 0x13 */
		err = ddTDA182I2Write(pObj, 0x13, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2GetLP_FC                                          */
/*                                                                            */
/* DESCRIPTION: Get the LP_Fc bit(s) status                                   */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2GetLP_FC(tmUnitSelect_t tUnit,/*I:Unit#*/
	UInt8 *puValue	/*I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Test the parameters */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Read byte 0x13 */
		err = ddTDA182I2Read(pObj, 0x13, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* Get value */
			*puValue = pObj->I2CMap.uBx13.bF.LP_Fc;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2SetDigital_Clock_Mode                             */
/*                                                                            */
/* DESCRIPTION: Set the Digital_Clock_Mode bit(s) status                      */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2SetDigital_Clock_Mode(tmUnitSelect_t tUnit,/*I:Unit#*/
					UInt8 uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Set value */
		pObj->I2CMap.uBx14.bF.Digital_Clock_Mode = uValue;

		/* Write byte 0x14 */
		err = ddTDA182I2Write(pObj, 0x14, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2GetDigital_Clock_Mode                             */
/*                                                                            */
/* DESCRIPTION: Get the Digital_Clock_Mode bit(s) status                      */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2GetDigital_Clock_Mode(tmUnitSelect_t tUnit,/*I:Unit#*/
	UInt8 *puValue	/*I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Test the parameters */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Read byte 0x14 */
		err = ddTDA182I2Read(pObj, 0x14, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* Get value */
			*puValue = pObj->I2CMap.uBx14.bF.Digital_Clock_Mode;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2SetIF_Freq                                        */
/*                                                                            */
/* DESCRIPTION: Set the IF_Freq bit(s) status                                 */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2SetIF_Freq(tmUnitSelect_t tUnit,/*I:Unit#*/
				     UInt32 uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Set value */
		pObj->I2CMap.uBx15.bF.IF_Freq = (UInt8) (uValue / 50000);

		/* Write byte 0x15 */
		err = ddTDA182I2Write(pObj, 0x15, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2GetIF_Freq                                        */
/*                                                                            */
/* DESCRIPTION: Get the IF_Freq bit(s) status                                 */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2GetIF_Freq(tmUnitSelect_t tUnit,/*I:Unit#*/
	UInt32 *puValue	/*I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Test the parameters */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Read byte 0x15 */
		err = ddTDA182I2Read(pObj, 0x15, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* Get value */
			*puValue = pObj->I2CMap.uBx15.bF.IF_Freq * 50000;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2SetRF_Freq                                        */
/*                                                                            */
/* DESCRIPTION: Set the RF_Freq bit(s) status                                 */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2SetRF_Freq(tmUnitSelect_t tUnit,/*I:Unit#*/
				     UInt32 uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;
	UInt32 uRF = 0;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/*****************************************/
		/* Tune the settings that depend on the RF input frequency,
		 * expressed in kHz. */
		/* RF filters tuning, PLL locking */
		/* State reached after 5ms */

		if (err == TM_OK) {
			/* Set IRQ_clear */
			err = tmddTDA182I2SetIRQ_Clear(tUnit, 0x0C);
			tmASSERTExT(err, TM_OK,
				    (DEBUGLVL_ERROR,
				     "tmddTDA182I2SetIRQ_clear(0x%08X, 0x0C) failed.",
				     tUnit));
		}

		if (err == TM_OK) {
			/* Set power state ON */
			err =
			    tmddTDA182I2SetPowerState(tUnit,
					      tmddTDA182I2_PowerNormalMode);
			tmASSERTExT(err, TM_OK,
				    (DEBUGLVL_ERROR,
				     "tmddTDA182I2SetPowerState(0x%08X, PowerNormalMode) failed.",
				     tUnit));
		}

		if (err == TM_OK) {
			/* Set RF frequency expressed in kHz */
			uRF = uValue / 1000;
			pObj->I2CMap.uBx16.bF.RF_Freq_1 =
			    (UInt8) ((uRF & 0x00FF0000) >> 16);
			pObj->I2CMap.uBx17.bF.RF_Freq_2 =
			    (UInt8) ((uRF & 0x0000FF00) >> 8);
			pObj->I2CMap.uBx18.bF.RF_Freq_3 =
			    (UInt8) (uRF & 0x000000FF);

			/* write bytes 0x16 to 0x18 */
			err = ddTDA182I2Write(pObj, 0x16, 3);
			tmASSERTExT(err, TM_OK,
				    (DEBUGLVL_ERROR,
				     "ddTDA182I2Write(0x%08X) failed.", tUnit));
		}

		if (err == TM_OK) {
			/* Set & trigger MSM */
			pObj->I2CMap.uBx19.MSM_byte_1 = 0x41;
			pObj->I2CMap.uBx1A.MSM_byte_2 = 0x01;

			/* Write bytes 0x19 to 0x1A */
			err = ddTDA182I2Write(pObj, 0x19, 2);
			tmASSERTExT(err, TM_OK,
				    (DEBUGLVL_ERROR,
				     "ddTDA182I2Write(0x%08X) failed.", tUnit));

			pObj->I2CMap.uBx1A.MSM_byte_2 = 0x00;
		}
		if (pObj->bIRQWait) {
			if (err == TM_OK) {
				err = ddTDA182I2WaitIRQ(pObj, 50, 5, 0x0C);
				tmASSERTExT(err, TM_OK,
					    (DEBUGLVL_ERROR,
					     "ddTDA182I2WaitIRQ(0x%08X) failed.",
					     tUnit));
			}
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2GetRF_Freq                                        */
/*                                                                            */
/* DESCRIPTION: Get the RF_Freq bit(s) status                                 */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2GetRF_Freq(tmUnitSelect_t tUnit,/*I:Unit#*/
	UInt32 *puValue	/*I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Test the parameters */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Read bytes 0x16 to 0x18 */
		err = ddTDA182I2Read(pObj, 0x16, 3);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* Get value */
			*puValue =
			    pObj->I2CMap.uBx16.bF.RF_Freq_1 * 65536 +
			    pObj->I2CMap.uBx17.bF.RF_Freq_2 * 256 +
			    pObj->I2CMap.uBx18.bF.RF_Freq_3;
			*puValue = *puValue * 1000;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2SetMSM_Launch                                     */
/*                                                                            */
/* DESCRIPTION: Set the MSM_Launch bit(s) status                              */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2SetMSM_Launch(tmUnitSelect_t tUnit/*I:Unit#*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Set value */
		pObj->I2CMap.uBx1A.bF.MSM_Launch = 1;

		/* Write byte 0x1A */
		err = ddTDA182I2Write(pObj, 0x1A, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		/* reset MSM_Launch (buffer only, no write) */
		pObj->I2CMap.uBx1A.bF.MSM_Launch = 0x00;

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2GetMSM_Launch                                     */
/*                                                                            */
/* DESCRIPTION: Get the MSM_Launch bit(s) status                              */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2GetMSM_Launch(tmUnitSelect_t tUnit,/*I:Unit#*/
	UInt8 *puValue	/*I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Test the parameters */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Read byte 0x1A */
		err = ddTDA182I2Read(pObj, 0x1A, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* Get value */
			*puValue = pObj->I2CMap.uBx1A.bF.MSM_Launch;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2SetPSM_StoB                                       */
/*                                                                            */
/* DESCRIPTION: Set the PSM_StoB bit(s) status                                */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2SetPSM_StoB(tmUnitSelect_t tUnit,/*I:Unit#*/
				      UInt8 uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Set value */
		pObj->I2CMap.uBx1B.bF.PSM_StoB = uValue;

		/* Read byte 0x1B */
		err = ddTDA182I2Write(pObj, 0x1B, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2GetPSM_StoB                                       */
/*                                                                            */
/* DESCRIPTION: Get the PSM_StoB bit(s) status                                */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2GetPSM_StoB(tmUnitSelect_t tUnit,/*I:Unit#*/
	UInt8 *puValue	/*I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Test the parameters */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Read byte 0x1B */
		err = ddTDA182I2Read(pObj, 0x1B, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* Get value */
			*puValue = pObj->I2CMap.uBx1B.bF.PSM_StoB;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2SetFmax_Lo                                        */
/*                                                                            */
/* DESCRIPTION: Set the Fmax_Lo bit(s) status                                 */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2SetFmax_Lo(tmUnitSelect_t tUnit,/*I:Unit#*/
				     UInt8 uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* set value */
		pObj->I2CMap.uBx1D.bF.Fmax_Lo = uValue;

		/* read byte 0x1D */
		err = ddTDA182I2Write(pObj, 0x1D, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2GetFmax_Lo                                        */
/*                                                                            */
/* DESCRIPTION: Get the Fmax_Lo bit(s) status                                 */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2GetFmax_Lo(tmUnitSelect_t tUnit,/*I:Unit#*/
	UInt8 *puValue	/*I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* test the parameter */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* read byte 0x1D */
		err = ddTDA182I2Read(pObj, 0x1D, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* get value */
			*puValue = pObj->I2CMap.uBx1D.bF.Fmax_Lo;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2SetIR_Loop                                        */
/*                                                                            */
/* DESCRIPTION: Set the IR_Loop bit(s) status                                 */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2SetIR_Loop(tmUnitSelect_t tUnit,/*I:Unit#*/
				     UInt8 uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Set value */
		pObj->I2CMap.uBx1E.bF.IR_Loop = uValue - 4;

		/* Read byte 0x1E */
		err = ddTDA182I2Write(pObj, 0x1E, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2GetIR_Loop                                        */
/*                                                                            */
/* DESCRIPTION: Get the IR_Loop bit(s) status                                 */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2GetIR_Loop(tmUnitSelect_t tUnit,/*I:Unit#*/
	UInt8 *puValue	/*I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Test the parameters */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Read byte 0x1E */
		err = ddTDA182I2Read(pObj, 0x1E, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* Get value */
			*puValue = pObj->I2CMap.uBx1E.bF.IR_Loop + 4;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2SetIR_Target                                      */
/*                                                                            */
/* DESCRIPTION: Set the IR_Target bit(s) status                               */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2SetIR_Target(tmUnitSelect_t tUnit,/*I:Unit#*/
				       UInt8 uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Set value */
		pObj->I2CMap.uBx1E.bF.IR_Target = uValue;

		/* Read byte 0x1E */
		err = ddTDA182I2Write(pObj, 0x1E, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2GetIR_Target                                      */
/*                                                                            */
/* DESCRIPTION: Get the IR_Target bit(s) status                               */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2GetIR_Target(tmUnitSelect_t tUnit,/*I:Unit#*/
	UInt8 *puValue	/*I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Test the parameters */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Read byte 0x1E */
		err = ddTDA182I2Read(pObj, 0x1E, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* Get value */
			*puValue = pObj->I2CMap.uBx1E.bF.IR_Target;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2SetIR_Corr_Boost                                  */
/*                                                                            */
/* DESCRIPTION: Set the IR_Corr_Boost bit(s) status                           */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2SetIR_Corr_Boost(tmUnitSelect_t tUnit,/*I:Unit#*/
					   UInt8 uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Set value */
		pObj->I2CMap.uBx1F.bF.IR_Corr_Boost = uValue;

		/* Read byte 0x1F */
		err = ddTDA182I2Write(pObj, 0x1F, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2GetIR_Corr_Boost                                  */
/*                                                                            */
/* DESCRIPTION: Get the IR_Corr_Boost bit(s) status                           */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2GetIR_Corr_Boost(tmUnitSelect_t tUnit,/*I:Unit#*/
	UInt8 *puValue	/*I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Test the parameters */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Read byte 0x1F */
		err = ddTDA182I2Read(pObj, 0x1F, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* Get value */
			*puValue = pObj->I2CMap.uBx1F.bF.IR_Corr_Boost;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2SetIR_mode_ram_store                              */
/*                                                                            */
/* DESCRIPTION: Set the IR_mode_ram_store bit(s) status                       */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2SetIR_mode_ram_store(tmUnitSelect_t tUnit,/*I:Unit#*/
				       UInt8 uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Set value */
		pObj->I2CMap.uBx1F.bF.IR_mode_ram_store = uValue;

		/* Write byte 0x1F */
		err = ddTDA182I2Write(pObj, 0x1F, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2GetIR_mode_ram_store                              */
/*                                                                            */
/* DESCRIPTION: Get the IR_mode_ram_store bit(s) status                       */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2GetIR_mode_ram_store(tmUnitSelect_t tUnit,/*I:Unit#*/
	UInt8 *puValue	/*I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Test the parameters */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Read byte 0x1F */
		err = ddTDA182I2Read(pObj, 0x1F, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* Get value */
			*puValue = pObj->I2CMap.uBx1F.bF.IR_mode_ram_store;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2SetPD_Udld                                        */
/*                                                                            */
/* DESCRIPTION: Set the PD_Udld bit(s) status                                 */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2SetPD_Udld(tmUnitSelect_t tUnit,/*I:Unit#*/
				     UInt8 uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Set value */
		pObj->I2CMap.uBx22.bF.PD_Udld = uValue;

		/* Write byte 0x22 */
		err = ddTDA182I2Write(pObj, 0x22, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2GetPD_Udld                                        */
/*                                                                            */
/* DESCRIPTION: Get the PD_Udld bit(s) status                                 */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2GetPD_Udld(tmUnitSelect_t tUnit,/*I:Unit#*/
	UInt8 *puValue	/*I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Test the parameters */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Read byte 0x22 */
		err = ddTDA182I2Read(pObj, 0x22, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* Get value */
			*puValue = pObj->I2CMap.uBx22.bF.PD_Udld;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2SetAGC_Ovld_TOP                                   */
/*                                                                            */
/* DESCRIPTION: Set the AGC_Ovld_TOP bit(s) status                            */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2SetAGC_Ovld_TOP(tmUnitSelect_t tUnit,/*I:Unit#*/
					  UInt8 uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Set value */
		pObj->I2CMap.uBx22.bF.AGC_Ovld_TOP = uValue;

		/* Write byte 0x22 */
		err = ddTDA182I2Write(pObj, 0x22, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2GetAGC_Ovld_TOP                                   */
/*                                                                            */
/* DESCRIPTION: Get the AGC_Ovld_TOP bit(s) status                            */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2GetAGC_Ovld_TOP(tmUnitSelect_t tUnit,/*I:Unit#*/
	UInt8 *puValue	/*I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Test the parameters */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Read byte 0x22 */
		err = ddTDA182I2Read(pObj, 0x22, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* Get value */
			*puValue = pObj->I2CMap.uBx22.bF.AGC_Ovld_TOP;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2SetHi_Pass                                        */
/*                                                                            */
/* DESCRIPTION: Set the Hi_Pass bit(s) status                                 */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2SetHi_Pass(tmUnitSelect_t tUnit,/*I:Unit#*/
				     UInt8 uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Set value */
		pObj->I2CMap.uBx23.bF.Hi_Pass = uValue;

		/* Read byte 0x23 */
		err = ddTDA182I2Write(pObj, 0x23, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2GetHi_Pass                                        */
/*                                                                            */
/* DESCRIPTION: Get the Hi_Pass bit(s) status                                 */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2GetHi_Pass(tmUnitSelect_t tUnit,/*I:Unit#*/
	UInt8 *puValue	/*I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Test the parameters */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Read byte 0x23 */
		err = ddTDA182I2Read(pObj, 0x23, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* Get value */
			*puValue = pObj->I2CMap.uBx23.bF.Hi_Pass;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2SetIF_Notch                                       */
/*                                                                            */
/* DESCRIPTION: Set the IF_Notch bit(s) status                                */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2SetIF_Notch(tmUnitSelect_t tUnit,/*I:Unit#*/
				      UInt8 uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Set value */
		pObj->I2CMap.uBx23.bF.IF_Notch = uValue;

		/* Read byte 0x23 */
		err = ddTDA182I2Write(pObj, 0x23, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2GetIF_Notch                                       */
/*                                                                            */
/* DESCRIPTION: Get the IF_Notch bit(s) status                                */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2GetIF_Notch(tmUnitSelect_t tUnit,/*I:Unit#*/
	UInt8 *puValue	/*I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Test the parameters */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Read byte 0x23 */
		err = ddTDA182I2Read(pObj, 0x23, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* Get value */
			*puValue = pObj->I2CMap.uBx23.bF.IF_Notch;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2SetAGC5_loop_off                                  */
/*                                                                            */
/* DESCRIPTION: Set the AGC5_loop_off bit(s) status                           */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2SetAGC5_loop_off(tmUnitSelect_t tUnit,/*I:Unit#*/
					   UInt8 uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Set value */
		pObj->I2CMap.uBx25.bF.AGC5_loop_off = uValue;

		/* Read byte 0x25 */
		err = ddTDA182I2Write(pObj, 0x25, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2GetAGC5_loop_off                                  */
/*                                                                            */
/* DESCRIPTION: Get the AGC5_loop_off bit(s) status                           */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2GetAGC5_loop_off(tmUnitSelect_t tUnit,/*I:Unit#*/
	UInt8 *puValue	/*I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Test the parameters */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Read byte 0x25 */
		err = ddTDA182I2Read(pObj, 0x25, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* Get value */
			*puValue = pObj->I2CMap.uBx25.bF.AGC5_loop_off;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}
	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2SetAGC5_Do_step                                   */
/*                                                                            */
/* DESCRIPTION: Set the AGC5_Do_step bit(s) status                            */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2SetAGC5_Do_step(tmUnitSelect_t tUnit,/*I:Unit#*/
					  UInt8 uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Set value */
		pObj->I2CMap.uBx25.bF.AGC5_Do_step = uValue;

		/* Read byte 0x25 */
		err = ddTDA182I2Write(pObj, 0x25, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2GetAGC5_Do_step                                   */
/*                                                                            */
/* DESCRIPTION: Get the AGC5_Do_step bit(s) status                            */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2GetAGC5_Do_step(tmUnitSelect_t tUnit,/*I:Unit#*/
	UInt8 *puValue	/*I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Test the parameters */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Read byte 0x25 */
		err = ddTDA182I2Read(pObj, 0x25, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* Get value */
			*puValue = pObj->I2CMap.uBx25.bF.AGC5_Do_step;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2SetForce_AGC5_gain                                */
/*                                                                            */
/* DESCRIPTION: Set the Force_AGC5_gain bit(s) status                         */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2SetForce_AGC5_gain(tmUnitSelect_t tUnit,/*I:Unit#*/
				     UInt8 uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Set value */
		pObj->I2CMap.uBx25.bF.Force_AGC5_gain = uValue;

		/* Read byte 0x25 */
		err = ddTDA182I2Write(pObj, 0x25, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2GetForce_AGC5_gain                                */
/*                                                                            */
/* DESCRIPTION: Get the Force_AGC5_gain bit(s) status                         */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2GetForce_AGC5_gain(tmUnitSelect_t tUnit,/*I:Unit#*/
	UInt8 *puValue	/*I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Test the parameters */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Read byte 0x25 */
		err = ddTDA182I2Read(pObj, 0x25, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* Get value */
			*puValue = pObj->I2CMap.uBx25.bF.Force_AGC5_gain;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2SetAGC5_Gain                                      */
/*                                                                            */
/* DESCRIPTION: Set the AGC5_Gain bit(s) status                               */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2SetAGC5_Gain(tmUnitSelect_t tUnit,/*I:Unit#*/
				       UInt8 uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Set value */
		pObj->I2CMap.uBx25.bF.AGC5_Gain = uValue;

		/* Read byte 0x25 */
		err = ddTDA182I2Write(pObj, 0x25, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2GetAGC5_Gain                                      */
/*                                                                            */
/* DESCRIPTION: Get the AGC5_Gain bit(s) status                               */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2GetAGC5_Gain(tmUnitSelect_t tUnit,/*I:Unit#*/
	UInt8 *puValue	/*I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Test the parameters */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Read byte 0x25 */
		err = ddTDA182I2Read(pObj, 0x25, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* Get value */
			*puValue = pObj->I2CMap.uBx25.bF.AGC5_Gain;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2SetRF_Filter_Bypass                               */
/*                                                                            */
/* DESCRIPTION: Set the RF_Filter_Bypass bit(s) status                        */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2SetRF_Filter_Bypass(tmUnitSelect_t tUnit,/*I:Unit#*/
				      UInt8 uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Set value */
		pObj->I2CMap.uBx2C.bF.RF_Filter_Bypass = uValue;

		/* Read byte 0x2C */
		err = ddTDA182I2Write(pObj, 0x2C, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2GetRF_Filter_Bypass                               */
/*                                                                            */
/* DESCRIPTION: Get the RF_Filter_Bypass bit(s) status                        */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2GetRF_Filter_Bypass(tmUnitSelect_t tUnit,/*I:Unit#*/
	UInt8 *puValue	/*I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Test the parameters */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Read byte 0x2C */
		err = ddTDA182I2Read(pObj, 0x2C, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* Get value */
			*puValue = pObj->I2CMap.uBx2C.bF.RF_Filter_Bypass;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2SetRF_Filter_Band                                 */
/*                                                                            */
/* DESCRIPTION: Set the RF_Filter_Band bit(s) status                          */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2SetRF_Filter_Band(tmUnitSelect_t tUnit,/*I:Unit#*/
				    UInt8 uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Set value */
		pObj->I2CMap.uBx2C.bF.RF_Filter_Band = uValue;

		/* Read byte 0x2C */
		err = ddTDA182I2Write(pObj, 0x2C, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2GetRF_Filter_Band                                 */
/*                                                                            */
/* DESCRIPTION: Get the RF_Filter_Band bit(s) status                          */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2GetRF_Filter_Band(tmUnitSelect_t tUnit,/*I:Unit#*/
	UInt8 *puValue	/*I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Test the parameters */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Read byte 0x2C */
		err = ddTDA182I2Read(pObj, 0x2C, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* Get value */
			*puValue = pObj->I2CMap.uBx2C.bF.RF_Filter_Band;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2SetRF_Filter_Cap                                  */
/*                                                                            */
/* DESCRIPTION: Set the RF_Filter_Cap bit(s) status                           */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2SetRF_Filter_Cap(tmUnitSelect_t tUnit,/*I:Unit#*/
					   UInt8 uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Set value */
		pObj->I2CMap.uBx2D.bF.RF_Filter_Cap = uValue;

		/* Read byte 0x2D */
		err = ddTDA182I2Write(pObj, 0x2D, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2GetRF_Filter_Cap                                  */
/*                                                                            */
/* DESCRIPTION: Get the RF_Filter_Cap bit(s) status                           */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2GetRF_Filter_Cap(tmUnitSelect_t tUnit,/*I:Unit#*/
	UInt8 *puValue	/*I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Test the parameters */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Read byte 0x2D */
		err = ddTDA182I2Read(pObj, 0x2D, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* Get value */
			*puValue = pObj->I2CMap.uBx2D.bF.RF_Filter_Cap;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2SetGain_Taper                                     */
/*                                                                            */
/* DESCRIPTION: Set the Gain_Taper bit(s) status                              */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2SetGain_Taper(tmUnitSelect_t tUnit,/*I:Unit#*/
					UInt8 uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Set value */
		pObj->I2CMap.uBx2E.bF.Gain_Taper = uValue;

		/* Read byte 0x2E */
		err = ddTDA182I2Write(pObj, 0x2E, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2GetGain_Taper                                     */
/*                                                                            */
/* DESCRIPTION: Get the Gain_Taper bit(s) status                              */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2GetGain_Taper(tmUnitSelect_t tUnit,/*I:Unit#*/
	UInt8 *puValue	/*I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Test the parameters */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	/* Get Instance Object */
	if (err == TM_OK) {
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK) {
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

		if (err == TM_OK) {
			/* Read byte 0x2E */
			err = ddTDA182I2Read(pObj, 0x2E, 1);
			tmASSERTExT(err, TM_OK,
				    (DEBUGLVL_ERROR,
				     "ddTDA182I2Read(0x%08X) failed.", tUnit));

			if (err == TM_OK) {
				/* Get value */
				*puValue = pObj->I2CMap.uBx2E.bF.Gain_Taper;
			}

			(void)ddTDA182I2MutexRelease(pObj);
		}
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2SetN_CP_Current                                   */
/*                                                                            */
/* DESCRIPTION: Set the N_CP_Current bit(s) status                            */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2SetN_CP_Current(tmUnitSelect_t tUnit,/*I:Unit#*/
					  UInt8 uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Set value */
		pObj->I2CMap.uBx30.bF.N_CP_Current = uValue;

		/* Read byte 0x30 */
		err = ddTDA182I2Write(pObj, 0x30, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2GetN_CP_Current                                   */
/*                                                                            */
/* DESCRIPTION: Get the N_CP_Current bit(s) status                            */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2GetN_CP_Current(tmUnitSelect_t tUnit,/*I:Unit#*/
	UInt8 *puValue	/*I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Test the parameters */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Read byte 0x30 */
		err = ddTDA182I2Read(pObj, 0x30, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* Get value */
			*puValue = pObj->I2CMap.uBx30.bF.N_CP_Current;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2SetRSSI_Ck_Speed                                  */
/*                                                                            */
/* DESCRIPTION: Set the RSSI_Ck_Speed bit(s) status                           */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2SetRSSI_Ck_Speed(tmUnitSelect_t tUnit,/*I:Unit#*/
					   UInt8 uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Set value */
		pObj->I2CMap.uBx36.bF.RSSI_Ck_Speed = uValue;

		/* Write byte 0x36 */
		err = ddTDA182I2Write(pObj, 0x36, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2GetRSSI_Ck_Speed                                  */
/*                                                                            */
/* DESCRIPTION: Get the RSSI_Ck_Speed bit(s) status                           */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2GetRSSI_Ck_Speed(tmUnitSelect_t tUnit,/*I:Unit#*/
	UInt8 *puValue	/*I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Test the parameters */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Read byte 0x36 */
		err = ddTDA182I2Read(pObj, 0x36, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* Get value */
			*puValue = pObj->I2CMap.uBx36.bF.RSSI_Ck_Speed;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2SetRFCAL_Phi2                                     */
/*                                                                            */
/* DESCRIPTION: Set the RFCAL_Phi2 bit(s) status                              */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2SetRFCAL_Phi2(tmUnitSelect_t tUnit,/*I:Unit#*/
					UInt8 uValue	/* I: Item value */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Set value */
		pObj->I2CMap.uBx37.bF.RFCAL_Phi2 = uValue;

		/* Write byte 0x37 */
		err = ddTDA182I2Write(pObj, 0x37, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Write(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2GetRFCAL_Phi2                                     */
/*                                                                            */
/* DESCRIPTION: Get the RFCAL_Phi2 bit(s) status                              */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2GetRFCAL_Phi2(tmUnitSelect_t tUnit,/*I:Unit#*/
	UInt8 *puValue	/*I: Address of the var to output item value*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Test the parameters */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		/* Read byte 0x37 */
		err = ddTDA182I2Read(pObj, 0x37, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     tUnit));

		if (err == TM_OK) {
			/* Get value */
			*puValue = pObj->I2CMap.uBx37.bF.RFCAL_Phi2;
		}

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2WaitIRQ                                           */
/*                                                                            */
/* DESCRIPTION: Wait the IRQ to trigger                                       */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2WaitIRQ(tmUnitSelect_t tUnit,/*I:Unit#*/
				  UInt32 timeOut,	/* I: timeout */
				  UInt32 waitStep,	/* I: wait step */
				  UInt8 irqStatus	/* I: IRQs to wait */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		err = ddTDA182I2WaitIRQ(pObj, timeOut, waitStep, irqStatus);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2WaitIRQ(0x%08X) failed.", tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2WaitXtalCal_End                                   */
/*                                                                            */
/* DESCRIPTION: Wait the MSM_XtalCal_End to trigger                           */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2WaitXtalCal_End(tmUnitSelect_t tUnit,/*I:Unit#*/
				  UInt32 timeOut,	/* I: timeout */
				  UInt32 waitStep	/* I: wait step */)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		err = ddTDA182I2WaitXtalCal_End(pObj, timeOut, waitStep);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2WaitXtalCal_End(0x%08X) failed.",
			     tUnit));

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2SetIRQWait                                        */
/*                                                                            */
/* DESCRIPTION: Set whether wait IRQ in driver or not                         */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2SetIRQWait(tmUnitSelect_t tUnit,/*I:Unit#*/
		/* I: Determine if we need to wait IRQ in driver functions */
		Bool bWait)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		pObj->bIRQWait = bWait;

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmddTDA182I2GetIRQWait                                        */
/*                                                                            */
/* DESCRIPTION: Get whether wait IRQ in driver or not                         */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2GetIRQWait(tmUnitSelect_t tUnit,/*I:Unit#*/
		/* O: Determine if we need to wait IRQ in driver functions */
		bool *pbWait)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;

	/* Test the parameters */
	if (pbWait == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Get Instance Object */
		err = ddTDA182I2GetInstance(tUnit, &pObj);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR,
			     "ddTDA182I2GetInstance(0x%08X) failed.", tUnit));
	}

	if (err == TM_OK)
		err = ddTDA182I2MutexAcquire(pObj, ddTDA182I2_MUTEX_TIMEOUT);

	if (err == TM_OK) {
		*pbWait = pObj->bIRQWait;

		(void)ddTDA182I2MutexRelease(pObj);
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    ddTDA182I2GetIRQ_status                                       */
/*                                                                            */
/* DESCRIPTION: Get IRQ status                                                */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t ddTDA182I2GetIRQ_status(ptmddTDA182I2Object_t pObj,/*I:Inst obj*/
	UInt8 *puValue	/*I: Address of the var to output item value*/)
{
	tmErrorCode_t err = TM_OK;

	/* Read byte 0x08 */
	err = ddTDA182I2Read(pObj, 0x08, 1);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
		     pObj->tUnit));

	if (err == TM_OK) {
		/* Get value */
		*puValue = pObj->I2CMap.uBx08.bF.IRQ_status;
	}

	return err;
}

/*============================================================================*/
/* FUNCTION:    ddTDA182I2GetMSM_XtalCal_End                                  */
/*                                                                            */
/* DESCRIPTION: Get MSM_XtalCal_End bit(s) status                             */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
ddTDA182I2GetMSM_XtalCal_End(ptmddTDA182I2Object_t pObj,/*I:Inst obj*/
	UInt8 *puValue	/*I: Address of the var to output item value*/)
{
	tmErrorCode_t err = TM_OK;

	/* Test the parameters */
	if (puValue == Null)
		err = ddTDA182I2_ERR_BAD_PARAMETER;

	if (err == TM_OK) {
		/* Read byte 0x08 */
		err = ddTDA182I2Read(pObj, 0x08, 1);
		tmASSERTExT(err, TM_OK,
			    (DEBUGLVL_ERROR, "ddTDA182I2Read(0x%08X) failed.",
			     pObj->tUnit));

		if (err == TM_OK) {
			/* Get value */
			*puValue = pObj->I2CMap.uBx08.bF.MSM_XtalCal_End;
		}
	}
	return err;
}

/*============================================================================*/
/* FUNCTION:    ddTDA182I2WaitIRQ                                             */
/*                                                                            */
/* DESCRIPTION: Wait for IRQ to trigger                                       */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t
ddTDA182I2WaitIRQ(ptmddTDA182I2Object_t pObj,	/* I: Instance object */
				UInt32 timeOut,	/* I: timeout */
				UInt32 waitStep,	/* I: wait step */
				UInt8 irqStatus	/* I: IRQs to wait */)
{
	tmErrorCode_t err = TM_OK;
	/* Wait max timeOut/waitStep ms */
	UInt32 counter = timeOut / waitStep;
	UInt8 uIRQ = 0;
	UInt8 uIRQStatus = 0;
	Bool bIRQTriggered = False;

	while (err == TM_OK && (--counter) > 0) {
		err = ddTDA182I2GetIRQ_status(pObj, &uIRQ);

		if (err == TM_OK && uIRQ == 1)
			bIRQTriggered = True;

		if (bIRQTriggered) {
			/* IRQ triggered => Exit */
			break;
		}

		if (err == TM_OK && irqStatus != 0x00) {
			uIRQStatus = ((pObj->I2CMap.uBx08.IRQ_status) & 0x1F);

			if (irqStatus == uIRQStatus)
				bIRQTriggered = True;
		}

		err = ddTDA182I2Wait(pObj, waitStep);
	}

	if (counter == 0)
		err = ddTDA182I2_ERR_NOT_READY;

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
tmErrorCode_t
ddTDA182I2WaitXtalCal_End(ptmddTDA182I2Object_t pObj,	/* I: Instance object */
					UInt32 timeOut,	/* I: timeout */
					UInt32 waitStep	/* I: wait step */)
{
	tmErrorCode_t err = TM_OK;
	/* Wait max timeOut/waitStepms */
	UInt32 counter = timeOut / waitStep;
	UInt8 uMSM_XtalCal_End = 0;

	while (err == TM_OK && (--counter) > 0) {
		err = ddTDA182I2GetMSM_XtalCal_End(pObj, &uMSM_XtalCal_End);

		if (uMSM_XtalCal_End == 1) {
			/* MSM_XtalCal_End triggered => Exit */
			break;
		}

		ddTDA182I2Wait(pObj, waitStep);
	}

	if (counter == 0)
		err = ddTDA182I2_ERR_NOT_READY;

	return err;
}

/*============================================================================*/
/* FUNCTION:    ddTDA182I2Write                                               */
/*                                                                            */
/* DESCRIPTION: Write in TDA182I2 hardware                                    */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t ddTDA182I2Write(ptmddTDA182I2Object_t pObj,/* I: Driver object */
			      UInt8 uSubAddress,	/* I: sub address */
			      UInt8 uNbData	/* I: nb of data */)
{
	tmErrorCode_t err = TM_OK;
	UInt8 *pI2CMap = Null;

	/* pI2CMap initialization */
	pI2CMap = &(pObj->I2CMap.uBx00.ID_byte_1);

	err =
	    POBJ_SRVFUNC_SIO.Write(pObj->tUnitW, 1, &uSubAddress, uNbData,
				   &(pI2CMap[uSubAddress]));

	/* return value */
	return err;
}

/*============================================================================*/
/* FUNCTION:    ddTDA182I2Read                                                */
/*                                                                            */
/* DESCRIPTION: Read in TDA182I2 hardware                                     */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t ddTDA182I2Read(ptmddTDA182I2Object_t pObj,/* I: Driver object */
			     UInt8 uSubAddress,	/* I: sub address */
			     UInt8 uNbData	/* I: nb of data */)
{
	tmErrorCode_t err = TM_OK;
	UInt8 *pI2CMap = Null;

	/* pRegister initialization */
	pI2CMap = &(pObj->I2CMap.uBx00.ID_byte_1) + uSubAddress;

	/* Read data from the Tuner */
	err =
	    POBJ_SRVFUNC_SIO.Read(pObj->tUnitW, 1, &uSubAddress, uNbData,
				  pI2CMap);

	/* return value */
	return err;
}

/*============================================================================*/
/* FUNCTION:    ddTDA182I2Wait                                                */
/*                                                                            */
/* DESCRIPTION: Wait for the requested time                                   */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t ddTDA182I2Wait(ptmddTDA182I2Object_t pObj,/* I: Driver object */
			     UInt32 Time	/*  I: time to wait for */)
{
	tmErrorCode_t err = TM_OK;

	/* wait Time ms */
	err = POBJ_SRVFUNC_STIME.Wait(pObj->tUnit, Time);

	/* Return value */
	return err;
}

/*============================================================================*/
/* FUNCTION:    ddTDA182I2MutexAcquire                                        */
/*                                                                            */
/* DESCRIPTION: Acquire driver mutex                                          */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t ddTDA182I2MutexAcquire(ptmddTDA182I2Object_t pObj, UInt32 timeOut)
{
	tmErrorCode_t err = TM_OK;

	if (pObj->sMutex.Acquire != Null && pObj->pMutex != Null)
		err = pObj->sMutex.Acquire(pObj->pMutex, timeOut);

	return err;
}

/*============================================================================*/
/* FUNCTION:    ddTDA182I2MutexRelease                                        */
/*                                                                            */
/* DESCRIPTION: Release driver mutex                                          */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t ddTDA182I2MutexRelease(ptmddTDA182I2Object_t pObj)
{
	tmErrorCode_t err = TM_OK;

	if (pObj->sMutex.Release != Null && pObj->pMutex != Null)
		err = pObj->sMutex.Release(pObj->pMutex);

	return err;
}

/*============================================================================*/
/* FUNCTION:    tmTDA182I2AGC1_change                                         */
/*                                                                            */
/* DESCRIPTION: adapt AGC1_gain from latest call  ( simulate AGC1 gain free ) */
/*                                                                            */
/* RETURN:      TM_OK if no error                                             */
/*                                                                            */
/* NOTES:                                                                     */
/*                                                                            */
/*============================================================================*/
tmErrorCode_t tmddTDA182I2AGC1_Adapt(tmUnitSelect_t tUnit/*I:Unit#*/)
{
	ptmddTDA182I2Object_t pObj = Null;
	tmErrorCode_t err = TM_OK;
	UInt8 counter, vAGC1min, vAGC1_max_step;
	Int16 TotUp, TotDo;
	UInt8 NbStepsDone = 0;

	/* Get Instance Object */
	err = ddTDA182I2GetInstance(tUnit, &pObj);
	tmASSERTExT(err, TM_OK,
		    (DEBUGLVL_ERROR, "ddTDA182I2GetInstance(0x%08X) failed.",
		     tUnit));

	if (pObj->I2CMap.uBx0C.bF.AGC1_6_15dB == 0) {
		vAGC1min = 0;	/* -12 dB */
		vAGC1_max_step = 10;	/* -12 +15 dB */
	} else {
		vAGC1min = 6;	/* 6 dB */
		vAGC1_max_step = 4;	/* 6 -> 15 dB */
	}

	/* limit to min - max steps 10 */
	while (err == TM_OK && NbStepsDone < vAGC1_max_step) {
		counter = 0;
		TotUp = 0;
		TotDo = 0;
		NbStepsDone = NbStepsDone + 1;
		while (err == TM_OK && (counter++) < 40) {
			/* read UP , DO AGC1 */
			err = ddTDA182I2Read(pObj, 0x31, 1);
			tmASSERTExT(err, TM_OK,
				    (DEBUGLVL_ERROR,
				     "ddTDA182I2Read(0x%08X) failed.", tUnit));
			TotDo =
			    TotDo + (pObj->I2CMap.uBx31.bF.Do_AGC1 ? 14 : -1);
			TotUp =
			    TotUp + (pObj->I2CMap.uBx31.bF.Up_AGC1 ? 1 : -4);
			err = ddTDA182I2Wait(pObj, 1);
		}
		if (TotUp >= 15 && pObj->I2CMap.uBx24.bF.AGC1_Gain != 9) {
			pObj->I2CMap.uBx24.bF.AGC1_Gain =
			    pObj->I2CMap.uBx24.bF.AGC1_Gain + 1;
			err = ddTDA182I2Write(pObj, 0x24, 1);
			tmASSERTExT(err, TM_OK,
				    (DEBUGLVL_ERROR,
				     "ddTDA182I2Write(0x%08X) failed.", tUnit));

		} else if (TotDo >= 10
			   && pObj->I2CMap.uBx24.bF.AGC1_Gain != vAGC1min) {
			pObj->I2CMap.uBx24.bF.AGC1_Gain =
			    pObj->I2CMap.uBx24.bF.AGC1_Gain - 1;
			err = ddTDA182I2Write(pObj, 0x24, 1);
			tmASSERTExT(err, TM_OK,
				    (DEBUGLVL_ERROR,
				     "ddTDA182I2Write(0x%08X) failed.", tUnit));
		} else
			NbStepsDone = vAGC1_max_step;
	}
	return err;
}
