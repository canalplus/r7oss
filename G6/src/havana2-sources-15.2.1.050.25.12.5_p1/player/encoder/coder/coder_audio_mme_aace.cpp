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

#include "coder_audio_mme_aace.h"
#include "audio_conversions.h"

#undef TRACE_TAG
#define TRACE_TAG "Coder_Audio_Mme_Aace_c"

#define AACE_MME_CONTEXT         "AaceMmeContext"
#define AACE_MME_CONTEXT_TYPE   {AACE_MME_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(CoderAudioMmeAaceTransformContext_t)}

static BufferDataDescriptor_t gAaceMmeContextDescriptor = AACE_MME_CONTEXT_TYPE;

static const CoderAudioStaticConfiguration_t gAaceStaticConfig =
{
    STM_SE_ENCODE_STREAM_ENCODING_AUDIO_AAC,
    CODER_AUDIO_MME_MAX_NB_CHAN,
    CODER_AUDIO_MME_MAX_SMP_PER_INPUT_BUFFER,
    CODER_AUDIO_MME_AACE_MAX_SAMPLES_PER_FRAME,
    CODER_AUDIO_MME_AACE_MIN_SAMPLES_PER_FRAME,
    CODER_AUDIO_MME_AACE_MAX_CODED_FRAME_SIZE
};

static const CoderBaseAudioMetadata_t gAaceBaseAudioMetadata =
{
    CODER_AUDIO_MME_AACE_DEFAULT_ENCODING_SAMPLING_FREQ,
    CODER_AUDIO_MME_MAX_NB_CHAN,  // max channel count; expected to be <= 8
    {
        STM_SE_AUDIO_CHAN_L,
        STM_SE_AUDIO_CHAN_R,
        STM_SE_AUDIO_CHAN_STUFFING,
        STM_SE_AUDIO_CHAN_STUFFING,
        STM_SE_AUDIO_CHAN_STUFFING,
        STM_SE_AUDIO_CHAN_STUFFING,
        STM_SE_AUDIO_CHAN_STUFFING,
        STM_SE_AUDIO_CHAN_STUFFING,
    }
};

static const CoderAudioControls_t gAaceBaseControls =
{
    {
        CODER_AUDIO_MME_AACE_DEFAULT_ENCODING_VBR,
        (uint32_t)CODER_AUDIO_DEFAULT_ENCODING_BITRATE, // will be refined
        (uint32_t)CODER_AUDIO_MME_AACE_DEFAULT_ENCODING_VBR_QUALITY,
        (uint32_t)0,  // bitrate cap
    },
    false, // bitrate updated
    CODER_AUDIO_MME_AACE_DEFAULT_ENCODING_LOUDNESS_LEVEL,
    false, // crc not supported
    false, // SCMS not supported
    false  // SCMS not supported
};

static const CoderAudioMmeAaceSupportedMode_t gCoderAudioMmeAaceSupportedModesLut[] =
{
    {  8000, ACC_MODE10,   8000,   8000,   32000 },
    {  8000, ACC_MODE20,  16000,  16000,   64000 },
    {  8000, ACC_MODE20t, 16000,  16000,   64000 },

    { 11025, ACC_MODE10,   8000,   8000,   48000 },
    { 11025, ACC_MODE20,  16000,  16000,   96000 },
    { 11025, ACC_MODE20t, 16000,  16000,   96000 },

    { 12000, ACC_MODE10,   8000,   8000,   64000 },
    { 12000, ACC_MODE20,  16000,  16000,  128000 },
    { 12000, ACC_MODE20t, 16000,  16000,  128000 },

    { 16000, ACC_MODE10,  12000,  24000,   80000 },
    { 16000, ACC_MODE20,  10000,  40000,  160000 },
    { 16000, ACC_MODE20t, 10000,  40000,  160000 },

    { 22050, ACC_MODE10,  12000,  24000,   80000 },
    { 22050, ACC_MODE20,  10000,  40000,  160000 },
    { 22050, ACC_MODE20t, 10000,  40000,  160000 },

    { 24000, ACC_MODE10,  12000,  24000,   80000 },
    { 24000, ACC_MODE20,  10000,  40000,  160000 },
    { 24000, ACC_MODE20t, 10000,  40000,  160000 },

    { 32000, ACC_MODE10,  16000,  44000,  160000 },
    { 32000, ACC_MODE20,  16000,  81000,  320000 },
    { 32000, ACC_MODE20t, 16000,  81000,  320000 },

    { 44100, ACC_MODE10,  16000,  44000,  160000 },
    { 44100, ACC_MODE20,  16000,  81000,  320000 },
    { 44100, ACC_MODE20t, 16000,  81000,  320000 },

    { 48000, ACC_MODE10,  16000,  44000,  160000 },
    { 48000, ACC_MODE20,  16000,  81000,  320000 },
    { 48000, ACC_MODE20t, 16000,  81000,  320000 },

    /*Termination */
    { 0, ACC_MODE_ID,         0,      0,       0 },
};

