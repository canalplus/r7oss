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
#ifndef MANIFESTOR_ENCODE_H
#define MANIFESTOR_ENCODE_H

#include <stm_event.h>
#include <stm_registry.h>

#include "allocinline.h"
#include "player.h"
#include "manifestor_base.h"
#include "player_generic.h"
#include "player_playback.h"
#include "player_stream.h"
#include "havana_stream.h"
#include "ring_generic.h"
#include "release_buffer_interface.h"

#include "encoder.h"

#undef TRACE_TAG
#define TRACE_TAG   "Manifestor_Encoder_c"

#define MANIFESTOR_MAX_INPUT_BUFFERS (MAX_DECODE_BUFFERS + 1)

class Manifestor_Encoder_c : public Manifestor_Base_c, public ReleaseBufferInterface_c
{
public:
    Manifestor_Encoder_c();
    virtual ~Manifestor_Encoder_c(void);

    ManifestorStatus_t  Halt(void);
    ManifestorStatus_t  Reset(void);

    // Event Handling
    ManifestorStatus_t  ManageEventSubscription(stm_object_h  SrcHandle);
    ManifestorStatus_t  HandleEvent(unsigned int number_of_events, stm_event_info_t *eventsInfo);

    // from Manifestor_c
    ManifestorStatus_t  QueueDecodeBuffer(Buffer_t Buffer, ManifestationOutputTiming_t **TimingArray, unsigned int *NumTimes);
    ManifestorStatus_t  FlushDisplayQueue(void);

    ManifestorStatus_t  GetNextQueuedManifestationTime(unsigned long long    *Time, unsigned int *NumTimes);
    ManifestorStatus_t  GetNativeTimeOfCurrentlyManifestedFrame(unsigned long long *Time);
    ManifestorStatus_t  GetSurfaceParameters(OutputSurfaceDescriptor_t   **SurfaceParameters, unsigned int *NumSurfaces);

    // Manifestor attachment methods
    ManifestorStatus_t  Connect(stm_object_h  SrcHandle, stm_object_h  SinkHandle);
    ManifestorStatus_t  Disconnect(stm_object_h  SinkHandle);
    bool                IsConnected(stm_object_h SinkHandle) const { return EncodeStream == SinkHandle ? true : false; }

    ManifestorStatus_t  Enable(void);
    ManifestorStatus_t  Disable(void);

    bool                GetEnable(void);
    unsigned int        GetBufferId(void);

protected:
    // The attached Encoder details.
    // These may be used by the Audio/Video leaf classes.
    Encoder_c                          *Encoder;
    EncodeStream_c                     *EncodeStream;
    const EncoderBufferTypes           *Encoder_BufferTypes;

    // Other Manifestor types have this as an array of size MAXIMUM_NUMBER_OF_TIMINGS_PER_MANIFESTATION
    OutputSurfaceDescriptor_t           SurfaceDescriptor;

    OS_Mutex_t                          ConnectBufferLock;

    ManifestorStatus_t                  PurgeEncodeStream(void);
    ManifestorStatus_t                  PushEndOfStreamInputBuffer(void);

    // The leaf classes must implement the method to perform an Encode
    virtual ManifestorStatus_t          PrepareEncodeMetaData(Buffer_t  Buffer, Buffer_t  *EncodeBuffer) = 0;

private:
    DISALLOW_COPY_AND_ASSIGN(Manifestor_Encoder_c);

    // Release method of pushed  Encode input buffer
    PlayerStatus_t                      ReleaseBuffer(Buffer_t EncodeBuffer);

    // Event Management
    stm_event_subscription_h            EventSubscription;
    // Flag to perform subscriptions on first buffer
    bool                                MustSubscribeToEOS;

    // Connection info
    bool                                Connected;
    bool                                Interrupted;
    bool                                Enabled;

    // Pushed to encoder management
    Buffer_t                            EncodeBufferArray[ENCODER_MAX_INPUT_BUFFERS];

    // Handling of our Rings
    Buffer_t                            FakeEndOfStreamBuffer;
    unsigned int                        LastBufferId;
    Port_c                             *mEncodeStreamPort;
    // time of last manifested buffer
    unsigned long long          mLastEncodedPts;
};

#endif
