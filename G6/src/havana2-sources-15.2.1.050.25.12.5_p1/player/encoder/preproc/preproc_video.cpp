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

#include "preproc_video.h"

#undef TRACE_TAG
#define TRACE_TAG "Preproc_Video_c"

// Macro
#define CHECK_OUT_OF_RANGE(a,b,c) ((a<b || a>c)?1:0)

// Default control values
#define PREPROC_ENCODE_RESOLUTION_WIDTH       720
#define PREPROC_ENCODE_RESOLUTION_HEIGHT      480
#define PREPROC_ENCODE_DEINTERLACING          STM_SE_CTRL_VALUE_APPLY
#define PREPROC_ENCODE_NOISE_FILTERING        STM_SE_CTRL_VALUE_DISAPPLY
#define PREPROC_ENCODE_FRAMERATE_NUM          25
#define PREPROC_ENCODE_FRAMERATE_DEN          1
#define PREPROC_ENCODE_DISPLAY_ASPECT_RATIO   STM_SE_CTRL_VALUE_DISPLAY_ASPECT_RATIO_IGNORE
#define PREPROC_ENCODE_CROPPING               STM_SE_CTRL_VALUE_DISAPPLY

// Other constants
#define PREPROC_BUFFER_ADDRESS_ALIGN      16

#define PREPROC_TIME_BASELINE             27000000 // 27 MHz
#define PREPROC_TIME_PER_FRAME            1080000  // FPS 25000/1000
#define PREPROC_TIME_PER_FRAME_MAX        2252250  //FPS 12000/1001
#define PREPROC_TIME_PER_FRAME_MIN        450000   //FPS 60000/1000
#define PREPROC_TIME_LINE_MAX             (((uint64_t)1)<<63)

#define TIME_27MHZ_TO_US_RATIO            (27000/1000)
#define TIME_27MHZ_TO_PTS_RATIO           (27000/90)

// Internal pixel aspect ratio defines for undefined display aspect ratio
// Values should be unique but not cause error
#define PIXEL_ASPECT_RATIO_NUM_UNSPECIFIED   0
#define PIXEL_ASPECT_RATIO_DEN_UNSPECIFIED   1

const struct stm_se_picture_resolution_s Preproc_Video_c::ProfileSize[] =
{
    {  352,  288 }, // EncodeProfileCIF,
    {  720,  576 }, // EncodeProfileSD,
    { 1280,  720 }, // EncodeProfile720p,
    { 1920, 1088 }, // EncodeProfileHD,
};

Preproc_Video_c::Preproc_Video_c(void)
    : Preproc_Base_c()
    , PreprocCtrl()
    , TimePerInputField(0)
    , TimePerOutputFrame(0)
    , InputTimeLine(0)
    , OutputTimeLine(0)
    , InputFramerate()
    , OutputFramerate()
    , InputScanType()
    , FirstFrame(true)
{
    SetGroupTrace(group_encoder_video_preproc);

    SetFrameMemory(PREPROC_VIDEO_FRAME_MAX_SIZE, ENCODER_STREAM_VIDEO_MAX_PREPROC_ALLOC_BUFFERS);

    InitControl();
    InitFRC();
}

Preproc_Video_c::~Preproc_Video_c(void)
{
}

PreprocStatus_t Preproc_Video_c::Halt(void)
{
    // Call the Base Class implementation
    return Preproc_Base_c::Halt();
}

PreprocStatus_t Preproc_Video_c::Input(Buffer_t  Buffer)
{
    SE_DEBUG(group_encoder_video_preproc, "\n");

    // Call the Base Class implementation to update statistics & fill metadata descriptors
    PreprocStatus_t Status = Preproc_Base_c::Input(Buffer);
    if (Status != PreprocNoError)
    {
        PREPROC_ERROR_RUNNING("Failed to call base class, Status = %u\n", Status);
        return Status;
    }

    // Return if no data on inject frame
    // It may occur when discontinuity is sent on a void frame
    if (InputBufferSize == 0)
    {
        SE_DEBUG(group_encoder_video_preproc, "No data on inject frame, InputBufferSize = %u\n", InputBufferSize);
        return PreprocNoError;
    }

    // Check that the input buffer address respects the H/W constraints
    Status = CheckBufferAlignment(Buffer);
    if (Status != PreprocNoError)
    {
        SE_ERROR("Bad buffer alignment, Status = %u\n", Status);
        PreprocFrameBuffer->DecrementReferenceCount(IdentifierEncoderPreprocessor);
        return Status;
    }

    ////////////////////////////////
    // Check metadata consistency //
    ////////////////////////////////

    // Check media
    Status = CheckMedia(InputMetaDataDescriptor->media);
    if (Status != PreprocNoError)
    {
        SE_ERROR("Bad media metadata, Status = %u\n", Status);
        PreprocFrameBuffer->DecrementReferenceCount(IdentifierEncoderPreprocessor);
        return Status;
    }

    // Actual input resolution
    uint32_t ActualWidth = 0;
    uint32_t ActualHeight = 0;
    if (PreprocCtrl.EnableCropping == STM_SE_CTRL_VALUE_DISAPPLY)
    {
        SE_DEBUG(group_encoder_video_preproc, "Crop disabled\n");
        // If crop disabled, use input frame dimension as preproc input resolution
        ActualWidth = InputMetaDataDescriptor->video.video_parameters.width;
        ActualHeight = InputMetaDataDescriptor->video.video_parameters.height;
    }
    else
    {
        SE_DEBUG(group_encoder_video_preproc, "Crop enabled\n");
        // If crop enabled, use input frame window dimension as preproc input resolution
        ActualWidth = InputMetaDataDescriptor->video.window_of_interest.width;
        ActualHeight = InputMetaDataDescriptor->video.window_of_interest.height;
    }

    // Check resolution
    Status = CheckResolution(ActualWidth, ActualHeight);
    if (Status != PreprocNoError)
    {
        SE_ERROR("Bad resolution metadata, Status = %u\n", Status);
        PreprocFrameBuffer->DecrementReferenceCount(IdentifierEncoderPreprocessor);
        return Status;
    }

    // Check for aspect ratio
    Status = CheckAspectRatio(InputMetaDataDescriptor->video.video_parameters.aspect_ratio, InputMetaDataDescriptor->video.video_parameters.pixel_aspect_ratio_numerator,
                              InputMetaDataDescriptor->video.video_parameters.pixel_aspect_ratio_denominator, ActualWidth, ActualHeight);
    if (Status != PreprocNoError)
    {
        SE_ERROR("Bad aspect ratio metadata, Status = %u\n", Status);
        PreprocFrameBuffer->DecrementReferenceCount(IdentifierEncoderPreprocessor);
        return Status;
    }

    // Check for scan type
    Status = CheckScanType(InputMetaDataDescriptor->video.video_parameters.scan_type);
    if (Status != PreprocNoError)
    {
        SE_ERROR("Bad scan type metadata, Status = %u\n", Status);
        PreprocFrameBuffer->DecrementReferenceCount(IdentifierEncoderPreprocessor);
        return Status;
    }

    // Check that 3d format is not used
    if (InputMetaDataDescriptor->video.video_parameters.format_3d != STM_SE_FORMAT_3D_NONE)
    {
        SE_ERROR("Bad 3d format metadata %u (%s)\n", InputMetaDataDescriptor->video.video_parameters.format_3d, StringifyFormat3d(InputMetaDataDescriptor->video.video_parameters.format_3d));
        PreprocFrameBuffer->DecrementReferenceCount(IdentifierEncoderPreprocessor);
        return EncoderNotSupported;
    }

    // Check for scan type
    Status = CheckColorspace(InputMetaDataDescriptor->video.video_parameters.colorspace);
    if (Status != PreprocNoError)
    {
        SE_ERROR("Bad colorspace metadata, Status = %u\n", Status);
        PreprocFrameBuffer->DecrementReferenceCount(IdentifierEncoderPreprocessor);
        return Status;
    }

    // Check surface format
    Status = CheckSurfaceFormat(InputMetaDataDescriptor->video.surface_format, InputMetaDataDescriptor->video.video_parameters.width, InputMetaDataDescriptor->video.video_parameters.height,
                                InputBufferSize);
    if (Status != PreprocNoError)
    {
        SE_ERROR("Bad surface format metadata, Status = %u\n", Status);
        PreprocFrameBuffer->DecrementReferenceCount(IdentifierEncoderPreprocessor);
        return Status;
    }

    // Check that the input buffer color format is in line with forecasted encode control
    Status = CheckBufferColorFormat();
    if (Status != PreprocNoError)
    {
        SE_ERROR("Bad color format metadata, Status = %u\n", Status);
        PreprocFrameBuffer->DecrementReferenceCount(IdentifierEncoderPreprocessor);
        return Status;
    }

    // Check framerate
    // In tunneled mode, this framerate is build from video.video_parameters.frame_rate
    // In non-tunneled mode, video.video_parameters.frame_rate is not used at all
    Status = CheckFrameRate(InputMetaDataDescriptor->video.frame_rate);
    if (Status != PreprocNoError)
    {
        SE_ERROR("Bad framerate metadata, Status = %u\n", Status);
        PreprocFrameBuffer->DecrementReferenceCount(IdentifierEncoderPreprocessor);
        return Status;
    }

    // Check pitch
    Status = CheckPitch(InputMetaDataDescriptor->video.pitch, InputMetaDataDescriptor->video.video_parameters.width, InputMetaDataDescriptor->video.surface_format);
    if (Status != PreprocNoError)
    {
        SE_ERROR("Bad pitch metadata, Status = %u\n", Status);
        PreprocFrameBuffer->DecrementReferenceCount(IdentifierEncoderPreprocessor);
        return Status;
    }

    // Check vertical alignment
    Status = CheckVerticalAlignment(InputMetaDataDescriptor->video.vertical_alignment, InputMetaDataDescriptor->video.video_parameters.height, InputMetaDataDescriptor->video.surface_format,
                                    InputBufferSize);
    if (Status != PreprocNoError)
    {
        SE_ERROR("Bad vertical alignment metadata, Status = %u\n", Status);
        PreprocFrameBuffer->DecrementReferenceCount(IdentifierEncoderPreprocessor);
        return Status;
    }

    // Check picture type
    // This field is not used by encoder
    // We still check that the value matches a picture type enum
    Status = CheckPictureType(InputMetaDataDescriptor->video.picture_type);
    if (Status != PreprocNoError)
    {
        SE_ERROR("Bad picture type metadata, Status = %u\n", Status);
        PreprocFrameBuffer->DecrementReferenceCount(IdentifierEncoderPreprocessor);
        return Status;
    }

    // Check discontinuity applicable to video
    if ((PreprocDiscontinuity & STM_SE_DISCONTINUITY_MUTE)
        || (PreprocDiscontinuity & STM_SE_DISCONTINUITY_FADEOUT)
        || (PreprocDiscontinuity & STM_SE_DISCONTINUITY_FADEIN))
    {
        SE_ERROR("Discontinuity MUTE/FADEOUT/FADEIN request not applicable to video\n");
        PreprocFrameBuffer->DecrementReferenceCount(IdentifierEncoderPreprocessor);
        return EncoderNotSupported;
    }

    /////////////////////////
    // Set default control //
    /////////////////////////

    // Initialize default resolution if not set by control
    if (!PreprocCtrl.ResolutionSet)
    {
        SE_DEBUG(group_encoder_video_preproc, "Resolution control not set, set input and output resolution to the same value %ux%u\n", ActualWidth, ActualHeight);
        PreprocCtrl.Resolution.width  = ActualWidth;
        PreprocCtrl.Resolution.height = ActualHeight;
    }

    // Initialize default framerate if not set by control
    if (!PreprocCtrl.FramerateSet)
    {
        PreprocCtrl.Framerate         = InputMetaDataDescriptor->video.frame_rate;
    }

    ////////////////////////////////////////////
    // Check metadata and control consistency //
    ////////////////////////////////////////////

    // Check for unsupported aspect ratio
    Status = CheckAspectRatioSupport();
    if (Status != PreprocNoError)
    {
        SE_ERROR("Aspect ratio not supported, Status = %u\n", Status);
        PreprocFrameBuffer->DecrementReferenceCount(IdentifierEncoderPreprocessor);
        return Status;
    }

    // Check for unsupported output framerate
    Status = CheckFrameRateSupport();
    if (Status != PreprocNoError)
    {
        SE_ERROR("Frame rate not supported, Status = %u\n", Status);
        PreprocFrameBuffer->DecrementReferenceCount(IdentifierEncoderPreprocessor);
        return Status;
    }

    return PreprocNoError;
}

