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
#ifndef H_MIXER_SETTINGS_CLASS
#define H_MIXER_SETTINGS_CLASS

#include "report.h"
#include "mixer_pcmproctuning.h"

#undef TRACE_TAG
#define TRACE_TAG   "Mixer_Settings_c"

/// PseudoMixerSettingsStruct is the repository of STKPI controls.
struct MixerOptionStruct
{
    // Mixer controls
    int MasterGrain;
    stm_se_output_frequency_t OutputSfreq;
    struct stm_se_audio_channel_assignment SpeakerConfig;
    // Gain and panning infos
    int PostMixGain;
    int Gain[MIXER_MAX_CLIENTS][SND_PSEUDO_MIXER_CHANNELS];
    int Pan[MIXER_MAX_CLIENTS][SND_PSEUDO_MIXER_CHANNELS];
    int AudioGeneratorGain[MIXER_MAX_AUDIOGEN_INPUTS][SND_PSEUDO_MIXER_CHANNELS];
    int InteractiveGain[SND_PSEUDO_MIXER_INTERACTIVE];
    int InteractivePan[SND_PSEUDO_MIXER_INTERACTIVE][SND_PSEUDO_MIXER_CHANNELS];

    //Flags to record the if the above variables are updated
    bool PostMixGainUpdated;
    bool GainUpdated;
    bool PanUpdated;
    bool InteractiveGainUpdated;
    bool AudioGeneratorGainUpdated;
    bool InteractivePanUpdated;

    // DRC infos
    stm_se_drc_t Drc;
    // Downmix infos
    int Stream_driven_downmix_enable;
    // Master Playback Volume
    int32_t MasterPlaybackVolume;


    struct PcmProcManagerParams_s VolumeManagerParams;
    int                           VolumeManagerAmount;
    struct PcmProcManagerParams_s VirtualizerParams;
    int                           VirtualizerAmount;
    struct PcmProcManagerParams_s UpmixerParams;
    int                           UpmixerAmount;
    struct PcmProcManagerParams_s DialogEnhancerParams;
    int                           DialogEnhancerAmount;

    MixerOptionStruct(void)
        : MasterGrain(SND_PSEUDO_MIXER_DEFAULT_GRAIN)
        , OutputSfreq()
        , SpeakerConfig()
        , PostMixGain(Q3_13_UNITY)
        , Gain()
        , Pan()
        , AudioGeneratorGain()
        , InteractiveGain()
        , InteractivePan()
        , PostMixGainUpdated(false)
        , GainUpdated(false)
        , PanUpdated(false)
        , InteractiveGainUpdated(false)
        , AudioGeneratorGainUpdated(false)
        , InteractivePanUpdated(false)
        , Drc()
        , Stream_driven_downmix_enable(false)
        , MasterPlaybackVolume(0)
        , VolumeManagerParams()
        , VolumeManagerAmount(100)
        , VirtualizerParams()
        , VirtualizerAmount(100)
        , UpmixerParams()
        , UpmixerAmount(100)
        , DialogEnhancerParams()
        , DialogEnhancerAmount(100)
    {
        Reset();  // TODO(pht) same as for mixer_mme: ctor and reset might share code, but ctor shall not call reset
    }

