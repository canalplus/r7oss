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

#include "pcmplayer_ksound.h"
#include "mixer_player.h"

#undef TRACE_TAG
#define TRACE_TAG   "Mixer_Player_c"

const int32_t Mixer_Player_c::AUDIO_MAX_OUTPUT_ATTENUATION = -96;
const uint32_t Mixer_Player_c::MIN_FREQ_AC3_ENCODER = 48000;
const uint32_t Mixer_Player_c::MAX_FREQ_AC3_ENCODER = 48000;
const uint32_t Mixer_Player_c::MIN_FREQ_DTS_ENCODER = 44100;
const uint32_t Mixer_Player_c::MAX_FREQ_DTS_ENCODER = 48000;
const uint32_t Mixer_Player_c::MIN_FREQUENCY = 32000;
const char *Mixer_Player_c::DefaultCardName = "CardName is Undefined";

PlayerStatus_t Mixer_Player_c::CreatePcmPlayer(PcmPlayer_c::OutputEncoding MixerPrimaryCodedDataTypeSPDIF,
                                               PcmPlayer_c::OutputEncoding MixerPrimaryCodedDataTypeHDMI)
{
    SE_ASSERT((true == IsPlayerObjectAttached()));
    SE_ASSERT((false == HasPcmPlayer()));
    // Manage this audio player previously attached.
    PcmPlayer_c::OutputEncoding OutputEncoding = this->LookupOutputEncoding(MixerPrimaryCodedDataTypeSPDIF,
                                                                            MixerPrimaryCodedDataTypeHDMI);
    const char *alsaname = GetCard().alsaname;
    unsigned int major, minor;
    major = alsaname[3] - '0';
    minor = alsaname[5] - '0';
    PcmPlayer = NULL;
    PcmPlayer = new PcmPlayer_Ksound_c(major, minor, minor, OutputEncoding);

    if (NULL == PcmPlayer)
    {
        SE_ERROR("Memory allocation failure for PcmPlayer for Card %s %s\n", GetCardName(), alsaname);
        return PlayerError;
    }

    if (PlayerNoError != PcmPlayer->InitializationStatus)
    {
        delete PcmPlayer;
        PcmPlayer = NULL;
        SE_ERROR("Failed to manufacture PCM player\n");
        return PlayerError;
    }

    SE_DEBUG(group_mixer,  "PcmPlayer %p created for Card %s %s encoding:%s\n",
             PcmPlayer,
             GetCardName(),
             alsaname,
             PcmPlayer_c::LookupOutputEncoding(OutputEncoding));

    // reset the Surface descriptor here so that these be forcefully set at next UpdatePcmParameters
    memset(&PcmPlayerConfig.SurfaceParameters, 0, sizeof(PcmPlayerConfig.SurfaceParameters));

    return PlayerNoError;
}

void Mixer_Player_c::DeletePcmPlayer()
{
    SE_DEBUG(group_mixer, ">: %p\n", this);

    if (pPlayerObjectForMixer->isConnectedToHdmi())
    {
        FlushAudioInfoFrame();
    }

    SE_ASSERT((true == IsPlayerObjectAttached()));
    SE_ASSERT((true == HasPcmPlayer()));
    delete PcmPlayer;
    PcmPlayer = NULL;
}

