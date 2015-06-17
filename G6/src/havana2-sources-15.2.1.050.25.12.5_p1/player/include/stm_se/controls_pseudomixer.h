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

#ifndef STM_SE_CONTROLS_PSEUDOMIXER_H_
#define STM_SE_CONTROLS_PSEUDOMIXER_H_

/*!
 * \defgroup pseudo_mixer_interfaces pseudo mixer interfaces
 *
 * @{
 */

/// Used as an internal convenience to facilitate audio mixer get/set compound controls
typedef union
{
    stm_se_q3dot13_input_gain_t     input_gain;
    stm_se_q3dot13_output_gain_t    output_gain;
    stm_se_drc_t                    drc;
    stm_se_output_frequency_t       output_sfreq;
    stm_se_audio_channel_assignment_t speaker_config;
    stm_se_limiter_t                          limiter;
    stm_se_ctrl_audio_player_hardware_mode_t  player_hardware_mode;
    stm_se_bassmgt_t                          bassmgt;
    stm_se_btsc_t                             btsc;
} stm_se_audio_mixer_value_t;

/*! \name Audio mixer role values */
//!\{
#define STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_MASTER      0 // not used in SE; one occurence in P-M
#define STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_SLAVE       1 // not used anywhere

#define STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_PRIMARY     0
#define STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_SECONDARY   1
#define STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_IN3         2
#define STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_IN4         3

#define STM_SE_CTRL_VALUE_MIXER_ROLE_AUDIO_GENERATOR_0 4
#define STM_SE_CTRL_VALUE_MIXER_ROLE_AUDIO_GENERATOR_1 5
#define STM_SE_CTRL_VALUE_MIXER_ROLE_AUDIO_GENERATOR_2 6
#define STM_SE_CTRL_VALUE_MIXER_ROLE_AUDIO_GENERATOR_3 7
#define STM_SE_CTRL_VALUE_MIXER_ROLE_AUDIO_GENERATOR_4 8
#define STM_SE_CTRL_VALUE_MIXER_ROLE_AUDIO_GENERATOR_5 9
#define STM_SE_CTRL_VALUE_MIXER_ROLE_AUDIO_GENERATOR_6 10
#define STM_SE_CTRL_VALUE_MIXER_ROLE_AUDIO_GENERATOR_7 11

#define STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_SFX0       16
#define STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_SFX1       17
#define STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_SFX2       18
#define STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_SFX3       19
#define STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_SFX4       20
#define STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_SFX5       21
#define STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_SFX6       22
#define STM_SE_CTRL_VALUE_AUDIO_MIXER_ROLE_SFX7       23
//!\}

typedef void *component_handle_t;
/// API used by pseudo mixer to set mixer parameters
int  stm_se_component_set_module_parameters(component_handle_t      Component,
                                            void                    *Data,
                                            unsigned int            Size);

// >>> start of pseudo_mixer.h copy - also includes some linux types at the moment;

#define SND_PSEUDO_MIXER_ADJUST_GRAIN(x)        ((x + 127) & 0xFF80) // round to 128
#define SND_PSEUDO_MIXER_MIN_GRAIN               512
#define SND_PSEUDO_MIXER_MAX_GRAIN              1536
#define SND_PSEUDO_MIXER_DEFAULT_GRAIN           768

#define SND_PSEUDO_MIXER_CHANNELS           8
#define SND_PSEUDO_MIXER_INTERACTIVE        8
#define SND_PSEUDO_MIXER_MAGIC              0xf051

#define SND_PSEUDO_TRANSFORMER_NAME_MAGIC   0xf052

/** Maximum number of supported outputs (subordinate ALSA soundcards) */
#define SND_PSEUDO_MAX_OUTPUTS              4

#define Q3_13_MIN               0
#define Q3_13_M3DB              0x16A7  // ( 2^13 * 10^(-3/20))
#define Q3_13_UNITY             ((1 << 13) - 1)
#define Q3_13_MAX               0xffff

#define Q0_8_MIN                0
#define Q0_8_UNITY              ((1 << 8) -1 )
#define Q0_8_MAX                0xFF

enum snd_pseudo_mixer_metadata_update
{
    SND_PSEUDO_MIXER_METADATA_UPDATE_NEVER,
    SND_PSEUDO_MIXER_METADATA_UPDATE_PRIMARY_AND_SECONDARY_ONLY,  // never used
    SND_PSEUDO_MIXER_METADATA_UPDATE_ALWAYS,
};

enum snd_pseudo_mixer_spdif_encoding
{
    SND_PSEUDO_MIXER_SPDIF_ENCODING_PCM,
    SND_PSEUDO_MIXER_SPDIF_ENCODING_AC3,
    SND_PSEUDO_MIXER_SPDIF_ENCODING_DTS,
    SND_PSEUDO_MIXER_SPDIF_ENCODING_FATPIPE
};

enum snd_pseudo_mixer_interactive_audio_mode
{
    SND_PSEUDO_MIXER_INTERACTIVE_AUDIO_MODE_3_4,
    SND_PSEUDO_MIXER_INTERACTIVE_AUDIO_MODE_3_2,
    SND_PSEUDO_MIXER_INTERACTIVE_AUDIO_MODE_2_0
};

enum snd_pseudo_mixer_audiogen_audio_mode
{
    SND_PSEUDO_MIXER_AUDIOGEN_AUDIO_MODE_3_4,
    SND_PSEUDO_MIXER_AUDIOGEN_AUDIO_MODE_3_2,
    SND_PSEUDO_MIXER_AUDIOGEN_AUDIO_MODE_2_0
};

