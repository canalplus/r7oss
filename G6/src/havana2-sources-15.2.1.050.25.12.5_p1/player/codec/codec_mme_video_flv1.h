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

#ifndef H_CODEC_MME_VIDEO_FLV1
#define H_CODEC_MME_VIDEO_FLV1

#include "codec_mme_video.h"
#include <FLV1_VideoTransformerTypes.h>

#undef TRACE_TAG
#define TRACE_TAG "Codec_MmeVideoFlv1_c"

#define FLV1_NUM_MME_INPUT_BUFFERS              2
#define FLV1_NUM_MME_OUTPUT_BUFFERS             3
#define FLV1_NUM_MME_BUFFERS                    (FLV1_NUM_MME_INPUT_BUFFERS+FLV1_NUM_MME_OUTPUT_BUFFERS)

#define FLV1_MME_CODED_DATA_BUFFER              0
#define FLV1_MME_CURRENT_FRAME_BUFFER_RASTER    1
#define FLV1_MME_REFERENCE_FRAME_BUFFER         2
#define FLV1_MME_CURRENT_FRAME_BUFFER_LUMA      3
#define FLV1_MME_CURRENT_FRAME_BUFFER_CHROMA    4

#define FLV1_DECODE_PICTURE_TYPE_I              1
#define FLV1_DECODE_PICTURE_TYPE_P              2

/// The FLV1 video codec proxy.
class Codec_MmeVideoFlv1_c : public Codec_MmeVideo_c
{
public:
    Codec_MmeVideoFlv1_c(void);
    ~Codec_MmeVideoFlv1_c(void);

protected:

    CodecStatus_t   HandleCapabilities(void);
    CodecStatus_t   InitializeMMETransformer(void);

    CodecStatus_t   FillOutTransformerInitializationParameters(void);
    CodecStatus_t   FillOutSetStreamParametersCommand(void);
    CodecStatus_t   FillOutDecodeCommand(void);

    CodecStatus_t   ValidateDecodeContext(CodecBaseDecodeContext_t       *Context);
    CodecStatus_t   DumpSetStreamParameters(void                           *Parameters);
    CodecStatus_t   DumpDecodeParameters(void                           *Parameters);

    CodecStatus_t   SendMMEStreamParameters(void);
    CodecStatus_t   CheckCodecReturnParameters(CodecBaseDecodeContext_t *Context);

private:
    FLV1_InitTransformerParam_t         Flv1InitializationParameters;

    bool                                RestartTransformer;
    bool                                RasterToOmega;
    unsigned int                        DecodingWidth;
    unsigned int                        DecodingHeight;
};

#endif
