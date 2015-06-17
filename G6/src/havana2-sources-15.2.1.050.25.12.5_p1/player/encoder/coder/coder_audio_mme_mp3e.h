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
#ifndef CODER_AUDIO_MME_MP3E_H
#define CODER_AUDIO_MME_MP3E_H

#include "coder_audio_mme.h"

#include <ACC_Transformers/MP3ELC_EncoderTypes.h>
#include <ACC_Transformers/MP3E_EncoderTypes.h>

#define CODER_AUDIO_MME_MP3E_MP2_SUPPORTED  0 //Waiting for feedback from XueYao
#define CODER_AUDIO_MME_MP3E_MP25_SUPPORTED 0 //Waiting for feedback from XueYao

#define CODER_AUDIO_MME_MP3E_MP1_SAMPLES_PER_FRAME            1152 //2 granules per frame for sampling freqs >= 32kHz
#define CODER_AUDIO_MME_MP3E_MP2_MP25_SAMPLES_PER_FRAME       576  //1 granule  per frame for sampling freqs <  32kHz

#if CODER_AUDIO_MME_MP3E_MP2_SUPPORTED
#define CODER_AUDIO_MME_MP3E_MIN_SAMPLES_PER_FRAME            CODER_AUDIO_MME_MP3E_MP2_MP25_SAMPLES_PER_FRAME
#else
#define CODER_AUDIO_MME_MP3E_MIN_SAMPLES_PER_FRAME            CODER_AUDIO_MME_MP3E_MP1_SAMPLES_PER_FRAME
#endif
#define CODER_AUDIO_MME_MP3E_MAX_SAMPLES_PER_FRAME            CODER_AUDIO_MME_MP3E_MP1_SAMPLES_PER_FRAME

/// Largest frame size is at Fs = 32kHz, where bit-buffer delay is smallest
/// @todo see how to use resizeable buffers to reduce memory usage
#define CODER_AUDIO_MME_MP3E_MAX_CODED_FRAME_SIZE             1440

/// Bitbuffer can yield up to 4 frame delay (mono 64kps, fs > 32kHz) + 1 frame for subfilter
#define CODER_AUDIO_MME_MP3E_MAX_CODEC_INTRISIC_FRAME_DELAY   5

#define CODER_AUDIO_MME_MP3E_MAX_OUT_MME_BUFFER_PER_TRANSFORM (CODER_AUDIO_MME_MP3E_MAX_CODEC_INTRISIC_FRAME_DELAY \
                                 + CODER_AUDIO_UPWARD_DIV(CODER_AUDIO_MME_MAX_SMP_PER_INPUT_BUFFER, CODER_AUDIO_MME_MP3E_MIN_SAMPLES_PER_FRAME))

#define CODER_AUDIO_MME_MP3E_TRANSFORM_CONTEXT_POOL_DEPTH     (CODER_AUDIO_MME_DEF_TRANSFORM_CONTEXT_POOL_DEPTH)

#define CODER_AUDIO_MME_MP3E_DEFAULT_ENCODING_SAMPLING_FREQ    48000
#define CODER_AUDIO_MME_MP3E_DEFAULT_ENCODING_LOUDNESS_LEVEL   (-3100)

/* Codec specific with streaming engine controls */
#define CODER_AUDIO_MME_MP3E_DEFAULT_ENCODING_VBR              false
#define CODER_AUDIO_MME_MP3E_DEFAULT_ENCODING_VBR_QUALITY      100
#define CODER_AUDIO_MME_MP3E_DEFAULT_ENCODING_CRC              false
#define CODER_AUDIO_MME_MP3E_DEFAULT_ENCODING_COPYRIGHT        false
#define CODER_AUDIO_MME_MP3E_DEFAULT_ENCODING_ORIGINAL         false

/* Codec specific without streaming controls: may later be addressable through binary config */
#define CODER_AUDIO_MME_MP3E_DEFAULT_ENCODING_INTENSITY_OFF    false /*!< Recommended value 0 = Intensity ON ; Must be OFF in mode 1p1*/
#define CODER_AUDIO_MME_MP3E_DEFAULT_ENCODING_BANDWIDTH        0     /*!< Recommended value 0 = Auto */

/* Codec specific that must not be changed */
#define CODER_AUDIO_MME_MP3E_RESERVED_ENCODING_FULL_HUFFMAN    false
#define CODER_AUDIO_MME_MP3E_RESERVED_ENCODING_PADDING_MODE    false
#define CODER_AUDIO_MME_MP3E_RESERVED_ENCODING_PRIVATE         false
#define CODER_AUDIO_MME_MP3E_RESERVED_ENCODING_EMPHASIS        false
#define CODER_AUDIO_MME_MP3E_RESERVED_ENCODING_DOWNMIX         false

