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

#include "manifestor_video_push_release_grab.h"

#undef TRACE_TAG
#define TRACE_TAG   "Manifestor_VideoPushRelease_c"

/*{{{  End of stream Event handler*/
void VideoPushRelease_EndOfStreamEvent(unsigned int number_of_events, stm_event_info_t *eventsInfo)
{
    SE_VERBOSE(group_manifestor_video_grab, "\n");
    Manifestor_VideoPushRelease_c *thisManifestor = (Manifestor_VideoPushRelease_c *)eventsInfo->cookie;
    thisManifestor->EndOfStreamEvent();
}
//}}}


//{{{  Callback function to to implement the EOS event handler
//{{{  doxynote
void Manifestor_VideoPushRelease_c::EndOfStreamEvent()
{
    uint64_t   CurrentSystemTime;
    //
    // check if currently connected, if not return immediately
    //
    if (!Connected)
    {
        SE_ERROR("Stream 0x%p this 0x%p Trying to push EOS while not attached to sink\n", Stream, this);
        return;
    }

    // General metadata setting for End of Stream
    CurrentSystemTime                                              = OS_GetTimeInMicroSeconds();
    CaptureBuffersForEOS.u.uncompressed.system_time                = CurrentSystemTime;
    // native_time = system_time for this EOS marked fake frame and so format is microsec
    CaptureBuffersForEOS.u.uncompressed.native_time                = CurrentSystemTime;
    CaptureBuffersForEOS.u.uncompressed.native_time_format         = TIME_FORMAT_US;

    CaptureBuffersForEOS.u.uncompressed.user_data_size             = 0;
    CaptureBuffersForEOS.u.uncompressed.user_data_buffer_address   = NULL;
    CaptureBuffersForEOS.u.uncompressed.media                      = STM_SE_ENCODE_STREAM_MEDIA_VIDEO;
    CaptureBuffersForEOS.buffer_length                             = 0;
    // Set copied frame size to zero to notify EOS
    CaptureBuffersForEOS.payload_length                            = 0;
    // set discontinuity End of Stream
    CaptureBuffersForEOS.u.uncompressed.discontinuity              = STM_SE_DISCONTINUITY_EOS;
    // Make sure EOS push will wait end of normal push
    SE_INFO(group_manifestor_video_grab, "Stream 0x%p this 0x%p Pushing EOS frame\n", Stream, this);
    OS_LockMutex(&BufferLock);

    if (PushReleaseInterface.push_data(SinkHandle, (stm_object_h)&CaptureBuffersForEOS) != 0)
    {
        // EOS frame not pushed
        SE_ERROR("Stream 0x%p this 0x%p Fails to push EOS frame\n", Stream, this);
    }

    OS_UnLockMutex(&BufferLock);
}
//}}}


//{{{  Callback function to release pushed frame
//{{{  doxynote
/// \param src_object           Pointer to the manifestor instance
/// \param released_object      Pointer to the uncompress metadata of the frame to release
/// \return                     -EPERM  if connection is closed
/// \return                     -EFAULT if invalid object
/// \return                     -EINVAL if frame already released
int sink_release_frame(stm_object_h src_object,
                       stm_object_h released_object)
{
    Manifestor_VideoPushRelease_c *thisManifestor = (Manifestor_VideoPushRelease_c *)src_object;
    return thisManifestor->SinkReleaseFrame(released_object);
}
//}}}