void Preproc_Video_c::EntryTrace(stm_se_uncompressed_frame_metadata_t *metadata, PreprocVideoCtrl_t *control)
{
    if (FirstFrame && metadata && control)
    {
        SE_INFO(group_encoder_video_preproc,
                "[ENCPP] > %ux%u pixels, CROP %s [%u,%u,%u,%u] => %ux%u pixels, %u/%u => %u/%u fps, DEI %s, PAR (%u/%u), DAR %u (%s) => %u (%s), TNR %s, FORMAT %u (%s), PITCH %u bytes, COLOR %u (%s), SCAN %u (%s), VALIGN %u lines\n",
                metadata->video.video_parameters.width,
                metadata->video.video_parameters.height,
                control->EnableCropping ? "on" : "off",
                metadata->video.window_of_interest.x,
                metadata->video.window_of_interest.y,
                metadata->video.window_of_interest.width,
                metadata->video.window_of_interest.height,
                control->Resolution.width,
                control->Resolution.height,
                metadata->video.frame_rate.framerate_num,
                metadata->video.frame_rate.framerate_den,
                control->Framerate.framerate_num,
                control->Framerate.framerate_den,
                control->EnableDeInterlacer ? "on" : "off",
                metadata->video.video_parameters.pixel_aspect_ratio_numerator,
                metadata->video.video_parameters.pixel_aspect_ratio_denominator,
                metadata->video.video_parameters.aspect_ratio,
                StringifyAspectRatio(metadata->video.video_parameters.aspect_ratio),
                control->DisplayAspectRatio,
                StringifyDisplayAspectRatio(control->DisplayAspectRatio),
                control->EnableNoiseFilter ? "on" : "off",
                metadata->video.surface_format,
                StringifySurfaceFormat(metadata->video.surface_format),
                metadata->video.pitch,
                metadata->video.video_parameters.colorspace,
                StringifyColorspace(metadata->video.video_parameters.colorspace),
                metadata->video.video_parameters.scan_type,
                StringifyScanType(metadata->video.video_parameters.scan_type),
                metadata->video.vertical_alignment);

        FirstFrame = false;
    }
}

void Preproc_Video_c::ExitTrace(stm_se_uncompressed_frame_metadata_t *metadata, PreprocVideoCtrl_t *control)
{
    if (metadata && control)
    {
        if (FirstFrame)
        {
            EntryTrace(metadata, control);
            FirstFrame = false;
        }

        SE_INFO(group_encoder_video_preproc,
                "[ENCPP] < %ux%u pixels, CROP %s [%u,%u,%u,%u] => %ux%u pixels, %u/%u => %u/%u fps, DEI %s, PAR (%u/%u), DAR %u (%s) => %u (%s), TNR %s, FORMAT %u (%s), PITCH %u bytes, COLOR %u (%s), SCAN %u (%s), VALIGN %u lines\n",
                metadata->video.video_parameters.width,
                metadata->video.video_parameters.height,
                control->EnableCropping ? "on" : "off",
                metadata->video.window_of_interest.x,
                metadata->video.window_of_interest.y,
                metadata->video.window_of_interest.width,
                metadata->video.window_of_interest.height,
                control->Resolution.width,
                control->Resolution.height,
                metadata->video.frame_rate.framerate_num,
                metadata->video.frame_rate.framerate_den,
                control->Framerate.framerate_num,
                control->Framerate.framerate_den,
                control->EnableDeInterlacer ? "on" : "off",
                metadata->video.video_parameters.pixel_aspect_ratio_numerator,
                metadata->video.video_parameters.pixel_aspect_ratio_denominator,
                metadata->video.video_parameters.aspect_ratio,
                StringifyAspectRatio(metadata->video.video_parameters.aspect_ratio),
                control->DisplayAspectRatio,
                StringifyDisplayAspectRatio(control->DisplayAspectRatio),
                control->EnableNoiseFilter ? "on" : "off",
                metadata->video.surface_format,
                StringifySurfaceFormat(metadata->video.surface_format),
                metadata->video.pitch,
                metadata->video.video_parameters.colorspace,
                StringifyColorspace(metadata->video.video_parameters.colorspace),
                metadata->video.video_parameters.scan_type,
                StringifyScanType(metadata->video.video_parameters.scan_type),
                metadata->video.vertical_alignment);
    }
}

void Preproc_Video_c::InitControl()
{
    PreprocCtrl.Resolution.width        = PREPROC_ENCODE_RESOLUTION_WIDTH;
    PreprocCtrl.Resolution.height       = PREPROC_ENCODE_RESOLUTION_HEIGHT;
    PreprocCtrl.EnableDeInterlacer      = PREPROC_ENCODE_DEINTERLACING;
    PreprocCtrl.EnableNoiseFilter       = PREPROC_ENCODE_NOISE_FILTERING;
    PreprocCtrl.Framerate.framerate_num = PREPROC_ENCODE_FRAMERATE_NUM;
    PreprocCtrl.Framerate.framerate_den = PREPROC_ENCODE_FRAMERATE_DEN;
    PreprocCtrl.ResolutionSet           = false;
    PreprocCtrl.FramerateSet            = false;
    PreprocCtrl.DisplayAspectRatio      = PREPROC_ENCODE_DISPLAY_ASPECT_RATIO;
    PreprocCtrl.EnableCropping          = PREPROC_ENCODE_CROPPING;
}

PreprocStatus_t Preproc_Video_c::GetControl(stm_se_ctrl_t    Control,
                                            void            *Data)
{
    switch (Control)
    {
    case STM_SE_CTRL_VIDEO_ENCODE_STREAM_DEINTERLACING:
        *(uint32_t *)Data = PreprocCtrl.EnableDeInterlacer;
        break;

    case STM_SE_CTRL_VIDEO_ENCODE_STREAM_NOISE_FILTERING:
        *(uint32_t *)Data = PreprocCtrl.EnableNoiseFilter;
        break;

    case STM_SE_CTRL_VIDEO_ENCODE_STREAM_DISPLAY_ASPECT_RATIO:
        *(uint32_t *)Data = PreprocCtrl.DisplayAspectRatio;
        break;

    case STM_SE_CTRL_VIDEO_ENCODE_STREAM_INPUT_WINDOW_CROPPING:
        *(uint32_t *)Data = PreprocCtrl.EnableCropping;
        break;

    default:
        // Cannot trace this as error because it can be a valid control for another object
        SE_DEBUG(group_encoder_video_preproc, "Not match preproc video control %u\n", Control);
        return PreprocControlNotMatch;
    }

    return PreprocNoError;
}

PreprocStatus_t Preproc_Video_c::GetCompoundControl(stm_se_ctrl_t    Control,
                                                    void            *Data)
{
    switch (Control)
    {
    case STM_SE_CTRL_VIDEO_ENCODE_STREAM_RESOLUTION:
        // We assume that the client point to the correct structure
        *(stm_se_picture_resolution_t *)Data = PreprocCtrl.Resolution;
        break;

    case STM_SE_CTRL_VIDEO_ENCODE_STREAM_FRAMERATE:
        // We assume that the client point to the correct structure
        *(stm_se_framerate_t *)Data = PreprocCtrl.Framerate;
        break;

    default:
        // Cannot trace this as error because it can be a valid control for another object
        SE_DEBUG(group_encoder_video_preproc, "Not match preproc video control %u\n", Control);
        return PreprocControlNotMatch;
    }

    return PreprocNoError;
}

