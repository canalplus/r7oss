/************************************************************************
Copyright (C) 2005 STMicroelectronics. All Rights Reserved.

This file is part of the STLinuxTV Library.

STLinuxTV is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

STLinuxTV is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with player2; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The STLinuxTV Library may alternatively be licensed under a proprietary
license from ST.

Source file name : dvb_module.c
Author :           Julian

Implementation of the LinuxDVB interface to the DVB streamer

Date        Modification                                    Name
----        ------------                                    --------
24-Mar-05   Created                                         Julian

************************************************************************/

#include <linux/sched.h>
#include <linux/syscalls.h>
#include <linux/ioport.h>
#include <linux/bpa2.h>
#include <linux/module.h>
#include <linux/file.h>
#include <linux/kthread.h>
#include <linux/version.h>
#include <asm/uaccess.h>

#include <linux/dvb/audio.h>
#include <linux/dvb/video.h>
#include <linux/dvb/version.h>

#include "dvb_demux.h"		/* provides kernel demux types */

#define USE_KERNEL_DEMUX

#include "dvb_module.h"
#include "dvb_audio.h"
#include "dvb_video.h"
#include "dvb_dmux.h"
#include "dvb_dvr.h"
#include "backend.h"
#include "display.h"
#include "audio_out.h"
#ifdef CONFIG_STLINUXTV_HDMIRX
#include "stmvidextin_hdmirx.h"
#endif
#include "stat.h"

#define MODULE_NAME     "STM Streamer"

/* You would like to see dvb_demux.c (AudioId, ..),
 * if number of devices per adapter exceeds 8 */
#define MAX_DMX_DEVICES_PER_ADAPTER		8
#define MAX_AUD_DEVICES_PER_ADAPTER		25
#define MAX_VID_DEVICES_PER_ADAPTER		25

static int dvb_max_dmx_devices_per_adapter = MAX_DMX_DEVICES_PER_ADAPTER;
static int dvb_max_aud_devices_per_adapter = MAX_AUD_DEVICES_PER_ADAPTER;
static int dvb_max_vid_devices_per_adapter = MAX_VID_DEVICES_PER_ADAPTER;


module_param(dvb_max_dmx_devices_per_adapter, int, S_IWUSR);
module_param(dvb_max_aud_devices_per_adapter, int, S_IWUSR);
module_param(dvb_max_vid_devices_per_adapter, int, S_IWUSR);


static struct DvbContext_s *DvbContext;
static struct dvb_adapter DvbAdapter = {
	.num = -1,
};