int  Manifestor_VideoPushRelease_c::SinkReleaseFrame(stm_object_h released_object)
{
    uint32_t    bufferToBeReleased;
    bool        validObject;

    //
    // check if currently connected
    //
    if (!Connected)
    {
        SE_ERROR("Stream 0x%p this 0x%p Unexpected frame release while not connected\n", Stream, this);
        return -EPERM;
    }

    //
    // Discard EOS released buffer
    //
    if ((stm_object_h) released_object == (stm_object_h)&CaptureBuffersForEOS)
    {
        // Nothing to release when getting back CaptureBuffersForEOS
        return 0;
    }

    //
    // Check if released frame has been pushed
    //
    validObject = false;

    for (bufferToBeReleased = 0; bufferToBeReleased < MAX_PUSHED_BUFFERS ; bufferToBeReleased++)
    {
        if ((stm_object_h) & (MetaDataForPushingArray [bufferToBeReleased]) == released_object)
        {
            validObject = true;

            //
            // Make sure this object is to be released
            //
            if (PushedMetadataArray[bufferToBeReleased] == true)
            {
                break;
            }
        }
    }

    //
    // return error if invalid object
    //
    if (validObject == false)
    {
        SE_ERROR("Stream 0x%p this 0x%p Release frame with invalid reference\n", Stream, this);
        return -EFAULT;
    }

    //
    // return error if frame already released
    //
    if (bufferToBeReleased >= MAX_PUSHED_BUFFERS)
    {
        SE_ERROR("Stream 0x%p this 0x%p Try to release same frame twice\n", Stream, this);
        return -EINVAL;
    }

    //
    // Do release the frame
    //
    SE_VERBOSE(group_manifestor_video_grab, "Stream 0x%p this 0x%p releasing frame idx %d\n", Stream, this, bufferToBeReleased);
    PushedMetadataArray[bufferToBeReleased] = false;
    mOutputPort->Insert((unsigned int) PushedBufferArray[bufferToBeReleased]);
    PushedBufferArray[bufferToBeReleased] = NULL;
    return  0;
}
//}}}


struct stm_data_interface_release_src se_manifestor_release_interface =
{
    sink_release_frame,
};


//{{{  Constructor
//{{{  doxynote
/// \brief                      Initial state
/// \return                     Success or fail
Manifestor_VideoPushRelease_c::Manifestor_VideoPushRelease_c(void)
    : Manifestor_Source_Base_c()
    , SurfaceDescriptor()
    , ManifestationComponent(PrimaryManifestationComponent)
    , Connected(false)
    , PushReleaseInterface()
    , EventSubscription()
    , SinkHandle(NULL)
    , VideoMetadataHelper()
    , BufferLock()
    , MetaDataForPushingArray()
    , CaptureBuffersForEOS()
    , PushedMetadataArray()
    , PushedBufferArray()
    , FramePhysicalAddress(NULL)
    , FrameVirtualAddress(NULL)
    , FrameSize(0)
{
    if (InitializationStatus != ManifestorNoError)
    {
        SE_ERROR("Stream 0x%p this 0x%p Initialization status not valid - aborting init\n", Stream, this);
        return;
    }

    SE_VERBOSE(group_manifestor_video_grab, "Stream 0x%p this 0x%p\n", Stream, this);

    SetGroupTrace(group_manifestor_video_grab);

    Configuration.Capabilities = MANIFESTOR_CAPABILITY_PUSH_RELEASE;

    SurfaceDescriptor.StreamType                = StreamTypeVideo;
    SurfaceDescriptor.ClockPullingAvailable     = false;
    SurfaceDescriptor.DisplayWidth              = FRAME_GRAB_DEFAULT_DISPLAY_WIDTH;
    SurfaceDescriptor.DisplayHeight             = FRAME_GRAB_DEFAULT_DISPLAY_HEIGHT;
    SurfaceDescriptor.Progressive               = true;
    SurfaceDescriptor.FrameRate                 = FRAME_GRAB_DEFAULT_FRAME_RATE; // rational
    SurfaceDescriptor.PercussiveCapable         = true;
    // But request that we set output to match input rates
    SurfaceDescriptor.InheritRateAndTypeFromSource = true;

    OS_InitializeMutex(&BufferLock);
}
//}}}

//{{{  Destructor
//{{{  doxynote
/// \brief                      Try to disconnect if not done
/// \return                     Success or fail
Manifestor_VideoPushRelease_c::~Manifestor_VideoPushRelease_c(void)
{
    SE_VERBOSE(group_manifestor_video_grab, "Stream 0x%p this 0x%p\n", Stream, this);
    Disconnect(SinkHandle);

    OS_TerminateMutex(&BufferLock);
}
//}}}


