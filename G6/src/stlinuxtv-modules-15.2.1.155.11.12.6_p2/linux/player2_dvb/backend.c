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

Source file name : backend.c - linuxdvb backend engine for driving player
Author :           Julian

Date        Modification                                    Name
----        ------------                                    --------
31-Jan-07   Created                                         Julian

************************************************************************/

#include <linux/module.h>
#include <linux/ioport.h>
#include <linux/mm.h>
#include <linux/bpa2.h>
#include <linux/mutex.h>
#include <stm_display.h>
#include <linux/stm/stmcoredisplay.h>

#include <linux/stm/stmedia_export.h>

#include "stm_se.h"

#include "pes.h"
#include "dvb_module.h"
#include "backend.h"

#include "dvb_adaptation.h"
#include "demux_filter.h"

#include "stm_v4l2_encode.h"
#include "stm_v4l2_tsmux.h"

/*{{{  static data*/
static unsigned char ASFHeaderObjectGuid[] = { 0x30, 0x26, 0xb2, 0x75,
	0x8e, 0x66, 0xcf, 0x11,
	0xa6, 0xd9, 0x00, 0xaa,
	0x00, 0x62, 0xce, 0x6c
};

/*}}}*/

/*{{{  enume conversion tables*/
static const stm_se_ctrl_t DvbMapOption[] = {
	STM_SE_CTRL_TRICK_MODE_AUDIO,	/* 0 */
	STM_SE_CTRL_PLAY_24FPS_VIDEO_AT_25FPS,	/* 1 */
	STM_SE_CTRL_MASTER_CLOCK,	/* 2 */
	STM_SE_CTRL_EXTERNAL_TIME_MAPPING,	/* 3 */
	STM_SE_CTRL_ENABLE_SYNC,	/* 4 */
	STM_SE_CTRL_DISPLAY_FIRST_FRAME_EARLY,	/* 5 */
	STM_SE_CTRL_VIDEO_LAST_FRAME_BEHAVIOUR,	/* 6 */
	STM_SE_CTRL_VIDEO_KEY_FRAMES_ONLY,	/* 7 */
	STM_SE_CTRL_VIDEO_SINGLE_GOP_UNTIL_NEXT_DISCONTINUITY,	/* 8 */
	STM_SE_CTRL_CLAMP_PLAY_INTERVAL_ON_DIRECTION_CHANGE,	/* 9 */
	STM_SE_CTRL_PLAYOUT_ON_TERMINATE,	/* 10 */
	STM_SE_CTRL_PLAYOUT_ON_SWITCH,	/* 11 */
	STM_SE_CTRL_PLAYOUT_ON_DRAIN,	/* 12 */
	STM_SE_CTRL_UNKNOWN_OPTION, /* STM_SE_CTRL_DISPLAY_ASPECT_RATIO, *//* 13 */
	STM_SE_CTRL_UNKNOWN_OPTION, /* STM_SE_CTRL_DISPLAY_FORMAT, *//* 14 */
	STM_SE_CTRL_TRICK_MODE_DOMAIN,	/* 15 */
	STM_SE_CTRL_DISCARD_LATE_FRAMES,	/* 16 */
	STM_SE_CTRL_VIDEO_START_IMMEDIATE,	/* 17 */
	STM_SE_CTRL_REBASE_ON_DATA_DELIVERY_LATE,	/* 18 */
	STM_SE_CTRL_ALLOW_REBASE_AFTER_LATE_DECODE,	/* 19 */
	STM_SE_CTRL_UNKNOWN_OPTION,	/*STM_SE_CTRL_LOWER_CODEC_DECODE_LIMITS_ON_FRAME_DECODE_LATE, *//* 20 */
	STM_SE_CTRL_H264_ALLOW_NON_IDR_RESYNCHRONIZATION,	/* 21 */
	STM_SE_CTRL_MPEG2_IGNORE_PROGRESSIVE_FRAME_FLAG,	/* 22 */
	STM_SE_CTRL_UNKNOWN_OPTION,	/*STM_SE_CTRL_AUDIO_SPDIF_SOURCE, *//* 23 */
	STM_SE_CTRL_H264_ALLOW_BAD_PREPROCESSED_FRAMES,	/* 24 */
	STM_SE_CTRL_CLOCK_RATE_ADJUSTMENT_LIMIT,	/* 25 */
	STM_SE_CTRL_LIMIT_INJECT_AHEAD,	/* 26 */
	STM_SE_CTRL_MPEG2_APPLICATION_TYPE,	/* 27 */
	STM_SE_CTRL_DECIMATE_DECODER_OUTPUT,	/* 28 */
	STM_SE_CTRL_SYMMETRIC_PTS_FORWARD_JUMP_DETECTION_THRESHOLD,	/* 29 */
	STM_SE_CTRL_H264_TREAT_DUPLICATE_DPB_VALUES_AS_NON_REF_FRAME_FIRST,	/* 30 */
	STM_SE_CTRL_EXTERNAL_TIME_MAPPING_VSYNC_LOCKED,	/* 31 */
	STM_SE_CTRL_UNKNOWN_OPTION, /* STM_SE_CTRL_PIXEL_ASPECT_RATIO_CORRECTION, *//* 32 */
	STM_SE_CTRL_H264_FORCE_PIC_ORDER_CNT_IGNORE_DPB_DISPLAY_FRAME_ORDERING,	/* 33 */
	STM_SE_CTRL_SYMMETRIC_JUMP_DETECTION,	/* 34 */
	STM_SE_CTRL_ALLOW_FRAME_DISCARD_AT_NORMAL_SPEED,	/* 35 */
	STM_SE_CTRL_OPERATE_COLLATOR2_IN_REVERSIBLE_MODE,	/* 36 */
	STM_SE_CTRL_UNKNOWN_OPTION,	/* STM_SE_CTRL_VIDEO_OUTPUT_WINDOW_RESIZE_STEPS, */	/* 37 */
	STM_SE_CTRL_IGNORE_STREAM_UNPLAYABLE_CALLS,	/* 38 */
	STM_SE_CTRL_USE_PTS_DEDUCED_DEFAULT_FRAME_RATES,	/* 39 */
	STM_SE_CTRL_H264_TREAT_TOP_BOTTOM_PICTURE_AS_INTERLACED,	/* 40 */
	STM_SE_CTRL_H264_VALIDATE_DPB_VALUES_AGAINST_PTS_VALUES,	/* 41 */
	STM_SE_CTRL_LIVE_PLAY,	/* 42 */
	STM_SE_CTRL_RUNNING_DEVLOG,	/* 43 */
	STM_SE_CTRL_ERROR_DECODING_LEVEL,	/* 44 */
	STM_SE_CTRL_ALLOW_REFERENCE_FRAME_SUBSTITUTION,	/* 45 */
	STM_SE_CTRL_DISCARD_FOR_REFERENCE_QUALITY_THRESHOLD,	/* 46 */
	STM_SE_CTRL_DISCARD_FOR_MANIFESTATION_QUALITY_THRESHOLD,	/* 47 */
	STM_SE_CTRL_PLAYBACK_LATENCY,	/* 48 */
	STM_SE_CTRL_VIDEO_DISCARD_FRAMES,  /* 49 */
	STM_SE_CTRL_FRAME_RATE_CALCULUATION_PRECEDENCE, /* 50 */
	STM_SE_CTRL_PACKET_INJECTOR,	/* 51 */
	STM_SE_CTRL_VIDEO_DECODE_MEMORY_PROFILE,  /* 52 */
	STM_SE_CTRL_DEEMPHASIS,	/* 53 */
	STM_SE_CTRL_CONTAINER_FRAMERATE  /* 54 */
};

/*}}}*/

/*{{{  DvbPlaybackCreate*/
int DvbPlaybackCreate(int Id, struct DvbPlaybackContext_s **DvbPlayback)
{
	int Result;
	char name[16];

	if (*DvbPlayback != NULL)
		return -EINVAL;

	*DvbPlayback = kzalloc(sizeof(struct DvbPlaybackContext_s), GFP_KERNEL);
	if (*DvbPlayback == NULL) {
		BACKEND_ERROR
		    ("Unable to create playback context - insufficient memory\n");
		return -ENOMEM;
	}
	snprintf(name, sizeof(name), "playback%d", Id);
	name[sizeof(name) - 1] = '\0';

	Result = stm_se_playback_new(name, &(*DvbPlayback)->Handle);
	if (Result != 0) {
		BACKEND_ERROR("Unable to create playback context %d\n", Result);
		kfree(*DvbPlayback);
		*DvbPlayback = NULL;
		return Result;
	}

	mutex_init(&((*DvbPlayback)->Lock));
	(*DvbPlayback)->UsageCount = 0;

	BACKEND_TRACE("DvbPlayback %p\n", *DvbPlayback);

	return 0;
}

/*}}}*/
/*{{{  DvbPlaybackDelete*/
int DvbPlaybackDelete(struct DvbPlaybackContext_s *DvbPlayback)
{
	int Result = 0;

	if (DvbPlayback == NULL)
		return -EINVAL;

	BACKEND_TRACE("DvbPlayback %p, Usage = %d\n", DvbPlayback,
		      DvbPlayback->UsageCount);

	mutex_lock(&(DvbPlayback->Lock));

	if (DvbPlayback->UsageCount != 0) {
		BACKEND_TRACE("Cannot delete playback - usage = %d\n",
			      DvbPlayback->UsageCount);
		mutex_unlock(&(DvbPlayback->Lock));
		return -EINVAL;
	}

	if (DvbPlayback->Handle != NULL) {
		Result = stm_se_playback_delete(DvbPlayback->Handle);
		if (Result != 0)
			BACKEND_ERROR("Failed to delete playback context\n");
	}
	mutex_unlock(&(DvbPlayback->Lock));

	if (Result == 0) {
		mutex_destroy(&(DvbPlayback->Lock));
		kfree(DvbPlayback);
	}

	return Result;
}

