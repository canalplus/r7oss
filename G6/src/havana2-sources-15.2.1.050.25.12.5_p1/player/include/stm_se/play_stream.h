/************************************************************************
Copyright (C) 2003-2014 STMicroelectronics. All Rights Reserved.

This file is part of the Streaming Engine.

Streaming Engine is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

Streaming Engine is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with Streaming Engine; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307,
USA.

The Streaming Engine Library may alternatively be licensed under a
proprietary license from ST.
************************************************************************/

#ifndef STM_SE_PLAY_STREAM_H_
#define STM_SE_PLAY_STREAM_H_

/*!
 * \defgroup play_stream Play stream
 *
 * A play stream is responsible for parsing, decoding, and rendering a single stream of audio or video
 * content. All play streams are members of exactly one playback.
 *
 * \section play_stream_control Controls
 *
 * The play stream object supports the following control selectors:
 *
 * \subsection play_stream_core_controls Core controls
 *
 *  - ::STM_SE_CTRL_REGION
 *  - ::STM_SE_CTRL_ALLOW_REBASE_AFTER_LATE_DECODE
 *  - ::STM_SE_CTRL_CLAMP_PLAY_INTERVAL_ON_DIRECTION_CHANGE
 *  - ::STM_SE_CTRL_CLOCK_RATE_ADJUSTMENT_LIMIT
 *  - ::STM_SE_CTRL_DISCARD_LATE_FRAMES
 *  - ::STM_SE_CTRL_DISPLAY_FIRST_FRAME_EARLY
 *  - ::STM_SE_CTRL_ENABLE_SYNC
 *  - ::STM_SE_CTRL_EXTERNAL_TIME_MAPPING
 *  - ::STM_SE_CTRL_EXTERNAL_TIME_MAPPING_VSYNC_LOCKED
 *  - ::STM_SE_CTRL_IGNORE_STREAM_UNPLAYABLE_CALLS
 *  - ::STM_SE_CTRL_LIMIT_INJECT_AHEAD
 *  - ::STM_SE_CTRL_MASTER_CLOCK
 *  - ::STM_SE_CTRL_PLAYOUT_ON_DRAIN
 *  - ::STM_SE_CTRL_PLAYOUT_ON_SWITCH
 *  - ::STM_SE_CTRL_PLAYOUT_ON_TERMINATE
 *  - ::STM_SE_CTRL_REBASE_ON_DATA_DELIVERY_LATE
 *  - ::STM_SE_CTRL_SYMMETRIC_JUMP_DETECTION
 *  - ::STM_SE_CTRL_SYMMETRIC_PTS_FORWARD_JUMP_DETECTION_THRESHOLD
 *  - ::STM_SE_CTRL_USE_PTS_DEDUCED_DEFAULT_FRAME_RATES
 *  - ::STM_SE_CTRL_NEGOTIATE_VIDEO_DECODE_BUFFERS
 *
 * \subsection play_stream_video_controls Video controls
 *
 *  - ::STM_SE_CTRL_VIDEO_START_IMMEDIATE
 *  - ::STM_SE_CTRL_ALLOW_FRAME_DISCARD_AT_NORMAL_SPEED
 *  - ::STM_SE_CTRL_DECIMATE_DECODER_OUTPUT
 *  - ::STM_SE_CTRL_TRICK_MODE_DOMAIN
 *  - ::STM_SE_CTRL_HEVC_ALLOW_BAD_PREPROCESSED_FRAMES
 *  - ::STM_SE_CTRL_H264_ALLOW_BAD_PREPROCESSED_FRAMES
 *  - ::STM_SE_CTRL_H264_ALLOW_NON_IDR_RESYNCHRONIZATION
 *  - ::STM_SE_CTRL_H264_FORCE_PIC_ORDER_CNT_IGNORE_DPB_DISPLAY_FRAME_ORDERING
 *  - ::STM_SE_CTRL_H264_TREAT_DUPLICATE_DPB_VALUES_AS_NON_REF_FRAME_FIRST
 *  - ::STM_SE_CTRL_H264_TREAT_TOP_BOTTOM_PICTURE_AS_INTERLACED
 *  - ::STM_SE_CTRL_H264_VALIDATE_DPB_VALUES_AGAINST_PTS_VALUES
 *  - ::STM_SE_CTRL_MPEG2_APPLICATION_TYPE
 *  - ::STM_SE_CTRL_MPEG2_IGNORE_PROGRESSIVE_FRAME_FLAG
 *  - ::STM_SE_CTRL_VIDEO_DISCARD_FRAMES
 *  - ::STM_SE_CTRL_VIDEO_LAST_FRAME_BEHAVIOUR
 *  - ::STM_SE_CTRL_VIDEO_SINGLE_GOP_UNTIL_NEXT_DISCONTINUITY
 *  - ::STM_SE_CTRL_ALLOW_REFERENCE_FRAME_SUBSTITUTION
 *  - ::STM_SE_CTRL_DISCARD_FOR_REFERENCE_QUALITY_THRESHOLD
 *  - ::STM_SE_CTRL_DISCARD_FOR_MANIFESTATION_QUALITY_THRESHOLD
 *  - ::STM_SE_CTRL_DECODE_BUFFERS_USER_ALLOCATED
 *  - ::STM_SE_CTRL_SHARE_VIDEO_DECODE_BUFFERS
 *
 * \subsection play_stream_audio_controls Audio controls
 *
 *  - ::STM_SE_CTRL_AUDIO_APPLICATION_TYPE
 *  - ::STM_SE_CTRL_AUDIO_SERVICE_TYPE
 *  - ::STM_SE_CTRL_AUDIO_SUBSTREAM_ID
 *  - ::STM_SE_CTRL_AAC_DECODER_CONFIG
 *  - ::STM_SE_CTRL_PLAY_STREAM_NBCHANNELS
 *  - ::STM_SE_CTRL_PLAY_STREAM_SAMPLING_FREQUENCY
 *  - ::STM_SE_CTRL_AUDIO_DYNAMIC_RANGE_COMPRESSION
 *  - ::STM_SE_CTRL_AUDIO_PROGRAM_PLAYBACK_LEVEL
 *  - ::STM_SE_CTRL_BASSMGT
 *  - ::STM_SE_CTRL_DCREMOVE
 *  - ::STM_SE_CTRL_STREAM_DRIVEN_STEREO
 *  - ::STM_SE_CTRL_SPEAKER_CONFIG
 *  - ::STM_SE_CTRL_STREAM_DRIVEN_DUALMONO
 *  - ::STM_SE_CTRL_DUALMONO
 *  - ::STM_SE_CTRL_DEEMPHASIS
 *  - ::STM_SE_CTRL_AUDIO_PROGRAM_REFERENCE_LEVEL
 *
 * @{
 */

