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

Source file name : tda18212_tuner.h
Author :



Date        Modification                                    Name
----        ------------                                    --------
************************************************************************/
/**
file tda18212_tuner.h
*/
#ifndef _TDA18212_TUNER_H
#define _TDA18212_TUNER_H

/*      #include "chip.h"*/
#include "fe_tc_tuner.h"

#if defined(CHIP_STAPI)		/*For STAPI use only */
/*              #include "dbtypes.h"    for Install function*/
/*              #include "stfrontend.h" for Install Function*/
#endif

/* TDA18212 includes */

#include "tmnxtypes.h"
#include "tmcompid.h"
#include "tmfrontend.h"
#include "tmbslfrontendtypes.h"
#include "tmddtda18212.h"
#include "tmbsltda18212.h"

/* constants --------------------------------------------------------------- */

typedef enum {
	RTDA18212_REGISTER00,
	RTDA18212_REGISTER01,
	RTDA18212_REGISTER02,
	RTDA18212_REGISTER03,
	RTDA18212_REGISTER04,
	RTDA18212_REGISTER05,
	RTDA18212_REGISTER06,
	RTDA18212_REGISTER07,
	RTDA18212_REGISTER08,
	RTDA18212_REGISTER09,
	RTDA18212_REGISTER0A,
	RTDA18212_REGISTER0B,
	RTDA18212_REGISTER0C,
	RTDA18212_REGISTER0D,
	RTDA18212_REGISTER0E,
	RTDA18212_REGISTER0F,
	RTDA18212_REGISTER10,
	RTDA18212_REGISTER11,
	RTDA18212_REGISTER12,
	RTDA18212_REGISTER13,
	RTDA18212_REGISTER14,
	RTDA18212_REGISTER15,
	RTDA18212_REGISTER16,
	RTDA18212_REGISTER17,
	RTDA18212_REGISTER18,
	RTDA18212_REGISTER19,
	RTDA18212_REGISTER1A,
	RTDA18212_REGISTER1B,
	RTDA18212_REGISTER1C,
	RTDA18212_REGISTER1D,
	RTDA18212_REGISTER1E,
	RTDA18212_REGISTER1F,
	RTDA18212_REGISTER20,
	RTDA18212_REGISTER21,
	RTDA18212_REGISTER22,
	RTDA18212_REGISTER23,
	RTDA18212_REGISTER24,
	RTDA18212_REGISTER25,
	RTDA18212_REGISTER26,
	RTDA18212_REGISTER27,
	RTDA18212_REGISTER28,
	RTDA18212_REGISTER29,
	RTDA18212_REGISTER2A,
	RTDA18212_REGISTER2B,
	RTDA18212_REGISTER2C,
	RTDA18212_REGISTER2D,
	RTDA18212_REGISTER2E,
	RTDA18212_REGISTER2F,
	RTDA18212_REGISTER30,
	RTDA18212_REGISTER31,
	RTDA18212_REGISTER32,
	RTDA18212_REGISTER33,
	RTDA18212_REGISTER34,
	RTDA18212_REGISTER35,
	RTDA18212_REGISTER36,
	RTDA18212_REGISTER37,
	RTDA18212_REGISTER38,
	RTDA18212_REGISTER39,
	RTDA18212_REGISTER3A,
	RTDA18212_REGISTER3B,
	RTDA18212_REGISTER3C,
	RTDA18212_REGISTER3D,
	RTDA18212_REGISTER3E,
	RTDA18212_REGISTER3F,
	RTDA18212_REGISTER40,
	RTDA18212_REGISTER41,
	RTDA18212_REGISTER42,
	RTDA18212_REGISTER43,
	NB_REGS_TDA18212
} RTDA18212;

#define TDA18212_NBREGS		NB_REGS_TDA18212
#define TDA18212_NBFIELDS	213;

/* Data Structures ---------------------------------------------------------- */

/* Structure to handle some information to allow easy transfer
 * between ST and NXP APIs */
struct TDA18212_TUNER {
	tmUnitSelect_t tUnit;
	tmbslFrontEndDependency_t TDATunerFunc;
	void *TunerHandle;
};
typedef struct TDA18212_TUNER TDA18212_TUNER_tds;
typedef TDA18212_TUNER_tds *pTDA18212_TUNER_td;
	/* create instance of tuner register mappings */

TUNER_Error_t TDA18212_TunerInit(void *pTunerInit,
				 STCHIP_Handle_t *TunerHandle);

TUNER_Error_t TDA18212_TunerSetFrequency(STCHIP_Handle_t hTuner, U32 Frequency);
U32 TDA18212_TunerGetFrequency(STCHIP_Handle_t hTuner);
BOOL TDA18212_TunerGetStatus(STCHIP_Handle_t hTuner);
TUNER_Error_t TDA18212_TunerSetBandWidth(STCHIP_Handle_t hTuner,
					 U32 Band_Width);
U32 TDA18212_TunerGetBandWidth(STCHIP_Handle_t hTuner);
TUNER_Error_t TDA18212_SwitchTunerToDVBT2(STCHIP_Handle_t hTuner);
TUNER_Error_t TDA18212_SwitchTunerToDVBT(STCHIP_Handle_t hTuner);
TUNER_Error_t TDA18212_SwitchTunerToDVBC(STCHIP_Handle_t hTuner);
TUNER_Error_t TDA18212_TunerSetStandby(STCHIP_Handle_t hTuner, U8 StandbyOn);
TUNER_Error_t TDA18212_TunerWrite(STCHIP_Handle_t hTuner);
TUNER_Error_t TDA18212_TunerRead(STCHIP_Handle_t hTuner);

TUNER_Model_t TDA18212_TunerGetModel(STCHIP_Handle_t hTuner);
void TDA18212_TunerSetIQ_Wiring(STCHIP_Handle_t hTuner, S8 IQ_Wiring);
S8 TDA18212_TunerGetIQ_Wiring(STCHIP_Handle_t hTuner);
void TDA18212_TunerSetReferenceFreq(STCHIP_Handle_t hTuner, U32 Reference);
U32 TDA18212_TunerGetReferenceFreq(STCHIP_Handle_t hTuner);
void TDA18212_TunerSetIF_Freq(STCHIP_Handle_t hTuner, U32 IF);
U32 TDA18212_TunerGetIF_Freq(STCHIP_Handle_t hTuner);
BOOL TDA18212_TunerGetPowerLevel(STCHIP_Handle_t hTuner, S32 *RF_Input_Level);
TUNER_Error_t TDA18212_TunerTerm(STCHIP_Handle_t hTuner);
U32 GetTDA18212TunerHandle(tmUnitSelect_t tUnit);
tmTDA182I2StandardMode_t TDA18212_Tuner_GetStandardMode(STCHIP_Handle_t hTuner,
							U32 Band_Width);
S32 TDA18212_TunerEstimateRfPower(STCHIP_Handle_t hTuner, U16 RfAgc, U16 IfAgc);

#if defined(CHIP_STAPI)		/*For STAPI use only */
/*ST_ErrorCode_t STFRONTEND_TUNER_TDA18212_Install(STFRONTEND_tunerDbase_t
 * *Tuner, STFRONTEND_TunerType_t TunerType);*/
/*ST_ErrorCode_t STFRONTEND_TUNER_TDA18212_Uninstall(STFRONTEND_tunerDbase_t
 * *Tuner);*/
#endif

#endif
/* End of tda18212_tuner.h */
