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

#include "havana_user_data.h"
#include "player_generic.h"
#include "player_playback.h"
#include "player_stream.h"
#include "decode_to_manifest_edge.h"
#include "post_manifest_edge.h"

#define MAXIMUM_MANIFESTATION_WINDOW_SLEEP      2000000 // 2s
#define MINIMUM_SLEEP_TIME_US                   10000  // 10ms
#define PTS_DISCONTINUITY_DETECTION_US          500000  // 0.5 second for gap detection

#undef TRACE_TAG
#define TRACE_TAG "DecodeToManifestEdge_c"

void DecodeToManifestEdge_c::CheckPtsDiscontinuity(unsigned long long CurNormPlayBackTime)
{

    // Apply Discontinuity action for video stream only and in normal speed
    if ((mStream->GetStreamType() != StreamTypeVideo) || (mStream->GetPlayback()->mSpeed != 1))
    {
        return;
    }

    if (mPreviousPlayBackTime == INVALID_TIME)
    {
        // Nothing to do more this time
        mPreviousPlayBackTime = CurNormPlayBackTime;
        return;
    }

    if (!inrange((long long)(CurNormPlayBackTime - mPreviousPlayBackTime), -PTS_DISCONTINUITY_DETECTION_US, PTS_DISCONTINUITY_DETECTION_US)
        && (mAccumulatedDecodeBufferTableOccupancy != 0))
    {
        SE_WARNING("Stream 0x%p PTS too high compared to last PTS, new PTS=%llu last PTS=%llu Diff=%llu us\n",
                   mStream, CurNormPlayBackTime, mPreviousPlayBackTime, CurNormPlayBackTime - mPreviousPlayBackTime);
        // Reset the reordering table
        FlushAccumulatedBuffers(&CurNormPlayBackTime);
    }

    mPreviousPlayBackTime = CurNormPlayBackTime ;
}

void DecodeToManifestEdge_c::FlushAccumulatedBuffers(unsigned long long *pCurNormPlayBackTime)
{

    if (mAccumulatedDecodeBufferTableOccupancy == 0) { return; }

    for (unsigned int i = 0; i < mStream->GetNumberOfDecodeBuffers(); i++)
    {
        if (mAccumulatedDecodeBufferTable[i].Buffer == NULL)
        { continue; }

        if ((pCurNormPlayBackTime != NULL) &&
            (inrange((long long)(*pCurNormPlayBackTime - mAccumulatedDecodeBufferTable[i].ParsedFrameParameters->NormalizedPlaybackTime),
                     -PTS_DISCONTINUITY_DETECTION_US, PTS_DISCONTINUITY_DETECTION_US)))
        { continue; }

        SE_DEBUG(group_player, "Releasing buffer with PTS %llu", mAccumulatedDecodeBufferTable[i].ParsedFrameParameters->NormalizedPlaybackTime);
        ReleaseDecodeBuffer(mAccumulatedDecodeBufferTable[i].Buffer);

        mAccumulatedDecodeBufferTable[i].Buffer        = NULL;
        mAccumulatedDecodeBufferTable[i].ParsedFrameParameters = NULL;
        mAccumulatedDecodeBufferTableOccupancy--;
    }
}

unsigned int DecodeToManifestEdge_c::GetMaxDecodesOutOfOrder()
{
    unsigned int    PossibleDecodeBuffers;

    mStream->GetDecodeBufferManager()->GetEstimatedBufferCount(&PossibleDecodeBuffers);
    // Set depth of reordering queue to #of available buffers minus absolute minimum number of working coded buffers free in a playback
    unsigned int MaxDecodesOutOfOrder   = PossibleDecodeBuffers - PLAYER_MINIMUM_NUMBER_OF_WORKING_DECODE_BUFFERS;

    if (mStream->GetPlayback()->mDirection == PlayForward)
    {
        MaxDecodesOutOfOrder  = min(PLAYER_LIMIT_ON_OUT_OF_ORDER_DECODES, MaxDecodesOutOfOrder);
    }
    else
    {
        MaxDecodesOutOfOrder  = min(((3 * PossibleDecodeBuffers) / 4), MaxDecodesOutOfOrder);
    }

    return MaxDecodesOutOfOrder;
}

