/***********************************************************************
 *
 * File: fvdp/fvdp_config.h
 * Copyright (c) 2011 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef FVDP_CONFIG_H
#define FVDP_CONFIG_H

/* Includes ----------------------------------------------------------------- */
#include "fvdp_private.h"
#include "fvdp_vqdata.h"

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

/* Exported Constants ------------------------------------------------------- */

/* Exported Types ----------------------------------------------------------- */

/* Exported Variables ------------------------------------------------------- */

/* Exported Macros ---------------------------------------------------------- */

/* Exported Structure ------------------------------------------------------- */


typedef struct
{
    FVDP_PSIState_t ACCState;
    VQ_ACC_Parameters_t ACCVQData;
} CONFIG_ACCParameters_t;

typedef struct
{
    FVDP_PSIState_t ACMState;
    VQ_ACM_Parameters_t ACMVQData;
} CONFIG_ACMParameters_t;

typedef struct
{
    FVDP_PSIState_t AFMState;
    VQ_AFM_Parameters_t AFMVQData;
} CONFIG_AFMParameters_t;

typedef struct
{
    FVDP_PSIState_t CCSState;
    VQ_CCS_Parameters_t CCSVQData;
} CONFIG_CCSParameters_t;

typedef struct
{
    FVDP_PSIState_t IPCSCState;
    VQ_COLOR_Parameters_t IPCSCVQData;
} CONFIG_InputColorSpaceParameters_t;

typedef struct
{
    FVDP_PSIState_t OPCSCState;
    VQ_COLOR_Parameters_t OPCSCVQData;
} CONFIG_OutputColorSpaceParameters_t;

typedef struct
{
    FVDP_PSIState_t DNRState;
    VQ_DNR_Parameters_t DNRVQData;
} CONFIG_DNRParameters_t;

typedef struct
{
    FVDP_PSIState_t MNRState;
    VQ_MNR_Parameters_t MNRVQData;
} CONFIG_MNRParameters_t;

typedef struct
{
    FVDP_PSIState_t MADIState;
    VQ_MADI_Parameters_t MADIVQData;
} CONFIG_MADIParameters_t;

typedef struct
{
    FVDP_PSIState_t MCTIState;
    VQ_MCTI_Parameters_t MCTIVQData;
} CONFIG_MCTIParameters_t;

typedef struct
{
    FVDP_PSIState_t ScalerState;
    VQ_SCALER_Parameters_t ScalerVQData;
} CONFIG_ScalerParameters_t;

typedef struct
{
    FVDP_PSIState_t SharpnessState;
    VQ_SHARPNESS_Parameters_t SharpnessVQData;
} CONFIG_SharpnessParameters_t;

typedef struct
{
    FVDP_PSIState_t TNRState;
    VQ_TNR_Parameters_t TNRVQData;
} CONFIG_TNRParameters_t;

typedef struct CONFIG_PSIFeature_e
{
    CONFIG_ACCParameters_t ACCParameters;
    CONFIG_ACMParameters_t ACMParameters;
    CONFIG_AFMParameters_t AFMParameters;
    CONFIG_CCSParameters_t CCSParameters;
    CONFIG_InputColorSpaceParameters_t InputColorSpaceParameters;
    CONFIG_OutputColorSpaceParameters_t OutputColorSpaceParameters;
    CONFIG_DNRParameters_t DNRParameters;
    CONFIG_MNRParameters_t MNRParameters;
    CONFIG_MADIParameters_t MADIParameters;
    CONFIG_MCTIParameters_t MCTIParameters;
    CONFIG_ScalerParameters_t ScalerParameters;
    CONFIG_SharpnessParameters_t SharpnessParameters;
    CONFIG_TNRParameters_t TNRParameters;
} CONFIG_PSIFeature_t;

typedef struct CONFIG_PSIControlLevel_e
{
    int32_t SharpnessLevel;
    int32_t BrightnessLevel;
    int32_t ContrastLevel;
    int32_t SaturationLevel;
    int32_t HueLevel;
    int32_t RedGain;
    int32_t GreenGain;
    int32_t BlueGain;
    int32_t RedOffset;
    int32_t GreenOffset;
    int32_t BlueOffset;
    int32_t Depth_3D;
} CONFIG_PSIControlLevel_t;

typedef struct CONFIG_BlankingColor_e
{
    uint16_t Red;
    uint16_t Green;
    uint16_t Blue;
} CONFIG_BlankingColor_t;

typedef struct CONFIG_Latency_e
{
    uint8_t FrameLatency;
    uint16_t  LineLatency;
} CONFIG_LatencyConfig_t;

typedef struct CONFIG_ExternalInfo_e
{
    int32_t AnalogNoiseMeas;
    int32_t AnalogNoiseEst;
    int32_t DigitalMpegBlockHOffset;
    int32_t DigitalMpegBlockVOffset;
} CONFIG_ExternalInfo_t;

typedef enum
{
    CHANNEL_OPENED,
    CHANNEL_CLOSED,
} ChannelState_t;

typedef struct CONFIG_ChannelConfig_e
{
    ChannelState_t   ChannelSate;
    uint32_t         MemBaseAddress;
    uint32_t         MemSize;
    FVDP_ProcessingType_t   ProcessingType;
    FVDP_MaxOutputWindow_t   MaxOutputWindow;
} CONFIG_ChannelConfig_t;

