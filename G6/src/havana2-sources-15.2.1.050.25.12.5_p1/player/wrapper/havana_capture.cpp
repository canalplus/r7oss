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

#include "player_threads.h"

#include "havana_player.h"
#include "manifestor_base.h"
#include "manifestor_video.h"
#include "manifestor_video_grab.h"
#include "havana_capture.h"

//{{{  Thread entry stub
static OS_TaskEntry(BufferCaptureThreadStub)
{
    HavanaCapture_c *HavanaCapture  = (HavanaCapture_c *)Parameter;
    HavanaCapture->BufferCaptureThread();
    OS_TerminateThread();
    return NULL;
}
//}}}

//{{{  HavanaCapture_c
HavanaCapture_c::HavanaCapture_c(void)
    : Lock()
    , Manifestor(NULL)
    , CaptureBuffers()
    , BufferIn(0)
    , BufferOut(0)
    , BufferForRelease(NULL)
    , BufferReceived()
    , BufferCaptureThreadTerminated()
    , BufferCaptureThreadRunning(false)
    , BufferCaptureCallback(NULL)
    , CallbackContext(NULL)
    , Latency(CAPTURE_DEFAULT_LATENCY)
{
    SE_VERBOSE(group_havana, "this = %p\n", this);

    OS_InitializeMutex(&Lock);
    OS_InitializeEvent(&BufferCaptureThreadTerminated);
    OS_InitializeEvent(&BufferReceived);
}
//}}}
//{{{  ~HavanaCapture_c
HavanaCapture_c::~HavanaCapture_c(void)
{
    SE_VERBOSE(group_havana, "this = %p\n", this);

    if (BufferCaptureThreadRunning)
    {
        SE_ERROR("Capture class has not been DeInitialised!\n");
    }

    OS_TerminateMutex(&Lock);
    OS_TerminateEvent(&BufferReceived);
    OS_TerminateEvent(&BufferCaptureThreadTerminated);
}
//}}}
//{{{  Init
HavanaStatus_t HavanaCapture_c::Init(class Manifestor_Base_c   *Manifestor)
{
    OS_Thread_t Thread;

    SE_VERBOSE(group_havana, "\n");
    BufferIn            = 0;
    BufferOut           = 0;
    BufferForRelease    = NULL;
    Latency             = CAPTURE_DEFAULT_LATENCY;      // normally 2 frames at 50fps

    // Create buffer capture thread, events and mutexes
    if (!BufferCaptureThreadRunning)
    {
        BufferCaptureThreadRunning = true;

        if (OS_CreateThread(&Thread, BufferCaptureThreadStub, this, &player_tasks_desc[SE_TASK_MANIF_BRCAPTURE]) != OS_NO_ERROR)
        {
            SE_ERROR("Unable to create Buffer Capture thread\n");
            BufferCaptureThreadRunning = false;
            return HavanaNoMemory;
        }
    }

    this->Manifestor    = Manifestor;
    return HavanaNoError;
}
//}}}
//{{{  DeInit
HavanaStatus_t HavanaCapture_c::DeInit(class Manifestor_Base_c           *Manifestor)
{
    SE_VERBOSE(group_havana, "this = %p Manifestor = %p\n", this, Manifestor);

    // Shut down buffer capture thread first
    if (BufferCaptureThreadRunning)
    {
        // Ask thread to terminate
        OS_ResetEvent(&BufferCaptureThreadTerminated);
        OS_Smp_Mb(); // Write memory barrier: wmb_for_HavanaCapture_Terminating coupled with: rmb_for_HavanaCapture_Terminating
        BufferCaptureThreadRunning = false;
        OS_SetEvent(&BufferReceived);

        // Wait for buffer capture thread to terminate
        OS_WaitForEventAuto(&BufferCaptureThreadTerminated, OS_INFINITE);
    }

    BufferCaptureCallback               = NULL;
    CallbackContext                     = NULL;
    this->Manifestor                    = NULL;
    return HavanaNoError;
}
//}}}
//{{{  SetLatency
HavanaStatus_t HavanaCapture_c::SetLatency(unsigned int    Latency)
{
    SE_VERBOSE(group_havana, "Latency = %d\n", Latency);
    this->Latency = Latency;
    return HavanaNoError;
}
//}}}
//{{{  QueueBuffer
HavanaStatus_t HavanaCapture_c::QueueBuffer(struct capture_buffer_s        *Buffer)
{
    SE_VERBOSE(group_havana, "Buffer = %d\n", Buffer->user_private);
    CaptureBuffers[BufferIn++]  = Buffer;

    if (BufferIn == MAX_QUEUED_BUFFERS)
    {
        BufferIn                = 0;
    }

    OS_SetEvent(&BufferReceived);
    return HavanaNoError;
}

//}}}
//{{{  RegisterBufferCaptureCallback
stream_buffer_capture_callback HavanaCapture_c::RegisterBufferCaptureCallback(stm_se_event_context_h          Context,
                                                                              stream_buffer_capture_callback  Callback)
{
    stream_buffer_capture_callback      PreviousCallback        = BufferCaptureCallback;
    SE_VERBOSE(group_havana, "Callback %p, Context %p\n", Callback, Context);
    FlushQueue();
    OS_LockMutex(&Lock);
    CallbackContext             = Context;
    BufferCaptureCallback       = Callback;

    if (BufferCaptureCallback != NULL)
    {
        Manifestor->RegisterCaptureDevice(this);
    }
    else
    {
        Manifestor->RegisterCaptureDevice(NULL);
    }

    OS_UnLockMutex(&Lock);
    return PreviousCallback;
}

