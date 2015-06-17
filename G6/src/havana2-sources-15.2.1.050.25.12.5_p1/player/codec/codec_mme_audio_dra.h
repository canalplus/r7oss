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

#ifndef H_CODEC_MME_AUDIO_DRA
#define H_CODEC_MME_AUDIO_DRA

#include "codec_mme_audio.h"
#include "dra_audio.h"

#undef TRACE_TAG
#define TRACE_TAG "Codec_MmeAudioDra_c"

class Codec_MmeAudioDra_c : public Codec_MmeAudio_c
{
public:
    Codec_MmeAudioDra_c(void);
    ~Codec_MmeAudioDra_c(void);

    static void     GetCapabilities(stm_se_audio_dec_capability_t::audio_dec_dra_capability_s *cap,
                                    const MME_LxAudioDecoderHDInfo_t decInfo);

protected:
    eAccDecoderId DecoderId;

    CodecStatus_t   FillOutTransformerGlobalParameters(MME_LxAudioDecoderGlobalParams_t *GlobalParams);
    CodecStatus_t   FillOutTransformerInitializationParameters(void);
    CodecStatus_t   FillOutSetStreamParametersCommand(void);
    CodecStatus_t   FillOutDecodeCommand(void);
    CodecStatus_t   ValidateDecodeContext(CodecBaseDecodeContext_t *Context);
    CodecStatus_t   DumpSetStreamParameters(void *Parameters);
    CodecStatus_t   DumpDecodeParameters(void    *Parameters);
};

#endif
