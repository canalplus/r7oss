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

#include "preproc_video_blitter.h"
#include "st_relayfs_se.h"

#undef TRACE_TAG
#define TRACE_TAG "Preproc_Video_Blitter_c"

// modified from linux/err.h
#define BLITTER_ERR(x) ((x == NULL) || ((unsigned long)x >= (unsigned long)-4095))

#define MIN_BLITTER_RECT_WIDTH    8
#define MIN_BLITTER_RECT_HEIGHT   8

typedef enum blitter_buffer_flags_e
{
    BLITTER_BUFFER_INTERLACED = (1L << 0),
    BLITTER_BUFFER_TOP        = (1L << 1),
    BLITTER_BUFFER_BOTTOM     = (1L << 2)
} blitter_buffer_flags_t;

static inline const char *StringifyBlitterColorspace(stm_blitter_surface_colorspace_t aBlitterColorspace)
{
    switch (aBlitterColorspace)
    {
        ENTRY(STM_BLITTER_SCS_RGB);
        ENTRY(STM_BLITTER_SCS_RGB_VIDEORANGE);
        ENTRY(STM_BLITTER_SCS_BT601);
        ENTRY(STM_BLITTER_SCS_BT601_FULLRANGE);
        ENTRY(STM_BLITTER_SCS_BT709);
        ENTRY(STM_BLITTER_SCS_BT709_FULLRANGE);
    default: return "<unknown blitter colorspace>";
    }
}

static inline const char *StringifyBlitterSurfaceFormat(stm_blitter_surface_format_t aBlitterSurfaceFormat)
{
    switch (aBlitterSurfaceFormat)
    {
        ENTRY(STM_BLITTER_SF_INVALID);
        ENTRY(STM_BLITTER_SF_RGB565);
        ENTRY(STM_BLITTER_SF_RGB24);
        ENTRY(STM_BLITTER_SF_RGB32);
        ENTRY(STM_BLITTER_SF_ARGB1555);
        ENTRY(STM_BLITTER_SF_ARGB4444);
        ENTRY(STM_BLITTER_SF_ARGB8565);
        ENTRY(STM_BLITTER_SF_ARGB);
        ENTRY(STM_BLITTER_SF_BGRA);
        ENTRY(STM_BLITTER_SF_LUT8);
        ENTRY(STM_BLITTER_SF_LUT4);
        ENTRY(STM_BLITTER_SF_LUT2);
        ENTRY(STM_BLITTER_SF_LUT1);
        ENTRY(STM_BLITTER_SF_ALUT88);
        ENTRY(STM_BLITTER_SF_ALUT44);
        ENTRY(STM_BLITTER_SF_A8);
        ENTRY(STM_BLITTER_SF_A1);
        ENTRY(STM_BLITTER_SF_AVYU);
        ENTRY(STM_BLITTER_SF_VYU);
        ENTRY(STM_BLITTER_SF_YUYV);
        ENTRY(STM_BLITTER_SF_UYVY);
        ENTRY(STM_BLITTER_SF_YV12);
        ENTRY(STM_BLITTER_SF_I420);
        ENTRY(STM_BLITTER_SF_YV16);
        ENTRY(STM_BLITTER_SF_YV61);
        ENTRY(STM_BLITTER_SF_YCBCR444P);
        ENTRY(STM_BLITTER_SF_NV12);
        ENTRY(STM_BLITTER_SF_NV21);
        ENTRY(STM_BLITTER_SF_NV16);
        ENTRY(STM_BLITTER_SF_NV61);
        ENTRY(STM_BLITTER_SF_YCBCR420MB);
        ENTRY(STM_BLITTER_SF_YCBCR422MB);
        ENTRY(STM_BLITTER_SF_AlRGB8565);
        ENTRY(STM_BLITTER_SF_AlRGB);
        ENTRY(STM_BLITTER_SF_BGRAl);
        ENTRY(STM_BLITTER_SF_AlLUT88);
        ENTRY(STM_BLITTER_SF_Al8);
        ENTRY(STM_BLITTER_SF_AlVYU);
        ENTRY(STM_BLITTER_SF_BGR24);
        ENTRY(STM_BLITTER_SF_RLD_BD);
        ENTRY(STM_BLITTER_SF_RLD_H2);
        ENTRY(STM_BLITTER_SF_RLD_H8);
        ENTRY(STM_BLITTER_SF_NV24);
        ENTRY(STM_BLITTER_SF_ABGR);
        ENTRY(STM_BLITTER_SF_COUNT);
    default: return "<unknown blitter surface format>";
    }
}

static const PreprocVideoCaps_t PreprocCaps =
{
    {ENCODE_WIDTH_MAX, ENCODE_HEIGHT_MAX},                     // MaxResolution
    {ENCODE_FRAMERATE_MAX, STM_SE_PLAY_FRAME_RATE_MULTIPLIER}, // MaxFramerate
    true,                                                      // DEISupport
    false,                                                     // TNRSupport
    ((1 << SURFACE_FORMAT_VIDEO_420_PLANAR)         |          // VideoFormatCaps
    (1 << SURFACE_FORMAT_VIDEO_420_PLANAR_ALIGNED) |
    (1 << SURFACE_FORMAT_VIDEO_422_PLANAR)         |
    (1 << SURFACE_FORMAT_VIDEO_422_RASTER)         |
    (1 << SURFACE_FORMAT_VIDEO_422_YUYV)           |
    (1 << SURFACE_FORMAT_VIDEO_420_RASTER2B)       |
    (1 << SURFACE_FORMAT_VIDEO_420_MACROBLOCK)     |
    (1 << SURFACE_FORMAT_VIDEO_420_PAIRED_MACROBLOCK))
};

Preproc_Video_Blitter_c::Preproc_Video_Blitter_c()
    : Preproc_Video_c()
    , blitter(NULL)
    , src(NULL)
    , dst(NULL)
    , src_bitmap()
    , dst_bitmap()
    , PreprocFrameBuffer2(NULL)
    , PreprocFrameBufferIntermediate(NULL)
    , mRelayfsIndex(0)
{
    mRelayfsIndex = st_relayfs_getindex_fortype_se(ST_RELAY_TYPE_ENCODER_VIDEO_PREPROC_INPUT);

    // TODO(pht) move FinalizeInit to a factory method
    InitializationStatus = FinalizeInit();
}

Preproc_Video_Blitter_c::~Preproc_Video_Blitter_c()
{
    Halt();

    st_relayfs_freeindex_fortype_se(ST_RELAY_TYPE_ENCODER_VIDEO_PREPROC_INPUT, mRelayfsIndex);
}