////////////////////////////////////////////////////////////////////////////
///
/// Determine the output encoding to use for the specified device.
///
/// The process of determining the output encoding of a particular device
/// involved the (slightly complex) conjunction of Mixer_Mme_c::ActualTopology
/// and Mixer_Mme_c::OutputConfiguration . The former describes the
/// capabilities of an output (is is PCM, SPDIF or HDMI? does it like FatPipe?)
/// whilst the later is the configuration requested by the user (should SPDIF
/// output be AC3 encoded?).
///
/// Since joining the structures is 'quirky' we make sure to do it
/// only in one place.
///
/// The freq argument may legitimately be zero (typically when this method is
/// called from sites that don't know or care about the output frequency). In
/// this case the logic that disables certain encodings for unsupported sampling
/// frequencies will be disabled. This was safe at the time of writing but...
///
PcmPlayer_c::OutputEncoding Mixer_Player_c::LookupOutputEncoding(PcmPlayer_c::OutputEncoding MixerPrimaryCodedDataTypeSPDIF,
                                                                 PcmPlayer_c::OutputEncoding MixerPrimaryCodedDataTypeHDMI,
                                                                 uint32_t freq) const
{
    //SE_DEBUG(group_mixer, ">: %p\n", this);
    SE_ASSERT((true == IsPlayerObjectAttached()));
    // this boolean tells whether the output is connected to the hdmi cell
    bool IsConnectedToHdmiHw = pPlayerObjectForMixer->isConnectedToHdmi();
    // this boolean tells whether the output is connected to a spdif player
    bool IsConnectedToSpdifHw = pPlayerObjectForMixer->isConnectedToSpdif();
    // this boolean tells whether the output is a spdif output (i.e. connected to a spdif player, but not to hdmi)
    //bool IsConnectedToSpdifOnly = IsConnectedToSpdifPlayer && !IsConnectedToHdmi;
    bool IsBypass = this->IsBypass();
    SE_VERBOSE(group_mixer, "> SPDIF: %s HDMI: %s\n",
               PcmPlayer_c::LookupOutputEncoding(MixerPrimaryCodedDataTypeSPDIF),
               PcmPlayer_c::LookupOutputEncoding(MixerPrimaryCodedDataTypeHDMI));

    if (true == IsBypass)
    {
        // Look for possible bypass for SPDIF
        if (IsConnectedToSpdifHw && PcmPlayer_c::IsOutputBypassed(MixerPrimaryCodedDataTypeSPDIF))
        {
            SE_DEBUG(group_mixer, "< Card: %s is bypassed (%s)\n",
                     GetCardName(),
                     PcmPlayer_c::LookupOutputEncoding(MixerPrimaryCodedDataTypeSPDIF));
            return MixerPrimaryCodedDataTypeSPDIF;
        }

        // Look for possible bypass for HDMI
        if (IsConnectedToHdmiHw && PcmPlayer_c::IsOutputBypassed(MixerPrimaryCodedDataTypeHDMI))
        {
            SE_DEBUG(group_mixer, "< Card: %s is bypassed (%s)\n",
                     GetCardName(),
                     PcmPlayer_c::LookupOutputEncoding(MixerPrimaryCodedDataTypeHDMI));
            return MixerPrimaryCodedDataTypeHDMI;
        }
    }
    else
    {
        // Look for possible encoding
        if (IsConnectedToHdmiHw || IsConnectedToSpdifHw)
        {
            stm_se_ctrl_audio_player_hardware_mode_t hw_mode;

            if (PlayerNoError == GetCompoundOption(STM_SE_CTRL_AUDIO_PLAYER_HARDWARE_MODE, &hw_mode))
            {
                stm_se_player_mode_t RequiredEncoding = hw_mode.playback_mode;

                switch (RequiredEncoding)
                {
                case STM_SE_PLAY_PCM_OUT:
                {
                    SE_DEBUG(group_mixer, "< Card: %s is PCM OUTPUT_IEC60958\n", GetCardName());
                    return PcmPlayer_c::OUTPUT_IEC60958;
                }

                case STM_SE_PLAY_AC3_OUT:
                {
                    if ((freq == 0) || ((freq >= MIN_FREQ_AC3_ENCODER) && (freq <= MAX_FREQ_AC3_ENCODER)))
                    {
                        SE_DEBUG(group_mixer, "< Card: %s is AC3\n", GetCardName());
                        return PcmPlayer_c::OUTPUT_AC3;
                    }
                    else
                    {
                        SE_INFO(group_mixer, "< Card: %s is PCM OUTPUT_IEC60958 (because AC3 encoder doesn't support %d hz)\n",
                                GetCardName(), freq);
                        return PcmPlayer_c::OUTPUT_IEC60958;
                    }
                }

                case STM_SE_PLAY_DTS_OUT:
                {
                    if (freq == 0 || (freq >= MIN_FREQ_DTS_ENCODER && freq <= MAX_FREQ_DTS_ENCODER))
                    {
                        SE_DEBUG(group_mixer, "< Card: %s is DTS\n", GetCardName());
                        return PcmPlayer_c::OUTPUT_DTS;
                    }
                    else
                    {
                        SE_INFO(group_mixer, "< Card: %s is PCM OUTPUT_IEC60958 (because DTS encoder doesn't support %d hz\n",
                                GetCardName(), freq);
                        return PcmPlayer_c::OUTPUT_IEC60958;
                    }
                }

                default:
                {
                    SE_DEBUG(group_mixer, "< Card: %s behaves as default (assuming PCM OUTPUT_IEC60958)\n", GetCardName());
                    return PcmPlayer_c::OUTPUT_IEC60958;
                }
                }
            }
            else
            {
                SE_ERROR("< Card: %s\n", GetCardName());
            }
        }
    }

    SE_DEBUG(group_mixer, "< Card: %s as OUTPUT_PCM\n", GetCardName());
    return PcmPlayer_c::OUTPUT_PCM;
}