static const CoderAudioMmeStaticConfiguration_t gMp3eMmeStaticConfig =
{
    ACC_GENERATE_ENCODERID(ACC_AACE_ID),
    sizeof(tMMEAaceConfig),
    sizeof(MME_AaceStatus_t),
    CODER_AUDIO_MME_AACE_MAX_CODEC_INTRISIC_FRAME_DELAY,
    CODER_AUDIO_MME_AACE_TRANSFORM_CONTEXT_POOL_DEPTH,
    sizeof(CoderAudioMmeAaceTransformContext_t),
    CODER_AUDIO_MME_AACE_MAX_OUT_MME_BUFFER_PER_TRANSFORM,
    1 + CODER_AUDIO_MME_AACE_MAX_OUT_MME_BUFFER_PER_TRANSFORM,
    1 + CODER_AUDIO_MME_AACE_MAX_OUT_MME_BUFFER_PER_TRANSFORM,
    sizeof(CoderAudioMmeAaceSpecificEncoderBArray_t),
    sizeof(CoderAudioMmeAaceStatusParams_t),
    false,
    false,
    false,
    &gAaceMmeContextDescriptor,
    {""}
};

///////

Coder_Audio_Mme_Aace_c::Coder_Audio_Mme_Aace_c(void)
    : Coder_Audio_Mme_c(gAaceStaticConfig, gAaceBaseAudioMetadata, gAaceBaseControls, gMp3eMmeStaticConfig)
{
    // update base class with codec specifics
    FillDefaultCodecSpecific(mMmeDynamicContext.CodecDynamicContext.CodecSpecificConfig);
}

Coder_Audio_Mme_Aace_c::~Coder_Audio_Mme_Aace_c(void)
{
}

bool  Coder_Audio_Mme_Aace_c::AreCurrentControlsAndMetadataSupported(const CoderAudioCurrentParameters_t *CurrentParams)
{
    bool FoundSamplingFrequency, FoundChannelMapping;
    enum eAccAcMode aAcMode;
    unsigned int GteBitrate = 0, LteBitrate = 0;

    /* Check parent class first */
    if (!Coder_Audio_Mme_c::AreCurrentControlsAndMetadataSupported(CurrentParams))
    {
        return false;
    }

    //Check Input Channel configuration
    {
        StmSeAudioChannelPlacementAnalysis_t Analysis;
        stm_se_audio_channel_placement_t SortedPlacement;
        bool AudioModeIsPhysical;

        if ((0 != StmSeAudioGetAcmodAndAnalysisFromChannelPlacement(&aAcMode, &AudioModeIsPhysical,
                                                                    &SortedPlacement, &Analysis,
                                                                    &CurrentParams->InputMetadata->audio.core_format.channel_placement))
            || (ACC_MODE_ID == aAcMode))
        {
            SE_ERROR("Channel Configuration does not map to eAccAcMode\n");
            return false;
        }
    }
    FoundSamplingFrequency = FoundChannelMapping = false;

    /* Loop till end of LUT, marked by 0 == Lut[ModeIdx].SampleRate */
    for (unsigned int ModeIdx = 0; 0 != gCoderAudioMmeAaceSupportedModesLut[ModeIdx].SampleRate ; ModeIdx++)
    {
        /* Checks for Frequency */
        unsigned int aFrequency = CurrentParams->InputMetadata->audio.core_format.sample_rate;

        if (aFrequency == gCoderAudioMmeAaceSupportedModesLut[ModeIdx].SampleRate)
        {
            FoundSamplingFrequency = true;

            if (aAcMode == gCoderAudioMmeAaceSupportedModesLut[ModeIdx].AudioCodingMode)
            {
                FoundChannelMapping = true;

                /* if not vbr, check for the bitrate */
                if (!CurrentParams->Controls.BitRateCtrl.is_vbr)
                {
                    /* Checks for bitrate */
                    unsigned int Bitrate = CurrentParams->Controls.BitRateCtrl.bitrate;
                    Bitrate = min(Bitrate, gCoderAudioMmeAaceSupportedModesLut[ModeIdx].MaxBitrate);
                    Bitrate = max(Bitrate, gCoderAudioMmeAaceSupportedModesLut[ModeIdx].MinBitrate);
                    GteBitrate = LteBitrate = Bitrate;
                }
            }
        }
    }

    if (!FoundSamplingFrequency)
    {
        SE_ERROR("Sampling Frequency %d is not supported\n", CurrentParams->InputMetadata->audio.core_format.sample_rate);
        return false;
    }

    if (!FoundChannelMapping)
    {
        SE_ERROR("Channel Configuration %s is not supported\n", StmSeAudioAcModeGetName(aAcMode));
        return false;
    }

    if (CurrentParams->Controls.BitRateCtrl.is_vbr)
    {
        if (100 < CurrentParams->Controls.BitRateCtrl.vbr_quality_factor)
        {
            SE_ERROR("vbr_quality_factor %d is more than max 100\n", CurrentParams->Controls.BitRateCtrl.vbr_quality_factor);
            return false;
        }
    }
    else if (GteBitrate != CurrentParams->Controls.BitRateCtrl.bitrate)
    {
        /* If enabled, use supported neighbor bit-rate */
        unsigned int ReplacementBitrate = 0;

        if (CODER_AUDIO_AUTO_BITRATE_FLAG == CODER_AUDIO_AUTO_BITRATE_LTE)
        {
            ReplacementBitrate = (0 != LteBitrate) ? LteBitrate : 0;
        }
        else if (CODER_AUDIO_AUTO_BITRATE_FLAG == CODER_AUDIO_AUTO_BITRATE_GTE)
        {
            ReplacementBitrate = (0 != GteBitrate) ? GteBitrate : 0;
        }

        if (0 != ReplacementBitrate)
        {
            SE_WARNING("Unsupported bitrate (%d) in this configuration, replaced by %d\n", CurrentParams->Controls.BitRateCtrl.bitrate, ReplacementBitrate);
            UpdateCurrentBitRate(ReplacementBitrate);
        }
        else
        {
            SE_ERROR("Unsupported bitrate (%d) in this configuration\n", CurrentParams->Controls.BitRateCtrl.bitrate);
            return false;
        }
    }

    return true;
}

