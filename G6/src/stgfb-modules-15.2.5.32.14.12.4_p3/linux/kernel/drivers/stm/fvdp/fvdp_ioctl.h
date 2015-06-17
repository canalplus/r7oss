/***********************************************************************
 *
 * File: linux/kernel/drivers/stm/fvdp/fvdp_ioctl.h
 * Copyright (c) 2012 by STMicroelectronics. All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/


/* Define to prevent recursive inclusion */
#ifndef __FVDP_IOCTL_H
#define __FVDP_IOCTL_H

#include <linux/ioctl.h>
#include <linux/stddef.h>
#include <stdbool.h> // so fvdp.h does not complain about bool

#include <fvdp/fvdp.h>

#define FVDPIO_MAGIC 'F'

/* FVDP MISC IOCTLS */
#define FVDPIO_READ_REG                 _IO(FVDPIO_MAGIC, 0)
#define FVDPIO_WRITE_REG                _IO(FVDPIO_MAGIC, 1)
#define FVDPIO_GET_DATA_INFO            _IO(FVDPIO_MAGIC, 2)
#define FVDPIO_COPY_DATA_TO_USER        _IO(FVDPIO_MAGIC, 3)

/* FVDP API IOCTLS */
#define FVDPIO_INIT_DRIVER              _IO(FVDPIO_MAGIC, 4)
#define FVDPIO_GET_REQ_VID_MEM_SIZE     _IO(FVDPIO_MAGIC, 5)
#define FVDPIO_OPEN_CHANNEL             _IO(FVDPIO_MAGIC, 6)
#define FVDPIO_CLOSE_CHANNEL            _IO(FVDPIO_MAGIC, 7)
#define FVDPIO_CONNECT_INPUT            _IO(FVDPIO_MAGIC, 8)
#define FVDPIO_DISCONNECT_INPUT         _IO(FVDPIO_MAGIC, 9)
#define FVDPIO_SET_INPUT_VIDEO_INFO     _IO(FVDPIO_MAGIC, 10)
#define FVDPIO_GET_INPUT_VIDEO_INFO     _IO(FVDPIO_MAGIC, 11)
#define FVDPIO_SET_OUTPUT_VIDEO_INFO    _IO(FVDPIO_MAGIC, 12)
#define FVDPIO_GET_OUTPUT_VIDEO_INFO    _IO(FVDPIO_MAGIC, 13)
#define FVDPIO_SET_INPUT_RASTER_INFO    _IO(FVDPIO_MAGIC, 14)
#define FVDPIO_GET_INPUT_RASTER_INFO    _IO(FVDPIO_MAGIC, 15)
#define FVDPIO_SET_STATE                _IO(FVDPIO_MAGIC, 16)
#define FVDPIO_GET_STATE                _IO(FVDPIO_MAGIC, 17)
#define FVDPIO_SET_LATENCY              _IO(FVDPIO_MAGIC, 18)
#define FVDPIO_GET_LATENCY              _IO(FVDPIO_MAGIC, 19)
#define FVDPIO_SET_BW_OPTIONS           _IO(FVDPIO_MAGIC, 20)
#define FVDPIO_GET_BW_OPTIONS           _IO(FVDPIO_MAGIC, 21)
#define FVDPIO_SET_CROP_WINDOW          _IO(FVDPIO_MAGIC, 22)
#define FVDPIO_GET_CROP_WINDOW          _IO(FVDPIO_MAGIC, 23)
#define FVDPIO_SET_BLANKING_COLOR       _IO(FVDPIO_MAGIC, 24)
#define FVDPIO_GET_BLANKING_COLOR       _IO(FVDPIO_MAGIC, 25)
#define FVDPIO_SET_PSI_CONTROL          _IO(FVDPIO_MAGIC, 26)
#define FVDPIO_GET_PSI_CONTROL          _IO(FVDPIO_MAGIC, 27)
#define FVDPIO_SET_PSI_STATE            _IO(FVDPIO_MAGIC, 28)
#define FVDPIO_GET_PSI_STATE            _IO(FVDPIO_MAGIC, 29)
#define FVDPIO_SET_PSI_TUNING_DATA      _IO(FVDPIO_MAGIC, 30)
#define FVDPIO_GET_PSI_TUNING_DATA      _IO(FVDPIO_MAGIC, 31)
#define FVDPIO_GET_VERSION              _IO(FVDPIO_MAGIC, 32)
#define FVDPIO_GET_PSI_CONTROLS_RANGE   _IO(FVDPIO_MAGIC, 33)
#define FVDPIO_SET_EXTERNAL_INFO        _IO(FVDPIO_MAGIC, 34)
#define FVDPIO_GET_EXTERNAL_INFO        _IO(FVDPIO_MAGIC, 35)
#define FVDPIO_INPUT_UPDATE             _IO(FVDPIO_MAGIC, 36)
#define FVDPIO_OUTPUT_UPDATE            _IO(FVDPIO_MAGIC, 37)
#define FVDPIO_COMMIT_UPDATE            _IO(FVDPIO_MAGIC, 38)
#define FVDPIO_SET_BUFFER_CALLBACK      _IO(FVDPIO_MAGIC, 39)
#define FVDPIO_GET_BUFFER_CALLBACK      _IO(FVDPIO_MAGIC, 40)
#define FVDPIO_QUEUE_BUFFER             _IO(FVDPIO_MAGIC, 41)
#define FVDPIO_QUEUE_GET_STATUS         _IO(FVDPIO_MAGIC, 42)
#define FVDPIO_QUEUE_FLUSH              _IO(FVDPIO_MAGIC, 43)
#define FVDPIO_SET_CRC_STATE            _IO(FVDPIO_MAGIC, 44)
#define FVDPIO_GET_CRC_INFO             _IO(FVDPIO_MAGIC, 45)
#define FVDPIO_SET_OUTPUT_RASTER_INFO   _IO(FVDPIO_MAGIC, 46)
#define FVDPIO_SET_PROCESSING           _IO(FVDPIO_MAGIC, 47)
#define FVDPIO_APPTEST                  _IOWR(FVDPIO_MAGIC, 100, FVDPIO_Apptest_t*)