// Size of the array storing user data. First element store the size, second is for the user data type
// last 3 ones are user data
#define STM_SE_STREAM_CONTROL_USER_DATA_SIZE    5
#define STM_SE_STREAM_PARSED_PTS_DATA_SIZE  10

/*!
 * This type is used to select the type of data that will flow through a connection established with
 * ::stm_se_play_stream_attach.
 */
typedef enum
{
    STM_SE_PLAY_STREAM_OUTPUT_PORT_FIRST,
    STM_SE_PLAY_STREAM_OUTPUT_PORT_DEFAULT = STM_SE_PLAY_STREAM_OUTPUT_PORT_FIRST,
    STM_SE_PLAY_STREAM_OUTPUT_PORT_ANCILLIARY
} stm_se_play_stream_output_port_t;

/*!
 * stm_se_sink_input_port_t should be use to specify sink input port
 * when calling stm_se_play_stream_attach_to_pad.
 *
 * When sink object is an audio mixer object, the port selection implies:
 *  * PRIMARY : master playstream (also primary of a master playback) this is the stream
 *              that will by bypassed if STM_SE_PLAY_MAIN_COMPRESSED_OUT/STM_SE_PLAY_MAIN_COMPRESSED_SD_OUT
 *              is set on audio player (see stm_se_player_mode_t)
 *
 *  * AUX1    : secondary of master playback or any other playstream
 *  * AUX2    : any other playstream that shall be part of the mix
 *  * AUX3    : any other playstream that shall be part of the mix
 */
typedef enum
{
    STM_SE_SINK_INPUT_PORT_PRIMARY,
    STM_SE_SINK_INPUT_PORT_AUX1,
    STM_SE_SINK_INPUT_PORT_AUX2,
    STM_SE_SINK_INPUT_PORT_AUX3,

    STM_SE_SINK_INPUT_PORT_LAST = STM_SE_SINK_INPUT_PORT_AUX3
} stm_se_sink_input_port_t;

/*!
 * This type enumerates the keys that may be used to identify events.
 */
typedef enum stm_se_play_stream_event_e
{
    STM_SE_PLAY_STREAM_EVENT_INVALID                        = (0),

    /*!
     * This event reports that the first frame has gone on display (that is, decode and render have commenced).
     */
    STM_SE_PLAY_STREAM_EVENT_FIRST_FRAME_ON_DISPLAY         = (1 << 0),

    /*!
     * This event reports a frame that left the decoder too late to be displayed at the correct time.
     *
     * This may indicate poor decoder performance (decoding at less than 1x) or early starvation (that is, data
     * was not discarded due to late delivery coupled with bad estimates of decode times).
     */
    STM_SE_PLAY_STREAM_EVENT_FRAME_DECODED_LATE             = (1 << 1),

    /*!
     * This event reports a frame that left the decoder too late to be displayed at the correct time and the
     * fault was caused by the date not being available for decode early enough.
     */
    STM_SE_PLAY_STREAM_EVENT_DATA_DELIVERED_LATE            = (1 << 2),

    /*!
     * This event reports a serious internal failure. A serious internal failure indicates the play stream
     * object has been damaged beyond repair.
     *
     * The event implies a serious bug in the implementation from which no internal recovery is possible.
     *
     * If this event is observed during development, then it should be treated as a bug in the drivers.
     *
     * If observed in a deployed system, then the application should, at minimum, delete the current playback
     * and all play streams contained within it. Another alternative would simply be to trigger a watchdog reset.
     *
     * The application may also choose to implement postmortem or failure reporting logic.
     */
    STM_SE_PLAY_STREAM_EVENT_FATAL_ERROR                    = (1 << 3),

    /*!
     * This event reports that the hardware has entered an unrecoverable failure state.
     *
     * If this event is observed during development, then it should be treated as a system level bug (that is,
     * encompassing the entire system, from board design and bootstrap logic, to software and hardware
     * implementation).
     *
     * If observed in a deployed system, then the application has no alternative but to deploy a watchdog reset.
     *
     * The application may also choose to implement postmortem or failure reporting logic.
     */
    STM_SE_PLAY_STREAM_EVENT_FATAL_HARDWARE_FAILURE         = (1 << 4),

    /*!
     * This event reports that a frame has been decoded.
     *
     * The event is purely informative and of no semantic interest to the driver; no action is expected from the
     * application. Its existence is a middleware or user/kernel personality requirement.
     */
    STM_SE_PLAY_STREAM_EVENT_FRAME_DECODED                  = (1 << 5),

    /*!
     * This event reports that a frame has been rendered.
     *
     * Rendering a frame implies that the data reported by ::stm_se_play_stream_get_info has been changed.
     *
     * This event is purely informative and of no semantic interest to the driver; no action is expected from
     * the application. Its existence is a middleware or user/kernel personality requirement.
     */
    STM_SE_PLAY_STREAM_EVENT_FRAME_RENDERED                 = (1 << 6),

    /*!
     * This event reports that a new item has been appended to the play stream message queue and should be
     * extracted with ::stm_se_play_stream_get_message.
     *
     * Multiple messages may have been queued since the message ready event was first raised. It is therefore
     * important to fetch all available messages from the message queue before waiting for further events.
     */
    STM_SE_PLAY_STREAM_EVENT_MESSAGE_READY                  = (1 << 7),

    /*!
     * This event reports that the play stream has succeeded in rendering the decoded frames synchronously
     */
    STM_SE_PLAY_STREAM_EVENT_STREAM_IN_SYNC                 = (1 << 8),

    /*!
     * This event reports that the play stream has not been fed with compressed data for an unexpected period of
     * time and all the internal queues of the pipeline are empty.
     */
    STM_SE_PLAY_STREAM_EVENT_FRAME_STARVATION               = (1 << 9),

    /*!
     * This event reports that the play stream has decoded the last injected frame before the 'EOS' (end of
     * stream) injected discontinuity.
     *
     */
    STM_SE_PLAY_STREAM_EVENT_END_OF_STREAM                  = (1 << 10),

    /*!
     * This event reports that the play_stream has completed its internal process initiated by the latest call
     * to ::stm_se_play_stream_switch. This event informs the streaming engine client that any new call to
     * ::stm_se_play_stream_switch will not any more result in -EBUSY.
     *
     * Note that new data can be injected to the play_stream at any point after ::stm_se_play_stream_switch
     * completes; the event need not be waited for in this instance.
     *
     */
    STM_SE_PLAY_STREAM_EVENT_SWITCH_COMPLETED               = (1 << 11),

    /*!
      * This event reports that the play stream has started playing out new decoded data.
      * It is sent on startup of playback , when exiting a pause or when data is being fed again after a starvation.
      */
    STM_SE_PLAY_STREAM_EVENT_FRAME_SUPPLIED                 = (1 << 12)
} stm_se_play_stream_event_t;