Buffer_t DecodeToManifestEdge_c::SearchNextDisplayOrderBufferInAccumulatedBuffers()
{
    mMinimumSequenceNumberAccumulated = 0xffffffffffffffffULL;

    if (mAccumulatedDecodeBufferTableOccupancy == 0) { return NULL; }

    unsigned int LowestIndex                = INVALID_INDEX;
    unsigned int LowestDisplayFrameIndex    = INVALID_INDEX;

    for (unsigned int i = 0; i < mStream->GetNumberOfDecodeBuffers(); i++)
    {
        if (mAccumulatedDecodeBufferTable[i].Buffer != NULL)
        {
            mMinimumSequenceNumberAccumulated = min(mMinimumSequenceNumberAccumulated, GetSequenceNumberStructure(mAccumulatedDecodeBufferTable[i].Buffer)->Value);

            if ((mAccumulatedDecodeBufferTable[i].ParsedFrameParameters->DisplayFrameIndex != INVALID_INDEX)
                && ((LowestIndex == INVALID_INDEX) || (mAccumulatedDecodeBufferTable[i].ParsedFrameParameters->DisplayFrameIndex < LowestDisplayFrameIndex)))
            {
                LowestDisplayFrameIndex     = mAccumulatedDecodeBufferTable[i].ParsedFrameParameters->DisplayFrameIndex;
                LowestIndex                 = i;
            }
        }
    }

    if (LowestIndex != INVALID_INDEX)
    {
        if (LowestDisplayFrameIndex == mDesiredFrameIndex)
        {
            return ExtractFromAccumulatedBuffers(LowestIndex);
        }
        else if (LowestDisplayFrameIndex < mDesiredFrameIndex)
        {
            SE_ERROR("Stream 0x%p: Frame re-ordering failure (Got %d Expected %d)\n",
                     mStream, LowestDisplayFrameIndex, mDesiredFrameIndex);
            return ExtractFromAccumulatedBuffers(LowestIndex);
        }
        else if (mAccumulatedDecodeBufferTable[LowestIndex].ParsedFrameParameters->CollapseHolesInDisplayIndices)
        {
            mDesiredFrameIndex = LowestDisplayFrameIndex ;
            return ExtractFromAccumulatedBuffers(LowestIndex);
        }
        else if (mAccumulatedDecodeBufferTableOccupancy >= GetMaxDecodesOutOfOrder())
        {
            SE_ERROR("Stream 0x%p: Hole in display frame indices (Got %d Expected %d)\n",
                     mStream, LowestDisplayFrameIndex, mDesiredFrameIndex);
            SE_INFO(group_player, "Increasing the buffer number may solve this issue (i.e. choose another memory profile)\n");
            return ExtractFromAccumulatedBuffers(LowestIndex);
        }
    }

    return NULL;
}

void DecodeToManifestEdge_c::InsertInAccumulatedBuffers(Buffer_t Buffer)
{
    ParsedFrameParameters_t *ParsedFrameParameters   = GetParsedFrameParameters(Buffer);

    SE_VERBOSE(group_player, "Stream 0x%p StreamType=%d SequenceNumber=%llu #%llu PTS=%llu DisplayFrameIndex=%u %s\n"
               , mStream
               , mStream->GetStreamType()
               , GetSequenceNumberStructure(Buffer)->Value
               , GetSequenceNumberStructure(Buffer)->CollatedFrameCounter
               , GetSequenceNumberStructure(Buffer)->PTS
               , ParsedFrameParameters->DisplayFrameIndex
               , ParsedFrameParameters->KeyFrame ? "I" : ParsedFrameParameters->ReferenceFrame ? "P" : "B");

    for (unsigned int i = 0; i < mStream->GetNumberOfDecodeBuffers(); i++)
    {
        if (mAccumulatedDecodeBufferTable[i].Buffer == NULL)
        {
            mAccumulatedDecodeBufferTable[i].Buffer                 = Buffer;
            mAccumulatedDecodeBufferTable[i].ParsedFrameParameters  = ParsedFrameParameters;
            mAccumulatedDecodeBufferTableOccupancy++;
            return;
        }
    }

    SE_ASSERT(0);
}

Buffer_t DecodeToManifestEdge_c::ExtractFromAccumulatedBuffers(int i)
{
    Buffer_t Buffer                                           = mAccumulatedDecodeBufferTable[i].Buffer;
    mAccumulatedDecodeBufferTable[i].Buffer                   = NULL;
    mAccumulatedDecodeBufferTable[i].ParsedFrameParameters    = NULL;
    mAccumulatedDecodeBufferTableOccupancy--;

    return Buffer;
}

void DecodeToManifestEdge_c::HandleMarkerFrame(Buffer_t MarkerFrameBuffer)
{
    mSequenceNumber                     = GetSequenceNumberStructure(MarkerFrameBuffer)->Value;
    mMaximumActualSequenceNumberSeen    = max(mSequenceNumber, mMaximumActualSequenceNumberSeen);

    SE_DEBUG(group_player, "Stream 0x%p Got Marker Frame #%llu\n", mStream, mSequenceNumber);

    FlushAccumulatedBuffers(NULL);

    ProcessAccumulatedBeforeControlMessages(mSequenceNumber, mTime);
    ProcessAccumulatedAfterControlMessages(mSequenceNumber, mTime);

    // Pass on the marker to Post Manifest Edge
    mStream->PostManifestEdge->Insert((uintptr_t)MarkerFrameBuffer);

    mDiscardingUntilMarkerFrame = false;
}