//{{{  QueueDecodeBuffer
//{{{  doxynote
/// \brief                      Push new decoded frame to attached sink port
/// \param Buffer               buffer
/// \return                     Success or fail
//}}}
ManifestorStatus_t  Manifestor_VideoPushRelease_c::QueueDecodeBuffer(Buffer_t Buffer, ManifestationOutputTiming_t **Timing, unsigned int *NumTimes)
{
    ManifestorStatus_t                  Status = ManifestorNoError;
    uint32_t                            bufferIndex;
    AssertComponentState(ComponentRunning);

    //
    // Check availability of queues
    //
    if (!mOutputPort)
    {
        SE_ERROR("Stream 0x%p this 0x%p mOutputPort not available\n", Stream, this);
        return ManifestorError;
    }

    //
    // No action if not connected yet
    //
    if (!Connected)
    {
        // Buffer can be released immediately
        mOutputPort->Insert((unsigned int) Buffer);
        return ManifestorNoError;
    }

    //
    // Set SourceStreamBuffer of the CurrentBuffer thanks to its index
    //
    Buffer->GetIndex(&bufferIndex);

    //
    // Make sure Index is correct
    //
    if (bufferIndex >= MAX_PUSHED_BUFFERS)
    {
        SE_ERROR("Stream 0x%p this 0x%p Wrong Buffer Index: %x\n", Stream, this, bufferIndex);
        return ManifestorError;
    }

    //
    // Check if frame is not already pushed (for debug only)
    //
    if (PushedMetadataArray[bufferIndex] != false)
    {
        SE_ERROR("Stream 0x%p this 0x%p Trying to push same frame twice %d\n", Stream, this, bufferIndex);
        return ManifestorError;
    }

    //
    // Select Primary or decimated component according if exist and requested
    //
    ManifestationComponent  = PrimaryManifestationComponent;

    if ((Player->PolicyValue(Playback, Stream, PolicyDecimateDecoderOutput) == PolicyValueDecimateDecoderOutputDisabled)
        && (Stream->GetDecodeBufferManager()->ComponentPresent(Buffer, DecimatedManifestationComponent)))
    {
        SE_VERBOSE(group_manifestor_video_grab, "Stream 0x%p this 0x%p DecimatedManifestationComponent selected", Stream, this);
        ManifestationComponent   = DecimatedManifestationComponent;
    }

    //
    // Prepare frame to be pushed
    //
    Status     = PrepareToPush(Buffer, &MetaDataForPushingArray[bufferIndex], *Timing);

    if (Status != ManifestorNoError)
    {
        SE_ERROR("Stream 0x%p this 0x%p Fails to prepare frame to push %d\n", Stream, this, bufferIndex);
        return ManifestorError;
    }

    //
    // Record frame as pushed
    //
    PushedMetadataArray[bufferIndex] = true;
    PushedBufferArray  [bufferIndex] = Buffer;
    //
    //  Perform effective push
    //  Make sure EOS push will wait end of this push (BufferLock)
    //
    SE_VERBOSE(group_manifestor_video_grab, "Stream 0x%p this 0x%p pushing frame %d", Stream, this, bufferIndex);
    OS_LockMutex(&BufferLock);

    if (PushReleaseInterface.push_data(SinkHandle, (stm_object_h)&MetaDataForPushingArray[bufferIndex]) != 0)
    {
        // frame not pushed
        SE_ERROR("Stream 0x%p this 0x%p Fails to push frame %d\n", Stream, this, bufferIndex);
        PushedMetadataArray[bufferIndex] = false;
        PushedBufferArray  [bufferIndex] = NULL;
        OS_UnLockMutex(&BufferLock);
        return ManifestorError;
    }

    OS_UnLockMutex(&BufferLock);
    return Status;
}

