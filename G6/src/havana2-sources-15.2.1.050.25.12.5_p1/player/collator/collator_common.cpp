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

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "stack_generic.h"
#include "collator_common.h"

// /////////////////////////////////////////////////////////////////////////
//
// Buffering window in microseconds

#define PIPELINE_LATENCY                    100000
#define REORDERING_GUARD                    100000
#define LIMITED_EARLY_INJECTION_WINDOW      1500000
#define ONE_SECOND_IN_PTS                   90000
// We want to keep a limit
// to avoid sleeping forever
// in case of bad PTS
#define MAXIMUM_LIMITED_EARLY_INJECTION_DELAY   20000000


//      Insert marker frame to the output port
//      Function is only used today for splicing feature

CollatorStatus_t   Collator_Common_c::InsertMarkerFrameToOutputPort(PlayerStream_t Stream, markerType_t markerType, void *Data)
{
    BufferStatus_t            Status;
    RingStatus_t              RingStatus;
    CodedFrameParameters_t   *CodedFrameParameters;
    PlayerSequenceNumber_t   *SequenceNumberStructure;
    Buffer_t                  MarkerFrame;

    // Only splicing marker is supported fttb as they will
    // not propagate further than the es_processor
    SE_ASSERT(markerType == SplicingMarker);

    //
    // Insert a marker frame into the output port
    //
    Status = Stream->GetCodedFrameBufferPool()->GetBuffer(&MarkerFrame, IdentifierDrain, 0);
    if (Status != BufferNoError)
    {
        SE_ERROR("Stream 0x%p Unable to obtain a marker frame\n", Stream);
        return CollatorError;
    }

    MarkerFrame->ObtainMetaDataReference(Player->MetaDataCodedFrameParametersType, (void **)(&CodedFrameParameters));
    SE_ASSERT(CodedFrameParameters != NULL);

    memset(CodedFrameParameters, 0, sizeof(*CodedFrameParameters));

    MarkerFrame->ObtainMetaDataReference(Player->MetaDataSequenceNumberType, (void **)(&SequenceNumberStructure));
    SE_ASSERT(SequenceNumberStructure != NULL);

    SequenceNumberStructure->MarkerFrame = true;
    SequenceNumberStructure->MarkerType  = markerType;

    if (markerType == SplicingMarker)
    {
        SequenceNumberStructure->SplicingMarkerData = *((stm_marker_splicing_data_t *) Data);

        SE_INFO(GetGroupTrace(), "Stream 0x%p Splicing Discontinuity found splicing_flags %u PTS_offset %lld\n",
                Stream, SequenceNumberStructure->SplicingMarkerData.splicing_flags, SequenceNumberStructure->SplicingMarkerData.PTS_offset);
    }

    //
    // Insert the marker into the flow
    //
    RingStatus = mOutputPort->Insert((uintptr_t)MarkerFrame);
    if (RingStatus != RingNoError)
    {
        SE_ERROR("Stream 0x%p Failed to insert buffer in ouput port\n", Stream);
        return CollatorError;
    }

    return CollatorNoError;
}

CollatorStatus_t Collator_Common_c::InsertInOutputPort(Buffer_t Buffer)
{
    PlayerSequenceNumber_t *SequenceNumberStructure;
    Buffer->ObtainMetaDataReference(Player->MetaDataSequenceNumberType, (void **)(&SequenceNumberStructure));
    SE_ASSERT(SequenceNumberStructure != NULL);

    memset(SequenceNumberStructure, 0, sizeof(*SequenceNumberStructure));
    StampFrame(SequenceNumberStructure);

    RingStatus_t RingStatus = mOutputPort->Insert((uintptr_t)Buffer);
    if (RingStatus != RingNoError)
    {
        SE_ERROR("Stream 0x%p Failed to insert buffer in ouput port\n", Stream);
        return CollatorError;
    }

    Stream->Statistics().BufferCountFromCollator++;
    return CollatorNoError;
}

