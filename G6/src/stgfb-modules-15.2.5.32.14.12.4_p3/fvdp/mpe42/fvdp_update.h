/***********************************************************************
 *
 * File: fvdp/mpe42/fvdp_update.h
 * Copyright (c) 2012 STMicroelectronics Limited.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/

#ifndef FVDP_UPDATE_H
#define FVDP_UPDATE_H

/* Includes ----------------------------------------------------------------- */

//#include "fvdp_config.h"
#include "fvdp_private.h"

#if defined(__cplusplus)
extern "C" {
#endif /* __cplusplus */

/* Exported Constants ------------------------------------------------------- */

/* Exported Types ----------------------------------------------------------- */

typedef enum
{
    UPDATE_INPUT_SOURCE          = BIT0,
    UPDATE_INPUT_VIDEO_INFO      = BIT1,
    UPDATE_INPUT_RASTER_INFO     = BIT2,
    UPDATE_OUTPUT_VIDEO_INFO     = BIT3,
    UPDATE_CROP_WINDOW           = BIT4,
    UPDATE_INPUT_VIDEO_FIELDTYPE = BIT5
} UPDATE_InputUpdateFlag_t;

typedef enum
{
    UPDATE_OUTPUT_COLOR_CONTROL   = BIT0,
    UPDATE_OUTPUT_HQ_SCALER       = BIT1,
    UPDATE_OUTPUT_RASTER_INFO     = BIT2,
    UPDATE_OUTPUT_ACTIVE_WINDOW   = BIT3
} UPDATE_OutputUpdateFlag_t;


typedef enum
{
    UPDATE_INPUT_PSI  = BIT0,
    UPDATE_OUTPUT_PSI = BIT1,
} UPDATE_CommitUpdateFlag_t;

typedef enum UPDATE_PSI_VQFlag_e
{
    UPDATE_PSI_VQ_IP_CSC = BIT0,
    UPDATE_PSI_VQ_ACC = BIT1,
    UPDATE_PSI_VQ_AFM = BIT2,
    UPDATE_PSI_VQ_MADI = BIT3,
    UPDATE_PSI_VQ_MCTI = BIT4,
    UPDATE_PSI_VQ_CCS = BIT5,
    UPDATE_PSI_VQ_DNR = BIT6,
    UPDATE_PSI_VQ_MNR = BIT7,
    UPDATE_PSI_VQ_TNR = BIT8,
    UPDATE_PSI_VQ_SCALER = BIT9,
    UPDATE_PSI_VQ_SHARPNESS = BIT10,
    UPDATE_PSI_VQ_ACM = BIT11,
    UPDATE_PSI_VQ_OP_CSC = BIT12,
    UPDATE_PSI_VQ_3D = BIT12,
    CONFIG_ALL_PSI_VQ = (uint32_t)(-1L)
} UPDATE_PSIVQFeatureFlag_t;



/* Exported Variables ------------------------------------------------------- */

/* Exported Macros ---------------------------------------------------------- */

/* Exported Functions ------------------------------------------------------- */






FVDP_Result_t UPDATE_InputUpdate(FVDP_CH_Handle_t FvdpChHandle);

FVDP_Result_t UPDATE_OutputUpdate(FVDP_CH_Handle_t FvdpChHandle);

FVDP_Result_t UPDATE_CommitUpdate(FVDP_CH_Handle_t FvdpChHandle);

FVDP_Result_t UPDATE_SetInputSource(FVDP_CH_Handle_t FvdpChHandle,
                                    FVDP_Input_t     Input);

FVDP_Result_t UPDATE_SetInputVideoInfo(FVDP_CH_Handle_t  FvdpChHandle,
                                       FVDP_VideoInfo_t* NextInputVideoInfo_p);

FVDP_Result_t UPDATE_SetInputRasterInfo(FVDP_CH_Handle_t  FvdpChHandle,
                                        FVDP_RasterInfo_t InputRasterInfo);

FVDP_Result_t UPDATE_SetOutputVideoInfo(FVDP_CH_Handle_t  FvdpChHandle,
                                        FVDP_VideoInfo_t* NextOutputVideoInfo_p);

FVDP_Result_t UPDATE_SetOutputWindow(FVDP_CH_Handle_t  FvdpChHandle,
                                        FVDP_OutputWindow_t *NextOutputActiveWindow_p);

FVDP_Result_t UPDATE_SetCropWindow(FVDP_CH_Handle_t  FvdpChHandle,
                                   FVDP_CropWindow_t NextCropWindow);

FVDP_Result_t UPDATE_SetPSIState(FVDP_CH_Handle_t FvdpChHandle,
                                       FVDP_PSIFeature_t   PSIFeature,
                                       FVDP_PSIState_t     PSIState);

FVDP_Result_t UPDATE_GetPSIState(FVDP_CH_Handle_t FvdpChHandle,
                                       FVDP_PSIFeature_t   PSIFeature,
                                       FVDP_PSIState_t     *PSIState);

FVDP_Result_t UPDATE_SetLatencyType(FVDP_CH_Handle_t  FvdpChHandle,
                                        uint32_t           Value);

FVDP_Result_t UPDATE_GetLatencyType(FVDP_CH_Handle_t FvdpChHandle,
                                       uint32_t     *Value);

FVDP_Result_t UPDATE_SetPSIControl(FVDP_CH_Handle_t  FvdpChHandle,
                                 FVDP_PSIControl_t PSIControl,
                                 int32_t           Value);

FVDP_Result_t UPDATE_GetPSIControl(FVDP_CH_Handle_t  FvdpChHandle,
                                 FVDP_PSIControl_t PSIControl,
                                 int32_t*          Value);

FVDP_Result_t UPDATE_GetPSIControlsRange(FVDP_CH_Handle_t   FvdpChHandle,
                                       FVDP_PSIControl_t  PSISel,
                                       int32_t *pMin,
                                       int32_t *pMax,
                                       int32_t *pDefault,
                                       int32_t *pStep);

FVDP_Result_t UPDATE_SetPSITuningData (FVDP_CH_Handle_t FvdpChHandle,
                                     FVDP_PSIFeature_t       PSIFeature,
                                     FVDP_PSITuningData_t    *PSITuningData);

FVDP_Result_t UPDATE_GetPSITuningData(FVDP_CH_Handle_t FvdpChHandle,
                                    FVDP_PSIFeature_t       PSIFeature,
                                    FVDP_PSITuningData_t    *PSITuningData);

FVDP_Result_t UPDATE_SetOutputRasterInfo(FVDP_CH_Handle_t  FvdpChHandle,
                                        FVDP_RasterInfo_t OutputRasterInfo);


FVDP_Result_t UPDATE_SetColorFill(  FVDP_CH_Handle_t  FvdpChHandle, FVDP_CtrlColor_t Enable, uint32_t value);                                


#if defined(__cplusplus)
}
#endif /* __cplusplus */

#endif /* FVDP_UPDATE_H */

