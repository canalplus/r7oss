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

#include "preproc_video_scaler.h"
#include "st_relayfs_se.h"

#undef TRACE_TAG
#define TRACE_TAG "Preproc_Video_Scaler_c"

// modified from linux/err.h
#define SCALER_ERR(x) ((x == NULL) || ((unsigned long)x >= (unsigned long)-4095))

#define SCALER_DISPATCH_TIMEOUT_MS 1000

static inline const char *StringifyScalerColorspace(stm_scaler_color_space_t aScalerColorspace)
{
    switch (aScalerColorspace)
    {
        ENTRY(STM_SCALER_COLORSPACE_RGB);
        ENTRY(STM_SCALER_COLORSPACE_RGB_VIDEORANGE);
        ENTRY(STM_SCALER_COLORSPACE_BT601);
        ENTRY(STM_SCALER_COLORSPACE_BT601_FULLRANGE);
        ENTRY(STM_SCALER_COLORSPACE_BT709);
        ENTRY(STM_SCALER_COLORSPACE_BT709_FULLRANGE);
    default: return "<unknown scaler colorspace>";
    }
}

static inline const char *StringifyScalerSurfaceFormat(stm_scaler_buffer_format_t aScalerSurfaceFormat)
{
    switch (aScalerSurfaceFormat)
    {
        ENTRY(STM_SCALER_BUFFER_RGB888);
        ENTRY(STM_SCALER_BUFFER_YUV420_NV12);
        ENTRY(STM_SCALER_BUFFER_YUV422_NV16);
    default: return "<unknown scaler surface format>";
    }
}

static BufferDataDescriptor_t PreprocScalerContextDescriptor = BUFFER_SCALER_CONTEXT_TYPE;

static const PreprocVideoCaps_t PreprocCaps =
{
    {ENCODE_WIDTH_MAX, ENCODE_HEIGHT_MAX},                     // MaxResolution
    {ENCODE_FRAMERATE_MAX, STM_SE_PLAY_FRAME_RATE_MULTIPLIER}, // MaxFramerate
    true,                                                      // DEISupport
    true,                                                      // TNRSupport
    (1 << SURFACE_FORMAT_VIDEO_420_RASTER2B)                   // VideoFormatCaps
};

static void ScalerTaskDoneCallback(void *ScalerContext, stm_scaler_time_t Time, bool Success);
static void ScalerInputBufferDoneCallback(void *ScalerContext);
static void ScalerOutputBufferDoneCallback(void *ScalerContext);

Preproc_Video_Scaler_c::Preproc_Video_Scaler_c(void)
    : Preproc_Video_c()
    , scaler(NULL)
    , PreprocFrameBuffer2(NULL)
    , PreprocContextBuffer2(NULL)
    , ScalerTaskCompletionSemaphore()
    , ScalerContext(NULL)
    , mRelayfsIndex(0)
{
    mRelayfsIndex = st_relayfs_getindex_fortype_se(ST_RELAY_TYPE_ENCODER_VIDEO_PREPROC_INPUT);

    Configuration.PreprocContextCount      = SCALER_CONTEXT_MAX_BUFFERS;
    Configuration.PreprocContextDescriptor = &PreprocScalerContextDescriptor;

    OS_SemaphoreInitialize(&ScalerTaskCompletionSemaphore, 0);

    // TODO(pht) move FinalizeInit to a factory method
    InitializationStatus = FinalizeInit();
}

Preproc_Video_Scaler_c::~Preproc_Video_Scaler_c(void)
{
    Halt();
    OS_SemaphoreTerminate(&ScalerTaskCompletionSemaphore);

    st_relayfs_freeindex_fortype_se(ST_RELAY_TYPE_ENCODER_VIDEO_PREPROC_INPUT, mRelayfsIndex);
}

PreprocStatus_t Preproc_Video_Scaler_c::FinalizeInit(void)
{
    if (SCALER_ERR(scaler))
    {
        stm_scaler_config_t ScalerConfig;
        ScalerConfig.scaler_input_buffer_done_callback  = &ScalerInputBufferDoneCallback;
        ScalerConfig.scaler_output_buffer_done_callback = &ScalerOutputBufferDoneCallback;
        ScalerConfig.scaler_task_done_callback = &ScalerTaskDoneCallback;

        int32_t ScalerStatus = stm_scaler_open(SCALER_ID, &scaler, &ScalerConfig);
        if (ScalerStatus != 0)
        {
            SE_ERROR("stm_scaler_open failed (%d)\n", ScalerStatus);
            scaler = NULL;
            return PreprocError;
        }
    }

    return PreprocNoError;
}

PreprocStatus_t Preproc_Video_Scaler_c::Halt()
{
    int32_t ScalerStatus;
    bool    PendingTask;

    SE_DEBUG(group_encoder_video_preproc, "\n");

    // Halt the base class early to prevent queuing new frames
    Preproc_Video_c::Halt();

    // Put a scaler handle
    if (scaler != NULL)
    {
        PendingTask = CheckPendingTask();

        if (PendingTask)   // abort pending tasks
        {
            ScalerStatus = stm_scaler_abort(scaler);
            if (ScalerStatus != 0)
            {
                SE_ERROR("stm_scaler_abort failed (%d)\n", ScalerStatus);
                // continue
            }
        }

        ScalerStatus = stm_scaler_close(scaler);
        if (ScalerStatus != 0)
        {
            SE_ERROR("stm_scaler_close failed (%d)\n", ScalerStatus);
            // continue
        }

        WaitForAllScalerContexts(); // wait for all scaler context buffers
        scaler = NULL;
    }

    return PreprocNoError;
}

PreprocStatus_t Preproc_Video_Scaler_c::WaitForAllScalerContexts()
{
    PreprocStatus_t Status = PreprocNoError;
    Buffer_t PreprocContextBuffers[SCALER_CONTEXT_MAX_BUFFERS] = { 0 };
    uint32_t i;
    uint32_t scaler_context_count = SCALER_CONTEXT_MAX_BUFFERS;

    if (PreprocContextBufferPool == NULL)
    {
        return PreprocNoError;
    }

    SE_DEBUG(group_encoder_video_preproc, "\n");

    // Acquire and release all context buffers
    for (i = 0; i < SCALER_CONTEXT_MAX_BUFFERS; i++)
    {
        Status = GetNewScalerContext(&PreprocContextBuffers[i]);
        if (Status != PreprocNoError)
        {
            PREPROC_ERROR_RUNNING("Failed to get new scaler context %d\n", i);
            scaler_context_count = i;
            break;
        }
    }

    for (i = 0; i < scaler_context_count; i++)
    {
        PreprocContextBuffers[i]->DecrementReferenceCount();
    }

    return Status;
}

