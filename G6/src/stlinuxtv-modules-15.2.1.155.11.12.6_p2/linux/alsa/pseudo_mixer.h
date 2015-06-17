/*
 * Pseudo ALSA device (mixer and PCM player) implemented in software
 * by the player.
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

#include <media/media-entity.h>

#if 0
#define SND_PSEUDO_MIXER_CHANNELS       8
#define SND_PSEUDO_MIXER_INTERACTIVE    8
#define SND_PSEUDO_MIXER_MAGIC          0xf051

#define SND_PSEUDO_TRANSFORMER_NAME_MAGIC 0xf052

/** Maximum number of supported outputs (subordinate ALSA soundcards) */
#define SND_PSEUDO_MAX_OUTPUTS          4

#define Q3_13_MIN               0
#define Q3_13_UNITY             ((1 << 13) - 1)
#define Q3_13_MAX               0xffff

#define Q0_8_MIN                0
#define Q0_8_UNITY              ((1 << 8) - 1)
#define Q0_8_MAX                0xFF

enum snd_pseudo_mixer_metadata_update {
	SND_PSEUDO_MIXER_METADATA_UPDATE_NEVER,
	SND_PSEUDO_MIXER_METADATA_UPDATE_PRIMARY_AND_SECONDARY_ONLY,
	SND_PSEUDO_MIXER_METADATA_UPDATE_ALWAYS,
};

enum snd_pseudo_mixer_spdif_encoding {
	SND_PSEUDO_MIXER_SPDIF_ENCODING_PCM,
	SND_PSEUDO_MIXER_SPDIF_ENCODING_AC3,
	SND_PSEUDO_MIXER_SPDIF_ENCODING_DTS,
	SND_PSEUDO_MIXER_SPDIF_ENCODING_FATPIPE
};

enum snd_pseudo_mixer_interactive_audio_mode {
	SND_PSEUDO_MIXER_INTERACTIVE_AUDIO_MODE_3_4,
	SND_PSEUDO_MIXER_INTERACTIVE_AUDIO_MODE_3_2,
	SND_PSEUDO_MIXER_INTERACTIVE_AUDIO_MODE_2_0
};

enum snd_pseudo_mixer_drc_type {
	SND_PSEUDO_MIXER_DRC_CUSTOM0,
	SND_PSEUDO_MIXER_DRC_CUSTOM1,
	SND_PSEUDO_MIXER_DRC_LINE_OUT,
	SND_PSEUDO_MIXER_DRC_RF
};

/* must be binary compatible with struct snd_aes_iec958
 * (which was can't include in C++ code) */
struct snd_pseudo_mixer_aes_iec958 {
	unsigned char status[24];	/* AES/IEC958 channel status bits */
	unsigned char subcode[147];	/* AES/IEC958 subcode bits */
	unsigned char pad;	/* nothing */
	unsigned char dig_subframe[4];	/* AES/IEC958 subframe bits */
};

struct snd_pseudo_mixer_fatpipe {
	unsigned short md[16];
};

enum snd_pseudo_mixer_channel_pair { /* pair0 pair1 pair2 pair3 pair4   */
	SND_PSEUDO_MIXER_CHANNEL_PAIR_DEFAULT,			/* Y Y Y Y Y */
	SND_PSEUDO_MIXER_CHANNEL_PAIR_L_R = 0,			/* Y . . . Y */
	SND_PSEUDO_MIXER_CHANNEL_PAIR_CNTR_LFE1 = 0,		/* . Y . . . */
	SND_PSEUDO_MIXER_CHANNEL_PAIR_LSUR_RSUR = 0,		/* . . Y . . */
	SND_PSEUDO_MIXER_CHANNEL_PAIR_LSURREAR_RSURREAR = 0,	/* . . . Y . */

	SND_PSEUDO_MIXER_CHANNEL_PAIR_LT_RT,			/* Y . . . Y */
	SND_PSEUDO_MIXER_CHANNEL_PAIR_LPLII_RPLII,		/* . . . . . */
	SND_PSEUDO_MIXER_CHANNEL_PAIR_CNTRL_CNTRR,		/* . . . . . */
	SND_PSEUDO_MIXER_CHANNEL_PAIR_LHIGH_RHIGH,		/* . . . . . */
	SND_PSEUDO_MIXER_CHANNEL_PAIR_LWIDE_RWIDE,		/* . . . . . */
	SND_PSEUDO_MIXER_CHANNEL_PAIR_LRDUALMONO,		/* . . . . . */
	SND_PSEUDO_MIXER_CHANNEL_PAIR_RESERVED1,		/* . . . . . */

	SND_PSEUDO_MIXER_CHANNEL_PAIR_CNTR_0,			/* . Y . . . */
	SND_PSEUDO_MIXER_CHANNEL_PAIR_0_LFE1,			/* . Y . . . */
	SND_PSEUDO_MIXER_CHANNEL_PAIR_0_LFE2,			/* . Y . . . */
	SND_PSEUDO_MIXER_CHANNEL_PAIR_CHIGH_0,			/* . Y . . . */
	SND_PSEUDO_MIXER_CHANNEL_PAIR_CLOWFRONT_0,		/* . Y . . . */

	SND_PSEUDO_MIXER_CHANNEL_PAIR_CNTR_CSURR,		/* . . . . . */
	SND_PSEUDO_MIXER_CHANNEL_PAIR_CNTR_CHIGH,		/* . . . . . */
	SND_PSEUDO_MIXER_CHANNEL_PAIR_CNTR_TOPSUR,		/* . . . . . */
	SND_PSEUDO_MIXER_CHANNEL_PAIR_CNTR_CHIGHREAR,		/* . . . . . */
	SND_PSEUDO_MIXER_CHANNEL_PAIR_CNTR_CLOWFRONT,		/* . . . . . */

