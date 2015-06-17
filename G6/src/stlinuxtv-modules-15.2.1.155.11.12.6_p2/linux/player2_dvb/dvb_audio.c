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

Source file name : dvb_audio.c
Author :           Julian

Implementation of linux dvb audio device

Date        Modification                                    Name
----        ------------                                    --------
24-Mar-05   Created                                         Julian

************************************************************************/

#include <linux/module.h>
#include <linux/dvb/video.h>
#include <linux/dvb/audio.h>
#include <linux/version.h>

#include "stmedia.h"
#include "stmedia_export.h"

#include "dvb_module.h"
#include "dvb_audio.h"
#include "dvb_video.h"
#include "backend.h"
#include "stmedia_export.h"
#include "audio_out.h"

#include "stm_v4l2_encode.h"

#include "audio_out.h"
#include "demux_filter.h"

/*{{{  prototypes*/
static int AudioOpen(struct inode *Inode, struct file *File);
static int AudioRelease(struct inode *Inode, struct file *File);
static int AudioIoctl(
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 35))
			     struct inode *Inode,
#endif
			     struct file *File,
			     unsigned int IoctlCode, void *ParamAddress);
static ssize_t dvb_audio_write(struct file *file,
			const char __user *buffer, size_t count, loff_t * ppos);
static unsigned int AudioPoll(struct file *File, poll_table * Wait);
static int AudioIoctlSetAvSync(struct AudioDeviceContext_s *Context,
			       unsigned int State);
static int AudioIoctlChannelSelect(struct AudioDeviceContext_s *Context,
				   audio_channel_select_t Channel);
static int AudioIoctlSetStreamDrivenDualMono(struct AudioDeviceContext_s *Context,
					     bool State);
static int AudioIoctlSetStreamDownmix(struct AudioDeviceContext_s *Context,
                                      unsigned int NumberOfOutputChannels);
static int AudioIoctlSetStreamDrivenStereo(struct AudioDeviceContext_s *Context,
					     bool State);
static int AudioIoctlSetServiceType(struct AudioDeviceContext_s *Context,
				    audio_service_t Type);
static int AudioIoctlSetApplicationType(struct AudioDeviceContext_s *Context,
					audio_application_t Type);
static int AudioIoctlSetRegionType(struct AudioDeviceContext_s *Context,
					audio_region_t Region);
static int AudioIoctlSetProgramReferenceLevel(struct AudioDeviceContext_s *Context,
					      unsigned int prl);
static int AudioIoctlSetSubStreamId(struct AudioDeviceContext_s *Context,
				    unsigned int Id);
static int AudioIoctlSetAacDecoderConfig(struct AudioDeviceContext_s *Context,
					 audio_mpeg4aac_t * AacDecoderConfig);
static int AudioStreamEventSubscriptionCreate(struct AudioDeviceContext_s *Context);
void AudioSetEvent(unsigned int number_of_events,
		   stm_event_info_t * eventsInfo);
static int AudioStreamEventSubscriptionDelete(struct AudioDeviceContext_s *Context);
int AudioIoctlSetSpeed(struct AudioDeviceContext_s *Context, int Speed,
			      int PlaySpeedUpdate);
int AudioIoctlSetPlayInterval(struct AudioDeviceContext_s *Context,
			      audio_play_interval_t * PlayInterval);
static audio_encoding_t AudioSE2STLinuxTVCoding(stm_se_stream_encoding_t  audio_coding_type);
static int AudioStop(struct AudioDeviceContext_s *Context, bool terminate);
static int AudioIoctlCommand(struct AudioDeviceContext_s *Context,
			     struct audio_command *command);
/*}}}*/
/*{{{  static data*/
static stm_se_stream_encoding_t AudioContent[AUDIO_ENCODING_PRIVATE] = {
	STM_SE_STREAM_ENCODING_AUDIO_AUTO,
	STM_SE_STREAM_ENCODING_AUDIO_PCM,
	STM_SE_STREAM_ENCODING_AUDIO_LPCM,
	STM_SE_STREAM_ENCODING_AUDIO_MPEG1,
	STM_SE_STREAM_ENCODING_AUDIO_MPEG2,
	STM_SE_STREAM_ENCODING_AUDIO_MP3,
	STM_SE_STREAM_ENCODING_AUDIO_AC3,
	STM_SE_STREAM_ENCODING_AUDIO_DTS,
	STM_SE_STREAM_ENCODING_AUDIO_AAC,
	STM_SE_STREAM_ENCODING_AUDIO_WMA,
	STM_SE_STREAM_ENCODING_AUDIO_NONE,	/* RAW is not supported */
	STM_SE_STREAM_ENCODING_AUDIO_LPCMA,
	STM_SE_STREAM_ENCODING_AUDIO_LPCMH,
	STM_SE_STREAM_ENCODING_AUDIO_LPCMB,
	STM_SE_STREAM_ENCODING_AUDIO_SPDIFIN,
	STM_SE_STREAM_ENCODING_AUDIO_DTS_LBR,
	STM_SE_STREAM_ENCODING_AUDIO_MLP,
	STM_SE_STREAM_ENCODING_AUDIO_RMA,
	STM_SE_STREAM_ENCODING_AUDIO_AVS,
	STM_SE_STREAM_ENCODING_AUDIO_VORBIS,
	STM_SE_STREAM_ENCODING_AUDIO_NONE,	/* FLAC is not implemented */
	STM_SE_STREAM_ENCODING_AUDIO_DRA,
	STM_SE_STREAM_ENCODING_AUDIO_NONE,
	STM_SE_STREAM_ENCODING_AUDIO_MS_ADPCM,
	STM_SE_STREAM_ENCODING_AUDIO_IMA_ADPCM
};

static long DvbAudioUnlockedIoctl(struct file *file, unsigned int foo,
			     unsigned long bar)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 35))
	return dvb_generic_ioctl(NULL, file, foo, bar);
#else
	return dvb_generic_ioctl(file, foo, bar);
#endif
}

static struct file_operations AudioFops = {
owner:	THIS_MODULE,
write:	dvb_audio_write,
unlocked_ioctl:DvbAudioUnlockedIoctl,
open:	AudioOpen,
release:AudioRelease,
poll:	AudioPoll,
};

static struct dvb_device AudioDevice = {
priv:	NULL,
users:	8,
readers:7,
writers:1,
fops:	&AudioFops,
kernel_ioctl:AudioIoctl,
};

#if defined(SDK2_ENABLE_STLINUXTV_STATISTICS)
/*}}}*/
/*{{{  Sysfs attributes */
static ssize_t AudioSysfsShowPlayState(struct device *class_dev,
				       struct device_attribute *attr, char *buf)
{
	struct AudioDeviceContext_s *Context =
	    container_of(class_dev, struct AudioDeviceContext_s, AudioClassDevice);

	switch ((int)Context->AudioPlayState) {
#define C(x) case STM_DVB_AUDIO_ ## x: return sprintf(buf, "%s\n", #x);
		C(STOPPED)
		    C(PLAYING)
		    C(PAUSED)
		    C(INCOMPLETE)
#undef C
	}

	return sprintf(buf, "UNKNOWN\n");
}
static DEVICE_ATTR(play_state, S_IRUGO, AudioSysfsShowPlayState, NULL);

static ssize_t AudioSysfsShowGrabCount(struct device *class_dev,
				       struct device_attribute *attr, char *buf)
{
	struct AudioDeviceContext_s *Context =
	    container_of(class_dev, struct AudioDeviceContext_s, AudioClassDevice);
	return sprintf(buf, "%d\n", Context->stats.grab_buffer_count);
}
static DEVICE_ATTR(grab_buffer_count, S_IRUGO, AudioSysfsShowGrabCount, NULL);
/*}}}*/
/*{{{  VideoInitSysfsAttributes */
int AudioInitSysfsAttributes(struct AudioDeviceContext_s *Context)
{
	int Result;

	Result = device_create_file(&Context->AudioClassDevice,
				    &dev_attr_play_state);
	if (Result)
		goto failed_play_state;

	Result = device_create_file(&Context->AudioClassDevice,
				    &dev_attr_grab_buffer_count);
	if (Result)
		goto failed_grab_count;

	return 0;

failed_grab_count:
	device_remove_file(&Context->AudioClassDevice, &dev_attr_play_state);
failed_play_state:
	DVB_ERROR("device_create_file failed (%d)\n", Result);
	return -1;
}

/*}}}*/

/**
 * AudioRemoveSysfsAttributes
 * This function removes the sysfs entry of Audio device
 */
void AudioRemoveSysfsAttributes(struct AudioDeviceContext_s *Context)
{
	if (!Context)
		return;

	device_remove_file(&Context->AudioClassDevice, &dev_attr_play_state);
	device_remove_file(&Context->AudioClassDevice, &dev_attr_grab_buffer_count);
}
#endif

void AudioResetStats(struct AudioDeviceContext_s *Context)
{
	if (!Context)
		return;

	memset(&Context->stats, 0, sizeof(struct AVStats_s));
}

/**
 * audio_decoder_link_setup()
 * Callback to setup link for audio decoder to mixer and encoder
 */
static int audio_decoder_link_setup(struct media_entity *entity,
				    const struct media_pad *local,
				    const struct media_pad *remote, u32 flags)
{
	int ret = 0;
	struct AudioDeviceContext_s *aud_ctx;

	/*
	 * Audio decoder connection is only valid for mixer and encoder and audio-player
	 */
	if (remote->entity->type != MEDIA_ENT_T_ALSA_SUBDEV_MIXER &&
		remote->entity->type != MEDIA_ENT_T_V4L2_SUBDEV_AUDIO_ENCODER &&
		remote->entity->type != MEDIA_ENT_T_ALSA_SUBDEV_PLAYER ) {
		DVB_ERROR("Invalid remote type for %s\n", entity->name);
		return -EINVAL;
	}

	aud_ctx = stm_media_entity_to_audio_context(entity);

	/*
	 * Lock to prevent any ioctl operation. Audio play/stop is connecting/
	 * disconnecting mixers and encoders from play_stream, thus changing
	 * the state.
	 */
	mutex_lock(&aud_ctx->audops_mutex);

	/*
	 * Check from sink if its ready for a connection.
	 */
	if (flags & MEDIA_LNK_FL_ENABLED) {
		ret = media_entity_call(remote->entity,
				link_setup, remote, local, flags);
		if (ret)
			goto audio_link_setup_done;
	}

	/*
	 * If there's no audio stream, then,
	 * connect   : is deferred
	 * disconnect: is already done, but remove this from Media
	 *             controller database now
	 */
	if (!aud_ctx->AudioStream)
		goto audio_link_setup_done;

	/*
	 * Management of audio decoder and mixer connections.
	 */
	if ((remote->entity->type == MEDIA_ENT_T_ALSA_SUBDEV_MIXER) ||
		(remote->entity->type == MEDIA_ENT_T_ALSA_SUBDEV_PLAYER)) {

		if (flags & MEDIA_LNK_FL_ENABLED)
			ret = stm_dvb_audio_connect_sink
					(&aud_ctx->mc_audio_pad,
					remote, aud_ctx->AudioStream->Handle);
		else
			ret = stm_dvb_audio_disconnect_sink
					(&aud_ctx->mc_audio_pad,
					remote, aud_ctx->AudioStream->Handle);

	} else if (remote->entity->type
				== MEDIA_ENT_T_V4L2_SUBDEV_AUDIO_ENCODER) {

		/*
		 * Management of audio decoder and encoder connections
		 */
		if (flags & MEDIA_LNK_FL_ENABLED)
			ret = stm_dvb_stream_connect_encoder
					(&aud_ctx->mc_audio_pad,
					remote, remote->entity->type,
					aud_ctx->AudioStream->Handle);
		else
			ret = stm_dvb_stream_disconnect_encoder
					(&aud_ctx->mc_audio_pad,
					remote, remote->entity->type,
					aud_ctx->AudioStream->Handle);
	}

audio_link_setup_done:
	mutex_unlock(&aud_ctx->audops_mutex);
	return ret;
}

static const struct media_entity_operations audio_decoder_media_ops = {
	.link_setup = audio_decoder_link_setup,
};