bool Preproc_Video_Scaler_c::CheckPendingTask()
{
    bool             Pending;
    unsigned int     BuffersWithNonZeroReferenceCount;

    if (PreprocContextBufferPool == NULL)
    {
        return false;
    }

    PreprocContextBufferPool->GetPoolUsage(NULL, &BuffersWithNonZeroReferenceCount, NULL, NULL, NULL, NULL);
    Pending = (BuffersWithNonZeroReferenceCount > 0) ? true : false;
    return Pending;
}

PreprocStatus_t Preproc_Video_Scaler_c::WaitForPendingTasksToComplete()
{
    bool             Pending;

    SE_DEBUG(group_encoder_video_preproc, "\n");

    Pending = CheckPendingTask();
    if (!Pending)
    {
        return PreprocNoError;
    }

    return WaitForAllScalerContexts();
}

PreprocStatus_t Preproc_Video_Scaler_c::GetNewScalerContext(Buffer_t   *Buffer)
{
    // Initialize buffer
    PreprocStatus_t Status = Preproc_Base_c::GetNewContext(Buffer);
    if (Status != PreprocNoError)
    {
        return Status;
    }

    (*Buffer)->SetUsedDataSize(sizeof(Scaler_Context_t));

    // Obtain scaler context reference
    (*Buffer)->ObtainDataReference(NULL, NULL, (void **)(&ScalerContext));
    if (ScalerContext == NULL)
    {
        SE_ERROR("Failed to obtain context buffer reference\n");
        (*Buffer)->DecrementReferenceCount(IdentifierEncoderPreprocessor);
        return PreprocError;
    }

    // Initialize scaler context
    memset((void *)ScalerContext, 0, sizeof(Scaler_Context_t));

    return PreprocNoError;
}

// The CompletionCallback should be entirely encoder dependent and
// Should reference the encode context for that current frame
// to ensure that the Completion of the buffer is marked correctly.

PreprocStatus_t Preproc_Video_Scaler_c::TaskCallback(Scaler_Context_t *Context)
{
    PreprocStatus_t Status;
    SE_DEBUG(group_encoder_video_preproc, "\n");

    // Check if output frame is output
    if (!Context->FRCCtrl.DiscardOutput)
    {
        // Get preproc metadata descriptor
        __stm_se_frame_metadata_t            *ContextPreprocFullMetaDataDescriptor;
        Context->OutputBuffer->ObtainMetaDataReference(OutputMetaDataBufferType, (void **)(&ContextPreprocFullMetaDataDescriptor));
        SE_ASSERT(ContextPreprocFullMetaDataDescriptor != NULL);

        stm_se_uncompressed_frame_metadata_t *ContextPreprocMetaDataDescriptor;
        ContextPreprocMetaDataDescriptor = &ContextPreprocFullMetaDataDescriptor->uncompressed_frame_metadata;

        // Get input buffer metadata descriptor for debug purpose
        stm_se_uncompressed_frame_metadata_t *ContextInputMetaDataDescriptor;
        Context->InputBuffer->ObtainMetaDataReference(InputMetaDataBufferType, (void **)(&ContextInputMetaDataDescriptor));
        SE_ASSERT(ContextInputMetaDataDescriptor != NULL);

        SE_DEBUG(group_encoder_video_preproc, "Output buffer %p\n", Context->OutputBuffer);

        // Repeat output frame
        if (Context->FRCCtrl.FrameRepeat > 0)
        {
            Status = RepeatOutputFrame(Context->InputBuffer, Context->OutputBuffer, &Context->FRCCtrl);

            if (Status != PreprocNoError)
            {
                SE_ERROR("Failed to repeat output frame\n");
                Context->ContextBuffer->DetachBuffer(Context->OutputBuffer);
                Context->ContextBuffer->DetachBuffer(Context->InputBuffer);
                return PreprocError;
            }
        }

        // Update PTS info
        // We assume encoded_time is always expressed in the same unit than native_time, as set by the EncodeCoordinator
        ContextPreprocMetaDataDescriptor->native_time = Context->FRCCtrl.NativeTimeOutput + (Context->FRCCtrl.FrameRepeat * Context->FRCCtrl.TimePerOutputFrame);
        ContextPreprocFullMetaDataDescriptor->encode_coordinator_metadata.encoded_time = Context->FRCCtrl.EncodedTimeOutput + (Context->FRCCtrl.FrameRepeat * Context->FRCCtrl.TimePerOutputFrame);

        SE_DEBUG(group_encoder_video_preproc, "PTS: In %llu Out %llu\n", ContextInputMetaDataDescriptor->native_time, ContextPreprocMetaDataDescriptor->native_time);

        // Output frame
        Status = Output(Context->OutputBuffer);
        if (Status != PreprocNoError)
        {
            SE_ERROR("Failed to output frame\n");
            Context->ContextBuffer->DetachBuffer(Context->OutputBuffer);
            Context->ContextBuffer->DetachBuffer(Context->InputBuffer);
            return PreprocError;
        }
    }
    else
    {
        // Silently discard frame
        Context->OutputBuffer->DecrementReferenceCount(IdentifierEncoderPreprocessor);
    }

    // Decrement context to ack task callback
    Context->ContextBuffer->DecrementReferenceCount(IdentifierEncoderPreprocessor);
    return PreprocNoError;
}

PreprocStatus_t Preproc_Video_Scaler_c::InputBufferCallback(Scaler_Context_t *Context)
{
    SE_DEBUG(group_encoder_video_preproc, "\n");

    // The input buffer is no more used by scaler, inject_frame() call can now be unblocked
    OS_SemaphoreSignal(&ScalerTaskCompletionSemaphore);
    Context->ContextBuffer->DetachBuffer(Context->InputBuffer);

    return PreprocNoError;
}

PreprocStatus_t Preproc_Video_Scaler_c::OutputBufferCallback(Scaler_Context_t *Context)
{
    SE_DEBUG(group_encoder_video_preproc, "\n");

    Context->ContextBuffer->DetachBuffer(Context->OutputBuffer);

    // Last callback to delete scaler context buffer, assuming that output buffer callback is the last callback.
    Context->ContextBuffer->DecrementReferenceCount(IdentifierEncoderPreprocessor);
    return PreprocNoError;
}

