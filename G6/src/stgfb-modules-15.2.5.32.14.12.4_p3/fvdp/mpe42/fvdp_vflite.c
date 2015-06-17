/***********************************************************************
 *
 * File: fvdp/mpe42/fvdp_vflite.c
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
#include "fvdp_scalerlite.h"
#include "fvdp_hostupdate.h"
#include "fvdp_eventctrl.h"
#include "fvdp_filtertab.h"

/* Private Types ------------------------------------------------------------ */

/* Private Constants -------------------------------------------------------- */

/* Private Macros ----------------------------------------------------------- */
static const uint32_t VFLITE_BASE_ADDR[] = {VFLITE_FE_BASE_ADDRESS, VFLITE_PIP_BASE_ADDRESS, VFLITE_AUX_BASE_ADDRESS, VFLITE_ENC_BASE_ADDRESS};

#define VFLITE_REG32_Write(Ch,Addr,Val)                REG32_Write(Addr+VFLITE_BASE_ADDR[Ch],Val)
#define VFLITE_REG32_Read(Ch,Addr)                     REG32_Read(Addr+VFLITE_BASE_ADDR[Ch])
#define VFLITE_REG32_Set(Ch,Addr,BitsToSet)            REG32_Set(Addr+VFLITE_BASE_ADDR[Ch],BitsToSet)
#define VFLITE_REG32_Clear(Ch,Addr,BitsToClear)        REG32_Clear(Addr+VFLITE_BASE_ADDR[Ch],BitsToClear)
#define VFLITE_REG32_ClearAndSet(Ch,Addr,BitsToClear,Offset,ValueToSet) \
                                                    REG32_ClearAndSet(Addr+VFLITE_BASE_ADDR[Ch],BitsToClear,Offset,ValueToSet)
#define VFLITE_UNIT_REG32_SetField(Ch,Addr,Field,ValueToSet)    REG32_SetField(Addr+VFLITE_BASE_ADDR[Ch],Field,ValueToSet)

/* VSCALE LITE Functions --------------------------------------- */
static FVDP_Result_t VScaler_Lite_load_ext_coefficient_table (FVDP_CH_t CH,
                  ScalerTableFormat_t TabType,FVDP_ScalerLiteConfig_t Scaler_VideoInfo);

/* Private Variables (static)------------------------------------------------ */
//Scaling Coefficient table bounder index
static uint16_t FilterTableBounderIndex_VLite[NUM_FVDP_CH] = {0xff, 0xff, 0xff, 0xff};

/*******************************************************************************
Name        : FVDP_Result_t HScaler_Lite_Init (FVDP_CH_t CH)
Description : Init H/W and local Variables of HScaler Lite
Parameters  : IN: CH channel
Assumptions :
Limitations :
Returns     : FVDP_Result_t
*******************************************************************************/
FVDP_Result_t VScaler_Lite_Init(FVDP_CH_t CH)
{
    FilterTableBounderIndex_VLite[CH] = 0xff;

    //Enable scaler internal filter coefficient table
    VFLITE_REG32_Set(CH, VFLITE_CTRL, VFLITE_INT_COEF_EN_MASK);

    return FVDP_OK;
}


