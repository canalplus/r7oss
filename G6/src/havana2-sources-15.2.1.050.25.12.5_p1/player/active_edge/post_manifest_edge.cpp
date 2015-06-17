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

/************************************************************************
State machine of the retiming process (video stream only)
---------------------------------------------------------

In the post manifest thread the retiming process will take in charge frame previously pushed to the display but flushed due to a change of the speed.
The retiming process consists to determine if these frames need to be dropped, re-compute the presentation time and push them to the display

The retiming is trigged when speed is changed. in this case:
=> mReTimeStart      = OS_GetTimeInMicroSeconds();
=> mReTimeQueuedFrames=true;

Please note that during the retiming process, the decode_to_manifest thread is waiting while (mReTimeQueuedFrames) before it can push another frame.

To slect frame to be retimed, a new field (SequenceNumberStructure->ManifestedState) has been added:
  ManifestedState =   InitialState     // not manifested yet
                      QueuedState,     // Frame is pushed to display
                      RenderedState,   // Frame come back from display and was rendered
                      RetimeState      // Frame come back from display but was not rendered and will be queued again for display
The Retiming state machine get 3 states :
[Initial]
    - no retiming on going
[retiming asked]
    - Retiming asked but no frame to be retimed yet discovered
[Retiming on-going]
    - At least one frame to be retimed seen, that is a frame with  (SequenceNumberStructure->ManifestedState == QueuedState)


Transitions
-----------
[Initial]  -> [retiming asked]
    - if (mReTimeQueuedFrames)
        => no action

[<any State>] -> [Initial]
    -   if( (RingStatus == RingNothingToGet) && mReTimeQueuedFrames && ((Now - mReTimeStart) > PLAYER_MAX_TIME_IN_RETIMING) )
        => mReTimeQueuedFrames   = false; // Timeout happens

[retiming asked] -> [Retiming on-going]
    - if (mReTimeQueuedFrames) &&(SequenceNumberStructure->ManifestedState == QueuedState)
        => AtLeastOneFrameToRetime = true;
        => Frame retimed and pushed to ManifestorCoordinator is not dropped
        => mReTimeStart= now ; // restart timeout

[Retiming on-going] state
    - if (AtLeastOneFrameToRetime && mReTimeQueuedFrames) &&(SequenceNumberStructure->ManifestedState == QueuedState)
        => Frame retimed and pushed to ManifestorCoordinator is not dropped
        => mReTimeStart= now ; // restart timeout

[Retiming on-going] -> [initial]
    - if (AtLeastOneFrameToRetime && mReTimeQueuedFrames) &&(SequenceNumberStructure->ManifestedState == RenderedState)
        => mReTimeStart= now ; // restart timeout
        => mReTimeQueuedFrames = false

************************************************************************/

#include "player_generic.h"
#include "player_playback.h"
#include "player_stream.h"
#include "post_manifest_edge.h"
#include "collate_to_parse_edge.h"
#include "parse_to_decode_edge.h"
#include "decode_to_manifest_edge.h"

#undef TRACE_TAG
#define TRACE_TAG "PostManifestEdge_c"

void PostManifestEdge_c::HandleMarkerFrame(Buffer_t Buffer, PlayerSequenceNumber_t *SequenceNumberStructure)
{
    SE_DEBUG(group_player, "Stream 0x%p Got Marker Frame #%llu\n", mStream, SequenceNumberStructure->Value);

    mStream->Statistics().FrameCountFromManifestor++;
    mStream->FramesFromManifestorCount++;

    mSequenceNumber                  = SequenceNumberStructure->Value;
    mMaximumActualSequenceNumberSeen = max(mSequenceNumber, mMaximumActualSequenceNumberSeen);
    mDiscardingUntilMarkerFrame      = false;

    ProcessAccumulatedBeforeControlMessages(mSequenceNumber, INVALID_TIME);

    Buffer->DecrementReferenceCount(IdentifierProcessPostManifest);

    ProcessAccumulatedAfterControlMessages(mSequenceNumber, INVALID_TIME);
}

