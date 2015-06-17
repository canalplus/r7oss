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

#include "player_threads.h"

#include "coder_video_mme_h264.h"
#include "H264ENCHW_VideoTransformerTypes.h"

#undef TRACE_TAG
#define TRACE_TAG "Coder_Video_Mme_H264_c"

#define MME_COMMAND_TIMEOUT_MS 1000

// Maximum and minimum supported resolution
#define CODER_WIDTH_MIN 16
#define CODER_HEIGHT_MIN 16
#define CODER_WIDTH_MAX 1920
#define CODER_HEIGHT_MAX 1088

// Required hardware buffer alignment
#define CODER_BUFFER_ADDRESS_ALIGN 32

static uint32_t GetLevelIndex(uint32_t levelIdc);
static uint32_t GetMinCR(uint32_t levelIdc);
static uint32_t ComputeMaxBitSizePerAU(uint32_t width, uint32_t height, uint32_t levelIdc);
static uint32_t ComputeMaxMBPS(uint32_t levelIdc);
static uint32_t ComputeMaxFS(uint32_t levelIdc);
static uint32_t GetCpbNalFactor(uint32_t profileIdc);
static uint32_t ComputeMaxBR(uint32_t profileIdc, uint32_t levelIdc);
static uint32_t ComputeMaxCPB(uint32_t profileIdc, uint32_t levelIdc);
static bool AreColorFormatMatching(surface_format_t SurfaceFormat, H264EncodeSamplingMode_t HVASamplingMode);
static bool AreColorSpaceMatching(stm_se_colorspace_t ColorSpace, H264EncodeVUIColorStd_t HVAVUIColorStd);
static bool AreFramerateMatching(uint16_t kpiFramerateNum, uint16_t kpiFramerateDen, uint16_t mmeFramerateNum, uint16_t mmeFramerateDen);
static bool AreAspectRatioMatching(uint32_t kpiAspectRatioNum, uint32_t kpiAspectRatioDen, uint32_t mmeAspectRatioNum, uint32_t mmeAspectRatioDen);
static void ReleaseBuffer(Buffer_t Buffer);

typedef struct H264AspectRatioValues_s
{
    H264EncodeVUIAspectRatio_t VUIAspectRatio;
    uint32_t                   AspectRatioNum;
    uint32_t                   AspectRatioDen;
} H264AspectRatioValues_t;

#define H264_ASPECT_RATIO_ENTRIES  17

H264AspectRatioValues_t H264AspectRatioList[H264_ASPECT_RATIO_ENTRIES] =
{
    {HVA_ENCODE_VUI_ASPECT_RATIO_UNSPECIFIED, PIXEL_ASPECT_RATIO_NUM_UNSPECIFIED, PIXEL_ASPECT_RATIO_DEN_UNSPECIFIED},
    {HVA_ENCODE_VUI_ASPECT_RATIO_1_1,      1,  1},
    {HVA_ENCODE_VUI_ASPECT_RATIO_12_11,   12, 11},
    {HVA_ENCODE_VUI_ASPECT_RATIO_10_11,   10, 11},
    {HVA_ENCODE_VUI_ASPECT_RATIO_16_11,   16, 11},
    {HVA_ENCODE_VUI_ASPECT_RATIO_40_33,   40, 33},
    {HVA_ENCODE_VUI_ASPECT_RATIO_24_11,   24, 11},
    {HVA_ENCODE_VUI_ASPECT_RATIO_20_11,   20, 11},
    {HVA_ENCODE_VUI_ASPECT_RATIO_32_11,   32, 11},
    {HVA_ENCODE_VUI_ASPECT_RATIO_80_33,   80, 33},
    {HVA_ENCODE_VUI_ASPECT_RATIO_18_11,   18, 11},
    {HVA_ENCODE_VUI_ASPECT_RATIO_15_11,   15, 11},
    {HVA_ENCODE_VUI_ASPECT_RATIO_64_33,   64, 33},
    {HVA_ENCODE_VUI_ASPECT_RATIO_160_99, 160, 99},
    {HVA_ENCODE_VUI_ASPECT_RATIO_4_3,      4,  3},
    {HVA_ENCODE_VUI_ASPECT_RATIO_3_2,      3,  2},
    {HVA_ENCODE_VUI_ASPECT_RATIO_2_1,      2,  1},
};

typedef void (*MME_GenericCallback_t)(MME_Event_t Event, MME_Command_t *CallbackData, void *UserData);
static void H264EncMMECallbackStub(MME_Event_t      Event,
                                   MME_Command_t   *CallbackData,
                                   void            *UserData)
{
    Coder_Video_Mme_H264_c         *Self = (Coder_Video_Mme_H264_c *)UserData;
    Self->CallbackFromMME(Event, CallbackData);
}

//{{{  Constructor
//{{{  doxynote
/// \brief                      Initialize configuration structure
/// \return                     Success
Coder_Video_Mme_H264_c::Coder_Video_Mme_H264_c(void)
    : Coder_Video_c()
    , H264EncodeContext()
    , MMECommandPreparedCount(0)
    , MMECommandAbortedCount(0)
    , MMECommandCompletedCount(0)
    , MMECallbackPriorityBoosted(false)
    , IsLowPowerMMEInitialized(false)
{
    // TODO(pht) move to a FinalizeInit method
    InitializationStatus = InitializeEncodeContext();
}
//}}}

//{{{  Destructor
//{{{  doxynote
/// \brief                      ensure full Halt and reset of the class
/// \return                     Success
Coder_Video_Mme_H264_c::~Coder_Video_Mme_H264_c(void)
{
    Halt();

    // do not rely on previous call to TerminateCoder
    if (H264EncodeContext.TransformHandleEncoder != NULL)
    {
        TerminateMMETransformer();
    }
}
//}}}

//{{{  Input
//{{{  doxynote
/// \brief                      inject a new frame buffer
/// \param Buffer               new Frame Buffer to be encoded
/// \return                     CoderStatus_t
CoderStatus_t Coder_Video_Mme_H264_c::Input(Buffer_t Buffer)
{
    __stm_se_frame_metadata_t *FullPreProcMetaDataDescriptor;
    __stm_se_frame_metadata_t *FullCoderMetaDataDescriptor;
    unsigned int DataSize;
    uint32_t     InputBufferSize;
    void        *InputBufferAddrPhys;
    void        *InputBufferAddrVirt;

    SE_DEBUG(group_encoder_video_coder, "\n");

    // Call the Base Class implementation to get new coded buffer and attach input frame buffer to this new coded buffer
    // Note1: InputMetadata already attached to input Frame Buffer
    // Note2: OutputMetadata already attached to coded buffer
    CoderStatus_t Status = Coder_Video_c::Input(Buffer);
    if (Status != CoderNoError)
    {
        CODER_ERROR_RUNNING("Unable to input frame buffer as generic processing\n");
        return CODER_STATUS_RUNNING(Status);
    }

    //Retrieve input metadata:
    Buffer->ObtainMetaDataReference(InputMetaDataBufferType, (void **)(&FullPreProcMetaDataDescriptor));
    SE_ASSERT(FullPreProcMetaDataDescriptor != NULL);

    // Dump input metadata
    DumpInputMetadata(&FullPreProcMetaDataDescriptor->uncompressed_frame_metadata);

    // Dump encode coordinator metadata
    DumpEncodeCoordinatorMetadata(&FullPreProcMetaDataDescriptor->encode_coordinator_metadata);

    // Generate Buffer EOS if Buffer EOS detected and exit
    if (CoderDiscontinuity)
    {
        if (CoderDiscontinuity & STM_SE_DISCONTINUITY_EOS)
        {
            CoderDiscontinuity = (stm_se_discontinuity_t)(CoderDiscontinuity & ~STM_SE_DISCONTINUITY_EOS);
            Status = GenerateBufferEOS(CodedFrameBuffer);
            if (Status != CoderNoError)
            {
                CODER_ERROR_RUNNING("Unable to generate EOS\n");
                ReleaseBuffer(CodedFrameBuffer);
            }
            ExitTrace(&H264EncodeContext);
            return CODER_STATUS_RUNNING(Status);
        }
        else
        {
            // Silently discard the frame
            ReleaseBuffer(CodedFrameBuffer);
            return CoderNoError;
        }
    }

    // Check native time format
    Status = CheckNativeTimeFormat(FullPreProcMetaDataDescriptor->uncompressed_frame_metadata.native_time_format, FullPreProcMetaDataDescriptor->uncompressed_frame_metadata.native_time);
    if (Status != CoderNoError)
    {
        SE_ERROR("Bad native time format, Status = %u\n", Status);
        ReleaseBuffer(CodedFrameBuffer);
        return Status;
    }

    // Check encoded time format
    Status = CheckNativeTimeFormat(FullPreProcMetaDataDescriptor->encode_coordinator_metadata.encoded_time_format, FullPreProcMetaDataDescriptor->encode_coordinator_metadata.encoded_time);
    if (Status != CoderNoError)
    {
        SE_ERROR("Bad encoded time format, Status = %u\n", Status);
        ReleaseBuffer(CodedFrameBuffer);
        return Status;
    }

    // Get buffer physical address
    Buffer->ObtainDataReference(NULL, &InputBufferSize, (void **)(&InputBufferAddrPhys), PhysicalAddress);
    if (InputBufferAddrPhys == NULL)
    {
        SE_ERROR("Unable to obtain buffer physical address\n");
        ReleaseBuffer(CodedFrameBuffer);
        return CoderError;
    }

    // Get buffer virtual address
    Buffer->ObtainDataReference(NULL, &InputBufferSize, (void **)(&InputBufferAddrVirt), CachedAddress);
    if (InputBufferAddrVirt == NULL)
    {
        SE_ERROR("Unable to obtain buffer virtual address\n");
        ReleaseBuffer(CodedFrameBuffer);
        return CoderError;
    }

    // Check that the input buffer address respects the H/W constraints
    Status = CheckBufferAlignment(InputBufferAddrPhys, InputBufferAddrVirt);
    if (Status != CoderNoError)
    {
        SE_ERROR("Bad buffer alignment, Status = %u\n", Status);
        ReleaseBuffer(CodedFrameBuffer);
        return Status;
    }

    // controls would be taken into account on the fly, updating context and requesting 'GlobalParamsNeedsUpdate' if suitable
    // Analyze input metadata to track a possible global parameters update
    Status = UpdateContextFromInputMetadata(FullPreProcMetaDataDescriptor, InputBufferSize);
    if (Status != CoderNoError)
    {
        SE_ERROR("Unable to update context from input metadata\n");
        ReleaseBuffer(CodedFrameBuffer);
        return Status;
    }

    // Analyze previous received controls and take them into account
    Status = UpdateContextFromControls();
    if (Status != CoderNoError)
    {
        SE_ERROR("Unable to take requested controls into account\n");
        ReleaseBuffer(CodedFrameBuffer);
        return Status;
    }

    // Launch the Sequence parameters MME command if needed (context related)
    Status = SendMMEGlobalParamsCommandIfNeeded();
    if (Status != CoderNoError)
    {
        SE_ERROR("Unable to send MME global command\n");
        ReleaseBuffer(CodedFrameBuffer);
        return Status;
    }

    // Compute MME Transform Parameters with provided buffer
    Status = ComputeMMETransformParameters(Buffer);
    if (Status != CoderNoError)
    {
        SE_ERROR("Unable to compute MME transform command\n");
        ReleaseBuffer(CodedFrameBuffer);
        return Status;
    }

    // Launch the Frame encode MME command, analyze processing status and manage context variables
    Status = SendMMETransformCommand();
    if (Status != CoderNoError)
    {
        SE_ERROR("error sending MME transform command\n");
        ReleaseBuffer(CodedFrameBuffer);
        return Status;
    }

    //For debug
    MonitorEncodeContext();

    //FIXME: TBD
    // Check command status management (frame skipped, error...)
    // Retrieve encode frame size from MME command status and fill info in coded buffer
    // Set coded buffer to the suitable encoded size
    DataSize = H264EncodeContext.CommandStatus.nonVCLNALUSize + H264EncodeContext.CommandStatus.bitstreamSize; // should be smaller than maximum size

    //Check encoded frame does not oversize coded buffer size
    if (DataSize > CodedFrameMaximumSize)
    {
        SE_ERROR("encoded frame oversizes coded buffer size\n");
        ReleaseBuffer(CodedFrameBuffer);
        return CoderError;
    }

    SE_DEBUG(group_encoder_video_coder, "H264 Coder buffer will be shrunk to compressed frame size = %d\n", DataSize);
    //
    //Fill encoded frame size info in coded buffer + shrink the coded buffer
    CodedFrameBuffer->SetUsedDataSize(DataSize);
    BufferStatus_t BufStatus = CodedFrameBuffer->ShrinkBuffer(max(DataSize, 1));
    if (BufStatus != BufferNoError)
    {
        SE_ERROR("Failed to shrink the operating buffer to size (%08x)\n", BufStatus);
        ReleaseBuffer(CodedFrameBuffer);
        return CoderError;
    }

    // Attach output metadata to coded buffer
    CodedFrameBuffer->ObtainMetaDataReference(OutputMetaDataBufferType, (void **)(&FullCoderMetaDataDescriptor));
    SE_ASSERT(FullCoderMetaDataDescriptor != NULL);

    //Fill output metadata, including control status
    Status = FillOutputMetadata(FullCoderMetaDataDescriptor);
    if (Status != CoderNoError)
    {
        SE_ERROR("Failed to fill coder output metadata\n");
        ReleaseBuffer(CodedFrameBuffer);
        return Status;
    }

    // Simply pass it on to the output ring - Input buffer to be detached there
    return Output(CodedFrameBuffer, H264EncodeContext.OutputMetadata.discontinuity & STM_SE_DISCONTINUITY_FRAME_SKIPPED ? true : false);
}
//}}}

CoderStatus_t Coder_Video_Mme_H264_c::CheckNativeTimeFormat(stm_se_time_format_t NativeTimeFormat, uint64_t NativeTime)
{
    switch (NativeTimeFormat)
    {
    case TIME_FORMAT_US:
        break;
    case TIME_FORMAT_PTS:
        if (NativeTime & (~PTS_MASK))
        {
            SE_ERROR("Unconsistent native time %llu, NativeTimeFormat = %u (%s)\n", NativeTime, NativeTimeFormat, StringifyTimeFormat(NativeTimeFormat));
            return CoderError;
        }
        break;
    case TIME_FORMAT_27MHz:
        break;
    default:
        SE_ERROR("Invalid native time format %u\n", NativeTimeFormat);
        return CoderError;
    }

    return CoderNoError;
}

CoderStatus_t Coder_Video_Mme_H264_c::CheckBufferAlignment(void *PhysicalAddress, void *VirtualAddress)
{
    // Check that the buffer physical and virtual address respects the alignment constraint, as required by video coder unit.
    if ((uint32_t)PhysicalAddress % CODER_BUFFER_ADDRESS_ALIGN != 0)
    {
        SE_ERROR("Physical address %p is not %d-bytes aligned - cannot be used by pre-processing unit\n", PhysicalAddress, CODER_BUFFER_ADDRESS_ALIGN);
        return CoderError;
    }
    if ((uint32_t)VirtualAddress % CODER_BUFFER_ADDRESS_ALIGN != 0)
    {
        SE_ERROR("Virtual address %p is not %d-bytes aligned - cannot be used by pre-processing unit\n", VirtualAddress, CODER_BUFFER_ADDRESS_ALIGN);
        return CoderError;
    }

    return CoderNoError;
}

CoderStatus_t Coder_Video_Mme_H264_c::CheckMedia(stm_se_encode_stream_media_t Media)
{
    switch (Media)
    {
    case STM_SE_ENCODE_STREAM_MEDIA_AUDIO:
        goto unsupported_media;
    case STM_SE_ENCODE_STREAM_MEDIA_VIDEO:
        break;
    case STM_SE_ENCODE_STREAM_MEDIA_ANY:
        goto unsupported_media;
    default:
        SE_ERROR("Invalid media %u\n", Media);
        return CoderNotSupported;
    }

    return CoderNoError;

unsupported_media:
    SE_ERROR("Unsupported media %u (%s)\n", Media, StringifyMedia(Media));
    return CoderNotSupported;
}

CoderStatus_t Coder_Video_Mme_H264_c::CheckDisplayAspectRatio(stm_se_aspect_ratio_t AspectRatio)
{
    switch (AspectRatio)
    {
    case STM_SE_ASPECT_RATIO_4_BY_3:
    case STM_SE_ASPECT_RATIO_16_BY_9:
    case STM_SE_ASPECT_RATIO_221_1:
        SE_ERROR("Unsupported aspect ratio %u (%s)\n", AspectRatio, StringifyAspectRatio(AspectRatio));
        return CoderNotSupported;
    case STM_SE_ASPECT_RATIO_UNSPECIFIED:
        break;
    default:
        SE_ERROR("Invalid aspect ratio %u\n", AspectRatio);
        return CoderNotSupported;
    }

    return CoderNoError;
}

CoderStatus_t Coder_Video_Mme_H264_c::CheckScanType(stm_se_scan_type_t ScanType)
{
    switch (ScanType)
    {
    case STM_SE_SCAN_TYPE_PROGRESSIVE:
        break;
    case STM_SE_SCAN_TYPE_INTERLACED:
        SE_ERROR("Unsupported scan type %u (%s)\n", ScanType, StringifyScanType(ScanType));
        return CoderNotSupported;
    default:
        SE_ERROR("Invalid scan type %u\n", ScanType);
        return CoderNotSupported;
    }

    return CoderNoError;
}

CoderStatus_t Coder_Video_Mme_H264_c::CheckFormat3dType(stm_se_format_3d_t Format3dType)
{
    switch (Format3dType)
    {
    case STM_SE_FORMAT_3D_NONE:
        break;
    case STM_SE_FORMAT_3D_FRAME_SEQUENTIAL:
    case STM_SE_FORMAT_3D_STACKED_HALF:
    case STM_SE_FORMAT_3D_SIDE_BY_SIDE_HALF:
        SE_ERROR("Unsupported 3d format %u (%s)\n", Format3dType, StringifyFormat3d(Format3dType));
        return CoderNotSupported;
    default:
        SE_ERROR("Invalid 3d format %u\n", Format3dType);
        return CoderNotSupported;
    }

    return CoderNoError;
}

CoderStatus_t Coder_Video_Mme_H264_c::CheckFrameRate(stm_se_framerate_t Framerate)
{
    if ((Framerate.framerate_num == 0) || (Framerate.framerate_den == 0))
    {
        SE_ERROR("Invalid framerate metadata %u/%u\n", Framerate.framerate_num, Framerate.framerate_den);
        return CoderError;
    }

    // Coder only support integer framerate
    if ((Framerate.framerate_num % Framerate.framerate_den) != 0)
    {
        SE_ERROR("Unsupported framerate metadata %u/%u\n", Framerate.framerate_num, Framerate.framerate_den);
        return CoderError;
    }

    return CoderNoError;
}

