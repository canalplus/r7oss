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

Source file name : stv0367qam_util.h
Author :           LLA

367 lla file

Date        Modification                                    Name
----        ------------                                    --------
01-Aug-11   Created                                         Ankur
04-Jul-12   Updated to v1.9
************************************************************************/
#ifndef _STV0367QAM_UTIL_H
#define _STV0367QAM_UTIL_H

/*      #include <chip.h>*/
#include "stv0367qam_drv.h"
#include <fecab_commlla_str.h>

#define FE_367qam_MAXLOOKUPSIZE 50

typedef enum {
	COUNTER1 = 0,
	COUNTER2 = 1
} FE_367qam_ERRORCOUNTER_t;

	/* One point of the lookup table */
typedef struct {
	S32 realval;		/*      real value */
	S32 regval;		/*      binary value */
} FE_367qam_LOOKPOINT_t;

	/*      Lookup table definition */
typedef struct {
	S32 size;		/*      Size of the lookup table        */
	/*      Lookup table    */
	FE_367qam_LOOKPOINT_t table[FE_367qam_MAXLOOKUPSIZE];
} FE_367qam_LOOKUP_t;

	/*      Monitoring structure */
typedef struct {
	U32 FE_367qam_TotalPacketsNumber,
	    FE_367qam_TotalPacketsNumberOld,
	    FE_367qam_TotalPacketsNumberOffset,
	    FE_367qam_CorrectedPacketsNumber,
	    FE_367qam_CorrectedPacketsNumberOld,
	    FE_367qam_CorrectedPacketsNumberOffset,
	    FE_367qam_CorruptedPacketsNumber,
	    FE_367qam_CorruptedPacketsNumberOld,
	    FE_367qam_CorruptedPacketsNumberOffset,
	    FE_367qam_PERxE7_u32,
	    FE_367qam_RFLevelx100Percent_u32,
	    FE_367qam_SNRx10dB_u32,
	    FE_367qam_SNRx100Percent_u32,
	    FE_367qam_CorrectedBitsNumber,
	    FE_367qam_BERxE7_u32,
	    FE_367qam_Saturation, FE_367qam_WaitingTime_ms;
	S32 FE_367qam_RFLevelx10dBm_s32;
#ifdef HOST_PC
	double FE_367qam_BER_dbl;
	double FE_367qam_PER_dbl;
	double FE_367qam_RFLeveldBm_dbl;
	double FE_367qam_SNRdB_dbl;
#endif
} FE_367qam_Monitor;

U32 FE_367qam_GetMclkFreq(STCHIP_Handle_t hChip, U32 ExtClk_Hz);
U32 FE_367qam_GetADCFreq(STCHIP_Handle_t hChip, U32 ExtClk_Hz);
FE_CAB_Modulation_t FE_367qam_GetQamSize(STCHIP_Handle_t hChip);
U32 FE_367qam_AuxClkFreq(U32 DigFreq, U32 Prescaler, U32 Divider);
U32 FE_367qam_F22Freq(U32 DigFreq, U32 F22);
void FE_367qam_GetErrorCount(STCHIP_Handle_t hChip, FE_CAB_Modulation_t QAMSize,
			     U32 SymbolRate_Bds,
			     FE_367qam_Monitor *Monitor_results);
void FE_367qam_ClearCounters(STCHIP_Handle_t hChip,
			     FE_367qam_Monitor *Monitor_results);
U32 FE_367qam_SetSymbolRate(STCHIP_Handle_t hChip, U32 AdcClock,
			    U32 MasterClock, U32 SymbolRate,
			    FE_CAB_Modulation_t QAMSize);
U32 FE_367qam_GetSymbolRate(STCHIP_Handle_t hChip, U32 MasterClock);
void FE_367qam_GetCarrierToNoiseRatio_u32(STCHIP_Handle_t hChip,
					  FE_367qam_Monitor *Monitor_results,
					  FE_CAB_Modulation_t QAMSize);
U32 FE_367qam_GetDerotFreq(STCHIP_Handle_t hChip, U32 AdcClock);
void FE_367qam_GetRFLevel(STCHIP_Handle_t hChip, STCHIP_Handle_t hTuner,
			  FE_367qam_Monitor *Monitor_results);
void FE_367qam_OptimiseNByteAndGetBER(STCHIP_Handle_t hChip,
				      FE_CAB_Modulation_t QAMSize,
				      U32 SymbolRate_Bds,
				      FE_367qam_Monitor *Monitor_results);
void FE_367qam_GetPacketsCount(STCHIP_Handle_t hChip,
			       FE_367qam_Monitor *Monitor_results);
#ifdef HOST_PC
void FE_367qam_GetCarrierToNoiseRatio_dbl(STCHIP_Handle_t hChip,
					  FE_367qam_Monitor *Monitor_results,
					  FE_CAB_Modulation_t QAMSize);
#endif

#endif /* _STV0367QAM_UTIL_H */
