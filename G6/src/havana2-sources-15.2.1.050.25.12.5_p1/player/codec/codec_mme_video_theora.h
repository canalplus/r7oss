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

#ifndef H_CODEC_MME_VIDEO_THEORA
#define H_CODEC_MME_VIDEO_THEORA

#include "codec_mme_video.h"

#undef TRACE_TAG
#define TRACE_TAG "Codec_MmeVideoTheora_c"

#define THEORA_NUM_MME_INPUT_BUFFERS            2
#define THEORA_NUM_MME_OUTPUT_BUFFERS           3
#define THEORA_NUM_MME_BUFFERS                  (THEORA_NUM_MME_INPUT_BUFFERS+THEORA_NUM_MME_OUTPUT_BUFFERS)

#define THEORA_MME_CURRENT_FRAME_BUFFER_RASTER  0
#define THEORA_MME_REFERENCE_FRAME_BUFFER       1
#define THEORA_MME_GOLDEN_FRAME_BUFFER          2
#define THEORA_MME_CURRENT_FRAME_BUFFER_LUMA    3
#define THEORA_MME_CURRENT_FRAME_BUFFER_CHROMA  4

#define THEORA_DEFAULT_PICTURE_WIDTH            320
#define THEORA_DEFAULT_PICTURE_HEIGHT           240

#define THEORA_BUFFER_SIZE                      0x200000


/// The Theora video codec proxy
class Codec_MmeVideoTheora_c : public Codec_MmeVideo_c
{
public:
    Codec_MmeVideoTheora_c(void);
    ~Codec_MmeVideoTheora_c(void);

protected:
    CodecStatus_t   HandleCapabilities(void);

    CodecStatus_t   FillOutTransformerInitializationParameters(void);
    CodecStatus_t   FillOutSetStreamParametersCommand(void);
    CodecStatus_t   FillOutDecodeCommand(void);
    CodecStatus_t   FillOutDecodeBufferRequest(DecodeBufferRequest_t    *Request);

    CodecStatus_t   ValidateDecodeContext(CodecBaseDecodeContext_t      *Context);
    CodecStatus_t   DumpSetStreamParameters(void                        *Parameters);
    CodecStatus_t   DumpDecodeParameters(void                           *Parameters);
    CodecStatus_t   CheckCodecReturnParameters(CodecBaseDecodeContext_t *Context);

private:
    unsigned int                        CodedWidth;
    unsigned int                        CodedHeight;

    THEORA_InitTransformerParam_t       TheoraInitializationParameters;
    THEORA_CapabilityParams_t           TheoraTransformCapability;

    THEORA_SetGlobalParamSequence_t     TheoraGlobalParamSequence;

    allocator_device_t                  InfoHeaderMemoryDevice;
    allocator_device_t                  CommentHeaderMemoryDevice;
    allocator_device_t                  SetupHeaderMemoryDevice;
    allocator_device_t                  BufferMemoryDevice;

    bool                                RestartTransformer;
    bool                                RasterToOmega;

    DISALLOW_COPY_AND_ASSIGN(Codec_MmeVideoTheora_c);
};

#endif