//}}}
//{{{  GetNextQueuedManifestationTime
//{{{  doxynote
/// \brief      Get the earliest System time at which the next frame to be queued will be manifested.
///             For more details, please have a look at the doxygen definition of
///             ManifestorStatus_t Manifestor_c::GetNextQueuedManifestationTime(unsigned long long *Time, unsigned int *NumTimes)
/// \param Buffer  Time : estimated worse case time at which the next frame will be grabbed.
/// \return     Alway no error
//}}}
ManifestorStatus_t      Manifestor_VideoPushRelease_c::GetNextQueuedManifestationTime(unsigned long long    *Time, unsigned int *NumTimes)
{
    uint32_t estimatedDelay;
    uint32_t currentQueuedBuffers;
    // Estimate pushed frame will be released in quarter of a second.
    *Time     = OS_GetTimeInMicroSeconds() + 250000;
    *NumTimes = 1;
    //
    // Take into account the number of pushed buffers to be manifested
    // Cumulated time at estimated 40ms per frame
    //
    currentQueuedBuffers = pushedBufferCount();
    estimatedDelay = currentQueuedBuffers * 40000;
    *Time += estimatedDelay;
    SE_DEBUG(group_manifestor_video_grab, "Stream 0x%p this 0x%p estimatedDelay %d for %d buffers\n", Stream, this, estimatedDelay, currentQueuedBuffers);
    return ManifestorNoError;
}


//}}}
//{{{  GetBufferId
//{{{  doxynote
/// \brief      Retrieve buffer index of last buffer queued. If no buffers
///             yet queued returns ANY_BUFFER_ID
/// \return     Buffer index of last buffer sent for display
//}}}
unsigned int    Manifestor_VideoPushRelease_c::GetBufferId(void)
{
    // This method is used to identify the events to notify, corresponding to a frame after manifestation
    // no event is managed by this manifestor. Consequently, it returns the default valid value.
    return ANY_BUFFER_ID;
}

///{{{ pushedBufferCount
/// \brief                      Return the number of frame currently pushed
/// \return                     Number of current pushed buffers
uint32_t  Manifestor_VideoPushRelease_c::pushedBufferCount(void)
{
    uint32_t  puhedBufferId;
    uint32_t  nbBufPushed = 0;

    for (puhedBufferId = 0; puhedBufferId < MAX_PUSHED_BUFFERS ; puhedBufferId++)
    {
        // Count the number of currently pushed frames not yet released
        if (PushedMetadataArray[puhedBufferId] == true)
        {
            nbBufPushed++;
        }
    }

    return nbBufPushed;
}

//{{{  FlushDisplayQueue
//{{{  doxynote
/// \brief      Flushes the display queue so buffers not yet manifested are returned
ManifestorStatus_t      Manifestor_VideoPushRelease_c::FlushDisplayQueue(void)
{
    SE_DEBUG(group_manifestor_video_grab, "Stream 0x%p this 0x%p >\n", Stream, this);
    uint32_t  NbTries = 0;
    unsigned int bufferToBeRelease = 0;

    //
    // check if currently connected
    //
    if (!Connected)
    {
        return ManifestorNoError;
    }

    //
    // Check if buffers still not released
    //
    do
    {
        for (bufferToBeRelease = 0; bufferToBeRelease < MAX_PUSHED_BUFFERS ; bufferToBeRelease++)
        {
            if (PushedMetadataArray[bufferToBeRelease] == true)
            {
                break;
            }
        }

        if (bufferToBeRelease < MAX_PUSHED_BUFFERS)
        {
            SE_INFO(group_manifestor_video_grab, "Stream 0x%p this 0x%p Waiting for pushed buffer to be released (0x%p) tries #%d\n", Stream, this, SinkHandle, NbTries);
            //
            // Pushed buffers must all be released
            // Wait up to max. pushed frames x 25pfs duration (40ms)
            //
            OS_SleepMilliSeconds(40);
            NbTries++;

            if (NbTries > MAX_PUSHED_BUFFERS)
            {
                SE_ERROR("Stream 0x%p this 0x%p Unreleased pushed frame detection. Abort after %d tries\n", Stream, this, NbTries);
                return ManifestorError;
            }
        }
    }
    while (bufferToBeRelease < MAX_PUSHED_BUFFERS);

    SE_DEBUG(group_manifestor_video_grab, "Stream 0x%p this 0x%p <\n", Stream, this);
    return ManifestorNoError;
}
//}}}

