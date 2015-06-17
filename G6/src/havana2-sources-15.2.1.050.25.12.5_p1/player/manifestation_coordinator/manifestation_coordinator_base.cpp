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

#include "ring_generic.h"
#include "player_generic.h"
#include "player_playback.h"
#include "player_stream.h"
#include "manifestation_coordinator_base.h"

#undef TRACE_TAG
#define TRACE_TAG "ManifestationCoordinator_Base_c"

#define MAX_EVENT_WAIT      20      // Ms - this is the time between checks for termination, and for timed buffer release

//
ManifestationCoordinator_Base_c::ManifestationCoordinator_Base_c(unsigned int NumberOfSupportedManifestors)
    : ManifestationCoordinatorLock()
    , NumberOfSupportedManifestations(NumberOfSupportedManifestors)
    , HighestSurfaceIndex(0)
    , SurfaceParameterPointers()
    , NextQueuedManifestationTimes()
    , ManifestationGranularities()
    , ManifestationContext()
    , BufferState()
    , InternalOutputRing(NULL)
    , mOutputPort(NULL)
    , HighestDecodeBufferIndex(0)
    , BuffersAwaitingTimedRelease(0)
    , BuffersAwaitingRelease(0)
    , Terminating(false)
    , StartStopEvent()
{
    OS_InitializeMutex(&ManifestationCoordinatorLock);
    OS_InitializeEvent(&StartStopEvent);

    // TODO(pht) move FinalizeInit to a factory method
    InitializationStatus = FinalizeInit();
}

//

ManifestationCoordinatorStatus_t ManifestationCoordinator_Base_c::FinalizeInit(void)
{
    // Create the ring used to inform the output process of a buffer
    InternalOutputRing = new class RingGeneric_c((MAXIMUM_NUMBER_OF_SUPPORTED_MANIFESTATIONS + 1) * MAX_DECODE_BUFFERS);
    if ((InternalOutputRing == NULL) ||
        (InternalOutputRing->InitializationStatus != RingNoError))
    {
        SE_ERROR("Failed to create internal output ring\n");
        if (InternalOutputRing != NULL)
        {
            delete InternalOutputRing;
            InternalOutputRing = NULL;
        }
        return ManifestationCoordinatorError;
    }

    // Create the buffer output process
    OS_ResetEvent(&StartStopEvent);

    OS_Thread_t Thread;
    if (OS_CreateThread(&Thread, ManifestationCoordinator_Base_OutputProcess, this, &player_tasks_desc[SE_TASK_MANIF_COORD]) != OS_NO_ERROR)
    {
        SE_ERROR("Failed to create output process\n");
        delete InternalOutputRing;
        InternalOutputRing = NULL;
        return ManifestationCoordinatorError;
    }

    // Wait for output process to run
    OS_Status_t WaitStatus;
    do
    {
        WaitStatus = OS_WaitForEventAuto(&StartStopEvent, MAX_EVENT_WAIT);
        if (WaitStatus == OS_TIMED_OUT)
        {
            SE_WARNING("Still waiting for output process to run\n");
        }
    }
    while (WaitStatus == OS_TIMED_OUT);

    OS_ResetEvent(&StartStopEvent);

    return ManifestationCoordinatorNoError;
}

//
ManifestationCoordinator_Base_c::~ManifestationCoordinator_Base_c(void)
{
    // Remove any on-going manifestations
    for (unsigned int i = 0; i < NumberOfSupportedManifestations; i++)
    {
        if (ManifestationContext[i].Manifestor != NULL)
        {
            RemoveManifestation(ManifestationContext[i].Manifestor, ManifestationContext[i].Identifier);
        }
    }

    // Shut down the buffer release process
    ScanForTimeDelayedBuffers();

    // Ask thread to terminate
    OS_ResetEvent(&StartStopEvent);
    OS_Smp_Mb(); // Write memory barrier: wmb_for_ManifestationCoordinator_Terminating coupled with: rmb_for_ManifestationCoordinator_Terminating
    Terminating = true;

    if (InternalOutputRing != NULL)
    {
        InternalOutputRing->Insert(0);
    }

    // Wait for output process to terminate
    OS_Status_t WaitStatus;
    do
    {
        WaitStatus = OS_WaitForEventAuto(&StartStopEvent, 2 * MAX_EVENT_WAIT);
        if (WaitStatus == OS_TIMED_OUT)
        {
            SE_WARNING("Still waiting for output process to stop\n");
        }
    }
    while (WaitStatus == OS_TIMED_OUT);

    // Kill the ring
    delete InternalOutputRing;

    OS_TerminateEvent(&StartStopEvent);
    OS_TerminateMutex(&ManifestationCoordinatorLock);
}


