/***********************************************************************
 *
 * Copyright (c) 2011 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef FVDP_SHARP_H
#define FVDP_SHARP_H

/* Includes ----------------------------------------------------------------- */

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

/* Exported Constants ------------------------------------------------------- */

/* Exported Types ----------------------------------------------------------- */
typedef enum
{
    SCALER_HORIZONTAL,
    SCALER_VERTICAL
} SHARPNESS_ScalerOrientation_t;

typedef enum
{
    SCALER_LUMA,
    SCALER_CHROMA
} SHARPNESS_ScalerChromaOrLuma;

/* Exported Variables ------------------------------------------------------- */

/* Exported Macros ---------------------------------------------------------- */

/* Exported Functions ------------------------------------------------------- */
FVDP_Result_t SHARPNESS_HQ_Init ( FVDP_CH_t  CH);
FVDP_Result_t SHARPNESS_HQ_SetState(FVDP_CH_t CH, FVDP_PSIState_t selection);
FVDP_Result_t SHARPNESS_HQ_SharpnessGain(FVDP_CH_t CH,
                                SHARPNESS_ScalerOrientation_t Orientation,
                                VQ_EnhancerGain_t * pGain);
 FVDP_Result_t SHARPNESS_HQ_EnhancerNonLinearControl(FVDP_CH_t CH,
                                SHARPNESS_ScalerOrientation_t Orientation,
                                VQ_EnhancerNonLinearControl_t * pControl);
 FVDP_Result_t SHARPNESS_HQ_EnhancerNoiseCoringControl(FVDP_CH_t CH,
                             SHARPNESS_ScalerOrientation_t Orientation,
                             VQ_EnhancerNoiseCoringControl_t * pControl);
 FVDP_Result_t SHARPNESS_HQ_EnhancerSetData(FVDP_CH_t CH,
         SHARPNESS_ScalerOrientation_t Orientation, SHARPNESS_ScalerChromaOrLuma Selection, VQ_EnhancerParameters_t * pParams);

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* FVDP_SHARP_H */