PreprocStatus_t Preproc_Video_Blitter_c::GetBlitSurfaceFormat(surface_format_t      Format,
                                                              stm_blitter_surface_format_t    *BlitFormat,
                                                              uint32_t *VAlign)
{
    *BlitFormat = STM_BLITTER_SF_INVALID;
    *VAlign = 1; // Vertical alignment
    // Vertical alignment derived from DecodeBufferManager_Base_c::FillOutComponentStructure

    switch (Format)
    {
    case SURFACE_FORMAT_VIDEO_420_PLANAR:
    case SURFACE_FORMAT_VIDEO_420_PLANAR_ALIGNED:
        *BlitFormat = STM_BLITTER_SF_I420; // U followed by V plane
        *VAlign     = 16;
        break;

    case SURFACE_FORMAT_VIDEO_422_PLANAR:
        *BlitFormat = STM_BLITTER_SF_YV61;
        *VAlign     = 16;
        break;

    case SURFACE_FORMAT_VIDEO_422_RASTER:
        *BlitFormat = STM_BLITTER_SF_UYVY;
        *VAlign     = 1;
        break;

    case SURFACE_FORMAT_VIDEO_422_YUYV:
        *BlitFormat = STM_BLITTER_SF_YUYV;
        *VAlign     = 32; // shouldn't this be 1 instead of 32
        break;

    case SURFACE_FORMAT_VIDEO_420_RASTER2B:
        *BlitFormat = STM_BLITTER_SF_NV12;
        *VAlign     = 32;
        break;

    case SURFACE_FORMAT_VIDEO_420_PAIRED_MACROBLOCK: // paired MB width
    case SURFACE_FORMAT_VIDEO_420_MACROBLOCK:
        *BlitFormat = STM_BLITTER_SF_YCBCR420MB;
        *VAlign     = 32;
        break;

    case SURFACE_FORMAT_VIDEO_8888_ARGB:

    //  *BlitFormat = STM_BLITTER_SF_ARGB;
    //  *VAlign        = 32;
    //break;
    case SURFACE_FORMAT_VIDEO_888_RGB:

    //  *BlitFormat = STM_BLITTER_SF_RGB24;
    //  *VAlign        = 32;
    //break;
    case SURFACE_FORMAT_VIDEO_565_RGB:

    //  *BlitFormat = STM_BLITTER_SF_RGB565;
    //  *VAlign        = 32;
    //break;
    case SURFACE_FORMAT_UNKNOWN:
    case SURFACE_FORMAT_MARKER_FRAME:
    case SURFACE_FORMAT_AUDIO:
    default:
        SE_ERROR("Unsupported video surface format 0x%08x\n", Format);
        return PreprocError;
    }

    return PreprocNoError;
}

PreprocStatus_t Preproc_Video_Blitter_c::GetBufferOffsetSize(stm_blitter_surface_format_t   BlitFormat,
                                                             stm_blitter_dimension_t *AlignedSize, stm_blitter_surface_address_t *BufAddr, unsigned long *BufSize)
{
    uint32_t  ComponentSize;
    ComponentSize = AlignedSize->w * AlignedSize->h; //AlignedSize->w is initialized with pitch
    *BufSize      = ComponentSize;

    // 3 planes format
    // 444 STM_BLITTER_SF_YCBCR444P
    // 422 STM_BLITTER_SF_YV16 STM_BLITTER_SF_YV61
    // 420 STM_BLITTER_SF_YV12 STM_BLITTER_SF_I420
    // 2 planes format
    // 422 STM_BLITTER_SF_NV16 STM_BLITTER_SF_NV61
    // 420 STM_BLITTER_SF_NV12 STM_BLITTER_SF_NV21
    // 422 MB STM_BLITTER_SF_YCBCR422MB
    // 420 MB STM_BLITTER_SF_YCBCR420MB
    // pitch not considered yet.

    switch (BlitFormat)
    {
    case STM_BLITTER_SF_YCBCR444P:
        BufAddr->cb_offset = ComponentSize;
        BufAddr->cr_offset = ComponentSize * 2;
        *BufSize           = ComponentSize * 3;
        break;

    case STM_BLITTER_SF_YV16:
        BufAddr->cr_offset = ComponentSize;
        BufAddr->cb_offset = ComponentSize + ComponentSize / 2;
        *BufSize           = ComponentSize * 2;
        break;

    case STM_BLITTER_SF_YV61:
        BufAddr->cb_offset = ComponentSize;
        BufAddr->cr_offset = ComponentSize + ComponentSize / 2;
        *BufSize           = ComponentSize * 2;
        break;

    case STM_BLITTER_SF_YV12:
        BufAddr->cr_offset = ComponentSize;
        BufAddr->cb_offset = ComponentSize + ComponentSize / 4;
        *BufSize           = ComponentSize * 3 / 2;
        break;

    case STM_BLITTER_SF_I420:
        BufAddr->cb_offset = ComponentSize;
        BufAddr->cr_offset = ComponentSize + ComponentSize / 4;
        *BufSize           = ComponentSize * 3 / 2;
        break;

    case STM_BLITTER_SF_NV16:
    case STM_BLITTER_SF_NV61:
        BufAddr->cbcr_offset = ComponentSize;
        *BufSize             = ComponentSize * 2;
        break;

    case STM_BLITTER_SF_NV12:
    case STM_BLITTER_SF_NV21:
        BufAddr->cbcr_offset = ComponentSize;
        *BufSize             = ComponentSize * 3 / 2;
        break;

    case STM_BLITTER_SF_YCBCR422MB:
        BufAddr->cbcr_offset = ComponentSize;
        *BufSize             = ComponentSize * 2;
        break;

    case STM_BLITTER_SF_YCBCR420MB:
        BufAddr->cbcr_offset = ComponentSize;
        *BufSize             = ComponentSize * 3 / 2;
        break;

    // These formats have alpha: is it going to be supported?
    case STM_BLITTER_SF_ARGB:
        *BufSize             = ComponentSize;
        AlignedSize->w       /= 4;
        break;

    case STM_BLITTER_SF_RGB24:
        *BufSize             = ComponentSize;
        AlignedSize->w       /= 3;
        break;

    case STM_BLITTER_SF_YUYV:
    case STM_BLITTER_SF_UYVY:
    case STM_BLITTER_SF_RGB565:
        *BufSize             = ComponentSize;
        AlignedSize->w       /= 2;
        break;

    default:
        SE_ERROR("Blitting surface format 0x%x not supported\n", src_bitmap.format);
        return PreprocError;
    }

    return PreprocNoError;
}