Buffer_t DecodeToManifestEdge_c::GetNextBufferInDisplayOrder()
{
    Buffer_t Buffer;

    while (!mStream->IsTerminating())
    {
        if (mDiscardingUntilMarkerFrame)
        {
            FlushAccumulatedBuffers(NULL);
        }

        Buffer = SearchNextDisplayOrderBufferInAccumulatedBuffers();
        if (Buffer) { return Buffer; }

        // Skip any frame indices that were unused
        while (CheckForNonDecodedFrame(mDesiredFrameIndex))
        {
            mDesiredFrameIndex++;
        }

        if (mMarkerFrameBuffer != NULL)
        {
            HandleMarkerFrame(mMarkerFrameBuffer);
            mMarkerFrameBuffer = NULL;
            continue;
        }

        RingStatus_t RingStatus  = mInputRing->Extract((uintptr_t *) &Buffer, PLAYER_NEXT_FRAME_EVENT_WAIT);
        if (RingStatus == RingNothingToGet || Buffer == NULL)
        {
            continue;
        }

        Buffer->TransferOwnership(IdentifierProcessDecodeToManifest);
        BufferType_t BufferType;
        Buffer->GetType(&BufferType);

        if (BufferType != mStream->DecodeBufferType)
        {
            return Buffer;
        }

        // If we were set to terminate while we were 'Extracting' we should
        // remove the buffer reference and exit.
        if (mStream->IsTerminating())
        {
            Buffer->DecrementReferenceCount(IdentifierProcessDecodeToManifest);
            continue;
        }

        PlayerSequenceNumber_t  *SequenceNumberStructure = GetSequenceNumberStructure(Buffer);

        // For audio streams, we will always remain in InitialState. There will be no treatment for retiming in PostM for audio.
        SequenceNumberStructure->ManifestedState            = InitialState;
        SequenceNumberStructure->TimeEntryInProcess2        = OS_GetTimeInMicroSeconds();

        if (SequenceNumberStructure->MarkerFrame)
        {
            mMarkerFrameBuffer = Buffer;
            continue;
        }

        InsertInAccumulatedBuffers(Buffer);
    }

    return NULL;
}

bool DecodeToManifestEdge_c::TestDiscardBuffer(Buffer_t Buffer, ParsedFrameParameters_t *ParsedFrameParameters)
{
    if (mStream->IsUnPlayable() || mDiscardingUntilMarkerFrame)
    {
        SE_DEBUG(group_player, "Stream 0x%p Frame Dropped stream UnPlayable %d DiscardingUntilMarkerFrame %d\n",
                 mStream, mStream->IsUnPlayable(), mDiscardingUntilMarkerFrame);
        return true;
    }

    if (ParsedFrameParameters->Discard_picture == true)
    {
        SE_DEBUG(group_player, "Stream 0x%p Discard a picture with desired index : %d\n", mStream, mDesiredFrameIndex);
        return true;
    }

    PlayerStatus_t Status = mOutputTimer->TestForFrameDrop(Buffer, OutputTimerBeforeOutputTiming);
    if (Status != OutputTimerNoError)
    {
        SE_DEBUG(group_player, "Stream 0x%p Frame Dropped before output timing: Status 0x%x DecodeFrameIndex %d\n",
                 mStream, Status, ParsedFrameParameters->DecodeFrameIndex);
        return true;
    }

    mOutputTimer->GenerateFrameTiming(Buffer);

    Status  = mOutputTimer->TestForFrameDrop(Buffer, OutputTimerBeforeManifestation);
    if (Status != OutputTimerNoError)
    {
        SE_DEBUG(group_player, "Stream 0x%p Frame Dropped before manifestation: Status 0x%x DecodeFrameIndex %d\n",
                 mStream, Status, ParsedFrameParameters->DecodeFrameIndex);
        return true;
    }

    // since we have waited, we need to reassess discarding conditions
    if (mStream->IsUnPlayable() || mDiscardingUntilMarkerFrame || mStream->IsTerminating())
    {
        SE_DEBUG(group_player, "Stream 0x%p Frame Dropped : stream UnPlayable %d DiscardingUntilMarkerFrame %d Terminating %d\n",
                 mStream, mStream->IsUnPlayable(), mDiscardingUntilMarkerFrame, mStream->IsTerminating());
        return true;
    }

    return false;
}