/*!
 * This type indicates what type of message is found in the corresponding
 * ::stm_se_play_stream_msg_s structure.
 */
typedef enum stm_se_play_stream_msg_id_e
{
    STM_SE_PLAY_STREAM_MSG_INVALID                          = (0),
    STM_SE_PLAY_STREAM_MSG_MESSAGE_LOST                     = (1 << 0),
    STM_SE_PLAY_STREAM_MSG_VIDEO_PARAMETERS_CHANGED         = (1 << 1),
    STM_SE_PLAY_STREAM_MSG_FRAME_RATE_CHANGED               = (1 << 2),
    STM_SE_PLAY_STREAM_MSG_STREAM_UNPLAYABLE                = (1 << 3),
    STM_SE_PLAY_STREAM_MSG_TRICK_MODE_CHANGE                = (1 << 4),
    STM_SE_PLAY_STREAM_MSG_VSYNC_OFFSET_MEASURED            = (1 << 5),
    STM_SE_PLAY_STREAM_MSG_ALARM_PARSED_PTS                 = (1 << 6),
    STM_SE_PLAY_STREAM_MSG_AUDIO_PARAMETERS_CHANGED         = (1 << 7),
    STM_SE_PLAY_STREAM_MSG_DISCARDING                       = (1 << 8),
    STM_SE_PLAY_STREAM_MSG_ALARM_PTS                        = (1 << 9),
} stm_se_play_stream_msg_id_t;

/*!
 * This type describes the reason that trigs a stm_se_play_stream_msg_id_t::STM_SE_PLAY_STREAM_MSG_STREAM_UNPLAYABLE message
 * This may be because the stream format is unintelligible (unknown), or because the stream format is not supported (invalid) or
 * because of insufficient memory for decode(insufficient memory)
 * Valid only when msg_id is stm_se_play_stream_msg_id_t::STM_SE_PLAY_STREAM_MSG_STREAM_UNPLAYABLE
 *
 * See ::stm_se_play_stream_msg_s.
 */
typedef enum stm_se_play_stream_reason_code_e
{
    STM_SE_PLAY_STREAM_MSG_REASON_CODE_STREAM_UNKNOWN,
    STM_SE_PLAY_STREAM_MSG_REASON_CODE_STREAM_INVALID,
    STM_SE_PLAY_STREAM_MSG_REASON_CODE_INSUFFICIENT_MEMORY
} stm_se_play_stream_reason_code_t;

/*!
 * This type is aimed to define the values of the events that the user can be warned by, with the
 * corresponding alarm message.
 *
 * ::STM_SE_PLAY_STREAM_ALARM_PARSED_PTS is the identifier of the event that consists in surveying the
 * next pts detected by the collating mechanism of a play_stream
 * ::STM_SE_PLAY_STREAM_ALARM_PTS is the identifier of the event that consists in surveying the
 * next pts detected by the esprocessor within a play_stream
 */
typedef enum stm_se_play_stream_alarm_e
{
    STM_SE_PLAY_STREAM_ALARM_PARSED_PTS,
    STM_SE_PLAY_STREAM_ALARM_PTS
} stm_se_play_stream_alarm_t;


/*!
 * This structure is attached to the stm_se_play_stream_msg_id_t::STM_SE_PLAY_STREAM_MSG_ALARM_PARSED_PTS message
 * It reports the data that are parsed by the early parsing engine as soon as the PTS alarm has been set.
 * pts is the first native presentation time stamp parsed after the alarm is set whatever its setting mode : PES control packet or API call
 *
 * See ::stm_se_play_stream_msg_s.
 */
typedef struct stm_se_play_stream_alarm_parsed_pts_s
{
    uint64_t pts;
    unsigned char                 data[STM_SE_STREAM_PARSED_PTS_DATA_SIZE];
    unsigned int                      size;
} stm_se_play_stream_alarm_parsed_pts_t;

/*!
 * See ::stm_se_play_stream_msg_s.
 */
typedef struct stm_se_play_stream_alarm_pts_s
{
    uint64_t pts;
} stm_se_play_stream_alarm_pts_t;



/*!
 * This structure is attached to the stm_se_play_stream_msg_id_t::STM_SE_PLAY_STREAM_MSG_AUDIO_PARAMETERS_CHANGED message
 *
 * See ::stm_se_play_stream_msg_s.
 */
typedef struct stm_se_play_stream_audio_parameters
{
    // codec common paramters
    stm_se_stream_encoding_t            audio_coding_type;
    int                                 bitrate;
    uint32_t                            sampling_freq;
    uint8_t                             num_channels;
    stm_se_audio_channel_assignment_t   channel_assignment;
    bool                                dual_mono;
    bool                                emphasis;
    bool                                copyright;
    // codec specific parameters
    union stm_se_codec_specific_parameters
    {
        struct stm_se_mpeg_audio_parameters
        {
            uint32_t                 layer;
        } mpeg_params;
    } codec_specific;
} stm_se_play_stream_audio_parameters_t;

/*!
 * This type indicates what type of trigger is found in the corresponding
 * ::stm_se_play_stream_discard_trigger_s structure.
 */
typedef enum stm_se_play_stream_trigger_type_e
{
    /*!
     * This type is used to cancel the corresponding trigger (start or end trigger)
     */
    STM_SE_PLAY_STREAM_CANCEL_TRIGGER,
    /*!
     * This type is used to configure PTS trigger.
     * pts_trigger from ::stm_se_play_stream_discard_trigger_s is used for this type.
     * ::stm_se_play_stream_pts_and_tolerance_s is specifying a pts value and a tolerance
     * so that if a frame whose pts is inside the window [pts; pts+tolerance] is detected,
     * then it has to be considered as a trigger (used in the context of discard for instance).
     */
    STM_SE_PLAY_STREAM_PTS_TRIGGER,
    /*!
     * This type is used to configure splicing marker trigger.
     * Splicing marker is conveyed with data and contains offset information
     * ::stm_marker_splicing_data_s describes marker content
     */
    STM_SE_PLAY_STREAM_SPLICING_MARKER_TRIGGER
} stm_se_play_stream_trigger_type_t;

/*!
 * This type is used to indicate the pts value and the tolerance
 * for a discard trigger with type ::STM_SE_PLAY_STREAM_PTS_TRIGGER
 */