#define CODER_AUDIO_MME_MP3E_MAX_RATES_PER_MODE 15

typedef struct CoderAudioMmeMp3eSupportedMode_s
{
    unsigned int    SampleRate;
    enum eAccAcMode AudioCodingMode;
    unsigned int    NumberOfSupportedRates;
    unsigned int    Rates[CODER_AUDIO_MME_MP3E_MAX_RATES_PER_MODE];
} CoderAudioMmeMp3eSupportedMode_t;

typedef struct
{
    CoderAudioMmeAccessorAudEncOutConfigCommon_t Common;
    union
    {
        U8              Config[NB_ENCODER_CONFIG_ELEMENT];  //!< Codec Specific encoder configuration
        union
        {
            tMMEMp3eLcConfig    LcSpecificConfig;
            tMMEMp3eConfig      SpecificConfig;
        };
    };
} CoderAudiMmeMp3eOutConfig_t;

/* For static size definition */
typedef struct
{
    MME_AudioEncoderFrameBufferStatus_t FrameStatus[CODER_AUDIO_MME_MP3E_MAX_OUT_MME_BUFFER_PER_TRANSFORM];
    unsigned int            SpecificStatusSize;  //!< Cannot be used to access member, only here for size
    union
    {
        MME_Mp3eLcStatus_t  LcSpecificStatus;      //!< Cannot be used to access member, only here for size
        MME_Mp3eStatus_t    SpecificStatus;      //!< Cannot be used to access member, only here for size
    };
} CoderAudioMmeMp3eSpecificEncoderBArray_t;

/* For static size definition*/
typedef struct
{
    CoderAudioMmeAccessorAudioEncoderStatusCommonParams_t CommonStatus;
    union
    {
        unsigned char SpecificEncoderStatusBArray[MAX_NB_ENCODER_TRANSFORM_RETURN_ELEMENT];
        CoderAudioMmeMp3eSpecificEncoderBArray_t ActualSpecificEncoderBArray;
    };
} CoderAudioMmeMp3eStatusParams_t;

typedef struct CoderAudioMmeMp3eControls_s
{
    bool CalculateCrc;
    bool Copyrighted;
    bool Original;
} CoderAudioMmeMp3eControls_t;


/* For static size definition */
typedef struct
{
    CoderAudioMmeTransformContext_t   CoderAudioMmeTransformContext;

    CoderAudioMmeMp3eStatusParams_t   CodecStatusParams;

    MME_DataBuffer_t         *MMEDataBuffers_pArray[1 + CODER_AUDIO_MME_MP3E_MAX_OUT_MME_BUFFER_PER_TRANSFORM];
    MME_ScatterPage_t         MMEScatterPagesArray[1 + CODER_AUDIO_MME_MP3E_MAX_OUT_MME_BUFFER_PER_TRANSFORM];
    MME_DataBuffer_t          MMEBuffersArray[1 + CODER_AUDIO_MME_MP3E_MAX_OUT_MME_BUFFER_PER_TRANSFORM];
    Buffer_t                  CodedBuffersArray[CODER_AUDIO_MME_MP3E_MAX_OUT_MME_BUFFER_PER_TRANSFORM];
} CoderAudioMmeMp3eTransformContext_t;

class Coder_Audio_Mme_Mp3e_c: public Coder_Audio_Mme_c
{
public:
    Coder_Audio_Mme_Mp3e_c(void);
    ~Coder_Audio_Mme_Mp3e_c(void);

protected:
    // Inherited Context independant
    void        FillDefaultMetadata(stm_se_uncompressed_frame_metadata_t *Metadata);
    void        FillDefaultCodecSpecific(unsigned char CodecSpecificConfig[NB_ENCODER_CONFIG_ELEMENT]);
    uint32_t    GetCodecCapabilityMask(void) {return ACC_MP3;};

    // Inherited Context Dependent
    CoderStatus_t UpdateDynamicCodecContext(CoderAudioMmeCodecDynamicContext_t *, const CoderAudioCurrentParameters_t *);
    void          SetCurrentTransformContextSizesAndPointers();
    bool          AreCurrentControlsAndMetadataSupported(const CoderAudioCurrentParameters_t *);

    void          AnalyseCurrentVsPreviousParameters(CoderAudioMmeParameterUpdateAnalysis_t *);

    void        DumpMmeGlobalParams(MME_AudioEncoderGlobalParams_t *GlobalParams);
};

#endif /* CODER_AUDIO_MME_MP3E_H */
