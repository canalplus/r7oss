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

#include "coder_audio_mme_mp3e.h"
#include "audio_conversions.h"

#undef TRACE_TAG
#define TRACE_TAG "Coder_Audio_Mme_Mp3e_c"

#define MP3E_MME_CONTEXT         "Mp3eMmeContext"
#define MP3E_MME_CONTEXT_TYPE    {MP3E_MME_CONTEXT, BufferDataTypeBase, AllocateFromOSMemory, 32, 0, true, true, sizeof(CoderAudioMmeMp3eTransformContext_t)}

static BufferDataDescriptor_t gMp3eMmeContextDescriptor = MP3E_MME_CONTEXT_TYPE;

static const CoderAudioStaticConfiguration_t gMp3eStaticConfig =
{
    STM_SE_ENCODE_STREAM_ENCODING_AUDIO_MP3,
    CODER_AUDIO_MME_MAX_NB_CHAN,
    CODER_AUDIO_MME_MAX_SMP_PER_INPUT_BUFFER,
    CODER_AUDIO_MME_MP3E_MAX_SAMPLES_PER_FRAME,
    CODER_AUDIO_MME_MP3E_MIN_SAMPLES_PER_FRAME,
    CODER_AUDIO_MME_MP3E_MAX_CODED_FRAME_SIZE
};

