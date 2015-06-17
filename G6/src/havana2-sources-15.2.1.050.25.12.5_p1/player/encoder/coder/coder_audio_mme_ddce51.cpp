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

#include "coder_audio_mme_ddce51.h"
#include "audio_conversions.h"

#undef TRACE_TAG
#define TRACE_TAG "Coder_Audio_Mme_Ddce51_c"

#define DDCE51_MME_CONTEXT         "Ddce51MmeContext"
#define DDCE51_MME_CONTEXT_TYPE    {DDCE51_MME_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(CoderAudioMmeDdce51TransformContext_t)}

static BufferDataDescriptor_t gDdce51MmeContextDescriptor = DDCE51_MME_CONTEXT_TYPE;

static const CoderAudioStaticConfiguration_t gDdce51StaticConfig =
{
    STM_SE_ENCODE_STREAM_ENCODING_AUDIO_AC3,
    CODER_AUDIO_MME_MAX_NB_CHAN,
    CODER_AUDIO_MME_MAX_SMP_PER_INPUT_BUFFER,
    CODER_AUDIO_MME_DDCE51_MAX_SAMPLES_PER_FRAME,
    CODER_AUDIO_MME_DDCE51_MIN_SAMPLES_PER_FRAME,
    CODER_AUDIO_MME_DDCE51_MAX_CODED_FRAME_SIZE
};

static const CoderBaseAudioMetadata_t gDdce51BaseAudioMetadata =
{
    CODER_AUDIO_MME_DDCE51_DEFAULT_ENCODING_SAMPLING_FREQ,
    CODER_AUDIO_MME_MAX_NB_CHAN,  // max channel count; expected to be <= 8
    {
#if 1 //Stereo
        STM_SE_AUDIO_CHAN_L,
        STM_SE_AUDIO_CHAN_R,
        STM_SE_AUDIO_CHAN_STUFFING,
        STM_SE_AUDIO_CHAN_STUFFING,
        STM_SE_AUDIO_CHAN_STUFFING,
        STM_SE_AUDIO_CHAN_STUFFING,
        STM_SE_AUDIO_CHAN_STUFFING,
        STM_SE_AUDIO_CHAN_STUFFING,
#else //5.1
        STM_SE_AUDIO_CHAN_L,
        STM_SE_AUDIO_CHAN_R,
        STM_SE_AUDIO_CHAN_LFE,
        STM_SE_AUDIO_CHAN_C,
        STM_SE_AUDIO_CHAN_LS,
        STM_SE_AUDIO_CHAN_RS,
        STM_SE_AUDIO_CHAN_STUFFING,
        STM_SE_AUDIO_CHAN_STUFFING
#endif
    }
};

static const CoderAudioControls_t gDdce51BaseControls =
{
    {
        false,  // vbr not supported
        (uint32_t)CODER_AUDIO_DEFAULT_ENCODING_BITRATE, // will be refined
        (uint32_t)0,  // vbr quality n.a.
        (uint32_t)0,  // bitrate cap n.a.
    },
    false, // bitrate updated
    CODER_AUDIO_MME_DDCE51_DEFAULT_ENCODING_LOUDNESS_LEVEL,
    false, // crc not supported
    false, // SCMS not supported
    false  // SCMS not supported
};

static const CoderAudioMmeDdce51SupportedMode_t gCoderAudioMmeDdce51SupportedModesLut[] =
{
    {48000, ACC_MODE10,     3, { 64000,  96000, 128000,   0000}},
    {48000, ACC_MODE_1p1,   4, {128000, 192000, 256000, 384000}},
    {48000, ACC_MODE20,     4, {128000, 192000, 256000, 384000}},
    {48000, ACC_MODE32,     3, {384000, 448000, 640000,   0000}},
    {48000, ACC_MODE32_LFE, 3, {384000, 448000, 640000,   0000}},

    {0,     ACC_MODE_ID,    0, {  0000,   0000,   0000,   0000}}
};

