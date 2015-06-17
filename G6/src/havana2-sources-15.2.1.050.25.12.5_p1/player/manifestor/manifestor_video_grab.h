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

#ifndef MANIFESTOR_VIDEO_GRAB_H
#define MANIFESTOR_VIDEO_GRAB_H

#include <stm_event.h>

#include "osinline.h"
#include "manifestor_video.h"
#include "player_generic.h"
#include "player_playback.h"
#include "player_stream.h"
#include "havana_capture.h"
#include "allocinline.h"

#undef TRACE_TAG
#define TRACE_TAG   "Manifestor_VideoGrab_c"

#define FRAME_GRAB_DEFAULT_DISPLAY_WIDTH                1280
#define FRAME_GRAB_DEFAULT_DISPLAY_HEIGHT               720
#define FRAME_GRAB_DEFAULT_FRAME_RATE                   60

/* Generic window structure - in pixels */
struct Rectangle_s
{
    unsigned int     X;         /* X coordinate of top left corner */
    unsigned int     Y;         /* Y coordinate of top left corner */
    unsigned int     Width;     /* Width of window in pixels */
    unsigned int     Height;    /* Height of window in pixels */
};

/// Video manifestor based on the grab driver API.
class Manifestor_VideoGrab_c : public Manifestor_Video_c
{
public:
    /* Constructor / Destructor */
    Manifestor_VideoGrab_c(void);
    ~Manifestor_VideoGrab_c(void);

    /* Overrides for component base class functions */
    ManifestorStatus_t   Halt(void);
    ManifestorStatus_t   Reset(void);

    /* Manifestor video class functions */
    ManifestorStatus_t  OpenOutputSurface(void *DisplayDevice);
    ManifestorStatus_t  CloseOutputSurface(void);
    ManifestorStatus_t  UpdateOutputSurfaceDescriptor(void);

    ManifestorStatus_t  QueueBuffer(unsigned int                    BufferIndex,
                                    struct ParsedFrameParameters_s *FrameParameters,
                                    struct ParsedVideoParameters_s *VideoParameters,
                                    struct ManifestationOutputTiming_s **VideoOutputTimingArray,
                                    Buffer_t                        Buffer);
    ManifestorStatus_t  QueueNullManifestation(void);
    ManifestorStatus_t  FlushDisplayQueue(void);

    ManifestorStatus_t  Enable(void);
    ManifestorStatus_t  Disable(void);
    bool                GetEnable(void);

    ManifestorStatus_t  SynchronizeOutput(void);

    ManifestorStatus_t  RegisterCaptureDevice(class HavanaCapture_c          *CaptureDevice);
    void                ReleaseCaptureBuffer(unsigned int                    BufferIndex);

    friend  void VideoGrab_EndOfStreamEvent(unsigned int number_of_events, stm_event_info_t *eventsInfo);
private:
    /* Generic stream information*/
    class HavanaCapture_c              *CaptureDevice;

    /* handle of teh Event subscription */
    stm_event_subscription_h            EventSubscription;
    /* flag to allow subscription only when first buffer is queued*/
    bool                                MustSubscribeToEOS;

    /* Buffer information */
    struct capture_buffer_s             CaptureBuffers[MAX_DECODE_BUFFERS];
    struct capture_buffer_s             CaptureBuffersForEOS;

    ManifestorStatus_t  PrepareToQueue(class Buffer_c                         *Buffer,
                                       struct capture_buffer_s                *CaptureBuffer,
                                       struct ManifestationOutputTiming_s     **VideoOutputTimingArray);

    DISALLOW_COPY_AND_ASSIGN(Manifestor_VideoGrab_c);
};

#endif
