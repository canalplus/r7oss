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

#include "audio_generator.h"
#include "audio_conversions.h"

#undef TRACE_TAG
#define TRACE_TAG "Audio_Generator_c"

////////////////////////////////////////////////////////////////////////////
///
///
///
Audio_Generator_c::Audio_Generator_c(const char *name)
    : Name()
    , RegisteredWithMixer(false)
    , PcmContent()
    , Mixer(NULL)
    , Buffer(NULL)
    , BufferSize(0)
    , BufferWriteOffset(0)
    , BufferReadOffset(0)
    , BufferCommittedBytes(0)
    , BytesPerSample(0)
    , PcmFormat()
    , Emphasis()
    , Info()
    , ApplicationType()
    , Lock()
{
    strncpy(Name, name, sizeof(Name));
    Name[sizeof(Name) - 1] = '\0';

    Info.state = STM_SE_AUDIO_GENERATOR_DISCONNECTED;

    OS_InitializeMutex(&Lock);
}

////////////////////////////////////////////////////////////////////////////
///
///
///
Audio_Generator_c::~Audio_Generator_c()
{
    OS_TerminateMutex(&Lock);
}

int32_t Audio_Generator_c::SetCompoundOption(stm_se_ctrl_t ctrl, const void *value)
{
    SE_DEBUG(group_audio_generator, ">%s (compound-ctrl= %d)\n", Name, ctrl);

    switch (GetState())
    {
    case STM_SE_AUDIO_GENERATOR_DISCONNECTED:
    case STM_SE_AUDIO_GENERATOR_STOPPED:
        break;
    case STM_SE_AUDIO_GENERATOR_STARTING:
    case STM_SE_AUDIO_GENERATOR_STARTED:
        return -EBUSY;
    case STM_SE_AUDIO_GENERATOR_STOPPING:
        return -EAGAIN;
    }

    switch (ctrl)
    {
    case STM_SE_CTRL_AUDIO_GENERATOR_BUFFER:
    {
        stm_se_audio_generator_buffer_t *buffer = (stm_se_audio_generator_buffer_t *)value;

        switch (buffer->format)
        {
        case STM_SE_AUDIO_PCM_FMT_S8:
        case STM_SE_AUDIO_PCM_FMT_U8:
        case STM_SE_AUDIO_PCM_FMT_ALAW_8:
        case STM_SE_AUDIO_PCM_FMT_ULAW_8:
            BytesPerSample = 1;
            break;

        case STM_SE_AUDIO_PCM_FMT_S16LE:
        case STM_SE_AUDIO_PCM_FMT_S16BE:
        case STM_SE_AUDIO_PCM_FMT_U16LE:
        case STM_SE_AUDIO_PCM_FMT_U16BE:
            BytesPerSample = 2;
            break;

        case STM_SE_AUDIO_PCM_FMT_S24LE:
        case STM_SE_AUDIO_PCM_FMT_S24BE:
            BytesPerSample = 3;
            break;

        case STM_SE_AUDIO_PCM_FMT_S32LE:
        case STM_SE_AUDIO_PCM_FMT_S32BE:
            BytesPerSample = 4;
            break;

        default:
            SE_ERROR("%s Unsupported STM_SE_CTRL_AUDIO_GENERATOR_BUFFER.Format %d\n", Name, (int) buffer->format);
            return -EINVAL;
        }

        Buffer               = (unsigned char *)buffer->audio_buffer;
        BufferSize           = buffer->audio_buffer_size;
        BytesPerSample      *= GetNumChannels();
        BufferCommittedBytes = 0;
        BufferWriteOffset    = 0;
        BufferReadOffset     = 0;
        PcmFormat            = buffer->format;
        break;
    }

    case STM_SE_CTRL_AUDIO_INPUT_FORMAT:
        memcpy((void *) &PcmContent, value, sizeof(stm_se_audio_core_format_t));
        break;

    default:
    {
        SE_ERROR("%s Invalid compound-control %d\n", Name, ctrl);
        return -EINVAL;
    }
    }

    return 0;
}

