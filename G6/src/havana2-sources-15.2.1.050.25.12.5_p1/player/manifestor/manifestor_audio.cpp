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

#include "manifestor_audio.h"
#include "player_generic.h"
#include "player_playback.h"
#include "player_stream.h"
#include "auto_lock_mutex.h"
#include "st_relayfs_se.h"

#undef TRACE_TAG
#define TRACE_TAG   "Manifestor_Audio_c"

////////////////////////////////////////////////////////////////////////////
///
/// constructor
///
Manifestor_Audio_c::Manifestor_Audio_c(void)
    : Manifestor_Base_c()
    , DisplayUpdatePending(false)
    , SurfaceDescriptor()
    , ProcessingBufRing(NULL)
    , PtsToDisplay(INVALID_TIME)
    , ForcedUnblock(false)
    , NumberOfAllocated_StreamBuf(0)
    , NumberOfFreed_StreamBuf(0)
    , QueuedBufRing(NULL)
    , BufferQueueLock()
    , BufferQueueUpdated()
    , BufferQueueTail(ANY_BUFFER_ID)
    , RelayfsIndex(0)
{
    if (InitializationStatus != ManifestorNoError)
    {
        SE_ERROR("Initialization status not valid - aborting init\n");
        return;
    }

    OS_InitializeMutex(&BufferQueueLock);
    OS_InitializeEvent(&BufferQueueUpdated);

    Configuration.ManifestorName = "Audio";
    Configuration.StreamType     = StreamTypeAudio;

    SurfaceDescriptor.StreamType = StreamTypeAudio;
    SurfaceDescriptor.ClockPullingAvailable = true;
    SurfaceDescriptor.PercussiveCapable = true;
    SurfaceDescriptor.MasterCapable = true;
    SurfaceDescriptor.BitsPerSample = 32;
    SurfaceDescriptor.ChannelCount = 8;
    SurfaceDescriptor.SampleRateHz = 0;
    SurfaceDescriptor.FrameRate = 1; // rational

    RelayfsIndex = st_relayfs_getindex_fortype_se(ST_RELAY_TYPE_DECODED_AUDIO_BUFFER);
}


////////////////////////////////////////////////////////////////////////////
///
///
///
Manifestor_Audio_c::~Manifestor_Audio_c(void)
{
    Manifestor_Audio_c::Halt();
    st_relayfs_freeindex_fortype_se(ST_RELAY_TYPE_DECODED_AUDIO_BUFFER, RelayfsIndex);

    OS_TerminateMutex(&BufferQueueLock);
    OS_TerminateEvent(&BufferQueueUpdated);
}

////////////////////////////////////////////////////////////////////////////
///
///
///
ManifestorStatus_t      Manifestor_Audio_c::Halt(void)
{
    SE_DEBUG(GetGroupTrace(), "\n");
    //
    // It is unlikely that we have any decode buffers queued at this point but if we do we
    // deperately need to jettison them (otherwise the reference these buffers hold to the
    // coded data can never be undone).
    //
    FreeQueuedBufRing();
    FreeProcessingBufRing();
    return Manifestor_Base_c::Halt();
}

////////////////////////////////////////////////////////////////////////////
///
///
///
ManifestorStatus_t Manifestor_Audio_c::Reset(void)
{
    SE_DEBUG(GetGroupTrace(), "\n");

    if (TestComponentState(ComponentRunning))
    {
        Halt();
    }

    BufferQueueTail             = ANY_BUFFER_ID;
    DisplayUpdatePending        = false;
    ForcedUnblock               = false;
    PtsToDisplay                = INVALID_TIME;

    if (SE_IS_DEBUG_ON(GetGroupTrace()))
    {
        SE_DEBUG(GetGroupTrace(), "NumberOfAllocated_StreamBuf=%06d, NumberOfFreed_StreamBuf=%06d delta:%d",
                 NumberOfAllocated_StreamBuf, NumberOfFreed_StreamBuf,
                 NumberOfAllocated_StreamBuf - NumberOfFreed_StreamBuf);
    }

    if ((NumberOfAllocated_StreamBuf - NumberOfFreed_StreamBuf) > 0)
    {
        SE_ERROR("Still %d allocated buffers!\n", NumberOfAllocated_StreamBuf - NumberOfFreed_StreamBuf);
    }

    NumberOfAllocated_StreamBuf = 0;
    NumberOfFreed_StreamBuf     = 0;

    FreeQueuedBufRing();
    FreeProcessingBufRing();

    return Manifestor_Base_c::Reset();
}

