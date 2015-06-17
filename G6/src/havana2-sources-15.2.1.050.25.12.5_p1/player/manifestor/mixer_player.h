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

#ifndef H_MIXER_PLAYER_CLASS
#define H_MIXER_PLAYER_CLASS

#include "report.h"
#include "audio_conversions.h"
#include "audio_player.h"
#include "mixer_output.h"

#undef TRACE_TAG
#define TRACE_TAG   "Mixer_Player_c"

#include <stm_display.h>
// Source : /stmfb/linux/kernel/drivers/stm/hdmi/stmhdmi.h
//          struct stmhdmiio_audio
// The following structure contents passed using STMHDMIIO_SET_AUDIO_DATA will
// be placed directly into the next audio InfoFrame when audio is sent over the
// HDMI interface.
//
// Those bits mandated to be zero in the HDMI specification will be masked out.
//
// The default is to send all zeros, to match either 2ch L-PCM or
// IEC60958 encoded data streams. If other audio sources are transmitted then
// this information should be set correctly.
//

typedef struct
{
    unsigned char LFELevel       : 2;
    unsigned char Reserved       : 1;
    unsigned char LevelShift     : 4;
    unsigned char DownmixInhibit : 1;
} tDowmixInfoBF;

typedef union
{
    unsigned char u8;     // CEA861 Audio InfoFrame byte 5 (bits 0-1(LFE playback level), bits 3-7 (Level shift value), bit 8 (downmix inhibit))
    tDowmixInfoBF bits;
} uDowmixInfoBF;

typedef struct stm_display_audio_iframe_s
{
    unsigned char channel_count;    // CEA861 Audio InfoFrame byte 1 (bits 0-2)
    unsigned char sample_frequency; // CEA861 Audio InfoFrame byte 2 (bits 2-4)
    unsigned char speaker_mapping;  // CEA861 Audio InfoFrame byte 4
    uDowmixInfoBF downmix_info;
} stm_display_audio_iframe_t;



extern "C" int snprintf(char *buf, long size, const char *fmt, ...) __attribute__((format(printf, 3, 4))) ;

/// Helper for mixer which aggregates player object and corresponding PCM Player.
class Mixer_Player_c
{
public:
    static const int32_t AUDIO_MAX_OUTPUT_ATTENUATION;
    static const uint32_t MIN_FREQ_AC3_ENCODER;
    static const uint32_t MAX_FREQ_AC3_ENCODER;
    static const uint32_t MIN_FREQ_DTS_ENCODER;
    static const uint32_t MAX_FREQ_DTS_ENCODER;
    static const uint32_t MIN_FREQUENCY;

    inline Mixer_Player_c(void)
        : PlayerObjectAttached(NULL)
        , PlayerObjectForMixer("ZZZZZZZ", "hw:X,Y")
        , pPlayerObjectForMixer(&PlayerObjectForMixer)
        , PcmPlayer(NULL)
        , PcmPlayerConfig()
        , mAudioInfoFrame()
        , mSamplingFrequency(0) // Set to 0 to force the emission of an AudioInfoFrame packet upon first commit.
        , mAcMode(ACC_MODE20)
        , mDownmixInfo(0)
    {
        strncpy(&CardName[0], DefaultCardName, sizeof(CardName));
        CardName[sizeof(CardName) - 1] = '\0';
        SE_DEBUG(group_mixer, "<: %p\n", this);
    }

    inline ~Mixer_Player_c(void)
    {
        SE_ASSERT((false == IsPlayerObjectAttached()));
        // Expected that PcmPlayer already deleted.
        SE_DEBUG(group_mixer, "<: %p\n", this);
    }

    PlayerStatus_t CreatePcmPlayer(PcmPlayer_c::OutputEncoding MixerPrimaryCodedDataTypeSPDIF,
                                   PcmPlayer_c::OutputEncoding MixerPrimaryCodedDataTypeHDMI);

    void DeletePcmPlayer();

    inline bool HasPcmPlayer() const
    {
        bool Result;
        SE_ASSERT((true == IsPlayerObjectAttached()));

        if (NULL == PcmPlayer)
        {
            Result = false;
        }
        else
        {
            Result = true;
        }

        //SE_DEBUG(group_mixer,  "<: %s Returning %s\n", GetCardName(), Result ? "true" : "false");
        return Result;
    }

    inline PcmPlayer_c *GetPcmPlayer() const
    {
        SE_ASSERT((true == HasPcmPlayer()));
        //SE_DEBUG(group_mixer, "<: %p %p\n", this, PcmPlayer);
        return PcmPlayer;
    }

    inline Mixer_Output_c &GetPcmPlayerConfig()
    {
        //SE_DEBUG(group_mixer, "<: %p %p\n", this, &PcmPlayerConfig);
        return PcmPlayerConfig;
    }