int32_t Audio_Generator_c::GetCompoundOption(stm_se_ctrl_t ctrl, void *value)
{
    switch (ctrl)
    {
    case STM_SE_CTRL_AUDIO_GENERATOR_BUFFER:
    {
        stm_se_audio_generator_buffer_t *buffer = (stm_se_audio_generator_buffer_t *) value;
        buffer->audio_buffer       = Buffer;
        buffer->audio_buffer_size  = BufferSize;
        buffer->format             = PcmFormat;
        break;
    }

    case STM_SE_CTRL_AUDIO_INPUT_FORMAT:
    {
        memcpy(value, &PcmContent, sizeof(stm_se_audio_core_format_t));
        break;
    }

    default:
        SE_ERROR("%s Invalid control %d\n", Name, ctrl);
        return -EINVAL;
    }

    return 0;
}

int32_t Audio_Generator_c::SetOption(stm_se_ctrl_t ctrl, int32_t value)
{
    SE_DEBUG(group_audio_generator, ">%s (ctrl= %d, value= %d)\n", Name, ctrl, value);

    switch (ctrl)
    {
    case STM_SE_CTRL_AUDIO_APPLICATION_TYPE:
        if (value <= STM_SE_CTRL_VALUE_LAST_AUDIO_APPLICATION_TYPE)
        {
            ApplicationType = value;
        }
        else
        {
            SE_ERROR(">%s: APPLICATION_TYPE invalid value %d\n", Name, value);
        }

        break;

    case STM_SE_CTRL_AUDIO_INPUT_EMPHASIS:
        Emphasis = ((value >= STM_SE_NO_EMPHASIS) && (value <= STM_SE_EMPH_CCITT_J_17)) ? (stm_se_emphasis_type_t) value : STM_SE_NO_EMPHASIS;
        break;

    default:
        SE_ERROR("%s Invalid control %d :: %d\n", Name, ctrl, value);
        return -EINVAL;
    }

    return 0;
}

int32_t Audio_Generator_c::GetOption(stm_se_ctrl_t ctrl, int32_t *value)
{
    switch (ctrl)
    {
    case STM_SE_CTRL_AUDIO_APPLICATION_TYPE:
        *value = ApplicationType;
        SE_DEBUG(group_audio_generator, ">%s %d [AUDIO_APPLICATION_TYPE] : %d\n", Name, ctrl, *value);
        break;

    case STM_SE_CTRL_AUDIO_INPUT_EMPHASIS:
        *value = Emphasis;
        break;

    default:
        SE_ERROR("%s Invalid control %d\n", Name, ctrl);
        return -EINVAL;
    }

    return 0;
}

int32_t Audio_Generator_c::Start(void)
{
    if (RegisteredWithMixer)
    {
        return Mixer->StartAudioGenerator(this);
    }

    return -EBUSY;
}

int32_t Audio_Generator_c::Stop(void)
{
    return Mixer->StopAudioGenerator(this);
}

int32_t Audio_Generator_c::SetState(stm_se_audio_generator_state_t state)
{
    Info.state = state;
    return 0;
}

void Audio_Generator_c::GetInfo(stm_se_audio_generator_info_t *info)
{
    uint32_t available_bytes;
    uint32_t number_of_samples;
    OS_LockMutex(&Lock);
    available_bytes   = BufferSize - BufferCommittedBytes;
    number_of_samples = BytesPerSample ? available_bytes / BytesPerSample : 0;
    info->avail       = number_of_samples;
    info->head_offset = BufferWriteOffset;
    OS_UnLockMutex(&Lock);
}

int32_t Audio_Generator_c::Commit(uint32_t number_of_samples)
{
    uint32_t number_of_bytes = number_of_samples * BytesPerSample;
    uint32_t available_bytes;
    OS_LockMutex(&Lock);
    available_bytes = BufferSize - BufferCommittedBytes;

    if (number_of_bytes > available_bytes)
    {
        OS_UnLockMutex(&Lock);
        return -EINVAL;
    }

    BufferWriteOffset += number_of_bytes;

    if (BufferWriteOffset >= BufferSize)
    {
        BufferWriteOffset -= BufferSize;
    }

    BufferCommittedBytes += number_of_bytes;
    OS_UnLockMutex(&Lock);
    return 0;
}

int32_t  Audio_Generator_c::SignalEvent(stm_se_audio_generator_event_t Event)
{
    stm_event_t   GeneratorEvent;
    int32_t err = 0;
    GeneratorEvent.object = (stm_object_h)this;
    GeneratorEvent.event_id = (uint32_t)Event;
    // Signal Event via Event manager
    err = stm_event_signal(&GeneratorEvent);
    if (err != 0)
    {
        SE_ERROR("%s Failed to Generate signal Event (%d)\n", Name, err);
        return HavanaError;
    }

    return 0;
}