PreprocStatus_t Preproc_Video_Blitter_c::PrepareSrcBlitSurface(void *InputBufferAddrPhys, uint32_t BufferFlag)
{
    PreprocStatus_t Status;
    int blitStatus;
    uint32_t vAlign;
    stm_blitter_serial_t serial;
    void *PreprocBufferAddrPhys2;

    // Get surface format
    Status = GetBlitSurfaceFormat(PreprocMetaDataDescriptor->video.surface_format, &src_bitmap.format, &vAlign);
    if (Status != PreprocNoError)
    {
        SE_ERROR("Video surface format %d not supported\n", PreprocMetaDataDescriptor->video.surface_format);
        return Status;
    }

    // Additional vertical alignment check to ensure good value is used, otherwise SE alignment values are used
    if (PreprocMetaDataDescriptor->video.vertical_alignment >= 1)
    {
        vAlign = PreprocMetaDataDescriptor->video.vertical_alignment;
    }

    // Get base address
    src_bitmap.buffer_address.base = (unsigned long)InputBufferAddrPhys;
    src_bitmap.aligned_size.w      = PreprocMetaDataDescriptor->video.pitch; //initialize to pitch, will be changed by GetBufferOffsetSize
    src_bitmap.aligned_size.h      = ALIGN_UP(PreprocMetaDataDescriptor->video.video_parameters.height, vAlign);
    Status = GetBufferOffsetSize(src_bitmap.format, &src_bitmap.aligned_size, &src_bitmap.buffer_address, &src_bitmap.buffer_size);
    if (Status != PreprocNoError)
    {
        SE_ERROR("Failed in GetBufferOffsetSize\n");
        return Status;
    }

    src_bitmap.pitch           = PreprocMetaDataDescriptor->video.pitch;

    switch (PreprocMetaDataDescriptor->video.video_parameters.colorspace)
    {
    case STM_SE_COLORSPACE_SMPTE170M:       src_bitmap.colorspace = STM_BLITTER_SCS_BT601; break;

    case STM_SE_COLORSPACE_SMPTE240M:       src_bitmap.colorspace = STM_BLITTER_SCS_BT709; break;

    case STM_SE_COLORSPACE_BT709:           src_bitmap.colorspace = STM_BLITTER_SCS_BT709; break;

    case STM_SE_COLORSPACE_BT470_SYSTEM_M:  src_bitmap.colorspace = STM_BLITTER_SCS_BT601; break;

    case STM_SE_COLORSPACE_BT470_SYSTEM_BG: src_bitmap.colorspace = STM_BLITTER_SCS_BT601; break;

    case STM_SE_COLORSPACE_SRGB:            src_bitmap.colorspace = STM_BLITTER_SCS_RGB;   break;

    default:                                src_bitmap.colorspace = (src_bitmap.format & STM_BLITTER_SF_YCBCR) ?
                                                                        (PreprocMetaDataDescriptor->video.video_parameters.width > SD_WIDTH ?
                                                                         STM_BLITTER_SCS_BT601 : STM_BLITTER_SCS_BT709) : STM_BLITTER_SCS_RGB;
    }

    src_bitmap.size.w          = PreprocMetaDataDescriptor->video.video_parameters.width;
    src_bitmap.size.h          = PreprocMetaDataDescriptor->video.video_parameters.height;
    if (PreprocCtrl.EnableCropping == STM_SE_CTRL_VALUE_APPLY)
    {
        src_bitmap.rect.position.x = PreprocMetaDataDescriptor->video.window_of_interest.x;
        src_bitmap.rect.position.y = PreprocMetaDataDescriptor->video.window_of_interest.y;
        src_bitmap.rect.size.w     = PreprocMetaDataDescriptor->video.window_of_interest.width;
        src_bitmap.rect.size.h     = PreprocMetaDataDescriptor->video.window_of_interest.height;
    }
    else
    {
        src_bitmap.rect.position.x = 0;
        src_bitmap.rect.position.y = 0;
        src_bitmap.rect.size.w     = PreprocMetaDataDescriptor->video.video_parameters.width;
        src_bitmap.rect.size.h     = PreprocMetaDataDescriptor->video.video_parameters.height;
    }

    if (src_bitmap.format == STM_BLITTER_SF_YUYV)
    {
        // Convert YUYV to UYVY
        src = stm_blitter_surface_new_preallocated(src_bitmap.format, src_bitmap.colorspace, &src_bitmap.buffer_address,
                                                   src_bitmap.buffer_size, &src_bitmap.size, src_bitmap.pitch);

        if (BLITTER_ERR(src))
        {
            SE_ERROR("Failed to allocate src surface (%d)\n", BLITTER_ERR(src));
            return PreprocError;
        }

        Status = Preproc_Base_c::GetNewBuffer(&PreprocFrameBufferIntermediate);
        if (Status != PreprocNoError)
        {
            PREPROC_ERROR_RUNNING("Failed to get new intermediate buffer\n");
            return Status;
        }

        PreprocFrameBufferIntermediate->ObtainDataReference(NULL, NULL, (void **)(&PreprocBufferAddrPhys2), PhysicalAddress);
        if (PreprocBufferAddrPhys2 == NULL)
        {
            SE_ERROR("Failed to get preproc data buffer reference\n");
            return PreprocError;
        }

        dst_bitmap         = src_bitmap;
        dst_bitmap.format  = STM_BLITTER_SF_UYVY;
        dst_bitmap.buffer_address.base = (unsigned long)PreprocBufferAddrPhys2;
        dst = stm_blitter_surface_new_preallocated(dst_bitmap.format, dst_bitmap.colorspace, &dst_bitmap.buffer_address,
                                                   dst_bitmap.buffer_size, &dst_bitmap.size, dst_bitmap.pitch);
        if (dst == NULL)
        {
            SE_ERROR("Failed to allocate dst surface\n");
            return PreprocError;
        }

        DumpBlitterTaskDescriptor(&src_bitmap, &dst_bitmap);

        // Blit surface using fence mode
        blitStatus = stm_blitter_surface_set_blitflags(dst, STM_BLITTER_SBF_NONE);
        if (blitStatus < 0)
        {
            SE_ERROR("Failed to set blit flags (%d)\n", blitStatus);
            stm_blitter_surface_put(src);
            stm_blitter_surface_put(dst);
            stm_blitter_put(blitter);
            return PreprocError;
        }

        blitStatus = stm_blitter_surface_add_fence(dst);
        if (blitStatus < 0)
        {
            SE_ERROR("Failed to add fence (%d)\n", blitStatus);
            stm_blitter_surface_put(src);
            stm_blitter_surface_put(dst);
            return PreprocError;
        }

        blitStatus = stm_blitter_surface_blit(blitter, src, &src_bitmap.rect, dst, &dst_bitmap.rect.position, 1);
        if (blitStatus < 0)
        {
            SE_ERROR("Failed to blit surface (%d)\n", blitStatus);
            stm_blitter_surface_put(src);
            stm_blitter_surface_put(dst);
            PreprocFrameBufferIntermediate->DecrementReferenceCount(IdentifierEncoderPreprocessor);
            return PreprocError;
        }

        blitStatus = stm_blitter_get_serial(blitter, &serial);

        if (blitStatus < 0)
        {
            SE_ERROR("Failed to get serial blit (%d)\n", blitStatus);
            stm_blitter_surface_put(src);
            stm_blitter_surface_put(dst);
            return PreprocError;
        }

        blitStatus = stm_blitter_wait(blitter, STM_BLITTER_WAIT_FENCE, serial);
        if (blitStatus < 0)
        {
            SE_ERROR("Failed to wait for serial blit (%d)\n", blitStatus);
            stm_blitter_surface_put(src);
            stm_blitter_surface_put(dst);
            return PreprocError;
        }

        // Clear the surfaces
        stm_blitter_surface_put(src);
        stm_blitter_surface_put(dst);
        src = dst = NULL;
        // reset source
        src_bitmap = dst_bitmap;
    }

    if (BufferFlag & BLITTER_BUFFER_INTERLACED)
    {
        src_bitmap.pitch        <<= 1; //double the pitch
        src_bitmap.size.h       >>= 1; // half the height
        src_bitmap.rect.size.h  >>= 1; // half the height

        if (BufferFlag & BLITTER_BUFFER_BOTTOM)
        {
            src_bitmap.buffer_address.base += PreprocMetaDataDescriptor->video.pitch;

            // contiguous chroma buffer has pitch half that of luma
            if (src_bitmap.format == STM_BLITTER_SF_YV16 || src_bitmap.format == STM_BLITTER_SF_YV12 ||
                src_bitmap.format == STM_BLITTER_SF_YV61 || src_bitmap.format == STM_BLITTER_SF_I420)
            {
                src_bitmap.buffer_address.cr_offset -= (PreprocMetaDataDescriptor->video.pitch >> 1);
                src_bitmap.buffer_address.cb_offset -= (PreprocMetaDataDescriptor->video.pitch >> 1);
            }
        }
    }

    src = stm_blitter_surface_new_preallocated(src_bitmap.format, src_bitmap.colorspace, &src_bitmap.buffer_address,
                                               src_bitmap.buffer_size, &src_bitmap.size, src_bitmap.pitch);
    if (BLITTER_ERR(src))
    {
        SE_ERROR("Failed to allocate src surface (%d)\n", BLITTER_ERR(src));
        return PreprocError;
    }

    return PreprocNoError;
}

PreprocStatus_t Preproc_Video_Blitter_c::PrepareDstBlitSurface(void *PreprocBufferAddrPhys, ARCCtrl_t *ARCCtrl)
{
    // Prepare blitting surface for destination
    dst_bitmap.colorspace          = src_bitmap.colorspace;
    dst_bitmap.size.w              = ALIGN_UP(PreprocCtrl.Resolution.width, 16);
    dst_bitmap.size.h              = ALIGN_UP(PreprocCtrl.Resolution.height, 16);
    dst_bitmap.buffer_address.base = (unsigned long)PreprocBufferAddrPhys;
    dst_bitmap.rect.position.x     = ARCCtrl->DstRect.x;
    dst_bitmap.rect.position.y     = ARCCtrl->DstRect.y;
    dst_bitmap.rect.size.w         = ARCCtrl->DstRect.width;
    dst_bitmap.rect.size.h         = ARCCtrl->DstRect.height;

    // To optimize memory consumption, dst format is always STM_BLITTER_SF_NV12
    // (and not STM_BLITTER_SF_UYVY that is also managed by HVA as input color format)
    dst_bitmap.format                     = STM_BLITTER_SF_NV12;
    dst_bitmap.aligned_size.w             = dst_bitmap.size.w;
    dst_bitmap.aligned_size.h             = dst_bitmap.size.h;
    dst_bitmap.pitch                      = dst_bitmap.aligned_size.w;
    dst_bitmap.buffer_address.cbcr_offset = dst_bitmap.aligned_size.w * dst_bitmap.aligned_size.h;
    dst_bitmap.buffer_size                = dst_bitmap.aligned_size.w * dst_bitmap.aligned_size.h * 3 / 2;

    dst = stm_blitter_surface_new_preallocated(dst_bitmap.format, dst_bitmap.colorspace, &dst_bitmap.buffer_address,
                                               dst_bitmap.buffer_size, &dst_bitmap.size, dst_bitmap.pitch);
    if (BLITTER_ERR(dst))
    {
        SE_ERROR("Failed to allocate dst surface (%d)\n", BLITTER_ERR(dst));
        return PreprocError;
    }

    return PreprocNoError;
}