PreprocStatus_t Preproc_Video_Scaler_c::ErrorCallback(Scaler_Context_t *Context)
{
    SE_WARNING("Scaler task failed\n");

    // We do not need to decrement the buffer reference count
    // as it is already done on the scaler task timeout

    return PreprocNoError;
}

PreprocStatus_t Preproc_Video_Scaler_c::PrepareScalerTask(Buffer_t InputBuffer, Buffer_t OutputBuffer, ARCCtrl_t *ARCCtrl)
{
    void *InputBufferAddrPhys;
    void *PreprocBufferAddrPhys;
    uint32_t DataSize;
    uint32_t ShrinkSize;
    uint32_t AlignedHeight;
    uint32_t vAlign;
    Buffer_t ContentBuffer;

    // Check Scaler Context
    if (ScalerContext == NULL)
    {
        SE_ERROR("Null scaler context\n");
        return PreprocError;
    }

    // Get input buffer reference
    InputBuffer->ObtainDataReference(NULL, &DataSize, (void **)(&InputBufferAddrPhys), PhysicalAddress);
    if (InputBufferAddrPhys == NULL)
    {
        SE_ERROR("Failed to get input buffer reference\n");
        return PreprocError;
    }

    // Get output buffer reference
    OutputBuffer->ObtainDataReference(NULL, NULL, (void **)(&PreprocBufferAddrPhys), PhysicalAddress);
    if (PreprocBufferAddrPhys == NULL)
    {
        SE_ERROR("Failed to get output buffer reference\n");
        return PreprocError;
    }

    SE_VERBOSE(group_encoder_video_preproc, "PrepareScalerTask - InputBufferAddrPhys 0x%p PreprocBufferAddrPhys 0x%p\n", InputBufferAddrPhys, PreprocBufferAddrPhys);
    // INPUT
    // TODO: add STM_SCALER_FLAG_TNR flag if TNR is requested
    ScalerContext->Desc.scaler_flags       = 0;
    // TODO: native_time format must be consistent with due_time format
    ScalerContext->Desc.due_time           = (stm_scaler_time_t)PreprocMetaDataDescriptor->native_time;
    ScalerContext->Desc.input.user_data    = (void *)ScalerContext;
    ScalerContext->Desc.input.width        = PreprocMetaDataDescriptor->video.video_parameters.width;
    ScalerContext->Desc.input.height       = PreprocMetaDataDescriptor->video.video_parameters.height;
    ScalerContext->Desc.input.stride       = PreprocMetaDataDescriptor->video.pitch;
    ScalerContext->Desc.input.buffer_flags = 0;

    // Only when control enables DEI, we program buffer as interlace
    if ((PreprocCtrl.EnableDeInterlacer != STM_SE_CTRL_VALUE_DISAPPLY) && (PreprocMetaDataDescriptor->video.video_parameters.scan_type == STM_SE_SCAN_TYPE_INTERLACED))
    {
        ScalerContext->Desc.input.buffer_flags = (uint32_t)(STM_SCALER_BUFFER_INTERLACED);
    }

    // Retrieve correct colorspace from input meta data
    switch (PreprocMetaDataDescriptor->video.video_parameters.colorspace)
    {
    case STM_SE_COLORSPACE_SMPTE170M:       ScalerContext->Desc.input.color_space = STM_SCALER_COLORSPACE_BT601; break;
    case STM_SE_COLORSPACE_SMPTE240M:       ScalerContext->Desc.input.color_space = STM_SCALER_COLORSPACE_BT709; break;
    case STM_SE_COLORSPACE_BT709:           ScalerContext->Desc.input.color_space = STM_SCALER_COLORSPACE_BT709; break;
    case STM_SE_COLORSPACE_BT470_SYSTEM_M:  ScalerContext->Desc.input.color_space = STM_SCALER_COLORSPACE_BT601; break;
    case STM_SE_COLORSPACE_BT470_SYSTEM_BG: ScalerContext->Desc.input.color_space = STM_SCALER_COLORSPACE_BT601; break;
    case STM_SE_COLORSPACE_SRGB:            ScalerContext->Desc.input.color_space = STM_SCALER_COLORSPACE_RGB;   break;
    default:                                ScalerContext->Desc.input.color_space = (PreprocMetaDataDescriptor->video.video_parameters.width > SD_WIDTH ?
                                                                                         STM_SCALER_COLORSPACE_BT709 : STM_SCALER_COLORSPACE_BT601);
    }

    // To retrieve buffer format
    switch (PreprocMetaDataDescriptor->video.surface_format)
    {
    case SURFACE_FORMAT_VIDEO_888_RGB:
        ScalerContext->Desc.input.format        = STM_SCALER_BUFFER_RGB888;
        ScalerContext->Desc.input.rgb_address   = (uint32_t)InputBufferAddrPhys;
        vAlign = 32;

        if (PreprocMetaDataDescriptor->video.vertical_alignment >= 1)
        {
            vAlign = PreprocMetaDataDescriptor->video.vertical_alignment;
        }

        AlignedHeight                           = ALIGN_UP(PreprocMetaDataDescriptor->video.video_parameters.height, vAlign);
        ScalerContext->Desc.input.size          = PreprocMetaDataDescriptor->video.pitch * AlignedHeight;
        break;

    case SURFACE_FORMAT_VIDEO_420_RASTER2B:
        ScalerContext->Desc.input.format        = STM_SCALER_BUFFER_YUV420_NV12;
        ScalerContext->Desc.input.luma_address  = (uint32_t)InputBufferAddrPhys;
        vAlign = 32;

        if (PreprocMetaDataDescriptor->video.vertical_alignment >= 1)
        {
            vAlign = PreprocMetaDataDescriptor->video.vertical_alignment;
        }

        AlignedHeight                           = ALIGN_UP(PreprocMetaDataDescriptor->video.video_parameters.height, vAlign);
        ScalerContext->Desc.input.chroma_offset = PreprocMetaDataDescriptor->video.pitch * AlignedHeight;
        ScalerContext->Desc.input.size          = PreprocMetaDataDescriptor->video.pitch * AlignedHeight * 3 / 2;
        break;

    case SURFACE_FORMAT_VIDEO_8888_ARGB:
    case SURFACE_FORMAT_VIDEO_565_RGB:
    case SURFACE_FORMAT_VIDEO_420_MACROBLOCK:
    case SURFACE_FORMAT_VIDEO_420_PAIRED_MACROBLOCK:
    case SURFACE_FORMAT_VIDEO_422_RASTER:
    case SURFACE_FORMAT_VIDEO_420_PLANAR:
    case SURFACE_FORMAT_VIDEO_420_PLANAR_ALIGNED:
    case SURFACE_FORMAT_VIDEO_422_PLANAR:
    case SURFACE_FORMAT_VIDEO_422_YUYV:
    default:
        SE_ERROR("Unsupported video surface format 0x%08x\n", PreprocMetaDataDescriptor->video.surface_format);
        return PreprocError;
    }

    if (ScalerContext->Desc.input.size > DataSize)
    {
        SE_ERROR("DataSize %d smaller than input size %d\n", DataSize, ScalerContext->Desc.input.size);
        return PreprocError;
    }

    if (PreprocCtrl.EnableCropping == STM_SE_CTRL_VALUE_APPLY)
    {
        ScalerContext->Desc.input_crop_area.x      = PreprocMetaDataDescriptor->video.window_of_interest.x;
        ScalerContext->Desc.input_crop_area.y      = PreprocMetaDataDescriptor->video.window_of_interest.y;
        ScalerContext->Desc.input_crop_area.width  = PreprocMetaDataDescriptor->video.window_of_interest.width;
        ScalerContext->Desc.input_crop_area.height = PreprocMetaDataDescriptor->video.window_of_interest.height;
    }
    else
    {
        ScalerContext->Desc.input_crop_area.x      = 0;
        ScalerContext->Desc.input_crop_area.y      = 0;
        ScalerContext->Desc.input_crop_area.width  = PreprocMetaDataDescriptor->video.video_parameters.width;
        ScalerContext->Desc.input_crop_area.height = PreprocMetaDataDescriptor->video.video_parameters.height;
    }

    // OUTPUT
    // Only STM_SCALER_BUFFER_YUV420_NV12 supported?
    ScalerContext->Desc.output.user_data       = (void *)ScalerContext;
    ScalerContext->Desc.output.width           = ALIGN_UP(PreprocCtrl.Resolution.width, 16);
    ScalerContext->Desc.output.height          = ALIGN_UP(PreprocCtrl.Resolution.height, 16);
    ScalerContext->Desc.output.stride          = ALIGN_UP(PreprocCtrl.Resolution.width, 16);
    ScalerContext->Desc.output.size            = ScalerContext->Desc.output.stride * ScalerContext->Desc.output.height * 3 / 2;
    ScalerContext->Desc.output.buffer_flags    = 0; // Progressive
    ScalerContext->Desc.output.color_space     = ScalerContext->Desc.input.color_space;
    ScalerContext->Desc.output.format          = STM_SCALER_BUFFER_YUV420_NV12;
    ScalerContext->Desc.output.luma_address    = (uint32_t)PreprocBufferAddrPhys;
    ScalerContext->Desc.output.chroma_offset   = ScalerContext->Desc.output.stride * ScalerContext->Desc.output.height;
    ScalerContext->Desc.output_image_area.x      = ARCCtrl->DstRect.x;
    ScalerContext->Desc.output_image_area.y      = ARCCtrl->DstRect.y;
    ScalerContext->Desc.output_image_area.width  = ARCCtrl->DstRect.width;
    ScalerContext->Desc.output_image_area.height = ARCCtrl->DstRect.height;

    // Set buffer size
    OutputBuffer->SetUsedDataSize(ScalerContext->Desc.output.size);
    OutputBuffer->ObtainAttachedBufferReference(PreprocFrameAllocType, &ContentBuffer);
    SE_ASSERT(ContentBuffer != NULL);

    ContentBuffer->SetUsedDataSize(ScalerContext->Desc.output.size);
    ShrinkSize = max(ScalerContext->Desc.output.size, 1);
    BufferStatus_t Status = ContentBuffer->ShrinkBuffer(ShrinkSize);
    if (Status != BufferNoError)
    {
        SE_ERROR("Failed to shrink the operating buffer to size %d\n", ShrinkSize);
        return PreprocError;
    }

    return PreprocNoError;
}

