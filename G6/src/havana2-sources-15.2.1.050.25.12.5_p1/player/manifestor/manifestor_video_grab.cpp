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

#include "manifestor_video_grab.h"

#undef TRACE_TAG
#define TRACE_TAG   "Manifestor_VideoGrab_c"

struct QueueRecord_s
{
    unsigned int        Flags;
    unsigned int        Count;
    int                 PSIndex;
};

/*{{{  End of stream Event hendler*/
void VideoGrab_EndOfStreamEvent(unsigned int number_of_events, stm_event_info_t *eventsInfo)
{
    SE_DEBUG(group_manifestor_video_grab, "\n");
    Manifestor_VideoGrab_c *thisManifestor = (Manifestor_VideoGrab_c *)eventsInfo->cookie;
    // Use of an extra capture buffer to notify End Of Stream
    struct capture_buffer_s    *CaptureBuffer   = &thisManifestor->CaptureBuffersForEOS;
    CaptureBuffer->virtual_address  = 0;                   // No real buffer attached
    CaptureBuffer->physical_address = 0;                   // No real buffer attached
    CaptureBuffer->size             = 0;                   // 0 means EOS End Of Stream
    CaptureBuffer->user_private     = INVALID_INDEX;       // It's a fake buffer
    CaptureBuffer->presentation_time = INVALID_TIME;
    thisManifestor->CaptureDevice->QueueBuffer(CaptureBuffer);
}


//{{{  Constructor
// /////////////////////////////////////////////////////////////////////////
//
//      Constructor :
//      Action  : Initialise state
//      Input   :
//      Output  :
//      Result  :
//

Manifestor_VideoGrab_c::Manifestor_VideoGrab_c(void)
    : Manifestor_Video_c()
    , CaptureDevice(NULL)
    , EventSubscription(NULL)
    , MustSubscribeToEOS(true)
    , CaptureBuffers()
    , CaptureBuffersForEOS()
{
    if (InitializationStatus != ManifestorNoError)
    {
        SE_ERROR("Initialization status not valid - aborting init\n");
        return;
    }

    SE_DEBUG(group_manifestor_video_grab, "\n");

    SetGroupTrace(group_manifestor_video_grab);

    Configuration.Capabilities                  = MANIFESTOR_CAPABILITY_GRAB;

    SurfaceDescriptor[0].StreamType             = StreamTypeVideo;
    SurfaceDescriptor[0].ClockPullingAvailable  = false;
    SurfaceDescriptor[0].DisplayWidth           = FRAME_GRAB_DEFAULT_DISPLAY_WIDTH;
    SurfaceDescriptor[0].DisplayHeight          = FRAME_GRAB_DEFAULT_DISPLAY_HEIGHT;
    SurfaceDescriptor[0].Progressive            = true;
    SurfaceDescriptor[0].FrameRate              = FRAME_GRAB_DEFAULT_FRAME_RATE; // rational
    SurfaceDescriptor[0].PercussiveCapable      = true;
    // But request that we set output to match input rates
    SurfaceDescriptor[0].InheritRateAndTypeFromSource = true;
}
//}}}
//{{{  Destructor
// /////////////////////////////////////////////////////////////////////////
//
//      Destructor :
//      Action  : Give up switch off the lights and go home
//      Input   :
//      Output  :
//      Result  :
//

