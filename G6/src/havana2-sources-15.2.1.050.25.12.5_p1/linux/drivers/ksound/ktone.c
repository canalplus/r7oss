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

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/kthread.h>

#include <linux/types.h>
#include <linux/kernel.h>

#include "ksound.h"

MODULE_DESCRIPTION("ktones generation (on spawned thread) -- for basic ksound test");
MODULE_AUTHOR("STMicroelectronics");
MODULE_LICENSE("GPL");

static unsigned int card = 0;
static unsigned int device = 0;
static unsigned int nrchannels = 2;
static unsigned int sampledepth = 24;
static unsigned int samplerate = 48000;
static unsigned int freq = 440;
static int thpolprio_inits[2] = {SCHED_NORMAL, 0};

module_param(card, uint, S_IRUGO);
module_param(device, uint, S_IRUGO);
module_param(nrchannels, uint, S_IRUGO);
module_param(sampledepth, uint, S_IRUGO);
module_param(samplerate, uint, S_IRUGO);
module_param(freq, uint, S_IRUGO | S_IWUSR);
module_param_array_named(thread_SE_Ktone, thpolprio_inits, int, NULL, S_IRUGO);

/*
 * This function returns a approximation of 32767 * sin(x) where x is measured
 * in degrees. The algorithm is very fast and does not use division but has a
 * large error deviation (3%) although the algorithm is tuned such that sin(0)
 * and sin(90) are exactly correct.
 *
 * Clearly it would be more accurate to use a table which given the limited
 * number of input cases would not even need to be very big. Basically the
 * algorithm is useless but cute (and should never have used integer degrees as
 * its input).
 */
static short sins(short x)
{
	long sign;

	/* TODO: this routine original implemented cosine so fudge it... */
	x -= 90;

	/* put x into the interval 0 <= x <= 90 */
	x = (x >   0 ? x : 0 - x);
	x = (x < 360 ? x : x % 360);
	x = (x < 180 ? x : 360 - x);
	x = (x <  90 ? (sign = 1, x) : (sign = -1, 180 - x));

	return (short)(sign * (32767l - ((16570l * x * x) >> 12)));
}

static void generate_signal(unsigned int *p,
                            unsigned int nrchannels,
                            unsigned int nrsamples,
                            unsigned int samplerate,
                            unsigned int freq, unsigned int *phasep)
{
	unsigned int phase = *phasep;
	unsigned int step;
	int i, j;

	/* calculate the phase change per sample (phase is Q16.16 and in degrees) */
	step = (freq * 0x10000) / samplerate;
	step *= 360;

	for (i = 0; i < nrsamples; i++) {
		for (j = 0; j < nrchannels; j++) {
			/* samples must occupy top 16-bits */
			p[nrchannels * i + j] =
			        ((unsigned int)sins(phase >> 16)) << 16;
		}

		phase += step;

		/* phase does not wrap naturally (i.e. when aliased to 0 degrees)
		 * so impose out own wrap
		 */
		while ((phase) > (360 << 16)) {
			phase -= 360 << 16;
		}
	}

	*phasep = phase;
}

static int xrun_recovery(ksnd_pcm_t *handle, int err)
{
	if (err == -EPIPE) {	/* under-run */
		/*err = snd_pcm_prepare(handle); */
		err = ksnd_pcm_prepare(handle);
		if (err < 0) {
			pr_err("Error: %s Can't recovery from underrun, prepare failed: %d\n", __func__, err);
		}
		return 0;
	} else if (err == -ESTRPIPE) {
#if 0
		while ((err = snd_pcm_resume(handle)) == -EAGAIN) {
			sleep(1);        /* wait until the suspend flag is released */
		}
		if (err < 0) {
			err = snd_pcm_prepare(handle);
			if (err < 0)
				printf
				("Can't recovery from suspend, prepare failed: %s\n",
				 snd_strerror(err));
		}
#endif
		BUG();
		return -1;
	}
	return err;
}