void Coder_Audio_Mme_Aace_c::FillDefaultCodecSpecific(unsigned char CodecSpecificConfig[NB_ENCODER_CONFIG_ELEMENT])
{
    tMMEAaceConfig *MmeCodecSpecificOutConfig = (tMMEAaceConfig *)CodecSpecificConfig;
    memset(MmeCodecSpecificOutConfig, 0, sizeof(tMMEAaceConfig));
    MmeCodecSpecificOutConfig->SubType        = CODER_AUDIO_MME_AACE_DEFAULT_SUBTYPE;
    MmeCodecSpecificOutConfig->quantqual      = CODER_AUDIO_MME_AACE_DEFAULT_ENCODING_VBR_QUALITY;
    MmeCodecSpecificOutConfig->VbrOn          = CODER_AUDIO_MME_AACE_DEFAULT_ENCODING_VBR ? 1 : 0;
    MmeCodecSpecificOutConfig->SbrFlag        = 0;
    MmeCodecSpecificOutConfig->Reserved       = 0;
}

CoderStatus_t Coder_Audio_Mme_Aace_c::UpdateDynamicCodecContext(CoderAudioMmeCodecDynamicContext_t *Context, const CoderAudioCurrentParameters_t *CurrentParams)
{
    tMMEAaceConfig *MmeCodecSpecificOutConfig = (tMMEAaceConfig *)Context->CodecSpecificConfig;

    if (!CurrentParams->Controls.BitRateCtrl.is_vbr)
    {
        /* Check for SBR On depending on bitrate */
        {
            enum eAccAcMode aAcMode;
            /* Parameters have been checked before for errors */
            {
                StmSeAudioChannelPlacementAnalysis_t Analysis;
                stm_se_audio_channel_placement_t SortedPlacement;
                bool AudioModeIsPhysical;
                StmSeAudioGetAcmodAndAnalysisFromChannelPlacement(&aAcMode, &AudioModeIsPhysical,
                                                                  &SortedPlacement, &Analysis,
                                                                  &CurrentParams->InputMetadata->audio.core_format.channel_placement);
            }

            for (int ModeIdx = 0; 0 != gCoderAudioMmeAaceSupportedModesLut[ModeIdx].SampleRate; ModeIdx++)
            {
                unsigned int aFrequency = CurrentParams->InputMetadata->audio.core_format.sample_rate;

                if (aFrequency == gCoderAudioMmeAaceSupportedModesLut[ModeIdx].SampleRate)
                {
                    if (aAcMode == gCoderAudioMmeAaceSupportedModesLut[ModeIdx].AudioCodingMode)
                    {
                        MmeCodecSpecificOutConfig->SbrFlag =
                            (CurrentParams->Controls.BitRateCtrl.bitrate < gCoderAudioMmeAaceSupportedModesLut[ModeIdx].SbrLcTransitionBitrate) ? 1 : 0;
                    }
                }
            }
        }
        MmeCodecSpecificOutConfig->VbrOn          = 0;
        MmeCodecSpecificOutConfig->quantqual      = 100;
    }
    else
    {
        // @todo Does the driver need to set SBR_ON in case of VBR? How? (Bugzilla 27644)
        MmeCodecSpecificOutConfig->SbrFlag   = 0;
        MmeCodecSpecificOutConfig->VbrOn     = 1;
        MmeCodecSpecificOutConfig->quantqual = CurrentParams->Controls.BitRateCtrl.vbr_quality_factor;
    }

    if (1 == MmeCodecSpecificOutConfig->SbrFlag)
    {
        Context->NrSamplePerCodedFrame = CODER_AUDIO_MME_AACE_HEACC_SAMPLES_PER_FRAME;
    }
    else
    {
        Context->NrSamplePerCodedFrame = CODER_AUDIO_MME_AACE_LCACC_SAMPLES_PER_FRAME;
    }

    return CoderNoError;
}