int32_t Audio_Generator_c::SamplesConsumed(uint32_t number_of_samples)
{
    uint32_t number_of_bytes = number_of_samples * BytesPerSample;
    uint32_t Status;
    OS_LockMutex(&Lock);
    BufferCommittedBytes -= number_of_bytes;
    BufferReadOffset += number_of_bytes;

    while (BufferReadOffset >= BufferSize)
    {
        BufferReadOffset -= BufferSize;
    }

    OS_UnLockMutex(&Lock);
    Status  = this->SignalEvent(STM_SE_AUDIO_GENERATOR_EVENT_DATA_CONSUMED);

    if (Status != 0)
    {
        SE_ERROR("Failed to signal event\n");
        return -EINVAL;
    }

    return 0;
}

uint32_t Audio_Generator_c::GetBytesPerSample(void)
{
    return BytesPerSample;
}

uint32_t Audio_Generator_c::GetPlayPointer(void)
{
    return BufferReadOffset;
}

stm_se_audio_generator_state_t Audio_Generator_c::GetState(void)
{
    return Info.state;
}

uint32_t Audio_Generator_c::GetApplicationType(void)
{
    return ApplicationType;
}

bool     Audio_Generator_c::IsInteractiveAudio(void)
{
    return (ApplicationType == STM_SE_CTRL_VALUE_AUDIO_APPLICATION_DVD);
}

uint32_t Audio_Generator_c::GetNumChannels(void)
{
    return PcmContent.channel_placement.channel_count;
}

enum eAccAcMode Audio_Generator_c::GetAudioMode(void)
{
    eAccAcMode Mode = ACC_MODE_ID;
    StmSeAudioChannelPlacementAnalysis_t Analysis;
    stm_se_audio_channel_placement_t SortedPlacement;
    bool AudioModeIsPhysical;

    if (0 != StmSeAudioGetAcmodAndAnalysisFromChannelPlacement(&Mode, &AudioModeIsPhysical,
                                                               &SortedPlacement, &Analysis,
                                                               &PcmContent.channel_placement))
    {
        SE_ERROR("Failed to convert to Audio Mode. Using MODE_ID\n");
    }

    return Mode;
}

uint32_t Audio_Generator_c::GetSamplingFrequency(void)
{
    return PcmContent.sample_rate;
}

AudioGeneratorStatus_t Audio_Generator_c::AttachMixer(Mixer_Mme_c *mixer)
{
    PlayerStatus_t Status;
    SE_DEBUG(group_audio_generator, ">\n");
    Mixer = mixer;
    Status = Mixer->SendSetupAudioGeneratorRequest(this);

    if (Status != PlayerNoError)
    {
        Mixer = (Mixer_Mme_c *)0;
        SE_ERROR("%s Cannot register with the mixer\n", Name);
        return Status;
    }

    RegisteredWithMixer = true;
    SE_DEBUG(group_audio_generator, "<\n");
    return PlayerNoError;
}

AudioGeneratorStatus_t Audio_Generator_c::DetachMixer(Mixer_Mme_c *mixer)
{
    SE_DEBUG(group_audio_generator, ">\n");

    if (RegisteredWithMixer && mixer == Mixer)
    {
        PlayerStatus_t Status;
        Status = Mixer->SendFreeAudioGeneratorRequest(this);

        if (Status != PlayerNoError)
        {
            SE_ERROR("%s Cannot unregister with the mixer\n", Name);
            // no recovery possible
        }

        this->Mixer = (Mixer_Mme_c *)0;
        RegisteredWithMixer = false;
    }

    SE_DEBUG(group_audio_generator, "<\n");
    return PlayerNoError;
}
void  Audio_Generator_c::AudioGeneratorEventUnderflow(uint32_t SamplesToDescatter, Rational_c RescaleFactor)
{
    uint32_t TimeToFillInSamples;
    uint32_t NumberOfSamplesAvailable;
    uint32_t Status;
    NumberOfSamplesAvailable = BufferCommittedBytes / BytesPerSample;
    TimeToFillInSamples = (RescaleFactor * SamplesToDescatter).RoundedUpIntegerPart();

    if (TimeToFillInSamples > NumberOfSamplesAvailable)
    {
        Status  = this->SignalEvent(STM_SE_AUDIO_GENERATOR_EVENT_UNDERFLOW);

        if (Status != 0)
        {
            SE_ERROR("Failed to signal event\n");
        }
    }
}
