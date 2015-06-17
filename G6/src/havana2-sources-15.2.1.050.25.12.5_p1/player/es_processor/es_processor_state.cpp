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
#include "es_processor_state.h"
#include "es_processor_base.h"
#include "player.h"

#undef TRACE_TAG
#define TRACE_TAG "ES_Processor_State_c"

/* 90kHz Native PTS Threshold to compare last frame's PTS to current frame's PTS */
#define NEXT_PTS_THRESHOLD  50000

PlayerStatus_t ES_Processor_State_c::Reset(void)
{
    SE_DEBUG(group_esprocessor, "Stream 0x%p\n", mEsProcessor.mStream);
    mEsProcessor.mStartTrigger.Reset();
    mEsProcessor.mEndTrigger.Reset();
    mEsProcessor.mAlarm.Reset();
    mEsProcessor.mPtsOffset = 0;
    mEsProcessor.mLastPts = 0;
    ChangeState(&mEsProcessor.mProcessingStateNoDiscardSet);

    return PlayerNoError;
}

void ES_Processor_State_c::ChangeState(ES_Processor_State_c *NewState)
{
    SE_DEBUG(group_esprocessor, "Stream 0x%p Changing State from 0x%p to 0x%p\n", mEsProcessor.mStream, mEsProcessor.mState, NewState);
    mEsProcessor.mState = NewState;
}

PlayerStatus_t ES_Processor_State_c::NotifyTriggerDetection(ES_Processor_Trigger_c *Trigger)
{
    PlayerEventRecord_t DiscardingEvent;

    DiscardingEvent.Code           = EventDiscarding;
    DiscardingEvent.Playback       = mEsProcessor.mStream->GetPlayback();
    DiscardingEvent.Stream         = mEsProcessor.mStream;
    DiscardingEvent.Value[0].UnsignedInt = (unsigned int)Trigger->GetType();
    DiscardingEvent.Value[1].Bool   = Trigger->IsStartTrigger();

    SE_DEBUG(group_esprocessor, "Stream 0x%p %s Trigger type %d\n", mEsProcessor.mStream,
             Trigger->IsStartTrigger() ? "Start" : "End", Trigger->GetType());
    return mEsProcessor.mStream->SignalEvent(&DiscardingEvent);
}

PlayerStatus_t ES_Processor_State_c::CheckTrigger(Buffer_t Buffer, ES_Processor_Trigger_c *Trigger, bool *IsDetected)
{
    SE_VERBOSE(group_esprocessor, "Stream 0x%p\n", mEsProcessor.mStream);
    PlayerStatus_t Status = PlayerNoError;
    *IsDetected = false;

    switch (Trigger->GetType())
    {
    case STM_SE_PLAY_STREAM_CANCEL_TRIGGER:
        break;
    case STM_SE_PLAY_STREAM_SPLICING_MARKER_TRIGGER:
        PlayerSequenceNumber_t *SequenceNumberStructure;
        Buffer->ObtainMetaDataReference(mEsProcessor.mStream->GetPlayer()->MetaDataSequenceNumberType, (void **)(&SequenceNumberStructure));
        SE_ASSERT(SequenceNumberStructure != NULL);

        if (SequenceNumberStructure->MarkerFrame && SequenceNumberStructure->MarkerType == SplicingMarker)
        {
            SE_DEBUG(group_esprocessor, "Stream 0x%p %s trigger SplicingMarker detected\n",
                     mEsProcessor.mStream, Trigger->IsStartTrigger() ? "Start" : "End");
            *IsDetected = true;
        }
        break;
    case STM_SE_PLAY_STREAM_PTS_TRIGGER:
        CodedFrameParameters_t *CodedFrameParameters;

        Buffer->ObtainMetaDataReference(mEsProcessor.mStream->GetPlayer()->MetaDataCodedFrameParametersType, (void **)(&CodedFrameParameters));
        SE_ASSERT(CodedFrameParameters != NULL);

        SE_VERBOSE(group_esprocessor, "Stream 0x%p current PTS %llu Trigger %llu\n",
                   mEsProcessor.mStream, CodedFrameParameters->PlaybackTime, Trigger->GetPts());
        if (inrange(CodedFrameParameters->PlaybackTime, Trigger->GetPts(), Trigger->GetPts() + Trigger->GetTolerance()))
        {
            SE_DEBUG(group_esprocessor, "Stream 0x%p %s trigger PTS detected\n",
                     mEsProcessor.mStream, Trigger->IsStartTrigger() ? "Start" : "End");
            *IsDetected = true;
        }
        else if (CodedFrameParameters->PlaybackTime > Trigger->GetPts() + Trigger->GetTolerance())
        {
            SE_WARNING("Stream 0x%p exceed PTS Trigger with no event send to user current PTS %llu Trigger %llu\n",
                       mEsProcessor.mStream, CodedFrameParameters->PlaybackTime, Trigger->GetPts());
        }
        break;
    default:
        Status = PlayerError;
        SE_ERROR("Stream 0x%p Incorrect trigger type\n", mEsProcessor.mStream);
        break;
    }

    if (*IsDetected && (Status == PlayerNoError))
    {
        Status = NotifyTriggerDetection(Trigger);
        if (Status != PlayerNoError)
        {
            SE_ERROR("Stream 0x%p Not able to notify Discard event\n", mEsProcessor.mStream);
        }
    }

    return Status;
}

