/*
 * Pseudo ALSA device (mixer and PCM player) implemented (mostly) software
 * Media controller extensions
 *
 * Copied from sound/drivers/dummy.c by Jaroslav Kysela
 * Copyright (c) 2007 STMicroelectronics R&D Limited <daniel.thompson@st.com>
 * Copyright (c) by Jaroslav Kysela <perex@suse.cz>
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

#include <linux/module.h>
#include <linux/ioport.h>
#include <linux/bpa2.h>
#include <linux/init.h>
#include <linux/debugfs.h>
#include <linux/err.h>
#include <linux/firmware.h>
#include <linux/platform_device.h>
#include <linux/jiffies.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/wait.h>
#include <linux/moduleparam.h>
#include <linux/version.h>
#include <sound/core.h>
#include <sound/control.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/rawmidi.h>
#include <sound/initval.h>
#include <linux/io.h>
#include <asm/cacheflush.h>

#include <ACC_Transformers/acc_mmedefines.h>

#include <stm_display.h>

#include "stm_se.h"
#include "pseudo_mixer.h"
#include "stmedia.h"
#include "pseudocard.h"

/* index into the devices[] array for the hw sound card */
static int hw_index;

int init_alsa_media_entity(struct snd_card *card,
			   struct media_entity *entity,
			   char *prefix, int i, char *suffix,
			   int pad_count, struct media_pad *pads)
{
	char name[20];
	int err;

	/* make the entity name */
	if (i != -1)
		snprintf(name, sizeof(name), "%s%d", prefix, i);
	else if (suffix != NULL)
		snprintf(name, sizeof(name), "%s_%s", prefix, suffix);
	else
		snprintf(name, sizeof(name), "%s", prefix);

	/* initialize the media entity */
	entity->name = kmalloc(strlen(name) + 1, GFP_KERNEL);
	if (!entity->name) {
		printk(KERN_ERR
		       "%s: failed to allocate the media entity name (%s)\n",
		       __func__, name);
		return -ENOMEM;
	}
	strcpy((char *)entity->name, name);

	entity->info.alsa.card = card->number;
	entity->info.alsa.device = i;

	err = media_entity_init(entity, pad_count, pads, 0);
	if (err < 0) {
		printk(KERN_ERR
		       "%s: failed to initialize the media entity (%d)\n",
		       __func__, err);
		goto media_entity_init_error;
	}

	/* register the media entity */
	err = stm_media_register_entity(entity);
	if (err < 0) {
		printk(KERN_ERR
		       "%s: failed to register the media entity (%d)\n",
		       __func__, err);
		goto media_entity_register_error;
	}

	printk(KERN_INFO "%s: media entity registered\n", name);

	goto media_entity_init_success;

media_entity_register_error:
	media_entity_cleanup(entity);

media_entity_init_error:
	kfree(entity->name);

media_entity_init_success:
	return err;
}

static bool is_pad_free(const struct media_pad *pad)
{
	int i;
	bool res = true;
	struct media_entity *entity = pad->entity;
	struct media_link *link;

	/* Find entity links that uses this pad */
	for (i = 0; i < entity->num_links; i++) {
		link = &entity->links[i];

		if (!(link->flags & MEDIA_LNK_FL_ENABLED))
			continue;

		if (pad == link->source || pad == link->sink) {
			printk(KERN_DEBUG "pad is used by %s[%d]->%s[%d]\n",
				link->source->entity->name, link->source->index,
				link->sink->entity->name, link->sink->index);
			res = false;
		}
	}
	return res;
}

