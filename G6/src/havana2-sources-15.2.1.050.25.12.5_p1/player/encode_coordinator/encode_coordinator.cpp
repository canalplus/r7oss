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

#include "player_threads.h"

#include "encode_coordinator.h"

#undef TRACE_TAG
#define TRACE_TAG "EncodeCoordinator_c"


OS_TaskEntry(EncoderCoordinatorProcess)
{
    EncodeCoordinator_c      *EncodeCoordinator = (EncodeCoordinator_c *)Parameter;
    EncodeCoordinator->ProcessEncodeCoordinator();
    OS_TerminateThread();
    return NULL;
}

EncodeCoordinator_c::EncodeCoordinator_c(void)
    : mEncodeCoordinatorStreamNb(0)
    , mEncodeCoordinatorStream()
    , mLock()
    , mStartStopEvent()
    , mInputStreamEvent()
    , mEncodeCoordinatorState(StartingStateStep0)
    , mTimeOut(NRT_INACTIVE_SIGNAL_TIMEOUT)
    , mCurrentTime(0)
    , mStartingStateInitialTime(INVALID_TIME)
    , mPtsOffset(0)
    , mTerminating(false)
{
    OS_InitializeMutex(&mLock);
    OS_InitializeEvent(&mStartStopEvent);
    OS_InitializeEvent(&mInputStreamEvent);
}

// FinalizeInit() function, to be called after the constructor
// Allocates all the resources needeed by the EncodeCoordinator object and creates the thread
EncoderStatus_t EncodeCoordinator_c::FinalizeInit(void)
{
    SE_DEBUG(group_encoder_stream, "\n");

    // Create EncodeCoordinator thread
    OS_ResetEvent(&mStartStopEvent);
    OS_Thread_t Thread;
    OS_Status_t OSStatus = OS_CreateThread(&Thread, EncoderCoordinatorProcess, this, &player_tasks_desc[SE_TASK_ENCOD_COORD]);
    if (OSStatus != OS_NO_ERROR)
    {
        SE_ERROR("Failed to create EncodeCoordinator thread\n");
        return EncoderError;
    }

    // Wait for thread to be running
    OS_Status_t WaitStatus;
    do
    {
        WaitStatus = OS_WaitForEventAuto(&mStartStopEvent, ENCODE_COORDINATOR_MAX_EVENT_WAIT);
        if (WaitStatus == OS_TIMED_OUT)
        {
            SE_WARNING("Still waiting for EncodeCoordinator process to run\n");
        }
    }
    while (WaitStatus == OS_TIMED_OUT);

    // Init OK
    return EncoderNoError;
}

// Halt() function, to be called before the destructor
// Blocking until thread termination
void EncodeCoordinator_c::Halt(void)
{
    SE_DEBUG(group_encoder_stream, "\n");

    // In case not already disconnected, force termination of all EncodeCoordinatorStream
    int index;
    for (index = 0; index < MAX_ENCODE_COORDINATOR_STREAM_NB; index++)
    {
        if (mEncodeCoordinatorStream[index] != NULL)
        {
            mEncodeCoordinatorStream[index]->Flush();
            mEncodeCoordinatorStream[index]->Halt();
            delete mEncodeCoordinatorStream[index];
            mEncodeCoordinatorStream[index] = NULL;
            mEncodeCoordinatorStreamNb --;
        }
    }

    // Ask threads to terminate
    OS_ResetEvent(&mInputStreamEvent);
    OS_ResetEvent(&mStartStopEvent);
    OS_Smp_Mb(); // Write memory barrier: wmb_for_EncodeCoordinator_Terminating coupled with: rmb_for_EncodeCoordinator_Terminating
    mTerminating = true;

    // Signal fake input event to wake-up the thread
    OS_SetEvent(&mInputStreamEvent);

    // Wait for thread to be terminated
    OS_Status_t WaitStatus;
    do
    {
        WaitStatus = OS_WaitForEventAuto(&mStartStopEvent, ENCODE_COORDINATOR_MAX_EVENT_WAIT);
        if (WaitStatus == OS_TIMED_OUT)
        {
            SE_WARNING("Still waiting for EncodeCoordinator process to terminate\n");
        }
    }
    while (WaitStatus == OS_TIMED_OUT);
}

EncodeCoordinator_c::~EncodeCoordinator_c(void)
{
    SE_DEBUG(group_encoder_stream, "\n");

    // Release remaining allocated resources
    OS_TerminateEvent(&mInputStreamEvent);
    OS_TerminateEvent(&mStartStopEvent);
    OS_TerminateMutex(&mLock);
}

