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

#include "osinline.h"
#include "manifestor_video_sourceGrab.h"

#undef TRACE_TAG
#define TRACE_TAG   "Manifestor_VideoSrc_c"

//#define BLIT_FORMAT_CONVERSION 1

///{{{ Constructor
/// \brief                      Initialise state
/// \return                     Success or fail
Manifestor_VideoSrc_c::Manifestor_VideoSrc_c(void)
    : Manifestor_Source_c()
    , InputWindow()
    , VideoMetadataHelper()
    , ManifestationComponent()
    , BlitterHandle(NULL)
    , BlitterSrce()
    , BlitterDest()

{
    if (InitializationStatus != ManifestorNoError)
    {
        SE_ERROR("Initialization status not valid - aborting init\n");
        return;
    }

    SE_VERBOSE(group_manifestor_video_grab, "\n");

    SetGroupTrace(group_manifestor_video_grab);

    // Supply some defaults
    SurfaceDescriptor.StreamType                = StreamTypeVideo;
    SurfaceDescriptor.ClockPullingAvailable     = false;
    SurfaceDescriptor.DisplayWidth              = FRAME_GRAB_DEFAULT_DISPLAY_WIDTH;
    SurfaceDescriptor.DisplayHeight             = FRAME_GRAB_DEFAULT_DISPLAY_HEIGHT;
    SurfaceDescriptor.Progressive               = true;
    SurfaceDescriptor.FrameRate                 = FRAME_GRAB_DEFAULT_FRAME_RATE; // rational
    SurfaceDescriptor.PercussiveCapable         = true;
    ManifestationComponent                      = PrimaryManifestationComponent;
    // But request that we set output to match input rates
    SurfaceDescriptor.InheritRateAndTypeFromSource = true;
}
//}}}

///{{{ Destructor
/// \brief                      Release video Source resources
/// \return                     Success or fail
Manifestor_VideoSrc_c::~Manifestor_VideoSrc_c(void)
{
    SE_VERBOSE(group_manifestor_video_grab, "\n");
}

//}}}
//{{{  PrepareToPull
//{{{  doxynote
/// \brief              Prepare the frame in the capture Buffer
/// \param              Buffer containing frame pulled by memorySink
/// \result             Success if frame displayed, fail if not displayed or a frame has already been displayed
//}}}
ManifestorStatus_t      Manifestor_VideoSrc_c::PrepareToPull(class Buffer_c                      *Buffer,
                                                             stm_se_capture_buffer_t             *CaptureBuffer,
                                                             struct ManifestationOutputTiming_s  *VideoOutputTiming)
{
    struct ParsedVideoParameters_s     *VideoParameters;
    struct ParsedFrameParameters_s     *ParsedFrameParameters;

    SE_VERBOSE(group_manifestor_video_grab, "\n");

    Buffer->ObtainMetaDataReference(Player->MetaDataParsedVideoParametersType, (void **)&VideoParameters);
    SE_ASSERT(VideoParameters != NULL);

    Buffer->ObtainMetaDataReference(Player->MetaDataParsedFrameParametersReferenceType, (void **)(&ParsedFrameParameters));
    SE_ASSERT(ParsedFrameParameters != NULL);

    //
    // Update OutputSurfaceDescriptor
    //
    if (VideoParameters->Content.FrameRate != 0)
    {
        SurfaceDescriptor.DisplayWidth          = VideoParameters->Content.DisplayWidth;
        SurfaceDescriptor.DisplayHeight         = VideoParameters->Content.DisplayHeight;
        SurfaceDescriptor.Progressive           = VideoParameters->Content.Progressive;
        SurfaceDescriptor.FrameRate             = VideoParameters->Content.FrameRate;
    }
    else
    {
        SurfaceDescriptor.DisplayWidth          = FRAME_GRAB_DEFAULT_DISPLAY_WIDTH;
        SurfaceDescriptor.DisplayHeight         = FRAME_GRAB_DEFAULT_DISPLAY_HEIGHT;
        SurfaceDescriptor.Progressive           = true;
        SurfaceDescriptor.FrameRate             = FRAME_GRAB_DEFAULT_FRAME_RATE; // rational
    }