PreprocStatus_t Preproc_Video_Scaler_c::DispatchScaler(Buffer_t InputBuffer, Buffer_t OutputBuffer,
                                                       Buffer_t ContextBuffer, uint32_t BufferFlag)
{
    PreprocStatus_t Status;
    BufferStatus_t  BufStatus;
    int32_t         ScalerStatus;
    ARCCtrl_t       ARCCtrl;

    // Check Scaler Context
    if (ScalerContext == NULL)
    {
        SE_ERROR("Null scaler context\n");
        return PreprocError;
    }

    // Prepare scaler context
    ScalerContext->PreprocVideoScaler    = this;
    ScalerContext->InputBuffer           = InputBuffer;
    ScalerContext->OutputBuffer          = OutputBuffer;
    ScalerContext->ContextBuffer         = ContextBuffer;
    SE_VERBOSE(group_encoder_video_preproc, "ScalerContext->PreprocVideoScaler 0x%p InputBuffer 0x%p OutputBuffer 0x%p\n",
               ScalerContext->PreprocVideoScaler, ScalerContext->InputBuffer, ScalerContext->OutputBuffer);

    // Convert aspect ratio
    ConvertAspectRatio(&ARCCtrl);

    // Prepare scaler task
    Status = PrepareScalerTask(InputBuffer, OutputBuffer, &ARCCtrl);
    if (Status != PreprocNoError)
    {
        SE_ERROR("Failed to prepare scaler task: %d\n", Status);
        return Status;
    }

    // Set field flag
    ScalerContext->Desc.input.buffer_flags |= BufferFlag;
    // Update meta data
    PreprocMetaDataDescriptor->video.video_parameters.width                          = PreprocCtrl.Resolution.width;
    PreprocMetaDataDescriptor->video.video_parameters.height                         = PreprocCtrl.Resolution.height;

    if (ScalerContext->Desc.output.format == STM_SCALER_BUFFER_YUV420_NV12)
    {
        PreprocMetaDataDescriptor->video.surface_format                              = SURFACE_FORMAT_VIDEO_420_RASTER2B;
        PreprocMetaDataDescriptor->video.pitch                                       = PreprocCtrl.Resolution.width;
    }

    PreprocMetaDataDescriptor->video.frame_rate                                      = PreprocCtrl.Framerate;

    // As coder only needs fractional framerate, integer framerate is set to 0
    PreprocMetaDataDescriptor->video.video_parameters.frame_rate                     = 0;

    // As coder only needs pixel aspect ratio, display aspect ratio is set to unspecified
    PreprocMetaDataDescriptor->video.video_parameters.aspect_ratio                   = STM_SE_ASPECT_RATIO_UNSPECIFIED;
    PreprocMetaDataDescriptor->video.video_parameters.pixel_aspect_ratio_numerator   = ARCCtrl.DstPixelAspectRatio.num;
    PreprocMetaDataDescriptor->video.video_parameters.pixel_aspect_ratio_denominator = ARCCtrl.DstPixelAspectRatio.den;

    PreprocMetaDataDescriptor->video.video_parameters.scan_type                      = STM_SE_SCAN_TYPE_PROGRESSIVE;
    PreprocMetaDataDescriptor->video.vertical_alignment                              = 1;

    // Keep additional reference on buffer for callbacks
    ContextBuffer->IncrementReferenceCount(IdentifierEncoderPreprocessor);

    BufStatus = ContextBuffer->AttachBuffer(InputBuffer);
    if (BufStatus != BufferNoError)
    {
        SE_ERROR("Failed to attach input buffer\n");
        ContextBuffer->DecrementReferenceCount(IdentifierEncoderPreprocessor);
        return PreprocError;
    }

    BufStatus = ContextBuffer->AttachBuffer(OutputBuffer);
    if (BufStatus != BufferNoError)
    {
        SE_ERROR("Failed to attach output buffer\n");
        ContextBuffer->DetachBuffer(InputBuffer);
        ContextBuffer->DecrementReferenceCount(IdentifierEncoderPreprocessor);
        return PreprocError;
    }

    if (SE_IS_VERBOSE_ON(group_encoder_video_preproc))
    {
        SE_VERBOSE(group_encoder_video_preproc, "[SCALER TASK DESCRIPTOR]\n");
        SE_VERBOSE(group_encoder_video_preproc, "|-scaler_flags = 0x%x (%s)\n", ScalerContext->Desc.scaler_flags, ScalerContext->Desc.scaler_flags & STM_SCALER_FLAG_TNR ? "STM_SCALER_FLAG_TNR" : "none");
        SE_VERBOSE(group_encoder_video_preproc, "|-due_time = %lld\n", ScalerContext->Desc.due_time);
        SE_VERBOSE(group_encoder_video_preproc, "|\n");
        SE_VERBOSE(group_encoder_video_preproc, "|-[input]\n");
        SE_VERBOSE(group_encoder_video_preproc, "| |-user_data = %p\n", ScalerContext->Desc.input.user_data);
        SE_VERBOSE(group_encoder_video_preproc, "| |-width = %u pixels\n", ScalerContext->Desc.input.width);
        SE_VERBOSE(group_encoder_video_preproc, "| |-height = %u pixels\n", ScalerContext->Desc.input.height);
        SE_VERBOSE(group_encoder_video_preproc, "| |-stride = %u bytes\n", ScalerContext->Desc.input.stride);
        SE_VERBOSE(group_encoder_video_preproc, "| |-size = %u bytes\n", ScalerContext->Desc.input.size);
        SE_VERBOSE(group_encoder_video_preproc, "| |-buffer_flags = 0x%x (%s)\n", ScalerContext->Desc.input.buffer_flags,
                   ScalerContext->Desc.input.buffer_flags & STM_SCALER_BUFFER_INTERLACED ? (ScalerContext->Desc.input.buffer_flags & STM_SCALER_BUFFER_TOP ? "STM_SCALER_BUFFER_TOP" : "STM_SCALER_BUFFER_BOTTOM") :
                   "STM_SCALER_BUFFER_PROGRESSIVE");
        SE_VERBOSE(group_encoder_video_preproc, "| |-color_space = %d (%s)\n", ScalerContext->Desc.input.color_space, StringifyScalerColorspace(ScalerContext->Desc.input.color_space));
        SE_VERBOSE(group_encoder_video_preproc, "| |-format = %d (%s)\n", ScalerContext->Desc.input.format, StringifyScalerSurfaceFormat(ScalerContext->Desc.input.format));
        SE_VERBOSE(group_encoder_video_preproc, "| |-luma_address = 0x%x\n", ScalerContext->Desc.input.luma_address);
        SE_VERBOSE(group_encoder_video_preproc, "| |-chroma_offset = %u bytes\n", ScalerContext->Desc.input.chroma_offset);
        SE_VERBOSE(group_encoder_video_preproc, "|\n");
        SE_VERBOSE(group_encoder_video_preproc, "|-[input_crop_area]\n");
        SE_VERBOSE(group_encoder_video_preproc, "| |-x = %d pixels\n", ScalerContext->Desc.input_crop_area.x);
        SE_VERBOSE(group_encoder_video_preproc, "| |-y = %d pixels\n", ScalerContext->Desc.input_crop_area.y);
        SE_VERBOSE(group_encoder_video_preproc, "| |-width = %u pixels\n", ScalerContext->Desc.input_crop_area.width);
        SE_VERBOSE(group_encoder_video_preproc, "| |-height = %u pixels\n", ScalerContext->Desc.input_crop_area.height);
        SE_VERBOSE(group_encoder_video_preproc, "|\n");
        SE_VERBOSE(group_encoder_video_preproc, "|-[output]\n");
        SE_VERBOSE(group_encoder_video_preproc, "| |-user_data = %p\n", ScalerContext->Desc.output.user_data);
        SE_VERBOSE(group_encoder_video_preproc, "| |-width = %u pixels\n", ScalerContext->Desc.output.width);
        SE_VERBOSE(group_encoder_video_preproc, "| |-height = %u pixels\n", ScalerContext->Desc.output.height);
        SE_VERBOSE(group_encoder_video_preproc, "| |-stride = %u bytes\n", ScalerContext->Desc.output.stride);
        SE_VERBOSE(group_encoder_video_preproc, "| |-size = %u bytes\n", ScalerContext->Desc.output.size);
        SE_VERBOSE(group_encoder_video_preproc, "| |-buffer_flags = 0x%x (%s)\n", ScalerContext->Desc.output.buffer_flags,
                   ScalerContext->Desc.output.buffer_flags & STM_SCALER_BUFFER_INTERLACED ? (ScalerContext->Desc.output.buffer_flags & STM_SCALER_BUFFER_TOP ? "STM_SCALER_BUFFER_TOP" : "STM_SCALER_BUFFER_BOTTOM") :
                   "STM_SCALER_BUFFER_PROGRESSIVE");
        SE_VERBOSE(group_encoder_video_preproc, "| |-color_space = %d (%s)\n", ScalerContext->Desc.output.color_space, StringifyScalerColorspace(ScalerContext->Desc.output.color_space));
        SE_VERBOSE(group_encoder_video_preproc, "| |-format = %d (%s)\n", ScalerContext->Desc.output.format, StringifyScalerSurfaceFormat(ScalerContext->Desc.output.format));
        SE_VERBOSE(group_encoder_video_preproc, "| |-luma_address = 0x%x\n", ScalerContext->Desc.output.luma_address);
        SE_VERBOSE(group_encoder_video_preproc, "| |-chroma_offset = %u bytes\n", ScalerContext->Desc.output.chroma_offset);
        SE_VERBOSE(group_encoder_video_preproc, "|\n");
        SE_VERBOSE(group_encoder_video_preproc, "|-[output_image_area]\n");
        SE_VERBOSE(group_encoder_video_preproc, "  |-x = %d pixels\n", ScalerContext->Desc.output_image_area.x);
        SE_VERBOSE(group_encoder_video_preproc, "  |-y = %d pixels\n", ScalerContext->Desc.output_image_area.y);
        SE_VERBOSE(group_encoder_video_preproc, "  |-width = %u pixels\n", ScalerContext->Desc.output_image_area.width);
        SE_VERBOSE(group_encoder_video_preproc, "  |-height = %u pixels\n", ScalerContext->Desc.output_image_area.height);
        SE_VERBOSE(group_encoder_video_preproc, "\n");
    }

    // Perform scaling as per requested
    ScalerStatus = stm_scaler_dispatch(scaler, &(ScalerContext->Desc));
    if (ScalerStatus != 0)
    {
        SE_ERROR("Failed to call scaler dispatch (%d)\n", ScalerStatus);
        ContextBuffer->DetachBuffer(OutputBuffer);
        ContextBuffer->DetachBuffer(InputBuffer);
        ContextBuffer->DecrementReferenceCount(IdentifierEncoderPreprocessor);
        return PreprocError;
    }

    // We must remain blocking in inject_frame() method as long as the input buffer is used by scaler
    if (OS_SemaphoreWaitTimeout(&ScalerTaskCompletionSemaphore, SCALER_DISPATCH_TIMEOUT_MS) == OS_TIMED_OUT)
    {
        SE_ERROR("Scaler callback is not responding after %d ms\n", SCALER_DISPATCH_TIMEOUT_MS);
        ContextBuffer->DetachBuffer(OutputBuffer);
        ContextBuffer->DetachBuffer(InputBuffer);
        ContextBuffer->DecrementReferenceCount(IdentifierEncoderPreprocessor);
        return PreprocError;
    }

    return PreprocNoError;
}

