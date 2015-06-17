/************************************************************************
Copyright (C) 2003 STMicroelectronics. All Rights Reserved.

This file is part of the Player2 Library.

Player2 is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License version 2 as published by the
Free Software Foundation.

Player2 is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with player2; see the file COPYING.  If not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

The Player2 Library may alternatively be licensed under a proprietary
license from ST.

Source file name : backend_ops.h - player device definitions
Author :           Julian

Date        Modification                                    Name
----        ------------                                    --------
24-Mar-03   Created                                         Julian

************************************************************************/

#ifndef H_BACKEND_OPS
#define H_BACKEND_OPS

#ifndef __cplusplus
#include <linux/dvb/audio.h>
#include <linux/dvb/video.h>
#else

/* extracts from linux/dvb/audio.h (which can't be included by C++ files in Linux 2.3) */
typedef enum {
        AUDIO_STEREO,
        AUDIO_MONO_LEFT,
        AUDIO_MONO_RIGHT
} audio_channel_select_t;

/* extracts from linux/dvb/video.h (which can't be included by C++ files in Linux 2.3) */
typedef enum {
        VIDEO_FORMAT_4_3,     /* Select 4:3 format */
        VIDEO_FORMAT_16_9,    /* Select 16:9 format. */
        VIDEO_FORMAT_221_1    /* 2.21:1 */
} video_format_t;

typedef enum {
        VIDEO_PAN_SCAN,       /* use pan and scan format */
        VIDEO_LETTER_BOX,     /* use letterbox format */
        VIDEO_CENTER_CUT_OUT  /* use center cut out format */
} video_displayformat_t;

#define VIDEO_EVENT_SIZE_CHANGED        1
#define VIDEO_EVENT_FRAME_RATE_CHANGED  2
#define VIDEO_EVENT_DECODER_STOPPED     3
#define VIDEO_EVENT_VSYNC               4

#endif

#include "linux/dvb/stm_ioctls.h"

#define BACKEND_AUDIO_ID        "audio"
#define BACKEND_VIDEO_ID        "video"

#define BACKEND_PES_ID          "pes"
#define BACKEND_TS_ID           "ts"

#define BACKEND_AUTO_ID         "auto"
#define BACKEND_PCM_ID          "pcm"
#define BACKEND_LPCM_ID         "lpcm"
#define BACKEND_MPEG1_ID        "mpeg1"
#define BACKEND_MPEG2_ID        "mpeg2"
#define BACKEND_MP3_ID          "mp3"
#define BACKEND_AC3_ID          "ac3"
#define BACKEND_DTS_ID          "dts"
#define BACKEND_AAC_ID          "aac"
#define BACKEND_WMA_ID          "wma"
#define BACKEND_RAW_ID          "raw"
#define BACKEND_LPCMA_ID        "lpcma"
#define BACKEND_LPCMH_ID        "lpcmh"
#define BACKEND_LPCMB_ID        "lpcmb"
#define BACKEND_SPDIFIN_ID      "spdifin"

#define BACKEND_DTS_LBR_ID      "dtslbr"
#define BACKEND_MLP_ID          "mlp"
#define BACKEND_RMA_ID          "rma"
#define BACKEND_AVS_ID          "avs"
#define BACKEND_VORBIS_ID       "vorbis"
#define BACKEND_NONE_ID         "none"

#define BACKEND_MIXER0_ID       "mixer0" /* main room mixer */
#define BACKEND_MIXER1_ID       "mixer1" /* second room mixer */

#define BACKEND_MJPEG_ID        "mjpeg"
#define BACKEND_DIVX3_ID        "divx3"
#define BACKEND_DIVX4_ID        "divx4"
#define BACKEND_DIVX5_ID        "divx5"
#define BACKEND_DIVXHD_ID       "divxhd"
#define BACKEND_MPEG4P2_ID      "mpeg4p2"
#define BACKEND_H264_ID         "h264"
#define BACKEND_WMV_ID          "wmv"
#define BACKEND_VC1_ID          "vc1"
#define BACKEND_H263_ID         "h263"
#define BACKEND_FLV1_ID         "flv1"
#define BACKEND_VP6_ID          "vp6"
#define BACKEND_RMV_ID          "rmv"
#define BACKEND_DVP_ID          "dvp"
#define BACKEND_VP3_ID          "vp3"
#define BACKEND_THEORA_ID       "theora"
#define BACKEND_CAP_ID          "cap"

