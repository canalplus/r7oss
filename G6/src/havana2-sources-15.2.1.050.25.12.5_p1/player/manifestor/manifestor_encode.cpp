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

#include "manifestor_encode.h"

#undef TRACE_TAG
#define TRACE_TAG   "Manifestor_Encoder_c"

//{{{  Stream Event C-Stub
static void EncodeStreamEventHandler(unsigned int number_of_events, stm_event_info_t *eventsInfo)
{
    Manifestor_Encoder_c *EncodeManifestor = (Manifestor_Encoder_c *)eventsInfo->cookie;
    EncodeManifestor->HandleEvent(number_of_events, eventsInfo);
}
//}}}

//{{{  Constructor
//{{{  doxynote
/// \brief                      Initial state
/// \return                     InitializationStatus should be checked following construct
//}}}
Manifestor_Encoder_c::Manifestor_Encoder_c(void)
    : Manifestor_Base_c()
    , Encoder(NULL)
    , EncodeStream(NULL)
    , Encoder_BufferTypes(NULL)
    , SurfaceDescriptor()
    , ConnectBufferLock()
    , EventSubscription(NULL)
    , MustSubscribeToEOS(true)
    , Connected(false)
    , Interrupted(false)
    , Enabled(false)
    , EncodeBufferArray()
    , FakeEndOfStreamBuffer(NULL)
    , LastBufferId(ANY_BUFFER_ID)
    , mEncodeStreamPort(NULL)
    , mLastEncodedPts(INVALID_TIME)
{
    if (InitializationStatus != ManifestorNoError)
    {
        SE_ERROR("Initialization status not valid - aborting init\n");
        return;
    }

    Configuration.Capabilities = MANIFESTOR_CAPABILITY_ENCODE;
    OS_InitializeMutex(&ConnectBufferLock);

    // need a fake buffer_c address for EOS buffer detection
    FakeEndOfStreamBuffer = (Buffer_t)&Connected;
}
//}}}

//{{{  Destructor
//{{{  doxynote
/// \brief                      Halt to disconnect and then purge any remaining frames on the queue
/// \return                     No return value
//}}}
Manifestor_Encoder_c::~Manifestor_Encoder_c(void)
{
    Halt();

    OS_TerminateMutex(&ConnectBufferLock);
}
//}}}

//{{{  Halt
//{{{  doxynote
/// \brief                      Release all external resources/handles.
///                             In this instance, the EncodeStream
/// \return                     Halt is a best effort task. It cannot fail.
//}}}
ManifestorStatus_t Manifestor_Encoder_c::Halt(void)
{
    SE_DEBUG(GetGroupTrace(), "%s\n", Configuration.ManifestorName);

    // Perform Halt Actions for the Base Class.
    Manifestor_Base_c::Halt();

    // Disconnect encode stream
    Disconnect(EncodeStream);

    return ManifestorNoError;
}
//}}}

//{{{  Reset
//{{{  doxynote
//}}}
ManifestorStatus_t Manifestor_Encoder_c::Reset(void)
{
    Encoder                 = NULL;
    EncodeStream            = NULL;
    Encoder_BufferTypes     = NULL;
    EventSubscription       = NULL;
    MustSubscribeToEOS      = true;
    Connected               = false;
    Interrupted             = false;
    Enabled                 = false;
    LastBufferId            = ANY_BUFFER_ID;
    mEncodeStreamPort       = NULL;
    mLastEncodedPts       = INVALID_TIME;

    // Reset array of Decoded buffers pushed to Encoder subsystem
    memset(EncodeBufferArray, 0, sizeof(EncodeBufferArray));

    return ManifestorNoError;
}
//}}}