//{{{  Connect
//{{{  doxynote
/// \brief                      Connect to memory Sink port
/// \param Buffer               SinkHandle : memory sink object to connect to
/// \return                     Success or fail
ManifestorStatus_t Manifestor_VideoPushRelease_c::Connect(stm_object_h  SrcHandle, stm_object_h  SinkHandle)
{
    SE_DEBUG(group_manifestor_video_grab, "Stream 0x%p this 0x%p >\n", Stream, this);
    int           retval;
    stm_object_h  sinkType;
    char          tagTypeName [STM_REGISTRY_MAX_TAG_SIZE];
    int32_t       returnedSize;

    if (Connected)
    {
        SE_ERROR("Stream 0x%p this 0x%p already connected\n", Stream, this);
        return ManifestorError;
    }

    //
    // Need to subscribe PlayerStream event 'STM_SE_PLAY_STREAM_EVENT_END_OF_STREAM'
    //
    {
        //
        // Subscribe to End of stream event and set handler event
        //
        stm_event_subscription_entry_t event_entry;
        event_entry.object = (stm_object_h) SrcHandle;
        event_entry.event_mask = STM_SE_PLAY_STREAM_EVENT_END_OF_STREAM;
        event_entry.cookie = this;
        // request only one subscription at a time
        retval = stm_event_subscription_create(&event_entry, 1, &EventSubscription);
        if (retval < 0)
        {
            EventSubscription = NULL;
            SE_ERROR("Stream 0x%p this 0x%p create event subscription for EOS event failed (%d)\n", Stream, this, retval);
            goto memsink_error;
        }

        // set the call back function for subscribed events
        retval = stm_event_set_handler(EventSubscription, VideoPushRelease_EndOfStreamEvent);
        if (retval < 0)
        {
            SE_ERROR("Stream 0x%p this 0x%p create event handler for EOS event failed (%d)\n", Stream, this, retval);
            /* release substriction before the exit */
            retval = stm_event_subscription_delete(EventSubscription);
            if (retval < 0)
            {
                SE_ERROR("Stream 0x%p this 0x%p release event subscription failed (%d)\n", Stream, this, retval);
            }

            EventSubscription = NULL;
            goto memsink_error;
        }
    }
    //
    // Check sink object support STM_DATA_INTERFACE_PUSH_RELEASE interface
    //
    retval = stm_registry_get_object_type(SinkHandle, &sinkType);
    if (retval)
    {
        SE_ERROR("Stream 0x%p this 0x%p stm_registry_get_object_type(0x%p, &0x%p) failed (%d)\n", Stream, this, SinkHandle, sinkType, retval);
        goto memsink_error;
    }

    retval = stm_registry_get_attribute(SinkHandle,
                                        STM_DATA_INTERFACE_PUSH_RELEASE,
                                        tagTypeName,
                                        sizeof(PushReleaseInterface),
                                        &PushReleaseInterface,
                                        (int *)&returnedSize);
    if ((retval) || (returnedSize != sizeof(PushReleaseInterface)))
    {
        SE_ERROR("Stream 0x%p this 0x%p stm_registry_get_attribute() failed (%d)\n", Stream, this, retval);
        goto memsink_error;
    }

    SE_DEBUG(group_manifestor_video_grab, "Stream 0x%p this 0x%p Getting tag type %s\n", Stream, this, tagTypeName);
    //
    // call the sink interface's connect handler to connect the consumer */
    //
    SE_DEBUG(group_manifestor_video_grab, "Stream 0x%p this 0x%p tries to connect to memorySink\n", Stream, this);
    retval = PushReleaseInterface.connect((stm_object_h)this       , // SRC
                                          SinkHandle,                // SINK
                                          (struct stm_data_interface_release_src *) &se_manifestor_release_interface);
    if (retval)
    {
        // Connection fails : Free OS objects
        SE_ERROR("Stream 0x%p this 0x%p PushReleaseInterface.connect(%p) failed (%d)\n", Stream, this, SinkHandle, retval);
        goto memsink_error;
    }

    //
    // Save sink object for disconnection
    //
    this->SinkHandle = SinkHandle;
    //
    // Initialize connection
    //
    initialiseConnection();
    Connected = true;
    SetComponentState(ComponentRunning);
    SE_DEBUG(group_manifestor_video_grab, "Stream 0x%p this 0x%p <\n", Stream, this);
    return ManifestorNoError;
    //
    // Cannot connect to memsink: release resources
    //
memsink_error:
    // release resources
    return ManifestorError;
}
//}}}

