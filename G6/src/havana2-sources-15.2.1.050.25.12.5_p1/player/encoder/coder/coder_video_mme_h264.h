/************************************************************************
Copyright (C) 2003-2014 STMicroelectronics. All Rights Reserved.

This file is part of the Streaming Engine.

Streaming Engine is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Streaming Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Streaming Engine; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
USA.

The Streaming Engine Library may alternatively be licensed under a
proprietary license from ST.
************************************************************************/
#ifndef CODER_VIDEO_MME_H264_H
#define CODER_VIDEO_MME_H264_H

#include <mme.h>
#include "osinline.h"
#include "coder_video.h"
#include "H264ENCHW_VideoTransformerTypes.h"

#define TRANSLATE_BRC_MODE_STKPI_TO_MME(mode) ( (mode) + 1 )
#define TRANSLATE_BRC_MODE_MME_TO_STKPI(mode) ( (mode) - 1 )

#define CODER_MAX_WAIT_FOR_MME_COMMAND_COMPLETION       100     /* Ms */

//max header size in bytes to be written by h264 encoder (corresponds to SPS/PPS/SEI)
#define H264_ENC_MAX_HEADER_SIZE                                               200

// Range of control value (taken from hardware test plan)
// Bitrate is in bits/s and cpb size is in bits
#define H264_ENC_MIN_BITRATE 1024
#define H264_ENC_MAX_BITRATE 60000000
#define H264_ENC_MIN_CPB_SIZE 1024
#define H264_ENC_MAX_CPB_SIZE 288000000

//default values for STKPI controlled parameters
#define H264_ENC_DEFAULT_GOP_SIZE                                                         15 //encode 'GOP size' can be updated by dedicated control
#define H264_ENC_DEFAULT_PROFILE           STM_SE_CTRL_VALUE_VIDEO_H264_PROFILE_IDC_BASELINE //matches baseline, encode 'Profile' can be updated by dedicated control
#define H264_ENC_DEFAULT_LEVEL                    STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_4_2 //matches level 4.2, encode 'Level' can be updated by dedicated control
#define H264_ENC_DEFAULT_DELAY                                                             2 //encode delay in seconds
#define H264_ENC_DEFAULT_CPB_BUFFER_SIZE (H264_ENC_DEFAULT_BITRATE * H264_ENC_DEFAULT_DELAY) //encode 'CPB Buffer size' can be updated by dedicated control
#define H264_ENC_DEFAULT_BITRATE_MODE             STM_SE_CTRL_VALUE_BITRATE_CONTROL_MODE_VBR //bitrate control mode can be updated by dedicated control
#define H264_ENC_DEFAULT_BITRATE                                                     4000000 //encode 'Bitrate' can be updated by dedicated control
#define H264_ENC_DEFAULT_FRAMERATE_NUM                                                    25 //encode 'framerate' can be updated by dedicated control
#define H264_ENC_DEFAULT_FRAMERATE_DEN                                                     1

// internal encode parameter
#define H264_ENC_DEFAULT_VBR_INIT_QP                                            28
#define H264_ENC_DEFAULT_QP_MIN                                                 18
#define H264_ENC_DEFAULT_QP_MAX                                                 51
#define H264_ENC_DEFAULT_MAX_BIT_SIZE_PER_AU                                     0
#define H264_ENC_MAX_FRAME_NUM                                                  15 //as log2MaxFrameNumMinus4='0' HW constraint => Max FrameNum = 15