CoderStatus_t Coder_Video_Mme_H264_c::CheckAndUpdateColorspace(stm_se_colorspace_t Colorspace)
{
    switch (Colorspace)
    {
    case STM_SE_COLORSPACE_UNSPECIFIED:
        H264EncodeContext.GlobalParams.vuiColorStd = HVA_ENCODE_VUI_COLOR_STD_UNSPECIFIED;
        break;
    case STM_SE_COLORSPACE_SMPTE170M:
        H264EncodeContext.GlobalParams.vuiColorStd = HVA_ENCODE_VUI_COLOR_STD_SMPTE_170M;
        break;
    case STM_SE_COLORSPACE_SMPTE240M:
        H264EncodeContext.GlobalParams.vuiColorStd = HVA_ENCODE_VUI_COLOR_STD_SMPTE_240M;
        break;
    case STM_SE_COLORSPACE_BT709:
        H264EncodeContext.GlobalParams.vuiColorStd = HVA_ENCODE_VUI_COLOR_STD_BT_709_5;
        break;
    case STM_SE_COLORSPACE_BT470_SYSTEM_M:
        H264EncodeContext.GlobalParams.vuiColorStd = HVA_ENCODE_VUI_COLOR_STD_BT_470_6_M;
        break;
    case STM_SE_COLORSPACE_BT470_SYSTEM_BG:
        H264EncodeContext.GlobalParams.vuiColorStd = HVA_ENCODE_VUI_COLOR_STD_BT_470_6_BG;
        break;
    case STM_SE_COLORSPACE_SRGB:
        H264EncodeContext.GlobalParams.vuiColorStd = HVA_ENCODE_VUI_COLOR_STD_UNSPECIFIED;
        break;
    default:
        SE_ERROR("Invalid colorspace %u\n", Colorspace);
        return CoderNotSupported;
    }

    return CoderNoError;
}

CoderStatus_t Coder_Video_Mme_H264_c::CheckAndUpdateSurfaceFormat(surface_format_t SurfaceFormat, uint32_t Width, uint32_t Height, uint32_t InputBufferSize)
{
    switch (SurfaceFormat)
    {
    case SURFACE_FORMAT_UNKNOWN:
    case SURFACE_FORMAT_MARKER_FRAME:
    case SURFACE_FORMAT_AUDIO:
    case SURFACE_FORMAT_VIDEO_420_MACROBLOCK:
    case SURFACE_FORMAT_VIDEO_420_PAIRED_MACROBLOCK:
        goto unsupported_color_format;
    case SURFACE_FORMAT_VIDEO_422_RASTER:
        if ((Width * Height * GetBitPerPixel(SurfaceFormat) / 8) > InputBufferSize)
        {
            goto inconsistent_color_format;
        }
        H264EncodeContext.GlobalParams.samplingMode = HVA_ENCODE_YUV422_RASTER;
        break;
    case SURFACE_FORMAT_VIDEO_420_PLANAR:
    case SURFACE_FORMAT_VIDEO_420_PLANAR_ALIGNED:
    case SURFACE_FORMAT_VIDEO_422_PLANAR:
    case SURFACE_FORMAT_VIDEO_8888_ARGB:
    case SURFACE_FORMAT_VIDEO_888_RGB:
    case SURFACE_FORMAT_VIDEO_565_RGB:
    case SURFACE_FORMAT_VIDEO_422_YUYV:
        goto unsupported_color_format;
    case SURFACE_FORMAT_VIDEO_420_RASTER2B:
        if ((Width * Height * GetBitPerPixel(SurfaceFormat) / 8) > InputBufferSize)
        {
            goto inconsistent_color_format;
        }
        H264EncodeContext.GlobalParams.samplingMode = HVA_ENCODE_YUV420_SEMI_PLANAR;
        break;
    default:
        SE_ERROR("Invalid surface format %u\n", SurfaceFormat);
        return CoderNotSupported;
    }

    return CoderNoError;

unsupported_color_format:
    SE_ERROR("Unsupported surface format %u (%s)\n", SurfaceFormat, StringifySurfaceFormat(SurfaceFormat));
    return CoderNotSupported;

inconsistent_color_format:
    SE_ERROR("Inconsistent surface format %u (%s), Width = %u, Height = %u, InputBufferSize = %u\n", SurfaceFormat, StringifySurfaceFormat(SurfaceFormat), Width, Height, InputBufferSize);
    return CoderNotSupported;
}

int32_t Coder_Video_Mme_H264_c::GetBitPerPixel(surface_format_t SurfaceFormat)
{
    int32_t bitPerPixel = 0;

    switch (SurfaceFormat)
    {
    case SURFACE_FORMAT_VIDEO_422_RASTER:
        bitPerPixel = 16;
        break;
    case SURFACE_FORMAT_VIDEO_420_RASTER2B:
        bitPerPixel = 12;
        break;
    default:
        SE_ERROR("Unsupported surface format %u\n", SurfaceFormat);
        return -1;
    }

    return bitPerPixel;
}

int32_t Coder_Video_Mme_H264_c::GetMinPitch(uint32_t Width, surface_format_t SurfaceFormat)
{
    int32_t minPitch = 0;

    switch (SurfaceFormat)
    {
    case SURFACE_FORMAT_VIDEO_422_RASTER:
        minPitch = Width * 2;
        break;
    case SURFACE_FORMAT_VIDEO_420_RASTER2B:
        minPitch = Width;
        break;
    default:
        SE_ERROR("Unsupported surface format %u\n", SurfaceFormat);
        return -1;
    }

    return minPitch;
}

CoderStatus_t Coder_Video_Mme_H264_c::CheckPitch(uint32_t Pitch, uint32_t Width, surface_format_t SurfaceFormat)
{
    if (Pitch != GetMinPitch(Width, SurfaceFormat))
    {
        SE_ERROR("Invalid pitch %u, Width = %u, SurfaceFormat = %u (%s)\n", Pitch, Width, SurfaceFormat, StringifySurfaceFormat(SurfaceFormat));
        return CoderNotSupported;
    }

    return CoderNoError;
}

CoderStatus_t Coder_Video_Mme_H264_c::CheckVerticalAlignment(uint32_t VerticalAlignment, uint32_t Height, uint32_t Pitch, uint32_t BufferSize)
{
    uint32_t ExpectedDataSize = 0;

    if (VerticalAlignment != 1)
    {
        SE_ERROR("Invalid vertical alignment %u\n", VerticalAlignment);
        return CoderNotSupported;
    }

    ExpectedDataSize = 3 * Height * Pitch / 2;
    if (ExpectedDataSize > BufferSize)
    {
        SE_ERROR("Invalid vertical alignment %u (ExpectedDataSize = %u, BufferSize = %u)\n", VerticalAlignment, ExpectedDataSize, BufferSize);
        return CoderNotSupported;
    }

    return CoderNoError;
}

CoderStatus_t Coder_Video_Mme_H264_c::CheckPictureType(stm_se_picture_type_t PictureType)
{
    switch (PictureType)
    {
    case STM_SE_PICTURE_TYPE_UNKNOWN:
    case STM_SE_PICTURE_TYPE_I:
    case STM_SE_PICTURE_TYPE_P:
    case STM_SE_PICTURE_TYPE_B:
        break;
    default:
        SE_ERROR("Invalid picture type %u\n", PictureType);
        return CoderNotSupported;
    }

    return CoderNoError;
}

CoderStatus_t Coder_Video_Mme_H264_c::CheckLevelLimits(H264EncodeHard_SetGlobalParamSequence_t *Parameter)
{
    uint32_t profile = Parameter->profileIdc;
    uint32_t level = Parameter->levelIdc;
    uint32_t currentFS = 0;
    uint32_t currentMBPS = 0;
    uint32_t currentBR = 0;
    uint32_t currentCPB = 0;

    currentFS = ((Parameter->frameWidth + 15) / 16) * ((Parameter->frameHeight + 15) / 16);
    if (currentFS > ComputeMaxFS(level))
    {
        SE_ERROR("Current frame size in mb %u is not inline with level %u (%s) on h264 standard, MaxFS = %u\n",
                 currentFS, level, StringifyLevel(level), ComputeMaxFS(level));
        return CoderError;
    }

    if (Parameter->framerateDen != 0)
    {
        currentMBPS = currentFS * Parameter->framerateNum / Parameter->framerateDen;
        if (currentMBPS > ComputeMaxMBPS(level))
        {
            SE_ERROR("Current macroblock per second %u is not inline with level %u (%s) on h264 standard, MaxMBPS = %u\n",
                     currentMBPS, level, StringifyLevel(level), ComputeMaxMBPS(level));
            return CoderError;
        }
    }

    currentBR = Parameter->bitRate;
    if (currentBR > ComputeMaxBR(profile, level))
    {
        SE_ERROR("Current bitrate %u is not inline with profile %u (%s) and level %u (%s) on h264 standard, MaxBR = %u\n",
                 currentBR, profile, StringifyProfile(profile), level, StringifyLevel(level), ComputeMaxBR(profile, level));
        return CoderError;
    }

    currentCPB = Parameter->cpbBufferSize;
    if (currentCPB > ComputeMaxCPB(profile, level))
    {
        SE_ERROR("Current coded picture buffer size %u is not inline with profile %u (%s) and level %u (%s) on h264 standard, MaxCPB = %u\n",
                 currentCPB, profile, StringifyProfile(profile), level, StringifyLevel(level), ComputeMaxCPB(profile, level));
        return CoderError;
    }

    if ((Parameter->brcType == HVA_ENCODE_CBR) || (Parameter->brcType == HVA_ENCODE_VBR))
    {
        if ((Parameter->bitRate != 0) && (Parameter->framerateDen != 0))
        {
            uint32_t compressionRatio = Parameter->framerateNum * ALIGN_UP(Parameter->frameWidth, 16) *
                                        ALIGN_UP(Parameter->frameHeight, 16) * HvaGetBitPerPixel(Parameter->samplingMode) /
                                        Parameter->bitRate / Parameter->framerateDen;
            if ((compressionRatio < GetMinCR(Parameter->levelIdc)) || (compressionRatio > 500))
            {
                SE_WARNING_ONCE("Inconsistent compression ratio (%u) => please check video encoder parameters\n", compressionRatio);
            }
        }
        else
        {
            SE_ERROR_ONCE("Incorrect %s\n", Parameter->bitRate ? "framerate" : "bitrate");
        }
    }

    return CoderNoError;
}

void Coder_Video_Mme_H264_c::EntryTrace(H264EncoderContext_t *Context)
{
    if (Context != NULL)
    {
        SE_INFO(group_encoder_video_coder,
                "[H264ENC] > %ux%u pixels, P%u (%s), L%u (%s), (%u/%u) fps, %u kbps, %u kbits, GOP %u\n",
                Context->GlobalParams.frameWidth,
                Context->GlobalParams.frameHeight,
                Context->GlobalParams.profileIdc,
                StringifyProfile(Context->GlobalParams.profileIdc),
                Context->GlobalParams.levelIdc,
                StringifyLevel(Context->GlobalParams.levelIdc),
                Context->GlobalParams.framerateNum,
                Context->GlobalParams.framerateDen,
                Context->GlobalParams.bitRate / 1000,
                Context->GlobalParams.cpbBufferSize / 1000,
                Context->GopSize);
    }
}

void Coder_Video_Mme_H264_c::ExitTrace(H264EncoderContext_t *Context)
{
    if (Context != NULL)
    {
        SE_INFO(group_encoder_video_coder,
                "[H264ENC] < %ux%u pixels, P%d (%s), L%d (%s), (%u/%u) fps, %u kbps, %u kbits, GOP %u, %u encoded, %u skipped, %u<DT<%u us, %u<DS<%u bytes, TOTAL %u us\n",
                Context->GlobalParams.frameWidth,
                Context->GlobalParams.frameHeight,
                Context->GlobalParams.profileIdc,
                StringifyProfile(Context->GlobalParams.profileIdc),
                Context->GlobalParams.levelIdc,
                StringifyLevel(Context->GlobalParams.levelIdc),
                Context->GlobalParams.framerateNum,
                Context->GlobalParams.framerateDen,
                Context->GlobalParams.bitRate / 1000,
                Context->GlobalParams.cpbBufferSize / 1000,
                Context->GopSize,
                Context->Statistics.EncodeNumber - 1,
                Context->FrameCount - H264EncodeContext.Statistics.EncodeNumber + 1,
                Context->Statistics.MinEncodeTime,
                Context->Statistics.MaxEncodeTime,
                Context->Statistics.MinEncodeSize,
                Context->Statistics.MaxEncodeSize,
                Context->Statistics.EncodeSumTime);
    }
}

//FIXME: to be upgraded
CoderStatus_t   Coder_Video_Mme_H264_c::Halt(void)
{
    // Call the base class to halt the Input() calls and commence Discard.
    Coder_Video_c::Halt();
    SE_DEBUG(group_encoder_video_coder, "\n");
    return CoderNoError;
}

//{{{  InitializeCoder
//{{{  doxynote
/// \brief                      called by Coder_Base_c::RegisterOutputBufferRing, init MME transformer in particular
/// \return                     CoderStatus_t
CoderStatus_t   Coder_Video_Mme_H264_c::InitializeCoder(void)
{
    //perform all init task including start of MME transformer
    CoderStatus_t Status = InitializeMMETransformer();
    return Status;
}

//{{{  TerminateCoder
//{{{  doxynote
/// \brief                      called by EncodeStream_c::StopStream, terminate MME transformer in particular
/// \return                     CoderStatus_t
CoderStatus_t   Coder_Video_Mme_H264_c::TerminateCoder(void)
{
    SE_DEBUG(group_encoder_video_coder, "\n");
    CoderStatus_t Status = TerminateMMETransformer();
    return Status;
}

CoderStatus_t   Coder_Video_Mme_H264_c::ManageMemoryProfile(void)
{
    uint32_t CodedFrameSize;
    EvaluateCodedFrameSize(&CodedFrameSize);
    SetFrameMemory(CodedFrameSize, ENCODER_STREAM_VIDEO_MAX_CODED_BUFFERS);

    SE_DEBUG(group_encoder_video_coder, "Computed CodedFrameSize from profile = %u\n", CodedFrameSize);

    return CoderNoError;
}

void Coder_Video_Mme_H264_c::EvaluateCodedFrameSize(uint32_t *CodedFrameSize)
{
    VideoEncodeMemoryProfile_t MemoryProfile = Encoder.Encode->GetVideoEncodeMemoryProfile();

    switch (MemoryProfile)
    {
    //Worst case theorical size is 400 x "Number of macroblocs" in bytes
    case EncodeProfileHD:
    //3264000 bytes
    case EncodeProfile720p:
    //1440000 bytes
    case EncodeProfileSD:
    //648000 bytes
    case EncodeProfileCIF:
        //158400 bytes
        *CodedFrameSize = (400 * (ProfileSize[MemoryProfile].width / 16) * (ProfileSize[MemoryProfile].height / 16)) + H264_ENC_MAX_HEADER_SIZE;
        SE_INFO(group_encoder_video_coder, "Memory profile = %u (%s), coder buffer size = %u\n", MemoryProfile, StringifyMemoryProfile(MemoryProfile), *CodedFrameSize);
        break;
    default:
        *CodedFrameSize = (400 * (ProfileSize[EncodeProfileHD].width / 16) * (ProfileSize[EncodeProfileHD].height / 16)) + H264_ENC_MAX_HEADER_SIZE;
        SE_WARNING("Invalid internal memory profile, set then to HD profile\n");
        break;
    }
}

CoderStatus_t   Coder_Video_Mme_H264_c::InitializeMMETransformer(void)
{
    MME_TransformerCapability_t MMECapability;
    H264EncodeHardInitParams_t MMEInitParams;
    VideoEncodeMemoryProfile_t MemoryProfile = Encoder.Encode->GetVideoEncodeMemoryProfile();

    SE_DEBUG(group_encoder_video_coder, "memory profile = %u\n", MemoryProfile);
    //Update MME init from known profile
    MMEInitParams.MemoryProfile = TranslateStkpiProfileToMmeProfile(MemoryProfile);
    H264EncodeContext.InitParamsEncoder.TransformerInitParams_p = (MME_GenericParams_t *)&MMEInitParams;

    //FIXME: add capabilities check
    //context already initialize
    MMECommandPreparedCount    = 0;
    MMECommandAbortedCount     = 0;
    MMECommandCompletedCount   = 0;

    //Init MME transformer
    MME_ERROR MMEStatus = MME_InitTransformer(H264ENCHW_MME_TRANSFORMER_NAME,
                                              &H264EncodeContext.InitParamsEncoder, &H264EncodeContext.TransformHandleEncoder);
    if (MMEStatus != MME_SUCCESS)
    {
        SE_ERROR("Unable to initialize H264 encode Transformer\n");
        return CoderError;
    }

    //we work in a 'blocking context'
    OS_SemaphoreInitialize(&H264EncodeContext.CompletionSemaphore, 0);

    MMECapability.StructSize          = sizeof(MME_TransformerCapability_t);
    MMECapability.TransformerInfoSize = sizeof(H264EncodeHard_TransformerCapability_t);
    MMECapability.TransformerInfo_p   = &H264EncodeContext.TransformCapability;
    MMEStatus = MME_GetTransformerCapability(H264ENCHW_MME_TRANSFORMER_NAME, &MMECapability);
    if (MMEStatus != MME_SUCCESS)
    {
        SE_ERROR("Unable to get transformer capability\n");
        return CoderError;
    }

    // We call this once at initialization
    // Default profile control is overwritten
    UpdateContextFromTransformCapability();
    MMECallbackPriorityBoosted        = false;
    return CoderNoError;
}

H264EncodeMemoryProfile_t   Coder_Video_Mme_H264_c::TranslateStkpiProfileToMmeProfile(VideoEncodeMemoryProfile_t MemoryProfile)
{
    switch (MemoryProfile)
    {
    case EncodeProfileCIF:
        return HVA_ENCODE_PROFILE_CIF;
    case EncodeProfileSD:
        return HVA_ENCODE_PROFILE_SD;
    case EncodeProfile720p:
        return HVA_ENCODE_PROFILE_720P;
    case EncodeProfileHD:
        return HVA_ENCODE_PROFILE_HD;
    default:
        SE_ERROR("STKPI MemoryProfile does not map to an HVA MME memory profile! Use HD profile\n");
        return HVA_ENCODE_PROFILE_HD;
    }
}