/*}}}*/

int DvbAttachStreamToSink( struct DvbStreamContext_s *DvbStream )
{
	int Result = 0;

	if (DvbStream->Handle == NULL)
		/* Nothing to do */
		return 0;

	if (DvbStream->display_Sink){
		Result = stm_se_play_stream_attach( DvbStream->Handle,
							DvbStream->display_Sink,
							STM_SE_PLAY_STREAM_OUTPUT_PORT_DEFAULT );
	}

	return Result;
}

/**
 * stm_dvb_stream_connect_encoder() - Connect decoder to encoder.
 * @src_pad    : decoder pad
 * @dst_pad    : encoder pad
 * @entity_type: specify the encoder type (Video/Audio)
 * @play_stream: decoder play stream
 * Find the connected encoder pads and connect it to decoder if started.
 * Caller is expected to take audops_mutex/vidops_mutex before calling this
 */
int stm_dvb_stream_connect_encoder(struct media_pad *src_pad,
					const struct media_pad *dst_pad,
					unsigned long entity_type,
					stm_se_play_stream_h play_stream)
{
	int id = 0, ret = 0;
	bool search_all_encoders = false;
	struct stm_v4l2_encoder_device *enc_ctx;

	/*
	 * When audio/video play stream is created, dst_pad passed will be
	 * NULL, so, that we find all the encoders whose connections are
	 * pending to be created.
	 */
	if (!dst_pad)
		search_all_encoders = true;

	do {
		if (search_all_encoders) {
			dst_pad = stm_media_find_remote_pad_with_type(src_pad,
						MEDIA_LNK_FL_ENABLED,
						entity_type, &id);
			/*
			 * Ran out of encoders, exit the loop
			 */
			if (!dst_pad)
				break;
		}

		enc_ctx = stm_media_entity_to_encoder_context(dst_pad->entity);

		mutex_lock(&enc_ctx->lock);

		/*
		 * Encoder device not started, so, do nothing.
		 */
		if (!(enc_ctx->encode_obj && enc_ctx->encode_stream)) {
			mutex_unlock(&enc_ctx->lock);
			continue;
		}

		ret = stm_se_play_stream_attach(play_stream,
					enc_ctx->encode_stream,
					STM_SE_PLAY_STREAM_OUTPUT_PORT_DEFAULT);

		if (ret && (ret != -EALREADY)) {
			BACKEND_ERROR("Failed to connect %s to %s\n",
				src_pad->entity->name, dst_pad->entity->name);
			mutex_unlock(&enc_ctx->lock);
			continue;
		}

		BACKEND_TRACE("Decoder %s --> Encoder %s\n",
			 src_pad->entity->name, dst_pad->entity->name);

		mutex_unlock(&enc_ctx->lock);

	} while (search_all_encoders);

	return ret;
}

/**
 * stm_dvb_stream_disconnect_encoder() - Disconnect decoder from encoder.
 * @src_pad    : decoder pad
 * @dst_pad    : encoder pad
 * @entity_type: specify the encoder type (Video/Audio)
 * @play_stream: decoder play stream
 * Find the connected encoder pads and disconnect from decoder if started.
 * Caller is expected to take audops_mutex/vidops_mutex before calling this
 */
int stm_dvb_stream_disconnect_encoder(struct media_pad *src_pad,
					const struct media_pad *dst_pad,
					unsigned long entity_type,
					stm_se_play_stream_h play_stream)
{
	int id = 0, ret = 0;
	bool search_all_encoders = false;
	struct stm_v4l2_encoder_device *enc_ctx;

	/*
	 * While terminating playback we will find all the connected encoders
	 * and detach them.
	 */
	if (!dst_pad)
		search_all_encoders = true;

	do {
		if (search_all_encoders) {
			dst_pad = stm_media_find_remote_pad_with_type(src_pad,
						MEDIA_LNK_FL_ENABLED,
						entity_type, &id);
			/*
			 * Ran out of encoders, exit the loop
			 */
			if (!dst_pad)
				break;
		}

		enc_ctx = stm_media_entity_to_encoder_context(dst_pad->entity);

		mutex_lock(&enc_ctx->lock);

		/*
		 * If there's no encoder stream, then disconnect cannot be done
		 */
		if (!(enc_ctx->encode_obj && enc_ctx->encode_stream)) {
			mutex_unlock(&enc_ctx->lock);
			continue;
		}

		ret = stm_se_play_stream_detach(play_stream,
					enc_ctx->encode_stream);
		if (ret) {
			BACKEND_ERROR("Failed to disconnect %s from %s\n",
				src_pad->entity->name, dst_pad->entity->name);
			mutex_unlock(&enc_ctx->lock);
			continue;
		}

		BACKEND_TRACE("Decoder %s -X-> Encoder %s\n",
			 src_pad->entity->name, dst_pad->entity->name);

		mutex_unlock(&enc_ctx->lock);

	} while (search_all_encoders);

	return ret;
}

int DvbAttachEncoderToTsmux(stm_se_encode_stream_h encode_stream,
                            void *tsmux_link)
{
#ifdef TUNNELLING_SUPPORTED
        struct tsmux_device *tsmux_dev_p;
        int ret = 0;

        if (tsmux_link == NULL)
                return 0;

        tsmux_dev_p = (struct tsmux_device *)tsmux_link;

        /* tsmux connection */
        /* mutex_lock(&tsmux_dev_p->lock); */
        if (tsmux_dev_p->tsmux_in_ctx.src_connect_type ==
            STM_V4L2_TSMUX_CONNECT_NONE) {
                if (encode_stream == NULL)
                        ret = 0;
                else if (tsmux_dev_p->tsmux_in_ctx.
                         tsmux_object /*&& tsmux_dev_p->esd_object */ ) {
                        ret =
                            stm_se_encode_stream_attach(encode_stream,
                                                        tsmux_dev_p->
                                                        tsmux_in_ctx.
                                                        tsmux_object);
                        if (ret == 0)
                                tsmux_dev_p->tsmux_in_ctx.src_connect_type =
                                    STM_V4L2_TSMUX_CONNECT_ENCODE;
                        else
                                BACKEND_ERROR
                                    ("encodestream can't attach to tsmux");
                }
        } else {
                ret = tsmux_dev_p->tsmux_in_ctx.src_connect_type;
        }
        /* mutex_unlock(&tsmux_dev_p->lock); */

        return ret;
#endif
	return 0;
}

int DvbDetachEncoderToTsmux(stm_se_encode_stream_h encode_stream,
                            void *tsmux_link)
{
#ifdef TUNNELLING_SUPPORTED
        struct tsmux_device *tsmux_dev_p;
        int ret = 0;

        if (encode_stream == NULL)
                return 0;

        if (tsmux_link == NULL)
                return 0;

        tsmux_dev_p = (struct tsmux_device *)tsmux_link;

        /* tsmux connection */
        /* mutex_lock(&tsmux_dev_p->lock); */
        if (tsmux_dev_p->tsmux_in_ctx.src_connect_type ==
            STM_V4L2_TSMUX_CONNECT_ENCODE) {
                if (tsmux_dev_p->tsmux_in_ctx.
                    tsmux_object /*&& tsmux_dev_p->esd_object */ ) {
                        ret =
                            stm_se_encode_stream_detach(encode_stream,
                                                        tsmux_dev_p->
                                                        tsmux_in_ctx.
                                                        tsmux_object);
                        if (ret == 0)
                                tsmux_dev_p->tsmux_in_ctx.src_connect_type =
                                    STM_V4L2_TSMUX_CONNECT_NONE;
                        else
                                BACKEND_ERROR
                                    ("encodestream can't detach from tsmux\n");
                }
        }
        /* mutex_unlock(&tsmux_dev_p->lock); */

        return ret;
#endif
	return 0;
}

/**
 * dvb_playback_add_stream
 * This function exposes the functionality of attaching the playback to the
 * newly created stream object to complete the backend infrastructure
 * required to start the playback.
 */
