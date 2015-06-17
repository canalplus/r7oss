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

#ifndef H_MANIFESTATION_COORDINATOR
#define H_MANIFESTATION_COORDINATOR

#include "player.h"

enum
{
    ManifestationCoordinatorNoError         = PlayerNoError,
    ManifestationCoordinatorError           = PlayerError,

    ManifestationCoordinatorTooManyManifestations   = BASE_MANIFESTATION_COORDINATOR,
    ManifestationCoordinatorManifestationNotFound
};

typedef PlayerStatus_t  ManifestationCoordinatorStatus_t;

enum
{
    ManifestationCoordinatorFnQueueEventSignal = BASE_MANIFESTATION_COORDINATOR,
    ManifestationCoordinatorFnEventMngrSignal,
};

class ManifestationCoordinator_c : public BaseComponentClass_c
{
public:
    //
    // Control functions adding and removing individual manifestors
    //

    virtual ManifestationCoordinatorStatus_t   AddManifestation(Manifestor_t       Manifestor,
                                                                void          *Identifier) = 0;

    virtual ManifestationCoordinatorStatus_t   RemoveManifestation(Manifestor_t    Manifestor,
                                                                   void          *Identifier) = 0;

    //
    // Aggregate functions, to retrieve information from all the manifestors, and return it
    // as an array of pointers int the first case, and as one composite value in the second.
    //

    virtual ManifestationCoordinatorStatus_t   GetSurfaceParameters(OutputSurfaceDescriptor_t   ***SurfaceParametersArray,
                                                                    unsigned int              *HighestIndex) = 0;

    virtual ManifestationCoordinatorStatus_t   GetLastQueuedManifestationTime(unsigned long long      *Time) = 0;
    virtual ManifestationCoordinatorStatus_t   GetNextQueuedManifestationTimes(unsigned long long    **Times,
                                                                               unsigned long long   **Granularities) = 0;

    //
    // Functions to perform composite actions, resulting in calls to the
    // attached manifestors, and behaviour within the coordinator.
    //

    virtual ManifestationCoordinatorStatus_t   Connect(Port_c *Port) = 0;
    virtual ManifestationCoordinatorStatus_t   ReleaseQueuedDecodeBuffers(void) = 0;
    virtual ManifestationCoordinatorStatus_t   QueueDecodeBuffer(Buffer_t        Buffer) = 0;
    virtual ManifestationCoordinatorStatus_t   QueueNullManifestation(void) = 0;
    virtual ManifestationCoordinatorStatus_t   QueueEventSignal(PlayerEventRecord_t *Event) = 0;
    virtual ManifestationCoordinatorStatus_t   SynchronizeOutput(void) = 0;

    virtual void ResetOnStreamSwitch(void) = 0;
};
#endif