static int mixer_link_setup(struct media_entity *entity,
			    const struct media_pad *local,
			    const struct media_pad *remote, u32 flags)
{
	struct snd_pseudo *pseudo =
	    container_of(entity, struct snd_pseudo, entity);
	struct snd_pseudo_hw_player_entity *player;
	int err = 0;

	switch (remote->entity->type) {
	case MEDIA_ENT_T_ALSA_SUBDEV_READER:
	case MEDIA_ENT_T_DVB_SUBDEV_AUDIO_DECODER:
	case MEDIA_ENT_T_V4L2_SUBDEV_AUDIO_DECODER:
	case MEDIA_ENT_T_V4L2_SUBDEV_HDMIRX:
		/* Mixer input can only have one link ENABLED */
		if (flags & MEDIA_LNK_FL_ENABLED && !is_pad_free(local)) {
			printk(KERN_ERR "%s[%d] pad is already used\n",
				entity->name, local->index);
			return -EMLINK;
		}
		break;
	case MEDIA_ENT_T_ALSA_SUBDEV_PLAYER:
		player = container_of(remote->entity,
				      struct snd_pseudo_hw_player_entity,
				      entity);
		if (flags & MEDIA_LNK_FL_ENABLED)
			err =
			    snd_pseudo_mixer_attach_backend_player(pseudo,
								   player);
		else
			err =
			    snd_pseudo_mixer_detach_backend_player(pseudo,
								   player);
		if (err < 0) {
			printk(KERN_DEBUG
			       "link_setup: %s local %s:%d [%lx], remote %s:%d [%lx], flags %x FAILED\n",
			       entity->name, local->entity->name,
			       local->index, local->flags,
			       remote->entity->name, remote->index,
			       remote->flags, flags);
			return err;
		}
		break;
	default:
		return -EINVAL;
	}

	printk(KERN_DEBUG
	       "link_setup: %s local %s:%d [%lx], remote %s:%d [%lx], flags %x SUCCEEDED\n",
	       entity->name, local->entity->name, local->index,
	       local->flags, remote->entity->name, remote->index,
	       remote->flags, flags);

	return 0;
}

/* mixer operations */
static const struct media_entity_operations mixer_media_ops = {
	.link_setup = mixer_link_setup,
};

void snd_pseudo_register_mixer_entity(struct snd_pseudo *pseudo, int dev)
{
	int err;

	/* set the pad flags */
	pseudo->pads[STM_SND_MIXER_PAD_PRIMARY].flags = MEDIA_PAD_FL_SINK;
	pseudo->pads[STM_SND_MIXER_PAD_SECONDARY].flags = MEDIA_PAD_FL_SINK;
	pseudo->pads[STM_SND_MIXER_PAD_AUX2].flags = MEDIA_PAD_FL_SINK;
	pseudo->pads[STM_SND_MIXER_PAD_AUX3].flags = MEDIA_PAD_FL_SINK;
	pseudo->pads[STM_SND_MIXER_PAD_OUTPUT].flags = MEDIA_PAD_FL_SOURCE;

	/* initialize the media entity */
	memset(&pseudo->entity, 0, sizeof(pseudo->entity));
	pseudo->entity.type = MEDIA_ENT_T_ALSA_SUBDEV_MIXER;
	pseudo->entity.ops = &mixer_media_ops;
	err = init_alsa_media_entity(pseudo->card, &pseudo->entity,
				     pseudo->mediactl_name, dev, NULL,
				     ARRAY_SIZE(pseudo->pads), pseudo->pads);
	if (err < 0) {
		printk(KERN_ERR
		       "%s: failed to init the alsa media entity (%d)\n",
		       __func__, err);
		BUG();
	}

}

void snd_pseudo_unregister_mixer_entity(struct snd_pseudo *pseudo)
{
	struct media_pad *remote;
	struct snd_pseudo_hw_player_entity *player;
	unsigned int i = 0;
	int err;

	while ((remote =
		stm_media_find_remote_pad(&pseudo->
					  pads[STM_SND_MIXER_PAD_OUTPUT],
					  MEDIA_LNK_FL_ENABLED, &i))) {
		player =
		    container_of(remote->entity,
				 struct snd_pseudo_hw_player_entity, entity);
		err = snd_pseudo_mixer_detach_backend_player(pseudo, player);
		if (err < 0)
			printk(KERN_ERR
			       "%s: failed to detach the player from mixer (%d)\n",
			       __func__, err);
	}

	stm_media_unregister_entity(&pseudo->entity);
	media_entity_cleanup(&pseudo->entity);
	kfree(pseudo->entity.name);
}