/*******************************************************************************
Name        : VScaler_Lite_HWConfig
Description :
Parameters  :

Assumptions :
Limitations :
Returns     :

*******************************************************************************/
FVDP_Result_t VScaler_Lite_HWConfig(FVDP_CH_t CH, FVDP_ScalerLiteConfig_t Scaler_VideoInfo)
{
    uint16_t InputWidth = Scaler_VideoInfo.InputFormat.HWidth;
    uint16_t InputHeight = Scaler_VideoInfo.InputFormat.VHeight;
    uint16_t OutputHeight = Scaler_VideoInfo.OutputFormat.VHeight;
    FVDP_ColorSampling_t InputColorSampling = Scaler_VideoInfo.InputFormat.ColorSampling;

    FVDP_ScanType_t InputScanType = Scaler_VideoInfo.InputFormat.ScanType;
    FVDP_ScanType_t OutputScanType = Scaler_VideoInfo.OutputFormat.ScanType;

    uint32_t vsv;
    uint32_t ScalerCtrlVal = 0;

    if (OutputHeight <= 1)
        return FVDP_VIDEO_INFO_ERROR;

    vsv = ScaleAndRound(1L << 18, InputHeight & ~1L, OutputHeight);

    VFLITE_REG32_Write(CH, VFLITE_SCALE_VALUE, vsv);
    //VFLITE_REG32_Write(CH, VFLITE_SV_OFFSET0, 0);

    //Interlace in to interlase out treat same as progressive in to progressive out
    if(((CH == FVDP_PIP)||(CH == FVDP_AUX))
        && (InputScanType == INTERLACED)
        && (OutputScanType == INTERLACED))
    {
        InputScanType = PROGRESSIVE;
        OutputScanType = PROGRESSIVE;
    }

    /* VScalser Configuration settings */
    //Enable VScalser
    ScalerCtrlVal |= VFLITE_EN_MASK;

    if(CH == FVDP_MAIN)
    {
    // Disable for temporally because 8 Tap vertical coefficient table is not ready
      // ScalerCtrlVal |= VFLITE_FILTER_TAP_MASK;
    }

    if (InputHeight <= OutputHeight)
    {
        ScalerCtrlVal |= VFLITE_ZOOM_EN_MASK;
    }
    else
    {
        ScalerCtrlVal &= ~VFLITE_ZOOM_EN_MASK;
    }

    if (InputColorSampling == FVDP_444)
    {
        ScalerCtrlVal &= ~VFLITE_COLOR_SPACE_MASK;
    }
    else    // (ColorSampling == FVDP_422 or FVDP_420)
    {
        ScalerCtrlVal |= VFLITE_COLOR_SPACE_MASK;
    }

    if (InputScanType == INTERLACED)
    {
        ScalerCtrlVal |= VFLITE_INTERLACE_IN_MASK;
    }
    else    // (ColorSampling == FVDP_422 or FVDP_420)
    {
        ScalerCtrlVal &= ~VFLITE_INTERLACE_IN_MASK;
    }


    if (OutputScanType == INTERLACED)
    {
        ScalerCtrlVal |= VFLITE_INTERLACE_OUT_MASK;
    }
    else    // (ColorSampling == FVDP_422 or FVDP_420)
    {
        ScalerCtrlVal &= ~VFLITE_INTERLACE_OUT_MASK;
    }

    VFLITE_REG32_Write(CH, VFLITE_CFG, ScalerCtrlVal);

    VFLITE_REG32_ClearAndSet(CH, VFLITE_SCALED_SIZE, VFLITE_SCALED_VSIZE_MASK, VFLITE_SCALED_VSIZE_OFFSET, OutputHeight);
    VFLITE_REG32_ClearAndSet(CH, VFLITE_INPUT_SIZE, VFLITE_INPUT_VSIZE_MASK, VFLITE_INPUT_VSIZE_OFFSET, InputHeight);
    VFLITE_REG32_ClearAndSet(CH, VFLITE_INPUT_SIZE, VFLITE_INPUT_HSIZE_MASK, VFLITE_INPUT_HSIZE_OFFSET, InputWidth);

    if(ENABLE_EXTENAL_FILTER_COEFFICIENT == TRUE)
    {
        //Load external filter coefficient table
        VScaler_Lite_load_ext_coefficient_table (CH, TBL_FORM_VF, Scaler_VideoInfo);
        //Disable scaler internal filter coefficient table
        VFLITE_REG32_Clear(CH, VFLITE_CTRL, VFLITE_INT_COEF_EN_MASK);
    }
    else
    {
        //Enable scaler internal filter coefficient table
         VFLITE_REG32_Set(CH, VFLITE_CTRL, VFLITE_INT_COEF_EN_MASK);
    }


    // Set the Host Update flag
    if (CH == FVDP_MAIN)
    {
        HostUpdate_SetUpdate_FE(FE_HOST_UPDATE_VFLITE, SYNC_UPDATE);
    }
    else
    {
        if (CH == FVDP_ENC)
        {
            // ENC channel requires FORCE UPDATE for mem to mem
            HostUpdate_SetUpdate_LITE(CH, LITE_HOST_UPDATE_VFLITE, FORCE_UPDATE);
        }
        else
        {
            HostUpdate_SetUpdate_LITE(CH, LITE_HOST_UPDATE_VFLITE, SYNC_UPDATE);
        }
    }

    return FVDP_OK;

}