    inline const PcmPlayerSurfaceParameters_t &GetPcmPlayerConfigSurfaceParameters() const
    {
        //SE_DEBUG(group_mixer,  "<: %s\n", GetCardName());
        return PcmPlayerConfig.SurfaceParameters;
    }

    inline void SetPcmPlayerConfigSurfaceParameters(const PcmPlayerSurfaceParameters_t &SurfaceParameters)
    {
        PcmPlayerConfig.SurfaceParameters = SurfaceParameters;
        SE_DEBUG(group_mixer,  "<: %s\n", GetCardName());
    }

    inline PlayerStatus_t MapPcmPlayerSamples(uint32_t SampleCount, bool NonBlock)
    {
        PlayerStatus_t Status(PlayerError);
        SE_ASSERT((true == HasPcmPlayer()));
        Status = GetPcmPlayer()->MapSamples(SampleCount,
                                            NonBlock,
                                            &PcmPlayerConfig.MappedSamples);
        SE_VERBOSE(group_mixer,  "<: %s Returning %d\n", GetCardName(), Status);
        return Status;
    }

    inline uint32_t GetPcmPlayerConfigSamplesAreaSize() const
    {
        uint32_t BufferSize = PcmPlayerConfig.SurfaceParameters.PeriodSize * PcmPlayerConfig.SurfaceParameters.NumPeriods;
        //SE_DEBUG(group_mixer,  "<: %s Returning %d\n", GetCardName(), BufferSize);
        return BufferSize;
    }

    inline void ResetPcmPlayerMappedSamplesArea()
    {
        SE_ASSERT((true == HasPcmPlayer()));
        uint32_t BufferSize = PcmPlayerConfig.SurfaceParameters.PeriodSize * PcmPlayerConfig.SurfaceParameters.NumPeriods;
        memset(PcmPlayerConfig.MappedSamples, 0, GetPcmPlayer()->SamplesToBytes(BufferSize));
        //SE_DEBUG(group_mixer,  "<: %s\n", GetCardName());
    }

    inline void *GetPcmPlayerMappedSamplesArea() const
    {
        void *MappedSamplesArea(NULL);
        SE_ASSERT((true == HasPcmPlayer()));
        MappedSamplesArea = PcmPlayerConfig.MappedSamples;
        //SE_DEBUG(group_mixer,  "<: %s Returning %p\n", GetCardName(), MappedSamplesArea);
        return MappedSamplesArea;
    }

    inline PlayerStatus_t CommitPcmPlayerMappedSamples(MME_ScatterPage_t *mme_page)
    {
        PlayerStatus_t  Status(PlayerError);
        SE_ASSERT((true == HasPcmPlayer()));
        PcmPlayerConfig.MappedSamples = NULL;
        Status = GetPcmPlayer()->CommitMappedSamples();
        SE_VERBOSE(group_mixer,  "<: %s Returning %d\n", GetCardName(), Status);
        return Status;
    }

    inline void ResetPcmPlayerConfig()
    {
        PcmPlayerConfig.Reset();
        //SE_DEBUG(group_mixer,  "<: %s\n", GetCardName());
    }

    inline void AttachPlayerObject(Audio_Player_c *PlayerObject)
    {
        SE_ASSERT(!(true == IsPlayerObjectAttached()));

        // Attach the given audio player.
        PlayerObjectAttached = PlayerObject;

        snprintf(&CardName[0], sizeof(CardName), "\"%s\" (\"%s\")", PlayerObject->card.name,
                 PlayerObject->card.alsaname);
        CardName[sizeof(CardName) - 1] = '\0';

        // Clone the given audio player for future usage in mixer sub-system.
        PlayerObjectForMixer = *PlayerObjectAttached;

        DebugDump();
        SE_DEBUG(group_mixer, "<: %s\n\n", GetCardName());
    }

    inline bool IsPlayerObjectAttached() const
    {
        bool Result;

        if (NULL == PlayerObjectAttached)
        {
            Result = false;
        }
        else
        {
            Result = true;
        }

        //SE_DEBUG(group_mixer, "<: %p Returning %s\n", this, Result ? "true" : "false");
        return Result;
    }

    inline bool IsMyPlayerObject(Audio_Player_c *PlayerObject) const
    {
        bool Result;

        if (NULL == PlayerObject)
        {
            Result = false;
        }
        else
        {
            Result = (PlayerObjectAttached == PlayerObject);
        }

        //SE_DEBUG(group_mixer, "<: %p Returning %s\n", this, Result ? "true" : "false");
        return Result;
    }

