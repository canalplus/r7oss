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
//

#include "codec_mme_audio.h"
#include "codec_mme_audio_lpcm.h"
#include "codec_mme_audio_eac3.h"
#include "codec_mme_audio_mpeg.h"
#include "codec_mme_audio_dtshd.h"
#include "codec_mme_audio_wma.h"
#include "codec_mme_audio_vorbis.h"
#include "codec_mme_audio_aac.h"
#include "codec_mme_audio_rma.h"
#include "codec_mme_audio_spdifin.h"
#include "codec_mme_audio_mlp.h"
#include "codec_mme_audio_dra.h"
#include "codec_capabilities.h"

#undef TRACE_TAG
#define TRACE_TAG "Codec_Capabilities_c"

int Codec_Capabilities_c::GetAudioDecodeCapabilities(stm_se_audio_dec_capability_t *decCap)
{
    const char                   *transformerName0   = AUDIO_DECODER_TRANSFORMER_NAME0;
    const char                   *transformerName1   = AUDIO_DECODER_TRANSFORMER_NAME1;
    MME_ERROR                     err                = MME_SUCCESS;
    MME_TransformerCapability_t   cap0;
    MME_TransformerCapability_t   cap1;
    MME_LxAudioDecoderHDInfo_t    decInfo0;
    MME_LxAudioDecoderHDInfo_t    decInfo1;
    MME_LxAudioDecoderHDInfo_t    decInfoJoint;
    // TODO(JG)
    // Today, we cannot know how many transformers should be available or not.
    // So we do not interpret MME_UNKNOWN_TRANSFORMER as an error because
    // the number of available transformer depends on the running platform.
    // If we move StreamingEngine to DeviceTree, we should be able to know how many transformers
    // must be available on a given board, and consequently interpret this case as an error.
    // Request audio decoder transformer capabilities - First Firmware
    cap0.StructSize          = sizeof(MME_TransformerCapability_t);
    cap0.TransformerInfo_p   = &decInfo0;
    cap0.TransformerInfoSize = sizeof(MME_LxAudioDecoderHDInfo_t);
    memset(&decInfo0, 0, sizeof(MME_LxAudioDecoderHDInfo_t));
    err = MME_GetTransformerCapability(transformerName0, &cap0);

    if (MME_UNKNOWN_TRANSFORMER == err)
    {
        SE_INFO(group_misc, "[%s] is not available, ignore its capabilities\n",
                transformerName0);
    }
    else if (MME_SUCCESS != err)
    {
        SE_ERROR("Cannot get transformer[%s] capability (MME_ERROR: %d)\n",
                 transformerName0, err);
        return -ENODEV;
    }

    // Request audio decoder transformer capabilities - Second Firmware
    cap1.StructSize          = sizeof(MME_TransformerCapability_t);
    cap1.TransformerInfo_p   = &decInfo1;
    cap1.TransformerInfoSize = sizeof(MME_LxAudioDecoderHDInfo_t);
    memset(&decInfo1, 0, sizeof(MME_LxAudioDecoderHDInfo_t));
    err = MME_GetTransformerCapability(transformerName1, &cap1);

    if (MME_UNKNOWN_TRANSFORMER == err)
    {
        SE_INFO(group_misc, "[%s] is not available, ignore its capabilities\n",
                transformerName1);
    }
    else if (MME_SUCCESS != err)
    {
        SE_ERROR("Cannot get transformer[%s] capability (MME_ERROR: %d)\n",
                 transformerName1, err);
        return -ENODEV;
    }

    /* Build an ORed union of capabilities returned by Decoder Transfromers */
    memset(&decInfoJoint, 0, sizeof(MME_LxAudioDecoderHDInfo_t));
    decInfoJoint.DecoderCapabilityFlags       = decInfo0.DecoderCapabilityFlags
                                                | decInfo1.DecoderCapabilityFlags;
    decInfoJoint.StreamDecoderCapabilityFlags = decInfo0.StreamDecoderCapabilityFlags
                                                | decInfo1.StreamDecoderCapabilityFlags;

    for (int i = 0; i < DECODER_CAPABILITY_ARRAY_SIZE; i++)
    {
        decInfoJoint.DecoderCapabilityExtFlags[i] = decInfo0.DecoderCapabilityExtFlags[i]
                                                    | decInfo1.DecoderCapabilityExtFlags[i];
    }

    for (int i = 0; i < AUDIO_DECODER_NB_PCMCAPABILITY_FIELD; i++)
    {
        decInfoJoint.PcmProcessorCapabilityFlags[i] = decInfo0.PcmProcessorCapabilityFlags[i]
                                                      | decInfo1.PcmProcessorCapabilityFlags[i];
    }

    /* Initialize the structure before filling it */
    memset(decCap, 0, sizeof(stm_se_audio_dec_capability_t));
    /* Let each audio codec fill its own capabilties */
    Codec_MmeAudioLpcm_c::GetCapabilities(&decCap->pcm,         decInfoJoint);
    Codec_MmeAudioEAc3_c::GetCapabilities(&decCap->ac3,         decInfoJoint);
    Codec_MmeAudioMpeg_c::GetCapabilities(&decCap->mp2a,        decInfoJoint);
    Codec_MmeAudioMpeg_c::GetCapabilities(&decCap->mp3,         decInfoJoint);
    Codec_MmeAudioDtshd_c::GetCapabilities(&decCap->dts,        decInfoJoint);
    Codec_MmeAudioWma_c::GetCapabilities(&decCap->wma,          decInfoJoint);
    Codec_MmeAudioVorbis_c::GetCapabilities(&decCap->vorbis,    decInfoJoint);
    Codec_MmeAudioAac_c::GetCapabilities(&decCap->aac,          decInfoJoint);
    Codec_MmeAudioRma_c::GetCapabilities(&decCap->realaudio,    decInfoJoint);
    Codec_MmeAudioSpdifin_c::GetCapabilities(&decCap->iec61937, decInfoJoint);
    Codec_MmeAudioMlp_c::GetCapabilities(&decCap->dolby_truehd, decInfoJoint);
    Codec_MmeAudioDra_c::GetCapabilities(&decCap->dra,          decInfoJoint);

    return 0;
}

/*!
 * Codec_Capabilities_c::ExtractAudioExtendedFlags
 * Helper function to easily extract DecoderCapabilityExtFlags for a given codec
 * from MME_LxAudioDecoderHDInfo_t structure exposed by firware.
 */
int Codec_Capabilities_c::ExtractAudioExtendedFlags(const MME_LxAudioDecoderHDInfo_t decInfo,
                                                    const eDecoderCapabilityFlags codec)
{
    const int ExtFlagsCell   = decInfo.DecoderCapabilityExtFlags[codec / NB_OF_CODEC_PER_U32];
    const int ExtFlagsPos    = (codec % NB_OF_CODEC_PER_U32) * NB_OF_BITS_PER_CODEC;
    return (ExtFlagsCell & (0xF << ExtFlagsPos)) >> ExtFlagsPos;
}


int Codec_Capabilities_c::GetVideoDecodeCapabilities(stm_se_video_dec_capability_t *decCap)
{
    SE_ERROR("Not yet implemented!\n");
    return 0;
}
