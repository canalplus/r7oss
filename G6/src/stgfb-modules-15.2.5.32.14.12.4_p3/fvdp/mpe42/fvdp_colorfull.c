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
#include "fvdp_hostupdate.h"
#include "../fvdp_vqdata.h"

//#define CSC_DEMO_TEST_ON
//#define CSC_DEBUG_MSG_ON

/* Definitions   ------------------------------------------------------------ */
#define CCTRL_BE_REG32_Write(Addr,Val)              REG32_Write(Addr+CCTRL_BE_BASE_ADDRESS,Val)
#define CCTRL_BE_REG32_Read(Addr)                   REG32_Read(Addr+CCTRL_BE_BASE_ADDRESS)
#define CCTRL_BE_REG32_Set(Addr,BitsToSet)          REG32_Set(Addr+CCTRL_BE_BASE_ADDRESS,BitsToSet)
#define CCTRL_BE_REG32_Clear(Addr,BitsToClear)      REG32_Clear(Addr+CCTRL_BE_BASE_ADDRESS,BitsToClear)
#define CCTRL_BE_REG32_ClearAndSet(Addr,BitsToClear,Offset,ValueToSet) \
                             REG32_ClearAndSet(Addr+CCTRL_BE_BASE_ADDRESS,BitsToClear,Offset,ValueToSet)


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

static bool IsMainSecondaryColorMatrixOn = FALSE;

/* Private Function Prototypes ---------------------------------------------- */

/* Global Variables --------------------------------------------------------- */

/* Functions ---------------------------------------------------------------- */
/*******************************************************************************
Name        : FVDP_Result_t CscFull_Init (FVDP_CH_t CH)
Description : Initializes 3x3 matrix hardware configuration registers.
Parameters  : FVDP_CH_t CH, channel number for the hardware to init.
Assumptions :
Limitations :
Returns     : FVDP_OK if channel number was corresponding to current hardware
            : FVDP_ERROR if there is an error in selecting channel

*******************************************************************************/
void CscFull_Init (void)
{
    uint16_t ccf_inter_coeff0_val = 160;
    uint16_t ccf_inter_coeff1_val = 980;
    uint16_t ccf_inter_coeff2_val = 18;
    uint16_t ccf_inter_coeff3_val = 1018;

    CCTRL_BE_REG32_Write(CCF_INTER_COEFF_A, ((ccf_inter_coeff1_val << CCF_INTER_COEFF_1_OFFSET) & CCF_INTER_COEFF_1_MASK)
                        | (ccf_inter_coeff0_val & CCF_INTER_COEFF_0_MASK));
    CCTRL_BE_REG32_Write(CCF_INTER_COEFF_B, ((ccf_inter_coeff3_val << CCF_INTER_COEFF_3_OFFSET) & CCF_INTER_COEFF_3_MASK)
                        | (ccf_inter_coeff2_val & CCF_INTER_COEFF_2_MASK));


    CCTRL_BE_REG32_Write(COLOR_MATRIX_CTRL, CCF_MATRIX_EN_MASK);
    Csc_SetWindowMapping(CSC_PRIMARY_COEFF_INSIDE_AND_OUTSIDE);
}