// level limits, from H264 Table A.1
// (level) (Max MB per second) (Max frame size) (max bitrate)  (max CPB size) (Min comp ratio)
// TODO: level 1.1 & constraint_set3_flag set ==> level 1.0b (TBC)
const unsigned int LevelLimits[][6] =
{
    { HVA_ENCODE_SPS_LEVEL_IDC_1_0,    1485,    99,     64,    175, 2 }, // 1.0  [0]
    { HVA_ENCODE_SPS_LEVEL_IDC_1_B,    1485,    99,    128,    350, 2 }, // 1.0b [1]
    { HVA_ENCODE_SPS_LEVEL_IDC_1_1,    3000,   396,    192,    500, 2 }, // 1.1  [2]
    { HVA_ENCODE_SPS_LEVEL_IDC_1_2,    6000,   396,    384,   1000, 2 }, // 1.2  [3]
    { HVA_ENCODE_SPS_LEVEL_IDC_1_3,   11880,   396,    768,   2000, 2 }, // 1.3  [4]
    { HVA_ENCODE_SPS_LEVEL_IDC_2_0,   11880,   396,   2000,   2000, 2 }, // 2.0  [5]
    { HVA_ENCODE_SPS_LEVEL_IDC_2_1,   19800,   792,   4000,   4000, 2 }, // 2.1  [6]
    { HVA_ENCODE_SPS_LEVEL_IDC_2_2,   20250,  1620,   4000,   4000, 2 }, // 2.2  [7]
    { HVA_ENCODE_SPS_LEVEL_IDC_3_0,   40500,  1620,  10000,  10000, 2 }, // 3.0  [8]
    { HVA_ENCODE_SPS_LEVEL_IDC_3_1,  108000,  3600,  14000,  14000, 4 }, // 3.1  [9]
    { HVA_ENCODE_SPS_LEVEL_IDC_3_2,  216000,  5120,  20000,  20000, 4 }, // 3.2  [10]
    { HVA_ENCODE_SPS_LEVEL_IDC_4_0,  245760,  8192,  20000,  25000, 4 }, // 4.0  [11]
    { HVA_ENCODE_SPS_LEVEL_IDC_4_1,  245760,  8192,  50000,  62500, 2 }, // 4.1  [12]
    { HVA_ENCODE_SPS_LEVEL_IDC_4_2,  522240,  8704,  50000,  62500, 2 }, // 4.2  [13]
    { HVA_ENCODE_SPS_LEVEL_IDC_5_0,  589824, 22080, 135000, 135000, 2 }, // 5.0  [14]
    { HVA_ENCODE_SPS_LEVEL_IDC_5_1,  983040, 36864, 240000, 240000, 2 }, // 5.1  [15]
    { HVA_ENCODE_SPS_LEVEL_IDC_5_2, 2073600, 36864, 240000, 240000, 2 }  // 5.2  [16]
};

#define VIDEOENCODER_MT_NAME H264ENCHW_MME_TRANSFORMER_NAME

typedef struct H264EncoderStatistics_s
{
    uint32_t EncodeNumber;
    uint32_t EncodeNumberIPic;
    uint32_t EncodeNumberPPic;
    uint32_t EncodeSumTime;
    uint32_t EncodeSumTimeIPic;
    uint32_t EncodeSumTimePPic;
    uint32_t MaxEncodeTime;
    uint32_t MinEncodeTime;
    uint32_t MaxEncodeSize;
    uint32_t MinEncodeSize;
} H264EncoderStatistics_t;

typedef struct H264EncoderControls_s
{
    H264EncodeBrcType_t       BitRateControlMode;
    uint32_t                  BitRate;
    bool                      BitRateSetByClient;
    uint32_t                  GopSize;
    H264EncodeSPSLevelIDC_t   H264Level;
    H264EncodeSPSProfileIDC_t H264Profile;
    uint32_t                  CodedPictureBufferSize;
    bool                      CpbSizeSetByClient;
    H264EncodeSliceMode_t     SliceMode;
    uint32_t                  SliceMaxMbSize;
    uint32_t                  SliceMaxByteSize;
} H264EncoderControls_t;