typedef struct
{
    uint32_t address;
    uint32_t value;
}FVDPIO_Reg_t;

typedef enum
{
    FVDPIO_TYPE_VCD_LOG,
    OTHER_TYPES // temporary
}FVDPIO_DataType_t;

typedef struct
{
    FVDPIO_DataType_t type;
    void*       uaddr;
    void*       kaddr;
    uint32_t    action_id;
    uint32_t    size;
    uint32_t    param0;
    uint32_t    param1;
}FVDPIO_Data_t;

typedef enum
{
    APPTEST_VCDLOG_GET_CONFIG,
    APPTEST_VCDLOG_SET_SOURCE,
    APPTEST_VCDLOG_SET_MODE,
    APPTEST_REGLOG_GET_CONFIG,
    APPTEST_REGLOG_SET_CONFIG,
    APPTEST_REGLOG_START_CAPTURE,
    APPTEST_REGLOG_STOP_CAPTURE,
    APPTEST_REGLOG_GET_LOG_DATA
}ApptestFunc_t;

typedef struct
{
    ApptestFunc_t   action_id;
    uint32_t        param[4];
}FVDPIO_Apptest_t;

typedef struct
{
    /* Result of FVDP API function */
    FVDP_Result_t Result;

    FVDP_Init_t InitParams;
}FVDPIO_InitDriver_t;

typedef struct
{
    /* Result of FVDP API function */
    FVDP_Result_t Result;

    FVDP_CH_t      CH;
    FVDP_Context_t Context;
    uint32_t      MemSize;
}FVDPIO_GetRequiredVideoMemorySize_t;

typedef struct
{
    /* Result of FVDP API function */
    FVDP_Result_t Result;

    /* Parameters to FVDP API function */
    FVDP_CH_Handle_t FvdpChHandle;
    FVDP_CH_t CH;
    uint32_t MemBaseAddress;
    uint32_t MemSize;
    FVDP_Context_t Context;
}FVDPIO_OpenChannel_t;

typedef struct
{
    /* Result of FVDP API function */
    FVDP_Result_t Result;

    /* Parameters to FVDP API function */
    FVDP_CH_Handle_t  FvdpChHandle;
}FVDPIO_CloseChannel_t;

typedef struct
{
    /* Result of FVDP API function */
    FVDP_Result_t Result;

    /* Parameters to FVDP API function */
    FVDP_CH_Handle_t    FvdpChHandle;
    FVDP_Input_t        Input;
}FVDPIO_ConnectInput_t;