////////////////////////////////////////////////////////////////////////////
///
/// Determine the output number of channels for the specified audio player.
///
///
uint32_t Mixer_Player_c::LookupOutputNumberOfChannels(PcmPlayer_c::OutputEncoding MixerPrimaryCodedDataTypeSPDIF,
                                                      PcmPlayer_c::OutputEncoding MixerPrimaryCodedDataTypeHDMI) const
{
    //SE_DEBUG(group_mixer, ">\n");
    SE_ASSERT((true == IsPlayerObjectAttached()));
    PcmPlayer_c::OutputEncoding Encoding = LookupOutputEncoding(MixerPrimaryCodedDataTypeSPDIF,
                                                                MixerPrimaryCodedDataTypeHDMI);
    uint32_t ChannelCount;

    if (Encoding < PcmPlayer_c::BYPASS_LOWEST)
    {
        ChannelCount = GetCardNumberOfChannels();
    }
    else if (Encoding < PcmPlayer_c::BYPASS_HBRA_LOWEST)
    {
        ChannelCount = 2;
    }
    else
    {
        // high bit rate audio case
        ChannelCount = 8;
    }

    SE_DEBUG(group_mixer, "< %d\n", ChannelCount);
    return ChannelCount;
}

uint32_t Mixer_Player_c::LookupOutputSamplingFrequency(PcmPlayer_c::OutputEncoding OutputEncoding,
                                                       uint32_t MixerOutputSamplingFrequency,
                                                       uint32_t MixerIec60958FrameRate) const
{
    SE_DEBUG(group_mixer, ">%s %u %u\n",
             PcmPlayer_c::LookupOutputEncoding(OutputEncoding),
             MixerOutputSamplingFrequency,
             MixerIec60958FrameRate);
    SE_ASSERT((true == IsPlayerObjectAttached()));
    uint32_t Freq, MixerFreq, MaxFreq;
    SE_ASSERT(MixerOutputSamplingFrequency);
    Freq = MixerFreq = MixerOutputSamplingFrequency;
    // start with the constraint imposed by the settings for this output device
    MaxFreq = GetCard().max_freq;

    // check to see if the output encoding imposes any constraints on the sampling frequency
    if (OutputEncoding == PcmPlayer_c::OUTPUT_AC3 && MaxFreq > MAX_FREQ_AC3_ENCODER)
    {
        MaxFreq = MAX_FREQ_AC3_ENCODER;
    }
    else if (OutputEncoding == PcmPlayer_c::OUTPUT_DTS && MaxFreq > MAX_FREQ_DTS_ENCODER)
    {
        MaxFreq = MAX_FREQ_DTS_ENCODER;
    }
    else if (PcmPlayer_c::IsOutputBypassed(OutputEncoding))
    {
        // the frequency we're calculating is the HDMI pcm player sampling frequency
        Freq = MixerIec60958FrameRate;

        if ((OutputEncoding >= PcmPlayer_c::BYPASS_HBRA_LOWEST) && (OutputEncoding <= PcmPlayer_c::BYPASS_HIGHEST))
        {
            Freq /= 4;
        }

        SE_DEBUG(group_mixer, "Output requires sampling frequency of %d (bypassed)\n", Freq);
        return (Freq);
    }

    // (conditionally) deploy post-mix resampling to meet our constraints
    while (Freq > MaxFreq && Freq > MIN_FREQUENCY)
    {
        unsigned int Fprime = Freq / 2;

        if (Fprime * 2 != Freq)
        {
            SE_ASSERT(0);
            break;
        }

        Freq = Fprime;
    }

    // verify correct operation of the pre-mix resampling (and that the topology's
    // maximum frequency is not impossible to honor)
    if (Freq < MIN_FREQUENCY)
    {
        SE_ERROR("Unexpected mixer output frequency %d (%s)\n",
                 Freq,
                 (MixerFreq < MIN_FREQUENCY ? "pre-mix SRC did not deploy" : "max_freq is too aggressive"));
    }

    while (Freq < MIN_FREQUENCY)
    {
        Freq *= 2;
    }

    if (Freq != MixerFreq)
    {
        SE_DEBUG(group_mixer,  "Requesting post-mix resampling (%d -> %d)\n", MixerFreq, Freq);
    }

    SE_DEBUG(group_mixer, "< Card: %s is %s requires sampling frequency of %d\n",
             GetCardName(),
             PcmPlayer_c::LookupOutputEncoding(OutputEncoding), Freq);
    return Freq;
}


