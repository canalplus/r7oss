/***********************************************************************
 *
 * File: fvdp/mpe42/fvdp_scalerlite.h
 * Copyright (c) 2012 by STMicroelectronics. All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

/* Private Variables (static)------------------------------------------------ */
typedef struct FVDP_ScalerVideoInfo_s
{
    uint16_t                HWidth;
    uint16_t                VHeight;
    FVDP_ScanType_t         ScanType;
    FVDP_FieldType_t        FieldType;
    FVDP_ColorSampling_t    ColorSampling;
    FVDP_ColorSpace_t       ColorSpace;
} FVDP_ScalerVideoInfo_t;

typedef struct FVDP_ScalerLiteConfig_s
{
    FVDP_ScalerVideoInfo_t   InputFormat;
    FVDP_ScalerVideoInfo_t   OutputFormat;
    DATAPATH_LITE_t           CheckDatapathLite;
    bool                            ProcessingRGB;
} FVDP_ScalerLiteConfig_t;

/* Private Function Prototypes ---------------------------------------------- */
FVDP_Result_t HScaler_Lite_Init(FVDP_CH_t CH);
FVDP_Result_t HScaler_Lite_HWConfig(FVDP_CH_t CH, FVDP_ScalerLiteConfig_t Scaler_VideoInfo);
FVDP_Result_t VScaler_Lite_Init(FVDP_CH_t CH);
FVDP_Result_t VScaler_Lite_HWConfig(FVDP_CH_t CH, FVDP_ScalerLiteConfig_t Scaler_VideoInfo);
FVDP_Result_t VScaler_Lite_OffsetUpdate(FVDP_CH_t CH, FVDP_ScalerLiteConfig_t Scaler_VideoInfo);
FVDP_Result_t VScaler_Lite_DcdiUpdate(FVDP_CH_t CH, bool Enable, uint8_t Strength);