//{{{  ManageEventSubscription
//{{{  doxynote
/// \brief                      Handle any STKPI Event Subscriptions desired by this object
/// \param                      SrcHandle  : HavanaStream object
/// \return                     Success or Fail.
//}}}
ManifestorStatus_t  Manifestor_Encoder_c::ManageEventSubscription(stm_object_h  SrcHandle)
{
    //
    // Subscribe to End of stream event and set handler event if not yet done
    //
    if (MustSubscribeToEOS)
    {
        int  result = 0;
        /* Don't try to subscribe next time, even if it doesn't succeed first time */
        MustSubscribeToEOS = false;
        stm_event_subscription_entry_t event_entry;
        event_entry.object = SrcHandle;
        event_entry.event_mask = STM_SE_PLAY_STREAM_EVENT_END_OF_STREAM;
        event_entry.cookie = this;
        /* request only one subscription at a time */

        result = stm_event_subscription_create(&event_entry, 1, &EventSubscription);
        if (result < 0)
        {
            EventSubscription = NULL;
            SE_ERROR("%s - Failed to Create Event Subscription for EOS event (%d)\n", Configuration.ManifestorName, result);
        }
        else
        {
            /* set the call back function for subscribed events */
            result = stm_event_set_handler(EventSubscription, EncodeStreamEventHandler);
            if (result < 0)
            {
                EventSubscription = NULL;
                SE_ERROR("%s - Failed to Create Event handler for EOS event (%d)\n", Configuration.ManifestorName, result);
            }
        }
    }

    return ManifestorNoError;
}
//}}}

//{{{  HandleEvent
//{{{  doxynote
/// \brief                      Handle any STKPI Event received by this object
/// \param                      number_of_events The number of events that have occured to process
/// \param                      eventsInfo A pointer to the Event Structures
/// \return                     Success or Fail, If the Manifestor is not connected events are not processed.
//}}}
ManifestorStatus_t  Manifestor_Encoder_c::HandleEvent(unsigned int number_of_events, stm_event_info_t *eventsInfo)
{
    stm_event_t                              *Event = &eventsInfo->event;

    // Prevent from parallel disconnection
    OS_LockMutex(&ConnectBufferLock);

    if (!Connected)
    {
        SE_ERROR("%s - Not connected\n", Configuration.ManifestorName);
        OS_UnLockMutex(&ConnectBufferLock);
        return ManifestorError;
    }

    //
    // Currently this handler manages only one event "PlayerStreamEventEndOfStream"
    //
    SE_VERBOSE(GetGroupTrace(), "%s - %d events to handle\n", Configuration.ManifestorName, number_of_events);

    switch (Event->event_id)
    {
    case STM_SE_PLAY_STREAM_EVENT_END_OF_STREAM:
        SE_INFO(GetGroupTrace(), "%s - EOS event\n", Configuration.ManifestorName);
        PushEndOfStreamInputBuffer();
        break;

    default:
        SE_ERROR("%s - Unhandled Event %u\n", Configuration.ManifestorName, Event->event_id);
        break;
    }

    OS_UnLockMutex(&ConnectBufferLock);

    return ManifestorNoError;
}
//}}}

