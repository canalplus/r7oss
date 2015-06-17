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

#ifndef MANIFESTOR_SOURCE_H
#define MANIFESTOR_SOURCE_H

#include "osinline.h"
#include <stm_memsrc.h>
#include <stm_memsink.h>
#include <stm_event.h>
#include <stm_registry.h>

#include "allocinline.h"
#include "player.h"
#include "manifestor_base.h"
#include "player_generic.h"
#include "player_playback.h"
#include "player_stream.h"
#include "havana_stream.h"

#undef TRACE_TAG
#define TRACE_TAG   "Manifestor_Source_c"

struct SourceStreamBuffer_s
{
    unsigned int                        BufferIndex;
    bool                                EventPending;
    class Buffer_c                     *BufferClass;
    struct ManifestationOutputTiming_s *OutputTiming;

    unsigned long long                  NativePlaybackTime;
};


class Manifestor_Source_Base_c : public Manifestor_Base_c
{
public:
    // Manifestor attachment methods
    virtual ManifestorStatus_t  Connect(stm_object_h  SrcHandle, stm_object_h  SinkHandle) = 0;
    virtual ManifestorStatus_t  Disconnect(stm_object_h  SinkHandle) = 0;
};

class Manifestor_Source_c : public Manifestor_Source_Base_c
{
public:
    Manifestor_Source_c(void);
    virtual ~Manifestor_Source_c(void);


// from Manifestor_c
    ManifestorStatus_t  QueueDecodeBuffer(Buffer_t Buffer, ManifestationOutputTiming_t **TimingArray, unsigned int *NumTimes);
    ManifestorStatus_t  GetNextQueuedManifestationTime(unsigned long long    *Time, unsigned int *NumTimes);
    ManifestorStatus_t  GetNativeTimeOfCurrentlyManifestedFrame(unsigned long long *Time)
    {SE_DEBUG(GetGroupTrace(), "\n"); *Time = INVALID_TIME; return ManifestorNoError;}

// Manifestor attachment methods
    ManifestorStatus_t  Connect(stm_object_h  SrcHandle, stm_object_h  SinkHandle);
    ManifestorStatus_t  Disconnect(stm_object_h  SinkHandle);


// Method for MemorySink Pull access
    virtual bool            GetEndOfStream(uint8_t *captureBufferAddr);
    virtual uint32_t        PullFrameRead(uint8_t *captureBufferAddr, uint32_t captureBufferLen) = 0;
    virtual int32_t         PullFrameAvailable();
    virtual OS_Status_t     PullFrameWait();
    virtual void            PullFramePostRead();

// Method for Buffer release thread
    virtual void            waitListBufferThread(void);
    virtual stm_se_encode_stream_media_t GetMediaEncode() const { return STM_SE_ENCODE_STREAM_MEDIA_ANY; };

    friend void SourceGrab_EndOfStreamEvent(unsigned int number_of_events, stm_event_info_t *eventsInfo);
protected:
// Display Information
    struct OutputSurfaceDescriptor_s    SurfaceDescriptor;

// Connection info
    bool                                Connected;
    bool                                Interrupted;
    struct stm_data_interface_pull_sink PullSinkInterface;
    stm_event_subscription_h            EventSubscription;

// Buffer management
    OS_Mutex_t                          BufferLock;
    bool                                WaitListThreadRunning;
    OS_Event_t                          WaitListThreadTerminated;
    OS_Event_t                          BufferReceived;

    Buffer_t                            FakeEndOfStreamBuffer;
    Buffer_t                            CurrentBuffer;
    struct SourceStreamBuffer_s         SourceStreamBuffer[MAX_DECODE_BUFFERS];

    Buffer_t                            waitListBuffer[MAX_DECODE_BUFFERS + 1];
    uint32_t                            waitListLimit;
    uint32_t                            waitListNextExtract;
    uint32_t                            waitListNextInsert;

// Source Frame buffer info
    void                               *FramePhysicalAddress;
    void                               *FrameVirtualAddress;
    uint32_t                            FrameSize;

// specific Init/term connection functions
    virtual ManifestorStatus_t  initialiseConnection(void);
    virtual ManifestorStatus_t  terminateConnection(void);
    virtual ManifestorStatus_t  waitListBufferInsert(Buffer_t receivedBuffer);
    virtual ManifestorStatus_t  waitListBufferExtract(Buffer_t *extractedBuffer, bool waitExtract = true);
    virtual uint32_t            waitListBufferCount(void);

// specific buffer release thread

// from Manifestor_Base_c
    unsigned int        GetBufferId(void);
    ManifestorStatus_t  FlushDisplayQueue(void);

private:
    // Only one attachment allowed
    stm_object_h       SinkHandle;

    DISALLOW_COPY_AND_ASSIGN(Manifestor_Source_c);
};

#endif