PreprocStatus_t Preproc_Video_Scaler_c::RepeatOutputFrame(Buffer_t InputBuffer, Buffer_t OutputBuffer, FRCCtrl_t *FRCCtrl)
{
    PreprocStatus_t Status;
    BufferStatus_t  BufStatus;
    Buffer_t PreprocFrameBufferClone;
    int32_t cnt;

    // Get input buffer metadata descriptor
    stm_se_uncompressed_frame_metadata_t *ContextInputMetaDataDescriptor;
    InputBuffer->ObtainMetaDataReference(InputMetaDataBufferType, (void **)(&ContextInputMetaDataDescriptor));
    SE_ASSERT(ContextInputMetaDataDescriptor != NULL);

    // Get ref preproc buffer metadata descriptor
    __stm_se_frame_metadata_t            *RefContextPreprocFullMetaDataDescriptor;
    OutputBuffer->ObtainMetaDataReference(OutputMetaDataBufferType, (void **)(&RefContextPreprocFullMetaDataDescriptor));
    SE_ASSERT(RefContextPreprocFullMetaDataDescriptor != NULL);

    for (cnt = 0; cnt < FRCCtrl->FrameRepeat; cnt ++)
    {
        // Get preproc buffer clone
        Status = Preproc_Base_c::GetBufferClone(OutputBuffer, &PreprocFrameBufferClone);
        if (Status != PreprocNoError)
        {
            PREPROC_ERROR_RUNNING("Failed to clone output buffer\n");
            return Status;
        }

        SE_DEBUG(group_encoder_video_preproc, "Copy buffer %p to clone buffer %p\n", OutputBuffer, PreprocFrameBufferClone);

        // Attach the Input buffer to the BufferClone so that it is kept until Preprocessing is complete.
        BufStatus = PreprocFrameBufferClone->AttachBuffer(InputBuffer);
        if (BufStatus != BufferNoError)
        {
            SE_ERROR("Failed to attach input buffer\n");
            PreprocFrameBufferClone->DecrementReferenceCount(IdentifierEncoderPreprocessor);
            return PreprocError;
        }

        // Obtain preproc metadata reference
        __stm_se_frame_metadata_t            *ContextPreprocFullMetaDataDescriptor;
        PreprocFrameBufferClone->ObtainMetaDataReference(OutputMetaDataBufferType, (void **)(&ContextPreprocFullMetaDataDescriptor));
        SE_ASSERT(ContextPreprocFullMetaDataDescriptor != NULL);

        stm_se_uncompressed_frame_metadata_t *ContextPreprocMetaDataDescriptor;
        ContextPreprocMetaDataDescriptor = &ContextPreprocFullMetaDataDescriptor->uncompressed_frame_metadata;

        // Copy metadata
        ContextPreprocMetaDataDescriptor = &ContextPreprocFullMetaDataDescriptor->uncompressed_frame_metadata;
        memcpy(ContextPreprocFullMetaDataDescriptor, RefContextPreprocFullMetaDataDescriptor, sizeof(__stm_se_frame_metadata_t));

        // Update PTS info
        // We assume encoded_time is always expressed in the same unit than native_time, as set by the EncodeCoordinator
        ContextPreprocMetaDataDescriptor->native_time = FRCCtrl->NativeTimeOutput + (cnt * FRCCtrl->TimePerOutputFrame);
        ContextPreprocFullMetaDataDescriptor->encode_coordinator_metadata.encoded_time = FRCCtrl->EncodedTimeOutput + (cnt * FRCCtrl->TimePerOutputFrame);

        SE_DEBUG(group_encoder_video_preproc, "PTS: In %llu Out %llu\n",
                 ContextInputMetaDataDescriptor->native_time, ContextPreprocMetaDataDescriptor->native_time);

        // Output frame
        Status = Output(PreprocFrameBufferClone);
        if (Status != PreprocNoError)
        {
            SE_ERROR("Failed to output frame\n");
            return PreprocError;
        }
    }

    return PreprocNoError;
}