PreprocStatus_t Preproc_Video_c::SetControl(stm_se_ctrl_t    Control,
                                            const void      *Data)
{
    uint32_t                    *EnableDeInterlacer;
    uint32_t                    *EnableNoiseFilter;
    uint32_t                    *DisplayAspectRatio;
    uint32_t                    *EnableCropping;

    switch (Control)
    {
    case STM_SE_CTRL_VIDEO_ENCODE_STREAM_DEINTERLACING:
        EnableDeInterlacer = (uint32_t *)Data;

        if ((*EnableDeInterlacer != STM_SE_CTRL_VALUE_DISAPPLY) && (*EnableDeInterlacer != STM_SE_CTRL_VALUE_APPLY))
        {
            SE_ERROR("Control VIDEO_ENCODE_STREAM_DEINTERLACING %d is not in allowed range\n", *EnableDeInterlacer);
            return PreprocUnsupportedControl;
        }

        SE_INFO(group_encoder_video_preproc, "Deinterlacing sets to %u (%s)\n", *EnableDeInterlacer, StringifyGenericValue(*EnableDeInterlacer));
        PreprocCtrl.EnableDeInterlacer = *EnableDeInterlacer;
        break;

    case STM_SE_CTRL_VIDEO_ENCODE_STREAM_NOISE_FILTERING:
        // Today TNR is currently not supported, and even not requested (see bz 29035).
        // Thus we disallow to enable it for all kind of video preprocessors
        EnableNoiseFilter = (uint32_t *)Data;

        if ((*EnableNoiseFilter != STM_SE_CTRL_VALUE_DISAPPLY)/* && (*EnableNoiseFilter != STM_SE_CTRL_VALUE_APPLY)*/)
        {
            SE_ERROR("Control VIDEO_ENCODE_STREAM_NOISE_FILTERING %d is not in allowed range\n", *EnableNoiseFilter);
            return PreprocUnsupportedControl;
        }

        SE_INFO(group_encoder_video_preproc, "Noise filtering sets to %u (%s)\n", *EnableNoiseFilter, StringifyGenericValue(*EnableNoiseFilter));
        PreprocCtrl.EnableNoiseFilter  = *EnableNoiseFilter;
        break;

    case STM_SE_CTRL_VIDEO_ENCODE_STREAM_DISPLAY_ASPECT_RATIO:
        DisplayAspectRatio = (uint32_t *)Data;

        if ((*DisplayAspectRatio != STM_SE_CTRL_VALUE_DISPLAY_ASPECT_RATIO_IGNORE)  &&
            (*DisplayAspectRatio != STM_SE_CTRL_VALUE_DISPLAY_ASPECT_RATIO_4_BY_3)  &&
            (*DisplayAspectRatio != STM_SE_CTRL_VALUE_DISPLAY_ASPECT_RATIO_16_BY_9))
        {
            SE_ERROR("Control STM_SE_CTRL_VIDEO_ENCODE_STREAM_ASPECT_RATIO %d is not in allowed range\n", *DisplayAspectRatio);
            return PreprocUnsupportedControl;
        }

        SE_INFO(group_encoder_video_preproc, "Display aspect ratio sets to %u (%s)\n", *DisplayAspectRatio, StringifyDisplayAspectRatio(*DisplayAspectRatio));
        PreprocCtrl.DisplayAspectRatio  = *DisplayAspectRatio;
        break;

    case STM_SE_CTRL_VIDEO_ENCODE_STREAM_INPUT_WINDOW_CROPPING:
        EnableCropping = (uint32_t *)Data;

        if ((*EnableCropping != STM_SE_CTRL_VALUE_DISAPPLY) && (*EnableCropping != STM_SE_CTRL_VALUE_APPLY))
        {
            SE_ERROR("Control STM_SE_CTRL_VIDEO_ENCODE_STREAM_INPUT_WINDOW_CROPPING %d is not in allowed range\n", *EnableCropping);
            return PreprocUnsupportedControl;
        }

        SE_INFO(group_encoder_video_preproc, "Input window cropping set to %u (%s)\n", *EnableCropping, StringifyGenericValue(*EnableCropping));
        PreprocCtrl.EnableCropping = *EnableCropping;
        break;

    default:
        // Cannot trace this as error because it can be a valid control for another object
        SE_DEBUG(group_encoder_video_preproc, "Not match preproc video control %u\n", Control);
        return PreprocControlNotMatch;
    }

    return PreprocNoError;
}

PreprocStatus_t Preproc_Video_c::SetCompoundControl(stm_se_ctrl_t    Control,
                                                    const void      *Data)
{
    stm_se_picture_resolution_t *Resolution;
    stm_se_framerate_t          *Framerate;
    uint64_t                     FramerateMul;

    switch (Control)
    {
    case STM_SE_CTRL_VIDEO_ENCODE_STREAM_RESOLUTION:
        // We assume that the client point to the correct structure
        Resolution = (stm_se_picture_resolution_t *)Data;

        if (CHECK_OUT_OF_RANGE((Resolution->width), ENCODE_WIDTH_MIN, ENCODE_WIDTH_MAX))
        {
            SE_ERROR("Control VIDEO_ENCODE_STREAM_RESOLUTION width %d is not in allowed range\n", Resolution->width);
            return PreprocUnsupportedControl;
        }

        if (CHECK_OUT_OF_RANGE((Resolution->height), ENCODE_HEIGHT_MIN, ENCODE_HEIGHT_MAX))
        {
            SE_ERROR("Control VIDEO_ENCODE_STREAM_RESOLUTION height %d is not in allowed range\n", Resolution->height);
            return PreprocUnsupportedControl;
        }

        //check if requested video encode resolution is compatible with requested profile
        if (IsRequestedResolutionCompatibleWithProfile(Resolution->width, Resolution->height))
        {
            SE_INFO(group_encoder_video_preproc, "Resolution sets to %ux%u pixels\n", Resolution->width, Resolution->height);
            PreprocCtrl.Resolution         = *Resolution;
            PreprocCtrl.ResolutionSet      = true;
        }
        else
        {
            SE_ERROR("Control VIDEO_ENCODE_STREAM_RESOLUTION (height = %u , width = %u ) is not compatible with encode_stream profile %u\n", \
                     Resolution->height, Resolution->width, Encoder.EncodeStream->GetVideoEncodeMemoryProfile());
            return PreprocUnsupportedControl;
        }
        break;

    case STM_SE_CTRL_VIDEO_ENCODE_STREAM_FRAMERATE:
        // We assume that the client point to the correct structure
        Framerate     = (stm_se_framerate_t *)Data;
        if ((Framerate->framerate_num == 0) || (Framerate->framerate_den == 0))
        {
            SE_ERROR("Control VIDEO_ENCODE_STREAM_FRAMERATE %u/%u is not valid\n", Framerate->framerate_num, Framerate->framerate_den);
            return PreprocError;
        }
        FramerateMul  = (uint64_t)Framerate->framerate_num * STM_SE_PLAY_FRAME_RATE_MULTIPLIER;
        FramerateMul /= Framerate->framerate_den;

        if (CHECK_OUT_OF_RANGE(FramerateMul, ENCODE_FRAMERATE_MIN, ENCODE_FRAMERATE_MAX))
        {
            SE_ERROR("Control VIDEO_ENCODE_STREAM_FRAMERATE %lld (%d*%d/%d) is not in allowed range\n",
                     FramerateMul, Framerate->framerate_num, STM_SE_PLAY_FRAME_RATE_MULTIPLIER, Framerate->framerate_den);
            return PreprocUnsupportedControl;
        }

        SE_INFO(group_encoder_video_preproc, "Framerate sets to %u/%u fps\n", Framerate->framerate_num, Framerate->framerate_den);
        PreprocCtrl.Framerate          = *Framerate;
        PreprocCtrl.FramerateSet       = true;
        break;

    default:
        // Cannot trace this as error because it can be a valid control for another object
        SE_DEBUG(group_encoder_video_preproc, "Not match preproc video control %u\n", Control);
        return PreprocControlNotMatch;
    }

    return PreprocNoError;
}

PreprocStatus_t Preproc_Video_c::ManageMemoryProfile(void)
{
    uint32_t PreProcFrameSize = 0;
    uint32_t PreProcBufferNumber = 0;

    EvaluatePreprocBufferNeeds(&PreProcFrameSize, &PreProcBufferNumber);
    SetFrameMemory(PreProcFrameSize, PreProcBufferNumber);
    SE_DEBUG(group_encoder_video_preproc, "From profile, evaluated PreProcFrameSize = %u PreProcBufferNumber= %u\n", PreProcFrameSize, PreProcBufferNumber);

    return PreprocNoError;
}

void Preproc_Video_c::EvaluatePreprocFrameSize(uint32_t *PreProcFrameSize)
{
    VideoEncodeMemoryProfile_t MemoryProfile = Encoder.Encode->GetVideoEncodeMemoryProfile();

    switch (MemoryProfile)
    {
    //Computed size takes into account a YUV4:2:0 preproc output format, hence 3/2 bytes per pixel
    case EncodeProfileHD:
    //3133440 bytes
    case EncodeProfile720p:
    //1382400 bytes
    case EncodeProfileSD:
    //622080 bytes
    case EncodeProfileCIF:
        //152064 bytes
        *PreProcFrameSize = (ProfileSize[MemoryProfile].width * ProfileSize[MemoryProfile].height * 3) / 2;
        SE_INFO(group_encoder_video_preproc, "Memory profile = %u (%s), preproc buffer size = %u\n", MemoryProfile, StringifyMemoryProfile(MemoryProfile), *PreProcFrameSize);
        break;
    //EncodeProfileDynamic case (not yet supported) already filtered at control level
    default:
        *PreProcFrameSize = (ProfileSize[EncodeProfileHD].width * ProfileSize[EncodeProfileHD].height * 3) / 2;
        SE_WARNING("Invalid internal memory profile, set then to HD profile\n");
        break;
    }
}

void Preproc_Video_c::EvaluatePreprocBufferNeeds(uint32_t *PreProcFrameSize, uint32_t *PreProcBufferNumber)
{
    surface_format_t InputColorFormatForecasted = Encoder.Encode->GetVideoInputColorFormatForecasted();
    VideoEncodeMemoryProfile_t MemoryProfile = Encoder.Encode->GetVideoEncodeMemoryProfile();

    //'Latch 'encode' profile information for this encode stream
    //store encode_stream forecasted color format for future checks on injected frame color format
    Encoder.EncodeStream->SetVideoInputColorFormatForecasted(InputColorFormatForecasted);
    //store encode_stream profile information for future checks on encode resolution control
    Encoder.EncodeStream->SetVideoEncodeMemoryProfile(MemoryProfile);

    switch (InputColorFormatForecasted)
    {
    //Computed size takes into account a YUV4:2:0 preproc output format, hence 3/2 bytes per pixel
    case SURFACE_FORMAT_VIDEO_422_YUYV:
        //intermediate preproc buffer requested to handle this input color format, thus all buffers needed
        *PreProcBufferNumber = ENCODER_STREAM_VIDEO_MAX_PREPROC_ALLOC_BUFFERS;
        //in that case, no preproc buffer size optimization as intermediate buffer is sized with input frame that can change on the fly
        //preproc buffer sized for HD frame in YUV420
        *PreProcFrameSize = (3 * 1920 * 1088) / 2;
        break;
    default:
        //in that case, intermediate preproc buffer not needed and buffer size can be optimized vs profile
        *PreProcBufferNumber = ENCODER_STREAM_VIDEO_MAX_PREPROC_ALLOC_BUFFERS - 1;
        EvaluatePreprocFrameSize(PreProcFrameSize);
        break;
    }
}