const struct v4l2_subdev_core_ops audio_core_ops = {
	.queryctrl = v4l2_subdev_queryctrl,
	.querymenu = v4l2_subdev_querymenu,
	.g_ctrl = v4l2_subdev_g_ctrl,
	.s_ctrl = v4l2_subdev_s_ctrl,
	.g_ext_ctrls = v4l2_subdev_g_ext_ctrls,
	.try_ext_ctrls = v4l2_subdev_try_ext_ctrls,
	.s_ext_ctrls = v4l2_subdev_s_ext_ctrls,
};


static const struct v4l2_subdev_ops audio_decoder_ops = {
	.core = &audio_core_ops,
};

/*{{{  AudioInit*/
struct dvb_device *AudioInit(struct AudioDeviceContext_s *Context)
{
	int i;

	Context->AudioState.AV_sync_state = 1;
	Context->AudioState.mute_state = 0;
	Context->AudioPlayState = STM_DVB_AUDIO_STOPPED;
	Context->AudioState.stream_source = AUDIO_SOURCE_DEMUX;
	Context->AudioState.channel_select = AUDIO_STEREO;
	Context->AudioState.bypass_mode = false;	/*the sense is inverted so this really means off */

	Context->AudioId = INVALID_DEMUX_ID;	/* CodeToInteger('a','u','d','s'); */
	Context->AudioEncoding = AUDIO_ENCODING_AUTO;
	Context->AudioEncodingFlags = 0;
	Context->AudioStream = NULL;
	Context->AudioOpenWrite = 0;
	Context->AudioSubstreamId = 0;
	Context->AudioApplicationProfile = 0;
	Context->PlaySpeed = DVB_SPEED_NORMAL_PLAY;
	Context->frozen_playspeed = 0;

	Context->AudioPlayInterval.start = DVB_TIME_NOT_BOUNDED;
	Context->AudioPlayInterval.end = DVB_TIME_NOT_BOUNDED;
	Context->AudioApplicationProfile =
	    STM_SE_CTRL_VALUE_AUDIO_APPLICATION_ISO;
	Context->RegionProfile = STM_SE_CTRL_VALUE_REGION_DVB;
	Context->AudioProgramReferenceLevel = 0;
	Context->AudioDownmixToNumberOfChannels.is_set = false;
	Context->AudioDownmixToNumberOfChannels.value = 0;
	Context->AudioStreamDrivenStereo.is_set = false;
	Context->AudioStreamDrivenStereo.value = false;
	Context->AudioStreamDrivenDualMono.is_set = false;
	Context->AudioStreamDrivenDualMono.value = false;

	init_waitqueue_head(&Context->AudioEvents.WaitQueue);
	Context->AudioEvents.Write = 0;
	Context->AudioEvents.Read = 0;
	Context->AudioEvents.Overflow = 0;

	Context->AacDecoderConfig.aac_profile = 0;
	Context->AacDecoderConfig.sbr_enable = 1;
	Context->AacDecoderConfig.sbr_96k_enable = 0;
	Context->AacDecoderConfig.ps_enable = 0;

	for (i = 0; i < DVB_OPTION_MAX; i++)
		Context->PlayOption[i] = DVB_OPTION_VALUE_INVALID;

	Context->playback_context = &Context->DvbContext->PlaybackDeviceContext[Context->DvbContext->audio_dev_off + Context->Id];

	Context->demux_filter = NULL;

	return &AudioDevice;
}

/*}}}*/
/*{{{  AudioInitSubdev */
int AudioInitSubdev(struct AudioDeviceContext_s *Context)
{
	struct v4l2_subdev *sd = &Context->v4l2_audio_sd;
	struct media_pad *pad = &Context->mc_audio_pad;
	struct media_entity *me = &sd->entity;
	int ret;

	ret = AudioInitCtrlHandler(Context);
	if (ret < 0) {
		printk(KERN_ERR "%s: audio ctrl handler init failed (%d)\n",
		       __func__, ret);
		return ret;
	}

	/* Initialize the V4L2 subdev / MC entity */
	v4l2_subdev_init(sd, &audio_decoder_ops);

	/* Entities are called in the LinuxDVB manner */
	snprintf(sd->name, sizeof(sd->name), "dvb%d.audio%d",
		 Context->DvbContext->DvbAdapter->num,
		 Context->Id);

	v4l2_set_subdevdata(sd, Context);

	pad->flags = MEDIA_PAD_FL_SOURCE;

	ret = media_entity_init(me, 1, pad, 0);
	if (ret < 0) {
		printk(KERN_ERR "%s: entity init failed(%d)\n", __func__, ret);
		v4l2_ctrl_handler_free(&Context->ctrl_handler);
		return ret;
	}

	me->ops = &audio_decoder_media_ops;
	me->type = MEDIA_ENT_T_DVB_SUBDEV_AUDIO_DECODER;

	/* copy the ctrl handler to sd*/
	sd->ctrl_handler = &Context->ctrl_handler;

	ret = stm_media_register_v4l2_subdev(sd);
	if (ret < 0) {
		printk(KERN_ERR "%s: stm_media register failed (%d)\n",
		       __func__, ret);
		v4l2_ctrl_handler_free(&Context->ctrl_handler);
		return ret;
	}

	return 0;
}

/*}}}*/

/**
 * AudioReleaseSubdev
 * This functions unregisters the AudioDevice with the entity driver
 * and v4l2 layer
 */
void AudioReleaseSubdev(struct AudioDeviceContext_s *DeviceContext)
{
	struct v4l2_subdev *sd = &DeviceContext->v4l2_audio_sd;
	struct media_entity *me = &sd->entity;

	v4l2_ctrl_handler_free(&DeviceContext->ctrl_handler);

	stm_media_unregister_v4l2_subdev(sd);

	media_entity_cleanup(me);
}

/*{{{  Ioctls*/

/**
 * AudioStop
 * Stops the audio data being pushed to player2.
 */
static int AudioStop(struct AudioDeviceContext_s *aud_ctx, bool terminate)
{
	struct DvbStreamContext_s *stream_ctx = aud_ctx->AudioStream;
	int ret = 0;

	if (!aud_ctx->AudioStream)
		goto audio_stop_done;

	/*
	 * Mark the audio as stopped, so, that any operations being done
	 * on other states be immediately returned.
	 */
	aud_ctx->AudioPlayState = STM_DVB_AUDIO_STOPPED;

	/*
	 * WA: It has been observed that in certain scenarios dvb_audio_write
	 * has taken the lock for injecting data, but is unable to inject as
	 * there are no free buffers with SE. So, free the SE buffers,
	 * anticipating that we have made enough space for injection to
	 * complete, so, that the lock can be released.
	 */
	ret = stm_se_play_stream_drain(stream_ctx->Handle, true);
	if (ret)
		DVB_ERROR("Failed to drain audio stream\n");

	/*
	 * audio-stop ioctl or ctrl-c (audio-release) may stop the audio.
	 * Make the audio-stop ioctl return from here and let audio-
	 * release complete the stuff anyhow.
	 */
	if (mutex_lock_interruptible(&aud_ctx->audwrite_mutex)) {
		if (!terminate) {
			ret = -ERESTARTSYS;
			goto audio_stop_done;
		} else {
			mutex_lock(&aud_ctx->audwrite_mutex);
		}
	}

	/*
	 * WA: The first drain may be competing with write and will return
	 * after it has flushed what is written. The remaining injected
	 * data needs to be flushed and is done now.
	 */
	ret = stm_se_play_stream_drain(stream_ctx->Handle, true);
	if (ret)
		DVB_ERROR("Failed to drain audio stream\n");

	/*DvbStreamEnable (Context->PlayerContext,
	 			 STREAM_CONTENT_AUDIO, false); */

	if (aud_ctx->AudioState.stream_source == AUDIO_SOURCE_DEMUX){
		ret = stm_dvb_detach_stream_from_demux(aud_ctx->playback_context->stm_demux,
							aud_ctx->demux_filter,
							stream_ctx->Handle);
		if (ret)
			printk(KERN_ERR "Failed to detach stream from demux\n");
	} else if (aud_ctx->AudioState.stream_source == AUDIO_SOURCE_MEMORY) {

		if (aud_ctx->AudioStream->memsrc) {
			dvb_playstream_remove_memsrc(aud_ctx->AudioStream->memsrc);
			aud_ctx->AudioStream->memsrc = NULL;
		}
	}

	if (terminate) {
		/*
		 * Remove audio event subscription
		 */
		AudioStreamEventSubscriptionDelete(aud_ctx);

		/*
		 * Detach a encoder
		 */
		ret = stm_dvb_stream_disconnect_encoder
				(&aud_ctx->mc_audio_pad, NULL,
				MEDIA_ENT_T_V4L2_SUBDEV_AUDIO_ENCODER,
				aud_ctx->AudioStream->Handle);
		if (ret)
			DVB_ERROR("Failed to disconnect stream from encoder\n");

		stm_dvb_audio_disconnect_sink(&aud_ctx->mc_audio_pad, NULL,
							aud_ctx->AudioStream->Handle);

		dvb_playback_remove_stream(aud_ctx->playback_context->Playback,
								stream_ctx);
		aud_ctx->AudioStream = NULL;
	}

	mutex_unlock(&aud_ctx->audwrite_mutex);

audio_stop_done:
	return ret;
}

/*{{{  Ioctls*/
/*{{{  AudioIoctlStop*/
int AudioIoctlStop(struct AudioDeviceContext_s *Context)
{
	int Result = 0;

	if (Context->AudioPlayState == STM_DVB_AUDIO_STOPPED)
		/* Nothing to do */
		return 0;

	DVB_DEBUG("(audio%d)\n", Context->Id);

	Result = AudioStop(Context,false);
	if (Result)
		printk(KERN_ERR "Failed to stop the Audio\n");

	DVB_DEBUG("Play state = %d\n", Context->AudioPlayState);

	return Result;
}