static const CoderBaseAudioMetadata_t gMp3eBaseAudioMetadata =
{
    CODER_AUDIO_MME_MP3E_DEFAULT_ENCODING_SAMPLING_FREQ,
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

static const CoderAudioControls_t gMp3eBaseControls =
{
    {
        CODER_AUDIO_MME_MP3E_DEFAULT_ENCODING_VBR,
        (uint32_t)CODER_AUDIO_DEFAULT_ENCODING_BITRATE, // will be refined
        (uint32_t)CODER_AUDIO_MME_MP3E_DEFAULT_ENCODING_VBR_QUALITY,
        (uint32_t)0,  // bitrate cap
    },
    false, // bitrate updated
    CODER_AUDIO_MME_MP3E_DEFAULT_ENCODING_LOUDNESS_LEVEL,
    CODER_AUDIO_MME_MP3E_DEFAULT_ENCODING_CRC,
    CODER_AUDIO_MME_MP3E_DEFAULT_ENCODING_COPYRIGHT,
    CODER_AUDIO_MME_MP3E_DEFAULT_ENCODING_ORIGINAL
};

static const CoderAudioMmeMp3eSupportedMode_t gCoderAudioMmeMp3eSupportedModesLut[] =
{
    {
        48000, ACC_MODE10,     10
        , { 64000,  80000,  96000, 112000, 128000, 160000, 192000, 224000, 256000, 320000,      0}
    },
    {
        48000, ACC_MODE20,     8
        , { 96000, 112000, 128000, 160000, 192000, 224000, 256000, 320000,      0}
    },

    {
        44100, ACC_MODE10,     10
        , { 64000,  80000,  96000, 112000, 128000, 160000, 192000, 224000, 256000, 320000,      0}
    },
    {
        44100, ACC_MODE20,     8
        , { 96000, 112000, 128000, 160000, 192000, 224000, 256000, 320000,      0}
    },

    {
        32000, ACC_MODE10,     10
        , { 64000,  80000,  96000, 112000, 128000, 160000, 192000, 224000, 256000, 320000,      0}
    },
    {
        32000, ACC_MODE20,     8
        , { 96000, 112000, 128000, 160000, 192000, 224000, 256000, 320000,      0}
    },

#if CODER_AUDIO_MME_MP3E_MP2_SUPPORTED
#warning CODER_AUDIO_MME_MP3E_MP2_SUPPORTED is enabled: gCoderAudioMmeMp3eSupportedModesLut must be checked
    {
        24000, ACC_MODE10,     14
        ,  { 8000,  16000,  24000,  32000,  40000,  48000,  56000,  64000,  80000,  96000, 112000, 128000, 144000, 160000,      0}
    },
    {
        24000, ACC_MODE20,     14
        ,  { 8000,  16000,  24000,  32000,  40000,  48000,  56000,  64000,  80000,  96000, 112000, 128000, 144000, 160000,      0}
    },

    {
        22050, ACC_MODE10,     14
        ,  { 8000,  16000,  24000,  32000,  40000,  48000,  56000,  64000,  80000,  96000, 112000, 128000, 144000, 160000,      0}
    },
    {
        22050, ACC_MODE20,     14
        ,  { 8000,  16000,  24000,  32000,  40000,  48000,  56000,  64000,  80000,  96000, 112000, 128000, 144000, 160000,      0}
    },

    {
        16000, ACC_MODE10,     14
        ,  { 8000,  16000,  24000,  32000,  40000,  48000,  56000,  64000,  80000,  96000, 112000, 128000, 144000, 160000,      0}
    },
    {
        16000, ACC_MODE20,     14
        ,  { 8000,  16000,  24000,  32000,  40000,  48000,  56000,  64000,  80000,  96000, 112000, 128000, 144000, 160000,      0}
    },

#if CODER_AUDIO_MME_MP3E_MP25_SUPPORTED
#warning CODER_AUDIO_MME_MP3E_MP25_SUPPORTED is enabled: gCoderAudioMmeMp3eSupportedModesLut must be checked
    {
        12000, ACC_MODE10,      8
        ,  { 8000,  16000,  24000,  32000,  40000,  48000,  56000,  64000,      0,      0,      0,      0,      0,      0,      0}
    },
    {
        12000, ACC_MODE20,      8
        ,  { 8000,  16000,  24000,  32000,  40000,  48000,  56000,  64000,      0,      0,      0,      0,      0,      0,      0}
    },

    {
        11025, ACC_MODE10,      8
        ,  { 8000,  16000,  24000,  32000,  40000,  48000,  56000,  64000,      0,      0,      0,      0,      0,      0,      0}
    },
    {
        11025, ACC_MODE20,      8
        ,  { 8000,  16000,  24000,  32000,  40000,  48000,  56000,  64000,      0,      0,      0,      0,      0,      0,      0}
    },

    {
        8000, ACC_MODE10,      8
        ,  { 8000,  16000,  24000,  32000,  40000,  48000,  56000,  64000,      0,      0,      0,      0,      0,      0,      0}
    },
    {
        8000, ACC_MODE20,      8
        ,  { 8000,  16000,  24000,  32000,  40000,  48000,  56000,  64000,      0,      0,      0,      0,      0,      0,      0}
    },
#endif
#endif

    /* Termination Line
     * parse till
     *    0 == gCoderAudioMmeMp3eSupportedModesLut[i].SampleRate
     * || 0 == gCoderAudioMmeMp3eSupportedModesLut[i].NumberOfSupportedRate
     * || AC_MODE_ID  == gCoderAudioMmeMp3eSupportedModesLut[i].AudioCodingMode
     * */
    {
        0,     ACC_MODE_ID,    0
        ,  {    0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0}
    }
};

static const CoderAudioMmeStaticConfiguration_t gMp3eMmeStaticConfig =
{
    ACC_GENERATE_ENCODERID(ACC_MP3ELC_ID),
    sizeof(tMMEMp3eLcConfig),
    sizeof(MME_Mp3eLcStatus_t),
    CODER_AUDIO_MME_MP3E_MAX_CODEC_INTRISIC_FRAME_DELAY,
    CODER_AUDIO_MME_MP3E_TRANSFORM_CONTEXT_POOL_DEPTH,
    sizeof(CoderAudioMmeMp3eTransformContext_t),
    CODER_AUDIO_MME_MP3E_MAX_OUT_MME_BUFFER_PER_TRANSFORM,
    1 + CODER_AUDIO_MME_MP3E_MAX_OUT_MME_BUFFER_PER_TRANSFORM,
    1 + CODER_AUDIO_MME_MP3E_MAX_OUT_MME_BUFFER_PER_TRANSFORM,
    sizeof(CoderAudioMmeMp3eSpecificEncoderBArray_t),
    sizeof(CoderAudioMmeMp3eStatusParams_t),
    false,
    false,
    false,
    &gMp3eMmeContextDescriptor,
    {""}
};

///////

Coder_Audio_Mme_Mp3e_c::Coder_Audio_Mme_Mp3e_c(void)
    : Coder_Audio_Mme_c(gMp3eStaticConfig, gMp3eBaseAudioMetadata, gMp3eBaseControls, gMp3eMmeStaticConfig)
{
    // update base class with codec specifics
    FillDefaultCodecSpecific(mMmeDynamicContext.CodecDynamicContext.CodecSpecificConfig);
}

Coder_Audio_Mme_Mp3e_c::~Coder_Audio_Mme_Mp3e_c(void)
{
}

bool  Coder_Audio_Mme_Mp3e_c::AreCurrentControlsAndMetadataSupported(const CoderAudioCurrentParameters_t *CurrentParams)
{
    bool FoundSamplingFrequency, FoundChannelMapping,  FoundBitrate;
    enum eAccAcMode aAcMode;
    unsigned int GteBitrate = 0, LteBitrate = 0; //closest bitrate neighbors

    /* Check parent class first */
    if (!Coder_Audio_Mme_c::AreCurrentControlsAndMetadataSupported(CurrentParams))
    {
        return false;
    }

    //Check Input Channel configuration
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

    FoundSamplingFrequency = FoundChannelMapping = FoundBitrate = false;

    /* Loop till end of LUT, marked by 0 == gCoderAudioMmeMp3eSupportedModesLut[ModeIdx].SampleRate */
    for (unsigned int ModeIdx = 0; 0 != gCoderAudioMmeMp3eSupportedModesLut[ModeIdx].SampleRate ; ModeIdx++)
    {
        /* Checks for Frequency */
        unsigned int aFrequency = CurrentParams->InputMetadata->audio.core_format.sample_rate;

        if (aFrequency == gCoderAudioMmeMp3eSupportedModesLut[ModeIdx].SampleRate)
        {
            FoundSamplingFrequency = true;

            if (aAcMode == gCoderAudioMmeMp3eSupportedModesLut[ModeIdx].AudioCodingMode)
            {
                FoundChannelMapping = true;

                /* if not vbr, check for the bitrate */
                if (!CurrentParams->Controls.BitRateCtrl.is_vbr)
                {
                    /* Checks for bitrate */
                    unsigned int aBitrate = CurrentParams->Controls.BitRateCtrl.bitrate;

                    for (unsigned int r = 0; r < gCoderAudioMmeMp3eSupportedModesLut[ModeIdx].NumberOfSupportedRates ; r++)
                    {
                        unsigned int Bitrate = gCoderAudioMmeMp3eSupportedModesLut[ModeIdx].Rates[r];

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

    if (CurrentParams->Controls.BitRateCtrl.is_vbr)
    {
        if (100 < CurrentParams->Controls.BitRateCtrl.vbr_quality_factor)
        {
            SE_ERROR("vbr_quality_factor %d is more than max 100\n", CurrentParams->Controls.BitRateCtrl.vbr_quality_factor);
            return false;
        }
    }
    else if (!FoundBitrate)
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

void Coder_Audio_Mme_Mp3e_c::FillDefaultCodecSpecific(unsigned char CodecSpecificConfig[NB_ENCODER_CONFIG_ELEMENT])
{
    tMMEMp3eLcConfig *MmeCodecSpecificOutConfig = (tMMEMp3eLcConfig *)CodecSpecificConfig;
    memset(MmeCodecSpecificOutConfig, 0, sizeof(tMMEMp3eLcConfig));
    MmeCodecSpecificOutConfig->bandWidth       =  CODER_AUDIO_MME_MP3E_DEFAULT_ENCODING_BANDWIDTH;
    MmeCodecSpecificOutConfig->fIntensity      =  CODER_AUDIO_MME_MP3E_DEFAULT_ENCODING_INTENSITY_OFF  ? ACC_MME_TRUE : ACC_MME_FALSE;
    MmeCodecSpecificOutConfig->fFullHuffman    =  CODER_AUDIO_MME_MP3E_RESERVED_ENCODING_FULL_HUFFMAN  ? ACC_MME_TRUE : ACC_MME_FALSE;
    MmeCodecSpecificOutConfig->paddingMode     =  CODER_AUDIO_MME_MP3E_RESERVED_ENCODING_PADDING_MODE  ? ACC_MME_TRUE : ACC_MME_FALSE;
    MmeCodecSpecificOutConfig->privateBit      =  CODER_AUDIO_MME_MP3E_RESERVED_ENCODING_PRIVATE       ? ACC_MME_TRUE : ACC_MME_FALSE;
    MmeCodecSpecificOutConfig->EmphasisFlag    =  CODER_AUDIO_MME_MP3E_RESERVED_ENCODING_EMPHASIS      ? ACC_MME_TRUE : ACC_MME_FALSE;
    MmeCodecSpecificOutConfig->downmix         =  CODER_AUDIO_MME_MP3E_RESERVED_ENCODING_DOWNMIX       ? ACC_MME_TRUE : ACC_MME_FALSE;
    MmeCodecSpecificOutConfig->fVbrMode        =  CODER_AUDIO_MME_MP3E_DEFAULT_ENCODING_VBR            ? ACC_MME_TRUE : ACC_MME_FALSE;
    MmeCodecSpecificOutConfig->vbrQuality      =  CODER_AUDIO_MME_MP3E_DEFAULT_ENCODING_VBR            ? CODER_AUDIO_MME_MP3E_DEFAULT_ENCODING_VBR_QUALITY : 0;
    MmeCodecSpecificOutConfig->fCrc            =  CODER_AUDIO_MME_MP3E_DEFAULT_ENCODING_CRC            ? ACC_MME_TRUE : ACC_MME_FALSE;
    MmeCodecSpecificOutConfig->copyRightBit    =  CODER_AUDIO_MME_MP3E_DEFAULT_ENCODING_COPYRIGHT      ? ACC_MME_TRUE : ACC_MME_FALSE;
    MmeCodecSpecificOutConfig->originalCopyBit =  CODER_AUDIO_MME_MP3E_DEFAULT_ENCODING_ORIGINAL       ? ACC_MME_TRUE : ACC_MME_FALSE;
}

CoderStatus_t Coder_Audio_Mme_Mp3e_c::UpdateDynamicCodecContext(CoderAudioMmeCodecDynamicContext_t *Context,
                                                                const CoderAudioCurrentParameters_t *CurrentParams)
{
    tMMEMp3eLcConfig       *MmeCodecSpecificOutConfig = (tMMEMp3eLcConfig *)Context->CodecSpecificConfig;
    //@todo Update with actual data from standard;
    Context->MaxCodedFrameSize     = CODER_AUDIO_MME_MP3E_MAX_CODED_FRAME_SIZE;

    if (CurrentParams->InputMetadata->audio.core_format.sample_rate >= 32000)
    {
        Context->NrSamplePerCodedFrame = CODER_AUDIO_MME_MP3E_MP1_SAMPLES_PER_FRAME;
    }
    else
    {
        Context->NrSamplePerCodedFrame = CODER_AUDIO_MME_MP3E_MP2_MP25_SAMPLES_PER_FRAME;
    }

    /*Intensity mode on for all modes except 1p1*/
    enum eAccAcMode aAcMode;
    StmSeAudioChannelPlacementAnalysis_t Analysis;
    stm_se_audio_channel_placement_t SortedPlacement;
    bool AudioModeIsPhysical;
    StmSeAudioGetAcmodAndAnalysisFromChannelPlacement(&aAcMode, &AudioModeIsPhysical,
                                                      &SortedPlacement, &Analysis,
                                                      &CurrentParams->InputMetadata->audio.core_format.channel_placement);

    if (ACC_MODE_1p1 == aAcMode)
    {
        MmeCodecSpecificOutConfig->fIntensity =  ACC_MME_TRUE;
    }
    else
    {
        MmeCodecSpecificOutConfig->fIntensity =  ACC_MME_FALSE;
    }

    MmeCodecSpecificOutConfig->fVbrMode        =  CurrentParams->Controls.BitRateCtrl.is_vbr      ? ACC_MME_TRUE : ACC_MME_FALSE;
    MmeCodecSpecificOutConfig->fCrc            =  CurrentParams->Controls.CrcOn               ? ACC_MME_TRUE : ACC_MME_FALSE;
    MmeCodecSpecificOutConfig->copyRightBit    =  CurrentParams->Controls.StreamIsCopyrighted ? ACC_MME_TRUE : ACC_MME_FALSE;
    MmeCodecSpecificOutConfig->originalCopyBit =  CurrentParams->Controls.OneCopyIsAuthorized ? ACC_MME_TRUE : ACC_MME_FALSE;
    MmeCodecSpecificOutConfig->vbrQuality      =  CurrentParams->Controls.BitRateCtrl.vbr_quality_factor;

    return CoderNoError;
}

void Coder_Audio_Mme_Mp3e_c::SetCurrentTransformContextSizesAndPointers()
{
    memset(mCurrentTransformContext, 0, sizeof(CoderAudioMmeMp3eTransformContext_t));
    //First call parent
    Coder_Audio_Mme_c::SetCurrentTransformContextSizesAndPointers();
    //Then specifics
    CoderAudioMmeMp3eTransformContext_t *CodecSpecificContext = (CoderAudioMmeMp3eTransformContext_t *)mCurrentTransformContext;
    /* Only addressable by CodecSpecificContext */
    CodecSpecificContext->CodecStatusParams.CommonStatus.SpecificEncoderStatusBArraySize = mMmeStaticConfiguration.SpecificEncoderStatusBArraySize;
    CodecSpecificContext->CodecStatusParams.CommonStatus.StructSize                      = mMmeStaticConfiguration.SpecificEncoderStatusStructSize;
    CodecSpecificContext->CoderAudioMmeTransformContext.CodedBuffers             = &CodecSpecificContext->CodedBuffersArray[0];
    CodecSpecificContext->CoderAudioMmeTransformContext.MMEDataBuffers_p         = &CodecSpecificContext->MMEDataBuffers_pArray[0];
    CodecSpecificContext->CoderAudioMmeTransformContext.MMEBuffers               = &CodecSpecificContext->MMEBuffersArray[0];
    CodecSpecificContext->CoderAudioMmeTransformContext.MMEScatterPages          = &CodecSpecificContext->MMEScatterPagesArray[0];
    CodecSpecificContext->CoderAudioMmeTransformContext.EncoderStatusParams_p    = (CoderAudioMmeAccessorAudioEncoderStatusGenericParams_t *)&CodecSpecificContext->CodecStatusParams;
}

void Coder_Audio_Mme_Mp3e_c::AnalyseCurrentVsPreviousParameters(CoderAudioMmeParameterUpdateAnalysis_t *Analysis)
{
    Analysis->Res.ChangeDetected     =  false;
    Analysis->Res.RestartTransformer =  false;
    Analysis->Res.InputConfig        =  false;
    Analysis->Res.OutputConfig       =  false;
    Analysis->Res.CodecConfig        =  false;

    /* Check Parent Class */
    Coder_Audio_Mme_c::AnalyseCurrentVsPreviousParameters(Analysis);

    /* Class Specific Checks */
    if ((Analysis->Prev.Controls->BitRateCtrl.is_vbr != Analysis->Cur.Controls->BitRateCtrl.is_vbr))
    {
        SE_INFO(group_encoder_audio_coder, "Output bitrate control change detected, requires restart\n");
        Analysis->Res.OutputConfig = true;
        Analysis->Res.RestartTransformer = true;/* @note this is required for MP3 see bug 25157 */
    }
    else
    {
        //@note nalysis->Cur.Controls->BitRateCtrl.bitrate_capis not supported by mp3e no need to check it
        if (!Analysis->Cur.Controls->BitRateCtrl.is_vbr)
        {
            if (Analysis->Prev.Controls->BitRateCtrl.bitrate != Analysis->Cur.Controls->BitRateCtrl.bitrate)
            {
                SE_INFO(group_encoder_audio_coder, "Output bitrate control change detected, requires restart\n");
                Analysis->Res.OutputConfig = true;
                Analysis->Res.RestartTransformer = true;/* @note this is required for MP3 see bug 25157 */
            }
        }
        else
        {
            if (Analysis->Prev.Controls->BitRateCtrl.vbr_quality_factor != Analysis->Cur.Controls->BitRateCtrl.vbr_quality_factor)
            {
                SE_INFO(group_encoder_audio_coder, "Output bitrate control change detected, requires restart\n");
                Analysis->Res.OutputConfig = true;
                Analysis->Res.RestartTransformer = true;/* @note this is required for MP3 see bug 25157 */
            }
        }
    }

    /* Codec Specific parameters check */
    /* Previous Config is in the mme init param Global params */
    tMMEMp3eLcConfig  *PrevMmeCodecSpecificOutConfig = (tMMEMp3eLcConfig *)Analysis->Prev.MmeContext->CodecDynamicContext.CodecSpecificConfig;
    tMMEMp3eLcConfig  *CurMmeCodecSpecificOutConfig         = (tMMEMp3eLcConfig *)Analysis->Cur.MmeContext->CodecDynamicContext.CodecSpecificConfig;

    if ((CurMmeCodecSpecificOutConfig->fIntensity      != PrevMmeCodecSpecificOutConfig->fIntensity)
        || (CurMmeCodecSpecificOutConfig->fVbrMode        != PrevMmeCodecSpecificOutConfig->fVbrMode)
        || (CurMmeCodecSpecificOutConfig->vbrQuality      != PrevMmeCodecSpecificOutConfig->vbrQuality)
        || (CurMmeCodecSpecificOutConfig->fCrc            != PrevMmeCodecSpecificOutConfig->fCrc)
        || (CurMmeCodecSpecificOutConfig->copyRightBit    != PrevMmeCodecSpecificOutConfig->copyRightBit)
        || (CurMmeCodecSpecificOutConfig->originalCopyBit != PrevMmeCodecSpecificOutConfig->originalCopyBit)
       )
    {
        SE_DEBUG(group_encoder_audio_coder, "Detected a change in MP3 specific parameters\n");
        Analysis->Res.CodecConfig = true;
    }

    Analysis->Res.ChangeDetected =  Analysis->Res.RestartTransformer
                                    || Analysis->Res.InputConfig
                                    || Analysis->Res.OutputConfig
                                    || Analysis->Res.CodecConfig;
}

void Coder_Audio_Mme_Mp3e_c::DumpMmeGlobalParams(MME_AudioEncoderGlobalParams_t *GlobalParams)
{
    if (SE_IS_VERBOSE_ON(group_encoder_audio_coder))
    {
        SE_ASSERT(GlobalParams != NULL);
        /* Call Parent for common fields */
        Coder_Audio_Mme_c::DumpMmeGlobalParams(GlobalParams);
        tMMEMp3eLcConfig       *MmeCodecSpecificOutConfig = (tMMEMp3eLcConfig *)GlobalParams->OutConfig.Config;
        SE_VERBOSE(group_encoder_audio_coder, "bandWidth       %ld\n" , MmeCodecSpecificOutConfig->bandWidth);
        SE_VERBOSE(group_encoder_audio_coder, "fIntensity      %01X\n", MmeCodecSpecificOutConfig->fIntensity      ? 1 : 0);
        SE_VERBOSE(group_encoder_audio_coder, "fFullHuffman    %01X\n", MmeCodecSpecificOutConfig->fFullHuffman    ? 1 : 0);
        SE_VERBOSE(group_encoder_audio_coder, "paddingMode     %01X\n", MmeCodecSpecificOutConfig->paddingMode     ? 1 : 0);
        SE_VERBOSE(group_encoder_audio_coder, "privateBit      %01X\n", MmeCodecSpecificOutConfig->privateBit      ? 1 : 0);
        SE_VERBOSE(group_encoder_audio_coder, "EmphasisFlag    %01X\n", MmeCodecSpecificOutConfig->EmphasisFlag    ? 1 : 0);
        SE_VERBOSE(group_encoder_audio_coder, "downmix         %01X\n", MmeCodecSpecificOutConfig->downmix         ? 1 : 0);
        SE_VERBOSE(group_encoder_audio_coder, "fVbrMode        %01X\n", MmeCodecSpecificOutConfig->fVbrMode        ? 1 : 0);
        SE_VERBOSE(group_encoder_audio_coder, "vbrQuality      %d\n"  , MmeCodecSpecificOutConfig->vbrQuality);
        SE_VERBOSE(group_encoder_audio_coder, "fCrc            %01X\n", MmeCodecSpecificOutConfig->fCrc            ? 1 : 0);
        SE_VERBOSE(group_encoder_audio_coder, "copyRightBit    %01X\n", MmeCodecSpecificOutConfig->copyRightBit    ? 1 : 0);
        SE_VERBOSE(group_encoder_audio_coder, "originalCopyBit %01X\n", MmeCodecSpecificOutConfig->originalCopyBit ? 1 : 0);
    }
}