    //
    // CaptureBuffer structure provides the size, physical and virtual address of the destination buffer
    //
    FramePhysicalAddress                    = (void *) Stream->GetDecodeBufferManager()->ComponentBaseAddress(Buffer, ManifestationComponent, PhysicalAddress);
    FrameVirtualAddress                     = (void *) Stream->GetDecodeBufferManager()->ComponentBaseAddress(Buffer, ManifestationComponent, CachedAddress);
    FrameSize                               = Stream->GetDecodeBufferManager()->ComponentSize(Buffer, ManifestationComponent);
    //
    // Meta Data setup
    //
    CaptureBuffer->u.uncompressed.system_time              = VideoOutputTiming->SystemPlaybackTime;
    CaptureBuffer->u.uncompressed.native_time_format       = TIME_FORMAT_US;
    CaptureBuffer->u.uncompressed.native_time              = ParsedFrameParameters->NormalizedPlaybackTime;
    setUncompressedMetadata(Buffer, &CaptureBuffer->u.uncompressed, VideoParameters);
    return ManifestorNoError;
}
//}}}

///{{{ PullFrameRead
/// \brief                      Prepare captureBuffer to be returned to memsink_pull
/// \param captureBufferAddr    Kernel Address of buffer provided by memsink
/// \param captureBufferLen     Length of buffer provided by memsink
/// \return                     Number of bytes copied
uint32_t        Manifestor_VideoSrc_c::PullFrameRead(uint8_t *captureBufferAddr, uint32_t captureBufferLen)
{
    struct SourceStreamBuffer_s        *StreamBuff      = NULL;
    stm_se_capture_buffer_t            *CaptureBuffer   = (stm_se_capture_buffer_t *)captureBufferAddr;
    ManifestorStatus_t                  Status;
    uint32_t                            bufferIndex;
    AssertComponentState(ComponentRunning);
    SE_DEBUG(group_manifestor_video_grab, "Input window onto decode buffer  captureBufferAddr %p, captureBufferLen %u\n",
             captureBufferAddr, captureBufferLen);
    //
    // Get StreamBuffer of the CurrentBuffer thanks to its index
    //
    CurrentBuffer->GetIndex(&bufferIndex);

    if (bufferIndex >= MAX_DECODE_BUFFERS)
    {
        SE_ERROR("invalid buffer index %d\n", bufferIndex);
        return 0;
    }

    //
    // retrieve StreamBuffer via its index
    //
    StreamBuff   = &SourceStreamBuffer[bufferIndex];
    //
    // Prepare frame pulled by memorySink
    //
    Status     = PrepareToPull(CurrentBuffer, CaptureBuffer, StreamBuff->OutputTiming);

    if (Status != ManifestorNoError)
    {
        return 0;
    }

    //
    // Copy Video buffer into provided buffer
    // First check buffer large enough
    //
    if (CaptureBuffer->buffer_length < FrameSize)
    {
        SE_ERROR("Video Capture buffer too small: expected %d got %d bytes\n", FrameSize, CaptureBuffer->buffer_length);
        return 0;
    }

#if 0
    // Perform software copy in case of HW copy issue
    memcpy((void *)CaptureBuffer->virtual_address,
           (void *)FrameVirtualAddress,
           FrameSize);
#else

    // prepare the source surface
    if (PrepareSrceSurfaceToBlit(CurrentBuffer, CaptureBuffer) == ManifestorError)
    {
        return 0;
    }

    // prepare the destination surface
    if (PrepareDestSurfaceToBlit(CaptureBuffer) == ManifestorError)
    {
        return 0;
    }

    // Apply the blit
    ApplyBlit();
#endif

    // Update the real copied frame size
    CaptureBuffer->payload_length = FrameSize;
    return (CaptureBuffer->payload_length);
}
//}}}

///{{{ initialiseConnection
/// \brief                      Perform after connection actions
/// \return                     Success or fail
ManifestorStatus_t  Manifestor_VideoSrc_c::initialiseConnection(void)
{
    //
    // do specific Video initialisation
    //
    BlitterHandle = stm_blitter_get(0);

    if (BlitterHandle == NULL)
    {
        return ManifestorError;
    }

    //
    // perform general initialisation
    //
    return Manifestor_Source_c::initialiseConnection();
}
///{{{ terminateConnection
/// \brief                      Perform before connection actions
/// \return                     Success or fail
ManifestorStatus_t  Manifestor_VideoSrc_c::terminateConnection(void)
{
    //
    // do specific Video initialisation
    //
    if (BlitterHandle)
    {
        stm_blitter_put(BlitterHandle);
    }

    BlitterHandle = NULL;

    if (BlitterSrce.surface)
    {
        stm_blitter_surface_put(BlitterSrce.surface);
    }

    BlitterSrce.surface = NULL;

    if (BlitterDest.surface)
    {
        stm_blitter_surface_put(BlitterDest.surface);
    }

    BlitterDest.surface = NULL;
    //
    // perform general termination
    //
    return Manifestor_Source_c::terminateConnection();
}


