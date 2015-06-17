/*
 * Pseudo ALSA device (mixer and PCM player) implemented in software by the player.
 * Copyright (C) 2007 STMicroelectronics R&D Limited <daniel.thompson@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef PSEUDO_DEV_H_
#define PSEUDO_DEV_H_
#define SND_PSEUDO_MIXER_CHANNELS	8
#define SND_PSEUDO_MIXER_INTERACTIVE    8
#define SND_PSEUDO_MIXER_MAGIC          0xf051

#define SND_PSEUDO_TRANSFORMER_NAME_MAGIC 0xf052

/** Maximum number of supported outputs (subordinate ALSA soundcards) */
#define SND_PSEUDO_MAX_OUTPUTS          4


#define Q3_13_MIN               0
#define Q3_13_UNITY             ((1 << 13) - 1)
#define Q3_13_MAX               0xffff

#define Q0_8_MIN                0
#define Q0_8_UNITY              ((1 << 8) -1 )
#define Q0_8_MAX                0xFF

enum snd_pseudo_mixer_metadata_update
{
	SND_PSEUDO_MIXER_METADATA_UPDATE_NEVER,
	SND_PSEUDO_MIXER_METADATA_UPDATE_PRIMARY_AND_SECONDARY_ONLY,
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

enum snd_pseudo_mixer_drc_type
{
	SND_PSEUDO_MIXER_DRC_CUSTOM0,
	SND_PSEUDO_MIXER_DRC_CUSTOM1,
	SND_PSEUDO_MIXER_DRC_LINE_OUT,
	SND_PSEUDO_MIXER_DRC_RF
};

enum snd_pseudo_mixer_emergency_mute {
	SND_PSEUDO_MIXER_EMERGENCY_MUTE_SOURCE_ONLY,
	SND_PSEUDO_MIXER_EMERGENCY_MUTE_PRE_MIX,
	SND_PSEUDO_MIXER_EMERGENCY_MUTE_POST_MIX,
	SND_PSEUDO_MIXER_EMERGENCY_MUTE_NO_MUTE
};

/* must be binary compatible with struct snd_aes_iec958 (which was can't include in C++ code) */
struct snd_pseudo_mixer_aes_iec958 {
	unsigned char status[24];	/* AES/IEC958 channel status bits */
	unsigned char subcode[147];	/* AES/IEC958 subcode bits */
	unsigned char pad;		/* nothing */
	unsigned char dig_subframe[4];	/* AES/IEC958 subframe bits */
};

struct snd_pseudo_mixer_fatpipe {
	unsigned short md[16];
};

enum snd_pseudo_mixer_channel_pair {                             /* pair0   pair1   pair2   pair3   pair4   */
	SND_PSEUDO_MIXER_CHANNEL_PAIR_DEFAULT,                   /*   Y       Y       Y       Y       Y     */
	SND_PSEUDO_MIXER_CHANNEL_PAIR_L_R = 0,                   /*   Y                               Y     */
	SND_PSEUDO_MIXER_CHANNEL_PAIR_CNTR_LFE1 = 0,             /*           Y                             */
	SND_PSEUDO_MIXER_CHANNEL_PAIR_LSUR_RSUR = 0,             /*                   Y                     */
	SND_PSEUDO_MIXER_CHANNEL_PAIR_LSURREAR_RSURREAR = 0,     /*                           Y             */

	SND_PSEUDO_MIXER_CHANNEL_PAIR_LT_RT,                     /*   Y                               Y     */
	SND_PSEUDO_MIXER_CHANNEL_PAIR_LPLII_RPLII,               /*                                         */
	SND_PSEUDO_MIXER_CHANNEL_PAIR_CNTRL_CNTRR,		 /*                                         */		
	SND_PSEUDO_MIXER_CHANNEL_PAIR_LHIGH_RHIGH,		 /*                                         */	
	SND_PSEUDO_MIXER_CHANNEL_PAIR_LWIDE_RWIDE,		 /*                                         */	
	SND_PSEUDO_MIXER_CHANNEL_PAIR_LRDUALMONO,		 /*                                         */	
	SND_PSEUDO_MIXER_CHANNEL_PAIR_RESERVED1,		 /*                                         */	

	SND_PSEUDO_MIXER_CHANNEL_PAIR_CNTR_0,			 /*           Y                             */
	SND_PSEUDO_MIXER_CHANNEL_PAIR_0_LFE1,			 /*           Y                             */
	SND_PSEUDO_MIXER_CHANNEL_PAIR_0_LFE2,			 /*           Y                             */
	SND_PSEUDO_MIXER_CHANNEL_PAIR_CHIGH_0,			 /*           Y                             */
	SND_PSEUDO_MIXER_CHANNEL_PAIR_CLOWFRONT_0,		 /*           Y                             */