EncoderStatus_t EncodeCoordinator_c::Connect(EncodeStreamInterface_c  *EncodeStream,
                                             ReleaseBufferInterface_c *OriginalReleaseBufferItf,
                                             ReleaseBufferInterface_c **EncodeCoordinatatorStreamReleaseBufferItf,
                                             Port_c **InputPort)
{
    SE_DEBUG(group_encoder_stream, "\n");
    int index;

    OS_LockMutex(&mLock);
    for (index = 0; index < MAX_ENCODE_COORDINATOR_STREAM_NB; index++)
    {
        if (mEncodeCoordinatorStream[index] == NULL)
        {
            break;
        }
    }

    if (index >= MAX_ENCODE_COORDINATOR_STREAM_NB)
    {
        SE_ERROR("Failed to add a new Stream to EncodeCoordinator\n");
        OS_UnLockMutex(&mLock);
        return EncoderError;
    }

    // Create new EncodeCoordinatorSTream
    mEncodeCoordinatorStream[index] = new EncodeCoordinatorStream_c(EncodeStream, this, OriginalReleaseBufferItf);
    if (mEncodeCoordinatorStream[index] == NULL)
    {
        SE_ERROR("Failed to allocate new EncodeCoordinatorStream\n");
        OS_UnLockMutex(&mLock);
        return EncoderError;
    }

    if (mEncodeCoordinatorStream[index]->FinalizeInit() != EncoderNoError)
    {
        SE_ERROR("Failed to initialise the new EncodeCoordinatorStream\n");
        delete mEncodeCoordinatorStream[index];
        mEncodeCoordinatorStream[index] = NULL;
        OS_UnLockMutex(&mLock);
        return EncoderError;
    }
    mEncodeCoordinatorStreamNb ++;

    *InputPort = mEncodeCoordinatorStream[index];
    *EncodeCoordinatatorStreamReleaseBufferItf = mEncodeCoordinatorStream[index];
    OS_UnLockMutex(&mLock);

    return EncoderNoError;
}

EncoderStatus_t EncodeCoordinator_c::Disconnect(EncodeStreamInterface_c *EncodeStream)
{
    SE_DEBUG(group_encoder_stream, "\n");
    int index;

    // Flush the encode stream
    Flush(EncodeStream);

    // Look for corresponding EncodeCoordinatorStream object
    OS_LockMutex(&mLock);
    for (index = 0; index < MAX_ENCODE_COORDINATOR_STREAM_NB; index++)
    {
        if ((mEncodeCoordinatorStream[index] != NULL)
            && (mEncodeCoordinatorStream[index]->GetEncodeStream() == EncodeStream))
        {
            break;
        }
    }

    if (index >= MAX_ENCODE_COORDINATOR_STREAM_NB)
    {
        SE_ERROR("Cannot find stream (%p)\n", EncodeStream);
        OS_UnLockMutex(&mLock);
        return EncoderError;
    }

    // Delete EncodeCoordinatorStream_c object
    mEncodeCoordinatorStream[index]->Halt();
    delete mEncodeCoordinatorStream[index];
    mEncodeCoordinatorStream[index] = NULL;
    mEncodeCoordinatorStreamNb --;
    OS_UnLockMutex(&mLock);

    return EncoderNoError;
}

EncoderStatus_t EncodeCoordinator_c::Flush(EncodeStreamInterface_c *EncodeStream)
{
    SE_DEBUG(group_encoder_stream, "\n");
    int index;

    // Look for corresponding EncodeCoordinatorStream object
    for (index = 0; index < MAX_ENCODE_COORDINATOR_STREAM_NB; index++)
    {
        if ((mEncodeCoordinatorStream[index] != NULL)
            && (mEncodeCoordinatorStream[index]->GetEncodeStream() == EncodeStream))
        {
            break;
        }
    }

    if (index >= MAX_ENCODE_COORDINATOR_STREAM_NB)
    {
        SE_ERROR("Cannot find stream (%p)\n", EncodeStream);
        return EncoderError;
    }

    // Flush the encode stream
    RingStatus_t RingStatus;
    RingStatus = mEncodeCoordinatorStream[index]->Flush();
    if (RingStatus != RingNoError)
    {
        SE_ERROR("Failed to flush stream (%p) index %d - status %d\n", EncodeStream, index, RingStatus);
        return EncoderError;
    }

    return EncoderNoError;
}
