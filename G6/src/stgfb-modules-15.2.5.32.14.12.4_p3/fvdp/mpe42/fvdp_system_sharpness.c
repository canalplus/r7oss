/***********************************************************************
 *
 * File: fvdp/mpe42/fvdp_system_sharpness.c
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
#include "fvdp_sharpness.h"
#include "fvdp_sharpness_vqtab.h"

/* Private Macros ----------------------------------------------------------- */
#define SHARPNESS_TEST FALSE


/* Private Enums ------------------------------------------------------------ */


/* Private Structure -------------------------------------------------------- */

/* Private Variables (static)------------------------------------------------ */

/* Private Function Prototypes ---------------------------------------------- */

static FVDP_Result_t system_PSI_Sharpness_Update (FVDP_CH_t CH, const VQ_SHARPNESS_Parameters_t * pSharpnessData, int32_t SharpnessLevel);
static void          system_PSI_Sharpness_SetEnhancerData(VQ_EnhancerParameters_t * pMin, VQ_EnhancerParameters_t * pMax,
                            VQ_SharpnessStepGroup_t * pStepGroup, VQ_EnhancerParameters_t * pOutputParams);
static uint32_t      system_PSI_Sharpness_GetValueFromSteps(uint32_t Min, uint32_t Max, uint32_t NumOfSteps, uint32_t StepNum);

/* Functions ---------------------------------------------------------------- */
/*******************************************************************************
Name        : SYSTEM_PSI_Sharpness_Init
Description : Initialize SHARPNESS software module and call SHARPNESS hardware init
                 function
Parameters  :FVDP_HandleContext_t* HandleContext_p
Assumptions :
Limitations :
Returns     : FVDP_OK
*******************************************************************************/
FVDP_Result_t SYSTEM_PSI_Sharpness_Init (FVDP_HandleContext_t* HandleContext_p)
{
    FVDP_Result_t             result = FVDP_OK;
    FVDP_CH_t                  CH;
    VQ_SHARPNESS_Parameters_t* pSharpnessVQData;
    int32_t                    SharpnessLevel;
    FVDP_HW_PSIConfig_t*       pHW_PSIConfig;

    CH = HandleContext_p->CH;

    if (CH != FVDP_MAIN)
        return result;

    pHW_PSIConfig = SYSTEM_GetHWPSIConfig(CH);

    pHW_PSIConfig->HWPSIFeature.SharpnessVQData = SharpnessDefaultVQTable;
    pHW_PSIConfig->HWPSIControlLevel.SharpnessLevel = DEFAULT_SHARPNESS;
    pHW_PSIConfig->HWPSIState[FVDP_SHARPNESS] = FVDP_PSI_OFF;
    HandleContext_p->PSIState[FVDP_SHARPNESS] = FVDP_PSI_OFF;


    SHARPNESS_HQ_Init(CH);

    //get the pointer to SHARPNESS structure
    pSharpnessVQData = &(pHW_PSIConfig->HWPSIFeature.SharpnessVQData);
    SharpnessLevel = pHW_PSIConfig->HWPSIControlLevel.SharpnessLevel;
    result = system_PSI_Sharpness_Update (HandleContext_p->CH, pSharpnessVQData, SharpnessLevel);

    return result;
}