int dvb_playback_add_stream(struct DvbPlaybackContext_s *playback,
		stm_se_media_t media, stm_se_stream_encoding_t encoding,
		unsigned int encoding_flags,
		unsigned int adapter_id, unsigned int demux_id,
		unsigned int ctx_id, struct DvbStreamContext_s **stream_ctx)
{
	int ret = 0;
	char media_name[32];
	bool new_stream = false;

	BACKEND_DEBUG("%p\n", playback);

	/*
	 * Check if the playback is already created to which
	 * stream has to be attached
	 */
	if (!playback) {
		BACKEND_ERROR("No playback created\n");
		ret = -EINVAL;
		goto add_stream_invalid_param;
	}

	/*
	 * Check if the call is re-attaching the stream to playback
	 */
	if (*stream_ctx && (*stream_ctx)->Handle) {
		DVB_ERROR("Stream context already exists\n");
		ret = -EINVAL;
		goto add_stream_invalid_param;
	}

	if (likely(!*stream_ctx)) {
		*stream_ctx = kzalloc(sizeof(struct DvbStreamContext_s),
								GFP_KERNEL);
		if (!*stream_ctx) {
			BACKEND_ERROR("Out of memory for stream context\n");
			ret = -ENOMEM;
			goto add_stream_invalid_param;
		}

		(*stream_ctx)->Media = media;

		if(media == STM_SE_MEDIA_ANY) {
			(*stream_ctx)->BufferLength = DEMUX_BUFFER_SIZE;
		} else {
			if (demux_id == INVALID_DEMUX_ID) {
				(*stream_ctx)->BufferLength = (media ==
					STM_SE_MEDIA_AUDIO) ?
					AUDIO_STREAM_BUFFER_SIZE :
					VIDEO_STREAM_BUFFER_SIZE;
			} else {
				/*
				 * The stream is part of a demux so it doesn't
				 * need its own buffer
				 */
				(*stream_ctx)->BufferLength = 0;
				(*stream_ctx)->Buffer = NULL;

				/*
				 * Default encoding to MPEG2
				 */
				if (encoding ==
					STM_SE_STREAM_ENCODING_AUDIO_AUTO) {
					encoding =
					    STM_SE_STREAM_ENCODING_AUDIO_MPEG2;
				} else if (encoding ==
					STM_SE_STREAM_ENCODING_VIDEO_AUTO) {
					encoding =
					    STM_SE_STREAM_ENCODING_VIDEO_MPEG2;
				}

			}
		}

		if ((*stream_ctx)->BufferLength) {
			(*stream_ctx)->Buffer =
				bigphysarea_alloc((*stream_ctx)->BufferLength);
			if (!(*stream_ctx)->Buffer) {
				BACKEND_ERROR("Stream memory: Out of memory\n");
				ret = -ENOMEM;
				goto add_stream_alloc_buffer_failed;
			}
		}
		new_stream = true;
	}

	mutex_lock(&(playback->Lock));
	if (encoding == STM_SE_STREAM_ENCODING_AUDIO_AUTO
	    || encoding == STM_SE_STREAM_ENCODING_VIDEO_AUTO) {

		(*stream_ctx)->Handle = NULL;
		ret = 0;

	}else {
		char *media_str = NULL;
		switch (media) {
			case STM_SE_MEDIA_AUDIO:
				media_str = "audio";
				break;

			case STM_SE_MEDIA_VIDEO:
				media_str = "video";
				break;

			case STM_SE_MEDIA_ANY:
				BACKEND_ERROR("Wrong media type %d", media);
				media_str = "unknown";
				break;
		}

		snprintf(media_name, sizeof(media_name), "dvb%u.%s%u",
					adapter_id, media_str, ctx_id);
		media_name[strlen(media_name)] = '\0';

		/* if in "zero copy mode", set policy to prevent the
                 * next play_stream_new to allocate display frame buffers.
                 * These will be given later (via V4L2) by user appl.
                 */
		if (encoding_flags & VIDEO_ENCODING_USER_ALLOCATED_FRAMES) {
			ret = stm_se_playback_set_control(playback->Handle,
				   STM_SE_CTRL_DECODE_BUFFERS_USER_ALLOCATED,
				   true);
			if (ret != 0) {
				/* treat as a stream creation error */
				BACKEND_ERROR("Cannot create a stream for 'zero copy' capture!\n");
				goto add_stream_create_stream_failed;
			}
		}
		ret = stm_se_play_stream_new(media_name, playback->Handle,
					     encoding, &(*stream_ctx)->Handle);
	}

	if (ret) {
		BACKEND_ERROR("Unable to create stream context\n");
		goto add_stream_create_stream_failed;
	}

	mutex_init(&((*stream_ctx)->Lock));

	if (new_stream)
		playback->UsageCount++;

	mutex_unlock(&(playback->Lock));

	BACKEND_DEBUG("%p: UsageCount = %d\n", playback, playback->UsageCount);

	return 0;

add_stream_create_stream_failed:
	bigphysarea_free_pages((*stream_ctx)->Buffer);
	(*stream_ctx)->Buffer = NULL;
	mutex_unlock(&(playback->Lock));
add_stream_alloc_buffer_failed:
	kfree(*stream_ctx);
	*stream_ctx = NULL;
add_stream_invalid_param:
	return ret;
}

int DvbDetachStreamFromSink( struct DvbStreamContext_s *DvbStream )
{
	int Result = 0;

	if (!DvbStream->Handle)
		return 0;

	if (DvbStream->display_Sink )
		Result = stm_se_play_stream_detach( DvbStream->Handle,
							DvbStream->display_Sink );

	if (DvbStream->grab_Sink )
		Result = stm_se_play_stream_detach( DvbStream->Handle,
							DvbStream->grab_Sink );

	if (DvbStream->user_data_Sink )
		Result = stm_se_play_stream_detach( DvbStream->Handle,
							DvbStream->user_data_Sink );

	return Result;
}

/**
 * dvb_playback_remove_stream
 * Detach the stream from player2
 */
int dvb_playback_remove_stream(struct DvbPlaybackContext_s *playback,
				    struct DvbStreamContext_s *stream_ctx)
{
	int ret = 0;

	if (!playback || !stream_ctx) {
		BACKEND_DEBUG("No playback/stream context, nothing to do\n");
		goto remove_stream_invalid_param;
	}

	BACKEND_DEBUG("%p: Usage = %d\n", playback, playback->UsageCount);

	mutex_lock(&(playback->Lock));

	if (stream_ctx->Handle) {
		ret = DvbDetachStreamFromSink(stream_ctx);
		if (ret)
			BACKEND_ERROR("Failed to detach stream from sink\n");

		if (stream_ctx->Media != STM_SE_MEDIA_ANY)
			ret = stm_se_play_stream_delete(stream_ctx->Handle);

		if (ret)
			BACKEND_ERROR
			    ("Failed to remove stream from playback\n");
	}

	if (stream_ctx->Buffer) {
		bigphysarea_free_pages(stream_ctx->Buffer);
		stream_ctx->Buffer = NULL;
	}

	kfree(stream_ctx);

	playback->UsageCount--;
	mutex_unlock(&(playback->Lock));

remove_stream_invalid_param:
	return ret;
}

/*}}}*/

/**
 * dvb_playstream_add_memsrc
 * This function creates a new memsrc object and connects it to play stream.
 * @media: STM_SE_MEDIA_AUDIO/STM_SE_MEDIA_VIDEO
 * @ctx_id : playback context id
 * @stream_ctx: stream to attach memsrc to
 * @memsrc: memsrc pointer which will be returned
 */
int dvb_playstream_add_memsrc(stm_se_media_t media, int ctx_id,
			stm_se_play_stream_h play_stream, stm_memsrc_h *memsrc)
{
	int ret = 0;
	char memsrc_name[STM_REGISTRY_MAX_TAG_SIZE];

	/*
	 * Check if any invalid parameters are passed?
	 */
	if ((media >= STM_SE_MEDIA_ANY) || (ctx_id < 0)) {
		BACKEND_ERROR("Invalid parameters passed for memsrc\n");
		ret = -EINVAL;
		goto add_memsrc_done;
	}

	/*
	 * Create a new memsrc
	 */
	snprintf(memsrc_name, STM_REGISTRY_MAX_TAG_SIZE, "DVB-%s%d-SE-SRC",
		(media == STM_SE_MEDIA_AUDIO) ? "AUDIO" : "VIDEO", ctx_id);

	ret = stm_memsrc_new(memsrc_name,
				STM_IOMODE_BLOCKING_IO, KERNEL, memsrc);
	if (ret) {
		BACKEND_ERROR("Failed to create %s memsrc\n", memsrc_name);
		goto add_memsrc_done;
	}

	ret = stm_memsrc_attach(*memsrc, play_stream, STM_DATA_INTERFACE_PUSH);
	if (ret) {
		BACKEND_ERROR("Failed to attach %s memsrc\n", memsrc_name);
		goto add_memsrc_attach_failed;
	}

	return 0;

add_memsrc_attach_failed:
	if (stm_memsrc_delete(*memsrc))
		BACKEND_ERROR("Failed to delete %s memsrc\n", memsrc_name);
	*memsrc = NULL;
add_memsrc_done:
	return ret;
}

/**
 * dvb_playstream_remove_memsrc() - deletes the memsrc attached to play stream
 * @memsrc: valid memsrc
 * Detach memsrc from playstream and delete it, so, that no injection can happen
 */
void dvb_playstream_remove_memsrc(stm_memsrc_h memsrc)
{
	int ret = 0;

	/*
	 * Detach and delete memsrc created to inject data into player2
	 */
	ret = stm_memsrc_detach(memsrc);
	if (ret)
		BACKEND_ERROR("Failed to detach memsrc\n");

	ret = stm_memsrc_delete(memsrc);
	if (ret)
		BACKEND_ERROR("Failed to delete memsrc\n");
}

/*{{{  DvbPlaybackSetSpeed*/
int DvbPlaybackSetSpeed(struct DvbPlaybackContext_s *DvbPlayback,
			unsigned int Speed)
{
	if (DvbPlayback == NULL)
		return -EINVAL;

	return stm_se_playback_set_speed(DvbPlayback->Handle, Speed);
}

/*}}}*/
/*{{{  DvbPlaybackGetSpeed*/
int DvbPlaybackGetSpeed(struct DvbPlaybackContext_s *DvbPlayback,
			unsigned int *Speed)
{
	if (DvbPlayback == NULL)
		return -EINVAL;

	return stm_se_playback_get_speed(DvbPlayback->Handle, Speed);
}

