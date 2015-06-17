/************************************************************************
Copyright (C) 2003 STMicroelectronics. All Rights Reserved.

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

Source file name : dvb_module.h - streamer device definitions
Author :           Julian

Date        Modification                                    Name
----        ------------                                    --------
24-Mar-03   Created                                         Julian

************************************************************************/

#ifndef H_DVB_MODULE
#define H_DVB_MODULE

#include <linux/dvb/audio.h>
#include <linux/dvb/video.h>
#include "linux/dvb/stm_ioctls.h"
#include <linux/device.h>
#include <linux/ratelimit.h>
#include <include/stm_display.h>
#include <media/v4l2-ctrls.h>

#include <stm_memsrc.h>

#include "stmedia.h"

#include "dvbdev.h"
#include "dmxdev.h"
#include "dvb_demux.h"

#include "backend.h"
#include "dvp.h"

#ifndef false
#define false   0
#define true    1
#endif

/* Enable/disable debug-level macros.
 *
 * This really should be off by default (for checked in versions of the player) if you
 * *really* need your message to hit the console every time then that is what DVB_TRACE()
 * is for.
 */
#ifndef ENABLE_DVB_DEBUG
#define ENABLE_DVB_DEBUG                0
#endif

#define DVB_DEBUG(fmt, args...)         ((void) (ENABLE_DVB_DEBUG && \
                                                 (printk(KERN_INFO "%s: " fmt, __FUNCTION__,##args), 0)))

/* Output trace information off the critical path */
#define DVB_TRACE(fmt, args...)         (printk(KERN_NOTICE "%s: " fmt, __FUNCTION__, ##args))
/* Output errors, should never be output in 'normal' operation */
#define DVB_ERROR(fmt, args...)         printk_ratelimited(KERN_CRIT "ERROR in %s: " fmt, __FUNCTION__, ##args)

#define DVB_ASSERT(x) do if(!(x)) printk(KERN_CRIT "%s: Assertion '%s' failed at %s:%d\n", \
                                         __FUNCTION__, #x, __FILE__, __LINE__); while(0)

typedef enum {
	STM_DVB_AUDIO_STOPPED,	/* Device is stopped */
	STM_DVB_AUDIO_PLAYING,	/* Device is currently playing */
	STM_DVB_AUDIO_PAUSED,	/* Device is paused */
	STM_DVB_AUDIO_INCOMPLETE	/* Can not yet start */
} stm_dvb_audio_play_state_t;

typedef enum {
	STM_DVB_VIDEO_STOPPED,	/* Device is stopped */
	STM_DVB_VIDEO_PLAYING,	/* Device is currently playing */
	STM_DVB_VIDEO_FREEZED,	/* Device is paused */
	STM_DVB_VIDEO_INCOMPLETE	/* Can not yet start */
} stm_dvb_video_play_state_t;

struct PlaybackDeviceContext_s {
	unsigned int Id;

	/* Used only in case of S/W demux */
	struct dvb_demux DvbDemux;
	struct dmxdev DmxDevice;
	struct dmx_frontend MemoryFrontend;
	struct DvbStreamContext_s *DemuxStream;
	unsigned char *dvr_in;
	unsigned char *dvr_out;

	void *stm_demux;
	struct mutex DemuxWriteLock;
	struct mutex Playback_alloc_mutex;/* Will be used when the Playback will be allocated */

	struct DvbPlaybackContext_s *Playback;
};

#define MAX_AUDIO_EVENT         8

struct AudioEvent_s {
	struct audio_event Event[MAX_AUDIO_EVENT];      /*event structure for Audio*/
	unsigned int Write;     /*! Pointer to next event location to write by decoder */
	unsigned int Read;      /*! Pointer to next event location to read by user */
	unsigned int Overflow;  /*! Flag to indicate events have been missed */
	wait_queue_head_t WaitQueue;    /*! Queue to wake up any waiters */
	struct mutex Lock;      /*! Protection for access to Read and Write pointers */
};


struct AVStats_s {
	unsigned int grab_buffer_count;
};

struct AudioDownmixToNumberOfChannels_s {
	bool is_set;
	unsigned int value;
};

struct AudioStreamDrivenStereo_s {
	bool is_set;
	bool value;
};

struct AudioStreamDrivenDualMono_s {
	bool is_set;
	bool value;
};

struct AudioDeviceContext_s {
	unsigned int Id;

	struct v4l2_subdev v4l2_audio_sd;
	struct v4l2_ctrl_handler ctrl_handler;
	struct media_pad mc_audio_pad;
	struct dvb_device *AudioDevice;
	struct device AudioClassDevice;
	struct audio_status AudioState;
	stm_dvb_audio_play_state_t AudioPlayState;	/* This replace AudioState.play_state, needed since we extend the state machine */
	unsigned int AudioId;
	audio_encoding_t AudioEncoding;
	unsigned int	 AudioEncodingFlags;	/* to ctrl encode behavior */
	struct DvbStreamContext_s *AudioStream;
	unsigned int AudioOpenWrite;
	struct AudioEvent_s AudioEvents;

	/*
	 * @auddev_mutex:   is used to sync open/release of audio device
	 * @audops_mutex:   is used to sync ioctl operations on audio device.
	 * @audwrite_mutex: is used to sync write operation to audio device and
	 *                  ioctl control which may affect its behaviour. For
	 *                  example, if playback is stopped, let the current
	 *                  write operation to complete and then issue stop.
	 */
	struct mutex auddev_mutex;
	struct mutex audops_mutex;
	struct mutex audwrite_mutex;

	unsigned int AudioCaptureStatus;
	audio_service_t AudioServiceType;
	audio_application_t AudioApplicationProfile;
	audio_region_t RegionProfile;
	unsigned int AudioProgramReferenceLevel;
	unsigned int AudioSubstreamId;
	audio_mpeg4aac_t AacDecoderConfig;
	audio_play_interval_t AudioPlayInterval;
	stm_event_subscription_h AudioEventSubscription;
	stm_se_play_stream_subscription_h AudioMsgSubscription;
	struct AudioDownmixToNumberOfChannels_s AudioDownmixToNumberOfChannels;
	struct AudioStreamDrivenStereo_s AudioStreamDrivenStereo;
	struct AudioStreamDrivenDualMono_s AudioStreamDrivenDualMono;
	unsigned int PlayOption[DVB_OPTION_MAX];
	unsigned int PlayValue[DVB_OPTION_MAX];

	struct AVStats_s stats;

	stream_type_t StreamType;
	struct DvbContext_s *DvbContext;

	/*
	 * @frozen_playspeed: playspeed before freezing
	 * @PlaySpeed       : Current playspeed
	 */
	int frozen_playspeed;
	int PlaySpeed;

	stm_object_h demux_filter;

	struct PlaybackDeviceContext_s *playback_context;
};

/*
 * stm_media_entity_to_audio_context
 * @me: media entity pointer
 * This macro returns the audio decoder context for the given media entity
 */
#define stm_media_entity_to_audio_context(me)			\
		container_of(media_entity_to_v4l2_subdev(me),	\
		struct AudioDeviceContext_s, v4l2_audio_sd)

#define MAX_VIDEO_EVENT         32
struct VideoEvent_s {
	struct video_event Event[MAX_VIDEO_EVENT];	/*! Linux dvb event structure */
	unsigned int Write;	/*! Pointer to next event location to write by decoder */
	unsigned int Read;	/*! Pointer to next event location to read by user */
	unsigned int Overflow;	/*! Flag to indicate events have been missed */
	wait_queue_head_t WaitQueue;	/*! Queue to wake up any waiters */
	struct mutex Lock;	/*! Protection for access to Read and Write pointers */
};

#define DVB_OPTION_VALUE_INVALID                0xffffffff
struct VideoDeviceContext_s {
	unsigned int Id;

	struct v4l2_subdev v4l2_video_sd;
	struct v4l2_ctrl_handler ctrl_handler;
	struct media_pad mc_video_pad;
	stm_display_source_h video_source;
	struct dvb_device *VideoDevice;
	struct device VideoClassDevice;
	struct video_status VideoState;
	stm_dvb_video_play_state_t VideoPlayState;	/* This replace VideoState.play_state, needed since we extend the state machine */
	unsigned int VideoId;
	video_encoding_t VideoEncoding;
	unsigned int	 VideoEncodingFlags;	/* to ctrl encode behavior */
	video_size_t VideoSize;
	unsigned int FrameRate;
	struct DvbStreamContext_s *VideoStream;
	unsigned int VideoOpenWrite;
	struct VideoEvent_s VideoEvents;
	/*
	 * @viddev_mutex:   is used to sync open/release of video device
	 * @vidops_mutex:   is used to sync ioctl operations on video device.
	 * @vidwrite_mutex: is used to sync write operation to video device and
	 *                  ioctl control which may affect its behaviour. For
	 *                  example, if playback is stopped, let the current
	 *                  write operation to complete and then issue stop.
	 */
	struct mutex viddev_mutex;
	struct mutex vidops_mutex;
	struct mutex vidwrite_mutex;

	unsigned int VideoCaptureStatus;
	video_play_interval_t VideoPlayInterval;

	struct AVStats_s stats;

	stm_event_subscription_h VideoEventSubscription;
	stm_se_play_stream_subscription_h VideoMsgSubscription;

	unsigned int PlayOption[DVB_OPTION_MAX];
	unsigned int PlayValue[DVB_OPTION_MAX];
	struct Ratio_s PixelAspectRatio;
	stream_type_t StreamType;
	struct DvbContext_s *DvbContext;

	stm_object_h demux_filter;

	/*
	 * @frozen_playspeed: playspeed before freezing
	 * @PlaySpeed       : Current playspeed
	 */
	int frozen_playspeed;
	int PlaySpeed;

	struct PlaybackDeviceContext_s *playback_context;
};

/*
 * stm_media_entity_to_video_context
 * @me: media entity pointer
 * This macro returns the video decoder context for the given media entity
 */
#define stm_media_entity_to_video_context(me)			\
		container_of(media_entity_to_v4l2_subdev(me),	\
		struct VideoDeviceContext_s, v4l2_video_sd)

struct DvbContext_s {
	struct dvb_device DvbDevice;
	struct dvb_adapter *DvbAdapter;

	struct mutex Lock;

	struct AudioDeviceContext_s *AudioDeviceContext;
	struct VideoDeviceContext_s *VideoDeviceContext;
	struct PlaybackDeviceContext_s *PlaybackDeviceContext;

	unsigned int audio_dev_off;
	unsigned int audio_dev_nb;
	unsigned int video_dev_off;
	unsigned int video_dev_nb;

	unsigned int demux_dev_nb;
};

#endif
