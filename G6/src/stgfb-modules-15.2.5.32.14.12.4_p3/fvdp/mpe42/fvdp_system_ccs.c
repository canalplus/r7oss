/***********************************************************************
 *
 * File: fvdp/mpe42/fvdp_system_ccs.c
 * Copyright (c) 2012 by STMicroelectronics. All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/


/* Includes ----------------------------------------------------------------- */
#include "fvdp_common.h"
#include "fvdp_types.h"
#include "fvdp_system.h"
#include "fvdp_driver.h"
#include "fvdp_ccs.h"
#include "fvdp_ccs_vqtab.h"

/* Private Macros ----------------------------------------------------------- */
#define CCS_TEST FALSE

/* Private Enums ------------------------------------------------------------ */


/* Private Structure -------------------------------------------------------- */

/* Private Variables (static)------------------------------------------------ */

/* Private Function Prototypes ---------------------------------------------- */
static FVDP_Result_t system_PSI_CCS_Update(FVDP_HandleContext_t* HandleContext_p,
                                           VQ_CCS_Parameters_t* pCCSVQData);
static bool system_PSI_CCS_IsApplicable(FVDP_HandleContext_t* HandleContext_p);
#if defined(CCS_ADAPTIVE)
static bool system_PSI_CCS_IsAdaptive(FVDP_CH_t CH);
#endif

/* Functions ---------------------------------------------------------------- */
/*******************************************************************************
Name        : SYSTEM_PSI_CCS_Init
Description : Initialize CCS software module and call CCS hardware init
                 function
Parameters  :FVDP_HandleContext_t* HandleContext_p
Assumptions :
Limitations :
Returns     : FVDP_OK
*******************************************************************************/
FVDP_Result_t SYSTEM_PSI_CCS_Init(FVDP_HandleContext_t* HandleContext_p)
{
    FVDP_Result_t   result = FVDP_OK;
    FVDP_CH_t       CH;

    CH = HandleContext_p->CH;

    if (CH != FVDP_MAIN)
        return result;

    CCS_Init(CH);

    //get the pointer to CCS structure
    HandleContext_p->PSIFeature.CCSVQData = CCSVQTable;
    HandleContext_p->UpdateFlags.PSIVQUpdateFlag |= UPDATE_PSI_VQ_CCS;
    HandleContext_p->PSIState[FVDP_CCS] = FVDP_PSI_OFF;

    result = SYSTEM_PSI_CCS (HandleContext_p);

    return result;
}

/*******************************************************************************
Name        : SYSTEM_PSI_CCS
Description : handling PSI

Parameters  : FVDP_HandleContext_t* HandleContext_p
Assumptions : This function needs to be called during open channel
Limitations :
Returns     : FVDP_OK
*******************************************************************************/
FVDP_Result_t SYSTEM_PSI_CCS(FVDP_HandleContext_t* HandleContext_p)
{
    FVDP_Result_t   ErrorCode = FVDP_OK;
    FVDP_CH_t       CH = HandleContext_p->CH;
    VQ_CCS_Parameters_t* pCCSVQData;
    FVDP_HW_PSIConfig_t*       pHW_PSIConfig;
    bool Updateflag = FALSE;
    bool TmpFlag = FALSE;

    pHW_PSIConfig = SYSTEM_GetHWPSIConfig(CH);

#if(CCS_TEST == TRUE)
        if( REG32_Read(IVP_PATGEN_INC_R + MIVP_BASE_ADDRESS) != 0)
        {
             HandleContext_p->PSIState[FVDP_CCS] = FVDP_PSI_ON;
             HandleContext_p->PSIControlLevel.CCSLevel = REG32_Read(IVP_PATGEN_INC_G + MIVP_BASE_ADDRESS);
        }
        else
        {
             HandleContext_p->PSIState[FVDP_CCS] = FVDP_PSI_OFF;
        }

        HandleContext_p->PSIFeature.CCSVQData = CCSVQTable;
        HandleContext_p->UpdateFlags.PSIVQUpdateFlag |= UPDATE_PSI_VQ_CCS;

#endif
    TmpFlag = system_PSI_CCS_IsApplicable(HandleContext_p);
    if (TmpFlag != TRUE)
    { 
        return FVDP_FEATURE_NOT_SUPPORTED;
    }

    //CCS state control
    if(pHW_PSIConfig->HWPSIState[FVDP_CCS]!= HandleContext_p->PSIState[FVDP_CCS])
    {
        //Update HW CCS PIS status
        pHW_PSIConfig->HWPSIState[FVDP_CCS] = HandleContext_p->PSIState[FVDP_CCS];
        CCS_SetState(CH, pHW_PSIConfig->HWPSIState[FVDP_CCS]);
    }

    if(HandleContext_p->UpdateFlags.PSIVQUpdateFlag & UPDATE_PSI_VQ_CCS)
    {
        //Update HW CCS PIS VQ Date
        pHW_PSIConfig->HWPSIFeature.CCSVQData = HandleContext_p->PSIFeature.CCSVQData;

        Updateflag = TRUE;
    }

    if(Updateflag == TRUE)
    {
        //get the pointer to CCS structure
        pCCSVQData = &(pHW_PSIConfig->HWPSIFeature.CCSVQData);
        ErrorCode = system_PSI_CCS_Update(HandleContext_p, pCCSVQData);
        if((ErrorCode == FVDP_OK) && (HandleContext_p->UpdateFlags.PSIVQUpdateFlag &UPDATE_PSI_VQ_CCS))
        {
            // Clear update flag
            HandleContext_p->UpdateFlags.PSIVQUpdateFlag &= ~UPDATE_PSI_VQ_CCS;
        }
    }
    return ErrorCode;
}