Manifestor_VideoGrab_c::~Manifestor_VideoGrab_c(void)
{
    SE_DEBUG(group_manifestor_video_grab, "\n");

    /* Remove event subscription */
    if (EventSubscription)
    {
        int result = stm_event_subscription_delete(EventSubscription);
        if (result < 0)
        {
            SE_ERROR("Failed to delete Event Subscription for EOS event (%d)\n", result);
        }
    }

    CloseOutputSurface();
    Halt();
}
//}}}
//{{{  Halt
//{{{  doxynote
/// \brief              Shutdown, stop presenting and retrieving frames
///                     don't return until complete
//}}}
ManifestorStatus_t      Manifestor_VideoGrab_c::Halt(void)
{
    SE_DEBUG(group_manifestor_video_grab, "\n");
    PtsOnDisplay    = INVALID_TIME;
    return Manifestor_Video_c::Halt();
}
//}}}
//{{{  Reset
/// \brief              Reset all state to intial values
ManifestorStatus_t Manifestor_VideoGrab_c::Reset(void)
{
    SE_DEBUG(group_manifestor_video_grab, "\n");

    if (TestComponentState(ComponentRunning))
    {
        Halt();
    }

    for (int i = 0; i < MAX_DECODE_BUFFERS; i++)
    {
        memset(&CaptureBuffers[i], 0, sizeof(struct capture_buffer_s));
    }

    return Manifestor_Video_c::Reset();
}
//}}}
//{{{  UpdateOutputSurfaceDescriptor
//{{{  doxynote
/// \brief      Find out information about display plane and output
/// \return     ManifestorNoError if done successfully
//}}}
ManifestorStatus_t Manifestor_VideoGrab_c::UpdateOutputSurfaceDescriptor(void)
{
    SE_DEBUG(group_manifestor_video_grab, "\n");
    SurfaceDescriptor[0].StreamType                = StreamTypeVideo;
    SurfaceDescriptor[0].ClockPullingAvailable     = false;

    if (StreamDisplayParameters.FrameRate != 0)
    {
        SurfaceDescriptor[0].DisplayWidth          = StreamDisplayParameters.DisplayWidth;
        SurfaceDescriptor[0].DisplayHeight         = StreamDisplayParameters.DisplayHeight;
        SurfaceDescriptor[0].Progressive           = StreamDisplayParameters.Progressive;
        SurfaceDescriptor[0].FrameRate             = StreamDisplayParameters.FrameRate;
    }
    else
    {
        SurfaceDescriptor[0].DisplayWidth          = FRAME_GRAB_DEFAULT_DISPLAY_WIDTH;
        SurfaceDescriptor[0].DisplayHeight         = FRAME_GRAB_DEFAULT_DISPLAY_HEIGHT;
        SurfaceDescriptor[0].Progressive           = true;
        SurfaceDescriptor[0].FrameRate             = FRAME_GRAB_DEFAULT_FRAME_RATE; // rational
    }

    // Request that we set output to match input rates
    SurfaceDescriptor[0].InheritRateAndTypeFromSource = true;
    return ManifestorNoError;
}
//}}}
//{{{  OpenOutputSurface
//{{{  doxynote
/// \brief      Find out information about display plane and output and use it to
///             open handles to the desired display plane
/// \param      Device Handle of the display device.
/// \param      PlaneId Plane identifier (interpreted in the same way as stgfb)
/// \return     ManifestorNoError if plane opened successfully
//}}}
ManifestorStatus_t Manifestor_VideoGrab_c::OpenOutputSurface(void *Device)
{
    SE_DEBUG(group_manifestor_video_grab, "\n");
    Visible             = false;
    return ManifestorNoError;
}
//}}}
//{{{  CloseOutputSurface
//{{{  doxynote
/// \brief              Release all frame buffer resources
//}}}
ManifestorStatus_t Manifestor_VideoGrab_c::CloseOutputSurface(void)
{
    SE_DEBUG(group_manifestor_video_grab, "\n");
    return ManifestorNoError;
}
//}}}
//{{{  PrepareToQueue
//{{{  doxynote
/// \brief              Queue the first field of the first picture to the display
/// \param              Buffer containing frame to be displayed
/// \result             Success if frame displayed, fail if not displayed or a frame has already been displayed
//}}}
ManifestorStatus_t      Manifestor_VideoGrab_c::PrepareToQueue(class Buffer_c              *Buffer,
                                                               struct capture_buffer_s     *CaptureBuffer,
                                                               struct ManifestationOutputTiming_s  **VideoOutputTimingArray)
{
    SE_DEBUG(group_manifestor_video_grab, "\n");