PreprocStatus_t Preproc_Video_Blitter_c::FinalizeInit(void)
{
    // Get a blitter handle
    blitter = stm_blitter_get(BLITTER_ID);
    if (BLITTER_ERR(blitter))
    {
        blitter = NULL;
        SE_ERROR("Failed to get blitter device %d (%d)\n", BLITTER_ID, BLITTER_ERR(blitter));
        return PreprocError;
    }

    return PreprocNoError;
}

PreprocStatus_t Preproc_Video_Blitter_c::Halt()
{
    // Put a blitter handle
    if (blitter != NULL)
    {
        stm_blitter_put(blitter);
        blitter = NULL;
    }

    // Call the Base Class implementation
    return Preproc_Video_c::Halt();
}

void Preproc_Video_Blitter_c::AdjustBorderRectToHWConstraints(stm_blitter_dimension_t *buffer_dim, stm_blitter_rect_t *black_rect)
{
    long delta;

    if (black_rect->size.w < MIN_BLITTER_RECT_WIDTH)
    {
        // Get delta size to increase rectangle width for reaching minimum value
        delta = MIN_BLITTER_RECT_WIDTH - black_rect->size.w;
        // Expand width rectangle to its minimum value
        black_rect->size.w += delta;

        if (black_rect->position.x + black_rect->size.w > buffer_dim->w)
        {
            // width rectangle is outside the buffer area so set it back inside
            black_rect->position.x -= delta;
        }
    }

    if (black_rect->size.h < MIN_BLITTER_RECT_HEIGHT)
    {
        // Get delta size to increase rectangle height for reaching minimum value
        delta = MIN_BLITTER_RECT_HEIGHT - black_rect->size.h;
        // Expand height rectangle to its minimum value
        black_rect->size.h += delta;

        if (black_rect->position.y + black_rect->size.h > buffer_dim->h)
        {
            // height rectangle is outside the buffer area so set it back inside
            black_rect->position.y -= delta;
        }
    }
}

PreprocStatus_t Preproc_Video_Blitter_c::FillPictureBorder()
{
    int blitStatus;
    stm_blitter_color_t      black_color;
    stm_blitter_rect_t       black_rect;
    stm_blitter_rect_t       picture_area = dst_bitmap.rect;
    stm_blitter_dimension_t  buffer_dim   = dst_bitmap.aligned_size;

    // Destination surface is always YUV
    blitStatus = stm_blitter_surface_set_color_colorspace(dst, STM_BLITTER_COLOR_YUV);
    if (blitStatus < 0)
    {
        SE_ERROR("Failed to set color space (%d)\n", blitStatus);
        return PreprocError;
    }

    blitStatus = stm_blitter_surface_set_blitflags(dst, STM_BLITTER_SBF_NONE);
    if (blitStatus < 0)
    {
        SE_ERROR("Failed to set blit flags (%d)\n", blitStatus);
        return PreprocError;
    }


    black_color.a  = 0x80;
    black_color.y  = 0x00;
    black_color.cr = 0x80;
    black_color.cb = 0x80;

    // Set black color for drawing
    blitStatus = stm_blitter_surface_set_color(dst, &black_color);
    if (blitStatus < 0)
    {
        SE_ERROR("Failed to set color (%d)\n", blitStatus);
        return PreprocError;
    }

    // Fill picture border rectangles: top/left/right/bottom.
    // There are at least 4 possible rectangles to fill with black color around the picture
    // in the destination buffer.
    // ----------------------------------
    // |                                |
    // |           top_rect             |
    // |                                |
    // |--------------------------------|
    // | left_ |               | right_ |
    // | rect  | PICTURE AREA  | rect   |
    // |       |               |        |
    // |--------------------------------|
    // |                                |
    // |          bottom_rect           |
    // |                                |
    // |--------------------------------|
    // picture area is already truncated to buffer dimension
    // due to previous generic treatments

    if (picture_area.position.y > 0)
    {
        // Fill top rectangle coordinate
        black_rect.position.x = 0;
        black_rect.position.y = 0;
        black_rect.size.w     = buffer_dim.w;
        black_rect.size.h     = picture_area.position.y;

        AdjustBorderRectToHWConstraints(&buffer_dim, &black_rect);

        SE_EXTRAVERB(group_encoder_video_preproc, "TOP BORDER %ld %ld %ld %ld\n",
                     black_rect.position.x,
                     black_rect.position.y,
                     black_rect.size.w,
                     black_rect.size.h);

        blitStatus = stm_blitter_surface_fill_rect(blitter, dst, &black_rect);
        if (blitStatus < 0)
        {
            SE_ERROR("Failed to fill top black border (%d)\n", blitStatus);
            return PreprocError;
        }
    }

    if (picture_area.position.x > 0)
    {
        // Fill left rectangle coordinate
        black_rect.position.x = 0;
        black_rect.position.y = picture_area.position.y;
        black_rect.size.w     = picture_area.position.x;
        black_rect.size.h     = picture_area.size.h;

        AdjustBorderRectToHWConstraints(&buffer_dim, &black_rect);

        SE_EXTRAVERB(group_encoder_video_preproc, "LEFT BORDER %ld %ld %ld %ld\n",
                     black_rect.position.x,
                     black_rect.position.y,
                     black_rect.size.w,
                     black_rect.size.h);

        blitStatus = stm_blitter_surface_fill_rect(blitter, dst, &black_rect);
        if (blitStatus < 0)
        {
            SE_ERROR("Failed to fill left black border (%d)\n", blitStatus);
            return PreprocError;
        }
    }

    if ((picture_area.position.x + picture_area.size.w) < buffer_dim.w)
    {
        // Fill right rectangle coordinate
        black_rect.position.x = picture_area.position.x + picture_area.size.w;
        black_rect.position.y = picture_area.position.y;
        black_rect.size.w     = buffer_dim.w - (picture_area.position.x +
                                                picture_area.size.w);
        black_rect.size.h     = picture_area.size.h;

        AdjustBorderRectToHWConstraints(&buffer_dim, &black_rect);

        // Blitter workaround
        // It seems that blitter is not filling the last right column of picture area when both picture area x and picture area width are odd.
        if ((picture_area.position.x & 0x1) && (picture_area.size.w & 0x1)
            && (black_rect.position.x > 0))
        {
            // So expand right rectangle on the last right column of picture area
            black_rect.position.x -= 1;
            black_rect.size.w     += 1;
        }

        SE_EXTRAVERB(group_encoder_video_preproc, "RIGHT BORDER %ld %ld %ld %ld\n",
                     black_rect.position.x,
                     black_rect.position.y,
                     black_rect.size.w,
                     black_rect.size.h);

        blitStatus = stm_blitter_surface_fill_rect(blitter, dst, &black_rect);
        if (blitStatus < 0)
        {
            SE_ERROR("Failed to fill right black border (%d)\n", blitStatus);
            return PreprocError;
        }
    }

    if ((picture_area.position.y + picture_area.size.h) < buffer_dim.h)
    {
        // Fill bottom rectangle coordinate
        black_rect.position.x = 0;
        black_rect.position.y = picture_area.position.y + picture_area.size.h;
        black_rect.size.w     = buffer_dim.w;
        black_rect.size.h     = buffer_dim.h - (picture_area.position.y +
                                                picture_area.size.h);

        AdjustBorderRectToHWConstraints(&buffer_dim, &black_rect);

        // Blitter workaround
        // It seems that blitter is not filling the last bottom line of picture area when both picture area y and picture area height are odd.
        if ((picture_area.position.y & 0x1) && (picture_area.size.h & 0x1)
            && (black_rect.position.y > 0))
        {
            // So expand bottom rectangle on the last bottom line of picture area
            black_rect.position.y -= 1;
            black_rect.size.h     += 1;
        }

        SE_EXTRAVERB(group_encoder_video_preproc, "BOTTOM BORDER %ld %ld %ld %ld\n",
                     black_rect.position.x,
                     black_rect.position.y,
                     black_rect.size.w,
                     black_rect.size.h);

        blitStatus = stm_blitter_surface_fill_rect(blitter, dst, &black_rect);
        if (blitStatus < 0)
        {
            SE_ERROR("Failed to fill bottom black border (%d)\n", blitStatus);
            return PreprocError;
        }
    }

    // Alternative method is to use stm_blitter_surface_clear
    // It may consume higher bandwidth compared to drawing rectangles
    return PreprocNoError;
}

