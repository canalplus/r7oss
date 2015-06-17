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
#ifndef _TMBSL_TDA182I2LOCAL_H
#define _TMBSL_TDA182I2LOCAL_H

/*----------------------------------------------------------------------------*/
/* Standard include files:                                                    */
/*----------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------*/
/* Project include files:                                                    */
/*---------------------------------------------------------------------------*/

#ifdef __cplusplus
extern "C"
{
#endif
/*---------------------------------------------------------------------------*/
/* Types and defines:                                                        */
/*---------------------------------------------------------------------------*/
#define TDA182I2_MUTEX_TIMEOUT  TMBSL_FRONTEND_MUTEX_TIMEOUT_INFINITE
#ifdef TMBSL_TDA18272
#define TMBSL_TDA182I2_COMPONENT_NAME_STR "TDA18272"
#else				/* TMBSL_TDA18272 */
#define TMBSL_TDA182I2_COMPONENT_NAME_STR "TDA18212"
#endif				/* TMBSL_TDA18272 */
#define _SYSTEMFUNC (pObj->SystemFunc)
#define POBJ_SRVFUNC_SIO (pObj->sRWFunc)
#define POBJ_SRVFUNC_STIME (pObj->sTime)
#define P_DBGPRINTEx pObj->sDebug.Print
#define P_DBGPRINTVALID ((pObj != Null) && (pObj->sDebug.Print != Null))
/*-------------*/
/* ERROR CODES */
/*-------------*/
#define TDA182I2_MAX_UNITS                          2
typedef struct _tmTDA182I2Object_t {
	tmUnitSelect_t tUnit;
	tmUnitSelect_t tUnitW;
	ptmbslFrontEndMutexHandle pMutex;
	Bool init;
	tmbslFrontEndIoFunc_t sRWFunc;
	tmbslFrontEndTimeFunc_t sTime;
	tmbslFrontEndDebugFunc_t sDebug;
	tmbslFrontEndMutexFunc_t sMutex;
	tmTDA182I2PowerState_t curPowerState;
	tmTDA182I2PowerState_t minPowerState;
	UInt32 uRF;
	tmTDA182I2StandardMode_t StandardMode;
	Bool Master;
	UInt8 LT_Enable;
	UInt8 PSM_AGC1;
	UInt8 AGC1_6_15dB;
	tmTDA182I2StdCoefficients Std_Array[tmTDA182I2_StandardMode_Max];
} tmTDA182I2Object_t, *ptmTDA182I2Object_t, **pptmTDA182I2Object_t;

extern tmErrorCode_t TDA182I2MutexAcquire(ptmTDA182I2Object_t pObj,
					  UInt32 timeOut);
extern tmErrorCode_t TDA182I2MutexRelease(ptmTDA182I2Object_t pObj);

#ifdef __cplusplus
}
#endif

#endif /* _TMBSL_TDA182I2LOCAL_H */