bool ES_Processor_State_c::DetectDiscontinuity(Buffer_t Buffer)
{
    CodedFrameParameters_t *CodedFrameParameters;
    Buffer->ObtainMetaDataReference(mEsProcessor.mStream->GetPlayer()->MetaDataCodedFrameParametersType, (void **)(&CodedFrameParameters));
    SE_ASSERT(CodedFrameParameters != NULL);

    unsigned int DataSize;
    Buffer->ObtainDataReference(NULL, &DataSize, NULL);
    if (CodedFrameParameters->StreamDiscontinuity && DataSize == 0)
    {
        SE_DEBUG(group_esprocessor, "Stream 0x%p Discontinuity detected\n", mEsProcessor.mStream);
        return true;
    }

    return false;
}

PlayerStatus_t ES_Processor_State_c::NotifyAlarm(unsigned long long PlaybackTime)
{
    PlayerEventRecord_t EventRecord;

    EventRecord.Code           = EventAlarmPts;
    EventRecord.Playback       = mEsProcessor.mStream->GetPlayback();
    EventRecord.Stream         = mEsProcessor.mStream;
    /* Transfer PTS */
    EventRecord.PlaybackTime   = PlaybackTime;

    SE_DEBUG(group_esprocessor, "Stream 0x%p PlaybackTime %llu\n", mEsProcessor.mStream, PlaybackTime);
    return mEsProcessor.mStream->SignalEvent(&EventRecord);
}

PlayerStatus_t ES_Processor_State_c::CheckAlarm(Buffer_t Buffer)
{
    if (!mEsProcessor.mAlarm.IsEnabled()) { return PlayerNoError; }

    PlayerStatus_t Status = PlayerNoError;
    CodedFrameParameters_t *CodedFrameParameters;

    Buffer->ObtainMetaDataReference(mEsProcessor.mStream->GetPlayer()->MetaDataCodedFrameParametersType, (void **)(&CodedFrameParameters));
    SE_ASSERT(CodedFrameParameters != NULL);

    SE_VERBOSE(group_esprocessor, "Stream 0x%p current PTS %llu Alarm %llu\n",
               mEsProcessor.mStream, CodedFrameParameters->PlaybackTime, mEsProcessor.mAlarm.GetPts());
    if (inrange(CodedFrameParameters->PlaybackTime, mEsProcessor.mAlarm.GetPts(),
                mEsProcessor.mAlarm.GetPts() + mEsProcessor.mAlarm.GetTolerance()))
    {
        SE_DEBUG(group_esprocessor, "Stream 0x%p PTS Alarm detected\n", mEsProcessor.mStream);
        Status = NotifyAlarm(CodedFrameParameters->PlaybackTime);
        if (Status != PlayerNoError)
        {
            SE_ERROR("Stream 0x%p Not able to notify PTS Alarm event\n", mEsProcessor.mStream);
        }
        mEsProcessor.mAlarm.Reset();
    }
    else if (CodedFrameParameters->PlaybackTime > mEsProcessor.mAlarm.GetPts() + mEsProcessor.mAlarm.GetTolerance())
    {
        SE_WARNING("Stream 0x%p exceed Alarm PTS with no event sent to user current PTS %llu Alarm %llu\n",
                   mEsProcessor.mStream, CodedFrameParameters->PlaybackTime, mEsProcessor.mAlarm.GetPts());
    }
    return Status;
}

void ES_Processor_State_c::ReleaseInputBuffer(Buffer_t Buffer)
{
    SE_VERBOSE(group_esprocessor, "\n");
    Buffer->DecrementReferenceCount(IdentifierEsProcessor);
}