    inline bool CheckAndUpdatePlayerObject()
    {
        bool Result(false);

        if (true == IsPlayerObjectAttached())
        {
            Result = PlayerObjectAttached->CheckAndUpdate(PlayerObjectForMixer);

            if (true == Result)
            {
                DebugDump();
            }
        }

        //SE_DEBUG(group_mixer, "<: %p Returning %s\n", this, Result ? "true" : "false");
        return Result;
    }

    inline const char *GetCardName() const
    {
        //SE_DEBUG(group_mixer, "<: %p %s\n", this, &CardName[0]);
        return &CardName[0];
    }

    inline bool IsConnectedToHdmi(void) const
    {
        bool Result(false);
        SE_ASSERT((true == IsPlayerObjectAttached()));
        Result = pPlayerObjectForMixer->isConnectedToHdmi();
        //SE_DEBUG(group_mixer, "<: %p Returning %s\n", this, Result ? "true" : "false");
        return Result;
    }

    inline bool IsConnectedToSpdif(void) const
    {
        bool Result(false);
        SE_ASSERT((true == IsPlayerObjectAttached()));
        Result = pPlayerObjectForMixer->isConnectedToSpdif();
        //SE_DEBUG(group_mixer, "<: %p Returning %s\n", this, Result ? "true" : "false");
        return Result;
    }
    inline bool IsBypass(void) const
    {
        bool Result(false);
        SE_ASSERT((true == IsPlayerObjectAttached()));
        Result = pPlayerObjectForMixer->isBypass();
        //SE_DEBUG(group_mixer, "<: %p Returning %s\n", this, Result ? "true" : "false");
        return Result;
    }
    inline bool IsBypassSD(void) const
    {
        bool Result(false);
        SE_ASSERT((true == IsPlayerObjectAttached()));
        Result = pPlayerObjectForMixer->isBypassSD();
        //SE_DEBUG(group_mixer, "<: %p Returning %s\n", this, Result ? "true" : "false");
        return Result;
    }

    inline stm_se_player_sink_t GetSinkType(void) const
    {
        SE_ASSERT((true == IsPlayerObjectAttached()));
        //SE_DEBUG(group_mixer, "<: %p Returning\n", this);
        return pPlayerObjectForMixer->getPlayerSinkType();
    }

    inline uint32_t GetCardMaxFreq(void) const
    {
        uint32_t MaxFreq(32000);
        SE_ASSERT((true == IsPlayerObjectAttached()));
        MaxFreq = pPlayerObjectForMixer->card.max_freq;
        //SE_DEBUG(group_mixer, "<: %p Returning %d\n", this, MaxFreq);
        return MaxFreq;
    }

    //struct snd_pseudo_mixer_downstream_card should not be part of interface
    inline struct snd_pseudo_mixer_downstream_card const &GetCard(void) const
    {
        SE_ASSERT((true == IsPlayerObjectAttached()));
        //SE_DEBUG(group_mixer, "<: %p Returning\n", this);
        return pPlayerObjectForMixer->card;
    }

    inline unsigned char GetCardNumberOfChannels(void) const
    {
        unsigned char NumberOfChannels(2);
        SE_ASSERT((true == IsPlayerObjectAttached()));
        NumberOfChannels = pPlayerObjectForMixer->card.num_channels;
        //SE_DEBUG(group_mixer, "<: %s num_channels: %d\n", GetCardName(), NumberOfChannels);
        return NumberOfChannels;
    }

    PlayerStatus_t CheckAndConfigureHDMICell(PcmPlayer_c::OutputEncoding OutputEncoding, SpdifInProperties_t *SpdifInProperties) const
    {
        PlayerStatus_t Status(PlayerError);
        SE_ASSERT((true == IsPlayerObjectAttached()));
        Status = ConfigureHDMICell(OutputEncoding, SpdifInProperties);
        //SE_DEBUG(group_mixer, "<: %p Returning %d\n", this, Status);
        return Status;
    }

    PlayerStatus_t GetOption(stm_se_ctrl_t ctrl, int &value) const
    {
        PlayerStatus_t Status(PlayerError);
        SE_ASSERT((true == IsPlayerObjectAttached()));
        Status = pPlayerObjectForMixer->GetOption(ctrl, &value);
        //SE_DEBUG(group_mixer, "<: %p Returning %d\n", this, Status);
        return Status;
    }

    PlayerStatus_t GetCompoundOption(stm_se_ctrl_t ctrl, void *value) const
    {
        PlayerStatus_t Status(PlayerError);
        SE_ASSERT((true == IsPlayerObjectAttached()));
        Status = pPlayerObjectForMixer->GetCompoundOption(ctrl, value);
        //SE_DEBUG(group_mixer, "<: %p Returning %d\n", this, Status);
        return Status;
    }

