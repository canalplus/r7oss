/************************************************************************
 * Copyright (C) 2013 STMicroelectronics. All Rights Reserved.
 *
 * This file is part of the STLinuxTV Library.
 *
 * STLinuxTV is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation.
 *
 * STLinuxTV is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with player2; see the file COPYING.  If not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * The STLinuxTV Library may alternatively be licensed under a proprietary
 * license from ST.
 * - Implementation of audio decoder connection to mixer
 *************************************************************************/
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/errno.h>
#include <linux/export.h>
#include "dvb_module.h"
#include "stmedia_export.h"
#include "audio_out.h"

#include "pseudocard.h"
#include "pseudo_mixer.h"

/**
 * stm_dvb_audio_connect_mixer
 * This function connects the enabled mixer or audio-player to play stream
 * @src_pad	: Media controller registered audio source media pad.
 * @dst_pad	: Media controller registered mixer media pad.
 * @play_stream	: Data source for the mixer
 */
int stm_dvb_audio_connect_sink(const struct media_pad *src_pad,
				const struct media_pad *dst_pad,
				stm_se_play_stream_h play_stream)
{
	int id = 0, ret = 0;
	bool search_all_sink = false;
	stm_object_h sink = NULL;

	if (!dst_pad)
		search_all_sink = true;
	/*
	 * If the destination(remote) pad passed is NULL, then find all the
	 * available remote connections for the source and connect them.
	 */
	do {
		/*
		 * Find the remote pad
		 */
		if (search_all_sink) {
			dst_pad = stm_media_find_remote_pad(src_pad,
						MEDIA_LNK_FL_ENABLED, &id);

			/*
			 * Exhausted all the remote connections
			 */
			if (!dst_pad)
				break;
		}

		if (dst_pad->entity->type == MEDIA_ENT_T_ALSA_SUBDEV_PLAYER) {
			sink = snd_hw_player_get_from_entity(dst_pad->entity);
			if (!sink) {
				DVB_ERROR("No audio-player found from entity\n");
				ret = -EINVAL;
				continue;
			}
		}
		else if (dst_pad->entity->type == MEDIA_ENT_T_ALSA_SUBDEV_MIXER) {
			sink = snd_pseudo_mixer_get_from_entity(dst_pad->entity);
			if (!sink) {
				DVB_ERROR("No mixer found from entity\n");
				ret = -EINVAL;
				continue;
			}
		}
		else
			continue;


		ret = stm_se_play_stream_attach_to_pad(play_stream, sink,
					STM_SE_PLAY_STREAM_OUTPUT_PORT_DEFAULT,
					dst_pad->index);
		if (ret) {
			DVB_ERROR("Stream(%p) -> Sink(%p) failed\n",
							play_stream, sink);
		}

		DVB_DEBUG("Stream(%p) -> Sink(%p) connected\n",
							play_stream, sink);

	} while(search_all_sink);

	return ret;
}

/**
 * stm_dvb_audio_disconnect_sink
 * This function disconnects enabled mixer to play stream
 * @src_pad	: Media controller registered audio source media pad
 * @dst_pad	: Media controller registered mixer media pad
 * @play_stream	: Data source for the mixer
 */
int stm_dvb_audio_disconnect_sink(const struct media_pad *src_pad,
				const struct media_pad *dst_pad,
				stm_se_play_stream_h play_stream)
{
	int id = 0, ret = 0;
	bool search_all_sink = false;
	stm_object_h sink = NULL;

	/*
	 * If there's no destination pad, find all
	 * enabled mixers and disconnect them.
	 */
	if (!dst_pad)
		search_all_sink = true;

	/*
	 * Start searching the enabled mixer(s)
	 */
	do {
		if (search_all_sink) {
			dst_pad = stm_media_find_remote_pad(src_pad,
						MEDIA_LNK_FL_ENABLED, &id);

			/*
			 * Traversed the whole list, so, go out
			 */
			if (!dst_pad)
				break;
		}

		if (dst_pad->entity->type == MEDIA_ENT_T_ALSA_SUBDEV_MIXER) {
		  sink = snd_pseudo_mixer_get_from_entity(dst_pad->entity);
		  if (!sink) {
		    DVB_ERROR("No mixer found from entity\n");
		    ret = -EINVAL;
		    continue;
		  }
		}
		else if (dst_pad->entity->type == MEDIA_ENT_T_ALSA_SUBDEV_PLAYER) {
		  sink = snd_hw_player_get_from_entity(dst_pad->entity);
		  if (!sink) {
		    DVB_ERROR("No audio-player found from entity\n");
		    ret = -EINVAL;
		    continue;
		  }
		}
		else
		  continue;

		ret = stm_se_play_stream_detach(play_stream, sink);
		if (ret)
			DVB_ERROR("Stream(%p) -X-> Sink(%p) failed\n",
							play_stream, sink);

		DVB_DEBUG("Stream(%p) -X-> Sink(%p) detached\n",
							play_stream, sink);
	} while(search_all_sink);

	return ret;
}

EXPORT_SYMBOL(stm_dvb_audio_connect_sink);
EXPORT_SYMBOL(stm_dvb_audio_disconnect_sink);