void Coder_Audio_Mme_Aace_c::SetCurrentTransformContextSizesAndPointers()
{
    memset(mCurrentTransformContext, 0, sizeof(CoderAudioMmeAaceTransformContext_t));
    //First call parent
    Coder_Audio_Mme_c::SetCurrentTransformContextSizesAndPointers();
    //Then specifics
    CoderAudioMmeAaceTransformContext_t *CodecSpecificContext = (CoderAudioMmeAaceTransformContext_t *)mCurrentTransformContext;
    /* Only addressable by CodecSpecificContext */
    CodecSpecificContext->CoderAudioMmeTransformContext.CodedBuffers             = &CodecSpecificContext->CodedBuffersArray[0];
    CodecSpecificContext->CoderAudioMmeTransformContext.MMEDataBuffers_p         = &CodecSpecificContext->MMEDataBuffers_pArray[0];
    CodecSpecificContext->CoderAudioMmeTransformContext.MMEBuffers               = &CodecSpecificContext->MMEBuffersArray[0];
    CodecSpecificContext->CoderAudioMmeTransformContext.MMEScatterPages          = &CodecSpecificContext->MMEScatterPagesArray[0];
    CodecSpecificContext->CoderAudioMmeTransformContext.EncoderStatusParams_p    = (CoderAudioMmeAccessorAudioEncoderStatusGenericParams_t *)&CodecSpecificContext->CodecStatusParams;
    CodecSpecificContext->CodecStatusParams.CommonStatus.SpecificEncoderStatusBArraySize = mMmeStaticConfiguration.SpecificEncoderStatusBArraySize;
    CodecSpecificContext->CodecStatusParams.CommonStatus.StructSize                      = mMmeStaticConfiguration.SpecificEncoderStatusStructSize;
}


void Coder_Audio_Mme_Aace_c::DumpMmeGlobalParams(MME_AudioEncoderGlobalParams_t *GlobalParams)
{
    if (SE_IS_VERBOSE_ON(group_encoder_audio_coder))
    {
        SE_ASSERT(GlobalParams != NULL);
        /* Call Parent for common fields */
        Coder_Audio_Mme_c::DumpMmeGlobalParams(GlobalParams);
        tMMEAaceConfig       *MmeCodecSpecificOutConfig = (tMMEAaceConfig *)GlobalParams->OutConfig.Config;
        SE_VERBOSE(group_encoder_audio_coder, "  SubType   %08X\n", MmeCodecSpecificOutConfig->SubType);
        SE_VERBOSE(group_encoder_audio_coder, "  quantqual %lu\n" , MmeCodecSpecificOutConfig->quantqual);
        SE_VERBOSE(group_encoder_audio_coder, "  VbrOn     %01X\n", MmeCodecSpecificOutConfig->VbrOn    ? 1 : 0);
        SE_VERBOSE(group_encoder_audio_coder, "  SbrFlag   %01X\n", MmeCodecSpecificOutConfig->SbrFlag  ? 1 : 0);
    }
}
