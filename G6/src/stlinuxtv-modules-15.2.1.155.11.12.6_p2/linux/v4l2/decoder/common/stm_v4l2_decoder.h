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
 * Header of v4l2 audio decoder subdev
************************************************************************/
#ifndef __STM_V4L2_DECODER_H__
#define __STM_V4L2_DECODER_H__

#include <media/v4l2-subdev.h>

#include "stm_common.h"
#include "stm_se/types.h"
#include "stm_se/controls.h"
#include "stm_se/playback.h"
#include "stm_se/play_stream.h"

#include "stmedia.h"

#define V4L2_MAX_PLAYBACKS	2

struct stm_v4l2_decoder_context
{
	int id;

	struct stmedia_v4l2_subdev stm_sd;
	struct v4l2_ctrl_handler ctrl_handler;
	struct media_pad *pads;

	struct mutex mutex;

	stm_se_play_stream_h play_stream;

	struct stm_v4l2_decoder_playback_context *playback_ctx;

	void *priv;
};

/*
 * We are targeting audio/video content progagation in an AVSYNC manner
 * so, only creating a mutual playback context to use.
 */
struct stm_v4l2_decoder_playback_context
{
	int users;

	struct mutex mutex;

	stm_se_playback_h playback;
};

/*
 * Function declarations common to audio video decoder
 */
struct stm_v4l2_decoder_playback_context *stm_v4l2_decoder_create_playback(u16 id);
void stm_v4l2_decoder_delete_playback(u16 id);
int stm_v4l2_create_links(u32 src_ent, u16 src_pad, char *src_name,
			u32 sink_ent, u16 sink_pad, char *sink_name,
			u32 flags);
#endif