PreprocStatus_t Preproc_Video_Scaler_c::ProcessField(Buffer_t InputBuffer, Buffer_t OutputBuffer,
                                                     Buffer_t ContextBuffer, bool SecondField)
{
    PreprocStatus_t Status;
    BufferStatus_t  BufStatus;
    uint32_t BufferFlag;

    // Get new scaler context
    Status = GetNewScalerContext(&ContextBuffer);
    if (Status != PreprocNoError)
    {
        PREPROC_ERROR_RUNNING("Failed to get new scaler context\n");
        return Status;
    }

    // Convert frame rate
    ScalerContext->FRCCtrl.SecondField = SecondField;
    ConvertFrameRate(&ScalerContext->FRCCtrl);

    // Check if input frame is processed
    if (!ScalerContext->FRCCtrl.DiscardInput)
    {
        // Initialize buffer and metadata if second field because it is not done by default in base classes
        if (SecondField)
        {
            // Get new preproc buffer
            Status = Preproc_Base_c::GetNewBuffer(&OutputBuffer);
            if (Status != PreprocNoError)
            {
                PREPROC_ERROR_RUNNING("Failed to get new buffer\n");
                return Status;
            }

            // Obtain preproc metadata reference
            OutputBuffer->ObtainMetaDataReference(OutputMetaDataBufferType, (void **)(&PreprocFullMetaDataDescriptor));
            SE_ASSERT(PreprocFullMetaDataDescriptor != NULL);

            // Copy metadata
            PreprocMetaDataDescriptor = &PreprocFullMetaDataDescriptor->uncompressed_frame_metadata;
            memcpy(PreprocMetaDataDescriptor, InputMetaDataDescriptor, sizeof(stm_se_uncompressed_frame_metadata_t));
            memcpy(&PreprocFullMetaDataDescriptor->encode_coordinator_metadata, EncodeCoordinatorMetaDataDescriptor, sizeof(__stm_se_encode_coordinator_metadata_t));

            // Attach the Input buffer to the output buffer so that it is kept until Preprocessing is complete.
            BufStatus = OutputBuffer->AttachBuffer(InputBuffer);
            if (BufStatus != BufferNoError)
            {
                SE_ERROR("Failed to attach input buffer\n");
                OutputBuffer->DecrementReferenceCount(IdentifierEncoderPreprocessor);
                return PreprocError;
            }
        }

        // Update preproc control status
        PreprocFullMetaDataDescriptor->encode_process_metadata.video.deinterlacing_on   = (PreprocCtrl.EnableDeInterlacer != STM_SE_CTRL_VALUE_DISAPPLY) ? true : false;
        PreprocFullMetaDataDescriptor->encode_process_metadata.video.noise_filtering_on = (PreprocCtrl.EnableNoiseFilter != STM_SE_CTRL_VALUE_DISAPPLY) ? true : false;

        // Program buffer flag
        BufferFlag = 0;

        if ((PreprocCtrl.EnableDeInterlacer != STM_SE_CTRL_VALUE_DISAPPLY) && (InputMetaDataDescriptor->video.video_parameters.scan_type == STM_SE_SCAN_TYPE_INTERLACED))
        {
            // Top field when top field first && first field or bot field first && second field
            if (!(PreprocMetaDataDescriptor->video.top_field_first == SecondField))
            {
                BufferFlag = (uint32_t)(STM_SCALER_BUFFER_TOP);
            }
            else
            {
                BufferFlag = (uint32_t)(STM_SCALER_BUFFER_BOTTOM);
            }
        }

        // Dispatch scaler and update preproc output metadata
        Status = DispatchScaler(InputBuffer, OutputBuffer, ContextBuffer, BufferFlag);
        if (Status != PreprocNoError)
        {
            SE_ERROR("Dispatch scaler failed\n");
            OutputBuffer->DecrementReferenceCount(IdentifierEncoderPreprocessor);
            return Status;
        }
    }
    else
    {
        ContextBuffer->DecrementReferenceCount(IdentifierEncoderPreprocessor);

        // Silently discard frame
        if (!SecondField)
        {
            OutputBuffer->DecrementReferenceCount(IdentifierEncoderPreprocessor);
        }
    }

    return PreprocNoError;
}