PreprocStatus_t Preproc_Video_c::CheckBufferAlignment(Buffer_t  Buffer)
{
    uint32_t     DataSize;
    void        *BufferAddrPhys;
    void        *BufferAddrVirt;

    // Get buffer physical address
    Buffer->ObtainDataReference(NULL, &DataSize, (void **)(&BufferAddrPhys), PhysicalAddress);
    if (BufferAddrPhys == NULL)
    {
        SE_ERROR("Unable to obtain buffer physical address\n");
        return PreprocError;
    }

    // Get buffer virtual address
    Buffer->ObtainDataReference(NULL, &DataSize, (void **)(&BufferAddrVirt), CachedAddress);
    if (BufferAddrVirt == NULL)
    {
        SE_ERROR("Unable to obtain buffer virtual address\n");
        return PreprocError;
    }

    // Check that the buffer physical and virtual address respects the alignment constraint, as required by video preprocessing unit.
    // 16-byte alignment is required at least for the fvdp-encode IP. As we do not know in SE what video preprocessing IP will be eventually used (fvdp-encode or blitter), we always enforce this condition.
    if ((uint32_t)BufferAddrPhys % PREPROC_BUFFER_ADDRESS_ALIGN != 0)
    {
        SE_ERROR("Physical address %p is not %d-bytes aligned - cannot be used by pre-processing unit, BufferAddrVirt = %p\n", BufferAddrPhys, PREPROC_BUFFER_ADDRESS_ALIGN, BufferAddrVirt);
        return PreprocError;
    }
    if ((uint32_t)BufferAddrVirt % PREPROC_BUFFER_ADDRESS_ALIGN != 0)
    {
        SE_ERROR("Virtual address %p is not %d-bytes aligned - cannot be used by pre-processing unit, BufferAddrPhys = %p\n", BufferAddrVirt, PREPROC_BUFFER_ADDRESS_ALIGN, BufferAddrPhys);
        return PreprocError;
    }

    return PreprocNoError;
}

PreprocStatus_t Preproc_Video_c::CheckMedia(stm_se_encode_stream_media_t Media)
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
        return EncoderNotSupported;
    }

    return EncoderNoError;

unsupported_media:
    SE_ERROR("Unsupported media %u (%s)\n", Media, StringifyMedia(Media));
    return EncoderNotSupported;
}

PreprocStatus_t Preproc_Video_c::CheckResolution(uint32_t Width, uint32_t Height)
{
    if (CHECK_OUT_OF_RANGE(Width, PREPROC_WIDTH_MIN, PREPROC_WIDTH_MAX))
    {
        SE_ERROR("Metadata width %d is not in allowed range\n", Width);
        return PreprocError;
    }

    if (CHECK_OUT_OF_RANGE(Height, PREPROC_HEIGHT_MIN, PREPROC_HEIGHT_MAX))
    {
        SE_ERROR("Metadata height %d is not in allowed range\n", Height);
        return PreprocError;
    }

    return PreprocNoError;
}

PreprocStatus_t Preproc_Video_c::CheckScanType(stm_se_scan_type_t ScanType)
{
    switch (ScanType)
    {
    case STM_SE_SCAN_TYPE_PROGRESSIVE:
    case STM_SE_SCAN_TYPE_INTERLACED:
        break;
    default:
        SE_ERROR("Invalid scan type %u\n", ScanType);
        return EncoderNotSupported;
    }

    return EncoderNoError;
}

PreprocStatus_t Preproc_Video_c::CheckColorspace(stm_se_colorspace_t Colorspace)
{
    switch (Colorspace)
    {
    case STM_SE_COLORSPACE_UNSPECIFIED:
    case STM_SE_COLORSPACE_SMPTE170M:
    case STM_SE_COLORSPACE_SMPTE240M:
    case STM_SE_COLORSPACE_BT709:
    case STM_SE_COLORSPACE_BT470_SYSTEM_M:
    case STM_SE_COLORSPACE_BT470_SYSTEM_BG:
    case STM_SE_COLORSPACE_SRGB:
        break;
    default:
        SE_ERROR("Invalid colorspace %u\n", Colorspace);
        return EncoderNotSupported;
    }

    return EncoderNoError;
}

PreprocStatus_t Preproc_Video_c::CheckSurfaceFormat(surface_format_t SurfaceFormat, uint32_t Width, uint32_t Height, uint32_t InputBufferSize)
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
    case SURFACE_FORMAT_VIDEO_420_PLANAR:
        if ((Width * Height * GetBitPerPixel(SurfaceFormat) / 8) > InputBufferSize)
        {
            goto inconsistent_color_format;
        }
        break;
    case SURFACE_FORMAT_VIDEO_420_PLANAR_ALIGNED:
    case SURFACE_FORMAT_VIDEO_422_PLANAR:
    case SURFACE_FORMAT_VIDEO_8888_ARGB:
    case SURFACE_FORMAT_VIDEO_888_RGB:
    case SURFACE_FORMAT_VIDEO_565_RGB:
        goto unsupported_color_format;
    case SURFACE_FORMAT_VIDEO_422_YUYV:
    case SURFACE_FORMAT_VIDEO_420_RASTER2B:
        if ((Width * Height * GetBitPerPixel(SurfaceFormat) / 8) > InputBufferSize)
        {
            goto inconsistent_color_format;
        }
        break;
    default:
        SE_ERROR("Invalid surface format %u\n", SurfaceFormat);
        return EncoderNotSupported;
    }

    return PreprocNoError;

unsupported_color_format:
    SE_ERROR("Unsupported surface format %u (%s)\n", SurfaceFormat, StringifySurfaceFormat(SurfaceFormat));
    return EncoderNotSupported;

inconsistent_color_format:
    SE_ERROR("Inconsistent surface format %u (%s), Width = %u, Height = %u, InputBufferSize = %u\n", SurfaceFormat, StringifySurfaceFormat(SurfaceFormat), Width, Height, InputBufferSize);
    return EncoderNotSupported;
}

PreprocStatus_t Preproc_Video_c::CheckBufferColorFormat()
{
    surface_format_t BufferInputColorFormat;
    surface_format_t VideoEncodeInputColorFormatForecasted = Encoder.EncodeStream->GetVideoInputColorFormatForecasted();

    // Check that the input buffer color format respects 'forecasted' color format of this encode_stream
    BufferInputColorFormat     = InputMetaDataDescriptor->video.surface_format;

    //If intermediate preproc buffer not budgeted, input buffer with SURFACE_FORMAT_VIDEO_422_YUYV should be rejected as can't be processed
    if ((VideoEncodeInputColorFormatForecasted != SURFACE_FORMAT_VIDEO_422_YUYV) && (BufferInputColorFormat == SURFACE_FORMAT_VIDEO_422_YUYV))
    {
        SE_ERROR("Input Buffer color format (%d) is not compatible with STM_SE_CTRL_VIDEO_ENCODE_INPUT_COLOR_FORMAT (%d) previous control\n", BufferInputColorFormat, VideoEncodeInputColorFormatForecasted);
        return PreprocError;
    }

    return PreprocNoError;
}

int32_t Preproc_Video_c::GetBitPerPixel(surface_format_t SurfaceFormat)
{
    uint32_t bitPerPixel = 0;

    switch (SurfaceFormat)
    {
    case SURFACE_FORMAT_VIDEO_422_RASTER:
        bitPerPixel = 16;
        break;
    case SURFACE_FORMAT_VIDEO_420_PLANAR:
        bitPerPixel = 12;
        break;
    case SURFACE_FORMAT_VIDEO_422_YUYV:
        bitPerPixel = 16;
        break;
    case SURFACE_FORMAT_VIDEO_420_RASTER2B:
        bitPerPixel = 12;
        break;
    default:
        SE_ERROR("Invalid surface format %u\n", SurfaceFormat);
        return -1;
    }

    return bitPerPixel;
}

int32_t Preproc_Video_c::GetMinPitch(uint32_t Width, surface_format_t SurfaceFormat)
{
    uint32_t minPitch = 0;

    switch (SurfaceFormat)
    {
    case SURFACE_FORMAT_VIDEO_422_RASTER:
        minPitch = Width * 2;
        break;
    case SURFACE_FORMAT_VIDEO_420_PLANAR:
        minPitch = Width;
        break;
    case SURFACE_FORMAT_VIDEO_422_YUYV:
        minPitch = Width * 2;
        break;
    case SURFACE_FORMAT_VIDEO_420_RASTER2B:
        minPitch = Width;
        break;
    default:
        SE_ERROR("Invalid surface format %u\n", SurfaceFormat);
        return -1;
    }

    return minPitch;
}

PreprocStatus_t Preproc_Video_c::CheckPitch(uint32_t Pitch, uint32_t Width, surface_format_t SurfaceFormat)
{
    if (Pitch < GetMinPitch(Width, SurfaceFormat))
    {
        SE_ERROR("Invalid pitch %u, Width = %u, SurfaceFormat = %u (%s)\n", Pitch, Width, SurfaceFormat, StringifySurfaceFormat(SurfaceFormat));
        return EncoderNotSupported;
    }

    return PreprocNoError;
}

