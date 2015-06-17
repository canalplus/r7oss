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

#ifndef H_CODEC_MME_VIDEO_AVS
#define H_CODEC_MME_VIDEO_AVS

#include "codec_mme_video.h"

#undef TRACE_TAG
#define TRACE_TAG "Codec_MmeVideoAvs_c"

#if defined (TRANSFORMER_AVSDEC_HD)
#define AVS_NUM_MME_INPUT_BUFFERS                       0
#define AVS_NUM_MME_OUTPUT_BUFFERS                      0
#else
#define AVS_NUM_MME_INPUT_BUFFERS                       3
#define AVS_NUM_MME_OUTPUT_BUFFERS                      0
#define AVS_NUM_MME_BUFFERS                             (AVS_NUM_MME_INPUT_BUFFERS+AVS_NUM_MME_OUTPUT_BUFFERS)

#define AVS_MME_CURRENT_FRAME_BUFFER                    0
#define AVS_MME_FORWARD_REFERENCE_FRAME_BUFFER          1
#define AVS_MME_BACKWARD_REFERENCE_FRAME_BUFFER         2
#endif

#define AVS_MAXIMUM_PICTURE_WIDTH                       1920
#define AVS_MAXIMUM_PICTURE_HEIGHT                      1088

/// The AVS video codec proxy
class Codec_MmeVideoAvs_c : public Codec_MmeVideo_c
{
public:
    Codec_MmeVideoAvs_c(void);
    ~Codec_MmeVideoAvs_c(void);

protected:
    unsigned int                                DecodingWidth;
    unsigned int                                DecodingHeight;

    MME_AVSVideoDecodeCapabilityParams_t        AvsTransformCapability;
    MME_AVSVideoDecodeInitParams_t              AvsInitializationParameters;

    allocator_device_t                          IntraMbStructMemoryDevice;
    allocator_device_t                          MbStructMemoryDevice;

    bool                                        RasterOutput;

    CodecStatus_t   HandleCapabilities(void);
    CodecStatus_t   FillOutTransformerInitializationParameters(void);
    CodecStatus_t   FillOutSetStreamParametersCommand(void);
    CodecStatus_t   FillOutDecodeCommand(void);
    CodecStatus_t   ValidateDecodeContext(CodecBaseDecodeContext_t *Context);
    CodecStatus_t   DumpSetStreamParameters(void    *Parameters);
    CodecStatus_t   DumpDecodeParameters(void    *Parameters);
    CodecStatus_t   CheckCodecReturnParameters(CodecBaseDecodeContext_t *Context);

private:
    DISALLOW_COPY_AND_ASSIGN(Codec_MmeVideoAvs_c);
};

#endif
