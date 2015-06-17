/************************************************************************
Copyright (C) 2012 STMicroelectronics. All Rights Reserved.

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
 * Header for v4l2 video decoder subdev
************************************************************************/
#ifndef __STM_V4L2_VIDEO_DECODER_H__
#define __STM_V4L2_VIDEO_DECODER_H__

#define RAW_VID_SNK_PAD		0
#define RAW_VID_SRC_PAD		1

int stm_v4l2_video_decoder_init(void);
void stm_v4l2_video_decoder_exit(void);
int stm_v4l2_rawvid_ctrl_init(struct v4l2_ctrl_handler *ctrl_handler, void *priv);
void stm_v4l2_viddec_ctrl_exit(struct v4l2_ctrl_handler *ctrl_handler);

#endif