/*}}}*/
/*{{{  DvbPlaybackSetOption*/
int DvbPlaybackSetOption(struct DvbPlaybackContext_s *DvbPlayback,
			 dvb_option_t DvbOption, unsigned int Value)
{
	stm_se_ctrl_t PlayerOption;
	unsigned int OptionValue = 0;

	if ((DvbPlayback == NULL) || (DvbPlayback->Handle == NULL))	/* No playback to set option on */
		return -EINVAL;

	PlayerOption = DvbMapOption[DvbOption];
	if (PlayerOption == STM_SE_CTRL_UNKNOWN_OPTION)
		return -EINVAL;

	switch (DvbOption) {
	case DVB_OPTION_MASTER_CLOCK:
		switch (Value) {
		case DVB_OPTION_VALUE_VIDEO_CLOCK_MASTER:
			OptionValue = STM_SE_CTRL_VALUE_VIDEO_CLOCK_MASTER;
			break;
		case DVB_OPTION_VALUE_AUDIO_CLOCK_MASTER:
			OptionValue = STM_SE_CTRL_VALUE_AUDIO_CLOCK_MASTER;
			break;
		default:
			OptionValue = STM_SE_CTRL_VALUE_SYSTEM_CLOCK_MASTER;
			break;
		}
		break;
	case DVB_OPTION_DISCARD_LATE_FRAMES:
		switch (Value) {
		case DVB_OPTION_VALUE_DISCARD_LATE_FRAMES_NEVER:
			OptionValue =
			    STM_SE_CTRL_VALUE_DISCARD_LATE_FRAMES_NEVER;
			break;
		case DVB_OPTION_VALUE_DISCARD_LATE_FRAMES_ALWAYS:
			OptionValue =
			    STM_SE_CTRL_VALUE_DISCARD_LATE_FRAMES_ALWAYS;
			break;
		default:
			OptionValue =
			    STM_SE_CTRL_VALUE_DISCARD_LATE_FRAMES_AFTER_SYNCHRONIZE;
			break;
		}
		break;
	case DVB_OPTION_VIDEO_START_IMMEDIATE:
		OptionValue =
		    (Value ==
		     DVB_OPTION_VALUE_ENABLE) ? STM_SE_CTRL_VALUE_APPLY :
		    STM_SE_CTRL_VALUE_DISAPPLY;
		break;
	case DVB_OPTION_SYMMETRIC_JUMP_DETECTION:
		OptionValue = Value;
		break;
	case DVB_OPTION_PTS_FORWARD_JUMP_DETECTION_THRESHOLD:
		OptionValue = Value;
		break;
	case DVB_OPTION_ALLOW_FRAME_DISCARD_AT_NORMAL_SPEED:
		OptionValue = Value;
		break;
	case DVB_OPTION_OPERATE_COLLATOR2_IN_REVERSIBLE_MODE:
		OptionValue = Value;
		break;
	case DVB_OPTION_LIVE_PLAY:
		OptionValue =
		    (Value ==
		     DVB_OPTION_VALUE_ENABLE) ? STM_SE_CTRL_VALUE_APPLY :
		    STM_SE_CTRL_VALUE_DISAPPLY;
		break;
	case DVB_OPTION_PACKET_INJECTOR:
		OptionValue =
		    (Value ==
		     DVB_OPTION_VALUE_ENABLE) ? STM_SE_CTRL_VALUE_APPLY :
		    STM_SE_CTRL_VALUE_DISAPPLY;
		break;
	case DVB_OPTION_RUNNING_DEVLOG:
		OptionValue =
		    (Value ==
		     DVB_OPTION_VALUE_ENABLE) ? STM_SE_CTRL_VALUE_APPLY :
		    STM_SE_CTRL_VALUE_DISAPPLY;
		break;
	case DVB_OPTION_CTRL_PLAYBACK_LATENCY:
		OptionValue = Value;
		break;
	case DVB_OPTION_CTRL_VIDEO_DISCARD_FRAMES:
		OptionValue = Value;
		break;

	case DVB_OPTION_CTRL_VIDEO_MEMORY_PROFILE:
		switch (Value) {
		case DVB_OPTION_CTRL_VALUE_4K2K_PROFILE:
			OptionValue = STM_SE_CTRL_VALUE_VIDEO_DECODE_4K2K_PROFILE;
			break;

		case DVB_OPTION_CTRL_VALUE_SD_PROFILE:
			OptionValue = STM_SE_CTRL_VALUE_VIDEO_DECODE_SD_PROFILE;
			break;

		case DVB_OPTION_CTRL_VALUE_720P_PROFILE:
			OptionValue = STM_SE_CTRL_VALUE_VIDEO_DECODE_720P_PROFILE;
			break;

		case DVB_OPTION_CTRL_VALUE_UHD_PROFILE:
			OptionValue = STM_SE_CTRL_VALUE_VIDEO_DECODE_UHD_PROFILE;
			break;

		case DVB_OPTION_CTRL_VALUE_HD_LEGACY_PROFILE:
			OptionValue = STM_SE_CTRL_VALUE_VIDEO_DECODE_HD_LEGACY_PROFILE;
			break;

		case DVB_OPTION_CTRL_VALUE_HD_PROFILE:
		default:
			OptionValue = STM_SE_CTRL_VALUE_VIDEO_DECODE_HD_PROFILE;
			break;
		}

		break;

	case DVB_OPTION_CTRL_AUDIO_DEEMPHASIS:
		OptionValue = Value;
		break;
	case DVB_OPTION_CTRL_CONTAINER_FRAMERATE:
		OptionValue = Value;
		break;
	case DVB_OPTION_DECIMATE_DECODER_OUTPUT:
		switch (Value) {
		case DVB_OPTION_VALUE_DECIMATE_DECODER_OUTPUT_HALF:
			OptionValue =
			    STM_SE_CTRL_VALUE_DECIMATE_DECODER_OUTPUT_HALF;
			break;
		case DVB_OPTION_VALUE_DECIMATE_DECODER_OUTPUT_QUARTER:
			OptionValue =
			    STM_SE_CTRL_VALUE_DECIMATE_DECODER_OUTPUT_QUARTER;
			break;
		default:
			OptionValue =
			    STM_SE_CTRL_VALUE_DECIMATE_DECODER_OUTPUT_DISABLED;
			break;
		}
		break;
	default:
		BACKEND_ERROR("Unknown dvb option %d\n", DvbOption);
		return -EINVAL;

	}

	return stm_se_playback_set_control(DvbPlayback->Handle, PlayerOption,
					   OptionValue);
}

/*}}}*/
/*{{{  DvbPlaybackSetNativePlaybackTime*/
int DvbPlaybackSetNativePlaybackTime(struct DvbPlaybackContext_s *DvbPlayback,
				     unsigned long long NativeTime,
				     unsigned long long SystemTime)
{
	if (DvbPlayback == NULL)
		return -EINVAL;

	if (NativeTime == SystemTime) {
		BACKEND_DEBUG
		    ("NativeTime == SystemTime -> no need for sync call \n");
		return 0;
	}

	return stm_se_playback_set_native_time(DvbPlayback->Handle, NativeTime,
					       SystemTime);
}

/*}}}*/
/*{{{  DvbPlaybackSetClockDataPoint*/
int DvbPlaybackSetClockDataPoint(struct DvbPlaybackContext_s *DvbPlayback,
				 dvb_clock_data_point_t * ClockData)
{
	stm_se_time_format_t TimeFormat;
	bool Initialise;
	dvb_clock_flags_t ClockFormat =
	    ClockData->flags & DVB_CLOCK_FORMAT_MASK;

	if (DvbPlayback == NULL)
		return -EINVAL;

	TimeFormat = (ClockFormat == DVB_CLOCK_FORMAT_US) ? TIME_FORMAT_US :
	    (ClockFormat ==
	     DVB_CLOCK_FORMAT_PTS) ? TIME_FORMAT_PTS : TIME_FORMAT_27MHz;
	Initialise = (ClockData->flags & DVB_CLOCK_INITIALIZE) ? true : false;
	return stm_se_playback_set_clock_data_point(DvbPlayback->Handle,
						    TimeFormat,
						    Initialise,
						    ClockData->source_time,
						    ClockData->system_time);

}

/*}}}*/
/*{{{  DvbPlaybackGetClockDataPoint*/
int DvbPlaybackGetClockDataPoint(struct DvbPlaybackContext_s *DvbPlayback,
				 dvb_clock_data_point_t * ClockData)
{
	if (DvbPlayback == NULL)
		return -EINVAL;

	return stm_se_playback_get_clock_data_point(DvbPlayback->Handle,
						    &ClockData->source_time,
						    &ClockData->system_time);

}

/*}}}*/

/**
 * dvb_player_inject_data
 * This API exports the functionality to inject audio/video data
 * using memsrc into player. For any other data, legacy interface
 * is used.
 * @stream_ctx: The Audio/Video stream context where the data
 *              is to be injected.
 * @buf:	The data from the user
 * @count:	The number of bytes requested.
 * This returns the number of bytes successfully written to the
 * stream context
 */
int dvb_stream_inject(struct DvbStreamContext_s *stream_ctx,
					const char *buf, int count)
{
	int ret = 0, size;

	mutex_lock(&stream_ctx->Lock);

	if (stream_ctx->Media != STM_SE_MEDIA_ANY) {

		ret = stm_memsrc_push_data(stream_ctx->memsrc,
					(void *)buf, count, &size);
		if (!ret)
			ret = size;
	}

	mutex_unlock(&stream_ctx->Lock);

	return ret;
}