void Collator_Common_c::StampFrame(PlayerSequenceNumber_t *SequenceNumberStructure)
{
    SequenceNumberStructure->TimeEntryInPipeline    = OS_GetTimeInMicroSeconds();
    SequenceNumberStructure->CollatedFrameCounter   = mCollatedFrameCounter++;

}


void Collator_Common_c::SetDrainingStatus(bool isDraining)
{
    mIsDraining = isDraining;
    if (isDraining)
    {
        SE_VERBOSE(GetGroupTrace() | group_avsync, "Stream 0x%p Drain requested, waking up collation thread\n", Stream);
        OS_SetEvent(&ThrottlingWakeUpEvent);
    }
    else
    {
        OS_ResetEvent(&ThrottlingWakeUpEvent);
    }
}

// /////////////////////////////////////////////////////////////////////////
//
//      Private - If the input is to be throttled, then the delay
//        will occur here.
//

void   Collator_Common_c::DelayForInjectionThrottling(CodedFrameParameters_t *CodedFrameParameters)
{
    PlayerStatus_t       Status;
    Rational_t       Speed;
    PlayDirection_t      Direction;
    unsigned long long   SystemPlaybackTime;
    unsigned long long   Now;
    unsigned long long   CurrentPTS;
    long long        DeltaPTS;
    long long        EarliestInjectionTime;
    long long        Delay;

    if (Stream->GetStreamType() != StreamTypeAudio)
    {
        return;
    }
    //
    // Is throttling enabled, and do we have a pts to base the limit on
    //

    if (
        (Player->PolicyValue(Playback, Stream, PolicyLimitInputInjectAhead) == PolicyValueDisapply) ||
        (Player->PolicyValue(Playback, Stream, PolicyLivePlayback) == PolicyValueApply) ||
        (Player->PolicyValue(Playback, Stream, PolicyHdmiRxMode) != PolicyValueHdmiRxModeDisabled) ||
        (Player->PolicyValue(Playback, Stream, PolicyAVDSynchronization) == PolicyValueDisapply)
    )
    {
        SE_VERBOSE(GetGroupTrace() | group_avsync, "Stream 0x%p Throttling not applied. policy mismatch\n", Stream);
        return;
    }
    if (GetDrainingStatus())
    {
        SE_VERBOSE(GetGroupTrace() | group_avsync, "Stream 0x%p Throttling not applied. Drain on-going\n", Stream);
        return;
    }

    if (!CodedFrameParameters->PlaybackTimeValid)
    {
        SE_VERBOSE(GetGroupTrace() | group_avsync, "Stream 0x%p Throttling not applied. Invalid playback time : %lld (%p)\n"
                   , Stream
                   , CodedFrameParameters->PlaybackTime
                   , CodedFrameParameters);
        return;
    }

    Player->GetPlaybackSpeed(Playback, &Speed, &Direction);

    if (Direction == PlayBackward)
    {
        SE_VERBOSE(GetGroupTrace() | group_avsync, "Stream 0x%p Throttling not applied. Playing backward\n", Stream);
        return;
    }


    Status    = Player->TranslateNativePlaybackTime(Playback, CodedFrameParameters->PlaybackTime, &SystemPlaybackTime);

    if (Status != PlayerNoError)
    {
        SE_VERBOSE(GetGroupTrace() | group_avsync
                   , "Stream 0x%p Throttling not applied. Could not retrieve playback time (%d), got %lld from %lld\n"
                   , Stream
                   , Status
                   , SystemPlaybackTime
                   , CodedFrameParameters->PlaybackTime);
        return;
    }

    //
    // Watch out for jumping PTS values, they can confuse us
    //

    if (NotValidTime(mLimitHandlingLastPTS))
    {
        mLimitHandlingLastPTS  = CodedFrameParameters->PlaybackTime;
    }

    DeltaPTS            = CodedFrameParameters->PlaybackTime - mLimitHandlingLastPTS;

    if (!inrange(DeltaPTS, -ONE_SECOND_IN_PTS, ONE_SECOND_IN_PTS))
    {
        mLimitHandlingJumpInEffect   = true;
        mLimitHandlingJumpAt     = CodedFrameParameters->PlaybackTime;
    }

    mLimitHandlingLastPTS    = CodedFrameParameters->PlaybackTime;

    if (mLimitHandlingJumpInEffect)
    {
        Status = Player->RetrieveNativePlaybackTime(Playback, &CurrentPTS);

        if ((Status != PlayerNoError) ||
            (CurrentPTS > mLimitHandlingJumpAt + 16 * ONE_SECOND_IN_PTS))
        {
            SE_VERBOSE(GetGroupTrace() | group_avsync, "Stream 0x%p Throttling not applied. CurrentPTS=%lld mLimitHandlingJumpAt=%lld diff=%lld\n"
                       , Stream
                       , CurrentPTS
                       , mLimitHandlingJumpAt
                       , CurrentPTS - mLimitHandlingJumpAt
                      );
            return;
        }

        mLimitHandlingJumpInEffect   = false;
    }

    if (Speed == 0)
    {
        SE_VERBOSE(GetGroupTrace() | group_avsync, "Stream 0x%p Throttling not applied. Speed is 0", Stream);
        return;
    }

    //
    // Calculate and perform a delay if necessary
    //
    Now             = OS_GetTimeInMicroSeconds();
    int window = LIMITED_EARLY_INJECTION_WINDOW;

    // Playback speed should compensate the window size
    window = (LIMITED_EARLY_INJECTION_WINDOW * Speed).LongLongIntegerPart();

    EarliestInjectionTime = SystemPlaybackTime - PIPELINE_LATENCY - (REORDERING_GUARD / Speed).LongLongIntegerPart() - window;
    Delay = EarliestInjectionTime - (long long)Now;

    if (Delay <= 1000)
    {
        SE_VERBOSE(GetGroupTrace() | group_avsync, "Stream 0x%p Throttling not applied. Delay too short (%lld)", Stream, Delay);
        return;
    }

    if (Delay > MAXIMUM_LIMITED_EARLY_INJECTION_DELAY)
    {
        SE_VERBOSE(GetGroupTrace() | group_avsync, "Stream 0x%p Computed delay is above threshold (%lld > %d) for playback time %lld"
                   , Stream
                   , Delay
                   , MAXIMUM_LIMITED_EARLY_INJECTION_DELAY
                   , SystemPlaybackTime);

        Delay = MAXIMUM_LIMITED_EARLY_INJECTION_DELAY;
    }
    SE_VERBOSE(GetGroupTrace() | group_avsync, "Stream 0x%p - Sleeping for %lld (window=%d earliest=%lld playbacktime=%lld now=%lld) \n"
               , Stream
               , Delay
               , window
               , EarliestInjectionTime
               , SystemPlaybackTime
               , Now);

    OS_Status_t WaitStatus = OS_WaitForEventAuto(&ThrottlingWakeUpEvent, Delay / 1000);
    if (WaitStatus != OS_TIMED_OUT)
    {
        SE_VERBOSE(GetGroupTrace() | group_avsync, "Stream 0x%p Throttling wait was woken up\n" , Stream);
    }

    OS_ResetEvent(&ThrottlingWakeUpEvent);
}

//
// Collate input buffer.
//
// Perform processing common to all collators and delegate collator-specific
// processing to derived classes.
//
CollatorStatus_t Collator_Common_c::Input(PlayerInputDescriptor_t *Input,
                                          unsigned int             DataLength,
                                          void                    *Data,
                                          bool                     NonBlocking,
                                          unsigned int            *DataLengthRemaining)
{
    return DoInput(Input, DataLength, Data, NonBlocking, DataLengthRemaining);
}