int StmLoadModule(void)
{
	int Result;
	int i;
	short int AdapterNumbers[] = { 1 };
	char InitialName[32];

	DvbContext =
	    kzalloc(sizeof(struct DvbContext_s), GFP_KERNEL);
	if (DvbContext == NULL) {
		DVB_ERROR("Unable to allocate device memory\n");
		return -ENOMEM;
	}
	DvbContext->DvbAdapter = &DvbAdapter;

#if DVB_API_VERSION < 5
	Result =
	    dvb_register_adapter(DvbContext->DvbAdapter, MODULE_NAME,
				 THIS_MODULE, NULL);
#else
	Result =
	    dvb_register_adapter(DvbContext->DvbAdapter, MODULE_NAME,
				 THIS_MODULE, NULL, AdapterNumbers);
#endif

	if (Result < 0) {
		DVB_ERROR("Failed to register adapter (%d)\n", Result);
		kfree(DvbContext);
		DvbContext = NULL;
		return -ENOMEM;
	}

	mutex_init(&(DvbContext->Lock));
	mutex_lock(&(DvbContext->Lock));

	if ((dvb_max_dmx_devices_per_adapter > MAX_DMX_DEVICES_PER_ADAPTER)
	    || (dvb_max_dmx_devices_per_adapter < 0))
		dvb_max_dmx_devices_per_adapter = MAX_DMX_DEVICES_PER_ADAPTER;

	if ((dvb_max_aud_devices_per_adapter > MAX_AUD_DEVICES_PER_ADAPTER)
	    || (dvb_max_aud_devices_per_adapter < 0))
		dvb_max_aud_devices_per_adapter = MAX_AUD_DEVICES_PER_ADAPTER;

	if ((dvb_max_vid_devices_per_adapter > MAX_VID_DEVICES_PER_ADAPTER)
	    || (dvb_max_vid_devices_per_adapter < 0))
		dvb_max_vid_devices_per_adapter = MAX_VID_DEVICES_PER_ADAPTER;

	DvbContext->demux_dev_nb = dvb_max_dmx_devices_per_adapter;
	DvbContext->audio_dev_off = dvb_max_dmx_devices_per_adapter;
	DvbContext->audio_dev_nb = dvb_max_aud_devices_per_adapter;
	DvbContext->video_dev_off = DvbContext->audio_dev_off + dvb_max_aud_devices_per_adapter;
	DvbContext->video_dev_nb = dvb_max_vid_devices_per_adapter;

	/* Allocate memory for Audio - Video - Playback device context */
	DvbContext->AudioDeviceContext =
	    kzalloc((sizeof(struct AudioDeviceContext_s) *
		     DvbContext->audio_dev_nb),
		    GFP_KERNEL);
	if (!DvbContext->AudioDeviceContext) {
		printk(KERN_ERR
		       "%s: failed to init memory for Audio device context\n",
		       __func__);
		kfree(DvbContext);
		return -ENOMEM;
	}

	DvbContext->VideoDeviceContext =
	    kzalloc((sizeof(struct VideoDeviceContext_s) *
		     DvbContext->video_dev_nb),
		    GFP_KERNEL);
	if (!DvbContext->VideoDeviceContext) {
		printk(KERN_ERR
		       "%s: failed to init memory for Video device context\n",
		       __func__);
		kfree(DvbContext->AudioDeviceContext);
		kfree(DvbContext);
		return -ENOMEM;
	}

	DvbContext->PlaybackDeviceContext =
	    kzalloc((sizeof(struct PlaybackDeviceContext_s) *
		     (DvbContext->demux_dev_nb + DvbContext->audio_dev_nb + DvbContext->video_dev_nb)),
		    GFP_KERNEL);
	if (!DvbContext->PlaybackDeviceContext) {
		printk(KERN_ERR
		       "%s: failed to init memory for Playback device context\n",
		       __func__);
		kfree(DvbContext->VideoDeviceContext);
		kfree(DvbContext->AudioDeviceContext);
		kfree(DvbContext);
		return -ENOMEM;
	}

	for (i = 0; i < (DvbContext->demux_dev_nb + DvbContext->audio_dev_nb + DvbContext->video_dev_nb); i++ ){
		DvbContext->PlaybackDeviceContext[i].Id = i;
		DvbContext->PlaybackDeviceContext[i].DvbContext = DvbContext;
		mutex_init(&DvbContext->PlaybackDeviceContext[i].DemuxWriteLock);
		mutex_init(&DvbContext->PlaybackDeviceContext[i].Playback_alloc_mutex);
	}

	for (i = 0; i < DvbContext->demux_dev_nb; i++) {
		struct PlaybackDeviceContext_s *Context =
		    &DvbContext->PlaybackDeviceContext[i];

		struct dvb_demux *DvbDemux = &Context->DvbDemux;
		struct dmxdev *DmxDevice = &Context->DmxDevice;
		struct dvb_device *DvrDevice;

#if defined USE_KERNEL_DEMUX
		memset(DvbDemux, 0, sizeof(struct dvb_demux));
		DvbDemux->dmx.capabilities =
		    DMX_TS_FILTERING | DMX_SECTION_FILTERING |
		    DMX_MEMORY_BASED_FILTERING;
		DvbDemux->priv = Context;
		DvbDemux->filternum = 32;
		DvbDemux->feednum = 32;
		DvbDemux->start_feed = StartFeed;
		DvbDemux->stop_feed = StopFeed;
		DvbDemux->write_to_decoder = NULL;
		Result = dvb_dmx_init(DvbDemux);
		if (Result < 0) {
			DVB_ERROR("dvb_dmx_init failed (errno = %d)\n", Result);
			return Result;
		}

		memset(DmxDevice, 0, sizeof(struct dmxdev));
		DmxDevice->filternum = DvbDemux->filternum;
		DmxDevice->demux = &DvbDemux->dmx;
		DmxDevice->capabilities = 0;
		Result =
		    dvb_dmxdev_init(DmxDevice,
				    DvbContext->DvbAdapter);
		if (Result < 0) {
			DVB_ERROR("dvb_dmxdev_init failed (errno = %d)\n",
				  Result);
			dvb_dmx_release(DvbDemux);
			return Result;
		}
		DvrDevice = DvrInit(DmxDevice->dvr_dvbdev->fops);
		/* Unregister the built-in dvr device and replace it with our own version */
		dvb_unregister_device(DmxDevice->dvr_dvbdev);
		dvb_register_device(DvbContext->DvbAdapter,
				    &DmxDevice->dvr_dvbdev,
				    DvrDevice, DmxDevice, DVB_DEVICE_DVR);

		Context->MemoryFrontend.source = DMX_MEMORY_FE;
		Result =
		    DvbDemux->dmx.add_frontend(&DvbDemux->dmx,
					       &Context->MemoryFrontend);
		if (Result < 0) {
			DVB_ERROR("add_frontend failed (errno = %d)\n", Result);
			dvb_dmxdev_release(DmxDevice);
			dvb_dmx_release(DvbDemux);
			return Result;
		}
#else
		dvb_register_device(DvbContext->DvbAdapter,
				    &Context->DemuxDevice,
				    DemuxInit(Context),
				    Context, DVB_DEVICE_DEMUX);

		dvb_register_device(DvbContext->DvbAdapter,
				    &Context->DvrDevice,
				    DvrInit(Context),
				    Context, DVB_DEVICE_DVR);
#endif

		Context->dvr_in = kmalloc(65536, GFP_KERNEL);	/* 128Kbytes is quite a lot per device. */
		Context->dvr_out = kmalloc(65536, GFP_KERNEL);	/* However allocating on each write is expensive. */
	}

	for (i = 0; i < DvbContext->audio_dev_nb; i++) {
		struct AudioDeviceContext_s *AudioContext =
		    &DvbContext->AudioDeviceContext[i];

		AudioContext->Id = i;
		AudioContext->StreamType = STREAM_TYPE_TRANSPORT;
		AudioContext->DvbContext = DvbContext;
		AudioContext->PlaySpeed = DVB_SPEED_NORMAL_PLAY;

		mutex_init(&AudioContext->auddev_mutex);
		mutex_init(&AudioContext->audops_mutex);
		mutex_init(&AudioContext->audwrite_mutex);

		init_waitqueue_head(&AudioContext->AudioEvents.WaitQueue);
		mutex_init(&AudioContext->AudioEvents.Lock);

		dvb_register_device(DvbContext->DvbAdapter,
				    &AudioContext->AudioDevice,
				    AudioInit(AudioContext),
				    AudioContext, DVB_DEVICE_AUDIO);
		AudioInitSubdev(AudioContext);

		stlinuxtv_stat_init_device(&AudioContext->AudioClassDevice);
		snprintf(InitialName, sizeof(InitialName), "dvb%d.audio%d",
			 DvbContext->DvbAdapter->num, i);
		AudioContext->AudioClassDevice.init_name = InitialName;
		Result = device_register(&AudioContext->AudioClassDevice);
		if (0 != Result)
			DVB_ERROR("Unable to register %s diagnostic device\n",
				  InitialName);
		Result = AudioInitSysfsAttributes(AudioContext);
		if (0 != Result)
			DVB_ERROR("Unable to create attributes for %s\n",
				  InitialName);
	}

	for (i = 0; i < DvbContext->video_dev_nb; i++) {
		struct VideoDeviceContext_s *VideoContext =
		    &DvbContext->VideoDeviceContext[i];

		VideoContext->Id = i;
		VideoContext->StreamType = STREAM_TYPE_TRANSPORT;
		VideoContext->DvbContext = DvbContext;
		VideoContext->PlaySpeed = DVB_SPEED_NORMAL_PLAY;

		mutex_init(&VideoContext->viddev_mutex);
		mutex_init(&VideoContext->vidops_mutex);
		mutex_init(&VideoContext->vidwrite_mutex);

		init_waitqueue_head(&VideoContext->VideoEvents.WaitQueue);
		mutex_init(&VideoContext->VideoEvents.Lock);

		dvb_register_device(DvbContext->DvbAdapter,
				    &VideoContext->VideoDevice,
				    VideoInit(VideoContext),
				    VideoContext, DVB_DEVICE_VIDEO);
		VideoInitSubdev(VideoContext);


		stlinuxtv_stat_init_device(&VideoContext->VideoClassDevice);
		snprintf(InitialName, sizeof(InitialName), "dvb%d.video%d",
			 DvbContext->DvbAdapter->num, i);
		VideoContext->VideoClassDevice.init_name = InitialName;
		Result = device_register(&VideoContext->VideoClassDevice);
		if (0 != Result)
			DVB_ERROR("Unable to register %s diagnostic device\n",
				  InitialName);
		Result = VideoInitSysfsAttributes(VideoContext);
		if (0 != Result)
			DVB_ERROR("Unable to create attributes for %s\n",
				  InitialName);
	}

	mutex_unlock(&(DvbContext->Lock));

	DVB_DEBUG("STM stream device loaded\n");

	return 0;
}