typedef enum CONFIG_UpdateFlag_e
{
    CONFIG_UPDATE_INPUT_SELECT = BIT0,
    CONFIG_UPDATE_CHANNEL_STATE = BIT1,
    CONFIG_UPDATE_INPUT_VIDEO_INFO = BIT2,
    CONFIG_UPDATE_INPUT_RASTER_INFO = BIT3,
    CONFIG_UPDATE_OUTPUT_VIDEO_INFO = BIT4,
    CONFIG_UPDATE_CROP_WINDOW = BIT5,
    CONFIG_UPDATE_BW_OPTIONS = BIT6,
    CONFIG_UPDATE_BLANK_COLOR = BIT7,
    CONFIG_UPDATE_LATENCY_CONFIG = BIT8,
    CONFIG_UPDATE_EXTERNAL_INFO = BIT9,
    CONFIG_UPDATE_PSI_FEATURE_STATE = BIT10,
    CONFIG_UPDATE_PSI_FEATURE_VQDATA = BIT11,
    CONFIG_UPDATE_PSI_SHARPNESS_CONTROL_LEVEL = BIT12,
    CONFIG_UPDATE_PSI_COLOR_CONTROL_LEVEL = BIT13,
    CONFIG_UPDATE_PSI_3D_DEPTH_CONTROL_LEVEL = BIT14,
    CONFIG_UPDATE_STATE_CHANGE = BIT15,
    CONFIG_UPDATE_INPUT_VIDEO_FIELDTYPE = BIT16,

    CONFIG_COMMIT_UPDATE_ALL = 
        (uint32_t)(CONFIG_UPDATE_PSI_SHARPNESS_CONTROL_LEVEL |
        CONFIG_UPDATE_PSI_COLOR_CONTROL_LEVEL |
        CONFIG_UPDATE_PSI_3D_DEPTH_CONTROL_LEVEL|
        CONFIG_UPDATE_PSI_FEATURE_VQDATA|
        CONFIG_UPDATE_PSI_FEATURE_STATE),
    CONFIG_UPDATE_ALL= (uint32_t)(-1L)
} CONFIG_UpdateFlag_t;

typedef enum CONFIG_PSIFeatureFlag_e
{
    CONFIG_PSI_IP_CSC = BIT0,
    CONFIG_PSI_ACC = BIT1,
    CONFIG_PSI_AFM = BIT2,
    CONFIG_PSI_MADI = BIT3,
    CONFIG_PSI_MCTI = BIT4,
    CONFIG_PSI_CCS = BIT5,
    CONFIG_PSI_DNR = BIT6,
    CONFIG_PSI_MNR = BIT7,
    CONFIG_PSI_TNR = BIT8,
    CONFIG_PSI_SCALER = BIT9,
    CONFIG_PSI_SHARPNESS = BIT10,
    CONFIG_PSI_ACM = BIT11,
    CONFIG_PSI_OP_CSC = BIT12,
    CONFIG_ALL_PSI = (uint32_t)(-1L)
} CONFIG_PSIFeatureFlag_t;

typedef enum CONFIG_CRCFeatureFlag_e
{
    CONFIG_CRC_IVP = BIT0,
    CONFIG_ALL_CRC = (uint32_t)(-1L)
} CONFIG_CRCFeatureFlag_t;

typedef struct CONFIG_Params_e
{
    CONFIG_UpdateFlag_t      UpdateFlag;
    CONFIG_PSIFeatureFlag_t  VQDataUpdateFlag;
    CONFIG_ChannelConfig_t   ChannelConfig;
    FVDP_BWOptions_t         BWOptions;
    FVDP_Input_t             InputSelection;
    FVDP_VideoInfo_t         InputVideoInfo;
    FVDP_RasterInfo_t        InputRasterInfo;
    FVDP_VideoInfo_t         OutputVideoInfo;
    FVDP_CropWindow_t        CropWindow;
    FVDP_State_t             State;
    CONFIG_LatencyConfig_t   LatencyConfig;
    CONFIG_BlankingColor_t   BlankingColor;
    CONFIG_ExternalInfo_t    ExternalInfo;
    CONFIG_PSIFeature_t     *PSIFeature;
    CONFIG_PSIControlLevel_t PSIControlLevel;
    CONFIG_CRCFeatureFlag_t  CrcStateFlag;
    uint32_t                 FvdpInputLock;
    uint32_t                 FvdpOutputLock;
} CONFIG_Params_t;

#if 0
typedef struct ACTIVE_CONFIG_Params_e
{
    CONFIG_UpdateFlag_t  UpdateFlag;
    CONFIG_PSIFeatureFlag_t  VQDataUpdateFlag;
    CONFIG_ChannelConfig_t ChannelConfig;
    FVDP_BWOptions_t    BWOptions;
    FVDP_Input_t    InputSelection;
    FVDP_VideoInfo_t InputVideoInfo;
    FVDP_RasterInfo_t InputRasterInfo;
    FVDP_VideoInfo_t OutputVideoInfo;
    FVDP_CropWindow_t   CropWindow;
    FVDP_State_t State;
    CONFIG_LatencyConfig_t LatencyConfig;
    CONFIG_BlankingColor_t BlankingColor;
    CONFIG_ExternalInfo_t ExternalInfo;
    CONFIG_PSIFeature_t *PSIFeature;
    CONFIG_PSIControlLevel_t PSIControlLevel;
    CONFIG_CRCFeatureFlag_t CrcStateFlag;
} ACTIVE_CONFIG_Params_t;
#else
#define ACTIVE_CONFIG_Params_t  CONFIG_Params_t
#endif

typedef struct CrcValue_e
{
    uint32_t ivp_crc;
} CrcValue_t;

/* FVDP update caller ID information */
typedef	enum FVDP_UpdateCallerID_e
{
   UPDATE_COMMITUPDATE = BIT0,
   UPDATE_INPUTUPDATE = BIT1,
   UPDATE_OUTPUTUPDATE = BIT2,

} FVDP_UpdateCallerID_t;

#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* FVDP_TYPES_H */