typedef struct stm_se_play_stream_pts_and_tolerance_s
{
    /*!
     * 33bits, 90KHz based Presentation Time Stamp information
     */
    uint64_t pts;
    /*!
     * 90KHz based tolerance associated to pts
     */
    uint32_t tolerance;
} stm_se_play_stream_pts_and_tolerance_t;

/*!
 * This type is used to describe the discard trigger to set when calling
 * ::stm_se_play_stream_set_discard_trigger
 */
typedef struct stm_se_play_stream_discard_trigger_s
{
    /*!
     * Trigger type to be considered
     */
    stm_se_play_stream_trigger_type_t type;
    /*!
     * Determine if it is a start or end trigger for discarding data
     */
    bool start_not_end;
    union
    {
        /*!
         * PTS trigger is specifying a pts value and a tolerance
         * so that if a frame whose pts is inside the window [pts; pts+tolerance] is detected,
         * then it has to be considered as a trigger (used in the context of discard for instance).
         * Valid only when stm_se_play_stream_trigger_type_t::STM_SE_PLAY_STREAM_PTS_TRIGGER
         */
        stm_se_play_stream_pts_and_tolerance_t pts_trigger;
    } u;
} stm_se_play_stream_discard_trigger_t;

/*!
 * This type contains a single message issued by the play stream to a message queue.
 *
 * The union structure contains a separate member for each supported message id.
 */
typedef struct stm_se_play_stream_msg_s
{
    stm_se_play_stream_msg_id_t msg_id;
    union
    {

        /*!
         * This message reports a change in at least one of the parameters of the video frames decoded by a video
         * play stream.
         *
         * Valid only when msg_id is stm_se_play_stream_msg_id_t::STM_SE_PLAY_STREAM_MSG_VIDEO_PARAMETERS_CHANGED.
         *
         */
        stm_se_play_stream_video_parameters_t    video_parameters;

        /*!
         * This message reports a change in frame rate.
         *
         * Valid only when msg_id is stm_se_play_stream_msg_id_t::STM_SE_PLAY_STREAM_MSG_FRAME_RATE_CHANGED.
         *
         * \deprecated A change in the frame rate should be monitored using ::STM_SE_PLAY_STREAM_MSG_VIDEO_PARAMETERS_CHANGED .
         */
        unsigned int                         frame_rate;

        /*!
         * This message reports that the stream cannot be played.
         *
         * Valid only when msg_id is stm_se_play_stream_msg_id_t::STM_SE_PLAY_STREAM_MSG_STREAM_UNPLAYABLE.
         *
         * This may be because the stream format is unintelligible (unknown), because the stream format is not
         * supported (invalid).
         */
        stm_se_play_stream_reason_code_t     reason;

        /*!
         * This message reports that the technique to manage trick mode decode bandwidth has changed.
         *
         * Valid only when msg_id is stm_se_play_stream_msg_id_t::STM_SE_PLAY_STREAM_MSG_TRICK_MODE_CHANGE.
         *
         * \todo Need a cross reference to the trick mode control (which shared the same numeric meaning)
         */
        unsigned int                         trick_mode_domain;

        /*!
         * This message reports the user data that have provided either along the stm_se_play_stream_set_alarm
         * function or along the corresponding injected marker.
         *
         * Valid only when msg_id is stm_se_play_stream_msg_id_t::STM_SE_PLAY_STREAM_MSG_ALARM_PARSED_PTS.
         *
         * See the markers injection application note section for more details
         *
         */
        stm_se_play_stream_alarm_parsed_pts_t    alarm_parsed_pts;

        /*!
         * This message reports that the offset between the vertical sync and the targeted presentation time has
         * been measured.
         *
         * Valid only when msg_id is stm_se_play_stream_msg_id_t::STM_SE_PLAY_STREAM_MSG_VSYNC_OFFSET_MEASURED.
         *
         * When adopting a time mapping, either automatic or fixed, the system will measure the degree by which the
         * already established vertical timings of the video output cause the fixed mapping to be missed. Once this
         * is measured, the mapping for video and audio will be corrected to account the position, in time, of the
         * vertical sync.
         *
         */
        long long                            vsync_offset;

        /*!
         *
         * This message reports the number of messages that are lost because of the lack of reactivity of the streaming engine client
         * to get the previousy inserted messages from the streaming engine internal queue.
         *
         * Valid only when msg_id is stm_se_play_stream_msg_id_t::STM_SE_PLAY_STREAM_MSG_MESSAGE_LOST.
         */
        unsigned int                         lost_count;

        /*!
         * This message reports a change in at least one of the parameters of the audio frames decoded by a audio
         * play stream.
         *
         * Valid only when msg_id is stm_se_play_stream_msg_id_t::STM_SE_PLAY_STREAM_MSG_AUDIO_PARAMETERS_CHANGED.
         *
         * \note The  parameters in ::stm_se_play_stream_audio_parameters_t are not complete and need to be reviewed.
         *       This may need to include some codec specific parameters as well.
         *
         */
        stm_se_play_stream_audio_parameters_t    audio_parameters;

        /*!
         * This message reports the stream reached a trigger start/end.
         * Each time stream is entering or exiting into a discarding period,
         * it raises an unconditional message.
         *
         * Valid only when msg_id is stm_se_play_stream_msg_id_t::STM_SE_PLAY_STREAM_MSG_DISCARDING.
         *
         * In case it is a start period, the discard_trigger.start_not_end will be set to true.
         * It will be set to false otherwise.
         *
         */
        stm_se_play_stream_discard_trigger_t discard_trigger;

        /*!
         * This message reports that PTS has been detected within the stream.
         *
         * Valid only when msg_id is stm_se_play_stream_msg_id_t::STM_SE_PLAY_STREAM_MSG_ALARM_PTS.
         * Alarm need to have been configured using ::stm_se_play_stream_set_alarm
         *
         */
        stm_se_play_stream_alarm_pts_t    alarm_pts;
    } u;
} stm_se_play_stream_msg_t;

/*!
 * A play stream subscription handle is an opaque data type with which to identify a subscription to a play
 * stream's message queue. It cannot be meaningfully de-referenced outside of the streaming engine
 * implementation.
 */
typedef struct stm_se_play_stream_subscription_s *stm_se_play_stream_subscription_h;

/*!
 * This type describes the timing state of a play stream, typically reported by ::stm_se_play_stream_get_info.
 *
 * The system time is the value of the (potentially recovered) system clock of the playback to which the
 * stream belongs (in microsecond units)
 *
 * The presentation time is the presentation time of the frame currently being rendered in the same time
 * domain as the system time.
 *
 * The PTS is the presentation time of the frame as found in the stream (in 90 kHz units).
 *
 * The frame count in the number of frames rendered since the play stream was instantiated.
 */
