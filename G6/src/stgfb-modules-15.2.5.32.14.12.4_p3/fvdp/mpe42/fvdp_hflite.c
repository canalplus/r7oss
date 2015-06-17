/***********************************************************************
 *
 * File: fvdp/mpe42/fvdp_hflite.c
 * Copyright (c) 2011 by STMicroelectronics. All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/


/* Includes ----------------------------------------------------------------- */
#include "fvdp_common.h"
#include "fvdp_update.h"
#include "fvdp_system.h"
#include "fvdp_scalerlite.h"
#include "fvdp_hostupdate.h"
#include "fvdp_filtertab.h"

/* Private Types ------------------------------------------------------------ */

/* Private Constants -------------------------------------------------------- */

/* Private Macros ----------------------------------------------------------- */
static const uint32_t HFLITE_BASE_ADDR[] = {HFLITE_FE_BASE_ADDRESS, HFLITE_PIP_BASE_ADDRESS, HFLITE_AUX_BASE_ADDRESS, HFLITE_ENC_BASE_ADDRESS};

#define HFLITE_REG32_Write(Ch,Addr,Val)                REG32_Write(Addr+HFLITE_BASE_ADDR[Ch],Val)
#define HFLITE_REG32_Read(Ch,Addr)                     REG32_Read(Addr+HFLITE_BASE_ADDR[Ch])
#define HFLITE_REG32_Set(Ch,Addr,BitsToSet)            REG32_Set(Addr+HFLITE_BASE_ADDR[Ch],BitsToSet)
#define HFLITE_REG32_Clear(Ch,Addr,BitsToClear)        REG32_Clear(Addr+HFLITE_BASE_ADDR[Ch],BitsToClear)
#define HFLITE_REG32_ClearAndSet(Ch,Addr,BitsToClear,Offset,ValueToSet) \
                                                    REG32_ClearAndSet(Addr+HFLITE_BASE_ADDR[Ch],BitsToClear,Offset,ValueToSet)
#define HFLITE_UNIT_REG32_SetField(Ch,Addr,Field,ValueToSet)    REG32_SetField(Addr+HFLITE_BASE_ADDR[Ch],Field,ValueToSet)

#define HFLITE_SHRINK_2X_EN_IN_VCPU
/* Private Variables (static)------------------------------------------------ */
//Scaling Coefficient table bounder index
static uint16_t FilterTableBounderIndex_HLite[NUM_FVDP_CH] = {0xff, 0xff, 0xff, 0xff};

/* Private Function Prototypes ---------------------------------------------- */
static FVDP_Result_t HScaler_Lite_load_ext_coefficient_table (FVDP_CH_t CH,
                  ScalerTableFormat_t TabType,FVDP_ScalerLiteConfig_t Scaler_VideoInfo);

/*******************************************************************************
Name        : FVDP_Result_t HScaler_Lite_Init (FVDP_CH_t CH)
Description : Init H/W and local Variables of HScaler Lite
Parameters  : IN: CH channel
Assumptions :
Limitations :
Returns     : FVDP_Result_t
*******************************************************************************/
FVDP_Result_t HScaler_Lite_Init(FVDP_CH_t CH)
{
    FilterTableBounderIndex_HLite[CH] = 0xff;

    //Enable scaler internal filter coefficient table
     HFLITE_REG32_Set(CH, HFLITE_CTRL, HFLITE_INT_COEF_EN_MASK);

    return FVDP_OK;
}