/*******************************************************************************
Name        : SYSTEM_PSI_Process
Description : handling PSI

Parameters  : FVDP_HandleContext_t* HandleContext_p
Assumptions : This function needs to be called during open channel
Limitations :
Returns     : FVDP_OK
*******************************************************************************/
FVDP_Result_t SYSTEM_PSI_Sharpness(FVDP_HandleContext_t* HandleContext_p)
{
    FVDP_Result_t ErrorCode = FVDP_OK;
    FVDP_CH_t               CH;
    VQ_SHARPNESS_Parameters_t* pSharpnessVQData;
    int32_t                    SharpnessLevel;
    FVDP_HW_PSIConfig_t*       pHW_PSIConfig;
    bool Updateflag = FALSE;

    CH = HandleContext_p->CH;

    pHW_PSIConfig = SYSTEM_GetHWPSIConfig(CH);

#if(SHARPNESS_TEST == TRUE)
        if( REG32_Read(IVP_PATGEN_INC_R + MIVP_BASE_ADDRESS) != 0)
        {
             HandleContext_p->PSIState[FVDP_SHARPNESS] = FVDP_PSI_ON;
             HandleContext_p->PSIControlLevel.SharpnessLevel = REG32_Read(IVP_PATGEN_INC_G + MIVP_BASE_ADDRESS);
        }
        else
        {
             HandleContext_p->PSIState[FVDP_SHARPNESS] = FVDP_PSI_OFF;
        }

        HandleContext_p->PSIFeature.SharpnessVQData = SharpnessDefaultVQTable;
        HandleContext_p->UpdateFlags.PSIVQUpdateFlag |= UPDATE_PSI_VQ_SHARPNESS;

#endif

    //Sharpness state control
    if(pHW_PSIConfig->HWPSIState[FVDP_SHARPNESS]!= HandleContext_p->PSIState[FVDP_SHARPNESS])
    {
        //Update HW Sharpness PIS status
        pHW_PSIConfig->HWPSIState[FVDP_SHARPNESS]= HandleContext_p->PSIState[FVDP_SHARPNESS];

        SHARPNESS_HQ_SetState(CH, pHW_PSIConfig->HWPSIState[FVDP_SHARPNESS]);
    }

    if(pHW_PSIConfig->HWPSIControlLevel.SharpnessLevel != HandleContext_p->PSIControlLevel.SharpnessLevel)
    {
        //Update HW Sharpness PIS Level
        pHW_PSIConfig->HWPSIControlLevel.SharpnessLevel = HandleContext_p->PSIControlLevel.SharpnessLevel;
        Updateflag = TRUE;
    }

    if(HandleContext_p->UpdateFlags.PSIVQUpdateFlag & UPDATE_PSI_VQ_SHARPNESS)
    {
        //Update HW Sharpness PIS VQ Date
        pHW_PSIConfig->HWPSIFeature.SharpnessVQData = HandleContext_p->PSIFeature.SharpnessVQData;

        Updateflag = TRUE;
    }

    if(Updateflag == TRUE)
    {
        //get the pointer to SHARPNESS structure
        pSharpnessVQData = &(pHW_PSIConfig->HWPSIFeature.SharpnessVQData);
        SharpnessLevel = pHW_PSIConfig->HWPSIControlLevel.SharpnessLevel;
        ErrorCode = system_PSI_Sharpness_Update (HandleContext_p->CH, pSharpnessVQData, SharpnessLevel);
        if((ErrorCode == FVDP_OK) && (HandleContext_p->UpdateFlags.PSIVQUpdateFlag &UPDATE_PSI_VQ_SHARPNESS))
        {
            // Clear update flag
            HandleContext_p->UpdateFlags.PSIVQUpdateFlag &= ~UPDATE_PSI_VQ_SHARPNESS;
        }
    }

    return ErrorCode;
}

/*******************************************************************************
Name        : SHARP_Update
Description : Update SHARPNESS software module information changes
Parameters  : CH: channel,  options: input/output
              pMin Pointer to Min Value for the requested Control.
              pMax Pointer to Max Value for the requested Control
              pStep Pointer to Step Value for the requested Control
Assumptions :
Limitations :
Returns     : FVDP_ERROR
*******************************************************************************/
FVDP_Result_t SYSTEM_PSI_Sharpness_ControlRange (FVDP_CH_t CH,
                                  int32_t *pMin,
                                  int32_t *pMax,
                                  int32_t *pStep)
{
    FVDP_HW_PSIConfig_t*       pHW_PSIConfig;

    if (CH != FVDP_MAIN)
        return FVDP_FEATURE_NOT_SUPPORTED;

    pHW_PSIConfig = SYSTEM_GetHWPSIConfig(CH);

    if( pHW_PSIConfig->HWPSIFeature.SharpnessVQData.NumOfSteps <= 0)
        return FVDP_ERROR;

    *pMin = 0;
    *pMax = pHW_PSIConfig->HWPSIFeature.SharpnessVQData.NumOfSteps - 1;
    *pStep = 1;

    return FVDP_OK;
}