////////////////////////////////////////////////////////////////////////////
///
/// Configure the HDMI audio cell
///
PlayerStatus_t Mixer_Player_c::ConfigureHDMICell(PcmPlayer_c::OutputEncoding OutputEncoding, SpdifInProperties_t *SpdifInProperties) const
{
    PlayerStatus_t       status = PlayerNoError;
    bool                 is_connected_to_spdif = pPlayerObjectForMixer->isConnectedToSpdif();

    if (pPlayerObjectForMixer->isConnectedToHdmi())
    {
        SE_DEBUG(group_audio_player, "> %s [Connected to Spdif : %d]\n", PcmPlayer_c::LookupOutputEncoding(OutputEncoding), is_connected_to_spdif);

        // get a handle to the output device
        stm_display_device_h pDev;
        stm_display_output_h out;

        status = OpenHdmiDevice(&pDev, &out);

        if (status == PlayerNoError)
        {
            // check the current hdmi audio switches...
            unsigned int ctrlVal;

            if (!stm_display_output_get_control(out, OUTPUT_CTRL_AUDIO_SOURCE_SELECT, &ctrlVal))
            {
                SE_DEBUG(group_audio_player, "Current HDMI Audio source selection: 0x%x\n", ctrlVal);
            }

            stm_hdmi_audio_output_type_t OutputType;
            unsigned int AudioSource = STM_AUDIO_SOURCE_NONE;

            if ((OutputEncoding == PcmPlayer_c::BYPASS_TRUEHD) ||
                (OutputEncoding == PcmPlayer_c::BYPASS_DTSHD_MA))
            {
                // High Bit Rate audio is required...
                OutputType = STM_HDMI_AUDIO_TYPE_HBR;
                AudioSource = STM_AUDIO_SOURCE_8CH_I2S;
            }
            else if (SpdifInProperties != NULL &&
                     (OutputEncoding == PcmPlayer_c::BYPASS_SPDIFIN_COMPRESSED ||
                      OutputEncoding == PcmPlayer_c::BYPASS_SPDIFIN_PCM))
            {
                if (SpdifInProperties->ChannelCount > 2)
                {
                    AudioSource = STM_AUDIO_SOURCE_8CH_I2S;
                }
                else
                {
                    AudioSource = STM_AUDIO_SOURCE_2CH_I2S;
                }
                switch (SpdifInProperties->SpdifInLayout)
                {
                case STM_SE_AUDIO_STREAM_TYPE_IEC :
                    OutputType = STM_HDMI_AUDIO_TYPE_NORMAL;
                    break;
                case STM_SE_AUDIO_STREAM_TYPE_DSD :
                    OutputType = STM_HDMI_AUDIO_TYPE_DST_DOUBLE;
                    break;
                case STM_SE_AUDIO_STREAM_TYPE_DST :
                    OutputType = STM_HDMI_AUDIO_TYPE_DST;
                    break;
                case STM_SE_AUDIO_STREAM_TYPE_HBR :
                    OutputType = STM_HDMI_AUDIO_TYPE_HBR;
                    break;
                default :
                    OutputType = STM_HDMI_AUDIO_TYPE_NORMAL;
                    break;
                }
            }
            else
            {
                // hdmi layout 0 mode
                OutputType = STM_HDMI_AUDIO_TYPE_NORMAL;

                if (is_connected_to_spdif)
                {
                    // we're connected to a spdif player...
                    AudioSource = STM_AUDIO_SOURCE_SPDIF;
                    SE_DEBUG(group_audio_player, "@: STM_AUDIO_SOURCE_SPDIF\n");
                }
                else
                {
                    // we're connected to a pcm player
                    if (pPlayerObjectForMixer->isStereo())   // Layout0
                    {
                        AudioSource = STM_AUDIO_SOURCE_2CH_I2S;
                        SE_DEBUG(group_audio_player, "@: STM_AUDIO_SOURCE_2CH_I2S\n");
                    }
                    else              // Layout1
                    {
                        AudioSource = STM_AUDIO_SOURCE_8CH_I2S;
                        SE_DEBUG(group_audio_player, "@: STM_AUDIO_SOURCE_8CH_I2S\n");
                    }
                }
            }

            if (stm_display_output_set_control(out, OUTPUT_CTRL_AUDIO_SOURCE_SELECT, AudioSource))
            {
                SE_ERROR("Could not set control OUTPUT_CTRL_AUDIO_SOURCE_SELECT\n");
                status = PlayerError;
            }

            if (stm_display_output_set_control(out, OUTPUT_CTRL_HDMI_AUDIO_OUT_SELECT, OutputType))
            {
                SE_ERROR("Could not set control OUTPUT_CTRL_HDMI_AUDIO_OUT_SELECT\n");
                status = PlayerError;
            }

            CloseHdmiDevice(pDev, out);
        }
    }

    return status;
}


