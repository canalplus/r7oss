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
#include "player_stream.h"
#include "manifestor_base.h"

#undef TRACE_TAG
#define TRACE_TAG   "Manifestor_Base_c"

#define BUFFER_OVERLAP_MAX_TRIES        16

// //////////////////////////////////////////////////////////////////////////
// Debugging support
// Action : register tuneable debug controls
// Input  : /debug/havana/audio_manifestor_enable_crc
// Input  : /debug/havana/audio_manifestor_enable_spdif_sw_formating
// Input  : /debug/havana/audio_manifestor_enable_hdmi_sw_formating
// ////////////////////////////////////////////////////////////////////////
unsigned int volatile Manifestor_c::EnableAudioCRC = 0;

//
Manifestor_Base_c::Manifestor_Base_c(void)
    : Configuration()
    , mOutputPort(NULL)
    , OutputRateSmoothing()
    , EventPending(false)
    , EventList()
    , EventLock()
    , NextEvent(0)
    , LastEvent(0)
    , FrameCountManifested(0)
{
    int i;

    Configuration.ManifestorName                = "Noname";
    Configuration.StreamType                    = StreamTypeNone;
    Configuration.OutputRateSmoothingFramesBetweenReCalculate = 8;

    for (i = 0; i < MAXIMUM_NUMBER_OF_TIMINGS_PER_MANIFESTATION; i++)
    {
        OutputRateSmoothing[i].FramesSinceLastCalculation = 0;
        OutputRateSmoothing[i].Index                      = 0;
        OutputRateSmoothing[i].LastRate                   = 0;
        OutputRateSmoothing[i].SubPPMPart                 = 0;
        OutputRateSmoothing[i].BaseValue                  = 1000000;
        OutputRateSmoothing[i].LastValue                  = 1000000;
        OutputRateSmoothing[i].OutputRateMovingTo         = false;
    }

    for (i = 0; i < MAXIMUM_WAITING_EVENTS; i++)
    {
        EventList[i].Id = INVALID_BUFFER_ID;
    }

    OS_InitializeMutex(&EventLock);
}

//
Manifestor_Base_c::~Manifestor_Base_c(void)
{
    Halt();

    OS_TerminateMutex(&EventLock);
}

// /////////////////////////////////////////////////////////////////////////
//
//      Halt :
//      Action  : Terminate access to any registered resources
//      Input   :
//      Output  :
//      Result  :
//

ManifestorStatus_t      Manifestor_Base_c::Halt(void)
{
    return BaseComponentClass_c::Halt();
}


// /////////////////////////////////////////////////////////////////////////
//
//      Reset function
//      Action  : Release any resources, and reset all variables
//      Input   :
//      Output  :
//      Result  :
//

ManifestorStatus_t      Manifestor_Base_c::Reset(void)
{
    if (TestComponentState(ComponentRunning))
    {
        Halt();
    }

    EventPending         = false;
    NextEvent            = 0;
    LastEvent            = 0;
    FrameCountManifested = 0ull;

    for (int i = 0; i < MAXIMUM_WAITING_EVENTS; i++)
    {
        EventList[i].Id = INVALID_BUFFER_ID;
    }

    return BaseComponentClass_c::Reset();
}


// /////////////////////////////////////////////////////////////////////////
//
//      Connect :
//      Action  : Save details of port on which to place manifested buffers
//      Input   : Pointer to port to use for finished decode frames
//      Output  :
//      Results :
//

