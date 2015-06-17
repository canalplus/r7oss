#include <linux/module.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include "ksound-streaming.h"

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

ksnd_pcm_t *playback_handle, *capture_handle;
ksnd_pcm_streaming_t streaming;

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

static int __init kst_module_init(void)
{
	int result;

	playback_handle = open(playback_card, 0);
	if (playback_handle == NULL) {
		printk(KERN_ERR "open(playback_card, 0) == NULL\n");
		return -1;
	}

	capture_handle = open(capture_card, 1);
	if (capture_handle == NULL)	{
		printk(KERN_ERR "open(capture_card, 1) == NULL\n");
		ksnd_pcm_close(playback_handle);
		return -2;
	}

	result = ksnd_pcm_streaming_start(&streaming, capture_handle, playback_handle);
	if (result != 0) {
		printk(KERN_ERR "ksnd_pcm_streaming_start() == %d\n", result);
		ksnd_pcm_close(capture_handle);
		ksnd_pcm_close(playback_handle);
		return result;
	}

	printk(KERN_INFO "Streaming started...\n");

	return 0;
}

static void __exit kst_module_exit(void)
{
	ksnd_pcm_streaming_stop(streaming);
	ksnd_pcm_close(capture_handle);
	ksnd_pcm_close(playback_handle);
	printk(KERN_INFO "Streaming stopped...\n");
}

module_init(kst_module_init);
module_exit(kst_module_exit);