CoderStatus_t   Coder_Video_Mme_H264_c::TerminateMMETransformer(void)
{
    MME_ERROR   MMEStatus;
    CoderStatus_t ret = CoderNoError;
    unsigned int i;
    unsigned int MaxTimeToWait;
    // The H264 Coder currently only handles a single Context per stream.
    const unsigned int StreamParameterContextCount = 1;
    const unsigned int EncodeContextCount = 1;

    //
    // Wait a reasonable time for all MME transactions to terminate
    //
    MaxTimeToWait   = StreamParameterContextCount + EncodeContextCount * CODER_MAX_WAIT_FOR_MME_COMMAND_COMPLETION;

    for (i = 0; ((i < MaxTimeToWait / 10) && (MMECommandPreparedCount > (MMECommandCompletedCount + MMECommandAbortedCount))); i++)
    {
        OS_SleepMilliSeconds(10);
    }

    if (MMECommandPreparedCount > (MMECommandCompletedCount + MMECommandAbortedCount))
    {
        SE_ERROR("Transformer failed to complete %d commands in %dms\n",
                 (MMECommandPreparedCount - MMECommandCompletedCount - MMECommandAbortedCount), MaxTimeToWait);
    }

    //
    // Terminate the transformer if it exists
    //
    if (H264EncodeContext.TransformHandleEncoder != NULL)
    {
        MMEStatus = MME_TermTransformer(H264EncodeContext.TransformHandleEncoder);
        if (MMEStatus != MME_SUCCESS)
        {
            SE_ERROR("Unable to terminate H264 encode Transformer (%s)\n", MME_ErrorStr(MMEStatus));
            ret = CoderError;
        }

        H264EncodeContext.TransformHandleEncoder = NULL;
        OS_SemaphoreTerminate(&H264EncodeContext.CompletionSemaphore);
    }
    return ret;
}

//{{{  InitializeEncodeContext
//{{{  doxynote
/// \brief                      initialize H264EncoderContext_t structure
/// \return                     CoderStatus_t
CoderStatus_t Coder_Video_Mme_H264_c::InitializeEncodeContext()
{
    H264EncodeHardInitParams_t MMEInitParams;
    SE_DEBUG(group_encoder_video_coder, "\n");

    //Initialize 'GlobalParams' field of H264EncoderContext_t
    InitializeMMESequenceParameters();
    H264EncodeContext.ControlsValues.BitRateControlMode            = (H264EncodeBrcType_t) TRANSLATE_BRC_MODE_STKPI_TO_MME(H264_ENC_DEFAULT_BITRATE_MODE);
    H264EncodeContext.ControlsValues.BitRate                       = H264_ENC_DEFAULT_BITRATE;
    H264EncodeContext.ControlsValues.BitRateSetByClient            = false;
    H264EncodeContext.ControlsValues.GopSize                       = H264_ENC_DEFAULT_GOP_SIZE;
    H264EncodeContext.ControlsValues.H264Level                     = (H264EncodeSPSLevelIDC_t)(H264_ENC_DEFAULT_LEVEL);
    H264EncodeContext.ControlsValues.H264Profile                   = (H264EncodeSPSProfileIDC_t)(H264_ENC_DEFAULT_PROFILE);
    H264EncodeContext.ControlsValues.CodedPictureBufferSize        = H264_ENC_DEFAULT_CPB_BUFFER_SIZE;
    H264EncodeContext.ControlsValues.CpbSizeSetByClient            = false;
    H264EncodeContext.ControlsValues.SliceMode                     = HVA_ENCODE_SLICE_MODE_SINGLE;
    H264EncodeContext.ControlsValues.SliceMaxMbSize                = 0;
    H264EncodeContext.ControlsValues.SliceMaxByteSize              = 0;
    H264EncodeContext.GlobalParamsNeedsUpdate                      = true;
    H264EncodeContext.ControlStatusUpdated                         = false;
    H264EncodeContext.NewGopRequested                              = true;
    H264EncodeContext.FirstFrameInSequence                         = true;
    H264EncodeContext.ClosedGopType                                = true;
    //Initialize 'TransformParams' field of H264EncoderContext_t
    InitializeMMETransformParameters();
    //'CommandStatus' already reseted to '0' with previous memset
    //Initialize 'InitParamsEncoder' field of H264EncoderContext_t
    H264EncodeContext.InitParamsEncoder.StructSize                 = sizeof(MME_TransformerInitParams_t);
    H264EncodeContext.InitParamsEncoder.Priority                   = MME_PRIORITY_HIGHEST;
    H264EncodeContext.InitParamsEncoder.Callback                   = H264EncMMECallbackStub;
    //object address is used as callback user data: context can be retrieved easily
    H264EncodeContext.InitParamsEncoder.CallbackUserData           = this;
    H264EncodeContext.InitParamsEncoder.TransformerInitParamsSize  = sizeof(MMEInitParams);
    H264EncodeContext.InitParamsEncoder.TransformerInitParams_p    = (MME_GenericParams_t *)&MMEInitParams; // FIXME local ptr?
    //FIXME: to be updated to '0' when v4l2 device updated!
    H264EncodeContext.GopSize                                      = H264_ENC_DEFAULT_GOP_SIZE;
    //Initialize semaphore: move to InitializeMMETransformer() to avoid failure in 'Coder_Video_Mme_H264_c' constructor
    //All fields set to '0' following memset() call
    H264EncodeContext.OutputMetadata.encoding                      = STM_SE_ENCODE_STREAM_ENCODING_VIDEO_H264;
    //Initialize statistics suitable fields for suitable min() behaviour
    H264EncodeContext.Statistics.MinEncodeTime                     = 0xFFFFFFFF;
    H264EncodeContext.Statistics.MinEncodeSize                     = 0xFFFFFFFF;

    return CoderNoError;
}
//}}}

//{{{  InitializeMMESequenceParameters
//{{{  doxynote
/// \brief                      initialize MME global/sequence parameters
/// \return                     CoderStatus_t
CoderStatus_t Coder_Video_Mme_H264_c::InitializeMMESequenceParameters()
{
    SE_DEBUG(group_encoder_video_coder, "\n");
    //Fill 'H264EncodeHard_SetGlobalParamSequence_t' structure of 'H264EncoderContext_t' with default parameters
    //Some parameters would be updated later, taking into account stkpi controls (bitrate...) or buffer's input metadata (resolution, framerate...)
    //Resolution will be updated with input Buffer metadata
    H264EncodeContext.GlobalParams.frameWidth              = 0;
    H264EncodeContext.GlobalParams.frameHeight             = 0;
    //picOrderCntType hard coded to '2' as only I & P frames
    H264EncodeContext.GlobalParams.picOrderCntType         = 2;
    H264EncodeContext.GlobalParams.log2MaxFrameNumMinus4   = 0;
    //useConstrainedIntraFlag set to false for better coding efficiency
    H264EncodeContext.GlobalParams.useConstrainedIntraFlag = false;
    //Intra refresh disabled
    H264EncodeContext.GlobalParams.intraRefreshType        = HVA_ENCODE_DISABLE_INTRA_REFRESH;
    H264EncodeContext.GlobalParams.irParamOption           = 0;
    //max size in bits for an Access Unit, passed to the BRC to limit the size of pictures
    //Warning, this is dependant from width, height and level: should be then computed if one of those is updated
    H264EncodeContext.GlobalParams.maxSumNumBitsInNALU     = 0;
    //BRC type hard coded to VBR
    H264EncodeContext.GlobalParams.brcType                 = (H264EncodeBrcType_t) TRANSLATE_BRC_MODE_STKPI_TO_MME(H264_ENC_DEFAULT_BITRATE_MODE);
    //Coded Picture Buffer size initialized to 4Mb, can be updated later with suitable control
    H264EncodeContext.GlobalParams.cpbBufferSize           = H264_ENC_DEFAULT_CPB_BUFFER_SIZE;
    //Bitrate information initialized to 4Mb, can be updated later with suitable control
    H264EncodeContext.GlobalParams.bitRate                 = H264_ENC_DEFAULT_BITRATE;
    //FrameRate information will be updated if suitable from frame input metadata
    H264EncodeContext.GlobalParams.framerateNum            = H264_ENC_DEFAULT_FRAMERATE_NUM;
    H264EncodeContext.GlobalParams.framerateDen            = H264_ENC_DEFAULT_FRAMERATE_DEN;
    //transformMode T8x8
    H264EncodeContext.GlobalParams.transformMode           = HVA_ENCODE_NO_T8x8_ALLOWED;
    //encoderComplexity hard coded to max complexity for better coding performances
    H264EncodeContext.GlobalParams.encoderComplexity       = HVA_ENCODE_I_16x16_I_NxN_P_16x16_P_WxH;
    H264EncodeContext.GlobalParams.vbrInitQp               = H264_ENC_DEFAULT_VBR_INIT_QP;
    //samplingMode will be updated with input Buffer metadata
    H264EncodeContext.GlobalParams.samplingMode            = HVA_ENCODE_YUV422_RASTER;
    //don't use CBR for the time being
    H264EncodeContext.GlobalParams.cbrDelay                = 0;
    //Hard code Qpmin and Qpmax
    H264EncodeContext.GlobalParams.qpmin                   = H264_ENC_DEFAULT_QP_MIN;
    H264EncodeContext.GlobalParams.qpmax                   = H264_ENC_DEFAULT_QP_MAX;
    //sliceNumber hard coded to '1'
    //FIXME: to upgrade
    H264EncodeContext.GlobalParams.sliceNumber             = 1;
    H264EncodeContext.GlobalParams.profileIdc                 = HVA_ENCODE_SPS_PROFILE_IDC_BASELINE;     // relates with SPS encoding profile
    H264EncodeContext.GlobalParams.levelIdc                   = HVA_ENCODE_SPS_LEVEL_IDC_4_2;            // relates with SPS encoding level
    H264EncodeContext.GlobalParams.vuiParametersPresentFlag   = true;                                    // relates with SPS VUI parameter presence
    H264EncodeContext.GlobalParams.vuiAspectRatioIdc          = HVA_ENCODE_VUI_ASPECT_RATIO_UNSPECIFIED; // relates with VUI aspact ratio
    H264EncodeContext.GlobalParams.vuiSarWidth                = PIXEL_ASPECT_RATIO_NUM_UNSPECIFIED;      // relates with VUI aspact ratio
    H264EncodeContext.GlobalParams.vuiSarHeight               = PIXEL_ASPECT_RATIO_DEN_UNSPECIFIED;      // relates with VUI aspact ratio
    H264EncodeContext.GlobalParams.vuiOverscanAppropriateFlag = false;                                   // relates with VUI overscan
    H264EncodeContext.GlobalParams.vuiVideoFormat             = HVA_ENCODE_VUI_VIDEO_FORMAT_UNSPECIFIED; // relates with VUI video format
    H264EncodeContext.GlobalParams.vuiVideoFullRangeFlag      = false;                                   // relates with VUI full or standard range
    H264EncodeContext.GlobalParams.vuiColorStd                = HVA_ENCODE_VUI_COLOR_STD_UNSPECIFIED;    // relates with VUI color primaries, transfer characteristics and matrix coefficients
    H264EncodeContext.GlobalParams.seiPicTimingPresentFlag    = true;                                    // relates with SEI picture timng
    H264EncodeContext.GlobalParams.seiBufPeriodPresentFlag    = true;                                    // relates with SEI buffering period
    return CoderNoError;
}
//}}}


//{{{  SendGlobalParamsCommand
//{{{  doxynote
/// \brief                      Perform a MME_SET_GLOBAL_TRANSFORM_PARAMS MME command
/// \brief                      H264EncodeContext.GlobalParamsNeedsUpdate has to be tested previously
/// \return                     CoderStatus_t
CoderStatus_t Coder_Video_Mme_H264_c::SendMMEGlobalParamsCommandIfNeeded()
{
    CoderStatus_t Status = CoderNoError;
    MME_Command_t MmeCommand;
    MME_ERROR MmeError;
    SE_DEBUG(group_encoder_video_coder, "\n");

    //
    // Check that we have not commenced shutdown.
    //

    if (TestComponentState(ComponentHalted))
    {
        return CoderNoError;
    }

    if (H264EncodeContext.GlobalParamsNeedsUpdate == true)
    {
        if (H264EncodeContext.FirstFrameInSequence == 1)
        {
            EntryTrace(&H264EncodeContext);
        }

        SE_DEBUG(group_encoder_video_coder, "New Input metadata requires encode context to be updated\n");
        // clear command status
        memset((void *)&H264EncodeContext.CommandStatus, 0, sizeof(H264EncodeHard_AddInfo_CommandStatus_t));
        MmeCommand.StructSize  = sizeof(MME_Command_t);
        MmeCommand.CmdEnd      = MME_COMMAND_END_RETURN_NOTIFY;
        MmeCommand.DueTime     = 0;
        MmeCommand.ParamSize   = sizeof(H264EncodeHard_SetGlobalParamSequence_t);
        MmeCommand.Param_p     = &H264EncodeContext.GlobalParams;
        MmeCommand.CmdCode     = MME_SET_GLOBAL_TRANSFORM_PARAMS;
        MmeCommand.NumberInputBuffers  = 0;
        MmeCommand.NumberOutputBuffers = 0;
        MmeCommand.DataBuffers_p = NULL;
        MmeCommand.CmdStatus.AdditionalInfoSize = sizeof(H264EncodeHard_AddInfo_CommandStatus_t);
        MmeCommand.CmdStatus.AdditionalInfo_p = &H264EncodeContext.CommandStatus;
        MMECommandPreparedCount++;
        MmeError = MME_SendCommand(H264EncodeContext.TransformHandleEncoder, &MmeCommand);

        if (MmeError != MME_SUCCESS)
        {
            SE_ERROR("MME_SET_GLOBAL_TRANSFORM_PARAMS error: %d\n", MmeError);
            MMECommandAbortedCount++;
            return CoderError;
        }

        if (OS_SemaphoreWaitTimeout(&H264EncodeContext.CompletionSemaphore, MME_COMMAND_TIMEOUT_MS) == OS_TIMED_OUT)
        {
            SE_ERROR("MME_SET_GLOBAL_TRANSFORM_PARAMS callback is not responding after %d ms\n", MME_COMMAND_TIMEOUT_MS);
            MMECommandAbortedCount++;
            return CoderError;
        }
        H264EncodeContext.GlobalParamsNeedsUpdate = false;
    }

    return Status;
}

//{{{  InitializeMMETransformParameters
//{{{  doxynote
/// \brief                      initialize MME transform (dynamic) parameters
/// \return                     CoderStatus_t
CoderStatus_t Coder_Video_Mme_H264_c::InitializeMMETransformParameters()
{
    SE_DEBUG(group_encoder_video_coder, "\n");
    //Fill 'H264EncodeHard_TransformParam_t' structure of 'H264EncoderContext_t' with default parameters
    //Those paraemeters would be then updated on a frame basis taking into account input buffer metadata + internal processing performed in ComputeMMETransformParameters()
    //Start stream encode with an 'I' frame
    H264EncodeContext.TransformParams.pictureCodingType          = HVA_ENCODE_I_FRAME;
    //Start stream encode with an 'IDR' frame
    H264EncodeContext.TransformParams.idrFlag                    = true;
    H264EncodeContext.TransformParams.frameNum                   = 0;
    H264EncodeContext.TransformParams.firstPictureInSequence     = true;
    //specifies that all luma and chroma block edges of the slice are filtered
    H264EncodeContext.TransformParams.disableDeblockingFilterIdc = HVA_ENCODE_DEBLOCKING_ENABLE;
    //Deblocking filter parameters not used
    H264EncodeContext.TransformParams.sliceAlphaC0OffsetDiv2     = 0;
    H264EncodeContext.TransformParams.sliceBetaOffsetDiv2        = 0;
    //Buffer adress to be updated with incoming input buffer
    H264EncodeContext.TransformParams.addrSourceBuffer           = 0;
    //This address should be 8 bytes aligned and is the aligned address from which HVA will write (offset will compensate alignment offset)
    H264EncodeContext.TransformParams.addrOutputBitstreamStart   = 0;
    //This address is the buffer end address
    H264EncodeContext.TransformParams.addrOutputBitstreamEnd     = 0;
    //offset in bits between aligned bitstream start address and first bit to be written by HVA . Range value is [0..63]
    H264EncodeContext.TransformParams.bitstreamOffset            = 0;
    H264EncodeContext.TransformParams.seiRecoveryPtPresentFlag   = false;
    H264EncodeContext.TransformParams.seiRecoveryFrameCnt        = 0;
    H264EncodeContext.TransformParams.seiBrokenLinkFlag          = false;
    H264EncodeContext.TransformParams.seiUsrDataT35PresentFlag   = false;
    H264EncodeContext.TransformParams.seiT35CountryCode          = 0;
    H264EncodeContext.TransformParams.seiAddrT35PayloadByte      = 0;
    H264EncodeContext.TransformParams.seiT35PayloadSize          = 0;

    return CoderNoError;
}
//}}}