PreprocStatus_t Preproc_Video_Scaler_c::Input(Buffer_t    Buffer)
{
    PreprocStatus_t        Status;
    SE_DEBUG(group_encoder_video_preproc, "\n");

    // Dump preproc input buffer via st_relay
    Buffer->DumpViaRelay(ST_RELAY_TYPE_ENCODER_VIDEO_PREPROC_INPUT + mRelayfsIndex, ST_RELAY_SOURCE_SE);

    // Call the Base Class implementation to update statistics
    Status = Preproc_Video_c::Input(Buffer);
    if (Status != PreprocNoError)
    {
        PREPROC_ERROR_RUNNING("Failed to call base class, Status = %u\n", Status);
        return PREPROC_STATUS_RUNNING(Status);
    }

    EntryTrace(InputMetaDataDescriptor, &PreprocCtrl);

    // Check for valid scaler device
    if (scaler == NULL)
    {
        PREPROC_ERROR_RUNNING("scaler device is not valid\n");
        PreprocFrameBuffer->DecrementReferenceCount(IdentifierEncoderPreprocessor);
        return PREPROC_STATUS_RUNNING(PreprocError);
    }

    // Generate buffer discontinuity if needed
    if (PreprocDiscontinuity &&
        !((PreprocDiscontinuity & STM_SE_DISCONTINUITY_EOS) && (InputBufferSize != 0)))
    {
        // To wait until pending tasks complete
        Status = WaitForPendingTasksToComplete();
        if (Status != PreprocNoError)
        {
            PREPROC_ERROR_RUNNING("WaitForPendingTasksToComplete failed, aborting EOS, Status = %u\n", Status);
            PreprocFrameBuffer->DecrementReferenceCount(IdentifierEncoderPreprocessor);
            return PREPROC_STATUS_RUNNING(Status);
        }

        // Insert buffer discontinuity
        Status = GenerateBufferDiscontinuity(PreprocDiscontinuity);
        if (PreprocNoError != Status)
        {
            PREPROC_ERROR_RUNNING("Failed to insert discontinuity, Status = %u\n", Status);
            PreprocFrameBuffer->DecrementReferenceCount(IdentifierEncoderPreprocessor);
            return PREPROC_STATUS_RUNNING(Status);
        }

        // Silently discard frame for null buffer input
        if (InputBufferSize == 0)
        {
            if (PreprocDiscontinuity & STM_SE_DISCONTINUITY_EOS)
            {
                ExitTrace(InputMetaDataDescriptor, &PreprocCtrl);
            }
            PreprocFrameBuffer->DecrementReferenceCount(IdentifierEncoderPreprocessor);
            return PreprocNoError;
        }
    }

    // First field/frame is processed
    SE_DEBUG(group_encoder_video_preproc, "Processing first field/frame\n");
    Status = ProcessField(Buffer, PreprocFrameBuffer, PreprocContextBuffer, false);
    if (Status != PreprocNoError)
    {
        PREPROC_ERROR_RUNNING("Failed to process first field, Status = %u\n", Status);
        return PREPROC_STATUS_RUNNING(Status);
    }

    // If second field is processed
    if ((PreprocCtrl.EnableDeInterlacer != STM_SE_CTRL_VALUE_DISAPPLY) &&
        (InputMetaDataDescriptor->video.video_parameters.scan_type == STM_SE_SCAN_TYPE_INTERLACED))
    {
        SE_DEBUG(group_encoder_video_preproc, "Processing second field\n");
        Status = ProcessField(Buffer, PreprocFrameBuffer2, PreprocContextBuffer2, true);
        if (Status != PreprocNoError)
        {
            PREPROC_ERROR_RUNNING("Failed to process second field, Status = %u\n", Status);
            return PREPROC_STATUS_RUNNING(Status);
        }
    }

    // Special case to ensure that valid buffer is processed before EOS buffer is generated
    if ((PreprocDiscontinuity & STM_SE_DISCONTINUITY_EOS) && (InputBufferSize != 0))
    {
        // To wait until pending tasks complete
        Status = WaitForPendingTasksToComplete();
        if (Status != PreprocNoError)
        {
            PREPROC_ERROR_RUNNING("WaitForPendingTasksToComplete failed, aborting EOS, Status = %u\n", Status);
            return PREPROC_STATUS_RUNNING(Status);
        }

        // Insert Buffer EOS
        Status = GenerateBufferDiscontinuity(PreprocDiscontinuity);
        if (Status != PreprocNoError)
        {
            PREPROC_ERROR_RUNNING("Failed to insert EOS discontinuity, Status = %u\n", Status);
            return PREPROC_STATUS_RUNNING(Status);
        }
        ExitTrace(InputMetaDataDescriptor, &PreprocCtrl);
    }

    return PreprocNoError;
}

