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

#include "osinline.h"
#include "audio_conversions.h"
#include "codec_mme_base.h"
#include "audio_player.h"

#undef TRACE_TAG
#define TRACE_TAG   "Audio_Player_c"

////////////////////////////////////////////////////////////////////////////
///
///
///
Audio_Player_c::Audio_Player_c(const char *name, const char *hw_name)
    : Name()
    , card()
    , hw()
    , enable_iec_sw_formating(false)
    , hdmi_device_id(0)
    , hdmi_output_id(-1)
    , stream_driven_dualmono(STM_SE_CTRL_VALUE_APPLY)
    , dual_mode(STM_SE_STEREO_OUT)
    , limiter_config()
    , bassmgt_config()
    , gain(0)
    , delay(0)
    , soft_mute(false)
    , dc_remove_enable(true)
    , btsc_config()
    , drc_config()
    , VolumeManagerParams()
    , VolumeManagerAmount(100)
    , VirtualizerParams()
    , VirtualizerAmount(100)
    , UpmixerParams()
    , UpmixerAmount(100)
    , DialogEnhancerParams()
    , DialogEnhancerAmount(100)
    , mLock()
    , UpdatedStatus(true)
{
    OS_InitializeRtMutex(&mLock);

    // TODO: This is a hack version of the card name parsing (only accepts hw:X,Y form).
    //       The alsa name handling should be moved into ksound.
    if ((6 != strlen(hw_name)) ||
        (0 != strncmp(hw_name, "hw:", 3)) ||
        (hw_name[3] < '0') ||
        (hw_name[4] != ',') ||
        (hw_name[5] < '0'))
    {
        SE_ERROR("Cannot find '%s', try 'hw:X,Y' instead\n", hw_name);
        return;
    }

    strncpy(&Name[0], hw_name, sizeof(Name));
    Name[sizeof(Name) - 1] = '\0';

    // card specific inits
    strncpy(card.name, name, sizeof(card.name));
    card.name[sizeof(card.name) - 1] = '\0';

    strncpy(card.alsaname, hw_name, sizeof(card.alsaname));
    card.alsaname[sizeof(card.alsaname) - 1] = '\0';

    card.target_level       = 0;
    card.max_freq           = 192000;
    card.channel_assignment.pair0 = STM_SE_AUDIO_CHANNEL_PAIR_L_R;
    card.channel_assignment.pair1 = STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED;
    card.channel_assignment.pair2 = STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED;
    card.channel_assignment.pair3 = STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED;
    card.channel_assignment.pair4 = STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED;
    card.channel_assignment.reserved0 = 0;
    card.channel_assignment.malleable = false;

    // hw specific inits
    hw.player_type      = STM_SE_PLAYER_I2S;
    hw.sink_type        = STM_SE_PLAYER_SINK_AUTO;
    hw.playback_mode    = STM_SE_PLAY_PCM_OUT;
    hw.playback_control = STM_SE_CTRL_VALUE_DISAPPLY; // force playback mode without restriction.
    hw.num_channels     = 2;

    // limiter specific inits
    limiter_config.apply              = STM_SE_CTRL_VALUE_APPLY;
    limiter_config.hard_gain          = false;
    limiter_config.lookahead_enable   = STM_SE_CTRL_VALUE_DISAPPLY;
    limiter_config.lookahead          = 0;

    // bassmgt specific inits
    bassmgt_config.apply               = STM_SE_CTRL_VALUE_APPLY;
    bassmgt_config.type                = STM_SE_BASSMGT_SPEAKER_BALANCE;
    bassmgt_config.boost_out           = false;
    bassmgt_config.ws_out              = 32;
    bassmgt_config.cut_off_frequency   = 100;   // within [50, 200] Hz
    bassmgt_config.filter_order        = 2;     // could be 1 or 2
    //bassmgt_config.gain[8]                    in mB, already init to 0 by memset
    bassmgt_config.delay_enable        = STM_SE_CTRL_VALUE_APPLY;
    //bassmgt_config.delay[8]                   in mB, already init to 0 by memset

    UpdateCard(); // mirror hw config to the card; TODO(pht) move to FinalizeInit + have status
}


