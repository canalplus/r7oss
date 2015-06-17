/************************************************************************
Copyright (C) 2012 STMicroelectronics. All Rights Reserved.

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

Source file name : stv0910_util.h
Author :

Header for low level driver of STV0910

Date        Modification                                    Name
----        ------------                                    --------
23-Apr-12   Created

************************************************************************/
#ifndef _STV0910_UTIL_H
#define _STV0910_UTIL_H

#include "stv0910_drv.h"
#include <stv0910_init.h>
#include <fe_sat_tuner.h>
/* Counter enum */
typedef enum {
	FE_STV0910_COUNTER1 = 0,
	FE_STV0910_COUNTER2 = 1
} FE_STV0910_ERRORCOUNTER;

/*PER/BER enum*/
typedef enum {
	FE_STV0910_BER = 0,
	FE_STV0910_BER_BIT7 = 1,
	FE_STV0910_PER = 2,
	FE_STV0910_BYTEER = 3
} FE_STV0910_BerPer;

/*CNR estimation enum*/
typedef enum {
	FE_STV0910_UNNORMDATAM,
	FE_STV0910_UNNORMDATAMSQ,
	FE_STV0910_NORMDATAM,
	FE_STV0910_NORMDATAMSQ,

	FE_STV0910_UNNORMPLHM,
	FE_STV0910_UNNORMPLHMSQ,
	FE_STV0910_NORMPLHM,
	FE_STV0910_NORMPLHMSQ
} FE_STV0910_CnrRegUsed;

/****************************************************************
  Util FUNCTIONS
 ****************************************************************/

U32 FE_STV0910_GetMclkFreq(STCHIP_Handle_t hChip, U32 ExtClk);

void FE_STV0910_SetSymbolRate(STCHIP_Handle_t hChip, U32 MasterClock,
			      U32 SymbolRate, FE_STV0910_DEMOD_t Demod);
BOOL FE_STV0910_WaitForLock(STCHIP_Handle_t hChip, FE_STV0910_DEMOD_t Demod,
		    S32 DemodTimeOut, S32 FecTimeOut, U32 Satellite_Scan);
U32 FE_STV0910_CarrierWidth(U32 SymbolRate, FE_STV0910_RollOff_t RollOff);

S32 FE_STV0910_GetCarrierFrequency(STCHIP_Handle_t hChip, U32 MasterClock,
				   FE_STV0910_DEMOD_t Demod);
U32 FE_STV0910_GetSymbolRate(STCHIP_Handle_t hChip, U32 MasterClock,
			     FE_STV0910_DEMOD_t Demod);
S32 FE_STV0910_CarrierGetQuality(STCHIP_Handle_t hChip,
				 FE_STV0910_LOOKUP_t *lookup,
				 FE_STV0910_DEMOD_t Demod);
U32 FE_STV0910_GetErrorCount(STCHIP_Handle_t hChip,
			     FE_STV0910_ERRORCOUNTER Counter,
			     FE_STV0910_DEMOD_t Demod);
U8 FE_STV0910_GetOptimCarrierLoop(S32 SymbolRate, FE_STV0910_ModCod_t ModCode,
				  S32 Pilots);

FE_STV0910_TrackingStandard_t FE_STV0910_GetStandard(STCHIP_Handle_t hChip,
						     FE_STV0910_DEMOD_t Demod);

S32 FE_STV0910_GetPADC(STCHIP_Handle_t hChip, FE_STV0910_LOOKUP_t *lookup,
		       FE_STV0910_DEMOD_t Demod);

S32 FE_STV0910_SetHMRFilter(STCHIP_Handle_t hChip, TUNER_Handle_t hTuner,
			    STCHIP_Handle_t hChipVGLNA,
			    FE_STV0910_DEMOD_t Demod);
#ifdef STM_FE_DEFINED_FOR_FUTURE_USE
FE_STV0910_Error_t FE_STV0910_SetReg(FE_STV0910_Handle_t Handle, U16 Reg,
				     U32 Val);
FE_STV0910_Error_t FE_STV0910_GetReg(FE_STV0910_Handle_t Handle, U16 Reg,
				     U32 *Val);
FE_STV0910_Error_t FE_STV0910_SetField(FE_STV0910_Handle_t Handle, U32 Field,
				       S32 Val);
FE_STV0910_Error_t FE_STV0910_GetField(FE_STV0910_Handle_t Handle, U32 Field,
				       S32 *Val);
#endif
FE_STV0910_Error_t FE_STV0910_GetSignalInfoLite(FE_STV0910_Handle_t Handle,
						FE_STV0910_DEMOD_t Demod,
						FE_STV0910_SignalInfo_t *
						pInfo);

void FE_STV0910_GetErrors(double *Errors, double *bits, double *Packets,
			  double *Ber);
void FE_STV0910_GetErrors2(double *Errors, double *bits, double *Packets,
			   double *Ber);

#endif /* _STV0910_UTIL_H */