/*******************************************************************************
Name        : system_PSI_Sharpness_Update
Description : Given sharpness data, locally defined range, and current step number, perform
            interpolation and program the various sharpness and enhancer registers
            reference: Sharpness_MediaTool_V2.xls
Parameters  : CH: channel, sharpness data pointer
Assumptions :
Limitations :
Returns     : FVDP_Result_t type value :
                - FVDP_OK, if no problem
                - FVDP_FEATURE_NOT_SUPPORTED, if channel not supported.
                - FVDP_ERROR, if bad parameters.
*******************************************************************************/
static FVDP_Result_t system_PSI_Sharpness_Update (FVDP_CH_t CH,
                                           const VQ_SHARPNESS_Parameters_t * pSharpnessData,
                                           int32_t SharpnessLevel)
{
    FVDP_Result_t                   result = FVDP_OK;
    VQ_SharpnessDataRange_t *       pSharpnessDataRange;
    VQ_EnhancerGain_t               Gain;
    uint32_t                        NumOfSteps;
    uint32_t                        CurrentStep;
    VQ_Sharpness_Step_Setting_t *   pStepData;
    VQ_EnhancerParameters_t *       pEnhancerDataMin;
    VQ_EnhancerParameters_t *       pEnhancerDataMax;
    VQ_SharpnessStepGroup_t *       pStepGroup;
    VQ_EnhancerNonLinearControl_t   NonLinearParams;
    VQ_EnhancerNoiseCoringControl_t NoiseCoringParams;
    VQ_EnhancerParameters_t         EnhancerParams;

    NumOfSteps = (pSharpnessData->NumOfSteps<=2)?1:(pSharpnessData->NumOfSteps-1);

    CurrentStep = SharpnessLevel;

    if (CurrentStep > NumOfSteps)
    {
        CurrentStep = NumOfSteps;
    }

    // select a range
    pSharpnessDataRange = (VQ_SharpnessDataRange_t *)&pSharpnessData->DataRange;

    // get correct set of step data
    pStepData = (VQ_Sharpness_Step_Setting_t*)&pSharpnessData->StepData[CurrentStep];

    /////////////////////////////////
    //
    // S H A R P N E S S   G A I N
    //
    /////////////////////////////////

    // Horizontal sharpness gain setting
    Gain.LumaGain = system_PSI_Sharpness_GetValueFromSteps(pSharpnessDataRange->MainLumaHGainMin,
                                        pSharpnessDataRange->MainLumaHGainMax,
                                        NumOfSteps, CurrentStep);
    Gain.ChromaGain = system_PSI_Sharpness_GetValueFromSteps(pSharpnessDataRange->MainChromaHGainMin,
                                        pSharpnessDataRange->MainChromaHGainMax,
                                        NumOfSteps, CurrentStep);
    result = SHARPNESS_HQ_SharpnessGain(CH, SCALER_HORIZONTAL, &Gain);

    if ((result != FVDP_OK) && (result != FVDP_FEATURE_NOT_SUPPORTED))
    {
        return result;
    }

    // Vertical sharpness gain setting
    Gain.LumaGain = system_PSI_Sharpness_GetValueFromSteps(pSharpnessDataRange->MainLumaVGainMin,
                                        pSharpnessDataRange->MainLumaVGainMax,
                                        NumOfSteps, CurrentStep);
    Gain.ChromaGain = system_PSI_Sharpness_GetValueFromSteps(pSharpnessDataRange->MainChromaVGainMin,
                                        pSharpnessDataRange->MainChromaVGainMax,
                                        NumOfSteps, CurrentStep);
    result = SHARPNESS_HQ_SharpnessGain(CH, SCALER_VERTICAL, &Gain);

    if ((result != FVDP_OK) && (result != FVDP_FEATURE_NOT_SUPPORTED))
    {
        return result;
    }

    //////////////////////////
    //
    // N O N    L I N E A R
    //
    //////////////////////////

    // Horizontal sharpness nonlinear setting
    NonLinearParams.Threshold1 = system_PSI_Sharpness_GetValueFromSteps(pSharpnessDataRange->MainNLHMin.Threshold1,
                                                    pSharpnessDataRange->MainNLHMax.Threshold1,
                                                    100, pStepData->NonLinearThreshold1);
    NonLinearParams.Gain1 = system_PSI_Sharpness_GetValueFromSteps(pSharpnessDataRange->MainNLHMin.Gain1,
                                                    pSharpnessDataRange->MainNLHMax.Gain1,
                                                    100, pStepData->NonLinearGain1);
    NonLinearParams.Threshold2 = system_PSI_Sharpness_GetValueFromSteps(pSharpnessDataRange->MainNLHMin.Threshold2,
                                                    pSharpnessDataRange->MainNLHMax.Threshold2,
                                                    100, pStepData->NonLinearThreshold2);
    NonLinearParams.Gain2 = system_PSI_Sharpness_GetValueFromSteps(pSharpnessDataRange->MainNLHMin.Gain2,
                                                    pSharpnessDataRange->MainNLHMax.Gain2,
                                                    100, pStepData->NonLinearGain2);
    result = SHARPNESS_HQ_EnhancerNonLinearControl(CH, SCALER_HORIZONTAL, &NonLinearParams);

    if ((result != FVDP_OK) && (result != FVDP_FEATURE_NOT_SUPPORTED))
    {
        return result;
    }

    // Vertical sharpness nonlinear setting
    NonLinearParams.Threshold1 = system_PSI_Sharpness_GetValueFromSteps(pSharpnessDataRange->MainNLVMin.Threshold1,
                                                    pSharpnessDataRange->MainNLVMax.Threshold1,
                                                    100, pStepData->NonLinearThreshold1);
    NonLinearParams.Gain1 = system_PSI_Sharpness_GetValueFromSteps(pSharpnessDataRange->MainNLVMin.Gain1,
                                                    pSharpnessDataRange->MainNLVMax.Gain1,
                                                    100, pStepData->NonLinearGain1);
    NonLinearParams.Threshold2 = system_PSI_Sharpness_GetValueFromSteps(pSharpnessDataRange->MainNLVMin.Threshold2,
                                                    pSharpnessDataRange->MainNLVMax.Threshold2,
                                                    100, pStepData->NonLinearThreshold2);
    NonLinearParams.Gain2 = system_PSI_Sharpness_GetValueFromSteps(pSharpnessDataRange->MainNLVMin.Gain2,
                                                    pSharpnessDataRange->MainNLVMax.Gain2,
                                                    100, pStepData->NonLinearGain2);

    result = SHARPNESS_HQ_EnhancerNonLinearControl(CH, SCALER_VERTICAL, &NonLinearParams);

    if ((result != FVDP_OK) && (result != FVDP_FEATURE_NOT_SUPPORTED))
    {
        return result;
    }

    //////////////////////////////
    //
    // N O I S E    C O R I N G
    //
    //////////////////////////////

    // Horizontal sharpness noise coring setting
    NoiseCoringParams.Threshold1 = system_PSI_Sharpness_GetValueFromSteps(pSharpnessDataRange->MainNCHMin.Threshold1,
                                                    pSharpnessDataRange->MainNCHMax.Threshold1,
                                                    100, pStepData->NoiseCoring);
    result = SHARPNESS_HQ_EnhancerNoiseCoringControl(CH, SCALER_HORIZONTAL, &NoiseCoringParams);

    if ((result != FVDP_OK) && (result != FVDP_FEATURE_NOT_SUPPORTED))
    {
        return result;
    }

    // Vertical sharpness noise coring setting
    NoiseCoringParams.Threshold1 = system_PSI_Sharpness_GetValueFromSteps(pSharpnessDataRange->MainNCVMin.Threshold1,
                                                    pSharpnessDataRange->MainNCVMax.Threshold1,
                                                    100, pStepData->NoiseCoring);
    result = SHARPNESS_HQ_EnhancerNoiseCoringControl(CH, SCALER_VERTICAL, &NoiseCoringParams);

    if ((result != FVDP_OK) && (result != FVDP_FEATURE_NOT_SUPPORTED))
    {
        return result;
    }

    /////////////////////
    //
    // E N H A N C E R
    //
    /////////////////////

    // Horizontal Luma sharpness enhancer setting
    pEnhancerDataMin = &pSharpnessDataRange->MainLumaHMin;
    pEnhancerDataMax = &pSharpnessDataRange->MainLumaHMax;
    pStepGroup = &pStepData->Luma;
    system_PSI_Sharpness_SetEnhancerData(pEnhancerDataMin, pEnhancerDataMax, pStepGroup, &EnhancerParams);
    result = SHARPNESS_HQ_EnhancerSetData(CH, SCALER_HORIZONTAL, SCALER_LUMA, &EnhancerParams);
    if ((result != FVDP_OK) && (result != FVDP_FEATURE_NOT_SUPPORTED))
    {
        return result;
    }

    // Horizontal Chroma sharpness enhancer setting
    pEnhancerDataMin = &pSharpnessDataRange->MainChromaHMin;
    pEnhancerDataMax = &pSharpnessDataRange->MainChromaHMax;
    pStepGroup = &pStepData->Chroma;
    system_PSI_Sharpness_SetEnhancerData(pEnhancerDataMin, pEnhancerDataMax, pStepGroup, &EnhancerParams);
    result = SHARPNESS_HQ_EnhancerSetData(CH, SCALER_HORIZONTAL, SCALER_CHROMA, &EnhancerParams);
    if ((result != FVDP_OK) && (result != FVDP_FEATURE_NOT_SUPPORTED))
    {
        return result;
    }

    // Vertical Luma sharpness enhancer setting
    pEnhancerDataMin = &pSharpnessDataRange->MainLumaVMin;
    pEnhancerDataMax = &pSharpnessDataRange->MainLumaVMax;
    pStepGroup = &pStepData->Luma;
    system_PSI_Sharpness_SetEnhancerData(pEnhancerDataMin, pEnhancerDataMax, pStepGroup, &EnhancerParams);
    result = SHARPNESS_HQ_EnhancerSetData(CH, SCALER_VERTICAL, SCALER_LUMA, &EnhancerParams);
    if ((result != FVDP_OK) && (result != FVDP_FEATURE_NOT_SUPPORTED))
    {
        return result;
    }

    // Vertical Chroma sharpness enhancer setting
    pEnhancerDataMin = &pSharpnessDataRange->MainChromaVMin;
    pEnhancerDataMax = &pSharpnessDataRange->MainChromaVMax;
    pStepGroup = &pStepData->Chroma;
    system_PSI_Sharpness_SetEnhancerData(pEnhancerDataMin, pEnhancerDataMax, pStepGroup, &EnhancerParams);
    result = SHARPNESS_HQ_EnhancerSetData(CH, SCALER_VERTICAL, SCALER_CHROMA, &EnhancerParams);
    if ((result != FVDP_OK) && (result != FVDP_FEATURE_NOT_SUPPORTED))
    {
        return result;
    }

    return (result);
}

