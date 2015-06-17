/***********************************************************************
 *
 * File: fvdp/mpe42/fvdp_hqscaler.h
 * Copyright (c) 2012 by STMicroelectronics. All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

/* Private Variables (static)------------------------------------------------ */
typedef struct FVDP_HQScalerInfo_s
{
    uint16_t                HWidth;
    uint16_t                VHeight;
    FVDP_ScanType_t         ScanType;
    FVDP_FieldType_t        FieldType;
    FVDP_ColorSampling_t    ColorSampling;
    FVDP_ColorSpace_t       ColorSpace;
} FVDP_HQScalerInfo_t;

typedef struct FVDP_HQScalerConfig_s
{
    FVDP_HQScalerInfo_t   InputFormat;
    FVDP_HQScalerInfo_t   OutputFormat;
    bool                            ProcessingRGB;
} FVDP_HQScalerConfig_t;


/* Private Function Prototypes ---------------------------------------------- */
FVDP_Result_t HFHQScaler_HWConfig(FVDP_CH_t CH, FVDP_HQScalerConfig_t HQScalerInfo);
FVDP_Result_t VFHQScaler_HWConfig(FVDP_CH_t CH, FVDP_HQScalerConfig_t HQScalerInfo, FVDP_ProcessingType_t ProcessingType);
FVDP_Result_t HQScaler_DcdiUpdate (FVDP_CH_t CH, bool Enable, uint8_t Strength);
FVDP_Result_t HQScaler_Init(void);
