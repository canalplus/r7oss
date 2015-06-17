/*
 * Pseudo ALSA device (mixer and PCM player) implemented (mostly) software
 * Media controller hw reader and player entities
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

static int hw_reader_link_setup(struct media_entity *entity,
				const struct media_pad *local,
				const struct media_pad *remote, u32 flags)
{
	return 0;
}

/* hw reader operations */
static const struct media_entity_operations hw_reader_media_ops = {
	.link_setup = hw_reader_link_setup,
};

static int hw_player_link_setup(struct media_entity *entity,
				const struct media_pad *local,
				const struct media_pad *remote, u32 flags)
{
	return 0;
}

/* hw player operations */
static const struct media_entity_operations hw_player_media_ops = {
	.link_setup = hw_player_link_setup,
};

static int snd_pseudo_hw_card_probe(struct platform_device *devptr)
{
	struct snd_card *card;
	struct snd_pseudo_hw_card *pseudo_hw_card;
	int err, i;
	int dev = devptr->id;
	int result;

	result =
	    snd_pseudo_create_card(dev, sizeof(struct snd_pseudo_hw_card),
				   &card);
	if (result != 0)
		return result;

	pseudo_hw_card = card->private_data;

	pseudo_hw_card->card = card;

	strlcpy(card->driver, "Pseudo HW Card", sizeof(card->driver));
	snprintf(card->shortname, sizeof(card->shortname), "HW%d", dev);
	snprintf(card->longname, sizeof(card->longname), "HW%d", dev);
	snd_card_set_dev(card, &devptr->dev);

	/* create the reader entities */
	for (i = 0; i < ARRAY_SIZE(pseudo_hw_card->reader_entities); ++i) {
		struct snd_pseudo_hw_reader_entity *reader =
		    &pseudo_hw_card->reader_entities[i];

		/* initialize the pads */
		reader->pads[STM_SND_READER_PAD_SINK].flags = MEDIA_PAD_FL_SINK;
		reader->pads[STM_SND_READER_PAD_SOURCE].flags =
		    MEDIA_PAD_FL_SOURCE;

		/* initialize the media entity */
		memset(&reader->entity, 0, sizeof(reader->entity));
		reader->entity.type = MEDIA_ENT_T_ALSA_SUBDEV_READER;
		reader->entity.ops = &hw_reader_media_ops;
		err = init_alsa_media_entity(card, &reader->entity,
					     "aud_reader", i, NULL,
					     ARRAY_SIZE(reader->pads),
					     reader->pads);
		if (err < 0) {
			printk(KERN_ERR
			       "%s: failed to init the alsa media entity (%d)\n",
			       __func__, err);
			BUG();
		}
	}

	/* create the player entities */
	for (i = 0; i < ARRAY_SIZE(pseudo_hw_card->player_entities); ++i) {
		struct snd_pseudo_hw_player_entity *player =
		    &pseudo_hw_card->player_entities[i];

		/* make sure the corresponding sound card is enabled */
		if (!card_enables[i])
			continue;

		/* create the backend player */
		player->index = i;
		player->hw_card = card;

		err = snd_pseudo_mixer_create_backend_player(player, i);
		if (err == -ENODEV)
			continue;

		if (err < 0) {
			printk(KERN_ERR
			       "%s: failed to create the backend player (%d)\n",
			       __func__, err);
			BUG();
		}

		/* initialize the pads */
		player->pads[STM_SND_PLAYER_PAD_SINK].flags = MEDIA_PAD_FL_SINK;
		player->pads[STM_SND_PLAYER_PAD_SOURCE].flags =
		    MEDIA_PAD_FL_SOURCE;

		/* initialize the media entity */
		memset(&player->entity, 0, sizeof(player->entity));
		player->entity.type = MEDIA_ENT_T_ALSA_SUBDEV_PLAYER;
		player->entity.ops = &hw_player_media_ops;
		err = init_alsa_media_entity(card, &player->entity,
					     "aud_player", -1, player->name,
					     ARRAY_SIZE(player->pads),
					     player->pads);
		if (err < 0) {
			printk(KERN_ERR
			       "%s: failed to init the alsa media entity (%d)\n",
			       __func__, err);
			BUG();
		}
	}

	snd_card_set_dev(card, &devptr->dev);

	err = snd_card_register(card);
	if (!err) {
		platform_set_drvdata(devptr, card);
		return 0;
	}
/*      __nodev: */
	snd_card_free(card);
	return err;
}

