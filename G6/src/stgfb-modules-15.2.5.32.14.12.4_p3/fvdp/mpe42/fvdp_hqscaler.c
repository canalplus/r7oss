/***********************************************************************
 *
 * File: fvdp/mpe42/fvdp_hqscaler.c
 * Copyright (c) 2012 by STMicroelectronics. All rights reserved.
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
#include "fvdp_hqscaler.h"
#include "fvdp_hostupdate.h"
#include "fvdp_filtertab.h"

/* Private Types ------------------------------------------------------------ */

/* Private Constants -------------------------------------------------------- */
#define OFFSET_ZERO_DEFAULT         0x800   // no line offset
#define OFFSET_HALF_DEFAULT         0x20800 // half line offset

/* Private Macros ----------------------------------------------------------- */
#define HFHQS_REG32_Write(Ch,Addr,Val)                  REG32_Write(Addr+HQS_HF_BE_BASE_ADDRESS,Val)
#define HFHQS_REG32_Read(Ch,Addr)                       REG32_Read(Addr+HQS_HF_BE_BASE_ADDRESS)
#define HFHQS_REG32_Set(Ch,Addr,BitsToSet)              REG32_Set(Addr+HQS_HF_BE_BASE_ADDRESS,BitsToSet)
#define HFHQS_REG32_Clear(Ch,Addr,BitsToClear)          REG32_Clear(Addr+HQS_HF_BE_BASE_ADDRESS,BitsToClear)
#define HFHQS_REG32_ClearAndSet(Ch,Addr,BitsToClear,Offset,ValueToSet) \
                                                        REG32_ClearAndSet(Addr+HQS_HF_BE_BASE_ADDRESS,BitsToClear,Offset,ValueToSet)
#define HFHQS_UNIT_REG32_SetField(Ch,Addr,Field,ValueToSet)     REG32_SetField(Addr+HQS_HF_BE_BASE_ADDRESS,Field,ValueToSet)

#define VFHQS_REG32_Write(Ch,Addr,Val)                  REG32_Write(Addr+HQS_VF_BE_BASE_ADDRESS,Val)
#define VFHQS_REG32_Read(Ch,Addr)                       REG32_Read(Addr+HQS_VF_BE_BASE_ADDRESS)
#define VFHQS_REG32_Set(Ch,Addr,BitsToSet)              REG32_Set(Addr+HQS_VF_BE_BASE_ADDRESS,BitsToSet)
#define VFHQS_REG32_Clear(Ch,Addr,BitsToClear)          REG32_Clear(Addr+HQS_VF_BE_BASE_ADDRESS,BitsToClear)
#define VFHQS_REG32_ClearAndSet(Ch,Addr,BitsToClear,Offset,ValueToSet) \
                                                        REG32_ClearAndSet(Addr+HQS_VF_BE_BASE_ADDRESS,BitsToClear,Offset,ValueToSet)
#define VFHQS_UNIT_REG32_SetField(Ch,Addr,Field,ValueToSet)     REG32_SetField(Addr+HQS_VF_BE_BASE_ADDRESS,Field,ValueToSet)

/* Private Variables (static)------------------------------------------------ */
//Scaling Coefficient table bounder index
static uint16_t FilterTableBounderIndex_HQHF = 0xff;
static uint16_t FilterTableBounderIndex_HQVF = 0xff;

/* Private Function Prototypes ---------------------------------------------- */
static FVDP_Result_t HQScaler_load_ext_coefficient_table (FVDP_CH_t CH,
                  ScalerTableFormat_t TabType, FVDP_HQScalerConfig_t HQScalerInfo);

/*******************************************************************************
Name        : FVDP_Result_t HQScaler_Init (FVDP_CH_t CH)
Description : Init H/W and local Variables of HQ scaler
Parameters  :
Assumptions :
Limitations :
Returns     : FVDP_Result_t
*******************************************************************************/
FVDP_Result_t HQScaler_Init(void)
{
    FilterTableBounderIndex_HQHF = 0xff;
    FilterTableBounderIndex_HQVF = 0xff;

    //Enable scaler internal filter coefficient table
    HFHQS_REG32_Set(CH, HFHQS_CTRL, HFHQS_INT_COEF_EN_MASK);
    VFHQS_REG32_Set(CH, VFHQS_CTRL, VFHQS_INT_COEF_EN_MASK);

    return FVDP_OK;
}