void PostManifestEdge_c::HandleCodedFrameBuffer(
    Buffer_t Buffer,
    unsigned long long Now,
    PlayerSequenceNumber_t *SequenceNumberStructure,
    ParsedFrameParameters_t  *ParsedFrameParameters)
{

    mStream->Statistics().FrameCountFromManifestor++;
    mStream->FramesFromManifestorCount++;

    if (mLastOutputTimeValid == false)
    {
        // confirm to the middleware that display has been (re) started
        PlayerEventRecord_t Event;
        Event.Code           = EventFrameSupplied;
        Event.Playback       = mStream->GetPlayback();
        Event.Stream         = mStream;
        mStream->SignalEvent(&Event);
    }

    mLastOutputTimeValid = true; // re-enable starvation report as we get something displayed

    OutputTiming_t  *OutputTiming;
    Buffer->ObtainMetaDataReference(mStream->GetPlayer()->MetaDataOutputTimingType, (void **)&OutputTiming);
    SE_ASSERT(OutputTiming != NULL);

    //
    // Check for whether or not we are in re-timing
    //
    if (!mDiscardingUntilMarkerFrame)
    {
        if (mReTimeQueuedFrames
            && mAtLeastOneFrameToReTime
            && (SequenceNumberStructure->ManifestedState == RenderedState))
        {
            mReTimeQueuedFrames = false;
            mAtLeastOneFrameToReTime = false;
        }

        // Only perform a retime for frames that were not rendered by display, only queued for display
        if (mReTimeQueuedFrames && SequenceNumberStructure->ManifestedState == QueuedState)
        {
            mAtLeastOneFrameToReTime = true;
            // Rearm the timeout for retime on any new frame found in the post manifested ring
            // We want all the frames to be retimed and queue to display, not exiting on time out if the ring is not empty
            mReTimeStart = Now;
            SequenceNumberStructure->ManifestedState = RetimeState;


            //
            // Exit re-timing if we have seen a frame actually get on display, or if there are no displays attached
            //

            if (mReTimeQueuedFrames)
            {
                OutputTiming->RetimeBuffer  = true;
                mStream->GetOutputTimer()->GenerateFrameTiming(Buffer);
                mStream->GetOutputTimer()->FillOutManifestationTimings(Buffer);

                OutputTimerStatus_t Status  = mStream->GetOutputTimer()->TestForFrameDrop(Buffer, OutputTimerBeforeManifestation);

                if (!mStream->IsTerminating() && (Status == OutputTimerNoError))
                {
                    unsigned int BufferIndex;
                    Buffer->GetIndex(&BufferIndex);
                    mStream->Statistics().FrameCountFromManifestor++;
                    mStream->FramesToManifestorCount++;
                    SE_VERBOSE2(group_player, group_avsync,
                                "Stream 0x%p - %d - Retiming & Re-Queueing frame #%lld buffer #%d (DecodeFrameIndex %d DisplayFrameIndex %d BaseSystemPlaybackTime %lld deltaNow %lld)\n",
                                mStream, mStream->GetStreamType(), SequenceNumberStructure->Value, BufferIndex, ParsedFrameParameters->DecodeFrameIndex,
                                ParsedFrameParameters->DisplayFrameIndex, OutputTiming->BaseSystemPlaybackTime,
                                OutputTiming->BaseSystemPlaybackTime - OS_GetTimeInMicroSeconds());
                    mStream->GetManifestationCoordinator()->QueueDecodeBuffer(Buffer);
                    // We can reset the step flag once the frame has been pushed to display. If we were not in speed 0, step was already false.
                    mStream->mStep = false;
                    return;
                }
            }
        }
    }

    //
    // Extract the sequence number, and write the timing statistics
    //
    SE_EXTRAVERB(group_player, "Stream 0x%p FrameCountManifested: %d\n", mStream, ParsedFrameParameters->DisplayFrameIndex + 1);

    SequenceNumberStructure->TimeEntryInProcess3    = OS_GetTimeInMicroSeconds();
    mSequenceNumber                                 = SequenceNumberStructure->Value;
    mMaximumActualSequenceNumberSeen                = max(mSequenceNumber, mMaximumActualSequenceNumberSeen);
    mTime                                           = ParsedFrameParameters->NativePlaybackTime;

    //
    // Process any outstanding control messages to be applied before this buffer
    //
    ProcessAccumulatedBeforeControlMessages(mSequenceNumber, mTime);

    long long int ActualTime = (&OutputTiming->ManifestationTimings[0])->ActualSystemPlaybackTime;

    // SE-PIPELINE TRACES
    if (ValidTime(ActualTime))
    {
        SE_VERBOSE(group_player, "Stream 0x%p - %d - #%lld PTS=%lld PM=%llu\n",
                   mStream,
                   mStream->GetStreamType(),
                   SequenceNumberStructure->CollatedFrameCounter,
                   mTime,
                   ActualTime);
    }
    else
    {
        SE_VERBOSE(group_player, "Stream 0x%p - %d - #%lld PTS=%lld PM=INVALID_TIME\n",
                   mStream,
                   mStream->GetStreamType(),
                   SequenceNumberStructure->CollatedFrameCounter,
                   mTime);
    }
    SE_VERBOSE(group_player, "Stream 0x%p - %d - #%lld PTS=%lld EXP=%llu\n",
               mStream,
               mStream->GetStreamType(),
               SequenceNumberStructure->CollatedFrameCounter,
               SequenceNumberStructure->PTS,
               OutputTiming->BaseSystemPlaybackTime
              );
    //
    // Pass buffer back into output timer
    // and release the buffer.
    //

    if (!mStream->IsUnPlayable())
    {
        mStream->GetOutputTimer()->RecordActualFrameTiming(Buffer);
    }

    bool IsCodecSwitchingCopy; // Store value before barrier to avoid reordeing with following test
    IsCodecSwitchingCopy = mStream->IsCodecSwitching();
    OS_Smp_Mb();//Read memory barrier: rmb_for_Stream_IsCodecSwitching coupled with: wmb_for_Stream_IsCodecSwitching

    if (!IsCodecSwitchingCopy)
    {
        mStream->GetCodec()->ReleaseDecodeBuffer(Buffer);
    }
    else
    {
        Buffer->DecrementReferenceCount(IdentifierProcessPostManifest);
    }

    //
    // Process any outstanding control messages to be applied after this buffer
    //
    ProcessAccumulatedAfterControlMessages(mSequenceNumber, mTime);
}