/*******************************************************************************
Name        :
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
FVDP_Result_t CscFull_Update(FVDP_ColorSpace_t ColorSpaceIn, FVDP_ColorSpace_t ColorSpaceOut, ColorMatrix_Adjuster_t * ColorControl)
{
    FVDP_Result_t Error;

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
#ifdef CSC_DEBUG_MSG_ON

    TRC( TRC_ID_MAIN_INFO, "- ColorSpace IN = [%d].   ColorSpace OUT = [%d].", ColorSpaceIn, ColorSpaceOut );
    TRC( TRC_ID_MAIN_INFO, "ColorMatrix Brightness  = 0x%X", ColorControl->Brightness );
    TRC( TRC_ID_MAIN_INFO, "ColorMatrix Contrast    = 0x%X", ColorControl->Contrast );
    TRC( TRC_ID_MAIN_INFO, "ColorMatrix Saturation  = 0x%X", ColorControl->Saturation );
    TRC( TRC_ID_MAIN_INFO, "ColorMatrix Hue         = 0x%X", ColorControl->Hue );
    TRC( TRC_ID_MAIN_INFO, "ColorMatrix RedGain     = 0x%X", ColorControl->RedGain );
    TRC( TRC_ID_MAIN_INFO, "ColorMatrix GreenGain   = 0x%X", ColorControl->GreenGain );
    TRC( TRC_ID_MAIN_INFO, "ColorMatrix BlueGain    = 0x%X", ColorControl->BlueGain );
    TRC( TRC_ID_MAIN_INFO, "ColorMatrix RedOffset   = 0x%X", ColorControl->RedOffset );
    TRC( TRC_ID_MAIN_INFO, "ColorMatrix GreenOffset = 0x%X", ColorControl->GreenOffset );
    TRC( TRC_ID_MAIN_INFO, "ColorMatrix BlueOffset  = 0x%X", ColorControl->BlueOffset );
#endif

    Error = ColorMatrix_Update (CCTRL_BE_BASE_ADDRESS + CCF_P_IN_OFFSET1, ColorSpaceIn, ColorSpaceOut, ColorControl);
    if(IsMainSecondaryColorMatrixOn==TRUE)
    {
        Error |= ColorMatrix_Update(CCTRL_BE_BASE_ADDRESS + CCF_S_IN_OFFSET1, ColorSpaceIn, ColorSpaceOut, ColorControl);
    }

    if((ColorSpaceIn ==  RGB) || (ColorSpaceIn ==  sRGB) || (ColorSpaceIn ==  RGB861))
    {
        CCTRL_BE_REG32_Set(CCF_CTRL, INTERPOLATOR_BYPASS_MASK);
    }
    else
    {
        CCTRL_BE_REG32_Clear(CCF_CTRL, INTERPOLATOR_BYPASS_MASK);
    }

#ifdef CSC_DEMO_TEST_ON
    CCTRL_BE_REG32_Write(CCF_UPDATE_CTRL, 0x1);
#endif
    HostUpdate_SetUpdate_BE(BE_HOST_UPDATE_CCTRL, SYNC_UPDATE);

    return Error;
}
/*******************************************************************************
Name        : Csc_SetWindowMapping
Description : Select CSC-to-window allocation
Parameters  : Mapping -
                          0: use secondary matrix for inside and outside of window
                          1: use primary inside and secondary outside
                          2: use secondary inside and primary outside
                          3: use primary matrix for inside and outside of window
Assumptions :
Limitations :
Returns     : FVDP_Result_t type value:
                  FVDP_OK if no error
*******************************************************************************/
void Csc_SetWindowMapping(CscMatrixToWindowMapping_t Mapping)
{
    CCTRL_BE_REG32_ClearAndSet(COLOR_MATRIX_CTRL, CCF_MATRIX_WIN_CTRL_MASK, CCF_MATRIX_WIN_CTRL_OFFSET, Mapping);

    if(Mapping == CSC_PRIMARY_COEFF_INSIDE_AND_OUTSIDE)
        IsMainSecondaryColorMatrixOn = FALSE;
    else
        IsMainSecondaryColorMatrixOn = TRUE;
}
/*******************************************************************************
Name        : CscLight_Mute
Description : Changes output to blank screen or color of choice
Parameters  : ColorMatrix_MuteColor_t - black or other color to blank the screen
Assumptions :
Limitations :
Returns     : void
*******************************************************************************/
void Csc_Mute(ColorMatrix_MuteColor_t MuteColor)
{
    ColorMatrix_MuteControl(CCTRL_BE_BASE_ADDRESS + CCF_P_IN_OFFSET1, MuteColor);
    HostUpdate_SetUpdate_BE(BE_HOST_UPDATE_CCTRL, SYNC_UPDATE);
}


FVDP_Result_t Csc_ColorFullFill (FVDP_CH_t CH, bool options, uint32_t color)
{
    uint32_t count;
    uint32_t red,green,blue;

    if(CH != FVDP_MAIN)
        return (FVDP_ERROR);

    if(options == TRUE)
    {
       //set (CCF_S_IN_OFFSET1 ~ CCF_S_IN_OFFSET3) register to 0.
        for(count = OFFSET_IN_1; count < OFFSET_OUT_1; count += REG_OFFSET)
            CCTRL_BE_REG32_Write(CCF_S_IN_OFFSET1 + count, 0);

        /*
            color value: ARGB = 8:8:8:8 bit
            GREEN color output: CCF_S_OUT_OFFSET1 set to 0x3fff
            BLUE  color output: CCF_S_OUT_OFFSET2 set to 0x3fff
            RED   color output: CCF_S_OUT_OFFSET3 set to 0x3fff
        */
        red   = (color & 0x00ff0000) >> 16;
        green = (color & 0x0000ff00) >> 8;
        blue  = (color & 0x000000ff);

        CCTRL_BE_REG32_Write(CCF_S_OUT_OFFSET1, (green << CCS_OFFSET) & CCS_MASK);
        CCTRL_BE_REG32_Write(CCF_S_OUT_OFFSET2, (blue << CCS_OFFSET) & CCS_MASK);
        CCTRL_BE_REG32_Write(CCF_S_OUT_OFFSET3, (red << CCS_OFFSET) & CCS_MASK);

        CCTRL_BE_REG32_ClearAndSet(COLOR_MATRIX_CTRL, CCF_MATRIX_WIN_CTRL_MASK, CCF_MATRIX_WIN_CTRL_OFFSET, 0);
    }
    else
    {
        CCTRL_BE_REG32_ClearAndSet(COLOR_MATRIX_CTRL, CCF_MATRIX_WIN_CTRL_MASK, CCF_MATRIX_WIN_CTRL_OFFSET, 3);
    }

    HostUpdate_SetUpdate_BE(BE_HOST_UPDATE_CCTRL, SYNC_UPDATE);

    return (FVDP_OK);
}