typedef struct H264EncoderContext_s
{
    H264EncodeHard_SetGlobalParamSequence_t GlobalParams;
    H264EncoderControls_t                   ControlsValues;
    bool                                    GlobalParamsNeedsUpdate;  //if true, a MME_SET_GLOBAL_TRANSFORM_PARAMS has to be issued before MME_TRANSFORM
    bool                                    ControlStatusUpdated;     //if true, at least one control evolved for the given encoded frame
    bool                                    NewGopRequested;          //if true, SPS/PPS/IDR would be generated
    //   triggered by Coder construct, GopSize control & Size/format update + Time discontinuity as input metadata can trigger a new gop request)
    bool                                    FirstFrameInSequence;     //if true, means first frame in sequence, usefull information for HVA
    //   triggered by Coder construct & Time discontinuity metadata
    bool                                    ClosedGopType;            //today initialized as 'true' as only closed gop type supported
    H264EncodeHard_TransformParam_t         TransformParams;
    H264EncodeHard_AddInfo_CommandStatus_t  CommandStatus;
    H264EncodeHard_TransformerCapability_t  TransformCapability;
    MME_TransformerInitParams_t             InitParamsEncoder;
    MME_TransformerHandle_t                 TransformHandleEncoder;
    OS_Semaphore_t                          CompletionSemaphore;     // to signal end of encode
    uint32_t                                FrameCount;              // frame number
    uint32_t                                GopId;                   // same as frameNum, but resets only on I picture
    uint32_t                                GopSize;                 // Intra-coded picture periodicity
    stm_se_compressed_frame_metadata_t      OutputMetadata;          //to store output information before attachment to coded buffer
    uint32_t                                GlobalMMECommandCompletedCount;
    uint32_t                                TransformMMECommandCompletedCount;
    H264EncoderStatistics_t                 Statistics;
    bool                                    VppDeinterlacingOn;
    bool                                    VppNoiseFilteringOn;
} H264EncoderContext_t;

class Coder_Video_Mme_H264_c: public Coder_Video_c
{
public:
    Coder_Video_Mme_H264_c(void);
    ~Coder_Video_Mme_H264_c(void);

    void            CallbackFromMME(MME_Event_t Event, MME_Command_t *CallbackData);

    CoderStatus_t   Halt(void);

    CoderStatus_t   InitializeCoder(void);
    CoderStatus_t   TerminateCoder(void);
    CoderStatus_t   ManageMemoryProfile(void);
    CoderStatus_t   Input(Buffer_t Buffer);

    CoderStatus_t   SetControl(stm_se_ctrl_t Control, const void *Data);
    CoderStatus_t   GetControl(stm_se_ctrl_t Control, void *Data);
    CoderStatus_t   SetCompoundControl(stm_se_ctrl_t Control, const void *Data);
    CoderStatus_t   GetCompoundControl(stm_se_ctrl_t Control, void *Data);

    CoderStatus_t   LowPowerEnter(void);
    CoderStatus_t   LowPowerExit(void);

protected:
    H264EncoderContext_t  H264EncodeContext;

    unsigned int          MMECommandPreparedCount;              // Mme command counts
    unsigned int          MMECommandAbortedCount;
    unsigned int          MMECommandCompletedCount;
    bool                  MMECallbackPriorityBoosted;

    // CPS: indicates whether MME transformer was initialized when entering low power state
    bool                  IsLowPowerMMEInitialized;

    CoderStatus_t   InitializeMMETransformer(void);
    CoderStatus_t   TerminateMMETransformer(void);
    CoderStatus_t   InitializeMMESequenceParameters(void);
    CoderStatus_t   InitializeMMETransformParameters(void);
    CoderStatus_t   InitializeEncodeContext(void);
    CoderStatus_t   UpdateContextFromInputMetadata(__stm_se_frame_metadata_t *PreProcMetaDataDescriptor, uint32_t InputBufferSize);
    CoderStatus_t   UpdateContextFromControls(void);
    CoderStatus_t   UpdateContextFromTransformCapability(void);
    CoderStatus_t   FillOutputMetadata(__stm_se_frame_metadata_t *CoderMetaDataDescriptor);
    CoderStatus_t   SendMMEGlobalParamsCommandIfNeeded(void);
    CoderStatus_t   SendMMETransformCommand(void);
    CoderStatus_t   ComputeMMETransformParameters(Buffer_t FrameBuffer);
    void            UpdateMaxBitSizePerAU(void);
    void            EvaluateCodedFrameSize(uint32_t *CodedFrameSize);

    void            MonitorEncodeContext(void);
    void            MonitorOutputMetadata(__stm_se_frame_metadata_t *CoderMetaDataDescriptor);

    H264EncodeMemoryProfile_t   TranslateStkpiProfileToMmeProfile(VideoEncodeMemoryProfile_t MemoryProfile);