    struct ParsedVideoParameters_s     *VideoParameters;
    Buffer->ObtainMetaDataReference(Player->MetaDataParsedVideoParametersType, (void **)&VideoParameters);
    SE_ASSERT(VideoParameters != NULL);

    struct ParsedFrameParameters_s     *ParsedFrameParameters;
    Buffer->ObtainMetaDataReference(Player->MetaDataParsedFrameParametersReferenceType, (void **)(&ParsedFrameParameters));
    SE_ASSERT(ParsedFrameParameters != NULL);

    memset((void *)CaptureBuffer, 0, sizeof(struct capture_buffer_s));

    //
    // Setup input window - portion of incoming content to copy to output window
    //
    struct Window_s                     InputWindow;
    InputWindow.X                       = 0;
    InputWindow.Y                       = 0;
    InputWindow.Width                   = VideoParameters->Content.Width;
    InputWindow.Height                  = VideoParameters->Content.Height;

    //
    // Get Decimated frame policy
    //
    bool DecimateIfAvailable = false;

    if (Player->PolicyValue(Playback, Stream, PolicyDecimateDecoderOutput) != PolicyValueDecimateDecoderOutputDisabled)
    {
        DecimateIfAvailable             = true;
    }

    //{{{  Set up buffer pointers
    DecodeBufferComponentType_t         ManifestationComponent;
    // Fill in src fields depends on decimation
    if ((!DecimateIfAvailable) || (!Stream->GetDecodeBufferManager()->ComponentPresent(Buffer, DecimatedManifestationComponent)))
    {
        ManifestationComponent              = PrimaryManifestationComponent;
        CaptureBuffer->rectangle.x          = InputWindow.X;
        CaptureBuffer->rectangle.y          = InputWindow.Y;
        CaptureBuffer->rectangle.width      = InputWindow.Width;
        CaptureBuffer->rectangle.height     = InputWindow.Height;
    }
    else
    {
        unsigned int    HDecimationFactor   = 0;
        unsigned int    VDecimationFactor   = 0;
        ManifestationComponent              = DecimatedManifestationComponent;
        HDecimationFactor                   = (Stream->GetDecodeBufferManager()->DecimationFactor(Buffer, 0) == 2) ? 1 : 2;
        VDecimationFactor                   = 1;
        CaptureBuffer->rectangle.x          = InputWindow.X      >> HDecimationFactor;
        CaptureBuffer->rectangle.y          = InputWindow.Y      >> VDecimationFactor;
        CaptureBuffer->rectangle.width      = InputWindow.Width  >> HDecimationFactor;
        CaptureBuffer->rectangle.height     = InputWindow.Height >> VDecimationFactor;
    }

    CaptureBuffer->virtual_address          = (void *)Stream->GetDecodeBufferManager()->ComponentBaseAddress(Buffer, ManifestationComponent, CachedAddress);
    CaptureBuffer->physical_address         = (unsigned long)Stream->GetDecodeBufferManager()->ComponentBaseAddress(Buffer, ManifestationComponent, PhysicalAddress);
    CaptureBuffer->size                     = Stream->GetDecodeBufferManager()->ComponentSize(Buffer, ManifestationComponent);
    CaptureBuffer->chroma_buffer_offset     = Stream->GetDecodeBufferManager()->Chroma(Buffer, ManifestationComponent) -
                                              Stream->GetDecodeBufferManager()->Luma(Buffer, ManifestationComponent);
    CaptureBuffer->stride                   = Stream->GetDecodeBufferManager()->ComponentStride(Buffer, ManifestationComponent, 0, 0);
    CaptureBuffer->presentation_time        = VideoOutputTimingArray[0]->SystemPlaybackTime;
    CaptureBuffer->native_presentation_time = ParsedFrameParameters->NormalizedPlaybackTime;
    CaptureBuffer->flags                    = 0;
    CaptureBuffer->total_lines              = Stream->GetDecodeBufferManager()->ComponentDimension(Buffer, ManifestationComponent, 1);