///{{{ GetSurfaceParameters
/// \brief                      Return Video surface descriptor
/// \return                     Success or fail
ManifestorStatus_t      Manifestor_VideoSrc_c::GetSurfaceParameters(OutputSurfaceDescriptor_t   **SurfaceParameters, unsigned int *NumSurfaces)
{
    SE_VERBOSE(group_manifestor_video_grab, "\n");
    *SurfaceParameters  = &SurfaceDescriptor;
    *NumSurfaces        = 1;
    return ManifestorNoError;
}
//}}}

//}}}
//{{{  PrepareSrceSurfaceToBlit
//{{{  doxynote
/// \brief              Prepare the source surface to blit
/// \param              Video frame captured structure
/// \result             Success if blitter surface allocated
ManifestorStatus_t      Manifestor_VideoSrc_c::PrepareSrceSurfaceToBlit(class Buffer_c          *Buffer,
                                                                        stm_se_capture_buffer_t *capture_buffer)
{
    unsigned int      chromaBufferOffset;

    if (BlitterSrce.surface)
    {
        stm_blitter_surface_put(BlitterSrce.surface);
    }

    BlitterSrce.surface = NULL;
    // Convert the source origin into something useful to the blitter. The incoming coordinates can be in
    // either whole integer or multiples of a 16th or a 32nd of a pixel/scanline.
    BlitterSrce.rect.position.x =
        capture_buffer->u.uncompressed.video.window_of_interest.x << (1 * 16);
    BlitterSrce.rect.position.y =
        capture_buffer->u.uncompressed.video.window_of_interest.y << (1 * 16);
    //
    // Will need the chroma buffer offset
    //
    chromaBufferOffset = Stream->GetDecodeBufferManager()->Chroma(Buffer, ManifestationComponent) -
                         Stream->GetDecodeBufferManager()->Luma(Buffer, ManifestationComponent);

    //
    // Determine Buffer_format and pixel_depth of the manifestation component
    //
    switch (Stream->GetDecodeBufferManager()->ComponentDataType(ManifestationComponent))
    {
    case FormatVideo420_MacroBlock:
        BlitterSrce.format = STM_BLITTER_SF_YCBCR420MB;
        BlitterSrce.buffer_add.cbcr_offset = chromaBufferOffset;
        break;

    case FormatVideo420_PairedMacroBlock:
        BlitterSrce.format = STM_BLITTER_SF_YCBCR420MB;
        BlitterSrce.buffer_add.cbcr_offset = chromaBufferOffset;
        break;

    case FormatVideo8888_ARGB:
        BlitterSrce.format = STM_BLITTER_SF_ARGB;
        break;

    case FormatVideo888_RGB:
        BlitterSrce.format = STM_BLITTER_SF_RGB24;
        break;

    case FormatVideo565_RGB:
        BlitterSrce.format = STM_BLITTER_SF_RGB565;
        break;

    case FormatVideo422_Raster:
        BlitterSrce.format = STM_BLITTER_SF_UYVY;
        break;

    case FormatVideo420_PlanarAligned:
    case FormatVideo420_Planar:
        BlitterSrce.format = STM_BLITTER_SF_I420;
        BlitterSrce.buffer_add.cb_offset = chromaBufferOffset;  /* FIXME */
        BlitterSrce.buffer_add.cr_offset = chromaBufferOffset;  /* FIXME */
        break;

    case FormatVideo422_Planar:
        BlitterSrce.format = STM_BLITTER_SF_YV61;
        BlitterSrce.buffer_add.cb_offset = chromaBufferOffset;  /* FIXME */
        BlitterSrce.buffer_add.cr_offset = chromaBufferOffset;  /* FIXME */
        break;

    case FormatVideo422_YUYV:
        BlitterSrce.format = STM_BLITTER_SF_YUY2;
        break;

    case FormatVideo420_Raster2B:
        BlitterSrce.format = STM_BLITTER_SF_NV12;
        BlitterSrce.buffer_add.cbcr_offset = chromaBufferOffset;
        break;

    default:
        SE_ERROR("Unsupported display format (%d)\n", Stream->GetDecodeBufferManager()->ComponentDataType(ManifestationComponent));
        return ManifestorError;
        break;
    }

    BlitterSrce.rect.size.w        = capture_buffer->u.uncompressed.video.window_of_interest.width;
    BlitterSrce.rect.size.h        = capture_buffer->u.uncompressed.video.window_of_interest.height;
    BlitterSrce.buffer_dimension.w = capture_buffer->u.uncompressed.video.video_parameters.width;
    BlitterSrce.buffer_dimension.h = capture_buffer->u.uncompressed.video.video_parameters.height;
    BlitterSrce.pitch              = capture_buffer->u.uncompressed.video.pitch;
    BlitterSrce.buffer_size        = FrameSize;
    BlitterSrce.buffer_add.base = (long unsigned int) FramePhysicalAddress;

    //
    // Determine the colour Space
    //

    switch (capture_buffer->u.uncompressed.video.video_parameters.colorspace)
    {
    case STM_SE_COLORSPACE_SMPTE170M:
        BlitterSrce.cspace = STM_BLITTER_SCS_BT601;
        break;

    case STM_SE_COLORSPACE_SMPTE240M:
        BlitterSrce.cspace = STM_BLITTER_SCS_BT709;
        break;

    case STM_SE_COLORSPACE_BT709:
        BlitterSrce.cspace = STM_BLITTER_SCS_BT709;
        break;

    case STM_SE_COLORSPACE_BT470_SYSTEM_M:
        BlitterSrce.cspace = STM_BLITTER_SCS_BT601;
        break;

    case STM_SE_COLORSPACE_BT470_SYSTEM_BG:
        BlitterSrce.cspace = STM_BLITTER_SCS_BT601;
        break;

    case STM_SE_COLORSPACE_SRGB:
        BlitterSrce.cspace = STM_BLITTER_SCS_RGB;
        break;

    default:
        BlitterSrce.cspace = (BlitterSrce.format & STM_BLITTER_SF_YCBCR) ?
                             (capture_buffer->u.uncompressed.video.video_parameters.width > SD_WIDTH ?
                              STM_BLITTER_SCS_BT601 : STM_BLITTER_SCS_BT709) : STM_BLITTER_SCS_RGB;
        break;
    }

#ifndef BLIT_FORMAT_CONVERSION
    BlitterSrce.format                 = STM_BLITTER_SF_ARGB;
    BlitterSrce.cspace                 = STM_BLITTER_SCS_RGB;
    BlitterSrce.buffer_dimension.w     = BlitterSrce.buffer_size / (BlitterSrce.buffer_dimension.h * 4);
    BlitterSrce.pitch                  = BlitterSrce.buffer_dimension.w * 4;
    BlitterSrce.rect.size.w            = BlitterSrce.buffer_dimension.w;
    BlitterSrce.rect.size.h            = BlitterSrce.buffer_dimension.h;
#endif
    BlitterSrce.surface = stm_blitter_surface_new_preallocated(
                              BlitterSrce.format,
                              BlitterSrce.cspace,
                              &BlitterSrce.buffer_add,
                              BlitterSrce.buffer_size,
                              &BlitterSrce.buffer_dimension,
                              BlitterSrce.pitch);

    if (IS_ERR(BlitterSrce.surface))
    {
        SE_ERROR("couldn't create source surface: %ld\n", PTR_ERR(BlitterSrce.surface));
        BlitterSrce.surface = NULL;
        return ManifestorError;
    }

    return ManifestorNoError;
}
//}}}