/*}}}*/
/*{{{  DvbStreamSetOption*/
int DvbStreamSetOption(struct DvbStreamContext_s *DvbStream,
		       dvb_option_t DvbOption, unsigned int Value)
{
	stm_se_ctrl_t PlayerOption;
	unsigned int OptionValue = 0;

	if ((DvbStream == NULL) || (DvbStream->Handle == NULL))	/* No playback to set option on */
		return -EINVAL;

	PlayerOption = DvbMapOption[DvbOption];
	if (PlayerOption == STM_SE_CTRL_UNKNOWN_OPTION)
		return -EINVAL;

	switch (DvbOption) {
	case DVB_OPTION_VIDEO_BLANK:
		OptionValue =
		    (Value ==
		     DVB_OPTION_VALUE_ENABLE) ? STM_SE_CTRL_VALUE_BLANK_SCREEN :
		    STM_SE_CTRL_VALUE_LEAVE_LAST_FRAME_ON_SCREEN;
		break;
	case DVB_OPTION_AV_SYNC:
	case DVB_OPTION_TRICK_MODE_AUDIO:
	case DVB_OPTION_PLAY_24FPS_VIDEO_AT_25FPS:
	case DVB_OPTION_EXTERNAL_TIME_MAPPING_VSYNC_LOCKED:
	case DVB_OPTION_DISPLAY_FIRST_FRAME_EARLY:
	case DVB_OPTION_STREAM_ONLY_KEY_FRAMES:
	case DVB_OPTION_STREAM_SINGLE_GROUP_BETWEEN_DISCONTINUITIES:
	case DVB_OPTION_VIDEO_START_IMMEDIATE:
	case DVB_OPTION_REBASE_ON_DATA_DELIVERY_LATE:
	case DVB_OPTION_REBASE_ON_FRAME_DECODE_LATE:
	case DVB_OPTION_H264_ALLOW_NON_IDR_RESYNCHRONIZATION:
	case DVB_OPTION_MPEG2_IGNORE_PROGESSIVE_FRAME_FLAG:
	case DVB_OPTION_CLAMP_PLAYBACK_INTERVAL_ON_PLAYBACK_DIRECTION_CHANGE:
	case DVB_OPTION_H264_ALLOW_BAD_PREPROCESSED_FRAMES:
	case DVB_OPTION_H264_TREAT_DUPLICATE_DPB_AS_NON_REFERENCE_FRAME_FIRST:
	case DVB_OPTION_H264_FORCE_PIC_ORDER_CNT_IGNORE_DPB_DISPLAY_FRAME_ORDERING:
	case DVB_OPTION_H264_VALIDATE_DPB_VALUES_AGAINST_PTS_VALUES:
	case DVB_OPTION_H264_TREAT_TOP_BOTTOM_PICTURE_STRUCT_AS_INTERLACED:
	case DVB_OPTION_LIMIT_INPUT_INJECT_AHEAD:
	case DVB_OPTION_IGNORE_STREAM_UNPLAYABLE_CALLS:
	case DVB_OPTION_USE_PTS_DEDUCED_DEFAULT_FRAME_RATES:
	case DVB_OPTION_EXTERNAL_TIME_MAPPING:
	case DVB_OPTION_SYMMETRIC_JUMP_DETECTION:
	case DVB_OPTION_PLAYOUT_ON_TERMINATE:
	case DVB_OPTION_PLAYOUT_ON_SWITCH:
	case DVB_OPTION_PLAYOUT_ON_DRAIN:
		OptionValue =
		    (Value ==
		     DVB_OPTION_VALUE_ENABLE) ? STM_SE_CTRL_VALUE_APPLY :
		    STM_SE_CTRL_VALUE_DISAPPLY;
		break;
	case DVB_OPTION_TRICK_MODE_DOMAIN:
		switch (Value) {
		case DVB_OPTION_VALUE_TRICK_MODE_AUTO:
			OptionValue = STM_SE_CTRL_VALUE_TRICK_MODE_AUTO;
			break;
		case DVB_OPTION_VALUE_TRICK_MODE_DECODE_ALL:
			OptionValue = STM_SE_CTRL_VALUE_TRICK_MODE_DECODE_ALL;
			break;
		case DVB_OPTION_VALUE_TRICK_MODE_DECODE_ALL_DEGRADE_NON_REFERENCE_FRAMES:
			OptionValue =
			    STM_SE_CTRL_VALUE_TRICK_MODE_DECODE_ALL_DEGRADE_NON_REFERENCE_FRAMES;
			break;
		case DVB_OPTION_VALUE_TRICK_MODE_START_DISCARDING_NON_REFERENCE_FRAMES:
			OptionValue =
			    STM_SE_CTRL_VALUE_TRICK_MODE_DISCARD_NON_REFERENCE_FRAMES;
			break;
		case DVB_OPTION_VALUE_TRICK_MODE_DECODE_REFERENCE_FRAMES_DEGRADE_NON_KEY_FRAMES:
			OptionValue =
			    STM_SE_CTRL_VALUE_TRICK_MODE_DECODE_REFERENCE_FRAMES_DEGRADE_NON_KEY_FRAMES;
			break;
		case DVB_OPTION_VALUE_TRICK_MODE_DECODE_KEY_FRAMES:
			OptionValue =
			    STM_SE_CTRL_VALUE_TRICK_MODE_DECODE_KEY_FRAMES;
			break;
		case DVB_OPTION_VALUE_TRICK_MODE_DISCONTINUOUS_KEY_FRAMES:
			OptionValue =
			    STM_SE_CTRL_VALUE_TRICK_MODE_DISCONTINUOUS_KEY_FRAMES;
			break;
		}
		break;
	case DVB_OPTION_DISCARD_LATE_FRAMES:
		switch (Value) {
		case DVB_OPTION_VALUE_DISCARD_LATE_FRAMES_NEVER:
			OptionValue =
			    STM_SE_CTRL_VALUE_DISCARD_LATE_FRAMES_NEVER;
			break;
		case DVB_OPTION_VALUE_DISCARD_LATE_FRAMES_ALWAYS:
			OptionValue =
			    STM_SE_CTRL_VALUE_DISCARD_LATE_FRAMES_ALWAYS;
			break;
		default:
			OptionValue =
			    STM_SE_CTRL_VALUE_DISCARD_LATE_FRAMES_AFTER_SYNCHRONIZE;
			break;
		}
		break;
	case DVB_OPTION_PTS_FORWARD_JUMP_DETECTION_THRESHOLD:
	case DVB_OPTION_CLOCK_RATE_ADJUSTMENT_LIMIT_2_TO_THE_N_PARTS_PER_MILLION:
	case DVB_OPTION_ALLOW_FRAME_DISCARD_AT_NORMAL_SPEED:
	case DVB_OPTION_OPERATE_COLLATOR2_IN_REVERSIBLE_MODE:
		OptionValue = Value;
		break;
	case DVB_OPTION_MPEG2_APPLICATION_TYPE:
		switch (Value) {
		case DVB_OPTION_VALUE_MPEG2_APPLICATION_DVB:
			OptionValue = STM_SE_CTRL_VALUE_MPEG2_APPLICATION_DVB;
			break;
		case DVB_OPTION_VALUE_MPEG2_APPLICATION_ATSC:
			OptionValue = STM_SE_CTRL_VALUE_MPEG2_APPLICATION_ATSC;
			break;
		default:
			OptionValue = STM_SE_CTRL_VALUE_MPEG2_APPLICATION_MPEG2;
			break;
		}
		break;
	case DVB_OPTION_DECIMATE_DECODER_OUTPUT:
		switch (Value) {
		case DVB_OPTION_VALUE_DECIMATE_DECODER_OUTPUT_HALF:
			OptionValue =
			    STM_SE_CTRL_VALUE_DECIMATE_DECODER_OUTPUT_HALF;
			break;
		case DVB_OPTION_VALUE_DECIMATE_DECODER_OUTPUT_QUARTER:
			OptionValue =
			    STM_SE_CTRL_VALUE_DECIMATE_DECODER_OUTPUT_QUARTER;
			break;
		default:
			OptionValue =
			    STM_SE_CTRL_VALUE_DECIMATE_DECODER_OUTPUT_DISABLED;
			break;
		}
		break;
	case DVB_OPTION_LOWER_CODEC_DECODE_LIMITS_ON_FRAME_DECODE_LATE:
		BACKEND_ERROR
		    ("Option DVB_OPTION_LOWER_CODEC_DECODE_LIMITS_ON_FRAME_DECODE_LATE no longer supported\n");
		return -EINVAL;
		break;
	case DVB_OPTION_ERROR_DECODING_LEVEL:
		OptionValue =
		    (Value ==
		     DVB_OPTION_VALUE_ERROR_DECODING_LEVEL_MAXIMUM) ?
		    STM_SE_CTRL_VALUE_ERROR_DECODING_LEVEL_MAXIMUM :
		    STM_SE_CTRL_VALUE_ERROR_DECODING_LEVEL_NORMAL;
		break;
	case DVB_OPTION_ALLOW_REFERENCE_FRAME_SUBSTITUTION:
		OptionValue = (Value == DVB_OPTION_VALUE_ENABLE) ?
		    STM_SE_CTRL_VALUE_APPLY : STM_SE_CTRL_VALUE_DISAPPLY;
		break;
	case DVB_OPTION_DISCARD_FOR_MANIFESTATION_QUALITY_THRESHOLD:
	case DVB_OPTION_DISCARD_FOR_REFERENCE_QUALITY_THRESHOLD:
		if (Value <= DVB_OPTION_QUALITY_THRESHOLD_MAX)
			OptionValue = Value;
		else
			return -EINVAL;
		break;
	case DVB_OPTION_FRAME_RATE_CALCULATION_PRECEDENCE:
		if (Value <= DVB_OPTION_VALUE_PRECEDENCE_CONTAINER_STREAM_PTS)
			OptionValue = Value;
		else
			return -EINVAL;
		break;
	default:
		BACKEND_ERROR("Unknown option %d\n", DvbOption);
		return -EINVAL;

	}

	return stm_se_play_stream_set_control(DvbStream->Handle, PlayerOption,
					      OptionValue);
}