void ES_Processor_State_c::TraceBufferPTS(Buffer_t Buffer)
{
    CodedFrameParameters_t       *CodedFrameParameters;
    Buffer->ObtainMetaDataReference(mEsProcessor.mStream->GetPlayer()->MetaDataCodedFrameParametersType, (void **)(&CodedFrameParameters));
    SE_ASSERT(CodedFrameParameters != NULL);
    PlayerSequenceNumber_t *SequenceNumberStructure;
    Buffer->ObtainMetaDataReference(mEsProcessor.mStream->GetPlayer()->MetaDataSequenceNumberType, (void **)(&SequenceNumberStructure));
    SE_ASSERT(SequenceNumberStructure != NULL);
    SequenceNumberStructure->PTS = CodedFrameParameters->PlaybackTime;

    // SE-PIPELINE TRACE
    SE_VERBOSE(group_player, "Stream 0x%p - %d - #%lld PTS=%lld %s E=%llu\n",
               mEsProcessor.mStream,
               mEsProcessor.mStream->GetStreamType(),
               SequenceNumberStructure->CollatedFrameCounter,
               SequenceNumberStructure->PTS,
               (CodedFrameParameters->PlaybackTimeValid ? "" : "(INVALID: no PTS)"),
               SequenceNumberStructure->TimeEntryInPipeline
              );

    if (CodedFrameParameters->PlaybackTimeValid)
    {
        SE_VERBOSE2(group_esprocessor, group_avsync, "Stream 0x%p Playback time %lld\n",
                    mEsProcessor.mStream, CodedFrameParameters->PlaybackTime);

    }
    else
    {
        SE_VERBOSE2(group_esprocessor, group_avsync, "Stream 0x%p INVALID PLAYBACK TIME\n",
                    mEsProcessor.mStream);
    }
}

void ES_Processor_State_c::UpdatePtsOffset(PlayerSequenceNumber_t *SequenceNumberStructure)
{
    mEsProcessor.mPtsOffset += SequenceNumberStructure->SplicingMarkerData.PTS_offset;
    SE_DEBUG(group_esprocessor, "Stream 0x%p New PTS Offset %lld\n",
             mEsProcessor.mStream, mEsProcessor.mPtsOffset);
}

// 33bits 90Khz wrap offset
#define WRAP_OFFSET 0x0000000200000000LL

void ES_Processor_State_c::ApplyPtsOffset(Buffer_t Buffer)
{
    if (mEsProcessor.mPtsOffset == 0) { return; }

    CodedFrameParameters_t  *CodedFrameParameters;
    Buffer->ObtainMetaDataReference(mEsProcessor.mStream->GetPlayer()->MetaDataCodedFrameParametersType, (void **)(&CodedFrameParameters));
    SE_ASSERT(CodedFrameParameters != NULL);

    if (CodedFrameParameters->PlaybackTimeValid)
    {
        signed long long NewPlaybackTime = static_cast<signed long long>(CodedFrameParameters->PlaybackTime);

        NewPlaybackTime += mEsProcessor.mPtsOffset;
        if (NewPlaybackTime >= WRAP_OFFSET || NewPlaybackTime < 0)
        {
            SE_ERROR("Stream 0x%p Unexpected: Playback time (time: %lld, offset: %lld) is wrapping up exceeding 33bits\n",
                     mEsProcessor.mStream, NewPlaybackTime, mEsProcessor.mPtsOffset);
        }
        SE_VERBOSE(group_esprocessor, "Stream 0x%p Old PlaybackTime %llu New playback time %lld offset %lld\n",
                   mEsProcessor.mStream, CodedFrameParameters->PlaybackTime, NewPlaybackTime, mEsProcessor.mPtsOffset);

        CodedFrameParameters->PlaybackTime = static_cast<unsigned long long>(NewPlaybackTime);
    }

    if (CodedFrameParameters->DecodeTimeValid)
    {
        signed long long NewDecodeTime = static_cast<signed long long>(CodedFrameParameters->DecodeTime);

        NewDecodeTime += mEsProcessor.mPtsOffset;
        if (NewDecodeTime >= WRAP_OFFSET || NewDecodeTime < 0)
        {
            SE_ERROR("Stream 0x%p Unexpected: Decode time (time: %lld, offset: %lld) is wrapping up exceeding 33bits\n",
                     mEsProcessor.mStream, NewDecodeTime, mEsProcessor.mPtsOffset);
        }
        SE_VERBOSE(group_esprocessor, "Stream 0x%p Old DecodeTime %llu New playback time %lld offset %lld\n",
                   mEsProcessor.mStream, CodedFrameParameters->DecodeTime, NewDecodeTime, mEsProcessor.mPtsOffset);

        CodedFrameParameters->DecodeTime = static_cast<unsigned long long>(NewDecodeTime);
    }
}