//{{{  QueueDecodeBuffer
//{{{  doxynote
/// \brief                      Queue a decoded buffer from the Player to the Encoder
///
///  This function is called to receive a decode buffer from the Decode
///  Pipeline. The buffer is inserted in the EncodeStream port
///
/// \param Buffer               A Decode buffer from the Decode Pipeline
/// \param TimingArray          Timing array from the Manifestation Coordinator
/// \param NumTimes             The number of times this frame should be presented.
/// \return                     Success or fail based on if the buffer is queued or not.
//}}}
ManifestorStatus_t  Manifestor_Encoder_c::QueueDecodeBuffer(Buffer_t                      Buffer,
                                                            ManifestationOutputTiming_t **TimingArray,
                                                            unsigned int                 *NumTimes)
{
    unsigned int                  EncodeBufferIndex = 0;
    Buffer_t                      EncodeBuffer;
    ManifestorStatus_t            Status;
    AssertComponentState(ComponentRunning);

    //
    // Check availability of queues
    //

    if (!mOutputPort)
    {
        SE_ERROR("%s - mOutputPort not available\n", Configuration.ManifestorName);
        return ManifestorError;
    }

    // Prevent from parallel disconnection
    OS_LockMutex(&ConnectBufferLock);

    //
    // No action if not connected yet
    //

    if (!Connected)
    {
        SE_ERROR("%s Not connected\n", Configuration.ManifestorName);
        OS_UnLockMutex(&ConnectBufferLock);
        return ManifestorError;
    }

    //
    // We will only ever queue one buffer once for encode ... (At least at the moment)
    //

    if (*NumTimes > 1)
    {
        SE_ERROR("%s - Implementation Error can only handle one output timing\n", Configuration.ManifestorName);
    }

    //
    // The base class would like to be able to find out what the last buffer queued was ...
    //
    Buffer->GetIndex(&LastBufferId);

    //
    // Now that we have a buffer containing a frame,
    // its time to Encode it...
    //
    SE_DEBUG(GetGroupTrace(), "%s - extract %p\n", Configuration.ManifestorName, Buffer);
    Status = PrepareEncodeMetaData(Buffer, &EncodeBuffer);

    if (Status != ManifestorNoError)
    {
        SE_ERROR("Unable to obtain EncodeBuffer - Frame not pushed to encoder\n");
        OS_UnLockMutex(&ConnectBufferLock);
        return ManifestorError;
    }

    //
    // Save decoded buffer reference in the corresponding Encode Input Buffer array index
    //
    EncodeBuffer->GetIndex(&EncodeBufferIndex);
    if (EncodeBufferIndex >= ENCODER_MAX_INPUT_BUFFERS)
    {
        SE_ERROR("Unable to obtain EncodeBuffer index - Implementation error\n");
        EncodeBuffer->DecrementReferenceCount(IdentifierGetInjectBuffer);
        OS_UnLockMutex(&ConnectBufferLock);
        return ManifestorError;
    }

    EncodeBufferArray[EncodeBufferIndex] = Buffer;

    //
    // Push the Encode Input Buffer to Encoder subsystem.
    // Buffer will be released via EncodeReleaseBuffer API.
    //
    if (mEncodeStreamPort->Insert((uintptr_t)EncodeBuffer) != RingNoError)
    {
        SE_ERROR("Unable to send InputBuffer to EncodeStream\n");
        EncodeBufferArray[EncodeBufferIndex] = NULL;
        EncodeBuffer->DecrementReferenceCount(IdentifierGetInjectBuffer);
        OS_UnLockMutex(&ConnectBufferLock);
        return ManifestorError;
    }

    OS_UnLockMutex(&ConnectBufferLock);
    return ManifestorNoError;
}
//}}}

//{{{  GetSurfaceParameters
//{{{  doxynote
/// \brief      Fill in private structure with timing details of display surface
/// \param      SurfaceParameters pointer to structure to complete
/// \param      NumSurfaces Number of surfaces our manifestor has
//}}}
ManifestorStatus_t      Manifestor_Encoder_c::GetSurfaceParameters(OutputSurfaceDescriptor_t   **SurfaceParameters, unsigned int *NumSurfaces)
{
    // Set our only Surface Parameter Set
    SurfaceParameters[0] = &SurfaceDescriptor;
    // We only have 1 Surface Parameter Timing Set
    *NumSurfaces = 1;
    return ManifestorNoError;
}
//}}}

//}}}
//{{{  GetBufferId
//{{{  doxynote
/// \brief      Retrieve buffer index of last buffer queued. If no buffers
///             yet queued returns ANY_BUFFER_ID
/// \return     Buffer index of last buffer sent for display
//}}}
unsigned int    Manifestor_Encoder_c::GetBufferId(void)
{
    return LastBufferId;
}
//}}}

//{{{  GetNextQueuedManifestationTime
//{{{  doxynote
/// \brief      Get the earliest System time at which the next frame to Be queued will be grabbed.
///             For more details, please have a look at the doxygen definition of
///             ManifestorStatus_t Manifestor_c::GetNextQueuedManifestationTime(unsigned long long *Time, unsigned int *NumTimes)
/// \param      Time     : estimated worse case time at which the next frame will be grabbed.
/// \param      NumTimes : Number of Timings to manifestations which will occur
/// \return     Always no error
//}}}
ManifestorStatus_t      Manifestor_Encoder_c::GetNextQueuedManifestationTime(unsigned long long    *Time, unsigned int *NumTimes)
{
    uint32_t estimatedDelay = 0;
    uint32_t currentQueuedBuffers = 0;
    // Estimate memsink pull will grab in quarter of a second.
    *Time     = OS_GetTimeInMicroSeconds() + 250000;
    *NumTimes = 1;
    //
    // Take into account the number of queued buffers to be grabbed
    // Cumulated time at estimated 40ms per frame
    //
#if 0 // Not implemented in the Ring_t type ... maybe it should be ...
    currentQueuedBuffers = waitListBufferCount();
    estimatedDelay = currentQueuedBuffers * 40000;
#endif
    *Time += estimatedDelay;
    SE_DEBUG(GetGroupTrace(), "%s - estimatedDelay:%d  for %d buffers\n",
             Configuration.ManifestorName,
             estimatedDelay, currentQueuedBuffers);
    return ManifestorNoError;
}
//}}}