/*}}}*/
/*{{{  AudioIoctlPlay*/
int AudioIoctlPlay(struct AudioDeviceContext_s *Context)
{
	int ret = 0;
	sigset_t newsigs, oldsigs;
	unsigned int i;
	struct audio_command command;

	DVB_DEBUG("(audio%d)\n", Context->Id);

	switch(Context->AudioPlayState){
	case STM_DVB_AUDIO_PLAYING:
	case STM_DVB_AUDIO_PAUSED:
		/* Play is used implicitly to exit slow motion and fast forward states so
		   set speed to times 1 if audio is playing or has been paused */
		if (Context->frozen_playspeed)
			Context->frozen_playspeed = 0;
		ret = AudioIoctlSetSpeed(Context, DVB_SPEED_NORMAL_PLAY, 1);
		break;

	case STM_DVB_AUDIO_STOPPED:
	case STM_DVB_AUDIO_INCOMPLETE:
		mutex_lock(&Context->playback_context->Playback_alloc_mutex);
		if (Context->playback_context->Playback == NULL) {
			/*
			   Check to see if we are wired to a demux.  If so the demux should create the playback and we will get
			   another play call.  Just exit in this case.  If we are playing from memory we need to create a playback.
			 */
			if (Context->AudioState.stream_source ==
			    AUDIO_SOURCE_DEMUX){
				Context->AudioPlayState = STM_DVB_AUDIO_INCOMPLETE;
				mutex_unlock(&Context->playback_context->Playback_alloc_mutex);
				return 0;
			}
			ret = DvbPlaybackCreate(Context->playback_context->Id,
						      &Context->playback_context->Playback);
			if (ret < 0){
				mutex_unlock(&Context->playback_context->Playback_alloc_mutex);
				return ret;
			}
		}
		mutex_unlock(&Context->playback_context->Playback_alloc_mutex);
		DvbPlaybackInitOption(Context->playback_context->Playback, Context->PlayOption, Context->PlayValue);

		if ((Context->AudioStream == NULL)
		    || (Context->AudioPlayState == STM_DVB_AUDIO_INCOMPLETE)) {
			unsigned int DemuxId =
			    (Context->AudioState.stream_source ==
			     AUDIO_SOURCE_DEMUX) ? Context->
			    playback_context->Id : INVALID_DEMUX_ID;

			/* a signal received in here can cause issues! Lets turn them off, just for this bit... */
			sigfillset(&newsigs);
			sigprocmask(SIG_BLOCK, &newsigs, &oldsigs);

			ret = dvb_playback_add_stream(Context->playback_context->Playback,
						      STM_SE_MEDIA_AUDIO,
						      AudioContent
						      [Context->AudioEncoding],
						      Context->
						      AudioEncodingFlags,
						      Context->
						      DvbContext->DvbAdapter->
						      num, DemuxId, Context->Id,
						      &Context->AudioStream);

			sigprocmask(SIG_SETMASK, &oldsigs, NULL);

			if (ret != 0) {
				DVB_DEBUG("Failed to create stream context (%d)\n", ret);
				goto end;
			}

			ret = stm_dvb_audio_connect_sink(&Context->mc_audio_pad,
						NULL, Context->AudioStream->Handle);
			if (ret) {
				DVB_DEBUG("Failed to open audio output\n");
				goto end;
			}

			ret = stm_dvb_stream_connect_encoder
					(&Context->mc_audio_pad, NULL,
					MEDIA_ENT_T_V4L2_SUBDEV_AUDIO_ENCODER,
					Context->AudioStream->Handle);
			if(ret) {
				DVB_ERROR("Failed to attach the stream to encoder\n");
				goto end;
			}


		} else {
			if ((Context->AudioEncoding <= AUDIO_ENCODING_AUTO)
			    || (Context->AudioEncoding >= AUDIO_ENCODING_NONE)) {
				DVB_ERROR
				    ("Cannot switch to undefined encoding after play has started\n");
				ret = -EINVAL;
				goto end;
			}

			ret = DvbStreamSwitch(Context->AudioStream,
						 AudioContent
						 [Context->AudioEncoding]);

			if (ret) {
				DVB_ERROR("Failed to switch stream\n");
				goto end;
			}
		}

		/*
		 * Setup for play
		 */

		/*
		 * Create a memsrc if the stream source is memory
		 */
		if (Context->AudioState.stream_source == AUDIO_SOURCE_MEMORY) {

			ret = dvb_playstream_add_memsrc(STM_SE_MEDIA_AUDIO,
						Context->Id, Context->AudioStream->Handle,
						&Context->AudioStream->memsrc);
			if (ret) {
				DVB_ERROR("Failed to create memsrc\n");
				goto end;
			}
		}

		ret = AudioIoctlSetSpeed(Context, Context->PlaySpeed, 0);
		if (ret) {
			DVB_ERROR("Failed to set speed\n");
			goto end;
		}

		ret = AudioIoctlSetPlayInterval(Context, &Context->AudioPlayInterval);
		if (ret ) {
			DVB_ERROR("Failed to set play interval\n");
			goto end;
		}

		ret = AudioSetDemuxId(Context, Context->AudioId);
		if (ret) {
			DVB_ERROR("Failed to set AudioId\n");
			goto end;
		}

		ret = AudioIoctlSetAvSync(Context, Context->AudioState.AV_sync_state);
		if (ret) {
			DVB_ERROR("Failed to set AV Sync\n");
			goto end;
		}

		ret = AudioIoctlChannelSelect(Context, Context->AudioState.channel_select);
		if (ret) {
			DVB_ERROR("Failed to select channel\n");
			goto end;
		}

		if (Context->AudioStreamDrivenDualMono.is_set) {
			ret = AudioIoctlSetStreamDrivenDualMono(Context,
						Context->AudioStreamDrivenDualMono.value);
			if (ret) {
				DVB_ERROR("Failed to set stream driver dual mono\n");
				goto end;
			}
		}

		if (Context->AudioDownmixToNumberOfChannels.is_set) {
			ret = AudioIoctlSetStreamDownmix(Context,
						Context->AudioDownmixToNumberOfChannels.value);
			if (ret) {
				DVB_ERROR("Failed to set number of downmix channel\n");
				goto end;
			}
		}

		if (Context->AudioStreamDrivenStereo.is_set) {
			ret = AudioIoctlSetStreamDrivenStereo(Context,
						Context->AudioStreamDrivenStereo.value);
			if (ret) {
				DVB_ERROR("Failed to set stream driven stereo\n");
				goto end;
			}
		}

		ret = AudioIoctlSetApplicationType(Context, Context->AudioApplicationProfile);
		if (ret) {
			DVB_ERROR("Failed to set application type\n");
			goto end;
		}

		ret = AudioIoctlSetRegionType(Context, Context->RegionProfile);
		if (ret) {
			DVB_ERROR("Failed to set region type\n");
			goto end;
		}

		ret = AudioIoctlSetProgramReferenceLevel(Context, Context->AudioProgramReferenceLevel);
		if (ret) {
			DVB_ERROR("Failed to set program reference level\n");
			goto end;
		}

		ret = AudioIoctlSetServiceType(Context, Context->AudioServiceType);
		if (ret) {
			DVB_ERROR("Failed to set service type\n");
			goto end;
		}

		ret = AudioIoctlSetSubStreamId(Context, Context->AudioSubstreamId);
		if (ret) {
			DVB_ERROR("Failed to set substream id\n");
			goto end;
		}

		if (Context->AudioEncoding == AUDIO_ENCODING_AAC) {
			ret = AudioIoctlSetAacDecoderConfig(Context, &Context->AacDecoderConfig);
			if (ret) {
				DVB_ERROR("Failed to set aac decoder config\n");
				goto end;
			}
		}

		ret = AudioStreamEventSubscriptionCreate(Context);
		if (ret) {
			DVB_ERROR("Failed to create event subscription\n");
			goto end;
		}

		memset(&command, 0, sizeof(struct audio_command));
		command.cmd = AUDIO_CMD_SET_OPTION;
		for (i = 0; i < DVB_OPTION_MAX; i++) {
			if (Context->PlayOption[i] != DVB_OPTION_VALUE_INVALID) {
				command.u.option.option = i;
				command.u.option.value = Context->PlayValue[i];
				Context->PlayOption[i] = DVB_OPTION_VALUE_INVALID;
				AudioIoctlCommand(Context, &command);
			}
		}

		if (Context->AudioState.stream_source == AUDIO_SOURCE_DEMUX) {
			ret = stm_dvb_attach_stream_to_demux(Context->playback_context->stm_demux,
						Context->demux_filter, Context->AudioStream->Handle);
			if (ret)
				printk(KERN_ERR "Failed to attach stream from demux\n");
		}

		break;

	default:
		/* NOT REACHED */
		break;
	}

	if (Context->AudioState.mute_state == false)
		DvbStreamEnable(Context->AudioStream, true);
	else
		DvbStreamEnable(Context->AudioStream, false);

	Context->AudioPlayState = STM_DVB_AUDIO_PLAYING;

end:
	DVB_DEBUG("State = %d\n", Context->AudioPlayState);

	return ret;
}

/*}}}*/

/**
 * AudioIoctlPause() - Pause the audio playback
 */
static int AudioIoctlPause(struct AudioDeviceContext_s *Context)
{
	int ret = 0;

	DVB_DEBUG("(audio%d)\n", Context->Id);

	if (Context->AudioPlayState == STM_DVB_AUDIO_PLAYING) {

		Context->frozen_playspeed = Context->PlaySpeed;

		ret = AudioIoctlSetSpeed(Context, DVB_SPEED_STOPPED, 1);
		if (ret) {
			DVB_ERROR("Failed to freeze the audio playback\n");
			Context->frozen_playspeed = 0;
		}
	}

	return ret;
}

/**
 * AudioIoctlContinue() - continue the audio from last play speed
 */
static int AudioIoctlContinue(struct AudioDeviceContext_s *Context)
{
	int ret = -EINVAL, speed = DVB_SPEED_NORMAL_PLAY;

	DVB_DEBUG("(audio%d)\n", Context->Id);

	if ((Context->AudioPlayState == STM_DVB_AUDIO_PAUSED) ||
		(Context->AudioPlayState == STM_DVB_AUDIO_PLAYING)) {

		/*
		 * Reuse the playspeed if continue is after a pause
		 */
		if (Context->frozen_playspeed) {
			speed = Context->frozen_playspeed;
			Context->frozen_playspeed = 0;
		}

		ret = AudioIoctlSetSpeed(Context, speed, 1);
	}

	return ret;
}

/*{{{  AudioIoctlSelectSource*/
static int AudioIoctlSelectSource(struct AudioDeviceContext_s *Context,
				  audio_stream_source_t Source)
{
	DVB_DEBUG("(audio%d)\n", Context->Id);
	Context->AudioState.stream_source = Source;
	if (Source == AUDIO_SOURCE_DEMUX)
		Context->StreamType = STREAM_TYPE_TRANSPORT;
	else
		Context->StreamType = STREAM_TYPE_PES;
	DVB_DEBUG("Source = %x\n", Context->AudioState.stream_source);

	return 0;
}

/*}}}*/
/*{{{  AudioIoctlSetMute*/
static int AudioIoctlSetMute(struct AudioDeviceContext_s *Context,
			     unsigned int State)
{
	int Result = 0;

	DVB_DEBUG("(audio%d) Mute = %d (was %d)\n", Context->Id, State,
		  Context->AudioState.mute_state);
	if (Context->AudioStream != NULL)
		Result = DvbStreamEnable(Context->AudioStream, !State);

	if (Result == 0)
		Context->AudioState.mute_state = (int)State;

	return Result;
}

/*}}}*/
/*{{{  AudioIoctlSetAvSync*/
static int AudioIoctlSetAvSync(struct AudioDeviceContext_s *Context,
			       unsigned int State)
{
	int Result = 0;

	DVB_DEBUG("(audio%d)\n", Context->Id);
	if (Context->AudioStream != NULL)
		Result =
		    DvbStreamSetOption(Context->AudioStream, DVB_OPTION_AV_SYNC,
				       State ? DVB_OPTION_VALUE_ENABLE :
				       DVB_OPTION_VALUE_DISABLE);

	if (Result != 0)
		return Result;

	Context->AudioState.AV_sync_state = (int)State;
	DVB_DEBUG("AV Sync = %d\n", Context->AudioState.AV_sync_state);

	return Result;
}

/*}}}*/
/*{{{  AudioIoctlSetBypassMode*/
static int AudioIoctlSetBypassMode(struct AudioDeviceContext_s *Context,
				   unsigned int Mode)
{
	DVB_DEBUG("(not implemented)\n");

	return -EPERM;
}

/*}}}*/
/*{{{  AudioIoctlChannelSelect*/
static int AudioIoctlChannelSelect(struct AudioDeviceContext_s *Context,
				   audio_channel_select_t Channel)
{
	int Result = 0;

	DVB_DEBUG("(audio%d) Channel = %x\n", Context->Id, Channel);

	if (Context->AudioStream != NULL)
		Result =
		    DvbStreamChannelSelect(Context->AudioStream,
					   (channel_select_t) Channel);
	if (Result != 0)
		return Result;

	Context->AudioState.channel_select = Channel;

	return Result;
}

/*}}}*/
/*{{{ AudioIoctlSetStreamDrivenDualMono*/
static int AudioIoctlSetStreamDrivenDualMono(struct AudioDeviceContext_s *Context,
					     bool Apply)
{
	int Result = 0;

	DVB_DEBUG("(audio%d) StreamDrivenDualMono = %d \n", Context->Id, Apply);

	if (Context->AudioStream != NULL)
		Result =
		   DvbStreamDrivenDualMono(Context->AudioStream,
					   (unsigned int) Apply);

	Context->AudioStreamDrivenDualMono.is_set = true;
	Context->AudioStreamDrivenDualMono.value = Apply;

	return Result;
}