#define DEMUX_INVALID_ID        0xffffffff

#define PLAY_SPEED_NORMAL_PLAY          DVB_SPEED_NORMAL_PLAY
#define PLAY_SPEED_STOPPED              DVB_SPEED_STOPPED
#define PLAY_SPEED_REVERSE_STOPPED      DVB_SPEED_REVERSE_STOPPED
#define PLAY_FRAME_RATE_MULTIPLIER      DVB_FRAME_RATE_MULTIPLIER

#define PLAY_TIME_NOT_BOUNDED           DVB_TIME_NOT_BOUNDED

typedef void           *context_handle_t;
typedef void           *playback_handle_t;
typedef void           *demux_handle_t;
typedef void           *stream_handle_t;
typedef void           *buffer_handle_t;

typedef enum play_option_e
{
#define PLAY_OPTION_VALUE_DISABLE                                                   DVB_OPTION_VALUE_DISABLE
#define PLAY_OPTION_VALUE_ENABLE                                                    DVB_OPTION_VALUE_ENABLE

    PLAY_OPTION_TRICK_MODE_AUDIO                                = DVB_OPTION_TRICK_MODE_AUDIO,
    PLAY_OPTION_PLAY_24FPS_VIDEO_AT_25FPS                       = DVB_OPTION_PLAY_24FPS_VIDEO_AT_25FPS,

#define PLAY_OPTION_VALUE_VIDEO_CLOCK_MASTER                                         DVB_OPTION_VALUE_VIDEO_CLOCK_MASTER
#define PLAY_OPTION_VALUE_AUDIO_CLOCK_MASTER                                         DVB_OPTION_VALUE_AUDIO_CLOCK_MASTER
#define PLAY_OPTION_VALUE_SYSTEM_CLOCK_MASTER                                        DVB_OPTION_VALUE_SYSTEM_CLOCK_MASTER
    PLAY_OPTION_MASTER_CLOCK                                    = DVB_OPTION_MASTER_CLOCK,

    PLAY_OPTION_EXTERNAL_TIME_MAPPING                           = DVB_OPTION_EXTERNAL_TIME_MAPPING,
    PLAY_OPTION_EXTERNAL_TIME_MAPPING_VSYNC_LOCKED              = DVB_OPTION_EXTERNAL_TIME_MAPPING_VSYNC_LOCKED,
    PLAY_OPTION_AV_SYNC                                         = DVB_OPTION_AV_SYNC,
    PLAY_OPTION_SYNC_START_IMMEDIATE                            = DVB_OPTION_SYNC_START_IMMEDIATE,
    PLAY_OPTION_DISPLAY_FIRST_FRAME_EARLY                       = DVB_OPTION_DISPLAY_FIRST_FRAME_EARLY,
    PLAY_OPTION_VIDEO_BLANK                                     = DVB_OPTION_VIDEO_BLANK,
    PLAY_OPTION_STREAM_ONLY_KEY_FRAMES                          = DVB_OPTION_STREAM_ONLY_KEY_FRAMES,
    PLAY_OPTION_STREAM_SINGLE_GROUP_BETWEEN_DISCONTINUITIES     = DVB_OPTION_STREAM_SINGLE_GROUP_BETWEEN_DISCONTINUITIES,

#define PLAY_OPTION_VALUE_PLAYOUT                                                    DVB_OPTION_VALUE_PLAYOUT
#define PLAY_OPTION_VALUE_DISCARD                                                    DVB_OPTION_VALUE_DISCARD
    PLAY_OPTION_PLAYOUT_ON_TERMINATE                            = DVB_OPTION_PLAYOUT_ON_TERMINATE,

    PLAY_OPTION_PLAYOUT_ON_SWITCH                               = DVB_OPTION_PLAYOUT_ON_SWITCH,
    PLAY_OPTION_PLAYOUT_ON_DRAIN                                = DVB_OPTION_PLAYOUT_ON_DRAIN,
    PLAY_OPTION_VIDEO_ASPECT_RATIO                              = DVB_OPTION_VIDEO_ASPECT_RATIO,
    PLAY_OPTION_VIDEO_DISPLAY_FORMAT                            = DVB_OPTION_VIDEO_DISPLAY_FORMAT,

#define PLAY_OPTION_VALUE_TRICK_MODE_AUTO                                            DVB_OPTION_VALUE_TRICK_MODE_AUTO
#define PLAY_OPTION_VALUE_TRICK_MODE_DECODE_ALL                                      DVB_OPTION_VALUE_TRICK_MODE_DECODE_ALL
#define PLAY_OPTION_VALUE_TRICK_MODE_DECODE_ALL_DEGRADE_NON_REFERENCE_FRAMES         DVB_OPTION_VALUE_TRICK_MODE_DECODE_ALL_DEGRADE_NON_REFERENCE_FRAMES
#define PLAY_OPTION_VALUE_TRICK_MODE_START_DISCARDING_NON_REFERENCE_FRAMES           DVB_OPTION_VALUE_TRICK_MODE_START_DISCARDING_NON_REFERENCE_FRAMES
#define PLAY_OPTION_VALUE_TRICK_MODE_DECODE_REFERENCE_FRAMES_DEGRADE_NON_KEY_FRAMES  DVB_OPTION_VALUE_TRICK_MODE_DECODE_REFERENCE_FRAMES_DEGRADE_NON_KEY_FRAMES
#define PLAY_OPTION_VALUE_TRICK_MODE_DECODE_KEY_FRAMES                               DVB_OPTION_VALUE_TRICK_MODE_DECODE_KEY_FRAMES
#define PLAY_OPTION_VALUE_TRICK_MODE_DISCONTINUOUS_KEY_FRAMES                        DVB_OPTION_VALUE_TRICK_MODE_DISCONTINUOUS_KEY_FRAMES
    PLAY_OPTION_TRICK_MODE_DOMAIN                               = DVB_OPTION_TRICK_MODE_DOMAIN,

#define PLAY_OPTION_VALUE_DISCARD_LATE_FRAMES_NEVER                                  DVB_OPTION_VALUE_DISCARD_LATE_FRAMES_NEVER
#define PLAY_OPTION_VALUE_DISCARD_LATE_FRAMES_ALWAYS                                 DVB_OPTION_VALUE_DISCARD_LATE_FRAMES_ALWAYS
#define PLAY_OPTION_VALUE_DISCARD_LATE_FRAMES_AFTER_SYNCHRONIZE                      DVB_OPTION_VALUE_DISCARD_LATE_FRAMES_AFTER_SYNCHRONIZE
    PLAY_OPTION_DISCARD_LATE_FRAMES                             = DVB_OPTION_DISCARD_LATE_FRAMES,

    PLAY_OPTION_VIDEO_START_IMMEDIATE                           = DVB_OPTION_VIDEO_START_IMMEDIATE,
    PLAY_OPTION_REBASE_ON_DATA_DELIVERY_LATE                    = DVB_OPTION_REBASE_ON_DATA_DELIVERY_LATE,
    PLAY_OPTION_REBASE_ON_FRAME_DECODE_LATE                     = DVB_OPTION_REBASE_ON_FRAME_DECODE_LATE,
    PLAY_OPTION_LOWER_CODEC_DECODE_LIMITS_ON_FRAME_DECODE_LATE  = DVB_OPTION_LOWER_CODEC_DECODE_LIMITS_ON_FRAME_DECODE_LATE,
    PLAY_OPTION_H264_ALLOW_NON_IDR_RESYNCHRONIZATION            = DVB_OPTION_H264_ALLOW_NON_IDR_RESYNCHRONIZATION,
    PLAY_OPTION_MPEG2_IGNORE_PROGESSIVE_FRAME_FLAG              = DVB_OPTION_MPEG2_IGNORE_PROGESSIVE_FRAME_FLAG,
    PLAY_OPTION_AUDIO_SPDIF_SOURCE                              = DVB_OPTION_AUDIO_SPDIF_SOURCE,

    PLAY_OPTION_CLAMP_PLAYBACK_INTERVAL_ON_PLAYBACK_DIRECTION_CHANGE    = DVB_OPTION_CLAMP_PLAYBACK_INTERVAL_ON_PLAYBACK_DIRECTION_CHANGE,
    PLAY_OPTION_H264_ALLOW_BAD_PREPROCESSED_FRAMES                      = DVB_OPTION_H264_ALLOW_BAD_PREPROCESSED_FRAMES,
    PLAY_OPTION_CLOCK_RATE_ADJUSTMENT_LIMIT_2_TO_THE_N_PARTS_PER_MILLION= DVB_OPTION_CLOCK_RATE_ADJUSTMENT_LIMIT_2_TO_THE_N_PARTS_PER_MILLION,
    PLAY_OPTION_LIMIT_INPUT_INJECT_AHEAD                                = DVB_OPTION_LIMIT_INPUT_INJECT_AHEAD,

#define PLAY_OPTION_VALUE_MPEG2_APPLICATION_MPEG2                                    DVB_OPTION_VALUE_MPEG2_APPLICATION_MPEG2
#define PLAY_OPTION_VALUE_MPEG2_APPLICATION_ATSC                                     DVB_OPTION_VALUE_MPEG2_APPLICATION_ATSC
#define PLAY_OPTION_VALUE_MPEG2_APPLICATION_DVB                                      DVB_OPTION_VALUE_MPEG2_APPLICATION_DVB
    PLAY_OPTION_MPEG2_APPLICATION_TYPE                                  = DVB_OPTION_MPEG2_APPLICATION_TYPE,

#define PLAY_OPTION_VALUE_DECIMATE_DECODER_OUTPUT_DISABLED                           DVB_OPTION_VALUE_DECIMATE_DECODER_OUTPUT_DISABLED
#define PLAY_OPTION_VALUE_DECIMATE_DECODER_OUTPUT_HALF                               DVB_OPTION_VALUE_DECIMATE_DECODER_OUTPUT_HALF
#define PLAY_OPTION_VALUE_DECIMATE_DECODER_OUTPUT_QUARTER                            DVB_OPTION_VALUE_DECIMATE_DECODER_OUTPUT_QUARTER
    PLAY_OPTION_DECIMATE_DECODER_OUTPUT                                 = DVB_OPTION_DECIMATE_DECODER_OUTPUT,

    PLAY_OPTION_PTS_SYMMETRIC_JUMP_DETECTION                            = DVB_OPTION_PTS_SYMMETRIC_JUMP_DETECTION,
    PLAY_OPTION_PTS_FORWARD_JUMP_DETECTION_THRESHOLD                    = DVB_OPTION_PTS_FORWARD_JUMP_DETECTION_THRESHOLD,
    PLAY_OPTION_H264_TREAT_DUPLICATE_DPB_AS_NON_REFERENCE_FRAME_FIRST   = DVB_OPTION_H264_TREAT_DUPLICATE_DPB_AS_NON_REFERENCE_FRAME_FIRST,
    PLAY_OPTION_H264_FORCE_PIC_ORDER_CNT_IGNORE_DPB_DISPLAY_FRAME_ORDERING = DVB_OPTION_H264_FORCE_PIC_ORDER_CNT_IGNORE_DPB_DISPLAY_FRAME_ORDERING,
    PLAY_OPTION_H264_TREAT_TOP_BOTTOM_PICTURE_STRUCT_AS_INTERLACED      = DVB_OPTION_H264_TREAT_TOP_BOTTOM_PICTURE_STRUCT_AS_INTERLACED,

    PLAY_OPTION_PIXEL_ASPECT_RATIO_CORRECTION                           = DVB_OPTION_PIXEL_ASPECT_RATIO_CORRECTION,

    PLAY_OPTION_ALLOW_FRAME_DISCARD_AT_NORMAL_SPEED                     = DVB_OPTION_ALLOW_FRAME_DISCARD_AT_NORMAL_SPEED,

    PLAY_OPTION_OPERATE_COLLATOR2_IN_REVERSIBLE_MODE                    = DVB_OPTION_OPERATE_COLLATOR2_IN_REVERSIBLE_MODE,

    PLAY_OPTION_VIDEO_OUTPUT_WINDOW_RESIZE_STEPS                        = DVB_OPTION_VIDEO_OUTPUT_WINDOW_RESIZE_STEPS,

    PLAY_OPTION_IGNORE_STREAM_UNPLAYABLE_CALLS                          = DVB_OPTION_IGNORE_STREAM_UNPLAYABLE_CALLS,

    PLAY_OPTION_USE_PTS_DEDUCED_DEFAULT_FRAME_RATES                     = DVB_OPTION_USE_PTS_DEDUCED_DEFAULT_FRAME_RATES,

    PLAY_OPTION_MAX                                                     = DVB_OPTION_MAX

} play_option_t;

