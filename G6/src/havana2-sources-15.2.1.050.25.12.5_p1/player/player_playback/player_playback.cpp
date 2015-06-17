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

#include "player_playback.h"
#include "player_stream.h"
#include "collate_to_parse_edge.h"
#include "parse_to_decode_edge.h"
#include "decode_to_manifest_edge.h"
#include "post_manifest_edge.h"

#undef TRACE_TAG
#define TRACE_TAG "PlayerPlayback_s"

PlayerPlaybackInterface_c::~PlayerPlaybackInterface_c() {}

PlayerPlayback_s::PlayerPlayback_s()
    : Next(NULL)
    , Player(NULL)
    , OutputCoordinator(NULL)
    , mSpeed(1)
    , mDirection(PlayForward)
    , PresentationIntervalReversalLimitStartNormalizedTime(INVALID_TIME)
    , PresentationIntervalReversalLimitEndNormalizedTime(INVALID_TIME)
    , RequestedPresentationIntervalStartNormalizedTime(INVALID_TIME)
    , RequestedPresentationIntervalEndNormalizedTime(INVALID_TIME)
    , PolicyRecord()
    , ControlsRecord()
    , AudioCodedFrameCount(0)
    , AudioCodedMemorySize(0)
    , AudioCodedFrameMaximumSize(0)
    , AudioCodedMemoryPartitionName()
    , VideoCodedFrameCount(0)
    , VideoCodedMemorySize(0)
    , VideoCodedFrameMaximumSize(0)
    , VideoCodedMemoryPartitionName()
    , OtherCodedFrameCount(0)
    , OtherCodedMemorySize(0)
    , OtherCodedFrameMaximumSize(0)
    , OtherCodedMemoryPartitionName()
    , LastNativeTime(0)
    , DrainStartStopEvent()
    , DrainSignalThreadRunning(false)
    , mDrainSignal()
    , mIsLowPowerState(false)
    , mStatistics()
    , mStreamListLock()
    , ListOfStreams(NULL)
    , mTranslateTimeLock()
    , mTranslateTimeStream(NULL)
{
    OS_InitializeMutex(&mStreamListLock);
    OS_InitializeMutex(&mTranslateTimeLock);

    OS_InitializeEvent(&mDrainSignal);
    OS_InitializeEvent(&DrainStartStopEvent);
}

PlayerPlayback_s::~PlayerPlayback_s()
{
    PlayerStatus_t            Status;

    //
    // Commence drain on all playing streams
    //
    Status = InternalDrain(PolicyPlayoutOnTerminate, false);
    if (Status != PlayerNoError)
    {
        SE_ERROR("Failed to drain streams\n");
    }

    // Close down Drain thread
    SE_INFO(group_player, "Playback 0x%p Close down drain thread\n", this);

    // Ask thread to terminate
    OS_ResetEvent(&DrainStartStopEvent);
    OS_Smp_Mb(); // Write memory barrier: wmb_for_Player_Generic_Terminating coupled with: rmb_for_Player_Generic_Terminating
    DrainSignalThreadRunning = false;
    OS_SetEvent(&mDrainSignal);

    // Wait for drain thread to terminate
    OS_Status_t WaitStatus;
    do
    {
        WaitStatus = OS_WaitForEventAuto(&DrainStartStopEvent, PLAYER_MAX_EVENT_WAIT);
        if (WaitStatus == OS_TIMED_OUT)
        {
            SE_WARNING("Playback 0x%p Still waiting for drain thread to stop\n", this);
        }
    }
    while (WaitStatus == OS_TIMED_OUT);

    OS_TerminateEvent(&DrainStartStopEvent);
    OS_TerminateEvent(&mDrainSignal);

    //
    // Clean up the structures
    //
    PlayerStream_t IterStream = ListOfStreams;
    while (IterStream != NULL)
    {
        PlayerStream_t CurStream = IterStream;
        IterStream = IterStream->Next;
        delete CurStream;  // will update ListOfStreams.. so don't use it directly
    }

    OS_TerminateMutex(&mTranslateTimeLock);
    OS_TerminateMutex(&mStreamListLock);
}

