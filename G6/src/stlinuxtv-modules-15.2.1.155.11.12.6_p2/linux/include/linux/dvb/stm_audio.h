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

Source file name : stm_audio.h

Extensions to the LinuxDVB API (v3)

Date        Modification                                    Name
----        ------------                                    --------

 ************************************************************************/

#ifndef H_STM_AUDIO
#define H_STM_AUDIO

#include <linux/dvb/stm_dvb.h>
#include <linux/dvb/stm_dvb_option.h>

/*
 * Whenever a sequence of values is extended (define or enum) always add the new values
 * So that old values are unchange to maintain binary compatibility.
 */

/*
 * List of possible audio encodings - used to select frame parser and codec.
 */
typedef enum {
	AUDIO_ENCODING_AUTO,
	AUDIO_ENCODING_PCM,
	AUDIO_ENCODING_LPCM,
	AUDIO_ENCODING_MPEG1,
	AUDIO_ENCODING_MPEG2,
	AUDIO_ENCODING_MP3,
	AUDIO_ENCODING_AC3,
	AUDIO_ENCODING_DTS,
	AUDIO_ENCODING_AAC,
	AUDIO_ENCODING_WMA,
	AUDIO_ENCODING_RAW,
	AUDIO_ENCODING_LPCMA,
	AUDIO_ENCODING_LPCMH,
	AUDIO_ENCODING_LPCMB,
	AUDIO_ENCODING_SPDIF,	/*<! Data coming through SPDIF link :: compressed or PCM data */
	AUDIO_ENCODING_DTS_LBR,
	AUDIO_ENCODING_MLP,
	AUDIO_ENCODING_RMA,
	AUDIO_ENCODING_AVS,
	AUDIO_ENCODING_VORBIS,
	AUDIO_ENCODING_FLAC,
	AUDIO_ENCODING_DRA,
	AUDIO_ENCODING_NONE,
	AUDIO_ENCODING_MS_ADPCM,
	AUDIO_ENCODING_IMA_ADPCM,
	AUDIO_ENCODING_PRIVATE
} audio_encoding_t;

/*
 * List of possible sources for SP/DIF output.
 */
typedef enum audio_spdif_source {
	AUDIO_SPDIF_SOURCE_PP,	/*<! normal decoder output */
	AUDIO_SPDIF_SOURCE_DEC,	/*<! decoder output w/o post-proc */
	AUDIO_SPDIF_SOURCE_ES,	/*<! raw elementary stream data */
} audio_spdif_source_t;

/*
 * audio discontinuity
 */
typedef enum {
	AUDIO_DISCONTINUITY_SKIP = DVB_DISCONTINUITY_SKIP,
	AUDIO_DISCONTINUITY_CONTINUOUS_REVERSE =
	    DVB_DISCONTINUITY_CONTINUOUS_REVERSE,
	AUDIO_DISCONTINUITY_SURPLUS_DATA = DVB_DISCONTINUITY_SURPLUS_DATA,
	AUDIO_DISCONTINUITY_EOS = DVB_DISCONTINUITY_EOS,
} audio_discontinuity_t;

typedef enum audio_aac_profile_e {
	AUDIO_AAC_LC_TS_PROFILE,	/* Auto detect */
	AUDIO_AAC_LC_ADTS_PROFILE,	/* ADTS force */
	AUDIO_AAC_LC_LOAS_PROFILE,	/* LOAS force */
	AUDIO_AAC_LC_RAW_PROFILE,	/* RAW  force */
	AUDIO_AAC_BSAC_PROFILE,	/* BSAC force */

} audio_aac_profile_t;

typedef struct audio_mpeg4aac_s {
	audio_aac_profile_t aac_profile;
	unsigned char sbr_enable;
	unsigned char sbr_96k_enable;
	unsigned char ps_enable;
} audio_mpeg4aac_t;

typedef struct audio_channel_assignment {
	unsigned int pair0:6; /* channels 0 and 1 */
	unsigned int pair1:6; /* channels 2 and 3 */
	unsigned int pair2:6; /* channels 4 and 5 */
	unsigned int pair3:6; /* channels 6 and 7 */
	unsigned int pair4:6; /* channels 8 and 9 */
	unsigned int reserved0:1;
	unsigned int malleable:1;
} audio_channel_assignment_t;

typedef struct audio_parameters_s {
	audio_encoding_t coding;
	audio_channel_assignment_t channel_assignment;
	int           bitrate;
	unsigned int  sampling_freq;
	unsigned char num_channels;
	int           dual_mono;
} audio_parameters_t;

typedef enum audio_application_e {
	/* Do not change this order nor the offset */
	AUDIO_APPLICATION_ISO = 0,
	AUDIO_APPLICATION_DVD,
	AUDIO_APPLICATION_DVB,
	AUDIO_APPLICATION_MS10,
	AUDIO_APPLICATION_MS11,
	AUDIO_APPLICATION_MS12,
} audio_application_t;