typedef struct stm_se_play_stream_info_s
{
    unsigned long long          system_time;
    unsigned long long          presentation_time;
    unsigned long long          pts;
    unsigned long long          frame_count;
} stm_se_play_stream_info_t;

/*!
 * This function allocates a play stream and associates it with a playback. All of the necessary management
 * objects, the display surface, and the internal Player components (Collator, Frame Parser, Codec, and
 * Manifestor) are allocated during this call. The default stream controls will be applied and buffers
 * allocated.
 *
 * \code
 * res = stm_se_play_stream_new("audio0", playback,
 *                           STM_SE_STREAM_ENCODING_AUDIO_VORBIS,
 *                           &stream);
 * if (0 != res) {
 *    return res;
 * }
 * \endcode
 *
 * \pre The streaming engine must be initialized.
 * \pre The playback parameter must have been previously provided by ::stm_se_playback_new and not deleted.
 *
 * \param[in] name Identifier used for system wide identification of this object (including directory names
 * when the object is represented in sysfs).
 *
 * \param[in] playback Handle of playback to which play stream is to be associated.
 * \param[in] encoding For example, ::STM_SE_STREAM_ENCODING_VIDEO_H264
 * \param[out] play_stream Pointer to an opaque handle variable which will be set to a valid handle if the
 * function succeeds.
 *
 * \retval 0 No error
 * \retval -EINVAL Encoding value is out of range.
 * \retval -ENOMEM Insufficient resources to complete operation.
 */
int             stm_se_play_stream_new(const char             *name,
                                       stm_se_playback_h       playback,
                                       stm_se_stream_encoding_t encoding,
                                       stm_se_play_stream_h        *play_stream);

/*!
 * This function deletes a play stream by disassociating it from its playback and releasing all associated
 * resources.
 *
 * \note Display resources used to leave the last available video picture on the display will remain
 *       allocated until the display surface is cleared or overwritten.
 *
 * \pre The streaming engine must be initialized.
 * \pre The stream parameter must have been previously provided by ::stm_se_play_stream_new and not deleted.
 *
 * \param[in] play_stream Handle of stream to be removed.
 *
 * \retval 0 No error
 * \retval -EBUSY Allocated objects (PES filters, play stream) etc.) attached or belonging to this playback
 * preclude deletion.
 */
int             stm_se_play_stream_delete(stm_se_play_stream_h    play_stream);

/*!
 * This function connects a sink object to a play stream.
 *
 * The following sink objects are valid for the default output port:
 *
 * * Source (from CoreDisplay)
 *
 * * Encode stream (from streaming engine)
 *
 * * Audio player (from streaming engine)
 *
 * * Memory sink (from Infrastructure)
 *
 * The correct sink object to use depends upon the media type of the play stream.
 *
 * The following sink objects are valid for the ancillary output port:
 *
 * * Memory sink (from Infrastructure)
 *
 * \pre The streaming engine must be initialized.
 * \pre The stream parameter must have been previously provided by ::stm_se_play_stream_new and not deleted.
 *
 * \pre The sink parameter must have been previously provided by another STKPI function and not deleted.
 *
  * \warning \note This function supports dynamic connection, allowing the sink objects to be attached during
 *       operation. However, preliminary implementations may not support this feature and will only support a
 *       single 1:1 connection at startup.
 *
 * \param[in] play_stream Handle of play stream to connect the sink to.
 * \param[in] sink Handle of an object that can accept data from a play stream.
 * \param[in] data_type Select the data type that will be issued by the play stream.
 *
 * \retval 0 No errors
 * \retval -EINVAL Sink object type cannot be attached to this play stream.
 * \retval -EALREADY The play stream is already connected to a sink object.
 * \retval -ENOTSUP See Stability above.
 */
int             stm_se_play_stream_attach(stm_se_play_stream_h    play_stream,
                                          stm_object_h            sink,
                                          stm_se_play_stream_output_port_t data_type);

/*!
 * This function connects a sink object to a play stream.
 *
 * The following sink objects are valid for the default output port:
 *
 * * Audio mixer (from streaming engine)
 *
 * The correct sink object to use depends upon the media type of the play stream.
 *
 * The following sink objects are valid for the ancillary output port:
 *
 * * None
 *
 * The following sink objects are valid for the input_port:
 *
 * * Audio mixer (from streaming engine)
 *
 * \pre The streaming engine must be initialized.
 * \pre The stream parameter must have been previously provided by ::stm_se_play_stream_new and not deleted.
 *
 * \pre The sink parameter must have been previously provided by another STKPI function and not deleted.
 *
  * \warning \note This function supports dynamic connection, allowing the sink objects to be attached during
 *       operation. However, preliminary implementations may not support this feature and will only support a
 *       single 1:1 connection at startup.
 *
 * \param[in] play_stream Handle of play stream to connect the sink to.
 * \param[in] sink Handle of an object that can accept data from a play stream.
 * \param[in] data_type Select the data type that will be issued by the play stream.
 * \param[in] input_port Select the sink's input pad that will be feed with play stream data.
 *
 * \retval 0 No errors
 * \retval -EINVAL Sink object type cannot be attached to this play stream.
 * \retval -EALREADY The play stream is already connected to a sink object.
 * \retval -ENOTSUP See Stability above.
 */
int             stm_se_play_stream_attach_to_pad(stm_se_play_stream_h    play_stream,
                                                 stm_object_h            sink,
                                                 stm_se_play_stream_output_port_t data_type,
                                                 stm_se_sink_input_port_t         input_port);
/*!
 * This function disconnects a sink object to a play stream.
 *
 * \pre The streaming engine must be initialized.
 * \pre The stream parameter must have been previously provided by ::stm_se_play_stream_new and not deleted.
 *
 * \pre The sink parameter must have been previously provided by another STKPI function and not deleted.
 *
 * \warning \note This function supports dynamic disconnection, allowing the sink objects to be detached during
 *       operation. However, preliminary implementations may not support this feature and implicitly detach from
 *       sink objects during ::stm_se_play_stream_delete.
 *
 * \param[in] play_stream Handle of play stream to disconnect the sink from.
 * \param[in] sink Handle of sink object to be disconnected from the play stream.
 *
 * \retval 0 No errors
 * \retval -ENOTCON The connection does not exist.
 * \retval -ENOTSUP See Stability above.
 */
int             stm_se_play_stream_detach(stm_se_play_stream_h    play_stream,
                                          stm_object_h            sink);

