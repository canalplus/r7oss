/*
 * Audio support for codec Philips UDA1380
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Copyright (c) 2005 Giorgio Padrin <giorgio@mandarinlogiq.org>
 */

#ifndef __SOUND_UDA1380_H
#define __SOUND_UDA1380_H

#include <linux/i2c.h>

enum snd_uda1380_capture_source {
	SND_UDA1380_CAP_SOURCE_LINE_IN,
	SND_UDA1380_CAP_SOURCE_MIC
};

struct snd_uda1380 {
	struct semaphore sem;

	u16 regs[0x29];	/* registers cache */

	unsigned int
		powered_on:1,
		playback_on:1,
		playback_switch_ureq:1,
		playback_clock_on:1,
		playback_stream_opened:1,
		capture_on:1,
		capture_switch_ureq:1,
		capture_clock_on:1,
		capture_stream_opened:1;

	/* -- configuration (to fill before activation) -- */
	unsigned int
		line_in_connected:1,
		mic_connected:1,
		hp_connected:1,
		hp_or_line_out;
	enum snd_uda1380_capture_source	capture_source;

	void (*power_on_chip)(int on);
	void (*reset_pin)(int on);
	void (*line_out_on)(int on);
	void (*line_in_on)(int on);
	void (*mic_on)(int on);

	struct i2c_client i2c_client; /* to fill .adapter and .addr */
	/* ----------------------------------------------- */

	struct {
		u8 start_reg;
		unsigned int count;
	} cache_dirty[3];

	struct {
		int detected;
		struct workqueue_struct *wq;
		struct work_struct w;
	} hp_detected;
};


/* Note: the functions (de)activate, (open|close)_stream, suspend|resume
 * require the SYSCLK running
 */

/* don't forget to specify I2C adapter and address in i2c_client field */
int snd_uda1380_activate(struct snd_uda1380 *uda);

void snd_uda1380_deactivate(struct snd_uda1380 *uda);
int snd_uda1380_add_mixer_controls(struct snd_uda1380 *uda, struct snd_card *card);
int snd_uda1380_open_stream(struct snd_uda1380 *uda, int stream);
int snd_uda1380_close_stream(struct snd_uda1380 *uda, int stream);
int snd_uda1380_suspend(struct snd_uda1380 *uda, pm_message_t state);
int snd_uda1380_resume(struct snd_uda1380 *uda);

void snd_uda1380_hp_connected(struct snd_uda1380 *uda, int connected); /* non atomic context */
void snd_uda1380_hp_detected(struct snd_uda1380 *uda, int detected); /* atomic context */

#endif /* __SOUND_UDA1380_H */