/*}}}*/
/*{{{ AudioIoctlSetStreamDownmix*/
static int AudioIoctlSetStreamDownmix(struct AudioDeviceContext_s *Context,
                                      unsigned int NumberOfOutputChannels)
{
	int Result = 0;
	stm_se_audio_channel_assignment_t channel_Assignment;

	DVB_DEBUG("(audio%d) StreamDownmix = %d \n", Context->Id, NumberOfOutputChannels);

	switch (NumberOfOutputChannels)
	{
	    case 1:
		DVB_DEBUG("Downmix to 1.0CH preset\n");
		channel_Assignment.pair0 = STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED;
		channel_Assignment.pair1 = STM_SE_AUDIO_CHANNEL_PAIR_CNTR_0;
		channel_Assignment.pair2 = STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED;
		channel_Assignment.pair3 = STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED;
		channel_Assignment.pair4 = STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED;
		channel_Assignment.malleable = 1;
		break;

	    case 2:
		DVB_DEBUG("Downmix to  2.0CH preset\n");
		channel_Assignment.pair0 = STM_SE_AUDIO_CHANNEL_PAIR_L_R;
		channel_Assignment.pair1 = STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED;
		channel_Assignment.pair2 = STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED;
		channel_Assignment.pair3 = STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED;
		channel_Assignment.pair4 = STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED;
		channel_Assignment.malleable = 1;
		break;

	    case 3:
		DVB_DEBUG("Downmix to 2.1CH preset\n");
		channel_Assignment.pair0 = STM_SE_AUDIO_CHANNEL_PAIR_L_R;
		channel_Assignment.pair1 = STM_SE_AUDIO_CHANNEL_PAIR_0_LFE1;
		channel_Assignment.pair2 = STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED;
		channel_Assignment.pair3 = STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED;
		channel_Assignment.pair4 = STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED;
		channel_Assignment.malleable = 1;
		break;

	    case 6:
		DVB_DEBUG("Downmix to 5.1CH preset\n");
		channel_Assignment.pair0 = STM_SE_AUDIO_CHANNEL_PAIR_L_R;
		channel_Assignment.pair1 = STM_SE_AUDIO_CHANNEL_PAIR_CNTR_LFE1;
		channel_Assignment.pair2 = STM_SE_AUDIO_CHANNEL_PAIR_LSUR_RSUR;
		channel_Assignment.pair3 = STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED;
		channel_Assignment.pair4 = STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED;
		channel_Assignment.malleable = 1;
		break;

	    case 8:
		DVB_DEBUG("Downmix to mode 7.1CH preset\n");
		channel_Assignment.pair0 = STM_SE_AUDIO_CHANNEL_PAIR_L_R;
		channel_Assignment.pair1 = STM_SE_AUDIO_CHANNEL_PAIR_CNTR_LFE1;
		channel_Assignment.pair2 = STM_SE_AUDIO_CHANNEL_PAIR_LSUR_RSUR;
		channel_Assignment.pair3 = STM_SE_AUDIO_CHANNEL_PAIR_LSURREAR_RSURREAR;
		channel_Assignment.pair4 = STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED;
		channel_Assignment.malleable = 1;
		break;

	    default:
		DVB_DEBUG("Downmix to invalid number of channels, no Downmix applied\n");
		channel_Assignment.pair0 = STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED;
		channel_Assignment.pair1 = STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED;
		channel_Assignment.pair2 = STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED;
		channel_Assignment.pair3 = STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED;
		channel_Assignment.pair4 = STM_SE_AUDIO_CHANNEL_PAIR_NOT_CONNECTED;
		channel_Assignment.malleable = 0;
		break;
	}

	if (Context->AudioStream != NULL)
		Result =
			DvbStreamDownmix(Context->AudioStream,
			                 (stm_se_audio_channel_assignment_t *) &channel_Assignment);

	Context->AudioDownmixToNumberOfChannels.is_set = true;
	Context->AudioDownmixToNumberOfChannels.value = NumberOfOutputChannels;

	return Result;
}
/*}}}*/
/*{{{ AudioIoctlSetStreamDrivenStereo*/
static int AudioIoctlSetStreamDrivenStereo(struct AudioDeviceContext_s *Context,
						 bool Apply)
{
	int Result = 0;

	DVB_DEBUG("(audio%d) StreamDrivenStereo = %d \n", Context->Id, Apply);

	if (Context->AudioStream != NULL)
		Result =
		   DvbStreamDrivenStereo(Context->AudioStream,
		                         (unsigned int) Apply);

	Context->AudioStreamDrivenStereo.is_set = true;
	Context->AudioStreamDrivenStereo.value = Apply;

	return Result;
}

/*}}}*/
/*{{{  AudioIoctlSetServiceType*/
int AudioIoctlSetServiceType(struct AudioDeviceContext_s *Context,
			     audio_service_t OptionValue)
{
	int Result = 0;

	if (Context->AudioStream != NULL)
		Result =
		    DvbStreamSetAudioServiceType(Context->AudioStream,
						 OptionValue);

	Context->AudioServiceType = OptionValue;

	return Result;

}

/*}}}*/
/*{{{  AudioIoctlSetSUbstreamId*/
int AudioIoctlSetSubStreamId(struct AudioDeviceContext_s *Context,
			     unsigned int OptionValue)
{
	int Result = 0;

	if (Context->AudioStream != NULL)
		Result =
			DvbStreamSetSubStreamId(Context->AudioStream,
						OptionValue);

	Context->AudioSubstreamId = OptionValue;

	return Result;

}

/*}}}*/
/*{{{  AudioIoctlSetApplicationType*/
int AudioIoctlSetApplicationType(struct AudioDeviceContext_s *Context,
				 audio_application_t OptionValue)
{
	int Result = 0;

	if (Context->AudioStream != NULL)
		Result =
		    DvbStreamSetApplicationType(Context->AudioStream,
						OptionValue);

	Context->AudioApplicationProfile = OptionValue;

	return Result;

}

/*}}}*/
/*{{{  AudioIoctlSetProgramReferenceLevel*/
int AudioIoctlSetProgramReferenceLevel(struct AudioDeviceContext_s *Context,
				       unsigned int OptionValue)
{
	int Result = 0;

	if (Context->AudioStream != NULL)
		Result = DvbStreamSetProgramReferenceLevel(Context->AudioStream,
						           OptionValue);

	Context->AudioProgramReferenceLevel = OptionValue;

	return Result;
}

/*}}}*/
/*{{{  AudioIoctlSetRegionType*/
int AudioIoctlSetRegionType(struct AudioDeviceContext_s *Context,
				 audio_region_t OptionValue)
{
	int Result = 0;

	if (Context->AudioStream != NULL)
		Result =
		    DvbStreamSetRegionType(Context->AudioStream,
						OptionValue);

	Context->RegionProfile = OptionValue;

	return Result;

}

/*}}}*/
/*{{{  AudioIoctlGetStatus*/
static int AudioIoctlGetStatus(struct AudioDeviceContext_s *Context,
			       struct audio_status *Status)
{
	DVB_DEBUG("(audio%d)\n", Context->Id);
	memcpy(Status, &Context->AudioState, sizeof(struct audio_status));
	Status->play_state = Context->AudioPlayState;

	return 0;
}

/*}}}*/
/*{{{  AudioIoctlGetCapabilities*/
static int AudioIoctlGetCapabilities(struct AudioDeviceContext_s *Context,
				     int *Capabilities)
{
	stm_se_audio_dec_capability_t se_cap;
	int err;

	memset(&se_cap, 0, sizeof(stm_se_audio_dec_capability_t));

	err = stm_se_get_compound_control(
		STM_SE_CTRL_GET_CAPABILITY_AUDIO_DECODE, (void *) &se_cap);

	if (err != 0) {
		DVB_ERROR("(audio%d) stm_se_get_compound_control returned:%d\n",
				Context->Id, err);
		return err;
	}

	*Capabilities = 0;

	if (se_cap.dts.common.capable)
		*Capabilities |= AUDIO_CAP_DTS;
	if (se_cap.pcm.common.capable)
		*Capabilities |= AUDIO_CAP_LPCM;
	if (se_cap.mp2a.common.capable)
		*Capabilities |= AUDIO_CAP_MP1 | AUDIO_CAP_MP2;
	if (se_cap.mp3.common.capable)
		*Capabilities |= AUDIO_CAP_MP3;
	if (se_cap.aac.common.capable)
		*Capabilities |= AUDIO_CAP_AAC;
	if (se_cap.vorbis.common.capable)
		*Capabilities |= AUDIO_CAP_OGG;
	if (se_cap.ac3.common.capable)
		*Capabilities |= AUDIO_CAP_AC3;

	DVB_DEBUG("(audio%d) Capabilities returned = %x\n", Context->Id,
		  *Capabilities);

	return 0;
}

/*}}}*/
/*{{{  AudioIoctlGetPts*/
static int AudioIoctlGetPts(struct AudioDeviceContext_s *Context,
			    unsigned long long int *Pts)
{
	int Result;
	stm_se_play_stream_info_t PlayInfo;

	/*DVB_DEBUG ("(audio%d)\n", Context->Id); */
	Result = DvbStreamGetPlayInfo(Context->AudioStream, &PlayInfo);
	if (Result != 0)
		return Result;

	*Pts = PlayInfo.pts;

	DVB_DEBUG("(audio%d) %llu\n", Context->Id, *Pts);
	return Result;
}

/*}}}*/
/*{{{  AudioIoctlGetPlayTime*/
static int AudioIoctlGetPlayTime(struct AudioDeviceContext_s *Context,
				 audio_play_time_t * AudioPlayTime)
{
	stm_se_play_stream_info_t PlayInfo;

	/*DVB_DEBUG ("(audio%d)\n", Context->Id); */
	if (Context->AudioStream == NULL)
		return -EINVAL;

	DvbStreamGetPlayInfo(Context->AudioStream, &PlayInfo);

	AudioPlayTime->system_time = PlayInfo.system_time;
	AudioPlayTime->presentation_time = PlayInfo.presentation_time;
	AudioPlayTime->pts = PlayInfo.pts;

	return 0;
}

/*}}}*/
/*{{{  AudioIoctlGetPlayInfo*/
static int AudioIoctlGetPlayInfo(struct AudioDeviceContext_s *Context,
				 audio_play_info_t * AudioPlayInfo)
{
	stm_se_play_stream_info_t PlayInfo;

	/*DVB_DEBUG ("(audio%d)\n", Context->Id); */
	if (Context->AudioStream == NULL)
		return -EINVAL;

	DvbStreamGetPlayInfo(Context->AudioStream, &PlayInfo);

	AudioPlayInfo->system_time = PlayInfo.system_time;
	AudioPlayInfo->presentation_time = PlayInfo.presentation_time;
	AudioPlayInfo->pts = PlayInfo.pts;
	AudioPlayInfo->frame_count = PlayInfo.frame_count;

	return 0;
}

/*}}}*/
/*{{{  AudioIoctlClearBuffer*/
int AudioIoctlClearBuffer(struct AudioDeviceContext_s *Context)
{
	int Result = 0;

	DVB_DEBUG("(audio%d)\n", Context->Id);

	/* Discard previously injected data to free the lock. */
	DvbStreamDrain(Context->AudioStream, true);

	if (mutex_lock_interruptible(&Context->audwrite_mutex) != 0)
		return -ERESTARTSYS;

	DvbStreamDrain(Context->AudioStream, true);

	mutex_unlock(&Context->audwrite_mutex);

	return Result;
}
/*}}}*/
/*{{{  AudioSetDemuxId*/
int AudioSetDemuxId(struct AudioDeviceContext_s *Context, int Id)
{
	DVB_DEBUG("(audio%d) Setting Audio Id to %04x\n", Context->Id, Id);

	Context->AudioId = Id;

	return 0;
}

/*}}}*/
/*{{{  AudioIoctlSetMixer*/
static int AudioIoctlSetMixer(struct AudioDeviceContext_s *Context,
			      audio_mixer_t * Mix)
{
	DVB_DEBUG
	    ("(audio%d) Volume left = %d, Volume right = %d (not yet implemented)\n",
	     Context->Id, Mix->volume_left, Mix->volume_right);
	return -EPERM;
}

/*}}}*/
/*{{{  AudioIoctlSetStreamType*/
static int AudioIoctlSetStreamType(struct AudioDeviceContext_s *Context,
				   unsigned int Type)
{
	DVB_DEBUG("(audio%d) Set type to %x\n", Context->Id, Type);

	if (Type > STREAM_TYPE_RAW)
		return -EINVAL;

	Context->StreamType = (stream_type_t) Type;

	return 0;
}

