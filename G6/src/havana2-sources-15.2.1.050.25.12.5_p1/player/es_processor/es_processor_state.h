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

#ifndef H_ES_PROCESSOR_STATE
#define H_ES_PROCESSOR_STATE

#include "es_processor.h"
#include "player_generic.h"
#include "player_playback.h"
#include "player_stream.h"

class ES_Processor_Trigger_c
{
public:
    ES_Processor_Trigger_c(void)
        : mType(STM_SE_PLAY_STREAM_CANCEL_TRIGGER)
        , mPts(0)
        , mTolerance(0)
        , mIsStart(false)
    {}

    void Set(stm_se_play_stream_discard_trigger_t const &trigger)
    {
        mType = trigger.type;
        mPts  = trigger.u.pts_trigger.pts;
        mTolerance = trigger.u.pts_trigger.tolerance;
        mIsStart = trigger.start_not_end;
    }

    void Reset(void)
    {
        mType = STM_SE_PLAY_STREAM_CANCEL_TRIGGER;
        mPts  = 0;
        mTolerance = 0;
        mIsStart = false;
    }

    stm_se_play_stream_trigger_type_t GetType(void) { return mType; }
    uint64_t GetPts(void) { return mPts; }
    uint32_t GetTolerance(void) {return mTolerance; }
    bool IsStartTrigger(void) { return mIsStart; }

private:
    stm_se_play_stream_trigger_type_t mType;
    uint64_t mPts;
    uint32_t mTolerance;
    bool mIsStart;
};

class ES_Processor_Alarm_c
{
public:
    ES_Processor_Alarm_c(void)
        : mPts(0)
        , mTolerance(0)
        , mIsEnabled(false)
    {}

    void Set(stm_se_play_stream_pts_and_tolerance_t const &config)
    {
        mPts  = config.pts;
        mTolerance = config.tolerance;
        mIsEnabled = true;
    }

    void Reset(void)
    {
        mPts  = 0;
        mTolerance = 0;
        mIsEnabled = false;
    }

    uint64_t GetPts(void) { return mPts; }
    uint32_t GetTolerance(void) { return mTolerance; }
    bool IsEnabled(void) { return mIsEnabled; }

private:
    uint64_t mPts;
    uint32_t mTolerance;
    bool mIsEnabled;
};

class ES_Processor_Base_c;

class ES_Processor_State_c
{
public:
    explicit ES_Processor_State_c(ES_Processor_Base_c &esProcessor)
        : mEsProcessor(esProcessor)
    {}
    virtual ~ES_Processor_State_c(void) {}

    // User api call
    virtual PlayerStatus_t SetDiscardTrigger(stm_se_play_stream_discard_trigger_t const &trigger) = 0;
    virtual PlayerStatus_t Reset(void);

    // Port_c Interface implementation
    virtual RingStatus_t Insert(uintptr_t  Value) = 0;
    virtual bool         NonEmpty(void) = 0;

protected:
    ES_Processor_Base_c &mEsProcessor;

    void ChangeState(ES_Processor_State_c *NewState);
    PlayerStatus_t NotifyTriggerDetection(ES_Processor_Trigger_c *Trigger);
    PlayerStatus_t CheckTrigger(Buffer_t Buffer, ES_Processor_Trigger_c *Trigger, bool *IsDetected);
    PlayerStatus_t NotifyAlarm(unsigned long long NativePlaybackTime);
    PlayerStatus_t CheckAlarm(Buffer_t Buffer);
    void UpdatePtsOffset(PlayerSequenceNumber_t *SequenceNumberStructure);
    void ApplyPtsOffset(Buffer_t Buffer);
    bool DetectSplicingMarker(Buffer_t Buffer);
    bool DetectDiscontinuity(Buffer_t Buffer);
    RingStatus_t InsertInOutputPort(Buffer_t Buffer);
    PlayerStatus_t CommonStateChecks(Buffer_t Buffer, bool *IsSplicingMarker, bool *IsDiscontinuity);
    void CheckPtsDiscontinuity(Buffer_t Buffer);
    void ReleaseInputBuffer(Buffer_t Buffer);
    void TraceBufferPTS(Buffer_t Buffer);

private:
    DISALLOW_COPY_AND_ASSIGN(ES_Processor_State_c);
};