    inline void Reset()
    {
        MasterGrain = SND_PSEUDO_MIXER_DEFAULT_GRAIN;
        PostMixGain = Q3_13_UNITY;
        // Update the flag
        PostMixGainUpdated = true;

        for (uint32_t PcmInputIdx(0); PcmInputIdx < MIXER_MAX_PCM_INPUTS; PcmInputIdx++)
        {
            for (uint32_t ChannelIdx(0); ChannelIdx < SND_PSEUDO_MIXER_CHANNELS; ChannelIdx++)
            {
                Gain[PcmInputIdx][ChannelIdx] = Q3_13_UNITY;
                Pan[PcmInputIdx][ChannelIdx] = Q3_13_UNITY;
                // Update the flag
                GainUpdated = true;
                PanUpdated = true;
            }
        }

        for (uint32_t AudioGeneratorIdx(0); AudioGeneratorIdx < MIXER_MAX_AUDIOGEN_INPUTS; AudioGeneratorIdx++)
        {
            for (uint32_t ChannelIdx(0); ChannelIdx < SND_PSEUDO_MIXER_CHANNELS; ChannelIdx++)
            {
                AudioGeneratorGain[AudioGeneratorIdx][ChannelIdx] = Q3_13_UNITY;
            }
            AudioGeneratorGainUpdated = true;
        }

        for (uint32_t InteractiveIdx(0); InteractiveIdx < SND_PSEUDO_MIXER_INTERACTIVE; InteractiveIdx++)
        {
            InteractiveGain[InteractiveIdx] = Q3_13_UNITY;
            // Update the flag
            InteractiveGainUpdated = true;

            for (uint32_t ChannelIdx(0); ChannelIdx < SND_PSEUDO_MIXER_CHANNELS; ChannelIdx++)
            {
                InteractivePan[InteractiveIdx][ChannelIdx] = Q3_13_UNITY;
            }

            // Update the flag
            InteractivePanUpdated = true;
        }
        {
            Drc.mode = STM_SE_NO_COMPRESSION;
            Drc.cut = Q0_8_MAX; /* 100% */
            Drc.boost = Q0_8_MAX; /* 100% */
        }

        Stream_driven_downmix_enable = 0;
        MasterPlaybackVolume = 0;

        VolumeManagerParams  = PcmProcManagerParams_s();
        VolumeManagerAmount  = 100;
        VirtualizerParams    = PcmProcManagerParams_s();
        VirtualizerAmount    = 100;
        UpmixerParams        = PcmProcManagerParams_s();
        UpmixerAmount        = 100;
        DialogEnhancerParams = PcmProcManagerParams_s();
        DialogEnhancerAmount = 100;


        SE_DEBUG(group_mixer, "<: %p\n", this);
    }

    inline void DebugDump()
    {
        if (SE_IS_VERBOSE_ON(group_mixer) == 0) { return; }

        SE_VERBOSE(group_mixer, "<SETTINGS> PostMixGain = %d\n", PostMixGain);
        SE_VERBOSE(group_mixer, "<SETTINGS> Gain[PrimaryClient] = %d / %d / %d / %d / %d / %d / %d / %d\n",
                   Gain[0][0], Gain[0][1],
                   Gain[0][2], Gain[0][3],
                   Gain[0][4], Gain[0][5],
                   Gain[0][6], Gain[0][7]);
        SE_VERBOSE(group_mixer, "<SETTINGS> Gain[SecondaryClient] = %d / %d / %d / %d / %d / %d / %d / %d\n",
                   Gain[1][0], Gain[1][1],
                   Gain[1][2], Gain[1][3],
                   Gain[1][4], Gain[1][5],
                   Gain[1][6], Gain[1][7]);
        SE_VERBOSE(group_mixer, "<SETTINGS> Pan[SecondaryClient] = %d / %d / %d / %d / %d / %d / %d / %d\n",
                   Pan[1][0], Pan[1][1],
                   Pan[1][2], Pan[1][3],
                   Pan[1][4], Pan[1][5],
                   Pan[1][6], Pan[1][7]);

        for (int i = 0; i < SND_PSEUDO_MIXER_INTERACTIVE; i++)
        {
            SE_VERBOSE(group_mixer, "<SETTINGS> interactive_%d : Gain = %d ; Pan= %d / %d / %d / %d / %d / %d / %d / %d\n",
                       i, InteractiveGain[i],
                       InteractivePan[i][0], InteractivePan[i][1],
                       InteractivePan[i][2], InteractivePan[i][3],
                       InteractivePan[i][4], InteractivePan[i][5],
                       InteractivePan[i][6], InteractivePan[i][7]);
        }

        {
            SE_VERBOSE(group_mixer, "\n<DRC: type= %d / boost-hdr= %d / cut-ldr= %d", Drc.mode, Drc.boost,
                       Drc.cut);
        }

        SE_VERBOSE(group_mixer, "<SETTINGS> Stream_driven_downmix_enable = %d\n", Stream_driven_downmix_enable);
        SE_VERBOSE(group_mixer, "<SETTINGS> Master Playback Volume = %d\n", MasterPlaybackVolume);
    }
};

class Mixer_Settings_c
{
public:
    inline Mixer_Settings_c(void)
        : OutputConfiguration()
        , MixerOptions()
        , MixerOptionsToBeUpdated()
        , OutputConfigurationToBeUpdated()
        , NeedsUpdate(false)
        , Lock()
    {
        OS_InitializeRtMutex(&Lock);
        Reset();
    }

    inline ~Mixer_Settings_c(void)
    {
        OS_TerminateRtMutex(&Lock);
    }