////////////////////////////////////////////////////////////////////////////
///
///
///
Audio_Player_c::~Audio_Player_c()
{
    OS_TerminateRtMutex(&mLock);
}


////////////////////////////////////////////////////////////////////////////
/// audio_player_type_flags array is only used to maintain legacy with HAVANA
/// implementation of card.flags
/// ToDO : replace throughout the code the use of card.flags by a call to
///        audio_player->EnableIecSWFormating()
const unsigned int audio_player_type_flags[4] =
{
    0,
    SND_PSEUDO_TOPOLOGY_FLAGS_ENABLE_SPDIF_FORMATING,
    SND_PSEUDO_TOPOLOGY_FLAGS_ENABLE_HDMI_FORMATING,
    SND_PSEUDO_TOPOLOGY_FLAGS_ENABLE_SPDIF_FORMATING + SND_PSEUDO_TOPOLOGY_FLAGS_ENABLE_HDMI_FORMATING
};

void Audio_Player_c::UpdateCard(void)
{
    //SE_DEBUG(group_audio_player, ">:%p\n", this);
    card.flags       |= (enable_iec_sw_formating) ? audio_player_type_flags[hw.player_type] : 0;
    // Card description
    card.num_channels = hw.num_channels;
    SE_DEBUG(group_audio_player, ">:%p num_channels: %d\n", this, card.num_channels);

    // Associate display for HDMI
    if (isConnectedToHdmi())
    {
        uint32_t              id = 0;
        stm_display_device_h  device;
        int                   error;

        do
        {
            error = stm_display_open_device(id, &device);
            if (error == 0)
            {
                stm_display_output_h out;
                hdmi_device_id = id;
                id = 0;

                while (stm_display_device_open_output(device, id++, &out) == 0)
                {
                    uint32_t caps;
                    if (stm_display_output_get_capabilities(out, &caps) < 0)
                    {
                        SE_ERROR("0x%p Not able to retrieve display capabilities\n", this);
                    }
                    stm_display_output_close(out);

                    if ((caps & OUTPUT_CAPS_HDMI) != 0)
                    {
                        hdmi_output_id = (id - 1);
                        break;
                    }
                }

                stm_display_device_close(device);
            }
        }
        while (error != -ENODEV);
    }
}