/*******************************************************************************
Name        :VScaler_Lite_OffsetUpdate
Description :
Parameters  :

Assumptions :
Limitations :
Returns     :

*******************************************************************************/
FVDP_Result_t VScaler_Lite_OffsetUpdate(FVDP_CH_t CH, FVDP_ScalerLiteConfig_t Scaler_VideoInfo)
{
    FVDP_ScanType_t InputScanType = Scaler_VideoInfo.InputFormat.ScanType;
    FVDP_ScanType_t OutputScanType = Scaler_VideoInfo.OutputFormat.ScanType;
    uint16_t InputHeight = Scaler_VideoInfo.InputFormat.VHeight;
    uint16_t OutputHeight = Scaler_VideoInfo.OutputFormat.VHeight;

    uint32_t vsv;

    FVDP_FieldType_t InputFieldType = Scaler_VideoInfo.InputFormat.FieldType;
    FVDP_FieldType_t OutputFieldType = Scaler_VideoInfo.OutputFormat.FieldType;
    DATAPATH_LITE_t DatapathLite = Scaler_VideoInfo.CheckDatapathLite;

    vsv = VFLITE_REG32_Read(CH, VFLITE_SCALE_VALUE);


    if(OutputFieldType == AUTO_FIELD_TYPE)
     {
        OutputFieldType = InputFieldType;
    }

    // Need to check
    if ((InputScanType == PROGRESSIVE) && (OutputScanType == PROGRESSIVE))
    {
        VFLITE_REG32_Write(CH, VFLITE_OFFSET0, 0x800);
        VFLITE_REG32_Write(CH, VFLITE_OFFSET1, 0x800);
    }
    else if ((InputScanType == PROGRESSIVE) && (OutputScanType == INTERLACED))
    {
        if (InputHeight <= OutputHeight)
        {
            VFLITE_REG32_Write(CH, VFLITE_OFFSET0, 0x800);
            VFLITE_REG32_Write(CH, VFLITE_OFFSET1, (vsv/2 + 0x800));
        }
        else
        {
            // TBD -- Croping Line
            //VFLITE_REG32_Set(CH, VFLITE_OFFSET0, MOD(vsv/2, 0x40000));
            //VFLITE_REG32_Set(CH, VFLITE_OFFSET1, 0);
        }
    }
    else if ((InputScanType == INTERLACED) && (OutputScanType == PROGRESSIVE))
    {
        VFLITE_REG32_Write(CH, VFLITE_OFFSET0, 0x0800);
        VFLITE_REG32_Write(CH, VFLITE_OFFSET1, 0x20800);
        // TODO: Set input field ID bit (odd=1, even=0)
    }
    else if ((InputScanType == INTERLACED) && (OutputScanType == INTERLACED))
    {

        if(CH == FVDP_MAIN)
        {
            VFLITE_REG32_Write(CH, VFLITE_OFFSET0, 0x800);
            VFLITE_REG32_Write(CH, VFLITE_OFFSET1, 0x800);
        }
        else
        {
            if (InputHeight <= OutputHeight)
            {
                if ((DatapathLite == DATAPATH_LITE_HZOOM_VZOOM)
                || (DatapathLite == DATAPATH_LITE_HSHRINK_VZOOM))
                {
                    if (InputHeight == OutputHeight)
                    {
                        VFLITE_REG32_Write(CH, VFLITE_OFFSET0, 0x800);
                        VFLITE_REG32_Write(CH, VFLITE_OFFSET1, 0x800);
                    }
                }
                else
                {
                    VFLITE_REG32_Write(CH, VFLITE_OFFSET0, 0x800);
                    if (InputFieldType == BOTTOM_FIELD)
                    {
                        VFLITE_REG32_Write(CH, VFLITE_OFFSET1, vsv - 0x20000 + 0x800);
                    }
                    else
                    {
                        VFLITE_REG32_Write(CH, VFLITE_OFFSET1, 0x800);
                    }
                }
            }
            else
            {
                if(InputFieldType == OutputFieldType)
                {

                     VFLITE_REG32_Write(CH, VFLITE_OFFSET0, 0x800);
                     VFLITE_REG32_Write(CH, VFLITE_OFFSET1, 0x20800);
                     // TODO: Set input field ID bit (odd=1, even=0)
                }
                else
                {
                    if (OutputFieldType == TOP_FIELD)
                    {
                        VFLITE_REG32_Write(CH, VFLITE_OFFSET0, 0x800);
                        VFLITE_REG32_Write(CH, VFLITE_OFFSET1, 0x20800);
                        // TODO: Set input field ID bit (odd=1, even=0)
                    }
                    else
                    {
                        VFLITE_REG32_Write(CH, VFLITE_OFFSET0, 0x800);
                        VFLITE_REG32_Write(CH, VFLITE_OFFSET1, 0x20800);
                        // TODO: Set input field ID bit (odd=1, even=0)
                    }
                }
            }
        }
    }

    // Set the Host Update flag
    if (CH == FVDP_MAIN)
    {
        HostUpdate_SetUpdate_FE(FE_HOST_UPDATE_VFLITE, SYNC_UPDATE);
    }
    else
    {
        if (CH == FVDP_ENC)
        {
            // ENC channel requires FORCE UPDATE for mem to mem
            HostUpdate_SetUpdate_LITE(CH, LITE_HOST_UPDATE_VFLITE, FORCE_UPDATE);
        }
        else
        {
            HostUpdate_SetUpdate_LITE(CH, LITE_HOST_UPDATE_VFLITE, SYNC_UPDATE);
        }
    }

    return FVDP_OK;
}