void PostManifestEdge_c::HandleCodedFrameBufferType(Buffer_t Buffer, unsigned long long Now)
{
    Buffer_t OriginalCodedFrameBuffer;
    Buffer->ObtainAttachedBufferReference(mStream->GetCodedFrameBufferType(), &OriginalCodedFrameBuffer);
    SE_ASSERT(OriginalCodedFrameBuffer != NULL);

    PlayerSequenceNumber_t   *SequenceNumberStructure;
    OriginalCodedFrameBuffer->ObtainMetaDataReference(mStream->GetPlayer()->MetaDataSequenceNumberType, (void **)(&SequenceNumberStructure));
    SE_ASSERT(SequenceNumberStructure != NULL);

    ParsedFrameParameters_t  *ParsedFrameParameters;
    Buffer->ObtainMetaDataReference(mStream->GetPlayer()->MetaDataParsedFrameParametersReferenceType, (void **)(&ParsedFrameParameters));
    SE_ASSERT(ParsedFrameParameters != NULL);

    if (SequenceNumberStructure->MarkerFrame)
    {
        HandleMarkerFrame(Buffer, SequenceNumberStructure);
    }
    else
    {
        HandleCodedFrameBuffer(Buffer, Now, SequenceNumberStructure, ParsedFrameParameters);
    }
}