//
// Return frame parser used for translating time.
// Must hold mTranslateTimeLock.
//
FrameParser_t PlayerPlayback_s::GetTranslateTimeFrameParser() const
{
    if (mTranslateTimeStream == NULL)
    {
        SE_ERROR("Playback 0x%p No stream to translate time\n", this);
        return NULL;
    }

    FrameParser_t fp = mTranslateTimeStream->GetFrameParser();
    if (fp == NULL)
    {
        SE_ERROR("Playback 0x%p No frame parser to translate time\n", this);
        return NULL;
    }

    return fp;
}

PlayerStatus_t PlayerPlayback_s::TranslateTimeNativeToNormalized(
    unsigned long long NativeTime,
    unsigned long long *NormalizedTime,
    PlayerTimeFormat_t NativeTimeFormat)
{
    OS_LockMutex(&mTranslateTimeLock);

    FrameParser_t fp = GetTranslateTimeFrameParser();
    if (fp == NULL)
    {
        OS_UnLockMutex(&mTranslateTimeLock);
        SE_ERROR("Playback 0x%p no translate time frameparser\n", this);
        return PlayerUnknownStream;
    }

    FrameParserStatus_t Status = fp->TranslatePlaybackTimeNativeToNormalized(NativeTime, NormalizedTime, NativeTimeFormat);
    if (Status != FrameParserNoError)
    {
        SE_ERROR("Playback 0x%p Failed to translate native time to normalized\n", this);
        OS_UnLockMutex(&mTranslateTimeLock);
        return PlayerError;
    }

    OS_UnLockMutex(&mTranslateTimeLock);

    return PlayerNoError;
}

PlayerStatus_t PlayerPlayback_s::TranslateTimeNormalizedToNative(
    unsigned long long NormalizedTime,
    unsigned long long *NativeTime,
    PlayerTimeFormat_t NativeTimeFormat)
{
    OS_LockMutex(&mTranslateTimeLock);

    FrameParser_t fp = GetTranslateTimeFrameParser();
    if (fp == NULL)
    {
        OS_UnLockMutex(&mTranslateTimeLock);
        SE_ERROR("Playback 0x%p no translate time frameparser\n", this);
        return PlayerUnknownStream;
    }

    FrameParserStatus_t Status = fp->TranslatePlaybackTimeNormalizedToNative(NormalizedTime, NativeTime, NativeTimeFormat);
    if (Status != FrameParserNoError)
    {
        SE_ERROR("Playback 0x%p Failed to translate normalized time to native\n", this);
        OS_UnLockMutex(&mTranslateTimeLock);
        return PlayerError;
    }

    OS_UnLockMutex(&mTranslateTimeLock);

    return PlayerNoError;
}

//
// Given that one stream is suffering from a failure to deliver data in time
// what is the status on other streams in the same multiplex?
//
// This function is called by an output timer, on discovery that data is not being delivered in
// time. The function checks if the data is multiplexed, and if it is, checks to see if a mismatch
// in input buffering is causing the late delivery on the specified stream.
// This is a heuristic calculation, and used as an indicator to help debugging rather than a hard
// and fast piece of code.
//
PlayerStatus_t PlayerPlayback_s::CheckForCodedBufferMismatch(PlayerStream_t Stream)
{
    PlayerStream_t  OtherStream;
    unsigned int    CodedFrameBufferCount;
    unsigned int    CodedFrameBuffersInUse;
    unsigned int    MemoryInPool;
    unsigned int    MemoryAllocated;

    OS_LockMutex(&mStreamListLock);

    for (OtherStream = ListOfStreams; OtherStream != NULL; OtherStream = OtherStream->Next)
        if (Stream != OtherStream)
        {
            OtherStream->GetCodedFrameBufferPool()->GetPoolUsage(&CodedFrameBufferCount,
                                                                 &CodedFrameBuffersInUse,
                                                                 &MemoryInPool,
                                                                 &MemoryAllocated, NULL);
            // todo: understand following
            if (((CodedFrameBuffersInUse * 10) >= (CodedFrameBufferCount * 9)) ||
                ((MemoryAllocated * 10) >= (MemoryInPool * 9)))
            {
                SE_INFO(group_player, "Stream(%s) appears to have filled all it's input buffers,\n",
                        ToString(OtherStream->GetStreamType()));
                SE_INFO(group_player, " =>probable inappropriate buffer sizing for the multiplexed stream\n");
                break;
            }
        }

    OS_UnLockMutex(&mStreamListLock);

    return PlayerNoError;
}

