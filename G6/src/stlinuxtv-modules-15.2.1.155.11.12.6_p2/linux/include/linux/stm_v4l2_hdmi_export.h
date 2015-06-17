/************************************************************************
 * Copyright (C) 2014 STMicroelectronics. All Rights Reserved.
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
 * Definitions for STM V4L2 HDMI controls
 *************************************************************************/
#ifndef __STM_V4L2_HDMI_EXPORT_H__
#define __STM_V4L2_HDMI_EXPORT_H__

#include "linux/dvb/dvb_v4l2_export.h"

/*
 * @codec             : encoding type (enum stm_v4l2_mpeg_audio_encoding)
 * @channel_count     : number of channels in the incoming signal
 * @sampling_frequency: sampling frequency (enum v4l2_audio_encode_pcm_sampling_freq)
 */
struct stm_v4l2_audio_fmt {
	__u32 codec;
	__u8 channel_count;
	__u32 sampling_frequency;
};

struct stm_v4l2_subdev_audio_fmt {
	__u32 which;
	__u32 pad;
	struct stm_v4l2_audio_fmt fmt;
	__u32 reserved[32];
};

/*
 * ST specific ioctls
 */
#define VIDIOC_SUBDEV_STI_G_AUDIO_FMT	\
		_IOR('V', BASE_VIDIOC_PRIVATE + 6, struct stm_v4l2_subdev_audio_fmt)


/*
 * ST specific HDMI (Rx/Tx) events
 * @V4L2_EVENT_STI_HOTPLUG : This event comes from hdmirx subdev whenever, the plug
 *                           status of the port changes.
 */
#define V4L2_EVENT_CLASS_DV			(V4L2_EVENT_PRIVATE_START + 1 * 1000)
#define V4L2_EVENT_STI_HOTPLUG			(V4L2_EVENT_CLASS_DV + 1)
#define V4L2_EVENT_STI_HDMI_SIG			(V4L2_EVENT_CLASS_DV + 2)

#endif
