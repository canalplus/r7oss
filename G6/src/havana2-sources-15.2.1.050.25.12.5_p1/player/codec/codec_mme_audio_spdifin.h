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

#ifndef H_CODEC_MME_AUDIO_SPDIFIN
#define H_CODEC_MME_AUDIO_SPDIFIN

#include "codec_mme_audio_stream.h"
#include "audio_reader.h"

#undef TRACE_TAG
#define TRACE_TAG "Codec_MmeAudioSpdifin_c"

#define SPDIFIN_MAX_TRANSCODED_FRAME_SIZE 16384 /* Corresponds to DTS max frame size DTSHD_FRAME_MAX_SIZE*/
/* Corresponds to the no. of samples captured and pass by the audio reader.
Default samples AUDIO_READER_DEFAULT_PERIOD_SPL captured by the reader is 512.
Current AUDIO_MAX_CHANNELS supported by audio reader is 2. Will be updated during layout1,HBRA support).
AUDIO_SAMPLE_DEPTH is in bits so diving by 8
Note:- Application can change the reader grain using STM_SE_CTRL_AUDIO_READER_GRAIN. In that case below computed size is not the Max*/
#define SPDIFIN_MAX_COMPRESSED_FRAME_SIZE AUDIO_READER_DEFAULT_PERIOD_SPL * AUDIO_MAX_CHANNELS * (AUDIO_SAMPLE_DEPTH / 8) * 4 /* * 4 is for over sampling case(DD+) */
#define SPDIFIN_MAXIMUM_SAMPLE_COUNT      0 // It is used to set the fix output BlockSize. 0 means output samples as set by parser NumberOfSamples.


typedef struct Codec_SpdifinStatus_s
{
    enum eMulticomSpdifinState State;
    enum eMulticomSpdifPC      StreamType;
    U32                        PlayedSamples;
} Codec_SpdifinStatus_t;

class Codec_MmeAudioSpdifin_c : public Codec_MmeAudioStream_c
{
public:
    Codec_MmeAudioSpdifin_c(void);
    ~Codec_MmeAudioSpdifin_c(void);

    //
    // Override Base Component class method
    //

    CodecStatus_t   GetAttribute(const char                     *Attribute,
                                 PlayerAttributeDescriptor_t    *Value);
    CodecStatus_t   SetAttribute(const char                     *Attribute,
                                 PlayerAttributeDescriptor_t    *Value);

    static void     GetCapabilities(stm_se_audio_dec_capability_t::audio_dec_iec61937_capability_s *cap,
                                    const MME_LxAudioDecoderHDInfo_t decInfo);

protected:
    uint32_t                   NbSamplesPerTransform;
    // Check policy PolicyHdmiRxMode and set HdmiRxMode accordingly.
    uint32_t                   HdmiRxMode;

    // AutoDetect SPDIFIN decoding Status
    Codec_SpdifinStatus_t      SpdifStatus;

    // Externally useful information
    int                        DecodeErrors;
    unsigned long long int     NumberOfSamplesProcessed;

    void  SetAudioCodecDecAttributes();

    CodecStatus_t   FillOutTransformerGlobalParameters(MME_LxAudioDecoderGlobalParams_t *GlobalParams);
    CodecStatus_t   FillOutTransformerInitializationParameters(void);
    void            SetCommandIO(void);
    void            PresetIOBuffers(void);
    CodecStatus_t   ValidateDecodeContext(CodecBaseDecodeContext_t *Context);
    CodecStatus_t   DumpSetStreamParameters(void                   *Parameters);
    CodecStatus_t   DumpDecodeParameters(void                      *Parameters);

};

#endif //H_CODEC_MME_AUDIO_SPDIFIN