    //SE_INFO(group_manifestor_video_grab, "Rect = %dx%d at %d,%d\n", CaptureBuffer->rectangle.width, CaptureBuffer->rectangle.height, CaptureBuffer->rectangle.x, CaptureBuffer->rectangle.y);
    //}}}
    //{{{  Set up buffer details
    switch (Stream->GetDecodeBufferManager()->ComponentDataType(PrimaryManifestationComponent))
    {
    case FormatVideo420_MacroBlock:
        CaptureBuffer->buffer_format        = SURFACE_FORMAT_VIDEO_420_MACROBLOCK;
        CaptureBuffer->pixel_depth          = 8;
        break;

    case FormatVideo420_PairedMacroBlock:
        CaptureBuffer->buffer_format        = SURFACE_FORMAT_VIDEO_420_PAIRED_MACROBLOCK;
        CaptureBuffer->pixel_depth          = 8;
        break;

    case FormatVideo8888_ARGB:
        CaptureBuffer->buffer_format        = SURFACE_FORMAT_VIDEO_8888_ARGB;
        CaptureBuffer->pixel_depth          = 32;
        break;

    case FormatVideo888_RGB:
        CaptureBuffer->buffer_format        = SURFACE_FORMAT_VIDEO_888_RGB;
        CaptureBuffer->pixel_depth          = 24;
        break;

    case FormatVideo565_RGB:
        CaptureBuffer->buffer_format        = SURFACE_FORMAT_VIDEO_565_RGB;
        CaptureBuffer->pixel_depth          = 16;
        break;

    case FormatVideo422_Raster:
        CaptureBuffer->buffer_format        = SURFACE_FORMAT_VIDEO_422_RASTER;
        CaptureBuffer->pixel_depth          = 16;
        break;

    case FormatVideo420_PlanarAligned:
    case FormatVideo420_Planar:
        CaptureBuffer->buffer_format        = SURFACE_FORMAT_VIDEO_420_PLANAR;
        CaptureBuffer->pixel_depth          = 8;
        CaptureBuffer->rectangle.x         += Stream->GetDecodeBufferManager()->ComponentBorder(PrimaryManifestationComponent, 0) * 16;
        CaptureBuffer->rectangle.y         += Stream->GetDecodeBufferManager()->ComponentBorder(PrimaryManifestationComponent, 1) * 16;
        break;

    case FormatVideo422_Planar:
        CaptureBuffer->buffer_format        = SURFACE_FORMAT_VIDEO_422_PLANAR;
        CaptureBuffer->pixel_depth          = 8;
        CaptureBuffer->rectangle.x         += Stream->GetDecodeBufferManager()->ComponentBorder(PrimaryManifestationComponent, 0) * 16;
        CaptureBuffer->rectangle.y         += Stream->GetDecodeBufferManager()->ComponentBorder(PrimaryManifestationComponent, 1) * 16;
        break;

    case FormatVideo422_YUYV:
        CaptureBuffer->buffer_format        = SURFACE_FORMAT_VIDEO_422_YUYV;
        CaptureBuffer->pixel_depth          = 16;
        break;

    case FormatVideo420_Raster2B:
        CaptureBuffer->buffer_format        = SURFACE_FORMAT_VIDEO_420_RASTER2B;
        CaptureBuffer->pixel_depth          = 8;
        break;

    default:
        SE_ERROR("Unsupported display format (%d)\n", Stream->GetDecodeBufferManager()->ComponentDataType(PrimaryManifestationComponent));
        break;
    }

