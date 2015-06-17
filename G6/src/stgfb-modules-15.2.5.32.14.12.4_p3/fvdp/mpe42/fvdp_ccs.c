/***********************************************************************
 *
 * File: fvdp/mpe42/fvdp_ccs.c
 * Copyright (c) 2011 by STMicroelectronics. All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

/* Includes ----------------------------------------------------------------- */
#include "fvdp_regs.h"
#include "fvdp_common.h"
#include "fvdp_hostupdate.h"
#include "../fvdp_vqdata.h"
#include "fvdp_ccs.h"
/* Private Types ------------------------------------------------------------ */

/* Private Constants -------------------------------------------------------- */

/* Private Macros ----------------------------------------------------------- */
#define CCS_REG32_Write(Addr,Val)                  REG32_Write(Addr+CCS_FE_BASE_ADDRESS,Val)
#define CCS_REG32_Read(Addr)                       REG32_Read(Addr+CCS_FE_BASE_ADDRESS)
#define CCS_REG32_Set(Addr,BitsToSet)              REG32_Set(Addr+CCS_FE_BASE_ADDRESS,BitsToSet)
#define CCS_REG32_Clear(Addr,BitsToClear)          REG32_Clear(Addr+CCS_FE_BASE_ADDRESS,BitsToClear)
#define CCS_REG32_ClearAndSet(Addr,BitsToClear,Offset,ValueToSet) \
                                                          REG32_ClearAndSet(Addr+CCS_FE_BASE_ADDRESS,BitsToClear,Offset,ValueToSet)
#define CCS_REG32_SetField(Addr,Field,ValueToSet)  REG32_SetField(Addr+CCS_FE_BASE_ADDRESS,Field,ValueToSet)

/* Private Variables (static)------------------------------------------------ */

/* Private Function Prototypes ---------------------------------------------- */

/* Global Variables --------------------------------------------------------- */


/* Functions ---------------------------------------------------------------- */
/*******************************************************************************
Name        : CCS_Init
Description : Initialize CCS software module and call CCS hardware init
                 function
Parameters  : FVDP_CH_t  CH
Assumptions :
Limitations :
Returns     : FVDP_Result_t result
*******************************************************************************/
FVDP_Result_t CCS_Init ( FVDP_CH_t  CH)
{
    FVDP_Result_t result = FVDP_OK;

    if (CH == FVDP_MAIN)
    {
        CCS_REG32_Clear(CCS_CTRL,CCS_EN_MASK);
        CCS_REG32_Clear(CCS_CTRL,CCS_DEMO_EN_MASK);
        CCS_REG32_Clear(CCS_CTRL,CCS_DEMO_HACT_WIDTH_MASK);
        CCS_REG32_Clear(CCS_CTRL,CCS_QUANT_THRESH0_LSB_MASK | CCS_QUANT_THRESH1_LSB_MASK);
        HostUpdate_SetUpdate_FE(FE_HOST_UPDATE_CCS, SYNC_UPDATE);

        CCS_SetData(CH, 0, 0);
    }
    else
    {
        result = FVDP_FEATURE_NOT_SUPPORTED;
    }

    return result;
}

/*******************************************************************************
Name        : CCS_SetState
Description : Initialize CCS software module and call CCS hardware init
                 function
Parameters  : CH: channel,  options: input/output
Assumptions :
Limitations :
Returns     : FVDP_Result_t result
*******************************************************************************/
FVDP_Result_t CCS_SetState(FVDP_CH_t CH, FVDP_PSIState_t selection)
{
    FVDP_Result_t result = FVDP_OK;

    if (CH == FVDP_MAIN)
    {
        if (selection == FVDP_PSI_ON)
        {
            CCS_REG32_Set(CCS_CTRL,CCS_EN_MASK);
        }
        else if ((selection == FVDP_PSI_DEMO_MODE_ON_LEFT) || (selection == FVDP_PSI_DEMO_MODE_ON_RIGHT))
        {
            CCS_REG32_Set(CCS_CTRL,CCS_DEMO_EN_MASK);
            CCS_REG32_Set(CCS_CTRL,CCS_EN_MASK);
        }
        else    // (selection == FVDP_PSI_OFF)
        {
            CCS_REG32_Clear(CCS_CTRL,CCS_EN_MASK);
            CCS_REG32_Clear(CCS_CTRL,CCS_DEMO_EN_MASK);
        }
        HostUpdate_SetUpdate_FE(FE_HOST_UPDATE_CCS, SYNC_UPDATE);
    }
    else
    {
        result = FVDP_FEATURE_NOT_SUPPORTED;
    }

    return result;
}

/*******************************************************************************
Name        : CCS_SetData
Description : Set CCS parameters
Parameters  : physical channel
Assumptions :
Limitations :
Returns     : FVDP_Result_t result
*******************************************************************************/
FVDP_Result_t CCS_SetData(FVDP_CH_t CH,
				uint16_t CCSQuantValue0, uint16_t CCSQuantValue1)
{
    FVDP_Result_t result = FVDP_OK;
    uint32_t CCSQuantValue;

	if (!CCS_IsAvailable(CH))
	{
		return FVDP_FEATURE_NOT_SUPPORTED;
	}

    CCSQuantValue = (CCSQuantValue1 << CCS_QUANT_THRESH1_OFFSET) | (CCSQuantValue0 << CCS_QUANT_THRESH0_OFFSET);
	CCS_REG32_Write(CCS_QUANT_THRESH,CCSQuantValue);

	return result;
}

/*******************************************************************************
Name        : CCS_IsAvailable
Description : Check CCS feature available
Parameters  : physical channel
Assumptions :
Limitations :
Returns     : FVDP_Result_t result
*******************************************************************************/
bool CCS_IsAvailable(FVDP_CH_t CH)
{
    return (CH == FVDP_MAIN) && (CCS_GetRevision(CH) == 0x100);
}

/*******************************************************************************
Name        : CCS_GetRevision
Description : Read CCS HW REVISION
Parameters  : physical channel
Assumptions :
Limitations :
Returns     : FVDP_Result_t result
*******************************************************************************/
uint32_t CCS_GetRevision(FVDP_CH_t CH)
{
    return CCS_REG32_Read(CCS_REVISION);
}