void StmUnloadModule(void)
{
	int i;

	for (i = 0; i < DvbContext->video_dev_nb; i++) {
		struct VideoDeviceContext_s *VideoContext =
		    &DvbContext->VideoDeviceContext[i];

		/* Unregister video device */
		VideoRemoveSysfsAttributes(VideoContext);

		device_unregister(&VideoContext->VideoClassDevice);

		/* Unregister Video subdev */
		VideoReleaseSubdev(VideoContext);

		dvb_unregister_device(VideoContext->VideoDevice);
	}

	for (i = 0; i < DvbContext->audio_dev_nb; i++) {
		struct AudioDeviceContext_s *AudioContext =
		    &DvbContext->AudioDeviceContext[i];

		/* Unregister audio device */
		AudioRemoveSysfsAttributes(AudioContext);

		device_unregister(&AudioContext->AudioClassDevice);

		AudioReleaseSubdev(AudioContext);

		dvb_unregister_device(AudioContext->AudioDevice);
	}

	for (i = 0; i < DvbContext->demux_dev_nb; i++) {
		struct PlaybackDeviceContext_s *Context =
		    &DvbContext->PlaybackDeviceContext[i];

		struct dvb_demux *DvbDemux = &Context->DvbDemux;
		struct dmxdev *DmxDevice = &Context->DmxDevice;

		kfree(Context->dvr_in);
		kfree(Context->dvr_out);

#if defined USE_KERNEL_DEMUX
		DvbDemux->dmx.remove_frontend(&DvbDemux->dmx, &Context->MemoryFrontend);

		dvb_dmxdev_release(DmxDevice);

		dvb_dmx_release(DvbDemux);
#else
		dvb_unregister_device(Context->DvrDevice);

		dvb_unregister_device(Context->DemuxDevice);
#endif
	}

	kfree(DvbContext->PlaybackDeviceContext);
	kfree(DvbContext->AudioDeviceContext);
	kfree(DvbContext->VideoDeviceContext);

	dvb_unregister_adapter(DvbContext->DvbAdapter);

	kfree(DvbContext);

	DVB_DEBUG("STM stream device unloaded\n");

	return;
}