PlayerStatus_t Audio_Player_c::SetCompoundOption(stm_se_ctrl_t ctrl, const void *value)
{
    PlayerStatus_t status = PlayerNoError;
    SE_DEBUG(group_audio_player, "[@:%p]>> ctrl %d ", this, ctrl);

    switch (ctrl)
    {
    case STM_SE_CTRL_AUDIO_PLAYER_HARDWARE_MODE:
    {
        SE_DEBUG(group_audio_player, "%p STM_SE_CTRL_AUDIO_PLAYER_HARDWARE_MODE\n", this);
        stm_se_ctrl_audio_player_hardware_mode_t *hw_mode = (stm_se_ctrl_audio_player_hardware_mode_t *)value;

        // check parameters validity
        if (hw.playback_mode != hw_mode->playback_mode)
        {
            // trying to modify the playback_mode
            if ((STM_SE_PLAYER_I2S == hw_mode->player_type) &&
                (STM_SE_PLAY_PCM_OUT != hw_mode->playback_mode) &&
                (STM_SE_PLAY_BTSC_OUT != hw_mode->playback_mode))
            {
                // I2S output only supports PCM or BTSC modes
                SE_ERROR("[%p] Playback Mode (%d) not supported by this Audio-Player\n", this, hw_mode->playback_mode);
                status = PlayerError; // Not supported control
            }

            if ((STM_SE_PLAYER_I2S != hw_mode->player_type) &&
                (STM_SE_PLAY_BTSC_OUT == hw_mode->playback_mode))
            {
                // BTSC mode is supported by I2S output only
                SE_ERROR("[%p] Playback Mode (%d) not supported by this Audio-Player\n", this, hw_mode->playback_mode);
                status = PlayerError; // Not supported control
            }
        }

        if ((STM_SE_PLAYER_SPDIF == hw_mode->player_type) && (hw_mode->num_channels > 2))
        {
            SE_ERROR("Invalid parameter value for STM_SE_PLAYER_SPDIF, supports 2 channel only\n");
            UpdatedStatus = false;
            return PlayerError;
        }

        if (PlayerError != status)
        {
            LockTake();
            // Copy the whole compound control
            hw = *hw_mode;
            SE_DEBUG(group_audio_player, "%p STM_SE_CTRL_AUDIO_PLAYER_HARDWARE_MODE %d\n", this, hw.playback_mode);

            // Flags IEC Formating
            // TODO C.Fenard to be reviewed.
            switch (hw.player_type)
            {
            case STM_SE_PLAYER_SPDIF:
            case STM_SE_PLAYER_SPDIF_HDMI:
            {
                enable_iec_sw_formating = true; //(audio_player_enable_spdif_sw_formating != 0)
            }
            break;

            case STM_SE_PLAYER_HDMI:
            {
                enable_iec_sw_formating = true; //(audio_player_enable_hdmi_sw_formating != 0)
            }
            break;

            default:
                enable_iec_sw_formating = false;
            }

            //  clear the channel assignment of the non present channels (which is set by another control)
            switch (hw_mode->num_channels)
            {
            case 0:
            case 1:
                card.channel_assignment.pair0 = STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED;

            // no break : fall through
            case 2:
            case 3:
                card.channel_assignment.pair1 = STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED;

            // no break : fall through
            case 4:
            case 5:
                card.channel_assignment.pair2 = STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED;

            // no break : fall through
            case 6:
            case 7:
                card.channel_assignment.pair3 = STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED;

            // no break : fall through
            default:
                card.channel_assignment.pair4 = STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED;
                break;
            }

            UpdateCard();
            UpdatedStatus = true;
            LockRelease();
        }
    }
    break;

    case STM_SE_CTRL_SPEAKER_CONFIG:
    {
        SE_DEBUG(group_audio_player, "%p STM_SE_CTRL_SPEAKER_CONFIG\n", this);
        struct stm_se_audio_channel_assignment *speaker_config = (struct stm_se_audio_channel_assignment *)value;
        LockTake();
        card.channel_assignment = *speaker_config;
        UpdatedStatus = true;
        LockRelease();
        SE_DEBUG(group_audio_player, "%p player channel assignment [pair0:%d]-[pair1:%d]-[pair2:%d]-[pair3:%d]-[pair4:%d] malleable=%d\n",
                 this,
                 card.channel_assignment.pair0,
                 card.channel_assignment.pair1,
                 card.channel_assignment.pair2,
                 card.channel_assignment.pair3,
                 card.channel_assignment.pair4,
                 card.channel_assignment.malleable);
    }
    break;

    case STM_SE_CTRL_LIMITER:
    {
        SE_DEBUG(group_audio_player, "%p STM_SE_CTRL_LIMITER\n", this);
        stm_se_limiter_t *limiter = (stm_se_limiter_t *)value;
        LockTake();
        limiter_config = *limiter;
        UpdatedStatus = true;
        LockRelease();
    }
    break;

    case STM_SE_CTRL_BASSMGT:
    {
        SE_DEBUG(group_audio_player, "%p STM_SE_CTRL_BASSMGT\n", this);
        stm_se_bassmgt_t *bassmgt = (stm_se_bassmgt_t *)value;

        // check ranges
        for (uint32_t idx(0); idx < SND_PSEUDO_MIXER_CHANNELS; idx++)
        {
            // check gain
            if (bassmgt->gain[idx] > 0)
            {
                SE_ERROR("[%p] BassMgt gain out of bounds (%d)\n", this, bassmgt->gain[idx]);
                bassmgt->gain[idx] = 0;
            }
            else if (bassmgt->gain[idx] < -9600)
            {
                SE_ERROR("[%p] BassMgt gain out of bounds (%d)\n", this, bassmgt->gain[idx]);
                bassmgt->gain[idx] = -9600;
            }

            // check delay
            if (bassmgt->delay[idx] > 30)
            {
                SE_ERROR("[%p] BassMgt delay out of bounds (%d)\n", this, bassmgt->delay[idx]);
                bassmgt->delay[idx] = 30;
            }
        }

        SE_DEBUG(group_audio_player, "%p STM_SE_CTRL_BASSMGT: gain (apply=%d) [%d][%d]-[%d][%d]-[%d][%d]-[%d][%d]\n", this,
                 bassmgt->apply,
                 bassmgt->gain[0], bassmgt->gain[1], bassmgt->gain[2], bassmgt->gain[3],
                 bassmgt->gain[4], bassmgt->gain[5], bassmgt->gain[6], bassmgt->gain[7]);
        SE_DEBUG(group_audio_player, "%p STM_SE_CTRL_BASSMGT: delay [%d][%d]-[%d][%d]-[%d][%d]-[%d][%d]\n", this,
                 bassmgt->delay[0], bassmgt->delay[1], bassmgt->delay[2], bassmgt->delay[3],
                 bassmgt->delay[4], bassmgt->delay[5], bassmgt->delay[6], bassmgt->delay[7]);
        LockTake();
        bassmgt_config = *bassmgt;
        UpdatedStatus = true;
        LockRelease();
    }
    break;

    case STM_SE_CTRL_BTSC:
    {
        SE_DEBUG(group_audio_player, "%p STM_SE_CTRL_BTSC\n", this);
        stm_se_btsc_t *btsc = (stm_se_btsc_t *)value;
        SE_DEBUG(group_audio_player, "%p STM_SE_CTRL_BTSC: dual_signal = %d, "
                 "tx_gain = 0x%x, input_gain = 0x%x\n",
                 this, btsc->dual_signal, btsc->tx_gain, btsc->input_gain);
        LockTake();
        btsc_config = *btsc;
        UpdatedStatus = true;
        LockRelease();
    }
    break;

    case STM_SE_CTRL_AUDIO_DYNAMIC_RANGE_COMPRESSION:
    {
        SE_DEBUG(group_audio_player, "%p STM_SE_CTRL_AUDIO_DYNAMIC_RANGE_COMPRESSION\n", this);
        stm_se_drc_t *drc = (stm_se_drc_t *) value;
        SE_DEBUG(group_audio_player, "Set drc record: mode=%d cut=%d boost=%d\n", drc->mode, drc->cut, drc->boost);
        LockTake();
        drc_config = *drc;
        UpdatedStatus = true;
        LockRelease();
    }
    break;

    case STM_SE_CTRL_OUTPUT_SAMPLING_FREQUENCY:
    {
        SE_DEBUG(group_audio_player, "%p STM_SE_CTRL_OUTPUT_SAMPLING_FREQUENCY\n", this);
        stm_se_output_frequency_t *output_freq = (stm_se_output_frequency_t *)value;

        if (STM_SE_MAX_OUTPUT_FREQUENCY != output_freq->control)
        {
            SE_ERROR("Invalid parameter value for Audio-Player, supports STM_SE_MAX_OUTPUT_FREQUENCY only\n");
        }

        LockTake();
        card.max_freq = output_freq->frequency;
        UpdatedStatus = true;
        LockRelease();
        SE_DEBUG(group_audio_player, "set card max_freq to %d\n", card.max_freq);
    }
    break;

    case STM_SE_CTRL_VOLUME_MANAGER:
    {
        LockTake();
        SetMixerTuningProfile((struct MME_PcmProcParamsHeader_s *) value, &VolumeManagerParams);
        UpdatedStatus = true;
        LockRelease();
    }
    break;

    case STM_SE_CTRL_VIRTUALIZER:
    {
        LockTake();
        SetMixerTuningProfile((struct MME_PcmProcParamsHeader_s *) value, &VirtualizerParams);
        UpdatedStatus = true;
        LockRelease();
    }
    break;

    case STM_SE_CTRL_UPMIXER:
    {
        LockTake();
        SetMixerTuningProfile((struct MME_PcmProcParamsHeader_s *) value, &UpmixerParams);
        UpdatedStatus = true;
        LockRelease();
    }
    break;

    case STM_SE_CTRL_DIALOG_ENHANCER:
    {
        LockTake();
        SetMixerTuningProfile((struct MME_PcmProcParamsHeader_s *) value, &DialogEnhancerParams);
        UpdatedStatus = true;
        LockRelease();
    }
    break;

    default:
        status = PlayerError; // Not supported control
    }

    SE_DEBUG(group_audio_player, "[status = %d] <<\n", status);
    return status;
}