typedef struct
{
    /* Result of FVDP API function */
    FVDP_Result_t Result;

    /* Parameters to FVDP API function */
    FVDP_CH_Handle_t FvdpChHandle;
}FVDPIO_DisconnectInput_t;

typedef struct
{
    /* Result of FVDP API function */
    FVDP_Result_t Result;

    /* Parameters to FVDP API function */
    FVDP_CH_Handle_t  FvdpChHandle;
    FVDP_VideoInfo_t  InputVideoInfo;
}FVDPIO_SetInputVideoInfo_t;

typedef struct
{
    /* Result of FVDP API function */
    FVDP_Result_t Result;

    /* Parameters to FVDP API function */
    FVDP_CH_Handle_t  FvdpChHandle;
    FVDP_VideoInfo_t InputVideoInfo;
}FVDPIO_GetInputVideoInfo_t;

typedef struct
{
    /* Result of FVDP API function */
    FVDP_Result_t Result;

    /* Parameters to FVDP API function */
    FVDP_CH_Handle_t FvdpChHandle;
    FVDP_VideoInfo_t OutputVideoInfo;
}FVDPIO_SetOutputVideoInfo_t;

typedef struct
{
    /* Result of FVDP API function */
    FVDP_Result_t Result;

    /* Parameters to FVDP API function */
    FVDP_CH_Handle_t  FvdpChHandle;
    FVDP_VideoInfo_t OutputVideoInfo;
}FVDPIO_GetOutputVideoInfo_t;

typedef struct
{
    /* Result of FVDP API function */
    FVDP_Result_t Result;

    /* Parameters to FVDP API function */
    FVDP_CH_Handle_t  FvdpChHandle;
    FVDP_RasterInfo_t InputRasterInfo;
}FVDPIO_SetInputRasterInfo_t;

typedef struct
{
    /* Result of FVDP API function */
    FVDP_Result_t Result;

    /* Parameters to FVDP API function */
    FVDP_CH_Handle_t   FvdpChHandle;
    FVDP_RasterInfo_t InputRasterInfo;
}FVDPIO_GetInputRasterInfo_t;

typedef struct
{
    /* Result of FVDP API function */
    FVDP_Result_t Result;

    /* Parameters to FVDP API function */
    FVDP_CH_Handle_t FvdpChHandle;
    FVDP_State_t     State;
}FVDPIO_SetState_t;

typedef struct
{
    /* Result of FVDP API function */
    FVDP_Result_t Result;

    /* Parameters to FVDP API function */
    FVDP_CH_Handle_t FvdpChHandle;
    FVDP_State_t    State;
}FVDPIO_GetState_t;


typedef struct
{
    /* Result of FVDP API function */
    FVDP_Result_t Result;

    /* Parameters to FVDP API function */
    FVDP_CH_Handle_t FvdpChHandle;
    uint32_t          ProcessingType;
}FVDPIO_SetProcessing_t;


typedef struct
{
    /* Result of FVDP API function */
    FVDP_Result_t Result;

    /* Parameters to FVDP API function */
    FVDP_CH_Handle_t FvdpChHandle;
    uint32_t          LatencyType;
}FVDPIO_SetLatencyType_t;

typedef struct
{
    /* Result of FVDP API function */
    FVDP_Result_t Result;

    /* Parameters to FVDP API function */
    FVDP_CH_Handle_t FvdpChHandle;
    uint32_t          LatencyType;
}FVDPIO_GetLatencyType_t;


typedef struct
{
    /* Result of FVDP API function */
    FVDP_Result_t Result;

    /* Parameters to FVDP API function */
    FVDP_CH_Handle_t FvdpChHandle;
    uint8_t          FrameLatency;
    uint16_t         LineLatency;
}FVDPIO_SetLatency_t;

typedef struct
{
    /* Result of FVDP API function */
    FVDP_Result_t Result;

    /* Parameters to FVDP API function */
    FVDP_CH_Handle_t FvdpChHandle;
    uint8_t         FrameLatency;
    uint16_t        LineLatency;
}FVDPIO_GetLatency_t;

