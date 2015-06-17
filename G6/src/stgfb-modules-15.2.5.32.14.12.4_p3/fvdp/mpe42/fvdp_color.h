/***********************************************************************
 *
 * File: fvdp/mpe42/fvdp_color.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef FVDP_COLOR_H
#define FVDP_COLOR_H

/* Includes ----------------------------------------------------------------- */

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

#include "fvdp_router.h"

/* Exported Constants ------------------------------------------------------- */
/* Exported Types ----------------------------------------------------------- */
typedef enum
{
    MC_GREEN,
    MC_BLUE,
    MC_RED,
    MC_BLACK,
    MC_NONE
} ColorMatrix_MuteColor_t;

typedef struct ColorMatrix_Adjuster_s
{
     int16_t        Brightness;
    uint16_t        Contrast;
    uint16_t        Saturation;
     int16_t        Hue;
    uint16_t        RedGain;
    uint16_t        GreenGain;
    uint16_t        BlueGain;
     int16_t        RedOffset;
     int16_t        GreenOffset;
     int16_t        BlueOffset;
}ColorMatrix_Adjuster_t;

/*Window definitin, where the 3x3 should be applied */
typedef enum
{
    CSC_SECONDARY_COEFF_INSIDE_AND_OUTSIDE = 0,
    CSC_PRIMARY_COEFF_INSIDE_SECONDARY_OUTSIDE,
    CSC_SECONDARY_COEFF_INSIDE_PRIMARY_OUTSIDE,
    CSC_PRIMARY_COEFF_INSIDE_AND_OUTSIDE,
} CscMatrixToWindowMapping_t;


/* Exported Variables ------------------------------------------------------- */

/* Exported Macros ---------------------------------------------------------- */

/* Exported Functions ------------------------------------------------------- */
FVDP_Result_t ColorMatrix_MuteControl (uint32_t ColorMatrix_Offset1BaseAddress, ColorMatrix_MuteColor_t color);
FVDP_Result_t ColorMatrix_GetColorRange(FVDP_PSIControl_t  ColorControlSel, int32_t *pMin, int32_t *pMax, int32_t *pStep);
FVDP_Result_t ColorMatrix_Update (uint32_t ColorMatrix_Offset1BaseAddress, FVDP_ColorSpace_t ColorSpaceIn, FVDP_ColorSpace_t ColorSpaceOutput , ColorMatrix_Adjuster_t * ColorControl);

FVDP_Result_t CscFull_Update(FVDP_ColorSpace_t ColorSpaceIn, FVDP_ColorSpace_t ColorSpaceOut, ColorMatrix_Adjuster_t * ColorControl);
void CscFull_Init (void);
void Csc_SetWindowMapping(CscMatrixToWindowMapping_t Mapping);
void Csc_Mute(ColorMatrix_MuteColor_t MuteColor);
void CscLight_Mute(FVDP_CH_t CH, ColorMatrix_MuteColor_t MuteColor);
FVDP_Result_t CscLight_Init (FVDP_CH_t CH);
FVDP_Result_t CscLight_Update(FVDP_CH_t CH, FVDP_ColorSpace_t ColorSpaceIn, FVDP_ColorSpace_t ColorSpaceOut,ColorMatrix_Adjuster_t * ColorControl);
FVDP_Result_t Csc_ColorFullFill (FVDP_CH_t CH, bool options, uint32_t color);
FVDP_Result_t Csc_ColorLiteFill (FVDP_CH_t CH, bool options, uint32_t color);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* FVDP_COLOR_H */