PreprocStatus_t Preproc_Video_c::CheckVerticalAlignment(uint32_t VerticalAlignment, uint32_t Height, surface_format_t SurfaceFormat, uint32_t BufferSize)
{
    uint32_t ExpectedDataSize = 0;

    switch (SurfaceFormat)
    {
    case SURFACE_FORMAT_VIDEO_422_RASTER:
    case SURFACE_FORMAT_VIDEO_420_PLANAR:
    case SURFACE_FORMAT_VIDEO_420_RASTER2B:
        // When vertical alignment is 0, it is then replaced by default value
        if (VerticalAlignment)
        {
            if ((VerticalAlignment != 1) && (VerticalAlignment != 16) && (VerticalAlignment != 32) && (VerticalAlignment != 64))
            {
                SE_ERROR("Invalid vertical alignment %u\n", VerticalAlignment);
                return EncoderNotSupported;
            }

            ExpectedDataSize = ALIGN_UP(Height, VerticalAlignment) + Height / 2;
            if (ExpectedDataSize > BufferSize)
            {
                SE_ERROR("Invalid vertical alignment %u (ExpectedDataSize = %u, BufferSize = %u)\n", VerticalAlignment, ExpectedDataSize, BufferSize);
                return EncoderNotSupported;
            }
        }
        break;
    case SURFACE_FORMAT_VIDEO_422_YUYV:
        if (VerticalAlignment)
        {
            SE_WARNING_ONCE("Vertical alignment is set to %u but it is not used, SurfaceFormat = %u (%s)\n", VerticalAlignment, SurfaceFormat, StringifySurfaceFormat(SurfaceFormat));
        }
        break;
    default:
        SE_ERROR("Invalid surface format %u\n", SurfaceFormat);
        return EncoderNotSupported;
    }

    return PreprocNoError;
}

PreprocStatus_t Preproc_Video_c::CheckPictureType(stm_se_picture_type_t PictureType)
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
        return EncoderNotSupported;
    }

    return PreprocNoError;
}

bool Preproc_Video_c::IsRequestedResolutionCompatibleWithProfile(uint32_t Width, uint32_t Height)
{
    VideoEncodeMemoryProfile_t Profile = Encoder.EncodeStream->GetVideoEncodeMemoryProfile();
    uint32_t ProfileArraySize = (sizeof(ProfileSize) / sizeof(stm_se_picture_resolution_s));

    SE_VERBOSE(group_encoder_video_preproc, "Profile is %u\n", Profile);

    //check Profile index does not go out of bound
    if ((Profile < 0) || (Profile >= ProfileArraySize))
    {
        SE_ERROR("Retrieved profile does not match with Profile array, out of bound error with Profile = %u\n", Profile);
        return false;
    }

    //Check if requested encode resolution fits within encode_stream memory profile
    SE_DEBUG(group_encoder_video_preproc, "Profile resolution width is  %u and Profile resolution height is %u\n", ProfileSize[Profile].width, ProfileSize[Profile].height);
    if ((Width <= ProfileSize[Profile].width) && (Height <= ProfileSize[Profile].height))
    {
        SE_VERBOSE(group_encoder_video_preproc, "Encode resolution is compatible with profile as request width = %u and height = %u\n", Width, Height);
        return true;
    }
    else
    {
        SE_WARNING("Encode resolution is NOT compatible with profile as request width = %u and height = %u\n", Width, Height);
        return false;
    }
}

void Preproc_Video_c::InitFRC(void)
{
    TimePerInputField             = PREPROC_TIME_PER_FRAME;
    TimePerOutputFrame            = PREPROC_TIME_PER_FRAME;
    InputTimeLine                 = 0;
    OutputTimeLine                = 0;
    InputFramerate.framerate_num  = PREPROC_ENCODE_FRAMERATE_NUM;
    InputFramerate.framerate_den  = PREPROC_ENCODE_FRAMERATE_DEN;
    OutputFramerate.framerate_num = PREPROC_ENCODE_FRAMERATE_NUM;
    OutputFramerate.framerate_den = PREPROC_ENCODE_FRAMERATE_DEN;
    InputScanType                 = STM_SE_SCAN_TYPE_PROGRESSIVE;
}

PreprocStatus_t Preproc_Video_c::UpdateInputFrameRate(void)
{
    uint64_t TimePerUnit;
    uint64_t TimePerUnitDen;

    // Update input time per frame/field
    if ((InputMetaDataDescriptor->video.frame_rate.framerate_num   != InputFramerate.framerate_num) ||
        (InputMetaDataDescriptor->video.frame_rate.framerate_den   != InputFramerate.framerate_den) ||
        (InputMetaDataDescriptor->video.video_parameters.scan_type != InputScanType))
    {
        TimePerUnit       = (uint64_t)InputMetaDataDescriptor->video.frame_rate.framerate_den * (uint64_t)PREPROC_TIME_BASELINE;
        TimePerUnitDen    = (uint64_t)InputMetaDataDescriptor->video.frame_rate.framerate_num;

        if (TimePerUnitDen == 0)
        {
            SE_DEBUG(group_encoder_video_preproc, "Unsupported Input FrameRate %d/%d , revert to Default FrameRate %d/%d\n",
                     InputFramerate.framerate_num, InputFramerate.framerate_den,
                     PREPROC_ENCODE_FRAMERATE_NUM, PREPROC_ENCODE_FRAMERATE_DEN);
            TimePerUnit       = PREPROC_ENCODE_FRAMERATE_DEN * PREPROC_TIME_BASELINE;
            TimePerUnitDen    = PREPROC_ENCODE_FRAMERATE_NUM;
        }

        // For progressive sequence, field rate = frame rate
        // For interlace sequence, field rate = 2 * frame rate
        // If interlace sequence is not deinterlaced, we treat the sequence as progressive
        if ((PreprocCtrl.EnableDeInterlacer != STM_SE_CTRL_VALUE_DISAPPLY) && (InputMetaDataDescriptor->video.video_parameters.scan_type == STM_SE_SCAN_TYPE_INTERLACED))
        {
            TimePerUnitDen <<= 1;
        }

        TimePerUnit       = (TimePerUnit + (TimePerUnitDen >> 1)) / TimePerUnitDen;
        TimePerInputField = (uint32_t) TimePerUnit;
        // Store the last frame rate
        InputFramerate    = InputMetaDataDescriptor->video.frame_rate;
        // Store the last scan type
        InputScanType     = InputMetaDataDescriptor->video.video_parameters.scan_type;
        // Reset time lines
        InputTimeLine = 0;
        OutputTimeLine = 0;
    }

    return PreprocNoError;
}

PreprocStatus_t Preproc_Video_c::UpdateOutputFrameRate(void)
{
    uint64_t TimePerUnit;
    uint64_t TimePerUnitDen;

    // Update output time per frame
    // Should we update this during control update?
    if ((PreprocCtrl.Framerate.framerate_num != OutputFramerate.framerate_num) ||
        (PreprocCtrl.Framerate.framerate_den != OutputFramerate.framerate_den))
    {
        TimePerUnit        = (uint64_t)PreprocCtrl.Framerate.framerate_den * (uint64_t)PREPROC_TIME_BASELINE;
        TimePerUnitDen     = (uint64_t)PreprocCtrl.Framerate.framerate_num;

        if (TimePerUnitDen == 0)
        {
            SE_DEBUG(group_encoder_video_preproc, "Unsupported Output FrameRate %d/%d , revert to Default FrameRate %d/%d\n",
                     InputFramerate.framerate_num, InputFramerate.framerate_den,
                     PREPROC_ENCODE_FRAMERATE_NUM, PREPROC_ENCODE_FRAMERATE_DEN);
            TimePerUnit       = PREPROC_ENCODE_FRAMERATE_DEN * PREPROC_TIME_BASELINE;
            TimePerUnitDen    = PREPROC_ENCODE_FRAMERATE_NUM;
        }

        TimePerUnit        = (TimePerUnit + (TimePerUnitDen >> 1)) / TimePerUnitDen;
        TimePerOutputFrame = (uint32_t) TimePerUnit;
        // Store the last frame rate
        OutputFramerate    = PreprocCtrl.Framerate;
        // Reset time lines
        InputTimeLine = 0;
        OutputTimeLine = 0;
    }

    return PreprocNoError;
}

PreprocStatus_t Preproc_Video_c::CheckFrameRate(stm_se_framerate_t Framerate)
{
    if ((Framerate.framerate_num == 0) || (Framerate.framerate_den == 0))
    {
        SE_ERROR("Invalid framerate metadata %u/%u\n", Framerate.framerate_num, Framerate.framerate_den);
        return PreprocError;
    }

    return PreprocNoError;
}

PreprocStatus_t Preproc_Video_c::CheckFrameRateSupport()
{
    uint64_t FramerateCtrl;
    uint64_t FramerateInput;
    UpdateInputFrameRate();
    UpdateOutputFrameRate();
    // Check for unsupported framerates
    // Should we do this at higher level
    FramerateCtrl  = (uint64_t)PreprocCtrl.Framerate.framerate_num * InputMetaDataDescriptor->video.frame_rate.framerate_den;
    FramerateInput = (uint64_t)InputMetaDataDescriptor->video.frame_rate.framerate_num * PreprocCtrl.Framerate.framerate_den;

    // Allowed upsampling is set to 4, a high upper limit that is uncommon
    // Higher upsampling should justify with user scenario.
    if (FramerateCtrl > (4 * FramerateInput))
    {
        FramerateInput  = (uint64_t)InputMetaDataDescriptor->video.frame_rate.framerate_num * STM_SE_PLAY_FRAME_RATE_MULTIPLIER;
        FramerateInput /= InputMetaDataDescriptor->video.frame_rate.framerate_den;
        FramerateCtrl   = (uint64_t)PreprocCtrl.Framerate.framerate_num * STM_SE_PLAY_FRAME_RATE_MULTIPLIER;
        FramerateCtrl  /= PreprocCtrl.Framerate.framerate_den;
        SE_ERROR("Unable to support frame rate (x%d) conversion from %d to %d fps\n",
                 STM_SE_PLAY_FRAME_RATE_MULTIPLIER, (uint32_t)FramerateInput, (uint32_t)FramerateCtrl);
        return PreprocError;
    }

    return PreprocNoError;
}