PlayerStatus_t Audio_Player_c::GetCompoundOption(stm_se_ctrl_t ctrl, void *value) const
{
    PlayerStatus_t status = PlayerNoError;
    SE_DEBUG(group_audio_player, "[@:%p]>> ctrl %d\n", this, ctrl);

    switch (ctrl)
    {
    case STM_SE_CTRL_AUDIO_PLAYER_HARDWARE_MODE:
    {
        stm_se_ctrl_audio_player_hardware_mode_t *hw_mode = (stm_se_ctrl_audio_player_hardware_mode_t *) value;
        // Copy the whole compound control
        *hw_mode = hw;
    }
    break;

    case STM_SE_CTRL_SPEAKER_CONFIG:
    {
        struct stm_se_audio_channel_assignment *speaker_config = (struct stm_se_audio_channel_assignment *) value;
        *speaker_config = card.channel_assignment;
        break;
    }

    case STM_SE_CTRL_LIMITER:
    {
        stm_se_limiter_t *limiter = (stm_se_limiter_t *) value;
        *limiter = limiter_config;
        break;
    }

    case STM_SE_CTRL_BASSMGT:
    {
        stm_se_bassmgt_t  *bassmgt = (stm_se_bassmgt_t *) value;
        *bassmgt = bassmgt_config;
        break;
    }

    case STM_SE_CTRL_BTSC:
    {
        stm_se_btsc_t  *btsc = (stm_se_btsc_t *) value;
        *btsc = btsc_config;
        SE_DEBUG(group_audio_player, "%p STM_SE_CTRL_BTSC: dual_signal = %d, "
                 "tx_gain = 0x%x, input_gain = 0x%x\n",
                 this, btsc->dual_signal, btsc->tx_gain, btsc->input_gain);
        break;
    }

    case STM_SE_CTRL_AUDIO_DYNAMIC_RANGE_COMPRESSION:
    {
        stm_se_drc_t *drc = (stm_se_drc_t *) value;
        *drc = drc_config;
        SE_DEBUG(group_audio_player, "Set drc record: mode=%d cut=%d boost=%d\n", drc->mode, drc->cut, drc->boost);
        break;
    }

    case STM_SE_CTRL_OUTPUT_SAMPLING_FREQUENCY:
    {
        stm_se_output_frequency_t *output_freq = (stm_se_output_frequency_t *) value;
        output_freq->control = STM_SE_MAX_OUTPUT_FREQUENCY; // always this value for audio-players
        output_freq->frequency = card.max_freq;
        break;
    }

    case STM_SE_CTRL_VOLUME_MANAGER:
    {
        struct PcmProcManagerParams_s **pVolumeManagerParams = (struct PcmProcManagerParams_s **) value;
        *pVolumeManagerParams = (struct PcmProcManagerParams_s *) &VolumeManagerParams;
        break;
    }

    case STM_SE_CTRL_VIRTUALIZER:
    {
        struct PcmProcManagerParams_s **pVirtualizerParams = (struct PcmProcManagerParams_s **) value;
        *pVirtualizerParams = (struct PcmProcManagerParams_s *) &VirtualizerParams;
        break;
    }

    case STM_SE_CTRL_UPMIXER:
    {
        struct PcmProcManagerParams_s **pUpmixerParams = (struct PcmProcManagerParams_s **) value;
        *pUpmixerParams = (struct PcmProcManagerParams_s *) &UpmixerParams;
        break;
    }

    case STM_SE_CTRL_DIALOG_ENHANCER:
    {
        struct PcmProcManagerParams_s **pDialogEnhancerParams = (struct PcmProcManagerParams_s **) value;
        *pDialogEnhancerParams = (struct PcmProcManagerParams_s *) &DialogEnhancerParams;
        break;
    }

    default:
        status = PlayerError; // Not supported control
        break;
    }

    SE_DEBUG(group_audio_player, "[status = %d] <<\n", status);
    return status;
}