////////////////////////////////////////////////////////////////////////////
///
/// Open the HDMI device
///
PlayerStatus_t Mixer_Player_c::OpenHdmiDevice(stm_display_device_h *pDev,  stm_display_output_h *out) const
{
    uint32_t hdmi_device_id = pPlayerObjectForMixer->GetHdmiDeviceId();
    int32_t  hdmi_output_id = pPlayerObjectForMixer->GetHdmiOutputId();

    if (stm_display_open_device(hdmi_device_id, pDev) != 0)
    {
        SE_ERROR("Unable to get display device %d\n", hdmi_device_id);
        return PlayerError;
    }

    if (stm_display_device_open_output(*pDev, hdmi_output_id, out) != 0)
    {
        SE_ERROR("Unable to get display output %d (device %d)\n", hdmi_output_id, hdmi_device_id);
        stm_display_device_close(*pDev);
        return PlayerError;
    }
    return PlayerNoError;
}

////////////////////////////////////////////////////////////////////////////
///
/// Close the HDMI device
///
void Mixer_Player_c::CloseHdmiDevice(stm_display_device_h pDev, stm_display_output_h out) const
{
    stm_display_output_close(out);
    stm_display_device_close(pDev);
}

////////////////////////////////////////////////////////////////////////////
///
/// Flush the AudioInfoFrame queue
///
PlayerStatus_t Mixer_Player_c::FlushAudioInfoFrame(void)
{
    PlayerStatus_t       status = PlayerNoError;

    // flush HDMI audio metadata frames
    stm_display_device_h pDev;
    stm_display_output_h out;
    status = OpenHdmiDevice(&pDev, &out);

    if (status == PlayerNoError)
    {
        if (stm_display_output_flush_metadata(out, STM_METADATA_TYPE_AUDIO_IFRAME) != 0)
        {
            SE_ERROR("Unable to flush audio-info-frame metadata queue\n");
            status = PlayerError;
        }

        CloseHdmiDevice(pDev, out);
    }

    return status;
}
////////////////////////////////////////////////////////////////////////////
///
/// Update the AudioInfoFrame
///
PlayerStatus_t Mixer_Player_c::UpdateAudioInfoFrame(uint32_t sfreq_hz, enum eAccAcMode acmod , uint8_t downmix_info)
{
    PlayerStatus_t       status = PlayerNoError;
    bool                 changed = false;

    if ((sfreq_hz     != mSamplingFrequency) ||
        (acmod        != mAcMode)            ||
        (downmix_info != mDownmixInfo))
    {
        changed = true;
    }

    // AudioInfoFrame only matters for HDMI player.
    if (changed && pPlayerObjectForMixer->isConnectedToHdmi())
    {
        // get a handle to the output device
        stm_display_device_h pDev;
        stm_display_output_h out;
        status = OpenHdmiDevice(&pDev, &out);

        if (status == PlayerNoError)
        {
            mAudioInfoFrame.downmix_info.u8 = downmix_info;
            StmSeTranslateIntegerSamplingFrequencyToHdmi(sfreq_hz, &mAudioInfoFrame.sample_frequency);
            StmSeAudioAcModeToHdmi(acmod, &mAudioInfoFrame.speaker_mapping, &mAudioInfoFrame.channel_count);

            SE_DEBUG(group_audio_player, "%d channels - acmod : 0x%02X  / speakermap 0x%02X - %d Hz (%d)\n",
                     mAudioInfoFrame.channel_count,
                     acmod,    mAudioInfoFrame.speaker_mapping,
                     sfreq_hz, mAudioInfoFrame.sample_frequency);

            // Because CEA 861-E states :
            //   " NOTE : HDMI requires the CT, SS, and SF fields to be set to 0
            //     ("Refer to Stream Header") when these items are indicated elsewhere."
            // we clear the SF bit from the AudioInfoFrame (these are already indicated
            // in the SPDIF ChannelStatus bits.
            mAudioInfoFrame.sample_frequency = 0;

            status = UpdateHDMIInfoFrame(out, &mAudioInfoFrame);
            CloseHdmiDevice(pDev, out);
        }
    }

    if (status == PlayerNoError)
    {
        // Remember the "queued" values.
        mDownmixInfo       = downmix_info;
        mSamplingFrequency = sfreq_hz;
        mAcMode            = acmod;
    }
    // else we'll try to resend them on next grain

    return status;
}

