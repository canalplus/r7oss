/***********************************************************************
 *
 * File: fvdp/mpe42/fvdp_color.c
 * Copyright (c) 2012 by STMicroelectronics. All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/


/* Includes ----------------------------------------------------------------- */

#include "fvdp_common.h"
#include "fvdp_color.h"
#include "../fvdp_vqdata.h"
#include "fvdp_hostupdate.h"

//#define CSC_DEMO_TEST_ON

/* Definitions   ------------------------------------------------------------ */

// Register COLOR_MATRIX_CTRL(0x18)
#define CCL_COLOR_MATRIX_CTRL              0x00000018 //PA POD:0x0 Event:CCL
#ifndef CCL_MATRIX_EN_MASK
#define CCL_MATRIX_EN_MASK                 0x00000001
#endif

#define CSCLIGHT_REG32_Write(Addr,Val)              REG32_Write(Addr,Val)
#define CSCLIGHT_REG32_ClearAndSet(Addr,BitsToClear,Offset,ValueToSet) \
                             REG32_ClearAndSet(Addr,BitsToClear,Offset,ValueToSet)

#define REG_OFFSET (4)

#define OFFSET_IN_1 (0x0000)
#define OFFSET_IN_2 (OFFSET_IN_1+REG_OFFSET)
#define OFFSET_IN_3 (OFFSET_IN_2+REG_OFFSET)

#define COEF_11         (OFFSET_IN_3 + REG_OFFSET)
#define COEF_12         (COEF_11 + REG_OFFSET)
#define COEF_13         (COEF_12 + REG_OFFSET)
#define COEF_21         (COEF_13 + REG_OFFSET)
#define COEF_22         (COEF_21 + REG_OFFSET)
#define COEF_23         (COEF_22 + REG_OFFSET)
#define COEF_31         (COEF_23 + REG_OFFSET)
#define COEF_32         (COEF_31 + REG_OFFSET)
#define COEF_33         (COEF_32 + REG_OFFSET)

#define OFFSET_OUT_1 (COEF_33 + REG_OFFSET)
#define OFFSET_OUT_2 (OFFSET_OUT_1+ REG_OFFSET)
#define OFFSET_OUT_3 (OFFSET_OUT_2+ REG_OFFSET)  

#define CCS_OFFSET  6
#define CCS_MASK    0x3FFF


/* Private Types ------------------------------------------------------------ */
/* Private Variables (static)------------------------------------------------ */
/* Private Function Prototypes ---------------------------------------------- */
static FVDP_Result_t CscLight_GetMatrixBaseAddress(FVDP_CH_t CH,uint32_t * MatrixBaseAddress);

/* Global Variables --------------------------------------------------------- */
/* Functions ---------------------------------------------------------------- */
/*******************************************************************************
Name        : FVDP_Result_t CscLight_Init (FVDP_CH_t CH)
Description : Initializes 3x3 matrix hardware configuration registers.
Parameters  : FVDP_CH_t CH, channel number for the hardware to init.
Assumptions :
Limitations :
Returns     : FVDP_OK if channel number was corresponding to current hardware
            : FVDP_ERROR if there is an error in selecting channel

*******************************************************************************/
FVDP_Result_t CscLight_Init (FVDP_CH_t CH)
{
    FVDP_Result_t result = FVDP_OK;
    uint32_t MatrixBaseAddress;
    result = CscLight_GetMatrixBaseAddress(CH,&MatrixBaseAddress);
    if (result != FVDP_OK)
        return (result);
    CSCLIGHT_REG32_Write(MatrixBaseAddress + CCL_CTRL,0);
    return (result);
}