////////////////////////////////////////////////////////////////////////////
///
///
///
PlayerStatus_t Audio_Player_c::SetOption(stm_se_ctrl_t ctrl, const int value)
{
    PlayerStatus_t status(PlayerNoError);
    SE_DEBUG(group_audio_player, "[@:%p]>> ctrl %d = %d\n", this, ctrl, value);

    switch (ctrl)
    {
    case STM_SE_CTRL_AUDIO_GAIN:
    {
        SE_DEBUG(group_audio_player, ">: %p STM_SE_CTRL_AUDIO_GAIN\n", this);
        LockTake();
        gain = value;
        UpdatedStatus = true;
        LockRelease();
    }
    break;

    case STM_SE_CTRL_AUDIO_DELAY:
    {
        SE_DEBUG(group_audio_player, ">: %p STM_SE_CTRL_AUDIO_DELAY\n", this);
        LockTake();
        delay = value;
        UpdatedStatus = true;
        LockRelease();
    }
    break;

    case STM_SE_CTRL_AUDIO_SOFTMUTE:
    {
        SE_DEBUG(group_audio_player, ">: %p STM_SE_CTRL_AUDIO_SOFTMUTE\n", this);
        LockTake();
        soft_mute = value;
        UpdatedStatus = true;
        LockRelease();
    }
    break;

    case STM_SE_CTRL_AUDIO_PROGRAM_PLAYBACK_LEVEL:
    {
        SE_DEBUG(group_audio_player, ">: %p STM_SE_CTRL_AUDIO_PROGRAM_PLAYBACK_LEVEL\n", this);
        LockTake();
        card.target_level = value;
        UpdatedStatus = true;
        LockRelease();
    }
    break;

    case STM_SE_CTRL_DUALMONO:
    {
        SE_DEBUG(group_audio_player, ">: %p STM_SE_CTRL_DUALMONO\n", this);

        if (value <= STM_SE_MONO_OUT)
        {
            LockTake();
            dual_mode = (stm_se_dual_mode_t)value;
            UpdatedStatus = true;
            LockRelease();
        }
        else
        {
            SE_ERROR("Value not in range for STM_SE_CTRL_DUALMONO\n");
            status = PlayerError;
        }
    }
    break;

    case STM_SE_CTRL_STREAM_DRIVEN_DUALMONO:
    {
        SE_DEBUG(group_audio_player, ">: %p STM_SE_CTRL_STREAM_DRIVEN_DUALMONO\n", this);

        if ((STM_SE_CTRL_VALUE_DISAPPLY == value) || (STM_SE_CTRL_VALUE_APPLY == value))
        {
            LockTake();
            stream_driven_dualmono = value;
            UpdatedStatus = true;
            LockRelease();
        }
        else
        {
            SE_ERROR("Value not in range for STM_SE_CTRL_STREAM_DRIVEN_DUALMONO\n");
            status = PlayerError;
        }
    }
    break;

    case STM_SE_CTRL_DCREMOVE:
    {
        SE_DEBUG(group_audio_player, ">: %p STM_SE_CTRL_DCREMOVE\n", this);
        LockTake();
        dc_remove_enable = value;
        UpdatedStatus = true;
        LockRelease();
    }
    break;

    case STM_SE_CTRL_VOLUME_MANAGER_AMOUNT:
    {
        SE_DEBUG(group_audio_player, ">: %p STM_SE_CTRL_VOLUME_MANAGER_AMOUNT\n", this);
        LockTake();
        VolumeManagerAmount = value;
        UpdatedStatus = true;
        LockRelease();
    }
    break;

    case STM_SE_CTRL_VIRTUALIZER_AMOUNT:
    {
        SE_DEBUG(group_audio_player, ">: %p STM_SE_CTRL_VIRTUALIZER_AMOUNT\n", this);
        LockTake();
        VirtualizerAmount = value;
        UpdatedStatus = true;
        LockRelease();
    }
    break;

    case STM_SE_CTRL_UPMIXER_AMOUNT:
    {
        SE_DEBUG(group_audio_player, ">: %p STM_SE_CTRL_UPMIXER_AMOUNT\n", this);
        LockTake();
        UpmixerAmount = value;
        UpdatedStatus = true;
        LockRelease();
    }
    break;

    case STM_SE_CTRL_DIALOG_ENHANCER_AMOUNT:
    {
        SE_DEBUG(group_audio_player, ">: %p STM_SE_CTRL_DIALOG_ENHANCER_AMOUNT\n", this);
        LockTake();
        DialogEnhancerAmount = value;
        UpdatedStatus = true;
        LockRelease();
    }
    break;

    default:
    {
        status = PlayerError; // Not supported control
    }
    break;
    }

    SE_DEBUG(group_audio_player, "[status = %d] <<\n", status);
    return status;
}

