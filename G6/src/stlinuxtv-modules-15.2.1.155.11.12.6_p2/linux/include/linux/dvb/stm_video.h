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
with STLinuxTV; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The STLinuxTV Library may alternatively be licensed under a proprietary
license from ST.

Source file name : stm_video.h

Extensions to the LinuxDVB API (v3)

Date        Modification                                    Name
----        ------------                                    --------

 ************************************************************************/

#ifndef H_STM_VIDEO
#define H_STM_VIDEO

#include <linux/dvb/stm_dvb_option.h>

/*
 * Whenever a sequence of values is extended (define or enum) always add the new values
 * So that old values are unchange to maintain binary compatibility.
 */

#define VIDEO_FULL_SCREEN               (VIDEO_CENTER_CUT_OUT+1)

/*
 * Extra events
 */

#define VIDEO_EVENT_FIRST_FRAME_ON_DISPLAY      5	/*(VIDEO_EVENT_VSYNC+1) */
#define VIDEO_EVENT_FRAME_DECODED_LATE          (VIDEO_EVENT_FIRST_FRAME_ON_DISPLAY+1)
#define VIDEO_EVENT_DATA_DELIVERED_LATE         (VIDEO_EVENT_FRAME_DECODED_LATE+1)
#define VIDEO_EVENT_STREAM_UNPLAYABLE           (VIDEO_EVENT_DATA_DELIVERED_LATE+1)
#define VIDEO_EVENT_TRICK_MODE_CHANGE           (VIDEO_EVENT_STREAM_UNPLAYABLE+1)
#define VIDEO_EVENT_VSYNC_OFFSET_MEASURED       (VIDEO_EVENT_TRICK_MODE_CHANGE+1)
#define VIDEO_EVENT_FATAL_ERROR                 (VIDEO_EVENT_VSYNC_OFFSET_MEASURED+1)
#define VIDEO_EVENT_OUTPUT_SIZE_CHANGED         (VIDEO_EVENT_FATAL_ERROR+1)
#define VIDEO_EVENT_FATAL_HARDWARE_FAILURE      (VIDEO_EVENT_OUTPUT_SIZE_CHANGED+1)
#define VIDEO_EVENT_FRAME_DECODED               (VIDEO_EVENT_FATAL_HARDWARE_FAILURE+1)
#define VIDEO_EVENT_FRAME_RENDERED              (VIDEO_EVENT_FRAME_DECODED+1)
#define VIDEO_EVENT_STREAM_IN_SYNC              (VIDEO_EVENT_FRAME_RENDERED+1)
#define VIDEO_EVENT_LOST                        (VIDEO_EVENT_STREAM_IN_SYNC+1)
#define VIDEO_EVENT_END_OF_STREAM               (VIDEO_EVENT_LOST+1)
#define VIDEO_EVENT_FRAME_STARVATION            (VIDEO_EVENT_END_OF_STREAM+1)
#define VIDEO_EVENT_FRAME_SUPPLIED              (VIDEO_EVENT_FRAME_STARVATION+1)

/*
 * List of possible video encodings - used to select frame parser and codec.
 */
typedef enum {
	VIDEO_ENCODING_AUTO,
	VIDEO_ENCODING_MPEG1,
	VIDEO_ENCODING_MPEG2,
	VIDEO_ENCODING_MJPEG,
	VIDEO_ENCODING_DIVX3,
	VIDEO_ENCODING_DIVX4,
	VIDEO_ENCODING_DIVX5,
	VIDEO_ENCODING_MPEG4P2,
	VIDEO_ENCODING_H264,
	VIDEO_ENCODING_WMV,
	VIDEO_ENCODING_VC1,
	VIDEO_ENCODING_RAW,
	VIDEO_ENCODING_H263,
	VIDEO_ENCODING_FLV1,
	VIDEO_ENCODING_VP6,
	VIDEO_ENCODING_RMV,
	VIDEO_ENCODING_DIVXHD,
	VIDEO_ENCODING_AVS,
	VIDEO_ENCODING_VP3,
	VIDEO_ENCODING_THEORA,
	VIDEO_ENCODING_COMPOCAP,
	VIDEO_ENCODING_VP8,
	VIDEO_ENCODING_MVC,
	VIDEO_ENCODING_HEVC,
	VIDEO_ENCODING_VC1_RP227SPMP,
	VIDEO_ENCODING_NONE,
	VIDEO_ENCODING_PRIVATE,
	VIDEO_ENCODING_USER_ALLOCATED_FRAMES = 0x10000000
} video_encoding_t;