PreprocStatus_t Preproc_Video_c::ConvertFrameRate(FRCCtrl_t *FRCCtrl)
{
    uint64_t TimeLineUppLimit;
    int64_t  TimeOutInOffset;
    uint64_t TimePerFieldOffset;
    int64_t  NativeTimeOutput;
    int64_t  EncodedTimeOutput;

    // Initialize FRC control parameters
    FRCCtrl->DiscardInput       = true;
    FRCCtrl->DiscardOutput      = true;
    FRCCtrl->FrameRepeat        = -1;
    FRCCtrl->NativeTimeOutput   = InputMetaDataDescriptor->native_time;
    FRCCtrl->EncodedTimeOutput  = EncodeCoordinatorMetaDataDescriptor->encoded_time;
    FRCCtrl->TimePerOutputFrame = 0;

    // ===============================================================
    // Frame Rate Conversion
    // ===============================================================
    // Input is computed based on field time where
    // we have two fields for interlace frame and one field for progressive frame
    // Output is computed based on frame time
    // Using a common timeline, we track the input frame/field time and output frame time
    // For each input field/frame, we allow the generation of as many outputs as required
    // provided it does not exceed half the input time period limit (upper limit)
    // Similarly, the input field/frame is discarded if the output frame exceed the upper limit.
    // A 27MHz time unit is used for FRC as it represents the smallest time denominator.
    // This implementation uses the fix time period according to the frame rate and then
    // outputs the PTS relative to the input PTS.
    // Alternative implementation can use the actual PTS on the timeline.
    // It is noticed that the PTS can vary quite a lot and carry risk of instability.
    // ===============================================================
    // Compute FRC parameters
    InputTimeLine      += (uint64_t)TimePerInputField;
    TimeLineUppLimit  = InputTimeLine + (uint64_t)((TimePerInputField + 1) / 2);

    while ((OutputTimeLine + (uint64_t)TimePerOutputFrame) < TimeLineUppLimit)
    {
        FRCCtrl->DiscardInput  = false;
        FRCCtrl->DiscardOutput = false;
        FRCCtrl->FrameRepeat ++;
        OutputTimeLine += (uint64_t)TimePerOutputFrame;
    }

    // Correct FRC for TNR input
    if (PreprocCtrl.EnableNoiseFilter != STM_SE_CTRL_VALUE_DISAPPLY)
    {
        // cannot skip input
        if (FRCCtrl->DiscardInput)
        {
            FRCCtrl->DiscardInput  = false;
            FRCCtrl->DiscardOutput = true;
            FRCCtrl->FrameRepeat ++;
        }
    }

    SE_DEBUG(group_encoder_video_preproc, "DiscardInput %d DiscardOutput %d FrameRepeat %d TimePerFieldInput %d TimePerFrameOutput %d InputTimeLine %llu OutputTimeLine %llu\n",
             FRCCtrl->DiscardInput, FRCCtrl->DiscardOutput, FRCCtrl->FrameRepeat,
             TimePerInputField, TimePerOutputFrame, InputTimeLine, OutputTimeLine);
    // ===============================================================
    // Extrapolate PTS
    // ===============================================================
    TimePerFieldOffset = 0; // Zero offset for first field
    TimeOutInOffset    = (int64_t)OutputTimeLine - (int64_t)InputTimeLine;

    //This offset is for the first output, not the last repeated output
    if (FRCCtrl->FrameRepeat > 0)
    {
        TimeOutInOffset -= FRCCtrl->FrameRepeat * (int64_t)TimePerOutputFrame;
    }

    // Compute PTS according to the pts format from a 27MHz time unit
    if (InputMetaDataDescriptor->native_time_format == TIME_FORMAT_US)
    {
        if (FRCCtrl->SecondField)
        {
            TimePerFieldOffset      = ((TimePerInputField + (uint64_t)(TIME_27MHZ_TO_US_RATIO >> 1)) / (uint64_t)TIME_27MHZ_TO_US_RATIO);
        }

        if (TimeOutInOffset > 0)
        {
            TimeOutInOffset         = ((TimeOutInOffset + (int64_t)(TIME_27MHZ_TO_US_RATIO >> 1)) / (int64_t)TIME_27MHZ_TO_US_RATIO);
        }
        else
        {
            TimeOutInOffset         = ((TimeOutInOffset - (int64_t)(TIME_27MHZ_TO_US_RATIO >> 1)) / (int64_t)TIME_27MHZ_TO_US_RATIO);
        }

        NativeTimeOutput   = InputMetaDataDescriptor->native_time + TimePerFieldOffset + TimeOutInOffset;
        EncodedTimeOutput  = EncodeCoordinatorMetaDataDescriptor->encoded_time + TimePerFieldOffset + TimeOutInOffset;
        FRCCtrl->TimePerOutputFrame = (TimePerOutputFrame + (TIME_27MHZ_TO_US_RATIO >> 1)) / TIME_27MHZ_TO_US_RATIO;
    }
    else if (InputMetaDataDescriptor->native_time_format == TIME_FORMAT_PTS)
    {
        if (FRCCtrl->SecondField)
        {
            TimePerFieldOffset      = ((TimePerInputField + (uint64_t)(TIME_27MHZ_TO_PTS_RATIO >> 1)) / (uint64_t)TIME_27MHZ_TO_PTS_RATIO);
        }

        if (TimeOutInOffset > 0)
        {
            TimeOutInOffset         = ((TimeOutInOffset + (int64_t)(TIME_27MHZ_TO_PTS_RATIO >> 1)) / (int64_t)TIME_27MHZ_TO_PTS_RATIO);
        }
        else
        {
            TimeOutInOffset         = ((TimeOutInOffset - (int64_t)(TIME_27MHZ_TO_PTS_RATIO >> 1)) / (int64_t)TIME_27MHZ_TO_PTS_RATIO);
        }

        NativeTimeOutput   = InputMetaDataDescriptor->native_time + TimePerFieldOffset + TimeOutInOffset;
        EncodedTimeOutput  = EncodeCoordinatorMetaDataDescriptor->encoded_time + TimePerFieldOffset + TimeOutInOffset;
        FRCCtrl->TimePerOutputFrame = (TimePerOutputFrame + (TIME_27MHZ_TO_PTS_RATIO >> 1)) / TIME_27MHZ_TO_PTS_RATIO;
    }
    else
    {
        if (FRCCtrl->SecondField)
        {
            TimePerFieldOffset      = TimePerInputField;
        }

        NativeTimeOutput   = InputMetaDataDescriptor->native_time + TimePerFieldOffset + TimeOutInOffset;
        EncodedTimeOutput  = EncodeCoordinatorMetaDataDescriptor->encoded_time + TimePerFieldOffset + TimeOutInOffset;
        FRCCtrl->TimePerOutputFrame = TimePerOutputFrame;
    }

    // To restrict negative native time representation
    if (NativeTimeOutput < 0)
    {
        FRCCtrl->NativeTimeOutput = 0;
    }
    else
    {
        FRCCtrl->NativeTimeOutput = (uint64_t)NativeTimeOutput;
    }
    if (EncodedTimeOutput < 0)
    {
        FRCCtrl->EncodedTimeOutput = 0;
    }
    else
    {
        FRCCtrl->EncodedTimeOutput = (uint64_t)EncodedTimeOutput;
    }

    SE_DEBUG(group_encoder_video_preproc, "NativeTimeInput %llu TimePerFieldOffset %d TimeOffset %d NativeTimeOutput %llu TimePerOutputFrame %d\n",
             InputMetaDataDescriptor->native_time, (int)TimePerFieldOffset, (int)TimeOutInOffset, FRCCtrl->NativeTimeOutput, (int)FRCCtrl->TimePerOutputFrame);

    // Check and reset timeline
    if ((InputTimeLine > PREPROC_TIME_LINE_MAX) && (OutputTimeLine > PREPROC_TIME_LINE_MAX))
    {
        InputTimeLine  -= (uint64_t)PREPROC_TIME_LINE_MAX;
        OutputTimeLine -= (uint64_t)PREPROC_TIME_LINE_MAX;
    }

    return PreprocNoError;
}

PreprocStatus_t Preproc_Video_c::RegisterPreprocVideo(void *PreprocHandle)
{
    Preproc_Video_c *PreprocVideoGeneric = (Preproc_Video_c *)PreprocHandle;
    // Register all resources
    // Preproc Base Class
    // Buffer types
    PreprocFrameBufferType     = PreprocVideoGeneric->PreprocFrameBufferType;
    PreprocFrameAllocType      = PreprocVideoGeneric->PreprocFrameAllocType;
    InputBufferType            = PreprocVideoGeneric->InputBufferType;
    InputMetaDataBufferType    = PreprocVideoGeneric->InputMetaDataBufferType;
    OutputMetaDataBufferType   = PreprocVideoGeneric->OutputMetaDataBufferType;
    MetaDataSequenceNumberType = PreprocVideoGeneric->MetaDataSequenceNumberType;
    EncodeCoordinatorMetaDataBufferType = PreprocVideoGeneric->EncodeCoordinatorMetaDataBufferType;
    // Buffer manager
    BufferManager              = PreprocVideoGeneric->BufferManager;
    // Preproc Frame Pool Attributes
    PreprocFrameBufferPool     = PreprocVideoGeneric->PreprocFrameBufferPool;
    PreprocFrameAllocPool      = PreprocVideoGeneric->PreprocFrameAllocPool;
    PreprocFrameMemoryDevice   = PreprocVideoGeneric->PreprocFrameMemoryDevice;
    PreprocFrameMemory[0]      = PreprocVideoGeneric->PreprocFrameMemory[0];
    PreprocFrameMemory[1]      = PreprocVideoGeneric->PreprocFrameMemory[1];
    PreprocFrameMemory[2]      = PreprocVideoGeneric->PreprocFrameMemory[2];
    PreprocMemorySize          = PreprocVideoGeneric->PreprocMemorySize;
    PreprocFrameMaximumSize    = PreprocVideoGeneric->PreprocFrameMaximumSize;
    PreprocMaxNbAllocBuffers   = PreprocVideoGeneric->PreprocMaxNbAllocBuffers;
    PreprocMaxNbBuffers        = PreprocVideoGeneric->PreprocMaxNbBuffers;

    strncpy(PreprocMemoryPartitionName, PreprocVideoGeneric->PreprocMemoryPartitionName,
            sizeof(PreprocMemoryPartitionName));
    PreprocMemoryPartitionName[sizeof(PreprocMemoryPartitionName) - 1] = '\0';

    // Context pool
    PreprocStatus_t Status = RegisterOutputBufferRing(PreprocVideoGeneric->OutputRing);
    if (Status != PreprocNoError)
    {
        return Status;
    }

    // Preproc Video Class
    // Data
    PreprocCtrl                = PreprocVideoGeneric->PreprocCtrl; // PreprocCtrl changes with GetControl and SetControl before input
    return PreprocNoError;
}