bool ES_Processor_State_c::DetectSplicingMarker(Buffer_t Buffer)
{
    PlayerSequenceNumber_t *SequenceNumberStructure;
    Buffer->ObtainMetaDataReference(mEsProcessor.mStream->GetPlayer()->MetaDataSequenceNumberType, (void **)(&SequenceNumberStructure));
    SE_ASSERT(SequenceNumberStructure != NULL);

    if (SequenceNumberStructure->MarkerFrame && SequenceNumberStructure->MarkerType == SplicingMarker)
    {
        UpdatePtsOffset(SequenceNumberStructure);
        SE_DEBUG(group_esprocessor, "Stream 0x%p SplicingMarker detected PTS Offset %lld\n",
                 mEsProcessor.mStream, SequenceNumberStructure->SplicingMarkerData.PTS_offset);
        return true;
    }
    return false;
}

RingStatus_t ES_Processor_State_c::InsertInOutputPort(Buffer_t Buffer)
{
    ApplyPtsOffset(Buffer);
    CheckPtsDiscontinuity(Buffer);
    return mEsProcessor.mPort->Insert((uintptr_t)Buffer);
}

PlayerStatus_t ES_Processor_State_c::CommonStateChecks(Buffer_t Buffer, bool *IsSplicingMarker, bool *IsDiscontinuity)
{
    if (Buffer == NULL)
    {
        SE_ERROR("Stream 0x%p Invalid Buffer\n", mEsProcessor.mStream);
        return PlayerError;
    }

    TraceBufferPTS(Buffer);
    Buffer->TransferOwnership(IdentifierEsProcessor);

    *IsSplicingMarker = DetectSplicingMarker(Buffer);
    *IsDiscontinuity  = DetectDiscontinuity(Buffer);

    PlayerStatus_t Status = CheckAlarm(Buffer);
    if (Status != PlayerNoError)
    {
        SE_ERROR("Stream 0x%p Error during Alarm check\n", mEsProcessor.mStream);
    }

    return PlayerNoError;
}

void ES_Processor_State_c::CheckPtsDiscontinuity(Buffer_t Buffer)
{
    CodedFrameParameters_t       *CodedFrameParameters;
    Buffer->ObtainMetaDataReference(mEsProcessor.mStream->GetPlayer()->MetaDataCodedFrameParametersType, (void **)(&CodedFrameParameters));
    SE_ASSERT(CodedFrameParameters != NULL);

    if (CodedFrameParameters->PlaybackTimeValid)
    {
        if (mEsProcessor.mPtsOffset != 0)
        {
            long long unsigned Diff;
            Rational_t        Speed;
            PlayDirection_t       Direction;

            if (CodedFrameParameters->PlaybackTime >= mEsProcessor.mLastPts)
            {
                Diff = CodedFrameParameters->PlaybackTime - mEsProcessor.mLastPts;
            }
            else
            {
                Diff = mEsProcessor.mLastPts - CodedFrameParameters->PlaybackTime;
            }

            mEsProcessor.mStream->GetPlayer()->GetPlaybackSpeed(mEsProcessor.mStream->GetPlayback(), &Speed, &Direction);

            if ((NEXT_PTS_THRESHOLD * Speed) < Diff)
            {
                SE_WARNING("Stream 0x%p PTS too high compared to last PTS, new PTS=%llu last PTS=%llu Diff=%llu\n", mEsProcessor.mStream, CodedFrameParameters->PlaybackTime,
                           mEsProcessor.mLastPts, Diff);
            }
        }

        mEsProcessor.mLastPts = CodedFrameParameters->PlaybackTime;
    }
}

#undef TRACE_TAG
#define TRACE_TAG "ES_ProcessorProcessingState_NoDiscardSet_c"

PlayerStatus_t ES_ProcessorProcessingState_NoDiscardSet_c::Reset(void)
{
    SE_DEBUG(group_esprocessor, "Stream 0x%p\n", mEsProcessor.mStream);
    mEsProcessor.mAlarm.Reset();
    return PlayerNoError;
}