    //}}}
    //{{{  Add colourspace flags
    switch (VideoParameters->Content.ColourMatrixCoefficients)
    {
    case MatrixCoefficients_ITU_R_BT601:                            // Do nothing, use 601 coefficients
    case MatrixCoefficients_ITU_R_BT470_2_M:
    case MatrixCoefficients_ITU_R_BT470_2_BG:
    case MatrixCoefficients_SMPTE_170M:
    case MatrixCoefficients_FCC:
        break;

    case MatrixCoefficients_ITU_R_BT709:                            // Use 709 coefficients
    case MatrixCoefficients_SMPTE_240M:
        CaptureBuffer->flags               |= CAPTURE_COLOURSPACE_709;
        break;

    case MatrixCoefficients_Undefined:
    default:
        if (VideoParameters->Content.Width > 720)                   // Base coefficients on display size SD=601, HD = 709
        {
            CaptureBuffer->flags           |= CAPTURE_COLOURSPACE_709;
        }

        break;
    }

    return ManifestorNoError;
}
//}}}
//{{{  QueueBuffer
//{{{  doxynote
/// \brief                      Actually put buffer on display
/// \param BufferIndex          Index into array of stream buffers
/// \param FrameParameters      Frame parameters generated by frame parser
/// \param VideoParameters      Display positioning information generated by frame parser
/// \param VideoOutputTiming    Display timing information generated by output timer
/// \return                     Success or fail
//}}}
ManifestorStatus_t Manifestor_VideoGrab_c::QueueBuffer(unsigned int                        BufferIndex,
                                                       struct ParsedFrameParameters_s     *FrameParameters,
                                                       struct ParsedVideoParameters_s     *VideoParameters,
                                                       struct ManifestationOutputTiming_s **VideoOutputTimingArray,
                                                       Buffer_t                            Buffer)
{
    struct StreamBuffer_s      *StreamBuff      = &StreamBuffer[BufferIndex];
    struct capture_buffer_s    *CaptureBuffer   = &CaptureBuffers[BufferIndex];
    int                         Status          = 0;

    AssertComponentState(ComponentRunning);
    SE_DEBUG(group_manifestor_video_grab, "CaptureDevice %p, Buffer %p, BufferClass %p\n", CaptureDevice, Buffer, StreamBuff->BufferClass);

    if (CaptureDevice == NULL)
    {
        return ManifestorError;
    }

    if ((VideoOutputTimingArray[SOURCE_INDEX]->DisplayCount[0] == 0)
        && (VideoOutputTimingArray[SOURCE_INDEX]->DisplayCount[1] == 0))
    {
        return ManifestorNoError;
    }

    //
    // Subscribe to End of stream event and set handler event if not yet done
    //
    if (MustSubscribeToEOS)
    {
        int  result = 0;
        /* Don't try to subscribe next time, even if it doesn't succeed first time */
        MustSubscribeToEOS = false;
        stm_event_subscription_entry_t event_entry;
        event_entry.object = (stm_object_h) Stream->GetHavanaStream();
        event_entry.event_mask = STM_SE_PLAY_STREAM_EVENT_END_OF_STREAM;
        event_entry.cookie = this;
        /* request only one subscription at a time */
        result = stm_event_subscription_create(&event_entry, 1, &EventSubscription);
        if (result < 0)
        {
            EventSubscription = NULL;
            SE_ERROR("Failed to Create Event Subscription for EOS event (%d)\n", result);
        }
        else
        {
            /* set the call back function for subscribed events */
            result = stm_event_set_handler(EventSubscription, VideoGrab_EndOfStreamEvent);
            if (result < 0)
            {
                EventSubscription = NULL;
                SE_ERROR("Failed to Create Event handler for EOS event (%d)\n", result);
            }
        }
    }

    StreamBuff->Manifestor          = this;
    Status                          = PrepareToQueue(Buffer, CaptureBuffer, VideoOutputTimingArray);

    if (Status != ManifestorNoError)
    {
        return Status;
    }

    StreamBuff->QueueCount          = 1;
    CaptureBuffer->user_private     = BufferIndex;
    StreamBuff->BufferState = BufferStateQueued;
    CaptureDevice->QueueBuffer(CaptureBuffer);

    return ManifestorNoError;
}
//}}}
//{{{  QueueNullManifestation
// /////////////////////////////////////////////////////////////////////////
//
//      QueueNullManifestation :
//      Action  : Insert null frame into display sequence
//      Input   :
//      Output  :
//      Results :
//