/*}}}*/
/*{{{  DvbStreamGetOption*/
int DvbStreamGetOption(struct DvbStreamContext_s *DvbStream,
		       dvb_option_t DvbOption, unsigned int *Value)
{
	stm_se_ctrl_t PlayerOption;
	int PlayerStatus;
	unsigned int OptionValue;

	if ((DvbStream == NULL) || (DvbStream->Handle == NULL))	/* No stream to get option on */
		return -EINVAL;

	PlayerOption = DvbMapOption[DvbOption];
	if (PlayerOption == STM_SE_CTRL_UNKNOWN_OPTION)
		return -EINVAL;

	PlayerStatus =
	    stm_se_play_stream_get_control(DvbStream->Handle, PlayerOption,
					   &OptionValue);
	if (PlayerStatus != 0)
		return PlayerStatus;

	switch (DvbOption) {
	case DVB_OPTION_VIDEO_BLANK:
		*Value =
		    (OptionValue ==
		     STM_SE_CTRL_VALUE_BLANK_SCREEN) ? DVB_OPTION_VALUE_ENABLE :
		    DVB_OPTION_VALUE_DISABLE;
		break;
	case DVB_OPTION_EXTERNAL_TIME_MAPPING:
	case DVB_OPTION_EXTERNAL_TIME_MAPPING_VSYNC_LOCKED:
	case DVB_OPTION_AV_SYNC:
	case DVB_OPTION_PLAY_24FPS_VIDEO_AT_25FPS:
	case DVB_OPTION_DISPLAY_FIRST_FRAME_EARLY:
	case DVB_OPTION_STREAM_ONLY_KEY_FRAMES:
	case DVB_OPTION_STREAM_SINGLE_GROUP_BETWEEN_DISCONTINUITIES:
	case DVB_OPTION_PLAYOUT_ON_TERMINATE:
	case DVB_OPTION_PLAYOUT_ON_SWITCH:
	case DVB_OPTION_PLAYOUT_ON_DRAIN:
	case DVB_OPTION_VIDEO_START_IMMEDIATE:
	case DVB_OPTION_REBASE_ON_DATA_DELIVERY_LATE:
	case DVB_OPTION_REBASE_ON_FRAME_DECODE_LATE:
	case DVB_OPTION_H264_ALLOW_NON_IDR_RESYNCHRONIZATION:
	case DVB_OPTION_MPEG2_IGNORE_PROGESSIVE_FRAME_FLAG:
	case DVB_OPTION_CLAMP_PLAYBACK_INTERVAL_ON_PLAYBACK_DIRECTION_CHANGE:
	case DVB_OPTION_H264_ALLOW_BAD_PREPROCESSED_FRAMES:
	case DVB_OPTION_H264_TREAT_DUPLICATE_DPB_AS_NON_REFERENCE_FRAME_FIRST:
	case DVB_OPTION_H264_FORCE_PIC_ORDER_CNT_IGNORE_DPB_DISPLAY_FRAME_ORDERING:
	case DVB_OPTION_H264_VALIDATE_DPB_VALUES_AGAINST_PTS_VALUES:
	case DVB_OPTION_H264_TREAT_TOP_BOTTOM_PICTURE_STRUCT_AS_INTERLACED:
	case DVB_OPTION_LIMIT_INPUT_INJECT_AHEAD:
	case DVB_OPTION_IGNORE_STREAM_UNPLAYABLE_CALLS:
	case DVB_OPTION_USE_PTS_DEDUCED_DEFAULT_FRAME_RATES:
	case DVB_OPTION_SYMMETRIC_JUMP_DETECTION:
		*Value =
		    (OptionValue ==
		     STM_SE_CTRL_VALUE_APPLY) ? DVB_OPTION_VALUE_ENABLE :
		    DVB_OPTION_VALUE_DISABLE;
		break;
	case DVB_OPTION_PTS_FORWARD_JUMP_DETECTION_THRESHOLD:
	case DVB_OPTION_CLOCK_RATE_ADJUSTMENT_LIMIT_2_TO_THE_N_PARTS_PER_MILLION:
	case DVB_OPTION_ALLOW_FRAME_DISCARD_AT_NORMAL_SPEED:
	case DVB_OPTION_OPERATE_COLLATOR2_IN_REVERSIBLE_MODE:
		*Value = OptionValue;
		break;
	case DVB_OPTION_TRICK_MODE_DOMAIN:
		switch (OptionValue) {
		case STM_SE_CTRL_VALUE_TRICK_MODE_AUTO:
			*Value = DVB_OPTION_VALUE_TRICK_MODE_AUTO;
			break;
		case STM_SE_CTRL_VALUE_TRICK_MODE_DECODE_ALL:
			*Value = DVB_OPTION_VALUE_TRICK_MODE_DECODE_ALL;
			break;
		case STM_SE_CTRL_VALUE_TRICK_MODE_DECODE_ALL_DEGRADE_NON_REFERENCE_FRAMES:
			*Value =
			    DVB_OPTION_VALUE_TRICK_MODE_DECODE_ALL_DEGRADE_NON_REFERENCE_FRAMES;
			break;
		case STM_SE_CTRL_VALUE_TRICK_MODE_DISCARD_NON_REFERENCE_FRAMES:
			*Value =
			    DVB_OPTION_VALUE_TRICK_MODE_START_DISCARDING_NON_REFERENCE_FRAMES;
			break;
		case STM_SE_CTRL_VALUE_TRICK_MODE_DECODE_REFERENCE_FRAMES_DEGRADE_NON_KEY_FRAMES:
			*Value =
			    DVB_OPTION_VALUE_TRICK_MODE_DECODE_REFERENCE_FRAMES_DEGRADE_NON_KEY_FRAMES;
			break;
		case STM_SE_CTRL_VALUE_TRICK_MODE_DECODE_KEY_FRAMES:
			*Value = DVB_OPTION_VALUE_TRICK_MODE_DECODE_KEY_FRAMES;
			break;
		case STM_SE_CTRL_VALUE_TRICK_MODE_DISCONTINUOUS_KEY_FRAMES:
			*Value =
			    DVB_OPTION_VALUE_TRICK_MODE_DISCONTINUOUS_KEY_FRAMES;
			break;
		}
		break;
	case DVB_OPTION_DISCARD_LATE_FRAMES:
		switch (OptionValue) {
		case STM_SE_CTRL_VALUE_DISCARD_LATE_FRAMES_NEVER:
			*Value = DVB_OPTION_VALUE_DISCARD_LATE_FRAMES_NEVER;
			break;
		case STM_SE_CTRL_VALUE_DISCARD_LATE_FRAMES_ALWAYS:
			*Value = DVB_OPTION_VALUE_DISCARD_LATE_FRAMES_ALWAYS;
			break;
		default:
			*Value =
			    DVB_OPTION_VALUE_DISCARD_LATE_FRAMES_AFTER_SYNCHRONIZE;
			break;
		}
		break;
	case DVB_OPTION_MPEG2_APPLICATION_TYPE:
		switch (OptionValue) {
		case STM_SE_CTRL_VALUE_MPEG2_APPLICATION_DVB:
			*Value = DVB_OPTION_VALUE_MPEG2_APPLICATION_DVB;
			break;
		case STM_SE_CTRL_VALUE_MPEG2_APPLICATION_ATSC:
			*Value = DVB_OPTION_VALUE_MPEG2_APPLICATION_ATSC;
			break;
		default:
			*Value = DVB_OPTION_VALUE_MPEG2_APPLICATION_MPEG2;
			break;
		}
		break;
	case DVB_OPTION_DECIMATE_DECODER_OUTPUT:
		switch (OptionValue) {
		case STM_SE_CTRL_VALUE_DECIMATE_DECODER_OUTPUT_HALF:
			*Value = DVB_OPTION_VALUE_DECIMATE_DECODER_OUTPUT_HALF;
			break;
		case STM_SE_CTRL_VALUE_DECIMATE_DECODER_OUTPUT_QUARTER:
			*Value =
			    DVB_OPTION_VALUE_DECIMATE_DECODER_OUTPUT_QUARTER;
			break;
		default:
			*Value =
			    DVB_OPTION_VALUE_DECIMATE_DECODER_OUTPUT_DISABLED;
			break;
		}
		break;
	case DVB_OPTION_FRAME_RATE_CALCULATION_PRECEDENCE:
		*Value = OptionValue;
		break;
	default:
		BACKEND_ERROR("Unknown option %d\n", DvbOption);
		return -EINVAL;
	}

	return 0;
}

/*}}}*/
/*{{{  DvbStreamEnable*/
int DvbStreamEnable(struct DvbStreamContext_s *DvbStream, unsigned int Enable)
{
	if ((DvbStream == NULL) || (DvbStream->Handle == NULL))
		return -EINVAL;

	return stm_se_play_stream_set_enable(DvbStream->Handle, Enable);
}