/**
 * snd_pseudo_hw_card_remove
 * This is the sound pseudocard platform remove function. It cleans
 * up the pseudocard registration with the media enity driver.
 */

static int snd_pseudo_hw_card_remove(struct platform_device *devptr)
{
	struct snd_card *card = platform_get_drvdata(devptr);
	struct snd_pseudo_hw_card *pseudo_hw_card = card->private_data;
	struct snd_pseudo_hw_player_entity *player;
	struct snd_pseudo_hw_reader_entity *reader;
	int i;

	/* Unregister player media entities */
	for (i = 0; i < ARRAY_SIZE(pseudo_hw_card->player_entities); ++i) {
		if (!card_enables[i])
			continue;
		player = &pseudo_hw_card->player_entities[i];
		snd_pseudo_mixer_delete_backend_player(player);
		media_device_unregister_entity(&player->entity);
		media_entity_cleanup(&player->entity);
		kfree(player->entity.name);
	}

	/* Unregister reader media entities */
	for (i = 0; i < ARRAY_SIZE(pseudo_hw_card->reader_entities); ++i) {
		reader = &pseudo_hw_card->reader_entities[i];
		stm_media_unregister_entity(&reader->entity);
		media_entity_cleanup(&reader->entity);
		kfree(reader->entity.name);
	}

	snd_card_free(card);
	platform_set_drvdata(devptr, NULL);
	return 0;
}

#ifdef CONFIG_PM
static int snd_pseudo_hw_card_suspend(struct platform_device *pdev,
				      pm_message_t state)
{
	struct snd_card *card = platform_get_drvdata(pdev);
/*	struct snd_pseudo_hw_card *pseudo_hw_card = card->private_data; */

	snd_power_change_state(card, SNDRV_CTL_POWER_D3hot);
	return 0;
}

static int snd_pseudo_hw_card_resume(struct platform_device *pdev)
{
	struct snd_card *card = platform_get_drvdata(pdev);

	snd_power_change_state(card, SNDRV_CTL_POWER_D0);
	return 0;
}
#endif

#define SND_PSEUDO_HW_CARD_DRIVER       "snd_pseudo_hw_card"

static struct platform_driver snd_pseudo_hw_card_driver = {
	.probe = snd_pseudo_hw_card_probe,
	.remove = snd_pseudo_hw_card_remove,
#ifdef CONFIG_PM
	.suspend = snd_pseudo_hw_card_suspend,
	.resume = snd_pseudo_hw_card_resume,
#endif
	.driver = {
		   .name = SND_PSEUDO_HW_CARD_DRIVER},
};

int snd_pseudo_register_hw_driver(void)
{
	return platform_driver_register(&snd_pseudo_hw_card_driver);
}

void snd_pseudo_unregister_hw_driver(void)
{
	platform_driver_unregister(&snd_pseudo_hw_card_driver);
}

struct platform_device *snd_pseudo_register_hw_device(void)
{
	return platform_device_register_simple(SND_PSEUDO_HW_CARD_DRIVER, 0,
					       NULL, 0);
}


stm_object_h snd_hw_player_get_from_entity(struct media_entity *entity)
{
	struct snd_pseudo_hw_player_entity * player;

	player = container_of(entity, struct snd_pseudo_hw_player_entity, entity);
	return (stm_object_h) (player->backend_player);
}
EXPORT_SYMBOL_GPL(snd_hw_player_get_from_entity);