/*!
 * This function injects a chunk of data into the stream.
 *
 * The call is a blocking call, meaning it will only return when all data has been injected.
 *
 * The data pointer must be directly addressable in the current execution context. Thus, for a Linux kernel
 * client, the pointer should be a kernel virtual address.
 *
 * Typically, this function is called by a connected source such as a demux or memory source object.
 * However, it can also be called directly providing the play stream is not attached to any source.
 *
 * \pre The streaming engine must be initialized.
 * \pre The stream parameter must have been previously provided by ::stm_se_play_stream_new and not deleted.
 *
 * \pre The play stream must not be attached to a source object.
 * \pre The data parameter must not be invalid.
 *
 * \param[in] play_stream Handle of stream to inject data to.
 * \param[in] data Pointer to contiguous data block.
 * \param[in] data_length Number of bytes to transfer.
 *
 * <table>
 *     <tr>
 *         <td>
 *             Positive (or zero)
 *         </td>
 *         <td>
 *             Number of bytes successfully injected. In current implementations this is always equal to the
 *             data_length.
 *         </td>
 *     </tr>
 * </table>
 */
int stm_se_play_stream_inject_data(stm_se_play_stream_h    play_stream,
                                   const void             *data,
                                   uint32_t                data_length);

/*!
 * This function informs the player of a continuity break in the input data. Discontinuities are used to
 * indicate a jump in the input data and during reverse play.
 *
 * The parameter smooth_reverse, does not influence the play direction. Instead, it is used to differentiate
 * partial injection (abridgement) from backwards injection (discontinuous injection with abridgement) when
 * the play stream is operated in a reverse direction.
 *
 * The parameter end_of_stream indicates that the stream to be played in entirely injected. Some restricted
 * types of sinks are provided with a marker frame, issued immediately after the latest issued decompressed
 * frame .
 *
 * \pre The streaming engine must be initialized.
 * \pre The stream parameter must have been previously provided by ::stm_se_play_stream_new and not deleted.
 *
 * \param[in] play_stream Handle of stream to inject data to.
 * \param[in] smooth_reverse True if the discontinuity is the seam resulting purely from reverse playback,
 *                           false otherwise.
 * \param[in] surplus_data True if the caller cannot guarantee the discontinuity is frame aligned, false
 *                         otherwise.
 * \param[in] end_of_stream True if the caller has reached the end of the current stream.
 *
 * \retval 0 No error.
 */
int             stm_se_play_stream_inject_discontinuity(stm_se_play_stream_h play_stream,
                                                        bool                    smooth_reverse,
                                                        bool                    surplus_data,
                                                        bool                    end_of_stream);

/*!
 * This function reports the state of the streams enable bit. See ::stm_se_play_stream_set_enable for more
 * details.
 *
 * \pre The streaming engine must be initialized.
 * \pre The stream parameter must have been previously provided by ::stm_se_play_stream_new and not deleted.
 *
 * \param[in] play_stream Handle of stream to enable/disable.
 * \param[out] enable False = hide, True = reveal.
 *
 * \retval 0 No error.
 */
int             stm_se_play_stream_get_enable(stm_se_play_stream_h    play_stream,
                                              bool                   *enable);

/*!
 * This function directs the player to play out or discard all buffered data. ::stm_se_play_stream_drain is a
 * blocking call and returns when all data has been discarded or passed to the display driver.
 *
 * \code
 * // Discard data if changing direction
 * DirectionChange     = ((Speed * Context->PlaySpeed) < 0);
 * if (DirectionChange)
 * {
 *     // Discard previously injected data to free the lock.
 *     (void) stm_se_play_stream_drain(Context->VideoStream, true);
 * }
 * Result      = PlaybackSetSpeed (Context->Playback, Speed);
 * if (Result == PLAYER_NO_ERROR)
 *     Result  = PlaybackGetSpeed (Context->Playback, &Context->PlaySpeed);
 * \endcode
 *
 * \pre The streaming engine must be initialized.
 * \pre The stream parameter must have been previously provided by ::stm_se_play_stream_new and not deleted.
 *
 * \param[in] play_stream Handle of stream to drain.
 * \param[in] discard False = play out the buffered data, True = discard buffered data.
 *
 * \retval 0 No error.
 */
int             stm_se_play_stream_drain(stm_se_play_stream_h    play_stream,
                                         unsigned int            discard);

/*!
 * This function reveals/blanks video or mutes/unmutes audio depending on the stream media type. This
 * applies only to the visual/audible presentation; the decode of the stream and all timing related activity
 * (such as synchronization) is unaffected.
 *
 * \pre The streaming engine must be initialized.
 * \pre The stream parameter must have been previously provided by ::stm_se_play_stream_new and not deleted.
 *
 * \param[in] play_stream Handle of stream to enable/disable.
 * \param[in] enable False = hide, True = reveal.
 *
 * \retval 0 No error.
 */
int             stm_se_play_stream_set_enable(stm_se_play_stream_h    play_stream,
                                              bool                    enable);


/*!
 * This function sets the value of a play stream specific simple control such as the chosen master clock.
 *
 * \code
 * res = stm_se_play_stream_set_control(
 *         Context->Playback,
 *         STM_SE_CTRL_MASTER_CLOCK,
 *         STM_SE_CTRL_VALUE_SYSTEM_CLOCK_MASTER);
 * if (res != 0)
 *     return res;
 * \endcode
 *
 * \pre The streaming engine must be initialized.
 * \pre The play stream parameter must have been previously provided by ::stm_se_play_stream_new and not deleted.
 *
 * \param[in] play_stream Handle of play stream to set the control.
 * \param[in] ctrl Selector identifying the control to be updated.
 * \param[in] value Value to apply.
 *
 * \retval 0 No error
 * \retval -EINVAL Invalid argument for the value.
 * \retval -ENOTSUP Control type is not supported in this context.
 */
int             stm_se_play_stream_set_control(stm_se_play_stream_h    play_stream,
                                               stm_se_ctrl_t           ctrl,
                                               int32_t                 value);

/*!
 * This function gets the value of a play stream specific simple control such as the chosen master clock.
 *
 * \code
 * res = stm_se_play_stream_get_control(
 *         Context->Playback,
 *         STM_SE_CTRL_MASTER_CLOCK,
 *         &value);
 * if (res < 0)
 *     return res;
 * \endcode
 *
 * \pre The streaming engine must be initialized.
 * \pre The play stream parameter must have been previously provided by ::stm_se_play_stream_new and not deleted.
 *
 * \param[in] play_stream Handle of playback to set the control.
 * \param[in] ctrl Selector identifying the control to be read.
 * \param[out] value Pointer where currently applied control value can be stored.
 *
 * \retval 0 No error
 * \retval -ENOTSUP Control type is not supported in this context.
 */