void Preproc_Video_Blitter_c::DumpBlitterTaskDescriptor(bitmap_t *Input, bitmap_t *Output)
{
    if (Input && Output && SE_IS_VERBOSE_ON(group_encoder_video_preproc))
    {
        SE_VERBOSE(group_encoder_video_preproc, "[BLITTER TASK DESCRIPTOR]\n");
        SE_VERBOSE(group_encoder_video_preproc, "|-[input]\n");
        SE_VERBOSE(group_encoder_video_preproc, "| |-format = 0x%x (%s)\n", Input->format, StringifyBlitterSurfaceFormat(Input->format));
        SE_VERBOSE(group_encoder_video_preproc, "| |-colorspace = %d (%s)\n", Input->colorspace, StringifyBlitterColorspace(Input->colorspace));
        SE_VERBOSE(group_encoder_video_preproc, "| |-[buffer_address]\n");
        SE_VERBOSE(group_encoder_video_preproc, "| | |-base = 0x%lx\n", Input->buffer_address.base);
        SE_VERBOSE(group_encoder_video_preproc, "| | |-cb_offset = %ld bytes\n", Input->buffer_address.cb_offset);
        SE_VERBOSE(group_encoder_video_preproc, "| | |-cr_offset = %ld bytes\n", Input->buffer_address.cr_offset);
        SE_VERBOSE(group_encoder_video_preproc, "| |\n");
        SE_VERBOSE(group_encoder_video_preproc, "| |-buffer_size = %lu bytes\n", Input->buffer_size);
        SE_VERBOSE(group_encoder_video_preproc, "| |-size.w = %ld pixels\n", Input->size.w);
        SE_VERBOSE(group_encoder_video_preproc, "| |-size.h = %ld pixels\n", Input->size.h);
        SE_VERBOSE(group_encoder_video_preproc, "| |-aligned_size.w = %ld pixels\n", Input->aligned_size.w);
        SE_VERBOSE(group_encoder_video_preproc, "| |-aligned_size.h = %ld pixels\n", Input->aligned_size.h);
        SE_VERBOSE(group_encoder_video_preproc, "| |-pitch = %lu bytes\n", Input->pitch);
        SE_VERBOSE(group_encoder_video_preproc, "| |-[rect]\n");
        SE_VERBOSE(group_encoder_video_preproc, "|   |-position.x = %ld pixels\n", Input->rect.position.x);
        SE_VERBOSE(group_encoder_video_preproc, "|   |-position.y = %ld pixels\n", Input->rect.position.y);
        SE_VERBOSE(group_encoder_video_preproc, "|   |-size.w = %ld pixels\n", Input->rect.size.w);
        SE_VERBOSE(group_encoder_video_preproc, "|   |-size.h = %ld pixels\n", Input->rect.size.h);
        SE_VERBOSE(group_encoder_video_preproc, "|\n");
        SE_VERBOSE(group_encoder_video_preproc, "|-[output]\n");
        SE_VERBOSE(group_encoder_video_preproc, "  |-format = 0x%x (%s)\n", Output->format, StringifyBlitterSurfaceFormat(Output->format));
        SE_VERBOSE(group_encoder_video_preproc, "  |-colorspace = %d (%s)\n", Output->colorspace, StringifyBlitterColorspace(Output->colorspace));
        SE_VERBOSE(group_encoder_video_preproc, "  |-[buffer_address]\n");
        SE_VERBOSE(group_encoder_video_preproc, "  | |-base = 0x%lx\n", Output->buffer_address.base);
        SE_VERBOSE(group_encoder_video_preproc, "  | |-cb_offset = %ld bytes\n", Output->buffer_address.cb_offset);
        SE_VERBOSE(group_encoder_video_preproc, "  | |-cr_offset = %ld bytes\n", Output->buffer_address.cr_offset);
        SE_VERBOSE(group_encoder_video_preproc, "  |\n");
        SE_VERBOSE(group_encoder_video_preproc, "  |-buffer_size = %lu bytes\n", Output->buffer_size);
        SE_VERBOSE(group_encoder_video_preproc, "  |-size.w = %ld pixels\n", Output->size.w);
        SE_VERBOSE(group_encoder_video_preproc, "  |-size.h = %ld pixels\n", Output->size.h);
        SE_VERBOSE(group_encoder_video_preproc, "  |-aligned_size.w = %ld pixels\n", Output->aligned_size.w);
        SE_VERBOSE(group_encoder_video_preproc, "  |-aligned_size.h = %ld pixels\n", Output->aligned_size.h);
        SE_VERBOSE(group_encoder_video_preproc, "  |-pitch = %lu bytes\n", Output->pitch);
        SE_VERBOSE(group_encoder_video_preproc, "  |-[rect]\n");
        SE_VERBOSE(group_encoder_video_preproc, "    |-position.x = %ld pixels\n", Output->rect.position.x);
        SE_VERBOSE(group_encoder_video_preproc, "    |-position.y = %ld pixels\n", Output->rect.position.y);
        SE_VERBOSE(group_encoder_video_preproc, "    |-size.w = %ld pixels\n", Output->rect.size.w);
        SE_VERBOSE(group_encoder_video_preproc, "    |-size.h = %ld pixels\n", Output->rect.size.h);
        SE_VERBOSE(group_encoder_video_preproc, "\n");
    }
}