//{{{  PurgeEncodeStream
//{{{  doxynote
/// \brief                      Discard all encode buffers
/// \return    ManifestorError   : in case at least one encode input buffer is not released
/// \return    ManifestorNoError : otherwise
//}}}
ManifestorStatus_t  Manifestor_Encoder_c::PurgeEncodeStream(void)
{
    unsigned int index;
    unsigned int nbTries;

    SE_INFO(GetGroupTrace(), ">\n");

    //
    // Flush Encode stream port
    //
    if (EncodeStream->Flush() != EncoderNoError)
    {
        SE_ERROR("Unable to flush the EncodeStream port - Implementation error\n");
        return ManifestorError;
    }

    //
    // Make sure all Encode input buffers pushed to Encoder have been released
    // Manage timeout in case of missing release of encode input buffer
    //
    index = 0;
    nbTries = 0;
    while ((index < ENCODER_MAX_INPUT_BUFFERS) && (nbTries < 100))
    {
        if (EncodeBufferArray[index] != NULL)
        {
            OS_SleepMilliSeconds(10);
            nbTries ++;
        }
        else
        {
            index ++;
            nbTries = 0;
        }
    }

    if (index < ENCODER_MAX_INPUT_BUFFERS)
    {
        SE_ERROR("Timeout - At least one encode input buffer is not released\n");
        return ManifestorError;
    }

    SE_DEBUG(GetGroupTrace(), "<\n");

    return ManifestorNoError;
}
//}}}

//{{{  FlushDisplayQueue
//{{{  doxynote
/// \brief                      Flushes the display queue so buffers not yet manifested are returned
/// \return                     Always no error.
//}}}
ManifestorStatus_t      Manifestor_Encoder_c::FlushDisplayQueue(void)
{
    //
    // check if currently connected
    //
    if (!Connected)
    {
        return ManifestorNoError;
    }

    SE_DEBUG(GetGroupTrace(), "%s\n", Configuration.ManifestorName);

    return PurgeEncodeStream();

}
//}}}

//{{{  Connect
//{{{  doxynote
/// \brief                      Connect to memory Sink port
/// \param                      SrcHandle  : HavanaStream object
/// \param                      SinkHandle : EncodeStream object to connect to
/// \return                     Success or fail
//}}}
ManifestorStatus_t Manifestor_Encoder_c::Connect(stm_object_h  SrcHandle, stm_object_h  SinkHandle)
{
    if (Connected)
    {
        SE_ERROR("%s - already connected\n", Configuration.ManifestorName);
        return ManifestorError;
    }

    if (SinkHandle == NULL || SrcHandle == NULL)
    {
        SE_ERROR("%s - Invalid attachment parameters\n", Configuration.ManifestorName);
        return ManifestorError;
    }

    SE_INFO(GetGroupTrace(), "%s srchandle:0x%p sinkhandle:0x%p (encodestream)\n",
            Configuration.ManifestorName, SrcHandle, SinkHandle);
    //
    // Take local references to the EncodeStream object we are connecting to.
    // These will be used by the Leaf Classes to Inject frames into the Encoder
    //
    EncodeStream = (EncodeStream_c *)SinkHandle;
    EncodeStream->GetEncoder(&Encoder);
    Encoder_BufferTypes = Encoder->GetBufferTypes();

    //
    // Now connect to EncoderStream port and ReleaseCallBack to Encoder
    //
    if (EncodeStream->Connect(this, &mEncodeStreamPort) != EncoderNoError)
    {
        SE_ERROR("%s - Failed to connect Encoder\n", Configuration.ManifestorName);
        EncodeStream = NULL;
        return ManifestorError;
    }

    //
    // Make sure InputPort is correctly created
    //
    if (mEncodeStreamPort == NULL)
    {
        SE_ERROR("%s - Failed to obtain Encoder Input port\n", Configuration.ManifestorName);
        EncodeStream->Disconnect();
        EncodeStream = NULL;
        return ManifestorError;
    }

    ManageEventSubscription(SrcHandle);

    // A memory barrier to ensure that the Connected Flag is not set before all of the above conditions
    OS_Smp_Mb();
    Connected   = true;

    //
    // We can begin accepting Input Data
    //
    SetComponentState(ComponentRunning);
    return ManifestorNoError;
}
//}}}

