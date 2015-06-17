/*
 * ktone.c
 *
 * Kernel based tone generator (a ksound example program)
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include "ksound.h"

#include <linux/types.h>
#include <linux/kernel.h>

MODULE_LICENSE("GPL");

static unsigned int card = 0;
static unsigned int device = 0;
static unsigned int nrchannels = 2;
static unsigned int sampledepth = 24;
static unsigned int samplerate = 48000;
static unsigned int freq = 440;

module_param(card, uint, S_IRUGO);
module_param(device, uint, S_IRUGO);
module_param(nrchannels, uint, S_IRUGO);
module_param(sampledepth, uint, S_IRUGO);
module_param(samplerate, uint, S_IRUGO);
module_param(freq, uint, S_IRUGO | S_IWUSR);

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

	return (short) (sign * (32767l - ((16570l * x * x) >> 12)));
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

	for (i=0; i<nrsamples; i++) {
		for (j=0; j<nrchannels; j++) {
			/* samples must occupy top 16-bits */
			p[nrchannels*i + j] =
			    ((unsigned int)sins(phase >> 16)) << 16;
		}

		phase += step;

		/* phase does not wrap naturally (i.e. when aliased to 0 degrees)
		 * so impose out own wrap
		 */
		while ((phase) > (360 << 16))
			phase -= 360 << 16;
	}

	*phasep = phase;
}

static int xrun_recovery(ksnd_pcm_t *handle, int err)
{
	if (err == -EPIPE) {	/* under-run */
		/*err = snd_pcm_prepare(handle); */
		err = ksnd_pcm_prepare(handle);
		if (err < 0)
			printk
			    ("Can't recovery from underrun, prepare failed: %d\n",
			     err);
		return 0;
	} else if (err == -ESTRPIPE) {
#if 0
		while ((err = snd_pcm_resume(handle)) == -EAGAIN)
			sleep(1);	/* wait until the suspend flag is released */
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
	unsigned int *samples;
	unsigned int state = 0;
	const snd_pcm_uframes_t period_size = 1536;

	samples = kmalloc(nrchannels * period_size, GFP_KERNEL);
	if (IS_ERR(samples)) {
		printk("Cannot allocate temporary sample memory\n");
		return PTR_ERR(samples);
	}

	printk("Using ALSA device %d:%d...\n", card, device);

	res = ksnd_pcm_open(&handle, card, device, SND_PCM_STREAM_PLAYBACK);
	if (0 != res) {
		printk("Cannot open ALSA device\n");
		goto do_free;
	}

	res = ksnd_pcm_set_params(handle, nrchannels, sampledepth, samplerate, period_size, period_size * 3);
	if (0 != res) {
		printk("Cannot set parameters on ALSA device\n");
		goto do_close;
	}

	do {
#if 0
		generate_signal(samples, nrchannels, period_size, samplerate,
				freq, &state);
		res = ksnd_pcm_writei(handle, samples, period_size, nrchannels);
		if (0 != res) {
			printk("Failed to write samples\n");
			goto do_close;
		}
#else
		snd_pcm_uframes_t avail, size;

		avail = ksnd_pcm_avail_update(handle);
		if (avail < period_size) {
			res = ksnd_pcm_wait(handle, -1);
			if (res <= 0) {
				printk("Failed to wait for period expiry\n");
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

			res =
			    ksnd_pcm_mmap_begin(handle, &my_areas, &offset,
						&frames);
			if (res < 0) {
				printk("Failed to mmap buffer\n");
				goto do_close;
			}

			samples = my_areas[0].addr;
			samples += my_areas[0].first / 8;
			samples += offset * my_areas[0].step / 8;

			/*printk("offset %d base %p samples %p\n", offset, my_areas[0].addr, samples); */
			generate_signal(samples, nrchannels, frames, samplerate,
					freq, &state);

			commitres =
			    ksnd_pcm_mmap_commit(handle, offset, frames);
			if (commitres < 0
			    || (snd_pcm_uframes_t) commitres != frames) {
				if ((res =
				     xrun_recovery(handle,
						   commitres >=
						   0 ? -EPIPE : commitres)) <
				    0) {
					printk("MMAP commit error\n");
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
	kfree(samples);

	return res;
}

static struct task_struct *ktone_kthread;

int __init ktone_module_init(void)
{
	ktone_kthread = kthread_run(ktone, 0, "ktone");
	if (!ktone_kthread) {
		printk("Failed to spawn kthread\n");
		return PTR_ERR(ktone_kthread);
	}

	printk(KERN_DEBUG "ktone: Built %s %s\n", __DATE__, __TIME__);
	return 0;
}

void __exit ktone_module_deinit(void)
{
	int res = kthread_stop(ktone_kthread);
	if (0 != res) {
		printk("ktone failed to run correctly\n");
	}
}

module_init(ktone_module_init);
module_exit(ktone_module_deinit);