//}}}
//{{{  releaseCaptureBufferCallback
static void releaseCaptureBufferCallback(stm_se_event_context_h context,
                                         unsigned int user_private)
{
    HavanaCapture_c *capture = (HavanaCapture_c *)context;
    capture->ReleaseCaptureBuffer(user_private);
}

//}}}
//{{{  ReleaseCaptureBuffer
void HavanaCapture_c::ReleaseCaptureBuffer(unsigned int user_private)
{
    Manifestor->ReleaseCaptureBuffer(user_private);
}

//}}}
//{{{  FlushQueue
HavanaStatus_t HavanaCapture_c::FlushQueue(void)
{
    SE_VERBOSE(group_havana, "\n");
    OS_LockMutex(&Lock);

    if (BufferForRelease != NULL)
    {
        SE_DEBUG(group_havana, "Flushing BufferForRelease, %d, %d\n", BufferOut, BufferForRelease->user_private);
        Manifestor->ReleaseCaptureBuffer(BufferForRelease->user_private);
        BufferForRelease        = NULL;
    }

    while (BufferOut != BufferIn)
    {
        SE_DEBUG(group_havana, "Flushing %d, %d\n", BufferOut, CaptureBuffers[BufferOut]->user_private);
        Manifestor->ReleaseCaptureBuffer(CaptureBuffers[BufferOut++]->user_private);

        if (BufferOut == MAX_QUEUED_BUFFERS)
        {
            BufferOut           = 0;
        }
    }

    OS_UnLockMutex(&Lock);
    return HavanaNoError;
}
//}}}
//{{{  TerminateQueue
HavanaStatus_t HavanaCapture_c::TerminateQueue(void)
{
    SE_VERBOSE(group_havana, "\n");
    //flush the q
    return HavanaNoError;
}
//}}}

//{{{  BufferCaptureThread
void  HavanaCapture_c::BufferCaptureThread(void)
{
    struct capture_buffer_s    *NextBuffer;
    SE_DEBUG(group_havana, "Starting\n");

    while (BufferCaptureThreadRunning)
    {
        int             SleepTime       = 0;
        OS_LockMutex(&Lock);

        while ((BufferOut != BufferIn) && BufferCaptureThreadRunning)
        {
            unsigned long long      Now;
            NextBuffer              = CaptureBuffers[BufferOut];
            Now                     = OS_GetTimeInMicroSeconds();

            /// need to be smarter here for shutdown
            if (ValidTime(NextBuffer->presentation_time))
            {
                if (NextBuffer->presentation_time > (Now + Latency))
                {
                    SleepTime       = (NextBuffer->presentation_time - (Now + Latency)) / 1000;
                }

                SE_DEBUG(group_havana, "Presentation time %llu, Now %llu, Latency %d Sleep %d\n",
                         NextBuffer->presentation_time, Now, Latency, SleepTime);
            }
            else
            {
                NextBuffer->presentation_time   = Now;
            }

            if (SleepTime > 0)
            {
                break;
            }

            if (!BufferCaptureThreadRunning)
            {
                break;
            }

            if (BufferCaptureCallback != NULL)
            {
                // Do not Release direclty buffer if not asked by user
                NextBuffer->releaseBuffer = releaseCaptureBufferCallback;
                NextBuffer->releaseBufferContext = this;

                if (BufferCaptureCallback(CallbackContext, NextBuffer) == 0)
                {
                    NextBuffer = NULL;
                }
            }

            if (BufferForRelease != NULL)
            {
                SE_DEBUG(group_havana, "Releasing buffer %d\n", BufferForRelease->user_private);
                Manifestor->ReleaseCaptureBuffer(BufferForRelease->user_private);
            }

            BufferForRelease        = NextBuffer;
            BufferOut++;

            if (BufferOut == MAX_QUEUED_BUFFERS)
            {
                BufferOut           = 0;
            }
        }

        OS_UnLockMutex(&Lock);

        if (SleepTime > 0)
        {
            OS_SleepMilliSeconds(SleepTime);
        }

        if ((BufferOut == BufferIn) && BufferCaptureThreadRunning)
        {
            // Wait for buffer to arrive
            OS_WaitForEventAuto(&BufferReceived, OS_INFINITE);
            OS_ResetEvent(&BufferReceived);
        }
    }
    OS_Smp_Mb(); // Read memory barrier: rmb_for_HavanaCapture_Terminating coupled with: wmb_for_HavanaCapture_Terminating

    // Tell capture callback we are quitting and then release all buffers.
    OS_LockMutex(&Lock);

    if (BufferCaptureCallback != NULL)
    {
        BufferCaptureCallback(CallbackContext, NULL);
    }

    OS_UnLockMutex(&Lock);

    if (BufferForRelease != NULL)
    {
        Manifestor->ReleaseCaptureBuffer(BufferForRelease->user_private);
    }

    while (BufferOut != BufferIn)
    {
        SE_DEBUG(group_havana, "Releasing buffer %d\n", CaptureBuffers[BufferOut]->user_private);
        Manifestor->ReleaseCaptureBuffer(CaptureBuffers[BufferOut++]->user_private);

        if (BufferOut == MAX_QUEUED_BUFFERS)
        {
            BufferOut           = 0;
        }
    }

    OS_SetEvent(&BufferCaptureThreadTerminated);
    SE_DEBUG(group_havana, "Terminating\n");
}
//}}}