/* HFHQSCALE Functions ------------------------------------------------- */
/*******************************************************************************
Name        : FVDP_Result_t HFHQScaler_HWConfig (FVDP_CH_t CH,
                             FVDP_HQScalerConfig_t HQScalerInfo)
Description : Config  HQ scaler
Parameters  : IN: CH channel
                   FVDP_HQScalerConfig_t HQScalerInfo input/output mode information
Assumptions :
Limitations :
Returns     : FVDP_Result_t
*******************************************************************************/
FVDP_Result_t HFHQScaler_HWConfig(FVDP_CH_t CH, FVDP_HQScalerConfig_t HQScalerInfo)
{
    uint16_t InputWidth = HQScalerInfo.InputFormat.HWidth;
    uint16_t OutputWidth = HQScalerInfo.OutputFormat.HWidth;
    FVDP_ColorSampling_t InputColorSampling = HQScalerInfo.InputFormat.ColorSampling;

    uint32_t hsv;
    uint32_t ScalerCtrlVal = 0;

    if (OutputWidth <= 1)
        return FVDP_VIDEO_INFO_ERROR;

    hsv = ScaleAndRound(1L << 20, InputWidth & ~1L, OutputWidth);

    HFHQS_REG32_Write(CH, HFHQS_CFG, HFHQS_EN_MASK);
    HFHQS_REG32_Write(CH, HFHQS_SCALE_VALUE, hsv);

    /* HScalser Configuration settings */
    if (InputWidth <= OutputWidth)
    {
        ScalerCtrlVal |= HFHQS_ZOOM_EN_MASK;
    }
    else
    {
        ScalerCtrlVal &= ~HFHQS_ZOOM_EN_MASK;

        /* shrink mode, and the ratio is 2x or greater. Set bit for better image quality */
        if ((CH != FVDP_AUX) && (InputWidth >= OutputWidth * 2))
        {
            ScalerCtrlVal |= HFHQS_SHRINK_2X_EN_MASK;
        }
    }

    if (InputColorSampling == FVDP_444)
    {
        ScalerCtrlVal &= ~HFHQS_COLOR_SPACE_MASK;
    }
    else    // (InputColorSampling == FVDP_422 or FVDP_420)
    {
        ScalerCtrlVal |= HFHQS_COLOR_SPACE_MASK;
    }
    HFHQS_REG32_Set(CH, HFHQS_CFG, ScalerCtrlVal);

    HFHQS_REG32_ClearAndSet(CH, HFHQS_SIZE, HFHQS_SCALED_HSIZE_MASK, HFHQS_SCALED_HSIZE_OFFSET, OutputWidth);
    HFHQS_REG32_ClearAndSet(CH, HFHQS_SIZE, HFHQS_INPUT_HSIZE_MASK, HFHQS_INPUT_HSIZE_OFFSET, InputWidth);

    if(ENABLE_EXTENAL_FILTER_COEFFICIENT == TRUE)
    {
        //Load external filter coefficient table
        HQScaler_load_ext_coefficient_table(CH, TBL_FORM_HQHF, HQScalerInfo);
        //Disable scaler internal filter coefficient table
        HFHQS_REG32_Clear(CH, HFHQS_CTRL, HFHQS_INT_COEF_EN_MASK);
    }
    else
    {
        //Enable scaler internal filter coefficient table
         HFHQS_REG32_Set(CH, HFHQS_CTRL, HFHQS_INT_COEF_EN_MASK);
    }

    HostUpdate_SetUpdate_BE(BE_HOST_UPDATE_HQSCALER, SYNC_UPDATE);

    return FVDP_OK;
}

