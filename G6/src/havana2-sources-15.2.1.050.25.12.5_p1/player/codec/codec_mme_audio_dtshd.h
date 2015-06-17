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

#ifndef H_CODEC_MME_AUDIO_DTSHD
#define H_CODEC_MME_AUDIO_DTSHD

#include "codec_mme_audio.h"
#include "dtshd.h"

#undef TRACE_TAG
#define TRACE_TAG "Codec_MmeAudioDtshd_c"


typedef struct
{
    U32                             Id;                           //< Id of this processing structure: ACC_MIX_METADATA_ID
    U32                             StructSize;                   //< Size of this structure
    U16                             PrimaryAudioGain[ACC_MAIN_CSURR + 1];       //< unsigned Q3.13 gain to be applied to each channel of primary stream
    U16                             PostMixGain;                  //< unsigned Q3.13 gain to be applied to output of mixed primary and secondary
    U16                             NbOutMixConfig;               //< Number of mix output configurations
    MME_MixingOutputConfiguration_t MixOutConfig[MAX_MIXING_OUTPUT_CONFIGURATION]; //< This array is extensible according to NbOutMixConfig
} MME_LxAudioDecoderDtsMixingMetadata_t;

/// Calculate the apparent size of the structure when NbOutMixConfig is zero.
#define DTSHD_MIN_MIXING_METADATA_SIZE       (sizeof(MME_LxAudioDecoderDtsMixingMetadata_t) - (MAX_MIXING_OUTPUT_CONFIGURATION * sizeof(MME_MixingOutputConfiguration_t)))
#define DTSHD_MIN_MIXING_METADATA_FIXED_SIZE (DTSHD_MIN_MIXING_METADATA_SIZE - (2 * sizeof(U32))) // same as above minus Id and StructSize


typedef struct
{
    MME_LxAudioDecoderFrameStatus_t               DecStatus;
    MME_PcmProcessingFrameExtCommonStatus_t       PcmStatus;
    MME_LxAudioDecoderDtsMixingMetadata_t         MixingMetadata;
} MME_LxAudioDecoderFrameExtendedDtsStatus_t;


typedef struct DtshdAudioCodecDecodeContext_s
{
    CodecBaseDecodeContext_t                      BaseContext;

    MME_LxAudioDecoderFrameParams_t               DecodeParameters;
    MME_LxAudioDecoderFrameExtendedDtsStatus_t    DecodeStatus;
    unsigned int                                  TranscodeBufferIndex;
    DtshdAudioFrameParameters_t                   ContextFrameParameters;
} DtshdAudioCodecDecodeContext_t;

class Codec_MmeAudioDtshd_c : public Codec_MmeAudio_c
{
public:
    explicit Codec_MmeAudioDtshd_c(bool IsLbrStream);
    CodecStatus_t FinalizeInit(void);
    ~Codec_MmeAudioDtshd_c(void);

    //
    // Stream specific functions
    //
    static void     FillStreamMetadata(ParsedAudioParameters_t *AudioParameters, MME_LxAudioDecoderFrameStatus_t *Status);
    static bool     CapableOfTranscodeDtshdToDts(ParsedAudioParameters_t *ParsedAudioParameters);
    static void     TranscodeDtshdToDts(CodecBaseDecodeContext_t     *BaseContext,
                                        DtshdAudioFrameParameters_t *FrameParameters,
                                        AdditionalBufferState_t      *TranscodedBuffer);

    static void     GetCapabilities(stm_se_audio_dec_capability_t::audio_dec_dts_capability_s *cap,
                                    const MME_LxAudioDecoderHDInfo_t decInfo);

protected:
    eAccDecoderId   DecoderId;
    bool            IsLbrStream;

    CodecStatus_t   FillOutTransformerGlobalParameters(MME_LxAudioDecoderGlobalParams_t *GlobalParams);
    CodecStatus_t   FillOutTransformerInitializationParameters(void);
    CodecStatus_t   FillOutSetStreamParametersCommand(void);
    CodecStatus_t   FillOutDecodeCommand(void);
    CodecStatus_t   ValidateDecodeContext(CodecBaseDecodeContext_t *Context);
    void            HandleMixingMetadata(CodecBaseDecodeContext_t *Context,
                                         MME_PcmProcessingStatusTemplate_t *PcmStatus);
    CodecStatus_t   DumpSetStreamParameters(void    *Parameters);
    void            SetCommandIO(void);
    CodecStatus_t   DumpDecodeParameters(void   *Parameters);
};

#endif //H_CODEC_MME_AUDIO_DTSHD
