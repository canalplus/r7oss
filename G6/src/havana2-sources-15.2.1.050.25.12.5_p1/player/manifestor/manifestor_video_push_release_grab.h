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

#ifndef MANIFESTOR_VIDEO_PUSH_RELEASE_H
#define MANIFESTOR_VIDEO_PUSH_RELEASE_H

#include <stm_registry.h>

#include "player.h"
#include "manifestor_base.h"
#include "player_generic.h"
#include "player_playback.h"
#include "player_stream.h"
#include "manifestor_sourceGrab.h"
#include "metadata_helper.h"

// Following include is required to get struct Window_s
#include "manifestor_video.h"

#undef TRACE_TAG
#define TRACE_TAG   "Manifestor_VideoPushRelease_c"

// The maximum number of pushed buffer is the maximum decode buffer plus one for EOS buffer
#define MAX_PUSHED_BUFFERS      (MAX_DECODE_BUFFERS+1)

#define FRAME_GRAB_DEFAULT_DISPLAY_WIDTH                1280
#define FRAME_GRAB_DEFAULT_DISPLAY_HEIGHT               720
#define FRAME_GRAB_DEFAULT_FRAME_RATE                   60

class Manifestor_VideoPushRelease_c : public Manifestor_Source_Base_c
{
public:
    // Constructor / Destructor
    Manifestor_VideoPushRelease_c(void);
    virtual ~Manifestor_VideoPushRelease_c(void);

    // C++ implementation of C callback for frame release
    int                  SinkReleaseFrame(stm_object_h released_object);
    // C++ implementation of C callback for Eos event manager
    void                 EndOfStreamEvent(void);

    // from Manifestor_c
    ManifestorStatus_t  QueueDecodeBuffer(Buffer_t Buffer, ManifestationOutputTiming_t **Timing, unsigned int *NumTime);
    ManifestorStatus_t  FlushDisplayQueue(void);
    ManifestorStatus_t  GetNextQueuedManifestationTime(unsigned long long    *Time, unsigned int *NumTimes);
    ManifestorStatus_t  GetNativeTimeOfCurrentlyManifestedFrame(unsigned long long *Time)
    {SE_DEBUG(group_manifestor_video_grab, "\n"); *Time = INVALID_TIME; return ManifestorNoError;}
    unsigned int         GetBufferId(void);

    // override Manifestor_base_c class
    ManifestorStatus_t   GetSurfaceParameters(OutputSurfaceDescriptor_t   **SurfaceParameters, unsigned int *NumSurfaces);

    // Manifestor attachment methods
    ManifestorStatus_t  Connect(stm_object_h  SrcHandle, stm_object_h  SinkHandle);
    ManifestorStatus_t  Disconnect(stm_object_h  SinkHandle);

protected:
    // Display Information
    struct OutputSurfaceDescriptor_s SurfaceDescriptor;

private:
    // Current decoded buffer component type
    DecodeBufferComponentType_t         ManifestationComponent;

    // Connection info
    bool                                Connected;

    struct stm_data_interface_push_release_sink PushReleaseInterface;

    // handle of the Event subscription
    stm_event_subscription_h            EventSubscription;

    // Only one attachment allowed
    stm_object_h                        SinkHandle;

    BaseMetadataHelper_c                VideoMetadataHelper;

    // Buffer management
    OS_Mutex_t                          BufferLock;
    stm_se_capture_buffer_t             MetaDataForPushingArray [MAX_PUSHED_BUFFERS];
    stm_se_capture_buffer_t             CaptureBuffersForEOS;

    bool                                PushedMetadataArray     [MAX_PUSHED_BUFFERS];
    Buffer_t                            PushedBufferArray       [MAX_PUSHED_BUFFERS];

    // Source Frame buffer info
    void                               *FramePhysicalAddress;
    void                               *FrameVirtualAddress;
    uint32_t                            FrameSize;

    ManifestorStatus_t  initialiseConnection(void);
    ManifestorStatus_t  terminateConnection(void);
    uint32_t            pushedBufferCount(void);

    // Internal video frame preparation for pushing
    ManifestorStatus_t  PrepareToPush(class Buffer_c                      *Buffer,
                                      stm_se_capture_buffer_t             *CaptureBuffer,
                                      struct ManifestationOutputTiming_s  *VideoOutputTiming);


    void setUncompressedMetadata(class Buffer_c                       *Buffer,
                                 stm_se_uncompressed_frame_metadata_t *Meta,
                                 struct ParsedVideoParameters_s       *VideoParameters)
    {
        VideoMetadataHelper.setVideoUncompressedMetadata(ManifestationComponent,
                                                         Stream->GetDecodeBufferManager(),
                                                         Buffer, Meta, VideoParameters);
    };

    DISALLOW_COPY_AND_ASSIGN(Manifestor_VideoPushRelease_c);
};

#endif
