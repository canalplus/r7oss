/***********************************************************************
 *
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
#include "fvdp_sharpness.h"
/* Private Types ------------------------------------------------------------ */

/* Private Constants -------------------------------------------------------- */

/* Private Macros ----------------------------------------------------------- */
static const uint32_t HSHARPENSS_BASE_ADDR[] = {HQS_HF_BE_BASE_ADDRESS, HFLITE_PIP_BASE_ADDRESS, HFLITE_AUX_BASE_ADDRESS, HFLITE_ENC_BASE_ADDRESS};
static const uint32_t VSHARPENSS_BASE_ADDR[] = {HQS_VF_BE_BASE_ADDRESS, VFLITE_PIP_BASE_ADDRESS, VFLITE_AUX_BASE_ADDRESS, VFLITE_ENC_BASE_ADDRESS};

#define HSHARPENSS_REG32_Write(Ch,Addr,Val)                  REG32_Write(Addr+HSHARPENSS_BASE_ADDR[Ch],Val)
#define HSHARPENSS_REG32_Read(Ch,Addr)                       REG32_Read(Addr+HSHARPENSS_BASE_ADDR[Ch])
#define HSHARPENSS_REG32_Set(Ch,Addr,BitsToSet)              REG32_Set(Addr+HSHARPENSS_BASE_ADDR[Ch],BitsToSet)
#define HSHARPENSS_REG32_Clear(Ch,Addr,BitsToClear)          REG32_Clear(Addr+HSHARPENSS_BASE_ADDR[Ch],BitsToClear)
#define HSHARPENSS_REG32_ClearAndSet(Ch,Addr,BitsToClear,Offset,ValueToSet) \
                                                        REG32_ClearAndSet(Addr+HSHARPENSS_BASE_ADDR[Ch],BitsToClear,Offset,ValueToSet)
#define HSHARPENSS_UNIT_REG32_SetField(Ch,Addr,Field,ValueToSet)     REG32_SetField(Addr+HSHARPENSS_BASE_ADDR[Ch],Field,ValueToSet)

#define VSHARPENSS_REG32_Write(Ch,Addr,Val)                  REG32_Write(Addr+VSHARPENSS_BASE_ADDR[Ch],Val)
#define VSHARPENSS_REG32_Read(Ch,Addr)                       REG32_Read(Addr+VSHARPENSS_BASE_ADDR[Ch])
#define VSHARPENSS_REG32_Set(Ch,Addr,BitsToSet)              REG32_Set(Addr+VSHARPENSS_BASE_ADDR[Ch],BitsToSet)
#define VSHARPENSS_REG32_Clear(Ch,Addr,BitsToClear)          REG32_Clear(Addr+VSHARPENSS_BASE_ADDR[Ch],BitsToClear)
#define VSHARPENSS_REG32_ClearAndSet(Ch,Addr,BitsToClear,Offset,ValueToSet) \
                                                        REG32_ClearAndSet(Addr+VSHARPENSS_BASE_ADDR[Ch],BitsToClear,Offset,ValueToSet)
#define VFHQS_UNIT_REG32_SetField(Ch,Addr,Field,ValueToSet)     REG32_SetField(Addr+VSHARPENSS_BASE_ADDR[Ch],Field,ValueToSet)


/* Private Variables (static)------------------------------------------------ */

/* Private Function Prototypes ---------------------------------------------- */

/* Global Variables --------------------------------------------------------- */


/* Functions ---------------------------------------------------------------- */
/*******************************************************************************
Name        : SHARPNESS_HQ_Init
Description : Initialize SHARPNESS software module and call SHARPNESS hardware init
                 function
Parameters  :FVDP_HandleContext_t* HandleContext_p
Assumptions :
Limitations :
Returns     : FVDP_OK
*******************************************************************************/
FVDP_Result_t SHARPNESS_HQ_Init ( FVDP_CH_t  CH)
{
    FVDP_Result_t result = FVDP_OK;
     //Enhancer Init
     if (CH == FVDP_MAIN)
     {
         HSHARPENSS_REG32_Clear(CH, HFHQS_CTRL, HFHQS_ENH_EN_MASK);
         HSHARPENSS_REG32_Set(CH, HFHQS_CTRL, HFHQS_ENH_BYPASS_MASK);
         VSHARPENSS_REG32_Clear(CH, VFHQS_CTRL, VFHQS_ENH_EN_MASK);
         VSHARPENSS_REG32_Set(CH, VFHQS_CTRL, VFHQS_ENH_BYPASS_MASK);

         HostUpdate_SetUpdate_BE(BE_HOST_UPDATE_HQSCALER, SYNC_UPDATE);
    }

    return result;
}