void DecodeToManifestEdge_c::DelayForManifestationThrottling(Buffer_t Buffer)
{
    if (mStream->GetPlayer()->PolicyValue(mStream->GetPlayback(), mStream, PolicyAVDSynchronization) == PolicyValueDisapply)
    {
        return;
    }
    if (mFirstFrame == true && mStream->GetPlayer()->PolicyValue(mStream->GetPlayback(), mStream, PolicyManifestFirstFrameEarly) == PolicyValueApply)
    {
        SE_VERBOSE(group_avsync, "Stream 0x%p No throttling for first frame\n", mStream);
        mFirstFrame = false;
        return;
    }

    OutputTiming_t  *OutputTiming;
    Buffer->ObtainMetaDataReference(mStream->GetPlayer()->MetaDataOutputTimingType, (void **)&OutputTiming);
    SE_ASSERT(OutputTiming != NULL);

    if (ValidTime(OutputTiming->BaseSystemPlaybackTime))
    {
        //
        // How long shall we sleep
        //

        long long Now = OS_GetTimeInMicroSeconds();
        long long DeltaTimeToPresentation = OutputTiming->BaseSystemPlaybackTime - Now;
        long long ManifestationDelay = DeltaTimeToPresentation - PLAYER_LIMITED_EARLY_MANIFESTATION_WINDOW;

        if (DeltaTimeToPresentation > PLAYER_LIMITED_EARLY_MANIFESTATION_WINDOW)
        {

            if (ManifestationDelay < MINIMUM_SLEEP_TIME_US)
            {
                SE_EXTRAVERB(group_avsync, "Stream 0x%p Sleep period too short (%lluus)\n", mStream, ManifestationDelay);
                return;
            }
            else if (ManifestationDelay > MAXIMUM_MANIFESTATION_WINDOW_SLEEP)
            {
                SE_DEBUG(group_avsync, "Stream 0x%p Sleep period too long (%lluus), clamping to %d us\n", mStream, ManifestationDelay, MAXIMUM_MANIFESTATION_WINDOW_SLEEP);
                ManifestationDelay               = MAXIMUM_MANIFESTATION_WINDOW_SLEEP;
            }

            if (ManifestationDelay > 1000000)
            {
                PlayerSequenceNumber_t  *SequenceNumberStructure = GetSequenceNumberStructure(Buffer);
                SE_VERBOSE(group_avsync, "Stream 0x%p - About to sleep for %lld ms (presentation_time=%lld, index=%lld)\n"
                           , mStream
                           , ManifestationDelay / 1000
                           , OutputTiming->BaseSystemPlaybackTime
                           , SequenceNumberStructure->CollatedFrameCounter);
            }

            OS_Status_t WaitStatus = OS_WaitForEventAuto(&mThrottlingEvent, ManifestationDelay / 1000);
            if (WaitStatus != OS_TIMED_OUT)
            {
                SE_VERBOSE(group_avsync, "Stream 0x%p wait for throttling woken up\n", mStream);
                OS_ResetEvent(&mThrottlingEvent);
            }
        }
        SE_VERBOSE(group_avsync, "Stream 0x%p - Slept for %lld ms vs expected %lld ms (presentation_time=%lld)\n",
                   mStream, (OS_GetTimeInMicroSeconds() - Now) / 1000, ManifestationDelay / 1000, OutputTiming->BaseSystemPlaybackTime);
    }
}

void DecodeToManifestEdge_c::ReleaseQueuedDecodeBuffers()
{
    OS_SetEvent(&mThrottlingEvent);
    OS_LockMutex(&mQueueBufferLock);
    mManifestationCoordinator->ReleaseQueuedDecodeBuffers();
    OS_UnLockMutex(&mQueueBufferLock);
}

