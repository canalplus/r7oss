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

#ifndef H_CODEC_MME_AUDIO_VORBIS
#define H_CODEC_MME_AUDIO_VORBIS

#include "codec_mme_audio.h"
#include "codec_mme_audio_stream.h"
#include "vorbis_audio.h"

#undef TRACE_TAG
#define TRACE_TAG "Codec_MmeAudioVorbis_c"

class Codec_MmeAudioVorbis_c : public Codec_MmeAudioStream_c
{
public:
    Codec_MmeAudioVorbis_c(void);
    ~Codec_MmeAudioVorbis_c(void);

    static void     GetCapabilities(stm_se_audio_dec_capability_t::audio_dec_vorbis_capability_s *cap,
                                    const MME_LxAudioDecoderHDInfo_t decInfo);

protected:
    CodecStatus_t   FillOutTransformerGlobalParameters(MME_LxAudioDecoderGlobalParams_t *GlobalParams);
};
#endif
