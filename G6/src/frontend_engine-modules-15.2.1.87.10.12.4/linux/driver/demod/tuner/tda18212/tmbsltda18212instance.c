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
/* FILE NAME:    tmbslTDA182I2Instance.c */
/*  */
/* DESCRIPTION:  define the static Objects */
/*  */
/* DOCUMENT REF: DVP Software Coding Guidelines v1.14 */
/* DVP Board Support Library Architecture Specification v0.5 */
/*  */
/* NOTES: */
/* ------------------------------------------------------------------------- */
/*  */
#include <linux/kernel.h>
#include "tmnxtypes.h"
#include "tmcompid.h"
#include "tmfrontend.h"
#include "tmunitparams.h"
#include "tmbslfrontendtypes.h"

#ifdef TMBSL_TDA18272
#include "tmbsltda18272.h"
#else /* TMBSL_TDA18272 */
#include "tmbsltda18212.h"
#endif /* TMBSL_TDA18272 */

#include "tmbsltda18212local.h"
#include "tmbsltda18212instance.h"
#include <tmbsltda18212_instancecustom.h>

/* ------------------------------------------------------------------------- */
/* Global data: */
/* ------------------------------------------------------------------------- */
/*  */
/*  */
/* default instance */
tmTDA182I2Object_t gTDA182I2Instance[] = {
	{
	 (tmUnitSelect_t) (-1),	/* tUnit */
	 (tmUnitSelect_t) (-1),	/* tUnit temporary */
	 Null,			/* pMutex */
	 False,			/* init (instance initialization default) */
	 {			/* sRWFunc */
	  Null,
	  Null}
	 ,
	 {			/* sTime */
	  Null,
	  Null}
	 ,
	 {			/* sDebug */
	  Null}
	 ,
	 {			/* sMutex */
	  Null,
	  Null,
	  Null,
	  Null}
	 ,
#ifdef TMBSL_TDA18272
	 TMBSL_TDA182I2_INSTANCE_CUSTOM_MODE_PATH0 {
		TMBSL_TDA182I2_INSTANCE_CUSTOM_STD_DIGITAL_SELECTION_PATH0
		TMBSL_TDA182I2_INSTANCE_CUSTOM_STD_ANALOG_SELECTION_PATH0}
#else
	 TMBSL_TDA182I2_INSTANCE_CUSTOM_MODE_PATH0 {
		TMBSL_TDA182I2_INSTANCE_CUSTOM_STD_DIGITAL_SELECTION_PATH0}
#endif
	 }
	,
	{
	 (tmUnitSelect_t) (-1),	/* tUnit */
	 (tmUnitSelect_t) (-1),	/* tUnit temporary */
	 Null,			/* pMutex */
	 False,			/* init (instance initialization default) */
	 {			/* sRWFunc */
	  Null,
	  Null}
	 ,
	 {			/* sTime */
	  Null,
	  Null}
	 ,
	 {			/* sDebug */
	  Null}
	 ,
	 {			/* sMutex */
	  Null,
	  Null,
	  Null,
	  Null}
	 ,
#ifdef TMBSL_TDA18272
	 TMBSL_TDA182I2_INSTANCE_CUSTOM_MODE_PATH1 {
		TMBSL_TDA182I2_INSTANCE_CUSTOM_STD_DIGITAL_SELECTION_PATH1
		TMBSL_TDA182I2_INSTANCE_CUSTOM_STD_ANALOG_SELECTION_PATH1}
#else
	 TMBSL_TDA182I2_INSTANCE_CUSTOM_MODE_PATH1 {
		TMBSL_TDA182I2_INSTANCE_CUSTOM_STD_DIGITAL_SELECTION_PATH1}
#endif
	 }
};

/* -------------------------------------------------------------------------
 * FUNCTION:    TDA182I2AllocInstance:
 *
 * DESCRIPTION: allocate new instance
 *
 * RETURN:
 *
 * NOTES:
 * ------------------------------------------------------------------------- */
tmErrorCode_t TDA182I2AllocInstance(tmUnitSelect_t tUnit,	/* I: Unit no.*/
		    pptmTDA182I2Object_t ppDrvObject	/* I: Device Object */)
{
	tmErrorCode_t err = TDA182I2_ERR_BAD_UNIT_NUMBER;
	ptmTDA182I2Object_t pObj = Null;
	UInt32 uLoopCounter = 0;

	/* Find a free instance */
	for (uLoopCounter = 0; uLoopCounter < TDA182I2_MAX_UNITS;
	     uLoopCounter++) {
		pObj = &gTDA182I2Instance[uLoopCounter];
		if (pObj->init == False) {
			pObj->tUnit = tUnit;
			pObj->tUnitW = tUnit;

			*ppDrvObject = pObj;
			err = TM_OK;
			break;
		}
	}

	/* return value */
	return err;
}

/* -------------------------------------------------------------------------
 * FUNCTION:    TDA182I2DeAllocInstance:
 *
 * DESCRIPTION: deallocate instance
 *
 * RETURN:      always TM_OK
 *
 * NOTES:
 * ------------------------------------------------------------------------- */
tmErrorCode_t TDA182I2DeAllocInstance(tmUnitSelect_t tUnit/* I: Unit no.*/)
{
	tmErrorCode_t err = TDA182I2_ERR_BAD_UNIT_NUMBER;
	ptmTDA182I2Object_t pObj = Null;

	/* check input parameters */
	err = TDA182I2GetInstance(tUnit, &pObj);

	/* check driver state */
	if (err == TM_OK) {
		if (pObj == Null || pObj->init == False)
			err = TDA182I2_ERR_NOT_INITIALIZED;
	}

	if ((err == TM_OK) && (pObj != Null))
		pObj->init = False;

	/* return value */
	return err;
}

/* -------------------------------------------------------------------------
 * FUNCTION:    TDA182I2GetInstance:
 *
 * DESCRIPTION: get the instance
 *
 * RETURN:      always True
 *
 * NOTES:
 * ------------------------------------------------------------------------- */
tmErrorCode_t TDA182I2GetInstance(tmUnitSelect_t tUnit,	/* I: Unit number */
		  pptmTDA182I2Object_t ppDrvObject	/* I: Device Object */)
{
	tmErrorCode_t err = TDA182I2_ERR_NOT_INITIALIZED;
	ptmTDA182I2Object_t pObj = Null;
	UInt32 uLoopCounter = 0;

	/* get instance */
	for (uLoopCounter = 0; uLoopCounter < TDA182I2_MAX_UNITS;
	     uLoopCounter++) {
		pObj = &gTDA182I2Instance[uLoopCounter];
		if (pObj->init == True
		    && pObj->tUnit == GET_INDEX_TYPE_TUNIT(tUnit)) {
			pObj->tUnitW = tUnit;

			*ppDrvObject = pObj;
			err = TM_OK;
			break;
		}
	}

	/* return value */
	return err;
}
