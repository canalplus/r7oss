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

#ifndef H_ES_PROCESSOR_BASE
#define H_ES_PROCESSOR_BASE

#include "es_processor.h"
#include "es_processor_state.h"

class ES_Processor_Base_c : public ES_Processor_c
{
public:
    ES_Processor_Base_c(void)
        : mPort(NULL)
        , mStream(NULL)
        , mState(&mProcessingStateNoDiscardSet)
        , mLock()
        , mProcessingStateNoDiscardSet(*this)
        , mProcessingStateStartDiscardSet(*this)
        , mProcessingStateStartEndDiscardSet(*this)
        , mDiscardingStateEndDiscardSet(*this)
        , mDiscardingStateNoEndDiscardSet(*this)
        , mStartTrigger()
        , mEndTrigger()
        , mAlarm()
        , mPtsOffset(0)
        , mLastPts(0)
    {
        OS_InitializeMutex(&mLock);
    }

    virtual ~ES_Processor_Base_c(void) { OS_TerminateMutex(&mLock); }

    virtual PlayerStatus_t FinalizeInit(PlayerStream_t Stream);
    virtual PlayerStatus_t Connect(Port_c *Port);
    virtual PlayerStatus_t SetDiscardTrigger(stm_se_play_stream_discard_trigger_t const &trigger);
    virtual PlayerStatus_t Reset()
    {
        PlayerStatus_t Status;
        OS_LockMutex(&mLock);
        Status = mState->Reset();
        OS_UnLockMutex(&mLock);
        return Status;
    }
    virtual PlayerStatus_t SetAlarm(bool enable, stm_se_play_stream_pts_and_tolerance_t const &config);

    // Port_c Interface implementation
    virtual RingStatus_t Insert(uintptr_t  Value)
    {
        RingStatus_t Status;
        OS_LockMutex(&mLock);
        Status = mState->Insert(Value);
        OS_UnLockMutex(&mLock);
        return Status;
    }
    RingStatus_t InsertFromIRQ(uintptr_t Value) { SE_ASSERT(0); return RingNoError; }
    virtual RingStatus_t Extract(uintptr_t *Value, unsigned int   BlockingPeriod) { return RingNoError; }
    virtual RingStatus_t Flush(void) { return RingNoError; }
    virtual bool         NonEmpty(void)
    {
        bool isNonEmpty;
        OS_LockMutex(&mLock);
        isNonEmpty = mState->NonEmpty();
        OS_UnLockMutex(&mLock);
        return isNonEmpty;
    }

private:
    friend class ES_Processor_State_c;
    friend class ES_ProcessorProcessingState_NoDiscardSet_c;
    friend class ES_ProcessorProcessingState_StartDiscardSet_c;
    friend class ES_ProcessorProcessingState_StartEndDiscardSet_c;
    friend class ES_ProcessorDiscardingState_EndDiscardSet_c;
    friend class ES_ProcessorDiscardingState_NoEndDiscardSet_c;

    // Private data
    Port_c                                           *mPort;
    PlayerStream_t                                    mStream;
    ES_Processor_State_c                             *mState;
    OS_Mutex_t                                        mLock;

    // Store one instance of each state
    ES_ProcessorProcessingState_NoDiscardSet_c        mProcessingStateNoDiscardSet;
    ES_ProcessorProcessingState_StartDiscardSet_c     mProcessingStateStartDiscardSet;
    ES_ProcessorProcessingState_StartEndDiscardSet_c  mProcessingStateStartEndDiscardSet;
    ES_ProcessorDiscardingState_EndDiscardSet_c       mDiscardingStateEndDiscardSet;
    ES_ProcessorDiscardingState_NoEndDiscardSet_c     mDiscardingStateNoEndDiscardSet;

    // Triggers
    ES_Processor_Trigger_c                            mStartTrigger;
    ES_Processor_Trigger_c                            mEndTrigger;

    // Alarm
    ES_Processor_Alarm_c                              mAlarm;

    // PTS Offset
    signed long long                                  mPtsOffset;

    //PTS of last frame inserted in output port
    unsigned long long                    mLastPts;

    // Private methods
    void ChangeState(ES_Processor_State_c *NewState);
    PlayerStatus_t CheckDiscardTrigger(stm_se_play_stream_discard_trigger_t const &trigger);

    DISALLOW_COPY_AND_ASSIGN(ES_Processor_Base_c);
};

#endif // H_ES_PROCESSOR_BASE
