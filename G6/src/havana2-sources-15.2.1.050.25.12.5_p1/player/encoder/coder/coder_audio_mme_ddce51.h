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
#ifndef CODER_AUDIO_MME_DDCE51_H
#define CODER_AUDIO_MME_DDCE51_H

#include "coder_audio_mme.h"

#include <ACC_Transformers/DDCE51_EncoderTypes.h>

#define CODER_AUDIO_MME_DDCE51_MAX_SAMPLES_PER_FRAME        1536
#define CODER_AUDIO_MME_DDCE51_MIN_SAMPLES_PER_FRAME        1536
#define CODER_AUDIO_MME_DDCE51_MAX_CODED_FRAME_SIZE         2560 // due to limited to 48kHz
#define CODER_AUDIO_MME_DDCE51_MAX_CODEC_INTRISIC_FRAME_DELAY   1    //256 samples delay

#define CODER_AUDIO_MME_DDCE51_MAX_OUT_MME_BUFFER_PER_TRANSFORM (CODER_AUDIO_MME_DDCE51_MAX_CODEC_INTRISIC_FRAME_DELAY \
                                 + CODER_AUDIO_UPWARD_DIV(CODER_AUDIO_MME_MAX_SMP_PER_INPUT_BUFFER, CODER_AUDIO_MME_DDCE51_MIN_SAMPLES_PER_FRAME))

#define CODER_AUDIO_MME_DDCE51_TRANSFORM_CONTEXT_POOL_DEPTH     (CODER_AUDIO_MME_DEF_TRANSFORM_CONTEXT_POOL_DEPTH)

/* Default parameters */
#define CODER_AUDIO_MME_DDCE51_DEFAULT_ENCODING_SAMPLING_FREQ    48000
#define CODER_AUDIO_MME_DDCE51_DEFAULT_ENCODING_LOUDNESS_LEVEL   (-3100)

/* Encoding Constants */
#define CODER_AUDIO_MME_DDCE51_SWP1 (false)
#define CODER_AUDIO_MME_DDCE51_SWP2 (false)
#define CODER_AUDIO_MME_DDCE51_SWP3 (true)

#define CODER_AUDIO_MME_DDCE51_MAX_RATES_PER_MODE 4

typedef struct CoderAudioMmeDdce51SupportedMode_s
{
    unsigned int    SampleRate;
    enum eAccAcMode AudioCodingMode;
    unsigned int    NumberOfSupportedRates;
    unsigned int    Rates[CODER_AUDIO_MME_DDCE51_MAX_RATES_PER_MODE];
} CoderAudioMmeDdce51SupportedMode_t;

/* For static size definition */
typedef struct
{
    CoderAudioMmeAccessorAudEncOutConfigCommon_t Common;
    union
    {
        U8                Config[NB_ENCODER_CONFIG_ELEMENT];  //!< Codec Specific encoder configuration
        tMMEDdce51Config  SpecificConfig;
    };
} CoderAudiMmeDdce51OutConfig_t;

/* For static size definition */
typedef struct
{
    MME_AudioEncoderFrameBufferStatus_t FrameStatus[CODER_AUDIO_MME_DDCE51_MAX_OUT_MME_BUFFER_PER_TRANSFORM];
    unsigned int            SpecificStatusSize;  //!< Cannot be used to access member, only here for size
    MME_Ddce51Status_t      SpecificStatus;      //!< Cannot be used to access member, only here for size
} CoderAudioMmeDdce51SpecificEncoderBArray_t;

/* For static size definition*/
typedef struct
{
    CoderAudioMmeAccessorAudioEncoderStatusCommonParams_t CommonStatus;
    union
    {
        unsigned char SpecificEncoderStatusBArray[MAX_NB_ENCODER_TRANSFORM_RETURN_ELEMENT];
        CoderAudioMmeDdce51SpecificEncoderBArray_t ActualSpecificEncoderBArray;
    };
} CoderAudioMmeDdce51StatusParams_t;

/* For static size definition */
typedef struct
{
    CoderAudioMmeTransformContext_t   CoderAudioMmeTransformContext;

    CoderAudioMmeDdce51StatusParams_t CodecStatusParams;

    MME_DataBuffer_t   *MMEDataBuffers_pArray[1 + CODER_AUDIO_MME_DDCE51_MAX_OUT_MME_BUFFER_PER_TRANSFORM];
    MME_ScatterPage_t   MMEScatterPagesArray[1 + CODER_AUDIO_MME_DDCE51_MAX_OUT_MME_BUFFER_PER_TRANSFORM];
    MME_DataBuffer_t    MMEBuffersArray[1 + CODER_AUDIO_MME_DDCE51_MAX_OUT_MME_BUFFER_PER_TRANSFORM];
    Buffer_t            CodedBuffersArray[CODER_AUDIO_MME_DDCE51_MAX_OUT_MME_BUFFER_PER_TRANSFORM];
} CoderAudioMmeDdce51TransformContext_t;

/**
 * @brief DDCE51 Specifics MME Encoder Class
 */
class Coder_Audio_Mme_Ddce51_c: public Coder_Audio_Mme_c
{
public:
    Coder_Audio_Mme_Ddce51_c(void);
    ~Coder_Audio_Mme_Ddce51_c(void);

protected:
    //Inherited Context independent
    void        FillDefaultCodecSpecific(unsigned char CodecSpecificConfig[NB_ENCODER_CONFIG_ELEMENT]);
    uint32_t    GetCodecCapabilityMask(void) {return ACC_AC3;};

    //Inherited Context Dependent
    CoderStatus_t UpdateDynamicCodecContext(CoderAudioMmeCodecDynamicContext_t *, const CoderAudioCurrentParameters_t *);
    void          SetCurrentTransformContextSizesAndPointers();
    bool          AreCurrentControlsAndMetadataSupported(const CoderAudioCurrentParameters_t *);

    //Debug
    void        DumpMmeGlobalParams(MME_AudioEncoderGlobalParams_t *GlobalParams);
};

#endif /* CODER_AUDIO_MME_DDCE51_H */
