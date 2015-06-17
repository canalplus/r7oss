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
#include "es_processor_base.h"
#include "es_processor_state.h"

#undef TRACE_TAG
#define TRACE_TAG "ES_Processor_Base_c"

PlayerStatus_t ES_Processor_Base_c::FinalizeInit(PlayerStream_t Stream)
{
    if (Stream == NULL)
    {
        SE_ERROR("Incorrect parameter\n");
        return PlayerError;
    }
    SE_DEBUG(group_esprocessor, "Stream 0x%p Type %d\n", Stream, Stream->GetStreamType());
    mStream = Stream;
    return PlayerNoError;
}

PlayerStatus_t ES_Processor_Base_c::Connect(Port_c *Port)
{
    if (Port == NULL)
    {
        SE_ERROR("Stream 0x%pIncorrect parameter\n", mStream);
        return PlayerError;
    }
    OS_LockMutex(&mLock);
    mPort = Port;
    OS_UnLockMutex(&mLock);

    SE_DEBUG(group_esprocessor, "Stream 0x%p mPort 0x%p\n", mStream, mPort);
    return PlayerNoError;
}

PlayerStatus_t ES_Processor_Base_c::SetDiscardTrigger(stm_se_play_stream_discard_trigger_t const &trigger)
{
    SE_VERBOSE(group_esprocessor, "Stream 0x%p\n", mStream);
    PlayerStatus_t Status;
    Status = CheckDiscardTrigger(trigger);
    if (Status != PlayerNoError) { return Status; }
    OS_LockMutex(&mLock);
    Status = mState->SetDiscardTrigger(trigger);
    OS_UnLockMutex(&mLock);
    return Status;
}

PlayerStatus_t ES_Processor_Base_c::SetAlarm(bool enable, stm_se_play_stream_pts_and_tolerance_t const &config)
{
    OS_LockMutex(&mLock);
    if (enable)
    {
        SE_DEBUG(group_esprocessor, "Stream 0x%p PTS %llu Tolerance %u\n", mStream, config.pts, config.tolerance);
        mAlarm.Set(config);
    }
    else
    {
        SE_DEBUG(group_esprocessor, "Stream 0x%p Disable Alarm\n", mStream);
        mAlarm.Reset();
    }
    OS_UnLockMutex(&mLock);
    return PlayerNoError;
}

PlayerStatus_t ES_Processor_Base_c::CheckDiscardTrigger(stm_se_play_stream_discard_trigger_t const &trigger)
{
    SE_VERBOSE(group_esprocessor, "Stream 0x%p\n", mStream);
    PlayerStatus_t Status;
    switch (trigger.type)
    {
    case STM_SE_PLAY_STREAM_CANCEL_TRIGGER:
    case STM_SE_PLAY_STREAM_PTS_TRIGGER:
    case STM_SE_PLAY_STREAM_SPLICING_MARKER_TRIGGER:
        Status = PlayerNoError;
        break;
    default:
        Status = PlayerError;
        SE_ERROR("Stream 0x%p Incorrect trigger type\n", mStream);
        break;
    }
    return Status;
}

