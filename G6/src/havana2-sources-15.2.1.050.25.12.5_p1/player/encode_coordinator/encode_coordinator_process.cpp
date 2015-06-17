/************************************************************************
Copyright (C) 2003-2013 STMicroelectronics. All Rights Reserved.

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

#include "encode_coordinator.h"

#undef TRACE_TAG
#define TRACE_TAG "EncodeCoordinator_c"

#define MAX_STARTCONDITION_WAIT   1000  // 1 second



void EncodeCoordinator_c::ProcessPendingFlush(void)
{
    for (unsigned int index = 0; index < MAX_ENCODE_COORDINATOR_STREAM_NB; index++)
    {
        if (mEncodeCoordinatorStream[index] != NULL)
        {
            mEncodeCoordinatorStream[index]->PerformPendingFlush();
        }
    }
}

void EncodeCoordinator_c::ProcessEncoderReturnedBuffer(void)
{
    for (unsigned int index = 0; index < MAX_ENCODE_COORDINATOR_STREAM_NB; index++)
    {
        if (mEncodeCoordinatorStream[index] != NULL)
        {
            mEncodeCoordinatorStream[index]->ProcessEncoderReturnedBuffer();
        }
    }
}

// Starting State processing
// Get the PTS of the first frame of all stream
// Set the higher PTS value as first pts for all streams
// Discard frames until each stream first frame has pts >= "first pts"
void EncodeCoordinator_c::StartingStateStep0Processing(void)
{
    SE_VERBOSE(group_encoder_stream, "\n");
    unsigned int    StreamCount = 0;

    // set it to maximum value
    uint64_t minFrameDuration  = (uint64_t) NRT_INACTIVE_SIGNAL_TIMEOUT;
    // We wait until each stream received a first frame
    uint64_t    highestCurrentTime = 0;
    bool    AllStreamReady = true;

    for (unsigned int index = 0; index < MAX_ENCODE_COORDINATOR_STREAM_NB; index++)
    {
        if (mEncodeCoordinatorStream[index] != NULL)
        {
            uint64_t        streamTime = 0;
            uint64_t        FrameDuration = NRT_INACTIVE_SIGNAL_TIMEOUT;
            StreamCount++;
            // Check first if we got the first frame for this stream
            if (mEncodeCoordinatorStream[index]->GetCurrentFrame())
            {
                // EOS frame are transmitted directly
                if (mEncodeCoordinatorStream[index]->TransmitEOS() == false)
                {
                    // and determine the highest PTS at the same time
                    mEncodeCoordinatorStream[index]->GetTime(&streamTime);
                    highestCurrentTime = max(streamTime, highestCurrentTime);
                    // ask for frame_duration to determine lowest value for mTimeOut
                    mEncodeCoordinatorStream[index]->GetFrameDuration(&FrameDuration);
                    //time out set to a frame duration in ms
                    minFrameDuration = min(minFrameDuration, (FrameDuration / 1000));
                    if (NotValidTime(mStartingStateInitialTime)) { mStartingStateInitialTime = OS_GetTimeInMilliSeconds(); }
                    SE_DEBUG(group_encoder_stream, "Got first frame for streamtime %lld highestCurrentTime %lld\n", streamTime, highestCurrentTime);
                }
                else { AllStreamReady = false; }
            }
            else
            {
                AllStreamReady = false;
            }
        }
    }

    // No stream connected, staying in Step0
    if (StreamCount == 0) { return; }

    // At least one stream has no data
    if (AllStreamReady == false)
    {
        // checking timeout
        uint64_t Now = OS_GetTimeInMilliSeconds();
        if (Now > (mStartingStateInitialTime + MAX_STARTCONDITION_WAIT))
        {
            // move to synchronised state with current streams
            mEncodeCoordinatorState = StartingStateStep1;
            mStartingStateInitialTime = Now;
            mCurrentTime = highestCurrentTime;
            mTimeOut = minFrameDuration;
            OS_SetEvent(&mInputStreamEvent);
            SE_WARNING("TimeOut Not all streams present, but move to Step1!\n");
        }
        return;
    }

    // Got a frame for all streams, switch to StartingStateStep1
    mEncodeCoordinatorState = StartingStateStep1;
    mStartingStateInitialTime = OS_GetTimeInMilliSeconds();
    mCurrentTime = highestCurrentTime;
    mTimeOut = minFrameDuration;
    OS_SetEvent(&mInputStreamEvent);
    SE_DEBUG(group_encoder_stream, "Got first frame for all streams (highestCurrentTime %lld), move to StartingStateStep1 with timeout set to %lld\n", highestCurrentTime, mTimeOut);
}

//
// Discard frames until each stream first frame has pts >= "first pts"
//
void EncodeCoordinator_c::StartingStateStep1Processing(void)
{
    SE_VERBOSE(group_encoder_stream, "\n");

    // Here we assume all streams got a first frame
    // We try to discard all frames under highestCurrentTime
    bool    AllStreamReady = true;
    for (unsigned index = 0; index < MAX_ENCODE_COORDINATOR_STREAM_NB; index++)
    {
        if (mEncodeCoordinatorStream[index] != NULL)
        {
            // Ask each stream if it succeeds to discard frames until highestCurrentTime
            if (mEncodeCoordinatorStream[index]->DiscardUntil(mCurrentTime) == false)
            {
                SE_DEBUG(group_encoder_stream, "Didn't Get all streams synchronized at time %lld\n", mCurrentTime);
                // At least one stream is not yet ready
                AllStreamReady = false;
            }
        }
    }

    if (AllStreamReady == false)
    {
        uint64_t Now = OS_GetTimeInMilliSeconds();
        if (Now > (mStartingStateInitialTime + MAX_STARTCONDITION_WAIT))
        {
            // We didn't manage to achieve synchronization of all streams before time-out
            // Force move to RunningState for valid streams
            mEncodeCoordinatorState = RunningState;
            OS_SetEvent(&mInputStreamEvent);
            SE_WARNING("Unable to achieve starting step1 synchronization, move to RunningState\n");
        }
        return;
    }

    // All streams are now synchronized, switch to RunningState and wake-up thread
    mEncodeCoordinatorState = RunningState;
    OS_SetEvent(&mInputStreamEvent);
    SE_DEBUG(group_encoder_stream, "Got all streams synchronized, move to RunningState\n");
}


bool EncodeCoordinator_c::StreamsHaveGap(uint64_t *StreamsGapEndTime)
{
    // Check for Gap present on all streams
    bool        AllStreamsInGap = true;
    uint64_t mindelta = (uint64_t)(-1);

    *StreamsGapEndTime = (uint64_t)(-1);

    for (unsigned int index = 0; index < MAX_ENCODE_COORDINATOR_STREAM_NB; index++)
    {
        uint64_t   GapEndTime = (uint64_t)(-1);

        if (mEncodeCoordinatorStream[index] != NULL)
        {
            if (mEncodeCoordinatorStream[index]->HasGap(mCurrentTime, &GapEndTime))
            {
                int64_t  delta = GapEndTime - mCurrentTime;
                if (delta < 0) { delta = delta + PTSMODULO; }
                // search for the nearest end of GAP
                if (delta < mindelta)
                {
                    *StreamsGapEndTime = GapEndTime;
                    mindelta = delta;
                }
            }
            else { AllStreamsInGap = false; }
        }
    }
    return AllStreamsInGap;
}

bool EncodeCoordinator_c::StreamsUpdateTimeCompression(uint64_t EndOfGapTime)
{
    // Determine the max frame duration and the most advanced stream
    uint64_t     maxFrameDuration = 0;
    uint64_t     maxStreamTime = 0;

    for (unsigned int index = 0; index < MAX_ENCODE_COORDINATOR_STREAM_NB; index++)
    {
        uint64_t     streamFrameDuration = 0;
        if (mEncodeCoordinatorStream[index] != NULL)
        {
            if (mEncodeCoordinatorStream[index]->GetFrameDuration(&streamFrameDuration) == true)
            {
                uint64_t    t;
                maxFrameDuration = max(streamFrameDuration, maxFrameDuration);
                if (mEncodeCoordinatorStream[index]->GetNextTime(&t)) { maxStreamTime = max(t, maxStreamTime); }
            }
        }
    }

    int64_t curOffset = EndOfGapTime - maxStreamTime;
    uint64_t absCurOffset = (curOffset > 0) ? curOffset : -curOffset;

    // Gap detected if greater than maximum frame duration
    if ((maxFrameDuration > 0)  && (absCurOffset > maxFrameDuration))
    {

        curOffset = (curOffset / (int64_t) maxFrameDuration) * (int64_t) maxFrameDuration;
        mPtsOffset   = mPtsOffset - curOffset;

        SE_DEBUG(group_encoder_stream, "Whole stream Gap at time %lld to %lld  ptsOffset %lld mPtsOffset %lld maxSTreamTime %lld\n", mCurrentTime, EndOfGapTime, -curOffset, mPtsOffset, maxStreamTime);
        mCurrentTime = EndOfGapTime;

        for (unsigned int index = 0; index < MAX_ENCODE_COORDINATOR_STREAM_NB; index++)
        {
            if (mEncodeCoordinatorStream[index] != NULL)
            {
                mEncodeCoordinatorStream[index]->AdjustJump(mCurrentTime);
            }
        }
        return true;
    }

    return false;
}

bool EncodeCoordinator_c::StreamsReady(void)
{
    for (unsigned int index = 0; index < MAX_ENCODE_COORDINATOR_STREAM_NB; index++)
    {
        if (mEncodeCoordinatorStream[index] != NULL)
        {
            mEncodeCoordinatorStream[index]->ComputeState(mCurrentTime);
        }
    }

    for (unsigned int index = 0; index < MAX_ENCODE_COORDINATOR_STREAM_NB; index++)
    {
        if (mEncodeCoordinatorStream[index] != NULL)
        {
            if (mEncodeCoordinatorStream[index]->IsReady(mCurrentTime) == false)
            {
                // At least one stream is not ready yet
                return false;
            }
        }
    }

    // all stream ready , now check for timeout
    for (unsigned int index = 0; index < MAX_ENCODE_COORDINATOR_STREAM_NB; index++)
    {
        if (mEncodeCoordinatorStream[index] != NULL)
        {
            if (mEncodeCoordinatorStream[index]->IsTimeOut(mCurrentTime) == false)
            {
                // At least one stream is not in time out , so streams are considered as ready
                return true;
            }
        }
    }

    return false;
}

void EncodeCoordinator_c::StreamsEncode(void)
{
    for (unsigned int index = 0; index < MAX_ENCODE_COORDINATOR_STREAM_NB; index++)
    {
        if (mEncodeCoordinatorStream[index] != NULL)
        {
            // Ask stream to encode frame
            mEncodeCoordinatorStream[index]->Encode(mCurrentTime);
            mEncodeCoordinatorStream[index]->UpdateState();
        }
    }
}

bool EncodeCoordinator_c::StreamsGetNextCurrentTime(uint64_t *nextCurrentTime)
{
    *nextCurrentTime = (uint64_t) - 1;
    bool         getTime = false;
    for (unsigned int index = 0; index < MAX_ENCODE_COORDINATOR_STREAM_NB; index++)
    {
        uint64_t     streamTime = 0;

        if (mEncodeCoordinatorStream[index] != NULL)
        {
            if (mEncodeCoordinatorStream[index]->GetNextTime(&streamTime) == true)
            {
                *nextCurrentTime = min(streamTime, *nextCurrentTime);
                getTime = true;
            }
        }
    }
    return getTime;
}


// Running State processing
void EncodeCoordinator_c::RunningStateProcessing(void)
{
    SE_VERBOSE(group_encoder_stream, "\n");

    // Loops on all streams to check that all streams are ready for the CurrenTime
    // Ready means either a buffer is available to be transmitted or a buffer covering
    // this time has already been sent, or the stream is switched off
    if (StreamsReady() == false) { return; }


    // Check for Gap present on all streams
    uint64_t    EndOfGapTime;

    if (StreamsHaveGap(&EndOfGapTime))
    {
        if (EndOfGapTime != mCurrentTime)
        {
            if (StreamsUpdateTimeCompression(EndOfGapTime)) { return; }
        }
    }

    // At that stage some streams have data, so extract the buffers and transmit to encode
    StreamsEncode();

    // Determine next CurrentTime
    uint64_t    nextCurrentTime;
    if (StreamsGetNextCurrentTime(&nextCurrentTime))
    {
        // Update CurrentTime
        mCurrentTime = nextCurrentTime;
        SE_VERBOSE(group_encoder_stream, "nextCurrentTime %lld\n", nextCurrentTime);
    }
}

// EncodeCoordinator thread
void EncodeCoordinator_c::ProcessEncodeCoordinator(void)
{
    SE_INFO(group_encoder_stream, "thread started\n");

    // Signal thread has started
    OS_SetEvent(&mStartStopEvent);

    //
    // Main Loop
    //
    while (!mTerminating)
    {
        OS_WaitForEventAuto(&mInputStreamEvent, mTimeOut);
        OS_ResetEvent(&mInputStreamEvent);

        SE_EXTRAVERB(group_encoder_stream, "wake up\n");

        // Lock access to mEncodeCoordinatorStream[] table
        OS_LockMutex(&mLock);

        // check and manage if buffers have been returned by encoder
        ProcessEncoderReturnedBuffer();

        // check and manage if a flush has to be performed
        ProcessPendingFlush();

        uint64_t    nextCurrentTime;
        if (StreamsGetNextCurrentTime(&nextCurrentTime) == false)
        {
            // Handle cases where there are no more active streams: EOS or flush
            // Go back to StartingState0
            SE_DEBUG(group_encoder_stream, "No more active stream, move to StartingStateStep0\n");
            mTimeOut = NRT_INACTIVE_SIGNAL_TIMEOUT;
            mEncodeCoordinatorState = StartingStateStep0;
            mStartingStateInitialTime = INVALID_TIME;
            mPtsOffset = 0;
        }

        // do normal processing
        switch (mEncodeCoordinatorState)
        {
        case StartingStateStep0:
            StartingStateStep0Processing();
            break;

        case StartingStateStep1:
            StartingStateStep1Processing();
            break;

        case RunningState:
            RunningStateProcessing();
            break;

        default:
            SE_ERROR("Invalid EncodeCoordinatorState %d\n", mEncodeCoordinatorState);
            break;
        }

        //check again
        ProcessEncoderReturnedBuffer();

        // Unlock access to mEncodeCoordinatorStream[] table
        OS_UnLockMutex(&mLock);
    }

    OS_Smp_Mb(); // Read memory barrier: rmb_for_EncodeCoordinator_Terminating coupled with: wmb_for_EncodeCoordinator_Terminating

    // Signal we have terminated
    OS_SetEvent(&mStartStopEvent);
    SE_INFO(group_encoder_stream, "thread terminated\n");
}




