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

Source file name : stv0900_util.h
Author :

Header for low level driver of STV0900

Date        Modification                                    Name
----        ------------                                    --------
30-May-12   Created

************************************************************************/
#ifndef _STV0900_UTIL_H
#define _STV0900_UTIL_H

#include "stv0900_drv.h"
#include <stv0900_init.h>
#include <fe_sat_tuner.h>
	/* Counter enum */
typedef enum {
	FE_STV0900_COUNTER1 = 0,
	FE_STV0900_COUNTER2 = 1
} FE_STV0900_ERRORCOUNTER;

	/*PER/BER enum */
typedef enum {
	FE_STV0900_BER,
	FE_STV0900_PER
} FE_STV0900_BerPer;

	/*CNR estimation enum */
typedef enum {
	FE_STV0900_UNNORMDATAM,
	FE_STV0900_UNNORMDATAMSQ,
	FE_STV0900_NORMDATAM,
	FE_STV0900_NORMDATAMSQ,

	FE_STV0900_UNNORMPLHM,
	FE_STV0900_UNNORMPLHMSQ,
	FE_STV0900_NORMPLHM,
	FE_STV0900_NORMPLHMSQ
} FE_STV0900_CnrRegUsed;

	/****************************************************************
						Util FUNCTIONS
	****************************************************************/

#ifdef CHIP_STAPI
STCHIP_Error_t FE_STV0900_RepeaterFn(STCHIP_Handle_t hChip, BOOL State,
				     unsigned char *Buffer);
STCHIP_Error_t FE_STV0900_Repeater2Fn(STCHIP_Handle_t hChip, BOOL State,
				      unsigned char *Buffer);
#else
STCHIP_Error_t FE_STV0900_RepeaterFn(STCHIP_Handle_t hChip, BOOL State);
STCHIP_Error_t FE_STV0900_Repeater2Fn(STCHIP_Handle_t hChip, BOOL State);
#endif

U32 FE_STV0900_GetMclkFreq(STCHIP_Handle_t hChip, U32 ExtClk);

void FE_STV0900_SetTuner(STCHIP_Handle_t hChip,
			 TUNER_Handle_t hTuner,
			 FE_STV0900_Tuner_t TunerType,
			 FE_STV0900_DEMOD_t Demod,
			 U32 Frequency, U32 Bandwidth);

void FE_STV0900_SetSymbolRate(STCHIP_Handle_t hChip, U32 MasterClock,
			      U32 SymbolRate, FE_STV0900_DEMOD_t Demod);
BOOL FE_STV0900_WaitForLock(STCHIP_Handle_t hChip, FE_STV0900_DEMOD_t Demod,
			    S32 DemodTimeOut, S32 FecTimeOut);
void FE_STV0900_SetTunerType(STCHIP_Handle_t hChip,
			     FE_STV0900_Tuner_t TunerType, U8 I2cAddr,
			     U32 Reference, U8 OutClkDiv,
			     FE_STV0900_DEMOD_t Demod);
U32 FE_STV0900_CarrierWidth(U32 SymbolRate, FE_STV0900_RollOff_t RollOff);

S32 FE_STV0900_GetCarrierFrequency(STCHIP_Handle_t hChip, U32 MasterClock,
				   FE_STV0900_DEMOD_t Demod);
U32 FE_STV0900_GetSymbolRate(STCHIP_Handle_t hChip, U32 MasterClock,
			     FE_STV0900_DEMOD_t Demod);
S32 FE_STV0900_CarrierGetQuality(STCHIP_Handle_t hChip,
				 FE_STV0900_LOOKUP_t *lookup,
				 FE_STV0900_DEMOD_t Demod);
U32 FE_STV0900_GetErrorCount(STCHIP_Handle_t hChip,
			     FE_STV0900_ERRORCOUNTER Counter,
			     FE_STV0900_DEMOD_t Demod);

void FE_STV0900_ActivateS2ModCode(STCHIP_Handle_t hChip,
				  FE_STV0900_DEMOD_t Demod);
void FE_STV0900_StopALL_S2_Modcod(STCHIP_Handle_t hChip,
				  FE_STV0900_DEMOD_t Demod);
void FE_STV0900_ActivateS2ModCodeSingle(STCHIP_Handle_t hChip,
					FE_STV0900_DEMOD_t Demod);

BOOL FE_STV0900_LDPCPowerMonitoring(STCHIP_Handle_t hChip,
				    FE_STV0900_DEMOD_t Demod);

#endif /* _STV0900_UTIL_H */
