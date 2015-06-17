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

#ifndef H_AUDIO_GENERATOR_CLASS
#define H_AUDIO_GENERATOR_CLASS

#include "osinline.h"
#include "player_types.h"
#include "mixer_mme.h"

#define AUDIO_GENERATOR_NAME_LENGTH 32
typedef PlayerStatus_t  AudioGeneratorStatus_t;

class Audio_Generator_c
{
public:
    explicit Audio_Generator_c(const char *);
    ~Audio_Generator_c(void);

    char                                     Name[AUDIO_GENERATOR_NAME_LENGTH];

    int32_t                                  SetOption(stm_se_ctrl_t ctrl, const int32_t   value);
    int32_t                                  GetOption(stm_se_ctrl_t ctrl, int32_t        *value);
    int32_t                                  SetCompoundOption(stm_se_ctrl_t ctrl, const void     *value);
    int32_t                                  GetCompoundOption(stm_se_ctrl_t ctrl, void           *value);
    int32_t                                  SetState(stm_se_audio_generator_state_t      state);

    stm_se_audio_generator_state_t           GetState(void);
    int32_t                                  Start(void);
    int32_t                                  Stop(void);
    void                                     GetInfo(stm_se_audio_generator_info_t      *info);
    int32_t                                  Commit(uint32_t nspl);
    int32_t                                  SamplesConsumed(uint32_t samples);
    uint32_t                                 GetApplicationType(void);
    bool                                     IsInteractiveAudio(void);
    uint32_t                                 GetNumChannels(void);
    uint32_t                                 GetSamplingFrequency(void);
    uint32_t                                 GetBytesPerSample(void);
    uint32_t                                 GetPlayPointer(void);
    enum eAccAcMode                          GetAudioMode(void);

    AudioGeneratorStatus_t AttachMixer(Mixer_Mme_c *mixer);
    AudioGeneratorStatus_t DetachMixer(Mixer_Mme_c *mixer);

    void  AudioGeneratorEventUnderflow(uint32_t SamplesToDescatter, Rational_c RescaleFactor);

private:
    bool                                     RegisteredWithMixer;
    stm_se_audio_core_format_t               PcmContent;
    Mixer_Mme_c                             *Mixer;
    unsigned char                           *Buffer;
    uint32_t                                 BufferSize;
    uint32_t                                 BufferWriteOffset;
    uint32_t                                 BufferReadOffset;
    uint32_t                                 BufferCommittedBytes;
    uint32_t                                 BytesPerSample;
    stm_se_audio_pcm_format_t                PcmFormat;
    stm_se_emphasis_type_t                   Emphasis;
    stm_se_audio_generator_info_t            Info;
    uint32_t                                 ApplicationType;
    OS_Mutex_t                               Lock;

    int32_t                                  SignalEvent(stm_se_audio_generator_event_t Event);

    DISALLOW_COPY_AND_ASSIGN(Audio_Generator_c);
};

#endif // H_AUDIO_GENERATOR_CLASS