typedef enum aspect_ratio_e
{
    ASPECT_RATIO_4_3    = VIDEO_FORMAT_4_3,
    ASPECT_RATIO_16_9   = VIDEO_FORMAT_16_9,
    ASPECT_RATIO_221_1  = VIDEO_FORMAT_221_1
} aspect_ratio_t;

typedef enum reason_code_e
{
    REASON_UNKNOWN,
    REASON_STREAM_INVALID,
    REASON_STREAM_CAPABILITY,
} reason_code_t;

#if 0
typedef enum
{
    TRICK_MODE_AUTO                                             = PLAY_OPTION_VALUE_TRICK_MODE_AUTO,
    TRICK_MODE_DECODE_ALL                                       = PLAY_OPTION_VALUE_TRICK_MODE_DECODE_ALL,
    TRICK_MODE_DECODE_ALL_DEGRADE_NON_REFERENCE_FRAMES          = PLAY_OPTION_VALUE_TRICK_MODE_DECODE_ALL_DEGRADE_NON_REFERENCE_FRAMES,
    TRICK_MODE_START_DISCARDING_NON_REFERENCE_FRAMES            = PLAY_OPTION_VALUE_TRICK_MODE_START_DISCARDING_NON_REFERENCE_FRAMES,
    TRICK_MODE_DECODE_REFERENCE_FRAMES_DEGRADE_NON_KEY_FRAMES   = PLAY_OPTION_VALUE_TRICK_MODE_DECODE_REFERENCE_FRAMES_DEGRADE_NON_KEY_FRAMES,
    TRICK_MODE_DECODE_KEY_FRAMES                                = PLAY_OPTION_VALUE_TRICK_MODE_DECODE_KEY_FRAMES,
    TRICK_MODE_DISCONTINUOUS_KEY_FRAMES                         = PLAY_OPTION_VALUE_TRICK_MODE_DISCONTINUOUS_KEY_FRAMES
} trick_mode_domain_t;
#define TRICK_MODE_AUTO                                                 PLAY_OPTION_VALUE_TRICK_MODE_AUTO
#define TRICK_MODE_DECODE_ALL                                           PLAY_OPTION_VALUE_TRICK_MODE_DECODE_ALL
#define TRICK_MODE_DECODE_ALL_DEGRADE_NON_REFERENCE_FRAMES              PLAY_OPTION_VALUE_TRICK_MODE_DECODE_ALL_DEGRADE_NON_REFERENCE_FRAMES
#define TRICK_MODE_START_DISCARDING_NON_REFERENCE_FRAMES                PLAY_OPTION_VALUE_TRICK_MODE_START_DISCARDING_NON_REFERENCE_FRAMES
#define TRICK_MODE_DECODE_REFERENCE_FRAMES_DEGRADE_NON_KEY_FRAMES       PLAY_OPTION_VALUE_TRICK_MODE_DECODE_REFERENCE_FRAMES_DEGRADE_NON_KEY_FRAMES
#define TRICK_MODE_DECODE_KEY_FRAMES                                    PLAY_OPTION_VALUE_TRICK_MODE_DECODE_KEY_FRAMES
#define TRICK_MODE_DISCONTINUOUS_KEY_FRAMES                             PLAY_OPTION_VALUE_TRICK_MODE_DISCONTINUOUS_KEY_FRAMES
#endif