enum snd_pseudo_mixer_drc_type
{
    SND_PSEUDO_MIXER_DRC_CUSTOM0 = STM_SE_COMP_CUSTOM_A,
    SND_PSEUDO_MIXER_DRC_CUSTOM1 = STM_SE_COMP_CUSTOM_B,
    SND_PSEUDO_MIXER_DRC_LINE_OUT = STM_SE_COMP_LINE_OUT,
    SND_PSEUDO_MIXER_DRC_RF = STM_SE_COMP_RF_MODE
};

/* must be binary compatible with struct snd_aes_iec958 (can't include in C++ code) */
struct snd_pseudo_mixer_aes_iec958
{
    unsigned char status[24];       /* AES/IEC958 channel status bits */
    unsigned char subcode[147];     /* AES/IEC958 subcode bits */
    unsigned char pad;              /* nothing */
    unsigned char dig_subframe[4];  /* AES/IEC958 subframe bits */
};

struct snd_pseudo_mixer_fatpipe
{
    unsigned short md[16];
};

#define SND_PSEUDO_TOPOLOGY_FLAGS_ENABLE_SPDIF_FORMATING 0x01
#define SND_PSEUDO_TOPOLOGY_FLAGS_FATPIPE                0x02
#define SND_PSEUDO_TOPOLOGY_FLAGS_ENABLE_HDMI_FORMATING  0x04
#define SND_PSEUDO_TOPOLOGY_FLAGS_ENABLE_PROMOTION       0x08

// Note : the following 3 flags are exclusive .. if none is flagged, then the driver will implement following default :
//  case HDMI   : default to TV (ToDO :: read from EDID )
//  case Analog : default to TV
//  case SPDIF  : default to AVR
#define SND_PSEUDO_TOPOLOGY_FLAGS_CONNECTED_TO_TV        0x10
#define SND_PSEUDO_TOPOLOGY_FLAGS_CONNECTED_TO_AVR       0x20
#define SND_PSEUDO_TOPOLOGY_FLAGS_CONNECTED_TO_HEADPHONE 0x40

struct snd_pseudo_mixer_downstream_card
{
    char          name[16]; /* e.g. Analog0, HDMI */

    char          alsaname[24]; /* card name (e.g. hw:0,0 or hw:ANALOG,1) */

    unsigned int  flags;
    unsigned int  max_freq;     /* in hz */
    unsigned char num_channels;

    char          reserved[7];

    int32_t       target_level; /* in mB */
    struct stm_se_audio_channel_assignment channel_assignment;
};

struct snd_pseudo_mixer_downstream_topology
{
    struct snd_pseudo_mixer_downstream_card card[SND_PSEUDO_MAX_OUTPUTS];
};

struct snd_pseudo_mixer_settings
{
    unsigned int magic;

    /* The next two indices help us getting an handle to the hdmi output device*/
    unsigned int display_device_id;
    int display_output_id;

    enum snd_pseudo_mixer_metadata_update metadata_update;

    /* switches */
    char all_speaker_stereo_enable;
    char dualmono_metadata_override;

    /* routes */
    enum snd_pseudo_mixer_interactive_audio_mode interactive_audio_mode;
    enum snd_pseudo_mixer_audiogen_audio_mode audiogen_audio_mode;

    /* latency tuning */
    int master_latency;

    /* generic spdif meta data */
    struct snd_pseudo_mixer_aes_iec958 iec958_metadata;
    struct snd_pseudo_mixer_aes_iec958 iec958_mask;

    /* fatpipe meta data */
    struct snd_pseudo_mixer_fatpipe fatpipe_metadata;
    struct snd_pseudo_mixer_fatpipe fatpipe_mask;

    /* topological structure */
    struct snd_pseudo_mixer_downstream_topology downstream_topology;

    /* local copy of mixer channel assignment */
    struct stm_se_audio_channel_assignment MixerChannelAssignment;
};

struct snd_pseudo_transformer_name
{
    unsigned int magic;

    char name[128];
};

#define SND_PSEUDO_MIXER_DOWNMIX_HEADER_MAGIC 0x58494d44 /* 'DMIX' */
#define SND_PSEUDO_MIXER_DOWNMIX_HEADER_VERSION 2
#define SND_PSEUDO_MIXER_DOWNMIX_HEADER_SIZE(hdr) \
    (sizeof(struct snd_pseudo_mixer_downmix_header) + \
     ((hdr).num_index_entries * sizeof(struct snd_pseudo_mixer_downmix_index)) + \
     ((hdr).data_length * sizeof(snd_pseudo_mixer_downmix_Q15)))

struct snd_pseudo_mixer_downmix_header
{
    uint32_t magic;
    uint32_t version;
    uint32_t num_index_entries;
    uint16_t data_length;
    uint16_t padding0;
    uint32_t padding1;
    uint32_t padding2;
};

struct snd_pseudo_mixer_downmix_index
{
    struct stm_se_audio_channel_assignment input_id;
    struct stm_se_audio_channel_assignment output_id;
    uint8_t  input_dimension;
    uint8_t  output_dimension;
    uint16_t offset;
};

struct snd_pseudo_mixer_downmix_rom
{
    struct snd_pseudo_mixer_downmix_header header;
    struct snd_pseudo_mixer_downmix_index index[];
};

typedef int16_t snd_pseudo_mixer_downmix_Q15;

typedef void (snd_pseudo_mixer_observer_t)(void *ctx,
                                           const struct snd_pseudo_mixer_settings *mixer_settings);

struct snd_pseudo_mixer_downmix_rom *snd_pseudo_get_downmix_rom(int mixer_num);

// <<< end of pseudo_mixer.h copy

/*! @} */ /* pseudo_mixer_interfaces */

#endif /* STM_SE_CONTROLS_PSEUDOMIXER_H_ */