ManifestorStatus_t      Manifestor_Base_c::Connect(Port_c *Port)
{
    SE_DEBUG(GetGroupTrace(), "\n");
    if (Port == NULL)
    {
        SE_ERROR("Incorrect parameter\n");
        return ManifestorError;
    }
    if (mOutputPort != NULL)
    {
        SE_WARNING("Port already connected\n");
    }

    mOutputPort  = Port;
    return ManifestorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      GetSurfaceParameters :
//      Action  : Fill in private structure with details about display surface
//      Input   : Opaque pointer to structure to complete
//      Output  : Filled in structure
//      Results :
//

ManifestorStatus_t      Manifestor_Base_c::GetSurfaceParameters(OutputSurfaceDescriptor_t   **SurfaceParameters, unsigned int *NumSurfaces)
{
    *NumSurfaces = 0;
    return ManifestorNoError;
}


// /////////////////////////////////////////////////////////////////////////
//
//      The GetNextQueuedManifestTime function :
//      Action  : Return the earliest system time at which the next frame to be queued will be manifested
//      Input   : Pointer to 64-bit system time variable
//      Output  : Estimated time
//      Results :
//

ManifestorStatus_t      Manifestor_Base_c::GetNextQueuedManifestTime(unsigned long long    *Time)
{
    return ManifestorNoError;
}


//{{{  ReleaseQueuedDecodeBuffers
//{{{  doxynote
/// \brief      Passes onto the output ring any decode buffers that are currently queued,
///             but not in the process of being manifested.
/// \return     Buffer index of last buffer sent for display
//}}}
ManifestorStatus_t      Manifestor_Base_c::ReleaseQueuedDecodeBuffers(void)
{
    int i;
    SE_DEBUG(GetGroupTrace(), "\n");
    FlushDisplayQueue();
    OS_LockMutex(&EventLock);
    NextEvent   = 0;
    LastEvent   = 0;

    for (i = 0; i < MAXIMUM_WAITING_EVENTS; i++)
    {
        EventList[i].Id         = INVALID_BUFFER_ID;
    }

    OS_UnLockMutex(&EventLock);
    return ManifestorNoError;
}
//}}}


// /////////////////////////////////////////////////////////////////////////
//
//      QueueNullManifestation :
//      Action  : Insert null frame into display sequence
//      Input   :
//      Output  :
//      Results :
//

ManifestorStatus_t      Manifestor_Base_c::QueueNullManifestation(void)
{
    SE_DEBUG(GetGroupTrace(), "\n");
    return ManifestorNoError;
}

//{{{  QueueEventSignal
//{{{  doxynote
/// \brief      Copy event record to be signalled when last queued buffer is displayed
/// \param      Event Pointer to a player 2 event record to be signalled
/// \return     Success if saved, failure if event queue full
//}}}
ManifestorStatus_t      Manifestor_Base_c::QueueEventSignal(PlayerEventRecord_t   *Event)
{
    SE_DEBUG(GetGroupTrace(), "\n");
    OS_LockMutex(&EventLock);

    if (((LastEvent + 1) % MAXIMUM_WAITING_EVENTS) == NextEvent)
    {
        //SE_ERROR("Event queue overflow\n");
        NextEvent++;

        if (NextEvent == MAXIMUM_WAITING_EVENTS)
        {
            NextEvent   = 0;
        }

        //OS_UnLockMutex (&EventLock);
        //return ManifestorError;
    }

    EventList[LastEvent].Id     = GetBufferId();
    EventList[LastEvent].Event  = *Event;
    EventPending                = true;
    LastEvent++;

    if (LastEvent == MAXIMUM_WAITING_EVENTS)
    {
        LastEvent       = 0;
    }

    OS_UnLockMutex(&EventLock);
    return ManifestorNoError;
}
//}}}

//{{{  ServiceEventQueue
//{{{  doxynote
/// \brief      Signal all events associated with buffer just manifested
///             We need to travel down the event queue signalling all events
///             including the one associated with the chosen buffer.
/// \param Id   Index of buffer
/// \return
//}}}
ManifestorStatus_t      Manifestor_Base_c::ServiceEventQueue(unsigned int    Id)
{
    bool        EventsToQueue;
    bool        FoundId         = false;
    SE_VERBOSE(GetGroupTrace(), "Id %d\n", Id);
    OS_LockMutex(&EventLock);
    EventsToQueue       = (LastEvent != NextEvent);
    ManifestorStatus_t MfStatus = ManifestorNoError;

    while (EventsToQueue)
    {
        if (EventList[NextEvent].Id == Id)
        {
            FoundId     = true;
        }

        EventList[NextEvent].Id = INVALID_BUFFER_ID;

        if (EventList[NextEvent].Event.Code != EventIllegalIdentifier)
        {
            PlayerStatus_t Status = Stream->SignalEvent(&EventList[NextEvent].Event);
            if (Status != PlayerNoError)
            {
                SE_ERROR("Failed to signal event\n");
                MfStatus = ManifestorError;
            }
        }

        NextEvent++;

        if (NextEvent == MAXIMUM_WAITING_EVENTS)
        {
            NextEvent   = 0;
        }

        if ((NextEvent == LastEvent) || (FoundId && (EventList[NextEvent].Id != ANY_BUFFER_ID) && (EventList[NextEvent].Id != Id)))
        {
            EventsToQueue       = false;
        }
    }

    OS_UnLockMutex(&EventLock);
    return MfStatus;
}
//}}}

// /////////////////////////////////////////////////////////////////////
//
//      The get decode buffer count function

ManifestorStatus_t   Manifestor_Base_c::DerivePPMValueFromOutputRateAdjustment(
    Rational_t               OutputRateAdjustment,
    int                     *PPMValue,
    unsigned int             Index)
{
    Rational_t      SubPartValue0;
    Rational_t      SubPartValue1;

    //
    // Reset on changed rate
    //

    if (Index > (MAXIMUM_NUMBER_OF_TIMINGS_PER_MANIFESTATION - 1) || PPMValue == NULL)
    {
        SE_ERROR("Invalid parameter\n");
        return ManifestorError;
    }

    if (OutputRateAdjustment != OutputRateSmoothing[Index].LastRate)
    {
        if (Configuration.StreamType == StreamTypeAudio) { SE_DEBUG(GetGroupTrace(), "Output Clock Change - %3d (%d)\n", OutputRateSmoothing[Index].BaseValue - IntegerPart(OutputRateAdjustment * 1000000), IntegerPart(OutputRateAdjustment * 1000000)); }

        OutputRateSmoothing[Index].FramesSinceLastCalculation   = 0;
        OutputRateSmoothing[Index].Index                        = 0;
        OutputRateSmoothing[Index].LastRate                     = OutputRateAdjustment;
        OutputRateSmoothing[Index].SubPPMPart                   = Remainder(OutputRateAdjustment * 1000000);
        OutputRateSmoothing[Index].BaseValue                    = IntegerPart(OutputRateAdjustment * 1000000);
        OutputRateSmoothing[Index].OutputRateMovingTo           = true;
    }

    //
    // If we have changed rate, and are moving to the new rate, we only allow PPM_ADJUSTMENT_PER_FRAME_SLOW_RATE ppm change per frame
    // when drift is <= MINIMUM_PPM_FOR_SLOW_RATE_CHANGE
    // else PPM_ADJUSTMENT_PER_FRAME_FAST_RATE PPM per change is allowed.
    //

    if (OutputRateSmoothing[Index].OutputRateMovingTo && (OutputRateSmoothing[Index].LastValue != OutputRateSmoothing[Index].BaseValue))
    {
        int PPMPerFrame = PPM_ADJUSTMENT_PER_FRAME_SLOW_RATE;
        int DifferenceBaseVsLast =  OutputRateSmoothing[Index].LastValue - OutputRateSmoothing[Index].BaseValue;
        if (!inrange(DifferenceBaseVsLast, -MINIMUM_PPM_FOR_SLOW_RATE_CHANGE, MINIMUM_PPM_FOR_SLOW_RATE_CHANGE))
        {
            PPMPerFrame = PPM_ADJUSTMENT_PER_FRAME_FAST_RATE;
        }

        OutputRateSmoothing[Index].LastValue                    = OutputRateSmoothing[Index].LastValue +
                                                                  ((OutputRateSmoothing[Index].LastValue > OutputRateSmoothing[Index].BaseValue) ? -PPMPerFrame : PPMPerFrame);
        *PPMValue               = OutputRateSmoothing[Index].LastValue;
        return ManifestorNoError;
    }

    OutputRateSmoothing[Index].OutputRateMovingTo          = false;

    //
    // Set default value, and check if we should just return
    //

    if (OutputRateSmoothing[Index].FramesSinceLastCalculation < Configuration.OutputRateSmoothingFramesBetweenReCalculate)
    {
        OutputRateSmoothing[Index].FramesSinceLastCalculation++;
        *PPMValue               = OutputRateSmoothing[Index].LastValue;
        return ManifestorNoError;
    }

    //
    // Calculate current and next subpart values
    //
    SubPartValue0                       = OutputRateSmoothing[Index].Index * OutputRateSmoothing[Index].SubPPMPart;
    SubPartValue1                       = SubPartValue0 + OutputRateSmoothing[Index].SubPPMPart;
    OutputRateSmoothing[Index].Index++;
    OutputRateSmoothing[Index].LastValue        = (SubPartValue0.IntegerPart() == SubPartValue1.IntegerPart()) ?
                                                  OutputRateSmoothing[Index].BaseValue :
                                                  OutputRateSmoothing[Index].BaseValue + 1;
//
    OutputRateSmoothing[Index].FramesSinceLastCalculation = 0;
    *PPMValue                           = OutputRateSmoothing[Index].LastValue;
    return ManifestorNoError;
}

ManifestorStatus_t  Manifestor_Base_c::GetCapabilities(unsigned int  *Capabilities)
{
    *Capabilities       = Configuration.Capabilities;
    return ManifestorNoError;
}

//}}}
//{{{  GetFrameCount
//{{{  doxynote
/// \brief Get number of frames appeared on the display
/// \param Framecount   Pointer to FrameCount variable
/// \return             Success
//}}}
ManifestorStatus_t Manifestor_Base_c::GetFrameCount(unsigned long long *FrameCount)
{
    *FrameCount         = this->FrameCountManifested;
    return ManifestorNoError;
}

ManifestorStatus_t Manifestor_Base_c::GetNumberOfTimings(unsigned int *NumTimes)
{
    *NumTimes         = 1;
    return ManifestorNoError;
}
