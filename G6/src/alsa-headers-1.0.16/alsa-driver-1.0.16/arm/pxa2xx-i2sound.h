/*
 * PXA2xx i2Sound: support for Intel PXA2xx I2S audio
 *
 * Copyright (c) 2005 Giorgio Padrin <giorgio@mandarinlogiq.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * History:
 *
 * 2005-03	Giorgio Padrin		Initial release
 * 2005-05-02	Giorgio Padrin		added event() to board plugins interface
 * 2005-05-14	Giorgio Padrin		Splitted event()
 */ 

#ifndef __SOUND_PXA2xx_I2SOUND_H
#define __SOUND_PXA2xx_I2SOUND_H

#include <sound/driver.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/initval.h>
 
struct snd_pxa2xx_i2sound_board_stream_hw { 
	unsigned int rates;		/* SNDRV_PCM_RATE_* */
	unsigned int rate_min;		/* min rate */
	unsigned int rate_max;		/* max rate */
	unsigned int *rates_table;	/* in case the field rates is not enough */
	unsigned int rates_num;		/* size of rates_table */
};
  
struct snd_pxa2xx_i2sound_board {
	char *name;
	char *desc;	/* short description */
	char *acard_id;	/* ALSA card id */

	unsigned int info;

	struct snd_pxa2xx_i2sound_board_stream_hw streams_hw[2];

	int (*activate)(void);
	void (*deactivate)(void);

	int (*set_rate)(unsigned int rate);

	int (*open_stream)(int stream);
	void (*close_stream)(int stream);

	/* ALSA mixer */
	int (*add_mixer_controls)(struct snd_card *acard);

	/* tune ALSA hw params space at substream opening */
	int (*add_hw_constraints)(struct snd_pcm_substream *asubs);

#ifdef CONFIG_PM
	/* Power Management */
	int (*suspend) (pm_message_t state);
	int (*resume) (void);
#endif
};

#define SND_PXA2xx_I2SOUND_INFO_CLOCK_FROM_PXA	1 << 0
#define SND_PXA2xx_I2SOUND_INFO_CAN_CAPTURE	1 << 1
#define SND_PXA2xx_I2SOUND_INFO_HALF_DUPLEX	1 << 2

void snd_pxa2xx_i2sound_i2slink_get(void);
void snd_pxa2xx_i2sound_i2slink_free(void);

int snd_pxa2xx_i2sound_card_activate(struct snd_pxa2xx_i2sound_board *board);
void snd_pxa2xx_i2sound_card_deactivate(void);

#endif /* __SOUND_PXA2xx_I2SOUND_H */