typedef struct
{
    /* Result of FVDP API function */
    FVDP_Result_t Result;

    /* Parameters to FVDP API function */
    FVDP_CH_Handle_t  FvdpChHandle;
    FVDP_BWOptions_t  BWOptions;
}FVDPIO_SetBWOptions_t;

typedef struct
{
    /* Result of FVDP API function */
    FVDP_Result_t Result;

    /* Parameters to FVDP API function */
    FVDP_CH_Handle_t  FvdpChHandle;
    FVDP_BWOptions_t BWOptions;
}FVDPIO_GetBWOptions_t;

typedef struct
{
    /* Result of FVDP API function */
    FVDP_Result_t Result;

    /* Parameters to FVDP API function */
    FVDP_CH_Handle_t   FvdpChHandle;
    FVDP_CropWindow_t  CropWindow;
}FVDPIO_SetCropWindow_t;

typedef struct
{
    /* Result of FVDP API function */
    FVDP_Result_t Result;

    /* Parameters to FVDP API function */
    FVDP_CH_Handle_t   FvdpChHandle;
    FVDP_CropWindow_t CropWindow;
}FVDPIO_GetCropWindow_t;

typedef struct
{
    /* Result of FVDP API function */
    FVDP_Result_t Result;

    /* Parameters to FVDP API function */
    FVDP_CH_Handle_t FvdpChHandle;
    uint16_t         Red;
    uint16_t         Green;
    uint16_t         Blue;
}FVDPIO_SetBlankingColor_t;

typedef struct
{
    /* Result of FVDP API function */
    FVDP_Result_t Result;

    /* Parameters to FVDP API function */
    FVDP_CH_Handle_t FvdpChHandle;
    uint16_t        Red;
    uint16_t        Green;
    uint16_t        Blue;
}FVDPIO_GetBlankingColor_t;

typedef struct
{
    /* Result of FVDP API function */
    FVDP_Result_t Result;

    /* Parameters to FVDP API function */
    FVDP_CH_Handle_t  FvdpChHandle;
    FVDP_PSIControl_t PSIControl;
    int32_t           Value;
}FVDPIO_SetPSIControl_t;

typedef struct
{
    /* Result of FVDP API function */
    FVDP_Result_t Result;

    /* Parameters to FVDP API function */
    FVDP_CH_Handle_t  FvdpChHandle;
    FVDP_PSIControl_t PSIControl;
    int32_t          Value;
}FVDPIO_GetPSIControl_t;

typedef struct
{
    /* Result of FVDP API function */
    FVDP_Result_t Result;

    /* Parameters to FVDP API function */
    FVDP_CH_Handle_t    FvdpChHandle;
    FVDP_PSIFeature_t   PSIFeature;
    FVDP_PSIState_t     PSIState;
}FVDPIO_SetPSIState_t;

typedef struct
{
    /* Result of FVDP API function */
    FVDP_Result_t Result;

    /* Parameters to FVDP API function */
    FVDP_CH_Handle_t    FvdpChHandle;
    FVDP_PSIFeature_t   PSIFeature;
    FVDP_PSIState_t    PSIState;
}FVDPIO_GetPSIState_t;

typedef struct
{
    /* Result of FVDP API function */
    FVDP_Result_t Result;

    /* Parameters to FVDP API function */
    FVDP_CH_Handle_t      FvdpChHandle;
    FVDP_PSIFeature_t     PSIFeature;
    FVDP_PSITuningData_t PSITuningData;
}FVDPIO_SetPSITuningData_t;

typedef struct
{
    /* Result of FVDP API function */
    FVDP_Result_t Result;

    /* Parameters to FVDP API function */
    FVDP_CH_Handle_t       FvdpChHandle;
    FVDP_PSIFeature_t      PSIFeature;
    FVDP_PSITuningData_t  PSITuningData;
}FVDPIO_GetPSITuningData_t;

typedef struct
{
    /* Result of FVDP API function */
    FVDP_Result_t Result;

    /* Parameters to FVDP API function */
    FVDP_Version_t Version;
}FVDPIO_GetVersion_t;

typedef struct
{
    /* Result of FVDP API function */
    FVDP_Result_t Result;

    /* Parameters to FVDP API function */
    FVDP_CH_Handle_t  FvdpChHandle;
    FVDP_PSIControl_t PSIControlsSelect;
    int32_t          MinValue;
    int32_t          MaxValue;
    int32_t          DefaultValue;
    int32_t          StepValue;
}FVDPIO_GetPSIControlsRange_t;

