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
// \class Codec_Capabilities_c
//
// This class allows client to access audio & video decoder capabilities.
// This class collects capabilities of each codec to expose it to client.

#ifndef H_CODEC_CAPABILITIES
#define H_CODEC_CAPABILITIES

////////////////////////////////////////////////////////////////////////////
//
//      Include any component headers

#include "stm_se.h"
#include <ACC_Transformers/acc_mmedefines.h>

#undef TRACE_TAG
#define TRACE_TAG "Codec_Capabilities_c"


//Defines below help to interpret MME_LxAudioDecoderHDInfo_t struct exposed by acc_mmedefines.h
// DecoderCapabilityExtFlags[4] - Codec info retrieved from MME:
//     - Each codec uses 4bits within a U32 to expose its extensions
//     - Each U32 contains extensions for 8 codecs
//     - Array of 4 U32 stores extensions for 32 codecs
#define NB_OF_BITS_PER_CODEC          4
#define NB_OF_CODEC_PER_U32           (32/NB_OF_BITS_PER_CODEC)
#define DECODER_CAPABILITY_ARRAY_SIZE 4

////////////////////////////////////////////////////////////////////////////
// SE_AUDIO_DEC_CAPABILITIES is a macro used
// to describe StreamingEngine player audio decoder driver capabilities.
//
#define SE_AUDIO_DEC_CAPABILITIES  (\
     (1<<ACC_PCM        )            \
  |  (1<<ACC_AC3        )            \
  |  (1<<ACC_MP2a       )            \
  |  (1<<ACC_MP3        )            \
  |  (1<<ACC_DTS        )            \
  |  (1<<ACC_MLP        )            \
  |  (1<<ACC_LPCM       )            \
/*|  (1<<ACC_SDDS       )         */ \
  |  (1<<ACC_WMA9       )            \
  |  (1<<ACC_OGG        )            \
  |  (1<<ACC_MP4a_AAC   )            \
  |  (1<<ACC_REAL_AUDIO )            \
/*|  (1<<ACC_HDCD       )         */ \
/*|  (1<<ACC_AMRWB      )         */ \
  |  (1<<ACC_WMAPROLSL  )            \
  |  (1<<ACC_DTS_LBR    )            \
/*|  (1<<ACC_GENERATOR  )         */ \
  |  (1<<ACC_SPDIFIN    )            \
  |  (1<<ACC_TRUEHD     )            \
/*|  (1<<ACC_FLAC       )         */ \
/*|  (1<<ACC_SBC        )         */ \
  |  (1<<ACC_DRA        )            \
  |  (1<<ACC_DOLBYPULSE )            \
/*|  (1<<ACC_AMRNB      )         */ \
/*|  (1<<ACC_G711       )         */ \
/*|  (1<<ACC_G729ab     )         */ \
/*|  (1<<ACC_G726       )         */ \
/*|  (1<<ACC_ALAC       )         */  )


class Codec_Capabilities_c
{
public:
    static int GetAudioDecodeCapabilities(stm_se_audio_dec_capability_t *decCap);
    static int GetVideoDecodeCapabilities(stm_se_video_dec_capability_t *decCap);

    static int ExtractAudioExtendedFlags(const MME_LxAudioDecoderHDInfo_t decInfo,
                                         const eDecoderCapabilityFlags codec);
};

#endif //H_CODEC_CAPABILITIES