struct picture_size_s
{
    unsigned int        width;
    unsigned int        height;
    aspect_ratio_t      aspect_ratio;
    unsigned int        pixel_aspect_ratio_numerator;
    unsigned int        pixel_aspect_ratio_denominator;
};

struct picture_rectangle_s
{
    unsigned int        x;
    unsigned int        y;
    unsigned int        width;
    unsigned int        height;
};

#define STREAM_EVENT_SIZE_CHANGED               VIDEO_EVENT_SIZE_CHANGED
#define STREAM_EVENT_OUTPUT_SIZE_CHANGED        VIDEO_EVENT_OUTPUT_SIZE_CHANGED
#define STREAM_EVENT_FRAME_RATE_CHANGED         VIDEO_EVENT_FRAME_RATE_CHANGED
#define STREAM_EVENT_FIRST_FRAME_ON_DISPLAY     VIDEO_EVENT_FIRST_FRAME_ON_DISPLAY
#define STREAM_EVENT_FRAME_DECODED_LATE         VIDEO_EVENT_FRAME_DECODED_LATE
#define STREAM_EVENT_DATA_DELIVERED_LATE        VIDEO_EVENT_DATA_DELIVERED_LATE
#define STREAM_EVENT_STREAM_UNPLAYABLE          VIDEO_EVENT_STREAM_UNPLAYABLE
#define STREAM_EVENT_TRICK_MODE_CHANGE          VIDEO_EVENT_TRICK_MODE_CHANGE
#define STREAM_EVENT_VSYNC_OFFSET_MEASURED      VIDEO_EVENT_VSYNC_OFFSET_MEASURED       /* Normally intercepted by DVP code */
#define STREAM_EVENT_FATAL_ERROR                VIDEO_EVENT_FATAL_ERROR
#define STREAM_EVENT_FATAL_HARDWARE_FAILURE     VIDEO_EVENT_FATAL_HARDWARE_FAILURE
#define STREAM_EVENT_INVALID                    0xffffffff