//{{{  Disconnect
//{{{  doxynote
/// \brief                      Disconnect from memory Sink port (if connected)
///                             Release pending pull call if any
/// \return                     Success or fail
ManifestorStatus_t Manifestor_VideoPushRelease_c::Disconnect(stm_object_h  SinkHandle)
{
    SE_DEBUG(group_manifestor_video_grab, "Stream 0x%p this 0x%p >\n", Stream, this);
    int       retval;

    //
    // check if currently connected
    //
    if (!Connected)
    {
        return ManifestorError;
    }

    //
    // Check that Sink object correspond the connected one
    //
    if (this->SinkHandle != SinkHandle)
    {
        return ManifestorError;
    }

    /* Remove event subscription */
    if (EventSubscription)
    {
        retval = stm_event_subscription_delete(EventSubscription);
        if (retval < 0)
        {
            SE_ERROR("Stream 0x%p this 0x%p delete Event Subscription for EOS event failed (%d)\n", Stream, this, retval);
        }

        EventSubscription = NULL;
    }

    //
    // Check if all pushed buffers are released
    // If not way for them
    //
    terminateConnection();
    //
    // Close connection for any incoming frames
    //
    Connected = false;
    // reset old sink object
    this->SinkHandle = NULL;
    //
    // call the sink interface's disconnect handler to disconnect the consumer
    //
    retval = PushReleaseInterface.disconnect((stm_object_h)this, SinkHandle);
    if (retval)
    {
        SE_ERROR("Stream 0x%p this 0x%p PushReleaseInterface.disconnect(0x%p) failed (%d)\n", Stream, this, SinkHandle, retval);
    }

    SE_DEBUG(group_manifestor_video_grab, "Stream 0x%p this 0x%p <\n", Stream, this);
    return ManifestorNoError;
}
//}}}

///{{{ initialiseConnection
/// \brief                      Reset CurrentBuffer and Waiting List
/// \return                     Success or fail
ManifestorStatus_t  Manifestor_VideoPushRelease_c::initialiseConnection(void)
{
    SE_DEBUG(group_manifestor_video_grab, "Stream 0x%p this 0x%p\n", Stream, this);
    //
    // perform general initialisation
    //
    for (unsigned int i = 0; i < MAX_PUSHED_BUFFERS; i++)
    {
        PushedMetadataArray[i] = false;
        PushedBufferArray[i]   = NULL;
    }

    return ManifestorNoError;
}
//}}}

///{{{ terminateConnection
/// \brief                      Check if all frames have been released
/// \brief                      wait for them if missing
/// \return                     Success or fail
ManifestorStatus_t  Manifestor_VideoPushRelease_c::terminateConnection(void)
{
    SE_DEBUG(group_manifestor_video_grab, "Stream 0x%p this 0x%p\n", Stream, this);
    return FlushDisplayQueue();
}
//}}}