    CoderStatus_t   GetSliceMode(stm_se_video_multi_slice_t *SliceMode);
    CoderStatus_t   GetBitrateMode(uint32_t *BitrateMode);
    CoderStatus_t   GetLevel(uint32_t *Level);
    CoderStatus_t   GetProfile(uint32_t *Profile);
    CoderStatus_t   SetSliceMode(stm_se_video_multi_slice_t SliceMode);
    CoderStatus_t   SetBitrateMode(uint32_t BitrateMode);
    CoderStatus_t   SetLevel(uint32_t Level);
    CoderStatus_t   SetProfile(uint32_t Profile);

    void          DumpInputMetadata(stm_se_uncompressed_frame_metadata_t *Metadata);
    void          DumpEncodeCoordinatorMetadata(__stm_se_encode_coordinator_metadata_t *Metadata);
    CoderStatus_t CheckNativeTimeFormat(stm_se_time_format_t NativeTimeFormat, uint64_t NativeTime);
    CoderStatus_t CheckBufferAlignment(void *PhysicalAddress, void *VirtualAddress);
    CoderStatus_t CheckMedia(stm_se_encode_stream_media_t Media);
    CoderStatus_t CheckDisplayAspectRatio(stm_se_aspect_ratio_t AspectRatio);
    CoderStatus_t CheckScanType(stm_se_scan_type_t ScanType);
    CoderStatus_t CheckFormat3dType(stm_se_format_3d_t Format3dType);
    CoderStatus_t CheckFrameRate(stm_se_framerate_t Framerate);
    CoderStatus_t CheckAndUpdateColorspace(stm_se_colorspace_t Colorspace);
    CoderStatus_t CheckAndUpdateSurfaceFormat(surface_format_t SurfaceFormat, uint32_t Width, uint32_t Height, uint32_t InputBufferSize);
    int32_t       GetBitPerPixel(surface_format_t SurfaceFormat);
    int32_t       GetMinPitch(uint32_t Width, surface_format_t SurfaceFormat);
    CoderStatus_t CheckPitch(uint32_t Pitch, uint32_t Width, surface_format_t SurfaceFormat);
    CoderStatus_t CheckVerticalAlignment(uint32_t VerticalAlignment, uint32_t Height, uint32_t Pitch, uint32_t BufferSize);
    CoderStatus_t CheckPictureType(stm_se_picture_type_t PictureType);
    CoderStatus_t CheckLevelLimits(H264EncodeHard_SetGlobalParamSequence_t *Parameter);
    void          EntryTrace(H264EncoderContext_t *Context);
    void          ExitTrace(H264EncoderContext_t *Context);
};

// ---------------------------------------------------------------------
//
// Documentation
//

/*! \class Coder_Video_Mme_H264_c
\brief Responsible for managing hardware interfacing to the H264 Encoder.

H264 encoder work in macrobloc format, meaning input buffers should be aligned on 16 pixels.<br>
Involved MME H264 encoder is able to manage a request for non aligned 16 pixels resolution (like 1080p).<br>
In that case, MME will encode at upper 16 pixels aligned resolution (macrobloc constraint) and<br>
embedd native requested resolution as crop window in generated SPS of H264 stream<br>

In this perspective, Input supplied Buffer should be 16 pixels aligned and extra lines / column should be filled<br>
with a black pattern to ease encoding for the unused area.<br>
Preprocessor is responsible for this task.<br>

To deal with this, we must implement GetBufferPoolDescriptor to meet these constraints in the preprocessor pipeline.<br>

We must then decide how to deal with the extra 8 lines at the preprocessor, and have three options.
<pre>
      A             B             C
   [Frame]       [ Pad ]       [ Pad ]
   [Frame]       [Frame]       [Frame]
   [Frame]       [Frame]       [Frame]
   [ Pad ]       [ Pad ]       [Frame]

 High Frame   Centre Frame    Low Frame
</pre>

Realistically, only the High-Frame(A) or the Centre-Frame(B) would be used;
Where possible we will set the active window in the h264 stream, to inform the decoder of the active pixels.

We must ensure that any padding added to a frame by the pre-processor must be a uniform colour. I.e. black.
*/

#endif /* CODER_VIDEO_MME_H264_H */
