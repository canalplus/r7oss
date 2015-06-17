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
#ifndef __STM_V4L2_AUDIO_DECODER_H__
#define __STM_V4L2_AUDIO_DECODER_H__

#include "stm_hdmirx.h"

#define AUD_SNK_PAD	0
#define AUD_SRC_PAD	1

int stm_v4l2_audio_decoder_init(void);
void stm_v4l2_audio_decoder_exit(void);
int stm_v4l2_auddec_ctrl_init(struct v4l2_ctrl_handler *ctrl_handler, void *priv);
void stm_v4l2_auddec_ctrl_exit(struct v4l2_ctrl_handler *ctrl_handler);


#define VIDIOC_STI_S_AUDIO_FMT 				\
	_IOW('V', BASE_VIDIOC_PRIVATE + 1,		\
		struct stm_hdmirx_audio_property_s)

#define VIDIOC_STI_S_AUDIO_INPUT_STABLE			\
	_IOW('V', VIDIOC_STI_S_AUDIO_FMT + 1, int )

#endif
