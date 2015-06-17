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

#ifndef H_CODEC_MME_AUDIO_SILENCE
#define H_CODEC_MME_AUDIO_SILENCE

#include "codec_mme_audio.h"
#include "codec_mme_audio_dtshd.h"

#undef TRACE_TAG
#define TRACE_TAG "Codec_MmeAudioSilence_c"

#define SILENCE_GENERATOR_MAX_SAMPLE_COUNT       1024

class Codec_MmeAudioSilence_c : public Codec_MmeAudio_c
{
public:
    Codec_MmeAudioSilence_c(void);
    CodecStatus_t FinalizeInit(void);
    ~Codec_MmeAudioSilence_c(void);

    //
    // Extension to base functions
    //
    CodecStatus_t   HandleCapabilities(unsigned int ActualTransformer);
    CodecStatus_t   HandleCapabilities(void);
    CodecStatus_t   ParseCapabilities(unsigned int ActualTransformer);

protected:
    CodecStatus_t   FillOutTransformerInitializationParameters(void);
    CodecStatus_t   FillOutSetStreamParametersCommand(void);
    CodecStatus_t   FillOutDecodeCommand(void);
    CodecStatus_t   ValidateDecodeContext(CodecBaseDecodeContext_t *Context);
    CodecStatus_t   DumpSetStreamParameters(void    *Parameters);
    void            SetCommandIO(void);
    CodecStatus_t   DumpDecodeParameters(void   *Parameters);
};

#endif /* H_CODEC_MME_AUDIO_SILENCE */