void Preproc_Video_c::DeRegisterPreprocVideo(void *PreprocHandle)
{
    // De-Register all resources
    // Preproc Base Class
    // Preproc Frame Pool Attributes
    // Reset buffer resources to null
    PreprocFrameBufferPool     = NULL;
    PreprocFrameAllocPool      = NULL;
    PreprocFrameMemoryDevice   = NULL;
}

PreprocStatus_t Preproc_Video_c::CheckAspectRatio(stm_se_aspect_ratio_t AspectRatio, uint32_t PixelAspectRatioNum, uint32_t PixelAspectRatioDen, uint32_t Width, uint32_t Height)
{
    switch (AspectRatio)
    {
    case STM_SE_ASPECT_RATIO_4_BY_3:
    case STM_SE_ASPECT_RATIO_16_BY_9:
    case STM_SE_ASPECT_RATIO_221_1:
        if ((Width * PixelAspectRatioNum * GetDisplayAspectRatio(AspectRatio).den) != (Height * PixelAspectRatioDen * GetDisplayAspectRatio(AspectRatio).num))
        {
            SE_ERROR_ONCE("Inconsistent aspect ratio %u (%s), width = %u, height = %u\n", AspectRatio, StringifyAspectRatio(AspectRatio), Width, Height);
            // TODO: aspect ratio is not consistent on v4l encode layer
            // As a consequence, this line is commented until it is fixed
            //return EncoderNotSupported;
        }
        break;
    case STM_SE_ASPECT_RATIO_UNSPECIFIED:
        break;
    default:
        SE_ERROR("Invalid aspect ratio %u\n", AspectRatio);
        return EncoderNotSupported;
    }

    return PreprocNoError;
}

PreprocStatus_t Preproc_Video_c::CheckAspectRatioSupport(void)
{
    if ((InputMetaDataDescriptor->video.video_parameters.aspect_ratio == STM_SE_ASPECT_RATIO_221_1)
        && (PreprocCtrl.DisplayAspectRatio != STM_SE_CTRL_VALUE_DISPLAY_ASPECT_RATIO_IGNORE))
    {
        SE_ERROR("Unable to support aspect ratio conversion %d using input aspect ratio %d\n",
                 PreprocCtrl.DisplayAspectRatio,
                 InputMetaDataDescriptor->video.video_parameters.aspect_ratio);
        return EncoderNotSupported;
    }

    if (PreprocCtrl.EnableCropping == STM_SE_CTRL_VALUE_APPLY)
    {
        if (((InputMetaDataDescriptor->video.window_of_interest.x + InputMetaDataDescriptor->video.window_of_interest.width) > InputMetaDataDescriptor->video.video_parameters.width) ||
            ((InputMetaDataDescriptor->video.window_of_interest.y + InputMetaDataDescriptor->video.window_of_interest.height) > InputMetaDataDescriptor->video.video_parameters.height))
        {
            SE_ERROR("Window of interest [%d %d]x[%d %d] does not fit in [0 %d]x[0 %d]\n",
                     InputMetaDataDescriptor->video.window_of_interest.x,
                     InputMetaDataDescriptor->video.window_of_interest.x + InputMetaDataDescriptor->video.window_of_interest.width,
                     InputMetaDataDescriptor->video.window_of_interest.y,
                     InputMetaDataDescriptor->video.window_of_interest.y + InputMetaDataDescriptor->video.window_of_interest.height,
                     InputMetaDataDescriptor->video.video_parameters.width,
                     InputMetaDataDescriptor->video.video_parameters.height);
            return EncoderNotSupported;
        }
    }
    else
    {
        // Add warning trace if window of interest is filled and is not the same as frame resolution
        if ((InputMetaDataDescriptor->video.window_of_interest.x ||
             InputMetaDataDescriptor->video.window_of_interest.y ||
             InputMetaDataDescriptor->video.window_of_interest.width ||
             InputMetaDataDescriptor->video.window_of_interest.height) &&
            !((InputMetaDataDescriptor->video.window_of_interest.x == 0) &&
              (InputMetaDataDescriptor->video.window_of_interest.y == 0) &&
              (InputMetaDataDescriptor->video.window_of_interest.width == InputMetaDataDescriptor->video.video_parameters.width) &&
              (InputMetaDataDescriptor->video.window_of_interest.height == InputMetaDataDescriptor->video.video_parameters.height)))
        {
            SE_WARNING_ONCE("Window of interest [%d %d]x[%d %d] not taken into account as suitable control is not set\n",
                            InputMetaDataDescriptor->video.window_of_interest.x,
                            InputMetaDataDescriptor->video.window_of_interest.x + InputMetaDataDescriptor->video.window_of_interest.width,
                            InputMetaDataDescriptor->video.window_of_interest.y,
                            InputMetaDataDescriptor->video.window_of_interest.y + InputMetaDataDescriptor->video.window_of_interest.height);
        }
    }

    SE_DEBUG(group_encoder_video_preproc, "Picture %ux%u AR %u PAR %u/%u DAR %u WOI [%u %u]x[%u %u] EnableCropping %u\n",
             InputMetaDataDescriptor->video.video_parameters.width,
             InputMetaDataDescriptor->video.video_parameters.height,
             InputMetaDataDescriptor->video.video_parameters.aspect_ratio,
             InputMetaDataDescriptor->video.video_parameters.pixel_aspect_ratio_numerator,
             InputMetaDataDescriptor->video.video_parameters.pixel_aspect_ratio_denominator,
             PreprocCtrl.DisplayAspectRatio,
             InputMetaDataDescriptor->video.window_of_interest.x,
             InputMetaDataDescriptor->video.window_of_interest.x + InputMetaDataDescriptor->video.window_of_interest.width,
             InputMetaDataDescriptor->video.window_of_interest.y,
             InputMetaDataDescriptor->video.window_of_interest.y + InputMetaDataDescriptor->video.window_of_interest.height,
             PreprocCtrl.EnableCropping);

    return PreprocNoError;
}

Ratio_t Preproc_Video_c::GetDisplayAspectRatio(stm_se_aspect_ratio_t AspectRatio)
{
    Ratio_t Ratio;

    switch (AspectRatio)
    {
    case STM_SE_ASPECT_RATIO_4_BY_3:
        Ratio.num = 4;
        Ratio.den = 3;
        break;

    case STM_SE_ASPECT_RATIO_16_BY_9:
        Ratio.num = 16;
        Ratio.den = 9;
        break;

    case STM_SE_ASPECT_RATIO_221_1:
        Ratio.num = 221;
        Ratio.den = 100;
        break;

    default:
        Ratio.num = 1;
        Ratio.den = 1;
    }

    return Ratio;
}

stm_se_aspect_ratio_t Preproc_Video_c::MapAspectRatio(uint32_t CtrlAspectRatio)
{
    stm_se_aspect_ratio_t AspectRatio;

    switch (CtrlAspectRatio)
    {
    case STM_SE_CTRL_VALUE_DISPLAY_ASPECT_RATIO_4_BY_3:
        AspectRatio = STM_SE_ASPECT_RATIO_4_BY_3;
        break;

    case STM_SE_CTRL_VALUE_DISPLAY_ASPECT_RATIO_16_BY_9:
        AspectRatio = STM_SE_ASPECT_RATIO_16_BY_9;
        break;

    case STM_SE_CTRL_VALUE_DISPLAY_ASPECT_RATIO_IGNORE:
    default:
        AspectRatio = STM_SE_ASPECT_RATIO_UNSPECIFIED;
    }

    return AspectRatio;
}

void Preproc_Video_c::SimplifyRatio(Ratio_t *Ratio)
{
    // Simplify ratio with first 6 prime numbers
    uint32_t Primes[] = { 2, 3, 5, 7, 11, 0 };
    uint32_t i;

    if (Ratio->num == 0 || Ratio->den == 0)
    {
        SE_ERROR("Undefined ratio %d/%d\n forcing 1:1", Ratio->num, Ratio->den);
        Ratio->num = 1;
        Ratio->den = 1;
        return;
    }

    for (i = 0; Primes[i]; i++)
    {
        while ((Ratio->num % Primes[i] == 0) && (Ratio->den % Primes[i] == 0))
        {
            Ratio->num /= Primes[i];
            Ratio->den /= Primes[i];
        }
    }
}

bool Preproc_Video_c::TestMultOverflow(uint32_t int1, uint32_t int2, uint32_t int3, uint32_t *mul)
{
    uint64_t Res64 = 0;

    // Perform multiplication as uint64_t
    Res64 = (uint64_t) int1 * (uint64_t) int2 * (uint64_t) int3;

    //Provide a uint32_t result, checking possible overflow
    if (Res64 > 0xFFFFFFFF)
    {
        *mul = 0xFFFFFFFF;
        SE_WARNING("32 bits overflow detected in multiplication computation, got %llu\n", Res64);
        return true;
    }
    else // result fits in 32 bits, no overflow
    {
        *mul = (uint32_t) Res64;
        return false;
    }
}