	SND_PSEUDO_MIXER_CHANNEL_PAIR_CHIGH_TOPSUR,		/* . . . . . */
	SND_PSEUDO_MIXER_CHANNEL_PAIR_CHIGH_CHIGHREAR,		/* . . . . . */
	SND_PSEUDO_MIXER_CHANNEL_PAIR_CHIGH_CLOWFRONT,		/* . . . . . */

	SND_PSEUDO_MIXER_CHANNEL_PAIR_CNTR_LFE2,		/* . . . . . */
	SND_PSEUDO_MIXER_CHANNEL_PAIR_CHIGH_LFE1,		/* . . . . . */
	SND_PSEUDO_MIXER_CHANNEL_PAIR_CHIGH_LFE2,		/* . . . . . */
	SND_PSEUDO_MIXER_CHANNEL_PAIR_CLOWFRONT_LFE1,		/* . . . . . */
	SND_PSEUDO_MIXER_CHANNEL_PAIR_CLOWFRONT_LFE2,		/* . . . . . */

	SND_PSEUDO_MIXER_CHANNEL_PAIR_LSIDESURR_RSIDESURR,	/* . . . . . */
	SND_PSEUDO_MIXER_CHANNEL_PAIR_LHIGHSIDE_RHIGHSIDE,	/* . . . . . */
	SND_PSEUDO_MIXER_CHANNEL_PAIR_LDIRSUR_RDIRSUR,		/* . . . . . */
	SND_PSEUDO_MIXER_CHANNEL_PAIR_LHIGHREAR_RHIGHREAR,	/* . . . . . */

	SND_PSEUDO_MIXER_CHANNEL_PAIR_CSURR_0,			/* . . . . . */
	SND_PSEUDO_MIXER_CHANNEL_PAIR_TOPSUR_0,			/* . . . . . */
	SND_PSEUDO_MIXER_CHANNEL_PAIR_CSURR_TOPSUR,		/* . . . . . */
	SND_PSEUDO_MIXER_CHANNEL_PAIR_CSURR_CHIGH,		/* . . . . . */
	SND_PSEUDO_MIXER_CHANNEL_PAIR_CSURR_CHIGHREAR,		/* . . . . . */
	SND_PSEUDO_MIXER_CHANNEL_PAIR_CSURR_CLOWFRONT,		/* . . . . . */
	SND_PSEUDO_MIXER_CHANNEL_PAIR_CSURR_LFE1,		/* . . . . . */
	SND_PSEUDO_MIXER_CHANNEL_PAIR_CSURR_LFE2,		/* . . . . . */
	SND_PSEUDO_MIXER_CHANNEL_PAIR_CHIGHREAR_0,		/* . . . . . */
	SND_PSEUDO_MIXER_CHANNEL_PAIR_DSTEREO_LsRs,		/* . . . . . */

	SND_PSEUDO_MIXER_CHANNEL_PAIR_NOT_CONNECTED,		/* Y Y Y Y Y */
};

struct snd_pseudo_mixer_channel_assignment {
	unsigned int pair0:6;	/* channels 0 and 1 */
	unsigned int pair1:6;	/* channels 2 and 3 */
	unsigned int pair2:6;	/* channels 4 and 5 */
	unsigned int pair3:6;	/* channels 6 and 7 */
	unsigned int pair4:6;	/* channels 8 and 9 */

	unsigned int reserved0:1;
	unsigned int malleable:1;
};

static const struct snd_pseudo_mixer_channel_assignment
SND_PSEUDO_MIXER_CHANNEL_ASSIGNMENT_AUTOMATIC = {
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
	char name[16];		/* e.g. Analog0, HDMI */

	char alsaname[24];	/* card name (e.g. hw:0,0 or hw:ANALOG,1) */

	unsigned int flags;
	unsigned int max_freq;	/* in hz */
	unsigned char num_channels;

	char reserved[11];

	struct snd_pseudo_mixer_channel_assignment channel_assignment;
};

struct snd_pseudo_mixer_downstream_topology {
	struct snd_pseudo_mixer_downstream_card card[SND_PSEUDO_MAX_OUTPUTS];
};

struct snd_pseudo_mixer_settings {
	unsigned int magic;

	/* The next two indicies help us getting an handle
	 * to the hdmi output device */
	unsigned int display_device_id;
	int display_output_id;

	enum snd_pseudo_mixer_metadata_update metadata_update;

	/* switches */
	char all_speaker_stereo_enable;
	char downmix_promotion_enable;
	char dualmono_metadata_override;

	/* routes */
	enum snd_pseudo_mixer_interactive_audio_mode interactive_audio_mode;

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
};

struct snd_pseudo_transformer_name {
	unsigned int magic;

	char name[128];
};

typedef void (snd_pseudo_mixer_observer_t) (void *ctx,
					    const struct
					    snd_pseudo_mixer_settings *
					    mixer_settings);

#endif

int snd_pseudo_register_mixer_observer(int mixer_num,
				       snd_pseudo_mixer_observer_t *observer,
				       void *ctx);
int snd_pseudo_deregister_mixer_observer(int mixer_num,
					 snd_pseudo_mixer_observer_t *observer,
					 void *ctx);
int snd_pseudo_mixer_get(int mixer_num, stm_object_h *sink);

#ifdef CONFIG_SOUND
stm_object_h snd_pseudo_mixer_get_from_entity(struct media_entity *entity);
#else
static inline stm_object_h snd_pseudo_mixer_get_from_entity(
		struct media_entity *entity)
{
	return NULL;
}
#endif


#endif /*PSEUDO_DEV_H_ */
