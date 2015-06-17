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

#ifndef PLAYER_PLAYBACK_H
#define PLAYER_PLAYBACK_H

#include "player_types.h"
#include "player_playback_interface.h"
#include "player_generic.h"

// TODO(theryn): rename to PlayerPlayback_c
class PlayerPlayback_s: public PlayerPlaybackInterface_c
{
public:
    PlayerPlayback_s();
    ~PlayerPlayback_s();

    // PlayerPlaybackInterface_c methods
    virtual PlayerPlaybackStatistics_t     &GetStatistics() { return mStatistics; }
    virtual OS_Event_t                     &GetDrainSignalEvent() { return mDrainSignal; }
    virtual bool                            IsLowPowerState() { return mIsLowPowerState; }
    virtual void                            LowPowerEnter() { mIsLowPowerState = true; }
    virtual void                            LowPowerExit()  { mIsLowPowerState = false; }

    //

    PlayerStatus_t TranslateTimeNativeToNormalized(unsigned long long NativeTime,
                                                   unsigned long long *NormalizedTime,
                                                   PlayerTimeFormat_t NativeTimeFormat);
    PlayerStatus_t TranslateTimeNormalizedToNative(unsigned long long NormalizedTime,
                                                   unsigned long long *NativeTime,
                                                   PlayerTimeFormat_t NativeTimeFormat);

    PlayerStatus_t CheckForCodedBufferMismatch(PlayerStream_t Stream);
    PlayerStatus_t SetPresentationInterval(PlayerStream_t Stream,
                                           unsigned long long IntervalStartNativeTime,
                                           unsigned long long IntervalEndNativeTime);
    PlayerStatus_t InternalDrain(PlayerPolicy_t PlayoutPolicy,
                                 bool ParseAllFrames);
    PlayerStatus_t SetPlaybackSpeed(Rational_t Speed,
                                    PlayDirection_t Direction);
    PlayerStream_t GetSecondaryStream();

    void RegisterStream(PlayerStream_t Stream);
    void UnregisterStream(PlayerStream_t Stream);

    PlayerPlayback_t      Next;
    Player_Generic_t      Player;

    OutputCoordinator_t   OutputCoordinator;

    Rational_t            mSpeed;
    PlayDirection_t       mDirection;

    unsigned long long    PresentationIntervalReversalLimitStartNormalizedTime;
    unsigned long long    PresentationIntervalReversalLimitEndNormalizedTime;
    unsigned long long    RequestedPresentationIntervalStartNormalizedTime;
    unsigned long long    RequestedPresentationIntervalEndNormalizedTime;

    PlayerPolicyState_t   PolicyRecord;

    PlayerControslStorageState_t ControlsRecord;

    unsigned int          AudioCodedFrameCount;         // One set of these will be used whenever a stream is added
    unsigned int          AudioCodedMemorySize;
    unsigned int          AudioCodedFrameMaximumSize;
    char                  AudioCodedMemoryPartitionName[ALLOCATOR_MAX_PARTITION_NAME_SIZE];
    unsigned int          VideoCodedFrameCount;
    unsigned int          VideoCodedMemorySize;
    unsigned int          VideoCodedFrameMaximumSize;
    char                  VideoCodedMemoryPartitionName[ALLOCATOR_MAX_PARTITION_NAME_SIZE];
    unsigned int          OtherCodedFrameCount;
    unsigned int          OtherCodedMemorySize;
    unsigned int          OtherCodedFrameMaximumSize;
    char                  OtherCodedMemoryPartitionName[ALLOCATOR_MAX_PARTITION_NAME_SIZE];

    unsigned long long    LastNativeTime;

    /* Data shared with drain process */

    OS_Event_t            DrainStartStopEvent;
    bool                  DrainSignalThreadRunning;

private:
    OS_Event_t            mDrainSignal;
    FrameParser_t GetTranslateTimeFrameParser() const;

    bool                            mIsLowPowerState; // HPS/CPS: indicates when SE is in low power state
    PlayerPlaybackStatistics_t      mStatistics;

    // Protect ListOfStreams and could be used later on to protect other fields
    // of this class.  This mutex is deadlock-prone because it is held while
    // iterating the list and calling into other SE modules.
    //
    // A deadlock has notably been observed when:
    //      - One thread calls this->InternalDrain() which calls
    //      Collator_c::InputJump() for each stream thus acquiring
    //      mStreamListLock first then InputJumpLock.
    //      - Another thread calls Collator_c::Input() which calls
    //      this->TranslateTimeNativeToNormalized().  If the latter function
    //      acquires mStreamListLock, a deadlock occurs because
    //      mStreamListLock and InputJumpLock are acquired in reverse order.
    OS_Mutex_t            mStreamListLock;

    PlayerStream_t        ListOfStreams;

    // Stream used by time translation methods and associated lock.  Accessing
    // ListOfStreams at time translation time is not an option because of the
    // deadlock mentioned in mStreamListLock comment.  We therefore cache the
    // stream here and use a dedicated lock.  This lock can be held while
    // holding mStreamListLock.
    // TODO(theryn): Remove this hack when bug52608 "factor out time translation
    // from frame parser" implemented.
    OS_Mutex_t            mTranslateTimeLock;
    PlayerStream_t        mTranslateTimeStream;

    DISALLOW_COPY_AND_ASSIGN(PlayerPlayback_s);
};

#endif