/*******************************************************************************
Name        : SHARPNESS_HQ_SetState
Description : Initialize SHARPNESS software module and call SHARPNESS hardware init
                 function
Parameters  : CH: channel,  options: input/output
Assumptions :
Limitations :
Returns     : FVDP_ERROR
*******************************************************************************/
FVDP_Result_t SHARPNESS_HQ_SetState(FVDP_CH_t CH, FVDP_PSIState_t selection)
{
    FVDP_Result_t result = FVDP_OK;

    if (CH == FVDP_MAIN)
    {
        if (selection == FVDP_PSI_ON)
        {

            HSHARPENSS_REG32_Set(CH, HFHQS_CTRL, HFHQS_ENH_EN_MASK);
            HSHARPENSS_REG32_Clear(CH, HFHQS_CTRL, HFHQS_ENH_BYPASS_MASK);
            VSHARPENSS_REG32_Set(CH, VFHQS_CTRL, VFHQS_ENH_EN_MASK);
            VSHARPENSS_REG32_Clear(CH, VFHQS_CTRL, VFHQS_ENH_BYPASS_MASK);
        }
        else
        {
            HSHARPENSS_REG32_Clear(CH, HFHQS_CTRL, HFHQS_ENH_EN_MASK);
            HSHARPENSS_REG32_Set(CH, HFHQS_CTRL, HFHQS_ENH_BYPASS_MASK);
            VSHARPENSS_REG32_Clear(CH, VFHQS_CTRL, VFHQS_ENH_EN_MASK);
            VSHARPENSS_REG32_Set(CH, VFHQS_CTRL, VFHQS_ENH_BYPASS_MASK);
        }

        HostUpdate_SetUpdate_BE(BE_HOST_UPDATE_HQSCALER, SYNC_UPDATE);

    }
    else
    {
        result = FVDP_FEATURE_NOT_SUPPORTED;
    }

    return result;
}

/*******************************************************************************
Name        : SHARPNESS_HQ_SharpnessGain
Description : Configure noise coring control
Parameters  : CH: channel
                    Orientation - horizontal, vertical
                    pGain - pointer to a structure which stores desired programming values
Assumptions :
Limitations :
Returns     : FVDP_Result_t type value :
                - FVDP_OK if no error
                - FVDP_ERROR gain pointer is NULL or Feature is not supported
                - FVDP_FEATURE_NOT_SUPPORTED, if channel not supported.
*******************************************************************************/
FVDP_Result_t SHARPNESS_HQ_SharpnessGain(FVDP_CH_t CH,
                                SHARPNESS_ScalerOrientation_t Orientation,
                                VQ_EnhancerGain_t * pGain)
{
    if (pGain == NULL)
    return FVDP_ERROR;

    if (CH == FVDP_MAIN)
    {
        if (Orientation == SCALER_HORIZONTAL)
        {
            HSHARPENSS_REG32_ClearAndSet(CH, HFHQS_SHARPNESS, HFHQS_SHARPNESS_LUMA_MASK, HFHQS_SHARPNESS_LUMA_OFFSET, pGain->LumaGain);
            HSHARPENSS_REG32_ClearAndSet(CH, HFHQS_SHARPNESS, HFHQS_SHARPNESS_CHROMA_MASK, HFHQS_SHARPNESS_CHROMA_OFFSET, pGain->ChromaGain);
        }
        else
        {
            VSHARPENSS_REG32_ClearAndSet(CH, VFHQS_SHARPNESS, VFHQS_SHARPNESS_LUMA_MASK, VFHQS_SHARPNESS_LUMA_OFFSET, pGain->LumaGain);
            VSHARPENSS_REG32_ClearAndSet(CH, VFHQS_SHARPNESS, VFHQS_SHARPNESS_CHROMA_MASK, VFHQS_SHARPNESS_CHROMA_OFFSET, pGain->ChromaGain);
        }
        HostUpdate_SetUpdate_BE(BE_HOST_UPDATE_HQSCALER, SYNC_UPDATE);
        return FVDP_OK;
    }
    else
        return FVDP_FEATURE_NOT_SUPPORTED;// not feature support
 }

