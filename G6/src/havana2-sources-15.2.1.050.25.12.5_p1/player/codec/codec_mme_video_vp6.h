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

#ifndef H_CODEC_MME_VIDEO_VP6
#define H_CODEC_MME_VIDEO_VP6

#include "codec_mme_video.h"
#include <VP6_VideoTransformerTypes.h>

#undef TRACE_TAG
#define TRACE_TAG "Codec_MmeVideoVp6_c"

#define VP6_NUM_MME_INPUT_BUFFERS               3
#define VP6_NUM_MME_OUTPUT_BUFFERS              3
#define VP6_NUM_MME_BUFFERS                     (VP6_NUM_MME_INPUT_BUFFERS+VP6_NUM_MME_OUTPUT_BUFFERS)

#define VP6_MME_CODED_DATA_BUFFER               0
#define VP6_MME_CURRENT_FRAME_BUFFER_RASTER     1
#define VP6_MME_REFERENCE_FRAME_BUFFER          2
#define VP6_MME_GOLDEN_FRAME_BUFFER             3
#define VP6_MME_CURRENT_FRAME_BUFFER_LUMA       4
#define VP6_MME_CURRENT_FRAME_BUFFER_CHROMA     5

#define VP6_DEFAULT_PICTURE_WIDTH               720
#define VP6_DEFAULT_PICTURE_HEIGHT              576

class Codec_MmeVideoVp6_c : public Codec_MmeVideo_c
{
public:
    Codec_MmeVideoVp6_c(void);
    ~Codec_MmeVideoVp6_c(void);

private:
    VP6_CapabilityParams_t              Vp6TransformCapability;
    VP6_InitTransformerParam_t          Vp6InitializationParameters;

    bool                                RestartTransformer;
    bool                                RasterToOmega;
    unsigned int                        DecodingWidth;
    unsigned int                        DecodingHeight;

    CodecStatus_t   HandleCapabilities(void);

    CodecStatus_t   FillOutTransformerInitializationParameters(void);
    CodecStatus_t   FillOutSetStreamParametersCommand(void);
    CodecStatus_t   FillOutDecodeCommand(void);

    CodecStatus_t   ValidateDecodeContext(CodecBaseDecodeContext_t       *Context);
    CodecStatus_t   DumpSetStreamParameters(void                           *Parameters);
    CodecStatus_t   DumpDecodeParameters(void                           *Parameters);

    CodecStatus_t   SendMMEStreamParameters(void);
    CodecStatus_t   CheckCodecReturnParameters(CodecBaseDecodeContext_t *Context);
};

#endif