static void snd_card_pseudo_setup_links(struct platform_device *devices[],
					int mixer_max_index)
{
	int i, j, err;
	struct snd_card *hw_card;
	struct snd_pseudo_hw_card *pseudo_hw_card;

	/* create the links between hardware readers and players and mixers */
	if (!devices[hw_index])
		return;

	hw_card = platform_get_drvdata(devices[hw_index]);
	pseudo_hw_card = hw_card->private_data;

	/* create the links between hardware readers and mixers */
	for (i = 0; i < ARRAY_SIZE(pseudo_hw_card->reader_entities); ++i) {
		struct snd_pseudo_hw_reader_entity *reader =
		    &pseudo_hw_card->reader_entities[i];

		for (j = 0; j < mixer_max_index; ++j) {
			struct snd_card *card;
			struct snd_pseudo *pseudo;
			struct media_entity *source;
			struct media_entity *sink;

			if (!devices[j])
				continue;

			card = platform_get_drvdata(devices[j]);
			pseudo = card->private_data;
			source = &reader->entity;
			sink = &pseudo->entity;

			err =
			    media_entity_create_link(source,
						     STM_SND_READER_PAD_SOURCE,
						     sink,
						     STM_SND_MIXER_PAD_PRIMARY,
						     0);
			if (err < 0) {
				printk(KERN_ERR
				       "%s: failed to create media link (%d)\n",
				       __func__, err);
				BUG();
			}
			err = media_entity_create_link(source,
					     STM_SND_READER_PAD_SOURCE,
					     sink,
					     STM_SND_MIXER_PAD_SECONDARY,
					     0);
			if (err < 0) {
				printk(KERN_ERR
				       "%s: failed to create media link (%d)\n",
				       __func__, err);
				BUG();
			}
		}
	}

	/* create the links between mixers and hardware players */
	for (i = 0; i < ARRAY_SIZE(pseudo_hw_card->player_entities); ++i) {
		struct snd_pseudo_hw_player_entity *player =
		    &pseudo_hw_card->player_entities[i];

		for (j = 0; j < mixer_max_index; ++j) {
			struct snd_card *card;
			struct snd_pseudo *pseudo;
			struct media_entity *source;
			struct media_entity *sink;

			if (!devices[j])
				continue;

			card = platform_get_drvdata(devices[j]);
			pseudo = card->private_data;
			source = &pseudo->entity;
			sink = &player->entity;

			if (!source->name || !sink->name)
				continue;

			err =
			    media_entity_create_link(source,
						     STM_SND_MIXER_PAD_OUTPUT,
						     sink,
						     STM_SND_PLAYER_PAD_SINK,
						     0);
			if (err < 0) {
				printk(KERN_ERR
				       "%s: failed to create media link (%d)\n",
				       __func__, err);
				BUG();
			}
		}
	}
}

int snd_pseudo_register_mc_platform_drivers(struct platform_device *devices[],
					    int mixer_max_index)
{
	struct platform_device *device;
	int err;

	/* register the hw reader/player card driver */
	err = snd_pseudo_register_hw_driver();
	if (err < 0)
		return err;

	/* determine the index of the hw sound card */
	hw_index = mixer_max_index + 1;

	/* create the hw sound card */
	device = snd_pseudo_register_hw_device();
	if (!IS_ERR(device))
		devices[hw_index] = device;

	snd_card_pseudo_setup_links(devices, mixer_max_index);

	return 0;
}

void snd_pseudo_unregister_mc_platform_drivers(void)
{
	snd_pseudo_unregister_hw_driver();
}