void DecodeToManifestEdge_c::HandleCodedFrameBuffer(Buffer_t Buffer)
{
    PlayerStatus_t Status;
    ParsedFrameParameters_t *ParsedFrameParameters   = GetParsedFrameParameters(Buffer);
    PlayerSequenceNumber_t  *SequenceNumberStructure = GetSequenceNumberStructure(Buffer);

    SE_VERBOSE(group_player, "Stream 0x%p - %d - frame #%llu DisplayFrameIndex=%u %s\n",
               mStream, mStream->GetStreamType(), SequenceNumberStructure->Value, ParsedFrameParameters->DisplayFrameIndex,
               ParsedFrameParameters->KeyFrame ? "I" : ParsedFrameParameters->ReferenceFrame ? "P" : "B");

    // First calculate the sequence number that applies to this frame
    // this calculation may appear weird, the idea is this, assume you
    // have a video stream IPBB, sequence numbers 0 1 2 3, frame reordering
    // will yield sequence numbers 0 2 3 1 IE any command to be executed at
    // the end of the stream will appear 1 frame early, the calculations
    // below will re-wossname the sequence numbers to 0 1 1 3 causing the
    // signal to occur at the correct point.
    //
    mSequenceNumber                      = SequenceNumberStructure->Value;
    mMaximumActualSequenceNumberSeen     = max(mSequenceNumber, mMaximumActualSequenceNumberSeen);
    mSequenceNumber                      = min(mMaximumActualSequenceNumberSeen, mMinimumSequenceNumberAccumulated);
    mTime                                = ParsedFrameParameters->NativePlaybackTime;

    // Check if Time discontinuity detected
    CheckPtsDiscontinuity(ParsedFrameParameters->NormalizedPlaybackTime);

    // Process any outstanding control messages to be applied before this buffer
    ProcessAccumulatedBeforeControlMessages(mSequenceNumber, mTime);

    // If we are paused, then we loop waiting for something to happen
    if (mStream->GetPlayback()->mSpeed == 0)
    {
        // If we are retiming frames, we do not want to push frames to display from DToM but from PostM.
        // Retiming will stop as soon as PostM will have pushed its last non rendered frame to display.
        while ((mStream->GetPlayback()->mSpeed == 0)
               && !mStream->mStep
               && !mStream->IsTerminating()
               && !mDiscardingUntilMarkerFrame
               && !mStream->IsUnPlayable())
        {
            // Release our hold on the manifestor critical section while waiting
            // to be resumed to avoid a deadlock between:
            // - This edge.
            // - A user-space process, e.g. grab_all, waiting for exclusive
            // access to the manifestor critical section for adding or removing
            // a manifestor while holding some STLinuxTV mutex(es).
            // - A user-space process, typically dvbtest, trying to resume this
            // edge but blocked on the aforementioned STLinuxTV mutex(es).
            //
            // Doing so is safe because we are not in a middle of some
            // computation relying on or modifying manifestation state.
            mStream->ExitManifestorSharedSection();

            SE_EXTRAVERB(group_player, "About to wait for step event (mStep=%d Terminating=%d Discarding=%d Unplayable=%d)\n",
                         mStream->mStep, mStream->IsTerminating(), mDiscardingUntilMarkerFrame, mStream->IsUnPlayable());

            OS_WaitForEventAuto(&mStream->SingleStepMayHaveHappened, PLAYER_NEXT_FRAME_EVENT_WAIT);

            SE_EXTRAVERB(group_player, "Woken up\n");

            mStream->EnterManifestorSharedSection();

            OS_ResetEvent(&mStream->SingleStepMayHaveHappened);
        }

        mStream->mStep    = false;
    }

    // Note we loop here if we are engaged in re-timing the decoded frames
    while (!mStream->IsTerminating() && mStream->PostManifestEdge->IsReTiming())
    {
        SE_EXTRAVERB(group_player, "About to sleep as PostManifestEdge is retiming\n");
        OS_SleepMilliSeconds(PLAYER_RETIMING_WAIT);
        SE_EXTRAVERB(group_player, "Woken up\n");
    }

    OS_LockMutex(&mQueueBufferLock);

    bool DiscardBuffer = TestDiscardBuffer(Buffer, ParsedFrameParameters);

    if (mStream->GetStreamType() == StreamTypeVideo)
    {
        CheckForVideoDisplayParametersChange(Buffer);
    }

    // calculate next desired frame index
    mDesiredFrameIndex   = ParsedFrameParameters->DisplayFrameIndex + 1;

    if (!DiscardBuffer)
    {
        OutputTiming_t *OutputTiming;
        Buffer->ObtainMetaDataReference(mStream->GetPlayer()->MetaDataOutputTimingType, (void **)&OutputTiming);
        SE_ASSERT(OutputTiming != NULL);

        // SE-PIPELINE TRACE
        SE_VERBOSE(group_player, "Stream 0x%p - %d - #%lld PTS=%lld DtM=%llu\n",
                   mStream,
                   mStream->GetStreamType(),
                   SequenceNumberStructure->CollatedFrameCounter,
                   SequenceNumberStructure->PTS,
                   SequenceNumberStructure->TimeEntryInProcess2
                  );

        SequenceNumberStructure->TimePassToManifestor   = OS_GetTimeInMicroSeconds();

        // Pass the buffer to the UserData sender to provide it to memory Sink
        // The Buffer is not queued, only user data are extracted
        mStream->UserDataSender->GetUserDataFromDecodeBuffer(Buffer);

        DelayForManifestationThrottling(Buffer);

        mOutputTimer->FillOutManifestationTimings(Buffer);

        // Send the buffer to manifestation coordinator
        mStream->FramesToManifestorCount++;
        mStream->Statistics().FrameCountToManifestor++;

        SE_VERBOSE(group_player, "Stream 0x%p - %d - Queueing frame #%lld to ManifestationCoordinator at PresentationTime %lld  deltaNow %lld\n",
                   mStream, mStream->GetStreamType(), SequenceNumberStructure->Value, OutputTiming->BaseSystemPlaybackTime,
                   OutputTiming->BaseSystemPlaybackTime - OS_GetTimeInMicroSeconds());

        Status  = mManifestationCoordinator->QueueDecodeBuffer(Buffer);
        if (Status != ManifestationCoordinatorNoError)
        {
            SE_ERROR("Stream 0x%p Failed to queue buffer to manifestation coordinator! (Status=%d)\n", mStream, Status);
            ReleaseDecodeBuffer(Buffer);
        }
    }
    else
    {
        // SE-PIPELINE TRACE
        SE_VERBOSE(group_player, "Stream 0x%p - %d - #%lld PTS=%lld Discard=%llu\n",
                   mStream,
                   mStream->GetStreamType(),
                   SequenceNumberStructure->CollatedFrameCounter,
                   SequenceNumberStructure->PTS,
                   SequenceNumberStructure->TimeEntryInProcess2
                  );

        ReleaseDecodeBuffer(Buffer);

        // If we are discarding a frame while step is on going, we arm the step again,
        // unless we are discarding because of a drain: DiscardingUntilMarkerFrame is set to true on a drain.
        if ((mStream->GetPlayback()->mSpeed == 0) && (!mDiscardingUntilMarkerFrame))
        {
            mStream->mStep  = true;
        }
    }

    OS_UnLockMutex(&mQueueBufferLock);

    //
    // Process any outstanding control messages to be applied after this buffer
    //
    ProcessAccumulatedAfterControlMessages(mSequenceNumber, mTime);
}

// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Main process code
//

