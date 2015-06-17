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

#ifndef H_CODEC_MME_VIDEO_RMV
#define H_CODEC_MME_VIDEO_RMV

#include "codec_mme_video.h"

#undef TRACE_TAG
#define TRACE_TAG "Codec_MmeVideoRmv_c"

#define RMV_DEFAULT_PICTURE_WIDTH               320
#define RMV_DEFAULT_PICTURE_HEIGHT              240

/// The RMV video codec proxy.
class Codec_MmeVideoRmv_c : public Codec_MmeVideo_c
{
public:
    Codec_MmeVideoRmv_c(void);
    ~Codec_MmeVideoRmv_c(void);

private:
    RV89Dec_InitTransformerParam_t InitializationParameters;

    bool                           RestartTransformer;

    CodecStatus_t   Connect(Port_c *Port);
    CodecStatus_t   HandleCapabilities(void);
    CodecStatus_t   FillOutTransformerInitializationParameters(void);
    CodecStatus_t   FillOutSetStreamParametersCommand(void);
    CodecStatus_t   FillOutDecodeCommand(void);
    CodecStatus_t   FillOutDecodeBufferRequest(DecodeBufferRequest_t          *Request);
    CodecStatus_t   ValidateDecodeContext(CodecBaseDecodeContext_t       *Context);
    CodecStatus_t   DumpSetStreamParameters(void                           *Parameters);
    CodecStatus_t   DumpDecodeParameters(void                           *Parameters);

    CodecStatus_t   SendMMEStreamParameters(void);
    CodecStatus_t   CheckCodecReturnParameters(CodecBaseDecodeContext_t *Context);
};

#endif