/*******************************************************************************
Name        : SHARPNESS_HQ_EnhancerNonLinearControl
Description : Configure non-linear control
Parameters  : CH: channel
                    Orientation - horizontal, vertical
                    Control_p - pointer to a structure which stores desired programming values
Assumptions :
Limitations :
Returns     : FVDP_Result_t type value :
                - FVDP_OK if no error
                - FVDP_ERROR gain pointer is NULL or Feature is not supported
                - FVDP_FEATURE_NOT_SUPPORTED, if channel not supported.
*******************************************************************************/
 FVDP_Result_t SHARPNESS_HQ_EnhancerNonLinearControl(FVDP_CH_t CH,
                                SHARPNESS_ScalerOrientation_t Orientation,
                                VQ_EnhancerNonLinearControl_t * pControl)
{
    if (pControl == NULL)
        return FVDP_ERROR;

    if (CH == FVDP_MAIN)
    {
        if (Orientation == SCALER_HORIZONTAL)
        {

            HSHARPENSS_REG32_ClearAndSet(CH, HFHQS_NONLINEAR_CTRL, HFHQS_NONLINEAR_THRESH1_MASK, HFHQS_NONLINEAR_THRESH1_OFFSET, pControl->Threshold1);
            HSHARPENSS_REG32_ClearAndSet(CH, HFHQS_NONLINEAR_CTRL, HFHQS_NONLINEAR_GAIN1_MASK, HFHQS_NONLINEAR_GAIN1_OFFSET, pControl->Gain1);
            HSHARPENSS_REG32_ClearAndSet(CH, HFHQS_NONLINEAR_CTRL, HFHQS_NONLINEAR_THRESH2_MASK, HFHQS_NONLINEAR_THRESH2_OFFSET, pControl->Threshold2);
            HSHARPENSS_REG32_ClearAndSet(CH, HFHQS_NONLINEAR_CTRL, HFHQS_NONLINEAR_GAIN2_MASK, HFHQS_NONLINEAR_GAIN2_OFFSET, pControl->Gain2);
        }
        else
        {

            VSHARPENSS_REG32_ClearAndSet(CH, VFHQS_NONLINEAR_CTRL, VFHQS_NONLINEAR_THRESH1_MASK, VFHQS_NONLINEAR_THRESH1_OFFSET, pControl->Threshold1);
            VSHARPENSS_REG32_ClearAndSet(CH, VFHQS_NONLINEAR_CTRL, VFHQS_NONLINEAR_GAIN1_MASK, VFHQS_NONLINEAR_GAIN1_OFFSET, pControl->Gain1);
            VSHARPENSS_REG32_ClearAndSet(CH, VFHQS_NONLINEAR_CTRL, VFHQS_NONLINEAR_THRESH2_MASK, VFHQS_NONLINEAR_THRESH2_OFFSET, pControl->Threshold2);
            VSHARPENSS_REG32_ClearAndSet(CH, VFHQS_NONLINEAR_CTRL, VFHQS_NONLINEAR_GAIN2_MASK, VFHQS_NONLINEAR_GAIN2_OFFSET, pControl->Gain2);
        }
        HostUpdate_SetUpdate_BE(BE_HOST_UPDATE_HQSCALER, SYNC_UPDATE);
        return FVDP_OK;
    }
    else
        return FVDP_FEATURE_NOT_SUPPORTED;
}