int             stm_se_play_stream_get_control(stm_se_play_stream_h    play_stream,
                                               stm_se_ctrl_t           ctrl,
                                               int32_t                *value);

/*!
 * This function causes the player to step the video to the next frame in the current direction of play if
 * one is available.
 *
 * If no complete frame is available, no action will be taken. ::stm_se_play_stream_get_info can be used to
 * detect frame changes.
 *
 * To ensure frames are available to step, it is recommended that the step be performed by a different
 * thread to the data injection.
 *
 * \pre The streaming engine must be initialized.
 * \pre The stream parameter must have been previously provided by ::stm_se_play_stream_new and not deleted.
 *
 * \param[in] play_stream Handle of stream to inject data to.
 *
 * \retval 0 No error.
 */
int             stm_se_play_stream_step(stm_se_play_stream_h    play_stream);

/*!
 * This function provides a optimized way to perform an encoding standard change, without having to delete
 * and recreate a play_stream. It interestingly deconstructs and reconstructs only the internal objects of a
 * play_stream that are encoding standard dependant. It moreover permits to perform an encoding standard
 * change without glitching the visual and audible presentation.
 *
 * \pre The streaming engine must be initialized.
 * \pre The stream parameter must have been previously provided by ::stm_se_play_stream_new and not deleted.
 *
 * \param[in] play_stream Handle of stream to change encoding.
 * \param[in] encoding new encoding standard
 *
 * \retval 0 No error.
 * \retval -ENOMEM Insufficient memory to create new components.
 * \retval -EINVAL Unable to switch to the  new encoding mode
 */
int             stm_se_play_stream_switch(stm_se_play_stream_h    play_stream,
                                          stm_se_stream_encoding_t encoding);

/*!
 * This function provides access to information about the play details of the current stream. The play
 * details for video include the PTS and frame number of the currently visible frame.
 *
 * \code
 * if (Context->VideoStream != NULL) {
 *     int res = stm_se_play_stream_get_info(Context->VideoStream, &PlayInfo);
 *     BUG_ON(0 != res); // I know the VideoStream is still valid!
 *     VideoPlayInfo->system_time          = PlayInfo.system_time;
 *     VideoPlayInfo->presentation_time    = PlayInfo.presentation_time;
 *     VideoPlayInfo->pts                  = PlayInfo.pts;
 *     VideoPlayInfo->frame_count          = PlayInfo.frame_count;
 * }
 * \endcode
 *
 * \pre The streaming engine must be initialized.
 * \pre The stream parameter must have been previously provided by ::stm_se_play_stream_new and not deleted.
 *
 * \param[in] play_stream Handle of stream to inject data to.
 * \param[out] info Pointer to structure to hold result.
 *
 * \retval 0 No error.
 */
int             stm_se_play_stream_get_info(stm_se_play_stream_h    play_stream,
                                            stm_se_play_stream_info_t *info);

/*!
 * This function is used to specify bounds for displaying the stream. The stream will be injected, decoded,
 * and synchronized in the normal manner but it will only be rendered if the stream native time (for
 * example, PTS) is between the specified start and end times inclusive.
 *
 * The value ::STM_SE_PLAY_TIME_NOT_BOUNDED is used to indicate that there is no bound on the start, the end,
 * or both times.
 *
 * \code
 * // mute when the PTS reaches 90000 (i.e. if the stream starts
 * // at zero then play the first second).
 * res = stm_se_play_stream_set_interval(
 *      STM_SE_PLAY_TIME_NOT_BOUNDED,
 *      90000);
 * \endcode
 *
 * \pre The streaming engine must be initialized.
 * \pre The stream parameter must have been previously provided by ::stm_se_play_stream_new and not deleted.
 *
 * \warning Stability: Unstable
 *
 * \warning Stability: This function is relatively weak in its capabilities to constrain playback based on the native playback
 * time of the stream. This function may be replaced by a richer alternative.
 *
 * \param[in] play_stream Handle of stream to inject data to.
 * \param[in] start Native playback time when stream should become visible/audible.
 * \param[in] end Native playback time when stream should cease to be visible/audible.
 *
 * \retval 0 No error.
 * \retval -ERANGE end is earlier than start.
 */
int             stm_se_play_stream_set_interval(stm_se_play_stream_h    play_stream,
                                                unsigned long long      start,
                                                unsigned long long      end);

/*!
 * This function allows the application to poll state that would normally be reported by the message system.
 *
 * Specifically, the application can request that a message of the specified type be immediately generated
 * and delivered (without needing an active subscription handle).
 *
 * \code
 * res = stm_se_play_stream_poll_message(
 *      video_stream,
 *      STM_SE_PLAY_STREAM_MSG_VIDEO_SIZE_CHANGED,
 *      &evt);
 * if (0 == res) {
 *  BUG_ON(evt.id != STM_SE_PLAY_STREAM_MSG_VIDEO_SIZE_CHANGED);
 *  pr_info("Video is %d x %d\n", evt.size.width, evt.size.height);
 * }
 * \endcode
 *
 * \pre The streaming engine must be initialized.
 * \pre The stream parameter must have been previously provided by ::stm_se_play_stream_new and not deleted.
 *
 * \param[in] play_stream Handle of the play stream the subscription was allocated from.
 * \param[in] id Identity of the message to be polled.
 * \param[out] message The number of buffers in use (non-zero reference count) will be stored through this
 * pointer.
 *
 * \retval 0 No error, message delivered.
 * \retval -EAGAIN Message cannot be polled. For example, the stream unplayable message cannot be polled for
 * unless the stream has been marked unplayable.
 */
int             stm_se_play_stream_poll_message(stm_se_play_stream_h       play_stream,
                                                stm_se_play_stream_msg_id_t id,
                                                stm_se_play_stream_msg_t   *message);

/*!
 * This function extracts a message from the queue identified by the subscription argument.
 *
 * See \ref handling_play_stream_events for more information.
 *
 * \pre The streaming engine must be initialized.
 * \pre The stream parameter must have been previously provided by ::stm_se_play_stream_new and not deleted.
 *
 * \param[in] play_stream Handle of the play stream the subscription was allocated from.
 * \param[in] subscription Handle of the subscription to get the message for.
 * \param[out] message The number of buffers in use (non-zero reference count) will be stored through this
 * pointer.
 *
 * \retval 0 No error, message was available.
 * \retval -EAGAIN No messages available.
 */