////////////////////////////////////////////////////////////////////////////
///
///
///
PlayerStatus_t Audio_Player_c::GetOption(stm_se_ctrl_t ctrl, int *value) const
{
    PlayerStatus_t status = PlayerNoError;
    SE_DEBUG(group_audio_player, "[@:%p]>> ctrl %d ", this, ctrl);

    switch (ctrl)
    {
    case STM_SE_CTRL_AUDIO_GAIN:
        *value = gain;
        break;

    case STM_SE_CTRL_AUDIO_DELAY:
        *value = delay;
        break;

    case STM_SE_CTRL_AUDIO_SOFTMUTE:
        *value = soft_mute;
        break;

    case STM_SE_CTRL_AUDIO_PROGRAM_PLAYBACK_LEVEL:
        *value = card.target_level;
        break;

    case STM_SE_CTRL_DUALMONO:
        *value = dual_mode;
        break;

    case STM_SE_CTRL_STREAM_DRIVEN_DUALMONO:
        *value = stream_driven_dualmono;
        break;

    case STM_SE_CTRL_DCREMOVE:
        *value = dc_remove_enable;
        break;

    case STM_SE_CTRL_VOLUME_MANAGER_AMOUNT:
        *value = VolumeManagerAmount;
        break;

    case STM_SE_CTRL_VIRTUALIZER_AMOUNT:
        *value = VirtualizerAmount;
        break;

    case STM_SE_CTRL_UPMIXER_AMOUNT:
        *value = UpmixerAmount;
        break;

    case STM_SE_CTRL_DIALOG_ENHANCER_AMOUNT:
        *value = DialogEnhancerAmount;
        break;

    default:
        status = PlayerError; // Not supported control
    }

    SE_DEBUG(group_audio_player, "[status = %d] <<\n", status);
    return status;
}