static const CoderAudioMmeStaticConfiguration_t gDdce51MmeStaticConfig =
{
    ACC_GENERATE_ENCODERID(ACC_DDCE51_ID),
    sizeof(tMMEDdce51Config),
    sizeof(MME_Ddce51Status_t),
    CODER_AUDIO_MME_DDCE51_MAX_CODEC_INTRISIC_FRAME_DELAY,
    CODER_AUDIO_MME_DDCE51_TRANSFORM_CONTEXT_POOL_DEPTH,
    sizeof(CoderAudioMmeDdce51TransformContext_t),
    CODER_AUDIO_MME_DDCE51_MAX_OUT_MME_BUFFER_PER_TRANSFORM,
    1 + CODER_AUDIO_MME_DDCE51_MAX_OUT_MME_BUFFER_PER_TRANSFORM,
    1 + CODER_AUDIO_MME_DDCE51_MAX_OUT_MME_BUFFER_PER_TRANSFORM,
    sizeof(CoderAudioMmeDdce51SpecificEncoderBArray_t),
    sizeof(CoderAudioMmeDdce51StatusParams_t),
    CODER_AUDIO_MME_DDCE51_SWP1,
    CODER_AUDIO_MME_DDCE51_SWP2,
    CODER_AUDIO_MME_DDCE51_SWP3,
    &gDdce51MmeContextDescriptor,
    {""}
};

///////

Coder_Audio_Mme_Ddce51_c::Coder_Audio_Mme_Ddce51_c()
    : Coder_Audio_Mme_c(gDdce51StaticConfig, gDdce51BaseAudioMetadata, gDdce51BaseControls, gDdce51MmeStaticConfig)
{
    // update base class with codec specifics
    FillDefaultCodecSpecific(mMmeDynamicContext.CodecDynamicContext.CodecSpecificConfig);
}

Coder_Audio_Mme_Ddce51_c::~Coder_Audio_Mme_Ddce51_c(void)
{
}

