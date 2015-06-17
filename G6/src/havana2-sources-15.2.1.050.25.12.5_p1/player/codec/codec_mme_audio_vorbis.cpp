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

////////////////////////////////////////////////////////////////////////////
/// \class Codec_MmeAudioVorbis_c
///
/// The VORBIS audio codec proxy.
///

#include "codec_mme_audio_vorbis.h"
#include "codec_capabilities.h"
#include "vorbis_audio.h"

#undef TRACE_TAG
#define TRACE_TAG "Codec_MmeAudioVorbis_c"

#define BUFFER_VORBIS_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT         "VorbisAudioCodecStreamParameterContext"
#define BUFFER_VORBIS_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT_TYPE    {BUFFER_VORBIS_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(StreamAudioCodecStreamParameterContext_t)}

static BufferDataDescriptor_t           VorbisAudioCodecStreamParameterContextDescriptor   = BUFFER_VORBIS_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT_TYPE;

#define BUFFER_VORBIS_AUDIO_CODEC_DECODE_CONTEXT                   "VorbisAudioCodecDecodeContext"
#define BUFFER_VORBIS_AUDIO_CODEC_DECODE_CONTEXT_TYPE              {BUFFER_VORBIS_AUDIO_CODEC_DECODE_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(StreamAudioCodecDecodeContext_t)}

static BufferDataDescriptor_t           VorbisAudioCodecDecodeContextDescriptor    = BUFFER_VORBIS_AUDIO_CODEC_DECODE_CONTEXT_TYPE;


//{{{  Constructor
///
Codec_MmeAudioVorbis_c::Codec_MmeAudioVorbis_c(void)
    : Codec_MmeAudioStream_c()
{
    Configuration.CodecName                             = "Vorbis audio";
    Configuration.StreamParameterContextCount           = 1;
    Configuration.StreamParameterContextDescriptor      = &VorbisAudioCodecStreamParameterContextDescriptor;
    Configuration.DecodeContextCount                    = 12;   // Vorbis demands 8 SEND_BUFFERS before it starts thinking about decoding
    Configuration.DecodeContextDescriptor               = &VorbisAudioCodecDecodeContextDescriptor;
    Configuration.MaximumSampleCount                    = VORBIS_MAX_DECODED_SAMPLE_COUNT;
    AudioDecoderTransformCapabilityMask.DecoderCapabilityFlags = (1 << ACC_OGG);

    DecoderId                                           = ACC_OGG_ID;
    SendbufTriggerTransformCount                        = 8;    // Vorbis demands 8 SEND_BUFFERS before it starts thinking about decoding
}
//}}}
//{{{  Destructor
///
Codec_MmeAudioVorbis_c::~Codec_MmeAudioVorbis_c(void)
{
    Halt();
}
//}}}

//{{{  FillOutTransformerGlobalParameters
////////////////////////////////////////////////////////////////////////////
///
/// Populate the supplied structure with parameters for VORBIS audio.
///
///
CodecStatus_t Codec_MmeAudioVorbis_c::FillOutTransformerGlobalParameters(MME_LxAudioDecoderGlobalParams_t *GlobalParams_p)
{
    MME_LxAudioDecoderGlobalParams_t   &GlobalParams    = *GlobalParams_p;
    SE_INFO(group_decoder_audio, "Initializing vorbis audio decoder\n");
    GlobalParams.StructSize             = sizeof(MME_LxAudioDecoderGlobalParams_t);
    MME_LxOvConfig_t &Config            = *((MME_LxOvConfig_t *)GlobalParams.DecConfig);
    Config.DecoderId                    = DecoderId;
    Config.StructSize                   = sizeof(Config);
    Config.MaxNbPages                   = 8;
    Config.MaxPageSize                  = 8192;
    Config.NbSamplesOut                 = 4096;

    if (SE_IS_DEBUG_ON(group_decoder_audio))
    {
        SE_DEBUG(group_decoder_audio, " DecoderId                  %d\n", Config.DecoderId);
        SE_DEBUG(group_decoder_audio, " StructSize                 %d\n", Config.StructSize);
        SE_DEBUG(group_decoder_audio, " MaxNbPages                 %d\n", Config.MaxNbPages);
        SE_DEBUG(group_decoder_audio, " MaxPageSize                %d\n", Config.MaxPageSize);
        SE_DEBUG(group_decoder_audio, " NbSamplesOut               %d\n", Config.NbSamplesOut);
    }

    return Codec_MmeAudio_c::FillOutTransformerGlobalParameters(GlobalParams_p);
}
//}}}
//{{{ GetCapabilities
////////////////////////////////////////////////////////////////////////////
///
///  Public static function to fill OGG codec capabilities
///  to expose it through STM_SE_CTRL_GET_CAPABILITY_AUDIO_DECODE control.
///
void Codec_MmeAudioVorbis_c::GetCapabilities(stm_se_audio_dec_capability_t::audio_dec_vorbis_capability_s *cap,
                                             const MME_LxAudioDecoderHDInfo_t decInfo)
{
    cap->common.capable = (decInfo.DecoderCapabilityFlags     & (1 << ACC_OGG)
                           & SE_AUDIO_DEC_CAPABILITIES        & (1 << ACC_OGG)
                          ) ? true : false;
}
//}}}