/*******************************************************************************
Name        : FVDP_Result_t VScaler_Lite_load_ext_coefficient_table (FVDP_CH_t CH,
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
static FVDP_Result_t VScaler_Lite_load_ext_coefficient_table (FVDP_CH_t CH,
                  ScalerTableFormat_t TabType,FVDP_ScalerLiteConfig_t Scaler_VideoInfo)
{
    bool IsRGB;
    bool IsProcessingRGB               = Scaler_VideoInfo.ProcessingRGB;
    uint16_t Insize_multi_factor = 1;
    uint16_t Outsize_multi_factor = 1;
    uint16_t Power_factor;
    uint16_t InputHeight              = Scaler_VideoInfo.InputFormat.VHeight;
    uint16_t OutputHeight             = Scaler_VideoInfo.OutputFormat.VHeight;
    FVDP_ColorSpace_t InputColorSpace = Scaler_VideoInfo.InputFormat.ColorSpace;
    FVDP_ScanType_t InputScanType     = Scaler_VideoInfo.InputFormat.ScanType;
    FVDP_ScanType_t OutputScanType    = Scaler_VideoInfo.OutputFormat.ScanType;
    FILTER_Core_t FilterCore;
    FVDP_Result_t  ErrorCode = FVDP_OK;
    uint32_t ScaleValue;
    uint16_t Index;

    if(OutputHeight < 1)
    {
       return FVDP_ERROR;
    }

    IsRGB = (InputColorSpace == RGB) ||(InputColorSpace == sRGB) ||(InputColorSpace == RGB861) ;

    Power_factor = 18;

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

    /* for interlaced input case, scaling index needs to be multipled by 2
       recommended by AE */
    if(InputScanType == INTERLACED)
    {
        Insize_multi_factor = 2;
    }
    // for interlaced input case with zoom needs to be multipled by 2
    if((OutputScanType == INTERLACED) && (InputHeight <= OutputHeight))
    {
        Outsize_multi_factor = 2;
    }

    ScaleValue = ((InputHeight * Insize_multi_factor)<< Power_factor) / (OutputHeight * Outsize_multi_factor);

    //Update Scaling Coefficient table bounder index
    FilterTable_GetBounderIndex(TabType, ScaleValue, IsRGB, &Index,IsProcessingRGB);

    if(FilterTableBounderIndex_VLite[CH] != Index)
    {
        //Load external filter coefficient table
        ErrorCode = FilterTableLoad(FilterCore, TabType, ScaleValue, IsRGB,IsProcessingRGB);

        //Save Scaling Coefficient table bounder index
        FilterTableBounderIndex_VLite[CH] = Index;
    }

    return ErrorCode;
}