PreprocStatus_t Preproc_Video_Blitter_c::BlitSurface(Buffer_t InputBuffer, Buffer_t OutputBuffer, uint32_t BufferFlag)
{
    unsigned int DataSize;
    unsigned int ShrinkSize;
    void *InputBufferAddrPhys;
    void *PreprocBufferAddrPhys;
    stm_blitter_serial_t serial;
    stm_blitter_surface_blitflags_t BlitFlags;
    int blitStatus;
    Buffer_t ContentBuffer;
    ARCCtrl_t ARCCtrl;

    // Copy data buffer
    InputBuffer->ObtainDataReference(NULL, &DataSize, (void **)(&InputBufferAddrPhys), PhysicalAddress);
    if (InputBufferAddrPhys == NULL)
    {
        SE_ERROR("Failed to get input data buffer reference\n");
        return PreprocError;
    }

    OutputBuffer->ObtainDataReference(NULL, NULL, (void **)(&PreprocBufferAddrPhys), PhysicalAddress);
    if (PreprocBufferAddrPhys == NULL)
    {
        SE_ERROR("Failed to get preproc data buffer reference\n");
        return PreprocError;
    }

    PreprocFrameBufferIntermediate = NULL;
    // Convert aspect ratio
    ConvertAspectRatio(&ARCCtrl);

    // Prepare blitting surface for source
    PreprocStatus_t Status = PrepareSrcBlitSurface(InputBufferAddrPhys, BufferFlag);
    if (Status != PreprocNoError)
    {
        PREPROC_ERROR_RUNNING("Failed to prepare src blit surface\n");
        return Status;
    }

    // Prepare blitting surface for destination
    Status = PrepareDstBlitSurface(PreprocBufferAddrPhys, &ARCCtrl);
    if (Status != PreprocNoError)
    {
        PREPROC_ERROR_RUNNING("Failed to prepare dest blit surface\n");
        return Status;
    }

    DumpBlitterTaskDescriptor(&src_bitmap, &dst_bitmap);

    // Fill picture borders
    Status = FillPictureBorder();
    if (Status != PreprocNoError)
    {
        SE_ERROR("Failed to fill picture border\n");
        stm_blitter_surface_put(src);
        stm_blitter_surface_put(dst);
        return PreprocError;
    }

    // Blit surface using fence mode
    BlitFlags = STM_BLITTER_SBF_NONE;

    if (BufferFlag & BLITTER_BUFFER_INTERLACED)
    {
        if (BufferFlag & BLITTER_BUFFER_TOP)
        {
            BlitFlags = STM_BLITTER_SBF_DEINTERLACE_TOP;
        }
        else if (BufferFlag & BLITTER_BUFFER_BOTTOM)
        {
            BlitFlags = STM_BLITTER_SBF_DEINTERLACE_BOTTOM;
        }
    }

    blitStatus = stm_blitter_surface_set_blitflags(dst, BlitFlags);
    if (blitStatus < 0)
    {
        SE_ERROR("Failed to set blit flags (%d)\n", blitStatus);
        stm_blitter_surface_put(src);
        stm_blitter_surface_put(dst);
        return PreprocError;
    }

    blitStatus = stm_blitter_surface_add_fence(dst);
    if (blitStatus < 0)
    {
        SE_ERROR("Failed to add fence (%d)\n", blitStatus);
        stm_blitter_surface_put(src);
        stm_blitter_surface_put(dst);
        return PreprocError;
    }

    // If deinterlacing (BufferFlag != 0) or resize is required, stretch blit is used.
    if (src_bitmap.rect.size.w == dst_bitmap.rect.size.w && src_bitmap.rect.size.h == dst_bitmap.rect.size.h && BufferFlag == 0)
    {
        blitStatus = stm_blitter_surface_blit(blitter, src, &src_bitmap.rect, dst, &dst_bitmap.rect.position, 1);
        if (blitStatus < 0)
        {
            SE_ERROR("Failed to blit surface (%d)\n", blitStatus);
            stm_blitter_surface_put(src);
            stm_blitter_surface_put(dst);
            return PreprocError;
        }
    }
    else
    {
        blitStatus = stm_blitter_surface_stretchblit(blitter, src, &src_bitmap.rect, dst, &dst_bitmap.rect, 1);
        if (blitStatus < 0)
        {
            SE_ERROR("Failed to blit surface stretch (%d)\n", blitStatus);
            stm_blitter_surface_put(src);
            stm_blitter_surface_put(dst);
            return PreprocError;
        }
    }

    // Update meta data
    PreprocMetaDataDescriptor->video.video_parameters.width                          = PreprocCtrl.Resolution.width;
    PreprocMetaDataDescriptor->video.video_parameters.height                         = PreprocCtrl.Resolution.height;

    if (dst_bitmap.format == STM_BLITTER_SF_UYVY)
    {
        PreprocMetaDataDescriptor->video.surface_format                              = SURFACE_FORMAT_VIDEO_422_RASTER;
        PreprocMetaDataDescriptor->video.pitch                                       = PreprocCtrl.Resolution.width * 2;
    }
    else if (dst_bitmap.format == STM_BLITTER_SF_NV12)
    {
        PreprocMetaDataDescriptor->video.surface_format                              = SURFACE_FORMAT_VIDEO_420_RASTER2B;
        PreprocMetaDataDescriptor->video.pitch                                       = PreprocCtrl.Resolution.width;
    }

    PreprocMetaDataDescriptor->video.frame_rate                                      = PreprocCtrl.Framerate;

    // As coder only needs fractional framerate, integer framerate is set to 0
    PreprocMetaDataDescriptor->video.video_parameters.frame_rate                     = 0;

    // As coder only needs pixel aspect ratio, display aspect ratio is set to unspecified
    PreprocMetaDataDescriptor->video.video_parameters.aspect_ratio                   = STM_SE_ASPECT_RATIO_UNSPECIFIED;
    PreprocMetaDataDescriptor->video.video_parameters.pixel_aspect_ratio_numerator   = ARCCtrl.DstPixelAspectRatio.num;
    PreprocMetaDataDescriptor->video.video_parameters.pixel_aspect_ratio_denominator = ARCCtrl.DstPixelAspectRatio.den;

    PreprocMetaDataDescriptor->video.video_parameters.scan_type                      = STM_SE_SCAN_TYPE_PROGRESSIVE;
    PreprocMetaDataDescriptor->video.vertical_alignment                              = 1;

    blitStatus = stm_blitter_get_serial(blitter, &serial);

    if (blitStatus < 0)
    {
        SE_ERROR("Failed to get serial blit (%d)\n", blitStatus);
        stm_blitter_surface_put(src);
        stm_blitter_surface_put(dst);
        return PreprocError;
    }

    blitStatus = stm_blitter_wait(blitter, STM_BLITTER_WAIT_FENCE, serial);
    if (blitStatus < 0)
    {
        SE_ERROR("Failed to wait for serial blit (%d)\n", blitStatus);
        stm_blitter_surface_put(src);
        stm_blitter_surface_put(dst);
        return PreprocError;
    }

    // Remove blitting surfaces
    stm_blitter_surface_put(src);
    stm_blitter_surface_put(dst);
    src = dst = NULL;

    // Clear the intermediate memory buffer due to format conversion.
    if (PreprocFrameBufferIntermediate != NULL)
    {
        PreprocFrameBufferIntermediate->DecrementReferenceCount(IdentifierEncoderPreprocessor);
        PreprocFrameBufferIntermediate = NULL;
    }

    // Completion
    DataSize = dst_bitmap.buffer_size;
    OutputBuffer->SetUsedDataSize(DataSize);

    OutputBuffer->ObtainAttachedBufferReference(PreprocFrameAllocType, &ContentBuffer);
    SE_ASSERT(ContentBuffer != NULL);

    ContentBuffer->SetUsedDataSize(DataSize);
    ShrinkSize = max(DataSize, 1);
    BufferStatus_t BufStatus = ContentBuffer->ShrinkBuffer(ShrinkSize);
    if (BufStatus != BufferNoError)
    {
        SE_ERROR("Failed to shrink the operating buffer to size %d\n", ShrinkSize);
        return PreprocError;
    }

    return PreprocNoError;
}