///{{{ GetSurfaceParameters
/// \brief                      Return Video surface descriptor
/// \return                     Success or fail
ManifestorStatus_t      Manifestor_VideoPushRelease_c::GetSurfaceParameters(OutputSurfaceDescriptor_t   **SurfaceParameters, unsigned int *NumSurfaces)
{
    *SurfaceParameters  = &SurfaceDescriptor;
    *NumSurfaces        = 1;
    return ManifestorNoError;
}

//}}}
//{{{  PrepareToPull
//{{{  doxynote
/// \brief              Prepare the frame in the capture Buffer
/// \param              Buffer containing frame pulled by memorySink
/// \result             Success if frame displayed, fail if not displayed or a frame has already been displayed
//}}}
ManifestorStatus_t      Manifestor_VideoPushRelease_c::PrepareToPush(class Buffer_c                      *Buffer,
                                                                     stm_se_capture_buffer_t             *CaptureBuffer,
                                                                     struct ManifestationOutputTiming_s  *VideoOutputTiming)
{
    struct ParsedVideoParameters_s     *VideoParameters;
    struct ParsedFrameParameters_s     *ParsedFrameParameters;

    Buffer->ObtainMetaDataReference(Player->MetaDataParsedVideoParametersType, (void **)&VideoParameters);
    SE_ASSERT(VideoParameters != NULL);

    Buffer->ObtainMetaDataReference(Player->MetaDataParsedFrameParametersReferenceType, (void **)(&ParsedFrameParameters));
    SE_ASSERT(ParsedFrameParameters != NULL);

    //
    // UpdateOutputSurfaceDescriptor
    //
    if (VideoParameters->Content.FrameRate != 0)
    {
        SurfaceDescriptor.DisplayWidth          = VideoParameters->Content.DisplayWidth;
        SurfaceDescriptor.DisplayHeight         = VideoParameters->Content.DisplayHeight;
        SurfaceDescriptor.Progressive           = VideoParameters->Content.Progressive;
        SurfaceDescriptor.FrameRate             = VideoParameters->Content.FrameRate;
    }
    else
    {
        SurfaceDescriptor.DisplayWidth          = FRAME_GRAB_DEFAULT_DISPLAY_WIDTH;
        SurfaceDescriptor.DisplayHeight         = FRAME_GRAB_DEFAULT_DISPLAY_HEIGHT;
        SurfaceDescriptor.Progressive           = true;
        SurfaceDescriptor.FrameRate             = FRAME_GRAB_DEFAULT_FRAME_RATE; // rational
    }

    //
    // CaptureBuffer structure provides the size, physical and virtual address of the destination buffer
    //
    FramePhysicalAddress                    = (void *) Stream->GetDecodeBufferManager()->ComponentBaseAddress(Buffer, ManifestationComponent, PhysicalAddress);
    FrameVirtualAddress                     = (void *) Stream->GetDecodeBufferManager()->ComponentBaseAddress(Buffer, ManifestationComponent, CachedAddress);
    FrameSize                               = Stream->GetDecodeBufferManager()->ComponentSize(Buffer, ManifestationComponent);
    //
    // Meta Data setup
    //
    CaptureBuffer->physical_address         = FramePhysicalAddress;
    CaptureBuffer->virtual_address          = FrameVirtualAddress;
    CaptureBuffer->buffer_length            = FrameSize;
    CaptureBuffer->payload_length           = FrameSize;
    CaptureBuffer->u.uncompressed.system_time              = VideoOutputTiming->SystemPlaybackTime;
    CaptureBuffer->u.uncompressed.native_time_format       = TIME_FORMAT_US;
    CaptureBuffer->u.uncompressed.native_time              = ParsedFrameParameters->NormalizedPlaybackTime;
    setUncompressedMetadata(Buffer, &CaptureBuffer->u.uncompressed, VideoParameters);
    return ManifestorNoError;
}
//}}}