//{{{  PrepareDestSurfaceToBlit
//{{{  doxynote
/// \brief              Prepare the destination surface to bit
/// \param
/// \result             Success if frame displayed, fail if not displayed or a frame has already been displayed
ManifestorStatus_t      Manifestor_VideoSrc_c::PrepareDestSurfaceToBlit(stm_se_capture_buffer_t   *capture_buffer)
{
    if (BlitterDest.surface)
    {
        stm_blitter_surface_put(BlitterDest.surface);
    }

    BlitterDest.surface = NULL;
#ifdef  BLIT_FORMAT_CONVERSION
//
// Force destination surface format to STM_BLITTER_SCS_RGB / STM_BLITTER_SF_ARGB
//
    BlitterDest.format                 = STM_BLITTER_SF_ARGB;
    BlitterDest.cspace                 = STM_BLITTER_SCS_RGB;
    BlitterDest.buffer_add             = BlitterSrce.buffer_add;
    BlitterDest.buffer_add.cbcr_offset = 0;
    BlitterDest.buffer_add.cb_offset   = 0;
    BlitterDest.buffer_add.cr_offset   = 0;
    BlitterDest.rect                   = BlitterSrce.rect;
    BlitterDest.rect.position.x        = 0;
    BlitterDest.rect.position.y        = 0;
    BlitterDest.pitch                  = capture_buffer->rectangle.width * (32 / 8); // 32 bit ARGB8888
    BlitterDest.buffer_dimension       = BlitterSrce.buffer_dimension;
//
// Update capture structure accordingly (TBD)
//
    capture_buffer->u.uncompressed.video.surface_format      = SURFACE_FORMAT_VIDEO_8888_ARGB;
    ....
//
//  don't forget to set new FrameSize after blit conversion
//
#else
    BlitterDest.buffer_add             = BlitterSrce.buffer_add;
    BlitterDest.buffer_add.cbcr_offset = BlitterSrce.buffer_add.cbcr_offset;
    BlitterDest.buffer_add.cb_offset   = BlitterSrce.buffer_add.cb_offset;
    BlitterDest.buffer_add.cr_offset   = BlitterSrce.buffer_add.cr_offset;
    BlitterDest.format                 = BlitterSrce.format;
    BlitterDest.cspace                 = BlitterSrce.cspace;
    BlitterDest.buffer_dimension       = BlitterSrce.buffer_dimension;
    BlitterDest.pitch                  = BlitterSrce.pitch;
    BlitterDest.rect                   = BlitterSrce.rect;
#endif
    //
    // Destination buffer address is specified in the captureBuffer struct
    //
    BlitterDest.buffer_add.base      = (unsigned long)(capture_buffer->physical_address);
    BlitterDest.buffer_size          = static_cast<unsigned long>(capture_buffer->buffer_length);
    BlitterDest.surface = stm_blitter_surface_new_preallocated(
                              BlitterDest.format,
                              BlitterDest.cspace,
                              &BlitterDest.buffer_add,
                              BlitterDest.buffer_size,
                              &BlitterDest.buffer_dimension,
                              BlitterDest.pitch);

    if (IS_ERR(BlitterDest.surface))
    {
        SE_ERROR("couldn't create destination surface: %ld\n",
                 PTR_ERR(BlitterDest.surface));
        BlitterDest.surface = NULL;
        return ManifestorError;
    }

    return ManifestorNoError;
}
//{{{  ApplyBlit
//{{{  doxynote
/// \brief              Blit the Source surface to destination surface
/// \result             Success if blit done
ManifestorStatus_t      Manifestor_VideoSrc_c::ApplyBlit()
{
    ManifestorStatus_t  status = ManifestorNoError;
    int res;
    stm_blitter_surface_add_fence(BlitterDest.surface);
#ifdef  BLIT_FORMAT_CONVERSION
    /* set blit flags - the source is _always_ in fixed point */
    stm_blitter_surface_set_blitflags(BlitterDest.surface,
                                      (stm_blitter_surface_blitflags_t)(STM_BLITTER_SBF_NONE | STM_BLITTER_SBF_SRC_XY_IN_FIXED_POINT));
    stm_blitter_surface_set_porter_duff(BlitterDest.surface, STM_BLITTER_PD_SOURCE);
    res = stm_blitter_surface_stretchblit(
              BlitterHandle,
              BlitterSrce.surface,
              &BlitterSrce.rect,
              BlitterDest.surface,
              &BlitterDest.rect, 1);
#else
    /* set blit flags - the source is _always_ in fixed point */
    stm_blitter_surface_set_blitflags(BlitterDest.surface,
                                      (stm_blitter_surface_blitflags_t)(STM_BLITTER_SBF_NONE));
//    stm_blitter_surface_set_porter_duff(BlitterDest.surface, STM_BLITTER_PD_SOURCE);
    stm_blitter_point_t dst_point = { 0, 0 };
    res = stm_blitter_surface_blit(
              BlitterHandle,
              BlitterSrce.surface,
              &BlitterSrce.rect,
              BlitterDest.surface,
              &dst_point, 1);
#endif

    if (!res)
    {
        /* success - wait for operation to finish */
        stm_blitter_serial_t serial;
        stm_blitter_surface_get_serial(BlitterDest.surface,  &serial);
        stm_blitter_wait(BlitterHandle, STM_BLITTER_WAIT_FENCE, serial);
    }
    else
    {
        status = ManifestorError;
        SE_ERROR("Error during blitter operation (%d)\n", res);
    }

    stm_blitter_surface_put(BlitterDest.surface);
    BlitterDest.surface = NULL;
    return status;
}
//}}}

