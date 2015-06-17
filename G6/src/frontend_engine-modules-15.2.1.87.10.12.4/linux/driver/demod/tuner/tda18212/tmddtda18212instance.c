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
/* FILE NAME:    tmddTDA182I2Instance.c */
/*  */
/* DESCRIPTION:  define the static Objects */
/*  */
/* DOCUMENT REF: DVP Software Coding Guidelines v1.14 */
/* DVP Board Support Library Architecture Specification v0.5 */
/*  */
/* NOTES: */
/* -------------------------------------------------------------------------- */
#include <linux/kernel.h>
#include "tmnxtypes.h"
#include "tmcompid.h"
#include "tmfrontend.h"
#include "tmunitparams.h"
#include "tmbslfrontendtypes.h"

#include "tmddtda18212.h"
#include "tmddtda18212local.h"

#include "tmddtda18212instance.h"

/*-----------------------------------------------------------------------------
 * Global data:
 *----------------------------------------------------------------------------*/

/* default instance */
tmddTDA182I2Object_t gddTDA182I2Instance[] = {
	{
	 (tmUnitSelect_t) (-1),	/* Unit not set */
	 (tmUnitSelect_t) (-1),	/* UnitW not set */
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
	 tmddTDA182I2_PowerStandbyWithXtalOn,	/* curPowerState */
	 True,			/* bIRQWait */
	 {
	  {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0},/* 10 x 0 */
	  {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0},/* 10 x 0 */
	  {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0},/* 10 x 0 */
	  {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0},/* 10 x 0 */
	  {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0},/* 10 x 0 */
	  {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0},/* 10 x 0 */
	  {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}	   /* 8 x 0  */
	  }
	 }
	,
	{
	 (tmUnitSelect_t) (-1),	/* Unit not set */
	 (tmUnitSelect_t) (-1),	/* UnitW not set */
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
	 tmddTDA182I2_PowerStandbyWithXtalOn,	/* curPowerState */
	 True,			/* bIRQWait */
	 {			/* I2 MAP */
	  {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0},/* 10 x 0 */
	  {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0},/* 10 x 0 */
	  {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0},/* 10 x 0 */
	  {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0},/* 10 x 0 */
	  {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0},/* 10 x 0 */
	  {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0},/* 10 x 0 */
	  {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}	   /* 8 x 0  */
	  }
	 }
};

/*----------------------------------------------------------------------------
 * FUNCTION:    ddTDA182I2AllocInstance:
 *
 * DESCRIPTION: allocate new instance
 *
 * RETURN:
 *
 * NOTES:
 * -------------------------------------------------------------------------- */
tmErrorCode_t ddTDA182I2AllocInstance(tmUnitSelect_t tUnit,/*I:Unit#*/
	      pptmddTDA182I2Object_t ppDrvObject	/* I: Device Object */)
{
	tmErrorCode_t err = ddTDA182I2_ERR_BAD_UNIT_NUMBER;
	ptmddTDA182I2Object_t pObj = Null;
	UInt32 uLoopCounter = 0;

	/* Find a free instance */
	for (uLoopCounter = 0; uLoopCounter < TDA182I2_MAX_UNITS;
	     uLoopCounter++) {
		pObj = &gddTDA182I2Instance[uLoopCounter];
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

/* --------------------------------------------------------------------------
 * FUNCTION:    ddTDA182I2DeAllocInstance:
 *
 * DESCRIPTION: deallocate instance
 *
 * RETURN:      always TM_OK
 *
 * NOTES:
 * -------------------------------------------------------------------------- */
tmErrorCode_t ddTDA182I2DeAllocInstance(tmUnitSelect_t tUnit/*I:Unit#*/)
{
	tmErrorCode_t err = ddTDA182I2_ERR_BAD_UNIT_NUMBER;
	ptmddTDA182I2Object_t pObj = Null;

	/* check input parameters */
	err = ddTDA182I2GetInstance(tUnit, &pObj);

	/* check driver state */
	if (err == TM_OK)
		if (pObj == Null || pObj->init == False)
			err = ddTDA182I2_ERR_NOT_INITIALIZED;

	if ((err == TM_OK) && (pObj != Null))
		pObj->init = False;

	/* return value */
	return err;
}

/* --------------------------------------------------------------------------
 * FUNCTION:    ddTDA182I2GetInstance:
 *
 * DESCRIPTION: get the instance
 *
 * RETURN:      always True
 *
 * NOTES:
 * -------------------------------------------------------------------------- */
tmErrorCode_t ddTDA182I2GetInstance(tmUnitSelect_t tUnit,/*I:Unit#*/
		    pptmddTDA182I2Object_t ppDrvObject	/* I: Device Object */)
{
	tmErrorCode_t err = ddTDA182I2_ERR_NOT_INITIALIZED;
	ptmddTDA182I2Object_t pObj = Null;
	UInt32 uLoopCounter = 0;

	/* get instance */
	for (uLoopCounter = 0; uLoopCounter < TDA182I2_MAX_UNITS;
	     uLoopCounter++) {
		pObj = &gddTDA182I2Instance[uLoopCounter];
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