// Processing States
class ES_ProcessorProcessingState_NoDiscardSet_c : public ES_Processor_State_c
{
public:
    explicit ES_ProcessorProcessingState_NoDiscardSet_c(ES_Processor_Base_c &esProcessor)
        : ES_Processor_State_c(esProcessor)
    {}
    virtual ~ES_ProcessorProcessingState_NoDiscardSet_c(void) {}

    // User api call
    virtual PlayerStatus_t SetDiscardTrigger(stm_se_play_stream_discard_trigger_t const &trigger);
    virtual PlayerStatus_t Reset(void);

    // Port_c Interface implementation
    virtual RingStatus_t Insert(uintptr_t  Value);
    virtual bool         NonEmpty(void);
};

class ES_ProcessorProcessingState_StartDiscardSet_c : public ES_Processor_State_c
{
public:
    explicit ES_ProcessorProcessingState_StartDiscardSet_c(ES_Processor_Base_c &esProcessor)
        : ES_Processor_State_c(esProcessor)
    {}
    virtual ~ES_ProcessorProcessingState_StartDiscardSet_c(void) {}

    // User api call
    virtual PlayerStatus_t SetDiscardTrigger(stm_se_play_stream_discard_trigger_t const &trigger);

    // Port_c Interface implementation
    virtual RingStatus_t Insert(uintptr_t  Value);
    virtual bool         NonEmpty(void);
};

class ES_ProcessorProcessingState_StartEndDiscardSet_c : public ES_Processor_State_c
{
public:
    explicit ES_ProcessorProcessingState_StartEndDiscardSet_c(ES_Processor_Base_c &esProcessor)
        : ES_Processor_State_c(esProcessor)
    {}
    virtual ~ES_ProcessorProcessingState_StartEndDiscardSet_c(void) {}

    // User api call
    virtual PlayerStatus_t SetDiscardTrigger(stm_se_play_stream_discard_trigger_t const &trigger);

    // Port_c Interface implementation
    virtual RingStatus_t Insert(uintptr_t  Value);
    virtual bool         NonEmpty(void);
};

//Discarding States
class ES_ProcessorDiscardingState_EndDiscardSet_c : public ES_Processor_State_c
{
public:
    explicit ES_ProcessorDiscardingState_EndDiscardSet_c(ES_Processor_Base_c &esProcessor)
        : ES_Processor_State_c(esProcessor)
    {}
    virtual ~ES_ProcessorDiscardingState_EndDiscardSet_c(void) {}

    // User api call
    virtual PlayerStatus_t SetDiscardTrigger(stm_se_play_stream_discard_trigger_t const &trigger);

    // Port_c Interface implementation
    virtual RingStatus_t Insert(uintptr_t  Value);
    virtual bool         NonEmpty(void);
};

class ES_ProcessorDiscardingState_NoEndDiscardSet_c : public ES_Processor_State_c
{
public:
    explicit  ES_ProcessorDiscardingState_NoEndDiscardSet_c(ES_Processor_Base_c &esProcessor)
        : ES_Processor_State_c(esProcessor)
    {}
    virtual ~ES_ProcessorDiscardingState_NoEndDiscardSet_c(void) {}

    // User api call
    virtual PlayerStatus_t SetDiscardTrigger(stm_se_play_stream_discard_trigger_t const &trigger);

    // Port_c Interface implementation
    virtual RingStatus_t Insert(uintptr_t  Value);
    virtual bool         NonEmpty(void);
};

#endif // H_ES_PROCESSOR_STATE