/*******************************************************************************
Name        : VScaler_Lite_DcdiUpdate
Description : Set up Scaler DCDi Status and Strength
Parameters  : CH, Enable, strength,
Assumptions :
Limitations :
Returns     : FVDP_Result_t type value:
                  FVDP_OK no error
*******************************************************************************/
FVDP_Result_t VScaler_Lite_DcdiUpdate(FVDP_CH_t CH, bool Enable, uint8_t Strength)
{
    /*
        VFLITE_DCDI_CTRL1 initialization: 0x3f703786
        0        VFLITE_DCDI_ENABLE       = 0x0
        1        VFLITE_EDGE_REP_EN       = 0x1
        2        VFLITE_VAR_DEP_EN        = 0x1
        7:4      VFLITE_CLAMP_REP_THRSH   = 0x8
        8        VFLITE_PRO_ANGLE_EN      = 0x1
        9        VFLITE_CS_ALG_SWITCH     = 0x1
        10       VFLITE_PS_QUAD_INTP_EN   = 0x1
        14:12    VFLITE_VAR_DEP_ORDER     = 0x3
        17:16    VFLITE_SP_COEFF_SEL      = 0x0
        20       VFLITE_PELCORR_EN        = 0x1
        21       VFLITE_MF_ALPHA_DEP_EN   = 0x1
        22       VFLITE_ANGLE_HOR_MF_EN   = 0x1
        29:24    VFLITE_ALPHA_LIMIT       = 0x3F
    */
        VFLITE_REG32_Write(CH, VFLITE_DCDI_CTRL1, 0x3f703786);

    /*
        VFLITE_DCDI_CTRL1 initialization: 0x30B
        3:0     VFLITE_DCDI_PELCORR_TH  = 0xB
        15:8    VFLITE_DCDI_VAR_THRSH   = 0x3
    */
        VFLITE_REG32_Write(CH, VFLITE_DCDI_CTRL2, 0x30B);

    /* Check the maximum DCDi Strength */
    if (Strength > VFLITE_DCDI_PELCORR_TH_MASK)
        Strength = VFLITE_DCDI_PELCORR_TH_MASK;

    /* Set up scaler DCDi streangth */
    VFLITE_REG32_ClearAndSet(CH, VFLITE_DCDI_CTRL2, VFLITE_DCDI_PELCORR_TH_MASK, VFLITE_DCDI_PELCORR_TH_OFFSET, Strength);

    /* Set scaler DCDi enable */
    VFLITE_REG32_ClearAndSet(CH, VFLITE_DCDI_CTRL1, VFLITE_DCDI_ENABLE_MASK,VFLITE_DCDI_ENABLE_OFFSET, Enable);

    return (FVDP_OK);
}

