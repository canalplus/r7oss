/***********************************************************************
 *
 * File: fvdp/fvdp.h
 * Copyright (c) 2011 by STMicroelectronics. All rights reserved.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
\***********************************************************************/


/* Define to prevent recursive inclusion */
#ifndef __FVDP_H
#define __FVDP_H

/* Includes ----------------------------------------------------------------- */
#include "fvdp_hw_rev.h"

/* C++ support */
#ifdef __cplusplus
extern "C" {
#endif

#define FVDP_DRIVER_ID      0x01    // TBD, 0x01 was arbitrary picked
#define FVDP_DRIVER_BASE   (FVDP_DRIVER_ID << 16)


/* Handle to an FVDP Channel */
typedef uint32_t FVDP_CH_Handle_t;


/* FVDP return codes */
typedef enum FVDP_Result_e
{
    FVDP_OK                 = FVDP_DRIVER_BASE,
    FVDP_ERROR,
    FVDP_MEM_INIT_ERROR,
    FVDP_PROC_TYPE_ERROR,
    FVDP_VIDEO_INFO_ERROR,
    FVDP_INFO_NOT_AVAILABLE,
    FVDP_LATENCY_TOO_HIGH_ERROR,
    FVDP_LATENCY_TOO_LOW_ERROR,
    FVDP_LATENCY_CAUSE_DEGRADATION_WARNING,
    FVDP_INVALID_MEM_ADDR,
    FVDP_INVALID_MEM_FORMAT,
    FVDP_REPEATED_GRAB,
    FVDP_QUEUE_OVERFLOW_ERROR,
    FVDP_QUEUE_UNDERFLOW_ERROR,
    FVDP_NOTHING_TO_RELEASE,
    FVDP_FEATURE_NOT_SUPPORTED
} FVDP_Result_t;

/* Physical video processing channel */
typedef enum FVDP_CH_e
{
    FVDP_MAIN,
    FVDP_PIP,
    FVDP_AUX,
#if (ENC_HW_REV != REV_NONE)
    FVDP_ENC,
#endif
    NUM_FVDP_CH
} FVDP_CH_t;




/* Latency type  */
typedef enum FVDP_LatencyType_e
{
    FVDP_MINIMUM_LATENCY,       // Lowest latency : DEI is off event for interlaced. [1:2]o
    FVDP_FE_ONLY_LATENCY,       // Trade between Quality and latency : DEI is on. 2i + [1:2]o
    FVDP_CONSTANT_LATENCY,      // Latency is constant 3i + [1:2]o
    NUM_LATENCY_TYPES
} FVDP_LatencyType_t;


/* Processing type on data through a specific channel */
typedef enum FVDP_ProcessingType_e
{
    FVDP_SPATIAL,               // Spatial only processing
    FVDP_TEMPORAL,              // Temporal only, MCTi permanent power down
    FVDP_TEMPORAL_FRC,          // Temporal and/or MCTi support
    FVDP_RGB_GRAPHIC,          // RGB spatial only processing
    NUM_PROCESSING_TYPES
} FVDP_ProcessingType_t;

/* Possible input sources that may be connected to FVDP */
typedef enum FVDP_Input_e
{
    FVDP_MEM_INPUT_SINGLE_BUFF, // Buffer Queue, RGB in same buffer
    FVDP_MEM_INPUT_DUAL_BUFF,   // Buffer Queue, Y & UV in separate buffers
    FVDP_VMUX_INPUT1,           // Physical connection to FVDP VMUX
    FVDP_VMUX_INPUT2,           // Physical connection to FVDP VMUX
    FVDP_VMUX_INPUT3,           // Physical connection to FVDP VMUX
    FVDP_VMUX_INPUT4,           // Physical connection to FVDP VMUX
    FVDP_VMUX_INPUT5,           // Physical connection to FVDP VMUX
    FVDP_VMUX_INPUT6,           // Physical connection to FVDP VMUX
    FVDP_VMUX_INPUT7            // Physical connection to FVDP VMUX
} FVDP_Input_t;

/* Data Source Scan Type */
typedef	enum
{
    INTERLACED,
    PROGRESSIVE
} FVDP_ScanType_t;

/* Type of field */
typedef	enum
{
    AUTO_FIELD_TYPE,            // Slave to HW generated field type
    TOP_FIELD,                  // Top (or ODD) field
    BOTTOM_FIELD                // Bottom (or EVEN) field
} FVDP_FieldType_t;

/* Color Space */
typedef	enum
{
    CUSTOM,                     // customized through Set_VSQlgoParams()
    RGB,                        // Full range RGB color space
    sRGB,                       // Full range RGB with Gamma: 2.2, Color temperature:6500K,
    sdYUV,                      // YUV based on ITU-R BT.601
    hdYUV,                      // YUV based on ITU-R BT.709
    xvYUV,                      // Extended YUV based on IEC 61966-2-4
    RGB861                      // Reduced range RGB color space
} FVDP_ColorSpace_t;

/* Chroma Sampling Format */
typedef	enum
{
    FVDP_444,                   // RGB 4:4:4
    FVDP_422,                   // YUV 4:2:2
    FVDP_420                    // YUV 4:2:0
} FVDP_ColorSampling_t;

/* FVDP supported 3D Formats */
typedef	enum
{
    FVDP_2D,                    // non-3D format
    FVDP_3D_FRAME_SEQ,          // Left and Right view are spearated by VSync
    FVDP_3D_TOP_BOTTOM,         // Top and Bottom format
    FVDP_3D_SIDE_BY_SIDE,       // Side-by-side, 3DFlag indicates sampling type
    FVDP_3D_LINE_INTERLEAVE,    // Line Interleaved, 3DFlag indicates 1st line left/right
    FVDP_3D_COLUMN_INTERLEAVE,  // Column Interleaved, 3DFlag indicates 1st column left/right
    FVDP_3D_CHECKERBOARD        // Checkerboard pattern, 3DFlag indicates 1st pixel
} FVDP_3DFormat_t;


/* Flags indicates details of 3D format */
typedef enum
{
    FVDP_AUTO3DFLAG,            // Salve to HW flag
    FVDP_LEFT,                  // Current frame is Frame Sequential Left
    FVDP_RIGHT,                 // Current frame is Frame Sequential Right
    FVDP_LEFTRIGHT,             // Left before right, SBS:left is on left,
                                // T&B:left on top, LI:left is first line,
                                // CI:left is first column,
                                // Checkerboard: left is 1st pixel
    FVDP_RIGHTLEFT,             // Right before left, SBS:right is on left,
                                // T&B:right on top, LI:right is first line,
                                // CI:right is first column,
                                // Checkerboard: right is 1st pixel
    FVDP_LLRR,                  // Left starts on first and second line
    FVDP_RRLL                   // Right starts on first and second line
} FVDP_3DFlag_t;


/* Subsampled mode for 3D Format with Half Resolution */
typedef enum
{
    FVDP_3D_NO_SUBSAMPLE,
    FVDP_3D_OLOR,
    FVDP_3D_OLER,
    FVDP_3D_ELER,
    FVDP_3D_ELOR
} FVDP_3DSubsampleMode_t;


/* Aspect Ratio, also provide information about the lcoation of video window
   with repect to active window
*/
typedef enum
{
    FVDP_ASPECT_RATIO_UNKNOWN,
    FVDP_ASPECT_RATIO_4_3,
    FVDP_ASPECT_RATIO_4_3_LETTERBOX,
    FVDP_ASPECT_RATIO_16_9,
    FVDP_ASPECT_RATIO_16_9_PILLARBOX,
    FVDP_ASPECT_RATIO_16_9_STRETCH,
    FVDP_ASPECT_RATIO_16_9_NONLINEAR
} FVDP_AspectRatio_t;

/* driver initialization parameters */
typedef struct FVDP_Init_e
{
    uint32_t* pDeviceBaseAddress;         // Device Base Address
} FVDP_Init_t;

/* Picture Display Mode Flag
   The flag indicated the incoming picture is one of the following status
   1. The inserted picture is repeated.  FVDP may ignore it as the data is already in buffer
   2. The inserted picture characteristics (size, scantype etc ...) have changed
   3. One or more pictures has been skipped upstream (temporal discontinuity), FVDP may reset motion analysis
*/
typedef enum FVDP_DisplayModeFlags_e
{
    FVDP_FLAGS_NONE                             = 0,
    FVDP_FLAGS_PICTURE_REPEATED                 = (1L<<0),
    FVDP_FLAGS_PICTURE_CHARACTERISTICS_CHANGED  = (1L<<1),
    FVDP_FLAGS_PICTURE_TEMPORAL_DISCONTINUITY   = (1L<<2),
} FVDP_DisplayModeFlags_t;


/* Information or attribute of video field/frame or stream data */
typedef	struct FVDP_VideoInfo_e
{
    uint32_t                FrameID;
    FVDP_ScanType_t         ScanType;
    FVDP_FieldType_t        FieldType;
    FVDP_ColorSpace_t       ColorSpace;
    FVDP_ColorSampling_t    ColorSampling;
    uint32_t                FrameRate;
    uint16_t                Width;
    uint16_t                Height;
    uint16_t                Stride;
    FVDP_AspectRatio_t      AspectRatio;
    FVDP_3DFormat_t         FVDP3DFormat;
    FVDP_3DFlag_t           FVDP3DFlag;
    FVDP_3DSubsampleMode_t  FVDP3DSubsampleMode;
    FVDP_DisplayModeFlags_t FVDPDisplayModeFlag;
} FVDP_VideoInfo_t;


typedef	struct FVDP_RasterInfo_e
{
    uint16_t     HStart;         // Location of first active pixel
    uint16_t     VStart;         // Location of first active line
    uint16_t     HTotal;         // Total # of pixel per line
    uint16_t     VTotal;         // Total # of active line
    uint16_t     HActive;        // Active width pixel # per line
    uint16_t     VActive;        // Active Height active line # per frame
    bool         UVAlign;        // First pixel of 4:2:2/0 is align to U(TRUE) or V(FALSE)
    bool         FieldAlign;     // Interlace input, bottom field start on same line as Top(TRUE) or
                                 // one line later(FALSE)
} FVDP_RasterInfo_t;


typedef	enum FVDP_State_e
{
    FVDP_STOP,
    FVDP_PLAY,              // Normal Play Mode
    FVDP_PLAY_AUTO_BLANK,   // Normal play mode, automatically blank screen if HW Auto Force BG is set
    FVDP_PAUSE,
    FVDP_BLANK,             // Display Blanking Color instead of next input frame
    FVDP_POWERDOWN
} FVDP_State_t;


typedef	enum FVDP_BWOptions_e
{
    FVDP_NONE               = 0,
    FVDP_LUMA_BASE          = 1,
    FVDP_LUMA_AGGRESSIVE    = 2,
    FVDP_CHROMA_420         = 4,
    FVDP_TNR_LUMA_ONLY      = 8,
    FVDP_DNR_3BIT_MEM       = 16,
    FVDP_8BIT               = 32
} FVDP_BWOptions_t;


typedef	struct FVDP_CropWindow_e
{
    uint16_t HCropStart;         // Integer horizontal start position
    uint8_t  HCropStartOffset;   // Fractional start postion, in 1/32 step increment
    uint16_t HCropWidth;         // Crop window width in number of pixel
    uint16_t VCropStart;         // Integer vertical start position
    uint8_t  VCropStartOffset;   // Fractional start postion, in 1/32 step increment
    uint16_t VCropHeight;        // Crop window height in number of line
} FVDP_CropWindow_t;


/* PSI features adjustment */
typedef	enum
{
    FVDP_SHARPNESS_LEVEL,         // Sharpness level
    FVDP_BRIGHTNESS,        // Brightness level (Luma offset)
    FVDP_CONTRAST,          // Contrast level (Luma gain)
    FVDP_SATURATION,        // Staturation (Chroma gain)
    FVDP_HUE,               // Hue (tint)
    FVDP_RED_GAIN,          // Red channel gain
    FVDP_GREEN_GAIN,        // Green channel gain
    FVDP_BLUE_GAIN,         // Blue channel gain
    FVDP_RED_OFFSET,        // Red channel offset
    FVDP_GREEN_OFFSET,      // Green channel offset
    FVDP_BLUE_OFFSET,       // Blue channel gain
    FVDP_3D_DEPTH           // 3D depth
} FVDP_PSIControl_t;

/* PSI features selection */
typedef	enum FVDP_PSIFeature_e
{
    FVDP_SCALER,
    FVDP_SHARPNESS,
    FVDP_MADI,
    FVDP_AFM,
    FVDP_MCTI,
    FVDP_TNR,
    FVDP_CCS,
    FVDP_DNR,
    FVDP_MNR,
    FVDP_ACC,
    FVDP_ACM,
    FVDP_IP_CSC,
    FVDP_OP_CSC,
    FVDP_PSI_NUM
} FVDP_PSIFeature_t;

/* PSI features enable, disable */
typedef	enum
{
    FVDP_PSI_ON,
    FVDP_PSI_OFF,
    FVDP_PSI_DEMO_MODE_ON_LEFT,
    FVDP_PSI_DEMO_MODE_ON_RIGHT
} FVDP_PSIState_t;


/* PSI tuning data file information */
typedef	struct FVDP_PSITuningData_e
{
    uint32_t              Size;       // Tuning file size, including header and payload, in # of bytes
    FVDP_PSIFeature_t   Signature;  // PSI feature for which the tuning data applies
    uint32_t                 Version;    // Version of tuning data.
} FVDP_PSITuningData_t;


/* External Information may be usful to pass to FVDP */
typedef	enum
{
    FVDP_EXT_ANALOG_NOISE_MEAS,
    FVDP_EXT_ANALOG_NOISE_EST,
    FVDP_EXT_DIGITAL_MPEG_BLOCK_HOFFSET,
    FVDP_EXT_DIGITAL_MPEG_BLOCK_VOFFSET
} FVDP_ExternalInfo_t;


/* FVDP buffer status */
typedef	enum FVDP_QueueStatus_e
{
    FVDP_QUEUE_NOT_EMPTY,       // Another buffer can be queue
    FVDP_QUEUE_NOT_FULL,        // Buffer is available to be grabbed
    FVDP_QUEUE_OVERFLOW,        // Not able to queue another buffer
    FVDP_QUEUE_UNDERFLOW        // No buffer is available fro grab
} FVDP_QueueStatus_t;


/* FVDP Driver version information */
typedef	struct FVDP_Version_e
{
    uint16_t Major;
    uint16_t Minor;
    uint8_t  ReleaseID[32];
} FVDP_Version_t;

//scaler mode
typedef enum
{
    FVDP_SCALER_MODE_NORMAL,
    FVDP_SCALER_MODE_PANORAMIC,
    FVDP_SCALER_MODE_DYNAMIC,
} FVDP_ScalerMode_t;

/* FVDP CRC information */
typedef enum FVDP_CrcFeature_e
{
    FVDP_CRC_IVP,
} FVDP_CrcFeature_t;

typedef struct FVDP_CrcInfo_e
{
    uint32_t     CrcValue;
} FVDP_CrcInfo_t;

/* FVDP Maximum Output Window */
typedef struct FVDP_MaxOutputWindow_e
{
    uint16_t Width;
    uint16_t Height;
} FVDP_MaxOutputWindow_t;

/*Processing type on data through a specific channel and Max output window*/
typedef struct FVDP_Context_e
{
     FVDP_ProcessingType_t   ProcessingType;
     FVDP_MaxOutputWindow_t   MaxWindowSize;
} FVDP_Context_t;

typedef    struct FVDP_OuputWindow_e
{
    uint16_t HStart;         // horizontal start position
    uint16_t HWidth;         // width in number of pixel
    uint16_t VStart;         // vertical start position
    uint16_t VHeight;        // height in number of line
} FVDP_OutputWindow_t;
/* FVDP API Prototype */

typedef     enum FVDP_Power_State_e
{
    FVDP_RESUME,              //Restore FVDP driver to play-ready state
    FVDP_SUSPEND,             //Put FVDP driver into suspend state
} FVDP_Power_State_t;


typedef enum FVDP_CtrlColor_e
{
    FVDP_CTRL_COLOR_STATE_DISABLE, //default state : disabled & no RGB value
    FVDP_CTRL_COLOR_STATE_ENABLE,  //enabled, no RGB value set
    FVDP_CTRL_COLOR_VALUE,         //RGB value set, not enabled yet
    FVDP_CTRL_COLOR_STATE_APPLIED  //enabled & RGB value set
} FVDP_CtrlColor_t;

typedef struct ColorFill_e
{
    FVDP_CtrlColor_t State;
    uint32_t RGB_Value;
} ColorFill_t;


FVDP_Result_t FVDP_InitDriver(FVDP_Init_t InitParams);

FVDP_Result_t FVDP_GetRequiredVideoMemorySize(FVDP_CH_t       CH,
                                              FVDP_Context_t  Context,
                                              uint32_t*       MemSize);

FVDP_Result_t FVDP_OpenChannel(FVDP_CH_Handle_t*  FvdpChHandle_p,  // returns handle to channel
                               FVDP_CH_t          CH,              // specifies the physical channel that the handle should will be associated with
                               uint32_t           MemBaseAddress,
                               uint32_t           MemSize,
                               FVDP_Context_t     Context);


FVDP_Result_t FVDP_CloseChannel(FVDP_CH_Handle_t  FvdpChHandle);


FVDP_Result_t FVDP_ConnectInput(FVDP_CH_Handle_t    FvdpChHandle,
                                FVDP_Input_t        Input);          // Use FVDP_MEM_INPUT_... for mem to mem connection


FVDP_Result_t FVDP_DisconnectInput(FVDP_CH_Handle_t FvdpChHandle);


FVDP_Result_t FVDP_SetInputVideoInfo(FVDP_CH_Handle_t  FvdpChHandle,
                                     FVDP_VideoInfo_t  InputVideoInfo);


FVDP_Result_t FVDP_GetInputVideoInfo(FVDP_CH_Handle_t  FvdpChHandle,
                                     FVDP_VideoInfo_t* InputVideoInfo);


FVDP_Result_t FVDP_SetOutputVideoInfo(FVDP_CH_Handle_t FvdpChHandle,
                                      FVDP_VideoInfo_t OutputVideoInfo);


FVDP_Result_t FVDP_GetOutputVideoInfo(FVDP_CH_Handle_t  FvdpChHandle,
                                      FVDP_VideoInfo_t* OutputVideoInfo);
FVDP_Result_t FVDP_SetOutputWindow(FVDP_CH_Handle_t FvdpChHandle,
                                      FVDP_OutputWindow_t *OutputActiveWindow);
FVDP_Result_t FVDP_GetOutputWindow(FVDP_CH_Handle_t  FvdpChHandle,
                                      FVDP_OutputWindow_t *OutputActiveWindow);


FVDP_Result_t FVDP_SetInputRasterInfo(FVDP_CH_Handle_t  FvdpChHandle,
                                      FVDP_RasterInfo_t InputRasterInfo);


FVDP_Result_t FVDP_GetInputRasterInfo(FVDP_CH_Handle_t   FvdpChHandle,
                                      FVDP_RasterInfo_t* InputRasterInfo);


FVDP_Result_t FVDP_SetState(FVDP_CH_Handle_t FvdpChHandle,
                            FVDP_State_t     State);


FVDP_Result_t FVDP_GetState(FVDP_CH_Handle_t FvdpChHandle,
                            FVDP_State_t*    State);


FVDP_Result_t FVDP_SetLatencyType(FVDP_CH_Handle_t FvdpChHandle,
                              uint32_t          LatencyType);


FVDP_Result_t FVDP_GetLatencyType(FVDP_CH_Handle_t FvdpChHandle,
                              uint32_t  *        LatencyType);


FVDP_Result_t FVDP_SetLatency(FVDP_CH_Handle_t FvdpChHandle,
                              uint8_t          FrameLatency,
                              uint16_t         LineLatency);


FVDP_Result_t FVDP_GetLatency(FVDP_CH_Handle_t FvdpChHandle,
                              uint8_t*         MinInputLatency,
                              uint8_t*         MaxInputLatency,
                              uint8_t*         MinOutputLatency,
                              uint8_t*         MaxOutputLatency);


FVDP_Result_t FVDP_SetBWOptions(FVDP_CH_Handle_t  FvdpChHandle,
                                FVDP_BWOptions_t  BWOptions);


FVDP_Result_t FVDP_GetBWOptions(FVDP_CH_Handle_t  FvdpChHandle,
                                FVDP_BWOptions_t* BWOptions);


FVDP_Result_t FVDP_SetCropWindow(FVDP_CH_Handle_t   FvdpChHandle,
                                 FVDP_CropWindow_t  CropWindow);


FVDP_Result_t FVDP_GetCropWindow(FVDP_CH_Handle_t   FvdpChHandle,
                                 FVDP_CropWindow_t* CropWindow);


FVDP_Result_t FVDP_SetBlankingColor(FVDP_CH_Handle_t FvdpChHandle,
                                    uint16_t         Red,
                                    uint16_t         Green,
                                    uint16_t         Blue);


FVDP_Result_t FVDP_GetBlankingColor(FVDP_CH_Handle_t FvdpChHandle,
                                    uint16_t*        Red,
                                    uint16_t*        Green,
                                    uint16_t*        Blue);


FVDP_Result_t FVDP_SetPSIControl(FVDP_CH_Handle_t  FvdpChHandle,
                                 FVDP_PSIControl_t PSIControl,
                                 int32_t           Value);


FVDP_Result_t FVDP_GetPSIControl(FVDP_CH_Handle_t  FvdpChHandle,
                                 FVDP_PSIControl_t PSIControl,
                                 int32_t*          Value);


FVDP_Result_t FVDP_SetPSIState(FVDP_CH_Handle_t    FvdpChHandle,
                               FVDP_PSIFeature_t   PSIFeature,
                               FVDP_PSIState_t     PSIState);


FVDP_Result_t FVDP_GetPSIState(FVDP_CH_Handle_t    FvdpChHandle,
                               FVDP_PSIFeature_t   PSIFeature,
                               FVDP_PSIState_t*    PSIState);


FVDP_Result_t FVDP_SetPSITuningData (FVDP_CH_Handle_t      FvdpChHandle,
                                     FVDP_PSIFeature_t     PSIFeature,
                                     FVDP_PSITuningData_t* PSITuningData);


FVDP_Result_t FVDP_GetPSITuningData(FVDP_CH_Handle_t       FvdpChHandle,
                                    FVDP_PSIFeature_t      PSIFeature,
                                    FVDP_PSITuningData_t*  PSITuningData);


FVDP_Result_t FVDP_GetVersion(FVDP_Version_t* Version);


FVDP_Result_t FVDP_GetPSIControlsRange(FVDP_CH_Handle_t  FvdpChHandle,
                                       FVDP_PSIControl_t PSIControlsSelect,
                                       int32_t*          MinValue,
                                       int32_t*          MaxValue,
                                       int32_t*          DefaultValue,
                                       int32_t*          StepValue);


FVDP_Result_t FVDP_SetExternalInfo(FVDP_CH_Handle_t       FvdpChHandle,
                                   FVDP_ExternalInfo_t    ExternalInfo,
                                   int32_t                Value);


FVDP_Result_t FVDP_GetExternalInfo(FVDP_CH_Handle_t       FvdpChHandle,
                                   FVDP_ExternalInfo_t    ExternalInfo,
                                   int32_t*               Value);


FVDP_Result_t FVDP_InputUpdate(FVDP_CH_Handle_t   FvdpChHandle);


FVDP_Result_t FVDP_OutputUpdate(FVDP_CH_Handle_t  FvdpChHandle);


FVDP_Result_t FVDP_CommitUpdate(FVDP_CH_Handle_t  FvdpChHandle);

#if (ENC_HW_REV != REV_NONE)
FVDP_Result_t FVDP_SetBufferCallback(FVDP_CH_Handle_t FvdpChHandle,
                                     void (*ScalingCompleted)(void* UserData, bool Status),    /* Callback when the scaling task is completed */
                                     void (*BufferDone)(void* UserData) );        /* Callback when the  buffer is no longer needed for any future processing */

FVDP_Result_t FVDP_QueueBuffer(FVDP_CH_Handle_t  FvdpChHandle,
                               void*             UserData,
                               uint32_t          SrcAddr1,
                               uint32_t          SrcAddr2,
                               uint32_t          DstAddr1,
                               uint32_t          DstAddr2);
// For detecting end of scaling in mem to mem case
FVDP_Result_t   FVDP_IntEndOfScaling(void);
#endif

FVDP_Result_t FVDP_QueueGetStatus(FVDP_CH_Handle_t    FvdpChHandle,
                                  FVDP_QueueStatus_t* QueueStatus);


FVDP_Result_t FVDP_QueueFlush(FVDP_CH_Handle_t FvdpChHandle, bool bFlushCurrentNode);


FVDP_Result_t FVDP_SetCrcState(FVDP_CH_Handle_t  FvdpChHandle,
                               FVDP_CrcFeature_t Feature,
                               bool              State);


FVDP_Result_t FVDP_GetCrcInfo(FVDP_CH_Handle_t   FvdpChHandle,
                              FVDP_CrcFeature_t  Feature,
                              FVDP_CrcInfo_t*    Info);

FVDP_Result_t FVDP_SetOutputRasterInfo(FVDP_CH_Handle_t  FvdpChHandle,
                                      FVDP_RasterInfo_t OutputRasterInfo);

FVDP_Result_t FVDP_PowerControl(FVDP_Power_State_t PowerState);

FVDP_Result_t FVDP_SetVideoProcType(FVDP_CH_Handle_t  FvdpChHandle, FVDP_ProcessingType_t VideoProcType);

FVDP_Result_t FVDP_ColorFill(FVDP_CH_Handle_t FvdpChHandle, FVDP_CtrlColor_t Enable, uint32_t Value);


/* C++ support */
#ifdef __cplusplus
}
#endif

#endif /* #ifndef __FVDP_H */

/* End of fvdp.h */