/*}}}*/
/*{{{  AudioIoctlSetExtId*/
static int AudioIoctlSetExtId(struct AudioDeviceContext_s *Context, int Id)
{
	DVB_ERROR("Not supported\n");
	return -EPERM;
}

/*}}}*/
/*{{{  AudioIoctlSetAttributes*/
static int AudioIoctlSetAttributes(struct AudioDeviceContext_s *Context,
				   audio_attributes_t Attributes)
{
	DVB_ERROR("Not supported\n");
	return -EPERM;
}

/*}}}*/
/*{{{  AudioIoctlSetKaraoke*/
static int AudioIoctlSetKaraoke(struct AudioDeviceContext_s *Context,
				audio_karaoke_t * Karaoke)
{
	DVB_ERROR("Not supported\n");
	return -EPERM;
}

/*}}}*/
/*{{{  AudioIoctlSetEncoding*/
static int AudioIoctlSetEncoding(struct AudioDeviceContext_s *Context,
				 unsigned int Encoding)
{
	DVB_DEBUG("(Audio%d) Set encoding to %d\n", Context->Id, Encoding);

#define ENCODING_MASK  0xFFFF0000
        /*
         * Split Encoding type and ehavioural flags!
         * See also VideoIoctlSetEncoding.  The mechanism is used
         * to pass behavioural flags to play stream. In the audio
         * is not yet used, however since Audio and Video join in
         * the same functions of backend.c it has been bnecessary
         * to omogenize the two
         */
	Context->AudioEncodingFlags = (Encoding & ENCODING_MASK);
	Encoding &= ~ENCODING_MASK;

	if (Encoding >= AUDIO_ENCODING_PRIVATE)
		return -EINVAL;


	/* Allow the stream switch to go ahead
	   if (Context->AudioEncoding == (audio_encoding_t)Encoding)
	   return 0;
	 */

	Context->AudioEncoding = (audio_encoding_t) Encoding;

	switch (Context->AudioPlayState) {
	case STM_DVB_AUDIO_STOPPED:
		return 0;
	case STM_DVB_AUDIO_INCOMPLETE:
		/* At this point we have received the missing piece of information which will allow the
		 * stream to be fully populated so we can reissue the play. */
		return AudioIoctlPlay(Context);
	default:
		{
			int Result = 0;

			if ((Context->AudioEncoding == AUDIO_ENCODING_AUTO)) {
				DVB_ERROR
				    ("Cannot switch to undefined encoding after play has started\n");
				return -EINVAL;
			}

			Result = DvbStreamSwitch(Context->AudioStream,
						 AudioContent
						 [Context->AudioEncoding]);

			/*
			   if (Result == 0)
			   Result  = AudioIoctlSetId            (Context, Context->AudioId);
			 */
			return Result;
		}
	}
}

/*}}}*/
/*{{{  AudioIoctlFlush*/
static int AudioIoctlFlush(struct AudioDeviceContext_s *Context)
{
	int Result = 0;

	DVB_DEBUG("(audio%d)\n", Context->Id);
	/* If the stream is frozen it cannot be drained so an error is returned. */
	if ((Context->PlaySpeed == 0)
	    || (Context->PlaySpeed == DVB_SPEED_REVERSE_STOPPED))
		return -EPERM;

	if (mutex_lock_interruptible(&Context->audwrite_mutex) != 0)
		return -ERESTARTSYS;
	mutex_unlock(&Context->audops_mutex);	/* release lock so non-writing ioctls still work while draining */

	Result = DvbStreamDrain(Context->AudioStream, false);

	mutex_unlock(&Context->audwrite_mutex);	/* release write lock so actions which have context lock can complete */
	mutex_lock(&Context->audops_mutex);	/* reclaim lock so can be released by outer function */

	return Result;
}

/*}}}*/
/*{{{  AudioIoctlSetSpdifSource*/
static int AudioIoctlSetSpdifSource(struct AudioDeviceContext_s *Context,
				    unsigned int Mode)
{
	int Result;
	int BypassCodedData = (Mode == AUDIO_SPDIF_SOURCE_ES);

	DVB_DEBUG("(audio%d)\n", Context->Id);
	if ((Mode != AUDIO_SPDIF_SOURCE_PP) && (Mode != AUDIO_SPDIF_SOURCE_ES))
		return -EINVAL;

	DVB_DEBUG("Bypass = %s\n",
		  BypassCodedData ? "Enabled (ES)" :
		  "Disabled (Post-proc LPCM)");

	if (Context->AudioState.bypass_mode == BypassCodedData)
		return 0;

	Result =
	    DvbStreamSetOption(Context->AudioStream,
			       DVB_OPTION_AUDIO_SPDIF_SOURCE,
			       (unsigned int)BypassCodedData);
	if (Result == 0)
		Context->AudioState.bypass_mode = Mode;

	return Result;
}

/*}}}*/
/*{{{  AudioIoctlSetSpeed*/
int AudioIoctlSetSpeed(struct AudioDeviceContext_s *Context, int Speed,
			      int PlaySpeedUpdate)
{
	int DirectionChange;
	int Result;

	DVB_DEBUG("(audio%d)\n", Context->Id);
	if (Context->playback_context->Playback == NULL) {
		Context->PlaySpeed = Speed;
		return 0;
	}

	/* If changing direction we require a write lock */
	DirectionChange = ((Speed * Context->PlaySpeed) < 0);
	if (DirectionChange) {
		/* Discard previously injected data to free the lock. */
		DvbStreamDrain(Context->AudioStream, true);
		if (mutex_lock_interruptible(&Context->audwrite_mutex) !=
		    0)
			return -ERESTARTSYS;	/* Give up for now. */
	}

	Result = DvbPlaybackSetSpeed(Context->playback_context->Playback, Speed);
	if (Result >= 0)
		Result =
		    DvbPlaybackGetSpeed(Context->playback_context->Playback, &Context->PlaySpeed);

	/* If changing direction release write lock */
	if (DirectionChange)
		mutex_unlock(&Context->audwrite_mutex);

	/* Fill in the AudioPlayState variable */
	if (PlaySpeedUpdate
	    && ((Context->AudioPlayState == STM_DVB_AUDIO_PAUSED) |
		(Context->AudioPlayState == STM_DVB_AUDIO_PLAYING))) {
		if (Context->PlaySpeed == DVB_SPEED_STOPPED)
			Context->AudioPlayState = STM_DVB_AUDIO_PAUSED;
		else
			Context->AudioPlayState = STM_DVB_AUDIO_PLAYING;
	}

	DVB_DEBUG("Speed set to %d\n", Context->PlaySpeed);

	return Result;
}

/*}}}*/
/*{{{  AudioIoctlDiscontinuity*/
static int AudioIoctlDiscontinuity(struct AudioDeviceContext_s *Context,
				   audio_discontinuity_t Discontinuity)
{
	int Result = 0;

	DVB_DEBUG("(audio%d) %d\n", Context->Id, Discontinuity);

	/*
	   If the stream is frozen a discontinuity cannot be injected.
	 */
	if ((Context->AudioPlayState == STM_DVB_AUDIO_PAUSED)
	    || (Context->PlaySpeed == 0)
	    || (Context->PlaySpeed == DVB_SPEED_REVERSE_STOPPED))
		return -EINVAL;

	if (mutex_lock_interruptible(&Context->audwrite_mutex) != 0)
		return -ERESTARTSYS;

	Result =
	    DvbStreamDiscontinuity(Context->AudioStream,
				   (dvb_discontinuity_t) Discontinuity);
	mutex_unlock(&Context->audwrite_mutex);

	return Result;
}

/*}}}*/
/*{{{  AudioIoctlSetPlayInterval*/
int AudioIoctlSetPlayInterval(struct AudioDeviceContext_s *Context,
			      audio_play_interval_t * PlayInterval)
{
	DVB_DEBUG("(audio%d)\n", Context->Id);

	memcpy(&Context->AudioPlayInterval, PlayInterval,
	       sizeof(audio_play_interval_t));

	if (Context->AudioStream == NULL)
		return 0;

	return DvbStreamSetPlayInterval(Context->AudioStream,
					(dvb_play_interval_t *) PlayInterval);
}

/*}}}*/

/*{{{  AudioIoctlSetSyncGroup*/
static int AudioIoctlSetSyncGroup(struct AudioDeviceContext_s *Context,
				  unsigned int Group)
{
	int Result = 0;
	unsigned int sync_id;
	struct PlaybackDeviceContext_s * new_context = NULL;

	DVB_DEBUG("(audio%d)\n", Context->Id);

	if (Context->AudioPlayState != STM_DVB_AUDIO_STOPPED){
		DVB_ERROR("Audio device not stopped - cannot set\n");
		return -EPERM;
	}

	sync_id = Group & ~AUDIO_SYNC_GROUP_MASK;

	switch(Group & AUDIO_SYNC_GROUP_MASK){
	case AUDIO_SYNC_GROUP_DEMUX:
		if (sync_id > Context->DvbContext->demux_dev_nb){
			DVB_ERROR("Sync group value is out of range\n");
			return -EINVAL;
		}

		new_context = &Context->DvbContext->PlaybackDeviceContext[sync_id];
		break;

	case AUDIO_SYNC_GROUP_AUDIO:
		if (sync_id > Context->DvbContext->audio_dev_nb){
			DVB_ERROR("Sync group value is out of range\n");
			return -EINVAL;
		}

		new_context = Context->DvbContext->AudioDeviceContext[sync_id].playback_context;
		break;

	case AUDIO_SYNC_GROUP_VIDEO:
		if (sync_id > Context->DvbContext->video_dev_nb){
			DVB_ERROR("Sync group value is out of range\n");
			return -EINVAL;
		}

		new_context = Context->DvbContext->VideoDeviceContext[sync_id].playback_context;
		break;

	default:
		DVB_ERROR("Invalid sync group value\n");
		return -EINVAL;
	}

	if (Context->playback_context == new_context)
		return 0;

	mutex_lock(&Context->playback_context->Playback_alloc_mutex);
	if (Context->playback_context->Playback){
		if (DvbPlaybackDelete(Context->playback_context->Playback) == 0) {
			DVB_DEBUG("Playback deleted successfully\n");
			Context->playback_context->Playback = NULL;
		}
	}
	mutex_unlock(&Context->playback_context->Playback_alloc_mutex);

	Context->playback_context = new_context;

	return Result;
}
/*}}}*/

/*{{{  VideoIoctlCommand*/
static int AudioIoctlCommand(struct AudioDeviceContext_s *Context,
			     struct audio_command *command)
{
	int Result = 0;
	int ApplyLater = 0;

	DVB_DEBUG("(audio%d)\n", Context->Id);
	if (command->cmd == AUDIO_CMD_SET_OPTION) {
		if (command->u.option.option >= DVB_OPTION_MAX) {
			DVB_ERROR("Option %d, out of range\n",
					command->u.option.option);
			return -EINVAL;
		}
		/*
		 * Determine if the command should be applied to the playback or just the audio stream.  Check if
		 * the stream or playback is valid.  If so apply the option to the stream.  If not check to see if
		 * there is a valid playback.  If so, apply the option to the playback if appropriate.  If not, save
		 * command for later.
		 */

		DVB_DEBUG("Option %d, Value 0x%x\n",
				command->u.option.option,
				command->u.option.value);

		if ((command->u.option.option ==
					DVB_OPTION_DISCARD_LATE_FRAMES)
				|| (command->u.option.option ==
					DVB_OPTION_PTS_FORWARD_JUMP_DETECTION_THRESHOLD)
				|| (command->u.option.option == DVB_OPTION_LIVE_PLAY)
				|| (command->u.option.option == DVB_OPTION_PACKET_INJECTOR)
				|| (command->u.option.option ==
					DVB_OPTION_CTRL_AUDIO_DEEMPHASIS)
				|| (command->u.option.option ==
					DVB_OPTION_CTRL_PLAYBACK_LATENCY)
				|| (command->u.option.option ==
					DVB_OPTION_MASTER_CLOCK)) {
			if (Context->playback_context->Playback != NULL)
				Result =
					DvbPlaybackSetOption(Context->playback_context->Playback,
							command->
							u.option.option,
							(unsigned int)
							command->
							u.option.value);
			else
				ApplyLater = 1;
		} else {
			if (Context->AudioStream != NULL)
				Result =
					DvbStreamSetOption(Context->AudioStream,
							command->
							u.option.option,
							(unsigned int)
							command->
							u.option.value);
			else
				ApplyLater = 1;
		}

		if (ApplyLater) {
			Context->PlayOption[command->u.option.option] = command->u.option.option;	/* save for later */
			Context->PlayValue[command->u.option.option] =
				command->u.option.value;
			if (Context->playback_context->Playback != NULL)	/* apply to playback if appropriate */
				DvbPlaybackInitOption(Context->playback_context->Playback, Context->PlayOption, Context->PlayValue);
		}

	}