////////////////////////////////////////////////////////////////////////////
///
///
///
void Manifestor_Audio_c::FreeQueuedBufRing(void)
{
    SE_DEBUG(GetGroupTrace(), "\n");

    if (QueuedBufRing != NULL)
    {
        ReleaseQueuedDecodeBuffers();
        delete QueuedBufRing;
        QueuedBufRing = NULL;
    }
}

////////////////////////////////////////////////////////////////////////////
///
///
///
void Manifestor_Audio_c::FreeProcessingBufRing(void)
{
    SE_DEBUG(GetGroupTrace(), "\n");

    if (ProcessingBufRing != NULL)
    {
        ReleaseProcessingBuffers();
        delete ProcessingBufRing;
        ProcessingBufRing = NULL;
    }
}

////////////////////////////////////////////////////////////////////////////
///
///
///
void Manifestor_Audio_c::ReleaseBuf(AudioStreamBuffer_t *StreamBufPtr)
{
    SE_DEBUG(GetGroupTrace(), "StreamBufPtr:%p\n", StreamBufPtr);

    if (mOutputPort == NULL)
    {
        SE_ERROR("mOutputPort is NULL; cannot release StreamBufPtr:%p\n", StreamBufPtr);
        return;
    }

    mOutputPort->Insert((uintptr_t) StreamBufPtr->Buffer);
    OS_Free(StreamBufPtr);
    NumberOfFreed_StreamBuf++;

    if (SE_IS_DEBUG_ON(GetGroupTrace()))
    {
        SE_DEBUG(GetGroupTrace(), "NumberOfAllocated_StreamBuf=%06d, NumberOfFreed_StreamBuf=%06d delta:%d",
                 NumberOfAllocated_StreamBuf, NumberOfFreed_StreamBuf,
                 NumberOfAllocated_StreamBuf - NumberOfFreed_StreamBuf);
    }

    return;
}

////////////////////////////////////////////////////////////////////////////
///
///
///
void Manifestor_Audio_c::ExtractAndReleaseProcessingBuf(void)
{
    SE_DEBUG(GetGroupTrace(), "");
    AudioStreamBuffer_t *StreamBufPtr;

    if (ProcessingBufRing == NULL)
    {
        SE_ERROR("ProcessingBufRing is NULL; cannot extract Buffer\n");
        return;
    }

    if (ProcessingBufRing->Extract((uintptr_t *) &StreamBufPtr, RING_NONE_BLOCKING) != RingNoError)
    {
        SE_ERROR("Cannot extract Buffer from ProcessingBufRing");
        return;
    }
    SE_DEBUG(GetGroupTrace(), "Extracted  from ProcessingBufRing[%02d]", StreamBufPtr->BufferIndex);
    return ReleaseBuf(StreamBufPtr);
}

////////////////////////////////////////////////////////////////////////////
///
/// Connect the output port, and initialize our decode buffer associated context
///
/// \param Port         Port to connect to.
/// \return             Succes or failure
///
ManifestorStatus_t      Manifestor_Audio_c::Connect(Port_c *Port)
{
    ManifestorStatus_t          Status;
    BufferPool_t                Pool;
    unsigned int                MaxBufferCount;

    SE_DEBUG(GetGroupTrace(), "\n");
    // Get the number of buffers in the decode buffer pool.
    Status = Stream->GetDecodeBufferManager()->GetDecodeBufferPool(&Pool);

    if (Status != PlayerNoError)
    {
        return Status;
    }

    Pool->GetPoolUsage(&MaxBufferCount);

    FreeQueuedBufRing();
    FreeProcessingBufRing();

    SE_DEBUG(GetGroupTrace(), "new QueuedBufRing ProcessingBufRing:%d", MaxBufferCount);
    QueuedBufRing    = new RingGeneric_c(MaxBufferCount, "Manifestor_Audio_c::QueuedBufRing");
    ProcessingBufRing = new RingGeneric_c(MaxBufferCount, "Manifestor_Audio_c::ProcessingBufRing");

    BufferQueueTail                     = ANY_BUFFER_ID;
    // Connect to the port
    Status = Manifestor_Base_c::Connect(Port);

    if (Status != ManifestorNoError)
    {
        return Status;
    }

    // Let outside world know we are up and running
    SetComponentState(ComponentRunning);
    return ManifestorNoError;
}


