/***************************************************************************
This file is part of display_engine
COPYRIGHT (C) 2012-2014 STMicroelectronics - All Rights Reserved
License type: GPLv2

display_engine is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as published by
the Free Software Foundation.

display_engine is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with  display_engine; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

This file was last modified by STMicroelectronics on 2014-05-30
***************************************************************************/

/* Define to prevent recursive inclusion */
#ifndef __FVDP_SYSTEM_H
#define __FVDP_SYSTEM_H

/* Includes ----------------------------------------------------------------- */

#include "fvdp_update.h"
#include "fvdp_services.h"
#include "fvdp_datapath.h"
#include "fvdp_mcc.h"
#include "fvdp_color.h"
#include "../fvdp_vqdata.h"

/* C++ support */
#ifdef __cplusplus
extern "C" {
#endif

// Over write field polarity signal based on InputVideoInfo.FieldType
#define SW_ODD_FIELD_CONTROL              TRUE

// Option to program the DFR in the VCPU (in the blanking area)
#define DFR_FE_PROGRAMMING_IN_VCPU        TRUE
#define DFR_BE_PROGRAMMING_IN_VCPU        FALSE
#define DFR_PIP_PROGRAMMING_IN_VCPU       FALSE
#define DFR_AUX_PROGRAMMING_IN_VCPU       FALSE

#define ENC_MEM_TO_MEM_DELAY_MS             10

#define BUG25599_AUX_WRONG_COLOR_COMPENSATION   TRUE
#define OUTPUT_FIELD_SWAP_COMPENSATION   TRUE

// Enable external coefficient table
// FALSE: Use internal coefficient table
// TRUE : Use external coefficient table
#define ENABLE_EXTENAL_FILTER_COEFFICIENT TRUE

/* Exported Enumerations ---------------------------------------------------- */

typedef enum BufferType_e
{
    BUFFER_CCS_UV_RAW       = 0,
    BUFFER_C_Y              = 1,
    BUFFER_C_UV             = 2,
    BUFFER_MCDI_SB          = 3,
    BUFFER_PRO_Y_G          = 4,
    BUFFER_PRO_U_UV_B       = 5,
    NUM_BUFFER_TYPES
} BufferType_t;

typedef enum Power_State_e
{
    RESUME_POWER_STATE,              //Restore FVDP driver to play-ready state
    SUSPEND_POWER_STATE,             //Put FVDP driver into suspend state
    UNKNOWN_POWER_STATE,             //Put FVDP driver into UNKNOWN power state when
} Power_State_t;

/* Exported Structure ------------------------------------------------------- */

typedef struct
{
    MCC_Client_t    MCC_Client;
    BufferType_t    BufferType;
} MCCClientToBufferTypeMap_t;

typedef struct
{
    UPDATE_InputUpdateFlag_t    InputUpdateFlag;        // Input Update flags
    UPDATE_OutputUpdateFlag_t   OutputUpdateFlag;       // Output Update flags
    UPDATE_CommitUpdateFlag_t   CommitUpdateFlag;       // Commit Update flags
    UPDATE_PSIVQFeatureFlag_t   PSIVQUpdateFlag;          // PSI Update flags
} FVDP_UpdateFlags;

typedef struct
{
    int32_t                 SharpnessLevel;
    int32_t                 BrightnessLevel;
    int32_t                 ContrastLevel;
    int32_t                 SaturationLevel;
    int32_t                 HueLevel;
    int32_t                 RedGain;
    int32_t                 GreenGain;
    int32_t                 BlueGain;
    int32_t                 RedOffset;
    int32_t                 GreenOffset;
    int32_t                 BlueOffset;
    int32_t                 Depth_3D;
} FVDP_PSIControlLevel_t;

typedef struct
{
    uint32_t                Addr;                       // The DDR address of this buffer
    MCC_Config_t            MccConfig;                  // The memory client (MCC) configuration for this buffer
    uint32_t                NumOfBuffers;               // The number of consequetive buffer objects allocated
} FVDP_FrameBuffer_t;

typedef struct
{
    vibe_time64_t           Timestamp;                  // Time when FVDP_Frame_t instance was created
    uint32_t                FrameID;                    // Frame ID
    uint32_t                FramePoolIndex;             // The FramePoolArray index for this frame
    FVDP_VideoInfo_t        InputVideoInfo;             // Input Video Info that applies to this frame -- describes the frame when it came into FVDP
    FVDP_VideoInfo_t        OutputVideoInfo;            // Output Video Info that applies to this frame -- describes what the frame should look like when it leaves the FVDP
    FVDP_OutputWindow_t     OutputActiveWindow;         // Output active window specifies what part of output eindow whill take the image, The borders take the rest of output window.
    FVDP_CropWindow_t       CropWindow;                 // Cropping parameters for this frame
    FVDP_BWOptions_t        BWOptions;                  // Compression options for this frame

    // Frame Buffers
    FVDP_FrameBuffer_t*     FrameBuffer[NUM_BUFFER_TYPES];
} FVDP_Frame_t;

typedef struct
{
    ObjectPool_t            FrameObjectPool;            // Object Pool handle that will be used for Pool Service calls.
    FVDP_Frame_t*           FramePoolArray;             // Array of Frames that constitutes the frame pool.
    uint8_t*                FramePoolStates;            // Array of object states (OBJECT_FREE or OBJECT_USED) that is required by the ObjectPool Service.
} FVDP_FramePool_t;

typedef struct
{
    ObjectPool_t            BufferObjectPool;
    FVDP_FrameBuffer_t*     BufferPoolArray;
    uint8_t*                BufferPoolStates;
    uint32_t                BufferStartAddr;
    uint32_t                BufferSize;
    uint8_t                 NumberOfBuffers;
} FVDP_BufferPool_t;

typedef struct
{
    FVDP_FramePool_t        FramePool;                  // Pool of frames for input interlaced fields
    FVDP_Frame_t*           NextInputFrame_p;           // The Frame information that should be applied to the NEXT input frame
    FVDP_Frame_t*           CurrentInputFrame_p;        // The Frame information for the CURRENT input frame
    FVDP_Frame_t*           CurrentProScanFrame_p;      // The Frame information for the CURRENT PRO frame (the output of FE to the BE)
    FVDP_Frame_t*           PrevProScanFrame_p;         // The Frame information for the PREV PRO frame (the output of FE to the BE)
    FVDP_Frame_t*           Prev1InputFrame_p;          // The Frame information for the PREV input frame
    FVDP_Frame_t*           Prev2InputFrame_p;          // The Frame information for the PREV-1 input frame
    FVDP_Frame_t*           Prev3InputFrame_p;          // The Frame information for the PREV-2 input frame
    FVDP_Frame_t*           CurrentOutputFrame_p;       // The Frame information for the CURRENT OUTPUT frame
    FVDP_Frame_t            PrevOutputFrame;            // The Frame information for the PREV OUTPUT frame
    FVDP_BufferPool_t       BufferPool[NUM_BUFFER_TYPES];
    MCC_Config_t            CurrentMccConfig[NUM_BUFFER_TYPES];  // The MCC Configuration for the Current input frame.
                                                                 // This is required since FVDP_FrameBuffer_t cannot be allocated for the current frame
                                                                 // since VCPU is allocating the buffers later (during FrameReady process)
    uint32_t                PrevProScanHeight;          // Vertical size of previous pro scan frame
    uint32_t                PrevProScanWidth;           // Horizontal size of previous pro scan frame
} FVDP_FrameInfo_t;

typedef struct
{
    DATAPATH_FE_t           DatapathFE;                 // The DFR datapath for FVDP_FE
    DATAPATH_BE_t           DatapathBE;                 // The DFR datapath for FVDP_BE
    DATAPATH_LITE_t         DatapathLite;               // The DFR datapath for FVDP_ENC or FVDP_PIP or FVDP_AUX
} FVDP_DataPath_t;

typedef struct
{
    uint32_t                SrcAddr1;                   // Luma or RGB DDR base address of the source frame.
    uint32_t                SrcAddr2;                   // Chroma base address of the source frame (not used for RGB source frame).
    uint32_t                DstAddr1;                   // Luma or RGB DDR base address of the destination frame.
    uint32_t                DstAddr2;                   // Chroma base address of the destination frame (not used for RGB destination frame).
    void (*ScalingCompleted)(void* UserData, bool Status);         // Callback when the scaling task is completed.
    void (*BufferDone)(void* UserData);               // Callback when the  buffer is no longer needed for any future processing.
    void*                   TimerThreadDescr;           // Timer thread descriptor. Used in ENC for creating timer thread
    uint32_t                TimerSemaphore;             // ENC timer delay semaphore. Used initiate delayed call of ENC callback functions
    bool                    ScalingStatus;              // TRUE when ENC scaling is completed successfully
    void*                   UserData;                   // User data asociated with end of memory operation.
    void*                   UserDataPrevious1;          // User data asociated with end of memory operation, previous buffer used by tnr
    void*                   UserDataPrevious2;          // User data asociated with end of memory operation, previous buffer used by tnr
} FVDP_QueueBufferInfo_t;

typedef struct
{
    VQ_ACC_Parameters_t       ACCVQData;
    VQ_ACM_Parameters_t       ACMVQData;
    VQ_AFM_Parameters_t       AFMVQData;
    VQ_CCS_Parameters_t       CCSVQData;
    VQ_COLOR_Parameters_t     IPCSCVQData;
    VQ_COLOR_Parameters_t     OPCSCVQData;
    VQ_DNR_Parameters_t       DNRVQData;
    VQ_MNR_Parameters_t       MNRVQData;
    VQ_MADI_Parameters_t      MADIVQData;
    VQ_MCTI_Parameters_t      MCTIVQData;
    VQ_SCALER_Parameters_t    ScalerVQData;
    VQ_SHARPNESS_Parameters_t SharpnessVQData;
    VQ_TNR_Parameters_t TNRVQData;
} FVDP_PSI_VQFeature_t;

typedef struct
{
    FVDP_CH_t               CH;                         // The Channel associated with this Context/Handle
    FVDP_CH_Handle_t        FvdpChHandle;               // The handle number -- for debug purpose.
    void                  * FvdpInputLock;
    void                  * FvdpOutputLock;
    uint32_t                MemBaseAddress;             // The base address of the video memory buffer used by this channel
    uint32_t                MemSize;                    // The size of the video memory buffer given to this channel
    FVDP_ProcessingType_t   ProcessingType;             // The type of processing expected on this channel
    FVDP_MaxOutputWindow_t  MaxOutputWindow;            // The maximum output window size
    FVDP_Input_t            InputSource;                // The input source of the channel
    FVDP_RasterInfo_t       InputRasterInfo;            // The raster info if the input is raster video
    FVDP_RasterInfo_t       OutputRasterInfo;           // The output display raster info
    FVDP_DataPath_t         DataPath;                   // The DFR data path information
    FVDP_FrameInfo_t        FrameInfo;                  // All the information related to the Frames associated with this FVDP Handle
    FVDP_PSI_VQFeature_t    PSIFeature;             //  All the information for PSI configuration should be applied on Commit Update
    FVDP_PSIControlLevel_t  PSIControlLevel;
    FVDP_PSIState_t         PSIState[(uint32_t)FVDP_PSI_NUM]; //State of psi features On of Off or demo
    FVDP_QueueBufferInfo_t  QueueBufferInfo;            // Information for memory to memory operation.
    FVDP_UpdateFlags        UpdateFlags;                // The InputUpdate and CommitUpdate flags
    uint16_t                OutputUnmuteDelay;          // Delay for unmuting output using csc, n-1
    FVDP_LatencyType_t      LatencyType;                // The kind of latency the pipe is supposed to expose
} FVDP_HandleContext_t;

typedef struct
{
    FVDP_PSI_VQFeature_t    HWPSIFeature;
    FVDP_PSIControlLevel_t  HWPSIControlLevel;
    FVDP_PSIState_t         HWPSIState[(uint32_t)FVDP_PSI_NUM];
} FVDP_HW_PSIConfig_t;

typedef struct
{
    FVDP_VideoInfo_t        InputVideoInfo;
    FVDP_CropWindow_t       CropWindow;
    FVDP_RasterInfo_t       InputRasterInfo;
} FVDP_HW_IVPConfig_t;

typedef struct
{
    Power_State_t           PowerState;
    FVDP_HW_PSIConfig_t     HW_PSIConfig;
    FVDP_HW_IVPConfig_t     HW_IVPConfig;
} FVDP_HW_Config_t;

/* Exported Variables ------------------------------------------------------- */
extern FVDP_HW_Config_t HW_Config[NUM_FVDP_CH];

/* Exported Functions ------------------------------------------------------- */

FVDP_Result_t         SYSTEM_Init(void);

FVDP_Result_t         SYSTEM_OpenChannel(FVDP_CH_Handle_t* FvdpChHandle_p,
                                         FVDP_CH_t         CH,
                                         uint32_t          MemBaseAddress,
                                         uint32_t          MemSize,
                                         FVDP_Context_t    Context);

bool                  SYSTEM_IsChannelHandleOpen(FVDP_CH_Handle_t Handle);

FVDP_Result_t         SYSTEM_FlushChannel(FVDP_CH_Handle_t FvdpChHandle, bool bFlushCurrentNode);

void                  SYSTEM_CloseChannel(FVDP_CH_Handle_t Handle);

FVDP_HandleContext_t* SYSTEM_GetHandleContext(FVDP_CH_Handle_t Handle);

FVDP_Frame_t*         SYSTEM_AllocateFrame(FVDP_HandleContext_t* HandleContext_p);

FVDP_Frame_t*         SYSTEM_CloneFrame(FVDP_HandleContext_t* HandleContext_p,
                                        FVDP_Frame_t*         SourceFrame_p);

void                  SYSTEM_FreeFrame(FVDP_HandleContext_t* HandleContext_p, FVDP_Frame_t* Frame_p);

bool                  SYSTEM_IsFrameValid(FVDP_HandleContext_t* HandleContext_p, FVDP_Frame_t* Frame_p);

FVDP_Frame_t*         SYSTEM_GetFrameFromIndex(FVDP_HandleContext_t* HandleContext_p, uint8_t Index);

FVDP_Frame_t*         SYSTEM_GetCurrentInputFrame(FVDP_HandleContext_t* HandleContext_p);

FVDP_Frame_t*         SYSTEM_GetCurrentProScanFrame(FVDP_HandleContext_t* HandleContext_p);

FVDP_Frame_t*         SYSTEM_GetPrevProScanFrame(FVDP_HandleContext_t* HandleContext_p);

FVDP_Frame_t*         SYSTEM_GetNextInputFrame(FVDP_HandleContext_t* HandleContext_p);

FVDP_Frame_t*         SYSTEM_GetPrev1InputFrame(FVDP_HandleContext_t* HandleContext_p);

FVDP_Frame_t*         SYSTEM_GetPrev2InputFrame(FVDP_HandleContext_t* HandleContext_p);

FVDP_Frame_t*         SYSTEM_GetPrev3InputFrame(FVDP_HandleContext_t* HandleContext_p);

FVDP_Frame_t*         SYSTEM_GetCurrentOutputFrame(FVDP_HandleContext_t* HandleContext_p);

FVDP_Frame_t*         SYSTEM_GetPrevOutputFrame(FVDP_HandleContext_t* HandleContext_p);

void                  SYSTEM_SetCurrentProScanFrame(FVDP_HandleContext_t* HandleContext_p,
                                                    FVDP_Frame_t* CurrentProScanFrame_p);

void                  SYSTEM_SetNextInputFrame(FVDP_HandleContext_t* HandleContext_p,
                                               FVDP_Frame_t* NextInputFrame_p);

FVDP_FrameBuffer_t*   SYSTEM_AllocateBufferFromIndex(FVDP_HandleContext_t* HandleContext_p,
                                                     BufferType_t          BufferType,
                                                     uint8_t               Index,
                                                     FVDP_ScanType_t       ScanType);

FVDP_Result_t         SYSTEM_GetRequiredVideoMemorySize(FVDP_CH_t              CH,
                                                        FVDP_ProcessingType_t  ProcessingType,
                                                        FVDP_MaxOutputWindow_t MaxOutputWindow,
                                                        uint32_t*              MemSize);

FVDP_Result_t         SYSTEM_BE_ProcessOutput(FVDP_HandleContext_t* HandleContext_p);

FVDP_Result_t         SYSTEM_VMUX_Init(FVDP_CH_t CH);

FVDP_Result_t         SYSTEM_VMUX_Config(FVDP_HandleContext_t* HandleContext_p);

FVDP_Result_t         SYSTEM_FE_Init(FVDP_HandleContext_t* HandleContext_p);

FVDP_Result_t         SYSTEM_BE_Init(FVDP_HandleContext_t* HandleContext_p);

FVDP_Result_t         SYSTEM_InitPsiControlData(FVDP_PSIControlLevel_t * FVDP_PSIControlLevel);

FVDP_Result_t         SYSTEM_FE_ProcessInput(FVDP_HandleContext_t* HandleContext_p);

FVDP_Result_t         SYSTEM_LITE_Init(FVDP_HandleContext_t* HandleContext_p);

FVDP_Result_t         SYSTEM_LITE_ProcessInput(FVDP_HandleContext_t* HandleContext_p);

FVDP_Result_t         SYSTEM_LITE_ProcessOutput(FVDP_HandleContext_t* HandleContext_p);

FVDP_Result_t         SYSTEM_ENC_Open(FVDP_HandleContext_t* HandleContext_p);

FVDP_Result_t         SYSTEM_ENC_ProcessMemToMem(FVDP_HandleContext_t* HandleContext_p);

FVDP_Result_t         SYSTEM_ENC_FlushChannel(FVDP_HandleContext_t* HandleContext_p);

FVDP_Result_t         SYSTEM_ENC_CompleteScalling(void);

FVDP_Result_t         SYSTEM_ENC_Init(void);

FVDP_Result_t         SYSTEM_CopyColorControlData(const FVDP_HandleContext_t* HandleContext_p,
                                                  ColorMatrix_Adjuster_t    * ColorCtrl);

FVDP_Result_t         SYSTEM_InitMCC(FVDP_HandleContext_t* HandleContext_p);

void                  SYSTEM_ENC_StartDelayedCallBack(FVDP_HandleContext_t* HandleContext_p);

void                  SYSTEM_ENC_Terminate(void);

void                  SYSTEM_ENC_Close(FVDP_HandleContext_t* HandleContext_p);

FVDP_Result_t         SYSTEM_os_lock_resource   ( FVDP_CH_t CH, void * FvdpLock );

FVDP_Result_t         SYSTEM_os_unlock_resource ( FVDP_CH_t CH, void * FvdpLock );

FVDP_HW_PSIConfig_t*  SYSTEM_GetHWPSIConfig(FVDP_CH_t CH);

FVDP_Result_t         SYSTEM_PSI_Sharpness_Init (FVDP_HandleContext_t* HandleContext_p);

FVDP_Result_t         SYSTEM_PSI_Sharpness(FVDP_HandleContext_t* HandleContext_p);

FVDP_Result_t         SYSTEM_PSI_Sharpness_ControlRange (FVDP_CH_t CH,
                                           int32_t *pMin,
                                           int32_t *pMax,
                                           int32_t *pStep);

FVDP_Result_t         SYSTEM_PSI_CCS_Init(FVDP_HandleContext_t* HandleContext_p);

FVDP_Result_t         SYSTEM_PSI_CCS(FVDP_HandleContext_t* HandleContext_p);

FVDP_HandleContext_t* SYSTEM_GetHandleContextOfChannel(FVDP_CH_t CH);

FVDP_Result_t         SYSTEM_InitBufferTable(void);

FVDP_Result_t         SYSTEM_SetPowerStateUpdate(FVDP_Power_State_t PowerState);

FVDP_Result_t         SYSTEM_FE_PowerDown(void);

FVDP_Result_t         SYSTEM_BE_PowerDown(void);

FVDP_Result_t         SYSTEM_LITE_PowerDown(FVDP_CH_t CH);

FVDP_Result_t         SYSTEM_ENC_PowerDown(void);

FVDP_Result_t         SYSTEM_FE_PowerUp(void);

FVDP_Result_t         SYSTEM_BE_PowerUp(void);

FVDP_Result_t         SYSTEM_LITE_PowerUp(FVDP_CH_t CH);

FVDP_Result_t         SYSTEM_ENC_PowerUp(void);

bool                  SYSTEM_CheckFVDPPowerUpState(void);

FVDP_Result_t         SYSTEM_ReInitFramePool(FVDP_CH_Handle_t FvdpChHandle, FVDP_ProcessingType_t  VideoProcType);

bool                  SYSTEM_CheckPSIColorLevel(FVDP_HandleContext_t* HandleContext_p);

FVDP_Result_t         SYSTEM_GetColorRange(FVDP_PSIControl_t  ColorControlSel,int32_t *pMin,int32_t *pMax,int32_t *pStep);

/* C++ support */
#ifdef __cplusplus
}
#endif

#endif /* #ifndef __FVDP_SYSTEM_H */

/* End of fvdp_system.h */