/*******************************************************************************
Name        : system_PSI_Sharpness_SetEnhancerData
Description : Given min, max and current step in percentage, perform interpolations and
                store results in a structure
Parameters  : pointer to Min structure, pointer to Max structure, pointer to Step structure,
                pointer to a structure for storing results
Assumptions : values in StepGroup_p are expected to be in percentage
Limitations :
Returns     : None
*******************************************************************************/
static void system_PSI_Sharpness_SetEnhancerData(VQ_EnhancerParameters_t * pMin, VQ_EnhancerParameters_t * pMax,
                            VQ_SharpnessStepGroup_t * pStepGroup, VQ_EnhancerParameters_t * pOutputParams)
{
    if ((pMin==NULL) || (pMax==NULL) || (pStepGroup==NULL) || (pOutputParams==NULL))
        return;

    pOutputParams->LOSHOOT_TOL_Value = system_PSI_Sharpness_GetValueFromSteps(pMin->LOSHOOT_TOL_Value,
                                        pMax->LOSHOOT_TOL_Value,
                                        100, pStepGroup->OverShoot);
    pOutputParams->LUSHOOT_TOL_Value = system_PSI_Sharpness_GetValueFromSteps(pMin->LUSHOOT_TOL_Value,
                                        pMax->LUSHOOT_TOL_Value,
                                        100, pStepGroup->OverShoot);
    pOutputParams->SOSHOOT_TOL_Value = system_PSI_Sharpness_GetValueFromSteps(pMin->SOSHOOT_TOL_Value,
                                        pMax->SOSHOOT_TOL_Value,
                                        100, pStepGroup->OverShoot);
    pOutputParams->SUSHOOT_TOL_Value = system_PSI_Sharpness_GetValueFromSteps(pMin->SUSHOOT_TOL_Value,
                                        pMax->SUSHOOT_TOL_Value,
                                        100, pStepGroup->OverShoot);
    pOutputParams->LGAIN_Value = system_PSI_Sharpness_GetValueFromSteps(pMin->LGAIN_Value,
                                        pMax->LGAIN_Value,
                                        100, pStepGroup->OverShoot);
    pOutputParams->SGAIN_Value = system_PSI_Sharpness_GetValueFromSteps(pMin->SGAIN_Value,
                                        pMax->SGAIN_Value,
                                        100, pStepGroup->OverShoot);
    pOutputParams->FINALLGAIN_Value = system_PSI_Sharpness_GetValueFromSteps(pMin->FINALLGAIN_Value,
                                        pMax->FINALLGAIN_Value,
                                        100, pStepGroup->LargeGain);
    pOutputParams->FINALSGAIN_Value = system_PSI_Sharpness_GetValueFromSteps(pMin->FINALSGAIN_Value,
                                        pMax->FINALSGAIN_Value,
                                        100, pStepGroup->SmallGain);
    pOutputParams->FINALGAIN_Value = system_PSI_Sharpness_GetValueFromSteps(pMin->FINALGAIN_Value,
                                        pMax->FINALGAIN_Value,
                                        100, pStepGroup->FinalGain);
    pOutputParams->DELTA_Value = (pMin->DELTA_Value + pMax->DELTA_Value)/2;
    pOutputParams->SLOPE_Value = (pMin->SLOPE_Value + pMax->SLOPE_Value)/2;
    pOutputParams->THRESH_Value = (pMin->THRESH_Value + pMax->THRESH_Value)/2;
    pOutputParams->HIGH_SLOPE_AGC_Value = system_PSI_Sharpness_GetValueFromSteps(pMax->HIGH_SLOPE_AGC_Value, //inverted
                                        pMin->HIGH_SLOPE_AGC_Value,
                                        100, pStepGroup->AGC);
    pOutputParams->LOW_SLOPE_AGC_Value = system_PSI_Sharpness_GetValueFromSteps(pMax->LOW_SLOPE_AGC_Value,  //inverted
                                        pMin->LOW_SLOPE_AGC_Value,
                                        100, pStepGroup->AGC);
    pOutputParams->HIGH_THRESH_AGC_Value = system_PSI_Sharpness_GetValueFromSteps(pMin->HIGH_THRESH_AGC_Value,
                                        pMax->HIGH_THRESH_AGC_Value,
                                        100, pStepGroup->AGC);
    pOutputParams->LOW_THRESH_AGC_Value = system_PSI_Sharpness_GetValueFromSteps(pMin->LOW_THRESH_AGC_Value,
                                        pMax->LOW_THRESH_AGC_Value,
                                        100, pStepGroup->AGC);
}

/*******************************************************************************
Name        : system_PSI_Sharpness_GetValueFromSteps
Description : Given max, min, current step and total number of steps, return interpolated
              value with round-up
Parameters  : Min - lower bound
              Max - upper bound
              NumOfSteps - total number of steps
              StepNum - current step
Assumptions :
Limitations :
Returns     : uint32_t
*******************************************************************************/
static uint32_t system_PSI_Sharpness_GetValueFromSteps(uint32_t Min, uint32_t Max, uint32_t NumOfSteps, uint32_t StepNum)
{
    uint32_t Result;

    if (NumOfSteps == 0)
        NumOfSteps = 1;

    if (Max >= Min)
        Result = (Min + ((Max - Min) * StepNum + NumOfSteps / 2) / NumOfSteps);
    else
        Result = ((int32_t)Min + (((int32_t)Max - (int32_t)Min) * (int32_t)StepNum - (int32_t)NumOfSteps / 2) / (int32_t)NumOfSteps);

    return Result;
}
