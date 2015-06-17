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

#ifndef H_MANIFESTOR_BASE
#define H_MANIFESTOR_BASE

#include "player.h"
#include "allocinline.h"

#undef TRACE_TAG
#define TRACE_TAG   "Manifestor_Base_c"

#define MAXIMUM_WAITING_EVENTS                  32
#define INVALID_BUFFER_ID                       0xffffffff
#define ANY_BUFFER_ID                           0xfffffffe

//
// The internal configuration for the manifestor base
//

#define MANIFESTOR_CAPABILITY_GRAB              0x01
#define MANIFESTOR_CAPABILITY_DISPLAY           0x02
#define MANIFESTOR_CAPABILITY_CRC               0x04
#define MANIFESTOR_CAPABILITY_SOURCE            0x08
#define MANIFESTOR_CAPABILITY_ENCODE            0x10
#define MANIFESTOR_CAPABILITY_PUSH_RELEASE      0x20

#define PPM_ADJUSTMENT_PER_FRAME_SLOW_RATE 1  // Allow 1 ppm change per frame when drift is <= 256
#define MINIMUM_PPM_FOR_SLOW_RATE_CHANGE 256
#define PPM_ADJUSTMENT_PER_FRAME_FAST_RATE 16 // Allow 16 ppm change per frame when drift is > 256

typedef struct ManifestorConfiguration_s
{
    const char                   *ManifestorName;
    PlayerStreamType_t            StreamType;

    unsigned int                  OutputRateSmoothingFramesBetweenReCalculate;
    unsigned int                  Capabilities;
} ManifestorConfiguration_t;

struct EventRecord_s
{
    unsigned int                Id;
    struct PlayerEventRecord_s  Event;
    EventRecord_s(void) : Id(0), Event() {}
};

/* Output rate smoothing data */
struct ManifestorOutputSmoothing_s
{
    unsigned int                         FramesSinceLastCalculation;
    unsigned int                         Index;
    Rational_t                           LastRate;
    Rational_t                           SubPPMPart;
    unsigned int                         BaseValue;
    unsigned int                         LastValue;
    bool                                 OutputRateMovingTo;
    ManifestorOutputSmoothing_s(void)
        : FramesSinceLastCalculation(0)
        , Index(0)
        , LastRate()
        , SubPPMPart()
        , BaseValue(0)
        , LastValue(0)
        , OutputRateMovingTo(false)
    {}
};

/// Framework for implementing manifestors.
class Manifestor_Base_c : public Manifestor_c
{
public:
    // Constructor/Destructor methods
    Manifestor_Base_c(void);
    ~Manifestor_Base_c(void);

    // Overrides for component base class functions
    ManifestorStatus_t   Halt(void);
    ManifestorStatus_t   Reset(void);

    // Manifestor class functions
    ManifestorStatus_t   Connect(Port_c *Port);
    ManifestorStatus_t   GetSurfaceParameters(OutputSurfaceDescriptor_t   **SurfaceParameters, unsigned int *NumSurfaces);
    ManifestorStatus_t   GetNumberOfTimings(unsigned int           *NumTimes);
    ManifestorStatus_t   GetNextQueuedManifestTime(unsigned long long    *Time);
    ManifestorStatus_t   ReleaseQueuedDecodeBuffers(void);
    ManifestorStatus_t   QueueNullManifestation(void);
    ManifestorStatus_t   QueueEventSignal(PlayerEventRecord_t   *Event);
    ManifestorStatus_t   GetFrameCount(unsigned long long    *FrameCount);

    void                 ResetOnStreamSwitch() {}

    // Provide null implementations of the video only implemented fns

    ManifestorStatus_t   SynchronizeOutput(void) { return PlayerNotSupported; }

    ManifestorStatus_t   GetCapabilities(unsigned int             *Capabilities);

    // Support functions for derived classes

    ManifestorStatus_t   DerivePPMValueFromOutputRateAdjustment(
        Rational_t    OutputRateAdjustment,
        int          *PPMValue,
        unsigned int  Index);
    virtual ManifestorStatus_t  RegisterCaptureDevice(class HavanaCapture_c *CaptureDevice) { return PlayerNotSupported; };
    virtual void                ReleaseCaptureBuffer(unsigned int            BufferIndex) { return ; };

protected:
    ManifestorConfiguration_t   Configuration;

    //  Output port for used buffers
    Port_c                     *mOutputPort;

    struct ManifestorOutputSmoothing_s   OutputRateSmoothing[MAXIMUM_NUMBER_OF_TIMINGS_PER_MANIFESTATION];

    /* Event control */
    bool                        EventPending;
    struct EventRecord_s        EventList[MAXIMUM_WAITING_EVENTS];
    OS_Mutex_t                  EventLock;
    unsigned int                NextEvent;
    unsigned int                LastEvent;
    unsigned long long          FrameCountManifested;

    ManifestorStatus_t   ServiceEventQueue(unsigned int           Id);
    ManifestorStatus_t   FlushEventQueue(void);

    virtual unsigned int GetBufferId(void) = 0;
    virtual ManifestorStatus_t FlushDisplayQueue(void) = 0;

private:
    DISALLOW_COPY_AND_ASSIGN(Manifestor_Base_c);
};

#endif