	SND_PSEUDO_MIXER_CHANNEL_PAIR_CNTR_CSURR,		 /*                                         */	
	SND_PSEUDO_MIXER_CHANNEL_PAIR_CNTR_CHIGH,		 /*                                         */	
	SND_PSEUDO_MIXER_CHANNEL_PAIR_CNTR_TOPSUR,		 /*                                         */	
	SND_PSEUDO_MIXER_CHANNEL_PAIR_CNTR_CHIGHREAR,		 /*                                         */	
	SND_PSEUDO_MIXER_CHANNEL_PAIR_CNTR_CLOWFRONT,		 /*                                         */	

	SND_PSEUDO_MIXER_CHANNEL_PAIR_CHIGH_TOPSUR,		 /*                                         */	
	SND_PSEUDO_MIXER_CHANNEL_PAIR_CHIGH_CHIGHREAR,		 /*                                         */	
	SND_PSEUDO_MIXER_CHANNEL_PAIR_CHIGH_CLOWFRONT,		 /*                                         */	

	SND_PSEUDO_MIXER_CHANNEL_PAIR_CNTR_LFE2,		 /*                                         */	
	SND_PSEUDO_MIXER_CHANNEL_PAIR_CHIGH_LFE1,		 /*                                         */	
	SND_PSEUDO_MIXER_CHANNEL_PAIR_CHIGH_LFE2,		 /*                                         */	
	SND_PSEUDO_MIXER_CHANNEL_PAIR_CLOWFRONT_LFE1,		 /*                                         */	
	SND_PSEUDO_MIXER_CHANNEL_PAIR_CLOWFRONT_LFE2,		 /*                                         */	

	SND_PSEUDO_MIXER_CHANNEL_PAIR_LSIDESURR_RSIDESURR,	 /*                                         */	
	SND_PSEUDO_MIXER_CHANNEL_PAIR_LHIGHSIDE_RHIGHSIDE,	 /*                                         */	
	SND_PSEUDO_MIXER_CHANNEL_PAIR_LDIRSUR_RDIRSUR,		 /*                                         */	
	SND_PSEUDO_MIXER_CHANNEL_PAIR_LHIGHREAR_RHIGHREAR,	 /*                                         */	

	SND_PSEUDO_MIXER_CHANNEL_PAIR_CSURR_0,			 /*                                         */	
	SND_PSEUDO_MIXER_CHANNEL_PAIR_TOPSUR_0,			 /*                                         */	
	SND_PSEUDO_MIXER_CHANNEL_PAIR_CSURR_TOPSUR,		 /*                                         */	
	SND_PSEUDO_MIXER_CHANNEL_PAIR_CSURR_CHIGH,		 /*                                         */	
	SND_PSEUDO_MIXER_CHANNEL_PAIR_CSURR_CHIGHREAR,		 /*                                         */	
	SND_PSEUDO_MIXER_CHANNEL_PAIR_CSURR_CLOWFRONT,		 /*                                         */	
	SND_PSEUDO_MIXER_CHANNEL_PAIR_CSURR_LFE1,		 /*                                         */	
	SND_PSEUDO_MIXER_CHANNEL_PAIR_CSURR_LFE2,		 /*                                         */	
	SND_PSEUDO_MIXER_CHANNEL_PAIR_CHIGHREAR_0,		 /*                                         */	
	SND_PSEUDO_MIXER_CHANNEL_PAIR_DSTEREO_LsRs,		 /*                                         */	

	SND_PSEUDO_MIXER_CHANNEL_PAIR_NOT_CONNECTED,		 /*   Y       Y       Y       Y       Y     */	
};

struct snd_pseudo_mixer_channel_assignment {
	unsigned int pair0:6; /* channels 0 and 1 */
	unsigned int pair1:6; /* channels 2 and 3 */
	unsigned int pair2:6; /* channels 4 and 5 */
	unsigned int pair3:6; /* channels 6 and 7 */
	unsigned int pair4:6; /* channels 8 and 9 */

	unsigned int reserved0:1;
	unsigned int malleable:1;
};

static const struct snd_pseudo_mixer_channel_assignment SND_PSEUDO_MIXER_CHANNEL_ASSIGNMENT_AUTOMATIC = {
	SND_PSEUDO_MIXER_CHANNEL_PAIR_NOT_CONNECTED,
	SND_PSEUDO_MIXER_CHANNEL_PAIR_NOT_CONNECTED,
	SND_PSEUDO_MIXER_CHANNEL_PAIR_NOT_CONNECTED,
	SND_PSEUDO_MIXER_CHANNEL_PAIR_NOT_CONNECTED,
	SND_PSEUDO_MIXER_CHANNEL_PAIR_NOT_CONNECTED
};

#define SND_PSEUDO_TOPOLOGY_FLAGS_ENABLE_SPDIF_FORMATING 0x01
#define SND_PSEUDO_TOPOLOGY_FLAGS_FATPIPE 0x02
#define SND_PSEUDO_TOPOLOGY_FLAGS_ENABLE_HDMI_FORMATING 0x04

struct snd_pseudo_mixer_downstream_card {
	char name[16]; /* e.g. Analog0, HDMI */