//{{{  UpdateContextFromInputMetadata
//{{{  doxynote
/// \brief                             update encode context depending of latest input metadata
/// \param PreProcMetaDataDescriptor   involved input metadata to be analyzed versus existing context
/// \param InputBufferSize             involved input buffer size
/// \return                            CoderStatus_t
CoderStatus_t Coder_Video_Mme_H264_c::UpdateContextFromInputMetadata(__stm_se_frame_metadata_t *PreProcMetaDataDescriptor, uint32_t InputBufferSize)
{
    CoderStatus_t Status = CoderNoError;
    uint32_t FramerateNum;
    uint32_t FramerateDen;
    stm_se_uncompressed_frame_metadata_t *InMeta;
    __stm_se_encode_coordinator_metadata_t *CoMeta;
    uint32_t AspectRatio;
    uint32_t InputAspectRatio;
    uint32_t i;

    if (PreProcMetaDataDescriptor == NULL)
    {
        return CoderError;
    }

    InMeta = &PreProcMetaDataDescriptor->uncompressed_frame_metadata;
    CoMeta = &PreProcMetaDataDescriptor->encode_coordinator_metadata;

    // Check media
    Status = CheckMedia(InMeta->media);
    if (Status != CoderNoError)
    {
        SE_ERROR("Bad media metadata, Status = %u\n", Status);
        return Status;
    }

    // Check for scan type
    Status = CheckScanType(InMeta->video.video_parameters.scan_type);
    if (Status != CoderNoError)
    {
        SE_ERROR("Bad scan type metadata, Status = %u\n", Status);
        return Status;
    }

    // Check that 3d format is not used
    Status = CheckFormat3dType(InMeta->video.video_parameters.format_3d);
    if (Status != CoderNoError)
    {
        SE_ERROR("Bad 3d format metadata, Status = %u\n", Status);
        return Status;
    }

    // Check picture type
    // This field is not used by encoder
    // We still check that the value matches a picture type enum
    Status = CheckPictureType(InMeta->video.picture_type);
    if (Status != CoderNoError)
    {
        SE_ERROR("Bad picture type metadata, Status = %u\n", Status);
        return Status;
    }

    // Check framerate
    Status = CheckFrameRate(InMeta->video.frame_rate);
    if (Status != CoderNoError)
    {
        SE_ERROR("Bad framerate metadata, Status = %u\n", Status);
        return Status;
    }

    // Add warning trace if integer framerate is filled as it is not used
    if (InMeta->video.video_parameters.frame_rate != 0)
    {
        SE_WARNING_ONCE("Integer framerate %u not taken into account\n", InMeta->video.video_parameters.frame_rate);
    }

    //Compare existing encode context with provided input metadata
    //check incoming framerate
    FramerateNum = InMeta->video.frame_rate.framerate_num;
    FramerateDen = InMeta->video.frame_rate.framerate_den;
    RestrictFramerateTo16Bits(&FramerateNum, &FramerateDen);

    if (!AreFramerateMatching(FramerateNum, FramerateDen,
                              H264EncodeContext.GlobalParams.framerateNum, H264EncodeContext.GlobalParams.framerateDen))
    {
        //update MME sequence parameter
        H264EncodeContext.GlobalParams.framerateNum = (uint16_t)FramerateNum;
        H264EncodeContext.GlobalParams.framerateDen = (uint16_t)FramerateDen;
        H264EncodeContext.GlobalParamsNeedsUpdate = true;
        H264EncodeContext.NewGopRequested         = true;
        //FIXME: new GOP : impact on output metadata!!!
    }

    // Add warning trace if window of interest is filled and is not the same as frame resolution
    if ((InMeta->video.window_of_interest.x || InMeta->video.window_of_interest.y ||
         InMeta->video.window_of_interest.width || InMeta->video.window_of_interest.height) &&
        !((InMeta->video.window_of_interest.x == 0) && (InMeta->video.window_of_interest.y == 0) &&
          (InMeta->video.window_of_interest.width == InMeta->video.video_parameters.width) &&
          (InMeta->video.window_of_interest.height == InMeta->video.video_parameters.height)))
    {
        SE_WARNING_ONCE("Window of interest [%d %d]x[%d %d] not taken into account\n",
                        InMeta->video.window_of_interest.x,
                        InMeta->video.window_of_interest.x + InMeta->video.window_of_interest.width,
                        InMeta->video.window_of_interest.y,
                        InMeta->video.window_of_interest.y + InMeta->video.window_of_interest.height);
    }

    //check incoming resolution
    if ((InMeta->video.video_parameters.width != H264EncodeContext.GlobalParams.frameWidth) ||
        (InMeta->video.video_parameters.height != H264EncodeContext.GlobalParams.frameHeight))
    {
        // Check resolution limit
        if ((InMeta->video.video_parameters.width * InMeta->video.video_parameters.height < CODER_WIDTH_MIN * CODER_HEIGHT_MIN) ||
            (InMeta->video.video_parameters.width * InMeta->video.video_parameters.height > CODER_WIDTH_MAX * CODER_HEIGHT_MAX))
        {
            SE_ERROR("Metadata height %d is not in allowed range\n", InMeta->video.video_parameters.height);
            return CoderError;
        }

        //update MME sequence parameter
        H264EncodeContext.GlobalParams.frameWidth = InMeta->video.video_parameters.width;
        H264EncodeContext.GlobalParams.frameHeight = InMeta->video.video_parameters.height;
        //frame resolution is a criteria for Maximum Coded frame size definition => to be reassessed
        UpdateMaxBitSizePerAU();
        H264EncodeContext.GlobalParamsNeedsUpdate  = true;
        H264EncodeContext.NewGopRequested          = true;
        //FIXME: new GOP : impact on output metadata!!!
    }

    //check incoming color format
    if (!AreColorFormatMatching(InMeta->video.surface_format, H264EncodeContext.GlobalParams.samplingMode))
    {
        // Check surface format and update MME sequence parameter
        Status = CheckAndUpdateSurfaceFormat(InMeta->video.surface_format,
                                             InMeta->video.video_parameters.width,
                                             InMeta->video.video_parameters.height,
                                             InputBufferSize);
        if (Status != CoderNoError)
        {
            SE_ERROR("Bad surface format metadata, Status = %u\n", Status);
            return Status;
        }

        H264EncodeContext.GlobalParamsNeedsUpdate = true;
        H264EncodeContext.NewGopRequested         = true;
        //FIXME: new GOP : impact on output metadata!!!
    }

    //check incoming color space
    if (!AreColorSpaceMatching(InMeta->video.video_parameters.colorspace, H264EncodeContext.GlobalParams.vuiColorStd))
    {
        // Check for colorspace and update MME sequence parameter
        Status = CheckAndUpdateColorspace(InMeta->video.video_parameters.colorspace);
        if (Status != CoderNoError)
        {
            SE_ERROR("Bad colorspace metadata, Status = %u\n", Status);
            return Status;
        }

        H264EncodeContext.GlobalParamsNeedsUpdate = true;
        H264EncodeContext.NewGopRequested         = true;
        //FIXME: new GOP : impact on output metadata!!!
    }

    // Check for new gop request
    // New gop for new gop request and time discontinuity
    if ((InMeta->discontinuity & STM_SE_DISCONTINUITY_CLOSED_GOP_REQUEST) ||
        (InMeta->discontinuity & STM_SE_DISCONTINUITY_DISCONTINUOUS))
    {
        H264EncodeContext.GlobalParamsNeedsUpdate = true;
        H264EncodeContext.NewGopRequested         = true;
    }

    // Check for display aspect ratio
    Status = CheckDisplayAspectRatio(InMeta->video.video_parameters.aspect_ratio);
    if (Status != CoderNoError)
    {
        SE_ERROR("Bad aspect ratio metadata, Status = %u\n", Status);
        return Status;
    }

    // Check incoming pixel aspect ratio
    if (!AreAspectRatioMatching(InMeta->video.video_parameters.pixel_aspect_ratio_numerator,
                                InMeta->video.video_parameters.pixel_aspect_ratio_denominator,
                                H264EncodeContext.GlobalParams.vuiSarWidth,
                                H264EncodeContext.GlobalParams.vuiSarHeight))
    {
        //update MME sequence parameter
        H264EncodeContext.GlobalParams.vuiAspectRatioIdc = HVA_ENCODE_VUI_ASPECT_RATIO_EXTENDED_SAR;

        for (i = 0; i < H264_ASPECT_RATIO_ENTRIES; i++)
        {
            AspectRatio      = H264AspectRatioList[i].AspectRatioNum * InMeta->video.video_parameters.pixel_aspect_ratio_denominator;
            InputAspectRatio = H264AspectRatioList[i].AspectRatioDen * InMeta->video.video_parameters.pixel_aspect_ratio_numerator;

            if (AspectRatio == InputAspectRatio)
            {
                H264EncodeContext.GlobalParams.vuiAspectRatioIdc = H264AspectRatioList[i].VUIAspectRatio;
                H264EncodeContext.GlobalParams.vuiSarWidth       = H264AspectRatioList[i].AspectRatioNum;
                H264EncodeContext.GlobalParams.vuiSarHeight      = H264AspectRatioList[i].AspectRatioDen;
                break;
            }
        }

        if (H264EncodeContext.GlobalParams.vuiAspectRatioIdc == HVA_ENCODE_VUI_ASPECT_RATIO_EXTENDED_SAR)
        {
            H264EncodeContext.GlobalParams.vuiSarWidth    = InMeta->video.video_parameters.pixel_aspect_ratio_numerator;
            H264EncodeContext.GlobalParams.vuiSarHeight   = InMeta->video.video_parameters.pixel_aspect_ratio_denominator;
        }

        H264EncodeContext.GlobalParamsNeedsUpdate = true;
        H264EncodeContext.NewGopRequested         = true;
        //FIXME: new GOP : impact on output metadata!!!
    }

    // Check pitch
    Status = CheckPitch(InMeta->video.pitch,
                        InMeta->video.video_parameters.width,
                        InMeta->video.surface_format);
    if (Status != CoderNoError)
    {
        SE_ERROR("Bad pitch metadata, Status = %u\n", Status);
        return Status;
    }

    // Check vertical alignment
    Status = CheckVerticalAlignment(InMeta->video.vertical_alignment,
                                    InMeta->video.video_parameters.height,
                                    InMeta->video.pitch,
                                    InputBufferSize);
    if (Status != CoderNoError)
    {
        SE_ERROR("Bad vertical alignment metadata, Status = %u\n", Status);
        return Status;
    }

    //FIXME: 'Time discontinuity' to be added as input metadata
    //Then, involve gop to be closed: encode impact + output metadata !!!!
    //FIXME: also manage case of frame skipped: Time discontinuity to be propagated to next frame!
    //update context with output metadata, copying time information from input metadata
    H264EncodeContext.OutputMetadata.system_time            = InMeta->system_time;
    H264EncodeContext.OutputMetadata.native_time_format     = InMeta->native_time_format;
    H264EncodeContext.OutputMetadata.native_time            = InMeta->native_time;
    H264EncodeContext.OutputMetadata.encoded_time           = CoMeta->encoded_time;
    H264EncodeContext.OutputMetadata.encoded_time_format    = CoMeta->encoded_time_format;
    H264EncodeContext.OutputMetadata.discontinuity          = InMeta->discontinuity;
    //update context with VPP applied controls
    H264EncodeContext.VppDeinterlacingOn    = PreProcMetaDataDescriptor->encode_process_metadata.video.deinterlacing_on;
    H264EncodeContext.VppNoiseFilteringOn   = PreProcMetaDataDescriptor->encode_process_metadata.video.noise_filtering_on;
    return CoderNoError;
}
//}}}

//{{{  UpdateContextFromControls
//{{{  doxynote
/// \brief                             update encode context depending of received controls
/// \return                            CoderStatus_t
CoderStatus_t Coder_Video_Mme_H264_c::UpdateContextFromControls()
{
    CoderStatus_t Status = CoderNoError;

    //Track if received controls implies H264EncodeContext/GlobalParams updates

    //check requested BitRateControlMode
    if (H264EncodeContext.ControlsValues.BitRateControlMode != H264EncodeContext.GlobalParams.brcType)
    {
        SE_DEBUG(group_encoder_video_coder, "Brc type control update detected: switch to %d (%s)\n", H264EncodeContext.ControlsValues.BitRateControlMode,
                 StringifyBrcType(H264EncodeContext.ControlsValues.BitRateControlMode));
        H264EncodeContext.GlobalParams.brcType    = H264EncodeContext.ControlsValues.BitRateControlMode;
        //a MME global command to be issued
        H264EncodeContext.GlobalParamsNeedsUpdate = true;
        //match 'updated_metadata' field of stm_se_encode_process_metadata_t
        H264EncodeContext.ControlStatusUpdated    = true;
        //SPS/PPS to be generated, hence new gop
        H264EncodeContext.NewGopRequested         = true;
        bool isBitRateControlModeEnabled = (H264EncodeContext.ControlsValues.BitRateControlMode != HVA_ENCODE_NO_BRC);
        H264EncodeContext.GlobalParams.seiPicTimingPresentFlag  = isBitRateControlModeEnabled; // relates with SEI picture timng
        H264EncodeContext.GlobalParams.seiBufPeriodPresentFlag  = isBitRateControlModeEnabled; // relates with SEI buffering period
        H264EncodeContext.GlobalParams.vuiParametersPresentFlag = isBitRateControlModeEnabled; // relates with SPS VUI parameter presence
    }

    //check requested BitRate and Coded Picture Buffer Size
    if ((H264EncodeContext.ControlsValues.BitRate != H264EncodeContext.GlobalParams.bitRate) ||
        (H264EncodeContext.ControlsValues.CodedPictureBufferSize != H264EncodeContext.GlobalParams.cpbBufferSize))
    {
        if ((H264EncodeContext.ControlsValues.BitRate != H264EncodeContext.GlobalParams.bitRate) &&
            (H264EncodeContext.ControlsValues.CodedPictureBufferSize == H264EncodeContext.GlobalParams.cpbBufferSize))
        {
            SE_DEBUG(group_encoder_video_coder, "Bitrate control update detected: switch to %d bits/s\n", H264EncodeContext.ControlsValues.BitRate);
            H264EncodeContext.GlobalParams.bitRate                  = H264EncodeContext.ControlsValues.BitRate;
            if (!H264EncodeContext.ControlsValues.CpbSizeSetByClient)
            {
                H264EncodeContext.GlobalParams.cpbBufferSize            = H264EncodeContext.ControlsValues.BitRate * H264_ENC_DEFAULT_DELAY; // default value when it is not set by client
                H264EncodeContext.ControlsValues.CodedPictureBufferSize = H264EncodeContext.GlobalParams.cpbBufferSize;
            }
        }
        else if ((H264EncodeContext.ControlsValues.BitRate == H264EncodeContext.GlobalParams.bitRate) &&
                 (H264EncodeContext.ControlsValues.CodedPictureBufferSize != H264EncodeContext.GlobalParams.cpbBufferSize))
        {
            SE_DEBUG(group_encoder_video_coder, "Cpb size control update detected: switch to %d bits\n", H264EncodeContext.ControlsValues.CodedPictureBufferSize);
            H264EncodeContext.GlobalParams.cpbBufferSize = H264EncodeContext.ControlsValues.CodedPictureBufferSize;
            if (!H264EncodeContext.ControlsValues.BitRateSetByClient)
            {
                H264EncodeContext.GlobalParams.bitRate       = H264EncodeContext.ControlsValues.CodedPictureBufferSize / H264_ENC_DEFAULT_DELAY; // default value when it is not set by client
                H264EncodeContext.ControlsValues.BitRate     = H264EncodeContext.GlobalParams.bitRate;
            }
        }
        else
        {
            SE_DEBUG(group_encoder_video_coder, "Bitrate control update detected: switch to %d bits/s\n", H264EncodeContext.ControlsValues.BitRate);
            H264EncodeContext.GlobalParams.bitRate    = H264EncodeContext.ControlsValues.BitRate;
            SE_DEBUG(group_encoder_video_coder, "Cpb size control update detected: switch to %d bits\n", H264EncodeContext.ControlsValues.CodedPictureBufferSize);
            H264EncodeContext.GlobalParams.cpbBufferSize = H264EncodeContext.ControlsValues.CodedPictureBufferSize;
        }
        H264EncodeContext.GlobalParamsNeedsUpdate = true;
        H264EncodeContext.ControlStatusUpdated    = true;
        //SPS/PPS to be generated, hence new gop
        H264EncodeContext.NewGopRequested         = true;
    }

    //check requested GopSize
    if (H264EncodeContext.ControlsValues.GopSize != H264EncodeContext.GopSize)
    {
        SE_DEBUG(group_encoder_video_coder, "Gop size control update detected: switch to %d\n", H264EncodeContext.ControlsValues.GopSize);
        H264EncodeContext.GopSize                 = H264EncodeContext.ControlsValues.GopSize;
        H264EncodeContext.GlobalParamsNeedsUpdate = true;
        H264EncodeContext.ControlStatusUpdated    = true;
        //SPS/PPS to be generated, hence new gop
        H264EncodeContext.NewGopRequested         = true;
    }

    //check requested H264 level
    if (H264EncodeContext.ControlsValues.H264Level != H264EncodeContext.GlobalParams.levelIdc)
    {
        SE_DEBUG(group_encoder_video_coder, "H264 level update detected: switch to %d\n", H264EncodeContext.ControlsValues.H264Level);
        H264EncodeContext.GlobalParams.levelIdc   = H264EncodeContext.ControlsValues.H264Level;
        //Level is a criteria for Maximum Coded frame size => to be reassessed
        UpdateMaxBitSizePerAU();
        H264EncodeContext.GlobalParamsNeedsUpdate = true;
        H264EncodeContext.ControlStatusUpdated    = true;
        //SPS/PPS to be generated, hence new gop
        H264EncodeContext.NewGopRequested         = true;
    }

    //check requested H264 profile
    if (H264EncodeContext.ControlsValues.H264Profile != H264EncodeContext.GlobalParams.profileIdc)
    {
        SE_DEBUG(group_encoder_video_coder, "H264 profile update detected: switch to %d\n", H264EncodeContext.ControlsValues.H264Profile);
        H264EncodeContext.GlobalParams.profileIdc = H264EncodeContext.ControlsValues.H264Profile;
        H264EncodeContext.GlobalParamsNeedsUpdate = true;
        H264EncodeContext.ControlStatusUpdated    = true;
        //SPS/PPS to be generated, hence new gop
        H264EncodeContext.NewGopRequested         = true;
    }

    // Check level limits
    Status = CheckLevelLimits(&H264EncodeContext.GlobalParams);
    if (Status != CoderNoError)
    {
        SE_ERROR("Level limits overflow, Status = %u\n", Status);
        return Status;
    }

    return CoderNoError;
}
//}}}


//{{{  UpdateContextFromTransformCapability
//{{{  doxynote
/// \brief                       Update context from MME transform capability
/// \return                     CoderStatus_t
CoderStatus_t Coder_Video_Mme_H264_c::UpdateContextFromTransformCapability(void)
{
    if (H264EncodeContext.TransformCapability.isIntraRefreshSupported)
    {
        H264EncodeContext.GlobalParams.intraRefreshType    = HVA_ENCODE_ADAPTIVE_INTRA_REFRESH;
    }
    else
    {
        H264EncodeContext.GlobalParams.intraRefreshType    = HVA_ENCODE_DISABLE_INTRA_REFRESH;
    }

    H264EncodeContext.GlobalParams.irParamOption           = 0;

    if (H264EncodeContext.TransformCapability.isT8x8Supported)
    {
        H264EncodeContext.GlobalParams.transformMode       = HVA_ENCODE_T8x8_ALLOWED;
        H264EncodeContext.ControlsValues.H264Profile       = (H264EncodeSPSProfileIDC_t)(HVA_ENCODE_SPS_PROFILE_IDC_HIGH);
    }
    else
    {
        H264EncodeContext.GlobalParams.transformMode       = HVA_ENCODE_NO_T8x8_ALLOWED;
        H264EncodeContext.ControlsValues.H264Profile       = (H264EncodeSPSProfileIDC_t)(HVA_ENCODE_SPS_PROFILE_IDC_BASELINE);
    }

    if (H264EncodeContext.TransformCapability.isT8x8Supported)
    {
        H264EncodeContext.GlobalParams.profileIdc          = HVA_ENCODE_SPS_PROFILE_IDC_HIGH;
    }
    else
    {
        H264EncodeContext.GlobalParams.profileIdc          = HVA_ENCODE_SPS_PROFILE_IDC_BASELINE;
    }

    return CoderNoError;
}