PreprocStatus_t Preproc_Video_Blitter_c::RepeatOutputFrame(Buffer_t InputBuffer, Buffer_t OutputBuffer, FRCCtrl_t *FRCCtrl)
{
    PreprocStatus_t Status;
    int32_t cnt;

    // Get ref preproc buffer metadata descriptor
    __stm_se_frame_metadata_t            *RefPreprocFullMetaDataDescriptor;
    OutputBuffer->ObtainMetaDataReference(OutputMetaDataBufferType, (void **)(&RefPreprocFullMetaDataDescriptor));
    SE_ASSERT(RefPreprocFullMetaDataDescriptor != NULL);

    for (cnt = 0; cnt < FRCCtrl->FrameRepeat; cnt ++)
    {
        // Get preproc buffer clone
        Buffer_t PreprocFrameBufferClone;
        Status = Preproc_Base_c::GetBufferClone(OutputBuffer, &PreprocFrameBufferClone);
        if (Status != PreprocNoError)
        {
            PREPROC_ERROR_RUNNING("Failed to clone output buffer\n");
            return Status;
        }

        SE_DEBUG(group_encoder_video_preproc, "Copy buffer %p to cloned buffer %p\n", OutputBuffer, PreprocFrameBufferClone);

        // Attach the Input buffer to the BufferClone so that it is kept until Preprocessing is complete.
        BufferStatus_t BufStatus = PreprocFrameBufferClone->AttachBuffer(InputBuffer);
        if (BufStatus != BufferNoError)
        {
            SE_ERROR("Failed to attach input buffer\n");
            PreprocFrameBufferClone->DecrementReferenceCount(IdentifierEncoderPreprocessor);
            return PreprocError;
        }

        // Obtain preproc metadata reference
        __stm_se_frame_metadata_t  *ClonePreprocFullMetaDataDescriptor;
        PreprocFrameBufferClone->ObtainMetaDataReference(OutputMetaDataBufferType, (void **)(&ClonePreprocFullMetaDataDescriptor));
        SE_ASSERT(ClonePreprocFullMetaDataDescriptor != NULL);

        // Copy metadata
        stm_se_uncompressed_frame_metadata_t *ClonePreprocMetaDataDescriptor;
        ClonePreprocMetaDataDescriptor = &ClonePreprocFullMetaDataDescriptor->uncompressed_frame_metadata;
        memcpy(ClonePreprocFullMetaDataDescriptor, RefPreprocFullMetaDataDescriptor, sizeof(__stm_se_frame_metadata_t));

        // Update PTS info
        // We assume encoded_time is always expressed in the same unit than native_time, as set by the EncodeCoordinator
        ClonePreprocMetaDataDescriptor->native_time = FRCCtrl->NativeTimeOutput + (cnt * FRCCtrl->TimePerOutputFrame);
        ClonePreprocFullMetaDataDescriptor->encode_coordinator_metadata.encoded_time = FRCCtrl->EncodedTimeOutput + (cnt * FRCCtrl->TimePerOutputFrame);

        SE_DEBUG(group_encoder_video_preproc, "PTS: In %llu Out %llu\n",
                 InputMetaDataDescriptor->native_time, ClonePreprocMetaDataDescriptor->native_time);

        // Output frame
        Status = Output(PreprocFrameBufferClone);
        if (Status != PreprocNoError)
        {
            SE_ERROR("Failed to output frame\n");
            return PreprocError;
        }
    }

    return PreprocNoError;
}

PreprocStatus_t Preproc_Video_Blitter_c::ProcessField(Buffer_t InputBuffer, Buffer_t OutputBuffer, bool SecondField)
{
    PreprocStatus_t Status;
    BufferStatus_t BufStatus;
    uint32_t BufferFlag;
    FRCCtrl_t FRCCtrl;
    // Convert frame rate
    FRCCtrl.SecondField = SecondField;
    ConvertFrameRate(&FRCCtrl);

    // Check if input frame is processed
    if (!FRCCtrl.DiscardInput)
    {
        // Initialize buffer and metadata if second field because it is not done by default in base classes
        if (SecondField)
        {
            // Get new preproc buffer
            Status = Preproc_Base_c::GetNewBuffer(&OutputBuffer);
            if (Status != PreprocNoError)
            {
                return Status;
            }

            // Obtain preproc metadata reference
            OutputBuffer->ObtainMetaDataReference(OutputMetaDataBufferType, (void **)(&PreprocFullMetaDataDescriptor));
            SE_ASSERT(PreprocFullMetaDataDescriptor != NULL);

            // copy metadata
            PreprocMetaDataDescriptor = &PreprocFullMetaDataDescriptor->uncompressed_frame_metadata;
            memcpy(PreprocMetaDataDescriptor, InputMetaDataDescriptor, sizeof(stm_se_uncompressed_frame_metadata_t));
            memcpy(&PreprocFullMetaDataDescriptor->encode_coordinator_metadata, EncodeCoordinatorMetaDataDescriptor, sizeof(__stm_se_encode_coordinator_metadata_t));

            // Attach the input buffer to the output buffer so that it is kept until preprocessing is complete.
            BufStatus = OutputBuffer->AttachBuffer(InputBuffer);
            if (BufStatus != BufferNoError)
            {
                SE_ERROR("Failed to attach input buffer\n");
                OutputBuffer->DecrementReferenceCount(IdentifierEncoderPreprocessor);
                return PreprocError;
            }
        }

        // Update preproc control status
        PreprocFullMetaDataDescriptor->encode_process_metadata.video.deinterlacing_on   = (PreprocCtrl.EnableDeInterlacer != STM_SE_CTRL_VALUE_DISAPPLY) ? true : false;
        PreprocFullMetaDataDescriptor->encode_process_metadata.video.noise_filtering_on = (PreprocCtrl.EnableNoiseFilter != STM_SE_CTRL_VALUE_DISAPPLY) ? true : false;
        // Program buffer flag
        BufferFlag = 0;

        if ((PreprocCtrl.EnableDeInterlacer != STM_SE_CTRL_VALUE_DISAPPLY) && (PreprocMetaDataDescriptor->video.video_parameters.scan_type == STM_SE_SCAN_TYPE_INTERLACED))
        {
            BufferFlag  = (uint32_t)(BLITTER_BUFFER_INTERLACED);

            // Top field when top field first && first field or bot field first && second field
            if (!(PreprocMetaDataDescriptor->video.top_field_first == SecondField))
            {
                BufferFlag |= (uint32_t)(BLITTER_BUFFER_TOP);
            }
            else
            {
                BufferFlag |= (uint32_t)(BLITTER_BUFFER_BOTTOM);
            }
        }

        // Blit surface and update preproc output metadata
        Status = BlitSurface(InputBuffer, OutputBuffer, BufferFlag);
        if (Status != PreprocNoError)
        {
            PREPROC_ERROR_RUNNING("Failed to blit surface\n");
            OutputBuffer->DecrementReferenceCount(IdentifierEncoderPreprocessor);
            return Status;
        }

        // Check if output frame is output
        if (!FRCCtrl.DiscardOutput)
        {
            SE_DEBUG(group_encoder_video_preproc, "Output buffer %p", OutputBuffer);

            // Repeat output frame
            if (FRCCtrl.FrameRepeat > 0)
            {
                Status = RepeatOutputFrame(InputBuffer, OutputBuffer, &FRCCtrl);
                if (Status != PreprocNoError)
                {
                    PREPROC_ERROR_RUNNING("Failed to repeat output frame\n");
                    OutputBuffer->DecrementReferenceCount(IdentifierEncoderPreprocessor);
                    return PreprocError;
                }
            }

            // Update PTS info
            // We assume encoded_time is always expressed in the same unit than native_time, as set by the EncodeCoordinator
            PreprocMetaDataDescriptor->native_time = FRCCtrl.NativeTimeOutput + (FRCCtrl.FrameRepeat * FRCCtrl.TimePerOutputFrame);
            PreprocFullMetaDataDescriptor->encode_coordinator_metadata.encoded_time = FRCCtrl.EncodedTimeOutput + (FRCCtrl.FrameRepeat * FRCCtrl.TimePerOutputFrame);

            SE_DEBUG(group_encoder_video_preproc, "PTS: In %llu Out %llu\n", InputMetaDataDescriptor->native_time, PreprocMetaDataDescriptor->native_time);

            // Output frame
            Status = Output(OutputBuffer);
            if (Status != PreprocNoError)
            {
                SE_ERROR("Failed to output frame\n");
                return PreprocError;
            }
        }
        else
        {
            // Silently discard frame
            OutputBuffer->DecrementReferenceCount(IdentifierEncoderPreprocessor);
            return PreprocNoError;
        }
    }
    else
    {
        if (!SecondField)
        {
            // Silently discard frame
            OutputBuffer->DecrementReferenceCount(IdentifierEncoderPreprocessor);
        }
    }

    return PreprocNoError;
}