PlayerStatus_t ES_ProcessorProcessingState_NoDiscardSet_c::SetDiscardTrigger(stm_se_play_stream_discard_trigger_t const &trigger)
{
    PlayerStatus_t Status = PlayerNoError;

    switch (trigger.type)
    {
    case STM_SE_PLAY_STREAM_CANCEL_TRIGGER:
        SE_WARNING("Stream 0x%p Unexpected trigger received\n", mEsProcessor.mStream);
        Status = PlayerError;
        break;
    case STM_SE_PLAY_STREAM_PTS_TRIGGER:
    case STM_SE_PLAY_STREAM_SPLICING_MARKER_TRIGGER:
        if (trigger.start_not_end)
        {
            mEsProcessor.mStartTrigger.Set(trigger);
            SE_DEBUG(group_esprocessor, "Stream 0x%p Trigger 0x%p type %d PTS %llu\n",
                     mEsProcessor.mStream, &mEsProcessor.mStartTrigger,
                     mEsProcessor.mStartTrigger.GetType(), mEsProcessor.mStartTrigger.GetPts());
            SE_DEBUG(group_esprocessor, "Stream 0x%p Changing State to mProcessingStateStartDiscardSet\n", mEsProcessor.mStream);
            ChangeState(&mEsProcessor.mProcessingStateStartDiscardSet);
        }
        else
        {
            SE_ERROR("Stream 0x%p Cannot set END trigger prior START trigger has been set\n", mEsProcessor.mStream);
            Status = PlayerError;
        }
        break;
    default:
        Status = PlayerError;
        SE_ERROR("Stream 0x%p Incorrect trigger type\n", mEsProcessor.mStream);
        break;
    }
    return Status;
}

RingStatus_t ES_ProcessorProcessingState_NoDiscardSet_c::Insert(uintptr_t  Value)
{
    SE_VERBOSE(group_esprocessor, "Stream 0x%p\n", mEsProcessor.mStream);
    Buffer_t Buffer = (Buffer_t) Value;
    bool IsSplicingMarker, IsDiscontinuity;
    PlayerStatus_t Status = CommonStateChecks(Buffer, &IsSplicingMarker, &IsDiscontinuity);
    if (Status != PlayerNoError)
    {
        SE_ERROR("Stream 0x%p error %d during common states checks drop buffer\n", mEsProcessor.mStream, Status);
        ReleaseInputBuffer(Buffer);
        return RingNoError;
    }
    if (IsSplicingMarker)
    {
        // No need to propagate this buffer
        ReleaseInputBuffer(Buffer);
        return RingNoError;
    }
    if (IsDiscontinuity)
    {
        SE_DEBUG(group_esprocessor, "Stream 0x%p Insert the Stream Discontinuity\n", mEsProcessor.mStream);
    }

    return InsertInOutputPort(Buffer);
}

bool ES_ProcessorProcessingState_NoDiscardSet_c::NonEmpty(void)
{
    SE_VERBOSE(group_esprocessor, "Stream 0x%p\n", mEsProcessor.mStream);
    return mEsProcessor.mPort->NonEmpty();
}

#undef TRACE_TAG
#define TRACE_TAG "ES_ProcessorProcessingState_StartDiscardSet_c"

PlayerStatus_t ES_ProcessorProcessingState_StartDiscardSet_c::SetDiscardTrigger(stm_se_play_stream_discard_trigger_t const &trigger)
{
    PlayerStatus_t Status = PlayerNoError;

    switch (trigger.type)
    {
    case STM_SE_PLAY_STREAM_CANCEL_TRIGGER:
        if (trigger.start_not_end)
        {
            SE_DEBUG(group_esprocessor, "Stream 0x%p Start Trigger type %d is canceled\n",
                     mEsProcessor.mStream, mEsProcessor.mStartTrigger.GetType());
            mEsProcessor.mStartTrigger.Reset();
            SE_DEBUG(group_esprocessor, "Stream 0x%p Changing State to mProcessingStateNoDiscardSet\n", mEsProcessor.mStream);
            ChangeState(&mEsProcessor.mProcessingStateNoDiscardSet);
        }
        else
        {
            SE_ERROR("Stream 0x%p Cannot cancel End trigger as not configured in this state !!!\n", mEsProcessor.mStream);
            Status = PlayerError;
        }
        break;
    case STM_SE_PLAY_STREAM_PTS_TRIGGER:
    case STM_SE_PLAY_STREAM_SPLICING_MARKER_TRIGGER:
        if (trigger.start_not_end)
        {
            SE_ERROR("Stream 0x%p New START trigger received !!!\n", mEsProcessor.mStream);
            Status = PlayerError;
        }
        else
        {
            mEsProcessor.mEndTrigger.Set(trigger);
            SE_DEBUG(group_esprocessor, "Stream 0x%p Trigger 0x%p type %d PTS %llu\n",
                     mEsProcessor.mStream, &mEsProcessor.mEndTrigger,
                     mEsProcessor.mEndTrigger.GetType(), mEsProcessor.mEndTrigger.GetPts());
            SE_DEBUG(group_esprocessor, "Stream 0x%p Changing State to mProcessingStateStartEndDiscardSet\n", mEsProcessor.mStream);
            ChangeState(&mEsProcessor.mProcessingStateStartEndDiscardSet);
        }
        break;
    default:
        Status = PlayerError;
        SE_ERROR("Stream 0x%p Incorrect trigger type\n", mEsProcessor.mStream);
        break;
    }
    return Status;
}

