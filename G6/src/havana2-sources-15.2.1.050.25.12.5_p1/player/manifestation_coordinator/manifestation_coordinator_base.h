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

#ifndef H_MANIFESTATION_COORDINATOR_BASE
#define H_MANIFESTATION_COORDINATOR_BASE

#include "player.h"

#undef TRACE_TAG
#define TRACE_TAG "ManifestationCoordinator_Base_c"

typedef struct ManifestationContext_s
{
    Manifestor_t   Manifestor;
    void          *Identifier;
} ManifestationContext_t;

typedef struct ManifestationBufferState_s
{
    Buffer_t         Buffer;
    OutputTiming_t  *OutputTiming;

    unsigned int        OutstandingCount;
    unsigned long long  ReleaseTime;
} ManifestationBufferState_t;


// /////////////////////////////////////////////////////////////////////////
//
// The C task entry stubs
//

extern "C" {
    OS_TaskEntry(ManifestationCoordinator_Base_OutputProcess);
}

class ManifestationCoordinator_Base_c : public ManifestationCoordinator_c
{
public:
    explicit ManifestationCoordinator_Base_c(unsigned int NumberOfSupportedManifestors = MAXIMUM_NUMBER_OF_SUPPORTED_MANIFESTATIONS);
    ManifestationCoordinatorStatus_t FinalizeInit(void);
    ~ManifestationCoordinator_Base_c(void);

    //
    // Override for component base class halt/reset functions
    //

    PlayerStatus_t   Halt(void);
    PlayerStatus_t   Reset(void);

    PlayerStatus_t   SetModuleParameters(unsigned int         ParameterBlockSize,
                                         void             *ParameterBlock);

    PlayerStatus_t   SpecifySignalledEvents(PlayerEventMask_t     EventMask,
                                            void             *EventUserData);


    PlayerStatus_t   RegisterPlayer(Player_t             Player,
                                    PlayerPlayback_t         Playback,
                                    PlayerStream_t           Stream);

    //
    // Control functions adding and removing individual manifestors
    //

    ManifestationCoordinatorStatus_t   AddManifestation(Manifestor_t       Manifestor,
                                                        void          *Identifier);

    ManifestationCoordinatorStatus_t   RemoveManifestation(Manifestor_t    Manifestor,
                                                           void          *Identifier);

    //
    // Aggregate functions, to retrieve information from all the manifestors, and return it
    // as an array of pointers int the first case, and as one composite value in the second.
    //

    ManifestationCoordinatorStatus_t   GetSurfaceParameters(OutputSurfaceDescriptor_t   ***SurfaceParametersArray,
                                                            unsigned int              *HighestIndex);

    ManifestationCoordinatorStatus_t   GetLastQueuedManifestationTime(unsigned long long      *Time);
    ManifestationCoordinatorStatus_t   GetNextQueuedManifestationTimes(unsigned long long    **Times,
                                                                       unsigned long long   **Granularities);

    //
    // Functions to perform composite actions, resulting in calls to the
    // attached manifestors, and behaviour within the coordinator.
    //

    ManifestationCoordinatorStatus_t   Connect(Port_c *Port);
    ManifestationCoordinatorStatus_t   ReleaseQueuedDecodeBuffers(void);
    ManifestationCoordinatorStatus_t   QueueDecodeBuffer(Buffer_t        Buffer);
    ManifestationCoordinatorStatus_t   QueueNullManifestation(void);
    ManifestationCoordinatorStatus_t   QueueEventSignal(PlayerEventRecord_t *Event);
    ManifestationCoordinatorStatus_t   SynchronizeOutput(void);

    void   ResetOnStreamSwitch(void);

    void OutputProcess(void);

protected:
    OS_Mutex_t                  ManifestationCoordinatorLock;

    unsigned int                NumberOfSupportedManifestations;
    unsigned int                HighestSurfaceIndex;
    OutputSurfaceDescriptor_t  *SurfaceParameterPointers[MAXIMUM_MANIFESTATION_TIMING_COUNT];
    unsigned long long          NextQueuedManifestationTimes[MAXIMUM_MANIFESTATION_TIMING_COUNT];
    unsigned long long          ManifestationGranularities[MAXIMUM_MANIFESTATION_TIMING_COUNT];
    ManifestationContext_t      ManifestationContext[MAXIMUM_NUMBER_OF_SUPPORTED_MANIFESTATIONS];
    ManifestationBufferState_t  BufferState[MAX_DECODE_BUFFERS];

    Ring_t                      InternalOutputRing;
    Port_c                     *mOutputPort;

    unsigned int                HighestDecodeBufferIndex;
    unsigned int                BuffersAwaitingTimedRelease;
    unsigned int                BuffersAwaitingRelease;

    bool                        Terminating;
    OS_Event_t                  StartStopEvent;

    void   ScanForTimeDelayedBuffers(unsigned long long ReleaseUpTo = INVALID_TIME);

private:
    DISALLOW_COPY_AND_ASSIGN(ManifestationCoordinator_Base_c);
};

#endif