//{{{  FillOutputMetadata
//{{{  doxynote
/// \brief                      Fill Coder Output Metadata using context information
/// \return                     CoderStatus_t
CoderStatus_t Coder_Video_Mme_H264_c::FillOutputMetadata(__stm_se_frame_metadata_t *CoderMetaDataDescriptor)
{
    stm_se_compressed_frame_metadata_t *OutputMetaDataDescriptor;
    stm_se_encode_process_metadata_t   *ProcessMetadataDescriptor;

    if (CoderMetaDataDescriptor == NULL)
    {
        return CoderError;
    }

    OutputMetaDataDescriptor  = &CoderMetaDataDescriptor->compressed_frame_metadata;
    ProcessMetadataDescriptor = &CoderMetaDataDescriptor->encode_process_metadata;
    //Fills output metadata from H264 context
    OutputMetaDataDescriptor->system_time                              = H264EncodeContext.OutputMetadata.system_time;
    OutputMetaDataDescriptor->native_time_format                       = H264EncodeContext.OutputMetadata.native_time_format;
    OutputMetaDataDescriptor->native_time                              = H264EncodeContext.OutputMetadata.native_time;
    OutputMetaDataDescriptor->encoded_time_format                      = H264EncodeContext.OutputMetadata.encoded_time_format;
    OutputMetaDataDescriptor->encoded_time                             = H264EncodeContext.OutputMetadata.encoded_time;
    OutputMetaDataDescriptor->discontinuity                            = H264EncodeContext.OutputMetadata.discontinuity;
    OutputMetaDataDescriptor->encoding                                 = STM_SE_ENCODE_STREAM_ENCODING_VIDEO_H264;
    OutputMetaDataDescriptor->video.new_gop                            = H264EncodeContext.OutputMetadata.video.new_gop;
    OutputMetaDataDescriptor->video.closed_gop                         = H264EncodeContext.OutputMetadata.video.closed_gop;
    OutputMetaDataDescriptor->video.picture_type                       = H264EncodeContext.OutputMetadata.video.picture_type;
    //Fills out encode control status from latched control values
    ProcessMetadataDescriptor->video.bitrate_control_mode              = TRANSLATE_BRC_MODE_MME_TO_STKPI(H264EncodeContext.GlobalParams.brcType);
    ProcessMetadataDescriptor->video.bitrate_control_value             = H264EncodeContext.GlobalParams.bitRate;
    ProcessMetadataDescriptor->video.encoded_picture_resolution.width  = H264EncodeContext.GlobalParams.frameWidth;
    ProcessMetadataDescriptor->video.encoded_picture_resolution.height = H264EncodeContext.GlobalParams.frameHeight;
    //FIXME: later add
    //ProcessMetadataDescriptor->video.encoded_framerate.framerate_num   = H264EncodeContext.GlobalParams.framerateNum;
    //ProcessMetadataDescriptor->video.encoded_framerate.framerate_den   = H264EncodeContext.GlobalParams.framerateDen;
    ProcessMetadataDescriptor->video.deinterlacing_on                  = H264EncodeContext.VppDeinterlacingOn;
    ProcessMetadataDescriptor->video.noise_filtering_on                = H264EncodeContext.VppNoiseFilteringOn;
    //FIXME: today, only mono slice supported
    ProcessMetadataDescriptor->video.multi_slice_mode.control          = STM_SE_VIDEO_MULTI_SLICE_MODE_SINGLE;
    ProcessMetadataDescriptor->video.gop_size                          = H264EncodeContext.GopSize;
    ProcessMetadataDescriptor->video.cpb_size                          = H264EncodeContext.GlobalParams.cpbBufferSize;
    ProcessMetadataDescriptor->video.h264_level                        = H264EncodeContext.GlobalParams.levelIdc;
    ProcessMetadataDescriptor->video.h264_profile                      = H264EncodeContext.GlobalParams.profileIdc;
    ProcessMetadataDescriptor->updated_metadata                       |= H264EncodeContext.ControlStatusUpdated;
    H264EncodeContext.ControlStatusUpdated = false;
    MonitorOutputMetadata(CoderMetaDataDescriptor);
    return CoderNoError;
}

//}}}

//{{{  ComputeMMETransformParameters
//{{{  doxynote
/// \brief                      compute MME transform (dynamic) parameters on frame basis
/// \return                     CoderStatus_t
CoderStatus_t Coder_Video_Mme_H264_c::ComputeMMETransformParameters(Buffer_t FrameBuffer)
{
    uint32_t FrameBufferPhysStartAddr = 0;
    uint32_t CodedBufferPhysStartAddr = 0;
    SE_DEBUG(group_encoder_video_coder, "\n");

    //Retrieve input frame buffer physical address & store in context
    FrameBuffer->ObtainDataReference(NULL, NULL, (void **)(&FrameBufferPhysStartAddr), PhysicalAddress);
    if (FrameBufferPhysStartAddr == NULL)
    {
        SE_ERROR("Unable to get frame buffer physical address\n");
        return CoderError;
    }

    //Retrieve output coded buffer physical address & store in context
    CodedFrameBuffer->ObtainDataReference(NULL, NULL, (void **)(&CodedBufferPhysStartAddr), PhysicalAddress);
    if (CodedBufferPhysStartAddr == NULL)
    {
        SE_ERROR("Unable to get coded buffer physical address\n");
        return CoderError;
    }

    //Handle picture coding type + related variables
    if ((H264EncodeContext.NewGopRequested) ||
        (H264EncodeContext.GopId >= H264EncodeContext.GopSize))
    {
        H264EncodeContext.NewGopRequested = true;
        H264EncodeContext.TransformParams.pictureCodingType = HVA_ENCODE_I_FRAME;
        H264EncodeContext.GopId = 0;
        H264EncodeContext.TransformParams.frameNum = 0;
    }
    else
    {
        H264EncodeContext.TransformParams.pictureCodingType = HVA_ENCODE_P_FRAME;
    }

    //All I frame are IDR
    H264EncodeContext.TransformParams.idrFlag = (H264EncodeContext.TransformParams.pictureCodingType == HVA_ENCODE_I_FRAME) ? true : false;

    //Handle FrameNum
    if (H264EncodeContext.TransformParams.frameNum > H264_ENC_MAX_FRAME_NUM)
    {
        H264EncodeContext.TransformParams.frameNum = 0;
    }

    H264EncodeContext.TransformParams.firstPictureInSequence = H264EncodeContext.FirstFrameInSequence;
    //Other TransformParams are static Params handled in InitializeMMETransformParameters()
    //Buffer address to be updated with incoming input buffer
    H264EncodeContext.TransformParams.addrSourceBuffer         = FrameBufferPhysStartAddr;
    //This address should be 16 bytes aligned and is the aligned address from which HVA will write (offset will compensate alignment offset)
    H264EncodeContext.TransformParams.addrOutputBitstreamStart = (CodedBufferPhysStartAddr & 0xfffffff0);
    //This address is the buffer end address
    H264EncodeContext.TransformParams.addrOutputBitstreamEnd   = ((CodedBufferPhysStartAddr + CodedFrameMaximumSize) & 0xfffffff0);
    //offset in bits between aligned bitstream start address and first bit to be written by HVA . Range value is [0..127]
    //FIXME: consider here that nonVCLNALU are managed by MME
    H264EncodeContext.TransformParams.bitstreamOffset          = ((CodedBufferPhysStartAddr & 0xF) << 3);

    return CoderNoError;
}
//}}}

//{{{  SendMMETransformCommand
//{{{  doxynote
/// \brief                      send MME_TRANSFORM MME command using information from H264EncoderContext_t
/// \return                     CoderStatus_t
CoderStatus_t Coder_Video_Mme_H264_c::SendMMETransformCommand()
{
    MME_Command_t MmeCommand;
    MME_ERROR MmeError;
    SE_DEBUG(group_encoder_video_coder, "\n");

    //
    // Check that we have not commenced shutdown.
    //

    if (TestComponentState(ComponentHalted))
    {
        return CoderNoError;
    }

    // clear command status
    memset((void *)&H264EncodeContext.CommandStatus, 0, sizeof(H264EncodeHard_AddInfo_CommandStatus_t));
    MmeCommand.StructSize                   = sizeof(MME_Command_t);
    MmeCommand.CmdEnd                       = MME_COMMAND_END_RETURN_NOTIFY;
    MmeCommand.DueTime                      = 0;
    MmeCommand.ParamSize                    = sizeof(H264EncodeHard_TransformParam_t);
    MmeCommand.Param_p                      = &H264EncodeContext.TransformParams;
    MmeCommand.CmdCode                      = MME_TRANSFORM;
    MmeCommand.NumberInputBuffers           = 0;
    MmeCommand.NumberOutputBuffers          = 0;
    MmeCommand.DataBuffers_p                = NULL;
    MmeCommand.CmdStatus.AdditionalInfoSize = sizeof(H264EncodeHard_AddInfo_CommandStatus_t);
    MmeCommand.CmdStatus.AdditionalInfo_p   = &H264EncodeContext.CommandStatus;
    MMECommandPreparedCount++;
    MmeError = MME_SendCommand(H264EncodeContext.TransformHandleEncoder, &MmeCommand);

    if (MmeError != MME_SUCCESS)
    {
        MMECommandAbortedCount++;
        SE_ERROR("MME_TRANSFORM error: %d\n", MmeError);
        return CoderError;
    }

    if (OS_SemaphoreWaitTimeout(&H264EncodeContext.CompletionSemaphore, MME_COMMAND_TIMEOUT_MS) == OS_TIMED_OUT)
    {
        MMECommandAbortedCount++;
        SE_ERROR("MME_TRANSFORM callback is not responding after %d ms\n", MME_COMMAND_TIMEOUT_MS);
        return CoderError;
    }

    //check encode status
    if (H264EncodeContext.CommandStatus.transformStatus == HVA_ENCODE_OK)
    {
        //Fill output metadata suitable information
        H264EncodeContext.OutputMetadata.video.new_gop              = H264EncodeContext.NewGopRequested;
        //We only manage closed gop at that time (No 'B' frame handled)
        H264EncodeContext.OutputMetadata.video.closed_gop           = H264EncodeContext.ClosedGopType;

        switch (H264EncodeContext.TransformParams.pictureCodingType)
        {
        case HVA_ENCODE_I_FRAME:
            H264EncodeContext.OutputMetadata.video.picture_type = STM_SE_PICTURE_TYPE_I;
            break;

        case HVA_ENCODE_P_FRAME:
            H264EncodeContext.OutputMetadata.video.picture_type = STM_SE_PICTURE_TYPE_P;
            break;

        default:
            SE_ERROR("H264 picture coding type unknown");
        }

        //'firstPictureInSequence' will be updated with 'H264EncodeContext.FirstFrameInSequence' at next transform
        H264EncodeContext.TransformParams.frameNum ++;
        H264EncodeContext.FrameCount ++;
        H264EncodeContext.GopId ++;
        //reset 'request' variables as request now handled
        //FIXME: synchro with stkpi controls to be analyzed
        H264EncodeContext.NewGopRequested         = false;
        H264EncodeContext.FirstFrameInSequence    = false;
        //Statistics handling
        H264EncodeContext.Statistics.MaxEncodeTime = max(H264EncodeContext.Statistics.MaxEncodeTime, H264EncodeContext.CommandStatus.frameEncodeDuration);
        H264EncodeContext.Statistics.MinEncodeTime = min(H264EncodeContext.Statistics.MinEncodeTime, H264EncodeContext.CommandStatus.frameEncodeDuration);
        H264EncodeContext.Statistics.MaxEncodeSize = max(H264EncodeContext.Statistics.MaxEncodeSize, H264EncodeContext.CommandStatus.bitstreamSize);
        H264EncodeContext.Statistics.MinEncodeSize = min(H264EncodeContext.Statistics.MinEncodeSize, H264EncodeContext.CommandStatus.bitstreamSize);
        H264EncodeContext.Statistics.EncodeNumber++;
        H264EncodeContext.Statistics.EncodeSumTime += H264EncodeContext.CommandStatus.frameEncodeDuration;

        if (H264EncodeContext.TransformParams.pictureCodingType == HVA_ENCODE_I_FRAME)
        {
            H264EncodeContext.Statistics.EncodeNumberIPic++;
            H264EncodeContext.Statistics.EncodeSumTimeIPic += H264EncodeContext.CommandStatus.frameEncodeDuration;
        }
        else
        {
            H264EncodeContext.Statistics.EncodeNumberPPic++;
            H264EncodeContext.Statistics.EncodeSumTimePPic += H264EncodeContext.CommandStatus.frameEncodeDuration;
        }
    }
    else
    {
        if (H264EncodeContext.CommandStatus.transformStatus == HVA_ENCODE_FRAME_SKIPPED)
        {
            //In case of HVA encode frame skip, provide an encoded buffer of '0' size (See Bug25698)
            SE_VERBOSE(group_encoder_video_coder, "Got an encode frame skip\n");
            H264EncodeContext.CommandStatus.nonVCLNALUSize = 0;
            H264EncodeContext.CommandStatus.bitstreamSize  = 0;
            H264EncodeContext.FrameCount ++;
            // Introduce frame skipped discontinuity
            H264EncodeContext.OutputMetadata.discontinuity = (stm_se_discontinuity_t)(H264EncodeContext.OutputMetadata.discontinuity | STM_SE_DISCONTINUITY_FRAME_SKIPPED);
            Encoder.EncodeStream->EncodeStreamStatistics().VideoEncodeFrameSkippedCountFromCoder++;
            SignalEvent(STM_SE_ENCODE_STREAM_EVENT_VIDEO_FRAME_SKIPPED);
            if (H264EncodeContext.TransformParams.pictureCodingType == HVA_ENCODE_I_FRAME)
            {
                H264EncodeContext.NewGopRequested = true;
            }
            else
            {
                //Fill output metadata suitable information
                H264EncodeContext.OutputMetadata.video.new_gop = H264EncodeContext.NewGopRequested;
                H264EncodeContext.OutputMetadata.video.closed_gop = H264EncodeContext.ClosedGopType;
                H264EncodeContext.GopId ++;
                H264EncodeContext.NewGopRequested         = false;
                H264EncodeContext.FirstFrameInSequence    = false;
            }
            SE_VERBOSE(group_encoder_video_coder, "transformStatus %u (%s), frameNum %u, FrameCount %u\n\n", H264EncodeContext.CommandStatus.transformStatus,
                       StringifyTransformStatus(H264EncodeContext.CommandStatus.transformStatus), H264EncodeContext.TransformParams.frameNum, H264EncodeContext.FrameCount);
            return CoderNoError;
        }
        else //HVA_ENCODE fail!
        {
            SE_ERROR("Hva command transformStatus %d (%s)\n", H264EncodeContext.CommandStatus.transformStatus, StringifyTransformStatus(H264EncodeContext.CommandStatus.transformStatus));
            return CoderError;
        }
    }

    SE_VERBOSE(group_encoder_video_coder, "transformStatus %u (%s), frameNum %u, FrameCount %u\n\n", H264EncodeContext.CommandStatus.transformStatus,
               StringifyTransformStatus(H264EncodeContext.CommandStatus.transformStatus), H264EncodeContext.TransformParams.frameNum,
               H264EncodeContext.FrameCount);
    return CoderNoError;
}

CoderStatus_t Coder_Video_Mme_H264_c::GetControl(stm_se_ctrl_t Control, void *Data)
{
    CoderStatus_t CoderStatus = CoderNoError;

    switch (Control)
    {
    case STM_SE_CTRL_ENCODE_STREAM_BITRATE_MODE:
        CoderStatus = GetBitrateMode((uint32_t *)Data);
        break;

    case STM_SE_CTRL_ENCODE_STREAM_BITRATE:
        *(uint32_t *)Data = H264EncodeContext.ControlsValues.BitRate;
        break;

    case STM_SE_CTRL_VIDEO_ENCODE_STREAM_GOP_SIZE:
        *(uint32_t *)Data = H264EncodeContext.ControlsValues.GopSize;
        break;

    case STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_LEVEL:
        CoderStatus = GetLevel((uint32_t *)Data);
        break;

    case STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_PROFILE:
        CoderStatus = GetProfile((uint32_t *)Data);
        break;

    case STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_CPB_SIZE:
        *(uint32_t *)Data = H264EncodeContext.ControlsValues.CodedPictureBufferSize;
        break;

    default:
        // Cannot trace this as error because it can be a valid control for another object
        SE_DEBUG(group_encoder_video_coder, "Not match coder video h264 control %u\n", Control);
        return CoderControlNotMatch;
    }

    return CoderStatus;
}

CoderStatus_t Coder_Video_Mme_H264_c::GetCompoundControl(stm_se_ctrl_t Control, void *Data)
{
    CoderStatus_t CoderStatus = CoderNoError;

    switch (Control)
    {
    case STM_SE_CTRL_VIDEO_ENCODE_STREAM_MULTI_SLICE:
        // We assume that the client point to the correct structure
        CoderStatus = GetSliceMode((stm_se_video_multi_slice_t *)Data);
        break;

    default:
        // Cannot trace this as error because it can be a valid control for another object
        SE_DEBUG(group_encoder_video_coder, "Not match coder video h264 control %u\n", Control);
        return CoderControlNotMatch;
    }

    return CoderStatus;
}

CoderStatus_t Coder_Video_Mme_H264_c::GetSliceMode(stm_se_video_multi_slice_t *SliceMode)
{
    switch (H264EncodeContext.ControlsValues.SliceMode)
    {
    case HVA_ENCODE_SLICE_MODE_SINGLE:
        SliceMode->control = STM_SE_VIDEO_MULTI_SLICE_MODE_SINGLE;
        SliceMode->slice_max_mb_size = 0;
        SliceMode->slice_max_byte_size = 0;
        break;

    default:
        SE_ERROR("Invalid internal slice mode %u\n", SliceMode->control);
        return EncoderUnsupportedControl;
    }

    return CoderNoError;
}

CoderStatus_t Coder_Video_Mme_H264_c::GetBitrateMode(uint32_t *BitrateMode)
{
    switch (H264EncodeContext.ControlsValues.BitRateControlMode)
    {
    case HVA_ENCODE_CBR:
        *BitrateMode = STM_SE_CTRL_VALUE_BITRATE_CONTROL_MODE_CBR;
        break;

    case HVA_ENCODE_VBR:
        *BitrateMode = STM_SE_CTRL_VALUE_BITRATE_CONTROL_MODE_VBR;
        break;

    case HVA_ENCODE_NO_BRC:
        *BitrateMode = STM_SE_CTRL_VALUE_BITRATE_CONTROL_MODE_OFF;
        break;

    default:
        SE_ERROR("Invalid internal bitrate control mode %u\n", H264EncodeContext.ControlsValues.BitRateControlMode);
        return EncoderUnsupportedControl;
    }

    return CoderNoError;
}