typedef struct
{
    /* Result of FVDP API function */
    FVDP_Result_t Result;

    /* Parameters to FVDP API function */
    FVDP_CH_Handle_t       FvdpChHandle;
    FVDP_ExternalInfo_t    ExternalInfo;
    int32_t                Value;
}FVDPIO_SetExternalInfo_t;

typedef struct
{
    /* Result of FVDP API function */
    FVDP_Result_t Result;

    /* Parameters to FVDP API function */
    FVDP_CH_Handle_t       FvdpChHandle;
    FVDP_ExternalInfo_t    ExternalInfo;
    int32_t               Value;
}FVDPIO_GetExternalInfo_t;

typedef struct
{
    /* Result of FVDP API function */
    FVDP_Result_t Result;

    /* Parameters to FVDP API function */
    FVDP_CH_Handle_t   FvdpChHandle;
}FVDPIO_InputUpdate_t;

typedef struct
{
    /* Result of FVDP API function */
    FVDP_Result_t Result;

    /* Parameters to FVDP API function */
    FVDP_CH_Handle_t  FvdpChHandle;
}FVDPIO_OutputUpdate_t;

typedef struct
{
    /* Result of FVDP API function */
    FVDP_Result_t Result;

    /* Parameters to FVDP API function */
    FVDP_CH_Handle_t  FvdpChHandle;
}FVDPIO_CommitUpdate_t;

typedef struct
{
    /* Result of FVDP API function */
    FVDP_Result_t Result;

    /* Parameters to FVDP API function */
    FVDP_CH_Handle_t FvdpChHandle;
    uint32_t AddrScalingCompleted; /* Address of callbacks when the scaling task is completed */
    uint32_t AddrBufferDone;
}FVDPIO_SetBufferCallback_t;

typedef struct
{
    /* Result of FVDP API function */
    FVDP_Result_t Result;

    /* Parameters to FVDP API function */
    FVDP_CH_Handle_t FvdpChHandle;
    uint32_t AddrScalingCompleted; /* Address of callbacks when the scaling task is completed */
    uint32_t AddrBufferDone;
}FVDPIO_GetBufferCallback_t;

typedef struct
{
    /* Result of FVDP API function */
    FVDP_Result_t Result;

    /* Parameters to FVDP API function */
    FVDP_CH_Handle_t  FvdpChHandle;
    uint32_t          FrameID;
    uint32_t          SrcAddr1;
    uint32_t          SrcAddr2;
    uint32_t          DstAddr1;
    uint32_t          DstAddr2;
}FVDPIO_QueueBuffer_t;

typedef struct
{
    /* Result of FVDP API function */
    FVDP_Result_t Result;

    /* Parameters to FVDP API function */
    FVDP_CH_Handle_t    FvdpChHandle;
    FVDP_QueueStatus_t QueueStatus;
}FVDPIO_QueueGetStatus_t;

typedef struct
{
    /* Result of FVDP API function */
    FVDP_Result_t Result;

    /* Parameters to FVDP API function */
    FVDP_CH_Handle_t   FvdpChHandle;
    bool               bFlushCurrentNode;
}FVDPIO_QueueFlush_t;

typedef struct
{
    /* Result of FVDP API function */
    FVDP_Result_t Result;

    /* Parameters to FVDP API function */
    FVDP_CH_Handle_t  FvdpChHandle;
    FVDP_CrcFeature_t Feature;
    bool              State;
}FVDPIO_SetCrcState_t;

typedef struct
{
    /* Result of FVDP API function */
    FVDP_Result_t Result;

    /* Parameters to FVDP API function */
    FVDP_CH_Handle_t   FvdpChHandle;
    FVDP_CrcFeature_t  Feature;
    FVDP_CrcInfo_t    Info;
}FVDPIO_GetCrcInfo_t;

typedef struct
{
    /* Result of FVDP API function */
    FVDP_Result_t Result;

    /* Parameters to FVDP API function */
    FVDP_CH_Handle_t  FvdpChHandle;
    FVDP_RasterInfo_t OutputRasterInfo;
}FVDPIO_SetOutputRasterInfo_t;
#endif /* #ifndef __FVDP_IOCTL_H */

/* End of fvdp_ioctl.h */