PreprocStatus_t Preproc_Video_c::ConvertAspectRatio(ARCCtrl_t *ARCCtrl)
{
    Ratio_t SrcDAR;
    Ratio_t DstDAR;
    Ratio_t S2DDAR;

    // Initialize ARC variables
    ARCCtrl->SrcSize.width           = InputMetaDataDescriptor->video.video_parameters.width;
    ARCCtrl->SrcSize.height          = InputMetaDataDescriptor->video.video_parameters.height;
    if (PreprocCtrl.EnableCropping == STM_SE_CTRL_VALUE_APPLY)
    {
        ARCCtrl->SrcRect             = InputMetaDataDescriptor->video.window_of_interest;
    }
    else
    {
        ARCCtrl->SrcRect.x           = 0;
        ARCCtrl->SrcRect.y           = 0;
        ARCCtrl->SrcRect.width       = InputMetaDataDescriptor->video.video_parameters.width;
        ARCCtrl->SrcRect.height      = InputMetaDataDescriptor->video.video_parameters.height;
    }
    ARCCtrl->SrcDisplayAspectRatio   = InputMetaDataDescriptor->video.video_parameters.aspect_ratio;
    ARCCtrl->SrcPixelAspectRatio.num = InputMetaDataDescriptor->video.video_parameters.pixel_aspect_ratio_numerator;
    ARCCtrl->SrcPixelAspectRatio.den = InputMetaDataDescriptor->video.video_parameters.pixel_aspect_ratio_denominator;
    ARCCtrl->DstSize                 = PreprocCtrl.Resolution;
    ARCCtrl->DstDisplayAspectRatio   = MapAspectRatio(PreprocCtrl.DisplayAspectRatio);

    // Set Display Aspect Ratio
    SrcDAR = GetDisplayAspectRatio(ARCCtrl->SrcDisplayAspectRatio);
    DstDAR = GetDisplayAspectRatio(ARCCtrl->DstDisplayAspectRatio);

    if (PreprocCtrl.DisplayAspectRatio == STM_SE_CTRL_VALUE_DISPLAY_ASPECT_RATIO_IGNORE)
    {
        // no black bars added in that case
        ARCCtrl->DstRect.x      = 0;
        ARCCtrl->DstRect.y      = 0;
        ARCCtrl->DstRect.width  = ARCCtrl->DstSize.width;
        ARCCtrl->DstRect.height = ARCCtrl->DstSize.height;

        // specific case of unspecified src display aspect ratio: don't rely on this info in that case
        if (InputMetaDataDescriptor->video.video_parameters.aspect_ratio == STM_SE_ASPECT_RATIO_UNSPECIFIED)
        {
            uint32_t DstPixelAspectRatioNum = 0;
            uint32_t DstPixelAspectRatioDen = 0;

            // Very specific case of unspecified source pixel aspect ratio (SrcPixelAspectRatio = 0)
            // should lead to 'DstPixelAspectRatio.num = 0 and den = 1' as expected by h264 coder
            if ((ARCCtrl->SrcPixelAspectRatio.num == 0) || (ARCCtrl->SrcPixelAspectRatio.den == 0))
            {
                ARCCtrl->DstPixelAspectRatio.num = PIXEL_ASPECT_RATIO_NUM_UNSPECIFIED;
                ARCCtrl->DstPixelAspectRatio.den = PIXEL_ASPECT_RATIO_DEN_UNSPECIFIED;
            }
            else
            {
                bool IsOverflow1, IsOverflow2;

                // Reduce fractional to limit risk of overflow
                SimplifyRatio(&ARCCtrl->SrcPixelAspectRatio);
                // Compute DstSAR without SrcDAR, but with DstSAR = input / output x SrcSAR so that to keep a constant DAR
                // DstPixelAspectRatioNum = ARCCtrl->SrcRect.width * ARCCtrl->DstSize.height * ARCCtrl->SrcPixelAspectRatio.num
                IsOverflow1 = TestMultOverflow(ARCCtrl->SrcRect.width, ARCCtrl->DstSize.height, ARCCtrl->SrcPixelAspectRatio.num, &DstPixelAspectRatioNum);
                // DstPixelAspectRatioDen = ARCCtrl->SrcRect.height * ARCCtrl->DstSize.width * ARCCtrl->SrcPixelAspectRatio.den
                IsOverflow2 = TestMultOverflow(ARCCtrl->SrcRect.height, ARCCtrl->DstSize.width, ARCCtrl->SrcPixelAspectRatio.den, &DstPixelAspectRatioDen);

                // if overflow in DstPAR computation, choose 'square pixel'
                if (IsOverflow1 || IsOverflow2)
                {
                    SE_WARNING("Overflow detected in DstPixelAspectRatio computation, set to square pixel\n");
                    DstPixelAspectRatioNum = 1;
                    DstPixelAspectRatioDen = 1;
                }
                ARCCtrl->DstPixelAspectRatio.num = DstPixelAspectRatioNum;
                ARCCtrl->DstPixelAspectRatio.den = DstPixelAspectRatioDen;
                SimplifyRatio(&ARCCtrl->DstPixelAspectRatio);
            }
        }
        else
        {
            // Keeping aspect ratio as source, using relevant SrcDAR info
            ARCCtrl->DstDisplayAspectRatio   = ARCCtrl->SrcDisplayAspectRatio;
            DstDAR = GetDisplayAspectRatio(ARCCtrl->DstDisplayAspectRatio);
            ARCCtrl->DstPixelAspectRatio.num = DstDAR.num * ARCCtrl->DstSize.height;
            ARCCtrl->DstPixelAspectRatio.den = DstDAR.den * ARCCtrl->DstSize.width;
            SimplifyRatio(&ARCCtrl->DstPixelAspectRatio);
        }
    }
    else // force display aspect ratio case
    {
        // Alternatively, DAR info is computed from SAR
        // If we disregard the window of interest, we use SrcSize directly
        // If we consider window of interest, we use SrcRect

        // Specific case of 'SrcPAR = 0' : to be handled as square pixel
        // Converted in SimplifyRatio() that convert '0' fractional to 1:1
        SimplifyRatio(&ARCCtrl->SrcPixelAspectRatio);

        SrcDAR.num = ARCCtrl->SrcPixelAspectRatio.num * ARCCtrl->SrcRect.width;
        SrcDAR.den = ARCCtrl->SrcPixelAspectRatio.den * ARCCtrl->SrcRect.height;
        SimplifyRatio(&SrcDAR);
        S2DDAR.num = SrcDAR.num * DstDAR.den;
        S2DDAR.den = SrcDAR.den * DstDAR.num;
        SimplifyRatio(&S2DDAR);

        // black bars management to keep frame proportion despite scaling
        // typical example is forcing DAR to 16/9 with a requested scaling from VGA to 720p
        // will perform scaling from VGA to a window inside 720p (DstRect)
        // adding vertical black bars to fill full framer buffer (DstSize)
        if (S2DDAR.num < S2DDAR.den)
        {
            // keeping height
            ARCCtrl->DstRect.height = ARCCtrl->DstSize.height;
            // compute output rectangle width, checking possible 32 bits overflow
            // ie ARCCtrl->DstRect.width   = (S2DDAR.num * ARCCtrl->DstSize.width) / S2DDAR.den;
            if (TestMultOverflow(S2DDAR.num, ARCCtrl->DstSize.width, 1, &ARCCtrl->DstRect.width))
            {
                SE_WARNING("Overflow detected in DstRect width computation, set to DstSize width ie no black bar\n");
                ARCCtrl->DstRect.width = ARCCtrl->DstSize.width;
            }
            else // default computation
            {
                ARCCtrl->DstRect.width = ARCCtrl->DstRect.width / S2DDAR.den;
            }
            ARCCtrl->DstRect.width   = ALIGN_UP(ARCCtrl->DstRect.width, 2);
            ARCCtrl->DstRect.y       = 0;
            ARCCtrl->DstRect.x       = (ARCCtrl->DstSize.width - ARCCtrl->DstRect.width) >> 1;
            SE_DEBUG(group_encoder_video_preproc, "Keeping HEIGHT!!! SrcDAR %d/%d DstDAR %d/%d S2DDAR %d/%d\n",
                     SrcDAR.num, SrcDAR.den, DstDAR.num, DstDAR.den, S2DDAR.num, S2DDAR.den);
        }
        // typical example is forcing DAR to 4/3 with a requested scaling from 720p to VGA
        // will perform scaling from 720p to a window inside VGA (DstRect)
        // adding horizontal black bars to fill full framer buffer (DstSize)
        else
        {
            // keeping width
            ARCCtrl->DstRect.width = ARCCtrl->DstSize.width;
            // compute output rectangle height, checking possible 32 bits overflow
            // ie ARCCtrl->DstRect.height  = (S2DDAR.den * ARCCtrl->DstSize.height) / S2DDAR.num;
            if (TestMultOverflow(S2DDAR.den, ARCCtrl->DstSize.height, 1, &ARCCtrl->DstRect.height))
            {
                SE_WARNING("Overflow detected in DstRect height computation, set to DstSize height ie no black bar\n");
                ARCCtrl->DstRect.height = ARCCtrl->DstSize.height;
            }
            else // default computation
            {
                ARCCtrl->DstRect.height = ARCCtrl->DstRect.height / S2DDAR.num;
            }
            ARCCtrl->DstRect.height  = ALIGN_UP(ARCCtrl->DstRect.height, 2);
            ARCCtrl->DstRect.x       = 0;
            ARCCtrl->DstRect.y       = (ARCCtrl->DstSize.height - ARCCtrl->DstRect.height) >> 1;
            SE_DEBUG(group_encoder_video_preproc, "Keeping WIDTH!!! SrcDAR %d/%d DstDAR %d/%d S2DDAR %d/%d\n",
                     SrcDAR.num, SrcDAR.den, DstDAR.num, DstDAR.den, S2DDAR.num, S2DDAR.den);
        }

        // Set Dst Pixel Aspect Ratio
        ARCCtrl->DstPixelAspectRatio.num = DstDAR.num * ARCCtrl->DstSize.height;
        ARCCtrl->DstPixelAspectRatio.den = DstDAR.den * ARCCtrl->DstSize.width;
        SimplifyRatio(&ARCCtrl->DstPixelAspectRatio);
    }

    SE_DEBUG(group_encoder_video_preproc, "SrcPixelAspectRatio %d/%d SrcDisplayAspectRatio %d/%d SrcSize %dx%d SrcRect (%dx%d,%dx%d)\n",
             ARCCtrl->SrcPixelAspectRatio.num,
             ARCCtrl->SrcPixelAspectRatio.den,
             SrcDAR.num,
             SrcDAR.den,
             ARCCtrl->SrcSize.width,
             ARCCtrl->SrcSize.height,
             ARCCtrl->SrcRect.x,
             ARCCtrl->SrcRect.y,
             ARCCtrl->SrcRect.width,
             ARCCtrl->SrcRect.height);
    SE_DEBUG(group_encoder_video_preproc, "DstPixelAspectRatio %d/%d DstDisplayAspectRatio %d/%d DstSize %dx%d DstRect (%dx%d,%dx%d)\n",
             ARCCtrl->DstPixelAspectRatio.num,
             ARCCtrl->DstPixelAspectRatio.den,
             DstDAR.num,
             DstDAR.den,
             ARCCtrl->DstSize.width,
             ARCCtrl->DstSize.height,
             ARCCtrl->DstRect.x,
             ARCCtrl->DstRect.y,
             ARCCtrl->DstRect.width,
             ARCCtrl->DstRect.height);

    return PreprocNoError;
}