static int ktone(void *unused)
{
	ksnd_pcm_t *handle;
	int res;
#if 0
	unsigned int *samples;
#endif
	unsigned int state = 0;
	const snd_pcm_uframes_t period_size = 1536;

#if 0
	samples = kmalloc(nrchannels * period_size, GFP_KERNEL);
	if (IS_ERR(samples)) {
		pr_err("Error: %s Cannot allocate temporary sample memory\n", __func__);
		return PTR_ERR(samples);
	}
#endif

	pr_info("Using ALSA device %d:%d..\n", card, device);

	res = ksnd_pcm_open(&handle, card, device, SND_PCM_STREAM_PLAYBACK);
	if (0 != res) {
		pr_err("Error: %s Cannot open ALSA device\n", __func__);
		goto do_free;
	}

	res = ksnd_pcm_set_params(handle, nrchannels, sampledepth, samplerate, period_size, period_size * 3);
	if (0 != res) {
		pr_err("Error: %s Cannot set parameters on ALSA device\n", __func__);
		goto do_close;
	}

	do {
#if 0
		generate_signal(samples, nrchannels, period_size, samplerate,
		                freq, &state);
		res = ksnd_pcm_writei(handle, samples, period_size, nrchannels);
		if (0 != res) {
			pr_err("Error: %s Failed to write samples\n", __func__);
			goto do_close;
		}
#else
		snd_pcm_uframes_t avail, size;

		avail = ksnd_pcm_avail_update(handle);
		if (avail < period_size) {
			res = ksnd_pcm_wait(handle, -1);
			if (res <= 0) {
				pr_err("Error: %s Failed to wait for period expiry\n", __func__);
				goto do_close;
			}
		}

		size = period_size;
		while (size > 0) {
			const snd_pcm_channel_area_t *my_areas;
			snd_pcm_uframes_t offset;
			snd_pcm_uframes_t frames = size;
			snd_pcm_sframes_t commitres;
			void *samples;

			res = ksnd_pcm_mmap_begin(handle, &my_areas, &offset, &frames);
			if (res < 0) {
				pr_err("Error: %s Failed to mmap buffer\n", __func__);
				goto do_close;
			}

			samples = my_areas[0].addr;
			samples += my_areas[0].first / 8;
			samples += offset * my_areas[0].step / 8;

			/*pr_info("offset %d base %p samples %p\n", offset, my_areas[0].addr, samples); */
			generate_signal(samples, nrchannels, frames, samplerate,
			                freq, &state);

			commitres = ksnd_pcm_mmap_commit(handle, offset, frames);
			if (commitres < 0
			    || (snd_pcm_uframes_t) commitres != frames) {
				if ((res =
				             xrun_recovery(handle,
				                           commitres >=
				                           0 ? -EPIPE : commitres)) <
				    0) {
					pr_err("Error: %s mmap commit error\n", __func__);
					goto do_close;
				}
			}
			size -= frames;
		}
#endif
	} while (!kthread_should_stop());

do_close:
	ksnd_pcm_close(handle);

do_free:
#if 0
	kfree(samples);
#endif

	return res;
}

static struct task_struct *ktone_kthread;

int __init ktone_module_init(void)
{
	ktone_kthread = kthread_run(ktone, 0, "SE-Ktone");
	if (!ktone_kthread) {
		pr_err("Error: %s module Failed to spawn kthread\n", __func__);
		return PTR_ERR(ktone_kthread);
	}
	if (0 != thpolprio_inits[1] || SCHED_NORMAL != thpolprio_inits[0]) {
		struct sched_param Param;
		Param.sched_priority = thpolprio_inits[1];
		if (0 != sched_setscheduler(current, thpolprio_inits[0], &Param)) {
			pr_err("Error: %s failed to set scheduling parameters to policy %d priority %d\n", __func__,
			       thpolprio_inits[0], thpolprio_inits[1]);
		}
	}
	pr_info("%s done ok\n", __func__);
	return 0;
}

void __exit ktone_module_deinit(void)
{
	int res = kthread_stop(ktone_kthread);
	if (0 != res) {
		pr_err("Error: %s failed to unload correctly\n", __func__);
	}
	pr_info("%s done ok\n", __func__);
}

module_init(ktone_module_init);
module_exit(ktone_module_deinit);