CoderStatus_t Coder_Video_Mme_H264_c::GetLevel(uint32_t *Level)
{
    switch (H264EncodeContext.ControlsValues.H264Level)
    {
    case HVA_ENCODE_SPS_LEVEL_IDC_1_0:
        *Level = STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_1_0;
        break;

    case HVA_ENCODE_SPS_LEVEL_IDC_1_B:
        *Level = STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_1_B;
        break;

    case HVA_ENCODE_SPS_LEVEL_IDC_1_1:
        *Level = STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_1_1;
        break;

    case HVA_ENCODE_SPS_LEVEL_IDC_1_2:
        *Level = STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_1_2;
        break;

    case HVA_ENCODE_SPS_LEVEL_IDC_1_3:
        *Level = STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_1_3;
        break;

    case HVA_ENCODE_SPS_LEVEL_IDC_2_0:
        *Level = STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_2_0;
        break;

    case HVA_ENCODE_SPS_LEVEL_IDC_2_1:
        *Level = STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_2_1;
        break;

    case HVA_ENCODE_SPS_LEVEL_IDC_2_2:
        *Level = STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_2_2;
        break;

    case HVA_ENCODE_SPS_LEVEL_IDC_3_0:
        *Level = STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_3_0;
        break;

    case HVA_ENCODE_SPS_LEVEL_IDC_3_1:
        *Level = STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_3_1;
        break;

    case HVA_ENCODE_SPS_LEVEL_IDC_3_2:
        *Level = STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_3_2;
        break;

    case HVA_ENCODE_SPS_LEVEL_IDC_4_0:
        *Level = STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_4_0;
        break;

    case HVA_ENCODE_SPS_LEVEL_IDC_4_1:
        *Level = STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_4_1;
        break;

    case HVA_ENCODE_SPS_LEVEL_IDC_4_2:
        *Level = STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_4_2;
        break;

    case HVA_ENCODE_SPS_LEVEL_IDC_5_0:
        *Level = STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_5_0;
        break;

    case HVA_ENCODE_SPS_LEVEL_IDC_5_1:
        *Level = STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_5_1;
        break;

    case HVA_ENCODE_SPS_LEVEL_IDC_5_2:
        *Level = STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_5_2;
        break;

    default:
        SE_ERROR("Invalid internal level idc %u\n", H264EncodeContext.ControlsValues.H264Level);
        return EncoderUnsupportedControl;
    }

    return CoderNoError;
}

CoderStatus_t Coder_Video_Mme_H264_c::GetProfile(uint32_t *Profile)
{
    switch (H264EncodeContext.ControlsValues.H264Profile)
    {
    case HVA_ENCODE_SPS_PROFILE_IDC_BASELINE:
        *Profile = STM_SE_CTRL_VALUE_VIDEO_H264_PROFILE_IDC_BASELINE;
        break;

    case HVA_ENCODE_SPS_PROFILE_IDC_MAIN:
        *Profile = STM_SE_CTRL_VALUE_VIDEO_H264_PROFILE_IDC_MAIN;
        break;

    case HVA_ENCODE_SPS_PROFILE_IDC_HIGH:
        *Profile = STM_SE_CTRL_VALUE_VIDEO_H264_PROFILE_IDC_HIGH;
        break;

    default:
        SE_ERROR("Invalid internal profile idc %u\n", H264EncodeContext.ControlsValues.H264Profile);
        return EncoderUnsupportedControl;
    }

    return CoderNoError;
}

//{{{  SetControl
//{{{  doxynote
/// \brief                      Record incoming controls : will be applied at next Input() call
/// \return                     CoderStatus_t
CoderStatus_t Coder_Video_Mme_H264_c::SetControl(stm_se_ctrl_t Control, const void *Data)
{
    CoderStatus_t CoderStatus = CoderNoError;

    switch (Control)
    {
    case STM_SE_CTRL_ENCODE_STREAM_BITRATE_MODE:
        CoderStatus = SetBitrateMode(*(uint32_t *)Data);
        break;

    case STM_SE_CTRL_ENCODE_STREAM_BITRATE:
        if ((*(uint32_t *)Data >= H264_ENC_MIN_BITRATE) && (*(uint32_t *)Data <= H264_ENC_MAX_BITRATE))
        {
            SE_INFO(group_encoder_video_coder, "Bitrate set to %u bits/s\n", *(uint32_t *)Data);
            H264EncodeContext.ControlsValues.BitRate = *(uint32_t *)Data;
            H264EncodeContext.ControlsValues.BitRateSetByClient = true;
        }
        else
        {
            SE_ERROR("Invalid bitrate %u bits/s\n", *(uint32_t *)Data);
            CoderStatus = EncoderUnsupportedControl;
        }
        break;

    case STM_SE_CTRL_VIDEO_ENCODE_STREAM_GOP_SIZE:
        H264EncodeContext.ControlsValues.GopSize = *(uint32_t *)Data;
        break;

    case STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_LEVEL:
        CoderStatus = SetLevel(*(uint32_t *)Data);
        break;

    case STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_PROFILE:
        CoderStatus = SetProfile(*(uint32_t *)Data);
        break;

    case STM_SE_CTRL_VIDEO_ENCODE_STREAM_H264_CPB_SIZE:
        if ((*(uint32_t *)Data >= H264_ENC_MIN_CPB_SIZE) && (*(uint32_t *)Data <= H264_ENC_MAX_CPB_SIZE))
        {
            SE_INFO(group_encoder_video_coder, "Cpb size set to %u bits\n", *(uint32_t *)Data);
            H264EncodeContext.ControlsValues.CodedPictureBufferSize = *(uint32_t *)Data;
            H264EncodeContext.ControlsValues.CpbSizeSetByClient = true;
        }
        else
        {
            SE_ERROR("Invalid cpb size %u bits\n", *(uint32_t *)Data);
            CoderStatus = EncoderUnsupportedControl;
        }
        break;

    default:
        // Cannot trace this as error because it can be a valid control for another object
        SE_DEBUG(group_encoder_video_coder, "Not match coder video h264 control %u\n", Control);
        return CoderControlNotMatch;
    }

    return CoderStatus;
}

//{{{  SetCompoundControl
//{{{  doxynote
/// \brief                      Record incoming controls : will be applied at next Input() call
/// \return                     CoderStatus_t
CoderStatus_t Coder_Video_Mme_H264_c::SetCompoundControl(stm_se_ctrl_t Control, const void *Data)
{
    CoderStatus_t CoderStatus = CoderNoError;

    switch (Control)
    {
    case STM_SE_CTRL_VIDEO_ENCODE_STREAM_MULTI_SLICE:
        // We assume that the client point to the correct structure
        CoderStatus = SetSliceMode(*(stm_se_video_multi_slice_t *)Data);
        break;

    default:
        // Cannot trace this as error because it can be a valid control for another object
        SE_DEBUG(group_encoder_video_coder, "Not match coder video h264 control %u\n", Control);
        return CoderControlNotMatch;
    }

    return CoderStatus;
}

CoderStatus_t Coder_Video_Mme_H264_c::SetSliceMode(stm_se_video_multi_slice_t SliceMode)
{
    switch (SliceMode.control)
    {
    case STM_SE_VIDEO_MULTI_SLICE_MODE_SINGLE:
        SE_INFO(group_encoder_video_coder, "Slice mode set to %u (%s)\n", SliceMode.control, StringifySliceMode(SliceMode.control));
        H264EncodeContext.ControlsValues.SliceMode = HVA_ENCODE_SLICE_MODE_SINGLE;
        H264EncodeContext.ControlsValues.SliceMaxMbSize = 0;
        H264EncodeContext.ControlsValues.SliceMaxByteSize = 0;
        break;

    case STM_SE_VIDEO_MULTI_SLICE_MODE_MAX_MB:
    case STM_SE_VIDEO_MULTI_SLICE_MODE_MAX_BYTES:
        SE_ERROR("Unsupported slice mode %u (%s)\n", SliceMode.control, StringifySliceMode(SliceMode.control));
        return EncoderUnsupportedControl;

    default:
        SE_ERROR("Invalid slice mode %u\n", SliceMode.control);
        return EncoderUnsupportedControl;
    }

    return CoderNoError;
}

CoderStatus_t Coder_Video_Mme_H264_c::SetBitrateMode(uint32_t BitrateMode)
{
    switch (BitrateMode)
    {
    case STM_SE_CTRL_VALUE_BITRATE_CONTROL_MODE_CBR:
        SE_ERROR("Unsupported bitrate control mode %u (%s)\n", BitrateMode, StringifyBitrateMode(BitrateMode));
        return EncoderUnsupportedControl;

    case STM_SE_CTRL_VALUE_BITRATE_CONTROL_MODE_VBR:
        H264EncodeContext.ControlsValues.BitRateControlMode = HVA_ENCODE_VBR;
        break;

    case STM_SE_CTRL_VALUE_BITRATE_CONTROL_MODE_OFF:
        H264EncodeContext.ControlsValues.BitRateControlMode = HVA_ENCODE_NO_BRC;
        break;

    default:
        SE_ERROR("Invalid bitrate control mode %u\n", BitrateMode);
        return EncoderUnsupportedControl;
    }

    SE_INFO(group_encoder_video_coder, "Bitrate control mode set to %u (%s)\n", BitrateMode, StringifyBitrateMode(BitrateMode));
    return CoderNoError;
}

CoderStatus_t Coder_Video_Mme_H264_c::SetLevel(uint32_t Level)
{
    switch (Level)
    {
    case STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_0_9:
        SE_ERROR("Unsupported level idc %u (%s)\n", Level, StringifyLevel(Level));
        return EncoderUnsupportedControl;

    case STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_1_0:
        H264EncodeContext.ControlsValues.H264Level = HVA_ENCODE_SPS_LEVEL_IDC_1_0;
        break;

    case STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_1_B:
        H264EncodeContext.ControlsValues.H264Level = HVA_ENCODE_SPS_LEVEL_IDC_1_B;
        break;

    case STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_1_1:
        H264EncodeContext.ControlsValues.H264Level = HVA_ENCODE_SPS_LEVEL_IDC_1_1;
        break;

    case STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_1_2:
        H264EncodeContext.ControlsValues.H264Level = HVA_ENCODE_SPS_LEVEL_IDC_1_2;
        break;

    case STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_1_3:
        H264EncodeContext.ControlsValues.H264Level = HVA_ENCODE_SPS_LEVEL_IDC_1_3;
        break;

    case STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_2_0:
        H264EncodeContext.ControlsValues.H264Level = HVA_ENCODE_SPS_LEVEL_IDC_2_0;
        break;

    case STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_2_1:
        H264EncodeContext.ControlsValues.H264Level = HVA_ENCODE_SPS_LEVEL_IDC_2_1;
        break;

    case STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_2_2:
        H264EncodeContext.ControlsValues.H264Level = HVA_ENCODE_SPS_LEVEL_IDC_2_2;
        break;

    case STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_3_0:
        H264EncodeContext.ControlsValues.H264Level = HVA_ENCODE_SPS_LEVEL_IDC_3_0;
        break;

    case STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_3_1:
        H264EncodeContext.ControlsValues.H264Level = HVA_ENCODE_SPS_LEVEL_IDC_3_1;
        break;

    case STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_3_2:
        H264EncodeContext.ControlsValues.H264Level = HVA_ENCODE_SPS_LEVEL_IDC_3_2;
        break;

    case STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_4_0:
        H264EncodeContext.ControlsValues.H264Level = HVA_ENCODE_SPS_LEVEL_IDC_4_0;
        break;

    case STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_4_1:
        H264EncodeContext.ControlsValues.H264Level = HVA_ENCODE_SPS_LEVEL_IDC_4_1;
        break;

    case STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_4_2:
        H264EncodeContext.ControlsValues.H264Level = HVA_ENCODE_SPS_LEVEL_IDC_4_2;
        break;

    case STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_5_0:
        H264EncodeContext.ControlsValues.H264Level = HVA_ENCODE_SPS_LEVEL_IDC_5_0;
        break;

    case STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_5_1:
        H264EncodeContext.ControlsValues.H264Level = HVA_ENCODE_SPS_LEVEL_IDC_5_1;
        break;

    case STM_SE_CTRL_VALUE_VIDEO_H264_LEVEL_IDC_5_2:
        H264EncodeContext.ControlsValues.H264Level = HVA_ENCODE_SPS_LEVEL_IDC_5_2;
        break;

    default:
        SE_ERROR("Invalid level idc %u\n", Level);
        return EncoderUnsupportedControl;
    }

    SE_INFO(group_encoder_video_coder, "Level idc set to %u (%s)\n", Level, StringifyLevel(Level));
    return CoderNoError;
}

CoderStatus_t Coder_Video_Mme_H264_c::SetProfile(uint32_t Profile)
{
    switch (Profile)
    {
    case STM_SE_CTRL_VALUE_VIDEO_H264_PROFILE_IDC_BASELINE:
        H264EncodeContext.ControlsValues.H264Profile = HVA_ENCODE_SPS_PROFILE_IDC_BASELINE;
        break;

    case STM_SE_CTRL_VALUE_VIDEO_H264_PROFILE_IDC_MAIN:
        H264EncodeContext.ControlsValues.H264Profile = HVA_ENCODE_SPS_PROFILE_IDC_MAIN;
        break;

    case STM_SE_CTRL_VALUE_VIDEO_H264_PROFILE_IDC_EXTENDED:
        goto unsupported_profile;

    case STM_SE_CTRL_VALUE_VIDEO_H264_PROFILE_IDC_HIGH:
        H264EncodeContext.ControlsValues.H264Profile = HVA_ENCODE_SPS_PROFILE_IDC_HIGH;
        break;

    case STM_SE_CTRL_VALUE_VIDEO_H264_PROFILE_IDC_HIGH_10:
    case STM_SE_CTRL_VALUE_VIDEO_H264_PROFILE_IDC_HIGH_422:
    case STM_SE_CTRL_VALUE_VIDEO_H264_PROFILE_IDC_HIGH_444:
        goto unsupported_profile;

    default:
        SE_ERROR("Invalid profile idc %u\n", Profile);
        return EncoderUnsupportedControl;
    }

    SE_INFO(group_encoder_video_coder, "Profile idc set to %u (%s)\n", Profile, StringifyProfile(Profile));
    return CoderNoError;

unsupported_profile:
    SE_ERROR("Unsupported profile idc %u (%s)\n", Profile, StringifyProfile(Profile));
    return EncoderUnsupportedControl;
}