//{{{  Disconnect
//{{{  doxynote
/// \brief                      Disconnect from memory Sink port (if connected)
///                             Release pending pull call if any
/// \return                     Success or fail
//}}}
ManifestorStatus_t Manifestor_Encoder_c::Disconnect(stm_object_h  SinkObject)
{
    SE_INFO(GetGroupTrace(), "sinkobject:0x%p encodestream:0x%p\n", SinkObject, EncodeStream);

    //
    // Check that Sink object corresponds to the connected Stream
    //
    if (SinkObject != EncodeStream)
    {
        SE_ERROR("%s - invalid object\n", Configuration.ManifestorName);
        return ManifestorError;
    }

    /* Remove event subscription */
    if (EventSubscription)
    {
        SE_INFO(GetGroupTrace(), "%s EventSubscription\n", Configuration.ManifestorName);

        if (stm_event_subscription_delete(EventSubscription) < 0)
        {
            SE_ERROR("%s - Failed to delete Event Subscription for EOS event\n", Configuration.ManifestorName);
        }
        EventSubscription = NULL;
    }

    if (Connected == true)
    {
        // prevent queuing of buffer during disconnection
        OS_LockMutex(&ConnectBufferLock);

        // Close connection for memsink calls
        // and shut down buffer release thread
        SE_INFO(GetGroupTrace(), "%s\n", Configuration.ManifestorName);
        Connected = false;

        // release resources
        PurgeEncodeStream();

        // Now disconnect from EncoderStream port
        if (EncodeStream->Disconnect() != EncoderNoError)
        {
            SE_ERROR("%s - Failed to disconnect Encoder\n", Configuration.ManifestorName);
        }
        mEncodeStreamPort = NULL;

        OS_UnLockMutex(&ConnectBufferLock);
    }

    return ManifestorNoError;
}
//}}}

//{{{  Enable
//{{{  doxynote
/// \brief                      Set the Enabled Internal flag
///                             Required by the Manifestor Interface
/// \return                     Always no error
//}}}
ManifestorStatus_t Manifestor_Encoder_c::Enable(void)
{
    SE_INFO(GetGroupTrace(), "%s\n", Configuration.ManifestorName);
    Enabled = true;
    return ManifestorNoError;
}
//}}}

//{{{  Disable
//{{{  doxynote
/// \brief                      Set the Enabled Internal flag
///                             Required by the Manifestor Interface
/// \return                     Always no error
//}}}
ManifestorStatus_t Manifestor_Encoder_c::Disable(void)
{
    SE_INFO(GetGroupTrace(), "%s\n", Configuration.ManifestorName);
    Enabled = false;
    return ManifestorNoError;
}
//}}}

//{{{  GetEnable
//{{{  doxynote
/// \brief                      Get the Enabled Internal flag
///                             Required by the Manifestor Interface
/// \return                     Always no error
//}}}
bool Manifestor_Encoder_c::GetEnable(void)
{
    return Enabled;
}
//}}}

//{{{  GetNativeTimeOfCurrentlyManifestedFrame
//{{{  doxynote
/// \brief                      Get the Native Time of Current Manifested Frame
///                             Required by the Manifestor Interface
/// \return                     Always no error
//}}}
ManifestorStatus_t  Manifestor_Encoder_c::GetNativeTimeOfCurrentlyManifestedFrame(unsigned long long *Time)
{
    SE_DEBUG(GetGroupTrace(), "%lld\n", mLastEncodedPts);
    *Time = mLastEncodedPts;
    return ManifestorNoError;
}
//}}}