	char alsaname[24]; /* card name (e.g. hw:0,0 or hw:ANALOG,1) */
	
	unsigned int flags;
	unsigned int max_freq; /* in hz */	
	unsigned char num_channels;
	
	char reserved[11];
	
	struct snd_pseudo_mixer_channel_assignment channel_assignment;
};

struct snd_pseudo_mixer_downstream_topology {
	struct snd_pseudo_mixer_downstream_card card[SND_PSEUDO_MAX_OUTPUTS];
};

struct snd_pseudo_mixer_settings
{
        unsigned int magic;
        char spdif_bypass;
        char hdmi_bypass;

        /* The next two indicies help us getting an handle to the hdmi output device*/
        unsigned int display_device_id;
        int display_output_id;
        
        /* pre-mix gain control. These are mostly controls to support BD-J mix
         * model (other clients will set disable override_gain_pan and can
	 * then ignore the other values).
         */
        int post_mix_gain;
        int primary_gain[SND_PSEUDO_MIXER_CHANNELS];
        int secondary_gain[SND_PSEUDO_MIXER_CHANNELS];
        int secondary_pan[SND_PSEUDO_MIXER_CHANNELS];
        int interactive_gain[SND_PSEUDO_MIXER_INTERACTIVE];
        int interactive_pan[SND_PSEUDO_MIXER_INTERACTIVE][SND_PSEUDO_MIXER_CHANNELS];
        enum snd_pseudo_mixer_metadata_update metadata_update;
       
        /* post-mix gain control */
        int     master_volume[SND_PSEUDO_MIXER_CHANNELS]; /* logarithmic */
        int     chain_delay[SND_PSEUDO_MAX_OUTPUTS][SND_PSEUDO_MIXER_CHANNELS]; /* in microseconds */
        int     chain_volume[SND_PSEUDO_MAX_OUTPUTS];
        char    chain_enable[SND_PSEUDO_MAX_OUTPUTS];
        char	chain_limiter_enable[SND_PSEUDO_MAX_OUTPUTS];
        /* DRC info */
        char                           drc_enable;
	enum snd_pseudo_mixer_drc_type drc_type;
	int                            hdr;
	int                            ldr;
    
        /* emergency mute control */
        enum snd_pseudo_mixer_emergency_mute emergency_mute;

        /* switches */
        char dc_remove_enable;
	char bass_mgt_bypass;
	char fixed_output_frequency;
	char all_speaker_stereo_enable;
	char downmix_promotion_enable;

	/* routes */
	enum snd_pseudo_mixer_spdif_encoding spdif_encoding;
	enum snd_pseudo_mixer_spdif_encoding hdmi_encoding;
	enum snd_pseudo_mixer_interactive_audio_mode interactive_audio_mode;
	char virtualization_mode;
	char spacialization_mode;

	/* latency tuning */
	int master_latency;
	int chain_latency[SND_PSEUDO_MAX_OUTPUTS];

        /* generic spdif meta data */
	struct snd_pseudo_mixer_aes_iec958 iec958_metadata;
	struct snd_pseudo_mixer_aes_iec958 iec958_mask;

	/* fatpipe meta data */
	struct snd_pseudo_mixer_fatpipe fatpipe_metadata;
	struct snd_pseudo_mixer_fatpipe fatpipe_mask;
	
	/* topological structure */
	struct snd_pseudo_mixer_downstream_topology downstream_topology;
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

struct snd_pseudo_mixer_downmix_header {
	uint32_t magic;
	uint32_t version;
	uint32_t num_index_entries;
	uint16_t data_length;
	uint16_t padding0;
	uint32_t padding1;
	uint32_t padding2;
};

struct snd_pseudo_mixer_downmix_index {
	struct snd_pseudo_mixer_channel_assignment input_id;
	struct snd_pseudo_mixer_channel_assignment output_id;
	uint8_t  input_dimension;
	uint8_t  output_dimension;
	uint16_t offset;
};

struct snd_pseudo_mixer_downmix_rom {
	struct snd_pseudo_mixer_downmix_header header;
	struct snd_pseudo_mixer_downmix_index index[];
};

typedef int16_t snd_pseudo_mixer_downmix_Q15;

typedef void (snd_pseudo_mixer_observer_t)(void *ctx,
		const struct snd_pseudo_mixer_settings *mixer_settings);

int snd_pseudo_register_mixer_observer(int mixer_num,
		snd_pseudo_mixer_observer_t *observer, void *ctx);
int snd_pseudo_deregister_mixer_observer(int mixer_num,
		snd_pseudo_mixer_observer_t *observer, void *ctx);

struct snd_pseudo_mixer_downmix_rom *snd_pseudo_get_downmix_rom(int mixer_num);

#endif /*PSEUDO_DEV_H_*/