/*}}}*/
/*{{{  DvbStreamDrain*/
int DvbStreamDrain(struct DvbStreamContext_s *DvbStream, unsigned int Discard)
{
	if ((DvbStream == NULL) || (DvbStream->Handle == NULL))	/* No stream to drain */
		return -EINVAL;

	return stm_se_play_stream_drain(DvbStream->Handle, Discard);
}

/*}}}*/
/*{{{  DvbStreamGetPlayInfo*/
int DvbStreamGetPlayInfo(struct DvbStreamContext_s *DvbStream,
			 stm_se_play_stream_info_t * PlayInfo)
{
	if ((DvbStream == NULL) || (DvbStream->Handle == NULL))
		return -EINVAL;

	return stm_se_play_stream_get_info(DvbStream->Handle, PlayInfo);
}

/*}}}*/
/*{{{  DvbStreamSetPlayInterval*/
int DvbStreamSetPlayInterval(struct DvbStreamContext_s *DvbStream,
			     dvb_play_interval_t * PlayInterval)
{
	if ((DvbStream == NULL) || (DvbStream->Handle == NULL))	/* No playback to set option on */
		return -EINVAL;

	/* FIXME - this is a WA until the SE fix the function handling */
	if ((PlayInterval->start == DVB_TIME_NOT_BOUNDED) &&
		(PlayInterval->end == DVB_TIME_NOT_BOUNDED)) {
		return 0;
	}

	return stm_se_play_stream_set_interval(DvbStream->Handle,
					       PlayInterval->start,
					       PlayInterval->end);
}

/*}}}*/
/*{{{  DvbStreamStep*/
int DvbStreamStep(struct DvbStreamContext_s *DvbStream)
{
	if ((DvbStream == NULL) || (DvbStream->Handle == NULL))	/* No stream to step */
		return -EINVAL;

	return stm_se_play_stream_step(DvbStream->Handle);
}

/*}}}*/
/*{{{  DvbStreamDiscontinuity*/
int DvbStreamDiscontinuity(struct DvbStreamContext_s *DvbStream,
			   dvb_discontinuity_t Discontinuity)
{
	if ((DvbStream == NULL) || (DvbStream->Handle == NULL))
		return -EINVAL;

	return stm_se_play_stream_inject_discontinuity(DvbStream->Handle,
						       (Discontinuity &
							DVB_DISCONTINUITY_CONTINUOUS_REVERSE)
						       != 0,
						       (Discontinuity &	DVB_DISCONTINUITY_SURPLUS_DATA) != 0,
								(Discontinuity & DVB_DISCONTINUITY_EOS) != 0

	);
}

/*}}}*/
/*{{{  DvbStreamSwitch*/
int DvbStreamSwitch(struct DvbStreamContext_s *DvbStream,
		    stm_se_stream_encoding_t Encoding)
{
	sigset_t newsigs;
	sigset_t oldsigs;
	int ret = 0;

	BACKEND_DEBUG("%p\n", DvbStream);

	if ((DvbStream == NULL) || (DvbStream->Handle == NULL))	/* No stream */
		return -EINVAL;

	/* a signal received in here can cause issues. Turn them off, just for this bit... */
	sigfillset(&newsigs);
	sigprocmask(SIG_BLOCK, &newsigs, &oldsigs);

	ret = stm_se_play_stream_switch(DvbStream->Handle, Encoding);

	sigprocmask(SIG_SETMASK, &oldsigs, NULL);
	return ret;
}

/*}}}*/
/*{{{  DvbStreamChannelSelect*/
int DvbStreamChannelSelect(struct DvbStreamContext_s *DvbStream,
			   stm_se_dual_mode_t Channel)
{
	DVB_DEBUG("DualMono Channel Selection= %d \n", Channel);

	if ((DvbStream == NULL) || (DvbStream->Handle == NULL))	/* No stream to set id */
		return -EINVAL;

	return stm_se_play_stream_set_control(DvbStream->Handle,
					      STM_SE_CTRL_DUALMONO,
					      (stm_se_dual_mode_t) Channel);

}
/*}}}*/
/*{{{ DvbStreamDrivenDualMono*/
int DvbStreamDrivenDualMono(struct DvbStreamContext_s *DvbStream,
			    unsigned int Apply)
{
	DVB_DEBUG("DualMono Stream Driven = %d \n", Apply);

	if ((DvbStream == NULL) || (DvbStream->Handle == NULL)) /* No stream to set id */
		return -EINVAL;

	return stm_se_play_stream_set_control(DvbStream->Handle,
					      STM_SE_CTRL_STREAM_DRIVEN_DUALMONO,
					      Apply);

}
/*}}}*/
/*{{{ DvbStreamDownmix*/
int DvbStreamDownmix(struct DvbStreamContext_s *DvbStream,
                     stm_se_audio_channel_assignment_t *channelAssignment)
{
    DVB_DEBUG("Stream Downmix configuration: pair0=%d, pair1=%d, pair2=%d, pair3=%d, pair4=%d, malleable=%d \n",
              channelAssignment->pair0, channelAssignment->pair1, channelAssignment->pair2,
              channelAssignment->pair3, channelAssignment->pair4, channelAssignment->malleable);

    if ((DvbStream == NULL) || (DvbStream->Handle == NULL)) /* No stream to set id */
	return -EINVAL;

    return stm_se_play_stream_set_compound_control(DvbStream->Handle,
                                                   STM_SE_CTRL_SPEAKER_CONFIG,
                                                   (void *) channelAssignment);
}
/*}}}*/
/*{{{ DvbStreamDrivenStereo*/
int DvbStreamDrivenStereo(struct DvbStreamContext_s *DvbStream,
			    unsigned int Apply)
{
	DVB_DEBUG("Stream Driven Stereo = %d \n", Apply);

	if ((DvbStream == NULL) || (DvbStream->Handle == NULL)) /* No stream to set id */
		return -EINVAL;

	return stm_se_play_stream_set_control(DvbStream->Handle,
					      STM_SE_CTRL_STREAM_DRIVEN_STEREO,
					      Apply);

}
/*}}}*/
/*{{{  DvbStreamSetAudioServiceType*/
int DvbStreamSetAudioServiceType(struct DvbStreamContext_s *DvbStream,
				 unsigned int Type)
{
	if ((DvbStream == NULL) || (DvbStream->Handle == NULL))	/* No stream to set id */
		return -EINVAL;

	return stm_se_play_stream_set_control(DvbStream->Handle,
					      STM_SE_CTRL_AUDIO_SERVICE_TYPE,
					      Type);
}

/*}}}*/
/*{{{  DvbStreamSetApplicationType*/
int DvbStreamSetApplicationType(struct DvbStreamContext_s *DvbStream,
				unsigned int Type)
{
	if ((DvbStream == NULL) || (DvbStream->Handle == NULL))	/* No stream to set id */
		return -EINVAL;

	return stm_se_play_stream_set_control(DvbStream->Handle,
					      STM_SE_CTRL_AUDIO_APPLICATION_TYPE,
					      Type);
}

/*}}}*/
/*{{{  DvbStreamSetSubStreamId*/
int DvbStreamSetSubStreamId(struct DvbStreamContext_s *DvbStream,
				unsigned int Id)
{
	if ((DvbStream == NULL) || (DvbStream->Handle == NULL)) /* No stream to set id */
		return -EINVAL;

	return stm_se_play_stream_set_control(DvbStream->Handle,
			STM_SE_CTRL_AUDIO_SUBSTREAM_ID,
			Id);
}


/*}}}*/
/*{{{  DvbStreamSetAacDecoderConfig*/
int DvbStreamSetAacDecoderConfig(struct DvbStreamContext_s *DvbStream,
				 void *Value)
{
	if ((DvbStream == NULL) || (DvbStream->Handle == NULL)) {	/* No stream to set id */
		DVB_DEBUG("No stream to set AAC Config\n");
		return -EINVAL;
	}
	return stm_se_play_stream_set_compound_control(DvbStream->Handle,
						       STM_SE_CTRL_AAC_DECODER_CONFIG,
						       Value);
}

/*}}}*/
/*{{{  DvbStreamSetRegionType */
int DvbStreamSetRegionType(struct DvbStreamContext_s *DvbStream,
				unsigned int Region)
{
	if ((DvbStream == NULL) || (DvbStream->Handle == NULL))	/* No stream to set id */
		return -EINVAL;

	return stm_se_play_stream_set_control(DvbStream->Handle,
					      STM_SE_CTRL_REGION,
					      Region);
}
/*}}}*/
/*{{{  DvbStreamSetProgramReferenceLevel */
int DvbStreamSetProgramReferenceLevel(struct DvbStreamContext_s *DvbStream,
				      unsigned int prl)
{
	if ((DvbStream == NULL) || (DvbStream->Handle == NULL))	/* No stream to set id */
		return -EINVAL;

	return stm_se_play_stream_set_control(DvbStream->Handle,
	                                     STM_SE_CTRL_AUDIO_PROGRAM_REFERENCE_LEVEL,
					     prl);
}

