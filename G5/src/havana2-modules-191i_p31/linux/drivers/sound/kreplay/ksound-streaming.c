/*
 * ALSA library for kernel clients (ksound)
 * Stream redirection routines
 * 
 * Copyright (c) 2007 STMicroelectronics R&D Limited <pawel.moll@st.com>
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

#include <sound/driver.h>
#include <sound/core.h>
#include <sound/pcm.h>

#include <asm/bitops.h>
#include <linux/stm/stm-dma.h>
#include <linux/dma-mapping.h>

#include "ksound-streaming.h"



EXPORT_SYMBOL(ksnd_pcm_streaming_start);
EXPORT_SYMBOL(ksnd_pcm_streaming_stop);



#define ERROR(fmt, args...) printk(KERN_ERR "%s:%d: ERROR in %s: " fmt, \
		__FILE__, __LINE__, __FUNCTION__, ## args)



#define FLAG_BUSY 0
#define FLAG_STOPPED 1

typedef enum { magic_bad = 0xdeadbeef, magic_good = 0xfee1900d } magic_t;

struct ksnd_pcm_streaming {
	unsigned long flags;

	int fdma_channel;
	struct stm_dma_params fdma_params;

	ksnd_pcm_t *capture_handle;
	snd_pcm_uframes_t capture_offset;

	ksnd_pcm_t *playback_handle;
	snd_pcm_uframes_t playback_offset;
	snd_pcm_uframes_t playback_frames;

	void *tmp;

	magic_t magic;
};



// Callback called each time the ALSA capture gets a data period
static void period_captured_callback(struct snd_pcm_substream *substream)
{
	int result;
	struct ksnd_pcm_streaming *streaming = substream->runtime->private_data;
  const snd_pcm_channel_area_t *capture_areas;
	snd_pcm_uframes_t capture_frames;
	const snd_pcm_channel_area_t *playback_areas;
	void *dest, *src;
	ssize_t size;

	BUG_ON(streaming->magic != magic_good);

	// Check if the transfer wasn't stopped by any chance...
	
	if (test_bit(FLAG_STOPPED, &streaming->flags))
		return;

	// Check if there is no transfers in progress...

	if (test_and_set_bit(FLAG_BUSY, &streaming->flags) != 0)
		return;

	// Get number of available periods (modulo period size)
	
	capture_frames = ksnd_pcm_avail_update(streaming->capture_handle);
 	capture_frames -= capture_frames % substream->runtime->period_size;

	// "Mmap" captured data
	
	result = ksnd_pcm_mmap_begin(streaming->capture_handle,
			&capture_areas, &streaming->capture_offset, &capture_frames);
	if (result < 0) {
		ERROR("Failed to mmap capture buffer!\n");
		return;
	}

	// "Mmap" playback buffer
	
	streaming->playback_frames = capture_frames;

	result = ksnd_pcm_mmap_begin(streaming->playback_handle,
			&playback_areas, &streaming->playback_offset, &streaming->playback_frames);
	if (result < 0) {
		ERROR("Failed to mmap playback buffer\n");
		return;
	}

	// Setup FDMA transfer
	
	src = capture_areas[0].addr + capture_areas[0].first / 8 +
			streaming->capture_offset * capture_areas[0].step / 8;
	dest = playback_areas[0].addr + playback_areas[0].first / 8 +
			streaming->playback_offset * playback_areas[0].step / 8;
	size = frames_to_bytes(substream->runtime, streaming->playback_frames);

	dma_params_addrs(&streaming->fdma_params, virt_to_phys(src), virt_to_phys(dest), size);

	result = dma_compile_list(streaming->fdma_channel, &streaming->fdma_params, GFP_KERNEL);
	if (result < 0) {
		ERROR("Can't compile FDMA parameters!\n");
		free_dma(streaming->fdma_channel);
		return;
	}

	// Launch the transfer
	
	result = dma_xfer_list(streaming->fdma_channel, &streaming->fdma_params);
	if (result < 0) {
		ERROR("Failed to launch FDMA tranfser!\n");
		return;
	}
}



static void transfer_done_callback(unsigned long param)
{
	int result;
	struct ksnd_pcm_streaming *streaming = (struct ksnd_pcm_streaming *)param;
	snd_pcm_sframes_t commited_frames;

	BUG_ON(streaming->magic != magic_good);
	BUG_ON(test_bit(FLAG_BUSY, &streaming->flags) != 1);

	// Check if the transfer wasn't stopped by any chance...
	
	if (test_bit(FLAG_STOPPED, &streaming->flags))
		return;

	// "Commit" playback data

	commited_frames = ksnd_pcm_mmap_commit(streaming->playback_handle,
			streaming->playback_offset, streaming->playback_frames);
	if (commited_frames < 0 ||
			(snd_pcm_uframes_t)commited_frames != streaming->playback_frames) {
		ERROR("Playback XRUN!\n");
		result = ksnd_pcm_prepare(streaming->playback_handle);
		if (result != 0) {
			ERROR("Can't recover from playback XRUN!\n");
			return;
		}
	}

	// "Commit" captured data
	
	commited_frames = ksnd_pcm_mmap_commit(streaming->capture_handle, 
			streaming->capture_offset, streaming->playback_frames);
	if (commited_frames < 0 ||
			(snd_pcm_uframes_t)commited_frames != streaming->playback_frames) {
		ERROR("Capture XRUN!\n");
		result = ksnd_pcm_prepare(streaming->capture_handle);
		if (result != 0) {
			ERROR("Can't recover from capture XRUN!\n");
			return;
		}
		ksnd_pcm_start(streaming->capture_handle);
	}

	// Release the lock

	clear_bit(FLAG_BUSY, &streaming->flags);

	// If another data period has been received in meantime, trigger it not
	// (period_captured_callback could be skipped because of transfer	in progress)
	
	if (ksnd_pcm_avail_update(streaming->capture_handle) >= 
			streaming->capture_handle->substream->runtime->period_size)
		period_captured_callback(streaming->capture_handle->substream);
}



static void transfer_error_callback(unsigned long param)
{
	printk("ksound-streaming: FDMA transfer error!\n");
}



int ksnd_pcm_streaming_start(ksnd_pcm_streaming_t *handle,
		ksnd_pcm_t *capture, ksnd_pcm_t *playback)
{
	int result;
	struct ksnd_pcm_streaming *streaming;
	const char *fdmac_id[] = { STM_DMAC_ID, NULL };
	const char *lb_cap[] = { STM_DMA_CAP_LOW_BW, NULL };
	const char *hb_cap[] = { STM_DMA_CAP_HIGH_BW, NULL };

	// Allocate and clean streaming structure
	
	streaming = kzalloc(sizeof(*streaming), GFP_KERNEL);
	if (streaming == NULL) {
		ERROR("Can't get memory for streaming structure!\n");
		return -ENOMEM;
	}

	// Prepare description

	streaming->capture_handle = capture;
	streaming->playback_handle = playback;
	streaming->magic = magic_good;

	// Initialize FDMA
	
	streaming->fdma_channel = request_dma_bycap(fdmac_id, lb_cap, "KSOUND_STREAMING");
	if (streaming->fdma_channel < 0) {
		streaming->fdma_channel = request_dma_bycap(fdmac_id, hb_cap, "KSOUND_STREAMING");
		if (streaming->fdma_channel < 0) {
			ERROR("Can't allocate FDMA channel!\n");
			kfree(streaming);
			return -EBUSY;
		}
	}

	dma_params_init(&streaming->fdma_params, MODE_FREERUNNING, STM_DMA_LIST_OPEN);

	dma_params_comp_cb(&streaming->fdma_params, transfer_done_callback,
			(unsigned long)streaming, STM_DMA_CB_CONTEXT_TASKLET);

	dma_params_err_cb(&streaming->fdma_params, transfer_error_callback,
			(unsigned long)streaming, STM_DMA_CB_CONTEXT_TASKLET);

	dma_params_DIM_1_x_1(&streaming->fdma_params);

	result = dma_compile_list(streaming->fdma_channel, &streaming->fdma_params, GFP_KERNEL);
	if (result < 0) {
		ERROR("Can't compile FDMA parameters!\n");
		free_dma(streaming->fdma_channel);
		kfree(streaming);
		return -EFAULT;
	}

	// Initialize ALSA

	capture->substream->runtime->transfer_ack_end = period_captured_callback;
	BUG_ON(capture->substream->runtime->private_data != NULL);  // It used to be not used ;-)
	capture->substream->runtime->private_data = streaming;

	ksnd_pcm_start(capture);

	// Return handle

	*handle = streaming;

	return 0;
}



void ksnd_pcm_streaming_stop(ksnd_pcm_streaming_t handle)
{
	// Raise the flag

	set_bit(FLAG_STOPPED, &handle->flags);

	// Clean ALSA notifications

	handle->capture_handle->substream->runtime->transfer_ack_end = NULL;

	// Break transfer (if any)
	
	dma_stop_channel(handle->fdma_channel);
	dma_wait_for_completion(handle->fdma_channel);

	// Free FDMA resources
	
	dma_params_free(&handle->fdma_params);
	free_dma(handle->fdma_channel);

	// Free description memory
	handle->magic = magic_bad;
	kfree(handle);
}