	return Result;
}

/*{{{  AudioIoctlSetClockDataPoint*/
int AudioIoctlSetClockDataPoint(struct AudioDeviceContext_s *Context,
				audio_clock_data_point_t * ClockData)
{
	DVB_DEBUG("(audio%d)\n", Context->Id);

	if (Context->playback_context->Playback == NULL)
		return -ENODEV;

	return DvbPlaybackSetClockDataPoint(Context->playback_context->Playback,
					    (dvb_clock_data_point_t *)
					    ClockData);
}

/*}}}*/
/*{{{  AudioIoctlSetTimeMapping*/
int AudioIoctlSetTimeMapping(struct AudioDeviceContext_s *Context,
			     audio_time_mapping_t * TimeMapping)
{
	int Result;

	DVB_DEBUG("(audio%d)\n", Context->Id);
	if ((Context->playback_context->Playback == NULL) || (Context->AudioStream == NULL))
		return -ENODEV;

	Result =
	    DvbStreamSetOption(Context->AudioStream,
			       DVB_OPTION_EXTERNAL_TIME_MAPPING,
			       DVB_OPTION_VALUE_ENABLE);
	if (Result) {
		printk(KERN_ERR "%s: failed to set stream option (%d)\n",
		       __func__, Result);
		return Result;
	}

	return DvbPlaybackSetNativePlaybackTime(Context->playback_context->Playback,
						TimeMapping->native_stream_time,
						TimeMapping->system_presentation_time);
}

/*}}}*/
/*{{{  AudioIoctlSetAacDecoderConfig*/
int AudioIoctlSetAacDecoderConfig(struct AudioDeviceContext_s *Context,
				  audio_mpeg4aac_t * AacDecoderConfig)
{
	int Result = 0;
	if (Context->AudioStream != NULL)
	{
		stm_se_mpeg4aac_t Mpeg4aacConfig;
		Mpeg4aacConfig.aac_profile = AacDecoderConfig->aac_profile;
		Mpeg4aacConfig.sbr_enable = !(!AacDecoderConfig->sbr_enable);
		Mpeg4aacConfig.sbr_96k_enable = !(!AacDecoderConfig->sbr_96k_enable);
		Mpeg4aacConfig.ps_enable = !(!AacDecoderConfig->ps_enable);

		Result =
		    DvbStreamSetAacDecoderConfig(Context->AudioStream,
						 &Mpeg4aacConfig);
	}

	Context->AacDecoderConfig.aac_profile = AacDecoderConfig->aac_profile;
	Context->AacDecoderConfig.sbr_enable = AacDecoderConfig->sbr_enable;
	Context->AacDecoderConfig.sbr_96k_enable = AacDecoderConfig->sbr_96k_enable;
	Context->AacDecoderConfig.ps_enable = AacDecoderConfig->ps_enable;

	return Result;
}

 /*}}} */
/*{{{  AudioIoctlGetClockDataPoint*/
int AudioIoctlGetClockDataPoint(struct AudioDeviceContext_s *Context,
				audio_clock_data_point_t * ClockData)
{
	DVB_DEBUG("(audio%d)\n", Context->Id);

	if (Context->playback_context->Playback == NULL)
		return -ENODEV;

	return DvbPlaybackGetClockDataPoint(Context->playback_context->Playback,
					    (dvb_clock_data_point_t *)
					    ClockData);
}

/*}}}*/

/**
 * AudioOpen
 * This is the open callback from the dvb_device. Take the instance
 * and get prepared for starting audio.
 */
static int AudioOpen(struct inode *Inode, struct file *File)
{
	struct dvb_device *DvbDevice = (struct dvb_device *)File->private_data;
	struct AudioDeviceContext_s *Context =
	    (struct AudioDeviceContext_s *)DvbDevice->priv;
	int Error;

	mutex_lock(&Context->auddev_mutex);

	DVB_DEBUG("(audio%d)\n", Context->Id);
	Error = dvb_generic_open(Inode, File);
	if (Error < 0) {
		mutex_unlock(&Context->auddev_mutex);
		return Error;
	}

	if ((File->f_flags & O_ACCMODE) != O_RDONLY) {
		Context->AudioPlayState = STM_DVB_AUDIO_STOPPED;
		Context->AudioEvents.Write = 0;
		Context->AudioEvents.Read  = 0;
		Context->AudioEvents.Overflow = 0;
		Context->AudioOpenWrite = 1;
		AudioResetStats(Context);
	}

	mutex_unlock(&Context->auddev_mutex);

	return 0;
}

/**
 * AudioRelease
 * This is audio device release call. Stop the playback
 * of audio and release the current instance.
 */
static int AudioRelease(struct inode *Inode, struct file *File)
{
	struct dvb_device *DvbDevice = (struct dvb_device *)File->private_data;
	struct AudioDeviceContext_s *Context =
	    (struct AudioDeviceContext_s *)DvbDevice->priv;
	struct stm_dvb_demux_s *stm_demux;
	int Result;

	DVB_DEBUG("(audio%d)\n", Context->Id);

	/*
	 * In case of tunneled decode, we would like to modify the
	 * demuxer playback context, in case, we succeed in deleting
	 * it. This allows demux to avoid using a bad playback
	 * associated with the playback context. Since, the locking
	 * is always: demux, audio/video, so we maintain here as well
	 */
	stm_demux = (struct stm_dvb_demux_s *)Context->playback_context->stm_demux;
	if (stm_demux && (Context->AudioState.stream_source == AUDIO_SOURCE_DEMUX))
		down_write(&stm_demux->rw_sem);

	mutex_lock(&Context->auddev_mutex);


	if ((File->f_flags & O_ACCMODE) != O_RDONLY) {

		mutex_lock(&Context->audops_mutex);

		Result = AudioStop(Context,true);

		mutex_unlock(&Context->audops_mutex);

		if (Result)
			printk(KERN_ERR "Failed to stop the Audio\n");

		mutex_lock(&Context->playback_context->Playback_alloc_mutex);
		/* Try to delete the playback - it will fail if someone is still using it */
		if (Context->playback_context->Playback != NULL){
			if (DvbPlaybackDelete(Context->playback_context->Playback) == 0) {
				DVB_DEBUG("Playback deleted successfully\n");
				Context->playback_context->Playback = NULL;
			}
		}
		mutex_unlock(&Context->playback_context->Playback_alloc_mutex);

		/*
		 * If we succeeded in deleting the playback, then, we reset the demux
		 * context too
		 */
		if (!Context->playback_context->Playback) {
			if (stm_demux && (Context->AudioState.stream_source == AUDIO_SOURCE_DEMUX))
				stm_demux->PlaybackContext = NULL;
		}

		Context->StreamType = STREAM_TYPE_TRANSPORT;
		Context->PlaySpeed = DVB_SPEED_NORMAL_PLAY;
		Context->AudioPlayInterval.start = DVB_TIME_NOT_BOUNDED;
		Context->AudioPlayInterval.end = DVB_TIME_NOT_BOUNDED;

		AudioInit(Context);

	}

	Result = dvb_generic_release(Inode, File);

	mutex_unlock(&Context->auddev_mutex);

	if (stm_demux && (Context->AudioState.stream_source == AUDIO_SOURCE_DEMUX))
		up_write(&stm_demux->rw_sem);

	return Result;
}

/**
 * AudioIoctlGetEvent() - fills the user event info
 * @Context   : Audio decoder context
 * @AudioEvent: Location to store event info
 * @Fileflags : file->f_flags
 * Fill in the event info from the last read index when this function was
 * called or from the read index which got pushed to another location if
 * there's overflow. This means, the event may not be the latest which has
 * occured. So, it is recommended that poll() be intiated immediately, in
 * fact, just after decoder open so, that the event is dequeued as soon as
 * it occurs.
 */
static int AudioIoctlGetEvent(struct AudioDeviceContext_s *Context,
			      struct audio_event *AudioEvent, int FileFlags)
{
	int ret = 0;
	struct AudioEvent_s *evt_lst = &Context->AudioEvents;

	if ((FileFlags & O_NONBLOCK) != O_NONBLOCK) {
		/*
		 * Relinquish the ioctl lock, so, that other ioctl operations
		 * can be done till we wait for events.
		 */
		mutex_unlock(&Context->audops_mutex);

		/*
		 * There may be inconsitency in reading Read/Write values,
		 * but, there is no reason to worry as the producer (event
		 * handler) will always keep them unequal. So, it is important
		 * here to break this wait. We take the list lock later, then
		 * the access is serialized guarrating the correct values.
		 */
		ret = wait_event_interruptible(evt_lst->WaitQueue,
					(evt_lst->Write != evt_lst->Read));

		/*
		 * Reacquire the lock
		 */
		mutex_lock(&Context->audops_mutex);

		if (ret) {
			ret = -ERESTARTSYS;
			goto get_event_failed;
		}
	}

	mutex_lock(&evt_lst->Lock);

	/*
	 * The internal event list overflowed.
	 */
	if (evt_lst->Overflow) {
		evt_lst->Overflow = 0;
		ret = -EOVERFLOW;
		goto get_event_done;
	}

	/*
	 * This can only happen for non-blocking mode
	 */
	if (evt_lst->Write == evt_lst->Read) {
		ret = -EWOULDBLOCK;
		goto get_event_done;
	}

	/*
	 * Copy the event information and pass onto user
	 */
	memcpy(AudioEvent, &evt_lst->Event[evt_lst->Read],
					sizeof(struct audio_event));

	evt_lst->Read = (evt_lst->Read + 1) % MAX_AUDIO_EVENT;

get_event_done:
	mutex_unlock(&evt_lst->Lock);
get_event_failed:
	return ret;
}

/*{{{  AudioIoctl*/
static int AudioIoctl(
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 35))
			     struct inode *Inode,
