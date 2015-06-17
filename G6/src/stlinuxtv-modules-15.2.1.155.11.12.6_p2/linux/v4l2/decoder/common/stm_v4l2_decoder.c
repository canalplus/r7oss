/************************************************************************
Copyright (C) 2014 STMicroelectronics. All Rights Reserved.

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
 * Implementation of V4L2 Audio Video Decoder
************************************************************************/
#include <linux/init.h>
#include <linux/module.h>

#include "stm_v4l2_decoder.h"
#include "stm_v4l2_video_decoder.h"
#include "stm_v4l2_audio_decoder.h"

struct stm_v4l2_decoder_playback_context *stm_v4l2_playback;

/**
 * stm_v4l2_create_links() - creates disabled links
 * @src_ent  : source entity type
 * @src_pad  : source pad number
 * @src_name : for selective src connection
 * @sink_ent : sink entity type
 * @sink_pad : sink pad number
 * @sink_name: for selective sink connection
 * @flags    : MEDIA_LNK_FL_ENABLED etc.
 * Create links from all sources to all sinks.
 */
int stm_v4l2_create_links(u32 src_ent, u16 src_pad, char *src_name,
			u32 sink_ent, u16 sink_pad, char *sink_name,
			u32 flags)
{
	int ret = 0;
	struct media_entity *src, *sink;

	/*
	 * Create all possible links from all sources to all sinks.
	 * a. Find the source and connect to all sinks
	 * b. Do this for all sources
	 */
	src = stm_media_find_entity_with_type_first(src_ent);
	while (src) {

		/*
		 * Too much indentation required, so, let's goto
		 */
		if (src_name && (strncmp(src->name, src_name, strlen(src_name))))
			goto get_next_source;

		/*
		 * Get ths sink, and connect to all sources
		 */
		sink = stm_media_find_entity_with_type_first(sink_ent);

		while(sink) {

			if (sink_name && strncmp(sink->name, sink_name, strlen(sink_name)))
				goto get_nex_sink;

			ret = media_entity_create_link(src, src_pad,
						sink, sink_pad, flags);
			if (ret) {
				pr_err("%s(): failed to create %s() -> %s()\n",
						 __func__, src->name, sink->name);
				goto link_setup_failed;
			}

			pr_info("%s(): created %s() -> %s()\n", __func__, src->name, sink->name);
get_nex_sink:
			sink = stm_media_find_entity_with_type_next(sink, sink_ent);
		}

get_next_source:
		src = stm_media_find_entity_with_type_next(src, src_ent);
	}

link_setup_failed:
	return ret;
}

/**
 * stm_v4l2_decoder_create_playback() - create playback for decoder
 */
struct stm_v4l2_decoder_playback_context *stm_v4l2_decoder_create_playback(u16 id)
{
	char name[16];
	struct stm_v4l2_decoder_playback_context *playback_ctx;

	if (id > V4L2_MAX_PLAYBACKS) {
		pr_err("%s(): Out of bounds play id\n", __func__);
		goto playback_invalid;
	}

	playback_ctx = &stm_v4l2_playback[id];

	mutex_lock(&playback_ctx->mutex);

	/*
	 * If no playback is created, create one, else do nothing
	 */
	if (!playback_ctx->playback) {

		snprintf(name, sizeof(name), "v4l2.playback%d", id);

		if (stm_se_playback_new(name, &playback_ctx->playback)) {
			pr_err("%s(): failed to create new playback\n", __func__);
			goto playback_new_failed;
		}
	}

	playback_ctx->users++;

	mutex_unlock(&playback_ctx->mutex);

	return playback_ctx;

playback_new_failed:
	mutex_unlock(&playback_ctx->mutex);
playback_invalid:
	return NULL;
}

/**
 * stm_v4l2_decoder_delete_playback() - delete playback for decoder
 */
void stm_v4l2_decoder_delete_playback(u16 id)
{
	struct stm_v4l2_decoder_playback_context *playback_ctx;
	if (id > V4L2_MAX_PLAYBACKS) {
		pr_err("%s(): Out of bounds play id\n", __func__);
		goto playback_invalid;
	}

	playback_ctx = &stm_v4l2_playback[id];

	mutex_lock(&playback_ctx->mutex);

	if (playback_ctx->users == 0) {
		mutex_unlock(&playback_ctx->mutex);
		return;
	}

	if (--playback_ctx->users == 0) {
		stm_se_playback_delete(playback_ctx->playback);
		playback_ctx->playback = NULL;
	}

	mutex_unlock(&playback_ctx->mutex);

playback_invalid:
	return;
}

/**
 * stm_v4l2_decoder_init() - init the V4L2 AV decoders
 */
static int __init stm_v4l2_decoder_init(void)
{
	int ret = 0, i;

	/*
	 * Init video decoder
	 */
	ret = stm_v4l2_video_decoder_init();
	if (ret) {
		pr_err("%s(): v4l2 video decoder init failed\n", __func__);
		goto video_decoder_init_failed;
	}

	/*
	 * Init audio decoder
	 */
	ret = stm_v4l2_audio_decoder_init();
	if (ret) {
		pr_err("%s(): v4l2 audio decoder init failed\n", __func__);
		goto audio_decoder_init_failed;
	}

	/*
	 * Allocate v4l2 playback context
	 */
	stm_v4l2_playback = kzalloc(sizeof(struct stm_v4l2_decoder_playback_context)
					* V4L2_MAX_PLAYBACKS, GFP_KERNEL);
	if (!stm_v4l2_playback) {
		pr_err("%s(): out of memory for v4l2 decode context\n", __func__);
		ret = -ENOMEM;
		goto context_alloc_failed;
	}

	/*
	 * Initialize the playback mutex
	 */
	for(i = 0; i < V4L2_MAX_PLAYBACKS; i++)
		mutex_init(&stm_v4l2_playback[i].mutex);

	return 0;

context_alloc_failed:
	stm_v4l2_audio_decoder_exit();
audio_decoder_init_failed:
	stm_v4l2_video_decoder_exit();
video_decoder_init_failed:
	return ret;
}

/**
 * stm_v4l2_decoder_exit() - exit the V4L2 AV decoders
 */
static void __exit stm_v4l2_decoder_exit(void)
{
	kfree(stm_v4l2_playback);

	/*
	 * Exit video decoder
	 */
	stm_v4l2_video_decoder_exit();

	/*
	 * Exit audio decoder
	 */
	stm_v4l2_audio_decoder_exit();
}

module_init(stm_v4l2_decoder_init);
module_exit(stm_v4l2_decoder_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("STMicroelectronics");
MODULE_DESCRIPTION("ST Microelectronics - V4L2 Audio Video Decoder");