RingStatus_t ES_ProcessorProcessingState_StartDiscardSet_c::Insert(uintptr_t  Value)
{
    SE_VERBOSE(group_esprocessor, "Stream 0x%p\n", mEsProcessor.mStream);
    Buffer_t Buffer = (Buffer_t) Value;
    bool IsDetected, IsSplicingMarker, IsDiscontinuity;
    PlayerStatus_t Status = CommonStateChecks(Buffer, &IsSplicingMarker, &IsDiscontinuity);
    if (Status != PlayerNoError)
    {
        SE_ERROR("Stream 0x%p error %d during common states checks drop buffer\n", mEsProcessor.mStream, Status);
        ReleaseInputBuffer(Buffer);
        return RingNoError;
    }

    Status = CheckTrigger(Buffer, &mEsProcessor.mStartTrigger, &IsDetected);
    if (Status != PlayerNoError)
    {
        SE_ERROR("Stream 0x%p Error During Trigger check\n", mEsProcessor.mStream);
        ReleaseInputBuffer(Buffer);
        return RingNoError;
    }
    if (IsDetected)
    {
        mEsProcessor.mStartTrigger.Reset();
        SE_DEBUG(group_esprocessor, "Stream 0x%p Start Discarding: Changing State to mDiscardingStateNoEndDiscardSet\n", mEsProcessor.mStream);
        ChangeState(&mEsProcessor.mDiscardingStateNoEndDiscardSet);
        // First buffer to be discarded
        ReleaseInputBuffer(Buffer);
        return RingNoError;
    }
    if (IsSplicingMarker)
    {
        // No need to propagate this buffer
        ReleaseInputBuffer(Buffer);
        return RingNoError;
    }
    if (IsDiscontinuity)
    {
        SE_DEBUG(group_esprocessor, "Stream 0x%p Insert the Stream Discontinuity\n", mEsProcessor.mStream);
    }

    return InsertInOutputPort(Buffer);
}

bool ES_ProcessorProcessingState_StartDiscardSet_c::NonEmpty(void)
{
    SE_VERBOSE(group_esprocessor, "Stream 0x%p\n", mEsProcessor.mStream);
    return mEsProcessor.mPort->NonEmpty();
}

#undef TRACE_TAG
#define TRACE_TAG "ES_ProcessorProcessingState_StartEndDiscardSet_c"

PlayerStatus_t ES_ProcessorProcessingState_StartEndDiscardSet_c::SetDiscardTrigger(stm_se_play_stream_discard_trigger_t const &trigger)
{
    PlayerStatus_t Status = PlayerNoError;

    switch (trigger.type)
    {
    case STM_SE_PLAY_STREAM_CANCEL_TRIGGER:
        SE_DEBUG(group_esprocessor, "Stream 0x%p %s Trigger type %d is canceled\n",
                 mEsProcessor.mStream, trigger.start_not_end ? "start" : "End", mEsProcessor.mStartTrigger.GetType());
        if (trigger.start_not_end)
        {
            SE_DEBUG(group_esprocessor, "Stream 0x%p Canceling Start and End triggers\n", mEsProcessor.mStream);
            mEsProcessor.mStartTrigger.Reset();
            mEsProcessor.mEndTrigger.Reset();
            SE_DEBUG(group_esprocessor, "Stream 0x%p Changing State to mProcessingStateNoDiscardSet\n", mEsProcessor.mStream);
            ChangeState(&mEsProcessor.mProcessingStateNoDiscardSet);
        }
        else
        {
            mEsProcessor.mEndTrigger.Reset();
            SE_DEBUG(group_esprocessor, "Stream 0x%p Changing State to mProcessingStateStartDiscardSet\n", mEsProcessor.mStream);
            ChangeState(&mEsProcessor.mProcessingStateStartDiscardSet);
        }
        break;
    case STM_SE_PLAY_STREAM_PTS_TRIGGER:
    case STM_SE_PLAY_STREAM_SPLICING_MARKER_TRIGGER:
        SE_ERROR("Stream 0x%p No trigger expected in this state !!!\n", mEsProcessor.mStream);
        Status = PlayerError;
        break;
    default:
        SE_ERROR("Stream 0x%p Incorrect trigger type\n", mEsProcessor.mStream);
        Status = PlayerError;
        break;
    }
    return Status;
}