PreprocStatus_t Preproc_Video_Scaler_c::GetControl(stm_se_ctrl_t  Control,
                                                   void          *Data)
{
    return Preproc_Video_c::GetControl(Control, Data);
}

PreprocStatus_t Preproc_Video_Scaler_c::GetCompoundControl(stm_se_ctrl_t  Control,
                                                           void          *Data)
{
    return Preproc_Video_c::GetCompoundControl(Control, Data);
}

PreprocStatus_t Preproc_Video_Scaler_c::SetControl(stm_se_ctrl_t  Control,
                                                   const void    *Data)
{
    return Preproc_Video_c::SetControl(Control, Data);
}

PreprocStatus_t Preproc_Video_Scaler_c::SetCompoundControl(stm_se_ctrl_t  Control,
                                                           const void    *Data)
{
    return Preproc_Video_c::SetCompoundControl(Control, Data);
}

PreprocStatus_t Preproc_Video_Scaler_c::InjectDiscontinuity(stm_se_discontinuity_t  Discontinuity)
{
    PreprocStatus_t Status;
    SE_DEBUG(group_encoder_video_preproc, "\n");

    Status = Preproc_Base_c::InjectDiscontinuity(Discontinuity);
    if (Status != PreprocNoError)
    {
        PREPROC_ERROR_RUNNING("Failed to inject discontinuity\n");
        return PREPROC_STATUS_RUNNING(Status);
    }

    if (Discontinuity)
    {
        // To wait until pending tasks complete
        Status = WaitForPendingTasksToComplete();
        if (Status != PreprocNoError)
        {
            PREPROC_ERROR_RUNNING("WaitForPendingTasksToComplete failed, aborting EOS!\n");
            return PREPROC_STATUS_RUNNING(Status);
        }

        // Generate Discontinuity Buffer
        Status = GenerateBufferDiscontinuity(Discontinuity);
        if (Status != PreprocNoError)
        {
            PREPROC_ERROR_RUNNING("Failed to insert discontinuity\n");
            return PREPROC_STATUS_RUNNING(Status);
        }
    }

    ExitTrace(InputMetaDataDescriptor, &PreprocCtrl);

    return PreprocNoError;
}

PreprocVideoCaps_t Preproc_Video_Scaler_c::GetCapabilities()
{
    return PreprocCaps;
}

static void ScalerTaskDoneCallback(void *ScalerContext, stm_scaler_time_t Time, bool Success)
{
    Scaler_Context_t *Context    = (Scaler_Context_t *)ScalerContext;
    Preproc_Video_Scaler_c *Self = (Preproc_Video_Scaler_c *)Context->PreprocVideoScaler;
    SE_VERBOSE(group_encoder_video_preproc, "\n");

    if (Success)
    {
        Self->TaskCallback(Context);
    }
    else
    {
        Self->ErrorCallback(Context);
    }
}

static void ScalerInputBufferDoneCallback(void *ScalerContext)
{
    Scaler_Context_t *Context    = (Scaler_Context_t *)ScalerContext;
    Preproc_Video_Scaler_c *Self = (Preproc_Video_Scaler_c *)Context->PreprocVideoScaler;
    SE_VERBOSE(group_encoder_video_preproc, "\n");
    Self->InputBufferCallback(Context);
}

static void ScalerOutputBufferDoneCallback(void *ScalerContext)
{
    Scaler_Context_t *Context    = (Scaler_Context_t *)ScalerContext;
    Preproc_Video_Scaler_c *Self = (Preproc_Video_Scaler_c *)Context->PreprocVideoScaler;
    SE_VERBOSE(group_encoder_video_preproc, "\n");
    Self->OutputBufferCallback(Context);
}