void   DecodeToManifestEdge_c::MainLoop()
{
    Buffer_t                          Buffer = NULL;
    BufferType_t                      BufferType;

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
        Buffer = GetNextBufferInDisplayOrder();
        mStream->EnterManifestorSharedSection();

        if (mStream->IsTerminating())
        {
            continue;
        }

        Buffer->GetType(&BufferType);

        if (BufferType == mStream->DecodeBufferType)
        {
            HandleCodedFrameBuffer(Buffer);
        }
        else if (BufferType == mStream->GetPlayer()->BufferPlayerControlStructureType)
        {
            HandlePlayerControlStructure(Buffer, mSequenceNumber, mTime, mMaximumActualSequenceNumberSeen);
        }
        else
        {
            SE_ERROR("Stream 0x%p Unknown buffer type received - Implementation error\n", mStream);
            Buffer->DecrementReferenceCount();
        }
    }
    mStream->ExitManifestorSharedSection();

    if (mAccumulatedDecodeBufferTableOccupancy != 0)
    {
        for (unsigned int i = 0; i < mStream->GetNumberOfDecodeBuffers(); i++)
        {
            if (mAccumulatedDecodeBufferTable[i].Buffer != NULL)
            {
                ReleaseDecodeBuffer(mAccumulatedDecodeBufferTable[i].Buffer);
            }
        }
    }

    FlushNonDecodedFrameList();
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


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Function to check if a frame index is non-decoded
//

bool   DecodeToManifestEdge_c::CheckForNonDecodedFrame(unsigned int DisplayFrameIndex)
{
    unsigned int    i;
    bool            Return;
    //
    // Check for this index in the table (if the table is non-empty)
    //
    OS_LockMutex(&mStream->NonDecodedBuffersLock);

    if (mStream->InsertionsIntoNonDecodedBuffers == mStream->RemovalsFromNonDecodedBuffers)
    {
        OS_UnLockMutex(&mStream->NonDecodedBuffersLock);
        return false;
    }

    for (i = 0; i < PLAYER_MAX_DISCARDED_FRAMES; i++)
    {
        if (mStream->NonDecodedBuffers[i].Buffer != NULL)
        {
            //
            // Can we release the actual buffer
            //
            if (!mStream->NonDecodedBuffers[i].ReleasedBuffer)
            {
                if (mStream->NonDecodedBuffers[i].ParsedFrameParameters->DisplayFrameIndex != INVALID_INDEX)
                {
                    if (mDiscardingUntilMarkerFrame ||
                        mStream->NonDecodedBuffers[i].ParsedFrameParameters->CollapseHolesInDisplayIndices)
                    {
                        mStream->DisplayIndicesCollapse = max(mStream->DisplayIndicesCollapse, mStream->NonDecodedBuffers[i].ParsedFrameParameters->DisplayFrameIndex);
                    }

                    // NOTE due to union, ParsedFrameParameters is invalid after this line, which is OK as we free the buffer in the next line anyway
                    mStream->NonDecodedBuffers[i].DisplayFrameIndex      = mStream->NonDecodedBuffers[i].ParsedFrameParameters->DisplayFrameIndex;
                    mStream->NonDecodedBuffers[i].Buffer->DecrementReferenceCount(IdentifierNonDecodedFrameList);
                    mStream->NonDecodedBuffers[i].ReleasedBuffer         = true;
                }
                else
                {
                    continue;
                }
            }

            //
            // Now check the index
            //

            if (mStream->NonDecodedBuffers[i].DisplayFrameIndex <= DisplayFrameIndex)
            {
                Return                                  = (mStream->NonDecodedBuffers[i].DisplayFrameIndex == DisplayFrameIndex);
                mStream->NonDecodedBuffers[i].Buffer     = NULL;
                mStream->RemovalsFromNonDecodedBuffers++;

                if (Return)
                {
                    OS_UnLockMutex(&mStream->NonDecodedBuffersLock);
                    return true;
                }
            }
        }
    }
//
    OS_UnLockMutex(&mStream->NonDecodedBuffersLock);
//
    return (DisplayFrameIndex < mStream->DisplayIndicesCollapse);
}


// //////////////////////////////////////////////////////////////////////////////////////////////////
//
//      Function to flush any recorded non decoded frames
//

void   DecodeToManifestEdge_c::FlushNonDecodedFrameList()
{
    unsigned int i;
//
    OS_LockMutex(&mStream->NonDecodedBuffersLock);

    if (mStream->InsertionsIntoNonDecodedBuffers == mStream->RemovalsFromNonDecodedBuffers)
    {
        OS_UnLockMutex(&mStream->NonDecodedBuffersLock);
        return;
    }

    for (i = 0; i < PLAYER_MAX_DISCARDED_FRAMES; i++)
        if ((mStream->NonDecodedBuffers[i].Buffer != NULL) && !mStream->NonDecodedBuffers[i].ReleasedBuffer)
        {
            mStream->NonDecodedBuffers[i].Buffer->DecrementReferenceCount(IdentifierNonDecodedFrameList);
            mStream->NonDecodedBuffers[i].Buffer                 = NULL;
            mStream->RemovalsFromNonDecodedBuffers++;
        }

    OS_UnLockMutex(&mStream->NonDecodedBuffersLock);
}

