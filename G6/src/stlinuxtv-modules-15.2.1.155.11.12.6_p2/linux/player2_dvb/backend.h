/************************************************************************
Copyright (C) 2011 STMicroelectronics. All Rights Reserved.

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

Source file name : backend.h - player access points
Author :           Julian

Date        Modification                                    Name
----        ------------                                    --------
31-Jan-07   Created                                         Julian

************************************************************************/

#ifndef H_BACKEND
#define H_BACKEND

#include <linux/kthread.h>
#include <linux/dvb/audio.h>
#include <linux/dvb/video.h>
#include "linux/dvb/stm_ioctls.h"
#include <asm/errno.h>

#include "stm_event.h"
#include "stm_se.h"
#include "dvb_module.h"

#define MAX_STREAMS_PER_PLAYBACK        8
#define MAX_PLAYBACKS                   4

#define TRANSPORT_PACKET_SIZE           188
#define BLUERAY_PACKET_SIZE             (TRANSPORT_PACKET_SIZE+sizeof(unsigned int))

#define AUDIO_STREAM_BUFFER_SIZE        (200*TRANSPORT_PACKET_SIZE)
#define VIDEO_STREAM_BUFFER_SIZE        (200*TRANSPORT_PACKET_SIZE)
#define DEMUX_BUFFER_SIZE               (512*TRANSPORT_PACKET_SIZE)

#define INVALID_DEMUX_ID                0xFFFFFFFF
/*      Debug printing macros   */

#ifndef ENABLE_BACKEND_DEBUG
#define ENABLE_BACKEND_DEBUG            1
#endif