typedef enum audio_service_e {
	/* Do not change this order nor the offset */
	AUDIO_SERVICE_PRIMARY = 0,
	AUDIO_SERVICE_SECONDARY,
	AUDIO_SERVICE_MAIN,
	AUDIO_SERVICE_AUDIO_DESCRIPTION,
	AUDIO_SERVICE_MAIN_AND_AUDIO_DESCRIPTION,
	AUDIO_SERVICE_CLEAN_AUDIO,
} audio_service_t;

typedef enum audio_region_e {
	/* Do not change this order nor the offset */
	AUDIO_REGION_UNDEFINED = 0,
	AUDIO_REGION_ATSC,
	AUDIO_REGION_DVB,
	AUDIO_REGION_NORDIG,
	AUDIO_REGION_DTG,
	AUDIO_REGION_ARIB,
	AUDIO_REGION_DTMB,
} audio_region_t;

/*
 * Following are the audio events which can be retrieved
 * by the interested user space application.
 */
enum audio_event_type {
	AUDIO_EVENT_PARAMETERS_CHANGED	= 1,
	AUDIO_EVENT_STREAM_UNPLAYABLE,
	AUDIO_EVENT_TRICK_MODE_CHANGE,
	AUDIO_EVENT_LOST,
	AUDIO_EVENT_END_OF_STREAM,
	AUDIO_EVENT_FRAME_STARVATION,
	AUDIO_EVENT_FRAME_SUPPLIED,
	AUDIO_EVENT_FRAME_DECODED_LATE,
	AUDIO_EVENT_DATA_DELIVERED_LATE,
	AUDIO_EVENT_FATAL_ERROR,
	AUDIO_EVENT_FATAL_HARDWARE_FAILURE,
	AUDIO_EVENT_FRAME_DECODED,
	AUDIO_EVENT_FRAME_RENDERED,
};

/*
 * This is the structure in which audio events are passed
 * @type            : audio event type (enum audio_event_type)
 * @value           : valid from (AUDIO_EVENT_STREAM_UNPLAYABLE to
 *                    AUDIO_EVENT_LOST)
 * @audio_parameters: valid for AUDIO_EVENT_PARAMETERS_CHANGED
 */
struct audio_event {
	int  type;
	union {
		unsigned int value;
		audio_parameters_t audio_parameters;
	} u;
};

typedef struct audio_option_s {
	unsigned int option;
	unsigned int value;
} audio_option_t;

struct audio_command {
	unsigned int cmd;
#define AUDIO_CMD_SET_OPTION 1
	union {
		audio_option_t option;
	} u;
};

typedef dvb_play_interval_t audio_play_interval_t;
typedef dvb_play_time_t audio_play_time_t;
typedef dvb_play_info_t audio_play_info_t;
typedef dvb_clock_data_point_t audio_clock_data_point_t;
typedef dvb_time_mapping_t audio_time_mapping_t;

/* ST specific audio ioctls */
#define AUDIO_SET_ENCODING              _IO('o',  70)
#define AUDIO_FLUSH                     _IO('o',  71)
#define AUDIO_SET_SPDIF_SOURCE          _IO('o',  72)
#define AUDIO_SET_SPEED                 _IO('o',  73)
#define AUDIO_DISCONTINUITY             _IO('o',  74)
#define AUDIO_SET_PLAY_INTERVAL         _IOW('o', 75, audio_play_interval_t)
#define AUDIO_SYNC_GROUP_MASK	0xC0
#define AUDIO_SYNC_GROUP_DEMUX	0x40
#define AUDIO_SYNC_GROUP_AUDIO	0x80
#define AUDIO_SYNC_GROUP_VIDEO	0xC0
#define AUDIO_SET_SYNC_GROUP            _IO('o',  76)
#define AUDIO_GET_PLAY_TIME             _IOR('o', 77, audio_play_time_t)
#define AUDIO_GET_PLAY_INFO             _IOR('o', 78, audio_play_info_t)
#define AUDIO_SET_CLOCK_DATA_POINT      _IOW('o', 79, audio_clock_data_point_t)
#define AUDIO_SET_TIME_MAPPING          _IOW('o', 80, audio_time_mapping_t)
#define AUDIO_GET_CLOCK_DATA_POINT      _IOR('o', 81, audio_clock_data_point_t)
/* Deprecated: #define AUDIO_STEP			_IO('o',  82) */
#define AUDIO_SET_APPLICATION_TYPE      _IO('o',  83)
#define AUDIO_SET_AAC_DECODER_CONFIG    _IOW('o', 84, audio_mpeg4aac_t)
#define AUDIO_SET_SERVICE_TYPE          _IO('o',  85)
#define AUDIO_STREAMDRIVEN_DUALMONO	_IO('o',  86)
#define AUDIO_SET_REGION_TYPE           _IO('o',  87)
#define AUDIO_SET_PROGRAM_REFERENCE_LEVEL  _IO('o',  88)
#define AUDIO_GET_EVENT                 _IOR('o', 89, struct audio_event)
#define AUDIO_STREAM_DOWNMIX            _IO('o',  90)
#define AUDIO_STREAMDRIVEN_STEREO       _IO('o',  91)
#define AUDIO_COMMAND			_IOWR('o',  92, struct audio_command)
#endif /* H_DVB_STM_H */
