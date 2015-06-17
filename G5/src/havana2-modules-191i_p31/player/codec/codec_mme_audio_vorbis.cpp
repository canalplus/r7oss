/************************************************************************
Copyright (C) 2007 STMicroelectronics. All Rights Reserved.

This file is part of the Player2 Library.

Player2 is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

Player2 is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with player2; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The Player2 Library may alternatively be licensed under a proprietary
license from ST.

Source file name : codec_mme_audio_vorbis.cpp
Author :           Julian

Implementation of the Ogg Vorbis Audio codec class for player 2.


Date            Modification            Name
----            ------------            --------
10-Mar-09       Created                 Julian

************************************************************************/

////////////////////////////////////////////////////////////////////////////
/// \class Codec_MmeAudioVorbis_c
///
/// The VORBIS audio codec proxy.
///

// /////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "codec_mme_audio_vorbis.h"
#include "vorbis_audio.h"

#ifdef __KERNEL__
extern "C"{void flush_cache_all();};
#endif

// /////////////////////////////////////////////////////////////////////////
//
// Locally defined constants
//


// /////////////////////////////////////////////////////////////////////////
//
// Locally defined structures
//

#define BUFFER_VORBIS_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT         "VorbisAudioCodecStreamParameterContext"
#define BUFFER_VORBIS_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT_TYPE    {BUFFER_VORBIS_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(StreamAudioCodecStreamParameterContext_t)}

static BufferDataDescriptor_t           VorbisAudioCodecStreamParameterContextDescriptor   = BUFFER_VORBIS_AUDIO_CODEC_STREAM_PARAMETER_CONTEXT_TYPE;

#define BUFFER_VORBIS_AUDIO_CODEC_DECODE_CONTEXT                   "VorbisAudioCodecDecodeContext"
#define BUFFER_VORBIS_AUDIO_CODEC_DECODE_CONTEXT_TYPE              {BUFFER_VORBIS_AUDIO_CODEC_DECODE_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(StreamAudioCodecDecodeContext_t)}

static BufferDataDescriptor_t           VorbisAudioCodecDecodeContextDescriptor    = BUFFER_VORBIS_AUDIO_CODEC_DECODE_CONTEXT_TYPE;


//{{{  Constructor
////////////////////////////////////////////////////////////////////////////
///
/// Fill in the configuration parameters used by the super-class and reset everything.
///
Codec_MmeAudioVorbis_c::Codec_MmeAudioVorbis_c( void )
{
    CODEC_TRACE ("Codec_MmeAudioVorbis_c::%s\n", __FUNCTION__);
    Configuration.CodecName                             = "Vorbis audio";

    Configuration.StreamParameterContextCount           = 1;
    Configuration.StreamParameterContextDescriptor      = &VorbisAudioCodecStreamParameterContextDescriptor;

    Configuration.DecodeContextCount                    = 9;   // Vorbis demands 8 SEND_BUFFERS before it starts thinking about decoding
    Configuration.DecodeContextDescriptor               = &VorbisAudioCodecDecodeContextDescriptor;

    Configuration.MaximumSampleCount                    = 4096;

    AudioDecoderTransformCapabilityMask.DecoderCapabilityFlags = (1 << ACC_OGG);

    DecoderId                                           = ACC_OGG_ID;

    SendbufTriggerTransformCount                        = 8;    // Vorbis demands 8 SEND_BUFFERS before it starts thinking about decoding

    Reset();
}
//}}}
//{{{  Destructor
////////////////////////////////////////////////////////////////////////////
///
///     Destructor function, ensures a full halt and reset 
///     are executed for all levels of the class.
///
Codec_MmeAudioVorbis_c::~Codec_MmeAudioVorbis_c( void )
{
    CODEC_TRACE ("Codec_MmeAudioVorbis_c::%s\n", __FUNCTION__);
    Halt();
    Reset();
}
//}}}

//{{{  FillOutTransformerGlobalParameters
////////////////////////////////////////////////////////////////////////////
///
/// Populate the supplied structure with parameters for VORBIS audio.
///
///
CodecStatus_t Codec_MmeAudioVorbis_c::FillOutTransformerGlobalParameters (MME_LxAudioDecoderGlobalParams_t* GlobalParams_p)
{
    MME_LxAudioDecoderGlobalParams_t   &GlobalParams    = *GlobalParams_p;

    CODEC_TRACE ("Codec_MmeAudioVorbis_c::%s\n", __FUNCTION__);

    GlobalParams.StructSize             = sizeof(MME_LxAudioDecoderGlobalParams_t);

    MME_LxOvConfig_t &Config            = *((MME_LxOvConfig_t*)GlobalParams.DecConfig);

    Config.DecoderId                    = DecoderId;
    Config.StructSize                   = sizeof(Config);
    Config.MaxNbPages                   = 8;
    Config.MaxPageSize                  = 8192;
    Config.NbSamplesOut                 = 4096;

#if 0
    if (ParsedFrameParameters != NULL)
    {
        VorbisAudioStreamParameters_s*     StreamParams    = (VorbisAudioStreamParameters_s*)ParsedFrameParameters->StreamParameterStructure;
    }
    else
    {
        CODEC_TRACE("%s - No Params\n", __FUNCTION__);
        Config.StructSize               = 2 * sizeof(U32);      // only transmit the ID and StructSize (all other params are irrelevant)
    }
#endif

    CODEC_TRACE("DecoderId                  %d\n", Config.DecoderId);
    CODEC_TRACE("StructSize                 %d\n", Config.StructSize);
    CODEC_TRACE("MaxNbPages                 %d\n", Config.MaxNbPages);
    CODEC_TRACE("MaxPageSize                %d\n", Config.MaxPageSize);
    CODEC_TRACE("NbSamplesOut               %d\n", Config.NbSamplesOut);

    return Codec_MmeAudio_c::FillOutTransformerGlobalParameters (GlobalParams_p);
}
//}}}