/* VFHQSCALE Functions --------------------------------------- */
/*******************************************************************************
Name        : FVDP_Result_t VFHQScaler_HWConfig (FVDP_CH_t CH,
                             FVDP_HQScalerConfig_t HQScalerInfo,
                             FVDP_ProcessingType_t ProcessingType)
Description : Config  HQ scaler
Parameters  : IN: CH channel
                  FVDP_HQScalerConfig_t HQScalerInfo input/output mode information
                  FVDP_ProcessingType_t ProcessingType => "standard" or RGB_GRAPHIC
Assumptions :
Limitations :
Returns     : FVDP_Result_t
*******************************************************************************/
FVDP_Result_t VFHQScaler_HWConfig(FVDP_CH_t CH, FVDP_HQScalerConfig_t HQScalerInfo, FVDP_ProcessingType_t ProcessingType)
{
    uint16_t InputWidth = HQScalerInfo.InputFormat.HWidth;
    //uint16_t ScaledWidth = HQScalerInfo.OutputFormat.HWidth;
    uint16_t InputHeight = HQScalerInfo.InputFormat.VHeight;
    uint16_t OutputHeight = HQScalerInfo.OutputFormat.VHeight;
    FVDP_ColorSampling_t InputColorSampling = HQScalerInfo.InputFormat.ColorSampling;

    FVDP_ScanType_t InputScanType = HQScalerInfo.InputFormat.ScanType;
    FVDP_ScanType_t OutputScanType = HQScalerInfo.OutputFormat.ScanType;

    uint32_t vsv;
    uint32_t ScalerCtrlVal = 0;

    if (OutputHeight <= 1)
        return FVDP_VIDEO_INFO_ERROR;

    vsv = ScaleAndRound(1L << 18, InputHeight & ~1L, OutputHeight);

    VFHQS_REG32_Write(CH, VFHQS_CFG, VFHQS_EN_MASK);
    VFHQS_REG32_Write(CH, VFHQS_SCALE_VALUE, vsv);
    //VFHQS_REG32_Write(CH, VFHQS_SV_OFFSET0, 0);
    //VFHQS_REG32_Write(CH, VFHQS_CTRL, VFHQS_INT_COEF_EN_MASK);


    /* HScalser Configuration settings */
    if (InputHeight <= OutputHeight)
    {
        ScalerCtrlVal |= VFHQS_ZOOM_EN_MASK;
    }
    else
    {
        ScalerCtrlVal &= ~VFHQS_ZOOM_EN_MASK;
    }

    if (InputColorSampling == FVDP_444)
    {
        ScalerCtrlVal &= ~VFHQS_COLOR_SPACE_MASK;
    }
    else    // (ColorSampling == FVDP_422 or FVDP_420)
    {
        ScalerCtrlVal |= VFHQS_COLOR_SPACE_MASK;
    }

    if (InputScanType == INTERLACED)// Input scan type
    {
        ScalerCtrlVal |= VFHQS_INTERLACE_IN_MASK;
    }
    else
    {
        ScalerCtrlVal &= ~VFHQS_INTERLACE_IN_MASK;
    }

    if (OutputScanType == INTERLACED) // Output scan type
    {
         ScalerCtrlVal |= VFHQS_INTERLACE_OUT_MASK;
    }
    else
    {
        ScalerCtrlVal &= ~VFHQS_INTERLACE_OUT_MASK;
    }

    VFHQS_REG32_Set(CH, VFHQS_CFG, ScalerCtrlVal);

    VFHQS_REG32_ClearAndSet(CH, VFHQS_SCALED_SIZE, VFHQS_SCALED_VSIZE_MASK, VFHQS_SCALED_VSIZE_OFFSET, OutputHeight);
    VFHQS_REG32_ClearAndSet(CH, VFHQS_INPUT_SIZE, VFHQS_INPUT_VSIZE_MASK, VFHQS_INPUT_VSIZE_OFFSET, InputHeight);
    #if 1
    VFHQS_REG32_ClearAndSet(CH, VFHQS_INPUT_SIZE, VFHQS_INPUT_HSIZE_MASK, VFHQS_INPUT_HSIZE_OFFSET, InputWidth);
    #else
    VFHQS_REG32_ClearAndSet(CH, VFHQS_INPUT_SIZE, VFHQS_INPUT_HSIZE_MASK, VFHQS_INPUT_HSIZE_OFFSET, ScaledWidth);
    #endif

    if(ENABLE_EXTENAL_FILTER_COEFFICIENT == TRUE)
    {
        //Load external filter coefficient table
        HQScaler_load_ext_coefficient_table(CH, TBL_FORM_HQVF, HQScalerInfo);
        //Disable scaler internal filter coefficient table
        if((ProcessingType == FVDP_RGB_GRAPHIC) && (vsv < 0x40000))
        {
            VFHQS_REG32_Set(CH, VFHQS_CTRL, VFHQS_INT_COEF_EN_MASK);
        }
        else
        {
            VFHQS_REG32_Clear(CH, VFHQS_CTRL, VFHQS_INT_COEF_EN_MASK);
         }
    }
    else
    {
        //Enable scaler internal filter coefficient table
         VFHQS_REG32_Set(CH, VFHQS_CTRL, VFHQS_INT_COEF_EN_MASK);
    }

    // TODO: check the effective HostUpdate method
    //VFHQS_REG32_Set(CH, VFHQS_UPDATE_CTRL, VFHQS_FORCE_UPDATE_EN_MASK);
    HostUpdate_SetUpdate_BE(BE_HOST_UPDATE_HQSCALER, SYNC_UPDATE);

    // Need to check
    if ((InputScanType == PROGRESSIVE) && (OutputScanType == PROGRESSIVE))
    {
        VFHQS_REG32_Set(CH, VFHQS_OFFSET0, OFFSET_ZERO_DEFAULT);
        VFHQS_REG32_Set(CH, VFHQS_OFFSET1, OFFSET_HALF_DEFAULT);
    }
    else if ((InputScanType == PROGRESSIVE) && (OutputScanType == INTERLACED))
    {
        if (InputHeight <= OutputHeight)
        {
            VFHQS_REG32_Set(CH, VFHQS_OFFSET0, OFFSET_ZERO_DEFAULT);
            VFHQS_REG32_Set(CH, VFHQS_OFFSET1, vsv/2);
        }
        else
        {
            // TBD -- Croping Line
            VFHQS_REG32_Set(CH, VFHQS_OFFSET0, OFFSET_ZERO_DEFAULT);
            VFHQS_REG32_Set(CH, VFHQS_OFFSET1, ((vsv/2)%0x40000));
        }
    }
    else if ((InputScanType == INTERLACED) && (OutputScanType == PROGRESSIVE))
    {
        VFHQS_REG32_Set(CH, VFHQS_OFFSET0, OFFSET_ZERO_DEFAULT);
        VFHQS_REG32_Set(CH, VFHQS_OFFSET1, OFFSET_HALF_DEFAULT);
        // TODO: Set input field ID bit (odd=1, even=0)
    }
    else if ((InputScanType == INTERLACED) && (OutputScanType == INTERLACED))
    {
        if (InputHeight <= OutputHeight)
        {
            // TODO: check output odd field.
            /*
                    if (output == odd field)
                    {
                        VFHQS_REG32_Set(CH, VFHQS_OFFSET0, OFFSET_ZERO_DEFAULT);
                        VFHQS_REG32_Set(CH, VFHQS_OFFSET1, OFFSET_HALF_DEFAULT);
                        // TODO: Set input field ID bit (odd=1, even=0)
                    }
                    else
                    {
                        VFHQS_REG32_Set(CH, VFHQS_OFFSET0, vsv/2);
                        VFHQS_REG32_Set(CH, VFHQS_OFFSET1, MOD(vsv/2+0x20000, 0x40000));
                    }
                    */
        }
        else
        {
            // TODO: check output odd field.
            /*
                    if (output == odd field)
                    {
                        VFHQS_REG32_Set(CH, VFHQS_OFFSET0, OFFSET_ZERO_DEFAULT);
                        VFHQS_REG32_Set(CH, VFHQS_OFFSET1, OFFSET_HALF_DEFAULT);
                        // TODO: Set input field ID bit (odd=1, even=0)
                    }
                    else
                    {
                        if (input == odd field)
                        {
                            VFHQS_REG32_Set(CH, VFHQS_OFFSET0, OFFSET_ZERO_DEFAULT);
                            VFHQS_REG32_Set(CH, VFHQS_OFFSET1, OFFSET_HALF_DEFAULT);
                            // TODO: Set input field ID bit (odd=1, even=0)
                        }
                        else
                        {
                            VFHQS_REG32_Set(CH, VFHQS_OFFSET0, OFFSET_ZERO_DEFAULT);
                            VFHQS_REG32_Set(CH, VFHQS_OFFSET1, OFFSET_HALF_DEFAULT);
                            // TODO: Set input field ID bit (odd=1, even=0)
                        }
                    }
                    */
        }
    }
    return FVDP_OK;
}