#endif
			     struct file *File,
			     unsigned int IoctlCode, void *Parameter)
{
	struct dvb_device *DvbDevice = (struct dvb_device *)File->private_data;
	struct AudioDeviceContext_s *Context =
	    (struct AudioDeviceContext_s *)DvbDevice->priv;
	int Result = 0;

	/*DVB_DEBUG("AudioIoctl : Ioctl %08x\n", IoctlCode); */

	if (((File->f_flags & O_ACCMODE) == O_RDONLY)
	    && (IoctlCode != AUDIO_GET_STATUS))
		return -EPERM;

	if (!Context->AudioOpenWrite)	/* Check to see that somebody has the device open for write */
		return -EBADF;

	mutex_lock(&Context->audops_mutex);

	switch (IoctlCode) {
	case AUDIO_STOP:
		Result = AudioIoctlStop(Context);
		break;
	case AUDIO_PLAY:
		Result = AudioIoctlPlay(Context);
		break;
	case AUDIO_PAUSE:
		Result = AudioIoctlPause(Context);
		break;
	case AUDIO_CONTINUE:
		Result = AudioIoctlContinue(Context);
		break;
	case AUDIO_SELECT_SOURCE:
		Result =
		    AudioIoctlSelectSource(Context,
					   (audio_stream_source_t) Parameter);
		break;
	case AUDIO_SET_MUTE:
		Result = AudioIoctlSetMute(Context, (unsigned int)Parameter);
		break;
	case AUDIO_SET_AV_SYNC:
		Result = AudioIoctlSetAvSync(Context, (unsigned int)Parameter);
		break;
	case AUDIO_SET_BYPASS_MODE:
		Result =
		    AudioIoctlSetBypassMode(Context, (unsigned int)Parameter);
		break;
	case AUDIO_CHANNEL_SELECT:
		Result =
		    AudioIoctlChannelSelect(Context,
					    (audio_channel_select_t) Parameter);
		break;
	case AUDIO_STREAMDRIVEN_DUALMONO:
		Result =
		    AudioIoctlSetStreamDrivenDualMono(Context,
						      (unsigned int) Parameter);
		break;
	case AUDIO_STREAM_DOWNMIX:
	    Result =
		    AudioIoctlSetStreamDownmix(Context,
					       (unsigned int) Parameter);
	    break;
	case AUDIO_STREAMDRIVEN_STEREO:
	    Result =
		    AudioIoctlSetStreamDrivenStereo(Context,
						    (bool) Parameter);
		break;
	case AUDIO_GET_STATUS:
		Result =
		    AudioIoctlGetStatus(Context,
					(struct audio_status *)Parameter);
		break;
	case AUDIO_GET_EVENT:
		Result =
		    AudioIoctlGetEvent(Context, (struct audio_event *)Parameter,
				       File->f_flags);
		break;
	case AUDIO_GET_CAPABILITIES:
		Result = AudioIoctlGetCapabilities(Context, (int *)Parameter);
		break;
	case AUDIO_GET_PTS:
		Result =
		    AudioIoctlGetPts(Context,
				     (unsigned long long int *)Parameter);
		break;
	case AUDIO_CLEAR_BUFFER:
		Result = AudioIoctlClearBuffer(Context);
		break;
	case AUDIO_SET_ID:
		Result = AudioIoctlSetSubStreamId(Context,(int) Parameter);
		break;
	case AUDIO_SET_MIXER:
		Result =
		    AudioIoctlSetMixer(Context, (audio_mixer_t *) Parameter);
		break;
	case AUDIO_SET_STREAMTYPE:
		Result =
		    AudioIoctlSetStreamType(Context, (unsigned int)Parameter);
		break;
	case AUDIO_SET_EXT_ID:
		Result = AudioIoctlSetExtId(Context, (int)Parameter);
		break;
	case AUDIO_SET_ATTRIBUTES:
		Result =
		    AudioIoctlSetAttributes(Context, (audio_attributes_t) ((int)
									   Parameter));
		break;
	case AUDIO_SET_KARAOKE:
		Result =
		    AudioIoctlSetKaraoke(Context,
					 (audio_karaoke_t *) Parameter);
		break;
	case AUDIO_SET_ENCODING:
		Result =
		    AudioIoctlSetEncoding(Context, (unsigned int)Parameter);
		break;
	case AUDIO_FLUSH:
		Result = AudioIoctlFlush(Context);
		break;
	case AUDIO_SET_SPDIF_SOURCE:
		Result =
		    AudioIoctlSetSpdifSource(Context, (unsigned int)Parameter);
		break;
	case AUDIO_SET_SPEED:
		Result = AudioIoctlSetSpeed(Context, (int)Parameter, 1);
		break;
	case AUDIO_DISCONTINUITY:
		Result =
		    AudioIoctlDiscontinuity(Context,
					    (audio_discontinuity_t) Parameter);
		break;
	case AUDIO_SET_PLAY_INTERVAL:
		Result =
		    AudioIoctlSetPlayInterval(Context, (audio_play_interval_t *)
					      Parameter);
		break;
	case AUDIO_SET_SYNC_GROUP:
		Result =
		    AudioIoctlSetSyncGroup(Context, (unsigned int)Parameter);
		break;
	case AUDIO_COMMAND:
		Result =
			AudioIoctlCommand(Context,
				(struct audio_command *)Parameter);
		break;
	case AUDIO_GET_PLAY_TIME:
		Result =
		    AudioIoctlGetPlayTime(Context,
					  (audio_play_time_t *) Parameter);
		break;
	case AUDIO_GET_PLAY_INFO:
		Result =
		    AudioIoctlGetPlayInfo(Context,
					  (audio_play_info_t *) Parameter);
		break;
	case AUDIO_SET_CLOCK_DATA_POINT:
		Result =
		    AudioIoctlSetClockDataPoint(Context,
						(audio_clock_data_point_t *)
						Parameter);
		break;
	case AUDIO_SET_TIME_MAPPING:
		Result =
		    AudioIoctlSetTimeMapping(Context, (audio_time_mapping_t *)
					     Parameter);
		break;
	case AUDIO_GET_CLOCK_DATA_POINT:
		Result =
		    AudioIoctlGetClockDataPoint(Context,
						(audio_clock_data_point_t *)
						Parameter);
		break;
	case AUDIO_SET_SERVICE_TYPE:
		Result = AudioIoctlSetServiceType(Context,
					(audio_service_t)Parameter);
		break;
	case AUDIO_SET_APPLICATION_TYPE:
		Result = AudioIoctlSetApplicationType(Context,
					(audio_application_t)Parameter);
		break;
	case AUDIO_SET_AAC_DECODER_CONFIG:
		Result =
		    AudioIoctlSetAacDecoderConfig(Context, (audio_mpeg4aac_t *)
						  Parameter);
		break;
	case AUDIO_SET_REGION_TYPE:
		Result = AudioIoctlSetRegionType(Context,
						(audio_region_t)Parameter);
		break;
	case AUDIO_SET_PROGRAM_REFERENCE_LEVEL:
		Result = AudioIoctlSetProgramReferenceLevel(Context, (int)Parameter);
		break;
	default:
		DVB_ERROR("Invalid ioctl %08x\n", IoctlCode);
		Result = -ENOIOCTLCMD;
	}
	mutex_unlock(&Context->audops_mutex);

	return Result;
}

/*}}}*/

/**
 * dvb_audio_write
 * This is the /dev/audioX write implementation to inject the data to player2
 */
static ssize_t dvb_audio_write(struct file *file,
			const char __user *buffer, size_t count, loff_t * ppos)
{
	struct dvb_device *dvbdev = file->private_data;
	struct AudioDeviceContext_s *aud_ctx = dvbdev->priv;
	struct DvbStreamContext_s *stream_ctx = aud_ctx->AudioStream;
	char *usr_data = (char *)buffer;
	int ret = 0;

	if (!count)
		return 0;

	mutex_lock(&aud_ctx->audwrite_mutex);

	/*
	 * Audio stream source has to be memory to be able to inject data
	 */
	if (aud_ctx->AudioState.stream_source == AUDIO_SOURCE_DEMUX) {
		DVB_ERROR("Audio stream source must be AUDIO_SOURCE_MEMORY\n");
		ret = -EPERM;
		goto write_invalid_state;
	}

	if (!stream_ctx) {
		DVB_ERROR("No audio stream exists to be written\n");
		ret = -EINVAL;
		goto write_invalid_state;
	}

	if (aud_ctx->AudioPlayState == STM_DVB_AUDIO_STOPPED) {
		DVB_ERROR("Audio not playing, start the audio first\n");
		ret = -EINVAL;
		goto write_invalid_state;
	}

#define PACK_HEADER_START_CODE	((usr_data[0] == 0x00) && \
				 (usr_data[1] == 0x00) && \
				 (usr_data[2] == 0x01) && \
				 (usr_data[3] == 0xba))
#define PACK_HEADER_LENGTH	14
/*
 * Extra 6 includes bytes up to length bytes
 */
#define PES_LENGTH		((usr_data[4] << 8) + \
				 usr_data[5] + 6)

/*
 *  TODO Skip Pack header until handled by audio collator
 */
	if (PACK_HEADER_START_CODE) {
		usr_data = &usr_data[PACK_HEADER_LENGTH];
		count = PES_LENGTH;
	}

	if (aud_ctx->AudioPlayState == STM_DVB_AUDIO_INCOMPLETE) {
		if (aud_ctx->AudioEncoding == AUDIO_ENCODING_AUTO) {
			DvbStreamGetFirstBuffer(stream_ctx, buffer,
						count);
			DvbStreamIdentifyAudio(stream_ctx,
					       &aud_ctx->AudioEncoding);
		} else {
			DVB_ERROR("Audio incomplete with encoding set (%d)\n",
				  aud_ctx->AudioEncoding);
		}

		AudioIoctlPlay(aud_ctx);

		if (aud_ctx->AudioPlayState == STM_DVB_AUDIO_INCOMPLETE) {
			aud_ctx->AudioPlayState = STM_DVB_AUDIO_STOPPED;
			goto write_invalid_state;
		}
	}

	ret = dvb_stream_inject(stream_ctx, usr_data, count);

	mutex_unlock(&aud_ctx->audwrite_mutex);

	return ret;

write_invalid_state:
	mutex_unlock(&aud_ctx->audwrite_mutex);
	return ret;
}

/*}}}*/
/*{{{  AudioPoll*/
static unsigned int AudioPoll(struct file *File, poll_table * Wait)
{
	struct dvb_device *DvbDevice = (struct dvb_device *)File->private_data;
	struct AudioDeviceContext_s *Context =
	    (struct AudioDeviceContext_s *)DvbDevice->priv;
	unsigned int Mask = 0;

	if (((File->f_flags & O_ACCMODE) == O_RDONLY)
	    || (Context->AudioStream == NULL))
		return 0;

	poll_wait (File, &Context->AudioEvents.WaitQueue, Wait);

	if (Context->AudioEvents.Write != Context->AudioEvents.Read)
		Mask    |= POLLPRI;

	if ((Context->AudioPlayState == STM_DVB_AUDIO_PLAYING)
	    || (Context->AudioPlayState == STM_DVB_AUDIO_INCOMPLETE)) {
		Mask |= (POLLOUT | POLLWRNORM);
	}
	return Mask;
}

/*}}}*/
/*{{{  AudioSE2STLinuxTVCoding*/
static audio_encoding_t AudioSE2STLinuxTVCoding(stm_se_stream_encoding_t  audio_coding_type)
{
	audio_encoding_t coding = AUDIO_ENCODING_NONE;

	switch (audio_coding_type) {
	case STM_SE_STREAM_ENCODING_AUDIO_AUTO:
		coding = AUDIO_ENCODING_AUTO;
		break;
	case STM_SE_STREAM_ENCODING_AUDIO_PCM:
		coding = AUDIO_ENCODING_PCM;
		break;
	case STM_SE_STREAM_ENCODING_AUDIO_LPCM:
		coding = AUDIO_ENCODING_LPCM;
		break;
	case STM_SE_STREAM_ENCODING_AUDIO_MPEG1:
		coding = AUDIO_ENCODING_MPEG1;
		break;
	case STM_SE_STREAM_ENCODING_AUDIO_MPEG2:
		coding = AUDIO_ENCODING_MPEG2;
		break;
	case STM_SE_STREAM_ENCODING_AUDIO_MP3:
		coding = AUDIO_ENCODING_MP3;
		break;
	case STM_SE_STREAM_ENCODING_AUDIO_AC3:
		coding = AUDIO_ENCODING_AC3;
		break;
	case STM_SE_STREAM_ENCODING_AUDIO_DTS:
		coding = AUDIO_ENCODING_DTS;
		break;
	case STM_SE_STREAM_ENCODING_AUDIO_AAC:
		coding = AUDIO_ENCODING_AAC;
		break;
	case STM_SE_STREAM_ENCODING_AUDIO_WMA:
		coding = AUDIO_ENCODING_WMA;
		break;
	case STM_SE_STREAM_ENCODING_AUDIO_LPCMA:
		coding = AUDIO_ENCODING_LPCMA;
		break;
	case STM_SE_STREAM_ENCODING_AUDIO_LPCMH:
		coding = AUDIO_ENCODING_LPCMH;
		break;
	case STM_SE_STREAM_ENCODING_AUDIO_LPCMB:
		coding = AUDIO_ENCODING_LPCMB;
		break;
	case STM_SE_STREAM_ENCODING_AUDIO_SPDIFIN:
		coding = AUDIO_ENCODING_SPDIF;
		break;
	case STM_SE_STREAM_ENCODING_AUDIO_DTS_LBR:
		coding = AUDIO_ENCODING_DTS_LBR;
		break;
	case STM_SE_STREAM_ENCODING_AUDIO_MLP:
		coding = AUDIO_ENCODING_MLP;
		break;
	case STM_SE_STREAM_ENCODING_AUDIO_RMA:
		coding = AUDIO_ENCODING_RMA;
		break;
	case STM_SE_STREAM_ENCODING_AUDIO_AVS:
		coding = AUDIO_ENCODING_AVS;
		break;
	case STM_SE_STREAM_ENCODING_AUDIO_VORBIS:
		coding = AUDIO_ENCODING_VORBIS;
		break;
	case STM_SE_STREAM_ENCODING_AUDIO_DRA:
		coding = AUDIO_ENCODING_DRA;
		break;
	case STM_SE_STREAM_ENCODING_AUDIO_MS_ADPCM:
		coding = AUDIO_ENCODING_MS_ADPCM;
		break;
	case STM_SE_STREAM_ENCODING_AUDIO_IMA_ADPCM:
		coding = AUDIO_ENCODING_IMA_ADPCM;
		break;
	default:
		break;
	}

	return coding;
}

