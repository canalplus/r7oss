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
 * Implementation of common V4L2-MediaController registration functions
************************************************************************/

#ifndef __STMEDIA_EXPORT_H__
#define __STMEDIA_EXPORT_H__

#include <linux/media.h>

/* ST specific V4L2 Subdevice types
 * Only use up 8 bits to keep space for new standard V4L2 subdev types */
#define MEDIA_ENT_T_V4L2_SUBDEV_ST	(MEDIA_ENT_T_V4L2_SUBDEV + 256)

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0))
#define MEDIA_ENT_T_V4L2_SUBDEV_DECODER		(MEDIA_ENT_T_V4L2_SUBDEV_ST)
#endif

#define MEDIA_ENT_T_V4L2_SUBDEV_PLANE_GRAPHIC	(MEDIA_ENT_T_V4L2_SUBDEV_ST + 1)
#define MEDIA_ENT_T_V4L2_SUBDEV_PLANE_VIDEO	(MEDIA_ENT_T_V4L2_SUBDEV_ST + 2)
#define MEDIA_ENT_T_V4L2_SUBDEV_PLANE_VBI	(MEDIA_ENT_T_V4L2_SUBDEV_ST + 3)
#define MEDIA_ENT_T_V4L2_SUBDEV_OUTPUT	(MEDIA_ENT_T_V4L2_SUBDEV_ST + 4)
#define MEDIA_ENT_T_DVB_SUBDEV_AUDIO_DECODER	(MEDIA_ENT_T_V4L2_SUBDEV_ST + 5)
#define MEDIA_ENT_T_DVB_SUBDEV_VIDEO_DECODER	(MEDIA_ENT_T_V4L2_SUBDEV_ST + 6)
#define MEDIA_ENT_T_V4L2_SUBDEV_AUDIO_DECODER	(MEDIA_ENT_T_V4L2_SUBDEV_ST + 7)
#define MEDIA_ENT_T_V4L2_SUBDEV_VIDEO_DECODER	(MEDIA_ENT_T_V4L2_SUBDEV_ST + 8)
#define MEDIA_ENT_T_V4L2_SUBDEV_AUDIO_ENCODER	(MEDIA_ENT_T_V4L2_SUBDEV_ST + 9)
#define MEDIA_ENT_T_V4L2_SUBDEV_VIDEO_ENCODER	(MEDIA_ENT_T_V4L2_SUBDEV_ST + 10)
#define MEDIA_ENT_T_V4L2_SUBDEV_HDMIRX	(MEDIA_ENT_T_V4L2_SUBDEV_ST + 11)
#define MEDIA_ENT_T_V4L2_SUBDEV_VIDEXTIN_HDMIRX	(MEDIA_ENT_T_V4L2_SUBDEV_ST + 12)
#define MEDIA_ENT_T_V4L2_SUBDEV_VXI	(MEDIA_ENT_T_V4L2_SUBDEV_ST + 13)
#define MEDIA_ENT_T_V4L2_SUBDEV_VIDEXTIN_VXI	(MEDIA_ENT_T_V4L2_SUBDEV_ST + 14)
#define MEDIA_ENT_T_V4L2_SUBDEV_TSMUXER	(MEDIA_ENT_T_V4L2_SUBDEV_ST + 15)
#define MEDIA_ENT_T_V4L2_SUBDEV_DVP		(MEDIA_ENT_T_V4L2_SUBDEV_ST + 16)
#define MEDIA_ENT_T_V4L2_SUBDEV_COMPO		(MEDIA_ENT_T_V4L2_SUBDEV_ST + 17)

/* ST specific ALSA Subdevice types */
#define MEDIA_ENT_T_ALSA_SUBDEV			(3 << MEDIA_ENT_TYPE_SHIFT)
#define MEDIA_ENT_T_ALSA_SUBDEV_MIXER		(MEDIA_ENT_T_ALSA_SUBDEV + 1)
#define MEDIA_ENT_T_ALSA_SUBDEV_READER		(MEDIA_ENT_T_ALSA_SUBDEV + 2)
#define MEDIA_ENT_T_ALSA_SUBDEV_PLAYER		(MEDIA_ENT_T_ALSA_SUBDEV + 3)

/* ALSA entities PADs */
enum {
	STM_SND_MIXER_PAD_PRIMARY,
	STM_SND_MIXER_PAD_SECONDARY,
	STM_SND_MIXER_PAD_AUX2,
	STM_SND_MIXER_PAD_AUX3,
	STM_SND_MIXER_PAD_OUTPUT,
	STM_SND_MIXER_PAD_COUNT
};

enum {
	STM_SND_READER_PAD_SINK,
	STM_SND_READER_PAD_SOURCE,
	STM_SND_READER_PAD_COUNT
};

enum {
	STM_SND_PLAYER_PAD_SINK,
	STM_SND_PLAYER_PAD_SOURCE,
	STM_SND_PLAYER_PAD_COUNT
};

/*
 * ST Proprietary V4L2 Buffer types
 */
#define V4L2_BUF_TYPE_PRIVATE_OFFSET_AUDIO_CAPTURE   3
#define V4L2_BUF_TYPE_AUDIO_CAPTURE                 ((enum v4l2_buf_type) (V4L2_BUF_TYPE_PRIVATE + V4L2_BUF_TYPE_PRIVATE_OFFSET_AUDIO_CAPTURE))
#define V4L2_BUF_TYPE_PRIVATE_OFFSET_USER_DATA_CAPTURE	4
#define V4L2_BUF_TYPE_USER_DATA_CAPTURE                 ((enum v4l2_buf_type) (V4L2_BUF_TYPE_PRIVATE + V4L2_BUF_TYPE_PRIVATE_OFFSET_USER_DATA_CAPTURE))


/* device names being used and shared across */
#define STM_HDMIRX_SUBDEV "hdmirx"
#define STM_VXI_SUBDEV "vxi"

#endif