PlayerStatus_t PlayerPlayback_s::SetPresentationInterval(
    PlayerStream_t        Stream,
    unsigned long long    IntervalStartNativeTime,
    unsigned long long    IntervalEndNativeTime)
{
    FrameParserStatus_t     Status                          = FrameParserNoError;
    unsigned long long      IntervalStartNormalizedTime     = INVALID_TIME;
    unsigned long long      IntervalEndNormalizedTime       = INVALID_TIME;
    PlayerStream_t          SubStream;

    OS_LockMutex(&mStreamListLock);

    //
    // If there are no streams, translate the times as standard PTS times.
    //
    if (ListOfStreams == NULL)
    {
        IntervalStartNormalizedTime = NotValidTime(IntervalStartNativeTime) ? INVALID_TIME : ((((IntervalStartNativeTime) * 300) + 13) / 27);
        IntervalEndNormalizedTime   = NotValidTime(IntervalEndNativeTime) ? INVALID_TIME : ((((IntervalEndNativeTime)   * 300) + 13) / 27);
    }

    //
    // Apply the setting to all streams or designated one.
    //
    for (SubStream = ((Stream == PlayerAllStreams) ? ListOfStreams : Stream);
         ((Stream == PlayerAllStreams) ? (SubStream != NULL) : (SubStream == Stream));
         SubStream = SubStream->Next)
    {
        // If the start or end time for the interval are invalid
        // it just means that API was called with value STM_SE_PLAY_TIME_NOT_BOUNDED.
        // In this case, we don't want to compute the normalized time,
        // but keep the normalized time as invalid, as the native one.
        if (NotValidTime(IntervalStartNativeTime))
        {
            IntervalStartNormalizedTime = INVALID_TIME;
        }
        else
        {
            Status = SubStream->GetFrameParser()->TranslatePlaybackTimeNativeToNormalized(IntervalStartNativeTime, &IntervalStartNormalizedTime, TimeFormatPts);
        }

        if (Status == FrameParserNoError)
        {
            if (NotValidTime(IntervalEndNativeTime))
            {
                IntervalEndNormalizedTime = INVALID_TIME;
            }
            else
            {
                Status = SubStream->GetFrameParser()->TranslatePlaybackTimeNativeToNormalized(IntervalEndNativeTime, &IntervalEndNormalizedTime, TimeFormatPts);
            }
        }

        if (Status != FrameParserNoError)
        {
            SE_ERROR("Failed to translate native time to normalized time\n");
            OS_UnLockMutex(&mStreamListLock);
            return Status;
        }

        SubStream->RequestedPresentationIntervalStartNormalizedTime = IntervalStartNormalizedTime;
        SubStream->RequestedPresentationIntervalEndNormalizedTime   = IntervalEndNormalizedTime;
    }

    //
    // Now erase any reversal limits, and if this was for all streams set the master requested times.
    //
    PresentationIntervalReversalLimitStartNormalizedTime  = INVALID_TIME;
    PresentationIntervalReversalLimitEndNormalizedTime    = INVALID_TIME;

    if (Stream == PlayerAllStreams)
    {
        RequestedPresentationIntervalStartNormalizedTime  = IntervalStartNormalizedTime;
        RequestedPresentationIntervalEndNormalizedTime    = IntervalEndNormalizedTime;
    }

    OS_UnLockMutex(&mStreamListLock);

    return PlayerNoError;
}