//{{{  CheckForVideoDisplayParametersChange
//{{{  doxynote
/// \brief              Allows notification of _FRAME_RATE_CHANGED and PARAMETER_CHANGED events
/// \brief              of a decoded frame buffer before manifestation
//}}}
void DecodeToManifestEdge_c::CheckForVideoDisplayParametersChange(Buffer_t         Buffer)
{
    ParsedVideoParameters_t           *ParsedVideoParameters;
    VideoDisplayParameters_t          *VideoParameters;
    PlayerEventRecord_t                DisplayEvent;
    //
    // Retrieve video parameter from the buffer
    //
    Buffer->ObtainMetaDataReference(mStream->GetPlayer()->MetaDataParsedVideoParametersType, (void **)&ParsedVideoParameters);
    SE_ASSERT(ParsedVideoParameters != NULL);

    VideoParameters = &ParsedVideoParameters->Content;

    //
    // check if something has changed since previous decoded buffer
    //
    if ((memcmp(&mVideoDisplayParameters, VideoParameters, sizeof(mVideoDisplayParameters)) == 0))
    {
        return;
    }

    //
    // Check framerate change
    //
    if (VideoParameters->FrameRate != mVideoDisplayParameters.FrameRate)
    {
        DisplayEvent.Code         = EventSourceFrameRateChangeManifest;
        DisplayEvent.Playback     = mStream->GetPlayback();
        DisplayEvent.Stream       = mStream;
        DisplayEvent.Rational     = VideoParameters->FrameRate;
        //
        // Notify this event
        //
        mStream->SignalEvent(&DisplayEvent);
    }

    //
    // Check if monitored parameters have changed
    //
    if ((VideoParameters->Width             != mVideoDisplayParameters.Width)
        || (VideoParameters->Height            != mVideoDisplayParameters.Height)
        || (VideoParameters->PixelAspectRatio  != mVideoDisplayParameters.PixelAspectRatio)
        || (VideoParameters->Progressive       != mVideoDisplayParameters.Progressive)
        || (VideoParameters->Output3DVideoProperty.Stream3DFormat != mVideoDisplayParameters.Output3DVideoProperty.Stream3DFormat)
        || (VideoParameters->ColourMatrixCoefficients             != mVideoDisplayParameters.ColourMatrixCoefficients)
        || (VideoParameters->FrameRate                            != mVideoDisplayParameters.FrameRate))
    {
        DisplayEvent.Code                   = EventSourceVideoParametersChangeManifest;
        DisplayEvent.Playback               = mStream->GetPlayback();
        DisplayEvent.Stream                 = mStream;
        DisplayEvent.Value[0].UnsignedInt   = VideoParameters->Width;
        DisplayEvent.Value[1].UnsignedInt   = VideoParameters->Height;
        DisplayEvent.Value[2].UnsignedInt   = VideoParameters->Progressive;
        DisplayEvent.Rational               = VideoParameters->PixelAspectRatio;
        DisplayEvent.Value[3].UnsignedInt   = VideoParameters->Output3DVideoProperty.Stream3DFormat;
        DisplayEvent.Value[4].Bool          = VideoParameters->Output3DVideoProperty.Frame0IsLeft;
        // Retrieve colorspace information from MatrixCoefficients

        switch (VideoParameters->ColourMatrixCoefficients)
        {
        case MatrixCoefficients_ITU_R_BT601:      DisplayEvent.Value[5].UnsignedInt = (unsigned int)STM_SE_COLORSPACE_SMPTE170M;       break;

        case MatrixCoefficients_ITU_R_BT709:      DisplayEvent.Value[5].UnsignedInt = (unsigned int)STM_SE_COLORSPACE_BT709;           break;

        case MatrixCoefficients_ITU_R_BT470_2_M:  DisplayEvent.Value[5].UnsignedInt = (unsigned int)STM_SE_COLORSPACE_BT470_SYSTEM_M;  break;

        case MatrixCoefficients_ITU_R_BT470_2_BG: DisplayEvent.Value[5].UnsignedInt = (unsigned int)STM_SE_COLORSPACE_BT470_SYSTEM_BG; break;

        case MatrixCoefficients_SMPTE_170M:       DisplayEvent.Value[5].UnsignedInt = (unsigned int)STM_SE_COLORSPACE_SMPTE170M;       break;

        case MatrixCoefficients_SMPTE_240M:       DisplayEvent.Value[5].UnsignedInt = (unsigned int)STM_SE_COLORSPACE_SMPTE240M;       break;

        default:                                  DisplayEvent.Value[5].UnsignedInt = (unsigned int)STM_SE_COLORSPACE_UNSPECIFIED;
        }

        DisplayEvent.Value[6].UnsignedInt   = IntegerPart(VideoParameters->FrameRate * STM_SE_PLAY_FRAME_RATE_MULTIPLIER);
        SE_INFO(group_player, "Stream 0x%p Incoming Source coded %dx%d visible area %dx%d\n",
                mStream, VideoParameters->DecodeWidth, VideoParameters->DecodeHeight,
                VideoParameters->Width, VideoParameters->Height);
        SE_INFO(group_player, "Stream 0x%p %s Content, FrameRate %d.%02d, PixelAspectRatio %d.%02d\n",
                mStream, VideoParameters->Progressive ? "Progressive" : "Interlaced",
                VideoParameters->FrameRate.IntegerPart(), VideoParameters->FrameRate.RemainderDecimal(),
                VideoParameters->PixelAspectRatio.IntegerPart(), VideoParameters->PixelAspectRatio.RemainderDecimal());
        //
        // Notify this event
        //
        mStream->SignalEvent(&DisplayEvent);
    }

    //
    // Save current Video parameter for next queued buffer
    //
    mVideoDisplayParameters = *VideoParameters;
}
//}}}