ManifestorStatus_t      Manifestor_VideoGrab_c::QueueNullManifestation(void)
{
    SE_DEBUG(group_manifestor_video_grab, "\n");
    return ManifestorNoError;
}
//}}}
//{{{  FlushDisplayQueue
//{{{  doxynote
/// \brief      Flushes the display queue so buffers not yet manifested are returned
//}}}
ManifestorStatus_t      Manifestor_VideoGrab_c::FlushDisplayQueue(void)
{
    SE_DEBUG(group_manifestor_video_grab, "\n");
    OS_LockMutex(&BufferLock);

    if (CaptureDevice == NULL)
    {
        OS_UnLockMutex(&BufferLock);
        return ManifestorError;
    }

    CaptureDevice->FlushQueue();
    OS_UnLockMutex(&BufferLock);
    return ManifestorNoError;
}
//}}}

//{{{  SynchronizeOutput
// ///////////////////////////////////////////////////////////////////////////////////////
//
//      SynchronizeOutput:
//      Action  : Force the grab driver to synchronize vsync
//      Input   : void
//      Output  :
//      Results :
//

ManifestorStatus_t Manifestor_VideoGrab_c::SynchronizeOutput(void)
{
    SE_DEBUG(group_manifestor_video_grab, "\n");
    return ManifestorNoError;
}
//}}}
//{{{  Enable
// ///////////////////////////////////////////////////////////////////////////////////////
//    Enable
//

ManifestorStatus_t Manifestor_VideoGrab_c::Enable(void)
{
    SE_DEBUG(group_manifestor_video_grab, "\n");
    Visible = true;
    return ManifestorNoError;
}
//}}}
//{{{  Disable
// ///////////////////////////////////////////////////////////////////////////////////////
//    Disable
//

ManifestorStatus_t Manifestor_VideoGrab_c::Disable(void)
{
    SE_DEBUG(group_manifestor_video_grab, "\n");
    return ManifestorNoError;
}
//}}}
//{{{  GetEnable
// ///////////////////////////////////////////////////////////////////////////////////////
//    GetEnable
//

bool Manifestor_VideoGrab_c::GetEnable(void)
{
    SE_DEBUG(group_manifestor_video_grab, "\n");
    return false;
}
//}}}
//{{{  RegisterCaptureDevice
ManifestorStatus_t Manifestor_VideoGrab_c::RegisterCaptureDevice(class HavanaCapture_c          *CaptureDevice)
{
    SE_INFO(group_manifestor_video_grab, "CaptureDevice %p\n", CaptureDevice);
    OS_LockMutex(&BufferLock);
    this->CaptureDevice     = CaptureDevice;
    OS_UnLockMutex(&BufferLock);
    return ManifestorNoError;
}
//}}}
//{{{  ReleaseCaptureBuffer
void Manifestor_VideoGrab_c::ReleaseCaptureBuffer(unsigned int    BufferIndex)
{
    struct StreamBuffer_s      *Buffer;
    SE_DEBUG(group_manifestor_video_grab, "%d\n", BufferIndex);

    if (BufferIndex == INVALID_INDEX)
    {
        SE_ERROR("Buffer with invalid Index\n");
        return;
    }

    Buffer                                              = &StreamBuffer[BufferIndex];
    BufferOnDisplay                                     = Buffer->BufferIndex;
    Buffer->OutputTiming[0]->ActualSystemPlaybackTime   = Buffer->OutputTiming[0]->SystemPlaybackTime;
    PtsOnDisplay                                        = Buffer->NativePlaybackTime;
    FrameCountManifested++;

    Buffer->QueueCount--;

    if (Buffer->QueueCount == 0)
    {
        Buffer->BufferState   = BufferStateAvailable;

        SE_VERBOSE(group_manifestor_video_grab, "Release Buffer #%d\n", Buffer->BufferIndex);
        mOutputPort->Insert((uintptr_t) Buffer->BufferClass);
    }
}
//}}}