/*******************************************************************************
Name        : SHARPNESS_HQ_EnhancerNoiseCoringControl
Description : Configure noise coring control
Parameters  : Physical channel - CH_A, CH_B, CH_C
                    Orientation - horizontal, vertical
                    Control_p - pointer to a structure which stores desired programming values
Assumptions :
Limitations :
Returns     : FVDP_Result_t type value :
                - FVDP_OK if no error
                - FVDP_ERROR gain pointer is NULL or Feature is not supported
                - FVDP_FEATURE_NOT_SUPPORTED, if channel not supported.
*******************************************************************************/
FVDP_Result_t SHARPNESS_HQ_EnhancerNoiseCoringControl(FVDP_CH_t CH,
                            SHARPNESS_ScalerOrientation_t Orientation,
                            VQ_EnhancerNoiseCoringControl_t * pControl)
{
    if (pControl == NULL)
        return FVDP_ERROR;

    if (CH == FVDP_MAIN)
    {
        if (Orientation == SCALER_HORIZONTAL)
        {
            HSHARPENSS_REG32_Write(CH, HFHQS_NOISE_CORING_CTRL, pControl->Threshold1<<HFHQS_NOISE_CORING_THRESH_OFFSET);
        }
        else
        {
            VSHARPENSS_REG32_Write(CH, VFHQS_NOISE_CORING_CTRL, pControl->Threshold1<<VFHQS_NOISE_CORING_THRESH_OFFSET);
        }

        HostUpdate_SetUpdate_BE(BE_HOST_UPDATE_HQSCALER, SYNC_UPDATE);

        return FVDP_OK;
    }
    else
    {
        return FVDP_FEATURE_NOT_SUPPORTED;
    }
}