bool  Coder_Audio_Mme_Ddce51_c::AreCurrentControlsAndMetadataSupported(const CoderAudioCurrentParameters_t *CurrentParams)
{
    bool FoundSamplingFrequency, FoundChannelMapping,  FoundBitrate;
    enum eAccAcMode aAcMode;
    unsigned int GteBitrate = 0, LteBitrate = 0;

    /* Check parent class first */
    if (!Coder_Audio_Mme_c::AreCurrentControlsAndMetadataSupported(CurrentParams))
    {
        return false;
    }

    //Check Input Channel configuration
    StmSeAudioChannelPlacementAnalysis_t Analysis;
    stm_se_audio_channel_placement_t SortedPlacement;
    const stm_se_audio_channel_placement_t *channel_placement = &CurrentParams->InputMetadata->audio.core_format.channel_placement;
    bool AudioModeIsPhysical;

    if ((0 != StmSeAudioGetAcmodAndAnalysisFromChannelPlacement(&aAcMode, &AudioModeIsPhysical,
                                                                &SortedPlacement, &Analysis,
                                                                channel_placement))
        || (ACC_MODE_ID == aAcMode))
    {
        SE_ERROR("Channel Configuration does not map to eAccAcMode\n");
        return false;
    }

    if (SE_IS_DEBUG_ON(group_encoder_audio_coder))
    {
        SE_DEBUG(group_encoder_audio_coder, "AcMode is %s\n", StmSeAudioAcModeGetName(aAcMode));

        for (int i = 0; i < channel_placement->channel_count; i++)
        {
            SE_DEBUG(group_encoder_audio_coder, "chan[%d] = %s\n", i,
                     StmSeAudioChannelIdGetName((enum stm_se_audio_channel_id_t)channel_placement->chan[i]));
        }
    }

    /* Checks for bitrate */
    if (CurrentParams->Controls.BitRateCtrl.is_vbr)
    {
        SE_ERROR("Ddce51 does not support vbr\n");
        return false;
    }

    FoundSamplingFrequency = FoundChannelMapping =  FoundBitrate = false;

    /* Loop till end of LUT, marked by 0 == Lut[ModeIdx].SampleRate */
    for (unsigned int ModeIdx = 0; 0 != gCoderAudioMmeDdce51SupportedModesLut[ModeIdx].SampleRate ; ModeIdx++)
    {
        /* Checks for Frequency */
        unsigned int aFrequency = CurrentParams->InputMetadata->audio.core_format.sample_rate;

        if (aFrequency == gCoderAudioMmeDdce51SupportedModesLut[ModeIdx].SampleRate)
        {
            FoundSamplingFrequency = true;

            if (aAcMode == gCoderAudioMmeDdce51SupportedModesLut[ModeIdx].AudioCodingMode)
            {
                FoundChannelMapping = true;
                {
                    unsigned int aBitrate = CurrentParams->Controls.BitRateCtrl.bitrate;

                    for (unsigned int r = 0; r < gCoderAudioMmeDdce51SupportedModesLut[ModeIdx].NumberOfSupportedRates ; r++)
                    {
                        unsigned int Bitrate = gCoderAudioMmeDdce51SupportedModesLut[ModeIdx].Rates[r];

                        if (Bitrate == aBitrate)
                        {
                            FoundBitrate = true;
                            LteBitrate = GteBitrate = aBitrate;
                            break;
                        }

                        if (Bitrate > aBitrate)
                        {
                            GteBitrate = Bitrate;
                            break;
                        }

                        LteBitrate = min(Bitrate, aBitrate);
                    }
                }
                break;
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
        SE_ERROR("Channel Configuration is not supported\n");
        return false;
    }

    if (!FoundBitrate)
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
            SE_WARNING("Unsupported bitrate (%d) in this configuration, replaced by %d\n",
                       CurrentParams->Controls.BitRateCtrl.bitrate, ReplacementBitrate);
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

void Coder_Audio_Mme_Ddce51_c::FillDefaultCodecSpecific(unsigned char CodecSpecificConfig[NB_ENCODER_CONFIG_ELEMENT])
{
    tMMEDdce51Config *MmeCodecSpecificOutConfig = (tMMEDdce51Config *)CodecSpecificConfig;
    memset(MmeCodecSpecificOutConfig, 0, sizeof(tMMEDdce51Config));
}

CoderStatus_t Coder_Audio_Mme_Ddce51_c::UpdateDynamicCodecContext(CoderAudioMmeCodecDynamicContext_t *Context,
                                                                  const CoderAudioCurrentParameters_t *CurrentParams)
{
    //@todo Update with actual data from standard;
    Context->MaxCodedFrameSize     = CODER_AUDIO_MME_DDCE51_MAX_CODED_FRAME_SIZE;
    Context->NrSamplePerCodedFrame = CODER_AUDIO_MME_DDCE51_MAX_SAMPLES_PER_FRAME;
    return CoderNoError;
}

void Coder_Audio_Mme_Ddce51_c::SetCurrentTransformContextSizesAndPointers()
{
    memset(mCurrentTransformContext, 0, sizeof(CoderAudioMmeDdce51TransformContext_t));
    //First call parent
    Coder_Audio_Mme_c::SetCurrentTransformContextSizesAndPointers();
    //Then specifics
    CoderAudioMmeDdce51TransformContext_t *CodecSpecificContext = (CoderAudioMmeDdce51TransformContext_t *)mCurrentTransformContext;
    /* Only addressable by CodecSpecificContext */
    CodecSpecificContext->CodecStatusParams.CommonStatus.SpecificEncoderStatusBArraySize = mMmeStaticConfiguration.SpecificEncoderStatusBArraySize;
    CodecSpecificContext->CodecStatusParams.CommonStatus.StructSize                      = mMmeStaticConfiguration.SpecificEncoderStatusStructSize;
    CodecSpecificContext->CoderAudioMmeTransformContext.CodedBuffers             = &CodecSpecificContext->CodedBuffersArray[0];
    CodecSpecificContext->CoderAudioMmeTransformContext.MMEDataBuffers_p         = &CodecSpecificContext->MMEDataBuffers_pArray[0];
    CodecSpecificContext->CoderAudioMmeTransformContext.MMEBuffers               = &CodecSpecificContext->MMEBuffersArray[0];
    CodecSpecificContext->CoderAudioMmeTransformContext.MMEScatterPages          = &CodecSpecificContext->MMEScatterPagesArray[0];
    CodecSpecificContext->CoderAudioMmeTransformContext.EncoderStatusParams_p    = (CoderAudioMmeAccessorAudioEncoderStatusGenericParams_t *)&CodecSpecificContext->CodecStatusParams;
}

void Coder_Audio_Mme_Ddce51_c::DumpMmeGlobalParams(MME_AudioEncoderGlobalParams_t *GlobalParams)
{
    if (SE_IS_VERBOSE_ON(group_encoder_audio_coder))
    {
        SE_ASSERT(GlobalParams != NULL);
        // First Call Parent
        Coder_Audio_Mme_c::DumpMmeGlobalParams(GlobalParams);
        //Then Specifics
        tMMEDdce51Config       *MmeCodecSpecificOutConfig = (tMMEDdce51Config *)GlobalParams->OutConfig.Config;
        SE_VERBOSE(group_encoder_audio_coder, "CompOn            %01X\n", MmeCodecSpecificOutConfig->CompOn           == 0 ? 0 : 1);
        SE_VERBOSE(group_encoder_audio_coder, "CompOnSec         %01X\n", MmeCodecSpecificOutConfig->CompOnSec        == 0 ? 0 : 1);
        SE_VERBOSE(group_encoder_audio_coder, "TestMode          %01X\n", MmeCodecSpecificOutConfig->TestMode         == 0 ? 0 : 1);
        SE_VERBOSE(group_encoder_audio_coder, "LfeActivate       %01X\n", MmeCodecSpecificOutConfig->LfeActivate      == 0 ? 0 : 1);
        SE_VERBOSE(group_encoder_audio_coder, "LfeLpActivate     %01X\n", MmeCodecSpecificOutConfig->LfeLpActivate    == 0 ? 0 : 1);
        SE_VERBOSE(group_encoder_audio_coder, "SurDelayActivate  %01X\n", MmeCodecSpecificOutConfig->SurDelayActivate == 0 ? 0 : 1);
        SE_VERBOSE(group_encoder_audio_coder, "BavariaOn         %01X\n", MmeCodecSpecificOutConfig->BavariaOn        == 0 ? 0 : 1);
        SE_VERBOSE(group_encoder_audio_coder, "Ac3Mode           %d\n"  , MmeCodecSpecificOutConfig->Ac3Mode);
        SE_VERBOSE(group_encoder_audio_coder, "ValidMeta         %d\n"  , MmeCodecSpecificOutConfig->Metadata.ValidMeta);
        SE_VERBOSE(group_encoder_audio_coder, "CMixLev           %d\n"  , MmeCodecSpecificOutConfig->Metadata.CMixLev);
        SE_VERBOSE(group_encoder_audio_coder, "SurMixLev         %d\n"  , MmeCodecSpecificOutConfig->Metadata.SurMixLev);
        SE_VERBOSE(group_encoder_audio_coder, "DSurMod           %d\n"  , MmeCodecSpecificOutConfig->Metadata.DSurMod);
        SE_VERBOSE(group_encoder_audio_coder, "ComprFlag         %d\n"  , MmeCodecSpecificOutConfig->Metadata.ComprFlag);
        SE_VERBOSE(group_encoder_audio_coder, "DialNorm0         %d\n"  , MmeCodecSpecificOutConfig->Metadata.DialNorm0);
        SE_VERBOSE(group_encoder_audio_coder, "DialNorm1         %d\n"  , MmeCodecSpecificOutConfig->Metadata.DialNorm1);
        SE_VERBOSE(group_encoder_audio_coder, "DynRangFlag       %d\n"  , MmeCodecSpecificOutConfig->Metadata.DynRangFlag);
        SE_VERBOSE(group_encoder_audio_coder, "Reserved1         %d\n"  , MmeCodecSpecificOutConfig->Metadata.Reserved1);
        SE_VERBOSE(group_encoder_audio_coder, "Compr0            %d\n"  , MmeCodecSpecificOutConfig->Metadata.Compr0);
        SE_VERBOSE(group_encoder_audio_coder, "Compr1            %d\n"  , MmeCodecSpecificOutConfig->Metadata.Compr1);

        for (unsigned int j = 0; j < 2; j++)
        {
            for (unsigned int i = 0 ; i < ACC_BAVERIA_NBLOCKS ; i++)
            {
                SE_VERBOSE(group_encoder_audio_coder, "DRC[%u][%u]=%02X", j, i, MmeCodecSpecificOutConfig->Metadata.DRC[j][i]);
            }
        }
    }
}