int             stm_se_play_stream_get_message(stm_se_play_stream_h              play_stream,
                                               stm_se_play_stream_subscription_h subscription,
                                               stm_se_play_stream_msg_t         *message);

/*!
 * This function is used to subscribe to the play stream's messaging system. It allocates a message queue to
 * hold the subscribed messages and provides a subscription handle from which to obtain messages.
 *
 * When a message is appended to the message queue, the play stream will emit the
 * ::STM_SE_PLAY_STREAM_EVENT_MESSAGE_READY event using the STKPI event manager. This notifies the subscriber
 * that it must call ::stm_se_play_stream_get_message to examine the incoming message.
 *
 * \note When multiple subscribers exist, the message ready event may be signaled even when no message is
 *       available because the message ready event is shared by all message subscriptions. The subscriber should
 *       not treat an empty message queue as an error unless it can guarantee that it is the only subscriber to
 *       any messages from this object.
 *
 * \pre The streaming engine must be initialized.
 * \pre The stream parameter must have been previously provided by ::stm_se_play_stream_new and not deleted.
 *
 * \param[in] play_stream Handle of the play stream to subscribe to.
 * \param[in] depth Number of entries to allocate in the message queue.
 * \param[in] message_mask Mask of the message types to subscribe to.
 * \param[out] subscription Pointer to hold the resulting subscription handle.
 *
 * \retval 0 No error.
 * \retval -ENOMEM Insufficient memory to complete subscription.
 */
int             stm_se_play_stream_subscribe(stm_se_play_stream_h                  play_stream,
                                             uint32_t                           message_mask,
                                             uint32_t                           depth,
                                             stm_se_play_stream_subscription_h *subscription);

/*!
 * This function unsubscribed from the play stream's messaging system and results in the associated message
 * queue being freed.
 *
 * \pre The streaming engine must be initialized.
 * \pre The stream parameter must have been previously provided by ::stm_se_play_stream_new and not deleted.
 *
 * \pre The subscription parameter must have been previously provided by ::stm_se_play_stream_subscribe and not
 * deleted.
 *
 * \param[in] play_stream Handle of the play stream the subscription was allocated from.
 * \param[in] subscription Handle of the subscription to be decommissioned.
 *
 * \retval 0 No error.
 */
int             stm_se_play_stream_unsubscribe(stm_se_play_stream_h               play_stream,
                                               stm_se_play_stream_subscription_h  subscription);

/*!
 * This function gets the value specified by the control type.
 *
 * \pre The streaming engine must be initialized.
 *
 * \param[in] play_stream Previously allocated play stream handle.
 * \param[in] ctrl Selector identifying the control to be read.
 * \param[in] value Pointer to return the structure describing the requested control.
 *
 * \retval 0 No error
 * \retval -ENOTSUP Control type is not supported in this context.
 */
int             stm_se_play_stream_get_compound_control(stm_se_play_stream_h      play_stream,
                                                        stm_se_ctrl_t               ctrl,
                                                        void                        *value);

/*!
 * This function sets the value specified by the control type.
 *
 * \pre The streaming engine must be initialized.
 *
 * \param[in] play_stream Previously allocated play stream handle.
 * \param[in] ctrl Selector identifying the control to be updated.
 * \param[in] value Pointer to the structure describing the control to set.
 *
 * \retval 0 No error
 * \retval -EINVAL Invalid argument for the value.
 * \retval -ENOTSUP Control type is not supported in this context.
 */
int             stm_se_play_stream_set_compound_control(stm_se_play_stream_h      play_stream,
                                                        stm_se_ctrl_t               ctrl,
                                                        void                        *value);

/*!
 * This function records the wish of the streaming engine user of being warned by the message corresponding
 * to the alarm when the provided event occurs.
 *
 * Depending on the alarm, the user data bytes are rendered along the corresponding message
 *
 * \pre The streaming engine must be initialized.
 * \pre The stream parameter must have been previously provided by ::stm_se_play_stream_new and not deleted.
 *
 * In case the alarm is set to ::STM_SE_PLAY_STREAM_ALARM_PARSED_PTS or ::STM_SE_PLAY_STREAM_ALARM_PTS
 * ::stm_se_play_stream_pts_and_tolerance_t is to be considered with *value.
 *
 * \param[in] play_stream Handle of the play stream the subscription was allocated from.
 * \param[in] alarm Kind of alarm to be monitored.
 * \param[in] enable State of the alarm (activated or not).
 * \param[in] value Pointer to the structure describing the alarm.
 *
 * \retval 0 No error.
 * \retval -EINVAL Invalid parameters.
 */
int         stm_se_play_stream_set_alarm(stm_se_play_stream_h         play_stream,
                                         stm_se_play_stream_alarm_t   alarm,
                                         bool                         enable,
                                         void                        *value);

/*!
 * This function is used to specify bounds for discarding part of the data stream.
 *
 * An injected stream is to be early discarded as soon as a start trigger is internally validated.
 * As soon as an end trigger is reached, the injected stream is to be properly processed and rendered again.
 * A stop (resp. start) trigger can be set in complement of a start (resp. stop) already programmed.
 * Only ONE stop and/or start trigger can be programmed at a time.
 * A stop (resp. start) trigger cannot replace an already programmed stop (resp. start) trigger.
 * It has to be set at CANCEL_TRIGGER first.
 * Each time the start or end trigger of discard is asserted, a corresponding message is sent to the S.E. client.
 *
 * \pre The streaming engine must be initialized.
 * \pre The stream parameter must have been previously provided by ::stm_se_play_stream_new and not deleted.
 *
 * \param[in] play_stream Handle of the play stream the subscription was allocated from.
 * \param[in] trigger Pointer on the characteristics of the trigger.
 *
 * \retval 0 No error, trigger is configured succesfully.
 * \retval -EINVAL Invalid parameters.
 * \retval -EALREADY Trigger already programmed.
 */
int             stm_se_play_stream_set_discard_trigger(stm_se_play_stream_h                  play_stream,
                                                       stm_se_play_stream_discard_trigger_t *trigger);

/*!
 * This function is resetting the internal discard state machine of the concerned play_stream.
 * It may stop an active discard period.
 *
 * \pre The streaming engine must be initialized.
 * \pre The stream parameter must have been previously provided by ::stm_se_play_stream_new and not deleted.
 *
 * \param[in] play_stream Handle of the play stream the subscription was allocated from.
 *
 * \retval 0 No error.
 * \retval -EINVAL Invalid parameters.
 */
int             stm_se_play_stream_reset_discard_triggers(stm_se_play_stream_h play_stream);


/*! @} */ /* play_stream */

#endif /* STM_SE_PLAY_STREAM_H_ */
