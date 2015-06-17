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

#ifndef H_CODEC_MME_AUDIO_AAC
#define H_CODEC_MME_AUDIO_AAC

#include "codec_mme_audio.h"
#include "codec_mme_audio_stream.h"

#undef TRACE_TAG
#define TRACE_TAG "Codec_MmeAudioAac_c"

#define SENDBUF_TRIGGER_TRANSFORM_COUNT         1  // AAC can start with 1 SEND_BUFFERS

class Codec_MmeAudioAac_c : public Codec_MmeAudioStream_c
{
public:
    Codec_MmeAudioAac_c(void);
    ~Codec_MmeAudioAac_c(void);

    CodecStatus_t   Halt(void);

    //
    // Stream specific functions
    //
    CodecStatus_t   Connect(Port_c *Port);
    CodecStatus_t   Input(Buffer_t                  CodedBuffer);
    CodecStatus_t   FillOutDecodeContext(void);
    CodecStatus_t   SendMMEDecodeCommand(void);
    void            FinishedDecode(void);
    CodecStatus_t   DiscardQueuedDecodes(void);
    void            CallbackFromMME(MME_Event_t               Event,
                                    MME_Command_t            *Command);
    CodecStatus_t   OutputPartialDecodeBuffers(void);
    //
    // Overrides for component audio class functions
    //
    void            UpdateConfig(unsigned int Update);

    static void     FillStreamMetadata(ParsedAudioParameters_t         *AudioParameters,
                                       MME_LxAudioDecoderFrameStatus_t *Status);
    static void     GetCapabilities(stm_se_audio_dec_capability_t::audio_dec_aac_capability_s *cap,
                                    const MME_LxAudioDecoderHDInfo_t decInfo);

protected:
    eAccDecoderId     DecoderId;
    stm_se_mpeg4aac_t ApplicationConfig;
    bool              StreamBased;
    bool              UpmixMono2Stereo;
    int               DPulseIdCnt;

    CodecStatus_t   FillOutTransformerGlobalParameters(MME_LxAudioDecoderGlobalParams_t *GlobalParams);
    CodecStatus_t   FillOutTransformerInitializationParameters(void);
    CodecStatus_t   FillOutSetStreamParametersCommand(void);
    CodecStatus_t   FillOutDecodeCommand(void);
    CodecStatus_t   ValidateDecodeContext(CodecBaseDecodeContext_t *Context);
    CodecStatus_t   DumpSetStreamParameters(void    *Parameters);
    CodecStatus_t   DumpDecodeParameters(void   *Parameters);
    void            SetCommandIO(void);
    void            PresetIOBuffers(void);

    CodecStatus_t   FillAACSpecificStatistics(MME_LxAudioDecoderFrameExtendedStatus_t    *extStatus, unsigned int size);
    unsigned int    DecodedNumberOfChannels(MME_ChannelMapInfo_u  *ChannelMap);

    unsigned int    GetDPulseMode(void);
};

#endif