RingStatus_t ES_ProcessorProcessingState_StartEndDiscardSet_c::Insert(uintptr_t  Value)
{
    SE_VERBOSE(group_esprocessor, "Stream 0x%p\n", mEsProcessor.mStream);
    Buffer_t Buffer = (Buffer_t) Value;
    bool IsDetected, IsSplicingMarker, IsDiscontinuity;
    PlayerStatus_t Status = CommonStateChecks(Buffer, &IsSplicingMarker, &IsDiscontinuity);
    if (Status != PlayerNoError)
    {
        SE_ERROR("Stream 0x%p error %d during common states checks drop buffer\n", mEsProcessor.mStream, Status);
        ReleaseInputBuffer(Buffer);
        return RingNoError;
    }

    Status = CheckTrigger(Buffer, &mEsProcessor.mStartTrigger, &IsDetected);
    if (Status != PlayerNoError)
    {
        SE_ERROR("Stream 0x%p Error During Trigger check\n", mEsProcessor.mStream);
        ReleaseInputBuffer(Buffer);
        return RingNoError;
    }
    if (IsDetected)
    {
        mEsProcessor.mStartTrigger.Reset();
        SE_DEBUG(group_esprocessor, "Stream 0x%p Start Discarding: Changing State to mDiscardingStateEndDiscardSet\n", mEsProcessor.mStream);
        ChangeState(&mEsProcessor.mDiscardingStateEndDiscardSet);
        // First buffer to be discarded
        ReleaseInputBuffer(Buffer);
        return RingNoError;
    }
    if (IsSplicingMarker)
    {
        // No need to propagate this buffer
        ReleaseInputBuffer(Buffer);
        return RingNoError;
    }
    if (IsDiscontinuity)
    {
        SE_DEBUG(group_esprocessor, "Stream 0x%p Insert the Stream Discontinuity\n", mEsProcessor.mStream);
    }

    return InsertInOutputPort(Buffer);
}

bool ES_ProcessorProcessingState_StartEndDiscardSet_c::NonEmpty(void)
{
    SE_VERBOSE(group_esprocessor, "Stream 0x%p\n", mEsProcessor.mStream);
    return mEsProcessor.mPort->NonEmpty();
}

#undef TRACE_TAG
#define TRACE_TAG "ES_ProcessorDiscardingState_EndDiscardSet_c"

PlayerStatus_t ES_ProcessorDiscardingState_EndDiscardSet_c::SetDiscardTrigger(stm_se_play_stream_discard_trigger_t const &trigger)
{
    PlayerStatus_t Status = PlayerNoError;

    switch (trigger.type)
    {
    case STM_SE_PLAY_STREAM_CANCEL_TRIGGER:
        if (trigger.start_not_end)
        {
            SE_ERROR("Stream 0x%p Cannot cancel start trigger as not configured in this state !!!\n", mEsProcessor.mStream);
            Status = PlayerError;
        }
        else
        {
            SE_DEBUG(group_esprocessor, "Stream 0x%p End Trigger type %d is canceled\n", mEsProcessor.mStream, mEsProcessor.mEndTrigger.GetType());
            mEsProcessor.mEndTrigger.Reset();
            SE_DEBUG(group_esprocessor, "Stream 0x%p Changing State to mDiscardingStateNoEndDiscardSet\n", mEsProcessor.mStream);
            ChangeState(&mEsProcessor.mDiscardingStateNoEndDiscardSet);
        }
        break;
    case STM_SE_PLAY_STREAM_PTS_TRIGGER:
    case STM_SE_PLAY_STREAM_SPLICING_MARKER_TRIGGER:
        SE_ERROR("Stream 0x%p No trigger expected in this state !!!\n", mEsProcessor.mStream);
        Status = PlayerError;
        break;
    default:
        SE_ERROR("Stream 0x%p Incorrect trigger type\n", mEsProcessor.mStream);
        Status = PlayerError;
        break;
    }
    return Status;
}