typedef struct {
	int x;
	int y;
	int width;
	int height;
} video_window_t;

/*
 * video discontinuity
 */
typedef enum {
	VIDEO_DISCONTINUITY_SKIP = DVB_DISCONTINUITY_SKIP,
	VIDEO_DISCONTINUITY_CONTINUOUS_REVERSE =
	    DVB_DISCONTINUITY_CONTINUOUS_REVERSE,
	VIDEO_DISCONTINUITY_SURPLUS_DATA = DVB_DISCONTINUITY_SURPLUS_DATA,
	VIDEO_DISCONTINUITY_EOS = DVB_DISCONTINUITY_EOS,
} video_discontinuity_t;

typedef dvb_play_interval_t video_play_interval_t;
typedef dvb_play_time_t video_play_time_t;
typedef dvb_play_info_t video_play_info_t;
typedef dvb_clock_data_point_t video_clock_data_point_t;
typedef dvb_time_mapping_t video_time_mapping_t;

/* Legacy typo correction */
#define DVP_OPTION_H264_FORCE_PIC_ORDER_CNT_IGNORE_DPB_DISPLAY_FRAME_ORDERING DVB_OPTION_H264_FORCE_PIC_ORDER_CNT_IGNORE_DPB_DISPLAY_FRAME_ORDERING

typedef dvb_option_t video_option_t;

/* Decoder commands */
#define VIDEO_CMD_PLAY                  (0)
#define VIDEO_CMD_STOP                  (1)
#define VIDEO_CMD_FREEZE                (2)
#define VIDEO_CMD_CONTINUE              (3)
#define VIDEO_CMD_SET_OPTION            (4)
#define VIDEO_CMD_GET_OPTION            (5)

/* Flags for VIDEO_CMD_FREEZE */
#define VIDEO_CMD_FREEZE_TO_BLACK       (1 << 0)

/* Flags for VIDEO_CMD_STOP */
#define VIDEO_CMD_STOP_TO_BLACK         (1 << 0)
#define VIDEO_CMD_STOP_IMMEDIATELY      (1 << 1)

/* Play input formats: */
/* The decoder has no special format requirements */
#define VIDEO_PLAY_FMT_NONE         (0)
/* The decoder requires full GOPs */
#define VIDEO_PLAY_FMT_GOP          (1)

/* ST specific video ioctls */
#define VIDEO_SET_ENCODING              _IO('o',  81)
#define VIDEO_FLUSH                     _IO('o',  82)
#define VIDEO_SET_SPEED                 _IO('o',  83)
#define VIDEO_DISCONTINUITY             _IO('o',  84)
#define VIDEO_STEP                      _IO('o',  85)
#define VIDEO_SET_PLAY_INTERVAL         _IOW('o', 86, video_play_interval_t)
#define VIDEO_SYNC_GROUP_MASK	0xC0
#define VIDEO_SYNC_GROUP_DEMUX	0x40
#define VIDEO_SYNC_GROUP_AUDIO	0x80
#define VIDEO_SYNC_GROUP_VIDEO	0xC0
#define VIDEO_SET_SYNC_GROUP            _IO('o',  87)
#define VIDEO_GET_PLAY_TIME             _IOR('o', 88, video_play_time_t)
#define VIDEO_GET_PLAY_INFO             _IOR('o', 89, video_play_info_t)
#define VIDEO_SET_CLOCK_DATA_POINT      _IOW('o', 90, video_clock_data_point_t)
#define VIDEO_SET_TIME_MAPPING          _IOW('o', 91, video_time_mapping_t)
#define VIDEO_GET_CLOCK_DATA_POINT      _IOR('o', 92, video_clock_data_point_t)

#endif /* H_DVB_STM_H */