PreprocStatus_t Preproc_Video_Blitter_c::Input(Buffer_t   Buffer)
{
    PreprocStatus_t        Status;
    SE_DEBUG(group_encoder_video_preproc, "\n");

    // Dump preproc input buffer via st_relay
    Buffer->DumpViaRelay(ST_RELAY_TYPE_ENCODER_VIDEO_PREPROC_INPUT + mRelayfsIndex, ST_RELAY_SOURCE_SE);

    // Call the Base Class implementation to update statistics
    Status = Preproc_Video_c::Input(Buffer);
    if (Status != PreprocNoError)
    {
        PREPROC_ERROR_RUNNING("Failed to call parent class, Status = %u\n", Status);
        return PREPROC_STATUS_RUNNING(Status);
    }

    EntryTrace(InputMetaDataDescriptor, &PreprocCtrl);

    // Check for valid blitter device
    if (blitter == NULL)
    {
        PREPROC_ERROR_RUNNING("Blitter device is not valid\n");
        PreprocFrameBuffer->DecrementReferenceCount(IdentifierEncoderPreprocessor);
        return PREPROC_STATUS_RUNNING(PreprocError);
    }

    // Generate buffer discontinuity if needed
    if (PreprocDiscontinuity &&
        !((PreprocDiscontinuity & STM_SE_DISCONTINUITY_EOS) && (InputBufferSize != 0)))
    {
        // Insert buffer discontinuity
        Status = GenerateBufferDiscontinuity(PreprocDiscontinuity);
        if (Status != PreprocNoError)
        {
            PREPROC_ERROR_RUNNING("Failed to insert discontinuity, Status = %u\n", Status);
            PreprocFrameBuffer->DecrementReferenceCount(IdentifierEncoderPreprocessor);
            return PREPROC_STATUS_RUNNING(Status);
        }

        // Silently discard frame for null buffer input
        if (InputBufferSize == 0)
        {
            if (PreprocDiscontinuity & STM_SE_DISCONTINUITY_EOS)
            {
                ExitTrace(InputMetaDataDescriptor, &PreprocCtrl);
            }
            PreprocFrameBuffer->DecrementReferenceCount(IdentifierEncoderPreprocessor);
            return PreprocNoError;
        }
    }

    // Check for unsupported deinterlacing formats
    if ((PreprocCtrl.EnableDeInterlacer != STM_SE_CTRL_VALUE_DISAPPLY) && (PreprocMetaDataDescriptor->video.video_parameters.scan_type == STM_SE_SCAN_TYPE_INTERLACED))
    {
        if ((PreprocMetaDataDescriptor->video.surface_format == SURFACE_FORMAT_VIDEO_420_PAIRED_MACROBLOCK) ||
            (PreprocMetaDataDescriptor->video.surface_format == SURFACE_FORMAT_VIDEO_420_MACROBLOCK))
        {
            SE_ERROR("Deinterlacing not supported for format %x\n", PreprocMetaDataDescriptor->video.surface_format);
            PreprocFrameBuffer->DecrementReferenceCount(IdentifierEncoderPreprocessor);
            return PreprocError;
        }
    }

    // First field/frame is processed
    SE_DEBUG(group_encoder_video_preproc, "Processing first field/frame\n");
    Status = ProcessField(Buffer, PreprocFrameBuffer, false);
    if (Status != PreprocNoError)
    {
        PREPROC_ERROR_RUNNING("Failed to process first field, Status = %u\n", Status);
        return PREPROC_STATUS_RUNNING(Status);
    }

    // If second field is processed
    if ((PreprocCtrl.EnableDeInterlacer != STM_SE_CTRL_VALUE_DISAPPLY) && (InputMetaDataDescriptor->video.video_parameters.scan_type == STM_SE_SCAN_TYPE_INTERLACED))
    {
        SE_DEBUG(group_encoder_video_preproc, "Processing second field\n");
        Status = ProcessField(Buffer, PreprocFrameBuffer2, true);
        if (Status != PreprocNoError)
        {
            PREPROC_ERROR_RUNNING("Failed to process second field, Status = %u\n", Status);
            return PREPROC_STATUS_RUNNING(Status);
        }
    }

    // Special case to ensure that valid buffer is processed before EOS buffer is generated
    if ((PreprocDiscontinuity & STM_SE_DISCONTINUITY_EOS) && (InputBufferSize != 0))
    {
        // Insert buffer EOS
        Status = GenerateBufferDiscontinuity(PreprocDiscontinuity);
        if (Status != PreprocNoError)
        {
            PREPROC_ERROR_RUNNING("Failed to insert EOS discontinuity, Status = %u\n", Status);
            return PREPROC_STATUS_RUNNING(Status);
        }
        ExitTrace(InputMetaDataDescriptor, &PreprocCtrl);
    }

    return PreprocNoError;
}

PreprocStatus_t Preproc_Video_Blitter_c::GetControl(stm_se_ctrl_t  Control,
                                                    void          *Data)
{
    return Preproc_Video_c::GetControl(Control, Data);
}

PreprocStatus_t Preproc_Video_Blitter_c::GetCompoundControl(stm_se_ctrl_t  Control,
                                                            void          *Data)
{
    return Preproc_Video_c::GetCompoundControl(Control, Data);
}

PreprocStatus_t Preproc_Video_Blitter_c::SetControl(stm_se_ctrl_t  Control,
                                                    const void    *Data)
{
    uint32_t    *EnableNoiseFilter;

    switch (Control)
    {
    case STM_SE_CTRL_VIDEO_ENCODE_STREAM_NOISE_FILTERING:
        EnableNoiseFilter = (uint32_t *)Data;

        if ((*EnableNoiseFilter) != STM_SE_CTRL_VALUE_DISAPPLY)
        {
            SE_ERROR("Control VIDEO_ENCODE_STREAM_NOISE_FILTERING not supported by blitter\n");
            return PreprocUnsupportedControl;
        }

        break;

    default:
        // Cannot trace this as error because it can be a valid control for another object
        SE_DEBUG(group_encoder_video_preproc, "Not match preproc video blitter control %u\n", Control);
        break;
    }

    return Preproc_Video_c::SetControl(Control, Data);
}

PreprocStatus_t Preproc_Video_Blitter_c::SetCompoundControl(stm_se_ctrl_t  Control,
                                                            const void    *Data)
{
    switch (Control)
    {
    default:
        // Cannot trace this as error because it can be a valid control for another object
        SE_DEBUG(group_encoder_video_preproc, "Not match preproc video blitter control %u\n", Control);
        break;
    }

    return Preproc_Video_c::SetCompoundControl(Control, Data);
}

PreprocStatus_t Preproc_Video_Blitter_c::InjectDiscontinuity(stm_se_discontinuity_t  Discontinuity)
{
    PreprocStatus_t Status;
    SE_DEBUG(group_encoder_video_preproc, "\n");

    Status = Preproc_Base_c::InjectDiscontinuity(Discontinuity);
    if (Status != PreprocNoError)
    {
        PREPROC_ERROR_RUNNING("Failed to inject discontinuity\n");
        return PREPROC_STATUS_RUNNING(Status);
    }

    if (Discontinuity)
    {
        // Generate Discontinuity Buffer
        Status = GenerateBufferDiscontinuity(Discontinuity);
        if (Status != PreprocNoError)
        {
            PREPROC_ERROR_RUNNING("Failed to insert discontinuity\n");
            return PREPROC_STATUS_RUNNING(Status);
        }
    }

    ExitTrace(InputMetaDataDescriptor, &PreprocCtrl);

    return PreprocNoError;
}

PreprocVideoCaps_t Preproc_Video_Blitter_c::GetCapabilities()
{
    return PreprocCaps;
}