/*******************************************************************************
Name        : FVDP_Result_t HQScaler_load_ext_coefficient_table (FVDP_CH_t CH,
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
static FVDP_Result_t HQScaler_load_ext_coefficient_table (FVDP_CH_t CH,
                  ScalerTableFormat_t TabType, FVDP_HQScalerConfig_t HQScalerInfo)
{
    bool IsRGB;
    bool  IsProcessingRGB    = HQScalerInfo.ProcessingRGB;
    uint16_t Insize_multi_factor = 1;
    uint16_t Outsize_multi_factor = 1;
    uint16_t Insize;
    uint16_t Outsize;
    uint16_t Power_factor;
    uint16_t InputHeight              = HQScalerInfo.InputFormat.VHeight;
    uint16_t OutputHeight             = HQScalerInfo.OutputFormat.VHeight;
    uint16_t InputWidth               = HQScalerInfo.InputFormat.HWidth;
    uint16_t OutputWidth              = HQScalerInfo.OutputFormat.HWidth;
    FVDP_ColorSpace_t InputColorSpace = HQScalerInfo.InputFormat.ColorSpace;
    FVDP_ScanType_t InputScanType     = HQScalerInfo.InputFormat.ScanType;
    FVDP_ScanType_t OutputScanType    = HQScalerInfo.OutputFormat.ScanType;
    FVDP_Result_t  ErrorCode = FVDP_OK;
    uint32_t ScaleValue;
    uint16_t CurrentIndex;
    uint16_t PreviousIndex;

    IsRGB = (InputColorSpace == RGB) ||(InputColorSpace == sRGB) ||(InputColorSpace == RGB861);

    Power_factor = (TabType == TBL_FORM_HQVF) ? 18: 20;

    /* for interlaced input case, scaling index needs to be multipled by 2
       recommended by AE */
    if(TabType == TBL_FORM_HQVF)
    {
        if(InputScanType == INTERLACED)
        {
            Insize_multi_factor = 2;
        }
        // for interlaced input case with zoom needs to be multipled by 2
        if((OutputScanType == INTERLACED) && (InputHeight <= OutputHeight))
        {
            Outsize_multi_factor = 2;
        }

        Insize = InputHeight;
        Outsize = OutputHeight;
        PreviousIndex = FilterTableBounderIndex_HQVF;
    }
    else // TBL_FORM_HQHF
    {
        Insize = InputWidth;
        Outsize = OutputWidth;
        PreviousIndex = FilterTableBounderIndex_HQHF;
    }

    if(Outsize < 1)
    {
       return FVDP_ERROR;
    }

    ScaleValue = ((Insize * Insize_multi_factor)<< Power_factor) / (Outsize * Outsize_multi_factor);

    FilterTable_GetBounderIndex(TabType, ScaleValue, IsRGB, &CurrentIndex,IsProcessingRGB);

    if(PreviousIndex != CurrentIndex)
    {
        //Load external filter coefficient table
        ErrorCode = FilterTableLoad(FILTER_BE, TabType, ScaleValue, IsRGB,IsProcessingRGB);
        //Update Scaling Coefficient table bounder index
        if(TabType == TBL_FORM_HQVF)
        {
            FilterTableBounderIndex_HQVF = CurrentIndex;
        }
        else //TBL_FORM_HQHF
        {
            FilterTableBounderIndex_HQHF = CurrentIndex;
        }
    }

    return ErrorCode;
}

