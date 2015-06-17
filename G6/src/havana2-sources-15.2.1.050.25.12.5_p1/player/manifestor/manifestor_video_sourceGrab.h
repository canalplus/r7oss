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

#ifndef MANIFESTOR_VIDEO_SOURCE_H
#define MANIFESTOR_VIDEO_SOURCE_H

#include "osinline.h"
#include "manifestor_video.h"
#include "player_generic.h"
#include "player_playback.h"
#include "player_stream.h"
#include "allocinline.h"
#include "manifestor_sourceGrab.h"
#include "metadata_helper.h"

#undef TRACE_TAG
#define TRACE_TAG   "Manifestor_VideoSrc_c"

extern "C" {
#include <blitter.h>
};

#define FRAME_GRAB_DEFAULT_DISPLAY_WIDTH                1280
#define FRAME_GRAB_DEFAULT_DISPLAY_HEIGHT               720
#define FRAME_GRAB_DEFAULT_FRAME_RATE                   60
#define SD_WIDTH                                        720

class Manifestor_VideoSrc_c : public Manifestor_Source_c
{
public:
    /* Constructor / Destructor */
    Manifestor_VideoSrc_c(void);
    ~Manifestor_VideoSrc_c(void);

// override Manifestor_base_c class
    ManifestorStatus_t   GetSurfaceParameters(OutputSurfaceDescriptor_t   **SurfaceParameters, unsigned int *NumSurfaces);

    struct Window_s     InputWindow;            /* Input window onto decode buffer */

// Method for MemorySink Pull access
    uint32_t             PullFrameRead(uint8_t *captureBufferAddr, uint32_t captureBufferLen);

// specific Init/term connection
    ManifestorStatus_t  initialiseConnection(void);
    ManifestorStatus_t  terminateConnection(void);

// override Manifestor_Source_c
    stm_se_encode_stream_media_t GetMediaEncode() const { return STM_SE_ENCODE_STREAM_MEDIA_VIDEO; };

private:
// Internal video frame preparation for pulling
    ManifestorStatus_t  PrepareToPull(class Buffer_c                      *Buffer,
                                      stm_se_capture_buffer_t             *CaptureBuffer,
                                      struct ManifestationOutputTiming_s  *VideoOutputTiming);

    ManifestorStatus_t  ApplyBlit(void);
    ManifestorStatus_t  PrepareDestSurfaceToBlit(stm_se_capture_buffer_t   *capture_buffer);
    ManifestorStatus_t  PrepareSrceSurfaceToBlit(class Buffer_c            *Buffer,
                                                 stm_se_capture_buffer_t   *capture_buffer);

    DISALLOW_COPY_AND_ASSIGN(Manifestor_VideoSrc_c);

    void setUncompressedMetadata(class Buffer_c    *Buffer,
                                 stm_se_uncompressed_frame_metadata_t *Meta,
                                 struct ParsedVideoParameters_s       *VideoParameters)
    {
        VideoMetadataHelper.setVideoUncompressedMetadata(ManifestationComponent,
                                                         Stream->GetDecodeBufferManager(), Buffer, Meta, VideoParameters);
    };

    // Private properties

    BaseMetadataHelper_c        VideoMetadataHelper;
    DecodeBufferComponentType_t ManifestationComponent;

    stm_blitter_t *BlitterHandle;
    struct
    {
        stm_blitter_surface_t           *surface;
        stm_blitter_surface_format_t     format;
        stm_blitter_surface_colorspace_t cspace;
        stm_blitter_dimension_t          buffer_dimension;
        stm_blitter_surface_address_t    buffer_add;
        unsigned long                    buffer_size;
        unsigned long                    pitch;
        stm_blitter_rect_t               rect;
    } BlitterSrce;

    struct
    {
        stm_blitter_surface_t           *surface;
        stm_blitter_surface_format_t     format;
        stm_blitter_surface_colorspace_t cspace;
        stm_blitter_dimension_t          buffer_dimension;
        stm_blitter_surface_address_t    buffer_add;
        unsigned long                    buffer_size;
        unsigned long                    pitch;
        stm_blitter_rect_t               rect;
    } BlitterDest;
};

#endif