/*******************************************************************************
Name        : system_PSI_CCS_Update
Description : Update CCS Thresold value
Parameters  : HandleContext_p, pCCSVQData
Assumptions :
Limitations :
Returns     : TRUE - CCS can be applied
              FALSE - CCS cannot be applied
*******************************************************************************/
FVDP_Result_t system_PSI_CCS_Update(FVDP_HandleContext_t* HandleContext_p, VQ_CCS_Parameters_t* pCCSVQData)
{
    FVDP_Result_t ErrorCode = FVDP_OK;
    FVDP_CH_t     CH = HandleContext_p->CH;
    uint16_t    CCSQuantValue0, CCSQuantValue1;

    CCSQuantValue0 = pCCSVQData->StdMotionTh0;
    CCSQuantValue1 = pCCSVQData->StdMotionTh1;

    ErrorCode = CCS_SetData(CH, CCSQuantValue0, CCSQuantValue1);
    TRC( TRC_ID_ERROR, "CCS_SetData returns %d", ErrorCode );
    return ErrorCode;
}

/*******************************************************************************
Name        : system_PSI_CCS_IsApplicable
Description : Return if CCS can be applicable
Parameters  : HandleContext_p
Assumptions :
Limitations :
Returns     : TRUE - CCS can be applied
              FALSE - CCS cannot be applied
*******************************************************************************/
bool system_PSI_CCS_IsApplicable(FVDP_HandleContext_t* HandleContext_p)
{
    FVDP_Result_t ErrorCode = FVDP_OK;
    FVDP_CH_t CH = HandleContext_p->CH;

    if (HandleContext_p->FrameInfo.CurrentInputFrame_p == NULL)
    {
        ErrorCode = FVDP_VIDEO_INFO_ERROR;
        return ErrorCode;
    }

    ErrorCode = (CCS_IsAvailable(CH)
        && (HandleContext_p->FrameInfo.CurrentInputFrame_p->InputVideoInfo.ScanType == INTERLACED));

    return ErrorCode;
}

#if defined(CCS_ADAPTIVE)
/*******************************************************************************
Name        : system_PSI_CCS_IsAdaptive
Description : Return if CCS is in adaptive mode
Parameters  : CH
Assumptions :
Limitations :
Returns     :
*******************************************************************************/
bool system_PSI_CCS_IsAdaptive(FVDP_CH_t CH)
{
    FVDP_HW_PSIConfig_t*       pHW_PSIConfig;

    pHW_PSIConfig = SYSTEM_GetHWPSIConfig(CH);
    return CCS_IsAvailable(CH)
        && (pHW_PSIConfig->HWPSIState[CH] != FVDP_PSI_OFF)
        && (pHW_PSIConfig->HWPSIFeature.CCSVQData.EnableGlobalMotionAdaptive);
}
#endif