PlayerStatus_t   PlayerPlayback_s::InternalDrain(
    PlayerPolicy_t            PlayoutPolicy,
    bool                      ParseAllFrames)
{
    PlayerStatus_t    Status;
    bool              Discard;
    PlayerStream_t    Stream;

    SE_DEBUG(group_player, "Playback 0x%p\n", this);

    //
    // Commence drain on all playing streams (use non-blocking on individuals).
    //
    Discard     = false;

    OS_LockMutex(&mStreamListLock);

    for (Stream  = ListOfStreams;
         Stream != NULL;
         Stream  = Stream->Next)
    {
        int PlayoutPolicyValue = Player->PolicyValue(this, Stream, PlayoutPolicy);

        if (PlayoutPolicyValue == PolicyValueDiscard)
        {
            Discard       = true;
        }

        Status = Stream->Drain(true, false, NULL, PlayoutPolicy, ParseAllFrames, NULL);
        if (Status != PlayerNoError)
        {
            SE_ERROR("Stream 0x%p - %d - failed to drain\n", Stream, Stream->GetStreamType());
        }
    }

    //
    // Start waiting for the drains to complete, and removing the appropriate data.
    //

    for (Stream  = ListOfStreams;
         Stream != NULL;
         Stream  = Stream->Next)
    {
        Status = Stream->WaitForDrainCompletion(Discard);
        if (Status != PlayerNoError)
        {
            SE_ERROR("Stream 0x%p - %d - failed to drain within allowed time (%d %d %d %d)\n",
                     Stream, Stream->GetStreamType(),
                     Stream->CollateToParseEdge->isDiscardingUntilMarkerFrame(), Stream->ParseToDecodeEdge->isDiscardingUntilMarkerFrame(),
                     Stream->DecodeToManifestEdge->isDiscardingUntilMarkerFrame(), Stream->PostManifestEdge->isDiscardingUntilMarkerFrame());
            break;
        }
    }

    OS_UnLockMutex(&mStreamListLock);

    SE_DEBUG(group_player, "Playback 0x%p completed\n", this);
    return PlayerNoError;
}

//
// Set the playback speed and direction.
//
// Setting the speed to zero has the effect of pausing the playback, less than 1
// means slow motion, greater than one means fast wind.
//
PlayerStatus_t   PlayerPlayback_s::SetPlaybackSpeed(
    Rational_t Speed,
    PlayDirection_t Direction)
{
    PlayerStatus_t      Status;
    unsigned long long  Now;
    unsigned long long  NormalizedTimeAtStartOfDrain;
    PlayerStream_t      Stream;

    SE_INFO(group_player, "Playback 0x%p Speed %d.%06d Direction %d\n",
            this, Speed.IntegerPart(), Speed.RemainderDecimal(), Direction);

    if ((mSpeed == Speed) && (mDirection == Direction))
    {
        return PlayerNoError;
    }

    //
    // Ensure that the playback interval is not cluttered by reversal clamps.
    //
    PresentationIntervalReversalLimitStartNormalizedTime  = INVALID_TIME;
    PresentationIntervalReversalLimitEndNormalizedTime    = INVALID_TIME;

    //
    // Are we performing a direction flip?
    //

    if (mDirection != Direction)
    {
        //
        // Find the current playback time
        //
        Now = OS_GetTimeInMicroSeconds();
        Status  = OutputCoordinator->TranslateSystemTimeToPlayback(PlaybackContext, Now, &NormalizedTimeAtStartOfDrain);

        if (Status != PlayerNoError)
        {
            SE_ERROR("Playback 0x%p Failed to translate system time to playback time\n", this);
            NormalizedTimeAtStartOfDrain    = INVALID_TIME;
        }

        //
        // Drain with prejudice, but ensuring all frames are parsed.
        //

        if (Speed == 0)
        {
            SetPlaybackSpeed(1, Direction);
        }

        InternalDrain((PlayerPolicy_t)PolicyPlayoutAlwaysDiscard, true);
        //
        // Find the current frames on display, and clamp the play
        // interval to ensure we don't go too far in the flip.
        //
        int Policy = Player->PolicyValue(this, PlayerAllStreams, PolicyClampPlaybackIntervalOnPlaybackDirectionChange);

        if (Policy == PolicyValueApply)
        {
            if (Direction == PlayForward)
            {
                PresentationIntervalReversalLimitStartNormalizedTime    = NormalizedTimeAtStartOfDrain;
            }
            else
            {
                PresentationIntervalReversalLimitEndNormalizedTime  = NormalizedTimeAtStartOfDrain;
            }
        }
    }

    //
    // Record the new speed and direction
    //
    mSpeed = Speed;
    mDirection = Direction;

    //
    // Specifically inform the output coordinator of the change
    //
    Status  = OutputCoordinator->SetPlaybackSpeed(PlaybackContext, Speed, Direction);
    if (Status != PlayerNoError)
    {
        SE_ERROR("Playback 0x%p failed to inform output coordinator of speed change\n", this);
        return Status;
    }

    OS_LockMutex(&mStreamListLock);

    for (Stream   = ListOfStreams; Stream  != NULL; Stream   = Stream->Next)
    {
        Stream->SetSpeed(Speed, Direction);
    }

    OS_UnLockMutex(&mStreamListLock);

    return PlayerNoError;
}