/* HSCALE LITE Functions ------------------------------------------------- */
/*******************************************************************************
Name        : FVDP_Result_t HScaler_Lite_HWConfig (FVDP_CH_t CH,
                             FVDP_ScalerLiteConfig_t Scaler_VideoInfo)
Description : Config  H scaler lite scaler
Parameters  : IN: CH channel
                   FVDP_ScalerLiteConfig_t Scaler_VideoInfo: input/output mode information
Assumptions :
Limitations :
Returns     : FVDP_Result_t
*******************************************************************************/
FVDP_Result_t HScaler_Lite_HWConfig(FVDP_CH_t CH, FVDP_ScalerLiteConfig_t Scaler_VideoInfo)
{
    uint16_t InputWidth = Scaler_VideoInfo.InputFormat.HWidth;
    uint16_t OutputWidth = Scaler_VideoInfo.OutputFormat.HWidth;
    FVDP_ColorSampling_t InputColorSampling = Scaler_VideoInfo.InputFormat.ColorSampling;

    uint32_t hsv;
    uint32_t ScalerCtrlVal = HFLITE_REG32_Read(CH, HFLITE_CFG);

    if (OutputWidth <= 1)
        return FVDP_VIDEO_INFO_ERROR;

    hsv = ScaleAndRound(1L << 20, InputWidth & ~1L, OutputWidth);

    HFLITE_REG32_Write(CH, HFLITE_SCALE_VALUE, hsv);
    HFLITE_REG32_Write(CH, HFLITE_SV_OFFSET0, 0);


    /* HScalser Configuration settings */

    //Enable HScalser
    ScalerCtrlVal |= HFLITE_EN_MASK;

   if (InputWidth <= OutputWidth)
    {
        ScalerCtrlVal |= HFLITE_ZOOM_EN_MASK;
    }
    else
    {
        ScalerCtrlVal &= ~HFLITE_ZOOM_EN_MASK;

#ifndef HFLITE_SHRINK_2X_EN_IN_VCPU
        /* shrink mode, and the ratio is 2x or greater. Set bit for better image quality */
        if ((CH != FVDP_AUX) && (InputWidth >= OutputWidth * 2))
        {
            ScalerCtrlVal |= HFLITE_SHRINK_2X_EN_MASK;
        }
#endif
    }

    if (InputColorSampling == FVDP_444)
    {
        ScalerCtrlVal &= ~HFLITE_COLOR_SPACE_MASK;
    }
    else    // (InputColorSampling == FVDP_422 or FVDP_420)
    {
        ScalerCtrlVal |= HFLITE_COLOR_SPACE_MASK;
    }
    HFLITE_REG32_Write(CH, HFLITE_CFG, ScalerCtrlVal);

    HFLITE_REG32_ClearAndSet(CH, HFLITE_SIZE, HFLITE_SCALED_HSIZE_MASK, HFLITE_SCALED_HSIZE_OFFSET, OutputWidth);

    if(ScalerCtrlVal & HFLITE_SHRINK_2X_EN_MASK)
    {
    // Scaler input width should be multiple by 4 when prefilter is enabled
        InputWidth = DivRoundUp(InputWidth, 4) * 4;
    }

    HFLITE_REG32_ClearAndSet(CH, HFLITE_SIZE, HFLITE_INPUT_HSIZE_MASK, HFLITE_INPUT_HSIZE_OFFSET, InputWidth);

    if(ENABLE_EXTENAL_FILTER_COEFFICIENT == TRUE)
    {
        //Load external filter coefficient table
        HScaler_Lite_load_ext_coefficient_table (CH, TBL_FORM_HF, Scaler_VideoInfo);
        //Disable scaler internal filter coefficient table
        HFLITE_REG32_Clear(CH, HFLITE_CTRL, HFLITE_INT_COEF_EN_MASK);
    }
    else
    {
        //Enable scaler internal filter coefficient table
         HFLITE_REG32_Set(CH, HFLITE_CTRL, HFLITE_INT_COEF_EN_MASK);
    }

    // Set the Host Update flag
    if (CH == FVDP_MAIN)
    {
        HostUpdate_SetUpdate_FE(FE_HOST_UPDATE_HFLITE, SYNC_UPDATE);
    }
    else
    {
        if (CH == FVDP_ENC)
        {
            // ENC channel requires FORCE UPDATE for mem to mem
            HostUpdate_SetUpdate_LITE(CH, LITE_HOST_UPDATE_HFLITE, FORCE_UPDATE);
        }
        else
        {
            HostUpdate_SetUpdate_LITE(CH, LITE_HOST_UPDATE_HFLITE, SYNC_UPDATE);
        }
    }

    return FVDP_OK;
}

/*******************************************************************************
Name        : void HScaler_Lite_load_ext_coefficient_table (FVDP_CH_t CH,
                          ScalerTableFormat_t TabType,FVDP_ScalerLiteConfig_t Scaler_VideoInfo)
Description : The function load scaler coefficient filter table
              returns the vertical size to be processed
Parameters  : IN: CH channel
                 ScalerTableFormat_t TabType
                 FVDP_ScalerLiteConfig_t Scaler_VideoInfo
Assumptions :
Limitations :
Returns     : FVDP_Result_t
*******************************************************************************/
static FVDP_Result_t HScaler_Lite_load_ext_coefficient_table (FVDP_CH_t CH,
                  ScalerTableFormat_t TabType,FVDP_ScalerLiteConfig_t Scaler_VideoInfo)
{
    bool IsRGB;
    bool IsProcessingRGB        = Scaler_VideoInfo.ProcessingRGB;
    uint16_t Power_factor;
    uint16_t InputWidth               = Scaler_VideoInfo.InputFormat.HWidth;
    uint16_t OutputWidth              = Scaler_VideoInfo.OutputFormat.HWidth;
    FVDP_ColorSpace_t InputColorSpace = Scaler_VideoInfo.InputFormat.ColorSpace;
    FILTER_Core_t FilterCore;
    FVDP_Result_t ErrorCode = FVDP_OK;
    uint32_t ScaleValue;
    uint16_t Index;

    if(OutputWidth < 1)
    {
       return FVDP_ERROR;
    }

    IsRGB = (InputColorSpace == RGB) ||(InputColorSpace == sRGB)  ||(InputColorSpace == RGB861) ;

    Power_factor =  20;

    switch(CH)
    {
        case FVDP_MAIN:
            FilterCore = FILTER_FE;
        break;

        case FVDP_PIP:
            FilterCore = FILTER_PIP;
        break;

        case FVDP_AUX:
            FilterCore = FILTER_AUX;
        break;

        case FVDP_ENC:
            FilterCore = FILTER_ENC;
        break;

         default:
            FilterCore = FILTER_FE;
        break;
    }

    ScaleValue = ((InputWidth<<Power_factor) / OutputWidth);

    FilterTable_GetBounderIndex(TabType, ScaleValue, IsRGB, &Index,IsProcessingRGB);

    if(FilterTableBounderIndex_HLite[CH] != Index)
    {
        //Load external filter coefficient table
        ErrorCode = FilterTableLoad(FilterCore, TabType, ScaleValue, IsRGB,IsProcessingRGB);
        //Update Scaling Coefficient table bounder index
        FilterTableBounderIndex_HLite[CH] = Index;
    }

    return ErrorCode;
}
