#ifndef PSEUDOCARD_H
#define PSEUDOCARD_H

#define MAX_DYNAMIC_CONTROLS	80

#include <linux/platform_device.h>
#include <sound/control.h>

#include "stmedia.h"
#include "stmedia_export.h"
#include "pcmproc_tuning_fw_header.h"

#define STM_SND_READER_COUNT		1
#define AUDIO_GENERATOR_MAX		8

struct snd_pseudo_hw_reader_entity {
	struct media_entity entity;
	struct media_pad pads[STM_SND_READER_PAD_COUNT];
};

struct snd_pseudo_hw_player_entity {
	struct media_entity entity;
	struct media_pad pads[STM_SND_PLAYER_PAD_COUNT];

	int index;
	char name[16];
	struct snd_pseudo *mixer;
	component_handle_t backend_player;

	struct snd_card *hw_card;
	struct snd_kcontrol *hw_card_controls[MAX_DYNAMIC_CONTROLS];

	struct snd_kcontrol *dynamic_controls[MAX_DYNAMIC_CONTROLS];
	struct snd_pseudo_mixer_downstream_card *card;
	struct snd_kcontrol *tuning_kcontrols[PCM_PROCESSING_TYPE_LAST];
	struct snd_kcontrol *tuning_amount_kcontrols[PCM_PROCESSING_TYPE_LAST];
	int tuning_kcontrols_values[PCM_PROCESSING_TYPE_LAST];
};

struct snd_pseudo_hw_card {
	struct snd_card *card;
	struct snd_pseudo_hw_reader_entity
	 reader_entities[STM_SND_READER_COUNT];
	struct snd_pseudo_hw_player_entity
	 player_entities[SND_PSEUDO_MAX_OUTPUTS];
};

#define MAX_NUMBER_OF_PCMPROC_FW 32

struct pcmproc_FW_s {
	unsigned int users;
	unsigned int nb_firmware;
	const struct firmware *fwArray[MAX_NUMBER_OF_PCMPROC_FW];
};

#define MAX_MEDIACTL_CARD_NAME 12
struct snd_pseudo {
	struct snd_card *card;
        char                mediactl_name[MAX_MEDIACTL_CARD_NAME];
	struct media_entity entity;
	struct media_pad pads[STM_SND_MIXER_PAD_COUNT];

	struct snd_pcm *pcm;

	struct bpa2_part *allocator;

	int room_identifier;
	component_handle_t backend_mixer;
	struct snd_pseudo_hw_player_entity
	    *backend_player[SND_PSEUDO_MAX_OUTPUTS];

	struct mutex mixer_lock;
	stm_se_mixer_spec_t              spec;
	struct snd_pseudo_mixer_settings mixer;

	snd_pseudo_mixer_observer_t *mixer_observer;
	void *mixer_observer_ctx;
	component_handle_t audiogeneratorhandle[AUDIO_GENERATOR_MAX];
	int audiogengainrestore[AUDIO_GENERATOR_MAX];
	long restoregain[AUDIO_GENERATOR_MAX];

	struct snd_kcontrol *mixer_tuning_kcontrols[PCM_PROCESSING_TYPE_LAST];
	struct snd_kcontrol *mixer_tuning_amount_kcontrols
						[PCM_PROCESSING_TYPE_LAST];

	int mixer_tuning_kcontrols_values[PCM_PROCESSING_TYPE_LAST];

	struct snd_kcontrol_new * static_io_ctls;
	int                       sizeof_static_io_ctls;
};

/* defined in pseudo_mixer.c */
extern int *card_enables;
int snd_pseudo_mixer_create_backend_player(struct snd_pseudo_hw_player_entity
					   *player, int index);
int snd_pseudo_mixer_delete_backend_player(struct snd_pseudo_hw_player_entity
					   *player);
int snd_pseudo_mixer_attach_backend_player(struct snd_pseudo *pseudo,
					   struct snd_pseudo_hw_player_entity
					   *player);
int snd_pseudo_mixer_detach_backend_player(struct snd_pseudo *pseudo,
					   struct snd_pseudo_hw_player_entity
					   *player);
int snd_pseudo_create_card(int dev, size_t size, struct snd_card **p_card);

/* defined in pseudo_mc.c */
int init_alsa_media_entity(struct snd_card *card,
			   struct media_entity *entity,
			   char *prefix, int i, char *suffix,
			   int pad_count, struct media_pad *pads);
void snd_pseudo_register_mixer_entity(struct snd_pseudo *pseudo, int dev);
void snd_pseudo_unregister_mixer_entity(struct snd_pseudo *pseudo);
int snd_pseudo_register_mc_platform_drivers(struct platform_device *devices[],
					    int mixer_max_index);
void snd_pseudo_unregister_mc_platform_drivers(void);

/* defined in pseudo_hw.c */
int snd_pseudo_register_hw_driver(void);
void snd_pseudo_unregister_hw_driver(void);
struct platform_device *snd_pseudo_register_hw_device(void);

/* defined in pseudo_pcmproc_tuning.c */
void request_pcmproc_fw(struct device *dev,
			struct pcmproc_FW_s *pcmproc_fw_ctx,
			int tuningFwNumber,
			char **tuningFw);
void release_pcmproc_fw(struct pcmproc_FW_s *pcmproc_fw_ctx,
			struct snd_pseudo *pseudo);

void add_mixer_tuning_controls(struct snd_pseudo *pseudo);
void remove_mixer_tuning_controls(struct snd_pseudo *pseudo);

void add_player_tuning_controls(struct snd_pseudo *pseudo,
				struct snd_pseudo_hw_player_entity *player);
void remove_player_tuning_controls(struct snd_pseudo *pseudo,
				struct snd_pseudo_hw_player_entity *player);

stm_object_h snd_hw_player_get_from_entity(struct media_entity *entity);
#endif