#define BACKEND_DEBUG(fmt, args...)  ((void) (ENABLE_BACKEND_DEBUG && \
                                            (printk(KERN_DEBUG "LinuxDvb:%s: " fmt, __FUNCTION__, ##args), 0)))

/* Output trace information off the critical path */
#define BACKEND_TRACE(fmt, args...)  (printk(KERN_DEBUG "LinuxDvb:%s: " fmt, __FUNCTION__, ##args))
/* Output errors, should never be output in 'normal' operation */
#define BACKEND_ERROR(fmt, args...)  (printk(KERN_ERR "ERROR:LinuxDvb:%s: " fmt, __FUNCTION__, ##args))

#define CodeToInteger(a,b,c,d)          ((a << 0) | (b << 8) | (c << 16) | (d << 24))

/* The stream context provides details on a single audio or video stream */
struct DvbStreamContext_s {
	struct mutex Lock;

	stm_se_play_stream_h Handle;	/* Opaque handle to access backend actions */
	stm_se_media_t Media;	/* Media type. Needed to know which inject/delete function to call */
	stm_object_h display_Sink;	/* Display sink for the stream */
	stm_object_h grab_Sink;	/* Grab sink for the stream */
	stm_object_h user_data_Sink;	/* User data sink for the stream */
	stm_memsrc_h memsrc;

	unsigned int BufferLength;
	unsigned char *Buffer;
};

/*
   The playback context keeps track of the bundle of streams playing synchronously
   Every stream or demux added to the bundle ups the usage count. Every one removed reduces it.
   The playback can be deleted when the count returns to zero
*/
struct DvbPlaybackContext_s {
	struct mutex Lock;
	stm_se_playback_h Handle;
	unsigned int UsageCount;
};

/* Entry point list */

/*
 * Playback management functions
 */
int DvbPlaybackCreate(int Id, struct DvbPlaybackContext_s **DvbPlaybackContext);
int DvbPlaybackDelete(struct DvbPlaybackContext_s *DvbPlayback);

/*
 * Stream management functions
 * The below functions are used to operate on the play_stream
 */
/*
 * Create/remove play_stream
 */
int dvb_playback_add_stream(struct DvbPlaybackContext_s *playback,
		stm_se_media_t media, stm_se_stream_encoding_t encoding,
		unsigned int encoding_flags,
		unsigned int adapter_id, unsigned int demux_id,
		unsigned int ctx_id, struct DvbStreamContext_s **stream_ctx);
int dvb_playback_remove_stream(struct DvbPlaybackContext_s *DvbPlayback,
					struct DvbStreamContext_s *DvbStream);

/*
 * Connect play_stream to encoder
 */
int stm_dvb_stream_connect_encoder(struct media_pad *src_pad,
					const struct media_pad *dst_pad,
					unsigned long entity_type,
					stm_se_play_stream_h play_stream);
int stm_dvb_stream_disconnect_encoder(struct media_pad *src_pad,
					const struct media_pad *dst_pad,
					unsigned long entity_type,
					stm_se_play_stream_h play_stream);

/*
 * Connect play_stream to memsrc
 */
int dvb_playstream_add_memsrc(stm_se_media_t media, int ctx_id,
			stm_se_play_stream_h play_stream, stm_memsrc_h *memsrc);
void dvb_playstream_remove_memsrc(stm_memsrc_h memsrc);

int DvbPlaybackSetSpeed(struct DvbPlaybackContext_s *DvbPlayback,
			unsigned int Speed);
int DvbPlaybackGetSpeed(struct DvbPlaybackContext_s *DvbPlayback,
			unsigned int *Speed);
int DvbPlaybackSetNativePlaybackTime(struct DvbPlaybackContext_s *DvbPlayback,
				     unsigned long long NativeTime,
				     unsigned long long SystemTime);
int DvbPlaybackSetOption(struct DvbPlaybackContext_s *DvbPlayback,
			 dvb_option_t DvbOption, unsigned int Value);
int DvbPlaybackSetClockDataPoint(struct DvbPlaybackContext_s *DvbPlayback,
				 dvb_clock_data_point_t * ClockData);
int DvbPlaybackGetClockDataPoint(struct DvbPlaybackContext_s *DvbPlayback,
				 dvb_clock_data_point_t * ClockData);

int DvbStreamEnable(struct DvbStreamContext_s *DvbStream, unsigned int Enable);
int DvbStreamChannelSelect(struct DvbStreamContext_s *DvbStream,
			   stm_se_dual_mode_t Channel);
int DvbStreamDrivenDualMono(struct DvbStreamContext_s *DvbStream,
			    unsigned int Apply);
int DvbStreamDownmix(struct DvbStreamContext_s *DvbStream,
                     stm_se_audio_channel_assignment_t *channelAssignment);
int DvbStreamDrivenStereo(struct DvbStreamContext_s *DvbStream,
			    unsigned int Apply);
int DvbStreamSetAudioServiceType(struct DvbStreamContext_s *DvbStream,
				 unsigned int Type);
int DvbStreamSetApplicationType(struct DvbStreamContext_s *DvbStream,
				unsigned int Type);
int DvbStreamSetSubStreamId(struct DvbStreamContext_s *DvbStream,
			    unsigned int Id);
int DvbStreamSetAacDecoderConfig(struct DvbStreamContext_s *DvbStream,
				 void *Value);
int DvbStreamSetOption(struct DvbStreamContext_s *DvbStream,
		       dvb_option_t DvbOption, unsigned int Value);
int DvbStreamGetOption(struct DvbStreamContext_s *DvbStream,
		       dvb_option_t DvbOption, unsigned int *Value);
int DvbStreamGetPlayInfo(struct DvbStreamContext_s *DvbStream,
			 stm_se_play_stream_info_t * PlayInfo);
int DvbStreamDrain(struct DvbStreamContext_s *DvbStream, unsigned int Discard);
int DvbStreamDiscontinuity(struct DvbStreamContext_s *DvbStream,
			   dvb_discontinuity_t Discontinuity);
int DvbStreamStep(struct DvbStreamContext_s *DvbStream);
int DvbStreamSwitch(struct DvbStreamContext_s *DvbStream,
		    stm_se_stream_encoding_t Encoding);
int DvbStreamSetPlayInterval(struct DvbStreamContext_s *DvbStream,
			     dvb_play_interval_t * PlayInterval);
int dvb_stream_inject(struct DvbStreamContext_s *DvbStream,
					    const char *Buffer, int count);
int DvbStreamGetFirstBuffer(struct DvbStreamContext_s *DvbStream,
			    const char __user * Buffer, unsigned int Length);
int DvbStreamIdentifyAudio(struct DvbStreamContext_s *DvbStream,
			   unsigned int *Id);
int DvbStreamIdentifyVideo(struct DvbStreamContext_s *DvbStream,
			   unsigned int *Id);
static inline int DvbPlaybackAddDemux(struct DvbPlaybackContext_s *DvbPlayback,
				      unsigned int AdapterId,
				      unsigned int DemuxId,
				      struct DvbStreamContext_s **Demux)
{
	/* STM_SE_STREAM_ENCODING_VIDEO_NONE is just a placeholder. */
	return dvb_playback_add_stream(DvbPlayback, STM_SE_MEDIA_ANY,
				    STM_SE_STREAM_ENCODING_VIDEO_NONE, 0,
				    AdapterId, DemuxId, 0, Demux);
}

static inline int DvbPlaybackRemoveDemux(struct DvbPlaybackContext_s
					 *DvbPlayback,
					 struct DvbStreamContext_s *Demux)
{
	return dvb_playback_remove_stream(DvbPlayback, Demux);
}

int DvbEventSubscribe(struct DvbStreamContext_s *DvbStream,
		      void *PrivateData,
		      uint32_t event_mask,
		      stm_event_handler_t callback,
		      stm_event_subscription_h * event_subscription);
int DvbEventUnsubscribe(struct DvbStreamContext_s *DvbStream,
			stm_event_subscription_h event_subscription);
int DvbStreamPollMessage(struct DvbStreamContext_s *DvbStream,
			 stm_se_play_stream_msg_id_t id,
			 stm_se_play_stream_msg_t * message);
int DvbStreamGetMessage(struct DvbStreamContext_s *DvbStream,
			stm_se_play_stream_subscription_h subscription,
			stm_se_play_stream_msg_t * message);
int DvbStreamMessageSubscribe(struct DvbStreamContext_s *DvbStream,
			      uint32_t message_mask,
			      uint32_t depth,
			      stm_se_play_stream_subscription_h * subscription);
int DvbStreamMessageUnsubscribe(struct DvbStreamContext_s *DvbStream,
				stm_se_play_stream_subscription_h subscription);
int DvbAttachEncoderToTsmux( stm_se_encode_stream_h	encode_stream, void *tsmux_link );
int DvbDetachEncoderToTsmux( stm_se_encode_stream_h	encode_stream, void *tsmux_link );
int DvbAttachStreamToSink( struct DvbStreamContext_s *DvbStream );
int DvbDetachStreamFromSink( struct DvbStreamContext_s *DvbStream );
int DvbPlaybackInitOption(struct DvbPlaybackContext_s *Playback, unsigned int * PlayOption, unsigned int * PlayValue);
int DvbStreamSetRegionType(struct DvbStreamContext_s *DvbStream, unsigned int Region);
int DvbStreamSetProgramReferenceLevel(struct DvbStreamContext_s *DvbStream, unsigned int prl);

#endif