/*}}}*/
/*{{{  AudioSetMessageEvent */
void AudioSetMessageEvent( struct AudioDeviceContext_s *Context )
{
	struct AudioEvent_s *ev_list;
	unsigned int Next;
	struct audio_event event;
	bool event_to_notify = false;
	struct stm_se_play_stream_msg_s StreamMsg;
	int ret = 0;

	ev_list = &Context->AudioEvents;

	/* we can receive more than one message,
	 * so need to loop to purge message queue*/
	while (1) {
		event_to_notify = false;
		memset(&event, 0, sizeof(struct audio_event));

		ret = DvbStreamGetMessage(Context->AudioStream,
					  Context->AudioMsgSubscription,
					  &StreamMsg);
		if (ret) {
			if (ret != -EAGAIN)
				DVB_ERROR("Stream get message failed (%d)\n",
					  ret);
			break;
		}

		switch(StreamMsg.msg_id) {
		case STM_SE_PLAY_STREAM_MSG_AUDIO_PARAMETERS_CHANGED:
			event.type = AUDIO_EVENT_PARAMETERS_CHANGED;
			event.u.audio_parameters.bitrate = StreamMsg.u.audio_parameters.bitrate;
			event.u.audio_parameters.sampling_freq = StreamMsg.u.audio_parameters.sampling_freq;
			event.u.audio_parameters.num_channels = StreamMsg.u.audio_parameters.num_channels;
			event.u.audio_parameters.dual_mono = StreamMsg.u.audio_parameters.dual_mono;
			event.u.audio_parameters.coding = AudioSE2STLinuxTVCoding(StreamMsg.u.audio_parameters.audio_coding_type);
			event.u.audio_parameters.channel_assignment.pair0 =
				StreamMsg.u.audio_parameters.channel_assignment.pair0;
			event.u.audio_parameters.channel_assignment.pair1 =
				StreamMsg.u.audio_parameters.channel_assignment.pair1;
			event.u.audio_parameters.channel_assignment.pair2 =
				StreamMsg.u.audio_parameters.channel_assignment.pair2;
			event.u.audio_parameters.channel_assignment.pair3 =
				StreamMsg.u.audio_parameters.channel_assignment.pair3;
			event.u.audio_parameters.channel_assignment.pair4 =
				StreamMsg.u.audio_parameters.channel_assignment.pair4;
			event.u.audio_parameters.channel_assignment.reserved0 =
				StreamMsg.u.audio_parameters.channel_assignment.reserved0;
			event.u.audio_parameters.channel_assignment.malleable =
				StreamMsg.u.audio_parameters.channel_assignment.malleable;
			event_to_notify = true;
			break;

		case STM_SE_PLAY_STREAM_MSG_STREAM_UNPLAYABLE:
			event.type = AUDIO_EVENT_STREAM_UNPLAYABLE;
			event.u.value = (unsigned int)(StreamMsg.u.reason);
			event_to_notify = true;
			break;

		case STM_SE_PLAY_STREAM_MSG_TRICK_MODE_CHANGE:
			event.type = AUDIO_EVENT_TRICK_MODE_CHANGE;
			event.u.value = StreamMsg.u.trick_mode_domain;
			event_to_notify = true;
			break;

		case STM_SE_PLAY_STREAM_MSG_MESSAGE_LOST:
			event.type = AUDIO_EVENT_LOST;
			event.u.value = StreamMsg.u.lost_count;
			event_to_notify = true;
			break;

		default:
			DVB_DEBUG("Unknown SE message received (%d)\n",
				  StreamMsg.msg_id);
			break;
		}

		if (!event_to_notify)
			continue;

		mutex_lock(&ev_list->Lock);

		Next = (ev_list->Write + 1) % MAX_AUDIO_EVENT;
		if (Next == ev_list->Read) {
			ev_list->Overflow = true;
			ev_list->Read = (ev_list->Read + 1) % MAX_AUDIO_EVENT;
		}

		memcpy(&(ev_list->Event[ev_list->Write]),&event,sizeof(event));

		ev_list->Write = Next;

		mutex_unlock(&ev_list->Lock);

		wake_up_interruptible(&ev_list->WaitQueue);
	}
}
/*}}}*/

/*{{{  AudioSetEvent*/
void AudioSetEvent(unsigned int number_of_events, stm_event_info_t * eventsInfo)
{
	struct AudioEvent_s *ev_list;
	unsigned int Next;
	struct audio_event event;
	bool event_to_notify;
	struct AudioDeviceContext_s *Context;

	/* retrieve device context from event subscription handler cookie */
	Context = eventsInfo->cookie;

	ev_list = &Context->AudioEvents;

	while(number_of_events){
		event_to_notify = true;
		memset(&event, 0, sizeof(struct audio_event));

		switch(eventsInfo->event.event_id) {
		case STM_SE_PLAY_STREAM_EVENT_MESSAGE_READY:
			AudioSetMessageEvent(Context);
			/* Nothing to notify here since already done within
			 * AudioSetMessageEvent function */
			event_to_notify = false;
			break;

		case STM_SE_PLAY_STREAM_EVENT_END_OF_STREAM:
			event.type = AUDIO_EVENT_END_OF_STREAM;
			break;

		case STM_SE_PLAY_STREAM_EVENT_FRAME_STARVATION:
			event.type = AUDIO_EVENT_FRAME_STARVATION;
			break;

		case STM_SE_PLAY_STREAM_EVENT_FRAME_SUPPLIED:
			event.type = AUDIO_EVENT_FRAME_SUPPLIED;
			break;

		case STM_SE_PLAY_STREAM_EVENT_FRAME_DECODED_LATE:
			event.type = AUDIO_EVENT_FRAME_DECODED_LATE;
			break;

		case STM_SE_PLAY_STREAM_EVENT_DATA_DELIVERED_LATE:
			event.type = AUDIO_EVENT_DATA_DELIVERED_LATE;
			break;

		case STM_SE_PLAY_STREAM_EVENT_FATAL_ERROR:
			event.type = AUDIO_EVENT_FATAL_ERROR;
			break;

		case STM_SE_PLAY_STREAM_EVENT_FATAL_HARDWARE_FAILURE:
			event.type = AUDIO_EVENT_FATAL_HARDWARE_FAILURE;
			break;

		case STM_SE_PLAY_STREAM_EVENT_FRAME_DECODED:
			event.type = AUDIO_EVENT_FRAME_DECODED;
			break;

		case STM_SE_PLAY_STREAM_EVENT_FRAME_RENDERED:
			event.type = AUDIO_EVENT_FRAME_RENDERED;
			break;

		default:
			DVB_DEBUG("Unknown SE event received (%d)\n",
				  eventsInfo->event.event_id);
			event_to_notify = false;
		}

		if (!event_to_notify){
			number_of_events--;
			eventsInfo++;
			continue;
		}

		mutex_lock(&ev_list->Lock);

		Next = (ev_list->Write + 1) % MAX_AUDIO_EVENT;
		if (Next == ev_list->Read) {
			ev_list->Overflow = true;
			ev_list->Read = (ev_list->Read + 1) % MAX_AUDIO_EVENT;
		}

		memcpy(&(ev_list->Event[ev_list->Write]),&event,sizeof(event));

		ev_list->Write = Next;

		mutex_unlock(&ev_list->Lock);

		wake_up_interruptible(&ev_list->WaitQueue);

		number_of_events--;
		eventsInfo++;
	}
}

/*}}}*/
/*{{{  AudioStreamEventSubscriptionCreate*/
static int AudioStreamEventSubscriptionCreate(struct AudioDeviceContext_s *Context)
{
	uint32_t message_mask;
	uint32_t event_mask;
	int Result = 0;

	/* mask of expected Audio stream event */
	event_mask = STM_SE_PLAY_STREAM_EVENT_MESSAGE_READY |
			STM_SE_PLAY_STREAM_EVENT_END_OF_STREAM |
			STM_SE_PLAY_STREAM_EVENT_FRAME_STARVATION |
			STM_SE_PLAY_STREAM_EVENT_FRAME_SUPPLIED |
			STM_SE_PLAY_STREAM_EVENT_FRAME_DECODED_LATE |
			STM_SE_PLAY_STREAM_EVENT_DATA_DELIVERED_LATE |
			STM_SE_PLAY_STREAM_EVENT_FATAL_ERROR |
			STM_SE_PLAY_STREAM_EVENT_FATAL_HARDWARE_FAILURE |
			STM_SE_PLAY_STREAM_EVENT_FRAME_DECODED |
			STM_SE_PLAY_STREAM_EVENT_FRAME_RENDERED;

	if ((Context == NULL) || (Context->AudioStream == NULL)
			|| (Context->AudioStream->Handle == NULL))
		return -EINVAL;

	/* If events are already subscribed, don't do anything */
	if (Context->AudioMsgSubscription && Context->AudioEventSubscription)
		return 0;

	/* create audio stream event subscription and handler
	   (only one event handle at a time)    */
	Result =
		DvbEventSubscribe(Context->AudioStream, Context, event_mask,
				AudioSetEvent, &Context->AudioEventSubscription);
	if (Result < 0) {
		Context->AudioEventSubscription = NULL;
		BACKEND_ERROR
			("Failed to set Audio stream Event handler: Context %x \n",
			 (uint32_t) Context);
		return -EINVAL;
	}

	/* mask of expected audio stream message */
	message_mask = STM_SE_PLAY_STREAM_MSG_AUDIO_PARAMETERS_CHANGED;

	/* create audio stream message subscription */
	Result =
		DvbStreamMessageSubscribe(Context->AudioStream, message_mask, 10,
				&Context->AudioMsgSubscription);
	if (Result < 0) {
		Context->AudioMsgSubscription = NULL;
		BACKEND_ERROR
			("Failed to create Audio stream message subscription: Context %x \n",
			 (uint32_t) Context);
		return -EINVAL;
	}

	return Result;
}

/*}}}*/
/*{{{  AudioStreamEventSubscriptionDelete*/
static int AudioStreamEventSubscriptionDelete(struct AudioDeviceContext_s *Context)
{
	int Result = 0;

	if ((Context == NULL) || (Context->AudioStream == NULL)
			|| (Context->AudioStream->Handle == NULL))
		return -EINVAL;

	/* delete audio stream event subscription */
	if (Context->AudioEventSubscription) {
		Result =
			DvbEventUnsubscribe(Context->AudioStream,
					Context->AudioEventSubscription);
		Context->AudioEventSubscription = NULL;
		if (Result < 0) {
			BACKEND_ERROR
				("Failed to reset Audio stream Event handler: Context %x \n",
				 (uint32_t) Context);
			return -EINVAL;
		}
	}

	/* delete Audio stream message subscription */
	if (Context->AudioMsgSubscription) {
		Result =
			DvbStreamMessageUnsubscribe(Context->AudioStream,
					Context->AudioMsgSubscription);
		Context->AudioMsgSubscription = NULL;
		if (Result < 0) {
			BACKEND_ERROR
				("Failed to create audio stream message subscription: Context %x \n",
				 (uint32_t) Context);
			return -EINVAL;
		}
	}

	return Result;
}

/*}}}*/