    PcmPlayer_c::OutputEncoding LookupOutputEncoding(PcmPlayer_c::OutputEncoding MixerPrimaryCodedDataTypeSPDIF,
                                                     PcmPlayer_c::OutputEncoding MixerPrimaryCodedDataTypeHDMI,
                                                     uint32_t freq = 0) const;

    uint32_t LookupOutputNumberOfChannels(PcmPlayer_c::OutputEncoding MixerPrimaryCodedDataTypeSPDIF,
                                          PcmPlayer_c::OutputEncoding MixerPrimaryCodedDataTypeHDMI) const;

    uint32_t LookupOutputSamplingFrequency(PcmPlayer_c::OutputEncoding OutputEncoding,
                                           uint32_t MixerOutputSamplingFrequency,
                                           uint32_t MixerIec60958FrameRate) const;

    inline void DetachPlayerObject()
    {
        SE_DEBUG(group_mixer, ">: %s\n", GetCardName());
        SE_ASSERT((true == IsPlayerObjectAttached()));
        SE_ASSERT((false == HasPcmPlayer()));
        PlayerObjectAttached = NULL;
        PcmPlayer = NULL;
        // CAUTION: Do not do PcmPlayerConfig.Reset();
        strncpy(&CardName[0], DefaultCardName, sizeof(CardName));
        CardName[sizeof(CardName) - 1] = '\0';
    }

    inline void DebugDump()
    {
        if (SE_IS_VERBOSE_ON(group_mixer) == 0) { return; }

        SE_VERBOSE(group_mixer, ">: %s\n", GetCardName());
        SE_VERBOSE(group_mixer, "@: IsConnectedToHdmi()                 : %s\n", (true == IsConnectedToHdmi()) ? "true" : "false");
        SE_VERBOSE(group_mixer, "@: IsConnectedToSpdif()                : %s\n", (true == IsConnectedToSpdif()) ? "true" : "false");
        SE_VERBOSE(group_mixer, "@: IsBypass()                          : %s\n", (true == IsBypass()) ? "true" : "false");
        SE_VERBOSE(group_mixer, "@: HasPcmPlayer()                      : %s\n", (true == HasPcmPlayer()) ? "true" : "false");
        SE_VERBOSE(group_mixer, "@: GetCardMaxFreq()                    : %d\n", GetCardMaxFreq());
        SE_VERBOSE(group_mixer, "@: GetCardNumberOfChannels()           : %d\n", GetCardNumberOfChannels());
        {
            PcmPlayerSurfaceParameters_t PcmPlayerSurfaceParameters(GetPcmPlayerConfigSurfaceParameters());
            SE_VERBOSE(group_mixer, "@: SampleRateHz                        : %d\n", PcmPlayerSurfaceParameters.PeriodParameters.SampleRateHz);
        }
        SE_VERBOSE(group_mixer, "@: PeriodSize                          : %u\n", GetPcmPlayerConfigSurfaceParameters().PeriodSize);
        SE_VERBOSE(group_mixer, "@: GetPcmPlayerConfigSamplesAreaSize() : %u\n", GetPcmPlayerConfigSamplesAreaSize());
        {
            //stm_se_player_sink_t SinkType(GetSinkType());
        }
        SE_VERBOSE(group_mixer, "<:\n\n");
    }

    PlayerStatus_t UpdateAudioInfoFrame(uint32_t sfreq_hz, enum eAccAcMode acmod , uint8_t downmix_info);

private:

    PlayerStatus_t OpenHdmiDevice(stm_display_device_h *pDev, stm_display_output_h *out) const;
    void           CloseHdmiDevice(stm_display_device_h  pDev, stm_display_output_h  out) const;
    PlayerStatus_t FlushAudioInfoFrame(void);
    PlayerStatus_t UpdateHDMIInfoFrame(stm_display_output_h output, stm_display_audio_iframe_t *audiocfg);
    PlayerStatus_t ConfigureHDMICell(PcmPlayer_c::OutputEncoding OutputEncoding, SpdifInProperties_t *SpdifInProperties) const;

    Audio_Player_c *PlayerObjectAttached;  // The audio player client side.
    Audio_Player_c  PlayerObjectForMixer;  // The audio player, clone of client side, dedicated to mixer (thread) usage.
    const Audio_Player_c *pPlayerObjectForMixer;  // As const pointer to strictly forbid write operation.

    char CardName[50];
    PcmPlayer_c *PcmPlayer;
    Mixer_Output_c PcmPlayerConfig;

    stm_display_audio_iframe_t               mAudioInfoFrame;
    uint32_t                                 mSamplingFrequency;
    uint32_t                                 mAcMode;
    uint8_t                                  mDownmixInfo;

    static const char *DefaultCardName;

    DISALLOW_COPY_AND_ASSIGN(Mixer_Player_c);
};

#endif // H_MIXER_PLAYER_CLASS