/*******************************************************************************
Name        : SHARPNESS_HQ_EnhancerSetData
Description : Program sharpness/enhancer hardware
Parameters  : CH: channel
                    Orientation - horizontal, vertical
                    Selection - luma or chroma
                    pParams - pointer to a structure which desired programming values
Assumptions :
Limitations :
Returns     : FVDP_Result_t type value :
                - FVDP_OK if no error
                - FVDP_ERROR gain pointer is NULL or Feature is not supported
                - FVDP_FEATURE_NOT_SUPPORTED, if channel not supported.
*******************************************************************************/
FVDP_Result_t SHARPNESS_HQ_EnhancerSetData(FVDP_CH_t CH,
        SHARPNESS_ScalerOrientation_t Orientation, SHARPNESS_ScalerChromaOrLuma Selection, VQ_EnhancerParameters_t * pParams)
{
    if (pParams == NULL)
        return FVDP_ERROR;

    if (CH == FVDP_MAIN)
    {
        if (Orientation == SCALER_HORIZONTAL)
        {
            if (Selection == SCALER_LUMA)
            {
                HSHARPENSS_REG32_ClearAndSet(CH, HFHQS_ENH_Y_LSHOOT_TOL, HFHQS_ENH_Y_LOSHOOT_TOL_MASK, HFHQS_ENH_Y_LOSHOOT_TOL_OFFSET, pParams->LOSHOOT_TOL_Value);
                HSHARPENSS_REG32_ClearAndSet(CH, HFHQS_ENH_Y_LSHOOT_TOL, HFHQS_ENH_Y_LUSHOOT_TOL_MASK, HFHQS_ENH_Y_LUSHOOT_TOL_OFFSET, pParams->LUSHOOT_TOL_Value);
                HSHARPENSS_REG32_ClearAndSet(CH, HFHQS_ENH_Y_SSHOOT_TOL, HFHQS_ENH_Y_SOSHOOT_TOL_MASK, HFHQS_ENH_Y_SOSHOOT_TOL_OFFSET, pParams->SOSHOOT_TOL_Value);
                HSHARPENSS_REG32_ClearAndSet(CH, HFHQS_ENH_Y_SSHOOT_TOL, HFHQS_ENH_Y_SUSHOOT_TOL_MASK, HFHQS_ENH_Y_SUSHOOT_TOL_OFFSET, pParams->SUSHOOT_TOL_Value);
                HSHARPENSS_REG32_ClearAndSet(CH, HFHQS_ENH_Y_GAIN, HFHQS_ENH_Y_LGAIN_MASK, HFHQS_ENH_Y_LGAIN_OFFSET, pParams->LGAIN_Value);
                HSHARPENSS_REG32_ClearAndSet(CH, HFHQS_ENH_Y_GAIN, HFHQS_ENH_Y_SGAIN_MASK, HFHQS_ENH_Y_SGAIN_OFFSET, pParams->SGAIN_Value);
                HSHARPENSS_REG32_ClearAndSet(CH, HFHQS_ENH_Y_EDGEGAIN, HFHQS_ENH_Y_FINALLGAIN_MASK, HFHQS_ENH_Y_FINALLGAIN_OFFSET, pParams->FINALLGAIN_Value);
                HSHARPENSS_REG32_ClearAndSet(CH, HFHQS_ENH_Y_EDGEGAIN, HFHQS_ENH_Y_FINALSGAIN_MASK, HFHQS_ENH_Y_FINALSGAIN_OFFSET, pParams->FINALSGAIN_Value);
                HSHARPENSS_REG32_Write(CH, HFHQS_ENH_Y_FINALGAIN, (pParams->FINALGAIN_Value<<HFHQS_ENH_Y_FINALGAIN_OFFSET));
                HSHARPENSS_REG32_Write(CH, HFHQS_ENH_Y_DELTA, (pParams->DELTA_Value<<HFHQS_ENH_Y_DELTA_OFFSET));
                HSHARPENSS_REG32_Write(CH, HFHQS_ENH_Y_SLOPE, (pParams->SLOPE_Value<<HFHQS_ENH_Y_SLOPE_OFFSET));
                HSHARPENSS_REG32_Write(CH, HFHQS_ENH_Y_THRESH, (pParams->THRESH_Value<<HFHQS_ENH_Y_THRESH_OFFSET));

                HSHARPENSS_REG32_ClearAndSet(CH, HFHQS_ENH_Y_SLOPE_AGC, HFHQS_ENH_Y_HIGH_SLOPE_AGC_MASK, HFHQS_ENH_Y_HIGH_SLOPE_AGC_OFFSET, pParams->HIGH_SLOPE_AGC_Value);
                HSHARPENSS_REG32_ClearAndSet(CH, HFHQS_ENH_Y_SLOPE_AGC, HFHQS_ENH_Y_LOW_SLOPE_AGC_MASK, HFHQS_ENH_Y_LOW_SLOPE_AGC_OFFSET, pParams->LOW_SLOPE_AGC_Value);
                HSHARPENSS_REG32_ClearAndSet(CH, HFHQS_ENH_Y_SLOPE_AGC, HFHQS_ENH_Y_HIGH_THRESH_AGC_MASK, HFHQS_ENH_Y_HIGH_THRESH_AGC_OFFSET, pParams->HIGH_THRESH_AGC_Value);
                HSHARPENSS_REG32_ClearAndSet(CH, HFHQS_ENH_Y_SLOPE_AGC, HFHQS_ENH_Y_LOW_THRESH_AGC_MASK, HFHQS_ENH_Y_LOW_THRESH_AGC_OFFSET, pParams->LOW_THRESH_AGC_Value);
            }
            else
            {
                HSHARPENSS_REG32_ClearAndSet(CH, HFHQS_ENH_UV_LSHOOT_TOL, HFHQS_ENH_UV_LOSHOOT_TOL_MASK, HFHQS_ENH_UV_LOSHOOT_TOL_OFFSET, pParams->LOSHOOT_TOL_Value);
                HSHARPENSS_REG32_ClearAndSet(CH, HFHQS_ENH_UV_LSHOOT_TOL, HFHQS_ENH_UV_LUSHOOT_TOL_MASK, HFHQS_ENH_UV_LUSHOOT_TOL_OFFSET, pParams->LUSHOOT_TOL_Value);
                HSHARPENSS_REG32_ClearAndSet(CH, HFHQS_ENH_UV_SSHOOT_TOL, HFHQS_ENH_UV_SOSHOOT_TOL_MASK, HFHQS_ENH_UV_SOSHOOT_TOL_OFFSET, pParams->SOSHOOT_TOL_Value);
                HSHARPENSS_REG32_ClearAndSet(CH, HFHQS_ENH_UV_SSHOOT_TOL, HFHQS_ENH_UV_SUSHOOT_TOL_MASK, HFHQS_ENH_UV_SUSHOOT_TOL_OFFSET, pParams->SUSHOOT_TOL_Value);
                HSHARPENSS_REG32_ClearAndSet(CH, HFHQS_ENH_UV_GAIN, HFHQS_ENH_UV_LGAIN_MASK, HFHQS_ENH_UV_LGAIN_OFFSET, pParams->LGAIN_Value);
                HSHARPENSS_REG32_ClearAndSet(CH, HFHQS_ENH_UV_GAIN, HFHQS_ENH_UV_SGAIN_MASK, HFHQS_ENH_UV_SGAIN_OFFSET, pParams->SGAIN_Value);
                HSHARPENSS_REG32_ClearAndSet(CH, HFHQS_ENH_UV_EDGEGAIN, HFHQS_ENH_UV_FINALLGAIN_MASK, HFHQS_ENH_UV_FINALLGAIN_OFFSET, pParams->FINALLGAIN_Value);
                HSHARPENSS_REG32_ClearAndSet(CH, HFHQS_ENH_UV_EDGEGAIN, HFHQS_ENH_UV_FINALSGAIN_MASK, HFHQS_ENH_UV_FINALSGAIN_OFFSET, pParams->FINALSGAIN_Value);
                HSHARPENSS_REG32_Write(CH, HFHQS_ENH_UV_FINALGAIN, (pParams->FINALGAIN_Value<<HFHQS_ENH_UV_FINALGAIN_OFFSET));
                HSHARPENSS_REG32_Write(CH, HFHQS_ENH_UV_DELTA, (pParams->DELTA_Value<<HFHQS_ENH_UV_DELTA_OFFSET));
                HSHARPENSS_REG32_Write(CH, HFHQS_ENH_UV_SLOPE, (pParams->SLOPE_Value<<HFHQS_ENH_UV_SLOPE_OFFSET));
                HSHARPENSS_REG32_Write(CH, HFHQS_ENH_UV_THRESH,	(pParams->THRESH_Value<<HFHQS_ENH_UV_THRESH_OFFSET));

                HSHARPENSS_REG32_ClearAndSet(CH, HFHQS_ENH_UV_SLOPE_AGC, HFHQS_ENH_UV_HIGH_SLOPE_AGC_MASK, HFHQS_ENH_UV_HIGH_SLOPE_AGC_OFFSET, pParams->HIGH_SLOPE_AGC_Value);
                HSHARPENSS_REG32_ClearAndSet(CH, HFHQS_ENH_UV_SLOPE_AGC, HFHQS_ENH_UV_LOW_SLOPE_AGC_MASK, HFHQS_ENH_UV_LOW_SLOPE_AGC_OFFSET, pParams->LOW_SLOPE_AGC_Value);
                HSHARPENSS_REG32_ClearAndSet(CH, HFHQS_ENH_UV_SLOPE_AGC, HFHQS_ENH_UV_HIGH_THRESH_AGC_MASK, HFHQS_ENH_UV_HIGH_THRESH_AGC_OFFSET, pParams->HIGH_THRESH_AGC_Value);
                HSHARPENSS_REG32_ClearAndSet(CH, HFHQS_ENH_UV_SLOPE_AGC, HFHQS_ENH_UV_LOW_THRESH_AGC_MASK, HFHQS_ENH_UV_LOW_THRESH_AGC_OFFSET, pParams->LOW_THRESH_AGC_Value);
            }
        }
        else
        {
            if (Selection == SCALER_LUMA)
            {
                VSHARPENSS_REG32_ClearAndSet(CH, VFHQS_ENH_Y_LSHOOT_TOL, VFHQS_ENH_Y_LOSHOOT_TOL_MASK, VFHQS_ENH_Y_LOSHOOT_TOL_OFFSET, pParams->LOSHOOT_TOL_Value);
                VSHARPENSS_REG32_ClearAndSet(CH, VFHQS_ENH_Y_LSHOOT_TOL, VFHQS_ENH_Y_LUSHOOT_TOL_MASK, VFHQS_ENH_Y_LUSHOOT_TOL_OFFSET, pParams->LUSHOOT_TOL_Value);
                VSHARPENSS_REG32_ClearAndSet(CH, VFHQS_ENH_Y_SSHOOT_TOL, VFHQS_ENH_Y_SOSHOOT_TOL_MASK, VFHQS_ENH_Y_SOSHOOT_TOL_OFFSET, pParams->SOSHOOT_TOL_Value);
                VSHARPENSS_REG32_ClearAndSet(CH, VFHQS_ENH_Y_SSHOOT_TOL, VFHQS_ENH_Y_SUSHOOT_TOL_MASK, VFHQS_ENH_Y_SUSHOOT_TOL_OFFSET, pParams->SUSHOOT_TOL_Value);
                VSHARPENSS_REG32_ClearAndSet(CH, VFHQS_ENH_Y_GAIN, VFHQS_ENH_Y_LGAIN_MASK, VFHQS_ENH_Y_LGAIN_OFFSET, pParams->LGAIN_Value);
                VSHARPENSS_REG32_ClearAndSet(CH, VFHQS_ENH_Y_GAIN, VFHQS_ENH_Y_SGAIN_MASK, VFHQS_ENH_Y_SGAIN_OFFSET, pParams->SGAIN_Value);
                VSHARPENSS_REG32_ClearAndSet(CH, VFHQS_ENH_Y_EDGEGAIN, VFHQS_ENH_Y_FINALLGAIN_MASK, VFHQS_ENH_Y_FINALLGAIN_OFFSET, pParams->FINALLGAIN_Value);
                VSHARPENSS_REG32_ClearAndSet(CH, VFHQS_ENH_Y_EDGEGAIN, VFHQS_ENH_Y_FINALSGAIN_MASK, VFHQS_ENH_Y_FINALSGAIN_OFFSET, pParams->FINALSGAIN_Value);
                VSHARPENSS_REG32_Write(CH, VFHQS_ENH_Y_FINALGAIN, (pParams->FINALGAIN_Value<<VFHQS_ENH_Y_FINALGAIN_OFFSET));
                VSHARPENSS_REG32_Write(CH, VFHQS_ENH_Y_DELTA, (pParams->DELTA_Value<<VFHQS_ENH_Y_DELTA_OFFSET));
                VSHARPENSS_REG32_Write(CH, VFHQS_ENH_Y_SLOPE, (pParams->SLOPE_Value<<VFHQS_ENH_Y_SLOPE_OFFSET));
                VSHARPENSS_REG32_Write(CH, VFHQS_ENH_Y_THRESH, (pParams->THRESH_Value<<VFHQS_ENH_Y_THRESH_OFFSET));

                VSHARPENSS_REG32_ClearAndSet(CH, VFHQS_ENH_Y_SLOPE_AGC, VFHQS_ENH_Y_HIGH_SLOPE_AGC_MASK, VFHQS_ENH_Y_HIGH_SLOPE_AGC_OFFSET, pParams->HIGH_SLOPE_AGC_Value);
                VSHARPENSS_REG32_ClearAndSet(CH, VFHQS_ENH_Y_SLOPE_AGC, VFHQS_ENH_Y_LOW_SLOPE_AGC_MASK, VFHQS_ENH_Y_LOW_SLOPE_AGC_OFFSET, pParams->LOW_SLOPE_AGC_Value);
                VSHARPENSS_REG32_ClearAndSet(CH, VFHQS_ENH_Y_SLOPE_AGC, VFHQS_ENH_Y_HIGH_THRESH_AGC_MASK, VFHQS_ENH_Y_HIGH_THRESH_AGC_OFFSET, pParams->HIGH_THRESH_AGC_Value);
                VSHARPENSS_REG32_ClearAndSet(CH, VFHQS_ENH_Y_SLOPE_AGC, VFHQS_ENH_Y_LOW_THRESH_AGC_MASK, VFHQS_ENH_Y_LOW_THRESH_AGC_OFFSET, pParams->LOW_THRESH_AGC_Value);
            }
            else
            {
                VSHARPENSS_REG32_ClearAndSet(CH, VFHQS_ENH_UV_LSHOOT_TOL, VFHQS_ENH_UV_LOSHOOT_TOL_MASK, VFHQS_ENH_UV_LOSHOOT_TOL_OFFSET, pParams->LOSHOOT_TOL_Value);
                VSHARPENSS_REG32_ClearAndSet(CH, VFHQS_ENH_UV_LSHOOT_TOL, VFHQS_ENH_UV_LUSHOOT_TOL_MASK, VFHQS_ENH_UV_LUSHOOT_TOL_OFFSET, pParams->LUSHOOT_TOL_Value);
                VSHARPENSS_REG32_ClearAndSet(CH, VFHQS_ENH_UV_SSHOOT_TOL, VFHQS_ENH_UV_SOSHOOT_TOL_MASK, VFHQS_ENH_UV_SOSHOOT_TOL_OFFSET, pParams->SOSHOOT_TOL_Value);
                VSHARPENSS_REG32_ClearAndSet(CH, VFHQS_ENH_UV_SSHOOT_TOL, VFHQS_ENH_UV_SUSHOOT_TOL_MASK, VFHQS_ENH_UV_SUSHOOT_TOL_OFFSET, pParams->SUSHOOT_TOL_Value);
                VSHARPENSS_REG32_ClearAndSet(CH, VFHQS_ENH_UV_GAIN, VFHQS_ENH_UV_LGAIN_MASK, VFHQS_ENH_UV_LGAIN_OFFSET, pParams->LGAIN_Value);
                VSHARPENSS_REG32_ClearAndSet(CH, VFHQS_ENH_UV_GAIN, VFHQS_ENH_UV_SGAIN_MASK, VFHQS_ENH_UV_SGAIN_OFFSET, pParams->SGAIN_Value);
                VSHARPENSS_REG32_ClearAndSet(CH, VFHQS_ENH_UV_EDGEGAIN, VFHQS_ENH_UV_FINALLGAIN_MASK, VFHQS_ENH_UV_FINALLGAIN_OFFSET, pParams->FINALLGAIN_Value);
                VSHARPENSS_REG32_ClearAndSet(CH, VFHQS_ENH_UV_EDGEGAIN, VFHQS_ENH_UV_FINALSGAIN_MASK, VFHQS_ENH_UV_FINALSGAIN_OFFSET, pParams->FINALSGAIN_Value);
                VSHARPENSS_REG32_Write(CH, VFHQS_ENH_UV_FINALGAIN, (pParams->FINALGAIN_Value<<VFHQS_ENH_UV_FINALGAIN_OFFSET));
                VSHARPENSS_REG32_Write(CH, VFHQS_ENH_UV_DELTA, (pParams->DELTA_Value<<VFHQS_ENH_UV_DELTA_OFFSET));
                VSHARPENSS_REG32_Write(CH, VFHQS_ENH_UV_SLOPE, (pParams->SLOPE_Value<<VFHQS_ENH_UV_SLOPE_OFFSET));
                VSHARPENSS_REG32_Write(CH, VFHQS_ENH_UV_THRESH, (pParams->THRESH_Value<<VFHQS_ENH_UV_THRESH_OFFSET));

                VSHARPENSS_REG32_ClearAndSet(CH, VFHQS_ENH_UV_SLOPE_AGC, VFHQS_ENH_UV_HIGH_SLOPE_AGC_MASK, VFHQS_ENH_UV_HIGH_SLOPE_AGC_OFFSET, pParams->HIGH_SLOPE_AGC_Value);
                VSHARPENSS_REG32_ClearAndSet(CH, VFHQS_ENH_UV_SLOPE_AGC, VFHQS_ENH_UV_LOW_SLOPE_AGC_MASK, VFHQS_ENH_UV_LOW_SLOPE_AGC_OFFSET, pParams->LOW_SLOPE_AGC_Value);
                VSHARPENSS_REG32_ClearAndSet(CH, VFHQS_ENH_UV_SLOPE_AGC, VFHQS_ENH_UV_HIGH_THRESH_AGC_MASK, VFHQS_ENH_UV_HIGH_THRESH_AGC_OFFSET, pParams->HIGH_THRESH_AGC_Value);
                VSHARPENSS_REG32_ClearAndSet(CH, VFHQS_ENH_UV_SLOPE_AGC, VFHQS_ENH_UV_LOW_THRESH_AGC_MASK, VFHQS_ENH_UV_LOW_THRESH_AGC_OFFSET, pParams->LOW_THRESH_AGC_Value);
            }
        }
        HostUpdate_SetUpdate_BE(BE_HOST_UPDATE_HQSCALER, SYNC_UPDATE);
    }
    else
    {
        return FVDP_FEATURE_NOT_SUPPORTED;// not feature support ;
    }
    return FVDP_OK;
}