/*}}}*/
/*{{{  DvbStreamIdentifyAudio*/
/*
   If the first three bytes of Header data contain a MPEG start code (0, 0, 1) then
   this function will use only the first four bytes of the header, otherwise this
   function assumes that there are at least 24 bytes of Header data to use to
   determine the type of the stream. It is the callers responsiblity to
   guarantee this (since we don't have a length argument).
*/
int DvbStreamIdentifyAudio(struct DvbStreamContext_s *DvbStream,
			   unsigned int *Id)
{
	int Status = 0;
	unsigned char *Header = DvbStream->Buffer;

	*Id = AUDIO_ENCODING_NONE;
	/* first check for PES start code */
	if ((Header[0] == 0x00) && (Header[1] == 0x00) && (Header[2] == 0x01)) {
		if (IS_PES_START_CODE_AUDIO(Header[3]))
			/* TODO: need to automagically detect MPEG layer (e.g. 0xfff vs. 0xffe) */
			*Id = AUDIO_ENCODING_MPEG1;
		else if (IS_PES_START_CODE_PRIVATE_STREAM_1(Header[3])) {
			/* find the length of the PES header */
			unsigned char PesHeaderDataLength = Header[8];
			if (PesHeaderDataLength > 15) {
				BACKEND_ERROR
				    ("PES header data length is too long (%2x)\n",
				     PesHeaderDataLength);
				Status = -EINVAL;
			} else {
				/* extract the sub-stream identifier */
				unsigned char SubStreamIdentifier =
				    Header[9 + PesHeaderDataLength];

				if (IS_PRIVATE_STREAM_1_AC3
				    (SubStreamIdentifier))
					*Id = AUDIO_ENCODING_AC3;
				else if (IS_PRIVATE_STREAM_1_DTS
					 (SubStreamIdentifier))
					*Id = AUDIO_ENCODING_DTS;
				else if (IS_PRIVATE_STREAM_1_LPCM
					 (SubStreamIdentifier))
					*Id = AUDIO_ENCODING_LPCM;
				else if (IS_PRIVATE_STREAM_1_SDDS
					 (SubStreamIdentifier)) {
					BACKEND_ERROR
					    ("Cannot decode SDDS audio\n");
					Status = -EINVAL;
				} else {
					BACKEND_ERROR
					    ("Unexpected sub stream identifier in private data stream (%2x)\n",
					     SubStreamIdentifier);
					Status = -EINVAL;
				}
			}
		} else {
			BACKEND_ERROR
			    ("Failed to determine PES data encoding (PES hdr 00 00 01 %02x)\n",
			     Header[3]);
			Status = -EINVAL;
		}
	} else if (memcmp(Header, ASFHeaderObjectGuid, 16) == 0) {
		*Id = AUDIO_ENCODING_WMA;
		Status = -EINVAL;
	} else {
		BACKEND_ERROR
		    ("Cannot identify Unknown stream format %02x %02x %02x %02x %02x %02x %02x %02x\n",
		     Header[0], Header[1], Header[2], Header[3], Header[4],
		     Header[5], Header[6], Header[7]);
		Status = -EINVAL;
	}

	return Status;
}

/*}}}*/
/*{{{  DvbStreamIdentifyVideo*/
int DvbStreamIdentifyVideo(struct DvbStreamContext_s *DvbStream,
			   unsigned int *Id)
{
	int Status = 0;
	unsigned char *Header = DvbStream->Buffer;

	*Id = VIDEO_ENCODING_NONE;
	/* check for PES start code */
	if ((Header[0] == 0x00) && (Header[1] == 0x00) && (Header[2] == 0x01)) {
		/*if (IS_PES_START_CODE_VIDEO(Header[3])) */
		*Id = VIDEO_ENCODING_MPEG2;
	} else {
		*Id = VIDEO_ENCODING_MPEG2;
		/*
		   BACKEND_ERROR("Cannot identify Unknown stream format %02x %02x %02x %02x %02x %02x %02x %02x\n",
		   Header[0], Header[1], Header[2], Header[3], Header[4], Header[5], Header[6], Header[7]);
		   Status  = -EINVAL;
		 */
	}

	return Status;
}

/*}}}*/
/*{{{  DvbStreamGetFirstBuffer*/
int DvbStreamGetFirstBuffer(struct DvbStreamContext_s *DvbStream,
			    const char __user * Buffer, unsigned int Length)
{
	int CopyAmount;
	int ret;

	mutex_lock(&(DvbStream->Lock));
	CopyAmount = DvbStream->BufferLength;
	if (CopyAmount >= Length)
		CopyAmount = Length;

	ret = copy_from_user(DvbStream->Buffer, Buffer, CopyAmount);
	if (ret > 0) {
		CopyAmount -= ret;
	} else if (ret < 0) {
		CopyAmount = -EFAULT;
	}

	mutex_unlock(&(DvbStream->Lock));

	return CopyAmount;

}

/*}}}*/

/*{{{  DvbEventSubscribe*/
int DvbEventSubscribe(struct DvbStreamContext_s *DvbStream,
		      void *PrivateData,
		      uint32_t event_mask,
		      stm_event_handler_t callback,
		      stm_event_subscription_h * event_subscription)
{
	int Result = 0;
	stm_event_subscription_entry_t event_entry;

	if ((DvbStream == NULL) || (DvbStream->Handle == NULL))
		return -EINVAL;

	event_entry.object = (stm_object_h) DvbStream->Handle;
	event_entry.event_mask = event_mask;
	event_entry.cookie = PrivateData;

	/* request only one subscription at a time */
	Result =
	    stm_event_subscription_create(&event_entry, 1, event_subscription);
	if (Result < 0) {
		*event_subscription = NULL;
		BACKEND_ERROR
		    ("Failed to Create Event Subscription: Stream %x \n",
		     (uint32_t) DvbStream->Handle);
		return -EINVAL;
	}

	/* set the call back function for subscribed events */
	Result = stm_event_set_handler(*event_subscription, callback);

	return Result;
}

/*}}}*/

/*{{{  DvbEventUnSubscribe*/
int DvbEventUnsubscribe(struct DvbStreamContext_s *DvbStream,
			stm_event_subscription_h event_subscription)
{
	int Result = 0;

	if ((DvbStream == NULL) || (DvbStream->Handle == NULL))
		return -EINVAL;

	/* Remove event subscription */
	Result = stm_event_subscription_delete(event_subscription);
	if (Result < 0) {
		BACKEND_ERROR
		    ("Failed to delete Event Subscription: Stream %x \n",
		     (uint32_t) DvbStream->Handle);
		return -EINVAL;
	}

	return Result;
}

/*}}}*/

/*{{{  DvbStreamPollMessage */
int DvbStreamPollMessage(struct DvbStreamContext_s *DvbStream,
			 stm_se_play_stream_msg_id_t id,
			 stm_se_play_stream_msg_t * message)
{
	int Result = 0;
	if ((DvbStream == NULL) || (DvbStream->Handle == NULL))
		return -EINVAL;

	Result =
	    stm_se_play_stream_poll_message(DvbStream->Handle, id, message);

	return Result;
}

/*}}}*/

/*{{{  DvbStreamGetMessage */
int DvbStreamGetMessage(struct DvbStreamContext_s *DvbStream,
			stm_se_play_stream_subscription_h subscription,
			stm_se_play_stream_msg_t * message)
{
	int Result = 0;
	if ((DvbStream == NULL) || (DvbStream->Handle == NULL))
		return -EINVAL;

	Result =
	    stm_se_play_stream_get_message(DvbStream->Handle, subscription,
					   message);

	return Result;
}

/*}}}*/

/*{{{  DvbStreamSubscribe */
int DvbStreamMessageSubscribe(struct DvbStreamContext_s *DvbStream,
			      uint32_t message_mask,
			      uint32_t depth,
			      stm_se_play_stream_subscription_h * subscription)
{
	int Result = 0;
	if ((DvbStream == NULL) || (DvbStream->Handle == NULL))
		return -EINVAL;

	Result =
	    stm_se_play_stream_subscribe(DvbStream->Handle, message_mask, depth,
					 subscription);
	if (Result < 0) {
		BACKEND_ERROR
		    ("Failed to create message Subscription: Stream %x \n",
		     (uint32_t) DvbStream->Handle);
	}

	return Result;
}

/*}}}*/

/*{{{  DvbStreamMessageUnsubscribe */
int DvbStreamMessageUnsubscribe(struct DvbStreamContext_s *DvbStream,
				stm_se_play_stream_subscription_h subscription)
{
	int Result = 0;
	if ((DvbStream == NULL) || (DvbStream->Handle == NULL))
		return -EINVAL;

	Result =
	    stm_se_play_stream_unsubscribe(DvbStream->Handle, subscription);
	if (Result < 0) {
		BACKEND_ERROR
		    ("Failed to delete message Subscription: Stream %x \n",
		     (uint32_t) DvbStream->Handle);
	}

	return Result;
}

/*}}}*/

int DvbPlaybackInitOption(struct DvbPlaybackContext_s *Playback, unsigned int * PlayOption, unsigned int * PlayValue)
{
	int ret = 0;
	int i;

	const unsigned int OptionsToCheck[] = {
		DVB_OPTION_MASTER_CLOCK,
		DVB_OPTION_CTRL_VIDEO_MEMORY_PROFILE,
		DVB_OPTION_DECIMATE_DECODER_OUTPUT,
	};

	if (Playback == NULL)
		return -ENODEV;

	for (i = 0; i < ARRAY_SIZE(OptionsToCheck); ++i) {
		if (DVB_OPTION_VALUE_INVALID != PlayOption[ OptionsToCheck[i] ]) {
			ret = DvbPlaybackSetOption(Playback,
			    OptionsToCheck[i], PlayValue[ OptionsToCheck[i] ]);

			if (!ret) {
				/* reset the flag to indicate it has been set succesfully */
				PlayOption[DVB_OPTION_MASTER_CLOCK] = DVB_OPTION_VALUE_INVALID;
			}
			/* keep applying options even if one has an error */
		}
	}

	return 0;
}