////////////////////////////////////////////////////////////////////////////
/// Fill in private structure with timing details of display surface.
///
/// \param      SurfaceParameters pointer to structure to complete
///
ManifestorStatus_t      Manifestor_Audio_c::GetSurfaceParameters(OutputSurfaceDescriptor_t   **SurfaceParameters, unsigned int *NumSurfaces)
{
    *SurfaceParameters  = &SurfaceDescriptor;
    *NumSurfaces        = 1;
    return ManifestorNoError;
}


////////////////////////////////////////////////////////////////////////////
///
///
///
ManifestorStatus_t      Manifestor_Audio_c::GetNextQueuedManifestationTime(unsigned long long    *Time, unsigned int *NumTimes)
{
    SE_DEBUG(GetGroupTrace(), "\n");
    // this is not an appropriate implementation... it just assumes that audio latency is a quarter of
    // a second.
    *Time     = OS_GetTimeInMicroSeconds() + 250000;
    *NumTimes = 1;
    return ManifestorNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Release any buffers queued within the StreamBuffer structure.
///
/// Pass any buffer held on the Manifestor_Audio_c::StreamBuffer structure
/// onto the output port.
///
/// The super-class will tidy up an queued events.
///
ManifestorStatus_t      Manifestor_Audio_c::ReleaseQueuedDecodeBuffers()
{
    SE_DEBUG(GetGroupTrace(), "\n");

    OS_LockMutex(&BufferQueueLock);
    AudioStreamBuffer_t *StreamBufPtr;

    //
    // Emit all the queued buffers
    //

    if (QueuedBufRing == NULL)
    {
        SE_ERROR("QueuedBufRing is NULL; Cannot extract Buffer\n");
        OS_UnLockMutex(&BufferQueueLock);
        return ManifestorError;
    }
    while (QueuedBufRing->Extract((uintptr_t *) &StreamBufPtr, RING_NONE_BLOCKING) == RingNoError)
    {
        ReleaseBuffer(StreamBufPtr); // no status check

        if (StreamBufPtr->EventPending)
        {
            ServiceEventQueue(StreamBufPtr->BufferIndex);
        }

        ReleaseBuf(StreamBufPtr);
    }

    OS_UnLockMutex(&BufferQueueLock);

    WaitForProcessingBufRingRelease();

    BufferQueueTail = ANY_BUFFER_ID;


    return Manifestor_Base_c::ReleaseQueuedDecodeBuffers();
}



////////////////////////////////////////////////////////////////////////////
///
/// Release any non queued buffers within the StreamBuffer structure and
/// pass the buffer onto the output port.
///
/// CAUTION: this function has been designed to be called in case of
/// non recoverable error in the playback thread.
///
void Manifestor_Audio_c::ReleaseProcessingBuffers()
{
    SE_DEBUG(GetGroupTrace(), ">");
    OS_LockMutex(&BufferQueueLock);
    AudioStreamBuffer_t *StreamBufPtr;

    if (ProcessingBufRing == NULL)
    {
        SE_ERROR("ProcessingBufRing is NULL; Cannot extract Buffer\n");
        OS_UnLockMutex(&BufferQueueLock);
        return;
    }
    while (ProcessingBufRing->Extract((uintptr_t *) &StreamBufPtr, RING_NONE_BLOCKING) == RingNoError)
    {
        SE_DEBUG(GetGroupTrace(), "**** Released NotQueuedBuf:\n");

        if (StreamBufPtr->EventPending)
        {
            ServiceEventQueue(StreamBufPtr->BufferIndex);
        }
        ReleaseBuf(StreamBufPtr);
    }
    OS_UnLockMutex(&BufferQueueLock);
}

////////////////////////////////////////////////////////////////////////////
///
///
///
ManifestorStatus_t      Manifestor_Audio_c::QueueDecodeBuffer(class Buffer_c *Buffer,
                                                              ManifestationOutputTiming_t **TimingPointerArray,
                                                              unsigned int *NumTimes)
{
    ManifestorStatus_t                  Status;
    RingStatus_t                        RingStatus;
    unsigned int                        BufferIndex;
    ParsedFrameParameters_t            *ParsedFrameParameters;
    AudioStreamBuffer_t                *StreamBufPtr;
    SE_DEBUG(GetGroupTrace(), "\n");
    AssertComponentState(ComponentRunning);

    //
    // Obtain the index for the buffer and populate the parameter data.
    //
    Buffer->GetIndex(&BufferIndex);
    if (BufferIndex >= MAX_DECODE_BUFFERS)
    {
        SE_ERROR("invalid buffer index %d\n", BufferIndex);
        return ManifestorError;
    }

    StreamBufPtr = (AudioStreamBuffer_t *) OS_Malloc(sizeof(AudioStreamBuffer_t));
    if (!StreamBufPtr)
    {
        SE_FATAL("Cannot allocate AudioStreamBuffer_t");
        return ManifestorError;
    }

    NumberOfAllocated_StreamBuf++;
    if (SE_IS_DEBUG_ON(GetGroupTrace()))
    {
        SE_DEBUG(GetGroupTrace(), "NumberOfAllocated_StreamBuf=%06d, NumberOfFreed_StreamBuf=%06d delta:%d",
                 NumberOfAllocated_StreamBuf, NumberOfFreed_StreamBuf,
                 NumberOfAllocated_StreamBuf - NumberOfFreed_StreamBuf);
    }

    StreamBufPtr->Buffer = Buffer;
    StreamBufPtr->BufferIndex = BufferIndex;
    StreamBufPtr->EventPending = EventPending;
    EventPending = false;

    StreamBufPtr->BufferState = AudioBufferStateQueued;
    Buffer->ObtainMetaDataReference(Player->MetaDataParsedFrameParametersReferenceType,
                                    (void **) &StreamBufPtr->FrameParameters);
    SE_ASSERT(StreamBufPtr->FrameParameters != NULL);


    Buffer->ObtainMetaDataReference(Player->MetaDataParsedAudioParametersType,
                                    (void **) &StreamBufPtr->AudioParameters);
    SE_ASSERT(StreamBufPtr->FrameParameters != NULL);

    Buffer->ObtainMetaDataReference(Player->MetaDataParsedFrameParametersReferenceType,
                                    (void **)(&ParsedFrameParameters));
    SE_ASSERT(ParsedFrameParameters != NULL);

    if (*NumTimes > 1)
    {
        SE_ERROR("Implementation error - can't handle more than one OutputTiming\n");
    }

    StreamBufPtr->AudioOutputTiming = *TimingPointerArray;
    StreamBufPtr->QueueAsCodedData  = true;

    //Copying ADMetaData to AudioParameter struct
    if ((!StreamBufPtr->AudioParameters->MixingMetadata.IsMixingMetadataPresent) && (ParsedFrameParameters->ADMetaData.ADInfoAvailable))
    {
        memcpy(&(StreamBufPtr->AudioParameters->MixingMetadata.ADPESMetaData),
               &(ParsedFrameParameters->ADMetaData),
               sizeof(ADMetaData_t));
        StreamBufPtr->AudioParameters->MixingMetadata.IsMixingMetadataPresent = true;
    }

    //
    // Dump the entire buffer via relay (if enabled)
    //
    {
        unsigned char *Data = (unsigned char *)Stream->GetDecodeBufferManager()->ComponentBaseAddress(StreamBufPtr->Buffer,
                                                                                                      PrimaryManifestationComponent,
                                                                                                      CachedAddress);
        unsigned int  Len = StreamBufPtr->AudioParameters->SampleCount *
                            StreamBufPtr->AudioParameters->Source.ChannelCount *
                            (StreamBufPtr->AudioParameters->Source.BitsPerSample / 8);
        st_relayfs_write_se(ST_RELAY_TYPE_DECODED_AUDIO_BUFFER + RelayfsIndex, ST_RELAY_SOURCE_SE, Data, Len, 0);
    }
    //
    // Allow the sub-class to have a peek at the buffer before we queue it for display
    //
    Status = QueueBuffer(StreamBufPtr);
    if (Status != ManifestorNoError)
    {
        if (Status != ManifestorRenderComplete) // transcoding error status is not an error !
        {
            SE_ERROR("Unable to queue buffer %x\n", Status);
        }

        goto freeOnError;
    }

    //
    // Enqueue the buffer for display within the playback thread
    //
    OS_LockMutex(&BufferQueueLock);

    RingStatus = QueuedBufRing->Insert((uintptr_t) StreamBufPtr);
    if (RingStatus != RingNoError)
    {
        SE_ERROR("Failed to insert in QueuedBufRing (RingStatus:%d)!\n", RingStatus);
        OS_UnLockMutex(&BufferQueueLock);
        Status = ManifestorError;
        goto freeOnError;
    }

    OS_UnLockMutex(&BufferQueueLock);
    OS_SetEvent(&BufferQueueUpdated);
    BufferQueueTail = BufferIndex;
    SE_DEBUG(GetGroupTrace(), "Inserted in QueuedBufRing");
    return ManifestorNoError;

freeOnError:
    OS_Free(StreamBufPtr);
    NumberOfFreed_StreamBuf++;
    if (SE_IS_DEBUG_ON(GetGroupTrace()))
    {
        SE_DEBUG(GetGroupTrace(), "NumberOfAllocated_StreamBuf=%06d, NumberOfFreed_StreamBuf=%06d delta:%d",
                 NumberOfAllocated_StreamBuf, NumberOfFreed_StreamBuf,
                 NumberOfAllocated_StreamBuf - NumberOfFreed_StreamBuf);
    }
    return Status;
}


////////////////////////////////////////////////////////////////////////////
///
/// Retrieve buffer index of last buffer queued.
///
/// If no buffers yet queued returns ANY_BUFFER_ID
///
/// \return     Buffer index of last buffer sent for display
///
unsigned int    Manifestor_Audio_c::GetBufferId(void)
{
    return QueuedBufRing->NonEmpty() ? BufferQueueTail : ANY_BUFFER_ID; //TODO Use QueuedBufRing
}

////////////////////////////////////////////////////////////////////////////
///
/// Get original PTS of currently visible buffer
///
/// \param Pts          Pointer to Pts variable
///
/// \return             Success if Pts value is available
///
ManifestorStatus_t Manifestor_Audio_c::GetNativeTimeOfCurrentlyManifestedFrame(unsigned long long *Time)
{
    struct ParsedFrameParameters_s     *FrameParameters;
    *Time      = INVALID_TIME;
    AudioStreamBuffer_t *StreamBufPtr;
    AutoLockMutex AutoLock(&BufferQueueLock);

    if (QueuedBufRing->NonEmpty() == false)
    {
        SE_DEBUG(GetGroupTrace(),  "No buffer on display\n");

        if (NotValidTime(PtsToDisplay))
        {
            SE_DEBUG(GetGroupTrace(),  "No previous buffer displayed too\n");
            return ManifestorError;
        }

        // We may be in pause and we had a previous buffer displayed
        // So this is the PTS of the frame to be displayed when we resume
        *Time = PtsToDisplay;
        return ManifestorNoError;
    }

    if (QueuedBufRing->Peek((uintptr_t *) &StreamBufPtr) != RingNoError)
    {
        SE_ERROR("Unable to extract Buffer from QueuedBufRing");
        return ManifestorError;
    }

    StreamBufPtr->Buffer->ObtainMetaDataReference(
        Player->MetaDataParsedFrameParametersReferenceType, (void **)&FrameParameters);
    SE_ASSERT(FrameParameters != NULL);

    if (NotValidTime(FrameParameters->NativePlaybackTime))
    {
        SE_ERROR("Buffer on display does not have a valid native playback time\n");
        return ManifestorError;
    }

    SE_DEBUG(GetGroupTrace(), "%lld\n", FrameParameters->NativePlaybackTime);
    *Time = FrameParameters->NativePlaybackTime;
    return ManifestorNoError;
}

void Manifestor_Audio_c::LockBuffferQueue()
{
    SE_EXTRAVERB(GetGroupTrace(), ">\n");
    OS_LockMutex(&BufferQueueLock);
    SE_EXTRAVERB(GetGroupTrace(), "<\n");
}

void Manifestor_Audio_c::UnLockBuffferQueue()
{
    SE_EXTRAVERB(GetGroupTrace(), ">\n");
    OS_UnLockMutex(&BufferQueueLock);
    SE_EXTRAVERB(GetGroupTrace(), "<\n");
}

////////////////////////////////////////////////////////////////////////////
///
/// Dequeue a buffer from the queue of buffers ready for presentation and
/// release it straightaway because it presentation time is in the past
///
/// \return Manifestor status code, ManifestorNoError indicates success.
///
ManifestorStatus_t Manifestor_Audio_c::DropNextQueuedBufferUnderLock(void)
{
    RingStatus_t ringStatus;
    SE_DEBUG(GetGroupTrace(), ">");
    //
    // Dequeue the buffer
    //
    SE_ASSERT(OS_MutexIsLocked(&BufferQueueLock));

    if (QueuedBufRing == NULL)
    {
        SE_ERROR("QueuedBufRing(%p) NULL; Cannot Deuqueue Buffer\n", QueuedBufRing);
        return ManifestorError;
    }

    AudioStreamBuffer_t *StreamBufPtr;
    if ((ringStatus = QueuedBufRing->Extract((uintptr_t *) &StreamBufPtr, RING_NONE_BLOCKING)) != RingNoError)
    {
        SE_INFO(GetGroupTrace(), "Failed to extract Buffer from QueuedBufRing (RingStatus:%d)", ringStatus);
        return ManifestorWouldBlock;
    }
    else
    {
        ReleaseBuf(StreamBufPtr);
    }

    return ManifestorNoError;
}


////////////////////////////////////////////////////////////////////////////
///
/// Dequeue a buffer from the queue of buffers ready for presentation.
///
/// Must be called only from the mixer playback thread with Manifestor Locked (see Lock/UnLockBuffferQueue)
///
/// \param Pointer to AudioStreamBuffer_t pointer that will be filled with extracted buffer pointer.
/// \return Manifestor status code, ManifestorNoError indicates success.
///
ManifestorStatus_t Manifestor_Audio_c::DequeueBufferUnderLock(AudioStreamBuffer_t **StreamBufPtr)
{
    RingStatus_t ringStatus;
    SE_DEBUG(GetGroupTrace(), ">");
    //
    // Dequeue the buffer
    //
    SE_ASSERT(OS_MutexIsLocked(&BufferQueueLock));

    if (QueuedBufRing == NULL || ProcessingBufRing == NULL)
    {
        SE_ERROR("QueuedBufRing(%p) or ProcessingBufRing(%p) NULL; Cannot Deuqueue Buffer\n",
                 QueuedBufRing, ProcessingBufRing);
        return ManifestorError;
    }
    if ((ringStatus = QueuedBufRing->Extract((uintptr_t *) StreamBufPtr, RING_NONE_BLOCKING)) != RingNoError)
    {
        SE_INFO(GetGroupTrace(), "Failed to extract Buffer from QueuedBufRing (RingStatus:%d)", ringStatus);
        return ManifestorWouldBlock;
    }

    if ((ringStatus = ProcessingBufRing->Insert((uintptr_t) * StreamBufPtr)) != RingNoError)
    {
        SE_ERROR("Failed to insert Buffer into ProcessingBufRing (RingStatus:%d)", ringStatus);
        return ManifestorError;
    }
    FrameCountManifested++;

    return ManifestorNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Peek a buffer from QueuedBufRing (buffers ready for presentation).
///
/// Must be called only from the mixer playback thread with Manifestor Locked (see Lock/UnLockBuffferQueue)
///
/// \param Pointer to AudioStreamBuffer_t pointer. Valid as long as Manifestor is locked.
/// \return RingStatus_t status code
///
RingStatus_t Manifestor_Audio_c::PeekBufferUnderLock(AudioStreamBuffer_t **StreamBufPtr)
{
    SE_ASSERT(OS_MutexIsLocked(&BufferQueueLock));

    return QueuedBufRing->Peek((uintptr_t *) StreamBufPtr);
}