struct stream_event_s
{
    unsigned int                code;
    unsigned long long          timestamp;
    union
    {
        struct picture_size_s   size;
        struct picture_rectangle_s   rectangle;
        unsigned int            frame_rate;        /* in frames per 1000sec */
        reason_code_t           reason;
        unsigned int            trick_mode_domain;
        unsigned long long      longlong;
    } u;
};

struct play_info_s
{
    unsigned long long          system_time;
    unsigned long long          presentation_time;
    unsigned long long          pts;
    unsigned long long          frame_count;
};

typedef enum
{
        DISCONTINUITY_SKIP                      = DVB_DISCONTINUITY_SKIP,
        DISCONTINUITY_CONTINUOUS_REVERSE        = DVB_DISCONTINUITY_CONTINUOUS_REVERSE,
        DISCONTINUITY_SURPLUS_DATA              = DVB_DISCONTINUITY_SURPLUS_DATA,
} discontinuity_t;

typedef enum {
        CHANNEL_STEREO                          = AUDIO_STEREO,
        CHANNEL_MONO_LEFT                       = AUDIO_MONO_LEFT,
        CHANNEL_MONO_RIGHT                      = AUDIO_MONO_RIGHT,
} channel_select_t;


typedef dvb_play_interval_t             play_interval_t;

