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

Source file name : stm_dvb.h

Extensions to the LinuxDVB API (v3)

Date        Modification                                    Name
----        ------------                                    --------

 ************************************************************************/

#ifndef H_STM_DVB
#define H_STM_DVB

/*
 * Whenever a sequence of values is extended (define or enum) always add the new values
 * So that old values are unchange to maintain binary compatibility.
 */

#define DVB_SPEED_NORMAL_PLAY           1000
#define DVB_SPEED_STOPPED               0
#define DVB_SPEED_REVERSE_STOPPED       0x80000000
#define DVB_FRAME_RATE_MULTIPLIER       1000

#define DMX_FILTER_BY_PRIORITY_LOW      0x00010000	/* These flags tell the transport pes filter whether to filter */
#define DMX_FILTER_BY_PRIORITY_HIGH     0x00020000	/* using the ts priority bit and, if so, whether to filter on */
#define DMX_FILTER_BY_PRIORITY_MASK     0x00030000	/* bit set or bit clear */

/*
 * List of possible container types - used to select demux..  If stream_source is VIDEO_SOURCE_DEMUX
 * then default is TRANSPORT, if stream_source is VIDEO_SOURCE_MEMORY then default is PES
 */
typedef enum {
	STREAM_TYPE_NONE,	/* Deprecated */
	STREAM_TYPE_TRANSPORT,	/* Use latest PTI driver so it can be Deprecated */
	STREAM_TYPE_PES,
	STREAM_TYPE_ES,		/* Deprecated */
	STREAM_TYPE_PROGRAM,	/* Deprecated */
	STREAM_TYPE_SYSTEM,	/* Deprecated */
	STREAM_TYPE_SPU,	/* Deprecated */
	STREAM_TYPE_NAVI,	/* Deprecated */
	STREAM_TYPE_CSS,	/* Deprecated */
	STREAM_TYPE_AVI,	/* Deprecated */
	STREAM_TYPE_MP3,	/* Deprecated */
	STREAM_TYPE_H264,	/* Deprecated */
	STREAM_TYPE_ASF,	/* Needs work so it can be deprecated */
	STREAM_TYPE_MP4,	/* Deprecated */
	STREAM_TYPE_RAW,	/* Deprecated */
} stream_type_t;

typedef enum {
	DVB_DISCONTINUITY_SKIP = 0x01,
	DVB_DISCONTINUITY_CONTINUOUS_REVERSE = 0x02,
	DVB_DISCONTINUITY_SURPLUS_DATA = 0x04,
	DVB_DISCONTINUITY_EOS = 0x08
} dvb_discontinuity_t;

#define DVB_TIME_NOT_BOUNDED            0xfedcba9876543210ULL

typedef struct dvb_play_interval_s {
	unsigned long long start;
	unsigned long long end;
} dvb_play_interval_t;

typedef struct dvb_play_time_s {
	unsigned long long system_time;
	unsigned long long presentation_time;
	unsigned long long pts;
} dvb_play_time_t;

typedef struct dvb_play_info_s {
	unsigned long long system_time;
	unsigned long long presentation_time;
	unsigned long long pts;
	unsigned long long frame_count;
} dvb_play_info_t;

typedef enum {
	DVB_CLOCK_FORMAT_US = 0x00,
	DVB_CLOCK_FORMAT_PTS = 0x01,
	DVB_CLOCK_FORMAT_27MHz = 0x02,
	DVB_CLOCK_FORMAT_MASK = 0xff,
	DVB_CLOCK_INITIALIZE = 0x100
} dvb_clock_flags_t;

typedef struct dvb_clock_data_point_s {
	dvb_clock_flags_t flags;
	unsigned long long source_time;
	unsigned long long system_time;
} dvb_clock_data_point_t;

typedef struct dvb_time_mapping_s {
	unsigned long long native_stream_time;
	unsigned long long system_presentation_time;
} dvb_time_mapping_t;

#endif /* H_STM_DVB */