PlayerStatus_t   DecodeToManifestEdge_c::CallInSequence(
    PlayerSequenceType_t      SequenceType,
    PlayerSequenceValue_t     SequenceValue,
    PlayerComponentFunction_t Fn,
    ...)
{
    va_list                   List;
    Buffer_t                  ControlStructureBuffer;
    PlayerControlStructure_t *ControlStructure;

    BufferStatus_t Status = mStream->GetPlayer()->GetControlStructurePool()->GetBuffer(&ControlStructureBuffer, IdentifierInSequenceCall);
    if (Status != BufferNoError)
    {
        SE_ERROR("Stream 0x%p Failed to get a control structure buffer\n", mStream);
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
    case ManifestationCoordinatorFnQueueEventSignal:
        va_start(List, Fn);
        memcpy(&ControlStructure->InSequence.Event, va_arg(List, PlayerEventRecord_t *), sizeof(PlayerEventRecord_t));
        va_end(List);
        break;

    case ManifestationCoordinatorFnEventMngrSignal:
        va_start(List, Fn);
        memcpy(&ControlStructure->InSequence.EventMngr, va_arg(List, stm_event_t *), sizeof(stm_event_t));
        va_end(List);
        break;

    case OutputTimerFnResetTimeMapping:
        break;

    case PlayerFnSwitchOutputTimer:
        va_start(List, Fn);
        ControlStructure->InSequence.Pointer    = (void *)va_arg(List, PlayerStream_t);
        va_end(List);
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

PlayerStatus_t   DecodeToManifestEdge_c::PerformInSequenceCall(PlayerControlStructure_t *ControlStructure)
{
    PlayerStatus_t  Status = PlayerNoError;

    switch (ControlStructure->InSequence.Fn)
    {
    case ManifestationCoordinatorFnQueueEventSignal:
        Status  = mManifestationCoordinator->QueueEventSignal(&ControlStructure->InSequence.Event);
        if (Status != PlayerNoError) { SE_ERROR("Stream 0x%p Failed InSequence call (ManifestationCoordinatorFnQueueEventSignal)\n", mStream); }
        break;

    case ManifestationCoordinatorFnEventMngrSignal:
        if (ControlStructure->InSequence.EventMngr.event_id == STM_SE_PLAY_STREAM_EVENT_END_OF_STREAM)
        {
            SE_INFO2(group_api, group_player, "Stream 0x%p event_signal::STM_SE_PLAY_STREAM_EVENT_END_OF_STREAM\n", mStream);
        }

        if (stm_event_signal(&ControlStructure->InSequence.EventMngr) != STM_EVT_NO_ERROR)
        {
            SE_ERROR("Stream 0x%p stm_event_signal error!\n", mStream);
            Status = PlayerError;
        }

        break;

    case OutputTimerFnResetTimeMapping:
        Status  = mOutputTimer->ResetTimeMapping();
        if (Status != PlayerNoError) { SE_ERROR("Stream 0x%p Failed InSequence call (OutputTimerFnResetTimeMapping)\n", mStream); }
        break;

    case PlayerFnSwitchOutputTimer:
        SwitchOutputTimer();
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
//      Switch stream component function for the OutputTimer
//

void    DecodeToManifestEdge_c::SwitchOutputTimer()
{
    SE_DEBUG(group_player, "Stream 0x%p\n", mStream);

    // We must reset to zero the VideoParameter data structure because we just switched to a new stream.
    // Rational_t variables of this structure must not be set to zero to avoid division by 0
    //
    memset((void *)&mVideoDisplayParameters, 0, sizeof(mVideoDisplayParameters));
    mVideoDisplayParameters.PixelAspectRatio = 1; // rational
    mVideoDisplayParameters.FrameRate        = 1; // rational

    // Next frame will be the first frame of a new stream
    mFirstFrame = true;
    OS_ResetEvent(&mThrottlingEvent);

    //
    // If we are ready to switch the output timer then the last decode must
    // have come through, so we signal that the codec can be swapped.
    //
    OS_SetEvent(&mStream->SwitchStreamLastOneOutOfTheCodec);

    mOutputTimer->ResetOnStreamSwitch();

    mManifestationCoordinator->ResetOnStreamSwitch();

    OS_SemaphoreSignal(&mStream->mSemaphoreStreamSwitchOutputTimer);

    SE_DEBUG(group_player, "Stream 0x%p completed\n", mStream);
}