typedef enum
{
        TIME_FORMAT_US                          = DVB_TIME_FORMAT_US,
        TIME_FORMAT_PTS                         = DVB_TIME_FORMAT_PTS
} time_format_t;

typedef dvb_clock_data_point_t           clock_data_point_t;

typedef void (*stream_event_signal_callback)   (context_handle_t        context,
                                                struct stream_event_s*  event);

struct dvb_backend_operations
{
    struct module                      *owner;

    int (*playback_create)             (playback_handle_t      *playback);
    int (*playback_delete)             (playback_handle_t       playback);
    int (*playback_get_speed)          (playback_handle_t       playback,
                                        int                    *speed);
    int (*playback_set_speed)          (playback_handle_t       playback,
                                        int                     speed);
    int (*playback_add_demux)          (playback_handle_t       playback,
                                        int                     demux_id,
                                        demux_handle_t         *demux_context);
    int (*playback_remove_demux)       (playback_handle_t       playback,
                                        demux_handle_t          demux_context);
    int (*playback_add_stream)         (playback_handle_t       playback,
                                        char                   *media,
                                        char                   *format,
                                        char                   *encoding,
                                        unsigned int            surface_id,
                                        stream_handle_t        *stream);
    int (*playback_remove_stream)      (playback_handle_t       playback,
                                        stream_handle_t         stream);
    int (*playback_set_native_playback_time)   (playback_handle_t       playback,
                                                unsigned long long      NativeTime,
                                                unsigned long long      SystemTime);
    int (*playback_set_option)         (playback_handle_t       playback,
                                        play_option_t           option,
                                        unsigned int            value);
    int (*playback_get_player_environment)     (playback_handle_t  Playback,
                                                playback_handle_t* playerplayback);
    int (*playback_set_clock_data_point)       (playback_handle_t       playback,
                                                clock_data_point_t*     data_point);

