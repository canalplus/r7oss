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

#ifndef H_HAVANA_CAPTURE
#define H_HAVANA_CAPTURE

#include "havana_player.h"

#undef TRACE_TAG
#define TRACE_TAG   "HavanaCapture_c"

#define CAPTURE_DEFAULT_LATENCY  40000 // 2 frames at 50 fps

#define MAX_QUEUED_BUFFERS      (MAX_DECODE_BUFFERS+8)

/// Player capture class responsible for passing captured frames to the outside world
class HavanaCapture_c
{
public:
    HavanaCapture_c(void);
    ~HavanaCapture_c(void);

    HavanaStatus_t                      Init(class Manifestor_Base_c                *Manifestor);
    HavanaStatus_t                      DeInit(class Manifestor_Base_c                *Manifestor);

    HavanaStatus_t                      SetLatency(unsigned int                            Latency);
    HavanaStatus_t                      QueueBuffer(struct capture_buffer_s                *Buffer);
    HavanaStatus_t                      FlushQueue(void);
    HavanaStatus_t                      TerminateQueue(void);

    stream_buffer_capture_callback      RegisterBufferCaptureCallback(stm_se_event_context_h                  Context,
                                                                      stream_buffer_capture_callback          Callback);
    void                                BufferCaptureThread(void);
    void                                ReleaseCaptureBuffer(unsigned int                user_private);

private:
    OS_Mutex_t                          Lock;
    class Manifestor_Base_c            *Manifestor;
    struct capture_buffer_s            *CaptureBuffers[MAX_QUEUED_BUFFERS];
    unsigned int                        BufferIn;
    unsigned int                        BufferOut;
    struct capture_buffer_s            *BufferForRelease;

    /* Data shared with Capture signal process */
    OS_Event_t                          BufferReceived;
    OS_Event_t                          BufferCaptureThreadTerminated;
    bool                                BufferCaptureThreadRunning;
    stream_buffer_capture_callback      BufferCaptureCallback;
    stm_se_event_context_h              CallbackContext;
    unsigned int                        Latency;

    DISALLOW_COPY_AND_ASSIGN(HavanaCapture_c);
};

#endif