/*******************************************************************************
Name        : FVDP_Result_t CscLight_Update
Description : This main function to control 3x3 matrix hardware. Based on input and output
color space it selects default conversion matrix, then it applies color control (like brightness, or contrast)
and calls function to write to hardware.


Parameters  : FVDP_CH_t CH - channel number (pip, aux or enc) to update
              FVDP_ColorSpace_t ColorSpaceIn - incoming color space
              FVDP_ColorSpace_t ColorSpaceOut - color space at the output of 3x3
              ColorMatrix_Adjuster_t * ColorControl - pointer to structure containing
                                       color information like brightness, contrast, hue..
Assumptions :
Limitations :
Returns     : FVDP_OK if channel number was corresponding to current hardware, and color space
                conversion is correct.
            : FVDP_ERROR if there is an error in some of the parameters

*******************************************************************************/
FVDP_Result_t CscLight_Update(FVDP_CH_t CH, FVDP_ColorSpace_t ColorSpaceIn,
                FVDP_ColorSpace_t ColorSpaceOut,
                ColorMatrix_Adjuster_t * ColorControl)
{
    FVDP_Result_t Error;
    uint32_t MatrixBaseAddress;

    Error = CscLight_GetMatrixBaseAddress(CH,&MatrixBaseAddress);
    if (Error != FVDP_OK)
        return (Error);

#ifdef CSC_DEMO_TEST_ON
    // default color parameters in 4.8 format
    ColorControl->Brightness = 0x100;
    ColorControl->Contrast = 0x100;
    ColorControl->Saturation = 0x100;
    ColorControl->Hue = 0x0;
    ColorControl->RedGain = 0x100;
    ColorControl->GreenGain = 0x100;
    ColorControl->BlueGain = 0x100;
    ColorControl->RedOffset = 0x10;
    ColorControl->GreenOffset = 0x10;
    ColorControl->BlueOffset = 0x10;
#endif

    Error = ColorMatrix_Update (MatrixBaseAddress + CCL_IN_OFFSET1, ColorSpaceIn, ColorSpaceOut, ColorControl);
    if (Error == FVDP_OK)
        CSCLIGHT_REG32_Write(MatrixBaseAddress + CCL_COLOR_MATRIX_CTRL , CCL_MATRIX_EN_MASK);

    HostUpdate_SetUpdate_LITE(CH, LITE_HOST_UPDATE_CCTRL_LITE, SYNC_UPDATE);

#ifdef CSC_DEMO_TEST_ON
    CSCLIGHT_REG32_Write(MatrixBaseAddress + CCL_UPDATE_CTRL,CCL_FORCE_UPDATE_MASK);
#endif
    return Error;
}
/*******************************************************************************
Name        : CscLight_Mute
Description : Changes output to blank screen or color of choice
Parameters  : FVDP_CH_t CH - channel number (pip, aux, or enc) to work on
             ColorMatrix_MuteColor_t - black or other color to blank the screen
Assumptions :
Limitations :
Returns     : void
*******************************************************************************/
void CscLight_Mute(FVDP_CH_t CH, ColorMatrix_MuteColor_t MuteColor)
{
    FVDP_Result_t Error;
    uint32_t MatrixBaseAddress;
    Error = CscLight_GetMatrixBaseAddress(CH,&MatrixBaseAddress);
    if (Error != FVDP_OK)
        return ;
    ColorMatrix_MuteControl(MatrixBaseAddress + CCL_IN_OFFSET1, MuteColor);
    HostUpdate_SetUpdate_LITE(CH, HOST_CTRL_LITE, SYNC_UPDATE);
}
/*******************************************************************************
Name        : static FVDP_Result_t CscLight_GetMatrixBaseAddress(FVDP_CH_t CH,uint32_t * MatrixBaseAddress)
Description : Helper function, returns matrix base address corresponding to given channel.

Parameters  : FVDP_CH_t CH - channel number (pip, aux, or enc) to work on
            : uint32_t * MatrixBaseAddress - base address of corresponding matrix
Assumptions :
Limitations :
Returns     : FVDP_OK if channel number was corresponding to current hardware
            : FVDP_ERROR if there is an error in selecting channel

*******************************************************************************/
static FVDP_Result_t CscLight_GetMatrixBaseAddress(FVDP_CH_t CH, uint32_t * MatrixBaseAddress)
{
    if (CH == FVDP_PIP)
    {
        *MatrixBaseAddress = CCLITE_PIP_BASE_ADDRESS;
    }
    else if (CH == FVDP_AUX)
    {
        *MatrixBaseAddress = CCLITE_AUX_BASE_ADDRESS;
    }
#if (ENC_HW_REV != REV_NONE)
    else if (CH == FVDP_ENC)
    {
        *MatrixBaseAddress = CCLITE_ENC_BASE_ADDRESS;
    }
#endif
    else
        return (FVDP_ERROR);
    return (FVDP_OK);
}

FVDP_Result_t Csc_ColorLiteFill (FVDP_CH_t CH, bool options, uint32_t color)
{
    FVDP_Result_t Error = FVDP_OK;
    uint32_t count;
    uint32_t red,green,blue;
    uint32_t MatrixBaseAddress;

    if(CH == FVDP_MAIN)
        return (FVDP_ERROR);

    Error = CscLight_GetMatrixBaseAddress(CH,&MatrixBaseAddress);

    if((options == TRUE) && (Error == FVDP_OK))
    {
       //set (CCF_S_IN_OFFSET1 ~ CCF_S_IN_OFFSET3) register to 0.
        for(count = OFFSET_IN_1; count < OFFSET_OUT_1; count += REG_OFFSET)
            CSCLIGHT_REG32_Write(MatrixBaseAddress + CCL_IN_OFFSET1 + count, 0);

        /*
            color value: ARGB = 8:8:8:8 bit
            GREEN color output: CCF_S_OUT_OFFSET1 set to 0x3fff
            BLUE  color output: CCF_S_OUT_OFFSET2 set to 0x3fff
            RED   color output: CCF_S_OUT_OFFSET3 set to 0x3fff
        */
        red   = (color & 0x00ff0000) >> 16;
        green = (color & 0x0000ff00) >> 8;
        blue  = (color & 0x000000ff);

        CSCLIGHT_REG32_Write(MatrixBaseAddress + CCL_OUT_OFFSET1, (green << CCS_OFFSET) & CCS_MASK);
        CSCLIGHT_REG32_Write(MatrixBaseAddress + CCL_OUT_OFFSET2, (blue <<  CCS_OFFSET) & CCS_MASK);
        CSCLIGHT_REG32_Write(MatrixBaseAddress + CCL_OUT_OFFSET3, (red <<  CCS_OFFSET) & CCS_MASK);
    }
    HostUpdate_SetUpdate_LITE(CH, LITE_HOST_UPDATE_CCTRL_LITE, SYNC_UPDATE);

    return (Error);
}