void   PostManifestEdge_c::MainLoop()
{
    RingStatus_t              RingStatus;
    Buffer_t                  Buffer;
    BufferType_t              BufferType;
    unsigned long long        Now;

    //
    // Signal we have started
    //
    OS_LockMutex(&mStream->StartStopLock);
    mStream->ProcessRunningCount++;
    OS_SetEvent(&mStream->StartStopEvent);
    OS_UnLockMutex(&mStream->StartStopLock);

    SE_DEBUG(group_player, "process starting Stream 0x%p\n", mStream);

    //
    // Main Loop
    //
    mStream->EnterManifestorSharedSection();
    while (!mStream->IsTerminating())
    {
        mStream->ExitManifestorSharedSection();
        RingStatus  = mInputRing->Extract((uintptr_t *)(&Buffer), PLAYER_MAX_EVENT_WAIT);
        mStream->EnterManifestorSharedSection();

        Now = OS_GetTimeInMicroSeconds();

        // The time out is needed to avoid waiting endlessly for a picture in case of speed 0.
        // It is also needed for speed changes while nothing is injected.
        if (mReTimeQueuedFrames
            && (RingStatus == RingNothingToGet)
            && (((Now - mReTimeStart) > PLAYER_MAX_TIME_IN_RETIMING)))
        {
            mReTimeQueuedFrames  = false;
            mAtLeastOneFrameToReTime = false;
        }

        if ((RingStatus == RingNothingToGet) || (Buffer == NULL))
        {
            if (mStream->GetPlayback()->mSpeed != 0)
            {
                if (mLastOutputTimeValid)
                {
                    if ((mStream->FramesToManifestorCount > 0)
                        && !(mStream->CollateToParseEdge->GetInputPort()->NonEmpty())
                        && !(mStream->ParseToDecodeEdge->GetInputPort()->NonEmpty())
                        && !(mStream->DecodeToManifestEdge->GetInputPort()->NonEmpty()))
                    {
                        PlayerEventRecord_t Event;
                        Event.Code           = EventFrameStarvation;
                        Event.Playback       = mStream->GetPlayback();
                        Event.Stream         = mStream;
                        mStream->SignalEvent(&Event);
                        mLastOutputTimeValid = false; // starvation is reported once but might be re-enabled only if we displayed something new
                    }
                }
            }
            else
            {
                mLastOutputTimeValid = false; // pausing is not a manifestation starvation
            }

            continue;
        }

        mStream->BuffersComingOutOfManifestation = true;
        Buffer->GetType(&BufferType);
        Buffer->TransferOwnership(IdentifierProcessPostManifest);

        //
        // Deal with a coded frame buffer
        //

        if (BufferType == mStream->DecodeBufferType)
        {
            HandleCodedFrameBufferType(Buffer, Now);
        }
        //
        // Deal with a player control structure
        //
        else if (BufferType == mStream->GetPlayer()->BufferPlayerControlStructureType)
        {
            HandlePlayerControlStructure(Buffer, mSequenceNumber, mTime, mMaximumActualSequenceNumberSeen);
        }
        else
        {
            SE_ERROR("Unknown buffer type received - Implementation error\n");
            Buffer->DecrementReferenceCount();
        }
    }
    mStream->ExitManifestorSharedSection();

    SE_DEBUG(group_player, "process terminating Stream 0x%p\n", mStream);

    // At this point, Stream->IsTerminating() must be true
    OS_Smp_Mb(); // Read memory barrier: rmb_for_Stream_Terminating coupled with: wmb_for_Stream_Terminating

    //
    // Signal we have terminated
    //
    OS_LockMutex(&mStream->StartStopLock);
    mStream->ProcessRunningCount--;
    OS_SetEvent(&mStream->StartStopEvent);
    OS_UnLockMutex(&mStream->StartStopLock);
}