// the following implementation has been directly inspired from
// $(SDK2_SOURCES)/kernelspace/modules/st/stmfb/linux/kernel/drivers/stm/hdmi/hdmiioctl.c::stmhdmi_set_audio_iframe_data()
PlayerStatus_t Mixer_Player_c::UpdateHDMIInfoFrame(stm_display_output_h output, stm_display_audio_iframe_t *audiocfg)
{
    stm_display_metadata_t *metadata;
    stm_hdmi_info_frame_t  *iframe;
    int                     res;

    metadata = (stm_display_metadata_t *) OS_Malloc(sizeof(stm_display_metadata_t) + sizeof(stm_hdmi_info_frame_t));
    if (metadata == NULL)
    {
        SE_ERROR("Malloc allocation error\n");
        return PlayerError;
    }

    memset(metadata, 0, sizeof(*metadata));

    metadata->size    = sizeof(stm_display_metadata_t) + sizeof(stm_hdmi_info_frame_t);
    metadata->release = (void(*)(struct stm_display_metadata_s *))OS_Free;
    metadata->type    = STM_METADATA_TYPE_AUDIO_IFRAME;

    iframe          = (stm_hdmi_info_frame_t *)&metadata->data[0];

    memset(iframe->data, 0, sizeof(iframe->data));

    iframe->type    = HDMI_AUDIO_INFOFRAME_TYPE;
    iframe->version = HDMI_AUDIO_INFOFRAME_VERSION;
    iframe->length  = HDMI_AUDIO_INFOFRAME_LENGTH;
    iframe->data[1] = ((audiocfg->channel_count - 1) << HDMI_AUDIO_INFOFRAME_CHANNEL_COUNT_SHIFT) & HDMI_AUDIO_INFOFRAME_CHANNEL_COUNT_MASK;
    iframe->data[2] = (audiocfg->sample_frequency << HDMI_AUDIO_INFOFRAME_FREQ_SHIFT) & HDMI_AUDIO_INFOFRAME_FREQ_MASK;
    iframe->data[4] = audiocfg->speaker_mapping;
    iframe->data[5] = audiocfg->downmix_info.u8 & (HDMI_AUDIO_INFOFRAME_LEVELSHIFT_MASK |
                                                   HDMI_AUDIO_INFOFRAME_DOWNMIX_INHIBIT | 0x3 /*LFE level*/);

    res = stm_display_output_queue_metadata(output, metadata);

    if (res == -EBUSY)
    {
        SE_DEBUG(group_audio_player, "flush audio-info-frame metadata queue before pushing new ones\n");

        if (stm_display_output_flush_metadata(output, STM_METADATA_TYPE_AUDIO_IFRAME) == 0)
        {
            res = stm_display_output_queue_metadata(output, metadata);
        }
        else
        {
            SE_ERROR("Unable to flush audio-info-frame metadata\n");
        }
    }

    if (res < 0)
    {
        OS_Free(metadata);
        SE_ERROR("failed to queue HDMI AudioInfoFrame metadata (%d)\n", res);
        return PlayerError;
    }
    return PlayerNoError;
}