RingStatus_t ES_ProcessorDiscardingState_EndDiscardSet_c::Insert(uintptr_t  Value)
{
    SE_VERBOSE(group_esprocessor, "Stream 0x%p\n", mEsProcessor.mStream);
    Buffer_t Buffer = (Buffer_t) Value;
    bool IsDetected, IsSplicingMarker, IsDiscontinuity;
    PlayerStatus_t Status = CommonStateChecks(Buffer, &IsSplicingMarker, &IsDiscontinuity);
    if (Status != PlayerNoError)
    {
        SE_ERROR("Stream 0x%p error %d during common states checks drop buffer\n", mEsProcessor.mStream, Status);
        ReleaseInputBuffer(Buffer);
        return RingNoError;
    }

    Status = CheckTrigger(Buffer, &mEsProcessor.mEndTrigger, &IsDetected);
    if (Status != PlayerNoError)
    {
        SE_ERROR("Stream 0x%p Error During Trigger check\n", mEsProcessor.mStream);
        ReleaseInputBuffer(Buffer);
        return RingNoError;
    }

    if (IsDetected)
    {
        SE_DEBUG(group_esprocessor, "Stream 0x%p Stop Discarding: Changing State to mProcessingStateNoDiscardSet\n", mEsProcessor.mStream);
        ChangeState(&mEsProcessor.mProcessingStateNoDiscardSet);
        if (mEsProcessor.mEndTrigger.GetType() == STM_SE_PLAY_STREAM_PTS_TRIGGER)
        {
            mEsProcessor.mEndTrigger.Reset();
            return InsertInOutputPort(Buffer);
        }
        else
        {
            mEsProcessor.mEndTrigger.Reset();
            // Splicing marker, do not propagate it
            ReleaseInputBuffer(Buffer);
            return RingNoError;
        }
    }
    else
    {
        if (IsDiscontinuity)
        {
            SE_DEBUG(group_esprocessor, "Stream 0x%p Insert the Stream Discontinuity\n", mEsProcessor.mStream);
            return InsertInOutputPort(Buffer);
        }

        ReleaseInputBuffer(Buffer);
        if (IsSplicingMarker)
        {
            // No need to propagate this buffer
            return RingNoError;
        }
        SE_DEBUG(group_esprocessor, "Stream 0x%p Discarding !!!\n", mEsProcessor.mStream);
        return RingNoError;
    }
}

bool ES_ProcessorDiscardingState_EndDiscardSet_c::NonEmpty(void)
{
    SE_VERBOSE(group_esprocessor, "Stream 0x%p\n", mEsProcessor.mStream);
    return mEsProcessor.mPort->NonEmpty();
}

#undef TRACE_TAG
#define TRACE_TAG "ES_ProcessorDiscardingState_NoEndDiscardSet_c"

PlayerStatus_t ES_ProcessorDiscardingState_NoEndDiscardSet_c::SetDiscardTrigger(stm_se_play_stream_discard_trigger_t const &trigger)
{
    PlayerStatus_t Status = PlayerNoError;

    switch (trigger.type)
    {
    case STM_SE_PLAY_STREAM_CANCEL_TRIGGER:
        SE_WARNING("Stream 0x%p Unexpected trigger received\n", mEsProcessor.mStream);
        Status = PlayerError;
        break;
    case STM_SE_PLAY_STREAM_PTS_TRIGGER:
    case STM_SE_PLAY_STREAM_SPLICING_MARKER_TRIGGER:
        if (trigger.start_not_end)
        {
            SE_ERROR("Stream 0x%p New START trigger received !!!\n", mEsProcessor.mStream);
            Status = PlayerError;
        }
        else
        {
            mEsProcessor.mEndTrigger.Set(trigger);
            SE_DEBUG(group_esprocessor, "Stream 0x%p Changing State to mDiscardingStateEndDiscardSet\n", mEsProcessor.mStream);
            ChangeState(&mEsProcessor.mDiscardingStateEndDiscardSet);
        }
        break;
    default:
        Status = PlayerError;
        SE_ERROR("Stream 0x%p Incorrect trigger type\n", mEsProcessor.mStream);
        break;
    }
    return Status;
}

RingStatus_t ES_ProcessorDiscardingState_NoEndDiscardSet_c::Insert(uintptr_t  Value)
{
    SE_VERBOSE(group_esprocessor, "Stream 0x%p\n", mEsProcessor.mStream);
    Buffer_t Buffer = (Buffer_t) Value;
    bool IsSplicingMarker, IsDiscontinuity;

    PlayerStatus_t Status = CommonStateChecks(Buffer, &IsSplicingMarker, &IsDiscontinuity);
    if (Status != PlayerNoError)
    {
        SE_ERROR("Stream 0x%p error %d during common states checks drop buffer\n", mEsProcessor.mStream, Status);
        ReleaseInputBuffer(Buffer);
        return RingNoError;
    }

    if (IsDiscontinuity)
    {
        SE_DEBUG(group_esprocessor, "Stream 0x%p Insert the Stream Discontinuity\n", mEsProcessor.mStream);
        return InsertInOutputPort(Buffer);
    }

    ReleaseInputBuffer(Buffer);
    if (IsSplicingMarker)
    {
        // No need to propagate this buffer
        return RingNoError;
    }
    SE_DEBUG(group_esprocessor, "Stream 0x%p Discarding !!!\n", mEsProcessor.mStream);
    return RingNoError;
}

bool ES_ProcessorDiscardingState_NoEndDiscardSet_c::NonEmpty(void)
{
    SE_VERBOSE(group_esprocessor, "Stream 0x%p\n", mEsProcessor.mStream);
    return mEsProcessor.mPort->NonEmpty();
}

