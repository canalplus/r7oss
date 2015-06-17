/************************************************************************
Copyright(C) 2013 STMicroelectronics. All Rights Reserved.

This file is part of the stm_fe Library.

stm_fe is free software ; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

stm_fe is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY ; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with stm_fe ; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111 - 1307, USA.

The stm_fe Library may alternatively be licensed under a proprietary
license from ST.

Source file name : stv0913_util.h
Author :

Header for low level driver of STV0913

Date		Modification				Name
----		------------				--------
5 - Jun - 13	Created					HS

************************************************************************/
#ifndef _STV0913_UTIL_H
#define _STV0913_UTIL_H



	#include "stv0913_drv.h"
	#include "stv0913_init.h"
	#include "fe_sat_tuner.h"
	/* Counter enum */
	typedef enum {
		FE_STV0913_COUNTER1 = 0,
		FE_STV0913_COUNTER2 = 1
	} FE_STV0913_ERRORCOUNTER;

	/*PER/BER enum*/
	typedef enum {
		FE_STV0913_BER = 0,
		FE_STV0913_BER_BIT7 = 1,
		FE_STV0913_PER = 2,
		FE_STV0913_BYTEER = 3

	} FE_STV0913_BerPer;

	/*CNR estimation enum*/
	typedef enum {
		FE_STV0913_UNNORMDATAM,
		FE_STV0913_UNNORMDATAMSQ,
		FE_STV0913_NORMDATAM,
		FE_STV0913_NORMDATAMSQ,

		FE_STV0913_UNNORMPLHM,
		FE_STV0913_UNNORMPLHMSQ,
		FE_STV0913_NORMPLHM,
		FE_STV0913_NORMPLHMSQ

	} FE_STV0913_CnrRegUsed;

	/****************************************************************
						Util FUNCTIONS
	****************************************************************/

	U32 FE_STV0913_GetMclkFreq(STCHIP_Handle_t hChip, U32 ExtClk);

	void FE_STV0913_SetSymbolRate(STCHIP_Handle_t hChip, U32 MasterClock,
							U32 SymbolRate);
	BOOL FE_STV0913_WaitForLock(STCHIP_Handle_t hChip, S32 DemodTimeOut,
					S32 FecTimeOut, U32 Satellite_Scan);

	U32 FE_STV0913_CarrierWidth(U32 SymbolRate,
					FE_STV0913_RollOff_t RollOff);
	S32 FE_STV0913_GetCarrierFrequency(STCHIP_Handle_t hChip,
							U32 MasterClock);
	U32 FE_STV0913_GetSymbolRate(STCHIP_Handle_t hChip, U32 MasterClock);
	S32 FE_STV0913_CarrierGetQuality(STCHIP_Handle_t hChip,
						FE_STV0913_LOOKUP_t *lookup);
	U32 FE_STV0913_GetErrorCount(STCHIP_Handle_t hChip,
				FE_STV0913_ERRORCOUNTER Counter);
	U8 FE_STV0913_GetOptimCarrierLoop(S32 SymbolRate,
				FE_STV0913_ModCod_t ModCode, S32 Pilots);
	U32 FE_STV0913_GetSymbolRate(STCHIP_Handle_t hChip,
							U32 MasterClock);
	S32 FE_STV0913_GetPADC(STCHIP_Handle_t hChip,
						FE_STV0913_LOOKUP_t *lookup);
#ifdef STM_FE_DEFINED_FOR_FUTURE_USE
	FE_STV0913_Error_t FE_STV0913_SetReg(FE_STV0913_Handle_t Handle,
						U16 Reg, U32 Val);
	FE_STV0913_Error_t FE_STV0913_GetReg(FE_STV0913_Handle_t Handle,
							U16 Reg, U32 *Val);
	FE_STV0913_Error_t FE_STV0913_SetField(FE_STV0913_Handle_t Handle,
							U32 Field, S32 Val);
	FE_STV0913_Error_t FE_STV0913_GetField(FE_STV0913_Handle_t Handle,
							U32 Field, S32 *Val);
#endif

	FE_STV0913_Error_t FE_STV0913_GetSignalInfoLite(FE_STV0913_Handle_t
					Handle,	FE_STV0913_SignalInfo_t	*pInfo);

	void FE_STV0913_GetErrors(double *Errors, double *bits, double *Packets,
					double *Ber); /* in Pnl_FecSpy.c */

#endif /* _STV0913_UTIL_H */
