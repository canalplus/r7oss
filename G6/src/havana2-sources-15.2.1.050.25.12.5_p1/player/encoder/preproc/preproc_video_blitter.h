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
#ifndef PREPROC_VIDEO_BLITTER_H
#define PREPROC_VIDEO_BLITTER_H

#include "preproc_video.h"

extern "C" {
#include <blitter.h>
};

#define BLITTER_ID 0
#define SD_WIDTH   720

typedef struct bitmap_s
{
    stm_blitter_surface_format_t     format;
    stm_blitter_surface_colorspace_t colorspace;
    stm_blitter_surface_address_t    buffer_address;
    unsigned long                    buffer_size;
    stm_blitter_dimension_t          size;
    stm_blitter_dimension_t          aligned_size;
    unsigned long                    pitch;
    stm_blitter_rect_t               rect;
} bitmap_t;

class Preproc_Video_Blitter_c : public Preproc_Video_c
{
public:
    Preproc_Video_Blitter_c(void);
    ~Preproc_Video_Blitter_c(void);

    PreprocStatus_t   FinalizeInit(void);

    PreprocStatus_t   Halt(void);

    PreprocStatus_t   Input(Buffer_t Buffer);

    PreprocStatus_t   GetControl(stm_se_ctrl_t   Control,
                                 void           *Data);
    PreprocStatus_t   SetControl(stm_se_ctrl_t   Control,
                                 const void     *Data);
    PreprocStatus_t   GetCompoundControl(stm_se_ctrl_t   Control,
                                         void           *Data);
    PreprocStatus_t   SetCompoundControl(stm_se_ctrl_t   Control,
                                         const void     *Data);

    PreprocStatus_t   InjectDiscontinuity(stm_se_discontinuity_t  Discontinuity);

    static PreprocVideoCaps_t   GetCapabilities(void);

protected:
    stm_blitter_t            *blitter;
    stm_blitter_surface_t    *src;
    stm_blitter_surface_t    *dst;

    // Should it be local function variable or class variable
    bitmap_t                  src_bitmap;
    bitmap_t                  dst_bitmap;

    Buffer_t PreprocFrameBuffer2;            // Buffer for second field
    Buffer_t PreprocFrameBufferIntermediate; //intermediate buffer for format conversion

    PreprocStatus_t   GetBlitSurfaceFormat(surface_format_t     Format,
                                           stm_blitter_surface_format_t    *BlitFormat,
                                           uint32_t *VAlign);
    PreprocStatus_t   GetBufferOffsetSize(stm_blitter_surface_format_t  BlitFormat,
                                          stm_blitter_dimension_t *AlignedSize, stm_blitter_surface_address_t *BufAddr, unsigned long *BufSize);
    PreprocStatus_t   PrepareSrcBlitSurface(void *InputBufferAddrPhys, uint32_t BufferFlag);
    PreprocStatus_t   PrepareDstBlitSurface(void *PreprocBufferAddrPhys, ARCCtrl_t *ARCCtrl);
    PreprocStatus_t   BlitSurface(Buffer_t InputBuffer, Buffer_t OutputBuffer, uint32_t BufferFlag);
    PreprocStatus_t   RepeatOutputFrame(Buffer_t InputBuffer, Buffer_t OutputBuffer, FRCCtrl_t *FRCCtrl);
    PreprocStatus_t   ProcessField(Buffer_t InputBuffer, Buffer_t OutputBuffer, bool SecondField);
    PreprocStatus_t   FillPictureBorder();
    void              AdjustBorderRectToHWConstraints(stm_blitter_dimension_t *buffer_dim, stm_blitter_rect_t *black_rect);

private:
    unsigned int    mRelayfsIndex;

    DISALLOW_COPY_AND_ASSIGN(Preproc_Video_Blitter_c);
    void DumpBlitterTaskDescriptor(bitmap_t *Input, bitmap_t *Output);
};

// ---------------------------------------------------------------------
//
// Documentation
//

/*! \class Preproc_Video_Blitter_c
\brief Video blitter implementation of the Preprocessor classes.

This class will function as a starting point for developing a video Preprocessor class,
whereby the blitter will be used for required preprocessing.

It will simply take buffers from its input, convert to the required resolution and format
and pass them to its output ring.

*/

#endif /* PREPROC_VIDEO_BLITTER_H */