//
// Return a stream which has its service policy set as SECONDARY.
//
PlayerStream_t PlayerPlayback_s::GetSecondaryStream()
{
    OS_LockMutex(&mStreamListLock);

    PlayerStream_t Stream = ListOfStreams;
    while (Stream != NULL)
    {
        int AudioServiceType = Player->PolicyValue(this, Stream, PolicyAudioServiceType);

        switch (AudioServiceType)
        {
        case STM_SE_CTRL_VALUE_AUDIO_SERVICE_SECONDARY:
        case STM_SE_CTRL_VALUE_AUDIO_SERVICE_AUDIO_DESCRIPTION:
            SE_VERBOSE(group_player, "Stream 0x%p - %d - is secondary stream\n", Stream, Stream->GetStreamType());
            OS_UnLockMutex(&mStreamListLock);
            return Stream;

        default:
            break;
        }

        Stream = Stream->Next;
    }

    OS_UnLockMutex(&mStreamListLock);

    return NULL;
}

//
// Register a newly created stream.  Must be called once the stream is fully
// initialized as this make it visible to the rest of the system.
//
void PlayerPlayback_s::RegisterStream(PlayerStream_t Stream)
{
    SE_DEBUG(group_player, "Stream 0x%p - %d\n", Stream, Stream->GetStreamType());
    SE_ASSERT(Stream != NULL);

    OS_LockMutex(&mStreamListLock);

    Stream->Next = ListOfStreams;
    ListOfStreams = Stream;

    OS_LockMutex(&mTranslateTimeLock);
    mTranslateTimeStream = Stream;
    OS_UnLockMutex(&mTranslateTimeLock);

    OS_UnLockMutex(&mStreamListLock);
}

//
// Unregister a previously registered stream.  Do nothing if stream was not
// registered.
// TODO(theryn): Using a doubly-linked list would allow removal in O(1).
//
void PlayerPlayback_s::UnregisterStream(PlayerStream_t Stream)
{
    SE_DEBUG(group_player, "Stream 0x%p - %d\n", Stream, Stream->GetStreamType());
    SE_ASSERT(Stream != NULL);

    OS_LockMutex(&mStreamListLock);

    //
    // Remove from playback list.
    //
    bool Found = false;
    for (PlayerStream_t *PointerToStream = &ListOfStreams;
         (*PointerToStream) != NULL;
         PointerToStream = &((*PointerToStream)->Next))
    {
        if ((*PointerToStream) == Stream)
        {
            (*PointerToStream) = Stream->Next;
            Found = true;
            break;
        }
    }

    if (!Found)
    {
        OS_UnLockMutex(&mStreamListLock);

        // We end up here either because the stream was not registered in the first
        // place (failure during stream construction before registration point) or
        // because there is a genuine bug (corrupted list for example).
        SE_WARNING("Stream 0x%p - %d not in list (failure during stream construction?)\n",
                   Stream, Stream->GetStreamType());
        return;
    }

    //
    // If the playback no longer has any streams, we reset the
    // output coordinator refreshing the registration of the player.
    //
    if (ListOfStreams == NULL)
    {
        OutputCoordinator->Halt();
        OutputCoordinator->Reset();
        OutputCoordinator->RegisterPlayer(Player, this, PlayerAllStreams);
    }

    OS_LockMutex(&mTranslateTimeLock);
    mTranslateTimeStream = ListOfStreams;
    OS_UnLockMutex(&mTranslateTimeLock);

    OS_UnLockMutex(&mStreamListLock);
}