    int (*demux_inject_data)           (demux_handle_t          demux,
                                        unsigned const char*    data,
                                        unsigned int            data_length);
    int (*stream_inject_data)          (stream_handle_t         stream,
                                        unsigned const char*    data,
                                        unsigned int            data_length);
    int (*stream_inject_data_packet)   (stream_handle_t         stream,
                                        unsigned const char*    data,
                                        unsigned int            data_length,
                                        bool                    presentation_time_valid,
                                        unsigned long long      presentation_time);
    int (*stream_discontinuity)        (stream_handle_t         stream,
                                        discontinuity_t         discontinuity);

    int (*stream_enable)               (stream_handle_t         stream,
                                        unsigned int            enable);
    int (*stream_set_id)               (stream_handle_t         stream,
                                        unsigned int            demux_id,
                                        unsigned int            id);
    int (*stream_channel_select)       (stream_handle_t         stream,
                                        channel_select_t        channel);
    int (*stream_set_option)           (stream_handle_t         stream,
                                        play_option_t           option,
                                        unsigned int            value);
    int (*stream_get_option)           (stream_handle_t         stream,
                                        play_option_t           option,
                                        unsigned int*           value);
    int (*stream_drain)                (stream_handle_t         stream,
                                        unsigned int            discard);
    int (*stream_step)                 (stream_handle_t         Stream);
    int (*stream_get_play_info)        (stream_handle_t         stream,
                                        struct play_info_s     *play_info);
    int (*stream_switch)               (stream_handle_t         stream,
                                        char                   *format,
                                        char                   *encoding);
    int (*stream_set_alarm)            (stream_handle_t         stream,
                                        unsigned long long      pts);
    int (*stream_get_decode_buffer)    (stream_handle_t         stream,
                                        buffer_handle_t        *buffer,
                                        unsigned char**         data,
                                        unsigned int            Format,
                                        unsigned int            DimensionCount,
                                        unsigned int            Dimensions[],
                                        unsigned int*           Index,
                                        unsigned int*           Stride);
    int (*stream_return_decode_buffer) (stream_handle_t         stream,
                                        buffer_handle_t        *buffer);
    int (*stream_set_output_window)    (stream_handle_t         stream,
                                        unsigned int            X,
                                        unsigned int            Y,
                                        unsigned int            Width,
                                        unsigned int            Height);
    int (*stream_get_output_window)    (stream_handle_t         stream,
                                        unsigned int*           X,
                                        unsigned int*           Y,
                                        unsigned int*           Width,
                                        unsigned int*           Height);
    int (*stream_set_input_window)     (stream_handle_t         stream,
                                        unsigned int            X,
                                        unsigned int            Y,
                                        unsigned int            Width,
                                        unsigned int            Height);
    int (*stream_set_play_interval)    (stream_handle_t         stream,
                                        play_interval_t        *play_interval);
    int (*stream_get_decode_buffer_pool_status)    (stream_handle_t         stream,
                                                    unsigned int*           BuffersInPool,
                                                    unsigned int*           BuffersWithNonZeroReferenceCount);
    int (*stream_get_player_environment)                                       (stream_handle_t                 stream,
                                                                                playback_handle_t               *PlayerPlayback,
                                                                                stream_handle_t                 *PlayerStream);
    stream_event_signal_callback (*stream_register_event_signal_callback)      (stream_handle_t                 stream,
                                                                                context_handle_t                context,
                                                                                stream_event_signal_callback    callback);

    int (*display_create)             (char*                   Media,
                                       unsigned int            SurfaceId);
    int (*display_delete)             (char*                   Media,
                                       unsigned int            SurfaceId);
    int (*display_synchronize)        (char*                   Media,
                                       unsigned int            SurfaceId);

};

int register_dvb_backend        (char                           *name,
                                 struct dvb_backend_operations  *backend_ops);
#endif