//{{{  MonitorEncodeContext
//{{{  doxynote
/// \brief                      For debug purpose, enable to monitor encode context
void Coder_Video_Mme_H264_c::MonitorEncodeContext()
{
    if (SE_IS_EXTRAVERB_ON(group_encoder_video_coder))
    {
        SE_EXTRAVERB(group_encoder_video_coder, "[Monitor encode context]\n");
        SE_EXTRAVERB(group_encoder_video_coder, "|-[Control parameters]\n");
        SE_EXTRAVERB(group_encoder_video_coder, "| |-BitRateControlMode         = %d (%s)\n", H264EncodeContext.ControlsValues.BitRateControlMode,
                     StringifyBrcType(H264EncodeContext.ControlsValues.BitRateControlMode));
        SE_EXTRAVERB(group_encoder_video_coder, "| |-BitRate                    = %u bps\n", H264EncodeContext.ControlsValues.BitRate);
        SE_EXTRAVERB(group_encoder_video_coder, "| |-GopSize                    = %u frames\n", H264EncodeContext.ControlsValues.GopSize);
        SE_EXTRAVERB(group_encoder_video_coder, "| |-H264Level                  = %d (%s)\n", H264EncodeContext.ControlsValues.H264Level, StringifyLevel(H264EncodeContext.ControlsValues.H264Level));
        SE_EXTRAVERB(group_encoder_video_coder, "| |-H264Profile                = %d (%s)\n", H264EncodeContext.ControlsValues.H264Profile, StringifyProfile(H264EncodeContext.ControlsValues.H264Profile));
        SE_EXTRAVERB(group_encoder_video_coder, "| |-CodedPictureBufferSize     = %u bits\n", H264EncodeContext.ControlsValues.CodedPictureBufferSize);
        SE_EXTRAVERB(group_encoder_video_coder, "| |-SliceMode                  = %u (%s)\n", H264EncodeContext.ControlsValues.SliceMode, StringifySliceMode(H264EncodeContext.ControlsValues.SliceMode));
        SE_EXTRAVERB(group_encoder_video_coder, "| |-SliceMaxMbSize             = %u macroblocks\n", H264EncodeContext.ControlsValues.SliceMaxMbSize);
        SE_EXTRAVERB(group_encoder_video_coder, "| |-SliceMaxByteSize           = %u bytes\n", H264EncodeContext.ControlsValues.SliceMaxByteSize);
        SE_EXTRAVERB(group_encoder_video_coder, "|\n");
        SE_EXTRAVERB(group_encoder_video_coder, "|-[Global/sequence parameters]\n");
        SE_EXTRAVERB(group_encoder_video_coder, "| |-frameWidth                 = %u pixels\n", H264EncodeContext.GlobalParams.frameWidth);
        SE_EXTRAVERB(group_encoder_video_coder, "| |-frameHeight                = %u pixels\n", H264EncodeContext.GlobalParams.frameHeight);
        SE_EXTRAVERB(group_encoder_video_coder, "| |-picOrderCntType            = %u\n", H264EncodeContext.GlobalParams.picOrderCntType);
        SE_EXTRAVERB(group_encoder_video_coder, "| |-log2MaxFrameNumMinus4      = %u\n", H264EncodeContext.GlobalParams.log2MaxFrameNumMinus4);
        SE_EXTRAVERB(group_encoder_video_coder, "| |-useConstrainedIntraFlag    = %d\n", H264EncodeContext.GlobalParams.useConstrainedIntraFlag);
        SE_EXTRAVERB(group_encoder_video_coder, "| |-intraRefreshType           = %d (%s)\n", H264EncodeContext.GlobalParams.intraRefreshType,
                     StringifyIntraRefreshType(H264EncodeContext.GlobalParams.intraRefreshType));
        if (H264EncodeContext.GlobalParams.irParamOption == HVA_ENCODE_DISABLE_INTRA_REFRESH)
        {
            SE_EXTRAVERB(group_encoder_video_coder, "| |-irParamOption              = %u\n", H264EncodeContext.GlobalParams.irParamOption);
        }
        else
        {
            SE_EXTRAVERB(group_encoder_video_coder, "| |-irParamOption              = %u %s\n", H264EncodeContext.GlobalParams.irParamOption,
                         (H264EncodeContext.GlobalParams.intraRefreshType == HVA_ENCODE_ADAPTIVE_INTRA_REFRESH) ? "macroblocks/frame refreshed" : "frames (refresh period)");
        }
        SE_EXTRAVERB(group_encoder_video_coder, "| |-maxSumNumBitsInNALU        = %u bits\n", H264EncodeContext.GlobalParams.maxSumNumBitsInNALU);
        SE_EXTRAVERB(group_encoder_video_coder, "| |-brcType                    = %d (%s)\n", H264EncodeContext.GlobalParams.brcType, StringifyBrcType(H264EncodeContext.GlobalParams.brcType));
        SE_EXTRAVERB(group_encoder_video_coder, "| |-cpbBufferSize              = %u bits\n", H264EncodeContext.GlobalParams.cpbBufferSize);
        SE_EXTRAVERB(group_encoder_video_coder, "| |-bitRate                    = %u bps\n", H264EncodeContext.GlobalParams.bitRate);
        SE_EXTRAVERB(group_encoder_video_coder, "| |-framerateNum               = %u\n", H264EncodeContext.GlobalParams.framerateNum);
        SE_EXTRAVERB(group_encoder_video_coder, "| |-framerateDen               = %u\n", H264EncodeContext.GlobalParams.framerateDen);
        SE_EXTRAVERB(group_encoder_video_coder, "| |-transformMode              = %d (%s)\n", H264EncodeContext.GlobalParams.transformMode,
                     StringifyTransformMode(H264EncodeContext.GlobalParams.transformMode));
        SE_EXTRAVERB(group_encoder_video_coder, "| |-encoderComplexity          = %d (%s)\n", H264EncodeContext.GlobalParams.encoderComplexity,
                     StringifyEncoderComplexity(H264EncodeContext.GlobalParams.encoderComplexity));
        SE_EXTRAVERB(group_encoder_video_coder, "| |-vbrInitQp                  = %u\n", H264EncodeContext.GlobalParams.vbrInitQp);
        SE_EXTRAVERB(group_encoder_video_coder, "| |-samplingMode               = %d (%s)\n", H264EncodeContext.GlobalParams.samplingMode, StringifySamplingMode(H264EncodeContext.GlobalParams.samplingMode));
        SE_EXTRAVERB(group_encoder_video_coder, "| |-cbrDelay                   = %u ms\n", H264EncodeContext.GlobalParams.cbrDelay);
        SE_EXTRAVERB(group_encoder_video_coder, "| |-qpmin                      = %u\n", H264EncodeContext.GlobalParams.qpmin);
        SE_EXTRAVERB(group_encoder_video_coder, "| |-qpmax                      = %u\n", H264EncodeContext.GlobalParams.qpmax);
        SE_EXTRAVERB(group_encoder_video_coder, "| |-sliceNumber                = %u slices\n", H264EncodeContext.GlobalParams.sliceNumber);
        SE_EXTRAVERB(group_encoder_video_coder, "| |-profileIdc                 = %d (%s)\n", H264EncodeContext.GlobalParams.profileIdc, StringifyProfile(H264EncodeContext.GlobalParams.profileIdc));
        SE_EXTRAVERB(group_encoder_video_coder, "| |-levelIdc                   = %d (%s)\n", H264EncodeContext.GlobalParams.levelIdc, StringifyLevel(H264EncodeContext.GlobalParams.levelIdc));
        SE_EXTRAVERB(group_encoder_video_coder, "| |-vuiParametersPresentFlag   = %d\n", H264EncodeContext.GlobalParams.vuiParametersPresentFlag);
        SE_EXTRAVERB(group_encoder_video_coder, "| |-vuiAspectRatioIdc          = %d (%s)\n", H264EncodeContext.GlobalParams.vuiAspectRatioIdc,
                     StringifyAspectRatio(H264EncodeContext.GlobalParams.vuiAspectRatioIdc));
        SE_EXTRAVERB(group_encoder_video_coder, "| |-vuiSarWidth                = %u pixels\n", H264EncodeContext.GlobalParams.vuiSarWidth);
        SE_EXTRAVERB(group_encoder_video_coder, "| |-vuiSarHeight               = %u pixels\n", H264EncodeContext.GlobalParams.vuiSarHeight);
        SE_EXTRAVERB(group_encoder_video_coder, "| |-vuiOverscanAppropriateFlag = %d\n", H264EncodeContext.GlobalParams.vuiOverscanAppropriateFlag);
        SE_EXTRAVERB(group_encoder_video_coder, "| |-vuiVideoFormat             = %d (%s)\n", H264EncodeContext.GlobalParams.vuiVideoFormat,
                     StringifyVideoFormat(H264EncodeContext.GlobalParams.vuiVideoFormat));
        SE_EXTRAVERB(group_encoder_video_coder, "| |-vuiVideoFullRangeFlag      = %d\n", H264EncodeContext.GlobalParams.vuiVideoFullRangeFlag);
        SE_EXTRAVERB(group_encoder_video_coder, "| |-vuiColorStd                = %d (%s)\n", H264EncodeContext.GlobalParams.vuiColorStd, StringifyColorStd(H264EncodeContext.GlobalParams.vuiColorStd));
        SE_EXTRAVERB(group_encoder_video_coder, "| |-seiPicTimingPresentFlag    = %d\n", H264EncodeContext.GlobalParams.seiPicTimingPresentFlag);
        SE_EXTRAVERB(group_encoder_video_coder, "| |-seiBufPeriodPresentFlag    = %d\n", H264EncodeContext.GlobalParams.seiBufPeriodPresentFlag);
        SE_EXTRAVERB(group_encoder_video_coder, "|\n");
        SE_EXTRAVERB(group_encoder_video_coder, "|-[Transform parameters]\n");
        SE_EXTRAVERB(group_encoder_video_coder, "| |-pictureCodingType          = %d (%s)\n", H264EncodeContext.TransformParams.pictureCodingType,
                     StringifyPictureCodingType(H264EncodeContext.TransformParams.pictureCodingType));
        SE_EXTRAVERB(group_encoder_video_coder, "| |-idrFlag                    = %d\n", H264EncodeContext.TransformParams.idrFlag);
        SE_EXTRAVERB(group_encoder_video_coder, "| |-frameNum                   = %u\n", H264EncodeContext.TransformParams.frameNum);
        SE_EXTRAVERB(group_encoder_video_coder, "| |-firstPictureInSequence     = %d\n", H264EncodeContext.TransformParams.firstPictureInSequence);
        SE_EXTRAVERB(group_encoder_video_coder, "| |-disableDeblockingFilterIdc = %d (%s)\n", H264EncodeContext.TransformParams.disableDeblockingFilterIdc,
                     StringifyDeblocking(H264EncodeContext.TransformParams.disableDeblockingFilterIdc));
        SE_EXTRAVERB(group_encoder_video_coder, "| |-sliceAlphaC0OffsetDiv2     = %d\n", H264EncodeContext.TransformParams.sliceAlphaC0OffsetDiv2);
        SE_EXTRAVERB(group_encoder_video_coder, "| |-sliceBetaOffsetDiv2        = %d\n", H264EncodeContext.TransformParams.sliceBetaOffsetDiv2);
        SE_EXTRAVERB(group_encoder_video_coder, "| |-addrSourceBuffer           = 0x%x\n", H264EncodeContext.TransformParams.addrSourceBuffer);
        SE_EXTRAVERB(group_encoder_video_coder, "| |-addrOutputBitstreamStart   = 0x%x\n", H264EncodeContext.TransformParams.addrOutputBitstreamStart);
        SE_EXTRAVERB(group_encoder_video_coder, "| |-addrOutputBitstreamEnd     = 0x%x\n", H264EncodeContext.TransformParams.addrOutputBitstreamEnd);
        SE_EXTRAVERB(group_encoder_video_coder, "| |-bitstreamOffset            = %u bits\n", H264EncodeContext.TransformParams.bitstreamOffset);
        SE_EXTRAVERB(group_encoder_video_coder, "| |-seiRecoveryPtPresentFlag   = %d\n", H264EncodeContext.TransformParams.seiRecoveryPtPresentFlag);
        SE_EXTRAVERB(group_encoder_video_coder, "| |-seiRecoveryFrameCnt        = %u\n", H264EncodeContext.TransformParams.seiRecoveryFrameCnt);
        SE_EXTRAVERB(group_encoder_video_coder, "| |-seiBrokenLinkFlag          = %d\n", H264EncodeContext.TransformParams.seiBrokenLinkFlag);
        SE_EXTRAVERB(group_encoder_video_coder, "| |-seiUsrDataT35PresentFlag   = %d\n", H264EncodeContext.TransformParams.seiUsrDataT35PresentFlag);
        SE_EXTRAVERB(group_encoder_video_coder, "| |-seiT35CountryCode          = %u\n", H264EncodeContext.TransformParams.seiT35CountryCode);
        SE_EXTRAVERB(group_encoder_video_coder, "| |-seiAddrT35PayloadByte      = %u\n", H264EncodeContext.TransformParams.seiAddrT35PayloadByte);
        SE_EXTRAVERB(group_encoder_video_coder, "| |-seiT35PayloadSize          = %u\n", H264EncodeContext.TransformParams.seiT35PayloadSize);
        SE_EXTRAVERB(group_encoder_video_coder, "| |-log2MaxFrameNumMinus4      = %u\n", H264EncodeContext.TransformParams.log2MaxFrameNumMinus4);
        SE_EXTRAVERB(group_encoder_video_coder, "|\n");
        SE_EXTRAVERB(group_encoder_video_coder, "|-[Command status]\n");
        SE_EXTRAVERB(group_encoder_video_coder, "| |-bitstreamSize              = %u bytes\n", H264EncodeContext.CommandStatus.bitstreamSize);
        SE_EXTRAVERB(group_encoder_video_coder, "| |-stuffingBits               = %u bits\n", H264EncodeContext.CommandStatus.stuffingBits);
        SE_EXTRAVERB(group_encoder_video_coder, "| |-nonVCLNALUSize             = %u bits\n", H264EncodeContext.CommandStatus.nonVCLNALUSize);
        SE_EXTRAVERB(group_encoder_video_coder, "| |-removalTime                = %u\n", H264EncodeContext.CommandStatus.removalTime);
        SE_EXTRAVERB(group_encoder_video_coder, "| |-frameEncodeDuration        = %u us\n", H264EncodeContext.CommandStatus.frameEncodeDuration);

        if (H264EncodeContext.CommandStatus.hwError)
        {
            SE_INFO(group_encoder_video_coder, "| |-hwError                   = %d\n", H264EncodeContext.CommandStatus.hwError);
        }

        SE_EXTRAVERB(group_encoder_video_coder, "| |-transformStatus            = %d (%s)\n", H264EncodeContext.CommandStatus.transformStatus,
                     StringifyTransformStatus(H264EncodeContext.CommandStatus.transformStatus));
        SE_EXTRAVERB(group_encoder_video_coder, "|\n");
        SE_EXTRAVERB(group_encoder_video_coder, "|-[Miscellaneous parameters]\n");
        SE_EXTRAVERB(group_encoder_video_coder, "| |-GlobalParamsNeedsUpdate           = %d\n", H264EncodeContext.GlobalParamsNeedsUpdate);
        SE_EXTRAVERB(group_encoder_video_coder, "| |-NewGopRequested                   = %d\n", H264EncodeContext.NewGopRequested);
        SE_EXTRAVERB(group_encoder_video_coder, "| |-FirstFrameInSequence              = %d\n", H264EncodeContext.FirstFrameInSequence);
        SE_EXTRAVERB(group_encoder_video_coder, "| |-ClosedGopType                     = %d\n", H264EncodeContext.ClosedGopType);
        SE_EXTRAVERB(group_encoder_video_coder, "| |-TransformHandleEncoder            = 0x%x\n", H264EncodeContext.TransformHandleEncoder);
        SE_EXTRAVERB(group_encoder_video_coder, "| |-FrameCount                        = %u frames processed\n", H264EncodeContext.FrameCount);
        SE_EXTRAVERB(group_encoder_video_coder, "| |-GopId                             = %u\n", H264EncodeContext.GopId);
        SE_EXTRAVERB(group_encoder_video_coder, "| |-GopSize                           = %u frames\n", H264EncodeContext.GopSize);
        SE_EXTRAVERB(group_encoder_video_coder, "| |-GlobalMMECommandCompletedCount    = %u\n", H264EncodeContext.GlobalMMECommandCompletedCount);
        SE_EXTRAVERB(group_encoder_video_coder, "| |-TransformMMECommandCompletedCount = %u\n", H264EncodeContext.TransformMMECommandCompletedCount);
        SE_EXTRAVERB(group_encoder_video_coder, "|\n");
        SE_EXTRAVERB(group_encoder_video_coder, "|-[Output metadata]\n");
        SE_EXTRAVERB(group_encoder_video_coder, "| |-system_time                 = %llu\n", H264EncodeContext.OutputMetadata.system_time);
        SE_EXTRAVERB(group_encoder_video_coder, "| |-native_time_format          = %d\n", H264EncodeContext.OutputMetadata.native_time_format);
        SE_EXTRAVERB(group_encoder_video_coder, "| |-native_time                 = %llu\n", H264EncodeContext.OutputMetadata.native_time);
        SE_EXTRAVERB(group_encoder_video_coder, "| |-encoded_time_format         = %d\n", H264EncodeContext.OutputMetadata.encoded_time_format);
        SE_EXTRAVERB(group_encoder_video_coder, "| |-encoded_time                = %llu\n", H264EncodeContext.OutputMetadata.encoded_time);
        SE_EXTRAVERB(group_encoder_video_coder, "| |-discontinuity               = %d\n", H264EncodeContext.OutputMetadata.discontinuity);
        SE_EXTRAVERB(group_encoder_video_coder, "| |-[Output video metadata]\n");
        SE_EXTRAVERB(group_encoder_video_coder, "|   |-new_gop                   = %d\n", H264EncodeContext.OutputMetadata.video.new_gop);
        SE_EXTRAVERB(group_encoder_video_coder, "|   |-closed_gop                = %d\n", H264EncodeContext.OutputMetadata.video.closed_gop);
        SE_EXTRAVERB(group_encoder_video_coder, "|   |-picture_type              = %d\n", H264EncodeContext.OutputMetadata.video.picture_type);
        SE_EXTRAVERB(group_encoder_video_coder, "|\n");
        SE_EXTRAVERB(group_encoder_video_coder, "|-[Statistics]\n");
        SE_EXTRAVERB(group_encoder_video_coder, "  |-EncodeNumber                = %d frames encoded\n", H264EncodeContext.Statistics.EncodeNumber);
        SE_EXTRAVERB(group_encoder_video_coder, "  |-EncodeNumberIPic            = %d intra frames encoded\n", H264EncodeContext.Statistics.EncodeNumberIPic);
        SE_EXTRAVERB(group_encoder_video_coder, "  |-EncodeNumberPPic            = %d inter frames encoded\n", H264EncodeContext.Statistics.EncodeNumberPPic);
        SE_EXTRAVERB(group_encoder_video_coder, "  |-EncodeSumTime               = %d us\n", H264EncodeContext.Statistics.EncodeSumTime);
        SE_EXTRAVERB(group_encoder_video_coder, "  |-EncodeSumTimeIPic           = %d us\n", H264EncodeContext.Statistics.EncodeSumTimeIPic);
        SE_EXTRAVERB(group_encoder_video_coder, "  |-EncodeSumTimePPic           = %d us\n", H264EncodeContext.Statistics.EncodeSumTimePPic);
        SE_EXTRAVERB(group_encoder_video_coder, "  |-MaxEncodeTime               = %d us\n", H264EncodeContext.Statistics.MaxEncodeTime);
        SE_EXTRAVERB(group_encoder_video_coder, "  |-MinEncodeTime               = %d us\n", H264EncodeContext.Statistics.MinEncodeTime);
        SE_EXTRAVERB(group_encoder_video_coder, "  |-MaxEncodeSize               = %d bytes\n", H264EncodeContext.Statistics.MaxEncodeSize);
        SE_EXTRAVERB(group_encoder_video_coder, "  |-MinEncodeSize               = %d bytes\n", H264EncodeContext.Statistics.MinEncodeSize);
        SE_EXTRAVERB(group_encoder_video_coder, "\n");
    }
}
//}}}

