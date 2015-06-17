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

#ifndef H_AUDIO_PLAYER_CLASS
#define H_AUDIO_PLAYER_CLASS

#include "osinline.h"
#include <stm_display.h>
#include "pcmplayer.h"
#include "player_types.h"
#include "player.h"
#include "mixer_pcmproctuning.h"

#undef TRACE_TAG
#define TRACE_TAG   "Audio_Player_c"

////////////////////////////////////////////////////////////////////////////
///
/// Abstract audio player interface definition.
///
class Audio_Player_c
{
public:
    char Name[32];
    struct snd_pseudo_mixer_downstream_card card;

    Audio_Player_c(const char *, const char *);
    ~Audio_Player_c(void);

    inline bool isConnectedToHdmi(void) const
    {
        return ((hw.player_type == STM_SE_PLAYER_HDMI) || (hw.player_type == STM_SE_PLAYER_SPDIF_HDMI));
    }

    inline bool isConnectedToSpdif(void) const
    {
        return ((hw.player_type == STM_SE_PLAYER_SPDIF) || (hw.player_type == STM_SE_PLAYER_SPDIF_HDMI));
    }

    inline bool isStereo(void) const
    {
        bool is_stereo = (hw.num_channels == 2);

        if ((card.channel_assignment.pair1 == STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED) &&
            (card.channel_assignment.pair2 == STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED) &&
            (card.channel_assignment.pair3 == STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED)
           )
        {
            is_stereo = true;
        }

        return is_stereo;
    }

    inline bool enableIecSWFormating(void) const
    {
        return enable_iec_sw_formating;
    }

    inline stm_se_player_sink_t getPlayerSinkType(void) const
    {
        return hw.sink_type;
    }

    inline bool CheckAndUpdate(Audio_Player_c &AudioPlayer)
    {
        bool Result(false);
        LockTake();

        if (true == UpdatedStatus)
        {
            Result = true;
            // Clone the given audio player for future usage in mixer sub-system.
            // Pay attention to assignment operator.
            AudioPlayer = *this;
            // Acknowledge audio player.
            UpdatedStatus = false;
            SE_DEBUG(group_audio_player, "<: %p ******* UPDATED *******\n", &AudioPlayer);
        }

        LockRelease();
        SE_EXTRAVERB(group_audio_player, "<: %p Returning %s\n", this, Result ? "true" : "false");
        return Result;
    }

    inline bool isBypass(void) const
    {
        //SE_INFO(group_audio_player, "%p %d\n", this, hw.playback_mode);
        return (STM_SE_PLAY_MAIN_COMPRESSED_OUT    == hw.playback_mode ||
                STM_SE_PLAY_MAIN_COMPRESSED_SD_OUT == hw.playback_mode);
    }

    inline bool isBypassSD(void) const
    {
        //SE_INFO(group_audio_player, "%p %d\n", this, hw.playback_mode);
        return (STM_SE_PLAY_MAIN_COMPRESSED_SD_OUT == hw.playback_mode);
    }

    uint32_t GetHdmiDeviceId() const
    {
        return hdmi_device_id;
    };
    int32_t  GetHdmiOutputId() const
    {
        return hdmi_output_id;
    }

    PlayerStatus_t SetCompoundOption(stm_se_ctrl_t ctrl, const void *value);
    PlayerStatus_t GetCompoundOption(stm_se_ctrl_t ctrl, void *value) const;
    PlayerStatus_t SetOption(stm_se_ctrl_t ctrl, const int value);
    PlayerStatus_t GetOption(stm_se_ctrl_t ctrl, int *value) const;

    Audio_Player_c &operator= (const Audio_Player_c &Other)
    {
        if (this == &Other)
        {
            return *this;
        }

        SE_DEBUG(group_audio_player, ">: %p\n", this);

        //Do not clone Mutex : lock and copy its own mutex
        LockTake();
        OS_RtMutex_t lock = mLock;

        // Overwrite the whole object
        memcpy(this, &Other, sizeof(*this));
        this->Name[sizeof(this->Name) - 1] = '\0';

        // restore its own mutex and unlock
        mLock = lock;
        LockRelease();

        SE_DEBUG(group_audio_player, "<: %p\n", this);

        return *this;
    }

private:
    stm_se_ctrl_audio_player_hardware_mode_t hw;
    bool                                     enable_iec_sw_formating;
    uint32_t                                 hdmi_device_id;
    int32_t                                  hdmi_output_id;

    uint32_t                                 stream_driven_dualmono;
    stm_se_dual_mode_t                       dual_mode;

    stm_se_limiter_t                         limiter_config;
    stm_se_bassmgt_t                         bassmgt_config;
    int32_t                                  gain;
    uint32_t                                 delay;
    bool                                     soft_mute;
    bool                                     dc_remove_enable;

    stm_se_btsc_t                            btsc_config;
    stm_se_drc_t                             drc_config;

    struct PcmProcManagerParams_s            VolumeManagerParams;
    int                                      VolumeManagerAmount;
    struct PcmProcManagerParams_s            VirtualizerParams;
    int                                      VirtualizerAmount;
    struct PcmProcManagerParams_s            UpmixerParams;
    int                                      UpmixerAmount;
    struct PcmProcManagerParams_s            DialogEnhancerParams;
    int                                      DialogEnhancerAmount;

    OS_RtMutex_t                             mLock; // Helps in sharing object between mixer thread and client side especially for usage of CheckAndUpdate().
    bool                                     UpdatedStatus;

    void UpdateCard(void);

    inline void LockTake()
    {
        SE_EXTRAVERB(group_audio_player, ">: %p\n", this);
        OS_LockRtMutex(&mLock);
        SE_EXTRAVERB(group_audio_player, "<: %p\n", this);
    }

    inline void LockRelease()
    {
        SE_EXTRAVERB(group_audio_player, ">: %p\n", this);
        OS_UnLockRtMutex(&mLock);
        SE_EXTRAVERB(group_audio_player, "<: %p\n", this);
    }

    // disallow copy ctor;
    // operator= is overloaded and public so dont use DISALLOW_COPY_AND_ASSIGN
    Audio_Player_c(const Audio_Player_c &);
};

#endif // H_AUDIO_PLAYER_CLASS
