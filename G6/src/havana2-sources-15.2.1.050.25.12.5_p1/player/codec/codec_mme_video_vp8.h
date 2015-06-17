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

#ifndef H_CODEC_MME_VIDEO_VP8
#define H_CODEC_MME_VIDEO_VP8

#include "codec_mme_video.h"
#include <VP8_VideoTransformerTypes.h>

#undef TRACE_TAG
#define TRACE_TAG "Codec_MmeVideoVp8_c"

#define VP8_NUM_MME_INPUT_BUFFERS               4
#define VP8_NUM_MME_OUTPUT_BUFFERS              3
#define VP8_NUM_MME_BUFFERS                     (VP8_NUM_MME_INPUT_BUFFERS+VP8_NUM_MME_OUTPUT_BUFFERS)

#define VP8_MME_CODED_DATA_BUFFER               0
#define VP8_MME_CURRENT_FRAME_BUFFER_RASTER     1
#define VP8_MME_REFERENCE_FRAME_BUFFER          2
#define VP8_MME_GOLDEN_FRAME_BUFFER             3
#define VP8_MME_ALTERNATE_FRAME_BUFFER          4
#define VP8_MME_CURRENT_FRAME_BUFFER_LUMA       5
#define VP8_MME_CURRENT_FRAME_BUFFER_CHROMA     6

#define VP8_DEFAULT_PICTURE_WIDTH               720
#define VP8_DEFAULT_PICTURE_HEIGHT              576

/// The Vp8 video codec proxy.
class Codec_MmeVideoVp8_c : public Codec_MmeVideo_c
{
public:
    Codec_MmeVideoVp8_c(void);
    ~Codec_MmeVideoVp8_c(void);

    CodecStatus_t   LowPowerEnter(void);
    CodecStatus_t   LowPowerExit(void);

private:
    VP8_CapabilityParams_t              Vp8TransformCapability;
    VP8_InitTransformerParam_t          Vp8InitializationParameters;

    bool                                RestartTransformer;
    bool                                RasterToOmega;
    unsigned int                        DecodingWidth;
    unsigned int                        DecodingHeight;

    CodecStatus_t   HandleCapabilities(void);

    CodecStatus_t   FillOutTransformerInitializationParameters(void);
    CodecStatus_t   FillOutSetStreamParametersCommand(void);
    CodecStatus_t   FillOutDecodeCommand(void);

    CodecStatus_t   ValidateDecodeContext(CodecBaseDecodeContext_t  *Context);
    CodecStatus_t   DumpSetStreamParameters(void                    *Parameters);
    CodecStatus_t   DumpDecodeParameters(void                       *Parameters);

    CodecStatus_t   SendMMEStreamParameters(void);
    CodecStatus_t   CheckCodecReturnParameters(CodecBaseDecodeContext_t *Context);
};

#endif