//{{{  ReleaseBuffer
//{{{  doxynote
/// \brief                      Callback function to release a previously pushed EncodeInputBuffer.
/// \param                      EncodeBuffer  : Encode input buffer to release
/// \return                     PlayerNoError: no error
///                             PlayerError  : fails to release the buffer
//}}}
PlayerStatus_t Manifestor_Encoder_c::ReleaseBuffer(Buffer_t EncodeBuffer)
{
    unsigned int           EncodeBufferIndex = 0;
    Buffer_t               DecodedBuffer;

    //
    // Check if there is a corresponding Decode buffer for this EncodeBuffer
    //
    EncodeBuffer->GetIndex(&EncodeBufferIndex);
    if (EncodeBufferIndex >= ENCODER_MAX_INPUT_BUFFERS)
    {
        SE_ERROR("Unable to obtain EncodeBuffer index - Implementation error\n");
        return PlayerError;
    }

    if (EncodeBufferArray[EncodeBufferIndex] == NULL)
    {
        SE_ERROR("Trying to release twice an Encode input buffer - Implementation error\n");
        return PlayerError;
    }

    DecodedBuffer = EncodeBufferArray[EncodeBufferIndex];
    EncodeBufferArray[EncodeBufferIndex] = NULL;

    //  Release InputBuffer
    EncodeBuffer->DecrementReferenceCount(IdentifierGetInjectBuffer);


    //  Check if it's not a Fake EOS buffer
    if (DecodedBuffer != FakeEndOfStreamBuffer)
    {
        // update Time to have GetPlayInfo function working properly
        ParsedFrameParameters_t  *FrameParameters;
        DecodedBuffer->ObtainMetaDataReference(Player->MetaDataParsedFrameParametersReferenceType, (void **)&FrameParameters);
        SE_ASSERT(FrameParameters != NULL);
        mLastEncodedPts                 = FrameParameters->NativePlaybackTime;
        // Release the corresponding DecodedBuffer
        mOutputPort->Insert((uintptr_t) DecodedBuffer);
    }

    return PlayerNoError;
}
//}}}

//{{{  PushEndOfStreamInputBuffer
//{{{  doxynote
/// \brief                      Create an Encode input buffer to notify the end of stream and push it to
///                             encoder. This is a specific buffer for which there is
///                             no decode buffer attached.
/// \return     ManifestorError  : fail to get buffer index or issue when inserting buffer to Encoder
/// \return     ManifestorNoError: otherwise
//}}}
ManifestorStatus_t Manifestor_Encoder_c::PushEndOfStreamInputBuffer(void)
{
    stm_se_uncompressed_frame_metadata_t     *Meta;
    Buffer_t                                  EncodeBuffer;
    unsigned int                              EncodeBufferIndex;
    EncoderStatus_t                           Status;

    SE_DEBUG(group_encoder_stream, "\n");

    // Get Encoder buffer
    Status = Encoder->GetInputBuffer(&EncodeBuffer);
    if ((Status != EncoderNoError) || (EncodeBuffer == NULL))
    {
        SE_ERROR("Failed to get Encoder Input Buffer\n");
        return ManifestorError;
    }

    EncodeBuffer->ObtainMetaDataReference(Encoder_BufferTypes->InputMetaDataBufferType, (void **)(&Meta));
    SE_ASSERT(Meta != NULL);

    // Set EOS discontinuity
    Meta->discontinuity = STM_SE_DISCONTINUITY_EOS;

    EncodeBuffer->GetIndex(&EncodeBufferIndex);
    if (EncodeBufferIndex >= ENCODER_MAX_INPUT_BUFFERS)
    {
        SE_ERROR("Unable to obtain EncodeBuffer index - Implementation error\n");
        EncodeBuffer->DecrementReferenceCount(IdentifierGetInjectBuffer);
        return ManifestorError;
    }

    // Use FakeEndOfStreamBuffer to notify that the Input Encode Buffer is
    // not attached to a real decode buffer but a temporary buffer for EOS discontinuity
    EncodeBufferArray[EncodeBufferIndex] = FakeEndOfStreamBuffer;

    // Push the Encode Input Buffer for EOS to Encoder subsystem.
    if (mEncodeStreamPort->Insert((uintptr_t) EncodeBuffer) != RingNoError)
    {
        SE_ERROR("Unable to send EOS buffer to EncodeStream\n");
        EncodeBufferArray[EncodeBufferIndex] = NULL;
        EncodeBuffer->DecrementReferenceCount(IdentifierGetInjectBuffer);
        return ManifestorError;
    }

    return ManifestorNoError;
}
//}}}