/*******************************************************************************
Name        : HQScaler_DcdiUpdate
Description : Set up HQScaler DCDi Status and Strength
Parameters  : void
Assumptions :
Limitations :
Returns     : FVDP_Result_t type value:
              FVDP_OK no error
*******************************************************************************/
FVDP_Result_t HQScaler_DcdiUpdate (FVDP_CH_t CH, bool Enable, uint8_t Strength)
{
    VFHQS_REG32_Set(CH, VFHQS_DCDI_CTRL2, 0);
    VFHQS_REG32_Set(CH, VFHQS_DCDI_CTRL3, 0);
    VFHQS_REG32_Set(CH, VFHQS_DCDI_CTRL4, 0);
    VFHQS_REG32_Set(CH, VFHQS_DCDI_COR_OFFSET1, 0);
    VFHQS_REG32_Set(CH, VFHQS_DCDI_COR_OFFSET2, 0);
    VFHQS_REG32_Set(CH, VFHQS_DCDI_FLT_COEF1, 0);
    VFHQS_REG32_Set(CH, VFHQS_DCDI_FLT_COEF2, 0);
    VFHQS_REG32_Set(CH, VFHQS_DCDI_VERT_THRD1, 0);
    VFHQS_REG32_Set(CH, VFHQS_DCDI_VERT_THRD2, 0);
    VFHQS_REG32_Set(CH, VFHQS_DCDI_DNLF, 0);

    /* Set up scaler DCDi streangth */
    VFHQS_REG32_ClearAndSet(CH, VFHQS_DCDI_CTRL2, VFHQS_DCDI_PELCORR_TH_MASK, VFHQS_DCDI_PELCORR_TH_OFFSET, Strength);

    /* Set scaler DCDi enable */
    VFHQS_REG32_ClearAndSet(CH, VFHQS_DCDI_CTRL1, VFHQS_DCDI_ENABLE_MASK,VFHQS_DCDI_ENABLE_OFFSET, Enable);

    //Disable HQ DCDi until all of DCDi register value is set
    VFHQS_REG32_ClearAndSet(CH, VFHQS_DCDI_CTRL1, VFHQS_DCDI_ENABLE_MASK,VFHQS_DCDI_ENABLE_OFFSET, FALSE);

    return (FVDP_OK);
}