    inline void Reset()
    {
        NeedsUpdate = false;
        memset(&OutputConfiguration, 0, sizeof(OutputConfiguration));
        OutputConfiguration.magic = SND_PSEUDO_MIXER_MAGIC;
        OutputConfiguration.metadata_update = SND_PSEUDO_MIXER_METADATA_UPDATE_NEVER;
        /* switches */
        OutputConfiguration.all_speaker_stereo_enable = 0; /* Off */
        OutputConfiguration.interactive_audio_mode = SND_PSEUDO_MIXER_INTERACTIVE_AUDIO_MODE_3_4;
        /* latency tuning */
        OutputConfiguration.master_latency = 0;
        /* generic spdif meta data */
        /* rely on memset */
        /* fatpipe meta data */
        /* rely on memset */
        /* topological structure */

        strncpy(OutputConfiguration.downstream_topology.card[0].name, "Default",
                sizeof(OutputConfiguration.downstream_topology.card[0].name));
        OutputConfiguration.downstream_topology.card[0].name[
            sizeof(OutputConfiguration.downstream_topology.card[0].name) - 1] = '\0';
        strncpy(OutputConfiguration.downstream_topology.card[0].alsaname, "hw:0,0",
                sizeof(OutputConfiguration.downstream_topology.card[0].alsaname));
        OutputConfiguration.downstream_topology.card[0].alsaname[
            sizeof(OutputConfiguration.downstream_topology.card[0].alsaname) - 1] = '\0';

        OutputConfiguration.downstream_topology.card[0].flags = 0;
        OutputConfiguration.downstream_topology.card[0].max_freq = 48000;
        OutputConfiguration.downstream_topology.card[0].num_channels = 2;

        // Inherit default values for output configuration.
        OutputConfigurationToBeUpdated = OutputConfiguration;

        // Update internals of STKI controls.
        MixerOptions.Reset();
        MixerOptionsToBeUpdated = MixerOptions;

        SE_DEBUG(group_mixer, "<: %p\n", this);
    }

    inline const MixerOptionStruct GetLastOptions()
    {
        MixerOptionStruct LastOptions;
        // Prevent from possible concurrency in output configuration change.
        LockTake();
        LastOptions = Mixer_Settings_c::MixerOptionsToBeUpdated;
        LockRelease();
        return LastOptions;
    }

    inline void SetTobeUpdated(const MixerOptionStruct &MixerOptionsToBeUpdated)
    {
        // Prevent from possible concurrency in output configuration change.
        LockTake();
        // Update STKPI controls.
        Mixer_Settings_c::MixerOptionsToBeUpdated = MixerOptionsToBeUpdated;
        NeedsUpdate = true;
        LockRelease();
    }

    inline void SetTobeUpdated(const snd_pseudo_mixer_settings &OutputConfigurationToBeUpdated)
    {
        // Prevent from possible concurrency in output configuration change.
        LockTake();
        NeedsUpdate = true;
        // Update legacy parameters (for controls not using STKPI).
        Mixer_Settings_c::OutputConfigurationToBeUpdated = OutputConfigurationToBeUpdated;
        LockRelease();
    }

    inline bool CheckAndUpdate()
    {
        bool Status(false);
        // Prevent from possible concurrency in output configuration change.
        LockTake();

        if (true == NeedsUpdate)
        {
            NeedsUpdate = false;
            // Update legacy parameters (for controls not using STKPI) at first.
            OutputConfiguration = OutputConfigurationToBeUpdated;
            // Update STKPI controls.
            MixerOptions = MixerOptionsToBeUpdated;
            Status = true;
            SE_DEBUG(group_mixer, "<: ******* UPDATED *******\n");
        }

        LockRelease();
        return Status;
    }

    // The settings to be used in PlaybackThread() context.
    struct snd_pseudo_mixer_settings OutputConfiguration;
    struct MixerOptionStruct MixerOptions;

private:
    struct MixerOptionStruct MixerOptionsToBeUpdated;
    struct snd_pseudo_mixer_settings OutputConfigurationToBeUpdated;

    bool NeedsUpdate;

    OS_RtMutex_t Lock; // Helps in sharing object between mixer thread and client side.

    inline void LockTake()
    {
        //SE_DEBUG(group_mixer, ">: %p\n", this);
        OS_LockRtMutex(&Lock);
        //SE_DEBUG(group_mixer, "<: %p\n", this);
    }

    inline void LockRelease()
    {
        //SE_DEBUG(group_mixer, ">: %p\n", this);
        OS_UnLockRtMutex(&Lock);
        //SE_DEBUG(group_mixer, "<: %p\n", this);
    }

    DISALLOW_COPY_AND_ASSIGN(Mixer_Settings_c);
};

#endif // H_MIXER_SETTINGS_CLASS