//{{{  MonitorEncodeContext
//{{{  doxynote
/// \brief                      For debug purpose, enable to monitor output metadata
void Coder_Video_Mme_H264_c::MonitorOutputMetadata(__stm_se_frame_metadata_t *CoderMetaDataDescriptor)
{
    stm_se_compressed_frame_metadata_t *OutputMetaDataDescriptor;
    stm_se_encode_process_metadata_t   *ProcessMetadataDescriptor;

    if (CoderMetaDataDescriptor == NULL)
    {
        return;
    }

    OutputMetaDataDescriptor  = &CoderMetaDataDescriptor->compressed_frame_metadata;
    ProcessMetadataDescriptor = &CoderMetaDataDescriptor->encode_process_metadata;
    if (SE_IS_EXTRAVERB_ON(group_encoder_video_coder))
    {
        SE_EXTRAVERB(group_encoder_video_coder, "[Video encode output metadata]\n");
        SE_EXTRAVERB(group_encoder_video_coder, "|-[stm_se_compressed_frame_metadata_t part]\n");
        SE_EXTRAVERB(group_encoder_video_coder, "|-|-system_time                         = %lld\n", OutputMetaDataDescriptor->system_time);
        SE_EXTRAVERB(group_encoder_video_coder, "|-|-native_time_format                  = %d\n", OutputMetaDataDescriptor->native_time_format);
        SE_EXTRAVERB(group_encoder_video_coder, "|-|-native_time                         = %lld\n", OutputMetaDataDescriptor->native_time);
        SE_EXTRAVERB(group_encoder_video_coder, "| |-encoded_time_format                 = %d\n", OutputMetaDataDescriptor->encoded_time_format);
        SE_EXTRAVERB(group_encoder_video_coder, "| |-encoded_time                        = %llu\n", OutputMetaDataDescriptor->encoded_time);
        SE_EXTRAVERB(group_encoder_video_coder, "|-|-discontinuity                       = %d\n", OutputMetaDataDescriptor->discontinuity);
        SE_EXTRAVERB(group_encoder_video_coder, "|-|-encoding                            = %d\n", OutputMetaDataDescriptor->encoding);
        SE_EXTRAVERB(group_encoder_video_coder, "|-|-[stm_se_compressed_frame_metadata_video_t part]\n");
        SE_EXTRAVERB(group_encoder_video_coder, "|-|-|-new_gop                           = %d\n", OutputMetaDataDescriptor->video.new_gop);
        SE_EXTRAVERB(group_encoder_video_coder, "|-|-|-closed_gop                        = %d\n", OutputMetaDataDescriptor->video.closed_gop);
        SE_EXTRAVERB(group_encoder_video_coder, "|-|-|-picture_type                      = %d\n", OutputMetaDataDescriptor->video.picture_type);
        SE_EXTRAVERB(group_encoder_video_coder, "|-|\n");
        SE_EXTRAVERB(group_encoder_video_coder, "|-[stm_se_encode_process_metadata_t part]\n");
        SE_EXTRAVERB(group_encoder_video_coder, "  |-updated_metadata                    = %d\n", ProcessMetadataDescriptor->updated_metadata);
        SE_EXTRAVERB(group_encoder_video_coder, "  |-[stm_se_encode_process_metadata_video_t part]\n");
        SE_EXTRAVERB(group_encoder_video_coder, "    |-encoded_picture_resolution.width  = %u pixels\n", ProcessMetadataDescriptor->video.multi_slice_mode.control);
        SE_EXTRAVERB(group_encoder_video_coder, "    |-encoded_picture_resolution.height = %u pixels\n", ProcessMetadataDescriptor->video.multi_slice_mode.control);
//FIXME : add when available
//    SE_EXTRAVERB(group_encoder_video_coder, "    |-encoded_framerate.framerate_num   = %d\n", ProcessMetadataDescriptor->video.encoded_framerate.framerate_num);
//    SE_EXTRAVERB(group_encoder_video_coder, "    |-encoded_framerate.framerate_den   = %d\n", ProcessMetadataDescriptor->video.encoded_framerate.framerate_den);
        SE_EXTRAVERB(group_encoder_video_coder, "    |-deinterlacing_on                  = %d\n", ProcessMetadataDescriptor->video.deinterlacing_on);
        SE_EXTRAVERB(group_encoder_video_coder, "    |-noise_filtering_on                = %d\n", ProcessMetadataDescriptor->video.noise_filtering_on);
        SE_EXTRAVERB(group_encoder_video_coder, "    |-bitrate_control_mode              = %u\n", ProcessMetadataDescriptor->video.bitrate_control_mode);
        SE_EXTRAVERB(group_encoder_video_coder, "    |-bitrate_control_value             = %u bps\n", ProcessMetadataDescriptor->video.bitrate_control_value);
        SE_EXTRAVERB(group_encoder_video_coder, "    |-multi_slice_mode.control          = %d\n", ProcessMetadataDescriptor->video.multi_slice_mode.control);
        SE_EXTRAVERB(group_encoder_video_coder, "    |-gop_size                          = %u frames\n", ProcessMetadataDescriptor->video.gop_size);
        SE_EXTRAVERB(group_encoder_video_coder, "    |-cpb_size                          = %u bits\n", ProcessMetadataDescriptor->video.cpb_size);
        SE_EXTRAVERB(group_encoder_video_coder, "    |-h264_level                        = %u\n", ProcessMetadataDescriptor->video.h264_level);
        SE_EXTRAVERB(group_encoder_video_coder, "    |-h264_profile                      = %u\n", ProcessMetadataDescriptor->video.h264_profile);
    }
}
//}}}

void Coder_Video_Mme_H264_c::CallbackFromMME(MME_Event_t Event, MME_Command_t *CallbackData)
{
    if (NULL == CallbackData)
    {
        SE_ERROR("No CallbackData from MME!\n");
        return;
    }

    switch (CallbackData->CmdCode)
    {
    case MME_SET_GLOBAL_TRANSFORM_PARAMS:
    {
        H264EncodeContext.GlobalMMECommandCompletedCount++;

        //
        // boost the callback priority to be the same as the CoderToOutput process
        //
        if (!MMECallbackPriorityBoosted)
        {
            OS_SetPriority(&player_tasks_desc[SE_TASK_ENCOD_VIDCTOO]);
            MMECallbackPriorityBoosted = true;
        }

        MMECommandCompletedCount++;
    }
    break;

    case MME_TRANSFORM:
    {
        if (Event == MME_COMMAND_COMPLETED_EVT)
        {
            H264EncodeContext.TransformMMECommandCompletedCount++;
        }

        MMECommandCompletedCount++;
    }
    break;

    default:
        break;
    }

    //up semaphore to warn MME processing is over
    OS_SemaphoreSignal(&H264EncodeContext.CompletionSemaphore);
}

//
// Low power enter method
// For CPS mode, all MME transformers must be terminated
//
CoderStatus_t Coder_Video_Mme_H264_c::LowPowerEnter(void)
{
    CoderStatus_t CoderStatus = CoderNoError;
    // Terminate MME transformer if needed
    IsLowPowerMMEInitialized = (H264EncodeContext.TransformHandleEncoder != NULL);

    if (IsLowPowerMMEInitialized)
    {
        CoderStatus = TerminateMMETransformer();
    }

    return CoderStatus;
}

//
// Low power exit method
// For CPS mode, all MME transformers must be re-initialized
//
CoderStatus_t Coder_Video_Mme_H264_c::LowPowerExit(void)
{
    CoderStatus_t CoderStatus = CoderNoError;

    // Re-initialize MME transformer if needed
    if (IsLowPowerMMEInitialized)
    {
        CoderStatus = InitializeMMETransformer();
    }

    return CoderStatus;
}

//This method is supposed to be called each time one of following parameter is updated: frameWidth, frameHeight, levelIdc
void Coder_Video_Mme_H264_c::UpdateMaxBitSizePerAU()
{
    H264EncodeContext.GlobalParams.maxSumNumBitsInNALU = ComputeMaxBitSizePerAU(H264EncodeContext.GlobalParams.frameWidth,   \
                                                                                H264EncodeContext.GlobalParams.frameHeight,  \
                                                                                H264EncodeContext.GlobalParams.levelIdc);
}

void Coder_Video_Mme_H264_c::DumpInputMetadata(stm_se_uncompressed_frame_metadata_t *Metadata)
{
    int i = 0;

    if (SE_IS_EXTRAVERB_ON(group_encoder_video_coder))
    {
        SE_EXTRAVERB(group_encoder_video_coder, "[INPUT CODER METADATA]\n");
        SE_EXTRAVERB(group_encoder_video_coder, "|-system_time = %llu\n", Metadata->system_time);
        SE_EXTRAVERB(group_encoder_video_coder, "|-native_time_format = %u (%s)\n", Metadata->native_time_format, StringifyTimeFormat(Metadata->native_time_format));
        SE_EXTRAVERB(group_encoder_video_coder, "|-native_time = %llu\n", Metadata->native_time);
        SE_EXTRAVERB(group_encoder_video_coder, "|-user_data_size = %u bytes\n", Metadata->user_data_size);
        SE_EXTRAVERB(group_encoder_video_coder, "|-user_data_buffer_address = 0x%p\n", Metadata->user_data_buffer_address);
        SE_EXTRAVERB(group_encoder_video_coder, "|-discontinuity = %u (%s)\n", Metadata->discontinuity, StringifyDiscontinuity(Metadata->discontinuity));
        SE_EXTRAVERB(group_encoder_video_coder, "|-media = %u (%s)\n", Metadata->media, StringifyMedia(Metadata->media));
        if (Metadata->media == STM_SE_ENCODE_STREAM_MEDIA_AUDIO)
        {
            SE_EXTRAVERB(group_encoder_video_coder, "|-[audio]\n");
            SE_EXTRAVERB(group_encoder_video_coder, "  |-[core_format]\n");
            SE_EXTRAVERB(group_encoder_video_coder, "  | |-sample_rate = %u Hz\n", Metadata->audio.core_format.sample_rate);
            SE_EXTRAVERB(group_encoder_video_coder, "  | |-[channel_placement]\n");
            SE_EXTRAVERB(group_encoder_video_coder, "  |   |-channel_count = %u\n", Metadata->audio.core_format.channel_placement.channel_count);
            for (i = 0 ; i < Metadata->audio.core_format.channel_placement.channel_count ; i++)
            {
                SE_EXTRAVERB(group_encoder_video_coder, "  |   |-chan[%d] = %u\n", i, Metadata->audio.core_format.channel_placement.chan[i]);
            }
            SE_EXTRAVERB(group_encoder_video_coder, "  |\n");
            SE_EXTRAVERB(group_encoder_video_coder, "  |-sample_format = %u (%s)\n", Metadata->audio.sample_format, StringifyPcmFormat(Metadata->audio.sample_format));
            SE_EXTRAVERB(group_encoder_video_coder, "  |-program_level = %d\n", Metadata->audio.program_level);
            SE_EXTRAVERB(group_encoder_video_coder, "  |-emphasis = %u (%s)\n", Metadata->audio.emphasis, StringifyEmphasisType(Metadata->audio.emphasis));
        }
        else if (Metadata->media == STM_SE_ENCODE_STREAM_MEDIA_VIDEO)
        {
            SE_EXTRAVERB(group_encoder_video_coder, "|-[video]\n");
            SE_EXTRAVERB(group_encoder_video_coder, "  |-[video_parameters]\n");
            SE_EXTRAVERB(group_encoder_video_coder, "  | |-width = %u pixels\n", Metadata->video.video_parameters.width);
            SE_EXTRAVERB(group_encoder_video_coder, "  | |-height = %u pixels\n", Metadata->video.video_parameters.height);
            SE_EXTRAVERB(group_encoder_video_coder, "  | |-aspect_ratio = %u (%s)\n", Metadata->video.video_parameters.aspect_ratio, StringifyAspectRatio(Metadata->video.video_parameters.aspect_ratio));
            SE_EXTRAVERB(group_encoder_video_coder, "  | |-scan_type = %u (%s)\n", Metadata->video.video_parameters.scan_type, StringifyScanType(Metadata->video.video_parameters.scan_type));
            SE_EXTRAVERB(group_encoder_video_coder, "  | |-pixel_aspect_ratio = %u/%u\n", Metadata->video.video_parameters.pixel_aspect_ratio_numerator,
                         Metadata->video.video_parameters.pixel_aspect_ratio_denominator);
            SE_EXTRAVERB(group_encoder_video_coder, "  | |-format_3d = %u (%s)\n", Metadata->video.video_parameters.format_3d, StringifyFormat3d(Metadata->video.video_parameters.format_3d));
            SE_EXTRAVERB(group_encoder_video_coder, "  | |-left_right_format = %u\n", Metadata->video.video_parameters.left_right_format);
            SE_EXTRAVERB(group_encoder_video_coder, "  | |-colorspace = %u (%s)\n", Metadata->video.video_parameters.colorspace, StringifyColorspace(Metadata->video.video_parameters.colorspace));
            SE_EXTRAVERB(group_encoder_video_coder, "  | |-frame_rate = %u fps\n", Metadata->video.video_parameters.frame_rate);
            SE_EXTRAVERB(group_encoder_video_coder, "  |\n");
            SE_EXTRAVERB(group_encoder_video_coder, "  |-[window_of_interest]\n");
            SE_EXTRAVERB(group_encoder_video_coder, "  | |-x = %u pixels\n", Metadata->video.window_of_interest.x);
            SE_EXTRAVERB(group_encoder_video_coder, "  | |-y = %u pixels\n", Metadata->video.window_of_interest.y);
            SE_EXTRAVERB(group_encoder_video_coder, "  | |-width = %u pixels\n", Metadata->video.window_of_interest.width);
            SE_EXTRAVERB(group_encoder_video_coder, "  | |-height = %u pixels\n", Metadata->video.window_of_interest.height);
            SE_EXTRAVERB(group_encoder_video_coder, "  |\n");
            SE_EXTRAVERB(group_encoder_video_coder, "  |-frame_rate = %u/%u fps\n", Metadata->video.frame_rate.framerate_num, Metadata->video.frame_rate.framerate_den);
            SE_EXTRAVERB(group_encoder_video_coder, "  |-pitch = %u bytes\n", Metadata->video.pitch);
            SE_EXTRAVERB(group_encoder_video_coder, "  |-vertical_alignment = %u lines of pixels\n", Metadata->video.vertical_alignment);
            SE_EXTRAVERB(group_encoder_video_coder, "  |-picture_type = %u (%s)\n", Metadata->video.picture_type, StringifyPictureType(Metadata->video.picture_type));
            SE_EXTRAVERB(group_encoder_video_coder, "  |-surface_format = %u (%s)\n", Metadata->video.surface_format, StringifySurfaceFormat(Metadata->video.surface_format));
            SE_EXTRAVERB(group_encoder_video_coder, "  |-top_field_first = %u\n", Metadata->video.top_field_first);
        }
        SE_EXTRAVERB(group_encoder_video_coder, "\n");
    }
}

void Coder_Video_Mme_H264_c::DumpEncodeCoordinatorMetadata(__stm_se_encode_coordinator_metadata_t *Metadata)
{
    if (SE_IS_EXTRAVERB_ON(group_encoder_video_coder))
    {
        SE_EXTRAVERB(group_encoder_video_coder, "[ENCODE COORDINATOR METADATA]\n");
        SE_EXTRAVERB(group_encoder_video_coder, "|-encoded_time_format = %u (%s)\n", Metadata->encoded_time_format, StringifyTimeFormat(Metadata->encoded_time_format));
        SE_EXTRAVERB(group_encoder_video_coder, "|-encoded_time = %llu\n", Metadata->encoded_time);
        SE_EXTRAVERB(group_encoder_video_coder, "\n");
    }
}

static uint32_t GetLevelIndex(uint32_t levelIdc)
{
    uint32_t Index;
    uint32_t LevelLimitsSize = sizeof(LevelLimits) / sizeof(LevelLimits[0]);

    for (Index = 0; Index < LevelLimitsSize; Index++)
    {
        if (levelIdc == LevelLimits[Index][0])
        {
            break;
        }
    }

    if (Index >= LevelLimitsSize)
    {
        SE_ERROR("Incorrect level idc %d\n", levelIdc);
        return 0;
    }

    return Index;
}

static uint32_t GetMinCR(uint32_t levelIdc)
{
    return LevelLimits[GetLevelIndex(levelIdc)][5];
}

static uint32_t ComputeMaxBitSizePerAU(uint32_t width, uint32_t height, uint32_t levelIdc)
{
    SE_ASSERT(GetMinCR(levelIdc) != 0);
    return 8 * (384 * ((width + 15) / 16) * ((height + 15) / 16)) / GetMinCR(levelIdc);
}

static uint32_t ComputeMaxMBPS(uint32_t levelIdc)
{
    return LevelLimits[GetLevelIndex(levelIdc)][1];
}

static uint32_t ComputeMaxFS(uint32_t levelIdc)
{
    return LevelLimits[GetLevelIndex(levelIdc)][2];
}

static uint32_t GetCpbNalFactor(uint32_t profileIdc)
{
    if ((profileIdc == HVA_ENCODE_SPS_PROFILE_IDC_HIGH)     ||
        (profileIdc == HVA_ENCODE_SPS_PROFILE_IDC_HIGH_10)  ||
        (profileIdc == HVA_ENCODE_SPS_PROFILE_IDC_HIGH_422) ||
        (profileIdc == HVA_ENCODE_SPS_PROFILE_IDC_HIGH_444))
    {
        return 1200;
    }
    else
    {
        return 1500;
    }
}

static uint32_t ComputeMaxBR(uint32_t profileIdc, uint32_t levelIdc)
{
    return GetCpbNalFactor(profileIdc) * LevelLimits[GetLevelIndex(levelIdc)][3]; // best case
}

static uint32_t ComputeMaxCPB(uint32_t profileIdc, uint32_t levelIdc)
{
    return GetCpbNalFactor(profileIdc) * LevelLimits[GetLevelIndex(levelIdc)][4]; // best case
}

bool AreColorFormatMatching(surface_format_t SurfaceFormat, H264EncodeSamplingMode_t HVASamplingMode)
{
    bool Match = false;

    if ((HVASamplingMode == HVA_ENCODE_YUV420_SEMI_PLANAR) && (SurfaceFormat == SURFACE_FORMAT_VIDEO_420_RASTER2B))
    {
        Match = true;
    }

    if ((HVASamplingMode == HVA_ENCODE_YUV422_RASTER) && (SurfaceFormat == SURFACE_FORMAT_VIDEO_422_RASTER))
    {
        Match = true;
    }

    return Match;
}

static bool AreFramerateMatching(uint16_t kpiFramerateNum, uint16_t kpiFramerateDen, uint16_t mmeFramerateNum, uint16_t mmeFramerateDen)
{
    bool Match = false;

    if ((kpiFramerateNum == mmeFramerateNum) && (kpiFramerateDen == mmeFramerateDen))
    {
        Match = true;
    }

    if ((kpiFramerateNum * mmeFramerateDen) == (mmeFramerateNum * kpiFramerateDen))
    {
        Match = true;
    }

    return Match;
}

static bool AreColorSpaceMatching(stm_se_colorspace_t ColorSpace, H264EncodeVUIColorStd_t HVAVUIColorStd)
{
    bool Match = false;

    if ((HVAVUIColorStd == HVA_ENCODE_VUI_COLOR_STD_BT_709_5) && (ColorSpace == STM_SE_COLORSPACE_BT709))
    {
        Match = true;
    }

    if ((HVAVUIColorStd == HVA_ENCODE_VUI_COLOR_STD_BT_470_6_M) && (ColorSpace == STM_SE_COLORSPACE_BT470_SYSTEM_M))
    {
        Match = true;
    }

    if ((HVAVUIColorStd == HVA_ENCODE_VUI_COLOR_STD_BT_470_6_BG) && (ColorSpace == STM_SE_COLORSPACE_BT470_SYSTEM_BG))
    {
        Match = true;
    }

    if ((HVAVUIColorStd == HVA_ENCODE_VUI_COLOR_STD_SMPTE_170M) && (ColorSpace == STM_SE_COLORSPACE_SMPTE170M))
    {
        Match = true;
    }

    if ((HVAVUIColorStd == HVA_ENCODE_VUI_COLOR_STD_SMPTE_240M) && (ColorSpace == STM_SE_COLORSPACE_SMPTE240M))
    {
        Match = true;
    }

    if ((HVAVUIColorStd == HVA_ENCODE_VUI_COLOR_STD_UNSPECIFIED) && ((ColorSpace == STM_SE_COLORSPACE_SRGB) || ColorSpace == (STM_SE_COLORSPACE_UNSPECIFIED)))
    {
        Match = true;
    }

    return Match;
}

static bool AreAspectRatioMatching(uint32_t kpiAspectRatioNum, uint32_t kpiAspectRatioDen, uint32_t mmeAspectRatioNum, uint32_t mmeAspectRatioDen)
{
    bool Match = false;

    if ((mmeAspectRatioNum * kpiAspectRatioDen) == (mmeAspectRatioDen * kpiAspectRatioNum))
    {
        Match = true;
    }

    return Match;
}

void ReleaseBuffer(Buffer_t Buffer)
{
    Buffer->DecrementReferenceCount();
}

