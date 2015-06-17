#include <linux/module.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include "ksound.h"

MODULE_LICENSE("GPL");

static unsigned int playback_card = 1;
static unsigned int capture_card = 5;
static unsigned int nr_channels = 2;
static unsigned int sample_depth = 32;
static unsigned int sample_rate = 44100;
module_param(playback_card, uint, S_IRUGO);
module_param(capture_card, uint, S_IRUGO);
module_param(nr_channels, uint, S_IRUGO);
module_param(sample_depth, uint, S_IRUGO);
module_param(sample_rate, uint, S_IRUGO);

static struct task_struct *kreplay_kthread;


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
				printk
				    ("Can't recovery from suspend, prepare failed: %s\n",
				     snd_strerror(err));
		}
#endif
		BUG();
		return -1;
	}
	return err;
}

static ksnd_pcm_t *open(int card, int capture_not_playback)
{
	ksnd_pcm_t *handle = NULL;
	int res;

	do {
		res = ksnd_pcm_open(&handle, card, 0, capture_not_playback ? SND_PCM_STREAM_CAPTURE : SND_PCM_STREAM_PLAYBACK);
		if (res < 0) {
			printk("Cannot open ALSA card %d\n", card);
			break;
		}

		res = ksnd_pcm_set_params(handle, nr_channels, sample_depth, sample_rate, 4410, 44100);
		if (res < 0) {
			printk("Cannot initialize parameters on ALSA card %d\n", card);
			break;
		}
	} while (0);

	if (res < 0) {
		if (handle)
			ksnd_pcm_close(handle);
		handle = NULL;
	}

	return handle;
}

static int kreplay(void *unused)
{
	ksnd_pcm_t *playback_handle, *capture_handle;
	int res = 0;
	snd_pcm_uframes_t buffer_frames, period_frames;

	playback_handle = open(playback_card, 0);
	if (playback_handle == NULL) {
		kreplay_kthread = NULL;
		return -1;
	}

	capture_handle = open(capture_card, 1);
	if (capture_handle == NULL)	{
		ksnd_pcm_close(playback_handle);
		kreplay_kthread = NULL;
		return -1;
	}

	// Let's assume that playback & capture period sizes are exact
	ksnd_pcm_get_params(capture_handle, &buffer_frames, &period_frames);
//	printk("period_frames = %lu\n", period_frames);

	ksnd_pcm_start(capture_handle);
	ksnd_pcm_start(playback_handle);

	while (!kthread_should_stop()) {
		const snd_pcm_channel_area_t *capture_areas;
		snd_pcm_uframes_t capture_offset, capture_frames;
		snd_pcm_uframes_t got_frames;
		snd_pcm_sframes_t commited_frames;

		capture_frames = ksnd_pcm_avail_update(capture_handle);
		while (capture_frames < period_frames) {
//			printk("capture_frames=%lu, waiting for more...\n", capture_frames);
			res = ksnd_pcm_wait(capture_handle, -1);
			if (res <= 0) {
				printk("Failed to wait for capture period expiry\n");
				res = -1;
				break;
			}
			if (kthread_should_stop()) {
				res = -1;
				break;
			}
			capture_frames = ksnd_pcm_avail_update(capture_handle);
		}
		if (res < 0)
			break;

		capture_frames = capture_frames - (capture_frames % period_frames);

//		printk("Captured %lu frames...\n", capture_frames);

		res = ksnd_pcm_mmap_begin(capture_handle, &capture_areas, &capture_offset, &capture_frames);
		if (res < 0) {
			printk("Failed to mmap capture buffer\n");
			break;
		}

//		printk("capture_areas[0].addr=%p, capture_areas[0].first=%d, capture_areas[0].step=%d, capture_offset=%lu, capture_frames=%lu\n",
//				capture_areas[0].addr, capture_areas[0].first, capture_areas[0].step, capture_offset, capture_frames);

		got_frames = capture_frames;
		while (got_frames >= period_frames) {
			const snd_pcm_channel_area_t *playback_areas;
			snd_pcm_uframes_t playback_offset, playback_frames;
			snd_pcm_uframes_t playback_avail;
			void *dest, *src;
			size_t size;

			playback_avail = ksnd_pcm_avail_update(playback_handle);
			while (playback_avail < period_frames) {
//				printk("playback_avail=%lu, waiting for more...\n", playback_avail);
				res = ksnd_pcm_wait(playback_handle, -1);
				if (res <= 0) {
					printk("Failed to wait for playback period expiry\n");
					res = -1;
					break;
				}
				if (kthread_should_stop()) {
					res = -1;
					break;
				}
				playback_avail = ksnd_pcm_avail_update(playback_handle);
			}
			if (res < 0)
				break;

//			printk("Playback can play %lu frames, %lu frames left...\n", playback_avail, got_frames);

			if (playback_avail < got_frames)
				playback_frames = playback_avail;
			else
				playback_frames = got_frames;

			res = ksnd_pcm_mmap_begin(playback_handle, &playback_areas, &playback_offset, &playback_frames);
			if (res < 0) {
				printk("Failed to mmap playback buffer\n");
				break;
			}

			got_frames -= playback_frames;

//			printk("playback_areas[0].addr=%p, playback_areas[0].first=%d, playback_areas[0].step=%d, playback_offset=%lu, playback_frames=%lu\n",
//					playback_areas[0].addr, playback_areas[0].first, playback_areas[0].step, playback_offset, playback_frames);

			size = playback_frames * (sample_depth * nr_channels / 8);
			src = capture_areas[0].addr + capture_areas[0].first / 8 + capture_offset * capture_areas[0].step / 8;
			dest = playback_areas[0].addr + playback_areas[0].first / 8 + playback_offset * playback_areas[0].step / 8;

//			printk("memcpy(dest=%p, src=%p, size=%d)\n", dest, src, size);

			memcpy(dest, src, size);

			commited_frames = ksnd_pcm_mmap_commit(playback_handle, playback_offset, playback_frames);
			if (commited_frames < 0 || (snd_pcm_uframes_t) commited_frames != playback_frames) {
				printk("Playback XRUN!\n");
				res = xrun_recovery(playback_handle, commited_frames >= 0 ? -EPIPE : commited_frames);
				if (res < 0) {
					printk("MMAP commit error\n");
					break;
				}
			}
		}
		if (res < 0)
			break;

		commited_frames = ksnd_pcm_mmap_commit(capture_handle, capture_offset, capture_frames - got_frames);
		if (commited_frames < 0 || (snd_pcm_uframes_t) commited_frames != capture_frames - got_frames) {
			printk("Capture XRUN!\n");
			res = xrun_recovery(capture_handle, commited_frames >= 0 ? -EPIPE : commited_frames);
			if (res < 0) {
				printk("MMAP commit error\n");
				break;
			}
			ksnd_pcm_start(capture_handle);
		}
	}

	ksnd_pcm_close(playback_handle);
	ksnd_pcm_close(capture_handle);

	kreplay_kthread = NULL;

	return res;
}

static int __init kreplay_module_init(void)
{
	kreplay_kthread = kthread_run(kreplay, 0, "kreplay");
	if (!kreplay_kthread) {
		printk("Failed to spawn kthread\n");
		return PTR_ERR(kreplay_kthread);
	}

	printk(KERN_DEBUG "kreplay: Built %s %s\n", __DATE__, __TIME__);
	return 0;
}

static void __exit kreplay_module_exit(void)
{
	if (kreplay_kthread)
		kthread_stop(kreplay_kthread);
}

module_init(kreplay_module_init);
module_exit(kreplay_module_exit);