PlayerStatus_t   PostManifestEdge_c::CallInSequence(
    PlayerSequenceType_t      SequenceType,
    PlayerSequenceValue_t     SequenceValue,
    PlayerComponentFunction_t Fn,
    ...)
{
    va_list                   List;
    Buffer_t                  ControlStructureBuffer;
    PlayerControlStructure_t *ControlStructure;

    BufferStatus_t BufferStatus = mStream->GetPlayer()->GetControlStructurePool()->GetBuffer(&ControlStructureBuffer, IdentifierInSequenceCall);
    if (BufferStatus != BufferNoError)
    {
        SE_ERROR("Failed to get a control structure buffer\n");
        return PlayerError;
    }

    ControlStructureBuffer->ObtainDataReference(NULL, NULL, (void **)(&ControlStructure));
    SE_ASSERT(ControlStructure != NULL); // not expected to be empty
    ControlStructure->Action            = ActionInSequenceCall;
    ControlStructure->SequenceType      = SequenceType;
    ControlStructure->SequenceValue     = SequenceValue;
    ControlStructure->InSequence.Fn     = Fn;

    switch (Fn)
    {
    case OSFnSetEventOnPostManifestation:
        va_start(List, Fn);
        ControlStructure->InSequence.Pointer    = (void *)va_arg(List, OS_Event_t *);
        va_end(List);
        break;

    case PlayerFnSwitchComplete:
        break;

    default:
        SE_ERROR("Unsupported function call\n");
        ControlStructureBuffer->DecrementReferenceCount(IdentifierInSequenceCall);
        return PlayerNotSupported;
    }

    RingStatus_t ringStatus = mInputRing->Insert((uintptr_t)ControlStructureBuffer);
    if (ringStatus != RingNoError) { return PlayerError; }

    return PlayerNoError;
}

PlayerStatus_t   PostManifestEdge_c::PerformInSequenceCall(PlayerControlStructure_t *ControlStructure)
{
    PlayerStatus_t  Status = PlayerNoError;

    switch (ControlStructure->InSequence.Fn)
    {
    case OSFnSetEventOnPostManifestation:
        OS_SetEvent((OS_Event_t *)ControlStructure->InSequence.Pointer);
        break;

    case PlayerFnSwitchComplete:
        SwitchComplete();
        break;

    default:
        SE_ERROR("Unsupported function call - Implementation error\n");
        Status  = PlayerNotSupported;
        break;
    }

    return Status;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Switch stream component function to recognize the switch is complete
//

void    PostManifestEdge_c::SwitchComplete()
{
    PlayerStatus_t    Status;
    PlayerEventRecord_t   Event;

    //
    // Wait for all stream components to be ready
    //
    OS_SemaphoreWaitAuto(&mStream->mSemaphoreStreamSwitchCollator);
    OS_SemaphoreWaitAuto(&mStream->mSemaphoreStreamSwitchFrameParser);
    OS_SemaphoreWaitAuto(&mStream->mSemaphoreStreamSwitchCodec);
    OS_SemaphoreWaitAuto(&mStream->mSemaphoreStreamSwitchOutputTimer);

    //
    // We have finished stream switch, so reset flag to allow
    // another switch request to be executed
    //
    mStream->SwitchStreamInProgress = false;

    //
    // Signal event that we've finished Stream Switch() request
    //
    Event.Code              = EventStreamSwitched;
    Event.Playback          = mStream->GetPlayback();
    Event.Stream            = mStream;
    Event.PlaybackTime      = TIME_NOT_APPLICABLE;
    Event.UserData          = NULL;

    Status = mStream->SignalEvent(&Event);
    if (Status != PlayerNoError)
    {
        SE_ERROR("Stream 0x%p Failed to issue event signal of stream switch complete\n", mStream);
    }

    OS_SemaphoreSignal(&mStream->mSemaphoreStreamSwitchComplete);
    SE_DEBUG(group_player, "Stream 0x%p completed\n", mStream);
}