// /////////////////////////////////////////////////////////////////////////
//
//      Function to add a manifestation
//

ManifestationCoordinatorStatus_t   ManifestationCoordinator_Base_c::AddManifestation(
    Manifestor_t       Manifestor,
    void          *Identifier)
{
    unsigned int i;
    ManifestationCoordinatorStatus_t Status = ManifestationCoordinatorNoError;

    Stream->EnterManifestorExclusiveSection();
    OS_LockMutex(&ManifestationCoordinatorLock);

    for (i = 0; i < NumberOfSupportedManifestations; i++)
    {
        if (ManifestationContext[i].Manifestor == NULL)
        {
            ManifestationContext[i].Manifestor  = Manifestor;
            ManifestationContext[i].Identifier  = Identifier;
            Manifestor->RegisterPlayer(Player, Playback, Stream);

            if (mOutputPort != NULL)
            {
                if (ManifestorNoError != Manifestor->Connect(InternalOutputRing))
                {
                    SE_ERROR("0x%p Not able to connect to manifestor 0x%p\n", this, Manifestor);
                    Status = ManifestationCoordinatorError;    // keep going
                }
            }

            break;
        }
    }

    OS_UnLockMutex(&ManifestationCoordinatorLock);
    Stream->ExitManifestorExclusiveSection();

    if (i == NumberOfSupportedManifestations)
    {
        SE_ERROR("Too many manifestations\n");
        return ManifestationCoordinatorTooManyManifestations;
    }

    return Status;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Function to remove a manifestation
//

ManifestationCoordinatorStatus_t   ManifestationCoordinator_Base_c::RemoveManifestation(
    Manifestor_t   Manifestor,
    void          *Identifier)
{
    unsigned int ManifestorIndex, TimingIndex = 0, SurfaceIndex;

    Stream->EnterManifestorExclusiveSection();
    OS_LockMutex(&ManifestationCoordinatorLock);

    for (ManifestorIndex = 0; ManifestorIndex < NumberOfSupportedManifestations;  ManifestorIndex++)
    {
        if ((ManifestationContext[ManifestorIndex].Manifestor == Manifestor) &&
            (ManifestationContext[ManifestorIndex].Identifier == Identifier))
        {
            //
            // We depend on the caller to cleanup this manifestor, and to release any buffers it may be holding
            //
            memset(&ManifestationContext[ManifestorIndex], 0, sizeof(ManifestationContext_t));
            TimingIndex = ManifestorIndex * MAXIMUM_NUMBER_OF_TIMINGS_PER_MANIFESTATION;

            for (SurfaceIndex = TimingIndex; SurfaceIndex < TimingIndex + MAXIMUM_NUMBER_OF_TIMINGS_PER_MANIFESTATION; SurfaceIndex++)
            {
                SurfaceParameterPointers[SurfaceIndex]      = NULL;
            }

            break;
        }
    }

    OS_UnLockMutex(&ManifestationCoordinatorLock);
    Stream->ExitManifestorExclusiveSection();

    if (ManifestorIndex == NumberOfSupportedManifestations)
    {
        SE_ERROR("Manifestation not found\n");
        return ManifestationCoordinatorManifestationNotFound;
    }

    return ManifestationCoordinatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Component base class clone of Halt()
//

PlayerStatus_t  ManifestationCoordinator_Base_c::Halt(void)
{
    unsigned int i;
    //
    // First call halt on the children
    //
    OS_LockMutex(&ManifestationCoordinatorLock);

    for (i = 0; i < NumberOfSupportedManifestations; i++)
        if (ManifestationContext[i].Manifestor != NULL)
        {
            ManifestationContext[i].Manifestor->Halt();
        }

    //
    // Scan for any buffers just waiting on time and release them
    //
    ScanForTimeDelayedBuffers();
    OS_UnLockMutex(&ManifestationCoordinatorLock);

    unsigned long long TimeBeforeWait = OS_GetTimeInMicroSeconds();
    while (BuffersAwaitingRelease != 0)      // Let the release process work first
    {
        OS_SleepMilliSeconds(5);
        if (OS_GetTimeInMicroSeconds() > (TimeBeforeWait + 1000000))
        {
            SE_WARNING("Stream 0x%p this 0x%p: Still waiting for %d buffers to be released\n", Stream, this, BuffersAwaitingRelease);
            TimeBeforeWait = OS_GetTimeInMicroSeconds();
        }
    }

    OS_LockMutex(&ManifestationCoordinatorLock);
    //
    // Do our own halt activity
    //
    mOutputPort = NULL;
    OS_UnLockMutex(&ManifestationCoordinatorLock);

    return ManifestationCoordinatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Component base class clone of Reset()
//

PlayerStatus_t  ManifestationCoordinator_Base_c::Reset(void)
{
    unsigned int i;
    //
    // Call reset on the children
    //
    OS_LockMutex(&ManifestationCoordinatorLock);

    for (i = 0; i < NumberOfSupportedManifestations; i++)
        if (ManifestationContext[i].Manifestor != NULL)
        {
            ManifestationContext[i].Manifestor->Reset();
        }

    OS_UnLockMutex(&ManifestationCoordinatorLock);

    return ManifestationCoordinatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Component base class clone of SetModuleParameters()
//

PlayerStatus_t  ManifestationCoordinator_Base_c::SetModuleParameters(unsigned int        ParameterBlockSize,
                                                                     void            *ParameterBlock)
{
    unsigned int i;
    //
    // We have no parameters so just pass on to children
    //
    OS_LockMutex(&ManifestationCoordinatorLock);

    for (i = 0; i < NumberOfSupportedManifestations; i++)
        if (ManifestationContext[i].Manifestor != NULL)
        {
            ManifestationContext[i].Manifestor->SetModuleParameters(ParameterBlockSize, ParameterBlock);
        }

    OS_UnLockMutex(&ManifestationCoordinatorLock);

    return ManifestationCoordinatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Component base class clone of SpecifySignalledEvents()
//

PlayerStatus_t  ManifestationCoordinator_Base_c::SpecifySignalledEvents(PlayerEventMask_t     EventMask,
                                                                        void            *EventUserData)
{
    unsigned int i;
    //
    // We have no parameters so just pass on to children
    //
    OS_LockMutex(&ManifestationCoordinatorLock);

    for (i = 0; i < NumberOfSupportedManifestations; i++)
        if (ManifestationContext[i].Manifestor != NULL)
        {
            ManifestationContext[i].Manifestor->SpecifySignalledEvents(EventMask, EventUserData);
        }

    OS_UnLockMutex(&ManifestationCoordinatorLock);

    return ManifestationCoordinatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Component base class clone of RegisterPlayer()
//

PlayerStatus_t  ManifestationCoordinator_Base_c::RegisterPlayer(Player_t            Player,
                                                                PlayerPlayback_t        Playback,
                                                                PlayerStream_t          Stream)
{
    unsigned int i;
    //
    // Record for ourselves, and pass on to children
    //
    OS_LockMutex(&ManifestationCoordinatorLock);
    BaseComponentClass_c::RegisterPlayer(Player, Playback, Stream);

    for (i = 0; i < NumberOfSupportedManifestations; i++)
        if (ManifestationContext[i].Manifestor != NULL)
        {
            ManifestationContext[i].Manifestor->RegisterPlayer(Player, Playback, Stream);
        }

    OS_UnLockMutex(&ManifestationCoordinatorLock);

    return ManifestationCoordinatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Run through and obtain the surface parameters
//

ManifestationCoordinatorStatus_t   ManifestationCoordinator_Base_c::GetSurfaceParameters(
    OutputSurfaceDescriptor_t   ***SurfaceParametersArray,
    unsigned int              *HighestIndex)
{
    unsigned int  ManifestorIndex, TimingIndex = 0, NumSurfaces, SurfaceIndex;
//
    OS_LockMutex(&ManifestationCoordinatorLock);

    for (ManifestorIndex = 0; ManifestorIndex < NumberOfSupportedManifestations; ManifestorIndex++)
    {
        if (ManifestationContext[ManifestorIndex].Manifestor != NULL)
        {
            ManifestationContext[ManifestorIndex].Manifestor->GetSurfaceParameters(&SurfaceParameterPointers[TimingIndex], &NumSurfaces);

            //iterate round each manifestor and manifestor surface
            for (SurfaceIndex = TimingIndex; SurfaceIndex < TimingIndex + MAXIMUM_NUMBER_OF_TIMINGS_PER_MANIFESTATION; SurfaceIndex++)
            {
                if (SurfaceIndex >= TimingIndex + NumSurfaces)
                {
                    SurfaceParameterPointers[SurfaceIndex] = NULL;    //not in use
                }
                else if (SurfaceParameterPointers[SurfaceIndex] != NULL)
                {
                    HighestSurfaceIndex   = SurfaceIndex;
                }
            }
        }
        else
        {
            //manifestor not used, so mark all surfaces as NULL
            for (SurfaceIndex = TimingIndex; SurfaceIndex < TimingIndex + MAXIMUM_NUMBER_OF_TIMINGS_PER_MANIFESTATION; SurfaceIndex++)
            {
                SurfaceParameterPointers[SurfaceIndex] = NULL;
            }
        }

        TimingIndex += MAXIMUM_NUMBER_OF_TIMINGS_PER_MANIFESTATION;
    }

    *SurfaceParametersArray = SurfaceParameterPointers;
    *HighestIndex       = HighestSurfaceIndex;
    OS_UnLockMutex(&ManifestationCoordinatorLock);
//
    return ManifestationCoordinatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Run through and obtain the latest manifestation time for a next frame (used in playout)
//

ManifestationCoordinatorStatus_t   ManifestationCoordinator_Base_c::GetLastQueuedManifestationTime(unsigned long long     *Time)
{
    unsigned int      ManifestorIndex, NumTimes, TimingIndex;
    unsigned long long    OneTime[MAXIMUM_NUMBER_OF_TIMINGS_PER_MANIFESTATION];
    unsigned long long    LatestTime;

    OS_LockMutex(&ManifestationCoordinatorLock);
    LatestTime          = INVALID_TIME;

    for (TimingIndex = 0; TimingIndex < MAXIMUM_NUMBER_OF_TIMINGS_PER_MANIFESTATION; TimingIndex++)
    {
        OneTime[TimingIndex]  = INVALID_TIME;
    }

    for (ManifestorIndex = 0; ManifestorIndex < NumberOfSupportedManifestations; ManifestorIndex++)
        if (ManifestationContext[ManifestorIndex].Manifestor != NULL)
        {
            ManifestationContext[ManifestorIndex].Manifestor->GetNextQueuedManifestationTime(&OneTime[0], &NumTimes);

            for (TimingIndex = 0; TimingIndex < MAXIMUM_NUMBER_OF_TIMINGS_PER_MANIFESTATION; TimingIndex++)
                if (NotValidTime(LatestTime) || (ValidTime(OneTime[TimingIndex]) && (OneTime[TimingIndex] > LatestTime)))
                {
                    LatestTime    = OneTime[TimingIndex];
                }
        }

    *Time = ValidTime(LatestTime) ? LatestTime : UNSPECIFIED_TIME;
    OS_UnLockMutex(&ManifestationCoordinatorLock);

    return ManifestationCoordinatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Run through and obtain the list of next available manifestation times (used in startup sync)
//  distinguishing between returned invalid times, and manifestors not present,
//  by marking one as unspecified and the other as invalid.
//
//      Get a list of times from a manifestor

ManifestationCoordinatorStatus_t   ManifestationCoordinator_Base_c::GetNextQueuedManifestationTimes(unsigned long long   **Times,
                                                                                                    unsigned long long  **Granularities)
{
    unsigned int            ManifestorIndex, TimingIndex, i;
    OutputSurfaceDescriptor_t   **DummyPtr;
    unsigned int             DummyIndex, NumTimes;
//
    GetSurfaceParameters(&DummyPtr, &DummyIndex);        // Force load
    OS_LockMutex(&ManifestationCoordinatorLock);

    for (ManifestorIndex = 0; ManifestorIndex < NumberOfSupportedManifestations; ManifestorIndex++)
    {
        TimingIndex = ManifestorIndex * MAXIMUM_NUMBER_OF_TIMINGS_PER_MANIFESTATION;

        if (ManifestationContext[ManifestorIndex].Manifestor != NULL)
        {
            ManifestationContext[ManifestorIndex].Manifestor->GetNextQueuedManifestationTime(&NextQueuedManifestationTimes[TimingIndex], &NumTimes);

            //iterate round each manifestor and manifestor timing
            for (i = TimingIndex; i < TimingIndex + MAXIMUM_NUMBER_OF_TIMINGS_PER_MANIFESTATION; i++)
            {
                if (i >= TimingIndex + NumTimes)
                {
                    //timing not in use by manifestor mark invalid
                    NextQueuedManifestationTimes[i] = INVALID_TIME;
                    ManifestationGranularities[i]   = 0;
                }
                else
                {
                    if (NotValidTime(NextQueuedManifestationTimes[i]))
                    {
                        NextQueuedManifestationTimes[i]   = UNSPECIFIED_TIME;
                    }

                    if ((SurfaceParameterPointers[i] != NULL) &&
                        (SurfaceParameterPointers[i]->StreamType == StreamTypeVideo))
                    {
                        ManifestationGranularities[i]   = RoundedLongLongIntegerPart(1000000 / SurfaceParameterPointers[i]->FrameRate);

                        if (!SurfaceParameterPointers[i]->Progressive)
                        {
                            ManifestationGranularities[i] *= 2;
                        }
                    }
                    else
                    {
                        ManifestationGranularities[i] = 1;
                    }
                }
            }
        }
        else
        {
            //manifestor not used, so mark all timings as Invalid
            for (i = TimingIndex; i < TimingIndex + MAXIMUM_NUMBER_OF_TIMINGS_PER_MANIFESTATION; i++)
            {
                NextQueuedManifestationTimes[i] = INVALID_TIME;
                ManifestationGranularities[i]           = 0;
            }
        }
    }

    for (i = 0; i <  MAXIMUM_MANIFESTATION_TIMING_COUNT; i++)
    {
        if (ValidTime(NextQueuedManifestationTimes[i]))
            SE_DEBUG(group_avsync, "[%d] NextQueuedManifestationTimes = %llu  ManifestationGranularities = %llu\n",
                     i, NextQueuedManifestationTimes[i], ManifestationGranularities[i]);
    }

    *Times      = NextQueuedManifestationTimes;
    *Granularities  = ManifestationGranularities;
    OS_UnLockMutex(&ManifestationCoordinatorLock);
//
    return ManifestationCoordinatorNoError;
}

void  ManifestationCoordinator_Base_c::ResetOnStreamSwitch()
{
    OS_LockMutex(&ManifestationCoordinatorLock);

    for (int i = 0; i < NumberOfSupportedManifestations; i++)
    {
        if (ManifestationContext[i].Manifestor != NULL)
        {
            ManifestationContext[i].Manifestor->ResetOnStreamSwitch();
        }
    }

    OS_UnLockMutex(&ManifestationCoordinatorLock);
}


// /////////////////////////////////////////////////////////////////////////
//
//  Connect the output port, allow us to register the internal
//  ring to child manifestations.
//

ManifestationCoordinatorStatus_t   ManifestationCoordinator_Base_c::Connect(Port_c *Port)
{
    unsigned int        i;
    ManifestationCoordinatorStatus_t Status = ManifestationCoordinatorNoError;

    if (Port == NULL)
    {
        SE_ERROR("Incorrect parameter\n");
        return ManifestationCoordinatorError;
    }
    if (mOutputPort != NULL)
    {
        SE_WARNING("Port already connected\n");
    }

    OS_LockMutex(&ManifestationCoordinatorLock);
    mOutputPort = Port;

    for (i = 0; i < NumberOfSupportedManifestations; i++)
        if (ManifestationContext[i].Manifestor != NULL)
        {
            if (ManifestorNoError != ManifestationContext[i].Manifestor->Connect(InternalOutputRing))
            {
                SE_ERROR("0x%p Not able to connect to manifestor 0x%p\n", this, ManifestationContext[i].Manifestor);
                Status = ManifestationCoordinatorError;    // keep going
            }
        }

    OS_UnLockMutex(&ManifestationCoordinatorLock);
//
    return Status;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Release any queued decode buffers
//

ManifestationCoordinatorStatus_t   ManifestationCoordinator_Base_c::ReleaseQueuedDecodeBuffers(void)
{
    unsigned int        i;

    SE_INFO(group_avsync, "Stream 0x%p this 0x%p", Stream, (void *)this);

    OS_LockMutex(&ManifestationCoordinatorLock);

    for (i = 0; i < NumberOfSupportedManifestations; i++)
        if (ManifestationContext[i].Manifestor != NULL)
        {
            ManifestationContext[i].Manifestor->ReleaseQueuedDecodeBuffers();
        }

    ScanForTimeDelayedBuffers();
    OS_UnLockMutex(&ManifestationCoordinatorLock);
//
    return ManifestationCoordinatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Queue a buffer for display
//

ManifestationCoordinatorStatus_t   ManifestationCoordinator_Base_c::QueueDecodeBuffer(Buffer_t   Buffer)
{
    unsigned int             i, NumTimings, TimingIndex, ManifestorIndex, k = 0;
    unsigned int             Index;
    ManifestorStatus_t       Status;
    ManifestationBufferState_t  *State;
    bool                 AVDSyncOff;
    bool                 UseUnspecifiedTime;
    long long            ManifestationDelayInMicroSecond, DelayLimitInMicroSecond;
    Rational_t           Speed;
    PlayDirection_t          Direction;
    bool                             QueueBuffer = false;
    ManifestationOutputTiming_t *ManifestationTimingPointers[MAXIMUM_NUMBER_OF_TIMINGS_PER_MANIFESTATION];
//
    OS_LockMutex(&ManifestationCoordinatorLock);

    //
    // Get the index, and initialise our entry
    //

    for (TimingIndex = 0; TimingIndex < MAXIMUM_NUMBER_OF_TIMINGS_PER_MANIFESTATION; TimingIndex++)
    {
        ManifestationTimingPointers[TimingIndex] = (ManifestationOutputTiming_t *) 0xdeadbeef;
    }

    Buffer->GetIndex(&Index);

    if (Index > HighestDecodeBufferIndex)
    {
        HighestDecodeBufferIndex  = Index;
    }

    Player->GetPlaybackSpeed(Playback, &Speed, &Direction);
    /* 1 is nominal speed value */

    /* This delay is useful for robustness: it's used to detect weird PTS and avoid facing a deadlock in this case */
    DelayLimitInMicroSecond = PLAYER_DELAY_LIMIT_IN_US;
    if (Speed > 0 && Speed != 1)
    {
        DelayLimitInMicroSecond = (DelayLimitInMicroSecond / Speed).LongLongIntegerPart();
    }


    State   = &BufferState[Index];

    if (State->OutstandingCount != 0)
    {
        SE_ERROR("Queuing a buffer we already have references to\n");
        OS_UnLockMutex(&ManifestationCoordinatorLock);
        return ManifestationCoordinatorError;
    }

    State->Buffer = Buffer;
    Buffer->ObtainMetaDataReference(Player->MetaDataOutputTimingType, (void **)(&State->OutputTiming));
    SE_ASSERT(State->OutputTiming != NULL);

    //
    // Check manifestation time
    //
    BuffersAwaitingRelease++;
    ManifestationDelayInMicroSecond = State->OutputTiming->BaseSystemPlaybackTime - OS_GetTimeInMicroSeconds();
    UseUnspecifiedTime          = ValidTime(State->OutputTiming->BaseSystemPlaybackTime) &&
                                  (ManifestationDelayInMicroSecond > DelayLimitInMicroSecond);

    if (UseUnspecifiedTime)
    {
        /* Picture will be sent to display anyway but with presentationTime set to 0 */
        SE_WARNING("Stream 0x%p this=%p Manifestation time discarded : delay_us=%lld limit=%lld BaseSystemPlaybackTime=%lld valid=%d\n"
                   , Stream
                   , (void *)this
                   , ManifestationDelayInMicroSecond
                   , DelayLimitInMicroSecond
                   , State->OutputTiming->BaseSystemPlaybackTime
                   , ValidTime(State->OutputTiming->BaseSystemPlaybackTime));
    }

    State->ReleaseTime          = UseUnspecifiedTime ? UNSPECIFIED_TIME : State->OutputTiming->BaseSystemPlaybackTime;

    if (ValidTime(State->ReleaseTime))
    {
        BuffersAwaitingTimedRelease++;
        State->OutstandingCount++;
    }

    //
    // Now queue onto the individual manifestors
    //
    // NOTE we perform the check for zero duration here, as we can no
    //      longer perform it for all manifestations in the output timer,
    //      due to it being possible that some manifestations have zero
    //      duration while others have non-zero.
    //
    AVDSyncOff  = (Player->PolicyValue(Playback, Stream, PolicyAVDSynchronization) == PolicyValueDisapply);
    Status  = ManifestorNoError;

    for (ManifestorIndex = 0; ((ManifestorIndex < NumberOfSupportedManifestations) && (Status != ManifestorUnplayable)); ManifestorIndex++)
    {
        if (ManifestationContext[ManifestorIndex].Manifestor != NULL)
        {
            ManifestationContext[ManifestorIndex].Manifestor->GetNumberOfTimings(&NumTimings);
            k = 0;
            TimingIndex = ManifestorIndex * MAXIMUM_NUMBER_OF_TIMINGS_PER_MANIFESTATION;

            for (i = TimingIndex; i < TimingIndex + NumTimings; i++)
            {
                if (AVDSyncOff || (State->OutputTiming->ManifestationTimings[i].TimingValid
                                   && (State->OutputTiming->ManifestationTimings[i].ExpectedDurationTime != 0)))
                {
                    if (UseUnspecifiedTime)
                    {
                        State->OutputTiming->ManifestationTimings[i].SystemPlaybackTime   = UNSPECIFIED_TIME;
                    }

                    QueueBuffer = true;
                }

                ManifestationTimingPointers[k++] = &State->OutputTiming->ManifestationTimings[i];
            }

            //queue the buffer if there is at least one valid timing or avsync turned off
            if (AVDSyncOff || QueueBuffer)
            {
                Status  = ManifestationContext[ManifestorIndex].Manifestor->QueueDecodeBuffer(Buffer, &ManifestationTimingPointers[0], &NumTimings);

                if (Status == ManifestorNoError)
                {
                    State->OutstandingCount++;
                }
            }
        }
    }

    //
    // Finally if no-one was interested just pass the buffer onto the output ring
    //

    if (State->OutstandingCount == 0 && mOutputPort != NULL)
    {
        mOutputPort->Insert((uintptr_t)Buffer);
        BuffersAwaitingRelease--;
    }

    OS_UnLockMutex(&ManifestationCoordinatorLock);

//

    // We can't return an error here or the layer above will destroy the buffer
    // We have already passed the buffer on - so it will be destroyed after post-manifest.
    if (Status == ManifestorUnplayable)
    {
        Stream->MarkUnPlayable();
    }

//
    return ManifestationCoordinatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Inform manifestors of a null manifestation
//

ManifestationCoordinatorStatus_t   ManifestationCoordinator_Base_c::QueueNullManifestation(void)
{
    unsigned int        i;
//
    OS_LockMutex(&ManifestationCoordinatorLock);

    for (i = 0; i < NumberOfSupportedManifestations; i++)
        if (ManifestationContext[i].Manifestor != NULL)
        {
            ManifestationContext[i].Manifestor->QueueNullManifestation();
        }

    OS_UnLockMutex(&ManifestationCoordinatorLock);
//
    return ManifestationCoordinatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Inform manifestors of an event signal - I am passing this on,
//  it may be that we want to handle this in the release process.
//

ManifestationCoordinatorStatus_t   ManifestationCoordinator_Base_c::QueueEventSignal(PlayerEventRecord_t    *Event)
{
    unsigned int        i;
//
    OS_LockMutex(&ManifestationCoordinatorLock);

    for (i = 0; i < NumberOfSupportedManifestations; i++)
        if (ManifestationContext[i].Manifestor != NULL)
        {
            ManifestationContext[i].Manifestor->QueueEventSignal(Event);
        }

    OS_UnLockMutex(&ManifestationCoordinatorLock);
//
    return ManifestationCoordinatorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      Perform a synchronise output call
//

ManifestationCoordinatorStatus_t   ManifestationCoordinator_Base_c::SynchronizeOutput(void)
{
    unsigned int        i;
//
    OS_LockMutex(&ManifestationCoordinatorLock);

    for (i = 0; i < NumberOfSupportedManifestations; i++)
        if (ManifestationContext[i].Manifestor != NULL)
        {
            ManifestationContext[i].Manifestor->SynchronizeOutput();
        }

    OS_UnLockMutex(&ManifestationCoordinatorLock);
//
    return ManifestationCoordinatorNoError;
}

// /////////////////////////////////////////////////////////////////////////
//
//      Private - helper function to scan outstanding buffers and
//  see if any of them is waiting on an elapsed time.
//

void   ManifestationCoordinator_Base_c::ScanForTimeDelayedBuffers(unsigned long long    ReleaseUpTo)
{
    unsigned int    i;

//

    for (i = 0; (BuffersAwaitingTimedRelease != 0) && (i <= HighestDecodeBufferIndex); i++)
        if ((BufferState[i].OutstandingCount != 0) &&
            ValidTime(BufferState[i].ReleaseTime) &&
            (NotValidTime(ReleaseUpTo) || (ReleaseUpTo >= BufferState[i].ReleaseTime)))
        {
            BuffersAwaitingTimedRelease--;
            BufferState[i].OutstandingCount--;
            BufferState[i].ReleaseTime  = INVALID_TIME;

            if (BufferState[i].OutstandingCount == 0 && mOutputPort != NULL)
            {
                mOutputPort->Insert((uintptr_t) BufferState[i].Buffer);
                BuffersAwaitingRelease--;
            }
        }
}


// /////////////////////////////////////////////////////////////////////////
//
//      The intermediate process, taking output from the pre-processor
//      and feeding it to the lower level of the codec.
//

OS_TaskEntry(ManifestationCoordinator_Base_OutputProcess)
{
    ManifestationCoordinator_Base_c    *ManifestationCoordinator = (ManifestationCoordinator_Base_c *)Parameter;
    ManifestationCoordinator->OutputProcess();
    OS_TerminateThread();
    return NULL;
}

// ----------------------

void ManifestationCoordinator_Base_c::OutputProcess(void)
{
    Buffer_t             Buffer;
    unsigned int         BufferIndex;
    RingStatus_t         RingStatus;

    //
    // Signal we have started
    //
    OS_SetEvent(&StartStopEvent);

    //
    // Main loop
    //
    RingStatus  = RingNothingToGet;
    OS_Smp_Mb();

    while (!Terminating)
    {
        //
        // Anything coming through the ring
        //
        RingStatus      = InternalOutputRing->Extract((uintptr_t *)(&Buffer), MAX_EVENT_WAIT);
        OS_Smp_Mb();

        if (Terminating)
        {
            continue;
        }

        OS_LockMutex(&ManifestationCoordinatorLock);

        //
        // if we had something, then process it
        //

        if (RingStatus == RingNoError)
        {
            Buffer->GetIndex(&BufferIndex);

            if (BufferIndex >= MAX_DECODE_BUFFERS)
            {
                SE_ERROR("BufferIndex = %d is > MAX_DECODE_BUFFERS = %d", BufferIndex, MAX_DECODE_BUFFERS);
                OS_UnLockMutex(&ManifestationCoordinatorLock);
                continue;
            }

            BufferState[BufferIndex].OutstandingCount--;

            if (BufferState[BufferIndex].OutstandingCount == 0 && mOutputPort != NULL)
            {
                mOutputPort->Insert((uintptr_t)Buffer);
                BuffersAwaitingRelease--;
            }
        }

        //
        // Check for any timed releases
        //
        ScanForTimeDelayedBuffers(OS_GetTimeInMicroSeconds());
        OS_UnLockMutex(&ManifestationCoordinatorLock);
        OS_Smp_Mb();
    }

    //
    // Signal we have terminated
    //
    OS_Smp_Mb(); // Read memory barrier: rmb_for_ManifestationCoordinator_Terminating coupled with: wmb_for_ManifestationCoordinator_Terminating
    OS_SetEvent(&StartStopEvent);
}
