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

#ifndef MANIFESTOR_VIDEO_CRC_H
#define MANIFESTOR_VIDEO_CRC_H

#include "manifestor_video.h"

#define CRC_BUFFER_HEADROOM   12              /* Number of buffers to wait after queue is full before we restart queueing buffers */

#undef TRACE_TAG
#define TRACE_TAG   "Manifestor_VideoCRC_c"

/// Video manifestor based on the stgfb core driver API.
class Manifestor_VideoCRC_c : public Manifestor_Video_c
{
public:
    /* Constructor / Destructor */
    Manifestor_VideoCRC_c(void);
    ~Manifestor_VideoCRC_c(void);

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
                                    struct ManifestationOutputTiming_s *VideoOutputTiming,
                                    Buffer_t                        Buffer);

    ManifestorStatus_t  FlushDisplayQueue(void);

    ManifestorStatus_t  Enable(void);
    ManifestorStatus_t  Disable(void);
    bool                GetEnable(void);

protected:
    ManifestorStatus_t  Relay_SW_CRC_Write(char PictureType,
                                           unsigned int LumaCRC,
                                           unsigned int ChromaCRC,
                                           unsigned int playbacktime);

    ManifestorStatus_t  Relay_SW_CRC(Buffer_t   Buffer,
                                     struct ParsedVideoParameters_s *ParsedVideoParameters,
                                     unsigned int playbacktime);
};

#endif
